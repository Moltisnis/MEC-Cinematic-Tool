#pragma once
#include "ReplayGameDraws.h"
#include "ReplayModels.h"
#include "ReplayStore.h"
#include "ReplayVehicles.h"
#include "../Visuals/Weather.h"
#include <memory>
#include <vector>

// Records Faith's movement (the pose of her body, every bone, 30 times a
// second) and plays it back on her own body, the way the game plays a Dash
// ghost: an override of her pose in the animation system. While it plays,
// her body runs the recording wherever it went; Faith herself stays where she
// is. Not thread safe on its own: CameraManager calls it with its mutex held,
// from the camera hook (game thread) and the menu.
class FaithReplay
{
public:
  FaithReplay();
  ~FaithReplay();

  // Every frame, from the camera hook. soldier: Faith's soldier entity (0 when
  // there's none). trackPlaying / trackPaused: the camera track, for Play
  // along with the camera track. fixedDt: the fixed playback step (1 / FPS)
  // if there is one, 0 for real time. bodyShown: the free camera is on with
  // her third-person body (her own camera unlinked), the only time her box
  // pose is moved too: it also has the camera joint her own camera follows.
  // faithRows: her soldier's transform (4x4, rows; null when it isn't known),
  // kept with each sample for GetPlayedFaith.
  void Update(__int64 soldier, bool trackPlaying, bool trackPaused, double fixedDt, bool bodyShown, float const* faithRows);

  // While a recording plays: where the replayed Faith is (at her feet, as her
  // soldier was when recorded) and her heading then (radians, the soldier's
  // atan2(row 2 x, row 2 z); hasHeading false when it wasn't known). False
  // when nothing plays.
  bool GetPlayedFaith(float* place, float& heading, bool& hasHeading);

  // The REPLAY tab. True if a setting changed (for the config).
  bool DrawUI();
  // One line for the status bar, empty when there's nothing to say
  const char* GetStatus(char* buffer, size_t size);

  bool IsLooping() { return m_Loop; }
  void SetLooping(bool loop) { m_Loop = loop; }
  bool IsWithTrack() { return m_WithTrack; }
  void SetWithTrack(bool withTrack) { m_WithTrack = withTrack; }
  // The camera track was sent to another moment (seconds of replay time):
  // playing along with it, the replay goes there too (in Update)
  void SeekAlongWithTrack(double seconds) { if (m_WithTrack) m_SeekTo = (float)(seconds > 0 ? seconds : 0.0); }
  // How fast it plays: times real time (the camera manager's game speed, 0
  // while time is frozen; 1 = as recorded). Recording stays in real time.
  void SetPlaySpeed(double speed) { m_PlaySpeed = speed > 0 ? speed : 0.0; }
  bool IsSounds() { return m_Sounds; }
  void SetSounds(bool sounds) { m_Sounds = sounds; }
  bool IsModels() { return m_ModelsOn; }
  void SetModels(bool models) { m_ModelsOn = models; }
  bool IsTimeOfDay() { return m_TodOn; }
  void SetTimeOfDay(bool timeOfDay) { m_TodOn = timeOfDay; }
  bool IsStartAfterLoad() { return m_StartAfterLoad; }
  void SetStartAfterLoad(bool after) { m_StartAfterLoad = after; }
  bool IsStandIns() { return m_StandInsOn; }
  void SetStandIns(bool standIns) { m_StandInsOn = standIns; }
  bool IsSoundTrack() { return m_SoundTrackOn; }
  void SetSoundTrack(bool soundTrack) { m_SoundTrackOn = soundTrack; }
  bool IsModelShapes() { return m_ModelShapesOn; }
  void SetModelShapes(bool shapes) { m_ModelShapesOn = shapes; m_Models.SetShapes(shapes); }
  bool IsKeepWorld() { return m_KeepWorldOn; }
  void SetKeepWorld(bool keep) { m_KeepWorldOn = keep; }
  bool IsGameDraws() { return m_GameDrawsOn; }
  void SetGameDraws(bool on) { m_GameDrawsOn = on; }
  bool IsReloadBundles() { return m_ReloadBundlesOn; }
  void SetReloadBundles(bool on) { m_ReloadBundlesOn = on; }
  bool IsPeople() { return m_PeopleOn; }
  void SetPeople(bool people) { m_PeopleOn = people; }
  bool IsRecordedOutfit() { return m_OutfitOn; }
  void SetRecordedOutfit(bool on) { m_OutfitOn = on; }
  bool IsMissionWorld();
  void SetMissionWorld(bool on);
  bool IsSaveRecordings() { return m_SaveOn; }
  void SetSaveRecordings(bool on) { m_SaveOn = on; }
  bool IsRecordDashes() { return m_DashAuto; }
  void SetRecordDashes(bool on) { m_DashAuto = on; }
  bool IsWorldDraws() { return m_WorldDrawsOn; }
  void SetWorldDraws(bool on) { m_WorldDrawsOn = on; }
  bool IsSkinnedTracks() { return m_TracksOn; }
  void SetSkinnedTracks(bool on) { m_TracksOn = on; }
  bool IsKeepingPeople() { return m_KeepPeople; }
  // Recording or playing
  bool IsBusy() const { return m_State != State_Idle; }
  void SetKeepingPeople(bool keep) { m_KeepPeople = keep; }

