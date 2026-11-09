#include "GameState.h"
#include "Util.h"
#include "../Main.h"
#include <Windows.h>
#include <atomic>
#include <cstring>

// The game's pause and loading screen (2026-10-26, offline from the game's
// code, the reads checked live).
// - The game's UI system (PamUISystem) is what 0x143269AE0 hands out:
//   mov rax, [0x1424157F0]; ret. Its vtable is 0x141CB63B0 (slot 0, its
//   getType, gives PamUISystem's type info 0x14288EE60).
// - The pause: PamClientUIPauseEntity (vtable 0x141CBD4A8) takes its events
//   in slot 4 (0x143B3AC80): "Pause" (0x0DD8DDF7, its name's hash) and
//   "Resume" (0xC44B5C3C). Each goes, once per entity, to 0x143B16210 with the
//   UI system's [+ 0x8C8]: it adds 1 to (Pause) or takes 1 from (Resume) the
//   count at + 8, and the one at + 0xC as well unless the entity's
//   PostponePauseUntilScreenIsFadedIn is set. So the count at + 8 is how
//   many pauses are asked for now; the game is paused while it's above 0.
// - The loading screen: PamClientUILoadScreenControlEntity's show and hide
//   events (see Hooks.cpp, hLoadScreenEvent) go to 0x143B11A30 / 0x143B11A60
//   with the UI system. Without the loading screen's widget ([+ 0x928] + 0x40
//   0) they set the byte at [+ 0x928] + 0x60 to 1 and 0; with it the show
//   only goes to the widget (0x143AEC540, its + 0x1A0 to 2) and that byte
//   stays 0 (2026-10-26 20:50, a mission restart's loading screen: up, + 0x60
//   0, + 0x68 1). Nothing else calls either, so the events as the hooks saw
//   them count too (util::hooks::LoadScreenShown).
// Read-only, inside __try; nothing is called.
namespace
{
  const __int64 UiSystemCodeRva = 0x3269AE0;      // mov rax, [rip + x]; ret
  const __int64 UiSystemGlobalRva = 0x24157F0;
  const __int64 UiSystemVTableRva = 0x1CB63B0;
  const int UiPauseOffset = 0x8C8;
  const int PauseCountOffset = 0x08;
  const int MostPauses = 64;                      // more than this isn't a count
  const int UiLoadScreenOffset = 0x928;
  const int LoadScreenShownOffset = 0x60;

  std::atomic<int> g_Checked{ 0 };   // 0 not yet, 1 the code is the one known, -1 it isn't

  bool CodeKnown()
  {
    int checked = g_Checked.load(std::memory_order_relaxed);
    if (checked)
      return checked > 0;
    bool known = false;
    __try
    {
      BYTE const* code = (BYTE const*)g_gameHandle + UiSystemCodeRva;
      int rel = 0;
      memcpy(&rel, code + 3, 4);
      known = code[0] == 0x48 && code[1] == 0x8B && code[2] == 0x05 && code[7] == 0xC3 &&
        (__int64)code + 7 + rel == (__int64)g_gameHandle + UiSystemGlobalRva;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      known = false;
    }
    g_Checked.store(known ? 1 : -1);
    if (!known)
      util::log::Warning("The game's UI system isn't where the tools know it: they can't tell its pause menu or loading "
        "screen (the thunder goes on in them)");
    return known;
  }

  // The UI system, if it's there and is one
  __int64 UiSystem()
  {
    if (!g_gameHandle || !CodeKnown())
      return 0;
    __try
    {
      __int64 ui = *(__int64 const*)((__int64)g_gameHandle + UiSystemGlobalRva);
      if (ui < 0x10000 || *(__int64 const*)ui != (__int64)g_gameHandle + UiSystemVTableRva)
        return 0;
      return ui;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return 0;
    }
  }
}

bool gamestate::Paused()
{
  __int64 ui = UiSystem();
  if (!ui)
    return false;
  __try
  {
    __int64 pause = *(__int64 const*)(ui + UiPauseOffset);
    if (pause < 0x10000)
      return false;
    int count = *(int const*)(pause + PauseCountOffset);
    return count > 0 && count <= MostPauses;
  }
  __except (EXCEPTION_EXECUTE_HANDLER)
  {
    return false;
  }
}

bool gamestate::Loading()
{
  // Shown through its widget, the byte below stays 0 (see above)
  if (util::hooks::LoadScreenShown())
    return true;
  __int64 ui = UiSystem();
  if (!ui)
    return false;
  __try
  {
    __int64 screen = *(__int64 const*)(ui + UiLoadScreenOffset);
    return screen >= 0x10000 && *(BYTE const*)(screen + LoadScreenShownOffset) != 0;
  }
  __except (EXCEPTION_EXECUTE_HANDLER)
  {
    return false;
  }
}
