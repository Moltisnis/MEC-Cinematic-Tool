#pragma once
#include <vector>

// The vehicles near Faith while the replay records (a KrugerSec VTOL is a
// ClientVehicleEntity, vtable 0x141B12D80), recorded with her and played back
// with her. Each one's transform (the entity's rows, where the game's getter
// reads them) is recorded at every replay sample while it moves; while the
// replay plays it's set again through the models' setter's body, drawn only
// (util::hooks::MoveVehicleDrawn: its physics body stays where the game has
// it), with the game's own moves of it held back meanwhile
// (util::hooks::HoldActor). Before it was first seen, and after it went if it
// went while recording, it's put 10 km down. Its live place is given back when
// playback stops. Its server vehicle is found by place while recording and
// kept in the level by who it is (util::hooks::SetKeptVehicles), so it's
// still there to be played. Game thread only (FaithReplay calls it from the
// camera hook).
// The entity of a class (its TypeInfo's RVA) nearest a place, within that many
// metres (0: none); game thread. For finding a recorded drone's server entity.
__int64 FindServerEntityNear(__int64 typeInfoRva, float const* place, float within);

class ReplayVehicles
{
public:
  void StartRecording();
  // At every replay sample, on the replay's clock; faith: where she is
  void RecordFrame(double time, float const* faith);
  // Keeps only the vehicles that moved
  void StopRecording();
  // Every frame while the replay plays (or holds, paused), at its time; the
  // first call takes each vehicle over
  void Play(double time);
  // Gives the live places back and lets the vehicles go
  void StopPlaying();
  void Clear();
  int Count() const { return (int)m_Tracks.size(); }
  // The server vehicles found for the recorded ones (for keeping them in the level)
  int ServerEntities(__int64* out, int max) const;
  // While recording: where the vehicles are that moved since they were first
  // seen and are still there (x y z each, at their last sample), at most max
  int MovingPlaces(float* xyz, int max) const;

private:
  struct Track
  {
    __int64 Entity;
    __int64 VTable;
    __int64 Server;              // its server vehicle, 0 until found
    std::vector<double> Times;
    std::vector<float> Rows;     // 16 a time
    float Last[16];              // read last sample
    double LastTime;
    bool Gone;                   // not there any more (while recording)
    // This playback
    float Live[16];              // its place when the playback took it
    bool HasLive;
    bool Held;
    bool Physics;                // moved with its physics body this playback
    int Frames;                  // diagnostic (temporary): frames moved
    float LastWanted[16];        // diagnostic (temporary): where it was put last frame
    bool HasLastWanted;
  };

  void Scan(float const* faith);
  void FindServer(Track& track);
  void StartPlaying();
  void Evaluate(Track const& track, double time, float* out) const;
  void Check(Track& track, float const* now, float const* wanted);

  std::vector<Track> m_Tracks;
  double m_LastScan{ -1.0 };
  bool m_Recording{ false };
  bool m_Playing{ false };
  int m_MoveFailures{ 0 };
};