  // One person near her (see the .cpp; named here for its helpers there)
  struct PersonTrack;

private:
  enum State { State_Idle, State_Recording, State_Playing };

  void StartRecording();
  void StopRecording();
  void StartPlaying();
  void StopPlaying();
  void SampleInto(double time, float* bones);
  double Length() const { return m_Times.empty() ? 0.0 : m_Times.back(); }

  // The people near her (KrugerSec, civilians): recorded with her, at her
  // samples, and played on their own bodies the way hers is (see
  // CharacterBodyOffset)
  void ScanPeople(__int64 soldier, float const* faith);
  bool KnownBody(__int64 animatable, double& reused) const;
  void LogReusedBody(PersonTrack const& person, double reused) const;
  void RecordPeople(__int64 soldier, float const* faith, double time);
  void PlayPeople(double time, __int64 soldier, float const* faith);
  void ScanOthers(__int64 soldier, float const* faith, double time);
  // A recorded person no longer in the level matched to a live one of its kind
  // standing where it was then (see the .cpp); world: its bones 0 - 3
  bool ClaimForGone(__int64 entity, __int64 vtable, __int64 animatable, int bones, float const* world, int bodyOffset, double time);
  // Those still gone after that: played by live ones of their kind from
  // anywhere in the loaded world (see the .cpp)
  void BorrowForGone(__int64 soldier, float const* center, double time);
  void RetargetPerson(PersonTrack& person, __int64 entity, __int64 animatable);
  int m_Borrowed{ 0 };
  int m_Refound{ 0 };
  // Stand-ins: live ones the game's spawners make for the gone ones borrowing
  // can't give a body of their kind (see BorrowForGone, the hooks' AskStandIn)
  bool m_StandInsOn{ true };
  bool StandInsUsable() const;
  int m_StandIns{ 0 };        // asked in this playback
  int m_StandInsPlaying{ 0 }; // taken over by their recorded person
  void StandInPlaces(std::vector<float>& places) const;   // the made ones' (x y z each)
  void AskStandInFor(PersonTrack& person, int possible);
  void AskStandInsAhead(double time);
  // People drawn with the game's own draws of them (stage 2, ReplayGameDraws):
  // the draws kept while it records matched to the recorded people (by their
  // bones); while it plays, the gone ones posed from the recording
  void AdoptGameDraws();
  void DrawPeopleWithGameDraws(double time);
  bool DrawnByGameDraws(PersonTrack const& person) const;
  void LogGameDrawsMissing();
  // Guns, drones and recorded models with no shape kept (skinned doors) drawn
  // with the game's own skinned draws of them (stage 2b, see the .cpp): each
  // draw's bones a recorded bone of it (a door's: its transform) times a fixed
  // transform
  struct BoundDraw
  {
    int Id;
    __int64 Vb;
    unsigned StartIndex, IndexCount;
    int Header;                 // its header rows (float4s) before its bones
    std::vector<float> HeaderRows;
    std::vector<int> Bone;      // each of its bones: the recorded bone it follows
    std::vector<float> Fixed;   // each of its bones: the fixed transform (12 floats)
    double Clock;               // the recording's time it was kept at
    std::vector<float> Seen;    // its bones then (12 floats each), to check with a later draw of it
    bool Checked;               // fitted again with a later draw of it, it having moved in between
  };
  struct BoundOwner
  {
    PersonTrack* Person{ nullptr };   // a gun or drone (in m_People); null: a recorded model (a door)
    __int64 Entity{ 0 };
    std::string Label;
    std::vector<__int64> Vbs;   // its mesh's vertex buffers (empty: not read)
    int VbTries{ 0 };
    double NextVbTry{ 0.0 };    // the recording's time
    std::vector<BoundDraw> Draws;   // the set kept: one frame's, the most detailed seen
    unsigned long long Indices{ 0 };
    int Kept{ 0 };              // its draws that came (for the log)
    int Misfits{ 0 };           // its sets a later draw didn't fit
    bool Refused{ false };      // given up on (MaxMisfits of them)
    float Miss{ 1e30f };        // the closest a fit came (metres) when one didn't
  };
  std::vector<BoundOwner> m_Bound;
  std::set<__int64> m_BoundVbs;   // all their meshes' vertex buffers
  std::vector<__int64> m_WeaponVbs;   // the weapon meshes' (weapons/...) in the game's mesh table
  double m_NextWeaponScan{ 0.0 };     // the recording's time
  // A gun's or drone's draw whose mesh isn't known, near one: kept for a later
  // draw of it, the two fitted by how they moved (see AdoptGameDraws)
  std::vector<replay_gamedraws::SkinCandidate> m_SkinPool;
  void BindOwners(double time);
  bool OwnerRowsAt(BoundOwner const& owner, double time, bool playing, std::vector<float>& rows);
  void AdoptBound(std::vector<replay_gamedraws::SkinCandidate*>& rest, std::vector<std::pair<double, double>> const& frameDt);
  void ReleaseBound();
  // The start held for the stand-ins of the people there then (see the .cpp)
  void StartStandInWait();
  bool WaitingForStandIns();
  unsigned long long m_StandInWaitUntil{ 0 };   // GetTickCount64 (0: not holding)
  unsigned long long m_WaitScanAt{ 0 };
  int m_StandInsPending{ 0 };
  // The game's sound as recorded (see ReplayAudio.h): recorded with her, and
  // played in place of the game's
  bool m_SoundTrackOn{ true };
  bool m_SoundTrackPlaying{ false };
  // The recorded models the game doesn't have when it plays, drawn from their
  // kept shapes (see ReplayShapes.h)
  bool m_ModelShapesOn{ true };
  bool m_KeepWorldOn{ true };   // the mission's world kept once it's over (the hooks' "The recorded mission's world")
  bool m_GameDrawsOn{ true };   // the shapes drawn with the game's own draws (ReplayGameDraws)
  bool m_ReloadBundlesOn{ true }; // the recorded mission's bundles loaded again while it plays (ReplayBundles)
  // Saved recordings (ReplayStore.h, see the .cpp): every recording saved a
  // moment after it stops (m_SaveAt: NowSeconds then, -1 none), one loaded
  // from the menu at the next update (in place of the one there)
  bool m_SaveOn{ true };
  // Dashes recorded by themselves (see UpdateDashRecording): the option, the
  // recording that's a Dash's, when it stops (NowSeconds, -1 none), a start
  // waiting for the next update, the last of each kind of the game's Dash
  // messages (GetTickCount64, for the ones that come twice), and the name
  // the next save gets for its folder ("" the usual)
  bool m_DashAuto{ true };
  bool m_DashRecording{ false };
  double m_DashStopAt{ -1.0 };
  bool m_DashStartPending{ false };
  unsigned long long m_DashLastTick[4]{};
  std::string m_SaveWhere;
  void UpdateDashRecording(bool faithFound);
  double m_SaveAt{ -1.0 };
  // The world's own draws kept while it records and drawn where the game
  // doesn't (ReplayGameDraws' CaptureWorld): the option, and when the game's
  // mesh table is next read for them (NowSeconds)
  bool m_WorldDrawsOn{ true };
  double m_WorldScanAt{ -1.0 };
  // Other skinned draws' tracks (ReplayGameDraws' CaptureTrack): the option
  bool m_TracksOn{ true };
  bool m_LoadRequested{ false };
  std::string m_LoadFolder;
  std::string m_ConfirmDelete;        // the menu's delete waiting for its yes
  std::string m_RenameFolder;         // the menu's rename being typed ("" none)
  char m_RenameText[128]{};
  std::string m_LoadedFrom;           // the folder it was loaded from ("" made here)
  std::vector<replay_store::Listed> m_Listed;
  void SaveRecording();
  void ForgetRecording();
  int DropWorldNearPeople();
  int ThinTracksNearPeople(int& tracks, unsigned long long& bytes);
  void AnchorClothsToPeople();
  bool SeenAt(PersonTrack const& person, double time) const;
  bool ApplyLoaded(replay_store::Loaded& loaded, char* why, size_t whySize);
  void DrawStoreUI(bool& configChanged);
  void StopPeople();
  void ReleaseGunTail(bool now);
  void ClearPeople();
  void KeepPeople(bool faithThere); // what the game needs to leave them be (see the .cpp)
  void ShareGuardPlaces();
  void SetAnimationCulling(bool off);

private:
  State m_State{ State_Idle };
  bool m_Paused{ false };
  double m_PlaySpeed{ 1.0 };   // see SetPlaySpeed
  bool m_Loop{ false };
  bool m_WithTrack{ true };
  bool m_Sounds{ true };      // her sounds recorded and played along (util::hooks::ReplaySounds*)
  bool m_ModelsOn{ true };    // the models near her that move (doors, elevators) played along
  // The loading screens a recording crossed (a mission started or left, a fast
  // travel), on its clock, and what the game had after each (see
  // NoteRecordedLoads, WhereRecorded)
  struct RecordedLoad
  {
    double Start, End;        // End -1: still up when it stopped
    int OutfitAfter;          // faith_outfit::GameOutfit() once it was gone
    float TodAfter;           // the time of day then (-1: not known)
  };
  std::vector<RecordedLoad> m_Loads;
  bool m_LoadUp{ false };     // while it records
  bool m_StartAfterLoad{ true }; // playing starts where its last loading screen went
  char m_WorldMessage[512]{}; // the menu's line when its world isn't there (see PlayPeople)
  void NoteRecordedLoads();
  const char* WhereRecorded(char* buffer, size_t size) const;
  bool m_TodOn{ true };       // the time of day and weather as recorded while it plays (see TimeOfDayAt)
  bool m_TodApplied{ false }; // it's setting the clock now
  // The weather menu's settings when it recorded (VISUALS > Weather), and the
  // ones it had when the playback gave those (given back when it stops)
  weather::State m_RecordedWeather{};
  bool m_HaveWeather{ false };
  weather::State m_LiveWeather{};
  bool m_WeatherApplied{ false };
  void ApplyRecordedWeather(bool on);
  // The game's weather as it showed it (clouds and wind: see
  // VisualsManager::ReadWeatherLook), 4 samples a second
  std::vector<double> m_LookTimes;
  std::vector<BYTE> m_Looks;
  size_t m_LookSize{ 0 };
  bool m_LookApplied{ false };
  void ApplyRecordedLook(double time);   // time < 0: stops
  ReplayModels m_Models;
  bool m_PeopleOn{ true };    // the people near her played along (and the vehicles: KrugerSec VTOLs)
  // Between recording and playing, the recorded people kept in the level
  // (off: only while it records and while it plays; see KeepPeople)
  bool m_KeepPeople{ true };
  std::vector<std::unique_ptr<PersonTrack>> m_People;
  ReplayVehicles m_Vehicles;
  // The people near the replayed Faith who aren't in the recording: hidden
  // while it plays (see ParkPose)
  std::vector<std::unique_ptr<PersonTrack>> m_Others;
  // Guns whose update stays held a few frames after a playback, so the game's
  // pose puts their own back before they're let go (see StopPeople)
  struct GunTail
  {
    __int64 Body, Entity, VTable, Animatable;
    int Frames;
    float Real[3];   // where the game had it when its hold started
  };
  std::vector<GunTail> m_GunTail;
  double m_LastOthersScan{ -1.0 };
  int m_OthersHidden{ -1 };
  // Where the recorded people are, for keeping them in the level (x y z
  // each): their places last seen while nothing played (while it plays, their
  // bodies' places are the replay's)
  std::vector<float> m_GuardPlaces;
  double m_LastPeopleScan{ -1.0 };
  size_t m_PeopleBytes{ 0 };
  bool m_PeopleFull{ false };  // the memory for them ran out (logged once)
  double m_CommitCheckedAt{ -1.0 }; // when Windows' free memory was last looked at (see MinFreeCommit)
  bool m_ActorsWithoutBodyLogged{ false }; // guns or drones without a body seen (logged once a recording)
  bool m_PeopleLogged{ false }; // this playback's look at them is in the log
  double m_PeopleLogAt{ 0 };
  // Faith gone (dead, respawning) while the replay held people: when that
  // started (-1: she's there, or nothing was held then; see KeepPeople)
  double m_FaithGoneAt{ -1.0 };
  bool m_PeopleHeld{ false };   // last frame's hold
  bool m_GoneHoldEnded{ false }; // this time she's gone, the hold already ran out
  bool m_LoadingLetGo{ false };  // let go for a loading screen (see KeepPeople)
  int m_PeopleShown{ -1 }, m_PeopleGone{ -1 }, m_PeopleNoSlot{ -1 };
  bool m_CullingOff{ false };  // AntSettings' DisableModelAnimationCulling set by us
  unsigned char m_CullingOriginal{ 0 };
  bool m_TrackWasPlaying{ false };
  bool m_StartRequested{ false };
  bool m_StopRequested{ false };
  bool m_RecordRequested{ false };

