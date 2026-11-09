#pragma once
#include <d3d11.h>
#include <vector>

// Diagnostic (temporary, 2026-10-27): stage 0 of drawing a recording with the
// game's own draws (Research_Replay_Mission_World_2026-10-26.md section 17).
// While the replay records, the game's draws of the recorded models and its
// draws that read bone matrices are described in the log: how their constant
// buffers and the instance buffer are bound, the passes they're in (render
// targets, depth test), and which of the game's shaders they use (by the
// names in the shaders, kept as the game makes them). Once, 5 s into a
// recording, one frame's constants and instance data of those draws are
// copied on the graphics card and read back, for the log. Nothing is drawn
// and nothing of the game's is changed. See the .cpp.
namespace replay_drawlog
{
  // Render thread, each Present: the device's shader creation hooked the
  // first time; the frame counted; a snapshot read back once it's done
  void Present(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);

  // Game thread: the replay starts / stops recording
  void Start();
  void Stop();

  // Any thread: a recorded model's mesh, by the game's buffers it's drawn from
  void NoteMesh(__int64 vb, __int64 ib, char const* name);

  // Game thread, while recording: where the recorded people are (their bone
  // 1), for the snapshot's bone-reading draws; WantsPeople: only then
  struct PersonPlace
  {
    __int64 Entity;
    char Label[40];
    float Place[3];
  };
  bool WantsPeople();
  void NotePeople(std::vector<PersonPlace> const& people);

  // Any thread: a shader of the game's by its object (noted as it was made):
  // where dxVectorBufferOffsets is in its functionConstants (at: -1 none);
  // false if it wasn't noted
  bool VectorOffsetsOf(__int64 shader, int& at);
  // A vertex shader of the game's that reads bone matrices (a skinned mesh's)
  bool ReadsBones(__int64 shader);
  // A pixel shader's video screen texture slots (resources named like the
  // game's g_lmsAlbedoTexture: LinearMedia's frames), at most max; how many
  int LmsSlots(__int64 shader, int* slots, int max);
  // The names in a shader of the game's (its constants and resources, as
  // noted when it was made); false if it wasn't noted
  bool ShaderNames(__int64 shader, char* out, size_t size);

  // From the replay's draw hooks (any thread, any context): Active is cheap
  bool Active();
  void OnDraw(ID3D11DeviceContext* pContext, UINT indexCount, UINT instances, UINT startIndex, INT baseVertex, UINT startInstance);
}
