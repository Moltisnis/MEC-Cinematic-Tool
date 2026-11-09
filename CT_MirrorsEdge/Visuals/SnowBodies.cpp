#include "SnowBodies.h"
#include "SnowBodyMesh.h"
#include "SnowHandMesh.h"
#include "WetLook.h"
#include "Weather.h"
#include "../Main.h"
#include "../Util/Util.h"
#include <Windows.h>
#include <wrl/client.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <mutex>

// The snow pressed in by bodies (see SnowBodies.h). Each frame Faith's body
// and the KrugerSec's round the camera are read as the game poses them (read
// only, every read inside __try): a body's skinning matrices, the world pose
// times each bone's inverse bind pose, 3x4 rows (FaithReplay.cpp, 2026-10-23,
// read live: "the world pose times a fixed transform per bone, the same in
// three bodies of that skeleton"), checked once a kind against the world pose
// itself (each bone's bind place through its matrix lands on its world
// place; CT.log 2026-10-27 21:41: Faith's 182 of 182, a KrugerSec's 163 of
// 163). Its mesh's vertices (SnowBodyMesh.h: from the game's own meshes and
// skin weights, made by the snow session's scratchpad body\gen.py) are put
// where the body is now, and each of its triangles that's down in the snow
// on the ground under it is pressed into a stamp: a top-down mask, 2 cm a
// texel, each texel how far down the body was there (0 the snow's top, 1 the
// ground), the most so far. The pass draws a stamp as the snow pressed in
// that far (WetLook.hlsl's SnowStampsAt), the falling snow filling it as it
// fills her prints. What the pass is given is the mask smoothed (a Gaussian
// 3 cm across, sigma 1.5 texels: 2026-10-27, the user: "snowbody outlines
// should be smoothed out"; the triangles' edges showed in 2 cm steps, and the
// depth the body went to, frame by frame, mottled the floor).
// - Faith: her lower and upper body (LOD1) and her arms with her gloves
//   (LOD2), her default clothes (another outfit's own shape isn't known
//   here). Her legs and shoes press only while her hips are low (a slide, a
//   roll, a hard landing): her steps have prints of their own, and her
//   ankles go into deep snow at every step. Her hands where they come down (a
//   landing, a roll) press as they are.
// - KrugerSec (every one with the enemy's skeleton, Skeleton_Enemy, 163
//   bones): an Enforcer's mesh (his full body, LOD2) for every kind (a
//   Sentinel's or a Protector's armour isn't the same, a few cm). Pressed
//   only while they lie (their hips down by their feet: down, dead, a
//   ragdoll), on the ground their last step was on, else the one their body
//   rests on (its lowest point); their steps are prints (WetLook.cpp's
//   OtherSnowStep, their foot's way from their bones, OtherFootWay).
// - In the rain (the wet look on, no snow): her body while her hips are low,
//   for her slide's wake (WetLook.hlsl's SlideWake), each stamp gone
//   WaterStampSeconds after (Take's fill).
// Only what's within a band over the ground under the body counts (from 15
// cm under it to the snow's top over it, and 3 cm more: the animations'
// bodies don't sink into the ground), so a body hanging from a ledge or
// reaching over an edge presses nothing. A stamp is taken only once a texel
// is pressed (2026-10-27 21:43: walking down a slope, its rise widened the
// band looked in, so stamps were taken that nothing was pressed into, and the
// ones the snow was filling were given up for them).
namespace
{
  // The game's layout (the replay's: FaithReplay.cpp's SlotOf,
  // FindCharacterPose, FindRawSlot)
  const int CharacterBodyOffset = 0x248;
  const int FaithOtherAnimatables[] = { 0x240, 0x248, 0x340 };   // her soldier's: never one of the people
  const __int64 AnimatableTypeInfoRva = 0x27EEE10;               // ClientAntAnimatableEntity
  const int AnimatablePoseOffset = 0x158;
  const int AnimatableManagerOffset = 0x1B8;
  const int PoseHandleOffset = 0x118;
  const int ManagerPoseStrideOffset = 0x8C;
  const int ManagerMatrixStrideOffset = 0x90;
  const int ManagerHasMatricesOffset = 0x2B9;
  const int ManagerPagesOffset = 0x210;
  const int PageWorldPoseOffset = 0x08;
  const int PageMatricesOffset = 0x10;
  const int FloatsPerBone = 8;           // the world pose: translation x y z 1, rotation
  const int MatrixFloats = 12;
  // The people: PamClientCharacterEntity's instance lists (TypeInfo + 0x60,
  // four 0x10 apart, each instance linked at + 0x40; Hooks.cpp's
  // KeptPeopleTypes), an entity's place from its components (FaithReplay.cpp's
  // EntityPlace)
  const __int64 CharacterTypeInfoRva = 0x2853B00;
  const int InstanceListsOffset = 0x60;
  const int InstanceLinkOffset = 0x40;
  const int MaxInstancesPerList = 4096;
  const float PeopleReach = 40.f;        // m from the camera
  const int MostPeople = 12;
  const ULONGLONG PeopleScanMs = 500;

  // Pressing
  const float UnderGround = 0.15f;       // m: lower than this under the ground isn't on it
  const float OverTop = 0.03f;           // m over the snow's top still pressing (see above)
  const float LeastLayer = 0.03f;        // m: the snow's least depth here (a print's, WetLook.cpp's SnowPrintDepth); the water's
  const float GateOver = 0.35f;          // m: a body's lowest gate bone this near the ground over the snow is looked at
  const float FaithHipsLow = 0.55f;      // m: her hips this near the ground, her legs and shoes press too
  const float PersonHipsLow = 0.35f;     // m: a KrugerSec's hips this near their lower foot: they lie
  const int CheckTries = 300;            // a kind's bodies read that don't check before it's given up
  // A body's hips this far from where it is (Faith: her transform; a
  // KrugerSec: their entity's place, which a ragdoll thrown by a kick leaves
  // a few metres behind) aren't taken: its pose isn't where it's drawn
  // (2026-10-27, the replay session: a gun's pose slot 1.2 km from where the
  // game drew it)
  const float FaithHipsReach = 2.f;
  const float PersonHipsReach = 8.f;
  const ULONGLONG PressKeptMs = 1000;    // a stamp is its body's while it pressed within this
  const ULONGLONG StepGroundMs = 10000;  // a KrugerSec's step tells the ground under them this long
  const float StepGroundReach = 2.5f;    // m from their hips
  const float FootWayReach = 0.4f;       // m: a step's contact this near one of their feet is theirs
  // A lying KrugerSec as capsules (snowbody::LyingBodies): from bone to bone
  // of Skeleton_Enemy, each one's radius from an Enforcer's mesh
  // (enforcer_fullbody_mesh LOD0, its 21,964 vertices each taken to its
  // nearest segment in the bind pose: the radius that holds 85 % of a
  // segment's; the snow session's scratchpad body\capsules.py), a pair's left
  // and right averaged (his armour isn't the same both sides)
  struct Capsule
  {
    int A, B;
    float Radius;
  };
  const Capsule LyingCapsules[] = {
    { 44, 45, 0.152f },     // Head - HeadEnd (his helmet)
    { 4, 6, 0.217f },       // Hips - Spine1
    { 6, 42, 0.211f },      // Spine1 - Neck
    { 9, 12, 0.151f },      // LeftArm - LeftForeArm
    { 106, 109, 0.151f },   // RightArm - RightForeArm
    { 12, 15, 0.060f },     // LeftForeArm - LeftHand
    { 109, 112, 0.060f },   // RightForeArm - RightHand
    { 15, 39, 0.057f },     // LeftHand - LeftHandMiddle4
    { 112, 136, 0.057f },   // RightHand - RightHandMiddle4
    { 141, 145, 0.155f },   // LeftUpLeg - LeftLeg
    { 150, 154, 0.155f },   // RightUpLeg - RightLeg
    { 145, 146, 0.085f },   // LeftLeg - LeftFoot
    { 154, 155, 0.085f },   // RightLeg - RightFoot
    { 146, 148, 0.089f },   // LeftFoot - LeftToe
    { 155, 157, 0.089f },   // RightFoot - RightToe
  };
  const int LyingCapsuleCount = (int)(sizeof(LyingCapsules) / sizeof(LyingCapsules[0]));
  static_assert(LyingCapsuleCount <= snowbody::LyingSegments, "LyingSegments too few");
  const float KnockReach = 2.f;          // m from a KrugerSec's hips: a knock is his
  // The smoothing (see above): its weights from the middle out (a Gaussian,
  // sigma 1.5 texels, 4 each side, summing to 1)
  const int SmoothReach = 4;
  const float SmoothWeight[SmoothReach + 1] = { 0.2666f, 0.2134f, 0.1096f, 0.0361f, 0.0076f };

  enum Kind { KindFaith = 0, KindEnforcer = 1, Kinds = 2 };