  // The recording: bones per frame (8 floats each: translation x y z 1, then
  // a rotation quaternion, local to each bone's parent), a time per frame
  int m_Bones{ 0 };
  std::vector<double> m_Times;
  std::vector<float> m_Frames;
  // Her soldier at each sample: x y z, heading (NaN: not known)
  std::vector<float> m_FaithPlaces;
  // The game's time of day at each sample (seconds 0 - 86400, -1: not known)
  std::vector<float> m_Tods;
  float TimeOfDayAt(double time) const;
  // Her outfit (faith_outfit::WornOutfit) from each time it changed, and
  // whether the playback dresses her in it (see faith_outfit::SetReplayOutfit)
  std::vector<double> m_OutfitTimes;
  std::vector<int> m_Outfits;
  int OutfitAt(double time) const;
  bool m_OutfitOn{ true };
  bool m_OutfitApplied{ false };
  float m_PlayRoot[3]{};        // bone 1 of the pose played last (where she is: for a recording without places)
  bool m_PlayRootOk{ false };
  double m_RecordClock{ 0 };
  double m_LastSample{ -1 };
  double m_LastNow{ 0 };

  double m_PlayTime{ 0 };
  float m_SeekTo{ -1.f };    // from the menu's slider, done on the game thread

  // The slot our override was handed to (her pose is found again every frame)
  __int64* m_RegisteredOverrides{ nullptr };
  unsigned int m_RegisteredHandle{ 0xFFFFFFFF };
  bool m_Registered{ false };
  int m_Buffer{ 0 };          // which of the two overrides is next
  // The same for her box pose (the bones her bounding box is made of)
  __int64* m_BoxRegisteredOverrides{ nullptr };
  unsigned int m_BoxRegisteredHandle{ 0xFFFFFFFF };
  bool m_BoxRegistered{ false };
  int m_BoxBuffer{ 0 };
  bool m_BoxMissingLogged{ false };
  bool m_RootWriteFailed{ false }; // logged once
  char m_Message[512]{};      // why the last thing didn't work, for the menu (copied in with _TRUNCATE where long)

public:
  FaithReplay(FaithReplay const&) = delete;
  void operator=(FaithReplay const&) = delete;
};
