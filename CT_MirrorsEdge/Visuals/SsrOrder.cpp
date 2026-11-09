#include "SsrOrder.h"
#include "../Main.h"
#include "../Util/Util.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>

// See SsrOrder.h. The game's addresses and layouts from
// Research_ScreenSpace_2026-11-02.md (sections 4.1, 4.2), checked again
// offline on the decrypted game image (the weather session's
// tools\game_image.bin) before they went in here: the driver's first 16
// bytes, the TypeInfo's class info, the fields' offsets.
namespace
{
  // The screen-space reflections' driver (reached through the thunk at
  // 0x1435B2F10): per view (its third argument), all of their compute passes.
  // Its start as the image has it: hooked only if it's that.
  const __int64 SsrDriverRva = 0x35BBB00;
  const unsigned char SsrDriverStart[16] = { 0x55, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57, 0x48,
    0x83, 0xEC, 0x30, 0x48 };
  // The settings registry's pointer (the game finds a settings object there
  // by its TypeInfo, 0x142AB21C0: hash buckets at + 0xA8, how many at + 0xB0;
  // a node: TypeInfo, the object, the next), and GlobalPostProcessSettings'
  // TypeInfo (its first qword its class info, 0x142837118; size 0x270)
  const __int64 SettingsRegistryRva = 0x2374A88;
  const __int64 PostProcessTypeInfoRva = 0x2838120, PostProcessClassInfoRva = 0x2837118;
  // Its fields for them (the class info's own field list)
  const int BlurPyramidEnableOffset = 0x231;
  const int SsrEnableOffset = 0x253, SsrDeferredResolveOffset = 0x254, SsrSeparateCoverageOffset = 0x255,
    SsrFullresOffset = 0x256, Ssr32BitOffset = 0x257, SsrReuseAllocOffset = 0x258;
  const int SsrAsyncComputeOffset = 0x19C, SsrThicknessFadeScaleOffset = 0x1A0, SsrReuseAllocWhitePointOffset = 0x1A4,
    SsrDebugOffset = 0x1A8, SsrQualityOffset = 0x1AC;
  // ID3D11DeviceContext's (its vtable)
  const int DispatchSlot = 41, DispatchIndirectSlot = 42, ExecuteCommandListSlot = 58;

  // A frame's order (see AtPresent): what happened, as it happened
  enum Kind : unsigned char { KindPass, KindSsrBegin, KindSsrEnd, KindDispatch, KindDispatchIndirect, KindCommandList };
  struct Event
  {
    std::atomic<int> Ready;   // 1 once the rest is written
    unsigned char Kind;
    signed char A, B;         // a pass change's from and to; the driver's view
    bool Immediate;           // on the game's immediate context
    DWORD Thread;
  };
  const int MostEvents = 4096;
  Event g_Events[MostEvents];
  std::atomic<int> g_EventCount{ 0 };
  std::atomic<bool> g_Recording{ false };
  std::atomic<ID3D11DeviceContext*> g_Immediate{ nullptr };
  std::atomic<bool> g_SawGbuffer{ false };       // a G-buffer pass began since the last look
  std::atomic<long long> g_DriverCalls{ 0 };     // the driver's calls, all along

  const int FramesTaken = 3;
  const ULONGLONG WaitFirstMs = 5000, BetweenFramesMs = 1000, CountForMs = 10000;
  int g_State = 0;   // 0 not begun, 1 waiting for a frame to take, 2 taking one, 3 counting the driver's calls, 4 done
  int g_Taken = 0;
  ULONGLONG g_HookedAt = 0, g_LastTakenAt = 0, g_CountFrom = 0;
  long long g_CallsFrom = 0;

  void Note(Kind kind, int a, int b, bool immediate)
  {
    if (!g_Recording.load(std::memory_order_acquire))
      return;
    int i = g_EventCount.fetch_add(1, std::memory_order_relaxed);
    if (i >= MostEvents)
      return;
    Event& e = g_Events[i];
    e.Kind = kind;
    e.A = (signed char)a;
    e.B = (signed char)b;
    e.Immediate = immediate;
    e.Thread = GetCurrentThreadId();
    e.Ready.store(1, std::memory_order_release);
  }

  // The hooks: each only notes the call, then calls the game's on as it was
  typedef void(STDMETHODCALLTYPE* tDispatch)(ID3D11DeviceContext*, UINT, UINT, UINT);
  typedef void(STDMETHODCALLTYPE* tDispatchIndirect)(ID3D11DeviceContext*, ID3D11Buffer*, UINT);
  typedef void(STDMETHODCALLTYPE* tExecuteCommandList)(ID3D11DeviceContext*, ID3D11CommandList*, BOOL);
  // (its arguments: the renderer, ?, the view's number, ?; two more passed on
  // as they are in case it has them, the image's code reads none)
  typedef __int64(__fastcall* tSsrDriver)(__int64, __int64, __int64, __int64, __int64, __int64);
  tDispatch oDispatch = nullptr;
  tDispatchIndirect oDispatchIndirect = nullptr;
  tExecuteCommandList oExecuteCommandList = nullptr;
  tSsrDriver oSsrDriver = nullptr;

