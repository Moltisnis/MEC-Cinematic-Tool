#include "ReplayAssetLog.h"
#include "ReplayGameDraws.h"
#include "../Main.h"
#include "../Util/Util.h"
#include "../Visuals/Weather.h"
#include <Windows.h>
#include <algorithm>
#include <atomic>
#include <cfloat>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <map>
#include <mutex>
#include <set>
#include <share.h>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Diagnostic (2026-10-27). How:
// - The meshes: the mesh streaming manager ([0x142563410]) keeps every mesh it
//   has in a table of 0x350-byte records (begin / end at + 0x50); a record in
//   use has its MeshSet in its first qword (Hooks.cpp hMeshManagerCtor,
//   CameraManager CountMeshesInUse; read live 2026-10-27 02:5x,
//   Research_Replay_Mission_World_2026-10-26.md section 10). The MeshSet
//   (vtable 0x141BBF880) + 0x20 is its data: + 0x20 a pointer for each detail
//   level, + 0x58 its full name; the MeshSet + 0x50 + level * 0x50 + 8 a
//   loaded level's vertex buffer (Dx11RenderBuffer, vtable 0x141BD6840: + 0x10
//   the ID3D11Buffer, + 0x44 1 for vertices). Read once a second with guarded
//   reads, a MeshSet's name once (again if its data changed).
// - The textures the game makes: the device's CreateTexture2D (slot 5) while
//   it records, those a shader reads that aren't render targets, depth or
//   unordered access: their size, format and detail levels, and the game's
//   name for one if it gave its D3D object one (WKPDID_D3DDebugObjectName;
//   not known whether it does). The tools' own such textures are among them.
// - The draws: every SampleEvery frames, each draw in the game's main G-buffer
//   pass (ReplayGameDraws' test, the immediate context), its first vertex
//   buffer (a mesh's level) and its pixel shader's first ViewSlots textures;
//   the replay's own draws there the same way. A vertex buffer is named by the
//   mesh table at the next look (one the replay holds after the game let go of
//   it: by what it was while recording). The draw hooks are the replay's own,
//   put in when a recording first keeps a model's shape (ReplayShapes): draws
//   before that aren't seen.
// - When playback stops: each mesh drawn while recording at a second the
//   playback had too (within a second either way) that the playback drew at
//   none of them (missing) or only some, and each drawn while recording with a
//   texture of a size and format it wasn't drawn with when it played. Textures
//   go by each draw of the mesh (its first index and index count, 2026-10-27
//   19:5x: a vertex buffer can be shared by many meshes, each with its own
//   textures), slot by slot (2026-10-27 20:4x: a draw's textures over the
//   whole recording mixed every streamed version of them): a draw drawn both
//   times with a slot whose texture has another format or shape is "other
//   textures", one whose slots only differ in their biggest size of each kind
//   (a level streamed in or out) "another resolution". The meshes the
//   replay drew itself are listed too.
// - The bundles (Research_Mission_Loading_2026-10-27.md): log-only hooks on
//   the game's bundle manager, byte-checked: 0x142ABA450 registers a bundle
//   by name (it gets a compartment, a slot of the table 0x14237DEB8: + 0x70
//   its name), 0x142ABA6E0 loads a compartment's bundle, 0x142ABA6B0 unloads
//   one. Each call while it records or plays is noted (name, parent, kind,
//   compartment); the compartments in the table as it starts recording too.
// Nothing of the game's is written. A camera that looks elsewhere than the
// recording's draws other things: what's out of its view shows as missing.
namespace
{
  const __int64 MeshManagerGlobalRva = 0x2563410;
  const int MeshTableOffset = 0x50;
  const int MeshRecordSize = 0x350;
  const __int64 MaxRecords = 16384 + 64;
  const __int64 MeshSetVTableRva = 0x1BBF880;
  const __int64 RenderBufferVTableRva = 0x1BD6840;
  const int MeshSetDataOffset = 0x20, DataLevelsOffset = 0x20, DataNameOffset = 0x58;
  const int MeshSetLevelsOffset = 0x50, MeshSetLevelSize = 0x50, MaxLods = 7;
  const int BufferD3DOffset = 0x10, BufferKindOffset = 0x44;
  const int CreateTexture2DSlot = 5;    // ID3D11Device
  const unsigned SampleEvery = 15;      // frames whose draws are looked at
  const int ViewSlots = 8;              // the pixel shader's textures looked at
  const ULONGLONG ScanEveryMs = 1000;
  const size_t MaxPending = 400000;     // draw notes waiting for the game thread
  const size_t MaxOtherPerFrame = 8192; // draws in other passes noted in a sampled frame (see g_FrameOther)
  const size_t MaxPendingOther = 400000;
  const size_t MaxCreated = 200000;
  const int MaxLogNames = 40;           // in CT.log; all of them in the file
  const char* const FileName = ".\\Cinematic Tools\\ReplayAssets.log";
  const __int64 BundleRegisterRva = 0x2ABA450, BundleLoadRva = 0x2ABA6E0, BundleUnloadRva = 0x2ABA6B0;
  const BYTE BundleRegisterBytes[] = { 0x48, 0x83, 0xEC, 0x48, 0x8B, 0x44, 0x24, 0x78, 0x89, 0x44, 0x24, 0x30 };
  const BYTE BundleLoadBytes[] = { 0x53, 0x48, 0x83, 0xEC, 0x50, 0x48, 0x89, 0xCB, 0xF0, 0xFF, 0x05 };
  const BYTE BundleUnloadBytes[] = { 0x53, 0x48, 0x83, 0xEC, 0x20, 0x45, 0x89, 0xC1, 0x44, 0x0F, 0xB7, 0xC2 };
  const __int64 CompartmentTableRva = 0x237DEB8;
  const int CompartmentNameOffset = 0x70;
  const int Compartments = 3005;        // 0xBBD: none
  // Entity creation, read only (2026-10-28; the user: "build the read-only step: log the game's creation
  // info for static models and prefabs, and find where loaded partitions are kept by guid";
  // Research_Mission_Prefabs_2026-10-28.md). The client's entity factories (constructor 0x14315FED0):
  // + 0x18 their data's TypeInfo, + 0x24 their realm (0 the client). Their create: the shared one of
  // the static models' and 38 other kinds' factories 0x143163210(factory, info), and the reference
  // kinds' 0x1431C3F40(factory, info) (ReferenceObjectData, SpatialReferenceObjectData,
  // ObjectReferenceObjectData, SpatialPrefabReferenceObjectData). The info, as far as seen: + 0x10 a
  // context, + 0x30 an int, + 0xD0 the data, + 0xE1 a byte the client's create compares; its size
  // isn't known, so InfoBytes are logged.
  const __int64 CreateGenericRva = 0x3163210, CreateReferenceRva = 0x31C3F40;
  const BYTE CreateGenericBytes[] = { 0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83, 0xEC, 0x20, 0x83, 0x79, 0x24, 0x00 };
  const BYTE CreateReferenceBytes[] = { 0x53, 0x48, 0x83, 0xEC, 0x30, 0x48, 0x8D, 0x4C, 0x24, 0x20, 0xE8 };
  const __int64 StaticModelDataTypeRva = 0x2820A90;   // StaticModelEntityData's TypeInfo
  const int FactoryTypeOffset = 0x18, FactoryRealmOffset = 0x24;
  const int InfoDataOffset = 0xD0, InfoContextOffset = 0x10;
  const int InfoBytes = 0x100;
  const int StaticModelMeshOffset = 0x98;             // StaticModelEntityData.Mesh (reflected)
  const int ReferenceBlueprintOffset = 0x18, ReferenceTransformOffset = 0x20;   // ReferenceObjectData (reflected)
  const int AssetNameOffset = 0x10;                   // Asset.Name (reflected)
  const int MaxCreateDumps = 3, MaxCreateLines = 160, MaxReferenceDumps = 4, MaxReferenceLines = 80;
  // Partitions by guid (seen: 0x142AEB5E0, slot 16 of a registry's vtable): a compartment's entry
  // + 0x68 its registry (0x142ABAD00), the registry's by-guid map: its buckets at + 0x130, their
  // count at + 0x138 (32 bits); a node: + 0 a pointer to its 16-byte guid, + 8 its value, + 0x10 the
  // next; a guid's bucket (Data1 + Data2 + Data3) modulo the count. The lookup returns the value + 8.
  const int CompartmentRegistryOffset = 0x68, RegistryBucketsOffset = 0x130, RegistryBucketCountOffset = 0x138;
  const __int64 RegistryGuidLookupRva = 0x2AEB5E0;
  struct KnownPartition
  {
    char const* Name;
    BYTE Guid[16];
  };
  // The Release mission's (their ebx files' guids, offline): for the probe
  const KnownPartition KnownPartitions[] = {
    { "levels/sp/sp_maincity/release", { 0xC5, 0xDE, 0x3F, 0x99, 0x5B, 0x13, 0x42, 0x46, 0x80, 0x4C, 0xA3, 0xB1, 0x2F, 0xCF, 0xF8, 0x60 } },
    { "levels/sp/sp_maincity/release/layer1_release_logic",
      { 0x43, 0x80, 0x99, 0x3C, 0x7A, 0x62, 0x4A, 0x41, 0x85, 0xA4, 0xC4, 0x24, 0xFB, 0xBB, 0xE7, 0xBC } },
    { "objects/architecture/rezoning/buildings/tutorial/area00/pf_releasegateglass",
      { 0x7C, 0xAC, 0xE8, 0x80, 0xD5, 0xFE, 0xE4, 0x11, 0xA6, 0x07, 0xB5, 0x55, 0xF9, 0x81, 0x94, 0x78 } },
    { "objects/architecture/rezoning/buildings/tutorial/area00/csg/csg_releasegateglass_01",
      { 0x46, 0xE6, 0x45, 0xBB, 0xD0, 0xFE, 0xE4, 0x11, 0xA6, 0x07, 0xB5, 0x55, 0xF9, 0x81, 0x94, 0x78 } },
    { "objects/props/releasegate/releasegatedoor_01_businpc",
      { 0x6C, 0xF6, 0xCE, 0xCD, 0x96, 0xBE, 0xE5, 0x11, 0x8F, 0x35, 0x96, 0x55, 0x89, 0x50, 0x2A, 0x2F } },
  };
  const size_t MaxBundleNotes = 20000;
  // WKPDID_D3DDebugObjectName (d3dcommon.h)
  const GUID DebugObjectName = { 0x429b8c22, 0x9188, 0x4b0c, { 0x87, 0x42, 0xac, 0xb0, 0xbf, 0x85, 0xc2, 0x00 } };

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

  struct TexInfo
  {
    UINT Width = 0, Height = 0, Depth = 0, Mips = 0, Array = 0;
    DXGI_FORMAT Format = DXGI_FORMAT_UNKNOWN;
    int Dimension = 0;            // D3D11_RESOURCE_DIMENSION
    char Name[64] = "";
    double Made = -1.0;           // while it recorded: when the game made it (-1: not seen)
    bool MadeLoading = false;
    int Described = -1;           // its g_Described id when last looked up (see DescribedId)
  };
  struct DrawNote
  {
    __int64 Vb;
    UINT Start, Count;            // the draw's first index and index count
    int Mode;                     // 1 recording, 2 playing
    int Second;
    bool Replay;                  // the replay's own draw
    int Draws;
    std::vector<std::pair<int, __int64>> Slots;   // each of its pixel shader's texture slots read and the texture there
  };
  struct Made
  {
    __int64 Texture;
    double Time;
    bool Loading;
    D3D11_TEXTURE2D_DESC Desc;
  };
  struct BundleNote
  {
    int What;                     // 0 registered, 1 load, 2 unload
    unsigned Compartment;         // the one it got (registered), or loaded / unloaded
    unsigned Parent;              // registered: its parent compartment
    int Kind;                     // registered: its kind
    double Time;
    bool Loading;
    unsigned Thread;              // the thread it was called on
    char Name[160];
  };
  std::mutex g_Mutex;   // g_Textures, g_Pending, g_Created, g_BundleNotes, g_PendingOther, g_OtherPassNamesShared
  std::unordered_map<__int64, TexInfo> g_Textures;
  std::vector<DrawNote> g_Pending;
  std::vector<Made> g_Created;
  std::vector<BundleNote> g_BundleNotes;
  std::vector<std::pair<__int64, int>> g_PendingOther;                 // g_FrameOther's, for the game thread
  std::vector<std::string> g_OtherPassNamesShared;                     // g_OtherPassNames, for the game thread
  std::atomic<int> g_Mode{ 0 };
  std::atomic<double> g_Clock{ 0.0 };
  std::atomic<bool> g_Loading{ false };
  std::atomic<bool> g_Dropped{ false };

  // Render thread
  unsigned g_Frame = 0;
  bool g_Hooked = false;
  struct FrameSeen
  {
    int Draws = 0;
    std::vector<std::pair<int, __int64>> Slots;   // slot, texture
  };
  typedef std::tuple<__int64, UINT, UINT> DrawKey;                     // vertex buffer, first index, index count
  std::map<DrawKey, FrameSeen> g_FrameGame, g_FrameReplay;             // this sampled frame, by draw
  std::unordered_set<__int64> g_FrameTextures;                         // described this frame
  // Draws in the game's other passes while it records (2026-10-28: the
  // Release gate's glass, csg_releasegateglass_01, was never drawn in the
  // G-buffer pass, and where it is drawn wasn't known): this sampled frame's,
  // by vertex buffer and pass; each pass's description by its number
  std::set<std::pair<__int64, int>> g_FrameOther;
  std::vector<std::string> g_OtherPassNames;
  std::map<std::tuple<__int64, __int64, int>, int> g_OtherPassIds;    // first target, depth target, targets -> number

  bool Sampled()
  {
    return g_Mode.load(std::memory_order_relaxed) != 0 && g_Frame % SampleEvery == 0;
  }

