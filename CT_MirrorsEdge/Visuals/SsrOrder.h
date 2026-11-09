#pragma once
#include <d3d11.h>

// Diagnostic, read-only (2026-11-02, the user said "Yes, check first" to the
// check that Research_ScreenSpace_2026-11-02.md's fix 7.A needs: the wet
// ground made smooth in the game's own G-buffer, for its own screen-space
// reflections and lighting). Two things that research couldn't tell offline:
// - whether the game's screen-space reflections are on at the user's
//   settings: GlobalPostProcessSettings' fields for them, read once through
//   the game's settings registry, and how often their driver (0x1435BBB00)
//   runs;
// - where they run in the frame: in 3 frames, in order, the immediate
//   context's pass changes (the replay's render target hook, through
//   gameframe::OnPass), the driver's start and end (a hook on it that only
//   notes them), and the compute dispatches on the game's contexts. Fix 7.A
//   needs the driver after the G-buffer's decal pass has ended.
// Nothing of the game's is written: its calls go on as they were.
namespace ssrorder
{
  // Render thread, each frame at Present (wetlook::Apply): the settings read
  // and the hooks put in the first time; a frame's order taken from one
  // Present to the next, then in the log
  void AtPresent(ID3D11DeviceContext* pImmediate);
  // Render thread (gameframe::OnPass): the immediate context's pass changes
  // from -> to (ReplayGameDraws' Pass: -1 none of its kinds, 0 the G-buffer,
  // 1 its decals, 2 lit, 3 half size, 4 the environment's)
  void NotePass(int from, int to);
}
