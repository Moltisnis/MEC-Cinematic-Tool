#include "ReplayBundles.h"
#include "ReplayStore.h"
#include "../Main.h"
#include "../Util/Util.h"
#include <Windows.h>
#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstring>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

// How (decompiled, Research_Mission_Loading_2026-10-27.md sections 3 and 12):
// - The game's sub-level bundle loader (the object at the client sub-level
//   streaming manager + 0x7330; the game gives it as game + 0x70) is updated
//   once a frame by 0x143155700(loader), called from the game's frame update
//   0x142A2D470 (the call at 0x142A2D797). That update issues the loads and
//   unloads of its entries.
// - 0x143151660(loader, request) asks for a bundle. It finds or makes the
//   loader's entry by the bundle's key (its name's hash XOR its kind's salt)
//   and adds 1 to its count (entry + 0x14); a new one is registered with the
//   bundle manager. 0x143155310(loader, key) takes 1 off; the update unloads
//   an entry at 0. 0x14314B8F0(loader, key, &compartment, &bundle) says
//   whether it's in. All three take the loader's lock (loader + 0x10).
// - The request is made by 0x143140950(request, name, kind) and filled from
//   the sub-level (0x1432F0EC0): + 8 kind, + 0xC heap type, + 0x10 its
//   initial size (KB), + 0x14 allow grow, + 0x18, + 0x1C the parent
//   compartment, + 0x34 priority (+ 0x30 stays -1). Its string at + 0x28 is
//   let go with 0x142A3D730 afterwards.
// - The loader's entries: a tree at loader + 0x48 (+ 8 its first node; a
//   node's + 0 / + 8 / + 0x10 right, left, parent; + 0x20 the key, + 0x28 the
//   entry). An entry's + 0 is the name (a string: the address of its
//   characters), + 8 heap type, + 0xC size, + 0x10 grow, + 0x14 count, + 0x1C
//   compartment, + 0x28 key, + 0x4C priority, + 0x4E bit 0 in, + 0x50 kind,
//   + 0x54 parent compartment, + 0x58 the request's + 0x18.
// The replay's requests, its releases and its look at the entries are made in
// a hook on the loader's update, before the game's own update runs: on its
// thread, with the game's own functions (byte-checked first), the look under
// the loader's lock. The game's loader does the rest as for its own.
namespace
{
  const __int64 LoaderUpdateRva = 0x3155700, LoaderRequestRva = 0x3151660, LoaderReleaseRva = 0x3155310,
    LoaderIsInRva = 0x314B8F0, RequestMakeRva = 0x3140950, StringFreeRva = 0x2A3D730, LockRva = 0x2A9F970,
    UnlockRva = 0x2AA1840, UpdateCallRva = 0x2A2D797, CompartmentTableRva = 0x237DEB8;
  struct Code
  {
    __int64 Rva;
    BYTE Bytes[16];
    size_t Size;
  };
  const Code Expected[] = {
    { LoaderUpdateRva, { 0x55, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57, 0x48, 0x8D, 0xAC, 0x24, 0xA0, 0xCF, 0xFF }, 16 },
    { LoaderRequestRva, { 0x48, 0x89, 0x4C, 0x24, 0x08, 0x55, 0x53, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41 }, 16 },
    { LoaderReleaseRva, { 0x57, 0x48, 0x83, 0xEC, 0x30, 0x48, 0xC7, 0x44, 0x24, 0x20, 0xFE, 0xFF, 0xFF, 0xFF, 0x48, 0x89 }, 16 },
    { LoaderIsInRva, { 0x57, 0x41, 0x56, 0x41, 0x57, 0x48, 0x83, 0xEC, 0x30, 0x48, 0xC7, 0x44, 0x24, 0x20, 0xFE, 0xFF }, 16 },
    { RequestMakeRva, { 0x53, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x89, 0xCB, 0x48, 0x89, 0x11, 0x44, 0x89, 0x41, 0x08, 0x48 }, 16 },
    { StringFreeRva, { 0x53, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B, 0x19, 0x48, 0x8D, 0x05, 0x0D, 0xF9, 0x71, 0xFF, 0x48 }, 16 },
    { LockRva, { 0xFF, 0x25, 0x82, 0x57, 0xD7, 0x01 }, 6 },
    { UnlockRva, { 0xFF, 0x25, 0xFA, 0x38, 0xD7, 0x01 }, 6 },
    // The game's frame update calls the loader's update (E8 rel32)
    { UpdateCallRva, { 0xE8, 0x64, 0x7F, 0x72, 0x00 }, 5 },
  };
  const int RequestBytes = 0x40;
  const int ReqKind = 0x08, ReqHeapType = 0x0C, ReqSize = 0x10, ReqGrow = 0x14, ReqValue18 = 0x18, ReqParent = 0x1C,
    ReqString = 0x28, ReqPriority = 0x34;
  const int LoaderLock = 0x10, LoaderTree = 0x48, NodeEntry = 0x28;
  const int EntryHeapType = 0x08, EntrySize = 0x0C, EntryGrow = 0x10, EntryCount = 0x14, EntryCompartment = 0x1C,
    EntryKey = 0x28, EntryPriority = 0x4C, EntryFlags = 0x4E, EntryKind = 0x50, EntryParent = 0x54, EntryValue18 = 0x58;
  const unsigned Compartments = 3005;   // 0xBBD: none
  const unsigned FixedCompartments = 4; // Static, Frontend, LoadingScreen, Game
  const int CompartmentNameOffset = 0x70;
  const int MaxEntries = 512;
  const double LeadSeconds = 8.0;       // asked for this far ahead of the time it was in
  const double LagSeconds = 2.0;
  const double KeepSeconds = 5.0;       // let go this long after it isn't wanted any more
  const double WaitSeconds = 20.0;      // the start waits at most this long (wall clock)
  const double JoinSeconds = 1.5;       // two notes this close are one stretch in