  // A texture a pixel shader reads (0: none, or a buffer), described the first
  // time it's seen in a frame
  __int64 TextureOf(ID3D11ShaderResourceView* view)
  {
    ID3D11Resource* resource = nullptr;
    view->GetResource(&resource);
    if (!resource)
      return 0;
    D3D11_RESOURCE_DIMENSION dimension = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    resource->GetType(&dimension);
    __int64 at = (__int64)resource;
    if (dimension == D3D11_RESOURCE_DIMENSION_BUFFER || dimension == D3D11_RESOURCE_DIMENSION_UNKNOWN)
    {
      resource->Release();
      return 0;
    }
    if (g_FrameTextures.insert(at).second)
    {
      TexInfo info;
      info.Dimension = dimension;
      if (dimension == D3D11_RESOURCE_DIMENSION_TEXTURE2D)
      {
        ID3D11Texture2D* texture = nullptr;
        if (SUCCEEDED(resource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&texture)) && texture)
        {
          D3D11_TEXTURE2D_DESC d{};
          texture->GetDesc(&d);
          info.Width = d.Width;
          info.Height = d.Height;
          info.Mips = d.MipLevels;
          info.Array = d.ArraySize;
          info.Format = d.Format;
          texture->Release();
        }
      }
      else if (dimension == D3D11_RESOURCE_DIMENSION_TEXTURE3D)
      {
        ID3D11Texture3D* texture = nullptr;
        if (SUCCEEDED(resource->QueryInterface(__uuidof(ID3D11Texture3D), (void**)&texture)) && texture)
        {
          D3D11_TEXTURE3D_DESC d{};
          texture->GetDesc(&d);
          info.Width = d.Width;
          info.Height = d.Height;
          info.Depth = d.Depth;
          info.Mips = d.MipLevels;
          info.Format = d.Format;
          texture->Release();
        }
      }
      UINT size = sizeof(info.Name) - 1;
      if (SUCCEEDED(resource->GetPrivateData(DebugObjectName, &size, info.Name)))
        info.Name[(std::min)(size, (UINT)sizeof(info.Name) - 1)] = 0;
      else
        info.Name[0] = 0;
      std::lock_guard<std::mutex> lock(g_Mutex);
      auto it = g_Textures.find(at);
      if (it == g_Textures.end())
        g_Textures.emplace(at, info);
      else
      {
        // Described anew (another texture made at its address keeps its making time, see hCreateTexture2D)
        info.Made = it->second.Made;
        info.MadeLoading = it->second.MadeLoading;
        it->second = info;
      }
    }
    resource->Release();
    return at;
  }

  char const* TargetFormat(DXGI_FORMAT f, char* buffer, size_t size)
  {
    switch (f)
    {
    case DXGI_FORMAT_R16G16B16A16_FLOAT: return "RGBA16F";
    case DXGI_FORMAT_R10G10B10A2_UNORM: return "RGB10A2";
    case DXGI_FORMAT_R8G8B8A8_UNORM: return "RGBA8";
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return "RGBA8_SRGB";
    case DXGI_FORMAT_R11G11B10_FLOAT: return "R11G11B10F";
    case DXGI_FORMAT_R16G16_FLOAT: return "RG16F";
    case DXGI_FORMAT_R32_FLOAT: return "R32F";
    case DXGI_FORMAT_D32_FLOAT: return "D32F";
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT: return "D32FS8";
    case DXGI_FORMAT_D24_UNORM_S8_UINT: return "D24S8";
    default:
      sprintf_s(buffer, size, "format %d", (int)f);
      return buffer;
    }
  }

  // A draw of the game's outside its G-buffer pass while it records (immediate
  // context only: the render thread's), by its vertex buffer and pass
  void NoteOther(ID3D11DeviceContext* pContext)
  {
    if (g_Mode.load(std::memory_order_relaxed) != 1 || pContext->GetType() != D3D11_DEVICE_CONTEXT_IMMEDIATE ||
        g_FrameOther.size() >= MaxOtherPerFrame)
      return;
    ID3D11Buffer* vb = nullptr;
    UINT stride = 0, offset = 0;
    pContext->IAGetVertexBuffers(0, 1, &vb, &stride, &offset);
    if (!vb)
      return;
    __int64 vbAt = (__int64)vb;
    vb->Release();
    ID3D11RenderTargetView* rtvs[8] = {};
    ID3D11DepthStencilView* dsv = nullptr;
    pContext->OMGetRenderTargets(8, rtvs, &dsv);
    int targets = 0;
    for (int i = 0; i < 8; ++i)
      targets += rtvs[i] != nullptr;
    auto key = std::make_tuple((__int64)rtvs[0], (__int64)dsv, targets);
    int id = -1;
    auto known = g_OtherPassIds.find(key);
    if (known != g_OtherPassIds.end())
      id = known->second;
    else
    {
      std::string desc;
      char buffer[32];
      if (!targets)
        desc = "no colour targets";
      else
      {
        desc = std::to_string(targets) + (targets == 1 ? " target" : " targets");
        for (int i = 0; i < 8; ++i)
          if (rtvs[i])
          {
            D3D11_RENDER_TARGET_VIEW_DESC d{};
            rtvs[i]->GetDesc(&d);
            desc += " ";
            desc += TargetFormat(d.Format, buffer, sizeof(buffer));
          }
      }
      if (dsv)
      {
        D3D11_DEPTH_STENCIL_VIEW_DESC d{};
        dsv->GetDesc(&d);
        ID3D11Resource* res = nullptr;
        dsv->GetResource(&res);
        ID3D11Texture2D* tex = nullptr;
        D3D11_TEXTURE2D_DESC td{};
        if (res && SUCCEEDED(res->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&tex)) && tex)
        {
          tex->GetDesc(&td);
          tex->Release();
        }
        if (res)
          res->Release();
        char depth[96];
        sprintf_s(depth, ", depth %s %ux%u", TargetFormat(d.Format, buffer, sizeof(buffer)), td.Width, td.Height);
        desc += depth;
      }
      else
        desc += ", no depth";
      int kept = replay_gamedraws::PassNow();
      desc += kept == 1 ? " (the replay keeps it: its decal pass)" : kept == 2 ? " (the replay keeps it: its lit picture's pass)" :
        kept == 3 ? " (the replay keeps it: a half-size transparent pass)" : kept == 5 ?
        " (the replay keeps it: the runner's vision layer)" : " (not kept by the replay)";
      for (size_t i = 0; i < g_OtherPassNames.size() && id < 0; ++i)
        if (g_OtherPassNames[i] == desc)
          id = (int)i;
      if (id < 0 && g_OtherPassNames.size() < 256)
      {
        g_OtherPassNames.push_back(desc);
        id = (int)g_OtherPassNames.size() - 1;
      }
      if (g_OtherPassIds.size() > 1024)
        g_OtherPassIds.clear();
      if (id >= 0)
        g_OtherPassIds[key] = id;
    }
    for (int i = 0; i < 8; ++i)
      if (rtvs[i])
        rtvs[i]->Release();
    if (dsv)
      dsv->Release();
    if (id >= 0)
      g_FrameOther.insert({ vbAt, id });
  }

  void Note(std::map<DrawKey, FrameSeen>& frame, __int64 vb, UINT start, UINT indices, ID3D11ShaderResourceView* const* views,
    int count)
  {
    FrameSeen& seen = frame[DrawKey(vb, start, indices)];
    ++seen.Draws;
    for (int i = 0; i < count; ++i)
      if (views[i])
      {
        __int64 texture = TextureOf(views[i]);
        std::pair<int, __int64> slot(i, texture);
        if (texture && std::find(seen.Slots.begin(), seen.Slots.end(), slot) == seen.Slots.end())
          seen.Slots.push_back(slot);
      }
  }

  typedef HRESULT(STDMETHODCALLTYPE* tCreateTexture2D)(ID3D11Device*, D3D11_TEXTURE2D_DESC const*, D3D11_SUBRESOURCE_DATA const*,
    ID3D11Texture2D**);
  tCreateTexture2D oCreateTexture2D = nullptr;

  HRESULT STDMETHODCALLTYPE hCreateTexture2D(ID3D11Device* pDevice, D3D11_TEXTURE2D_DESC const* desc, D3D11_SUBRESOURCE_DATA const* data,
    ID3D11Texture2D** out)
  {
    HRESULT hr = oCreateTexture2D(pDevice, desc, data, out);
    if (SUCCEEDED(hr) && out && *out && desc && g_Mode.load(std::memory_order_relaxed) == 1 &&
        (desc->BindFlags & D3D11_BIND_SHADER_RESOURCE) &&
        !(desc->BindFlags & (D3D11_BIND_RENDER_TARGET | D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_UNORDERED_ACCESS)))
    {
      std::lock_guard<std::mutex> lock(g_Mutex);
      if (g_Created.size() < MaxCreated)
        g_Created.push_back({ (__int64)*out, g_Clock.load(), g_Loading.load(), *desc });
      else
        g_Dropped = true;
    }
    return hr;
  }

  // A compartment's bundle name, as the table has it now ("" if none)
  void CompartmentName(unsigned compartment, char* out, size_t size)
  {
    out[0] = 0;
    if (compartment >= (unsigned)Compartments)
      return;
    __int64 entry = Q((__int64)g_gameHandle + CompartmentTableRva + (__int64)compartment * 8);
    ReadName(entry ? Q(entry + CompartmentNameOffset) : 0, out, size);
  }

  void NoteBundle(int what, unsigned compartment, unsigned parent, int kind, char const* name)
  {
    if (g_Mode.load(std::memory_order_relaxed) == 0)
      return;
    BundleNote note{ what, compartment, parent, kind, g_Clock.load(), g_Loading.load(), GetCurrentThreadId(), "" };
    if (name)
      ReadName((__int64)name, note.Name, sizeof(note.Name));
    else
      CompartmentName(compartment, note.Name, sizeof(note.Name));
    std::lock_guard<std::mutex> lock(g_Mutex);
    if (g_BundleNotes.size() < MaxBundleNotes)
      g_BundleNotes.push_back(note);
    else
      g_Dropped = true;
  }

  typedef unsigned __int64(__fastcall* tBundleRegister)(char const*, unsigned short, int, void*, unsigned char, int);
  typedef int*(__fastcall* tBundleLoad)(int*, unsigned short, void*, int, __int64, int, int);
  typedef int*(__fastcall* tBundleUnload)(int*, unsigned short, int);
  tBundleRegister oBundleRegister = nullptr;
  tBundleLoad oBundleLoad = nullptr;
  tBundleUnload oBundleUnload = nullptr;

  unsigned __int64 __fastcall hBundleRegister(char const* name, unsigned short parent, int kind, void* heap, unsigned char flag,
    int extra)
  {
    unsigned __int64 result = oBundleRegister(name, parent, kind, heap, flag, extra);
    NoteBundle(0, (unsigned short)result, parent, kind, name);
    return result;
  }

  int* __fastcall hBundleLoad(int* out, unsigned short compartment, void* ref, int a4, __int64 a5, int a6, int a7)
  {
    int* result = oBundleLoad(out, compartment, ref, a4, a5, a6, a7);
    NoteBundle(1, compartment, 0, 0, nullptr);
    return result;
  }

  int* __fastcall hBundleUnload(int* out, unsigned short compartment, int a3)
  {
    // Its name before it goes
    NoteBundle(2, compartment, 0, 0, nullptr);
    return oBundleUnload(out, compartment, a3);
  }

  // Up to 32 bytes (2026-11-01: 16 before, which refused the 17- and 20-byte
  // mission sub-level checks and with them every mission hook)
  bool CodeIs(__int64 rva, BYTE const* bytes, size_t size)
  {
    BYTE here[32] = {};
    return size <= sizeof(here) && ReadRaw(here, (__int64)g_gameHandle + rva, size) && !memcmp(here, bytes, size);
  }

  void HookBundles()
  {
    __int64 base = (__int64)g_gameHandle;
    if (!CodeIs(BundleRegisterRva, BundleRegisterBytes, sizeof(BundleRegisterBytes)) ||
        !CodeIs(BundleLoadRva, BundleLoadBytes, sizeof(BundleLoadBytes)) ||
        !CodeIs(BundleUnloadRva, BundleUnloadBytes, sizeof(BundleUnloadBytes)))
    {
      util::log::Warning("Replay assets: the game's bundle manager isn't what the tools know: its bundles aren't noted");
      return;
    }
    bool a = util::hooks::CreateGameHook("ReplayAssetsBundleRegister", base + BundleRegisterRva, (PVOID)hBundleRegister,
      (LPVOID*)&oBundleRegister);
    bool b = util::hooks::CreateGameHook("ReplayAssetsBundleLoad", base + BundleLoadRva, (PVOID)hBundleLoad, (LPVOID*)&oBundleLoad);
    bool c = util::hooks::CreateGameHook("ReplayAssetsBundleUnload", base + BundleUnloadRva, (PVOID)hBundleUnload,
      (LPVOID*)&oBundleUnload);
    util::log::Write("Replay assets: the game's bundles are noted while the replay records or plays (%s, %s, %s)",
      a ? "registered" : "registering NOT hooked", b ? "loaded" : "loading NOT hooked", c ? "unloaded" : "unloading NOT hooked");
  }

  // A TypeInfo's class name ("" if not readable): + 0 its info, whose + 0 is the name
  void TypeName(__int64 typeInfo, char* out, size_t size)
  {
    out[0] = 0;
    __int64 info = typeInfo ? Q(typeInfo) : 0;
    ReadName(info ? Q(info) : 0, out, size);
  }

  bool InImage(__int64 p)
  {
    __int64 base = (__int64)g_gameHandle;
    return p >= base && p < base + 0x8000000;
  }

  // A pointer's note: an object whose vtable is the game's (its vtable's offset in the game), or ""
  void PointerNote(__int64 p, char* out, size_t size)
  {
    out[0] = 0;
    if (p < 0x10000 || InImage(p))
      return;
    __int64 vtable = 0;
    if (ReadRaw(&vtable, p, sizeof(vtable)) && InImage(vtable))
      sprintf_s(out, size, " (vtable +0x%I64X)", vtable - (__int64)g_gameHandle);
  }

  // Bytes at a place, 16 a line: two qwords (with PointerNote) and four floats
  void DumpRows(char const* what, __int64 at, BYTE const* bytes, int count)
  {
    for (int row = 0; row + 16 <= count; row += 16)
    {
      __int64 a = 0, b = 0;
      float f[4];
      memcpy(&a, bytes + row, 8);
      memcpy(&b, bytes + row + 8, 8);
      memcpy(f, bytes + row, sizeof(f));
      char na[48], nb[48];
      PointerNote(a, na, sizeof(na));
      PointerNote(b, nb, sizeof(nb));
      util::log::Write("Replay assets:     %s 0x%I64X + 0x%02X: 0x%016I64X%s 0x%016I64X%s | %g %g %g %g", what, at, row, a, na, b, nb,
        f[0], f[1], f[2], f[3]);
    }
  }

  // The game's code calling (the frames in the game, past the tools' own)
  void CallersText(char* out, size_t size)
  {
    void* frames[12] = {};
    USHORT n = RtlCaptureStackBackTrace(0, 12, frames, nullptr);
    size_t at = 0;
    out[0] = 0;
    int shown = 0;
    for (USHORT i = 0; i < n && shown < 5; ++i)
      if (InImage((__int64)frames[i]))
      {
        int w = sprintf_s(out + at, size - at, "%s%I64X", shown ? " " : "", (__int64)frames[i]);
        if (w <= 0)
          break;
        at += w;
        ++shown;
      }
  }

  std::atomic<int> g_CreateDumps{ 0 }, g_CreateLines{ 0 }, g_ReferenceDumps{ 0 }, g_ReferenceLines{ 0 };
  std::atomic<int> g_StaticCreated{ 0 }, g_ReferencesCreated{ 0 };
  std::mutex g_CreatedMutex;
  std::vector<std::pair<__int64, std::string>> g_CreatedData;   // while it records: the data made from, and what (for ProbePartitions)

  typedef __int64(__fastcall* tCreate)(__int64 factory, __int64 info);
  tCreate oCreateGeneric = nullptr, oCreateReference = nullptr;

  // One creation, in the log (the info as it was before the create)
  void NoteCreation(bool reference, __int64 factory, __int64 info, BYTE const* infoBytes, bool infoRead, __int64 entity)
  {
    __int64 typeInfo = Q(factory + FactoryTypeOffset);
    UINT realm = D(factory + FactoryRealmOffset);
    __int64 data = infoRead ? *(__int64 const*)(infoBytes + InfoDataOffset) : 0;
    __int64 context = infoRead ? *(__int64 const*)(infoBytes + InfoContextOffset) : 0;
    char type[96], what[200] = "", callers[120], contextNote[48];
    TypeName(typeInfo, type, sizeof(type));
    float place[3] = {};
    bool placed = false;
    if (reference)
    {
      __int64 blueprint = data ? Q(data + ReferenceBlueprintOffset) : 0;
      ReadName(blueprint ? Q(blueprint + AssetNameOffset) : 0, what, sizeof(what));
      placed = data && ReadRaw(place, data + ReferenceTransformOffset + 0x30, sizeof(place));
    }
    else
    {
      __int64 mesh = data ? Q(data + StaticModelMeshOffset) : 0;
      ReadName(mesh ? Q(mesh + AssetNameOffset) : 0, what, sizeof(what));
    }
    std::atomic<int>& lines = reference ? g_ReferenceLines : g_CreateLines;
    std::atomic<int>& dumps = reference ? g_ReferenceDumps : g_CreateDumps;
    if (data)
    {
      std::lock_guard<std::mutex> lock(g_CreatedMutex);
      if (g_CreatedData.size() < 20000)
        g_CreatedData.push_back({ data, std::string(type) + " " + what });
    }
    if (lines.fetch_add(1) >= (reference ? MaxReferenceLines : MaxCreateLines))
      return;
    CallersText(callers, sizeof(callers));
    PointerNote(context, contextNote, sizeof(contextNote));
    char where[80] = "";
    if (placed)
      sprintf_s(where, " at %.2f %.2f %.2f (its transform in its blueprint)", place[0], place[1], place[2]);
    util::log::Write("Replay assets: made a %s (realm %u) for '%s'%s: data 0x%I64X, context 0x%I64X%s, entity 0x%I64X, info 0x%I64X, "
      "thread %u, from %s", type[0] ? type : "?", realm, what, where, data, context, contextNote, entity, info, GetCurrentThreadId(),
      callers);
    if (infoRead && dumps.fetch_add(1) < (reference ? MaxReferenceDumps : MaxCreateDumps))
    {
      DumpRows("info", info, infoBytes, InfoBytes);
      BYTE head[0x40];
      if (data && ReadRaw(head, data, sizeof(head)))
        DumpRows("data", data, head, sizeof(head));
      BYTE ctx[0x40];
      if (context > 0x10000 && ReadRaw(ctx, context, sizeof(ctx)))
        DumpRows("context", context, ctx, sizeof(ctx));
    }
  }

  __int64 __fastcall hCreateGeneric(__int64 factory, __int64 info)
  {
    // Only the static models' factory, while it records
    bool note = g_Mode.load(std::memory_order_relaxed) == 1 &&
      Q(factory + FactoryTypeOffset) == (__int64)g_gameHandle + StaticModelDataTypeRva;
    BYTE bytes[InfoBytes];
    bool read = note && ReadRaw(bytes, info, sizeof(bytes));
    __int64 entity = oCreateGeneric(factory, info);
    if (note)
    {
      g_StaticCreated.fetch_add(1);
      NoteCreation(false, factory, info, bytes, read, entity);
    }
    return entity;
  }

  __int64 __fastcall hCreateReference(__int64 factory, __int64 info)
  {
    bool note = g_Mode.load(std::memory_order_relaxed) == 1;
    BYTE bytes[InfoBytes];
    bool read = note && ReadRaw(bytes, info, sizeof(bytes));
    __int64 entity = oCreateReference(factory, info);
    if (note)
    {
      g_ReferencesCreated.fetch_add(1);
      NoteCreation(true, factory, info, bytes, read, entity);
    }
    return entity;
  }

  void HookCreation()
  {
    __int64 base = (__int64)g_gameHandle;
    if (!CodeIs(CreateGenericRva, CreateGenericBytes, sizeof(CreateGenericBytes)) ||
        !CodeIs(CreateReferenceRva, CreateReferenceBytes, sizeof(CreateReferenceBytes)))
    {
      util::log::Warning("Replay assets: the game's entity factories aren't what the tools know: their creations aren't noted");
      return;
    }
    bool a = util::hooks::CreateGameHook("ReplayAssetsCreateGeneric", base + CreateGenericRva, (PVOID)hCreateGeneric,
      (LPVOID*)&oCreateGeneric);
    bool b = util::hooks::CreateGameHook("ReplayAssetsCreateReference", base + CreateReferenceRva, (PVOID)hCreateReference,
      (LPVOID*)&oCreateReference);
    util::log::Write("Replay assets: the static models' and references' creations are noted while the replay records (read only; "
      "%s, %s)", a ? "static models" : "static models NOT hooked", b ? "references" : "references NOT hooked");
  }

  // The missions (2026-11-01; the user: "Replays should always spawn mission
  // meshes/models and place them where they should be (decompile all the
  // necessary systems to learn how missions work and what happens when you
  // abort a mission)"; Research_Mission_Lifecycle_2026-11-01.md). Read only,
  // the step before having the game stream a mission's world in while its
  // recording plays:
  // - the server's mission entities (PamServerMissionEntity: constructor
  //   0x143A72330, destructor 0x143A72F70, vtable 0x141C7ACD8) noted as the
  //   game makes and destroys them;
  // - each call of its availability (0x143A77640(entity, force)), its state
  //   update (0x143A76D60(entity, c1, c2, c3, force)), its abort handler
  //   (0x143A792D0) and its sub-levels' streaming (below) in the log, with the
  //   thread that made it;
  // - each one's mission (its asset's name), data (Enabled, StreamInOnAvailable,
  //   the stream-out and re-enable settings), state (+ 0x110 - + 0x127) and
  //   sub-levels logged when they change, looked at once a second from Update
  //   (the replay's thread, its id in the log too).
  // The 11-01 04:33 log: the game calls them on its server update thread (the
  // one FaithOutfit's Server::update hook runs on), not on the replay's.
  // Decompiled since (offline; Research_Mission_Lifecycle section 7):
  // - 0x143A74150 (hooked as "stream out" in 715CDCED454C) isn't one: it
  //   decides whether the mission keeps being updated, at the start of each of
  //   its updates and after a stream out; not hooked any more (every frame);
  // - the entity's init (0x143A77A40) links two sub-level entities
  //   (ServerSubLevelEntity), found by their link names' hashes: + 0xD0
  //   "SubLevel" (the mission's world) and + 0xD8 "PreMissionSubLevel", each
  //   a weak reference (its token's + 0 is the entity + 8);
  // - 0x143A7A0A0(entity, in) sends the SubLevel the event "StreamIn" (in) or
  //   "StreamOut" (then also resets the stream-out time and looks at the
  //   availability again); the sub-level entity queues it (0x14335EFB0) and
  //   streams the sub-level's bundle and entities in or out later. The
  //   mission's update does that itself when Faith comes near an available
  //   mission, and streams it out again when she's away while + 0x11F (set
  //   while the mission runs) is clear. 0x143A79FE0(entity, in) is the same
  //   for the PreMissionSubLevel. Both hooked, log only.
  // Nothing of the game's is written; the one call is the REPLAY tick's
  // (see the mission's world below).
  const __int64 MissionCtorRva = 0x3A72330, MissionDtorRva = 0x3A72F70, MissionStateRva = 0x3A76D60,
    MissionAvailRva = 0x3A77640, MissionAbortRva = 0x3A792D0, MissionSubLevelRva = 0x3A7A0A0, MissionPreSubLevelRva = 0x3A79FE0,
    MissionVTableRva = 0x1C7ACD8, MissionTypeInfoRva = 0x2864420, MissionTypeDataRva = 0x2863BF8;
  // mov [rsp + 8], rcx; push rbx; push rbp; push rsi; push rdi; push r14; push r15
  const BYTE MissionCtorBytes[] = { 0x48, 0x89, 0x4C, 0x24, 0x08, 0x53, 0x55, 0x56, 0x57, 0x41, 0x56, 0x41, 0x57 };
  // mov [rsp + 8], rcx; push rdi; sub rsp, 0x30
  const BYTE MissionDtorBytes[] = { 0x48, 0x89, 0x4C, 0x24, 0x08, 0x57, 0x48, 0x83, 0xEC, 0x30 };
  // push rbp; push rsi; push rdi; push r12; push r13; push r14; push r15; mov rbp, rsp
  const BYTE MissionStateBytes[] = { 0x55, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57, 0x48, 0x89, 0xE5 };
  // mov [rsp + 8], rbx; mov [rsp + 0x10], rbp; mov [rsp + 0x18], rsi
  const BYTE MissionAvailBytes[] = { 0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x6C, 0x24, 0x10, 0x48, 0x89, 0x74, 0x24 };
  // push rbx; sub rsp, 0x60; mov qword [rsp + 0x20], -2
  const BYTE MissionAbortBytes[] = { 0x53, 0x48, 0x83, 0xEC, 0x60, 0x48, 0xC7, 0x44, 0x24, 0x20, 0xFE, 0xFF, 0xFF, 0xFF };
  // push rbx; sub rsp, 0x40; mov qword [rsp + 0x20], -2; mov rbx, rcx
  const BYTE MissionSubLevelBytes[] = { 0x53, 0x48, 0x83, 0xEC, 0x40, 0x48, 0xC7, 0x44, 0x24, 0x20, 0xFE, 0xFF, 0xFF, 0xFF, 0x48,
    0x89, 0xCB };
  // sub rsp, 0x48; mov qword [rsp + 0x20], -2; cmp dword [rcx + 0x124], 0
  const BYTE MissionPreSubLevelBytes[] = { 0x48, 0x83, 0xEC, 0x48, 0x48, 0xC7, 0x44, 0x24, 0x20, 0xFE, 0xFF, 0xFF, 0xFF, 0x83,
    0xB9, 0x24, 0x01, 0x00, 0x00, 0x00 };
  const int MissionDataOffset = 0x28;          // the entity's data (PamMissionEntityData)
  const int MissionAssetOffset = 0x18;         // the data's Mission
  const int MissionStateFrom = 0x110, MissionStateSize = 0x18;
  const int MissionSubLevelLink = 0xD0, MissionPreSubLevelLink = 0xD8;
  const int MissionRunningOffset = 0x11F;      // set while the mission runs (see above)
  // The 04:33 log ran out at 400 lines as the level loaded (121 missions, 4
  // calls each): the abort wasn't in it. Now 60 a mission, 6000 in all
  const int MaxMissionCallLogs = 6000, MaxMissionCallLogsEach = 60;
  const ULONGLONG MissionLookMs = 1000;

  typedef __int64(__fastcall* tMissionOne)(__int64);
  typedef __int64(__fastcall* tMissionCtor)(__int64, __int64, __int64);
  typedef __int64(__fastcall* tMissionAvail)(__int64, char);
  typedef __int64(__fastcall* tMissionState)(__int64, int, int, int, char);
  typedef void(__fastcall* tMissionStream)(__int64, char);
  tMissionCtor oMissionCtor = nullptr;
  tMissionOne oMissionDtor = nullptr, oMissionAbort = nullptr;
  tMissionAvail oMissionAvail = nullptr;
  tMissionState oMissionState = nullptr;
  tMissionStream oMissionSubLevel = nullptr, oMissionPreSubLevel = nullptr;
  std::mutex g_MissionMutex;
  std::vector<__int64> g_Missions;                          // under it
  std::map<__int64, int> g_MissionCallCounts;               // under it: lines logged of each
  std::vector<std::string> g_RecordedMissions;              // under it: running while it recorded (or a loaded recording's)
  std::map<__int64, std::vector<BYTE>> g_MissionLogged;     // Update's thread: what was logged last of each
  std::atomic<int> g_MissionCallLogs{ 0 };
  ULONGLONG g_MissionLookAt = 0;
  bool g_MissionThreadLogged = false;
  bool g_MissionsHooked = false;

  // The mission's world from the game (2026-11-01, a REPLAY tick, off by
  // default, a test of the plan in Research_Mission_Lifecycle section 4):
  // while a recording of a mission plays, the server's update (its own
  // thread: FaithOutfit's hook calls ServerUpdate) asks that mission's entity
  // in this level, once, to stream its SubLevel in with the game's own
  // 0x143A7A0A0(entity, true), as the mission's update does when Faith comes
  // near an available mission: the game's sub-level streaming then makes the
  // mission's world (the gate, the glass, the doors, the bus) with its own
  // detail, shadows and logic. When the playback ends, the same function
  // streams it out again (as an abort does), if the mission isn't running
  // then. The missions are the ones running (+ 0x11F) while it recorded, by
  // name (kept in a saved recording too).
  std::atomic<bool> g_MissionWorld{ false };
  std::vector<__int64> g_MissionAsked;                      // server update thread: the entities asked to stream in
  int g_MissionServerMode = 0;                              // server update thread: the replay's mode seen last
  bool g_MissionAskDone = false;                            // server update thread: this playback's ask made or given up
  bool g_MissionWaitLogged = false;                         // server update thread

  // The guards (2026-11-01, Research_Mission_Lifecycle section 8: what a
  // mission's sub-level does when it's in while the mission doesn't run):
  // - a mission running now: its world is the game's (and with another
  //   mission running the game streams this one out: the state update's
  //   second condition);
  // - the real Faith near the mission's player triggers: those fire for the
  //   player (AreaTriggerInclude ATPlayers) and start the mission's
  //   KrugerSec, voice-over, input restrictions and progression flags (the
  //   save). The replay leaves her where she stands, so the world is asked
  //   for only while she's away from them: from the mission's own trigger
  //   box where it was measured (offline, the enabled player triggers' shapes
  //   with their reach), and from the recording's path otherwise too. If she
  //   comes closer while it plays, the world is streamed out again.
  struct MissionTriggerBox
  {
    char const* Mission;
    float Min[3], Max[3];      // the shapes' places, then their biggest reach around them
    float Reach;
  };
  // Release: 80 enabled player triggers (78 boxes, a volume, a sphere), the
  // shapes at x -42 - 114, y 3 - 45, z 488 - 1187, reaching 26.2 m at most
  const MissionTriggerBox MissionTriggerBoxes[] = {
    { "Release", { -42.0f, 3.0f, 488.0f }, { 114.0f, 45.0f, 1187.0f }, 26.2f },
  };
  const float AskBoxClearance = 60.0f, KeepBoxClearance = 30.0f;     // metres beyond a box's reach
  const float AskRouteClearance = 150.0f, KeepRouteClearance = 100.0f; // metres from the recording's path
  std::mutex g_FaithMutex;
  bool g_FaithKnown = false;                                // under it
  float g_FaithPlace[3] = {};                               // under it
  float g_FaithRoute = FLT_MAX;                             // under it

  bool IsMission(__int64 entity)
  {
    return entity > 0x10000 && Q(entity) == (__int64)g_gameHandle + MissionVTableRva;
  }

  void NoteMission(__int64 entity)
  {
    if (!IsMission(entity))
      return;
    std::lock_guard<std::mutex> lock(g_MissionMutex);
    if (std::find(g_Missions.begin(), g_Missions.end(), entity) == g_Missions.end() && g_Missions.size() < 256)
      g_Missions.push_back(entity);
  }

  void MissionName(__int64 entity, char* out, size_t size)
  {
    __int64 data = Q(entity + MissionDataOffset);
    __int64 asset = data ? Q(data + MissionAssetOffset) : 0;
    ReadName(asset ? Q(asset + AssetNameOffset) : 0, out, size);
    if (!out[0])
      strcpy_s(out, size, "?");
  }

  void LogMissionCall(char const* what, __int64 entity, char const* args)
  {
    if (!util::DiagnosticsOn())
      return;
    {
      std::lock_guard<std::mutex> lock(g_MissionMutex);
      int& each = g_MissionCallCounts[entity];
      if (each >= MaxMissionCallLogsEach)
        return;
      ++each;
    }
    if (g_MissionCallLogs.fetch_add(1) >= MaxMissionCallLogs)
      return;
    char name[160];
    MissionName(entity, name, sizeof(name));
    util::log::Write("Replay missions: %s on '%s' (entity 0x%I64X%s), thread %u, the replay %s at %.2f s", what, name, entity, args,
      GetCurrentThreadId(), g_Mode.load() == 1 ? "recording" : g_Mode.load() == 2 ? "playing" : "idle", g_Clock.load());
  }

  __int64 __fastcall hMissionCtor(__int64 entity, __int64 info, __int64 data)
  {
    __int64 result = oMissionCtor(entity, info, data);
    NoteMission(entity);
    if (util::DiagnosticsOn() && IsMission(entity))
    {
      char args[64];
      sprintf_s(args, ", data 0x%I64X", data);
      LogMissionCall("made", entity, args);
    }
    return result;
  }

  __int64 __fastcall hMissionDtor(__int64 entity)
  {
    if (IsMission(entity))
    {
      LogMissionCall("destroyed", entity, "");
      std::lock_guard<std::mutex> lock(g_MissionMutex);
      g_Missions.erase(std::remove(g_Missions.begin(), g_Missions.end(), entity), g_Missions.end());
      if (util::DiagnosticsOn())   // (the lines' counts)
        g_MissionCallCounts.erase(entity);
    }
    return oMissionDtor(entity);
  }

  __int64 __fastcall hMissionAvail(__int64 entity, char force)
  {
    NoteMission(entity);
    if (util::DiagnosticsOn())
    {
      char args[32];
      sprintf_s(args, ", force %d", (int)force);
      LogMissionCall("availability looked at (0x143A77640)", entity, args);
    }
    return oMissionAvail(entity, force);
  }

  __int64 __fastcall hMissionState(__int64 entity, int c1, int c2, int c3, char force)
  {
    NoteMission(entity);
    if (util::DiagnosticsOn())
    {
      char args[80];
      sprintf_s(args, ", conditions %d %d %d, force %d", c1, c2, c3, (int)force);
      LogMissionCall("state update (0x143A76D60)", entity, args);
    }
    return oMissionState(entity, c1, c2, c3, force);
  }

  __int64 __fastcall hMissionAbort(__int64 entity)
  {
    NoteMission(entity);
    LogMissionCall("aborted (0x143A792D0)", entity, "");
    return oMissionAbort(entity);
  }

  // A weak reference's entity (its token's + 0 is the entity + 8), 0 if none
  __int64 LinkedEntity(__int64 entity, int link)
  {
    __int64 token = Q(entity + link);
    __int64 at = token ? Q(token) : 0;
    return at > 0x10008 ? at - 8 : 0;
  }

  bool MissionRunning(__int64 entity)
  {
    BYTE running = 0;
    return ReadRaw(&running, entity + MissionRunningOffset, 1) && running;
  }

  // An entity's class name: its vtable's slot 0 (getType: lea rax, [type
  // info]; ret), the type info's + 0 (its data), the data's + 0 (the name), a
  // layout checked on the mission entity's own type info first
  void ClassName(__int64 entity, char* out, size_t size)
  {
    strcpy_s(out, size, "?");
    __int64 base = (__int64)g_gameHandle;
    if (!entity || Q(base + MissionTypeInfoRva) != base + MissionTypeDataRva)
      return;
    __int64 getType = Q(Q(entity));
    BYTE code[8] = {};
    if (!ReadRaw(code, getType, sizeof(code)) || code[0] != 0x48 || code[1] != 0x8D || code[2] != 0x05 || code[7] != 0xC3)
      return;
    __int64 typeInfo = getType + 7 + *(int const*)&code[3];
    __int64 data = Q(typeInfo);
    ReadName(data ? Q(data) : 0, out, size);
    if (!out[0])
      strcpy_s(out, size, "?");
  }

  void LogStreamCall(char const* which, __int64 entity, int link, char in)
  {
    if (!util::DiagnosticsOn())
      return;
    __int64 sub = LinkedEntity(entity, link);
    char kind[96], args[200];
    ClassName(sub, kind, sizeof(kind));
    sprintf_s(args, ", %s: its %s entity 0x%I64X (%s)", in ? "StreamIn" : "StreamOut", which, sub, sub ? kind : "none");
    LogMissionCall(link == MissionSubLevelLink ? "SubLevel streamed (0x143A7A0A0)" : "PreMissionSubLevel streamed (0x143A79FE0)",
      entity, args);
  }

  void __fastcall hMissionSubLevel(__int64 entity, char in)
  {
    NoteMission(entity);
    LogStreamCall("SubLevel", entity, MissionSubLevelLink, in);
    // (the weather's rain sound let go before that world goes, see Weather.h)
    if (!in)
      weather::WorldGoing("a mission's world streamed out");
    oMissionSubLevel(entity, in);
  }

  void __fastcall hMissionPreSubLevel(__int64 entity, char in)
  {
    NoteMission(entity);
    LogStreamCall("PreMissionSubLevel", entity, MissionPreSubLevelLink, in);
    if (!in)
      weather::WorldGoing("a mission's pre-mission world streamed out");
    oMissionPreSubLevel(entity, in);
  }

  void HookMissions()
  {
    __int64 base = (__int64)g_gameHandle;
    if (!CodeIs(MissionCtorRva, MissionCtorBytes, sizeof(MissionCtorBytes)) ||
        !CodeIs(MissionDtorRva, MissionDtorBytes, sizeof(MissionDtorBytes)) ||
        !CodeIs(MissionStateRva, MissionStateBytes, sizeof(MissionStateBytes)) ||
        !CodeIs(MissionAvailRva, MissionAvailBytes, sizeof(MissionAvailBytes)) ||
        !CodeIs(MissionAbortRva, MissionAbortBytes, sizeof(MissionAbortBytes)) ||
        !CodeIs(MissionSubLevelRva, MissionSubLevelBytes, sizeof(MissionSubLevelBytes)) ||
        !CodeIs(MissionPreSubLevelRva, MissionPreSubLevelBytes, sizeof(MissionPreSubLevelBytes)) ||
        Q(base + MissionVTableRva) != base + 0x3A75DD0)
    {
      util::log::Warning("Replay missions: the game's mission entity code isn't what the tools know: the missions aren't logged");
      return;
    }
    int made = 0;
    made += util::hooks::CreateGameHook("ReplayMissionMade", base + MissionCtorRva, (PVOID)hMissionCtor, (LPVOID*)&oMissionCtor);
    made += util::hooks::CreateGameHook("ReplayMissionDestroyed", base + MissionDtorRva, (PVOID)hMissionDtor, (LPVOID*)&oMissionDtor);
    made += util::hooks::CreateGameHook("ReplayMissionState", base + MissionStateRva, (PVOID)hMissionState, (LPVOID*)&oMissionState);
    made += util::hooks::CreateGameHook("ReplayMissionAvailability", base + MissionAvailRva, (PVOID)hMissionAvail,
      (LPVOID*)&oMissionAvail);
    made += util::hooks::CreateGameHook("ReplayMissionAbort", base + MissionAbortRva, (PVOID)hMissionAbort, (LPVOID*)&oMissionAbort);
    made += util::hooks::CreateGameHook("ReplayMissionSubLevel", base + MissionSubLevelRva, (PVOID)hMissionSubLevel,
      (LPVOID*)&oMissionSubLevel);
    made += util::hooks::CreateGameHook("ReplayMissionPreSubLevel", base + MissionPreSubLevelRva, (PVOID)hMissionPreSubLevel,
      (LPVOID*)&oMissionPreSubLevel);
    g_MissionsHooked = made == 7;
    if (util::DiagnosticsOn())
      util::log::Write("Replay missions: the server's mission entities are logged (%d of 7 hooks made)", made);
  }

  // Update's thread, once a second: each mission entity's data and state, in
  // the log when they changed
  void LookAtMissions(int mode, double clock)
  {
    ULONGLONG now = GetTickCount64();
    if (now < g_MissionLookAt)
      return;
    g_MissionLookAt = now + MissionLookMs;
    // Off, only a recording needs them (its missions)
    if (!util::DiagnosticsOn() && mode != 1)
      return;
    std::vector<__int64> missions;
    {
      std::lock_guard<std::mutex> lock(g_MissionMutex);
      missions = g_Missions;
    }
    if (missions.empty())
      return;
    if (util::DiagnosticsOn() && !g_MissionThreadLogged)
    {
      g_MissionThreadLogged = true;
      util::log::Write("Replay missions: the replay's own thread (the game thread it runs on) is %u", GetCurrentThreadId());
    }
    for (__int64 entity : missions)
    {
      if (!IsMission(entity))
        continue;
      __int64 data = Q(entity + MissionDataOffset);
      BYTE fields[0x20] = {}, state[MissionStateSize] = {};
      bool read = data && ReadRaw(fields, data + 0x28, sizeof(fields)) && ReadRaw(state, entity + MissionStateFrom, sizeof(state));
      if (!read)
        continue;
      char name[160];
      MissionName(entity, name, sizeof(name));
      // A mission running while it records: its recording's mission
      if (mode == 1 && state[MissionRunningOffset - MissionStateFrom])
      {
        std::lock_guard<std::mutex> lock(g_MissionMutex);
        if (std::find(g_RecordedMissions.begin(), g_RecordedMissions.end(), name) == g_RecordedMissions.end() &&
            g_RecordedMissions.size() < 16)
        {
          g_RecordedMissions.push_back(name);
          util::log::Write("Replay missions: '%s' runs while it records (+ 0x11F set): the recording's mission", name);
        }
      }
      // The rest: the mission's line when it changed (diagnostics only)
      if (!util::DiagnosticsOn())
        continue;
      __int64 flags[3] = { Q(entity + 0x90), Q(entity + 0x98), Q(entity + 0xA0) };
      __int64 subs[2] = { LinkedEntity(entity, MissionSubLevelLink), LinkedEntity(entity, MissionPreSubLevelLink) };
      std::vector<BYTE> now(state, state + sizeof(state));
      now.insert(now.end(), fields, fields + sizeof(fields));
      now.insert(now.end(), (BYTE const*)subs, (BYTE const*)subs + sizeof(subs));
      auto& logged = g_MissionLogged[entity];
      if (logged == now)
        continue;
      bool first = logged.empty();
      logged = now;
      char subKind[96] = "", preKind[96] = "";
      ClassName(subs[0], subKind, sizeof(subKind));
      ClassName(subs[1], preKind, sizeof(preKind));
      float const* f = (float const*)fields;   // data + 0x28 on
      float const* st = (float const*)state;   // entity + 0x110 on
      util::log::Write("Replay missions: %s '%s' (entity 0x%I64X, the replay %s at %.2f s): state + 0x110 %08X, re-enable %.2f, stream out "
        "at %.2f, + 0x11C %08X, + 0x120 %02X %02X %02X, + 0x124 %08X; data Enabled %d, StreamInOnAvailable %d, StreamOutOnCompleted %d "
        "(after %.0f s, radius %.0f), StreamOutOnFail %d, StreamOutOnAbort %d (after %.0f s), re-enable after abort %.0f s / completed "
        "%.0f s; flag sets 0x%I64X 0x%I64X 0x%I64X; SubLevel 0x%I64X (%s), PreMissionSubLevel 0x%I64X (%s)",
        first ? "first look at" : "changed:", name, entity,
        mode == 1 ? "recording" : mode == 2 ? "playing" : "idle", clock, *(UINT const*)&state[0], st[1], st[2], *(UINT const*)&state[12],
        state[16], state[17], state[18], *(UINT const*)&state[20], fields[0x18], fields[0x19], fields[0x1A], f[3], f[2], fields[0x1B],
        fields[0x1C], f[4], f[0], f[1], flags[0], flags[1], flags[2], subs[0], subs[0] ? subKind : "none", subs[1],
        subs[1] ? preKind : "none");
    }
  }

  // Server update thread: a live mission entity of that name with a SubLevel
  __int64 MissionNamed(std::string const& wanted)
  {
    std::vector<__int64> missions;
    {
      std::lock_guard<std::mutex> lock(g_MissionMutex);
      missions = g_Missions;
    }
    for (__int64 entity : missions)
    {
      if (!IsMission(entity))
        continue;
      char name[160];
      MissionName(entity, name, sizeof(name));
      if (wanted == name && LinkedEntity(entity, MissionSubLevelLink))
        return entity;
    }
    return 0;
  }

  // Game thread: the Release mission's partitions looked up by guid in each loaded compartment's
  // registry (read only), each found one's head, and where in it the data made while it recorded is
  void ProbePartitions(char const* when)
  {
    __int64 base = (__int64)g_gameHandle;
    int compartments = 0, registries = 0, lookupSlot = -1, found = 0;
    std::vector<std::pair<__int64, std::string>> made;
    {
      std::lock_guard<std::mutex> lock(g_CreatedMutex);
      made = g_CreatedData;
    }
    std::unordered_map<__int64, std::string> madeAt(made.begin(), made.end());
    for (int c = 0; c < Compartments; ++c)
    {
      __int64 entry = Q(base + CompartmentTableRva + (__int64)c * 8);
      if (!entry)
        continue;
      ++compartments;
      __int64 registry = Q(entry + CompartmentRegistryOffset);
      __int64 vtable = registry ? Q(registry) : 0;
      if (!InImage(vtable))
        continue;
      // Its vtable holds the by-guid lookup (checked, not called)
      int slot = -1;
      for (int k = 0; k < 32 && slot < 0; ++k)
        if (Q(vtable + k * 8) == base + RegistryGuidLookupRva)
          slot = k;
      if (slot < 0)
        continue;
      lookupSlot = slot;
      ++registries;
      __int64 buckets = Q(registry + RegistryBucketsOffset);
      UINT count = D(registry + RegistryBucketCountOffset);
      if (!buckets || !count || count > (1u << 22))
        continue;
      for (auto const& known : KnownPartitions)
      {
        UINT d1 = 0;
        USHORT d2 = 0, d3 = 0;
        memcpy(&d1, known.Guid, 4);
        memcpy(&d2, known.Guid + 4, 2);
        memcpy(&d3, known.Guid + 6, 2);
        UINT bucket = (d1 + d2 + d3) % count;
        __int64 node = Q(buckets + (__int64)bucket * 8);
        for (int steps = 0; node && steps < 256; ++steps, node = Q(node + 0x10))
        {
          BYTE key[16];
          __int64 keyAt = Q(node);
          if (!keyAt || !ReadRaw(key, keyAt, sizeof(key)) || memcmp(key, known.Guid, 16))
            continue;
          ++found;
          __int64 value = Q(node + 8);
          char bundle[160];
          CompartmentName((unsigned)c, bundle, sizeof(bundle));
          util::log::Write("Replay assets: partition %s (%s): in compartment %d '%s', registry 0x%I64X, node 0x%I64X, value 0x%I64X "
            "(the game's lookup gives it + 8), its guid at 0x%I64X", known.Name, when, c, bundle, registry, node, value, keyAt);
          BYTE head[0x100];
          if (value && ReadRaw(head, value, sizeof(head)))
          {
            DumpRows("partition", value, head, sizeof(head));
            // The data made while it recorded, in an array this head points at
            for (int q = 0; q < (int)sizeof(head); q += 8)
            {
              __int64 array = *(__int64 const*)(head + q);
              if (array < 0x10000 || InImage(array))
                continue;
              static __int64 items[1024];
              if (!ReadRaw(items, array, sizeof(items)))
                continue;
              int hits = 0;
              for (int i = 0; i < 1024 && hits < 4; ++i)
              {
                auto it = madeAt.find(items[i]);
                if (it == madeAt.end())
                  continue;
                ++hits;
                util::log::Write("Replay assets:     its + 0x%02X points at 0x%I64X, whose item %d is the data a %s was made from", q,
                  array, i, it->second.c_str());
              }
            }
          }
          break;
        }
      }
    }
    util::log::Write("Replay assets: partitions by guid (%s): %d compartments, %d with a registry whose slot %d is the game's "
      "lookup; %d of the %d Release partitions looked for found; %d creations noted (%d static models, %d references)", when,
      compartments, registries, lookupSlot, found, (int)(sizeof(KnownPartitions) / sizeof(KnownPartitions[0])), (int)made.size(),
      g_StaticCreated.load(), g_ReferencesCreated.load());
  }

  ULONGLONG g_ProbeAt = 0;
  char const* g_ProbeWhen = "";

  // Game thread
  struct SetSeen
  {
    __int64 Data;       // its data when read (another mesh at the same address has other data)
    std::string Name;
  };
  struct Mesh
  {
    bool AtStart = false;         // in the table as it started recording
    double Loaded = -1.0;         // came into the table while it recorded
    bool LoadedLoading = false;
    double Unloaded = -1.0;       // last went out of it while it recorded
    std::set<int> RecSeconds, PlaySeconds;
    std::set<int> GameSeconds, ReplaySeconds;   // while it plays: those the game drew it at, and the replay
    int RecDraws = 0, PlayDraws = 0, ReplayDraws = 0;
    std::set<__int64> RecTextures, PlayTextures;
    // The same by each draw of it (first index, index count), slot by slot:
    // the textures there as they were described then (g_Described)
    std::map<std::pair<UINT, UINT>, std::map<int, std::set<int>>> RecByDraw, PlayByDraw;
  };
  std::unordered_map<__int64, SetSeen> g_Sets;                   // MeshSet -> what it is, as last read
  std::unordered_set<__int64> g_InTable;                         // MeshSets in the table at the last look
  std::unordered_map<__int64, std::string> g_VbNames;            // vertex buffer -> mesh, at the last look
  std::unordered_map<__int64, std::string> g_RecordedVbNames;    // the same, while it recorded
  std::map<std::string, Mesh> g_Meshes;
  std::map<std::string, std::set<int>> g_OtherByMesh;            // while it recorded: the other passes each mesh was drawn in
  std::vector<std::string> g_OtherPassNamesGame;
  int g_OtherUnnamed = 0;                                        // draws there of vertex buffers not in the mesh table
  std::set<int> g_RecSeconds, g_PlaySeconds;                     // seconds with draws noted
  bool g_FirstLook = true;
  int g_Loaded = 0, g_LoadedLoading = 0, g_Unloaded = 0, g_AtStart = 0, g_Made = 0, g_MadeLoading = 0, g_Playbacks = 0;
  int g_BundlesRegistered = 0, g_BundlesLoaded = 0, g_BundlesUnloaded = 0, g_BundlesLoading = 0;
  ULONGLONG g_NextScan = 0;
  FILE* g_File = nullptr;

  void Out(char const* format, ...)
  {
    if (!g_File)
      return;
    va_list args;
    va_start(args, format);
    vfprintf(g_File, format, args);
    va_end(args);
  }

  char const* FormatName(DXGI_FORMAT format)
  {
    switch (format)
    {
    case DXGI_FORMAT_BC1_UNORM: return "BC1";
    case DXGI_FORMAT_BC1_UNORM_SRGB: return "BC1 sRGB";
    case DXGI_FORMAT_BC2_UNORM: return "BC2";
    case DXGI_FORMAT_BC3_UNORM: return "BC3";
    case DXGI_FORMAT_BC3_UNORM_SRGB: return "BC3 sRGB";
    case DXGI_FORMAT_BC4_UNORM: return "BC4";
    case DXGI_FORMAT_BC5_UNORM: return "BC5";
    case DXGI_FORMAT_BC5_SNORM: return "BC5 signed";
    case DXGI_FORMAT_BC6H_UF16: return "BC6H";
    case DXGI_FORMAT_BC7_UNORM: return "BC7";
    case DXGI_FORMAT_BC7_UNORM_SRGB: return "BC7 sRGB";
    case DXGI_FORMAT_R8G8B8A8_UNORM: return "RGBA8";
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return "RGBA8 sRGB";
    case DXGI_FORMAT_B8G8R8A8_UNORM: return "BGRA8";
    case DXGI_FORMAT_R8_UNORM: return "R8";
    case DXGI_FORMAT_R16G16B16A16_FLOAT: return "RGBA16F";
    case DXGI_FORMAT_R32_FLOAT: return "R32F";
    default: return nullptr;
    }
  }

  std::string Describe(TexInfo const& t)
  {
    char format[24];
    char const* name = FormatName(t.Format);
    if (name)
      strcpy_s(format, name);
    else
      sprintf_s(format, "format %d", (int)t.Format);
    char text[160];
    if (t.Depth)
      sprintf_s(text, "%ux%ux%u %s, %u levels", t.Width, t.Height, t.Depth, format, t.Mips);
    else
      sprintf_s(text, "%ux%u%s %s, %u levels", t.Width, t.Height, t.Array > 1 ? " (array)" : "", format, t.Mips);
    std::string out = text;
    if (t.Name[0])
      out += std::string(" '") + t.Name + "'";
    return out;
  }

  // The textures as a draw read them (an address can hold another texture
  // later): each description once
  std::vector<TexInfo> g_Described;
  std::unordered_map<std::string, int> g_DescribedIds;

  // What Describe makes its text from, the same in both
  bool SameDescription(TexInfo const& a, TexInfo const& b)
  {
    return a.Width == b.Width && a.Height == b.Height && a.Depth == b.Depth && a.Mips == b.Mips && a.Array == b.Array &&
      a.Format == b.Format && !strcmp(a.Name, b.Name);
  }

  int DescribedId(TexInfo const& t)
  {
    std::string text = Describe(t);
    auto it = g_DescribedIds.find(text);
    if (it != g_DescribedIds.end())
      return it->second;
    int id = (int)g_Described.size();
    g_Described.push_back(t);
    g_DescribedIds.emplace(text, id);
    return id;
  }

  // (The id it got last while that's still the description: 2026-11-04, the
  // user: "find out why fps drop when recording a replay": a description made
  // and looked up for every texture slot of every draw noted was most of the
  // 23 - 32 ms the scan took on the game thread once a second)
  int DescribedId(TexInfo& t)
  {
    if (t.Described >= 0 && t.Described < (int)g_Described.size() && SameDescription(g_Described[t.Described], t))
      return t.Described;
    t.Described = DescribedId(static_cast<TexInfo const&>(t));
    return t.Described;
  }

  // The mesh table now: each MeshSet in it, and its loaded levels' vertex buffers
  bool ReadTable(std::vector<std::pair<__int64, std::vector<__int64>>>& out)
  {
    out.clear();
    __int64 base = (__int64)g_gameHandle;
    __int64 manager = Q(base + MeshManagerGlobalRva);
    __int64 begin = manager ? Q(manager + MeshTableOffset) : 0, end = manager ? Q(manager + MeshTableOffset + 8) : 0;
    if (!begin || end < begin)
      return false;
    __int64 count = (std::min)((end - begin) / MeshRecordSize, MaxRecords);
    for (__int64 i = 0; i < count; ++i)
    {
      __int64 set = Q(begin + i * MeshRecordSize);
      if (!set || Q(set) != base + MeshSetVTableRva)
        continue;
      std::vector<__int64> vbs;
      __int64 data = Q(set + MeshSetDataOffset);
      for (int lod = 0; lod < MaxLods && data && Q(data + DataLevelsOffset + lod * 8); ++lod)
      {
        __int64 buffer = Q(set + MeshSetLevelsOffset + lod * MeshSetLevelSize + 8);
        if (!buffer || Q(buffer) != base + RenderBufferVTableRva || D(buffer + BufferKindOffset) != 1)
          continue;   // that level isn't loaded
        __int64 d3d = Q(buffer + BufferD3DOffset);
        if (d3d)
          vbs.push_back(d3d);
      }
      out.push_back({ set, std::move(vbs) });
    }
    return true;
  }

  // What a scan took from the render thread (the other passes' draws, the
  // textures made, the draws noted) and hasn't gone through yet: gone through
  // DrainBudgetMs a frame (Drain), and all of it before the next look at the
  // mesh table (each draw is named by the table looked at with it, as
  // before) and when it stops recording or playing. Until 2026-11-04 (the
  // user: "find out why fps drop when recording a replay and fix the fps
  // issue no mistakes") all of it went through in the scan's own frame: 23 -
  // 32 ms of one frame a second on the game thread while it recorded. Each
  // vertex buffer's mesh (g_Meshes, g_OtherByMesh) as that table names it is
  // looked up once a scan.
  const double DrainBudgetMs = 0.5;
  const size_t DrainStep = 64;
  std::vector<std::pair<__int64, int>> g_ScanOthers;
  std::vector<Made> g_ScanMade;
  std::vector<DrawNote> g_ScanNotes;
  size_t g_ScanOtherAt = 0, g_ScanMadeAt = 0, g_ScanNoteAt = 0;
  std::unordered_map<__int64, Mesh*> g_ScanMeshOf;
  std::unordered_map<__int64, std::set<int>*> g_ScanOtherOf;
  // The table as last gone through (its MeshSets, their data and vertex
  // buffers) and the mode then: the same table again in the same mode isn't
  // gone through again (nothing came in or went out, the names are the same)
  std::vector<std::pair<__int64, std::vector<__int64>>> g_LastTable;
  std::vector<__int64> g_LastTableData;
  int g_LastTableMode = -1;

  void ClearScan()
  {
    g_ScanOthers.clear();
    g_ScanMade.clear();
    g_ScanNotes.clear();
    g_ScanOtherAt = g_ScanMadeAt = g_ScanNoteAt = 0;
    g_ScanMeshOf.clear();
    g_ScanOtherOf.clear();
    g_LastTable.clear();
    g_LastTableData.clear();
    g_LastTableMode = -1;
  }

  // At most budgetMs of it (a step of DrainStep at a time; < 0: all of it)
  void Drain(double budgetMs)
  {
    if (g_ScanOtherAt >= g_ScanOthers.size() && g_ScanMadeAt >= g_ScanMade.size() && g_ScanNoteAt >= g_ScanNotes.size())
      return;
    static LARGE_INTEGER frequency{};
    if (!frequency.QuadPart)
      QueryPerformanceFrequency(&frequency);
    LARGE_INTEGER start;
    QueryPerformanceCounter(&start);
    auto spent = [&]() {
      if (budgetMs < 0.0)
        return false;
      LARGE_INTEGER now;
      QueryPerformanceCounter(&now);
      return (now.QuadPart - start.QuadPart) * 1000.0 >= budgetMs * frequency.QuadPart;
    };
    // The other passes' draws, by mesh (named as the G-buffer pass's are)
    while (g_ScanOtherAt < g_ScanOthers.size())
    {
      for (size_t end = (std::min)(g_ScanOthers.size(), g_ScanOtherAt + DrainStep); g_ScanOtherAt < end; ++g_ScanOtherAt)
      {
        auto const& o = g_ScanOthers[g_ScanOtherAt];
        auto known = g_ScanOtherOf.find(o.first);
        if (known == g_ScanOtherOf.end())
        {
          auto it = g_VbNames.find(o.first);
          if (it == g_VbNames.end() && (it = g_RecordedVbNames.find(o.first)) == g_RecordedVbNames.end())
            known = g_ScanOtherOf.emplace(o.first, nullptr).first;
          else
            known = g_ScanOtherOf.emplace(o.first, &g_OtherByMesh[it->second]).first;
        }
        if (known->second)
          known->second->insert(o.second);
        else
          ++g_OtherUnnamed;
      }
      if (spent())
        return;
    }
    while (g_ScanMadeAt < g_ScanMade.size())
    {
      for (size_t end = (std::min)(g_ScanMade.size(), g_ScanMadeAt + DrainStep); g_ScanMadeAt < end; ++g_ScanMadeAt)
      {
        Made const& m = g_ScanMade[g_ScanMadeAt];
        TexInfo t;
        t.Width = m.Desc.Width;
        t.Height = m.Desc.Height;
        t.Mips = m.Desc.MipLevels;
        t.Array = m.Desc.ArraySize;
        t.Format = m.Desc.Format;
        ++g_Made;
        g_MadeLoading += m.Loading;
        Out("%9.2f s  texture made %s (0x%I64X)%s\n", m.Time, Describe(t).c_str(), m.Texture, m.Loading ? "  (a loading screen up)" : "");
      }
      if (spent())
        return;
    }
    // The draws (the textures' descriptions as the render thread last made
    // them: under g_Mutex, a step at a time)
    while (g_ScanNoteAt < g_ScanNotes.size())
    {
      {
        std::lock_guard<std::mutex> lock(g_Mutex);
        for (size_t end = (std::min)(g_ScanNotes.size(), g_ScanNoteAt + DrainStep); g_ScanNoteAt < end; ++g_ScanNoteAt)
        {
          DrawNote const& n = g_ScanNotes[g_ScanNoteAt];
          Mesh*& mesh = g_ScanMeshOf[n.Vb];
          if (!mesh)
          {
            auto it = g_VbNames.find(n.Vb);
            std::string name;
            if (it != g_VbNames.end())
              name = it->second;
            else if ((it = g_RecordedVbNames.find(n.Vb)) != g_RecordedVbNames.end())
              name = it->second;
            else
            {
              char unknown[64];
              sprintf_s(unknown, "(vertex buffer 0x%I64X, not in the mesh table)", n.Vb);
              name = unknown;
            }
            mesh = &g_Meshes[name];
          }
          Mesh& m = *mesh;
          if (n.Mode == 1)
          {
            m.RecSeconds.insert(n.Second);
            m.RecDraws += n.Draws;
            auto& slots = m.RecByDraw[{ n.Start, n.Count }];
            for (auto const& s : n.Slots)
            {
              m.RecTextures.insert(s.second);
              auto t = g_Textures.find(s.second);
              if (t != g_Textures.end())
                slots[s.first].insert(DescribedId(t->second));
            }
            g_RecSeconds.insert(n.Second);
          }
          else if (n.Mode == 2)
          {
            m.PlaySeconds.insert(n.Second);
            (n.Replay ? m.ReplaySeconds : m.GameSeconds).insert(n.Second);
            (n.Replay ? m.ReplayDraws : m.PlayDraws) += n.Draws;
            auto& slots = m.PlayByDraw[{ n.Start, n.Count }];
            for (auto const& s : n.Slots)
            {
              m.PlayTextures.insert(s.second);
              auto t = g_Textures.find(s.second);
              if (t != g_Textures.end())
                slots[s.first].insert(DescribedId(t->second));
            }
            g_PlaySeconds.insert(n.Second);
          }
        }
      }
      if (spent())
        return;
    }
    // All of it gone through
    g_ScanOthers.clear();
    g_ScanMade.clear();
    g_ScanNotes.clear();
    g_ScanOtherAt = g_ScanMadeAt = g_ScanNoteAt = 0;
    if (g_File)
      fflush(g_File);
  }

  // A look at the mesh table (while it records: what came in and went out),
  // and the draws and made textures noted since taken (gone through by Drain)
  void Scan(int mode, double clock, bool loading)
  {
    // The last scan's first, named by its table
    Drain(-1.0);
    std::vector<std::pair<__int64, std::vector<__int64>>> table;
    if (ReadTable(table))
    {
      std::vector<__int64> datas;
      datas.reserve(table.size());
      for (auto const& entry : table)
        datas.push_back(Q(entry.first + MeshSetDataOffset));
      if (g_FirstLook || mode != g_LastTableMode || table != g_LastTable || datas != g_LastTableData)
      {
        std::unordered_set<__int64> now;
        g_VbNames.clear();
        for (size_t i = 0; i < table.size(); ++i)
        {
          auto const& entry = table[i];
          __int64 set = entry.first;
          __int64 data = datas[i];
          auto it = g_Sets.find(set);
          if (it == g_Sets.end() || it->second.Data != data)
          {
            char name[160];
            ReadName(data ? Q(data + DataNameOffset) : 0, name, sizeof(name));
            char unnamed[48];
            sprintf_s(unnamed, "(a mesh with no name, 0x%I64X)", set);
            it = g_Sets.insert_or_assign(set, SetSeen{ data, name[0] ? name : unnamed }).first;
          }
          std::string const& name = it->second.Name;
          now.insert(set);
          for (__int64 vb : entry.second)
          {
            g_VbNames[vb] = name;
            if (mode == 1)
              g_RecordedVbNames[vb] = name;
          }
          if (mode == 1 && !g_InTable.count(set))
          {
            Mesh& m = g_Meshes[name];
            if (g_FirstLook)
            {
              m.AtStart = true;
              ++g_AtStart;
            }
            else
            {
              if (m.Loaded < 0.0)
              {
                m.Loaded = clock;
                m.LoadedLoading = loading;
              }
              ++g_Loaded;
              g_LoadedLoading += loading;
              Out("%9.2f s  mesh in      %s%s\n", clock, name.c_str(), loading ? "  (a loading screen up)" : "");
            }
          }
        }
        if (mode == 1 && !g_FirstLook)
          for (__int64 set : g_InTable)
            if (!now.count(set))
            {
              auto it = g_Sets.find(set);
              std::string name = it != g_Sets.end() ? it->second.Name : "?";
              g_Meshes[name].Unloaded = clock;
              ++g_Unloaded;
              Out("%9.2f s  mesh out     %s%s\n", clock, name.c_str(), loading ? "  (a loading screen up)" : "");
            }
        g_InTable.swap(now);
        if (mode == 1)
          g_FirstLook = false;
        g_LastTable.swap(table);
        g_LastTableData.swap(datas);
        g_LastTableMode = mode;
      }
    }
    g_ScanMeshOf.clear();
    g_ScanOtherOf.clear();
    std::vector<BundleNote> bundles;
    {
      std::lock_guard<std::mutex> lock(g_Mutex);
      g_ScanOthers.swap(g_PendingOther);
      if (g_OtherPassNamesGame.size() != g_OtherPassNamesShared.size())
        g_OtherPassNamesGame = g_OtherPassNamesShared;
      g_ScanNotes.swap(g_Pending);
      g_ScanMade.swap(g_Created);
      bundles.swap(g_BundleNotes);
      for (Made const& m : g_ScanMade)
      {
        TexInfo& t = g_Textures[m.Texture];
        t.Made = m.Time;
        t.MadeLoading = m.Loading;
      }
    }
    for (BundleNote const& b : bundles)
    {
      static char const* const what[] = { "registered", "load      ", "unload    " };
      if (b.What == 0)
        Out("%9.2f s  bundle %s  compartment %u (parent %u, kind %d, thread %u): %s%s\n", b.Time, what[0], b.Compartment, b.Parent,
          b.Kind, b.Thread, b.Name[0] ? b.Name : "?", b.Loading ? "  (a loading screen up)" : "");
      else
        Out("%9.2f s  bundle %s  compartment %u (thread %u): %s%s\n", b.Time, what[b.What], b.Compartment, b.Thread,
          b.Name[0] ? b.Name : "?", b.Loading ? "  (a loading screen up)" : "");
      (b.What == 0 ? g_BundlesRegistered : b.What == 1 ? g_BundlesLoaded : g_BundlesUnloaded) += 1;
      g_BundlesLoading += b.Loading;
    }
    if (g_File)
      fflush(g_File);
  }

  std::string Span(std::set<int> const& seconds)
  {
    if (seconds.empty())
      return "never";
    char text[64];
    sprintf_s(text, "%d - %d s, %d seconds", *seconds.begin(), *seconds.rbegin(), (int)seconds.size());
    return text;
  }

  std::string TexturesOf(std::set<__int64> const& textures, std::unordered_map<__int64, TexInfo> const& info)
  {
    std::string out;
    for (__int64 t : textures)
    {
      auto it = info.find(t);
      if (it == info.end())
        continue;
      if (!out.empty())
        out += "; ";
      out += Describe(it->second);
      if (it->second.Made >= 0.0)
      {
        char made[64];
        sprintf_s(made, " (made at %.1f s%s)", it->second.Made, it->second.MadeLoading ? ", loading screen" : "");
        out += made;
      }
    }
    return out.empty() ? "none seen" : out;
  }

  void StartRecording()
  {
    // (none of the last one's left to go through: its meshes are let go here)
    ClearScan();
    g_Sets.clear();
    g_InTable.clear();
    g_VbNames.clear();
    g_RecordedVbNames.clear();
    g_Meshes.clear();
    g_CreateDumps = 0;
    g_CreateLines = 0;
    g_ReferenceDumps = 0;
    g_ReferenceLines = 0;
    g_StaticCreated = 0;
    g_ReferencesCreated = 0;
    {
      std::lock_guard<std::mutex> lock(g_CreatedMutex);
      g_CreatedData.clear();
    }
    g_OtherByMesh.clear();
    g_OtherPassNamesGame.clear();
    g_OtherUnnamed = 0;
    g_RecSeconds.clear();
    g_PlaySeconds.clear();
    g_Described.clear();
    g_DescribedIds.clear();
    g_FirstLook = true;
    g_Loaded = g_LoadedLoading = g_Unloaded = g_AtStart = g_Made = g_MadeLoading = g_Playbacks = 0;
    g_BundlesRegistered = g_BundlesLoaded = g_BundlesUnloaded = g_BundlesLoading = 0;
    {
      std::lock_guard<std::mutex> lock(g_Mutex);
      g_Textures.clear();
      g_Pending.clear();
      g_Created.clear();
      g_BundleNotes.clear();
      g_PendingOther.clear();
    }
    g_Dropped = false;
    if (g_File)
      fclose(g_File);
    g_File = _fsopen(FileName, "w", _SH_DENYWR);
    Out("Replay assets (CT_MirrorsEdge): the bundles, meshes and textures of a recording, and what its playbacks missed\n"
      "Times are the recording's. 'mesh in' / 'mesh out': into or out of the game's mesh table while it recorded.\n\n"
      "The game's bundles in its compartments as it started (compartment: name):\n");
    int compartments = 0;
    for (unsigned c = 0; c < (unsigned)Compartments; ++c)
    {
      if (!Q((__int64)g_gameHandle + CompartmentTableRva + (__int64)c * 8))
        continue;
      char name[160];
      CompartmentName(c, name, sizeof(name));
      Out("  %u: %s\n", c, name[0] ? name : "?");
      ++compartments;
    }
    Out("  (%d)\n\n", compartments);
    g_NextScan = 0;
  }

  void FinishRecording(double clock)
  {
    Scan(1, clock, g_Loading.load());
    Drain(-1.0);
    std::unordered_map<__int64, TexInfo> textures;
    {
      std::lock_guard<std::mutex> lock(g_Mutex);
      textures = g_Textures;
    }
    int drawn = 0, drawnLoaded = 0;
    for (auto const& m : g_Meshes)
      if (m.second.RecDraws)
      {
        ++drawn;
        drawnLoaded += m.second.Loaded >= 0.0;
      }
    char summary[1024];
    _snprintf_s(summary, _TRUNCATE, "the recording (%.1f s): bundles %d registered, %d loaded, %d unloaded (%d of those while a loading screen "
      "was up); %d meshes in the game's mesh table as it started, %d came in while it recorded (%d while a loading screen was "
      "up), %d went out; %d textures made (%d while a loading screen was up); %d meshes drawn in the G-buffer pass (%d of them "
      "came in while it recorded), %d seconds with draws noted%s", clock, g_BundlesRegistered, g_BundlesLoaded, g_BundlesUnloaded,
      g_BundlesLoading, g_AtStart, g_Loaded, g_LoadedLoading, g_Unloaded, g_Made, g_MadeLoading, drawn, drawnLoaded,
      (int)g_RecSeconds.size(), g_Dropped ? " (some notes left out: too many)" : "");
    util::log::Write("Replay assets: %s. The lists: Cinematic Tools\\ReplayAssets.log", summary);
    Out("\n== The end of %s\n\nDrawn while it recorded (name: when; draws; how it came; its textures):\n", summary);
    for (auto const& m : g_Meshes)
    {
      Mesh const& mesh = m.second;
      if (!mesh.RecDraws)
        continue;
      char how[96];
      if (mesh.Loaded >= 0.0)
        sprintf_s(how, "came in at %.1f s%s", mesh.Loaded, mesh.LoadedLoading ? " (loading screen)" : "");
      else
        strcpy_s(how, mesh.AtStart ? "there as it started" : "not seen in the mesh table");
      Out("  %s: %s; %d draws; %s; %s\n", m.first.c_str(), Span(mesh.RecSeconds).c_str(), mesh.RecDraws, how,
        TexturesOf(mesh.RecTextures, textures).c_str());
    }
    Out("\nCame in while it recorded but not drawn in the G-buffer pass then:\n");
    for (auto const& m : g_Meshes)
      if (m.second.Loaded >= 0.0 && !m.second.RecDraws)
        Out("  %s (came in at %.1f s%s%s)\n", m.first.c_str(), m.second.Loaded, m.second.LoadedLoading ? ", loading screen" : "",
          m.second.Unloaded >= 0.0 ? ", went out again" : "");
    // Drawn in other passes only: the replay's kept draws come from the
    // G-buffer pass and the two it keeps besides (glass, effects)
    int onlyOther = 0;
    Out("\nDrawn while it recorded in other passes only, never in the G-buffer pass (name: the passes, listed below):\n");
    for (auto const& m : g_OtherByMesh)
    {
      auto g = g_Meshes.find(m.first);
      if (g != g_Meshes.end() && g->second.RecDraws)
        continue;
      std::string passes;
      for (int id : m.second)
        passes += (passes.empty() ? "#" : ", #") + std::to_string(id);
      Out("  %s: %s\n", m.first.c_str(), passes.c_str());
      ++onlyOther;
    }
    Out("The passes (in the order first seen; the picture's own size is the G-buffer's):\n");
    for (size_t i = 0; i < g_OtherPassNamesGame.size(); ++i)
      Out("  #%d: %s\n", (int)i, g_OtherPassNamesGame[i].c_str());
    Out("(%d draws there of vertex buffers not in the mesh table left out)\n", g_OtherUnnamed);
    util::log::Write("Replay assets: %d meshes drawn in other passes only while it recorded (%d of the game's other passes seen); "
      "the list: Cinematic Tools\\ReplayAssets.log", onlyOther, (int)g_OtherPassNamesGame.size());
    if (g_File)
      fflush(g_File);
  }

  void StartPlaying()
  {
    for (auto& m : g_Meshes)
    {
      m.second.PlaySeconds.clear();
      m.second.GameSeconds.clear();
      m.second.ReplaySeconds.clear();
      m.second.PlayDraws = m.second.ReplayDraws = 0;
      m.second.PlayTextures.clear();
      m.second.PlayByDraw.clear();
    }
    g_PlaySeconds.clear();
    ++g_Playbacks;
    if (!g_File)
      g_File = _fsopen(FileName, "a", _SH_DENYWR);
    g_NextScan = 0;
  }

  // The same kind of texture (format, shape, layers), whatever its size: a
  // level streamed in or out changes only its size
  bool SameKind(TexInfo const& a, TexInfo const& b)
  {
    return a.Format == b.Format && a.Dimension == b.Dimension && a.Array == b.Array && a.Width && a.Height && b.Width &&
      b.Height && (unsigned long long)a.Width * b.Height == (unsigned long long)b.Width * a.Height &&
      (unsigned long long)a.Width * b.Depth == (unsigned long long)b.Width * a.Depth;
  }

  enum SlotMatch { Slot_Same, Slot_Resized, Slot_Other };

  // One texture slot of a draw drawn both times, what it read while it
  // recorded against while it played: each texture there either time needs
  // one of its kind the other time (else other); of each kind, the biggest
  // recorded and the biggest played are the same size (else another
  // resolution)
  SlotMatch CompareSlot(std::set<int> const& rec, std::set<int> const& play)
  {
    bool resized = false;
    for (int side = 0; side < 2; ++side)
      for (int a : side ? play : rec)
      {
        UINT mine = 0, theirs = 0;
        for (int b : side ? play : rec)
          if (SameKind(g_Described[a], g_Described[b]))
            mine = (std::max)(mine, g_Described[b].Width);
        for (int b : side ? rec : play)
          if (SameKind(g_Described[a], g_Described[b]))
            theirs = (std::max)(theirs, g_Described[b].Width);
        if (!theirs)
          return Slot_Other;
        resized = resized || mine != theirs;
      }
    return resized ? Slot_Resized : Slot_Same;
  }

  std::string Descriptions(std::set<int> const& set)
  {
    std::string out;
    for (int id : set)
      out += (out.empty() ? "" : "; ") + Describe(g_Described[id]);
    return out.empty() ? "nothing" : out;
  }

  bool Around(std::set<int> const& seconds, int second)
  {
    return seconds.count(second - 1) || seconds.count(second) || seconds.count(second + 1);
  }

  void FinishPlaying(double clock)
  {
    Scan(2, clock, g_Loading.load());
    Drain(-1.0);
    if (g_RecSeconds.empty())
      return;
    std::unordered_set<std::string> inTable;
    for (__int64 set : g_InTable)
    {
      auto it = g_Sets.find(set);
      if (it != g_Sets.end())
        inTable.insert(it->second.Name);
    }
    struct Finding
    {
      std::string Name;
      std::string Text;
    };
    std::vector<Finding> missing, partly, otherTextures, resized;
    int expected = 0, whole = 0, byReplay = 0, notReached = 0;
    for (auto const& m : g_Meshes)
    {
      Mesh const& mesh = m.second;
      if (!mesh.RecDraws)
        continue;
      int covered = 0, drawn = 0;
      for (int s : mesh.RecSeconds)
        if (Around(g_PlaySeconds, s))
        {
          ++covered;
          drawn += Around(mesh.PlaySeconds, s);
        }
      if (!covered)
      {
        ++notReached;
        continue;
      }
      ++expected;
      byReplay += mesh.ReplayDraws > 0;
      char text[400];
      if (!drawn)
      {
        sprintf_s(text, "drawn while it recorded at %s (%d draws), none of the %d played; in the mesh table now: %s; %s",
          Span(mesh.RecSeconds).c_str(), mesh.RecDraws, covered, inTable.count(m.first) ? "yes" : "no",
          mesh.Loaded >= 0.0 ? "came in while it recorded" : mesh.AtStart ? "there as it started" : "not seen in the mesh table");
        missing.push_back({ m.first, text });
        continue;
      }
      if (drawn < covered)
      {
        sprintf_s(text, "drawn at %d of the %d seconds played that it was drawn at while it recorded (%s by the game, %s by the "
          "replay)", drawn, covered, mesh.PlayDraws ? "some" : "none", mesh.ReplayDraws ? "some" : "none");
        partly.push_back({ m.first, text });
      }
      else
        ++whole;
      // Its textures, by each draw of it drawn both times, slot by slot: a
      // slot with a texture of a kind the other time didn't have there has
      // other textures; one whose kinds only changed their biggest size,
      // another resolution (see CompareSlot)
      int otherDraws = 0, resizedDraws = 0, bothDraws = 0;
      std::string firstOther, firstResized;
      static std::set<int> const none;
      for (auto const& draw : mesh.RecByDraw)
      {
        auto played = mesh.PlayByDraw.find(draw.first);
        if (played == mesh.PlayByDraw.end())
          continue;
        ++bothDraws;
        std::set<int> slots;
        for (auto const& s : draw.second)
          slots.insert(s.first);
        for (auto const& s : played->second)
          slots.insert(s.first);
        std::string otherText, resizedText;
        for (int slot : slots)
        {
          auto r = draw.second.find(slot);
          auto p = played->second.find(slot);
          std::set<int> const& rec = r != draw.second.end() ? r->second : none;
          std::set<int> const& play = p != played->second.end() ? p->second : none;
          SlotMatch match = CompareSlot(rec, play);
          if (match == Slot_Same)
            continue;
          char head[48];
          sprintf_s(head, "slot %d recorded with ", slot);
          std::string& text = match == Slot_Other ? otherText : resizedText;
          text += (text.empty() ? "" : " | ") + std::string(head) + Descriptions(rec) + ", played with " + Descriptions(play);
        }
        char where[64];
        sprintf_s(where, "the draw from index %u, %u indices: ", draw.first.first, draw.first.second);
        if (!otherText.empty())
        {
          if (!otherDraws)
            firstOther = where + otherText + (resizedText.empty() ? "" : " | " + resizedText);
          ++otherDraws;
        }
        else if (!resizedText.empty())
        {
          if (!resizedDraws)
            firstResized = where + resizedText;
          ++resizedDraws;
        }
      }
      char counts[96];
      if (otherDraws)
      {
        sprintf_s(counts, "%d of its %d draws drawn both times; ", otherDraws, bothDraws);
        otherTextures.push_back({ m.first, counts + firstOther });
      }
      else if (resizedDraws)
      {
        sprintf_s(counts, "%d of its %d draws drawn both times; ", resizedDraws, bothDraws);
        resized.push_back({ m.first, counts + firstResized });
      }
    }
    // The meshes the replay drew itself (its kept draws of the game's: the
    // models, people, guns and drones the game didn't have)
    std::vector<Finding> drawnByReplay;
    for (auto const& m : g_Meshes)
    {
      Mesh const& mesh = m.second;
      if (mesh.ReplaySeconds.empty())
        continue;
      int alsoGame = 0;
      for (int s : mesh.ReplaySeconds)
        alsoGame += mesh.GameSeconds.count(s) != 0;
      char text[300];
      sprintf_s(text, "by the replay at %s (%d draws), the game drew it too at %d of those seconds; while it recorded: %s",
        Span(mesh.ReplaySeconds).c_str(), mesh.ReplayDraws, alsoGame, mesh.RecDraws ? Span(mesh.RecSeconds).c_str() : "not drawn");
      drawnByReplay.push_back({ m.first, text });
    }
    char summary[1024];
    _snprintf_s(summary, _TRUNCATE, "playback %d (%d seconds with draws, up to %.1f s): of the %d meshes drawn while it recorded at the seconds "
      "played, %d drawn then too (%d of them by the replay's own drawing), %d missing, %d missing at some of them; by each draw "
      "drawn both times, %d with other textures and %d with the same textures at another resolution; %d drawn while it recorded "
      "only at seconds not played", g_Playbacks, (int)g_PlaySeconds.size(), clock, expected, whole + (int)partly.size(), byReplay,
      (int)missing.size(), (int)partly.size(), (int)otherTextures.size(), (int)resized.size(), notReached);
    util::log::Write("Replay assets: %s. The lists: Cinematic Tools\\ReplayAssets.log", summary);
    for (int i = 0; i < (int)missing.size() && i < MaxLogNames; ++i)
      util::log::Write("Replay assets:   missing: %s (%s)", missing[i].Name.c_str(), missing[i].Text.c_str());
    if ((int)missing.size() > MaxLogNames)
      util::log::Write("Replay assets:   ... and %d more missing (in the file)", (int)missing.size() - MaxLogNames);
    util::log::Write("Replay assets: %d meshes drawn by the replay itself", (int)drawnByReplay.size());
    for (int i = 0; i < (int)drawnByReplay.size() && i < MaxLogNames; ++i)
      util::log::Write("Replay assets:   drawn %s: %s", drawnByReplay[i].Text.c_str(), drawnByReplay[i].Name.c_str());
    if ((int)drawnByReplay.size() > MaxLogNames)
      util::log::Write("Replay assets:   ... and %d more drawn by the replay (in the file)", (int)drawnByReplay.size() - MaxLogNames);
    Out("\n== The end of %s\n\nDrawn by the replay itself:\n", summary);
    for (auto const& f : drawnByReplay)
      Out("  %s: drawn %s\n", f.Name.c_str(), f.Text.c_str());
    Out("\nMissing:\n");
    for (auto const& f : missing)
      Out("  %s: %s\n", f.Name.c_str(), f.Text.c_str());
    Out("\nMissing at some of the seconds:\n");
    for (auto const& f : partly)
      Out("  %s: %s\n", f.Name.c_str(), f.Text.c_str());
    Out("\nDrawn with other textures (a draw drawn both times with a slot whose texture had another format or shape, or none):\n");
    for (auto const& f : otherTextures)
      Out("  %s: %s\n", f.Name.c_str(), f.Text.c_str());
    Out("\nDrawn with its textures at another resolution (slot by slot, only the biggest size of a kind differs):\n");
    for (auto const& f : resized)
      Out("  %s: %s\n", f.Name.c_str(), f.Text.c_str());
    if (g_File)
      fflush(g_File);
  }
}

