#include "ReplayGameDraws.h"
#include "ReplayAssetLog.h"
#include "ReplayDrawLog.h"
#include "ReplayStore.h"
#include "../Main.h"
#include "../Visuals/GameFrame.h"
#include "../Visuals/D3DStateBackup.h"
#include "../Util/Util.h"
#include "RunnersVisionVs.h"
#include "RunnersVisionPs.h"
#include <Windows.h>
#include <d3d11_1.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <functional>
#include <cmath>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

// What the draw log found (2026-10-27 15:59 and 16:09 tests, Research_Replay_
// Mission_World_2026-10-26.md sections 19 - 20):
// - The game draws its G-buffer on the immediate context: 4 targets
//   (R10G10B10A2_UNORM, R8G8B8A8_UNORM_SRGB, R8G8B8A8_UNORM, R11G11B10_FLOAT)
//   and a D32 / S8 depth target the frame's size (a 512 x 512 one is its
//   reflections'), depth GREATER_EQUAL with writes, stencil REPLACE.
// - A mesh's draw takes b0 functionConstants (its dxVectorBufferOffsets:
//   start, stride, start, stride in float4s) and b1 externalConstants (the
//   material's) as windows (D3D 11.1 offsets) into a pool of 1 MB dynamic
//   buffers, b2 viewConstants (the camera) from a 1136 B one, and its
//   instances from t0 instanceVectorBuffer (1 MB dynamic, float4s): a
//   model's world matrix is 3 of an instance's rows (rotation in xyz,
//   translation in w; which rows is the shader's: rows 2 - 4 of 5 for the
//   commonest).
// So, while the replay records (Capturing), the first draw of each part of a
// recorded model in the G-buffer pass (a shape's section, by its vertex
// buffer, first index and count: replay_shapes) is kept as a recipe: its
// shaders, input layout, vertex and index buffers, views, samplers and
// states held (references: they stay valid once the game lets them go), and
// where its b0 / b1 windows, the camera and its instances were. At the end of
// that pass (the next render target change, hooked: OMSetRenderTargets, slot
// 33, and its UAV form, slot 34) the buffers they're in are copied on the
// graphics card into staging buffers (CopyResource; the pool's windows aren't
// written over within a pass) and read back ReadbackAfterFrames later: the
// windows' bytes, the instances (start + instance x stride), and which rows
// are the world matrix (3 rows whose xyz columns are at right angles, their
// place within MaxWorldFromCamera of the camera). A recipe whose data can't be
// read is tried again (MaxAttempts).
// While the replay plays, at the end of the game's G-buffer pass (its targets
// still bound), the parts of the models the game doesn't have (SetDraws, the
// shapes' list) are drawn with their recipes: the game's shaders, buffers,
// views and states, the tools' own b0 (the recipe's, its offsets pointing
// into the tools' instance buffer), b1 (a copy), b2 (this frame's camera,
// copied at the pass's first draw) and t0 (each model's instance as recorded,
// picked by its place, with its recorded transform written into the world
// rows), as one instanced draw per recipe. Every state changed is put back
// as it was (the constant buffers with their 11.1 offsets). The game then
// lights, fogs and reflects them as its own. Those parts aren't drawn as
// shapes then (TakeDrawn). Not drawn this way: the shadows they'd cast,
// their motion (a moving one's), see-through parts, parts the game drew only
// in other passes, and shaders that take their world matrix from b0 (not
// instanced). Nothing of the game's is written: D3D calls on its context,
// the tools' own buffers.
namespace
{
  const DXGI_FORMAT GbufferFormats[4] = { DXGI_FORMAT_R10G10B10A2_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_FORMAT_R8G8B8A8_UNORM,
    DXGI_FORMAT_R11G11B10_FLOAT };
  const int OmSetRenderTargetsSlot = 33, OmSetRenderTargetsUavSlot = 34;
  const int MaxRecipes = 1500;
  const int MaxCapturesPerPass = 48;
  const size_t MaxStagingBytes = 64u << 20;
  const unsigned ReadbackAfterFrames = 3, ReadbackGiveUpFrames = 120;
  const int MaxAttempts = 3;
  const UINT MaxWindowBytes = 4096;
  const UINT MaxStride = 512;           // float4s an instance
  const UINT MaxInstancesKept = 256;
  const UINT MaxInstancesDrawn = 16384;
  const float MaxWorldFromCamera = 600.f;
  const float InstanceMatchMetres = 2.f;
  // A model that didn't move: its own instance in a kept draw is where it
  // stood, this near (2026-11-01 19:04 test: the gate's sign planes, which the
  // game never drew, counted as drawn by the bus sign plane of the same prefab
  // within 2 m)
  const float StillMatchMetres = 0.3f;
  // A Dash's marker (replay_shapes::Draw::RvLayer): its own instance within
  // RvMatchMetres (the game draws one with its part's offset: dash_arrowmedium's
  // part is 1.14 m from its model's origin; ReplayModels' RvSeenMetres)
  const float RvMatchMetres = 3.f;
  const int CameraPosOffset = 720;      // viewConstants
  const int CbSlots = D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT;   // 14
  const int VsViewSlots = 8, PsViewSlots = 32, VsSamplerSlots = 4, PsSamplerSlots = 16, VbSlots = 4;
  const int MaxRecipeLogs = MaxRecipes, MaxFailLogs = 20;   // every recipe kept is in the log (the user, 2026-10-27)
  // People: their draws kept every SkinCaptureEvery frames while it records
  const unsigned SkinCaptureEvery = 30;
  const int MaxSkinCapturesPerPass = 64;
  const int MaxSkins = 1024;            // kept draws of people, waiting to be matched or matched
  const UINT MaxSkinInstances = 16;
  // Guns', drones' and doors' meshes (stage 2b, SetWantedSkins): their draws
  // kept every WantedCaptureEvery frames, MaxWantedCapturesPerPass on top
  const unsigned WantedCaptureEvery = 10;
  const int MaxWantedCapturesPerPass = 32;
  // The world's own draws (2026-10-27; the user, after a saved recording was
  // loaded in a new session: "the ksec gates are still only the doors part"):
  // a mission's models the replay doesn't record as models (its model groups'
  // members: the gates' barriers and pillars) aren't in a later session. While
  // it records, the G-buffer pass's draws that aren't a recorded model's or
  // person's are kept every WorldCaptureEvery frames (MaxWorldCapturesPerPass
  // a pass), with their instances within WorldKeepMetres of the camera; each
  // mesh at the most detailed level seen. While it plays, each is drawn as
  // kept where the replayed Faith is within WorldDrawMetres of it, unless the
  // game drew its mesh within the last WorldGameDrewFrames frames (then it's
  // the game's own world there).
  const unsigned WorldCaptureEvery = 20;
  const int MaxWorldCapturesPerPass = 24;
  const int MaxWorld = 4000;
  const float WorldKeepMetres = 100.f;
  const float WorldDrawMetres = 150.f;
  const unsigned WorldGameDrewFrames = 60;
  const UINT MaxWorldInstancesDrawn = 8192 * 16;   // float4s, of MaxInstancesDrawn's
  // A kept model's draw has its textures taken anew from the game's every
  // UpgradeEvery frames while it records (see Upgrade)
  const unsigned UpgradeEvery = 30;
  // Mesh variations (2026-10-27, the game's data: Release's MeshVariationDb
  // has door_breachsingle_01 in 3 variations, door_glitchkey_oneway_right's red
  // one; each a material of its own, drawn as a draw of its own). A kept
  // model's or world part's draw is kept again from the game's later draws of
  // it (a candidate, every UpgradeEvery frames, the world's every
  // WorldCaptureEvery): one with an instance within VariantSameMetres of a kept
  // one's is the same placed objects (its instances it hadn't added, its
  // bigger textures taken, with the same shaders), and so is one with the same
  // shaders and textures wherever it is (SameLook); any other is another
  // variation, kept as its own (MaxVariants a part). A model is drawn with
  // the kept one that has its own instance.
  const float VariantSameMetres = 0.3f;
  const int MaxVariants = 32;
  // A Dash's markers (the runner's vision layer, see Pass_Rv): each one's
  // draw kept as a variation of its own, its own material values with it
  // (2026-11-08: the arrows share one mesh and look, so the later arrows'
  // draws were merged into the first one's, drawn with that one's values: a
  // marker's own brightness, its pixel shader's b1 x, as the Dash had it);
  // a Dash's arrows can be more than MaxVariants (bits 40 - 47 of the key)
  const int MaxRvVariants = 200;
  // Other skinned draws (2026-10-27 22:04; the user: "hunter drone didn't
  // had any model": the security drone, dronesecurityskinned, 27 - 67 s, isn't
  // one the replay records as a drone, nor are the crowds): while it records,
  // every draw in the G-buffer pass whose vertex shader reads bones is kept
  // as a track: its state once, its instances (their bones) each
  // TrackSampleSeconds of the recording. While it plays, each track's
  // instances are drawn in a straight line between the samples around the
  // time (an instance matched to the next sample's nearest, its first bone
  // within TrackSameMetres), but not in a gap longer than TrackHoldSeconds (not
  // drawn then), not Faith's (the game draws her), and not one within
  // TrackPosedMetres of a recorded person's, gun's or drone's posed draw of
  // the same mesh (those are drawn as posed).
  const double TrackSampleSeconds = 0.1;
  const int MaxTracks = 512;
  const size_t MaxTrackBytes = 512u << 20;
  const int MaxTrackCapturesPerPass = 128;
  const double TrackHoldSeconds = 0.6;
  const float TrackSameMetres = 1.5f;
  const float TrackPosedMetres = 0.5f;
  const float TrackDrawMetres = 250.f;   // from the replayed Faith
  const int MaxHeaderRows = 32;
  // The passes kept (2026-10-27 22:55; the user: the KrugerSec doors "half of
  // the mesh is missing", "alphas on doors are still wrong": a door's screen
  // and decals are drawn after the G-buffer pass, in its decal pass (the
  // G-buffer's first three targets) and its lit picture's (one RGBA16F target
  // with the G-buffer's depth): the draw log saw door_glitchkey's there). A
  // recorded model's or the world's draw in those is kept with its pass and
  // its place among that pass's kind in the frame (after the G-buffer pass),
  // and drawn at the end of the same one while it plays. Shadows aren't kept.
  // 2026-10-28: and the half-size transparent passes (one RGBA16F target with
  // a depth target half the picture's size: 6 - 7 draws a frame in the draw
  // log). The Release gate's glass (csg_releasegateglass_01, a static model
  // with a glass material) was never drawn in the G-buffer pass nor kept from
  // the others; where the game draws it the asset log now says.
  // 2026-11-01 (the user: "meshes and models shouldn't disappear because
  // faith wasn't looking at them"): the reflections' G-buffer passes (the
  // game's cubemap around the camera, its faces in turn: the G-buffer's 4
  // targets at another size, 512 x 512) see what's beside and behind the
  // camera; the 10-28 logs had the Release gate's doors and frame drawn there.
  // A recorded model's or the world's draw there is kept as if it were the
  // G-buffer pass's (drawn in that pass when it plays), only for a part none
  // of the G-buffer pass's draws gave yet, and that pass's own draw of it
  // takes its place later (FromEnv). Nothing is drawn into those passes.
  // 2026-11-08 (the user: "Replay arrow/starting are wrongly textured /
  // missing model(?) finish line is missing"): the runner's vision layer. The
  // Dash's start, checkpoint arrows and finish (PrimaryInRVLayer True in
  // their PamStaticModelEntityData) are drawn by the game only into its
  // runner's vision mask: one R8_UNORM target with the scene's depth (the
  // draw log: dash_arrowmedium and dash_end there, depth test, no writes;
  // every such pass in the logs was theirs), after its decal pass, cleared
  // first (0x143743A10). Its tonemap pass mixes that mask into the picture in
  // red (runnersVisionAlphaMaskTexture, runnersVisionColor; 0x1435CA23E), and
  // the game makes the pass and that mix only in a frame with something in
  // that layer ([draw lists + 0x100], the same three checks before both,
  // 0x14374697D: WorldRenderSettings.RunnersVisionEnable at + 0x450 too). So
  // while it records a recorded model's draw in that pass is kept like the
  // other passes', and while it plays the replay draws the kept ones into a
  // mask of its own at the end of the game's G-buffer pass (DrawRvLayer) and
  // mixes it in at Present as the tonemap would (ApplyRunnersVision); nothing
  // is drawn into the game's own mask.
  enum Pass { Pass_None = -1, Pass_Main = 0, Pass_Decal = 1, Pass_Lit = 2, Pass_Half = 3, Pass_Env = 4, Pass_Rv = 5, Passes = 6 };
  // 2026-10-28: 8 until then. The Release gate's glass is drawn in the lit
  // picture's pass (the asset log: csg_releasegateglass_01, one RGBA16F
  // target with the G-buffer's depth) but was never kept: no try at all in
  // the log while it was in sight, so it's taken to be drawn past the 8th
  // such pass of a frame (the transparent ones come after the lighting's).
  // 15 from 2026-10-28 02:51: the glass was kept then, and the log counted
  // up to 17 lit picture's passes in a frame. 24 now: a world key holds its
  // pass in 7 bits from bit 25 (WorldPassShift), its first index below.
  const int MaxPassOrdinals = 24;
  const int WorldPassShift = 25;
  // The game's own instances of a kept world mesh, read back each ProbeEvery
  // frames while it plays (MaxProbesPerPass a pass): a kept one of the world
  // where the game draws one within ProbeSameMetres isn't drawn (2026-10-27:
  // all of a mesh's were left out whenever the game drew any of it)
  const unsigned ProbeEvery = 30;
  const int MaxProbesPerPass = 16;
  // Every draw of a mesh in the frame it's probed in is read back, and their
  // places go together (2026-11-03: only its first draw was, and its places
  // replaced the mesh's). Where the game draws a mesh in more than one draw a
  // frame (its instances in more than one batch), the recorded models in the
  // others counted as not drawn, and the replay drew one-colour copies over
  // the game's own across the view: the mission test's 21:40 run, items 17,
  // 18, 20 and 22 (billboard_01_1024x1536, which the asset log has the game
  // drawing at all 20 seconds the replay drew its copy; aircon08_s_160x40_b;
  // an industrial wall). A mesh's later draws are read back up to this many a
  // pass in all; a frame with one of them left out has its places dropped
  const int MaxProbesPerPassAll = 64;
  const float ProbeSameMetres = 0.5f;
  const unsigned ProbeFreshFrames = 150;
  const unsigned TimingLogFrames = 600;  // the replay's own drawing's time logged each this many frames it draws

  struct Window
  {
    __int64 Page;     // the buffer (its address: the page is held by the recipe)
    UINT First, Num;  // 16-byte constants
  };

  struct Recipe
  {
    int Shape = -1, Section = -1;
    bool Skin = false;     // a person's (stage 2): matched by FaithReplay, drawn with the bones it gives
    bool World = false;    // the world's own (CaptureWorld): drawn as kept, where the game doesn't draw its mesh
    bool Track = false;    // a sample of another skinned draw's track (CaptureTrack; its key in WorldKey)
    bool SampleOnly = false;   // a track's later sample: only its instances are read (no constant buffers made)
    bool Probe = false;    // the game's own instances of a kept world mesh read back (ProbeEvery)
    bool ProbeOther = false;   // of a probe: from its decal, lit or half-size passes (see g_GamePlacesOther)
    bool Candidate = false;    // a later draw of a kept one's part (see VariantSameMetres)
    bool FromEnv = false;      // kept from a reflection's G-buffer pass (see Pass_Env)
    int Pass = Pass_Main;  // the pass it was kept in, and its place among that pass's kind in the frame
    int PassOrdinal = 0;
    std::tuple<__int64, UINT, UINT> WorldKey{};   // a world one's: the game's vertex buffer, first index, index count
    std::string Mesh;      // a world one's mesh (its full name) and level, once known (UpdateWorld)
    int Lod = -1;
    int MeshId = -1;
    float Center[3] = {};  // a world one's instances: their middle, and how far from it they reach
    float Radius = 0.f;    // (their mesh's reach, MeshReach, included)
    float MeshReach = 0.f; // a world one's: how far its mesh reaches from an instance's place, scaled (see KeepNear)
    double Clock = 0.0;    // the recording's time when it was kept
    char Name[64] = "";
    ComPtr<ID3D11VertexShader> Vs;
    ComPtr<ID3D11PixelShader> Ps;
    ComPtr<ID3D11GeometryShader> Gs;
    ComPtr<ID3D11HullShader> Hs;
    ComPtr<ID3D11DomainShader> Ds;
    ComPtr<ID3D11InputLayout> Layout;
    D3D11_PRIMITIVE_TOPOLOGY Topology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
    ComPtr<ID3D11Buffer> Vbs[VbSlots];
    UINT Strides[VbSlots] = {}, Offsets[VbSlots] = {};
    ComPtr<ID3D11Buffer> Ib;
    DXGI_FORMAT IbFormat = DXGI_FORMAT_UNKNOWN;
    UINT IbOffset = 0;
    UINT IndexCount = 0, StartIndex = 0, Instances = 0;
    INT BaseVertex = 0;
    ComPtr<ID3D11ShaderResourceView> VsViews[VsViewSlots], PsViews[PsViewSlots];
    bool VsInstanceSlot[VsViewSlots] = {}, PsInstanceSlot[PsViewSlots] = {};
    // Its video screen slots (see NoteFeedTextures): 0 none, else 1 + the
    // channel x 2 + which of the channel's two textures; HasFeed: any
    unsigned char Feed[PsViewSlots] = {};
    bool HasFeed = false;
    ComPtr<ID3D11SamplerState> VsSamplers[VsSamplerSlots], PsSamplers[PsSamplerSlots];
    ComPtr<ID3D11DepthStencilState> Depth;
    UINT StencilRef = 0;
    ComPtr<ID3D11BlendState> Blend;
    FLOAT BlendFactor[4] = {};
    UINT SampleMask = 0xFFFFFFFF;
    ComPtr<ID3D11RasterizerState> Raster;
    // Where its data was
    Window VsB0{}, VsB1{}, PsB0{}, PsB1{};
    ComPtr<ID3D11Buffer> Pages[6];   // held until read back: VS b0, VS b1, PS b0, PS b1, the instances, a cloth's vertices
    // A cloth's draw (see ClothSlotOf): the vertex buffer slot the graphics
    // card writes, that buffer (the game's, as kept), and while it plays the
    // tools' own buffer its vertices are put in each frame
    bool Cloth = false;
    int ClothSlot = -1;
    __int64 ClothSource = 0;
    ComPtr<ID3D11Buffer> ClothVb;
    UINT ClothVbBytes = 0;
    // A recorded model's material values over time (see NoteMaterial): its
    // b1s (externalConstants, what the game's entities set on its material)
    // at later draws of the same objects while it recorded, the recording's
    // time of each and the VS b1's bytes then the PS b1's; while it plays the
    // tools' own b1s hold them (VsB1Live, PsB1Live)
    std::vector<float> B1Times;
    std::vector<std::vector<BYTE>> B1Samples;
    float B1SameUntil = -1.f;   // the last later draw that had the last sample's values
    ComPtr<ID3D11Buffer> VsB1Live, PsB1Live;
    __int64 InstancePage = 0;
    UINT InstanceFirst = 0;
    int VsOffsetsAt = -1, PsOffsetsAt = -1;
    // Read back
    bool Ready = false;
    std::vector<BYTE> VsB0Bytes, PsB0Bytes;
    std::vector<BYTE> VsB1Bytes, PsB1Bytes;   // for a saved recording (ReplayStore.h)
    ComPtr<ID3D11Buffer> VsB1Copy, PsB1Copy;
    UINT Stride = 0;
    UINT Kept = 0;
    std::vector<float> InstanceData;   // Kept x Stride x 4
    std::vector<int> WorldRows;
    std::unordered_map<__int64, std::pair<int, float>> ByEntity;   // render thread: entity -> its instance and how far (squared)
    // Playback: its own b0s
    ComPtr<ID3D11Buffer> VsB0Own, PsB0Own;
  };

  // A staging buffer's bytes where it's mapped (see ReadBack)
  struct MappedBytes
  {
    BYTE const* Data = nullptr;
    size_t Size = 0;
    BYTE const* data() const
    {
      return Data;
    }
    size_t size() const
    {
      return Size;
    }
    BYTE const* begin() const
    {
      return Data;
    }
    BYTE const* end() const
    {
      return Data + Size;
    }
  };

  struct Staged
  {
    __int64 Source;
    ComPtr<ID3D11Buffer> Staging;
    UINT Width;
    bool Read;
    MappedBytes Bytes;   // while its batch is read back (ReadBack): Width bytes, mapped
    UINT Capacity = 0;   // the staging buffer's own size (0: Width)
    bool Mapped = false;
  };

  // Staging buffers kept for the next copies (render thread; 2026-11-01, the
  // user: "game lags/drop fps when recording a replay": each copy made a new
  // one, a driver allocation at every capture of every kind), by their size
  std::multimap<UINT, ComPtr<ID3D11Buffer>> g_StagingPool;
  size_t g_StagingPoolBytes = 0;
  const size_t MaxStagingPoolBytes = 128u << 20;
  // The recording's cost, for the log (render thread; see Present)
  long long g_PresentTicks = 0;
  unsigned long long g_StagedBytesNow = 0;
  unsigned g_CostFrames = 0;
  ULONGLONG g_CostSince = 0;
  int g_StagingMade = 0, g_StagingReused = 0;

  // A staging buffer of at least bytes (one kept if there's one at most twice
  // that size or the size a new one would have, else a new one rounded up to
  // a power of two, 4096 bytes at least); width: its size. 2026-11-04: the
  // camera's 1136-byte copy, one in every batch, never took the 4096-byte
  // ones kept (more than twice its size): 30 - 50 new ones a second while it
  // recorded, each one kept after, filling the pool
  ComPtr<ID3D11Buffer> TakeStaging(ID3D11Device* device, UINT bytes, UINT& width)
  {
    UINT size = 4096;
    while (size < bytes && size < (1u << 30))
      size *= 2;
    if (size < bytes)
      size = bytes;
    auto it = g_StagingPool.lower_bound(bytes);
    if (it != g_StagingPool.end() && (unsigned long long)it->first <= (std::max)((unsigned long long)bytes * 2, (unsigned long long)size))
    {
      width = it->first;
      ComPtr<ID3D11Buffer> out = std::move(it->second);
      g_StagingPoolBytes -= (std::min)(g_StagingPoolBytes, (size_t)it->first);
      g_StagingPool.erase(it);
      ++g_StagingReused;
      return out;
    }
    D3D11_BUFFER_DESC s{};
    s.ByteWidth = size;
    s.Usage = D3D11_USAGE_STAGING;
    s.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Buffer> out;
    replay_gamedraws::CostTimer timer(replay_gamedraws::Cost_Staging);
    if (!device || FAILED(device->CreateBuffer(&s, nullptr, out.GetAddressOf())))
      return nullptr;
    width = size;
    ++g_StagingMade;
    return out;
  }

  void GiveStaging(ComPtr<ID3D11Buffer>&& buffer, UINT width)
  {
    if (!buffer || !width || g_StagingPoolBytes + width > MaxStagingPoolBytes)
      return;
    g_StagingPool.emplace(width, std::move(buffer));
    g_StagingPoolBytes += width;
  }

  struct Batch
  {
    unsigned Frame;
    std::vector<std::shared_ptr<Recipe>> Recipes;
    std::vector<Staged> Pages;
    int CameraPage;      // in Pages, -1 none
    UINT CameraFirst;
    int Pass = Pass_None;   // the pass it was kept in
  };

  std::mutex g_Mutex;   // g_Recipes, g_Attempts, g_Draws, g_Drawn
  std::map<long long, std::shared_ptr<Recipe>> g_Recipes;   // by shape << 16 | section
  std::map<long long, int> g_Attempts;
  std::vector<replay_shapes::Draw> g_Draws;
  std::set<long long> g_Drawn;
  std::atomic<int> g_Ready{ 0 };
  std::atomic<bool> g_Capturing{ false }, g_Enabled{ true };
  std::atomic<bool> g_Playing{ false };
  // People (stage 2), under g_Mutex but the atomics
  std::map<int, std::shared_ptr<Recipe>> g_Skins;
  int g_NextSkinId = 1;
  std::vector<replay_gamedraws::SkinCandidate> g_SkinCandidates;
  std::vector<replay_gamedraws::PersonDraw> g_PeopleDraws;
  std::atomic<bool> g_PeoplePlaying{ false }, g_LoggedPeople{ false };
  std::atomic<double> g_Clock{ 0.0 };
  int g_PendingModels = 0, g_PendingSkins = 0;   // render thread: kept in this pass
  int g_PendingWorld = 0;                        // render thread: the world's kept in this pass
  int g_PendingTracks = 0;                       // render thread: tracks' samples kept in this pass
  int g_PendingCloth = 0;                        // render thread: of those, cloths' (MaxClothCapturesPerPass of their own)
  int g_Pass = Pass_None;                        // render thread: the pass bound now (see Pass)
  int g_PassOrdinal = 0;                         // its place among its kind's passes after the G-buffer pass this frame
  int g_PassCounts[Passes] = {};
  // 2026-11-08 (the user: "there is still a minor texture/glass texture
  // flickering this must be from the alphas"): a kept draw of the decal, lit
  // or half-size passes is drawn at the end of the pass of its kind with its
  // number in the frame (see Pass), and how many of those the game makes
  // isn't fixed: it binds the lit picture's target from at least 9 places of
  // its view's drawing (0x143742BD0), each one only when its own draws are
  // there, 17 in a frame at most while it recorded, and the see-through
  // parts were kept at the 13th. A frame with fewer passes of the kind didn't
  // draw the ones kept at a later number at all, and they flickered. So the
  // fewest passes of each kind in the last PassHistoryFrames frames that had
  // any (render thread) are kept, and at the end of the last of those the
  // kept draws of every later number are drawn too (at most once a frame:
  // g_InjectedPass), always at the same place; how many such frames, and
  // each kind's fewest and most passes, are in the log every PassLogSeconds
  // while it plays
  const int PassHistoryFrames = 32;
  int g_PassHistory[Passes][PassHistoryFrames] = {};
  int g_PassHistoryAt = 0, g_PassHistoryFilled = 0;
  int g_PassFewest[Passes] = {};
  int g_PassLogFewest[Passes] = {}, g_PassLogMost[Passes] = {}, g_PassLogLate[Passes] = {};
  ULONGLONG g_PassLogSince = 0;
  const ULONGLONG PassLogSeconds = 10;
  bool g_SawMain = false;                        // this frame
  bool g_InjectedPass[Passes][MaxPassOrdinals] = {};
  std::map<std::pair<__int64, __int64>, int> g_PassKinds;   // targets[0], depth -> its pass
  std::map<int, int> g_ChosenLevel;              // shape -> the level the G-buffer pass drew it at (-1 its shape's), this frame
  std::vector<unsigned> g_ProbedAt;              // mesh id -> the frame it was last probed
  std::vector<unsigned> g_ProbedOtherAt;         // the same in the decal, lit and half-size passes
  int g_PendingProbes = 0;
  // Render thread: a probed frame missing one of a mesh's draws (its places
  // that frame dropped, see MaxProbesPerPassAll): mesh id -> that frame; and
  // the last frame whose probes ran out of room (all of that frame dropped)
  std::vector<unsigned> g_ProbeCutAt, g_ProbeCutOtherAt;
  unsigned g_ProbeFullAt = 0, g_ProbeFullOtherAt = 0;
  bool g_LoggedPass[Passes][MaxPassOrdinals] = {};
  bool g_TrackPass = false;                      // render thread: this main pass samples the tracks
  double g_LastTrackSample = -1e9;               // render thread: the recording's time of the last one that did
  // Render thread: the replay's own drawing's time (see TimingLogFrames)
  long long g_DrawTicks = 0;
  unsigned g_DrawFrames = 0;
  unsigned long long g_DrawCalls = 0;
  std::atomic<int> g_Upgraded{ 0 };              // texture slots of kept draws taken anew (see Upgrade), this recording
  std::shared_ptr<std::unordered_map<__int64, int> const> g_GameVbMeshesNow;   // render thread's copy of g_GameVbMeshes
  std::vector<unsigned> g_MeshDrawnAt;           // render thread: the frame the game last drew each mesh id in its main pass
  // And in its decal, lit and half-size passes, for the recorded models only
  // (2026-11-01: the Release gate's glass, csg_releasegateglass_01, is drawn
  // by the game only in a lit picture's pass, so GameDrawsModel took it to be
  // not drawn and the replay drew its recorded copy over the game's own all
  // through the playback). The world's own draws still go by the main pass's
  std::vector<unsigned> g_MeshDrawnOtherAt;
  int g_PendingWanted = 0;                       // of g_PendingSkins: of g_WantedVbs
  std::set<__int64> g_WantedVbs;                 // under g_Mutex
  // The world's own draws (under g_Mutex but the atomics)
  typedef std::tuple<__int64, UINT, UINT> WorldKeyType;
  std::map<WorldKeyType, std::shared_ptr<Recipe>> g_World;
  std::map<WorldKeyType, int> g_WorldAttempts;
  std::unordered_map<std::string, int> g_MeshIds;   // a mesh's full name -> its id
  // While it plays: the recorded models' meshes too, by their short names, so
  // the game's own drawing of them is known (2026-10-28: the Release gate's
  // doors and glass were in the level again, so the replay left them to the
  // game, which didn't draw them; see GameDrawsModel). And the render
  // thread's g_MeshDrawnAt / g_Frame as of its last copy (g_Mutex)
  std::unordered_map<std::string, int> g_ModelMeshIds;
  std::vector<unsigned> g_MeshDrawnShared;
  std::vector<unsigned> g_MeshDrawnOtherShared;
  unsigned g_FrameShared = 0;
  const float ModelDrawnMetres = 1.f;
  // While it records: each loaded mesh level's vertex buffer and how far its
  // mesh reaches from its origin (its bounding box; UpdateWorld), for
  // KeepNear. Vista meshes (a far zone's stand-in) aren't in it: they're
  // kept by their place only, as before
  std::shared_ptr<std::unordered_map<__int64, float> const> g_VbReach;
  // While it records: each loaded mesh level's vertex buffer and its mesh's
  // short name, for the log of the world's draws not kept (2026-10-28: the
  // crowds, a catwalk's glass sheet and a floodlight were missing in the
  // playback with no line saying why; see NoteWorldNotKept)
  std::shared_ptr<std::unordered_map<__int64, std::string> const> g_VbNames;
  std::set<std::string> g_WorldNotKeptLogged;   // g_Mutex
  std::shared_ptr<std::unordered_map<__int64, int> const> g_GameVbMeshes;   // while it plays: the game's vertex buffers' mesh ids
  std::atomic<bool> g_WorldCapturing{ false }, g_WorldPlaying{ false }, g_WorldOn{ true }, g_LoggedWorld{ false };
  float g_WorldCenter[3] = {};
  // The game's own instances' places of the kept world meshes (mesh id -> the
  // frame read and x y z each; see ProbeEvery)
  std::unordered_map<int, std::pair<unsigned, std::vector<float>>> g_GamePlaces;
  // The same from its decal, lit and half-size passes (see g_MeshDrawnOtherAt)
  std::unordered_map<int, std::pair<unsigned, std::vector<float>>> g_GamePlacesOther;
  // Other skinned draws' tracks (under g_Mutex but the atomics; see TrackSampleSeconds)
  struct Track
  {
    std::shared_ptr<Recipe> R;      // its state, from its first sample read back
    std::vector<double> Times;      // each sample's time (the recording's)
    std::vector<UINT> Counts;       // each sample's instances
    std::vector<size_t> Starts;     // each sample's first float in Data
    std::vector<float> Data;        // the samples' instances, R->Stride x 4 floats each
    int Header = -1;                // its instances' rows before their bones (-1: not known)
    std::string Mesh;               // its mesh (full name), once known (UpdateWorld)
    bool Faith = false;             // Faith's own (the game draws her)
    // A cloth's (see ClothSlotOf; its key's buffer is its index buffer): each
    // sample's vertices as the graphics card wrote them (ClothBytes each,
    // from ClothStarts in ClothData), which 4 bytes of a vertex are plain
    // numbers in its first sample (a bit each: blended between samples), and
    // a buffer of its mesh's that isn't written (MeshVb, for its name)
    bool Cloth = false;
    UINT ClothBytes = 0;
    unsigned ClothColumns = 0;
    std::vector<size_t> ClothStarts;
    std::vector<BYTE> ClothData;
    __int64 MeshVb = 0;
    // Only its bytes from the draw's own offset in that buffer are kept
    // (2026-11-02: two of Icarus's cloth draws, 22677 and 1842 indices, are
    // one 229296-byte buffer, the second's from byte 211296; both kept it whole)
    UINT ClothOffset = 0;
    // Carried by the recorded person it's on when no sample of it is near the
    // time (see AnchorCloths): that person, their bone nearest its vertices'
    // middle, that bone's pose at each sample (12 floats a sample, rows of its
    // turn with its place last; NaN where it isn't known), and whether its
    // vertices are in its instance's space (then its instance is carried)
    __int64 Owner = 0;
    int OwnerBone = -1;
    std::vector<float> AnchorPoses;
    bool ClothLocal = false;
  };
  std::map<WorldKeyType, Track> g_Tracks;
  size_t g_TrackBytes = 0;
  // Cloth (2026-11-02, the user: "Icarus doesn't has a hoodie still"; the
  // 19:40 log: a G-buffer pass draw whose vertex shader doesn't read bones
  // and whose vertex buffer the graphics card writes, 2493 vertices of 48
  // bytes, bind 0x89: EACloth's compute shaders write a cloth's vertices each
  // frame, so no kept draw had them): such a draw is kept as a track of its
  // own, its vertices copied with each sample (at most MaxClothBytes in all)
  // and put in a buffer of the tools' each frame while it plays
  const size_t MaxClothBytes = 384u << 20;
  // The recorded models' material values over time (see NoteMaterial)
  const size_t MaxMaterialBytes = 64u << 20, MaxMaterialSamples = 4096;
  size_t g_MaterialBytes = 0;   // under g_Mutex
  // While it plays: the cloths' owners' bones now (see AnchorCloths), by owner and bone (under g_Mutex)
  std::map<std::pair<__int64, int>, std::array<float, 12>> g_ClothAnchorsNow;
  const int MaxClothCapturesPerPass = 16;
  size_t g_ClothBytes = 0;
  bool g_ClothFullLogged = false;
  int g_ClothLogs = 0;
  bool g_TracksFullLogged = false;
  std::atomic<bool> g_TrackCapturing{ false }, g_TracksOn{ true }, g_LoggedTracks{ false };
  double g_TrackTime = -1.0;        // while it plays: the time played

  // Crowds (2026-11-01; the user: "Decompile the whole texture/crowd system and
  // how they are placed in the prefabs and fix them"; Research_Crowds_2026-11-01.md).
  // A crowd is a swarm (SwarmEntityData: MaxAgentCount members, 64 for a kind
  // of pedestrians, 1300 for the cars) placed by the city's layer31_crowds and
  // walking its boids network. The crowd system draws a mesh part of a swarm's
  // visible members as one draw: its vertex shader takes the draw's first entry
  // from b1 externalConstants (its first 4 bytes), and for each instance reads
  // the member's number from indirectionBuffer (16 bits), then its float4 from
  // transformBuffer and, for people, its animation from animationBuffer (32
  // bits; the poses from the swarm's animTexture, t1). None of that is the
  // instance buffer b0 points into, so the world's and the tracks' capture
  // never took them ("no instance offsets in its b0"). While it records, in the
  // G-buffer passes that sample the tracks (TrackSampleSeconds), each crowd draw
  // has its b1 window and those buffers copied on the graphics card and read
  // back: its members (number, float4, animation) kept with its kind (its mesh
  // part and look; its state kept once). While it plays, each kind's members
  // are drawn in a straight line between the samples around the time (a member
  // by its number, the float4's xyz; not in a gap longer than CrowdHoldSeconds)
  // with the tools' own buffers in those slots and b1 as recorded with its
  // first entry 0, and the game's own crowd draws aren't drawn (its swarms then
  // aren't the recorded ones; SkipGameDraw). Not 1:1 yet: the people's poses are
  // the swarm's animTexture as it is when it plays.
  enum CrowdBuffer { Crowd_Indirection, Crowd_Transform, Crowd_Animation, CrowdBuffers };
  const double CrowdHoldSeconds = 0.6;
  const int MaxCrowdKinds = 512;
  const int MaxCrowdCapturesPerPass = 96;
  const size_t MaxCrowdBytes = 256u << 20;
  const UINT MaxCrowdBufferBytes = 1u << 20;
  const UINT MaxCrowdMembers = 4096;          // a draw's
  const int CrowdVsViewSlots = 32;
  const int MaxCrowdKindLogs = 60;
  struct CrowdShader
  {
    bool Crowd = false;
    int Slot[CrowdBuffers] = { -1, -1, -1 };
  };
  std::unordered_map<__int64, CrowdShader> g_CrowdShaders;   // render thread, by vertex shader
  // Any thread (SkipGameDraw): whether a vertex shader is a crowd one
  SRWLOCK g_CrowdVsLock = SRWLOCK_INIT;
  std::unordered_map<__int64, bool> g_CrowdVs;
  struct CrowdKind
  {
    std::shared_ptr<Recipe> R;                // its state; its pixel shader's crowd buffer slots are instance slots (not saved)
    int Slot[CrowdBuffers] = { -1, -1, -1 };  // its vertex shader's
    int PsSlot[CrowdBuffers] = { -1, -1, -1 };
    DXGI_FORMAT Format[CrowdBuffers] = {};
    bool PsB1IsVsB1 = false;                  // its pixel shader's b1 is the vertex shader's window
    bool StateRead = false;                   // its b0 / b1 bytes read back
    std::string Mesh;
    std::vector<size_t> Samples;              // its samples in g_CrowdSamples, in time order
    // Playback (render thread)
    ComPtr<ID3D11Buffer> Own[CrowdBuffers];
    ComPtr<ID3D11ShaderResourceView> OwnView[CrowdBuffers];
    UINT OwnCapacity = 0;
    ComPtr<ID3D11Buffer> OwnB1;
    UINT OwnB1Bytes = 0;
  };
  struct CrowdSample
  {
    double Time = 0.0;
    int Kind = -1;
    std::vector<BYTE> B1;          // its vertex shader's b1 window, as read
    std::vector<UINT> Members;     // the members' numbers
    std::vector<float> Transforms; // a float4 each
    std::vector<UINT> Animations;  // one each (empty: none)
  };
  std::vector<std::shared_ptr<CrowdKind>> g_CrowdKinds;       // g_Mutex
  typedef std::tuple<__int64, UINT, UINT, unsigned long long> CrowdKeyType;   // vertex buffer, first index, count, look
  std::map<CrowdKeyType, int> g_CrowdKindKeys;               // g_Mutex
  std::vector<CrowdSample> g_CrowdSamples;                   // g_Mutex, in time order
  size_t g_CrowdBytes = 0;
  int g_CrowdBadDraws = 0, g_CrowdKindLogs = 0, g_CrowdSampleLogs = 0;
  bool g_CrowdsFullLogged = false;
  std::atomic<bool> g_CrowdPlaying{ false }, g_LoggedCrowds{ false };
  int g_PendingCrowds = 0;                                   // render thread: this pass
  bool g_CrowdInjected = false;                              // render thread: this frame
  // A crowd draw's copies (render thread), read back ReadbackAfterFrames later
  struct CrowdCopy
  {
    unsigned Frame = 0;
    double Time = 0.0;
    int Kind = -1;
    bool State = false;            // its kind's b0 / b1 windows copied too
    UINT Instances = 0;
    ComPtr<ID3D11Buffer> B1;       // its vertex shader's b1 buffer, whole (the frame's draws share it)
    UINT B1Bytes = 0, B1Offset = 0, B1Page = 0;   // the window's size and where, the buffer's size
    ComPtr<ID3D11Buffer> Buffer[CrowdBuffers];   // whole (the frame's draws share them)
    UINT Bytes[CrowdBuffers] = {}, First[CrowdBuffers] = {}, Count[CrowdBuffers] = {};
    ComPtr<ID3D11Buffer> VsB0, PsB0, PsB1;       // its kind's state's windows (State)
    UINT VsB0Bytes = 0, PsB0Bytes = 0, PsB1Bytes = 0;
  };
  std::vector<CrowdCopy> g_CrowdCopies;
  std::map<__int64, ComPtr<ID3D11Buffer>> g_CrowdFrameCopies;   // this frame's whole-buffer copies, by the game's buffer
  unsigned g_CrowdFrameCopiesAt = 0;

  // Render thread only
  ID3D11DeviceContext* g_Immediate = nullptr;
  ComPtr<ID3D11DeviceContext1> g_Immediate1;
  UINT g_FrameWidth = 0, g_FrameHeight = 0;
  unsigned g_Frame = 0;
  bool g_InMain = false;
  bool g_Injected = false;          // this frame
  bool g_ViewCopied = false;        // this frame
  std::map<std::pair<__int64, __int64>, bool> g_TargetKinds;
  std::vector<std::shared_ptr<Recipe>> g_Pending;   // kept this pass
  __int64 g_PendingCamera = 0;
  UINT g_PendingCameraFirst = 0;
  ComPtr<ID3D11Buffer> g_PendingCameraCopy;
  UINT g_PendingCameraWidth = 0, g_PendingCameraCapacity = 0;   // its buffer's size, its staging buffer's
  std::vector<Batch> g_Batches;
  size_t g_StagingBytes = 0;
  ComPtr<ID3D11Buffer> g_View;      // this frame's viewConstants, copied
  UINT g_ViewWidth = 0;
  ComPtr<ID3D11Buffer> g_Instances;
  ComPtr<ID3D11ShaderResourceView> g_InstancesView;
  UINT g_InstancesCapacity = 0;     // float4s
  int g_RecipeLogs = 0, g_FailLogs = 0;

  // The video screens (2026-11-01, the user: "Videos that play on the walls
  // (not billboards) are frozen"; Research_Billboards_2026-10-24.md). A kept
  // draw of a screen holds the texture its LinearMedia feed showed then (the
  // pixel shader's g_lmsAlbedoTexture), but a feed's frames are a ring of
  // textures (feed + 0x270, the frame now at + 0x248, each handle looked up
  // by 0x1437DFD20), and once the screen isn't in the game's world its feed
  // isn't streamed (LOD 255: its still BaseTexture, feed + 0x368). Each frame
  // the billboard manager's job (0x1437DEB30) puts each of its 32 channels'
  // textures now in its table (0x1437DF8D0: 0x40 a channel, + 0x00 the first,
  // + 0x08 the second), handed here by the hooks' hBillboardSizes; a texture's
  // view is its m_shaderView, + 0xB0 (Dx11Texture: 0x1435F34A2 creates it
  // there), taken only when it is a view as the game's are (the same vtable
  // as one of theirs) and held (a reference) until the channel's next. A draw
  // kept with one of a channel's recent frames in a video slot is drawn with
  // that channel's frame now; where the replay draws it goes back to the
  // hooks (FeedSizes), so the channel is streamed as if its screen were there.
  const int Feeds = 32;
  const int FeedTableStride = 0x40;
  const int TextureViewOffset = 0xB0;
  const size_t FeedSeenKept = 32;               // a channel's views remembered (its ring's, its levels')
  const ULONGLONG FeedWantedMs = 1000;
  const float FeedScreenSquareMetres = 100.f;   // a screen the replay draws taken as this big
  const float FeedMinSize = 0.006f, FeedMaxSize = 0.3f;   // the game's rows: LOD 4 at least, LOD 1 at most
  const size_t MaxFeedPlaces = 32;
  const int MaxFeedLogs = 24;
  SRWLOCK g_FeedLock = SRWLOCK_INIT;
  ComPtr<ID3D11ShaderResourceView> g_FeedNow[Feeds][2];   // under g_FeedLock
  std::vector<__int64> g_FeedSeen[Feeds][2];              // the views it had lately (their addresses), under it
  std::vector<float> g_FeedPlaces[Feeds];                 // where the replay drew it last (x y z), under it
  ULONGLONG g_FeedPlacedAt[Feeds] = {};                   // when, under it
  std::atomic<__int64> g_ViewVTable{ 0 };                 // a game view's vtable (TakeState)
  std::atomic<int> g_FeedLogs{ 0 };
  std::atomic<bool> g_FeedsLogged{ false };

  // A game texture's view: its m_shaderView when that is a view (the game's
  // vtable); 0 otherwise
  __int64 ViewOfTexture(__int64 texture, __int64 vtable)
  {
    if (texture < 0x10000 || !vtable)
      return 0;
    __try
    {
      __int64 view = *(__int64 const*)(texture + TextureViewOffset);
      if (view < 0x10000 || *(__int64 const*)view != vtable)
        return 0;
      return view;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return 0;
    }
  }

  // The billboard manager's table: each channel's two views (0: none)
  bool ReadFeedTable(__int64 table, __int64 vtable, __int64 (*views)[2])
  {
    __try
    {
      for (int ch = 0; ch < Feeds; ++ch)
        for (int k = 0; k < 2; ++k)
          views[ch][k] = ViewOfTexture(*(__int64 const*)(table + (__int64)ch * FeedTableStride + k * 8), vtable);
      return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
  }

  // The channel a view is a frame of (1 + channel x 2 + which), 0 none
  unsigned char FeedOf(ID3D11ShaderResourceView* view)
  {
    if (!view)
      return 0;
    unsigned char found = 0;
    AcquireSRWLockShared(&g_FeedLock);
    for (int ch = 0; ch < Feeds && !found; ++ch)
      for (int k = 0; k < 2 && !found; ++k)
        if (g_FeedNow[ch][k].Get() == view ||
            std::find(g_FeedSeen[ch][k].begin(), g_FeedSeen[ch][k].end(), (__int64)view) != g_FeedSeen[ch][k].end())
          found = (unsigned char)(1 + ch * 2 + k);
    ReleaseSRWLockShared(&g_FeedLock);
    return found;
  }

  // Where the game drew the recorded models (2026-11-02, the user: "Doors still
  // have the solid texture for some reason from one side": the Release gate's
  // sign planes, plane_1-1_01, which the game draws only while the gate's
  // events have them Enabled, were drawn by the replay at 6 - 92 s of the
  // playback; the game drew them at 29 - 38 s of the recording). Each model
  // part's draw read back while it records (every UpgradeEvery frames) is
  // kept as a sighting: its shape, the recording's time, whether it was the
  // G-buffer pass's, and its instances' places. ReplayModels asks for a
  // model's (SightingsAt) when the recording stops.
  struct Sighting
  {
    int Shape;
    float Clock;
    bool Main;
    std::vector<float> Places;
  };
  std::vector<Sighting> g_Sightings;   // under g_Mutex
  size_t g_SightingFloats = 0;
  const size_t MaxSightingFloats = 3u << 20;

  void NoteSighting(Recipe const& r)
  {
    if (r.WorldRows.empty() || !r.Kept || r.Shape < 0 || g_SightingFloats >= MaxSightingFloats)
      return;
    Sighting s{ r.Shape, (float)r.Clock, r.Pass == Pass_Main && !r.FromEnv, {} };
    int k = r.WorldRows[0];
    for (UINT i = 0; i < r.Kept; ++i)
    {
      float const* rows = &r.InstanceData[(size_t)i * r.Stride * 4];
      s.Places.insert(s.Places.end(), { rows[k * 4 + 3], rows[(k + 1) * 4 + 3], rows[(k + 2) * 4 + 3] });
    }
    g_SightingFloats += s.Places.size();
    g_Sightings.push_back(std::move(s));
  }

  // Where the main view looked while it recorded (2026-11-03, the user: "Door
  // arrow textures don't move with the doors either"). The Release prison
  // door's prefab (pf_area00_release_animatedprisondoor_..._gen4) has its two
  // arrow signs, plane_1-1_01_gatesign_01, as planes that stand still; its
  // open event starts the doors' timeline and sets the bool that is both
  // signs' Enabled (DefaultValue True), so the game stops drawing them as the
  // doors open. The replay's show windows (ReplayModels::ShowAndFollow) judged
  // that from its neighbours' draws within 4 m only, and the doors were the
  // only ones: once open, further off. So the camera of each G-buffer pass
  // read back (every UpgradeEvery frames) is kept: viewConstants'
  // viewProjMatrix (+ 160) and cameraPos (+ 720, Research_Replay_Mission_World
  // 2026-10-26), with the recording's time. Whether the matrix is used with
  // the place as a row vector or a column one isn't known from the game's
  // code: it's judged from the G-buffer pass's own draws (JudgeViewOrder).
  struct ViewSample
  {
    float Clock;
    float ViewProj[16];
    float Camera[3];
  };
  std::vector<ViewSample> g_ViewSamples;   // under g_Mutex
  const size_t MaxViewSamples = 1u << 16;
  const int ViewProjOffset = 160;          // viewConstants
  int g_ViewOrder = 0;                     // under g_Mutex: 0 not judged, 1 the place a row vector, 2 a column one, -1 neither

  void NoteView(Batch const& batch)
  {
    if (batch.Pass != Pass_Main || batch.CameraPage < 0 || batch.Recipes.empty())
      return;
    Staged const& page = batch.Pages[batch.CameraPage];
    size_t at = (size_t)batch.CameraFirst * 16;
    if (!page.Read || at + CameraPosOffset + 12 > page.Bytes.size() || at + ViewProjOffset + 64 > page.Bytes.size())
      return;
    ViewSample v{};
    v.Clock = (float)batch.Recipes[0]->Clock;
    memcpy(v.ViewProj, page.Bytes.data() + at + ViewProjOffset, sizeof(v.ViewProj));
    memcpy(v.Camera, page.Bytes.data() + at + CameraPosOffset, sizeof(v.Camera));
    std::lock_guard<std::mutex> lock(g_Mutex);
    if (g_ViewSamples.size() < MaxViewSamples && (g_ViewSamples.empty() || v.Clock > g_ViewSamples.back().Clock))
      g_ViewSamples.push_back(v);
  }

  // A place in a view's picture (within margin of its edges, in front of it)
  // with its matrix used either way
  bool InView(ViewSample const& v, float const* p, int order, float margin)
  {
    float clip[4];
    for (int c = 0; c < 4; ++c)
      clip[c] = order == 1 ? p[0] * v.ViewProj[0 * 4 + c] + p[1] * v.ViewProj[1 * 4 + c] + p[2] * v.ViewProj[2 * 4 + c] + v.ViewProj[3 * 4 + c]
                           : v.ViewProj[c * 4 + 0] * p[0] + v.ViewProj[c * 4 + 1] * p[1] + v.ViewProj[c * 4 + 2] * p[2] + v.ViewProj[c * 4 + 3];
    return clip[3] > 0.1f && fabsf(clip[0]) <= clip[3] * margin && fabsf(clip[1]) <= clip[3] * margin;
  }

  ViewSample const* ViewNear(float clock, float within)
  {
    auto it = std::lower_bound(g_ViewSamples.begin(), g_ViewSamples.end(), clock,
      [](ViewSample const& v, float t) { return v.Clock < t; });
    ViewSample const* best = nullptr;
    if (it != g_ViewSamples.end() && it->Clock - clock <= within)
      best = &*it;
    if (it != g_ViewSamples.begin() && clock - (it - 1)->Clock <= within && (!best || clock - (it - 1)->Clock < best->Clock - clock))
      best = &*(it - 1);
    return best;
  }

  // Which way the matrix is used (under g_Mutex): the one that has the G-buffer
  // pass's own draws' places in the picture of their frame's view (loosely:
  // a big model's place can be off it while it's drawn), at least
  // ViewOrderShare of them and well more than the other
  void JudgeViewOrder()
  {
    const float ViewOrderShare = 0.7f, Loose = 1.5f;
    if (g_ViewOrder != 0)
      return;
    int tried = 0, in[3] = {};
    for (Sighting const& s : g_Sightings)
    {
      if (!s.Main)
        continue;
      ViewSample const* v = ViewNear(s.Clock, 0.001f);
      if (!v)
        continue;
      for (size_t j = 0; j + 3 <= s.Places.size() && j < 8 * 3; j += 3)
      {
        ++tried;
        in[1] += InView(*v, &s.Places[j], 1, Loose);
        in[2] += InView(*v, &s.Places[j], 2, Loose);
      }
    }
    int better = in[1] >= in[2] ? 1 : 2, worse = 3 - better;
    g_ViewOrder = tried >= 50 && in[better] >= tried * ViewOrderShare && in[better] > in[worse] * 1.3f ? better : -1;
    util::log::Write("Replay: the recording's camera (%d views kept): its matrix (viewConstants + 160) taken with the place as a %s "
      "(%d of %d places of the G-buffer pass's draws in its view that way, %d the other way)%s", (int)g_ViewSamples.size(),
      better == 1 ? "row vector" : "column vector", in[better], tried, in[worse], g_ViewOrder < 0 ?
      ": not clear, so where it looked isn't used" : "");
  }

  // Where a kept draw with video slots is drawn (its instance's place), by channel
  void NoteFeedPlace(Recipe const& r, float const* place, std::vector<float>* places)
  {
    for (int i = 0; i < PsViewSlots; ++i)
    {
      if (!r.Feed[i])
        continue;
      std::vector<float>& list = places[(r.Feed[i] - 1) / 2];
      if (list.size() < MaxFeedPlaces * 3)
        list.insert(list.end(), place, place + 3);
    }
  }
  std::atomic<bool> g_LoggedPlaying{ false };
  bool g_LoggedNoTargets = false;
  thread_local bool t_Drawing = false;

  typedef void(STDMETHODCALLTYPE* tOMSetRenderTargets)(ID3D11DeviceContext*, UINT, ID3D11RenderTargetView* const*,
    ID3D11DepthStencilView*);
  typedef void(STDMETHODCALLTYPE* tOMSetRenderTargetsUav)(ID3D11DeviceContext*, UINT, ID3D11RenderTargetView* const*,
    ID3D11DepthStencilView*, UINT, UINT, ID3D11UnorderedAccessView* const*, UINT const*);
  tOMSetRenderTargets oOMSetRenderTargets = nullptr;
  tOMSetRenderTargetsUav oOMSetRenderTargetsUav = nullptr;

  // The game's immediate context (its pointer may be another interface of it than the tools')
  bool IsImmediate(ID3D11DeviceContext* pContext)
  {
    return g_Immediate && pContext && (pContext == g_Immediate || pContext->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE);
  }

  long long KeyOf(int shape, int section)
  {
    return ((long long)shape << 16) | (long long)(section & 0xFFFF);
  }

  // The main view's G-buffer: its 4 targets in their formats and a depth target the frame's size
  bool IsMainGbuffer(UINT count, ID3D11RenderTargetView* const* targets, ID3D11DepthStencilView* depth)
  {
    if (count < 4 || !targets || !depth || !g_FrameWidth)
      return false;
    for (UINT i = 0; i < count; ++i)
      if ((i < 4) != (targets[i] != nullptr))
        return false;
    auto key = std::make_pair((__int64)targets[0], (__int64)depth);
    auto it = g_TargetKinds.find(key);
    if (it != g_TargetKinds.end())
      return it->second;
    bool main = true;
    for (int i = 0; i < 4 && main; ++i)
    {
      D3D11_RENDER_TARGET_VIEW_DESC d{};
      targets[i]->GetDesc(&d);
      main = d.Format == GbufferFormats[i];
    }
    if (main)
    {
      ComPtr<ID3D11Resource> res;
      depth->GetResource(res.GetAddressOf());
      ComPtr<ID3D11Texture2D> tex;
      D3D11_TEXTURE2D_DESC td{};
      main = res && SUCCEEDED(res.As(&tex)) && tex;
      if (main)
      {
        tex->GetDesc(&td);
        main = td.Width == g_FrameWidth && td.Height == g_FrameHeight;
      }
    }
    if (g_TargetKinds.size() > 256)
      g_TargetKinds.clear();
    g_TargetKinds[key] = main;
    return main;
  }

  // The depth target the frame's size (the main view's)
  bool FrameDepth(ID3D11DepthStencilView* depth)
  {
    if (!depth || !g_FrameWidth)
      return false;
    ComPtr<ID3D11Resource> res;
    depth->GetResource(res.GetAddressOf());
    ComPtr<ID3D11Texture2D> tex;
    D3D11_TEXTURE2D_DESC td{};
    if (!res || FAILED(res.As(&tex)) || !tex)
      return false;
    tex->GetDesc(&td);
    return td.Width == g_FrameWidth && td.Height == g_FrameHeight;
  }

  // The depth target half the frame's size (rounded either way)
  bool HalfDepth(ID3D11DepthStencilView* depth)
  {
    if (!depth || !g_FrameWidth)
      return false;
    ComPtr<ID3D11Resource> res;
    depth->GetResource(res.GetAddressOf());
    ComPtr<ID3D11Texture2D> tex;
    D3D11_TEXTURE2D_DESC td{};
    if (!res || FAILED(res.As(&tex)) || !tex)
      return false;
    tex->GetDesc(&td);
    return (td.Width == g_FrameWidth / 2 || td.Width == (g_FrameWidth + 1) / 2) &&
      (td.Height == g_FrameHeight / 2 || td.Height == (g_FrameHeight + 1) / 2);
  }

  // The pass the targets are (see Pass)
  // The G-buffer's 4 targets in their formats, any size (a reflection's face too)
  bool GbufferTargets(UINT count, ID3D11RenderTargetView* const* targets)
  {
    if (count < 4 || !targets)
      return false;
    for (UINT i = 0; i < count; ++i)
      if ((i < 4) != (targets[i] != nullptr))
        return false;
    for (int i = 0; i < 4; ++i)
    {
      D3D11_RENDER_TARGET_VIEW_DESC d{};
      targets[i]->GetDesc(&d);
      if (d.Format != GbufferFormats[i])
        return false;
    }
    return true;
  }

  int PassOf(UINT count, ID3D11RenderTargetView* const* targets, ID3D11DepthStencilView* depth)
  {
    if (IsMainGbuffer(count, targets, depth))
      return Pass_Main;
    if (!targets || !count || !targets[0] || !depth || !g_FrameWidth)
      return Pass_None;
    auto key = std::make_pair((__int64)targets[0], (__int64)depth);
    auto it = g_PassKinds.find(key);
    if (it != g_PassKinds.end())
      return it->second;
    int pass = Pass_None;
    UINT bound = 0;
    for (UINT i = 0; i < count; ++i)
      bound += targets[i] != nullptr;
    D3D11_RENDER_TARGET_VIEW_DESC d{};
    targets[0]->GetDesc(&d);
    if (bound == 4 && GbufferTargets(count, targets))
      pass = Pass_Env;
    else if (bound == 3 && count >= 3 && targets[1] && targets[2])
    {
      bool decal = d.Format == GbufferFormats[0];
      for (int i = 1; i < 3 && decal; ++i)
      {
        D3D11_RENDER_TARGET_VIEW_DESC o{};
        targets[i]->GetDesc(&o);
        decal = o.Format == GbufferFormats[i];
      }
      if (decal && FrameDepth(depth))
        pass = Pass_Decal;
    }
    else if (bound == 1 && d.Format == DXGI_FORMAT_R16G16B16A16_FLOAT && FrameDepth(depth))
      pass = Pass_Lit;
    else if (bound == 1 && d.Format == DXGI_FORMAT_R16G16B16A16_FLOAT && HalfDepth(depth))
      pass = Pass_Half;
    else if (bound == 1 && d.Format == DXGI_FORMAT_R8_UNORM && FrameDepth(depth))
      pass = Pass_Rv;
    if (g_PassKinds.size() > 256)
      g_PassKinds.clear();
    g_PassKinds[key] = pass;
    return pass;
  }

  // A staging copy of a buffer of the game's, made now on the immediate context
  bool Stage(ID3D11Buffer* source, Staged& out)
  {
    D3D11_BUFFER_DESC d{};
    source->GetDesc(&d);
    if (g_StagingBytes + d.ByteWidth > MaxStagingBytes)
      return false;
    ComPtr<ID3D11Device> device;
    g_Immediate->GetDevice(device.GetAddressOf());
    UINT capacity = 0;
    out.Staging = TakeStaging(device.Get(), d.ByteWidth, capacity);
    if (!out.Staging)
      return false;
    g_Immediate->CopySubresourceRegion(out.Staging.Get(), 0, 0, 0, 0, source, 0, nullptr);
    out.Source = (__int64)source;
    out.Width = d.ByteWidth;
    out.Capacity = capacity;
    out.Read = false;
    g_StagingBytes += d.ByteWidth;
    g_StagedBytesNow += d.ByteWidth;
    return true;
  }

  // The end of the G-buffer pass with recipes kept in it: the buffers their data is in, copied
  void CopyPending()
  {
    if (g_Pending.empty())
      return;
    replay_gamedraws::CostTimer timer(replay_gamedraws::Cost_Copies);
    Batch batch{ g_Frame, {}, {}, -1, g_PendingCameraFirst };
    batch.Pass = g_Pass;
    if (g_PendingCameraCopy)
    {
      // (Its own buffer's size, as Stage counted it; the staging buffer's
      // goes back to the pool. Until 2026-11-04 the staging buffer's size
      // was taken for both: the bytes past the camera's were read too)
      batch.Pages.push_back({ g_PendingCamera, g_PendingCameraCopy, g_PendingCameraWidth, false, {}, g_PendingCameraCapacity });
      batch.CameraPage = 0;
      g_PendingCameraCopy.Reset();
    }
    for (auto& recipe : g_Pending)
    {
      bool ok = true;
      for (auto& page : recipe->Pages)
      {
        if (!page)
          continue;
        bool have = false;
        for (Staged const& s : batch.Pages)
          have = have || s.Source == (__int64)page.Get();
        Staged staged{};
        if (!have && !(ok = Stage(page.Get(), staged)))
          break;
        if (!have)
          batch.Pages.push_back(std::move(staged));
      }
      if (ok)
        batch.Recipes.push_back(recipe);
      else if (recipe->Candidate)
        ;   // a later draw of a kept one: let go (the kept one stays)
      else if (recipe->World)
      {
        std::lock_guard<std::mutex> lock(g_Mutex);
        g_World.erase(recipe->WorldKey);
      }
      else if (!recipe->Skin && !recipe->Track && !recipe->Probe)
      {
        // No room for its copies now: tried again later (a person's: in a later frame kept)
        std::lock_guard<std::mutex> lock(g_Mutex);
        long long key = KeyOf(recipe->Shape, recipe->Section);
        if (recipe->Pass != Pass_Main)
          key |= ((long long)(recipe->Pass * MaxPassOrdinals + recipe->PassOrdinal + 1)) << 48;
        g_Recipes.erase(key);
      }
    }
    g_Pending.clear();
    g_PendingCamera = 0;
    g_PendingModels = 0;
    g_PendingSkins = 0;
    g_PendingWanted = 0;
    g_PendingWorld = 0;
    g_PendingTracks = 0;
    g_PendingCloth = 0;
    g_PendingProbes = 0;
    if (!batch.Recipes.empty())
      g_Batches.push_back(std::move(batch));
    else
      // None of them copied whole: their copies given back (until
      // 2026-11-04 let go, and still counted in g_StagingBytes)
      for (Staged& s : batch.Pages)
        if (s.Staging)
        {
          g_StagingBytes -= (std::min)(g_StagingBytes, (size_t)s.Width);
          GiveStaging(std::move(s.Staging), s.Capacity ? s.Capacity : s.Width);
        }
  }

  // The context's state the tools' draws change, put back exactly after
  struct GameState
  {
    ID3D11VertexShader* Vs = nullptr;
    ID3D11PixelShader* Ps = nullptr;
    ID3D11GeometryShader* Gs = nullptr;
    ID3D11HullShader* Hs = nullptr;
    ID3D11DomainShader* Ds = nullptr;
    ID3D11Buffer* VsCb[CbSlots] = {};
    UINT VsFirst[CbSlots] = {}, VsNum[CbSlots] = {};
    ID3D11Buffer* PsCb[CbSlots] = {};
    UINT PsFirst[CbSlots] = {}, PsNum[CbSlots] = {};
    ID3D11ShaderResourceView* VsViews[VsViewSlots] = {};
    ID3D11ShaderResourceView* PsViews[PsViewSlots] = {};
    ID3D11SamplerState* VsSamplers[VsSamplerSlots] = {};
    ID3D11SamplerState* PsSamplers[PsSamplerSlots] = {};
    ID3D11InputLayout* Layout = nullptr;
    D3D11_PRIMITIVE_TOPOLOGY Topology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
    ID3D11Buffer* Vbs[VbSlots] = {};
    UINT Strides[VbSlots] = {}, Offsets[VbSlots] = {};
    ID3D11Buffer* Ib = nullptr;
    DXGI_FORMAT IbFormat = DXGI_FORMAT_UNKNOWN;
    UINT IbOffset = 0;
    ID3D11DepthStencilState* Depth = nullptr;
    UINT StencilRef = 0;
    ID3D11BlendState* Blend = nullptr;
    FLOAT BlendFactor[4] = {};
    UINT SampleMask = 0;
    ID3D11RasterizerState* Raster = nullptr;

    void Save(ID3D11DeviceContext1* c)
    {
      c->VSGetShader(&Vs, nullptr, nullptr);
      c->PSGetShader(&Ps, nullptr, nullptr);
      c->GSGetShader(&Gs, nullptr, nullptr);
      c->HSGetShader(&Hs, nullptr, nullptr);
      c->DSGetShader(&Ds, nullptr, nullptr);
      c->VSGetConstantBuffers1(0, CbSlots, VsCb, VsFirst, VsNum);
      c->PSGetConstantBuffers1(0, CbSlots, PsCb, PsFirst, PsNum);
      c->VSGetShaderResources(0, VsViewSlots, VsViews);
      c->PSGetShaderResources(0, PsViewSlots, PsViews);
      c->VSGetSamplers(0, VsSamplerSlots, VsSamplers);
      c->PSGetSamplers(0, PsSamplerSlots, PsSamplers);
      c->IAGetInputLayout(&Layout);
      c->IAGetPrimitiveTopology(&Topology);
      c->IAGetVertexBuffers(0, VbSlots, Vbs, Strides, Offsets);
      c->IAGetIndexBuffer(&Ib, &IbFormat, &IbOffset);
      c->OMGetDepthStencilState(&Depth, &StencilRef);
      c->OMGetBlendState(&Blend, BlendFactor, &SampleMask);
      c->RSGetState(&Raster);
    }

    void Restore(ID3D11DeviceContext1* c)
    {
      c->VSSetShader(Vs, nullptr, 0);
      c->PSSetShader(Ps, nullptr, 0);
      c->GSSetShader(Gs, nullptr, 0);
      c->HSSetShader(Hs, nullptr, 0);
      c->DSSetShader(Ds, nullptr, 0);
      // Each slot as it was bound: with its 11.1 window, or whole when it has none
      for (int i = 0; i < CbSlots; ++i)
      {
        if (VsCb[i] && !VsNum[i])
          c->VSSetConstantBuffers(i, 1, &VsCb[i]);
        else
          c->VSSetConstantBuffers1(i, 1, &VsCb[i], &VsFirst[i], &VsNum[i]);
        if (PsCb[i] && !PsNum[i])
          c->PSSetConstantBuffers(i, 1, &PsCb[i]);
        else
          c->PSSetConstantBuffers1(i, 1, &PsCb[i], &PsFirst[i], &PsNum[i]);
      }
      c->VSSetShaderResources(0, VsViewSlots, VsViews);
      c->PSSetShaderResources(0, PsViewSlots, PsViews);
      c->VSSetSamplers(0, VsSamplerSlots, VsSamplers);
      c->PSSetSamplers(0, PsSamplerSlots, PsSamplers);
      c->IASetInputLayout(Layout);
      c->IASetPrimitiveTopology(Topology);
      c->IASetVertexBuffers(0, VbSlots, Vbs, Strides, Offsets);
      c->IASetIndexBuffer(Ib, IbFormat, IbOffset);
      c->OMSetDepthStencilState(Depth, StencilRef);
      c->OMSetBlendState(Blend, BlendFactor, SampleMask);
      c->RSSetState(Raster);
      auto release = [](IUnknown* p) {
        if (p)
          p->Release();
      };
      release(Vs);
      release(Ps);
      release(Gs);
      release(Hs);
      release(Ds);
      for (auto p : VsCb) release(p);
      for (auto p : PsCb) release(p);
      for (auto p : VsViews) release(p);
      for (auto p : PsViews) release(p);
      for (auto p : VsSamplers) release(p);
      for (auto p : PsSamplers) release(p);
      release(Layout);
      for (auto p : Vbs) release(p);
      release(Ib);
      release(Depth);
      release(Blend);
      release(Raster);
    }
  };

  ComPtr<ID3D11Buffer> ConstantsWith(ID3D11Device* device, std::vector<BYTE> const& bytes, bool dynamic)
  {
    ComPtr<ID3D11Buffer> out;
    if (bytes.empty())
      return out;
    D3D11_BUFFER_DESC d{};
    d.ByteWidth = (UINT)((bytes.size() + 15) & ~(size_t)15);
    d.Usage = dynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
    d.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    d.CPUAccessFlags = dynamic ? D3D11_CPU_ACCESS_WRITE : 0;
    std::vector<BYTE> padded(bytes);
    padded.resize(d.ByteWidth, 0);
    D3D11_SUBRESOURCE_DATA init{ padded.data(), 0, 0 };
    device->CreateBuffer(&d, &init, out.GetAddressOf());
    return out;
  }

  Staged const* PageOf(Batch const& batch, __int64 page)
  {
    for (Staged const& s : batch.Pages)
      if (s.Source == page && s.Read)
        return &s;
    return nullptr;
  }

  bool WindowBytes(Batch const& batch, Window const& w, std::vector<BYTE>& out)
  {
    out.clear();
    if (!w.Page)
      return true;
    Staged const* s = PageOf(batch, w.Page);
    size_t at = (size_t)w.First * 16, size = (size_t)w.Num * 16;
    if (!s || !size || size > MaxWindowBytes)
      return false;
    if (at + size > s->Bytes.size())
      size = at < s->Bytes.size() ? s->Bytes.size() - at : 0;   // a binding past the buffer's end reads 0 there
    if (!size)
      return false;
    out.assign(s->Bytes.begin() + at, s->Bytes.begin() + at + size);
    return true;
  }

  // An instance's 3 rows from row k that are a transform: their xyz columns
  // at right angles and not flat, its place a number
  bool IsTransformAt(float const* rows, int k)
  {
    float c[3][3];
    for (int col = 0; col < 3; ++col)
      for (int r = 0; r < 3; ++r)
        c[col][r] = rows[(k + r) * 4 + col];
    float n[3];
    for (int col = 0; col < 3; ++col)
    {
      n[col] = sqrtf(c[col][0] * c[col][0] + c[col][1] * c[col][1] + c[col][2] * c[col][2]);
      if (!(n[col] > 0.01f && n[col] < 1000.f))
        return false;
    }
    for (int a = 0; a < 3; ++a)
      for (int b = a + 1; b < 3; ++b)
      {
        float dot = c[a][0] * c[b][0] + c[a][1] * c[b][1] + c[a][2] * c[b][2];
        if (fabsf(dot) > 0.02f * n[a] * n[b])
          return false;
      }
    for (int r = 0; r < 3; ++r)
      if (!std::isfinite(rows[(k + r) * 4 + 3]) || fabsf(rows[(k + r) * 4 + 3]) > 50000.f)
        return false;
    return true;
  }

  // A recipe's data from its batch's copies; false with why
  bool DecodeWith(ID3D11Device* device, Batch const& batch, Recipe& r, char const** why, bool needWorld, float meshReach)
  {
    std::vector<BYTE> vsB1, psB1;
    if (!WindowBytes(batch, r.VsB0, r.VsB0Bytes) || r.VsB0Bytes.empty())
    {
      *why = "its b0 wasn't read";
      return false;
    }
    if (!WindowBytes(batch, r.VsB1, vsB1) || !WindowBytes(batch, r.PsB0, r.PsB0Bytes) || !WindowBytes(batch, r.PsB1, psB1))
    {
      *why = "a b0 / b1 window wasn't read";
      return false;
    }
    if (r.VsOffsetsAt < 0 || (size_t)r.VsOffsetsAt + 16 > r.VsB0Bytes.size())
    {
      *why = "no instance offsets in its b0";
      return false;
    }
    int start = *(int const*)(r.VsB0Bytes.data() + r.VsOffsetsAt);
    int stride = *(int const*)(r.VsB0Bytes.data() + r.VsOffsetsAt + 4);
    Staged const* page = PageOf(batch, r.InstancePage);
    if (!page || start < 0 || stride < 3 || (UINT)stride > MaxStride)
    {
      *why = "its instances weren't read (or the offsets aren't sane)";
      return false;
    }
    UINT kept = r.Instances < MaxInstancesKept ? r.Instances : MaxInstancesKept;
    size_t instanceBytes = (size_t)stride * 16;
    size_t first = ((size_t)r.InstanceFirst + (size_t)start) * 16;
    if (!kept || first + (kept - 1) * instanceBytes + instanceBytes > page->Bytes.size())
    {
      *why = "its instances are past the copy";
      return false;
    }
    r.Stride = (UINT)stride;
    r.Kept = kept;
    r.InstanceData.resize((size_t)kept * stride * 4);
    for (UINT i = 0; i < kept; ++i)
      memcpy(&r.InstanceData[(size_t)i * stride * 4], page->Bytes.data() + first + i * instanceBytes, instanceBytes);
    // The world matrix rows (on its first instance), near the camera when it's known
    float camera[3] = {};
    bool haveCamera = false;
    if (batch.CameraPage >= 0 && batch.Pages[batch.CameraPage].Read &&
        (size_t)batch.CameraFirst * 16 + CameraPosOffset + 12 <= batch.Pages[batch.CameraPage].Bytes.size())
    {
      memcpy(camera, batch.Pages[batch.CameraPage].Bytes.data() + (size_t)batch.CameraFirst * 16 + CameraPosOffset, 12);
      haveCamera = true;
    }
    // (A world mesh's place by the nearest it can be: its reach from its
    // bounding box, scaled, as in KeepNear. 2026-10-28: Release's own ground,
    // ground_release_01, wasn't kept for its origin being farther away)
    float const* rows = r.InstanceData.data();
    for (int k = 0; k + 3 <= stride; ++k)
      if (IsTransformAt(rows, k))
      {
        float dx = rows[k * 4 + 3] - camera[0], dy = rows[(k + 1) * 4 + 3] - camera[1], dz = rows[(k + 2) * 4 + 3] - camera[2];
        float scale2 = 0.f;
        for (int row = 0; row < 3; ++row)
          for (int c = 0; c < 3; ++c)
            scale2 += rows[(k + row) * 4 + c] * rows[(k + row) * 4 + c];
        float limit = MaxWorldFromCamera + (meshReach > 0.f && std::isfinite(scale2) ? meshReach * sqrtf(scale2) : 0.f);
        if (!haveCamera || dx * dx + dy * dy + dz * dz <= limit * limit)
        {
          r.WorldRows.push_back(k);
          k += 2;
        }
      }
    if (needWorld && r.WorldRows.empty())
    {
      *why = "no rows of its instance are a transform near the camera";
      return false;
    }
    // A track's later sample: its instances are all that's kept of it
    if (r.SampleOnly)
    {
      for (auto& p : r.Pages)
        p.Reset();
      return true;
    }
    r.VsB1Copy = ConstantsWith(device, vsB1, false);
    r.PsB1Copy = ConstantsWith(device, psB1, false);
    r.VsB1Bytes = vsB1;
    r.PsB1Bytes = psB1;
    r.VsB0Own = ConstantsWith(device, r.VsB0Bytes, true);
    r.PsB0Own = ConstantsWith(device, r.PsB0Bytes, true);
    if (!r.VsB0Own || (!vsB1.empty() && !r.VsB1Copy) || (!psB1.empty() && !r.PsB1Copy) || (!r.PsB0Bytes.empty() && !r.PsB0Own))
    {
      *why = "the tools' constant buffers couldn't be made";
      return false;
    }
    for (auto& p : r.Pages)
      p.Reset();
    return true;
  }

  // A draw's input state now, in the log (render thread, during the draw):
  // its vertex buffers, its vertex shader's views and constant buffers.
  // 2026-10-28: the crowds (pedestrians, crowd drones and cars) weren't kept,
  // "no instance offsets in its b0": where their instances are wasn't known
  void LogDrawInputs(char const* name, UINT indexCount, UINT instances)
  {
    ID3D11Buffer* vbs[4] = {};
    UINT strides[4] = {}, offsets[4] = {};
    g_Immediate1->IAGetVertexBuffers(0, 4, vbs, strides, offsets);
    char text[700];
    size_t at = 0;
    text[0] = 0;
    for (int i = 0; i < 4; ++i)
    {
      if (!vbs[i])
        continue;
      D3D11_BUFFER_DESC d{};
      vbs[i]->GetDesc(&d);
      int w = sprintf_s(text + at, sizeof(text) - at, "%sslot %d: stride %u offset %u (%u bytes, usage %d, cpu 0x%X)", at ? "; " : "", i,
        strides[i], offsets[i], d.ByteWidth, (int)d.Usage, d.CPUAccessFlags);
      at += w > 0 ? w : 0;
      vbs[i]->Release();
    }
    util::log::Write("Replay (diagnostic): '%s' (%u indices x %u instances): vertex buffers %s", name, indexCount, instances,
      text[0] ? text : "none");
    ID3D11ShaderResourceView* views[32] = {};
    g_Immediate1->VSGetShaderResources(0, 32, views);
    at = 0;
    text[0] = 0;
    for (int i = 0; i < 32; ++i)
    {
      if (!views[i])
        continue;
      D3D11_SHADER_RESOURCE_VIEW_DESC vd{};
      views[i]->GetDesc(&vd);
      ComPtr<ID3D11Resource> res;
      views[i]->GetResource(res.GetAddressOf());
      ComPtr<ID3D11Buffer> buffer;
      int w = 0;
      if (res && SUCCEEDED(res.As(&buffer)) && buffer)
      {
        D3D11_BUFFER_DESC d{};
        buffer->GetDesc(&d);
        w = _snprintf_s(text + at, sizeof(text) - at, _TRUNCATE,
          "%st%d: buffer %u bytes, structure %u, misc 0x%X, view format %d first %u count %u", at ? "; " : "", i, d.ByteWidth,
          d.StructureByteStride, d.MiscFlags, (int)vd.Format, vd.Buffer.FirstElement, vd.Buffer.NumElements);
      }
      else
        w = _snprintf_s(text + at, sizeof(text) - at, _TRUNCATE, "%st%d: dimension %d format %d", at ? "; " : "", i,
          (int)vd.ViewDimension, (int)vd.Format);
      // Cut short where it's full (up to 32 views don't all fit)
      at = w >= 0 ? at + w : sizeof(text) - 1;
      views[i]->Release();
    }
    util::log::Write("Replay (diagnostic):   its vertex shader's views: %s", text[0] ? text : "none");
    ID3D11Buffer* cbs[4] = {};
    UINT first[4] = {}, num[4] = {};
    g_Immediate1->VSGetConstantBuffers1(0, 4, cbs, first, num);
    at = 0;
    text[0] = 0;
    for (int i = 0; i < 4; ++i)
    {
      if (!cbs[i])
        continue;
      D3D11_BUFFER_DESC d{};
      cbs[i]->GetDesc(&d);
      int w = sprintf_s(text + at, sizeof(text) - at, "%sb%d: %u bytes, first %u num %u", at ? "; " : "", i, d.ByteWidth, first[i], num[i]);
      at += w > 0 ? w : 0;
      cbs[i]->Release();
    }
    util::log::Write("Replay (diagnostic):   its vertex shader's constant buffers: %s", text[0] ? text : "none");
    // The names in its vertex shader (its constants and resources)
    ID3D11VertexShader* vs = nullptr;
    g_Immediate1->VSGetShader(&vs, nullptr, nullptr);
    char names[600] = "";
    bool noted = vs && replay_drawlog::ShaderNames((__int64)vs, names, sizeof(names));
    if (vs)
      vs->Release();
    util::log::Write("Replay (diagnostic):   its vertex shader's names: %s", noted ? names : "(not noted)");
  }
  int g_DrawInputLogs = 0, g_FullDrawInputLogs = 0;   // render thread

  // A crowd draw's inputs copied on the graphics card and read back for the log
  // (2026-10-28, the 04:01 test: the crowd system's draws read their instances
  // from t24 transformBuffer through t23 indirectionBuffer, with 16 bytes of
  // b1 externalConstants a draw; how those are laid out wasn't known)
  struct CrowdProbe
  {
    std::string Name;
    UINT IndexCount = 0, Instances = 0;
    ComPtr<ID3D11Buffer> B1, T23, T24;
    UINT B1First = 0, B1Num = 0;
    D3D11_SHADER_RESOURCE_VIEW_DESC V23{}, V24{};
    UINT T23Stride = 0, T24Stride = 0, T23Bytes = 0, T24Bytes = 0;
    unsigned Frame = 0;
  };
  std::vector<CrowdProbe> g_CrowdProbes;   // render thread
  int g_CrowdProbesTaken = 0;
  const UINT MaxCrowdProbeBytes = 16u << 20;

  // A staging copy of a buffer, or of part of one (render thread)
  ComPtr<ID3D11Buffer> StagingCopy(ID3D11Buffer* source, UINT from, UINT bytes)
  {
    ComPtr<ID3D11Device> device;
    g_Immediate->GetDevice(device.GetAddressOf());
    D3D11_BUFFER_DESC d{};
    d.ByteWidth = bytes;
    d.Usage = D3D11_USAGE_STAGING;
    d.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Buffer> staging;
    if (!device || FAILED(device->CreateBuffer(&d, nullptr, staging.GetAddressOf())))
      return nullptr;
    D3D11_BOX box{ from, 0, 0, from + bytes, 1, 1 };
    g_Immediate1->CopySubresourceRegion(staging.Get(), 0, 0, 0, 0, source, 0, &box);
    return staging;
  }

  void TakeCrowdProbe(char const* name, UINT indexCount, UINT instances)
  {
    CrowdProbe probe;
    probe.Name = name;
    probe.IndexCount = indexCount;
    probe.Instances = instances;
    probe.Frame = g_Frame;
    ID3D11Buffer* cb = nullptr;
    UINT first = 0, num = 0;
    g_Immediate1->VSGetConstantBuffers1(1, 1, &cb, &first, &num);
    if (cb)
    {
      D3D11_BUFFER_DESC d{};
      cb->GetDesc(&d);
      UINT bytes = (num ? num : 16) * 16;
      if ((size_t)first * 16 + bytes <= d.ByteWidth)
      {
        probe.B1 = StagingCopy(cb, first * 16, bytes);
        probe.B1First = first;
        probe.B1Num = num;
      }
      cb->Release();
    }
    ID3D11ShaderResourceView* views[2] = {};
    g_Immediate1->VSGetShaderResources(23, 2, views);
    for (int i = 0; i < 2; ++i)
    {
      if (!views[i])
        continue;
      D3D11_SHADER_RESOURCE_VIEW_DESC vd{};
      views[i]->GetDesc(&vd);
      ComPtr<ID3D11Resource> res;
      views[i]->GetResource(res.GetAddressOf());
      ComPtr<ID3D11Buffer> buffer;
      if (res && SUCCEEDED(res.As(&buffer)) && buffer)
      {
        D3D11_BUFFER_DESC d{};
        buffer->GetDesc(&d);
        if (d.ByteWidth && d.ByteWidth <= MaxCrowdProbeBytes)
        {
          (i ? probe.T24 : probe.T23) = StagingCopy(buffer.Get(), 0, d.ByteWidth);
          (i ? probe.V24 : probe.V23) = vd;
          (i ? probe.T24Stride : probe.T23Stride) = d.StructureByteStride;
          (i ? probe.T24Bytes : probe.T23Bytes) = d.ByteWidth;
        }
      }
      views[i]->Release();
    }
    g_CrowdProbes.push_back(std::move(probe));
  }

  // Render thread, each Present: the probes copied a few frames ago, read and logged
  void ReadCrowdProbes()
  {
    for (size_t i = 0; i < g_CrowdProbes.size();)
    {
      CrowdProbe& p = g_CrowdProbes[i];
      if (g_Frame - p.Frame < 3)
      {
        ++i;
        continue;
      }
      auto map = [](ComPtr<ID3D11Buffer> const& b, std::vector<BYTE>& out, UINT bytes) {
        out.clear();
        if (!b)
          return false;
        D3D11_MAPPED_SUBRESOURCE m{};
        if (FAILED(g_Immediate->Map(b.Get(), 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &m)) || !m.pData)
          return false;
        out.assign((BYTE const*)m.pData, (BYTE const*)m.pData + bytes);
        g_Immediate->Unmap(b.Get(), 0);
        return true;
      };
      std::vector<BYTE> b1, t23, t24;
      bool r1 = map(p.B1, b1, (p.B1Num ? p.B1Num : 16) * 16), r23 = map(p.T23, t23, p.T23Bytes), r24 = map(p.T24, t24, p.T24Bytes);
      if ((p.B1 && !r1) || (p.T23 && !r23) || (p.T24 && !r24))
      {
        if (g_Frame - p.Frame < 120)
        {
          ++i;
          continue;   // not copied yet
        }
      }
      util::log::Write("Replay (diagnostic): crowd probe '%s' (%u indices x %u instances): b1 window at constant %u (%u), t23 %u bytes "
        "(structure %u, view format %d first %u count %u), t24 %u bytes (structure %u, view format %d first %u count %u)",
        p.Name.c_str(), p.IndexCount, p.Instances, p.B1First, p.B1Num, p.T23Bytes, p.T23Stride, (int)p.V23.Format,
        p.V23.Buffer.FirstElement, p.V23.Buffer.NumElements, p.T24Bytes, p.T24Stride, (int)p.V24.Format, p.V24.Buffer.FirstElement,
        p.V24.Buffer.NumElements);
      if (b1.size() >= 32)
      {
        UINT const* u = (UINT const*)b1.data();
        float const* f = (float const*)b1.data();
        util::log::Write("Replay (diagnostic):   b1: %u %u %u %u | %g %g %g %g; next %u %u %u %u", u[0], u[1], u[2], u[3], f[0], f[1],
          f[2], f[3], u[4], u[5], u[6], u[7]);
      }
      // t23 from its view's first element: the first 32 values (32 bits each)
      UINT index23[32] = {};
      int got23 = 0;
      if (!t23.empty())
      {
        UINT stride = p.T23Stride ? p.T23Stride : 4;
        size_t from = (size_t)p.V23.Buffer.FirstElement * stride;
        char text[400];
        size_t at = 0;
        text[0] = 0;
        for (int k = 0; k < 32 && from + (size_t)(k + 1) * 4 <= t23.size(); ++k)
        {
          memcpy(&index23[k], t23.data() + from + (size_t)k * 4, 4);
          ++got23;
          int w = sprintf_s(text + at, sizeof(text) - at, "%s%u", k ? " " : "", index23[k]);
          at += w > 0 ? w : 0;
        }
        util::log::Write("Replay (diagnostic):   t23 from its view's first element: %s", text);
      }
      // t24: its first entries, and the ones t23's first values name
      if (!t24.empty())
      {
        UINT stride = p.T24Stride ? p.T24Stride : 16;
        auto entry = [&](char const* what, size_t index) {
          size_t from = ((size_t)p.V24.Buffer.FirstElement + index) * stride;
          if (from + stride > t24.size())
            return;
          char text[600];
          size_t at = 0;
          text[0] = 0;
          for (UINT k = 0; k < stride / 4 && k < 24; ++k)
          {
            float f;
            memcpy(&f, t24.data() + from + (size_t)k * 4, 4);
            int w = sprintf_s(text + at, sizeof(text) - at, "%s%g", k ? " " : "", f);
            at += w > 0 ? w : 0;
          }
          util::log::Write("Replay (diagnostic):   t24 %s %zu: %s", what, index, text);
        };
        for (size_t k = 0; k < 3; ++k)
          entry("entry", k);
        for (int k = 0; k < got23 && k < 4; ++k)
          entry("entry named by t23", index23[k]);
      }
      g_CrowdProbes.erase(g_CrowdProbes.begin() + i);
    }
  }

  // The world's draw of a mesh not kept, in the log once a mesh (g_Mutex held)
  void NoteWorldNotKept(__int64 vb, char const* why, UINT instances, UINT stride)
  {
    if (!g_VbNames || g_WorldNotKeptLogged.size() >= 120)
      return;
    auto it = g_VbNames->find(vb);
    if (it == g_VbNames->end() || !g_WorldNotKeptLogged.insert(it->second).second)
      return;
    util::log::Write("Replay (diagnostic): the world's draw of '%s' wasn't kept (%s; %u instances of %u float4s)", it->second.c_str(),
      why ? why : "?", instances, stride);
  }

  // A world draw's instances within WorldKeepMetres of the camera it was
  // kept with (the others let go), their middle and reach; false if none is.
  // An instance counts by the nearest its mesh can be: its place less the
  // mesh's reach (meshReach, from its bounding box) times its scale. 2026-10-28:
  // the Release mission's monorail ground (monorailground_release_01) was
  // never kept, missing at all 173 seconds played, a mesh too long for its
  // origin to be near; nor were the elevated roads beside it
  bool KeepNear(Batch const& batch, Recipe& r, char const** why, float meshReach)
  {
    if (batch.CameraPage < 0 || !batch.Pages[batch.CameraPage].Read ||
        (size_t)batch.CameraFirst * 16 + CameraPosOffset + 12 > batch.Pages[batch.CameraPage].Bytes.size() || r.WorldRows.empty())
    {
      *why = "the camera wasn't read";
      return false;
    }
    float camera[3];
    memcpy(camera, batch.Pages[batch.CameraPage].Bytes.data() + (size_t)batch.CameraFirst * 16 + CameraPosOffset, sizeof(camera));
    int k = r.WorldRows[0];
    std::vector<float> kept;
    std::vector<float> places;
    float farthest = 0.f;
    for (UINT i = 0; i < r.Kept; ++i)
    {
      float const* rows = &r.InstanceData[(size_t)i * r.Stride * 4];
      float p[3] = { rows[k * 4 + 3], rows[(k + 1) * 4 + 3], rows[(k + 2) * 4 + 3] };
      float dx = p[0] - camera[0], dy = p[1] - camera[1], dz = p[2] - camera[2];
      // Its scale, at most: the 3 x 3 part's own size (never less than its
      // largest axis)
      float scale2 = 0.f;
      for (int row = 0; row < 3; ++row)
        for (int c = 0; c < 3; ++c)
          scale2 += rows[(k + row) * 4 + c] * rows[(k + row) * 4 + c];
      float reach = meshReach > 0.f && std::isfinite(scale2) ? meshReach * sqrtf(scale2) : 0.f;
      float limit = WorldKeepMetres + reach;
      if (dx * dx + dy * dy + dz * dz > limit * limit)
        continue;
      kept.insert(kept.end(), rows, rows + (size_t)r.Stride * 4);
      places.insert(places.end(), p, p + 3);
      farthest = (std::max)(farthest, reach);
    }
    UINT count = (UINT)(places.size() / 3);
    if (!count)
    {
      *why = "none of its instances near the camera";
      return false;
    }
    float center[3] = {};
    for (UINT i = 0; i < count; ++i)
      for (int c = 0; c < 3; ++c)
        center[c] += places[i * 3 + c] / count;
    float reach = 0.f;
    for (UINT i = 0; i < count; ++i)
    {
      float dx = places[i * 3] - center[0], dy = places[i * 3 + 1] - center[1], dz = places[i * 3 + 2] - center[2];
      reach = (std::max)(reach, sqrtf(dx * dx + dy * dy + dz * dz));
    }
    r.InstanceData.swap(kept);
    r.Kept = count;
    memcpy(r.Center, center, sizeof(center));
    r.MeshReach = farthest;
    r.Radius = reach + farthest;
    return true;
  }

  bool Decode(ID3D11Device* device, Batch const& batch, Recipe& r, char const** why, float meshReach)
  {
    return DecodeWith(device, batch, r, why, !r.Skin && !r.Track, meshReach);
  }

}

// See below (it compares textures, defined with the texture helpers)
static int UpgradeFrom(Recipe& r, Recipe const& c);
static bool LooksAlike(Recipe const& a, Recipe const& b);

namespace
{
  // An instance's place (its transform's rows' places)
  void PlaceOf(Recipe const& r, UINT i, float* out)
  {
    int k = r.WorldRows[0];
    float const* rows = &r.InstanceData[(size_t)i * r.Stride * 4];
    out[0] = rows[k * 4 + 3];
    out[1] = rows[(k + 1) * 4 + 3];
    out[2] = rows[(k + 2) * 4 + 3];
  }

  // Whether a candidate is the same placed objects as a kept one (an
  // instance of each within VariantSameMetres)
  bool SameObjects(Recipe const& kept, Recipe const& c)
  {
    if (kept.WorldRows.empty() || c.WorldRows.empty())
      return false;
    for (UINT i = 0; i < c.Kept; ++i)
    {
      float a[3];
      PlaceOf(c, i, a);
      for (UINT j = 0; j < kept.Kept; ++j)
      {
        float b[3];
        PlaceOf(kept, j, b);
        float dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
        if (dx * dx + dy * dy + dz * dz <= VariantSameMetres * VariantSameMetres)
          return true;
      }
    }
    return false;
  }

  // Whether a candidate looks the same as a kept one wherever it is: its
  // shaders and every texture the same (2026-10-28: a backdrop car driving by
  // was never within VariantSameMetres of where it was kept, 31 of its draws
  // kept as variations of their own; the 22:20 build's textures streamed in
  // bigger are still another texture, so those still are)
  bool SameLook(Recipe const& kept, Recipe const& c)
  {
    if (kept.Vs.Get() != c.Vs.Get() || kept.Ps.Get() != c.Ps.Get())
      return false;
    auto same = [](ID3D11ShaderResourceView* a, ID3D11ShaderResourceView* b)
    {
      if (a == b)
        return true;
      if (!a || !b)
        return false;
      ComPtr<ID3D11Resource> ra, rb;
      a->GetResource(ra.GetAddressOf());
      b->GetResource(rb.GetAddressOf());
      return ra && ra.Get() == rb.Get();
    };
    // (a video slot's texture is its feed's frame then: not the look)
    for (int i = 0; i < PsViewSlots; ++i)
      if (!kept.PsInstanceSlot[i] && !c.PsInstanceSlot[i] && !kept.Feed[i] && !c.Feed[i] &&
          !same(kept.PsViews[i].Get(), c.PsViews[i].Get()))
        return false;
    for (int i = 0; i < VsViewSlots; ++i)
      if (!kept.VsInstanceSlot[i] && !c.VsInstanceSlot[i] && !same(kept.VsViews[i].Get(), c.VsViews[i].Get()))
        return false;
    return true;
  }

  // A kept model's material values as a later draw of the same objects had
  // them (2026-11-02, the user: "gates textures don't move with the doors":
  // the Release gate's sign planes were drawn by the game only where they
  // stand, so their arrows' motion is their material's: the gate's events
  // start two timelines that drive two ShaderParameterEntities, which set
  // values of the signs' material; the replay drew them with the values of
  // the one moment it kept). Its b1s (externalConstants) are kept each time
  // they changed, with the recording's time, and the last time they were
  // the same before a change (so a change isn't spread back over the time
  // they held). At most MaxMaterialSamples a kept draw, MaxMaterialBytes in all.
  void NoteMaterial(Recipe& kept, Recipe const& c)
  {
    if ((kept.VsB1Bytes.empty() && kept.PsB1Bytes.empty()) || c.VsB1Bytes.size() != kept.VsB1Bytes.size() ||
        c.PsB1Bytes.size() != kept.PsB1Bytes.size())
      return;
    auto both = [](Recipe const& r) {
      std::vector<BYTE> b(r.VsB1Bytes);
      b.insert(b.end(), r.PsB1Bytes.begin(), r.PsB1Bytes.end());
      return b;
    };
    if (kept.B1Times.empty())
    {
      kept.B1Times.push_back((float)kept.Clock);
      kept.B1Samples.push_back(both(kept));
    }
    float clock = (float)c.Clock;
    if (clock <= kept.B1Times.back())
      return;
    std::vector<BYTE> now = both(c);
    if (now == kept.B1Samples.back())
    {
      kept.B1SameUntil = clock;
      return;
    }
    if (kept.B1Samples.size() + 2 > MaxMaterialSamples || g_MaterialBytes + 2 * now.size() > MaxMaterialBytes)
      return;
    if (kept.B1SameUntil > kept.B1Times.back())
    {
      kept.B1Times.push_back(kept.B1SameUntil);
      std::vector<BYTE> held = kept.B1Samples.back();
      kept.B1Samples.push_back(std::move(held));
      g_MaterialBytes += now.size();
    }
    kept.B1Times.push_back(clock);
    kept.B1Samples.push_back(std::move(now));
    g_MaterialBytes += kept.B1Samples.back().size();
    kept.B1SameUntil = -1.f;
  }

  // A kept model's b1s at a time of the recording (see NoteMaterial): its
  // plain numbers in a straight line between the samples around it, the
  // others the nearer sample's
  void MaterialAt(Recipe const& r, double time, std::vector<BYTE>& out)
  {
    size_t n = (std::min)(r.B1Times.size(), r.B1Samples.size());
    size_t hi = (size_t)(std::upper_bound(r.B1Times.begin(), r.B1Times.begin() + n, (float)time) - r.B1Times.begin());
    if (hi == 0 || hi >= n)
    {
      out = r.B1Samples[hi == 0 ? 0 : n - 1];
      return;
    }
    std::vector<BYTE> const& a = r.B1Samples[hi - 1];
    std::vector<BYTE> const& b = r.B1Samples[hi];
    float span = r.B1Times[hi] - r.B1Times[hi - 1];
    float f = span > 0.f ? (float)((time - r.B1Times[hi - 1]) / span) : 1.f;
    out = a;
    for (size_t k = 0; k + 4 <= out.size() && k + 4 <= b.size(); k += 4)
    {
      float va, vb;
      memcpy(&va, &a[k], 4);
      memcpy(&vb, &b[k], 4);
      auto plain = [](float v) { return std::isfinite(v) && fabsf(v) < 1e6f && (v == 0.f || fabsf(v) > 1e-20f); };
      if (plain(va) && plain(vb))
      {
        float v = va + (vb - va) * f;
        memcpy(&out[k], &v, 4);
      }
      else if (f > 0.5f)
        memcpy(&out[k], &b[k], 4);
    }
  }

  // A candidate's instances a kept one of the same objects hadn't, taken
  // (at most MaxInstancesKept); its textures (UpgradeFrom). Then the world's
  // middle and reach worked out again.
  void Merge(Recipe& kept, Recipe const& c)
  {
    g_Upgraded += UpgradeFrom(kept, c);
    if (kept.Stride != c.Stride || kept.WorldRows.empty() || c.WorldRows.empty() || kept.WorldRows[0] != c.WorldRows[0])
      return;
    size_t floats = (size_t)kept.Stride * 4;
    for (UINT i = 0; i < c.Kept && kept.Kept < MaxInstancesKept; ++i)
    {
      float a[3];
      PlaceOf(c, i, a);
      bool known = false;
      for (UINT j = 0; j < kept.Kept && !known; ++j)
      {
        float b[3];
        PlaceOf(kept, j, b);
        float dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
        known = dx * dx + dy * dy + dz * dz <= VariantSameMetres * VariantSameMetres;
      }
      if (known)
        continue;
      kept.InstanceData.insert(kept.InstanceData.end(), c.InstanceData.begin() + i * floats, c.InstanceData.begin() + (i + 1) * floats);
      ++kept.Kept;
    }
    if (kept.World && kept.Kept)
    {
      float center[3] = {};
      for (UINT i = 0; i < kept.Kept; ++i)
      {
        float p[3];
        PlaceOf(kept, i, p);
        for (int k = 0; k < 3; ++k)
          center[k] += p[k] / kept.Kept;
      }
      float reach = 0.f;
      for (UINT i = 0; i < kept.Kept; ++i)
      {
        float p[3];
        PlaceOf(kept, i, p);
        float dx = p[0] - center[0], dy = p[1] - center[1], dz = p[2] - center[2];
        reach = (std::max)(reach, sqrtf(dx * dx + dy * dy + dz * dz));
      }
      memcpy(kept.Center, center, sizeof(center));
      kept.MeshReach = (std::max)(kept.MeshReach, c.MeshReach);
      kept.Radius = reach + kept.MeshReach;
    }
  }

  // The world's kept instances not where the game draws one of its mesh now
  // (its places read back: see ProbeEvery); all of them when it doesn't draw
  // that mesh at all
  void InstancesNotTheGames(Recipe const& r, std::vector<float> const* places, std::vector<float>& out)
  {
    out.clear();
    int k = r.WorldRows.empty() ? -1 : r.WorldRows[0];
    size_t floats = (size_t)r.Stride * 4;
    for (UINT i = 0; i < r.Kept; ++i)
    {
      float const* rows = &r.InstanceData[i * floats];
      bool theirs = false;
      if (places && k >= 0)
      {
        float p[3] = { rows[k * 4 + 3], rows[(k + 1) * 4 + 3], rows[(k + 2) * 4 + 3] };
        for (size_t j = 0; j + 3 <= places->size() && !theirs; j += 3)
        {
          float dx = p[0] - (*places)[j], dy = p[1] - (*places)[j + 1], dz = p[2] - (*places)[j + 2];
          theirs = dx * dx + dy * dy + dz * dz <= ProbeSameMetres * ProbeSameMetres;
        }
      }
      if (!theirs)
        out.insert(out.end(), rows, rows + floats);
    }
  }

  // A skinned instance's rows before its bones (12 the game's usual; else
  // the first count from which every row triple is a transform), -1 if none
  int SkinHeaderOf(float const* rows, UINT stride)
  {
    for (int pass = 0; pass < 2; ++pass)
      for (int header = pass == 0 ? 12 : 0; header <= (pass == 0 ? 12 : MaxHeaderRows); ++header)
      {
        if ((pass == 1 && header == 12) || (int)stride < header + 3 || ((int)stride - header) % 3)
          continue;
        bool all = true;
        for (int k = header; k + 3 <= (int)stride && all; k += 3)
          all = IsTransformAt(rows, k);
        if (all)
          return header;
      }
    return -1;
  }

  // Where a skinned instance is: its first bone matrix's place
  void AnchorOf(float const* rows, int header, float* out)
  {
    out[0] = rows[header * 4 + 3];
    out[1] = rows[(header + 1) * 4 + 3];
    out[2] = rows[(header + 2) * 4 + 3];
  }

  float Distance2(float const* a, float const* b)
  {
    float dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
    return dx * dx + dy * dy + dz * dz;
  }

  // Render thread: a mesh's probe in a frame missed one of its draws (see
  // MaxProbesPerPassAll)
  void CutProbe(int id, bool other, unsigned frame)
  {
    std::vector<unsigned>& cutAt = other ? g_ProbeCutOtherAt : g_ProbeCutAt;
    if (id < 0)
      return;
    if ((size_t)id >= cutAt.size())
      cutAt.resize((size_t)id + 1, 0);
    cutAt[id] = frame;
  }

  bool ProbeCut(int id, bool other, unsigned frame)
  {
    std::vector<unsigned> const& cutAt = other ? g_ProbeCutOtherAt : g_ProbeCutAt;
    return (other ? g_ProbeFullOtherAt : g_ProbeFullAt) == frame || (id >= 0 && (size_t)id < cutAt.size() && cutAt[id] == frame);
  }

  // Render thread: the copies read back (a batch once all its copies are)
  void ReadBack(ID3D11Device* device)
  {
    replay_gamedraws::CostTimer timer(replay_gamedraws::Cost_ReadBack);
    std::shared_ptr<std::unordered_map<__int64, float> const> reachOf;
    {
      std::lock_guard<std::mutex> lock(g_Mutex);
      reachOf = g_VbReach;
    }
    for (size_t b = 0; b < g_Batches.size();)
    {
      Batch& batch = g_Batches[b];
      if (g_Frame - batch.Frame < ReadbackAfterFrames)
      {
        ++b;
        continue;
      }
      // Its pages mapped, all of them or none (one the graphics card is still
      // copying into: the batch waits for a later frame, none kept mapped),
      // and read where they're mapped while it's decoded; unmapped and given
      // back after. Until 2026-11-04 each page was copied out whole first
      // (about 1 MB each, 130 - 165 MB a second while it recorded: 0.3 - 0.46
      // ms a frame, up to 12 ms in one; the decode reads a few KB of each)
      bool all = true;
      {
        replay_gamedraws::CostTimer maps(replay_gamedraws::Cost_Maps);
        for (Staged& s : batch.Pages)
        {
          if (s.Read || !s.Staging)
            continue;
          D3D11_MAPPED_SUBRESOURCE m{};
          HRESULT hr = g_Immediate->Map(s.Staging.Get(), 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &m);
          if (hr == DXGI_ERROR_WAS_STILL_DRAWING && g_Frame - batch.Frame < ReadbackGiveUpFrames)
          {
            all = false;
            break;
          }
          if (SUCCEEDED(hr) && m.pData)
          {
            s.Bytes = MappedBytes{ (BYTE const*)m.pData, s.Width };
            s.Read = true;
            s.Mapped = true;
          }
          else
          {
            // Not read (given up on, or the map failed): given back now
            GiveStaging(std::move(s.Staging), s.Capacity ? s.Capacity : s.Width);
            s.Staging.Reset();
            g_StagingBytes -= (std::min)(g_StagingBytes, (size_t)s.Width);
          }
        }
        if (!all)
          for (Staged& s : batch.Pages)
            if (s.Mapped)
            {
              g_Immediate->Unmap(s.Staging.Get(), 0);
              s.Mapped = false;
              s.Read = false;
              s.Bytes = MappedBytes{};
            }
      }
      if (!all)
      {
        ++b;
        continue;
      }
      // Where the main view looked then (see ViewSample)
      if (g_Capturing.load(std::memory_order_relaxed))
        NoteView(batch);
      for (auto& recipe : batch.Recipes)
      {
        char const* why = "";
        float meshReach = 0.f;
        if (recipe->World && reachOf)
        {
          auto found = reachOf->find(std::get<0>(recipe->WorldKey));
          if (found != reachOf->end())
            meshReach = found->second;
        }
        bool ok = Decode(device, batch, *recipe, &why, meshReach);
        if (recipe->Skin)
        {
          // A person's: each instance a candidate for FaithReplay (TakeSkinCandidates)
          if (!ok)
            continue;
          std::lock_guard<std::mutex> lock(g_Mutex);
          UINT count = recipe->Kept < MaxSkinInstances ? recipe->Kept : MaxSkinInstances;
          for (UINT i = 0; i < count && (int)g_Skins.size() < MaxSkins; ++i)
          {
            replay_gamedraws::SkinCandidate c{ g_NextSkinId++, recipe->Clock, (__int64)recipe->Vbs[0].Get(), recipe->StartIndex,
              recipe->IndexCount, recipe->Stride, {} };
            c.PassKey = recipe->Pass == Pass_Main ? 0 : recipe->Pass * MaxPassOrdinals + recipe->PassOrdinal + 1;
            c.Instance.assign(recipe->InstanceData.begin() + (size_t)i * recipe->Stride * 4,
              recipe->InstanceData.begin() + (size_t)(i + 1) * recipe->Stride * 4);
            g_Skins[c.Id] = recipe;
            g_SkinCandidates.push_back(std::move(c));
          }
          continue;
        }
        if (recipe->Probe)
        {
          // The game's own instances' places of the mesh: every draw of it in
          // the frame it was probed in, together (see MaxProbesPerPassAll);
          // none from a frame one of its draws is missing from
          std::lock_guard<std::mutex> lock(g_Mutex);
          auto& known = recipe->ProbeOther ? g_GamePlacesOther : g_GamePlaces;
          if (!ok || recipe->WorldRows.empty())
            CutProbe(recipe->MeshId, recipe->ProbeOther, batch.Frame);
          if (ProbeCut(recipe->MeshId, recipe->ProbeOther, batch.Frame))
          {
            auto entry = known.find(recipe->MeshId);
            if (entry != known.end() && entry->second.first == batch.Frame)
              known.erase(entry);
            continue;
          }
          int k = recipe->WorldRows[0];
          std::vector<float> places;
          for (UINT i = 0; i < recipe->Kept; ++i)
          {
            float const* rows = &recipe->InstanceData[(size_t)i * recipe->Stride * 4];
            float p[3] = { rows[k * 4 + 3], rows[(k + 1) * 4 + 3], rows[(k + 2) * 4 + 3] };
            places.insert(places.end(), p, p + 3);
          }
          // (A batch may be read back after a later one: a frame older than
          // the places kept is left out)
          auto& entry = known[recipe->MeshId];
          if (batch.Frame > entry.first)
            entry = { batch.Frame, std::move(places) };
          else if (batch.Frame == entry.first)
            entry.second.insert(entry.second.end(), places.begin(), places.end());
          continue;
        }
        if (recipe->Track)
        {
          std::lock_guard<std::mutex> lock(g_Mutex);
          auto it = g_Tracks.find(recipe->WorldKey);
          if (it == g_Tracks.end() || !ok)
            continue;
          Track& t = it->second;
          // A cloth's vertices as copied with it (see MaxClothBytes)
          Staged const* clothPage = recipe->Cloth ? PageOf(batch, recipe->ClothSource) : nullptr;
          if (recipe->Cloth && (!clothPage || clothPage->Bytes.size() <= recipe->Offsets[recipe->ClothSlot] ||
              (t.ClothBytes && clothPage->Bytes.size() != (size_t)t.ClothOffset + t.ClothBytes)))
            continue;
          if (!t.R)
          {
            // Its first sample with its constant buffers made: its state
            if (recipe->SampleOnly)
              continue;
            t.R = recipe;
            if (recipe->Cloth)
            {
              // Which 4 bytes of a vertex are plain numbers everywhere (blended between samples)
              t.Cloth = true;
              t.Header = -1;
              UINT stride = recipe->Strides[recipe->ClothSlot], offset = recipe->Offsets[recipe->ClothSlot];
              t.ClothOffset = offset;
              t.ClothBytes = (UINT)clothPage->Bytes.size() - offset;
              UINT columns = (std::min)(stride / 4, 32u);
              t.ClothColumns = columns >= 32 ? 0xFFFFFFFFu : (1u << columns) - 1;
              for (size_t at = 0; stride && at + stride <= t.ClothBytes && t.ClothColumns; at += stride)
                for (UINT c = 0; c < columns; ++c)
                {
                  float v;
                  memcpy(&v, clothPage->Bytes.data() + offset + at + c * 4, 4);
                  if (!(std::isfinite(v) && fabsf(v) < 1e6f && (v == 0.f || fabsf(v) > 1e-20f)))
                    t.ClothColumns &= ~(1u << c);
                }
              for (int s = 0; s < VbSlots && !t.MeshVb; ++s)
                if (s != recipe->ClothSlot && recipe->Vbs[s])
                  t.MeshVb = (__int64)recipe->Vbs[s].Get();
              recipe->Ready = true;
              if (g_ClothLogs++ < 16)
                util::log::Write("Replay: a cloth's draw kept as a track (%u indices; its vertices in slot %d, %u bytes, stride %u, offset "
                  "%u; plain numbers, blended between samples, in its 4-byte columns 0x%X): its vertices copied with each sample",
                  recipe->IndexCount, recipe->ClothSlot, t.ClothBytes, stride, offset, t.ClothColumns);
            }
            else
            {
              t.Header = SkinHeaderOf(recipe->InstanceData.data(), recipe->Stride);
              recipe->Ready = t.Header >= 0;
            }
          }
          if (recipe->Stride != t.R->Stride || !t.R->Ready || (!t.Times.empty() && recipe->Clock <= t.Times.back()))
            continue;
          size_t floats = (size_t)recipe->Kept * recipe->Stride * 4;
          if (g_TrackBytes + floats * sizeof(float) > MaxTrackBytes)
          {
            if (!g_TracksFullLogged)
              util::log::Write("Replay: the %u MB kept for the other skinned draws' tracks are used: no more samples kept",
                (UINT)(MaxTrackBytes >> 20));
            g_TracksFullLogged = true;
            continue;
          }
          if (t.Cloth && g_ClothBytes + t.ClothBytes > MaxClothBytes)
          {
            if (!g_ClothFullLogged)
              util::log::Write("Replay: the %u MB kept for the cloths' vertices are used: no more cloth samples kept",
                (UINT)(MaxClothBytes >> 20));
            g_ClothFullLogged = true;
            continue;
          }
          try
          {
            if (t.Cloth)
            {
              t.ClothStarts.push_back(t.ClothData.size());
              t.ClothData.insert(t.ClothData.end(), clothPage->Bytes.begin() + t.ClothOffset, clothPage->Bytes.end());
              g_ClothBytes += t.ClothBytes;
            }
            t.Starts.push_back(t.Data.size());
            t.Data.insert(t.Data.end(), recipe->InstanceData.begin(), recipe->InstanceData.begin() + floats);
            t.Times.push_back(recipe->Clock);
            t.Counts.push_back(recipe->Kept);
            g_TrackBytes += floats * sizeof(float);
          }
          catch (std::bad_alloc const&)
          {
            t.Starts.resize(t.Times.size());
            if (t.Cloth)
            {
              t.ClothStarts.resize(t.Times.size());
              t.ClothData.resize(t.ClothStarts.size() * t.ClothBytes);
            }
          }
          if (recipe != t.R)
            recipe->InstanceData.clear();
          continue;
        }
        if (recipe->World)
        {
          if (ok)
            ok = KeepNear(batch, *recipe, &why, meshReach);
          std::lock_guard<std::mutex> lock(g_Mutex);
          // A later draw of a kept part: merged into the one of the same
          // objects, else another variation of its own
          if (recipe->Candidate)
          {
            if (!ok)
              continue;
            WorldKeyType base = recipe->WorldKey;
            std::shared_ptr<Recipe> same;
            int variants = 0;
            for (int v = 0; v < MaxVariants; ++v)
            {
              auto it = g_World.find(WorldKeyType(std::get<0>(base), std::get<1>(base), std::get<2>(base) | ((UINT)v << 24)));
              if (it == g_World.end())
                break;
              ++variants;
              if (!same && it->second->Ready && (SameObjects(*it->second, *recipe) || SameLook(*it->second, *recipe)))
                same = it->second;
            }
            if (same)
            {
              if (same->Vs.Get() == recipe->Vs.Get() && same->Ps.Get() == recipe->Ps.Get())
                Merge(*same, *recipe);
            }
            else if (variants && variants < MaxVariants && (int)g_World.size() < MaxWorld)
            {
              WorldKeyType key(std::get<0>(base), std::get<1>(base), std::get<2>(base) | ((UINT)variants << 24));
              recipe->Candidate = false;
              recipe->WorldKey = key;
              recipe->Ready = true;
              // Its mesh as the first one's (the same buffer)
              auto first = g_World.find(base);
              if (first != g_World.end())
              {
                recipe->Mesh = first->second->Mesh;
                recipe->Lod = first->second->Lod;
                recipe->MeshId = first->second->MeshId;
              }
              g_World[key] = recipe;
            }
            continue;
          }
          auto current = g_World.find(recipe->WorldKey);
          if (current == g_World.end() || current->second != recipe)
            continue;
          if (ok)
            recipe->Ready = true;
          else
          {
            // Tried again in a later frame (MaxAttempts times; not counted
            // before the meshes' reach is known: see g_VbReach)
            if (reachOf)
              ++g_WorldAttempts[recipe->WorldKey];
            NoteWorldNotKept(std::get<0>(recipe->WorldKey), why, recipe->Kept, recipe->Stride);
            g_World.erase(current);
          }
          continue;
        }
        long long key = KeyOf(recipe->Shape, recipe->Section);
        if (recipe->Pass != Pass_Main)
          key |= ((long long)(recipe->Pass * MaxPassOrdinals + recipe->PassOrdinal + 1)) << 48;
        std::lock_guard<std::mutex> lock(g_Mutex);
        // Where the game drew it then (see Sighting)
        if (ok)
          NoteSighting(*recipe);
        // A later draw of a kept part (see VariantSameMetres): merged into the
        // one of the same objects, else another variation of its own (its
        // number from bit 40 of its key)
        if (recipe->Candidate)
        {
          if (!ok)
            continue;
          std::shared_ptr<Recipe> same;
          int variants = 0;
          // (A Dash's marker: only the same marker's draws are merged, see MaxRvVariants)
          bool marker = recipe->Pass == Pass_Rv;
          int most = marker ? MaxRvVariants : MaxVariants;
          for (int v = 0; v < most; ++v)
          {
            auto it = g_Recipes.find(key | ((long long)v << 40));
            if (it == g_Recipes.end())
              break;
            ++variants;
            if (!same && it->second->Ready && (SameObjects(*it->second, *recipe) || (!marker && SameLook(*it->second, *recipe))))
              same = it->second;
          }
          if (same)
          {
            if (same->Vs.Get() == recipe->Vs.Get() && same->Ps.Get() == recipe->Ps.Get())
            {
              // Its material values then, only from a draw of the same objects
              bool objects = SameObjects(*same, *recipe);
              Merge(*same, *recipe);
              if (objects)
                NoteMaterial(*same, *recipe);
            }
          }
          else if (variants && variants < most && (int)g_Recipes.size() < MaxRecipes)
          {
            recipe->Candidate = false;
            recipe->Ready = true;
            g_Recipes[key | ((long long)variants << 40)] = recipe;
            if (g_RecipeLogs++ < MaxRecipeLogs)
              util::log::Write("Replay: '%s' section %d: another variation of it kept as the game draws it (%u instances)",
                recipe->Name, recipe->Section, recipe->Kept);
          }
          continue;
        }
        // Forgotten meanwhile (a new recording): not counted
        auto current = g_Recipes.find(key);
        if (current == g_Recipes.end() || current->second != recipe)
          continue;
        if (ok)
        {
          recipe->Ready = true;
          int ready = ++g_Ready;
          if (g_RecipeLogs++ < MaxRecipeLogs)
            util::log::Write("Replay: '%s' section %d kept as the game draws it (recipe %d: %u indices, %u instances of %u float4s, "
              "its transform at row %d%s)", recipe->Name, recipe->Section, ready, recipe->IndexCount, recipe->Kept, recipe->Stride,
              recipe->WorldRows[0], recipe->WorldRows.size() > 1 ? " and more" : "");
        }
        else
        {
          int attempts = ++g_Attempts[key];
          if (g_FailLogs++ < MaxFailLogs)
            util::log::Write("Replay: '%s' section %d: the game's draw of it couldn't be kept (%s; attempt %d of %d)", recipe->Name,
              recipe->Section, why, attempts, MaxAttempts);
          if (attempts < MaxAttempts)
            g_Recipes.erase(key);
          else
            for (auto& p : recipe->Pages)
              p.Reset();
        }
      }
      // Its pages unmapped and given back
      for (Staged& s : batch.Pages)
      {
        if (s.Mapped)
        {
          g_Immediate->Unmap(s.Staging.Get(), 0);
          s.Mapped = false;
          s.Bytes = MappedBytes{};
        }
        if (s.Staging)
        {
          GiveStaging(std::move(s.Staging), s.Capacity ? s.Capacity : s.Width);
          s.Staging.Reset();
          g_StagingBytes -= (std::min)(g_StagingBytes, (size_t)s.Width);
        }
      }
      g_Batches.erase(g_Batches.begin() + b);
    }
  }

  // The instance buffer the tools' draws read, big enough
  bool EnsureInstances(ID3D11Device* device, UINT float4s)
  {
    if (g_Instances && g_InstancesCapacity >= float4s)
      return true;
    UINT capacity = 1024;
    while (capacity < float4s)
      capacity *= 2;
    g_Instances.Reset();
    g_InstancesView.Reset();
    D3D11_BUFFER_DESC d{};
    d.ByteWidth = capacity * 16;
    d.Usage = D3D11_USAGE_DYNAMIC;
    d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    d.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(device->CreateBuffer(&d, nullptr, g_Instances.GetAddressOf())))
      return false;
    D3D11_SHADER_RESOURCE_VIEW_DESC v{};
    v.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    v.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    v.Buffer.FirstElement = 0;
    v.Buffer.NumElements = capacity;
    if (FAILED(device->CreateShaderResourceView(g_Instances.Get(), &v, g_InstancesView.GetAddressOf())))
    {
      g_Instances.Reset();
      return false;
    }
    g_InstancesCapacity = capacity;
    return true;
  }

  // A b0 of the tools': the recipe's bytes with its instances at start
  bool FillConstants(ID3D11Buffer* buffer, std::vector<BYTE> const& bytes, int offsetsAt, UINT start, UINT stride)
  {
    D3D11_MAPPED_SUBRESOURCE m{};
    if (FAILED(g_Immediate->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &m)) || !m.pData)
      return false;
    memcpy(m.pData, bytes.data(), bytes.size());
    if (offsetsAt >= 0 && (size_t)offsetsAt + 16 <= bytes.size())
    {
      int offsets[4] = { (int)start, (int)stride, (int)start, (int)stride };
      memcpy((BYTE*)m.pData + offsetsAt, offsets, sizeof(offsets));
    }
    g_Immediate->Unmap(buffer, 0);
    return true;
  }

  // The end of the main view's G-buffer pass while the replay plays: the
  // recorded models the game doesn't have drawn with their recipes
  // The replay's own drawing's time (see TimingLogFrames)
  struct DrawTiming
  {
    LARGE_INTEGER Start;
    DrawTiming() { QueryPerformanceCounter(&Start); }
    ~DrawTiming()
    {
      LARGE_INTEGER end;
      QueryPerformanceCounter(&end);
      g_DrawTicks += end.QuadPart - Start.QuadPart;
      ++g_DrawFrames;
    }
  };

  // A model's own instance in a kept draw of its shape: the nearest within
  // InstanceMatchMetres of where it is, and how far (squared; InstanceMatchMetres
  // squared when there's none), worked out once (render thread)
  // A cloth's vertices' middle at a sample (each vertex's first 12 bytes: its
  // place), in the world's space when local is false, else through its
  // instance's transform; false when its places aren't plain numbers
  bool ClothMiddle(Track const& t, size_t k, bool local, float* out)
  {
    if (!t.R || t.R->ClothSlot < 0 || t.R->ClothSlot >= VbSlots || k >= t.ClothStarts.size() || (t.ClothColumns & 7) != 7)
      return false;
    UINT stride = t.R->Strides[t.R->ClothSlot];
    if (stride < 12 || t.ClothStarts[k] + t.ClothBytes > t.ClothData.size())
      return false;
    double sum[3] = {};
    size_t n = 0;
    for (size_t at = 0; at + 12 <= t.ClothBytes; at += stride)
    {
      float p[3];
      memcpy(p, &t.ClothData[t.ClothStarts[k] + at], sizeof(p));
      sum[0] += p[0];
      sum[1] += p[1];
      sum[2] += p[2];
      ++n;
    }
    if (!n)
      return false;
    float middle[3] = { (float)(sum[0] / n), (float)(sum[1] / n), (float)(sum[2] / n) };
    if (!local)
    {
      memcpy(out, middle, sizeof(middle));
      return true;
    }
    if (t.R->WorldRows.empty() || k >= t.Counts.size() || !t.Counts[k])
      return false;
    float const* rows = &t.Data[t.Starts[k]];
    int w = t.R->WorldRows[0];
    for (int row = 0; row < 3; ++row)
      out[row] = rows[(w + row) * 4 + 0] * middle[0] + rows[(w + row) * 4 + 1] * middle[1] + rows[(w + row) * 4 + 2] * middle[2] +
        rows[(w + row) * 4 + 3];
    return true;
  }

  // a x b for 3x4 matrices (rows of a turn, the place last; a point x goes to
  // turn x + place)
  void Mul3x4(float const* a, float const* b, float* out)
  {
    for (int r = 0; r < 3; ++r)
    {
      for (int c = 0; c < 4; ++c)
        out[r * 4 + c] = a[r * 4 + 0] * b[0 * 4 + c] + a[r * 4 + 1] * b[1 * 4 + c] + a[r * 4 + 2] * b[2 * 4 + c];
      out[r * 4 + 3] += a[r * 4 + 3];
    }
  }

  // A cloth with no sample near the time (see AnchorCloths): its sample
  // nearest the time with its owner's bone known, moved as that bone moved
  // since (its vertices' places, or its instance's transform when they're in
  // its own space; their other numbers, its normals' among them, as they
  // were). Under g_Mutex. False when its owner isn't drawn now (no bone of
  // theirs given by SetClothAnchors)
  bool CarriedCloth(Track const& t, double time, std::vector<float>& out, std::vector<BYTE>& vertices)
  {
    if (!t.Owner || t.OwnerBone < 0 || !t.R || t.R->ClothSlot < 0 || t.R->ClothSlot >= VbSlots)
      return false;
    auto now = g_ClothAnchorsNow.find(std::make_pair(t.Owner, t.OwnerBone));
    if (now == g_ClothAnchorsNow.end())
      return false;
    size_t samples = (std::min)((std::min)(t.Times.size(), t.ClothStarts.size()), t.AnchorPoses.size() / 12);
    size_t best = samples;
    double bestAway = 1e30;
    for (size_t k = 0; k < samples; ++k)
    {
      if (!std::isfinite(t.AnchorPoses[k * 12]) || t.ClothStarts[k] + t.ClothBytes > t.ClothData.size())
        continue;
      double away = fabs(t.Times[k] - time);
      if (away < bestAway)
      {
        bestAway = away;
        best = k;
      }
    }
    if (best == samples)
      return false;
    // now x the inverse of then
    float const* then = &t.AnchorPoses[best * 12];
    float inverse[12], move[12];
    for (int r = 0; r < 3; ++r)
      for (int c = 0; c < 3; ++c)
        inverse[r * 4 + c] = then[c * 4 + r];
    for (int r = 0; r < 3; ++r)
      inverse[r * 4 + 3] = -(inverse[r * 4 + 0] * then[3] + inverse[r * 4 + 1] * then[7] + inverse[r * 4 + 2] * then[11]);
    Mul3x4(now->second.data(), inverse, move);
    size_t floats = (size_t)t.R->Stride * 4;
    out.assign(t.Data.begin() + t.Starts[best], t.Data.begin() + t.Starts[best] + t.Counts[best] * floats);
    vertices.assign(t.ClothData.begin() + t.ClothStarts[best], t.ClothData.begin() + t.ClothStarts[best] + t.ClothBytes);
    if (t.ClothLocal)
    {
      if (t.R->WorldRows.empty())
        return false;
      size_t w = (size_t)t.R->WorldRows[0] * 4;
      for (UINT i = 0; i < t.Counts[best]; ++i)
      {
        float* rows = &out[i * floats + w];
        float was[12];
        memcpy(was, rows, sizeof(was));
        Mul3x4(move, was, rows);
      }
      return true;
    }
    UINT stride = t.R->Strides[t.R->ClothSlot];
    if ((t.ClothColumns & 7) != 7 || stride < 12)
      return false;
    for (size_t at = 0; at + stride <= t.ClothBytes; at += stride)
    {
      float v[3], o[3];
      memcpy(v, &vertices[at], sizeof(v));
      for (int r = 0; r < 3; ++r)
        o[r] = move[r * 4 + 0] * v[0] + move[r * 4 + 1] * v[1] + move[r * 4 + 2] * v[2] + move[r * 4 + 3];
      memcpy(&vertices[at], o, sizeof(o));
    }
    return true;
  }

  // A place on a model's path (4 floats a point: time, x y z) at a time, in a
  // straight line between its points, held at its ends
  void PathAt(std::vector<float> const& path, double time, float* out)
  {
    size_t points = path.size() / 4;
    if (!points)
      return;
    size_t hi = 0;
    while (hi < points && path[hi * 4] < time)
      ++hi;
    if (hi == 0 || hi == points)
    {
      size_t k = hi == 0 ? 0 : points - 1;
      memcpy(out, &path[k * 4 + 1], 3 * sizeof(float));
      return;
    }
    float const* a = &path[(hi - 1) * 4];
    float const* b = &path[hi * 4];
    float f = b[0] > a[0] ? (float)((time - a[0]) / (b[0] - a[0])) : 0.f;
    for (int i = 0; i < 3; ++i)
      out[i] = a[1 + i] + (b[1 + i] - a[1 + i]) * f;
  }

  // (A model with a path, see replay_shapes::Draw: matched where it was when
  // the draw was kept, so the answer holds whenever it's asked)
  // (within: how far its instance may be; one entity is always asked with the same)
  std::pair<int, float> const& OwnInstance(Recipe& r, __int64 entity, float const* transform, std::vector<float> const* path = nullptr,
    float within = InstanceMatchMetres)
  {
    auto found = r.ByEntity.find(entity);
    if (found != r.ByEntity.end())
      return found->second;
    float then[3] = { transform[12], transform[13], transform[14] };
    if (path && !path->empty())
      PathAt(*path, r.Clock, then);
    int instance = 0;
    float best = within * within;
    if (!r.WorldRows.empty())
    {
      int k = r.WorldRows[0];
      for (UINT i = 0; i < r.Kept; ++i)
      {
        float const* rows = &r.InstanceData[(size_t)i * r.Stride * 4];
        float dx = rows[k * 4 + 3] - then[0], dy = rows[(k + 1) * 4 + 3] - then[1], dz = rows[(k + 2) * 4 + 3] - then[2];
        float d2 = dx * dx + dy * dy + dz * dz;
        if (d2 < best)
        {
          best = d2;
          instance = (int)i;
        }
      }
    }
    return r.ByEntity.emplace(entity, std::make_pair(instance, best)).first->second;
  }

  // (lastOrdinal: the kept draws of the numbers ordinal to lastOrdinal of the
  // pass's kind, see g_PassFewest; < 0: ordinal's only)
  void DrawRecipes(int pass, int ordinal, int lastOrdinal = -1)
  {
    int lastOne = lastOrdinal < 0 ? ordinal : lastOrdinal;
    auto inPass = [pass, ordinal, lastOne](int p, int o) { return p == pass && o >= ordinal && o <= lastOne; };
    DrawTiming timing;
    bool mainPass = pass == Pass_Main;
    std::vector<replay_shapes::Draw> draws;
    std::map<int, std::vector<std::shared_ptr<Recipe>>> byShape;
    // Those shapes' kept draws in every pass (whether the game drew a model at all)
    std::map<int, std::vector<std::shared_ptr<Recipe>>> allOfShape;
    std::set<int> wholeLevel;   // shapes drawn whole from another level's kept draws
    // People (stage 2): each draw's recipe and its instance as FaithReplay posed it
    std::vector<std::pair<std::shared_ptr<Recipe>, std::vector<float>>> people;
    // The world's own (see WorldCaptureEvery): the ones whose mesh the game
    // didn't draw lately, near the replayed Faith, the nearest first
    struct WorldDraw
    {
      float Away;
      std::shared_ptr<Recipe> R;
      std::vector<float> Instances;
    };
    std::vector<WorldDraw> world;
    // The other skinned draws': each track's instances now
    std::vector<std::pair<std::shared_ptr<Recipe>, std::vector<float>>> tracked;
    std::map<Recipe*, std::vector<BYTE>> clothNow;   // the cloths' vertices now, by their recipe
    float center[3] = {};   // the replayed Faith's place (for the log)
    double playTime = -1.0; // the time played (the recording's)
    {
      std::lock_guard<std::mutex> lock(g_Mutex);
      memcpy(center, g_WorldCenter, sizeof(center));
      playTime = g_TrackTime;
      bool worldNow = g_WorldPlaying && g_WorldOn && !g_World.empty();
      bool tracksNow = mainPass && g_WorldPlaying && g_TracksOn && !g_Tracks.empty() && g_TrackTime >= 0.0;
      if (g_Draws.empty() && g_PeopleDraws.empty() && !worldNow && !tracksNow)
        return;
      if (worldNow)
        for (auto const& entry : g_World)
        {
          Recipe const& r = *entry.second;
          if (!r.Ready || r.MeshId < 0 || !r.Kept || !r.Stride || !inPass(r.Pass, r.PassOrdinal))
            continue;
          float dx = r.Center[0] - g_WorldCenter[0], dy = r.Center[1] - g_WorldCenter[1], dz = r.Center[2] - g_WorldCenter[2];
          float away = sqrtf(dx * dx + dy * dy + dz * dz) - r.Radius;
          if (away > WorldDrawMetres)
            continue;
          // The game draws its mesh: only the ones where it doesn't draw one (its
          // places read back lately), none until they're known. A draw kept
          // in the decal, lit or half-size passes: by the game's drawing in
          // those too (2026-11-08: a glass part is drawn only in the lit
          // picture's passes, so the game was never seen drawing it and the
          // replay drew its kept glass over the game's own, a second layer)
          std::vector<float> const* places = nullptr;
          std::vector<float> both;
          bool unknown = false;
          for (int other = 0; other < (r.Pass == Pass_Main ? 1 : 2); ++other)
          {
            std::vector<unsigned> const& drawnAt = other ? g_MeshDrawnOtherAt : g_MeshDrawnAt;
            if (r.MeshId >= (int)drawnAt.size() || !drawnAt[r.MeshId] || g_Frame - drawnAt[r.MeshId] > WorldGameDrewFrames)
              continue;
            auto const& placesOf = other ? g_GamePlacesOther : g_GamePlaces;
            auto known = placesOf.find(r.MeshId);
            if (known == placesOf.end() || g_Frame - known->second.first > ProbeFreshFrames)
            {
              unknown = true;
              break;
            }
            if (!places)
              places = &known->second.second;
            else
            {
              both.assign(places->begin(), places->end());
              both.insert(both.end(), known->second.second.begin(), known->second.second.end());
              places = &both;
            }
          }
          if (unknown)
            continue;
          std::vector<float> kept;
          InstancesNotTheGames(r, places, kept);
          if (!kept.empty())
            world.push_back({ away, entry.second, std::move(kept) });
        }
      for (auto const& entry : g_Recipes)
        if (entry.second->Ready && inPass(entry.second->Pass, entry.second->PassOrdinal))
          byShape[entry.second->Shape].push_back(entry.second);
      // The models this frame: only looked at for the shapes with recipes in this pass
      if (!byShape.empty())
      {
        draws = g_Draws;
        for (auto const& entry : g_Recipes)
          if (entry.second->Ready && byShape.count(entry.second->Shape))
            allOfShape[entry.second->Shape].push_back(entry.second);
      }
      // A model's parts kept at its shape's level; or, when none was (the
      // game drew it only at another level), the most detailed other level
      // whose every part was kept (its part count in bits 6 - 11, see
      // replay_shapes' CaptureSection), as a whole (its shape isn't drawn
      // then: see replay_shapes::Apply). A level not whole isn't drawn (the
      // shape is: 2026-10-27 22:53, "half of the mesh is missing").
      for (auto& entry : byShape)
      {
        auto& list = entry.second;
        bool own = false;
        std::map<int, std::set<int>> numbers;   // level -> its parts kept
        std::map<int, int> parts;               // level -> its parts
        for (auto const& r : list)
        {
          if (r->Section < 0x8000)
          {
            own = true;
            continue;
          }
          int lod = (r->Section >> 12) & 7;
          numbers[lod].insert(r->Section & 0x3F);
          parts[lod] = (r->Section >> 6) & 0x3F;
        }
        int best = -1;
        if (mainPass)
        {
          for (auto const& level : numbers)
            if (parts[level.first] > 0 && (int)level.second.size() >= parts[level.first])
            {
              best = level.first;
              break;   // the map's first: the most detailed
            }
          g_ChosenLevel[entry.first] = own ? -1 : best;
        }
        else
        {
          // Another pass's: at the level the G-buffer pass drew the model at
          auto chosen = g_ChosenLevel.find(entry.first);
          best = chosen != g_ChosenLevel.end() ? chosen->second : -1;
          own = best < 0;
        }
        list.erase(std::remove_if(list.begin(), list.end(), [own, best](std::shared_ptr<Recipe> const& r) {
          return r->Section >= 0x8000 ? (own || ((r->Section >> 12) & 7) != best) : (!own && best >= 0);
        }), list.end());
        if (!own && best >= 0 && !list.empty())
          wholeLevel.insert(entry.first);
      }
      // People, each draw in the pass it was kept in (the G-buffer pass's;
      // the lit pass's see-through parts, see CapturingSkins)
      for (auto const& d : g_PeopleDraws)
      {
        auto it = g_Skins.find(d.Id);
        if (it != g_Skins.end() && it->second->Stride && d.Instance.size() == (size_t)it->second->Stride * 4 &&
            inPass(it->second->Pass, it->second->PassOrdinal))
          people.push_back({ it->second, d.Instance });
      }
      // The other skinned draws' tracks at the time played (see TrackSampleSeconds)
      if (tracksNow)
      {
        // The posed ones' places, by their mesh part
        std::map<WorldKeyType, std::vector<float>> posedAt;
        for (auto const& p : people)
        {
          int header = SkinHeaderOf(p.second.data(), p.first->Stride);
          if (header < 0)
            continue;
          float anchor[3];
          AnchorOf(p.second.data(), header, anchor);
          auto& list = posedAt[WorldKeyType((__int64)p.first->Vbs[0].Get(), p.first->StartIndex, p.first->IndexCount)];
          list.insert(list.end(), anchor, anchor + 3);
        }
        double time = g_TrackTime;
        for (auto const& entry : g_Tracks)
        {
          Track const& t = entry.second;
          if (!t.R || !t.R->Ready || t.Faith || (t.Header < 0 && !t.Cloth) || t.Times.empty())
            continue;
          // A cloth's (see MaxClothBytes): its instances in their order, its
          // vertices from the samples around the time; with none near it,
          // carried by the person it's on (see AnchorCloths). 2026-11-02, the
          // user: "when faith isn't around icarus anymore the hoodie is gone":
          // the game draws a cloth only while it's in sight (Icarus's samples
          // ended at 90.4 s, he was recorded to 103.9 s, with gaps between)
          if (t.Cloth)
          {
            size_t floats = (size_t)t.R->Stride * 4;
            size_t samples = (std::min)(t.Times.size(), t.ClothStarts.size());
            if (!samples || t.ClothStarts[samples - 1] + t.ClothBytes > t.ClothData.size())
              continue;
            bool early = time < t.Times.front();
            size_t a = early ? 0 : (std::min)((size_t)(std::upper_bound(t.Times.begin(), t.Times.end(), time) - t.Times.begin()) - 1,
              samples - 1);
            bool next = !early && a + 1 < samples && t.Times[a + 1] - t.Times[a] <= TrackHoldSeconds;
            if (early || (!next && time - t.Times[a] > TrackHoldSeconds / 2))
            {
              std::vector<float> out;
              std::vector<BYTE> vertices;
              if (CarriedCloth(t, time, out, vertices))
              {
                clothNow[t.R.get()] = std::move(vertices);
                if (!out.empty())
                  tracked.push_back({ t.R, std::move(out) });
              }
              continue;
            }
            float f = next ? (float)((time - t.Times[a]) / (t.Times[a + 1] - t.Times[a])) : 0.f;
            bool blend = next && t.Counts[a + 1] == t.Counts[a];
            std::vector<float> out(t.Data.begin() + t.Starts[a], t.Data.begin() + t.Starts[a] + t.Counts[a] * floats);
            if (blend)
              for (size_t k = 0; k < out.size(); ++k)
                out[k] += (t.Data[t.Starts[a + 1] + k] - out[k]) * f;
            std::vector<BYTE> vertices(t.ClothData.begin() + t.ClothStarts[a], t.ClothData.begin() + t.ClothStarts[a] + t.ClothBytes);
            UINT vStride = t.R->Strides[t.R->ClothSlot];
            if (blend && t.ClothColumns && vStride)
            {
              // Its plain numbers blended, the rest the nearer sample's
              BYTE const* then = &t.ClothData[t.ClothStarts[a + 1]];
              UINT columns = (std::min)(vStride / 4, 32u);
              for (size_t at = 0; at + vStride <= t.ClothBytes; at += vStride)
                for (UINT c = 0; c < columns; ++c)
                {
                  size_t k = at + (size_t)c * 4;
                  if (t.ClothColumns & (1u << c))
                  {
                    float v, w;
                    memcpy(&v, &vertices[k], 4);
                    memcpy(&w, then + k, 4);
                    v += (w - v) * f;
                    memcpy(&vertices[k], &v, 4);
                  }
                  else if (f > 0.5f)
                    memcpy(&vertices[k], then + k, 4);
                }
            }
            else if (next && f > 0.5f)
              memcpy(vertices.data(), &t.ClothData[t.ClothStarts[a + 1]], t.ClothBytes);
            clothNow[t.R.get()] = std::move(vertices);
            if (!out.empty())
              tracked.push_back({ t.R, std::move(out) });
            continue;
          }
          if (time < t.Times.front())
            continue;
          size_t a = (size_t)(std::upper_bound(t.Times.begin(), t.Times.end(), time) - t.Times.begin()) - 1;
          bool next = a + 1 < t.Times.size() && t.Times[a + 1] - t.Times[a] <= TrackHoldSeconds;
          if (!next && time - t.Times[a] > TrackHoldSeconds / 2)
            continue;
          float f = next ? (float)((time - t.Times[a]) / (t.Times[a + 1] - t.Times[a])) : 0.f;
          UINT stride = t.R->Stride;
          size_t floats = (size_t)stride * 4;
          auto posed = posedAt.find(WorldKeyType((__int64)t.R->Vbs[0].Get(), t.R->StartIndex, t.R->IndexCount));
          std::vector<float> out;
          for (UINT i = 0; i < t.Counts[a]; ++i)
          {
            float const* rows = &t.Data[t.Starts[a] + i * floats];
            float anchor[3];
            AnchorOf(rows, t.Header, anchor);
            if (Distance2(anchor, g_WorldCenter) > TrackDrawMetres * TrackDrawMetres)
              continue;
            bool theirs = false;
            if (posed != posedAt.end())
              for (size_t k = 0; k + 3 <= posed->second.size() && !theirs; k += 3)
                theirs = Distance2(anchor, &posed->second[k]) <= TrackPosedMetres * TrackPosedMetres;
            if (theirs)
              continue;
            // The same one in the next sample: the nearest, within TrackSameMetres
            float const* then = nullptr;
            if (next)
            {
              float best = TrackSameMetres * TrackSameMetres;
              for (UINT j = 0; j < t.Counts[a + 1]; ++j)
              {
                float const* other = &t.Data[t.Starts[a + 1] + j * floats];
                float there[3];
                AnchorOf(other, t.Header, there);
                float d2 = Distance2(anchor, there);
                if (d2 < best)
                {
                  best = d2;
                  then = other;
                }
              }
            }
            size_t at = out.size();
            out.insert(out.end(), rows, rows + floats);
            if (then)
              for (size_t k = 0; k < floats; ++k)
                out[at + k] += (then[k] - rows[k]) * f;
          }
          if (!out.empty())
            tracked.push_back({ t.R, std::move(out) });
        }
      }
    }
    // Each recipe's instances this frame
    struct Group
    {
      std::shared_ptr<Recipe> R;
      UINT Start, Count;
    };
    std::map<Recipe*, std::vector<std::pair<int, float const*>>> wanted;   // instance, transform
    // Parts not drawn because none of their kept looks is the object's own and
    // they have more than one (their plain shape isn't drawn either); and the
    // parts' "one look" answers this frame
    std::set<long long> withheld;
    std::map<std::pair<int, long long>, bool> oneLook;
    for (auto const& d : draws)
    {
      auto it = byShape.find(d.Shape);
      if (it == byShape.end())
        continue;
      // Each part (its section, its pass's place) drawn once: with the kept
      // variation whose instance is nearest where the model was (see
      // VariantSameMetres), the first one's when none is within InstanceMatchMetres
      std::map<long long, std::pair<Recipe*, std::pair<int, float>>> chosen;
      // (A Dash's marker: within RvMatchMetres, the game draws one with its part's offset)
      float within = d.RvLayer ? RvMatchMetres : InstanceMatchMetres;
      float match2 = d.RvLayer ? RvMatchMetres * RvMatchMetres : d.Still ? StillMatchMetres * StillMatchMetres :
        InstanceMatchMetres * InstanceMatchMetres;
      for (auto const& r : it->second)
      {
        // Its own instance: the one where it was (the nearest within InstanceMatchMetres)
        std::pair<int, float> const& own = OwnInstance(*r, d.Entity, d.Transform, d.Path.get(), within);
        long long part = ((long long)r->Section << 8) | (r->Pass * MaxPassOrdinals + r->PassOrdinal);
        auto c = chosen.find(part);
        if (c == chosen.end() || own.second < c->second.second.second)
          chosen[part] = { r.get(), own };
      }
      // Whether the game drew this object while it recorded: one of its
      // shape's kept draws, in any pass, has its own instance (2026-11-01, the
      // user's gate screenshot: "it's the facility 7 texture with the foodome
      // texture together": the Release gate's two sign planes,
      // plane_1-1_01_gatesign_01 in pf_releasegateglass, are Enabled only by
      // the gate's events (their BoolEntity's DefaultValue is False), so the
      // game never drew them while it recorded; their copies were drawn with
      // the one kept look of that shared plane mesh, the Facility 07 sign of
      // another part of the city, plane_1-1_01_facility07)
      bool proven = false;
      auto every = allOfShape.find(d.Shape);
      if (every != allOfShape.end())
        for (auto const& r : every->second)
          if (OwnInstance(*r, d.Entity, d.Transform, d.Path.get(), within).second < match2)
          {
            proven = true;
            break;
          }
      for (auto const& c : chosen)
      {
        // None of the part's kept looks has an instance of this object's
        // (within InstanceMatchMetres): the first one is taken only when all
        // of them look alike (one look: the same props); else it isn't drawn
        // (2026-10-28, the user: "stop shared meshes falling back to another
        // object's look": a sign plane, plane_1-1_01, is many signs with
        // pictures of their own)
        if (c.second.second.second >= match2)
        {
          auto key = std::make_pair(d.Shape, c.first);
          // Never drawn by the game while it recorded: not drawn with any of them
          if (!proven)
          {
            withheld.insert(KeyOf(c.second.first->Shape, c.second.first->Section));
            static std::set<int> neverLogged;
            if (neverLogged.size() < 60 && neverLogged.insert(d.Shape).second)
              util::log::Write("Replay: '%s': a model of it the game never drew while it recorded (none of its mesh's kept draws, in "
                "any pass, has it within %.1f m) isn't drawn with another object's look", c.second.first->Name, sqrtf(match2));
            continue;
          }
          // One that didn't move: in each pass only with a kept draw of its own
          // (2026-11-02, the user: "the food-dome texture is still there in the
          // replay when it shouldn't": the Release gate's sign planes had their
          // own draw kept in one pass, so the one look kept of the shared plane
          // mesh in another pass, another sign's, was taken for "one look" and
          // drawn on both panes). A part the game drew it in has its instance
          // in a kept draw of that pass; one it didn't, isn't drawn.
          if (d.Still)
          {
            withheld.insert(KeyOf(c.second.first->Shape, c.second.first->Section));
            static std::set<std::pair<int, long long>> stillLogged;
            if (stillLogged.size() < 60 && stillLogged.insert(key).second)
              util::log::Write("Replay: '%s' section %d (pass %d, its %d): not drawn for a model that didn't move at %.1f %.1f %.1f "
                "(none of the part's kept draws has its own instance within %.1f m: another object's look isn't used)",
                c.second.first->Name, c.second.first->Section, c.second.first->Pass, c.second.first->PassOrdinal + 1, d.Transform[12],
                d.Transform[13], d.Transform[14], sqrtf(match2));
            continue;
          }
          auto known = oneLook.find(key);
          if (known == oneLook.end())
          {
            bool one = true;
            Recipe const* first = nullptr;
            for (auto const& r : it->second)
            {
              long long part = ((long long)r->Section << 8) | (r->Pass * MaxPassOrdinals + r->PassOrdinal);
              if (part != c.first)
                continue;
              if (!first)
                first = r.get();
              else if (!LooksAlike(*first, *r))
              {
                one = false;
                break;
              }
            }
            known = oneLook.emplace(key, one).first;
          }
          if (!known->second)
          {
            withheld.insert(KeyOf(c.second.first->Shape, c.second.first->Section));
            // In the log once a part (2026-11-01: which of the Release gate's
            // parts the replay left out wasn't known)
            static std::set<std::pair<int, long long>> logged;
            if (logged.size() < 60 && logged.insert(key).second)
              util::log::Write("Replay: '%s' section %d (pass %d, its %d): not drawn for one of its models (none of its kept looks "
                "has an instance within %.0f m of where the model was, and they don't look alike: another object's picture isn't "
                "used)", c.second.first->Name, c.second.first->Section, c.second.first->Pass, c.second.first->PassOrdinal + 1,
                InstanceMatchMetres);
            continue;
          }
        }
        // Which kept draw a model that didn't move is drawn with, within 30 m
        // of the replayed Faith (once a model's part; 2026-11-02: which look
        // the gate's panes had wasn't in the log)
        if (d.Still && Distance2(d.Transform + 12, center) < 30.f * 30.f)
        {
          static std::set<std::pair<__int64, long long>> drawnLogged;
          if (drawnLogged.size() < 120 && drawnLogged.insert(std::make_pair(d.Entity, c.first)).second)
            util::log::Write("Replay: '%s' section %d (pass %d, its %d): a model that didn't move at %.1f %.1f %.1f drawn with the "
              "game's draw kept at %.1f s%s, its own instance %.2f m from it", c.second.first->Name, c.second.first->Section,
              c.second.first->Pass, c.second.first->PassOrdinal + 1, d.Transform[12], d.Transform[13], d.Transform[14],
              c.second.first->Clock, c.second.first->FromEnv ? " (from a reflection)" : "", sqrtf(c.second.second.second));
        }
        // (A Dash's marker as the game drew it: its kept instance as it was,
        // with its part's offset and its prefab's scale; it never moves)
        wanted[c.second.first].push_back({ c.second.second.first, d.RvLayer ? nullptr : d.Transform });
      }
    }
    std::map<Recipe*, std::vector<std::vector<float> const*>> posed;   // people's, by their recipe
    std::map<Recipe*, std::shared_ptr<Recipe>> posedRecipes;
    for (auto const& p : people)
    {
      posed[p.first.get()].push_back(&p.second);
      posedRecipes[p.first.get()] = p.first;
    }
    UINT total = 0;
    for (auto const& w : wanted)
      total += (UINT)w.second.size() * w.first->Stride;
    for (auto const& p : posed)
      total += (UINT)p.second.size() * p.first->Stride;
    std::sort(world.begin(), world.end(), [](WorldDraw const& a, WorldDraw const& b) { return a.Away < b.Away; });
    UINT worldTotal = 0;
    size_t worldCount = 0;
    for (; worldCount < world.size(); ++worldCount)
    {
      UINT size = (UINT)(world[worldCount].Instances.size() / 4);
      if (worldTotal + size > MaxWorldInstancesDrawn)
        break;
      worldTotal += size;
    }
    world.resize(worldCount);
    total += worldTotal;
    UINT trackTotal = 0;
    for (size_t i = 0; i < tracked.size(); ++i)
    {
      UINT size = (UINT)(tracked[i].second.size() / 4);
      if (trackTotal + size > MaxWorldInstancesDrawn / 2)
      {
        tracked.resize(i);
        break;
      }
      trackTotal += size;
    }
    total += trackTotal;
    if (!total || total > MaxInstancesDrawn * 16)
      return;
    // Still in the G-buffer (it's left bound when the game changes it: checked)
    {
      ID3D11RenderTargetView* targets[8] = {};
      ID3D11DepthStencilView* depth = nullptr;
      g_Immediate->OMGetRenderTargets(8, targets, &depth);
      bool main = PassOf(8, targets, depth) == pass;
      for (auto t : targets)
        if (t)
          t->Release();
      if (depth)
        depth->Release();
      if (!main)
      {
        if (!g_LoggedNoTargets)
          util::log::Write("Replay: the game's G-buffer wasn't bound at the end of its pass: the recorded models are drawn as shapes "
            "and its people aren't drawn");
        g_LoggedNoTargets = true;
        return;
      }
    }
    ComPtr<ID3D11Device> device;
    g_Immediate->GetDevice(device.GetAddressOf());
    if (!device || !g_View || !EnsureInstances(device.Get(), total))
      return;
    // The instances: a model's as recorded, with its transform now; a person's as posed
    std::vector<Group> groups;
    std::vector<float> feedPlaces[Feeds];   // where the video screens are drawn (see NoteFeedTextures)
    int modelGroups = 0, peopleEnd = 0, worldEnd = 0;
    {
      D3D11_MAPPED_SUBRESOURCE m{};
      if (FAILED(g_Immediate->Map(g_Instances.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &m)) || !m.pData)
        return;
      float* out = (float*)m.pData;
      UINT at = 0;
      for (auto const& entry : byShape)
        for (auto const& r : entry.second)
        {
          auto w = wanted.find(r.get());
          if (w == wanted.end())
            continue;
          Group g{ r, at, 0 };
          for (auto const& one : w->second)
          {
            float* rows = out + (size_t)at * 4;
            memcpy(rows, &r->InstanceData[(size_t)one.first * r->Stride * 4], (size_t)r->Stride * 16);
            float const* m4 = one.second;
            if (r->HasFeed && m4)
              NoteFeedPlace(*r, m4 + 12, feedPlaces);
            for (int k : r->WorldRows)
            {
              if (!m4)
                break;
              float* row = rows + k * 4;
              row[0] = m4[0]; row[1] = m4[4]; row[2] = m4[8]; row[3] = m4[12];
              row[4] = m4[1]; row[5] = m4[5]; row[6] = m4[9]; row[7] = m4[13];
              row[8] = m4[2]; row[9] = m4[6]; row[10] = m4[10]; row[11] = m4[14];
            }
            at += r->Stride;
            ++g.Count;
          }
          groups.push_back(g);
        }
      modelGroups = (int)groups.size();
      for (auto const& p : posed)
      {
        Group g{ posedRecipes[p.first], at, 0 };
        for (auto const* instance : p.second)
        {
          memcpy(out + (size_t)at * 4, instance->data(), instance->size() * sizeof(float));
          at += p.first->Stride;
          ++g.Count;
        }
        groups.push_back(g);
      }
      peopleEnd = (int)groups.size();
      // The world's: their instances as kept (not where the game draws one)
      for (auto const& w : world)
      {
        UINT count = (UINT)(w.Instances.size() / ((size_t)w.R->Stride * 4));
        if (w.R->HasFeed && !w.R->WorldRows.empty())
          for (UINT i = 0; i < count && i < 16; ++i)
          {
            float const* rows = w.Instances.data() + (size_t)i * w.R->Stride * 4;
            int k = w.R->WorldRows[0];
            float place[3] = { rows[k * 4 + 3], rows[(k + 1) * 4 + 3], rows[(k + 2) * 4 + 3] };
            NoteFeedPlace(*w.R, place, feedPlaces);
          }
        memcpy(out + (size_t)at * 4, w.Instances.data(), w.Instances.size() * sizeof(float));
        groups.push_back(Group{ w.R, at, count });
        at += count * w.R->Stride;
      }
      worldEnd = (int)groups.size();
      // The tracks': their instances now
      for (auto const& t : tracked)
      {
        UINT count = (UINT)(t.second.size() / ((size_t)t.first->Stride * 4));
        memcpy(out + (size_t)at * 4, t.second.data(), t.second.size() * sizeof(float));
        groups.push_back(Group{ t.first, at, count });
        at += count * t.first->Stride;
      }
      g_Immediate->Unmap(g_Instances.Get(), 0);
    }
    {
      ULONGLONG now = GetTickCount64();
      AcquireSRWLockExclusive(&g_FeedLock);
      for (int ch = 0; ch < Feeds; ++ch)
        if (!feedPlaces[ch].empty())
        {
          g_FeedPlaces[ch].swap(feedPlaces[ch]);
          g_FeedPlacedAt[ch] = now;
        }
      ReleaseSRWLockExclusive(&g_FeedLock);
    }
    for (Group const& g : groups)
    {
      Recipe& r = *g.R;
      // A loaded one's own constant buffers, made at its first draw (see Load)
      if (!r.VsB0Own)
      {
        r.VsB0Own = ConstantsWith(device.Get(), r.VsB0Bytes, true);
        r.PsB0Own = ConstantsWith(device.Get(), r.PsB0Bytes, true);
        r.VsB1Copy = ConstantsWith(device.Get(), r.VsB1Bytes, false);
        r.PsB1Copy = ConstantsWith(device.Get(), r.PsB1Bytes, false);
      }
      if (!r.VsB0Own || !FillConstants(r.VsB0Own.Get(), r.VsB0Bytes, r.VsOffsetsAt, g.Start, r.Stride) ||
          (r.PsB0Own && !FillConstants(r.PsB0Own.Get(), r.PsB0Bytes, r.PsOffsetsAt, g.Start, r.Stride)))
        return;
      // A model's material values of the time played (see NoteMaterial), in the tools' own b1s
      if (r.B1Samples.size() > 1 && playTime >= 0.0)
      {
        std::vector<BYTE> b1;
        MaterialAt(r, playTime, b1);
        size_t vsBytes = r.VsB1Bytes.size();
        if (b1.size() == vsBytes + r.PsB1Bytes.size())
        {
          std::vector<BYTE> vs(b1.begin(), b1.begin() + vsBytes), ps(b1.begin() + vsBytes, b1.end());
          if (!vs.empty() && !r.VsB1Live)
            r.VsB1Live = ConstantsWith(device.Get(), vs, true);
          if (!ps.empty() && !r.PsB1Live)
            r.PsB1Live = ConstantsWith(device.Get(), ps, true);
          if (r.VsB1Live)
            FillConstants(r.VsB1Live.Get(), vs, -1, 0, 0);
          if (r.PsB1Live)
            FillConstants(r.PsB1Live.Get(), ps, -1, 0, 0);
          // At most 2 a mesh (2026-11-03: the first 40 went to air conditioners and billboards)
          static std::set<Recipe const*> logged;
          static std::map<std::string, int> loggedNames;
          if (logged.size() < 150 && !logged.count(&r) && loggedNames[r.Name] < 2 && logged.insert(&r).second &&
              ++loggedNames[r.Name])
            util::log::Write("Replay: '%s' section %d (pass %d, its %d): drawn with its material values as the game had them at each "
              "time (%d samples while it recorded, %.1f - %.1f s)", r.Name, r.Section, r.Pass, r.PassOrdinal + 1, (int)r.B1Times.size(),
              r.B1Times.front(), r.B1Times.back());
        }
      }
      // A cloth's vertices now, in the tools' own buffer (see MaxClothBytes)
      auto cloth = r.Cloth ? clothNow.find(&r) : clothNow.end();
      if (cloth != clothNow.end())
      {
        UINT bytes = (UINT)cloth->second.size();
        if (!r.ClothVb || r.ClothVbBytes != bytes)
        {
          D3D11_BUFFER_DESC d{};
          d.ByteWidth = bytes;
          d.Usage = D3D11_USAGE_DYNAMIC;
          d.BindFlags = D3D11_BIND_VERTEX_BUFFER;
          d.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
          r.ClothVb.Reset();
          r.ClothVbBytes = 0;
          if (SUCCEEDED(device->CreateBuffer(&d, nullptr, r.ClothVb.GetAddressOf())))
            r.ClothVbBytes = bytes;
        }
        D3D11_MAPPED_SUBRESOURCE m{};
        if (r.ClothVb && SUCCEEDED(g_Immediate->Map(r.ClothVb.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &m)) && m.pData)
        {
          memcpy(m.pData, cloth->second.data(), bytes);
          g_Immediate->Unmap(r.ClothVb.Get(), 0);
        }
      }
    }
    // The video channels' frames now (held while they're drawn; see NoteFeedTextures)
    ComPtr<ID3D11ShaderResourceView> feedNow[Feeds][2];
    AcquireSRWLockShared(&g_FeedLock);
    for (int ch = 0; ch < Feeds; ++ch)
      for (int k = 0; k < 2; ++k)
        feedNow[ch][k] = g_FeedNow[ch][k];
    ReleaseSRWLockShared(&g_FeedLock);
    // Drawn, the game's state saved and put back around them
    GameState saved;
    saved.Save(g_Immediate1.Get());
    t_Drawing = true;
    ID3D11DeviceContext1* c = g_Immediate1.Get();
    std::set<long long> drawn;
    // A part withheld above: its plain shape not drawn either (its picture is
    // some object's of the same mesh)
    for (long long k : withheld)
      if ((k & 0xFFFF) < 0x4000)
        drawn.insert(k);
    int parts = 0, personParts = 0, worldParts = 0, trackParts = 0;
    for (size_t gi = 0; gi < groups.size(); ++gi)
    {
      Group const& g = groups[gi];
      Recipe& r = *g.R;
      c->VSSetShader(r.Vs.Get(), nullptr, 0);
      c->PSSetShader(r.Ps.Get(), nullptr, 0);
      c->GSSetShader(r.Gs.Get(), nullptr, 0);
      c->HSSetShader(r.Hs.Get(), nullptr, 0);
      c->DSSetShader(r.Ds.Get(), nullptr, 0);
      bool live = r.B1Samples.size() > 1 && playTime >= 0.0;
      ID3D11Buffer* vsCb[3] = { r.VsB0Own.Get(), live && r.VsB1Live ? r.VsB1Live.Get() : r.VsB1Copy.Get(), g_View.Get() };
      ID3D11Buffer* psCb[3] = { r.PsB0Own.Get(), live && r.PsB1Live ? r.PsB1Live.Get() : r.PsB1Copy.Get(), g_View.Get() };
      c->VSSetConstantBuffers(0, 3, vsCb);
      c->PSSetConstantBuffers(0, 3, psCb);
      ID3D11ShaderResourceView* vsViews[VsViewSlots];
      for (int i = 0; i < VsViewSlots; ++i)
        vsViews[i] = r.VsInstanceSlot[i] ? g_InstancesView.Get() : r.VsViews[i].Get();
      ID3D11ShaderResourceView* psViews[PsViewSlots];
      for (int i = 0; i < PsViewSlots; ++i)
      {
        psViews[i] = r.PsInstanceSlot[i] ? g_InstancesView.Get() : r.PsViews[i].Get();
        // A video screen's: its channel's frame now
        if (r.Feed[i] && feedNow[(r.Feed[i] - 1) / 2][(r.Feed[i] - 1) % 2])
          psViews[i] = feedNow[(r.Feed[i] - 1) / 2][(r.Feed[i] - 1) % 2].Get();
      }
      c->VSSetShaderResources(0, VsViewSlots, vsViews);
      c->PSSetShaderResources(0, PsViewSlots, psViews);
      ID3D11SamplerState* vsSamplers[VsSamplerSlots];
      for (int i = 0; i < VsSamplerSlots; ++i)
        vsSamplers[i] = r.VsSamplers[i].Get();
      ID3D11SamplerState* psSamplers[PsSamplerSlots];
      for (int i = 0; i < PsSamplerSlots; ++i)
        psSamplers[i] = r.PsSamplers[i].Get();
      c->VSSetSamplers(0, VsSamplerSlots, vsSamplers);
      c->PSSetSamplers(0, PsSamplerSlots, psSamplers);
      c->IASetInputLayout(r.Layout.Get());
      c->IASetPrimitiveTopology(r.Topology);
      ID3D11Buffer* vbs[VbSlots];
      for (int i = 0; i < VbSlots; ++i)
        vbs[i] = r.Vbs[i].Get();
      // A cloth's: its vertices as recorded (from the draw's own offset, see
      // ClothOffset), not the game's buffer as it is now
      UINT offsets[VbSlots];
      memcpy(offsets, r.Offsets, sizeof(offsets));
      if (r.Cloth)
      {
        if (r.ClothSlot < 0 || r.ClothSlot >= VbSlots || !r.ClothVb || !clothNow.count(&r))
          continue;
        vbs[r.ClothSlot] = r.ClothVb.Get();
        offsets[r.ClothSlot] = 0;
      }
      c->IASetVertexBuffers(0, VbSlots, vbs, r.Strides, offsets);
      c->IASetIndexBuffer(r.Ib.Get(), r.IbFormat, r.IbOffset);
      c->OMSetDepthStencilState(r.Depth.Get(), r.StencilRef);
      c->OMSetBlendState(r.Blend.Get(), r.BlendFactor, r.SampleMask);
      c->RSSetState(r.Raster.Get());
      c->DrawIndexedInstanced(r.IndexCount, g.Count, r.StartIndex, r.BaseVertex, 0);
      // (The G-buffer pass's only: the asset log compares the game's there.
      // 2026-10-28: the other passes' were counted too, a sign plane's slot 2
      // "played with" a 3840 x 2054 target of the lit picture's pass)
      if (mainPass)
        replay_assetlog::OnReplayDraw(vbs[0], r.StartIndex, r.IndexCount, psViews, PsViewSlots);
      if ((int)gi < modelGroups)
      {
        // A shape's part drawn (the G-buffer pass's, or another pass's of one of its own sections)
        if (mainPass || r.Section < 0x4000)
          drawn.insert(KeyOf(r.Shape, r.Section));
        if (mainPass && wholeLevel.count(r.Shape))
          drawn.insert(KeyOf(r.Shape, 0xFFFF));
        parts += (int)g.Count;
      }
      else if ((int)gi < peopleEnd)
        personParts += (int)g.Count;
      else if ((int)gi < worldEnd)
        worldParts += (int)g.Count;
      else
        trackParts += (int)g.Count;
    }
    t_Drawing = false;
    saved.Restore(g_Immediate1.Get());
    if (mainPass)
      g_Injected = true;
    else if (!g_LoggedPass[pass][ordinal] && !groups.empty())
    {
      util::log::Write("Replay: %d kept draws drawn in the game's %s (its %d%s after the G-buffer pass)", (int)groups.size(),
        pass == Pass_Decal ? "decal pass" : pass == Pass_Lit ? "lit picture's pass" : pass == Pass_Rv ?
        "runner's vision layer, into the replay's own mask" : "half-size transparent pass", ordinal + 1, ordinal == 0 ? "st" : ordinal == 1 ? "nd" :
        ordinal == 2 ? "rd" : "th");
      g_LoggedPass[pass][ordinal] = true;
    }
    if (mainPass && modelGroups && !g_LoggedPlaying)
    {
      util::log::Write("Replay: %d parts of the recorded models the game doesn't have drawn with the game's own draws (%d recipes) at "
        "the end of its G-buffer pass", parts, modelGroups);
      g_LoggedPlaying = true;
    }
    if (personParts && !g_LoggedPeople)
    {
      util::log::Write("Replay: %d parts of the recorded people the game doesn't have drawn with the game's own draws of them (%d "
        "kept draws), posed as recorded", personParts, peopleEnd - modelGroups);
      g_LoggedPeople = true;
    }
    g_DrawCalls += groups.size();
    if (trackParts && !g_LoggedTracks)
    {
      util::log::Write("Replay: %d instances of other skinned draws (drones, crowds: not recorded people) drawn from their tracks "
        "(%d tracks)", trackParts, (int)groups.size() - worldEnd);
      g_LoggedTracks = true;
    }
    if (worldParts && !g_LoggedWorld)
    {
      util::log::Write("Replay: %d parts of the recorded world whose meshes the game isn't drawing drawn with its own draws of them "
        "(%d kept draws, near the replayed Faith)", worldParts, (int)groups.size() - peopleEnd);
      g_LoggedWorld = true;
    }
    std::lock_guard<std::mutex> lock(g_Mutex);
    g_Drawn.insert(drawn.begin(), drawn.end());
  }

  // A buffer view's element size for the crowd buffers' formats (0: not one)
  UINT CrowdElementBytes(DXGI_FORMAT f)
  {
    switch (f)
    {
    case DXGI_FORMAT_R16_UINT:
    case DXGI_FORMAT_R16_SINT:
      return 2;
    case DXGI_FORMAT_R32_UINT:
    case DXGI_FORMAT_R32_SINT:
    case DXGI_FORMAT_R32_FLOAT:
      return 4;
    case DXGI_FORMAT_R32G32B32A32_FLOAT:
    case DXGI_FORMAT_R32G32B32A32_UINT:
      return 16;
    default:
      return 0;
    }
  }

  // A vertex shader's crowd buffers by the names in it (replay_drawlog::ShaderNames:
  // "t24 indirectionBuffer, t25 transformBuffer, ..."); found: a slot each
  CrowdShader ParseCrowdShader(char const* names)
  {
    CrowdShader s;
    static char const* const wanted[CrowdBuffers] = { "indirectionBuffer", "transformBuffer", "animationBuffer" };
    for (int b = 0; b < CrowdBuffers; ++b)
    {
      size_t length = strlen(wanted[b]);
      for (char const* at = strstr(names, wanted[b]); at; at = strstr(at + 1, wanted[b]))
      {
        char after = at[length];
        if (at - names < 3 || at[-1] != ' ' || (after != ',' && after != ' ' && after != 0))
          continue;
        char const* p = at - 2;
        while (p > names && *p >= '0' && *p <= '9')
          --p;
        if (*p == 't' && (p == names || p[-1] == ' '))
        {
          s.Slot[b] = atoi(p + 1);
          break;
        }
      }
    }
    s.Crowd = s.Slot[Crowd_Indirection] >= 0 && s.Slot[Crowd_Indirection] < CrowdVsViewSlots && s.Slot[Crowd_Transform] >= 0 &&
      s.Slot[Crowd_Transform] < CrowdVsViewSlots && s.Slot[Crowd_Animation] < CrowdVsViewSlots;
    return s;
  }

  // Render thread: a vertex shader's crowd buffers (not kept while its names
  // aren't noted yet)
  CrowdShader CrowdShaderOf(ID3D11VertexShader* vs)
  {
    auto it = g_CrowdShaders.find((__int64)vs);
    if (it != g_CrowdShaders.end())
      return it->second;
    char names[600] = "";
    if (!vs || !replay_drawlog::ShaderNames((__int64)vs, names, sizeof(names)))
      return CrowdShader{};
    CrowdShader s = ParseCrowdShader(names);
    if (g_CrowdShaders.size() < 65536)
      g_CrowdShaders[(__int64)vs] = s;
    return s;
  }

  // A crowd draw's state (render thread, during the draw), as TakeState with
  // no instance buffer: its vertex shader's views 0 - 7 (t1: the swarm's
  // animTexture), its pixel shader's (a crowd buffer's slot there marked as an
  // instance slot: the tools' own then), its constant buffers' windows
  char const* TakeCrowdState(Recipe& r, CrowdKind& k, CrowdShader const& shader, ID3D11ShaderResourceView* const* vsViews,
    UINT indexCount, UINT instances, UINT startIndex, INT baseVertex)
  {
    ID3D11DeviceContext1* c = g_Immediate1.Get();
    c->VSGetShader(r.Vs.GetAddressOf(), nullptr, nullptr);
    c->PSGetShader(r.Ps.GetAddressOf(), nullptr, nullptr);
    if (!r.Vs || !r.Ps)
      return "no vertex or pixel shader";
    c->GSGetShader(r.Gs.GetAddressOf(), nullptr, nullptr);
    c->HSGetShader(r.Hs.GetAddressOf(), nullptr, nullptr);
    c->DSGetShader(r.Ds.GetAddressOf(), nullptr, nullptr);
    c->IAGetInputLayout(r.Layout.GetAddressOf());
    c->IAGetPrimitiveTopology(&r.Topology);
    {
      ID3D11Buffer* vbs[VbSlots] = {};
      c->IAGetVertexBuffers(0, VbSlots, vbs, r.Strides, r.Offsets);
      for (int i = 0; i < VbSlots; ++i)
        r.Vbs[i].Attach(vbs[i]);
    }
    c->IAGetIndexBuffer(r.Ib.GetAddressOf(), &r.IbFormat, &r.IbOffset);
    r.IndexCount = indexCount;
    r.StartIndex = startIndex;
    r.BaseVertex = baseVertex;
    r.Instances = instances;
    ID3D11Buffer* vsCb[2] = {}, * psCb[2] = {};
    UINT vsFirst[2] = {}, vsNum[2] = {}, psFirst[2] = {}, psNum[2] = {};
    c->VSGetConstantBuffers1(0, 2, vsCb, vsFirst, vsNum);
    c->PSGetConstantBuffers1(0, 2, psCb, psFirst, psNum);
    r.VsB0 = Window{ (__int64)vsCb[0], vsFirst[0], vsNum[0] };
    r.VsB1 = Window{ (__int64)vsCb[1], vsFirst[1], vsNum[1] };
    r.PsB0 = Window{ (__int64)psCb[0], psFirst[0], psNum[0] };
    r.PsB1 = Window{ (__int64)psCb[1], psFirst[1], psNum[1] };
    k.PsB1IsVsB1 = psCb[1] && psCb[1] == vsCb[1] && psFirst[1] == vsFirst[1];
    for (int i = 0; i < 2; ++i)
    {
      if (vsCb[i])
        vsCb[i]->Release();
      if (psCb[i])
        psCb[i]->Release();
    }
    for (int i = 0; i < VsViewSlots; ++i)
      r.VsViews[i] = vsViews[i];
    __int64 crowdBuffers[CrowdBuffers] = {};
    for (int b = 0; b < CrowdBuffers; ++b)
    {
      int slot = shader.Slot[b];
      if (slot < 0 || !vsViews[slot])
        continue;
      ComPtr<ID3D11Resource> res;
      vsViews[slot]->GetResource(res.GetAddressOf());
      crowdBuffers[b] = (__int64)res.Get();
      if (slot < VsViewSlots)
        r.VsInstanceSlot[slot] = true;
    }
    {
      ID3D11ShaderResourceView* views[PsViewSlots] = {};
      c->PSGetShaderResources(0, PsViewSlots, views);
      for (int i = 0; i < PsViewSlots; ++i)
      {
        r.PsViews[i].Attach(views[i]);
        if (!views[i])
          continue;
        ComPtr<ID3D11Resource> res;
        views[i]->GetResource(res.GetAddressOf());
        for (int b = 0; b < CrowdBuffers; ++b)
          if (crowdBuffers[b] && (__int64)res.Get() == crowdBuffers[b])
          {
            k.PsSlot[b] = i;
            r.PsInstanceSlot[i] = true;
          }
      }
    }
    {
      ID3D11SamplerState* samplers[PsSamplerSlots] = {};
      c->VSGetSamplers(0, VsSamplerSlots, samplers);
      for (int i = 0; i < VsSamplerSlots; ++i)
        r.VsSamplers[i].Attach(samplers[i]);
      memset(samplers, 0, sizeof(samplers));
      c->PSGetSamplers(0, PsSamplerSlots, samplers);
      for (int i = 0; i < PsSamplerSlots; ++i)
        r.PsSamplers[i].Attach(samplers[i]);
    }
    c->OMGetDepthStencilState(r.Depth.GetAddressOf(), &r.StencilRef);
    c->OMGetBlendState(r.Blend.GetAddressOf(), r.BlendFactor, &r.SampleMask);
    c->RSGetState(r.Raster.GetAddressOf());
    return nullptr;
  }

  // A window of a constant buffer copied for reading back (null when it's past the buffer)
  ComPtr<ID3D11Buffer> WindowCopy(ID3D11Buffer* cb, UINT first, UINT num, UINT& bytes)
  {
    bytes = 0;
    if (!cb)
      return nullptr;
    D3D11_BUFFER_DESC d{};
    cb->GetDesc(&d);
    UINT size = num ? num * 16 : d.ByteWidth;
    if (size > MaxWindowBytes)
      size = MaxWindowBytes;
    if (!size || (size_t)first * 16 + size > d.ByteWidth)
      return nullptr;
    bytes = size;
    return StagingCopy(cb, first * 16, size);
  }

  // Render thread, at Present: the crowd draws' copies read back, their members kept
  void ReadCrowds()
  {
    if (g_CrowdCopies.empty())
      return;
    std::map<ID3D11Buffer*, std::vector<BYTE>> read;   // mapped this time, by staging buffer
    // 1: read, 0: not copied yet, -1: can't be
    auto bytesOf = [&](ComPtr<ID3D11Buffer> const& b, UINT size, std::vector<BYTE> const** out) -> int {
      *out = nullptr;
      if (!b)
        return -1;
      auto it = read.find(b.Get());
      if (it == read.end())
      {
        D3D11_MAPPED_SUBRESOURCE m{};
        HRESULT hr = g_Immediate->Map(b.Get(), 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &m);
        if (hr == DXGI_ERROR_WAS_STILL_DRAWING)
          return 0;
        if (FAILED(hr) || !m.pData)
          return -1;
        it = read.emplace(b.Get(), std::vector<BYTE>((BYTE const*)m.pData, (BYTE const*)m.pData + size)).first;
        g_Immediate->Unmap(b.Get(), 0);
      }
      *out = &it->second;
      return 1;
    };
    std::vector<CrowdSample> samples;
    struct StateBytes { int Kind; std::vector<BYTE> VsB0, VsB1, PsB0, PsB1; };
    std::vector<StateBytes> states;
    int bad = 0;
    for (size_t i = 0; i < g_CrowdCopies.size();)
    {
      CrowdCopy& cc = g_CrowdCopies[i];
      if (g_Frame - cc.Frame < ReadbackAfterFrames)
      {
        ++i;
        continue;
      }
      std::vector<BYTE> const* b1Page = nullptr;
      std::vector<BYTE> const* buffer[CrowdBuffers] = {};
      int got = bytesOf(cc.B1, cc.B1Page, &b1Page);
      std::vector<BYTE> b1Window;
      if (got > 0 && b1Page && (size_t)cc.B1Offset + cc.B1Bytes <= b1Page->size())
        b1Window.assign(b1Page->begin() + cc.B1Offset, b1Page->begin() + cc.B1Offset + cc.B1Bytes);
      std::vector<BYTE> const* b1 = b1Window.empty() ? nullptr : &b1Window;
      for (int b = 0; b < CrowdBuffers; ++b)
        if (cc.Buffer[b])
          got = (std::min)(got, bytesOf(cc.Buffer[b], cc.Bytes[b], &buffer[b]));
      std::vector<BYTE> const* vsB0 = nullptr, * psB0 = nullptr, * psB1 = nullptr;
      if (cc.State)
      {
        if (cc.VsB0)
          got = (std::min)(got, bytesOf(cc.VsB0, cc.VsB0Bytes, &vsB0));
        if (cc.PsB0)
          got = (std::min)(got, bytesOf(cc.PsB0, cc.PsB0Bytes, &psB0));
        if (cc.PsB1)
          got = (std::min)(got, bytesOf(cc.PsB1, cc.PsB1Bytes, &psB1));
      }
      if (got == 0 && g_Frame - cc.Frame < ReadbackGiveUpFrames)
      {
        ++i;
        continue;
      }
      if (got > 0 && b1 && b1->size() >= 16 && buffer[Crowd_Indirection] && buffer[Crowd_Transform])
      {
        if (cc.State)
          states.push_back({ cc.Kind, vsB0 ? *vsB0 : std::vector<BYTE>(), *b1, psB0 ? *psB0 : std::vector<BYTE>(),
            psB1 ? *psB1 : std::vector<BYTE>() });
        // Its members: the draw's first entry (b1's first 4 bytes) on in the
        // indirection buffer's view, each a member's number into the others'
        UINT first = *(UINT const*)b1->data();
        CrowdSample s;
        s.Time = cc.Time;
        s.Kind = cc.Kind;
        s.B1 = *b1;
        bool ok = (size_t)first + cc.Instances <= cc.Count[Crowd_Indirection];
        UINT elementBytes = 0;
        {
          std::lock_guard<std::mutex> lock(g_Mutex);
          if (cc.Kind >= 0 && cc.Kind < (int)g_CrowdKinds.size())
            elementBytes = CrowdElementBytes(g_CrowdKinds[cc.Kind]->Format[Crowd_Indirection]);
        }
        ok = ok && (elementBytes == 2 || elementBytes == 4);
        for (UINT k = 0; ok && k < cc.Instances; ++k)
        {
          size_t at = ((size_t)cc.First[Crowd_Indirection] + first + k) * elementBytes;
          if (at + elementBytes > buffer[Crowd_Indirection]->size())
          {
            ok = false;
            break;
          }
          UINT member = elementBytes == 2 ? *(unsigned short const*)(buffer[Crowd_Indirection]->data() + at) :
            *(UINT const*)(buffer[Crowd_Indirection]->data() + at);
          size_t t = ((size_t)cc.First[Crowd_Transform] + member) * 16;
          if (member >= cc.Count[Crowd_Transform] || t + 16 > buffer[Crowd_Transform]->size())
            continue;   // a number past the members (left out)
          float const* f = (float const*)(buffer[Crowd_Transform]->data() + t);
          if (!std::isfinite(f[0]) || !std::isfinite(f[1]) || !std::isfinite(f[2]))
            continue;
          s.Members.push_back(member);
          s.Transforms.insert(s.Transforms.end(), f, f + 4);
          if (buffer[Crowd_Animation])
          {
            size_t a = ((size_t)cc.First[Crowd_Animation] + member) * 4;
            s.Animations.push_back(a + 4 <= buffer[Crowd_Animation]->size() ? *(UINT const*)(buffer[Crowd_Animation]->data() + a) : 0);
          }
        }
        if (ok && !s.Members.empty())
          samples.push_back(std::move(s));
        else if (!ok)
          ++bad;
      }
      g_CrowdCopies.erase(g_CrowdCopies.begin() + i);
    }
    if (samples.empty() && states.empty() && !bad)
      return;
    std::vector<std::pair<int, CrowdSample const*>> logged;
    std::lock_guard<std::mutex> lock(g_Mutex);
    g_CrowdBadDraws += bad;
    for (StateBytes& st : states)
    {
      if (st.Kind < 0 || st.Kind >= (int)g_CrowdKinds.size())
        continue;
      CrowdKind& k = *g_CrowdKinds[st.Kind];
      k.R->VsB0Bytes.swap(st.VsB0);
      k.R->VsB1Bytes.swap(st.VsB1);
      k.R->PsB0Bytes.swap(st.PsB0);
      k.R->PsB1Bytes.swap(st.PsB1);
      k.StateRead = true;
      k.R->Ready = true;
    }
    for (CrowdSample& s : samples)
    {
      if (s.Kind < 0 || s.Kind >= (int)g_CrowdKinds.size())
        continue;
      size_t bytes = s.B1.size() + s.Members.size() * 4 + s.Transforms.size() * 4 + s.Animations.size() * 4;
      if (g_CrowdBytes + bytes > MaxCrowdBytes)
      {
        if (!g_CrowdsFullLogged)
          util::log::Write("Replay: the crowds' samples reached %u MB: no more kept in this recording", (unsigned)(MaxCrowdBytes >> 20));
        g_CrowdsFullLogged = true;
        break;
      }
      // The first few, in the log: what's in b1 and the members (their
      // layout isn't known from the game's code yet)
      if (g_CrowdSampleLogs < 6)
      {
        ++g_CrowdSampleLogs;
        CrowdKind const& k = *g_CrowdKinds[s.Kind];
        UINT const* u = (UINT const*)s.B1.data();
        size_t words = s.B1.size() / 4;
        char text[900];
        int at = sprintf_s(text, "Replay (diagnostic): crowd sample of '%s' at %.2f s: %zu members; b1", k.Mesh.c_str(), s.Time,
          s.Members.size());
        for (size_t w = 0; w < words && w < 24 && at > 0 && at < (int)sizeof(text) - 32; ++w)
          at += sprintf_s(text + at, sizeof(text) - at, " %08X", u[w]);
        util::log::Write("%s", text);
        for (size_t m = 0; m < s.Members.size() && m < 3; ++m)
        {
          float const* f = &s.Transforms[m * 4];
          UINT w;
          memcpy(&w, &f[3], 4);
          if (s.Animations.empty())
            util::log::Write("Replay (diagnostic):   member %u: %.3f %.3f %.3f, w %g (0x%08X)", s.Members[m], f[0], f[1], f[2], f[3], w);
          else
            util::log::Write("Replay (diagnostic):   member %u: %.3f %.3f %.3f, w %g (0x%08X), animation 0x%08X (%u)", s.Members[m], f[0],
              f[1], f[2], f[3], w, s.Animations[m], s.Animations[m]);
        }
      }
      g_CrowdBytes += bytes;
      g_CrowdKinds[s.Kind]->Samples.push_back(g_CrowdSamples.size());
      g_CrowdSamples.push_back(std::move(s));
    }
  }

  // Render thread, at the end of the G-buffer pass while it plays: each kind's
  // members at the time played, drawn with the tools' buffers (see the crowds above)
  void DrawCrowds()
  {
    if (g_CrowdInjected || !g_View || !g_Immediate1)
      return;
    g_CrowdInjected = true;
    struct Work
    {
      std::shared_ptr<CrowdKind> K;
      std::vector<BYTE> B1;
      std::vector<float> Transforms;
      std::vector<UINT> Animations;
      UINT Count = 0;
    };
    std::vector<Work> work;
    {
      std::lock_guard<std::mutex> lock(g_Mutex);
      double time = g_TrackTime;
      if (time < 0.0 || g_CrowdSamples.empty())
        return;
      for (auto const& kp : g_CrowdKinds)
      {
        CrowdKind& k = *kp;
        if (!k.StateRead || k.Samples.empty() || !k.R)
          continue;
        // Its last sample at or before the time
        size_t lo = 0, hi = k.Samples.size();
        while (lo < hi)
        {
          size_t mid = (lo + hi) / 2;
          if (g_CrowdSamples[k.Samples[mid]].Time <= time)
            lo = mid + 1;
          else
            hi = mid;
        }
        if (lo == 0)
          continue;
        CrowdSample const& s0 = g_CrowdSamples[k.Samples[lo - 1]];
        CrowdSample const* s1 = lo < k.Samples.size() ? &g_CrowdSamples[k.Samples[lo]] : nullptr;
        double gap = s1 ? s1->Time - s0.Time : 1e9;
        bool line = s1 && gap > 0.0 && gap <= CrowdHoldSeconds;
        if (!line && time - s0.Time > CrowdHoldSeconds)
          continue;   // not drawn then
        Work w;
        w.K = kp;
        w.B1 = s0.B1;
        w.Transforms = s0.Transforms;
        w.Animations = s0.Animations;
        w.Count = (UINT)s0.Members.size();
        if (line)
        {
          float f = (float)((time - s0.Time) / gap);
          std::unordered_map<UINT, size_t> next;
          for (size_t m = 0; m < s1->Members.size(); ++m)
            next[s1->Members[m]] = m;
          for (size_t m = 0; m < s0.Members.size(); ++m)
          {
            auto it = next.find(s0.Members[m]);
            if (it == next.end())
              continue;
            float* to = &w.Transforms[m * 4];
            float const* b = &s1->Transforms[it->second * 4];
            for (int c = 0; c < 3; ++c)
              to[c] += (b[c] - to[c]) * f;
          }
        }
        work.push_back(std::move(w));
      }
    }
    if (work.empty())
      return;
    ComPtr<ID3D11Device> device;
    g_Immediate->GetDevice(device.GetAddressOf());
    if (!device)
      return;
    // The tools' buffers: a kind's members, numbered from 0, and b1 with its first entry 0
    auto ensure = [&](CrowdKind& k, UINT count) {
      if (k.OwnCapacity >= count && k.Own[Crowd_Indirection] && k.Own[Crowd_Transform])
        return true;
      UINT capacity = 64;
      while (capacity < count)
        capacity *= 2;
      for (int b = 0; b < CrowdBuffers; ++b)
      {
        k.Own[b].Reset();
        k.OwnView[b].Reset();
        if (k.Slot[b] < 0)
          continue;
        UINT element = CrowdElementBytes(k.Format[b]);
        if (!element)
          return false;
        D3D11_BUFFER_DESC d{};
        d.ByteWidth = (capacity * element + 15) & ~15u;
        d.Usage = D3D11_USAGE_DYNAMIC;
        d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        d.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (FAILED(device->CreateBuffer(&d, nullptr, k.Own[b].GetAddressOf())))
          return false;
        D3D11_SHADER_RESOURCE_VIEW_DESC v{};
        v.Format = k.Format[b];
        v.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        v.Buffer.FirstElement = 0;
        v.Buffer.NumElements = capacity;
        if (FAILED(device->CreateShaderResourceView(k.Own[b].Get(), &v, k.OwnView[b].GetAddressOf())))
          return false;
      }
      k.OwnCapacity = capacity;
      return true;
    };
    std::vector<Work*> drawn;
    for (Work& w : work)
    {
      CrowdKind& k = *w.K;
      Recipe& r = *k.R;
      if (!w.Count || !ensure(k, w.Count) || w.B1.size() < 16)
        continue;
      bool ok = true;
      for (int b = 0; b < CrowdBuffers && ok; ++b)
      {
        if (!k.Own[b])
          continue;
        D3D11_MAPPED_SUBRESOURCE m{};
        if (FAILED(g_Immediate->Map(k.Own[b].Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &m)) || !m.pData)
        {
          ok = false;
          break;
        }
        if (b == Crowd_Indirection)
        {
          bool shorts = CrowdElementBytes(k.Format[b]) == 2;
          for (UINT i = 0; i < w.Count; ++i)
            if (shorts)
              ((unsigned short*)m.pData)[i] = (unsigned short)i;
            else
              ((UINT*)m.pData)[i] = i;
        }
        else if (b == Crowd_Transform)
          memcpy(m.pData, w.Transforms.data(), (size_t)w.Count * 16);
        else
        {
          for (UINT i = 0; i < w.Count; ++i)
            ((UINT*)m.pData)[i] = i < w.Animations.size() ? w.Animations[i] : 0;
        }
        g_Immediate->Unmap(k.Own[b].Get(), 0);
      }
      if (!ok)
        continue;
      UINT b1Bytes = (UINT)((w.B1.size() + 15) & ~(size_t)15);
      if (!k.OwnB1 || k.OwnB1Bytes < b1Bytes)
      {
        k.OwnB1.Reset();
        D3D11_BUFFER_DESC d{};
        d.ByteWidth = b1Bytes;
        d.Usage = D3D11_USAGE_DYNAMIC;
        d.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        d.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (FAILED(device->CreateBuffer(&d, nullptr, k.OwnB1.GetAddressOf())))
          continue;
        k.OwnB1Bytes = b1Bytes;
      }
      {
        D3D11_MAPPED_SUBRESOURCE m{};
        if (FAILED(g_Immediate->Map(k.OwnB1.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &m)) || !m.pData)
          continue;
        memset(m.pData, 0, k.OwnB1Bytes);
        memcpy(m.pData, w.B1.data(), w.B1.size());
        *(UINT*)m.pData = 0;   // its first entry: the tools' buffer starts with its members
        g_Immediate->Unmap(k.OwnB1.Get(), 0);
      }
      if (!r.VsB0Own && !r.VsB0Bytes.empty())
        r.VsB0Own = ConstantsWith(device.Get(), r.VsB0Bytes, false);
      if (!r.PsB0Own && !r.PsB0Bytes.empty())
        r.PsB0Own = ConstantsWith(device.Get(), r.PsB0Bytes, false);
      if (!k.PsB1IsVsB1 && !r.PsB1Copy && !r.PsB1Bytes.empty())
        r.PsB1Copy = ConstantsWith(device.Get(), r.PsB1Bytes, false);
      drawn.push_back(&w);
    }
    if (drawn.empty())
      return;
    GameState saved;
    saved.Save(g_Immediate1.Get());
    ID3D11DeviceContext1* c = g_Immediate1.Get();
    ID3D11ShaderResourceView* vsHigh[CrowdVsViewSlots - VsViewSlots] = {};
    c->VSGetShaderResources(VsViewSlots, CrowdVsViewSlots - VsViewSlots, vsHigh);
    t_Drawing = true;
    UINT members = 0;
    for (Work* w : drawn)
    {
      CrowdKind& k = *w->K;
      Recipe& r = *k.R;
      c->VSSetShader(r.Vs.Get(), nullptr, 0);
      c->PSSetShader(r.Ps.Get(), nullptr, 0);
      c->GSSetShader(r.Gs.Get(), nullptr, 0);
      c->HSSetShader(r.Hs.Get(), nullptr, 0);
      c->DSSetShader(r.Ds.Get(), nullptr, 0);
      ID3D11Buffer* vsCb[3] = { r.VsB0Own.Get(), k.OwnB1.Get(), g_View.Get() };
      ID3D11Buffer* psCb[3] = { r.PsB0Own.Get(), k.PsB1IsVsB1 ? k.OwnB1.Get() : r.PsB1Copy.Get(), g_View.Get() };
      c->VSSetConstantBuffers(0, 3, vsCb);
      c->PSSetConstantBuffers(0, 3, psCb);
      ID3D11ShaderResourceView* vsViews[CrowdVsViewSlots] = {};
      for (int i = 0; i < VsViewSlots; ++i)
        vsViews[i] = r.VsViews[i].Get();
      ID3D11ShaderResourceView* psViews[PsViewSlots] = {};
      for (int i = 0; i < PsViewSlots; ++i)
        psViews[i] = r.PsInstanceSlot[i] ? nullptr : r.PsViews[i].Get();
      for (int b = 0; b < CrowdBuffers; ++b)
      {
        if (k.Slot[b] >= 0)
          vsViews[k.Slot[b]] = k.OwnView[b].Get();
        if (k.PsSlot[b] >= 0)
          psViews[k.PsSlot[b]] = k.OwnView[b].Get();
      }
      c->VSSetShaderResources(0, CrowdVsViewSlots, vsViews);
      c->PSSetShaderResources(0, PsViewSlots, psViews);
      ID3D11SamplerState* vsSamplers[VsSamplerSlots];
      for (int i = 0; i < VsSamplerSlots; ++i)
        vsSamplers[i] = r.VsSamplers[i].Get();
      ID3D11SamplerState* psSamplers[PsSamplerSlots];
      for (int i = 0; i < PsSamplerSlots; ++i)
        psSamplers[i] = r.PsSamplers[i].Get();
      c->VSSetSamplers(0, VsSamplerSlots, vsSamplers);
      c->PSSetSamplers(0, PsSamplerSlots, psSamplers);
      c->IASetInputLayout(r.Layout.Get());
      c->IASetPrimitiveTopology(r.Topology);
      ID3D11Buffer* vbs[VbSlots];
      for (int i = 0; i < VbSlots; ++i)
        vbs[i] = r.Vbs[i].Get();
      c->IASetVertexBuffers(0, VbSlots, vbs, r.Strides, r.Offsets);
      c->IASetIndexBuffer(r.Ib.Get(), r.IbFormat, r.IbOffset);
      c->OMSetDepthStencilState(r.Depth.Get(), r.StencilRef);
      c->OMSetBlendState(r.Blend.Get(), r.BlendFactor, r.SampleMask);
      c->RSSetState(r.Raster.Get());
      c->DrawIndexedInstanced(r.IndexCount, w->Count, r.StartIndex, r.BaseVertex, 0);
      members += w->Count;
    }
    t_Drawing = false;
    c->VSSetShaderResources(VsViewSlots, CrowdVsViewSlots - VsViewSlots, vsHigh);
    for (auto v : vsHigh)
      if (v)
        v->Release();
    saved.Restore(g_Immediate1.Get());
    g_DrawCalls += drawn.size();
    if (!g_LoggedCrowds)
    {
      g_LoggedCrowds = true;
      util::log::Write("Replay: %u crowd members drawn from the recording (%d kinds of crowd draws now); the game's own crowds aren't "
        "drawn while it plays", members, (int)drawn.size());
    }
  }

  // The runner's vision layer while it plays (see Pass_Rv): the replay's own
  // mask (R8_UNORM, the frame's size, like the game's), the frame it was
  // last drawn in (g_Frame then; ApplyRunnersVision mixes it in at that
  // frame's Present), whether it was tried this frame, and the mix's own
  // shaders, states and copy of the frame
  ComPtr<ID3D11Texture2D> g_RvMask;
  ComPtr<ID3D11RenderTargetView> g_RvMaskTarget;
  ComPtr<ID3D11ShaderResourceView> g_RvMaskView;
  UINT g_RvMaskWidth = 0, g_RvMaskHeight = 0;
  unsigned g_RvMaskFrame = 0;
  bool g_RvMaskDrawn = false;
  bool g_RvTried = false;
  bool g_RvFailed = false;
  bool g_RvLoggedDrawn = false, g_RvLoggedMixed = false, g_RvLoggedUavs = false;
  ComPtr<ID3D11VertexShader> g_RvVs;
  ComPtr<ID3D11PixelShader> g_RvPs;
  ComPtr<ID3D11Buffer> g_RvConstants;
  ComPtr<ID3D11RasterizerState> g_RvRaster;
  ComPtr<ID3D11BlendState> g_RvBlend;
  ComPtr<ID3D11DepthStencilState> g_RvDepth;
  ComPtr<ID3D11Texture2D> g_RvCopy;
  ComPtr<ID3D11ShaderResourceView> g_RvCopyView;
  D3D11_TEXTURE2D_DESC g_RvCopyDesc{};
  D3DStateBackup g_RvBackup;   // (big: its class instance lists)

  // The game's runner's vision colours, from its data: the city's visual
  // environment VE_ToD_PostEffects' RunnersVisionComponentData (the only one
  // with its colour; gameplay/fadeenvironment sets the two amounts to 0 while
  // the screen fades): OverlayColor (0.7, 0.05, 0.05), AlphaBlendAmount 0.463,
  // AdditiveBlendAmount 0.493, ColorBurnEnable True. The tonemap's
  // runnersVisionColor is max(OverlayColor, 0.001), its preBlendAmount the
  // AlphaBlendAmount and its postAddAmount the AdditiveBlendAmount
  // (0x1437469A2 - 0x1437469D5 take them from the renderer's copy of that
  // component, + 0x86210, + 0x86224, + 0x86228: the component's own + 0x80,
  // + 0x94, + 0x98)
  const float RvOverlayColor[3] = { 0.7f, 0.05f, 0.05f };
  const float RvAlphaBlendAmount = 0.463f, RvAdditiveBlendAmount = 0.493f;

  // A view format for a typeless one (the copy is made in the frame's own)
  DXGI_FORMAT RvViewFormat(DXGI_FORMAT format)
  {
    switch (format)
    {
    case DXGI_FORMAT_R8G8B8A8_TYPELESS: return DXGI_FORMAT_R8G8B8A8_UNORM;
    case DXGI_FORMAT_B8G8R8A8_TYPELESS: return DXGI_FORMAT_B8G8R8A8_UNORM;
    case DXGI_FORMAT_B8G8R8X8_TYPELESS: return DXGI_FORMAT_B8G8R8X8_UNORM;
    case DXGI_FORMAT_R10G10B10A2_TYPELESS: return DXGI_FORMAT_R10G10B10A2_UNORM;
    case DXGI_FORMAT_R16G16B16A16_TYPELESS: return DXGI_FORMAT_R16G16B16A16_FLOAT;
    default: return format;
    }
  }

  // A frame whose views read and write linear values (the mix works on the
  // gamma-encoded colour the game's tonemap writes)
  bool RvLinearViews(DXGI_FORMAT format)
  {
    switch (format)
    {
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
      return true;
    default:
      return false;
    }
  }

  // The end of the game's G-buffer pass while it plays (its targets still
  // bound): the recorded models' kept draws of the runner's vision layer
  // drawn into the replay's mask, with the game's depth target of that pass
  // (as the game's own layer is drawn with it), then the game's targets put
  // back. Once a frame.
  void DrawRvLayer()
  {
    if (g_RvTried || g_RvFailed || !g_Immediate1 || !g_FrameWidth || !g_FrameHeight)
      return;
    g_RvTried = true;
    {
      std::lock_guard<std::mutex> lock(g_Mutex);
      if (g_Draws.empty())
        return;
      bool any = false;
      for (auto const& entry : g_Recipes)
        if (entry.second->Ready && entry.second->Pass == Pass_Rv)
        {
          any = true;
          break;
        }
      if (!any)
        return;
    }
    ID3D11RenderTargetView* targets[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
    ID3D11DepthStencilView* depth = nullptr;
    g_Immediate->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, targets, &depth);
    auto releaseTargets = [&]() {
      for (auto t : targets)
        if (t)
          t->Release();
      if (depth)
        depth->Release();
    };
    if (!depth || !FrameDepth(depth))
    {
      releaseTargets();
      return;
    }
    // Unordered access views bound with the targets: the replay's own target
    // change would let them go (the game may keep them for its next pass), so
    // the layer isn't drawn then
    {
      ID3D11UnorderedAccessView* uavs[D3D11_PS_CS_UAV_REGISTER_COUNT] = {};
      g_Immediate->OMGetRenderTargetsAndUnorderedAccessViews(0, nullptr, nullptr, 0, D3D11_PS_CS_UAV_REGISTER_COUNT, uavs);
      bool any = false;
      for (auto u : uavs)
        if (u)
        {
          any = true;
          u->Release();
        }
      if (any)
      {
        if (!g_RvLoggedUavs)
        {
          g_RvLoggedUavs = true;
          util::log::Write("Replay: the game's G-buffer pass ended with unordered access views bound: the Dash markers' layer isn't "
            "drawn in such a frame");
        }
        releaseTargets();
        return;
      }
    }
    if (!g_RvMask || g_RvMaskWidth != g_FrameWidth || g_RvMaskHeight != g_FrameHeight)
    {
      g_RvMaskView.Reset();
      g_RvMaskTarget.Reset();
      g_RvMask.Reset();
      g_RvMaskWidth = g_RvMaskHeight = 0;
      ComPtr<ID3D11Device> device;
      g_Immediate->GetDevice(device.GetAddressOf());
      D3D11_TEXTURE2D_DESC d{};
      d.Width = g_FrameWidth;
      d.Height = g_FrameHeight;
      d.MipLevels = 1;
      d.ArraySize = 1;
      d.Format = DXGI_FORMAT_R8_UNORM;
      d.SampleDesc.Count = 1;
      d.Usage = D3D11_USAGE_DEFAULT;
      d.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
      HRESULT hr = device ? device->CreateTexture2D(&d, nullptr, g_RvMask.GetAddressOf()) : E_FAIL;
      if (SUCCEEDED(hr))
        hr = device->CreateRenderTargetView(g_RvMask.Get(), nullptr, g_RvMaskTarget.GetAddressOf());
      if (SUCCEEDED(hr))
        hr = device->CreateShaderResourceView(g_RvMask.Get(), nullptr, g_RvMaskView.GetAddressOf());
      if (FAILED(hr))
      {
        g_RvMaskView.Reset();
        g_RvMaskTarget.Reset();
        g_RvMask.Reset();
        g_RvFailed = true;
        util::log::Warning("Replay: the runner's vision mask couldn't be made (%u x %u, HRESULT 0x%X): the Dash's markers aren't "
          "drawn", g_FrameWidth, g_FrameHeight, hr);
        releaseTargets();
        return;
      }
      g_RvMaskWidth = g_FrameWidth;
      g_RvMaskHeight = g_FrameHeight;
    }
    // The mask cleared and bound with the game's depth target (the replay's
    // own target changes aren't looked at: t_Drawing)
    const float clear[4] = { 0.f, 0.f, 0.f, 0.f };
    t_Drawing = true;
    g_Immediate->ClearRenderTargetView(g_RvMaskTarget.Get(), clear);
    ID3D11RenderTargetView* mask = g_RvMaskTarget.Get();
    g_Immediate->OMSetRenderTargets(1, &mask, depth);
    t_Drawing = false;
    unsigned long long before = g_DrawCalls;
    DrawRecipes(Pass_Rv, 0);
    g_RvMaskDrawn = g_DrawCalls != before;
    if (g_RvMaskDrawn)
      g_RvMaskFrame = g_Frame;
    // The game's G-buffer targets back as they were
    t_Drawing = true;
    g_Immediate->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, targets, depth);
    t_Drawing = false;
    releaseTargets();
    if (g_RvMaskDrawn && !g_RvLoggedDrawn)
    {
      g_RvLoggedDrawn = true;
      util::log::Write("Replay: the recorded Dash markers (the game's runner's vision layer) drawn with the game's own draws of them "
        "into the replay's own mask (%u x %u), mixed into the picture at Present as the game's tonemap mixes its own",
        g_RvMaskWidth, g_RvMaskHeight);
    }
  }

  bool MakeRvMix(ID3D11Device* pDevice)
  {
    D3D11_BUFFER_DESC constants{};
    constants.ByteWidth = 32;
    constants.Usage = D3D11_USAGE_DYNAMIC;
    constants.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    constants.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    D3D11_RASTERIZER_DESC rasterizer{};
    rasterizer.FillMode = D3D11_FILL_SOLID;
    rasterizer.CullMode = D3D11_CULL_NONE;
    rasterizer.DepthClipEnable = TRUE;
    D3D11_BLEND_DESC blend{};
    blend.RenderTarget[0].BlendEnable = FALSE;
    blend.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    D3D11_DEPTH_STENCIL_DESC depth{};
    depth.DepthEnable = FALSE;
    depth.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    depth.DepthFunc = D3D11_COMPARISON_ALWAYS;
    depth.StencilEnable = FALSE;
    HRESULT hr = pDevice->CreateVertexShader(g_RunnersVisionVs, sizeof(g_RunnersVisionVs), nullptr, g_RvVs.GetAddressOf());
    if (SUCCEEDED(hr)) hr = pDevice->CreatePixelShader(g_RunnersVisionPs, sizeof(g_RunnersVisionPs), nullptr, g_RvPs.GetAddressOf());
    if (SUCCEEDED(hr)) hr = pDevice->CreateBuffer(&constants, nullptr, g_RvConstants.GetAddressOf());
    if (SUCCEEDED(hr)) hr = pDevice->CreateRasterizerState(&rasterizer, g_RvRaster.GetAddressOf());
    if (SUCCEEDED(hr)) hr = pDevice->CreateBlendState(&blend, g_RvBlend.GetAddressOf());
    if (SUCCEEDED(hr)) hr = pDevice->CreateDepthStencilState(&depth, g_RvDepth.GetAddressOf());
    if (FAILED(hr))
    {
      g_RvFailed = true;
      util::log::Error("Replay: the runner's vision mix's shaders or states couldn't be made (HRESULT 0x%X): the Dash's markers "
        "aren't drawn", hr);
      return false;
    }
    return true;
  }

  // The render targets change on the immediate context: leaving the main
  // view's G-buffer is the end of its pass
  void Targets(ID3D11DeviceContext* pContext, UINT count, ID3D11RenderTargetView* const* targets, ID3D11DepthStencilView* depth)
  {
    if (t_Drawing || g_shutdown || !IsImmediate(pContext))
      return;
    // The draw functions the game's context has now (d3d11 changes them while it runs; see ReplayShapes' MaxMoreDraws)
    replay_shapes::NoteContextDraws(pContext);
    int pass = PassOf(count, targets, depth);
    bool main = pass == Pass_Main;
    if (main && !g_InMain)
    {
      double clock = g_Clock.load();
      g_TrackPass = g_TrackCapturing && clock - g_LastTrackSample >= TrackSampleSeconds;
      if (g_TrackPass)
        g_LastTrackSample = clock;
      g_PendingCrowds = 0;
    }
    if (g_Pass != pass)
    {
      // (the snow's copies of the G-buffer and the scene's depth target, GameFrame.h)
      gameframe::OnPass(pContext, g_Pass, pass, count, targets, depth);
      // The end of a pass kept: what's kept for it drawn
      if (g_Pass != Pass_None)
      {
        CopyPending();
        bool playing = (g_Playing || g_PeoplePlaying || g_WorldPlaying || g_CrowdPlaying) && g_Enabled && g_Immediate1;
        if (playing && g_Pass == Pass_Main && !g_Injected)
          DrawRecipes(Pass_Main, 0);
        if (playing && g_Pass == Pass_Main && g_CrowdPlaying && g_TracksOn)
          DrawCrowds();
        else if (playing && g_Pass != Pass_Main && g_Pass != Pass_Env && g_Pass != Pass_Rv && g_PassOrdinal < MaxPassOrdinals &&
          !g_InjectedPass[g_Pass][g_PassOrdinal])
        {
          g_InjectedPass[g_Pass][g_PassOrdinal] = true;
          DrawRecipes(g_Pass, g_PassOrdinal);
          // The last pass of its kind every frame lately has (see
          // g_PassFewest): the kept draws of the later numbers drawn here too,
          // the ones not drawn yet this frame
          int fewest = g_PassFewest[g_Pass];
          if (fewest > 0 && g_PassOrdinal + 1 == fewest && fewest < MaxPassOrdinals)
          {
            int from = fewest, to = MaxPassOrdinals - 1;
            while (from <= to && g_InjectedPass[g_Pass][from])
              ++from;
            for (int o = from; o <= to; ++o)
              g_InjectedPass[g_Pass][o] = true;
            if (from <= to)
            {
              DrawRecipes(g_Pass, from, to);
              ++g_PassLogLate[g_Pass];
            }
          }
        }
        // The runner's vision layer's (see Pass_Rv), last: the G-buffer's targets are put back after it
        if (playing && g_Pass == Pass_Main)
          DrawRvLayer();
      }
      // Another pass: its place among its kind's after the G-buffer pass
      if (main)
        g_SawMain = true;
      else if (pass == Pass_Env)
        g_PassOrdinal = 0;
      else if (pass != Pass_None)
        g_PassOrdinal = g_SawMain ? g_PassCounts[pass]++ : MaxPassOrdinals;
    }
    g_Pass = pass;
    g_InMain = main;
  }

  void STDMETHODCALLTYPE hOMSetRenderTargets(ID3D11DeviceContext* pContext, UINT count, ID3D11RenderTargetView* const* targets,
    ID3D11DepthStencilView* depth)
  {
    Targets(pContext, count, targets, depth);
    oOMSetRenderTargets(pContext, count, targets, depth);
  }

  void STDMETHODCALLTYPE hOMSetRenderTargetsUav(ID3D11DeviceContext* pContext, UINT count, ID3D11RenderTargetView* const* targets,
    ID3D11DepthStencilView* depth, UINT uavStart, UINT uavs, ID3D11UnorderedAccessView* const* views, UINT const* counts)
  {
    if (count != D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL)
      Targets(pContext, count, targets, depth);
    oOMSetRenderTargetsUav(pContext, count, targets, depth, uavStart, uavs, views, counts);
  }
}

void replay_gamedraws::Hook(ID3D11DeviceContext* pImmediate)
{
  if (g_Immediate || !pImmediate)
    return;
  g_Immediate = pImmediate;
  if (FAILED(pImmediate->QueryInterface(__uuidof(ID3D11DeviceContext1), (void**)g_Immediate1.GetAddressOf())) || !g_Immediate1)
  {
    util::log::Warning("Replay: the game's context has no D3D 11.1 interface: the recorded models are drawn as shapes only");
    return;
  }
  void** table = *(void***)pImmediate;
  bool a = util::hooks::CreateGameHook("ReplayGameDrawsTargets", (__int64)table[OmSetRenderTargetsSlot], (PVOID)hOMSetRenderTargets,
    (LPVOID*)&oOMSetRenderTargets);
  bool b = util::hooks::CreateGameHook("ReplayGameDrawsTargetsUav", (__int64)table[OmSetRenderTargetsUavSlot],
    (PVOID)hOMSetRenderTargetsUav, (LPVOID*)&oOMSetRenderTargetsUav);
  if (!a || !b)
    util::log::Warning("Replay: the game's render target changes couldn't be hooked (%d %d): the recorded models are drawn as shapes "
      "only", (int)a, (int)b);
}

// The recording's cost in the log every CostLogMs while it records
// (2026-11-01, the user: "game lags/drop fps when recording a replay"): the
// replay's draw hooks (ReplayShapes, every draw of the game's), its work at
// Present, the copies it reads back from the graphics card, the staging
// buffers made and reused
// 2026-11-04 (the user: "find out why fps drop when recording a replay and
// fix the fps issue no mistakes"): its render thread's work by part too, each
// part's most in one frame, and how many frames it took over 2 and 5 ms in
// (the drop was mostly slow frames: the 95th percentile frame 3 - 8 ms longer
// while it recorded, the median about the same)
namespace
{
  const ULONGLONG CostLogMs = 10000;
  // The parts' time since the last Present (any thread adds to it)
  std::atomic<long long> g_CostTicks[replay_gamedraws::CostParts];
  // Over the CostLogMs: each part's total and its most in a frame, the draw
  // hooks' too, the whole's; the frames whose whole took over 2 and 5 ms
  long long g_CostTotal[replay_gamedraws::CostParts] = {}, g_CostMost[replay_gamedraws::CostParts] = {};
  long long g_CostHookTicks = 0, g_CostHookDraws = 0, g_CostHookMost = 0, g_CostWork = 0, g_CostWorkMost = 0;
  int g_CostOver2 = 0, g_CostOver5 = 0;

  void ResetCost(ULONGLONG now)
  {
    g_CostSince = now;
    g_PresentTicks = 0;
    g_StagedBytesNow = 0;
    g_CostFrames = 0;
    g_StagingMade = g_StagingReused = 0;
    memset(g_CostTotal, 0, sizeof(g_CostTotal));
    memset(g_CostMost, 0, sizeof(g_CostMost));
    g_CostHookTicks = g_CostHookDraws = g_CostHookMost = g_CostWork = g_CostWorkMost = 0;
    g_CostOver2 = g_CostOver5 = 0;
  }

  void LogCost()
  {
    using namespace replay_gamedraws;
    ULONGLONG now = GetTickCount64();
    bool recording = g_Capturing.load(std::memory_order_relaxed);
    // The frame's parts, taken every frame (what's added while it doesn't record isn't counted later)
    long long parts[CostParts];
    for (int p = 0; p < CostParts; ++p)
      parts[p] = g_CostTicks[p].exchange(0, std::memory_order_relaxed);
    long long hookTicks = 0, draws = 0;
    replay_shapes::TakeHookCost(hookTicks, draws);
    if (!recording)
    {
      g_CostSince = 0;
      return;
    }
    if (!g_CostSince)
    {
      ResetCost(now);
      return;
    }
    static LARGE_INTEGER frequency{};
    if (!frequency.QuadPart)
      QueryPerformanceFrequency(&frequency);
    ++g_CostFrames;
    g_CostHookTicks += hookTicks;
    g_CostHookDraws += draws;
    g_CostHookMost = (std::max)(g_CostHookMost, hookTicks);
    for (int p = 0; p < CostParts; ++p)
    {
      g_CostTotal[p] += parts[p];
      g_CostMost[p] = (std::max)(g_CostMost[p], parts[p]);
    }
    // The whole: the parts that aren't in another's time
    long long work = hookTicks + parts[Cost_Copies] + parts[Cost_ReadBack] + parts[Cost_Crowds] + parts[Cost_AssetPresent] +
      parts[Cost_DrawLogPresent];
    g_CostWork += work;
    g_CostWorkMost = (std::max)(g_CostWorkMost, work);
    g_CostOver2 += work * 1000 > frequency.QuadPart * 2;
    g_CostOver5 += work * 1000 > frequency.QuadPart * 5;
    if (now - g_CostSince < CostLogMs || !g_CostFrames)
      return;
    double seconds = (now - g_CostSince) / 1000.0;
    auto ms = [](long long ticks) {
      return ticks * 1000.0 / frequency.QuadPart;
    };
    double frames = g_CostFrames;
    util::log::Write("Replay (recording cost): %.0f frames a second; its draw hooks %.2f ms a frame (%.0f draws a frame, %.2f us each), "
      "its work at Present %.2f ms a frame, %.1f MB a second copied back from the graphics card, %d staging buffers made and %d "
      "reused", frames / seconds, ms(g_CostHookTicks) / frames, g_CostHookDraws / frames,
      g_CostHookDraws ? ms(g_CostHookTicks) * 1000.0 / g_CostHookDraws : 0.0, ms(g_PresentTicks) / frames,
      g_StagedBytesNow / 1048576.0 / seconds, g_StagingMade, g_StagingReused);
    auto avg = [&](int p) {
      return ms(g_CostTotal[p]) / frames;
    };
    auto most = [&](int p) {
      return ms(g_CostMost[p]);
    };
    util::log::Write("Replay (recording cost, render thread, diagnostic): its work %.2f ms a frame, the most in a frame %.2f ms (%d of "
      "%.0f frames over 2 ms, %d over 5 ms); by part, ms a frame (the most in a frame): its draw hooks %.2f (%.2f; in them the game's "
      "draws kept %.2f (%.2f), the asset log's sampled draws %.2f (%.2f), the draw log's %.2f (%.2f)), the kept draws' copies at "
      "their passes' ends %.2f (%.2f; staging buffers made in them %.2f (%.2f)), the copies read back and decoded %.2f (%.2f; their "
      "maps %.2f (%.2f)), the crowds' copies read %.2f (%.2f), the asset log's frame %.2f (%.2f), the draw log's %.2f (%.2f)",
      ms(g_CostWork) / frames, ms(g_CostWorkMost), g_CostOver2, frames, g_CostOver5, ms(g_CostHookTicks) / frames, ms(g_CostHookMost),
      avg(Cost_Captures), most(Cost_Captures), avg(Cost_AssetDraws), most(Cost_AssetDraws), avg(Cost_DrawLogDraws),
      most(Cost_DrawLogDraws), avg(Cost_Copies), most(Cost_Copies), avg(Cost_Staging), most(Cost_Staging), avg(Cost_ReadBack),
      most(Cost_ReadBack), avg(Cost_Maps), most(Cost_Maps), avg(Cost_Crowds), most(Cost_Crowds), avg(Cost_AssetPresent),
      most(Cost_AssetPresent), avg(Cost_DrawLogPresent), most(Cost_DrawLogPresent));
    ResetCost(now);
  }
}

void replay_gamedraws::AddCost(int part, long long ticks)
{
  if (part >= 0 && part < CostParts)
    g_CostTicks[part].fetch_add(ticks, std::memory_order_relaxed);
}

void replay_gamedraws::Present(IDXGISwapChain* pSwapChain, ID3D11DeviceContext* pContext)
{
  if (!pSwapChain || !pContext || g_shutdown || !IsImmediate(pContext))
    return;
  LARGE_INTEGER presentStart;
  QueryPerformanceCounter(&presentStart);
  struct Timed
  {
    LARGE_INTEGER Start;
    ~Timed()
    {
      LARGE_INTEGER end;
      QueryPerformanceCounter(&end);
      g_PresentTicks += end.QuadPart - Start.QuadPart;
    }
  } timed{ presentStart };
  LogCost();
  ++g_Frame;
  if ((!g_CrowdProbes.empty() || !g_CrowdCopies.empty()) && g_Immediate1)
  {
    CostTimer timer(Cost_Crowds);
    if (!g_CrowdProbes.empty())
      ReadCrowdProbes();
    if (!g_CrowdCopies.empty())
      ReadCrowds();
  }
  g_CrowdInjected = false;
  // What the game drew lately, for the game thread (see GameDrawsModel)
  if (g_Frame % 10 == 0 && g_WorldPlaying.load(std::memory_order_relaxed))
  {
    std::lock_guard<std::mutex> lock(g_Mutex);
    g_MeshDrawnShared = g_MeshDrawnAt;
    g_MeshDrawnOtherShared = g_MeshDrawnOtherAt;
    g_FrameShared = g_Frame;
  }
  // The frame just drawn: how many passes of each kind it had after its
  // G-buffer pass, the most seen while it records (in the log when it grows)
  if (g_SawMain && g_Capturing.load(std::memory_order_relaxed))
  {
    static int most[Passes] = {};
    static int logs = 0;
    bool more = false;
    for (int k = 1; k < Passes; ++k)
      if (g_PassCounts[k] > most[k])
      {
        most[k] = g_PassCounts[k];
        more = true;
      }
    if (more && logs < 12)
    {
      ++logs;
      util::log::Write("Replay (diagnostic): a frame with %d decal, %d lit picture's and %d half-size transparent passes after its "
        "G-buffer pass (the most so far; the replay keeps the first %d of each kind)", g_PassCounts[Pass_Decal], g_PassCounts[Pass_Lit],
        g_PassCounts[Pass_Half], MaxPassOrdinals);
    }
  }
  // The frame's passes of each kind (one whose view had its G-buffer pass),
  // the fewest of the last PassHistoryFrames that had any (see g_PassFewest)
  if (g_SawMain)
  {
    for (int k = 0; k < Passes; ++k)
      g_PassHistory[k][g_PassHistoryAt] = g_PassCounts[k];
    g_PassHistoryAt = (g_PassHistoryAt + 1) % PassHistoryFrames;
    g_PassHistoryFilled = (std::min)(g_PassHistoryFilled + 1, PassHistoryFrames);
    for (int k = 0; k < Passes; ++k)
    {
      int fewest = 0;
      for (int i = 0; i < g_PassHistoryFilled; ++i)
        if (g_PassHistory[k][i] > 0 && (fewest == 0 || g_PassHistory[k][i] < fewest))
          fewest = g_PassHistory[k][i];
      g_PassFewest[k] = fewest;
      if (g_PassCounts[k] > 0 && (g_PassLogFewest[k] == 0 || g_PassCounts[k] < g_PassLogFewest[k]))
        g_PassLogFewest[k] = g_PassCounts[k];
      g_PassLogMost[k] = (std::max)(g_PassLogMost[k], g_PassCounts[k]);
    }
  }
  {
    ULONGLONG now = GetTickCount64();
    bool playing = g_Playing.load(std::memory_order_relaxed) || g_WorldPlaying.load(std::memory_order_relaxed);
    if (!g_PassLogSince)
      g_PassLogSince = now;
    else if (now - g_PassLogSince >= PassLogSeconds * 1000)
    {
      if (playing)
        util::log::Write("Replay (diagnostic): the last %llu s's frames had %d - %d decal, %d - %d lit picture's and %d - %d half-size "
          "passes after the G-buffer pass; kept draws of a later pass than the fewest of the last %d frames were drawn at the end "
          "of that last one in %d, %d and %d frames", (unsigned long long)((now - g_PassLogSince) / 1000), g_PassLogFewest[Pass_Decal],
          g_PassLogMost[Pass_Decal], g_PassLogFewest[Pass_Lit], g_PassLogMost[Pass_Lit], g_PassLogFewest[Pass_Half],
          g_PassLogMost[Pass_Half], PassHistoryFrames, g_PassLogLate[Pass_Decal], g_PassLogLate[Pass_Lit], g_PassLogLate[Pass_Half]);
      g_PassLogSince = now;
      memset(g_PassLogFewest, 0, sizeof(g_PassLogFewest));
      memset(g_PassLogMost, 0, sizeof(g_PassLogMost));
      memset(g_PassLogLate, 0, sizeof(g_PassLogLate));
    }
  }
  g_Injected = false;
  g_ViewCopied = false;
  g_RvTried = false;
  g_InMain = false;
  g_Pass = Pass_None;
  g_SawMain = false;
  memset(g_PassCounts, 0, sizeof(g_PassCounts));
  memset(g_InjectedPass, 0, sizeof(g_InjectedPass));
  DXGI_SWAP_CHAIN_DESC desc{};
  if (SUCCEEDED(pSwapChain->GetDesc(&desc)) && (desc.BufferDesc.Width != g_FrameWidth || desc.BufferDesc.Height != g_FrameHeight))
  {
    g_FrameWidth = desc.BufferDesc.Width;
    g_FrameHeight = desc.BufferDesc.Height;
    g_TargetKinds.clear();
    // (and the other passes' kinds: told apart by the frame's size too)
    g_PassKinds.clear();
  }
  // The replay's own drawing's time, now and then while it draws
  if (g_DrawFrames >= TimingLogFrames)
  {
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    util::log::Write("Replay: its own drawing in the G-buffer pass took %.2f ms a frame on average (on the processor, the last %u "
      "frames it drew in; %.0f draws a frame)", g_DrawTicks * 1000.0 / frequency.QuadPart / g_DrawFrames, g_DrawFrames,
      (double)g_DrawCalls / g_DrawFrames);
    g_DrawTicks = 0;
    g_DrawFrames = 0;
    g_DrawCalls = 0;
  }
  // The game's meshes by vertex buffer, as UpdateWorld last read them
  {
    std::lock_guard<std::mutex> lock(g_Mutex);
    g_GameVbMeshesNow = g_WorldPlaying ? g_GameVbMeshes : nullptr;
  }
  // A pass's kept recipes whose end wasn't seen: copied now
  CopyPending();
  if (!g_Batches.empty())
  {
    ComPtr<ID3D11Device> device;
    pContext->GetDevice(device.GetAddressOf());
    if (device)
      ReadBack(device.Get());
  }
}

// The runner's vision layer mixed in (see Pass_Rv and DrawRvLayer): only in
// a frame whose G-buffer pass drew a recorded Dash marker into the replay's
// mask, after the replay's shapes, before the pictures in the world, the
// green screen and the lens distortion (as the game's tonemap mixes its own
// layer before the tools draw over its picture). The finished frame is
// copied and drawn over with the game's mix (RunnersVision.hlsl); the
// context's state put back.
void replay_gamedraws::ApplyRunnersVision(IDXGISwapChain* pSwapChain, ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
  if (!pSwapChain || !pDevice || !pContext || g_shutdown || g_RvFailed || !g_RvMaskDrawn || !g_RvMaskView ||
      g_RvMaskFrame + 1 != g_Frame || !g_Enabled.load(std::memory_order_relaxed))
    return;
  if (!g_RvVs && !MakeRvMix(pDevice))
    return;
  ComPtr<ID3D11Texture2D> pBack;
  if (FAILED(pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)pBack.GetAddressOf())) || !pBack)
    return;
  D3D11_TEXTURE2D_DESC back{};
  pBack->GetDesc(&back);
  if (back.SampleDesc.Count != 1 || back.ArraySize != 1 || back.Width != g_RvMaskWidth || back.Height != g_RvMaskHeight)
    return;
  if (!g_RvCopy || back.Width != g_RvCopyDesc.Width || back.Height != g_RvCopyDesc.Height || back.Format != g_RvCopyDesc.Format)
  {
    g_RvCopyView.Reset();
    g_RvCopy.Reset();
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = back.Width;
    desc.Height = back.Height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = back.Format;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SHADER_RESOURCE_VIEW_DESC view{};
    view.Format = RvViewFormat(back.Format);
    view.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    view.Texture2D.MipLevels = 1;
    HRESULT hr = pDevice->CreateTexture2D(&desc, nullptr, g_RvCopy.GetAddressOf());
    if (SUCCEEDED(hr))
      hr = pDevice->CreateShaderResourceView(g_RvCopy.Get(), &view, g_RvCopyView.GetAddressOf());
    if (FAILED(hr))
    {
      g_RvCopyView.Reset();
      g_RvCopy.Reset();
      g_RvFailed = true;
      util::log::Warning("Replay: no copy of the frame for the runner's vision mix could be made (%u x %u, format %d, HRESULT 0x%X): "
        "the Dash's markers aren't drawn", back.Width, back.Height, (int)back.Format, hr);
      return;
    }
    g_RvCopyDesc = desc;
  }
  DXGI_FORMAT viewFormat = RvViewFormat(back.Format);
  ComPtr<ID3D11RenderTargetView> pTarget;
  D3D11_RENDER_TARGET_VIEW_DESC targetDesc{};
  targetDesc.Format = viewFormat;
  targetDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
  if (FAILED(pDevice->CreateRenderTargetView(pBack.Get(), viewFormat != back.Format ? &targetDesc : nullptr, pTarget.GetAddressOf())))
    return;
  D3D11_MAPPED_SUBRESOURCE mapped;
  if (FAILED(pContext->Map(g_RvConstants.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    return;
  bool linear = RvLinearViews(viewFormat);
  float params[8] = { RvOverlayColor[0], RvOverlayColor[1], RvOverlayColor[2], 0.f, RvAlphaBlendAmount, RvAdditiveBlendAmount,
    linear ? 1.f : 0.f, 0.f };
  memcpy(mapped.pData, params, sizeof(params));
  pContext->Unmap(g_RvConstants.Get(), 0);

  g_RvBackup.Save(pContext);
  pContext->CopyResource(g_RvCopy.Get(), pBack.Get());
  D3D11_VIEWPORT viewport{ 0.f, 0.f, (float)back.Width, (float)back.Height, 0.f, 1.f };
  ID3D11Buffer* noBuffer = nullptr;
  UINT zero = 0;
  const float blendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
  pContext->OMSetRenderTargets(1, pTarget.GetAddressOf(), nullptr);
  pContext->RSSetViewports(1, &viewport);
  pContext->RSSetState(g_RvRaster.Get());
  pContext->OMSetBlendState(g_RvBlend.Get(), blendFactor, 0xFFFFFFFF);
  pContext->OMSetDepthStencilState(g_RvDepth.Get(), 0);
  pContext->IASetInputLayout(nullptr);
  pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  pContext->IASetVertexBuffers(0, 1, &noBuffer, &zero, &zero);
  pContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_R16_UINT, 0);
  pContext->VSSetShader(g_RvVs.Get(), nullptr, 0);
  pContext->GSSetShader(nullptr, nullptr, 0);
  pContext->HSSetShader(nullptr, nullptr, 0);
  pContext->DSSetShader(nullptr, nullptr, 0);
  pContext->PSSetShader(g_RvPs.Get(), nullptr, 0);
  ID3D11ShaderResourceView* views[2] = { g_RvCopyView.Get(), g_RvMaskView.Get() };
  pContext->PSSetShaderResources(0, 2, views);
  pContext->PSSetConstantBuffers(0, 1, g_RvConstants.GetAddressOf());
  pContext->Draw(3, 0);
  // The mask's slot isn't in the backup: emptied (the frame's copy's is put back)
  ID3D11ShaderResourceView* none[2] = { nullptr, nullptr };
  pContext->PSSetShaderResources(0, 2, none);
  g_RvBackup.Restore(pContext);
  if (!g_RvLoggedMixed)
  {
    g_RvLoggedMixed = true;
    util::log::Write("Replay: the runner's vision layer mixed into the picture (%u x %u, format %d%s): the game's own mix, its colour "
      "%.2f %.2f %.2f, %.3f and %.3f", back.Width, back.Height, (int)back.Format, linear ? ", read and written linear" : "",
      RvOverlayColor[0], RvOverlayColor[1], RvOverlayColor[2], RvAlphaBlendAmount, RvAdditiveBlendAmount);
  }
}

void replay_gamedraws::SetCapturing(bool on)
{
  g_Capturing = on;
}

void replay_gamedraws::SetEnabled(bool on)
{
  g_Enabled = on;
}

void replay_gamedraws::Clear()
{
  std::lock_guard<std::mutex> lock(g_Mutex);
  g_Recipes.clear();
  g_Attempts.clear();
  g_Draws.clear();
  g_Drawn.clear();
  g_Ready = 0;
  g_Playing = false;
  g_RecipeLogs = 0;
  g_FailLogs = 0;
  g_Skins.clear();
  g_SkinCandidates.clear();
  g_WantedVbs.clear();
  g_PeopleDraws.clear();
  g_PeoplePlaying = false;
  g_World.clear();
  g_WorldAttempts.clear();
  g_WorldNotKeptLogged.clear();
  g_MeshIds.clear();
  g_ModelMeshIds.clear();
  g_GameVbMeshes.reset();
  g_WorldPlaying = false;
  g_Upgraded = 0;
  g_Tracks.clear();
  g_TrackBytes = 0;
  g_TracksFullLogged = false;
  g_ClothBytes = 0;
  g_ClothFullLogged = false;
  g_ClothLogs = 0;
  g_ClothAnchorsNow.clear();
  g_MaterialBytes = 0;
  g_TrackTime = -1.0;
  g_GamePlaces.clear();
  g_GamePlacesOther.clear();
  g_Sightings.clear();
  g_SightingFloats = 0;
  g_ViewSamples.clear();
  g_ViewOrder = 0;
  g_CrowdKinds.clear();
  g_CrowdKindKeys.clear();
  g_CrowdSamples.clear();
  g_CrowdBytes = 0;
  g_CrowdBadDraws = 0;
  g_CrowdKindLogs = 0;
  g_CrowdSampleLogs = 0;
  g_CrowdsFullLogged = false;
  g_CrowdPlaying = false;
}

int replay_gamedraws::Upgraded()
{
  return g_Upgraded.load();
}

void replay_gamedraws::SetDraws(replay_shapes::Draw const* draws, int count)
{
  std::lock_guard<std::mutex> lock(g_Mutex);
  if (draws && count > 0)
    g_Draws.assign(draws, draws + count);
  else
    g_Draws.clear();
  bool playing = !g_Draws.empty();
  if (playing && !g_Playing)
    g_LoggedPlaying = false;
  g_Playing = playing;
}

bool replay_gamedraws::InMainPass(ID3D11DeviceContext* pContext)
{
  return g_InMain && IsImmediate(pContext);
}

bool replay_gamedraws::Bypass()
{
  return t_Drawing;
}

// Each draw of the game's on its immediate context: this frame's camera,
// copied at the main G-buffer pass's first draw while the replay plays
void replay_gamedraws::OnDraw(ID3D11DeviceContext* pContext)
{
  // While the world's own draws play: each mesh the game draws in its main
  // pass noted (that mesh's kept draws aren't drawn then)
  if (g_InMain && g_WorldPlaying.load(std::memory_order_relaxed) && g_GameVbMeshesNow && IsImmediate(pContext))
  {
    ID3D11Buffer* vb = nullptr;
    UINT stride = 0, offset = 0;
    pContext->IAGetVertexBuffers(0, 1, &vb, &stride, &offset);
    if (vb)
    {
      auto it = g_GameVbMeshesNow->find((__int64)vb);
      if (it != g_GameVbMeshesNow->end() && it->second >= 0)
      {
        if ((size_t)it->second >= g_MeshDrawnAt.size())
          g_MeshDrawnAt.resize((size_t)it->second + 1, 0);
        g_MeshDrawnAt[it->second] = g_Frame;
      }
      vb->Release();
    }
  }
  // And in its decal, lit and half-size passes (see g_MeshDrawnOtherAt), and
  // its runner's vision layer (a live Dash's markers)
  else if (!g_InMain && (g_Pass == Pass_Decal || g_Pass == Pass_Lit || g_Pass == Pass_Half || g_Pass == Pass_Rv) &&
      g_WorldPlaying.load(std::memory_order_relaxed) && g_GameVbMeshesNow && IsImmediate(pContext))
  {
    ID3D11Buffer* vb = nullptr;
    UINT stride = 0, offset = 0;
    pContext->IAGetVertexBuffers(0, 1, &vb, &stride, &offset);
    if (vb)
    {
      auto it = g_GameVbMeshesNow->find((__int64)vb);
      if (it != g_GameVbMeshesNow->end() && it->second >= 0)
      {
        if ((size_t)it->second >= g_MeshDrawnOtherAt.size())
          g_MeshDrawnOtherAt.resize((size_t)it->second + 1, 0);
        g_MeshDrawnOtherAt[it->second] = g_Frame;
      }
      vb->Release();
    }
  }
  if (!g_InMain || g_ViewCopied || !(g_Playing || g_PeoplePlaying || g_WorldPlaying || g_CrowdPlaying) || !g_Enabled || !g_Immediate1 ||
      !IsImmediate(pContext))
    return;
  g_ViewCopied = true;
  ID3D11Buffer* view = nullptr;
  UINT first = 0, num = 0;
  g_Immediate1->VSGetConstantBuffers1(2, 1, &view, &first, &num);
  if (!view)
    return;
  D3D11_BUFFER_DESC d{};
  view->GetDesc(&d);
  if (first == 0 && d.ByteWidth <= 65536)
  {
    if (!g_View || g_ViewWidth != d.ByteWidth)
    {
      g_View.Reset();
      ComPtr<ID3D11Device> device;
      pContext->GetDevice(device.GetAddressOf());
      D3D11_BUFFER_DESC c{};
      c.ByteWidth = d.ByteWidth;
      c.Usage = D3D11_USAGE_DEFAULT;
      c.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
      if (device && SUCCEEDED(device->CreateBuffer(&c, nullptr, g_View.GetAddressOf())))
        g_ViewWidth = d.ByteWidth;
    }
    if (g_View)
      pContext->CopyResource(g_View.Get(), view);
  }
  view->Release();
}

bool replay_gamedraws::Capturing()
{
  return g_Capturing.load(std::memory_order_relaxed) && g_Enabled.load(std::memory_order_relaxed) && g_Immediate1 &&
    (g_Pass == Pass_Main || (g_Pass != Pass_None && g_PassOrdinal < MaxPassOrdinals));
}

int replay_gamedraws::PassNow()
{
  return g_Pass == Pass_Env ? Pass_Main : g_Pass;
}

int replay_gamedraws::GameDrawsModel(std::string const& mesh, float const* place, float within)
{
  float reach = within >= 0.f ? within : ModelDrawnMetres;
  std::lock_guard<std::mutex> lock(g_Mutex);
  auto known = g_ModelMeshIds.find(mesh);
  if (known == g_ModelMeshIds.end() || !g_FrameShared)
    return -1;
  int id = known->second;
  // Drawn in its main pass or in its decal, lit and half-size passes (see
  // g_MeshDrawnOtherAt): there where it was in either is drawn; drawn
  // elsewhere only is not; drawn with its places not read back yet: not known
  bool drawnAny = false, unknown = false;
  for (int other = 0; other < 2; ++other)
  {
    std::vector<unsigned> const& drawn = other ? g_MeshDrawnOtherShared : g_MeshDrawnShared;
    unsigned drawnAt = id < (int)drawn.size() ? drawn[id] : 0;
    if (!drawnAt || g_FrameShared - drawnAt > WorldGameDrewFrames)
      continue;
    drawnAny = true;
    auto const& placesOf = other ? g_GamePlacesOther : g_GamePlaces;
    auto places = placesOf.find(id);
    if (places == placesOf.end() || g_FrameShared - places->second.first > ProbeFreshFrames)
    {
      unknown = true;
      continue;
    }
    std::vector<float> const& p = places->second.second;
    for (size_t j = 0; j + 3 <= p.size(); j += 3)
    {
      float dx = place[0] - p[j], dy = place[1] - p[j + 1], dz = place[2] - p[j + 2];
      if (dx * dx + dy * dy + dz * dz <= reach * reach)
        return 1;
    }
  }
  if (!drawnAny)
    return 0;
  return unknown ? -1 : 0;
}

void replay_gamedraws::InViewTimes(float const* place, float maxMetres, std::vector<float>& times)
{
  // Inside its picture's edges by a tenth (a model at an edge may be drawn or not)
  const float Margin = 0.9f;
  times.clear();
  std::lock_guard<std::mutex> lock(g_Mutex);
  JudgeViewOrder();
  if (g_ViewOrder <= 0)
    return;
  for (ViewSample const& v : g_ViewSamples)
  {
    float dx = place[0] - v.Camera[0], dy = place[1] - v.Camera[1], dz = place[2] - v.Camera[2];
    if (dx * dx + dy * dy + dz * dz <= maxMetres * maxMetres && InView(v, place, g_ViewOrder, Margin))
      times.push_back(v.Clock);
  }
}

float replay_gamedraws::CameraDistanceAt(double clock, float const* place)
{
  std::lock_guard<std::mutex> lock(g_Mutex);
  ViewSample const* v = ViewNear((float)clock, 0.25f);
  if (!v)
    return -1.f;
  float dx = place[0] - v->Camera[0], dy = place[1] - v->Camera[1], dz = place[2] - v->Camera[2];
  return sqrtf(dx * dx + dy * dy + dz * dz);
}

void replay_gamedraws::SightingsOf(int shape, std::vector<ShapeSighting>& out)
{
  out.clear();
  std::lock_guard<std::mutex> lock(g_Mutex);
  for (Sighting const& s : g_Sightings)
    if (s.Shape == shape)
    {
      ShapeSighting one;
      one.Clock = s.Clock;
      one.Main = s.Main;
      one.Places = s.Places;
      out.push_back(std::move(one));
    }
}

void replay_gamedraws::SightingsAt(int shape, float const* place, float ownMetres, float nearMetres, Seen& out)
{
  out = Seen{};
  std::lock_guard<std::mutex> lock(g_Mutex);
  for (Sighting const& s : g_Sightings)
  {
    bool own = false, near_ = false;
    for (size_t j = 0; j + 3 <= s.Places.size(); j += 3)
    {
      float dx = s.Places[j] - place[0], dy = s.Places[j + 1] - place[1], dz = s.Places[j + 2] - place[2];
      float d2 = dx * dx + dy * dy + dz * dz;
      if (s.Shape == shape && d2 <= ownMetres * ownMetres)
        own = true;
      else if (d2 <= nearMetres * nearMetres)
        near_ = true;
    }
    if (own)
    {
      out.Own.push_back(s.Clock);
      out.OwnMain = out.OwnMain || s.Main;
    }
    else if (near_)
      out.Near.push_back(s.Clock);
  }
}

void replay_gamedraws::NoteFeedTextures(__int64 table)
{
  __int64 vtable = g_ViewVTable.load(std::memory_order_relaxed);
  if (!vtable || table < 0x10000)
    return;
  // Only while the replay records or plays (nothing of the game's held otherwise)
  if (!g_Capturing.load(std::memory_order_relaxed) && !g_WorldCapturing.load(std::memory_order_relaxed) &&
      !g_Playing.load(std::memory_order_relaxed) && !g_WorldPlaying.load(std::memory_order_relaxed) &&
      !g_PeoplePlaying.load(std::memory_order_relaxed))
  {
    AcquireSRWLockExclusive(&g_FeedLock);
    for (int ch = 0; ch < Feeds; ++ch)
      for (int k = 0; k < 2; ++k)
        g_FeedNow[ch][k].Reset();
    ReleaseSRWLockExclusive(&g_FeedLock);
    return;
  }
  __int64 views[Feeds][2] = {};
  if (!ReadFeedTable(table, vtable, views))
    return;
  int seen = 0;
  AcquireSRWLockExclusive(&g_FeedLock);
  for (int ch = 0; ch < Feeds; ++ch)
    for (int k = 0; k < 2; ++k)
    {
      __int64 view = views[ch][k];
      seen += view != 0;
      if ((__int64)g_FeedNow[ch][k].Get() == view)
        continue;
      // A reference held while it's the channel's (the game's own table has it now)
      g_FeedNow[ch][k] = (ID3D11ShaderResourceView*)view;
      auto& list = g_FeedSeen[ch][k];
      if (view && std::find(list.begin(), list.end(), view) == list.end())
      {
        list.push_back(view);
        if (list.size() > FeedSeenKept)
          list.erase(list.begin());
      }
    }
  ReleaseSRWLockExclusive(&g_FeedLock);
  if (!g_FeedsLogged.exchange(true))
    util::log::Write("Replay: the video channels' frames read from the billboard manager's table (%d of its %d textures are views)",
      seen, Feeds * 2);
}

void replay_gamedraws::FeedSizes(float const* camera, float* sizes)
{
  ULONGLONG now = GetTickCount64();
  AcquireSRWLockShared(&g_FeedLock);
  for (int ch = 0; ch < Feeds; ++ch)
  {
    sizes[ch] = 0.f;
    std::vector<float> const& places = g_FeedPlaces[ch];
    if (places.empty() || now - g_FeedPlacedAt[ch] > FeedWantedMs)
      continue;
    float best = 1e30f;
    for (size_t j = 0; j + 3 <= places.size(); j += 3)
    {
      float dx = places[j] - camera[0], dy = places[j + 1] - camera[1], dz = places[j + 2] - camera[2];
      best = (std::min)(best, dx * dx + dy * dy + dz * dz);
    }
    // As the game sizes a screen: its square metres / (distance squared x 1.4)
    float size = FeedScreenSquareMetres / ((std::max)(best, 1.f) * 1.4f);
    sizes[ch] = (std::min)((std::max)(size, FeedMinSize), FeedMaxSize);
  }
  ReleaseSRWLockShared(&g_FeedLock);
}

int replay_gamedraws::PastKeptPass()
{
  if (!g_Capturing.load(std::memory_order_relaxed) || !g_Enabled.load(std::memory_order_relaxed) || g_Pass == Pass_None ||
      g_Pass == Pass_Main || !g_SawMain || g_PassOrdinal < MaxPassOrdinals)
    return -1;
  return g_Pass * 1000 + g_PassCounts[g_Pass];
}

// The immediate context's state for a draw now, held in a recipe; null, or
// why it isn't one the tools can draw again
static char const* TakeState(Recipe& r, UINT indexCount, UINT instances, UINT startIndex, INT baseVertex)
{
  ID3D11DeviceContext1* c = g_Immediate1.Get();
  c->VSGetShader(r.Vs.GetAddressOf(), nullptr, nullptr);
  c->PSGetShader(r.Ps.GetAddressOf(), nullptr, nullptr);
  int vsAt = -1, psAt = -1;
  bool known = r.Vs && replay_drawlog::VectorOffsetsOf((__int64)r.Vs.Get(), vsAt);
  if (!known)
    return "its vertex shader wasn't noted";
  if (vsAt < 0)
    return "not instanced: no instance offsets in its b0";
  if (!r.Ps)
    return "no pixel shader";
  replay_drawlog::VectorOffsetsOf((__int64)r.Ps.Get(), psAt);
  r.VsOffsetsAt = vsAt;
  r.PsOffsetsAt = psAt;
  c->GSGetShader(r.Gs.GetAddressOf(), nullptr, nullptr);
  c->HSGetShader(r.Hs.GetAddressOf(), nullptr, nullptr);
  c->DSGetShader(r.Ds.GetAddressOf(), nullptr, nullptr);
  c->IAGetInputLayout(r.Layout.GetAddressOf());
  c->IAGetPrimitiveTopology(&r.Topology);
  {
    ID3D11Buffer* vbs[VbSlots] = {};
    c->IAGetVertexBuffers(0, VbSlots, vbs, r.Strides, r.Offsets);
    for (int i = 0; i < VbSlots; ++i)
      r.Vbs[i].Attach(vbs[i]);
  }
  c->IAGetIndexBuffer(r.Ib.GetAddressOf(), &r.IbFormat, &r.IbOffset);
  r.IndexCount = indexCount;
  r.StartIndex = startIndex;
  r.BaseVertex = baseVertex;
  r.Instances = instances;
  // Constants: where its windows are
  ID3D11Buffer* vsCb[3] = {}, * psCb[3] = {};
  UINT vsFirst[3] = {}, vsNum[3] = {}, psFirst[3] = {}, psNum[3] = {};
  c->VSGetConstantBuffers1(0, 3, vsCb, vsFirst, vsNum);
  c->PSGetConstantBuffers1(0, 3, psCb, psFirst, psNum);
  auto window = [](ID3D11Buffer* b, UINT first, UINT num) {
    return Window{ (__int64)b, first, num };
  };
  r.VsB0 = window(vsCb[0], vsFirst[0], vsNum[0]);
  r.VsB1 = window(vsCb[1], vsFirst[1], vsNum[1]);
  r.PsB0 = window(psCb[0], psFirst[0], psNum[0]);
  r.PsB1 = window(psCb[1], psFirst[1], psNum[1]);
  r.Pages[0] = vsCb[0];
  r.Pages[1] = vsCb[1];
  r.Pages[2] = psCb[0];
  r.Pages[3] = psCb[1];
  // The camera: copied at the pass's first kept draw (this view's then)
  if (!g_PendingCamera && vsCb[2])
  {
    Staged staged{};
    if (Stage(vsCb[2], staged))
    {
      g_PendingCamera = (__int64)vsCb[2];
      g_PendingCameraFirst = vsFirst[2];
      g_PendingCameraCopy = staged.Staging;
      g_PendingCameraWidth = staged.Width;
      g_PendingCameraCapacity = staged.Capacity;
    }
  }
  for (int i = 0; i < 3; ++i)
  {
    if (vsCb[i])
      vsCb[i]->Release();
    if (psCb[i])
      psCb[i]->Release();
  }
  // Views: the instance buffer's slots (t0's buffer) are the tools' then
  {
    ID3D11ShaderResourceView* views[PsViewSlots] = {};
    c->VSGetShaderResources(0, VsViewSlots, views);
    for (int i = 0; i < VsViewSlots; ++i)
      r.VsViews[i].Attach(views[i]);
    if (r.VsViews[0])
    {
      D3D11_SHADER_RESOURCE_VIEW_DESC d{};
      r.VsViews[0]->GetDesc(&d);
      ComPtr<ID3D11Resource> res;
      r.VsViews[0]->GetResource(res.GetAddressOf());
      ComPtr<ID3D11Buffer> buffer;
      if (d.ViewDimension == D3D11_SRV_DIMENSION_BUFFER && d.Format == DXGI_FORMAT_R32G32B32A32_FLOAT && res &&
          SUCCEEDED(res.As(&buffer)) && buffer)
      {
        r.InstancePage = (__int64)buffer.Get();
        r.InstanceFirst = d.Buffer.FirstElement;
        r.Pages[4] = buffer;
      }
    }
    memset(views, 0, sizeof(views));
    c->PSGetShaderResources(0, PsViewSlots, views);
    for (int i = 0; i < PsViewSlots; ++i)
      r.PsViews[i].Attach(views[i]);
    __int64 instancePage = r.InstancePage;
    auto isInstances = [instancePage](ID3D11ShaderResourceView* view) {
      if (!view || !instancePage)
        return false;
      ComPtr<ID3D11Resource> res;
      view->GetResource(res.GetAddressOf());
      return (__int64)res.Get() == instancePage;
    };
    for (int i = 0; i < VsViewSlots; ++i)
      r.VsInstanceSlot[i] = isInstances(r.VsViews[i].Get());
    bool psInstances = false;
    for (int i = 0; i < PsViewSlots; ++i)
      psInstances = (r.PsInstanceSlot[i] = isInstances(r.PsViews[i].Get())) || psInstances;
    // A pixel shader reading the instances with no offsets noted: the vertex shader's, when its b0 is the same window
    if (psInstances && r.PsOffsetsAt < 0 && r.PsB0.Page == r.VsB0.Page && r.PsB0.First == r.VsB0.First)
      r.PsOffsetsAt = r.VsOffsetsAt;
    // A video screen's slots: which channel's frame each is (see NoteFeedTextures)
    if (!g_ViewVTable.load(std::memory_order_relaxed))
      for (int i = 0; i < PsViewSlots; ++i)
        if (r.PsViews[i] && !r.PsInstanceSlot[i])
        {
          g_ViewVTable = *(__int64 const*)r.PsViews[i].Get();
          break;
        }
    int lms[2];
    int lmsCount = replay_drawlog::LmsSlots((__int64)r.Ps.Get(), lms, 2);
    for (int k = 0; k < lmsCount; ++k)
    {
      if (lms[k] < 0 || lms[k] >= PsViewSlots || !r.PsViews[lms[k]] || r.PsInstanceSlot[lms[k]])
        continue;
      r.Feed[lms[k]] = FeedOf(r.PsViews[lms[k]].Get());
      r.HasFeed = r.HasFeed || r.Feed[lms[k]];
      if (g_FeedLogs.fetch_add(1) < MaxFeedLogs)
      {
        if (r.Feed[lms[k]])
          util::log::Write("Replay: a kept draw ('%s', vertex buffer 0x%I64X) shows video channel %d's frames (t%d, its texture %d): "
            "drawn with that channel's frame of the moment", r.Name, (__int64)r.Vbs[0].Get(), (r.Feed[lms[k]] - 1) / 2, lms[k],
            (r.Feed[lms[k]] - 1) % 2 + 1);
        else
          util::log::Write("Replay: a kept draw ('%s', vertex buffer 0x%I64X) has a video slot (t%d) whose texture is none of the "
            "channels' frames seen lately: drawn with it as kept", r.Name, (__int64)r.Vbs[0].Get(), lms[k]);
      }
    }
  }
  if (!r.InstancePage)
  {
    for (auto& p : r.Pages)
      p.Reset();
    return "no instance buffer the tools know";
  }
  {
    ID3D11SamplerState* samplers[PsSamplerSlots] = {};
    c->VSGetSamplers(0, VsSamplerSlots, samplers);
    for (int i = 0; i < VsSamplerSlots; ++i)
      r.VsSamplers[i].Attach(samplers[i]);
    memset(samplers, 0, sizeof(samplers));
    c->PSGetSamplers(0, PsSamplerSlots, samplers);
    for (int i = 0; i < PsSamplerSlots; ++i)
      r.PsSamplers[i].Attach(samplers[i]);
  }
  c->OMGetDepthStencilState(r.Depth.GetAddressOf(), &r.StencilRef);
  c->OMGetBlendState(r.Blend.GetAddressOf(), r.BlendFactor, &r.SampleMask);
  c->RSGetState(r.Raster.GetAddressOf());
  return nullptr;
}

// A texture view's texels (a texture's every level and layer counted once:
// width x height x depth or layers; 0 for none or a buffer)
static unsigned long long Texels(ID3D11ShaderResourceView* view)
{
  if (!view)
    return 0;
  ComPtr<ID3D11Resource> resource;
  view->GetResource(resource.GetAddressOf());
  ComPtr<ID3D11Texture2D> t2;
  ComPtr<ID3D11Texture3D> t3;
  ComPtr<ID3D11Texture1D> t1;
  if (resource && SUCCEEDED(resource.As(&t2)) && t2)
  {
    D3D11_TEXTURE2D_DESC d{};
    t2->GetDesc(&d);
    return (unsigned long long)d.Width * d.Height * (std::max)(d.ArraySize, 1u);
  }
  if (resource && SUCCEEDED(resource.As(&t3)) && t3)
  {
    D3D11_TEXTURE3D_DESC d{};
    t3->GetDesc(&d);
    return (unsigned long long)d.Width * d.Height * (std::max)(d.Depth, 1u);
  }
  if (resource && SUCCEEDED(resource.As(&t1)) && t1)
  {
    D3D11_TEXTURE1D_DESC d{};
    t1->GetDesc(&d);
    return (unsigned long long)d.Width * (std::max)(d.ArraySize, 1u);
  }
  return 0;
}

// A kept draw's textures as the game streams better ones in (2026-10-27
// 22:04: kept draws held the 128 x 128 or 1 x 1 placeholder versions their
// textures had when they were kept, the game drawing them at 1024 x 1024 by
// then; a placeholder has no cut-out alpha): the game's draw of the same mesh
// part now, each texture slot taken when its texture has more texels than
// the kept one's (a placeholder's format may differ from the texture's).
// Render thread, the immediate context's state for the draw now; how many slots.
// A texture view's kind: its texture's format, dimension, layers and shape
// (any size of the same kind is the same texture streamed in or out)
static bool SameKind(ID3D11ShaderResourceView* a, ID3D11ShaderResourceView* b)
{
  if (!a || !b)
    return false;
  ComPtr<ID3D11Resource> ra, rb;
  a->GetResource(ra.GetAddressOf());
  b->GetResource(rb.GetAddressOf());
  ComPtr<ID3D11Texture2D> ta, tb;
  if (!ra || !rb || FAILED(ra.As(&ta)) || FAILED(rb.As(&tb)) || !ta || !tb)
    return false;
  D3D11_TEXTURE2D_DESC da{}, db{};
  ta->GetDesc(&da);
  tb->GetDesc(&db);
  return da.Format == db.Format && da.ArraySize == db.ArraySize &&
    (da.MiscFlags & D3D11_RESOURCE_MISC_TEXTURECUBE) == (db.MiscFlags & D3D11_RESOURCE_MISC_TEXTURECUBE) &&
    (unsigned long long)da.Width * db.Height == (unsigned long long)db.Width * da.Height;
}

// A kept one's textures from a candidate of the same placed objects (see
// VariantSameMetres): only with the same shaders (another shader has its own
// slots: 2026-10-27 22:55, the gate frame's slot 7 took a 2048 x 2048 array
// texture it was never drawn with); a bigger one of its kind, or anything
// for a placeholder (8 x 8 or less). How many slots.
// Whether two kept draws of a part look alike: the same shaders, and in each
// texture slot the same texture, or one of the same kind at another size (the
// same texture streamed in or out), or a placeholder (8 x 8 or less). The same
// kind at the same size but another texture is another look (a variation).
static bool LooksAlike(Recipe const& a, Recipe const& b)
{
  if (a.Vs.Get() != b.Vs.Get() || a.Ps.Get() != b.Ps.Get())
    return false;
  auto alike = [](ID3D11ShaderResourceView* x, ID3D11ShaderResourceView* y) {
    if (x == y)
      return true;
    if (!x || !y)
      return false;
    ComPtr<ID3D11Resource> rx, ry;
    x->GetResource(rx.GetAddressOf());
    y->GetResource(ry.GetAddressOf());
    if (rx && rx.Get() == ry.Get())
      return true;
    unsigned long long tx = Texels(x), ty = Texels(y);
    if (tx <= 64 || ty <= 64)
      return true;
    return SameKind(x, y) && tx != ty;
  };
  for (int i = 0; i < PsViewSlots; ++i)
    if (!a.PsInstanceSlot[i] && !b.PsInstanceSlot[i] && !a.Feed[i] && !b.Feed[i] && !alike(a.PsViews[i].Get(), b.PsViews[i].Get()))
      return false;
  for (int i = 0; i < VsViewSlots; ++i)
    if (!a.VsInstanceSlot[i] && !b.VsInstanceSlot[i] && !alike(a.VsViews[i].Get(), b.VsViews[i].Get()))
      return false;
  return true;
}

static int UpgradeFrom(Recipe& r, Recipe const& c)
{
  if (c.Vs.Get() != r.Vs.Get() || c.Ps.Get() != r.Ps.Get())
    return 0;
  int taken = 0;
  for (int i = 0; i < PsViewSlots; ++i)
  {
    // A video slot: drawn with its channel's frame of the moment (its channel learnt)
    if (r.Feed[i] || c.Feed[i])
    {
      if (!r.Feed[i])
      {
        r.Feed[i] = c.Feed[i];
        r.HasFeed = true;
      }
      continue;
    }
    unsigned long long now = Texels(r.PsViews[i].Get());
    ID3D11ShaderResourceView* view = c.PsViews[i].Get();
    if (view && !r.PsInstanceSlot[i] && !c.PsInstanceSlot[i] && Texels(view) > now && (now <= 64 || SameKind(view, r.PsViews[i].Get())))
    {
      r.PsViews[i] = view;
      ++taken;
    }
  }
  for (int i = 0; i < VsViewSlots; ++i)
  {
    unsigned long long now = Texels(r.VsViews[i].Get());
    ID3D11ShaderResourceView* view = c.VsViews[i].Get();
    if (view && !r.VsInstanceSlot[i] && !c.VsInstanceSlot[i] && Texels(view) > now && (now <= 64 || SameKind(view, r.VsViews[i].Get())))
    {
      r.VsViews[i] = view;
      ++taken;
    }
  }
  return taken;
}

void replay_gamedraws::CaptureDraw(ID3D11DeviceContext* pContext, int shape, int section, char const* name, UINT indexCount,
  UINT instances, UINT startIndex, INT baseVertex)
{
  if (!Capturing() || shape < 0 || section < 0 || !IsImmediate(pContext))
    return;
  CostTimer timer(Cost_Captures);
  // Another pass's: its own key (the pass and its place above the shape's);
  // a reflection's G-buffer pass's: the G-buffer pass's (see Pass_Env)
  bool env = g_Pass == Pass_Env;
  long long key = KeyOf(shape, section);
  if (g_Pass != Pass_Main && !env)
    key |= ((long long)(g_Pass * MaxPassOrdinals + g_PassOrdinal + 1)) << 48;
  std::shared_ptr<Recipe> kept;
  {
    std::lock_guard<std::mutex> lock(g_Mutex);
    auto it = g_Recipes.find(key);
    if (it != g_Recipes.end())
      kept = it->second;
    else if ((int)g_Recipes.size() >= MaxRecipes)
      return;
    // A reflection's draw only for a part not kept yet; the G-buffer pass's
    // own draw of one kept from a reflection kept in its place
    if (env && kept)
      return;
    if (!env && kept && kept->FromEnv && g_Pass == Pass_Main)
    {
      g_Recipes.erase(it);
      kept = nullptr;
    }
  }
  // Kept already: kept again every UpgradeEvery frames, as a candidate (see
  // VariantSameMetres)
  bool candidate = kept != nullptr;
  if (candidate && (!kept->Ready || g_Frame % UpgradeEvery != 0))
    return;
  if (g_PendingModels >= MaxCapturesPerPass)
    return;
  // A skinned draw (its vertex shader reads bones) isn't one to place with
  // the model's transform: the skinned ones are kept by CaptureSkin (a
  // skinned door's mesh can be drawn either way)
  ID3D11VertexShader* vs = nullptr;
  g_Immediate1->VSGetShader(&vs, nullptr, nullptr);
  bool bones = vs && replay_drawlog::ReadsBones((__int64)vs);
  if (vs)
    vs->Release();
  if (bones)
    return;
  auto r = std::make_shared<Recipe>();
  r->Candidate = candidate;
  r->Shape = shape;
  r->Section = section;
  r->Pass = env ? Pass_Main : g_Pass;
  r->PassOrdinal = g_Pass == Pass_Main || env ? 0 : g_PassOrdinal;
  r->FromEnv = env;
  r->Clock = g_Clock.load();
  strncpy_s(r->Name, name ? name : "?", _TRUNCATE);
  char const* why = TakeState(*r, indexCount, instances, startIndex, baseVertex);
  std::lock_guard<std::mutex> lock(g_Mutex);
  if (candidate)
  {
    // Not kept in the list: matched once it's read back
    if (!why)
    {
      g_Pending.push_back(r);
      ++g_PendingModels;
    }
    return;
  }
  g_Recipes[key] = r;
  if (why)
  {
    // Not one the tools can draw (its world matrix in its b0, or its shader not noted): never tried again
    g_Attempts[key] = MaxAttempts;
    if (g_FailLogs++ < MaxFailLogs)
      util::log::Write("Replay: '%s' section %d: the game's draw of it isn't one the tools can draw again (%s)", r->Name, section,
        why);
    return;
  }
  g_Pending.push_back(r);
  ++g_PendingModels;
}

void replay_gamedraws::SetClock(double recordTime)
{
  g_Clock.store(recordTime);
}

bool replay_gamedraws::CapturingSkins()
{
  // The lit pass's too (2026-11-01, the user: "glasses are transparent when
  // they shouldn't": Icarus's lenses are drawn see-through in it, not in the
  // G-buffer pass, and weren't kept): a person's draws there are drawn there
  bool pass = g_Pass == Pass_Main || (g_Pass == Pass_Lit && g_SawMain && g_PassOrdinal < MaxPassOrdinals);
  return Capturing() && pass && g_Frame % WantedCaptureEvery == 0 && g_PendingSkins < MaxSkinCapturesPerPass + MaxWantedCapturesPerPass;
}

// A draw of the game's in its G-buffer pass in a frame that people's are kept
// in: kept if its vertex shader reads bones (FaithReplay matches it to a
// recorded person once it's read back). One of a gun's, drone's or door's
// mesh (SetWantedSkins): in the frames between too, and past the others' limit.
void replay_gamedraws::CaptureSkin(ID3D11DeviceContext* pContext, UINT indexCount, UINT instances, UINT startIndex,
  INT baseVertex)
{
  if (!CapturingSkins() || !IsImmediate(pContext))
    return;
  ID3D11VertexShader* vs = nullptr;
  g_Immediate1->VSGetShader(&vs, nullptr, nullptr);
  bool bones = vs && replay_drawlog::ReadsBones((__int64)vs);
  if (vs)
    vs->Release();
  if (!bones)
    return;
  ID3D11Buffer* vb = nullptr;
  UINT vbStride = 0, vbOffset = 0;
  g_Immediate1->IAGetVertexBuffers(0, 1, &vb, &vbStride, &vbOffset);
  __int64 vbAt = (__int64)vb;
  if (vb)
    vb->Release();
  bool wanted = false;
  std::vector<std::shared_ptr<Recipe>> same;
  {
    std::lock_guard<std::mutex> lock(g_Mutex);
    // The kept ones of this mesh part: their textures as the game has them now
    if (g_Frame % SkinCaptureEvery == 0)
      for (auto const& entry : g_Skins)
        if ((__int64)entry.second->Vbs[0].Get() == vbAt && entry.second->StartIndex == startIndex &&
            entry.second->IndexCount == indexCount && entry.second->Stride)
          same.push_back(entry.second);
    if ((int)g_Skins.size() >= MaxSkins)
      return;
    wanted = vbAt && g_WantedVbs.count(vbAt);
  }
  // (No textures taken by mesh: people of one mesh wear their own variations)
  same.clear();
  // A gun's, drone's or door's: the G-buffer pass's only
  if (wanted && g_Pass != Pass_Main)
    return;
  if (wanted ? g_PendingWanted >= MaxWantedCapturesPerPass :
      g_Frame % SkinCaptureEvery != 0 || g_PendingSkins - g_PendingWanted >= MaxSkinCapturesPerPass)
    return;
  auto r = std::make_shared<Recipe>();
  r->Skin = true;
  r->Clock = g_Clock.load();
  r->Pass = g_Pass;
  r->PassOrdinal = g_Pass == Pass_Main ? 0 : g_PassOrdinal;
  strcpy_s(r->Name, "person");
  if (TakeState(*r, indexCount, instances, startIndex, baseVertex))
    return;
  g_Pending.push_back(r);
  ++g_PendingSkins;
  if (wanted)
    ++g_PendingWanted;
}

bool replay_gamedraws::CapturingWorld()
{
  return g_WorldCapturing.load(std::memory_order_relaxed) && g_WorldOn.load(std::memory_order_relaxed) &&
    g_Enabled.load(std::memory_order_relaxed) && g_Immediate1 && g_Frame % WorldCaptureEvery == 0 &&
    g_PendingWorld < MaxWorldCapturesPerPass && (g_Pass == Pass_Main || (g_Pass != Pass_None && g_Pass != Pass_Rv &&
    g_PassOrdinal < MaxPassOrdinals));
}

// A draw of the game's in its G-buffer pass that isn't a recorded model's
// (ReplayShapes: not one of their shapes' buffers) nor a person's, gun's or
// drone's (its vertex shader doesn't read bones, its buffer isn't a wanted
// one): kept once (by its vertex buffer, first index and count) with its
// instances near the camera (see WorldCaptureEvery)
void replay_gamedraws::CaptureWorld(ID3D11DeviceContext* pContext, UINT indexCount, UINT instances, UINT startIndex,
  INT baseVertex)
{
  if (!CapturingWorld() || !IsImmediate(pContext))
    return;
  ID3D11VertexShader* vs = nullptr;
  g_Immediate1->VSGetShader(&vs, nullptr, nullptr);
  bool bones = vs && replay_drawlog::ReadsBones((__int64)vs);
  if (vs)
    vs->Release();
  if (bones)
    return;
  ID3D11Buffer* vb = nullptr;
  UINT vbStride = 0, vbOffset = 0;
  g_Immediate1->IAGetVertexBuffers(0, 1, &vb, &vbStride, &vbOffset);
  __int64 vbAt = (__int64)vb;
  if (vb)
    vb->Release();
  if (!vbAt)
    return;
  // Another pass's: the pass and its place in the first index's top bits
  if (startIndex >> WorldPassShift)
    return;   // a first index the key can't hold with the pass (never seen)
  bool env = g_Pass == Pass_Env;
  UINT passBits = g_Pass == Pass_Main || env ? 0 : ((UINT)(g_Pass * MaxPassOrdinals + g_PassOrdinal + 1) << WorldPassShift);
  WorldKeyType key(vbAt, startIndex | passBits, indexCount);
  std::shared_ptr<Recipe> kept;
  {
    std::lock_guard<std::mutex> lock(g_Mutex);
    auto known = g_World.find(key);
    // A reflection's draw only for a mesh part not kept yet; the G-buffer
    // pass's own draw of one kept from a reflection kept in its place
    if (known != g_World.end() && env)
      return;
    if (known != g_World.end() && known->second->FromEnv && g_Pass == Pass_Main)
    {
      g_World.erase(known);
      known = g_World.end();
    }
    if (known != g_World.end())
    {
      kept = known->second;
      if (!kept->Ready)
        return;
    }
    else if ((int)g_World.size() >= MaxWorld || g_WantedVbs.count(vbAt))
      return;
    auto tried = g_WorldAttempts.find(key);
    if (!kept && tried != g_WorldAttempts.end() && tried->second >= MaxAttempts)
      return;
  }
  // Kept already: kept again, as a candidate (see VariantSameMetres)
  auto r = std::make_shared<Recipe>();
  r->Candidate = kept != nullptr;
  r->World = true;
  r->WorldKey = key;
  r->Pass = env ? Pass_Main : g_Pass;
  r->PassOrdinal = g_Pass == Pass_Main || env ? 0 : g_PassOrdinal;
  r->FromEnv = env;
  r->Clock = g_Clock.load();
  strcpy_s(r->Name, "world");
  char const* why = TakeState(*r, indexCount, instances, startIndex, baseVertex);
  std::lock_guard<std::mutex> lock(g_Mutex);
  if (why)
  {
    if (!r->Candidate)
    {
      g_WorldAttempts[key] = MaxAttempts;   // not one the tools can draw again
      NoteWorldNotKept(vbAt, why, instances, r->Stride);
      // Where a "not instanced" draw's instances are (the first few)
      bool full = indexCount > 6;   // the 6-index ones are far quads; the full bodies too (2026-10-28)
      if ((full ? g_FullDrawInputLogs < 6 : g_DrawInputLogs < 4) && strstr(why, "no instance offsets in its b0") && g_VbNames)
      {
        auto named = g_VbNames->find(vbAt);
        if (named != g_VbNames->end())
        {
          ++(full ? g_FullDrawInputLogs : g_DrawInputLogs);
          LogDrawInputs(named->second.c_str(), indexCount, instances);
          if (g_CrowdProbesTaken < 4 && (full || g_CrowdProbesTaken < 1))
          {
            ++g_CrowdProbesTaken;
            TakeCrowdProbe(named->second.c_str(), indexCount, instances);
          }
        }
      }
    }
    return;
  }
  if (!r->Candidate)
    g_World[key] = r;
  g_Pending.push_back(r);
  ++g_PendingWorld;
}

bool replay_gamedraws::Probing()
{
  // The decal, lit and half-size passes too (see g_MeshDrawnOtherAt), and the runner's vision layer
  bool pass = g_InMain || g_Pass == Pass_Decal || g_Pass == Pass_Lit || g_Pass == Pass_Half || g_Pass == Pass_Rv;
  return g_WorldPlaying.load(std::memory_order_relaxed) && g_WorldOn.load(std::memory_order_relaxed) && pass && g_Immediate1 &&
    g_GameVbMeshesNow && g_PendingProbes < MaxProbesPerPassAll;
}

void replay_gamedraws::ProbeDraw(ID3D11DeviceContext* pContext, UINT indexCount, UINT instances, UINT startIndex, INT baseVertex)
{
  if (!Probing() || !IsImmediate(pContext))
    return;
  ID3D11Buffer* vb = nullptr;
  UINT vbStride = 0, vbOffset = 0;
  g_Immediate1->IAGetVertexBuffers(0, 1, &vb, &vbStride, &vbOffset);
  __int64 vbAt = (__int64)vb;
  if (vb)
    vb->Release();
  auto it = vbAt ? g_GameVbMeshesNow->find(vbAt) : g_GameVbMeshesNow->end();
  if (it == g_GameVbMeshesNow->end() || it->second < 0)
    return;
  int id = it->second;
  bool other = !g_InMain;
  std::vector<unsigned>& probedAt = other ? g_ProbedOtherAt : g_ProbedAt;
  if ((size_t)id >= probedAt.size())
    probedAt.resize((size_t)id + 1, 0);
  // Another of its draws in the frame it's probed in: read back too (see
  // MaxProbesPerPassAll); a mesh not probed yet this frame only while there's
  // room for MaxProbesPerPass
  bool more = probedAt[id] == g_Frame;
  if (!more && ((probedAt[id] && g_Frame - probedAt[id] < ProbeEvery) || g_PendingProbes >= MaxProbesPerPass))
    return;
  probedAt[id] = g_Frame;
  auto r = std::make_shared<Recipe>();
  r->Probe = true;
  r->ProbeOther = other;
  r->SampleOnly = true;
  r->MeshId = id;
  strcpy_s(r->Name, "probe");
  if (TakeState(*r, indexCount, instances, startIndex, baseVertex))
  {
    CutProbe(id, other, g_Frame);
    return;
  }
  g_Pending.push_back(r);
  // No room left this pass: a later draw of a mesh probed this frame would be
  // missed, so the frame's places are all dropped
  if (++g_PendingProbes >= MaxProbesPerPassAll)
    (other ? g_ProbeFullOtherAt : g_ProbeFullAt) = g_Frame;
}

void replay_gamedraws::SetWorldCapture(bool on)
{
  g_WorldCapturing = on;
}

bool replay_gamedraws::CapturingTracks()
{
  return g_TrackCapturing.load(std::memory_order_relaxed) && g_TracksOn.load(std::memory_order_relaxed) &&
    g_Enabled.load(std::memory_order_relaxed) && g_InMain && g_Immediate1 && g_TrackPass &&
    (g_PendingTracks - g_PendingCloth < MaxTrackCapturesPerPass || g_PendingCloth < MaxClothCapturesPerPass);
}

// A cloth's draw (see MaxClothBytes): the first vertex buffer slot whose
// buffer the graphics card writes (bindable for unordered access), -1 if none
static int ClothSlotOf(ID3D11DeviceContext* c)
{
  ID3D11Buffer* vbs[VbSlots] = {};
  UINT strides[VbSlots] = {}, offsets[VbSlots] = {};
  c->IAGetVertexBuffers(0, VbSlots, vbs, strides, offsets);
  int slot = -1;
  for (int i = 0; i < VbSlots; ++i)
  {
    if (!vbs[i])
      continue;
    D3D11_BUFFER_DESC d{};
    vbs[i]->GetDesc(&d);
    if (slot < 0 && (d.BindFlags & D3D11_BIND_UNORDERED_ACCESS) && d.ByteWidth <= MaxClothBytes / 64)
      slot = i;
    vbs[i]->Release();
  }
  return slot;
}

// A draw of the game's in its G-buffer pass whose vertex shader reads bones,
// in a pass that samples the tracks: a sample of its track (see
// TrackSampleSeconds). Or a cloth's (see MaxClothBytes): keyed by its index
// buffer (its vertex buffer may be another each frame), its vertices copied
void replay_gamedraws::CaptureTrack(ID3D11DeviceContext* pContext, UINT indexCount, UINT instances, UINT startIndex,
  INT baseVertex)
{
  if (!CapturingTracks() || !IsImmediate(pContext))
    return;
  ID3D11VertexShader* vs = nullptr;
  g_Immediate1->VSGetShader(&vs, nullptr, nullptr);
  bool bones = vs && replay_drawlog::ReadsBones((__int64)vs);
  if (vs)
    vs->Release();
  int clothSlot = bones ? -1 : ClothSlotOf(g_Immediate1.Get());
  if (!bones && clothSlot < 0)
    return;
  // Each kind within its own number a pass (the recorded people's skinned
  // draws alone can fill the others')
  if (clothSlot >= 0 ? g_PendingCloth >= MaxClothCapturesPerPass : g_PendingTracks - g_PendingCloth >= MaxTrackCapturesPerPass)
    return;
  ID3D11Buffer* vb = nullptr;
  UINT vbStride = 0, vbOffset = 0;
  g_Immediate1->IAGetVertexBuffers(0, 1, &vb, &vbStride, &vbOffset);
  __int64 vbAt = (__int64)vb;
  if (vb)
    vb->Release();
  if (clothSlot >= 0)
  {
    ID3D11Buffer* ib = nullptr;
    DXGI_FORMAT ibFormat = DXGI_FORMAT_UNKNOWN;
    UINT ibOffset = 0;
    g_Immediate1->IAGetIndexBuffer(&ib, &ibFormat, &ibOffset);
    vbAt = (__int64)ib;
    if (ib)
      ib->Release();
  }
  if (!vbAt)
    return;
  WorldKeyType key(vbAt, startIndex, indexCount);
  bool started = false;
  {
    std::lock_guard<std::mutex> lock(g_Mutex);
    auto it = g_Tracks.find(key);
    if (it == g_Tracks.end())
    {
      if ((int)g_Tracks.size() >= MaxTracks || g_TrackBytes >= MaxTrackBytes)
        return;
      g_Tracks[key];
    }
    else
    {
      if (it->second.Faith)
        return;
      started = it->second.R != nullptr;
    }
  }
  auto r = std::make_shared<Recipe>();
  r->Track = true;
  r->SampleOnly = started;
  r->WorldKey = key;
  r->Clock = g_Clock.load();
  strcpy_s(r->Name, clothSlot >= 0 ? "cloth" : "track");
  if (TakeState(*r, indexCount, instances, startIndex, baseVertex))
    return;
  if (clothSlot >= 0)
  {
    if (!r->Vbs[clothSlot])
      return;
    r->Cloth = true;
    r->ClothSlot = clothSlot;
    r->ClothSource = (__int64)r->Vbs[clothSlot].Get();
    r->Pages[5] = r->Vbs[clothSlot];
  }
  g_Pending.push_back(r);
  ++g_PendingTracks;
  g_PendingCloth += r->Cloth;
}

void replay_gamedraws::SetTrackCapture(bool on)
{
  g_TrackCapturing = on;
}

void replay_gamedraws::SetTracksOn(bool on)
{
  g_TracksOn = on;
}

void replay_gamedraws::SetPlayTime(double time)
{
  std::lock_guard<std::mutex> lock(g_Mutex);
  if (time >= 0.0 && g_TrackTime < 0.0)
  {
    g_LoggedTracks = false;
    g_LoggedCrowds = false;
  }
  g_TrackTime = time;
  g_CrowdPlaying = time >= 0.0 && !g_CrowdSamples.empty();
}

bool replay_gamedraws::CapturingCrowds()
{
  return g_TrackCapturing.load(std::memory_order_relaxed) && g_TracksOn.load(std::memory_order_relaxed) &&
    g_Enabled.load(std::memory_order_relaxed) && g_InMain && g_Immediate1 && g_TrackPass && g_PendingCrowds < MaxCrowdCapturesPerPass;
}

// A crowd draw of the game's in its G-buffer pass, in a pass that samples the
// tracks: its members copied (see the crowds in the namespace above)
void replay_gamedraws::CaptureCrowd(ID3D11DeviceContext* pContext, UINT indexCount, UINT instances, UINT startIndex,
  INT baseVertex)
{
  if (!CapturingCrowds() || !IsImmediate(pContext) || !instances || instances > MaxCrowdMembers)
    return;
  ID3D11DeviceContext1* c = g_Immediate1.Get();
  ComPtr<ID3D11VertexShader> vs;
  c->VSGetShader(vs.GetAddressOf(), nullptr, nullptr);
  if (!vs)
    return;
  CrowdShader shader = CrowdShaderOf(vs.Get());
  if (!shader.Crowd)
    return;
  ID3D11ShaderResourceView* views[CrowdVsViewSlots] = {};
  c->VSGetShaderResources(0, CrowdVsViewSlots, views);
  struct Release
  {
    ID3D11ShaderResourceView** Views;
    ~Release()
    {
      for (int i = 0; i < CrowdVsViewSlots; ++i)
        if (Views[i])
          Views[i]->Release();
    }
  } release{ views };
  // Its crowd buffers, each a buffer's view
  ComPtr<ID3D11Buffer> buffers[CrowdBuffers];
  D3D11_SHADER_RESOURCE_VIEW_DESC descs[CrowdBuffers] = {};
  __int64 crowdResources[CrowdBuffers] = {};
  for (int b = 0; b < CrowdBuffers; ++b)
  {
    int slot = shader.Slot[b];
    if (slot < 0)
      continue;
    if (!views[slot])
    {
      if (b != Crowd_Animation)
        return;
      continue;
    }
    views[slot]->GetDesc(&descs[b]);
    ComPtr<ID3D11Resource> res;
    views[slot]->GetResource(res.GetAddressOf());
    if (descs[b].ViewDimension != D3D11_SRV_DIMENSION_BUFFER || !res || FAILED(res.As(&buffers[b])) || !buffers[b])
      return;
    D3D11_BUFFER_DESC bd{};
    buffers[b]->GetDesc(&bd);
    if (bd.ByteWidth > MaxCrowdBufferBytes)
      return;
    crowdResources[b] = (__int64)res.Get();
  }
  UINT indexBytes = CrowdElementBytes(descs[Crowd_Indirection].Format);
  if ((indexBytes != 2 && indexBytes != 4) || descs[Crowd_Transform].Format != DXGI_FORMAT_R32G32B32A32_FLOAT ||
      (buffers[Crowd_Animation] && CrowdElementBytes(descs[Crowd_Animation].Format) != 4))
    return;
  // Its kind: its mesh part and its look (its pixel shader and that shader's views but the crowd buffers)
  ID3D11Buffer* vb = nullptr;
  UINT vbStride = 0, vbOffset = 0;
  c->IAGetVertexBuffers(0, 1, &vb, &vbStride, &vbOffset);
  __int64 vbAt = (__int64)vb;
  if (vb)
    vb->Release();
  if (!vbAt)
    return;
  unsigned long long look = 1469598103934665603ull;
  {
    ComPtr<ID3D11PixelShader> ps;
    c->PSGetShader(ps.GetAddressOf(), nullptr, nullptr);
    look = (look ^ (unsigned long long)(__int64)ps.Get()) * 1099511628211ull;
    ID3D11ShaderResourceView* psViews[PsViewSlots] = {};
    c->PSGetShaderResources(0, PsViewSlots, psViews);
    for (int i = 0; i < PsViewSlots; ++i)
    {
      if (!psViews[i])
        continue;
      // Only its material's textures (block-compressed or sRGB): a frame's
      // own resources bound there would make every frame another kind
      D3D11_SHADER_RESOURCE_VIEW_DESC vd{};
      psViews[i]->GetDesc(&vd);
      DXGI_FORMAT f = vd.Format;
      bool material = (vd.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2D || vd.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2DARRAY) &&
        ((f >= DXGI_FORMAT_BC1_TYPELESS && f <= DXGI_FORMAT_BC5_SNORM) || (f >= DXGI_FORMAT_BC6H_TYPELESS && f <= DXGI_FORMAT_BC7_UNORM_SRGB) ||
          f == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB || f == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB);
      if (material)
        look = (look ^ ((unsigned long long)(__int64)psViews[i] + (unsigned long long)i)) * 1099511628211ull;
      psViews[i]->Release();
    }
  }
  CrowdKeyType key(vbAt, startIndex, indexCount, look);
  int kind = -1;
  {
    std::lock_guard<std::mutex> lock(g_Mutex);
    auto it = g_CrowdKindKeys.find(key);
    if (it != g_CrowdKindKeys.end())
      kind = it->second;
    else if ((int)g_CrowdKinds.size() >= MaxCrowdKinds)
      return;
    if (g_CrowdBytes >= MaxCrowdBytes)
      return;
  }
  bool state = false;
  if (kind < 0)
  {
    auto k = std::make_shared<CrowdKind>();
    k->R = std::make_shared<Recipe>();
    k->R->Clock = g_Clock.load();
    strcpy_s(k->R->Name, "crowd");
    char const* why = TakeCrowdState(*k->R, *k, shader, views, indexCount, instances, startIndex, baseVertex);
    if (why)
      return;
    for (int b = 0; b < CrowdBuffers; ++b)
    {
      k->Slot[b] = buffers[b] ? shader.Slot[b] : -1;
      k->Format[b] = descs[b].Format;
    }
    if (g_VbNames)
    {
      auto named = g_VbNames->find(vbAt);
      if (named != g_VbNames->end())
        k->Mesh = named->second;
    }
    if (k->Mesh.empty())
      k->Mesh = "(a crowd mesh)";
    {
      std::lock_guard<std::mutex> lock(g_Mutex);
      kind = (int)g_CrowdKinds.size();
      g_CrowdKinds.push_back(k);
      g_CrowdKindKeys[key] = kind;
    }
    state = true;
    if (g_CrowdKindLogs < MaxCrowdKindLogs)
    {
      ++g_CrowdKindLogs;
      // The swarm's poses (people's): the texture's size
      char poses[160] = "";
      if (k->R->VsViews[1])
      {
        ComPtr<ID3D11Resource> res;
        k->R->VsViews[1]->GetResource(res.GetAddressOf());
        ComPtr<ID3D11Texture2D> tex;
        if (res && SUCCEEDED(res.As(&tex)) && tex)
        {
          D3D11_TEXTURE2D_DESC td{};
          tex->GetDesc(&td);
          sprintf_s(poses, "; t1 %u x %u x %u layers, format %d, usage %d", td.Width, td.Height, td.ArraySize, (int)td.Format,
            (int)td.Usage);
        }
      }
      util::log::Write("Replay: crowd draw kept: '%s' (%u indices, %u members now; its vertex shader's indirectionBuffer t%d, "
        "transformBuffer t%d, animationBuffer %s%d; the pixel shader reads %s of them%s)", k->Mesh.c_str(), indexCount, instances,
        shader.Slot[Crowd_Indirection], shader.Slot[Crowd_Transform], k->Slot[Crowd_Animation] >= 0 ? "t" : "none ",
        k->Slot[Crowd_Animation], (k->PsSlot[0] >= 0 || k->PsSlot[1] >= 0 || k->PsSlot[2] >= 0) ? "some" : "none", poses);
    }
  }
  // The copies: its b1 window (its first entry), the buffers (each once a frame)
  CrowdCopy copy;
  copy.Frame = g_Frame;
  copy.Time = g_Clock.load();
  copy.Kind = kind;
  copy.Instances = instances;
  copy.State = state;
  {
    ID3D11Buffer* cb[2] = {};
    UINT first[2] = {}, num[2] = {};
    c->VSGetConstantBuffers1(0, 2, cb, first, num);
    if (cb[1])
    {
      D3D11_BUFFER_DESC d{};
      cb[1]->GetDesc(&d);
      UINT size = num[1] ? num[1] * 16 : d.ByteWidth;
      if (size > MaxWindowBytes)
        size = MaxWindowBytes;
      if (size >= 16 && (size_t)first[1] * 16 + size <= d.ByteWidth && d.ByteWidth <= MaxCrowdBufferBytes * 4)
      {
        if (g_CrowdFrameCopiesAt != g_Frame)
        {
          g_CrowdFrameCopies.clear();
          g_CrowdFrameCopiesAt = g_Frame;
        }
        ComPtr<ID3D11Buffer>& staged = g_CrowdFrameCopies[(__int64)cb[1]];
        if (!staged)
          staged = StagingCopy(cb[1], 0, d.ByteWidth);
        copy.B1 = staged;
        copy.B1Bytes = size;
        copy.B1Offset = first[1] * 16;
        copy.B1Page = d.ByteWidth;
      }
    }
    if (state)
      copy.VsB0 = WindowCopy(cb[0], first[0], num[0], copy.VsB0Bytes);
    for (auto b : cb)
      if (b)
        b->Release();
    if (state)
    {
      ID3D11Buffer* pcb[2] = {};
      UINT pfirst[2] = {}, pnum[2] = {};
      c->PSGetConstantBuffers1(0, 2, pcb, pfirst, pnum);
      copy.PsB0 = WindowCopy(pcb[0], pfirst[0], pnum[0], copy.PsB0Bytes);
      copy.PsB1 = WindowCopy(pcb[1], pfirst[1], pnum[1], copy.PsB1Bytes);
      for (auto b : pcb)
        if (b)
          b->Release();
    }
  }
  if (!copy.B1)
    return;
  if (g_CrowdFrameCopiesAt != g_Frame)
  {
    g_CrowdFrameCopies.clear();
    g_CrowdFrameCopiesAt = g_Frame;
  }
  for (int b = 0; b < CrowdBuffers; ++b)
  {
    if (!buffers[b])
      continue;
    D3D11_BUFFER_DESC bd{};
    buffers[b]->GetDesc(&bd);
    ComPtr<ID3D11Buffer>& staged = g_CrowdFrameCopies[(__int64)buffers[b].Get()];
    if (!staged)
      staged = StagingCopy(buffers[b].Get(), 0, bd.ByteWidth);
    if (!staged)
      return;
    copy.Buffer[b] = staged;
    copy.Bytes[b] = bd.ByteWidth;
    copy.First[b] = descs[b].Buffer.FirstElement;
    copy.Count[b] = descs[b].Buffer.NumElements;
  }
  g_CrowdCopies.push_back(std::move(copy));
  ++g_PendingCrowds;
}

// Any thread, from the draw hooks while it plays: the game's own crowd draws
// left out (its swarms now aren't the recorded ones; the recorded crowds are
// drawn by the replay)
bool replay_gamedraws::SkipGameDraw(ID3D11DeviceContext* pContext)
{
  if (!g_CrowdPlaying.load(std::memory_order_relaxed) || !g_TracksOn.load(std::memory_order_relaxed) ||
      !g_Enabled.load(std::memory_order_relaxed) || t_Drawing || !pContext)
    return false;
  ID3D11VertexShader* vs = nullptr;
  pContext->VSGetShader(&vs, nullptr, nullptr);
  if (!vs)
    return false;
  __int64 key = (__int64)vs;
  vs->Release();
  AcquireSRWLockShared(&g_CrowdVsLock);
  auto it = g_CrowdVs.find(key);
  bool known = it != g_CrowdVs.end(), crowd = known && it->second;
  ReleaseSRWLockShared(&g_CrowdVsLock);
  if (known)
    return crowd;
  char names[600] = "";
  if (!replay_drawlog::ShaderNames(key, names, sizeof(names)))
    return false;   // not noted (yet): drawn
  crowd = ParseCrowdShader(names).Crowd;
  AcquireSRWLockExclusive(&g_CrowdVsLock);
  if (g_CrowdVs.size() < 65536)
    g_CrowdVs[key] = crowd;
  ReleaseSRWLockExclusive(&g_CrowdVsLock);
  return crowd;
}

void replay_gamedraws::CrowdCounts(int& kinds, int& samples, int& bad, unsigned long long& bytes)
{
  std::lock_guard<std::mutex> lock(g_Mutex);
  kinds = 0;
  for (auto const& k : g_CrowdKinds)
    kinds += k->StateRead && !k->Samples.empty();
  samples = (int)g_CrowdSamples.size();
  bad = g_CrowdBadDraws;
  bytes = g_CrowdBytes;
}

int replay_gamedraws::ThinTracks(std::function<int(double clock, float const* place)> const& theirs, int& tracks,
  unsigned long long& bytes)
{
  std::lock_guard<std::mutex> lock(g_Mutex);
  int dropped = 0;
  // The cloths' (see MaxClothBytes): whose each is, by where its vertices'
  // middle was at its samples (as they are, and through its instance's
  // transform when it has one: whether they're in the world's space or their
  // own wasn't known); kept when a recorded person's at most of them
  int clothKept = 0, clothFaith = 0, clothNobody = 0;
  std::string clothList;
  for (auto it = g_Tracks.begin(); it != g_Tracks.end();)
  {
    Track& t = it->second;
    if (!t.Cloth || !t.R || t.Faith)
    {
      ++it;
      continue;
    }
    int counts[3] = {}, placedHits = 0, rawHits = 0;
    for (size_t k = 0; k < t.Times.size() && k < t.ClothStarts.size(); ++k)
    {
      float middle[3], placed[3];
      if (!ClothMiddle(t, k, false, middle))
        break;
      int who = theirs(t.Times[k], middle);
      rawHits += who == 1;
      if (!who && ClothMiddle(t, k, true, placed))
      {
        who = theirs(t.Times[k], placed);
        placedHits += who == 1;
      }
      ++counts[who >= 0 && who <= 2 ? who : 0];
    }
    t.ClothLocal = placedHits > rawHits;
    int samples = counts[0] + counts[1] + counts[2];
    // (2026-11-03: Faith's hair, 19 of 33 a KrugerSec's while they fought and
    // 14 hers, was carried by him: one a quarter of whose samples are Faith's
    // isn't taken for another's)
    bool person = samples && counts[1] * 2 > samples && counts[2] * 4 < samples;
    char item[240];
    size_t slash = t.Mesh.find_last_of('/');
    sprintf_s(item, "%s, %u indices, %d samples %.1f - %.1f s (%d a person's, %d Faith's, %d nobody's)", t.Mesh.empty() ?
      "(its mesh not known)" : slash == std::string::npos ? t.Mesh.c_str() : t.Mesh.c_str() + slash + 1, t.R->IndexCount,
      (int)t.Times.size(), t.Times.empty() ? 0.0 : t.Times.front(), t.Times.empty() ? 0.0 : t.Times.back(), counts[1], counts[2],
      counts[0]);
    if (clothList.size() < 900)
      clothList += (clothList.empty() ? "" : "; ") + std::string(person ? "kept: " : "let go: ") + item;
    if (person)
    {
      ++clothKept;
      ++it;
      continue;
    }
    if (counts[2] && counts[2] * 4 >= samples)
      ++clothFaith;
    else
      ++clothNobody;
    g_TrackBytes -=(std::min)(g_TrackBytes, t.Data.size() * sizeof(float));
    g_ClothBytes -= (std::min)(g_ClothBytes, t.ClothData.size());
    it = g_Tracks.erase(it);
  }
  if (clothKept || clothFaith || clothNobody)
    util::log::Write("Replay: cloths: %d kept (a recorded person's), %d of Faith's and %d of nobody recorded let go (the game draws "
      "those); %.1f MB of their vertices: %s", clothKept, clothFaith, clothNobody, g_ClothBytes / 1048576.0, clothList.c_str());
  // By mesh (2026-10-28: the crowds were missing in the playback, 86 tracks
  // kept and 2 drawn at its start; which were usable wasn't in the log)
  std::map<std::string, int> unusable, kept;
  for (auto const& entry : g_Tracks)
  {
    Track const& t = entry.second;
    size_t slash = t.Mesh.find_last_of('/');
    std::string name = t.Mesh.empty() ? "(not in the mesh table)" : slash == std::string::npos ? t.Mesh : t.Mesh.substr(slash + 1);
    if (t.Faith)
      continue;
    if (t.Cloth && t.R)
      ++kept[name + " (cloth)"];
    else if (!t.R || t.Header < 0)
      ++unusable[name + (!t.R ? " (no first sample read back)" : " (its instances' rows aren't bones the replay reads)")];
    else
      ++kept[name];
  }
  for (int list = 0; list < 2; ++list)
  {
    std::string line;
    for (auto const& m : list ? unusable : kept)
    {
      std::string item = m.first + (m.second > 1 ? " x" + std::to_string(m.second) : "");
      if (!line.empty() && line.size() + item.size() > 900)
      {
        util::log::Write("Replay:   tracks %s: %s", list ? "not usable" : "kept", line.c_str());
        line.clear();
      }
      line += (line.empty() ? "" : ", ") + item;
    }
    if (!line.empty())
      util::log::Write("Replay:   tracks %s: %s", list ? "not usable" : "kept", line.c_str());
  }
  for (auto it = g_Tracks.begin(); it != g_Tracks.end();)
  {
    Track& t = it->second;
    if (!t.R || (t.Header < 0 && !t.Cloth) || t.Faith)
    {
      g_TrackBytes -= (std::min)(g_TrackBytes, t.Data.size() * sizeof(float));
      g_ClothBytes -= (std::min)(g_ClothBytes, t.ClothData.size());
      it = g_Tracks.erase(it);
      continue;
    }
    // A cloth kept above: all of its samples
    if (t.Cloth)
    {
      ++it;
      continue;
    }
    size_t floats = (size_t)t.R->Stride * 4;
    std::vector<double> times;
    std::vector<UINT> counts;
    std::vector<size_t> starts;
    std::vector<float> data;
    for (size_t k = 0; k < t.Times.size(); ++k)
    {
      UINT kept = 0;
      size_t start = data.size();
      for (UINT i = 0; i < t.Counts[k]; ++i)
      {
        float const* rows = &t.Data[t.Starts[k] + i * floats];
        float anchor[3];
        AnchorOf(rows, t.Header, anchor);
        if (theirs(t.Times[k], anchor))
        {
          ++dropped;
          continue;
        }
        data.insert(data.end(), rows, rows + floats);
        ++kept;
      }
      if (kept)
      {
        times.push_back(t.Times[k]);
        counts.push_back(kept);
        starts.push_back(start);
      }
    }
    g_TrackBytes -= (std::min)(g_TrackBytes, t.Data.size() * sizeof(float));
    if (times.empty())
    {
      it = g_Tracks.erase(it);
      continue;
    }
    t.Times.swap(times);
    t.Counts.swap(counts);
    t.Starts.swap(starts);
    t.Data.swap(data);
    t.Data.shrink_to_fit();
    g_TrackBytes += t.Data.size() * sizeof(float);
    ++it;
  }
  tracks = (int)g_Tracks.size();
  bytes = g_TrackBytes;
  return dropped;
}

// The cloths carried by the people they're on (2026-11-02, the user: "when
// faith isn't around icarus anymore the hoodie is gone": the game draws a
// cloth only while it's in sight, so none of its samples covers the times
// it wasn't). Each kept cloth's owner is the recorded person drawn near its
// vertices' middle at its first sample that has one, with that person's bone
// nearest that middle; that bone's pose is kept for each of its samples.
// While it plays, at a time with no sample near (see TrackHoldSeconds), the
// cloth is its nearest sample moved as that bone moved since (CarriedCloth):
// it goes along with the person, without the cloth's own sway.
int replay_gamedraws::AnchorCloths(std::function<bool(double clock, float const* place, __int64& owner, int& bone)> const& nearest,
  std::function<bool(__int64 owner, int bone, double clock, float* pose)> const& boneAt)
{
  std::lock_guard<std::mutex> lock(g_Mutex);
  int anchored = 0, cloths = 0;
  std::string list;
  for (auto& entry : g_Tracks)
  {
    Track& t = entry.second;
    if (!t.Cloth || !t.R || t.Faith)
      continue;
    ++cloths;
    size_t samples = (std::min)(t.Times.size(), t.ClothStarts.size());
    t.Owner = 0;
    t.OwnerBone = -1;
    for (size_t k = 0; k < samples && !t.Owner; ++k)
    {
      float middle[3];
      __int64 owner = 0;
      int bone = -1;
      if (ClothMiddle(t, k, t.ClothLocal, middle) && nearest(t.Times[k], middle, owner, bone) && owner && bone >= 0)
      {
        t.Owner = owner;
        t.OwnerBone = bone;
      }
    }
    if (!t.Owner)
      continue;
    t.AnchorPoses.assign(samples * 12, NAN);
    int known = 0;
    for (size_t k = 0; k < samples; ++k)
    {
      if (boneAt(t.Owner, t.OwnerBone, t.Times[k], &t.AnchorPoses[k * 12]))
        ++known;
      else
        std::fill(t.AnchorPoses.begin() + k * 12, t.AnchorPoses.begin() + (k + 1) * 12, NAN);
    }
    ++anchored;
    char item[160];
    sprintf_s(item, "%u indices on 0x%I64X, its bone %d (known at %d of %d samples; %s)", t.R->IndexCount, t.Owner, t.OwnerBone,
      known, (int)samples, t.ClothLocal ? "its instance carried" : "its vertices carried");
    if (list.size() < 700)
      list += (list.empty() ? "" : "; ") + std::string(item);
  }
  if (cloths)
    util::log::Write("Replay: %d of the %d cloths kept carried by the person they're on where none of their samples is near the "
      "time: %s", anchored, cloths, list.c_str());
  return anchored;
}

void replay_gamedraws::ClothAnchorsWanted(std::vector<std::pair<__int64, int>>& out)
{
  out.clear();
  std::lock_guard<std::mutex> lock(g_Mutex);
  for (auto const& entry : g_Tracks)
  {
    Track const& t = entry.second;
    if (t.Cloth && t.Owner && t.OwnerBone >= 0 &&
        std::find(out.begin(), out.end(), std::make_pair(t.Owner, t.OwnerBone)) == out.end())
      out.push_back({ t.Owner, t.OwnerBone });
  }
}

void replay_gamedraws::SetClothAnchors(std::vector<ClothAnchor> const& anchors)
{
  std::lock_guard<std::mutex> lock(g_Mutex);
  g_ClothAnchorsNow.clear();
  for (ClothAnchor const& a : anchors)
  {
    std::array<float, 12> pose;
    memcpy(pose.data(), a.Pose, sizeof(a.Pose));
    g_ClothAnchorsNow[std::make_pair(a.Owner, a.Bone)] = pose;
  }
}

void replay_gamedraws::SetWorldOn(bool on)
{
  g_WorldOn = on;
}

void replay_gamedraws::SetWorldCenter(float const* place)
{
  std::lock_guard<std::mutex> lock(g_Mutex);
  if (place)
    memcpy(g_WorldCenter, place, sizeof(g_WorldCenter));
  bool playing = place != nullptr;
  if (playing && !g_WorldPlaying)
    g_LoggedWorld = false;
  g_WorldPlaying = playing;
}

namespace
{
  // A mesh's id (the caller holds g_Mutex)
  int MeshIdOf(std::string const& name)
  {
    auto it = g_MeshIds.find(name);
    if (it != g_MeshIds.end())
      return it->second;
    int id = (int)g_MeshIds.size();
    g_MeshIds.emplace(name, id);
    return id;
  }

  char const* ShortName(std::string const& name)
  {
    size_t slash = name.find_last_of('/');
    return slash == std::string::npos ? name.c_str() : name.c_str() + slash + 1;
  }
}

void replay_gamedraws::UpdateWorld(bool recording, bool playing)
{
  // The game's mesh table read again (names, reach) only when its pointers
  // changed (2026-11-04: read every second, with the maps below, it took
  // 6 - 8 ms of one frame a second on the game thread while it recorded; in
  // the 01:20 recording's 56 s the table changed in 9 of them). Game thread.
  static std::vector<__int64> s_key, s_namedKey;
  static std::unordered_map<__int64, replay_shapes::MeshLevel> s_table;
  {
    std::vector<__int64> key;
    replay_shapes::MeshTableKey(key);
    if (key != s_key || s_table.empty())
    {
      replay_shapes::MeshTable(s_table);
      s_key.swap(key);
    }
  }
  std::unordered_map<__int64, replay_shapes::MeshLevel> const& table = s_table;
  std::set<std::string> shapes;
  if (recording || playing)
    replay_shapes::ShapeNames(shapes);
  // The meshes' reach and short names by vertex buffer, made before the lock
  // (2026-11-04: made under it once a second, the render thread's draw hooks
  // and read back waited on it); again only for a table read since
  std::shared_ptr<std::unordered_map<__int64, float>> reach;
  std::shared_ptr<std::unordered_map<__int64, std::string>> names;
  if (recording && (s_namedKey != s_key || s_namedKey.empty()))
  {
    s_namedKey = s_key;
    reach = std::make_shared<std::unordered_map<__int64, float>>();
    names = std::make_shared<std::unordered_map<__int64, std::string>>();
    for (auto const& entry : table)
    {
      if (entry.second.Reach > 0.f && entry.second.Name.find("/vistameshes/") == std::string::npos)
        (*reach)[entry.first] = entry.second.Reach;
      (*names)[entry.first] = ShortName(entry.second.Name);
    }
  }
  std::lock_guard<std::mutex> lock(g_Mutex);
  if (recording)
  {
    // Each kept one's mesh, once it's known
    for (auto& entry : g_World)
    {
      Recipe& r = *entry.second;
      if (r.MeshId >= 0)
        continue;
      auto it = table.find(std::get<0>(entry.first));
      if (it == table.end())
        continue;
      r.Mesh = it->second.Name;
      r.Lod = it->second.Lod;
      r.MeshId = MeshIdOf(r.Mesh);
    }
    // Each track's mesh; Faith's let go (the game draws her)
    for (auto& entry : g_Tracks)
    {
      Track& t = entry.second;
      if (!t.Mesh.empty())
        continue;
      // (A cloth's key is its index buffer: its mesh by a buffer of it the graphics card doesn't write)
      auto it = table.find(t.Cloth ? t.MeshVb : std::get<0>(entry.first));
      if (it == table.end())
        continue;
      t.Mesh = it->second.Name;
      t.Faith = _strnicmp(t.Mesh.c_str(), "characters/faith", 16) == 0;
      if (t.Faith)
      {
        g_TrackBytes -= (std::min)(g_TrackBytes, t.Data.size() * sizeof(float));
        std::vector<float>().swap(t.Data);
        t.Times.clear();
        t.Counts.clear();
        t.Starts.clear();
        g_ClothBytes -= (std::min)(g_ClothBytes, t.ClothData.size());
        std::vector<BYTE>().swap(t.ClothData);
        t.ClothStarts.clear();
      }
    }
    // A mesh at its most detailed level kept only, and none of a recorded
    // model's (drawn as the model: here it would stay where it was kept)
    std::unordered_map<int, int> best;
    for (auto const& entry : g_World)
      if (entry.second->MeshId >= 0)
      {
        auto it = best.find(entry.second->MeshId);
        if (it == best.end() || entry.second->Lod < it->second)
          best[entry.second->MeshId] = entry.second->Lod;
      }
    for (auto it = g_World.begin(); it != g_World.end();)
    {
      Recipe const& r = *it->second;
      // A recorded model's mesh, or a gun's, drone's or door's (by its buffer: a
      // door drawn without bones too)
      bool model = (r.MeshId >= 0 && shapes.count(ShortName(r.Mesh))) || g_WantedVbs.count(std::get<0>(it->first));
      if (model || (r.MeshId >= 0 && r.Lod > best[r.MeshId]))
      {
        g_WorldAttempts[it->first] = MaxAttempts;
        it = g_World.erase(it);
      }
      else
        ++it;
    }
  }
  if (reach)
  {
    g_VbReach = reach;
    g_VbNames = names;
  }
  if (playing)
  {
    // The game's vertex buffers of the meshes kept, for its draws to be known (see OnDraw)
    auto meshes = std::make_shared<std::unordered_map<__int64, int>>();
    for (auto const& entry : table)
    {
      auto it = g_MeshIds.find(entry.second.Name);
      if (it != g_MeshIds.end())
        (*meshes)[entry.first] = it->second;
      // A recorded model's mesh: its own id too (probed like the world's)
      char const* name = ShortName(entry.second.Name);
      if (shapes.count(name))
      {
        int id = it != g_MeshIds.end() ? it->second : MeshIdOf(entry.second.Name);
        (*meshes)[entry.first] = id;
        g_ModelMeshIds[name] = id;
      }
    }
    g_GameVbMeshes = meshes;
  }
  else
    g_GameVbMeshes.reset();
}

int replay_gamedraws::DropWorld(std::function<bool(double clock, float const* center, float radius)> const& test)
{
  std::lock_guard<std::mutex> lock(g_Mutex);
  int dropped = 0;
  for (auto it = g_World.begin(); it != g_World.end();)
  {
    Recipe const& r = *it->second;
    if (r.Ready && test(r.Clock, r.Center, r.Radius))
    {
      g_WorldAttempts[it->first] = MaxAttempts;
      it = g_World.erase(it);
      ++dropped;
    }
    else
      ++it;
  }
  return dropped;
}

void replay_gamedraws::WorldCounts(int& kept, int& ready)
{
  std::lock_guard<std::mutex> lock(g_Mutex);
  kept = (int)g_World.size();
  ready = 0;
  for (auto const& entry : g_World)
    ready += entry.second->Ready && entry.second->MeshId >= 0;
}

void replay_gamedraws::SetWantedSkins(std::vector<__int64> const& vbs)
{
  std::lock_guard<std::mutex> lock(g_Mutex);
  g_WantedVbs.clear();
  g_WantedVbs.insert(vbs.begin(), vbs.end());
}

void replay_gamedraws::TakeSkinCandidates(std::vector<SkinCandidate>& out)
{
  std::lock_guard<std::mutex> lock(g_Mutex);
  out.swap(g_SkinCandidates);
  g_SkinCandidates.clear();
}

void replay_gamedraws::ReleaseSkin(int id)
{
  std::lock_guard<std::mutex> lock(g_Mutex);
  g_Skins.erase(id);
}

void replay_gamedraws::SetPeopleDraws(std::vector<PersonDraw> const& draws)
{
  std::lock_guard<std::mutex> lock(g_Mutex);
  g_PeopleDraws = draws;
  bool playing = !g_PeopleDraws.empty();
  if (playing && !g_PeoplePlaying)
    g_LoggedPeople = false;
  g_PeoplePlaying = playing;
}

void replay_gamedraws::TakeDrawn(std::set<long long>& out)
{
  std::lock_guard<std::mutex> lock(g_Mutex);
  out.swap(g_Drawn);
  g_Drawn.clear();
}

int replay_gamedraws::Ready()
{
  return g_Ready.load();
}

// Saved recordings (ReplayStore.h): each kept draw's objects as parts (its
// instance slots' views are the game's instance buffer: not kept, the tools'
// own are bound there when drawn), its constants and instances as they were
// read back
namespace
{
  const int SavedObjects = 10 + VbSlots + VsViewSlots + PsViewSlots + VsSamplerSlots + PsSamplerSlots;

  void ObjectsOf(Recipe const& r, std::vector<IUnknown*>& out)
  {
    out = { r.Vs.Get(), r.Ps.Get(), r.Gs.Get(), r.Hs.Get(), r.Ds.Get(), r.Layout.Get(), r.Ib.Get(), r.Depth.Get(), r.Blend.Get(),
      r.Raster.Get() };
    for (int i = 0; i < VbSlots; ++i)
      out.push_back(r.Vbs[i].Get());
    for (int i = 0; i < VsViewSlots; ++i)
      out.push_back(r.VsInstanceSlot[i] ? nullptr : r.VsViews[i].Get());
    for (int i = 0; i < PsViewSlots; ++i)
      out.push_back(r.PsInstanceSlot[i] ? nullptr : r.PsViews[i].Get());
    for (int i = 0; i < VsSamplerSlots; ++i)
      out.push_back(r.VsSamplers[i].Get());
    for (int i = 0; i < PsSamplerSlots; ++i)
      out.push_back(r.PsSamplers[i].Get());
  }

  // Nothing written (false) when one of its objects can't be saved
  const int RecipesVersion2 = -2;   // the first int of the section from version 2 on (before: the count)
  const int RecipesVersion3 = -3;   // version 3: each one's pass and its place
  const int RecipesVersion4 = -4;   // version 4: each one's video screen slots (see NoteFeedTextures)

  bool WriteRecipe(replay_store::Out& out, replay_store::SaveParts& parts, Recipe const& r, int skinId, bool feeds = false)
  {
    std::vector<IUnknown*> objects;
    ObjectsOf(r, objects);
    std::vector<int> ids;
    for (IUnknown* object : objects)
    {
      int id = replay_store::AddPart(parts, object);
      if (id < -1)
        return false;
      ids.push_back(id);
    }
    out.Put(skinId);
    out.Put(r.Shape);
    out.Put(r.Section);
    out.Put((unsigned char)r.Skin);
    out.Put(r.Clock);
    out.Bytes(r.Name, sizeof(r.Name));
    out.Vec(ids);
    out.Put(r.Topology);
    out.Bytes(r.Strides, sizeof(r.Strides));
    out.Bytes(r.Offsets, sizeof(r.Offsets));
    out.Put(r.IbFormat);
    out.Put(r.IbOffset);
    out.Put(r.IndexCount);
    out.Put(r.StartIndex);
    out.Put(r.Instances);
    out.Put(r.BaseVertex);
    out.Bytes(r.VsInstanceSlot, sizeof(r.VsInstanceSlot));
    out.Bytes(r.PsInstanceSlot, sizeof(r.PsInstanceSlot));
    out.Put(r.StencilRef);
    out.Bytes(r.BlendFactor, sizeof(r.BlendFactor));
    out.Put(r.SampleMask);
    out.Put(r.VsOffsetsAt);
    out.Put(r.PsOffsetsAt);
    out.Put(r.Stride);
    out.Put(r.Kept);
    out.Vec(r.VsB0Bytes);
    out.Vec(r.PsB0Bytes);
    out.Vec(r.VsB1Bytes);
    out.Vec(r.PsB1Bytes);
    out.Vec(r.InstanceData);
    out.Vec(r.WorldRows);
    // Version 2
    out.Text(r.Mesh);
    out.Put(r.Lod);
    out.Bytes(r.Center, sizeof(r.Center));
    out.Put(r.Radius);
    // Version 3
    out.Put(r.Pass);
    out.Put(r.PassOrdinal);
    // Version 4 (the kept draws' section only)
    if (feeds)
      out.Bytes(r.Feed, sizeof(r.Feed));
    return true;
  }

  // complete: every object it had was made again
  bool ReadRecipe(replay_store::In& in, replay_store::LoadParts const& parts, Recipe& r, int& skinId, bool& complete, int version,
    bool instances = true)
  {
    unsigned char skin = 0;
    std::vector<int> ids;
    if (!in.Get(skinId) || !in.Get(r.Shape) || !in.Get(r.Section) || !in.Get(skin) || !in.Get(r.Clock) ||
        !in.Bytes(r.Name, sizeof(r.Name)) || !in.Vec(ids, SavedObjects) || ids.size() != (size_t)SavedObjects ||
        !in.Get(r.Topology) || !in.Bytes(r.Strides, sizeof(r.Strides)) || !in.Bytes(r.Offsets, sizeof(r.Offsets)) ||
        !in.Get(r.IbFormat) || !in.Get(r.IbOffset) || !in.Get(r.IndexCount) || !in.Get(r.StartIndex) || !in.Get(r.Instances) ||
        !in.Get(r.BaseVertex) || !in.Bytes(r.VsInstanceSlot, sizeof(r.VsInstanceSlot)) ||
        !in.Bytes(r.PsInstanceSlot, sizeof(r.PsInstanceSlot)) || !in.Get(r.StencilRef) ||
        !in.Bytes(r.BlendFactor, sizeof(r.BlendFactor)) || !in.Get(r.SampleMask) || !in.Get(r.VsOffsetsAt) ||
        !in.Get(r.PsOffsetsAt) || !in.Get(r.Stride) || !in.Get(r.Kept) || !in.Vec(r.VsB0Bytes, MaxWindowBytes) ||
        !in.Vec(r.PsB0Bytes, MaxWindowBytes) || !in.Vec(r.VsB1Bytes, MaxWindowBytes) || !in.Vec(r.PsB1Bytes, MaxWindowBytes) ||
        !in.Vec(r.InstanceData, (size_t)MaxInstancesKept * MaxStride * 4) || !in.Vec(r.WorldRows, MaxStride))
      return false;
    if (version >= 2 && (!in.Text(r.Mesh, 512) || !in.Get(r.Lod) || !in.Bytes(r.Center, sizeof(r.Center)) || !in.Get(r.Radius)))
      return false;
    if (version >= 3 && (!in.Get(r.Pass) || !in.Get(r.PassOrdinal) || r.Pass < Pass_Main || r.Pass >= Passes || r.PassOrdinal < 0 ||
        r.PassOrdinal >= MaxPassOrdinals))
      return false;
    if (version >= 4)
    {
      if (!in.Bytes(r.Feed, sizeof(r.Feed)))
        return false;
      for (unsigned char f : r.Feed)
      {
        if (f > Feeds * 2)
          return false;
        r.HasFeed = r.HasFeed || f;
      }
    }
    r.Name[sizeof(r.Name) - 1] = 0;
    r.Skin = skin != 0;
    complete = !instances || (r.Stride && r.Stride <= MaxStride && r.Kept && r.InstanceData.size() == (size_t)r.Kept * r.Stride * 4 &&
      !r.VsB0Bytes.empty() && (r.Skin || !r.WorldRows.empty()));
    for (int k : r.WorldRows)
      complete = complete && k >= 0 && (UINT)k + 3 <= r.Stride;
    for (int id : ids)
      complete = complete && (id < 0 || replay_store::PartObject(parts, id));
    if (!complete)
      return true;
    int at = 0;
    r.Vs = replay_store::PartAs<ID3D11VertexShader>(parts, ids[at++]);
    r.Ps = replay_store::PartAs<ID3D11PixelShader>(parts, ids[at++]);
    r.Gs = replay_store::PartAs<ID3D11GeometryShader>(parts, ids[at++]);
    r.Hs = replay_store::PartAs<ID3D11HullShader>(parts, ids[at++]);
    r.Ds = replay_store::PartAs<ID3D11DomainShader>(parts, ids[at++]);
    r.Layout = replay_store::PartAs<ID3D11InputLayout>(parts, ids[at++]);
    r.Ib = replay_store::PartAs<ID3D11Buffer>(parts, ids[at++]);
    r.Depth = replay_store::PartAs<ID3D11DepthStencilState>(parts, ids[at++]);
    r.Blend = replay_store::PartAs<ID3D11BlendState>(parts, ids[at++]);
    r.Raster = replay_store::PartAs<ID3D11RasterizerState>(parts, ids[at++]);
    for (int i = 0; i < VbSlots; ++i)
      r.Vbs[i] = replay_store::PartAs<ID3D11Buffer>(parts, ids[at++]);
    for (int i = 0; i < VsViewSlots; ++i)
      r.VsViews[i] = replay_store::PartAs<ID3D11ShaderResourceView>(parts, ids[at++]);
    for (int i = 0; i < PsViewSlots; ++i)
      r.PsViews[i] = replay_store::PartAs<ID3D11ShaderResourceView>(parts, ids[at++]);
    for (int i = 0; i < VsSamplerSlots; ++i)
      r.VsSamplers[i] = replay_store::PartAs<ID3D11SamplerState>(parts, ids[at++]);
    for (int i = 0; i < PsSamplerSlots; ++i)
      r.PsSamplers[i] = replay_store::PartAs<ID3D11SamplerState>(parts, ids[at++]);
    // Each object as it was (one that was there and isn't now: not complete)
    std::vector<IUnknown*> objects;
    ObjectsOf(r, objects);
    for (size_t i = 0; i < objects.size(); ++i)
      complete = complete && ((ids[i] >= 0) == (objects[i] != nullptr));
    return true;
  }
}

int replay_gamedraws::Save(replay_store::Out& out, replay_store::SaveParts& parts, std::set<int> const& skins, int& unsaved)
{
  std::vector<std::pair<int, std::shared_ptr<Recipe>>> list;
  {
    std::lock_guard<std::mutex> lock(g_Mutex);
    for (auto const& entry : g_Recipes)
      if (entry.second->Ready)
        list.push_back({ -1, entry.second });
    for (int id : skins)
    {
      auto it = g_Skins.find(id);
      if (it != g_Skins.end() && it->second->Stride)
        list.push_back({ id, it->second });
    }
    // The world's, with their meshes known (-2)
    for (auto const& entry : g_World)
      if (entry.second->Ready && entry.second->MeshId >= 0)
        list.push_back({ -2, entry.second });
  }
  replay_store::Out body;
  int written = 0;
  unsaved = 0;
  for (auto const& entry : list)
  {
    if (WriteRecipe(body, parts, *entry.second, entry.first, true))
      ++written;
    else
      ++unsaved;
  }
  out.Put(RecipesVersion4);
  out.Put(written);
  out.Bytes(body.Data.data(), body.Data.size());
  return written;
}

int replay_gamedraws::SaveTracks(replay_store::Out& out, replay_store::SaveParts& parts, int& unsaved)
{
  // Written under the lock (not copied first: they may take hundreds of MB)
  std::lock_guard<std::mutex> lock(g_Mutex);
  replay_store::Out body;
  int written = 0;
  unsaved = 0;
  for (auto const& entry : g_Tracks)
  {
    Track const& t = entry.second;
    if (!t.R || !t.R->Ready || t.Header < 0 || t.Faith || t.Times.empty())
      continue;
    replay_store::Out one;
    if (!WriteRecipe(one, parts, *t.R, -3))
    {
      ++unsaved;
      continue;
    }
    one.Text(t.Mesh);
    one.Put(t.Header);
    one.Vec(t.Times);
    one.Vec(t.Counts);
    size_t floats = (size_t)t.R->Stride * 4, rows = t.Data.size() / floats;
    one.Frames(t.Data.data(), rows, floats * sizeof(float), sizeof(float));
    body.Bytes(one.Data.data(), one.Data.size());
    ++written;
  }
  out.Put(RecipesVersion3);
  out.Put(written);
  out.Bytes(body.Data.data(), body.Data.size());
  return written;
}

int replay_gamedraws::LoadTracks(replay_store::In& in, replay_store::LoadParts const& parts, int& dropped)
{
  int count = 0, version = 2;
  dropped = 0;
  if (!in.Get(count))
    return -1;
  if (count == RecipesVersion3)
  {
    version = 3;
    if (!in.Get(count))
      return -1;
  }
  if (count < 0 || count > MaxTracks)
    return -1;
  std::map<WorldKeyType, Track> tracks;
  size_t bytes = 0;
  for (int i = 0; i < count; ++i)
  {
    auto r = std::make_shared<Recipe>();
    int skinId = 0;
    bool complete = false;
    Track t;
    std::vector<unsigned char> data;
    if (!ReadRecipe(in, parts, *r, skinId, complete, version) || !in.Text(t.Mesh, 512) || !in.Get(t.Header) ||
        !in.Vec(t.Times, 1 << 22) || !in.Vec(t.Counts, 1 << 22) || t.Counts.size() != t.Times.size() || !in.Frames(data, MaxTrackBytes))
      return -1;
    size_t floats = (size_t)r->Stride * 4;
    size_t total = 0;
    for (UINT c : t.Counts)
      total += c;
    if (!complete || !floats || data.size() != total * floats * sizeof(float) || t.Header < 0)
    {
      ++dropped;
      continue;
    }
    t.Data.resize(total * floats);
    memcpy(t.Data.data(), data.data(), data.size());
    size_t at = 0;
    for (UINT c : t.Counts)
    {
      t.Starts.push_back(at);
      at += c * floats;
    }
    r->Track = true;
    r->Ready = true;
    t.R = r;
    bytes += data.size();
    tracks[WorldKeyType((__int64)i + 1, 1, 0)] = std::move(t);
  }
  std::lock_guard<std::mutex> lock(g_Mutex);
  g_Tracks.swap(tracks);
  g_TrackBytes = bytes;
  return (int)g_Tracks.size();
}

// The crowds (their own section): each kind's state as a kept draw (its
// crowd buffers' slots and formats after it), then the samples
namespace
{
  const int CrowdsVersion1 = -101;
}

int replay_gamedraws::SaveCrowds(replay_store::Out& out, replay_store::SaveParts& parts, int& unsaved)
{
  std::lock_guard<std::mutex> lock(g_Mutex);
  replay_store::Out body;
  std::vector<int> saved(g_CrowdKinds.size(), -1);
  int written = 0;
  unsaved = 0;
  for (size_t i = 0; i < g_CrowdKinds.size(); ++i)
  {
    CrowdKind const& k = *g_CrowdKinds[i];
    if (!k.StateRead || k.Samples.empty())
      continue;
    replay_store::Out one;
    if (!WriteRecipe(one, parts, *k.R, -4))
    {
      ++unsaved;
      continue;
    }
    one.Bytes(k.Slot, sizeof(k.Slot));
    one.Bytes(k.PsSlot, sizeof(k.PsSlot));
    one.Bytes(k.Format, sizeof(k.Format));
    one.Put((unsigned char)k.PsB1IsVsB1);
    one.Text(k.Mesh);
    body.Bytes(one.Data.data(), one.Data.size());
    saved[i] = written++;
  }
  int samples = 0;
  replay_store::Out list;
  for (CrowdSample const& s : g_CrowdSamples)
  {
    if (s.Kind < 0 || s.Kind >= (int)saved.size() || saved[s.Kind] < 0)
      continue;
    list.Put(s.Time);
    list.Put(saved[s.Kind]);
    list.Vec(s.B1);
    list.Vec(s.Members);
    list.Vec(s.Transforms);
    list.Vec(s.Animations);
    ++samples;
  }
  out.Put(CrowdsVersion1);
  out.Put(written);
  out.Bytes(body.Data.data(), body.Data.size());
  out.Put(samples);
  out.Bytes(list.Data.data(), list.Data.size());
  return written;
}

int replay_gamedraws::LoadCrowds(replay_store::In& in, replay_store::LoadParts const& parts, int& dropped)
{
  int version = 0, count = 0;
  dropped = 0;
  if (!in.Get(version) || version != CrowdsVersion1 || !in.Get(count) || count < 0 || count > MaxCrowdKinds)
    return -1;
  std::vector<std::shared_ptr<CrowdKind>> kinds;
  std::vector<int> index;   // in the file -> in kinds (-1: dropped)
  for (int i = 0; i < count; ++i)
  {
    auto k = std::make_shared<CrowdKind>();
    k->R = std::make_shared<Recipe>();
    int skinId = 0;
    bool complete = false;
    unsigned char psB1 = 0;
    if (!ReadRecipe(in, parts, *k->R, skinId, complete, 3, false) || !in.Bytes(k->Slot, sizeof(k->Slot)) ||
        !in.Bytes(k->PsSlot, sizeof(k->PsSlot)) || !in.Bytes(k->Format, sizeof(k->Format)) || !in.Get(psB1) || !in.Text(k->Mesh, 512))
      return -1;
    bool sane = k->Slot[Crowd_Indirection] >= 0 && k->Slot[Crowd_Indirection] < CrowdVsViewSlots && k->Slot[Crowd_Transform] >= 0 &&
      k->Slot[Crowd_Transform] < CrowdVsViewSlots && k->Slot[Crowd_Animation] < CrowdVsViewSlots;
    for (int b = 0; b < CrowdBuffers; ++b)
      sane = sane && k->PsSlot[b] < PsViewSlots && (k->Slot[b] < 0 || CrowdElementBytes(k->Format[b]));
    if (!complete || !sane || !k->R->Vs || !k->R->Ps)
    {
      ++dropped;
      index.push_back(-1);
      continue;
    }
    k->PsB1IsVsB1 = psB1 != 0;
    k->StateRead = true;
    k->R->Ready = true;
    index.push_back((int)kinds.size());
    kinds.push_back(k);
  }
  int samples = 0;
  if (!in.Get(samples) || samples < 0 || samples > (1 << 24))
    return -1;
  std::vector<CrowdSample> list;
  size_t bytes = 0;
  for (int i = 0; i < samples; ++i)
  {
    CrowdSample s;
    int kind = -1;
    if (!in.Get(s.Time) || !in.Get(kind) || !in.Vec(s.B1, MaxWindowBytes) || !in.Vec(s.Members, MaxCrowdMembers) ||
        !in.Vec(s.Transforms, (size_t)MaxCrowdMembers * 4) || !in.Vec(s.Animations, MaxCrowdMembers))
      return -1;
    if (kind < 0 || kind >= (int)index.size() || index[kind] < 0 || s.Transforms.size() != s.Members.size() * 4 ||
        (!s.Animations.empty() && s.Animations.size() != s.Members.size()) || s.B1.size() < 16)
      continue;
    s.Kind = index[kind];
    bytes += s.B1.size() + s.Members.size() * 4 + s.Transforms.size() * 4 + s.Animations.size() * 4;
    kinds[s.Kind]->Samples.push_back(list.size());
    list.push_back(std::move(s));
  }
  std::lock_guard<std::mutex> lock(g_Mutex);
  g_CrowdKinds.swap(kinds);
  g_CrowdKindKeys.clear();
  g_CrowdSamples.swap(list);
  g_CrowdBytes = bytes;
  return (int)g_CrowdKinds.size();
}

int replay_gamedraws::Load(replay_store::In& in, replay_store::LoadParts const& parts, int& dropped)
{
  int count = 0, version = 1;
  dropped = 0;
  if (!in.Get(count))
    return -1;
  if (count == RecipesVersion2 || count == RecipesVersion3 || count == RecipesVersion4)
  {
    version = count == RecipesVersion4 ? 4 : count == RecipesVersion3 ? 3 : 2;
    if (!in.Get(count))
      return -1;
  }
  if (count < 0 || count > MaxRecipes + MaxSkins + MaxWorld)
    return -1;
  std::map<WorldKeyType, std::shared_ptr<Recipe>> world;
  std::map<long long, std::shared_ptr<Recipe>> recipes;
  std::map<int, std::shared_ptr<Recipe>> skins;
  int lastSkin = 0;
  for (int i = 0; i < count; ++i)
  {
    auto r = std::make_shared<Recipe>();
    int skinId = -1;
    bool complete = false;
    if (!ReadRecipe(in, parts, *r, skinId, complete, version))
      return -1;
    if (!complete)
    {
      ++dropped;
      continue;
    }
    if (skinId == -2)
    {
      // The world's: by its place in the file (its game buffer isn't this session's)
      r->World = true;
      r->Ready = !r->Mesh.empty();
      r->WorldKey = WorldKeyType((__int64)i + 1, 0, 0);
      world[r->WorldKey] = r;
    }
    else if (skinId >= 0)
    {
      skins[skinId] = r;
      lastSkin = (std::max)(lastSkin, skinId);
    }
    else
    {
      r->Ready = true;
      long long key = KeyOf(r->Shape, r->Section);
      if (r->Pass != Pass_Main)
        key |= ((long long)(r->Pass * MaxPassOrdinals + r->PassOrdinal + 1)) << 48;
      // A part's variations (see VariantSameMetres): numbered from bit 40 again
      int variant = 0;
      int most = r->Pass == Pass_Rv ? MaxRvVariants : MaxVariants;
      while (variant < most && recipes.count(key | ((long long)variant << 40)))
        ++variant;
      if (variant < most)
        recipes[key | ((long long)variant << 40)] = r;
    }
  }
  std::lock_guard<std::mutex> lock(g_Mutex);
  g_Recipes.swap(recipes);
  g_Skins.swap(skins);
  g_World.swap(world);
  g_WorldAttempts.clear();
  g_MeshIds.clear();
  g_ModelMeshIds.clear();
  for (auto& entry : g_World)
    entry.second->MeshId = MeshIdOf(entry.second->Mesh);
  g_NextSkinId = (std::max)(g_NextSkinId, lastSkin + 1);
  g_Ready = (int)g_Recipes.size();
  g_Attempts.clear();
  g_Draws.clear();
  g_Drawn.clear();
  g_SkinCandidates.clear();
  g_PeopleDraws.clear();
  return (int)(g_Recipes.size() + g_Skins.size() + g_World.size());
}
