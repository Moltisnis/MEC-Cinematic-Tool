#include "FaithSize.h"
#include "../Main.h"
#include "../Util/Util.h"
#include <Windows.h>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <intrin.h>
#include <malloc.h>
#include <mutex>

namespace
{
  // The game build these offsets are from (the replay checks the same PE
  // timestamp)
  const DWORD KnownTimeStamp = 0x5762E3B7;

  // Faith's pose, found the way the replay finds it (FaithReplay.cpp): her
  // soldier's PamClientReplayRecordingComponent (+0x4B0) holds her drawn
  // ClientAntAnimatableEntity (+0x50) and the pose manager of her skeleton
  // (+0x98). Her box pose is the animatable at soldier + 0x340, another slot
  // of the same manager. A slot's pose object is at animatable + 0x158, its
  // handle at + 0x118 (byte 3 a page, byte 0 a slot); the manager's pages
  // (+0x210) hold the local pose (page + 0x00) and the world pose (+ 0x08),
  // 0x20 a bone (translation x y z, the bone's scale, rotation x y z w), the
  // skinning matrices (+ 0x10, 3x4 rows a bone: the world pose times the
  // inverse bind pose, there when manager + 0x2B9 is set) and one override per
  // slot (+ 0x30).
  const int SoldierRecordingComponentOffset = 0x4B0;
  const __int64 RecordingComponentVTableRva = 0x1C48CD0;
  const int ComponentAnimatableOffset = 0x50;
  const int ComponentManagerOffset = 0x98;
  const int SoldierBoxAnimatableOffset = 0x340;
  const __int64 AnimatableTypeInfoRva = 0x27EEE10;  // ClientAntAnimatableEntity
  const int AnimatablePoseOffset = 0x158;
  const int PoseManagerOffset = 0x1B8;
  const int PoseHandleOffset = 0x118;
  const int ManagerPoseStrideOffset = 0x8C;
  const int ManagerMatrixStrideOffset = 0x90;
  const int ManagerHasMatricesOffset = 0x2B9;
  const int ManagerPagesOffset = 0x210;
  const int PageLocalPoseOffset = 0x00;
  const int PageWorldPoseOffset = 0x08;
  const int PageMatricesOffset = 0x10;
  const int PageOverridesOffset = 0x30;
  const __int64 GameAllocatorRva = 0x2364F80;       // what the game's own override vectors carry
  const int FloatsPerBone = 8;
  const int MatrixFloats = 12;
  const int FaithBones = 182;                       // Skeleton_Faith
  const int RootBone = 1;                           // AITrajectory: everything she is hangs off it
  const int HipsBone = 4;                           // under bone 1
  // Bones 0 - 3 (Reference, AITrajectory, Trajectory, TrajectoryEnd) stay as
  // the game has them; every bone from the hips on is sized (the camera
  // joint, 179, too)
  const int FirstSizedBone = 4;
  const ULONGLONG SizeLogDelay = 1000;              // ms the slider has to rest before the new size is logged
  const ULONGLONG DiagnosticPeriod = 10000;         // ms between the timing lines in the log
  const int MaxRestoreFrames = 120;                 // bone 1's scale handed back as 1.00 at most this long
  const int CameraJointBone = 179;                  // CameraJoint: where her first-person camera is
  const float FirstPersonReach = 0.5f;              // m: the game's camera this near her camera joint is her first-person view
  const int RenderViewTransform0 = 0x00;            // the render view's camera transforms (the free camera writes both)
  const int RenderViewTransform1 = 0x40;
  // Her hair cloth, for the log (as CameraManager.cpp's hair code finds it):
  // her soldier's components (+0x38, 0x20 apart), her customization component,
  // its ClothEntity, the EA cloth instance (+0x68): + 0x100 its root place
  // (bone 1's world place, then a fourth float), + 0x160 / + 0x170 two
  // corners of its box (2026-11-03, live at size 1.00: her head bones 50 - 53)
  const __int64 CustomizationComponentVTableRva = 0x1C3C360;
  const __int64 ClothEntityVTableRva = 0x1AD2998;
  const int ClothEntityInstanceOffset = 0x68;
  const int ClothRootOffset = 0x100;
  const int ClothBoxOffset = 0x160;
  const int HeadEndBone = 53;
  // Each cloth's simulation (EA SimpleCloth): the cloth instance (vtable
  // 0x141AB6380) holds it at + 0x38; it holds its particles (+ 0x00, an array:
  // + 0x00 its data, 16 bytes a particle), its definition (+ 0x10,
  // ClothDefinition: + 0x84 its anchor count) and its solver (+ 0x60)
  const __int64 ClothInstanceVTableRva = 0x1AB6380;
  const __int64 ClothDefinitionVTableRva = 0x1CCC3B0;
  const int ClothSimulationOffset = 0x38;
  const int SimulationParticlesOffset = 0x00;
  const int SimulationDefinitionOffset = 0x10;
  const int SimulationSolverOffset = 0x60;
  const int DefinitionAnchorCountOffset = 0x84;
  const int ParticleFloats = 4;
  const int SimulationPreviousOffset = 0x08;        // its particles' places a step before (Verlet), an array like + 0x00
  const int SimulationParametersOffset = 0x68;      // what its step reads (0x143B76390): + 0x00 gravity, + 0x0C wind
  const int DefinitionParticleCountOffset = 0x80;
  const int ArrayCountOffset = 0x08;                // a particle array's room (live: 118 for her hair's 105)

  // The game's code this relies on, byte for byte
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

  // The game's override, laid out like the ghost player component's (+0x1D8)
  // and the replay's
  struct GameVector
  {
    __int64 Allocator;
    void* Data;
    unsigned int Count;
    unsigned int Capacity;
  };
  struct PoseOverride
  {
    float* Bones;           // 0x20 per listed bone
    GameVector* List;       // the skeleton's bones it holds
    GameVector* Map;        // per skeleton bone, its place in Bones (-1 for none)
    float Weight;
    unsigned int Count;
    int Handle;             // the slot it's for (the game sets -1 when it releases the slot)
    int Unused;
  };
  static_assert(offsetof(PoseOverride, Weight) == 0x18 && offsetof(PoseOverride, Count) == 0x1C &&
    offsetof(PoseOverride, Handle) == 0x20, "the game's layout");

  // One bone, bone 1; the map like the ghost's: every skeleton bone, -1 but
  // bone 1's
  unsigned int g_ListBones[1] = { RootBone };
  unsigned int g_MapBones[FaithBones];
  GameVector g_List{}, g_Map{};

  // Per slot: two overrides, filled in turn (one is written while the pose
  // job can still be copying the other)
  struct SlotState
  {
    __declspec(align(16)) float Buffer[2][FloatsPerBone];
    PoseOverride Override[2];
    int Next;
    __int64* Overrides;     // the page's table ours is in (0: not handed)
    unsigned int Handle;
    bool Refused;           // the slot held an override not ours (logged once)
  };
  SlotState g_Body{}, g_Box{};

  struct Slot
  {
    unsigned int Handle;
    float const* Local;     // her local pose (the slot's bones)
    float* World;           // her world pose
    float* Matrices;        // her skinning matrices (0: the manager keeps none)
    __int64* Overrides;
    __int64 Pose;           // its pose object (animatable + 0x158)
  };

  // Per slot: the world pose and matrices as the game wrote them, and as the
  // tools wrote them sized, bone by bone. A bone still holding what the tools
  // wrote isn't sized again (the game didn't write it since); one that differs
  // is the game's, taken as its own and sized.
  struct PoseState
  {
    float* World;           // the slot's buffers these are for (0: none)
    float* Matrices;
    float GameWorld[FaithBones][FloatsPerBone];
    float OurWorld[FaithBones][FloatsPerBone];
    float GameMatrices[FaithBones][MatrixFloats];
    float OurMatrices[FaithBones][MatrixFloats];
    bool HaveWorld[FaithBones];
    bool HaveMatrices[FaithBones];
  };
  PoseState g_BodyPose{}, g_BoxPose{};
  // This frame's copy of a slot's buffers (Update runs on one thread)
  float g_ReadWorld[FaithBones][FloatsPerBone];
  float g_ReadMatrices[FaithBones][MatrixFloats];

  // How often the game had written her body's buffers anew between two camera
  // hooks (the log's timing line, every DiagnosticPeriod)
  struct Timing
  {
    int Hooks;
    int WorldFresh, MatricesFresh;        // hooks with any bone the game's
    long long WorldBones, MatricesBones;  // the game's bones, summed
    int AlreadySized;                     // the game's buffers came sized (left alone)
    ULONGLONG Since;
    __int64 Soldier;                      // for the hair line
    float Size;
  };
  Timing g_Timing{};

  std::atomic<float> g_Size{ 1.f };
  std::once_flag g_Checked;
  bool g_Available = false;
  float g_LoggedSize = 1.f;         // the size the log last gave
  float g_SeenSize = 1.f;           // the size last frame
  ULONGLONG g_SizeChangedAt = 0;
  bool g_Sizing = false;            // her pose sized last frame
  bool g_ScaleDirty = false;        // bone 1's scale was handed something else than 1.00 since it last read 1.00
  int g_RestoreFrames = 0;
  bool g_LoggedNoSlot = false;
  bool g_LoggedNoBox = false;
  bool g_LoggedNoMatrices = false;
  float g_CameraWritten[3] = {};    // the first-person camera place the tools wrote last (not sized again)
  bool g_CameraHave = false;
  bool g_LoggedCamera = false;      // the first frame's camera distance from her camera joint logged