void replay_assetlog::Update(int mode, double clock, bool loading)
{
  // Diagnostics off: only what the mission's world needs (the replay's mode,
  // the recording's missions); no scans, file, probes or summaries
  if (!util::DiagnosticsOn())
  {
    if (mode == 1 && g_Mode.load() != 1)
    {
      std::lock_guard<std::mutex> lock(g_MissionMutex);
      g_RecordedMissions.clear();
    }
    g_Mode = mode;
    g_Clock = clock;
    g_Loading = loading;
    LookAtMissions(mode, clock);
    return;
  }
  int was = g_Mode.load();
  if (mode != was)
  {
    // What was noted under the one before goes to it first
    if (was == 1)
    {
      FinishRecording(g_Clock.load());
      ProbePartitions("as it stopped recording");
    }
    else if (was == 2)
      FinishPlaying(g_Clock.load());
    g_Clock = clock;
    g_Loading = loading;
    if (mode == 1)
    {
      StartRecording();
      std::lock_guard<std::mutex> lock(g_MissionMutex);
      g_RecordedMissions.clear();
    }
    else if (mode == 2)
      StartPlaying();
    g_Mode = mode;
    g_ProbeAt = mode == 0 ? 0 : GetTickCount64() + (mode == 1 ? 8000 : 10000);
    g_ProbeWhen = mode == 1 ? "8 s into the recording" : "10 s into the playback (its bundles loaded again)";
  }
  if (g_ProbeAt && GetTickCount64() >= g_ProbeAt && !loading)
  {
    g_ProbeAt = 0;
    ProbePartitions(g_ProbeWhen);
  }
  g_Clock = clock;
  g_Loading = loading;
  LookAtMissions(mode, clock);
  if (mode == 0)
    return;
  ULONGLONG now = GetTickCount64();
  if (now < g_NextScan)
  {
    // What the last scan took, a little each frame (see Drain)
    Drain(DrainBudgetMs);
    return;
  }
  g_NextScan = now + ScanEveryMs;
  Scan(mode, clock, loading);
}

