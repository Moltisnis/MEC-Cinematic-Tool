#pragma once
#include <DirectXMath.h>
#include <cmath>
#include <string>
#include <vector>

// Game speed's range (GAME > Game speed, and keyframes'): 0 is Freeze time
const float GameSpeedMin = 0.05f;
const float GameSpeedMax = 2.f;

// How a keyframe moves on to the next one
enum KeyframeInterpolation
{
  Interpolation_Smooth,    // Catmull-Rom spline through the keyframes
  Interpolation_Linear,    // Straight line, constant speed
  Interpolation_EaseInOut, // Smooth path, cubic bezier ease in and out of the keyframes
  Interpolation_Hold,      // Stay on this keyframe, then cut to the next
  Interpolation_Count
};

struct CatmullRomNode
{
  DirectX::XMFLOAT3 Position{ 0,0,0 };
  DirectX::XMFLOAT4 Rotation{ 0,0,0,1 };
  float FieldOfView{ 60.f };
  float TimeStamp{ 0 }; // Seconds from the start of the track
  int Interpolation{ Interpolation_Smooth }; // For the segment starting at this node
  float MotionBlur{ 0.5f }; // WorldRenderSettings MotionBlurScale (game default 0.5)
  float TimeOfDay{ -1.f };  // Seconds 0 - 86400, -1 = not recorded (no level was loaded)
  bool TimeOfDayBackward{ false }; // The clock runs backwards to the next keyframe
  // Made while the camera circled the focus (CAMERA > Around the focus > Orbit),
  // or ticked Orbit keyframe afterwards: where round it. Between two of
  // these the track goes round the focus where it is when it plays, looking
  // at it, instead of along the straight line between the two places
  // (2026-10-24: two keyframes made orbiting Faith played as a line past
  // her). OrbitDistance 0: never placed round it.
  bool Orbit{ false };
  float OrbitYaw{ 0.f };       // radians round it, from the heading it turns with (see CameraManager::OrbitBaseYaw)
  float OrbitPitch{ 0.f };     // radians up from level
  float OrbitDistance{ 0.f };  // metres
  // The point it goes round: the focus where it was when it was ticked or
  // made circling (2026-11-06, the user: "There is a bug where orbit ticked
  // camera follows faith": they went round the focus where it was when the
  // track played or was drawn, so a ticked keyframe and the moves to and from
  // it went wherever Faith went, though ticking was to leave it where it
  // was). Its OrbitYaw is then round it from the world's heading, no turn
  // added (see CameraManager::OrbitBaseYaw). Following what moves is FOLLOW's
  // (a track moved with what's followed). Not set: round the focus where it
  // is, from the heading the circle turns with, as before.
  bool OrbitFocusSet{ false };
  DirectX::XMFLOAT3 OrbitFocus{ 0,0,0 };
  // Its own view, as a turn on top of looking at the focus (row-vector order:
  // view = OrbitView * looking at it), worked out when it's ticked so its view
  // stays as it was, then turning with it round the focus (2026-10-25: a
  // keyframe looking away from Faith next to her turned round to face her).
  // None for keyframes made while circling: they look at the focus.
  DirectX::XMFLOAT4 OrbitView{ 0,0,0,1 };
  // The way round from here to the next orbit keyframe, as found when one of
  // them was ticked (CameraManager::FindOrbitWay): 0 the shorter way (as
  // before), 1 round to the camera's right (the yaw grows), -1 to its left;
  // OrbitTurns whole turns more that way; OrbitLift radians the arc rises
  // in its middle (sin-shaped, 0 at both keyframes) to clear what's in the way
  int OrbitWay{ 0 };
  int OrbitTurns{ 0 };
  float OrbitLift{ 0.f };
  // Metres the arc to the next goes out from the focus in its middle (less
  // than 0: in towards it), sin-shaped as the lift; with the lift, set by
  // dragging the arc's handle on the drawn path (2026-11-06, the user: "Orbit
  // keyframe improvements (make it be able to affect the arch of the dots)")
  float OrbitBulge{ 0.f };
  // The hand-held shake's strength at this keyframe (0 - 1; -1: not kept),
  // used while the track sets the shake (Keyframe options)
  float Shake{ -1.f };
  // The game speed at this keyframe (GameSpeedMin - GameSpeedMax; -1: not
  // kept), used while the track sets the game speed (Keyframe options)
  float GameSpeed{ -1.f };
  // The path's curve round this keyframe, set by hand (its handles dragged on
  // the drawn path, CAMERA > Show the track's path): Bezier handles, metres
  // from the keyframe's place, the way in from the keyframe before and the
  // way out to the next. A Smooth or Ease in/out segment with a handle at
  // either end is the cubic Bezier through them (an end without one keeps
  // its automatic tangent). Handles off: the automatic curve, as before.
  bool Handles{ false };
  bool HandlesFree{ false };  // each handle moves on its own (a corner); off, the other stays in line with it
  DirectX::XMFLOAT3 HandleIn{ 0,0,0 };
  DirectX::XMFLOAT3 HandleOut{ 0,0,0 };
};

