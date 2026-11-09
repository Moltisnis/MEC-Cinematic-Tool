#include "ReplayDrawLog.h"
#include "ReplayGameDraws.h"
#include "ReplayStore.h"
#include "../Main.h"
#include "../Util/Util.h"
#include <Windows.h>
#include <d3d11_1.h>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

// Stage 0 of drawing a recording with the game's own draws (2026-10-27,
// diagnostic, temporary; Research_Replay_Mission_World_2026-10-26.md
// section 17). The game's mesh shaders (its city shader database, read
// offline) take a draw's data from b0 functionConstants (per draw: where its
// instances start in the instance buffer and their stride,
// dxVectorBufferOffsets), b1 externalConstants (the material's) and b2
// viewConstants (the camera), and its placement from t0 instanceVectorBuffer
// (float4s: a model's world matrix as 3 rows; a skinned mesh's bones as 3
// rows each from its instance's + 12). What can't be read offline is
// logged here while the replay records:
// - The game's shaders by the names in them: the device's CreateVertexShader
//   and CreatePixelShader (its vtable slots 12 and 15) are hooked at the first
//   Present, and each shader made after is noted by its object: a hash of its
//   code, its bindings (slot and name), its constant buffers' sizes, where
//   dxVectorBufferOffsets is in its functionConstants, how many outputs a
//   pixel shader has, and whether a vertex shader reads bone matrices (its
//   code has the immediate l(3, 3, 3, 3), the bone index times 3 rows: checked
//   offline over the 2637 instanced vertex shaders against fxc's disassembly,
//   all 649 with that multiply have it and none of the others).
// - Once a second (one frame in SampleEvery), each of the game's indexed draws
//   (the replay's DrawIndexed / DrawIndexedInstanced hooks, ReplayShapes.cpp)
//   is counted by its pass (its render targets and depth target), and a draw
//   of a recorded model (by its vertex buffer, NoteMesh) or one whose vertex
//   shader reads bones (not a depth-only one) has its state logged the first
//   time it's seen with those shaders in that pass: the context, its
//   constant buffers (with the D3D 11.1 offsets, VSGetConstantBuffers1), its
//   instance buffer, its pixel shader's views, its targets, depth test,
//   blending, culling. Every 10 s the passes are listed.
// - Once, 5 s into a recording: in one frame, on the first deferred or
//   immediate context that draws one of those, up to MaxSnapshotDraws of them
//   have their b0 (and the first's b2) and their instance buffer copied on the
//   graphics card (CopyResource into the tools' staging buffers, in the
//   game's command list right at the draw) and read back three frames later
//   on the render thread: each draw's start and stride, its world rows or its
//   first bones, and for a skinned one the recorded person nearest its first
//   bone (FaithReplay notes their places then).
// Only Get calls and copies into the tools' own buffers: nothing is drawn,
// no state of the game's is set, nothing of the game's is written.
namespace
{
  const unsigned SampleEvery = 60;
  const int MaxStateLogs = 120;
  const int MaxStateLogsPerPass = 24;   // 2026-10-27 15:59: 48 in all went to the G-buffer pass in the first second
  const ULONGLONG PassLogEveryMs = 10000;
  const ULONGLONG SnapshotAfterMs = 5000;
  const int MaxSnapshotDraws = 24;
  // b0 and b1 are windows (11.1 offsets) into one 1 MB dynamic ring (2026-10-27 15:59), copied whole
  const UINT MaxConstantCopyBytes = 2u << 20;
  const UINT MaxInstanceCopyBytes = 32u << 20;
  const UINT EveryDrawCopyBytes = 4u << 20;    // smaller instance buffers are copied at each draw
  const size_t MaxSnapshotBytes = 160u << 20;
  const unsigned ReadbackAfterFrames = 3, ReadbackGiveUpFrames = 120;
  const size_t MaxShaders = 65536;
  const int MaxPasses = 64;
  const int VertexShaderSlot = 12, PixelShaderSlot = 15;   // ID3D11Device
  const int CameraPosOffset = 720;                         // viewConstants
  const int BoneRowsStart = 12;

  struct ShaderInfo
  {
    unsigned long long Hash;
    UINT Size;
    bool Parsed;
    bool Vertex;
    bool VectorBuffer;
    bool Bones;
    int VectorOffsetsAt;   // in functionConstants, -1: none
    int Outputs;
    bool Described;
    char Names[600];
    // Its video screens' texture slots (a resource named "...lms...Texture",
    // the game's g_lmsAlbedoTexture: LinearMedia's frames), -1 none; and
    // whether one of its resources is named for cloth (2026-11-01: how the
    // game draws a cloth mesh, Icarus's jacket, wasn't known)
    int LmsSlots[2];
    bool Cloth;
  };
  std::mutex g_ShaderMutex;
  std::unordered_map<__int64, ShaderInfo> g_Shaders;
  std::atomic<int> g_VertexShaders{ 0 }, g_PixelShaders{ 0 }, g_ShadersUnread{ 0 };

  struct MeshNote
  {
    __int64 Ib;
    char Name[64];
  };
  std::mutex g_MeshMutex;
  std::unordered_map<__int64, MeshNote> g_Meshes;

  std::atomic<bool> g_Active{ false };
  std::atomic<unsigned> g_Frame{ 0 };
  ULONGLONG g_StartedAt = 0;            // game thread / render thread (read after Start)
  ULONGLONG g_PassesLoggedAt = 0;       // render thread
  bool g_Hooked = false;                // render thread

  // The passes: by their first target, how many and the depth target
  struct Pass
  {
    __int64 Rt0, Dsv;
    int Targets;
    char Desc[200];
  };
  std::mutex g_PassMutex;
  std::vector<Pass> g_Passes;
  std::atomic<int> g_PassDraws[MaxPasses], g_PassMeshDraws[MaxPasses], g_PassBoneDraws[MaxPasses];
  int g_LastDraws[MaxPasses], g_LastMeshDraws[MaxPasses], g_LastBoneDraws[MaxPasses];   // render thread

  std::mutex g_LogMutex;
  std::set<std::tuple<__int64, __int64, int>> g_Logged;   // vertex shader, pixel shader, pass
  int g_StateLogs = 0;
  // Cloth draws' states: a budget of their own (2026-11-01 19:04 test: the 120
  // lines were all used in the recording's first seconds, Icarus and his
  // jacket came at 24 s), on any context
  const int MaxClothStateLogs = 8;
  int g_ClothStateLogs = 0;
  std::atomic<int> g_ClothShaderLogs{ 0 };
  std::unordered_set<__int64> g_VsNotGpu;   // vertex shaders seen drawing from no buffer the graphics card writes; under g_LogMutex
  const int MaxClothShaderLogs = 16;
  int g_PassStateLogs[MaxPasses] = {};

  // The snapshot
  enum { Snap_None, Snap_Armed, Snap_Copied, Snap_Done };
  struct SnapCopy
  {
    __int64 Source;
    ComPtr<ID3D11Buffer> Staging;
    UINT Width;
    int AtDraw;
    bool Read;
    std::vector<BYTE> Bytes;
  };
  struct SnapDraw
  {
    bool Mesh;
    char Name[64];
    __int64 Vs, Ps, Vb;
    unsigned long long VsHash, PsHash;
    UINT Count, Instances, StartIndex, StartInstance;
    INT Base;
    bool Bones;
    int VectorOffsetsAt;
    int B0Copy;        // in g_SnapCopies, -1
    UINT B0First, B0Num, B0Width;
    __int64 T0;
    UINT T0First, T0Num, T0Width;
    DXGI_FORMAT T0Format;
    UINT T0Dimension;
  };
  std::mutex g_SnapMutex;
  int g_SnapState = Snap_None;
  std::atomic<unsigned> g_SnapFrame{ 0 };
  __int64 g_SnapContext = 0;
  std::vector<SnapDraw> g_SnapDraws;
  std::vector<SnapCopy> g_SnapCopies;
  int g_SnapB2Copy = -1;
  UINT g_SnapB2First = 0;
  size_t g_SnapBytes = 0;
  std::atomic<int> g_SnapStateQuick{ Snap_None };
  std::vector<replay_drawlog::PersonPlace> g_People;   // under g_SnapMutex