std::vector<std::string> replay_assetlog::RecordedMissions()
{
  std::lock_guard<std::mutex> lock(g_MissionMutex);
  return g_RecordedMissions;
}

void replay_assetlog::SetRecordedMissions(std::vector<std::string> const& names)
{
  std::lock_guard<std::mutex> lock(g_MissionMutex);
  g_RecordedMissions = names;
}

bool replay_assetlog::MissionWorld()
{
  return g_MissionWorld.load();
}

void replay_assetlog::SetMissionWorld(bool on)
{
  g_MissionWorld = on;
}

void replay_assetlog::SetFaithPlace(bool known, float const* place, float routeClearance)
{
  std::lock_guard<std::mutex> lock(g_FaithMutex);
  g_FaithKnown = known && place;
  if (g_FaithKnown)
    memcpy(g_FaithPlace, place, sizeof(g_FaithPlace));
  g_FaithRoute = routeClearance;
}

namespace
{
  // How far a place is from a mission's measured trigger box (beyond the
  // shapes' reach; negative inside), FLT_MAX if none was measured
  float BoxClearance(std::string const& mission, float const* place)
  {
    for (MissionTriggerBox const& b : MissionTriggerBoxes)
    {
      if (mission != b.Mission)
        continue;
      float d2 = 0.0f;
      bool inside = true;
      for (int i = 0; i < 3; ++i)
      {
        float lo = b.Min[i] - b.Reach, hi = b.Max[i] + b.Reach;
        float out = place[i] < lo ? lo - place[i] : place[i] > hi ? place[i] - hi : 0.0f;
        inside = inside && out == 0.0f;
        d2 += out * out;
      }
      return inside ? -1.0f : sqrtf(d2);
    }
    return FLT_MAX;
  }

