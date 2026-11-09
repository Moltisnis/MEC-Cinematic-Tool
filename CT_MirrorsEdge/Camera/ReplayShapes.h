#pragma once
#include <d3d11.h>
#include <cstddef>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace replay_store { class Out; class In; struct SaveParts; struct LoadParts; }

// The shapes of the replay's moving models (2026-10-27): a model's mesh as the
// game had it while recording, so a model the game no longer has when the
// replay plays (a mission's gates and doors, taken out with the mission) is
// drawn by the replay where it was recorded. See the .cpp.
namespace replay_shapes
{
  // Game thread, while recording: a static model's mesh (its most detailed
  // level loaded then), kept; the shape's id (one mesh is kept once), or -1
  // if it couldn't be read (not a rigid mesh, nothing loaded, too big).
  // quiet: nothing in the log for it (counted in Count / Refused instead)
  int Capture(__int64 entity, char const* label, bool quiet);
  int Refused();
  // Game thread (stage 2b): the vertex buffers of a mesh's loaded levels (the
  // game's ID3D11Buffer addresses; how many, 0 if they couldn't be read): a
  // static model's whatever its mesh (a skinned door's), or the MeshSet whose
  // address is at 'at' (a gun's, its entity + 0x258)
  int ModelMeshBuffers(__int64 entity, std::vector<__int64>& vbs);
  int MeshSetBuffersAt(__int64 at, std::vector<__int64>& vbs);
  // Game thread: the vertex buffers of every mesh in the game's mesh table
  // whose name starts with prefix (case not minded); how many meshes, and
  // their names if asked for
  int MeshBuffersNamed(char const* prefix, std::vector<__int64>& vbs, std::vector<std::string>* names);
  // Game thread: the recording is over (the game's draws aren't looked at for
  // the shapes' colour textures any more, see the .cpp)
  void EndCapture();

  // Game thread, each frame while the replay plays: what to draw this frame
  // (the recorded models that aren't there), in place of the last list
  struct Draw
  {
    int Shape;
    float Transform[16];   // the model's transform as the game's getTransform gives it (4 rows)
    __int64 Entity;        // who it was (the game's own draws pick its instance by it, ReplayGameDraws)
    bool Still = false;    // it didn't move while it recorded (ReplayModels' Track::Still)
    bool LiveCopy = false; // the game still has it, but doesn't draw it there (ReplayModels' Track::CopyLive)
    // Drawn by the game only in its runner's vision layer (ReplayModels'
    // Track::RvLayer: a Dash's marker): never drawn as a shape, only with the
    // game's own draws of it into the replay's mask (ReplayGameDraws' Pass_Rv)
    bool RvLayer = false;
    // One that went along with another recorded model (ReplayModels'
    // Track::Follows): its place over the recording, 4 floats a point (time,
    // x y z); none for the others
    std::shared_ptr<std::vector<float> const> Path;
  };
  void SetDraws(Draw const* draws, int count);

  // Game thread: every shape forgotten (a new recording, or forgotten)
  void Clear();
  int Count();
  // A kept shape's mesh (its short name, as the mesh table's names end), "" if none
  std::string NameOf(int shape);
  // The draw hooks' own time (QueryPerformanceCounter ticks) and the draws
  // they looked at since the last call (for the recording's cost in the log)
  void TakeHookCost(long long& ticks, long long& draws);
  size_t Bytes();

  // Render thread, from the Present hook (on the game's swapchain)
  void Apply(IDXGISwapChain* pSwapChain, ID3D11Device* pDevice, ID3D11DeviceContext* pContext);

  // Saved recordings (ReplayStore.h): every shape written in order (a model's
  // shape id stays its index) with its parts (its copies, its colour
  // textures), and read back in place of these (after Clear)
  void Save(replay_store::Out& out, replay_store::SaveParts& parts);
  bool Load(replay_store::In& in, replay_store::LoadParts const& parts);
  // Game thread, about once a second while it records: each captured
  // shape's mesh at its other levels loaded now, for the game's draws of a
  // model at another level to be kept too (its parts numbered from 0x8000,
  // the level from bit 12: replay_gamedraws draws them, as a whole, when none
  // at the shape's own level was kept)
  void MapLevels();
  // A saved recording was loaded: the game's draws hooked at the next
  // Present though nothing records (its kept draws are drawn through them)
  void WantDrawHooks();
  // Render thread, from the game's render target changes on its immediate
  // context (ReplayGameDraws): its draw functions now, any the draw hooks
  // don't have yet noted (hooked too at the next Present; see the .cpp's
  // MoreDrawFunctions)
  void NoteContextDraws(ID3D11DeviceContext* pContext);

  // Game thread: every loaded level's vertex buffer in the game's mesh table
  // (the game's ID3D11Buffer addresses), with its mesh's full name and level
  // (0 the most detailed); how many meshes
  struct MeshLevel
  {
    std::string Name;
    int Lod;
    // How far the mesh reaches from its origin (its bounding box's farthest
    // corner, unscaled); 0 when the box wasn't read or isn't plausible
    float Reach = 0.f;
  };
  int MeshTable(std::unordered_map<__int64, MeshLevel>& out);
  // The same table's pointers only (each MeshSet, its data, its loaded
  // levels' vertex buffers): the same while MeshTable would read the same
  void MeshTableKey(std::vector<__int64>& out);
  // Game thread: the skeleton's bone for each bone matrix of a skinned draw
  // (in their order), from the game's mesh: the loaded level whose vertex
  // buffer is vb, its section starting at startIndex with indexCount indices;
  // false when it isn't found
  bool SectionBones(__int64 vb, unsigned startIndex, unsigned indexCount, std::vector<int>& bones);
  // Game thread: the kept shapes' meshes (their short names)
  void ShapeNames(std::set<std::string>& out);
}
