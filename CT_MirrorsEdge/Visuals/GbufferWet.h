#pragma once
#include <d3d11.h>

// VISUALS > Weather > Wet surfaces > "In the game's own lighting" (see
// GbufferWet.cpp and GbufferWet.hlsl): the wet look's wet ground written into
// the game's own G-buffer before the game lights it, so the game's own
// screen-space reflections, cube maps and lights show it; the wet look at
// Present then leaves its ground's darkening, sheen and reflection out.
// (2026-11-02, the user: "Lets go for A option": Research_ScreenSpace_
// 2026-11-02.md section 7.A.)
namespace gbufferwet
{
  // The menu (any thread): on (the default) or off (the wet look's own
  // reflection at Present, as before)
  void SetOn(bool on);
  bool On();
  // Render thread (gameframe::OnPass, the replay's render target hook): the
  // immediate context's pass changes from -> to (ReplayGameDraws' Pass: -1
  // none, 0 the G-buffer, 1 its decals, 2 the lit, others), with the targets
  // about to be bound. Drawn as the frame's first lit pass begins (after the
  // G-buffer and decal passes, before the game's screen-space reflections and
  // its lights: the order CT.log's "Screen-space reflections (diagnostic)"
  // showed, 2026-11-02 21:36)
  void OnPass(ID3D11DeviceContext* pContext, int from, int to, UINT count, ID3D11RenderTargetView* const* targets,
    ID3D11DepthStencilView* depth);
  // Render thread, at Present before the wet look's pass: whether the wet
  // ground was written into this frame's G-buffer
  bool DrawnThisFrame();
  // Render thread, at Present: its pass's info this frame, where it's open
  // and kept dry at the depth's texels (WetLook.hlsl's g_WetInfo, t10), for the
  // wet look not to work them out again; null when it wasn't drawn or doesn't
  // cover the depth (a viewport smaller than it)
  ID3D11ShaderResourceView* InfoThisFrame();
  // The ground's nodes (WetLook.hlsl's NodesPs and GroundGoesOn: where a
  // puddle can lie, worked out at fixed points round the camera, 2026-11-03),
  // drawn by this pass before its info, or at Present by the wet look when it
  // wasn't. Render thread:
  // - NodesReady: whether they can be drawn (their shader made; the first
  //   call starts making it);
  // - DrawNodes: drawn now into their target with the wet look's constants
  //   (b0), the frame's depth (t1) and where the camera its places are
  //   relative to is (world): the context's targets, viewport, shaders,
  //   states, t1 and b0 and b4 bound to the pixel shader are changed (the
  //   caller puts its own back); false if they can't be;
  // - NodesThisFrame: their view (t11) and constants (b4) if drawn this
  //   frame, null and null if not.
  bool NodesReady(ID3D11DeviceContext* pContext);
  bool DrawNodes(ID3D11DeviceContext* pContext, ID3D11Buffer* constants, ID3D11ShaderResourceView* depth, float const camera[3]);
  ID3D11ShaderResourceView* NodesThisFrame(ID3D11Buffer** constants);
  // Render thread, at the end of Present (gameframe::FrameEnd)
  void FrameEnd();
  // The menu (any thread): the game's G-buffer before and after the pass,
  // its depth and view constants and the pass's info, saved once into
  // Cinematic Tools\GbufferDump\ the next frame it's drawn (diagnostic)
  void RequestDump();
}
