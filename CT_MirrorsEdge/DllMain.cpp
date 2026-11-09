#include "Main.h"
#include "Camera/ReplayAudio.h"
#include "Util/Util.h"
#include <cstdio>

// Its destructor runs after every other static object of the tools is gone
// (lib objects are made first and destroyed last): CT.log then shows the
// tools' part of the process's end got all the way through
#pragma warning(disable: 4073)
#pragma init_seg(lib)
namespace
{
  bool g_exitNoted = false;   // the process detach below was logged

  struct LastToGo
  {
    ~LastToGo()
    {
      if (g_exitNoted)
        util::log::ExitNote("Exit: the tools' objects are gone; the rest of the process's end is Windows' and the game's");
    }
  };
  LastToGo g_lastToGo;
}

DWORD WINAPI RunCT(LPVOID lpArg)
{
  g_mainHandle = new Main();
  if (!g_mainHandle->Initialize())
  {
    delete g_mainHandle;
    g_mainHandle = nullptr;
    return 0;
  }
  g_mainHandle->Run();

  // Run only returns when the game's window closes. The tools' objects are
  // left as they are then (2026-10-27): deleting them ran their destructors on
  // this thread while the game was still closing on its own, putting values
  // back in game memory and releasing the menu's D3D objects while the game
  // took down its device, and freed Main while the game's threads could still
  // be in a hook or the window procedure that uses g_mainHandle. Windows
  // frees it all with the process.
  return 0;
}

DWORD WINAPI DllMain(_In_ HINSTANCE hInstance, _In_ DWORD fdwReason, _In_ LPVOID lpvReserved)
{
  if (fdwReason == DLL_PROCESS_ATTACH)
  {
    g_dllHandle = hInstance;

    // Only one copy of the tools per game process, e.g. when both the
    // version.dll loader and an injector load it (from different folders)
    char mutexName[64];
    sprintf_s(mutexName, "CT_MirrorsEdge_%lu", GetCurrentProcessId());
    HANDLE instanceMutex = CreateMutexA(NULL, FALSE, mutexName);
    if (!instanceMutex || GetLastError() == ERROR_ALREADY_EXISTS)
      return 1;

    CreateThread(NULL, NULL, RunCT, NULL, NULL, NULL);
  }
  else if (fdwReason == DLL_PROCESS_DETACH && lpvReserved)
  {
    // The game process is ending: Windows has already stopped every other
    // thread (one may have held a lock), and the tools' static objects are
    // destroyed after this returns
    replay_audio::OnProcessExit();
    util::log::ExitNote("Exit: the game process is ending (Windows has stopped its other threads): the tools' objects go");
    g_exitNoted = true;
  }

  return 1;
}
