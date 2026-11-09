#include "DryShapes.h"
#include "FanTable.h"
#include "GroundMarks.h"
#include "SnowBodies.h"
#include "WetLook.h"
#include "Weather.h"
#include "../Camera/AttachTargets.h"
#include "../Camera/CameraCollision.h"
#include "../Util/Util.h"
#include <Windows.h>
#include <DirectXMath.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <mutex>
#include <vector>

using namespace DirectX;

// The wet surfaces' pass (WetLook.hlsl) works from the game's depth alone, so
// whatever faces up got wet and puddled: Faith's arms and legs in front of
// her own camera (most when she jumps), people's shoulders, cars' roofs and
// bonnets, the trains' tops. The game's stencil can't tell them apart there:
// by Present its sky pass has set every world pixel's to 0 (see
// Research_Greenscreen, "sky/main", depth-fail Zero). So the pass is given
// boxes round them, from what the tools already find for the free camera to
// follow (Camera/AttachTargets: the game's own instance lists and the
// swarms' members, read from memory, nothing called), and leaves what's in
// a box as the game drew it:
// - Faith (her transform; and while the camera is in her head, what's near
//   it, see Near);
// - people: characters (Faith's enemies and the rest) and the crowds;
// - road traffic (the Cars swarms: cars, trucks and motorcycles in one swarm,
//   which member is which isn't known, so every one gets a box a truck fits
//   in), the flying traffic and drones, vehicle entities (the VTOL);
// - the monorail trains' carriages and vehicle models (their meshes' boxes).
// Found again every ScanMs, within the wet look's reach; their places are
// read again every frame. A box's floor is above the ground they stand on, so
// the puddle at Faith's feet or round a car stays.
namespace
{
  const ULONGLONG ScanMs = 1000;   // (400 before 2026-10-26: each scan took 3 - 5 ms of the game thread)
  const int MostFound = 2048;          // one scan's finds (static models within the radius among them), at first
  const int MostFoundGrown = 16384;    // grown to while scans fill it (see Rescan)
  const int MostKept = 160;            // looked at every frame, the nearest
  const float ScanBeyond = 15.f;       // m past the wet look's reach (things move between scans)
  const float MostScanRadius = 175.f;
  const float TrainsBeyond = 80.f;     // trains are listed wherever they are; kept this much past the radius
  const float MotionPad = 1.5f;        // a box grows along the way it moved since the last frame, times this
  const float EdgePad = 0.25f;         // m round a model's own box
  // The city's fans (FanTable.h): their boxes this much bigger each way, so
  // their surfaces are inside the kept boxes' faded edge (WetLook.hlsl's
  // KeptDryKind, 25 cm)
  const float FanPad = 0.3f;

