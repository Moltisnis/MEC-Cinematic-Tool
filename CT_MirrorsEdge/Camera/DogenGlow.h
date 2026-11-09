#pragma once
#include <d3d11.h>

// Dogen's kitchen lamps lit the game's way (2026-11-03, the user: "make the seem like they emit light just like any
// other light in the game"; DogenGlow.cpp): their panels' glow added into the game's G-buffer target 3, where the
// game's own emissive materials put theirs, so its lighting, reflections, bloom and exposure take it as theirs.
namespace dogenglow
{
  // Game thread (DogenDoor.cpp): whether the lamps glow (Faith near, the lamps found with their own look)
  void SetWanted(bool wanted);
  // Game thread (DogenDoor.cpp): their glow's share of the day's (0 - 1: dimmer at night, with their lights)
  void SetShare(float share);
  // Render thread (gameframe::OnPass, the replay's render target hook): the immediate context's pass changes
  // from -> to (ReplayGameDraws' Pass: -1 none, 0 the G-buffer, 1 its decals, 2 the lit, others), with the targets
  // about to be bound
  void OnPass(ID3D11DeviceContext* pContext, int from, int to, UINT count, ID3D11RenderTargetView* const* targets,
    ID3D11DepthStencilView* depth);
  // Render thread, at the end of Present (gameframe::FrameEnd)
  void FrameEnd();
}
