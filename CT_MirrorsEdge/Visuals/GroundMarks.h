#pragma once
#include <d3d11.h>

// The ground's marks round the camera (2026-11-01, the user: "cars/motorbikes
// should leave behind water trails if on puddles and pedestrians should have
// the pudde ripple effect when they walk", then "Do the same about snow
// too"): a top-down map, MarksTexels across at MarksTexel metres each (about
// 82 m), wrapping round as the camera moves (a world texel's place is its
// number modulo MarksTexels; the strips that come in are cleared). Red: in
// the snow how far it's pressed there, in the rain how wet a wheel that came
// out of a puddle left it (its wet tyre track); green: how stirred the water
// is (0 - 1); alpha: the ground's height there (Map::HeightLow +
// MarksHeightRange x it, set only where a mark is left): a mark is only on
// what's that high (not on an awning or a bridge over it).
// - Marks: the crowd's and the citizens' steps and the road traffic's back
//   wheels (DryShapes.cpp, from their moves), drawn into it on the graphics
//   card each frame (GroundMarks.hlsl) wherever the snow lies (red) or the
//   wet look is drawn (green). The traffic all as cars: a motorcycle can't be
//   told from a car yet (one swarm; which member is which isn't known on the
//   processor). A wheel carries water out of a puddle (DryShapes.cpp), its
//   wet track wearing off as it goes.
// - Filled: the snow's marks as the falling snow fills prints (WetLook.cpp's
//   SnowPrintFillSeconds), the wet tracks drying within WetTrackSeconds, the
//   water's stir within WaterMarkSeconds. Snow and rain take turns: the map
//   is cleared when it goes from one to the other.
// - Read by the wet look's pass (WetLook.hlsl's MarksAt): the snow pressed
//   there, the puddles and the wet ground stirred.
namespace groundmarks
{
  const int MarksTexels = 2048;
  const float MarksTexel = 0.04f;
  const float WaterMarkSeconds = 4.f, WetTrackSeconds = 30.f;
  // The heights alpha holds (WetLook.hlsl's MarksAt uses the same range): a
  // 16-bit fraction of MarksHeightRange (1.8 cm steps) from the map's
  // HeightLow, the camera's height less half of it when the map was cleared;
  // cleared again for a new one when the camera is MarksHeightDrift from its
  // middle (marks out of the range aren't kept)
  const float MarksHeightRange = 1200.f, MarksHeightDrift = 300.f;

  // Any thread (DryShapes, the game thread): a step at a point of the ground
  // (world), its sole pointing way (x, z; unit); a wheel's path since its
  // last place (world, on the ground), halfWidth its tyre's half width (m),
  // carried how much water it brings out of a puddle (0 - 1; its wet track)
  void Step(float const* at, float const* way);
  void Wheel(float const* from, float const* to, float halfWidth, float carried);

  struct Map
  {
    ID3D11ShaderResourceView* View;   // R16G16B16A16: red the snow pressed or the wet track, green the water stirred, alpha the height
    float PerMetre;                   // the map's uv per world metre (it wraps)
    float Middle[2];                  // world x, z of the window's middle
    float Reach;                      // metres from the middle its marks are kept to
    float HeightLow;                  // world height alpha 0 stands for (see MarksHeightRange)
  };
  // Render thread, each frame before the wet look's pass: the marks queued
  // since drawn into the map (red the snow's if snow lies, else the wet
  // tracks if the wet look is drawn; green if the wet look is drawn), the
  // window moved with the camera (world), red taken down by redFill (the snow
  // filling or the tracks drying) and green by waterFill (each 0 - 1 of a
  // full mark, this frame). False while neither is on (the queue let go; the
  // map cleared when it comes on again, goes from snow to rain, or the camera
  // has gone MarksHeightDrift up or down).
  bool Update(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, float const* camera, bool snow, bool water,
    float redFill, float waterFill, Map& out);

  // Diagnostic (game thread, DryShapes): a road traffic member followed
  // (its swarm's CrowdBoidsComponent, its record's number). The first
  // ProbedMembers' records (0x20 bytes: position, a dword, the place before,
  // a dword) and their swarm's component's first 0x100 bytes go in the log
  // once, read-only, to find which member is a motorcycle (its appearance
  // item; not known yet, so all the traffic is marked as cars)
  const int ProbedMembers = 8;
  void ProbeTraffic(__int64 boids, int agent);
}
