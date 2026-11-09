#pragma once
#include <d3d11.h>
#include <string>

class INIReader;

// VISUALS > Images: the user's own pictures (JPG and PNG files in the game's
// "Cinematic Tools\Images" folder) standing in the city, the way the game
// shows pictures on its video screens (LinearMedia billboards; its first
// mission, Release, starts a newscast feed on them). Each is a flat rectangle
// with the picture on it, put in front of the camera or on what was clicked
// on the picture, then moved, turned, sized and faded in the menu.
//
// They're drawn over the game's finished frame at Present (after the wet
// surfaces, before the green screen, the lens distortion and the Panini
// projection, which bend them with the rest), only where they're nearer than
// what the game drew there (the green screen's copy of the game's depth,
// see greenplane::TakeFrameDepth): whatever stands in front of one stays in
// front of it. The game's light, fog, exposure, depth of field and motion
// blur don't touch them: they show the picture's own colors.
namespace worldimages
{
  // Render thread, from the Present hook, on the game's swapchain only
  void Apply(IDXGISwapChain* pSwapChain, ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
  // Render thread, in the menu's ImGui frame before the lights' dots: the
  // click on the picture while placing by click, each picture's dot dragged
  // with the mouse (as the lights' dots; a click used is taken off ImGui's
  // input), and the outlines of the pictures open in the menu
  void DrawOverlay();
  // Render thread: VISUALS > Images
  void DrawUI();
  // Render thread: true once after a dot was clicked, for the menu to open
  // the Images header
  bool TakeOpenRequest();

  // Main's config, the [Images] section
  void ReadConfig(INIReader* pReader);
  std::string GetConfig();
}
