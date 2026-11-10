#include "ReplayVehicles.h"
#include "../Main.h"
#include "../Util/Util.h"
#include <DirectXMath.h>
#include <Windows.h>
#include <cmath>
#include <cstring>

using namespace DirectX;

namespace
{
  // The classes, by their TypeInfo (their instances are in its four lists,
  // + 0x60 .. + 0x90, linked through the entity's + 0x40; vehicles are listed
  // by 0x1431665B0, which uses + 0x80 / + 0x90 when + 0x18 bit 0x10 isn't set)
  const __int64 ClientVehicleTypeInfoRva = 0x27F10D0;   // ClientVehicleEntity
  const __int64 ServerVehicleTypeInfoRva = 0x27FF880;   // ServerVehicleEntity
  const int TypeInstanceListsOffset = 0x60;
  const int EntityLinkOffset = 0x40;
  const int MaxPerList = 4096;
  const int MaxFound = 256;
  const float RecordRadius = 300.f;          // a VTOL keeps its distance (it fires from afar)
  const float ServerMatchDistance = 15.f;    // the server vehicle's place from the client one's (the client's lags it)
  const double ScanInterval = 1.0;
  const size_t MaxVehicles = 16;
  const float SameEpsilon = 1e-4f;
  const float HideDrop = 10000.f;            // how far below the world a hidden one is put
  const int CheckFrame = 15;                 // diagnostic (temporary)