  bool CheckGame()
  {
    __int64 base = (__int64)g_gameHandle;
    __try
    {
      IMAGE_DOS_HEADER* pDos = (IMAGE_DOS_HEADER*)base;
      IMAGE_NT_HEADERS* pNt = (IMAGE_NT_HEADERS*)(base + pDos->e_lfanew);
      if (pNt->FileHeader.TimeDateStamp != KnownTimeStamp)
      {
        util::log::Warning("Faith's size: the game isn't the build the tools know (PE timestamp 0x%08X), GAME > Faith "
          "can't change her size", pNt->FileHeader.TimeDateStamp);
        return false;
      }
      for (CodeCheck const& check : CodeChecks)
        if (memcmp((void const*)(base + check.Rva), check.Bytes, check.Size) != 0)
        {
          util::log::Warning("Faith's size: the game's pose code at 0x%I64X (%s) isn't what the tools know, GAME > "
            "Faith can't change her size", 0x140000000 + check.Rva, check.What);
          return false;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      util::log::Warning("Faith's size: the game's code couldn't be read, GAME > Faith can't change her size");
      return false;
    }
    __int64 allocator = base + GameAllocatorRva;
    for (int i = 0; i < FaithBones; ++i)
      g_MapBones[i] = 0xFFFFFFFFu;
    g_MapBones[RootBone] = 0;
    g_List = { allocator, g_ListBones, 1, 1 };
    g_Map = { allocator, g_MapBones, (unsigned int)FaithBones, (unsigned int)FaithBones };
    return true;
  }

  // What an object's getType gives (vtable [0]: lea rax, [rip + x]; ret)
  __int64 TypeOf(__int64 object)
  {
    __int64 vtable = *(__int64*)object;
    BYTE const* code = vtable ? *(BYTE const**)vtable : nullptr;
    if (!code || code[0] != 0x48 || code[1] != 0x8D || code[2] != 0x05)
      return 0;
    return (__int64)code + 7 + *(int const*)(code + 3);
  }

  // An animatable's slot in her skeleton's pose manager, checked all the way.
  // Game memory: only inside the caller's __try.
  bool SlotOf(__int64 manager, __int64 animatable, Slot& slot)
  {
    __int64 base = (__int64)g_gameHandle;
    if (animatable < 0x10000 || TypeOf(animatable) != base + AnimatableTypeInfoRva)
      return false;
    __int64 pose = *(__int64*)(animatable + AnimatablePoseOffset);
    if (pose < 0x10000 || *(__int64*)(pose + PoseManagerOffset) != manager)
      return false;
    unsigned int handle = *(unsigned int*)(pose + PoseHandleOffset);
    int stride = *(int*)(manager + ManagerPoseStrideOffset);
    int matrixStride = *(int*)(manager + ManagerMatrixStrideOffset);
    if (handle == 0xFFFFFFFF || stride != FaithBones * FloatsPerBone * (int)sizeof(float))
      return false;
    __int64* pagesBegin = *(__int64**)(manager + ManagerPagesOffset);
    __int64* pagesEnd = *(__int64**)(manager + ManagerPagesOffset + 8);
    unsigned int page = handle >> 24, index = handle & 0xFF;
    if (!pagesBegin || pagesEnd < pagesBegin || page >= (unsigned int)(pagesEnd - pagesBegin))
      return false;
    __int64 pPage = pagesBegin[page];
    if (pPage < 0x10000)
      return false;
    __int64 local = *(__int64*)(pPage + PageLocalPoseOffset);
    __int64 world = *(__int64*)(pPage + PageWorldPoseOffset);
    __int64 matrices = *(__int64*)(pPage + PageMatricesOffset);
    __int64 overrides = *(__int64*)(pPage + PageOverridesOffset);
    if (local < 0x10000 || world < 0x10000 || overrides < 0x10000)
      return false;
    bool hasMatrices = *(BYTE*)(manager + ManagerHasMatricesOffset) && matrices >= 0x10000 &&
      matrixStride == FaithBones * MatrixFloats * (int)sizeof(float);
    slot.Handle = handle;
    slot.Local = (float const*)(local + (__int64)index * stride);
    slot.World = (float*)(world + (__int64)index * stride);
    slot.Matrices = hasMatrices ? (float*)(matrices + (__int64)index * matrixStride) : nullptr;
    slot.Overrides = (__int64*)overrides;
    slot.Pose = pose;
    return true;
  }

  // Her body's slot and her box pose's (another slot of the same manager)
  void FindSlots(__int64 soldier, Slot& body, bool& bodyOk, Slot& box, bool& boxOk)
  {
    __int64 base = (__int64)g_gameHandle;
    bodyOk = boxOk = false;
    __try
    {
      __int64 component = *(__int64*)(soldier + SoldierRecordingComponentOffset);
      if (component < 0x10000 || *(__int64*)component != base + RecordingComponentVTableRva)
        return;
      __int64 manager = *(__int64*)(component + ComponentManagerOffset);
      if (manager < 0x10000)
        return;
      bodyOk = SlotOf(manager, *(__int64*)(component + ComponentAnimatableOffset), body);
      if (bodyOk)
        boxOk = SlotOf(manager, *(__int64*)(soldier + SoldierBoxAnimatableOffset), box) && box.Handle != body.Handle;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      bodyOk = boxOk = false;
    }
  }

  // ---- Her body: the world pose and skinning matrices sized about bone 1 ----
  //
  // The game builds her world pose and skinning matrices from her local
  // pose's translations and rotations only: its bone scale (what the
  // override below sets) doesn't reach them (2026-11-03 12:3x, live with
  // bone 1's scale 0.50: her hips 0.870 m from bone 1, the length of their
  // local offset, every matrix row of length 1.0000). So they're sized here,
  // each frame from the camera hook, about bone 1's world place: each sized
  // bone's world place p1 + s (p - p1); each matrix's rotation rows times s
  // and its translation p1 + s (t - p1). Rotations stay.

  bool Finite(float const* v, int n)
  {
    for (int i = 0; i < n; ++i)
      if (!std::isfinite(v[i]))
        return false;
    return true;
  }

  float RowLength(float const* m, int row)
  {
    return sqrtf(m[row * 4] * m[row * 4] + m[row * 4 + 1] * m[row * 4 + 1] + m[row * 4 + 2] * m[row * 4 + 2]);
  }

  void ResetPose(PoseState& state, Slot const* slot)
  {
    state.World = slot ? slot->World : nullptr;
    state.Matrices = slot ? slot->Matrices : nullptr;
    memset(state.HaveWorld, 0, sizeof(state.HaveWorld));
    memset(state.HaveMatrices, 0, sizeof(state.HaveMatrices));
  }

  bool ReadPose(Slot const& slot)
  {
    __try
    {
      memcpy(g_ReadWorld, slot.World, sizeof(g_ReadWorld));
      if (slot.Matrices)
        memcpy(g_ReadMatrices, slot.Matrices, sizeof(g_ReadMatrices));
      return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
  }

  bool WriteBone(float* target, float const* source, size_t bytes)
  {
    __try
    {
      memcpy(target, source, bytes);
      return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
  }

  // Sizes one slot's buffers; counts into the timing if it's her body's
  void SizePose(PoseState& state, Slot const& slot, float size, bool body)
  {
    if (state.World != slot.World || state.Matrices != slot.Matrices)
      ResetPose(state, &slot);
    if (!ReadPose(slot))
      return;
    float const* root = g_ReadWorld[RootBone];
    if (!Finite(root, 3))
      return;
    float p[3] = { root[0], root[1], root[2] };

    // The game's world pose ignores the scale: a hips place it wrote is its
    // local offset away from bone 1. One nearer the sized distance than that
    // (written sized by something, or torn) is left alone this frame.
    bool hipsOurs = state.HaveWorld[HipsBone] && !memcmp(g_ReadWorld[HipsBone], state.OurWorld[HipsBone],
      sizeof(state.OurWorld[HipsBone]));
    bool worldOk = true;
    if (!hipsOurs)
    {
      float local = 0.f;
      __try
      {
        float const* l = slot.Local + HipsBone * FloatsPerBone;
        local = sqrtf(l[0] * l[0] + l[1] * l[1] + l[2] * l[2]);
      }
      __except (EXCEPTION_EXECUTE_HANDLER)
      {
        local = -1.f;
      }
      float const* h = g_ReadWorld[HipsBone];
      float dx = h[0] - p[0], dy = h[1] - p[1], dz = h[2] - p[2];
      float distance = sqrtf(dx * dx + dy * dy + dz * dz);
      worldOk = local > 0.1f && fabsf(distance - local) < fabsf(distance - local * size);
    }

    int worldFresh = 0, matricesFresh = 0, alreadySized = 0;
    for (int i = FirstSizedBone; i < FaithBones; ++i)
    {
      // World pose
      float* read = g_ReadWorld[i];
      bool ours = state.HaveWorld[i] && !memcmp(read, state.OurWorld[i], sizeof(state.OurWorld[i]));
      if (!ours)
      {
        if (worldOk && Finite(read, FloatsPerBone))
        {
          memcpy(state.GameWorld[i], read, sizeof(state.GameWorld[i]));
          state.HaveWorld[i] = true;
          ++worldFresh;
        }
        else
        {
          state.HaveWorld[i] = false;
          if (!worldOk)
            ++alreadySized;
        }
      }
      if (state.HaveWorld[i])
      {
        float* our = state.OurWorld[i];
        float const* game = state.GameWorld[i];
        memcpy(our, game, sizeof(state.OurWorld[i]));
        for (int k = 0; k < 3; ++k)
          our[k] = p[k] + size * (game[k] - p[k]);
        if (memcmp(read, our, sizeof(state.OurWorld[i])) && !WriteBone(slot.World + i * FloatsPerBone, our,
          sizeof(state.OurWorld[i])))
          state.HaveWorld[i] = false;
      }

      // Skinning matrix
      if (!slot.Matrices)
        continue;
      float* m = g_ReadMatrices[i];
      ours = state.HaveMatrices[i] && !memcmp(m, state.OurMatrices[i], sizeof(state.OurMatrices[i]));
      if (!ours)
      {
        // The game's are rotations: rows nearer length 1 than the size
        float row0 = RowLength(m, 0), row1 = RowLength(m, 1);
        if (Finite(m, MatrixFloats) && fabsf(row0 - 1.f) < fabsf(row0 - size) && fabsf(row1 - 1.f) < fabsf(row1 - size))
        {
          memcpy(state.GameMatrices[i], m, sizeof(state.GameMatrices[i]));
          state.HaveMatrices[i] = true;
          ++matricesFresh;
        }
        else
        {
          state.HaveMatrices[i] = false;
          ++alreadySized;
        }
      }
      if (state.HaveMatrices[i])
      {
        float* our = state.OurMatrices[i];
        float const* game = state.GameMatrices[i];
        for (int r = 0; r < 3; ++r)
        {
          for (int c = 0; c < 3; ++c)
            our[r * 4 + c] = game[r * 4 + c] * size;
          our[r * 4 + 3] = p[r] + size * (game[r * 4 + 3] - p[r]);
        }
        if (memcmp(m, our, sizeof(state.OurMatrices[i])) && !WriteBone(slot.Matrices + i * MatrixFloats, our,
          sizeof(state.OurMatrices[i])))
          state.HaveMatrices[i] = false;
      }
    }

    if (body)
    {
      ++g_Timing.Hooks;
      if (worldFresh)
        ++g_Timing.WorldFresh;
      if (matricesFresh)
        ++g_Timing.MatricesFresh;
      g_Timing.WorldBones += worldFresh;
      g_Timing.MatricesBones += matricesFresh;
      g_Timing.AlreadySized += alreadySized ? 1 : 0;
    }
  }

  // The game's own values back in every bone that still holds what the tools
  // wrote (the game rewrites the others itself); nothing written if the slot
  // isn't the one they're for
  int PutBackPose(PoseState& state, Slot const* slot)
  {
    int putBack = 0;
    if (slot && state.World == slot->World && state.Matrices == slot->Matrices && ReadPose(*slot))
      for (int i = FirstSizedBone; i < FaithBones; ++i)
      {
        if (state.HaveWorld[i] && !memcmp(g_ReadWorld[i], state.OurWorld[i], sizeof(state.OurWorld[i])) &&
          WriteBone(slot->World + i * FloatsPerBone, state.GameWorld[i], sizeof(state.GameWorld[i])))
          ++putBack;
        if (slot->Matrices && state.HaveMatrices[i] &&
          !memcmp(g_ReadMatrices[i], state.OurMatrices[i], sizeof(state.OurMatrices[i])) &&
          WriteBone(slot->Matrices + i * MatrixFloats, state.GameMatrices[i], sizeof(state.GameMatrices[i])))
          ++putBack;
      }
    ResetPose(state, nullptr);
    return putBack;
  }

  // ---- Her hair: bone 1's scale through the game's pose override ----
  //
  // The game's own pose override writes its 4th translation float as the
  // listed bone's scale whatever its weight (0x142FCFA5A); rotation and
  // translation are blended by the weight. Her hair cloth follows that scale
  // (2026-11-03, the user's test: "only her hair"); her world pose and
  // matrices don't (sized above instead). The game never writes that scale
  // back itself (live: 0.50 left in her local and world poses after the
  // override was taken back), so going back to 1.00 hands 1.00 until her
  // local pose reads 1.00 again.

  bool IsOurs(SlotState const& state, __int64 entry)
  {
    return entry == (__int64)&state.Override[0] || entry == (__int64)&state.Override[1];
  }

  // Ours out of the slot it was handed to, if it's still there (the game
  // clears it itself when it releases the slot, at a level load or when she's
  // gone)
  void Take(SlotState& state)
  {
    if (state.Overrides)
    {
      __try
      {
        LONG64 volatile* entry = (LONG64 volatile*)&state.Overrides[state.Handle & 0xFF];
        LONG64 now = *entry;
        if (IsOurs(state, now))
          InterlockedCompareExchange64(entry, 0, now);
      }
      __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
    state.Overrides = nullptr;
    state.Override[0].Handle = state.Override[1].Handle = -1;
  }

  // Bone 1 with the scale and weight 0 into the slot: the game's blend keeps
  // her animated translation and rotation and takes the scale as the bone's.
  // The rotation given is bone 1's own from her last pose, so the blend keeps
  // it on the same side as before (at weight 0 the result is her animation's
  // rotation either way). False if the slot holds another override (the
  // replay's) or can't be written.
  bool Hand(SlotState& state, Slot const& slot, float scale)
  {
    float rotation[4] = { 0.f, 0.f, 0.f, 1.f };
    __try
    {
      LONG64 now = *(LONG64 volatile*)&slot.Overrides[slot.Handle & 0xFF];
      if (now != 0 && !IsOurs(state, now))
        return false;
      memcpy(rotation, slot.Local + RootBone * FloatsPerBone + 4, sizeof(rotation));
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
    float lengthSq = rotation[0] * rotation[0] + rotation[1] * rotation[1] + rotation[2] * rotation[2] +
      rotation[3] * rotation[3];
    if (!(lengthSq > 0.9f && lengthSq < 1.1f))
    {
      rotation[0] = rotation[1] = rotation[2] = 0.f;
      rotation[3] = 1.f;
    }
    int k = state.Next;
    state.Next ^= 1;
    float* bone = state.Buffer[k];
    bone[0] = bone[1] = bone[2] = 0.f;
    bone[3] = scale;
    memcpy(bone + 4, rotation, sizeof(rotation));
    PoseOverride& o = state.Override[k];
    o = { bone, &g_List, &g_Map, 0.f, 1u, (int)slot.Handle, 0 };
    __try
    {
      InterlockedExchange64((LONG64 volatile*)&slot.Overrides[slot.Handle & 0xFF], (LONG64)&o);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      o.Handle = -1;
      return false;
    }
    state.Overrides = slot.Overrides;
    state.Handle = slot.Handle;
    state.Refused = false;
    return true;
  }

  // Ours taken back from a slot that's gone or moved. The one in the slot is
  // the one handed last (Next was flipped past it); its handle -1 means the
  // game released the slot.
  void Drop(SlotState& state, bool wanted, bool ok, Slot const& slot)
  {
    if (!state.Overrides)
      return;
    bool same = wanted && ok && state.Overrides == slot.Overrides && state.Handle == slot.Handle &&
      state.Override[state.Next ^ 1].Handle == (int)state.Handle;
    if (!same)
      Take(state);
  }

  // Bone 1's scale in a slot's local pose (-1: not readable)
  float RootScale(Slot const& slot)
  {
    __try
    {
      return slot.Local[RootBone * FloatsPerBone + 3];
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return -1.f;
    }
  }

  // ---- Her first-person view: the game's camera moved with her ----
  //
  // The game places its camera on her camera joint (bone 179) before the
  // camera hook, from her world pose as it wrote it (unsized). With the free
  // camera off, a game camera that near her camera joint is moved as the
  // joint is: bone 1's place + size x (its place - bone 1's place), in both of
  // the render view's transforms. Its rotation stays. Third-person cameras
  // (cutscenes, finishers) are further away and left alone.
  void SizeCamera(__int64 renderView, float size)
  {
    PoseState const& state = g_BodyPose;
    if (!renderView || !state.HaveWorld[CameraJointBone])
    {
      g_CameraHave = false;
      return;
    }
    float const* joint = state.GameWorld[CameraJointBone];
    float root[3];
    float camera[3];
    memcpy(root, g_ReadWorld[RootBone], sizeof(root));
    __try
    {
      memcpy(camera, (float const*)(renderView + RenderViewTransform1) + 12, sizeof(camera));
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return;
    }
    if (!Finite(camera, 3) || !Finite(root, 3))
      return;
    if (g_CameraHave && !memcmp(camera, g_CameraWritten, sizeof(camera)))
      return;   // what the tools wrote: the game didn't write its camera since
    float dx = camera[0] - joint[0], dy = camera[1] - joint[1], dz = camera[2] - joint[2];
    float distance = sqrtf(dx * dx + dy * dy + dz * dz);
    if (!g_LoggedCamera)
    {
      util::log::Write("Faith's size: the game's camera is %.3f m from her camera joint (%s)", distance,
        distance < FirstPersonReach ? "her first-person view: moved with her" : "not her first-person view: left alone");
      g_LoggedCamera = true;
    }
    if (distance >= FirstPersonReach)
    {
      g_CameraHave = false;
      return;
    }
    float moved[3];
    for (int k = 0; k < 3; ++k)
      moved[k] = root[k] + size * (camera[k] - root[k]);
    __try
    {
      memcpy((float*)(renderView + RenderViewTransform0) + 12, moved, sizeof(moved));
      memcpy((float*)(renderView + RenderViewTransform1) + 12, moved, sizeof(moved));
      memcpy(g_CameraWritten, moved, sizeof(moved));
      g_CameraHave = true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      g_CameraHave = false;
    }
  }

  // ---- Her hair: what its cloth holds, for the log ----
  __int64 ReadQword(__int64 address)
  {
    if (address < 0x10000 || address > 0x7FFFFFFF0000)
      return 0;
    __try
    {
      return *(__int64 const*)address;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return 0;
    }
  }

  __int64 FindHairClothInstance(__int64 soldier)
  {
    __int64 base = (__int64)g_gameHandle;
    __int64 collection = ReadQword(soldier + 0x38);
    if (!collection)
      return 0;
    for (int entry = 0; entry < 32; ++entry)
    {
      __int64 component = ReadQword(collection + 0x20 + entry * 0x20);
      if (!component || ReadQword(component) != base + CustomizationComponentVTableRva)
        continue;
      for (int off = 0; off < 0x800; off += 8)
      {
        __int64 cloth = ReadQword(component + off);
        if (cloth && cloth != component && ReadQword(cloth) == base + ClothEntityVTableRva)
          return ReadQword(cloth + ClothEntityInstanceOffset);
      }
      return 0;
    }
    return 0;
  }

  bool ReadClothFloats(__int64 instance, float* root, float* box)
  {
    __try
    {
      memcpy(root, (float const*)(instance + ClothRootOffset), 4 * sizeof(float));
      memcpy(box, (float const*)(instance + ClothBoxOffset), 8 * sizeof(float));
      return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
  }

  // ---- Her hair at her size: its cloth's anchors and lengths ----
  //
  // Her hair is EA SimpleCloth (Research_Faith_Size_2026-11-03.md section 6.5:
  // decompiled and read live, 2026-11-03). Two things kept it its normal size:
  // - Its anchors. The sim job (0x142FA07A0 -> 0x142FA5020) makes the
  //   cloth's palette: each bone's inverse bind (manager + 0xA8, rows of
  //   length 1) times its world matrix, which it gets one of two ways, picked
  //   by its pose provider's slot 8 (vtable 0x141AD2AB0; the pose's + 0x12D):
  //   - Set (hers, live 2026-11-03): 0x142FA45D0 chains the matrices itself
  //     from her local pose. It starts from the cloth's root (bone 1's world
  //     place, its rows times bone 1's own scale: the size), then for each
  //     bone takes its local record as a matrix (slot 0 -> 0x142FD46A0: rows
  //     times the record's scale) times its parent's (slot 3), and hands each
  //     to 0x142FA4950 for its palette entry. Replayed from her live records
  //     at size 10, that gave the game's palette to within 0.0001. But her
  //     local pose has a scale of 1 / size on bone 7 (her spine) and ten more
  //     (40, 46, 47, 49, 112, 148, 155, 156, 158, 159: 0.1 at size 10), which
  //     undoes the size from there on: in the chain her hips were sized but
  //     her spine, neck, head and the hair's anchors were their normal size on
  //     top, so the hair floated off a small head and sank into a big one.
  //   - Not set: 0x142FA42C0 asks the provider's slot 1 (0x14304E7A0 ->
  //     0x142FD2B00) for each bone's matrix from her world pose: the
  //     rotation from the bone's quaternion, each row times the pose's 4th
  //     float (1), and the bone's place plus bone 0's.
  //   Either way, for a bone of her body's pose from the hips on, the cloth
  //   gets her world matrix as the drawn body has it: the game's own (slot 1,
  //   from her world pose), its rows times the size and its place sized about
  //   bone 1 (as the tools size her world pose, if the game's place is still
  //   there). Live at size 10, a bone's inverse bind times that matrix was her
  //   drawn body's skinning matrix to within 0.00004. The provider's vtable
  //   is set only in ClothEntity's two constructors (0x143049200,
  //   0x143049BD0): other characters' cloths go through as they came.
  // - Its lengths. Its solver points at its definition's arrays (set once by
  //   0x143B781C0; live, the same addresses) and its step (0x143B7BDF0) reads
  //   them every pass, in metres: the constraints' rest lengths (8 bytes each:
  //   two u16 particles, the float), the anchors' and anchor planes'
  //   distances, the particles' radii and the fixed distances. So the solver
  //   of each cloth of hers is pointed at the tools' copies, every length
  //   times the size, and back at the definition's at 1.00. The definition
  //   (any cloth made from it shares it) is never written. Each pointer is one
  //   aligned 8-byte store and the step loads it once a pass: a pass reads one
  //   array whole, the game's or the tools'. There are two copies, and the one
  //   not pointed at is the one filled.
  // - Its gravity and wind, in metres too (an acceleration, a speed). The
  //   step recomputes its gravity per step from the vector at the cloth's
  //   simulation + 0x68 every step (0x143B76390: gravity x dt x dt, live
  //   -8.8375 x (1/30)^2 = -0.0098194), and its wind from the vector at
  //   + 0x74, which the game writes anew as the wind changes. Left at the
  //   game's, a big Faith's hair floated as in low gravity and flew about (its
  //   box 26.6 - 90.0 m at size 10), a small one's was pulled by a gravity a
  //   hundred times too strong for it. So both are times the size, before
  //   each step, and the game's are given back at 1.00.
  // - Its particles on a size change. They kept their places, so the strands
  //   were dragged to the new size over seconds (a second after 0.01 -> 10
  //   the box still spanned 63 m; 10 s later it was 3 m). So when the size
  //   changes, each particle's place and its place a step before are moved
  //   as her body is, about bone 1, by the new size over the old.
  // Left as they are: its root (it carries bone 1's scale already), the
  // particles' weights.
  const __int64 ClothProviderVTableRva = 0x1AD2AB0;
  const int ClothEntityProviderOffset = 0x40;       // a ClothEntity's pose provider
  const __int64 ClothBoneMatrixRva = 0x304E7A0;     // the provider's slot 1
  const __int64 BoneMatrixRva = 0x2FD2B00;
  const int ProviderAnimatableOffset = 0x58;        // the provider -> its animatable's sub-object (animatable + 0x60)
  const int SubPoseOffset = 0xF8;                   // ... -> the animatable's pose object (animatable + 0x158)
  const int PoseWorldOffset = 0x50;                 // a pose object's world pose (its slot of page + 0x08)
  const int ClothBoneMatrixBytes = 19;              // the provider's slot 1, its jump included
  const __int64 ClothBonePaletteRva = 0x2FA4950;    // a palette entry from the chain's matrix
  const int ClothInstanceProviderOffset = 0x30;     // the cloth instance's pose provider
  const int MaxHairCloths = 4;
  const int MaxHairEntries = 4096;
  const int MaxHairCopySets = 8;

  const CodeCheck HairCodeChecks[] = {
    // The provider's slot 1: mov rcx, [rcx + 0x58]; mov r9b, 1; mov rcx,
    // [rcx + 0xF8]; jmp (to 0x142FD2B00, checked apart)
    { ClothBoneMatrixRva, "the cloth's bone matrix", { 0x48, 0x8B, 0x49, 0x58, 0x41, 0xB1, 0x01, 0x48, 0x8B, 0x89, 0xF8,
      0x00, 0x00, 0x00, 0xE9 }, 15 },
    { BoneMatrixRva, "the bone matrix's start", { 0x48, 0x83, 0xEC, 0x38, 0x80, 0x79, 0x68, 0x00, 0x41, 0x89, 0xD2, 0x75,
      0x36, 0x4D, 0x85, 0xC0 }, 16 },
    // mov rdx, [rcx + 0x50]: the pose object's world pose
    { 0x2FD2B43, "the bone matrix's world pose", { 0x48, 0x8B, 0x51, 0x50 }, 4 },
    // The bone's place into the matrix, its rows times the place's 4th float
    { 0x2FD2C48, "the bone matrix's rows", { 0x0F, 0x28, 0x04, 0x10, 0x41, 0x0F, 0x29, 0x40, 0x30, 0x66, 0x0F, 0x70, 0x0C,
      0x10, 0xFF, 0x0F, 0x59, 0xF9, 0x0F, 0x59, 0xD9, 0x0F, 0x59, 0xD1 }, 24 },
    // Bone 0's place added to every other bone's (r9b set)
    { 0x2FD2C73, "the bone matrix's bone 0", { 0x45, 0x85, 0xD2, 0x0F, 0x95, 0xD1, 0x41, 0x84, 0xC9, 0x74, 0x0D, 0x0F, 0x28,
      0x02, 0x41, 0x0F, 0x58, 0x40, 0x30, 0x41, 0x0F, 0x29, 0x40, 0x30 }, 24 },
    // The cloth step: rsi = rcx (the solver), lea rbp, [rcx + 0xA8] (its counts)
    { 0x3B7BE3B, "the cloth step's counts", { 0x48, 0x8D, 0xA9, 0xA8, 0x00, 0x00, 0x00 }, 7 },
    // 0x142FA5020: the provider's slot 8 picks the chain (call 0x142FA45D0)
    // or the world pose's matrices (call 0x142FA42C0)
    { 0x2FA513B, "the cloth's choice of bone matrices", { 0x48, 0x8B, 0x4B, 0x30, 0x48, 0x8B, 0x01, 0xFF, 0x50, 0x40,
      0x48, 0x89, 0xD9, 0x84, 0xC0, 0x74, 0x08, 0xE8, 0x7F, 0xF4, 0xFF, 0xFF, 0xEB, 0x06, 0x00, 0xE8, 0x67, 0xF1 }, 28 },
    // The chain's palette loop: mov r8d, ebx; shl r8, 6; add r8, [rsp + 0x30]
    // (the bone's matrix in the chain); mov edx, ebx; mov rcx, rsi; call
    // 0x142FA4950
    { 0x2FA48BF, "the chain's palette loop", { 0x41, 0x89, 0xD8, 0x49, 0xC1, 0xE0, 0x06, 0x4C, 0x03, 0x44, 0x24, 0x30,
      0x89, 0xDA, 0x48, 0x89, 0xF1, 0xE8, 0x7B, 0x00, 0x00, 0x00 }, 22 },
    // 0x142FA4950: mov rax, rsp; mov [rax + 0x18], rbx; mov [rax + 0x20], rsi;
    // push rdi; sub rsp, 0x120 (it reads x y z of the matrix's four rows)
    { ClothBonePaletteRva, "the chain's palette entry", { 0x48, 0x89, 0xE0, 0x48, 0x89, 0x58, 0x18, 0x48, 0x89, 0x70,
      0x20, 0x57, 0x48, 0x81, 0xEC, 0x20, 0x01, 0x00, 0x00 }, 19 } };

  // One of a solver's arrays: where its definition keeps it, the count the
  // setup takes from there, the solver's pointer, count and size in bytes, and
  // the step's read of that pointer (rsi the solver). Live, her hair's
  // (2026-11-03): structure 164, shear 156, fold 168, long-range 84, anchor
  // distances and planes 104, radii 84, fixed distances 104.
  struct HairArray
  {
    const char* Name;
    int Definition;         // its array object there: + 0x00 data, + 0x08 count, + 0x0C count rounded up to 4
    int DefinitionCount;
    int Solver;
    int Count;              // 0: only its size in bytes
    int Bytes;
    int Stride;             // 8: a constraint (two u16 particles, a float rest length); 4: a float
    CodeCheck Read;
  };
  const HairArray HairArrays[] = {
    { "structure", 0xA0, 0x0C, 0x28, 0x12C, 0x1B0, 8, { 0x3B7C0C8, "its structure read", { 0x4C, 0x8B, 0x46, 0x28 }, 4 } },
    { "shear", 0xA8, 0x0C, 0x30, 0x130, 0x1B4, 8, { 0x3B7C089, "its shear read", { 0x4C, 0x8B, 0x46, 0x30 }, 4 } },
    { "fold", 0xB0, 0x0C, 0x38, 0x134, 0x1B8, 8, { 0x3B7C04A, "its fold read", { 0x4C, 0x8B, 0x46, 0x38 }, 4 } },
    { "long-range", 0xC8, 0x0C, 0x50, 0x138, 0x1BC, 8, { 0x3B7C2CF, "its long-range read", { 0x4C, 0x8B, 0x46, 0x50 },
      4 } },
    { "anchor distance", 0xB8, 0x0C, 0x40, 0x13C, 0x1A8, 4, { 0x3B7C1D6, "its anchor distances' read", { 0x48, 0x8B,
      0x46, 0x40 }, 4 } },
    { "anchor plane", 0xC0, 0x0C, 0x48, 0x140, 0x1AC, 4, { 0x3B7C354, "its anchor planes' read", { 0x48, 0x8B, 0x46,
      0x48 }, 4 } },
    { "radius", 0x100, 0x08, 0x88, 0, 0x1D4, 4, { 0x3B7C388, "its radii's read", { 0x48, 0x8B, 0x86, 0x88, 0x00, 0x00,
      0x00 }, 7 } },
    { "fixed distance", 0x108, 0x08, 0x98, 0, 0x1D8, 4, { 0x3B7C1CA, "its fixed distances' read", { 0x48, 0x8B, 0x86,
      0x98, 0x00, 0x00, 0x00 }, 7 } } };
  const int HairArrayCount = sizeof(HairArrays) / sizeof(HairArrays[0]);

  // The tools' copies for one definition: two of each array (the one not
  // pointed at is filled), what each holds; never freed, a solver may still
  // point at them
  struct HairCopies
  {
    __int64 Definition;
    int Count[HairArrayCount];
    BYTE* Data[2][HairArrayCount];
    float Size[2];          // 0: nothing yet
  };
  HairCopies g_HairCopies[MaxHairCopySets];
  int g_HairCopyCount = 0;

  struct HairCloth
  {
    __int64 Cloth, Simulation, Definition, Solver;
  };
  // A cloth of hers whose solver the tools have pointed
  struct HairState
  {
    HairCloth Cloth;                  // 0: none
    HairCopies* Copies;
    __int64 Game[HairArrayCount];     // its definition's arrays (what the solver had)
    int Pointed;                      // the copy pointed at last (-1: the game's)
    int PutBack;                      // since the log: pointers the game had put back to its own
    int Odd;                          // since the log: pointers neither the game's nor the tools' (left)
  };
  HairState g_HairStates[MaxHairCloths];
  __int64 g_HairRefusedSolver = 0;    // logged once

  typedef void(__fastcall* tClothBoneMatrix)(__int64 provider, unsigned int bone, float* out);
  tClothBoneMatrix oClothBoneMatrix = nullptr;
  typedef void(__fastcall* tClothBonePalette)(__int64 cloth, unsigned int bone, float* chain);
  tClothBonePalette oClothBonePalette = nullptr;
  bool g_HairHooked = false;
  std::atomic<bool> g_HairOn{ true };               // the menu's tick
  // For the hook, from Update: her body's pose object and world pose, the
  // places the tools wrote her sized bones with (two copies: Update fills the
  // one not shown), and the size (stored last; 1: nothing scaled)
  std::atomic<__int64> g_HairPose{ 0 };
  std::atomic<__int64> g_HairWorld{ 0 };
  std::atomic<float> g_HairSize{ 1.f };
  float g_HairPlaces[2][FaithBones][3];
  std::atomic<int> g_HairPlacesShown{ 0 };
  std::atomic<long long> g_HairRowsScaled{ 0 };     // her bones' matrices sized, their places the tools'
  std::atomic<long long> g_HairPlacesSized{ 0 };    // ... their places the game's, sized here
  std::atomic<long long> g_HairRowsLeft{ 0 };       // her bones' matrices made from another place (left)
  std::atomic<long long> g_HairOtherWorld{ 0 };     // her pose object with another world pose than her slot's
  std::atomic<long long> g_HairChained{ 0 };        // of the sized ones, the ones handed to the chain's palette

  // Per cloth of hers, before each of its steps (from the hooks below, in
  // the cloth's own sim job: the palette is made, 0x142FA5020, then it steps,
  // 0x143B7C5A0): the game's gravity and wind and what the tools wrote, and
  // the size its particles were last moved to
  struct HairStep
  {
    __int64 Cloth;                    // 0: none
    float Gravity[3], Wind[3];        // the game's
    float GravityOurs[3], WindOurs[3];
    bool Wrote;
    float ParticlesAt;
    ULONGLONG Seen;
  };
  std::mutex g_HairStepMutex;
  HairStep g_HairSteps[MaxHairCloths];
  std::atomic<long long> g_HairSnaps{ 0 };
  std::atomic<long long> g_HairSnapsRefused{ 0 };
  float g_HairGravityShown = 0.f;     // the gravity's y last written to a cloth of hers (for the log)

  bool IsHerPose(__int64 provider)
  {
    __int64 pose = g_HairPose.load(std::memory_order_relaxed);
    return pose && ReadQword(ReadQword(provider + ProviderAnimatableOffset) + SubPoseOffset) == pose;
  }

  // A matrix the game's bone matrix function (0x142FD2B00, the provider's
  // slot 1) has just made from the provider's pose, made her drawn body's:
  // its rows times the size, and its place sized about bone 1 (p1 + size x
  // (p - p1), as the tools size her world pose) unless it's the place the
  // tools wrote already. False, the matrix left as the game made it, for
  // another pose, another world pose, or a matrix made from another place
  // than the bone's now.
  bool SizeGameMatrix(__int64 provider, unsigned int bone, float size, float* out)
  {
    if (!IsHerPose(provider))
      return false;
    __int64 pose = g_HairPose.load(std::memory_order_relaxed);
    float place[3], first[3], zero[3];
    __try
    {
      float const* world = *(float const* const*)(pose + PoseWorldOffset);
      if ((__int64)world != g_HairWorld.load(std::memory_order_relaxed))
      {
        g_HairOtherWorld.fetch_add(1, std::memory_order_relaxed);
        return false;
      }
      memcpy(place, world + bone * FloatsPerBone, sizeof(place));
      memcpy(first, world + RootBone * FloatsPerBone, sizeof(first));
      memcpy(zero, world, sizeof(zero));
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
    // The matrix's place is the bone's plus bone 0's (0x142FD2C7E): one the
    // bone's place doesn't give was made from another
    bool same = Finite(first, 3);
    for (int k = 0; k < 3; ++k)
      same = same && out[12 + k] == zero[k] + place[k] && Finite(out + k * 4, 3);
    if (!same)
    {
      g_HairRowsLeft.fetch_add(1, std::memory_order_relaxed);
      return false;
    }
    float const* ours = g_HairPlaces[g_HairPlacesShown.load(std::memory_order_acquire)][bone];
    bool sized = place[0] == ours[0] && place[1] == ours[1] && place[2] == ours[2];
    for (int i = 0; i < 12; ++i)
      out[i] *= size;
    if (!sized)
      for (int k = 0; k < 3; ++k)
        out[12 + k] = zero[k] + first[k] + size * (place[k] - first[k]);
    (sized ? g_HairRowsScaled : g_HairPlacesSized).fetch_add(1, std::memory_order_relaxed);
    return true;
  }

  // Its gravity and wind times the size (values that aren't what the tools
  // wrote last are the game's). False if they can't be read.
  bool SizeHairForces(HairStep& s, __int64 simulation, float size)
  {
    float* parameters = (float*)(simulation + SimulationParametersOffset);
    __try
    {
      float gravity[3], wind[3];
      memcpy(gravity, parameters, sizeof(gravity));
      memcpy(wind, parameters + 3, sizeof(wind));
      if (!s.Wrote || memcmp(gravity, s.GravityOurs, sizeof(gravity)))
        memcpy(s.Gravity, gravity, sizeof(gravity));
      if (!s.Wrote || memcmp(wind, s.WindOurs, sizeof(wind)))
        memcpy(s.Wind, wind, sizeof(wind));
      if (!Finite(s.Gravity, 3) || !Finite(s.Wind, 3))
        return false;
      for (int k = 0; k < 3; ++k)
      {
        s.GravityOurs[k] = s.Gravity[k] * size;
        s.WindOurs[k] = s.Wind[k] * size;
      }
      memcpy(parameters, s.GravityOurs, sizeof(s.GravityOurs));
      memcpy(parameters + 3, s.WindOurs, sizeof(s.WindOurs));
      s.Wrote = true;
      g_HairGravityShown = s.GravityOurs[1];
      return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
  }

  // Its particles' places, and their places a step before, moved about bone
  // 1's place (plus bone 0's, as the bone matrices have it) by the ratio.
  // False, nothing moved, if its arrays or her world pose aren't as known.
  bool SnapHairParticles(__int64 simulation, __int64 definition, __int64 pose, float ratio)
  {
    __try
    {
      int count = *(int const*)(definition + DefinitionParticleCountOffset);
      __int64 nowArray = *(__int64 const*)(simulation + SimulationParticlesOffset);
      __int64 beforeArray = *(__int64 const*)(simulation + SimulationPreviousOffset);
      float* places = nowArray ? *(float**)nowArray : nullptr;
      float* before = beforeArray ? *(float**)beforeArray : nullptr;
      float const* world = *(float const* const*)(pose + PoseWorldOffset);
      if (count <= 0 || count > MaxHairEntries || !places || !before || (__int64)world != g_HairWorld.load() ||
        *(int const*)(nowArray + ArrayCountOffset) < count || *(int const*)(beforeArray + ArrayCountOffset) < count)
        return false;
      float centre[3];
      for (int k = 0; k < 3; ++k)
        centre[k] = world[k] + world[RootBone * FloatsPerBone + k];
      if (!Finite(centre, 3) || !std::isfinite(ratio))
        return false;
      for (int i = 0; i < count; ++i)
        for (int k = 0; k < 3; ++k)
        {
          float& place = places[i * ParticleFloats + k];
          float& last = before[i * ParticleFloats + k];
          place = centre[k] + ratio * (place - centre[k]);
          last = centre[k] + ratio * (last - centre[k]);
        }
      return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
  }

  // A cloth of hers before its step (its first palette entry, bone 0): its
  // gravity and wind times the size (the game's at 1.00), and on a size
  // change its particles moved about bone 1 by the new size over the old
  void PrepareHairStep(__int64 cloth)
  {
    float size = g_HairSize.load(std::memory_order_acquire);
    __int64 simulation = ReadQword(cloth + ClothSimulationOffset);
    __int64 definition = simulation ? ReadQword(simulation + SimulationDefinitionOffset) : 0;
    __int64 pose = g_HairPose.load(std::memory_order_relaxed);
    if (!definition || !pose || ReadQword(definition) != (__int64)g_gameHandle + ClothDefinitionVTableRva)
      return;
    ULONGLONG now = GetTickCount64();
    std::lock_guard<std::mutex> lock(g_HairStepMutex);
    HairStep* s = nullptr;
    for (HairStep& one : g_HairSteps)
      if (one.Cloth == cloth)
        s = &one;
    if (!s)
    {
      if (size == 1.f)
        return;   // nothing of the tools' in it
      s = &g_HairSteps[0];
      for (HairStep& one : g_HairSteps)
        if (!one.Cloth || one.Seen < s->Seen)
          s = &one;
      *s = HairStep{};
      s->Cloth = cloth;
      s->ParticlesAt = 1.f;   // it was stepped at 1.00 until now (the hooks pass nothing on then)
    }
    s->Seen = now;
    if (!SizeHairForces(*s, simulation, size) || s->ParticlesAt == size)
      return;
    float ratio = size / s->ParticlesAt;
    s->ParticlesAt = size;
    if (SnapHairParticles(simulation, definition, pose, ratio))
      g_HairSnaps.fetch_add(1, std::memory_order_relaxed);
    else
      g_HairSnapsRefused.fetch_add(1, std::memory_order_relaxed);
  }

  // The provider's slot 1 (the world pose's way): the game's matrix, sized
  // for her body's pose
  void __fastcall hClothBoneMatrix(__int64 provider, unsigned int bone, float* out)
  {
    // Bone 0 comes first in each update (0x142FA42C0): the cloth is the
    // ClothEntity's instance (provider - 0x40 + 0x68), whose provider this is
    if (bone == 0 && IsHerPose(provider))
    {
      __int64 cloth = ReadQword(provider - ClothEntityProviderOffset + ClothEntityInstanceOffset);
      if (cloth && ReadQword(cloth) == (__int64)g_gameHandle + ClothInstanceVTableRva &&
        ReadQword(cloth + ClothInstanceProviderOffset) == provider)
        PrepareHairStep(cloth);
    }
    oClothBoneMatrix(provider, bone, out);
    float size = g_HairSize.load(std::memory_order_acquire);
    if (size != 1.f && out && bone >= (unsigned int)FirstSizedBone && bone < (unsigned int)FaithBones)
      SizeGameMatrix(provider, bone, size, out);
  }

  // A palette entry from the chain (0x142FA4950: the bone's inverse bind
  // times x y z of the four rows of its matrix in the chain): for her body's
  // pose, a bone from the hips on gets her sized world matrix instead
  void __fastcall hClothBonePalette(__int64 cloth, unsigned int bone, float* chain)
  {
    // Bone 0 comes first in each update (0x142FA48BF)
    if (bone == 0)
    {
      __int64 provider = ReadQword(cloth + ClothInstanceProviderOffset);
      if (provider && ReadQword(provider) == (__int64)g_gameHandle + ClothProviderVTableRva && IsHerPose(provider))
        PrepareHairStep(cloth);
    }
    float size = g_HairSize.load(std::memory_order_acquire);
    if (size != 1.f && chain && bone >= (unsigned int)FirstSizedBone && bone < (unsigned int)FaithBones)
    {
      __int64 provider = ReadQword(cloth + ClothInstanceProviderOffset);
      if (provider && ReadQword(provider) == (__int64)g_gameHandle + ClothProviderVTableRva && IsHerPose(provider))
      {
        __declspec(align(16)) float m[16];
        oClothBoneMatrix(provider, bone, m);
        if (SizeGameMatrix(provider, bone, size, m))
        {
          for (int r = 0; r < 4; ++r)
            for (int c = 0; c < 3; ++c)
              chain[r * 4 + c] = m[r * 4 + c];
          g_HairChained.fetch_add(1, std::memory_order_relaxed);
        }
      }
    }
    oClothBonePalette(cloth, bone, chain);
  }

  // After her body's pose is sized: what the hook compares and scales with
  void ShowHairPlaces(Slot const& body, float size)
  {
    PoseState const& state = g_BodyPose;
    int k = g_HairPlacesShown.load(std::memory_order_relaxed) ^ 1;
    for (int i = 0; i < FaithBones; ++i)
      for (int c = 0; c < 3; ++c)
        g_HairPlaces[k][i][c] = i >= FirstSizedBone && state.HaveWorld[i] ? state.OurWorld[i][c] : NAN;
    g_HairPlacesShown.store(k, std::memory_order_release);
    g_HairPose.store(body.Pose, std::memory_order_relaxed);
    g_HairWorld.store((__int64)body.World, std::memory_order_relaxed);
    g_HairSize.store(size, std::memory_order_release);
  }

  // Each cloth on her customization component posed by her body (its
  // provider's pose object), with its simulation's definition and solver
  int FindHairCloths(__int64 soldier, __int64 bodyPose, HairCloth* out)
  {
    __int64 base = (__int64)g_gameHandle;
    int count = 0;
    __int64 collection = ReadQword(soldier + 0x38);
    if (!collection)
      return 0;
    for (int entry = 0; entry < 32; ++entry)
    {
      __int64 component = ReadQword(collection + 0x20 + entry * 0x20);
      if (!component || ReadQword(component) != base + CustomizationComponentVTableRva)
        continue;
      for (int off = 0; off < 0x800 && count < MaxHairCloths; off += 8)
      {
        __int64 entity = ReadQword(component + off);
        if (!entity || entity == component || ReadQword(entity) != base + ClothEntityVTableRva)
          continue;
        __int64 provider = entity + ClothEntityProviderOffset;
        if (ReadQword(provider) != base + ClothProviderVTableRva ||
          ReadQword(ReadQword(provider + ProviderAnimatableOffset) + SubPoseOffset) != bodyPose)
          continue;
        HairCloth c{};
        c.Cloth = ReadQword(entity + ClothEntityInstanceOffset);
        if (!c.Cloth || ReadQword(c.Cloth) != base + ClothInstanceVTableRva)
          continue;
        c.Simulation = ReadQword(c.Cloth + ClothSimulationOffset);
        c.Definition = c.Simulation ? ReadQword(c.Simulation + SimulationDefinitionOffset) : 0;
        c.Solver = c.Simulation ? ReadQword(c.Simulation + SimulationSolverOffset) : 0;
        if (!c.Definition || !c.Solver || ReadQword(c.Definition) != base + ClothDefinitionVTableRva)
          continue;
        bool known = false;
        for (int k = 0; k < count; ++k)
          known |= out[k].Cloth == c.Cloth;
        if (!known)
          out[count++] = c;
      }
      break;
    }
    return count;
  }

  // A solver's counts and sizes checked against its definition's arrays (the
  // data pointers of those into game)
  bool ReadHairArrays(HairCloth const& c, int* count, __int64* game, char* why, size_t whySize)
  {
    __try
    {
      for (int i = 0; i < HairArrayCount; ++i)
      {
        HairArray const& a = HairArrays[i];
        __int64 object = *(__int64 const*)(c.Definition + a.Definition);
        if (object < 0x10000)
        {
          sprintf_s(why, whySize, "its definition has no %s list", a.Name);
          return false;
        }
        __int64 data = *(__int64 const*)object;
        unsigned int listed = *(unsigned int const*)(object + a.DefinitionCount);
        unsigned int bytes = *(unsigned int const*)(c.Solver + a.Bytes);
        unsigned int n = a.Count ? *(unsigned int const*)(c.Solver + a.Count) : bytes / a.Stride;
        if (data < 0x10000 || n == 0 || n > (unsigned int)MaxHairEntries || n != listed || bytes != n * a.Stride)
        {
          sprintf_s(why, whySize, "%s: %u in its definition, %u (%u bytes) in its solver", a.Name, listed, n, bytes);
          return false;
        }
        count[i] = (int)n;
        game[i] = data;
      }
      return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      sprintf_s(why, whySize, "not readable");
      return false;
    }
  }

  HairCopies* CopiesFor(__int64 definition, int const* count)
  {
    for (int i = 0; i < g_HairCopyCount; ++i)
      if (g_HairCopies[i].Definition == definition && !memcmp(g_HairCopies[i].Count, count, sizeof(g_HairCopies[i].Count)))
        return &g_HairCopies[i];
    if (g_HairCopyCount == MaxHairCopySets)
      return nullptr;
    HairCopies& set = g_HairCopies[g_HairCopyCount];
    set = HairCopies{};
    for (int b = 0; b < 2; ++b)
      for (int i = 0; i < HairArrayCount; ++i)
      {
        // Its count rounded up to 4, and 4 more, the rest zeros
        size_t bytes = (size_t)(((count[i] + 3) & ~3) + 4) * HairArrays[i].Stride;
        set.Data[b][i] = (BYTE*)_aligned_malloc(bytes, 16);
        if (!set.Data[b][i])
        {
          for (int f = 0; f < 2; ++f)
            for (int j = 0; j < HairArrayCount; ++j)
              _aligned_free(set.Data[f][j]);
          set = HairCopies{};
          return nullptr;
        }
        memset(set.Data[b][i], 0, bytes);
      }
    set.Definition = definition;
    memcpy(set.Count, count, sizeof(set.Count));
    ++g_HairCopyCount;
    return &set;
  }

  // Copy b: the definition's values, every length times the size (a
  // constraint's is its last 4 bytes)
  bool FillHairCopy(HairCopies& set, __int64 const* game, int b, float size)
  {
    set.Size[b] = 0.f;
    __try
    {
      for (int i = 0; i < HairArrayCount; ++i)
        memcpy(set.Data[b][i], (void const*)game[i], (size_t)set.Count[i] * HairArrays[i].Stride);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
    for (int i = 0; i < HairArrayCount; ++i)
    {
      int stride = HairArrays[i].Stride;
      for (int n = 0; n < set.Count[i]; ++n)
        *(float*)(set.Data[b][i] + (size_t)n * stride + stride - 4) *= size;
    }
    set.Size[b] = size;
    return true;
  }

  // The solver's pointers to copy b (-1: its definition's arrays); one that's
  // neither the game's nor the tools' is left as it is. How many were moved.
  int PointHair(HairState& s, int b)
  {
    int moved = 0;
    for (int i = 0; i < HairArrayCount; ++i)
    {
      __int64 game = s.Game[i];
      __int64 copy0 = (__int64)s.Copies->Data[0][i], copy1 = (__int64)s.Copies->Data[1][i];
      __int64 want = b == 0 ? copy0 : b == 1 ? copy1 : game;
      LONG64 volatile* pointer = (LONG64 volatile*)(s.Cloth.Solver + HairArrays[i].Solver);
      __try
      {
        LONG64 now = *pointer;
        if (now == want)
          continue;
        if (now != game && now != copy0 && now != copy1)
        {
          ++s.Odd;
          continue;
        }
        if (now == game && s.Pointed >= 0)
          ++s.PutBack;
        if (InterlockedCompareExchange64(pointer, want, now) == now)
          ++moved;
        else
          ++s.Odd;
      }
      __except (EXCEPTION_EXECUTE_HANDLER)
      {
        ++s.Odd;
      }
    }
    return moved;
  }

  // How many of the solver's pointers are the tools' copies
  int CountOurs(HairState const& s)
  {
    int ours = 0;
    for (int i = 0; i < HairArrayCount; ++i)
    {
      __int64 now = ReadQword(s.Cloth.Solver + HairArrays[i].Solver);
      if (now && (now == (__int64)s.Copies->Data[0][i] || now == (__int64)s.Copies->Data[1][i]))
        ++ours;
    }
    return ours;
  }

  // Every frame: her cloths' solvers at the size (1: back to their
  // definitions'). Only cloths found through her soldier this frame are
  // written. One not found is kept as it was (its solver may still point at
  // the tools' copies, so it's pointed back when it's found again) until its
  // place is needed for another.
  void UpdateHairLengths(__int64 soldier, __int64 bodyPose, float size)
  {
    bool any = false;
    for (HairState const& s : g_HairStates)
      any |= s.Cloth.Cloth != 0;
    if (size == 1.f && !any)
      return;
    HairCloth found[MaxHairCloths];
    int count = soldier >= 0x10000 && bodyPose ? FindHairCloths(soldier, bodyPose, found) : 0;
    bool seen[MaxHairCloths] = {};
    for (int i = 0; i < MaxHairCloths; ++i)
      for (int k = 0; k < count; ++k)
        seen[i] |= g_HairStates[i].Cloth.Cloth && !memcmp(&found[k], &g_HairStates[i].Cloth, sizeof(HairCloth));
    for (int k = 0; k < count; ++k)
    {
      HairCloth const& c = found[k];
      HairState* s = nullptr;
      HairState* unused = nullptr;
      for (int i = 0; i < MaxHairCloths; ++i)
      {
        HairState& one = g_HairStates[i];
        if (one.Cloth.Cloth && !memcmp(&one.Cloth, &c, sizeof(HairCloth)))
          s = &one;
        else if (!one.Cloth.Cloth && !unused)
          unused = &one;
      }
      if (!s && size == 1.f)
        continue;
      if (!s && !unused)
        for (int i = 0; i < MaxHairCloths && !unused; ++i)
          if (!seen[i])
          {
            util::log::Write("Faith's size: her cloth 0x%I64X wasn't found any more, its place goes to 0x%I64X (its "
              "lengths are left as they are)", g_HairStates[i].Cloth.Cloth, c.Cloth);
            g_HairStates[i] = HairState{};
            unused = &g_HairStates[i];
          }
      int n[HairArrayCount];
      __int64 game[HairArrayCount];
      char why[200] = "";
      bool ok = ReadHairArrays(c, n, game, why, sizeof(why));
      if (!s)
      {
        HairCopies* copies = ok ? CopiesFor(c.Definition, n) : nullptr;
        if (!copies || !unused)
        {
          if (g_HairRefusedSolver != c.Solver)
            util::log::Warning("Faith's size: her cloth 0x%I64X's lengths stay its normal size (%s)", c.Cloth,
              !ok ? why : !copies ? "no room for the tools' copies" : "too many cloths");
          g_HairRefusedSolver = c.Solver;
          continue;
        }
        s = unused;
        *s = HairState{};
        s->Cloth = c;
        s->Copies = copies;
        memcpy(s->Game, game, sizeof(s->Game));
        s->Pointed = -1;
      }
      else if (!ok || memcmp(n, s->Copies->Count, sizeof(n)) || memcmp(game, s->Game, sizeof(game)))
      {
        util::log::Warning("Faith's size: her cloth 0x%I64X's lists changed under the tools (%s), its lengths are left "
          "to it", c.Cloth, ok ? "other lists" : why);
        *s = HairState{};
        continue;
      }
      if (size == 1.f)
      {
        int moved = PointHair(*s, -1);
        util::log::Write("Faith's size: her cloth 0x%I64X's lengths are its definition's again (%d lists pointed back)",
          c.Cloth, moved);
        *s = HairState{};
        continue;
      }
      HairCopies& set = *s->Copies;
      int b = s->Pointed >= 0 && set.Size[s->Pointed] == size ? s->Pointed : s->Pointed == 0 ? 1 : 0;
      if (set.Size[b] != size && !FillHairCopy(set, s->Game, b, size))
      {
        util::log::Warning("Faith's size: her cloth 0x%I64X's lists couldn't be read, its lengths are left to it",
          c.Cloth);
        PointHair(*s, -1);
        *s = HairState{};
        continue;
      }
      bool first = s->Pointed < 0;
      int moved = PointHair(*s, b);
      s->Pointed = b;
      if (first)
        util::log::Write("Faith's size: her cloth 0x%I64X's lengths are the tools' at %.2f (%d of %d lists pointed: %d "
          "structure, %d shear, %d fold, %d long-range constraints, %d radii)", c.Cloth, size, moved, HairArrayCount,
          n[0], n[1], n[2], n[3], n[6]);
    }
  }

  void HookHair()
  {
    __int64 base = (__int64)g_gameHandle;
    char const* what = nullptr;
    __try
    {
      if (*(__int64 const*)(base + ClothProviderVTableRva + 8) != base + ClothBoneMatrixRva)
        what = "the cloth provider's slot 1";
      for (CodeCheck const& check : HairCodeChecks)
        if (!what && memcmp((void const*)(base + check.Rva), check.Bytes, check.Size))
          what = check.What;
      if (!what && ClothBoneMatrixRva + ClothBoneMatrixBytes +
        *(int const*)(base + ClothBoneMatrixRva + ClothBoneMatrixBytes - 4) != BoneMatrixRva)
        what = "the cloth's bone matrix's jump";
      for (HairArray const& a : HairArrays)
        if (!what && memcmp((void const*)(base + a.Read.Rva), a.Read.Bytes, a.Read.Size))
          what = a.Read.What;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      what = "not readable";
    }
    if (what)
    {
      util::log::Warning("Faith's size: the game's cloth code (%s) isn't what the tools know, her hair stays its "
        "normal size", what);
      return;
    }
    g_HairHooked = util::hooks::CreateGameHook("FaithSizeClothBoneMatrix", base + ClothBoneMatrixRva, hClothBoneMatrix,
      (LPVOID*)&oClothBoneMatrix);
    g_HairHooked = util::hooks::CreateGameHook("FaithSizeClothBonePalette", base + ClothBonePaletteRva, hClothBonePalette,
      (LPVOID*)&oClothBonePalette) && g_HairHooked;
    util::log::Write("Faith's size: her cloths' bone matrices %s", g_HairHooked ? "hooked, both ways (sized for her pose "
      "only)" : "not all hooked, her hair stays its normal size");
  }

  // Her hair's first particles (as many as its definition's anchor count): y
  // lowest and highest (0: not read)
  int ReadAnchorHeights(__int64 cloth, float& low, float& high)
  {
    __int64 simulation = ReadQword(cloth + ClothSimulationOffset);
    __int64 definition = simulation ? ReadQword(simulation + SimulationDefinitionOffset) : 0;
    __int64 particles = simulation ? ReadQword(ReadQword(simulation + SimulationParticlesOffset)) : 0;
    if (!definition || !particles)
      return 0;
    __try
    {
      int anchors = *(int const*)(definition + DefinitionAnchorCountOffset);
      if (anchors <= 0 || anchors > 256)
        return 0;
      float const* p = (float const*)particles;
      low = high = p[1];
      for (int i = 1; i < anchors; ++i)
      {
        float y = p[i * ParticleFloats + 1];
        low = y < low ? y : low;
        high = y > high ? y : high;
      }
      return anchors;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return 0;
    }
  }

  void LogHair(__int64 soldier, float size)
  {
    __int64 instance = soldier >= 0x10000 ? FindHairClothInstance(soldier) : 0;
    float root[4] = {}, box[8] = {};
    if (!instance || !ReadClothFloats(instance, root, box))
    {
      util::log::Write("Faith's size (diagnostic): her hair cloth isn't found (instance 0x%I64X)", instance);
      return;
    }
    PoseState const& state = g_BodyPose;
    bool head = state.HaveWorld[HeadEndBone];
    float low = 0.f, high = 0.f;
    int anchors = ReadAnchorHeights(instance, low, high);
    util::log::Write("Faith's size (diagnostic): at %.2f her hair cloth's root %.3f %.3f %.3f (4th float %.4f), its box "
      "y %.3f - %.3f, its first %d particles (its anchor count) y %.3f - %.3f; her head end's world place as the game "
      "wrote it y %.3f, sized y %.3f", size, root[0], root[1], root[2], root[3], box[1], box[5], anchors, low, high,
      head ? state.GameWorld[HeadEndBone][1] : 0.f, head ? state.OurWorld[HeadEndBone][1] : 0.f);
    if (!g_HairHooked)
      return;
    int handled = 0, ours = 0, putBack = 0, odd = 0;
    for (HairState& s : g_HairStates)
      if (s.Cloth.Cloth)
      {
        ++handled;
        ours += CountOurs(s);
        putBack += s.PutBack;
        odd += s.Odd;
        s.PutBack = s.Odd = 0;
      }
    long long scaled = g_HairRowsScaled.exchange(0), placesSized = g_HairPlacesSized.exchange(0);
    util::log::Write("Faith's size (hair): %s; bone matrices for her cloths sized: %lld (%lld of them into the chain's "
      "palette; %lld with places the tools wrote, %lld with the game's places sized here), %lld left (made from another "
      "place), %lld with another world pose; %d cloth(s) of hers handled, %d of their %d lists the tools' copies; the "
      "game had put %d back to its own, %d were neither (left)", g_HairOn.load() ? "hair follows size" :
      "hair follows size unticked", scaled + placesSized, g_HairChained.exchange(0), scaled, placesSized,
      g_HairRowsLeft.exchange(0), g_HairOtherWorld.exchange(0), handled, ours, handled * HairArrayCount, putBack, odd);
    float gravity;
    {
      std::lock_guard<std::mutex> lock(g_HairStepMutex);
      gravity = g_HairGravityShown;
    }
    util::log::Write("Faith's size (hair): its gravity's y %.4f (the game's times the size), wind times the size too; "
      "particles moved to a new size %lld times (%lld not: arrays or places not as known)", gravity,
      g_HairSnaps.exchange(0), g_HairSnapsRefused.exchange(0));
  }


  // ---- Her collision: what the game's collision queries near her hold (diagnostic) ----
  //
  // Ant asks the game's FrostbiteCollisionWorld (vtable 0x141ABC7E8, one at
  // animation manager + 0x150) for its collision: slot 6 a shape cast with an
  // Ant collision volume (made into a Havok shape each time, 0x142FD0ED0),
  // slot 7 a shape cast with a Havok shape it already has, slot 8 a collision
  // object registered once (its volumes made into Havok shapes then,
  // 0x142FD1070). Which of them carries her movement capsule, and with what
  // sizes, isn't known (Research_Faith_Size_2026-11-03.md section 6). While
  // she's sized, the casts that start near her are tallied (who asked, the
  // shape, its radius and half height, its own offset and scale, the cast's
  // transform from her feet), and every registration is logged once (the
  // first 60). Nothing is changed: each call goes on to the game's own with
  // its arguments as they came (all 64 bits of each).
  const __int64 CollisionWorldVTableRva = 0x1ABC7E8;
  const __int64 ShapeCastRva = 0x2FCB200;           // slot 6 (10 arguments)
  const __int64 HavokShapeCastRva = 0x2FCB5B0;      // slot 7 (9 arguments)
  const __int64 RegisterRva = 0x2FCC1B0;            // slot 8 (6 arguments)
  const __int64 CapsuleAssetVTableRva = 0x1A10038;  // Ant's CapsuleAsset: Radius + 0x60, HalfHeight + 0x64
  const unsigned int CompoundVolumeType = 0xC39365CD; // a list of volumes: + 0x60 count, + 0x68 the volumes
  const int MaxDiagEntries = 64;
  const int MaxRegistrationLines = 60;
  // Their first bytes (mov rax, rsp; push ...; lea rbp, [rax - ...]; sub rsp, ...)
  const BYTE ShapeCastStart[] = { 0x48, 0x89, 0xE0, 0x55, 0x53, 0x56, 0x57, 0x41, 0x56, 0x41, 0x57, 0x48, 0x8D, 0xA8,
    0x38, 0xFC, 0xFF, 0xFF, 0x48, 0x81, 0xEC, 0x98, 0x04, 0x00 };
  const BYTE HavokShapeCastStart[] = { 0x48, 0x89, 0xE0, 0x55, 0x56, 0x57, 0x41, 0x56, 0x41, 0x57, 0x48, 0x8D, 0xA8,
    0x78, 0xFC, 0xFF, 0xFF, 0x48, 0x81, 0xEC, 0x60, 0x04, 0x00, 0x00 };
  const BYTE RegisterStart[] = { 0x48, 0x89, 0x5C, 0x24, 0x10, 0x48, 0x89, 0x6C, 0x24, 0x18, 0x48, 0x89, 0x74, 0x24,
    0x20, 0x57, 0x48, 0x83, 0xEC, 0x30, 0x48, 0x89, 0xCE, 0x48 };

  typedef unsigned long long Raw;
  typedef Raw(__fastcall* tShapeCast)(Raw, Raw, Raw, Raw, Raw, Raw, Raw, Raw, Raw, Raw);
  typedef Raw(__fastcall* tHavokShapeCast)(Raw, Raw, Raw, Raw, Raw, Raw, Raw, Raw, Raw);
  typedef Raw(__fastcall* tRegister)(Raw, Raw, Raw, Raw, Raw, Raw);
  tShapeCast oShapeCast = nullptr;
  tHavokShapeCast oHavokShapeCast = nullptr;
  tRegister oRegister = nullptr;

  // One cast as read (plain data: filled inside __try, tallied outside it)
  struct CastRead
  {
    int Path;                 // 6 or 7
    __int64 Shape;            // the Ant volume (slot 6) or the Havok shape (slot 7)
    __int64 Caller;           // the game's code that asked (an RVA)
    __int64 ShapeVTable;      // an RVA
    unsigned int ShapeType;   // the Ant volume's type (0: not read)
    float Params[4];          // the volume's + 0x60 .. + 0x6C (a capsule's Radius, HalfHeight)
    float LocalScale[4];      // the volume's own + 0x20
    float LocalPlace[4];      // the volume's own + 0x40
    float Scale[4];           // the cast's transform: scale
    float Offset[3];          // its translation, from her feet
    float Sweep[3];           // direction x distance
    unsigned int Id1, Id2;
    int Filter;
  };
  struct DiagEntry
  {
    CastRead Last;
    int Count;
  };
  std::mutex g_DiagMutex;
  DiagEntry g_Diag[MaxDiagEntries];
  int g_DiagCount = 0;
  int g_DiagDropped = 0;
  std::atomic<long long> g_DiagCasts{ 0 }; // every cast while on, near her or not
  std::atomic<bool> g_DiagOn{ false };
  std::atomic<float> g_DiagRoot[3];
  std::atomic<float> g_DiagReach{ 3.f };
  std::atomic<int> g_RegistrationLines{ 0 };
  bool g_DiagHooked = false;

  // An Ant asset's type, read from its getter (vtable slot 1) only when that
  // is "mov eax, imm32; ret": the getters of the collision volumes are
  unsigned int AntType(__int64 asset)
  {
    __try
    {
      BYTE const* getter = *(BYTE const**)(*(__int64 const*)asset + 8);
      if (getter[0] == 0xB8 && getter[5] == 0xC3)
        return *(unsigned int const*)(getter + 1);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
    return 0;
  }

  bool ReadCast(int path, Raw shape, Raw transform, Raw direction, Raw distance, Raw id1, Raw id2, Raw filter,
    __int64 caller, CastRead& out)
  {
    __int64 base = (__int64)g_gameHandle;
    memset(&out, 0, sizeof(out));
    __try
    {
      float const* t = (float const*)transform;
      float root[3] = { g_DiagRoot[0].load(), g_DiagRoot[1].load(), g_DiagRoot[2].load() };
      float dx = t[8] - root[0], dy = t[9] - root[1], dz = t[10] - root[2];
      float reach = g_DiagReach.load();
      if (!(dx * dx + dy * dy + dz * dz < reach * reach))
        return false;
      out.Path = path;
      out.Shape = (__int64)shape;
      out.Caller = caller - base;
      memcpy(out.Scale, t, sizeof(out.Scale));
      out.Offset[0] = dx; out.Offset[1] = dy; out.Offset[2] = dz;
      float d;
      memcpy(&d, &distance, sizeof(d));
      float const* dir = (float const*)direction;
      for (int k = 0; k < 3; ++k)
        out.Sweep[k] = dir ? dir[k] * d : 0.f;
      out.Id1 = (unsigned int)(id1 & 0xFFFF);
      out.Id2 = (unsigned int)(id2 & 0xFFFF);
      out.Filter = (int)(filter & 0xFFFFFFFF);
      if (shape && path == 6)
      {
        out.ShapeVTable = *(__int64 const*)shape - base;
        memcpy(out.LocalScale, (float const*)(shape + 0x20), sizeof(out.LocalScale));
        memcpy(out.LocalPlace, (float const*)(shape + 0x40), sizeof(out.LocalPlace));
        memcpy(out.Params, (float const*)(shape + 0x60), sizeof(out.Params));
      }
      else if (shape)
        out.ShapeVTable = *(__int64 const*)shape - base;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
    if (path == 6 && out.Shape)
      out.ShapeType = AntType(out.Shape);
    return true;
  }

  void Tally(CastRead const& cast)
  {
    std::lock_guard<std::mutex> lock(g_DiagMutex);
    for (int i = 0; i < g_DiagCount; ++i)
      if (g_Diag[i].Last.Path == cast.Path && g_Diag[i].Last.Shape == cast.Shape && g_Diag[i].Last.Caller == cast.Caller)
      {
        g_Diag[i].Last = cast;
        ++g_Diag[i].Count;
        return;
      }
    if (g_DiagCount < MaxDiagEntries)
      g_Diag[g_DiagCount++] = { cast, 1 };
    else
      ++g_DiagDropped;
  }

  void CountCast()
  {
    g_DiagCasts.fetch_add(1, std::memory_order_relaxed);
  }

  // ---- Collision follows size: her capsules handed to the game sized ----
  //
  // Her movement sweeps ready-made Havok capsules through slot 7 (the
  // diagnostic's logs, 2026-11-03 13:47 and 14:18 - 14:26, her casts asked by
  // 0x142F3C117): one of radius 0.30 and half height 0.60 centred 0.90 m over
  // her feet (1.8 m tall), one of 0.30 / 0.18 centred 0.48 m up. Read live
  // (14:2x): hknpCapsuleShape, vtable 0x141D4F448, 0x1B0 bytes (+ 0x0A, its
  // size; refcounting is skipped when that is 0, 0x143514E50): + 0x14 its
  // convex radius (0.99 x the radius: 0x143F6B310), + 0x50 / + 0x60 its two
  // ends (0, -h, 0) / (0, h, 0), + 0x70 eight hull vertices (the ends grown by
  // 0.01 x the radius), + 0xF0 six planes (normal, -distance) and two empty
  // ones. The wrapper Frostbite casts with (vtable 0x141BB19C0) holds the
  // shape at + 0x08. Her capsules are only hers (each character's are made
  // for it), but the game's own aren't touched: a cast of hers gets a copy
  // of the tools' (every length times the size, its own wrapper), and its
  // place is raised so the capsule's bottom stays where the game put it. Her
  // sweep stays as the game asked: her moves keep their distances. Her
  // character's id in these casts changes when she respawns (7, 10, 22 in
  // the logs), so it's learnt again and again: the id of the movement casts
  // that start over her feet.
  const __int64 MovementCallerRva = 0x2F3C117;      // where her movement's slot 7 casts return
  const __int64 CapsuleShapeVTableRva = 0x1D4F448;  // hknpCapsuleShape
  const __int64 ShapeWrapperVTableRva = 0x1BB19C0;  // Frostbite's shape wrapper (+ 0x08 the Havok shape)
  const int CapsuleShapeBytes = 0x1B0;
  const int MaxOwnCapsules = 8;
  const int MaxSizedCopies = 64;
  const int MaxIdVotes = 8;
  const int HerIdVotes = 20;                        // movement casts over her feet in a second to take an id as hers
  const ULONGLONG CopyReuseAfter = 2000;            // ms a sized copy must have been unused before it's made again

  struct OwnCapsule
  {
    __int64 Wrapper;
    __int64 Shape;
    float HalfHeight, Radius;
    ULONGLONG Seen;
  };
  struct __declspec(align(16)) SizedCopy
  {
    BYTE ShapeBytes[CapsuleShapeBytes];
    BYTE Wrapper[0x20];
    __int64 Shape;                       // the game's it was copied from (0: free)
    float Size;
    ULONGLONG Used;
  };
  struct IdVote
  {
    unsigned int Id;
    int Count;
  };

  std::atomic<bool> g_CollisionOn{ false };         // the menu's tick
  std::atomic<float> g_CollisionSize{ 1.f };        // the size her casts get (1: none)
  std::atomic<int> g_HerId{ -1 };
  std::atomic<long long> g_SizedCasts{ 0 };
  std::mutex g_CollisionMutex;
  OwnCapsule g_Own[MaxOwnCapsules];
  int g_OwnCount = 0;
  SizedCopy g_Copies[MaxSizedCopies];
  IdVote g_Votes[MaxIdVotes];
  int g_VoteCount = 0;
  ULONGLONG g_VotesSince = 0;
  bool g_LoggedCollisionOn = false;

  bool ReadTransform(Raw transform, float* out)
  {
    __try
    {
      memcpy(out, (float const*)transform, 12 * sizeof(float));
      return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
  }

  // A movement cast that starts over her feet votes for its id
  void Vote(unsigned int id, Raw transform, float size)
  {
    float t[12];
    if (!ReadTransform(transform, t))
      return;
    float root[3] = { g_DiagRoot[0].load(), g_DiagRoot[1].load(), g_DiagRoot[2].load() };
    float dx = t[8] - root[0], dy = t[9] - root[1], dz = t[10] - root[2];
    float reach = 0.4f * (size > 1.f ? size : 1.f);
    if (!(dx * dx + dz * dz < reach * reach) || !(dy > 0.2f && dy < 1.5f))
      return;
    std::lock_guard<std::mutex> lock(g_CollisionMutex);
    for (int i = 0; i < g_VoteCount; ++i)
      if (g_Votes[i].Id == id)
      {
        ++g_Votes[i].Count;
        return;
      }
    if (g_VoteCount < MaxIdVotes)
      g_Votes[g_VoteCount++] = { id, 1 };
  }

  // A wrapper's capsule as the game made it (false if it isn't one of the
  // kind read live)
  bool ReadCapsule(Raw wrapper, OwnCapsule& out)
  {
    __int64 base = (__int64)g_gameHandle;
    __try
    {
      if (*(__int64 const*)wrapper != base + ShapeWrapperVTableRva)
        return false;
      __int64 shape = *(__int64 const*)(wrapper + 8);
      if (shape < 0x10000 || *(__int64 const*)shape != base + CapsuleShapeVTableRva ||
        *(unsigned short const*)(shape + 0x0A) != CapsuleShapeBytes)
        return false;
      float const* a = (float const*)(shape + 0x50);
      float const* b = (float const*)(shape + 0x60);
      float const* v = (float const*)(shape + 0x70);
      float convex = *(float const*)(shape + 0x14);
      float h = b[1];
      if (a[0] != 0.f || a[2] != 0.f || b[0] != 0.f || b[2] != 0.f || a[1] != -h || !(h > 0.01f && h < 5.f) ||
        !(convex > 0.01f && convex < 5.f))
        return false;
      float radius = convex / 0.99f;
      if (fabsf((fabsf(v[1]) - h) / 0.01f - radius) > 0.01f)
        return false;
      out.Wrapper = (__int64)wrapper;
      out.Shape = shape;
      out.HalfHeight = h;
      out.Radius = radius;
      return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
  }

  bool CopyShape(__int64 shape, BYTE* out)
  {
    __try
    {
      memcpy(out, (void const*)shape, CapsuleShapeBytes);
      return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
  }

  bool CopyWrapper(__int64 wrapper, BYTE* out)
  {
    __try
    {
      memcpy(out, (void const*)wrapper, 0x18);
      return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
  }

  // Her capsule for this wrapper: one known, or (from a movement cast of
  // hers) read now and kept
  bool FindCapsule(Raw wrapper, bool movement, OwnCapsule& out)
  {
    ULONGLONG now = GetTickCount64();
    {
      std::lock_guard<std::mutex> lock(g_CollisionMutex);
      for (int i = 0; i < g_OwnCount; ++i)
        if (g_Own[i].Wrapper == (__int64)wrapper)
        {
          g_Own[i].Seen = now;
          out = g_Own[i];
          return true;
        }
    }
    if (!movement)
      return false;
    OwnCapsule read{};
    if (!ReadCapsule(wrapper, read))
      return false;
    read.Seen = now;
    std::lock_guard<std::mutex> lock(g_CollisionMutex);
    int slot = g_OwnCount < MaxOwnCapsules ? g_OwnCount++ : 0;
    if (slot == 0 && g_OwnCount == MaxOwnCapsules)
      for (int i = 1; i < MaxOwnCapsules; ++i)
        if (g_Own[i].Seen < g_Own[slot].Seen)
          slot = i;
    g_Own[slot] = read;
    out = read;
    return true;
  }

  // The tools' sized copy of one of her capsules (made once per size; never
  // freed: one unused for CopyReuseAfter is made again for another)
  SizedCopy* CopyFor(OwnCapsule const& capsule, float size)
  {
    ULONGLONG now = GetTickCount64();
    std::lock_guard<std::mutex> lock(g_CollisionMutex);
    int pick = -1;
    for (int i = 0; i < MaxSizedCopies; ++i)
    {
      SizedCopy& c = g_Copies[i];
      if (c.Shape == capsule.Shape && c.Size == size)
      {
        c.Used = now;
        return &c;
      }
      if (pick < 0 && (c.Shape == 0 || now - c.Used > CopyReuseAfter))
        pick = i;
    }
    if (pick < 0)
      return nullptr;
    SizedCopy& c = g_Copies[pick];
    if (!CopyShape(capsule.Shape, c.ShapeBytes) || !CopyWrapper(capsule.Wrapper, c.Wrapper))
    {
      c.Shape = 0;
      return nullptr;
    }
    // Not the game's to count or free
    *(unsigned short*)(c.ShapeBytes + 0x0A) = 0;
    float* f = (float*)c.ShapeBytes;
    f[0x14 / 4] *= size;                                   // its convex radius
    for (int k = 0; k < 3; ++k)
    {
      f[0x50 / 4 + k] *= size;                             // its ends
      f[0x60 / 4 + k] *= size;
    }
    for (int v = 0; v < 8; ++v)
      for (int k = 0; k < 3; ++k)
        f[0x70 / 4 + v * 4 + k] *= size;                   // the hull's vertices
    for (int p = 0; p < 6; ++p)
      f[0xF0 / 4 + p * 4 + 3] *= size;                     // the planes' distances
    *(__int64*)(c.Wrapper + 8) = (__int64)c.ShapeBytes;
    c.Shape = capsule.Shape;
    c.Size = size;
    c.Used = now;
    return &c;
  }

  // One slot 7 cast: the wrapper and transform to hand on (false: the game's
  // own, as they came)
  bool SizeCast(Raw wrapper, Raw transform, Raw id, __int64 caller, float* moved, Raw& useWrapper)
  {
    float size = g_CollisionSize.load(std::memory_order_relaxed);
    if (size == 1.f || !wrapper || !transform)
      return false;
    unsigned int castId = (unsigned int)(id & 0xFFFF);
    bool movement = caller == (__int64)g_gameHandle + MovementCallerRva;
    if (movement)
      Vote(castId, transform, size);
    int herId = g_HerId.load(std::memory_order_relaxed);
    if (herId < 0 || castId != (unsigned int)herId)
      return false;
    OwnCapsule capsule;
    if (!FindCapsule(wrapper, movement, capsule))
      return false;
    SizedCopy* copy = CopyFor(capsule, size);
    if (!copy || !ReadTransform(transform, moved))
      return false;
    // Raised along the capsule's own axis (its rotation's y) so its bottom
    // stays where it was
    float rise = (size - 1.f) * (capsule.HalfHeight + capsule.Radius);
    float qx = moved[4], qy = moved[5], qz = moved[6], qw = moved[7];
    moved[8] += 2.f * (qx * qy - qw * qz) * rise;
    moved[9] += (1.f - 2.f * (qx * qx + qz * qz)) * rise;
    moved[10] += 2.f * (qy * qz + qw * qx) * rise;
    useWrapper = (Raw)copy->Wrapper;
    g_SizedCasts.fetch_add(1, std::memory_order_relaxed);
    return true;
  }

  // Once a second from the camera hook: the id with the most movement casts
  // over her feet is hers
  void PickHerId(ULONGLONG now)
  {
    if (now - g_VotesSince < 1000)
      return;
    std::lock_guard<std::mutex> lock(g_CollisionMutex);
    int best = -1;
    for (int i = 0; i < g_VoteCount; ++i)
      if (g_Votes[i].Count >= HerIdVotes && (best < 0 || g_Votes[i].Count > g_Votes[best].Count))
        best = i;
    if (best >= 0)
    {
      int id = (int)g_Votes[best].Id;
      if (id != g_HerId.load())
      {
        g_HerId.store(id);
        g_OwnCount = 0;                  // her capsules are found again for this id
      }
    }
    g_VoteCount = 0;
    g_VotesSince = now;
  }

  void LogCollisionSizing(float size)
  {
    char text[600];
    int used;
    {
      std::lock_guard<std::mutex> lock(g_CollisionMutex);
      used = sprintf_s(text, "Faith's size (collision): at %.2f, her id %d, %d capsules of hers known:", size,
        g_HerId.load(), g_OwnCount);
      for (int i = 0; i < g_OwnCount && used > 0 && used < (int)sizeof(text) - 60; ++i)
        used += sprintf_s(text + used, sizeof(text) - used, " r %.2f h %.2f", g_Own[i].Radius, g_Own[i].HalfHeight);
    }
    util::log::Write("%s; %lld casts of hers handed sized", text, g_SizedCasts.exchange(0));
  }

  Raw __fastcall hShapeCast(Raw world, Raw context, Raw shape, Raw id1, Raw id2, Raw transform, Raw direction,
    Raw distance, Raw a9, Raw filter)
  {
    if (g_DiagOn.load(std::memory_order_relaxed))
    {
      CastRead cast;
      CountCast();
      if (ReadCast(6, shape, transform, direction, distance, id1, id2, filter, (__int64)_ReturnAddress(), cast))
        Tally(cast);
    }
    return oShapeCast(world, context, shape, id1, id2, transform, direction, distance, a9, filter);
  }

  Raw __fastcall hHavokShapeCast(Raw world, Raw context, Raw shape, Raw id, Raw transform, Raw direction, Raw distance,
    Raw a8, Raw filter)
  {
    __int64 caller = (__int64)_ReturnAddress();
    if (g_DiagOn.load(std::memory_order_relaxed))
    {
      CastRead cast;
      CountCast();
      if (ReadCast(7, shape, transform, direction, distance, id, 0, filter, caller, cast))
        Tally(cast);
    }
    if (g_CollisionOn.load(std::memory_order_relaxed))
    {
      __declspec(align(16)) float moved[12];
      Raw useWrapper = shape;
      if (SizeCast(shape, transform, id, caller, moved, useWrapper))
        return oHavokShapeCast(world, context, useWrapper, id, (Raw)moved, direction, distance, a8, filter);
    }
    return oHavokShapeCast(world, context, shape, id, transform, direction, distance, a8, filter);
  }

  // One volume of a registration, as text
  void DescribeVolume(__int64 volume, char* out, size_t size)
  {
    __int64 base = (__int64)g_gameHandle;
    float scale[4] = {}, place[4] = {}, params[4] = {};
    __int64 vtable = 0;
    bool read = false;
    __try
    {
      vtable = *(__int64 const*)volume - base;
      memcpy(scale, (float const*)(volume + 0x20), sizeof(scale));
      memcpy(place, (float const*)(volume + 0x40), sizeof(place));
      memcpy(params, (float const*)(volume + 0x60), sizeof(params));
      read = true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
    if (!read)
    {
      sprintf_s(out, size, "[0x%I64X unreadable]", volume);
      return;
    }
    sprintf_s(out, size, "[0x%I64X vt 0x%I64X type 0x%08X: +0x60 %.3f %.3f %.3f %.3f, own scale %.2f %.2f %.2f, own place "
      "%.3f %.3f %.3f]", volume, vtable, AntType(volume), params[0], params[1], params[2], params[3], scale[0], scale[1],
      scale[2], place[0], place[1], place[2]);
  }

  void LogRegistration(Raw asset, Raw id1, Raw id2, Raw a6, __int64 caller)
  {
    __int64 base = (__int64)g_gameHandle;
    char text[1600];
    int used = 0;
    unsigned int type = asset ? AntType((__int64)asset) : 0;
    int count = 0;
    __int64 list = 0;
    if (type == CompoundVolumeType)
    {
      __try
      {
        count = *(int const*)(asset + 0x60);
        list = *(__int64 const*)(asset + 0x68);
      }
      __except (EXCEPTION_EXECUTE_HANDLER)
      {
        count = 0;
      }
    }
    used = sprintf_s(text, "Faith's size (collision diagnostic): registration by 0x%I64X: asset 0x%I64X type 0x%08X, ids %u "
      "/ %u, a6 %d", caller - base, (__int64)asset, type, (unsigned int)(id1 & 0xFFFF), (unsigned int)(id2 & 0xFFFF),
      (int)(a6 & 0xFFFFFFFF));
    if (type == CompoundVolumeType && list)
    {
      used += sprintf_s(text + used, sizeof(text) - used, ", %d volumes:", count);
      for (int i = 0; i < count && i < 6 && used > 0 && used < (int)sizeof(text) - 300; ++i)
      {
        __int64 volume = 0;
        __try
        {
          volume = *(__int64 const*)(list + i * 8);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
          break;
        }
        char one[280];
        DescribeVolume(volume, one, sizeof(one));
        used += sprintf_s(text + used, sizeof(text) - used, " %s", one);
      }
    }
    else if (asset)
    {
      char one[280];
      DescribeVolume((__int64)asset, one, sizeof(one));
      used += sprintf_s(text + used, sizeof(text) - used, ": %s", one);
    }
    util::log::Write("%s", text);
  }

  Raw __fastcall hRegister(Raw world, Raw asset, Raw id1, Raw id2, Raw a5, Raw a6)
  {
    if (g_RegistrationLines.fetch_add(1) < MaxRegistrationLines)
      LogRegistration(asset, id1, id2, a6, (__int64)_ReturnAddress());
    return oRegister(world, asset, id1, id2, a5, a6);
  }

  void HookCollision()
  {
    __int64 base = (__int64)g_gameHandle;
    bool known = false;
    __try
    {
      __int64 const* vtable = (__int64 const*)(base + CollisionWorldVTableRva);
      known = vtable[6] == base + ShapeCastRva && vtable[7] == base + HavokShapeCastRva &&
        vtable[8] == base + RegisterRva &&
        !memcmp((void const*)(base + ShapeCastRva), ShapeCastStart, sizeof(ShapeCastStart)) &&
        !memcmp((void const*)(base + HavokShapeCastRva), HavokShapeCastStart, sizeof(HavokShapeCastStart)) &&
        !memcmp((void const*)(base + RegisterRva), RegisterStart, sizeof(RegisterStart));
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      known = false;
    }
    if (!known)
    {
      util::log::Warning("Faith's size: the game's collision world code isn't what the tools know, no collision "
        "diagnostic");
      return;
    }
    bool hooked = util::hooks::CreateGameHook("FaithSizeShapeCast", base + ShapeCastRva, hShapeCast,
      (LPVOID*)&oShapeCast);
    hooked = util::hooks::CreateGameHook("FaithSizeHavokShapeCast", base + HavokShapeCastRva, hHavokShapeCast,
      (LPVOID*)&oHavokShapeCast) && hooked;
    hooked = util::hooks::CreateGameHook("FaithSizeCollisionRegister", base + RegisterRva, hRegister,
      (LPVOID*)&oRegister) && hooked;
    g_DiagHooked = hooked;
    util::log::Write("Faith's size: collision diagnostic %s (shape casts, Havok shape casts, registrations)",
      hooked ? "hooked" : "not all hooked");
  }

  // The tally while sized, every DiagnosticPeriod with the timing line
  void LogCollision(float size)
  {
    if (!g_DiagHooked)
      return;
    DiagEntry entries[MaxDiagEntries];
    int count, dropped;
    long long casts;
    {
      std::lock_guard<std::mutex> lock(g_DiagMutex);
      count = g_DiagCount;
      dropped = g_DiagDropped;
      casts = g_DiagCasts.exchange(0);
      memcpy(entries, g_Diag, sizeof(DiagEntry) * count);
      g_DiagCount = 0;
      g_DiagDropped = 0;
    }
    __int64 base = (__int64)g_gameHandle;
    util::log::Write("Faith's size (collision diagnostic): at %.2f, %lld shape casts in all; %d kinds started within "
      "%.1f m of her feet%s", size, casts, count, g_DiagReach.load(), dropped ? " (more kinds than kept)" : "");
    // Most frequent first
    for (int shown = 0; shown < 24 && shown < count; ++shown)
    {
      int best = shown;
      for (int i = shown + 1; i < count; ++i)
        if (entries[i].Count > entries[best].Count)
          best = i;
      DiagEntry e = entries[best];
      entries[best] = entries[shown];
      CastRead const& c = e.Last;
      util::log::Write("Faith's size (collision diagnostic):   slot %d x%d, asked by 0x%I64X (the game's 0x%I64X): shape "
        "0x%I64X vt 0x%I64X type 0x%08X, +0x60 %.3f %.3f %.3f %.3f, own scale %.2f %.2f %.2f, own place %.3f %.3f %.3f; "
        "the cast's scale %.3f %.3f %.3f, from her feet %.3f %.3f %.3f, sweep %.3f %.3f %.3f, ids %u / %u, filter 0x%X",
        c.Path, e.Count, c.Caller, base + c.Caller, c.Shape, c.ShapeVTable, c.ShapeType, c.Params[0], c.Params[1],
        c.Params[2], c.Params[3], c.LocalScale[0], c.LocalScale[1], c.LocalScale[2], c.LocalPlace[0], c.LocalPlace[1],
        c.LocalPlace[2], c.Scale[0], c.Scale[1], c.Scale[2], c.Offset[0], c.Offset[1], c.Offset[2], c.Sweep[0],
        c.Sweep[1], c.Sweep[2], c.Id1, c.Id2, c.Filter);
    }
  }

  void LogTiming(ULONGLONG now, bool force)
  {
    if (!g_Timing.Hooks || (!force && now - g_Timing.Since < DiagnosticPeriod))
      return;
    util::log::Write("Faith's size (diagnostic): %d camera hooks in %.1f s; the game had written her body's world pose "
      "anew before %d of them (%.0f bones each), its skinning matrices before %d (%.0f bones each); %d came sized "
      "already (left alone)", g_Timing.Hooks, (now - g_Timing.Since) / 1000.0, g_Timing.WorldFresh,
      g_Timing.WorldFresh ? (double)g_Timing.WorldBones / g_Timing.WorldFresh : 0.0, g_Timing.MatricesFresh,
      g_Timing.MatricesFresh ? (double)g_Timing.MatricesBones / g_Timing.MatricesFresh : 0.0, g_Timing.AlreadySized);
    if (!force)
    {
      LogHair(g_Timing.Soldier, g_Timing.Size);
      LogCollision(g_Timing.Size);
      if (g_CollisionOn.load())
        LogCollisionSizing(g_Timing.Size);
    }
    g_Timing = Timing{};
    g_Timing.Since = now;
  }
}

void faith_size::Init()
{
  if (IsAvailable())
  {
    HookCollision();
    HookHair();
  }
}

bool faith_size::IsAvailable()
{
  std::call_once(g_Checked, [] { g_Available = CheckGame(); });
  return g_Available;
}

void faith_size::SetSize(float size)
{
  if (!(size == size))
    return;
  if (size < MinSize) size = MinSize;
  if (size > MaxSize) size = MaxSize;
  // What the menu shows as 1.00 is the game's own size
  if (fabsf(size - 1.f) < 0.005f)
    size = 1.f;
  g_Size.store(size);
}

float faith_size::GetSize()
{
  return g_Size.load();
}

bool faith_size::IsCollisionAvailable()
{
  return IsAvailable() && g_DiagHooked;
}

void faith_size::SetCollision(bool on)
{
  if (on == g_CollisionOn.load())
    return;
  g_CollisionOn.store(on);
  if (!on)
    g_CollisionSize.store(1.f);
  util::log::Write("Faith's size: collision follows size %s", on ? "ticked" : "unticked: her capsules are the game's own");
}

bool faith_size::GetCollision()
{
  return g_CollisionOn.load();
}

bool faith_size::IsHairAvailable()
{
  return IsAvailable() && g_HairHooked;
}

void faith_size::SetHair(bool on)
{
  if (on == g_HairOn.load())
    return;
  g_HairOn.store(on);
  if (!on)
    g_HairSize.store(1.f, std::memory_order_release);
  util::log::Write("Faith's size: hair follows size %s", on ? "ticked" : "unticked: her cloths are their normal size");
}

bool faith_size::GetHair()
{
  return g_HairOn.load();
}

void faith_size::Update(__int64 soldier, bool replayBusy, __int64 gameView)
{
  if (!IsAvailable())
    return;
  float size = g_Size.load();
  ULONGLONG now = GetTickCount64();
  if (size != g_SeenSize)
  {
    g_SeenSize = size;
    g_SizeChangedAt = now;
  }
  bool wanted = size != 1.f && !replayBusy && soldier >= 0x10000;

  Slot body{}, box{};
  bool bodyOk = false, boxOk = false;
  if (soldier >= 0x10000)
    FindSlots(soldier, body, bodyOk, box, boxOk);

  // Her body: the world pose and matrices
  if (wanted && bodyOk)
  {
    if (!g_Sizing)
    {
      g_Timing = Timing{};
      g_Timing.Since = now;
      if (!body.Matrices && !g_LoggedNoMatrices)
      {
        util::log::Warning("Faith's size: her pose manager keeps no skinning matrices (manager + 0x2B9), only her world "
          "pose is sized");
        g_LoggedNoMatrices = true;
      }
    }
    SizePose(g_BodyPose, body, size, true);
    if (g_HairHooked && g_HairOn.load())
      ShowHairPlaces(body, size);
    else
      g_HairSize.store(1.f, std::memory_order_release);
    for (int k = 0; k < 3; ++k)
      g_DiagRoot[k].store(g_ReadWorld[RootBone][k]);
    g_DiagReach.store(3.f * (size > 1.f ? size : 1.f));
    g_DiagOn.store(g_DiagHooked);
    g_CollisionSize.store(g_CollisionOn.load() ? size : 1.f);
    if (g_CollisionOn.load())
    {
      PickHerId(now);
      if (!g_LoggedCollisionOn)
        util::log::Write("Faith's size: collision follows size (%.2f): her capsules are handed to the game sized", size);
      g_LoggedCollisionOn = true;
    }
    g_Timing.Soldier = soldier;
    g_Timing.Size = size;
    SizeCamera(gameView, size);
    if (boxOk)
      SizePose(g_BoxPose, box, size, false);
    else
      PutBackPose(g_BoxPose, nullptr);
    LogTiming(now, false);
  }
  else
  {
    g_HairSize.store(1.f, std::memory_order_release);
    g_DiagOn.store(false);
    g_CollisionSize.store(1.f);
    if (g_LoggedCollisionOn)
      util::log::Write("Faith's size: her capsules are the game's own again");
    g_LoggedCollisionOn = false;
    g_CameraHave = false;
    g_LoggedCamera = false;
    if (g_Sizing)
      LogTiming(now, true);
    int putBack = PutBackPose(g_BodyPose, bodyOk ? &body : nullptr);
    putBack += PutBackPose(g_BoxPose, boxOk ? &box : nullptr);
    if (g_Sizing)
    {
      if (size == 1.f)
        util::log::Write("Faith's size: back to the game's own (1.00); %d bones' world places and matrices put back as "
          "the game had them", putBack);
      else if (replayBusy)
        util::log::Write("Faith's size: %.2f steps aside while a replay records or plays (%d bones put back)", size,
          putBack);
      else
        util::log::Write("Faith's size: her pose slot is gone (soldier 0x%I64X)", soldier);
    }
  }

  // Her cloths' lengths (the hook sizes their anchors)
  if (g_HairHooked)
    UpdateHairLengths(soldier, bodyOk ? body.Pose : 0, wanted && bodyOk && g_HairOn.load() ? size : 1.f);

  // Her hair: bone 1's scale, the size while sized, then 1.00 until her pose
  // reads 1.00 again
  bool restoring = false;
  if (!wanted && g_ScaleDirty && bodyOk && !replayBusy)
  {
    float bodyScale = RootScale(body), boxScale = boxOk ? RootScale(box) : 1.f;
    if (bodyScale == 1.f && boxScale == 1.f)
    {
      util::log::Write("Faith's size: bone 1's scale reads 1.00 again in her pose (after %d frames), the override "
        "taken back", g_RestoreFrames);
      g_ScaleDirty = false;
    }
    else if (++g_RestoreFrames > MaxRestoreFrames)
    {
      util::log::Warning("Faith's size: bone 1's scale still reads %.3f / %.3f after %d frames of 1.00, the override "
        "taken back anyway", bodyScale, boxScale, MaxRestoreFrames);
      g_ScaleDirty = false;
    }
    else
      restoring = true;
  }
  if (!g_ScaleDirty)
    g_RestoreFrames = 0;
  bool handing = (wanted || restoring) && bodyOk;
  float scale = wanted ? size : 1.f;
  Drop(g_Body, handing, bodyOk, body);
  Drop(g_Box, handing, boxOk, box);

  if (wanted && !bodyOk)
  {
    if (!g_LoggedNoSlot)
      util::log::Warning("Faith's size: her pose slot isn't found (soldier 0x%I64X), her size stays the game's", soldier);
    g_LoggedNoSlot = true;
  }
  else if (bodyOk)
    g_LoggedNoSlot = false;

  bool bodyIn = false, boxIn = false;
  if (handing)
  {
    bodyIn = Hand(g_Body, body, scale);
    boxIn = boxOk && Hand(g_Box, box, scale);
    if (wanted && (bodyIn || boxIn))
      g_ScaleDirty = true;
    if (!bodyIn && !g_Body.Refused)
    {
      util::log::Warning("Faith's size: her body's pose slot 0x%X holds another override (or can't be written), "
        "her hair's size stays the game's", body.Handle);
      g_Body.Refused = true;
    }
    if (boxOk && !boxIn && !g_Box.Refused)
    {
      util::log::Warning("Faith's size: her box pose's slot 0x%X holds another override (or can't be written)",
        box.Handle);
      g_Box.Refused = true;
    }
  }
  if (wanted && bodyOk && !boxOk && !g_LoggedNoBox)
  {
    util::log::Warning("Faith's size: her box pose (soldier + 0x340) isn't found, only her body is sized (she may be "
      "culled by her normal-size box)");
    g_LoggedNoBox = true;
  }
  if (boxOk)
    g_LoggedNoBox = false;

  bool sizing = wanted && bodyOk;
  if (sizing && !g_Sizing)
  {
    util::log::Write("Faith's size: %.2f: her world pose and skinning matrices sized about bone 1 (body slot 0x%X, "
      "box slot 0x%X%s), bone 1's scale %s through the game's pose override for her hair (soldier 0x%I64X)", size,
      body.Handle, boxOk ? box.Handle : 0xFFFFFFFFu, boxOk ? "" : " not found", bodyIn ? "handed" : "not handed",
      soldier);
    g_LoggedSize = size;
  }
  else if (sizing && size != g_LoggedSize && now - g_SizeChangedAt >= SizeLogDelay)
  {
    util::log::Write("Faith's size: now %.2f", size);
    g_LoggedSize = size;
  }
  if (!sizing && size == 1.f)
    g_LoggedSize = 1.f;
  g_Sizing = sizing;
}
