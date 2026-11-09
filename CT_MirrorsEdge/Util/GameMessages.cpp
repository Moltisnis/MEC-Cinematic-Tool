#include "GameMessages.h"
#include "Util.h"
#include "../Main.h"
#include <Windows.h>
#include <atomic>
#include <cstring>

namespace
{
  // MessageManager_registerListener (0x142A3AD40): mov rcx, [rcx] (the
  // manager's handle to its implementation), then its body (0x142A3AD50): it
  // asks the listener its order (vtable slot 1), takes the manager's lock
  // (+ 0x40) and adds it to the category's list (+ 0x80). The game's own 298
  // registrations take the manager as [0x142401CB0] + 0x10 (the client's game
  // context, 161 of them), [0x14253D670] + 0x10 (the server's, 59) or
  // [0x142142CE0] (the core's, 31), realm 0xFF for any. Its dispatch
  // (0x142A394A0) calls each listener whose realm is the message's (+ 0x10) or
  // 0xFF: vtable slot 0 (listener, message), its result unused.
  const __int64 RegisterListenerRva = 0x2A3AD40;
  const __int64 RegisterBodyRva = 0x2A3AD50;
  const __int64 ClientContextRva = 0x2401CB0;
  const __int64 ServerContextRva = 0x253D670;
  const int ContextManagerOffset = 0x10;
  const unsigned AnyRealm = 0xFF;
  // A message: + 0x08 its category, + 0x0C its type (each the djb2-xor hash
  // of a name; the category's is the start the type names share)
  const int MessageCategoryOffset = 0x08;
  const int MessageTypeOffset = 0x0C;
  const unsigned SocialPlayCategory = 0x54776FC6;   // "PamSocialPlay"
  // The Dash messages ("named challenges"). The server sends them to the
  // player (the finished one from 0x1439DEB10, twice, the started one from
  // 0x143A83E80, through 0x1433D56A0). Their own fields from + 0x48: the
  // started and cancelled ones 8 bytes (0x50 in all), the finished one 0x20
  // (0x68 in all: + 0x48 an id or -1, + 0x4C which of the two it is, + 0x50,
  // + 0x54 a byte, + 0x58 a qword, + 0x60 a byte; what they hold isn't known)
  const unsigned NamedChallengeStarted = 0xC164590C;
  const unsigned NamedChallengeFinished = 0xD71D027F;
  const unsigned NamedChallengeCancelled = 0x9A9A27C6;
  const int DashFieldsOffset = 0x48;

  // The category's message types (from their names), for the log
  struct TypeName
  {
    unsigned Type;
    char const* Name;
  };
  const TypeName SocialPlayTypes[] = {
    { 0xE7D40012, "BlockedListUpdated" }, { 0x052D5907, "BlockedUsers" }, { 0x9B35497C, "ChallengeFinished" },
    { 0xFC74C3CA, "Control" }, { 0x924D6BD9, "DownloadSharedContent" }, { 0x85EBBF13, "EchoCustomizationChanged" },
    { 0xD089D3E2, "FetchSharedContents" }, { 0x36154959, "FireEvent" }, { 0x68F236EE, "FriendsList" },
    { 0xF33D51FF, "HackableBillboardsLeader" }, { 0xAA327A65, "InventoryChanged" }, { 0x320071B1, "MapMarkerUpdates" },
    { 0xB7633230, "NamedChallengeActivated" }, { 0xA78A38F4, "NamedChallengeBookmarkStatusRequest" },
    { 0xE6FD16CC, "NamedChallengeBookmarkStatusResult" }, { 0x9A9A27C6, "NamedChallengeCancelled" },
    { 0xD71D027F, "NamedChallengeFinished" }, { 0xC164590C, "NamedChallengeStarted" },
    { 0xA64F89EE, "NamedChallengeStatsUploaded" }, { 0x606B598C, "NamedChallengeTeleport" },
    { 0x955F5CBD, "NamedChallengeUploadReplay" }, { 0x5B61F030, "OnHideMap" }, { 0x855BB873, "OnShowMap" },
    { 0xA3118CF7, "PermissionRequest" }, { 0xF1C3CEE5, "PermissionResponse" }, { 0xFC61A69A, "PinSharedContent" },
    { 0x82AD8C5D, "RefreshHackableBillboardLeaderCache" }, { 0x9A0DFB2B, "RemoveSharedContent" },
    { 0x57D92A5D, "RequestHackableBillboardLeader" }, { 0x72F865F9, "RestartChallenge" },
    { 0xA390E6BE, "RunnerKitOpened" }, { 0x33D42557, "SocialTeleport" }, { 0x1CCA484E, "StartChallenge" },
    { 0x1846085D, "UgcFilterOptionChanged" }, { 0x5A464701, "UnpinSharedContent" },
  };