  // Server update thread: why the mission's world can't be asked for now
  // (or kept, with keep), nullptr if it can
  char const* MissionWorldRefused(std::string const& mission, bool keep, char* why, size_t size)
  {
    std::vector<__int64> missions;
    {
      std::lock_guard<std::mutex> lock(g_MissionMutex);
      missions = g_Missions;
    }
    for (__int64 entity : missions)
      if (IsMission(entity) && MissionRunning(entity))
      {
        char name[160];
        MissionName(entity, name, sizeof(name));
        sprintf_s(why, size, "the mission '%s' runs now", name);
        return why;
      }
    bool known;
    float place[3], route;
    {
      std::lock_guard<std::mutex> lock(g_FaithMutex);
      known = g_FaithKnown;
      memcpy(place, g_FaithPlace, sizeof(place));
      route = g_FaithRoute;
    }
    if (!known)
    {
      sprintf_s(why, size, "where the real Faith is isn't known");
      return why;
    }
    float box = BoxClearance(mission, place);
    if (box < (keep ? KeepBoxClearance : AskBoxClearance))
    {
      sprintf_s(why, size, "the real Faith (%.0f %.0f %.0f) is %s the mission's player triggers (%.0f m from their area; "
        "%.0f m needed)", place[0], place[1], place[2], box < 0 ? "among" : "near", box < 0 ? 0.0f : box,
        keep ? KeepBoxClearance : AskBoxClearance);
      return why;
    }
    if (route < (keep ? KeepRouteClearance : AskRouteClearance))
    {
      sprintf_s(why, size, "the real Faith (%.0f %.0f %.0f) is %.0f m from where the recording went (%.0f m needed: the "
        "mission's player triggers are along it)", place[0], place[1], place[2], route,
        keep ? KeepRouteClearance : AskRouteClearance);
      return why;
    }
    return nullptr;
  }
}