  struct BodyMesh
  {
    int Bones;
    int VertexCount;
    int TriangleCount;
    snowbodymesh::Vertex const* Vertices;
    unsigned short const* Triangles;
    unsigned char const* Part;           // per vertex: 2 its feet's, 1 its legs', 0 the rest
    float const (*BindPlace)[3];
    int Gate[7];                         // the bones looked at first (see GateOver), not its legs'
    int Knees[2];                        // and its knees, while its legs press
    int Hips, LeftFoot, RightFoot, LeftToe, RightToe, Head;
    char const* Name;
  };
  const BodyMesh Meshes[Kinds] = {
    { snowbodymesh::FaithBones, snowbodymesh::FaithVertexCount, snowbodymesh::FaithTriangleCount,
      snowbodymesh::FaithVertices, snowbodymesh::FaithTriangles, snowbodymesh::FaithPart, snowbodymesh::FaithBindPlace,
      { snowbodymesh::FaithHips, snowbodymesh::FaithLeftHand, snowbodymesh::FaithRightHand, snowbodymesh::FaithHead,
        snowbodymesh::FaithSpine2, snowbodymesh::FaithLeftForeArm, snowbodymesh::FaithRightForeArm },
      { snowbodymesh::FaithLeftLeg, snowbodymesh::FaithRightLeg },
      snowbodymesh::FaithHips, snowbodymesh::FaithLeftFoot, snowbodymesh::FaithRightFoot, snowbodymesh::FaithLeftToeBase,
      snowbodymesh::FaithRightToeBase, snowbodymesh::FaithHead, "Faith's" },
    { snowbodymesh::EnforcerBones, snowbodymesh::EnforcerVertexCount, snowbodymesh::EnforcerTriangleCount,
      snowbodymesh::EnforcerVertices, snowbodymesh::EnforcerTriangles, snowbodymesh::EnforcerPart,
      snowbodymesh::EnforcerBindPlace,
      { snowbodymesh::EnforcerHips, snowbodymesh::EnforcerLeftHand, snowbodymesh::EnforcerRightHand,
        snowbodymesh::EnforcerHead, snowbodymesh::EnforcerSpine2, snowbodymesh::EnforcerLeftForeArm,
        snowbodymesh::EnforcerRightForeArm },
      { snowbodymesh::EnforcerLeftLeg, snowbodymesh::EnforcerRightLeg },
      snowbodymesh::EnforcerHips, snowbodymesh::EnforcerLeftFoot, snowbodymesh::EnforcerRightFoot,
      snowbodymesh::EnforcerLeftToeBase, snowbodymesh::EnforcerRightToeBase, snowbodymesh::EnforcerHead, "a KrugerSec's" },
  };
  const int MostBones = snowbodymesh::FaithBones > snowbodymesh::EnforcerBones ? snowbodymesh::FaithBones :
    snowbodymesh::EnforcerBones;
  const int MostVertices = snowbodymesh::FaithVertexCount > snowbodymesh::EnforcerVertexCount ?
    snowbodymesh::FaithVertexCount : snowbodymesh::EnforcerVertexCount;

  // A kind's check (see g_Checked): 0 not yet, 1 its matrices are rows, 2 its
  // matrices are columns, -1 neither (that kind presses nothing)
  int g_Checked[Kinds] = {};
  int g_CheckFails[Kinds] = {};

  // A stamp's slice (see snowbody::Stamp), under g_Mutex
  struct Slice
  {
    bool Used;
    int Mode;                 // what it's pressed into (snowbody::Mode)
    float Corner[3];
    float Along[2];
    float Rise;
    float Left;
    __int64 Owner;            // whose it is while they press it (1 Faith, else their entity)
    __int64 LastOwner;        // whose it was (a KrugerSec lying there again takes it back)
    ULONGLONG Pressed;        // when it was last pressed
    int DirtyFrom, DirtyTo;   // rows changed since it was last sent (DirtyFrom > DirtyTo: none)
    unsigned char Mask[snowbody::StampAlong][snowbody::StampAcross];     // as pressed
    unsigned char Smooth[snowbody::StampAlong][snowbody::StampAcross];   // smoothed (see SmoothWeight): what's sent
  };
  Slice g_Slices[snowbody::MostStamps];

  // Her handprints (see SnowBodies.h's MostHandStamps): her hand followed for
  // HandPressMs after a touch; each frame pressed where it's within HandBand
  // of its nearest point to the surface, while that point is within
  // HandReach of the surface either way (the game's contact and her
  // animated hand don't meet exactly: a hand a little over the surface or
  // into it still leaves its print, one a hand's breadth off doesn't)
  const ULONGLONG HandPressMs = 400;
  const float HandBand = 0.015f;
  const float HandReach = 0.06f;
  const ULONGLONG HandSameMs = 300;
  const float HandSameMetres = 0.15f;
  struct HandSlice
  {
    bool Used;
    int Side;                 // 1 her right, -1 her left
    int Generation;           // given again to another touch: its stamp numbers no longer this one's
    float Contact[3];         // the game's contact
    float Normal[3];          // the surface's facing, turned towards her hand (once framed)
    bool Framed;              // its middle and axes taken from her hand
    float Middle[3], U[3], V[3];
    ULONGLONG Touched;
    int State;                // 0 being pressed, 1 pressed, -1 nothing went in
    bool Pressed;             // something went in so far
    bool Dirty;
    unsigned char Mask[snowbody::HandStampSide][snowbody::HandStampSide];
    unsigned char Smooth[snowbody::HandStampSide][snowbody::HandStampSide];
  };
  HandSlice g_Hands[snowbody::MostHandStamps];
  int g_HandNext = 0;
  const int HandMostVertices = snowhandmesh::LeftVertexCount > snowhandmesh::RightVertexCount ?
    snowhandmesh::LeftVertexCount : snowhandmesh::RightVertexCount;
  float g_HandSkinned[HandMostVertices][3];
  Microsoft::WRL::ComPtr<ID3D11Device> g_HandDevice;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> g_HandTexture;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> g_HandView;
  bool g_HandTextureFailed = false;
  ULONGLONG g_HandLoggedTick = 0;

  // A KrugerSec followed (see FindPeople), under g_Mutex
  struct Person
  {
    __int64 Entity;
    __int64 Animatable;
    float Place[3];
    bool Read;                // their pose read this frame
    float Feet[2][3];         // left, right: their foot bones' places
    float Toes[2][3];
    float Ground;             // under them, from their last step (see StepGroundMs)
    ULONGLONG GroundTick;
    int Slice;                // the stamp they press (-1 none)
    int LastSlice;            // the one they pressed last (see Slice's LastOwner)
    bool Lying;               // lying at the last update (hips down by their feet)
    ULONGLONG DownAt;         // when they lay down this time (0: not lying)
    ULONGLONG KnockAt;        // their last ragdoll knock since (NoteKnock)
    float Hips[3];
    float Ends[LyingCapsuleCount][2][3];   // while lying: each capsule's ends (LyingCapsules)
  };
  Person g_People[MostPeople];
  int g_PeopleCount = 0;
  ULONGLONG g_PeopleScanTick = 0;

  std::mutex g_Mutex;
  int g_Mode = snowbody::ModeNone;       // what bodies press into now (Update's)
  int g_FaithSlice = -1;
  __int64 g_FaithSoldier = 0;
  std::atomic<ULONGLONG> g_FaithPressedAt{ 0 };
  // Her feet by her bones (see FaithShoes), under g_Mutex: left foot, left toe
  // base, right foot, right toe base; when last read (0 never)
  float g_FaithShoes[4][3] = {};
  ULONGLONG g_FaithShoesTick = 0;
  float g_FaithLast[3] = {};
  double g_FaithLastSeconds = 0.0;
  float g_FaithVelocity[3] = {};
  float g_FaithRise = 0.f;
  ULONGLONG g_PressLoggedTick[Kinds] = {};
  ULONGLONG g_WaterLoggedTick = 0;
  bool g_TypeChecked = false, g_TypeOk = false;

  // The pass's texture (render thread)
  Microsoft::WRL::ComPtr<ID3D11Device> g_Device;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> g_Texture;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> g_View;
  bool g_TextureFailed = false;

  double SecondsNow()
  {
    static LARGE_INTEGER frequency = {};
    if (!frequency.QuadPart)
      QueryPerformanceFrequency(&frequency);
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return (double)now.QuadPart / (double)frequency.QuadPart;
  }

  // What an object's getType gives (vtable [0]: lea rax, [rip + x]; ret).
  // Game memory: only inside the callers' __try.
  __int64 TypeOf(__int64 object)
  {
    __int64 vtable = *(__int64 const*)object;
    BYTE const* code = vtable ? *(BYTE const* const*)vtable : nullptr;
    if (!code || code[0] != 0x48 || code[1] != 0x8D || code[2] != 0x05)
      return 0;
    return (__int64)code + 7 + *(int const*)(code + 3);
  }

