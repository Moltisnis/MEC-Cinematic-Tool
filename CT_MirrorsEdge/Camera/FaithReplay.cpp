#include "FaithReplay.h"
#include "AttachTargets.h"
#include "FaithOutfit.h"
#include "ReplayAssetLog.h"
#include "ReplayAudio.h"
#include "ReplayBundles.h"
#include "ReplayDrawLog.h"
#include "ReplayGameDraws.h"
#include "ReplayShapes.h"
#include "ReplayStore.h"
#include "../Main.h"
#include "../Util/GameMessages.h"
#include "../Util/Util.h"
#include "../imgui/imgui.h"
#include "../Util/ImGuiEXT.h"
#include <Windows.h>
#include <algorithm>
#include <chrono>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <cstdint>
#include <deque>
#include <limits>
#include <map>
#include <malloc.h>
#include <new>
#include <tuple>
#include <DirectXPackedVector.h>

using namespace DirectX;

namespace
{
  // The sound track's dialog spans widened by this much before and after each
  // line (2026-11-01: it keeps only the dialog)
  const double DialogBeforeSeconds = 0.25, DialogAfterSeconds = 0.6;

  // Faith's pose, the way the game's own replay components get at it (PE
  // timestamp 0x5762E3B7 only). Her soldier keeps its
  // PamClientReplayRecordingComponent at +0x4B0. The component's owner (+0x50)
  // is her ClientAntAnimatableEntity, whose pose handle is
  // [[animatable + 0x158] + 0x118] (byte 3 a page, byte 0 a slot), and +0x98
  // is the pose manager of her skeleton (she and the Dash ghosts share it).
  // The manager's pages (+0x210) hold every pose: at page + 0x00 the local
  // pose, 0x20 bytes per bone (translation x y z 1, then a rotation
  // quaternion, each relative to the bone's parent; bone 1 is where she is in
  // the world), slot * (manager + 0x8C) bytes in, and at page + 0x30 one
  // override per slot: the animation system takes the listed bones from it
  // instead of from her animation (the ghost player component registers its
  // own at +0x1D8 every update, 0x142FCEE50).
  const DWORD ReplayTimeStamp = 0x5762E3B7;
  const int SoldierRecordingComponentOffset = 0x4B0;
  const __int64 RecordingComponentVTableRva = 0x1C48CD0;
  const __int64 AnimatableTypeInfoRva = 0x27EEE10;  // ClientAntAnimatableEntity
  const __int64 GameAllocatorRva = 0x2364F80;       // what the game's own override vectors carry
  const int ComponentAnimatableOffset = 0x50;
  const int ComponentManagerOffset = 0x98;
  const int AnimatablePoseOffset = 0x158;
  const int PoseHandleOffset = 0x118;
  const int ManagerPoseStrideOffset = 0x8C;
  const int ManagerPagesOffset = 0x210;              // vector of pages: begin, end
  const int PageLocalPoseOffset = 0x00;
  const int PageWorldPoseOffset = 0x08;              // the same in world space, the same stride
  const int PageOverridesOffset = 0x30;
  const int MaxBones = 512;

  // Her box pose. Faith has a second ClientAntAnimatableEntity (soldier +
  // 0x340), the same skeleton in the same pose manager, and her bounding box
  // is made of its bones: at creation her character (PamClientCharacterEntity
  // vtable slot 65, 0x143916B40) registers PamCharacterEntityData's
  // BoundingBoxBones (camera joint, head end, feet, legs, arms) of it with the
  // joint output manager [0x14257C180]. The replay's own animatable has none,
  // so with only that one overridden the box stayed where she really is, and
  // her body wasn't drawn when that wasn't in view. That pose gets the replay
  // too, with her own root and trajectory (bones 0 - 3: Reference,
  // AITrajectory, Trajectory, TrajectoryEnd) and her camera left as they are
  // (see OwnCameraBones), and the other bones under bone 1 (Hips, Connect)
  // placed so they end up where the replayed ones are: everything below them
  // follows. Skeleton_Faith only.
  const int SoldierBoxAnimatableOffset = 0x340;
  const int AnimatableManagerOffset = 0x1B8;         // Animatable -> its pose manager
  const int FaithBones = 182;
  const int OwnRootBones = 4;                        // bones 0 - 3 stay hers
  const int RootChildren[] = { 4, 180 };             // under bone 1, besides Trajectory and the camera
  // Her camera (178 CameraBase, under bone 1, and 179 CameraJoint under it)
  // stays her own in the box pose: the camera joint is where her camera
  // component is (+ 0x80), and with it at the replayed body the game took her
  // to be there (2026-10-24: the position the level tracks for the player
  // followed the replayed Faith 100 - 400 m from the real one all through
  // every playback, 2 m from her otherwise; KrugerSec near a replayed fight
  // went after the real Faith and a chase started). The box is then made of
  // her real camera joint and the replayed bones: big enough to still hold
  // the replayed body.
  const int OwnCameraBones[] = { 178, 179 };
  const int FloatsPerBone = 8;
  const double SampleInterval = 1.0 / 30.0;          // like the game's own recorder
  const double MaxRecordSeconds = 600.0;             // about 100 MB

  bool IsReplayBuild()
  {
    IMAGE_DOS_HEADER* pDos = (IMAGE_DOS_HEADER*)g_gameHandle;
    IMAGE_NT_HEADERS* pNt = (IMAGE_NT_HEADERS*)((BYTE*)g_gameHandle + pDos->e_lfanew);
    return pNt->FileHeader.TimeDateStamp == ReplayTimeStamp;
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

  struct PoseSlot
  {
    __int64 Manager;
    unsigned int Handle;
    int Bones;
    float const* Local;     // her local pose this frame
    float const* World;     // the same in world space (0 if the page has none)
    __int64* Overrides;     // the page's override per slot
  };

  // An animatable's pose slot in a pose manager, checked all the way. Game
  // memory, so only inside __try (the callers').
  bool SlotOf(__int64 manager, __int64 animatable, PoseSlot& slot)
  {
    __int64 pose = *(__int64*)(animatable + AnimatablePoseOffset);
    if (pose < 0x10000)
      return false;
    unsigned int handle = *(unsigned int*)(pose + PoseHandleOffset);
    int stride = *(int*)(manager + ManagerPoseStrideOffset);
    if (handle == 0xFFFFFFFF || stride <= 0 || stride % (FloatsPerBone * 4) || stride / (FloatsPerBone * 4) > MaxBones)
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
    __int64 overrides = *(__int64*)(pPage + PageOverridesOffset);
    if (local < 0x10000 || overrides < 0x10000)
      return false;
    slot.Manager = manager;
    slot.Handle = handle;
    slot.Bones = stride / (FloatsPerBone * 4);
    slot.Local = (float const*)(local + (__int64)index * stride);
    slot.World = world >= 0x10000 ? (float const*)(world + (__int64)index * stride) : nullptr;
    slot.Overrides = (__int64*)overrides;
    return true;
  }

  // Faith's pose slot (the body that's drawn), checked all the way
  bool FindPose(__int64 soldier, PoseSlot& slot)
  {
    __int64 base = (__int64)g_gameHandle;
    __try
    {
      if (soldier < 0x10000)
        return false;
      __int64 component = *(__int64*)(soldier + SoldierRecordingComponentOffset);
      if (component < 0x10000 || *(__int64*)component != base + RecordingComponentVTableRva)
        return false;
      __int64 animatable = *(__int64*)(component + ComponentAnimatableOffset);
      __int64 manager = *(__int64*)(component + ComponentManagerOffset);
      if (animatable < 0x10000 || manager < 0x10000 || TypeOf(animatable) != base + AnimatableTypeInfoRva)
        return false;
      return SlotOf(manager, animatable, slot);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
  }

  // Her box pose's slot (see SoldierBoxAnimatableOffset): another slot of the
  // same manager, with as many bones
  bool FindBoxPose(__int64 soldier, PoseSlot const& body, PoseSlot& slot)
  {
    __int64 base = (__int64)g_gameHandle;
    __try
    {
      __int64 animatable = *(__int64*)(soldier + SoldierBoxAnimatableOffset);
      if (animatable < 0x10000 || TypeOf(animatable) != base + AnimatableTypeInfoRva)
        return false;
      __int64 pose = *(__int64*)(animatable + AnimatablePoseOffset);
      if (pose < 0x10000 || *(__int64*)(pose + AnimatableManagerOffset) != body.Manager)
        return false;
      return SlotOf(body.Manager, animatable, slot) && slot.Handle != body.Handle && slot.Bones == body.Bones;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
  }

  bool CopyPose(float const* source, float* target, int bones)
  {
    __try
    {
      memcpy(target, source, (size_t)bones * FloatsPerBone * sizeof(float));
      return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
  }

  // The bone where she is in the world (every other bone hangs off it)
  const int RootBone = 1;

  // The replayed root into her own pose as well, not only the override, so
  // what reads this pose (her hair cloth, its box and its simulation) is at
  // the replayed body, next to the head it hangs from. Her whole skeleton
  // follows the override anyway. Whether her body is drawn is decided by her
  // box pose (see SoldierBoxAnimatableOffset), not by this one.
  bool WriteRoot(float const* local, float const* bones)
  {
    __try
    {
      memcpy((float*)local + RootBone * FloatsPerBone, bones + RootBone * FloatsPerBone, FloatsPerBone * sizeof(float));
      return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
  }

  // The game's override, laid out like the ghost player component's (+0x1D8)
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
  static_assert(offsetof(PoseOverride, Weight) == 0x18 && offsetof(PoseOverride, Handle) == 0x20, "the game's layout");

  // Two, filled in turn: one is written while the animation system can still
  // be reading the other
  struct OverrideSet
  {
    int Bones;
    PoseOverride Override[2];
    float* Buffer[2];
    unsigned int* Indices;
    GameVector List, Map;
  };
  OverrideSet g_Override{};     // the body that's drawn
  OverrideSet g_BoxOverride{};  // her box pose

  bool PrepareOverride(OverrideSet& set, int bones)
  {
    if (set.Bones == bones)
      return true;
    for (int k = 0; k < 2; ++k)
    {
      if (set.Buffer[k]) _aligned_free(set.Buffer[k]);
      set.Buffer[k] = nullptr;
    }
    if (set.Indices) _aligned_free(set.Indices);
    set.Indices = nullptr;
    set.Bones = 0;

    size_t bytes = (size_t)bones * FloatsPerBone * sizeof(float);
    set.Indices = (unsigned int*)_aligned_malloc((size_t)bones * sizeof(unsigned int), 16);
    for (int k = 0; k < 2; ++k)
      set.Buffer[k] = (float*)_aligned_malloc(bytes, 128);
    if (!set.Indices || !set.Buffer[0] || !set.Buffer[1])
      return false;
    for (int i = 0; i < bones; ++i)
      set.Indices[i] = (unsigned int)i;
    __int64 allocator = (__int64)g_gameHandle + GameAllocatorRva;
    // Every bone, in its own place: the list and the map are the same
    set.List = { allocator, set.Indices, (unsigned int)bones, (unsigned int)bones };
    set.Map = { allocator, set.Indices, (unsigned int)bones, (unsigned int)bones };
    for (int k = 0; k < 2; ++k)
    {
      memset(set.Buffer[k], 0, bytes);
      set.Override[k] = { set.Buffer[k], &set.List, &set.Map, 1.f, (unsigned int)bones, -1, 0 };
    }
    set.Bones = bones;
    return true;
  }

  // Hands the override to her slot (what 0x142FCEE50 does)
  bool RegisterOverride(PoseSlot const& slot, PoseOverride* pOverride)
  {
    __try
    {
      pOverride->Handle = (int)slot.Handle;
      slot.Overrides[slot.Handle & 0xFF] = (__int64)pOverride;
      return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
  }

  // Takes ours out of the slot it was handed to, if it's still there (the
  // game clears it itself when it releases the slot, at a level load)
  void UnregisterOverride(OverrideSet& set, __int64* overrides, unsigned int handle)
  {
    __try
    {
      __int64& entry = overrides[handle & 0xFF];
      if (entry == (__int64)&set.Override[0] || entry == (__int64)&set.Override[1])
        entry = 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
    set.Override[0].Handle = set.Override[1].Handle = -1;
  }

  // Bone transforms: translation x y z (1), then a rotation quaternion x y z
  // w. A bone's world transform is its parent's times its own (worked out
  // from her live pose: her camera joint comes out exactly where her camera
  // component is, her foot on the ground).
  void QuatMul(float const* a, float const* b, float* out)
  {
    float x = a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1];
    float y = a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0];
    float z = a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3];
    float w = a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2];
    out[0] = x; out[1] = y; out[2] = z; out[3] = w;
  }

  void QuatRotate(float const* q, float const* v, float* out)
  {
    // v + 2w (u x v) + 2 u x (u x v), u = q.xyz
    float c1[3] = { q[1] * v[2] - q[2] * v[1], q[2] * v[0] - q[0] * v[2], q[0] * v[1] - q[1] * v[0] };
    float c2[3] = { q[1] * c1[2] - q[2] * c1[1], q[2] * c1[0] - q[0] * c1[2], q[0] * c1[1] - q[1] * c1[0] };
    for (int k = 0; k < 3; ++k)
      out[k] = v[k] + 2.f * q[3] * c1[k] + 2.f * c2[k];
  }

  // A child of bone 1 put where it is under the replayed bone 1, but under
  // her own bone 1: own^-1 * replayed * local
  void Reparent(float const* own, float const* replayed, float const* local, float* out)
  {
    float t[3], r[4];
    QuatRotate(replayed + 4, local, t);
    for (int k = 0; k < 3; ++k)
      t[k] += replayed[k];
    QuatMul(replayed + 4, local + 4, r);
    float inverse[4] = { -own[4], -own[5], -own[6], own[7] };
    float d[3] = { t[0] - own[0], t[1] - own[1], t[2] - own[2] };
    QuatRotate(inverse, d, out);
    out[3] = 1.f;
    QuatMul(inverse, r, out + 4);
  }

  // Her box pose for this frame: the replayed pose, with her own bones 0 - 3
  // and her own camera (read from her box pose's local pose) and the other
  // children of bone 1 moved under them
  bool FillBoxPose(float const* replayed, float const* ownLocal, float* out, int bones)
  {
    __try
    {
      memcpy(out, replayed, (size_t)bones * FloatsPerBone * sizeof(float));
      memcpy(out, ownLocal, (size_t)OwnRootBones * FloatsPerBone * sizeof(float));
      for (int bone : OwnCameraBones)
        if (bone < bones)
          memcpy(out + bone * FloatsPerBone, ownLocal + bone * FloatsPerBone, FloatsPerBone * sizeof(float));
      float const* own = out + 1 * FloatsPerBone;
      float const* replayedRoot = replayed + 1 * FloatsPerBone;
      for (int child : RootChildren)
        Reparent(own, replayedRoot, replayed + child * FloatsPerBone, out + child * FloatsPerBone);
      return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
  }

  double NowSeconds()
  {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
  }

  // The recording's cost on the game thread by part (2026-11-04, the user:
  // "find out why fps drop when recording a replay and fix the fps issue no
  // mistakes"; the camera hook's line has the replay's whole only): each part
  // of FaithReplay::Update while it records, its time a frame and its most in
  // one frame, in the log every RecordCostLogMs and when it stops; the frames
  // it starts and stops in on their own
  enum RecordPart
  {
    Rec_WorldTable, Rec_Levels, Rec_AssetLog, Rec_Bundles, Rec_Pose, Rec_Models, Rec_People, Rec_GameDraws, Rec_Vehicles,
    Rec_Sounds, Rec_Effects, Rec_Items, Rec_Rope, Rec_Loads, Rec_Parts
  };
  char const* const RecordPartNames[Rec_Parts] = {
    "the game's mesh table read (once a second)", "the recorded models' other levels (once a second)",
    "the asset log (the mesh table and its draw notes once a second)", "the bundles", "her pose found and sampled",
    "the moving models (read each sample, looked for once a second)", "the people (each sample, looked for once a second)",
    "the game's draws matched to the people", "the vehicles", "her sounds", "the gun effects", "the mission items", "her MagRope",
    "the loads noted" };
  const ULONGLONG RecordCostLogMs = 10000;
  struct RecordCost
  {
    long long Frame[Rec_Parts] = {};
    long long Total[Rec_Parts] = {}, Most[Rec_Parts] = {};
    long long Work = 0, WorkMost = 0, Rest = 0, RestMost = 0;
    int Frames = 0, Over2 = 0, Over5 = 0;
    ULONGLONG Since = 0;
  } g_RecordCost;

  long long Ticks()
  {
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return t.QuadPart;
  }

  double TicksMs(long long ticks)
  {
    static LARGE_INTEGER frequency{};
    if (!frequency.QuadPart)
      QueryPerformanceFrequency(&frequency);
    return ticks * 1000.0 / frequency.QuadPart;
  }

  struct RecordTimer
  {
    int Part;
    long long From;
    explicit RecordTimer(int part) : Part(part), From(Ticks())
    {
    }
    ~RecordTimer()
    {
      g_RecordCost.Frame[Part] += Ticks() - From;
    }
  };

  void LogRecordCost()
  {
    RecordCost& c = g_RecordCost;
    double frames = c.Frames;
    char text[2048];
    int at = _snprintf_s(text, _TRUNCATE, "Replay (recording cost, game thread, diagnostic): %d frames in %.1f s; its work %.3f ms a "
      "frame, the most in a frame %.2f ms (%d frames over 2 ms, %d over 5 ms); by part, ms a frame (the most in a frame):", c.Frames,
      (GetTickCount64() - c.Since) / 1000.0, TicksMs(c.Work) / frames, TicksMs(c.WorkMost), c.Over2, c.Over5);
    for (int p = 0; p < Rec_Parts && at > 0 && at < (int)sizeof(text); ++p)
    {
      int n = _snprintf_s(text + at, sizeof(text) - at, _TRUNCATE, " %s %.3f (%.2f),", RecordPartNames[p], TicksMs(c.Total[p]) / frames,
        TicksMs(c.Most[p]));
      at = n < 0 ? -1 : at + n;
    }
    if (at > 0 && at < (int)sizeof(text))
      _snprintf_s(text + at, sizeof(text) - at, _TRUNCATE, " the rest %.3f (%.2f)", TicksMs(c.Rest) / frames, TicksMs(c.RestMost));
    util::log::Write("%s", text);
  }

  // The end of a frame of Update: recording before and after it, its whole time
  void EndRecordFrame(bool wasRecording, bool isRecording, long long whole)
  {
    RecordCost& c = g_RecordCost;
    if (wasRecording && isRecording)
    {
      if (!c.Frames)
      {
        c.Since = GetTickCount64();
        memset(c.Total, 0, sizeof(c.Total));
        memset(c.Most, 0, sizeof(c.Most));
        c.Work = c.WorkMost = c.Rest = c.RestMost = 0;
        c.Over2 = c.Over5 = 0;
      }
      ++c.Frames;
      long long parts = 0;
      for (int p = 0; p < Rec_Parts; ++p)
      {
        c.Total[p] += c.Frame[p];
        c.Most[p] = (std::max)(c.Most[p], c.Frame[p]);
        parts += c.Frame[p];
      }
      long long rest = (std::max)(0ll, whole - parts);
      c.Rest += rest;
      c.RestMost = (std::max)(c.RestMost, rest);
      c.Work += whole;
      c.WorkMost = (std::max)(c.WorkMost, whole);
      c.Over2 += TicksMs(whole) > 2.0;
      c.Over5 += TicksMs(whole) > 5.0;
      if (GetTickCount64() - c.Since >= RecordCostLogMs)
      {
        LogRecordCost();
        c.Frames = 0;
      }
    }
    else
    {
      // Stopped: what's left of the last 10 s
      if (c.Frames && !isRecording)
      {
        LogRecordCost();
        c.Frames = 0;
      }
      if (wasRecording != isRecording)
        util::log::Write("Replay (recording cost, game thread, diagnostic): the frame it %s recording in took %.1f ms of the replay's "
          "work", isRecording ? "started" : "stopped", TicksMs(whole));
    }
    memset(c.Frame, 0, sizeof(c.Frame));
  }

  // The two frames around a time (count of them, at least 2) and how far
  // from the first to the second it is (0 - 1)
  void SpanAt(std::vector<double> const& times, size_t count, double time, size_t& prev, size_t& next, float& f)
  {
    next = 1;
    if (time <= times[0])
      next = 1;
    else if (time >= times[count - 1])
      next = count - 1;
    else
    {
      size_t lo = 0, hi = count - 1;
      while (hi - lo > 1)
      {
        size_t mid = (lo + hi) / 2;
        if (times[mid] <= time) lo = mid; else hi = mid;
      }
      next = hi;
    }
    prev = next - 1;
    double span = times[next] - times[prev];
    f = span > 0 ? (float)((time - times[prev]) / span) : 0.f;
    if (f < 0.f) f = 0.f;
    if (f > 1.f) f = 1.f;
  }

  // Between two poses: translations in a straight line, rotations along the
  // shorter way round
  void BlendPose(float const* a, float const* b, int bones, float f, float* out);

  // A recorded pose at a time (see BlendPose)
  void SamplePose(std::vector<double> const& times, std::vector<float> const& frames, int bones, double time, float* out)
  {
    size_t count = times.size();
    if (count == 1)
    {
      memcpy(out, frames.data(), (size_t)bones * FloatsPerBone * sizeof(float));
      return;
    }
    size_t prev, next;
    float f;
    SpanAt(times, count, time, prev, next, f);
    size_t frameFloats = (size_t)bones * FloatsPerBone;
    BlendPose(&frames[prev * frameFloats], &frames[next * frameFloats], bones, f, out);
  }

  // A recorded person's frames, packed (2026-10-25: a 335 s fight with 48
  // people and 39 guns and drones had used the 512 MB for them up 222 s into
  // it, at 32 bytes a bone): bones 0 - 3 as they are (8 floats; bone 1 is
  // where they are in the world), every other bone's rotation as four 16-bit
  // numbers (x 32767: within 0.004 degrees) and its translation from its
  // parent as three half floats (within 0.5 mm a metre), and the float that's
  // always 1 left out: 14 bytes. In blocks of FramesPerBlock frames, so
  // there's no unused room past the last block, and nothing is copied as it
  // grows.
  const int ExactBones = 4;
  const int PackedBoneBytes = 14;
  const size_t FramesPerBlock = 64;

  class PackedFrames
  {
  public:
    // What one frame of a body with this many bones takes (a whole number of
    // floats, so each frame's bones 0 - 3 are floats where floats can be)
    static size_t FrameBytes(int bones)
    {
      int exact = bones < ExactBones ? bones : ExactBones;
      size_t bytes = (size_t)exact * FloatsPerBone * sizeof(float) + (size_t)(bones - exact) * PackedBoneBytes;
      return (bytes + sizeof(float) - 1) & ~(sizeof(float) - 1);
    }

    size_t Count() const { return m_Count; }

    // Saved recordings (ReplayStore.h): the frames as stored, one after
    // another (FrameBytes(bones) each), and frames read back in place of these
    // (false if the sizes don't agree; on std::bad_alloc nothing is kept)
    size_t StoredFrameBytes() const { return m_FrameBytes; }
    void CopyTo(std::vector<BYTE>& out) const
    {
      out.resize(m_Count * m_FrameBytes);
      for (size_t k = 0; k < m_Count; ++k)
        memcpy(out.data() + k * m_FrameBytes, At(k), m_FrameBytes);
    }
    bool Assign(int bones, size_t count, BYTE const* data, size_t size)
    {
      size_t frameBytes = FrameBytes(bones);
      if (bones <= 0 || size != count * frameBytes)
        return false;
      m_Blocks.clear();
      m_Count = 0;
      m_Bones = bones;
      m_Exact = bones < ExactBones ? bones : ExactBones;
      m_FrameBytes = frameBytes;
      for (size_t k = 0; k < count; ++k)
      {
        BYTE* slot = NextSlot();
        memcpy(slot, data + k * frameBytes, frameBytes);
        ++m_Count;
      }
      return true;
    }

    // A frame (bones x FloatsPerBone floats) at the end. On std::bad_alloc
    // nothing has changed.
    void Append(float const* frame, int bones)
    {
      if (!m_Count)
      {
        m_Bones = bones;
        m_Exact = bones < ExactBones ? bones : ExactBones;
        m_FrameBytes = FrameBytes(bones);
      }
      BYTE* slot = NextSlot();
      Pack(frame, slot);
      ++m_Count;
    }

    // The last frame again at the end (likewise)
    void RepeatLast()
    {
      if (!m_Count)
        return;
      BYTE* slot = NextSlot();
      memcpy(slot, At(m_Count - 1), m_FrameBytes);
      ++m_Count;
    }

    // Whether a frame is the last one as it would be stored (game thread)
    bool SameAsLast(float const* frame) const
    {
      if (!m_Count)
        return false;
      static std::vector<BYTE> packed;
      packed.resize(m_FrameBytes);
      Pack(frame, packed.data());
      return !memcmp(packed.data(), At(m_Count - 1), m_FrameBytes);
    }

    // Only the first count frames kept (doesn't allocate)
    void Truncate(size_t count)
    {
      if (count >= m_Count)
        return;
      m_Count = count;
      size_t blocks = (count + FramesPerBlock - 1) / FramesPerBlock;
      while (m_Blocks.size() > blocks)
        m_Blocks.pop_back();
    }

    // Frame k (the last one past the end) as bones x FloatsPerBone floats
    void Decode(size_t k, float* out) const
    {
      if (!m_Count)
      {
        for (int i = 0; i < m_Bones; ++i)
        {
          float* o = out + (size_t)i * FloatsPerBone;
          o[0] = o[1] = o[2] = o[4] = o[5] = o[6] = 0.f;
          o[3] = o[7] = 1.f;
        }
        return;
      }
      BYTE const* p = At(k < m_Count ? k : m_Count - 1);
      size_t exactBytes = (size_t)m_Exact * FloatsPerBone * sizeof(float);
      memcpy(out, p, exactBytes);
      p += exactBytes;
      for (int i = m_Exact; i < m_Bones; ++i, p += PackedBoneBytes)
      {
        float* o = out + (size_t)i * FloatsPerBone;
        short q[4];
        DirectX::PackedVector::HALF t[3];
        memcpy(q, p, sizeof(q));
        memcpy(t, p + sizeof(q), sizeof(t));
        for (int c = 0; c < 3; ++c)
          o[c] = DirectX::PackedVector::XMConvertHalfToFloat(t[c]);
        o[3] = 1.f;
        float length = 0.f;
        for (int c = 0; c < 4; ++c)
        {
          o[4 + c] = q[c] / 32767.f;
          length += o[4 + c] * o[4 + c];
        }
        if (length > 1e-12f)
        {
          length = 1.f / sqrtf(length);
          for (int c = 0; c < 4; ++c)
            o[4 + c] *= length;
        }
        else
        {
          o[4] = o[5] = o[6] = 0.f;
          o[7] = 1.f;
        }
      }
    }

    // Bones 0 - 3 of frame k (the last one past the end), as recorded
    float const* Exact(size_t k) const
    {
      static const float none[ExactBones * FloatsPerBone] = {};
      if (!m_Count)
        return none;
      return (float const*)At(k < m_Count ? k : m_Count - 1);
    }

  private:
    BYTE const* At(size_t k) const { return m_Blocks[k / FramesPerBlock].get() + (k % FramesPerBlock) * m_FrameBytes; }
    BYTE* At(size_t k) { return m_Blocks[k / FramesPerBlock].get() + (k % FramesPerBlock) * m_FrameBytes; }

    // Where the next frame goes, a new block made for it when needed (on
    // std::bad_alloc nothing has changed)
    BYTE* NextSlot()
    {
      if (m_Count / FramesPerBlock >= m_Blocks.size())
      {
        if (m_Blocks.size() == m_Blocks.capacity())
          m_Blocks.reserve(m_Blocks.capacity() * 2 + 8);
        std::unique_ptr<BYTE[]> block(new BYTE[FramesPerBlock * m_FrameBytes]);
        m_Blocks.push_back(std::move(block));
      }
      return At(m_Count);
    }

    void Pack(float const* frame, BYTE* out) const
    {
      size_t exactBytes = (size_t)m_Exact * FloatsPerBone * sizeof(float);
      memcpy(out, frame, exactBytes);
      out += exactBytes;
      for (int i = m_Exact; i < m_Bones; ++i, out += PackedBoneBytes)
      {
        float const* b = frame + (size_t)i * FloatsPerBone;
        short q[4];
        DirectX::PackedVector::HALF t[3];
        for (int c = 0; c < 4; ++c)
        {
          // Within -1 - 1 (0 for NaN)
          float v = b[4 + c];
          v = v == v ? (v < -1.f ? -1.f : (v > 1.f ? 1.f : v)) : 0.f;
          q[c] = (short)lrintf(v * 32767.f);
        }
        for (int c = 0; c < 3; ++c)
        {
          // Within what a half float holds (0 for NaN)
          float v = b[c];
          v = v == v ? (v < -65000.f ? -65000.f : (v > 65000.f ? 65000.f : v)) : 0.f;
          t[c] = DirectX::PackedVector::XMConvertFloatToHalf(v);
        }
        memcpy(out, q, sizeof(q));
        memcpy(out + sizeof(q), t, sizeof(t));
      }
    }

    std::vector<std::unique_ptr<BYTE[]>> m_Blocks;
    size_t m_Count{ 0 };
    size_t m_FrameBytes{ 0 };
    int m_Bones{ 0 };
    int m_Exact{ 0 };
  };

  // A person's recorded pose at a time (see BlendPose; game thread)
  void SamplePacked(std::vector<double> const& times, PackedFrames const& frames, int bones, double time, float* out)
  {
    size_t count = (std::min)(times.size(), frames.Count());
    if (count <= 1)
    {
      frames.Decode(0, out);
      return;
    }
    size_t prev, next;
    float f;
    SpanAt(times, count, time, prev, next, f);
    static std::vector<float> a, b;
    size_t frameFloats = (size_t)bones * FloatsPerBone;
    a.resize(frameFloats);
    b.resize(frameFloats);
    frames.Decode(prev, a.data());
    frames.Decode(next, b.data());
    BlendPose(a.data(), b.data(), bones, f, out);
  }

  void BlendPose(float const* a, float const* b, int bones, float f, float* out)
  {
    for (int i = 0; i < bones; ++i)
    {
      float const* pa = a + i * FloatsPerBone;
      float const* pb = b + i * FloatsPerBone;
      float* o = out + i * FloatsPerBone;
      for (int k = 0; k < 3; ++k)
        o[k] = pa[k] + (pb[k] - pa[k]) * f;
      o[3] = 1.f;
      float dot = pa[4] * pb[4] + pa[5] * pb[5] + pa[6] * pb[6] + pa[7] * pb[7];
      float sign = dot < 0.f ? -1.f : 1.f;
      float q[4], length = 0.f;
      for (int k = 0; k < 4; ++k)
      {
        q[k] = pa[4 + k] + (sign * pb[4 + k] - pa[4 + k]) * f;
        length += q[k] * q[k];
      }
      length = length > 1e-12f ? 1.f / sqrtf(length) : 0.f;
      for (int k = 0; k < 4; ++k)
        o[4 + k] = length > 0.f ? q[k] * length : (k == 3 ? 1.f : 0.f);
    }
  }

  void FreeOverride(OverrideSet& set)
  {
    for (int k = 0; k < 2; ++k)
    {
      if (set.Buffer[k]) _aligned_free(set.Buffer[k]);
      set.Buffer[k] = nullptr;
    }
    if (set.Indices) _aligned_free(set.Indices);
    set.Indices = nullptr;
    set.Bones = 0;
  }

  // The people: a PamClientCharacterEntity's body is the
  // ClientAntAnimatableEntity at + 0x248 (world log, 2026-10-23: Faith's
  // there is her body, handle 2; KrugerSec's in their own skeleton's manager,
  // handles 0x3000003 ...; some have the same one at + 0x340 too, the box
  // their bounding box is made of), its manager [[animatable + 0x158] +
  // 0x1B8]. Its pose slot is found like hers (SlotOf), and the recorded pose is
  // handed to it as an override, the way the game poses its Dash ghosts:
  // nothing is written into their own pose, so where the game has them (their
  // AI, their collision) stays as it is.
  const int CharacterBodyOffset = 0x248;
  // AntSettings (cached at [0x1423DD0F0], looked up there by the game when
  // it isn't): DisableModelAnimationCulling (+ 0x53), read at 0x1431F2486 and
  // 0x1433AF883 (with it set a culling step is skipped). 0x1431F2486 looks
  // like the setting up of a body, so whether it does anything to bodies that
  // are already there isn't known (2026-10-23); it's set while people play.
  const __int64 AntSettingsCacheRva = 0x23DD0F0;
  const int DisableModelAnimationCullingOffset = 0x53;
  const int CharacterOtherAnimatables[] = { 0x240, 0x248, 0x340 }; // Faith's: never one of the people
  const float PeopleRadius = 40.f;
  // While it plays, the people who aren't in the recording and are within
  // this of the replayed Faith are hidden (the live game goes on underneath:
  // KrugerSec chasing the real Faith came through doors the replay shows
  // shut, and out of the level's closed security doors; 2026-10-23), and so
  // is a recorded person before they were first seen and after they went
  const float OthersRadius = 60.f;
  const double OthersScanInterval = 1.0;
  const size_t MaxOthers = 128;       // people, guns and drones
  const double PersonWindowSlack = 0.05;
  const float ParkDrop = 10000.f;     // how far below the world a hidden body is put
  const double PeopleScanInterval = 1.0;
  // 16 were all taken 27 s into a 221 s fight (2026-10-23), four of them by
  // guards lying dead from before; a frame the same as the one before (a body
  // lying still) isn't stored again (RecordPeople)
  const size_t MaxPeople = 48;
  // KrugerSec (a character whose spawn template is under /Enemy/) have places
  // of their own, MaxEnemies of them, apart from the others' MaxPeople; and a
  // person seen without moving who went again takes none of the others'
  // (2026-11-04, the user: "replays spawned in ksec are invisible ... fix it
  // once and for all with the ksec encounters": in the 01:57 recording 41 of
  // the 48 went to the road traffic's walkers, each one recorded again every
  // time it came back into sight, the last place taken 27.5 s in, so the
  // KrugerSec that came 36 s into the fight weren't recorded at all). All of
  // them together, the brief ones too, at most MaxPeopleTracks.
  const size_t MaxEnemies = 64;
  const size_t MaxPeopleTracks = 512;
  // A live character's spawn template: the one of the spawners' characters
  // nearest it within TemplateReach (the hooks note them once a second, so a
  // walking one may be a few metres on)
  const float TemplateReach = 2.5f;

  bool IsEnemyTemplate(char const* path)
  {
    for (char const* p = path; *p; ++p)
      if (!_strnicmp(p, "/Enemy/", 7))
        return true;
    return false;
  }
  // Their frames packed (see PackedFrames), a 335 s fight with 48 people and
  // 39 guns and drones (2026-10-25 03:09, 512 MB used up 222 s into it as
  // floats) should fit a whole recording (600 s) in this. They're held from
  // then on too when Windows can commit less than MinFreeCommit more for the
  // game, or when no more room could be had (a failed allocation would end
  // the game).
  const size_t MaxPeopleBytes = (size_t)768 << 20;
  const unsigned long long MinFreeCommit = 2ull << 30;
  const double CommitCheckInterval = 1.0;
  // The people around a recorded vehicle that moves (a VTOL dropping
  // KrugerSec off may be further than PeopleRadius from her) are scanned for
  // too: within this of it, for the vehicles within VehiclePeopleReach of her
  const float VehiclePeopleRadius = 30.f;
  const float VehiclePeopleReach = 150.f;
  const int MaxVehiclePeopleScans = 4;

  // Whether the game updates a body at all. Its update job (0x1432DE310, on
  // animatable + 0x70) leaves the body's pose as it is (0x1432E86D0 says so,
  // and the body is moved to a list of ones not updated) unless + 0xFC, +
  // 0x5C0, or "always update" (+ 0x5C3, set by 0x1432E7EA0) with an update
  // mode ([+ 0x118] + 0x10) other than 1 or 3 is set, or AntSettings'
  // ForcePoseUpdate (+ 0x4A); mode 4 never. + 0x5C1 is cleared when the job
  // starts and set when it updated the body; then 0x1432EAD70 turns its pose
  // update on (pose object + 0x12C) when its distance's update interval has
  // gone by (0x1432E98D0). A KrugerSec who's taken down has "always update"
  // turned off by their AI (0x1439336D0 -> 0x143981A80, once), so their
  // ragdoll body is left as it lay and doesn't take an override (2026-10-23:
  // four with ours in their slot didn't move in 16 s of playback). Turning
  // their "always update" back on with the game's switch (0x1432E7EA0, builds
  // 4c20d7d74aa2 - 28e2865c93ba) made them take it, but the game crashed in
  // an animation job (0x142CF7ABB reading a freed list, 2026-10-23 18:28) as
  // it was done for the third time to six such bodies: a body the game has
  // stopped updating may have its animation partly torn down, so its update
  // isn't touched. Instead the replay writes such a body's slot buffers itself
  // (RawSlot, DrawBody; 2026-10-23: four KrugerSec taken down during a
  // recording lay where they fell all through its playback), and puts them
  // back as they were when the playback took it when it stops.
  const int BodyUpdateOffset = 0x70;
  const int BodyUpdateFlagOffset = 0xFC;
  const int BodyUpdateModeOffset = 0x118;          // -> the mode at + 0x10
  const int BodyExternalOffset = 0x5C0;
  const int BodyUpdatedOffset = 0x5C1;
  const int BodyAlwaysUpdateOffset = 0x5C3;
  const int PoseUpdateOnOffset = 0x12C;

  // A body's pose slot buffers, as the game's pose reader hands them out
  // (0x142FCEF00): local pose (page + 0x00) and world pose (+ 0x08), manager
  // + 0x8C bytes a slot each; 3x4 matrices (+ 0x10, manager + 0x90 a slot,
  // there when manager + 0x2B9 is set); + 0x18 and + 0x20 (manager + 0x98 a
  // slot each, the two animation steps). Read live (2026-10-23, KrugerSec
  // the game no longer updated, 163 bones): + 0x18 and + 0x20 are both the
  // local pose again, 0x30 a bone (scale x y z, rotation, translation x y z,
  // each with a fourth float), then 0x60 more; the matrices are rows (rotation,
  // then the translation in the fourth column) and are the world pose times a
  // fixed transform per bone, the same in three bodies of that skeleton (the
  // skinning's inverse bind pose).
  const int ManagerMatrixStrideOffset = 0x90;
  const int ManagerDrawnStrideOffset = 0x98;
  const int ManagerHasMatricesOffset = 0x2B9;
  const int PageMatricesOffset = 0x10;
  const int PageDrawnOffset = 0x18;
  const int PageStepOffset = 0x20;
  const int StepBoneBytes = 0x30;
  const int MatrixFloats = 12;
  const int RawRestoreFrames = 3;    // written that many frames (an update job of the frame it stopped may still run)
  enum { RawLocal, RawWorld, RawMatrices, RawDrawn, RawStep, RawBuffers };

  struct RawSlot
  {
    BYTE* Buffer[RawBuffers];
    int Bytes[RawBuffers];
    size_t Total() const
    {
      size_t total = 0;
      for (int k = 0; k < RawBuffers; ++k)
        total += Bytes[k];
      return total;
    }
  };

  bool FindRawSlot(PoseSlot const& slot, RawSlot& raw)
  {
    __try
    {
      __int64* pages = *(__int64**)(slot.Manager + ManagerPagesOffset);
      BYTE* page = (BYTE*)pages[slot.Handle >> 24];
      unsigned int index = slot.Handle & 0xFF;
      int strides[RawBuffers] = { *(int*)(slot.Manager + ManagerPoseStrideOffset), *(int*)(slot.Manager + ManagerPoseStrideOffset),
        *(BYTE*)(slot.Manager + ManagerHasMatricesOffset) ? *(int*)(slot.Manager + ManagerMatrixStrideOffset) : 0,
        *(int*)(slot.Manager + ManagerDrawnStrideOffset), *(int*)(slot.Manager + ManagerDrawnStrideOffset) };
      int offsets[RawBuffers] = { PageLocalPoseOffset, PageWorldPoseOffset, PageMatricesOffset, PageDrawnOffset, PageStepOffset };
      for (int k = 0; k < RawBuffers; ++k)
      {
        raw.Buffer[k] = nullptr;
        raw.Bytes[k] = 0;
      }
      for (int k = 0; k < RawBuffers; ++k)
      {
        if (strides[k] <= 0)
          continue;
        if (strides[k] > 0x10000)
          return false;
        BYTE* base = *(BYTE**)(page + offsets[k]);
        if ((__int64)base < 0x10000)
        {
          if (k == RawStep)
            continue;   // the second step's buffer isn't needed
          return false;
        }
        raw.Buffer[k] = base + (size_t)index * strides[k];
        raw.Bytes[k] = strides[k];
      }
      return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
  }

  bool ReadRaw(RawSlot const& raw, BYTE* out)
  {
    __try
    {
      for (int k = 0; k < RawBuffers; ++k)
        if (raw.Buffer[k])
        {
          memcpy(out, raw.Buffer[k], raw.Bytes[k]);
          out += raw.Bytes[k];
        }
      return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
  }

  bool WriteRaw(RawSlot const& raw, BYTE const* data)
  {
    __try
    {
      for (int k = 0; k < RawBuffers; ++k)
        if (raw.Buffer[k])
        {
          memcpy(raw.Buffer[k], data, raw.Bytes[k]);
          data += raw.Bytes[k];
        }
      return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
  }

  struct BodyGates
  {
    int Always, Flag, External, Mode, Updated, PoseUpdate;
  };

  bool ReadBodyGates(__int64 animatable, BodyGates& gates)
  {
    __try
    {
      BYTE const* body = (BYTE const*)(animatable + BodyUpdateOffset);
      gates.Always = body[BodyAlwaysUpdateOffset];
      gates.Flag = body[BodyUpdateFlagOffset];
      gates.External = body[BodyExternalOffset];
      gates.Updated = body[BodyUpdatedOffset];
      __int64 mode = *(__int64 const*)(body + BodyUpdateModeOffset);
      gates.Mode = mode >= 0x10000 ? *(int const*)(mode + 0x10) : -1;
      __int64 pose = *(__int64 const*)(animatable + AnimatablePoseOffset);
      gates.PoseUpdate = pose >= 0x10000 ? *(BYTE const*)(pose + PoseUpdateOnOffset) : -1;
      return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
  }

  // Without ForcePoseUpdate (mode 1's check of how big it is on screen left
  // out), and without + 0xFC: that's on for part of every frame only (read
  // live 2026-10-24: on and off about every 5 ms on every body, live ones
  // too), so reading it took the bodies the replay draws itself back and
  // forth every few frames; with the update held off for those
  // (util::hooks::HoldBodyUpdate) it doesn't let the game's update through.
  bool UpdatedByGame(BodyGates const& gates)
  {
    if (gates.Mode == 4)
      return false;
    return gates.External || (gates.Always && gates.Mode != 1 && gates.Mode != 3);
  }

  // A skeleton's hierarchy (the bone each bone hangs from), worked out while
  // recording from a body the game updates: there every bone's world pose is
  // its parent's world pose times its own local pose. It's for the bodies the
  // game doesn't update (taken down): when their local pose doesn't make
  // their world pose (their ragdoll moving only that), their local pose is
  // made from the world one (LocalFromWorld). One per pose manager (a
  // manager is one skeleton), learnt again each recording. A few bones have
  // no bone that works: Faith's bone 1 (where she is) has its world rotation
  // changed after (2026-10-23, live: the other 180 all had one); those keep
  // their local pose. KrugerSec's bone 1 likewise (its world rotation is its
  // local one turned 90 degrees about up), and bone 2's world pose is bone 1's
  // local one (2026-10-23, live): so a body the replay poses itself (see
  // DrawBody) gets bones 0 - 3 of its world pose as they were recorded, and
  // every other bone from its parent, in Order.
  const int RootWorldBones = 4;
  const int RootWorldFloats = RootWorldBones * 8;
  struct Hierarchy
  {
    __int64 Manager;
    int Bones;
    int Samples;
    std::vector<unsigned short> Hits;   // [bone * Bones + parent]: in how many samples it held
    std::vector<int> Parent;            // once Ready (-1: none, the local pose kept)
    bool Ready;
    bool Failed;
    std::vector<int> Order;             // bones from RootWorldBones on, each after its parent (empty: a loop)
    int Loose;                          // of those, the ones without a parent
    // Bones more than one bone held for as often (see RefineHierarchy): those
    // bones, and how often each failed since while another held
    std::vector<std::vector<int>> Tied;
    std::vector<std::vector<int>> TiedFails;
    int TiedLeft = 0;
  };
  std::vector<Hierarchy> g_Hierarchies;
  const int HierarchySamples = 30;
  const int HierarchyMinHits = 27;        // a few torn reads (the game writing it meanwhile) allowed
  const int HierarchyMaxWithout = 4;      // bones (past bone 0) that may have none
  const float HierarchyDistance = 0.002f;
  const float HierarchyDot = 0.9999f;
  const float StaleDistance = 0.01f;      // a local pose further than this off its world one isn't kept up

  Hierarchy* FindHierarchy(__int64 manager, int bones)
  {
    for (auto& hierarchy : g_Hierarchies)
      if (hierarchy.Manager == manager && hierarchy.Bones == bones)
        return &hierarchy;
    return nullptr;
  }

  Hierarchy& HierarchyOf(__int64 manager, int bones)
  {
    if (Hierarchy* found = FindHierarchy(manager, bones))
      return *found;
    g_Hierarchies.push_back(Hierarchy{ manager, bones, 0, {}, {}, false, false, {}, 0, {}, {}, 0 });
    return g_Hierarchies.back();
  }

  // How far a bone's world pose is from its parent's world pose times its own
  // local one: the distance, and |dot| of the two rotations
  void ComposeError(float const* parentWorld, float const* local, float const* world, float& distance, float& dot)
  {
    float t[3], r[4];
    QuatRotate(parentWorld + 4, local, t);
    float dx = parentWorld[0] + t[0] - world[0], dy = parentWorld[1] + t[1] - world[1], dz = parentWorld[2] + t[2] - world[2];
    distance = sqrtf(dx * dx + dy * dy + dz * dz);
    QuatMul(parentWorld + 4, local + 4, r);
    dot = fabsf(r[0] * world[4] + r[1] * world[5] + r[2] * world[6] + r[3] * world[7]);
  }

  // The order a world pose is made in from a local one (see DrawBody): bones
  // from RootWorldBones on, each after its parent; false on a loop
  bool MakeOrder(Hierarchy& hierarchy)
  {
    int bones = hierarchy.Bones;
    hierarchy.Order.clear();
    hierarchy.Loose = 0;
    if (bones <= RootWorldBones)
      return true;
    std::vector<char> done(bones, 0);
    for (int i = 0; i < RootWorldBones; ++i)
      done[i] = 1;
    for (bool progress = true; progress;)
    {
      progress = false;
      for (int i = RootWorldBones; i < bones; ++i)
        if (!done[i] && (hierarchy.Parent[i] < 0 || done[hierarchy.Parent[i]]))
        {
          hierarchy.Order.push_back(i);
          hierarchy.Loose += hierarchy.Parent[i] < 0;
          done[i] = 1;
          progress = true;
        }
    }
    if ((int)hierarchy.Order.size() == bones - RootWorldBones)
      return true;
    hierarchy.Order.clear();
    return false;
  }

  // One sample of a body the game updates: each bone's parent is the one
  // that held in nearly every sample (the lowest of those that held as often;
  // the others that held as often kept, see RefineHierarchy)
  void SampleHierarchy(Hierarchy& hierarchy, float const* local, float const* world)
  {
    if (hierarchy.Ready || hierarchy.Failed)
      return;
    int bones = hierarchy.Bones;
    if (hierarchy.Hits.empty())
      hierarchy.Hits.assign((size_t)bones * bones, 0);
    for (int i = 1; i < bones; ++i)
      for (int p = 0; p < bones; ++p)
      {
        if (p == i)
          continue;
        float distance, dot;
        ComposeError(world + p * FloatsPerBone, local + i * FloatsPerBone, world + i * FloatsPerBone, distance, dot);
        if (distance < HierarchyDistance && dot > HierarchyDot)
          ++hierarchy.Hits[(size_t)i * bones + p];
      }
    if (++hierarchy.Samples < HierarchySamples)
      return;
    hierarchy.Parent.assign(bones, -1);
    int without = 0;
    char list[64] = "";
    size_t at = 0;
    for (int i = 1; i < bones; ++i)
    {
      int best = -1, bestHits = 0;
      for (int p = 0; p < bones; ++p)
        if (p != i && hierarchy.Hits[(size_t)i * bones + p] > bestHits)
        {
          best = p;
          bestHits = hierarchy.Hits[(size_t)i * bones + p];
        }
      if (bestHits >= HierarchyMinHits)
      {
        hierarchy.Parent[i] = best;
        std::vector<int> tied;
        for (int p = 0; p < bones; ++p)
          if (p != i && hierarchy.Hits[(size_t)i * bones + p] == bestHits)
            tied.push_back(p);
        if (tied.size() > 1)
        {
          if (hierarchy.Tied.empty())
          {
            hierarchy.Tied.resize(bones);
            hierarchy.TiedFails.resize(bones);
          }
          hierarchy.TiedFails[i].assign(tied.size(), 0);
          hierarchy.Tied[i].swap(tied);
          ++hierarchy.TiedLeft;
        }
      }
      else
      {
        ++without;
        if (at + 12 < sizeof(list))
          at += sprintf_s(list + at, sizeof(list) - at, " %d (%d)", i, bestHits);
      }
    }
    hierarchy.Hits.clear();
    hierarchy.Hits.shrink_to_fit();
    hierarchy.Ready = without <= HierarchyMaxWithout;
    hierarchy.Failed = !hierarchy.Ready;
    // The order a world pose is made in from a local one (see DrawBody)
    hierarchy.Order.clear();
    hierarchy.Loose = 0;
    if (hierarchy.Ready && !MakeOrder(hierarchy))
      util::log::Warning("Replay: the hierarchy of the skeleton of %d bones (pose manager 0x%I64X) has a loop: its bodies the "
        "game doesn't update aren't posed by the replay", bones, hierarchy.Manager);
    if (hierarchy.Ready)
    {
      char tiedList[200] = "";
      size_t tiedAt = 0;
      for (int i = 0; i < (int)hierarchy.Tied.size(); ++i)
        if (hierarchy.Tied[i].size() > 1 && tiedAt + 24 < sizeof(tiedList))
        {
          tiedAt += sprintf_s(tiedList + tiedAt, sizeof(tiedList) - tiedAt, " %d (", i);
          for (size_t k = 0; k < hierarchy.Tied[i].size() && tiedAt + 8 < sizeof(tiedList); ++k)
            tiedAt += sprintf_s(tiedList + tiedAt, sizeof(tiedList) - tiedAt, "%s%d", k ? " " : "", hierarchy.Tied[i][k]);
          if (tiedAt + 2 < sizeof(tiedList))
            tiedAt += sprintf_s(tiedList + tiedAt, sizeof(tiedList) - tiedAt, ")");
        }
      util::log::Write("Replay: the hierarchy of a skeleton of %d bones (pose manager 0x%I64X) worked out from %d samples; "
        "%d without a parent that held in %d (their own pose kept):%s; %d with more than one bone that held as often (the lowest "
        "taken until the frames after tell them apart):%s", bones, hierarchy.Manager, HierarchySamples, without, HierarchyMinHits,
        list, hierarchy.TiedLeft, tiedList);
    }
    else
      util::log::Warning("Replay: the hierarchy of a skeleton of %d bones (pose manager 0x%I64X) couldn't be worked out "
        "(%d bones without a parent that held in %d of %d samples, the first, with their best:%s): its bodies the game "
        "doesn't update are recorded from their local pose", bones, hierarchy.Manager, without, HierarchyMinHits,
        HierarchySamples, list);
  }

  // A tie of SampleHierarchy settled (2026-11-01, the user: "Icarus ... his
  // mouth bones seem wrong"): a bone two bones held for in every sample
  // (where they were the same place and turn then: a jaw shut at its head's
  // pivot) hung from the lower one, so it didn't follow the other once that
  // moved. Each later frame of a body the game updates, a bone still tied
  // tries each of its bones; one that fails TiedFailsToDrop times while
  // another holds is dropped, and the last one left is its parent.
  const int TiedFailsToDrop = 3;

  void RefineHierarchy(Hierarchy& hierarchy, float const* local, float const* world)
  {
    if (!hierarchy.Ready || hierarchy.TiedLeft <= 0)
      return;
    bool changed = false;
    for (int i = 0; i < (int)hierarchy.Tied.size(); ++i)
    {
      std::vector<int>& tied = hierarchy.Tied[i];
      if (tied.size() < 2)
        continue;
      bool held[64] = {};
      int holding = 0;
      size_t count = (std::min)(tied.size(), sizeof(held));
      for (size_t k = 0; k < count; ++k)
      {
        float distance, dot;
        ComposeError(world + tied[k] * FloatsPerBone, local + i * FloatsPerBone, world + i * FloatsPerBone, distance, dot);
        held[k] = distance < HierarchyDistance && dot > HierarchyDot;
        holding += held[k];
      }
      if (!holding || holding == (int)count)
        continue;   // a torn read, or nothing told apart
      std::vector<int>& fails = hierarchy.TiedFails[i];
      for (size_t k = count; k-- > 0;)
        if (!held[k] && ++fails[k] >= TiedFailsToDrop)
        {
          tied.erase(tied.begin() + k);
          fails.erase(fails.begin() + k);
        }
      if (tied.size() == 1)
      {
        int before = hierarchy.Parent[i];
        hierarchy.Parent[i] = tied[0];
        --hierarchy.TiedLeft;
        changed = changed || before != tied[0];
        static int s_logs = 0;
        if (s_logs++ < 40)
          util::log::Write("Replay: the skeleton of %d bones (pose manager 0x%I64X): bone %d hangs from bone %d (it held in every "
            "frame; %s), %d ties left", hierarchy.Bones, hierarchy.Manager, i, tied[0], before == tied[0] ?
            "as taken" : "taken from the lower bone it was tied with", hierarchy.TiedLeft);
      }
    }
    if (changed && !MakeOrder(hierarchy))
      util::log::Warning("Replay: the hierarchy of the skeleton of %d bones (pose manager 0x%I64X) has a loop once a tie was "
        "settled: its bodies the game doesn't update aren't posed by the replay", hierarchy.Bones, hierarchy.Manager);
  }

  // The local pose that makes this world pose, each bone under its parent's
  // world pose (bone 0, and those without a parent, left as they are)
  void LocalFromWorld(Hierarchy const& hierarchy, float const* world, float* local)
  {
    for (int i = 1; i < hierarchy.Bones; ++i)
    {
      if (hierarchy.Parent[i] < 0)
        continue;
      float const* p = world + hierarchy.Parent[i] * FloatsPerBone;
      float const* w = world + i * FloatsPerBone;
      float* out = local + i * FloatsPerBone;
      float inverse[4] = { -p[4], -p[5], -p[6], p[7] };
      float d[3] = { w[0] - p[0], w[1] - p[1], w[2] - p[2] };
      QuatRotate(inverse, d, out);
      out[3] = 1.f;
      QuatMul(inverse, w + 4, out + 4);
    }
  }

  // The furthest any bone of a local pose is from where it is in this world
  // pose (under its parent's world pose; a bone turned another way counts 1 m)
  float LocalWorldError(Hierarchy const& hierarchy, float const* local, float const* world)
  {
    float worst = 0.f;
    for (int i = 1; i < hierarchy.Bones; ++i)
    {
      if (hierarchy.Parent[i] < 0)
        continue;
      float distance, dot;
      ComposeError(world + hierarchy.Parent[i] * FloatsPerBone, local + i * FloatsPerBone, world + i * FloatsPerBone, distance, dot);
      if (dot < HierarchyDot)
        distance += 1.f;
      if (distance > worst)
        worst = distance;
    }
    return worst;
  }

  // A rotation quaternion as a matrix, rows (what QuatRotate does)
  void QuatMatrix(float const* q, float m[3][3])
  {
    float x = q[0], y = q[1], z = q[2], w = q[3];
    m[0][0] = 1.f - 2.f * (y * y + z * z); m[0][1] = 2.f * (x * y - z * w); m[0][2] = 2.f * (x * z + y * w);
    m[1][0] = 2.f * (x * y + z * w); m[1][1] = 1.f - 2.f * (x * x + z * z); m[1][2] = 2.f * (y * z - x * w);
    m[2][0] = 2.f * (x * z - y * w); m[2][1] = 2.f * (y * z + x * w); m[2][2] = 1.f - 2.f * (x * x + y * y);
  }

  // A body's fixed transform per bone between its world pose and its matrices
  // (matrix = world * fixed; see PageStepOffset), from its slot buffers as they
  // are: 12 floats a bone (rows: rotation, translation last). False unless
  // every one is a rotation (and every world rotation a unit quaternion).
  bool ReadUnbind(RawSlot const& raw, int bones, float* unbind)
  {
    if (!raw.Buffer[RawWorld] || !raw.Buffer[RawMatrices] || raw.Bytes[RawWorld] < bones * FloatsPerBone * 4 ||
        raw.Bytes[RawMatrices] < bones * MatrixFloats * 4)
      return false;
    __try
    {
      float const* world = (float const*)raw.Buffer[RawWorld];
      float const* matrices = (float const*)raw.Buffer[RawMatrices];
      for (int i = 0; i < bones; ++i)
      {
        float const* w = world + i * FloatsPerBone;
        float const* c = matrices + i * MatrixFloats;
        float length = w[4] * w[4] + w[5] * w[5] + w[6] * w[6] + w[7] * w[7];
        if (!(length > 0.98f && length < 1.02f))
          return false;
        float r[3][3];
        QuatMatrix(w + 4, r);
        float* k = unbind + i * MatrixFloats;
        for (int row = 0; row < 3; ++row)
        {
          // (R^T C) and R^T (c - t)
          for (int col = 0; col < 3; ++col)
            k[row * 4 + col] = r[0][row] * c[0 * 4 + col] + r[1][row] * c[1 * 4 + col] + r[2][row] * c[2 * 4 + col];
          k[row * 4 + 3] = r[0][row] * (c[3] - w[0]) + r[1][row] * (c[7] - w[1]) + r[2][row] * (c[11] - w[2]);
        }
        for (int a = 0; a < 3; ++a)
          for (int b = 0; b < 3; ++b)
          {
            float dot = k[a * 4] * k[b * 4] + k[a * 4 + 1] * k[b * 4 + 1] + k[a * 4 + 2] * k[b * 4 + 2];
            if (!(fabsf(dot - (a == b ? 1.f : 0.f)) < 0.01f))
              return false;
          }
      }
      return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
  }

  // A pose the replay draws itself, into a body the game doesn't update: its
  // local pose (and both animation steps' copies of it), its world pose (bones
  // 0 - 3 given in world; the others from their parent in order, those without
  // one left where they are) and its matrices (world * unbind). world holds
  // bones 0 - 3 on the way in.
  bool WriteBody(RawSlot const& raw, int bones, float const* local, float* world, int const* order, int orderCount,
    int const* parent, float const* unbind)
  {
    int poseBytes = bones * FloatsPerBone * 4;
    if (!raw.Buffer[RawLocal] || !raw.Buffer[RawWorld] || !raw.Buffer[RawMatrices] || raw.Bytes[RawLocal] < poseBytes ||
        raw.Bytes[RawWorld] < poseBytes || raw.Bytes[RawMatrices] < bones * MatrixFloats * 4)
      return false;
    __try
    {
      float const* own = (float const*)raw.Buffer[RawWorld];
      for (int n = 0; n < orderCount; ++n)
      {
        int i = order[n];
        float* w = world + i * FloatsPerBone;
        int p = parent[i];
        if (p < 0)
        {
          memcpy(w, own + i * FloatsPerBone, FloatsPerBone * sizeof(float));
          continue;
        }
        float const* pw = world + p * FloatsPerBone;
        float const* l = local + i * FloatsPerBone;
        QuatRotate(pw + 4, l, w);
        w[0] += pw[0]; w[1] += pw[1]; w[2] += pw[2];
        w[3] = 1.f;
        QuatMul(pw + 4, l + 4, w + 4);
      }
      memcpy(raw.Buffer[RawLocal], local, poseBytes);
      int const steps[2] = { RawDrawn, RawStep };
      for (int k : steps)
        if (raw.Buffer[k] && raw.Bytes[k] >= bones * StepBoneBytes)
          for (int i = 0; i < bones; ++i)
          {
            float* step = (float*)(raw.Buffer[k] + i * StepBoneBytes);
            memcpy(step + 4, local + i * FloatsPerBone + 4, 4 * sizeof(float));   // rotation
            memcpy(step + 8, local + i * FloatsPerBone, 3 * sizeof(float));       // translation (scale kept)
          }
      memcpy(raw.Buffer[RawWorld], world, poseBytes);
      float* matrices = (float*)raw.Buffer[RawMatrices];
      for (int i = 0; i < bones; ++i)
      {
        float const* w = world + i * FloatsPerBone;
        float const* k = unbind + i * MatrixFloats;
        float r[3][3];
        QuatMatrix(w + 4, r);
        float* c = matrices + i * MatrixFloats;
        for (int row = 0; row < 3; ++row)
        {
          for (int col = 0; col < 3; ++col)
            c[row * 4 + col] = r[row][0] * k[0 * 4 + col] + r[row][1] * k[1 * 4 + col] + r[row][2] * k[2 * 4 + col];
          c[row * 4 + 3] = r[row][0] * k[3] + r[row][1] * k[7] + r[row][2] * k[11] + w[row];
        }
      }
      return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
  }

  // A body the replay hides itself (see ParkDrop): its slot buffers as they
  // were (before), the whole body moved ParkDrop down: bone 1 of the local
  // poses, every bone but bone 0 (the origin) of the world pose and matrices
  void ParkRaw(RawSlot const& raw, int bones, unsigned char* data)
  {
    unsigned char* at = data;
    for (int k = 0; k < RawBuffers; ++k)
    {
      if (!raw.Buffer[k])
        continue;
      float* f = (float*)at;
      int count = raw.Bytes[k] / 4;
      if (k == RawLocal && count > 1 * FloatsPerBone + 1)
        f[1 * FloatsPerBone + 1] -= ParkDrop;
      else if (k == RawWorld)
        for (int i = 1; i < bones && i * FloatsPerBone + 1 < count; ++i)
          f[i * FloatsPerBone + 1] -= ParkDrop;
      else if (k == RawMatrices)
        for (int i = 1; i < bones && i * MatrixFloats + 7 < count; ++i)
          f[i * MatrixFloats + 7] -= ParkDrop;
      else if ((k == RawDrawn || k == RawStep) && count > 1 * 12 + 9)
        f[1 * 12 + 9] -= ParkDrop;
      at += raw.Bytes[k];
    }
  }

  // A body's own pose for the replay: its local pose, or, for a body the
  // game doesn't update whose local pose doesn't make its world pose, the
  // local pose made from the world one. learn: a body the game updates is a
  // sample of its skeleton's hierarchy (only while recording: while a
  // playback poses bodies their local poses aren't what's shown). shown:
  // made from the world pose whenever the hierarchy is known, whether the
  // game updates the body or not (what it shows, for putting it back).
  struct OwnPoseInfo
  {
    bool Gated;           // Gates read
    BodyGates Gates;
    bool FromWorld;
    float Error;          // how far its local pose was off its world one, -1 when not looked at
  };

  bool OwnPose(PoseSlot const& slot, __int64 animatable, float* out, bool learn, bool shown, OwnPoseInfo& info)
  {
    info = {};
    info.Error = -1.f;
    if (!CopyPose(slot.Local, out, slot.Bones))
      return false;
    info.Gated = ReadBodyGates(animatable, info.Gates);
    if (!info.Gated || !slot.World)
      return true;
    static std::vector<float> world;
    world.resize((size_t)slot.Bones * FloatsPerBone);
    if (!CopyPose(slot.World, world.data(), slot.Bones))
      return true;
    if (UpdatedByGame(info.Gates))
    {
      if (learn && info.Gates.Updated == 1 && info.Gates.PoseUpdate == 1)
      {
        Hierarchy& hierarchy = HierarchyOf(slot.Manager, slot.Bones);
        SampleHierarchy(hierarchy, out, world.data());
        RefineHierarchy(hierarchy, out, world.data());
      }
      if (!shown)
        return true;
    }
    Hierarchy* hierarchy = FindHierarchy(slot.Manager, slot.Bones);
    if (!hierarchy || !hierarchy->Ready)
      return true;
    info.Error = LocalWorldError(*hierarchy, out, world.data());
    if (shown || info.Error > StaleDistance)
    {
      LocalFromWorld(*hierarchy, world.data(), out);
      info.FromWorld = true;
    }
    return true;
  }

  // Diagnostic (temporary): a sum of a pose's numbers, to see whether it changed
  bool PoseSum(float const* pose, int bones, double& sum)
  {
    __try
    {
      sum = 0;
      for (int i = 0; i < bones * FloatsPerBone; ++i)
        sum += pose[i];
      return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
  }

  // Keeping the recorded people in the level (see the hooks' KeepGuard): a
  // character the game wants out is kept when it's this near one of their
  // places: each one's place now, and where they were every quarter second of
  // the recording (whether a taken-down KrugerSec's server character follows
  // their ragdoll is set per character, EACharacterPhysicsComponentData's
  // UseServerRagdoll, so it may have stayed where they went down; a quarter
  // second is about a metre of running). While a playback plays their bodies
  // (and so their places) are the replay's: the places from just before it
  // are kept, with more room (their AI may walk).
  const float GuardKeepRadius = 3.f;
  const float GuardKeepRadiusPlaying = 30.f;
  const double GuardPlaceInterval = 0.25;

  // An entity's place: its components (+ 0x38), the translation at + 0x40 +
  // (byte 9 + 2 * byte 0xA) * 0x20 (as the game's own despawn check reads it)
  bool EntityPlace(__int64 entity, float* place)
  {
    __try
    {
      BYTE const* components = *(BYTE const**)(entity + 0x38);
      if ((__int64)components < 0x10000)
        return false;
      memcpy(place, components + 0x40 + ((__int64)components[9] + 2 * components[0xA]) * 0x20, 3 * sizeof(float));
      return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
  }

  bool ReadWorldRoot(PoseSlot const& slot, float* root)
  {
    __try
    {
      if (!slot.World)
        return false;
      memcpy(root, slot.World + 1 * FloatsPerBone, 3 * sizeof(float));
      return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
  }

  bool FindCharacterPose(__int64 character, __int64& animatable, PoseSlot& slot, int bodyOffset = CharacterBodyOffset)
  {
    __int64 base = (__int64)g_gameHandle;
    __try
    {
      animatable = *(__int64*)(character + bodyOffset);
      if (animatable < 0x10000 || TypeOf(animatable) != base + AnimatableTypeInfoRva)
        return false;
      __int64 pose = *(__int64*)(animatable + AnimatablePoseOffset);
      if (pose < 0x10000)
        return false;
      __int64 manager = *(__int64*)(pose + AnimatableManagerOffset);
      if (manager < 0x10000)
        return false;
      return SlotOf(manager, animatable, slot);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
  }

  // A character's bounding box, like Faith's (see SoldierBoxAnimatableOffset):
  // PamClientCharacterEntity slot 65 (0x143916B40) puts the animatable it
  // registers its BoundingBoxBones for at + 0x340. For some KrugerSec that's
  // their body (+ 0x248), which the replay's override moves; for others it's
  // another, so their box stayed where the real one is, and the replayed body
  // was only drawn while that was in view (2026-10-23: replayed KrugerSec
  // shown only while the real Faith, whom the live ones were with, was close,
  // as Faith's own body was before her box got the replay too).
  enum class BoxKind
  {
    Body,     // its body: the override moves it
    Other,    // another slot of the same manager, as many bones: gets the replay too
    None,     // none (no box bones registered)
    Unusable  // another animatable, not in a slot like that
  };

  BoxKind FindPersonBox(__int64 character, __int64 bodyAnimatable, PoseSlot const& body, PoseSlot& box)
  {
    __int64 animatable = 0;
    __try
    {
      animatable = *(__int64*)(character + SoldierBoxAnimatableOffset);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return BoxKind::Unusable;
    }
    if (!animatable)
      return BoxKind::None;
    if (animatable == bodyAnimatable)
      return BoxKind::Body;
    return FindBoxPose(character, body, box) ? BoxKind::Other : BoxKind::Unusable;
  }

  // Diagnostic (temporary): what became of a person's override last frame.
  // The page (manager + 0x210, [pages + (handle >> 24) * 8]) holds per slot
  // the local pose (+ 0x00, stride manager + 0x8C), the same in world space
  // (+ 0x08, the same stride) and 3x4 world matrices (+ 0x10, stride manager +
  // 0x90): bone 1 of the world pose is where the body is.
  bool ReadPersonWorld(PoseSlot const& slot, float* worldRoot, float* matrixRoot, float* position, __int64 entity)
  {
    __try
    {
      __int64* pages = *(__int64**)(slot.Manager + ManagerPagesOffset);
      __int64 page = pages[slot.Handle >> 24];
      int stride = *(int*)(slot.Manager + ManagerPoseStrideOffset);
      int matrixStride = *(int*)(slot.Manager + 0x90);
      unsigned int index = slot.Handle & 0xFF;
      float const* world = (float const*)(*(__int64*)(page + 0x08) + (__int64)index * stride);
      float const* matrices = (float const*)(*(__int64*)(page + 0x10) + (__int64)index * matrixStride);
      memcpy(worldRoot, world + 1 * FloatsPerBone, 3 * sizeof(float));
      memcpy(matrixRoot, matrices + 12, 12 * sizeof(float));   // bone 1's 3x4
      __int64 components = *(__int64*)(entity + 0x38);
      BYTE const* c = (BYTE const*)components;
      memcpy(position, (void const*)(components + ((__int64)c[9] + 2 * c[0xA]) * 0x20 + 0x10 + 0x30), 3 * sizeof(float));
      return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
  }

  // 12 floats as text, for the diagnostics
  void Format12(char* out, size_t size, float const* m)
  {
    sprintf_s(out, size, "%.2f %.2f %.2f %.2f / %.2f %.2f %.2f %.2f / %.2f %.2f %.2f %.2f", m[0], m[1], m[2], m[3], m[4], m[5],
      m[6], m[7], m[8], m[9], m[10], m[11]);
  }

  __int64 ReadOverrideEntry(PoseSlot const& slot)
  {
    __try { return slot.Overrides[slot.Handle & 0xFF]; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return -1; }
  }

  // A pose with its root (bone 1, where the body is in the world) far below
  // the world: the body is posed where nothing can see it, the way the game
  // parks its Dash ghosts out of the way
  void ParkPose(float const* source, float* out, int bones)
  {
    memcpy(out, source, (size_t)bones * FloatsPerBone * sizeof(float));
    out[1 * FloatsPerBone + 1] -= ParkDrop;
  }

  // Hides a body the game updates without moving its own bones 0 - 3, which
  // its client entity follows (2026-10-24: parked with bone 1 10 km down,
  // four live KrugerSec's entities went down with their bodies, the game
  // stopped updating them down there, and they stayed under the map, not
  // drawn when their turn in the replay came): those as the game has them
  // this frame (its local pose), and every other bone hanging from one of
  // them (the learnt hierarchy; one without a parent too) 10 km down in its
  // parent's frame (bones 0 - 3 only turn about up). False without the
  // hierarchy (ParkPose then).
  bool ParkPoseKeepingRoot(float const* source, PoseSlot const& slot, float* out, int bones)
  {
    Hierarchy* hierarchy = FindHierarchy(slot.Manager, slot.Bones);
    if (!hierarchy || !hierarchy->Ready || (int)hierarchy->Parent.size() != bones || bones <= RootWorldBones)
      return false;
    memcpy(out, source, (size_t)bones * FloatsPerBone * sizeof(float));
    if (!CopyPose(slot.Local, out, RootWorldBones))
      return false;
    int dropped = 0;
    for (int i = RootWorldBones; i < bones; ++i)
      if (hierarchy->Parent[i] < RootWorldBones)
      {
        out[i * FloatsPerBone + 1] -= ParkDrop;
        ++dropped;
      }
    return dropped > 0;
  }

  float Distance3(float const* a, float const* b)
  {
    float dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
    return sqrtf(dx * dx + dy * dy + dz * dz);
  }

  // Diagnostic (temporary, read only): a slot's render-side entry, [page +
  // 0x40] + slot * 0x20 (on at + 0x14, there when manager + 0x2B8 is set),
  // whose + 0x00 and + 0x08 point to 0x30 bytes a bone. The pose job
  // (0x142FCC7C0) checks it, and the placement setter (0x142FD8F20, through
  // 0x142FD8840) writes the root's 3x4 into both; neither runs for a body
  // whose update is held. What it holds against the world pose (page + 0x08)
  // and the matrices (page + 0x10) isn't known (2026-10-24, offline).
  const int ManagerHasRenderEntriesOffset = 0x2B8;
  const int PageRenderEntriesOffset = 0x40;
  const int RenderEntryStride = 0x20;
  const int RenderEntryOnOffset = 0x14;
  const int RenderBoneBytes = 0x30;
  const float RenderSameDistance = 0.01f;
  struct RenderEntry
  {
    int Has, HasMatrices, On;
    bool Found;                    // the page has entries
    __int64 Buffer[2];             // + 0x00, + 0x08
    bool IsMatrices[2];            // that pointer is the slot's own matrices (page + 0x10)
    unsigned int Words[4];         // + 0x10 .. + 0x1F as they are
    float Root[2][12];             // bone 1 in each buffer
    float WorldRoot[8];            // bone 1 of the world pose
    float MatrixRoot[12];          // bone 1 of the matrices
    int Bones;                     // compared in + 0x08 (0: none)
    int SameAsMatrices, SameAsOther;
    int AtWorld[2];                // bones whose + 0x08 translation (floats 3 7 11 / 9 10 11) is the world pose's
    float Farthest[2];             // the most a bone's is off it
  };

  bool ReadRenderEntry(PoseSlot const& slot, RenderEntry& out)
  {
    memset(&out, 0, sizeof(out));
    __try
    {
      out.Has = *(BYTE const*)(slot.Manager + ManagerHasRenderEntriesOffset);
      out.HasMatrices = *(BYTE const*)(slot.Manager + ManagerHasMatricesOffset);
      __int64* pages = *(__int64**)(slot.Manager + ManagerPagesOffset);
      __int64 page = pages[slot.Handle >> 24];
      unsigned int index = slot.Handle & 0xFF;
      int stride = *(int*)(slot.Manager + ManagerPoseStrideOffset);
      int matrixStride = *(int*)(slot.Manager + ManagerMatrixStrideOffset);
      __int64 worldBase = *(__int64*)(page + PageWorldPoseOffset);
      __int64 matrixBase = *(__int64*)(page + PageMatricesOffset);
      float const* world = worldBase >= 0x10000 ? (float const*)(worldBase + (__int64)index * stride) : nullptr;
      float const* matrices = out.HasMatrices && matrixBase >= 0x10000 ?
        (float const*)(matrixBase + (__int64)index * matrixStride) : nullptr;
      if (world)
        memcpy(out.WorldRoot, world + 1 * FloatsPerBone, sizeof(out.WorldRoot));
      if (matrices)
        memcpy(out.MatrixRoot, matrices + 1 * MatrixFloats, sizeof(out.MatrixRoot));
      __int64 entries = *(__int64*)(page + PageRenderEntriesOffset);
      if (entries < 0x10000)
        return true;
      out.Found = true;
      BYTE const* entry = (BYTE const*)(entries + (__int64)index * RenderEntryStride);
      out.Buffer[0] = *(__int64 const*)entry;
      out.Buffer[1] = *(__int64 const*)(entry + 8);
      memcpy(out.Words, entry + 0x10, sizeof(out.Words));
      out.On = entry[RenderEntryOnOffset];
      for (int k = 0; k < 2; ++k)
      {
        out.IsMatrices[k] = matrices && out.Buffer[k] == (__int64)matrices;
        if (out.Buffer[k] >= 0x10000)
          memcpy(out.Root[k], (BYTE const*)out.Buffer[k] + 1 * RenderBoneBytes, sizeof(out.Root[k]));
      }
      if (out.Buffer[1] < 0x10000 || !world)
        return true;
      out.Bones = slot.Bones;
      for (int i = 0; i < slot.Bones; ++i)
      {
        float const* m = (float const*)((BYTE const*)out.Buffer[1] + (size_t)i * RenderBoneBytes);
        if (matrices && !memcmp(m, matrices + i * MatrixFloats, RenderBoneBytes))
          ++out.SameAsMatrices;
        if (out.Buffer[0] >= 0x10000 && !memcmp(m, (BYTE const*)out.Buffer[0] + (size_t)i * RenderBoneBytes, RenderBoneBytes))
          ++out.SameAsOther;
        float const* w = world + i * FloatsPerBone;
        // 0x30 a bone: scale, rotation, translation (2026-10-24 log), so the
        // translation is floats 8 9 10; 3 7 11 is kept for comparison
        float const columns[3] = { m[3], m[7], m[11] }, last[3] = { m[8], m[9], m[10] };
        float off[2] = { Distance3(columns, w), Distance3(last, w) };
        for (int k = 0; k < 2; ++k)
        {
          out.AtWorld[k] += off[k] <= RenderSameDistance;
          out.Farthest[k] = off[k] > out.Farthest[k] ? off[k] : out.Farthest[k];
        }
      }
      return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
  }

  void LogRenderEntry(char const* label, __int64 entity, char const* whose, __int64 animatable, PoseSlot const& slot)
  {
    RenderEntry render;
    BodyGates gates{ -1, -1, -1, -1, -1, -1 };
    ReadBodyGates(animatable, gates);
    if (!ReadRenderEntry(slot, render))
    {
      util::log::Write("Replay:   render %s 0x%I64X (%s): couldn't be read", label, entity, whose);
      return;
    }
    util::log::Write("Replay:   render %s 0x%I64X (%s; updated last frame %d, always update %d, mode %d): manager + 0x2B8 %d, "
      "+ 0x2B9 %d; entry %s, on %d, + 0x00 0x%I64X%s, + 0x08 0x%I64X%s, + 0x10 %08X %08X %08X %08X; + 0x08 against the "
      "matrices: %d of %d bones the same, against + 0x00: %d; its translations at the world pose's: columns 3 7 11 %d "
      "(farthest %.2f m), floats 8 9 10 %d (farthest %.2f m)", label, entity, whose, gates.Updated, gates.Always, gates.Mode,
      render.Has, render.HasMatrices, render.Found ? "there" : "none", render.On, render.Buffer[0],
      render.IsMatrices[0] ? " (= the matrices)" : "", render.Buffer[1], render.IsMatrices[1] ? " (= the matrices)" : "",
      render.Words[0], render.Words[1], render.Words[2], render.Words[3], render.SameAsMatrices, render.Bones,
      render.SameAsOther, render.AtWorld[0], render.Farthest[0], render.AtWorld[1], render.Farthest[1]);
    char inEntry[2][200], inMatrices[200];
    Format12(inEntry[0], sizeof(inEntry[0]), render.Root[0]);
    Format12(inEntry[1], sizeof(inEntry[1]), render.Root[1]);
    Format12(inMatrices, sizeof(inMatrices), render.MatrixRoot);
    util::log::Write("Replay:     bone 1: world pose %.2f %.2f %.2f (rotation %.3f %.3f %.3f %.3f); + 0x08 %s; + 0x00 %s; "
      "matrices %s", render.WorldRoot[0], render.WorldRoot[1], render.WorldRoot[2], render.WorldRoot[4], render.WorldRoot[5],
      render.WorldRoot[6], render.WorldRoot[7], inEntry[1], inEntry[0], inMatrices);
  }

  // Still that character (or gun, or drone), with that body
  bool StillCharacter(__int64 character, __int64 vtable, __int64 animatable, int bodyOffset = CharacterBodyOffset)
  {
    __try
    {
      return *(__int64*)character == vtable && *(__int64*)(character + bodyOffset) == animatable;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
  }

  bool IsFaithAnimatable(__int64 soldier, __int64 animatable)
  {
    __try
    {
      for (int offset : CharacterOtherAnimatables)
        if (*(__int64*)(soldier + offset) == animatable)
          return true;
      return false;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return true;
    }
  }

  // Guns and drones. A KrugerSec's gun (PamClientCharacterWeaponEntity, an
  // actor) has a body of its own: a ClientAntAnimatableEntity at + 0x100, 26
  // bones in a skeleton of its own, bone 1 of its local pose where the gun is
  // in the world, updated by the game every frame whether it's held or
  // dropped; + 0x120 is the character it belongs to (2026-10-23, live: three
  // guns, two of them dropped). So a gun is recorded and played like a person.
  // A drone (PamClientDroneActorEntity) is an actor too; whether it has a
  // body at + 0x100 isn't known (none was in the level to read long enough),
  // so each one is checked. The attach scan leaves held weapons out, so both
  // are found through their classes' instance lists (TypeInfo + 0x60 / 0x70 /
  // 0x80 / 0x90; the link at entity + 0x40, as in AttachTargets).
  const int ActorBodyOffset = 0x100;
  struct ActorClass
  {
    __int64 TypeInfoRva;
    char const* Name;
  };
  const ActorClass ActorClasses[] = { { 0x284FB60, "PamClientCharacterWeaponEntity" }, { 0x2855C20, "PamClientDroneActorEntity" } };
  // A drone's server entity (PamServerDroneActorEntity) and how near its
  // client one it has to be to be taken for it
  const __int64 DroneServerTypeInfoRva = 0x28575E0;
  const float DroneServerDistance = 15.f;
  const int TypeInstanceListsOffset = 0x60;
  const int EntityLinkOffset = 0x40;
  const int MaxActorsPerList = 4096;
  const size_t MaxActors = 64;
  // Drones are looked for this far from her (guns within PeopleRadius): a
  // drone's effects (its lasers, searchlight and downwash) are recorded
  // within 80 m of her (ReplayEffectRadius), and a hunter drone keeps its
  // distance in a fight. 2026-10-25 03:46: one 40 - 165 m off had its lasers
  // recorded the whole 364 s but its body only the last 2 s (found once
  // within 40 m), so it played back as lasers with no drone. A little farther
  // than its effects, so its body is found (one scan a second) before they
  // come in.
  const int DroneActorClass = 1;          // in ActorClasses
  const float DroneFindRadius = 100.f;

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

  struct ActorFound
  {
    __int64 Entity, VTable, Animatable;
    int Class;
  };

  // Those within radius of a point: with a body, into out; without one, counted
  void WalkActorList(__int64 head, __int64 typeInfo, int classIndex, float const* center, float radius, ActorFound* out,
    int max, int& count, int& withoutBody, __int64& firstWithoutBody)
  {
    __int64 base = (__int64)g_gameHandle;
    __try
    {
      __int64 link = *(__int64*)head;
      for (int n = 0; link && n < MaxActorsPerList && count < max; ++n, link = *(__int64*)link)
      {
        __int64 entity = link - EntityLinkOffset;
        if (TypeOf(entity) != typeInfo)
          continue;
        float place[3];
        if (!EntityPlace(entity, place) || !(Distance3(place, center) <= radius))
          continue;
        __int64 animatable = *(__int64*)(entity + ActorBodyOffset);
        if (animatable < 0x10000 || TypeOf(animatable) != base + AnimatableTypeInfoRva)
        {
          if (!withoutBody++)
            firstWithoutBody = entity;
          continue;
        }
        out[count++] = { entity, *(__int64*)entity, animatable, classIndex };
      }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
  }

  // Guns within radius of center, drones within droneRadius (radius if less)
  int FindActors(float const* center, float radius, float droneRadius, ActorFound* out, int max, int& withoutBody,
    __int64& firstWithoutBody)
  {
    __int64 base = (__int64)g_gameHandle;
    int count = 0;
    withoutBody = 0;
    firstWithoutBody = 0;
    for (int c = 0; c < (int)_countof(ActorClasses); ++c)
    {
      __int64 typeInfo = base + ActorClasses[c].TypeInfoRva;
      if (!TypeNameIs(typeInfo, ActorClasses[c].Name))
        continue;
      float classRadius = c == DroneActorClass ? (std::max)(radius, droneRadius) : radius;
      for (int list = 0; list < 4; ++list)
        WalkActorList(typeInfo + TypeInstanceListsOffset + list * 0x10, typeInfo, c, center, classRadius, out, max, count,
          withoutBody, firstWithoutBody);
    }
    return count;
  }
}

struct FaithReplay::PersonTrack
{
  __int64 Entity;
  __int64 VTable;
  __int64 Animatable;
  __int64 Data{ 0 };         // its entity data (+ 0x30) when found: the kind of character, for borrowing (see BorrowForGone)
  bool Borrowed{ false };    // played by a live one of its kind from elsewhere (see BorrowForGone)
  char Template[128]{};      // a character's spawn template, from its spawner while it was recorded (see RecordPeople)
  int StandIn{ 0 };          // its stand-in's id while it plays (the hooks' AskStandIn); -1: none could be made
  int StandInMisses{ 0 };    // borrow passes its made stand-in wasn't found in (see BorrowForGone)
  int StuckFrames{ 0 };      // frames in a row its posed body was away from the replayed place (see PlayPeople)
  bool Stuck{ false };       // so the replay draws it itself
  bool NoneLogged{ false };  // "nothing for it" logged this playback (see BorrowForGone)
  int Bones;
  char Label[40];
  int BodyOffset{ CharacterBodyOffset };   // a character's body, or a gun's or drone's (ActorBodyOffset)
  bool Other{ false };                     // not in the recording: hidden while it plays (m_Others)
  std::vector<double> Times;
  PackedFrames Recorded;         // its recorded frames, one for each time
  std::vector<float> Frames;     // one not in the recording (m_Others): its pose as it was, parked with it
  std::vector<float> RootWorld;  // bones 0 - 3 of its world pose at each frame (RootWorldBones)
  bool RootWorldOk{ true };      // there at every frame
  bool Moved{ false };           // its pose changed while it was recorded
  double PendingTime{ -1.0 };    // the last sample, the same as the frame stored before it (not stored yet)
  bool Gone{ false };       // not found any more while recording
  OverrideSet Set{};        // its own two overrides (the game reads them by address)
  __int64* RegisteredOverrides{ nullptr };
  unsigned int RegisteredHandle{ 0xFFFFFFFF };
  bool Registered{ false };
  int Buffer{ 0 };
  // Its bounding box's pose, when that isn't its body (see FindPersonBox)
  OverrideSet BoxSet{};
  __int64* BoxRegisteredOverrides{ nullptr };
  unsigned int BoxRegisteredHandle{ 0xFFFFFFFF };
  bool BoxRegistered{ false };
  int BoxBuffer{ 0 };
  bool BoxLogged{ false };               // this playback
  // This playback (see TakeOver)
  bool Taken{ false };
  std::vector<float> Before;             // its own pose when taken (a hidden one is parked in it)
  std::vector<unsigned char> RawBefore;  // its slot buffers when taken (see RawSlot)
  bool Frozen{ false };                  // the game doesn't update its body (see KeepTaken)
  bool FrozeWhilePlaying{ false };
  int RawRestoreLeft{ 0 };               // frames its slot buffers are still to be put back
  int DrawWait{ 0 };                     // frames before the replay draws it itself (its last update may still run)
  bool DrawTried{ false };
  bool Drawing{ false };                 // the replay writes its slot buffers itself (DrawBody / ParkRaw)
  __int64 HeldBody{ 0 };                 // its body, the game's update of it held off meanwhile (see HoldUpdate)
  __int64 ForcedBody{ 0 };               // its body, switched on by the replay for this playback (see KeepTaken)
  int ForceCooldown{ 0 };                // frames before it's switched on again after the game switched it off
  __int64 Server{ 0 };                   // a drone's server entity, kept in the level by who it is (see ShareGuardPlaces)
  bool GunHeld{ false };                 // a gun away from where the game has it: the replay places it (see KeepGun)
  bool GunPlaced{ false };               // its body's placement given the replay's place (see PlaceGun)
  float GunReal[3]{};                    // where the game had it when that started
  bool DeadLogged{ false };
  // A drone, while it records: samples where the game drew it elsewhere than
  // its pose has it (see RecordDronePlace)
  int DronePlaceSeen{ 0 };
  int DronePlaceUsed{ 0 };
  float DronePlaceMost{ 0.f };
  // A character it draws: its client entity goes where the drawn body's bone 1
  // is (see FollowEntity), and back at stop
  float EntityBefore[16]{};
  float EntityOffset[16]{};              // its transform as seen from its body's bone 1 when it was taken
  bool EntityOffsetOk{ false };
  bool EntityMoved{ false };
  bool DrawFailLogged{ false };
  bool CullBoxSet{ false };              // its culling box goes with the drawn body (see KeepCullBox)
  bool CullBoxLogged{ false };           // this playback
  std::vector<float> Unbind;             // its fixed transforms (ReadUnbind)
  __int64 Manager{ 0 };                  // its pose manager while recorded (its skeleton's hierarchy by it)
  int UnbindTries{ 0 };                  // frames its fixed transforms were looked for while recorded
  std::vector<float> UnbindSeen;         // the last ones read, kept once another frame's agree
  int PoseChecks{ 0 };                   // the diagnostic CheckRecordedPose's lines for it
  // Its parts as the game drew them (stage 2, ReplayGameDraws; see AdoptGameDraws):
  // each a kept draw, its header rows as kept, and the skeleton bone of each of its bones
  struct GameDraw
  {
    int Id;
    __int64 Vb;
    unsigned StartIndex, IndexCount;
    std::vector<int> Map;
    std::vector<float> HeaderRows;
    int PassKey = 0;                        // its pass (replay_gamedraws::SkinCandidate::PassKey)
    // While it records: each of its bones' skeleton bones that fitted it in a
    // frame (within MatchMetres), and how far each was summed over every frame
    // of this part matched since (see AdoptGameDraws: Map is the nearest over all)
    std::vector<std::vector<int>> Cands;
    std::vector<std::vector<float>> Sums;
    int Frames = 0;
    // Whether its bones that fitted one skeleton bone only are in the
    // skeleton's order (-1 too few to tell; see AdoptGameDraws' orderedOver)
    int Ordered = -1;
  };
  std::vector<GameDraw> GameDraws;
  unsigned long long GameDrawIndices{ 0 };   // the set kept: the most detailed one seen (most indices)
  // A gun's or drone's whole world pose, one a stored frame (stage 2b: the
  // game's draws of it posed from it, see AdoptBound; no hierarchy needed, the
  // hunter drone's had a loop); WorldOk false once one couldn't be read
  std::vector<float> WorldFrames;
  bool WorldOk{ true };
  bool BoundDrawn{ false };              // drawn with the game's draws of its mesh (m_Bound)
  // The times the game drew it while it recorded (its draws' tracks' samples,
  // see ThinTracksNearPeople), sorted; empty: not known
  std::vector<double> SeenTimes;
  // For the log: the game's kept draws tried on it (near it) and the closest miss
  int DrawsNear{ 0 };
  float BestMiss{ 1e30f };
  int StateLogs{ 0 };
  // A gun or drone: its entity moved by the replay (see MoveActor)
  float ActorBefore[16]{};               // its transform when taken, put back when it stops
  bool ActorBeforeOk{ false };
  bool ActorHeld{ false };               // the game's own moves of it held back (util::hooks::HoldActor)
  int ActorFrames{ 0 };                  // diagnostic (temporary): frames moved, logged once at ActorCheckFrame
  // While recording, for the log (temporary)
  int GateAlways{ -1 }, GateFlag{ -1 }, GateExternal{ -1 }, GateMode{ -2 };
  bool FromWorldLogged{ false }, NoHierarchyLogged{ false };
  double DiagAt{ -1.0 };
  int DiagSamples{ 0 }, DiagLocalChanged{ 0 }, DiagWorldChanged{ 0 }, DiagFromWorld{ 0 };
  double LastLocalSum{ 0 }, LastWorldSum{ 0 };
  float DiagWorst{ -1.f };
};

namespace
{
  // An entity's data (+ 0x30: a character's data, MEC_Reverse_Notes), 0 if
  // it can't be read
  __int64 ReadEntityData(__int64 entity)
  {
    __try
    {
      __int64 data = *(__int64 const*)(entity + 0x30);
      return data > 0x10000 ? data : 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return 0;
    }
  }

  // An entity's transform, the 4 rows where the models' getter reads them
  // (components + 0x38, + 0x10 + (byte 9 + 2 * byte 0xA) * 0x20)
  bool ReadEntityRows(__int64 entity, float* rows)
  {
    __try
    {
      BYTE const* components = *(BYTE const**)(entity + 0x38);
      if ((__int64)components < 0x10000)
        return false;
      memcpy(rows, components + 0x10 + ((__int64)components[9] + 2 * components[0xA]) * 0x20, 16 * sizeof(float));
      for (int i = 0; i < 16; ++i)
        if (!(fabsf(rows[i]) < 1e6f))
          return false;
      return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
  }

  // A gun's or drone's body goes where its entity is (see the hooks' "Replay
  // guns and drones"): bone 1 of its local pose is the entity's transform
  // (2026-10-23, live: its rotation as XMMatrixRotationQuaternion's rows are
  // the entity's rows), so the entity is put where this pose's bone 1 is. The
  // rows' fourth floats are left as they are (not part of the transform).
  const int ActorCheckFrame = 15;

  bool MoveActor(FaithReplay::PersonTrack& person, float const* bones)
  {
    float rows[16];
    if (!ReadEntityRows(person.Entity, rows))
      return false;
    float const* root = bones + 1 * FloatsPerBone;
    XMFLOAT4X4 m;
    XMStoreFloat4x4(&m, XMMatrixRotationQuaternion(XMVectorSet(root[4], root[5], root[6], root[7])));
    for (int r = 0; r < 3; ++r)
      for (int c = 0; c < 3; ++c)
        rows[r * 4 + c] = m.m[r][c];
    for (int c = 0; c < 3; ++c)
      rows[12 + c] = root[c];
    if (!util::hooks::MoveActorDrawn(person.Entity, rows))
      return false;
    if (!person.ActorHeld)
    {
      util::hooks::HoldActor(person.Entity, true);
      person.ActorHeld = true;
    }
    return true;
  }

  // Diagnostic (temporary): whether the body went with the entity, once
  void CheckActor(FaithReplay::PersonTrack& person, PoseSlot const& slot, float const* bones)
  {
    if (++person.ActorFrames != ActorCheckFrame)
      return;
    float rows[16];
    float root[3] = {}, second[3] = {};
    bool entity = ReadEntityRows(person.Entity, rows);
    __try
    {
      memcpy(root, slot.Local + 1 * FloatsPerBone, sizeof(root));
      if (slot.World)
        memcpy(second, slot.World + 2 * FloatsPerBone, sizeof(second));
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
    float const* wanted = bones + 1 * FloatsPerBone;
    util::log::Write("Replay (diagnostic):   %s 0x%I64X after %d frames moved: wanted at %.2f %.2f %.2f; its entity at %.2f %.2f %.2f, "
      "its body's bone 1 at %.2f %.2f %.2f, its bone 2 drawn at %.2f %.2f %.2f (%.2f m off)", person.Label, person.Entity,
      ActorCheckFrame, wanted[0], wanted[1], wanted[2], entity ? rows[12] : 0.f, entity ? rows[13] : 0.f, entity ? rows[14] : 0.f,
      root[0], root[1], root[2], second[0], second[1], second[2], Distance3(second, wanted));
  }

  bool WriteEntityRows(__int64 entity, float const* rows)
  {
    __try
    {
      BYTE* components = *(BYTE**)(entity + 0x38);
      if ((__int64)components < 0x10000)
        return false;
      memcpy(components + 0x10 + ((__int64)components[9] + 2 * components[0xA]) * 0x20, rows, 16 * sizeof(float));
      return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
  }

  // A bone's world pose (translation, rotation quaternion) as a matrix (rows,
  // translation last: what the entity rows are)
  XMMATRIX BoneMatrix(float const* bone)
  {
    return XMMatrixRotationQuaternion(XMVectorSet(bone[4], bone[5], bone[6], bone[7])) *
      XMMatrixTranslation(bone[0], bone[1], bone[2]);
  }

  XMMATRIX RowsMatrix(float const* rows)
  {
    return XMMatrixSet(rows[0], rows[1], rows[2], 0.f, rows[4], rows[5], rows[6], 0.f, rows[8], rows[9], rows[10], 0.f,
      rows[12], rows[13], rows[14], 1.f);
  }

  // A character the replay draws itself: a body the game doesn't update is
  // kept where its client entity is (2026-10-24, b62c9b2d6196: with the
  // game's update of it held off, each frame its world pose's bone 1 was back
  // on its entity's place, 3 - 12 m from where the replay had just put it;
  // those were drawn where they really stood, or flickered). So its entity
  // goes where the replayed bone 1 is, as it was from its body's bone 1 when
  // the playback took it (its transform as seen from that bone), and back to
  // where it was at stop (it doesn't move by itself meanwhile: the game
  // doesn't animate it). The rows' fourth floats are left as they are.
  void TakeEntity(FaithReplay::PersonTrack& person, PoseSlot const& slot)
  {
    person.EntityOffsetOk = false;
    if (person.BodyOffset != CharacterBodyOffset || !slot.World || !ReadEntityRows(person.Entity, person.EntityBefore))
      return;
    float bone[FloatsPerBone];
    if (!CopyPose(slot.World + 1 * FloatsPerBone, bone, 1))
      return;
    XMVECTOR determinant;
    XMMATRIX inverse = XMMatrixInverse(&determinant, BoneMatrix(bone));
    if (!(fabsf(XMVectorGetX(determinant)) > 1e-6f))
      return;
    XMFLOAT4X4 offset;
    XMStoreFloat4x4(&offset, RowsMatrix(person.EntityBefore) * inverse);
    memcpy(person.EntityOffset, &offset, sizeof(offset));
    person.EntityOffsetOk = true;
  }

  void FollowEntity(FaithReplay::PersonTrack& person, float const* rootWorld)
  {
    float rows[16];
    if (!person.EntityOffsetOk || !ReadEntityRows(person.Entity, rows))
      return;
    XMFLOAT4X4 m;
    XMStoreFloat4x4(&m, XMLoadFloat4x4((XMFLOAT4X4 const*)person.EntityOffset) * BoneMatrix(rootWorld));
    for (int r = 0; r < 4; ++r)
      for (int c = 0; c < 3; ++c)
        rows[r * 4 + c] = m.m[r][c];
    if (WriteEntityRows(person.Entity, rows))
      person.EntityMoved = true;
  }

  // One that isn't in the recording, hidden by the replay drawing it 10 km
  // down (DrawParked): its entity goes down too
  void HideEntity(FaithReplay::PersonTrack& person)
  {
    if (!person.EntityOffsetOk)
      return;
    float rows[16];
    memcpy(rows, person.EntityBefore, sizeof(rows));
    rows[13] -= ParkDrop;
    if (WriteEntityRows(person.Entity, rows))
      person.EntityMoved = true;
  }

  // Its entity back where it was when the playback took it (gone: nothing)
  void PutBackEntity(FaithReplay::PersonTrack& person)
  {
    if (person.EntityMoved && StillCharacter(person.Entity, person.VTable, person.Animatable, person.BodyOffset))
      WriteEntityRows(person.Entity, person.EntityBefore);
    person.EntityMoved = false;
  }

  // Back where it was, and the game's own moves of it let through again
  void PutBackActor(FaithReplay::PersonTrack& person)
  {
    if (!person.ActorHeld)
      return;
    if (person.ActorBeforeOk && StillCharacter(person.Entity, person.VTable, person.Animatable, person.BodyOffset))
      util::hooks::MoveActorDrawn(person.Entity, person.ActorBefore);
    util::hooks::HoldActor(person.Entity, false);
    person.ActorHeld = false;
  }

  // The first time a playback poses a body: its own pose and its slot
  // buffers then (put back when it stops, if the replay wrote them; see
  // KeepTaken)
  bool TakeOver(FaithReplay::PersonTrack& person, PoseSlot const& slot)
  {
    person.ActorBeforeOk = person.BodyOffset == ActorBodyOffset && ReadEntityRows(person.Entity, person.ActorBefore);
    person.ActorFrames = 0;
    person.Taken = true;
    person.Frozen = person.FrozeWhilePlaying = false;
    person.RawRestoreLeft = 0;
    person.DrawWait = 0;
    person.DrawTried = person.Drawing = person.DrawFailLogged = false;
    person.StateLogs = 0;
    person.RawBefore.clear();
    person.Before.resize((size_t)person.Bones * FloatsPerBone);
    OwnPoseInfo info;
    if (!OwnPose(slot, person.Animatable, person.Before.data(), false, true, info))
    {
      person.Before.clear();
      return false;
    }
    RawSlot raw;
    if (FindRawSlot(slot, raw))
    {
      person.RawBefore.resize(raw.Total());
      if (!ReadRaw(raw, person.RawBefore.data()))
        person.RawBefore.clear();
    }
    return true;
  }

  // Each skeleton's fixed transforms as first read, to check the others' by
  struct SkeletonUnbind
  {
    __int64 Manager;
    int Bones;
    std::vector<float> Unbind;
  };
  std::vector<SkeletonUnbind> g_Unbinds;
  const float UnbindTolerance = 0.02f;
  const int MaxUnbindTries = 300;   // frames a recorded person's are looked for (see RecordPeople)
  const int MaxStateLogs = 2;      // per body and playback (some go on and off again and again)
  // A posed body this far (metres) from the replayed place for this many
  // frames in a row is drawn by the replay itself (see PlayPeople)
  const float StuckDistance = 2.f;
  const int StuckFrames = 30;
  const int ForceCooldownFrames = 60;

  // The replay starts writing a body's slot buffers itself (DrawBody, or for
  // one that isn't in the recording ParkRaw): false (logged) if it can't
  bool StartDrawing(FaithReplay::PersonTrack& person, PoseSlot const& slot)
  {
    char const* why = nullptr;
    char detail[160] = "";
    RawSlot raw;
    Hierarchy* hierarchy = FindHierarchy(slot.Manager, slot.Bones);
    if (!FindRawSlot(slot, raw) || person.RawBefore.empty() || raw.Total() != person.RawBefore.size())
      why = "its slot buffers couldn't be read";
    else if (person.Other)
      ;
    else if (!person.Moved)
    {
      // Lying still all through the recording: as it is, it's what was recorded
      util::log::Write("Replay:   %s 0x%I64X: the game doesn't update its body, and it didn't move while it was recorded: "
        "left as it is", person.Label, person.Entity);
      return false;
    }
    else if (!raw.Buffer[RawMatrices])
      why = "its skeleton has no matrices";
    else if (!hierarchy || !hierarchy->Ready || hierarchy->Order.empty())
      why = "its skeleton's hierarchy isn't known (no body of it that the game updated was recorded)";
    else if (!person.RootWorldOk || person.RootWorld.size() != person.Times.size() * RootWorldFloats)
      why = "its world pose wasn't recorded";
    else
    {
      person.Unbind.resize((size_t)slot.Bones * MatrixFloats);
      if (!ReadUnbind(raw, slot.Bones, person.Unbind.data()))
        why = "its matrices aren't its world pose times a fixed transform per bone";
      else
      {
        SkeletonUnbind* known = nullptr;
        for (auto& entry : g_Unbinds)
          if (entry.Manager == slot.Manager && entry.Bones == slot.Bones)
            known = &entry;
        if (!known)
        {
          g_Unbinds.push_back({ slot.Manager, slot.Bones, person.Unbind });
          strcpy_s(detail, ", the first of its skeleton");
        }
        else
        {
          float worst = 0.f;
          for (size_t k = 0; k < known->Unbind.size(); ++k)
            worst = (std::max)(worst, fabsf(known->Unbind[k] - person.Unbind[k]));
          if (!(worst <= UnbindTolerance))
            why = "its matrices' fixed transforms aren't those of the first body of its skeleton";
          sprintf_s(detail, ", its fixed transforms %.4f off the first body's of its skeleton", worst);
        }
        if (!why && hierarchy->Loose)
          sprintf_s(detail + strlen(detail), sizeof(detail) - strlen(detail), ", %d bones without a parent kept where they are",
            hierarchy->Loose);
      }
    }
    if (why)
    {
      util::log::Warning("Replay:   %s 0x%I64X: the game doesn't update its body, and the replay can't %s it itself: %s",
        person.Label, person.Entity, person.Other ? "hide" : "pose", why);
      return false;
    }
    util::log::Write("Replay:   %s 0x%I64X: the game doesn't update its body: the replay %s it itself, its pose buffers "
      "written each frame%s", person.Label, person.Entity, person.Other ? "hides" : "poses", detail);
    return true;
  }

  // How a playback has a body this frame
  enum class BodyMode
  {
    Posed,    // the game updates it: the override poses it
    Drawn,    // it doesn't: the replay writes its slot buffers (the override stays in its slot too)
    Left      // it doesn't, and it's left as it is
  };

  // Every frame a playback has a body: whether the game still updates it
  // (see BodyUpdateOffset). One it doesn't is posed by the replay itself
  // (StartDrawing), 3 frames after it stopped if that was while it played (an
  // update of that frame may still be under way), until the game updates it
  // again; its slot buffers are put back as they were when the playback took
  // it when it stops (StopPeople). One that can't be drawn is left as it is,
  // or, if it stopped while it played, gets its slot buffers back then.
  BodyMode KeepTaken(FaithReplay::PersonTrack& person, PoseSlot const& slot, bool justTaken)
  {
    BodyGates g{};
    bool updated = !ReadBodyGates(person.Animatable, g) || UpdatedByGame(g);
    // A live body the game doesn't animate (too far from the real Faith, or
    // one it was taking out of the level) is switched on for the playback, so
    // the game animates it and the override poses it (see the hooks'
    // hAlwaysUpdate); a dead one never is. Switched off again by the game (its
    // distance switch): switched on again a second later.
    __int64 body = person.Animatable + BodyUpdateOffset;
    if (person.ForcedBody && util::hooks::GameSwitchedBody(person.ForcedBody))
    {
      util::hooks::ReleaseBodyUpdate(person.ForcedBody, false);
      person.ForcedBody = 0;
      person.ForceCooldown = ForceCooldownFrames;
    }
    if (person.ForceCooldown > 0)
      --person.ForceCooldown;
    if (!updated && !person.ForcedBody && person.ForceCooldown == 0)
    {
      if (util::hooks::IsBodyDead(body))
      {
        if (!person.DeadLogged && person.StateLogs < MaxStateLogs)
          util::log::Write("Replay:   %s 0x%I64X: dead (the game switched its body off for that): not switched on", person.Label,
            person.Entity);
        person.DeadLogged = true;
      }
      else if (util::hooks::ForceBodyUpdate(body))
      {
        person.ForcedBody = body;
        if (person.StateLogs++ < MaxStateLogs)
          util::log::Write("Replay:   %s 0x%I64X: alive and not animated by the game: switched on for the playback", person.Label,
            person.Entity);
        updated = ReadBodyGates(person.Animatable, g) && UpdatedByGame(g);
      }
      else if (!person.DeadLogged)
      {
        // Switched off another way than for its distance (a respawn's reset):
        // switching it on can run animation the game took apart
        person.DeadLogged = true;
        if (person.StateLogs++ < MaxStateLogs)
          util::log::Write("Replay:   %s 0x%I64X: not animated, and not switched off for its distance: not switched on", person.Label,
            person.Entity);
      }
    }
    // One the game updates but keeps where it really is (see StuckFrames):
    // drawn by the replay as one it doesn't update
    if (person.Stuck)
      updated = false;
    if (!updated && !person.Frozen)
    {
      person.Frozen = true;
      person.FrozeWhilePlaying = !justTaken;
      person.DrawWait = justTaken ? 0 : RawRestoreFrames;
      person.DrawTried = false;
      if (person.StateLogs++ < MaxStateLogs)
        util::log::Write("Replay:   %s 0x%I64X: the game %s its body (always update %d, +0xFC %d, +0x5C0 %d, mode %d)",
          person.Label, person.Entity, person.Stuck ? "keeps where it really is" : justTaken ? "doesn't update" :
          "stopped updating", g.Always, g.Flag, g.External, g.Mode);
    }
    else if (updated && person.Frozen)
    {
      person.Frozen = false;
      person.Drawing = person.DrawTried = false;
      PutBackEntity(person);
      person.RawRestoreLeft = 0;
      if (person.StateLogs++ < MaxStateLogs)
        util::log::Write("Replay:   %s 0x%I64X: the game updates its body again, posed again", person.Label, person.Entity);
    }
    if (person.RawRestoreLeft > 0)
    {
      RawSlot raw;
      if (FindRawSlot(slot, raw) && raw.Total() == person.RawBefore.size())
        WriteRaw(raw, person.RawBefore.data());
      --person.RawRestoreLeft;
    }
    if (!person.Frozen)
      return BodyMode::Posed;
    if (person.DrawWait > 0)
    {
      --person.DrawWait;
      return BodyMode::Posed;
    }
    if (!person.DrawTried)
    {
      person.DrawTried = true;
      person.Drawing = StartDrawing(person, slot);
      if (person.Drawing)
        TakeEntity(person, slot);
      if (!person.Drawing && person.FrozeWhilePlaying && !person.RawBefore.empty())
        person.RawRestoreLeft = RawRestoreFrames;
    }
    return person.Drawing ? BodyMode::Drawn : BodyMode::Left;
  }

  // The replayed pose of a recorded body the game doesn't update, written into
  // its slot buffers (see WriteBody): local is this frame's (in its override),
  // bones 0 - 3 of its world pose are sampled from the recording; parked: the
  // body is hidden (ParkPose), so those go down too
  // A culling box around a drawn world pose (see util::hooks::SetCullBox):
  // axis aligned, around bones 1 - 3 (sampled) and the ones made from their
  // parents (not bone 0, the origin, nor the ones without a parent, which stay
  // where the game has them); false if it's no body's size
  const float CullBoxMaxSize = 20.f;
  bool PoseCullBox(float const* world, Hierarchy const& hierarchy, float* box)
  {
    float lo[3] = { FLT_MAX, FLT_MAX, FLT_MAX }, hi[3] = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
    auto take = [&](int i)
    {
      float const* t = world + i * FloatsPerBone;
      for (int a = 0; a < 3; ++a)
      {
        lo[a] = (std::min)(lo[a], t[a]);
        hi[a] = (std::max)(hi[a], t[a]);
      }
    };
    for (int i = 1; i < RootWorldBones; ++i)
      take(i);
    for (int i : hierarchy.Order)
      if (hierarchy.Parent[i] >= 0)
        take(i);
    for (int a = 0; a < 3; ++a)
      if (!(hi[a] - lo[a] >= 0.f && hi[a] - lo[a] <= CullBoxMaxSize) || !std::isfinite(lo[a]) || !std::isfinite(hi[a]))
        return false;
    memset(box, 0, util::hooks::CullBoxFloats * sizeof(float));
    box[0] = box[5] = box[10] = 1.f;   // rows
    for (int a = 0; a < 3; ++a)
    {
      float centre = (lo[a] + hi[a]) * 0.5f, half = (hi[a] - lo[a]) * 0.5f;
      box[12 + a] = centre;
      box[16 + a] = -half;
      box[20 + a] = half;
    }
    box[15] = 1.f;
    return true;
  }

  bool DrawBody(FaithReplay::PersonTrack& person, PoseSlot const& slot, float const* local, double time, bool parked,
    float* rootWorld, float* cullBox, bool& cullBoxOk)
  {
    cullBoxOk = false;
    Hierarchy* hierarchy = FindHierarchy(slot.Manager, slot.Bones);
    RawSlot raw;
    if (!hierarchy || !hierarchy->Ready || hierarchy->Order.empty() || slot.Bones <= RootWorldBones ||
        person.Unbind.size() != (size_t)slot.Bones * MatrixFloats || person.RootWorld.size() != person.Times.size() * RootWorldFloats ||
        !FindRawSlot(slot, raw))
      return false;
    static std::vector<float> world;
    world.resize((size_t)slot.Bones * FloatsPerBone);
    SamplePose(person.Times, person.RootWorld, RootWorldBones, time, world.data());
    if (parked)
      for (int i = 1; i < RootWorldBones; ++i)
        world[i * FloatsPerBone + 1] -= ParkDrop;
    memcpy(rootWorld, &world[1 * FloatsPerBone], FloatsPerBone * sizeof(float));
    if (!WriteBody(raw, slot.Bones, local, world.data(), hierarchy->Order.data(), (int)hierarchy->Order.size(),
        hierarchy->Parent.data(), person.Unbind.data()))
      return false;
    cullBoxOk = PoseCullBox(world.data(), *hierarchy, cullBox);
    return true;
  }

  // A character the replay draws: its culling box goes with the drawn body
  // (box), or is the game's again (null); see util::hooks::SetCullBox
  void KeepCullBox(FaithReplay::PersonTrack& person, float const* box)
  {
    if (box && person.BodyOffset == CharacterBodyOffset && !person.Other)
    {
      float game[3];
      bool first = !person.CullBoxSet;
      person.CullBoxSet = util::hooks::SetCullBox(person.Entity, box, game);
      if (first && person.CullBoxSet && !person.CullBoxLogged)
      {
        person.CullBoxLogged = true;
        float centre[3] = { box[12], box[13], box[14] };
        util::log::Write("Replay:   %s 0x%I64X: its culling box goes with the drawn body (%.1f x %.1f x %.1f m; the game's was "
          "%.1f m from it)", person.Label, person.Entity, box[20] * 2.f, box[21] * 2.f, box[22] * 2.f, Distance3(game, centre));
      }
    }
    else if (person.CullBoxSet)
    {
      util::hooks::SetCullBox(person.Entity, nullptr, nullptr);
      person.CullBoxSet = false;
    }
  }

  // A body that isn't in the recording and that the game doesn't update,
  // hidden: its slot buffers from when it was taken, moved down (ParkRaw)
  bool DrawParked(FaithReplay::PersonTrack& person, PoseSlot const& slot)
  {
    RawSlot raw;
    if (person.RawBefore.empty() || !FindRawSlot(slot, raw) || raw.Total() != person.RawBefore.size())
      return false;
    static std::vector<unsigned char> data;
    data = person.RawBefore;
    ParkRaw(raw, slot.Bones, data.data());
    return WriteRaw(raw, data.data());
  }

  // A dropped gun's body is placed by the game from its physics part
  // (0x1432E06B0: [body + 0xC0] / + 0xC8), not from its entity, so it stayed
  // where it lies while the replay moved its entity (2026-10-24 log: 1.4 -
  // 2.4 m off until the replay reached the drop, then 0). While the replayed
  // gun is away from where the game has it, its body's update is held, so the
  // game's own placement doesn't run and its pose takes the override (as it
  // did for the held taken-down KrugerSec in the same log); let go where
  // they meet again. A gun in a drawn hand is placed with the hand, so it
  // isn't away.
  const float GunHoldDistance = 0.25f;
  const float GunReleaseDistance = 0.05f;
  // At stop a held gun keeps the hold that many frames with no override, so
  // the game's pose puts its own back before it's let go (see StopPeople)
  const int GunTailFrames = 3;

  bool IsGun(FaithReplay::PersonTrack const& person)
  {
    return person.BodyOffset == ActorBodyOffset && !strcmp(person.Label, ActorClasses[0].Name);
  }

  bool IsDrone(FaithReplay::PersonTrack const& person)
  {
    return person.BodyOffset == ActorBodyOffset && !strcmp(person.Label, ActorClasses[1].Name);
  }

  // A drone's place as the game draws it: its body's placement (see
  // util::hooks::ReadBodyPlace), which is its physics part's once one moves it
  // (as a dropped gun's is). Its entity and pose stayed where it was shot down
  // while it fell (2026-10-24 22:4x: a dead drone stayed in the air in the
  // replay), so bone 1 of its recorded pose (where the replay puts its entity
  // and its placement, see MoveActor and PlaceGun) is that place when it's
  // more than DronePlaceDistance off.
  const float DronePlaceDistance = 0.02f;

  void RecordDronePlace(FaithReplay::PersonTrack& person, float* frame, double time)
  {
    float rows[16];
    if (!util::hooks::ReadBodyPlace(person.Animatable + BodyUpdateOffset, rows))
      return;
    ++person.DronePlaceSeen;
    float* root = frame + 1 * FloatsPerBone;
    float off = Distance3(root, rows + 12);
    if (off <= DronePlaceDistance)
      return;
    XMFLOAT4X4 m(rows[0], rows[1], rows[2], 0.f, rows[4], rows[5], rows[6], 0.f, rows[8], rows[9], rows[10], 0.f, 0.f, 0.f, 0.f, 1.f);
    XMFLOAT4 q;
    XMStoreFloat4(&q, XMQuaternionNormalize(XMQuaternionRotationMatrix(XMLoadFloat4x4(&m))));
    if (!person.DronePlaceUsed)
      util::log::Write("Replay:   %s 0x%I64X at %.2f s: drawn %.2f m from its pose (at %.1f %.1f %.1f, its pose at %.1f %.1f %.1f): "
        "recorded where it's drawn from here on", person.Label, person.Entity, time, off, rows[12], rows[13], rows[14], root[0], root[1],
        root[2]);
    for (int c = 0; c < 3; ++c)
      root[c] = rows[12 + c];
    root[4] = q.x;
    root[5] = q.y;
    root[6] = q.z;
    root[7] = q.w;
    ++person.DronePlaceUsed;
    person.DronePlaceMost = (std::max)(person.DronePlaceMost, off);
  }

  bool ReadGunPlace(float const* world, float* place)
  {
    __try
    {
      memcpy(place, world + 1 * FloatsPerBone, 3 * sizeof(float));
      return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
  }

  // Whether a gun is away from where the game has it this frame; root: where
  // the replay has it (8 floats: place, rotation; see GunRoot)
  bool KeepGun(FaithReplay::PersonTrack& person, PoseSlot const& slot, float const* root)
  {
    if (!IsGun(person))
      return false;
    float const* place = root;
    if (!person.GunHeld)
    {
      // Where the game has it now (it updates it while it isn't held)
      if (!slot.World || !ReadGunPlace(slot.World, person.GunReal) || Distance3(place, person.GunReal) <= GunHoldDistance)
        return false;
      person.GunHeld = true;
      if (person.StateLogs++ < MaxStateLogs)
        util::log::Write("Replay:   %s 0x%I64X: %.2f m from where the game has it (%.1f %.1f %.1f): the replay places it",
          person.Label, person.Entity, Distance3(place, person.GunReal), person.GunReal[0], person.GunReal[1], person.GunReal[2]);
    }
    else if (Distance3(place, person.GunReal) < GunReleaseDistance)
    {
      person.GunHeld = false;
      if (person.StateLogs++ < MaxStateLogs)
        util::log::Write("Replay:   %s 0x%I64X: back where the game has it: let go", person.Label, person.Entity);
    }
    return person.GunHeld;
  }

  // Where the replay has a gun this frame: bone 1 of its recorded world pose
  // (RootWorld). Its own pose's bone 1 isn't where it was while a hand held
  // it (2026-10-24 20:0x: placed from it, guns floated beside the hands).
  // Before its first sample and after it went, the override's (parked).
  float const* GunRoot(FaithReplay::PersonTrack const& person, double time, bool parked, float const* bones, float* root)
  {
    if (!parked && person.RootWorldOk && !person.Times.empty() &&
        person.RootWorld.size() == person.Times.size() * RootWorldFloats)
    {
      float world[RootWorldFloats];
      SamplePose(person.Times, person.RootWorld, RootWorldBones, time, world);
      memcpy(root, world + 1 * FloatsPerBone, FloatsPerBone * sizeof(float));
      return root;
    }
    return bones + 1 * FloatsPerBone;
  }

  // A gun the replay places (see KeepGun), or a drone it moves (see
  // MoveActor): the game keeps updating it, and its placement is given the
  // replay's rotation and place (bone 1 of its pose; see
  // util::hooks::PlaceBody), or its own again (place false). A drone shot
  // down is drawn where its physics part is (see RecordDronePlace), not where
  // its entity is moved.
  void PlaceGun(FaithReplay::PersonTrack& person, bool place, float const* root)
  {
    __int64 body = person.Animatable + BodyUpdateOffset;
    if (!place)
    {
      if (person.GunPlaced)
        util::hooks::PlaceBody(body, nullptr);
      person.GunPlaced = false;
      return;
    }
    XMFLOAT4X4 m;
    XMStoreFloat4x4(&m, XMMatrixRotationQuaternion(XMVectorSet(root[4], root[5], root[6], root[7])));
    float rows[16] = {};
    for (int r = 0; r < 3; ++r)
      for (int c = 0; c < 3; ++c)
        rows[r * 4 + c] = m.m[r][c];
    for (int c = 0; c < 3; ++c)
      rows[12 + c] = root[c];
    rows[15] = 1.f;
    person.GunPlaced = util::hooks::PlaceBody(body, rows);
  }

  // A body the replay draws itself: the game's own update of it held off, so
  // it doesn't put its own pose (a taken-down KrugerSec's ragdoll) in the same
  // buffers between the replay's writes (see util::hooks::HoldBodyUpdate);
  // let go when it isn't drawn, or went
  void HoldUpdate(FaithReplay::PersonTrack& person, bool hold)
  {
    __int64 body = hold ? person.Animatable + BodyUpdateOffset : 0;
    if (person.HeldBody == body)
      return;
    if (person.HeldBody)
      util::hooks::HoldBodyUpdate(person.HeldBody, false);
    person.HeldBody = body && util::hooks::HoldBodyUpdate(body, true) ? body : 0;
  }

  // A person's box override taken out of the slot it was handed to
  void DropBox(FaithReplay::PersonTrack& person)
  {
    if (!person.BoxRegistered)
      return;
    UnregisterOverride(person.BoxSet, person.BoxRegisteredOverrides, person.BoxRegisteredHandle);
    person.BoxRegistered = false;
  }

  // A recorded character's bounding box pose this frame (see FindPersonBox):
  // when it isn't its body, the same pose as its body's override (bones: this
  // frame's), so the box is where the replayed body is; logged once a
  // playback. Diagnostic (temporary): with diagnose, where the box pose's
  // bone 1 ended up last frame.
  void PoseBox(FaithReplay::PersonTrack& person, PoseSlot const& body, float const* bones, bool diagnose)
  {
    PoseSlot box{};
    BoxKind kind = FindPersonBox(person.Entity, person.Animatable, body, box);
    if (!person.BoxLogged)
    {
      person.BoxLogged = true;
      if (kind == BoxKind::Other)
        util::log::Write("Replay:   %s 0x%I64X: its bounding box is another pose of it (handle 0x%X, its body's 0x%X): "
          "that gets the replay too", person.Label, person.Entity, box.Handle, body.Handle);
      else
        util::log::Write("Replay:   %s 0x%I64X: its bounding box %s", person.Label, person.Entity,
          kind == BoxKind::Body ? "is its body's pose (it follows the replay)" :
          kind == BoxKind::None ? "isn't made of any of its poses" :
          "is another animatable, not a pose slot like its body's: left where the real one is");
    }
    if (diagnose && person.BoxRegistered && kind == BoxKind::Other)
    {
      float root[3];
      PoseOverride const* last = &person.BoxSet.Override[person.BoxBuffer ^ 1];
      if (ReadWorldRoot(box, root))
        util::log::Write("Replay:   0x%I64X: its box pose (world bone 1) %.2f m from the replayed body", person.Entity,
          Distance3(root, last->Bones + 1 * FloatsPerBone));
    }
    if (person.BoxRegistered && (kind != BoxKind::Other || box.Overrides != person.BoxRegisteredOverrides ||
        box.Handle != person.BoxRegisteredHandle))
      DropBox(person);
    if (kind != BoxKind::Other || !PrepareOverride(person.BoxSet, person.Bones))
      return;
    PoseOverride* pBox = &person.BoxSet.Override[person.BoxBuffer];
    if (!CopyPose(bones, pBox->Bones, person.Bones) || !RegisterOverride(box, pBox))
      return;
    person.BoxRegistered = true;
    person.BoxRegisteredOverrides = box.Overrides;
    person.BoxRegisteredHandle = box.Handle;
    person.BoxBuffer ^= 1;
  }

  // What one recorded frame of a person takes
  size_t PersonFrameBytes(FaithReplay::PersonTrack const& person)
  {
    return PackedFrames::FrameBytes(person.Bones) + (person.RootWorldOk ? RootWorldFloats * sizeof(float) : 0) + sizeof(double) +
      (person.BodyOffset == ActorBodyOffset && person.WorldOk ? (size_t)person.Bones * FloatsPerBone * sizeof(float) : 0);
  }

  // After a failed allocation: a person's frames and roots back in step with
  // their times (making them smaller doesn't allocate)
  void TrimPerson(FaithReplay::PersonTrack& person)
  {
    person.Recorded.Truncate(person.Times.size());
    if (person.RootWorldOk)
      person.RootWorld.resize(person.Times.size() * RootWorldFloats);
    size_t world = person.Times.size() * (size_t)person.Bones * FloatsPerBone;
    if (person.WorldFrames.size() > world)
      person.WorldFrames.resize(world);
    else if (person.WorldFrames.size() < world)
    {
      person.WorldOk = false;
      person.WorldFrames.clear();
    }
    person.PendingTime = -1.0;
  }

  // What Windows can still commit for the game, in bytes (the most there is
  // when that isn't known)
  unsigned long long FreeCommit()
  {
    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);
    return GlobalMemoryStatusEx(&status) ? status.ullAvailPageFile : ~0ull;
  }

  // The end of a run of samples the same as the frame stored before them:
  // that frame again, at the last one's time (so the replay holds it until
  // then, and moves on from there). The bytes it took (0 too when there was
  // no room for it: the run's end is left out).
  size_t FlushPending(FaithReplay::PersonTrack& person)
  {
    if (person.PendingTime < 0 || person.Times.empty())
    {
      person.PendingTime = -1.0;
      return 0;
    }
    try
    {
      person.Recorded.RepeatLast();
      if (person.RootWorldOk)
      {
        size_t rootAt = person.RootWorld.size();
        person.RootWorld.resize(rootAt + RootWorldFloats);
        memcpy(&person.RootWorld[rootAt], &person.RootWorld[rootAt - RootWorldFloats], RootWorldFloats * sizeof(float));
      }
      size_t worldFloats = (size_t)person.Bones * FloatsPerBone;
      if (person.WorldOk && person.WorldFrames.size() == person.Times.size() * worldFloats && !person.WorldFrames.empty())
      {
        size_t at = person.WorldFrames.size();
        person.WorldFrames.resize(at + worldFloats);
        memcpy(&person.WorldFrames[at], &person.WorldFrames[at - worldFloats], worldFloats * sizeof(float));
      }
      person.Times.push_back(person.PendingTime);
    }
    catch (std::bad_alloc const&)
    {
      TrimPerson(person);
      return 0;
    }
    person.PendingTime = -1.0;
    return PersonFrameBytes(person);
  }

  // A gun's or drone's whole world pose as the game has it now (its pose
  // slot's), with the frame just stored (stage 2b: the game's draws of it
  // posed from it, see AdoptBound; 2026-10-27 17:51 its slot's matrices
  // weren't where the game drew it, 1.2 km off, or couldn't be read). Given up
  // on (and let go) once it can't be read, or isn't in step with its frames.
  void AppendWorld(FaithReplay::PersonTrack& person, PoseSlot const& slot)
  {
    size_t floats = (size_t)person.Bones * FloatsPerBone;
    bool ok = !person.Times.empty() && person.WorldFrames.size() == (person.Times.size() - 1) * floats && slot.World;
    if (ok)
    {
      try
      {
        size_t at = person.WorldFrames.size();
        person.WorldFrames.resize(at + floats);
        ok = CopyPose(slot.World, &person.WorldFrames[at], person.Bones);
      }
      catch (std::bad_alloc const&)
      {
        ok = false;
      }
    }
    if (!ok)
    {
      person.WorldOk = false;
      person.WorldFrames.clear();
      person.WorldFrames.shrink_to_fit();
    }
  }

  // Diagnostic (temporary): how the game updates a recorded body, logged when
  // that changes; for one it doesn't update, every second, how often its
  // local and world poses changed, and whether it was recorded from the world one
  void NoteRecordedBody(FaithReplay::PersonTrack& person, PoseSlot const& slot, OwnPoseInfo const& info, double time)
  {
    if (!info.Gated)
      return;
    BodyGates const& g = info.Gates;
    bool updated = UpdatedByGame(g);
    if (g.Always != person.GateAlways || g.Flag != person.GateFlag || g.External != person.GateExternal || g.Mode != person.GateMode)
    {
      person.GateAlways = g.Always;
      person.GateFlag = g.Flag;
      person.GateExternal = g.External;
      person.GateMode = g.Mode;
      util::log::Write("Replay:   0x%I64X at %.2f s: always update %d, +0xFC %d, +0x5C0 %d, mode %d: the game %s its body",
        person.Entity, time, g.Always, g.Flag, g.External, g.Mode, updated ? "updates" : "doesn't update");
    }
    if (updated)
    {
      person.DiagAt = -1.0;
      return;
    }
    if (info.FromWorld && !person.FromWorldLogged)
    {
      person.FromWorldLogged = true;
      util::log::Write("Replay:   0x%I64X: recorded from its world pose from %.2f s on (its local pose was %.2f m off it)",
        person.Entity, time, info.Error);
    }
    if (info.Error < 0 && !person.NoHierarchyLogged)
    {
      person.NoHierarchyLogged = true;
      util::log::Write("Replay:   0x%I64X: its skeleton's hierarchy isn't known (no body of it the game updates seen yet): "
        "recorded from its local pose", person.Entity);
    }
    double localSum = 0, worldSum = 0;
    bool sums = PoseSum(slot.Local, person.Bones, localSum) && slot.World && PoseSum(slot.World, person.Bones, worldSum);
    if (person.DiagAt < 0)
    {
      person.DiagAt = time;
      person.DiagSamples = person.DiagLocalChanged = person.DiagWorldChanged = person.DiagFromWorld = 0;
      person.DiagWorst = -1.f;
    }
    else if (sums)
    {
      person.DiagLocalChanged += localSum != person.LastLocalSum;
      person.DiagWorldChanged += worldSum != person.LastWorldSum;
    }
    if (sums)
    {
      person.LastLocalSum = localSum;
      person.LastWorldSum = worldSum;
    }
    ++person.DiagSamples;
    person.DiagFromWorld += info.FromWorld;
    if (info.Error > person.DiagWorst)
      person.DiagWorst = info.Error;
    if (time - person.DiagAt >= 1.0)
    {
      char off[48] = "its skeleton's hierarchy not known";
      if (person.DiagWorst >= 0)
        sprintf_s(off, "its local pose up to %.2f m off", person.DiagWorst);
      util::log::Write("Replay:   0x%I64X %.2f - %.2f s, not updated by the game: its local pose changed in %d of %d "
        "samples, its world pose in %d; recorded from the world pose in %d; %s", person.Entity, person.DiagAt, time,
        person.DiagLocalChanged, person.DiagSamples, person.DiagWorldChanged, person.DiagFromWorld, off);
      person.DiagAt = time;
      person.DiagSamples = person.DiagLocalChanged = person.DiagWorldChanged = person.DiagFromWorld = 0;
      person.DiagWorst = -1.f;
    }
  }
}

FaithReplay::FaithReplay()
{
}

FaithReplay::~FaithReplay()
{
  util::hooks::HoldAIDespawn(false);
  util::hooks::ProtectFaith(false);
  SetAnimationCulling(true);
  // The game may still hold an override: theirs are taken out and never freed
  StopPeople();
  for (auto& person : m_People)
    person.release();
  for (auto& other : m_Others)
    other.release();
}

void FaithReplay::StartRecording()
{
  // The one before saved first if it's still to be (see SaveRecording)
  if (m_SaveAt >= 0.0)
    SaveRecording();
  m_SaveWhere.clear();
  m_LoadedFrom.clear();
  m_Models.Clear();
  // The world's own draws kept too (see ReplayGameDraws' CaptureWorld), and
  // any skinned draw's track (CaptureTrack)
  replay_gamedraws::SetWorldCapture(true);
  replay_gamedraws::SetTrackCapture(true);
  m_WorldScanAt = -1.0;
  // Diagnostic (temporary): the game's draws described while it records (ReplayDrawLog.h)
  replay_drawlog::Start();
  ClearPeople();
  g_Hierarchies.clear();   // a manager's memory may be another skeleton's since
  g_Unbinds.clear();
  m_ActorsWithoutBodyLogged = false;
  m_Times.clear();
  m_Frames.clear();
  m_FaithPlaces.clear();
  m_Tods.clear();
  m_OutfitTimes.clear();
  m_Outfits.clear();
  m_Loads.clear();
  m_LoadUp = false;
  m_LookTimes.clear();
  m_Looks.clear();
  m_Bones = 0;
  m_RecordClock = 0;
  m_LastSample = -1;
  m_State = State_Recording;
  m_Message[0] = 0;
  util::hooks::ReleaseWatchedBodies();
  util::hooks::ReplaySoundsStartRecording();
  util::hooks::ReplayEffectsStartRecording();
  util::hooks::MissionItemsStartRecording();
  util::hooks::ReplayRopeStartRecording();
  m_RecordedWeather = weather::GetState();
  m_HaveWeather = true;
  // The spawners' characters, for the recorded people's templates (see RecordPeople)
  util::hooks::WatchSpawners(true);
  // The mission's world is kept once it's over (the last recording's let go)
  util::hooks::MissionWorldsRecording(1);
  // The game's sound itself (see ReplayAudio.h)
  if (m_SoundTrackOn)
    replay_audio::StartRecording();
  else
    replay_audio::Forget();
  util::log::Write("Replay: recording Faith");
}

// A recording that crosses a loading screen (2026-10-26: the Release mission
// started while recording, 1 - 19 s; its playback after the mission was over
// found none of its people, the mission's world unloaded, and its first part
// is in free roam elsewhere). Each loading screen it crossed is kept (the
// hooks' LoadScreenShown: the game's show / hide events, 0x143B2E130), with
// what the game had once it went: Faith's outfit (the Release mission dresses
// her in C_Faith_Prologue or its no-gridlink one, see FaithOutfit.h) and the
// time of day. Playing then starts where its last loading screen went (an
// option), and when its people aren't there the menu says where it was
// recorded. Which mission was running isn't read (where the game keeps it
// wasn't traced): only the Release mission is named, from her outfit.
void FaithReplay::NoteRecordedLoads()
{
  bool up = util::hooks::LoadScreenShown();
  if (up && !m_LoadUp)
    m_Loads.push_back({ m_RecordClock, -1.0, -1, -1.f });
  else if (!up && m_LoadUp && !m_Loads.empty() && m_Loads.back().End < 0.0)
  {
    RecordedLoad& load = m_Loads.back();
    load.End = m_RecordClock;
    load.OutfitAfter = faith_outfit::GameOutfit();
    load.TodAfter = g_mainHandle->GetVisualsManager()->GetCurrentTimeOfDay();
    util::log::Write("Replay: the recording crossed a loading screen, %.1f - %.1f s%s", load.Start, load.End,
      faith_outfit::IsReleaseOutfit(load.OutfitAfter) ? " (into the Release mission: Faith's outfit)" : "");
  }
  m_LoadUp = up;
}

const char* FaithReplay::WhereRecorded(char* buffer, size_t size) const
{
  buffer[0] = 0;
  if (m_Loads.empty() || m_Loads.back().End < 0.0)
    return buffer;
  RecordedLoad const& load = m_Loads.back();
  char tod[16] = "";
  if (load.TodAfter >= 0.f)
    sprintf_s(tod, " (%02d:%02d)", ((int)load.TodAfter / 3600) % 24, ((int)load.TodAfter / 60) % 60);
  if (faith_outfit::IsReleaseOutfit(load.OutfitAfter))
    sprintf_s(buffer, size, "the Release mission, after the loading screen at %.0f - %.0f s%s", load.Start, load.End, tod);
  else
    sprintf_s(buffer, size, "what a loading screen at %.0f - %.0f s took Faith to (a mission or a fast travel)%s", load.Start,
      load.End, tod);
  return buffer;
}

void FaithReplay::StopRecording()
{
  m_State = State_Idle;
  if (!m_Loads.empty() && m_Loads.back().End < 0.0)
    m_Loads.pop_back();   // still up at the end: nothing after it in the recording
  m_LoadUp = false;
  util::hooks::ReplaySoundsStopRecording();
  util::hooks::ReplayEffectsStopRecording();
  util::hooks::MissionItemsStopRecording();
  util::hooks::ReplayRopeStopRecording();
  util::hooks::WatchSpawners(false);
  replay_drawlog::Stop();
  // Kept while this recording is (none to play: let go)
  util::hooks::MissionWorldsRecording(m_Times.size() >= 2 ? 2 : 0);
  replay_audio::StopRecording();
  // The sound track: the dialog only (the voice lines' spans; all of it when
  // the voice lines aren't hooked: nothing says where the dialog is then)
  {
    std::vector<std::pair<double, double>> spans;
    if (util::hooks::ReplayVoiceLineSpans(spans))
      replay_audio::KeepOnly(spans, DialogBeforeSeconds, DialogAfterSeconds);
  }
  m_Models.StopRecording();
  util::log::Write("Replay: recorded %.1f s (%d frames of %d bones, %.1f MB)", Length(), (int)m_Times.size(), m_Bones,
    m_Frames.size() * sizeof(float) / 1048576.0);
  int characters = 0, templated = 0;
  for (auto& person : m_People)
  {
    m_PeopleBytes += FlushPending(*person);
    characters += person->BodyOffset == CharacterBodyOffset;
    templated += person->Template[0] != 0;
  }
  util::log::Write("Replay: %d people and %d guns or drones near Faith recorded too (%.1f MB); %d of the people with the "
    "spawn template they were made from", characters, (int)m_People.size() - characters, m_PeopleBytes / 1048576.0, templated);
  m_Vehicles.StopRecording();
  for (auto const& person : m_People)
    if (!person->Times.empty())
    {
      char const* slash = strrchr(person->Template, '/');
      util::log::Write("Replay:   %s 0x%I64X: %d bones, %.2f - %.2f s, %d frames stored%s%s%s%s", person->Label, person->Entity,
        person->Bones, person->Times.front(), person->Times.back(), (int)person->Times.size(),
        person->Moved ? "" : ", didn't move", person->Gone ? " (gone before the end)" : "", person->Template[0] ? ", made from " : "",
        slash ? slash + 1 : person->Template);
    }
  // The drones: how often the game drew them elsewhere than their pose (see RecordDronePlace)
  for (auto const& person : m_People)
    if (IsDrone(*person))
      util::log::Write("Replay:   %s 0x%I64X: the game's placement seen in %d samples, %d of them away from its pose (up to %.2f m)",
        person->Label, person->Entity, person->DronePlaceSeen, person->DronePlaceUsed, person->DronePlaceMost);
  // The game's own draws of them (stage 2 / 2b): the last ones kept, then who has none and why
  if (m_GameDrawsOn && m_ModelsOn && m_ModelShapesOn)
    AdoptGameDraws();
  LogGameDrawsMissing();
  // Guns', drones' and doors' draws: no more kept, those waiting let go
  for (auto const& c : m_SkinPool)
    replay_gamedraws::ReleaseSkin(c.Id);
  m_SkinPool.clear();
  replay_gamedraws::SetWantedSkins({});
  util::hooks::ReleaseWatchedBodies();
  // The world's own draws and the tracks: no more kept, their meshes named
  replay_gamedraws::SetWorldCapture(false);
  replay_gamedraws::SetTrackCapture(false);
  replay_gamedraws::UpdateWorld(true, false);
  {
    int tracks = 0;
    unsigned long long bytes = 0;
    int theirs = ThinTracksNearPeople(tracks, bytes);
    AnchorClothsToPeople();
    util::log::Write("Replay: %d tracks of other skinned draws kept (drones, crowds: %.1f MB), %d of their instances let go (a "
      "recorded person's, gun's or drone's drawn with its own kept draws, or Faith's); %d texture slots of the kept draws taken "
      "anew from the game's better ones while it recorded", tracks, bytes / 1048576.0, theirs, replay_gamedraws::Upgraded());
    int kinds = 0, samples = 0, bad = 0;
    unsigned long long crowdBytes = 0;
    replay_gamedraws::CrowdCounts(kinds, samples, bad, crowdBytes);
    util::log::Write("Replay: crowds: %d kinds of crowd draws kept (a mesh part and look each), %d samples of their members (%.1f MB)%s",
      kinds, samples, crowdBytes / 1048576.0, bad ? "; some draws' members couldn't be read (their first entry past the buffer)" : "");
  }
  {
    int nearPeople = DropWorldNearPeople();
    int kept = 0, ready = 0;
    replay_gamedraws::WorldCounts(kept, ready);
    util::log::Write("Replay: %d of the world's own draws kept near Faith (%d of them read back with their mesh known), drawn where "
      "the game doesn't draw their meshes when it plays; %d let go (kept within 2.5 m of Faith or a recorded person, gun or "
      "drone: a part of theirs drawn without bones)", kept, ready, nearPeople);
  }
  // Saved a moment from now: its last kept draws are read back by then (see SaveRecording)
  if (m_SaveOn && m_Times.size() >= 2)
    m_SaveAt = NowSeconds() + 1.5;
}

void FaithReplay::StartPlaying()
{
  if (m_Times.size() < 2)
    return;
  m_State = State_Playing;
  m_Paused = false;
  m_PlayTime = 0;
  m_Message[0] = 0;
  m_PeopleLogged = false;
  m_PeopleShown = m_PeopleGone = m_PeopleNoSlot = -1;
  m_PeopleLogAt = -10.0;
  m_Refound = 0;
  m_Borrowed = 0;
  m_StandIns = m_StandInsPlaying = 0;
  m_LastOthersScan = -1.0;
  // Where she is: from this playback's first pose on (see GetPlayedFaith)
  m_PlayRootOk = false;
  // Each taken over afresh (see TakeOver)
  for (auto& person : m_People)
  {
    person->Taken = person->Frozen = person->FrozeWhilePlaying = false;
    person->RawRestoreLeft = person->DrawWait = 0;
    person->DrawTried = person->Drawing = person->DrawFailLogged = false;
    person->BoxLogged = person->CullBoxLogged = false;
    person->StateLogs = 0;
    person->Before.clear();
    person->RawBefore.clear();
    person->StandIn = 0;
    person->StuckFrames = 0;
    person->Stuck = false;
    person->NoneLogged = false;
  }
  util::log::Write("Replay: playing %.1f s on Faith's body (her own pose's root goes along)", Length());
  // The spawners' characters: the live ones' kinds, and stand-ins (see BorrowForGone)
  util::hooks::WatchSpawners(m_PeopleOn && m_StandInsOn);
  StartStandInWait();
  // Its sound track in place of the game's sound (see ReplayAudio.h)
  m_SoundTrackPlaying = m_SoundTrackOn && replay_audio::StartPlaying();
  // A recording that crossed a loading screen: from where the last one went
  // (what's before it is somewhere else; see NoteRecordedLoads)
  if (m_StartAfterLoad && !m_Loads.empty() && m_Loads.back().End > 0.0 && m_Loads.back().End < Length() - 1.0)
  {
    m_SeekTo = (float)m_Loads.back().End;
    util::log::Write("Replay: it starts at %.1f s, where the loading screen it crossed went", m_Loads.back().End);
  }
  util::hooks::SetReplayPlaying(true);
  if (m_ModelsOn)
    m_Models.StartPlaying();
}

void FaithReplay::StopPlaying()
{
  m_State = State_Idle;
  m_Paused = false;
  replay_gamedraws::SetPeopleDraws({});
  replay_gamedraws::SetClothAnchors({});
  replay_gamedraws::SetWorldCenter(nullptr);
  replay_gamedraws::SetPlayTime(-1.0);
  util::hooks::SetReplayPlaying(false);
  util::hooks::ReplaySoundsStop();
  util::hooks::ReplayEffectsStop();
  util::hooks::ReplayRopeStop();
  // The game's sound back
  replay_audio::StopPlaying();
  m_SoundTrackPlaying = false;
  m_StandInWaitUntil = 0;
  m_Models.StopPlaying();
  util::hooks::MissionItemsStopPlaying();
  StopPeople();
  // Given back by StopPeople: the stand-ins go out of the level now
  util::hooks::RemoveStandIns();
  util::hooks::WatchSpawners(false);
  if (m_StandIns)
    util::log::Write("Replay: %d stand-ins were asked of the game's spawners, %d played their recorded person; they're taken out of "
      "the level at the server's next update", m_StandIns, m_StandInsPlaying);
  if (m_TodApplied)
  {
    g_mainHandle->GetVisualsManager()->StopReplayTimeOfDay();
    m_TodApplied = false;
  }
  ApplyRecordedWeather(false);
  ApplyRecordedLook(-1.0);
  // Dressed back as the menu or the game has her
  if (m_OutfitApplied)
    faith_outfit::SetReplayOutfit(-1);
  m_OutfitApplied = false;
  if (m_Refound)
    util::log::Write("Replay: %d recorded people who weren't in the level any more were played by others standing where they were",
      m_Refound);
  if (m_Borrowed)
    util::log::Write("Replay: %d recorded people who weren't in the level any more were played by live ones of their own "
      "template from elsewhere (given back now)", m_Borrowed);
  util::log::Write("Replay: stopped");
}

// The game's weather as recorded (clouds and wind as it showed them), in a
// straight line between the two samples around the time (see
// VisualsManager::SetReplayWeatherLook)
void FaithReplay::ApplyRecordedLook(double time)
{
  size_t count = m_LookSize ? (std::min)(m_LookTimes.size(), m_Looks.size() / m_LookSize) : 0;
  VisualsManager* pVisuals = g_mainHandle->GetVisualsManager();
  if (time < 0.0 || count == 0)
  {
    if (m_LookApplied)
      pVisuals->SetReplayWeatherLook(nullptr, nullptr, 0.f);
    m_LookApplied = false;
    return;
  }
  size_t lo = 0, hi = 0;
  float t = 0.f;
  if (time >= m_LookTimes[count - 1])
    lo = hi = count - 1;
  else if (time > m_LookTimes[0])
  {
    hi = count - 1;
    while (hi - lo > 1)
    {
      size_t mid = (lo + hi) / 2;
      if (m_LookTimes[mid] <= time) lo = mid; else hi = mid;
    }
    double span = m_LookTimes[hi] - m_LookTimes[lo];
    t = span > 0 ? (float)((time - m_LookTimes[lo]) / span) : 0.f;
  }
  pVisuals->SetReplayWeatherLook(&m_Looks[lo * m_LookSize], &m_Looks[hi * m_LookSize], t);
  m_LookApplied = true;
}

// The weather menu's settings (VISUALS > Weather: rain, heavy rain, kept out
// of buildings, its zones loaded) as they were when it recorded, given while
// it plays; when it stops, the ones from before, unless they were changed
// meanwhile (the menu's then stay). The clouds and wind (VISUALS effects) and
// the game's own weather effects aren't these: the game's effects playing
// near Faith as it started recording are in the effects replay (see the
// hooks' TakeInPlayingEffects).
void FaithReplay::ApplyRecordedWeather(bool on)
{
  auto same = [](weather::State const& a, weather::State const& b) {
    return a.Rain == b.Rain && a.Heavy == b.Heavy && a.KeepOut == b.KeepOut && a.LoadZones == b.LoadZones;
  };
  if (on && m_HaveWeather && !m_WeatherApplied)
  {
    m_LiveWeather = weather::GetState();
    m_WeatherApplied = true;
    if (!same(m_LiveWeather, m_RecordedWeather))
    {
      weather::SetState(m_RecordedWeather);
      util::log::Write("Replay: the weather as recorded (rain %s, heavy rain %s)", m_RecordedWeather.Rain ? "on" : "off",
        m_RecordedWeather.Heavy ? "on" : "off");
    }
  }
  else if (!on && m_WeatherApplied)
  {
    m_WeatherApplied = false;
    if (same(weather::GetState(), m_RecordedWeather) && !same(m_LiveWeather, m_RecordedWeather))
      weather::SetState(m_LiveWeather);
  }
}

// What she wore at a replay time (the last change at or before it; before the
// first, the first), -1 when not known
int FaithReplay::OutfitAt(double time) const
{
  size_t count = (std::min)(m_OutfitTimes.size(), m_Outfits.size());
  if (count == 0)
    return -1;
  size_t at = 0;
  while (at + 1 < count && m_OutfitTimes[at + 1] <= time)
    ++at;
  return m_Outfits[at];
}

// The recorded time of day at a replay time: in a straight line between two
// samples the shorter way round midnight, but a jump (a mission or a story
// beat setting the clock, the tools' slider: more than 2 minutes between two
// samples a 30th of a second apart) is kept a jump; -1 when not known
float FaithReplay::TimeOfDayAt(double time) const
{
  const float Day = 86400.f, MaxGlide = 120.f;
  size_t count = (std::min)(m_Times.size(), m_Tods.size());
  if (count == 0)
    return -1.f;
  if (time <= m_Times[0] || count == 1)
    return m_Tods[0];
  if (time >= m_Times[count - 1])
    return m_Tods[count - 1];
  size_t lo = 0, hi = count - 1;
  while (hi - lo > 1)
  {
    size_t mid = (lo + hi) / 2;
    if (m_Times[mid] <= time) lo = mid; else hi = mid;
  }
  float a = m_Tods[lo], b = m_Tods[hi];
  if (a < 0.f || b < 0.f)
    return a >= 0.f ? a : b;
  float step = b - a;
  if (step > Day / 2) step -= Day;
  if (step < -Day / 2) step += Day;
  if (fabsf(step) > MaxGlide)
    return a;
  double span = m_Times[hi] - m_Times[lo];
  float t = a + (span > 0 ? (float)((time - m_Times[lo]) / span) : 0.f) * step;
  return fmodf(t + Day, Day);
}

// The recorded pose at a time (see SamplePose)
void FaithReplay::SampleInto(double time, float* bones)
{
  SamplePose(m_Times, m_Frames, m_Bones, time, bones);
}

bool FaithReplay::GetPlayedFaith(float* place, float& heading, bool& hasHeading)
{
  hasHeading = false;
  size_t count = m_Times.size();
  if (m_State != State_Playing || count < 2)
    return false;
  if (m_FaithPlaces.size() != count * 4)
  {
    // No soldier with the samples: bone 1 of the pose played (where she is), no heading
    if (!m_PlayRootOk)
      return false;
    memcpy(place, m_PlayRoot, sizeof(m_PlayRoot));
    return true;
  }
  // Between the two samples around the time played, as the pose is (see
  // SamplePose); before the first or after the last, that one
  size_t lo = 0, hi = 0;
  if (m_PlayTime >= m_Times[count - 1])
    lo = hi = count - 1;
  else if (m_PlayTime > m_Times[0])
  {
    hi = count - 1;
    while (hi - lo > 1)
    {
      size_t mid = (lo + hi) / 2;
      if (m_Times[mid] <= m_PlayTime) lo = mid; else hi = mid;
    }
  }
  float const* a = &m_FaithPlaces[lo * 4];
  float const* b = &m_FaithPlaces[hi * 4];
  if (!std::isfinite(a[0]) || !std::isfinite(b[0]))
  {
    if (!m_PlayRootOk)
      return false;
    memcpy(place, m_PlayRoot, sizeof(m_PlayRoot));
    return true;
  }
  double span = m_Times[hi] - m_Times[lo];
  float f = span > 0 ? (float)((m_PlayTime - m_Times[lo]) / span) : 0.f;
  f = f < 0.f ? 0.f : (f > 1.f ? 1.f : f);
  for (int c = 0; c < 3; ++c)
    place[c] = a[c] + (b[c] - a[c]) * f;
  if (std::isfinite(a[3]) && std::isfinite(b[3]))
  {
    heading = a[3] + XMScalarModAngle(b[3] - a[3]) * f;
    hasHeading = true;
  }
  return true;
}

// Whether a body found is a recorded one's still there (2026-11-03, the user:
// "the ksec was missing the baton": the SubProtector that came at 135.65 s had
// no weapon recorded, every other melee KrugerSec had theirs, and three
// weapons had gone at 70.96 s. A body was taken for known when any recorded
// one, gone or not, had its address; a new one made where a gone one's memory
// was, wasn't recorded. The game's reuse of that memory is inferred, not
// read.) A gone one's address doesn't count; reused: the time it went (-1 none).
bool FaithReplay::KnownBody(__int64 animatable, double& reused) const
{
  reused = -1.0;
  for (auto const& person : m_People)
    if (person->Animatable == animatable)
    {
      if (!person->Gone)
        return true;
      if (!person->Times.empty())
        reused = person->Times.back();
    }
  return false;
}

void FaithReplay::LogReusedBody(PersonTrack const& person, double reused) const
{
  static int s_logs = 0;
  if (reused >= 0.0 && s_logs++ < 40)
    util::log::Write("Replay:   %s 0x%I64X found with the body address 0x%I64X of a recorded one that went at %.2f s: recorded as "
      "another one", person.Label, person.Entity, person.Animatable, reused);
}

// The people within PeopleRadius of her who have a body with a pose slot,
// each once (a character has two entities with the same body), and the guns
// and drones (see ActorBodyOffset)
void FaithReplay::ScanPeople(__int64 soldier, float const* faith)
{
  if (!soldier)
    return;
  // (KrugerSec on their own; see MaxEnemies)
  size_t characters = 0, enemies = 0, actors = 0;
  for (auto const& person : m_People)
    if (person->BodyOffset != CharacterBodyOffset)
      ++actors;
    else if (IsEnemyTemplate(person->Template))
      ++enemies;
    else if (!person->Gone || person->Moved)
      ++characters;
  bool fromFaith = false;
  if (attach::FindClasses(soldier, fromFaith))
  {
    attach::View view{};
    view.Camera = XMFLOAT3(faith[0], faith[1], faith[2]);
    view.Look = XMFLOAT3(0.f, 0.f, 1.f);
    view.Skip = soldier;
    static std::vector<attach::Target> found;
    found.resize(32768);
    std::unique_ptr<attach::Scan> scan = std::make_unique<attach::Scan>();
    // The characters within radius of center (fromVehicle: around a vehicle)
    auto scanAround = [&](float const* center, float radius, bool fromVehicle)
    {
      view.Camera = XMFLOAT3(center[0], center[1], center[2]);
      int count = attach::ScanNearby(view, radius, found.data(), (int)found.size(), *scan);
      for (int i = 0; i < count && (characters < MaxPeople || enemies < MaxEnemies) && m_People.size() < MaxPeopleTracks; ++i)
      {
        attach::Target const& target = found[i];
        if (target.Agent >= 0 || target.Distance > radius || target.Entity == soldier ||
            attach::GroupOf(target) != attach::Group::People)
          continue;
        __int64 animatable = 0;
        PoseSlot slot{};
        if (!FindCharacterPose(target.Entity, animatable, slot) || IsFaithAnimatable(soldier, animatable))
          continue;
        double reused = -1.0;
        if (KnownBody(animatable, reused))
          continue;
        // Its spawn template now, for KrugerSec's own places (one not known
        // yet takes one of the others', or is looked at again next time)
        char spawnTemplate[sizeof(PersonTrack::Template)] = "";
        float root[RootWorldFloats];
        if (slot.World && slot.Bones > RootWorldBones && CopyPose(slot.World, root, RootWorldBones))
          util::hooks::SpawnTemplateNear(root + 1 * FloatsPerBone, TemplateReach, spawnTemplate, sizeof(spawnTemplate));
        bool enemy = IsEnemyTemplate(spawnTemplate);
        if (enemy ? enemies >= MaxEnemies : characters >= MaxPeople)
          continue;
        std::unique_ptr<PersonTrack> person = std::make_unique<PersonTrack>();
        person->Entity = target.Entity;
        person->VTable = target.VTable;
        person->Animatable = animatable;
        person->Bones = slot.Bones;
        person->Data = ReadEntityData(target.Entity);
        strncpy_s(person->Label, attach::ClassName(target.Class), _TRUNCATE);
        if (spawnTemplate[0])
        {
          strcpy_s(person->Template, spawnTemplate);
          util::log::Write("Replay:   %s 0x%I64X: made from %s", person->Label, person->Entity, person->Template);
        }
        if (fromVehicle)
          util::log::Write("Replay: %s 0x%I64X found near a moving vehicle (%.0f m from it)", person->Label, target.Entity,
            target.Distance);
        LogReusedBody(*person, reused);
        m_People.push_back(std::move(person));
        ++(enemy ? enemies : characters);
      }
    };
    scanAround(faith, PeopleRadius, false);
    float places[MaxVehiclePeopleScans * 3];
    int vehicles = m_Vehicles.MovingPlaces(places, MaxVehiclePeopleScans);
    for (int v = 0; v < vehicles; ++v)
      if (Distance3(places + v * 3, faith) <= VehiclePeopleReach)
        scanAround(places + v * 3, VehiclePeopleRadius, true);
  }
  static ActorFound found[256];
  int withoutBody = 0;
  __int64 firstWithoutBody = 0;
  int count = FindActors(faith, PeopleRadius, DroneFindRadius, found, (int)_countof(found), withoutBody, firstWithoutBody);
  if (withoutBody && !m_ActorsWithoutBodyLogged)
  {
    m_ActorsWithoutBodyLogged = true;
    util::log::Write("Replay: %d guns or drones near Faith have no body at + 0x%X (the first 0x%I64X): not recorded", withoutBody,
      ActorBodyOffset, firstWithoutBody);
  }
  for (int i = 0; i < count && actors < MaxActors; ++i)
  {
    __int64 animatable = 0;
    PoseSlot slot{};
    if (!FindCharacterPose(found[i].Entity, animatable, slot, ActorBodyOffset) || animatable != found[i].Animatable)
      continue;
    double reused = -1.0;
    if (KnownBody(animatable, reused))
      continue;
    std::unique_ptr<PersonTrack> person = std::make_unique<PersonTrack>();
    person->Entity = found[i].Entity;
    person->VTable = found[i].VTable;
    person->Animatable = animatable;
    person->Bones = slot.Bones;
    person->Data = ReadEntityData(found[i].Entity);
    person->BodyOffset = ActorBodyOffset;
    strncpy_s(person->Label, ActorClasses[found[i].Class].Name, _TRUNCATE);
    LogReusedBody(*person, reused);
    m_People.push_back(std::move(person));
    ++actors;
  }
}

// A mission's people again (2026-10-26). A recording made in a mission and
// played after the mission was started again from the game's own menu found
// none of its people: they're matched by entity, and the restarted mission
// has new ones (the old mission's were taken out with it, see
// Research_Replay_Mission_World_2026-10-26.md). A recorded person that isn't in
// the level any more takes over a live one of its kind (the same class, as
// many bones, a character or a gun / drone) whose bone 1 is within
// RefindRadius of where its own was at that moment of the recording (its world
// pose's bones 0 - 3, kept with each frame), the nearest of them, while the
// replay is within its recorded time; once, and only one not taken yet (not
// hidden as an other): from then on it's that person's, as if found while
// recording. Its own state for this playback starts afresh.
namespace
{
  const float RefindRadius = 10.f;
}

bool FaithReplay::ClaimForGone(__int64 entity, __int64 vtable, __int64 animatable, int bones, float const* world, int bodyOffset,
  double time)
{
  PersonTrack* best = nullptr;
  float bestDistance = RefindRadius;
  for (auto& person : m_People)
  {
    if (person->Times.empty() || !person->RootWorldOk || person->VTable != vtable || person->BodyOffset != bodyOffset ||
        person->Bones != bones || time < person->Times.front() - 1.0 || time > person->Times.back() + 1.0 ||
        person->RootWorld.size() < person->Times.size() * RootWorldFloats ||
        StillCharacter(person->Entity, person->VTable, person->Animatable, person->BodyOffset))
      continue;
    size_t frame = std::upper_bound(person->Times.begin(), person->Times.end(), time) - person->Times.begin();
    frame = frame ? frame - 1 : 0;
    float const* recorded = &person->RootWorld[frame * RootWorldFloats + 1 * FloatsPerBone];
    float distance = Distance3(recorded, world + 1 * FloatsPerBone);
    if (distance < bestDistance)
    {
      bestDistance = distance;
      best = person.get();
    }
  }
  if (!best)
    return false;
  util::log::Write("Replay: %s 0x%I64X wasn't in the level any more: the %s 0x%I64X %.1f m from where it was at %.1f s plays it now",
    best->Label, best->Entity, best->Label, entity, bestDistance, time);
  RetargetPerson(*best, entity, animatable);
  ++m_Refound;
  return true;
}

// A recorded person's playing taken over by another live entity: its own
// state for this playback starts afresh
void FaithReplay::RetargetPerson(PersonTrack& p, __int64 entity, __int64 animatable)
{
  p.Entity = entity;
  p.Animatable = animatable;
  p.Registered = p.BoxRegistered = false;
  p.Taken = p.Frozen = p.FrozeWhilePlaying = false;
  p.Before.clear();
  p.RawBefore.clear();
  p.RawRestoreLeft = p.DrawWait = 0;
  p.DrawTried = p.Drawing = p.DrawFailLogged = false;
  p.HeldBody = p.ForcedBody = p.Server = 0;
  p.ForceCooldown = 0;
  p.GunHeld = p.GunPlaced = false;
  p.EntityOffsetOk = p.EntityMoved = false;
  p.CullBoxSet = p.CullBoxLogged = p.BoxLogged = false;
  p.ActorBeforeOk = p.ActorHeld = false;
  p.ActorFrames = 0;
  p.StateLogs = 0;
  p.DeadLogged = false;
  p.StuckFrames = 0;
  p.Stuck = false;
}

// Recorded people a mission took with it (2026-10-27: played after the
// mission was over, all 29 gone; spawning new ones the game's way was judged
// unsafe, MEC_Reverse_Notes "Runner ghost spawn vs KSec"). The replay already
// draws a person's body wherever it was recorded while the live one stands
// elsewhere (taken over, its client entity and culling box following the
// drawn body). So a recorded person still gone after ClaimForGone is played
// by a live one of its kind from anywhere within BorrowRadius of the replayed
// Faith: the same class and number of bones, a character or a gun / drone,
// one the same entity data (+ 0x30: the same kind of KrugerSec) first, then
// the nearest to where the recorded one was; never Faith, one in the
// recording, or an other already hidden; each once. While it plays, that live
// one is seen only where the replay draws it; when it stops it's given back
// as any recorded person is. Its look is the live one's (another KrugerSec of
// the same kind looks the same; with no same-data one left, one of the same
// class and skeleton is taken). No entity is made.
//
// Stand-ins (2026-10-27, stage 2; the 01:13 playback borrowed 3 of 18). A
// recorded character has its spawner's template from the recording (see
// RecordPeople), and the live ones theirs from the hooks' notes of the
// spawners' characters. With REPLAY > "Stand-ins" on, one with a template
// takes only a live one of the same data or template; with none, it asks the
// game for a stand-in of that template (the hooks' AskStandIn: Spawn sent to a
// free-roam spawner of it, or of its kind) and waits for it; once made, the
// live one nearest that stand-in's server character (within StandInReach)
// plays it, wherever it stands, and is left alone by ScanOthers meanwhile.
// When no stand-in can be made it takes any of its class, as before. The
// stand-ins go out of the level when the playback stops (StopPlaying).
namespace
{
  const float BorrowRadius = 3000.f;
  // A live character's spawn template: see TemplateReach; a stand-in's live
  // one: the one nearest its server character within StandInReach
  const float StandInReach = 3.f;
  const int StandInMissesLogged = 5;   // borrow passes (about a second apart) before a made one not found is logged
  // The playback's start is held up to StandInWaitMs for the stand-ins of the
  // people there then (02:17: the first five took 5 s to be played, their
  // client copies coming after the spawn), scanning every StandInWaitScanMs;
  // the ones still to come are asked StandInLeadSeconds ahead
  const unsigned long long StandInWaitMs = 10000;
  const unsigned long long StandInWaitScanMs = 500;
  const double StandInLeadSeconds = 20.0;
}

void FaithReplay::StartStandInWait()
{
  m_StandInWaitUntil = StandInsUsable() ? GetTickCount64() + StandInWaitMs : 0;
  m_StandInsPending = 0;
  m_WaitScanAt = 0;
}

bool FaithReplay::StandInsUsable() const
{
  return m_StandInsOn && m_PeopleOn && util::hooks::StandInsKnown() && faith_outfit::IsAvailable();
}

void FaithReplay::StandInPlaces(std::vector<float>& places) const
{
  places.clear();
  for (auto const& person : m_People)
  {
    float place[3];
    if (person->StandIn > 0 && util::hooks::StandInState(person->StandIn, place) == util::hooks::StandInReady)
      places.insert(places.end(), place, place + 3);
  }
}

void FaithReplay::BorrowForGone(__int64 soldier, float const* center, double time)
{
  struct Candidate
  {
    __int64 Entity, VTable, Animatable, Data;
    int Bones, BodyOffset;
    float Place[3];
    bool Used;
    char Template[128];
  };
  // Who's gone now and within its recorded time; not a runner projection (a
  // mission's red Runner Vision guide): one isn't played by another ghost,
  // nor by a stand-in from the city's replay markers (see the hooks'
  // IsNoStandInTemplate)
  std::vector<PersonTrack*> wanting;
  for (auto& person : m_People)
    if (!person->Times.empty() && person->RootWorldOk && time >= person->Times.front() - 1.0 &&
        time <= person->Times.back() + 1.0 && person->RootWorld.size() >= person->Times.size() * RootWorldFloats &&
        !util::hooks::IsNoStandInTemplate(person->Template) && !DrawnByGameDraws(*person) &&
        !StillCharacter(person->Entity, person->VTable, person->Animatable, person->BodyOffset))
      wanting.push_back(person.get());
  if (wanting.empty() || !soldier || !center)
    return;
  auto known = [this](__int64 animatable)
  {
    for (auto const& person : m_People)
      if (person->Animatable == animatable)
        return true;
    for (auto const& other : m_Others)
      if (other->Animatable == animatable)
        return true;
    return false;
  };
  bool standIns = StandInsUsable();
  std::vector<Candidate> candidates;
  auto add = [&](__int64 entity, __int64 vtable, __int64 animatable, PoseSlot const& slot, int bodyOffset)
  {
    float world[RootWorldFloats];
    if (!slot.World || slot.Bones <= RootWorldBones || !CopyPose(slot.World, world, RootWorldBones))
      return;
    Candidate c{ entity, vtable, animatable, ReadEntityData(entity), slot.Bones, bodyOffset,
      { world[FloatsPerBone], world[FloatsPerBone + 1], world[FloatsPerBone + 2] }, false, {} };
    if (standIns && bodyOffset == CharacterBodyOffset)
      util::hooks::SpawnTemplateNear(c.Place, TemplateReach, c.Template, sizeof(c.Template));
    candidates.push_back(c);
  };
  bool fromFaith = false;
  if (attach::FindClasses(soldier, fromFaith))
  {
    attach::View view{};
    view.Camera = XMFLOAT3(center[0], center[1], center[2]);
    view.Look = XMFLOAT3(0.f, 0.f, 1.f);
    view.Skip = soldier;
    static std::vector<attach::Target> found;
    found.resize(32768);
    std::unique_ptr<attach::Scan> scan = std::make_unique<attach::Scan>();
    int count = attach::ScanNearby(view, BorrowRadius, found.data(), (int)found.size(), *scan);
    for (int i = 0; i < count; ++i)
    {
      attach::Target const& target = found[i];
      if (target.Agent >= 0 || target.Distance > BorrowRadius || target.Entity == soldier ||
          attach::GroupOf(target) != attach::Group::People)
        continue;
      __int64 animatable = 0;
      PoseSlot slot{};
      if (!FindCharacterPose(target.Entity, animatable, slot) || IsFaithAnimatable(soldier, animatable) || known(animatable))
        continue;
      add(target.Entity, target.VTable, animatable, slot, CharacterBodyOffset);
    }
  }
  static ActorFound actors[256];
  int withoutBody = 0;
  __int64 firstWithoutBody = 0;
  int count = FindActors(center, BorrowRadius, BorrowRadius, actors, (int)_countof(actors), withoutBody, firstWithoutBody);
  for (int i = 0; i < count; ++i)
  {
    __int64 animatable = 0;
    PoseSlot slot{};
    if (!FindCharacterPose(actors[i].Entity, animatable, slot, ActorBodyOffset) || animatable != actors[i].Animatable ||
        known(animatable))
      continue;
    add(actors[i].Entity, actors[i].VTable, animatable, slot, ActorBodyOffset);
  }
  // Their stand-ins: where the made ones are; a person whose stand-in is on
  // its way waits for it
  std::vector<int> states(wanting.size(), util::hooks::StandInNone);
  std::vector<float> standInPlaces(wanting.size() * 3, 0.f);
  for (size_t w = 0; w < wanting.size(); ++w)
  {
    PersonTrack* person = wanting[w];
    if (person->StandIn <= 0)
      continue;
    states[w] = util::hooks::StandInState(person->StandIn, &standInPlaces[w * 3]);
    if (states[w] == util::hooks::StandInNone)
    {
      util::log::Write("Replay: %s 0x%I64X: no stand-in was made (or it's gone): a live one of its own data or template plays it "
        "if one's found, else it isn't shown", person->Label, person->Entity);
      person->StandIn = -1;
    }
  }
  auto sameShape = [](Candidate const& c, PersonTrack const& person) {
    return c.VTable == person.VTable && c.Bones == person.Bones && c.BodyOffset == person.BodyOffset;
  };
  // A made stand-in's live one: its person's only (its gun may play a recorded gun)
  auto reserved = [&](Candidate const& c) {
    if (c.BodyOffset != CharacterBodyOffset)
      return false;
    for (size_t w = 0; w < wanting.size(); ++w)
      if (states[w] == util::hooks::StandInReady && Distance3(&standInPlaces[w * 3], c.Place) <= StandInReach)
        return true;
    return false;
  };
  for (size_t w = 0; w < wanting.size(); ++w)
  {
    if (states[w] != util::hooks::StandInReady)
      continue;
    PersonTrack* person = wanting[w];
    float const* standIn = &standInPlaces[w * 3];
    Candidate* best = nullptr;
    float bestDistance = StandInReach;
    for (Candidate& c : candidates)
    {
      float distance = Distance3(standIn, c.Place);
      if (!c.Used && sameShape(c, *person) && distance <= bestDistance)
      {
        best = &c;
        bestDistance = distance;
      }
    }
    if (!best)
    {
      if (++person->StandInMisses == StandInMissesLogged)
        util::log::Write("Replay: %s 0x%I64X: its stand-in at %.1f %.1f %.1f isn't among the live ones within %.0f m of the "
          "replayed Faith yet", person->Label, person->Entity, standIn[0], standIn[1], standIn[2], BorrowRadius);
      continue;
    }
    best->Used = true;
    util::log::Write("Replay: %s 0x%I64X isn't in the level any more: its stand-in, the live %s 0x%I64X (%.1f m from the "
      "stand-in's server character), plays it now", person->Label, person->Entity, person->Label, best->Entity, bestDistance);
    RetargetPerson(*person, best->Entity, best->Animatable);
    person->Borrowed = true;
    ++m_StandInsPlaying;
  }
  // The rest: each takes the nearest live one with the same body, the same
  // data or the same spawn template; when there's none, a stand-in of its own
  // template is asked for while a spawner of it has room; else it isn't shown.
  // Not one of its folder and kind, nor any of its class (2026-10-27 16:39:
  // the two runners, SpawnTemplate_AIMale_Runner_02, were played by city men,
  // HighCaste02 and Black_November_02, 591 and 905 m away; the user: the
  // replay is a 1:1 copy, so not by another model). The same data first, for
  // everyone, then the template (2026-10-27 03:48: the recorded Icarus lost the
  // free-roam one to a runner earlier in the list).
  std::vector<bool> served(wanting.size(), false);
  for (int pass = 0; pass < 2; ++pass)
  for (size_t w = 0; w < wanting.size(); ++w)
  {
    if (states[w] != util::hooks::StandInNone || served[w])
      continue;   // played by its stand-in now, or waiting for it, or served in the first pass
    PersonTrack* person = wanting[w];
    int possible = standIns && person->Template[0] && person->StandIn == 0 && person->BodyOffset == CharacterBodyOffset ?
      util::hooks::StandInPossible(person->Template) : 0;
    int needed = pass == 0 ? 3 : 2;   // the least rank it takes now: the same data, then the same template
    size_t frame = std::upper_bound(person->Times.begin(), person->Times.end(), time) - person->Times.begin();
    frame = frame ? frame - 1 : 0;
    float const* recorded = &person->RootWorld[frame * RootWorldFloats + 1 * FloatsPerBone];
    Candidate* best = nullptr;
    int bestRank = -1;   // 3: the same data, 2: the same template (no other is taken)
    float bestDistance = 1e30f;
    for (Candidate& c : candidates)
    {
      if (c.Used || !sameShape(c, *person) || reserved(c))
        continue;
      int rank = person->Data && c.Data == person->Data ? 3 : util::hooks::SpawnTemplatesMatch(person->Template, c.Template);
      if (rank < needed)
        continue;
      float distance = Distance3(recorded, c.Place);
      if (rank > bestRank || (rank == bestRank && distance < bestDistance))
      {
        best = &c;
        bestRank = rank;
        bestDistance = distance;
      }
    }
    if (best)
    {
      best->Used = true;
      char const* slash = strrchr(best->Template, '/');
      util::log::Write("Replay: %s 0x%I64X isn't in the level any more: the live %s 0x%I64X (%s, %.0f m away; made from %s) plays it now",
        person->Label, person->Entity, person->Label, best->Entity, bestRank == 3 ? "the same data" : "the same spawn template",
        bestDistance, best->Template[0] ? (slash ? slash + 1 : best->Template) : "a template not known");
      RetargetPerson(*person, best->Entity, best->Animatable);
      person->Borrowed = true;
      ++m_Borrowed;
      served[w] = true;
      continue;
    }
    if (pass == 0)
      continue;
    if (possible == 0)
    {
      // Nothing for it (2026-10-27 14:22: the runners and Icarus were silently left out)
      if (!person->NoneLogged)
      {
        person->NoneLogged = true;
        char const* slash = strrchr(person->Template, '/');
        util::log::Write("Replay: %s 0x%I64X (%s) isn't in the level any more, and there's no live one of its own data or template "
          "within %.0f m of the replayed Faith and no spawner of its template with room: it isn't shown (not by another "
          "model)", person->Label, person->Entity, person->Template[0] ? (slash ? slash + 1 : person->Template) :
          "its template not known", BorrowRadius);
      }
      continue;
    }
    // None of its template left: the game is asked for one
    AskStandInFor(*person, possible);
  }
}

// A stand-in for a gone person (possible: the hooks' StandInPossible)
void FaithReplay::AskStandInFor(PersonTrack& person, int possible)
{
  char const* slash = strrchr(person.Template, '/');
  int id = util::hooks::AskStandIn(person.Template);
  person.StandIn = id > 0 ? id : -1;
  person.StandInMisses = 0;
  if (id > 0)
  {
    ++m_StandIns;
    util::log::Write("Replay: %s 0x%I64X isn't in the level any more and no live %s is: stand-in #%d asked of the game's "
      "spawners (%s)", person.Label, person.Entity, slash ? slash + 1 : person.Template, id,
      "one of its template");
  }
  else
    util::log::Write("Replay: %s 0x%I64X isn't in the level any more and no live %s is; no stand-in can be asked now (%d asked "
      "already): a live one of its own data or template plays it if one's found, else it isn't shown", person.Label,
      person.Entity, slash ? slash + 1 : person.Template, m_StandIns);
}

// The gone ones still to come in the recording (from StandInLeadSeconds on):
// their stand-ins asked now, when a spawner of their own template has room,
// so they're there by then
void FaithReplay::AskStandInsAhead(double time)
{
  if (!StandInsUsable())
    return;
  for (auto& person : m_People)
    if (!person->Times.empty() && person->StandIn == 0 && person->Template[0] && person->BodyOffset == CharacterBodyOffset &&
        !DrawnByGameDraws(*person) &&
        person->RootWorldOk && time < person->Times.front() - 1.0 && time >= person->Times.front() - StandInLeadSeconds &&
        !StillCharacter(person->Entity, person->VTable, person->Animatable, person->BodyOffset) &&
        util::hooks::StandInPossible(person->Template) == 2)
      AskStandInFor(*person, 2);
}

// People drawn with the game's own draws of them (2026-10-27, stage 2;
// Research_Replay_Mission_World_2026-10-26.md sections 20 - 22). A skinned
// draw's instance holds its bones' 3x4 matrices (rows: rotation, translation
// last; absolute world) after a header (12 float4s in the draw log's
// snapshot); they're its skeleton's matrices, the world pose times a fixed
// transform per bone (see ReadUnbind), for the bones that mesh uses. So:
// - While it records, ReplayGameDraws keeps the game's skinned draws of a
//   frame every SkinCaptureEvery frames and hands them over (read back) with
//   the recording's time then. Each is matched here to the recorded person
//   whose matrices, made from the recording (the world pose from bones 0 - 3
//   as recorded and the others from their parent, as DrawBody does, times
//   its fixed transforms read while it was recorded), hold every one of its
//   bones within MatchMetres and MatchRotation, at a time within
//   [MatchDtMin, MatchDtMax] of that one (the frame drawn is the game's a
//   little earlier). Which skeleton bone each of its bones is comes with it.
// - A person keeps the draws of one frame: the most detailed set seen (the
//   most indices; the game draws a far one with fewer).
// - While it plays, a person with those that the game doesn't have any more
//   is drawn with them (ReplayGameDraws, at the end of the G-buffer pass),
//   its matrices made from the recording at the replay's time, its header
//   rows as kept. Such a person isn't borrowed nor given a stand-in.
// - Guns, drones and skinned doors (stage 2b): see AdoptBound.
namespace
{
  const float MatchMetres = 0.04f;
  const float MatchRotation = 0.05f;      // the largest difference of a rotation's 9 numbers
  const float MatchNear = 3.f;            // a person's place from the draw's first bone, to be tried
  const double MatchDtMin = -0.20, MatchDtMax = 0.05, MatchDtStep = 1.0 / 120.0;
  const int MaxHeaderRows = 20;
  const int MaxGameDrawsPerPerson = 24;
  const int MaxAdoptLogs = 40;

  // A recorded person's world pose at a time, every bone: bones 0 - 3 as
  // recorded in world, the others from their parent in order (those without
  // one: their local pose as it is), as DrawBody makes it
  bool RecordedWorld(FaithReplay::PersonTrack const& person, Hierarchy const& hierarchy, double time, std::vector<float>& local,
    std::vector<float>& world);

  // Its matrices (12 floats a bone): world times the fixed transforms
  void MatricesOf(float const* world, float const* unbind, int bones, float* out)
  {
    for (int i = 0; i < bones; ++i)
    {
      float const* w = world + i * FloatsPerBone;
      float const* k = unbind + i * MatrixFloats;
      float r[3][3];
      QuatMatrix(w + 4, r);
      float* c = out + i * MatrixFloats;
      for (int row = 0; row < 3; ++row)
      {
        for (int col = 0; col < 3; ++col)
          c[row * 4 + col] = r[row][0] * k[0 * 4 + col] + r[row][1] * k[1 * 4 + col] + r[row][2] * k[2 * 4 + col];
        c[row * 4 + 3] = r[row][0] * k[3] + r[row][1] * k[7] + r[row][2] * k[11] + w[row];
      }
    }
  }

  // How far two 3x4 matrices are: their places' distance, and the largest
  // difference of their rotations' numbers
  void MatrixError(float const* a, float const* b, float& metres, float& rotation)
  {
    float dx = a[3] - b[3], dy = a[7] - b[7], dz = a[11] - b[11];
    metres = sqrtf(dx * dx + dy * dy + dz * dz);
    rotation = 0.f;
    for (int row = 0; row < 3; ++row)
      for (int col = 0; col < 3; ++col)
        rotation = (std::max)(rotation, fabsf(a[row * 4 + col] - b[row * 4 + col]));
  }

  // A draw's bones (from header on) are all transforms: their 3x3 parts'
  // columns at right angles, not flat, their places numbers
  bool AllTransforms(float const* bones, int count)
  {
    for (int b = 0; b < count; ++b)
    {
      float const* m = bones + b * 12;
      float c[3][3];
      for (int col = 0; col < 3; ++col)
        for (int row = 0; row < 3; ++row)
          c[col][row] = m[row * 4 + col];
      for (int col = 0; col < 3; ++col)
      {
        float n = sqrtf(c[col][0] * c[col][0] + c[col][1] * c[col][1] + c[col][2] * c[col][2]);
        if (!(n > 0.3f && n < 3.f))
          return false;
      }
      for (int a = 0; a < 3; ++a)
        for (int d = a + 1; d < 3; ++d)
          if (fabsf(c[a][0] * c[d][0] + c[a][1] * c[d][1] + c[a][2] * c[d][2]) > 0.05f)
            return false;
      for (int row = 0; row < 3; ++row)
        if (!std::isfinite(m[row * 4 + 3]) || fabsf(m[row * 4 + 3]) > 50000.f)
          return false;
    }
    return true;
  }

  bool RecordedWorld(FaithReplay::PersonTrack const& person, Hierarchy const& hierarchy, double time, std::vector<float>& local,
    std::vector<float>& world)
  {
    size_t floats = (size_t)person.Bones * FloatsPerBone;
    local.resize(floats);
    world.resize(floats);
    SamplePacked(person.Times, person.Recorded, person.Bones, time, local.data());
    SamplePose(person.Times, person.RootWorld, RootWorldBones, time, world.data());
    for (int i : hierarchy.Order)
    {
      float* w = &world[(size_t)i * FloatsPerBone];
      float const* l = &local[(size_t)i * FloatsPerBone];
      int p = hierarchy.Parent[i];
      if (p < 0)
      {
        memcpy(w, l, FloatsPerBone * sizeof(float));
        continue;
      }
      float const* pw = &world[(size_t)p * FloatsPerBone];
      QuatRotate(pw + 4, l, w);
      w[0] += pw[0];
      w[1] += pw[1];
      w[2] += pw[2];
      w[3] = 1.f;
      QuatMul(pw + 4, l + 4, w + 4);
    }
    return true;
  }
}

// A recorded person the game's draws can be posed for from its skeleton: its
// fixed transforms read, its skeleton's hierarchy known, its world pose recorded
static Hierarchy const* GameDrawable(FaithReplay::PersonTrack const& person)
{
  if (person.Times.empty() || !person.RootWorldOk || person.RootWorld.size() != person.Times.size() * RootWorldFloats ||
      person.Unbind.size() != (size_t)person.Bones * MatrixFloats || !person.Manager || person.Bones <= RootWorldBones)
    return nullptr;
  Hierarchy const* hierarchy = FindHierarchy(person.Manager, person.Bones);
  return hierarchy && hierarchy->Ready && !hierarchy->Order.empty() ? hierarchy : nullptr;
}

// A gun's or drone's whole world pose recorded with its frames (stage 2b, AppendWorld)
static bool HasRecordedWorld(FaithReplay::PersonTrack const& person)
{
  return person.BodyOffset == ActorBodyOffset && person.WorldOk && !person.Times.empty() &&
    person.WorldFrames.size() == person.Times.size() * (size_t)person.Bones * FloatsPerBone;
}

// A recorded character's matrices at a time, made from the recording
// (RecordedWorld times its fixed transforms); false if they can't be
static bool PersonMatricesAt(FaithReplay::PersonTrack const& person, double time, std::vector<float>& out, std::vector<float>& local,
  std::vector<float>& world)
{
  Hierarchy const* hierarchy = GameDrawable(person);
  if (!hierarchy || !RecordedWorld(person, *hierarchy, time, local, world))
    return false;
  out.resize((size_t)person.Bones * MatrixFloats);
  MatricesOf(world.data(), person.Unbind.data(), person.Bones, out.data());
  return true;
}

// Where a recorded person is at a time (bone 1 of its world pose)
static bool PersonPlaceAt(FaithReplay::PersonTrack const& person, double time, float* place)
{
  if (person.RootWorldOk && person.RootWorld.size() == person.Times.size() * RootWorldFloats && !person.Times.empty())
  {
    float root[RootWorldFloats];
    SamplePose(person.Times, person.RootWorld, RootWorldBones, time, root);
    memcpy(place, root + 1 * FloatsPerBone, 3 * sizeof(float));
    return true;
  }
  if (HasRecordedWorld(person) && person.Bones > 1)
  {
    size_t prev = 0, next = 0;
    float f = 0.f;
    if (person.Times.size() > 1)
      SpanAt(person.Times, person.Times.size(), time, prev, next, f);
    memcpy(place, &person.WorldFrames[prev * (size_t)person.Bones * FloatsPerBone + 1 * FloatsPerBone], 3 * sizeof(float));
    return true;
  }
  return false;
}

namespace
{
  // A model's transform (the game's 4 rows: right, up, forward, place) as a
  // 3x4 matrix (rows x y z, the place last) the way a draw's bones are
  void TransformRows(float const* m, float* a)
  {
    for (int r = 0; r < 3; ++r)
    {
      a[r * 4 + 0] = m[0 * 4 + r];
      a[r * 4 + 1] = m[1 * 4 + r];
      a[r * 4 + 2] = m[2 * 4 + r];
      a[r * 4 + 3] = m[3 * 4 + r];
    }
  }

  // 3x4 matrices: a * b (the fourth row 0 0 0 1), and the inverse
  void Multiply3x4(float const* a, float const* b, float* out)
  {
    for (int r = 0; r < 3; ++r)
    {
      for (int c = 0; c < 4; ++c)
        out[r * 4 + c] = a[r * 4 + 0] * b[0 * 4 + c] + a[r * 4 + 1] * b[1 * 4 + c] + a[r * 4 + 2] * b[2 * 4 + c];
      out[r * 4 + 3] += a[r * 4 + 3];
    }
  }

  bool Inverse3x4(float const* a, float* out)
  {
    float const m00 = a[0], m01 = a[1], m02 = a[2], m10 = a[4], m11 = a[5], m12 = a[6], m20 = a[8], m21 = a[9], m22 = a[10];
    float c00 = m11 * m22 - m12 * m21, c01 = m12 * m20 - m10 * m22, c02 = m10 * m21 - m11 * m20;
    float det = m00 * c00 + m01 * c01 + m02 * c02;
    if (!(fabsf(det) > 1e-8f) || !std::isfinite(det))
      return false;
    float inv = 1.f / det;
    float r[9] = { c00 * inv, (m02 * m21 - m01 * m22) * inv, (m01 * m12 - m02 * m11) * inv,
                   c01 * inv, (m00 * m22 - m02 * m20) * inv, (m02 * m10 - m00 * m12) * inv,
                   c02 * inv, (m01 * m20 - m00 * m21) * inv, (m00 * m11 - m01 * m10) * inv };
    for (int row = 0; row < 3; ++row)
    {
      out[row * 4 + 0] = r[row * 3 + 0];
      out[row * 4 + 1] = r[row * 3 + 1];
      out[row * 4 + 2] = r[row * 3 + 2];
      out[row * 4 + 3] = -(r[row * 3 + 0] * a[3] + r[row * 3 + 1] * a[7] + r[row * 3 + 2] * a[11]);
    }
    return true;
  }

  // Guns, drones and doors (stage 2b, see AdoptBound)
  // A gun's or baton's mesh: the game's weapon meshes by name (cpf_shotgun,
  // cpf_baton, ...: 2026-10-27 19:4x the asset log named them; the MeshSet
  // MEC_Reverse_Notes had at a gun entity + 0x258 wasn't one for any of 13)
  char const* const WeaponMeshPrefix = "weapons/";
  const double WeaponScanEvery = 2.0;      // the recording's seconds
  const int MaxVbTries = 10;
  const double VbTryEvery = 1.0;           // the recording's seconds
  const double MinPairSeconds = 0.15;      // two draws of a part fitted together at least this far apart
  const float MinMotionMetres = 0.05f;     // the owner moved this far between them (or turned MinMotionRotation)
  const float MinMotionRotation = 0.05f;
  const int MaxSkinPool = 128;
  const double SkinPoolSeconds = 5.0;
  const int MaxMisfits = 3;                // sets of an owner let go for not fitting it before it's given up on
  const int MaxBoundLogs = 60;

  // A world pose's bone as a 3x4 matrix (rows: rotation, its place last)
  void PoseRows(float const* w, float* a)
  {
    float r[3][3];
    QuatMatrix(w + 4, r);
    for (int row = 0; row < 3; ++row)
    {
      a[row * 4 + 0] = r[row][0];
      a[row * 4 + 1] = r[row][1];
      a[row * 4 + 2] = r[row][2];
      a[row * 4 + 3] = w[row];
    }
  }

  // A skinned draw's header (float4 rows before its bones) and its bones:
  // 12 rows as the draw log found, else the fewest that leave only
  // transforms; -1 if none do
  int SkinHeader(replay_gamedraws::SkinCandidate const& c, int& count)
  {
    for (int pass = 0; pass < 2; ++pass)
      for (int header = pass == 0 ? 12 : 0; header <= (pass == 0 ? 12 : MaxHeaderRows); ++header)
      {
        if ((pass == 1 && header == 12) || (int)c.Stride < header + 3 || ((int)c.Stride - header) % 3)
          continue;
        count = ((int)c.Stride - header) / 3;
        if (AllTransforms(c.Instance.data() + (size_t)header * 4, count))
          return header;
      }
    return -1;
  }

  // The same with the game's mesh's list of its bones when it has one
  // (2026-11-03, the user: "Hunter drone is missing once again"; see
  // replay_shapes::SectionBones): the header is the rows before them, and
  // listed has the skeleton's bone for each (empty when not read)
  int ListedSkinHeader(replay_gamedraws::SkinCandidate const& c, int& count, std::vector<int>& listed)
  {
    if (replay_shapes::SectionBones(c.Vb, c.StartIndex, c.IndexCount, listed) && (int)c.Stride >= (int)listed.size() * 3 &&
        (int)c.Stride - (int)listed.size() * 3 <= MaxHeaderRows)
    {
      count = (int)listed.size();
      int header = (int)c.Stride - count * 3;
      if (AllTransforms(c.Instance.data() + (size_t)header * 4, count))
        return header;
    }
    listed.clear();
    return SkinHeader(c, count);
  }

  bool SameDraw(replay_gamedraws::SkinCandidate const& c, __int64 vb, unsigned startIndex, unsigned indexCount)
  {
    return c.Vb == vb && c.StartIndex == startIndex && c.IndexCount == indexCount;
  }

  // The times tried round a draw's (the frame drawn is the game's a little
  // earlier, see MatchDtMin): the preferred one first, then the others by how
  // far from it they are (an even fit goes to the nearer)
  std::vector<double> DtOrder(double preferred)
  {
    std::vector<double> dts;
    for (double dt = MatchDtMin; dt <= MatchDtMax + 1e-9; dt += MatchDtStep)
      dts.push_back(dt);
    std::stable_sort(dts.begin(), dts.end(), [preferred](double a, double b) { return fabs(a - preferred) < fabs(b - preferred); });
    return dts;
  }

  // An owner's bones (rows, 12 floats each) moved between two times: any of
  // them by MinMotionMetres, or turned by MinMotionRotation
  bool Moved(std::vector<float> const& a, std::vector<float> const& b)
  {
    for (size_t j = 0; j + 12 <= a.size() && j + 12 <= b.size(); j += 12)
    {
      float metres, rotation;
      MatrixError(&a[j], &b[j], metres, rotation);
      if (metres >= MinMotionMetres || rotation >= MinMotionRotation)
        return true;
    }
    return false;
  }

  // Every bone of a part (count of them) moved between two draws of it: by
  // MinMotionMetres, or turned by MinMotionRotation (a drone's draw is taken
  // only then: a part that stood still fits any recorded bone that did)
  bool AllMoved(float const* sa, float const* sb, int count)
  {
    for (int i = 0; i < count; ++i)
    {
      float metres, rotation;
      MatrixError(sa + (size_t)i * 12, sb + (size_t)i * 12, metres, rotation);
      if (metres < MinMotionMetres && rotation < MinMotionRotation)
        return false;
    }
    return count > 0;
  }

  // Any bone of a part moved between two draws of it (a part whose bones the
  // game's mesh lists: its hunter drone's body has the skeleton's bone 2,
  // which the gun and lights don't, and never was taken with AllMoved)
  bool AnyMoved(float const* sa, float const* sb, int count)
  {
    for (int i = 0; i < count; ++i)
    {
      float metres, rotation;
      MatrixError(sa + (size_t)i * 12, sb + (size_t)i * 12, metres, rotation);
      if (metres >= MinMotionMetres || rotation >= MinMotionRotation)
        return true;
    }
    return false;
  }

  // Two draws of a part a while apart (sa, sb: their bones, count of them)
  // fitted to an owner's recorded bones (ra, rb: its bones' rows at the times
  // tried round each, owned bones in each): each of the part's bones is the
  // recorded bone that moved the same way, times the fixed transform it had
  // to it at the first. The two times are the pair that fits the part's first
  // bone best (with any recorded bone). The worst bone's miss (metres; 1e30
  // if none fitted its rotation). With only (the game's mesh's list of the
  // part's bones), each of its bones is fitted to that recorded bone alone.
  float FitPair(std::vector<std::vector<float>> const& ra, std::vector<std::vector<float>> const& rb, int owned, float const* sa,
    float const* sb, int count, std::vector<int>& bone, std::vector<float>& fixed, std::vector<int> const* only = nullptr)
  {
    float best = 1e30f;
    size_t ka = 0, kb = 0;
    float inverse[12], k[12], p[12], metres, rotation;
    if (only && ((int)only->size() != count || count < 1))
      return best;
    for (int j = 0; j < owned; ++j)
    {
      if (only && j != (*only)[0])
        continue;
      for (size_t x = 0; x < ra.size(); ++x)
      {
        if (!Inverse3x4(&ra[x][(size_t)j * 12], inverse))
          continue;
        Multiply3x4(inverse, sa, k);
        for (size_t y = 0; y < rb.size(); ++y)
        {
          Multiply3x4(&rb[y][(size_t)j * 12], k, p);
          MatrixError(p, sb, metres, rotation);
          if (rotation <= MatchRotation && metres < best - 1e-4f)
          {
            best = metres;
            ka = x;
            kb = y;
          }
        }
      }
    }
    if (best > MatchMetres)
      return best;
    bone.assign(count, -1);
    fixed.assign((size_t)count * 12, 0.f);
    float worst = 0.f;
    for (int i = 0; i < count && worst <= MatchMetres; ++i)
    {
      float bestBone = 1e30f;
      for (int j = 0; j < owned; ++j)
      {
        if (only && j != (*only)[i])
          continue;
        if (!Inverse3x4(&ra[ka][(size_t)j * 12], inverse))
          continue;
        Multiply3x4(inverse, sa + (size_t)i * 12, k);
        Multiply3x4(&rb[kb][(size_t)j * 12], k, p);
        MatrixError(p, sb + (size_t)i * 12, metres, rotation);
        if (rotation <= MatchRotation && metres < bestBone - 1e-4f)
        {
          bestBone = metres;
          bone[i] = j;
          memcpy(&fixed[(size_t)i * 12], k, sizeof(k));
        }
      }
      worst = (std::max)(worst, bestBone);
    }
    return worst;
  }
}

bool FaithReplay::DrawnByGameDraws(PersonTrack const& person) const
{
  return m_GameDrawsOn && m_ModelsOn && m_ModelShapesOn && (!person.GameDraws.empty() || person.BoundDrawn);
}

void FaithReplay::AdoptGameDraws()
{
  BindOwners(m_RecordClock);
  std::vector<replay_gamedraws::SkinCandidate> candidates;
  replay_gamedraws::TakeSkinCandidates(candidates);
  if (candidates.empty())
    return;
  // Each person's matrices at the times tried (the same for all the
  // candidates of a frame: worked out once)
  struct Posed
  {
    PersonTrack const* Person;
    double Time;
    bool Ok;
    std::vector<float> Matrices;
  };
  std::vector<Posed> posed;
  static std::vector<float> local, world;   // game thread
  auto matricesAt = [&](PersonTrack const& person, double time) -> float const* {
    for (Posed const& p : posed)
      if (p.Person == &person && fabs(p.Time - time) < 1e-9)
        return p.Ok ? p.Matrices.data() : nullptr;
    posed.push_back({ &person, time, false, {} });
    posed.back().Ok = PersonMatricesAt(person, time, posed.back().Matrices, local, world);
    return posed.back().Ok ? posed.back().Matrices.data() : nullptr;
  };
  struct Match
  {
    replay_gamedraws::SkinCandidate* C;
    PersonTrack* P;
    int Header;
    std::vector<int> Map;
    float Metres;
    double Dt;
    std::vector<std::vector<int>> Near;      // each of its bones: the skeleton bones within MatchMetres (and turn)
    std::vector<std::vector<float>> NearErr; // how far each was
    // The game's mesh's list of its bones (see below): -1 not read, 0 read
    // and not used (one didn't fit), 1 used; and how many of them weren't the
    // nearest-fitting one
    int Listed = -1;
    int ListedOther = 0;
  };
  std::vector<Match> matches;
  std::vector<replay_gamedraws::SkinCandidate*> unmatched;
  for (auto& c : candidates)
  {
    // A gun's or door's mesh: not a person's (see AdoptBound)
    if (m_BoundVbs.count(c.Vb))
    {
      unmatched.push_back(&c);
      continue;
    }
    Match best{ &c, nullptr, 0, {}, 1e30f, 0.0, {}, {} };
    // Its bones as the game's mesh lists them (2026-11-03, the user: "ghost
    // runner head still stays at place instead of following the body"; see
    // replay_shapes::SectionBones): its header is the rows before them. The
    // ghost runner's part has 3 header rows and 80 bones; taken as 12 and 77,
    // its first 3 bones (the skeleton's 54 52 7: its head) were drawn as kept
    // from its first frame
    std::vector<int> listed;
    int listedHeader = -1;
    if (replay_shapes::SectionBones(c.Vb, c.StartIndex, c.IndexCount, listed) && (int)c.Stride >= (int)listed.size() * 3 &&
        (int)c.Stride - (int)listed.size() * 3 <= MaxHeaderRows)
      listedHeader = (int)c.Stride - (int)listed.size() * 3;
    // Its header: 12 rows as the draw log found, else any that leaves only transforms
    for (int pass = 0; pass < 2 && !best.P; ++pass)
      for (int header = pass == 0 ? 12 : 0; header <= (pass == 0 ? 12 : MaxHeaderRows) && !best.P; ++header)
      {
        if (pass == 1 && header == 12)
          continue;
        if (listedHeader >= 0 && header != listedHeader)
          continue;
        if ((int)c.Stride < header + 3 || ((int)c.Stride - header) % 3)
          continue;
        int count = ((int)c.Stride - header) / 3;
        float const* bones = c.Instance.data() + (size_t)header * 4;
        if (!AllTransforms(bones, count))
          continue;
        float first[3] = { bones[3], bones[7], bones[11] };
        for (auto& person : m_People)
        {
          // Characters (guns and drones: AdoptBound)
          if (person->Other || person->BodyOffset == ActorBodyOffset || person->Times.empty() ||
              c.Clock < person->Times.front() - 1.0 || c.Clock > person->Times.back() + 1.0)
            continue;
          // Near it then
          float place[3];
          if (!PersonPlaceAt(*person, c.Clock, place) || Distance3(place, first) > MatchNear)
            continue;
          if (pass == 0)
            ++person->DrawsNear;
          // The time that fits its first bone best
          double bestDt = 0.0;
          float bestFirst = 1e30f;
          for (double dt = MatchDtMin; dt <= MatchDtMax + 1e-9; dt += MatchDtStep)
          {
            float const* m = matricesAt(*person, c.Clock + dt);
            if (!m)
              break;
            for (int j = 0; j < person->Bones; ++j)
            {
              float metres, rotation;
              MatrixError(bones, m + (size_t)j * MatrixFloats, metres, rotation);
              if (rotation <= MatchRotation && metres < bestFirst)
              {
                bestFirst = metres;
                bestDt = dt;
              }
            }
          }
          if (bestFirst > MatchMetres)
          {
            person->BestMiss = (std::min)(person->BestMiss, bestFirst);
            continue;
          }
          // Every bone of it then
          float const* m = matricesAt(*person, c.Clock + bestDt);
          std::vector<int> map(count, -1);
          std::vector<std::vector<int>> fitting(count);
          std::vector<std::vector<float>> fittingErr(count);
          float worst = 0.f;
          for (int i = 0; i < count && worst <= MatchMetres; ++i)
          {
            float bestMetres = 1e30f;
            for (int j = 0; j < person->Bones; ++j)
            {
              float metres, rotation;
              MatrixError(bones + (size_t)i * 12, m + (size_t)j * MatrixFloats, metres, rotation);
              if (rotation <= MatchRotation && metres < bestMetres)
              {
                bestMetres = metres;
                map[i] = j;
              }
              if (rotation <= MatchRotation && metres <= MatchMetres)
              {
                fitting[i].push_back(j);
                fittingErr[i].push_back(metres);
              }
            }
            worst = (std::max)(worst, bestMetres);
          }
          person->BestMiss = (std::min)(person->BestMiss, worst);
          // The game's list in place of the nearest (bones that hadn't moved
          // against each other since the bind pose fit alike), each checked
          // against the pose: none taken if one doesn't fit
          int listedUse = -1, listedOther = 0;
          if (listedHeader == header && worst <= MatchMetres)
          {
            std::vector<float> listedErr(count, 0.f);
            listedUse = 1;
            for (int i = 0; i < count && listedUse; ++i)
            {
              int j = listed[i];
              float metres = 1e30f, rotation = 1e30f;
              if (j >= 0 && j < person->Bones)
                MatrixError(bones + (size_t)i * 12, m + (size_t)j * MatrixFloats, metres, rotation);
              if (rotation > MatchRotation || metres > MatchMetres)
                listedUse = 0;
              listedErr[i] = metres;
              listedOther += map[i] != j;
            }
            if (listedUse)
              for (int i = 0; i < count; ++i)
              {
                map[i] = listed[i];
                fitting[i].assign(1, listed[i]);
                fittingErr[i].assign(1, listedErr[i]);
              }
          }
          if (worst <= MatchMetres && worst < best.Metres)
          {
            best = { &c, person.get(), header, map, worst, bestDt, std::move(fitting), std::move(fittingErr) };
            best.Listed = listedUse;
            best.ListedOther = listedOther;
          }
        }
      }
    // Once a part: whether the game's list was used
    if (best.P && best.Listed >= 0)
    {
      static std::set<std::tuple<__int64, unsigned, unsigned>> s_listedLogged;   // game thread
      if (s_listedLogged.size() < 120 && s_listedLogged.insert(std::make_tuple(c.Vb, c.StartIndex, c.IndexCount)).second)
        util::log::Write("Replay:   %s 0x%I64X: a part's bones (%u indices) %s the game's mesh lists them: %d bones after %d header rows, "
          "%d of them another skeleton bone than the nearest-fitting one", best.P->Label, best.P->Entity, c.IndexCount, best.Listed ?
          "taken as" : "not taken as (one didn't fit the recorded pose)", (int)listed.size(), best.Header, best.ListedOther);
    }
    else if (best.P)
    {
      static int s_unlisted = 0;
      if (s_unlisted++ < 20)
        util::log::Write("Replay:   %s 0x%I64X: a part's bones (%u indices) %s: matched as before (%d header rows)", best.P->Label,
          best.P->Entity, c.IndexCount, listed.empty() ? "not found in the game's mesh" : "listed by the game's mesh with more bones "
          "than its draw has room for", best.Header);
    }
    if (best.P)
      matches.push_back(std::move(best));
    else
      unmatched.push_back(&c);
  }
  // Each frame's time as the people in it fitted it (their closest match's),
  // for the guns, drones and doors drawn in it
  std::vector<std::pair<double, double>> frameDt;
  std::vector<float> frameBest;
  for (Match const& m : matches)
  {
    size_t f = 0;
    while (f < frameDt.size() && frameDt[f].first != m.C->Clock)
      ++f;
    if (f == frameDt.size())
    {
      frameDt.push_back({ m.C->Clock, m.Dt });
      frameBest.push_back(m.Metres);
    }
    else if (m.Metres < frameBest[f])
    {
      frameDt[f].second = m.Dt;
      frameBest[f] = m.Metres;
    }
  }
  // A part's bones told apart over its frames (2026-11-01, the user: "Icarus
  // ... his mouth bones seem wrong"; the 19:04 test: the replay's world pose
  // and matrices matched the game's on every bone, so the draws' bones' map
  // was left). A draw's bone was taken to be the skeleton bone nearest it in
  // the one frame its set was kept from, and face bones lie a centimetre or
  // two apart (two coincide while a mouth is shut); Icarus's set was kept
  // from a frame matched within 3.1 cm. Now each kept part keeps, per bone,
  // the skeleton bones within MatchMetres in its first frame, sums how far
  // each is in every frame of that part matched after (one that didn't fit
  // counts MatchMetres + 1 cm), and is drawn with the nearest over all of them.
  // (2026-11-02, the 15:53 test: up to 48 of a part's 59 bones changed back
  // and forth in a second, KrugerSec standing still: bones in the same place,
  // a limb's and its twist bone's, fit as well as each other. A bone changes
  // only to one nearer by MapMarginMetres a frame on the whole.)
  auto nearestOver = [](PersonTrack::GameDraw& d) {
    const float MapMarginMetres = 0.002f;
    int changed = 0;
    for (size_t i = 0; i < d.Map.size() && i < d.Cands.size(); ++i)
    {
      if (d.Cands[i].empty())
        continue;
      size_t best = 0, now = d.Cands[i].size();
      for (size_t k = 0; k < d.Cands[i].size(); ++k)
      {
        if (d.Sums[i][k] < d.Sums[i][best])
          best = k;
        if (d.Cands[i][k] == d.Map[i])
          now = k;
      }
      if (now < d.Cands[i].size() && d.Sums[i][best] >= d.Sums[i][now] - MapMarginMetres * (std::max)(d.Frames, 1))
        continue;
      if (d.Map[i] != d.Cands[i][best])
      {
        d.Map[i] = d.Cands[i][best];
        ++changed;
      }
    }
    return changed;
  };
  // The skeleton's order (2026-11-02, the user: "Just like the ksec the ghost
  // runner head is stretched"). A draw's bone matrix is its bone's pose
  // relative to the mesh's bind pose, so bones that hadn't moved against
  // each other since it (a still guard's spine, neck and head; the ghost
  // runner's: 70 of its 77 bones fitted more than one skeleton bone) give
  // the same matrix: the frame it was kept from can't tell them apart, and
  // when no later frame of that same part is matched (another level of
  // detail drawn then) the first fitting one stays. A wrong one is drawn
  // right while it stands so and pulled away once it moves (a head turned
  // with the spine's bone). Whether a part's bone list is in the skeleton's
  // order isn't known from the game's code: it's judged by the parts' bones
  // that fitted one skeleton bone only. A part with OrderedJudgeSingles of
  // them is in order or not; one in order with OrderedSinglesNeeded of them,
  // or with OrderedSinglesSome while every part judged so far was (at least
  // OrderedPartsNeeded of them, none not), has its fitting bones chosen in
  // rising order, the least summed distance in all (Sums). Otherwise nothing
  // changes (Ordered 0 or -1 in the log).
  auto orderedOver = [](PersonTrack::GameDraw& d) {
    const int OrderedJudgeSingles = 5, OrderedSinglesNeeded = 8, OrderedSinglesSome = 2, OrderedPartsNeeded = 3;
    static std::map<std::tuple<__int64, unsigned, unsigned>, bool> judged;   // game thread
    size_t n = d.Map.size();
    if (n < 2 || d.Cands.size() != n || d.Sums.size() != n)
      return 0;
    int last = -1, singles = 0;
    bool rising = true;
    for (size_t i = 0; i < n; ++i)
    {
      if (d.Cands[i].empty() || d.Sums[i].size() != d.Cands[i].size())
        return 0;
      if (d.Cands[i].size() != 1)
        continue;
      rising = rising && d.Cands[i][0] > last;
      last = d.Cands[i][0];
      ++singles;
    }
    if (singles >= OrderedJudgeSingles && judged.size() < 4096)
      judged[std::make_tuple(d.Vb, d.StartIndex, d.IndexCount)] = rising;
    int inOrder = 0, notInOrder = 0;
    for (auto const& j : judged)
      (j.second ? inOrder : notInOrder) += 1;
    // (2026-11-02, the 21:36 test: no part's were in order, 3 - 5 of each
    // person's not, so the lists aren't in the skeleton's order: never used
    // while any part judged wasn't)
    d.Ordered = !rising ? 0 : notInOrder == 0 && (singles >= OrderedSinglesNeeded ||
      (singles >= OrderedSinglesSome && inOrder >= OrderedPartsNeeded)) ? 1 : -1;
    if (d.Ordered != 1)
      return 0;
    const float none = 1e30f;
    std::vector<std::vector<float>> cost(n);
    std::vector<std::vector<int>> from(n);
    for (size_t i = 0; i < n; ++i)
    {
      cost[i].assign(d.Cands[i].size(), none);
      from[i].assign(d.Cands[i].size(), -1);
    }
    for (size_t k = 0; k < d.Cands[0].size(); ++k)
      cost[0][k] = d.Sums[0][k];
    for (size_t i = 1; i < n; ++i)
      for (size_t k = 0; k < d.Cands[i].size(); ++k)
        for (size_t p = 0; p < d.Cands[i - 1].size(); ++p)
          if (cost[i - 1][p] < none && d.Cands[i - 1][p] < d.Cands[i][k] && cost[i - 1][p] + d.Sums[i][k] < cost[i][k])
          {
            cost[i][k] = cost[i - 1][p] + d.Sums[i][k];
            from[i][k] = (int)p;
          }
    int at = -1;
    for (size_t k = 0; k < d.Cands[n - 1].size(); ++k)
      if (cost[n - 1][k] < none && (at < 0 || cost[n - 1][k] < cost[n - 1][at]))
        at = (int)k;
    if (at < 0)
    {
      d.Ordered = 0;   // no choice in rising order fits
      return 0;
    }
    int changed = 0;
    for (size_t i = n; i-- > 0;)
    {
      int bone = d.Cands[i][at];
      changed += d.Map[i] != bone;
      d.Map[i] = bone;
      at = i > 0 ? from[i][at] : -1;
      if (i > 0 && at < 0)
        return changed;   // (not reached: every step had a fitting one before it)
    }
    return changed;
  };
  auto settle = [&nearestOver, &orderedOver](PersonTrack::GameDraw& d) {
    std::vector<int> before = d.Map;
    nearestOver(d);
    orderedOver(d);
    int changed = 0;
    for (size_t i = 0; i < before.size() && i < d.Map.size(); ++i)
      changed += before[i] != d.Map[i];
    return changed;
  };
  auto sameKey = [](PersonTrack::GameDraw const& d, replay_gamedraws::SkinCandidate const& c) {
    return d.Vb == c.Vb && d.StartIndex == c.StartIndex && d.IndexCount == c.IndexCount && d.PassKey == c.PassKey;
  };
  static int s_mapLogs = 0;
  for (Match const& m : matches)
    for (auto& d : m.P->GameDraws)
    {
      if (d.Id == m.C->Id || !sameKey(d, *m.C) || d.Cands.size() != d.Map.size() || m.Near.size() != d.Map.size())
        continue;
      for (size_t i = 0; i < d.Cands.size(); ++i)
        for (size_t k = 0; k < d.Cands[i].size(); ++k)
        {
          float err = MatchMetres + 0.01f;
          for (size_t n = 0; n < m.Near[i].size(); ++n)
            if (m.Near[i][n] == d.Cands[i][k])
            {
              err = m.NearErr[i][n];
              break;
            }
          d.Sums[i][k] += err;
        }
      ++d.Frames;
      int changed = settle(d);
      if (changed && s_mapLogs++ < MaxAdoptLogs)
        util::log::Write("Replay:   %s 0x%I64X: a part's bones (%u indices) checked against %d frames of it: %d of its %d bones now drawn "
          "from another skeleton bone, the nearest over all of them", m.P->Label, m.P->Entity, d.IndexCount, d.Frames + 1, changed,
          (int)d.Map.size());
      // Diagnostic (2026-11-03, the user: "ghost runner head is still
      // streching"; the cause isn't known): the part's bones whose skeleton
      // bone, as drawn now, didn't fit this frame of it, and its header rows
      // (drawn as kept from its first frame) that weren't the same in this one
      {
        static int s_fitLogs = 0, s_headerLogs = 0;
        int misfit = 0;
        char list[240] = "";
        size_t at = 0;
        for (size_t i = 0; i < d.Map.size(); ++i)
        {
          if (std::find(m.Near[i].begin(), m.Near[i].end(), d.Map[i]) != m.Near[i].end())
            continue;
          ++misfit;
          if (at + 24 < sizeof(list))
            at += sprintf_s(list + at, sizeof(list) - at, " %d -> %d (%d)", (int)i, d.Map[i], i < m.Map.size() ? m.Map[i] : -1);
        }
        if (misfit && s_fitLogs++ < 40)
          util::log::Write("Replay (diagnostic):   %s 0x%I64X at %.2f s: a part's bones (%u indices) against frame %d of it: %d of its %d "
            "drawn from a skeleton bone that didn't fit it then (its bone -> drawn from (the nearest then)):%s", m.P->Label, m.P->Entity,
            m.C->Clock, d.IndexCount, d.Frames, misfit, (int)d.Map.size(), list);
        size_t headerFloats = d.HeaderRows.size();
        if (headerFloats && (size_t)m.Header * 4 == headerFloats && m.C->Instance.size() >= headerFloats && s_headerLogs < 20)
        {
          int rows = 0;
          char rowList[480] = "";
          size_t rowAt = 0;
          for (size_t r = 0; r < headerFloats / 4; ++r)
          {
            float const* kept = &d.HeaderRows[r * 4];
            float const* now = &m.C->Instance[r * 4];
            bool differs = false;
            for (int c = 0; c < 4; ++c)
              differs = differs || fabsf(kept[c] - now[c]) > 1e-3f * (std::max)(1.f, fabsf(kept[c]));
            if (!differs)
              continue;
            ++rows;
            if (rowAt + 100 < sizeof(rowList))
              rowAt += sprintf_s(rowList + rowAt, sizeof(rowList) - rowAt, " row %d %.4g %.4g %.4g %.4g -> %.4g %.4g %.4g %.4g;", (int)r,
                kept[0], kept[1], kept[2], kept[3], now[0], now[1], now[2], now[3]);
          }
          if (rows)
          {
            ++s_headerLogs;
            util::log::Write("Replay (diagnostic):   %s 0x%I64X at %.2f s: a part's header rows (%u indices; drawn as kept from its "
              "first frame) not the same in this frame of it: %d of %d:%s", m.P->Label, m.P->Entity, m.C->Clock, d.IndexCount, rows,
              (int)(headerFloats / 4), rowList);
          }
        }
      }
      break;
    }
  // A person's set from one frame (its candidates' time): the most detailed one kept
  std::vector<bool> taken(matches.size(), false);
  static int s_logs = 0;
  for (size_t a = 0; a < matches.size(); ++a)
  {
    if (taken[a])
      continue;
    PersonTrack* person = matches[a].P;
    double clock = matches[a].C->Clock;
    std::vector<size_t> set;
    unsigned long long indices = 0;
    for (size_t b = a; b < matches.size(); ++b)
    {
      Match const& m = matches[b];
      if (taken[b] || m.P != person || m.C->Clock != clock)
        continue;
      taken[b] = true;
      bool same = false;
      for (size_t s : set)
        same = same || (matches[s].C->Vb == m.C->Vb && matches[s].C->StartIndex == m.C->StartIndex &&
          matches[s].C->IndexCount == m.C->IndexCount && matches[s].C->PassKey == m.C->PassKey);
      if (same || (int)set.size() >= MaxGameDrawsPerPerson)
      {
        replay_gamedraws::ReleaseSkin(m.C->Id);
        continue;
      }
      set.push_back(b);
      indices += m.C->IndexCount;
    }
    if (indices <= person->GameDrawIndices)
    {
      for (size_t s : set)
        replay_gamedraws::ReleaseSkin(matches[s].C->Id);
      continue;
    }
    std::vector<PersonTrack::GameDraw> previous;
    previous.swap(person->GameDraws);
    for (auto const& old : previous)
      replay_gamedraws::ReleaseSkin(old.Id);
    float worst = 0.f;
    int ambiguous = 0, ordered = 0, unordered = 0;
    for (size_t s : set)
    {
      Match const& m = matches[s];
      PersonTrack::GameDraw draw{ m.C->Id, m.C->Vb, m.C->StartIndex, m.C->IndexCount, m.Map, {} };
      draw.HeaderRows.assign(m.C->Instance.begin(), m.C->Instance.begin() + (size_t)m.Header * 4);
      draw.PassKey = m.C->PassKey;
      // The part kept before carries its frames (this one's added above); a new part starts from this frame
      PersonTrack::GameDraw* old = nullptr;
      for (auto& p : previous)
        if (sameKey(p, *m.C) && p.Cands.size() == m.Map.size())
          old = &p;
      if (old)
      {
        draw.Cands = std::move(old->Cands);
        draw.Sums = std::move(old->Sums);
        draw.Frames = old->Frames;
        settle(draw);
      }
      else if (m.Near.size() == m.Map.size())
      {
        draw.Cands = m.Near;
        draw.Sums = m.NearErr;
        draw.Frames = 1;
        settle(draw);
      }
      for (auto const& c : draw.Cands)
        ambiguous += c.size() > 1;
      ordered += draw.Ordered == 1;
      unordered += draw.Ordered == 0;
      person->GameDraws.push_back(std::move(draw));
      worst = (std::max)(worst, m.Metres);
    }
    bool first = person->GameDrawIndices == 0;
    person->GameDrawIndices = indices;
    if (s_logs++ < MaxAdoptLogs)
      util::log::Write("Replay:   %s 0x%I64X: %s %d parts as the game draws it (%llu indices; its bones matched within %.1f cm, the "
        "frame drawn %.0f ms from the recording's; posed from its skeleton; %d of their bones fit more than one skeleton bone, told "
        "apart by the frames after; %d parts' bones in the skeleton's order, ties chosen in it, %d not)", person->Label,
        person->Entity, first ? "kept" : "a more detailed set kept,", (int)set.size(), indices, worst * 100.f,
        matches[set[0]].Dt * 1000.0, ambiguous, ordered, unordered);
  }
  // The rest: guns', drones' and doors' (stage 2b)
  AdoptBound(unmatched, frameDt);
}

// While it records: the guns and drones, and the recorded models with no
// shape kept that moved, each with its mesh's vertex buffers once they're
// read (a gun's or baton's: every weapon mesh the game has, by name, see
// WeaponMeshPrefix; a drone's isn't known: its draws are found by how they move
// with it), all handed to ReplayGameDraws so their draws are kept more often.
// (A skinned door has its shape kept now, ReplayShapes MeshSetOf: it's drawn
// as a model.)
void FaithReplay::BindOwners(double time)
{
  static int s_logs = 0;
  auto add = [this](PersonTrack* person, __int64 entity, char const* label)
  {
    for (auto const& owner : m_Bound)
      if (person ? owner.Person == person : !owner.Person && owner.Entity == entity)
        return;
    BoundOwner owner;
    owner.Person = person;
    owner.Entity = entity;
    owner.Label = label;
    m_Bound.push_back(std::move(owner));
  };
  for (auto const& person : m_People)
    if (person->BodyOffset == ActorBodyOffset && !person->Other && !person->Times.empty())
      add(person.get(), person->Entity, person->Label);
  std::vector<std::pair<__int64, std::string>> shapeless;
  m_Models.ShapelessMoved(shapeless);
  for (auto const& model : shapeless)
    add(nullptr, model.first, model.second.c_str());
  bool changed = false;
  // The weapon meshes the game has (guns of a kind share one; looked for again
  // as they stream in), for every gun and baton
  if (time >= m_NextWeaponScan || time < m_NextWeaponScan - WeaponScanEvery)
  {
    m_NextWeaponScan = time + WeaponScanEvery;
    std::vector<__int64> vbs;
    std::vector<std::string> names;
    int meshes = replay_shapes::MeshBuffersNamed(WeaponMeshPrefix, vbs, &names);
    std::sort(vbs.begin(), vbs.end());
    if (vbs != m_WeaponVbs)
    {
      m_WeaponVbs.swap(vbs);
      changed = true;
      std::string list;
      for (std::string const& name : names)
        list += (list.empty() ? "" : ", ") + name;
      if (s_logs++ < MaxBoundLogs)
        util::log::Write("Replay:   the game's weapon meshes: %d (%d vertex buffers): %s", meshes, (int)m_WeaponVbs.size(),
          list.empty() ? "none" : list.c_str());
    }
  }
  for (auto& owner : m_Bound)
  {
    if (owner.Person && IsGun(*owner.Person))
    {
      // A gun or baton: the weapon meshes' buffers (while there are none, it
      // waits; past MaxVbTries without any it's found by how it moves, as a drone)
      if (owner.Vbs != m_WeaponVbs && !m_WeaponVbs.empty())
      {
        owner.Vbs = m_WeaponVbs;
        changed = true;
      }
      else if (m_WeaponVbs.empty() && owner.VbTries < MaxVbTries && time >= owner.NextVbTry)
      {
        ++owner.VbTries;
        owner.NextVbTry = time + VbTryEvery;
      }
      continue;
    }
    if (!owner.Vbs.empty() || owner.VbTries >= MaxVbTries || time < owner.NextVbTry)
      continue;
    if (owner.Person)
    {
      owner.VbTries = MaxVbTries;   // a drone: where its mesh is isn't known
      continue;
    }
    ++owner.VbTries;
    owner.NextVbTry = time + VbTryEvery;
    int found = replay_shapes::ModelMeshBuffers(owner.Entity, owner.Vbs);
    if (found)
    {
      changed = true;
      if (s_logs++ < MaxBoundLogs)
        util::log::Write("Replay:   %s 0x%I64X: its mesh's vertex buffers read (%d levels): the game's draws of it are known by them",
          owner.Label.c_str(), owner.Entity, found);
    }
    else if (owner.VbTries == MaxVbTries && s_logs++ < MaxBoundLogs)
      util::log::Write("Replay:   %s 0x%I64X: its mesh's vertex buffers couldn't be read (%d tries)", owner.Label.c_str(),
        owner.Entity, owner.VbTries);
  }
  if (changed)
  {
    std::vector<__int64> vbs;
    for (auto const& owner : m_Bound)
      vbs.insert(vbs.end(), owner.Vbs.begin(), owner.Vbs.end());
    m_BoundVbs.clear();
    m_BoundVbs.insert(vbs.begin(), vbs.end());
    replay_gamedraws::SetWantedSkins(vbs);
  }
}

// An owner's recorded bones at a time as 3x4 matrices (12 floats each): a
// gun's or drone's from its world pose, a door's its transform (one). While
// it records, a door's as recorded so far; while it plays, only one the game
// doesn't have (and isn't hidden then)
bool FaithReplay::OwnerRowsAt(BoundOwner const& owner, double time, bool playing, std::vector<float>& rows)
{
  if (!owner.Person)
  {
    float transform[16];
    if (!(playing ? m_Models.GoneTransform(owner.Entity, time, transform) : m_Models.RecordedTransform(owner.Entity, time, transform)))
      return false;
    rows.resize(12);
    TransformRows(transform, rows.data());
    return true;
  }
  PersonTrack const& person = *owner.Person;
  if (!HasRecordedWorld(person))
    return false;
  static std::vector<float> world;   // game thread
  world.resize((size_t)person.Bones * FloatsPerBone);
  SamplePose(person.Times, person.WorldFrames, person.Bones, time, world.data());
  rows.resize((size_t)person.Bones * 12);
  for (int b = 0; b < person.Bones; ++b)
    PoseRows(&world[(size_t)b * FloatsPerBone], &rows[(size_t)b * 12]);
  return true;
}

// The game's skinned draws no recorded person took (stage 2b; 2026-10-27
// 17:51 matched by place only, a door took Icarus's body, and the guns' slot
// matrices weren't where they were drawn):
// - one of a gun's or door's mesh (its vertex buffer, BindOwners) is the
//   nearest owner of that mesh's (guns of a kind share one) within MatchNear;
// - another is a drone's maybe (its mesh isn't known): near one, not a part of
//   a person's, it's kept (m_SkinPool) until a later draw of the same part,
//   the drone having moved, and taken if each of its bones moved as one of the
//   drone's recorded bones did (FitPair).
// An owner keeps the draws of one frame, the most detailed set seen (as a
// person does). Each draw's bones are a recorded bone's (a door's: its
// transform) times a fixed transform: from the draw kept, all of them
// following the owner's bone 1 (where a gun is), until a later draw of the
// same part, the owner having moved in between, fits each bone to the
// recorded bone that moved the same way (Checked). A set a later draw doesn't
// fit is let go (the wrong one of two guns of a kind, or it doesn't move with
// its recording), and the owner given up on after MaxMisfits of them.
void FaithReplay::AdoptBound(std::vector<replay_gamedraws::SkinCandidate*>& rest,
  std::vector<std::pair<double, double>> const& frameDt)
{
  static int s_logs = 0;
  // The lit pass's draws (people's see-through parts, see replay_gamedraws::
  // CapturingSkins) aren't a gun's, drone's or door's
  rest.erase(std::remove_if(rest.begin(), rest.end(), [](replay_gamedraws::SkinCandidate* c) {
    if (!c->PassKey)
      return false;
    replay_gamedraws::ReleaseSkin(c->Id);
    return true;
  }), rest.end());
  auto preferred = [&frameDt](double clock)
  {
    for (auto const& f : frameDt)
      if (f.first == clock)
        return f.second;
    return 0.0;
  };
  // An owner's rows at the times tried round a time (worked out once; a
  // deque, so those had stay where they are)
  struct Grid
  {
    size_t Owner;
    double Clock;
    bool Ok;
    std::vector<std::vector<float>> Rows;
  };
  std::deque<Grid> grids;
  auto gridAt = [&](size_t o, double clock) -> Grid const&
  {
    for (Grid const& g : grids)
      if (g.Owner == o && g.Clock == clock)
        return g;
    grids.push_back({ o, clock, true, {} });
    Grid& g = grids.back();
    for (double dt : DtOrder(preferred(clock)))
    {
      g.Rows.emplace_back();
      if (!OwnerRowsAt(m_Bound[o], clock + dt, false, g.Rows.back()))
      {
        g.Ok = false;
        break;
      }
    }
    return g;
  };
  // The parts kept for a later draw: let go once old
  double newest = 0.0;
  for (auto const* c : rest)
    newest = (std::max)(newest, c->Clock);
  for (size_t p = 0; p < m_SkinPool.size();)
    if (m_SkinPool[p].Clock < newest - SkinPoolSeconds)
    {
      replay_gamedraws::ReleaseSkin(m_SkinPool[p].Id);
      m_SkinPool.erase(m_SkinPool.begin() + p);
    }
    else
      ++p;
  auto personsPart = [this](replay_gamedraws::SkinCandidate const& c)
  {
    for (auto const& person : m_People)
      for (auto const& d : person->GameDraws)
        if (SameDraw(c, d.Vb, d.StartIndex, d.IndexCount))
          return true;
    return false;
  };
  struct Owned
  {
    replay_gamedraws::SkinCandidate* C;
    size_t Owner;
    int Header, Count;
    bool Paired;               // fitted with an earlier draw of it already (a drone's)
    std::vector<int> Bone;
    std::vector<float> Fixed;
    std::vector<int> Listed;   // the skeleton's bone for each of its bones as the game's mesh lists them (empty: not read)
  };
  std::vector<Owned> owned;
  static std::vector<float> rows;   // game thread
  static int s_listedMisses = 0;
  for (auto* c : rest)
  {
    int count = 0;
    std::vector<int> listed;
    int header = ListedSkinHeader(*c, count, listed);
    if (header < 0)
    {
      replay_gamedraws::ReleaseSkin(c->Id);
      continue;
    }
    float const* bones = c->Instance.data() + (size_t)header * 4;
    float first[3] = { bones[3], bones[7], bones[11] };
    if (m_BoundVbs.count(c->Vb))
    {
      // Its mesh's owner nearest it then
      size_t best = SIZE_MAX;
      float bestDistance = MatchNear;
      for (size_t o = 0; o < m_Bound.size(); ++o)
      {
        BoundOwner const& owner = m_Bound[o];
        if (owner.Refused || std::find(owner.Vbs.begin(), owner.Vbs.end(), c->Vb) == owner.Vbs.end())
          continue;
        float place[3];
        if (owner.Person)
        {
          if (!PersonPlaceAt(*owner.Person, c->Clock, place))
            continue;
        }
        else
        {
          if (!OwnerRowsAt(owner, c->Clock, false, rows))
            continue;
          place[0] = rows[3];
          place[1] = rows[7];
          place[2] = rows[11];
        }
        float distance = Distance3(place, first);
        if (distance < bestDistance)
        {
          bestDistance = distance;
          best = o;
        }
      }
      if (best == SIZE_MAX)
        replay_gamedraws::ReleaseSkin(c->Id);
      else
        owned.push_back({ c, best, header, count, false, {}, {}, listed });
      continue;
    }
    // A drone's maybe: near one whose mesh isn't known, not a person's part
    std::vector<size_t> nearby;
    if (!personsPart(*c))
      for (size_t o = 0; o < m_Bound.size(); ++o)
      {
        BoundOwner const& owner = m_Bound[o];
        float place[3];
        if (owner.Person && !owner.Refused && owner.Vbs.empty() && owner.VbTries >= MaxVbTries && count <= owner.Person->Bones &&
            PersonPlaceAt(*owner.Person, c->Clock, place) && Distance3(place, first) <= MatchNear)
          nearby.push_back(o);
      }
    if (nearby.empty())
    {
      replay_gamedraws::ReleaseSkin(c->Id);
      continue;
    }
    // An earlier draw of the same part kept: the two fitted to each of them
    size_t earlier = SIZE_MAX;
    for (size_t p = 0; p < m_SkinPool.size() && earlier == SIZE_MAX; ++p)
      if (SameDraw(m_SkinPool[p], c->Vb, c->StartIndex, c->IndexCount))
        earlier = p;
    if (earlier == SIZE_MAX)
    {
      if ((int)m_SkinPool.size() >= MaxSkinPool)
      {
        replay_gamedraws::ReleaseSkin(m_SkinPool.front().Id);
        m_SkinPool.erase(m_SkinPool.begin());
      }
      m_SkinPool.push_back(*c);
      continue;
    }
    replay_gamedraws::SkinCandidate const& a = m_SkinPool[earlier];
    int countA = 0;
    std::vector<int> listedA;
    // Every bone moved (any, when the game's mesh lists them: each is fitted
    // to its own recorded bone then)
    bool same = a.Clock <= c->Clock - MinPairSeconds && ListedSkinHeader(a, countA, listedA) == header && countA == count;
    float const* bonesA = a.Instance.data() + (size_t)header * 4;
    bool allMoved = same && AllMoved(bonesA, bones, count);
    if (!same || !(allMoved || (!listed.empty() && AnyMoved(bonesA, bones, count))))
    {
      replay_gamedraws::ReleaseSkin(c->Id);   // the earlier one waits on
      continue;
    }
    Owned best{ c, 0, header, count, true, {}, {}, listed };
    float bestWorst = 1e30f;
    bool moved = false;
    for (size_t o : nearby)
    {
      Grid const& ga = gridAt(o, a.Clock);
      Grid const& gb = gridAt(o, c->Clock);
      if (!ga.Ok || !gb.Ok || !Moved(ga.Rows[0], gb.Rows[0]))
        continue;
      moved = true;
      std::vector<int> bone;
      std::vector<float> fixed;
      float worst = FitPair(ga.Rows, gb.Rows, (int)ga.Rows[0].size() / 12, bonesA, bones, count, bone, fixed,
        listed.empty() ? nullptr : &listed);
      // The game's list not fitting: fitted as before, where every bone moved
      if (worst > MatchMetres && !listed.empty())
      {
        if (s_listedMisses++ < 20)
          util::log::Write("Replay:   %s 0x%I64X: a part's bones (%u indices) as the game's mesh lists them don't move as its recorded "
            "bones did (%.1f cm off)%s", m_Bound[o].Label.c_str(), m_Bound[o].Entity, c->IndexCount, worst * 100.f, allMoved ?
            ": each fitted to the recorded bone that moved the same way" : "");
        if (allMoved)
        {
          worst = FitPair(ga.Rows, gb.Rows, (int)ga.Rows[0].size() / 12, bonesA, bones, count, bone, fixed);
          best.Listed.clear();
        }
      }
      m_Bound[o].Miss = (std::min)(m_Bound[o].Miss, worst);
      if (worst <= MatchMetres && worst < bestWorst)
      {
        bestWorst = worst;
        best.Owner = o;
        best.Bone.swap(bone);
        best.Fixed.swap(fixed);
      }
    }
    if (bestWorst <= MatchMetres)
    {
      // Taken: the earlier one let go
      replay_gamedraws::ReleaseSkin(a.Id);
      m_SkinPool.erase(m_SkinPool.begin() + earlier);
      owned.push_back(std::move(best));
    }
    else if (moved)
    {
      // No fit: this one kept in its place, for the next
      replay_gamedraws::ReleaseSkin(a.Id);
      m_SkinPool[earlier] = *c;
    }
    else
      replay_gamedraws::ReleaseSkin(c->Id);   // nothing moved yet: the earlier one waits on
  }
  // Each owner's draws from one frame: the most detailed set seen kept
  std::vector<bool> taken(owned.size(), false);
  for (size_t a = 0; a < owned.size(); ++a)
  {
    if (taken[a])
      continue;
    size_t o = owned[a].Owner;
    BoundOwner& owner = m_Bound[o];
    double clock = owned[a].C->Clock;
    std::vector<size_t> set;
    unsigned long long indices = 0;
    for (size_t b = a; b < owned.size(); ++b)
    {
      Owned const& m = owned[b];
      if (taken[b] || m.Owner != o || m.C->Clock != clock)
        continue;
      taken[b] = true;
      bool same = false;
      for (size_t s : set)
        same = same || SameDraw(*owned[s].C, m.C->Vb, m.C->StartIndex, m.C->IndexCount);
      if (same || (int)set.size() >= MaxGameDrawsPerPerson)
      {
        replay_gamedraws::ReleaseSkin(m.C->Id);
        continue;
      }
      set.push_back(b);
      indices += m.C->IndexCount;
    }
    owner.Kept += (int)set.size();
    auto releaseSet = [&]()
    {
      for (size_t s : set)
        replay_gamedraws::ReleaseSkin(owned[s].C->Id);
    };
    if (owner.Refused)
    {
      releaseSet();
      continue;
    }
    if (indices > owner.Indices)
    {
      // A more detailed set: kept in place of the one before, its bones
      // following the owner's bone 1 (a door's transform) until checked
      std::vector<float> at;
      float inverse[12];
      bool ok = OwnerRowsAt(owner, clock + preferred(clock), false, at);
      int follow = owner.Person ? (std::min)(1, (int)at.size() / 12 - 1) : 0;
      if (!ok || follow < 0 || !Inverse3x4(&at[(size_t)follow * 12], inverse))
      {
        releaseSet();
        continue;
      }
      for (auto const& d : owner.Draws)
        replay_gamedraws::ReleaseSkin(d.Id);
      owner.Draws.clear();
      int paired = 0, listedParts = 0;
      for (size_t s : set)
      {
        Owned& m = owned[s];
        listedParts += !m.Listed.empty();
        float const* bones = m.C->Instance.data() + (size_t)m.Header * 4;
        BoundDraw d{ m.C->Id, m.C->Vb, m.C->StartIndex, m.C->IndexCount, m.Header, {}, {}, {}, clock, {}, m.Paired };
        d.HeaderRows.assign(m.C->Instance.begin(), m.C->Instance.begin() + (size_t)m.Header * 4);
        d.Seen.assign(bones, bones + (size_t)m.Count * 12);
        if (m.Paired)
        {
          d.Bone.swap(m.Bone);
          d.Fixed.swap(m.Fixed);
          ++paired;
        }
        else
        {
          d.Bone.assign(m.Count, follow);
          d.Fixed.resize((size_t)m.Count * 12);
          for (int i = 0; i < m.Count; ++i)
            Multiply3x4(inverse, bones + (size_t)i * 12, &d.Fixed[(size_t)i * 12]);
        }
        owner.Draws.push_back(std::move(d));
      }
      bool first = owner.Indices == 0;
      owner.Indices = indices;
      if (owner.Person)
        owner.Person->BoundDrawn = true;
      if (s_logs++ < MaxBoundLogs)
        util::log::Write("Replay:   %s 0x%I64X: %s %d parts as the game draws it (%llu indices; %s; %d of them with their bones as the "
          "game's mesh lists them)", owner.Label.c_str(), owner.Entity, first ? "kept" : "a more detailed set kept,", (int)set.size(),
          indices, paired ? "each bone fitted to the recorded bone that moved with it, from two draws of it" : owner.Person ?
          "its bones following its recorded bone 1 until a later draw of it checks them" :
          "its bones following its recorded transform until a later draw of it checks them", listedParts);
      continue;
    }
    // The same set again: each part not checked yet, checked with this draw
    // of it (the owner having moved in between)
    int checkedNow = 0;
    float miss = -1.f;
    for (size_t s : set)
    {
      Owned const& m = owned[s];
      BoundDraw* d = nullptr;
      for (auto& kept : owner.Draws)
        if (SameDraw(*m.C, kept.Vb, kept.StartIndex, kept.IndexCount))
          d = &kept;
      if (d && !d->Checked && miss < 0.f && m.Header == d->Header && (size_t)m.Count * 12 == d->Seen.size() &&
          m.C->Clock >= d->Clock + MinPairSeconds)
      {
        Grid const& ga = gridAt(o, d->Clock);
        Grid const& gb = gridAt(o, m.C->Clock);
        if (ga.Ok && gb.Ok && Moved(ga.Rows[0], gb.Rows[0]))
        {
          std::vector<int> bone;
          std::vector<float> fixed;
          float worst = FitPair(ga.Rows, gb.Rows, (int)ga.Rows[0].size() / 12, d->Seen.data(),
            m.C->Instance.data() + (size_t)m.Header * 4, m.Count, bone, fixed, m.Listed.empty() ? nullptr : &m.Listed);
          if (worst > MatchMetres && !m.Listed.empty())
            worst = FitPair(ga.Rows, gb.Rows, (int)ga.Rows[0].size() / 12, d->Seen.data(),
              m.C->Instance.data() + (size_t)m.Header * 4, m.Count, bone, fixed);
          if (worst <= MatchMetres)
          {
            d->Bone.swap(bone);
            d->Fixed.swap(fixed);
            d->Checked = true;
            ++checkedNow;
          }
          else
            miss = worst;
        }
      }
      replay_gamedraws::ReleaseSkin(m.C->Id);
    }
    if (checkedNow && s_logs++ < MaxBoundLogs)
      util::log::Write("Replay:   %s 0x%I64X: %d parts checked with a later draw of them (each bone fitted to the recorded %s)",
        owner.Label.c_str(), owner.Entity, checkedNow, owner.Person ? "bone that moved with it" : "transform");
    if (miss >= 0.f)
    {
      owner.Miss = (std::min)(owner.Miss, miss);
      for (auto const& d : owner.Draws)
        replay_gamedraws::ReleaseSkin(d.Id);
      owner.Draws.clear();
      owner.Indices = 0;
      if (owner.Person)
        owner.Person->BoundDrawn = false;
      owner.Refused = ++owner.Misfits >= MaxMisfits;
      if (s_logs++ < MaxBoundLogs)
        util::log::Write("Replay:   %s 0x%I64X: a later draw of it didn't fit the parts kept (%.1f cm off): %s", owner.Label.c_str(),
          owner.Entity, miss * 100.f, owner.Refused ? "given up on" : "let go, kept again from its next draws");
    }
  }
}

void FaithReplay::ReleaseBound()
{
  for (auto const& owner : m_Bound)
    for (auto const& d : owner.Draws)
      replay_gamedraws::ReleaseSkin(d.Id);
  m_Bound.clear();
  for (auto const& c : m_SkinPool)
    replay_gamedraws::ReleaseSkin(c.Id);
  m_SkinPool.clear();
  m_BoundVbs.clear();
  m_WeaponVbs.clear();
  m_NextWeaponScan = 0.0;
  replay_gamedraws::SetWantedSkins({});
}

// At the end of a recording: who got no parts of the game's draws, and why
void FaithReplay::LogGameDrawsMissing()
{
  if (!(m_GameDrawsOn && m_ModelsOn && m_ModelShapesOn))
    return;
  int kept = 0, missing = 0;
  for (auto const& person : m_People)
  {
    if (person->Other || person->Times.empty() || person->BodyOffset == ActorBodyOffset ||
        util::hooks::IsNoStandInTemplate(person->Template))
      continue;
    if (!person->GameDraws.empty())
    {
      ++kept;
      continue;
    }
    ++missing;
    char why[200];
    if (!GameDrawable(*person))
    {
      Hierarchy const* hierarchy = person->Manager ? FindHierarchy(person->Manager, person->Bones) : nullptr;
      if (person->Unbind.size() != (size_t)person->Bones * MatrixFloats)
        sprintf_s(why, "its matrices' fixed transforms weren't read (%d tries)", person->UnbindTries);
      else if (!hierarchy || !hierarchy->Ready)
        sprintf_s(why, "its skeleton's hierarchy isn't known");
      else if (hierarchy->Order.empty())
        sprintf_s(why, "its skeleton's hierarchy has a loop");
      else
        sprintf_s(why, "its world pose wasn't recorded");
    }
    else if (!person->DrawsNear)
      sprintf_s(why, "no draw of the game's that reads bones was kept near it (none of its draws reads them, or it wasn't "
        "drawn in a frame they were kept in)");
    else
      sprintf_s(why, "%d draws kept near it, none fitted it (the closest %.1f cm off)", person->DrawsNear, person->BestMiss * 100.f);
    util::log::Write("Replay:   %s 0x%I64X: no parts of the game's draws of it: %s", person->Label, person->Entity, why);
  }
  // Guns, drones and doors (stage 2b)
  int actors = 0, actorsKept = 0, models = 0, modelsKept = 0;
  for (auto const& owner : m_Bound)
  {
    if (owner.Person && owner.Person->Times.empty())
      continue;
    ++(owner.Person ? actors : models);
    if (!owner.Draws.empty() && !owner.Refused)
    {
      ++(owner.Person ? actorsKept : modelsKept);
      int checked = 0;
      for (auto const& d : owner.Draws)
        checked += d.Checked;
      util::log::Write("Replay:   %s 0x%I64X: %d parts of the game's draws of it kept (%llu indices), %d of them checked with a "
        "later draw of them", owner.Label.c_str(), owner.Entity, (int)owner.Draws.size(), owner.Indices, checked);
      continue;
    }
    char why[200];
    bool drone = owner.Person && !IsGun(*owner.Person);
    if (owner.Person && !HasRecordedWorld(*owner.Person))
      sprintf_s(why, "its world pose wasn't recorded");
    else if (owner.Refused)
      sprintf_s(why, "%d sets of its draws didn't fit later draws of it (the closest %.1f cm off)", owner.Misfits, owner.Miss * 100.f);
    else if (owner.Vbs.empty() && !drone && owner.Person)
      sprintf_s(why, "no weapon mesh was in the game's mesh table, and no draw near it moved with it (the closest %.1f cm off)",
        owner.Miss < 1e29f ? owner.Miss * 100.f : -1.f);
    else if (owner.Vbs.empty() && !drone)
      sprintf_s(why, "its mesh's vertex buffers couldn't be read (%d tries)", owner.VbTries);
    else if (drone && owner.Miss < 1e29f)
      sprintf_s(why, "no draw near it moved with it (the closest %.1f cm off)", owner.Miss * 100.f);
    else if (drone)
      sprintf_s(why, "no draw that reads bones was kept near it twice as it moved");
    else
      sprintf_s(why, "no draw of its mesh was kept near it (it wasn't drawn in a frame they were kept in)");
    util::log::Write("Replay:   %s 0x%I64X: no parts of the game's draws of it: %s", owner.Label.c_str(), owner.Entity, why);
  }
  util::log::Write("Replay: the game's own draws kept for %d of %d recorded people, %d of %d guns and drones, and %d of %d moving "
    "models with no shape", kept, kept + missing, actorsKept, actors, modelsKept, models);
}

// Diagnostic (2026-11-01, the user: "Character(s) bones don't seem correct
// now", "ksec encountered had almost indivisible ksec but could still see
// hands/baton", "hunter drone was invisible"): while it plays, a few lines
// for each person, gun and drone drawn with the game's kept draws of it: how
// many of its draws are drawn, their bone palettes against its skeleton, the
// first bone's matrix (rows x y z, the place last) against where the
// recording has it. Game thread.
namespace
{
  const int PoseLogsEach = 6;
  const double PoseLogSeconds = 4.0;
  std::map<__int64, std::pair<int, double>> g_PoseLogs;

  bool WantPoseLog(__int64 entity)
  {
    double now = GetTickCount64() / 1000.0;
    auto& l = g_PoseLogs[entity];
    if (l.first >= PoseLogsEach || (l.first && now - l.second < PoseLogSeconds))
      return false;
    ++l.first;
    l.second = now;
    return true;
  }

  void LogPose(char const* who, __int64 entity, double time, int drawn, int kept, int minMap, int maxMap, int bones, int bad,
    float const* place, float const* m)
  {
    char where[64] = "not known";
    if (place)
      sprintf_s(where, "%.2f %.2f %.2f", place[0], place[1], place[2]);
    char first[200] = "none";
    float away = -1.f;
    if (m)
    {
      sprintf_s(first, "%.3f %.3f %.3f %.2f / %.3f %.3f %.3f %.2f / %.3f %.3f %.3f %.2f", m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7],
        m[8], m[9], m[10], m[11]);
      if (place)
        away = sqrtf((m[3] - place[0]) * (m[3] - place[0]) + (m[7] - place[1]) * (m[7] - place[1]) + (m[11] - place[2]) * (m[11] - place[2]));
    }
    util::log::Write("Replay (diagnostic): %s 0x%I64X at %.2f s drawn with the game's kept draws: %d of its %d draws, bone palettes "
      "%d - %d of its %d bones (%d with a bone past them); where it is %s; the first draw's first bone %s (%.2f m from there)", who,
      entity, time, drawn, kept, minMap, maxMap, bones, bad, where, first, away);
  }
}

// A recorded person's bone's world pose at a time, 12 floats (rows of its
// turn, its place last; see PoseRows), for a cloth carried by them
// (replay_gamedraws::AnchorCloths)
static bool PersonBoneAt(FaithReplay::PersonTrack const& person, int bone, double time, float* pose, std::vector<float>& local,
  std::vector<float>& world)
{
  Hierarchy const* hierarchy = GameDrawable(person);
  if (!hierarchy || bone < 0 || bone >= person.Bones || !RecordedWorld(person, *hierarchy, time, local, world))
    return false;
  PoseRows(&world[(size_t)bone * FloatsPerBone], pose);
  return true;
}

// The cloths kept, each carried by the recorded person it's on (see
// replay_gamedraws::AnchorCloths): the person drawn by the game's draws with
// the bone nearest the cloth's middle (within ClothOwnerMetres), at a time
// within their recording
void FaithReplay::AnchorClothsToPeople()
{
  const float ClothOwnerMetres = 1.f;
  static std::vector<float> local, world;   // game thread
  replay_gamedraws::AnchorCloths([this](double clock, float const* place, __int64& owner, int& bone) {
    float bestAway = ClothOwnerMetres;
    for (auto const& person : m_People)
    {
      if (person->Times.empty() || clock < person->Times.front() - 0.2 || clock > person->Times.back() + 0.2 ||
          !DrawnByGameDraws(*person))
        continue;
      Hierarchy const* hierarchy = GameDrawable(*person);
      if (!hierarchy || !RecordedWorld(*person, *hierarchy, clock, local, world))
        continue;
      for (int b = 0; b < person->Bones; ++b)
      {
        float away = Distance3(&world[(size_t)b * FloatsPerBone], place);
        if (away < bestAway)
        {
          bestAway = away;
          owner = person->Entity;
          bone = b;
        }
      }
    }
    return bestAway < ClothOwnerMetres;
  }, [this](__int64 owner, int bone, double clock, float* pose) {
    for (auto const& person : m_People)
      if (person->Entity == owner)
        return !person->Times.empty() && clock >= person->Times.front() - 0.2 && clock <= person->Times.back() + 0.2 &&
          PersonBoneAt(*person, bone, clock, pose, local, world);
    return false;
  });
}

void FaithReplay::DrawPeopleWithGameDraws(double time)
{
  std::vector<replay_gamedraws::PersonDraw> draws;
  static std::vector<float> local, world, matrices;   // game thread
  // The cloths' owners' bones wanted now (see AnchorClothsToPeople): only the people drawn now give theirs
  static std::vector<std::pair<__int64, int>> anchorsWanted;
  std::vector<replay_gamedraws::ClothAnchor> anchors;
  replay_gamedraws::ClothAnchorsWanted(anchorsWanted);
  for (auto& person : m_People)
  {
    if (person->GameDraws.empty() || !DrawnByGameDraws(*person))
      continue;
    // Within the time it was recorded, and gone from the level (a live one
    // is played on its own body)
    if (time < person->Times.front() - PersonWindowSlack || (person->Gone && time > person->Times.back() + PersonWindowSlack))
      continue;
    if (StillCharacter(person->Entity, person->VTable, person->Animatable, person->BodyOffset))
      continue;
    // Not while the game didn't draw it (hidden, gone from sight: see SeenAt)
    if (!SeenAt(*person, time))
      continue;
    if (!PersonMatricesAt(*person, time, matrices, local, world))
    {
      if (WantPoseLog(person->Entity))
        util::log::Write("Replay (diagnostic): %s 0x%I64X at %.2f s: its matrices couldn't be made from the recording (not drawn)",
          person->Template, person->Entity, time);
      continue;
    }
    for (auto const& wanted : anchorsWanted)
      if (wanted.first == person->Entity && wanted.second >= 0 && (size_t)(wanted.second + 1) * FloatsPerBone <= world.size())
      {
        replay_gamedraws::ClothAnchor anchor{ wanted.first, wanted.second, {} };
        PoseRows(&world[(size_t)wanted.second * FloatsPerBone], anchor.Pose);
        anchors.push_back(anchor);
      }
    int drawn = 0, bad = 0, minMap = (1 << 30), maxMap = 0;
    float const* firstBone = nullptr;
    float where[3];
    bool whereKnown = PersonPlaceAt(*person, time, where);
    for (auto const& d : person->GameDraws)
    {
      replay_gamedraws::PersonDraw out{ d.Id, d.HeaderRows };
      out.Instance.reserve(d.HeaderRows.size() + d.Map.size() * 12);
      bool ok = true;
      minMap = (std::min)(minMap, (int)d.Map.size());
      maxMap = (std::max)(maxMap, (int)d.Map.size());
      for (int j : d.Map)
      {
        if (j < 0 || j >= person->Bones)
        {
          ok = false;
          break;
        }
        float const* m = &matrices[(size_t)j * MatrixFloats];
        if (!firstBone)
          firstBone = m;
        out.Instance.insert(out.Instance.end(), m, m + MatrixFloats);
      }
      // Diagnostic (2026-11-03, the user: "ghost runner head is still
      // streching"; 2026-11-02's picture had sheets from a head to the far
      // left): a bone of a part drawn far from where the person is (its
      // skeleton bone more than FarBoneMetres from its bone 1, or its matrix's
      // place more than FarMatrixMetres), once a person's part
      if (ok && whereKnown)
      {
        const float FarBoneMetres = 3.f, FarMatrixMetres = 8.f;
        static std::map<std::pair<__int64, int>, bool> farLogged;   // game thread
        int farCount = 0, firstFar = -1;
        float farBone = 0.f, farMatrix = 0.f;
        for (size_t i = 0; i < d.Map.size(); ++i)
        {
          int j = d.Map[i];
          float const* m = &matrices[(size_t)j * MatrixFloats];
          float place[3] = { m[3], m[7], m[11] };
          float bone = Distance3(&world[(size_t)j * FloatsPerBone], where), matrix = Distance3(place, where);
          if (bone <= FarBoneMetres && matrix <= FarMatrixMetres)
            continue;
          ++farCount;
          if (firstFar < 0)
          {
            firstFar = (int)i;
            farBone = bone;
            farMatrix = matrix;
          }
        }
        if (farCount && farLogged.size() < 40 && farLogged.emplace(std::make_pair(person->Entity, d.Id), true).second)
          util::log::Write("Replay (diagnostic): %s 0x%I64X at %.2f s: %d of the %d bones of its part of %u indices drawn far from where "
            "it is (%.1f %.1f %.1f); the first, its bone %d (skeleton bone %d): the bone %.1f m from there, its matrix's place %.1f m",
            person->Template[0] ? person->Template : person->Label, person->Entity, time, farCount, (int)d.Map.size(), d.IndexCount, where[0],
            where[1], where[2], firstFar, d.Map[firstFar], farBone, farMatrix);
      }
      if (ok)
      {
        draws.push_back(std::move(out));
        ++drawn;
      }
      else
        ++bad;
    }
    if (WantPoseLog(person->Entity))
    {
      float place[3];
      bool known = PersonPlaceAt(*person, time, place);
      LogPose(person->Template, person->Entity, time, drawn, (int)person->GameDraws.size(), minMap == (1 << 30) ? 0 : minMap, maxMap,
        person->Bones, bad, known ? place : nullptr, firstBone);
    }
  }
  // Guns, drones and doors (stage 2b): each draw's bones a recorded bone's (a
  // door's: its transform) times its fixed transform
  if (m_GameDrawsOn && m_ModelsOn && m_ModelShapesOn)
    for (auto const& owner : m_Bound)
    {
      if (owner.Refused || owner.Draws.empty())
        continue;
      PersonTrack const* person = owner.Person;
      if (person && (time < person->Times.front() - PersonWindowSlack ||
          (person->Gone && time > person->Times.back() + PersonWindowSlack) ||
          StillCharacter(person->Entity, person->VTable, person->Animatable, person->BodyOffset) || !SeenAt(*person, time)))
        continue;
      if (!OwnerRowsAt(owner, time, true, matrices))
      {
        if (WantPoseLog(owner.Entity))
          util::log::Write("Replay (diagnostic): %s 0x%I64X at %.2f s: its rows couldn't be made from the recording (not drawn)",
            owner.Label.c_str(), owner.Entity, time);
        continue;
      }
      int bones = (int)matrices.size() / 12;
      int drawn = 0, bad = 0, minMap = (1 << 30), maxMap = 0;
      float firstBone[12];
      bool haveFirst = false;
      for (auto const& d : owner.Draws)
      {
        replay_gamedraws::PersonDraw out{ d.Id, d.HeaderRows };
        out.Instance.resize(d.HeaderRows.size() + d.Bone.size() * 12);
        bool ok = true;
        minMap = (std::min)(minMap, (int)d.Bone.size());
        maxMap = (std::max)(maxMap, (int)d.Bone.size());
        for (size_t i = 0; i < d.Bone.size() && ok; ++i)
        {
          ok = d.Bone[i] >= 0 && d.Bone[i] < bones;
          if (ok)
            Multiply3x4(&matrices[(size_t)d.Bone[i] * 12], &d.Fixed[i * 12], &out.Instance[d.HeaderRows.size() + i * 12]);
        }
        if (ok && !haveFirst && !d.Bone.empty())
        {
          memcpy(firstBone, &out.Instance[d.HeaderRows.size()], sizeof(firstBone));
          haveFirst = true;
        }
        if (ok)
        {
          draws.push_back(std::move(out));
          ++drawn;
        }
        else
          ++bad;
      }
      if (WantPoseLog(owner.Entity))
      {
        float place[3];
        bool known = person && PersonPlaceAt(*person, time, place);
        LogPose(owner.Label.c_str(), owner.Entity, time, drawn, (int)owner.Draws.size(), minMap == (1 << 30) ? 0 : minMap, maxMap, bones,
          bad, known ? place : nullptr, haveFirst ? firstBone : nullptr);
      }
    }
  replay_gamedraws::SetClothAnchors(anchors);
  replay_gamedraws::SetPeopleDraws(draws);
}

// The playback's start held while the stand-ins for the people there then
// are on their way (at most StandInWaitMs): asked, made, and played by their
// person (see BorrowForGone)
bool FaithReplay::WaitingForStandIns()
{
  if (!m_StandInWaitUntil)
    return false;
  bool waiting = GetTickCount64() < m_StandInWaitUntil && StandInsUsable();
  if (waiting && m_LastOthersScan >= 0.0)
  {
    int pending = 0;
    for (auto const& person : m_People)
      if (!person->Times.empty() && person->StandIn > 0 && m_PlayTime >= person->Times.front() - 1.0 &&
          m_PlayTime <= person->Times.back() + 1.0 &&
          !StillCharacter(person->Entity, person->VTable, person->Animatable, person->BodyOffset) &&
          util::hooks::StandInState(person->StandIn, nullptr) != util::hooks::StandInNone)
        ++pending;
    m_StandInsPending = pending;
    // The first scan asked for none (all there, or none to ask): nothing to wait for
    waiting = pending > 0;
  }
  if (!waiting)
  {
    if (m_StandInsPending > 0 || m_StandIns > 0)
      util::log::Write("Replay: the start was held %.1f s for the stand-ins (%d still on their way)",
        (GetTickCount64() - (m_StandInWaitUntil - StandInWaitMs)) / 1000.0, m_StandInsPending);
    m_StandInWaitUntil = 0;
    m_StandInsPending = 0;
  }
  return waiting;
}

// The people (and guns and drones) within OthersRadius of the replayed
// Faith who aren't in the recording (nor Faith), each once; one standing where
// a recorded person who isn't in the level any more was takes its place
// instead (see ClaimForGone)
void FaithReplay::ScanOthers(__int64 soldier, float const* faith, double time)
{
  if (!soldier)
    return;
  auto known = [this](__int64 animatable)
  {
    for (auto const& person : m_People)
      if (person->Animatable == animatable)
        return true;
    for (auto const& other : m_Others)
      if (other->Animatable == animatable)
        return true;
    return false;
  };
  // A made stand-in (and the gun it holds) is left for its recorded person (see BorrowForGone)
  std::vector<float> standIns;
  StandInPlaces(standIns);
  auto nearStandIn = [&standIns](float const* place) {
    for (size_t i = 0; i + 2 < standIns.size(); i += 3)
      if (Distance3(&standIns[i], place) <= StandInReach)
        return true;
    return false;
  };
  bool fromFaith = false;
  if (attach::FindClasses(soldier, fromFaith))
  {
    attach::View view{};
    view.Camera = XMFLOAT3(faith[0], faith[1], faith[2]);
    view.Look = XMFLOAT3(0.f, 0.f, 1.f);
    view.Skip = soldier;
    static std::vector<attach::Target> found;
    found.resize(32768);
    std::unique_ptr<attach::Scan> scan = std::make_unique<attach::Scan>();
    int count = attach::ScanNearby(view, OthersRadius, found.data(), (int)found.size(), *scan);
    for (int i = 0; i < count && m_Others.size() < MaxOthers; ++i)
    {
      attach::Target const& target = found[i];
      if (target.Agent >= 0 || target.Distance > OthersRadius || target.Entity == soldier ||
          attach::GroupOf(target) != attach::Group::People)
        continue;
      __int64 animatable = 0;
      PoseSlot slot{};
      if (!FindCharacterPose(target.Entity, animatable, slot) || IsFaithAnimatable(soldier, animatable) || known(animatable))
        continue;
      float world[RootWorldFloats];
      bool haveWorld = slot.World && slot.Bones > RootWorldBones && CopyPose(slot.World, world, RootWorldBones);
      if (haveWorld && nearStandIn(world + FloatsPerBone))
        continue;
      if (haveWorld && ClaimForGone(target.Entity, target.VTable, animatable, slot.Bones, world, CharacterBodyOffset, time))
        continue;
      std::unique_ptr<PersonTrack> other = std::make_unique<PersonTrack>();
      other->Entity = target.Entity;
      other->VTable = target.VTable;
      other->Animatable = animatable;
      other->Bones = slot.Bones;
      other->Other = true;
      strncpy_s(other->Label, attach::ClassName(target.Class), _TRUNCATE);
      m_Others.push_back(std::move(other));
    }
  }
  static ActorFound found[256];
  int withoutBody = 0;
  __int64 firstWithoutBody = 0;
  int count = FindActors(faith, OthersRadius, OthersRadius, found, (int)_countof(found), withoutBody, firstWithoutBody);
  for (int i = 0; i < count && m_Others.size() < MaxOthers; ++i)
  {
    __int64 animatable = 0;
    PoseSlot slot{};
    if (!FindCharacterPose(found[i].Entity, animatable, slot, ActorBodyOffset) || animatable != found[i].Animatable ||
        known(animatable))
      continue;
    float world[RootWorldFloats];
    bool haveWorld = slot.World && slot.Bones > RootWorldBones && CopyPose(slot.World, world, RootWorldBones);
    if (haveWorld && nearStandIn(world + FloatsPerBone))
      continue;
    if (haveWorld && ClaimForGone(found[i].Entity, found[i].VTable, animatable, slot.Bones, world, ActorBodyOffset, time))
      continue;
    std::unique_ptr<PersonTrack> other = std::make_unique<PersonTrack>();
    other->Entity = found[i].Entity;
    other->VTable = found[i].VTable;
    other->Animatable = animatable;
    other->Bones = slot.Bones;
    other->BodyOffset = ActorBodyOffset;
    other->Other = true;
    strncpy_s(other->Label, ActorClasses[found[i].Class].Name, _TRUNCATE);
    m_Others.push_back(std::move(other));
  }
}

// At each of her samples: a scan for people now and then, and every person
// found so far, while they're still there
// Diagnostic (2026-11-01, the user: "Icarus ... his mouth bones seem wrong",
// "Character(s) bones don't seem correct now"): a person's world pose as the
// replay makes it from the recording (RecordedWorld: bones 0 - 3 as recorded,
// the others' local pose under their parent) against the game's own world
// pose this frame, every bone; and the game's matrices against its world pose
// times the fixed transforms (what the replay draws with). Twice a person
// (1 s and 20 s into its track). Game thread.
static bool CopyRawBytes(void const* source, void* target, size_t bytes)
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

static void CheckRecordedPose(FaithReplay::PersonTrack& person, PoseSlot const& slot, double time)
{
  Hierarchy const* hierarchy = GameDrawable(person);
  if (!hierarchy || !slot.World)
    return;
  ++person.PoseChecks;
  static std::vector<float> local, world, game, matrices;   // game thread
  game.resize((size_t)person.Bones * FloatsPerBone);
  if (!RecordedWorld(person, *hierarchy, time, local, world) || !CopyPose(slot.World, game.data(), person.Bones))
    return;
  int off = 0, worstBone = -1;
  float worst = 0.f;
  char list[400] = "";
  size_t at = 0;
  for (int i = RootWorldBones; i < person.Bones; ++i)
  {
    float const* a = &world[(size_t)i * FloatsPerBone];
    float const* b = &game[(size_t)i * FloatsPerBone];
    float dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
    float metres = sqrtf(dx * dx + dy * dy + dz * dz);
    float dot = fabsf(a[4] * b[4] + a[5] * b[5] + a[6] * b[6] + a[7] * b[7]);
    if (metres <= 0.005f && dot >= 0.999f)
      continue;
    ++off;
    if (metres > worst)
    {
      worst = metres;
      worstBone = i;
    }
    if (at + 32 < sizeof(list))
      at += sprintf_s(list + at, sizeof(list) - at, " %d (from %d: %.1f cm, dot %.3f)", i, hierarchy->Parent[i], metres * 100.f, dot);
  }
  // The game's matrices against its world pose times the fixed transforms
  int matricesOff = 0;
  float matricesWorst = 0.f;
  RawSlot raw;
  if (FindRawSlot(slot, raw) && raw.Buffer[RawMatrices] && raw.Bytes[RawMatrices] >= person.Bones * MatrixFloats * 4)
  {
    matrices.resize((size_t)person.Bones * MatrixFloats);
    MatricesOf(game.data(), person.Unbind.data(), person.Bones, matrices.data());
    std::vector<float> theirs((size_t)person.Bones * MatrixFloats);
    if (!CopyRawBytes(raw.Buffer[RawMatrices], theirs.data(), theirs.size() * sizeof(float)))
      theirs.clear();
    for (int i = 0; i < person.Bones && !theirs.empty(); ++i)
    {
      float metres, rotation;
      MatrixError(&matrices[(size_t)i * MatrixFloats], &theirs[(size_t)i * MatrixFloats], metres, rotation);
      if (metres > 0.005f || rotation > 0.02f)
        ++matricesOff;
      matricesWorst = (std::max)(matricesWorst, metres);
    }
  }
  util::log::Write("Replay (diagnostic): %s 0x%I64X at %.2f s: its world pose made from the recording against the game's own: %d of "
    "its %d bones off by more than 5 mm or turned (the most: bone %d, %.1f cm):%s; the game's matrices against its own world pose "
    "times the fixed transforms: %d off (the most %.1f cm)", person.Label, person.Entity, time, off, person.Bones - RootWorldBones,
    worstBone, worst * 100.f, off ? list : " none", matricesOff, matricesWorst * 100.f);
}

void FaithReplay::RecordPeople(__int64 soldier, float const* faith, double time)
{
  if (m_LastPeopleScan < 0 || time - m_LastPeopleScan >= PeopleScanInterval)
  {
    ScanPeople(soldier, faith);
    m_LastPeopleScan = time;
  }
  // The PC's own memory, now and then (see MinFreeCommit)
  if (!m_PeopleFull && (m_CommitCheckedAt < 0 || time < m_CommitCheckedAt || time - m_CommitCheckedAt >= CommitCheckInterval))
  {
    m_CommitCheckedAt = time;
    unsigned long long freeCommit = FreeCommit();
    if (freeCommit < MinFreeCommit)
    {
      util::log::Warning("Replay: Windows can commit only %.1f GB more for the game, the people near Faith are held from here on "
        "(%.1f MB of them stored)", freeCommit / 1073741824.0, m_PeopleBytes / 1048576.0);
      m_PeopleFull = true;
    }
  }
  static std::vector<replay_drawlog::PersonPlace> drawLogPeople;   // this thread's only
  drawLogPeople.clear();
  for (auto& person : m_People)
  {
    if (person->Gone)
      continue;
    __int64 animatable = 0;
    PoseSlot slot{};
    if (!StillCharacter(person->Entity, person->VTable, person->Animatable, person->BodyOffset) ||
        !FindCharacterPose(person->Entity, animatable, slot, person->BodyOffset) || animatable != person->Animatable ||
        slot.Bones != person->Bones)
    {
      person->Gone = true;
      m_PeopleBytes += FlushPending(*person);
      util::log::Write("Replay:   %s 0x%I64X went at %.2f s", person->Label, person->Entity, time);
      continue;
    }
    size_t frameFloats = (size_t)person->Bones * FloatsPerBone;
    size_t bytes = PersonFrameBytes(*person);
    if (m_PeopleFull || m_PeopleBytes + 2 * bytes > MaxPeopleBytes)
    {
      if (!m_PeopleFull)
        util::log::Warning("Replay: the memory for the people near Faith (%d MB) is used up, they're held from here on",
          (int)(MaxPeopleBytes >> 20));
      m_PeopleFull = true;
      return;
    }
    // Their own pose (from their world pose once the game doesn't update
    // them, if that's where they move; see OwnPose), and bones 0 - 3 of their
    // world pose (for posing them while the game doesn't update them; see
    // DrawBody)
    static std::vector<float> frame;
    frame.resize(frameFloats);
    OwnPoseInfo info;
    if (!OwnPose(slot, person->Animatable, frame.data(), true, false, info))
      continue;
    // A drone: where the game draws it (see RecordDronePlace)
    if (IsDrone(*person))
      RecordDronePlace(*person, frame.data(), time);
    float root[RootWorldFloats] = {};
    if (person->RootWorldOk && !(person->Bones > RootWorldBones && slot.World && CopyPose(slot.World, root, RootWorldBones)))
    {
      person->RootWorldOk = false;
      person->RootWorld.clear();
      person->RootWorld.shrink_to_fit();
    }
    NoteRecordedBody(*person, slot, info, time);
    // Its skeleton, for the game's own draws of it (stage 2, AdoptGameDraws):
    // its pose manager, and its matrices' fixed transforms (looked for until
    // read; a torn read fails ReadUnbind's checks and is tried again)
    person->Manager = slot.Manager;
    if (person->Unbind.empty() && person->UnbindTries < MaxUnbindTries)
    {
      ++person->UnbindTries;
      RawSlot raw;
      std::vector<float> unbind((size_t)slot.Bones * MatrixFloats);
      if (FindRawSlot(slot, raw) && ReadUnbind(raw, slot.Bones, unbind.data()))
      {
        // A read while the game wrote it can still be rotations: kept once two frames' agree
        float worst = person->UnbindSeen.size() == unbind.size() ? 0.f : 1e30f;
        for (size_t k = 0; k < unbind.size() && worst <= UnbindTolerance; ++k)
          worst = (std::max)(worst, fabsf(unbind[k] - person->UnbindSeen[k]));
        if (worst <= UnbindTolerance)
        {
          person->Unbind.swap(unbind);
          person->UnbindSeen.clear();
        }
        else
          person->UnbindSeen.swap(unbind);
      }
    }
    // Diagnostic (temporary): where they are for the draw log's snapshot (its bone-reading draws)
    if (person->RootWorldOk && replay_drawlog::WantsPeople())
    {
      replay_drawlog::PersonPlace place{};
      place.Entity = person->Entity;
      strncpy_s(place.Label, person->Label, _TRUNCATE);
      memcpy(place.Place, root + FloatsPerBone, sizeof(place.Place));
      drawLogPeople.push_back(place);
    }
    // A character: the template of the spawner that made it (the server
    // character of a spawner nearest its bone 1, within TemplateReach; the
    // hooks note them once a second), so a stand-in of its kind can be had
    // once it's gone (see BorrowForGone)
    if (!person->Template[0] && person->BodyOffset == CharacterBodyOffset && person->RootWorldOk &&
        util::hooks::SpawnTemplateNear(root + 1 * FloatsPerBone, TemplateReach, person->Template, sizeof(person->Template)))
      util::log::Write("Replay:   %s 0x%I64X: made from %s", person->Label, person->Entity, person->Template);
    // The same as the frame stored before (a body lying still): not stored
    // again, only the run's last one at its end (FlushPending)
    size_t count = person->Times.size();
    if (count && person->Recorded.SameAsLast(frame.data()) &&
        (!person->RootWorldOk || !memcmp(root, &person->RootWorld[(count - 1) * RootWorldFloats], sizeof(root))))
    {
      person->PendingTime = time;
      continue;
    }
    m_PeopleBytes += FlushPending(*person);
    person->Moved = person->Moved || count > 0;
    try
    {
      person->Recorded.Append(frame.data(), person->Bones);
      if (person->RootWorldOk)
        person->RootWorld.insert(person->RootWorld.end(), root, root + RootWorldFloats);
      person->Times.push_back(time);
    }
    catch (std::bad_alloc const&)
    {
      TrimPerson(*person);
      util::log::Warning("Replay: no more memory could be had for the people near Faith (%.1f MB of them stored), they're held "
        "from here on", m_PeopleBytes / 1048576.0);
      m_PeopleFull = true;
      return;
    }
    m_PeopleBytes += PersonFrameBytes(*person);
    // Diagnostic: its pose as the replay makes it against the game's (see
    // CheckRecordedPose; the first 1 s in: the ghost runners of 2026-11-03's
    // test were there 1.9 - 2.6 s, never checked at 3 s)
    if (person->BodyOffset == CharacterBodyOffset && person->PoseChecks < 2 && person->Times.size() > 1 &&
        time - person->Times.front() >= (person->PoseChecks ? 20.0 : 1.0))
      CheckRecordedPose(*person, slot, time);
    // A gun's or drone's whole world pose with each stored frame (stage 2b, see AppendWorld)
    if (person->BodyOffset == ActorBodyOffset && person->WorldOk)
      AppendWorld(*person, slot);
    // A drone: its server entity, found by place once (the nearest within
    // DroneServerDistance), so it's kept in the level by who it is. Kept by
    // place it went (2026-10-24 18:52: it flew on from its last recorded place
    // after the recording and was taken out before the playback).
    if (!person->Server && person->BodyOffset == ActorBodyOffset && !strcmp(person->Label, ActorClasses[1].Name))
    {
      float const* place = person->Recorded.Exact(person->Recorded.Count() - 1) + 1 * FloatsPerBone;
      person->Server = FindServerEntityNear(DroneServerTypeInfoRva, place, DroneServerDistance);
      if (person->Server)
        util::log::Write("Replay:   %s 0x%I64X: its server entity 0x%I64X, kept in the level by who it is", person->Label,
          person->Entity, person->Server);
    }
  }
  if (!drawLogPeople.empty())
    replay_drawlog::NotePeople(drawLogPeople);
}

// Each person's recorded pose on their own body (from their first sample on;
// before it, and after their last, held)
void FaithReplay::PlayPeople(double time, __int64 soldier, float const* faith)
{
  // Guns still held from the playback before: let go first, held again below if they're away
  ReleaseGunTail(true);
  int shown = 0, gone = 0, noSlot = 0;
  bool diagnose = time - m_PeopleLogAt >= 2.0 || time < m_PeopleLogAt;
  if (diagnose)
  {
    m_PeopleLogAt = time;
    util::log::Write("Replay: %.1f s: what became of the people's poses last frame:", time);
    // Diagnostic (temporary): Faith's body, one the game always updates, for
    // reference (see ReadRenderEntry)
    __int64 faithAnimatable = 0;
    PoseSlot faithSlot{};
    if (soldier && FindCharacterPose(soldier, faithAnimatable, faithSlot))
      LogRenderEntry("Faith", soldier, "the game's, for reference", faithAnimatable, faithSlot);
  }
  for (auto& person : m_People)
  {
    if (person->Times.empty())
      continue;
    __int64 animatable = 0;
    PoseSlot slot{};
    bool there = StillCharacter(person->Entity, person->VTable, person->Animatable, person->BodyOffset);
    bool ok = there && FindCharacterPose(person->Entity, animatable, slot, person->BodyOffset) &&
      animatable == person->Animatable && slot.Bones == person->Bones;
    gone += !there;
    noSlot += there && !ok;
    // What happened to each, once a playback, then when the counts change
    if (!m_PeopleLogged)
      util::log::Write("Replay:   %s 0x%I64X: %s", person->Label, person->Entity,
        !there ? "gone (not in the level any more)" : !ok ? "there, but without a pose slot" : "posed");
    // Taken out of a slot that moved, or of a person who went
    if (person->Registered && (!ok || slot.Overrides != person->RegisteredOverrides || slot.Handle != person->RegisteredHandle))
    {
      UnregisterOverride(person->Set, person->RegisteredOverrides, person->RegisteredHandle);
      person->Registered = false;
    }
    if (!ok)
    {
      DropBox(*person);
      HoldUpdate(*person, false);
      person->GunHeld = false;
      PlaceGun(*person, false, nullptr);
      if (there)
        PutBackEntity(*person);
      person->EntityMoved = false;   // gone: its memory may be another's soon
      if (person->ForcedBody)
        util::hooks::ReleaseBodyUpdate(person->ForcedBody, there);
      person->ForcedBody = 0;
    }
    // A gun or drone that went: let go at once (its memory may be another's soon)
    if (!there && person->ActorHeld)
    {
      util::hooks::HoldActor(person->Entity, false);
      person->ActorHeld = false;
    }
    if (!ok || !PrepareOverride(person->Set, person->Bones))
      continue;
    bool justTaken = !person->Taken;
    if (justTaken)
      TakeOver(*person, slot);
    // A body the game updates and the override poses, but whose world pose
    // stays where the live one really is (2026-10-27 03:23: the free-roam
    // Icarus playing the mission's was 893 m from the replayed place at every
    // check while every other borrowed body was on it): after StuckFrames
    // frames more than StuckDistance off, the replay draws it itself (see
    // KeepTaken), its client entity following
    // Only within its recorded time: before and after it, its pose is parked
    // out of sight on purpose (2026-10-27 03:49: six were counted 10 km off
    // just before their time)
    if (person->Registered && !person->Frozen && !person->Stuck && person->BodyOffset == CharacterBodyOffset &&
        time >= person->Times.front() && time <= person->Times.back())
    {
      float worldRoot[3], matrixRoot[12], position[3];
      PoseOverride const* last = &person->Set.Override[person->Buffer ^ 1];
      if (ReadPersonWorld(slot, worldRoot, matrixRoot, position, person->Entity) &&
          Distance3(worldRoot, last->Bones + 1 * FloatsPerBone) > StuckDistance)
      {
        if (++person->StuckFrames >= StuckFrames)
        {
          person->Stuck = true;
          util::log::Write("Replay:   %s 0x%I64X: posed, but the game keeps its body where it really is (%.0f m from the replayed "
            "place for %d frames): the replay draws it itself", person->Label, person->Entity,
            Distance3(worldRoot, last->Bones + 1 * FloatsPerBone), StuckFrames);
        }
      }
      else
        person->StuckFrames = 0;
    }
    BodyMode mode = KeepTaken(*person, slot, justTaken);
    // Diagnostic (temporary), every 2 s: whether last frame's override is
    // still in the slot, whether the body ended up where it put it, and
    // whether the game updated it
    if (diagnose && person->Registered)
    {
      __int64 entry = ReadOverrideEntry(slot);
      PoseOverride const* last = &person->Set.Override[person->Buffer ^ 1];
      char const* state = entry == (__int64)&person->Set.Override[0] || entry == (__int64)&person->Set.Override[1] ? "ours" :
        entry == 0 ? "empty" : "another";
      BodyGates gates{ -1, -1, -1, -1, -1, -1 };
      ReadBodyGates(person->Animatable, gates);
      float worldRoot[3], matrixRoot[12], position[3];
      if (ReadPersonWorld(slot, worldRoot, matrixRoot, position, person->Entity))
        util::log::Write("Replay:   0x%I64X: slot entry %s (0x%I64X); body (world bone 1) %.2f m from the replayed one, its matrix row "
          "%.1f %.1f %.1f / %.1f %.1f %.1f / %.1f %.1f %.1f / %.1f %.1f %.1f; the real one at %.1f %.1f %.1f, %.1f m away; "
          "updated last frame %d, pose update %d, always update %d%s, mode %d", person->Entity, state, entry,
          Distance3(worldRoot, last->Bones + 1 * FloatsPerBone), matrixRoot[0], matrixRoot[1], matrixRoot[2], matrixRoot[3],
          matrixRoot[4], matrixRoot[5], matrixRoot[6], matrixRoot[7], matrixRoot[8], matrixRoot[9], matrixRoot[10],
          matrixRoot[11], position[0], position[1], position[2], Distance3(position, last->Bones + 1 * FloatsPerBone),
          gates.Updated, gates.PoseUpdate, gates.Always, "", gates.Mode);
    }
    // Diagnostic (temporary), every 2 s: the slot's render-side entry (see
    // ReadRenderEntry) against its world pose and matrices, as last frame left
    // them, for the bodies the replay draws itself and the ones the game updates
    if (diagnose)
      LogRenderEntry(person->Label, person->Entity, person->HeldBody ? "drawn by the replay, update held" :
        person->ForcedBody ? "the game's, switched on for the playback" : "the game's", person->Animatable, slot);
    // One the game doesn't update that the replay can't pose is left alone
    // (see KeepTaken)
    if (mode == BodyMode::Left)
    {
      if (person->Registered)
      {
        UnregisterOverride(person->Set, person->RegisteredOverrides, person->RegisteredHandle);
        person->Registered = false;
      }
      DropBox(*person);
      HoldUpdate(*person, false);
      PlaceGun(*person, false, nullptr);
      PutBackEntity(*person);
      continue;
    }
    PoseOverride* pOverride = &person->Set.Override[person->Buffer];
    // Only within the time they were recorded: before they were first seen,
    // and after they went (taken out of the level while it recorded), hidden
    bool before = time < person->Times.front() - PersonWindowSlack;
    bool after = person->Gone && time > person->Times.back() + PersonWindowSlack;
    if (before || after)
    {
      static std::vector<float> shapeFrame;   // game thread
      shapeFrame.resize((size_t)person->Bones * FloatsPerBone);
      person->Recorded.Decode(before ? 0 : person->Recorded.Count() - 1, shapeFrame.data());
      float const* shape = shapeFrame.data();
      // A character keeps its own root (see ParkPoseKeepingRoot); a gun or
      // drone goes down with its entity (MoveActor puts it back at stop)
      if (person->BodyOffset != CharacterBodyOffset || !ParkPoseKeepingRoot(shape, slot, pOverride->Bones, person->Bones))
        ParkPose(shape, pOverride->Bones, person->Bones);
    }
    else
      SamplePacked(person->Times, person->Recorded, person->Bones, time, pOverride->Bones);
    // A gun or drone: its entity goes where the replay has it (its body follows
    // it). Not a gun whose update is held (see KeepGun): the override places
    // its body, and its entity moved every frame against its physics made a
    // dead KrugerSec's gun flicker between his hand and the ground, and the
    // guns had fallen through the floor by the next playback (2026-10-24 19:3x);
    // the game moves its entity itself meanwhile (its moves aren't held back)
    bool droneMoved = false;
    if (person->BodyOffset == ActorBodyOffset && person->GunHeld)
    {
      if (person->ActorHeld)
      {
        util::hooks::HoldActor(person->Entity, false);
        person->ActorHeld = false;
      }
    }
    else if (person->BodyOffset == ActorBodyOffset && person->ActorBeforeOk && MoveActor(*person, pOverride->Bones))
    {
      CheckActor(*person, slot, pOverride->Bones);
      droneMoved = IsDrone(*person);
    }
    // The game doesn't update it: the replay writes it itself (the override
    // stays in its slot for a frame the game does)
    float drawnRoot[FloatsPerBone];
    float cullBox[util::hooks::CullBoxFloats];
    bool cullBoxOk = false;
    bool drawn = mode == BodyMode::Drawn && DrawBody(*person, slot, pOverride->Bones, time, before || after, drawnRoot, cullBox,
      cullBoxOk);
    HoldUpdate(*person, drawn);
    // Its culling box with it (the game's own stays where it lies)
    KeepCullBox(*person, drawn && cullBoxOk ? cullBox : nullptr);
    // A gun away from where the game has it: placed where the replay has it
    float gunRootBuffer[FloatsPerBone];
    float const* gunRoot = GunRoot(*person, time, before || after, pOverride->Bones, gunRootBuffer);
    // A drone with its entity: its placement too (see PlaceGun)
    if (droneMoved)
      PlaceGun(*person, true, pOverride->Bones + 1 * FloatsPerBone);
    else
      PlaceGun(*person, !drawn && KeepGun(*person, slot, gunRoot), gunRoot);
    // Its client entity with it (see TakeEntity)
    if (drawn)
      FollowEntity(*person, drawnRoot);
    if (mode == BodyMode::Drawn && !drawn && !person->DrawFailLogged)
    {
      person->DrawFailLogged = true;
      util::log::Warning("Replay:   %s 0x%I64X: its pose buffers couldn't be written at %.1f s", person->Label, person->Entity, time);
    }
    if (RegisterOverride(slot, pOverride))
    {
      person->Registered = true;
      person->RegisteredOverrides = slot.Overrides;
      person->RegisteredHandle = slot.Handle;
      person->Buffer ^= 1;
      shown += !before && !after;
      // A character's bounding box goes with it (see FindPersonBox)
      if (person->BodyOffset == CharacterBodyOffset)
        PoseBox(*person, slot, pOverride->Bones, diagnose);
    }
    else
      DropBox(*person);
  }

  // Everyone else near the replayed Faith, hidden (and while the start is held
  // for the stand-ins, its time standing still, every StandInWaitScanMs)
  bool waitScan = m_StandInWaitUntil && GetTickCount64() - m_WaitScanAt >= StandInWaitScanMs;
  if (faith && (m_LastOthersScan < 0 || time - m_LastOthersScan >= OthersScanInterval || time < m_LastOthersScan || waitScan))
  {
    ScanOthers(soldier, faith, time);
    // Those still gone: live ones of their own data or template from elsewhere (see BorrowForGone)
    if (gone > 0)
    {
      BorrowForGone(soldier, faith, time);
      AskStandInsAhead(time);
    }
    m_LastOthersScan = time;
    m_WaitScanAt = GetTickCount64();
  }
  int hidden = 0, othersParked = 0;
  for (auto& other : m_Others)
  {
    __int64 animatable = 0;
    PoseSlot slot{};
    bool ok = StillCharacter(other->Entity, other->VTable, other->Animatable, other->BodyOffset) &&
      FindCharacterPose(other->Entity, animatable, slot, other->BodyOffset) && animatable == other->Animatable &&
      slot.Bones == other->Bones;
    if (other->Registered && (!ok || slot.Overrides != other->RegisteredOverrides || slot.Handle != other->RegisteredHandle))
    {
      UnregisterOverride(other->Set, other->RegisteredOverrides, other->RegisteredHandle);
      other->Registered = false;
    }
    if (!ok)
    {
      HoldUpdate(*other, false);
      PutBackEntity(*other);   // (only if it's still that character)
      other->EntityMoved = false;
      if (other->ForcedBody)
        util::hooks::ReleaseBodyUpdate(other->ForcedBody,
          StillCharacter(other->Entity, other->VTable, other->Animatable, other->BodyOffset));
      other->ForcedBody = 0;
    }
    if (!ok && other->ActorHeld && !StillCharacter(other->Entity, other->VTable, other->Animatable, other->BodyOffset))
    {
      util::hooks::HoldActor(other->Entity, false);
      other->ActorHeld = false;
    }
    if (!ok || !PrepareOverride(other->Set, other->Bones))
      continue;
    // Their own pose as it is when first hidden, kept (only its place changes)
    bool justTaken = other->Times.empty();
    if (justTaken)
    {
      if (!TakeOver(*other, slot))
        continue;
      other->Frames = other->Before;
      other->Times.push_back(time);
    }
    BodyMode mode = KeepTaken(*other, slot, justTaken);
    if (mode == BodyMode::Left)
    {
      if (other->Registered)
      {
        UnregisterOverride(other->Set, other->RegisteredOverrides, other->RegisteredHandle);
        other->Registered = false;
      }
      HoldUpdate(*other, false);
      PutBackEntity(*other);
      continue;
    }
    // Diagnostic (temporary), every 2 s: whether last frame's put them out of sight
    float root[3];
    if (diagnose && other->Registered && ReadWorldRoot(slot, root))
      othersParked += root[1] < -ParkDrop / 2;
    PoseOverride* pOverride = &other->Set.Override[other->Buffer];
    if (other->BodyOffset != CharacterBodyOffset || !ParkPoseKeepingRoot(other->Frames.data(), slot, pOverride->Bones, other->Bones))
      ParkPose(other->Frames.data(), pOverride->Bones, other->Bones);
    // A gun or drone: its entity goes down too (its body goes where it is)
    if (other->BodyOffset == ActorBodyOffset && other->ActorBeforeOk)
      MoveActor(*other, pOverride->Bones);
    bool parked = mode == BodyMode::Drawn && DrawParked(*other, slot);
    HoldUpdate(*other, parked);
    // Its client entity down with it (see TakeEntity)
    if (parked)
      HideEntity(*other);
    if (mode == BodyMode::Drawn && !parked && !other->DrawFailLogged)
    {
      other->DrawFailLogged = true;
      util::log::Warning("Replay:   %s 0x%I64X: its pose buffers couldn't be written at %.1f s", other->Label, other->Entity, time);
    }
    if (RegisterOverride(slot, pOverride))
    {
      other->Registered = true;
      other->RegisteredOverrides = slot.Overrides;
      other->RegisteredHandle = slot.Handle;
      other->Buffer ^= 1;
      ++hidden;
    }
  }
  if (hidden != m_OthersHidden)
  {
    m_OthersHidden = hidden;
    util::log::Write("Replay: %.1f s: %d people, guns or drones that aren't in the recording hidden near the replayed Faith", time,
      hidden);
  }
  if (diagnose && hidden)
    util::log::Write("Replay: %.1f s: %d of the hidden ones were out of sight last frame (their bodies %.0f m down)", time,
      othersParked, ParkDrop);
  m_PeopleLogged = true;
  if (shown != m_PeopleShown || gone != m_PeopleGone || noSlot != m_PeopleNoSlot)
  {
    m_PeopleShown = shown;
    m_PeopleGone = gone;
    m_PeopleNoSlot = noSlot;
    util::log::Write("Replay: %.1f s: %d people posed, %d gone, %d without a pose slot", time, shown, gone, noSlot);
    // None of them there (2026-10-26: a recording made in the Release mission
    // played after the mission was over: 37 gone, 0 of 29 moving models there,
    // the game had taken the mission's containers out whole, 4096 entities,
    // through 0x1434691BC < 0x14323B47A < 0x1434943BA < 0x14323B359 <
    // 0x1431A337D, as Faith was put back in free roam). The replay poses what
    // still exists; a mission's world comes back only through the game's own
    // mission start (its progress is saved, so the tools don't set it)
    // Where it was recorded, when it crossed a loading screen (see
    // NoteRecordedLoads). strncpy_s / sprintf_s into the 512 bytes: strcpy_s
    // ends the game when the text is longer than the buffer (2026-10-26 22:41:
    // 298 bytes into 160 crashed the game at the playback's first frame)
    char where[192];
    if (WhereRecorded(where, sizeof(where))[0])
      sprintf_s(m_WorldMessage, "The people in this recording aren't in the level now: it was recorded in %s. Start that again "
        "from the game's menu, wait until it's loaded, then press Play: people standing where the recorded ones were play them.",
        where);
    else
      strncpy_s(m_WorldMessage, "The people in this recording aren't in the level any more: the mission or part of the city "
        "they were in was unloaded (a mission that's over, a fast travel). Play it before leaving the mission, or start the "
        "mission again from the game's menu: people standing where the recorded ones were play them.", _TRUNCATE);
    if (shown == 0 && gone > 0 && !m_Message[0])
    {
      strncpy_s(m_Message, m_WorldMessage, _TRUNCATE);
      // Diagnostic (temporary): which spawners could make stand-ins
      util::hooks::LogSpawners("the recorded people aren't in the level");
    }
    else if (shown > 0 && !strcmp(m_Message, m_WorldMessage))
      m_Message[0] = 0;   // found again since (see ClaimForGone)
  }
}

// While the replay holds people (recording them, or a recording with them),
// the game doesn't take AI out of the level; while it plays them,
// DisableModelAnimationCulling is set (see AntSettingsCacheRva). How long the
// hold lasts while Faith is gone (her death took 8 s; see KeepPeople):
const double DeathHoldSeconds = 30.0;
void FaithReplay::KeepPeople(bool faithThere)
{
  // While a recording with people plays. While it records, and between the
  // two unless asked not to (kept KrugerSec stay in the world and are heard
  // near them, 2026-10-25: 18 the game wanted gone after the fight), only when
  // the people are played on the game's own bodies alone: drawn with the
  // game's draws of them (DrawnByGameDraws), one the game takes out is drawn
  // from those up to when it went. 2026-10-28: the hold kept the Release
  // mission's ghost runners (SpawnTemplate_Projection_Replay), its runners
  // and its opening's people in the level while the user played it (9 the
  // game asked to take out, and the sub-level the Icarus runner was in)
  bool recorded = !m_People.empty() || m_Vehicles.Count() > 0;
  bool bodiesOnly = !(m_GameDrawsOn && m_ModelsOn && m_ModelShapesOn);
  bool wanted = m_PeopleOn && ((bodiesOnly && m_State == State_Recording) ||
    (recorded && (m_State == State_Playing || (bodiesOnly && m_KeepPeople))));
  // Faith gone while it held them: a death. The game then unloads the fight's
  // sub-levels, which took every kept KrugerSec with them (2026-10-24 17:33:21,
  // "destroyed another way" right after the hold ended), while the level
  // itself stays (39 of 50 recorded models still there after, Faith back 8 s
  // later). The hold goes on through it: a refused unload is only skipped
  // and asked again (0x1432F2246), other loads go on. A level unloading
  // can't be told apart from a death yet, so it lasts DeathHoldSeconds at most.
  double now = NowSeconds();
  if (faithThere)
  {
    if (m_FaithGoneAt >= 0)
      util::log::Write("Replay: Faith back after %.1f s; the people %s", now - m_FaithGoneAt,
        m_GoneHoldEnded ? "were let go meanwhile" : "were held through it");
    m_FaithGoneAt = -1.0;
    m_GoneHoldEnded = false;
  }
  else if (m_FaithGoneAt < 0 && m_PeopleHeld && wanted)
  {
    m_FaithGoneAt = now;
    util::log::Write("Replay: Faith is gone (dead or loading): the people stay held up to %.0f s for her to come back",
      DeathHoldSeconds);
  }
  bool gone = !faithThere && m_FaithGoneAt >= 0 && !m_GoneHoldEnded;
  if (gone && now - m_FaithGoneAt >= DeathHoldSeconds)
  {
    gone = false;
    m_GoneHoldEnded = true;
    util::log::Write("Replay: Faith still gone after %.0f s: the people are let go", DeathHoldSeconds);
  }
  // A loading screen up long enough (a fast travel) waits on what's kept: let
  // go (see util::hooks::LoadScreenWaiting)
  bool loading = util::hooks::LoadScreenWaiting();
  if (loading && !m_LoadingLetGo && m_PeopleHeld)
  {
    util::log::Write("Replay: a loading screen has been up a while and waits on what the replay keeps: the people are let go");
    // Their copies can't be kept past it either (the game deletes them as
    // their areas go, and keeping them would block the load too, or run them
    // with no game data behind them; 2026-10-24, see the notes)
    if (!m_People.empty())
      strcpy_s(m_Message, "A loading screen took this recording's KrugerSec out of the level: it plays without them now.");
  }
  m_LoadingLetGo = loading;
  bool hold = (faithThere || gone) && wanted && !loading;
  m_PeopleHeld = hold;
  util::hooks::HoldAIDespawn(hold);
  if (hold)
    ShareGuardPlaces();
  // Nothing in the live game hurts the real Faith while a replay plays
  util::hooks::ProtectFaith(m_State == State_Playing);
  SetAnimationCulling(!(m_PeopleOn && m_State == State_Playing && !m_People.empty()));
}

// Where the recorded people are, for the hooks to keep them in the level
// (see GuardKeepRadius)
void FaithReplay::ShareGuardPlaces()
{
  bool playing = m_State == State_Playing;
  if (!playing)
  {
    m_GuardPlaces.clear();
    for (auto const& person : m_People)
    {
      // People and drones (the hooks keep their server entities); guns aren't kept that way
      bool drone = person->BodyOffset == ActorBodyOffset && !strcmp(person->Label, ActorClasses[1].Name);
      if (person->Gone || (person->BodyOffset != CharacterBodyOffset && !drone))
        continue;
      float place[3];
      if (StillCharacter(person->Entity, person->VTable, person->Animatable, person->BodyOffset) && EntityPlace(person->Entity, place))
        m_GuardPlaces.insert(m_GuardPlaces.end(), place, place + 3);
      // Bone 1 of each recorded pose is where they were (a drone's too: its
      // body's bone 1 is where its entity is)
      double next = -1.0;
      for (size_t k = 0; k < person->Times.size(); ++k)
      {
        bool last = k + 1 == person->Times.size();
        if (person->Times[k] < next && !last)
          continue;
        next = person->Times[k] + GuardPlaceInterval;
        float const* root = person->Recorded.Exact(k) + 1 * FloatsPerBone;
        m_GuardPlaces.insert(m_GuardPlaces.end(), root, root + 3);
      }
    }
  }
  util::hooks::SetGuardPlaces(m_GuardPlaces.data(), (int)(m_GuardPlaces.size() / 3),
    playing ? GuardKeepRadiusPlaying : GuardKeepRadius);
  // The recorded vehicles' server entities, by who they are
  // The recorded vehicles' and drones' server entities, by who they are (a
  // drone only while its client entity is still there)
  __int64 servers[16 + MaxActors];
  int count = m_Vehicles.ServerEntities(servers, 16);
  for (auto const& person : m_People)
    if (person->Server && count < (int)_countof(servers) && !person->Gone &&
        StillCharacter(person->Entity, person->VTable, person->Animatable, person->BodyOffset))
      servers[count++] = person->Server;
  util::hooks::SetKeptVehicles(servers, count);
}

void FaithReplay::SetAnimationCulling(bool on)
{
  if (on == !m_CullingOff)
    return;
  __try
  {
    BYTE* pSettings = *(BYTE**)((__int64)g_gameHandle + AntSettingsCacheRva);
    if (!pSettings)
      return; // the game hasn't looked them up yet: tried again next frame
    BYTE* pFlag = pSettings + DisableModelAnimationCullingOffset;
    if (!on)
    {
      m_CullingOriginal = *pFlag;
      *pFlag = 1;
      m_CullingOff = true;
    }
    else
    {
      *pFlag = m_CullingOriginal;
      m_CullingOff = false;
    }
  }
  __except (EXCEPTION_EXECUTE_HANDLER)
  {
  }
}

void FaithReplay::StopPeople()
{
  // The vehicles back where they were, let go
  m_Vehicles.StopPlaying();
  // A gun whose update was held keeps the hold a few frames with no override
  // (taken out below), so the game's pose puts its own back (where the game
  // has it) before it's let go (ReleaseGunTail)
  std::vector<GunTail> tail;
  for (auto& person : m_People)
    if (person->GunHeld && person->HeldBody)
      tail.push_back({ person->HeldBody, person->Entity, person->VTable, person->Animatable, GunTailFrames,
        { person->GunReal[0], person->GunReal[1], person->GunReal[2] } });
  // The game updates the bodies the replay drew itself again (their buffers
  // are put back below, as they were when taken)
  int updatesHeld = util::hooks::ReleaseBodyUpdates();
  // The guns it placed are placed by the game again
  util::hooks::ReleasePlacedBodies();
  // The culling boxes of the bodies it drew are the game's again
  util::hooks::ReleaseCullBoxes();
  if (updatesHeld)
    util::log::Write("Replay: the game's own updates of the bodies the replay drew itself were held off %d times while it played",
      updatesHeld);
  m_GunTail.clear();
  for (GunTail const& gun : tail)
    if (util::hooks::HoldBodyUpdate(gun.Body, true))
      m_GunTail.push_back(gun);
  if (!m_GunTail.empty())
    util::log::Write("Replay: %d guns keep their update held %d frames with their own pose before they're let go", (int)m_GunTail.size(),
      GunTailFrames);
  for (auto* list : { &m_People, &m_Others })
    for (auto& person : *list)
    {
      person->HeldBody = 0;
      person->GunHeld = false;
      person->GunPlaced = false;
      person->CullBoxSet = false;
    }
  for (auto* list : { &m_People, &m_Others })
    for (auto& person : *list)
    {
      // One whose slot was still being put back, or that the replay wrote
      // itself (see KeepTaken): its slot buffers as they were when taken (the
      // game doesn't update it, so they stay)
      if (person->RawRestoreLeft > 0 || person->Drawing)
      {
        __int64 animatable = 0;
        PoseSlot slot{};
        RawSlot raw;
        if (StillCharacter(person->Entity, person->VTable, person->Animatable, person->BodyOffset) &&
            FindCharacterPose(person->Entity, animatable, slot, person->BodyOffset) && animatable == person->Animatable &&
            FindRawSlot(slot, raw) && raw.Total() == person->RawBefore.size())
          WriteRaw(raw, person->RawBefore.data());
        person->RawRestoreLeft = 0;
      }
      person->Drawing = person->DrawTried = false;
      // A gun or drone the replay moved: back where it was; a character it
      // drew, its client entity likewise
      PutBackActor(*person);
      PutBackEntity(*person);
      // A body the replay switched on, off again (unless the game switched it meanwhile)
      if (person->ForcedBody)
        util::hooks::ReleaseBodyUpdate(person->ForcedBody,
          StillCharacter(person->Entity, person->VTable, person->Animatable, person->BodyOffset));
      person->ForcedBody = 0;
      person->ForceCooldown = 0;
      person->DeadLogged = false;
      DropBox(*person);
      if (!person->Registered)
        continue;
      UnregisterOverride(person->Set, person->RegisteredOverrides, person->RegisteredHandle);
      person->Registered = false;
    }
  // Every gun and drone let go (one that went may have been left in the list)
  int movesHeld = util::hooks::ReleaseActors();
  if (movesHeld)
    util::log::Write("Replay: the game's own moves of the replay's guns and drones were held back %d times while it played", movesHeld);
  // Looked at afresh next time (their poses then)
  for (auto& other : m_Others)
  {
    other->Times.clear();
    other->Frames.clear();
    other->Before.clear();
    other->RawBefore.clear();
    other->Taken = other->Frozen = other->FrozeWhilePlaying = false;
    other->DrawWait = 0;
    other->DrawFailLogged = false;
  }
  m_OthersHidden = -1;
}

// Every frame: the guns kept held past a playback (see StopPeople) let go
// once their frames are up, or at once if they went; all of them at once
// (now) when a playback takes them again
void FaithReplay::ReleaseGunTail(bool now)
{
  if (m_GunTail.empty())
    return;
  for (size_t i = 0; i < m_GunTail.size();)
  {
    GunTail& gun = m_GunTail[i];
    bool there = StillCharacter(gun.Entity, gun.VTable, gun.Animatable, ActorBodyOffset);
    if (now || --gun.Frames <= 0 || !there)
    {
      // Diagnostic (temporary): where its render-side entry has it when it's
      // let go, against where the game had it (see ReadRenderEntry)
      __int64 animatable = 0;
      PoseSlot slot{};
      RenderEntry render;
      if (there && FindCharacterPose(gun.Entity, animatable, slot, ActorBodyOffset) && animatable == gun.Animatable &&
          ReadRenderEntry(slot, render) && render.Buffer[1] >= 0x10000)
        util::log::Write("Replay:   gun 0x%I64X let go: its render-side bone 1 at %.2f %.2f %.2f, %.2f m from where the game had it",
          gun.Entity, render.Root[1][8], render.Root[1][9], render.Root[1][10], Distance3(&render.Root[1][8], gun.Real));
      util::hooks::HoldBodyUpdate(gun.Body, false);
      m_GunTail.erase(m_GunTail.begin() + i);
    }
    else
      ++i;
  }
  if (m_GunTail.empty())
    util::log::Write("Replay: the guns held past the playback are let go%s", now ? " (a playback took them again)" : "");
}

// Only when nothing plays (their overrides were taken out frames before;
// one still being put back is let go as it is)
void FaithReplay::ClearPeople()
{
  StopPeople();
  m_Vehicles.Clear();
  util::hooks::ForgetKeptVehicles();
  // A chase's sub-level kept loaded for these people may unload now
  util::hooks::ReleaseKeptSubLevels();
  for (auto& person : m_People)
  {
    FreeOverride(person->Set);
    FreeOverride(person->BoxSet);
  }
  ReleaseBound();
  m_People.clear();
  for (auto& other : m_Others)
    FreeOverride(other->Set);
  m_Others.clear();
  m_LastOthersScan = -1.0;
  m_PeopleBytes = 0;
  m_PeopleFull = false;
  m_CommitCheckedAt = -1.0;
  m_LastPeopleScan = -1.0;
}

void FaithReplay::Update(__int64 soldier, bool trackPlaying, bool trackPaused, double fixedDt, bool bodyShown,
  float const* faithRows)
{
  static bool s_build = IsReplayBuild();
  // The recording's cost on the game thread (see RecordCost): this frame's, at the end
  long long updateFrom = Ticks();
  bool wasRecording = m_State == State_Recording;
  auto endCost = [this, wasRecording, updateFrom]() {
    EndRecordFrame(wasRecording, m_State == State_Recording, Ticks() - updateFrom);
  };
  struct AtEnd
  {
    decltype(endCost)& Do;
    ~AtEnd()
    {
      Do();
    }
  } atEnd{ endCost };
  double now = NowSeconds();
  double realDt = m_LastNow > 0 ? now - m_LastNow : 0.0;
  m_LastNow = now;
  if (realDt < 0 || realDt > 0.25) realDt = 0.0; // a stall (loading) isn't time that went by
  // The game's Dash messages heard (see UpdateDashRecording)
  game_messages::ClientUpdate();
  ReleaseGunTail(false);
  // The option (its world goes back at the server's next update if it's turned off)
  util::hooks::HoldMissionWorlds(m_KeepWorldOn && faith_outfit::IsAvailable());
  replay_gamedraws::SetEnabled(m_ModelsOn && m_ModelShapesOn && m_GameDrawsOn);
  replay_gamedraws::SetWorldOn(m_WorldDrawsOn);
  replay_gamedraws::SetTracksOn(m_TracksOn);
  // The world's own draws: their meshes named while it records, the game's
  // draws of those meshes known while it plays (the game's mesh table, once a second)
  if (m_State != State_Idle && NowSeconds() >= m_WorldScanAt)
  {
    m_WorldScanAt = NowSeconds() + 1.0;
    {
      RecordTimer timer(Rec_WorldTable);
      replay_gamedraws::UpdateWorld(m_State == State_Recording, m_State == State_Playing);
    }
    // The recorded models' meshes at their other levels (see replay_shapes::MapLevels)
    if (m_State == State_Recording)
    {
      RecordTimer timer(Rec_Levels);
      replay_shapes::MapLevels();
    }
  }
  // Where the real Faith is, and how far from the recording's path, for the
  // mission's world's guards (see ReplayAssetLog's missions)
  if (m_State == State_Playing)
  {
    float route = FLT_MAX;
    if (faithRows)
      for (size_t i = 0; i + 3 < m_FaithPlaces.size(); i += 4)
        if (m_FaithPlaces[i] == m_FaithPlaces[i])   // not NaN (not known then)
          route = (std::min)(route, Distance3(faithRows + 12, &m_FaithPlaces[i]));
    replay_assetlog::SetFaithPlace(faithRows != nullptr, faithRows ? faithRows + 12 : nullptr, route);
  }
  // Diagnostic: the meshes and textures it loads and draws, and what a playback misses (see ReplayAssetLog.h)
  {
    RecordTimer timer(Rec_AssetLog);
    replay_assetlog::Update(m_State == State_Recording ? 1 : m_State == State_Playing ? 2 : 0,
      m_State == State_Playing ? m_PlayTime : m_RecordClock, util::hooks::LoadScreenShown());
  }
  // The recorded mission's bundles loaded again while it plays (see ReplayBundles.h)
  {
    RecordTimer timer(Rec_Bundles);
    replay_bundles::Update(m_State == State_Recording ? 1 : m_State == State_Playing ? 2 : 0,
      m_State == State_Playing ? m_PlayTime : m_RecordClock, m_ReloadBundlesOn, util::hooks::LoadScreenShown());
  }

  long long poseFrom = Ticks();
  PoseSlot slot{};
  bool found = s_build && FindPose(soldier, slot);
  // At the end of the frame's changes too (a state change below is seen next frame)
  KeepPeople(found);
  g_RecordCost.Frame[Rec_Pose] += Ticks() - poseFrom;

  // Dashes recorded by themselves
  UpdateDashRecording(found);

  // What the menu asked for
  if (m_RecordRequested)
  {
    m_RecordRequested = false;
    if (m_State == State_Recording)
      StopRecording();
    else if (m_State == State_Idle)
    {
      if (found)
        StartRecording();
      else
        strcpy_s(m_Message, s_build ? "Faith's pose wasn't found (load a level first)." : "Not available in this game build.");
    }
  }
  if (m_StopRequested)
  {
    m_StopRequested = false;
    if (m_State == State_Playing)
      StopPlaying();
  }
  if (m_StartRequested)
  {
    m_StartRequested = false;
    if (m_State == State_Idle)
      StartPlaying();
  }
  // Saved recordings: the one just made saved, one asked for loaded once
  // it's read and its parts made (see SaveRecording, ApplyLoaded)
  if (m_SaveAt >= 0.0 && m_State != State_Recording && NowSeconds() >= m_SaveAt)
    SaveRecording();
  if (m_LoadRequested)
  {
    m_LoadRequested = false;
    if (m_State == State_Idle && !replay_store::StartLoad(m_LoadFolder))
      strcpy_s(m_Message, "Not loaded: another one is loading.");
  }
  if (m_State == State_Idle)
  {
    replay_store::Loaded loaded;
    if (replay_store::TakeLoaded(loaded))
    {
      char why[160] = "";
      if (ApplyLoaded(loaded, why, sizeof(why)))
        _snprintf_s(m_Message, _TRUNCATE, "Loaded %s: press Play.", loaded.Folder.c_str());
      else
      {
        _snprintf_s(m_Message, _TRUNCATE, "Not loaded: %s (%s).", loaded.Folder.c_str(), why);
        util::log::Warning("Replay recordings: %s wasn't loaded: %s", loaded.Folder.c_str(), why);
      }
    }
  }

  // Along with the camera track: it starts, pauses and stops the replay
  if (m_WithTrack && m_Times.size() >= 2)
  {
    if (trackPlaying && !m_TrackWasPlaying && m_State == State_Idle)
      StartPlaying();
    else if (!trackPlaying && m_TrackWasPlaying && m_State == State_Playing)
      StopPlaying();
    if (m_State == State_Playing && trackPlaying)
      m_Paused = trackPaused;
  }
  m_TrackWasPlaying = trackPlaying;

  if (m_State == State_Recording)
  {
    if (!found)
    {
      strcpy_s(m_Message, "Recording stopped: Faith's pose went away (level load?).");
      StopRecording();
    }
    else
    {
      if (m_Bones == 0)
        m_Bones = slot.Bones;
      if (slot.Bones != m_Bones)
      {
        strcpy_s(m_Message, "Recording stopped: her skeleton changed.");
        StopRecording();
      }
      else
      {
        m_RecordClock += realDt;
        {
          RecordTimer timer(Rec_Loads);
          replay_audio::NoteClock(m_RecordClock);
          NoteRecordedLoads();
        }
        bool sampled = false;
        if (m_LastSample < 0 || m_RecordClock - m_LastSample >= SampleInterval)
        {
          poseFrom = Ticks();
          size_t frameFloats = (size_t)m_Bones * FloatsPerBone;
          size_t at = m_Frames.size();
          m_Frames.resize(at + frameFloats);
          if (CopyPose(slot.Local, &m_Frames[at], m_Bones))
          {
            m_Times.push_back(m_RecordClock);
            m_LastSample = m_RecordClock;
            sampled = true;
            // Her soldier then (see GetPlayedFaith)
            float heading = std::numeric_limits<float>::quiet_NaN();
            float place[3] = { heading, heading, heading };
            if (faithRows)
            {
              place[0] = faithRows[12];
              place[1] = faithRows[13];
              place[2] = faithRows[14];
              if (faithRows[8] * faithRows[8] + faithRows[10] * faithRows[10] > 1e-6f)
                heading = atan2f(faithRows[8], faithRows[10]);
            }
            m_FaithPlaces.insert(m_FaithPlaces.end(), { place[0], place[1], place[2], heading });
            // The game's time of day then (a mission's own time too: see
            // TimeOfDayAt)
            m_Tods.push_back(g_mainHandle->GetVisualsManager()->GetCurrentTimeOfDay());
            // Her outfit, when it changed (a mission's: the Release mission
            // dresses her in its own; not known for a moment after a level load)
            int outfit = faith_outfit::WornOutfit();
            if (outfit >= 0 && (m_Outfits.empty() || m_Outfits.back() != outfit))
            {
              m_OutfitTimes.push_back(m_RecordClock);
              m_Outfits.push_back(outfit);
              util::log::Write("Replay: at %.1f s Faith wears %s", m_RecordClock, faith_outfit::OutfitName(outfit));
            }
            // And its weather as shown, 4 times a second (a mission's own
            // clouds and wind: see VisualsManager::ReadWeatherLook)
            if (m_LookTimes.empty() || m_RecordClock - m_LookTimes.back() >= 0.25)
            {
              VisualsManager* pVisuals = g_mainHandle->GetVisualsManager();
              if (!m_LookSize)
                m_LookSize = pVisuals->WeatherLookSize();
              if (m_LookSize)
              {
                size_t lookAt = m_Looks.size();
                m_Looks.resize(lookAt + m_LookSize);
                if (pVisuals->ReadWeatherLook(&m_Looks[lookAt]))
                  m_LookTimes.push_back(m_RecordClock);
                else
                  m_Looks.resize(lookAt);
              }
            }
            g_RecordCost.Frame[Rec_Pose] += Ticks() - poseFrom;
            // The models near her that move, at the same samples (bone 1 of
            // her local pose is where she is in the world)
            float const* faith = &m_Frames[at + 1 * FloatsPerBone];
            {
              RecordTimer timer(Rec_Models);
              if (m_Times.size() == 1)
              {
                m_Models.StartRecording(faith, soldier);
                m_Vehicles.StartRecording();
              }
              m_Models.RecordFrame(m_RecordClock, faith, soldier);
            }
            {
              RecordTimer timer(Rec_People);
              RecordPeople(soldier, faith, m_RecordClock);
            }
            // The people's draws kept by the game's own drawing, matched to them
            {
              RecordTimer timer(Rec_GameDraws);
              replay_gamedraws::SetClock(m_RecordClock);
              if (m_GameDrawsOn && m_ModelsOn && m_ModelShapesOn)
                AdoptGameDraws();
            }
            {
              RecordTimer timer(Rec_Vehicles);
              m_Vehicles.RecordFrame(m_RecordClock, faith);
            }
          }
          else
          {
            m_Frames.resize(at);
            g_RecordCost.Frame[Rec_Pose] += Ticks() - poseFrom;
          }
        }
        // Her sounds, on the same clock (their voices' places with each pose)
        {
          RecordTimer timer(Rec_Sounds);
          util::hooks::ReplaySoundsRecordFrame(m_RecordClock, sampled);
        }
        // The gun effects near her, on the same clock
        {
          RecordTimer timer(Rec_Effects);
          util::hooks::ReplayEffectsRecordFrame(m_RecordClock);
        }
        // The mission items the game shows, hides or switches, on the same clock
        {
          RecordTimer timer(Rec_Items);
          util::hooks::MissionItemsRecordFrame(m_RecordClock);
        }
        // Her MagRope's line, on the same clock
        {
          RecordTimer timer(Rec_Rope);
          util::hooks::ReplayRopeRecordFrame(m_RecordClock);
        }
        if (m_RecordClock >= MaxRecordSeconds)
        {
          strcpy_s(m_Message, "Recording stopped at its 10 minute limit.");
          StopRecording();
        }
      }
    }
  }

  // Taken out of the slot when it isn't playing any more, or her slot moved
  bool shouldOverride = m_State == State_Playing && found && slot.Bones == m_Bones;
  if (m_Registered && (!shouldOverride || slot.Overrides != m_RegisteredOverrides || slot.Handle != m_RegisteredHandle))
  {
    UnregisterOverride(g_Override, m_RegisteredOverrides, m_RegisteredHandle);
    m_Registered = false;
  }
  // Her box pose likewise (Skeleton_Faith only)
  PoseSlot box{};
  bool shouldBox = shouldOverride && bodyShown && m_Bones == FaithBones && FindBoxPose(soldier, slot, box);
  if (m_BoxRegistered && (!shouldBox || box.Overrides != m_BoxRegisteredOverrides || box.Handle != m_BoxRegisteredHandle))
  {
    UnregisterOverride(g_BoxOverride, m_BoxRegisteredOverrides, m_BoxRegisteredHandle);
    m_BoxRegistered = false;
  }
  if (shouldOverride && bodyShown && !shouldBox && !m_BoxMissingLogged)
  {
    m_BoxMissingLogged = true;
    util::log::Warning("Replay: her box pose wasn't found, her body may not be drawn away from where she stands");
  }

  if (m_State == State_Playing)
  {
    if (!found || slot.Bones != m_Bones)
    {
      strcpy_s(m_Message, found ? "Not played: the recording is of another skeleton." : "Stopped: Faith's pose went away.");
      StopPlaying();
      return;
    }
    bool jumped = false;
    // The start held for the stand-ins or the mission's bundles: its time
    // stands still, and so do its sound track and effects (as paused)
    bool waiting = false;
    if (m_SeekTo >= 0.f)
    {
      m_PlayTime = m_SeekTo;
      m_SeekTo = -1.f;
      jumped = true;
    }
    else if (!m_Paused && !(waiting = WaitingForStandIns() || replay_bundles::Waiting()))
      m_PlayTime += (fixedDt > 0 ? fixedDt : realDt) * m_PlaySpeed;
    double length = Length();
    if (m_PlayTime > length)
    {
      jumped = jumped || (m_Loop && length > 0);
      if (m_Loop && length > 0)
        m_PlayTime = fmod(m_PlayTime, length);
      else if (m_WithTrack && trackPlaying)
        m_PlayTime = length; // holds the last frame until the track ends
      else
      {
        StopPlaying();
        return;
      }
    }
    // Its sound track at the replay's time (silent while held: paused, or the
    // start held for the stand-ins or the mission's bundles; not heard with a
    // fixed playback FPS). 2026-10-27, the user: "a weird sound stack until the
    // replay loads": while the bundles were waited for, the track wasn't told
    // it was held, ran on past the time standing still and jumped back to it
    // each time it was JumpSeconds off, over and over.
    if (m_SoundTrackOn && !m_SoundTrackPlaying && replay_audio::HasTrack())
      m_SoundTrackPlaying = replay_audio::StartPlaying();
    else if (!m_SoundTrackOn && m_SoundTrackPlaying)
    {
      replay_audio::StopPlaying();
      m_SoundTrackPlaying = false;
    }
    if (m_SoundTrackPlaying)
    {
      // Heard only while a recorded line the replay can't say live covers
      // the time (a loaded recording has no lines: its track is the dialog only)
      bool gate = !util::hooks::ReplayHasVoiceLines() ||
        util::hooks::ReplayDialogFromTrack(m_PlayTime, DialogBeforeSeconds, DialogAfterSeconds);
      replay_audio::SetPlayTime(m_PlayTime, m_Paused || waiting || m_StandInWaitUntil != 0, fixedDt > 0 ? 0.0 : m_PlaySpeed, gate);
    }
    if (!PrepareOverride(g_Override, m_Bones))
    {
      strcpy_s(m_Message, "Not played: out of memory.");
      StopPlaying();
      return;
    }
    PoseOverride* pOverride = &g_Override.Override[m_Buffer];
    SampleInto(m_PlayTime, pOverride->Bones);
    memcpy(m_PlayRoot, pOverride->Bones + 1 * FloatsPerBone, sizeof(m_PlayRoot));
    m_PlayRootOk = true;
    // The world's own draws round her, the tracks at the time played
    replay_gamedraws::SetWorldCenter(m_PlayRoot);
    replay_gamedraws::SetPlayTime(m_PlayTime);
    // Her sounds at the replay's place and time
    if (m_Sounds)
      util::hooks::ReplaySoundsPlay(m_PlayTime, jumped);
    else
      util::hooks::ReplaySoundsStop();
    // The gun effects near her (muzzle flashes, tracers, impacts) at their times
    if (m_PeopleOn)
    {
      // Paused or held: the shots' flashes, tracers and blasts don't go on firing
      if ((m_Paused || waiting) && !jumped)
        util::hooks::ReplayEffectsHold(m_PlayTime);
      else
        util::hooks::ReplayEffectsPlay(m_PlayTime, jumped);
    }
    else
      util::hooks::ReplayEffectsStop();
    // Her MagRope's line, drawn from the recorded points
    util::hooks::ReplayRopePlay(m_PlayTime);
    // The time of day as it was then (a camera track's own time of day wins)
    float tod = m_TodOn ? TimeOfDayAt(m_PlayTime) : -1.f;
    if (tod >= 0.f)
    {
      g_mainHandle->GetVisualsManager()->SetReplayTimeOfDay(tod);
      m_TodApplied = true;
    }
    else if (m_TodApplied)
    {
      g_mainHandle->GetVisualsManager()->StopReplayTimeOfDay();
      m_TodApplied = false;
    }
    ApplyRecordedWeather(m_TodOn);
    ApplyRecordedLook(m_TodOn ? m_PlayTime : -1.0);
    // Her outfit as it was then (the server's next update dresses her)
    int outfit = m_OutfitOn ? OutfitAt(m_PlayTime) : -1;
    if (outfit >= 0 || m_OutfitApplied)
      faith_outfit::SetReplayOutfit(outfit);
    m_OutfitApplied = outfit >= 0;
    // The models that moved near her, where they were at that time
    if (m_ModelsOn)
    {
      if (!m_Models.IsPlaying())
        m_Models.StartPlaying();
      m_Models.Play(m_PlayTime, m_PlayRootOk ? m_PlayRoot : nullptr);
      // The mission items shown, hidden or switched as they were then
      util::hooks::MissionItemsPlay(m_PlayTime);
    }
    else
    {
      m_Models.StopPlaying();
      util::hooks::MissionItemsStopPlaying();
    }
    // The people near her, as they moved then (bone 1 of her replayed pose
    // is where she is)
    if (m_PeopleOn)
    {
      PlayPeople(m_PlayTime, soldier, pOverride->Bones + 1 * FloatsPerBone);
      DrawPeopleWithGameDraws(m_PlayTime);
      m_Vehicles.Play(m_PlayTime);
    }
    else
    {
      StopPeople();
      replay_gamedraws::SetPeopleDraws({});
      replay_gamedraws::SetClothAnchors({});
    }
    if (m_Bones > RootBone && !WriteRoot(slot.Local, pOverride->Bones) && !m_RootWriteFailed)
    {
      m_RootWriteFailed = true;
      util::log::Warning("Replay: couldn't move her own pose along with the replay");
    }
    if (RegisterOverride(slot, pOverride))
    {
      m_Registered = true;
      m_RegisteredOverrides = slot.Overrides;
      m_RegisteredHandle = slot.Handle;
      m_Buffer ^= 1;
    }

    // Her box pose follows, so her body is drawn wherever the replay is
    if (shouldBox && PrepareOverride(g_BoxOverride, m_Bones))
    {
      PoseOverride* pBox = &g_BoxOverride.Override[m_BoxBuffer];
      if (FillBoxPose(pOverride->Bones, box.Local, pBox->Bones, m_Bones) && RegisterOverride(box, pBox))
      {
        m_BoxRegistered = true;
        m_BoxRegisteredOverrides = box.Overrides;
        m_BoxRegisteredHandle = box.Handle;
        m_BoxBuffer ^= 1;
      }
    }
  }
  else
    m_SeekTo = -1.f;
}

const char* FaithReplay::GetStatus(char* buffer, size_t size)
{
  buffer[0] = 0;
  if (m_State == State_Recording)
    sprintf_s(buffer, size, "Recording Faith %.1f s", m_RecordClock);
  else if (m_State == State_Playing)
    sprintf_s(buffer, size, "Replay %s%.1f / %.1f s", m_Paused ? "paused " : "", m_PlayTime, Length());
  return buffer;
}

bool FaithReplay::DrawUI()
{
  bool configChanged = false;
  float fontSize = ImGui::GetFontSize();

  // (The tab's intro, "Records Faith's movement ...", taken out 2026-11-09)
  if (m_State == State_Recording)
    ImGui::TextColored(ImVec4(1.f, 0.35f, 0.35f, 1.f), "Recording: %.1f s", m_RecordClock);
  else if (m_State == State_Playing)
    ImGui::Text("Playing: %.1f of %.1f s%s", m_PlayTime, Length(), m_Paused ? " (paused)" : "");
  else if (m_Times.size() >= 2)
    ImGui::Text("Recorded: %.1f s (%.1f MB), %d moving models, %d people", Length(),
      (m_Frames.size() * sizeof(float) + m_PeopleBytes) / 1048576.0, m_Models.Count(), (int)m_People.size());
  else
    ImGui::TextDisabled("Nothing recorded yet.");
  if (m_Message[0])
    ImGui::TextColored(ImVec4(1.f, 0.8f, 0.2f, 1.f), "%s", m_Message);

  float buttonWidth = fontSize * 8.f;
  bool canRecord = m_State != State_Playing;
  if (!canRecord) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.4f);
  if (ImGui::Button(m_State == State_Recording ? "Stop recording" : "Record", ImVec2(buttonWidth, 0)) && canRecord)
    m_RecordRequested = true;
  if (!canRecord) ImGui::PopStyleVar();
  ImGui::HelpMarker("Starts recording Faith from now on (it replaces the last recording), and stops it again. "
    "Play as Faith while it records: run the route you want to film. Up to 10 minutes.");
  configChanged |= ImGui::Checkbox("Record Dashes by themselves", &m_DashAuto);
  ImGui::HelpMarker(game_messages::Listening() ?
    "Starting a Dash starts a recording (when nothing is recording or playing), and it stops 2 seconds after "
    "the finish, saved with its folder named Dash (with Save every recording on). A cancelled or restarted "
    "Dash's recording isn't saved (it stays here to play until the next one); a restart records again. It "
    "goes by the game's own Dash messages, which the tools only listen to. A recording started from the menu "
    "is left alone." :
    "Starting a Dash starts a recording, and it stops 2 seconds after the finish. The game's Dash messages "
    "aren't heard right now (once a level is loaded they are; CT.log says when).");

  bool canPlay = m_State != State_Recording && m_Times.size() >= 2;
  if (!canPlay) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.4f);
  if (ImGui::Button(m_State == State_Playing ? "Stop##Replay" : "Play##Replay", ImVec2(buttonWidth, 0)) && canPlay)
  {
    if (m_State == State_Playing)
      m_StopRequested = true;
    else
      m_StartRequested = true;
  }
  ImGui::SameLine();
  bool canPause = m_State == State_Playing;
  if (!canPause) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.4f);
  if (ImGui::Button(m_Paused ? "Resume##Replay" : "Pause##Replay", ImVec2(buttonWidth, 0)) && canPause)
    m_Paused = !m_Paused;
  if (!canPause) ImGui::PopStyleVar();
  if (!canPlay) ImGui::PopStyleVar();

  if (m_Times.size() >= 2)
  {
    float position = (float)(m_State == State_Playing ? m_PlayTime : 0.0);
    float length = (float)Length();
    ImGui::PushItemWidth(fontSize * 16.f);
    // A value typed in (Ctrl+click) isn't held to the slider's range
    if (ImGui::SliderFloat("Position##Replay", &position, 0.f, length, "%.1f s") && m_State == State_Playing)
      m_SeekTo = position < 0.f ? 0.f : position > length ? length : position;
    ImGui::PopItemWidth();
    ImGui::HelpMarker("Where the replay is. Drag it while it plays (or paused) to go to another moment.");
  }

  configChanged |= ImGui::Checkbox("Loop##Replay", &m_Loop);
  ImGui::HelpMarker("Starts again from the beginning when it gets to the end.");
  configChanged |= ImGui::Checkbox("Faith's sounds##Replay", &m_Sounds);
  configChanged |= ImGui::Checkbox("The mission's dialog as recorded##Replay", &m_SoundTrackOn);
  ImGui::HelpMarker("The replay says the recorded dialog live through the game's own voice-over system (KrugerSec's and "
    "the people's lines, Faith's, the radio's, a mission's narrative), from where the people are, whenever the game still "
    "has the line's sound. A mission's dialog goes with the mission once it's over, so while it records the game's sound "
    "is captured too (Windows' capture of the game's own sound) and only the dialog's moments are kept: while it plays, "
    "a line the game can't say any more is heard from that capture at its moment (paused with the replay, faster or slower "
    "with its speed; silent with a fixed playback FPS), as it was heard then. Whatever else was heard at that moment is in "
    "it too. The game's own sound plays as usual (nothing is muted).");
  // Always on since 2026-11-04, no ticks: the mission's world from the game,
  // the moving models with the ones a mission took drawn (the game's own
  // drawing of them, the mission's world, other animated things), the people
  // (kept in the level until it plays, stand-ins), the mission's world kept
  // after it's over and its bundles loaded again (CameraManager::ReadConfig)
  {
    char status[200];
    ImGui::PushTextWrapPos(0.f);
    ImGui::TextDisabled("%s", replay_audio::Status(status, sizeof(status)));
    ImGui::PopTextWrapPos();
  }
  {
    int shapes = replay_shapes::Count();
    if (shapes > 0)
      ImGui::TextDisabled("%d shapes kept (%.1f MB).", shapes, replay_shapes::Bytes() / 1048576.0);
    int ready = replay_gamedraws::Ready();
    if (ready > 0)
      ImGui::TextDisabled("%d parts kept as the game draws them.", ready);
    int worldKept = 0, worldReady = 0;
    replay_gamedraws::WorldCounts(worldKept, worldReady);
    if (worldKept > 0)
      ImGui::TextDisabled("%d of the world's draws kept (%d ready).", worldKept, worldReady);
  }
  configChanged |= ImGui::Checkbox("Time of day and weather as recorded##Replay", &m_TodOn);
  configChanged |= ImGui::Checkbox("Faith's outfit as recorded##Replay", &m_OutfitOn);
  if (!faith_outfit::IsAvailable())
    ImGui::TextDisabled("Faith's outfit: not available, the game's character customizing code isn't what the tools "
      "know (see CT.log).");
  {
    if (m_State == State_Playing && m_StandInWaitUntil)
      ImGui::TextDisabled("Holding the start for the stand-ins (%d on their way, at most %d s)...", m_StandInsPending,
        (int)(StandInWaitMs / 1000));
    else if (m_State == State_Playing && m_StandIns > 0)
    {
      ImGui::PushTextWrapPos(0.f);
      ImGui::TextDisabled("%d stand-ins asked of the game's spawners, %d playing their recorded person.", m_StandIns,
        m_StandInsPlaying);
      ImGui::PopTextWrapPos();
    }
    else if (m_PeopleOn && m_StandInsOn && !StandInsUsable())
      ImGui::TextDisabled("Stand-ins: not available, the game's spawning code isn't what the tools know (see CT.log).");
  }
  {
    int kept = util::hooks::KeptMissionWorlds();
    if (!util::hooks::MissionWorldsKnown() || !faith_outfit::IsAvailable())
      ImGui::TextDisabled("Keeping the mission's world: not available, the game's mission streaming code isn't what the "
        "tools know (see CT.log).");
    else if (kept > 0)
    {
      ImGui::TextDisabled("%d mission world%s kept.", kept, kept == 1 ? "" : "s");
      if (ImGui::Button("Let the mission's world go##Replay"))
        util::hooks::LetMissionWorldsGo("the menu's button");
      ImGui::HelpMarker("The game streams it out now, as it would have when the mission was over. The replay then draws "
        "the mission's models itself and asks for stand-ins for its people.");
    }
  }
  {
    int recorded = 0, held = 0, in = 0;
    replay_bundles::Status(recorded, held, in);
    if (!replay_bundles::Available())
      ImGui::TextDisabled("The mission's bundles: not available, the game's bundle loader isn't what the tools know (see CT.log).");
    else if (recorded > 0)
      ImGui::TextDisabled("%d mission bundle%s noted; %d asked for, %d of them in.", recorded, recorded == 1 ? "" : "s", held, in);
  }
  configChanged |= ImGui::Checkbox("Start after its loading screen##Replay", &m_StartAfterLoad);
  ImGui::HelpMarker("A recording that crossed a loading screen (you started or left a mission, or fast travelled while "
    "it recorded) starts playing where that loading screen went: what's before it is somewhere else. Untick it to "
    "play it from the beginning.");
  if (!m_Loads.empty() && m_Loads.back().End > 0.0)
  {
    char where[192];
    ImGui::PushTextWrapPos(0.f);
    ImGui::TextDisabled("Recorded in %s.", WhereRecorded(where, sizeof(where)));
    ImGui::PopTextWrapPos();
  }
  configChanged |= ImGui::Checkbox("Play along with the camera track", &m_WithTrack);

  if (m_Times.size() >= 2 && m_State == State_Idle)
  {
    if (ImGui::Button("Forget the recording"))
    {
      ForgetRecording();
      util::log::Write("Replay: recording forgotten");
    }
  }
  DrawStoreUI(configChanged);
  return configChanged;
}

void FaithReplay::ForgetRecording()
{
  // Saved first if it's still to be (see SaveRecording)
  if (m_SaveAt >= 0.0)
    SaveRecording();
  m_Times.clear();
  m_Frames.clear();
  m_FaithPlaces.clear();
  m_Tods.clear();
  m_OutfitTimes.clear();
  m_Outfits.clear();
  m_Loads.clear();
  m_LookTimes.clear();
  m_Looks.clear();
  m_Bones = 0;
  m_Message[0] = 0;
  m_SaveAt = -1.0;
  m_LoadedFrom.clear();
  m_Models.Clear();
  ClearPeople();
  g_Hierarchies.clear();
  replay_audio::Forget();
  util::hooks::MissionWorldsRecording(0);
  replay_bundles::Forget();
}

// Saved recordings (2026-10-27; the user: "save every recording to Cinematic
// Tools\Recordings with all its parts, and load it in any session. also find
// a way to make them smaller"; see ReplayStore.h). A recording is saved a
// moment after it stops (1.5 s: its last kept draws are read back by then):
// Faith's frames, places, time of day, weather, outfit and loading
// screens; the people (their frames as packed while recording, their world
// poses, their fixed transforms, their skeletons' hierarchy, their kept draws'
// ids and bone maps); the guns', drones' and doors' kept draws; the moving
// models; the shapes and kept draws with their parts; the bundles; the sound
// track. The game's addresses (entities, bodies, pose managers, vertex
// buffers) are written as keys. Loaded, they're addresses of the tools' own
// zeroed memory: the replay finds none of them in the level, as for a
// recording whose mission is over, and plays it the same way (models found
// again by name and place, people drawn with their kept draws or by
// stand-ins, the mission's bundles loaded again). Not saved: her own sounds,
// the gun effects, the mission items and the MagRope's line (the game's own
// sounds, effects and entities, which a later session doesn't have; the
// sound track has what was heard), and the vehicles (moved only when the
// game has them).
namespace
{
  const unsigned TagFaith = replay_store::Tag('F', 'A', 'I', 'T'), TagPeople = replay_store::Tag('P', 'E', 'O', 'P'),
    TagModels = replay_store::Tag('M', 'O', 'D', 'L'), TagShapes = replay_store::Tag('S', 'H', 'A', 'P'),
    TagDraws = replay_store::Tag('D', 'R', 'A', 'W'), TagBundles = replay_store::Tag('B', 'U', 'N', 'D'),
    TagAudio = replay_store::Tag('A', 'U', 'D', 'I'), TagTracks = replay_store::Tag('T', 'R', 'A', 'K'),
    TagCrowds = replay_store::Tag('C', 'R', 'W', 'D'), TagMissions = replay_store::Tag('M', 'I', 'S', 'N');
  const unsigned SaveVersion = 1;
  const size_t MaxSavedFloats = (size_t)1 << 28;

  // Floats in rows of rowFloats (one column a float, see Out::Frames)
  void PutFloats(replay_store::Out& out, std::vector<float> const& v, size_t rowFloats)
  {
    if (!rowFloats || v.size() % rowFloats)
      rowFloats = 1;
    out.Frames(v.data(), v.size() / rowFloats, rowFloats * sizeof(float), sizeof(float));
  }

  bool GetFloats(replay_store::In& in, std::vector<float>& v)
  {
    std::vector<unsigned char> bytes;
    if (!in.Frames(bytes, MaxSavedFloats * sizeof(float)) || bytes.size() % sizeof(float))
      return false;
    v.resize(bytes.size() / sizeof(float));
    if (!bytes.empty())
      memcpy(v.data(), bytes.data(), bytes.size());
    return true;
  }

  std::vector<unsigned char> const* SectionOf(replay_store::Loaded const& loaded, unsigned tag)
  {
    for (auto const& section : loaded.Sections)
      if (section.first == tag)
        return &section.second;
    return nullptr;
  }
}

// Dashes recorded by themselves (the REPLAY tab's tick): the game's own Dash
// messages (game_messages: the server's started, finished and cancelled ones
// sent to the player) start a recording when a Dash starts with nothing
// recording or playing, stop it DashTailSeconds after it's finished (so its
// end is in it; saved as any recording, its folder named Dash), and stop it
// unsaved when it's cancelled. A recording started from the menu is left
// alone, and one stopped from the menu isn't the Dash's any more. Each
// message is logged with its manager and raw fields (what they hold isn't
// known yet).
void FaithReplay::UpdateDashRecording(bool faithFound)
{
  const double DashTailSeconds = 2.0;
  const unsigned long long SameMessageMs = 1500;   // both managers, or the game's two finished ones
  game_messages::DashEvent events[16];
  int count = game_messages::TakeDashEvents(events, 16);
  for (int i = 0; i < count; ++i)
  {
    game_messages::DashEvent const& e = events[i];
    if (e.Kind < game_messages::DashStarted || e.Kind > game_messages::DashCancelled)
      continue;
    bool again = m_DashLastTick[e.Kind] && e.Tick - m_DashLastTick[e.Kind] < SameMessageMs;
    m_DashLastTick[e.Kind] = e.Tick;
    char fields[96];
    if (e.Kind == game_messages::DashFinished)
      sprintf_s(fields, "%08X %08X %08X %08X %08X %08X %08X %08X", e.Fields[0], e.Fields[1], e.Fields[2], e.Fields[3],
        e.Fields[4], e.Fields[5], e.Fields[6], e.Fields[7]);
    else
      sprintf_s(fields, "%08X %08X", e.Fields[0], e.Fields[1]);
    util::log::Write("Replay: the game's Dash %s message (on its %s message manager%s; its fields from + 0x48: %s)",
      game_messages::KindName(e.Kind), e.Manager == 1 ? "client's" : "server's", again ? ", the same one again" : "", fields);
    if (again || !m_DashAuto)
      continue;
    if (e.Kind == game_messages::DashStarted)
    {
      // Started again with no cancel: the one before stops, unsaved
      if (m_State == State_Recording && m_DashRecording)
      {
        StopRecording();
        m_SaveAt = -1.0;
        m_SaveWhere.clear();
        m_DashRecording = false;
        m_DashStopAt = -1.0;
        util::log::Write("Replay: the Dash started again: the recording of the one before stopped, not saved");
      }
      if (m_State == State_Idle)
        m_DashStartPending = true;   // at the next update
      else
        util::log::Write("Replay: a Dash started while %s: not recorded by itself",
          m_State == State_Recording ? "a recording from the menu runs" : "a recording plays");
    }
    else if (e.Kind == game_messages::DashFinished)
    {
      m_DashStartPending = false;
      if (m_DashRecording && m_State == State_Recording && m_DashStopAt < 0.0)
        m_DashStopAt = NowSeconds() + DashTailSeconds;
    }
    else
    {
      m_DashStartPending = false;
      if (m_DashRecording && m_State == State_Recording)
      {
        StopRecording();
        m_SaveAt = -1.0;
        m_SaveWhere.clear();
        strcpy_s(m_Message, "The Dash was cancelled: its recording is here to play, not saved.");
        util::log::Write("Replay: the Dash was cancelled: its recording stopped (%.1f s), not saved", Length());
      }
      m_DashRecording = false;
      m_DashStopAt = -1.0;
    }
  }
  // Stopped some other way (the menu, the 10 minute limit): not the Dash's any more
  if (m_DashRecording && m_State != State_Recording)
  {
    m_DashRecording = false;
    m_DashStopAt = -1.0;
  }
  if (m_DashStopAt >= 0.0 && NowSeconds() >= m_DashStopAt)
  {
    m_DashStopAt = -1.0;
    if (m_DashRecording && m_State == State_Recording)
    {
      StopRecording();
      strcpy_s(m_Message, "Dash recorded.");
      util::log::Write("Replay: the Dash's recording stopped %.0f s after its finish (%.1f s in all)%s", DashTailSeconds,
        Length(), m_SaveAt >= 0.0 ? ", saved as Dash in a moment" : "");
    }
    m_DashRecording = false;
  }
  if (m_DashStartPending && m_State == State_Idle)
  {
    m_DashStartPending = false;
    if (!faithFound)
      util::log::Write("Replay: a Dash started but Faith's pose wasn't found: not recorded");
    else
    {
      StartRecording();
      if (m_State == State_Recording)
      {
        m_DashRecording = true;
        m_SaveWhere = "Dash";
        strcpy_s(m_Message, "Recording the Dash: it stops 2 s after the finish.");
        util::log::Write("Replay: recording the Dash (started by the game's Dash message)");
      }
    }
  }
}

void FaithReplay::SaveRecording()
{
  m_SaveAt = -1.0;
  if (m_Times.size() < 2 || m_Bones <= 0)
    return;
  replay_store::SaveRequest request;
  request.Parts = replay_store::NewSaveParts();
  request.Seconds = Length();
  request.Where = replay_bundles::Where();
  if (request.Where.empty())
  {
    bool release = false;
    for (int outfit : m_Outfits)
      release = release || faith_outfit::IsReleaseOutfit(outfit);
    request.Where = release ? "Release" : "Free roam";
  }
  // A Dash's (see UpdateDashRecording)
  if (!m_SaveWhere.empty())
    request.Where = m_SaveWhere;
  m_SaveWhere.clear();
  replay_store::Keys keys;
  // Faith
  replay_store::Out faith;
  faith.Put(SaveVersion);
  faith.Put(m_Bones);
  faith.Vec(m_Times);
  PutFloats(faith, m_Frames, (size_t)m_Bones * FloatsPerBone);
  faith.Vec(m_FaithPlaces);
  faith.Vec(m_Tods);
  faith.Vec(m_OutfitTimes);
  faith.Vec(m_Outfits);
  faith.Vec(m_Loads);
  faith.Put(m_RecordedWeather);
  faith.Put((unsigned char)m_HaveWeather);
  faith.Put((unsigned long long)m_LookSize);
  faith.Vec(m_LookTimes);
  faith.Vec(m_Looks);
  // The people, their skeletons' hierarchies, the guns', drones' and doors' kept draws
  replay_store::Out people;
  std::set<int> skins;
  people.Put((unsigned)m_People.size());
  std::vector<std::pair<__int64, int>> managers;   // pose manager, bones
  for (auto const& owned : m_People)
  {
    PersonTrack const& p = *owned;
    people.Put(keys.Of(p.Entity));
    people.Put(p.VTable);
    people.Put(keys.Of(p.Animatable));
    people.Bytes(p.Template, sizeof(p.Template));
    people.Put(p.Bones);
    people.Bytes(p.Label, sizeof(p.Label));
    people.Put(p.BodyOffset);
    people.Vec(p.Times);
    std::vector<BYTE> packed;
    p.Recorded.CopyTo(packed);
    people.Put((unsigned long long)p.Recorded.Count());
    people.Frames(packed.data(), p.Recorded.Count(), p.Recorded.StoredFrameBytes(), 2);
    PutFloats(people, p.RootWorld, RootWorldFloats);
    people.Put((unsigned char)p.RootWorldOk);
    people.Put((unsigned char)p.Moved);
    people.Put((unsigned char)p.Gone);
    people.Vec(p.Unbind);
    people.Put(keys.Of(p.Manager));
    if (p.Manager)
      managers.push_back({ p.Manager, p.Bones });
    people.Put((unsigned)p.GameDraws.size());
    for (auto const& d : p.GameDraws)
    {
      people.Put(d.Id);
      people.Put(keys.Of(d.Vb));
      people.Put(d.StartIndex);
      people.Put(d.IndexCount);
      people.Vec(d.Map);
      people.Vec(d.HeaderRows);
      skins.insert(d.Id);
    }
    people.Put(p.GameDrawIndices);
    PutFloats(people, p.WorldFrames, (size_t)p.Bones * FloatsPerBone);
    people.Put((unsigned char)p.WorldOk);
    people.Put((unsigned char)p.BoundDrawn);
  }
  std::vector<Hierarchy const*> hierarchies;
  for (auto const& m : managers)
  {
    Hierarchy const* h = FindHierarchy(m.first, m.second);
    if (h && h->Ready && std::find(hierarchies.begin(), hierarchies.end(), h) == hierarchies.end())
      hierarchies.push_back(h);
  }
  people.Put((unsigned)hierarchies.size());
  for (Hierarchy const* h : hierarchies)
  {
    people.Put(keys.Of(h->Manager));
    people.Put(h->Bones);
    people.Vec(h->Parent);
    people.Put((unsigned char)h->Failed);
    people.Vec(h->Order);
    people.Put(h->Loose);
  }
  people.Put((unsigned)m_Bound.size());
  for (BoundOwner const& owner : m_Bound)
  {
    int person = -1;
    for (size_t i = 0; i < m_People.size(); ++i)
      if (m_People[i].get() == owner.Person)
        person = (int)i;
    people.Put(person);
    people.Put(keys.Of(owner.Entity));
    people.Text(owner.Label);
    people.Put((unsigned)owner.Vbs.size());
    for (__int64 vb : owner.Vbs)
      people.Put(keys.Of(vb));
    people.Put((unsigned)owner.Draws.size());
    for (BoundDraw const& d : owner.Draws)
    {
      people.Put(d.Id);
      people.Put(keys.Of(d.Vb));
      people.Put(d.StartIndex);
      people.Put(d.IndexCount);
      people.Put(d.Header);
      people.Vec(d.HeaderRows);
      people.Vec(d.Bone);
      people.Vec(d.Fixed);
      people.Put(d.Clock);
      people.Vec(d.Seen);
      people.Put((unsigned char)d.Checked);
      skins.insert(d.Id);
    }
    people.Put(owner.Indices);
    people.Put(owner.Kept);
    people.Put(owner.Misfits);
    people.Put((unsigned char)owner.Refused);
    people.Put(owner.Miss);
  }
  // The times each was seen drawn (2026-10-27 later: an older file ends before them)
  people.Put((unsigned)m_People.size());
  for (auto const& owned : m_People)
    people.Vec(owned->SeenTimes);
  replay_store::Out models, shapes, draws, bundles, audio, tracks, crowds;
  m_Models.Save(models, keys);
  replay_shapes::Save(shapes, *request.Parts);
  int unsaved = 0;
  int kept = replay_gamedraws::Save(draws, *request.Parts, skins, unsaved);
  int tracksUnsaved = 0;
  int trackCount = replay_gamedraws::SaveTracks(tracks, *request.Parts, tracksUnsaved);
  unsaved += tracksUnsaved;
  int crowdsUnsaved = 0;
  int crowdKinds = replay_gamedraws::SaveCrowds(crowds, *request.Parts, crowdsUnsaved);
  unsaved += crowdsUnsaved;
  replay_bundles::Save(bundles);
  bool track = replay_audio::Save(audio);
  char summary[300];
  sprintf_s(summary, "%d people, %d guns or drones, %d moving models, %d kept draws, %d other animated, %d kinds of crowd draws%s%s",
    (int)std::count_if(m_People.begin(), m_People.end(), [](std::unique_ptr<PersonTrack> const& p) {
      return p->BodyOffset == CharacterBodyOffset; }), (int)std::count_if(m_People.begin(), m_People.end(),
    [](std::unique_ptr<PersonTrack> const& p) { return p->BodyOffset != CharacterBodyOffset; }), m_Models.Count(), kept, trackCount,
    crowdKinds, unsaved ? " (some couldn't be saved)" : "", track ? ", the sound track" : "");
  request.Summary = summary;
  request.Sections.emplace_back(TagFaith, std::move(faith));
  request.Sections.emplace_back(TagPeople, std::move(people));
  request.Sections.emplace_back(TagModels, std::move(models));
  request.Sections.emplace_back(TagShapes, std::move(shapes));
  request.Sections.emplace_back(TagDraws, std::move(draws));
  request.Sections.emplace_back(TagBundles, std::move(bundles));
  request.Sections.emplace_back(TagTracks, std::move(tracks));
  request.Sections.emplace_back(TagCrowds, std::move(crowds));
  // The missions running while it recorded (2026-11-01; see ReplayAssetLog's mission's world)
  {
    replay_store::Out missions;
    std::vector<std::string> names = replay_assetlog::RecordedMissions();
    missions.Put((unsigned)names.size());
    for (std::string const& name : names)
      missions.Text(name);
    request.Sections.emplace_back(TagMissions, std::move(missions));
  }
  if (track)
    request.Sections.emplace_back(TagAudio, std::move(audio));
  if (unsaved)
    util::log::Write("Replay recordings: %d kept draws can't be saved (a shader, input layout or texture of theirs the tools can't "
      "keep: made before the tools were there, or a geometry, hull or domain shader)", unsaved);
  replay_store::StartSave(std::move(request));
}

bool FaithReplay::IsMissionWorld()
{
  return replay_assetlog::MissionWorld();
}

void FaithReplay::SetMissionWorld(bool on)
{
  replay_assetlog::SetMissionWorld(on);
}

bool FaithReplay::ApplyLoaded(replay_store::Loaded& loaded, char* why, size_t whySize)
{
  ForgetRecording();
  auto fail = [&](char const* text) {
    strcpy_s(why, whySize, text);
    ForgetRecording();
    return false;
  };
  std::vector<unsigned char> const* section = SectionOf(loaded, TagFaith);
  if (!section)
    return fail("it has no Faith");
  {
    replay_store::In in(section->data(), section->size());
    unsigned version = 0;
    unsigned char haveWeather = 0;
    unsigned long long lookSize = 0;
    if (!in.Get(version) || version != SaveVersion || !in.Get(m_Bones) || m_Bones <= 0 || m_Bones > 4096 ||
        !in.Vec(m_Times, 1 << 22) || !GetFloats(in, m_Frames) || !in.Vec(m_FaithPlaces, 1 << 24) || !in.Vec(m_Tods, 1 << 22) ||
        !in.Vec(m_OutfitTimes, 1 << 16) || !in.Vec(m_Outfits, 1 << 16) || !in.Vec(m_Loads, 1 << 16) ||
        !in.Get(m_RecordedWeather) || !in.Get(haveWeather) || !in.Get(lookSize) || lookSize > (1 << 20) ||
        !in.Vec(m_LookTimes, 1 << 22) || !in.Vec(m_Looks, (size_t)1 << 30) ||
        m_Frames.size() != m_Times.size() * (size_t)m_Bones * FloatsPerBone)
      return fail("Faith's part of it couldn't be read");
    m_HaveWeather = haveWeather != 0;
    // The looks only if they're this game's size (see VisualsManager::WeatherLookSize)
    size_t size = g_mainHandle->GetVisualsManager()->WeatherLookSize();
    if (!size || size != lookSize || m_Looks.size() != m_LookTimes.size() * size)
    {
      m_LookTimes.clear();
      m_Looks.clear();
    }
    m_LookSize = size;
  }
  int keptDraws = 0, dropped = 0;
  if ((section = SectionOf(loaded, TagPeople)) != nullptr)
  {
    replay_store::In in(section->data(), section->size());
    unsigned count = 0;
    if (!in.Get(count) || count > 4096)
      return fail("its people couldn't be read");
    for (unsigned i = 0; i < count; ++i)
    {
      std::unique_ptr<PersonTrack> p = std::make_unique<PersonTrack>();
      long long entity = 0, animatable = 0, manager = 0;
      unsigned long long frames = 0;
      std::vector<unsigned char> packed;
      unsigned char rootOk = 0, moved = 0, gone = 0, worldOk = 0, boundDrawn = 0;
      unsigned draws = 0;
      if (!in.Get(entity) || !in.Get(p->VTable) || !in.Get(animatable) || !in.Bytes(p->Template, sizeof(p->Template)) ||
          !in.Get(p->Bones) || p->Bones <= 0 || p->Bones > 4096 || !in.Bytes(p->Label, sizeof(p->Label)) || !in.Get(p->BodyOffset) ||
          !in.Vec(p->Times, 1 << 22) || !in.Get(frames) || !in.Frames(packed, MaxSavedFloats) ||
          !p->Recorded.Assign(p->Bones, (size_t)frames, packed.data(), packed.size()) || !GetFloats(in, p->RootWorld) ||
          !in.Get(rootOk) || !in.Get(moved) || !in.Get(gone) || !in.Vec(p->Unbind, 1 << 20) || !in.Get(manager) || !in.Get(draws) ||
          draws > 4096)
        return fail("its people couldn't be read");
      p->Template[sizeof(p->Template) - 1] = 0;
      p->Label[sizeof(p->Label) - 1] = 0;
      p->Entity = replay_store::AddressOf(entity);
      p->Animatable = replay_store::AddressOf(animatable);
      p->Manager = replay_store::AddressOf(manager);
      p->Data = 0;
      p->RootWorldOk = rootOk != 0;
      p->Moved = moved != 0;
      p->Gone = gone != 0;
      for (unsigned k = 0; k < draws; ++k)
      {
        PersonTrack::GameDraw d{};
        long long vb = 0;
        if (!in.Get(d.Id) || !in.Get(vb) || !in.Get(d.StartIndex) || !in.Get(d.IndexCount) || !in.Vec(d.Map, 4096) ||
            !in.Vec(d.HeaderRows, 1 << 16))
          return fail("its people couldn't be read");
        d.Vb = replay_store::AddressOf(vb);
        p->GameDraws.push_back(std::move(d));
      }
      if (!in.Get(p->GameDrawIndices) || !GetFloats(in, p->WorldFrames) || !in.Get(worldOk) || !in.Get(boundDrawn))
        return fail("its people couldn't be read");
      p->WorldOk = worldOk != 0;
      p->BoundDrawn = boundDrawn != 0;
      m_PeopleBytes += PackedFrames::FrameBytes(p->Bones) * (size_t)frames + p->WorldFrames.size() * sizeof(float);
      m_People.push_back(std::move(p));
    }
    unsigned hierarchies = 0;
    if (!in.Get(hierarchies) || hierarchies > 4096)
      return fail("its people's skeletons couldn't be read");
    for (unsigned i = 0; i < hierarchies; ++i)
    {
      Hierarchy h{ 0, 0, 0, {}, {}, true, false, {}, 0 };
      long long manager = 0;
      unsigned char failed = 0;
      if (!in.Get(manager) || !in.Get(h.Bones) || !in.Vec(h.Parent, 4096) || !in.Get(failed) || !in.Vec(h.Order, 4096) ||
          !in.Get(h.Loose) || h.Parent.size() != (size_t)h.Bones)
        return fail("its people's skeletons couldn't be read");
      h.Manager = replay_store::AddressOf(manager);
      h.Failed = failed != 0;
      g_Hierarchies.push_back(std::move(h));
    }
    unsigned owners = 0;
    if (!in.Get(owners) || owners > 4096)
      return fail("its guns, drones and doors couldn't be read");
    for (unsigned i = 0; i < owners; ++i)
    {
      BoundOwner owner;
      int person = -1;
      long long entity = 0;
      unsigned vbs = 0, draws = 0;
      if (!in.Get(person) || !in.Get(entity) || !in.Text(owner.Label, 256) || !in.Get(vbs) || vbs > 64)
        return fail("its guns, drones and doors couldn't be read");
      owner.Person = person >= 0 && person < (int)m_People.size() ? m_People[person].get() : nullptr;
      owner.Entity = replay_store::AddressOf(entity);
      for (unsigned k = 0; k < vbs; ++k)
      {
        long long vb = 0;
        if (!in.Get(vb))
          return fail("its guns, drones and doors couldn't be read");
        owner.Vbs.push_back(replay_store::AddressOf(vb));
        m_BoundVbs.insert(owner.Vbs.back());
      }
      if (!in.Get(draws) || draws > 4096)
        return fail("its guns, drones and doors couldn't be read");
      for (unsigned k = 0; k < draws; ++k)
      {
        BoundDraw d{};
        long long vb = 0;
        unsigned char checked = 0;
        if (!in.Get(d.Id) || !in.Get(vb) || !in.Get(d.StartIndex) || !in.Get(d.IndexCount) || !in.Get(d.Header) ||
            !in.Vec(d.HeaderRows, 1 << 16) || !in.Vec(d.Bone, 4096) || !in.Vec(d.Fixed, 1 << 16) || !in.Get(d.Clock) ||
            !in.Vec(d.Seen, 1 << 16) || !in.Get(checked))
          return fail("its guns, drones and doors couldn't be read");
        d.Vb = replay_store::AddressOf(vb);
        d.Checked = checked != 0;
        owner.Draws.push_back(std::move(d));
      }
      unsigned char refused = 0;
      if (!in.Get(owner.Indices) || !in.Get(owner.Kept) || !in.Get(owner.Misfits) || !in.Get(refused) || !in.Get(owner.Miss))
        return fail("its guns, drones and doors couldn't be read");
      owner.Refused = refused != 0;
      owner.VbTries = 1;   // not looked for again
      m_Bound.push_back(std::move(owner));
    }
    unsigned seenCount = 0;
    if (in.Left() && in.Get(seenCount) && seenCount == m_People.size())
      for (auto& person : m_People)
        if (!in.Vec(person->SeenTimes, 1 << 20))
          return fail("its people's times seen couldn't be read");
  }
  if ((section = SectionOf(loaded, TagModels)) != nullptr)
  {
    replay_store::In in(section->data(), section->size());
    if (!m_Models.Load(in))
      return fail("its moving models couldn't be read");
  }
  if ((section = SectionOf(loaded, TagShapes)) != nullptr && loaded.Parts)
  {
    replay_store::In in(section->data(), section->size());
    if (!replay_shapes::Load(in, *loaded.Parts))
      return fail("its models' shapes couldn't be read");
  }
  if ((section = SectionOf(loaded, TagDraws)) != nullptr && loaded.Parts)
  {
    replay_store::In in(section->data(), section->size());
    keptDraws = replay_gamedraws::Load(in, *loaded.Parts, dropped);
    if (keptDraws < 0)
      return fail("its kept draws couldn't be read");
  }
  int trackCount = 0, tracksDropped = 0;
  if ((section = SectionOf(loaded, TagTracks)) != nullptr && loaded.Parts)
  {
    replay_store::In in(section->data(), section->size());
    trackCount = replay_gamedraws::LoadTracks(in, *loaded.Parts, tracksDropped);
    if (trackCount < 0)
      return fail("its other animated things couldn't be read");
  }
  // The crowds (2026-11-01; an older file has none)
  int crowdKinds = 0, crowdsDropped = 0;
  if ((section = SectionOf(loaded, TagCrowds)) != nullptr && loaded.Parts)
  {
    replay_store::In in(section->data(), section->size());
    crowdKinds = replay_gamedraws::LoadCrowds(in, *loaded.Parts, crowdsDropped);
    if (crowdKinds < 0)
    {
      util::log::Warning("Replay recordings: the crowds of %s couldn't be read (played without them)", loaded.Folder.c_str());
      crowdKinds = 0;
    }
  }
  // Its missions (2026-11-01; an older file has none)
  {
    std::vector<std::string> names;
    if ((section = SectionOf(loaded, TagMissions)) != nullptr)
    {
      replay_store::In in(section->data(), section->size());
      unsigned count = 0;
      if (in.Get(count) && count <= 16)
      {
        names.resize(count);
        for (std::string& name : names)
          if (!in.Text(name, 256))
            break;
      }
      if (!in.Ok())
        names.clear();
    }
    replay_assetlog::SetRecordedMissions(names);
  }
  if ((section = SectionOf(loaded, TagBundles)) != nullptr)
  {
    replay_store::In in(section->data(), section->size());
    if (!replay_bundles::Load(in))
      return fail("its bundles couldn't be read");
  }
  if ((section = SectionOf(loaded, TagAudio)) != nullptr)
  {
    replay_store::In in(section->data(), section->size());
    if (!replay_audio::Load(in))
      util::log::Warning("Replay recordings: the sound track of %s couldn't be read (played without it)", loaded.Folder.c_str());
  }
  // The game's own sounds, effects, mission items and rope line of the
  // recording before: gone (not in a saved one)
  util::hooks::ReplaySoundsStartRecording();
  util::hooks::ReplaySoundsStopRecording();
  util::hooks::ReplayEffectsStartRecording();
  util::hooks::ReplayEffectsStopRecording();
  util::hooks::MissionItemsStartRecording();
  util::hooks::MissionItemsStopRecording();
  util::hooks::ReplayRopeStartRecording();
  util::hooks::ReplayRopeStopRecording();
  m_LoadedFrom = loaded.Folder;
  // Its kept draws are drawn through the replay's draw hooks: in though nothing records in this session
  replay_shapes::WantDrawHooks();
  int worldKept = 0, worldReady = 0;
  replay_gamedraws::WorldCounts(worldKept, worldReady);
  util::log::Write("Replay recordings: %s loaded: %.1f s, %d people, %d guns, drones or doors with kept draws, %d moving models, "
    "%d shapes, %d kept draws (%d of them the world's own; %d not made again), %d tracks of other animated things (%d not made "
    "again), %d kinds of crowd draws (%d not made again), %d parts not made, %s", loaded.Folder.c_str(), Length(),
    (int)m_People.size(), (int)m_Bound.size(), m_Models.Count(), replay_shapes::Count(), keptDraws, worldKept, dropped, trackCount,
    tracksDropped, crowdKinds, crowdsDropped, loaded.PartsMissing, replay_audio::HasTrack() ? "the sound track" : "no sound track");
  return true;
}

// The world's kept draws that are someone's (2026-10-27): a person's, gun's or
// drone's part drawn without bones (an accessory), or Faith's, would be left
// where it was kept. A single one (reaching at most 3 m) kept within 2.5 m of
// Faith or a recorded person, gun or drone at the time it was kept is let go.
int FaithReplay::DropWorldNearPeople()
{
  return replay_gamedraws::DropWorld([this](double clock, float const* center, float radius) {
    if (radius > 3.f)
      return false;
    float reach = 2.5f + radius;
    float place[3];
    for (auto const& person : m_People)
      if (PersonPlaceAt(*person, clock, place) && Distance3(place, center) < reach)
        return true;
    size_t count = (std::min)(m_Times.size(), m_FaithPlaces.size() / 4);
    if (count)
    {
      size_t at = (size_t)(std::lower_bound(m_Times.begin(), m_Times.begin() + count, clock) - m_Times.begin());
      at = (std::min)(at, count - 1);
      float const* faith = &m_FaithPlaces[at * 4];
      if (faith[0] == faith[0] && Distance3(faith, center) < reach)
        return true;
    }
    return false;
  });
}

// The tracks' instances that are a recorded person's, gun's or drone's drawn
// with its own kept draws (posed as recorded), or Faith's: within 1.6 m (a
// person: the draw's first bone is somewhere on the body; 2 m a gun's or
// drone's) of where it was at the sample's time
int FaithReplay::ThinTracksNearPeople(int& tracks, unsigned long long& bytes)
{
  for (auto& person : m_People)
    person->SeenTimes.clear();
  // (1: a recorded person's, gun's or drone's; 2: Faith's; 0: nobody's)
  int theirs = replay_gamedraws::ThinTracks([this](double clock, float const* place) -> int {
    float there[3];
    float personAway = -1.f;
    for (auto const& person : m_People)
    {
      if (person->Times.empty() || clock < person->Times.front() - 0.2 || clock > person->Times.back() + 0.2 ||
          !DrawnByGameDraws(*person) || !PersonPlaceAt(*person, clock, there))
        continue;
      float reach = person->BodyOffset == CharacterBodyOffset ? 1.6f : 2.f;
      float away = Distance3(there, place);
      if (away < reach)
      {
        // Drawn by the game then (see SeenAt)
        person->SeenTimes.push_back(clock);
        personAway = away;
        break;
      }
    }
    // Faith's when she's nearer it (a cloth of hers beside a recorded person).
    // Her place is where she stands, so she's taken as the line from there up
    // FaithBodyMetres (2026-11-03, the user: "ksec has a weird faith hair next
    // to him": her hair, 1.6 m above her feet, was nearer a KrugerSec's place
    // than hers at 19 of its 33 samples while they fought)
    const float FaithBodyMetres = 1.9f, FaithClothMetres = 0.8f;
    size_t count = (std::min)(m_Times.size(), m_FaithPlaces.size() / 4);
    if (count)
    {
      size_t at = (size_t)(std::lower_bound(m_Times.begin(), m_Times.begin() + count, clock) - m_Times.begin());
      at = (std::min)(at, count - 1);
      float const* faith = &m_FaithPlaces[at * 4];
      float away = 1e30f;
      if (faith[0] == faith[0])
      {
        float up = (std::max)(0.f, (std::min)(FaithBodyMetres, place[1] - faith[1]));
        float onLine[3] = { faith[0], faith[1] + up, faith[2] };
        away = Distance3(onLine, place);
      }
      if (away < FaithClothMetres && (personAway < 0.f || away < personAway))
        return 2;
    }
    return personAway >= 0.f ? 1 : 0;
  }, tracks, bytes);
  for (auto& person : m_People)
  {
    std::sort(person->SeenTimes.begin(), person->SeenTimes.end());
    person->SeenTimes.erase(std::unique(person->SeenTimes.begin(), person->SeenTimes.end()), person->SeenTimes.end());
  }
  return theirs;
}

// Whether the game drew a recorded person around a time (2026-10-27 22:55;
// the user: "ghost runners/runner don't despawn": the Release mission's
// projection runners and its runner stayed in the level, hidden, after the
// game stopped drawing them, so they were drawn to the end): within
// SeenSlack of a time it was seen drawn, or between two such no further
// apart than SeenGap (off the camera's view a moment). True when it's not
// known (no tracks were kept).
bool FaithReplay::SeenAt(PersonTrack const& person, double time) const
{
  const double SeenSlack = 0.5, SeenGap = 5.0;
  // Only the mission's projection runners (2026-11-01, the user: "characters
  // and animations should always be there and not invisible": "seen" is the
  // game's G-buffer drawing of them, so anyone off the camera's view for more
  // than SeenGap was left out then; the hidden ones this was for are the
  // Release mission's projection runners, SpawnTemplate_Projection_Replay)
  if (!strstr(person.Template, "Projection") && !strstr(person.Template, "projection"))
    return true;
  auto const& seen = person.SeenTimes;
  if (seen.empty())
    return true;
  auto next = std::lower_bound(seen.begin(), seen.end(), time);
  if (next != seen.end() && *next - time <= SeenSlack)
    return true;
  if (next == seen.begin())
    return false;
  double before = *(next - 1);
  return time - before <= SeenSlack || (next != seen.end() && *next - before <= SeenGap);
}

void FaithReplay::DrawStoreUI(bool& configChanged)
{
  // A section of its own, always open (2026-11-01, the user: "saved replay
  // shouldn't be a header")
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Text("Saved recordings");
  configChanged |= ImGui::Checkbox("Save every recording##Replay", &m_SaveOn);
  ImGui::HelpMarker("Each recording is saved a moment after it stops, to Cinematic Tools\\Recordings in the game's folder, "
    "with everything it plays: Faith, the people, the moving models, the game's own draws of them (their shaders, meshes "
    "and textures read from the graphics card), the mission's bundles and the sound track. A saved recording loads in any "
    "session and plays as one whose mission is over: the mission's bundles are loaded again, its people and models drawn "
    "from what was kept. The meshes and textures are saved once for all recordings (Recordings\\Parts), and everything is "
    "packed with Windows' own compression. Not saved: her own sounds and the gun effects as the game played them (the "
    "sound track has what was heard), the mission items and the MagRope's line.");
  char status[400];
  replay_store::Status(status, sizeof(status));
  if (status[0])
  {
    ImGui::PushTextWrapPos(0.f);
    ImGui::TextDisabled("%s", status);
    ImGui::PopTextWrapPos();
  }
  if (!m_LoadedFrom.empty())
  {
    // Renamed since it was loaded: its new name
    std::string renamed = replay_store::RenamedTo(m_LoadedFrom);
    if (!renamed.empty())
      m_LoadedFrom = renamed;
    ImGui::TextDisabled("This recording was loaded from %s.", m_LoadedFrom.c_str());
  }
  bool idle = m_State == State_Idle;
  if (m_Times.size() >= 2 && idle && m_LoadedFrom.empty() && ImGui::Button("Save this recording now##Replay"))
    m_SaveAt = 0.0;
  replay_store::List(m_Listed);
  ImGui::TextDisabled("%d saved; their meshes and textures take %.1f MB.", (int)m_Listed.size(), replay_store::PartsBytes() / 1048576.0);
  ImGui::SameLine();
  if (ImGui::SmallButton("Refresh##ReplayStore"))
    replay_store::Refresh();
  bool loading = replay_store::Loading() || m_LoadRequested;
  for (auto const& one : m_Listed)
  {
    ImGui::PushID(one.Folder.c_str());
    ImGui::Text("%s", one.Folder.c_str());
    ImGui::PushTextWrapPos(0.f);
    ImGui::TextDisabled("%.1f s, %.1f MB: %s", one.Seconds, one.Bytes / 1048576.0, one.Summary.c_str());
    ImGui::PopTextWrapPos();
    bool canLoad = idle && !loading;
    if (!canLoad) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.4f);
    if (ImGui::SmallButton("Load") && canLoad)
    {
      m_LoadFolder = one.Folder;
      m_LoadRequested = true;
    }
    if (!canLoad) ImGui::PopStyleVar();
    ImGui::SameLine();
    // Its name, typed in place (Enter or Save name renames its folder)
    if (m_RenameFolder == one.Folder)
    {
      ImGui::PushItemWidth(ImGui::GetFontSize() * 18.f);
      bool enter = ImGui::InputText("##RenameRecording", m_RenameText, sizeof(m_RenameText), ImGuiInputTextFlags_EnterReturnsTrue);
      ImGui::PopItemWidth();
      ImGui::SameLine();
      if (ImGui::SmallButton("Save name") || enter)
      {
        replay_store::Rename(one.Folder, m_RenameText);
        m_RenameFolder.clear();
      }
      ImGui::SameLine();
      if (ImGui::SmallButton("Cancel"))
        m_RenameFolder.clear();
      ImGui::PopID();
      continue;
    }
    if (ImGui::SmallButton("Rename"))
    {
      m_RenameFolder = one.Folder;
      strncpy_s(m_RenameText, one.Folder.c_str(), _TRUNCATE);
      m_ConfirmDelete.clear();
    }
    ImGui::SameLine();
    if (m_ConfirmDelete == one.Folder)
    {
      ImGui::TextColored(ImVec4(1.f, 0.8f, 0.2f, 1.f), "Delete it?");
      ImGui::SameLine();
      if (ImGui::SmallButton("Yes, delete"))
      {
        replay_store::Delete(one.Folder);
        m_ConfirmDelete.clear();
      }
      ImGui::SameLine();
      if (ImGui::SmallButton("No"))
        m_ConfirmDelete.clear();
    }
    else if (ImGui::SmallButton("Delete"))
      m_ConfirmDelete = one.Folder;
    ImGui::PopID();
  }
}