  typedef void(__fastcall* tLoaderUpdate)(__int64);
  typedef int(__fastcall* tLoaderRequest)(__int64, void*);
  typedef int(__fastcall* tLoaderRelease)(__int64, int);
  typedef bool(__fastcall* tLoaderIsIn)(__int64, int, unsigned short*, __int64*);
  typedef void*(__fastcall* tRequestMake)(void*, char const*, int);
  typedef void(__fastcall* tStringFree)(void*);
  typedef void(__fastcall* tLock)(void*);
  tLoaderUpdate oLoaderUpdate = nullptr;
  tLoaderRequest pLoaderRequest = nullptr;
  tLoaderRelease pLoaderRelease = nullptr;
  tLoaderIsIn pLoaderIsIn = nullptr;
  tRequestMake pRequestMake = nullptr;
  tStringFree pStringFree = nullptr;
  tLock pLock = nullptr, pUnlock = nullptr;

  // Guarded reads (no objects to unwind in here)
  bool ReadRaw(void* out, __int64 at, size_t size)
  {
    __try
    {
      if (at < 0x10000)
        return false;
      memcpy(out, (void const*)at, size);
      return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
  }

  __int64 Q(__int64 at)
  {
    __int64 v = 0;
    return ReadRaw(&v, at, sizeof(v)) ? v : 0;
  }

  void ReadText(__int64 at, char* out, size_t size)
  {
    out[0] = 0;
    if (at < 0x10000 || !ReadRaw(out, at, size - 1))
    {
      out[0] = 0;
      return;
    }
    out[size - 1] = 0;
    for (size_t i = 0; i < size && out[i]; ++i)
      if ((unsigned char)out[i] < 32 || (unsigned char)out[i] > 126)
      {
        out[i] = 0;
        break;
      }
  }

  void CompartmentName(unsigned compartment, char* out, size_t size)
  {
    out[0] = 0;
    if (compartment >= Compartments)
      return;
    __int64 entry = Q((__int64)g_gameHandle + CompartmentTableRva + (__int64)compartment * 8);
    ReadText(entry ? Q(entry + CompartmentNameOffset) : 0, out, size);
  }

  // A loader entry as noted (plain data: read in a guarded function)
  struct Seen
  {
    char Name[160];
    char Parent[160];
    unsigned ParentCompartment;
    int Kind, HeapType, Grow, Value18, Priority, Key;
    unsigned Size;
  };
  Seen g_Seen[MaxEntries];   // the loader's thread only

  // The tree's next node, as the game's 0x142A34820 (a bad pointer throws to the caller's __try)
  __int64 TreeNext(__int64 node)
  {
    __int64 right = *(__int64 const*)node;
    if (right)
    {
      for (int i = 0; i < 64 && *(__int64 const*)(right + 8); ++i)
        right = *(__int64 const*)(right + 8);
      return right;
    }
    __int64 parent = *(__int64 const*)(node + 0x10);
    for (int i = 0; i < 64 && node == *(__int64 const*)parent; ++i)
    {
      node = parent;
      parent = *(__int64 const*)(parent + 0x10);
    }
    return *(__int64 const*)node != parent ? parent : node;
  }

  // The loader's entries that are in (the caller holds the loader's lock)
  int ReadEntries(__int64 loader)
  {
    int count = 0;
    __try
    {
      __int64 header = loader + LoaderTree;
      __int64 node = *(__int64 const*)(header + 8);
      for (int steps = 0; node && node != header && steps < 4 * MaxEntries && count < MaxEntries; ++steps)
      {
        __int64 entry = node + NodeEntry;
        if (*(BYTE const*)(entry + EntryFlags) & 1)
        {
          Seen& s = g_Seen[count];
          ReadText(*(__int64 const*)entry, s.Name, sizeof(s.Name));
          if (!s.Name[0])
            CompartmentName(*(unsigned short const*)(entry + EntryCompartment), s.Name, sizeof(s.Name));
          s.ParentCompartment = *(unsigned short const*)(entry + EntryParent);
          CompartmentName(s.ParentCompartment, s.Parent, sizeof(s.Parent));
          s.Kind = *(int const*)(entry + EntryKind);
          s.HeapType = *(int const*)(entry + EntryHeapType);
          s.Size = *(unsigned const*)(entry + EntrySize);
          s.Grow = *(int const*)(entry + EntryGrow);
          s.Value18 = *(int const*)(entry + EntryValue18);
          s.Priority = *(BYTE const*)(entry + EntryPriority);
          s.Key = *(int const*)(entry + EntryKey);
          if (s.Name[0])
            ++count;
        }
        node = TreeNext(node);
      }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
    return count;
  }

  struct Bundle
  {
    std::string Name;            // as the game asked for it (its loader entry's)
    std::string Parent;          // its parent compartment's bundle when noted ("" none)
    unsigned ParentCompartment;
    int Kind, HeapType, Grow, Value18, Priority;
    unsigned Size;
    std::vector<std::pair<double, double>> In;   // the recording's times it was in
    // While it plays
    bool Held = false;           // asked for by the replay (Key once the loader has it)
    int Key = 0;
    bool InNow = false;
    bool NeededNow = false;      // in at the time played (the start waits on it)
    double WantedUntil = -1.0;   // the playback's time
    ULONGLONG AskedAt = 0;
    ULONGLONG NotBefore = 0;     // not asked for again before (wall clock: its parent wasn't in)
    int ParentMisses = 0;
  };
  std::mutex g_Mutex;            // g_Bundles, g_ToAsk, g_ToRelease
  std::vector<Bundle> g_Bundles;
  std::vector<std::string> g_ToAsk;
  std::vector<std::pair<int, std::string>> g_ToRelease;
  std::atomic<double> g_SnapshotAt{ -1.0 };
  std::atomic<bool> g_Work{ false }, g_Checking{ false };
  std::atomic<bool> g_Available{ false };
  std::atomic<__int64> g_Loader{ 0 };
  std::atomic<unsigned> g_LoaderThread{ 0 };
  bool g_Tried = false;          // game thread
  int g_Mode = 0;
  double g_LastSnapshot = -1e9;
  bool g_ThreadsLogged = false;

  Bundle* Find(std::string const& name)
  {
    for (Bundle& b : g_Bundles)
      if (b.Name == name)
        return &b;
    return nullptr;
  }

  // The loader's thread: the entries in now, noted at the recording's time
  void Snapshot(__int64 loader, double time)
  {
    pLock((void*)(loader + LoaderLock));
    int count = ReadEntries(loader);
    pUnlock((void*)(loader + LoaderLock));
    std::lock_guard<std::mutex> lock(g_Mutex);
    for (int i = 0; i < count; ++i)
    {
      Seen const& s = g_Seen[i];
      Bundle* b = Find(s.Name);
      if (!b)
      {
        Bundle fresh;
        fresh.Name = s.Name;
        fresh.Parent = s.Parent;
        fresh.ParentCompartment = s.ParentCompartment;
        fresh.Kind = s.Kind;
        fresh.HeapType = s.HeapType;
        fresh.Size = s.Size;
        fresh.Grow = s.Grow;
        fresh.Value18 = s.Value18;
        fresh.Priority = s.Priority;
        g_Bundles.push_back(std::move(fresh));
        b = &g_Bundles.back();
        util::log::Write("Replay bundles: '%s' in while it records (from %.1f s; parent %u '%s', kind %d, heap type %d, %u KB, "
          "grow %d, priority %d)", s.Name, time, s.ParentCompartment, s.Parent, s.Kind, s.HeapType, s.Size, s.Grow, s.Priority);
      }
      if (!b->In.empty() && time - b->In.back().second <= JoinSeconds)
        b->In.back().second = time;
      else
        b->In.push_back({ time, time });
    }
  }

  // Where a bundle's parent is now: its compartment by name, or a fixed one as noted
  bool ParentNow(Bundle const& b, unsigned short& parent)
  {
    if (!b.Parent.empty())
      for (unsigned c = 0; c < Compartments; ++c)
      {
        char name[160];
        CompartmentName(c, name, sizeof(name));
        if (name[0] && !_stricmp(name, b.Parent.c_str()))
        {
          parent = (unsigned short)c;
          return true;
        }
      }
    if (b.ParentCompartment < FixedCompartments)
    {
      parent = (unsigned short)b.ParentCompartment;
      return true;
    }
    return false;
  }

  // The loader's thread, before its update: the replay's releases, then its requests
  void Work(__int64 loader)
  {
    std::vector<std::pair<int, std::string>> releases;
    std::vector<std::string> asks;
    {
      std::lock_guard<std::mutex> lock(g_Mutex);
      releases.swap(g_ToRelease);
      asks.swap(g_ToAsk);
    }
    for (auto const& r : releases)
    {
      int result = pLoaderRelease(loader, r.first);
      util::log::Write("Replay bundles: '%s' let go (key %08X)%s", r.second.c_str(), (unsigned)r.first,
        result ? "" : ": the game's loader didn't have it any more");
    }
    for (std::string const& name : asks)
    {
      Bundle params;
      unsigned short parent = 0;
      bool known = false, parentOk = false;
      {
        std::lock_guard<std::mutex> lock(g_Mutex);
        Bundle* b = Find(name);
        if (b && b->Held && !b->Key)
        {
          params = *b;
          known = true;
        }
      }
      if (!known)
        continue;
      parentOk = ParentNow(params, parent);
      if (!parentOk)
      {
        std::lock_guard<std::mutex> lock(g_Mutex);
        if (Bundle* b = Find(name))
        {
          b->Held = false;
          b->NotBefore = GetTickCount64() + 3000;
          if (b->ParentMisses++ < 3)
            util::log::Write("Replay bundles: '%s' not asked for: its parent '%s' isn't in now (asked again in 3 s)", name.c_str(),
              params.Parent.c_str());
        }
        continue;
      }
      alignas(16) BYTE request[RequestBytes] = {};
      pRequestMake(request, name.c_str(), params.Kind);
      *(int*)(request + ReqKind) = params.Kind;
      *(int*)(request + ReqHeapType) = params.HeapType;
      *(unsigned*)(request + ReqSize) = params.Size;
      *(int*)(request + ReqGrow) = params.Grow;
      *(int*)(request + ReqValue18) = params.Value18;
      *(unsigned short*)(request + ReqParent) = parent;
      *(BYTE*)(request + ReqPriority) = (BYTE)params.Priority;
      int key = pLoaderRequest(loader, request);
      pStringFree(request + ReqString);
      util::log::Write("Replay bundles: '%s' asked for (key %08X, parent compartment %u '%s', kind %d, heap type %d)", name.c_str(),
        (unsigned)key, parent, params.Parent.c_str(), params.Kind, params.HeapType);
      std::lock_guard<std::mutex> lock(g_Mutex);
      if (Bundle* b = Find(name))
      {
        b->Key = key;
        b->AskedAt = GetTickCount64();
      }
      g_Checking = true;
    }
  }

  // The loader's thread, after its update: which of the replay's are in
  void CheckIn(__int64 loader)
  {
    std::vector<std::pair<int, std::string>> waiting;
    {
      std::lock_guard<std::mutex> lock(g_Mutex);
      for (Bundle const& b : g_Bundles)
        if (b.Held && b.Key && !b.InNow)
          waiting.push_back({ b.Key, b.Name });
    }
    if (waiting.empty())
    {
      g_Checking = false;
      return;
    }
    for (auto const& w : waiting)
    {
      unsigned short compartment = 0xBBD;
      __int64 bundle = 0;
      if (!pLoaderIsIn(loader, w.first, &compartment, &bundle))
        continue;
      std::lock_guard<std::mutex> lock(g_Mutex);
      if (Bundle* b = Find(w.second))
        if (b->Key == w.first && !b->InNow)
        {
          b->InNow = true;
          util::log::Write("Replay bundles: '%s' in (compartment %u) after %.1f s", b->Name.c_str(), compartment,
            (GetTickCount64() - b->AskedAt) / 1000.0);
        }
    }
  }

  void __fastcall hLoaderUpdate(__int64 loader)
  {
    g_Loader = loader;
    if (!g_LoaderThread)
      g_LoaderThread = GetCurrentThreadId();
    double at = g_SnapshotAt.exchange(-1.0);
    if (at >= 0.0)
      Snapshot(loader, at);
    if (g_Work.exchange(false))
      Work(loader);
    oLoaderUpdate(loader);
    if (g_Checking)
      CheckIn(loader);
  }

  bool CodeIs(Code const& code)
  {
    BYTE here[16] = {};
    return code.Size <= sizeof(here) && ReadRaw(here, (__int64)g_gameHandle + code.Rva, code.Size) &&
      !memcmp(here, code.Bytes, code.Size);
  }

  void Hook()
  {
    for (Code const& code : Expected)
      if (!CodeIs(code))
      {
        util::log::Warning("Replay bundles: the game's bundle loader isn't what the tools know (at 0x%I64X): the recorded "
          "mission's bundles aren't loaded again", 0x140000000 + code.Rva);
        return;
      }
    __int64 base = (__int64)g_gameHandle;
    pLoaderRequest = (tLoaderRequest)(base + LoaderRequestRva);
    pLoaderRelease = (tLoaderRelease)(base + LoaderReleaseRva);
    pLoaderIsIn = (tLoaderIsIn)(base + LoaderIsInRva);
    pRequestMake = (tRequestMake)(base + RequestMakeRva);
    pStringFree = (tStringFree)(base + StringFreeRva);
    pLock = (tLock)(base + LockRva);
    pUnlock = (tLock)(base + UnlockRva);
    if (!util::hooks::CreateGameHook("ReplayBundlesLoaderUpdate", base + LoaderUpdateRva, (PVOID)hLoaderUpdate,
        (LPVOID*)&oLoaderUpdate))
    {
      util::log::Warning("Replay bundles: the game's bundle loader couldn't be hooked: the recorded mission's bundles aren't "
        "loaded again");
      return;
    }
    g_Available = true;
    util::log::Write("Replay bundles: the recorded mission's bundles are loaded again while it plays (the game's bundle loader "
      "hooked)");
  }

  void ReleaseAll(char const* why)
  {
    int count = 0;
    {
      std::lock_guard<std::mutex> lock(g_Mutex);
      for (Bundle& b : g_Bundles)
      {
        if (!b.Held)
          continue;
        if (b.Key)
          g_ToRelease.push_back({ b.Key, b.Name });
        g_ToAsk.erase(std::remove(g_ToAsk.begin(), g_ToAsk.end(), b.Name), g_ToAsk.end());
        b.Held = false;
        b.Key = 0;
        b.InNow = false;
        b.NeededNow = false;
        b.WantedUntil = -1.0;
        ++count;
      }
    }
    if (count)
    {
      g_Work = true;
      util::log::Write("Replay bundles: %d let go (%s)", count, why);
    }
  }

  bool Overlaps(Bundle const& b, double from, double to)
  {
    for (auto const& span : b.In)
      if (span.first <= to && span.second >= from)
        return true;
    return false;
  }

  // While it plays, at the time played: the bundles in then (and LeadSeconds
  // ahead) asked for, those past it let go after KeepSeconds; parents first
  void Plan(double time)
  {
    bool work = false;
    std::lock_guard<std::mutex> lock(g_Mutex);
    for (int pass = 0; pass < 2; ++pass)
      for (Bundle& b : g_Bundles)
      {
        bool isParent = false;
        for (Bundle const& other : g_Bundles)
          isParent = isParent || (!other.Parent.empty() && !_stricmp(other.Parent.c_str(), b.Name.c_str()));
        // Parents asked for in the first pass, the others in the second
        if ((pass == 0) != isParent)
          continue;
        bool wanted = Overlaps(b, time - LagSeconds, time + LeadSeconds);
        b.NeededNow = Overlaps(b, time - LagSeconds, time + 0.5);
        if (wanted)
        {
          b.WantedUntil = time + KeepSeconds;
          if (!b.Held && GetTickCount64() >= b.NotBefore)
          {
            b.Held = true;
            b.Key = 0;
            b.InNow = false;
            b.AskedAt = GetTickCount64();
            g_ToAsk.push_back(b.Name);
            work = true;
          }
        }
        else if (b.Held && (time > b.WantedUntil || time < b.WantedUntil - KeepSeconds - LeadSeconds - 1.0))
        {
          if (b.Key)
            g_ToRelease.push_back({ b.Key, b.Name });
          g_ToAsk.erase(std::remove(g_ToAsk.begin(), g_ToAsk.end(), b.Name), g_ToAsk.end());
          b.Held = false;
          b.Key = 0;
          b.InNow = false;
          work = true;
        }
      }
    if (work)
      g_Work = true;
  }
}

bool replay_bundles::Available()
{
  return g_Available;
}

void replay_bundles::Update(int mode, double clock, bool enabled, bool loading)
{
  if (!g_Tried)
  {
    g_Tried = true;
    Hook();
  }
  if (!g_Available)
    return;
  if (!g_ThreadsLogged && g_LoaderThread)
  {
    g_ThreadsLogged = true;
    unsigned replay = GetCurrentThreadId();
    util::log::Write("Replay bundles: the game's bundle loader is updated on thread %u, the replay runs on thread %u%s",
      (unsigned)g_LoaderThread, replay, g_LoaderThread == replay ? " (the same)" : "");
  }
  int was = g_Mode;
  g_Mode = mode;
  if (mode == 1)
  {
    if (was != 1)
    {
      Forget();
      g_LastSnapshot = -1e9;
    }
    if (clock - g_LastSnapshot >= 1.0 || clock < g_LastSnapshot)
    {
      g_SnapshotAt = clock;
      g_LastSnapshot = clock;
    }
  }
  if (was == 1 && mode != 1)
  {
    std::lock_guard<std::mutex> lock(g_Mutex);
    util::log::Write("Replay bundles: %d sub-level bundles were in while it recorded", (int)g_Bundles.size());
    for (Bundle const& b : g_Bundles)
      util::log::Write("Replay bundles:   '%s': %d stretch%s, %.1f - %.1f s", b.Name.c_str(), (int)b.In.size(),
        b.In.size() == 1 ? "" : "es", b.In.empty() ? 0.0 : b.In.front().first, b.In.empty() ? 0.0 : b.In.back().second);
  }
  bool hold = mode == 2 && enabled && !loading;
  if (hold)
    Plan(clock);
  else
    ReleaseAll(mode != 2 ? "the playback stopped" : loading ? "a loading screen" : "the option is off");
}

void replay_bundles::Forget()
{
  ReleaseAll("the recording is forgotten");
  std::lock_guard<std::mutex> lock(g_Mutex);
  g_Bundles.clear();
}

bool replay_bundles::Waiting()
{
  if (!g_Available || g_Mode != 2)
    return false;
  ULONGLONG now = GetTickCount64();
  std::lock_guard<std::mutex> lock(g_Mutex);
  for (Bundle const& b : g_Bundles)
    if (b.Held && b.NeededNow && !b.InNow && now - b.AskedAt < (ULONGLONG)(WaitSeconds * 1000.0))
      return true;
  return false;
}

void replay_bundles::Status(int& recorded, int& held, int& in)
{
  recorded = held = in = 0;
  std::lock_guard<std::mutex> lock(g_Mutex);
  recorded = (int)g_Bundles.size();
  for (Bundle const& b : g_Bundles)
  {
    held += b.Held;
    in += b.Held && b.InNow;
  }
}

void replay_bundles::Save(replay_store::Out& out)
{
  std::lock_guard<std::mutex> lock(g_Mutex);
  out.Put((unsigned)g_Bundles.size());
  for (Bundle const& b : g_Bundles)
  {
    out.Text(b.Name);
    out.Text(b.Parent);
    out.Put(b.ParentCompartment);
    out.Put(b.Kind);
    out.Put(b.HeapType);
    out.Put(b.Grow);
    out.Put(b.Value18);
    out.Put(b.Priority);
    out.Put(b.Size);
    out.Put((unsigned)b.In.size());
    for (auto const& span : b.In)
    {
      out.Put(span.first);
      out.Put(span.second);
    }
  }
}

bool replay_bundles::Load(replay_store::In& in)
{
  unsigned count = 0;
  if (!in.Get(count) || count > MaxEntries * 4)
    return false;
  std::vector<Bundle> bundles(count);
  for (Bundle& b : bundles)
    if (!in.Text(b.Name, 512) || !in.Text(b.Parent, 512) || !in.Get(b.ParentCompartment) || !in.Get(b.Kind) || !in.Get(b.HeapType) ||
        !in.Get(b.Grow) || !in.Get(b.Value18) || !in.Get(b.Priority) || !in.Get(b.Size))
      return false;
    else
    {
      unsigned spans = 0;
      if (!in.Get(spans) || spans > (1u << 20))
        return false;
      b.In.resize(spans);
      for (auto& span : b.In)
        if (!in.Get(span.first) || !in.Get(span.second))
          return false;
    }
  Forget();
  std::lock_guard<std::mutex> lock(g_Mutex);
  g_Bundles.swap(bundles);
  return true;
}

// A mission's own bundle: Levels/SP/SP_MainCity/<name>, not a zone's (_ZS)
std::string replay_bundles::Where()
{
  const char* const Prefix = "Levels/SP/SP_MainCity/";
  std::lock_guard<std::mutex> lock(g_Mutex);
  std::string where;
  for (Bundle const& b : g_Bundles)
  {
    if (_strnicmp(b.Name.c_str(), Prefix, strlen(Prefix)))
      continue;
    std::string rest = b.Name.substr(strlen(Prefix));
    if (rest.empty() || rest.find('/') != std::string::npos || rest.find("_ZS") != std::string::npos ||
        (!where.empty() && where.find(rest) != std::string::npos))
      continue;
    // The city's own (its looks, far views, lighting, logic), not a mission's
    // (2026-10-27 21:26: a Release recording was named
    // "VisualEnvironments+VISTA_CityBuildings+R...")
    static char const* const City[] = { "VisualEnvironment", "VISTA", "LMS", "Logic", "Backdrop", "Sky", "Ground", "Audio",
      "Lighting", "Shared" };
    bool city = false;
    for (char const* part : City)
    {
      std::string lower = rest, key = part;
      for (char& c : lower)
        c = (char)tolower((unsigned char)c);
      for (char& c : key)
        c = (char)tolower((unsigned char)c);
      city = city || lower.find(key) != std::string::npos;
    }
    if (city)
      continue;
    if (!where.empty())
      where += "+";
    where += rest;
    if (where.size() > 40)
      break;
  }
  return where;
}
