#pragma once
#include <cstdint>

// Faith's size (GAME > Faith > Size): her model only, about her feet
// (Research_Faith_Size_2026-11-03.md).
//
// Her body: the game builds her world pose and skinning matrices from her
// local pose's translations and rotations only (2026-11-03, live: with bone
// 1's scale at 0.50 her hips stayed 0.870 m from it and every matrix row had
// length 1). So each frame, from the camera hook, both of her pose slots (the
// body that's drawn, soldier + 0x248, and her box pose, + 0x340, whose bones
// make her bounding box) get their world pose and matrices sized about bone 1
// (AITrajectory, at her feet), bones from the hips on. A bone still holding
// what the tools wrote isn't sized again.
//
// Her hair: its cloth's root follows bone 1's own scale, which the game's
// pose override sets (its 4th translation float, whatever its weight:
// 0x142FCFA5A). An override of bone 1 only, weight 0, carries the size. The
// game never sets that scale back itself, so going back to 1.00 hands 1.00
// until her pose reads 1.00 again. That left the hair its normal size: with
// Hair follows size, her cloths' bone matrices (from her world pose, or the
// chain they make from her local pose, which undoes the size from her spine
// up) are her drawn body's, sized (so their anchors sit on her sized head),
// and their solvers are pointed at the tools' copies of their lengths, sized.
//
// Her first-person view: with the free camera off, the game's camera, when
// it's on her camera joint, is moved with her (the game places it from her
// unsized pose before the camera hook).
//
// Not changed: her collision, her moves (their distances are the game's, for
// her normal size), the free camera's own heights on her. At 1.00 the sized
// bones are put back as the game had them. While a replay records or plays,
// the size steps aside (the replay hands its own override to the same two
// slots).
namespace faith_size
{
  const float MinSize = 0.01f;
  const float MaxSize = 10.f;
  const float SliderPower = 3.32f;   // 1.00 in the middle of the slider

  // Once at start, after util::hooks::Init: hooks the game's collision
  // queries for the collision diagnostic (nothing changed by them)
  void Init();

  // False if the game isn't the build these offsets are from, or its pose
  // code isn't what the tools know (checked once)
  bool IsAvailable();

  // From the menu (kept between MinSize and MaxSize; 1 is the game's own)
  void SetSize(float size);
  float GetSize();

  // Collision follows size (the menu's tick, off at start): while she's
  // sized, her movement's capsules are handed to the game's casts sized (the
  // tools' copies), their bottoms where the game put them; her moves keep
  // their distances. False if the collision hooks aren't in.
  bool IsCollisionAvailable();
  void SetCollision(bool on);
  bool GetCollision();

  // Hair follows size (the menu's tick, on at start): while she's sized, her
  // cloths' anchors and lengths are her size too. False if the game's cloth
  // code isn't what the tools know (or its hook isn't in).
  bool IsHairAvailable();
  void SetHair(bool on);
  bool GetHair();

  // Every frame from the camera hook, after the replay's update: her soldier
  // (0: none), whether a replay records or plays, the main render view while
  // the game's own camera is shown (0 with the free camera on): her
  // first-person view is moved with her
  void Update(__int64 soldier, bool replayBusy, __int64 gameView);
}
