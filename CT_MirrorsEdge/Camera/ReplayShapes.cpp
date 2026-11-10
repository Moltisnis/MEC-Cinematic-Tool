#include "ReplayShapes.h"
#include "ReplayAssetLog.h"
#include "ReplayDrawLog.h"
#include "ReplayGameDraws.h"
#include "ReplayStore.h"
#include "../Main.h"
#include "../Util/Util.h"
#include "../Visuals/GreenPlane.h"
#include "../Visuals/D3DStateBackup.h"
#include "ReplayShapesVs.h"
#include "ReplayShapesPs.h"
#include <Windows.h>
#include <DirectXMath.h>
#include <algorithm>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstring>
#include <map>
#include <tuple>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <new>
#include <vector>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;
using namespace DirectX;

// The shapes of the replay's moving models (2026-10-27). The user, after the
// 02:17 test (the Release mission recorded, played once it was over): the
// mission's own models ("doors bus etc.") were missing. The replay moved the
// recorded models through the game's own entities, and the game destroys a
// mission's with the mission (6 of 21 were found again, the gates, the bus's
// doors, the glitch-key door and the lens not). So the replay keeps their
// shapes while it records and draws them itself.
//
// Where a static model's mesh is (2026-10-27, read live, read only):
// - the entity (ClientStaticModelEntity / PamClientStaticModelEntity) + 0x30
//   its StaticModelEntityData, + 0x98 its MeshAsset (RigidMeshAsset, vtable
//   0x141BBDAB0: + 0x10 its name), + 0x20 its MeshSet (vtable 0x141BBF880,
//   TypeInfo 0x142835050);
// - the MeshSet + 0x20 its data as loaded (the MeshSet res: + 0x20 a pointer
//   for each detail level, up to MaxLods, + 0x58 the full name, + 0x60 the
//   short one); a level's data: + 8 its section count, + 0xC (unaligned) its
//   sections, + 0x54 its index format (0x21: 16 bits, 0x2E: 32 bits), + 0x58
//   its index bytes, + 0x5C its vertex bytes; a section (0x130 apart): + 0x10
//   its material's name, + 0x20 its triangles, + 0x24 its first index, + 0x28
//   where its vertices start (bytes), + 0x2C how many, + 0x4C 16 vertex
//   elements (4 bytes each: usage, format, offset, stream; 0xFF ends them),
//   + 0x8C its vertex size. Usage 1 is the position; its formats here 3
//   Float3, 4 Float4, 7 Half3, 8 Half4 (the game's VertexElementFormat
//   values, from its reflected enum at 0x1428E0CB0);
// - the MeshSet + 0x50 + level * 0x50: that level's buffers while it's
//   loaded: + 8 its vertices, + 0x10 its indices, each a Dx11RenderBuffer
//   (vtable 0x141BD6840: + 0x10 the ID3D11Buffer, + 0x40 its size, + 0x44 1
//   vertices / 2 indices, + 0x58 the bytes it was made from). Checked over
//   every loaded mesh (4474 levels): every buffer's size was its level's
//   vertex or index bytes.
// The bytes at + 0x58 aren't kept for long (2026-10-27 03:21: of 23 models
// whose shapes were copied from them only 3 were kept, the others' indices
// past their vertices; read live a minute apart, the same car's indices were
// right, then not): the memory goes back to the game once the level is on
// the graphics card. So while it records, each recorded model's mesh (its
// most detailed level loaded then, and per section where its triangles and
// its positions are) is taken from the graphics card: its two ID3D11Buffers
// are held (QueryInterface, which adds a reference) and at the next Present
// copied on the graphics card into buffers of the replay's own
// (CopySubresourceRegion), then let go. Sections without a material name (a
// depth-only copy of the positions with indices of its own) or named Shadow
// (a flat shadow under a car) aren't drawn. Every read is guarded and checked
// (the classes by vtable, the sizes against each other, the buffers still
// the same after they're held). Nothing of the game is written; the only
// calls are the D3D buffers' own QueryInterface and Release.
//
// While it plays, the recorded models the game doesn't have (see
// ReplayModels::Play) are drawn at Present at their recorded places, with the
// game's own camera and projection, only where they're nearer than what the
// game drew (a copy of its depth, see greenplane::TakeFrameDepth) and their
// own nearer parts (a depth target of the replay's). A section is coloured
// with its colour texture as the game drew it while it recorded (see
// PickColour), or one colour (Colour) where none was kept, lit from a fixed
// direction (LightFrom) by each face's facing, both sides alike: not the
// game's lighting, its other textures (normals, smoothness) or its material
// parameters (a texture's tiling). They're drawn after the game's lighting,
// fog and depth of field, and over its HUD.
namespace
{
  const __int64 MeshSetVTableRva = 0x1BBF880;
  const __int64 RenderBufferVTableRva = 0x1BD6840;
  const int EntityDataOffset = 0x30;
  const int StaticModelMeshOffset = 0x98;
  const int MeshAssetMeshSetOffset = 0x20;
  const int MeshSetDataOffset = 0x20;
  const int MeshSetLevelsOffset = 0x50;
  const int MeshSetLevelSize = 0x50;
  const int DataLevelsOffset = 0x20;
  const int DataNameOffset = 0x58;
  const float MaxBoxMetres = 20000.f;   // a bounding box's corner farther from its origin isn't believed (the far Shard's: 9261)
  const int DataShortNameOffset = 0x60;
  const int MaxLods = 7;                     // the level pointers from + 0x20 up to the name at + 0x58
  // The mesh streaming manager's table (Hooks.cpp hMeshManagerCtor): a record in use has its MeshSet first
  const __int64 MeshManagerGlobalRva = 0x2563410;
  const int MeshTableOffset = 0x50;
  const int MeshRecordSize = 0x350;
  const __int64 MaxMeshRecords = 16384 + 64;
  const int LevelSectionCountOffset = 0x08;
  const int LevelSectionsOffset = 0x0C;
  const int LevelIndexFormatOffset = 0x54;
  const int LevelIndexBytesOffset = 0x58;
  const int LevelVertexBytesOffset = 0x5C;
  const int SectionSize = 0x130;
  const int MaxSections = 64;
  const int MaxElements = 16;
  const unsigned IndexFormat16 = 0x21, IndexFormat32 = 0x2E;
  const int BufferD3DOffset = 0x10, BufferSizeOffset = 0x40, BufferKindOffset = 0x44;
  const int SectionMaterialOffset = 0x10;
  const BYTE UsagePosition = 1, UsageTexCoord0 = 0x21;
  const BYTE FormatFloat2 = 2, FormatFloat3 = 3, FormatFloat4 = 4, FormatHalf2 = 6, FormatHalf3 = 7, FormatHalf4 = 8;
  const int MaxTextureLogs = 24;             // the slots seen at a captured section's draw, in the log
  const size_t MaxShapeBytes = 32u << 20;    // one mesh's vertices and indices
  const size_t MaxTotalBytes = 384u << 20;   // all of them (on the graphics card)
  const int MaxShapes = 2048;
  const int MaxDraws = 1024;
  // How far in front of the game's surface a pixel must be to be drawn
  // (metres, plus this much for each metre away). Until 2026-11-03 a pixel
  // 3 cm (+ 0.05 % a metre) behind it was drawn: a copy of a model the game
  // does draw, but took to be not drawn there, lay right on the game's own
  // and covered it in one colour (the mission test's 23:25 run: a tunnel's
  // ceiling over most of the picture, a wall). A model the game really
  // doesn't draw has what's behind it farther away (a door's room, a gate's
  // street) and is drawn as before; one flush with a wall shows only where
  // it stands out from it
  const float DepthNearer = 0.10f, DepthNearerPerMetre = 0.004f;
  // A still model's copy over the game's own isn't drawn with the camera
  // this near its place (see Apply)
  const float NearCameraSkipMetres = 3.f;
  const float Colour[3] = { 0.62f, 0.62f, 0.60f };
  const float LightFrom[3] = { 0.35f, 0.85f, 0.40f };
  const float Ambient = 0.45f;

  struct Section
  {
    BYTE PosFormat;
    UINT PosOffset;
    BYTE TcFormat;        // its first texture coordinates (0: none the replay reads)
    UINT TcOffset;
    UINT Stride;
    UINT VertexOffset;    // bytes
    UINT VertexCount;
    UINT StartIndex;
    UINT IndexCount;
  };

  struct Shape
  {
    __int64 MeshSet;
    char Name[96];
    int Lod;
    bool Index32;
    UINT VertexBytes, IndexBytes;
    std::vector<Section> Sections;
    // The game's buffers, held from the capture until the copy, and while it
    // records for the colour textures (render thread); their addresses, for
    // the draws (any thread: never dereferenced)
    ComPtr<ID3D11Buffer> GameVb, GameIb;
    __int64 GameVbAddress = 0, GameIbAddress = 0;
    // Each section's colour texture, kept from a draw of it by the game (see
    // Inspect): set once, from any thread; held until the shape goes
    std::unique_ptr<std::atomic<ID3D11ShaderResourceView*>[]> Textures;
    // Each section's passes the game drew it in (PassSeenGbuffer /
    // PassSeenOther; see Inspect): a section the game draws only in its
    // decal or transparent passes isn't drawn as the replay's opaque shape
    std::unique_ptr<std::atomic<unsigned char>[]> Passes;
    // Render thread: the replay's own copies
    ComPtr<ID3D11Buffer> Vb, Ib;
    std::atomic<bool> Uploaded{ false };
    bool Failed = false;
    ~Shape()
    {
      if (Textures)
        for (size_t s = 0; s < Sections.size(); ++s)
          if (ID3D11ShaderResourceView* view = Textures[s].exchange(nullptr))
            view->Release();
    }
  };

  std::mutex g_Mutex;   // g_Shapes, g_Draws, g_Bytes
  std::vector<std::shared_ptr<Shape>> g_Shapes;
  std::vector<replay_shapes::Draw> g_Draws;
  size_t g_Bytes = 0;
  bool g_CopiesPending = false;   // a shape's game buffers wait for their copy
  int g_Refused = 0;              // meshes that couldn't be kept (each once)
  // While it records: the game's draws are looked at for the captured
  // sections' colour textures (see Inspect), by the game's vertex buffer
  std::atomic<bool> g_Watching{ false };
  std::atomic<bool> g_ReleaseGameBuffers{ false };   // the recording is over: the held buffers go (render thread)
  std::mutex g_WatchMutex;   // g_WatchByVb, g_SectionsByVb
  std::unordered_map<__int64, std::shared_ptr<Shape>> g_WatchByVb;
  // While it records: each captured shape by the game's vertex buffer, with
  // its index (for its draws in the G-buffer pass, ReplayGameDraws)
  struct SectionsOf
  {
    int Index;
    std::shared_ptr<Shape> Shape;
  };
  std::unordered_map<__int64, SectionsOf> g_SectionsByVb;
  // While it records: a captured shape's mesh at its other levels loaded
  // (MapLevels), by their vertex buffers: the game's draws of a model at
  // another level than its shape's are kept too (2026-10-27: a model the game
  // drew only at another level had no kept draw, and was drawn as its plain
  // shape); each of its parts numbered (AltSection) under g_WatchMutex
  struct LevelOf
  {
    int Index;
    int Lod;
    int Parts;            // its parts drawn (sections with a material, not Shadow), 0 if not known
    char Name[96];
  };
  std::unordered_map<__int64, LevelOf> g_AltByVb;
  std::map<std::tuple<int, int, UINT, UINT>, int> g_AltIndex;   // shape, level, first index, count -> its number
  std::map<std::pair<int, int>, int> g_AltNext;                  // shape, level -> the next number
  const int MaxAltParts = 63;   // a level's parts and numbers (6 bits each in the section's number)
  std::atomic<int> g_TexturesKept{ 0 };
  std::atomic<int> g_TextureLogs{ 0 };
  std::atomic<int> g_NoColourLogs{ 0 };
  const int MaxNoColourLogs = 12;