  bool TypeNamed(__int64 typeInfo, char const* name)
  {
    __try
    {
      char const* s = *(char const* const*)*(__int64 const*)typeInfo;
      return s && strcmp(s, name) == 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
  }

  // A body's bones this frame (an animatable's pose slot): its skinning
  // matrices and its world pose's places, bones of each (false: not
  // readable, not that many bones, or no matrices)
  bool ReadBody(__int64 animatable, int bones, float* matrices, float* places)
  {
    __int64 base = (__int64)g_gameHandle;
    __try
    {
      if (animatable < 0x10000 || TypeOf(animatable) != base + AnimatableTypeInfoRva)
        return false;
      __int64 pose = *(__int64 const*)(animatable + AnimatablePoseOffset);
      if (pose < 0x10000)
        return false;
      __int64 manager = *(__int64 const*)(pose + AnimatableManagerOffset);
      if (manager < 0x10000 || !*(BYTE const*)(manager + ManagerHasMatricesOffset))
        return false;
      unsigned int handle = *(unsigned int const*)(pose + PoseHandleOffset);
      int stride = *(int const*)(manager + ManagerPoseStrideOffset);
      int matrixStride = *(int const*)(manager + ManagerMatrixStrideOffset);
      if (handle == 0xFFFFFFFF || stride != bones * FloatsPerBone * (int)sizeof(float) ||
          matrixStride < bones * MatrixFloats * (int)sizeof(float))
        return false;
      __int64 const* pagesBegin = *(__int64 const* const*)(manager + ManagerPagesOffset);
      __int64 const* pagesEnd = *(__int64 const* const*)(manager + ManagerPagesOffset + 8);
      unsigned int page = handle >> 24, index = handle & 0xFF;
      if (!pagesBegin || pagesEnd < pagesBegin || page >= (unsigned int)(pagesEnd - pagesBegin))
        return false;
      __int64 pPage = pagesBegin[page];
      if (pPage < 0x10000)
        return false;
      __int64 world = *(__int64 const*)(pPage + PageWorldPoseOffset);
      __int64 m = *(__int64 const*)(pPage + PageMatricesOffset);
      if (world < 0x10000 || m < 0x10000)
        return false;
      memcpy(matrices, (void const*)(m + (__int64)index * matrixStride), (size_t)bones * MatrixFloats * sizeof(float));
      float const* w = (float const*)(world + (__int64)index * stride);
      for (int i = 0; i < bones; ++i)
        memcpy(places + i * 3, w + i * FloatsPerBone, 3 * sizeof(float));
      return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
  }

  // How many bones a body's pose has (0: not readable)
  int BodyBones(__int64 animatable)
  {
    __int64 base = (__int64)g_gameHandle;
    __try
    {
      if (animatable < 0x10000 || TypeOf(animatable) != base + AnimatableTypeInfoRva)
        return 0;
      __int64 pose = *(__int64 const*)(animatable + AnimatablePoseOffset);
      if (pose < 0x10000)
        return 0;
      __int64 manager = *(__int64 const*)(pose + AnimatableManagerOffset);
      if (manager < 0x10000)
        return 0;
      int stride = *(int const*)(manager + ManagerPoseStrideOffset);
      return stride > 0 && stride % (FloatsPerBone * (int)sizeof(float)) == 0 ? stride / (FloatsPerBone * (int)sizeof(float)) : 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return 0;
    }
  }

  __int64 ReadPointer(__int64 at)
  {
    __try
    {
      return *(__int64 const*)at;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return 0;
    }
  }

  // A point through a bone's matrix (rows: 3x4, the translation in the
  // fourth column; columns: 4x3, its fourth row)
  void Through(float const* m, bool rows, float const* p, float* out)
  {
    if (rows)
    {
      out[0] = m[0] * p[0] + m[1] * p[1] + m[2] * p[2] + m[3];
      out[1] = m[4] * p[0] + m[5] * p[1] + m[6] * p[2] + m[7];
      out[2] = m[8] * p[0] + m[9] * p[1] + m[10] * p[2] + m[11];
    }
    else
    {
      out[0] = m[0] * p[0] + m[3] * p[1] + m[6] * p[2] + m[9];
      out[1] = m[1] * p[0] + m[4] * p[1] + m[7] * p[2] + m[10];
      out[2] = m[2] * p[0] + m[5] * p[1] + m[8] * p[2] + m[11];
    }
  }

  // A kind's check (see g_Checked): how many bones' bind places land within
  // 1 cm of their world places through their matrices, either way round
  void Check(int kind, float const* matrices, float const* places)
  {
    BodyMesh const& mesh = Meshes[kind];
    int good[2] = {};
    for (int i = 0; i < mesh.Bones; ++i)
      for (int way = 0; way < 2; ++way)
      {
        float at[3];
        Through(matrices + i * MatrixFloats, way == 0, mesh.BindPlace[i], at);
        float dx = at[0] - places[i * 3], dy = at[1] - places[i * 3 + 1], dz = at[2] - places[i * 3 + 2];
        good[way] += dx * dx + dy * dy + dz * dz < 0.01f * 0.01f;
      }
    int best = good[0] >= good[1] ? 0 : 1;
    if (good[best] * 10 >= mesh.Bones * 9)
    {
      g_Checked[kind] = best == 0 ? 1 : 2;
      util::log::Write("Snow: %s body read for the snow's body prints (%d bones): its skinning matrices (%s) put %d of its "
        "bones' bind places at their world places (within 1 cm)", mesh.Name, mesh.Bones, best == 0 ? "rows" : "columns",
        good[best]);
    }
    else if (++g_CheckFails[kind] >= CheckTries)
    {
      // (that many bodies read that didn't check: not the layout looked for)
      g_Checked[kind] = -1;
      util::log::Warning("Snow: %s body's skinning matrices don't put its bones where its world pose has them (%d of %d as "
        "rows, %d as columns, within 1 cm): no body prints of theirs in the snow", mesh.Name, good[0], mesh.Bones, good[1]);
    }
  }

  // A vertex where its bones are now (its 4 bones' matrices, weighted)
  void Skin(snowbodymesh::Vertex const& v, float const* matrices, bool rows, float* out)
  {
    float p[3] = { v.Mm[0] * 0.001f, v.Mm[1] * 0.001f, v.Mm[2] * 0.001f };
    out[0] = out[1] = out[2] = 0.f;
    for (int k = 0; k < 4; ++k)
    {
      if (!v.Weight[k])
        continue;
      float at[3];
      Through(matrices + v.Bone[k] * MatrixFloats, rows, p, at);
      float w = v.Weight[k] * (1.f / 255.f);
      out[0] += at[0] * w;
      out[1] += at[1] * w;
      out[2] += at[2] * w;
    }
  }

  // A slice for a body (g_Mutex held): a free one, else the one the snow has
  // filled most that nobody presses now; cleared, sent whole
  int TakeSlice(__int64 owner, float const* hips, float const* along, float hipsAlong, float ground, float rise,
    ULONGLONG now)
  {
    int best = -1;
    for (int i = 0; i < snowbody::MostStamps; ++i)
    {
      Slice const& s = g_Slices[i];
      if (!s.Used)
      {
        best = i;
        break;
      }
      if (s.Owner && now - s.Pressed < PressKeptMs)
        continue;
      if (best < 0 || s.Left < g_Slices[best].Left)
        best = i;
    }
    if (best < 0)
      return -1;
    Slice& s = g_Slices[best];
    s.Used = true;
    s.Mode = g_Mode;
    s.Along[0] = along[0];
    s.Along[1] = along[1];
    float across[2] = { -along[1], along[0] };
    float half = snowbody::StampAcross * snowbody::StampTexel * 0.5f;
    s.Corner[0] = hips[0] - along[0] * hipsAlong - across[0] * half;
    s.Corner[2] = hips[2] - along[1] * hipsAlong - across[1] * half;
    s.Corner[1] = ground - rise * hipsAlong;
    s.Rise = rise;
    s.Left = 1.f;
    s.Owner = owner;
    s.LastOwner = owner;
    s.Pressed = now;
    memset(s.Mask, 0, sizeof(s.Mask));
    memset(s.Smooth, 0, sizeof(s.Smooth));
    s.DirtyFrom = 0;
    s.DirtyTo = snowbody::StampAlong - 1;
    return best;
  }

  // A point's place in a slice's texels (across, along): false if it's off it
  bool InSlice(Slice const& s, float const* p, float* t)
  {
    float dx = p[0] - s.Corner[0], dz = p[2] - s.Corner[2];
    float along = dx * s.Along[0] + dz * s.Along[1];
    float across = -dx * s.Along[1] + dz * s.Along[0];
    t[0] = across / snowbody::StampTexel;
    t[1] = along / snowbody::StampTexel;
    return t[0] >= 0.f && t[0] < (float)snowbody::StampAcross && t[1] >= 0.f && t[1] < (float)snowbody::StampAlong;
  }

  // A triangle (world corners, their texels in the slice) pressed into it:
  // each texel it covers takes how far down it is there (see above), the
  // most so far. The ground: the slice's corner's (s.Corner[1]), rising
  // s.Rise a metre along. Gives whether any texel was pressed.
  bool PressTriangle(Slice& s, float const* a, float const* b, float const* c, float const* ta, float const* tb,
    float const* tc, float layer)
  {
    float area = (tb[0] - ta[0]) * (tc[1] - ta[1]) - (tb[1] - ta[1]) * (tc[0] - ta[0]);
    if (fabsf(area) < 1e-6f)
      return false;
    int i0 = (std::max)(0, (int)floorf((std::min)((std::min)(ta[0], tb[0]), tc[0])));
    int i1 = (std::min)(snowbody::StampAcross - 1, (int)floorf((std::max)((std::max)(ta[0], tb[0]), tc[0])));
    int j0 = (std::max)(0, (int)floorf((std::min)((std::min)(ta[1], tb[1]), tc[1])));
    int j1 = (std::min)(snowbody::StampAlong - 1, (int)floorf((std::max)((std::max)(ta[1], tb[1]), tc[1])));
    bool pressed = false, touched = false;
    for (int j = j0; j <= j1; ++j)
    {
      float y = j + 0.5f;
      float ground = s.Corner[1] + s.Rise * y * snowbody::StampTexel;
      for (int i = i0; i <= i1; ++i)
      {
        float x = i + 0.5f;
        float w0 = ((tb[0] - x) * (tc[1] - y) - (tb[1] - y) * (tc[0] - x)) / area;
        float w1 = ((tc[0] - x) * (ta[1] - y) - (tc[1] - y) * (ta[0] - x)) / area;
        float w2 = 1.f - w0 - w1;
        if (w0 < 0.f || w1 < 0.f || w2 < 0.f)
          continue;
        float h = a[1] * w0 + b[1] * w1 + c[1] * w2;
        if (h < ground - UnderGround)
          continue;
        float down = (ground + layer + OverTop - h) / (layer + OverTop);
        if (down <= 0.f)
          continue;
        pressed = true;
        unsigned char v = (unsigned char)((std::min)(down, 1.f) * 255.f + 0.5f);
        if (v > s.Mask[j][i])
        {
          s.Mask[j][i] = v;
          touched = true;
        }
      }
    }
    if (touched)
    {
      s.DirtyFrom = (std::min)(s.DirtyFrom, j0);
      s.DirtyTo = (std::max)(s.DirtyTo, j1);
    }
    return pressed;
  }

  float g_Skinned[MostVertices][3];

  // A body's frame (g_Mutex held): its mesh where its bones are, each
  // triangle down in the snow pressed into its stamp (taken once a texel is
  // pressed; a new one when the body's off the one it presses). ground: under
  // it (groundKnown false: given the lowest point of its mesh, a body lying
  // rests on the ground); mostPart: its parts that press (see BodyMesh's
  // Part: 0 not its legs nor feet, 2 all of it); along: the way a new stamp
  // runs, hipsAlong: its hips that far along it; rise: the ground's along it.
  // Gives whether anything was pressed, and its lowest point (m over the
  // ground; its pressing parts').
  bool PressBody(int kind, float const* matrices, float const* hips, float& ground, bool groundKnown, float layer,
    int mostPart, __int64 owner, int& slice, float const* along, float hipsAlong, float rise, ULONGLONG now,
    float& lowest)
  {
    BodyMesh const& mesh = Meshes[kind];
    bool rows = g_Checked[kind] == 1;
    for (int i = 0; i < mesh.VertexCount; ++i)
      Skin(mesh.Vertices[i], matrices, rows, g_Skinned[i]);
    if (!groundKnown)
    {
      ground = 1e9f;
      for (int i = 0; i < mesh.VertexCount; ++i)
        ground = (std::min)(ground, g_Skinned[i][1]);
    }
    lowest = 1e9f;
    for (int i = 0; i < mesh.VertexCount; ++i)
      if (mesh.Part[i] <= mostPart)
        lowest = (std::min)(lowest, g_Skinned[i][1] - ground);
    bool any = false, here = false;   // (here: into the stamp of this pass)
    int took = -1;
    for (int pass = 0; pass < 2; ++pass)
    {
      bool off = false;
      here = false;
      for (int k = 0; k < mesh.TriangleCount; ++k)
      {
        unsigned short ia = mesh.Triangles[k * 3], ib = mesh.Triangles[k * 3 + 1], ic = mesh.Triangles[k * 3 + 2];
        if (mesh.Part[ia] > mostPart || mesh.Part[ib] > mostPart || mesh.Part[ic] > mostPart)
          continue;
        float const* a = g_Skinned[ia];
        float const* b = g_Skinned[ib];
        float const* c = g_Skinned[ic];
        // (only one that reaches into the band over the ground under its
        // corners: the stamp's ground, or the one it would have, rising
        // along it)
        float const* way = slice >= 0 ? g_Slices[slice].Along : along;
        float low = 1e9f, high = -1e9f;
        float const* corners[3] = { a, b, c };
        for (float const* p : corners)
        {
          float rising = ((p[0] - hips[0]) * way[0] + (p[2] - hips[2]) * way[1]) * rise;
          float h = p[1] - (ground + rising);
          low = (std::min)(low, h);
          high = (std::max)(high, h);
        }
        if (low >= layer + OverTop || high < -UnderGround)
          continue;
        if (slice < 0)
        {
          slice = TakeSlice(owner, hips, along, hipsAlong, ground, rise, now);
          if (slice < 0)
            return false;
          took = slice;
        }
        Slice& s = g_Slices[slice];
        float ta[2], tb[2], tc[2];
        bool inA = InSlice(s, a, ta), inB = InSlice(s, b, tb), inC = InSlice(s, c, tc);
        if (!inA || !inB || !inC)
        {
          off = true;
          continue;
        }
        here |= PressTriangle(s, a, b, c, ta, tb, tc, layer);
      }
      any |= here;
      if (!off || pass == 1)
        break;
      // Off its stamp (she's slid on): a new one from here, pressed again
      // (the one just taken given back if nothing went into it)
      if (slice >= 0 && here)
        g_Slices[slice].Pressed = now;
      else if (slice >= 0 && slice == took)
      {
        g_Slices[took].Used = false;
        g_Slices[took].Owner = 0;
      }
      took = -1;
      slice = -1;
    }
    // (a stamp taken that nothing went into goes back)
    if (!here && took >= 0 && slice == took)
    {
      g_Slices[took].Used = false;
      g_Slices[took].Owner = 0;
      slice = -1;
    }
    if (here && slice >= 0)
      g_Slices[slice].Pressed = now;
    return any;
  }

  // A hand stamp's frame (g_Mutex held): her hand now, its middle and the way
  // its fingers point, laid in the surface's plane, the plane's facing
  // turned towards it
  void FrameHand(HandSlice& h, float const* places, int count)
  {
    float centre[3] = {};
    for (int i = 0; i < count; ++i)
      for (int k = 0; k < 3; ++k)
        centre[k] += g_HandSkinned[i][k] / count;
    float* n = h.Normal;
    float toHand = (centre[0] - h.Contact[0]) * n[0] + (centre[1] - h.Contact[1]) * n[1] + (centre[2] - h.Contact[2]) * n[2];
    if (toHand < 0.f)
      for (int k = 0; k < 3; ++k)
        n[k] = -n[k];
    toHand = fabsf(toHand);
    for (int k = 0; k < 3; ++k)
      h.Middle[k] = centre[k] - n[k] * toHand;
    int hand = h.Side > 0 ? snowhandmesh::RightHand : snowhandmesh::LeftHand;
    int middle = h.Side > 0 ? snowhandmesh::RightMiddle1 : snowhandmesh::LeftMiddle1;
    float f[3];
    for (int k = 0; k < 3; ++k)
      f[k] = places[middle * 3 + k] - places[hand * 3 + k];
    float along = f[0] * n[0] + f[1] * n[1] + f[2] * n[2];
    for (int k = 0; k < 3; ++k)
      f[k] -= n[k] * along;
    float l = sqrtf(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
    if (l < 1e-3f)
    {
      // (her fingers straight into the surface: any way across it)
      float other[3] = { fabsf(n[1]) < 0.9f ? 0.f : 1.f, fabsf(n[1]) < 0.9f ? 1.f : 0.f, 0.f };
      f[0] = other[1] * n[2] - other[2] * n[1];
      f[1] = other[2] * n[0] - other[0] * n[2];
      f[2] = other[0] * n[1] - other[1] * n[0];
      l = sqrtf(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
    }
    for (int k = 0; k < 3; ++k)
      h.U[k] = f[k] / l;
    // (V = facing x U, so U x V is the facing)
    h.V[0] = n[1] * h.U[2] - n[2] * h.U[1];
    h.V[1] = n[2] * h.U[0] - n[0] * h.U[2];
    h.V[2] = n[0] * h.U[1] - n[1] * h.U[0];
    h.Framed = true;
  }

  // Her hand pressed into its stamp this frame (g_Mutex held; matrices:
  // her body's, checked; places: its bones' world places)
  void PressHand(HandSlice& h, float const* matrices, float const* places)
  {
    bool rows = g_Checked[KindFaith] == 1;
    bool right = h.Side > 0;
    int count = right ? snowhandmesh::RightVertexCount : snowhandmesh::LeftVertexCount;
    int triangles = right ? snowhandmesh::RightTriangleCount : snowhandmesh::LeftTriangleCount;
    snowhandmesh::Vertex const* vertices = right ? snowhandmesh::RightVertices : snowhandmesh::LeftVertices;
    unsigned short const* index = right ? snowhandmesh::RightTriangles : snowhandmesh::LeftTriangles;
    for (int i = 0; i < count; ++i)
    {
      snowhandmesh::Vertex const& v = vertices[i];
      float p[3] = { v.Tenth[0] * 1e-4f, v.Tenth[1] * 1e-4f, v.Tenth[2] * 1e-4f };
      float* out = g_HandSkinned[i];
      out[0] = out[1] = out[2] = 0.f;
      for (int k = 0; k < 4; ++k)
      {
        if (!v.Weight[k])
          continue;
        float at[3];
        Through(matrices + v.Bone[k] * MatrixFloats, rows, p, at);
        float w = v.Weight[k] * (1.f / 255.f);
        out[0] += at[0] * w;
        out[1] += at[1] * w;
        out[2] += at[2] * w;
      }
    }
    if (!h.Framed)
      FrameHand(h, places, count);
    // How far each corner is off the surface (towards her hand), its nearest
    float nearest = 1e9f;
    static float off[HandMostVertices];   // game thread
    for (int i = 0; i < count; ++i)
    {
      float const* p = g_HandSkinned[i];
      off[i] = (p[0] - h.Contact[0]) * h.Normal[0] + (p[1] - h.Contact[1]) * h.Normal[1] + (p[2] - h.Contact[2]) * h.Normal[2];
      nearest = (std::min)(nearest, off[i]);
    }
    if (nearest > HandReach || nearest < -HandReach)
      return;
    float top = nearest + HandBand;
    const float half = snowbody::HandStampSide * 0.5f;
    for (int k = 0; k < triangles; ++k)
    {
      int ia = index[k * 3], ib = index[k * 3 + 1], ic = index[k * 3 + 2];
      if ((std::min)((std::min)(off[ia], off[ib]), off[ic]) >= top)
        continue;
      float t[3][2];
      int corner[3] = { ia, ib, ic };
      for (int c = 0; c < 3; ++c)
      {
        float const* p = g_HandSkinned[corner[c]];
        float d[3] = { p[0] - h.Middle[0], p[1] - h.Middle[1], p[2] - h.Middle[2] };
        t[c][0] = (d[0] * h.U[0] + d[1] * h.U[1] + d[2] * h.U[2]) / snowbody::HandStampTexel + half;
        t[c][1] = (d[0] * h.V[0] + d[1] * h.V[1] + d[2] * h.V[2]) / snowbody::HandStampTexel + half;
      }
      float area = (t[1][0] - t[0][0]) * (t[2][1] - t[0][1]) - (t[1][1] - t[0][1]) * (t[2][0] - t[0][0]);
      if (fabsf(area) < 1e-6f)
        continue;
      int i0 = (std::max)(0, (int)floorf((std::min)((std::min)(t[0][0], t[1][0]), t[2][0])));
      int i1 = (std::min)(snowbody::HandStampSide - 1, (int)floorf((std::max)((std::max)(t[0][0], t[1][0]), t[2][0])));
      int j0 = (std::max)(0, (int)floorf((std::min)((std::min)(t[0][1], t[1][1]), t[2][1])));
      int j1 = (std::min)(snowbody::HandStampSide - 1, (int)floorf((std::max)((std::max)(t[0][1], t[1][1]), t[2][1])));
      for (int j = j0; j <= j1; ++j)
        for (int i = i0; i <= i1; ++i)
        {
          float x = i + 0.5f, y = j + 0.5f;
          float w0 = ((t[1][0] - x) * (t[2][1] - y) - (t[1][1] - y) * (t[2][0] - x)) / area;
          float w1 = ((t[2][0] - x) * (t[0][1] - y) - (t[2][1] - y) * (t[0][0] - x)) / area;
          float w2 = 1.f - w0 - w1;
          if (w0 < 0.f || w1 < 0.f || w2 < 0.f)
            continue;
          float d = off[ia] * w0 + off[ib] * w1 + off[ic] * w2;
          float pressed = (top - d) / HandBand;
          if (pressed <= 0.f)
            continue;
          unsigned char v = (unsigned char)((std::min)(pressed, 1.f) * 255.f + 0.5f);
          if (v > h.Mask[j][i])
          {
            h.Mask[j][i] = v;
            h.Dirty = true;
            h.Pressed = true;
          }
        }
    }
  }

  // The characters round the camera with the enemy's skeleton (see
  // CharacterTypeInfoRva), not Faith: their entities and bodies, how many
  struct Found
  {
    __int64 Entity, Animatable;
    float Place[3];
    float Distance;
  };

  void WalkPeople(__int64 head, __int64 typeInfo, float const* camera, __int64 soldier, Found* out, int most, int& count)
  {
    __try
    {
      __int64 faith[3] = {};
      if (soldier > 0x10000)
        for (int k = 0; k < 3; ++k)
          faith[k] = *(__int64 const*)(soldier + FaithOtherAnimatables[k]);
      __int64 link = *(__int64 const*)head;
      for (int n = 0; link >= 0x10000 && n < MaxInstancesPerList && count < most; ++n, link = *(__int64 const*)link)
      {
        __int64 entity = link - InstanceLinkOffset;
        if (TypeOf(entity) != typeInfo)
          continue;
        BYTE const* components = *(BYTE const* const*)(entity + 0x38);
        if ((__int64)components < 0x10000)
          continue;
        float place[3];
        memcpy(place, components + 0x40 + ((__int64)components[9] + 2 * components[0xA]) * 0x20, sizeof(place));
        float dx = place[0] - camera[0], dy = place[1] - camera[1], dz = place[2] - camera[2];
        float distance = sqrtf(dx * dx + dy * dy + dz * dz);
        if (!(distance <= PeopleReach))
          continue;
        __int64 animatable = *(__int64 const*)(entity + CharacterBodyOffset);
        if (animatable < 0x10000 || animatable == faith[0] || animatable == faith[1] || animatable == faith[2])
          continue;
        Found& f = out[count++];
        f.Entity = entity;
        f.Animatable = animatable;
        memcpy(f.Place, place, sizeof(place));
        f.Distance = distance;
      }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
  }

  // The nearest MostPeople of them with the enemy's skeleton
  int FindPeople(float const* camera, __int64 soldier, Found* out)
  {
    __int64 typeInfo = (__int64)g_gameHandle + CharacterTypeInfoRva;
    const int MostFound = 64;
    Found found[MostFound];
    int count = 0;
    for (int k = 0; k < 4; ++k)
      WalkPeople(typeInfo + InstanceListsOffset + k * 0x10, typeInfo, camera, soldier, found, MostFound, count);
    int kept = 0;
    for (int i = 0; i < count; ++i)
      if (BodyBones(found[i].Animatable) == snowbodymesh::EnforcerBones)
        found[kept++] = found[i];
    std::sort(found, found + kept, [](Found const& a, Found const& b) { return a.Distance < b.Distance; });
    kept = (std::min)(kept, MostPeople);
    memcpy(out, found, kept * sizeof(Found));
    return kept;
  }

  // A slice's changed rows smoothed (see SmoothWeight; g_Mutex held): rows
  // from - to of Mask, and SmoothReach round them, into Smooth. Gives the
  // rows of Smooth that changed.
  void SmoothRows(Slice& s, int from, int to, int& smoothFrom, int& smoothTo)
  {
    smoothFrom = (std::max)(0, from - SmoothReach);
    smoothTo = (std::min)(snowbody::StampAlong - 1, to + SmoothReach);
    int rowFrom = (std::max)(0, smoothFrom - SmoothReach), rowTo = (std::min)(snowbody::StampAlong - 1, smoothTo + SmoothReach);
    // Across first (rowFrom - rowTo), then along
    static float across[snowbody::StampAlong][snowbody::StampAcross];   // render thread
    for (int j = rowFrom; j <= rowTo; ++j)
      for (int i = 0; i < snowbody::StampAcross; ++i)
      {
        float sum = s.Mask[j][i] * SmoothWeight[0];
        for (int k = 1; k <= SmoothReach; ++k)
        {
          if (i - k >= 0)
            sum += s.Mask[j][i - k] * SmoothWeight[k];
          if (i + k < snowbody::StampAcross)
            sum += s.Mask[j][i + k] * SmoothWeight[k];
        }
        across[j][i] = sum;
      }
    for (int j = smoothFrom; j <= smoothTo; ++j)
      for (int i = 0; i < snowbody::StampAcross; ++i)
      {
        float sum = across[j][i] * SmoothWeight[0];
        for (int k = 1; k <= SmoothReach; ++k)
        {
          if (j - k >= rowFrom)
            sum += across[j - k][i] * SmoothWeight[k];
          if (j + k <= rowTo)
            sum += across[j + k][i] * SmoothWeight[k];
        }
        s.Smooth[j][i] = (unsigned char)(std::min)(sum + 0.5f, 255.f);
      }
  }

  float g_Matrices[MostBones * MatrixFloats];
  float g_Places[MostBones * 3];
}

void snowbody::Update(__int64 soldier, float const* faith, float const* camera)
{
  ULONGLONG now = GetTickCount64();
  // What bodies press into: the snow on the ground, or the rain's water while
  // the wet look is on (wetlook::Reach before g_Mutex: its lock is never taken
  // under it); nothing read while neither
  weather::SnowFall snow = weather::GetSnow();
  int mode = snow.Chosen ? (snow.Cover > 0.001f ? ModeSnow : ModeNone) : wetlook::Reach() > 0.f ? ModeWater : ModeNone;
  if (mode == ModeNone)
  {
    std::lock_guard<std::mutex> lock(g_Mutex);
    g_Mode = ModeNone;
    g_PeopleCount = 0;
    g_PeopleScanTick = 0;
    g_FaithSlice = -1;
    g_FaithLastSeconds = 0.0;
    return;
  }
  bool water = mode == ModeWater;
  if (!g_TypeChecked)
  {
    g_TypeChecked = true;
    g_TypeOk = TypeNamed((__int64)g_gameHandle + CharacterTypeInfoRva, "PamClientCharacterEntity");
    if (!g_TypeOk)
      util::log::Warning("Snow: PamClientCharacterEntity's type isn't where it was looked for: no KrugerSec body prints");
  }
  // Where the snow lies under Faith and how deep (before g_Mutex: wetlook's
  // own lock is never taken under it); in the water, anywhere (her slide's
  // wake is drawn only where the water is, SlideWake)
  float faithLayer = 0.f;
  bool faithHere = faith && (water || wetlook::SnowLayerAt(faith + 12, faithLayer));
  // The people (in the snow and in the rain: their bodies lying in the water
  // too, LyingBodies), found again every PeopleScanMs
  Found found[MostPeople];
  int foundCount = -1;
  if (g_TypeOk && camera && (!g_PeopleScanTick || now - g_PeopleScanTick >= PeopleScanMs))
  {
    g_PeopleScanTick = now;
    foundCount = FindPeople(camera, soldier, found);
  }
  float peopleLayer[MostPeople] = {};
  bool peopleSnow[MostPeople] = {};
  {
    Person places[MostPeople];
    int count;
    {
      std::lock_guard<std::mutex> lock(g_Mutex);
      g_Mode = mode;
      if (foundCount >= 0)
      {
        // (the ones followed already keep what's known of them)
        Person next[MostPeople];
        for (int i = 0; i < foundCount; ++i)
        {
          Person p{};
          p.Entity = found[i].Entity;
          p.Animatable = found[i].Animatable;
          memcpy(p.Place, found[i].Place, sizeof(p.Place));
          p.Slice = -1;
          p.LastSlice = -1;
          for (int j = 0; j < g_PeopleCount; ++j)
            if (g_People[j].Entity == p.Entity && g_People[j].Animatable == p.Animatable)
            {
              p = g_People[j];
              memcpy(p.Place, found[i].Place, sizeof(p.Place));
            }
          next[i] = p;
        }
        memcpy(g_People, next, foundCount * sizeof(Person));
        g_PeopleCount = foundCount;
      }
      count = g_PeopleCount;
      memcpy(places, g_People, count * sizeof(Person));
    }
    // (in the rain, anywhere: their wake is drawn only where the water is)
    for (int i = 0; i < count; ++i)
      peopleSnow[i] = water || wetlook::SnowLayerAt(places[i].Place, peopleLayer[i]);
  }

  std::lock_guard<std::mutex> lock(g_Mutex);
  // Faith
  if (soldier != g_FaithSoldier)
  {
    g_FaithSoldier = soldier;
    g_FaithSlice = -1;
  }
  __int64 body = soldier > 0x10000 ? ReadPointer(soldier + CharacterBodyOffset) : 0;
  BodyMesh const& her = Meshes[KindFaith];
  bool faithRead = faith && body && g_Checked[KindFaith] >= 0 && ReadBody(body, her.Bones, g_Matrices, g_Places);
  if (faithRead && !g_Checked[KindFaith])
    Check(KindFaith, g_Matrices, g_Places);
  bool faithOk = faithRead && g_Checked[KindFaith] > 0;
  if (faithOk)
  {
    float const* hips = g_Places + her.Hips * 3;
    float dx = hips[0] - faith[12], dy = hips[1] - faith[13], dz = hips[2] - faith[14];
    faithOk = dx * dx + dy * dy + dz * dz < FaithHipsReach * FaithHipsReach;
  }
  // Her feet by her bones, for her shoes kept as drawn under the snow
  // (FaithShoes; 2026-11-01, the user: "Faiths shoes turn blue on snow fix
  // it": they were kept within 0.43 m of her transform's place only, so
  // with her legs out ahead, sliding or sitting, the snow lay on them)
  if (faithOk)
  {
    int const shoeBones[4] = { her.LeftFoot, her.LeftToe, her.RightFoot, her.RightToe };
    for (int k = 0; k < 4; ++k)
      memcpy(g_FaithShoes[k], g_Places + shoeBones[k] * 3, sizeof(g_FaithShoes[k]));
    g_FaithShoesTick = now;
  }
  if (faith)
  {
    // Her speed (from her transform's place frame to frame) and the ground's
    // rise the way she goes while she's on it (not falling)
    double seconds = SecondsNow();
    double dt = seconds - g_FaithLastSeconds;
    if (g_FaithLastSeconds > 0.0 && dt > 0.001 && dt < 0.2)
    {
      for (int k = 0; k < 3; ++k)
        g_FaithVelocity[k] = g_FaithVelocity[k] * 0.7f + (float)((faith[12 + k] - g_FaithLast[k]) / dt) * 0.3f;
      float along = sqrtf(g_FaithVelocity[0] * g_FaithVelocity[0] + g_FaithVelocity[2] * g_FaithVelocity[2]);
      float rise = along > 1.f && fabsf(g_FaithVelocity[1]) < 0.8f * along ? g_FaithVelocity[1] / along : 0.f;
      g_FaithRise = g_FaithRise * 0.9f + rise * 0.1f;
    }
    memcpy(g_FaithLast, faith + 12, sizeof(g_FaithLast));
    g_FaithLastSeconds = seconds;
  }
  bool faithPressing = false;
  if (faithOk && faithHere)
  {
    float ground = faith[13];
    float const* hips = g_Places + her.Hips * 3;
    bool low = hips[1] - ground < FaithHipsLow;
    // (looked at only while a part that may press is near the ground: not
    // her knees while her legs don't press; in the water only while her hips
    // are low, her slides)
    float lowestGate = 1e9f;
    for (int b : her.Gate)
      lowestGate = (std::min)(lowestGate, g_Places[b * 3 + 1] - ground);
    if (low)
      for (int b : her.Knees)
        lowestGate = (std::min)(lowestGate, g_Places[b * 3 + 1] - ground);
    float layer = water ? LeastLayer : (std::max)(faithLayer, LeastLayer);
    if (lowestGate < layer + GateOver && (!water || low))
    {
      // (a new stamp runs the way she's going, her hips near its start; the
      // way she faces, her hips at its middle, when she isn't moving)
      float speed = sqrtf(g_FaithVelocity[0] * g_FaithVelocity[0] + g_FaithVelocity[2] * g_FaithVelocity[2]);
      float along[2] = { faith[8], faith[10] };
      float hipsAlong = StampAlong * StampTexel * 0.5f;
      if (speed > 1.f)
      {
        along[0] = g_FaithVelocity[0] / speed;
        along[1] = g_FaithVelocity[2] / speed;
        hipsAlong = 1.2f;
      }
      float l = sqrtf(along[0] * along[0] + along[1] * along[1]);
      if (l > 1e-3f)
      {
        along[0] /= l;
        along[1] /= l;
        // (the ground under her rises along the stamp's way as it does the
        // way she goes: g_FaithRise is along her motion)
        float rise = speed > 1.f ? g_FaithRise : 0.f;
        float lowest;
        faithPressing = PressBody(KindFaith, g_Matrices, hips, ground, true, layer, low ? 2 : 0, 1, g_FaithSlice, along,
          hipsAlong, rise, now, lowest);
        if (faithPressing)
          g_FaithPressedAt = now;
        ULONGLONG& logged = water ? g_WaterLoggedTick : g_PressLoggedTick[KindFaith];
        if (faithPressing && (!logged || now - logged > 30000))
        {
          logged = now;
          if (water)
            util::log::Write("Snow: Faith's body slides through the water at %.2f %.2f %.2f (stamp %d; her hips %.2f m over "
              "the ground, her lowest %.2f m): her wake takes its shape", hips[0], ground, hips[2], g_FaithSlice,
              hips[1] - ground, lowest);
          else
            util::log::Write("Snow: Faith's body presses the snow at %.2f %.2f %.2f (stamp %d; her hips %.2f m over the "
              "ground, her lowest %.2f m, %s; the snow %.0f cm deep there)", hips[0], ground, hips[2], g_FaithSlice,
              hips[1] - ground, lowest, low ? "all of her" : "not her legs nor shoes", 100.f * faithLayer);
        }
      }
    }
  }
  if (!faithPressing && g_FaithSlice >= 0 && now - g_Slices[g_FaithSlice].Pressed > PressKeptMs)
  {
    g_Slices[g_FaithSlice].Owner = 0;
    g_FaithSlice = -1;
  }

  // Her handprints (see HandPressMs): her hand pressed into each touch's
  // stamp while it's followed (her body read and checked this frame), then
  // it's done: pressed, or nothing went in (HandShape's drawn instead)
  for (HandSlice& h : g_Hands)
  {
    if (!h.Used || h.State != 0)
      continue;
    if (now - h.Touched > HandPressMs)
    {
      h.State = h.Pressed ? 1 : -1;
      if (!g_HandLoggedTick || now - g_HandLoggedTick > 30000)
      {
        g_HandLoggedTick = now;
        util::log::Write("Snow: Faith's %s hand at %.2f %.2f %.2f (the surface facing %.2f %.2f %.2f): %s", h.Side > 0 ? "right" :
          "left", h.Contact[0], h.Contact[1], h.Contact[2], h.Normal[0], h.Normal[1], h.Normal[2], h.Pressed ?
          "her own hand's print" : h.Framed ? "her hand didn't come within 6 cm of it, the drawn hand's print" :
          "her body wasn't read, the drawn hand's print");
      }
      continue;
    }
    if (faithOk)
      PressHand(h, g_Matrices, g_Places);
  }

  // The KrugerSec: their feet each frame (their steps' way, OtherFootWay),
  // and while they lie, their bodies
  BodyMesh const& theirs = Meshes[KindEnforcer];
  for (int i = 0; i < g_PeopleCount; ++i)
  {
    Person& p = g_People[i];
    p.Read = g_Checked[KindEnforcer] >= 0 && ReadBody(p.Animatable, theirs.Bones, g_Matrices, g_Places);
    if (p.Read && !g_Checked[KindEnforcer])
      Check(KindEnforcer, g_Matrices, g_Places);
    if (p.Read && g_Checked[KindEnforcer] > 0)
    {
      float const* at = g_Places + theirs.Hips * 3;
      float dx = at[0] - p.Place[0], dy = at[1] - p.Place[1], dz = at[2] - p.Place[2];
      p.Read = dx * dx + dy * dy + dz * dz < PersonHipsReach * PersonHipsReach;
    }
    if (!p.Read || g_Checked[KindEnforcer] <= 0)
    {
      p.Read = false;
      p.Lying = false;
      continue;
    }
    memcpy(p.Feet[0], g_Places + theirs.LeftFoot * 3, sizeof(p.Feet[0]));
    memcpy(p.Feet[1], g_Places + theirs.RightFoot * 3, sizeof(p.Feet[1]));
    memcpy(p.Toes[0], g_Places + theirs.LeftToe * 3, sizeof(p.Toes[0]));
    memcpy(p.Toes[1], g_Places + theirs.RightToe * 3, sizeof(p.Toes[1]));
    bool pressing = false;
    float const* hips = g_Places + theirs.Hips * 3;
    memcpy(p.Hips, hips, sizeof(p.Hips));
    // (they lie: their hips down by their lower foot; since when, and their
    // capsules for LyingBodies)
    bool lying = hips[1] - (std::min)(p.Feet[0][1], p.Feet[1][1]) < PersonHipsLow;
    if (lying && !p.Lying)
    {
      p.DownAt = now;
      p.KnockAt = 0;
    }
    p.Lying = lying;
    if (!lying)
      p.DownAt = 0;
    else
      for (int c = 0; c < LyingCapsuleCount; ++c)
      {
        memcpy(p.Ends[c][0], g_Places + LyingCapsules[c].A * 3, sizeof(p.Ends[c][0]));
        memcpy(p.Ends[c][1], g_Places + LyingCapsules[c].B * 3, sizeof(p.Ends[c][1]));
      }
    if (peopleSnow[i] && lying)
    {
      // The ground under them: their last step's (see OtherFootWay), else
      // their body's lowest (PressBody: a body lying rests on it)
      bool stepKnown = p.GroundTick && now - p.GroundTick < StepGroundMs &&
        (hips[0] - p.Place[0]) * (hips[0] - p.Place[0]) + (hips[2] - p.Place[2]) * (hips[2] - p.Place[2]) <
        StepGroundReach * StepGroundReach && fabsf(hips[1] - p.Ground) < 1.f;
      float ground = p.Ground;
      // (a stamp along them, head from hips; their hips at its middle)
      float const* head = g_Places + theirs.Head * 3;
      float along[2] = { head[0] - hips[0], head[2] - hips[2] };
      float l = sqrtf(along[0] * along[0] + along[1] * along[1]);
      if (l < 0.1f)
      {
        along[0] = 1.f;
        along[1] = 0.f;
      }
      else
      {
        along[0] /= l;
        along[1] /= l;
      }
      // (lying there again: the stamp they pressed last, if it's still theirs)
      if (p.Slice < 0 && p.LastSlice >= 0 && g_Slices[p.LastSlice].Used && g_Slices[p.LastSlice].LastOwner == p.Entity &&
          g_Slices[p.LastSlice].Mode == g_Mode)
      {
        p.Slice = p.LastSlice;
        g_Slices[p.Slice].Owner = p.Entity;
        g_Slices[p.Slice].Left = 1.f;
      }
      float layer = water ? LeastLayer : (std::max)(peopleLayer[i], LeastLayer);
      float lowest;
      pressing = PressBody(KindEnforcer, g_Matrices, hips, ground, stepKnown, layer, 2, p.Entity, p.Slice, along,
        StampAlong * StampTexel * 0.5f, 0.f, now, lowest);
      if (p.Slice >= 0)
        p.LastSlice = p.Slice;
      if (pressing && (!g_PressLoggedTick[KindEnforcer] || now - g_PressLoggedTick[KindEnforcer] > 30000))
      {
        g_PressLoggedTick[KindEnforcer] = now;
        util::log::Write("Snow: a KrugerSec lies in the %s at %.2f %.2f %.2f: their outline pressed (stamp %d; the ground "
          "from %s; their lowest %.2f m over it, their hips %.2f m; the snow %.0f cm deep there)", water ? "water" : "snow",
          hips[0], hips[1], hips[2], p.Slice, stepKnown ? "their last step" : "their body's lowest point", lowest,
          hips[1] - ground, 100.f * peopleLayer[i]);
      }
    }
    if (!pressing && p.Slice >= 0 && now - g_Slices[p.Slice].Pressed > PressKeptMs)
    {
      g_Slices[p.Slice].Owner = 0;
      p.Slice = -1;
    }
  }
}

ID3D11ShaderResourceView* snowbody::Take(ID3D11DeviceContext* context, float fill, int mode, Stamp* out, int& count)
{
  count = 0;
  if (!context)
    return nullptr;
  Microsoft::WRL::ComPtr<ID3D11Device> device;
  context->GetDevice(&device);
  if (device.Get() != g_Device.Get())
  {
    g_View.Reset();
    g_Texture.Reset();
    g_Device = device;
    g_TextureFailed = false;
    std::lock_guard<std::mutex> lock(g_Mutex);
    for (Slice& s : g_Slices)
    {
      s.DirtyFrom = 0;
      s.DirtyTo = StampAlong - 1;
    }
  }
  if (!g_Texture && !g_TextureFailed && g_Device)
  {
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = StampAcross;
    desc.Height = StampAlong;
    desc.MipLevels = 1;
    desc.ArraySize = MostStamps;
    desc.Format = DXGI_FORMAT_R8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    static unsigned char zeros[StampAlong][StampAcross];
    D3D11_SUBRESOURCE_DATA initial[MostStamps];
    for (D3D11_SUBRESOURCE_DATA& d : initial)
      d = { zeros, StampAcross, 0 };
    HRESULT hr = g_Device->CreateTexture2D(&desc, initial, &g_Texture);
    if (SUCCEEDED(hr))
      hr = g_Device->CreateShaderResourceView(g_Texture.Get(), nullptr, &g_View);
    if (FAILED(hr))
    {
      g_TextureFailed = true;
      g_Texture.Reset();
      g_View.Reset();
      util::log::Warning("Snow: the body prints' texture couldn't be made (0x%08X): no body prints", (unsigned int)hr);
    }
  }
  if (!g_View)
    return nullptr;
  ULONGLONG now = GetTickCount64();
  std::lock_guard<std::mutex> lock(g_Mutex);
  for (int i = 0; i < MostStamps; ++i)
  {
    Slice& s = g_Slices[i];
    if (!s.Used)
      continue;
    // (none while nothing's pressed into; a stamp of the snow gone in the
    // rain, and the other way round)
    if (mode == ModeNone || s.Mode != mode)
    {
      s.Used = false;
      s.Owner = 0;
      continue;
    }
    // (filled as her prints are, once nobody presses it)
    if (!s.Owner || now - s.Pressed >= PressKeptMs)
      s.Left -= fill;
    if (s.Left <= 0.01f)
    {
      s.Used = false;
      s.Owner = 0;
      continue;
    }
    if (s.DirtyFrom <= s.DirtyTo)
    {
      int from, to;
      SmoothRows(s, s.DirtyFrom, s.DirtyTo, from, to);
      D3D11_BOX box{ 0, (UINT)from, 0, (UINT)StampAcross, (UINT)to + 1, 1 };
      context->UpdateSubresource(g_Texture.Get(), D3D11CalcSubresource(0, i, 1), &box, &s.Smooth[from][0], StampAcross, 0);
      s.DirtyFrom = StampAlong;
      s.DirtyTo = -1;
    }
    Stamp& o = out[count++];
    memcpy(o.Corner, s.Corner, sizeof(o.Corner));
    o.Along[0] = s.Along[0];
    o.Along[1] = s.Along[1];
    o.Rise = s.Rise;
    o.Left = s.Left;
    o.Slice = i;
  }
  // (their owners let go of the ones gone)
  if (g_FaithSlice >= 0 && !g_Slices[g_FaithSlice].Used)
    g_FaithSlice = -1;
  for (int i = 0; i < g_PeopleCount; ++i)
  {
    if (g_People[i].Slice >= 0 && !g_Slices[g_People[i].Slice].Used)
      g_People[i].Slice = -1;
    if (g_People[i].LastSlice >= 0 && !g_Slices[g_People[i].LastSlice].Used)
      g_People[i].LastSlice = -1;
  }
  return g_View.Get();
}

bool snowbody::OtherFootWay(float const* contact, int foot, float* way)
{
  ULONGLONG now = GetTickCount64();
  std::lock_guard<std::mutex> lock(g_Mutex);
  float best = FootWayReach * FootWayReach;
  int bestPerson = -1, bestFoot = 0;
  for (int i = 0; i < g_PeopleCount; ++i)
  {
    Person const& p = g_People[i];
    if (!p.Read)
      continue;
    for (int f = 0; f < 2; ++f)
    {
      if (foot != 0 && (foot > 0 ? 1 : 0) != f)
        continue;
      float dx = p.Feet[f][0] - contact[0], dz = p.Feet[f][2] - contact[2];
      float d = dx * dx + dz * dz;
      if (d < best && fabsf(p.Feet[f][1] - contact[1]) < 0.35f)
      {
        best = d;
        bestPerson = i;
        bestFoot = f;
      }
    }
  }
  if (bestPerson < 0)
    return false;
  Person& p = g_People[bestPerson];
  // (their step tells the ground under them: see StepGroundMs)
  p.Ground = contact[1];
  p.GroundTick = now;
  float wx = p.Toes[bestFoot][0] - p.Feet[bestFoot][0], wz = p.Toes[bestFoot][2] - p.Feet[bestFoot][2];
  float l = sqrtf(wx * wx + wz * wz);
  if (l < 0.03f)
    return false;
  way[0] = wx / l;
  way[1] = wz / l;
  return true;
}

int snowbody::LyingBodies(LyingBody* out, int most)
{
  std::lock_guard<std::mutex> lock(g_Mutex);
  int count = 0;
  for (int i = 0; i < g_PeopleCount && count < most; ++i)
  {
    Person const& p = g_People[i];
    if (!p.Read || !p.Lying)
      continue;
    LyingBody& b = out[count++];
    b.Owner = p.Entity;
    b.Segments = LyingCapsuleCount;
    for (int c = 0; c < LyingCapsuleCount; ++c)
    {
      memcpy(b.A[c], p.Ends[c][0], sizeof(b.A[c]));
      memcpy(b.B[c], p.Ends[c][1], sizeof(b.B[c]));
      b.Radius[c] = LyingCapsules[c].Radius;
    }
    b.DownAt = p.DownAt;
    b.KnockAt = p.KnockAt;
  }
  return count;
}

void snowbody::NoteKnock(float const* at)
{
  ULONGLONG now = GetTickCount64();
  std::lock_guard<std::mutex> lock(g_Mutex);
  float best = KnockReach * KnockReach;
  int bestPerson = -1;
  for (int i = 0; i < g_PeopleCount; ++i)
  {
    Person const& p = g_People[i];
    if (!p.Read)
      continue;
    float dx = p.Hips[0] - at[0], dy = p.Hips[1] - at[1], dz = p.Hips[2] - at[2];
    float d = dx * dx + dy * dy + dz * dz;
    if (d < best)
    {
      best = d;
      bestPerson = i;
    }
  }
  if (bestPerson >= 0)
    g_People[bestPerson].KnockAt = now;
}

int snowbody::NoteHandTouch(float const* world, float const* normal, int hand)
{
  float l = sqrtf(normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2]);
  if (!(l > 1e-3f))
    return -1;
  int side = hand > 0 ? 1 : -1;
  ULONGLONG now = GetTickCount64();
  std::lock_guard<std::mutex> lock(g_Mutex);
  // (the same touch: its sound's and its effect's, or the same hand again)
  for (int i = 0; i < MostHandStamps; ++i)
  {
    HandSlice const& h = g_Hands[i];
    float dx = h.Contact[0] - world[0], dy = h.Contact[1] - world[1], dz = h.Contact[2] - world[2];
    if (h.Used && h.Side == side && now - h.Touched < HandSameMs &&
        dx * dx + dy * dy + dz * dz < HandSameMetres * HandSameMetres)
      return i + MostHandStamps * h.Generation;
  }
  int i = g_HandNext;
  g_HandNext = (g_HandNext + 1) % MostHandStamps;
  HandSlice& h = g_Hands[i];
  int generation = (h.Generation + 1) & 0xFFFFFF;
  memset(&h, 0, sizeof(h));
  h.Used = true;
  h.Side = side;
  h.Generation = generation;
  memcpy(h.Contact, world, sizeof(h.Contact));
  for (int k = 0; k < 3; ++k)
    h.Normal[k] = normal[k] / l;
  h.Touched = now;
  h.Dirty = true;
  return i + MostHandStamps * generation;
}

int snowbody::HandStampState(int stamp)
{
  if (stamp < 0)
    return -1;
  ULONGLONG now = GetTickCount64();
  std::lock_guard<std::mutex> lock(g_Mutex);
  HandSlice const& h = g_Hands[stamp % MostHandStamps];
  if (!h.Used || h.Generation != stamp / MostHandStamps)
    return -1;
  // (not followed any more: no update since, nothing read)
  if (h.State == 0 && now - h.Touched > HandPressMs + 500)
    return h.Pressed ? 1 : -1;
  return h.State;
}

ID3D11ShaderResourceView* snowbody::TakeHands(ID3D11DeviceContext* context)
{
  if (!context)
    return nullptr;
  Microsoft::WRL::ComPtr<ID3D11Device> device;
  context->GetDevice(&device);
  if (device.Get() != g_HandDevice.Get())
  {
    g_HandView.Reset();
    g_HandTexture.Reset();
    g_HandDevice = device;
    g_HandTextureFailed = false;
    std::lock_guard<std::mutex> lock(g_Mutex);
    for (HandSlice& h : g_Hands)
      h.Dirty = h.Used;
  }
  if (!g_HandTexture && !g_HandTextureFailed && g_HandDevice)
  {
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = HandStampSide;
    desc.Height = HandStampSide;
    desc.MipLevels = 1;
    desc.ArraySize = MostHandStamps;
    desc.Format = DXGI_FORMAT_R8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    static unsigned char zeros[HandStampSide][HandStampSide];
    D3D11_SUBRESOURCE_DATA initial[MostHandStamps];
    for (D3D11_SUBRESOURCE_DATA& d : initial)
      d = { zeros, HandStampSide, 0 };
    HRESULT hr = g_HandDevice->CreateTexture2D(&desc, initial, &g_HandTexture);
    if (SUCCEEDED(hr))
      hr = g_HandDevice->CreateShaderResourceView(g_HandTexture.Get(), nullptr, &g_HandView);
    if (FAILED(hr))
    {
      g_HandTextureFailed = true;
      g_HandTexture.Reset();
      g_HandView.Reset();
      util::log::Warning("Snow: the handprints' texture couldn't be made (0x%08X): the drawn hand's prints only",
        (unsigned int)hr);
    }
  }
  if (!g_HandView)
    return nullptr;
  std::lock_guard<std::mutex> lock(g_Mutex);
  for (int i = 0; i < MostHandStamps; ++i)
  {
    HandSlice& h = g_Hands[i];
    if (!h.Dirty)
      continue;
    h.Dirty = false;
    // (a texel's softened by its 8 round it, 1 2 1 each way: 4 mm)
    for (int j = 0; j < HandStampSide; ++j)
      for (int k = 0; k < HandStampSide; ++k)
      {
        int sum = 0, weights = 0;
        for (int dj = -1; dj <= 1; ++dj)
          for (int dk = -1; dk <= 1; ++dk)
          {
            int jj = j + dj, kk = k + dk;
            int w = (dj ? 1 : 2) * (dk ? 1 : 2);
            weights += w;
            if (jj >= 0 && jj < HandStampSide && kk >= 0 && kk < HandStampSide)
              sum += h.Mask[jj][kk] * w;
          }
        h.Smooth[j][k] = (unsigned char)((sum + weights / 2) / weights);
      }
    context->UpdateSubresource(g_HandTexture.Get(), D3D11CalcSubresource(0, i, 1), nullptr, h.Smooth, HandStampSide, 0);
  }
  return g_HandView.Get();
}

bool snowbody::HandStampOf(int stamp, HandStamp& out)
{
  if (stamp < 0)
    return false;
  std::lock_guard<std::mutex> lock(g_Mutex);
  HandSlice const& h = g_Hands[stamp % MostHandStamps];
  if (!h.Used || h.Generation != stamp / MostHandStamps || !h.Pressed || !h.Framed)
    return false;
  memcpy(out.Middle, h.Middle, sizeof(out.Middle));
  memcpy(out.U, h.U, sizeof(out.U));
  memcpy(out.V, h.V, sizeof(out.V));
  out.Slice = stamp % MostHandStamps;
  return true;
}

ULONGLONG snowbody::FaithPressedAt()
{
  return g_FaithPressedAt.load();
}

bool snowbody::FaithShoes(float (*out)[3])
{
  std::lock_guard<std::mutex> lock(g_Mutex);
  if (!g_FaithShoesTick || GetTickCount64() - g_FaithShoesTick > FaithShoesMs)
    return false;
  memcpy(out, g_FaithShoes, sizeof(g_FaithShoes));
  return true;
}

void snowbody::Release()
{
  g_View.Reset();
  g_Texture.Reset();
  g_Device.Reset();
}