// Which way a camera circling something is from it: round by yaw (radians,
// the camera's own heading, its back row's), up by pitch
inline DirectX::XMVECTOR OrbitDirection(float yaw, float pitch)
{
  float level = cosf(pitch);
  return DirectX::XMVectorSet(sinf(yaw) * level, sinf(pitch), cosf(yaw) * level, 0.f);
}

// An orbit keyframe with no view of its own: it looks at the focus
// (CatmullRomNode::OrbitView exactly the identity)
inline bool IsPlainOrbitView(DirectX::XMFLOAT4 const& view)
{
  return view.x == 0.f && view.y == 0.f && view.z == 0.f && view.w == 1.f;
}

struct Camera
{
  DirectX::XMFLOAT3 Position{ 0,0,0 };
  DirectX::XMFLOAT4 Rotation{ 0,0,0,1 };
  float FieldOfView{ 60.f }; // Original MEC tools started at 60 degrees
  float MotionBlur{ 0.5f };  // Motion blur strength while the override is on
  float TimeOfDay{ -1.f };   // The game's time of day, updated every frame while the camera is on

  float MovementSpeed{ 3.0f };
  float RotationSpeed{ DirectX::XM_PI/4 };
  float RollSpeed{ DirectX::XM_PI / 8 };
  float FovSpeed{ 20.0f };
  float MouseSensitivity{ 0.0015f }; // Radians per mouse count

  float dX{ 0 };
  float dY{ 0 };
  float dZ{ 0 };
  float dPitch{ 0 };
  float dYaw{ 0 };
  float dRoll{ 0 };
  float dFov{ 0 };

  // Mouse look, already in radians (not scaled by dt or speed)
  float dMousePitch{ 0 };
  float dMouseYaw{ 0 };

  // While it circles the focus (CAMERA > Around the focus > Orbit): where round
  // it, for new keyframes (see CatmullRomNode::Orbit): the focus there
  // (OrbitFocus) and the yaw round it from the world's heading
  bool Orbiting{ false };
  DirectX::XMFLOAT3 OrbitFocus{ 0,0,0 };
  float OrbitYaw{ 0.f };
  float OrbitPitch{ 0.f };
  float OrbitDistance{ 0.f };
  // The hand-held shake's strength as set (for new keyframes)
  float ShakeStrength{ 0.f };
  // Game speed as set (the slider's; for new keyframes)
  float GameSpeed{ 1.f };

  // Frostbite LinearTransform layout: right, up, forward, translation.
  // The camera looks down -forward (the original tools moved along -row 2 on W).
  DirectX::XMFLOAT4X4 Transform{  1,0,0,0,
                                  0,1,0,0,
                                  0,0,1,0,
                                  0,0,0,1 };
};

struct CameraTrack
{
  explicit CameraTrack(std::string const& name) : Name(name) {}

  std::string Name;
  std::vector<CatmullRomNode> Nodes;
};
