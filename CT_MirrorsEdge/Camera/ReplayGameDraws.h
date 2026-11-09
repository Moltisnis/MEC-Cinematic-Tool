#pragma once
#include <d3d11.h>
#include <functional>
#include <set>
#include <vector>
#include "ReplayShapes.h"

namespace replay_store { class Out; class In; struct SaveParts; struct LoadParts; }

// The recorded models drawn with the game's own draws (2026-10-27, stage 1;
// Research_Replay_Mission_World_2026-10-26.md sections 17 - 20). While the
// replay records, each part (a shape's section) of a recorded model has its
// draw in the game's G-buffer pass kept: the game's shaders, buffers,
// textures, samplers and states, and its constants and instance data (read
// back from the graphics card). While it plays, the parts of the models the
// game doesn't have any more are drawn again with those, at their recorded
// places, at the end of the game's G-buffer pass, so the game lights them
// as its own. See the .cpp.
namespace replay_gamedraws
{
  // Render thread, once (with the replay's draw hooks): the immediate
  // context's render target changes hooked
  void Hook(ID3D11DeviceContext* pImmediate);
  // Render thread, each Present: the frame's size; captures read back
  void Present(IDXGISwapChain* pSwapChain, ID3D11DeviceContext* pContext);

  // The recording's cost by part, in its log line every 10 s (see the .cpp's
  // LogCost; 2026-11-04, the user: "find out why fps drop when recording a
  // replay and fix the fps issue"): a part's time added from any thread
  // (QueryPerformanceCounter ticks); CostTimer adds its scope's
  enum CostPart
  {
    Cost_Captures,        // the game's draws kept by the draw hooks (in the hooks' time)
    Cost_AssetDraws,      // the asset log's sampled draws (in the hooks' time)
    Cost_DrawLogDraws,    // the draw log's sampled draws (in the hooks' time)
    Cost_Copies,          // the kept draws' buffers copied at their passes' ends
    Cost_Staging,         // staging buffers made (in the copies' time)
    Cost_ReadBack,        // the copies read back and decoded
    Cost_Maps,            // their maps and copies out (in the read back's time)
    Cost_Crowds,          // the crowds' copies read
    Cost_AssetPresent,    // the asset log's frame handed over
    Cost_DrawLogPresent,  // the draw log's frame
    CostParts
  };
  void AddCost(int part, long long ticks);
  struct CostTimer
  {
    int Part;
    LARGE_INTEGER From;
    explicit CostTimer(int part) : Part(part)
    {
      QueryPerformanceCounter(&From);
    }
    ~CostTimer()
    {
      LARGE_INTEGER to;
      QueryPerformanceCounter(&to);
      AddCost(Part, to.QuadPart - From.QuadPart);
    }
  };

  // Game thread: the replay records (its shapes are being captured) or not
  void SetCapturing(bool on);
  // The option (REPLAY > the game's own drawing)
  void SetEnabled(bool on);
  // Game thread: a new recording, or forgotten
  void Clear();
  // Any thread: this frame's models the game doesn't have (as the shapes get them)
  void SetDraws(replay_shapes::Draw const* draws, int count);

