#include "PoseEditor.h"
#include "PoseSkeletons.h"
#include "AttachTargets.h"
#include "CameraCollision.h"
#include "PausedHair.h"
#include "../Main.h"
#include "../Util/Util.h"
#include "../imgui/imgui.h"
#include "../imgui/imgui_internal.h"
#include "../Util/ImGuiEXT.h"
#include "../Visuals/Panini.h"
#include <Windows.h>
#include <DirectXMath.h>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <memory>
#include <atomic>
#include <mutex>
#include <string>
#include <vector>

// See PoseEditor.h and Research_Pose_Editor_2026-11-08.md
namespace
{
  // The game build these offsets are from (the replay and Faith's size check
  // the same PE timestamp)
  const DWORD KnownTimeStamp = 0x5762E3B7;

  // Faith's body, found the way the replay finds it (FaithReplay.cpp): her
  // soldier's PamClientReplayRecordingComponent (+ 0x4B0) holds her drawn
  // ClientAntAnimatableEntity (+ 0x50) and her skeleton's pose manager
  // (+ 0x98); her box pose (her culling box's bones) is the animatable at
  // soldier + 0x340, another slot of the same manager.
  const int SoldierRecordingComponentOffset = 0x4B0;
  const __int64 RecordingComponentVTableRva = 0x1C48CD0;
  const int ComponentAnimatableOffset = 0x50;
  const int ComponentManagerOffset = 0x98;
  const int SoldierBoxAnimatableOffset = 0x340;
  // A character's body (the replay's people), a drone's or a gun's (actors)
  const int CharacterBodyOffset = 0x248;
  const int ActorBodyOffset = 0x100;
  const int WeaponOwnerOffset = 0x120;               // a gun's character (FaithReplay.cpp)
  const __int64 AnimatableTypeInfoRva = 0x27EEE10;   // ClientAntAnimatableEntity
  const __int64 WeaponTypeInfoRva = 0x284FB60;       // PamClientCharacterWeaponEntity
  const __int64 DroneTypeInfoRva = 0x2855C20;        // PamClientDroneActorEntity
  const int TypeInstanceListsOffset = 0x60;          // a class's 4 instance lists, 0x10 apart
  const int EntityLinkOffset = 0x40;
  const int MaxPerList = 4096;
  // A body's pose: its pose object (animatable + 0x158) has its pose manager
  // (+ 0x1B8) and its slot's handle (+ 0x118: byte 3 a page, byte 0 a slot).
  // The manager: + 0x08 the bone count (0x142FD7814), + 0x8C the pose stride
  // per slot (bones x 0x20), + 0xF0 the parent of each bone (u32, -1 none: the
  // world pose is built from it, 0x142FD77E8 / 0x142FD021C), + 0x210 pages.
  // A page: + 0x00 the local pose, + 0x08 the world pose (0x20 a bone:
  // translation x y z, the bone's scale, then a rotation quaternion), + 0x30
  // one override per slot.
  const int AnimatablePoseOffset = 0x158;
  const int PoseManagerOffset = 0x1B8;
  const int PoseHandleOffset = 0x118;
  const int ManagerBoneCountOffset = 0x08;
  const int ManagerPoseStrideOffset = 0x8C;
  const int ManagerParentsOffset = 0xF0;
  const int ManagerPagesOffset = 0x210;
  const int PageLocalPoseOffset = 0x00;
  const int PageWorldPoseOffset = 0x08;
  const int PageOverridesOffset = 0x30;
  const __int64 GameAllocatorRva = 0x2364F80;        // what the game's own override vectors carry
  // Whether the game animates a body (FaithReplay.cpp's UpdatedByGame): body
  // = animatable + 0x70, "always update" + 0x5C3, + 0x5C0, mode [+ 0x118] +
  // 0x10 (4: never; 1 and 3 only when external)
  const int BodyUpdateOffset = 0x70;
  const int BodyExternalOffset = 0x5C0;
  const int BodyAlwaysUpdateOffset = 0x5C3;
  const int BodyUpdateModeOffset = 0x118;
  // The main render view (GameRenderer + 0x58: + 0x40 its transform, + 0x360
  // its projection), as the lights' dots read it
  const int RenderViewOffset = 0x58;
  const int RenderViewTransform = 0x40;
  const int RenderViewProjection = 0x360;

  const int FloatsPerBone = 8;
  const int MaxBones = 512;
  const int MaxHeld = 8;
  const int MaxCandidates = 40;
  const float ScanRadius = 30.f;                     // m around the camera: people
  const float DroneScanRadius = 60.f;                // drones
  const ULONGLONG ScanPeriodMs = 1000;
  const ULONGLONG ListWantedMs = 2000;               // the list is kept up this long after the menu last showed it
  const ULONGLONG CheckLogMs = 10000;
  // A held bone counts as the game's taking the pose when its world pose is
  // its parent's times the local handed, this near
  const float TakenDistance = 0.01f;
  const float TakenDot = 0.9995f;
  const size_t MaxUndo = 50;
  const char* const PoseFolder = "Cinematic Tools\\Poses";

  // The game's code this relies on, byte for byte (Faith's size checks the
  // same)
  struct CodeCheck
  {
    __int64 Rva;
    const char* What;
    BYTE Bytes[28];
    size_t Size;
  };
  const CodeCheck CodeChecks[] = {
    // The override's blend: its 4th translation float broadcast into the
    // bone's scale (movss xmm1, [rbx + rbp + 0xC] .. movaps [r15 + rsi*8], xmm4)
    { 0x2FCFA5A, "the override's scale write", { 0xF3, 0x0F, 0x10, 0x4C, 0x2B, 0x0C, 0x0F, 0x28, 0xD9, 0x0F, 0x28, 0xE1,
      0x0F, 0x14, 0xE1, 0x0F, 0x14, 0xD9, 0x0F, 0x14, 0xE3, 0x41, 0x0F, 0x29, 0x24, 0xF7 }, 26 },
    // The pose job's copy of the slot's override (buffer, list, map, weight,
    // count, handle) into the job
    { 0x2FD7D7A, "the override's copy", { 0x49, 0x8B, 0x0C, 0x01, 0x48, 0x85, 0xC9, 0x74, 0x3A, 0x0F, 0x10, 0x01, 0x41,
      0x0F, 0x11, 0x80, 0x30, 0x01, 0x00, 0x00 }, 20 },
    // The blend skips an override whose handle is -1
    { 0x2FCF62D, "the override's handle check", { 0x83, 0xBA, 0x50, 0x01, 0x00, 0x00, 0xFF, 0x4D, 0x89, 0xC5, 0x48,
      0x89, 0xD7, 0x0F, 0x84, 0xB2, 0x04, 0x00, 0x00 }, 19 } };

  // The game's override, laid out like the ghost player component's (+0x1D8),
  // the replay's and Faith's size's
  struct GameVector
  {
    __int64 Allocator;
    void* Data;
    unsigned int Count;
    unsigned int Capacity;
  };
  struct PoseOverride
  {
    float* Bones;           // 0x20 per listed bone, in the list's order
    GameVector* List;       // the skeleton's bones it holds
    GameVector* Map;        // per skeleton bone, its place in Bones (-1 for none)
    float Weight;
    unsigned int Count;
    int Handle;             // the slot it's for (the game sets -1 when it releases the slot)
    int Unused;
  };
  static_assert(offsetof(PoseOverride, Weight) == 0x18 && offsetof(PoseOverride, Count) == 0x1C &&
    offsetof(PoseOverride, Handle) == 0x20, "the game's layout");

  // ---- Small maths: quaternions x y z w, a bone as translation x y z, its
  // 4th float, then a quaternion (the game's 0x20 layout) ----

