#pragma once
#include <d3d11.h>

// The green screen (VISUALS > Green screen): a flat rectangle of one color
// standing in the world, to key out when editing, and the whole sky in that
// color if asked. They're drawn over the game's finished frame at Present
// (before the Panini projection and the menu) in exactly that color, only
// where the rectangle is nearer than what the game drew there (or only over
// the sky), going by a
// copy of the game's depth, taken from its depth target (the one shaped like
// the frame) before the game clears it a second time in a frame, else at
// Present. Projected with the game's own camera and projection (the main
// render view's).
namespace greenplane
{
  struct Settings
  {
    bool On;
    float Distance;      // metres from where the camera was when it was put there, along where it looked
    float Width, Height; // metres
    float TurnDegrees;   // round the upright, from facing that camera
    float Raise;         // metres up (down below 0)
    float Color[3];
    bool KeepWorld;      // it covers only the sky inside its outline: nothing in the world is hidden
    bool GreenSky;       // the whole sky in its color (with the plane shown or not)
  };

  // The menu (any thread)
  void Set(Settings const& settings);
  // Put it where the camera looks at the next frame (facing it)
  void RequestPlace();
  bool IsPlaced();
  // Its middle now; false before it's put anywhere
  bool GetPlace(float* center);

  // The camera hook, on the game's thread once the main render view's camera
  // is final for the frame: its transform and projection, kept for Present
  // (read there they could be half written)
  void SetView(__int64 pRenderView);

  // Render thread, from the Present hook, on the game's swapchain only
  void Apply(IDXGISwapChain* pSwapChain, ID3D11Device* pDevice, ID3D11DeviceContext* pContext);

  // From the depth clear hook (see util::hooks::HookDepthClears), before the
  // game clears a depth target; any thread the game clears on
  void OnDepthClear(ID3D11DeviceContext* pContext, ID3D11DepthStencilView* pView, UINT flags);

  // The scene's depth for something else drawn at Present (VISUALS > Weather's
  // wet surfaces, VISUALS > Images): kept, with the camera, while any of them
  // wants it (any thread)
  enum DepthUser { WantWetLook = 1, WantImages = 2, WantReplayShapes = 4 };
  void WantDepth(bool want, unsigned user = WantWetLook);
  struct FrameDepth
  {
    ID3D11ShaderResourceView* View;   // this frame's copy, the green screen's own (not to be released)
    float Scale[2];                   // the copy's size over the frame's
    bool Reversed;                    // nearer = bigger
    float SkyDepth;                   // farther than this is the sky
    float Transform[16];              // the camera (a LinearTransform, looking along -row 2)
    float Projection[16];             // row vectors
    UINT Width, Height;               // the frame
    UINT DepthWidth, DepthHeight;     // the copy's (the game's render size: the frame's at a resolution scale of 100 %)
  };
  // Render thread, from the Present hook before Apply: this frame's (the
  // copy taken now unless it was before the game's second clear; Apply then
  // uses the same one). False when there's none yet (no camera, no depth)
  bool TakeFrameDepth(IDXGISwapChain* pSwapChain, ID3D11Device* pDevice, ID3D11DeviceContext* pContext, FrameDepth& out);
  // Render thread, between the game's first clear of the scene's depth in a
  // frame and Present (the wet ground in the game's G-buffer, GbufferWet.cpp):
  // this frame's camera, the one TakeFrameDepth will give (false: not taken
  // at a clear this frame), and how many times the scene's depth has been
  // cleared since the last Present
  bool FrameCamera(float* transform, float* projection, int& clears);
}