  // Guarded reads (no objects to unwind in here)
  bool ReadRaw(void* out, __int64 at, size_t size)
  {
    __try
    {
      if (at < 0x10000)
        return false;
      memcpy(out, (void const*)at, size);
      return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
  }

  __int64 Q(__int64 at)
  {
    __int64 v = 0;
    return ReadRaw(&v, at, sizeof(v)) ? v : 0;
  }

  UINT D(__int64 at)
  {
    UINT v = 0;
    return ReadRaw(&v, at, sizeof(v)) ? v : 0;
  }

  void ReadName(__int64 at, char* out, size_t size)
  {
    out[0] = 0;
    if (at < 0x10000 || !ReadRaw(out, at, size - 1))
    {
      out[0] = 0;
      return;
    }
    out[size - 1] = 0;
    for (size_t i = 0; i < size && out[i]; ++i)
      if ((unsigned char)out[i] < 32 || (unsigned char)out[i] > 126)
      {
        out[i] = 0;
        break;
      }
  }

  // A static model's MeshSet (0 if it has none the replay can read): its mesh
  // asset's + 0x20, a RigidMeshAsset's (vtable 0x141BBDAB0) or a
  // SkinnedMeshAsset's. 2026-10-27 19:4x a skinned door (door_glitchkey_oneway
  // _right) was drawn 442 times as a model, by a vertex shader that doesn't
  // read bones; its MeshSet's levels are read the same way (FaithReplay's
  // stage 2b read them). The MeshSet's vtable is checked.
  __int64 MeshSetOf(__int64 entity)
  {
    __int64 base = (__int64)g_gameHandle;
    __int64 data = Q(entity + EntityDataOffset);
    __int64 asset = data ? Q(data + StaticModelMeshOffset) : 0;
    __int64 meshSet = asset ? Q(asset + MeshAssetMeshSetOffset) : 0;
    return meshSet && Q(meshSet) == base + MeshSetVTableRva ? meshSet : 0;
  }

  // A MeshSet's loaded levels' vertex buffers (the game's ID3D11Buffer
  // addresses; of the levels its data has, those loaded: a far one may have
  // only its last) (stage 2b: a skinned mesh's draws are known by them; the
  // MeshSet's and each buffer's vtable checked)
  int LevelVertexBuffers(__int64 meshSet, std::vector<__int64>& vbs)
  {
    vbs.clear();
    __int64 base = (__int64)g_gameHandle;
    if (!meshSet || Q(meshSet) != base + MeshSetVTableRva)
      return 0;
    __int64 data = Q(meshSet + MeshSetDataOffset);
    for (int lod = 0; lod < MaxLods && data && Q(data + DataLevelsOffset + lod * 8); ++lod)
    {
      __int64 buffer = Q(meshSet + MeshSetLevelsOffset + lod * MeshSetLevelSize + 8);
      if (!buffer || Q(buffer) != base + RenderBufferVTableRva || D(buffer + BufferKindOffset) != 1)
        continue;   // that level isn't loaded
      __int64 d3d = Q(buffer + BufferD3DOffset);
      if (d3d)
        vbs.push_back(d3d);
    }
    return (int)vbs.size();
  }

  // A loaded level's buffers: their size, kind and ID3D11Buffer
  bool LevelBuffer(__int64 buffer, UINT kind, UINT size, __int64& d3d)
  {
    if (!buffer || Q(buffer) != (__int64)g_gameHandle + RenderBufferVTableRva)
      return false;
    d3d = Q(buffer + BufferD3DOffset);
    return D(buffer + BufferSizeOffset) == size && D(buffer + BufferKindOffset) == kind && d3d;
  }

  // The game's ID3D11Buffer, held (null if it isn't one)
  ID3D11Buffer* HoldBuffer(__int64 at)
  {
    __try
    {
      if (at < 0x10000)
        return nullptr;
      ID3D11Buffer* buffer = nullptr;
      if (FAILED(((IUnknown*)at)->QueryInterface(__uuidof(ID3D11Buffer), (void**)&buffer)))
        return nullptr;
      return buffer;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return nullptr;
    }
  }

  bool IsDrawnSection(char const* material)
  {
    if (!material[0])
      return false;
    char lower[64];
    size_t i = 0;
    for (; material[i] && i + 1 < sizeof(lower); ++i)
      lower[i] = (char)tolower((unsigned char)material[i]);
    lower[i] = 0;
    return !strstr(lower, "shadow");
  }

  // The mesh's most detailed level loaded now: its sections, and its buffers
  // held (see above)
  bool CopyShape(__int64 meshSet, Shape& shape, char* why, size_t whySize)
  {
    __int64 data = Q(meshSet + MeshSetDataOffset);
    if (!data)
    {
      strcpy_s(why, whySize, "its data isn't there");
      return false;
    }
    ReadName(Q(data + DataShortNameOffset), shape.Name, sizeof(shape.Name));
    for (int lod = 0; lod < MaxLods; ++lod)
    {
      __int64 level = Q(data + DataLevelsOffset + lod * 8);
      if (!level)
        break;
      __int64 block = meshSet + MeshSetLevelsOffset + lod * MeshSetLevelSize;
      __int64 vbObject = Q(block + 8), ibObject = Q(block + 0x10);
      UINT indexBytes = D(level + LevelIndexBytesOffset), vertexBytes = D(level + LevelVertexBytesOffset);
      __int64 vbD3D = 0, ibD3D = 0;
      if (!vertexBytes || !indexBytes || !LevelBuffer(vbObject, 1, vertexBytes, vbD3D) ||
          !LevelBuffer(ibObject, 2, indexBytes, ibD3D))
        continue;   // not loaded: a less detailed one may be
      if ((size_t)vertexBytes + indexBytes > MaxShapeBytes)
      {
        sprintf_s(why, whySize, "level %d is %u KB, more than the %u KB kept for one", lod, (vertexBytes + indexBytes) >> 10,
          (UINT)(MaxShapeBytes >> 10));
        return false;
      }
      UINT format = D(level + LevelIndexFormatOffset);
      if (format != IndexFormat16 && format != IndexFormat32)
      {
        sprintf_s(why, whySize, "level %d's index format 0x%X isn't known", lod, format);
        return false;
      }
      UINT indexSize = format == IndexFormat32 ? 4 : 2;
      UINT sectionCount = D(level + LevelSectionCountOffset);
      __int64 sections = Q(level + LevelSectionsOffset);
      if (!sectionCount || sectionCount > MaxSections || !sections)
      {
        sprintf_s(why, whySize, "level %d has %u sections", lod, sectionCount);
        return false;
      }
      shape.Sections.clear();
      for (UINT s = 0; s < sectionCount; ++s)
      {
        BYTE raw[SectionSize];
        if (!ReadRaw(raw, sections + (__int64)s * SectionSize, sizeof(raw)))
          break;
        char material[64];
        ReadName(*(__int64 const*)(raw + SectionMaterialOffset), material, sizeof(material));
        if (!IsDrawnSection(material))
          continue;
        Section sec{};
        UINT triangles = *(UINT const*)(raw + 0x20);
        sec.StartIndex = *(UINT const*)(raw + 0x24);
        sec.VertexOffset = *(UINT const*)(raw + 0x28);
        sec.VertexCount = *(UINT const*)(raw + 0x2C);
        sec.Stride = *(UINT const*)(raw + 0x8C);
        sec.IndexCount = triangles * 3;
        bool found = false;
        for (int e = 0; e < MaxElements; ++e)
        {
          BYTE const* element = raw + 0x4C + e * 4;
          if (element[0] == 0xFF)
            break;
          if (element[0] == UsagePosition && element[3] == 0 && !found)
          {
            sec.PosFormat = element[1];
            sec.PosOffset = element[2];
            found = true;
          }
          else if (element[0] == UsageTexCoord0 && element[3] == 0 && !sec.TcFormat &&
                   (element[1] == FormatHalf2 || element[1] == FormatFloat2))
          {
            sec.TcFormat = element[1];
            sec.TcOffset = element[2];
          }
        }
        if (sec.TcFormat && sec.TcOffset + (sec.TcFormat == FormatHalf2 ? 4u : 8u) > *(UINT const*)(raw + 0x8C))
          sec.TcFormat = 0;
        UINT posSize = sec.PosFormat == FormatFloat3 ? 12 : sec.PosFormat == FormatFloat4 ? 16 :
          sec.PosFormat == FormatHalf3 ? 6 : sec.PosFormat == FormatHalf4 ? 8 : 0;
        if (!found || !posSize || !triangles || sec.Stride < 6 || sec.Stride > 256 || sec.PosOffset + posSize > sec.Stride ||
            !sec.VertexCount || (unsigned long long)sec.VertexOffset + (unsigned long long)sec.VertexCount * sec.Stride > vertexBytes ||
            ((unsigned long long)sec.StartIndex + sec.IndexCount) * indexSize > indexBytes)
          continue;
        shape.Sections.push_back(sec);
      }
      if (shape.Sections.empty())
      {
        sprintf_s(why, whySize, "none of level %d's %u sections has a position the replay can draw", lod, sectionCount);
        return false;
      }
      // Its two buffers held, and still the level's after that (the game
      // streams levels in and out)
      shape.GameVb.Attach(HoldBuffer(vbD3D));
      shape.GameIb.Attach(HoldBuffer(ibD3D));
      __int64 vbAfter = 0, ibAfter = 0;
      if (!shape.GameVb || !shape.GameIb || Q(block + 8) != vbObject || Q(block + 0x10) != ibObject ||
          !LevelBuffer(vbObject, 1, vertexBytes, vbAfter) || !LevelBuffer(ibObject, 2, indexBytes, ibAfter) ||
          vbAfter != vbD3D || ibAfter != ibD3D)
      {
        shape.GameVb.Reset();
        shape.GameIb.Reset();
        sprintf_s(why, whySize, "level %d's buffers couldn't be held as they were", lod);
        return false;
      }
      shape.VertexBytes = vertexBytes;
      shape.IndexBytes = indexBytes;
      shape.GameVbAddress = (__int64)shape.GameVb.Get();
      shape.GameIbAddress = (__int64)shape.GameIb.Get();
      shape.Lod = lod;
      shape.Index32 = indexSize == 4;
      try
      {
        shape.Textures.reset(new std::atomic<ID3D11ShaderResourceView*>[shape.Sections.size()]);
        shape.Passes.reset(new std::atomic<unsigned char>[shape.Sections.size()]);
      }
      catch (std::bad_alloc const&)
      {
        strcpy_s(why, whySize, "no memory for it");
        return false;
      }
      for (size_t s = 0; s < shape.Sections.size(); ++s)
      {
        shape.Textures[s] = nullptr;
        shape.Passes[s] = 0;
      }
      return true;
    }
    strcpy_s(why, whySize, "none of its levels is loaded");
    return false;
  }

  // Render thread
  ComPtr<ID3D11VertexShader> g_Vs;
  ComPtr<ID3D11PixelShader> g_Ps;
  ComPtr<ID3D11Buffer> g_Constants;
  ComPtr<ID3D11RasterizerState> g_Rasterizer;
  ComPtr<ID3D11BlendState> g_Blend;
  ComPtr<ID3D11DepthStencilState> g_DepthNormal, g_DepthReversed;
  ComPtr<ID3D11SamplerState> g_Sampler;
  std::map<unsigned long long, ComPtr<ID3D11InputLayout>> g_Layouts;   // position format, offset, texture coordinates' format, offset
  ComPtr<ID3D11Texture2D> g_DepthTexture;
  ComPtr<ID3D11DepthStencilView> g_DepthView;
  UINT g_DepthWidth = 0, g_DepthHeight = 0;
  bool g_Made = false, g_Failed = false;
  D3DStateBackup g_Backup;

  struct Constants
  {
    XMFLOAT4X4 ViewProjection;
    XMFLOAT4X4 World;
    XMFLOAT4 Depth;
    XMFLOAT4 Projection;
    XMFLOAT4 Light;
    XMFLOAT4 Color;
  };

  bool Make(ID3D11Device* pDevice)
  {
    HRESULT hr = pDevice->CreateVertexShader(g_ReplayShapesVs, sizeof(g_ReplayShapesVs), nullptr, g_Vs.GetAddressOf());
    if (SUCCEEDED(hr))
      hr = pDevice->CreatePixelShader(g_ReplayShapesPs, sizeof(g_ReplayShapesPs), nullptr, g_Ps.GetAddressOf());
    D3D11_BUFFER_DESC constants{};
    constants.ByteWidth = (sizeof(Constants) + 15) & ~15u;
    constants.Usage = D3D11_USAGE_DYNAMIC;
    constants.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    constants.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (SUCCEEDED(hr))
      hr = pDevice->CreateBuffer(&constants, nullptr, g_Constants.GetAddressOf());
    D3D11_RASTERIZER_DESC rasterizer{};
    rasterizer.FillMode = D3D11_FILL_SOLID;
    rasterizer.CullMode = D3D11_CULL_NONE;
    rasterizer.DepthClipEnable = TRUE;
    if (SUCCEEDED(hr))
      hr = pDevice->CreateRasterizerState(&rasterizer, g_Rasterizer.GetAddressOf());
    D3D11_BLEND_DESC blend{};
    blend.RenderTarget[0].BlendEnable = FALSE;
    blend.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    if (SUCCEEDED(hr))
      hr = pDevice->CreateBlendState(&blend, g_Blend.GetAddressOf());
    D3D11_DEPTH_STENCIL_DESC depth{};
    depth.DepthEnable = TRUE;
    depth.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    depth.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    if (SUCCEEDED(hr))
      hr = pDevice->CreateDepthStencilState(&depth, g_DepthNormal.GetAddressOf());
    depth.DepthFunc = D3D11_COMPARISON_GREATER_EQUAL;
    if (SUCCEEDED(hr))
      hr = pDevice->CreateDepthStencilState(&depth, g_DepthReversed.GetAddressOf());
    D3D11_SAMPLER_DESC sampler{};
    sampler.Filter = D3D11_FILTER_ANISOTROPIC;
    sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sampler.MaxAnisotropy = 8;
    sampler.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampler.MaxLOD = D3D11_FLOAT32_MAX;
    if (SUCCEEDED(hr))
      hr = pDevice->CreateSamplerState(&sampler, g_Sampler.GetAddressOf());
    if (FAILED(hr))
    {
      g_Failed = true;
      util::log::Warning("Replay: the drawing of the recorded models the game doesn't have couldn't be set up (HRESULT 0x%08X)",
        (unsigned int)hr);
      return false;
    }
    g_Made = true;
    return true;
  }

  // A section's input layout: its position, and its first texture
  // coordinates (when it has none the shader's are read from its position,
  // unused then)
  ID3D11InputLayout* LayoutFor(ID3D11Device* pDevice, Section const& s)
  {
    unsigned long long key = ((unsigned long long)s.PosFormat << 48) | ((unsigned long long)s.PosOffset << 32) |
      ((unsigned long long)s.TcFormat << 16) | s.TcOffset;
    auto it = g_Layouts.find(key);
    if (it != g_Layouts.end())
      return it->second.Get();
    D3D11_INPUT_ELEMENT_DESC elements[2]{};
    elements[0].SemanticName = "POSITION";
    elements[0].Format = s.PosFormat == FormatFloat3 ? DXGI_FORMAT_R32G32B32_FLOAT : s.PosFormat == FormatFloat4 ?
      DXGI_FORMAT_R32G32B32A32_FLOAT : DXGI_FORMAT_R16G16B16A16_FLOAT;   // a half3 read with what follows it (the shader takes xyz)
    elements[0].AlignedByteOffset = s.PosOffset;
    elements[0].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
    elements[1].SemanticName = "TEXCOORD";
    elements[1].Format = s.TcFormat == FormatFloat2 ? DXGI_FORMAT_R32G32_FLOAT : DXGI_FORMAT_R16G16_FLOAT;
    elements[1].AlignedByteOffset = s.TcFormat ? s.TcOffset : s.PosOffset;
    elements[1].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
    ComPtr<ID3D11InputLayout> layout;
    if (FAILED(pDevice->CreateInputLayout(elements, 2, g_ReplayShapesVs, sizeof(g_ReplayShapesVs), layout.GetAddressOf())))
      layout = nullptr;
    g_Layouts[key] = layout;
    return layout.Get();
  }

  // The colour textures (2026-10-27, the user: "draw the replay's shapes with
  // their real colour textures"). The meshes' own data hold no texture names
  // in this game (a mesh's materials are its shaders and a few parameters), so
  // the textures are taken from the game's drawing: while it records, its
  // indexed draws (ID3D11DeviceContext's DrawIndexed and DrawIndexedInstanced,
  // slots 12 and 20, the immediate context's and a deferred context's
  // functions) are looked at, and a draw whose vertex buffer (slot 0) is a
  // captured level's, with its index buffer and a section's first index and
  // count, has its pixel shader's views read (slots 0 - 15): the first 2D one
  // in an sRGB colour format is that section's colour texture, held (its view
  // and so its texture stay alive after the mission is over). The game draws
  // a mesh in several passes (shadows, depth: no colour texture then); the
  // first one with one gives it. The slots seen are logged for the first
  // MaxTextureLogs, for checking.
  bool IsColourFormat(DXGI_FORMAT format)
  {
    switch (format)
    {
    case DXGI_FORMAT_BC1_UNORM_SRGB:
    case DXGI_FORMAT_BC2_UNORM_SRGB:
    case DXGI_FORMAT_BC3_UNORM_SRGB:
    case DXGI_FORMAT_BC7_UNORM_SRGB:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
      return true;
    default:
      return false;
    }
  }

  // The pixel shader's colour texture now, held (null if none)
  ID3D11ShaderResourceView* PickColour(ID3D11DeviceContext* pContext, Shape const& shape, size_t section)
  {
    ID3D11ShaderResourceView* views[16] = {};
    pContext->PSGetShaderResources(0, 16, views);
    int chosen = -1;
    char seen[320] = "";
    size_t at = 0;
    bool diagnostics = util::DiagnosticsOn();   // the slots seen and their lines
    for (int i = 0; i < 16; ++i)
    {
      if (!views[i])
        continue;
      D3D11_SHADER_RESOURCE_VIEW_DESC desc{};
      views[i]->GetDesc(&desc);
      if (diagnostics && at + 24 < sizeof(seen))
        at += sprintf_s(seen + at, sizeof(seen) - at, " t%d format %d (%s)", i, (int)desc.Format,
          desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2D ? "2D" : "other");
      if (chosen < 0 && desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2D && IsColourFormat(desc.Format))
        chosen = i;
    }
    if (diagnostics && chosen >= 0 && g_TextureLogs.fetch_add(1) < MaxTextureLogs)
      util::log::Write("Replay:   '%s' section %d: its colour texture kept from the game's draw (t%d; the pixel shader's views:%s)",
        shape.Name, (int)section, chosen, seen);
    else if (diagnostics && chosen < 0 && at > 0 && g_NoColourLogs.fetch_add(1) < MaxNoColourLogs)
      util::log::Write("Replay (diagnostic):   '%s' section %d: a draw of it by the game with no sRGB colour texture (the pixel "
        "shader's views:%s); a later one may have it", shape.Name, (int)section, seen);
    ID3D11ShaderResourceView* kept = nullptr;
    for (int i = 0; i < 16; ++i)
      if (views[i])
      {
        if (i == chosen)
          kept = views[i];
        else
          views[i]->Release();
      }
    return kept;
  }

  // The passes a section was drawn in by the game (2026-11-01, the user's
  // gate screenshot: "it's the facility 7 texture with the foodome texture
  // together"). The gate door's glass panes (releasegatedoor_01 section 1)
  // are drawn by the game only in a lit picture's pass, transparent, with a
  // printed texture in t3 (BC7 sRGB) over clear glass; their colour texture
  // was taken from that draw, and whenever their kept draw wasn't drawn (its
  // lit pass is matched by its number in the frame, which can differ when it
  // plays) the replay drew them as its opaque shape: the print, solid, over
  // the whole pane. A decal's part would be the same.
  const unsigned char PassSeenGbuffer = 1, PassSeenOther = 2;
  const unsigned ShapePassesTag = 0x53534150;   // "PASS"

  bool OnlyOtherPasses(Shape const& shape, size_t section)
  {
    unsigned char seen = shape.Passes ? shape.Passes[section].load(std::memory_order_relaxed) : 0;
    return (seen & PassSeenOther) && !(seen & PassSeenGbuffer);
  }

  // A draw of the game's (any thread, any context): a captured section's
  // colour texture, if it's one of theirs; on the immediate context, the
  // pass it's drawn in (a deferred context's pass isn't known then)
  void Inspect(ID3D11DeviceContext* pContext, UINT indexCount, UINT startIndex)
  {
    ID3D11Buffer* vb = nullptr;
    UINT stride = 0, offset = 0;
    pContext->IAGetVertexBuffers(0, 1, &vb, &stride, &offset);
    if (!vb)
      return;
    std::shared_ptr<Shape> shape;
    {
      std::lock_guard<std::mutex> lock(g_WatchMutex);
      auto it = g_WatchByVb.find((__int64)vb);
      if (it != g_WatchByVb.end())
        shape = it->second;
    }
    vb->Release();
    if (!shape || !shape->Textures)
      return;
    ID3D11Buffer* ib = nullptr;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    UINT ibOffset = 0;
    pContext->IAGetIndexBuffer(&ib, &format, &ibOffset);
    bool sameIb = ib && (__int64)ib == shape->GameIbAddress;
    if (ib)
      ib->Release();
    if (!sameIb)
      return;
    UINT first = startIndex + ibOffset / (shape->Index32 ? 4 : 2);
    unsigned char passBit = 0;
    if (pContext->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE)
    {
      int pass = replay_gamedraws::PassNow();
      passBit = pass == 0 ? PassSeenGbuffer : pass > 0 ? PassSeenOther : 0;
    }
    bool all = true;
    for (size_t s = 0; s < shape->Sections.size(); ++s)
    {
      Section const& sec = shape->Sections[s];
      if (passBit && shape->Passes && sec.StartIndex == first && sec.IndexCount == indexCount)
        shape->Passes[s].fetch_or(passBit, std::memory_order_relaxed);
      if (!shape->Textures[s].load() && sec.StartIndex == first && sec.IndexCount == indexCount && sec.TcFormat)
        if (ID3D11ShaderResourceView* view = PickColour(pContext, *shape, s))
        {
          ID3D11ShaderResourceView* none = nullptr;
          if (shape->Textures[s].compare_exchange_strong(none, view))
            ++g_TexturesKept;
          else
            view->Release();
        }
      // Still looked at while drawn only in another pass so far (the G-buffer
      // pass may draw it too later)
      all = all && (shape->Textures[s].load() || !sec.TcFormat) && !OnlyOtherPasses(*shape, s);
    }
    // Every section has one: not looked for any more
    if (all)
    {
      std::lock_guard<std::mutex> lock(g_WatchMutex);
      g_WatchByVb.erase(shape->GameVbAddress);
    }
  }

  typedef void(STDMETHODCALLTYPE* tDrawIndexed)(ID3D11DeviceContext*, UINT, UINT, INT);
  typedef void(STDMETHODCALLTYPE* tDrawIndexedInstanced)(ID3D11DeviceContext*, UINT, UINT, UINT, INT, UINT);
  tDrawIndexed oDrawIndexed = nullptr, oDeferredDrawIndexed = nullptr;
  tDrawIndexedInstanced oDrawIndexedInstanced = nullptr, oDeferredDrawIndexedInstanced = nullptr;
  const int DrawIndexedSlot = 12, DrawIndexedInstancedSlot = 20;
  bool g_DrawsHooked = false;
  // 2026-11-08 (the user: "replays are weird now arrows disappear also
  // textures are flickering"): d3d11 changes an immediate context's draw
  // functions while it runs. It keeps a table of them in the context
  // (d3d11.dll 10.0.26100: its + 0xE0, DrawIndexed at + 0x140) and writes
  // other functions into it when it starts or stops checking draws
  // (0x46F40: DrawIndexed 0x153B70 or 0x154D30 in place of 0x1535A0,
  // DrawIndexedInstanced 0x1A4580 or 0x155640 in place of 0x1531B0, the other
  // draws and dispatches too). The hooks went on the ones in the table when
  // they were made: in the 03:19 session the quick ones (1289 draws a frame
  // seen), in the 04:31 one the checking ones, which the game hardly drew
  // through after (1 draw a frame seen). So the recording kept 113 draws
  // instead of 1831, no Dash marker but one arrow's first half second, and
  // while it played the replay took the game's models for not drawn and drew
  // its own copies over them, on and off (the flicker). Now each function
  // found in the context's table is hooked: the ones there when the hooks
  // are made, then any other seen at the game's render target changes
  // (NoteContextDraws), hooked at the next Present (MaxMoreDraws of each).
  const int MaxMoreDraws = 4;
  tDrawIndexed oDrawIndexedMore[MaxMoreDraws] = {};
  tDrawIndexedInstanced oDrawIndexedInstancedMore[MaxMoreDraws] = {};
  // The functions hooked (render thread): the immediate context's first,
  // the deferred context's, then the others in turn; a new one seen
  std::vector<__int64> g_DrawIndexedHooked, g_DrawIndexedInstancedHooked;
  __int64 g_DrawIndexedSeen = 0, g_DrawIndexedInstancedSeen = 0;
  int g_DrawIndexedMoreUsed = 0, g_DrawIndexedInstancedMoreUsed = 0;   // the hooks of each kind used
  bool g_LoggedMoreDrawsFull = false;
  // A saved recording was loaded: the draws are hooked though nothing records
  // (2026-10-27 21:33: a loaded one's kept draws were never drawn, the hooks
  // only went in when a recording captured shapes)
  std::atomic<bool> g_WantHooks{ false };

  // A draw of the game's in its G-buffer pass while the replay records: kept
  // (ReplayGameDraws) if it's a captured shape's section (by the game's
  // buffers, its first index and count, as Inspect finds it)
  void CaptureSection(ID3D11DeviceContext* pContext, UINT indexCount, UINT instances, UINT startIndex, INT baseVertex)
  {
    ID3D11Buffer* vb = nullptr;
    UINT stride = 0, offset = 0;
    pContext->IAGetVertexBuffers(0, 1, &vb, &stride, &offset);
    if (!vb)
      return;
    SectionsOf watched{ -1, nullptr };
    {
      std::lock_guard<std::mutex> lock(g_WatchMutex);
      auto it = g_SectionsByVb.find((__int64)vb);
      if (it != g_SectionsByVb.end())
        watched = it->second;
    }
    vb->Release();
    if (!watched.Shape)
    {
      // Another level of a captured shape's mesh: its part numbered (0x8000,
      // its level from bit 12, its number below)
      LevelOf level{ -1, 0, 0, "" };
      {
        std::lock_guard<std::mutex> lock(g_WatchMutex);
        auto it = g_AltByVb.find((__int64)vb);
        if (it != g_AltByVb.end())
          level = it->second;
      }
      if (level.Index < 0)
        return;
      ID3D11Buffer* altIb = nullptr;
      DXGI_FORMAT altFormat = DXGI_FORMAT_UNKNOWN;
      UINT altOffset = 0;
      pContext->IAGetIndexBuffer(&altIb, &altFormat, &altOffset);
      if (altIb)
        altIb->Release();
      if (!altIb)
        return;
      UINT altFirst = startIndex + altOffset / (altFormat == DXGI_FORMAT_R32_UINT ? 4 : 2);
      int number = -1;
      {
        std::lock_guard<std::mutex> lock(g_WatchMutex);
        auto key = std::make_tuple(level.Index, level.Lod, altFirst, indexCount);
        auto it = g_AltIndex.find(key);
        int& next = g_AltNext[std::make_pair(level.Index, level.Lod)];
        if (it != g_AltIndex.end())
          number = it->second;
        else if (next <= MaxAltParts)
          number = g_AltIndex[key] = next++;
      }
      // The level's parts with it (bits 6 - 11), for it to be drawn whole
      // only when every part of it was kept (2026-10-27 22:53: "half of the
      // mesh is missing")
      if (number >= 0)
        replay_gamedraws::CaptureDraw(pContext, level.Index, 0x8000 | (level.Lod << 12) | (level.Parts << 6) | number, level.Name,
          indexCount, instances, startIndex, baseVertex);
      return;
    }
    ID3D11Buffer* ib = nullptr;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    UINT ibOffset = 0;
    pContext->IAGetIndexBuffer(&ib, &format, &ibOffset);
    bool sameIb = ib && (__int64)ib == watched.Shape->GameIbAddress;
    if (ib)
      ib->Release();
    if (!sameIb)
      return;
    UINT first = startIndex + ibOffset / (watched.Shape->Index32 ? 4 : 2);
    for (size_t s = 0; s < watched.Shape->Sections.size(); ++s)
      if (watched.Shape->Sections[s].StartIndex == first && watched.Shape->Sections[s].IndexCount == indexCount)
      {
        replay_gamedraws::CaptureDraw(pContext, watched.Index, (int)s, watched.Shape->Name, indexCount, instances, startIndex,
          baseVertex);
        return;
      }
    // Another pass's part that isn't one of its sections (a door's decal or
    // screen): numbered from 0x4000
    int pass = replay_gamedraws::PassNow();
    if (pass <= 0)
      return;
    int number = -1;
    {
      std::lock_guard<std::mutex> lock(g_WatchMutex);
      auto key = std::make_tuple(watched.Index, 100 + pass, first, indexCount);
      auto it = g_AltIndex.find(key);
      int& next = g_AltNext[std::make_pair(watched.Index, 100)];
      if (it != g_AltIndex.end())
        number = it->second;
      else if (next < 0x1000)
        number = g_AltIndex[key] = next++;
    }
    if (number >= 0)
      replay_gamedraws::CaptureDraw(pContext, watched.Index, 0x4000 | number, watched.Shape->Name, indexCount, instances, startIndex,
        baseVertex);
  }

  // A recorded model's draw in a pass past the ones the replay keeps (see
  // replay_gamedraws::PastKeptPass): in the log, once a shape (2026-10-28:
  // the Release gate's glass wasn't kept, and where it went wasn't known)
  void NotePastPass(ID3D11DeviceContext* pContext, int past)
  {
    ID3D11Buffer* vb = nullptr;
    UINT stride = 0, offset = 0;
    pContext->IAGetVertexBuffers(0, 1, &vb, &stride, &offset);
    if (!vb)
      return;
    __int64 vbAt = (__int64)vb;
    vb->Release();
    // Under g_WatchMutex: the draw hooks run on any thread (deferred contexts' draws too)
    static std::set<int> logged;
    char name[sizeof(Shape::Name)] = "";
    {
      std::lock_guard<std::mutex> lock(g_WatchMutex);
      auto it = g_SectionsByVb.find(vbAt);
      if (it == g_SectionsByVb.end() || !it->second.Shape)
        return;
      if (logged.size() >= 64 || !logged.insert(it->second.Index).second)
        return;
      strcpy_s(name, it->second.Shape->Name);
    }
    int kind = past / 1000;
    util::log::Write("Replay (diagnostic): '%s' (a recorded model's mesh) is drawn in the game's %s pass number %d after the G-buffer "
      "pass, past the ones the replay keeps: not kept", name, kind == 1 ? "decal" : kind == 2 ? "lit picture's" : kind == 5 ?
      "runner's vision layer" : "half-size transparent", past % 1000);
  }

  // The draw hooks' own time and draws (any thread; see replay_shapes::TakeHookCost)
  std::atomic<long long> g_HookTicks{ 0 }, g_HookDraws{ 0 };

  void LookAtTimed(ID3D11DeviceContext* pContext, UINT count, UINT instances, UINT start, INT base, UINT startInstance);

  // Each draw of the game's (the tools' own drawing in its G-buffer pass,
  // ReplayGameDraws, isn't looked at)
  void LookAt(ID3D11DeviceContext* pContext, UINT count, UINT instances, UINT start, INT base, UINT startInstance)
  {
    if (g_shutdown)
      return;
    // (timed for the recording's cost line only: diagnostics)
    if (!util::DiagnosticsOn())
    {
      LookAtTimed(pContext, count, instances, start, base, startInstance);
      return;
    }
    LARGE_INTEGER from, to;
    QueryPerformanceCounter(&from);
    LookAtTimed(pContext, count, instances, start, base, startInstance);
    QueryPerformanceCounter(&to);
    g_HookTicks.fetch_add(to.QuadPart - from.QuadPart, std::memory_order_relaxed);
    g_HookDraws.fetch_add(1, std::memory_order_relaxed);
  }

  void LookAtTimed(ID3D11DeviceContext* pContext, UINT count, UINT instances, UINT start, INT base, UINT startInstance)
  {
    replay_gamedraws::OnDraw(pContext);
    if (g_Watching.load(std::memory_order_relaxed))
      Inspect(pContext, count, start);
    if (replay_drawlog::Active())
      replay_drawlog::OnDraw(pContext, count, instances, start, base, startInstance);
    replay_assetlog::OnDraw(pContext, count, start);
    if (replay_gamedraws::Capturing())
      CaptureSection(pContext, count, instances, start, base);
    else if (util::DiagnosticsOn())   // (NotePastPass's line only)
    {
      int past = replay_gamedraws::PastKeptPass();
      if (past >= 0)
        NotePastPass(pContext, past);
    }
    // (Their time in the recording's cost, replay_gamedraws::Cost_Captures)
    if (replay_gamedraws::CapturingSkins())
    {
      replay_gamedraws::CostTimer timer(replay_gamedraws::Cost_Captures);
      replay_gamedraws::CaptureSkin(pContext, count, instances, start, base);
    }
    // Any skinned draw's track (drones and crowds the replay doesn't record)
    if (replay_gamedraws::CapturingTracks())
    {
      replay_gamedraws::CostTimer timer(replay_gamedraws::Cost_Captures);
      replay_gamedraws::CaptureTrack(pContext, count, instances, start, base);
    }
    // The crowd system's draws: their members (see ReplayGameDraws' crowds)
    if (replay_gamedraws::CapturingCrowds())
    {
      replay_gamedraws::CostTimer timer(replay_gamedraws::Cost_Captures);
      replay_gamedraws::CaptureCrowd(pContext, count, instances, start, base);
    }
    // While it plays: where the game draws a kept world mesh's own instances
    if (replay_gamedraws::Probing())
      replay_gamedraws::ProbeDraw(pContext, count, instances, start, base);
    // The world's own draws (not a recorded model's: those go by their shapes)
    if (replay_gamedraws::CapturingWorld())
    {
      replay_gamedraws::CostTimer timer(replay_gamedraws::Cost_Captures);
      ID3D11Buffer* vb = nullptr;
      UINT stride = 0, offset = 0;
      pContext->IAGetVertexBuffers(0, 1, &vb, &stride, &offset);
      bool shape = false;
      if (vb)
      {
        std::lock_guard<std::mutex> lock(g_WatchMutex);
        shape = g_SectionsByVb.count((__int64)vb) != 0 || g_AltByVb.count((__int64)vb) != 0;
      }
      if (vb)
        vb->Release();
      if (vb && !shape)
        replay_gamedraws::CaptureWorld(pContext, count, instances, start, base);
    }
  }

  // (A draw the replay leaves out isn't passed on: the game's crowds while the
  // recorded ones play, replay_gamedraws::SkipGameDraw)
  void STDMETHODCALLTYPE hDrawIndexed(ID3D11DeviceContext* pContext, UINT count, UINT start, INT base)
  {
    if (!replay_gamedraws::Bypass())
    {
      LookAt(pContext, count, 1, start, base, 0);
      if (replay_gamedraws::SkipGameDraw(pContext))
        return;
    }
    oDrawIndexed(pContext, count, start, base);
  }

  void STDMETHODCALLTYPE hDeferredDrawIndexed(ID3D11DeviceContext* pContext, UINT count, UINT start, INT base)
  {
    if (!replay_gamedraws::Bypass())
    {
      LookAt(pContext, count, 1, start, base, 0);
      if (replay_gamedraws::SkipGameDraw(pContext))
        return;
    }
    oDeferredDrawIndexed(pContext, count, start, base);
  }

  void STDMETHODCALLTYPE hDrawIndexedInstanced(ID3D11DeviceContext* pContext, UINT count, UINT instances, UINT start, INT base,
    UINT startInstance)
  {
    if (!replay_gamedraws::Bypass())
    {
      LookAt(pContext, count, instances, start, base, startInstance);
      if (replay_gamedraws::SkipGameDraw(pContext))
        return;
    }
    oDrawIndexedInstanced(pContext, count, instances, start, base, startInstance);
  }

  void STDMETHODCALLTYPE hDeferredDrawIndexedInstanced(ID3D11DeviceContext* pContext, UINT count, UINT instances, UINT start,
    INT base, UINT startInstance)
  {
    if (!replay_gamedraws::Bypass())
    {
      LookAt(pContext, count, instances, start, base, startInstance);
      if (replay_gamedraws::SkipGameDraw(pContext))
        return;
    }
    oDeferredDrawIndexedInstanced(pContext, count, instances, start, base, startInstance);
  }

  // The same for the other draw functions d3d11 gives the immediate context
  // (see MaxMoreDraws), one hook function each
  template <int K>
  void STDMETHODCALLTYPE hDrawIndexedMore(ID3D11DeviceContext* pContext, UINT count, UINT start, INT base)
  {
    if (!replay_gamedraws::Bypass())
    {
      LookAt(pContext, count, 1, start, base, 0);
      if (replay_gamedraws::SkipGameDraw(pContext))
        return;
    }
    oDrawIndexedMore[K](pContext, count, start, base);
  }

  template <int K>
  void STDMETHODCALLTYPE hDrawIndexedInstancedMore(ID3D11DeviceContext* pContext, UINT count, UINT instances, UINT start, INT base,
    UINT startInstance)
  {
    if (!replay_gamedraws::Bypass())
    {
      LookAt(pContext, count, instances, start, base, startInstance);
      if (replay_gamedraws::SkipGameDraw(pContext))
        return;
    }
    oDrawIndexedInstancedMore[K](pContext, count, instances, start, base, startInstance);
  }

  PVOID const DrawIndexedMoreHooks[MaxMoreDraws] = { (PVOID)hDrawIndexedMore<0>, (PVOID)hDrawIndexedMore<1>,
    (PVOID)hDrawIndexedMore<2>, (PVOID)hDrawIndexedMore<3> };
  PVOID const DrawIndexedInstancedMoreHooks[MaxMoreDraws] = { (PVOID)hDrawIndexedInstancedMore<0>,
    (PVOID)hDrawIndexedInstancedMore<1>, (PVOID)hDrawIndexedInstancedMore<2>, (PVOID)hDrawIndexedInstancedMore<3> };

  // Render thread, at Present once the draws are hooked: the immediate
  // context's draw functions now, and the ones seen at the game's render
  // target changes, hooked where they aren't yet (see MaxMoreDraws)
  void MoreDrawFunctions(ID3D11DeviceContext* pContext)
  {
    void** table = *(void***)pContext;
    __int64 found[2][2] = { { (__int64)table[DrawIndexedSlot], g_DrawIndexedSeen },
      { (__int64)table[DrawIndexedInstancedSlot], g_DrawIndexedInstancedSeen } };
    g_DrawIndexedSeen = g_DrawIndexedInstancedSeen = 0;
    for (int kind = 0; kind < 2; ++kind)
    {
      std::vector<__int64>& hooked = kind == 0 ? g_DrawIndexedHooked : g_DrawIndexedInstancedHooked;
      for (__int64 function : found[kind])
      {
        if (!function || std::find(hooked.begin(), hooked.end(), function) != hooked.end())
          continue;
        int& used = kind == 0 ? g_DrawIndexedMoreUsed : g_DrawIndexedInstancedMoreUsed;
        if (used >= MaxMoreDraws)
        {
          if (!g_LoggedMoreDrawsFull)
          {
            g_LoggedMoreDrawsFull = true;
            util::log::Warning("Replay: the game's context draws through yet another of d3d11's %s functions (0x%I64X): not hooked "
              "(%d more are the most)", kind == 0 ? "DrawIndexed" : "DrawIndexedInstanced", function, MaxMoreDraws);
          }
          continue;
        }
        int k = used++;
        char name[64];
        sprintf_s(name, "ReplayShapes%s%d", kind == 0 ? "DrawIndexed" : "DrawIndexedInstanced", k + 2);
        bool ok = kind == 0 ?
          util::hooks::CreateGameHook(name, function, DrawIndexedMoreHooks[k], (LPVOID*)&oDrawIndexedMore[k]) :
          util::hooks::CreateGameHook(name, function, DrawIndexedInstancedMoreHooks[k], (LPVOID*)&oDrawIndexedInstancedMore[k]);
        hooked.push_back(function);
        util::log::Write("Replay: the game's context draws through another of d3d11's %s functions (0x%I64X; d3d11 changes them "
          "while it runs): %s", kind == 0 ? "DrawIndexed" : "DrawIndexedInstanced", function, ok ? "hooked too" : "NOT hooked");
      }
    }
  }

  // Render thread, once (the first time a recording captures a shape): the
  // immediate context's draws, and a deferred context's where they're other
  // functions (the game may draw on either); and its render target changes
  // (ReplayGameDraws)
  void HookDraws(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
  {
    g_DrawsHooked = true;
    replay_gamedraws::Hook(pContext);
    void** immediate = *(void***)pContext;
    bool a = util::hooks::CreateGameHook("ReplayShapesDrawIndexed", (__int64)immediate[DrawIndexedSlot], (PVOID)hDrawIndexed,
      (LPVOID*)&oDrawIndexed);
    bool b = util::hooks::CreateGameHook("ReplayShapesDrawIndexedInstanced", (__int64)immediate[DrawIndexedInstancedSlot],
      (PVOID)hDrawIndexedInstanced, (LPVOID*)&oDrawIndexedInstanced);
    // (The functions hooked, for the others d3d11 gives the context later: see MaxMoreDraws)
    g_DrawIndexedHooked.assign(1, (__int64)immediate[DrawIndexedSlot]);
    g_DrawIndexedInstancedHooked.assign(1, (__int64)immediate[DrawIndexedInstancedSlot]);
    int deferredHooks = 0;
    ComPtr<ID3D11DeviceContext> deferred;
    if (SUCCEEDED(pDevice->CreateDeferredContext(0, deferred.GetAddressOf())) && deferred)
    {
      void** table = *(void***)deferred.Get();
      if (table[DrawIndexedSlot] != immediate[DrawIndexedSlot] &&
          util::hooks::CreateGameHook("ReplayShapesDeferredDrawIndexed", (__int64)table[DrawIndexedSlot], (PVOID)hDeferredDrawIndexed,
            (LPVOID*)&oDeferredDrawIndexed))
        ++deferredHooks;
      if (table[DrawIndexedInstancedSlot] != immediate[DrawIndexedInstancedSlot] &&
          util::hooks::CreateGameHook("ReplayShapesDeferredDrawIndexedInstanced", (__int64)table[DrawIndexedInstancedSlot],
            (PVOID)hDeferredDrawIndexedInstanced, (LPVOID*)&oDeferredDrawIndexedInstanced))
        ++deferredHooks;
      // (A deferred context's own: the immediate one may be given it too, it's looked at the same way. Its
      // slot holds 0 when it isn't hooked, so it isn't taken for one)
      g_DrawIndexedHooked.push_back(table[DrawIndexedSlot] != immediate[DrawIndexedSlot] && oDeferredDrawIndexed ?
        (__int64)table[DrawIndexedSlot] : 0);
      g_DrawIndexedInstancedHooked.push_back(table[DrawIndexedInstancedSlot] != immediate[DrawIndexedInstancedSlot] &&
        oDeferredDrawIndexedInstanced ? (__int64)table[DrawIndexedInstancedSlot] : 0);
    }
    util::log::Write("Replay: the game's draws are looked at for the recorded models' colour textures (%s%s; %d deferred "
      "context functions of their own hooked)", a ? "DrawIndexed" : "DrawIndexed NOT hooked", b ? ", DrawIndexedInstanced" :
      ", DrawIndexedInstanced NOT hooked", deferredHooks);
  }

  // The game's buffer copied on the graphics card into one of the replay's
  bool CopyBuffer(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, ID3D11Buffer* pGame, UINT bytes, UINT bind,
    ComPtr<ID3D11Buffer>& out, char const** why)
  {
    D3D11_BUFFER_DESC game{};
    pGame->GetDesc(&game);
    if (game.ByteWidth < bytes)
    {
      *why = "the game's buffer is smaller than its level";
      return false;
    }
    D3D11_BUFFER_DESC desc{};
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = bind;
    desc.ByteWidth = (bytes + 3) & ~3u;
    if (FAILED(pDevice->CreateBuffer(&desc, nullptr, out.GetAddressOf())))
    {
      *why = "a buffer of the replay's couldn't be made";
      return false;
    }
    D3D11_BOX box{ 0, 0, 0, bytes, 1, 1 };
    pContext->CopySubresourceRegion(out.Get(), 0, 0, 0, 0, pGame, 0, &box);
    return true;
  }

  // Render thread: a captured shape's buffers copied from the game's, which
  // are let go then (or once the recording is over: while it records, their
  // addresses are what its draws are known by, see Inspect)
  void Upload(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, Shape& shape)
  {
    shape.Uploaded = true;
    char const* why = "";
    bool ok = shape.GameVb && shape.GameIb &&
      CopyBuffer(pDevice, pContext, shape.GameVb.Get(), shape.VertexBytes, D3D11_BIND_VERTEX_BUFFER, shape.Vb, &why) &&
      CopyBuffer(pDevice, pContext, shape.GameIb.Get(), shape.IndexBytes, D3D11_BIND_INDEX_BUFFER, shape.Ib, &why);
    if (!g_Watching)
    {
      shape.GameVb.Reset();
      shape.GameIb.Reset();
    }
    if (!ok)
    {
      shape.Failed = true;
      shape.Vb.Reset();
      shape.Ib.Reset();
      util::log::Warning("Replay: the shape of '%s' couldn't be copied on the graphics card (%s)", shape.Name, why);
    }
  }

  DXGI_FORMAT TargetFormat(DXGI_FORMAT format)
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

  bool EnsureDepth(ID3D11Device* pDevice, UINT width, UINT height)
  {
    if (g_DepthView && g_DepthWidth == width && g_DepthHeight == height)
      return true;
    g_DepthView.Reset();
    g_DepthTexture.Reset();
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_D32_FLOAT;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    if (FAILED(pDevice->CreateTexture2D(&desc, nullptr, g_DepthTexture.GetAddressOf())) ||
        FAILED(pDevice->CreateDepthStencilView(g_DepthTexture.Get(), nullptr, g_DepthView.GetAddressOf())))
    {
      g_DepthView.Reset();
      g_DepthTexture.Reset();
      return false;
    }
    g_DepthWidth = width;
    g_DepthHeight = height;
    return true;
  }
}

int replay_shapes::Capture(__int64 entity, char const* label, bool quiet)
{
  __int64 meshSet = MeshSetOf(entity);
  if (!meshSet)
  {
    if (!quiet)
      util::log::Write("Replay:   '%s' 0x%I64X: its shape isn't kept (no mesh the replay can read)", label, entity);
    std::lock_guard<std::mutex> lock(g_Mutex);
    ++g_Refused;
    return -1;
  }
  {
    std::lock_guard<std::mutex> lock(g_Mutex);
    for (size_t i = 0; i < g_Shapes.size(); ++i)
      if (g_Shapes[i]->MeshSet == meshSet)
        return (int)i;
    if ((int)g_Shapes.size() >= MaxShapes)
      return -1;
  }
  std::shared_ptr<Shape> shape;
  try
  {
    shape = std::make_shared<Shape>();
  }
  catch (std::bad_alloc const&)
  {
    return -1;
  }
  shape->MeshSet = meshSet;
  char why[160] = "";
  if (!CopyShape(meshSet, *shape, why, sizeof(why)))
  {
    if (!quiet)
      util::log::Write("Replay:   '%s' 0x%I64X: its shape isn't kept (%s)", label, entity, why);
    std::lock_guard<std::mutex> lock(g_Mutex);
    ++g_Refused;
    return -1;
  }
  size_t bytes = (size_t)shape->VertexBytes + shape->IndexBytes;
  UINT triangles = 0;
  for (Section const& s : shape->Sections)
    triangles += s.IndexCount / 3;
  std::lock_guard<std::mutex> lock(g_Mutex);
  if (g_Bytes + bytes > MaxTotalBytes)
  {
    if (!quiet)
      util::log::Write("Replay:   '%s' 0x%I64X: its shape isn't kept (the %u MB kept for the shapes are used)", label, entity,
        (UINT)(MaxTotalBytes >> 20));
    ++g_Refused;
    return -1;
  }
  g_Bytes += bytes;
  g_Shapes.push_back(shape);
  g_CopiesPending = true;
  // Its colour textures from the game's draws of it while it records (see Inspect)
  {
    std::lock_guard<std::mutex> watch(g_WatchMutex);
    g_WatchByVb[shape->GameVbAddress] = shape;
    g_SectionsByVb[shape->GameVbAddress] = { (int)g_Shapes.size() - 1, shape };
  }
  replay_drawlog::NoteMesh(shape->GameVbAddress, shape->GameIbAddress, shape->Name);
  g_Watching = true;
  replay_gamedraws::SetCapturing(true);
  if (!quiet)
    util::log::Write("Replay:   '%s' 0x%I64X: its shape kept (%s, level %d, %d sections, %u triangles, %u KB)", label, entity,
      shape->Name, shape->Lod, (int)shape->Sections.size(), triangles, (UINT)(bytes >> 10));
  return (int)g_Shapes.size() - 1;
}

int replay_shapes::ModelMeshBuffers(__int64 entity, std::vector<__int64>& vbs)
{
  __int64 data = Q(entity + EntityDataOffset);
  __int64 asset = data ? Q(data + StaticModelMeshOffset) : 0;
  return LevelVertexBuffers(asset ? Q(asset + MeshAssetMeshSetOffset) : 0, vbs);
}

int replay_shapes::MeshSetBuffersAt(__int64 at, std::vector<__int64>& vbs)
{
  return LevelVertexBuffers(Q(at), vbs);
}

int replay_shapes::MeshBuffersNamed(char const* prefix, std::vector<__int64>& vbs, std::vector<std::string>* names)
{
  vbs.clear();
  if (names)
    names->clear();
  __int64 base = (__int64)g_gameHandle;
  __int64 manager = Q(base + MeshManagerGlobalRva);
  __int64 begin = manager ? Q(manager + MeshTableOffset) : 0, end = manager ? Q(manager + MeshTableOffset + 8) : 0;
  if (!begin || end < begin)
    return 0;
  __int64 count = (std::min)((end - begin) / MeshRecordSize, MaxMeshRecords);
  size_t length = strlen(prefix);
  int meshes = 0;
  std::vector<__int64> levels;
  for (__int64 i = 0; i < count; ++i)
  {
    __int64 meshSet = Q(begin + i * MeshRecordSize);
    if (!meshSet || Q(meshSet) != base + MeshSetVTableRva)
      continue;
    __int64 data = Q(meshSet + MeshSetDataOffset);
    char name[160];
    ReadName(data ? Q(data + DataNameOffset) : 0, name, sizeof(name));
    if (!name[0] || _strnicmp(name, prefix, length))
      continue;
    if (!LevelVertexBuffers(meshSet, levels))
      continue;
    vbs.insert(vbs.end(), levels.begin(), levels.end());
    ++meshes;
    if (names)
      names->push_back(name);
  }
  return meshes;
}

void replay_shapes::EndCapture()
{
  replay_gamedraws::SetCapturing(false);
  if (!g_Watching.exchange(false))
    return;
  {
    std::lock_guard<std::mutex> watch(g_WatchMutex);
    g_WatchByVb.clear();
    g_SectionsByVb.clear();
    g_AltByVb.clear();
  }
  g_ReleaseGameBuffers = true;
  int sections = 0, drawable = 0;
  {
    std::lock_guard<std::mutex> lock(g_Mutex);
    for (auto const& shape : g_Shapes)
      for (Section const& s : shape->Sections)
      {
        ++sections;
        drawable += s.TcFormat != 0;
      }
  }
  util::log::Write("Replay: colour textures kept for %d of the %d sections of the recorded models' shapes (%d with texture "
    "coordinates the replay reads); the others are drawn in one colour", g_TexturesKept.load(), sections, drawable);
}

int replay_shapes::Refused()
{
  std::lock_guard<std::mutex> lock(g_Mutex);
  return g_Refused;
}

void replay_shapes::SetDraws(Draw const* draws, int count)
{
  int kept = (std::max)(0, (std::min)(count, MaxDraws));
  {
    std::lock_guard<std::mutex> lock(g_Mutex);
    g_Draws.assign(draws, draws + kept);
  }
  // The ones whose parts the game's own draws were kept for are drawn with those (ReplayGameDraws)
  replay_gamedraws::SetDraws(draws, kept);
}

void replay_shapes::Clear()
{
  g_Watching = false;
  replay_gamedraws::SetCapturing(false);
  replay_gamedraws::Clear();
  {
    std::lock_guard<std::mutex> watch(g_WatchMutex);
    g_WatchByVb.clear();
    g_SectionsByVb.clear();
    g_AltByVb.clear();
    g_AltIndex.clear();
    g_AltNext.clear();
  }
  g_TexturesKept = 0;
  g_TextureLogs = 0;
  g_NoColourLogs = 0;
  std::lock_guard<std::mutex> lock(g_Mutex);
  g_Shapes.clear();
  g_Draws.clear();
  g_Bytes = 0;
  g_Refused = 0;
}

void replay_shapes::NoteContextDraws(ID3D11DeviceContext* pContext)
{
  if (!g_DrawsHooked || !pContext)
    return;
  void** table = *(void***)pContext;
  __int64 drawIndexed = (__int64)table[DrawIndexedSlot], drawIndexedInstanced = (__int64)table[DrawIndexedInstancedSlot];
  if (std::find(g_DrawIndexedHooked.begin(), g_DrawIndexedHooked.end(), drawIndexed) == g_DrawIndexedHooked.end())
    g_DrawIndexedSeen = drawIndexed;
  if (std::find(g_DrawIndexedInstancedHooked.begin(), g_DrawIndexedInstancedHooked.end(), drawIndexedInstanced) ==
      g_DrawIndexedInstancedHooked.end())
    g_DrawIndexedInstancedSeen = drawIndexedInstanced;
}

void replay_shapes::WantDrawHooks()
{
  g_WantHooks = true;
}

void replay_shapes::MapLevels()
{
  if (!g_Watching)
    return;
  std::vector<std::shared_ptr<Shape>> shapes;
  {
    std::lock_guard<std::mutex> lock(g_Mutex);
    shapes = g_Shapes;
  }
  __int64 base = (__int64)g_gameHandle;
  std::unordered_map<__int64, LevelOf> levels;
  for (size_t i = 0; i < shapes.size(); ++i)
  {
    Shape const& shape = *shapes[i];
    __int64 meshSet = shape.MeshSet;
    if (!meshSet || Q(meshSet) != base + MeshSetVTableRva)
      continue;
    __int64 data = Q(meshSet + MeshSetDataOffset);
    for (int lod = 0; lod < MaxLods - 1 && data && Q(data + DataLevelsOffset + lod * 8); ++lod)
    {
      if (lod == shape.Lod)
        continue;
      __int64 buffer = Q(meshSet + MeshSetLevelsOffset + lod * MeshSetLevelSize + 8);
      if (!buffer || Q(buffer) != base + RenderBufferVTableRva || D(buffer + BufferKindOffset) != 1)
        continue;   // that level isn't loaded
      __int64 d3d = Q(buffer + BufferD3DOffset);
      if (!d3d || d3d == shape.GameVbAddress)
        continue;
      // Its parts drawn, as CopyShape counts them
      int parts = 0;
      __int64 levelData = Q(data + DataLevelsOffset + lod * 8);
      UINT sectionCount = levelData ? D(levelData + LevelSectionCountOffset) : 0;
      __int64 sections = levelData ? Q(levelData + LevelSectionsOffset) : 0;
      if (sectionCount && sectionCount <= MaxSections && sections)
        for (UINT k = 0; k < sectionCount; ++k)
        {
          char material[64];
          ReadName(Q(sections + (__int64)k * SectionSize + SectionMaterialOffset), material, sizeof(material));
          parts += IsDrawnSection(material);
        }
      LevelOf level{ (int)i, lod, parts <= MaxAltParts ? parts : 0, "" };
      strncpy_s(level.Name, shape.Name, _TRUNCATE);
      levels[d3d] = level;
    }
  }
  std::lock_guard<std::mutex> watch(g_WatchMutex);
  g_AltByVb.swap(levels);
}

int replay_shapes::MeshTable(std::unordered_map<__int64, MeshLevel>& out)
{
  out.clear();
  __int64 base = (__int64)g_gameHandle;
  __int64 manager = Q(base + MeshManagerGlobalRva);
  __int64 begin = manager ? Q(manager + MeshTableOffset) : 0, end = manager ? Q(manager + MeshTableOffset + 8) : 0;
  if (!begin || end < begin)
    return 0;
  __int64 count = (std::min)((end - begin) / MeshRecordSize, MaxMeshRecords);
  int meshes = 0;
  for (__int64 i = 0; i < count; ++i)
  {
    __int64 meshSet = Q(begin + i * MeshRecordSize);
    if (!meshSet || Q(meshSet) != base + MeshSetVTableRva)
      continue;
    __int64 data = Q(meshSet + MeshSetDataOffset);
    char name[160];
    ReadName(data ? Q(data + DataNameOffset) : 0, name, sizeof(name));
    if (!name[0])
      continue;
    // Its bounding box: the MeshSet res starts with it (min, max: two float4;
    // Research_Shard_Streaming section 6, its decoded vertices all inside it),
    // and the data as loaded is that res (its levels at + 0x20 and name at
    // + 0x58 are the res's)
    float box[8] = {};
    float reach = 0.f;
    if (data && ReadRaw(box, data, sizeof(box)))
    {
      bool plausible = true;
      float far2 = 0.f;
      for (int c = 0; c < 3 && plausible; ++c)
      {
        float lo = box[c], hi = box[4 + c];
        plausible = std::isfinite(lo) && std::isfinite(hi) && lo <= hi && fabsf(lo) < MaxBoxMetres && fabsf(hi) < MaxBoxMetres;
        float m = (std::max)(fabsf(lo), fabsf(hi));
        far2 += m * m;
      }
      if (plausible)
        reach = sqrtf(far2);
    }
    bool any = false;
    for (int lod = 0; lod < MaxLods && data && Q(data + DataLevelsOffset + lod * 8); ++lod)
    {
      __int64 buffer = Q(meshSet + MeshSetLevelsOffset + lod * MeshSetLevelSize + 8);
      if (!buffer || Q(buffer) != base + RenderBufferVTableRva || D(buffer + BufferKindOffset) != 1)
        continue;   // that level isn't loaded
      __int64 d3d = Q(buffer + BufferD3DOffset);
      if (!d3d)
        continue;
      out[d3d] = MeshLevel{ name, lod, reach };
      any = true;
    }
    meshes += any;
  }
  return meshes;
}

void replay_shapes::MeshTableKey(std::vector<__int64>& out)
{
  out.clear();
  __int64 base = (__int64)g_gameHandle;
  __int64 manager = Q(base + MeshManagerGlobalRva);
  __int64 begin = manager ? Q(manager + MeshTableOffset) : 0, end = manager ? Q(manager + MeshTableOffset + 8) : 0;
  if (!begin || end < begin)
    return;
  __int64 count = (std::min)((end - begin) / MeshRecordSize, MaxMeshRecords);
  for (__int64 i = 0; i < count; ++i)
  {
    __int64 meshSet = Q(begin + i * MeshRecordSize);
    if (!meshSet || Q(meshSet) != base + MeshSetVTableRva)
      continue;
    __int64 data = Q(meshSet + MeshSetDataOffset);
    out.push_back(meshSet);
    out.push_back(data);
    for (int lod = 0; lod < MaxLods && data && Q(data + DataLevelsOffset + lod * 8); ++lod)
    {
      __int64 buffer = Q(meshSet + MeshSetLevelsOffset + lod * MeshSetLevelSize + 8);
      if (!buffer || Q(buffer) != base + RenderBufferVTableRva || D(buffer + BufferKindOffset) != 1)
        continue;
      out.push_back(lod);
      out.push_back(Q(buffer + BufferD3DOffset));
    }
  }
}

// The skeleton's bones a part of a skinned mesh is drawn with (2026-11-03,
// the user: "ghost runner head still stays at place instead of following the
// body"). A level's section + 0x40 its bones a vertex, + 0x41 how many bone
// matrices its draw has, + 0x44 (unaligned) a list of that many 16-bit
// skeleton bones in the matrices' order. Read from the game's files (the
// MeshSet res is the data as loaded, see MeshTable): the ghost runner's
// (characters/projection/female_runner_projection_01/fullbody: 80 bones, the
// first 54 52 7), the enforcers', the subprotector's and the hunter drone's;
// the enforcers' counts are the ones the replay matched in the 10:56 test.
// Found by the game's vertex buffer of the level (the mesh table looked
// through again for one not known, at most once a second), the section by its
// first index and count. Game thread.
namespace
{
  const int SectionTrianglesOffset = 0x20, SectionFirstIndexOffset = 0x24;
  const int SectionBoneCountOffset = 0x41, SectionBoneListOffset = 0x44;
  struct LevelAt
  {
    __int64 MeshSet;
    int Lod;
  };
  std::unordered_map<__int64, LevelAt> g_LevelByVb;   // game thread
  ULONGLONG g_LevelByVbAt = 0;
}

bool replay_shapes::SectionBones(__int64 vb, unsigned startIndex, unsigned indexCount, std::vector<int>& bones)
{
  bones.clear();
  __int64 base = (__int64)g_gameHandle;
  // The level's data while that buffer is still the level's
  auto levelData = [base, vb](LevelAt const& at) -> __int64 {
    if (!at.MeshSet || Q(at.MeshSet) != base + MeshSetVTableRva || at.Lod < 0 || at.Lod >= MaxLods)
      return 0;
    __int64 buffer = Q(at.MeshSet + MeshSetLevelsOffset + at.Lod * MeshSetLevelSize + 8);
    if (!buffer || Q(buffer) != base + RenderBufferVTableRva || D(buffer + BufferKindOffset) != 1 || Q(buffer + BufferD3DOffset) != vb)
      return 0;
    __int64 data = Q(at.MeshSet + MeshSetDataOffset);
    return data ? Q(data + DataLevelsOffset + at.Lod * 8) : 0;
  };
  auto it = g_LevelByVb.find(vb);
  __int64 level = it != g_LevelByVb.end() ? levelData(it->second) : 0;
  if (!level)
  {
    ULONGLONG now = GetTickCount64();
    if (now - g_LevelByVbAt < 1000)
      return false;
    g_LevelByVbAt = now;
    g_LevelByVb.clear();
    __int64 manager = Q(base + MeshManagerGlobalRva);
    __int64 begin = manager ? Q(manager + MeshTableOffset) : 0, end = manager ? Q(manager + MeshTableOffset + 8) : 0;
    if (!begin || end < begin)
      return false;
    __int64 count = (std::min)((end - begin) / MeshRecordSize, MaxMeshRecords);
    for (__int64 i = 0; i < count; ++i)
    {
      __int64 meshSet = Q(begin + i * MeshRecordSize);
      if (!meshSet || Q(meshSet) != base + MeshSetVTableRva)
        continue;
      __int64 data = Q(meshSet + MeshSetDataOffset);
      for (int lod = 0; lod < MaxLods && data && Q(data + DataLevelsOffset + lod * 8); ++lod)
      {
        __int64 buffer = Q(meshSet + MeshSetLevelsOffset + lod * MeshSetLevelSize + 8);
        if (!buffer || Q(buffer) != base + RenderBufferVTableRva || D(buffer + BufferKindOffset) != 1)
          continue;
        if (__int64 d3d = Q(buffer + BufferD3DOffset))
          g_LevelByVb[d3d] = LevelAt{ meshSet, lod };
      }
    }
    it = g_LevelByVb.find(vb);
    level = it != g_LevelByVb.end() ? levelData(it->second) : 0;
    if (!level)
      return false;
  }
  UINT sectionCount = D(level + LevelSectionCountOffset);
  __int64 sections = Q(level + LevelSectionsOffset);
  if (!sectionCount || sectionCount > MaxSections || !sections)
    return false;
  for (UINT k = 0; k < sectionCount; ++k)
  {
    __int64 s = sections + (__int64)k * SectionSize;
    if (D(s + SectionFirstIndexOffset) != startIndex || (unsigned long long)D(s + SectionTrianglesOffset) * 3 != indexCount)
      continue;
    BYTE n = 0;
    __int64 list = 0;
    unsigned short values[256];
    if (!ReadRaw(&n, s + SectionBoneCountOffset, 1) || !n || !ReadRaw(&list, s + SectionBoneListOffset, sizeof(list)) ||
        !ReadRaw(values, list, n * sizeof(unsigned short)))
      return false;
    bones.assign(values, values + n);
    return true;
  }
  return false;
}

void replay_shapes::ShapeNames(std::set<std::string>& out)
{
  out.clear();
  std::lock_guard<std::mutex> lock(g_Mutex);
  for (auto const& shape : g_Shapes)
    if (shape->Name[0])
      out.insert(shape->Name);
}

void replay_shapes::TakeHookCost(long long& ticks, long long& draws)
{
  ticks = g_HookTicks.exchange(0);
  draws = g_HookDraws.exchange(0);
}

std::string replay_shapes::NameOf(int shape)
{
  std::lock_guard<std::mutex> lock(g_Mutex);
  return shape >= 0 && shape < (int)g_Shapes.size() ? std::string(g_Shapes[shape]->Name) : std::string();
}

int replay_shapes::Count()
{
  std::lock_guard<std::mutex> lock(g_Mutex);
  return (int)g_Shapes.size();
}

void replay_shapes::Save(replay_store::Out& out, replay_store::SaveParts& parts)
{
  std::vector<std::shared_ptr<Shape>> shapes;
  {
    std::lock_guard<std::mutex> lock(g_Mutex);
    shapes = g_Shapes;
  }
  out.Put((unsigned)shapes.size());
  for (auto const& kept : shapes)
  {
    Shape const& shape = *kept;
    bool ok = !shape.Failed && shape.Uploaded && shape.Vb && shape.Ib;
    int vb = ok ? replay_store::AddPart(parts, shape.Vb.Get()) : -1;
    int ib = ok ? replay_store::AddPart(parts, shape.Ib.Get()) : -1;
    ok = ok && vb >= 0 && ib >= 0;
    out.Put((unsigned char)ok);
    out.Bytes(shape.Name, sizeof(shape.Name));
    out.Put(shape.Lod);
    out.Put((unsigned char)shape.Index32);
    out.Put(shape.VertexBytes);
    out.Put(shape.IndexBytes);
    out.Vec(shape.Sections);
    out.Put(ok ? vb : -1);
    out.Put(ok ? ib : -1);
    for (size_t s = 0; s < shape.Sections.size(); ++s)
    {
      ID3D11ShaderResourceView* view = shape.Textures ? shape.Textures[s].load() : nullptr;
      int id = view ? replay_store::AddPart(parts, view) : -1;
      out.Put(id >= 0 ? id : -1);
    }
  }
  // The sections' passes (2026-11-01; see PassSeenOther), after all the
  // shapes: an older file has none and an older build doesn't read it
  out.Put(ShapePassesTag);
  for (auto const& kept : shapes)
    for (size_t s = 0; s < kept->Sections.size(); ++s)
      out.Put((unsigned char)(kept->Passes ? kept->Passes[s].load() : 0));
}

bool replay_shapes::Load(replay_store::In& in, replay_store::LoadParts const& parts)
{
  unsigned count = 0;
  if (!in.Get(count) || count > (unsigned)MaxShapes)
    return false;
  std::vector<std::shared_ptr<Shape>> shapes;
  size_t bytes = 0;
  try
  {
    for (unsigned k = 0; k < count; ++k)
    {
      auto shape = std::make_shared<Shape>();
      unsigned char ok = 0, index32 = 0;
      int vb = -1, ib = -1;
      if (!in.Get(ok) || !in.Bytes(shape->Name, sizeof(shape->Name)) || !in.Get(shape->Lod) || !in.Get(index32) ||
          !in.Get(shape->VertexBytes) || !in.Get(shape->IndexBytes) || !in.Vec(shape->Sections, MaxSections) || !in.Get(vb) ||
          !in.Get(ib))
        return false;
      shape->Name[sizeof(shape->Name) - 1] = 0;
      shape->Index32 = index32 != 0;
      size_t sections = shape->Sections.size();
      shape->Textures.reset(new std::atomic<ID3D11ShaderResourceView*>[sections ? sections : 1]);
      shape->Passes.reset(new std::atomic<unsigned char>[sections ? sections : 1]);
      for (size_t s = 0; s < (sections ? sections : 1); ++s)
      {
        shape->Textures[s] = nullptr;
        shape->Passes[s] = 0;
      }
      for (size_t s = 0; s < sections; ++s)
      {
        int id = -1;
        if (!in.Get(id))
          return false;
        ComPtr<ID3D11ShaderResourceView> view = replay_store::PartAs<ID3D11ShaderResourceView>(parts, id);
        shape->Textures[s] = view.Detach();
      }
      // Its copies as the load made them: nothing to copy from the game's
      shape->Vb = replay_store::PartAs<ID3D11Buffer>(parts, vb);
      shape->Ib = replay_store::PartAs<ID3D11Buffer>(parts, ib);
      shape->Failed = !ok || !shape->Vb || !shape->Ib;
      shape->Uploaded = true;
      bytes += (size_t)shape->VertexBytes + shape->IndexBytes;
      shapes.push_back(std::move(shape));
    }
  }
  catch (std::bad_alloc const&)
  {
    return false;
  }
  // The sections' passes, when the file has them (see Save)
  unsigned tag = 0;
  if (in.Left() >= sizeof(tag) && in.Get(tag) && tag == ShapePassesTag)
    for (auto& shape : shapes)
      for (size_t s = 0; s < shape->Sections.size(); ++s)
      {
        unsigned char seen = 0;
        if (!in.Get(seen))
          break;
        shape->Passes[s] = seen;
      }
  std::lock_guard<std::mutex> lock(g_Mutex);
  g_Shapes.swap(shapes);
  g_Draws.clear();
  g_Bytes = bytes;
  g_CopiesPending = false;
  return true;
}

size_t replay_shapes::Bytes()
{
  std::lock_guard<std::mutex> lock(g_Mutex);
  return g_Bytes;
}

void replay_shapes::Apply(IDXGISwapChain* pSwapChain, ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
  // The parts the game's own draws drew in this frame (ReplayGameDraws): not drawn as shapes
  std::set<long long> drawnByGame;
  replay_gamedraws::TakeDrawn(drawnByGame);
  // This frame's list, and the shapes it draws (kept alive while drawn)
  std::vector<Draw> draws;
  std::vector<std::shared_ptr<Shape>> shapes;
  bool copies = false;
  bool release = g_ReleaseGameBuffers.exchange(false);
  {
    std::lock_guard<std::mutex> lock(g_Mutex);
    for (Draw const& d : g_Draws)
      if (d.Shape >= 0 && d.Shape < (int)g_Shapes.size())
        draws.push_back(d);
    copies = g_CopiesPending;
    g_CopiesPending = false;
    if (copies || release || !draws.empty())
      shapes = g_Shapes;
  }
  if (!pSwapChain || !pDevice || !pContext)
    return;
  // The game's draws looked at while a recording captures shapes (their colour textures)
  if ((g_Watching || replay_drawlog::Active() || g_WantHooks) && !g_DrawsHooked)
    HookDraws(pDevice, pContext);
  // Any other draw function d3d11 gave the game's context since (see MaxMoreDraws)
  if (g_DrawsHooked)
    MoreDrawFunctions(pContext);
  // Captured shapes: copied from the game's buffers now, while they're held
  if (copies)
    for (auto const& shape : shapes)
      if (!shape->Uploaded)
        Upload(pDevice, pContext, *shape);
  // The recording is over: the game's buffers held for its draws let go
  if (release)
    for (auto const& shape : shapes)
      if (shape->Uploaded)
      {
        shape->GameVb.Reset();
        shape->GameIb.Reset();
      }
  greenplane::WantDepth(!draws.empty(), greenplane::WantReplayShapes);
  if (draws.empty() || g_Failed)
    return;
  greenplane::FrameDepth frame;
  if (!greenplane::TakeFrameDepth(pSwapChain, pDevice, pContext, frame))
    return;
  if (!g_Made && !Make(pDevice))
    return;
  ComPtr<ID3D11Texture2D> pBack;
  if (FAILED(pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)pBack.GetAddressOf())) || !pBack)
    return;
  D3D11_TEXTURE2D_DESC back{};
  pBack->GetDesc(&back);
  if (back.SampleDesc.Count != 1 || !back.Width || !back.Height || !EnsureDepth(pDevice, back.Width, back.Height))
    return;
  ComPtr<ID3D11RenderTargetView> pTarget;
  D3D11_RENDER_TARGET_VIEW_DESC targetDesc{};
  targetDesc.Format = TargetFormat(back.Format);
  targetDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
  if (FAILED(pDevice->CreateRenderTargetView(pBack.Get(), targetDesc.Format != back.Format ? &targetDesc : nullptr,
      pTarget.GetAddressOf())))
    return;
  for (Draw const& d : draws)
  {
    Shape& shape = *shapes[d.Shape];
    if (!shape.Uploaded)
      Upload(pDevice, pContext, shape);
  }