  char const* TypeNameOf(unsigned type)
  {
    for (TypeName const& t : SocialPlayTypes)
      if (t.Type == type)
        return t.Name;
    return nullptr;
  }

  // The listener objects, one per manager (0 core, unused; 1 client; 2 server)
  struct Listener
  {
    void const* const* VTable;
    int Manager;
  };
  void __fastcall OnMessage(Listener* self, unsigned char const* message);
  unsigned __fastcall ListenerOrder(Listener*) { return 100; }   // as GridNode's (the engine's usual)
  void const* const g_ListenerVTable[2] = { (void const*)&OnMessage, (void const*)&ListenerOrder };
  Listener g_Listeners[3] = { { g_ListenerVTable, 0 }, { g_ListenerVTable, 1 }, { g_ListenerVTable, 2 } };

  // Per manager: the game context and the implementation it was registered
  // with (0: not registered). [1] the camera hook's thread only, [2] the server's.
  struct Registration
  {
    __int64 Context;
    __int64 Impl;
  };
  Registration g_Registered[3];
  std::atomic<int> g_ListeningMask{ 0 };

  // The Dash messages that came, until taken
  const int MaxEvents = 32;
  SRWLOCK g_EventsLock = SRWLOCK_INIT;
  game_messages::DashEvent g_Events[MaxEvents];
  int g_EventCount = 0;
  std::atomic<int> g_EventsDropped{ 0 };

  // The category's other types that came, once each, for the log: (manager << 32) | type
  const int MaxOtherTypes = 64;
  std::atomic<unsigned long long> g_OtherTypes[MaxOtherTypes];
  int g_OtherLogged = 0;   // the camera hook's thread only

  void NoteOther(unsigned type, int manager)
  {
    unsigned long long packed = ((unsigned long long)(unsigned)manager << 32) | type;
    for (int i = 0; i < MaxOtherTypes; ++i)
    {
      unsigned long long was = g_OtherTypes[i].load(std::memory_order_relaxed);
      if ((was & 0xFFFFFFFFull) == type)
        return;
      if (!was)
      {
        unsigned long long expected = 0;
        if (g_OtherTypes[i].compare_exchange_strong(expected, packed))
          return;
        if ((expected & 0xFFFFFFFFull) == type)
          return;
      }
    }
  }

