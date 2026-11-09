#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace replay_store { class Out; class In; class Keys; }

// The models near Faith that move while the replay records (doors turning
// open, elevators, the MagRope's visuals, collectables opening), recorded
// with her and played back with her. The level moves them as static models
// (PamClientStaticModelEntity, ClientStaticModelEntity: world log,
// 2026-10-23), so they're read with the game's getTransform (vtable slot 24)
// and put back with its setTransform (slot 25, 0x1432E8300), which also tells
// the renderer. Only what's drawn moves: nothing linked to it, and not its
// collision (see SetTransform). The lights the level moves near her (an
// elevator lamp's) are recorded and moved on their own. Their live places are
// given back when playback stops.
// Game thread only (FaithReplay calls it from the camera hook).
class ReplayModels
{
public:
  // faith: where she is; soldier: her soldier, for the scan's classes
  void StartRecording(float const* faith, __int64 soldier);
  // At every replay sample, on the replay's clock
  void RecordFrame(double time, float const* faith, __int64 soldier);
  // Keeps only the models that moved
  void StopRecording();
  // Keeps each recorded model's live place
  void StartPlaying();
  // Every frame while the replay plays (or holds, paused), at its time;
  // around: where the replayed Faith is (null: not known), round which the
  // models that aren't there any more are looked for again (see Refind)
  void Play(double time, float const* around);
  // Gives the live places back
  void StopPlaying();
  void Clear();
  // Models in the recording (that moved)
  int Count() const { return (int)m_Tracks.size(); }
  bool IsPlaying() const { return m_Playing; }
  // The recorded models the game doesn't have when it plays (a mission's):
  // drawn by the replay from the shapes it kept while recording (see
  // ReplayShapes.h)
  void SetShapes(bool on) { m_ShapesOn = on; }
  // The recorded models with no shape kept (a skinned door), for the game's own
  // draws of them (FaithReplay::AdoptGameDraws, stage 2b). ShapelessMoved: the
  // ones that moved (while recording: so far). RecordedTransform: while
  // recording, one's transform at a time of the recording (its place before it
  // moved, the moves in between, its last after). GoneTransform: while
  // playing, one the game doesn't have, its transform at the time (false if
  // it's there or hidden then).
  void ShapelessMoved(std::vector<std::pair<__int64, std::string>>& out) const;
  bool RecordedTransform(__int64 entity, double time, float* transform) const;
  bool GoneTransform(__int64 entity, double time, float* transform) const;
  // Saved recordings (ReplayStore.h): the tracks written (the lights' not:
  // only the game's own lights move), their entities as keys; read back in
  // place of these, each gone until it's found again near the replayed Faith
  // (see Refind) and drawn from its shape meanwhile
  void Save(replay_store::Out& out, replay_store::Keys& keys) const;
  bool Load(replay_store::In& in);

private:
  struct Key
  {
    double Time;
    float Transform[16];
  };
  struct Track
  {
    __int64 Entity;
    __int64 VTable;
    __int64 Data;         // a static model's data when found (another model built in its place has another)
    bool Light;           // a light (moved through its own transform, see util::hooks::MoveLight)
    char Label[40];
    float Last[16];       // read last sample
    double LastTime;
    std::vector<Key> Keys; // empty until it moves: the place just before, then every change
    bool Gone;            // not readable any more (while recording)
    float Live[16];       // its place when playback started, or when it was found again
    bool HasLive;
    double NextRefind;    // while it plays and isn't there: when it's looked for again (wall clock, s)
    int VisibleFirst = -1; // diagnostic: its Visible as it first moved (-1: not a single static model)
    // A single static model's Visible as the game's reader gave it while it
    // recorded: at the start, then each change (see Play)
    int VisibleStart = -1;
    std::vector<std::pair<double, int>> VisibleChanges;
    // While it plays: the reader pointed at the replay's own cell (one of
    // g_VisibleCells), and the game's cell it had
    int CellIndex = -1;
    __int64 GameCell = 0;
    __int64 CellEntity = 0;
    int Shape = -1;       // its mesh as kept while it recorded (replay_shapes), -1 none
    bool Still = false;   // didn't move: in the recording for its shape only (one place)
    // One the game shows and hides (see StopRecording): drawn only from
    // ShowFrom to ShowTo of the recording's time (ShowTo < 0: always), or
    // never (NeverShown)
    double ShowFrom = 0.0, ShowTo = -1.0;
    bool NeverShown = false;
    // Where the camera saw the game not draw it before ShowFrom (1) or after
    // ShowTo (2): the game's own taken out of sight then while it plays
    // (LiveHidden: it is now; see Play)
    unsigned char HideLive = 0;
    bool LiveHidden = false;
    // One that didn't move itself but that the game drew going along with
    // another recorded model (a sign on a gate's door; see StopRecording):
    // its Keys are that one's moves, it stays Still (the game's own isn't
    // moved), and its draws are matched by where it was then (Path: 4 floats
    // a point, time and x y z, made from its Keys)
    bool Follows = false;
    std::shared_ptr<std::vector<float> const> Path;
    // While it plays and the game has it: whether the game draws it where it
    // was (the last answer, since when) and so whether its recorded copy is
    // drawn too (see Play)
    int LiveDrawn = -1;
    double LiveDrawnSince = 0.0;
    bool CopyLive = false;
    std::string Mesh;     // its shape's mesh (replay_shapes::NameOf), once asked
    // Drawn by the game only in its runner's vision layer (a Dash's start,
    // checkpoint arrows and finish: attach::PrimaryInRvLayer), and whether
    // that's known (a recording saved before 2026-11-08: once it's found)
    bool RvLayer = false;
    bool RvKnown = false;
  };
  bool m_ShapesOn{ true };
  int VisibleAt(Track const& track, double time) const;
  static bool ShownAt(Track const& track, double time);
  static bool HidesLive(Track const& track, double time);
  void GiveVisible(Track& track, double time);
  void GiveBackVisible(Track& track);
  int m_VisibleGiven{ 0 };
  int m_CopyLiveLogs{ 0 };

  void Scan(float const* faith, __int64 soldier);
  void AddLights(double time, float const* faith);
  bool Read(Track const& track, float* out) const;
  bool Write(Track const& track, float const* transform) const;
  void Evaluate(Track const& track, double time, float* out) const;
  int Refind(float const* around);

  void ShowAndFollow();
  float FaithDistance(double time, float const* place) const;

  std::vector<Track> m_Tracks;
  std::unordered_map<__int64, size_t> m_Index;  // while recording: entity -> track
  // While it records: where Faith was (every FaithPathSeconds), for ShowAndFollow
  std::vector<double> m_FaithTimes;
  std::vector<float> m_FaithPlaces;
  double m_LastScan{ -1.0 };
  double m_LastSampleTime{ 0.0 };
  double m_LastRefindScan{ -1e9 };   // wall clock, s
  bool m_Playing{ false };
  int m_SetFailures{ 0 };
};