  // The shapes (metres from where it stands, its origin: feet or wheels)
  // (Body: Faith's or a person's, a little wet out in the rain instead of
  // left as drawn: WetLook.hlsl's BodyWetness)
  struct Upright { float HalfSide, HalfLength, Bottom, Top; bool Body; };
  const Upright FaithShape   = { 0.55f, 0.55f, 0.12f, 2.5f, true };
  const Upright PersonShape  = { 0.6f, 0.6f, 0.12f, 2.4f, true };
  // A swarm member's record isn't where it's drawn up and down (the user,
  // 2026-10-26, a screenshot: a dry box round each car on the wet road, its
  // edge a frame round the car): road traffic's records keep the swarm's own
  // height (only x and z change, read live before) while the cars are drawn
  // on the road, higher there (the road was inside the boxes, so less than
  // 4.5 m higher). So the people and the cars of the swarms are put on the
  // ground found under them (FindGround: the game's collision straight down
  // at their place, from TrafficGroundAbove / PersonGroundAbove over the
  // record to GroundBelow under it: over a person only a little, so an
  // awning over them isn't taken for their ground), their boxes from just
  // over it; one whose ground isn't known yet gets no box (the wet look on it
  // for a moment rather than a dry patch on the road). Looked for again
  // every GroundRefreshMs, at most GroundSweepsPerFrame a frame.
  const Upright TrafficShape = { 1.6f, 6.5f, 0.12f, 4.2f };   // over the road
  const Upright FlyingShape  = { 2.5f, 2.5f, -1.5f, 2.f };
  const Upright VehicleShape = { 7.f, 9.f, 0.3f, 6.f };       // a vehicle entity (the VTOL): its size not measured
  // Their steps and wheels in the puddles (wetlook::OtherStep, from their
  // moves): a person a step each PersonStride walked (PersonFootApart to
  // alternate sides), a car its two back wheels' rings each WheelEvery driven
  // (CarWheelBack behind its middle, CarWheelApart to each side); within
  // OtherStepReach of the camera, moving, not after a jump
  const float PersonStride = 0.75f, PersonFootApart = 0.1f;
  const float WheelEvery = 1.2f, CarWheelBack = 1.4f, CarWheelApart = 0.8f;
  const float OtherStepReach = 40.f;
  // Their marks on the ground (GroundMarks.h: prints in the snow, trails in
  // the water): each step's sole, and each back wheel's path every frame
  // (a car's tyre 22 cm wide; a path over WheelPathMost in a frame is a jump,
  // left out). All the road traffic as cars: a motorcycle can't be told from
  // a car yet. (2026-11-01, the user: "cars/motorbikes should leave behind
  // water trails if on puddles and pedestrians should have the pudde ripple
  // effect when they walk", "Do the same about snow too".)
  const float TyreHalfWidth = 0.11f, WheelPathMost = 3.f;
  // A back wheel out of a puddle carries its water on (its wet tyre track,
  // GroundMarks.h), wearing off over TyreWetMetres driven; whether it's in a
  // puddle looked at every TyreLookMetres (wetlook::PuddleUnder). (2026-11-01,
  // the user said "Yes, add them" to wet tyre tracks after a puddle.)
  const float TyreWetMetres = 12.f, TyreLookMetres = 0.5f;
  const float TrafficGroundAbove = 5.f, PersonGroundAbove = 1.5f, GroundBelow = 4.f;   // m over / under a record
  // The rain kept out of people and drones (2026-10-26, the user: "rain also
  // shouldn't go through people and all the ksec types"): a box round each
  // one's body only (narrower and lower than the wet look's), the
  // dry::MostBodies nearest within BodyReach, given to the game's rain as
  // exclusion volumes (Weather.cpp's UpdateRoofs, after the roofs). Not
  // Faith's: the camera is in her head. Looked for while it rains even with
  // the wet look off.
  const Upright PersonBody = { 0.35f, 0.35f, 0.f, 2.05f };
  const Upright FlyingBody = { 1.f, 1.f, -0.8f, 0.8f };
  const float BodyReach = 30.f;
  const ULONGLONG GroundRefreshMs = 1000;
  const float GroundRefreshMetres = 2.f;   // or once it's this far from where it was looked for (a car on a slope)
  const int GroundSweepsPerFrame = 16;
  // The camera in Faith's head: her own arms and legs are in front of it
  const float HeadAcross = 0.8f;       // the camera this close to her (level) ...
  const float HeadLow = 0.3f, HeadHigh = 2.2f; // ... and this high over her feet

  enum Shape { ShapePerson, ShapeTraffic, ShapeFlying, ShapeVehicle, ShapeModel };

  struct Kept
  {
    attach::Target Target;
    int Shape;
    bool HasLast;
    float Last[3];
    bool GroundKnown;     // swarm members: the ground under them (see FindGround)
    float Ground;
    ULONGLONG GroundTick;
    float GroundAt[2];    // where it was looked for (x, z)
    float Walked = 0.f;   // how far since its last step or wheel ring (see PersonStride)
    float Side = 1.f;     // its next step's side
    bool WheelsKnown = false;   // its back wheels' places last frame (see TyreHalfWidth)
    float Wheels[2][3] = {};
    float Carried[2] = {};      // the water each carries (see TyreWetMetres)
    float Looked[2] = {};       // driven since a puddle was looked for under it
  };

  struct Ranked
  {
    dry::Box Box;
    float Distance;
  };

  std::mutex g_Mutex;
  std::vector<dry::Box> g_Boxes;     // under g_Mutex
  std::vector<dry::Box> g_Bodies;    // under g_Mutex (see PersonBody)
  dry::Near g_Near{};                // under g_Mutex
  bool g_FaithKnown = false;         // under g_Mutex: her feet at the last update
  float g_FaithFeet[3] = {};
  // Her transform's forward row (x, z, unit) at the last update (see
  // dry::FaithForward), under g_Mutex; and as the update found it (game thread)
  float g_FaithForward[2] = {};
  bool g_FaithForwardKnown = false;
  float g_FaithLastForward[2] = {};
  bool g_FaithLastForwardOk = false;
  // Game thread only
  std::vector<attach::Target> g_Found;
  int g_FoundRoom = MostFound;       // game thread: the scan's list's size now
  std::vector<Kept> g_Kept;
  std::vector<Ranked> g_Ranked;
  std::vector<Ranked> g_BodyRanked;
  attach::Scan g_Scan;
  ULONGLONG g_ScanTick = 0;
  bool g_FaithHasLast = false;
  float g_FaithLast[3] = {};
  ULONGLONG g_LoggedTick = 0;
  int g_LoggedCounts[5] = { -1, -1, -1, -1, -1 };