  float const* t = frame.Transform;
  float const* projection = frame.Projection;
  XMMATRIX cameraMatrix = XMLoadFloat4x4((XMFLOAT4X4 const*)t);
  XMMATRIX viewProjection = XMMatrixInverse(nullptr, cameraMatrix) * XMLoadFloat4x4((XMFLOAT4X4 const*)projection);
  XMVECTOR camera = XMVectorSet(t[12], t[13], t[14], 0.f);
  XMVECTOR light = XMVector3Normalize(XMVectorSet(LightFrom[0], LightFrom[1], LightFrom[2], 0.f));

  g_Backup.Save(pContext);
  D3D11_VIEWPORT viewport{ 0.f, 0.f, (float)back.Width, (float)back.Height, 0.f, 1.f };
  const float blendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
  pContext->ClearDepthStencilView(g_DepthView.Get(), D3D11_CLEAR_DEPTH, frame.Reversed ? 0.f : 1.f, 0);
  pContext->OMSetRenderTargets(1, pTarget.GetAddressOf(), g_DepthView.Get());
  pContext->RSSetViewports(1, &viewport);
  pContext->RSSetState(g_Rasterizer.Get());
  pContext->OMSetBlendState(g_Blend.Get(), blendFactor, 0xFFFFFFFF);
  pContext->OMSetDepthStencilState(frame.Reversed ? g_DepthReversed.Get() : g_DepthNormal.Get(), 0);
  pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  pContext->VSSetShader(g_Vs.Get(), nullptr, 0);
  pContext->VSSetConstantBuffers(0, 1, g_Constants.GetAddressOf());
  pContext->GSSetShader(nullptr, nullptr, 0);
  pContext->HSSetShader(nullptr, nullptr, 0);
  pContext->DSSetShader(nullptr, nullptr, 0);
  pContext->PSSetShader(g_Ps.Get(), nullptr, 0);
  pContext->PSSetConstantBuffers(0, 1, g_Constants.GetAddressOf());
  pContext->PSSetShaderResources(0, 1, &frame.View);
  pContext->PSSetSamplers(0, 1, g_Sampler.GetAddressOf());