// Server update thread, before each server frame (see the mission's world)
void replay_assetlog::ServerUpdate()
{
  if (!g_MissionsHooked || g_shutdown)
    return;
  int mode = g_Mode.load();
  int was = g_MissionServerMode;
  g_MissionServerMode = mode;
  if (mode == 2 && was != 2)
  {
    g_MissionAskDone = false;
    g_MissionWaitLogged = false;
  }
  // While it plays: the asked worlds streamed out again if Faith comes near
  // their triggers or a mission starts
  if (mode == 2 && !g_MissionAsked.empty())
  {
    std::vector<std::string> names = RecordedMissions();
    char why[300];
    char const* refused = nullptr;
    for (std::string const& name : names)
      if ((refused = MissionWorldRefused(name, true, why, sizeof(why))) != nullptr)
        break;
    if (refused)
    {
      for (__int64 entity : g_MissionAsked)
      {
        bool live = false;
        {
          std::lock_guard<std::mutex> lock(g_MissionMutex);
          live = std::find(g_Missions.begin(), g_Missions.end(), entity) != g_Missions.end();
        }
        if (!live || !IsMission(entity) || MissionRunning(entity))
          continue;
        char name[160];
        MissionName(entity, name, sizeof(name));
        util::log::Write("Replay missions: '%s''s world streamed out again during the playback (0x143A7A0A0(entity 0x%I64X, false)): "
          "%s", name, entity, refused);
        weather::WorldGoing("the replay's mission world streamed out");
        oMissionSubLevel(entity, 0);
      }
      g_MissionAsked.clear();
    }
  }
  if (mode == 2 && !g_MissionAskDone && g_MissionWorld.load())
  {
    std::vector<std::string> names = RecordedMissions();
    char why[300];
    char const* refused = nullptr;
    for (std::string const& name : names)
      if ((refused = MissionWorldRefused(name, false, why, sizeof(why))) != nullptr)
        break;
    if (refused)
    {
      // Asked for once it's clear (Faith moved away, the mission ended)
      if (!g_MissionWaitLogged && !names.empty())
      {
        g_MissionWaitLogged = true;
        util::log::Write("Replay missions: the recording's mission's world isn't asked for yet: %s", refused);
      }
      return;
    }
    g_MissionAskDone = true;
    for (std::string const& name : names)
    {
      __int64 entity = MissionNamed(name);
      if (!entity)
      {
        util::log::Write("Replay missions: the recording's mission '%s' has no mission entity with a SubLevel in this level: its world "
          "isn't asked for", name.c_str());
        continue;
      }
      if (MissionRunning(entity))
      {
        util::log::Write("Replay missions: '%s' runs now: its world is the game's already", name.c_str());
        continue;
      }
      __int64 sub = LinkedEntity(entity, MissionSubLevelLink);
      char kind[96];
      ClassName(sub, kind, sizeof(kind));
      util::log::Write("Replay missions: the game asked to stream '%s''s world in for the playback: 0x143A7A0A0(entity 0x%I64X, "
        "true), its SubLevel entity 0x%I64X (%s), on the server's update thread %u", name.c_str(), entity, sub, kind,
        GetCurrentThreadId());
      oMissionSubLevel(entity, 1);
      g_MissionAsked.push_back(entity);
    }
  }
  else if (mode != 2 && was == 2 && !g_MissionAsked.empty())
  {
    for (__int64 entity : g_MissionAsked)
    {
      bool live = false;
      {
        std::lock_guard<std::mutex> lock(g_MissionMutex);
        live = std::find(g_Missions.begin(), g_Missions.end(), entity) != g_Missions.end();
      }
      char name[160];
      if (!live || !IsMission(entity))
        continue;
      MissionName(entity, name, sizeof(name));
      if (MissionRunning(entity))
      {
        util::log::Write("Replay missions: '%s' runs now: its world stays as the playback ends", name);
        continue;
      }
      util::log::Write("Replay missions: the playback ended: the game asked to stream '%s''s world out again (0x143A7A0A0(entity "
        "0x%I64X, false), as an abort does)", name, entity);
      weather::WorldGoing("the replay's mission world streamed out");
      oMissionSubLevel(entity, 0);
    }
    g_MissionAsked.clear();
  }
}