  // From the replay's draw hooks (render thread): Bypass while the tools
  // draw their own (their draws aren't looked at); OnDraw for every draw of
  // the game's; Capturing: a recorded model's draw now would be kept
  bool Bypass();
  void OnDraw(ID3D11DeviceContext* pContext);
  bool Capturing();
  void CaptureDraw(ID3D11DeviceContext* pContext, int shape, int section, char const* name, UINT indexCount, UINT instances,
    UINT startIndex, INT baseVertex);
  // Render thread: the pass bound now (0 the G-buffer's, 1 its decals', 2 the
  // lit picture's, 3 a half-size transparent one, 5 the runner's vision
  // layer's, -1 none the replay keeps draws of; see the .cpp's Pass)
  int PassNow();
  // Render thread, at Present after the replay's shapes: the recorded Dash
  // markers drawn into the replay's runner's vision mask this frame (at the
  // end of the game's G-buffer pass) mixed into the picture as the game's
  // tonemap mixes its own (see the .cpp's Pass_Rv)
  void ApplyRunnersVision(IDXGISwapChain* pSwapChain, ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
  // Game thread, while it plays: whether the game draws a recorded model's
  // mesh (its short name) at a place now: 1 one of its instances read back
  // lately within ModelDrawnMetres, 0 not (its mesh not drawn lately, or its
  // instances read back lately and none near), -1 not known
  // (within: the reach in metres, < 0 ModelDrawnMetres; a Dash's marker's is
  // wider, the game draws one with its part's offset)
  int GameDrawsModel(std::string const& mesh, float const* place, float within = -1.f);
  // Render thread, while it records: the pass bound now when it's a kept
  // kind's past the ones the replay keeps (its kind x 1000 + its place among
  // that kind's in the frame), else -1 (for the log: see replay_shapes)
  int PastKeptPass();
  // Render thread, from the draw hooks while it plays: a draw of a mesh the
  // world's kept draws have, its instances read back now and then (see the
  // .cpp's ProbeEvery)
  bool Probing();
  void ProbeDraw(ID3D11DeviceContext* pContext, UINT indexCount, UINT instances, UINT startIndex, INT baseVertex);

  // Render thread, from the draw hooks: a draw now is in the game's main
  // G-buffer pass, on its immediate context (ReplayAssetLog)
  bool InMainPass(ID3D11DeviceContext* pContext);
  // Render thread, at Present (replay_shapes::Apply): the parts drawn in the
  // frame ended now (shape << 16 | section), taken
  void TakeDrawn(std::set<long long>& out);
  // The parts kept so far (ready to draw), for the menu
  int Ready();

  // People (stage 2). While the replay records, every SkinCaptureEvery frames
  // the game's draws in its G-buffer pass whose vertex shader reads bone
  // matrices are kept the same way, each instance read back as a candidate
  // for FaithReplay to match to a recorded person (by its bones).
  // SetClock: the recording's time now (game thread), stamped on them.
  // CapturingSkins / CaptureSkin: from the replay's draw hooks.
  void SetClock(double recordTime);
  bool CapturingSkins();
  void CaptureSkin(ID3D11DeviceContext* pContext, UINT indexCount, UINT instances, UINT startIndex, INT baseVertex);
  struct SkinCandidate
  {
    int Id;                  // its draw's id (ReleaseSkin when it isn't kept)
    double Clock;            // the recording's time when it was drawn (the frame drawn may be a little older)
    __int64 Vb;              // the game's vertex buffer
    UINT StartIndex, IndexCount;
    UINT Stride;             // float4s an instance
    std::vector<float> Instance;   // Stride x 4 floats: its header rows, then its bones' 3x4 matrices
    int PassKey = 0;         // 0: the G-buffer pass's; else its pass and its place in the frame (the lit pass's: see-through parts)
  };
  // Game thread: the candidates read back since the last call
  void TakeSkinCandidates(std::vector<SkinCandidate>& out);
  // The video screens (see the .cpp): the billboard manager's table of its
  // channels' textures now (its job's thread, each frame; the hooks'
  // hBillboardSizes), and the screen size each channel's screens the replay
  // draws would have from this camera (its place; 0: none drawn lately)
  void NoteFeedTextures(__int64 table);
  void FeedSizes(float const* camera, float* sizes);
  // Game thread, once it stopped recording: the times (the recording's, s) the
  // game drew a recorded model at this place (an instance of its shape within
  // ownMetres; OwnMain: in the G-buffer pass's draws at least once), and the
  // times it drew another model within nearMetres of it and not it (see the
  // .cpp's Sighting)
  struct Seen
  {
    std::vector<float> Own, Near;
    bool OwnMain = false;
  };
  void SightingsAt(int shape, float const* place, float ownMetres, float nearMetres, Seen& out);
  // The same: each time the game drew a shape (the recording's time, whether
  // in the G-buffer pass, its instances' places: 3 floats each)
  struct ShapeSighting
  {
    float Clock = 0.f;
    bool Main = false;
    std::vector<float> Places;
  };
  void SightingsOf(int shape, std::vector<ShapeSighting>& out);
  // Game thread, once it stopped recording: the times (the recording's, s)
  // the place was in the main view's picture no further than maxMetres from
  // its camera (none when which way its matrix goes isn't clear, see the
  // .cpp's ViewSample); and how far the camera was from it at a time (-1: no
  // view kept within 0.25 s of it)
  void InViewTimes(float const* place, float maxMetres, std::vector<float>& times);
  float CameraDistanceAt(double clock, float const* place);
  void ReleaseSkin(int id);
  // Game thread (stage 2b): the vertex buffers of the recorded guns', drones'
  // and doors' meshes (the game's ID3D11Buffer addresses); their draws are
  // kept more often than the others (every WantedCaptureEvery frames)
  void SetWantedSkins(std::vector<__int64> const& vbs);
  // Game thread, each frame while it plays: the people drawn this frame, each
  // a kept draw with its instance (the header rows as kept, the bones posed)
  struct PersonDraw
  {
    int Id;
    std::vector<float> Instance;
  };
  void SetPeopleDraws(std::vector<PersonDraw> const& draws);

  // The world's own draws (see the .cpp, WorldCaptureEvery): a mission's
  // models the replay doesn't record as models, for a saved recording played
  // in a later session. CapturingWorld / CaptureWorld: from the replay's draw
  // hooks while it records (SetWorldCapture). UpdateWorld: game thread, about
  // once a second while it records (each kept one's mesh, only a mesh's most
  // detailed level, none of a recorded model's) and while it plays (the
  // game's buffers of those meshes, for its draws of them to be known).
  // SetWorldCenter: each frame while it plays, the replayed Faith (null:
  // nothing plays). SetWorldOn: the option.
  bool CapturingWorld();
  void CaptureWorld(ID3D11DeviceContext* pContext, UINT indexCount, UINT instances, UINT startIndex, INT baseVertex);
  void SetWorldCapture(bool on);
  void SetWorldOn(bool on);
  void SetWorldCenter(float const* place);
  void UpdateWorld(bool recording, bool playing);
  void WorldCounts(int& kept, int& ready);
  // Game thread, once it stops recording: the kept ones the test says aren't
  // the world's (near someone recorded when they were kept: a part of theirs
  // drawn without bones) let go; how many
  int DropWorld(std::function<bool(double clock, float const* center, float radius)> const& test);
  // Texture slots of kept draws taken anew from the game's better ones while
  // it recorded (see the .cpp's Upgrade), for the log
  int Upgraded();

  // Other skinned draws' tracks (see the .cpp, TrackSampleSeconds): drones,
  // crowds, anything skinned the replay doesn't record as a person, gun or
  // drone. CapturingTracks / CaptureTrack: from the replay's draw hooks while
  // it records (SetTrackCapture). SetPlayTime: each frame while it plays, the
  // time played (-1: nothing plays). SetTracksOn: the option. ThinTracks:
  // game thread, once it stops recording, the instances theirs says are a
  // recorded person's, gun's, drone's or Faith's (drawn otherwise) let go;
  // how many (and the tracks and bytes left). theirs: 0 nobody's, 1 a
  // recorded person's (or gun's, drone's), 2 Faith's. A cloth's track (see
  // the .cpp's MaxClothBytes) is kept only when it's a recorded person's (not
  // saved). SaveTracks / LoadTracks: saved recordings, like Save / Load.
  bool CapturingTracks();
  void CaptureTrack(ID3D11DeviceContext* pContext, UINT indexCount, UINT instances, UINT startIndex, INT baseVertex);
  void SetTrackCapture(bool on);
  void SetTracksOn(bool on);
  void SetPlayTime(double time);
  int ThinTracks(std::function<int(double clock, float const* place)> const& theirs, int& tracks, unsigned long long& bytes);
  // The cloths kept (see the .cpp's AnchorCloths). AnchorCloths: game thread,
  // after ThinTracks: each one's owner and that owner's bone nearest it
  // (nearest: the recorded person drawn near a place at a time, and their
  // bone nearest it), and that bone's pose at each of its samples (boneAt: 12
  // floats, rows of its turn with its place last); how many have one.
  // ClothAnchorsWanted / SetClothAnchors: game thread, each frame while it
  // plays: the owners' bones wanted, and their poses now (only the people
  // drawn now; the others' cloths aren't drawn from a carried sample)
  struct ClothAnchor
  {
    __int64 Owner;
    int Bone;
    float Pose[12];
  };
  int AnchorCloths(std::function<bool(double clock, float const* place, __int64& owner, int& bone)> const& nearest,
    std::function<bool(__int64 owner, int bone, double clock, float* pose)> const& boneAt);
  void ClothAnchorsWanted(std::vector<std::pair<__int64, int>>& out);
  void SetClothAnchors(std::vector<ClothAnchor> const& anchors);
  int SaveTracks(replay_store::Out& out, replay_store::SaveParts& parts, int& unsaved);
  int LoadTracks(replay_store::In& in, replay_store::LoadParts const& parts, int& dropped);

  // Crowds (see the .cpp): the crowd system's draws (people, cars, drones,
  // boats of the city's swarms), whose members are read through their
  // indirection and transform buffers. CapturingCrowds / CaptureCrowd: from
  // the replay's draw hooks while it records (with the tracks:
  // SetTrackCapture, SetTracksOn). SkipGameDraw: any thread, from the draw
  // hooks: a game's draw left out (its crowds while the recorded ones play).
  // CrowdCounts: kinds kept, samples, draws whose members couldn't be read,
  // bytes. SaveCrowds / LoadCrowds: saved recordings (their own section).
  bool CapturingCrowds();
  void CaptureCrowd(ID3D11DeviceContext* pContext, UINT indexCount, UINT instances, UINT startIndex, INT baseVertex);
  bool SkipGameDraw(ID3D11DeviceContext* pContext);
  void CrowdCounts(int& kinds, int& samples, int& bad, unsigned long long& bytes);
  int SaveCrowds(replay_store::Out& out, replay_store::SaveParts& parts, int& unsaved);
  int LoadCrowds(replay_store::In& in, replay_store::LoadParts const& parts, int& dropped);

  // Saved recordings (ReplayStore.h): every model's kept draw and the
  // people's with these ids written with their parts (how many; unsaved: the
  // ones that couldn't be, a part of theirs can't be saved), and read back in
  // place of these (how many, -1 if they couldn't be read; dropped: a part of
  // theirs not made again)
  int Save(replay_store::Out& out, replay_store::SaveParts& parts, std::set<int> const& skins, int& unsaved);
  int Load(replay_store::In& in, replay_store::LoadParts const& parts, int& dropped);
}