  // Text added to a fixed buffer, cut short when it's full (never past its end)
  void Append(char* out, size_t size, size_t& at, char const* format, ...)
  {
    if (at + 1 >= size)
      return;
    va_list args;
    va_start(args, format);
    int n = _vsnprintf_s(out + at, size - at, _TRUNCATE, format, args);
    va_end(args);
    at = n < 0 ? size - 1 : at + (size_t)n;
  }

  // The shader's code read: guarded (the game's own bytes, read once at its making)
  unsigned long long Fnv64(BYTE const* p, size_t n)
  {
    unsigned long long h = 1469598103934665603ull;
    for (size_t i = 0; i < n; ++i)
      h = (h ^ p[i]) * 1099511628211ull;
    return h;
  }

  char const* NameAt(BYTE const* chunk, UINT size, UINT at)
  {
    if (at >= size)
      return "?";
    for (UINT i = at; i < size; ++i)
      if (!chunk[i])
        return (char const*)chunk + at;
    return "?";
  }

  // A vertex shader that reads bone matrices: an imad whose first source is a
  // vertex input and whose second is an immediate 3 in every component (the
  // bone index times its 3 rows; 4 bones a vertex, l(3, 3, 3, 3), or one,
  // l(3): a gun's or drone's parts). Checked offline (2026-10-27 18:0x) over
  // the city's 2637 instanced vertex shaders against fxc's disassembly: 912
  // have that multiply, all found and nothing else (the 4-bone byte test
  // before found 649 and missed the 263 with one bone a vertex).
  bool ReadsBoneIndices(BYTE const* shex, UINT size)
  {
    UINT count = size / 4;
    UINT const* words = (UINT const*)shex;
    for (UINT i = 2; i < count;)
    {
      UINT token = words[i];
      UINT opcode = token & 0x7FF;
      if (opcode == 0x35)   // custom data: its length in the next word
      {
        if (i + 1 >= count)
          return false;
        i += words[i + 1] < 2 ? 2 : words[i + 1];
        continue;
      }
      UINT length = (token >> 24) & 0x7F;
      if (!length || i + length > count)
        return false;
      if (opcode == 0x23)   // imad: dst, src0, src1, src2
      {
        UINT j = i + 1 + ((token >> 31) & 1);
        int operand = 0;
        bool input = false;
        while (j < i + length && operand < 3)
        {
          UINT op = words[j++];
          if ((op >> 31) & 1)
            ++j;   // an extended operand
          UINT type = (op >> 12) & 0xFF;
          if (type == 4)   // an immediate
          {
            UINT n = (op & 3) == 1 ? 1 : 4;
            bool threes = j + n <= i + length;
            for (UINT k = 0; k < n && threes; ++k)
              threes = words[j + k] == 3;
            if (operand == 2 && input && threes)
              return true;
            j += n;
          }
          else
          {
            if (operand == 1)
              input = type == 1;
            UINT dimension = (op >> 20) & 3;
            for (UINT k = 0; k < dimension; ++k)
            {
              UINT representation = (op >> (22 + 3 * k)) & 7;
              j += representation == 1 ? 2 : 1;
            }
          }
          ++operand;
        }
      }
      i += length;
    }
    return false;
  }

  bool ParseUnguarded(BYTE const* code, SIZE_T size, ShaderInfo& info)
  {
    info.Hash = Fnv64(code, size);
    info.Size = (UINT)size;
    info.VectorOffsetsAt = -1;
    info.LmsSlots[0] = info.LmsSlots[1] = -1;
    if (size < 32 || memcmp(code, "DXBC", 4))
      return false;
    UINT total = *(UINT const*)(code + 24), count = *(UINT const*)(code + 28);
    if (total > size || count > 32 || 32 + count * 4 > total)
      return false;
    size_t at = 0;
    for (UINT k = 0; k < count; ++k)
    {
      UINT o = *(UINT const*)(code + 32 + k * 4);
      if (o + 8 > total)
        return false;
      UINT csize = *(UINT const*)(code + o + 4);
      if (o + 8 + csize > total)
        return false;
      BYTE const* c = code + o + 8;
      if (!memcmp(code + o, "RDEF", 4) && csize >= 28)
      {
        UINT ncb = *(UINT const*)(c), cbo = *(UINT const*)(c + 4), nres = *(UINT const*)(c + 8), reso = *(UINT const*)(c + 12);
        UINT target = *(UINT const*)(c + 16);
        UINT varSize = ((target >> 8) & 0xFF) >= 5 ? 40 : 24;
        // The constant buffers' sizes (and where the instance offsets are)
        char const* cbNames[16] = {};
        UINT cbSizes[16] = {};
        for (UINT b = 0; b < ncb && b < 16; ++b)
        {
          if (cbo + (b + 1) * 24 > csize)
            return false;
          BYTE const* cb = c + cbo + b * 24;
          cbNames[b] = NameAt(c, csize, *(UINT const*)cb);
          UINT nvar = *(UINT const*)(cb + 4), varo = *(UINT const*)(cb + 8);
          cbSizes[b] = *(UINT const*)(cb + 12);
          if (!strcmp(cbNames[b], "functionConstants"))
            for (UINT v = 0; v < nvar; ++v)
            {
              if (varo + (v + 1) * varSize > csize)
                return false;
              BYTE const* var = c + varo + v * varSize;
              if (!strcmp(NameAt(c, csize, *(UINT const*)var), "dxVectorBufferOffsets"))
                info.VectorOffsetsAt = (int)*(UINT const*)(var + 4);
            }
        }
        for (UINT r = 0; r < nres; ++r)
        {
          if (reso + (r + 1) * 32 > csize)
            return false;
          BYTE const* res = c + reso + r * 32;
          char const* name = NameAt(c, csize, *(UINT const*)res);
          UINT type = *(UINT const*)(res + 4), point = *(UINT const*)(res + 20);
          char slot = type == 0 ? 'b' : type == 3 ? 's' : (type == 4 || type == 6 || type == 8 || type >= 9) ? 'u' : 't';
          UINT bytes = 0;
          if (type == 0)
            for (UINT b = 0; b < ncb && b < 16; ++b)
              if (cbNames[b] && !strcmp(cbNames[b], name))
                bytes = cbSizes[b];
          if (!strcmp(name, "instanceVectorBuffer"))
            info.VectorBuffer = true;
          // (case aside: the game's names are g_lmsAlbedoTexture and the like)
          char lower[64] = {};
          for (size_t k = 0; k + 1 < sizeof(lower) && name[k]; ++k)
            lower[k] = (char)tolower((unsigned char)name[k]);
          if (slot == 't' && strstr(lower, "lms") && strstr(lower, "texture"))
          {
            if (info.LmsSlots[0] < 0)
              info.LmsSlots[0] = (int)point;
            else if (info.LmsSlots[1] < 0 && info.LmsSlots[0] != (int)point)
              info.LmsSlots[1] = (int)point;
          }
          if (strstr(lower, "cloth"))
            info.Cloth = true;
            if (type == 0)
              Append(info.Names, sizeof(info.Names), at, "%s%c%u %.40s (%u B)", at ? ", " : "", slot, point, name, bytes);
            else
              Append(info.Names, sizeof(info.Names), at, "%s%c%u %.40s", at ? ", " : "", slot, point, name);
        }
      }
      else if (!memcmp(code + o, "SHEX", 4) || !memcmp(code + o, "SHDR", 4))
      {
        if (csize >= 4)
          info.Vertex = (*(UINT const*)c >> 16) == 1;
        info.Bones = ReadsBoneIndices(c, csize);
      }
      else if ((!memcmp(code + o, "OSGN", 4) || !memcmp(code + o, "OSG5", 4) || !memcmp(code + o, "OSG1", 4)) && csize >= 4)
        info.Outputs = (int)*(UINT const*)c;
    }
    return true;
  }

