#include "ReplayModels.h"
#include "ReplayShapes.h"
#include "ReplayGameDraws.h"
#include "ReplayStore.h"
#include "AttachTargets.h"
#include "../Main.h"
#include "../Util/Util.h"
#include <DirectXMath.h>
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <string>

using namespace DirectX;

namespace
{
  // The static models' getTransform and setTransform (vtable slots 24 and 25,
  // shared by ClientStaticModelEntity and PamClientStaticModelEntity). The
  // setter is `mov r8b, 1; jmp 0x1432EB1F0`: it writes the 4 rows where the
  // getter reads them, then (flag bit 3 of entity + 0x18) the entity's
  // "transform changed" (vtable + 0xE0), which moves what's drawn.
  const __int64 GetTransformRva = 0x32E1440;
  const __int64 SetTransformRva = 0x32E8300;
  const float ScanRadius = 60.f;       // around Faith (40 until 2026-10-27: the models that don't move are kept too now)
  const double ScanInterval = 1.0;     // she can run 10 m a second
  const size_t MaxCandidates = 6000;
  const size_t MaxMovers = 2000;
  const float SameEpsilon = 1e-4f;
  // A recorded model that isn't there any more is looked for again while
  // the replay plays (see ReplayModels::Refind): the same mesh at its first
  // place, within RefindRadius of the replayed Faith, a scan every
  // RefindScanInterval at most, each one looked for every RefindInterval
  const float RefindDistance = 0.1f;
  const float RefindRadius = 100.f;
  const double RefindScanInterval = 2.0;
  // How long the game has to (not) draw a model it has before its recorded
  // copy is drawn (let go): see Play
  const double LiveDrawnHoldSeconds = 0.5;
  // A model the game shows and hides (see ShowAndFollow): its own instance
  // within OwnSeenMetres of where it was in the game's draws read back, a
  // neighbour's within NearSeenMetres; HiddenClocksNeeded times a neighbour
  // was drawn without it (at least 0.5 s apart), none of its own within
  // OwnSeenSlack of them, Faith no further from it then than at the farthest
  // the game drew it (+ FaithSeenSlackMetres; FaithNeverMetres for one never
  // drawn)
  const float OwnSeenMetres = 0.3f, NearSeenMetres = 4.f;
  const float OwnSeenSlack = 0.6f;
  const int HiddenClocksNeeded = 3;
  const double ShownSlack = 0.5;
  const float FaithSeenSlackMetres = 3.f, FaithNeverMetres = 15.f;
  // The camera's view (see ShowAndFollow): times in its picture counted as
  // its neighbours' are, within ViewSlackMetres more than the farthest the
  // game drew it from the camera; only for one drawn at ViewSeenNeeded of its
  // times in view between its first and last draw, and at most of them
  const float ViewSlackMetres = 5.f;
  const int ViewSeenNeeded = 3;
  const unsigned ShowTag = 0x574F4853;   // "SHOW"
  // The game's own taken out of sight while it plays (see Play): shrunk to
  // HiddenScale where it stands and moved HiddenDropMetres down
  const float HiddenScale = 1e-4f, HiddenDropMetres = 200.f;
  const unsigned HideTag = 0x45444948;   // "HIDE"
  // Where Faith was while it recorded, every FaithPathSeconds (see ShowAndFollow)
  const double FaithPathSeconds = 0.25;
  // A model that didn't move itself, drawn by the game going along with
  // another recorded one (see ShowAndFollow): the game's draws of its shape
  // within FollowMetres of where it stood, nearer it than any other still
  // one of its shape; it follows a moving one (within FollowMoverMetres) when
  // each was within FollowFitMetres of where that one's moves take it, and
  // FollowMovedNeeded of them were away from where it stood
  const float FollowMetres = 6.f, FollowFitMetres = 0.15f, FollowMoverMetres = 12.f;
  const int FollowMovedNeeded = 2;
  const unsigned FollowTag = 0x574C4F46;   // "FOLW"
  // A Dash's marker (Track::RvLayer, see ShowAndFollow): the game's draws of
  // it within RvSeenMetres of where it stood are its own (the game draws one
  // with its part's offset: dash_arrowmedium's part is 1.14 m from its
  // model's origin); saved after the tracks (RvTag)
  const float RvSeenMetres = 3.f;
  const unsigned RvTag = 0x45564C52;   // "RLVE"
  const double RefindInterval = 5.0;
  const int MaxRefindTargets = 16384;

  // The flags at entity + 0x18. With bit 21 (these models have it) the
  // setter goes through vtable + 0xF0 (0x1431C3210: the rows written, the
  // change queued, 0x1431CA500); later in the frame the queue (0x1431C3290)
  // passes the change on to what's linked to the model (its slot 29,
  // 0x143189D50 -> 0x143168270), then runs its change handler (slot 28,
  // 0x143193310, ending in 0x1431CBCD0), which passes it on to the physics
  // component at + 0x60 (ClientStaticModelPhysicsComponent: the elevator's
  // car and door have one) while bit 27 is set. Moving their collision killed
  // the real Faith when a replayed elevator came up to her; so did the queued
  // way with only bit 27 held (what's linked to the car moved, solid with its
  // own flags; 2026-10-23). With both bits cleared for the call, the setter
  // writes the rows and runs the handler at once (flag bit 3): what's drawn
  // moves (0x1432EB290), nothing linked and no collision does. Faith lived
  // through this way (f974d32f81a3). The game clears bit 27 round that same
  // handler call itself (0x14319361D). Lights linked to a model (an elevator
  // lamp's) are recorded and moved on their own (util::hooks::MoveLight).
  const int EntityFlagsOffset = 0x18;
  const unsigned int DeferredFlag = 1u << 21;
  const unsigned int PhysicsFlag = 1u << 27;

  typedef void(__fastcall* tSetTransform)(__int64, float const*);

  bool IsSetterCode()
  {
    static int s_checked = -1;
    if (s_checked < 0)
    {
      BYTE const* p = (BYTE const*)g_gameHandle + SetTransformRva;
      __int64 target = SetTransformRva + 3 + 5 + *(int const*)(p + 4);
      // mov r8b, 1; jmp rel32 -> 0x1432EB1F0, or the tools' own hook there
      // (for the replay's guns and drones; it checked the same bytes first)
      s_checked = util::hooks::IsModelSetTransformHooked() ||
        (p[0] == 0x41 && p[1] == 0xB0 && p[2] == 0x01 && p[3] == 0xE9 && target == 0x32EB1F0) ? 1 : 0;
      if (!s_checked)
        util::log::Warning("Replay: the static models' setTransform isn't the code the tools know, moving models aren't replayed");
    }
    return s_checked == 1;
  }

