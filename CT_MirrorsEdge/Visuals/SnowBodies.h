#pragma once
#include <Windows.h>
#include <d3d11.h>

// The snow pressed in by bodies (VISUALS > Snow; 2026-10-27, the user: "The
// snow slide should be exactly faiths mesh instead of what it is now", "Ksec
// should leave behind snow footsteps and when they die and ragdoll they
// should leave behind their outline in the snow", "faiths snow handprints
// should be visible when she jump rolls / lands"): where a body's own mesh,
// posed as the game has it this frame, is down in the snow on the ground,
// it's pressed into a top-down mask (a stamp) the pass draws as the snow
// pressed in (WetLook.hlsl's SnowStampsAt). See SnowBodies.cpp.
namespace snowbody
{
  const int MostStamps = 16;                     // WetLook.hlsl's g_SnowStamp: the texture's slices
  const int StampAcross = 128, StampAlong = 256; // texels
  const float StampTexel = 0.02f;                // m a texel
  // What the stamps are pressed into (Update decides, Take is told): the snow
  // on the ground (VISUALS > Snow), the rain's water on it (the wet look: her
  // slide's wake takes her body's shape, WetLook.hlsl's SlideWake; 2026-10-27,
  // the user: "waterslideing should have faiths model outline just like snow
  // and smoothed out"), or nothing (all let go)
  enum Mode { ModeNone = 0, ModeSnow = 1, ModeWater = 2 };

  // A stamp for the pass: its texel (0, 0)'s corner on the ground, the way
  // its long side runs (its across is that turned: (-z, x)), the ground's
  // rise along it, how much the falling snow hasn't filled, its slice
  struct Stamp
  {
    float Corner[3];
    float Along[2];
    float Rise;
    float Left;
    int Slice;
  };

  // Game thread, once a frame (dry::Update, after the camera is final):
  // Faith's body (her soldier; her transform, 16 floats, rows right, up,
  // forward, position, or null) and the KrugerSec round the camera (their
  // lying bodies pressed into the snow or the rain's water)
  void Update(__int64 soldier, float const* faith, float const* camera);
  // Render thread, the pass (WetLook.cpp's Apply): the stamps' texture (made
  // the first time, its changed slices smoothed and sent to it) and the stamps
  // to draw (mode: the pass's, see Mode; the ones of another mode let go),
  // filled by fill (what the falling snow fills this frame, as the prints; in
  // the water, the part of their time gone). Null when there's no texture
  ID3D11ShaderResourceView* Take(ID3D11DeviceContext* context, float fill, int mode, Stamp* out, int& count);
  // Any thread: a KrugerSec's foot at a step's contact (foot: 1 their right,
  // -1 their left, 0 not known): the way its toe points (world x, z; unit),
  // from that foot's bones in their pose this frame. False: no such foot
  // within reach
  bool OtherFootWay(float const* contact, int foot, float* way);
  // A KrugerSec lying near the camera (LyingBodies; 2026-10-28, for the
  // weather session, the user: "ksec ragdoll/dying should leave behind their
  // model ripples"): his body this frame as capsules between his bones (head,
  // lower and upper torso, upper arms, forearms, hands, thighs, shins, feet;
  // world, m), each one's radius from an Enforcer's mesh (SnowBodies.cpp's
  // LyingCapsules). DownAt: when he lay down this time (GetTickCount64);
  // KnockAt: his last ragdoll knock (NoteKnock; 0 none since he lay down).
  const int LyingSegments = 16;
  struct LyingBody
  {
    __int64 Owner;            // his entity
    int Segments;
    float A[LyingSegments][3], B[LyingSegments][3], Radius[LyingSegments];
    ULONGLONG DownAt, KnockAt;
  };
  // Any thread: the KrugerSec lying near the camera at the last update (in
  // the snow or the rain), how many (at most most)
  int LyingBodies(LyingBody* out, int most);
  // Any thread: a downed body's knock at a point (its ragdoll's sound): the
  // KrugerSec whose hips are nearest it, within 2 m, gets it as his KnockAt
  void NoteKnock(float const* at);
  // Her handprints in the snow and the rain from her own hand (2026-10-28,
  // the user: "handprints for snow/rain should be faithful to faiths hand"):
  // at a touch, her hand as the game poses it over the next 0.4 s (her arms
  // mesh's own hand, its skin and glove, LOD0: SnowHandMesh.h), pressed where
  // it's within 1.5 cm of its nearest point to the surface touched, into a
  // stamp in that surface's plane (64 x 64 texels of 4 mm, its middle her
  // hand's, its U along her fingers). See SnowBodies.cpp.
  const int MostHandStamps = 32;      // WetLook.hlsl's g_HandStamps: the texture's slices
  const int HandStampSide = 64;       // texels
  const float HandStampTexel = 0.004f;
  // Any thread (Hooks.cpp's hand impacts): one of her hands touched a surface
  // (world; normal: its facing, either way round; hand 1 her right, -1 her
  // left). Gives its stamp (pass it to HandStampState; -1 none). The same
  // hand's touch within 300 ms and 15 cm of one is that one's.
  int NoteHandTouch(float const* world, float const* normal, int hand);
  // Any thread: a stamp's state: 1 her hand went into it (draw it, not
  // HandShape), 0 still being pressed (draw neither yet), -1 nothing went
  // in, or it's been given to another touch since (draw HandShape)
  int HandStampState(int stamp);
  // A pressed hand stamp for the pass: its middle (world), its U and V axes
  // (unit, in the surface's plane; its facing is U x V), its slice
  struct HandStamp
  {
    float Middle[3];
    float U[3], V[3];
    int Slice;
  };
  // Render thread: the hand stamps' texture (made the first time, its changed
  // slices sent), and the stamp a stamp number (NoteHandTouch's) has now
  // (false: not pressed). Null when there's no texture
  ID3D11ShaderResourceView* TakeHands(ID3D11DeviceContext* context);
  bool HandStampOf(int stamp, HandStamp& out);
  // Any thread: when Faith's body last pressed the snow by its mesh
  // (GetTickCount64; 0 never): her slide's or roll's own shape from
  // WetLook.cpp (its trail, its roll's print) isn't drawn as well then; they
  // are wherever her body doesn't press (it isn't read, or it isn't low)
  ULONGLONG FaithPressedAt();
  // Any thread: Faith's feet by her own bones at the last update (world, m):
  // her left foot (the ankle) and left toe base, her right foot and right toe
  // base; false if she wasn't read in the last FaithShoesMs (her shoes kept
  // as drawn under the snow, WetLook.hlsl's ShoesKeptAt)
  const ULONGLONG FaithShoesMs = 250;
  bool FaithShoes(float (*out)[3]);
  // Render thread: the texture let go (the device's going)
  void Release();
}