  // The render view's camera: its place (+ 0x40 is its LinearTransform, rows
  // right, up, forward, position) and the way it looks (minus row 2)
  bool ReadCamera(__int64 pRenderView, float* camera, float* look)
  {
    __try
    {
      float const* m = (float const*)(pRenderView + 0x40);
      for (int i = 0; i < 3; ++i)
      {
        camera[i] = m[12 + i];
        look[i] = -m[8 + i];
      }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
    return std::isfinite(camera[0] + camera[1] + camera[2] + look[0] + look[1] + look[2]);
  }

  bool HasNoCase(char const* text, char const* part)
  {
    size_t n = strlen(part);
    for (; *text; ++text)
      if (_strnicmp(text, part, n) == 0)
        return true;
    return false;
  }

  // The ground under a swarm member (see TrafficShape): the first surface
  // facing up straight down from above over its record to GroundBelow under
  // it
  bool FindGround(float const* at, float above, float& ground)
  {
    float from[3] = { at[0], at[1] + above, at[2] }, to[3] = { at[0], at[1] - GroundBelow, at[2] };
    float fraction = 1.f, normal[3] = {};
    if (!collision::Sweep(from, to, 0.f, &fraction, normal) || normal[1] < 0.5f)
      return false;
    ground = from[1] + (to[1] - from[1]) * fraction;
    return std::isfinite(ground);
  }

  // Which shape a find gets, -1 none
  int ShapeOf(attach::Target const& t)
  {
    switch (attach::GroupOf(t))
    {
    case attach::Group::People: return ShapePerson;
    case attach::Group::Cars: return ShapeTraffic;
    case attach::Group::Drones: return t.Agent >= 0 || strcmp(attach::Kind(t.Class), "drone") == 0 ? ShapeFlying : -1;
    case attach::Group::Trains: return t.HasBox ? ShapeModel : -1;
    default: break;
    }
    if (t.Agent >= 0)
      return -1;
    if (strcmp(attach::Kind(t.Class), "vehicle") == 0)
      return ShapeVehicle;
    // Vehicle models (objects/props/vehicles/truck/vehicletruck_01; vtol)
    if (t.HasBox && (t.Named == attach::Named_Vtol || HasNoCase(t.Label, "vehicle")))
      return ShapeModel;
    return -1;
  }

  // Into the frame's list: grown along the way it moved, and how far it is
  void Add(dry::Box& b, float const* moved, float const* camera, float reach)
  {
    for (int i = 0; i < 3; ++i)
      if (moved)
        b.Half[i] += MotionPad * fabsf(moved[0] * b.Axis[i][0] + moved[1] * b.Axis[i][1] + moved[2] * b.Axis[i][2]);
    float dx = b.Middle[0] - camera[0], dy = b.Middle[1] - camera[1], dz = b.Middle[2] - camera[2];
    float size = (std::max)(b.Half[0], (std::max)(b.Half[1], b.Half[2]));
    float distance = (std::max)(0.f, sqrtf(dx * dx + dy * dy + dz * dz) - size * 1.733f);
    if (distance <= reach && std::isfinite(distance))
      g_Ranked.push_back({ b, distance });
  }

  // An upright box where something stands, its length the way it faces (level)
  void AddUpright(float const* at, float const* forward, Upright const& shape, float const* moved, float const* camera,
    float reach)
  {
    float fx = forward[0], fz = forward[2];
    float length = sqrtf(fx * fx + fz * fz);
    if (length < 1e-3f)
    {
      fx = 0.f;
      fz = 1.f;
    }
    else
    {
      fx /= length;
      fz /= length;
    }
    dry::Box b{};
    b.Middle[0] = at[0];
    b.Middle[1] = at[1] + (shape.Bottom + shape.Top) * 0.5f;
    b.Middle[2] = at[2];
    // right = up x forward
    b.Axis[0][0] = fz; b.Axis[0][1] = 0.f; b.Axis[0][2] = -fx;
    b.Axis[1][0] = 0.f; b.Axis[1][1] = 1.f; b.Axis[1][2] = 0.f;
    b.Axis[2][0] = fx; b.Axis[2][1] = 0.f; b.Axis[2][2] = fz;
    b.Half[0] = shape.HalfSide;
    b.Half[1] = (shape.Top - shape.Bottom) * 0.5f;
    b.Half[2] = shape.HalfLength;
    b.Body = shape.Body;
    // (a vehicle on the ground too, its box from just over it: the road under
    // it stays the road's, see AddModel)
    b.Floored = !shape.Body && shape.Bottom >= 0.f;
    b.FeetBelow = shape.Body || b.Floored ? shape.Bottom : 0.f;
    Add(b, moved, camera, reach);
  }

  // A body's box for the rain (see PersonBody), where it stands, its length
  // the way it faces (level)
  void AddBody(float const* at, float const* forward, Upright const& shape, float const* camera)
  {
    float fx = forward[0], fz = forward[2];
    float length = sqrtf(fx * fx + fz * fz);
    if (length < 1e-3f)
    {
      fx = 0.f;
      fz = 1.f;
    }
    else
    {
      fx /= length;
      fz /= length;
    }
    dry::Box b{};
    b.Middle[0] = at[0];
    b.Middle[1] = at[1] + (shape.Bottom + shape.Top) * 0.5f;
    b.Middle[2] = at[2];
    b.Axis[0][0] = fz; b.Axis[0][1] = 0.f; b.Axis[0][2] = -fx;
    b.Axis[1][0] = 0.f; b.Axis[1][1] = 1.f; b.Axis[1][2] = 0.f;
    b.Axis[2][0] = fx; b.Axis[2][1] = 0.f; b.Axis[2][2] = fz;
    b.Half[0] = shape.HalfSide;
    b.Half[1] = (shape.Top - shape.Bottom) * 0.5f;
    b.Half[2] = shape.HalfLength;
    float dx = b.Middle[0] - camera[0], dy = b.Middle[1] - camera[1], dz = b.Middle[2] - camera[2];
    float distance = sqrtf(dx * dx + dy * dy + dz * dz);
    if (distance <= BodyReach && std::isfinite(distance))
      g_BodyRanked.push_back({ b, distance });
  }

  // A model's own box (local, from its mesh) where its transform puts it
  void AddModel(XMFLOAT4X4 const& m, XMFLOAT3 const& lo, XMFLOAT3 const& hi, float const* moved, float const* camera,
    float reach)
  {
    float local[3] = { (lo.x + hi.x) * 0.5f, (lo.y + hi.y) * 0.5f, (lo.z + hi.z) * 0.5f };
    float half[3] = { (hi.x - lo.x) * 0.5f, (hi.y - lo.y) * 0.5f, (hi.z - lo.z) * 0.5f };
    dry::Box b{};
    for (int i = 0; i < 3; ++i)
      b.Middle[i] = m.m[3][i];
    for (int i = 0; i < 3; ++i)
    {
      float const* row = m.m[i];
      float scale = sqrtf(row[0] * row[0] + row[1] * row[1] + row[2] * row[2]);
      if (!(scale > 1e-4f) || !std::isfinite(scale))
        return;
      for (int j = 0; j < 3; ++j)
      {
        b.Axis[i][j] = row[j] / scale;
        b.Middle[j] += local[i] * row[j];
      }
      b.Half[i] = half[i] * scale + EdgePad;
    }
    // Standing upright (a parked car or truck, a train's carriage: its mesh's
    // bottom the ground it stands on): the ground under it and round it in its
    // box left as it is, the wet look and the snow on it; only what's over it
    // kept (2026-11-01, the user: "Cars have a weird square around them with
    // rain": its box went EdgePad under its wheels, so the road in it and the
    // margin round it were kept dry, a rectangle round the car)
    if (b.Axis[1][1] > 0.9f)
    {
      b.Floored = true;
      b.FeetBelow = -EdgePad;
    }
    Add(b, moved, camera, reach);
  }

  // Everything near found again (see ScanMs); what was kept before keeps its
  // state (a swarm member's heading, the last place)
  void Rescan(float const* camera, float const* look, __int64 pSoldier, float const* faith, float radius)
  {
    bool fromFaith = false;
    if (!pSoldier || !attach::FindClasses(pSoldier, fromFaith))
    {
      g_Kept.clear();
      return;
    }
    attach::View view{};
    view.Camera = XMFLOAT3(camera[0], camera[1], camera[2]);
    view.Look = XMFLOAT3(look[0], look[1], look[2]);
    view.Skip = pSoldier;
    if (faith)
    {
      view.SkipPosition = XMFLOAT3(faith[12], faith[13], faith[14]);
      view.HasSkipPosition = true;
    }
    memset(&g_Scan, 0, sizeof(g_Scan));
    g_Found.resize(g_FoundRoom);
    int count;
    {
      // (timed on its own, see the weather's performance line)
      wetlook::ScopedTime timed(wetlook::TimePeopleScan);
      count = attach::ScanNearby(view, radius, g_Found.data(), g_FoundRoom, g_Scan);
    }
    // (its list full: the people and the traffic were the ones left out, the
    // 2026-11-01 logs: "0 people, 0 road traffic ... the scan's list was full";
    // the next scan has twice the room, up to MostFoundGrown. This list is
    // the wet look's own: the camera's, the replay's and the others' aren't
    // changed.)
    if (g_Scan.Dropped > 0 && g_FoundRoom < MostFoundGrown)
    {
      g_FoundRoom = (std::min)(g_FoundRoom * 2, MostFoundGrown);
      util::log::Write("Wet surfaces: the scan's list was full (%d left out): %d places from the next scan", g_Scan.Dropped,
        g_FoundRoom);
    }
    std::vector<Kept> kept;
    kept.reserve(MostKept);
    for (int i = 0; i < count; ++i)
    {
      attach::Target const& t = g_Found[i];
      int shape = ShapeOf(t);
      if (shape < 0 || t.Distance > radius + (attach::ListedAtAnyDistance(t) ? TrainsBeyond : 0.f))
        continue;
      Kept k{ t, shape, false, {}, false, 0.f, 0, {} };
      for (Kept const& old : g_Kept)
        if (attach::SameTarget(old.Target, t) && old.Target.VTable == t.VTable)
        {
          k.Target = old.Target;
          k.Target.Distance = t.Distance;
          k.HasLast = old.HasLast;
          memcpy(k.Last, old.Last, sizeof(k.Last));
          k.GroundKnown = old.GroundKnown;
          k.Ground = old.Ground;
          k.GroundTick = old.GroundTick;
          memcpy(k.GroundAt, old.GroundAt, sizeof(k.GroundAt));
          // (its stride and wheels go on through the scan: its steps kept
          // in step, its wheels' paths unbroken)
          k.Walked = old.Walked;
          k.Side = old.Side;
          k.WheelsKnown = old.WheelsKnown;
          memcpy(k.Carried, old.Carried, sizeof(k.Carried));
          memcpy(k.Looked, old.Looked, sizeof(k.Looked));
          memcpy(k.Wheels, old.Wheels, sizeof(k.Wheels));
          break;
        }
      kept.push_back(k);
    }
    std::sort(kept.begin(), kept.end(), [](Kept const& a, Kept const& b) { return a.Target.Distance < b.Target.Distance; });
    if (kept.size() > (size_t)MostKept)
      kept.resize(MostKept);
    g_Kept.swap(kept);

    // What's kept, for the log: when it changes, at most every 30 s
    int counts[5] = {};
    for (Kept const& k : g_Kept)
      ++counts[k.Shape];
    ULONGLONG now = GetTickCount64();
    if (memcmp(counts, g_LoggedCounts, sizeof(counts)) != 0 && (!g_LoggedTick || now - g_LoggedTick >= 30000))
    {
      g_LoggedTick = now;
      memcpy(g_LoggedCounts, counts, sizeof(counts));
      util::log::Write("Wet surfaces: kept dry within %.0f m: Faith%s, %d people, %d road traffic, %d drones and flying "
        "traffic, %d vehicles, %d trains' carriages and vehicle models (%d found in all%s)", radius, faith ? "" : " (not found)",
        counts[ShapePerson], counts[ShapeTraffic], counts[ShapeFlying], counts[ShapeVehicle], counts[ShapeModel], count,
        g_Scan.Dropped ? ", some left out: the scan's list was full" : "");
    }
  }
}

void dry::Update(__int64 pRenderView, __int64 pSoldier, float const* faith)
{
  // (on the game thread while it rains or the wet look is on: where the
  // drips off the slopes' ends land)
  if (pRenderView)
  {
    wetlook::LandSlopeEnds();
    wetlook::LandBodies();
  }
  wetlook::ScopedTime timed(wetlook::TimePeople);
  // (while it rains, at least as far as the rain's bodies, see PersonBody)
  float reach = wetlook::Reach();
  if (weather::IsRaining(nullptr))
    reach = (std::max)(reach, BodyReach);
  float camera[3], look[3];
  if (reach <= 0.f || !pRenderView || !ReadCamera(pRenderView, camera, look))
  {
    g_Kept.clear();
    g_ScanTick = 0;
    g_FaithHasLast = false;
    std::lock_guard<std::mutex> lock(g_Mutex);
    g_Boxes.clear();
    g_Bodies.clear();
    g_Near.On = false;
    g_FaithKnown = false;
    return;
  }
  // (the snow pressed in by bodies: Faith's and the KrugerSec's, posed as the
  // game has them now; SnowBodies.cpp, the snow session's)
  snowbody::Update(pSoldier, faith, camera);
  reach = (std::min)(reach, MostScanRadius);
  ULONGLONG now = GetTickCount64();
  if (!g_ScanTick || now - g_ScanTick >= ScanMs)
  {
    g_ScanTick = now;
    Rescan(camera, look, pSoldier, faith, reach + ScanBeyond);
  }

  g_Ranked.clear();
  g_BodyRanked.clear();
  dry::Near close{};
  // Faith, and what's in front of her camera while it's in her head
  if (faith && std::isfinite(faith[12] + faith[13] + faith[14]))
  {
    float moved[3] = { faith[12] - g_FaithLast[0], faith[13] - g_FaithLast[1], faith[14] - g_FaithLast[2] };
    bool step = g_FaithHasLast && fabsf(moved[0]) + fabsf(moved[1]) + fabsf(moved[2]) < 10.f;
    AddUpright(faith + 12, faith + 8, FaithShape, step ? moved : nullptr, camera, reach);
    memcpy(g_FaithLast, faith + 12, sizeof(g_FaithLast));
    g_FaithHasLast = true;
    float fl = sqrtf(faith[8] * faith[8] + faith[10] * faith[10]);
    g_FaithLastForwardOk = fl > 1e-3f && std::isfinite(fl);
    if (g_FaithLastForwardOk)
    {
      g_FaithLastForward[0] = faith[8] / fl;
      g_FaithLastForward[1] = faith[10] / fl;
    }
    float ax = camera[0] - faith[12], az = camera[2] - faith[14], up = camera[1] - faith[13];
    if (ax * ax + az * az <= HeadAcross * HeadAcross && up >= HeadLow && up <= HeadHigh)
    {
      close.On = true;
      close.Floor = faith[13] + FaithShape.Bottom;
    }
  }
  else
    g_FaithHasLast = false;

  // The rest, where they are now
  int groundSweeps = 0;
  bool canSweep = collision::Available();
  for (size_t i = 0; i < g_Kept.size();)
  {
    Kept& k = g_Kept[i];
    XMFLOAT4X4 m;
    if (!attach::ReadTransform(k.Target, m) || !std::isfinite(m._41 + m._42 + m._43))
    {
      g_Kept.erase(g_Kept.begin() + i);
      continue;
    }
    float at[3] = { m._41, m._42, m._43 };
    float moved[3] = { at[0] - k.Last[0], at[1] - k.Last[1], at[2] - k.Last[2] };
    float step = sqrtf(moved[0] * moved[0] + moved[1] * moved[1] + moved[2] * moved[2]);
    bool jumped = k.HasLast && step > attach::JumpLimit(k.Target);
    if (jumped && !attach::KeepsThroughJumps(k.Target))
    {
      // Not the same one any more (a swarm's record given to a new member)
      g_Kept.erase(g_Kept.begin() + i);
      continue;
    }
    float const* motion = k.HasLast && !jumped ? moved : nullptr;
    memcpy(k.Last, at, sizeof(at));
    k.HasLast = true;
    float forward[3] = { m._31, m._32, m._33 };
    // A swarm's people and cars on the ground under them (see TrafficShape)
    bool onGround = k.Target.Agent >= 0 && (k.Shape == ShapePerson || k.Shape == ShapeTraffic);
    float gx = at[0] - k.GroundAt[0], gz = at[2] - k.GroundAt[1];
    if (onGround && canSweep && groundSweeps < GroundSweepsPerFrame && (!k.GroundKnown ||
        now - k.GroundTick >= GroundRefreshMs || gx * gx + gz * gz > GroundRefreshMetres * GroundRefreshMetres))
    {
      ++groundSweeps;
      k.GroundAt[0] = at[0];
      k.GroundAt[1] = at[2];
      float ground = 0.f;
      k.GroundKnown = FindGround(at, k.Shape == ShapeTraffic ? TrafficGroundAbove : PersonGroundAbove, ground);
      k.Ground = ground;
      k.GroundTick = now;
    }
    if (onGround)
    {
      if (!k.GroundKnown)
      {
        ++i;
        continue;
      }
      at[1] = k.Ground;
    }
    // Their steps and wheels in the puddles (see PersonStride)
    if (motion && (k.Shape == ShapePerson || k.Shape == ShapeTraffic))
    {
      float along = sqrtf(motion[0] * motion[0] + motion[2] * motion[2]);
      float cx = at[0] - camera[0], cz = at[2] - camera[2];
      if (along > 1e-3f && cx * cx + cz * cz < OtherStepReach * OtherStepReach)
      {
        k.Walked += along;
        float across[2] = { -motion[2] / along, motion[0] / along };
        if (k.Shape == ShapePerson && k.Walked >= PersonStride)
        {
          k.Walked = fmodf(k.Walked, PersonStride);
          k.Side = -k.Side;
          float foot[3] = { at[0] + across[0] * PersonFootApart * k.Side, at[1], at[2] + across[1] * PersonFootApart * k.Side };
          wetlook::OtherStep(foot, 1.f, 1, true);
          // (and its sole on the ground, the way it walks: see TyreHalfWidth)
          float way[2] = { motion[0] / along, motion[2] / along };
          groundmarks::Step(foot, way);
        }
        else if (k.Shape == ShapeTraffic && k.Walked >= WheelEvery)
        {
          k.Walked = fmodf(k.Walked, WheelEvery);
          float fl = sqrtf(forward[0] * forward[0] + forward[2] * forward[2]);
          float back[2] = { fl > 1e-3f ? forward[0] / fl : motion[0] / along, fl > 1e-3f ? forward[2] / fl : motion[2] / along };
          for (int w = -1; w <= 1; w += 2)
          {
            float wheel[3] = { at[0] - back[0] * CarWheelBack + across[0] * CarWheelApart * w, at[1],
              at[2] - back[1] * CarWheelBack + across[1] * CarWheelApart * w };
            wetlook::OtherStep(wheel, 1.f, 5, true);
          }
        }
        // Its back wheels' paths since the frame before, on the ground (see
        // TyreHalfWidth), across its own way (not its move's: steadier)
        if (k.Shape == ShapeTraffic)
        {
          // (diagnostic: the first few members' records in the log, to find
          // which is a motorcycle; GroundMarks.h)
          groundmarks::ProbeTraffic(k.Target.Boids, k.Target.Agent);
          float fl = sqrtf(forward[0] * forward[0] + forward[2] * forward[2]);
          float ahead[2] = { fl > 1e-3f ? forward[0] / fl : motion[0] / along, fl > 1e-3f ? forward[2] / fl : motion[2] / along };
          float side[2] = { -ahead[1], ahead[0] };
          for (int w = 0; w < 2; ++w)
          {
            float s = w ? 1.f : -1.f;
            float wheel[3] = { at[0] - ahead[0] * CarWheelBack + side[0] * CarWheelApart * s, at[1],
              at[2] - ahead[1] * CarWheelBack + side[1] * CarWheelApart * s };
            float px = wheel[0] - k.Wheels[w][0], pz = wheel[2] - k.Wheels[w][2];
            float path = sqrtf(px * px + pz * pz);
            if (k.WheelsKnown && path < WheelPathMost)
            {
              // (its water: all of it in a puddle, wearing off on the way;
              // looked for only while the wet look is drawn)
              k.Carried[w] = (std::max)(0.f, k.Carried[w] - path / TyreWetMetres);
              k.Looked[w] += path;
              if (k.Looked[w] >= TyreLookMetres)
              {
                k.Looked[w] = 0.f;
                if (wetlook::PuddleUnder(wheel))
                  k.Carried[w] = 1.f;
              }
              groundmarks::Wheel(k.Wheels[w], wheel, TyreHalfWidth, k.Carried[w]);
            }
            memcpy(k.Wheels[w], wheel, sizeof(wheel));
          }
          k.WheelsKnown = true;
        }
      }
    }
    switch (k.Shape)
    {
    case ShapePerson:
      AddUpright(at, forward, PersonShape, motion, camera, reach);
      AddBody(at, forward, PersonBody, camera);
      break;
    case ShapeTraffic: AddUpright(at, forward, TrafficShape, motion, camera, reach); break;
    case ShapeFlying:
      AddUpright(at, forward, FlyingShape, motion, camera, reach);
      AddBody(at, forward, FlyingBody, camera);
      break;
    case ShapeVehicle: AddUpright(at, forward, VehicleShape, motion, camera, reach); break;
    case ShapeModel: AddModel(m, k.Target.BoxMin, k.Target.BoxMax, motion, camera, reach); break;
    }
    ++i;
  }

  // The city's fans (2026-10-27, the user: "make snow/water not apply on any
  // fan surface"; FanTable.h): kept as the game draws them, the wet look and
  // the snow not on them, the nearest within reach as the rest. (A flat axis,
  // none in the table, would take the other two's cross.)
  for (FanBoxData const& f : FanBoxes)
  {
    dry::Box b{};
    float length[3];
    for (int i = 0; i < 3; ++i)
    {
      b.Middle[i] = f.Middle[i];
      length[i] = sqrtf(f.Half[i][0] * f.Half[i][0] + f.Half[i][1] * f.Half[i][1] + f.Half[i][2] * f.Half[i][2]);
      for (int j = 0; j < 3; ++j)
        b.Axis[i][j] = length[i] > 1e-4f ? f.Half[i][j] / length[i] : 0.f;
      b.Half[i] = length[i] + FanPad;
    }
    for (int i = 0; i < 3; ++i)
      if (!(length[i] > 1e-4f))
      {
        float const* p = b.Axis[(i + 1) % 3];
        float const* q = b.Axis[(i + 2) % 3];
        b.Axis[i][0] = p[1] * q[2] - p[2] * q[1];
        b.Axis[i][1] = p[2] * q[0] - p[0] * q[2];
        b.Axis[i][2] = p[0] * q[1] - p[1] * q[0];
      }
    Add(b, nullptr, camera, reach);
  }

  std::sort(g_Ranked.begin(), g_Ranked.end(), [](Ranked const& a, Ranked const& b) { return a.Distance < b.Distance; });
  std::sort(g_BodyRanked.begin(), g_BodyRanked.end(), [](Ranked const& a, Ranked const& b) { return a.Distance < b.Distance; });
  size_t count = (std::min)(g_Ranked.size(), (size_t)dry::MostBoxes);
  size_t bodies = (std::min)(g_BodyRanked.size(), (size_t)dry::MostBodies);
  std::lock_guard<std::mutex> lock(g_Mutex);
  g_Boxes.resize(count);
  for (size_t i = 0; i < count; ++i)
    g_Boxes[i] = g_Ranked[i].Box;
  g_Bodies.resize(bodies);
  for (size_t i = 0; i < bodies; ++i)
    g_Bodies[i] = g_BodyRanked[i].Box;
  g_Near = close;
  g_FaithKnown = g_FaithHasLast;
  if (g_FaithHasLast)
    memcpy(g_FaithFeet, g_FaithLast, sizeof(g_FaithFeet));
  g_FaithForwardKnown = g_FaithHasLast && g_FaithLastForwardOk;
  if (g_FaithForwardKnown)
    memcpy(g_FaithForward, g_FaithLastForward, sizeof(g_FaithForward));
}

bool dry::FaithFeet(float* at)
{
  std::lock_guard<std::mutex> lock(g_Mutex);
  if (g_FaithKnown)
    memcpy(at, g_FaithFeet, sizeof(g_FaithFeet));
  return g_FaithKnown;
}

bool dry::FaithForward(float* way)
{
  std::lock_guard<std::mutex> lock(g_Mutex);
  if (g_FaithForwardKnown)
    memcpy(way, g_FaithForward, sizeof(g_FaithForward));
  return g_FaithForwardKnown;
}

int dry::GetBodies(Box* out, int most)
{
  std::lock_guard<std::mutex> lock(g_Mutex);
  int count = (std::min)((int)g_Bodies.size(), most);
  for (int i = 0; i < count; ++i)
    out[i] = g_Bodies[i];
  return count;
}

int dry::Get(Box* out, int most, Near& close)
{
  std::lock_guard<std::mutex> lock(g_Mutex);
  int count = (std::min)((int)g_Boxes.size(), most);
  for (int i = 0; i < count; ++i)
    out[i] = g_Boxes[i];
  close = g_Near;
  return count;
}