  // Only the classes whose vtable has the static models' getter and setter
  bool HasStaticModelSetter(__int64 vtable)
  {
    __int64 base = (__int64)g_gameHandle;
    __try
    {
      return *(__int64 const*)(vtable + 24 * 8) == base + GetTransformRva && *(__int64 const*)(vtable + 25 * 8) == base + SetTransformRva;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
  }

  // The classes the level moves doors, elevators and the MagRope's visuals as
  // (world log, 2026-10-23)
  bool IsStaticModelClass(int classIndex)
  {
    const char* name = attach::ClassName(classIndex);
    return !strcmp(name, "ClientStaticModelEntity") || !strcmp(name, "PamClientStaticModelEntity");
  }

  // What's drawn only (see the flags above)
  bool SetTransform(__int64 entity, __int64 vtable, float const* transform)
  {
    unsigned int* flags = (unsigned int*)(entity + EntityFlagsOffset);
    unsigned int before = 0;
    bool cleared = false;
    __try
    {
      if (*(__int64 const*)entity != vtable)
        return false;
      before = *flags;
      *flags = before & ~(DeferredFlag | PhysicsFlag);
      cleared = true;
      ((tSetTransform)((__int64)g_gameHandle + SetTransformRva))(entity, transform);
      *flags = (*flags & ~(DeferredFlag | PhysicsFlag)) | (before & (DeferredFlag | PhysicsFlag));
      return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      if (cleared)
        *flags = (*flags & ~(DeferredFlag | PhysicsFlag)) | (before & (DeferredFlag | PhysicsFlag));
      return false;
    }
  }

  // Through attach: still the same entity, still linked, still the same
  // static model data
  bool ReadTransform(__int64 entity, __int64 vtable, __int64 data, float* out)
  {
    attach::Target target{};
    target.Entity = entity;
    target.VTable = vtable;
    target.Data = data;
    target.Agent = -1;
    XMFLOAT4X4 transform;
    if (!attach::ReadTransform(target, transform))
      return false;
    memcpy(out, &transform, sizeof(float) * 16);
    return true;
  }

  __int64 ReadVTable(__int64 entity)
  {
    __try { return *(__int64 const*)entity; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
  }

  bool Same(float const* a, float const* b)
  {
    for (int i = 0; i < 16; ++i)
      if (fabsf(a[i] - b[i]) > SameEpsilon)
        return false;
    return true;
  }

  float Distance2(float const* a, float const* b)
  {
    float dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
    return dx * dx + dy * dy + dz * dz;
  }

  // A track's places from its keys (4 floats a point: time, x y z; see
  // replay_shapes::Draw::Path)
  template <class K>
  std::shared_ptr<std::vector<float> const> PathOf(std::vector<K> const& keys)
  {
    auto path = std::make_shared<std::vector<float>>();
    path->reserve(keys.size() * 4);
    for (K const& k : keys)
      path->insert(path->end(), { (float)k.Time, k.Transform[12], k.Transform[13], k.Transform[14] });
    return path;
  }
}

void ReplayModels::Scan(float const* faith, __int64 soldier)
{
  bool fromFaith = false;
  if (!soldier || !attach::FindClasses(soldier, fromFaith))
    return;
  attach::View view{};
  view.Camera = XMFLOAT3(faith[0], faith[1], faith[2]);
  view.Look = XMFLOAT3(0.f, 0.f, 1.f);
  view.Skip = soldier;
  static std::vector<attach::Target> found;
  found.resize(32768);
  std::unique_ptr<attach::Scan> scan = std::make_unique<attach::Scan>();
  int count = attach::ScanNearby(view, ScanRadius, found.data(), (int)found.size(), *scan);
  for (int i = 0; i < count && m_Tracks.size() < MaxCandidates; ++i)
  {
    attach::Target const& target = found[i];
    // Not swarm members, nor what's listed at any distance (trains, aircraft)
    if (target.Agent >= 0 || target.Distance > ScanRadius || m_Index.count(target.Entity) || !HasStaticModelSetter(target.VTable))
      continue;
    // Only static models. Characters have the same getter and setter, and
    // the scan skips only one of Faith's two entities: her other one was
    // recorded once and set along her recorded path, which killed her when
    // the replay stopped (2026-10-23).
    if (target.Entity == soldier || attach::GroupOf(target) == attach::Group::People || !IsStaticModelClass(target.Class))
      continue;
    Track track{};
    track.Entity = target.Entity;
    track.VTable = target.VTable;
    track.Data = target.Data;
    track.RvLayer = attach::PrimaryInRvLayer(track.Data);
    track.RvKnown = true;
    strncpy_s(track.Label, attach::LabelOf(target), _TRUNCATE);
    if (!ReadTransform(track.Entity, track.VTable, track.Data, track.Last))
      continue;
    track.LastTime = -1.0; // seen from the start of the recording on
    // Its shape now, moving or not: a mission's own models that never move
    // (the Release mission's bus area, its bus in a prefab its timeline moves)
    // go with the mission too (2026-10-27 03:48, the user: "a lot of mesh ...
    // that only the mission spawns"), so they're drawn from it when the game
    // doesn't have them (see replay_shapes)
    track.Shape = replay_shapes::Capture(track.Entity, track.Label, true);
    m_Index[track.Entity] = m_Tracks.size();
    m_Tracks.push_back(std::move(track));
  }
}

// The lights the level moved since the last sample (see
// util::hooks::NoteMovingLights), near Faith: from the sample before, they
// were where they were before the change
void ReplayModels::AddLights(double time, float const* faith)
{
  const int MaxTaken = 64;
  __int64 entities[MaxTaken];
  static float before[MaxTaken * 16];
  int count = util::hooks::TakeMovingLights(entities, before, MaxTaken);
  for (int i = 0; i < count && m_Tracks.size() < MaxCandidates; ++i)
  {
    // A real place near her (a light only now being placed was at 0 0 0)
    float const* m = before + 16 * i;
    float dx = m[12] - faith[0], dy = m[13] - faith[1], dz = m[14] - faith[2];
    bool placed = m[12] != 0.f || m[13] != 0.f || m[14] != 0.f;
    if (m_Index.count(entities[i]) || !placed || dx * dx + dy * dy + dz * dz > ScanRadius * ScanRadius)
      continue;
    __int64 vtable = ReadVTable(entities[i]);
    if (!vtable)
      continue;
    Track track{};
    track.Entity = entities[i];
    track.VTable = vtable;
    track.Light = true;
    sprintf_s(track.Label, "a light (%s)", util::hooks::LightClassName(vtable));
    memcpy(track.Last, m, sizeof(track.Last));
    track.LastTime = m_LastSampleTime;
    m_Index[track.Entity] = m_Tracks.size();
    m_Tracks.push_back(std::move(track));
  }
  (void)time;
}

bool ReplayModels::Read(Track const& track, float* out) const
{
  return track.Light ? util::hooks::ReadLight(track.Entity, track.VTable, out) : ReadTransform(track.Entity, track.VTable, track.Data, out);
}

bool ReplayModels::Write(Track const& track, float const* transform) const
{
  return track.Light ? util::hooks::MoveLight(track.Entity, track.VTable, transform) : SetTransform(track.Entity, track.VTable, transform);
}

void ReplayModels::StartRecording(float const* faith, __int64 soldier)
{
  Clear();
  if (!IsSetterCode())
    return;
  Scan(faith, soldier);
  m_LastScan = 0.0;
  m_LastSampleTime = 0.0;
  util::hooks::NoteMovingLights(true);
}

void ReplayModels::RecordFrame(double time, float const* faith, __int64 soldier)
{
  if (!IsSetterCode())
    return;
  // Where Faith is (see ShowAndFollow)
  if (faith && (m_FaithTimes.empty() || time - m_FaithTimes.back() >= FaithPathSeconds))
  {
    m_FaithTimes.push_back(time);
    m_FaithPlaces.insert(m_FaithPlaces.end(), faith, faith + 3);
  }
  if (time - m_LastScan >= ScanInterval)
  {
    size_t before = m_Tracks.size();
    Scan(faith, soldier);
    // Ones found now were where they are since the recording started
    for (size_t i = before; i < m_Tracks.size(); ++i)
      m_Tracks[i].LastTime = time;
    m_LastScan = time;
  }
  AddLights(time, faith);
  m_LastSampleTime = time;
  size_t movers = 0;
  for (Track& track : m_Tracks)
  {
    if (track.Gone)
      continue;
    float now[16];
    if (!Read(track, now))
    {
      track.Gone = true;
      continue;
    }
    // A single static model's Visible as the game's reader gives it: at the
    // start, then each change (see Play)
    int visible = -1;
    bool connected = false;
    if (!track.Light && util::hooks::ReadStaticModelVisible(track.Entity, visible, connected))
    {
      if (track.VisibleStart < 0)
        track.VisibleStart = visible;
      else if (visible != VisibleAt(track, time) && track.VisibleChanges.size() < 256)
        track.VisibleChanges.push_back({ time, visible });
    }
    if (!Same(now, track.Last))
    {
      if (movers < MaxMovers || !track.Keys.empty())
      {
        // Where it was until now, then the change
        if (track.Keys.empty() || track.Keys.back().Time < track.LastTime)
        {
          // Diagnostic (temporary): its Visible as it first moves (see
          // util::hooks::ReadStaticModelVisible)
          bool connected = false;
          if (util::DiagnosticsOn() && track.Keys.empty() &&
              !util::hooks::ReadStaticModelVisible(track.Entity, track.VisibleFirst, connected))
            track.VisibleFirst = -1;
          // Its mesh, kept as it first moves if it couldn't be when it was
          // found (a less detailed level loaded then), so it can be drawn by
          // the replay itself if the game doesn't have it when it plays (a
          // mission's, see replay_shapes)
          if (track.Keys.empty() && !track.Light && track.Shape < 0)
            track.Shape = replay_shapes::Capture(track.Entity, track.Label, false);
          Key before;
          before.Time = track.LastTime < 0 ? 0.0 : track.LastTime;
          memcpy(before.Transform, track.Last, sizeof(before.Transform));
          track.Keys.push_back(before);
        }
        Key key;
        key.Time = time;
        memcpy(key.Transform, now, sizeof(key.Transform));
        track.Keys.push_back(key);
      }
      memcpy(track.Last, now, sizeof(now));
    }
    track.LastTime = time;
    movers += !track.Keys.empty();
  }
}

void ReplayModels::StopRecording()
{
  util::hooks::NoteMovingLights(false);
  // The shapes' colour textures aren't looked for in the game's draws any more
  replay_shapes::EndCapture();
  size_t candidates = m_Tracks.size();
  // The ones that moved, and the ones that didn't with their shape kept (one
  // place: where they stood; drawn only if the game doesn't have them when it
  // plays, never moved)
  int still = 0;
  for (Track& track : m_Tracks)
    if (track.Keys.empty() && !track.Light && track.Shape >= 0)
    {
      Key key;
      key.Time = 0.0;
      memcpy(key.Transform, track.Last, sizeof(key.Transform));
      track.Keys.push_back(key);
      track.Still = true;
      ++still;
    }
  m_Tracks.erase(std::remove_if(m_Tracks.begin(), m_Tracks.end(), [](Track const& track) { return track.Keys.empty(); }),
    m_Tracks.end());
  m_Index.clear();
  ShowAndFollow();
  util::log::Write("Replay: %d of the %d models within %.0f m of Faith moved and are in the recording; %d more that didn't move "
    "are in it with their shapes (drawn if the game doesn't have them when it plays); %d shapes kept, %.1f MB, %d models "
    "whose shape couldn't be kept",
    (int)m_Tracks.size() - still, (int)candidates, ScanRadius, still, replay_shapes::Count(), replay_shapes::Bytes() / 1048576.0,
    replay_shapes::Refused());
  // The ones that didn't move, by label (2026-10-28: whether the Release
  // gate's glass, csg_releasegateglass_01, was among them wasn't known)
  std::map<std::string, int> stillLabels;
  for (Track const& track : m_Tracks)
    if (track.Still)
      ++stillLabels[track.Label];
  std::string line;
  for (auto const& l : stillLabels)
  {
    char item[160];
    if (l.second > 1)
      sprintf_s(item, "%s x%d", l.first.c_str(), l.second);
    else
      strncpy_s(item, l.first.c_str(), _TRUNCATE);
    if (!line.empty() && line.size() + strlen(item) > 900)
    {
      util::log::Write("Replay:   didn't move: %s", line.c_str());
      line.clear();
    }
    if (!line.empty())
      line += ", ";
    line += item;
  }
  if (!line.empty())
    util::log::Write("Replay:   didn't move: %s", line.c_str());
  int shown = 0;
  for (Track const& track : m_Tracks)
  {
    if (track.Still)
      continue;
    if (shown++ >= 30)
      break;
    // Diagnostic (temporary): its Visible as it first moved and now, for a
    // single static model (a mission item the game hides through it, not
    // through a model group: see util::hooks::ReadStaticModelVisible)
    char visibility[160] = "";
    int visible = -1;
    bool connected = false;
    if (util::DiagnosticsOn() && util::hooks::ReadStaticModelVisible(track.Entity, visible, connected))
    {
      int at = sprintf_s(visibility, ", Visible %d at the start, %d as it first moved, %d now (%s), %d changes", track.VisibleStart,
        track.VisibleFirst, visible, connected ? "from a connection" : "its data's", (int)track.VisibleChanges.size());
      for (size_t c = 0; c < track.VisibleChanges.size() && c < 4 && at > 0 && at < (int)sizeof(visibility) - 24; ++c)
        at += sprintf_s(visibility + at, sizeof(visibility) - at, "%s%d at %.2f s", c ? ", " : ": ", track.VisibleChanges[c].second,
          track.VisibleChanges[c].first);
    }
    util::log::Write("Replay:   '%s' 0x%I64X: %d places, %.2f - %.2f s%s", track.Label, track.Entity, (int)track.Keys.size(),
      track.Keys.front().Time, track.Keys.back().Time, visibility);
  }
}

// How far Faith was from a place at a time of the recording (her nearest
// sample's place); -1 when she wasn't sampled
float ReplayModels::FaithDistance(double time, float const* place) const
{
  if (m_FaithTimes.empty() || m_FaithPlaces.size() != m_FaithTimes.size() * 3)
    return -1.f;
  size_t k = (size_t)(std::lower_bound(m_FaithTimes.begin(), m_FaithTimes.end(), time) - m_FaithTimes.begin());
  if (k == m_FaithTimes.size() || (k > 0 && time - m_FaithTimes[k - 1] < m_FaithTimes[k] - time))
    k = k > 0 ? k - 1 : 0;
  return sqrtf(Distance2(&m_FaithPlaces[k * 3], place));
}

// The models that didn't move, by where the game drew them while it recorded
// (replay_gamedraws::SightingsOf; every model part's draw read back, every
// half second or so while it was drawn). 2026-11-02, after the 19:40 test of
// 448A17A12E0A (the user: "the food-dome texture is still there in the replay
// when it shouldn't", "the arrow texture from the doors doesn't move with
// them"): the Release gate's sign planes (plane_1-1_01) are drawn by the game
// only while the gate's events have them on, and on the gate's doors as they
// open, but their own transforms never changed, so the replay drew them where
// they stood for the whole playback. 448A17A12E0A's rule left out every model
// the game drew in its G-buffer pass, and the asset log had plane_1-1_01 there.
// - Following: one the game drew away from where it stood (FollowMovedNeeded
//   of its draws), each draw within FollowFitMetres of where a recorded
//   moving model's moves would take it (that one as it was before it moved,
//   then each of its places), follows that one: its keys become the other's
//   moves applied to it. Its own copy in the game isn't moved.
// - Shown only part of the time: its own draws (where it was then, within
//   OwnSeenMetres) against the draws of its neighbours (within
//   NearSeenMetres) while it wasn't drawn and Faith was no further from it
//   than when it was (see the constants). HiddenClocksNeeded such times
//   before its first draw: not drawn before it; after its last: not drawn
//   after it. Only for one the game drew steadily in between (at most a
//   quarter as many such times as its own draws' half seconds), at least 3
//   times or over a second (an air conditioner drawn once, then left out when
//   far, isn't one). Never drawn while its neighbours were, near Faith: never.
void ReplayModels::ShowAndFollow()
{
  std::map<int, std::vector<size_t>> byShape;
  std::vector<size_t> movers;
  for (size_t i = 0; i < m_Tracks.size(); ++i)
  {
    Track const& t = m_Tracks[i];
    if (t.Light)
      continue;
    if (t.Still && t.Shape >= 0 && t.Keys.size() == 1)
      byShape[t.Shape].push_back(i);
    else if (!t.Still && t.Keys.size() >= 2)
      movers.push_back(i);
  }
  int switched = 0, never = 0, followed = 0, away = 0, logs = 0;
  std::string switchedList, followedList, awayList;
  std::vector<replay_gamedraws::ShapeSighting> sightings;
  struct Mine
  {
    float Clock;
    bool Main;
    float Place[3];
  };
  for (auto const& shape : byShape)
  {
    replay_gamedraws::SightingsOf(shape.first, sightings);
    for (size_t index : shape.second)
    {
      Track& track = m_Tracks[index];
      float home[3] = { track.Last[12], track.Last[13], track.Last[14] };
      // Its draws: each sighting's instance nearest it within FollowMetres,
      // when no other still one of its shape stood nearer that instance
      std::vector<Mine> mine;
      for (auto const& s : sightings)
      {
        float best = FollowMetres * FollowMetres;
        int at = -1;
        for (size_t j = 0; j + 3 <= s.Places.size(); j += 3)
        {
          float d2 = Distance2(&s.Places[j], home);
          if (d2 < best)
          {
            best = d2;
            at = (int)j;
          }
        }
        if (at < 0)
          continue;
        bool other = false;
        for (size_t o : shape.second)
          if (o != index && Distance2(&s.Places[at], m_Tracks[o].Last + 12) < best)
          {
            other = true;
            break;
          }
        if (!other)
          mine.push_back(Mine{ s.Clock, s.Main, { s.Places[at], s.Places[at + 1], s.Places[at + 2] } });
      }
      std::sort(mine.begin(), mine.end(), [](Mine const& a, Mine const& b) { return a.Clock < b.Clock; });
      const float own2 = track.RvLayer ? RvSeenMetres * RvSeenMetres : OwnSeenMetres * OwnSeenMetres;
      int moved = 0;
      float farthest = 0.f;
      for (Mine const& m : mine)
        if (Distance2(m.Place, home) > own2)
        {
          ++moved;
          farthest = (std::max)(farthest, sqrtf(Distance2(m.Place, home)));
        }
      // Drawn away from where it stood: the moving one it went along with (not
      // a Dash's marker: it stands where the Dash put it)
      if (moved >= FollowMovedNeeded && !track.RvLayer)
      {
        XMMATRIX self = XMLoadFloat4x4((XMFLOAT4X4 const*)track.Last);
        int bestMover = -1;
        float bestWorst = FollowFitMetres;
        XMMATRIX bestRelative = XMMatrixIdentity();
        for (size_t mi : movers)
        {
          Track const& d = m_Tracks[mi];
          if (Distance2(d.Keys.front().Transform + 12, home) > FollowMoverMetres * FollowMoverMetres)
            continue;
          XMVECTOR det;
          XMMATRIX before = XMMatrixInverse(&det, XMLoadFloat4x4((XMFLOAT4X4 const*)d.Keys.front().Transform));
          if (fabsf(XMVectorGetX(det)) < 1e-6f)
            continue;
          XMMATRIX relative = XMMatrixMultiply(self, before);
          float worst = 0.f;
          int movedWith = 0;
          for (Mine const& m : mine)
          {
            alignas(16) float then[16];
            Evaluate(d, m.Clock, then);
            XMFLOAT4X4 at;
            XMStoreFloat4x4(&at, XMMatrixMultiply(relative, XMLoadFloat4x4((XMFLOAT4X4 const*)then)));
            float there[3] = { at._41, at._42, at._43 };
            worst = (std::max)(worst, sqrtf(Distance2(there, m.Place)));
            if (worst >= bestWorst)
              break;
            movedWith += Distance2(m.Place, home) > own2;
          }
          if (worst < bestWorst && movedWith >= FollowMovedNeeded)
          {
            bestWorst = worst;
            bestMover = (int)mi;
            bestRelative = relative;
          }
        }
        char item[160];
        if (bestMover >= 0)
        {
          Track const& d = m_Tracks[bestMover];
          std::vector<Key> keys;
          keys.reserve(d.Keys.size());
          for (Key const& k : d.Keys)
          {
            Key key;
            key.Time = k.Time;
            XMStoreFloat4x4((XMFLOAT4X4*)key.Transform, XMMatrixMultiply(bestRelative,
              XMLoadFloat4x4((XMFLOAT4X4 const*)k.Transform)));
            keys.push_back(key);
          }
          track.Keys.swap(keys);
          track.Follows = true;
          track.Path = PathOf(track.Keys);
          ++followed;
          sprintf_s(item, "%s with '%s' 0x%I64X (%d of its %d draws away from where it stood, up to %.2f m; within %.1f cm)",
            track.Label, d.Label, d.Entity, moved, (int)mine.size(), farthest, bestWorst * 100.f);
          if (followedList.size() < 900)
            followedList += (followedList.empty() ? "" : ", ") + std::string(item);
        }
        else
        {
          ++away;
          sprintf_s(item, "%s at %.1f %.1f %.1f (%d of %d, up to %.2f m)", track.Label, home[0], home[1], home[2], moved,
            (int)mine.size(), farthest);
          if (awayList.size() < 700)
            awayList += (awayList.empty() ? "" : ", ") + std::string(item);
        }
      }
      // Its own draws: where it was then (on its path when it follows one)
      std::vector<float> own;
      float ownFar = 0.f;
      bool ownMain = false;
      for (Mine const& m : mine)
      {
        alignas(16) float then[16];
        Evaluate(track, m.Clock, then);
        if (Distance2(m.Place, then + 12) > own2)
          continue;
        own.push_back(m.Clock);
        ownMain = ownMain || m.Main;
        ownFar = (std::max)(ownFar, FaithDistance(m.Clock, then + 12));
      }
      // Its neighbours' draws while it wasn't drawn, Faith no further than then
      replay_gamedraws::Seen seen;
      replay_gamedraws::SightingsAt(track.Shape, home, OwnSeenMetres, NearSeenMetres, seen);
      std::sort(seen.Near.begin(), seen.Near.end());
      float faithLimit = own.empty() ? FaithNeverMetres : ownFar + FaithSeenSlackMetres;
      float first = own.empty() ? 0.f : own.front(), last = own.empty() ? 0.f : own.back();
      int before = 0, after = 0, inside = 0, hidden = 0;
      float lastCounted = -1e9f;
      for (float t : seen.Near)
      {
        if (t - lastCounted < 0.5f)
          continue;
        auto o = std::lower_bound(own.begin(), own.end(), t - OwnSeenSlack);
        if (o != own.end() && *o <= t + OwnSeenSlack)
          continue;
        float faith = FaithDistance(t, home);
        if (faith < 0.f || faith > faithLimit)
          continue;
        lastCounted = t;
        ++hidden;
        if (!own.empty())
        {
          if (t < first)
            ++before;
          else if (t > last)
            ++after;
          else
            ++inside;
        }
      }
      // Where the camera looked too (2026-11-03, the user: "Door arrow
      // textures don't move with the doors either": the Release prison door's
      // arrow signs are switched off by its open event, and its doors, the
      // only neighbours within NearSeenMetres, were further off once open; see
      // replay_gamedraws::InViewTimes): the times it was in the picture, the
      // camera no further from it than at any of its own draws (+
      // ViewSlackMetres), and not drawn, counted like the neighbours'. Only
      // for one drawn at most of such times between its first and last draw
      // (ViewSeenNeeded at least): the draws are read back every UpgradeEvery
      // frames, up to a number a pass, so one left out by that isn't taken for
      // hidden.
      int viewBefore = 0, viewAfter = 0, viewInside = 0, viewSeen = 0;
      bool viewCounted = false;
      if (!own.empty() && !ownMain)
      {
        float cameraFar = -1.f;
        for (float t : own)
          cameraFar = (std::max)(cameraFar, replay_gamedraws::CameraDistanceAt(t, home));
        std::vector<float> inView;
        if (cameraFar > 0.f)
          replay_gamedraws::InViewTimes(home, cameraFar + ViewSlackMetres, inView);
        float lastView = -1e9f;
        for (float t : inView)
        {
          auto o = std::lower_bound(own.begin(), own.end(), t - OwnSeenSlack);
          bool drawn = o != own.end() && *o <= t + OwnSeenSlack;
          if (t >= first && t <= last)
          {
            ++(drawn ? viewSeen : viewInside);
            continue;
          }
          if (drawn || t - lastView < 0.5f)
            continue;
          lastView = t;
          ++(t < first ? viewBefore : viewAfter);
        }
        if (viewSeen >= ViewSeenNeeded && viewInside <= viewSeen)
        {
          before += viewBefore;
          after += viewAfter;
          viewCounted = true;
        }
      }
      int ownHalves = 0;
      float lastOwn = -1e9f;
      for (float t : own)
        if (t - lastOwn >= 0.5f)
        {
          ++ownHalves;
          lastOwn = t;
        }
      char item[128] = "";
      // A Dash's marker (2026-11-08, the user: "Replay arrow/starting are
      // wrongly textured / missing model(?) finish line is missing"): the game
      // draws one only into its runner's vision mask, and only while the Dash
      // has it on (the Dash's prefabs switch their markers' Enabled: the
      // next checkpoint's arrow, then the finish), so its neighbours and the
      // camera's view don't tell when it was on. Shown from its first draw
      // of the game's to its last (ShownSlack each side), the game's draws in
      // that layer kept (ReplayGameDraws' Pass_Rv); never when the game didn't
      // draw it while it recorded. Until now its draws in that layer weren't
      // looked at: the finish counted as never shown (not drawn), the start
      // and the arrows as always shown, drawn as solid shapes with another
      // texture all through the playback.
      if (track.RvLayer)
      {
        track.HideLive = 0;
        if (own.empty())
        {
          track.NeverShown = true;
          ++never;
          sprintf_s(item, "%s (never: a Dash's marker the game didn't draw)", track.Label);
        }
        else
        {
          track.ShowFrom = first - ShownSlack;
          track.ShowTo = last + ShownSlack;
          ++switched;
          sprintf_s(item, "%s (%.1f - %.1f s: a Dash's marker)", track.Label, first, last);
        }
      }
      // Only one the game never drew in its G-buffer pass (2026-11-02, the
      // 21:36 test: with every model counted, air conditioners and lights
      // the game left out far from Faith were taken for shown part of the
      // time; the Release gate's signs are drawn in the lit picture's pass
      // only: the G-buffer pass's plane_1-1_01 draws were other signs')
      else if (own.empty())
      {
        if (hidden >= HiddenClocksNeeded)
        {
          track.NeverShown = true;
          ++never;
          sprintf_s(item, "%s (never; its neighbours drawn %d times)", track.Label, hidden);
        }
      }
      else if (!ownMain && (before >= HiddenClocksNeeded || after >= HiddenClocksNeeded) && (own.size() >= 3 || last - first >= 1.f) &&
               inside <= (std::max)(1, ownHalves / 4))
      {
        track.ShowFrom = before >= HiddenClocksNeeded ? first - ShownSlack : 0.0;
        track.ShowTo = after >= HiddenClocksNeeded ? last + ShownSlack : 1e30;
        // The game's own too where the camera saw it not drawn (2026-11-03,
        // the user: "this door texture should move with the door": the prison
        // door's arrow signs, drawn by the game 9.9 - 31.1 s, its doors moving
        // from 31.17 s; in the playback the game drew its own, which nothing
        // had switched off, and the replay let its copy go for it)
        track.HideLive = viewCounted ? (unsigned char)((before >= HiddenClocksNeeded && viewBefore >= HiddenClocksNeeded ? 1 : 0) |
          (after >= HiddenClocksNeeded && viewAfter >= HiddenClocksNeeded ? 2 : 0)) : 0;
        ++switched;
        sprintf_s(item, "%s (%s%.1f - %.1f s%s%s)", track.Label, before >= HiddenClocksNeeded ? "" : "start ... ", first, last,
          after >= HiddenClocksNeeded ? "" : " ... end", track.HideLive == 3 ? "; the game's own hidden before and after" :
          track.HideLive == 1 ? "; the game's own hidden before" : track.HideLive == 2 ? "; the game's own hidden after" : "");
      }
      if (item[0] && switchedList.size() < 900)
        switchedList += (switchedList.empty() ? "" : ", ") + std::string(item);
      // What was seen of it (once a model; one that moved, is shown part of
      // the time, or had a neighbour drawn without it while it was drawn too)
      if (util::DiagnosticsOn() && (moved || item[0] || (inside && !own.empty()) || viewBefore || viewAfter) && logs++ < 60)
        util::log::Write("Replay (diagnostic):   '%s' 0x%I64X at %.1f %.1f %.1f: %d draws of it (%d where it was, %.1f - %.1f s, "
          "%d away), its neighbours drawn without it %d times (%d between; Faith within %.0f m), in the camera's view and not drawn "
          "%d times before and %d after (drawn at %d of its %d times in view between%s); %d before, %d after in all", track.Label,
          track.Entity, home[0], home[1], home[2], (int)mine.size(), (int)own.size(), first, last, moved, hidden, inside, faithLimit,
          viewBefore, viewAfter, viewSeen, viewSeen + viewInside, viewSeen >= ViewSeenNeeded && viewInside <= viewSeen ? "" :
          ": not counted", before, after);
    }
  }
  if (followed)
    util::log::Write("Replay: %d models that didn't move themselves follow a recorded one that moved, as the game drew them: %s",
      followed, followedList.c_str());
  if (away)
    util::log::Write("Replay: %d models that didn't move were drawn by the game elsewhere too, along no recorded model (drawn where "
      "they stood): %s", away, awayList.c_str());
  if (switched || never)
    util::log::Write("Replay: %d models the game showed only part of the time (drawn only then) and %d it never showed (never "
      "drawn), by its draws of them: %s", switched, never, switchedList.c_str());
}

void ReplayModels::StartPlaying()
{
  m_Playing = true;
  m_SetFailures = 0;
  m_LastRefindScan = -1e9;
  int live = 0;
  for (Track& track : m_Tracks)
  {
    track.HasLive = Read(track, track.Live);
    track.NextRefind = 0.0;
    live += track.HasLive;
  }
  int drawn = 0;
  for (Track const& track : m_Tracks)
    drawn += !track.HasLive && !track.Light && track.Shape >= 0;
  if (!m_Tracks.empty())
    util::log::Write("Replay: playing %d moving models (%d of them there now; %d of the others drawn by the replay from the "
      "shapes it kept%s)", (int)m_Tracks.size(), live, drawn, m_ShapesOn ? "" : ", not now: the option is off");
  // Diagnostic (temporary): the single static models the game hides now
  if (!util::DiagnosticsOn())
    return;
  int hidden = 0;
  for (Track const& track : m_Tracks)
  {
    int visible = -1;
    bool connected = false;
    if (!track.HasLive || !util::hooks::ReadStaticModelVisible(track.Entity, visible, connected) || visible)
      continue;
    if (hidden++ < 30)
      util::log::Write("Replay (diagnostic):   '%s' 0x%I64X is hidden by the game now (Visible 0, %s; %d as it first moved): "
        "not shown while it plays", track.Label, track.Entity, connected ? "from a connection" : "its data's", track.VisibleFirst);
  }
}

// The recorded place at a time: scale and position in a straight line between
// two places, the turn along the shorter way round
void ReplayModels::Evaluate(Track const& track, double time, float* out) const
{
  std::vector<Key> const& keys = track.Keys;
  if (time <= keys.front().Time)
  {
    memcpy(out, keys.front().Transform, sizeof(float) * 16);
    return;
  }
  if (time >= keys.back().Time)
  {
    memcpy(out, keys.back().Transform, sizeof(float) * 16);
    return;
  }
  size_t lo = 0, hi = keys.size() - 1;
  while (hi - lo > 1)
  {
    size_t mid = (lo + hi) / 2;
    if (keys[mid].Time <= time) lo = mid; else hi = mid;
  }
  double span = keys[hi].Time - keys[lo].Time;
  float f = span > 0 ? (float)((time - keys[lo].Time) / span) : 0.f;
  XMMATRIX a = XMLoadFloat4x4((XMFLOAT4X4 const*)keys[lo].Transform);
  XMMATRIX b = XMLoadFloat4x4((XMFLOAT4X4 const*)keys[hi].Transform);
  XMVECTOR sa, ra, ta, sb, rb, tb;
  if (XMMatrixDecompose(&sa, &ra, &ta, a) && XMMatrixDecompose(&sb, &rb, &tb, b))
  {
    XMMATRIX m = XMMatrixAffineTransformation(XMVectorLerp(sa, sb, f), XMVectorZero(), XMQuaternionSlerp(ra, rb, f),
      XMVectorLerp(ta, tb, f));
    XMStoreFloat4x4((XMFLOAT4X4*)out, m);
  }
  else
  {
    for (int i = 0; i < 16; ++i)
      out[i] = keys[lo].Transform[i] + (keys[hi].Transform[i] - keys[lo].Transform[i]) * f;
  }
}

// The recorded models that aren't there any more, found again round where
// the replayed Faith is: the game builds a model again when the part of the
// level it's in streams back in (2026-10-25: 52 of 77, the doors among them,
// were other entities by the playback, so they didn't move), with the same
// mesh at its first place (where it was before it moved, as built). How many
// were found.
int ReplayModels::Refind(float const* around)
{
  if (!around)
    return 0;
  double now = GetTickCount64() / 1000.0;
  if (now - m_LastRefindScan < RefindScanInterval)
    return 0;
  std::vector<size_t> missing;
  for (size_t i = 0; i < m_Tracks.size(); ++i)
  {
    Track const& track = m_Tracks[i];
    if (track.Light || track.HasLive || track.Keys.empty() || now < track.NextRefind)
      continue;
    float const* first = track.Keys.front().Transform + 12;
    float dx = first[0] - around[0], dy = first[1] - around[1], dz = first[2] - around[2];
    if (dx * dx + dy * dy + dz * dz <= RefindRadius * RefindRadius)
      missing.push_back(i);
  }
  if (missing.empty())
    return 0;
  m_LastRefindScan = now;

  attach::View view{};
  view.Camera = XMFLOAT3(around[0], around[1], around[2]);
  view.Look = XMFLOAT3(0.f, 0.f, 1.f);
  static std::vector<attach::Target> found;
  found.resize(MaxRefindTargets);
  std::unique_ptr<attach::Scan> scan = std::make_unique<attach::Scan>();
  int count = attach::ScanNearby(view, RefindRadius + 1.f, found.data(), (int)found.size(), *scan);
  // Not one another recorded model is already
  std::unordered_map<__int64, bool> taken;
  for (Track const& track : m_Tracks)
    if (track.HasLive)
      taken[track.Entity] = true;
  int refound = 0;
  for (size_t i : missing)
  {
    Track& track = m_Tracks[i];
    float const* first = track.Keys.front().Transform + 12;
    int best = -1;
    float bestDistance = RefindDistance * RefindDistance;
    for (int k = 0; k < count; ++k)
    {
      attach::Target const& target = found[k];
      if (target.Agent >= 0 || taken.count(target.Entity) || !HasStaticModelSetter(target.VTable) ||
          !IsStaticModelClass(target.Class) || strcmp(attach::LabelOf(target), track.Label))
        continue;
      float dx = target.Position.x - first[0], dy = target.Position.y - first[1], dz = target.Position.z - first[2];
      float d = dx * dx + dy * dy + dz * dz;
      if (d <= bestDistance)
      {
        best = k;
        bestDistance = d;
      }
    }
    if (best >= 0)
    {
      __int64 entity = track.Entity;
      track.Entity = found[best].Entity;
      track.VTable = found[best].VTable;
      track.Data = found[best].Data;
      track.HasLive = Read(track, track.Live);
      if (track.HasLive)
      {
        // (A recording saved before 2026-11-08 has no flag: the live model's)
        if (!track.RvKnown)
        {
          track.RvLayer = attach::PrimaryInRvLayer(track.Data);
          track.RvKnown = true;
        }
        taken[track.Entity] = true;
        ++refound;
        continue;
      }
      track.Entity = entity;
    }
    track.NextRefind = now + RefindInterval;
  }
  if (refound)
    util::log::Write("Replay: %d of the recorded models (%d looked for) found again near the replayed Faith: built again by the "
      "game since", refound, (int)missing.size());
  return refound;
}

void ReplayModels::Play(double time, float const* around)
{
  if (!m_Playing)
    return;
  Refind(around);
  for (Track& track : m_Tracks)
  {
    // Only while it's still the same model, and when it isn't there already
    // (the game may move it too). Gone since (its part of the level streamed
    // out): looked for again (see Refind).
    float now[16];
    if (!track.HasLive)
      continue;
    if (!Read(track, now))
    {
      track.HasLive = false;
      track.NextRefind = 0.0;
      continue;
    }
    // Hidden by the game then, where the camera saw it not drawn (see
    // ShowAndFollow): the game's own shrunk to nothing and moved far down
    // (a static model's transform is all the replay sets), given back once
    // shown again (and when it stops, StopPlaying)
    bool hide = HidesLive(track, time), wasHidden = track.LiveHidden;
    if (hide != wasHidden)
    {
      track.LiveHidden = hide;
      track.LiveDrawn = -1;   // asked again (the game doesn't draw it there while it's away)
      static std::set<std::string> logged;
      std::string key = std::string(track.Label) + (hide ? "+" : "-");
      if (logged.size() < 60 && logged.insert(key).second)
        util::log::Write("Replay: '%s' is hidden by the game then, but the game has it: its own %s (at %.2f s played; the first such change "
          "of a model with this label)", track.Label, hide ? "taken out of sight" : "given back", time);
    }
    if (hide)
    {
      alignas(16) float wanted[16];
      if (track.Still)
        memcpy(wanted, track.Live, sizeof(wanted));
      else
        Evaluate(track, time, wanted);
      for (int row = 0; row < 3; ++row)
        for (int column = 0; column < 3; ++column)
          wanted[row * 4 + column] *= HiddenScale;
      wanted[13] -= HiddenDropMetres;
      if (!Same(now, wanted) && !Write(track, wanted) && m_SetFailures++ == 0)
        util::log::Warning("Replay: a model ('%s') couldn't be moved", track.Label);
      continue;
    }
    // One that didn't move: left to the game while it has it (where it was,
    // once shown again)
    if (track.Still)
    {
      alignas(16) float live[16];
      memcpy(live, track.Live, sizeof(live));
      if (wasHidden && !Same(now, live) && !Write(track, live) && m_SetFailures++ == 0)
        util::log::Warning("Replay: a model ('%s') couldn't be moved", track.Label);
      continue;
    }
    // Shown or hidden as it was then (a collected item)
    GiveVisible(track, time);
    alignas(16) float wanted[16];
    Evaluate(track, time, wanted);
    if (Same(now, wanted))
      continue;
    if (!Write(track, wanted) && m_SetFailures++ == 0)
      util::log::Warning("Replay: a model ('%s') couldn't be moved", track.Label);
  }
  // One the game has but doesn't draw where it was (hidden by its mission,
  // taken elsewhere by its own animation): its recorded copy drawn too
  // (2026-10-28, the user: "draw the recorded doors and glass when the game's
  // live ones aren't actually drawn"). The game's drawing of its mesh is read
  // back now and then (replay_gamedraws::GameDrawsModel); a change counts once
  // it has held LiveDrawnHoldSeconds.
  double wall = GetTickCount64() / 1000.0;
  if (m_ShapesOn)
    for (Track& track : m_Tracks)
    {
      // A Dash's marker: whether the game draws it there is followed before
      // its time comes too, so its copy is drawn as soon as it's shown (not
      // LiveDrawnHoldSeconds later)
      bool shown = ShownAt(track, time);
      if (!track.HasLive || track.Light || track.Shape < 0 || track.Keys.empty() || (!shown && !track.RvLayer))
      {
        track.CopyLive = false;
        continue;
      }
      if (track.Mesh.empty())
        track.Mesh = replay_shapes::NameOf(track.Shape);
      alignas(16) float place[16];
      Evaluate(track, time, place);
      int drawn = replay_gamedraws::GameDrawsModel(track.Mesh, place + 12, track.RvLayer ? RvSeenMetres : -1.f);
      if (drawn >= 0 && drawn != track.LiveDrawn)
      {
        track.LiveDrawn = drawn;
        track.LiveDrawnSince = wall;
      }
      bool copy = track.LiveDrawn == 0 && wall - track.LiveDrawnSince >= LiveDrawnHoldSeconds;
      bool keep = track.LiveDrawn == 1 && wall - track.LiveDrawnSince >= LiveDrawnHoldSeconds;
      if (!shown)
      {
        track.CopyLive = false;
        continue;
      }
      if ((copy && !track.CopyLive) || (keep && track.CopyLive))
      {
        track.CopyLive = copy;
        // Once a model's label and change (2026-11-01: the first 40 lines went
        // to air conditioners in one second, so the gate's doors and glass
        // weren't in the log)
        static std::set<std::string> logged;
        std::string key = std::string(track.Label) + (copy ? "+" : "-");
        if (logged.size() < 300 && logged.insert(key).second)
          util::log::Write("Replay: '%s' is in the level, but the game %s where it was: its recorded copy %s (at %.2f s played; the "
            "first such change of a model with this label)", track.Label, copy ? "doesn't draw it" : "draws it again",
            copy ? "drawn" : "let go", time);
        ++m_CopyLiveLogs;
      }
    }
  // The ones the game doesn't have (a mission's, taken out with it), and the
  // ones above: drawn by the replay from the shapes it kept, where they were,
  // unless hidden then
  std::vector<replay_shapes::Draw> draws;
  if (m_ShapesOn)
    for (Track const& track : m_Tracks)
    {
      if ((track.HasLive && !track.CopyLive) || track.Light || track.Shape < 0 || track.Keys.empty() || VisibleAt(track, time) == 0 ||
          !ShownAt(track, time))
        continue;
      alignas(16) float place[16];
      Evaluate(track, time, place);
      replay_shapes::Draw draw;
      draw.Shape = track.Shape;
      memcpy(draw.Transform, place, sizeof(draw.Transform));
      draw.Entity = track.Entity;
      draw.Still = track.Still;
      draw.LiveCopy = track.HasLive && track.CopyLive;
      draw.RvLayer = track.RvLayer;
      draw.Path = track.Path;
      draws.push_back(draw);
    }
  replay_shapes::SetDraws(draws.data(), (int)draws.size());
}

// Collected items (2026-10-25 evening). The electric box's key, the kiosk's
// chip and the checkpoint beacons are single static models (not physics
// objects: ObjectBlueprints of a StaticModelEntityData with keyframed bodies)
// the game hides once taken, so the replay moved them where they were but they
// stayed hidden. Their Visible goes through a reader at entity + 0x148 (see
// util::hooks::ReadStaticModelVisible): while it records, the value the
// reader gives is kept at the start and at each change; while it plays, a
// model whose reader gives another value than it had then gets the replay's
// own cell with the recorded value (util::hooks::SwapStaticModelVisibleCell,
// only while it still has the game's cell seen just before), and the game's
// cell back when it stops or its value is the game's again. Not verified in
// game: that what draws the model reads that reader (no code reading + 0x148
// was found in the image, so the model may be shown or hidden some other way,
// then this changes nothing). The cells are the replay's for as long as the
// tools are loaded, so a model made again while it pointed at one never reads
// freed memory.
namespace
{
  struct VisibleCell
  {
    __int64 Value;          // + 0x00: a pointer to the value (see 0x142A424B0)
    BYTE Pad[0x14];
    BYTE Flags;             // + 0x1C: 0x40, a connected value
    BYTE Pad2[3];
    BYTE Visible;           // the value itself
    BYTE Pad3[0x1F];
  };
  static_assert(offsetof(VisibleCell, Flags) == 0x1C, "the cell's flags are at + 0x1C");
  const int MaxVisibleCells = 512;
  VisibleCell g_VisibleCells[MaxVisibleCells]{};
  bool g_VisibleCellUsed[MaxVisibleCells]{};
}

// Whether a model the game shows and hides was shown then (see StopRecording)
bool ReplayModels::ShownAt(Track const& track, double time)
{
  if (track.NeverShown)
    return false;
  return track.ShowTo < 0.0 || (time >= track.ShowFrom && time <= track.ShowTo);
}

bool ReplayModels::HidesLive(Track const& track, double time)
{
  if (!track.HideLive || track.NeverShown || track.ShowTo < 0.0 || track.Light)
    return false;
  return ((track.HideLive & 1) && time < track.ShowFrom) || ((track.HideLive & 2) && time > track.ShowTo);
}

int ReplayModels::VisibleAt(Track const& track, double time) const
{
  int visible = track.VisibleStart;
  for (auto const& change : track.VisibleChanges)
  {
    if (change.first > time)
      break;
    visible = change.second;
  }
  return visible;
}

void ReplayModels::GiveVisible(Track& track, double time)
{
  if (track.Light || track.VisibleStart < 0)
    return;
  // The model made again since: what it had given back where that's still ours
  if (track.CellIndex >= 0 && track.CellEntity != track.Entity)
    GiveBackVisible(track);
  int wanted = VisibleAt(track, time);
  int visible = -1;
  __int64 cell = 0;
  if (!util::hooks::ReadStaticModelVisibleCell(track.Entity, visible, cell))
    return;
  if (track.CellIndex >= 0)
  {
    VisibleCell& mine = g_VisibleCells[track.CellIndex];
    if (cell == (__int64)&mine)
    {
      mine.Visible = (BYTE)wanted;
      return;
    }
    // The game bound it again meanwhile: its new cell is the game's now
    g_VisibleCellUsed[track.CellIndex] = false;
    track.CellIndex = -1;
  }
  if (visible == wanted)
    return;
  int index = -1;
  for (int i = 0; i < MaxVisibleCells && index < 0; ++i)
    if (!g_VisibleCellUsed[i])
      index = i;
  if (index < 0)
    return;
  VisibleCell& mine = g_VisibleCells[index];
  mine.Value = (__int64)&mine.Visible;
  mine.Flags = 0x40;
  mine.Visible = (BYTE)wanted;
  if (!util::hooks::SwapStaticModelVisibleCell(track.Entity, cell, (__int64)&mine))
    return;
  g_VisibleCellUsed[index] = true;
  track.CellIndex = index;
  track.GameCell = cell;
  track.CellEntity = track.Entity;
  ++m_VisibleGiven;
}

void ReplayModels::GiveBackVisible(Track& track)
{
  if (track.CellIndex < 0)
    return;
  util::hooks::SwapStaticModelVisibleCell(track.CellEntity, (__int64)&g_VisibleCells[track.CellIndex], track.GameCell);
  g_VisibleCellUsed[track.CellIndex] = false;
  track.CellIndex = -1;
  track.CellEntity = 0;
}

void ReplayModels::StopPlaying()
{
  if (!m_Playing)
    return;
  m_Playing = false;
  replay_shapes::SetDraws(nullptr, 0);
  if (m_VisibleGiven)
    util::log::Write("Replay: %d times a model was given the Visible it had while it recorded (a collected item)", m_VisibleGiven);
  m_VisibleGiven = 0;
  for (Track& track : m_Tracks)
  {
    GiveBackVisible(track);
    float now[16];
    alignas(16) float live[16];
    memcpy(live, track.Live, sizeof(live));
    if (track.HasLive && Read(track, now) && !Same(now, live))
      Write(track, live);
    track.HasLive = false;
    track.LiveHidden = false;
  }
}

void ReplayModels::Clear()
{
  util::hooks::NoteMovingLights(false);
  StopPlaying();
  m_Tracks.clear();
  m_Index.clear();
  m_FaithTimes.clear();
  m_FaithPlaces.clear();
  replay_shapes::Clear();
  m_LastScan = -1.0;
}

// The recorded models with no shape kept (2026-10-27, stage 2b): a skinned
// door ("not a rigid mesh the replay can read") is drawn with the game's own
// skinned draws of it (found by its mesh's vertex buffers), its bones kept
// relative to its transform (see FaithReplay::AdoptGameDraws)
bool ReplayModels::RecordedTransform(__int64 entity, double time, float* transform) const
{
  for (Track const& track : m_Tracks)
    if (track.Entity == entity)
    {
      if (track.Light)
        return false;
      if (track.Keys.empty())
        memcpy(transform, track.Last, sizeof(track.Last));
      else
        Evaluate(track, time, transform);
      return true;
    }
  return false;
}

bool ReplayModels::GoneTransform(__int64 entity, double time, float* transform) const
{
  for (Track const& track : m_Tracks)
    if (track.Entity == entity)
    {
      if (track.HasLive || track.Light || track.Keys.empty() || VisibleAt(track, time) == 0)
        return false;
      Evaluate(track, time, transform);
      return true;
    }
  return false;
}

void ReplayModels::ShapelessMoved(std::vector<std::pair<__int64, std::string>>& out) const
{
  out.clear();
  for (Track const& track : m_Tracks)
    if (!track.Light && track.Shape < 0 && !track.Keys.empty() && !track.Still)
      out.push_back({ track.Entity, track.Label });
}

void ReplayModels::Save(replay_store::Out& out, replay_store::Keys& keys) const
{
  unsigned count = 0;
  for (Track const& track : m_Tracks)
    count += !track.Light && !track.Keys.empty();
  out.Put(count);
  for (Track const& track : m_Tracks)
  {
    if (track.Light || track.Keys.empty())
      continue;
    out.Put(keys.Of(track.Entity));
    out.Put(track.VTable);
    out.Bytes(track.Label, sizeof(track.Label));
    out.Bytes(track.Last, sizeof(track.Last));
    out.Put(track.LastTime);
    out.Vec(track.Keys);
    out.Put(track.VisibleFirst);
    out.Put(track.VisibleStart);
    out.Put((unsigned)track.VisibleChanges.size());
    for (auto const& change : track.VisibleChanges)
    {
      out.Put(change.first);
      out.Put(change.second);
    }
    out.Put(track.Shape);
    out.Put((unsigned char)track.Still);
  }
  // When each was shown (2026-11-02; see StopRecording), after the tracks: an
  // older file has none and an older build doesn't read it
  out.Put(ShowTag);
  for (Track const& track : m_Tracks)
  {
    if (track.Light || track.Keys.empty())
      continue;
    out.Put(track.ShowFrom);
    out.Put(track.ShowTo);
    out.Put((unsigned char)track.NeverShown);
  }
  // Which follow another (2026-11-02; see ShowAndFollow), after that
  out.Put(FollowTag);
  for (Track const& track : m_Tracks)
  {
    if (track.Light || track.Keys.empty())
      continue;
    out.Put((unsigned char)track.Follows);
  }
  // Where the game's own is taken out of sight (2026-11-03; see ShowAndFollow), after that
  out.Put(HideTag);
  for (Track const& track : m_Tracks)
  {
    if (track.Light || track.Keys.empty())
      continue;
    out.Put(track.HideLive);
  }
  // Which are a Dash's markers (2026-11-08; see ShowAndFollow), after that:
  // 0 no, 1 yes, 2 not known
  out.Put(RvTag);
  for (Track const& track : m_Tracks)
  {
    if (track.Light || track.Keys.empty())
      continue;
    out.Put((unsigned char)(!track.RvKnown ? 2 : track.RvLayer ? 1 : 0));
  }
}

bool ReplayModels::Load(replay_store::In& in)
{
  unsigned count = 0;
  if (!in.Get(count) || count > MaxCandidates)
    return false;
  std::vector<Track> tracks(count);
  for (Track& track : tracks)
  {
    long long key = 0;
    unsigned changes = 0;
    unsigned char still = 0;
    if (!in.Get(key) || !in.Get(track.VTable) || !in.Bytes(track.Label, sizeof(track.Label)) ||
        !in.Bytes(track.Last, sizeof(track.Last)) || !in.Get(track.LastTime) || !in.Vec(track.Keys, 1 << 20) ||
        track.Keys.empty() || !in.Get(track.VisibleFirst) || !in.Get(track.VisibleStart) || !in.Get(changes) || changes > 256)
      return false;
    track.VisibleChanges.resize(changes);
    for (auto& change : track.VisibleChanges)
      if (!in.Get(change.first) || !in.Get(change.second))
        return false;
    if (!in.Get(track.Shape) || !in.Get(still))
      return false;
    track.Label[sizeof(track.Label) - 1] = 0;
    // Not the game's: nothing there until it's found again (see Refind)
    track.Entity = replay_store::AddressOf(key);
    track.Data = 0;
    track.Light = false;
    track.Gone = true;
    track.HasLive = false;
    track.NextRefind = 0.0;
    track.CellIndex = -1;
    track.GameCell = 0;
    track.CellEntity = 0;
    track.Still = still != 0;
  }
  // When each was shown, when the file has it (see Save)
  unsigned tag = 0;
  if (in.Left() >= sizeof(tag) && in.Get(tag) && tag == ShowTag)
    for (Track& track : tracks)
    {
      unsigned char never = 0;
      if (!in.Get(track.ShowFrom) || !in.Get(track.ShowTo) || !in.Get(never))
        return false;
      track.NeverShown = never != 0;
    }
  // Which follow another, when the file has it
  if (tag == ShowTag && in.Left() >= sizeof(tag) && in.Get(tag) && tag == FollowTag)
    for (Track& track : tracks)
    {
      unsigned char follows = 0;
      if (!in.Get(follows))
        return false;
      track.Follows = follows != 0;
      if (track.Follows)
        track.Path = PathOf(track.Keys);
    }
  // Where the game's own is taken out of sight, when the file has it
  if (tag == FollowTag && in.Left() >= sizeof(tag) && in.Get(tag) && tag == HideTag)
    for (Track& track : tracks)
      if (!in.Get(track.HideLive))
        return false;
  // Which are a Dash's markers, when the file has it (else found out once each is found again: see Refind)
  if (tag == HideTag && in.Left() >= sizeof(tag) && in.Get(tag) && tag == RvTag)
    for (Track& track : tracks)
    {
      unsigned char rv = 0;
      if (!in.Get(rv) || rv > 2)
        return false;
      track.RvKnown = rv != 2;
      track.RvLayer = rv == 1;
    }
  // Not Clear: the shapes and kept draws are read on their own
  StopPlaying();
  m_Index.clear();
  m_LastScan = -1.0;
  m_Tracks.swap(tracks);
  return true;
}