  for (Draw const& d : draws)
  {
    Shape& shape = *shapes[d.Shape];
    if (shape.Failed || !shape.Vb || !shape.Ib)
      continue;
    float const* m = d.Transform;
    XMFLOAT4X4 world(m[0], m[1], m[2], 0.f, m[4], m[5], m[6], 0.f, m[8], m[9], m[10], 0.f, m[12], m[13], m[14], 1.f);
    float distance = XMVectorGetX(XMVector3Length(XMVectorSet(m[12], m[13], m[14], 0.f) - camera));
    // A model that didn't move, which the game still has but doesn't draw,
    // with the camera right next to it: the game doesn't draw a prop that
    // near the camera (it didn't while it recorded either), so neither does
    // the replay (2026-11-03, the mission test's start points 9 and 20: an
    // air conditioner's one-colour copy over half the picture, where the
    // recording shows open street)
    if (d.Still && d.LiveCopy && distance < NearCameraSkipMetres)
      continue;
    // A Dash's marker: the game draws it only into its runner's vision mask,
    // and the replay does the same with the game's own draws of it (see
    // replay_gamedraws::ApplyRunnersVision); as a lit shape it would be a
    // solid object with another texture (2026-11-08, the user: "Replay
    // arrow/starting are wrongly textured")
    if (d.RvLayer)
      continue;
    pContext->IASetIndexBuffer(shape.Ib.Get(), shape.Index32 ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT, 0);
    for (size_t si = 0; si < shape.Sections.size(); ++si)
    {
      Section const& s = shape.Sections[si];
      if (drawnByGame.count(((long long)d.Shape << 16) | (long long)si) || drawnByGame.count(((long long)d.Shape << 16) | 0xFFFF))
        continue;
      // Drawn by the game only in its decal or transparent passes: not as an
      // opaque shape (see PassSeenOther)
      if (OnlyOtherPasses(shape, si))
        continue;
      ID3D11InputLayout* layout = LayoutFor(pDevice, s);
      if (!layout)
        continue;
      // Its colour texture when one was kept (and it has texture coordinates)
      ID3D11ShaderResourceView* texture = shape.Textures && s.TcFormat ? shape.Textures[si].load() : nullptr;
      D3D11_MAPPED_SUBRESOURCE mapped;
      if (FAILED(pContext->Map(g_Constants.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        continue;
      Constants* c = (Constants*)mapped.pData;
      XMStoreFloat4x4(&c->ViewProjection, viewProjection);
      c->World = world;
      c->Depth = XMFLOAT4(frame.Scale[0], frame.Scale[1], 0.f, -(DepthNearer + DepthNearerPerMetre * distance));
      c->Projection = XMFLOAT4(projection[10], projection[11], projection[14], projection[15]);
      XMStoreFloat4(&c->Light, light);
      c->Light.w = Ambient;
      c->Color = XMFLOAT4(Colour[0], Colour[1], Colour[2], texture ? 1.f : 0.f);
      pContext->Unmap(g_Constants.Get(), 0);
      pContext->PSSetShaderResources(1, 1, &texture);
      ID3D11Buffer* vb = shape.Vb.Get();
      UINT stride = s.Stride, offset = s.VertexOffset;
      pContext->IASetInputLayout(layout);
      pContext->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
      pContext->DrawIndexed(s.IndexCount, s.StartIndex, 0);
    }
  }

  // The colour texture's slot isn't in the backup: emptied
  ID3D11ShaderResourceView* none[2] = { nullptr, nullptr };
  pContext->PSSetShaderResources(0, 2, none);
  g_Backup.Restore(pContext);
}
