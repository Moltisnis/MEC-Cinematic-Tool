#pragma once
#include <d3d11.h>

// The Panini projection (CAMERA > Wide fields of view): the game's finished frame
// drawn again at Present, before the menu, with the Panini projection (see
// Panini.hlsl). Wide fields of view keep the middle of the picture as it is
// and squeeze the sides instead of stretching them.
namespace panini
{
  // Camera hook, every frame: on or off, its distance (0: the normal view, 1:
  // the classic Panini), how much it crops to fill the screen (0 - 1), and
  // the view's vertical field of view (radians) and aspect
  void Set(bool on, float distance, float crop, float fovY, float aspect);

  // Render thread, from the Present hook, on the game's swapchain only
  void Apply(IDXGISwapChain* pSwapChain, ID3D11Device* pDevice, ID3D11DeviceContext* pContext);

  // Where a point in the normal view (x, y: right / depth and up / depth) is
  // in the Panini view, the same units; the point itself when it's off. For
  // the markers drawn over the picture.
  void ToPanini(float& x, float& y);
  // How big the Panini view's screen is against the normal view's (below 1:
  // it's cropped to fill the screen); 1 when it's off. A point's place on
  // screen is its Panini x, y over this times the normal view's extent.
  float ScreenScale();
}
