#pragma once
#include <d3d11.h>

// The game's own G-buffer, for the snow (WetLook): copies of its normals,
// base colour and material targets and of its view constants, made on the
// graphics card as the frame's first lit pass begins (the G-buffer whole:
// its decals and the G-buffer passes after them drawn; until 2026-11-02 as
// its first G-buffer pass ended, before those) and as that pass ended (seen
// through the replay's render target hook, ReplayGameDraws.cpp), only while
// they're wanted. The wet ground written into the G-buffer (GbufferWet.h)
// reads them, just after. And which depth target is the scene's: the one the G-buffer pass
// draws with (GreenPlane.cpp takes no other once it's known).
//
// What the targets hold (2026-11-01, read from the game's own shaders,
// offline: a city material shader that writes all four, and the sun's
// deferred light pass that reads them; Research_Snow section 23):
// - target 0, R10G10B10A2: xy the normal in one of 6 bases (x 0.5 + 0.5; its
//   z is sqrt(1 - x^2 - y^2)), z the smoothness, w a material kind (2 bits);
// - target 1, R8G8B8A8 sRGB: rgb the base colour, a the normal's basis / 6;
// - target 2, R8G8B8A8: y the metal mask, z the reflectance, w the sky
//   visibility (the light map's, x 3.5 + the material's own, saturated);
// - viewConstants (the shaders' b2, 1120 bytes): normalBasisTransforms[6] at
//   416, 3 float4s each: the world normal's c-th part is the local normal
//   dotted with the c-th of them.
// Nothing of the game's is written: copies into the tools' own textures and
// buffer, on its context.
namespace gameframe
{
  // Render thread, each frame: whether the copies are wanted for the coming
  // frames (the hook is put in the first time); all: the three targets (the
  // snow's light reads the material's), else the normals and the colour only
  // (the facing, WetLook.hlsl's GameSurfaceAt: the wet look's in the rain,
  // a third less to copy; 2026-11-01, the weather session)
  void Want(ID3D11DeviceContext* pImmediate, bool copies, bool all = true);
  // Render thread (the replay's render target hook): the immediate
  // context's pass changes from -> to (ReplayGameDraws' Pass: -1 none, 0 the
  // G-buffer, 1 its decals, 2 the lit, others), with the targets about to be
  // bound
  void OnPass(ID3D11DeviceContext* pContext, int from, int to, UINT count, ID3D11RenderTargetView* const* targets,
    ID3D11DepthStencilView* depth);
  // Any thread: whether a depth texture is one the game's G-buffer pass has
  // drawn with lately (the last 4 seen, as the game may take turns with two):
  // -1 no G-buffer pass seen yet, 1 it is, 0 it isn't
  int IsSceneDepth(__int64 texture);
  // Render thread: the depth targets known so far given up on (GreenPlane:
  // none of them cleared for a while, another one shaped like the frame
  // was): not learned again, a new one is
  void ForgetSceneDepths();
  // Render thread: the depth targets known so far let go of (not given up
  // on: they're learned again when seen), when the game's render size the
  // passes are told apart at changes (ReplayGameDraws, LearnRenderSize): a
  // depth of the old size (a pass of a menu, say) isn't the scene's any more
  void ResetSceneDepths();

  struct Frame
  {
    ID3D11ShaderResourceView* Normals;    // target 0
    ID3D11ShaderResourceView* Colour;     // target 1, read as linear (sRGB)
    ID3D11ShaderResourceView* Material;   // target 2
    ID3D11Buffer* View;                   // viewConstants
    UINT Width, Height;
  };
  // Render thread, at Present: this frame's copies (false: none this frame;
  // they're the tools' own, kept alive until the next copy)
  bool Take(Frame& out);
  // Render thread, at the end of Present: the frame's copies given up
  void FrameEnd();
  // Render thread, at Present, with this frame's copies taken (Take true):
  // how far the camera the tools draw this frame with (transform,
  // projection: greenplane::TakeFrameDepth's, the camera hook's at the
  // frame's first depth clear) is off the game's own camera of the frame
  // (the viewConstants copy's crViewProjMatrix and cameraPos), and whether
  // the frame before's or after's fits it better. Read back a few frames
  // later, twice a second; logged every 10 s (diagnostic). width, height:
  // the frame's.
  void CheckCamera(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, float const* transform, float const* projection,
    float width, float height);
}