  void STDMETHODCALLTYPE hDispatch(ID3D11DeviceContext* pContext, UINT x, UINT y, UINT z)
  {
    Note(KindDispatch, 0, 0, pContext == g_Immediate.load(std::memory_order_relaxed));
    oDispatch(pContext, x, y, z);
  }

  void STDMETHODCALLTYPE hDispatchIndirect(ID3D11DeviceContext* pContext, ID3D11Buffer* pArgs, UINT offset)
  {
    Note(KindDispatchIndirect, 0, 0, pContext == g_Immediate.load(std::memory_order_relaxed));
    oDispatchIndirect(pContext, pArgs, offset);
  }

  void STDMETHODCALLTYPE hExecuteCommandList(ID3D11DeviceContext* pContext, ID3D11CommandList* pList, BOOL restore)
  {
    Note(KindCommandList, 0, 0, pContext == g_Immediate.load(std::memory_order_relaxed));
    oExecuteCommandList(pContext, pList, restore);
  }

  __int64 __fastcall hSsrDriver(__int64 a, __int64 b, __int64 view, __int64 d, __int64 e, __int64 f)
  {
    g_DriverCalls.fetch_add(1, std::memory_order_relaxed);
    Note(KindSsrBegin, (int)(unsigned int)view, 0, false);
    __int64 result = oSsrDriver(a, b, view, d, e, f);
    Note(KindSsrEnd, (int)(unsigned int)view, 0, false);
    return result;
  }