  bool ParseShader(void const* code, SIZE_T size, ShaderInfo& info)
  {
    __try
    {
      return ParseUnguarded((BYTE const*)code, size, info);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
  }

  void Register(__int64 shader, void const* code, SIZE_T size, bool vertex)
  {
    ShaderInfo info{};
    info.Parsed = ParseShader(code, size, info);
    info.Vertex = vertex;
    if (!info.Parsed)
      ++g_ShadersUnread;
    // A shader with a resource named for cloth, as the game makes it (whether
    // there is any; see ShaderInfo::Cloth)
    if (info.Parsed && info.Cloth && g_ClothShaderLogs.fetch_add(1) < MaxClothShaderLogs)
      util::log::Write("Draw log (diagnostic): a %s shader made with a resource named for cloth (0x%I64X, code hash %016llX, %u bytes): %s",
        vertex ? "vertex" : "pixel", shader, info.Hash, info.Size, info.Names);
    ++(vertex ? g_VertexShaders : g_PixelShaders);
    std::lock_guard<std::mutex> lock(g_ShaderMutex);
    if (g_Shaders.size() < MaxShaders || g_Shaders.count(shader))
      g_Shaders[shader] = info;
  }

  typedef HRESULT(STDMETHODCALLTYPE* tCreateVertexShader)(ID3D11Device*, void const*, SIZE_T, ID3D11ClassLinkage*,
    ID3D11VertexShader**);
  typedef HRESULT(STDMETHODCALLTYPE* tCreatePixelShader)(ID3D11Device*, void const*, SIZE_T, ID3D11ClassLinkage*,
    ID3D11PixelShader**);
  tCreateVertexShader oCreateVertexShader = nullptr;
  tCreatePixelShader oCreatePixelShader = nullptr;

  // A crowd shader's code (its names have the crowd system's buffers or its
  // poses) saved to Cinematic Tools\Shaders once a code (2026-11-01: they
  // aren't in the shader databases read offline; their disassembly says how a
  // crowd member's pose is read)
  void SaveCrowdShader(void const* code, SIZE_T size, bool vertex)
  {
    static char const* const names[] = { "indirectionBuffer", "animTexture", "animationBuffer" };
    std::string bytes((char const*)code, size);
    bool crowd = false;
    for (char const* n : names)
      crowd = crowd || bytes.find(n) != std::string::npos;
    if (!crowd)
      return;
    unsigned long long hash = 1469598103934665603ull;
    for (char ch : bytes)
      hash = (hash ^ (unsigned char)ch) * 1099511628211ull;
    static std::mutex mutex;
    static std::set<unsigned long long> saved;
    std::lock_guard<std::mutex> lock(mutex);
    if (saved.size() >= 256 || !saved.insert(hash).second)
      return;
    CreateDirectoryA(".\\Cinematic Tools\\Shaders", nullptr);
    char path[MAX_PATH];
    sprintf_s(path, ".\\Cinematic Tools\\Shaders\\crowd_%s_%016llX.dxbc", vertex ? "vs" : "ps", hash);
    FILE* f = nullptr;
    if (fopen_s(&f, path, "wb") == 0 && f)
    {
      fwrite(code, 1, size, f);
      fclose(f);
      if (saved.size() <= 32)
        util::log::Write("Replay (diagnostic): a crowd %s shader's code saved: %s (%zu bytes)", vertex ? "vertex" : "pixel", path,
          (size_t)size);
    }
  }

  HRESULT STDMETHODCALLTYPE hCreateVertexShader(ID3D11Device* pDevice, void const* code, SIZE_T size, ID3D11ClassLinkage* linkage,
    ID3D11VertexShader** out)
  {
    HRESULT hr = oCreateVertexShader(pDevice, code, size, linkage, out);
    if (SUCCEEDED(hr) && out && *out && code && !g_shutdown)
    {
      SaveCrowdShader(code, size, true);
      Register((__int64)*out, code, size, true);
      // Its code, for a saved recording's parts (see ReplayStore.h)
      replay_store::NoteShader(*out, code, size);
    }
    return hr;
  }

  HRESULT STDMETHODCALLTYPE hCreatePixelShader(ID3D11Device* pDevice, void const* code, SIZE_T size, ID3D11ClassLinkage* linkage,
    ID3D11PixelShader** out)
  {
    HRESULT hr = oCreatePixelShader(pDevice, code, size, linkage, out);
    if (SUCCEEDED(hr) && out && *out && code && !g_shutdown)
    {
      SaveCrowdShader(code, size, false);
      Register((__int64)*out, code, size, false);
      replay_store::NoteShader(*out, code, size);
    }
    return hr;
  }

  // A shader's note (a copy), false if it was made before the hook
  bool ShaderOf(__int64 shader, ShaderInfo& out)
  {
    std::lock_guard<std::mutex> lock(g_ShaderMutex);
    auto it = g_Shaders.find(shader);
    if (it == g_Shaders.end())
      return false;
    out = it->second;
    return true;
  }

  // In the log once: a shader's bindings
  void DescribeShader(__int64 shader)
  {
    ShaderInfo info{};
    {
      std::lock_guard<std::mutex> lock(g_ShaderMutex);
      auto it = g_Shaders.find(shader);
      if (it == g_Shaders.end() || it->second.Described)
        return;
      it->second.Described = true;
      info = it->second;
    }
    util::log::Write("Draw log (diagnostic): shader 0x%I64X: %s, code hash %016llX, %u bytes%s%s%s; its bindings: %s", shader,
      info.Vertex ? "vertex" : "pixel", info.Hash, info.Size, info.VectorBuffer ? ", the instance buffer" : "",
      info.Bones ? ", reads bone matrices (index x 3)" : "", info.Parsed ? "" : " (its code couldn't be read)", info.Names);
    if (!info.Vertex)
      util::log::Write("Draw log (diagnostic): shader 0x%I64X: %d outputs", shader, info.Outputs);
    else if (info.VectorOffsetsAt >= 0)
      util::log::Write("Draw log (diagnostic): shader 0x%I64X: dxVectorBufferOffsets at byte %d of its functionConstants", shader,
        info.VectorOffsetsAt);
  }

  char const* FormatName(DXGI_FORMAT f)
  {
    switch (f)
    {
    case DXGI_FORMAT_UNKNOWN: return "UNKNOWN";
    case DXGI_FORMAT_R32G32B32A32_FLOAT: return "RGBA32F";
    case DXGI_FORMAT_R16G16B16A16_FLOAT: return "RGBA16F";
    case DXGI_FORMAT_R10G10B10A2_UNORM: return "RGB10A2";
    case DXGI_FORMAT_R11G11B10_FLOAT: return "R11G11B10F";
    case DXGI_FORMAT_R8G8B8A8_UNORM: return "RGBA8";
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return "RGBA8_SRGB";
    case DXGI_FORMAT_B8G8R8A8_UNORM: return "BGRA8";
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: return "BGRA8_SRGB";
    case DXGI_FORMAT_R16G16_FLOAT: return "RG16F";
    case DXGI_FORMAT_R32_FLOAT: return "R32F";
    case DXGI_FORMAT_D32_FLOAT: return "D32F";
    case DXGI_FORMAT_D24_UNORM_S8_UINT: return "D24S8";
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT: return "D32FS8";
    case DXGI_FORMAT_D16_UNORM: return "D16";
    case DXGI_FORMAT_R8_UNORM: return "R8";
    case DXGI_FORMAT_R8G8_UNORM: return "RG8";
    case DXGI_FORMAT_BC1_UNORM_SRGB: return "BC1_SRGB";
    case DXGI_FORMAT_BC7_UNORM_SRGB: return "BC7_SRGB";
    default: return nullptr;
    }
  }

  void FormatText(DXGI_FORMAT f, char* out, size_t size)
  {
    char const* name = FormatName(f);
    if (name)
      strcpy_s(out, size, name);
    else
      _snprintf_s(out, size, _TRUNCATE, "fmt%d", (int)f);
  }

  // The pass a draw is in: by its targets (render thread or a game's thread)
  int PassOf(ID3D11DeviceContext* pContext)
  {
    ID3D11RenderTargetView* rtvs[8] = {};
    ID3D11DepthStencilView* dsv = nullptr;
    pContext->OMGetRenderTargets(8, rtvs, &dsv);
    int targets = 0;
    for (int i = 0; i < 8; ++i)
      targets += rtvs[i] != nullptr;
    __int64 rt0 = (__int64)rtvs[0], ds = (__int64)dsv;
    int index = -1;
    {
      std::lock_guard<std::mutex> lock(g_PassMutex);
      for (size_t i = 0; i < g_Passes.size(); ++i)
        if (g_Passes[i].Rt0 == rt0 && g_Passes[i].Dsv == ds && g_Passes[i].Targets == targets)
          index = (int)i;
      if (index < 0 && (int)g_Passes.size() < MaxPasses)
      {
        Pass pass{ rt0, ds, targets, "" };
        size_t at = 0;
        for (int i = 0; i < 8; ++i)
          if (rtvs[i])
          {
            D3D11_RENDER_TARGET_VIEW_DESC desc{};
            rtvs[i]->GetDesc(&desc);
            char f[24];
            FormatText(desc.Format, f, sizeof(f));
            Append(pass.Desc, sizeof(pass.Desc), at, "%s%s", at ? " " : "", f);
          }
        if (dsv)
        {
          D3D11_DEPTH_STENCIL_VIEW_DESC desc{};
          dsv->GetDesc(&desc);
          char f[24];
          FormatText(desc.Format, f, sizeof(f));
          ComPtr<ID3D11Resource> res;
          dsv->GetResource(res.GetAddressOf());
          ComPtr<ID3D11Texture2D> tex;
          D3D11_TEXTURE2D_DESC td{};
          if (res && SUCCEEDED(res.As(&tex)))
            tex->GetDesc(&td);
          Append(pass.Desc, sizeof(pass.Desc), at, "%sdepth %s %ux%u", at ? ", " : "", f, td.Width, td.Height);
        }
        g_Passes.push_back(pass);
        index = (int)g_Passes.size() - 1;
      }
    }
    for (int i = 0; i < 8; ++i)
      if (rtvs[i])
        rtvs[i]->Release();
    if (dsv)
      dsv->Release();
    return index;
  }

  // A buffer's description, short
  void BufferText(ID3D11Buffer* buffer, char* out, size_t size)
  {
    if (!buffer)
    {
      strcpy_s(out, size, "none");
      return;
    }
    D3D11_BUFFER_DESC d{};
    buffer->GetDesc(&d);
    _snprintf_s(out, size, _TRUNCATE, "0x%I64X %u B usage %d cpu 0x%X bind 0x%X misc 0x%X stride %u", (__int64)buffer, d.ByteWidth, (int)d.Usage,
      d.CPUAccessFlags, d.BindFlags, d.MiscFlags, d.StructureByteStride);
  }

  // A stage's constant buffers 0 - 3 (with their 11.1 offsets where the context has them)
  void ConstantsText(ID3D11DeviceContext* pContext, bool vertex, char* out, size_t size)
  {
    ID3D11Buffer* buffers[4] = {};
    UINT first[4] = {}, num[4] = {};
    bool offsets = false;
    ComPtr<ID3D11DeviceContext1> one;
    if (SUCCEEDED(pContext->QueryInterface(__uuidof(ID3D11DeviceContext1), (void**)one.GetAddressOf())) && one)
    {
      offsets = true;
      if (vertex)
        one->VSGetConstantBuffers1(0, 4, buffers, first, num);
      else
        one->PSGetConstantBuffers1(0, 4, buffers, first, num);
    }
    else if (vertex)
      pContext->VSGetConstantBuffers(0, 4, buffers);
    else
      pContext->PSGetConstantBuffers(0, 4, buffers);
    size_t at = 0;
    for (int i = 0; i < 4; ++i)
    {
      char b[160];
      BufferText(buffers[i], b, sizeof(b));
      if (offsets && buffers[i])
        Append(out, size, at, "%sb%d %s, first %u num %u", i ? "; " : "", i, b, first[i], num[i]);
      else
        Append(out, size, at, "%sb%d %s", i ? "; " : "", i, b);
      if (buffers[i])
        buffers[i]->Release();
    }
    if (!offsets)
      Append(out, size, at, " (no 11.1 context)");
  }

  // A draw's state, in the log (once for its shaders and pass)
  // A draw whose vertices come from a buffer the graphics card writes (one
  // bindable as an unordered access view or a stream output: a vertex buffer
  // or a buffer view of its vertex shader's). 2026-11-02: no shader of the game's
  // has a resource named for cloth (the 15:53 test), and EACloth's compute
  // shaders (EAClothComputeSimTransformCS, EAClothWrappingCS) write the cloth's
  // render buffers, so Icarus's jacket is one of these. Which of them: its
  // own budget of logs (MaxClothStateLogs), with what's bound.
  bool FromGpuBuffer(ID3D11DeviceContext* pContext, char* what, size_t size)
  {
    const UINT GpuWritten = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_STREAM_OUTPUT;
    bool found = false;
    what[0] = 0;
    ID3D11Buffer* vbs[4] = {};
    UINT strides[4] = {}, offsets[4] = {};
    pContext->IAGetVertexBuffers(0, 4, vbs, strides, offsets);
    for (int i = 0; i < 4; ++i)
    {
      if (!vbs[i])
        continue;
      D3D11_BUFFER_DESC d{};
      vbs[i]->GetDesc(&d);
      if (d.BindFlags & GpuWritten)
      {
        found = true;
        size_t at = strlen(what);
        _snprintf_s(what + at, size - at, _TRUNCATE, "%svb%d (%u bytes, stride %u, bind 0x%X, misc 0x%X)", at ? ", " : "", i, d.ByteWidth,
          strides[i], d.BindFlags, d.MiscFlags);
      }
      vbs[i]->Release();
    }
    ID3D11ShaderResourceView* views[16] = {};
    pContext->VSGetShaderResources(0, 16, views);
    for (int i = 0; i < 16; ++i)
    {
      if (!views[i])
        continue;
      ID3D11Resource* resource = nullptr;
      views[i]->GetResource(&resource);
      ID3D11Buffer* buffer = nullptr;
      if (resource && SUCCEEDED(resource->QueryInterface(__uuidof(ID3D11Buffer), (void**)&buffer)) && buffer)
      {
        D3D11_BUFFER_DESC d{};
        buffer->GetDesc(&d);
        if (d.BindFlags & GpuWritten)
        {
          found = true;
          size_t at = strlen(what);
          _snprintf_s(what + at, size - at, _TRUNCATE, "%st%d (%u bytes, structure %u, bind 0x%X, misc 0x%X)", at ? ", " : "", i,
            d.ByteWidth, d.StructureByteStride, d.BindFlags, d.MiscFlags);
        }
        buffer->Release();
      }
      if (resource)
        resource->Release();
      views[i]->Release();
    }
    return found;
  }

  void LogState(ID3D11DeviceContext* pContext, char const* what, int pass, __int64 vs, __int64 ps, UINT indexCount, UINT instances,
    UINT startIndex, INT baseVertex, UINT startInstance)
  {
    DescribeShader(vs);
    DescribeShader(ps);
    char passText[200] = "?";
    {
      std::lock_guard<std::mutex> lock(g_PassMutex);
      if (pass >= 0 && pass < (int)g_Passes.size())
        _snprintf_s(passText, _TRUNCATE, "#%d: %d targets %s", pass, g_Passes[pass].Targets, g_Passes[pass].Desc);
    }
    util::log::Write("Draw log (diagnostic): %s: pass %s; %s context; VS 0x%I64X PS 0x%I64X; %u indices x %u instances (first %u, "
      "base %d, first instance %u)", what, passText, pContext->GetType() == D3D11_DEVICE_CONTEXT_DEFERRED ? "deferred" : "immediate",
      vs, ps, indexCount, instances, startIndex, baseVertex, startInstance);
    char text[900];
    ConstantsText(pContext, true, text, sizeof(text));
    util::log::Write("Draw log (diagnostic):   VS constants: %s", text);
    ConstantsText(pContext, false, text, sizeof(text));
    util::log::Write("Draw log (diagnostic):   PS constants: %s", text);
    // The vertex shader's views 0 - 3 (t0: the instance buffer)
    {
      ID3D11ShaderResourceView* views[4] = {};
      pContext->VSGetShaderResources(0, 4, views);
      size_t at = 0;
      text[0] = 0;
      for (int i = 0; i < 4; ++i)
        if (views[i])
        {
          D3D11_SHADER_RESOURCE_VIEW_DESC d{};
          views[i]->GetDesc(&d);
          ComPtr<ID3D11Resource> res;
          views[i]->GetResource(res.GetAddressOf());
          ComPtr<ID3D11Buffer> buffer;
          char b[160] = "not a buffer";
          if (res && SUCCEEDED(res.As(&buffer)))
            BufferText(buffer.Get(), b, sizeof(b));
          UINT firstElement = d.ViewDimension == D3D11_SRV_DIMENSION_BUFFEREX ? d.BufferEx.FirstElement : d.Buffer.FirstElement;
          UINT elements = d.ViewDimension == D3D11_SRV_DIMENSION_BUFFEREX ? d.BufferEx.NumElements : d.Buffer.NumElements;
          char f[24];
          FormatText(d.Format, f, sizeof(f));
          Append(text, sizeof(text), at, "%st%d view dim %d format %s first element %u elements %u, of %s",
            at ? "; " : "", i, (int)d.ViewDimension, f, firstElement, elements, b);
          views[i]->Release();
        }
      util::log::Write("Draw log (diagnostic):   VS views: %s", at ? text : "none");
    }
    // The pixel shader's views (their names: its shader line)
    {
      ID3D11ShaderResourceView* views[16] = {};
      pContext->PSGetShaderResources(0, 16, views);
      size_t at = 0;
      for (int i = 0; i < 16; ++i)
        if (views[i])
        {
          D3D11_SHADER_RESOURCE_VIEW_DESC d{};
          views[i]->GetDesc(&d);
          char f[24];
          FormatText(d.Format, f, sizeof(f));
          if (at + 40 < sizeof(text))
            Append(text, sizeof(text), at, "%st%d %s dim %d", at ? ", " : "", i, f, (int)d.ViewDimension);
          views[i]->Release();
        }
      util::log::Write("Draw log (diagnostic):   PS views: %s", at ? text : "none");
    }
    // Input, depth test, blending, culling, viewport
    {
      ID3D11Buffer* vbs[2] = {};
      UINT strides[2] = {}, offsets[2] = {};
      pContext->IAGetVertexBuffers(0, 2, vbs, strides, offsets);
      ID3D11Buffer* ib = nullptr;
      DXGI_FORMAT ibFormat = DXGI_FORMAT_UNKNOWN;
      UINT ibOffset = 0;
      pContext->IAGetIndexBuffer(&ib, &ibFormat, &ibOffset);
      D3D11_PRIMITIVE_TOPOLOGY topology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
      pContext->IAGetPrimitiveTopology(&topology);
      ID3D11InputLayout* layout = nullptr;
      pContext->IAGetInputLayout(&layout);
      char vb0[160], vb1[160], ibText[160];
      BufferText(vbs[0], vb0, sizeof(vb0));
      BufferText(vbs[1], vb1, sizeof(vb1));
      BufferText(ib, ibText, sizeof(ibText));
      util::log::Write("Draw log (diagnostic):   input: layout 0x%I64X, topology %d; vb0 %s (stride %u offset %u); vb1 %s (stride %u); "
        "ib %s (format %d offset %u)", (__int64)layout, (int)topology, vb0, strides[0], offsets[0], vb1, strides[1], ibText,
        (int)ibFormat, ibOffset);
      for (ID3D11Buffer* b : vbs)
        if (b)
          b->Release();
      if (ib)
        ib->Release();
      if (layout)
        layout->Release();
    }
    {
      ID3D11DepthStencilState* dss = nullptr;
      UINT ref = 0;
      pContext->OMGetDepthStencilState(&dss, &ref);
      D3D11_DEPTH_STENCIL_DESC ds{};
      if (dss)
        dss->GetDesc(&ds);
      ID3D11BlendState* bs = nullptr;
      FLOAT factor[4] = {};
      UINT mask = 0;
      pContext->OMGetBlendState(&bs, factor, &mask);
      D3D11_BLEND_DESC bd{};
      if (bs)
        bs->GetDesc(&bd);
      ID3D11RasterizerState* rs = nullptr;
      pContext->RSGetState(&rs);
      D3D11_RASTERIZER_DESC rd{};
      if (rs)
        rs->GetDesc(&rd);
      D3D11_VIEWPORT vp{};
      UINT nvp = 1;
      pContext->RSGetViewports(&nvp, &vp);
      util::log::Write("Draw log (diagnostic):   depth %s: test %d write %d func %d, stencil %d (read 0x%X write 0x%X, front func %d pass "
        "op %d, ref %u); blend %s: rt0 on %d, write masks %X %X %X %X, alpha to coverage %d; raster %s: cull %d fill %d bias %d "
        "scissor %d; viewport %.0fx%.0f at %.0f,%.0f", dss ? "state" : "default", ds.DepthEnable, (int)ds.DepthWriteMask,
        (int)ds.DepthFunc, ds.StencilEnable, ds.StencilReadMask, ds.StencilWriteMask, (int)ds.FrontFace.StencilFunc,
        (int)ds.FrontFace.StencilPassOp, ref, bs ? "state" : "default", bd.RenderTarget[0].BlendEnable,
        bd.RenderTarget[0].RenderTargetWriteMask, bd.RenderTarget[1].RenderTargetWriteMask, bd.RenderTarget[2].RenderTargetWriteMask,
        bd.RenderTarget[3].RenderTargetWriteMask, bd.AlphaToCoverageEnable, rs ? "state" : "default", (int)rd.CullMode,
        (int)rd.FillMode, rd.DepthBias, rd.ScissorEnable, vp.Width, vp.Height, vp.TopLeftX, vp.TopLeftY);
      if (dss)
        dss->Release();
      if (bs)
        bs->Release();
      if (rs)
        rs->Release();
    }
  }

  // The device that made a context's buffers (for the snapshot's own)
  int CopyForSnapshot(ID3D11DeviceContext* pContext, ID3D11Buffer* source, int atDraw, UINT limit)
  {
    D3D11_BUFFER_DESC d{};
    source->GetDesc(&d);
    if (d.ByteWidth > limit || g_SnapBytes + d.ByteWidth > MaxSnapshotBytes)
      return -1;
    ComPtr<ID3D11Device> device;
    pContext->GetDevice(device.GetAddressOf());
    D3D11_BUFFER_DESC s{};
    s.ByteWidth = d.ByteWidth;
    s.Usage = D3D11_USAGE_STAGING;
    s.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    s.MiscFlags = d.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    s.StructureByteStride = s.MiscFlags ? d.StructureByteStride : 0;
    SnapCopy copy{ (__int64)source, nullptr, d.ByteWidth, atDraw, false, {} };
    if (!device || FAILED(device->CreateBuffer(&s, nullptr, copy.Staging.GetAddressOf())) || !copy.Staging)
      return -1;
    pContext->CopyResource(copy.Staging.Get(), source);
    g_SnapBytes += d.ByteWidth;
    g_SnapCopies.push_back(std::move(copy));
    return (int)g_SnapCopies.size() - 1;
  }

  // The snapshot: this draw's constants and instance data copied (its context's thread)
  void AddToSnapshot(ID3D11DeviceContext* pContext, char const* name, bool mesh, __int64 vs, __int64 ps, __int64 vb,
    ShaderInfo const& vsInfo, unsigned long long psHash, UINT indexCount, UINT instances, UINT startIndex, INT baseVertex,
    UINT startInstance)
  {
    std::lock_guard<std::mutex> lock(g_SnapMutex);
    if (g_SnapState != Snap_Armed || (int)g_SnapDraws.size() >= MaxSnapshotDraws)
      return;
    if (!g_SnapContext)
      g_SnapContext = (__int64)pContext;
    else if (g_SnapContext != (__int64)pContext)
      return;
    int index = (int)g_SnapDraws.size();
    SnapDraw s{};
    s.Mesh = mesh;
    strncpy_s(s.Name, name, _TRUNCATE);
    s.Vs = vs;
    s.Ps = ps;
    s.Vb = vb;
    s.VsHash = vsInfo.Hash;
    s.PsHash = psHash;
    s.Count = indexCount;
    s.Instances = instances;
    s.StartIndex = startIndex;
    s.Base = baseVertex;
    s.StartInstance = startInstance;
    s.Bones = vsInfo.Bones;
    s.VectorOffsetsAt = vsInfo.VectorOffsetsAt;
    s.B0Copy = -1;
    ID3D11Buffer* buffers[3] = {};
    UINT first[3] = {}, num[3] = {};
    ComPtr<ID3D11DeviceContext1> one;
    if (SUCCEEDED(pContext->QueryInterface(__uuidof(ID3D11DeviceContext1), (void**)one.GetAddressOf())) && one)
      one->VSGetConstantBuffers1(0, 3, buffers, first, num);
    else
      pContext->VSGetConstantBuffers(0, 3, buffers);
    if (buffers[0])
    {
      D3D11_BUFFER_DESC d{};
      buffers[0]->GetDesc(&d);
      s.B0Width = d.ByteWidth;
      s.B0First = first[0];
      s.B0Num = num[0] ? num[0] : d.ByteWidth / 16;
      s.B0Copy = CopyForSnapshot(pContext, buffers[0], index, MaxConstantCopyBytes);
    }
    if (buffers[2] && g_SnapB2Copy < 0)
    {
      g_SnapB2First = first[2];
      g_SnapB2Copy = CopyForSnapshot(pContext, buffers[2], index, MaxConstantCopyBytes);
    }
    for (ID3D11Buffer* b : buffers)
      if (b)
        b->Release();
    ID3D11ShaderResourceView* view = nullptr;
    pContext->VSGetShaderResources(0, 1, &view);
    if (view)
    {
      D3D11_SHADER_RESOURCE_VIEW_DESC d{};
      view->GetDesc(&d);
      s.T0Format = d.Format;
      s.T0Dimension = (UINT)d.ViewDimension;
      s.T0First = d.ViewDimension == D3D11_SRV_DIMENSION_BUFFEREX ? d.BufferEx.FirstElement : d.Buffer.FirstElement;
      s.T0Num = d.ViewDimension == D3D11_SRV_DIMENSION_BUFFEREX ? d.BufferEx.NumElements : d.Buffer.NumElements;
      ComPtr<ID3D11Resource> res;
      view->GetResource(res.GetAddressOf());
      ComPtr<ID3D11Buffer> buffer;
      if (res && SUCCEEDED(res.As(&buffer)) && buffer)
      {
        D3D11_BUFFER_DESC bd{};
        buffer->GetDesc(&bd);
        s.T0 = (__int64)buffer.Get();
        s.T0Width = bd.ByteWidth;
        // Copied at each draw if it's small, else at the 1st, 8th, 16th and 24th
        bool copy = bd.ByteWidth <= EveryDrawCopyBytes || index % 8 == 7 || index == 0;
        if (copy)
          CopyForSnapshot(pContext, buffer.Get(), index, MaxInstanceCopyBytes);
      }
      view->Release();
    }
    g_SnapDraws.push_back(s);
  }

  // Render thread: the snapshot's copies read back and what they hold logged
  void LogSnapshot()
  {
    std::vector<SnapDraw> draws;
    std::vector<SnapCopy> copies;
    std::vector<replay_drawlog::PersonPlace> people;
    int b2 = -1;
    UINT b2First = 0;
    {
      std::lock_guard<std::mutex> lock(g_SnapMutex);
      draws = g_SnapDraws;
      copies.swap(g_SnapCopies);
      people = g_People;
      b2 = g_SnapB2Copy;
      b2First = g_SnapB2First;
    }
    auto readFloat4 = [](std::vector<BYTE> const& bytes, size_t at, float* out) {
      if (at + 16 > bytes.size())
        return false;
      memcpy(out, bytes.data() + at, 16);
      return true;
    };
    util::log::Write("Draw log (diagnostic): snapshot of %d draws (context 0x%I64X), %d copies read back", (int)draws.size(),
      g_SnapContext, (int)copies.size());
    float camera[4] = {};
    if (b2 >= 0 && b2 < (int)copies.size() && copies[b2].Read && readFloat4(copies[b2].Bytes, b2First * 16 + CameraPosOffset, camera))
      util::log::Write("Draw log (diagnostic):   viewConstants' cameraPos %.2f %.2f %.2f (b2 first constant %u)", camera[0],
        camera[1], camera[2], b2First);
    for (size_t i = 0; i < draws.size(); ++i)
    {
      SnapDraw const& s = draws[i];
      char b0Text[220] = "not copied";
      int start = -1, stride = -1;
      if (s.B0Copy >= 0 && s.B0Copy < (int)copies.size() && copies[s.B0Copy].Read)
      {
        std::vector<BYTE> const& bytes = copies[s.B0Copy].Bytes;
        size_t base = (size_t)s.B0First * 16;
        size_t at = 0;
        for (int k = 0; k < 12 && base + k * 4 + 4 <= bytes.size(); ++k)
          Append(b0Text, sizeof(b0Text), at, "%s%08X", k ? " " : "", *(UINT const*)(bytes.data() + base + k * 4));
        if (s.VectorOffsetsAt >= 0 && base + s.VectorOffsetsAt + 8 <= bytes.size())
        {
          start = *(int const*)(bytes.data() + base + s.VectorOffsetsAt);
          stride = *(int const*)(bytes.data() + base + s.VectorOffsetsAt + 4);
        }
      }
      util::log::Write("Draw log (diagnostic):   #%d %s '%s' VS %016llX PS %016llX vb 0x%I64X, %u indices x %u: b0 (first %u num %u of "
        "%u B): %s; instances start %d stride %d; t0 0x%I64X (%u B, view dim %u format %d, first element %u of %u)", (int)i,
        s.Mesh ? "model" : "bones", s.Name, s.VsHash, s.PsHash, s.Vb, s.Count, s.Instances, s.B0First, s.B0Num, s.B0Width, b0Text,
        start, stride, s.T0, s.T0Width, s.T0Dimension, (int)s.T0Format, s.T0First, s.T0Num);
      if (start < 0 || !s.T0)
        continue;
      // The copy of its instance buffer made at it or the next after (or the last before)
      int use = -1;
      bool before = false;
      for (size_t c = 0; c < copies.size(); ++c)
        if (copies[c].Source == s.T0 && copies[c].Read && copies[c].AtDraw >= (int)i &&
            (use < 0 || copies[c].AtDraw < copies[use].AtDraw))
          use = (int)c;
      if (use < 0)
        for (size_t c = 0; c < copies.size(); ++c)
          if (copies[c].Source == s.T0 && copies[c].Read && (use < 0 || copies[c].AtDraw > copies[use].AtDraw))
          {
            use = (int)c;
            before = true;
          }
      if (use < 0)
      {
        util::log::Write("Draw log (diagnostic):     its instance buffer wasn't copied (too big, or the snapshot's memory used)");
        continue;
      }
      if (s.T0Format != DXGI_FORMAT_R32G32B32A32_FLOAT)
      {
        util::log::Write("Draw log (diagnostic):     its instance view isn't float4s (format %d): not read", (int)s.T0Format);
        continue;
      }
      std::vector<BYTE> const& bytes = copies[use].Bytes;
      size_t at = ((size_t)s.T0First + (size_t)start) * 16;
      float r[3][4] = {};
      bool rows = readFloat4(bytes, at, r[0]) && readFloat4(bytes, at + 16, r[1]) && readFloat4(bytes, at + 32, r[2]);
      float r3[4] = {};
      readFloat4(bytes, at + 48, r3);
      util::log::Write("Draw log (diagnostic):     its instance 0 (copy at draw %d%s): rows 0-3 %s(%.3f %.3f %.3f %.2f) (%.3f %.3f %.3f %.2f) "
        "(%.3f %.3f %.3f %.2f) (%.3f %.3f %.3f %.3f)", copies[use].AtDraw, before ? ", before it" : "", rows ? "" : "past the copy ",
        r[0][0], r[0][1], r[0][2], r[0][3], r[1][0], r[1][1], r[1][2], r[1][3], r[2][0], r[2][1], r[2][2], r[2][3], r3[0], r3[1], r3[2],
        r3[3]);
      if (!s.Bones)
        continue;
      // Bones 0 - 3: their places (each 3 rows' w), and the recorded person nearest bone 0
      char text[400];
      size_t t = 0;
      float bone0[3] = {};
      bool have0 = false;
      for (int b = 0; b < 4; ++b)
      {
        float m[3][4] = {};
        size_t bat = at + (size_t)(BoneRowsStart + b * 3) * 16;
        if (!readFloat4(bytes, bat, m[0]) || !readFloat4(bytes, bat + 16, m[1]) || !readFloat4(bytes, bat + 32, m[2]))
          break;
        Append(text, sizeof(text), t, "%s%.2f %.2f %.2f", b ? " / " : "", m[0][3], m[1][3], m[2][3]);
        if (!b)
        {
          bone0[0] = m[0][3];
          bone0[1] = m[1][3];
          bone0[2] = m[2][3];
          have0 = true;
        }
      }
      char nearest[160] = "no recorded person noted";
      if (have0)
      {
        float best = 1e30f;
        for (auto const& p : people)
        {
          float dx = p.Place[0] - bone0[0], dy = p.Place[1] - bone0[1], dz = p.Place[2] - bone0[2];
          float d2 = dx * dx + dy * dy + dz * dz;
          if (d2 < best)
          {
            best = d2;
            _snprintf_s(nearest, _TRUNCATE, "nearest recorded person: %s 0x%I64X at %.2f m (at %.1f %.1f %.1f)", p.Label, p.Entity, sqrtf(d2),
              p.Place[0], p.Place[1], p.Place[2]);
          }
        }
      }
      util::log::Write("Draw log (diagnostic):     bones 0 - 3 at %s; %s", t ? text : "past the copy", nearest);
    }
  }

  void ReadBack(ID3D11DeviceContext* pContext, unsigned frame)
  {
    bool all = true;
    {
      std::lock_guard<std::mutex> lock(g_SnapMutex);
      for (SnapCopy& c : g_SnapCopies)
      {
        if (c.Read || !c.Staging)
          continue;
        D3D11_MAPPED_SUBRESOURCE m{};
        HRESULT hr = pContext->Map(c.Staging.Get(), 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &m);
        if (hr == DXGI_ERROR_WAS_STILL_DRAWING)
        {
          all = false;
          continue;
        }
        if (SUCCEEDED(hr) && m.pData)
        {
          try
          {
            c.Bytes.assign((BYTE const*)m.pData, (BYTE const*)m.pData + c.Width);
            c.Read = true;
          }
          catch (std::bad_alloc const&)
          {
          }
          pContext->Unmap(c.Staging.Get(), 0);
        }
        c.Staging.Reset();
      }
    }
    if (!all && frame - g_SnapFrame < ReadbackGiveUpFrames)
      return;
    LogSnapshot();
    std::lock_guard<std::mutex> lock(g_SnapMutex);
    g_SnapCopies.clear();
    g_SnapDraws.clear();
    g_People.clear();
    g_SnapBytes = 0;
    g_SnapState = Snap_Done;
    g_SnapStateQuick = Snap_Done;
  }

  void LogPasses()
  {
    std::lock_guard<std::mutex> lock(g_PassMutex);
    util::log::Write("Draw log (diagnostic): passes in the last sampled frame (indexed draws, of them the recorded models' and "
      "bone-reading ones):");
    for (size_t i = 0; i < g_Passes.size(); ++i)
      if (g_LastDraws[i])
        util::log::Write("Draw log (diagnostic):   #%d: %d targets %s: %d draws, %d recorded models', %d bone-reading", (int)i,
          g_Passes[i].Targets, g_Passes[i].Desc, g_LastDraws[i], g_LastMeshDraws[i], g_LastBoneDraws[i]);
  }
}

void replay_drawlog::Present(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
  if (!pDevice || !pContext || g_shutdown)
    return;
  replay_gamedraws::CostTimer timer(replay_gamedraws::Cost_DrawLogPresent);
  if (!g_Hooked)
  {
    g_Hooked = true;
    void** table = *(void***)pDevice;
    bool vs = util::hooks::CreateGameHook("DrawLogCreateVertexShader", (__int64)table[VertexShaderSlot], (PVOID)hCreateVertexShader,
      (LPVOID*)&oCreateVertexShader);
    bool ps = util::hooks::CreateGameHook("DrawLogCreatePixelShader", (__int64)table[PixelShaderSlot], (PVOID)hCreatePixelShader,
      (LPVOID*)&oCreatePixelShader);
    util::log::Write("Draw log (diagnostic): the game's shaders are noted as it makes them (%s, %s)",
      vs ? "CreateVertexShader" : "CreateVertexShader NOT hooked", ps ? "CreatePixelShader" : "CreatePixelShader NOT hooked");
  }
  unsigned frame = g_Frame.fetch_add(1) + 1;
  // The sampled frame before this one: its counts kept for the pass list
  if (frame % SampleEvery == 1)
    for (int i = 0; i < MaxPasses; ++i)
    {
      g_LastDraws[i] = g_PassDraws[i].exchange(0);
      g_LastMeshDraws[i] = g_PassMeshDraws[i].exchange(0);
      g_LastBoneDraws[i] = g_PassBoneDraws[i].exchange(0);
    }
  ULONGLONG now = GetTickCount64();
  if (g_Active && now - g_PassesLoggedAt >= PassLogEveryMs && frame % SampleEvery == 2)
  {
    g_PassesLoggedAt = now;
    LogPasses();
  }
  // The snapshot: in a sampled frame (its draws are looked at then anyway)
  int state = g_SnapStateQuick.load();
  if (state == Snap_None && g_Active && now - g_StartedAt >= SnapshotAfterMs && frame % SampleEvery == 0)
  {
    std::lock_guard<std::mutex> lock(g_SnapMutex);
    g_SnapState = Snap_Armed;
    g_SnapFrame = frame;
    g_SnapContext = 0;
    g_SnapStateQuick = Snap_Armed;
  }
  else if (state == Snap_Armed && frame != g_SnapFrame)
  {
    std::lock_guard<std::mutex> lock(g_SnapMutex);
    g_SnapState = Snap_Copied;
    g_SnapStateQuick = Snap_Copied;
    if (g_SnapDraws.empty())
    {
      // Nothing to copy in that frame: the next sampled one
      g_SnapState = Snap_None;
      g_SnapStateQuick = Snap_None;
      g_SnapCopies.clear();
      g_SnapB2Copy = -1;
      g_SnapBytes = 0;
    }
  }
  else if (state == Snap_Copied && frame - g_SnapFrame >= ReadbackAfterFrames)
    ReadBack(pContext, frame);
}

void replay_drawlog::Start()
{
  {
    std::lock_guard<std::mutex> lock(g_MeshMutex);
    g_Meshes.clear();
  }
  {
    std::lock_guard<std::mutex> lock(g_LogMutex);
    g_Logged.clear();
    g_StateLogs = 0;
    g_ClothStateLogs = 0;
    g_VsNotGpu.clear();
    memset(g_PassStateLogs, 0, sizeof(g_PassStateLogs));
  }
  {
    std::lock_guard<std::mutex> lock(g_SnapMutex);
    g_SnapState = Snap_None;
    g_SnapStateQuick = Snap_None;
    g_SnapDraws.clear();
    g_SnapCopies.clear();
    g_SnapB2Copy = -1;
    g_SnapBytes = 0;
    g_People.clear();
  }
  g_StartedAt = GetTickCount64();
  g_PassesLoggedAt = g_StartedAt;
  g_Active = true;
  size_t known = 0;
  {
    std::lock_guard<std::mutex> lock(g_ShaderMutex);
    known = g_Shaders.size();
  }
  util::log::Write("Draw log (diagnostic): on while the replay records; %d vertex and %d pixel shaders noted so far (%d of them "
    "unread), %d kept", g_VertexShaders.load(), g_PixelShaders.load(), g_ShadersUnread.load(), (int)known);
}

void replay_drawlog::Stop()
{
  if (!g_Active.exchange(false))
    return;
  int logs = 0;
  {
    std::lock_guard<std::mutex> lock(g_LogMutex);
    logs = g_StateLogs;
  }
  {
    std::lock_guard<std::mutex> lock(g_SnapMutex);
    if (g_SnapState == Snap_Armed)
    {
      g_SnapState = Snap_None;
      g_SnapStateQuick = Snap_None;
      g_SnapDraws.clear();
      g_SnapCopies.clear();
      g_SnapBytes = 0;
      util::log::Write("Draw log (diagnostic): the recording stopped before the snapshot found a draw to copy");
    }
  }
  util::log::Write("Draw log (diagnostic): off; %d draws' states logged", logs);
}

void replay_drawlog::NoteMesh(__int64 vb, __int64 ib, char const* name)
{
  if (!vb)
    return;
  MeshNote note{ ib, "" };
  strncpy_s(note.Name, name ? name : "?", _TRUNCATE);
  std::lock_guard<std::mutex> lock(g_MeshMutex);
  g_Meshes[vb] = note;
}

bool replay_drawlog::WantsPeople()
{
  int state = g_SnapStateQuick.load(std::memory_order_relaxed);
  return g_Active.load(std::memory_order_relaxed) && (state == Snap_Armed || state == Snap_Copied);
}

void replay_drawlog::NotePeople(std::vector<PersonPlace> const& people)
{
  std::lock_guard<std::mutex> lock(g_SnapMutex);
  if (g_SnapState == Snap_Armed || g_SnapState == Snap_Copied)
    g_People = people;
}

bool replay_drawlog::VectorOffsetsOf(__int64 shader, int& at)
{
  std::lock_guard<std::mutex> lock(g_ShaderMutex);
  auto it = g_Shaders.find(shader);
  if (it == g_Shaders.end() || !it->second.Parsed)
    return false;
  at = it->second.VectorOffsetsAt;
  return true;
}

bool replay_drawlog::ShaderNames(__int64 shader, char* out, size_t size)
{
  std::lock_guard<std::mutex> lock(g_ShaderMutex);
  auto it = g_Shaders.find(shader);
  if (it == g_Shaders.end() || !it->second.Parsed)
    return false;
  strncpy_s(out, size, it->second.Names, _TRUNCATE);
  return true;
}

int replay_drawlog::LmsSlots(__int64 shader, int* slots, int max)
{
  std::lock_guard<std::mutex> lock(g_ShaderMutex);
  auto it = g_Shaders.find(shader);
  if (it == g_Shaders.end() || !it->second.Parsed)
    return 0;
  int count = 0;
  for (int k = 0; k < 2 && count < max; ++k)
    if (it->second.LmsSlots[k] >= 0)
      slots[count++] = it->second.LmsSlots[k];
  return count;
}

bool replay_drawlog::ReadsBones(__int64 shader)
{
  std::lock_guard<std::mutex> lock(g_ShaderMutex);
  auto it = g_Shaders.find(shader);
  return it != g_Shaders.end() && it->second.Parsed && it->second.Vertex && it->second.Bones;
}

bool replay_drawlog::Active()
{
  return g_Active.load(std::memory_order_relaxed);
}

void replay_drawlog::OnDraw(ID3D11DeviceContext* pContext, UINT indexCount, UINT instances, UINT startIndex, INT baseVertex,
  UINT startInstance)
{
  unsigned frame = g_Frame.load(std::memory_order_relaxed);
  bool snapshot = g_SnapStateQuick.load(std::memory_order_relaxed) == Snap_Armed && frame == g_SnapFrame;
  if (frame % SampleEvery && !snapshot)
    return;
  replay_gamedraws::CostTimer timer(replay_gamedraws::Cost_DrawLogDraws);
  int pass = PassOf(pContext);
  // Its vertex shader and its first vertex buffer
  ID3D11VertexShader* vs = nullptr;
  pContext->VSGetShader(&vs, nullptr, nullptr);
  ID3D11PixelShader* ps = nullptr;
  pContext->PSGetShader(&ps, nullptr, nullptr);
  ID3D11Buffer* vb = nullptr;
  UINT stride = 0, offset = 0;
  pContext->IAGetVertexBuffers(0, 1, &vb, &stride, &offset);
  __int64 vsAt = (__int64)vs, psAt = (__int64)ps, vbAt = (__int64)vb;
  if (vs)
    vs->Release();
  if (ps)
    ps->Release();
  if (vb)
    vb->Release();
  ShaderInfo vsInfo{};
  bool known = vsAt && ShaderOf(vsAt, vsInfo);
  MeshNote mesh{};
  bool isMesh = false;
  if (vbAt)
  {
    std::lock_guard<std::mutex> lock(g_MeshMutex);
    auto it = g_Meshes.find(vbAt);
    if (it != g_Meshes.end())
    {
      mesh = it->second;
      isMesh = true;
    }
  }
  int targets = 0;
  if (pass >= 0)
  {
    std::lock_guard<std::mutex> lock(g_PassMutex);
    targets = g_Passes[pass].Targets;
  }
  bool bones = known && vsInfo.Bones && targets > 0;
  // A cloth mesh's (a resource of its vertex shader named for cloth; 2026-11-01,
  // the user: "Icarus still has no hoodie": his jacket is cloth, drawn from
  // buffers of the cloth's own, not the game's mesh table, and its vertex
  // shader doesn't read bones, so nothing of it was kept)
  bool cloth = known && vsInfo.Cloth && !isMesh && !bones;
  // Or from a buffer the graphics card writes (see FromGpuBuffer), in a pass with targets or on a deferred context
  char gpuWhat[300] = "";
  bool gpu = false;
  if (!isMesh && !bones && !cloth && (targets > 0 || pass < 0) && vsAt)
  {
    bool tryIt = false;
    {
      std::lock_guard<std::mutex> lock(g_LogMutex);
      tryIt = g_ClothStateLogs < MaxClothStateLogs && !g_VsNotGpu.count(vsAt);
    }
    if (tryIt)
    {
      gpu = FromGpuBuffer(pContext, gpuWhat, sizeof(gpuWhat));
      if (!gpu)
      {
        std::lock_guard<std::mutex> lock(g_LogMutex);
        if (g_VsNotGpu.size() < 65536)
          g_VsNotGpu.insert(vsAt);
      }
    }
  }
  if (gpu)
    cloth = true;
  if (pass >= 0)
  {
    ++g_PassDraws[pass];
    if (isMesh)
      ++g_PassMeshDraws[pass];
    if (bones)
      ++g_PassBoneDraws[pass];
  }
  if (!isMesh && !bones && !cloth)
    return;
  char const* what = isMesh ? mesh.Name : cloth ? "cloth draw" : "bone-reading draw";
  // Its state, once for these shaders in this pass
  bool log = false;
  {
    std::lock_guard<std::mutex> lock(g_LogMutex);
    if (cloth)
    {
      if (g_ClothStateLogs < MaxClothStateLogs && g_Logged.insert(std::make_tuple(vsAt, psAt, pass)).second)
      {
        ++g_ClothStateLogs;
        log = true;
      }
    }
    else if (g_StateLogs < MaxStateLogs && (pass < 0 || g_PassStateLogs[pass] < MaxStateLogsPerPass) &&
        g_Logged.insert(std::make_tuple(vsAt, psAt, pass)).second)
    {
      ++g_StateLogs;
      if (pass >= 0)
        ++g_PassStateLogs[pass];
      log = true;
    }
  }
  if (log && gpu)
    util::log::Write("Draw log (diagnostic): a draw from buffers the graphics card writes (cloth?): %s", gpuWhat);
  if (log)
    LogState(pContext, isMesh ? mesh.Name : gpu ? "a draw from a buffer the graphics card writes" : cloth ?
      "a cloth draw (its vertex shader's resources named for cloth)" : (known ? "a draw whose vertex shader reads bones" : "?"), pass,
      vsAt, psAt, indexCount, instances, startIndex, baseVertex, startInstance);
  // The snapshot: the ones drawn to 3 or more targets (the G-buffer's; not shadows or depth)
  if (snapshot && targets >= 3)
  {
    ShaderInfo psInfo{};
    ShaderOf(psAt, psInfo);
    AddToSnapshot(pContext, what, isMesh, vsAt, psAt, vbAt, vsInfo, psInfo.Hash, indexCount, instances, startIndex, baseVertex,
      startInstance);
  }
}