void replay_assetlog::Present(ID3D11Device* pDevice)
{
  if (!pDevice || g_shutdown)
    return;
  if (!g_Hooked)
  {
    g_Hooked = true;
    // The textures, bundles and creations: noted for the asset log only (diagnostics)
    if (util::DiagnosticsOn())
    {
      void** table = *(void***)pDevice;
      bool hooked = util::hooks::CreateGameHook("ReplayAssetsCreateTexture2D", (__int64)table[CreateTexture2DSlot], (PVOID)hCreateTexture2D,
        (LPVOID*)&oCreateTexture2D);
      if (!hooked)
        util::log::Warning("Replay assets: the device's CreateTexture2D couldn't be hooked: the textures made while it records aren't noted");
      HookBundles();
      HookCreation();
    }
    // (the missions' also serve the mission's world)
    HookMissions();
  }
  // The rest is the asset log's (diagnostics only)
  if (!util::DiagnosticsOn())
    return;
  // The frame just drawn, if it was a sampled one: its draws handed over
  replay_gamedraws::CostTimer timer(replay_gamedraws::Cost_AssetPresent);
  int mode = g_Mode.load(std::memory_order_relaxed);
  if (mode != 0 && (!g_FrameGame.empty() || !g_FrameReplay.empty()))
  {
    int second = (int)floor(g_Clock.load());
    std::lock_guard<std::mutex> lock(g_Mutex);
    for (int replay = 0; replay < 2; ++replay)
      for (auto& seen : replay ? g_FrameReplay : g_FrameGame)
      {
        if (g_Pending.size() >= MaxPending)
        {
          g_Dropped = true;
          break;
        }
        g_Pending.push_back({ std::get<0>(seen.first), std::get<1>(seen.first), std::get<2>(seen.first), mode, second, replay != 0,
          seen.second.Draws, std::move(seen.second.Slots) });
      }
  }
  if (mode == 1 && !g_FrameOther.empty())
  {
    std::lock_guard<std::mutex> lock(g_Mutex);
    for (auto const& o : g_FrameOther)
    {
      if (g_PendingOther.size() >= MaxPendingOther)
      {
        g_Dropped = true;
        break;
      }
      g_PendingOther.push_back(o);
    }
    if (g_OtherPassNamesShared.size() != g_OtherPassNames.size())
      g_OtherPassNamesShared = g_OtherPassNames;
  }
  g_FrameOther.clear();
  g_FrameGame.clear();
  g_FrameReplay.clear();
  g_FrameTextures.clear();
  ++g_Frame;
}