  void __fastcall OnMessage(Listener* self, unsigned char const* message)
  {
    __try
    {
      if (*(unsigned const*)(message + MessageCategoryOffset) != SocialPlayCategory)
        return;
      unsigned type = *(unsigned const*)(message + MessageTypeOffset);
      int kind = type == NamedChallengeStarted ? game_messages::DashStarted :
        type == NamedChallengeFinished ? game_messages::DashFinished :
        type == NamedChallengeCancelled ? game_messages::DashCancelled : 0;
      if (!kind)
      {
        NoteOther(type, self->Manager);
        return;
      }
      game_messages::DashEvent e{};
      e.Kind = kind;
      e.Manager = self->Manager;
      e.Tick = GetTickCount64();
      memcpy(e.Fields, message + DashFieldsOffset, kind == game_messages::DashFinished ? 0x20 : 8);
      AcquireSRWLockExclusive(&g_EventsLock);
      if (g_EventCount < MaxEvents)
        g_Events[g_EventCount++] = e;
      else
        g_EventsDropped.fetch_add(1);
      ReleaseSRWLockExclusive(&g_EventsLock);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
  }

  // mov rcx, [rcx]; jmp short to the body; the body's first: mov [rsp + 0x10],
  // edx; push rbp, rbx, rsi, rdi, r12, r14, r15
  bool IsRegisterCode()
  {
    static const BYTE thunk[] = { 0x48, 0x8B, 0x09, 0xEB, 0x0B };
    static const BYTE body[] = { 0x89, 0x54, 0x24, 0x10, 0x55, 0x53, 0x56, 0x57, 0x41, 0x54, 0x41, 0x56, 0x41, 0x57 };
    BYTE const* pBase = (BYTE const*)g_gameHandle;
    return !memcmp(pBase + RegisterListenerRva, thunk, sizeof(thunk)) && !memcmp(pBase + RegisterBodyRva, body, sizeof(body));
  }

  // The manager's handle and implementation now (false: none, or a fault reading them)
  bool ReadManager(__int64 contextRva, __int64& context, __int64& handle, __int64& impl)
  {
    context = handle = impl = 0;
    __try
    {
      context = *(__int64 const*)((BYTE const*)g_gameHandle + contextRva);
      if (context < 0x10000)
        return false;
      handle = context + ContextManagerOffset;
      impl = *(__int64 const*)handle;
      return impl >= 0x10000;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
  }

  // Registered again when the game made a new manager (a level load, a new
  // server): true when it was just now. The call itself isn't under __try:
  // a fault inside it would leave the manager's lock held.
  bool Keep(int manager, __int64 contextRva)
  {
    typedef unsigned char(__fastcall* tRegister)(__int64 handle, unsigned category, void* listener, unsigned realm);
    Registration& r = g_Registered[manager];
    __int64 context, handle, impl;
    if (!ReadManager(contextRva, context, handle, impl))
    {
      r.Context = r.Impl = 0;
      g_ListeningMask.fetch_and(~(1 << manager));
      return false;
    }
    if (r.Context == context && r.Impl == impl)
      return false;
    ((tRegister)((BYTE*)g_gameHandle + RegisterListenerRva))(handle, SocialPlayCategory, &g_Listeners[manager], AnyRealm);
    r.Context = context;
    r.Impl = impl;
    g_ListeningMask.fetch_or(1 << manager);
    return true;
  }

  bool Known()
  {
    static int s_known = -1;
    if (s_known < 0)
    {
      s_known = IsRegisterCode() ? 1 : 0;
      if (!s_known)
        util::log::Warning("The game's message manager isn't the code the tools know: Dashes aren't recorded by themselves");
    }
    return s_known == 1;
  }
}

void game_messages::ClientUpdate()
{
  if (g_shutdown || !Known())
    return;
  if (Keep(1, ClientContextRva))
    util::log::Write("Game messages: listening for the game's Dash messages on the client's message manager (0x%I64X)",
      g_Registered[1].Impl);
  // The category's other messages, each once (what time trials and the rest send)
  for (; g_OtherLogged < MaxOtherTypes; ++g_OtherLogged)
  {
    unsigned long long packed = g_OtherTypes[g_OtherLogged].load(std::memory_order_relaxed);
    if (!packed)
      break;
    unsigned type = (unsigned)(packed & 0xFFFFFFFFull);
    int manager = (int)(packed >> 32);
    char const* name = TypeNameOf(type);
    util::log::Write("Game messages: PamSocialPlay%sMessage (0x%08X) came, on the %s message manager (only noted)",
      name ? name : "?", type, manager == 1 ? "client's" : "server's");
  }
  int dropped = g_EventsDropped.exchange(0);
  if (dropped)
    util::log::Warning("Game messages: %d Dash messages weren't kept (more than %d came before the replay took them)", dropped,
      MaxEvents);
}

void game_messages::ServerUpdate()
{
  if (g_shutdown || !Known())
    return;
  if (Keep(2, ServerContextRva))
    util::log::Write("Game messages: listening for the game's Dash messages on the server's message manager (0x%I64X)",
      g_Registered[2].Impl);
}

int game_messages::TakeDashEvents(DashEvent* out, int max)
{
  AcquireSRWLockExclusive(&g_EventsLock);
  int count = g_EventCount < max ? g_EventCount : max;
  memcpy(out, g_Events, sizeof(DashEvent) * count);
  memmove(g_Events, g_Events + count, sizeof(DashEvent) * (g_EventCount - count));
  g_EventCount -= count;
  ReleaseSRWLockExclusive(&g_EventsLock);
  return count;
}

bool game_messages::Listening()
{
  return g_ListeningMask.load() != 0;
}

char const* game_messages::KindName(int kind)
{
  return kind == DashStarted ? "started" : kind == DashFinished ? "finished" : kind == DashCancelled ? "cancelled" : "?";
}