  bool TypeNamed(__int64 typeInfo, char const* name)
  {
    __try
    {
      return !strcmp(*(char const**)*(__int64 const*)typeInfo, name);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
  }

  // Both classes as the tools know them (checked once)
  bool KnownClasses()
  {
    static int s_ok = -1;
    if (s_ok < 0)
    {
      __int64 base = (__int64)g_gameHandle;
      s_ok = TypeNamed(base + ClientVehicleTypeInfoRva, "ClientVehicleEntity") &&
        TypeNamed(base + ServerVehicleTypeInfoRva, "ServerVehicleEntity") ? 1 : 0;
      if (!s_ok)
        util::log::Warning("Replay: the vehicle classes aren't the ones the tools know, vehicles aren't recorded");
    }
    return s_ok == 1;
  }

  // What an entity's getType gives (vtable [0]: lea rax, [rip + x]; ret)
  __int64 TypeOf(__int64 entity)
  {
    __try
    {
      BYTE const* code = *(BYTE const**)*(__int64 const*)entity;
      if (code[0] != 0x48 || code[1] != 0x8D || code[2] != 0x05 || code[7] != 0xC3)
        return 0;
      return (__int64)code + 7 + *(int const*)(code + 3);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return 0;
    }
  }

  void WalkList(__int64 head, __int64 typeInfo, __int64* out, int max, int& count)
  {
    __try
    {
      __int64 link = *(__int64 const*)head;
      for (int n = 0; link >= 0x10000 && n < MaxPerList && count < max; ++n, link = *(__int64 const*)link)
      {
        __int64 entity = link - EntityLinkOffset;
        if (TypeOf(entity) == typeInfo)
          out[count++] = entity;
      }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
  }

  int Instances(__int64 typeInfo, __int64* out, int max)
  {
    int count = 0;
    for (int list = 0; list < 4; ++list)
      WalkList(typeInfo + TypeInstanceListsOffset + list * 0x10, typeInfo, out, max, count);
    return count;
  }

  // An entity's transform, the 4 rows where the getter (slot 24, 0x1432E1440)
  // reads them: components (+ 0x38) + 0x10 + (byte 9 + 2 * byte 0xA) * 0x20
  bool ReadRows(__int64 entity, float* rows)
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

  __int64 ReadVTable(__int64 entity)
  {
    __try { return *(__int64 const*)entity; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
  }

  // Still that vehicle, and its transform
  bool Still(__int64 entity, __int64 vtable, float* rows)
  {
    __try
    {
      if (*(__int64 const*)entity != vtable)
        return false;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
    return ReadRows(entity, rows);
  }

  bool Same(float const* a, float const* b)
  {
    for (int i = 0; i < 16; ++i)
      if (fabsf(a[i] - b[i]) > SameEpsilon)
        return false;
    return true;
  }

  float Distance3(float const* a, float const* b)
  {
    float dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
    return sqrtf(dx * dx + dy * dy + dz * dz);
  }

}

void ReplayVehicles::StartRecording()
{
  Clear();
  m_Recording = KnownClasses() && util::hooks::CanMoveVehicles();
  m_LastScan = -1.0;
}

// The vehicles within RecordRadius of her, each once
void ReplayVehicles::Scan(float const* faith)
{
  __int64 base = (__int64)g_gameHandle;
  __int64 found[MaxFound];
  int count = Instances(base + ClientVehicleTypeInfoRva, found, MaxFound);
  for (int i = 0; i < count && m_Tracks.size() < MaxVehicles; ++i)
  {
    bool known = false;
    for (Track const& track : m_Tracks)
      known |= track.Entity == found[i];
    float rows[16];
    __int64 vtable = ReadVTable(found[i]);
    if (known || !vtable || !ReadRows(found[i], rows) || !(Distance3(rows + 12, faith) <= RecordRadius))
      continue;
    Track track{};
    track.Entity = found[i];
    track.VTable = vtable;
    memcpy(track.Last, rows, sizeof(rows));
    track.LastTime = -1.0;
    m_Tracks.push_back(std::move(track));
  }
}

// Its server vehicle: the one nearest its place, within ServerMatchDistance
void ReplayVehicles::FindServer(Track& track)
{
  __int64 base = (__int64)g_gameHandle;
  __int64 found[MaxFound];
  int count = Instances(base + ServerVehicleTypeInfoRva, found, MaxFound);
  float best = ServerMatchDistance;
  for (int i = 0; i < count; ++i)
  {
    float rows[16];
    if (!ReadRows(found[i], rows))
      continue;
    float distance = Distance3(rows + 12, track.Last + 12);
    if (distance <= best)
    {
      best = distance;
      track.Server = found[i];
    }
  }
}

int ReplayVehicles::MovingPlaces(float* xyz, int max) const
{
  int count = 0;
  if (!m_Recording)
    return 0;
  for (Track const& track : m_Tracks)
  {
    if (count >= max)
      break;
    // A change is recorded after the first sample once it moved
    if (track.Gone || track.Times.size() < 2)
      continue;
    memcpy(xyz + count * 3, track.Last + 12, 3 * sizeof(float));
    ++count;
  }
  return count;
}

__int64 FindServerEntityNear(__int64 typeInfoRva, float const* place, float within)
{
  __int64 found[MaxFound];
  int count = Instances((__int64)g_gameHandle + typeInfoRva, found, MaxFound);
  __int64 best = 0;
  float bestDistance = within;
  for (int i = 0; i < count; ++i)
  {
    float rows[16];
    if (!ReadRows(found[i], rows))
      continue;
    float distance = Distance3(rows + 12, place);
    if (distance <= bestDistance)
    {
      bestDistance = distance;
      best = found[i];
    }
  }
  return best;
}

void ReplayVehicles::RecordFrame(double time, float const* faith)
{
  if (!m_Recording)
    return;
  if (m_LastScan < 0 || time - m_LastScan >= ScanInterval)
  {
    Scan(faith);
    m_LastScan = time;
  }
  for (Track& track : m_Tracks)
  {
    if (track.Gone)
      continue;
    float now[16];
    if (!Still(track.Entity, track.VTable, now))
    {
      track.Gone = true;
      util::log::Write("Replay:   vehicle 0x%I64X went at %.2f s", track.Entity, time);
      continue;
    }
    if (!track.Server)
    {
      memcpy(track.Last, now, sizeof(now));
      FindServer(track);
    }
    // The first sample, and every change (with where it was until then)
    bool first = track.Times.empty();
    if (first || !Same(now, track.Last))
    {
      if (!first && track.Times.back() < track.LastTime)
      {
        track.Times.push_back(track.LastTime);
        track.Rows.insert(track.Rows.end(), track.Last, track.Last + 16);
      }
      track.Times.push_back(time);
      track.Rows.insert(track.Rows.end(), now, now + 16);
      memcpy(track.Last, now, sizeof(now));
    }
    track.LastTime = time;
  }
}

void ReplayVehicles::StopRecording()
{
  if (!m_Recording)
    return;
  m_Recording = false;
  // Held to the end where it stopped moving
  for (Track& track : m_Tracks)
    if (!track.Times.empty() && !track.Gone && track.LastTime > track.Times.back())
    {
      track.Times.push_back(track.LastTime);
      track.Rows.insert(track.Rows.end(), track.Last, track.Last + 16);
    }
  // Only the ones that moved
  size_t seen = m_Tracks.size();
  std::vector<Track> moved;
  for (Track& track : m_Tracks)
  {
    bool moves = false;
    for (size_t k = 1; k < track.Times.size() && !moves; ++k)
      moves = !Same(&track.Rows[k * 16], &track.Rows[0]);
    if (moves)
      moved.push_back(std::move(track));
  }
  m_Tracks = std::move(moved);
  util::log::Write("Replay: %d of the %d vehicles within %.0f m of Faith moved and are in the recording", (int)m_Tracks.size(),
    (int)seen, RecordRadius);
  for (Track const& track : m_Tracks)
    util::log::Write("Replay:   vehicle 0x%I64X (server 0x%I64X): %d places, %.2f - %.2f s%s", track.Entity, track.Server,
      (int)track.Times.size(), track.Times.front(), track.Times.back(), track.Gone ? " (gone before the end)" : "");
}

int ReplayVehicles::ServerEntities(__int64* out, int max) const
{
  int count = 0;
  for (Track const& track : m_Tracks)
    if (track.Server && count < max)
      out[count++] = track.Server;
  return count;
}

void ReplayVehicles::StartPlaying()
{
  m_Playing = true;
  m_MoveFailures = 0;
  int live = 0;
  for (Track& track : m_Tracks)
  {
    track.HasLive = Still(track.Entity, track.VTable, track.Live);
    track.Held = false;
    track.Frames = 0;
    track.HasLastWanted = false;
    live += track.HasLive;
  }
  if (!m_Tracks.empty())
    util::log::Write("Replay: playing %d vehicles (%d of them there now)", (int)m_Tracks.size(), live);
}

// Its recorded place at a time: in a straight line between two places, the
// turn along the shorter way round; 10 km down before it was first seen, and
// after it went if it went
void ReplayVehicles::Evaluate(Track const& track, double time, float* out) const
{
  size_t count = track.Times.size();
  bool before = time < track.Times.front();
  bool after = time >= track.Times.back();
  if (before || after)
  {
    memcpy(out, &track.Rows[(before ? 0 : count - 1) * 16], 16 * sizeof(float));
    if (before || track.Gone)
      out[13] -= HideDrop;
    return;
  }
  size_t lo = 0, hi = count - 1;
  while (hi - lo > 1)
  {
    size_t mid = (lo + hi) / 2;
    if (track.Times[mid] <= time) lo = mid; else hi = mid;
  }
  double span = track.Times[hi] - track.Times[lo];
  float f = span > 0 ? (float)((time - track.Times[lo]) / span) : 0.f;
  XMMATRIX a = XMLoadFloat4x4((XMFLOAT4X4 const*)&track.Rows[lo * 16]);
  XMMATRIX b = XMLoadFloat4x4((XMFLOAT4X4 const*)&track.Rows[hi * 16]);
  XMVECTOR sa, ra, ta, sb, rb, tb;
  if (XMMatrixDecompose(&sa, &ra, &ta, a) && XMMatrixDecompose(&sb, &rb, &tb, b))
    XMStoreFloat4x4((XMFLOAT4X4*)out, XMMatrixAffineTransformation(XMVectorLerp(sa, sb, f), XMVectorZero(),
      XMQuaternionSlerp(ra, rb, f), XMVectorLerp(ta, tb, f)));
  else
    for (int i = 0; i < 16; ++i)
      out[i] = track.Rows[lo * 16 + i] + (track.Rows[hi * 16 + i] - track.Rows[lo * 16 + i]) * f;
}

// Diagnostic (temporary), once a playback: whether something else moved the
// entity since the replay put it somewhere last frame (now: its place read
// before this frame's move), and where it is right after this frame's
void ReplayVehicles::Check(Track& track, float const* now, float const* wanted)
{
  if (++track.Frames != CheckFrame)
    return;
  float after[16];
  bool moved = ReadRows(track.Entity, after);
  util::log::Write("Replay (diagnostic):   vehicle 0x%I64X at frame %d: %.2f m from where the replay put it last frame before this "
    "frame's move; wanted at %.2f %.2f %.2f, its entity at %.2f %.2f %.2f after it (%.2f m off)", track.Entity, CheckFrame,
    track.HasLastWanted ? Distance3(now + 12, track.LastWanted + 12) : -1.f, wanted[12], wanted[13], wanted[14],
    moved ? after[12] : 0.f, moved ? after[13] : 0.f, moved ? after[14] : 0.f, moved ? Distance3(after + 12, wanted + 12) : -1.f);
}

void ReplayVehicles::Play(double time)
{
  if (m_Tracks.empty())
    return;
  if (!m_Playing)
    StartPlaying();
  for (Track& track : m_Tracks)
  {
    if (!track.HasLive)
      continue;
    float now[16];
    if (!Still(track.Entity, track.VTable, now))
    {
      // Gone: let go at once (its memory may be another's soon)
      if (track.Held)
        util::hooks::HoldActor(track.Entity, false);
      util::hooks::DrawVehicleAt(track.Entity, nullptr);
      track.Held = track.HasLive = false;
      util::log::Write("Replay:   vehicle 0x%I64X went while the replay played", track.Entity);
      continue;
    }
    // Outside its recorded time (before it was first seen, after it went) it's
    // left where the game has it: its physics body put 10 km down would leave
    // the world
    if (track.Times.empty() || time < track.Times.front() || (track.Gone && time > track.Times.back()))
    {
      if (track.Held)
        util::hooks::HoldActor(track.Entity, false);
      util::hooks::DrawVehicleAt(track.Entity, nullptr);
      track.Held = false;
      continue;
    }
    alignas(16) float wanted[16];
    Evaluate(track, time, wanted);
    // The rows' fourth floats as they are (not part of the transform)
    for (int r = 0; r < 4; ++r)
      wanted[r * 4 + 3] = now[r * 4 + 3];
    if (!track.Held)
    {
      util::hooks::HoldActor(track.Entity, true);
      track.Held = true;
    }
    // Moved with its physics body: a vehicle is drawn where its physics has it
    bool moved = util::hooks::MoveVehiclePhysics(track.Entity, wanted);
    track.Physics = track.Physics || moved;
    // And kept there when the game's own changes of it come through (see
    // util::hooks::DrawVehicleAt)
    util::hooks::DrawVehicleAt(track.Entity, wanted);
    if (!moved)
    {
      if (m_MoveFailures++ == 0)
        util::log::Warning("Replay: vehicle 0x%I64X couldn't be moved", track.Entity);
      continue;
    }
    if (util::DiagnosticsOn())
    {
      Check(track, now, wanted);
      memcpy(track.LastWanted, wanted, sizeof(track.LastWanted));
      track.HasLastWanted = true;
    }
  }
}

void ReplayVehicles::StopPlaying()
{
  if (!m_Playing)
    return;
  m_Playing = false;
  // The game's changes of them their own again first (the live places below
  // aren't overwritten)
  int placed = util::hooks::ReleaseDrawnVehicles();
  if (!m_Tracks.empty())
    util::log::Write("Replay: the game's own changes of the replayed vehicles' places were given the replay's place %d times", placed);
  for (Track& track : m_Tracks)
  {
    float now[16];
    if (track.HasLive && Still(track.Entity, track.VTable, now))
    {
      alignas(16) float live[16];
      memcpy(live, track.Live, sizeof(live));
      if (track.Physics)
        util::hooks::MoveVehiclePhysics(track.Entity, live);
      else
        util::hooks::MoveVehicleDrawn(track.Entity, live);
    }
    if (track.Held)
      util::hooks::HoldActor(track.Entity, false);
    track.Held = track.HasLive = track.Physics = false;
  }
}

void ReplayVehicles::Clear()
{
  StopPlaying();
  m_Tracks.clear();
  m_Recording = false;
  m_LastScan = -1.0;
}