void replay_assetlog::OnDraw(ID3D11DeviceContext* pContext, UINT indexCount, UINT startIndex)
{
  if (!util::DiagnosticsOn() || !Sampled())
    return;
  replay_gamedraws::CostTimer timer(replay_gamedraws::Cost_AssetDraws);
  if (!replay_gamedraws::InMainPass(pContext))
  {
    NoteOther(pContext);
    return;
  }
  ID3D11Buffer* vb = nullptr;
  UINT stride = 0, offset = 0;
  pContext->IAGetVertexBuffers(0, 1, &vb, &stride, &offset);
  if (!vb)
    return;
  ID3D11ShaderResourceView* views[ViewSlots] = {};
  pContext->PSGetShaderResources(0, ViewSlots, views);
  Note(g_FrameGame, (__int64)vb, startIndex, indexCount, views, ViewSlots);
  for (ID3D11ShaderResourceView* view : views)
    if (view)
      view->Release();
  vb->Release();
}

void replay_assetlog::OnReplayDraw(ID3D11Buffer* vb, UINT startIndex, UINT indexCount, ID3D11ShaderResourceView* const* views,
  int count)
{
  if (!util::DiagnosticsOn() || !Sampled() || !vb)
    return;
  Note(g_FrameReplay, (__int64)vb, startIndex, indexCount, views, (std::min)(count, ViewSlots));
}