  // Read-only, inside __try (no C++ objects here): whether the driver's start
  // is the image's
  bool DriverIsTheImages(__int64 at)
  {
    __try
    {
      return memcmp((void const*)at, SsrDriverStart, sizeof(SsrDriverStart)) == 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
  }

  // Read-only, inside __try: GlobalPostProcessSettings through the settings
  // registry, as the game finds it; its fields for the reflections. 0 found,
  // 1 the TypeInfo isn't the image's, 2 not in the registry, 3 a read failed.
  struct PostProcess
  {
    unsigned char BlurPyramid, Enable, DeferredResolve, SeparateCoverage, Fullres, Bits32, ReuseAlloc;
    int AsyncCompute, Debug, Quality;
    float ThicknessFadeScale, ReuseAllocWhitePoint;
    __int64 At;
  };
  int ReadPostProcess(__int64 base, PostProcess* out)
  {
    __try
    {
      __int64 typeInfo = base + PostProcessTypeInfoRva;
      if (*(__int64 const*)typeInfo != base + PostProcessClassInfoRva)
        return 1;
      __int64 registry = *(__int64 const*)(base + SettingsRegistryRva);
      if (!registry)
        return 2;
      __int64 buckets = *(__int64 const*)(registry + 0xA8);
      unsigned int count = *(unsigned int const*)(registry + 0xB0);
      if (!buckets || !count || count > (1u << 24))
        return 2;
      __int64 node = *(__int64 const*)(buckets + (__int64)(((unsigned long long)typeInfo & 0xFFFFFFFFull) % count) * 8);
      __int64 settings = 0;
      for (int guard = 0; node && guard < 4096 && !settings; ++guard)
      {
        if (*(__int64 const*)node == typeInfo)
          settings = *(__int64 const*)(node + 8);
        else
          node = *(__int64 const*)(node + 0x10);
      }
      if (!settings)
        return 2;
      BYTE const* s = (BYTE const*)settings;
      out->At = settings;
      out->BlurPyramid = s[BlurPyramidEnableOffset];
      out->Enable = s[SsrEnableOffset];
      out->DeferredResolve = s[SsrDeferredResolveOffset];
      out->SeparateCoverage = s[SsrSeparateCoverageOffset];
      out->Fullres = s[SsrFullresOffset];
      out->Bits32 = s[Ssr32BitOffset];
      out->ReuseAlloc = s[SsrReuseAllocOffset];
      out->AsyncCompute = *(int const*)(s + SsrAsyncComputeOffset);
      out->Debug = *(int const*)(s + SsrDebugOffset);
      out->Quality = *(int const*)(s + SsrQualityOffset);
      out->ThicknessFadeScale = *(float const*)(s + SsrThicknessFadeScaleOffset);
      out->ReuseAllocWhitePoint = *(float const*)(s + SsrReuseAllocWhitePointOffset);
      return 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return 3;
    }
  }

  void LogSettings(__int64 base)
  {
    PostProcess p{};
    int got = ReadPostProcess(base, &p);
    if (got)
    {
      util::log::Write("Screen-space reflections (diagnostic): GlobalPostProcessSettings not read (%s)",
        got == 1 ? "its TypeInfo isn't the one the game image has" : got == 2 ? "not found in the settings registry" :
        "a read failed");
      return;
    }
    // (as GlobalPostProcessSettings' own field list names them; the async
    // compute method's numbers as its enum orders them: None, GBuffer,
    // Shadows)
    util::log::Write("Screen-space reflections (diagnostic): GlobalPostProcessSettings at 0x%I64X: ScreenSpaceRaytraceEnable %u, "
      "Quality %d, FullresEnable %u, DeferredResolveEnable %u, SeparateCoverageEnable %u, 32BitEnable %u, ReuseAllocEnable %u "
      "(white point %.3f), ThicknessFadeScale %.3f, AsyncComputeMethod %d, Debug %d; BlurPyramidEnable %u (without it the game "
      "turns them off)", p.At, p.Enable, p.Quality, p.Fullres, p.DeferredResolve, p.SeparateCoverage, p.Bits32, p.ReuseAlloc,
      p.ReuseAllocWhitePoint, p.ThicknessFadeScale, p.AsyncCompute, p.Debug, p.BlurPyramid);
  }

  void Hook(ID3D11DeviceContext* pImmediate)
  {
    __int64 base = (__int64)g_gameHandle;
    void** table = *(void***)pImmediate;
    bool dispatch = util::hooks::CreateGameHook("SsrOrderDispatch", (__int64)table[DispatchSlot], (PVOID)hDispatch,
      (LPVOID*)&oDispatch);
    bool indirect = util::hooks::CreateGameHook("SsrOrderDispatchIndirect", (__int64)table[DispatchIndirectSlot],
      (PVOID)hDispatchIndirect, (LPVOID*)&oDispatchIndirect);
    bool lists = util::hooks::CreateGameHook("SsrOrderCommandList", (__int64)table[ExecuteCommandListSlot],
      (PVOID)hExecuteCommandList, (LPVOID*)&oExecuteCommandList);
    bool driver = false;
    if (DriverIsTheImages(base + SsrDriverRva))
      driver = util::hooks::CreateGameHook("SsrOrderDriver", base + SsrDriverRva, (PVOID)hSsrDriver, (LPVOID*)&oSsrDriver);
    else
      util::log::Warning("Screen-space reflections (diagnostic): their driver at 0x%I64X doesn't start as the game image's does: "
        "not hooked", base + SsrDriverRva);
    util::log::Write("Screen-space reflections (diagnostic): hooked to watch them, nothing changed: the compute dispatches %s, the "
      "indirect ones %s, the command lists %s, their driver %s; %d frames' order next, from %.0f s on", dispatch ? "yes" : "no",
      indirect ? "yes" : "no", lists ? "yes" : "no", driver ? "yes" : "no", FramesTaken, WaitFirstMs / 1000.0);
  }

  const char* PassName(int pass)
  {
    switch (pass)
    {
    case 0: return "G-buffer";
    case 1: return "decals";
    case 2: return "lit";
    case 3: return "half size";
    case 4: return "environment";
    default: return "other";
    }
  }

  // The frame just taken, in the log: its events in order (dispatches in a
  // row counted together), and what fix 7.A needs to know
  void LogFrame()
  {
    int count = (std::min)(g_EventCount.load(std::memory_order_acquire), MostEvents);
    char line[6144];
    int at = 0;
    auto add = [&](char const* format, auto... args)
    {
      if (at < 0 || at >= (int)sizeof(line) - 64)
        return;
      int n = snprintf(line + at, sizeof(line) - at, format, args...);
      at = n < 0 ? -1 : (std::min)(at + n, (int)sizeof(line) - 1);
    };
    int runDispatches = 0, runIndirect = 0, runOther = 0;
    auto endRun = [&]()
    {
      if (runDispatches + runIndirect + runOther)
        add("; %d dispatches (%d indirect%s)", runDispatches + runIndirect, runIndirect,
          runOther ? ", some on another context" : "");
      runDispatches = runIndirect = runOther = 0;
    };
    int decalsEnded = -1, firstDriver = -1, driverDispatches = 0, driverOther = 0, driverLists = 0, skipped = 0;
    DWORD passThread = 0, driverThread = 0;
    bool inDriver = false;
    for (int i = 0; i < count; ++i)
    {
      Event const& e = g_Events[i];
      if (!e.Ready.load(std::memory_order_acquire))
      {
        ++skipped;
        continue;
      }
      if (e.Kind == KindDispatch || e.Kind == KindDispatchIndirect)
      {
        (e.Kind == KindDispatch ? runDispatches : runIndirect)++;
        runOther += !e.Immediate;
        if (inDriver)
        {
          ++driverDispatches;
          driverOther += !e.Immediate;
        }
        continue;
      }
      endRun();
      if (e.Kind == KindPass)
      {
        add("; pass %s -> %s", PassName(e.A), PassName(e.B));
        passThread = e.Thread;
        if (e.A == 1 && e.B != 1 && firstDriver < 0)
          decalsEnded = i;
      }
      else if (e.Kind == KindSsrBegin)
      {
        add("; their driver begins (view %d, thread %u)", (int)e.A, (unsigned)e.Thread);
        if (firstDriver < 0)
          firstDriver = i;
        driverThread = e.Thread;
        inDriver = true;
      }
      else if (e.Kind == KindSsrEnd)
      {
        add("; their driver ends");
        inDriver = false;
      }
      else if (e.Kind == KindCommandList)
      {
        add("; a command list run%s", e.Immediate ? "" : " (not on the immediate context)");
        driverLists += inDriver;
      }
    }
    endRun();
    char verdict[512];
    if (firstDriver < 0)
      snprintf(verdict, sizeof(verdict), "their driver didn't run in this frame");
    else
      snprintf(verdict, sizeof(verdict), "their driver ran %s the decal pass ended (event %d; the decals ended at %d), on %s "
        "thread as the pass changes (%u, %u), with %d dispatches during it (%d not on the immediate context) and %d command "
        "lists run during it", decalsEnded >= 0 ? "after" : "with no end of the decal pass before it, where", firstDriver,
        decalsEnded, driverThread == passThread ? "the same" : "another", (unsigned)driverThread, (unsigned)passThread,
        driverDispatches, driverOther, driverLists);
    util::log::Write("Screen-space reflections (diagnostic): frame %d of %d, from the tools' drawing at Present to the next, "
      "%d events%s: %s. In order%s%s", g_Taken, FramesTaken, count, skipped ? " (some not written in time)" : "", verdict,
      at > 0 ? line : "", at < 0 || at >= (int)sizeof(line) - 64 ? " ... (more)" : "");
  }
}

void ssrorder::NotePass(int from, int to)
{
  if (to == 0)
    g_SawGbuffer.store(true, std::memory_order_relaxed);
  Note(KindPass, from, to, true);
}

void ssrorder::AtPresent(ID3D11DeviceContext* pImmediate)
{
  if (!pImmediate || g_State == 4)
    return;
  ULONGLONG now = GetTickCount64();
  if (g_State == 0)
  {
    g_State = 1;
    g_Immediate.store(pImmediate, std::memory_order_relaxed);
    LogSettings((__int64)g_gameHandle);
    Hook(pImmediate);
    g_HookedAt = now;
    g_SawGbuffer.store(false, std::memory_order_relaxed);
    return;
  }
  if (g_State == 2)
  {
    // the frame taken
    g_Recording.store(false, std::memory_order_release);
    ++g_Taken;
    LogFrame();
    g_LastTakenAt = now;
    g_State = g_Taken < FramesTaken ? 1 : 3;
    if (g_State == 3)
    {
      g_CountFrom = now;
      g_CallsFrom = g_DriverCalls.load(std::memory_order_relaxed);
    }
    g_SawGbuffer.store(false, std::memory_order_relaxed);
    return;
  }
  if (g_State == 3)
  {
    if (now - g_CountFrom < CountForMs)
      return;
    long long calls = g_DriverCalls.load(std::memory_order_relaxed) - g_CallsFrom;
    util::log::Write("Screen-space reflections (diagnostic): their driver ran %lld times in %.0f s (%lld in all since the tools "
      "hooked it); the watch is over, the hooks only pass the calls on", calls, CountForMs / 1000.0,
      g_DriverCalls.load(std::memory_order_relaxed));
    g_State = 4;
    return;
  }
  // (1) a frame to take: once the game draws the world (a G-buffer pass since
  // the last look), a while after the hooks went in, a second apart
  bool world = g_SawGbuffer.exchange(false, std::memory_order_relaxed);
  if (!world || now - g_HookedAt < WaitFirstMs || (g_LastTakenAt && now - g_LastTakenAt < BetweenFramesMs))
    return;
  for (Event& e : g_Events)
    e.Ready.store(0, std::memory_order_relaxed);
  g_EventCount.store(0, std::memory_order_relaxed);
  g_State = 2;
  g_Recording.store(true, std::memory_order_release);
}