  // a * b (as FaithReplay.cpp's QuatMul)
  void QMul(float const* a, float const* b, float* out)
  {
    float x = a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1];
    float y = a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0];
    float z = a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3];
    float w = a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2];
    out[0] = x; out[1] = y; out[2] = z; out[3] = w;
  }

  // v turned by q
  void QRot(float const* q, float const* v, float* out)
  {
    float c1[3] = { q[1] * v[2] - q[2] * v[1], q[2] * v[0] - q[0] * v[2], q[0] * v[1] - q[1] * v[0] };
    float c2[3] = { q[1] * c1[2] - q[2] * c1[1], q[2] * c1[0] - q[0] * c1[2], q[0] * c1[1] - q[1] * c1[0] };
    for (int k = 0; k < 3; ++k)
      out[k] = v[k] + 2.f * q[3] * c1[k] + 2.f * c2[k];
  }

  void QConj(float const* q, float* out)
  {
    out[0] = -q[0]; out[1] = -q[1]; out[2] = -q[2]; out[3] = q[3];
  }

  // Unit length; the identity for one that isn't a rotation
  void QNorm(float* q)
  {
    float length = sqrtf(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
    if (!(length > 1e-6f) || !std::isfinite(length))
    {
      q[0] = q[1] = q[2] = 0.f;
      q[3] = 1.f;
      return;
    }
    for (int k = 0; k < 4; ++k)
      q[k] /= length;
  }

  void QIdentity(float* q)
  {
    q[0] = q[1] = q[2] = 0.f;
    q[3] = 1.f;
  }

  bool QIsIdentity(float const* q)
  {
    return fabsf(q[0]) < 1e-6f && fabsf(q[1]) < 1e-6f && fabsf(q[2]) < 1e-6f;
  }

  // A turn of angle (radians) about a unit axis
  void QAxis(float const* axis, float angle, float* out)
  {
    float s = sinf(angle * 0.5f);
    out[0] = axis[0] * s; out[1] = axis[1] * s; out[2] = axis[2] * s;
    out[3] = cosf(angle * 0.5f);
  }

  // Turns about the parent's X, then Y, then Z (degrees): qz * qy * qx
  void QFromAngles(float const* degrees, float* out)
  {
    const float toRad = 3.14159265f / 180.f;
    float x[4], y[4], z[4], zy[4];
    const float ax[3] = { 1.f, 0.f, 0.f }, ay[3] = { 0.f, 1.f, 0.f }, az[3] = { 0.f, 0.f, 1.f };
    QAxis(ax, degrees[0] * toRad, x);
    QAxis(ay, degrees[1] * toRad, y);
    QAxis(az, degrees[2] * toRad, z);
    QMul(z, y, zy);
    QMul(zy, x, out);
    QNorm(out);
  }

  // The angles QFromAngles makes q from
  void QToAngles(float const* q, float* degrees)
  {
    const float toDeg = 180.f / 3.14159265f;
    float x = q[0], y = q[1], z = q[2], w = q[3];
    float sinp = 2.f * (w * y - z * x);
    sinp = sinp > 1.f ? 1.f : (sinp < -1.f ? -1.f : sinp);
    degrees[0] = atan2f(2.f * (w * x + y * z), 1.f - 2.f * (x * x + y * y)) * toDeg;
    degrees[1] = asinf(sinp) * toDeg;
    degrees[2] = atan2f(2.f * (w * z + x * y), 1.f - 2.f * (y * y + z * z)) * toDeg;
  }

  // A bone's world pose from its parent's and its own local pose (the way
  // the game builds it, 0x142FCFC30: the 4th float is 1)
  void Compose(float const* parent, float const* local, float* out)
  {
    float t[3];
    QRot(parent + 4, local, t);
    float q[4];
    QMul(parent + 4, local + 4, q);
    out[0] = parent[0] + t[0];
    out[1] = parent[1] + t[1];
    out[2] = parent[2] + t[2];
    out[3] = 1.f;
    memcpy(out + 4, q, sizeof(q));
  }

  float Distance3(float const* a, float const* b)
  {
    float dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
    return sqrtf(dx * dx + dy * dy + dz * dz);
  }

  void Cross(float const* a, float const* b, float* out)
  {
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
  }

  void Normalize3(float* v)
  {
    float length = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (length > 1e-6f)
      for (int k = 0; k < 3; ++k)
        v[k] /= length;
  }

  // ---- The game's memory (read only but the override table), all in __try ----

  // What an object's getType gives (vtable [0]: lea rax, [rip + x]; ret).
  // Inside the caller's __try.
  __int64 TypeOf(__int64 object)
  {
    __int64 vtable = *(__int64*)object;
    BYTE const* code = vtable ? *(BYTE const**)vtable : nullptr;
    if (!code || code[0] != 0x48 || code[1] != 0x8D || code[2] != 0x05)
      return 0;
    return (__int64)code + 7 + *(int const*)(code + 3);
  }

  bool TypeNameIs(__int64 typeInfo, char const* name)
  {
    __try
    {
      char const* s = *(char const**)*(__int64*)typeInfo;
      return s && strcmp(s, name) == 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
  }

  // Where an entity is (its components' transform, as the replay reads it)
  bool EntityPlace(__int64 entity, float* place)
  {
    __try
    {
      BYTE const* components = *(BYTE const**)(entity + 0x38);
      if ((__int64)components < 0x10000)
        return false;
      memcpy(place, components + 0x40 + ((__int64)components[9] + 2 * components[0xA]) * 0x20, 3 * sizeof(float));
      return std::isfinite(place[0]) && std::isfinite(place[1]) && std::isfinite(place[2]);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
  }

  struct LiveSlot
  {
    __int64 Manager;
    unsigned int Handle;
    int Bones;
    float* Local;
    float* World;
    __int64* Overrides;     // the page's override table
    unsigned int const* Parents;
  };

  // An animatable's pose slot, checked all the way; manager 0: whichever its
  // pose has. Inside the caller's __try.
  bool SlotOf(__int64 animatable, __int64 manager, LiveSlot& slot)
  {
    __int64 base = (__int64)g_gameHandle;
    if (animatable < 0x10000 || TypeOf(animatable) != base + AnimatableTypeInfoRva)
      return false;
    __int64 pose = *(__int64*)(animatable + AnimatablePoseOffset);
    if (pose < 0x10000)
      return false;
    __int64 m = *(__int64*)(pose + PoseManagerOffset);
    if (m < 0x10000 || (manager && m != manager))
      return false;
    unsigned int handle = *(unsigned int*)(pose + PoseHandleOffset);
    int bones = *(int*)(m + ManagerBoneCountOffset);
    int stride = *(int*)(m + ManagerPoseStrideOffset);
    if (handle == 0xFFFFFFFF || bones < 8 || bones > MaxBones || stride != bones * FloatsPerBone * (int)sizeof(float))
      return false;
    unsigned int const* parents = *(unsigned int const**)(m + ManagerParentsOffset);
    if ((__int64)parents < 0x10000)
      return false;
    __int64* pagesBegin = *(__int64**)(m + ManagerPagesOffset);
    __int64* pagesEnd = *(__int64**)(m + ManagerPagesOffset + 8);
    unsigned int page = handle >> 24, index = handle & 0xFF;
    if (!pagesBegin || pagesEnd < pagesBegin || page >= (unsigned int)(pagesEnd - pagesBegin))
      return false;
    __int64 pPage = pagesBegin[page];
    if (pPage < 0x10000)
      return false;
    __int64 local = *(__int64*)(pPage + PageLocalPoseOffset);
    __int64 world = *(__int64*)(pPage + PageWorldPoseOffset);
    __int64 overrides = *(__int64*)(pPage + PageOverridesOffset);
    if (local < 0x10000 || world < 0x10000 || overrides < 0x10000)
      return false;
    slot.Manager = m;
    slot.Handle = handle;
    slot.Bones = bones;
    slot.Local = (float*)(local + (__int64)index * stride);
    slot.World = (float*)(world + (__int64)index * stride);
    slot.Overrides = (__int64*)overrides;
    slot.Parents = parents;
    return true;
  }

  // Faith's body and box pose
  bool FaithSlots(__int64 soldier, __int64& animatable, LiveSlot& body, __int64& boxAnimatable, LiveSlot& box, bool& boxOk)
  {
    __int64 base = (__int64)g_gameHandle;
    boxOk = false;
    __try
    {
      if (soldier < 0x10000)
        return false;
      __int64 component = *(__int64*)(soldier + SoldierRecordingComponentOffset);
      if (component < 0x10000 || *(__int64*)component != base + RecordingComponentVTableRva)
        return false;
      __int64 manager = *(__int64*)(component + ComponentManagerOffset);
      animatable = *(__int64*)(component + ComponentAnimatableOffset);
      if (manager < 0x10000 || !SlotOf(animatable, manager, body))
        return false;
      boxAnimatable = *(__int64*)(soldier + SoldierBoxAnimatableOffset);
      boxOk = SlotOf(boxAnimatable, manager, box) && box.Handle != body.Handle && box.Bones == body.Bones;
      return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
  }

  // A character's or actor's body at its offset, and its slot
  bool BodySlot(__int64 entity, int bodyOffset, __int64& animatable, LiveSlot& slot)
  {
    __try
    {
      if (entity < 0x10000)
        return false;
      animatable = *(__int64*)(entity + bodyOffset);
      return SlotOf(animatable, 0, slot);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
  }

  bool StillThere(__int64 entity, __int64 vtable, int bodyOffset, __int64 animatable)
  {
    __try
    {
      return *(__int64*)entity == vtable && *(__int64*)(entity + bodyOffset) == animatable;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
  }

  __int64 ReadVTable(__int64 entity)
  {
    __try
    {
      return *(__int64*)entity;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return 0;
    }
  }

  // 1: the game animates the body, 0: it doesn't, -1: not readable
  int AnimatedByGame(__int64 animatable)
  {
    __try
    {
      BYTE const* body = (BYTE const*)(animatable + BodyUpdateOffset);
      int always = body[BodyAlwaysUpdateOffset], external = body[BodyExternalOffset];
      __int64 mode = *(__int64 const*)(body + BodyUpdateModeOffset);
      int m = mode >= 0x10000 ? *(int const*)(mode + 0x10) : -1;
      if (m == 4)
        return 0;
      return (external || (always && m != 1 && m != 3)) ? 1 : 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return -1;
    }
  }

  // The slot's override now (-1: not readable)
  __int64 OverrideIn(LiveSlot const& slot)
  {
    __try
    {
      return *(__int64 volatile*)&slot.Overrides[slot.Handle & 0xFF];
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return -1;
    }
  }

  bool CopyFrom(float const* source, float* target, size_t floats)
  {
    __try
    {
      memcpy(target, source, floats * sizeof(float));
      return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
  }

  // The skeleton in the tools' tables with this bone count and these parents
  // (the live manager's, + 0xF0); -1 if none
  int MatchSkeleton(int bones, unsigned int const* parents)
  {
    static unsigned int copy[MaxBones];
    if (bones <= 0 || bones > MaxBones)
      return -1;
    __try
    {
      memcpy(copy, parents, (size_t)bones * sizeof(unsigned int));
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return -1;
    }
    for (int k = 0; k < (int)_countof(pose_skeletons::Skeletons); ++k)
    {
      pose_skeletons::Skeleton const& s = pose_skeletons::Skeletons[k];
      if (s.Bones != bones)
        continue;
      bool same = true;
      for (int i = 0; i < bones && same; ++i)
        same = (int)copy[i] == (int)s.Parents[i];
      if (same)
        return k;
    }
    return -1;
  }

  // ---- The main view, for the dots (read after the free camera is written,
  // as the lights' dots do) ----

  struct View
  {
    float T[16];
    float P[16];
    bool Ok;
  };
  std::mutex g_ViewMutex;
  View g_View{};
  __int64 g_ppGameRenderer = 0;

  bool ReadMainView(float* transform, float* projection)
  {
    __try
    {
      __int64 pGameRenderer = *(__int64*)g_ppGameRenderer;
      if (!pGameRenderer)
        return false;
      __int64 pRenderView = *(__int64*)(pGameRenderer + RenderViewOffset);
      if (!pRenderView)
        return false;
      memcpy(transform, (void const*)(pRenderView + RenderViewTransform), 16 * sizeof(float));
      memcpy(projection, (void const*)(pRenderView + RenderViewProjection), 16 * sizeof(float));
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
    return projection[0] != 0.f && projection[5] != 0.f &&
      fabsf(transform[12]) + fabsf(transform[13]) + fabsf(transform[14]) > 2.f;
  }

  // Where a point is on the screen, in pixels (Panini-aware, as the lights'
  // dots); false behind the camera
  bool ToScreen(View const& v, float const* point, float& x, float& y)
  {
    float const* t = v.T;
    float offset[3] = { point[0] - t[12], point[1] - t[13], point[2] - t[14] };
    float right = offset[0] * t[0] + offset[1] * t[1] + offset[2] * t[2];
    float up = offset[0] * t[4] + offset[1] * t[5] + offset[2] * t[6];
    float depth = -(offset[0] * t[8] + offset[1] * t[9] + offset[2] * t[10]);
    if (depth < 0.1f)
      return false;
    float px = right / depth, py = up / depth;
    panini::ToPanini(px, py);
    float scale = panini::ScreenScale();
    float nx = px * v.P[0] / scale;
    float ny = py * v.P[5] / scale;
    ImVec2 size = ImGui::GetIO().DisplaySize;
    x = (nx * 0.5f + 0.5f) * size.x;
    y = (0.5f - ny * 0.5f) * size.y;
    return true;
  }

  // ---- Who can be posed ----

  enum Kind { Kind_Faith, Kind_Person, Kind_Drone };

  struct Candidate
  {
    __int64 Entity;
    __int64 VTable;
    __int64 Animatable;
    int Kind;
    float Distance;
    int Bones;
    int Skeleton;           // in pose_skeletons::Skeletons, -1 none
    int Animated;           // AnimatedByGame
    bool OtherOverride;     // its slot holds an override not the tools'
    bool Held;
    bool LettingGo;         // held, being let go (its bones put back at the game's next pass)
  };

  // ---- A held pose ----

  struct Snapshot
  {
    std::vector<float> Turns;   // Bones x 4
    std::vector<float> Moves;   // Bones x 3
    std::vector<float> Weapons; // per weapon WeaponSnapshotFloats: its frame (a bone), offsets and edit
  };
  const int WeaponSnapshotFloats = 1 + 8 + 8 + 3 + 4;

  // The face's bones in groups (GAME > Pose's "Group the face"), by their
  // names under the Face bone: the same 57 face bones in Faith's, KrugerSec's,
  // the men's and the women's skeletons (2026-11-08, their SkeletonAssets).
  // Face, Jaw, Tongue, TongueTip and Chin stay single; a grouped bone's
  // children (the inner lips, the irises, the mouth's inner corners and
  // creases) go with it.
  const int FaceGroupCount = 9;
  const int EyesGroup = 0;                   // FaceGroupNames' "Eyes"
  // Dragging the eyes: they look at a point LookDistance in front of them,
  // moved LookGain times as far as the cursor went at their distance
  const float LookDistance = 0.5f;
  const float LookGain = 2.f;
  const int MaxGroupMembers = 16;
  const char* const FaceGroupNames[FaceGroupCount] = { "Eyes", "Upper eyelids", "Lower eyelids", "Eyebrows", "Upper lip",
    "Lower lip", "Mouth corners", "Cheeks", "Nose" };

  // A helper bone (roll, twist, bend, bulge, arm pit, collar, holder, ankle,
  // throat, the end bones, the gun grip): it rides on a real joint, so its
  // dot is hidden unless asked for (they sat on top of the arms' joints)
  bool IsHelperName(char const* n)
  {
    static char const* const parts[] = { "Roll", "Twist", "Bend", "Bulge", "ArmPit", "Collar", "Holder", "Ankle" };
    for (char const* part : parts)
      if (strstr(n, part))
        return true;
    size_t length = strlen(n);
    return !strcmp(n, "Wep_Root") || !strcmp(n, "Throat") || (length > 3 && !strcmp(n + length - 3, "End"));
  }

  int FaceGroupOf(char const* n)
  {
    auto is = [n](char const* s) { return strcmp(n, s) == 0; };
    if (is("LeftEye") || is("RightEye"))
      return 0;
    if (is("LeftUpEyelid") || is("RightUpEyelid"))
      return 1;
    if (is("LeftLowEyelid") || is("RightLowEyelid") || is("LeftLowEyelidCrease") || is("RightLowEyelidCrease"))
      return 2;
    if (strstr(n, "Eyebrow"))
      return 3;
    if (is("LeftUpperLip") || is("UpperLip") || is("RightUpperLip"))
      return 4;
    if (is("LeftLowerLip") || is("LowerLip") || is("RightLowerLip"))
      return 5;
    if (is("LeftMouth") || is("RightMouth"))
      return 6;
    if (strstr(n, "Cheek") || is("LeftCrease") || is("RightCrease"))
      return 7;
    if (is("LeftNose") || is("RightNose") || is("NoseTip") || is("LowNose"))
      return 8;
    return -1;
  }

  // A character's weapon (PamClientCharacterWeaponEntity: KrugerSec's guns
  // and batons, 2026-11-08 live: cpf_shotgun_Mesh, cpf_baton_Mesh). It's
  // drawn where its pose's bone 1 is, and that is its entity's transform (in
  // its local pose; its world pose's bone 1 has the same place, its rotation a
  // quarter turn off, as a character's). The game moves the entity with the
  // hand (the replay's finding), but not with the posed hand: which hand it
  // takes isn't known (live, frozen: a posed KrugerSec's shotgun 0.34 m and
  // another's baton 1.03 m from the posed grips).
  // So while posed, the tools move the entity with the bone it was on and
  // hold the game's own moves back, as the replay does for its guns
  // (util::hooks::MoveActorDrawn / HoldActor), and give its body's placement
  // the same place (util::hooks::PlaceBody: used by the game only for a body
  // with its placement on, + 0xB8: live, the shotgun's was, the baton's not).
  const int MaxWeapons = 4;
  const float GripReach = 0.25f;             // m: a weapon this near the grip (Wep_Root) when taken is in that hand
  const float CarryReach = 0.4f;             // m: else it goes with the nearest body bone this near, or stays the game's
  const int WeaponDataOffset = 0x30;         // the entity's data
  const int WeaponDataMeshOffset = 0x90;     // its Mesh (MeshAsset: + 0x10 its name)
  const int BodyPlacementOnOffset = 0xB8;    // a body (animatable + 0x70): its placement on (0x1432E06B0)
  const int WorldFrame = -2;                 // a weapon's Bone: put down, where it is in the world
  const int MaxWeaponBones = 64;             // a weapon's skeleton (26 bones, 2026-11-08 live)
  struct HeldWeapon
  {
    __int64 Entity, VTable, Animatable;
    // The frame it goes with: a bone, WorldFrame (put down), or -1 (left
    // where the game has it). Its offsets are in that frame; its edit (a
    // move in that frame, a turn about its own place) is on top of them.
    int Bone;
    float OffsetLocal[FloatsPerBone];        // its local pose's bone 1 (its entity's transform) in that frame
    float OffsetWorld[FloatsPerBone];        // its world pose's bone 1 (its body's placement) in that frame
    float EditMove[3];
    float EditTurn[4];
    int TakenBone;                           // as taken (Reset goes back to it)
    float TakenOffsetLocal[FloatsPerBone], TakenOffsetWorld[FloatsPerBone];
    char Name[48];
    bool Holding;                            // its entity moved by the tools, the game's own moves held back
    bool Placing;                            // its body's placement given
    // Its body drawn by the tools (see KeepWeapons): its slot buffers written
    // each frame, these as they were when the tools took it (put back when
    // they let go)
    bool Drawing;
    __int64 Manager;
    unsigned int Handle;
    int Bones;
    float Local0[MaxWeaponBones][FloatsPerBone];
    float World0[MaxWeaponBones][FloatsPerBone];
    float Matrices0[MaxWeaponBones][12];
    float Drawn0[12], Step0[12];             // bone 1's records in the two animation steps (page + 0x18, + 0x20)
    float WrittenMatrix[3];                  // the matrix of bone 1's place written last frame
    float MatrixGap;                         // m: how far the game moved that since (its jobs ran after the tools)
    // The world pose and matrices written last frame: when the slot's differ,
    // the game rebuilt the weapon's pose (time running: its own animation), and
    // the kept pose is taken afresh from it (Rebased)
    float LastWorld[MaxWeaponBones][FloatsPerBone];
    float LastMatrices[MaxWeaponBones][12];
    bool Rebased;
    float FirstLocal1[FloatsPerBone];        // its local bone 1 when the tools first took its body (to put it back there)
    bool FirstLocal1Ok;
    bool TimeRan;                            // time ran while the tools had it: what was kept isn't where the game has it now
    bool Gone;                               // it stopped being the character's (dropped or gone): the game's for good
    // Its body's pose override (the game's own, as the characters' poses
    // use): bone 1 where it's wanted, weight 1, so a pose job of the game's
    // that rebuilds the weapon after the tools' write (time running: Ant's
    // output, override applied, copied over the step and rebuilt into the
    // local and world pose and the matrices, 0x142FCC7C0) rebuilds it there.
    // Two in turn, one filled while the game can still read the other.
    alignas(16) float OverBuffer[2][FloatsPerBone];
    PoseOverride Over[2];
    GameVector OverListVector, OverMapVector;
    unsigned int OverList[1];
    unsigned int OverMap[MaxWeaponBones];
    bool OverReady;
    int OverNext;
    __int64* OverTable;                      // where it was handed (0: not now)
    unsigned int OverTableHandle;
    bool OverRefused;
    float Rows0[16];                         // its entity's rows before the tools first moved it
    bool Rows0Ok;
    float Written[FloatsPerBone];            // the local bone 1 written last frame
    bool WrittenOk;
    float Gap;                               // m: how far the game moved it from the tools' place since the last frame
    bool GapOk;
    int MoveFailures;
    int DrawFailures;
  };

  struct Held
  {
    bool Used;
    int Id;
    int Kind;
    __int64 Entity, VTable;
    int BodyOffset;              // people and drones: where the body is (Faith: her recording component's)
    __int64 Animatable, BoxAnimatable;
    __int64 Manager;
    unsigned int Handle, BoxHandle;
    bool HasBox;
    int Bones, Skeleton, Root;
    short Parents[MaxBones];
    unsigned char Depth[MaxBones];
    bool InHeld[MaxBones];
    bool Face[MaxBones];         // the Face bone and every bone below it (moved, not bent, when dragged)
    bool Finger[MaxBones];       // a hand's children (but Wep_Root) and below
    bool Helper[MaxBones];       // a roll, twist or other helper bone (its dot hidden unless asked for)
    // Whether the world's sweeps hit this character's own body (-1 not known
    // yet): probed at its first drag with "Bones stop at walls" (ProbeSelf)
    int SelfHit;
    bool Eye[MaxBones];          // LeftEye / RightEye: dragged, they look (LookEyes)
    short Part[MaxBones];        // the body part it's in (its first bone): a drag bends bones of that part only
    signed char Group[MaxBones]; // a face group it's in (FaceGroupNames), -1 none
    bool FollowsGroup[MaxBones]; // below a bone in a face group
    int GroupSize[FaceGroupCount];
    int GroupMembers[FaceGroupCount][MaxGroupMembers];
    int Count;
    unsigned int List[MaxBones];
    unsigned int Map[MaxBones];
    float Base[MaxBones][FloatsPerBone];   // the local pose when taken
    float Turn[MaxBones][4];               // each bone's turn in its parent's frame (local = turn * base)
    float Move[MaxBones][3];               // each bone's move, in its parent's frame (the held root's moves the whole body)
    // Two buffers in turn: one is filled while the game's pose job can still
    // be reading the other. The box pose's override shares them (same bones).
    alignas(128) float Buffer[2][MaxBones * FloatsPerBone];
    PoseOverride Body[2], Box[2];
    GameVector ListVector, MapVector;
    int Next;
    __int64* BodyTable;          // where ours was handed (0: not now)
    __int64* BoxTable;
    unsigned int BodyTableHandle, BoxTableHandle;
    float Handed[MaxBones][FloatsPerBone]; // the locals handed last frame, for the check
    bool HandedOk;
    float World[MaxBones][FloatsPerBone];  // this frame's world pose: the game's, the held bones from the pose
    bool WorldOk;
    char Label[64];
    char Status[200];
    bool Aside, Refused, BoxRefused, NoBoxLogged;
    // Whether the game took the pose: frames with time running / frozen
    int Frames, Taken, FrozenFrames, FrozenTaken;
    float Worst;
    ULONGLONG CheckSince;
    char CheckLine[200];
    // A character's weapons (guns, batons: actors of their own), each going
    // with the bone it was on when taken (see KeepWeapons)
    int GripBone;
    HeldWeapon Weapons[MaxWeapons];
    int WeaponCount;
    ULONGLONG WeaponsCheckedAt;
    // Idle movement (UpdateIdle): each held bone's animation as the game
    // made it under the pose, smoothed (where it has been: Ref, local
    // rotation then translation), and its movement about that now (Idle:
    // a turn in the bone's own frame, then a move in its parent's)
    float IdleRef[MaxBones][7];
    float Idle[MaxBones][7];
    bool IdleRefTurn[MaxBones];              // that bone's Ref rotation taken (from a good record the game wrote)
    bool IdleRefMove[MaxBones];              // and its translation
    bool IdleRefOk;
    bool IdleActive;                         // Idle is laid over the handed pose (by IdleWeight)
    float IdleWeight;                        // 0 - 1, fading in and out (IdleFadeSeconds)
    ULONGLONG IdleWeightAt;
    bool IdleGateOpen;                       // the character standing (see UpdateIdle)
    float IdleRoot[FloatsPerBone];           // where it was at the last capture (its root's parent)
    bool IdleRootOk;
    // Letting go (KeepLettingGo): its overrides out, held on until the game's
    // own pass of its slots put back what the game doesn't animate
    bool Restoring;
    ULONGLONG RestoreSince;
    bool RestoreWaitLogged;
    char RestoreWhy[96];
    unsigned int IdleGen;                    // its wanting (IdleCapture::Gen)
    unsigned int IdleSeen;                   // the capture's count last read
    ULONGLONG IdleAt, IdleNewAt;
    int IdleFrames;                          // captures used since the last log line
    std::vector<Snapshot> Undo;
  };

  std::mutex g_Mutex;                        // everything below (the menu and the game's thread)
  Held g_Held[MaxHeld];
  int g_NextId = 1;
  int g_Active = -1;                         // the held pose the menu and the dots edit (index in g_Held)
  int g_SelectedBone = -1;
  int g_SelectedGroup = -1;                  // a face group picked instead of a bone (-1 none)
  int g_SelectedWeapon = -1;                 // a weapon of the held pose picked instead (index in its Weapons, -1 none)
  std::vector<Candidate> g_Candidates;
  __int64 g_Picked = 0;                      // the candidate picked in the list (its entity)
  ULONGLONG g_ListWantedAt = 0, g_ScannedAt = 0;
  // From the menu, done on the game's thread
  bool g_TakeRequest = false;
  Candidate g_TakeWhat{};
  int g_ReleaseRequest = -2;                 // -2 none, -1 all, else a held index
  bool g_ShowBones = true;
  bool g_ShowFace = false;
  bool g_ShowFingers = false;
  bool g_GroupFace = true;                   // the face's bones in groups (FaceGroupNames)
  bool g_ShowHelpers = false;                // roll, twist and other helper bones' dots
  bool g_StopAtWalls = true;                 // a dragged bone (and its limb) stops at what's solid
  bool g_FreeMove = false;                   // dragging moves the bone itself (any bone), with X / Y / Z arrows
  const float MaxFreeMove = 1.f;             // m: a bone's move (not the Hips', not a face bone's)
  bool g_WallsReady = false;                 // the world's sweeps can be made (the game's thread says)
  const float BoneRadius = 0.05f;            // m: the dragged bone's ball in the world's sweeps
  const float JointRadius = 0.03f;           // m: the other moved joints' (smaller: toes sit 2 cm over the floor)
  const float SelfRadius = 0.5f;             // m: round where a character stands, its own body (see SelfHit)
  bool g_RingsOnBone = true;                 // the rings about the bone's own axes (else the world's)
  bool g_HairWhilePaused = true;             // Faith's hair simulated while time is frozen (PausedHair.cpp)
  bool g_WeaponsFollow = true;               // a posed character's guns and batons go with its posed bones (KeepWeapons)
  bool g_IdleOn = false;                     // the game's own animation's small movements over the pose (UpdateIdle)
  float g_IdleAmount = 1.f;                  // how much of them
  const float IdleSmoothSeconds = 3.f;       // each bone's movement is taken about where it has been this long
  const float MaxIdleTurn = 15.f * 3.14159265f / 180.f;  // and kept small: at most this turn
  const float MaxIdleMove = 0.08f;           // and this move (m)
  const float IdleFadeSeconds = 0.4f;        // it fades in and out this fast
  const float IdleGateTurn = 10.f * 3.14159265f / 180.f;  // a body bone this far off where its animation has been: not idle
  const float IdleGateSpeed = 0.3f;          // m/s: the character moving faster than this: not idle
  const ULONGLONG IdleStaleMs = 500;         // no capture this long: it fades out
  char g_Filter[64] = "";
  char g_SaveName[64] = "";
  std::vector<std::string> g_PoseFiles;
  ULONGLONG g_FilesListedAt = 0;
  char g_FileMessage[200] = "";
  bool g_SlidersWereActive = false;
  // The ring being dragged
  struct RingDrag
  {
    bool On;
    int Held, Bone, Ring;
    float Axis[3];
    float Tangent[2];        // pixels per radian along the ring where it was taken
    float Mouse[2];
    float Parent[4];         // the bone's parent's world rotation when taken
    float Turn[4];           // the bone's turn when taken
    int Group;               // a face group's ring: each member turned the same way about its own place (-1: a bone)
    float GroupParents[MaxGroupMembers][4];
    float GroupTurns[MaxGroupMembers][4];
    int Weapon = -1;         // a weapon's ring (Bone and Group -1): its edit turned (Parent: its frame's rotation)
  };
  RingDrag g_Drag{};
  // A bone's dot held with the mouse: pressed on it (Pending), then moving it
  // once the cursor went MoveStartPixels (On)
  struct MoveDrag
  {
    bool Pending, On;
    int Held, Bone;
    int Group;               // a face group's dot (Bone -1): every member moved the same way
    float Mouse[2];          // where it was pressed
    float Offset[2];         // the dot's place on the screen less the cursor's then
    float Base[3];           // the plane it moves on goes through here (the wheel: nearer or farther)
    float Start[3];          // the bone's (or group's middle's) place when pressed
    Snapshot Pose;           // the pose when it was pressed
    float Target[3];         // where it's wanted now
    bool TargetOk;
    // Shift held: the cursor going up and down moves it farther and nearer
    // along the view, from where it was when Shift went down
    bool Depth;
    float DepthBase[3];
    float DepthMouseY;
    // Bones stop at walls: the menu's thread only says where it's wanted
    // (Want); the game's thread (the world's sweeps are only made there)
    // moves it there as far as it can go from where it was last (Accepted),
    // and solves the pose
    float Want[3];
    bool WantOk;
    Snapshot Accepted;       // the pose the last frame that went through
    float AcceptedPlace[3];  // the dragged bone's place in it
    // Free move's arrows: taken by an arrow, it moves along that axis only,
    // as far as the cursor goes along the arrow on the screen
    bool OnAxis;
    float Axis[3];           // the world way it moves
    float AxisScreen[2];     // the arrow on the screen, pixels for AxisLength metres
    float AxisLength;
    int Weapon = -1;         // a weapon's dot (Bone and Group -1): its edit's move
    float WeaponFrame[FloatsPerBone];  // its frame (its bone's posed place, or the world's) when pressed
  };
  MoveDrag g_Move{};
  const float MoveStartPixels = 3.f;
  const float MaxFaceMove = 0.15f;           // m: a face bone's move
  const float MoveWheelStep = 0.9f;          // the distance from the camera a wheel notch
  const float DepthSpeed = 1.5f;             // Shift held: a pixel up moves it this many pixels' worth farther
  const float DotReach = 0.75f;              // font sizes: how near the cursor has to be to a dot to take it
  int g_MoveBones = 2;                       // a moved bone bends this many bones above it
  // The game's thread
  std::once_flag g_Checked;
  bool g_Available = false;
  char g_Unavailable[200] = "";
  std::unique_ptr<attach::Scan> g_Scan;
  std::vector<attach::Target> g_Found;
  bool g_ClassesFound = false, g_ClassesLogged = false;
  bool g_DronesLogged = false;
  ULONGLONG g_ReplayAsideLoggedAt = 0;

  bool CheckGame()
  {
    __int64 base = (__int64)g_gameHandle;
    __try
    {
      IMAGE_DOS_HEADER* pDos = (IMAGE_DOS_HEADER*)base;
      IMAGE_NT_HEADERS* pNt = (IMAGE_NT_HEADERS*)(base + pDos->e_lfanew);
      if (pNt->FileHeader.TimeDateStamp != KnownTimeStamp)
      {
        sprintf_s(g_Unavailable, "Not available: the game isn't the build the tools know (PE timestamp 0x%08X)",
          pNt->FileHeader.TimeDateStamp);
        return false;
      }
      for (CodeCheck const& check : CodeChecks)
        if (memcmp((void const*)(base + check.Rva), check.Bytes, check.Size) != 0)
        {
          sprintf_s(g_Unavailable, "Not available: the game's pose code at 0x%I64X (%s) isn't what the tools know",
            0x140000000 + check.Rva, check.What);
          return false;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      strcpy_s(g_Unavailable, "Not available: the game's code couldn't be read");
      return false;
    }
    return true;
  }

  bool Available()
  {
    std::call_once(g_Checked, []
    {
      g_Available = CheckGame();
      if (!g_Available)
        util::log::Warning("Pose: %s; GAME > Pose is off", g_Unavailable);
      g_ppGameRenderer = util::offsets::GetOffset("OFFSET_GAMERENDERER");
    });
    return g_Available;
  }

  char const* BoneName(Held const& h, int bone)
  {
    static char buffer[16];
    if (h.Skeleton >= 0 && bone >= 0 && bone < h.Bones)
      return pose_skeletons::Skeletons[h.Skeleton].Names[bone];
    sprintf_s(buffer, "bone %d", bone);
    return buffer;
  }

  int BoneByName(int skeleton, char const* name)
  {
    if (skeleton < 0)
      return -1;
    pose_skeletons::Skeleton const& s = pose_skeletons::Skeletons[skeleton];
    for (int i = 0; i < s.Bones; ++i)
      if (!strcmp(s.Names[i], name))
        return i;
    return -1;
  }

  char const* KindName(int kind, int skeleton)
  {
    if (kind == Kind_Faith)
      return "Faith";
    if (skeleton >= 0)
      return pose_skeletons::Skeletons[skeleton].Label;
    return kind == Kind_Drone ? "drone" : "character";
  }

  // The held bone's local pose as the editor has it (the 4th float always 1:
  // the game's blend takes it as the bone's scale and never puts it back)
  void EditedLocal(Held const& h, int bone, float* out)
  {
    memcpy(out, h.Base[bone], FloatsPerBone * sizeof(float));
    for (int k = 0; k < 3; ++k)
      out[k] += h.Move[bone][k];
    out[3] = 1.f;
    float q[4];
    QMul(h.Turn[bone], h.Base[bone] + 4, q);
    QNorm(q);
    memcpy(out + 4, q, sizeof(q));
  }

  bool IsOurs(PoseOverride const* pair, __int64 entry)
  {
    return entry == (__int64)&pair[0] || entry == (__int64)&pair[1];
  }

  // Ours into the slot (false if it holds another override, or can't be written)
  bool HandTo(PoseOverride* pair, int k, LiveSlot const& slot, __int64*& table, unsigned int& tableHandle)
  {
    __try
    {
      LONG64 volatile* entry = (LONG64 volatile*)&slot.Overrides[slot.Handle & 0xFF];
      LONG64 now = *entry;
      if (now != 0 && !IsOurs(pair, now))
        return false;
      pair[k].Handle = (int)slot.Handle;
      InterlockedExchange64(entry, (LONG64)&pair[k]);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      pair[k].Handle = -1;
      return false;
    }
    table = slot.Overrides;
    tableHandle = slot.Handle;
    return true;
  }

  // Ours out of the slot it was handed to, if it's still there (the game
  // clears it itself when it releases a slot)
  void TakeBack(PoseOverride* pair, __int64*& table, unsigned int tableHandle)
  {
    if (table)
    {
      __try
      {
        LONG64 volatile* entry = (LONG64 volatile*)&table[tableHandle & 0xFF];
        LONG64 now = *entry;
        if (IsOurs(pair, now))
          InterlockedCompareExchange64(entry, 0, now);
      }
      __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
    table = nullptr;
    pair[0].Handle = pair[1].Handle = -1;
  }

  // A character's weapons: the PamClientCharacterWeaponEntities whose
  // character is it (+ 0x120), each with a body of its own (+ 0x100)
  int FindWeapons(__int64 character, HeldWeapon* out, int max)
  {
    __int64 base = (__int64)g_gameHandle;
    __int64 typeInfo = base + WeaponTypeInfoRva;
    if (!TypeNameIs(typeInfo, "PamClientCharacterWeaponEntity"))
      return 0;
    int count = 0;
    __try
    {
      for (int list = 0; list < 4 && count < max; ++list)
      {
        __int64 link = *(__int64*)(typeInfo + TypeInstanceListsOffset + list * 0x10);
        for (int n = 0; link && n < MaxPerList && count < max; ++n, link = *(__int64*)link)
        {
          __int64 entity = link - EntityLinkOffset;
          if (TypeOf(entity) != typeInfo || *(__int64*)(entity + WeaponOwnerOffset) != character)
            continue;
          __int64 body = *(__int64*)(entity + ActorBodyOffset);
          if (body < 0x10000 || TypeOf(body) != base + AnimatableTypeInfoRva)
            continue;
          bool known = false;
          for (int k = 0; k < count; ++k)
            known |= out[k].Entity == entity;
          if (known)
            continue;
          HeldWeapon& w = out[count++];
          memset(&w, 0, sizeof(w));
          w.Entity = entity;
          w.VTable = *(__int64*)entity;
          w.Animatable = body;
          w.Bone = -1;
        }
      }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
    return count;
  }

  // Its mesh's name, its path's last part without "_Mesh" (cpf_baton)
  void WeaponName(HeldWeapon& w)
  {
    strcpy_s(w.Name, "weapon");
    __try
    {
      __int64 data = *(__int64*)(w.Entity + WeaponDataOffset);
      __int64 mesh = data >= 0x10000 ? *(__int64*)(data + WeaponDataMeshOffset) : 0;
      char const* name = mesh >= 0x10000 ? *(char const**)(mesh + 0x10) : nullptr;
      if ((__int64)name < 0x10000)
        return;
      char const* last = name;
      for (int n = 0; n < 200 && name[n]; ++n)
        if (name[n] == '/')
          last = name + n + 1;
      size_t length = 0;
      while (length + 1 < sizeof(w.Name) && last[length] >= 32 && last[length] < 127)
      {
        w.Name[length] = last[length];
        ++length;
      }
      w.Name[length] = 0;
      char* suffix = strstr(w.Name, "_Mesh");
      if (suffix)
        *suffix = 0;
      if (!w.Name[0])
        strcpy_s(w.Name, "weapon");
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      strcpy_s(w.Name, "weapon");
    }
  }

  // Where a weapon is: its pose's bone 1, local (its entity's transform) and
  // world (its body's placement); false if it isn't that character's any more
  bool WeaponPlace(HeldWeapon const& w, __int64 character, float* local, float* world)
  {
    LiveSlot slot{};
    __try
    {
      if (*(__int64*)w.Entity != w.VTable || *(__int64*)(w.Entity + ActorBodyOffset) != w.Animatable ||
          *(__int64*)(w.Entity + WeaponOwnerOffset) != character || !SlotOf(w.Animatable, 0, slot))
        return false;
      memcpy(local, slot.Local + 1 * FloatsPerBone, FloatsPerBone * sizeof(float));
      memcpy(world, slot.World + 1 * FloatsPerBone, FloatsPerBone * sizeof(float));
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
    float const* both[2] = { local, world };
    for (float const* p : both)
    {
      float length = p[4] * p[4] + p[5] * p[5] + p[6] * p[6] + p[7] * p[7];
      if (!std::isfinite(p[0]) || !std::isfinite(p[1]) || !std::isfinite(p[2]) || !(length > 0.9f && length < 1.1f))
        return false;
    }
    return true;
  }

  // Its entity's rows (4x4, as FaithReplay.cpp's ReadEntityRows: its
  // components + 0x38, the transform at + 0x10 + (c[9] + 2 c[0xA]) x 0x20)
  bool WeaponRows(__int64 entity, float* rows)
  {
    __try
    {
      BYTE const* components = *(BYTE const**)(entity + 0x38);
      if ((__int64)components < 0x10000)
        return false;
      memcpy(rows, components + 0x10 + ((__int64)components[9] + 2 * components[0xA]) * 0x20, 16 * sizeof(float));
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
    for (int i = 0; i < 16; ++i)
      if (!(fabsf(rows[i]) < 1e6f))
        return false;
    return true;
  }

  // A place and rotation into rows: the rotation as XMMatrixRotationQuaternion
  // makes it (an actor's rows are, the replay's finding), then the place; the
  // rows' fourth floats left as they are
  void RowsFrom(float const* qt, float* rows)
  {
    float x = qt[4], y = qt[5], z = qt[6], w = qt[7];
    rows[0] = 1.f - 2.f * (y * y + z * z); rows[1] = 2.f * (x * y + z * w); rows[2] = 2.f * (x * z - y * w);
    rows[4] = 2.f * (x * y - z * w); rows[5] = 1.f - 2.f * (x * x + z * z); rows[6] = 2.f * (y * z + x * w);
    rows[8] = 2.f * (x * z + y * w); rows[9] = 2.f * (y * z - x * w); rows[10] = 1.f - 2.f * (x * x + y * y);
    rows[12] = qt[0]; rows[13] = qt[1]; rows[14] = qt[2];
  }

  // a^-1 * b: b in a's frame
  void Relative(float const* a, float const* b, float* out)
  {
    float inverse[4];
    QConj(a + 4, inverse);
    float d[3] = { b[0] - a[0], b[1] - a[1], b[2] - a[2] };
    QRot(inverse, d, out);
    out[3] = 1.f;
    QMul(inverse, b + 4, out + 4);
  }

  // A weapon body's slot buffers, as the replay finds a body's (FaithReplay.cpp
  // FindRawSlot): local and world pose (manager + 0x8C a slot), the 3x4
  // matrices (+ 0x10, manager + 0x90, there when manager + 0x2B9 is set) and
  // the two animation steps (+ 0x18, + 0x20, manager + 0x98; 0x30 a bone:
  // scale, rotation, translation). Live 2026-11-08 (cpf_baton, 26 bones):
  // 0x340, 0x4E0, 0x4E0, + 0x2B9 set. The two steps are Ant's last two pose
  // steps in the local pose's layout (bone 1 is local bone 1), which the
  // render-side entry at [page + 0x40] + slot x 0x20 points at as from and to
  // for its blend (read live on Faith, 2026-11-08); the world pose and the
  // matrices are built from the local pose only (0x142FCFC30), and a skinned
  // draw takes the matrices (Research_Pose_Editor section 2.2).
  const int ManagerMatrixStrideOffset = 0x90;
  const int ManagerStepStrideOffset = 0x98;
  const int ManagerHasMatricesOffset = 0x2B9;
  const int PageMatricesOffset = 0x10;
  const int PageDrawnOffset = 0x18;
  const int PageStepOffset = 0x20;
  const int StepBoneFloats = 12;
  struct WeaponSlot
  {
    __int64 Manager;
    unsigned int Handle;
    int Bones;
    float* Local;
    float* World;
    float* Matrices;
    float* Drawn;
    float* Step;
  };

  bool WeaponSlotOf(__int64 animatable, WeaponSlot& out)
  {
    LiveSlot slot{};
    __try
    {
      if (!SlotOf(animatable, 0, slot) || slot.Bones > MaxWeaponBones)
        return false;
      __int64 m = slot.Manager;
      int matrixStride = *(int*)(m + ManagerMatrixStrideOffset);
      int stepStride = *(int*)(m + ManagerStepStrideOffset);
      if (!*(BYTE*)(m + ManagerHasMatricesOffset) || matrixStride != slot.Bones * 12 * (int)sizeof(float) ||
          stepStride < slot.Bones * StepBoneFloats * (int)sizeof(float) || stepStride > 0x10000)
        return false;
      __int64* pages = *(__int64**)(m + ManagerPagesOffset);
      BYTE* page = (BYTE*)pages[slot.Handle >> 24];
      unsigned int index = slot.Handle & 0xFF;
      BYTE* matrices = *(BYTE**)(page + PageMatricesOffset);
      BYTE* drawn = *(BYTE**)(page + PageDrawnOffset);
      BYTE* step = *(BYTE**)(page + PageStepOffset);
      if ((__int64)matrices < 0x10000 || (__int64)drawn < 0x10000 || (__int64)step < 0x10000)
        return false;
      out.Manager = m;
      out.Handle = slot.Handle;
      out.Bones = slot.Bones;
      out.Local = slot.Local;
      out.World = slot.World;
      out.Matrices = (float*)(matrices + (size_t)index * matrixStride);
      out.Drawn = (float*)(drawn + (size_t)index * stepStride);
      out.Step = (float*)(step + (size_t)index * stepStride);
      return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
  }

  // Its slot buffers as they are, kept (the tools take it over from there)
  bool KeepWeaponBuffers(HeldWeapon& w)
  {
    WeaponSlot slot{};
    if (!WeaponSlotOf(w.Animatable, slot) || slot.Bones < 3)
      return false;
    __try
    {
      memcpy(w.Local0, slot.Local, (size_t)slot.Bones * FloatsPerBone * sizeof(float));
      memcpy(w.World0, slot.World, (size_t)slot.Bones * FloatsPerBone * sizeof(float));
      memcpy(w.Matrices0, slot.Matrices, (size_t)slot.Bones * 12 * sizeof(float));
      memcpy(w.Drawn0, slot.Drawn + 1 * StepBoneFloats, sizeof(w.Drawn0));
      memcpy(w.Step0, slot.Step + 1 * StepBoneFloats, sizeof(w.Step0));
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
    float const* l = w.Local0[1];
    float length = l[4] * l[4] + l[5] * l[5] + l[6] * l[6] + l[7] * l[7];
    if (!std::isfinite(l[0]) || !std::isfinite(l[1]) || !std::isfinite(l[2]) || !(length > 0.98f && length < 1.02f))
      return false;
    w.Manager = slot.Manager;
    w.Handle = slot.Handle;
    w.Bones = slot.Bones;
    return true;
  }

  // A rotation as a matrix for column vectors (rows), as FaithReplay.cpp's QuatMatrix
  void QMatrix(float const* q, float m[3][3])
  {
    float x = q[0], y = q[1], z = q[2], w = q[3];
    m[0][0] = 1.f - 2.f * (y * y + z * z); m[0][1] = 2.f * (x * y - z * w); m[0][2] = 2.f * (x * z + y * w);
    m[1][0] = 2.f * (x * y + z * w); m[1][1] = 1.f - 2.f * (x * x + z * z); m[1][2] = 2.f * (y * z - x * w);
    m[2][0] = 2.f * (x * z - y * w); m[2][1] = 2.f * (y * z + x * w); m[2][2] = 1.f - 2.f * (x * x + y * y);
  }

  // A step record's bone (scale, rotation, translation) with this local pose
  void StepWith(float const* kept, float const* local, float* out)
  {
    memcpy(out, kept, StepBoneFloats * sizeof(float));
    memcpy(out + 4, local + 4, 4 * sizeof(float));   // rotation
    memcpy(out + 8, local, 3 * sizeof(float));       // translation (scale and fourth floats kept)
  }

  // Its body where its local bone 1 (its entity's transform; bone 0 is the
  // identity, so that's in the world) is wanted: the whole body as it was
  // kept, moved rigidly by the move from its kept bone 1 to wanted. Bone 1's
  // local pose and both animation steps' bone 1, every world pose from bone 1
  // on, every matrix from bone 1 on (the world pose times the skinning's fixed
  // transform: the same move from the left). Bone 0 and the other bones'
  // local poses are left as they are. False if its slot isn't the one kept.
  // A 3x4 matrix's place (rows, the translation in the fourth column)
  void ReadMatrixPlace(float const* m, float* place)
  {
    __try
    {
      place[0] = m[3];
      place[1] = m[7];
      place[2] = m[11];
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
  }

  // The slot's buffers now, if the game rebuilt them since the tools' write
  // last frame (they differ from it): the kept pose taken afresh from them
  void RebaseWeaponBody(HeldWeapon& w, WeaponSlot const& slot)
  {
    if (!w.WrittenOk)
      return;
    __try
    {
      size_t worldBytes = (size_t)w.Bones * FloatsPerBone * sizeof(float), matrixBytes = (size_t)w.Bones * 12 * sizeof(float);
      if (!memcmp(slot.World, w.LastWorld, worldBytes) && !memcmp(slot.Matrices, w.LastMatrices, matrixBytes))
        return;
      float local1[FloatsPerBone];
      memcpy(local1, slot.Local + 1 * FloatsPerBone, sizeof(local1));
      float length = local1[4] * local1[4] + local1[5] * local1[5] + local1[6] * local1[6] + local1[7] * local1[7];
      if (!std::isfinite(local1[0]) || !std::isfinite(local1[1]) || !std::isfinite(local1[2]) ||
          !(length > 0.98f && length < 1.02f))
        return;
      memcpy(w.Local0[1], local1, sizeof(local1));
      memcpy(w.World0, slot.World, worldBytes);
      memcpy(w.Matrices0, slot.Matrices, matrixBytes);
      memcpy(w.Drawn0, slot.Drawn + 1 * StepBoneFloats, sizeof(w.Drawn0));
      memcpy(w.Step0, slot.Step + 1 * StepBoneFloats, sizeof(w.Step0));
      w.Rebased = true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
  }

  bool WriteWeaponBody(HeldWeapon& w, float const* wanted, float* matrixPlace)
  {
    WeaponSlot slot{};
    if (!WeaponSlotOf(w.Animatable, slot) || slot.Manager != w.Manager || slot.Handle != w.Handle || slot.Bones != w.Bones)
      return false;
    RebaseWeaponBody(w, slot);
    float inverse[4], dq[4], turned[3], dt[3];
    QConj(w.Local0[1] + 4, inverse);
    QMul(wanted + 4, inverse, dq);
    QNorm(dq);
    QRot(dq, w.Local0[1], turned);
    for (int k = 0; k < 3; ++k)
      dt[k] = wanted[k] - turned[k];
    float r[3][3];
    QMatrix(dq, r);
    static float world[MaxWeaponBones][FloatsPerBone], matrices[MaxWeaponBones][12];
    memcpy(world[0], w.World0[0], sizeof(world[0]));
    memcpy(matrices[0], w.Matrices0[0], sizeof(matrices[0]));
    for (int i = 1; i < w.Bones; ++i)
    {
      float const* w0 = w.World0[i];
      float p[3];
      QRot(dq, w0, p);
      for (int k = 0; k < 3; ++k)
        world[i][k] = dt[k] + p[k];
      world[i][3] = w0[3];
      QMul(dq, w0 + 4, world[i] + 4);
      QNorm(world[i] + 4);
      float const* m0 = w.Matrices0[i];
      for (int row = 0; row < 3; ++row)
      {
        for (int col = 0; col < 3; ++col)
          matrices[i][row * 4 + col] = r[row][0] * m0[0 * 4 + col] + r[row][1] * m0[1 * 4 + col] + r[row][2] * m0[2 * 4 + col];
        matrices[i][row * 4 + 3] = r[row][0] * m0[3] + r[row][1] * m0[7] + r[row][2] * m0[11] + dt[row];
      }
    }
    float local1[FloatsPerBone], drawn1[StepBoneFloats], step1[StepBoneFloats];
    memcpy(local1, wanted, sizeof(local1));
    local1[3] = w.Local0[1][3];
    StepWith(w.Drawn0, wanted, drawn1);
    StepWith(w.Step0, wanted, step1);
    __try
    {
      memcpy(slot.Local + 1 * FloatsPerBone, local1, sizeof(local1));
      memcpy(slot.Drawn + 1 * StepBoneFloats, drawn1, sizeof(drawn1));
      memcpy(slot.Step + 1 * StepBoneFloats, step1, sizeof(step1));
      memcpy(slot.World, world, (size_t)w.Bones * FloatsPerBone * sizeof(float));
      memcpy(slot.Matrices, matrices, (size_t)w.Bones * 12 * sizeof(float));
      memcpy(w.LastWorld, world, (size_t)w.Bones * FloatsPerBone * sizeof(float));
      memcpy(w.LastMatrices, matrices, (size_t)w.Bones * 12 * sizeof(float));
      matrixPlace[0] = matrices[1][3];
      matrixPlace[1] = matrices[1][7];
      matrixPlace[2] = matrices[1][11];
      return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
  }

  // Its body's slot buffers as they were kept, if it's still that slot
  void RestoreWeaponBody(HeldWeapon const& w)
  {
    WeaponSlot slot{};
    if (!WeaponSlotOf(w.Animatable, slot) || slot.Manager != w.Manager || slot.Handle != w.Handle || slot.Bones != w.Bones)
      return;
    __try
    {
      memcpy(slot.Local, w.Local0, (size_t)w.Bones * FloatsPerBone * sizeof(float));
      memcpy(slot.World, w.World0, (size_t)w.Bones * FloatsPerBone * sizeof(float));
      memcpy(slot.Matrices, w.Matrices0, (size_t)w.Bones * 12 * sizeof(float));
      memcpy(slot.Drawn + 1 * StepBoneFloats, w.Drawn0, sizeof(w.Drawn0));
      memcpy(slot.Step + 1 * StepBoneFloats, w.Step0, sizeof(w.Step0));
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
  }

  // Where a weapon is wanted: its frame (its bone's posed place, or the
  // world's) times its offset with its edit (the move in that frame, the turn
  // about its own place)
  void WeaponFrame(Held const& h, HeldWeapon const& w, float (*world)[FloatsPerBone], float* frame)
  {
    if (w.Bone >= 0 && w.Bone < h.Bones)
      memcpy(frame, world[w.Bone], FloatsPerBone * sizeof(float));
    else
    {
      frame[0] = frame[1] = frame[2] = 0.f;
      frame[3] = 1.f;
      QIdentity(frame + 4);
    }
  }

  void WeaponWanted(Held const& h, HeldWeapon const& w, float (*world)[FloatsPerBone], float const* offset, float* out)
  {
    float frame[FloatsPerBone], edited[FloatsPerBone];
    WeaponFrame(h, w, world, frame);
    for (int k = 0; k < 3; ++k)
      edited[k] = offset[k] + w.EditMove[k];
    edited[3] = 1.f;
    QMul(w.EditTurn, offset + 4, edited + 4);
    QNorm(edited + 4);
    Compose(frame, edited, out);
  }

  // Its body's pose override handed for the next frame: bone 1 at wanted
  void HandWeaponPose(Held const& h, HeldWeapon& w, float const* wanted)
  {
    LiveSlot slot{};
    bool ok = false;
    __try
    {
      ok = SlotOf(w.Animatable, 0, slot) && slot.Bones == w.Bones && slot.Bones <= MaxWeaponBones && slot.Bones > 1;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      ok = false;
    }
    if (!ok)
      return;
    if (!w.OverReady)
    {
      __int64 allocator = (__int64)g_gameHandle + GameAllocatorRva;
      w.OverList[0] = 1;
      for (int i = 0; i < MaxWeaponBones; ++i)
        w.OverMap[i] = 0xFFFFFFFFu;
      w.OverMap[1] = 0;
      w.OverListVector = { allocator, w.OverList, 1u, 1u };
      w.OverMapVector = { allocator, w.OverMap, (unsigned int)w.Bones, (unsigned int)w.Bones };
      for (int k = 0; k < 2; ++k)
        w.Over[k] = { w.OverBuffer[k], &w.OverListVector, &w.OverMapVector, 1.f, 1u, -1, 0 };
      w.OverNext = 0;
      w.OverReady = true;
    }
    // Another slot than the one it was handed to: out of that one first
    if (w.OverTable && (w.OverTable != slot.Overrides || w.OverTableHandle != slot.Handle))
      TakeBack(w.Over, w.OverTable, w.OverTableHandle);
    int k = w.OverNext;
    memcpy(w.OverBuffer[k], wanted, sizeof(w.OverBuffer[k]));
    w.OverBuffer[k][3] = 1.f;                 // the override's 4th float is the bone's scale
    w.Over[k] = { w.OverBuffer[k], &w.OverListVector, &w.OverMapVector, 1.f, 1u, (int)slot.Handle, 0 };
    if (HandTo(w.Over, k, slot, w.OverTable, w.OverTableHandle))
    {
      w.OverNext ^= 1;
      w.OverRefused = false;
    }
    else if (!w.OverRefused)
    {
      w.OverRefused = true;
      util::log::Write("Pose: %s 0x%I64X: its %s's pose slot holds another override (a replay's?): only drawn by the "
        "tools", h.Label, h.Entity, w.Name);
    }
  }

  // Its weapons back to the game: their bodies' slot buffers as the tools
  // found them, their entities back where they were and the game's own moves
  // of them let through (its next one puts each where it has its hand), their
  // bodies' placement their own. Game thread.
  // characterThere: the character was found this frame (false: it's gone or
  // being torn down, and its weapons with it: nothing of them is touched).
  // What was kept is put back only into a weapon checked this frame (still
  // that weapon, the character's), and only if time didn't run while the
  // tools had it (else it isn't where the game has the weapon now, and the
  // game's next update and move put it in the hand once time runs). The
  // tools' own lists are always let go.
  void LetGoWeapon(Held const& h, HeldWeapon& w, bool characterThere)
  {
    float local[FloatsPerBone], world[FloatsPerBone];
    bool there = characterThere && (w.Drawing || w.Holding || w.Rows0Ok) && WeaponPlace(w, h.Entity, local, world);
    bool putBack = there && !w.TimeRan;
    if (w.Drawing && putBack)
    {
      // As kept, or (the game rebuilt it meanwhile) its pose now moved back to
      // its first bone 1
      float scratch[3];
      if (!w.Rebased)
        RestoreWeaponBody(w);
      else if (w.FirstLocal1Ok)
        WriteWeaponBody(w, w.FirstLocal1, scratch);
    }
    if (w.Placing)
      util::hooks::PlaceBody(w.Animatable + BodyUpdateOffset, nullptr);
    // Its override out (only if it's still ours there)
    TakeBack(w.Over, w.OverTable, w.OverTableHandle);
    if (w.Rows0Ok && putBack)
      util::hooks::MoveActorDrawn(w.Entity, w.Rows0);
    if (w.Holding)
      util::hooks::PoseHoldActor(w.Entity, false);
    w.Drawing = w.Placing = w.Holding = w.WrittenOk = w.GapOk = w.Rows0Ok = w.Rebased = w.TimeRan = w.FirstLocal1Ok = false;
  }

  void LetGoWeapons(Held& h, bool characterThere)
  {
    for (int i = 0; i < h.WeaponCount; ++i)
      LetGoWeapon(h, h.Weapons[i], characterThere);
  }

  // Each frame the pose is handed: each of its weapons where its frame is now
  // (its bone's posed place, or put down), as it was on it when taken, with
  // its edit. The game doesn't redraw a weapon from its moved entity while
  // time is frozen (2026-11-08 live: its entity at the posed grip, its local
  // bone 1, and so its world pose and matrices, 14.5 cm away where it was
  // before), so its body is drawn by the tools, the way the replay draws the
  // bodies the game doesn't update (FaithReplay.cpp DrawBody): its slot
  // buffers written each frame (WriteWeaponBody) at the camera hook, after
  // the frame's pose jobs (the game wrote Faith's world
  // pose and matrices before every one of 1336 camera hooks, and the tools'
  // writes there were drawn: Research_Pose_Editor section 2.2). Its update
  // isn't held: a held body goes on the game's list of bodies it doesn't
  // update, and nothing was found that puts one with a pose slot back
  // (MEC_Reverse_Notes, the replay's guns, 2026-10-24). The entity is moved
  // too, its own moves held back. Game thread.
  // drawn: the world pose of the character as the game drew it this frame
  // (KeepHeld's chain: the pose handed last frame), which the weapon goes with
  // (h.World is the one handed now, drawn next frame)
  void KeepWeapons(Held& h, ULONGLONG now, float (*drawn)[FloatsPerBone], bool timeFrozen)
  {
    bool check = now - h.WeaponsCheckedAt >= 1000;
    if (check)
      h.WeaponsCheckedAt = now;
    for (int i = 0; i < h.WeaponCount; ++i)
    {
      HeldWeapon& w = h.Weapons[i];
      if (w.Bone == -1 || w.Bone >= h.Bones)
      {
        // Left to the game (an Undo can give one back)
        if (w.Drawing || w.Holding || w.Placing || w.Rows0Ok || w.OverTable)
          LetGoWeapon(h, w, true);
        continue;
      }
      float local[FloatsPerBone], world[FloatsPerBone];
      if (!WeaponPlace(w, h.Entity, local, world))
      {
        // Dropped, gone or another's: the game's from now on (its memory may
        // be another's soon: nothing of it is put back)
        if (w.Placing)
          util::hooks::PlaceBody(w.Animatable + BodyUpdateOffset, nullptr);
        if (w.Holding)
          util::hooks::PoseHoldActor(w.Entity, false);
        TakeBack(w.Over, w.OverTable, w.OverTableHandle);
        w.Drawing = w.Placing = w.Holding = w.GapOk = w.WrittenOk = w.Rows0Ok = w.Rebased = w.TimeRan = w.FirstLocal1Ok = false;
        util::log::Write("Pose: %s 0x%I64X: its %s 0x%I64X isn't its own any more (dropped or gone): left to the game",
          h.Label, h.Entity, w.Name, w.Entity);
        w.Bone = -1;
        w.Gone = true;
        continue;
      }
      // Whether the game moved it from where the tools put it last frame: its
      // local bone 1, and its bone 1's matrix (the drawn one)
      if (w.WrittenOk && check)
      {
        w.Gap = Distance3(w.Written, local);
        WeaponSlot now{};
        float place[3] = { w.WrittenMatrix[0], w.WrittenMatrix[1], w.WrittenMatrix[2] };
        if (WeaponSlotOf(w.Animatable, now) && now.Bones == w.Bones)
          ReadMatrixPlace(now.Matrices + 1 * 12, place);
        w.MatrixGap = Distance3(w.WrittenMatrix, place);
        w.GapOk = true;
      }
      float wantLocal[FloatsPerBone], wantWorld[FloatsPerBone];
      WeaponWanted(h, w, drawn, w.OffsetLocal, wantLocal);
      WeaponWanted(h, w, drawn, w.OffsetWorld, wantWorld);
      bool wantOk = true;
      for (int k = 0; k < FloatsPerBone; ++k)
        wantOk = wantOk && std::isfinite(wantLocal[k]) && std::isfinite(wantWorld[k]) && fabsf(wantLocal[k]) < 5e5f &&
          fabsf(wantWorld[k]) < 5e5f;
      if (!wantOk)
        continue;

      if (!timeFrozen && (w.Drawing || w.Holding))
        w.TimeRan = true;

      // Its body, drawn by the tools
      if (!w.Drawing && w.DrawFailures < 3)
      {
        if (KeepWeaponBuffers(w))
        {
          w.Drawing = true;
          if (!w.FirstLocal1Ok)
          {
            memcpy(w.FirstLocal1, w.Local0[1], sizeof(w.FirstLocal1));
            w.FirstLocal1Ok = true;
          }
          util::log::Write("Pose: %s 0x%I64X: its %s's body is drawn by the tools now (%d bones)", h.Label, h.Entity, w.Name,
            w.Bones);
        }
        else if (++w.DrawFailures == 3)
          util::log::Warning("Pose: %s 0x%I64X: its %s's body couldn't be taken over (its slot buffers not found): only "
            "its entity is moved", h.Label, h.Entity, w.Name);
      }
      if (w.Drawing)
      {
        if (WriteWeaponBody(w, wantLocal, w.WrittenMatrix))
        {
          memcpy(w.Written, wantLocal, sizeof(w.Written));
          w.WrittenOk = true;
          w.DrawFailures = 0;
          // The override is used by the next frame's pose job, with the pose
          // handed now: from that pose
          float nextLocal[FloatsPerBone];
          WeaponWanted(h, w, h.World, w.OffsetLocal, nextLocal);
          HandWeaponPose(h, w, nextLocal);
        }
        else
        {
          // Its slot changed: taken over afresh next frame (that snapshot isn't
          // the first one: put back from the first bone 1, as after a rebase).
          // Its override out meanwhile (handed again with the next write).
          TakeBack(w.Over, w.OverTable, w.OverTableHandle);
          w.Drawing = w.WrittenOk = w.GapOk = false;
          w.Rebased = true;
          ++w.DrawFailures;
          util::log::Write("Pose: %s 0x%I64X: its %s's pose slot changed: taken over again", h.Label, h.Entity, w.Name);
        }
      }

      // Its entity (its rows as the game had them kept first, to put back;
      // after that its rows are made from those, not read back)
      alignas(16) float rows[16];
      if (w.Rows0Ok)
        memcpy(rows, w.Rows0, sizeof(rows));
      else if (!WeaponRows(w.Entity, rows))
      {
        if (w.MoveFailures++ == 0)
          util::log::Warning("Pose: %s 0x%I64X: its %s's transform couldn't be read: its entity isn't moved", h.Label,
            h.Entity, w.Name);
        continue;
      }
      if (!w.Rows0Ok)
      {
        memcpy(w.Rows0, rows, sizeof(w.Rows0));
        w.Rows0Ok = true;
      }
      RowsFrom(wantLocal, rows);
      if (!w.Holding)
      {
        w.Holding = util::hooks::PoseHoldActor(w.Entity, true);
        if (!w.Holding && w.MoveFailures++ == 0)
          util::log::Warning("Pose: %s 0x%I64X: the game's own moves of its %s can't be held back (too many held)",
            h.Label, h.Entity, w.Name);
      }
      if (!util::hooks::MoveActorDrawn(w.Entity, rows) && w.MoveFailures++ == 0)
        util::log::Warning("Pose: %s 0x%I64X: its %s's entity can't be moved (the models' setTransform isn't hooked)",
          h.Label, h.Entity, w.Name);
      // Its body's placement too, for when its body isn't drawn by the tools
      if (!w.Drawing)
      {
        float placement[16] = {};
        RowsFrom(wantWorld, placement);
        placement[15] = 1.f;
        if (util::hooks::PlaceBody(w.Animatable + BodyUpdateOffset, placement))
          w.Placing = true;
      }
      else if (w.Placing)
      {
        util::hooks::PlaceBody(w.Animatable + BodyUpdateOffset, nullptr);
        w.Placing = false;
      }
    }
  }

  // A click the overlay takes from ImGui ends a field being typed in, as a
  // click elsewhere would (else the typed text goes on into what's picked next)
  void EndTextEdit()
  {
    if (ImGui::IsAnyItemActive())
      ImGui::ClearActiveID();
  }

  // A weapon's frame changed with it staying where it is: its offsets made
  // anew from where it's wanted now, its edit none. To WorldFrame: put down.
  void MoveWeaponTo(Held& h, HeldWeapon& w, int bone)
  {
    if (!h.WorldOk || w.Bone == -1)
      return;
    float local[FloatsPerBone], world[FloatsPerBone], frame[FloatsPerBone];
    WeaponWanted(h, w, h.World, w.OffsetLocal, local);
    WeaponWanted(h, w, h.World, w.OffsetWorld, world);
    w.Bone = bone;
    WeaponFrame(h, w, h.World, frame);
    Relative(frame, local, w.OffsetLocal);
    Relative(frame, world, w.OffsetWorld);
    w.EditMove[0] = w.EditMove[1] = w.EditMove[2] = 0.f;
    QIdentity(w.EditTurn);
  }

  // As it was taken: its bone (or the game's), no edit
  void ResetWeapon(HeldWeapon& w)
  {
    if (w.Gone)
      return;
    w.Bone = w.TakenBone;
    memcpy(w.OffsetLocal, w.TakenOffsetLocal, sizeof(w.OffsetLocal));
    memcpy(w.OffsetWorld, w.TakenOffsetWorld, sizeof(w.OffsetWorld));
    w.EditMove[0] = w.EditMove[1] = w.EditMove[2] = 0.f;
    QIdentity(w.EditTurn);
  }

  void IdleUnwanted(int index);              // see Idle movement, below
  bool BlendHook();

  // ---- What the game's blend writes ----
  //
  // The game's blend (0x142FCF620, see Idle movement below) writes the pose
  // into Ant's own records, in place, and Ant rewrites only the channels it
  // animates: one it doesn't (a free-moved bone's translation, mostly; the
  // size override's scale before it) kept the pose's value after the override
  // went, and the character stayed so (2026-11-09, the user: "free moved
  // bones don't return to their original position"). So for each held pose
  // and each of its slots (the body's; Faith's box pose's) the blend hook
  // keeps, per held bone, its record as the slot's last pass left it (Seen)
  // and as the game itself last wrote it (Game: the record just before the
  // pose's first blend, then each channel the game wrote since a pass). A
  // channel (scale, rotation or translation: 4 floats) that is, at a pass's
  // start, bit for bit as Seen is one the game didn't write since:
  //  - while our override is in, the idle movement leaves it out (it holds
  //    the tools' own pose, not the game's animation; CaptureIdle);
  //  - while it's out (let go, stepped aside for a replay, its slot taken by
  //    another override), the hook puts it back as in Game at the slot's next
  //    pass, before the blend (PutBackChannels): a channel the game animates
  //    gets the game's last value of it, one it doesn't the value it had
  //    before the pose.
  // The pose job fills the blend's item afresh for each slot (0x142FD7C60:
  // + 0 - 0xEF from the slot's own data, + 0xA0 its records; + 0x130 -
  // 0x157 from its override if it has one, else + 0x130 null and + 0x150 -1
  // as the caller set them, 0x142FCC935 / 0x142FCC981) and its map (rsp +
  // 0x40, 0x142FCC8EF: + 0 the manager, + 0x20 its bone -> record map, + 0x2C
  // its bones, + 0x30 [manager + 0x88], the record count step 2 copies the
  // records by at 0x142FCCB05), then calls the blend with or without an
  // override (offline, 2026-11-09). So a slot's pass without our override is
  // known by its manager and its records.
  struct BlendWatch
  {
    std::atomic<bool> On;
    std::atomic<int> Busy;                   // hooks working in it now (Unwatch waits for none)
    // Set by the game thread while it's off
    float* Buffers[2];                       // the pose's override bone buffers
    int Handles[2];                          // its slots: the body's, the box pose's (-1 none)
    int Count;
    unsigned int List[MaxBones];             // the held bones (skeleton bones)
    // The game thread's say, per slot
    std::atomic<bool> PutBack[2];            // our override isn't in it: its channels put back at its passes
    std::atomic<int> Passes[2];              // its passes that did (zeroed as PutBack goes on)
    std::atomic<int> Channels, Changed;      // channels put back; those of them more than PutBackNoticed off
    // Learnt by the hook at our blends (a slot's passes come one at a time)
    std::atomic<__int64> Records[2], Manager[2];  // the slot's records and their manager
    std::atomic<unsigned int> RecordCount[2];
    std::atomic<int> RecordsMoved;           // our blends that found a slot's records somewhere else
    std::atomic<bool> GameOk[2];             // the slot blended by us (set at its first pass, never cleared: the game thread reads it)
    std::atomic<bool> SeenOk[2];             // Seen holds (the hook's own gate)
    float Game[2][MaxBones][12];             // per skeleton bone: its record (scale, rotation, translation) as the game last wrote it
    float Seen[2][MaxBones][12];             // as the slot's last pass left it
    // The idle capture's copy, made before its seqlock opens (the body's passes only)
    float Scratch[MaxBones][12];
    unsigned char ScratchKept[MaxBones];
  };
  BlendWatch g_Watches[MaxHeld];
  std::atomic<int> g_Watched{ 0 };           // watches on
  std::atomic<int> g_BlendFaults{ 0 };       // passes whose reads or writes faulted (the rest of them skipped)
  const float PutBackNoticed = 1e-4f;        // a channel put back more than this off (m, or a rotation's part): changed
  const ULONGLONG PutBackMs = 3000;          // letting go waits this long for the game's pass at most
  const unsigned int MaxRecords = 65536;     // a slot's record count above this isn't taken as one

  // The hook out of a watch: none is in it once this returns. Game thread.
  void Unwatch(int index)
  {
    BlendWatch& w = g_Watches[index];
    if (w.On.exchange(false, std::memory_order_seq_cst))
      g_Watched.fetch_sub(1, std::memory_order_seq_cst);
    while (w.Busy.load(std::memory_order_seq_cst) != 0)
      YieldProcessor();
  }

  // Whether our override is out of that slot now (on: its channels are put
  // back at its passes). Game thread.
  void SetPutBack(int index, int k, bool on)
  {
    BlendWatch& w = g_Watches[index];
    if (w.PutBack[k].load(std::memory_order_relaxed) == on)
      return;
    if (on)
      w.Passes[k].store(0, std::memory_order_relaxed);
    w.PutBack[k].store(on, std::memory_order_seq_cst);
  }

  // A held pose's slots watched from its first blend on (Take). Game thread.
  bool Watch(Held& h)
  {
    int index = (int)(&h - g_Held);
    Unwatch(index);
    if (!BlendHook())
      return false;
    BlendWatch& w = g_Watches[index];
    w.Buffers[0] = h.Buffer[0];
    w.Buffers[1] = h.Buffer[1];
    w.Handles[0] = (int)h.Handle;
    w.Handles[1] = h.HasBox ? (int)h.BoxHandle : -1;
    w.Count = h.Count;
    memcpy(w.List, h.List, (size_t)h.Count * sizeof(w.List[0]));
    for (int k = 0; k < 2; ++k)
    {
      w.PutBack[k].store(false, std::memory_order_relaxed);
      w.Passes[k].store(0, std::memory_order_relaxed);
      w.Records[k].store(0, std::memory_order_relaxed);
      w.Manager[k].store(0, std::memory_order_relaxed);
      w.RecordCount[k].store(0, std::memory_order_relaxed);
      w.GameOk[k].store(false, std::memory_order_relaxed);
      w.SeenOk[k].store(false, std::memory_order_relaxed);
    }
    w.Channels.store(0, std::memory_order_relaxed);
    w.Changed.store(0, std::memory_order_relaxed);
    w.RecordsMoved.store(0, std::memory_order_relaxed);
    w.On.store(true, std::memory_order_seq_cst);
    g_Watched.fetch_add(1, std::memory_order_seq_cst);
    return true;
  }

  // Not editable from now on: no pick, no drag
  void Unpick(int index)
  {
    if (g_Active == index)
    {
      g_Active = -1;
      g_SelectedBone = -1;
      g_SelectedGroup = -1;
      g_SelectedWeapon = -1;
    }
    if (g_Drag.On && g_Drag.Held == index)
      g_Drag.On = false;
    if ((g_Move.Pending || g_Move.On) && g_Move.Held == index)
      g_Move.Pending = g_Move.On = false;
  }

  // Its overrides out, its watch off and its slot free (after letting go, or at once)
  void FinishRelease(Held& h, char const* why, char const* restored)
  {
    int index = (int)(&h - g_Held);
    TakeBack(h.Body, h.BodyTable, h.BodyTableHandle);
    TakeBack(h.Box, h.BoxTable, h.BoxTableHandle);
    Unwatch(index);
    if (h.Frames + h.FrozenFrames > 0)
      util::log::Write("Pose: %s 0x%I64X: the game took the pose in %d of %d frames with time running, %d of %d with time "
        "frozen, since the last line", h.Label, h.Entity, h.Taken, h.Frames, h.FrozenTaken, h.FrozenFrames);
    util::log::Write("Pose: %s 0x%I64X let go (%s)%s", h.Label, h.Entity, why, restored);
    int moved = g_Watches[index].RecordsMoved.load();
    if (moved)
      util::log::Warning("Pose: %s 0x%I64X: its slots' records were found somewhere else %d times while held (the tools "
        "take them to stay put)", h.Label, h.Entity, moved);
    int faults = g_BlendFaults.exchange(0);
    if (faults)
      util::log::Warning("Pose: the tools' part of the game's pose blend faulted in %d passes (the rest of each skipped)",
        faults);
    IdleUnwanted(index);
    h.Restoring = false;
    h.Used = false;
    h.Undo.clear();
    h.Undo.shrink_to_fit();
    Unpick(index);
  }

  // Let go: its overrides out at once, and (see BlendWatch) the pose held on
  // (Restoring, not editable) until the game's own pass of each slot it was
  // blended into has put back what the game didn't write since (KeepLettingGo).
  // gone: the character or its slot was found gone or changed this frame: at
  // once (its weapons are then only let go of, nothing written into them).
  void Release(Held& h, char const* why, bool gone = false)
  {
    int index = (int)(&h - g_Held);
    if (h.Restoring)
    {
      // Gone, or let go again while it waits (the menu)
      FinishRelease(h, why, gone ? "; gone before the game's pass put its bones back" : "; let go again before the game's "
        "pass: what the game doesn't animate stays as posed");
      return;
    }
    bool there = !gone && (h.Kind != Kind_Person || StillThere(h.Entity, h.VTable, h.BodyOffset, h.Animatable));
    LetGoWeapons(h, there);
    IdleUnwanted(index);
    TakeBack(h.Body, h.BodyTable, h.BodyTableHandle);
    TakeBack(h.Box, h.BoxTable, h.BoxTableHandle);
    BlendWatch& w = g_Watches[index];
    bool watched = w.On.load(std::memory_order_relaxed), blended = false;
    if (there)
      for (int k = 0; k < 2; ++k)
        if (w.GameOk[k].load(std::memory_order_acquire))
        {
          // Its passes counted from now (the override is out already)
          w.Passes[k].store(0, std::memory_order_seq_cst);
          w.PutBack[k].store(true, std::memory_order_seq_cst);
          blended = true;
        }
    if (blended)
    {
      h.Restoring = true;
      h.RestoreSince = GetTickCount64();
      h.RestoreWaitLogged = false;
      strncpy_s(h.RestoreWhy, why, _TRUNCATE);
      Unpick(index);
      strcpy_s(h.Status, "Letting go: its bones put back as the game has them");
      return;
    }
    FinishRelease(h, why, there && !watched ? "; its bones not put back: the game's pose blend isn't hooked (see above)" :
      "");
  }

  // Each frame while letting go: done once the game's own pass of each slot
  // the pose was blended into has put its channels back (the box pose's only
  // while it's still the one). While the game doesn't animate the body (far
  // away: its distance switch) no pass comes: it waits (KeepHeld lets it go
  // if the character or its slot goes). A body the game switched off for its
  // death isn't switched on again: let go then. With the game animating it
  // and still no pass in PutBackMs (the slot not recognised): let go, logged.
  void KeepLettingGo(Held& h, bool boxSame, ULONGLONG now)
  {
    BlendWatch& w = g_Watches[&h - g_Held];
    bool body = !w.GameOk[0].load(std::memory_order_acquire) || w.Passes[0].load(std::memory_order_acquire) > 0;
    bool box = !w.GameOk[1].load(std::memory_order_acquire) || !boxSame || w.Passes[1].load(std::memory_order_acquire) > 0;
    char done[260];
    if (body && box)
    {
      sprintf_s(done, ": %d of its bones' channels the game hadn't written since the pose put back as the game last had "
        "them, %d of them more than %g off (its body's records: %u)", w.Channels.load(), w.Changed.load(), PutBackNoticed,
        w.RecordCount[0].load());
      FinishRelease(h, h.RestoreWhy, done);
      return;
    }
    int animated = AnimatedByGame(h.Animatable);
    if (animated != 1)
    {
      if (animated == 0 && util::hooks::IsBodyDead(h.Animatable + BodyUpdateOffset))
      {
        sprintf_s(done, ": dead (the game switched its body off) before the game's pass, so what it doesn't animate stays "
          "as posed (%d channels put back)", w.Channels.load());
        FinishRelease(h, h.RestoreWhy, done);
        return;
      }
      if (!h.RestoreWaitLogged)
        util::log::Write("Pose: %s 0x%I64X: let go while the game doesn't animate its body: its bones are put back when it "
          "does", h.Label, h.Entity);
      h.RestoreWaitLogged = true;
      h.RestoreSince = now;
      strcpy_s(h.Status, "Letting go: waiting for the game to animate it again");
      return;
    }
    if (now >= h.RestoreSince + PutBackMs)
    {
      int k = body ? 1 : 0;
      sprintf_s(done, ": the game animates it, but no pass of its %s reached the tools within %.0f s (records 0x%I64X, "
        "manager 0x%I64X), so what it doesn't animate there stays as posed (%d channels put back)", k ? "box pose" : "body",
        PutBackMs / 1000.0, w.Records[k].load(), w.Manager[k].load(), w.Channels.load());
      FinishRelease(h, h.RestoreWhy, done);
    }
  }

  // The candidate's pose taken as it is now, and held. Game thread.
  void Take(Candidate const& c, __int64 soldier)
  {
    for (int i = 0; i < MaxHeld; ++i)
      if (g_Held[i].Used && g_Held[i].Entity == c.Entity)
      {
        if (g_Held[i].Restoring)
          util::log::Warning("Pose: %s 0x%I64X is being let go (its bones put back): take it again in a moment",
            g_Held[i].Label, c.Entity);
        else
          g_Active = i;
        return;
      }
    int index = -1;
    for (int i = 0; i < MaxHeld && index < 0; ++i)
      if (!g_Held[i].Used)
        index = i;
    if (index < 0)
    {
      util::log::Warning("Pose: %d poses are held already, let one go first", MaxHeld);
      return;
    }
    LiveSlot body{}, box{};
    __int64 animatable = 0, boxAnimatable = 0;
    bool boxOk = false, ok = false;
    int bodyOffset = 0;
    if (c.Kind == Kind_Faith)
      ok = c.Entity == soldier && FaithSlots(soldier, animatable, body, boxAnimatable, box, boxOk);
    else
    {
      bodyOffset = c.Kind == Kind_Drone ? ActorBodyOffset : CharacterBodyOffset;
      ok = BodySlot(c.Entity, bodyOffset, animatable, body) && animatable == c.Animatable;
    }
    char const* kind = KindName(c.Kind, c.Skeleton);
    if (!ok)
    {
      util::log::Warning("Pose: %s 0x%I64X: its pose slot isn't found any more, not taken", kind, c.Entity);
      return;
    }
    int skeleton = MatchSkeleton(body.Bones, body.Parents);
    if (skeleton < 0)
    {
      util::log::Warning("Pose: %s 0x%I64X: its skeleton (%d bones) isn't one the tools know the bones of, not taken",
        kind, c.Entity, body.Bones);
      return;
    }
    int animated = AnimatedByGame(animatable);
    if (animated == 0)
    {
      util::log::Warning("Pose: %s 0x%I64X: the game doesn't animate its body now (far away or taken down), so it can't "
        "take a pose; not taken", kind, c.Entity);
      return;
    }
    __int64 entry = OverrideIn(body);
    if (entry != 0)
    {
      util::log::Warning("Pose: %s 0x%I64X: its pose slot 0x%X holds another override (a replay's or a Dash ghost's), "
        "not taken", kind, c.Entity, body.Handle);
      return;
    }

    Held& h = g_Held[index];
    h.Used = true;
    h.Id = g_NextId++;
    h.Kind = c.Kind;
    h.Entity = c.Entity;
    h.VTable = ReadVTable(c.Entity);
    h.BodyOffset = bodyOffset;
    h.Animatable = animatable;
    h.BoxAnimatable = boxOk ? boxAnimatable : 0;
    h.HasBox = boxOk;
    h.Manager = body.Manager;
    h.Handle = body.Handle;
    h.BoxHandle = boxOk ? box.Handle : 0xFFFFFFFFu;
    h.Bones = body.Bones;
    h.Skeleton = skeleton;
    pose_skeletons::Skeleton const& s = pose_skeletons::Skeletons[skeleton];
    h.Root = s.HeldRoot;
    // Held: the root and every bone below it (a bone's parent always comes
    // before it in the game's skeletons)
    int face = BoneByName(skeleton, "Face");
    int leftHand = BoneByName(skeleton, "LeftHand"), rightHand = BoneByName(skeleton, "RightHand");
    // A hand's children are its fingers, and Wep_Root (the grip): that one isn't
    int grip = BoneByName(skeleton, "Wep_Root");
    for (int g = 0; g < FaceGroupCount; ++g)
      h.GroupSize[g] = 0;
    h.Count = 0;
    for (int i = 0; i < h.Bones; ++i)
    {
      int p = s.Parents[i];
      h.Parents[i] = (short)p;
      h.InHeld[i] = i == h.Root || (p >= 0 && p < i && h.InHeld[p]);
      h.Depth[i] = (unsigned char)(i == h.Root || p < 0 ? 0 : (h.Depth[p] < 250 ? h.Depth[p] + 1 : 250));
      h.Face[i] = i == face || (p >= 0 && h.Face[p]);
      h.Finger[i] = i != grip && p >= 0 && (p == leftHand || p == rightHand || h.Finger[p]);
      h.Helper[i] = IsHelperName(s.Names[i]);
      h.Eye[i] = !strcmp(s.Names[i], "LeftEye") || !strcmp(s.Names[i], "RightEye");
      // The body parts: one starts at each child of the held root (the spine,
      // each leg), at each shoulder, the neck, the face and each finger; any
      // other bone is in its parent's
      bool partStart = i == h.Root || p < 0 || p == h.Root || !strcmp(s.Names[i], "LeftShoulder") ||
        !strcmp(s.Names[i], "RightShoulder") || !strcmp(s.Names[i], "Neck") || i == face || (h.Finger[i] && !h.Finger[p]);
      h.Part[i] = (short)(partStart ? i : h.Part[p]);
      h.Group[i] = -1;
      h.FollowsGroup[i] = p >= 0 && (h.Group[p] >= 0 || h.FollowsGroup[p]);
      if (h.Face[i] && i != face && !h.FollowsGroup[i] && h.InHeld[i])
      {
        int g = FaceGroupOf(s.Names[i]);
        if (g >= 0 && h.GroupSize[g] < MaxGroupMembers)
        {
          h.Group[i] = (signed char)g;
          h.GroupMembers[g][h.GroupSize[g]++] = i;
        }
      }
      h.Map[i] = 0xFFFFFFFFu;
      if (h.InHeld[i])
      {
        h.Map[i] = (unsigned int)h.Count;
        h.List[h.Count++] = (unsigned int)i;
      }
    }
    if (!CopyFrom(body.Local, &h.Base[0][0], (size_t)h.Bones * FloatsPerBone))
    {
      h.Used = false;
      util::log::Warning("Pose: %s 0x%I64X: its local pose couldn't be read, not taken", kind, c.Entity);
      return;
    }
    for (int i = 0; i < h.Bones; ++i)
    {
      QNorm(h.Base[i] + 4);
      QIdentity(h.Turn[i]);
      h.Move[i][0] = h.Move[i][1] = h.Move[i][2] = 0.f;
    }
    __int64 allocator = (__int64)g_gameHandle + GameAllocatorRva;
    h.ListVector = { allocator, h.List, (unsigned int)h.Count, (unsigned int)h.Count };
    h.MapVector = { allocator, h.Map, (unsigned int)h.Bones, (unsigned int)h.Bones };
    for (int k = 0; k < 2; ++k)
    {
      h.Body[k] = { h.Buffer[k], &h.ListVector, &h.MapVector, 1.f, (unsigned int)h.Count, -1, 0 };
      h.Box[k] = { h.Buffer[k], &h.ListVector, &h.MapVector, 1.f, (unsigned int)h.Count, -1, 0 };
    }
    h.Next = 0;
    h.BodyTable = h.BoxTable = nullptr;
    h.HandedOk = false;
    h.WorldOk = false;
    h.Aside = h.Refused = h.BoxRefused = h.NoBoxLogged = false;
    h.Frames = h.Taken = h.FrozenFrames = h.FrozenTaken = 0;
    h.Worst = 0.f;
    h.CheckSince = GetTickCount64();
    h.CheckLine[0] = 0;
    h.Status[0] = 0;
    h.Undo.clear();
    h.SelfHit = -1;
    h.IdleRefOk = h.IdleActive = h.IdleGateOpen = h.IdleRootOk = false;
    memset(h.IdleRefTurn, 0, sizeof(h.IdleRefTurn));
    memset(h.IdleRefMove, 0, sizeof(h.IdleRefMove));
    h.IdleWeight = 0.f;
    h.IdleWeightAt = h.IdleAt = h.IdleNewAt = 0;
    h.IdleGen = h.IdleSeen = 0;
    h.IdleFrames = 0;
    h.Restoring = false;
    h.RestoreSince = 0;
    h.RestoreWaitLogged = false;
    // Its slots' records watched from its first blend (see BlendWatch)
    if (!Watch(h))
      util::log::Warning("Pose: the game's pose blend isn't hooked, so letting go won't put back what the game doesn't "
        "animate (free-moved bones stay moved)");
    sprintf_s(h.Label, "%s", kind);
    // Its weapons: each on the bone it's on now (the pose is the game's yet).
    // In the grip's hand if it's near the grip, else with the nearest body
    // bone (a holster), else left where the game has it.
    h.GripBone = grip;
    h.WeaponCount = 0;
    h.WeaponsCheckedAt = 0;
    char weaponText[200] = "";
    if (c.Kind == Kind_Person)
    {
      static float takeWorld[MaxBones][FloatsPerBone];
      bool worldOk = CopyFrom(body.World, &takeWorld[0][0], (size_t)h.Bones * FloatsPerBone);
      static HeldWeapon found[MaxWeapons];
      int count = worldOk ? FindWeapons(c.Entity, found, MaxWeapons) : 0;
      for (int k = 0; k < count; ++k)
      {
        HeldWeapon& w = found[k];
        WeaponName(w);
        float local[FloatsPerBone], world[FloatsPerBone];
        if (!WeaponPlace(w, c.Entity, local, world))
          continue;
        float nearest = FLT_MAX;
        if (grip >= 0 && h.InHeld[grip] && Distance3(takeWorld[grip], local) <= GripReach)
        {
          w.Bone = grip;
          nearest = Distance3(takeWorld[grip], local);
        }
        else
          for (int i = 0; i < h.Bones; ++i)
          {
            if (!h.InHeld[i] || h.Face[i] || h.Finger[i] || h.Helper[i])
              continue;
            float d = Distance3(takeWorld[i], local);
            if (d <= CarryReach && d < nearest)
            {
              nearest = d;
              w.Bone = i;
            }
          }
        char line[96];
        QIdentity(w.EditTurn);
        w.TakenBone = w.Bone;
        if (w.Bone >= 0)
        {
          Relative(takeWorld[w.Bone], local, w.OffsetLocal);
          Relative(takeWorld[w.Bone], world, w.OffsetWorld);
          memcpy(w.TakenOffsetLocal, w.OffsetLocal, sizeof(w.TakenOffsetLocal));
          memcpy(w.TakenOffsetWorld, w.OffsetWorld, sizeof(w.TakenOffsetWorld));
          sprintf_s(line, "%s%s on %s (%.0f cm)", weaponText[0] ? ", " : "", w.Name, s.Names[w.Bone], nearest * 100.f);
        }
        else
          sprintf_s(line, "%s%s left to the game (no bone within %.0f cm)", weaponText[0] ? ", " : "", w.Name,
            CarryReach * 100.f);
        strncat_s(weaponText, line, _TRUNCATE);
        h.Weapons[h.WeaponCount++] = w;
      }
    }
    g_Active = index;
    g_SelectedBone = h.Root;
    g_SelectedGroup = -1;
    g_SelectedWeapon = -1;
    char boxText[64] = "";
    if (h.HasBox)
      sprintf_s(boxText, ", box pose slot 0x%X", h.BoxHandle);
    else if (c.Kind == Kind_Faith)
      strcpy_s(boxText, ", her box pose not found");
    util::log::Write("Pose: holding %s 0x%I64X (%s, %d bones, %d held from %s; pose slot 0x%X%s%s%s)", h.Label, h.Entity,
      s.Asset, h.Bones, h.Count, s.Names[h.Root], h.Handle, boxText, weaponText[0] ? "; its weapons: " : "", weaponText);
  }

  // ---- Idle movement ----
  //
  // A held pose replaces the character's animation (weight 1), so its idle
  // (breathing, small shifts, blinks) is gone. The game blends the override in
  // 0x142FCF620 (rdx: the pose job's item; r8: + 0x20 its skeleton-bone ->
  // Ant-record map): for each listed bone (item + 0x158, count + 0x14C) it
  // reads Ant's record (item + 0xA0 + map[bone] x 0x30: scale, rotation at +
  // 0x10, translation at + 0x20; the local layout antFbUpdateLocalPose turns
  // into the local pose) and writes the blend back over it (in place: the
  // same records are copied into the step and made into the local pose after
  // it; offline, 2026-11-09). So at its entry those records are the animation
  // as Ant made it this frame, under the pose (with time running Ant rewrites
  // them before each pose job). A hook there copies them, for the held poses
  // that want it (their override's bone buffer, item + 0x130, is one of the
  // held pose's two; the body's slot, item + 0x150, as Faith's box pose shares
  // the buffers), leaving out what the game didn't write (see BlendWatch: the
  // same hook puts that back when a pose is let go). It takes register
  // arguments only.
  const __int64 OverrideBlendRva = 0x2FCF620;
  struct IdleCodeCheck
  {
    __int64 Rva;
    BYTE Bytes[48];
    size_t Size;
  };
  const IdleCodeCheck IdleCodeChecks[] = {
    // Its start: the handle's -1 check, the records' null check
    { 0x2FCF620, { 0x48, 0x89, 0xE0, 0x57, 0x41, 0x55, 0x48, 0x81, 0xEC, 0xE8, 0x00, 0x00, 0x00, 0x83, 0xBA, 0x50, 0x01,
      0x00, 0x00, 0xFF, 0x4D, 0x89, 0xC5, 0x48, 0x89, 0xD7, 0x0F, 0x84, 0xB2, 0x04, 0x00, 0x00, 0x48, 0x83, 0xBA, 0xA0,
      0x00, 0x00, 0x00, 0x00, 0x0F, 0x84, 0xA4, 0x04, 0x00, 0x00 }, 46 },
    // The loop: bones + 0x130, list + 0x158, weight + 0x148, list[n], map + 0x20
    { 0x2FCF70B, { 0x48, 0x8B, 0xAF, 0x30, 0x01, 0x00, 0x00, 0x48, 0x8B, 0x87, 0x58, 0x01, 0x00, 0x00, 0xF3, 0x0F, 0x10,
      0xB7, 0x48, 0x01, 0x00, 0x00, 0x44, 0x89, 0xF3, 0x45, 0x0F, 0x28, 0xEE, 0x0F, 0xC6, 0xF6, 0x00, 0x8B, 0x0C, 0x98,
      0x49, 0x8B, 0x45, 0x20 }, 40 },
    // map[bone], then x 3 x 2 x 8 = 0x30 a record, the records + 0xA0
    { 0x2FCF743, { 0x8B, 0x04, 0x88 }, 3 },
    { 0x2FCF760, { 0x48, 0x8D, 0x34, 0x40, 0x48, 0x8B, 0x87, 0xA0, 0x00, 0x00, 0x00 }, 11 },
    { 0x2FCF77A, { 0x48, 0x01, 0xF6 }, 3 },
    // The record's rotation (+ 0x10) and translation (+ 0x20) read
    { 0x2FCF790, { 0x0F, 0x28, 0x5C, 0xF0, 0x10 }, 5 },
    { 0x2FCFA44, { 0x44, 0x0F, 0x5C, 0x64, 0xF0, 0x20 }, 6 },
    // Its only call (in the pose job): rdx = the item, r8 = the map
    { 0x2FCC990, { 0x4C, 0x8B, 0x8D, 0x78, 0x01, 0x00, 0x00, 0x48, 0x8B, 0x8D, 0x50, 0x01, 0x00, 0x00, 0x4C, 0x8D, 0x44,
      0x24, 0x40, 0x48, 0x8D, 0x55, 0x90, 0xE8, 0x74, 0x2C, 0x00, 0x00 }, 28 },
    // The map (rsp + 0x40) filled before it: the manager, [+ 0xB0] (the bone ->
    // record map), [+ 0x190], [+ 8] (its bones), [+ 0x88] (its records)
    { 0x2FCC8EF, { 0x48, 0x8B, 0x83, 0xB0, 0x00, 0x00, 0x00, 0x48, 0x8D, 0x4D, 0x90, 0x48, 0x89, 0x5C, 0x24, 0x40, 0x48,
      0x89, 0x44, 0x24, 0x60, 0x8B, 0x83, 0x90, 0x01, 0x00, 0x00, 0x89, 0x44, 0x24, 0x68, 0x8B, 0x43, 0x08, 0x89, 0x44,
      0x24, 0x6C, 0x8B, 0x83, 0x88, 0x00, 0x00, 0x00, 0x89, 0x44, 0x24, 0x70 }, 48 },
    // The item's override part: + 0x130 null and + 0x150 -1 first
    { 0x2FCC935, { 0x66, 0x0F, 0x7F, 0x85, 0xC0, 0x00, 0x00, 0x00 }, 8 },
    { 0x2FCC981, { 0xC7, 0x85, 0xE0, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF }, 10 },
    // The item filled: the slot's own data into + 0 (its records at + 0xA0),
    // the override (if any) into + 0x130
    { 0x2FD7C88, { 0x4A, 0x8B, 0x14, 0x10, 0x48, 0x8B, 0x42, 0x28, 0x49, 0x8B, 0x14, 0x01, 0x49, 0x8D, 0x80, 0x80, 0x00,
      0x00, 0x00, 0x48, 0x83, 0xEA, 0x80, 0x0F, 0x10, 0x42, 0x80 }, 27 },
    { 0x2FD7D7A, { 0x49, 0x8B, 0x0C, 0x01, 0x48, 0x85, 0xC9, 0x74, 0x3A, 0x0F, 0x10, 0x01, 0x41, 0x0F, 0x11, 0x80, 0x30,
      0x01, 0x00, 0x00 }, 20 } };
  const int ItemBonesOffset = 0x130, ItemListOffset = 0x158, ItemCountOffset = 0x14C, ItemHandleOffset = 0x150;
  const int ItemRecordsOffset = 0xA0, MapRecordsOffset = 0x20, RecordBytes = 0x30;
  const int MapManagerOffset = 0x00, MapBonesOffset = 0x2C, MapRecordCountOffset = 0x30;

  struct IdleCapture
  {
    std::atomic<unsigned int> Gen;           // which wanting it is for (0: not wanted)
    std::atomic<unsigned int> Seq;           // odd while it's written
    std::atomic<unsigned int> Count;         // captures made
    unsigned int CapturedGen;                // the Gen the records were taken for (written inside Seq)
    float Records[MaxBones][12];             // per skeleton bone: scale, rotation, translation (Ant's)
    unsigned char Kept[MaxBones];            // per skeleton bone: 1 its rotation, 2 its translation not the game's (3: both, or not known)
  };
  IdleCapture g_IdleCaptures[MaxHeld];
  std::atomic<unsigned int> g_IdleGenNext{ 0 };
  typedef void(__fastcall* tOverrideBlend)(__int64, __int64, __int64, __int64);
  tOverrideBlend oOverrideBlend = nullptr;
  std::once_flag g_IdleHookOnce;
  bool g_IdleHooked = false;
  char g_IdleUnavailable[160] = "";

  // A channel (4 floats) bit for bit the same
  bool SameBits(float const* a, float const* b)
  {
    return memcmp(a, b, 4 * sizeof(float)) == 0;
  }

  // Inside the pose job, before our blend of the body's slot: the held
  // pose's records copied for its idle movement, each bone's rotation and
  // translation the game didn't write since the slot's last pass marked (they
  // hold the tools' own pose). With none written (the game's animation didn't
  // run since: time frozen), no capture. The game's memory is read before the
  // capture's seqlock opens (a fault can't leave it open).
  void CaptureIdle(IdleCapture& c, BlendWatch& w, BYTE const* records, unsigned int const* toRecord,
    unsigned int recordCount, int bones, bool seen)
  {
    unsigned int gen = c.Gen.load(std::memory_order_acquire);
    if (!gen)
      return;
    bool written = false;
    for (int n = 0; n < w.Count; ++n)
    {
      unsigned int bone = w.List[n];
      if (bone >= (unsigned int)bones || toRecord[bone] >= recordCount)
      {
        w.ScratchKept[bone] = 3;             // not known: no movement of its own
        continue;
      }
      float const* record = (float const*)(records + (size_t)toRecord[bone] * RecordBytes);
      memcpy(w.Scratch[bone], record, sizeof(w.Scratch[bone]));
      unsigned char kept = (unsigned char)(seen ? ((SameBits(record + 4, w.Seen[0][bone] + 4) ? 1 : 0) |
        (SameBits(record + 8, w.Seen[0][bone] + 8) ? 2 : 0)) : 0);
      w.ScratchKept[bone] = kept;
      written = written || kept != 3;
    }
    if (!written)
      return;
    c.Seq.fetch_add(1, std::memory_order_acq_rel);
    for (int n = 0; n < w.Count; ++n)
    {
      unsigned int bone = w.List[n];
      memcpy(c.Records[bone], w.Scratch[bone], sizeof(c.Records[bone]));
      c.Kept[bone] = w.ScratchKept[bone];
    }
    c.CapturedGen = gen;
    c.Seq.fetch_add(1, std::memory_order_release);
    c.Count.fetch_add(1, std::memory_order_release);
  }

  // A slot's pass with no override of ours: each held bone's channel that's
  // bit for bit as the slot's last pass left it (the game didn't write it
  // since) put back as the game last had it
  void PutBackChannels(BlendWatch& w, int k, BYTE* records, unsigned int const* toRecord, unsigned int recordCount,
    int bones)
  {
    int channels = 0, changed = 0;
    for (int n = 0; n < w.Count; ++n)
    {
      unsigned int bone = w.List[n];
      if (bone >= (unsigned int)bones || toRecord[bone] >= recordCount)
        continue;
      float* record = (float*)(records + (size_t)toRecord[bone] * RecordBytes);
      float const* seen = w.Seen[k][bone];
      float const* game = w.Game[k][bone];
      for (int part = 0; part < 12; part += 4)
      {
        if (!SameBits(record + part, seen + part) || SameBits(record + part, game + part))
          continue;
        float off = 0.f;
        for (int j = 0; j < 4; ++j)
        {
          float d = fabsf(record[part + j] - game[part + j]);
          off = (d > off || d != d) ? d : off;
        }
        memcpy(record + part, game + part, 4 * sizeof(float));
        ++channels;
        changed += !(off <= PutBackNoticed);
      }
    }
    w.Channels.fetch_add(channels, std::memory_order_relaxed);
    w.Changed.fetch_add(changed, std::memory_order_relaxed);
    w.Passes[k].fetch_add(1, std::memory_order_release);
  }

  // What a blend's pass is to a watch: 0 none of its slots', 2 one of its
  // slots' with our override, 3 one of its slots' without it (k: which slot;
  // AfterBlend follows both). Before the blend: what the game wrote since
  // the slot's last pass taken as its own; the idle capture (ours, the body);
  // the put back (not ours, if wanted)
  int SeeBlend(BlendWatch& w, int index, float const* overrideBones, int handle, BYTE* records, __int64 manager,
    unsigned int const* toRecord, unsigned int recordCount, int bones, int& k)
  {
    k = -1;
    bool ours = overrideBones && (overrideBones == w.Buffers[0] || overrideBones == w.Buffers[1]);
    if (ours)
    {
      k = handle == w.Handles[0] ? 0 : (w.Handles[1] != -1 && handle == w.Handles[1] ? 1 : -1);
      if (k < 0)
        return 0;
      __int64 known = w.Records[k].load(std::memory_order_relaxed);
      if (known && (known != (__int64)records || w.Manager[k].load(std::memory_order_relaxed) != manager))
      {
        // Its records somewhere else now: what was seen there doesn't hold here
        w.SeenOk[k].store(false, std::memory_order_release);
        w.RecordsMoved.fetch_add(1, std::memory_order_relaxed);
      }
      w.Records[k].store((__int64)records, std::memory_order_relaxed);
      w.Manager[k].store(manager, std::memory_order_relaxed);
      w.RecordCount[k].store(recordCount, std::memory_order_relaxed);
    }
    else
    {
      for (int s = 0; s < 2 && k < 0; ++s)
        if (w.SeenOk[s].load(std::memory_order_acquire) && w.Records[s].load(std::memory_order_relaxed) == (__int64)records &&
            w.Manager[s].load(std::memory_order_relaxed) == manager)
          k = s;
      if (k < 0)
        return 0;
    }
    bool seen = w.SeenOk[k].load(std::memory_order_acquire);
    if (!w.GameOk[k].load(std::memory_order_relaxed))
    {
      // The pose's first blend of the slot: the records are the game's own
      for (int n = 0; n < w.Count; ++n)
      {
        unsigned int bone = w.List[n];
        if (bone < (unsigned int)bones && toRecord[bone] < recordCount)
          memcpy(w.Game[k][bone], records + (size_t)toRecord[bone] * RecordBytes, sizeof(w.Game[k][bone]));
      }
      w.GameOk[k].store(true, std::memory_order_release);
    }
    else if (seen)
      for (int n = 0; n < w.Count; ++n)
      {
        unsigned int bone = w.List[n];
        if (bone >= (unsigned int)bones || toRecord[bone] >= recordCount)
          continue;
        float const* record = (float const*)(records + (size_t)toRecord[bone] * RecordBytes);
        for (int part = 0; part < 12; part += 4)
          if (!SameBits(record + part, w.Seen[k][bone] + part))
            memcpy(w.Game[k][bone] + part, record + part, 4 * sizeof(float));
      }
    if (ours && k == 0)
      CaptureIdle(g_IdleCaptures[index], w, records, toRecord, recordCount, bones, seen);
    if (!ours && seen && w.PutBack[k].load(std::memory_order_acquire))
      PutBackChannels(w, k, records, toRecord, recordCount, bones);
    return ours ? 2 : 3;
  }

  // Inside the pose job (a worker thread), before the game's blend: the watch
  // whose slot this pass is (still Busy: AfterBlend, then let go), or null
  BlendWatch* BeforeBlend(__int64 item, __int64 map, int& slot, BYTE*& records, unsigned int const*& toRecord,
    unsigned int& recordCount, int& bones)
  {
    float const* overrideBones = nullptr;
    int handle = -1;
    __int64 manager = 0;
    __try
    {
      overrideBones = *(float const* const*)(item + ItemBonesOffset);
      handle = *(int const*)(item + ItemHandleOffset);
      records = *(BYTE* const*)(item + ItemRecordsOffset);
      manager = *(__int64 const*)(map + MapManagerOffset);
      toRecord = *(unsigned int const* const*)(map + MapRecordsOffset);
      bones = *(int const*)(map + MapBonesOffset);
      recordCount = *(unsigned int const*)(map + MapRecordCountOffset);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      g_BlendFaults.fetch_add(1, std::memory_order_relaxed);
      return nullptr;
    }
    if (!records || !toRecord || manager < 0x10000 || bones <= 0 || bones > MaxBones || !recordCount ||
        recordCount > MaxRecords)
      return nullptr;
    for (int i = 0; i < MaxHeld; ++i)
    {
      BlendWatch& w = g_Watches[i];
      if (!w.On.load(std::memory_order_relaxed))
        continue;
      w.Busy.fetch_add(1, std::memory_order_seq_cst);
      int what = 0;
      if (w.On.load(std::memory_order_seq_cst))
      {
        __try
        {
          what = SeeBlend(w, i, overrideBones, handle, records, manager, toRecord, recordCount, bones, slot);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
          g_BlendFaults.fetch_add(1, std::memory_order_relaxed);
          // Its slot's Seen isn't what the pass left now
          for (int s = 0; s < 2; ++s)
            if (w.Records[s].load(std::memory_order_relaxed) == (__int64)records)
              w.SeenOk[s].store(false, std::memory_order_release);
          what = 0;
        }
      }
      if (what >= 2)
        return &w;
      w.Busy.fetch_sub(1, std::memory_order_seq_cst);
    }
    return nullptr;
  }

  // After the game's blend of a watched slot: the records as the pass left
  // them, for the next one
  void AfterBlend(BlendWatch& w, int k, BYTE const* records, unsigned int const* toRecord, unsigned int recordCount,
    int bones)
  {
    for (int n = 0; n < w.Count; ++n)
    {
      unsigned int bone = w.List[n];
      if (bone < (unsigned int)bones && toRecord[bone] < recordCount)
        memcpy(w.Seen[k][bone], records + (size_t)toRecord[bone] * RecordBytes, sizeof(w.Seen[k][bone]));
    }
    w.SeenOk[k].store(true, std::memory_order_release);
  }

  void __fastcall hOverrideBlend(__int64 a, __int64 item, __int64 map, __int64 d)
  {
    BlendWatch* watch = nullptr;
    int slot = -1, bones = 0;
    BYTE* records = nullptr;
    unsigned int const* toRecord = nullptr;
    unsigned int recordCount = 0;
    if (g_Watched.load(std::memory_order_acquire) > 0)
      watch = BeforeBlend(item, map, slot, records, toRecord, recordCount, bones);
    oOverrideBlend(a, item, map, d);
    if (watch)
    {
      __try
      {
        AfterBlend(*watch, slot, records, toRecord, recordCount, bones);
      }
      __except (EXCEPTION_EXECUTE_HANDLER)
      {
        g_BlendFaults.fetch_add(1, std::memory_order_relaxed);
        watch->SeenOk[slot].store(false, std::memory_order_release);   // not what the pass left now
      }
      watch->Busy.fetch_sub(1, std::memory_order_seq_cst);
    }
  }

  bool IdleCodeOk()
  {
    __int64 base = (__int64)g_gameHandle;
    __try
    {
      for (IdleCodeCheck const& check : IdleCodeChecks)
        if (memcmp((void const*)(base + check.Rva), check.Bytes, check.Size) != 0)
        {
          sprintf_s(g_IdleUnavailable, "Not available: the game's pose blend at 0x%I64X isn't what the tools know",
            0x140000000 + check.Rva);
          return false;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      strcpy_s(g_IdleUnavailable, "Not available: the game's pose blend couldn't be read");
      return false;
    }
    return true;
  }

  // Made the first time it's wanted (its code checked first). Game thread.
  bool BlendHook()
  {
    std::call_once(g_IdleHookOnce, []
    {
      if (!IdleCodeOk())
      {
        util::log::Warning("Pose: %s; Idle movement is off, and Let go can't put bones back", g_IdleUnavailable);
        return;
      }
      g_IdleHooked = util::hooks::CreateGameHook("PoseOverrideBlend", (__int64)g_gameHandle + OverrideBlendRva,
        hOverrideBlend, (LPVOID*)&oOverrideBlend);
      if (!g_IdleHooked)
        strcpy_s(g_IdleUnavailable, "Not available: the game's pose blend couldn't be hooked (see CT.log)");
      util::log::Write("Pose: the game's pose blend %s (idle movement, putting bones back on Let go)", g_IdleHooked ?
        "hooked" : "not hooked");
    });
    return g_IdleHooked;
  }

  void IdleUnwanted(int index)
  {
    g_IdleCaptures[index].Gen.store(0, std::memory_order_release);
  }

  void ForgetIdleRefs(Held& h)
  {
    h.IdleRefOk = false;
    memset(h.IdleRefTurn, 0, sizeof(h.IdleRefTurn));
    memset(h.IdleRefMove, 0, sizeof(h.IdleRefMove));
  }

  // Wanted for this pose; when it wasn't, a new wanting (its own Gen): only
  // captures made for it count, from scratch
  void IdleWant(Held& h, int index)
  {
    IdleCapture& c = g_IdleCaptures[index];
    if (c.Gen.load(std::memory_order_relaxed) != 0 && c.Gen.load(std::memory_order_relaxed) == h.IdleGen)
      return;
    c.Gen.store(0, std::memory_order_release);
    unsigned int gen = g_IdleGenNext.fetch_add(1, std::memory_order_relaxed) + 1;
    if (!gen)
      gen = g_IdleGenNext.fetch_add(1, std::memory_order_relaxed) + 1;
    h.IdleGen = gen;
    h.IdleSeen = c.Count.load(std::memory_order_acquire);
    ForgetIdleRefs(h);
    c.Gen.store(gen, std::memory_order_release);
  }

  // A turn's angle made amount times as big, then at most maxAngle (w kept >= 0)
  void ScaleTurn(float* q, float amount, float maxAngle)
  {
    if (q[3] < 0.f)
      for (int k = 0; k < 4; ++k)
        q[k] = -q[k];
    float s = sqrtf(q[0] * q[0] + q[1] * q[1] + q[2] * q[2]);
    if (!(s > 1e-7f))
    {
      QIdentity(q);
      return;
    }
    float angle = 2.f * atan2f(s, q[3]) * amount;
    if (angle > maxAngle)
      angle = maxAngle;
    float half = angle * 0.5f, k = sinf(half) / s;
    q[0] *= k; q[1] *= k; q[2] *= k;
    q[3] = cosf(half);
  }

  // A turn's angle (radians, 0 - pi)
  float TurnAngle(float const* q)
  {
    float s = sqrtf(q[0] * q[0] + q[1] * q[1] + q[2] * q[2]);
    return 2.f * atan2f(s, fabsf(q[3]));
  }

  // Each frame for a held pose: the animation the game made under it (the
  // capture), smoothed, and each held bone's movement about that. It's laid
  // over the pose by a weight that goes to 1 in IdleFadeSeconds when wanted,
  // with captures coming, and the character standing (no body bone more than
  // IdleGateTurn off where its animation has been, its root slower than
  // IdleGateSpeed), and to 0 the same way otherwise (unticked, Freeze time
  // with nothing new, walking, fighting).
  void UpdateIdle(Held& h, int index, ULONGLONG now)
  {
    float dtWeight = h.IdleWeightAt ? (float)(now - h.IdleWeightAt) / 1000.f : 0.f;
    h.IdleWeightAt = now;
    dtWeight = dtWeight < 0.f ? 0.f : (dtWeight > 0.5f ? 0.5f : dtWeight);
    auto fade = [&](bool up)
    {
      float step = dtWeight / IdleFadeSeconds;
      h.IdleWeight = up ? (h.IdleWeight + step > 1.f ? 1.f : h.IdleWeight + step) :
        (h.IdleWeight - step < 0.f ? 0.f : h.IdleWeight - step);
      if (h.IdleWeight <= 0.f)
        h.IdleActive = false;
    };
    bool want = g_IdleOn && g_IdleAmount > 0.f && BlendHook();
    if (!want)
    {
      // No more captures; the last movement fades out
      IdleUnwanted(index);
      h.IdleGen = 0;
      ForgetIdleRefs(h);
      fade(false);
      return;
    }
    IdleWant(h, index);
    IdleCapture& c = g_IdleCaptures[index];
    unsigned int count = c.Count.load(std::memory_order_acquire);
    if (count == h.IdleSeen)
    {
      // Nothing new: kept, fading out after a while (the game doesn't blend this pose now)
      fade(now - h.IdleNewAt < IdleStaleMs && h.IdleGateOpen);
      return;
    }
    unsigned int before = c.Seq.load(std::memory_order_acquire);
    if (before & 1u)
      return;
    static float records[MaxBones][12];
    static unsigned char kept[MaxBones];
    for (int n = 0; n < h.Count; ++n)
    {
      int i = (int)h.List[n];
      memcpy(records[i], c.Records[i], sizeof(records[i]));
      kept[i] = c.Kept[i];
    }
    unsigned int capturedGen = c.CapturedGen;
    std::atomic_thread_fence(std::memory_order_acquire);
    if (c.Seq.load(std::memory_order_relaxed) != before)
      return;                                // written meanwhile: next frame
    h.IdleSeen = count;
    if (capturedGen != h.IdleGen)
      return;                                // made for another pose, or before this wanting
    float dt = h.IdleRefOk ? (float)(now - h.IdleAt) / 1000.f : 0.f;
    h.IdleAt = now;
    h.IdleNewAt = now;
    float alpha = dt > 0.f ? 1.f - expf(-dt / IdleSmoothSeconds) : 0.f;
    float worst = 0.f;                       // the body's largest turn off where its animation has been
    for (int n = 0; n < h.Count; ++n)
    {
      int i = (int)h.List[n];
      float const* r = records[i];
      float q[4] = { r[4], r[5], r[6], r[7] }, t[3] = { r[8], r[9], r[10] };
      float length = q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3];
      if (!(length > 0.9f && length < 1.1f) || !std::isfinite(t[0]) || !std::isfinite(t[1]) || !std::isfinite(t[2]))
      {
        QIdentity(h.Idle[i]);
        h.Idle[i][4] = h.Idle[i][5] = h.Idle[i][6] = 0.f;
        h.IdleRefTurn[i] = h.IdleRefMove[i] = false;
        continue;
      }
      QNorm(q);
      float* ref = h.IdleRef[i];
      // A rotation or translation the game didn't write since the pose's last
      // blend holds the tools' own pose (with the idle movement in it), not the
      // game's animation: no movement of its own (it would feed on itself), and
      // its Ref left as it is
      bool turnKept = (kept[i] & 1) != 0, moveKept = (kept[i] & 2) != 0;
      float turn[4];
      QIdentity(turn);
      if (!turnKept)
      {
        if (!h.IdleRefTurn[i])
        {
          memcpy(ref, q, sizeof(q));
          h.IdleRefTurn[i] = true;
        }
        else
        {
          // Towards now, by the smoothing (the shorter way round)
          float dot = ref[0] * q[0] + ref[1] * q[1] + ref[2] * q[2] + ref[3] * q[3];
          float sign = dot < 0.f ? -1.f : 1.f;
          for (int k = 0; k < 4; ++k)
            ref[k] += (q[k] * sign - ref[k]) * alpha;
          QNorm(ref);
        }
        // Its movement: ref^-1 * now in the bone's own frame
        float inverse[4];
        QConj(ref, inverse);
        QMul(inverse, q, turn);
        QNorm(turn);
        if (!h.Face[i] && !h.Finger[i] && !h.Helper[i] && !h.Eye[i])
        {
          float angle = TurnAngle(turn);
          worst = angle > worst ? angle : worst;
        }
        ScaleTurn(turn, g_IdleAmount, MaxIdleTurn);
      }
      memcpy(h.Idle[i], turn, sizeof(turn));
      if (moveKept)
      {
        h.Idle[i][4] = h.Idle[i][5] = h.Idle[i][6] = 0.f;
        continue;
      }
      // The translation's, in its parent's frame
      if (!h.IdleRefMove[i])
      {
        memcpy(ref + 4, t, sizeof(t));
        h.IdleRefMove[i] = true;
      }
      else
        for (int k = 0; k < 3; ++k)
          ref[4 + k] += (t[k] - ref[4 + k]) * alpha;
      float move[3], length3 = 0.f;
      for (int k = 0; k < 3; ++k)
      {
        move[k] = (t[k] - ref[4 + k]) * g_IdleAmount;
        length3 += move[k] * move[k];
      }
      length3 = sqrtf(length3);
      float cut = length3 > MaxIdleMove ? MaxIdleMove / length3 : 1.f;
      for (int k = 0; k < 3; ++k)
        h.Idle[i][4 + k] = move[k] * cut;
    }
    // The character standing: its root (where it is: the bone the held root
    // hangs from, as the game drew it) slow, its body's bones near where
    // their animation has been
    int base = h.Root >= 0 && h.Root < h.Bones ? h.Parents[h.Root] : -1;
    float speed = 0.f;
    if (base >= 0 && h.WorldOk)
    {
      float const* at = h.World[base];
      if (h.IdleRootOk && dt > 0.f)
        speed = Distance3(at, h.IdleRoot) / dt;
      memcpy(h.IdleRoot, at, sizeof(h.IdleRoot));
      h.IdleRootOk = true;
    }
    h.IdleGateOpen = worst <= IdleGateTurn && speed <= IdleGateSpeed;
    h.IdleRefOk = true;
    h.IdleActive = true;
    fade(h.IdleGateOpen);
    ++h.IdleFrames;
  }

  // The idle movement laid over a bone's handed local pose, by its weight
  void AddIdle(Held const& h, int bone, float* local)
  {
    if (!h.IdleActive || !(h.IdleWeight > 0.f))
      return;
    float turn[4];
    memcpy(turn, h.Idle[bone], sizeof(turn));
    ScaleTurn(turn, h.IdleWeight, MaxIdleTurn);
    float turned[4];
    QMul(local + 4, turn, turned);
    QNorm(turned);
    memcpy(local + 4, turned, sizeof(turned));
    for (int k = 0; k < 3; ++k)
      local[k] += h.Idle[bone][4 + k] * h.IdleWeight;
  }

  // Every frame for a held pose: its slots found again, the check of what the
  // game made of last frame's pose, the world pose for the dots, and this
  // frame's pose handed. Game thread.
  void KeepHeld(Held& h, __int64 soldier, bool replayBusy, bool timeFrozen, ULONGLONG now)
  {
    LiveSlot body{}, box{};
    __int64 animatable = 0, boxAnimatable = 0;
    bool boxOk = false, ok;
    if (h.Kind == Kind_Faith)
      ok = h.Entity == soldier && FaithSlots(soldier, animatable, body, boxAnimatable, box, boxOk) &&
        animatable == h.Animatable;
    else
      ok = StillThere(h.Entity, h.VTable, h.BodyOffset, h.Animatable) && BodySlot(h.Entity, h.BodyOffset, animatable, body) &&
        animatable == h.Animatable;
    if (!ok || body.Manager != h.Manager || body.Handle != h.Handle || body.Bones != h.Bones)
    {
      Release(h, h.Kind == Kind_Faith && h.Entity != soldier ? "Faith is gone or new (a respawn or a level load)" :
        "its body or pose slot is gone or changed", true);
      return;
    }
    if (h.BodyTable && h.Body[h.Next ^ 1].Handle == -1)
    {
      // The game released the slot (it sets the override's handle to -1)
      Release(h, "the game released its pose slot", true);
      return;
    }
    bool boxSame = boxOk && h.HasBox && boxAnimatable == h.BoxAnimatable && box.Handle == h.BoxHandle;
    int index = (int)(&h - g_Held);
    if (h.Restoring)
    {
      KeepLettingGo(h, boxSame, now);
      return;
    }
    if (h.HasBox && !boxSame && h.BoxTable)
      TakeBack(h.Box, h.BoxTable, h.BoxTableHandle);

    // The game's world pose and local pose now, and each bone's world pose as
    // the game builds it: bone 0's, then every bone its parent's times its
    // local pose. The world pose's own entry for the root (bone 1,
    // AITrajectory) has another rotation, a quarter turn about up from the one
    // its children are built on (2026-11-08, live, Faith: Hips, CameraBase,
    // Connect and Trajectory 0.71 dot off it, Hips 10.6 cm; the chain of local
    // poses from bone 0 gave every other bone's world pose exactly). So the
    // chain is the frame the held bones hang from, not that entry.
    static float live[MaxBones][FloatsPerBone], liveLocal[MaxBones][FloatsPerBone], chain[MaxBones][FloatsPerBone];
    bool liveOk = CopyFrom(body.World, &live[0][0], (size_t)h.Bones * FloatsPerBone) &&
      CopyFrom(body.Local, &liveLocal[0][0], (size_t)h.Bones * FloatsPerBone);
    if (liveOk)
    {
      memcpy(chain[0], live[0], sizeof(chain[0]));
      for (int i = 1; i < h.Bones; ++i)
      {
        int p = h.Parents[i];
        if (p >= 0 && p < i)
          Compose(chain[p], liveLocal[i], chain[i]);
        else
          memcpy(chain[i], live[i], sizeof(chain[i]));
      }
    }

    // What the game made of the locals handed last frame: each held bone's
    // world pose its parent's (as the game builds it) times them
    if (liveOk && h.HandedOk && h.BodyTable)
    {
      float worst = 0.f;
      bool taken = true;
      for (int n = 0; n < h.Count; ++n)
      {
        int i = (int)h.List[n], p = h.Parents[i];
        if (p < 0)
          continue;
        float expected[FloatsPerBone];
        Compose(chain[p], h.Handed[i], expected);
        float distance = Distance3(expected, live[i]);
        float dot = fabsf(expected[4] * live[i][4] + expected[5] * live[i][5] + expected[6] * live[i][6] +
          expected[7] * live[i][7]);
        if (!(distance <= TakenDistance) || !(dot >= TakenDot))
          taken = false;
        if (distance > worst)
          worst = distance;
      }
      if (timeFrozen)
      {
        ++h.FrozenFrames;
        h.FrozenTaken += taken;
      }
      else
      {
        ++h.Frames;
        h.Taken += taken;
      }
      if (worst > h.Worst)
        h.Worst = worst;
    }
    if (now - h.CheckSince >= CheckLogMs)
    {
      if (h.Frames + h.FrozenFrames > 0)
      {
        sprintf_s(h.CheckLine, "The game took the pose in %d of %d frames with time running, %d of %d with time frozen "
          "(last %.0f s)", h.Taken, h.Frames, h.FrozenTaken, h.FrozenFrames, (now - h.CheckSince) / 1000.0);
        util::log::Write("Pose: %s 0x%I64X: the game took the pose in %d of %d frames with time running, %d of %d with "
          "time frozen; the most a held bone was off its parent times the pose: %.1f cm", h.Label, h.Entity, h.Taken,
          h.Frames, h.FrozenTaken, h.FrozenFrames, h.Worst * 100.f);
      }
      if (g_IdleOn)
        util::log::Write("Pose: %s 0x%I64X: idle movement from %d of the game's animation frames under the pose (last %.0f s)",
          h.Label, h.Entity, h.IdleFrames, (now - h.CheckSince) / 1000.0);
      h.IdleFrames = 0;
      for (int i = 0; i < h.WeaponCount; ++i)
      {
        HeldWeapon const& w = h.Weapons[i];
        if (w.Bone != -1 && w.GapOk && (w.Drawing || w.Holding))
          util::log::Write("Pose: %s 0x%I64X: its %s (%s%s): %s; since the tools' write the frame before, the game moved its "
            "local bone 1 %.1f cm and its bone 1's matrix %.1f cm", h.Label, h.Entity, w.Name,
            w.Bone == WorldFrame ? "put down" : "with ", w.Bone == WorldFrame ? "" : BoneName(h, w.Bone),
            w.Drawing ? "its body drawn by the tools" : (w.Holding ? "only its entity moved" : "not moved by the tools now"),
            w.Gap * 100.f, w.MatrixGap * 100.f);
      }
      h.Frames = h.Taken = h.FrozenFrames = h.FrozenTaken = 0;
      h.Worst = 0.f;
      h.CheckSince = now;
    }

    // This frame's pose: the world pose for the dots and the rings (the
    // game's chain, the held bones from their parents and the pose) and the
    // locals to hand
    float* buffer = h.Buffer[h.Next];
    if (liveOk)
      memcpy(h.World, chain, (size_t)h.Bones * FloatsPerBone * sizeof(float));
    UpdateIdle(h, (int)(&h - g_Held), now);
    for (int n = 0; n < h.Count; ++n)
    {
      int i = (int)h.List[n], p = h.Parents[i];
      float local[FloatsPerBone];
      EditedLocal(h, i, local);
      AddIdle(h, i, local);
      memcpy(buffer + n * FloatsPerBone, local, sizeof(local));
      memcpy(h.Handed[i], local, sizeof(local));
      if (liveOk && p >= 0)
        Compose(h.World[p], local, h.World[i]);
    }
    h.WorldOk = liveOk;

    // Steps aside while a replay records or plays (it hands its own override
    // to people and Faith)
    if (replayBusy)
    {
      if (h.BodyTable || h.BoxTable)
        util::log::Write("Pose: %s 0x%I64X steps aside while a replay records or plays (the pose is kept)", h.Label,
          h.Entity);
      TakeBack(h.Body, h.BodyTable, h.BodyTableHandle);
      TakeBack(h.Box, h.BoxTable, h.BoxTableHandle);
      // Meanwhile the game's own animation, all of it (see BlendWatch)
      SetPutBack(index, 0, true);
      SetPutBack(index, 1, true);
      LetGoWeapons(h, true);
      h.HandedOk = false;
      h.Aside = true;
      strcpy_s(h.Status, "Steps aside while a replay records or plays (the pose is kept)");
      return;
    }
    if (h.Aside)
      util::log::Write("Pose: %s 0x%I64X held again", h.Label, h.Entity);
    h.Aside = false;

    int k = h.Next;
    h.Body[k] = { buffer, &h.ListVector, &h.MapVector, 1.f, (unsigned int)h.Count, (int)body.Handle, 0 };
    bool bodyIn = HandTo(h.Body, k, body, h.BodyTable, h.BodyTableHandle);
    if (!bodyIn)
    {
      if (!h.Refused)
        util::log::Warning("Pose: %s 0x%I64X: its pose slot 0x%X holds another override now (a replay's or a Dash "
          "ghost's): the pose waits", h.Label, h.Entity, body.Handle);
      h.Refused = true;
      h.HandedOk = false;
      SetPutBack(index, 0, true);
      LetGoWeapons(h, true);
      strcpy_s(h.Status, "Its pose slot holds another override (a replay's or a Dash ghost's): the pose waits");
      return;
    }
    h.Refused = false;
    bool boxIn = false;
    if (boxSame)
    {
      h.Box[k] = { buffer, &h.ListVector, &h.MapVector, 1.f, (unsigned int)h.Count, (int)box.Handle, 0 };
      boxIn = HandTo(h.Box, k, box, h.BoxTable, h.BoxTableHandle);
      if (!boxIn && !h.BoxRefused)
        util::log::Warning("Pose: %s 0x%I64X: its box pose's slot 0x%X holds another override", h.Label, h.Entity,
          box.Handle);
      h.BoxRefused = !boxIn;
    }
    else if (h.HasBox && !h.NoBoxLogged)
    {
      util::log::Warning("Pose: %s 0x%I64X: its box pose isn't found any more: only the body is posed (it may be culled "
        "by its unposed box)", h.Label, h.Entity);
      h.NoBoxLogged = true;
    }
    h.Next ^= 1;
    h.HandedOk = true;
    SetPutBack(index, 0, false);
    SetPutBack(index, 1, !boxIn);
    // Its weapons with the posed bones
    if (!g_WeaponsFollow)
      LetGoWeapons(h, true);
    else if (h.WorldOk)
      KeepWeapons(h, now, chain, timeFrozen);
    int animated = AnimatedByGame(h.Animatable);
    sprintf_s(h.Status, "Held%s%s", h.HasBox ? (boxIn ? " (and its box pose)" : " (not its box pose)") : "",
      animated == 0 ? ". The game doesn't animate its body now, so it doesn't take the pose" : "");
  }

  // The drones within DroneScanRadius of the camera with a body of their own
  // (+ 0x100), into out (their entity, animatable and distance); how many of
  // them there are at all, how many with a body, and the first body's bones
  int WalkDrones(float const* camera, Candidate* out, int max, int& drones, int& withBody, int& firstBones)
  {
    __int64 base = (__int64)g_gameHandle;
    __int64 typeInfo = base + DroneTypeInfoRva;
    int count = 0;
    drones = withBody = firstBones = 0;
    if (!TypeNameIs(typeInfo, "PamClientDroneActorEntity"))
      return 0;
    __try
    {
      for (int list = 0; list < 4; ++list)
      {
        __int64 link = *(__int64*)(typeInfo + TypeInstanceListsOffset + list * 0x10);
        for (int n = 0; link && n < MaxPerList; ++n, link = *(__int64*)link)
        {
          __int64 entity = link - EntityLinkOffset;
          if (TypeOf(entity) != typeInfo)
            continue;
          float place[3];
          if (!EntityPlace(entity, place) || !(Distance3(place, camera) <= DroneScanRadius))
            continue;
          ++drones;
          __int64 animatable = *(__int64*)(entity + ActorBodyOffset);
          LiveSlot slot{};
          if (!SlotOf(animatable, 0, slot))
            continue;
          if (!withBody++)
            firstBones = slot.Bones;
          if (count < max)
          {
            out[count] = Candidate{};
            out[count].Entity = entity;
            out[count].Animatable = animatable;
            out[count].Distance = Distance3(place, camera);
            ++count;
          }
        }
      }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
    return count;
  }

  // People, drones and Faith that can be posed, nearest first. Game thread.
  void ScanCandidates(__int64 soldier, ULONGLONG now)
  {
    std::vector<Candidate> found;
    float camera[3] = { 0.f, 0.f, 0.f }, look[3] = { 0.f, 0.f, 1.f };
    bool haveCamera = false;
    {
      std::lock_guard<std::mutex> lock(g_ViewMutex);
      if (g_View.Ok)
      {
        camera[0] = g_View.T[12]; camera[1] = g_View.T[13]; camera[2] = g_View.T[14];
        look[0] = -g_View.T[8]; look[1] = -g_View.T[9]; look[2] = -g_View.T[10];
        haveCamera = true;
      }
    }
    float faithPlace[3];
    bool faithThere = soldier >= 0x10000 && EntityPlace(soldier, faithPlace);
    if (!haveCamera && faithThere)
    {
      memcpy(camera, faithPlace, sizeof(camera));
      haveCamera = true;
    }
    auto addOne = [&](int kind, __int64 entity, __int64 animatable, LiveSlot const& slot, float distance)
    {
      Candidate c{};
      c.Entity = entity;
      c.VTable = ReadVTable(entity);
      c.Animatable = animatable;
      c.Kind = kind;
      c.Distance = distance;
      c.Bones = slot.Bones;
      c.Skeleton = MatchSkeleton(slot.Bones, slot.Parents);
      c.Animated = AnimatedByGame(animatable);
      __int64 entry = OverrideIn(slot);
      c.OtherOverride = entry != 0;
      for (int i = 0; i < MaxHeld; ++i)
        if (g_Held[i].Used && g_Held[i].Entity == entity)
        {
          c.Held = true;
          c.LettingGo = g_Held[i].Restoring;
          if (IsOurs(g_Held[i].Body, entry))
            c.OtherOverride = false;
        }
      found.push_back(c);
    };

    // Faith
    if (soldier >= 0x10000)
    {
      __int64 animatable = 0, boxAnimatable = 0;
      LiveSlot body{}, box{};
      bool boxOk = false;
      if (FaithSlots(soldier, animatable, body, boxAnimatable, box, boxOk))
        addOne(Kind_Faith, soldier, animatable, body, faithThere && haveCamera ? Distance3(faithPlace, camera) : 0.f);
    }

    // People: the attach scan's characters (not swarm members)
    if (haveCamera)
    {
      if (!g_ClassesFound)
      {
        bool fromFaith = false;
        g_ClassesFound = attach::FindClasses(soldier, fromFaith);
        if (!g_ClassesFound && !g_ClassesLogged)
        {
          util::log::Warning("Pose: the game's entity classes weren't found: only Faith and drones are listed");
          g_ClassesLogged = true;
        }
      }
      if (g_ClassesFound)
      {
        if (!g_Scan)
          g_Scan = std::make_unique<attach::Scan>();
        g_Found.resize(256);
        attach::View view{};
        view.Camera = DirectX::XMFLOAT3(camera[0], camera[1], camera[2]);
        Normalize3(look);
        view.Look = DirectX::XMFLOAT3(look[0], look[1], look[2]);
        view.Skip = soldier;
        int count = attach::ScanNearby(view, ScanRadius, g_Found.data(), (int)g_Found.size(), *g_Scan);
        for (int n = 0; n < count && (int)found.size() < MaxCandidates; ++n)
        {
          attach::Target const& target = g_Found[n];
          if (target.Agent >= 0 || target.Entity == soldier || strcmp(attach::KindOf(target), "character") != 0)
            continue;
          __int64 animatable = 0;
          LiveSlot slot{};
          if (!BodySlot(target.Entity, CharacterBodyOffset, animatable, slot))
            continue;
          float place[3];
          float distance = EntityPlace(target.Entity, place) ? Distance3(place, camera) : target.Distance;
          addOne(Kind_Person, target.Entity, animatable, slot, distance);
        }
      }

      // Drones: whether they have a body of their own wasn't known (the log
      // says, once)
      int drones = 0, withBody = 0, firstBones = 0;
      static Candidate droneFound[MaxCandidates];
      int droneCount = WalkDrones(camera, droneFound, MaxCandidates, drones, withBody, firstBones);
      for (int n = 0; n < droneCount && (int)found.size() < MaxCandidates; ++n)
      {
        Candidate& c = droneFound[n];
        LiveSlot slot{};
        __int64 animatable = 0;
        if (!BodySlot(c.Entity, ActorBodyOffset, animatable, slot) || animatable != c.Animatable)
          continue;
        addOne(Kind_Drone, c.Entity, animatable, slot, c.Distance);
      }
      if (drones > 0 && !g_DronesLogged)
      {
        util::log::Write("Pose: %d drones within %.0f m of the camera, %d of them with an Ant body at + 0x100%s", drones,
          DroneScanRadius, withBody, withBody ? "" : " (so drones can't be posed this way)");
        if (withBody)
          util::log::Write("Pose:   the first drone's body has %d bones", firstBones);
        g_DronesLogged = true;
      }
    }
    std::stable_sort(found.begin(), found.end(), [](Candidate const& a, Candidate const& b)
    {
      if ((a.Kind == Kind_Faith) != (b.Kind == Kind_Faith))
        return a.Kind == Kind_Faith;
      return a.Distance < b.Distance;
    });
    g_Candidates.swap(found);
    g_ScannedAt = now;
  }

  // ---- Undo ----

  Snapshot SnapshotOf(Held const& h)
  {
    Snapshot s;
    s.Turns.assign(&h.Turn[0][0], &h.Turn[0][0] + (size_t)h.Bones * 4);
    s.Moves.assign(&h.Move[0][0], &h.Move[0][0] + (size_t)h.Bones * 3);
    for (int i = 0; i < h.WeaponCount; ++i)
    {
      HeldWeapon const& w = h.Weapons[i];
      s.Weapons.push_back((float)w.Bone);
      s.Weapons.insert(s.Weapons.end(), w.OffsetLocal, w.OffsetLocal + FloatsPerBone);
      s.Weapons.insert(s.Weapons.end(), w.OffsetWorld, w.OffsetWorld + FloatsPerBone);
      s.Weapons.insert(s.Weapons.end(), w.EditMove, w.EditMove + 3);
      s.Weapons.insert(s.Weapons.end(), w.EditTurn, w.EditTurn + 4);
    }
    return s;
  }

  void PushUndo(Held& h, Snapshot const& s)
  {
    h.Undo.push_back(s);
    if (h.Undo.size() > MaxUndo)
      h.Undo.erase(h.Undo.begin());
  }

  void ApplySnapshot(Held& h, Snapshot const& s)
  {
    if (s.Turns.size() == (size_t)h.Bones * 4)
      memcpy(&h.Turn[0][0], s.Turns.data(), s.Turns.size() * sizeof(float));
    if (s.Moves.size() == (size_t)h.Bones * 3)
      memcpy(&h.Move[0][0], s.Moves.data(), s.Moves.size() * sizeof(float));
    if (s.Weapons.size() == (size_t)h.WeaponCount * WeaponSnapshotFloats)
      for (int i = 0; i < h.WeaponCount; ++i)
      {
        HeldWeapon& w = h.Weapons[i];
        float const* f = s.Weapons.data() + (size_t)i * WeaponSnapshotFloats;
        int bone = (int)f[0];
        // One the game took back (dropped or gone) stays the game's
        if (w.Gone && bone != -1)
          continue;
        w.Bone = bone;
        memcpy(w.OffsetLocal, f + 1, FloatsPerBone * sizeof(float));
        memcpy(w.OffsetWorld, f + 9, FloatsPerBone * sizeof(float));
        memcpy(w.EditMove, f + 17, 3 * sizeof(float));
        memcpy(w.EditTurn, f + 20, 4 * sizeof(float));
      }
  }

  // How far a bone can be moved: the held root (the whole body) as far as
  // anyone would go (the user, 2026-11-08: "remove the limitation ... make it
  // infinite"): 100 km (the city is a few km across), so no sum overflows;
  // a face bone MaxFaceMove, any other (Free move) MaxFreeMove
  const float MaxRootMove = 1e5f;
  float MoveLimit(Held const& h, int bone)
  {
    return bone == h.Root ? MaxRootMove : (h.Face[bone] ? MaxFaceMove : MaxFreeMove);
  }

  // Within its limit; a move that isn't a number (or is endless) is none
  void ClampMove(Held& h, int bone)
  {
    float limit = MoveLimit(h, bone);
    for (int k = 0; k < 3; ++k)
    {
      float& m = h.Move[bone][k];
      if (!std::isfinite(m))
        m = 0.f;
      m = m < -limit ? -limit : (m > limit ? limit : m);
    }
  }

  bool IsEdited(Held const& h, int bone)
  {
    return !QIsIdentity(h.Turn[bone]) || h.Move[bone][0] != 0.f || h.Move[bone][1] != 0.f || h.Move[bone][2] != 0.f;
  }

  void ResetBone(Held& h, int bone)
  {
    QIdentity(h.Turn[bone]);
    h.Move[bone][0] = h.Move[bone][1] = h.Move[bone][2] = 0.f;
  }

  // Shown on the picture and in the list (no search): its kind is shown, and
  // with the face grouped, a face bone that's in a group or below one isn't
  // (its group is)
  bool BoneShown(Held const& h, int bone)
  {
    if (h.Face[bone] && !g_ShowFace)
      return false;
    if (h.Finger[bone] && !g_ShowFingers)
      return false;
    if (h.Helper[bone] && !g_ShowHelpers)
      return false;
    if (g_GroupFace && (h.Group[bone] >= 0 || h.FollowsGroup[bone]))
      return false;
    return true;
  }

  bool GroupShown(Held const& h, int group)
  {
    return g_ShowFace && g_GroupFace && group >= 0 && group < FaceGroupCount && h.GroupSize[group] > 0;
  }

  bool GroupEdited(Held const& h, int group)
  {
    for (int k = 0; k < h.GroupSize[group]; ++k)
      if (IsEdited(h, h.GroupMembers[group][k]))
        return true;
    return false;
  }

  // The middle of a group's members (world places in w)
  void GroupMiddle(Held const& h, int group, float (*w)[FloatsPerBone], float* out)
  {
    out[0] = out[1] = out[2] = 0.f;
    int n = h.GroupSize[group];
    for (int k = 0; k < n; ++k)
      for (int a = 0; a < 3; ++a)
        out[a] += w[h.GroupMembers[group][k]][a];
    if (n > 0)
      for (int a = 0; a < 3; ++a)
        out[a] /= (float)n;
  }

  // ---- Saved poses: Cinematic Tools\Poses\<name>.txt, a held bone a line:
  // its name, its local rotation (x y z w) and translation (x y z), as the
  // pose has them ----

  void ListPoseFiles()
  {
    g_PoseFiles.clear();
    WIN32_FIND_DATAA data;
    char pattern[MAX_PATH];
    sprintf_s(pattern, "%s\\*.txt", PoseFolder);
    HANDLE find = FindFirstFileA(pattern, &data);
    if (find != INVALID_HANDLE_VALUE)
    {
      do
      {
        if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        {
          std::string name = data.cFileName;
          if (name.size() > 4)
            g_PoseFiles.push_back(name.substr(0, name.size() - 4));
        }
      } while (FindNextFileA(find, &data));
      FindClose(find);
    }
    std::sort(g_PoseFiles.begin(), g_PoseFiles.end());
  }

  bool CleanName(char const* in, char* out, size_t size)
  {
    size_t n = 0;
    for (char const* p = in; *p && n + 1 < size; ++p)
    {
      char c = *p;
      if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' || c == '-' || c == '_')
        out[n++] = c;
    }
    while (n > 0 && out[n - 1] == ' ')
      --n;
    out[n] = 0;
    return n > 0;
  }

  void SavePose(Held const& h, char const* name)
  {
    char clean[64];
    if (!CleanName(name, clean, sizeof(clean)))
    {
      strcpy_s(g_FileMessage, "Give the pose a name first (letters, digits, spaces, - and _)");
      return;
    }
    CreateDirectoryA("Cinematic Tools", nullptr);
    CreateDirectoryA(PoseFolder, nullptr);
    char path[MAX_PATH];
    sprintf_s(path, "%s\\%s.txt", PoseFolder, clean);
    FILE* f = nullptr;
    if (fopen_s(&f, path, "w") || !f)
    {
      sprintf_s(g_FileMessage, "%s couldn't be written", path);
      return;
    }
    fprintf(f, "# MEC Cinematic Tools pose, %s, %s; each held bone: name, local rotation x y z w, translation x y z\n",
      h.Label, pose_skeletons::Skeletons[h.Skeleton].Asset);
    for (int n = 0; n < h.Count; ++n)
    {
      int i = (int)h.List[n];
      float local[FloatsPerBone];
      EditedLocal(h, i, local);
      fprintf(f, "%s %.7g %.7g %.7g %.7g %.7g %.7g %.7g\n", BoneName(h, i), local[4], local[5], local[6], local[7], local[0],
        local[1], local[2]);
    }
    fclose(f);
    sprintf_s(g_FileMessage, "Saved %s (%d bones)", path, h.Count);
    util::log::Write("Pose: %s saved from %s 0x%I64X (%d bones)", path, h.Label, h.Entity, h.Count);
    g_FilesListedAt = 0;
  }

  // Each saved bone with the same name in the held skeleton gets the saved
  // rotation; the held root its translation, and every bone its translation
  // when the pose was saved from the same skeleton (another skeleton's bones
  // are other lengths)
  void LoadPose(Held& h, char const* name)
  {
    char path[MAX_PATH];
    sprintf_s(path, "%s\\%s.txt", PoseFolder, name);
    FILE* f = nullptr;
    if (fopen_s(&f, path, "r") || !f)
    {
      sprintf_s(g_FileMessage, "%s couldn't be read", path);
      return;
    }
    Snapshot before = SnapshotOf(h);
    int set = 0, notHere = 0;
    bool sameSkeleton = false;
    char line[512];
    while (fgets(line, sizeof(line), f))
    {
      if (line[0] == '#')
      {
        // "# MEC Cinematic Tools pose, <who>, <its SkeletonAsset>; ..."
        char const* asset = pose_skeletons::Skeletons[h.Skeleton].Asset;
        char const* at = strstr(line, asset);
        size_t length = strlen(asset);
        sameSkeleton |= at && (at[length] == ';' || at[length] == ',' || at[length] == '\n');
        continue;
      }
      char bone[128];
      float q[4], t[3];
      if (sscanf_s(line, "%127s %f %f %f %f %f %f %f", bone, (unsigned)sizeof(bone), &q[0], &q[1], &q[2], &q[3], &t[0], &t[1],
          &t[2]) != 8)
        continue;
      int i = BoneByName(h.Skeleton, bone);
      if (i < 0 || !h.InHeld[i])
      {
        ++notHere;
        continue;
      }
      QNorm(q);
      // local = turn * base, so turn = saved * base^-1
      float inverse[4];
      QConj(h.Base[i] + 4, inverse);
      QMul(q, inverse, h.Turn[i]);
      QNorm(h.Turn[i]);
      if (i == h.Root || sameSkeleton)
      {
        for (int k = 0; k < 3; ++k)
          h.Move[i][k] = t[k] - h.Base[i][k];
        ClampMove(h, i);
      }
      else
        h.Move[i][0] = h.Move[i][1] = h.Move[i][2] = 0.f;
      ++set;
    }
    fclose(f);
    if (set > 0)
      PushUndo(h, before);
    sprintf_s(g_FileMessage, "Loaded %s: %d bones set", name, set);
    if (notHere)
      sprintf_s(g_FileMessage + strlen(g_FileMessage), sizeof(g_FileMessage) - strlen(g_FileMessage),
        ", %d not in this skeleton's held bones", notHere);
    util::log::Write("Pose: %s loaded onto %s 0x%I64X: %d bones set, %d not in its held bones", path, h.Label, h.Entity, set,
      notHere);
  }

  // ---- The turning rings ----

  const int RingSegments = 48;
  const float RingPixels = 4.5f;       // font sizes: a ring's radius on the screen
  const float RingReach = 0.4f;        // font sizes: how near the cursor has to be

  struct Ring
  {
    float Axis[3], U[3], V[3];
    ImU32 Colour;
    ImVec2 Points[RingSegments + 1];
    bool Shown[RingSegments + 1];
  };

  // Rings about X, Y and Z and the view's axis, round a bone: the bone's own
  // axes (its world rotation, boneRotation), or the world's (null)
  int MakeRings(View const& view, float const* centre, float const* boneRotation, Ring* rings, float& radius)
  {
    float const* t = view.T;
    float offset[3] = { centre[0] - t[12], centre[1] - t[13], centre[2] - t[14] };
    float depth = -(offset[0] * t[8] + offset[1] * t[9] + offset[2] * t[10]);
    ImVec2 size = ImGui::GetIO().DisplaySize;
    if (depth < 0.2f || view.P[0] == 0.f || size.x <= 0.f)
      return 0;
    radius = depth * (RingPixels * ImGui::GetFontSize()) / (fabsf(view.P[0]) * size.x * 0.5f);
    float axes[3][3] = { { 1.f, 0.f, 0.f }, { 0.f, 1.f, 0.f }, { 0.f, 0.f, 1.f } };
    if (boneRotation)
    {
      float q[4];
      memcpy(q, boneRotation, sizeof(q));
      QNorm(q);
      for (int a = 0; a < 3; ++a)
      {
        float unit[3] = { a == 0 ? 1.f : 0.f, a == 1 ? 1.f : 0.f, a == 2 ? 1.f : 0.f };
        QRot(q, unit, axes[a]);
        Normalize3(axes[a]);
      }
    }
    const ImU32 colours[4] = { IM_COL32(235, 80, 80, 230), IM_COL32(90, 210, 90, 230), IM_COL32(90, 140, 255, 230),
      IM_COL32(255, 255, 255, 200) };
    for (int r = 0; r < 4; ++r)
    {
      Ring& ring = rings[r];
      if (r < 3)
      {
        // u, v with u x v = the axis
        int a = (r + 1) % 3, b = (r + 2) % 3;
        for (int k = 0; k < 3; ++k)
        {
          ring.U[k] = axes[a][k];
          ring.V[k] = axes[b][k];
        }
      }
      else
      {
        // The view's own: the camera's right and up (the ring a bit bigger)
        for (int k = 0; k < 3; ++k)
        {
          ring.U[k] = t[k] * 1.15f;
          ring.V[k] = t[4 + k] * 1.15f;
        }
      }
      Cross(ring.U, ring.V, ring.Axis);
      Normalize3(ring.Axis);
      ring.Colour = colours[r];
      for (int s = 0; s <= RingSegments; ++s)
      {
        float a = 6.2831853f * s / RingSegments;
        float p[3];
        for (int k = 0; k < 3; ++k)
          p[k] = centre[k] + radius * (cosf(a) * ring.U[k] + sinf(a) * ring.V[k]);
        float x = 0.f, y = 0.f;
        ring.Shown[s] = ToScreen(view, p, x, y);
        ring.Points[s] = ImVec2(x, y);
      }
    }
    return 4;
  }

  // How near a point is to a segment, in pixels
  float SegmentDistance(ImVec2 p, ImVec2 a, ImVec2 b, float& along)
  {
    float dx = b.x - a.x, dy = b.y - a.y;
    float lengthSq = dx * dx + dy * dy;
    along = lengthSq > 1e-6f ? ((p.x - a.x) * dx + (p.y - a.y) * dy) / lengthSq : 0.f;
    along = along < 0.f ? 0.f : (along > 1.f ? 1.f : along);
    float cx = a.x + dx * along - p.x, cy = a.y + dy * along - p.y;
    return sqrtf(cx * cx + cy * cy);
  }

  // ---- Moving a bone with the mouse ----

  // The world pose with the held bones from the pose as it is now (the rest
  // from this frame's chain, h.World)
  void ComposeHeld(Held const& h, float (*out)[FloatsPerBone])
  {
    memcpy(out, h.World, (size_t)h.Bones * FloatsPerBone * sizeof(float));
    for (int n = 0; n < h.Count; ++n)
    {
      int i = (int)h.List[n], p = h.Parents[i];
      if (p < 0)
        continue;
      float local[FloatsPerBone];
      EditedLocal(h, i, local);
      Compose(out[p], local, out[i]);
    }
  }

  // The bone turned by q in the world (about its own place): its turn becomes
  // parent^-1 * q * parent * its turn (world is the pose's world now)
  void ApplyWorldTurn(Held& h, int bone, float const* q, float (*world)[FloatsPerBone])
  {
    int p = h.Parents[bone];
    float parent[4] = { 0.f, 0.f, 0.f, 1.f };
    if (p >= 0)
      memcpy(parent, world[p] + 4, sizeof(parent));
    QNorm(parent);
    float inverse[4], a[4], b[4], turn[4];
    QConj(parent, inverse);
    QMul(inverse, q, a);
    QMul(a, parent, b);
    QMul(b, h.Turn[bone], turn);
    QNorm(turn);
    memcpy(h.Turn[bone], turn, sizeof(turn));
  }

  // The shortest turn from one unit vector to another
  void FromTo(float const* u, float const* v, float* q)
  {
    float d = u[0] * v[0] + u[1] * v[1] + u[2] * v[2];
    if (d < -0.99999f)
    {
      float other[3] = { fabsf(u[0]) > 0.9f ? 0.f : 1.f, fabsf(u[0]) > 0.9f ? 1.f : 0.f, 0.f };
      float axis[3];
      Cross(u, other, axis);
      Normalize3(axis);
      QAxis(axis, 3.14159265f, q);
      return;
    }
    float c[3];
    Cross(u, v, c);
    q[0] = c[0]; q[1] = c[1]; q[2] = c[2]; q[3] = 1.f + d;
    QNorm(q);
  }

  float Length3(float const* v)
  {
    return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  }

  // A unit vector at right angles to v
  void AnyPerpendicular(float const* v, float* out)
  {
    float other[3] = { fabsf(v[0]) > 0.9f ? 0.f : 1.f, fabsf(v[0]) > 0.9f ? 1.f : 0.f, 0.f };
    Cross(v, other, out);
    Normalize3(out);
  }

  // Where the point under the screen spot x, y is, on the plane across the
  // picture through base (the camera's right and up): Newton steps on
  // ToScreen, as the lights' dots are dragged (LightManager.cpp DragPlace),
  // so it's where the dot is drawn (the Panini view too). False behind the
  // camera.
  bool PlanePoint(View const& view, float const* base, float x, float y, float* out)
  {
    float const* t = view.T;
    float depth = -((base[0] - t[12]) * t[8] + (base[1] - t[13]) * t[9] + (base[2] - t[14]) * t[10]);
    if (depth < 0.1f)
      return false;
    float axisA[3] = { t[0], t[1], t[2] }, axisB[3] = { t[4], t[5], t[6] };
    Normalize3(axisA);
    Normalize3(axisB);
    float step = depth * 0.001f;
    float a = 0.f, b = 0.f, sx, sy;
    if (!ToScreen(view, base, sx, sy))
      return false;
    float error = (x - sx) * (x - sx) + (y - sy) * (y - sy);
    for (int i = 0; i < 16 && error > 0.0025f; ++i)
    {
      float v[3], pa[3], pb[3];
      for (int k = 0; k < 3; ++k)
      {
        v[k] = base[k] + axisA[k] * a + axisB[k] * b;
        pa[k] = v[k] + axisA[k] * step;
        pb[k] = v[k] + axisB[k] * step;
      }
      float ax, ay, bx, by;
      if (!ToScreen(view, pa, ax, ay) || !ToScreen(view, pb, bx, by))
        break;
      float j11 = (ax - sx) / step, j21 = (ay - sy) / step, j12 = (bx - sx) / step, j22 = (by - sy) / step;
      float det = j11 * j22 - j12 * j21;
      if (fabsf(det) < 1e-8f)
        break;
      float ex = x - sx, ey = y - sy;
      float da = (j22 * ex - j12 * ey) / det, db = (j11 * ey - j21 * ex) / det;
      float length = sqrtf(da * da + db * db);
      if (length > depth)
      {
        da *= depth / length;
        db *= depth / length;
      }
      bool nearer = false;
      for (int halve = 0; halve < 6 && !nearer; ++halve, da *= 0.5f, db *= 0.5f)
      {
        float n[3];
        for (int k = 0; k < 3; ++k)
          n[k] = base[k] + axisA[k] * (a + da) + axisB[k] * (b + db);
        float nx, ny;
        if (!ToScreen(view, n, nx, ny))
          continue;
        float e = (x - nx) * (x - nx) + (y - ny) * (y - ny);
        if (e < error)
        {
          a += da;
          b += db;
          sx = nx;
          sy = ny;
          error = e;
          nearer = true;
        }
      }
      if (!nearer)
        break;
    }
    for (int k = 0; k < 3; ++k)
      out[k] = base[k] + axisA[k] * a + axisB[k] * b;
    return true;
  }

  // Two bones bent so the third's place is the target (as near as they
  // reach): an upper bone (a), its child (b) and its child (c), e.g. arm,
  // forearm, hand. The joint keeps bending to the side it bent to.
  void TwoBone(Held& h, int a, int b, int c, float const* target, float (*w)[FloatsPerBone])
  {
    ComposeHeld(h, w);
    float pa[3], pb[3], pc[3];
    memcpy(pa, w[a], sizeof(pa));
    memcpy(pb, w[b], sizeof(pb));
    memcpy(pc, w[c], sizeof(pc));
    float ab[3] = { pb[0] - pa[0], pb[1] - pa[1], pb[2] - pa[2] };
    float bc[3] = { pc[0] - pb[0], pc[1] - pb[1], pc[2] - pb[2] };
    float ac[3] = { pc[0] - pa[0], pc[1] - pa[1], pc[2] - pa[2] };
    float at[3] = { target[0] - pa[0], target[1] - pa[1], target[2] - pa[2] };
    float lab = Length3(ab), lbc = Length3(bc), lat = Length3(at);
    if (lab < 1e-4f || lbc < 1e-4f || lat < 1e-4f)
      return;
    float d[3] = { at[0] / lat, at[1] / lat, at[2] / lat };
    float lo = fabsf(lab - lbc) + 1e-3f, hi = lab + lbc - 1e-3f;
    lat = lat < lo ? lo : (lat > hi ? hi : lat);
    // The side the joint bends to: b off the line from a to c
    float acn[3] = { ac[0], ac[1], ac[2] };
    if (Length3(acn) < 1e-5f)
      memcpy(acn, d, sizeof(acn));
    Normalize3(acn);
    float along = ab[0] * acn[0] + ab[1] * acn[1] + ab[2] * acn[2];
    float bend[3] = { ab[0] - acn[0] * along, ab[1] - acn[1] * along, ab[2] - acn[2] * along };
    if (Length3(bend) < 1e-5f)
      AnyPerpendicular(acn, bend);
    float off = bend[0] * d[0] + bend[1] * d[1] + bend[2] * d[2];
    for (int k = 0; k < 3; ++k)
      bend[k] -= d[k] * off;
    if (Length3(bend) < 1e-5f)
      AnyPerpendicular(d, bend);
    Normalize3(bend);
    float x = (lab * lab - lbc * lbc + lat * lat) / (2.f * lat);
    float height = sqrtf((std::max)(0.f, lab * lab - x * x));
    float b2[3], c2[3];
    for (int k = 0; k < 3; ++k)
    {
      b2[k] = pa[k] + d[k] * x + bend[k] * height;
      c2[k] = pa[k] + d[k] * lat;
    }
    // The upper bone turned so its child is at b2, then the child so its own
    // child is at c2
    float u[3] = { ab[0] / lab, ab[1] / lab, ab[2] / lab };
    float v[3] = { b2[0] - pa[0], b2[1] - pa[1], b2[2] - pa[2] };
    Normalize3(v);
    float q[4];
    FromTo(u, v, q);
    ApplyWorldTurn(h, a, q, w);
    ComposeHeld(h, w);
    float now[3] = { w[c][0] - w[b][0], w[c][1] - w[b][1], w[c][2] - w[b][2] };
    float want[3] = { c2[0] - w[b][0], c2[1] - w[b][1], c2[2] - w[b][2] };
    if (Length3(now) < 1e-5f || Length3(want) < 1e-5f)
      return;
    Normalize3(now);
    Normalize3(want);
    FromTo(now, want, q);
    ApplyWorldTurn(h, b, q, w);
  }

  // The bone moved by a world offset (its move is in its parent's frame; w is
  // the pose's world now)
  void TranslateBone(Held& h, int bone, float const* delta, float (*w)[FloatsPerBone])
  {
    int p = h.Parents[bone];
    float inverse[4] = { 0.f, 0.f, 0.f, 1.f }, local[3];
    if (p >= 0)
    {
      float parent[4];
      memcpy(parent, w[p] + 4, sizeof(parent));
      QNorm(parent);
      QConj(parent, inverse);
    }
    QRot(inverse, delta, local);
    for (int k = 0; k < 3; ++k)
      h.Move[bone][k] += local[k];
    ClampMove(h, bone);
  }

  // A face group moved by a world offset: each member the same way
  void MoveGroup(Held& h, int group, float const* delta)
  {
    static float w[MaxBones][FloatsPerBone];
    ComposeHeld(h, w);
    for (int k = 0; k < h.GroupSize[group]; ++k)
      TranslateBone(h, h.GroupMembers[group][k], delta, w);
  }

  // A bone that can't be moved without bending a bone outside its body part
  // (the first bone of a part: the spine's first, a thigh, a shoulder, the
  // neck, a finger's first) swings about its own place instead: its farthest
  // child goes as far and the same way as the cursor did
  void SwingBone(Held& h, int bone, float const* target, float (*w)[FloatsPerBone])
  {
    ComposeHeld(h, w);
    int child = -1;
    float best = 1e-3f;
    for (int c = bone + 1; c < h.Bones; ++c)
      if (h.Parents[c] == bone && h.InHeld[c])
      {
        float d = Distance3(w[c], w[bone]);
        if (d > best)
        {
          best = d;
          child = c;
        }
      }
    if (child < 0)
      return;
    float u[3] = { w[child][0] - w[bone][0], w[child][1] - w[bone][1], w[child][2] - w[bone][2] };
    float v[3] = { u[0] + target[0] - w[bone][0], u[1] + target[1] - w[bone][1], u[2] + target[2] - w[bone][2] };
    if (Length3(v) < 1e-4f)
      return;
    Normalize3(u);
    Normalize3(v);
    float q[4];
    FromTo(u, v, q);
    ApplyWorldTurn(h, bone, q, w);
  }

  // The bone moved to the target: the held root (Hips) moves the whole body,
  // a face bone moves itself; any other bone bends the bones above it (up to
  // chainBones of them) so it gets there, as near as they reach. They're
  // only ever bones of its own body part (an arm's, a leg's, the spine's, the
  // neck's, a finger's), never the held root: a hand or a knee doesn't turn
  // the whole body (2026-11-08, the user: "using a bone moves the whole body
  // instead of that bone"). A part's first bone swings instead (SwingBone).
  // From the pose as it is.
  void SolveMove(Held& h, int bone, float const* target, int chainBones)
  {
    static float w[MaxBones][FloatsPerBone];
    if (bone < 0 || bone >= h.Bones || !h.InHeld[bone])
      return;
    if (bone == h.Root || h.Face[bone] || g_FreeMove)
    {
      // Moved itself (Free move: any bone; its children go with it)
      ComposeHeld(h, w);
      float delta[3] = { target[0] - w[bone][0], target[1] - w[bone][1], target[2] - w[bone][2] };
      TranslateBone(h, bone, delta, w);
      return;
    }
    int chain[4];
    int count = 0;
    for (int a = h.Parents[bone]; a >= 0 && count < chainBones && count < 4 && h.InHeld[a] && a != h.Root &&
        h.Part[a] == h.Part[bone]; a = h.Parents[a])
      chain[count++] = a;
    if (count == 0)
    {
      SwingBone(h, bone, target, w);
      return;
    }
    if (count == 2)
    {
      TwoBone(h, chain[1], chain[0], bone, target, w);
      return;
    }
    // One bone: it turns to point at it; more: each in turn, again and again
    // (cyclic coordinate descent)
    int passes = count == 1 ? 1 : 16;
    for (int pass = 0; pass < passes; ++pass)
    {
      for (int k = 0; k < count; ++k)
      {
        ComposeHeld(h, w);
        float const* a = w[chain[k]];
        float u[3] = { w[bone][0] - a[0], w[bone][1] - a[1], w[bone][2] - a[2] };
        float v[3] = { target[0] - a[0], target[1] - a[1], target[2] - a[2] };
        if (Length3(u) < 1e-4f || Length3(v) < 1e-4f)
          continue;
        Normalize3(u);
        Normalize3(v);
        float q[4];
        FromTo(u, v, q);
        ApplyWorldTurn(h, chain[k], q, w);
      }
      ComposeHeld(h, w);
      if (Distance3(w[bone], target) < 0.001f)
        break;
    }
  }

  // The eye a dragged bone stands for: an eye, or an iris (its child); -1
  int EyeOf(Held const& h, int bone)
  {
    if (bone < 0 || bone >= h.Bones)
      return -1;
    if (h.Eye[bone])
      return bone;
    int p = h.Parents[bone];
    return p >= 0 && h.Eye[p] ? p : -1;
  }

  // The eyes turned to look somewhere: from the pose they were pressed in,
  // each eye's gaze is towards its iris (the iris sits 0.5 - 1.4 cm in front
  // of the eye's middle in Faith's, KrugerSec's and the women's skeletons'
  // bind poses), they looked at a point LookDistance in front of them, and
  // that point moves LookGain times as far as the cursor went from start to
  // target. Both eyes look at the same point (they turn in for a near one).
  // eye: one eye (an eye bone), or -1 for the Eyes group.
  void LookEyes(Held& h, int eye, float const* start, float const* target)
  {
    static float w[MaxBones][FloatsPerBone];
    ComposeHeld(h, w);
    int eyes[2], irises[2], count = 0;
    auto add = [&](int e)
    {
      if (count >= 2 || e < 0 || e >= h.Bones || !h.Eye[e])
        return;
      int iris = -1;
      float best = 1e-4f;
      for (int c = e + 1; c < h.Bones; ++c)
        if (h.Parents[c] == e && Distance3(w[c], w[e]) > best)
        {
          best = Distance3(w[c], w[e]);
          iris = c;
        }
      if (iris < 0)
        return;
      eyes[count] = e;
      irises[count] = iris;
      ++count;
    };
    if (eye >= 0)
      add(eye);
    else
      for (int k = 0; k < h.GroupSize[EyesGroup]; ++k)
        add(h.GroupMembers[EyesGroup][k]);
    if (count == 0)
      return;
    float middle[3] = {}, gaze[3] = {};
    float gazes[2][3];
    for (int n = 0; n < count; ++n)
      for (int k = 0; k < 3; ++k)
      {
        gazes[n][k] = w[irises[n]][k] - w[eyes[n]][k];
        middle[k] += w[eyes[n]][k] / count;
      }
    for (int n = 0; n < count; ++n)
    {
      Normalize3(gazes[n]);
      for (int k = 0; k < 3; ++k)
        gaze[k] += gazes[n][k];
    }
    Normalize3(gaze);
    float look[3];
    for (int k = 0; k < 3; ++k)
      look[k] = middle[k] + gaze[k] * LookDistance + (target[k] - start[k]) * LookGain;
    for (int n = 0; n < count; ++n)
    {
      float want[3] = { look[0] - w[eyes[n]][0], look[1] - w[eyes[n]][1], look[2] - w[eyes[n]][2] };
      if (Length3(want) < 1e-4f)
        continue;
      Normalize3(want);
      float q[4];
      FromTo(gazes[n], want, q);
      ApplyWorldTurn(h, eyes[n], q, w);
    }
  }

  // ---- Bones stop at walls (the game's thread: collision::Sweep, the free
  // camera's sweep through the game's physics, is only made there) ----

  // Within the space round where the character stands (its own body), when
  // the world's sweeps hit its own body (SelfHit 1)
  bool InSelf(Held const& h, float const* p)
  {
    if (h.SelfHit != 1)
      return false;
    float const* root = h.World[1];
    float dx = p[0] - root[0], dz = p[2] - root[2];
    return dx * dx + dz * dz < SelfRadius * SelfRadius && p[1] > root[1] - 0.3f && p[1] < root[1] + 2.2f;
  }

  // Where the line from p to q leaves that space (false: it doesn't)
  bool LeaveSelf(Held const& h, float const* p, float const* q, float* out)
  {
    for (int s = 1; s <= 50; ++s)
    {
      float t = s / 50.f;
      for (int k = 0; k < 3; ++k)
        out[k] = p[k] + (q[k] - p[k]) * t;
      if (!InSelf(h, out))
        return true;
    }
    return false;
  }

  // Whether the world's sweeps hit the character's own body: 4 rays at its
  // chest from 0.8 m away (in front, behind, left, right). Its own collision
  // capsule would be hit 0.1 - 0.55 m from its middle by 3 or 4 of them; a
  // wall can't be that near on 3 sides. Whether the sweep (collision filter
  // 9) takes characters at all wasn't known (2026-11-08); the log says.
  void ProbeSelf(Held& h)
  {
    float const* root = h.World[1];
    float centre[3] = { root[0], root[1] + 1.2f, root[2] };
    const float ways[4][2] = { { 1.f, 0.f }, { -1.f, 0.f }, { 0.f, 1.f }, { 0.f, -1.f } };
    int hits = 0;
    float nearest = 99.f, farthest = 0.f;
    for (int k = 0; k < 4; ++k)
    {
      float from[3] = { centre[0] + ways[k][0] * 0.8f, centre[1], centre[2] + ways[k][1] * 0.8f };
      float fraction = 1.f, normal[3];
      if (!collision::Sweep(from, centre, 0.f, &fraction, normal))
        continue;
      float distance = 0.8f * (1.f - fraction);   // from the middle
      if (distance >= 0.1f && distance <= 0.55f)
      {
        ++hits;
        nearest = (std::min)(nearest, distance);
        farthest = (std::max)(farthest, distance);
      }
    }
    h.SelfHit = hits >= 3 ? 1 : 0;
    if (h.SelfHit)
      util::log::Write("Pose: %s 0x%I64X: the world's sweeps hit its own body (%d of 4 rays at its chest, %.2f - %.2f m from "
        "its middle): what they touch within %.1f m of where it stands is taken as its own and left out", h.Label, h.Entity,
        hits, nearest, farthest, SelfRadius);
    else
      util::log::Write("Pose: %s 0x%I64X: the world's sweeps don't hit its own body (%d of 4 rays at its chest touched "
        "something 0.1 - 0.55 m from its middle): everything they touch counts", h.Label, h.Entity, hits);
  }

  // A ball of radius moved from from towards to as far as it gets, sliding
  // along what it touches (the free camera's way, collision::Move), its own
  // body left out (InSelf); where it ends up is in out
  void ClampPath(Held const& h, float const* from, float const* to, float radius, float* out)
  {
    float place[3] = { from[0], from[1], from[2] }, target[3] = { to[0], to[1], to[2] };
    for (int i = 0; i < 6; ++i)
    {
      float delta[3] = { target[0] - place[0], target[1] - place[1], target[2] - place[2] };
      float length = Length3(delta);
      if (length < 1e-4f)
        break;
      float fraction = 1.f, normal[3];
      if (!collision::Sweep(place, target, radius, &fraction, normal))
      {
        memcpy(place, target, sizeof(place));
        break;
      }
      float contact[3] = { place[0] + delta[0] * fraction, place[1] + delta[1] * fraction, place[2] + delta[2] * fraction };
      if (InSelf(h, contact))
      {
        // Its own body: on from where the line leaves it
        float past[3];
        if (!LeaveSelf(h, contact, target, past))
        {
          memcpy(place, target, sizeof(place));
          break;
        }
        memcpy(place, past, sizeof(place));
        continue;
      }
      float direction[3] = { delta[0] / length, delta[1] / length, delta[2] / length };
      float wall[3] = { normal[0], normal[1], normal[2] };
      if (Length3(wall) > 1e-4f)
        Normalize3(wall);
      else
        for (int k = 0; k < 3; ++k)
          wall[k] = -direction[k];
      // Already touching and going away from it (or along it): free
      if (delta[0] * wall[0] + delta[1] * wall[1] + delta[2] * wall[2] >= 0.f)
      {
        memcpy(place, target, sizeof(place));
        break;
      }
      float travel = fraction * length - 0.01f;
      if (travel > 0.f)
        for (int k = 0; k < 3; ++k)
          place[k] += direction[k] * travel;
      // The rest of the move along the surface
      float rest[3] = { target[0] - place[0], target[1] - place[1], target[2] - place[2] };
      float into = rest[0] * wall[0] + rest[1] * wall[1] + rest[2] * wall[2];
      for (int k = 0; k < 3; ++k)
        target[k] = place[k] + rest[k] - wall[k] * into;
    }
    memcpy(out, place, sizeof(place));
  }

  // Whether a ball going from from to to goes into something (its own body
  // left out)
  bool PathBlocked(Held const& h, float const* from, float const* to, float radius)
  {
    float place[3] = { from[0], from[1], from[2] };
    for (int i = 0; i < 4; ++i)
    {
      float delta[3] = { to[0] - place[0], to[1] - place[1], to[2] - place[2] };
      if (Length3(delta) < 1e-4f)
        return false;
      float fraction = 1.f, normal[3];
      if (!collision::Sweep(place, to, radius, &fraction, normal))
        return false;
      float contact[3] = { place[0] + delta[0] * fraction, place[1] + delta[1] * fraction, place[2] + delta[2] * fraction };
      if (!InSelf(h, contact))
        return delta[0] * normal[0] + delta[1] * normal[1] + delta[2] * normal[2] < 0.f;   // into it, not away
      if (!LeaveSelf(h, contact, to, place))
        return false;
    }
    return false;
  }

  // A joint whose path the walls check: the body's main joints (not the face,
  // fingers or helpers, which sit close to a main one)
  bool CheckedJoint(Held const& h, int bone)
  {
    return h.InHeld[bone] && !h.Face[bone] && !h.Finger[bone] && !h.Helper[bone];
  }

  // One frame of a bone dragged with "Bones stop at walls": the bone goes
  // from where it was last towards where it's wanted, as far as it gets
  // (sliding along walls), and the pose is solved for that from the pose it
  // was pressed in; if any of the main joints that move would go into
  // something on the way, the frame is left out (the pose stays as it was
  // last). Faces and groups aren't checked. Game thread, under g_Mutex.
  void DragWithWalls(Held& h, int heldIndex)
  {
    static float before[MaxBones][FloatsPerBone], after[MaxBones][FloatsPerBone];
    if (!g_Move.On || g_Move.Held != heldIndex || !g_Move.WantOk || g_Move.Group >= 0 || g_Move.Bone < 0 ||
        g_Move.Bone >= h.Bones || !h.WorldOk)
      return;
    if (g_Move.Pose.Turns.size() != (size_t)h.Bones * 4 || g_Move.Pose.Moves.size() != (size_t)h.Bones * 3 ||
        g_Move.Accepted.Turns.size() != (size_t)h.Bones * 4 || g_Move.Accepted.Moves.size() != (size_t)h.Bones * 3)
      return;
    int bone = g_Move.Bone;
    if (h.Face[bone])
    {
      // A face bone moves itself a little, inside the head: not checked
      ApplySnapshot(h, g_Move.Pose);
      SolveMove(h, bone, g_Move.Want, g_MoveBones);
      return;
    }
    if (h.SelfHit < 0)
      ProbeSelf(h);
    float place[3];
    ClampPath(h, g_Move.AcceptedPlace, g_Move.Want, BoneRadius, place);
    memcpy(g_Move.Target, g_Move.Want, sizeof(g_Move.Target));   // the line to where it's wanted shows when it stops short
    ApplySnapshot(h, g_Move.Accepted);
    ComposeHeld(h, before);
    ApplySnapshot(h, g_Move.Pose);
    SolveMove(h, bone, place, g_MoveBones);
    ComposeHeld(h, after);
    bool blocked = false;
    for (int n = 0; n < h.Count && !blocked; ++n)
    {
      int i = (int)h.List[n];
      if (i == bone || !CheckedJoint(h, i) || Distance3(before[i], after[i]) < 0.002f)
        continue;
      blocked = PathBlocked(h, before[i], after[i], JointRadius);
    }
    if (blocked)
    {
      ApplySnapshot(h, g_Move.Accepted);
      return;
    }
    g_Move.Accepted = SnapshotOf(h);
    memcpy(g_Move.AcceptedPlace, after[bone], sizeof(g_Move.AcceptedPlace));
  }
}

void pose_editor::OnViewWritten()
{
  if (g_shutdown || !g_mainHandle || !g_mainHandle->GetUI() || !g_mainHandle->GetUI()->IsEnabled() || !Available() ||
      !g_ppGameRenderer)
    return;
  View view{};
  view.Ok = ReadMainView(view.T, view.P);
  std::lock_guard<std::mutex> lock(g_ViewMutex);
  g_View = view;
}

void pose_editor::Update(__int64 soldier, bool replayBusy, bool timeFrozen)
{
  // Her hair while frozen (its own checks of the game's code)
  paused_hair::Update(soldier, timeFrozen, g_HairWhilePaused);
  if (!Available())
    return;
  std::lock_guard<std::mutex> lock(g_Mutex);
  ULONGLONG now = GetTickCount64();

  // What the menu asked for
  if (g_ReleaseRequest != -2)
  {
    for (int i = 0; i < MaxHeld; ++i)
      if (g_Held[i].Used && (g_ReleaseRequest == -1 || g_ReleaseRequest == i))
        Release(g_Held[i], "the menu's Let go");
    g_ReleaseRequest = -2;
  }
  if (g_TakeRequest)
  {
    g_TakeRequest = false;
    Take(g_TakeWhat, soldier);
  }

  // A bone dragged with "Bones stop at walls": solved here, where the world's
  // sweeps can be made, before its pose is handed
  g_WallsReady = collision::Available();
  if (g_StopAtWalls && g_WallsReady && g_Move.On && g_Move.Held >= 0 && g_Move.Held < MaxHeld && g_Held[g_Move.Held].Used)
    DragWithWalls(g_Held[g_Move.Held], g_Move.Held);

  for (int i = 0; i < MaxHeld; ++i)
    if (g_Held[i].Used)
      KeepHeld(g_Held[i], soldier, replayBusy, timeFrozen, now);

  // How often the game tried to update or move the weapons the tools hold
  static ULONGLONG weaponsLoggedAt = 0;
  if (now - weaponsLoggedAt >= CheckLogMs)
  {
    weaponsLoggedAt = now;
    int moves = util::hooks::PoseActorMovesHeld();
    if (moves)
      util::log::Write("Pose: the game's own moves of posed people's weapons were held back %d times in the last %.0f s "
        "(time %s)", moves, CheckLogMs / 1000.0, timeFrozen ? "frozen" : "running");
  }

  // The list, while the menu shows it
  if (now - g_ListWantedAt <= ListWantedMs && now - g_ScannedAt >= ScanPeriodMs)
    ScanCandidates(soldier, now);
}

void pose_editor::DrawUI()
{
  if (!Available())
  {
    ImGui::TextDisabled("%s", g_Unavailable[0] ? g_Unavailable : "Not available (see CT.log)");
    return;
  }
  std::lock_guard<std::mutex> lock(g_Mutex);
  ULONGLONG now = GetTickCount64();
  g_ListWantedAt = now;
  float fontSize = ImGui::GetFontSize();

  ImGui::PushTextWrapPos(0.f);
  ImGui::TextDisabled("Pose Faith, KrugerSec and other people bone by bone: pick one, Take pose, then drag its bones' "
    "dots on the picture where you like, turn a bone with its rings, or use the sliders here. Freeze time holds "
    "everyone while you pose (with time running, posed KrugerSec keep their AI: they walk, aim and fight in the "
    "pose).");
  ImGui::PopTextWrapPos();

  // Who can be posed
  ImGui::Text("Near the camera (%d):", (int)g_Candidates.size());
  ImGui::BeginChild("##PoseWho", ImVec2(0.f, fontSize * 6.5f), true);
  for (Candidate const& c : g_Candidates)
  {
    char row[200];
    char const* note = c.LettingGo ? "  (being let go)" : c.Held ? "  (held)" : c.Skeleton < 0 ? "  (skeleton not known: can't be posed)" :
      c.Animated == 0 ? "  (the game doesn't animate it now: can't be posed)" :
      c.OtherOverride ? "  (a replay or Dash ghost poses it)" : "";
    if (c.Kind == Kind_Faith)
      sprintf_s(row, "Faith%s##%I64X", note, c.Entity);
    else
      sprintf_s(row, "%s, %.1f m%s##%I64X", KindName(c.Kind, c.Skeleton), c.Distance, note, c.Entity);
    if (ImGui::Selectable(row, g_Picked == c.Entity))
    {
      g_Picked = c.Entity;
      for (int i = 0; i < MaxHeld; ++i)
        if (g_Held[i].Used && !g_Held[i].Restoring && g_Held[i].Entity == c.Entity && g_Active != i)
        {
          g_Active = i;
          g_SelectedBone = g_Held[i].Root;
          g_SelectedGroup = -1;
          g_SelectedWeapon = -1;
        }
    }
  }
  if (g_Candidates.empty())
    ImGui::TextDisabled(g_ScannedAt ? "Nobody that can be posed within %.0f m of the camera" : "Looking...", ScanRadius);
  ImGui::EndChild();

  Candidate const* picked = nullptr;
  for (Candidate const& c : g_Candidates)
    if (c.Entity == g_Picked)
      picked = &c;
  bool canTake = picked && !picked->Held && picked->Skeleton >= 0 && picked->Animated != 0 && !picked->OtherOverride;
  if (!canTake)
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
  if (ImGui::Button("Take pose", ImVec2(fontSize * 7.f, 0.f)) && canTake)
  {
    g_TakeWhat = *picked;
    g_TakeRequest = true;
  }
  if (!canTake)
    ImGui::PopStyleVar();
  ImGui::SameLine();
  ImGui::HelpMarker("Holds the picked one's pose as it is now: from then on it's drawn in the pose you make, from its "
    "hips down to its fingers, while it stays where the game has it. Up to 8 at a time.");
  Held* active = g_Active >= 0 && g_Held[g_Active].Used && !g_Held[g_Active].Restoring ? &g_Held[g_Active] : nullptr;
  int heldCount = 0;
  for (int i = 0; i < MaxHeld; ++i)
    heldCount += g_Held[i].Used;
  ImGui::SameLine();
  if (ImGui::Button("Let go", ImVec2(fontSize * 5.f, 0.f)) && active)
    g_ReleaseRequest = g_Active;
  ImGui::SameLine();
  char allLabel[48];
  sprintf_s(allLabel, "Let go of all (%d)", heldCount);
  if (ImGui::Button(allLabel) && heldCount)
    g_ReleaseRequest = -1;
  for (int i = 0; i < MaxHeld; ++i)
    if (g_Held[i].Used && g_Held[i].Restoring && g_Held[i].RestoreWaitLogged)
    {
      ImGui::PushTextWrapPos(0.f);
      ImGui::TextDisabled("%s: let go, its bones put back when the game animates it again (it doesn't now). Let go of all "
        "lets it go now, as posed.", g_Held[i].Label);
      ImGui::PopTextWrapPos();
    }

  // Faith's hair while frozen: always on, no tick since 2026-11-09 (g_HairWhilePaused)
  if (g_HairWhilePaused)
  {
    char hairStatus[160];
    paused_hair::Status(hairStatus, sizeof(hairStatus));
    if (hairStatus[0])
      ImGui::TextDisabled("%s", hairStatus);
  }

  if (!active)
  {
    ImGui::TextDisabled(heldCount ? "Pick a held one in the list to edit it" : "No pose held");
    return;
  }
  Held& h = *active;

  ImGui::Separator();
  ImGui::Text("Editing: %s", h.Label);
  ImGui::PushTextWrapPos(0.f);
  ImGui::TextDisabled("%s", h.Status[0] ? h.Status : "Being taken...");
  if (h.CheckLine[0])
    ImGui::TextDisabled("%s", h.CheckLine);
  ImGui::PopTextWrapPos();
  for (int i = 0; i < h.WeaponCount; ++i)
  {
    HeldWeapon const& w = h.Weapons[i];
    char row[200];
    char where[64];
    if (w.Bone == WorldFrame)
      strcpy_s(where, "put down");
    else if (w.Bone >= 0)
      sprintf_s(where, "with %s", BoneName(h, w.Bone));
    else
      strcpy_s(where, w.Gone ? "the game's (dropped or gone)" : "the game's (not on a bone when taken)");
    char const* state = w.Bone == -1 ? "" : (!g_WeaponsFollow ? ", where the game has it (Weapons go with the hands is off)" :
      (w.Drawing ? ", drawn by the tools" : (w.Holding ? ", its entity moved (its body not drawn by the tools)" : "")));
    sprintf_s(row, "Its %s: %s%s%s##weapon%d", w.Name, where, state, i == g_SelectedWeapon ? "  (picked)" : "", i);
    if (ImGui::Selectable(row, g_SelectedWeapon == i))
    {
      g_SelectedWeapon = i;
      g_SelectedBone = -1;
      g_SelectedGroup = -1;
    }
  }

  // Undo is pushed when a slider starts moving: the pose before this frame's change
  Snapshot before = SnapshotOf(h);
  bool slidersActive = false;

  // The bones
  ImGui::Checkbox("Show the bones on the picture", &g_ShowBones);
  ImGui::Checkbox("Face", &g_ShowFace);
  ImGui::SameLine();
  ImGui::Checkbox("Group the face", &g_GroupFace);
  ImGui::SameLine();
  ImGui::HelpMarker("The face's bones as groups: eyes, upper eyelids, lower eyelids, eyebrows, upper lip, lower lip, "
    "mouth corners, cheeks and nose, with the jaw, tongue and chin on their own. A group's dot is in the middle of "
    "its bones: turning it turns each of them the same way about its own place (the lids close), dragging it moves "
    "them all the same way (a lip, the brows). Dragging the Eyes makes them look: both turn to look at one point, "
    "which goes the way the cursor goes (an eye or an iris alone does the same for that eye). Untick it to work on "
    "single face bones. Other face bones are moved when you drag them, not bent like an arm.");
  ImGui::SameLine();
  ImGui::Checkbox("Fingers", &g_ShowFingers);
  ImGui::SameLine();
  ImGui::Checkbox("Helper bones", &g_ShowHelpers);
  {
    const char* bends[] = { "only the bone above it", "the 2 bones above it (arm, leg)", "the 3 bones above it",
      "the 4 bones above it" };
    int bendIndex = g_MoveBones - 1;
    ImGui::PushItemWidth(fontSize * 13.f);
    if (ImGui::Combo("A dragged bone bends", &bendIndex, bends, 4))
      g_MoveBones = bendIndex + 1;
    ImGui::PopItemWidth();
  }
  ImGui::Checkbox("Free move", &g_FreeMove);
  ImGui::SameLine();
  ImGui::HelpMarker("Dragging a bone moves the bone itself instead of bending the bones above it; the bones below it "
    "go with it. Drag one of the picked bone's red, green or blue arrows to move it along that axis only.");
  ImGui::Checkbox("Bones stop at walls", &g_StopAtWalls);
  if (g_StopAtWalls && !g_WallsReady)
  {
    ImGui::SameLine();
    ImGui::TextDisabled("(not available now: see CT.log)");
  }
  ImGui::Checkbox("Idle movement", &g_IdleOn);
  if (g_IdleOn)
  {
    ImGui::SliderFloat("Amount##PoseIdle", &g_IdleAmount, 0.f, 2.f, "%.2f");
    if (g_IdleUnavailable[0])
      ImGui::TextDisabled("%s", g_IdleUnavailable);
  }
  // Weapons go with the hands: always on, no tick since 2026-11-09 (g_WeaponsFollow)
  ImGui::Checkbox("Rings on the bone's own axes", &g_RingsOnBone);
  ImGui::SameLine();
  ImGui::HelpMarker("The picked bone's rings: drag one to turn the bone about it. Ticked, red, green and blue are the "
    "bone's own X, Y and Z (they turn with it); unticked, the world's X, Y (up) and Z. The white ring turns it about "
    "the view.");
  ImGui::InputText("Find a bone", g_Filter, sizeof(g_Filter));
  ImGui::BeginChild("##PoseBones", ImVec2(0.f, fontSize * 10.f), true);
  for (int n = 0; n < h.Count; ++n)
  {
    int i = (int)h.List[n];
    char const* name = BoneName(h, i);
    if (g_Filter[0])
    {
      // Case-insensitive part of the name
      bool match = false;
      size_t fl = strlen(g_Filter);
      for (char const* p = name; *p && !match; ++p)
        match = _strnicmp(p, g_Filter, fl) == 0;
      if (!match)
        continue;
    }
    else
    {
      // A face group's row, once, where its first bone is
      if (GroupShown(h, h.Group[i]))
      {
        int g = h.Group[i];
        if (h.GroupMembers[g][0] != i)
          continue;
        char row[160];
        int indent = h.Depth[i] > 30 ? 30 : h.Depth[i];
        sprintf_s(row, "%*s%s (%d bones)%s##group%d", indent * 2, "", FaceGroupNames[g], h.GroupSize[g],
          GroupEdited(h, g) ? "  *" : "", g);
        if (ImGui::Selectable(row, g_SelectedGroup == g))
        {
          g_SelectedGroup = g;
          g_SelectedBone = -1;
          g_SelectedWeapon = -1;
        }
        continue;
      }
      if (!BoneShown(h, i) && i != g_SelectedBone)
        continue;
    }
    char row[160];
    int indent = g_Filter[0] ? 0 : (h.Depth[i] > 30 ? 30 : h.Depth[i]);
    sprintf_s(row, "%*s%s%s##bone%d", indent * 2, "", name, IsEdited(h, i) ? "  *" : "", i);
    if (ImGui::Selectable(row, g_SelectedBone == i))
    {
      g_SelectedBone = i;
      g_SelectedGroup = -1;
      g_SelectedWeapon = -1;
    }
  }
  ImGui::EndChild();

  // The picked thing's sliders under IDs of its own: a field being typed in
  // isn't carried over to the next thing picked (the overlay takes the click
  // that picks it from ImGui)
  ImGui::PushID(g_SelectedGroup >= 0 ? 10000 + g_SelectedGroup : (g_SelectedWeapon >= 0 ? 20000 + g_SelectedWeapon :
    g_SelectedBone));
  if (g_SelectedGroup >= 0 && g_SelectedGroup < FaceGroupCount && h.GroupSize[g_SelectedGroup] > 0)
  {
    // A face group: every member gets the same turn (in its parent's frame,
    // about its own place) and the same move
    int g = g_SelectedGroup, first = h.GroupMembers[g][0];
    ImGui::Text("%s (%d bones)", FaceGroupNames[g], h.GroupSize[g]);
    float angles[3];
    QToAngles(h.Turn[first], angles);
    bool changed = false;
    changed |= ImGui::SliderFloat("Turn X##PoseGroupX", &angles[0], -180.f, 180.f, "%.1f deg");
    slidersActive |= ImGui::IsItemActive();
    changed |= ImGui::SliderFloat("Turn Y##PoseGroupY", &angles[1], -180.f, 180.f, "%.1f deg");
    slidersActive |= ImGui::IsItemActive();
    changed |= ImGui::SliderFloat("Turn Z##PoseGroupZ", &angles[2], -180.f, 180.f, "%.1f deg");
    slidersActive |= ImGui::IsItemActive();
    ImGui::SameLine();
    ImGui::HelpMarker("Each bone of the group gets this turn about its parent bone's X, Y and Z, about its own place "
      "(the values shown are its first bone's).");
    if (changed)
    {
      float q[4];
      QFromAngles(angles, q);
      for (int k = 0; k < h.GroupSize[g]; ++k)
        memcpy(h.Turn[h.GroupMembers[g][k]], q, sizeof(q));
    }
    float move[3] = { h.Move[first][0], h.Move[first][1], h.Move[first][2] };
    if (ImGui::SliderFloat3("Move##PoseGroupMove", move, -MaxFaceMove, MaxFaceMove, "%.3f m"))
      for (int k = 0; k < h.GroupSize[g]; ++k)
      {
        int m = h.GroupMembers[g][k];
        memcpy(h.Move[m], move, sizeof(move));
        ClampMove(h, m);
      }
    slidersActive |= ImGui::IsItemActive();
    ImGui::SameLine();
    ImGui::HelpMarker("Moves each bone of the group the same way, in its parent bone's frame (the face's; the jaw's "
      "for the lower lip and the low cheeks).");
    if (ImGui::Button("Reset this group"))
    {
      PushUndo(h, before);
      for (int k = 0; k < h.GroupSize[g]; ++k)
      {
        int m = h.GroupMembers[g][k];
        bool below[MaxBones] = {};
        for (int b = m; b < h.Bones; ++b)
        {
          below[b] = b == m || (h.Parents[b] >= m && below[h.Parents[b]]);
          if (below[b])
            ResetBone(h, b);
        }
      }
    }
  }
  else if (g_SelectedBone >= 0 && g_SelectedBone < h.Bones && h.InHeld[g_SelectedBone])
  {
    int i = g_SelectedBone;
    ImGui::Text("%s", BoneName(h, i));
    float angles[3];
    QToAngles(h.Turn[i], angles);
    float shown[3] = { angles[0], angles[1], angles[2] };
    bool changed = false;
    changed |= ImGui::SliderFloat("Turn X##PoseX", &shown[0], -180.f, 180.f, "%.1f deg");
    slidersActive |= ImGui::IsItemActive();
    changed |= ImGui::SliderFloat("Turn Y##PoseY", &shown[1], -180.f, 180.f, "%.1f deg");
    slidersActive |= ImGui::IsItemActive();
    changed |= ImGui::SliderFloat("Turn Z##PoseZ", &shown[2], -180.f, 180.f, "%.1f deg");
    slidersActive |= ImGui::IsItemActive();
    ImGui::SameLine();
    ImGui::HelpMarker("The bone's turn from the pose it was taken in, about its parent bone's X, Y and Z (in that "
      "order). The rings on the picture turn it too (see Rings on the bone's own axes).");
    if (changed)
      QFromAngles(shown, h.Turn[i]);
    bool moved = h.Move[i][0] != 0.f || h.Move[i][1] != 0.f || h.Move[i][2] != 0.f;
    if (i == h.Root || h.Face[i] || g_FreeMove || moved)
    {
      // The Hips: a drag field (a slider would need an end), up to 100 km
      bool changedMove = i == h.Root ? ImGui::DragFloat3("Move##PoseMove", h.Move[i], 0.01f, 0.f, 0.f, "%.3f m") :
        ImGui::SliderFloat3("Move##PoseMove", h.Move[i], -MoveLimit(h, i), MoveLimit(h, i), "%.3f m");
      if (changedMove)
        ClampMove(h, i);
      slidersActive |= ImGui::IsItemActive();
      ImGui::SameLine();
      ImGui::HelpMarker(i == h.Root ? "Moves the whole body from where the character is, in the frame of the bone under "
        "it (AITrajectory: where the game has the character, turned its way), as far as you like (up to 100 km): drag a "
        "field sideways, or double-click it and type a number." : (h.Face[i] ? "Moves the face bone, in "
        "its parent bone's frame." : "Moves the bone (and every bone below it) from where it is, in its parent bone's "
        "frame. The mesh stretches between it and its parent."));
    }
    if (ImGui::Button("Reset this bone"))
    {
      PushUndo(h, before);
      ResetBone(h, i);
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset it and the bones below"))
    {
      PushUndo(h, before);
      bool below[MaxBones] = {};
      for (int b = i; b < h.Bones; ++b)
        below[b] = b == i || (h.Parents[b] >= 0 && h.Parents[b] >= i && below[h.Parents[b]]);
      for (int b = i; b < h.Bones; ++b)
        if (below[b])
          ResetBone(h, b);
    }
  }
  else if (g_SelectedWeapon >= 0 && g_SelectedWeapon < h.WeaponCount)
  {
    HeldWeapon& w = h.Weapons[g_SelectedWeapon];
    ImGui::Text("Its %s", w.Name);
    if (w.Bone == -1)
    {
      ImGui::PushTextWrapPos(0.f);
      ImGui::TextDisabled(!w.Gone ? "It wasn't on one of its bones when the pose was taken, so the game has it." :
        "It isn't its own any more (dropped or gone): the game has it.");
      ImGui::PopTextWrapPos();
      float local[FloatsPerBone], world[FloatsPerBone];
      if (!w.Gone && ImGui::Button("Pose it where it is") && WeaponPlace(w, h.Entity, local, world))
      {
        PushUndo(h, before);
        w.Bone = WorldFrame;
        memcpy(w.OffsetLocal, local, sizeof(w.OffsetLocal));
        memcpy(w.OffsetWorld, world, sizeof(w.OffsetWorld));
        w.OffsetLocal[3] = w.OffsetWorld[3] = 1.f;
        w.EditMove[0] = w.EditMove[1] = w.EditMove[2] = 0.f;
        QIdentity(w.EditTurn);
      }
    }
    else
    {
      int attachTo = w.TakenBone >= 0 ? w.TakenBone : (h.GripBone >= 0 ? h.GripBone : h.Root);
      bool attached = w.Bone >= 0;
      char label[96];
      sprintf_s(label, "Goes with %s##PoseWeaponWith", BoneName(h, attached ? w.Bone : attachTo));
      if (ImGui::Checkbox(label, &attached) && h.WorldOk)
      {
        PushUndo(h, before);
        MoveWeaponTo(h, w, attached ? attachTo : WorldFrame);
      }
      ImGui::SameLine();
      ImGui::HelpMarker("Ticked, it goes with that bone when you pose it (where it is on the bone stays). Unticked, it's "
        "put down: it stays where it is in the world however the body moves. Either way, drag its square on the picture "
        "to move it, turn it with its rings, or use the sliders.");
      float angles[3];
      QToAngles(w.EditTurn, angles);
      bool changed = false;
      changed |= ImGui::SliderFloat("Turn X##PoseWeaponX", &angles[0], -180.f, 180.f, "%.1f deg");
      slidersActive |= ImGui::IsItemActive();
      changed |= ImGui::SliderFloat("Turn Y##PoseWeaponY", &angles[1], -180.f, 180.f, "%.1f deg");
      slidersActive |= ImGui::IsItemActive();
      changed |= ImGui::SliderFloat("Turn Z##PoseWeaponZ", &angles[2], -180.f, 180.f, "%.1f deg");
      slidersActive |= ImGui::IsItemActive();
      ImGui::SameLine();
      ImGui::HelpMarker(w.Bone >= 0 ? "Its turn about its own place, about its bone's X, Y and Z (in that order)." :
        "Its turn about its own place, about the world's X, Y (up) and Z (in that order).");
      if (changed)
        QFromAngles(angles, w.EditTurn);
      if (ImGui::DragFloat3("Move##PoseWeaponMove", w.EditMove, 0.005f, 0.f, 0.f, "%.3f m"))
        for (int k = 0; k < 3; ++k)
          if (!std::isfinite(w.EditMove[k]))
            w.EditMove[k] = 0.f;
      slidersActive |= ImGui::IsItemActive();
      ImGui::SameLine();
      ImGui::HelpMarker(w.Bone >= 0 ? "Its move from where it was on its bone, in the bone's frame. Drag a field "
        "sideways, or double-click it and type a number." : "Its move from where it was put down, in the world's frame "
        "(Y is up). Drag a field sideways, or double-click it and type a number.");
    }
    // (not one that was dropped: the game has it now)
    if (!w.Gone)
    {
      if (ImGui::Button("Reset this weapon"))
      {
        PushUndo(h, before);
        ResetWeapon(w);
      }
      ImGui::SameLine();
      ImGui::HelpMarker("Back as it was when the pose was taken: on the bone it was on (or the game's), not moved or "
        "turned.");
    }
  }
  else
    ImGui::TextDisabled("Pick a bone in the list or click its dot on the picture");
  ImGui::PopID();

  if (ImGui::Button("Reset all"))
  {
    PushUndo(h, before);
    for (int b = 0; b < h.Bones; ++b)
      ResetBone(h, b);
    for (int i = 0; i < h.WeaponCount; ++i)
      ResetWeapon(h.Weapons[i]);
  }
  ImGui::SameLine();
  char undoLabel[32];
  sprintf_s(undoLabel, "Undo (%d)", (int)h.Undo.size());
  if (ImGui::Button(undoLabel) && !h.Undo.empty())
  {
    ApplySnapshot(h, h.Undo.back());
    h.Undo.pop_back();
  }
  if (slidersActive && !g_SlidersWereActive)
    PushUndo(h, before);
  g_SlidersWereActive = slidersActive;

  // Saved poses
  ImGui::Separator();
  ImGui::InputText("Pose name", g_SaveName, sizeof(g_SaveName));
  ImGui::SameLine();
  if (ImGui::Button("Save"))
    SavePose(h, g_SaveName);
  if (now - g_FilesListedAt > 2000)
  {
    ListPoseFiles();
    g_FilesListedAt = now;
  }
  if (!g_PoseFiles.empty())
  {
    ImGui::Text("Load onto %s:", h.Label);
    ImGui::SameLine();
    ImGui::HelpMarker("Each saved bone with the same name gets the saved turn, so a pose loads onto any skeleton "
      "with those bones (Faith's onto a KrugerSec's and so on). Where two skeletons' bones are set up differently, "
      "a bone can come out turned another way. Undo takes a load back.");
    ImGui::BeginChild("##PoseFiles", ImVec2(0.f, fontSize * 5.f), true);
    for (std::string const& file : g_PoseFiles)
    {
      char label[128];
      sprintf_s(label, "%s##posefile", file.c_str());
      if (ImGui::Selectable(label, false))
        LoadPose(h, file.c_str());
    }
    ImGui::EndChild();
  }
  if (g_FileMessage[0])
  {
    ImGui::PushTextWrapPos(0.f);
    ImGui::TextDisabled("%s", g_FileMessage);
    ImGui::PopTextWrapPos();
  }
}

void pose_editor::DrawOverlay()
{
  if (!Available())
    return;
  std::lock_guard<std::mutex> lock(g_Mutex);
  ImGuiIO& io = ImGui::GetIO();
  Held* active = g_Active >= 0 && g_Held[g_Active].Used ? &g_Held[g_Active] : nullptr;
  if (g_Drag.On && (!io.MouseDown[0] || !active || g_Drag.Held != g_Active))
    g_Drag.On = false;
  if ((g_Move.Pending || g_Move.On) && (!io.MouseDown[0] || !active || g_Move.Held != g_Active))
    g_Move.Pending = g_Move.On = false;
  if (!active || !g_ShowBones || !active->WorldOk)
  {
    g_Move.Pending = g_Move.On = false;
    return;
  }
  View view;
  {
    std::lock_guard<std::mutex> viewLock(g_ViewMutex);
    view = g_View;
  }
  if (!view.Ok)
    return;
  Held& h = *active;
  float fontSize = ImGui::GetFontSize();

  // The ring held with the mouse turns its bone, or each of its face group's
  // bones about its own place: by the angle the cursor went along the ring
  // where it was taken
  if (g_Drag.On)
  {
    float tx = g_Drag.Tangent[0], ty = g_Drag.Tangent[1];
    float tt = tx * tx + ty * ty;
    float angle = tt > 1e-6f ? ((io.MousePos.x - g_Drag.Mouse[0]) * tx + (io.MousePos.y - g_Drag.Mouse[1]) * ty) / tt : 0.f;
    angle = angle > 6.2831853f ? 6.2831853f : (angle < -6.2831853f ? -6.2831853f : angle);
    float q[4];
    QAxis(g_Drag.Axis, angle, q);
    // turn = parent^-1 * q * parent * turn when taken
    auto turnOne = [&](int bone, float const* parent, float const* before)
    {
      float inverse[4], a[4], b[4];
      QConj(parent, inverse);
      QMul(inverse, q, a);
      QMul(a, parent, b);
      QMul(b, before, h.Turn[bone]);
      QNorm(h.Turn[bone]);
    };
    if (g_Drag.Weapon >= 0 && g_Drag.Weapon < h.WeaponCount && h.Weapons[g_Drag.Weapon].Bone != -1)
    {
      // Its edit's turn, in its frame: frame^-1 * q * frame * the turn when taken
      float inverse[4], a[4], b[4];
      float* turn = h.Weapons[g_Drag.Weapon].EditTurn;
      QConj(g_Drag.Parent, inverse);
      QMul(inverse, q, a);
      QMul(a, g_Drag.Parent, b);
      QMul(b, g_Drag.Turn, turn);
      QNorm(turn);
    }
    else if (g_Drag.Group >= 0 && g_Drag.Group < FaceGroupCount)
    {
      for (int k = 0; k < h.GroupSize[g_Drag.Group]; ++k)
        turnOne(h.GroupMembers[g_Drag.Group][k], g_Drag.GroupParents[k], g_Drag.GroupTurns[k]);
    }
    else if (g_Drag.Bone >= 0 && g_Drag.Bone < h.Bones)
      turnOne(g_Drag.Bone, g_Drag.Parent, g_Drag.Turn);
  }

  // A bone's or a face group's dot held with the mouse: it's moved once the
  // cursor has gone a few pixels (a click alone only picks it), to the point
  // under the cursor at its distance from the camera (the wheel: nearer or
  // farther), each frame from the pose it was pressed in
  bool moveWeapon = g_Move.Weapon >= 0 && g_Move.Weapon < h.WeaponCount && h.Weapons[g_Move.Weapon].Bone != -1;
  bool moveThing = (g_Move.Bone >= 0 && g_Move.Bone < h.Bones) || (g_Move.Group >= 0 && g_Move.Group < FaceGroupCount) ||
    moveWeapon;
  if (g_Move.Pending && moveThing)
  {
    float dx = io.MousePos.x - g_Move.Mouse[0], dy = io.MousePos.y - g_Move.Mouse[1];
    if (dx * dx + dy * dy >= MoveStartPixels * MoveStartPixels)
    {
      g_Move.Pending = false;
      g_Move.On = true;
      PushUndo(h, g_Move.Pose);
      g_Move.Accepted = g_Move.Pose;
      memcpy(g_Move.AcceptedPlace, g_Move.Start, sizeof(g_Move.AcceptedPlace));
      g_Move.WantOk = false;
    }
  }
  bool poseOk = g_Move.Pose.Turns.size() == (size_t)h.Bones * 4 && g_Move.Pose.Moves.size() == (size_t)h.Bones * 3;
  // A right click while one's held puts it back as it was pressed
  if ((g_Move.On || g_Move.Pending) && io.MouseClicked[1])
  {
    if (g_Move.On && poseOk)
    {
      ApplySnapshot(h, g_Move.Pose);
      if (!h.Undo.empty())
        h.Undo.pop_back();   // the drag's own step (pushed when it started)
    }
    g_Move.Pending = g_Move.On = false;
    io.MouseClicked[1] = false;
  }
  if (g_Move.On && moveThing && poseOk)
  {
    // Shift: up and down move it farther and nearer along the view, from
    // where it is when Shift goes down; let go, across the picture again
    // from there (no jump either way)
    bool depth = !g_Move.OnAxis && (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    if (depth != g_Move.Depth)
    {
      float now[3];
      memcpy(now, g_Move.TargetOk ? g_Move.Target : g_Move.Start, sizeof(now));
      if (depth)
      {
        memcpy(g_Move.DepthBase, now, sizeof(now));
        g_Move.DepthMouseY = io.MousePos.y;
      }
      else
      {
        float x, y;
        memcpy(g_Move.Base, now, sizeof(now));
        if (ToScreen(view, now, x, y))
        {
          g_Move.Offset[0] = x - io.MousePos.x;
          g_Move.Offset[1] = y - io.MousePos.y;
        }
      }
      g_Move.Depth = depth;
    }
    // The wheel: nearer or farther too
    if (io.MouseWheel != 0.f && !g_Move.OnAxis)
    {
      float* base = g_Move.Depth ? g_Move.DepthBase : g_Move.Base;
      float away[3] = { base[0] - view.T[12], base[1] - view.T[13], base[2] - view.T[14] };
      float distance = Length3(away);
      if (distance > 1e-3f)
      {
        float wanted = distance * powf(MoveWheelStep, io.MouseWheel);
        wanted = wanted < 0.3f ? 0.3f : (wanted > 500.f ? 500.f : wanted);
        for (int k = 0; k < 3; ++k)
          base[k] = view.T[12 + k] + away[k] * (wanted / distance);
      }
      io.MouseWheel = 0.f;
    }
    float target[3];
    if (g_Move.OnAxis)
    {
      // Along its arrow: as far as the cursor went along the arrow's way on the
      // screen, in the arrow's metres
      float ax = g_Move.AxisScreen[0], ay = g_Move.AxisScreen[1];
      float aa = ax * ax + ay * ay;
      float along = aa > 1e-4f ? ((io.MousePos.x - g_Move.Mouse[0]) * ax + (io.MousePos.y - g_Move.Mouse[1]) * ay) / aa *
        g_Move.AxisLength : 0.f;
      along = along > 5.f ? 5.f : (along < -5.f ? -5.f : along);
      for (int k = 0; k < 3; ++k)
        target[k] = g_Move.Start[k] + g_Move.Axis[k] * along;
      g_Move.TargetOk = true;
    }
    else if (g_Move.Depth)
    {
      float forward[3] = { -view.T[8], -view.T[9], -view.T[10] };
      Normalize3(forward);
      float off[3] = { g_Move.DepthBase[0] - view.T[12], g_Move.DepthBase[1] - view.T[13], g_Move.DepthBase[2] - view.T[14] };
      float depthNow = off[0] * forward[0] + off[1] * forward[1] + off[2] * forward[2];
      depthNow = depthNow < 0.3f ? 0.3f : depthNow;
      float height = ImGui::GetIO().DisplaySize.y;
      // A pixel up is as far as a pixel across the picture there, times DepthSpeed
      float perPixel = fabsf(view.P[5]) > 1e-4f && height > 0.f ? depthNow / (fabsf(view.P[5]) * height * 0.5f) : 0.f;
      float along = (g_Move.DepthMouseY - io.MousePos.y) * perPixel * DepthSpeed;
      along = along < 0.3f - depthNow ? 0.3f - depthNow : along;   // not behind or into the camera
      for (int k = 0; k < 3; ++k)
        target[k] = g_Move.DepthBase[k] + forward[k] * along;
      g_Move.TargetOk = true;
    }
    else
      g_Move.TargetOk = PlanePoint(view, g_Move.Base, io.MousePos.x + g_Move.Offset[0], io.MousePos.y + g_Move.Offset[1],
        target);
    // The eyes look (not with Free move: then they're moved like any bone)
    bool eyes = !g_FreeMove && ((g_Move.Group >= 0 && g_Move.Group == EyesGroup) || EyeOf(h, g_Move.Bone) >= 0);
    if (g_Move.TargetOk && moveWeapon)
    {
      // A weapon: its edit's move, in its frame (it isn't stopped by walls)
      memcpy(g_Move.Target, target, sizeof(target));
      ApplySnapshot(h, g_Move.Pose);
      HeldWeapon& w = h.Weapons[g_Move.Weapon];
      float const* frame = g_Move.WeaponFrame;
      float inverse[4], away[3] = { target[0] - frame[0], target[1] - frame[1], target[2] - frame[2] }, inFrame[3];
      QConj(frame + 4, inverse);
      QRot(inverse, away, inFrame);
      for (int k = 0; k < 3; ++k)
      {
        float m = inFrame[k] - w.OffsetLocal[k];
        w.EditMove[k] = std::isfinite(m) ? m : 0.f;
      }
    }
    else if (g_Move.TargetOk && g_StopAtWalls && g_WallsReady && g_Move.Group < 0 && !eyes)
    {
      // Bones stop at walls: the game's thread moves it (DragWithWalls)
      memcpy(g_Move.Want, target, sizeof(target));
      g_Move.WantOk = true;
    }
    else if (g_Move.TargetOk)
    {
      memcpy(g_Move.Target, target, sizeof(target));
      ApplySnapshot(h, g_Move.Pose);
      if (eyes)
        LookEyes(h, g_Move.Group >= 0 ? -1 : EyeOf(h, g_Move.Bone), g_Move.Start, target);
      else if (g_Move.Group >= 0)
      {
        float delta[3] = { target[0] - g_Move.Start[0], target[1] - g_Move.Start[1], target[2] - g_Move.Start[2] };
        MoveGroup(h, g_Move.Group, delta);
      }
      else
        SolveMove(h, g_Move.Bone, target, g_MoveBones);
    }
  }

  // The pose as it is now (the dots follow the mouse at once, not at the
  // next camera hook)
  static float shown[MaxBones][FloatsPerBone];
  ComposeHeld(h, shown);

  // The bones on the screen: At where the bone is, Draw where its dot is
  // drawn (face dots spread apart where they'd be on top of each other)
  struct Dot
  {
    ImVec2 At, Draw;
    bool Shown;
  };
  static Dot dots[MaxBones];
  int face = BoneByName(h.Skeleton, "Face");
  for (int n = 0; n < h.Count; ++n)
  {
    int i = (int)h.List[n];
    float x = 0.f, y = 0.f;
    // The Face bone sits on the head: with the face grouped, only in the list
    bool kind = BoneShown(h, i) && !(g_GroupFace && i == face);
    dots[i].Shown = (kind || i == g_SelectedBone) && ToScreen(view, shown[i], x, y);
    dots[i].At = dots[i].Draw = ImVec2(x, y);
  }
  // The face groups' dots: the middle of their bones
  float groupMiddle[FaceGroupCount][3];
  ImVec2 groupAt[FaceGroupCount], groupDraw[FaceGroupCount];
  bool groupShown[FaceGroupCount];
  for (int g = 0; g < FaceGroupCount; ++g)
  {
    groupShown[g] = false;
    if (!GroupShown(h, g))
      continue;
    GroupMiddle(h, g, shown, groupMiddle[g]);
    float x = 0.f, y = 0.f;
    groupShown[g] = ToScreen(view, groupMiddle[g], x, y);
    groupAt[g] = groupDraw[g] = ImVec2(x, y);
  }
  // The weapons' handles: where each one's pivot (its entity's place) is in
  // the pose as it is now, its rotation with it. One left to the game has none.
  float weaponPlace[MaxWeapons][FloatsPerBone];
  ImVec2 weaponAt[MaxWeapons], weaponDraw[MaxWeapons];
  bool weaponShown[MaxWeapons];
  for (int i = 0; i < MaxWeapons; ++i)
  {
    weaponShown[i] = false;
    if (i >= h.WeaponCount || h.Weapons[i].Bone == -1 || !g_WeaponsFollow)
      continue;
    WeaponWanted(h, h.Weapons[i], shown, h.Weapons[i].OffsetLocal, weaponPlace[i]);
    float x = 0.f, y = 0.f;
    weaponShown[i] = ToScreen(view, weaponPlace[i], x, y);
    weaponAt[i] = weaponDraw[i] = ImVec2(x, y);
  }
  bool weaponOk = g_SelectedWeapon >= 0 && g_SelectedWeapon < h.WeaponCount && weaponShown[g_SelectedWeapon];

  // Spread apart: the face's dots (the eyes' and the eyelids' middles are
  // both between the eyes, the lips' and the mouth corners' in the middle of
  // the mouth)
  {
    ImVec2* spread[96];
    int count = 0;
    for (int n = 0; n < h.Count && count < 96; ++n)
    {
      int i = (int)h.List[n];
      if (dots[i].Shown && h.Face[i])
        spread[count++] = &dots[i].Draw;
    }
    for (int g = 0; g < FaceGroupCount && count < 96; ++g)
      if (groupShown[g])
        spread[count++] = &groupDraw[g];
    float apart = fontSize * 0.85f;
    for (int pass = 0; pass < 12; ++pass)
      for (int a = 0; a < count; ++a)
        for (int b = a + 1; b < count; ++b)
        {
          float dx = spread[b]->x - spread[a]->x, dy = spread[b]->y - spread[a]->y;
          float distance = sqrtf(dx * dx + dy * dy);
          if (distance >= apart)
            continue;
          if (distance < 0.01f)
          {
            // Right on top of each other: apart in a fixed way
            float angle = (float)(a * 7 + b) * 2.39996f;
            dx = cosf(angle);
            dy = sinf(angle);
            distance = 1.f;
          }
          float push = (apart - distance) * 0.5f / distance;
          spread[a]->x -= dx * push;
          spread[a]->y -= dy * push;
          spread[b]->x += dx * push;
          spread[b]->y += dy * push;
        }
  }

  // A weapon's handle sits on its grip, where the hand's dot is: drawn apart
  // from the dots, the groups' and the other handles (they stay where they are)
  {
    float apart = fontSize * 0.85f;
    for (int pass = 0; pass < 6; ++pass)
      for (int i = 0; i < h.WeaponCount && i < MaxWeapons; ++i)
      {
        if (!weaponShown[i])
          continue;
        auto pushFrom = [&](ImVec2 other, int salt)
        {
          float dx = weaponDraw[i].x - other.x, dy = weaponDraw[i].y - other.y;
          float distance = sqrtf(dx * dx + dy * dy);
          if (distance >= apart)
            return;
          if (distance < 0.01f)
          {
            float angle = (float)(i * 7 + salt) * 2.39996f;
            dx = cosf(angle);
            dy = sinf(angle);
            distance = 1.f;
          }
          float push = (apart - distance) / distance;
          weaponDraw[i].x += dx * push;
          weaponDraw[i].y += dy * push;
        };
        for (int n = 0; n < h.Count; ++n)
        {
          int b = (int)h.List[n];
          if (dots[b].Shown)
            pushFrom(dots[b].Draw, b);
        }
        for (int g = 0; g < FaceGroupCount; ++g)
          if (groupShown[g])
            pushFrom(groupDraw[g], 200 + g);
        for (int k = 0; k < i; ++k)
          if (weaponShown[k])
            pushFrom(weaponDraw[k], 300 + k);
      }
  }

  // The picked bone's or face group's rings (not while something is moved)
  Ring rings[4];
  float radius = 0.f;
  int ringCount = 0;
  bool groupOk = g_SelectedGroup >= 0 && g_SelectedGroup < FaceGroupCount && groupShown[g_SelectedGroup];
  bool selectedOk = !groupOk && g_SelectedBone >= 0 && g_SelectedBone < h.Bones && h.InHeld[g_SelectedBone];
  if (!g_Move.On)
  {
    if (groupOk)
    {
      int first = h.GroupMembers[g_SelectedGroup][0];
      ringCount = MakeRings(view, groupMiddle[g_SelectedGroup], g_RingsOnBone ? shown[first] + 4 : nullptr, rings, radius);
    }
    else if (selectedOk)
      ringCount = MakeRings(view, shown[g_SelectedBone], g_RingsOnBone ? shown[g_SelectedBone] + 4 : nullptr, rings,
        radius);
    else if (weaponOk)
      ringCount = MakeRings(view, weaponPlace[g_SelectedWeapon], g_RingsOnBone ? weaponPlace[g_SelectedWeapon] + 4 : nullptr,
        rings, radius);
  }
  // Free move: the picked bone's X / Y / Z arrows, in place of its rings (the
  // bone's own axes or the world's, as the rings' tick says)
  struct Arrow
  {
    float Axis[3];
    ImVec2 From, To;
    bool Shown;
    ImU32 Colour;
  };
  Arrow arrows[3];
  int arrowCount = 0;
  if (g_FreeMove && selectedOk && ringCount > 0)
  {
    ringCount = 0;
    float const* p = shown[g_SelectedBone];
    float q[4] = { 0.f, 0.f, 0.f, 1.f };
    if (g_RingsOnBone)
    {
      memcpy(q, p + 4, sizeof(q));
      QNorm(q);
    }
    const ImU32 colours[3] = { IM_COL32(235, 80, 80, 235), IM_COL32(90, 210, 90, 235), IM_COL32(90, 140, 255, 235) };
    for (int a = 0; a < 3; ++a)
    {
      float unit[3] = { a == 0 ? 1.f : 0.f, a == 1 ? 1.f : 0.f, a == 2 ? 1.f : 0.f };
      Arrow& arrow = arrows[a];
      QRot(q, unit, arrow.Axis);
      Normalize3(arrow.Axis);
      float tip[3] = { p[0] + arrow.Axis[0] * radius, p[1] + arrow.Axis[1] * radius, p[2] + arrow.Axis[2] * radius };
      float x0, y0, x1, y1;
      arrow.Shown = ToScreen(view, p, x0, y0) && ToScreen(view, tip, x1, y1);
      arrow.From = ImVec2(x0, y0);
      arrow.To = ImVec2(x1, y1);
      arrow.Colour = colours[a];
    }
    arrowCount = 3;
  }

  // What the cursor is on (off the menu's windows): the nearest dot (a
  // bone's or a face group's) first, then a ring. The dots come first: the
  // rings ran across the next joints' dots and took their clicks.
  int hoverRing = -1, hoverSegment = -1, hoverDot = -1, hoverGroup = -1, hoverArrow = -1, hoverWeapon = -1;
  if (!g_Drag.On && !g_Move.Pending && !g_Move.On && !io.WantCaptureMouse)
  {
    float reach = fontSize * DotReach;
    float bestDot = reach * reach;
    for (int n = 0; n < h.Count; ++n)
    {
      int i = (int)h.List[n];
      if (!dots[i].Shown)
        continue;
      float dx = dots[i].Draw.x - io.MousePos.x, dy = dots[i].Draw.y - io.MousePos.y;
      if (dx * dx + dy * dy <= bestDot)
      {
        bestDot = dx * dx + dy * dy;
        hoverDot = i;
      }
    }
    for (int g = 0; g < FaceGroupCount; ++g)
    {
      if (!groupShown[g])
        continue;
      float dx = groupDraw[g].x - io.MousePos.x, dy = groupDraw[g].y - io.MousePos.y;
      if (dx * dx + dy * dy <= bestDot)
      {
        bestDot = dx * dx + dy * dy;
        hoverGroup = g;
        hoverDot = -1;
      }
    }
    for (int i = 0; i < h.WeaponCount && i < MaxWeapons; ++i)
    {
      if (!weaponShown[i])
        continue;
      float dx = weaponDraw[i].x - io.MousePos.x, dy = weaponDraw[i].y - io.MousePos.y;
      if (dx * dx + dy * dy <= bestDot)
      {
        bestDot = dx * dx + dy * dy;
        hoverWeapon = i;
        hoverDot = hoverGroup = -1;
      }
    }
    if (hoverDot < 0 && hoverGroup < 0 && hoverWeapon < 0)
    {
      float best = RingReach * fontSize;
      for (int a = 0; a < arrowCount; ++a)
      {
        if (!arrows[a].Shown)
          continue;
        float along;
        float d = SegmentDistance(io.MousePos, arrows[a].From, arrows[a].To, along);
        if (d < best)
        {
          best = d;
          hoverArrow = a;
        }
      }
      for (int r = 0; r < ringCount; ++r)
        for (int s = 0; s < RingSegments; ++s)
        {
          if (!rings[r].Shown[s] || !rings[r].Shown[s + 1])
            continue;
          float along;
          float d = SegmentDistance(io.MousePos, rings[r].Points[s], rings[r].Points[s + 1], along);
          if (d < best)
          {
            best = d;
            hoverRing = r;
            hoverSegment = s;
          }
        }
    }
  }

  // A click: on an arrow it moves the bone along it, on a ring it turns the
  // bone (or the group), on a dot it picks it and holds it. The click is the
  // pose's: taken off ImGui's input, so the lights' dots and the camera's
  // track path (drawn after) don't take it too.
  if (io.MouseClicked[0] && !io.MouseDownOwned[0] && !g_Drag.On && hoverArrow >= 0 && selectedOk)
  {
    Arrow const& arrow = arrows[hoverArrow];
    g_Move.Pending = true;
    g_Move.On = false;
    g_Move.Held = g_Active;
    g_Move.Bone = g_SelectedBone;
    g_Move.Group = -1;
    g_Move.Weapon = -1;
    g_Move.Mouse[0] = io.MousePos.x;
    g_Move.Mouse[1] = io.MousePos.y;
    g_Move.Offset[0] = g_Move.Offset[1] = 0.f;
    memcpy(g_Move.Start, shown[g_SelectedBone], sizeof(g_Move.Start));
    memcpy(g_Move.Base, g_Move.Start, sizeof(g_Move.Base));
    g_Move.Pose = SnapshotOf(h);
    g_Move.TargetOk = false;
    g_Move.Depth = false;
    g_Move.OnAxis = true;
    memcpy(g_Move.Axis, arrow.Axis, sizeof(g_Move.Axis));
    g_Move.AxisScreen[0] = arrow.To.x - arrow.From.x;
    g_Move.AxisScreen[1] = arrow.To.y - arrow.From.y;
    g_Move.AxisLength = radius;
    EndTextEdit();
    io.MouseClicked[0] = false;
  }
  else if (io.MouseClicked[0] && !io.MouseDownOwned[0] && !g_Drag.On && hoverRing >= 0)
  {
    Ring const& ring = rings[hoverRing];
    PushUndo(h, SnapshotOf(h));
    g_Drag.On = true;
    g_Drag.Held = g_Active;
    g_Drag.Ring = hoverRing;
    memcpy(g_Drag.Axis, ring.Axis, sizeof(g_Drag.Axis));
    // The ring's way on the screen where it was taken: pixels per radian
    float step = 6.2831853f / RingSegments;
    ImVec2 a = ring.Points[hoverSegment], b = ring.Points[hoverSegment + 1];
    g_Drag.Tangent[0] = (b.x - a.x) / step;
    g_Drag.Tangent[1] = (b.y - a.y) / step;
    g_Drag.Mouse[0] = io.MousePos.x;
    g_Drag.Mouse[1] = io.MousePos.y;
    auto parentOf = [&](int bone, float* out)
    {
      int p = h.Parents[bone];
      if (p >= 0)
        memcpy(out, shown[p] + 4, 4 * sizeof(float));
      else
        QIdentity(out);
      QNorm(out);
    };
    g_Drag.Weapon = -1;
    if (weaponOk && !groupOk && !selectedOk)
    {
      // A weapon's ring: its edit's turn, in its frame
      HeldWeapon const& w = h.Weapons[g_SelectedWeapon];
      float frame[FloatsPerBone];
      WeaponFrame(h, w, shown, frame);
      g_Drag.Weapon = g_SelectedWeapon;
      g_Drag.Group = -1;
      g_Drag.Bone = -1;
      memcpy(g_Drag.Parent, frame + 4, sizeof(g_Drag.Parent));
      QNorm(g_Drag.Parent);
      memcpy(g_Drag.Turn, w.EditTurn, sizeof(g_Drag.Turn));
    }
    else if (groupOk)
    {
      g_Drag.Group = g_SelectedGroup;
      g_Drag.Bone = -1;
      for (int k = 0; k < h.GroupSize[g_SelectedGroup]; ++k)
      {
        int m = h.GroupMembers[g_SelectedGroup][k];
        parentOf(m, g_Drag.GroupParents[k]);
        memcpy(g_Drag.GroupTurns[k], h.Turn[m], sizeof(g_Drag.GroupTurns[k]));
      }
    }
    else
    {
      g_Drag.Group = -1;
      g_Drag.Bone = g_SelectedBone;
      parentOf(g_SelectedBone, g_Drag.Parent);
      memcpy(g_Drag.Turn, h.Turn[g_SelectedBone], sizeof(g_Drag.Turn));
    }
    EndTextEdit();
    io.MouseClicked[0] = false;
  }
  else if (io.MouseClicked[0] && !io.MouseDownOwned[0] && !g_Drag.On && hoverWeapon >= 0)
  {
    // A weapon's handle: picked, and held (moved if the cursor goes on)
    HeldWeapon const& w = h.Weapons[hoverWeapon];
    g_SelectedWeapon = hoverWeapon;
    g_SelectedBone = -1;
    g_SelectedGroup = -1;
    g_Move.Pending = true;
    g_Move.On = false;
    g_Move.Held = g_Active;
    g_Move.Mouse[0] = io.MousePos.x;
    g_Move.Mouse[1] = io.MousePos.y;
    g_Move.Bone = -1;
    g_Move.Group = -1;
    g_Move.Weapon = hoverWeapon;
    WeaponFrame(h, w, shown, g_Move.WeaponFrame);
    g_Move.Offset[0] = weaponAt[hoverWeapon].x - io.MousePos.x;
    g_Move.Offset[1] = weaponAt[hoverWeapon].y - io.MousePos.y;
    memcpy(g_Move.Start, weaponPlace[hoverWeapon], sizeof(g_Move.Start));
    memcpy(g_Move.Base, g_Move.Start, sizeof(g_Move.Base));
    g_Move.Pose = SnapshotOf(h);
    g_Move.TargetOk = false;
    g_Move.Depth = false;
    g_Move.OnAxis = false;
    EndTextEdit();
    io.MouseClicked[0] = false;
  }
  else if (io.MouseClicked[0] && !io.MouseDownOwned[0] && !g_Drag.On && (hoverDot >= 0 || hoverGroup >= 0))
  {
    // Picked, and held: moved if the cursor goes on. The drag goes by where
    // the bone (or the group's middle) really is, not where its dot is drawn.
    g_Move.Pending = true;
    g_Move.On = false;
    g_Move.Held = g_Active;
    g_Move.Mouse[0] = io.MousePos.x;
    g_Move.Mouse[1] = io.MousePos.y;
    g_Move.Weapon = -1;
    if (hoverGroup >= 0)
    {
      g_SelectedGroup = hoverGroup;
      g_SelectedBone = -1;
      g_SelectedWeapon = -1;
      g_Move.Group = hoverGroup;
      g_Move.Bone = -1;
      g_Move.Offset[0] = groupAt[hoverGroup].x - io.MousePos.x;
      g_Move.Offset[1] = groupAt[hoverGroup].y - io.MousePos.y;
      memcpy(g_Move.Start, groupMiddle[hoverGroup], sizeof(g_Move.Start));
    }
    else
    {
      g_SelectedBone = hoverDot;
      g_SelectedGroup = -1;
      g_SelectedWeapon = -1;
      g_Move.Group = -1;
      g_Move.Bone = hoverDot;
      g_Move.Offset[0] = dots[hoverDot].At.x - io.MousePos.x;
      g_Move.Offset[1] = dots[hoverDot].At.y - io.MousePos.y;
      memcpy(g_Move.Start, shown[hoverDot], sizeof(g_Move.Start));
    }
    memcpy(g_Move.Base, g_Move.Start, sizeof(g_Move.Base));
    g_Move.Pose = SnapshotOf(h);
    g_Move.TargetOk = false;
    g_Move.Depth = false;
    g_Move.OnAxis = false;
    EndTextEdit();
    io.MouseClicked[0] = false;
  }

  ImDrawList* pDrawList = ImGui::GetOverlayDrawList();
  const ImU32 shadow = IM_COL32(0, 0, 0, 160);
  const ImU32 line = IM_COL32(255, 255, 255, 90);
  const ImU32 dot = IM_COL32(255, 220, 90, 230);
  const ImU32 groupDot = IM_COL32(120, 210, 255, 235);
  const ImU32 turned = IM_COL32(255, 140, 40, 255);
  const ImU32 picked = IM_COL32(255, 255, 255, 255);
  auto shadowedText = [&](ImVec2 at, ImU32 colour, char const* text)
  {
    pDrawList->AddText(ImVec2(at.x + 1.f, at.y + 1.f), shadow, text);
    pDrawList->AddText(at, colour, text);
  };
  // The skeleton's lines, then its dots
  for (int n = 0; n < h.Count; ++n)
  {
    int i = (int)h.List[n], p = h.Parents[i];
    if (p < 0 || !h.InHeld[p] || !dots[i].Shown || !dots[p].Shown)
      continue;
    pDrawList->AddLine(dots[p].Draw, dots[i].Draw, shadow, 3.f);
    pDrawList->AddLine(dots[p].Draw, dots[i].Draw, line, 1.5f);
  }
  // A face dot spread from where its bone (or group) really is: a faint line
  // to there
  auto leader = [&](ImVec2 from, ImVec2 to)
  {
    float dx = to.x - from.x, dy = to.y - from.y;
    if (dx * dx + dy * dy > 9.f)
    {
      pDrawList->AddLine(from, to, IM_COL32(255, 255, 255, 70), 1.f);
      pDrawList->AddCircleFilled(to, 2.f, IM_COL32(255, 255, 255, 120), 6);
    }
  };
  for (int n = 0; n < h.Count; ++n)
  {
    int i = (int)h.List[n];
    if (dots[i].Shown && h.Face[i])
      leader(dots[i].Draw, dots[i].At);
  }
  for (int g = 0; g < FaceGroupCount; ++g)
    if (groupShown[g])
      leader(groupDraw[g], groupAt[g]);
  // The picked or hovered group's bones: small dots where they are
  for (int g = 0; g < FaceGroupCount; ++g)
  {
    if (!groupShown[g] || (g != g_SelectedGroup && g != hoverGroup))
      continue;
    for (int k = 0; k < h.GroupSize[g]; ++k)
    {
      float x, y;
      if (ToScreen(view, shown[h.GroupMembers[g][k]], x, y))
        pDrawList->AddCircleFilled(ImVec2(x, y), 2.5f, groupDot, 8);
    }
  }
  // The dots: the picked one, the one under the cursor and the one being
  // moved on top, with a ring round them
  int moving = g_Move.On && g_Move.Bone >= 0 && g_Move.Bone < h.Bones ? g_Move.Bone : -1;
  auto drawDot = [&](int i)
  {
    bool hot = i == hoverDot || i == moving;
    float size = fontSize * (i == g_SelectedBone ? 0.34f : (hot ? 0.32f : 0.24f));
    pDrawList->AddCircleFilled(dots[i].Draw, size + 1.5f, shadow, 16);
    pDrawList->AddCircleFilled(dots[i].Draw, size, i == g_SelectedBone ? picked : (IsEdited(h, i) ? turned : dot), 16);
    if (hot)
      pDrawList->AddCircle(dots[i].Draw, size + fontSize * 0.18f, IM_COL32(255, 255, 255, 220), 20, 2.f);
  };
  for (int n = 0; n < h.Count; ++n)
  {
    int i = (int)h.List[n];
    if (dots[i].Shown && i != g_SelectedBone && i != hoverDot && i != moving)
      drawDot(i);
  }
  for (int i : { g_SelectedBone, hoverDot, moving })
    if (i >= 0 && i < h.Bones && h.InHeld[i] && dots[i].Shown)
      drawDot(i);
  for (int g = 0; g < FaceGroupCount; ++g)
  {
    if (!groupShown[g])
      continue;
    float size = fontSize * (g == g_SelectedGroup ? 0.34f : (g == hoverGroup ? 0.32f : 0.24f));
    ImU32 colour = g == g_SelectedGroup ? picked : (GroupEdited(h, g) ? turned : groupDot);
    // A diamond, so groups stand apart from bones
    ImVec2 c = groupDraw[g];
    pDrawList->AddQuadFilled(ImVec2(c.x, c.y - size - 1.5f), ImVec2(c.x + size + 1.5f, c.y), ImVec2(c.x, c.y + size + 1.5f),
      ImVec2(c.x - size - 1.5f, c.y), shadow);
    pDrawList->AddQuadFilled(ImVec2(c.x, c.y - size), ImVec2(c.x + size, c.y), ImVec2(c.x, c.y + size), ImVec2(c.x - size, c.y),
      colour);
  }
  // The weapons' handles: squares, so they stand apart from bones and groups
  const ImU32 weaponDot = IM_COL32(150, 240, 150, 235);
  for (int i = 0; i < h.WeaponCount && i < MaxWeapons; ++i)
  {
    if (!weaponShown[i])
      continue;
    HeldWeapon const& w = h.Weapons[i];
    bool edited = w.EditMove[0] != 0.f || w.EditMove[1] != 0.f || w.EditMove[2] != 0.f || !QIsIdentity(w.EditTurn) ||
      w.Bone != w.TakenBone || memcmp(w.OffsetLocal, w.TakenOffsetLocal, sizeof(w.OffsetLocal)) != 0;
    bool hot = i == hoverWeapon || (g_Move.On && g_Move.Weapon == i);
    float size = fontSize * (i == g_SelectedWeapon ? 0.30f : (hot ? 0.28f : 0.22f));
    ImVec2 c = weaponDraw[i];
    leader(c, weaponAt[i]);
    pDrawList->AddRectFilled(ImVec2(c.x - size - 1.5f, c.y - size - 1.5f), ImVec2(c.x + size + 1.5f, c.y + size + 1.5f), shadow);
    pDrawList->AddRectFilled(ImVec2(c.x - size, c.y - size), ImVec2(c.x + size, c.y + size),
      i == g_SelectedWeapon ? picked : (edited ? turned : weaponDot));
    if (hot)
      pDrawList->AddRect(ImVec2(c.x - size - fontSize * 0.18f, c.y - size - fontSize * 0.18f),
        ImVec2(c.x + size + fontSize * 0.18f, c.y + size + fontSize * 0.18f), IM_COL32(255, 255, 255, 220), 0.f, 0, 2.f);
  }
  // While one's moved: what the keys do, by the cursor
  if (g_Move.On)
  {
    ImVec2 at(io.MousePos.x + fontSize * 1.2f, io.MousePos.y + fontSize * 0.9f);
    shadowedText(at, IM_COL32(255, 255, 255, 210), g_Move.OnAxis ? "Along the arrow   Right click: cancel" :
      (g_Move.Depth ? "Nearer / farther (let go of Shift: across)" : "Shift: nearer / farther   Right click: cancel"));
  }
  // A moved bone that can't reach where it's wanted: a line to there
  if (g_Move.On && g_Move.TargetOk && g_Move.Bone >= 0 && g_Move.Bone < h.Bones && dots[g_Move.Bone].Shown)
  {
    float tx, ty;
    if (ToScreen(view, g_Move.Target, tx, ty))
    {
      ImVec2 at(tx, ty), from = dots[g_Move.Bone].At;
      float dx = at.x - from.x, dy = at.y - from.y;
      if (dx * dx + dy * dy > 36.f)
      {
        pDrawList->AddLine(from, at, IM_COL32(255, 255, 255, 110), 1.5f);
        pDrawList->AddCircle(at, fontSize * 0.25f, shadow, 12, 3.f);
        pDrawList->AddCircle(at, fontSize * 0.25f, IM_COL32(255, 255, 255, 200), 12, 1.5f);
      }
    }
  }
  // Free move's arrows, with a head at the tip
  for (int a = 0; a < arrowCount; ++a)
  {
    Arrow const& arrow = arrows[a];
    if (!arrow.Shown)
      continue;
    bool hot = a == hoverArrow;
    pDrawList->AddLine(arrow.From, arrow.To, shadow, hot ? 5.f : 3.5f);
    pDrawList->AddLine(arrow.From, arrow.To, arrow.Colour, hot ? 3.f : 2.f);
    float dx = arrow.To.x - arrow.From.x, dy = arrow.To.y - arrow.From.y;
    float length = sqrtf(dx * dx + dy * dy);
    if (length > 1.f)
    {
      float ux = dx / length, uy = dy / length, size = fontSize * (hot ? 0.55f : 0.45f);
      ImVec2 base(arrow.To.x - ux * size, arrow.To.y - uy * size);
      ImVec2 left(base.x - uy * size * 0.5f, base.y + ux * size * 0.5f), right(base.x + uy * size * 0.5f, base.y - ux * size * 0.5f);
      pDrawList->AddTriangleFilled(ImVec2(arrow.To.x + ux * 1.5f, arrow.To.y + uy * 1.5f), ImVec2(left.x - uy * 1.5f, left.y + ux * 1.5f),
        ImVec2(right.x + uy * 1.5f, right.y - ux * 1.5f), shadow);
      pDrawList->AddTriangleFilled(arrow.To, left, right, arrow.Colour);
    }
  }
  // The rings, and the picked and hovered names
  for (int r = 0; r < ringCount; ++r)
  {
    Ring const& ring = rings[r];
    bool hot = r == hoverRing || (g_Drag.On && g_Drag.Ring == r);
    for (int s = 0; s < RingSegments; ++s)
    {
      if (!ring.Shown[s] || !ring.Shown[s + 1])
        continue;
      pDrawList->AddLine(ring.Points[s], ring.Points[s + 1], shadow, hot ? 5.f : 3.5f);
      pDrawList->AddLine(ring.Points[s], ring.Points[s + 1], ring.Colour, hot ? 3.f : 1.75f);
    }
  }
  // The pick checked again here: a click above may have picked a bone in
  // place of a group, or a group in place of a bone, after groupOk and
  // selectedOk were worked out (2026-11-08 13:58: pressing a bone's dot with
  // a face group picked gave FaceGroupNames[-1] here and crashed the game)
  bool groupLabel = g_SelectedGroup >= 0 && g_SelectedGroup < FaceGroupCount && groupShown[g_SelectedGroup];
  bool boneLabel = !groupLabel && g_SelectedBone >= 0 && g_SelectedBone < h.Bones && h.InHeld[g_SelectedBone];
  bool weaponLabel = !groupLabel && !boneLabel && g_SelectedWeapon >= 0 && g_SelectedWeapon < h.WeaponCount &&
    g_SelectedWeapon < MaxWeapons && weaponShown[g_SelectedWeapon];
  if (weaponLabel)
  {
    ImVec2 at(weaponDraw[g_SelectedWeapon].x + fontSize * 0.6f, weaponDraw[g_SelectedWeapon].y - fontSize * 1.4f);
    shadowedText(at, picked, h.Weapons[g_SelectedWeapon].Name);
  }
  if (groupLabel)
  {
    ImVec2 at(groupDraw[g_SelectedGroup].x + fontSize * 0.6f, groupDraw[g_SelectedGroup].y - fontSize * 1.4f);
    shadowedText(at, picked, FaceGroupNames[g_SelectedGroup]);
  }
  else if (boneLabel && dots[g_SelectedBone].Shown)
  {
    ImVec2 at(dots[g_SelectedBone].Draw.x + fontSize * 0.6f, dots[g_SelectedBone].Draw.y - fontSize * 1.4f);
    shadowedText(at, picked, BoneName(h, g_SelectedBone));
  }
  if (hoverGroup >= 0 && hoverGroup != g_SelectedGroup)
  {
    ImVec2 at(groupDraw[hoverGroup].x + fontSize * 0.5f, groupDraw[hoverGroup].y - fontSize * 1.2f);
    shadowedText(at, IM_COL32(255, 255, 255, 200), FaceGroupNames[hoverGroup]);
  }
  else if (hoverDot >= 0 && hoverDot != g_SelectedBone && dots[hoverDot].Shown)
  {
    ImVec2 at(dots[hoverDot].Draw.x + fontSize * 0.5f, dots[hoverDot].Draw.y - fontSize * 1.2f);
    shadowedText(at, IM_COL32(255, 255, 255, 200), BoneName(h, hoverDot));
  }
  else if (hoverWeapon >= 0 && hoverWeapon != g_SelectedWeapon && hoverWeapon < h.WeaponCount && weaponShown[hoverWeapon])
  {
    ImVec2 at(weaponDraw[hoverWeapon].x + fontSize * 0.5f, weaponDraw[hoverWeapon].y - fontSize * 1.2f);
    shadowedText(at, IM_COL32(255, 255, 255, 200), h.Weapons[hoverWeapon].Name);
  }
}
