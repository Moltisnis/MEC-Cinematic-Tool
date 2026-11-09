#pragma once

// What the wet surfaces leave alone (the user, 2026-10-26: "peddles should
// not affect faith (they especially do when she jumps) or other people",
// "vechicles bikes trains as well"): see DryShapes.cpp
namespace dry
{
  const int MostBoxes = 64;
  const int MostBodies = 24;

  // A box in the world: its middle, its three axes (unit) and its half sizes
  // along them
  struct Box
  {
    float Middle[3];
    float Axis[3][3];
    float Half[3];
    bool Body;          // Faith's or a person's: a little wet out in the rain (WetLook.hlsl's BodyWetness)
    bool Floored;       // a vehicle's or a train's standing on the ground: the ground under it left as it is (as a body's)
    float FeetBelow;    // a body's or a floored box's: the ground this far under the box's floor (WetLook.hlsl's
                        // KeptDryKind; < 0 over it), more than -0.5
  };

  // Near the camera while it's in Faith's head (her own first-person arms
  // and legs, see DryShapes.cpp): what's no lower than Floor (world y)
  struct Near
  {
    bool On;
    float Floor;
  };

  // Game thread, the camera hook, once a frame after the camera is final.
  // faith: her transform (16 floats, rows right, up, forward, position), or
  // null when she isn't known
  void Update(__int64 pRenderView, __int64 pSoldier, float const* faith);
  // Render thread: the last update's boxes, nearest first (how many), and
  // the near part
  int Get(Box* out, int most, Near& close);
  // Render thread: where Faith's feet were at the last update (her
  // transform's place), false when she wasn't known
  bool FaithFeet(float* at);
  // Render thread: her transform's forward row at the last update, over the
  // ground (x, z, unit); which way along it she faces isn't known here (see
  // WetLook.cpp's HerForward). False when she wasn't known
  bool FaithForward(float* way);
  // Any thread: the people (KSec of every kind and everyone else, the
  // crowds) and drones round the camera at the last update, not Faith, as
  // boxes round their bodies, nearest first (how many): the rain is kept
  // out of them (Weather.cpp)
  int GetBodies(Box* out, int most);
}
