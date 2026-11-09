#include "ReplayStore.h"
#include "../Main.h"
#include "../Util/Util.h"
#include <Windows.h>
#include <bcrypt.h>
#include <compressapi.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <thread>

#pragma comment(lib, "Cabinet.lib")
#pragma comment(lib, "bcrypt.lib")

using Microsoft::WRL::ComPtr;

// How (2026-10-27):
// - Shader code: the draw log's CreateVertexShader / CreatePixelShader hooks
//   (put in at the first Present) hand each shader's code here, kept by its
//   object (the same code kept once). Input layouts: the device's
//   CreateInputLayout (vtable slot 11) is hooked here at the first Present:
//   each layout's elements and the signature it was made with. A kept draw
//   using a shader or layout made before that (or a geometry, hull or domain
//   shader: not hooked) can't be saved; the save counts them.
// - Buffers and textures: copied on the graphics card into staging copies of
//   the store's at Present (CopyResource, at most MaxStaging bytes at once),
//   read ReadAfterFrames frames later without waiting (DO_NOT_WAIT), every
//   detail level and layer, rows packed tight. States and samplers: their
//   descriptions (GetDesc). Views: their description and their resource.
// - Each part with bytes is hashed (SHA-256, Windows' BCrypt) and written to
//   Recordings\Parts\<hash>.part unless it's there already (another
//   recording's), packed with LZMS (Windows' Compression API) when that makes
//   it smaller. The recording's file has an index of its parts (kind,
//   description, hash) and its sections, packed the same way.
// - Loading reads the sections and parts on the store's thread and makes the
//   objects at Present (CreateBuffer, CreateTexture2D, ... with the bytes as
//   their first data; never changed after: immutable where the binding
//   allows), CreateBudgetMs a frame. Nothing of the game's is written.
namespace
{
  const char* const RecordingsFolder = ".\\Cinematic Tools\\Recordings";
  const char* const PartsFolder = ".\\Cinematic Tools\\Recordings\\Parts";
  const char* const RecordingFile = "Recording.ctr";
  const char Magic[8] = { 'C', 'T', 'R', 'E', 'C', '0', '0', '1' };
  const char PartMagic[8] = { 'C', 'T', 'P', 'A', 'R', 'T', '0', '1' };
  const unsigned TagInfo = replay_store::Tag('I', 'N', 'F', 'O'), TagParts = replay_store::Tag('P', 'R', 'T', 'S');
  const int CreateInputLayoutSlot = 11;           // ID3D11Device
  const size_t MaxStaging = 256u << 20;           // copies on the graphics card waiting to be read
  const size_t MaxQueued = 512u << 20;            // read back, waiting to be packed and written
  const size_t MaxPartBytes = 768u << 20;         // one buffer or texture
  const size_t MaxCodeBytes = 512u << 20;         // all shader code kept
  const unsigned ReadAfterFrames = 2, GiveUpFrames = 600;
  const double CreateBudgetMs = 6.0;
  const size_t MaxLoadAhead = 768u << 20;         // read from disk, waiting to be made
  const size_t MaxSectionBytes = (size_t)4 << 30;
  const long long MaxKeys = 8192;
  const size_t KeySpacing = 0x1000;

  // LZMS for the sections (frame data packs far smaller with it); XPRESS
  // with Huffman for the parts (2026-10-27 21:28: a save's 124 MB of parts,
  // mostly block-compressed textures, took 106 s with LZMS)
  // 2026-11-01 (the user: "Improve the way that replays save on the disk and
  // size"): a block-compressed texture's bytes regrouped before XPRESS, each
  // byte of a block with the same byte of the others (BC1 / BC4: 8-byte blocks,
  // the others 16: the colour ends together, the index bits together, which
  // repeat far more), when that packs smaller
  enum Method : unsigned { Method_Raw = 0, Method_Lzms = 1, Method_XpressHuff = 2, Method_XpressSplit8 = 3, Method_XpressSplit16 = 4 };
  enum Kind : unsigned
  {
    Kind_Buffer = 1, Kind_Texture1D, Kind_Texture2D, Kind_Texture3D, Kind_View, Kind_Sampler, Kind_Blend, Kind_Depth,
    Kind_Raster, Kind_Vs, Kind_Ps, Kind_Gs, Kind_Hs, Kind_Ds, Kind_Layout
  };

  typedef std::shared_ptr<std::vector<unsigned char> const> Code;

  // ---- Packing and hashing ----

  // Packed with the method's algorithm when that's smaller, else as it is
  void Pack(std::vector<unsigned char>&& in, std::vector<unsigned char>& out, unsigned& method, unsigned wanted = Method_Lzms)
  {
    method = Method_Raw;
    out.clear();
    COMPRESSOR_HANDLE compressor = nullptr;
    DWORD algorithm = wanted == Method_XpressHuff ? COMPRESS_ALGORITHM_XPRESS_HUFF : COMPRESS_ALGORITHM_LZMS;
    if (in.size() >= 256 && CreateCompressor(algorithm, nullptr, &compressor))
    {
      SIZE_T needed = 0;
      Compress(compressor, in.data(), in.size(), nullptr, 0, &needed);
      if (needed && needed < (SIZE_T)-1)
      {
        try
        {
          out.resize(needed);
          SIZE_T size = 0;
          if (Compress(compressor, in.data(), in.size(), out.data(), out.size(), &size) && size < in.size())
          {
            out.resize(size);
            method = wanted == Method_XpressHuff ? Method_XpressHuff : Method_Lzms;
          }
        }
        catch (std::bad_alloc const&)
        {
        }
      }
      CloseCompressor(compressor);
    }
    if (method == Method_Raw)
      out.swap(in);
  }

  // A buffer's bytes regrouped by their place in each group of n (the tail
  // that doesn't fill one left as it is), and back
  void SplitBytes(std::vector<unsigned char> const& in, std::vector<unsigned char>& out, size_t n)
  {
    out.resize(in.size());
    size_t groups = in.size() / n;
    for (size_t k = 0; k < n; ++k)
      for (size_t i = 0; i < groups; ++i)
        out[k * groups + i] = in[i * n + k];
    for (size_t i = groups * n; i < in.size(); ++i)
      out[i] = in[i];
  }

  void JoinBytes(std::vector<unsigned char>& data, size_t n)
  {
    std::vector<unsigned char> out(data.size());
    size_t groups = data.size() / n;
    for (size_t k = 0; k < n; ++k)
      for (size_t i = 0; i < groups; ++i)
        out[i * n + k] = data[k * groups + i];
    for (size_t i = groups * n; i < data.size(); ++i)
      out[i] = data[i];
    data.swap(out);
  }

  // A part's bytes packed: XPRESS as they are, and regrouped by blocks of
  // blockBytes when it's a block-compressed texture's (the smaller kept)
  void PackPart(std::vector<unsigned char>&& raw, size_t blockBytes, std::vector<unsigned char>& out, unsigned& method)
  {
    std::vector<unsigned char> split;
    if (blockBytes && raw.size() >= blockBytes * 64)
    {
      try
      {
        SplitBytes(raw, split, blockBytes);
      }
      catch (std::bad_alloc const&)
      {
        split.clear();
      }
    }
    Pack(std::move(raw), out, method, Method_XpressHuff);
    if (split.empty())
      return;
    std::vector<unsigned char> packed;
    unsigned splitMethod = Method_Raw;
    Pack(std::move(split), packed, splitMethod, Method_XpressHuff);
    if (splitMethod == Method_XpressHuff && packed.size() < out.size())
    {
      out.swap(packed);
      method = blockBytes == 8 ? Method_XpressSplit8 : Method_XpressSplit16;
    }
  }

  // A texture format's block bytes (0: not block-compressed)
  size_t BlockBytesOf(DXGI_FORMAT f)
  {
    if ((f >= DXGI_FORMAT_BC1_TYPELESS && f <= DXGI_FORMAT_BC1_UNORM_SRGB) || (f >= DXGI_FORMAT_BC4_TYPELESS && f <= DXGI_FORMAT_BC4_SNORM))
      return 8;
    if ((f >= DXGI_FORMAT_BC2_TYPELESS && f <= DXGI_FORMAT_BC3_UNORM_SRGB) || (f >= DXGI_FORMAT_BC5_TYPELESS && f <= DXGI_FORMAT_BC5_SNORM) ||
        (f >= DXGI_FORMAT_BC6H_TYPELESS && f <= DXGI_FORMAT_BC7_UNORM_SRGB))
      return 16;
    return 0;
  }

  bool Unpack(unsigned method, unsigned char const* in, size_t size, size_t raw, std::vector<unsigned char>& out)
  {
    if (method == Method_XpressSplit8 || method == Method_XpressSplit16)
    {
      if (!Unpack(Method_XpressHuff, in, size, raw, out))
        return false;
      try
      {
        JoinBytes(out, method == Method_XpressSplit8 ? 8 : 16);
      }
      catch (std::bad_alloc const&)
      {
        return false;
      }
      return true;
    }
    if (method == Method_Raw)
    {
      if (size != raw)
        return false;
      out.assign(in, in + size);
      return true;
    }
    if (method != Method_Lzms && method != Method_XpressHuff)
      return false;
    DECOMPRESSOR_HANDLE decompressor = nullptr;
    if (!CreateDecompressor(method == Method_XpressHuff ? COMPRESS_ALGORITHM_XPRESS_HUFF : COMPRESS_ALGORITHM_LZMS, nullptr,
        &decompressor))
      return false;
    bool ok = false;
    try
    {
      out.resize(raw);
      SIZE_T got = 0;
      ok = Decompress(decompressor, in, size, out.data(), raw, &got) && got == raw;
    }
    catch (std::bad_alloc const&)
    {
    }
    CloseDecompressor(decompressor);
    return ok;
  }

  std::string Sha256(unsigned char const* data, size_t size)
  {
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    unsigned char digest[32] = {};
    bool ok = BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0)) &&
      BCRYPT_SUCCESS(BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0));
    for (size_t at = 0; ok && at < size;)
    {
      ULONG n = (ULONG)(std::min)(size - at, (size_t)1 << 30);
      ok = BCRYPT_SUCCESS(BCryptHashData(hash, (PUCHAR)data + at, n, 0));
      at += n;
    }
    ok = ok && BCRYPT_SUCCESS(BCryptFinishHash(hash, digest, sizeof(digest), 0));
    if (hash)
      BCryptDestroyHash(hash);
    if (algorithm)
      BCryptCloseAlgorithmProvider(algorithm, 0);
    if (!ok)
      return "";
    char hex[65];
    for (int i = 0; i < 32; ++i)
      sprintf_s(hex + i * 2, 3, "%02x", digest[i]);
    return hex;
  }

  unsigned long long Fnv64(void const* data, size_t size)
  {
    unsigned long long h = 1469598103934665603ull;
    unsigned char const* p = (unsigned char const*)data;
    for (size_t i = 0; i < size; ++i)
      h = (h ^ p[i]) * 1099511628211ull;
    return h;
  }

  // ---- Files ----

  bool WriteAll(HANDLE file, void const* data, size_t size)
  {
    unsigned char const* p = (unsigned char const*)data;
    while (size)
    {
      DWORD n = (DWORD)(std::min)(size, (size_t)1 << 30), written = 0;
      if (!WriteFile(file, p, n, &written, nullptr) || written != n)
        return false;
      p += n;
      size -= n;
    }
    return true;
  }

  bool ReadAll(HANDLE file, void* data, size_t size)
  {
    unsigned char* p = (unsigned char*)data;
    while (size)
    {
      DWORD n = (DWORD)(std::min)(size, (size_t)1 << 30), got = 0;
      if (!ReadFile(file, p, n, &got, nullptr) || got != n)
        return false;
      p += n;
      size -= n;
    }
    return true;
  }

  bool Exists(std::string const& path)
  {
    DWORD attributes = GetFileAttributesA(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
  }

  unsigned long long SizeOf(std::string const& path)
  {
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &data))
      return 0;
    return ((unsigned long long)data.nFileSizeHigh << 32) | data.nFileSizeLow;
  }

  std::string PartPath(std::string const& hash)
  {
    return std::string(PartsFolder) + "\\" + hash + ".part";
  }

  // A section or a part: its method, its sizes, its bytes
  bool WriteBlock(HANDLE file, unsigned tag, bool withTag, std::vector<unsigned char>&& raw, bool pack,
    unsigned long long* written, unsigned algorithm = Method_Lzms)
  {
    unsigned long long rawSize = raw.size();
    std::vector<unsigned char> packed;
    unsigned method = Method_Raw;
    if (pack)
      Pack(std::move(raw), packed, method, algorithm);
    else
      packed.swap(raw);
    unsigned long long packedSize = packed.size();
    bool ok = (!withTag || WriteAll(file, &tag, sizeof(tag))) && WriteAll(file, &method, sizeof(method)) &&
      WriteAll(file, &rawSize, sizeof(rawSize)) && WriteAll(file, &packedSize, sizeof(packedSize)) &&
      WriteAll(file, packed.data(), packed.size());
    if (written)
      *written += packedSize + 20 + (withTag ? 4 : 0);
    return ok;
  }

  // A part's block: packed already (PackPart)
  bool WritePacked(HANDLE file, unsigned method, unsigned long long rawSize, std::vector<unsigned char> const& packed,
    unsigned long long* written)
  {
    unsigned long long packedSize = packed.size();
    bool ok = WriteAll(file, &method, sizeof(method)) && WriteAll(file, &rawSize, sizeof(rawSize)) &&
      WriteAll(file, &packedSize, sizeof(packedSize)) && WriteAll(file, packed.data(), packed.size());
    if (written)
      *written += packedSize + 20;
    return ok;
  }

  bool ReadBlock(HANDLE file, std::vector<unsigned char>& out, size_t maxRaw)
  {
    unsigned method = 0;
    unsigned long long raw = 0, packed = 0;
    if (!ReadAll(file, &method, sizeof(method)) || !ReadAll(file, &raw, sizeof(raw)) || !ReadAll(file, &packed, sizeof(packed)) ||
        raw > maxRaw || packed > maxRaw + (1 << 20))
      return false;
    try
    {
      std::vector<unsigned char> bytes((size_t)packed);
      if (!ReadAll(file, bytes.data(), bytes.size()))
        return false;
      if (method == Method_Raw)
      {
        if (raw != packed)
          return false;
        out.swap(bytes);
        return true;
      }
      return Unpack(method, bytes.data(), bytes.size(), (size_t)raw, out);
    }
    catch (std::bad_alloc const&)
    {
      return false;
    }
  }

  // ---- Texture rows ----

  UINT BitsPerPixel(DXGI_FORMAT format)
  {
    switch (format)
    {
    case DXGI_FORMAT_R32G32B32A32_TYPELESS: case DXGI_FORMAT_R32G32B32A32_FLOAT: case DXGI_FORMAT_R32G32B32A32_UINT:
    case DXGI_FORMAT_R32G32B32A32_SINT:
      return 128;
    case DXGI_FORMAT_R32G32B32_TYPELESS: case DXGI_FORMAT_R32G32B32_FLOAT: case DXGI_FORMAT_R32G32B32_UINT:
    case DXGI_FORMAT_R32G32B32_SINT:
      return 96;
    case DXGI_FORMAT_R16G16B16A16_TYPELESS: case DXGI_FORMAT_R16G16B16A16_FLOAT: case DXGI_FORMAT_R16G16B16A16_UNORM:
    case DXGI_FORMAT_R16G16B16A16_UINT: case DXGI_FORMAT_R16G16B16A16_SNORM: case DXGI_FORMAT_R16G16B16A16_SINT:
    case DXGI_FORMAT_R32G32_TYPELESS: case DXGI_FORMAT_R32G32_FLOAT: case DXGI_FORMAT_R32G32_UINT: case DXGI_FORMAT_R32G32_SINT:
    case DXGI_FORMAT_R32G8X24_TYPELESS: case DXGI_FORMAT_D32_FLOAT_S8X24_UINT: case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
    case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
      return 64;
    case DXGI_FORMAT_R10G10B10A2_TYPELESS: case DXGI_FORMAT_R10G10B10A2_UNORM: case DXGI_FORMAT_R10G10B10A2_UINT:
    case DXGI_FORMAT_R11G11B10_FLOAT: case DXGI_FORMAT_R8G8B8A8_TYPELESS: case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: case DXGI_FORMAT_R8G8B8A8_UINT: case DXGI_FORMAT_R8G8B8A8_SNORM:
    case DXGI_FORMAT_R8G8B8A8_SINT: case DXGI_FORMAT_R16G16_TYPELESS: case DXGI_FORMAT_R16G16_FLOAT:
    case DXGI_FORMAT_R16G16_UNORM: case DXGI_FORMAT_R16G16_UINT: case DXGI_FORMAT_R16G16_SNORM: case DXGI_FORMAT_R16G16_SINT:
    case DXGI_FORMAT_R32_TYPELESS: case DXGI_FORMAT_D32_FLOAT: case DXGI_FORMAT_R32_FLOAT: case DXGI_FORMAT_R32_UINT:
    case DXGI_FORMAT_R32_SINT: case DXGI_FORMAT_R24G8_TYPELESS: case DXGI_FORMAT_D24_UNORM_S8_UINT:
    case DXGI_FORMAT_R24_UNORM_X8_TYPELESS: case DXGI_FORMAT_X24_TYPELESS_G8_UINT: case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
    case DXGI_FORMAT_B8G8R8A8_UNORM: case DXGI_FORMAT_B8G8R8X8_UNORM: case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
    case DXGI_FORMAT_B8G8R8A8_TYPELESS: case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: case DXGI_FORMAT_B8G8R8X8_TYPELESS:
    case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
      return 32;
    case DXGI_FORMAT_R8G8_TYPELESS: case DXGI_FORMAT_R8G8_UNORM: case DXGI_FORMAT_R8G8_UINT: case DXGI_FORMAT_R8G8_SNORM:
    case DXGI_FORMAT_R8G8_SINT: case DXGI_FORMAT_R16_TYPELESS: case DXGI_FORMAT_R16_FLOAT: case DXGI_FORMAT_D16_UNORM:
    case DXGI_FORMAT_R16_UNORM: case DXGI_FORMAT_R16_UINT: case DXGI_FORMAT_R16_SNORM: case DXGI_FORMAT_R16_SINT:
    case DXGI_FORMAT_B5G6R5_UNORM: case DXGI_FORMAT_B5G5R5A1_UNORM: case DXGI_FORMAT_B4G4R4A4_UNORM:
      return 16;
    case DXGI_FORMAT_R8_TYPELESS: case DXGI_FORMAT_R8_UNORM: case DXGI_FORMAT_R8_UINT: case DXGI_FORMAT_R8_SNORM:
    case DXGI_FORMAT_R8_SINT: case DXGI_FORMAT_A8_UNORM:
      return 8;
    default:
      return 0;
    }
  }

  // A level's rows (of pixels, or of 4x4 blocks) and the bytes of each, packed
  // tight; false for a format the store doesn't know
  bool LevelRows(DXGI_FORMAT format, UINT width, UINT height, size_t& rowBytes, UINT& rows)
  {
    width = (std::max)(width, 1u);
    height = (std::max)(height, 1u);
    size_t block = 0;
    switch (format)
    {
    case DXGI_FORMAT_BC1_TYPELESS: case DXGI_FORMAT_BC1_UNORM: case DXGI_FORMAT_BC1_UNORM_SRGB:
    case DXGI_FORMAT_BC4_TYPELESS: case DXGI_FORMAT_BC4_UNORM: case DXGI_FORMAT_BC4_SNORM:
      block = 8;
      break;
    case DXGI_FORMAT_BC2_TYPELESS: case DXGI_FORMAT_BC2_UNORM: case DXGI_FORMAT_BC2_UNORM_SRGB:
    case DXGI_FORMAT_BC3_TYPELESS: case DXGI_FORMAT_BC3_UNORM: case DXGI_FORMAT_BC3_UNORM_SRGB:
    case DXGI_FORMAT_BC5_TYPELESS: case DXGI_FORMAT_BC5_UNORM: case DXGI_FORMAT_BC5_SNORM:
    case DXGI_FORMAT_BC6H_TYPELESS: case DXGI_FORMAT_BC6H_UF16: case DXGI_FORMAT_BC6H_SF16:
    case DXGI_FORMAT_BC7_TYPELESS: case DXGI_FORMAT_BC7_UNORM: case DXGI_FORMAT_BC7_UNORM_SRGB:
      block = 16;
      break;
    default:
      break;
    }
    if (block)
    {
      rowBytes = (size_t)((width + 3) / 4) * block;
      rows = (height + 3) / 4;
      return true;
    }
    UINT bits = BitsPerPixel(format);
    if (!bits)
      return false;
    rowBytes = ((size_t)width * bits + 7) / 8;
    rows = height;
    return true;
  }

  // A texture's description as the store needs it: its dimension (1 - 3),
  // size, levels, layers (6 a cube's face counted), format
  struct TexShape
  {
    int Dimension;
    UINT Width, Height, Depth, Mips, Layers;
    DXGI_FORMAT Format;
  };

  // The bytes all its levels and layers take, packed tight (0: can't)
  size_t TexBytes(TexShape const& t)
  {
    size_t total = 0;
    for (UINT layer = 0; layer < t.Layers; ++layer)
      for (UINT mip = 0; mip < t.Mips; ++mip)
      {
        size_t rowBytes = 0;
        UINT rows = 0;
        if (!LevelRows(t.Format, t.Width >> mip, t.Height >> mip, rowBytes, rows))
          return 0;
        total += rowBytes * rows * (std::max)(t.Depth >> mip, 1u);
      }
    return total;
  }

  // ---- What the game made ----

  struct LayoutElement
  {
    std::string Semantic;
    D3D11_INPUT_ELEMENT_DESC Desc;   // its SemanticName isn't kept (Semantic is)
  };
  struct LayoutNote
  {
    std::vector<LayoutElement> Elements;
    Code Signature;
  };

  std::mutex g_CodeMutex;   // g_Code, g_CodeByHash, g_Layouts, g_CodeBytes
  std::unordered_map<__int64, Code> g_Code;                        // shader object -> its code
  std::unordered_multimap<unsigned long long, Code> g_CodeByHash;  // each code once
  std::unordered_map<__int64, std::shared_ptr<LayoutNote const>> g_Layouts;
  size_t g_CodeBytes = 0;
  bool g_CodeFullLogged = false;

  // The code kept once (null when there's no room for more; the caller holds g_CodeMutex)
  Code SharedCode(void const* code, size_t size)
  {
    unsigned long long hash = Fnv64(code, size);
    auto range = g_CodeByHash.equal_range(hash);
    for (auto it = range.first; it != range.second; ++it)
      if (it->second->size() == size && !memcmp(it->second->data(), code, size))
        return it->second;
    if (g_CodeBytes + size > MaxCodeBytes)
    {
      if (!g_CodeFullLogged)
        util::log::Warning("Replay recordings: %zu MB of shader code kept, no more is (draws using later shaders can't be saved)",
          g_CodeBytes >> 20);
      g_CodeFullLogged = true;
      return nullptr;
    }
    Code kept = std::make_shared<std::vector<unsigned char> const>((unsigned char const*)code, (unsigned char const*)code + size);
    g_CodeByHash.emplace(hash, kept);
    g_CodeBytes += size;
    return kept;
  }

  typedef HRESULT(STDMETHODCALLTYPE* tCreateInputLayout)(ID3D11Device*, D3D11_INPUT_ELEMENT_DESC const*, UINT, void const*, SIZE_T,
    ID3D11InputLayout**);
  tCreateInputLayout oCreateInputLayout = nullptr;

  HRESULT STDMETHODCALLTYPE hCreateInputLayout(ID3D11Device* pDevice, D3D11_INPUT_ELEMENT_DESC const* descs, UINT count,
    void const* signature, SIZE_T size, ID3D11InputLayout** out)
  {
    HRESULT hr = oCreateInputLayout(pDevice, descs, count, signature, size, out);
    if (SUCCEEDED(hr) && out && *out && descs && signature && size && count <= D3D11_IA_VERTEX_INPUT_STRUCTURE_ELEMENT_COUNT &&
        !g_shutdown)
    {
      try
      {
        auto note = std::make_shared<LayoutNote>();
        for (UINT i = 0; i < count; ++i)
        {
          LayoutElement e{ descs[i].SemanticName ? descs[i].SemanticName : "", descs[i] };
          e.Desc.SemanticName = nullptr;
          note->Elements.push_back(std::move(e));
        }
        std::lock_guard<std::mutex> lock(g_CodeMutex);
        note->Signature = SharedCode(signature, size);
        if (note->Signature)
          g_Layouts[(__int64)*out] = note;
        else
          g_Layouts.erase((__int64)*out);
      }
      catch (std::bad_alloc const&)
      {
      }
    }
    return hr;
  }

  Code CodeOf(void const* object)
  {
    std::lock_guard<std::mutex> lock(g_CodeMutex);
    auto it = g_Code.find((__int64)object);
    return it != g_Code.end() ? it->second : nullptr;
  }

  std::shared_ptr<LayoutNote const> LayoutOf(void const* object)
  {
    std::lock_guard<std::mutex> lock(g_CodeMutex);
    auto it = g_Layouts.find((__int64)object);
    return it != g_Layouts.end() ? it->second : nullptr;
  }

  template <class T> std::vector<unsigned char> DescBytes(T const& desc)
  {
    return std::vector<unsigned char>((unsigned char const*)&desc, (unsigned char const*)&desc + sizeof(T));
  }

  template <class T> bool DescOf(std::vector<unsigned char> const& bytes, T& desc)
  {
    if (bytes.size() != sizeof(T))
      return false;
    memcpy(&desc, bytes.data(), sizeof(T));
    return true;
  }

  // ---- A save's parts ----

  struct Part
  {
    unsigned Kind = 0;
    int Ref = -1;                         // a view's resource
    std::vector<unsigned char> Desc;      // its description (the D3D11 *_DESC)
    std::vector<LayoutElement> Elements;  // an input layout's
    TexShape Tex{};                       // a texture's
    size_t Bytes = 0;                     // its bytes (read back, or code)
    ComPtr<ID3D11Resource> Resource;      // a buffer or texture, held until read
    Code Bytecode;                        // a shader's code, a layout's signature
    std::vector<unsigned char> Data;      // read back (until written)
    bool HasData = false;                 // bytes in a part file
    bool Read = false, Failed = false, Written = false;
    std::string Hash;
    // Read back (render thread)
    ComPtr<ID3D11Resource> Staging;
    unsigned StagedAt = 0;
  };
}

struct replay_store::SaveParts
{
  std::mutex Mutex;
  std::vector<Part> List;
  std::unordered_map<__int64, int> ByObject;   // by its IUnknown
  int Unsaved = 0;
  int UnsavedByKind[16] = {};                  // for the log (0: not a kind the store knows)
};

struct replay_store::LoadParts
{
  std::vector<ComPtr<IUnknown>> Objects;
};

namespace
{
  int AddLocked(replay_store::SaveParts& parts, IUnknown* object)
  {
    if (!object)
      return -1;
    ComPtr<IUnknown> identity;
    if (FAILED(object->QueryInterface(IID_PPV_ARGS(identity.GetAddressOf()))) || !identity)
      return -2;
    auto found = parts.ByObject.find((__int64)identity.Get());
    if (found != parts.ByObject.end())
      return found->second;
    Part p;
    bool ok = false;
    ComPtr<ID3D11ShaderResourceView> view;
    ComPtr<ID3D11Buffer> buffer;
    ComPtr<ID3D11Texture1D> t1;
    ComPtr<ID3D11Texture2D> t2;
    ComPtr<ID3D11Texture3D> t3;
    ComPtr<ID3D11SamplerState> sampler;
    ComPtr<ID3D11BlendState> blend;
    ComPtr<ID3D11DepthStencilState> depth;
    ComPtr<ID3D11RasterizerState> raster;
    ComPtr<ID3D11VertexShader> vs;
    ComPtr<ID3D11PixelShader> ps;
    ComPtr<ID3D11GeometryShader> gs;
    ComPtr<ID3D11HullShader> hs;
    ComPtr<ID3D11DomainShader> ds;
    ComPtr<ID3D11InputLayout> layout;
    if (SUCCEEDED(object->QueryInterface(IID_PPV_ARGS(view.GetAddressOf()))))
    {
      D3D11_SHADER_RESOURCE_VIEW_DESC d{};
      view->GetDesc(&d);
      ComPtr<ID3D11Resource> resource;
      view->GetResource(resource.GetAddressOf());
      p.Kind = Kind_View;
      p.Desc = DescBytes(d);
      p.Ref = AddLocked(parts, resource.Get());
      ok = p.Ref >= 0;
    }
    else if (SUCCEEDED(object->QueryInterface(IID_PPV_ARGS(buffer.GetAddressOf()))))
    {
      D3D11_BUFFER_DESC d{};
      buffer->GetDesc(&d);
      p.Kind = Kind_Buffer;
      p.Desc = DescBytes(d);
      p.Bytes = d.ByteWidth;
      p.Resource = buffer;
      p.HasData = true;
      ok = d.ByteWidth && d.ByteWidth <= MaxPartBytes;
    }
    else if (SUCCEEDED(object->QueryInterface(IID_PPV_ARGS(t2.GetAddressOf()))))
    {
      D3D11_TEXTURE2D_DESC d{};
      t2->GetDesc(&d);
      p.Kind = Kind_Texture2D;
      p.Desc = DescBytes(d);
      p.Tex = { 2, d.Width, d.Height, 1, d.MipLevels, d.ArraySize, d.Format };
      p.Resource = t2;
      ok = d.SampleDesc.Count == 1;
    }
    else if (SUCCEEDED(object->QueryInterface(IID_PPV_ARGS(t1.GetAddressOf()))))
    {
      D3D11_TEXTURE1D_DESC d{};
      t1->GetDesc(&d);
      p.Kind = Kind_Texture1D;
      p.Desc = DescBytes(d);
      p.Tex = { 1, d.Width, 1, 1, d.MipLevels, d.ArraySize, d.Format };
      p.Resource = t1;
      ok = true;
    }
    else if (SUCCEEDED(object->QueryInterface(IID_PPV_ARGS(t3.GetAddressOf()))))
    {
      D3D11_TEXTURE3D_DESC d{};
      t3->GetDesc(&d);
      p.Kind = Kind_Texture3D;
      p.Desc = DescBytes(d);
      p.Tex = { 3, d.Width, d.Height, d.Depth, d.MipLevels, 1, d.Format };
      p.Resource = t3;
      ok = true;
    }
    else if (SUCCEEDED(object->QueryInterface(IID_PPV_ARGS(sampler.GetAddressOf()))))
    {
      D3D11_SAMPLER_DESC d{};
      sampler->GetDesc(&d);
      p.Kind = Kind_Sampler;
      p.Desc = DescBytes(d);
      ok = true;
    }
    else if (SUCCEEDED(object->QueryInterface(IID_PPV_ARGS(blend.GetAddressOf()))))
    {
      D3D11_BLEND_DESC d{};
      blend->GetDesc(&d);
      p.Kind = Kind_Blend;
      p.Desc = DescBytes(d);
      ok = true;
    }
    else if (SUCCEEDED(object->QueryInterface(IID_PPV_ARGS(depth.GetAddressOf()))))
    {
      D3D11_DEPTH_STENCIL_DESC d{};
      depth->GetDesc(&d);
      p.Kind = Kind_Depth;
      p.Desc = DescBytes(d);
      ok = true;
    }
    else if (SUCCEEDED(object->QueryInterface(IID_PPV_ARGS(raster.GetAddressOf()))))
    {
      D3D11_RASTERIZER_DESC d{};
      raster->GetDesc(&d);
      p.Kind = Kind_Raster;
      p.Desc = DescBytes(d);
      ok = true;
    }
    else if (SUCCEEDED(object->QueryInterface(IID_PPV_ARGS(vs.GetAddressOf()))))
    {
      p.Kind = Kind_Vs;
      p.Bytecode = CodeOf(vs.Get());
    }
    else if (SUCCEEDED(object->QueryInterface(IID_PPV_ARGS(ps.GetAddressOf()))))
    {
      p.Kind = Kind_Ps;
      p.Bytecode = CodeOf(ps.Get());
    }
    else if (SUCCEEDED(object->QueryInterface(IID_PPV_ARGS(gs.GetAddressOf()))))
      p.Kind = Kind_Gs;   // their code isn't kept (not hooked)
    else if (SUCCEEDED(object->QueryInterface(IID_PPV_ARGS(hs.GetAddressOf()))))
      p.Kind = Kind_Hs;
    else if (SUCCEEDED(object->QueryInterface(IID_PPV_ARGS(ds.GetAddressOf()))))
      p.Kind = Kind_Ds;
    else if (SUCCEEDED(object->QueryInterface(IID_PPV_ARGS(layout.GetAddressOf()))))
    {
      p.Kind = Kind_Layout;
      if (std::shared_ptr<LayoutNote const> note = LayoutOf(layout.Get()))
      {
        p.Elements = note->Elements;
        p.Bytecode = note->Signature;
      }
    }
    if (p.Kind >= Kind_Vs && p.Kind <= Kind_Layout)
    {
      ok = p.Bytecode != nullptr;
      if (ok)
      {
        p.HasData = true;
        p.Read = true;
        p.Bytes = p.Bytecode->size();
      }
    }
    if (p.Tex.Dimension)
    {
      p.Bytes = TexBytes(p.Tex);
      p.HasData = true;
      ok = ok && p.Bytes && p.Bytes <= MaxPartBytes && p.Tex.Mips && p.Tex.Layers;
    }
    int id = -2;
    if (ok)
    {
      id = (int)parts.List.size();
      parts.List.push_back(std::move(p));
    }
    else
    {
      ++parts.Unsaved;
      ++parts.UnsavedByKind[p.Kind < 16 ? p.Kind : 0];
    }
    parts.ByObject[(__int64)identity.Get()] = id;
    return id;
  }

  // ---- The store's work ----

  struct SaveJob
  {
    replay_store::SaveRequest Request;
    std::string Folder;       // its folder's name
    std::string When;
    std::atomic<bool> Started{ false };
    size_t Queued = 0;        // read back, not written yet (under Parts->Mutex)
    size_t Staged = 0;        // render thread
    int ReadCount = 0, Total = 0;
    std::atomic<int> Written{ 0 }, Shared{ 0 }, Failed{ 0 };
    std::atomic<unsigned long long> NewBytes{ 0 };
  };

  struct LoadEntry
  {
    unsigned Kind = 0;
    int Ref = -1;
    std::vector<unsigned char> Desc;
    std::vector<LayoutElement> Elements;
    std::string Hash;
    std::vector<unsigned char> Data;
    bool Ready = false;       // its bytes read (or none needed)
    bool Bad = false;         // its part file couldn't be read
  };

  struct LoadJob
  {
    std::string Folder;
    std::mutex Mutex;         // Entries' Ready / Data / Bad, Ahead
    std::vector<LoadEntry> Entries;
    std::vector<std::pair<unsigned, std::vector<unsigned char>>> Sections;
    std::atomic<bool> Indexed{ false }, ReadDone{ false }, Failed{ false }, Made{ false };
    size_t Ahead = 0;         // bytes read, not made yet
    std::atomic<size_t> Next{ 0 };   // the next to make (made at Present)
    int Missing = 0;          // render thread
    std::shared_ptr<replay_store::LoadParts> Parts = std::make_shared<replay_store::LoadParts>();
  };

  std::mutex g_Mutex;         // g_Tasks, g_Save, g_Load, g_Status, g_List, g_ListStale, g_Loaded
  std::condition_variable g_Wake;
  std::deque<std::function<void()>> g_Tasks;
  std::shared_ptr<SaveJob> g_Save;          // the one being read back and written
  std::shared_ptr<LoadJob> g_Load;          // the one being read and made
  std::shared_ptr<LoadJob> g_LoadedJob;     // made, for TakeLoaded
  std::string g_Status;
  std::vector<replay_store::Listed> g_List;
  bool g_ListStale = true;
  bool g_ThreadStarted = false;
  std::atomic<unsigned long long> g_PartsBytes{ 0 };
  bool g_Hooked = false;
  unsigned g_Frame = 0;
  BYTE* g_KeyMemory = nullptr;
  std::once_flag g_KeyOnce;

  void SetStatus(std::string const& text)
  {
    std::lock_guard<std::mutex> lock(g_Mutex);
    g_Status = text;
  }

  void Worker()
  {
    for (;;)
    {
      std::function<void()> task;
      {
        std::unique_lock<std::mutex> lock(g_Mutex);
        g_Wake.wait(lock, [] { return !g_Tasks.empty() || g_shutdown; });
        if (g_shutdown)
          return;
        task = std::move(g_Tasks.front());
        g_Tasks.pop_front();
      }
      try
      {
        task();
      }
      catch (std::exception const& e)
      {
        util::log::Error("Replay recordings: %s", e.what());
        SetStatus(std::string("Stopped: ") + e.what());
      }
    }
  }

  void Queue(std::function<void()> task)
  {
    std::lock_guard<std::mutex> lock(g_Mutex);
    g_Tasks.push_back(std::move(task));
    if (!g_ThreadStarted)
    {
      g_ThreadStarted = true;
      std::thread(Worker).detach();
    }
    g_Wake.notify_all();
  }

  std::string Clean(std::string text)
  {
    for (char& c : text)
      if (strchr("\\/:*?\"<>|", c) || (unsigned char)c < 32)
        c = '_';
    size_t first = text.find_first_not_of(" .");
    text = first == std::string::npos ? "" : text.substr(first, 60);
    while (!text.empty() && (text.back() == ' ' || text.back() == '.'))
      text.pop_back();
    return text;
  }

  std::map<std::string, std::string> g_Renamed;   // under g_Mutex: a folder's new name, renamed in this session

  // The parts index: each part's kind, reference, description and hash
  void WriteIndex(replay_store::Out& out, std::vector<Part> const& list)
  {
    out.Put((unsigned)list.size());
    for (Part const& p : list)
    {
      out.Put(p.Kind);
      out.Put(p.Ref);
      out.Text(p.Hash);
      out.Put((unsigned)p.Desc.size());
      out.Bytes(p.Desc.data(), p.Desc.size());
      out.Put((unsigned)p.Elements.size());
      for (LayoutElement const& e : p.Elements)
      {
        out.Text(e.Semantic);
        out.Put(e.Desc.SemanticIndex);
        out.Put((unsigned)e.Desc.Format);
        out.Put(e.Desc.InputSlot);
        out.Put(e.Desc.AlignedByteOffset);
        out.Put((unsigned)e.Desc.InputSlotClass);
        out.Put(e.Desc.InstanceDataStepRate);
      }
    }
  }

  bool ReadIndex(replay_store::In& in, std::vector<LoadEntry>& entries)
  {
    unsigned count = 0;
    if (!in.Get(count) || count > 1000000)
      return false;
    entries.resize(count);
    for (LoadEntry& e : entries)
    {
      unsigned descSize = 0, elements = 0;
      if (!in.Get(e.Kind) || !in.Get(e.Ref) || !in.Text(e.Hash, 128) || !in.Get(descSize) || descSize > 4096)
        return false;
      e.Desc.resize(descSize);
      if (!in.Bytes(e.Desc.data(), descSize) || !in.Get(elements) || elements > D3D11_IA_VERTEX_INPUT_STRUCTURE_ELEMENT_COUNT)
        return false;
      e.Elements.resize(elements);
      for (LayoutElement& l : e.Elements)
      {
        unsigned format = 0, slotClass = 0;
        if (!in.Text(l.Semantic, 256) || !in.Get(l.Desc.SemanticIndex) || !in.Get(format) || !in.Get(l.Desc.InputSlot) ||
            !in.Get(l.Desc.AlignedByteOffset) || !in.Get(slotClass) || !in.Get(l.Desc.InstanceDataStepRate))
          return false;
        l.Desc.Format = (DXGI_FORMAT)format;
        l.Desc.InputSlotClass = (D3D11_INPUT_CLASSIFICATION)slotClass;
        l.Desc.SemanticName = nullptr;
      }
    }
    return in.Ok();
  }

  // The store's thread: a save's parts written as they're read back, then its file
  void RunSave(std::shared_ptr<SaveJob> job)
  {
    replay_store::SaveParts& parts = *job->Request.Parts;
    CreateDirectoryA(".\\Cinematic Tools", nullptr);
    CreateDirectoryA(RecordingsFolder, nullptr);
    CreateDirectoryA(PartsFolder, nullptr);
    {
      std::lock_guard<std::mutex> lock(g_Mutex);
      g_Save = job;
    }
    job->Started = true;
    size_t total = 0;
    {
      std::lock_guard<std::mutex> lock(parts.Mutex);
      total = parts.List.size();
    }
    // Each part, in order, once it's read
    for (size_t i = 0; i < total;)
    {
      std::vector<unsigned char> data;
      Code bytecode;
      bool ready = false, failed = false, hasData = false;
      size_t blockBytes = 0;
      {
        std::lock_guard<std::mutex> lock(parts.Mutex);
        Part& p = parts.List[i];
        hasData = p.HasData;
        failed = p.Failed;
        if (p.Kind == Kind_Texture1D || p.Kind == Kind_Texture2D || p.Kind == Kind_Texture3D)
          blockBytes = BlockBytesOf(p.Tex.Format);
        ready = !p.HasData || p.Read || p.Failed;
        if (ready && p.HasData && !p.Failed)
        {
          data.swap(p.Data);
          bytecode = p.Bytecode;
          job->Queued -= (std::min)(job->Queued, data.size());
        }
      }
      if (!ready)
      {
        Sleep(5);
        if (g_shutdown)
          return;
        continue;
      }
      if (hasData && !failed)
      {
        unsigned char const* bytes = bytecode ? bytecode->data() : data.data();
        size_t size = bytecode ? bytecode->size() : data.size();
        std::string hash = Sha256(bytes, size);
        bool written = !hash.empty();
        std::string path = PartPath(hash);
        if (written && Exists(path))
          ++job->Shared;
        else if (written)
        {
          std::string temp = path + ".tmp";
          HANDLE file = CreateFileA(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
          written = file != INVALID_HANDLE_VALUE;
          unsigned long long fileBytes = 8;
          if (written)
          {
            std::vector<unsigned char> raw = bytecode ? *bytecode : std::move(data);
            unsigned long long rawSize = raw.size();
            std::vector<unsigned char> packed;
            unsigned method = Method_Raw;
            PackPart(std::move(raw), bytecode ? 0 : blockBytes, packed, method);
            written = WriteAll(file, PartMagic, sizeof(PartMagic)) && WritePacked(file, method, rawSize, packed, &fileBytes);
            CloseHandle(file);
            written = written && MoveFileExA(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING);
            if (!written)
              DeleteFileA(temp.c_str());
          }
          if (written)
          {
            ++job->Written;
            job->NewBytes += fileBytes;
            g_PartsBytes += fileBytes;
          }
        }
        std::lock_guard<std::mutex> lock(parts.Mutex);
        Part& p = parts.List[i];
        p.Written = written;
        p.Failed = !written;
        p.Hash = written ? hash : "";
        p.Bytecode.reset();
        if (!written)
          ++job->Failed;
      }
      else if (failed)
        ++job->Failed;
      ++i;
      char text[300];
      sprintf_s(text, "Saving %s: %d of %d parts written (%d new, %d already saved by another recording), %.1f MB new",
        job->Folder.c_str(), (int)i, (int)total, job->Written.load(), job->Shared.load(), job->NewBytes / 1048576.0);
      SetStatus(text);
    }
    {
      std::lock_guard<std::mutex> lock(g_Mutex);
      g_Save.reset();
    }
    // The file: its summary, its parts' index, its sections
    std::string folder = std::string(RecordingsFolder) + "\\" + job->Folder;
    CreateDirectoryA(folder.c_str(), nullptr);
    std::string path = folder + "\\" + RecordingFile, temp = path + ".tmp";
    HANDLE file = CreateFileA(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    bool ok = file != INVALID_HANDLE_VALUE;
    unsigned long long fileBytes = sizeof(Magic);
    int missing = 0;
    if (ok)
    {
      replay_store::Out info, index;
      {
        std::lock_guard<std::mutex> lock(parts.Mutex);
        for (Part const& p : parts.List)
          missing += p.HasData && p.Hash.empty();
        info.Text(job->Request.Where);
        info.Text(job->When);
        info.Put(job->Request.Seconds);
        info.Text(job->Request.Summary);
        info.Put((unsigned)parts.List.size());
        info.Put((unsigned)(missing + parts.Unsaved));
        WriteIndex(index, parts.List);
      }
      ok = WriteAll(file, Magic, sizeof(Magic)) && WriteBlock(file, TagInfo, true, std::move(info.Data), false, &fileBytes) &&
        WriteBlock(file, TagParts, true, std::move(index.Data), true, &fileBytes);
      for (auto& section : job->Request.Sections)
        ok = ok && WriteBlock(file, section.first, true, std::move(section.second.Data), true, &fileBytes);
      CloseHandle(file);
      ok = ok && MoveFileExA(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING);
      if (!ok)
        DeleteFileA(temp.c_str());
    }
    job->Request.Sections.clear();
    {
      std::lock_guard<std::mutex> lock(parts.Mutex);
      parts.List.clear();
    }
    char text[400];
    if (ok)
    {
      sprintf_s(text, "Saved %s: %.1f MB, and %.1f MB of new parts (%d parts already saved by another recording; %d of its draws' "
        "parts couldn't be saved)", job->Folder.c_str(), fileBytes / 1048576.0, job->NewBytes / 1048576.0, job->Shared.load(),
        missing + job->Request.Parts->Unsaved);
      int const* by = job->Request.Parts->UnsavedByKind;
      util::log::Write("Replay recordings: %s. Not saved: %d vertex and %d pixel shaders (made before the tools' hooks), %d input "
        "layouts (likewise), %d geometry, hull or domain shaders, %d textures or buffers (too big, multisampled or of a format the "
        "store doesn't know), %d views of those, %d other objects; %d buffers or textures not read back from the graphics card",
        text, by[Kind_Vs], by[Kind_Ps], by[Kind_Layout], by[Kind_Gs] + by[Kind_Hs] + by[Kind_Ds],
        by[Kind_Buffer] + by[Kind_Texture1D] + by[Kind_Texture2D] + by[Kind_Texture3D], by[Kind_View], by[0], missing);
    }
    else
    {
      sprintf_s(text, "The recording couldn't be saved to %s (error %lu)", folder.c_str(), GetLastError());
      util::log::Warning("Replay recordings: %s", text);
    }
    SetStatus(text);
    std::lock_guard<std::mutex> lock(g_Mutex);
    g_ListStale = true;
  }

  // The store's thread: a recording's file and parts read
  void RunLoad(std::shared_ptr<LoadJob> job)
  {
    std::string path = std::string(RecordingsFolder) + "\\" + job->Folder + "\\" + RecordingFile;
    HANDLE file = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    char magic[8] = {};
    bool ok = file != INVALID_HANDLE_VALUE && ReadAll(file, magic, sizeof(magic)) && !memcmp(magic, Magic, sizeof(magic));
    std::vector<LoadEntry> entries;
    std::vector<std::pair<unsigned, std::vector<unsigned char>>> sections;
    while (ok)
    {
      unsigned tag = 0;
      DWORD got = 0;
      if (!ReadFile(file, &tag, sizeof(tag), &got, nullptr) || got == 0)
        break;   // the end
      std::vector<unsigned char> bytes;
      ok = got == sizeof(tag) && ReadBlock(file, bytes, MaxSectionBytes);
      if (!ok)
        break;
      if (tag == TagParts)
      {
        replay_store::In in(bytes.data(), bytes.size());
        ok = ReadIndex(in, entries);
      }
      else if (tag != TagInfo)
        sections.emplace_back(tag, std::move(bytes));
    }
    if (file != INVALID_HANDLE_VALUE)
      CloseHandle(file);
    if (!ok)
    {
      util::log::Warning("Replay recordings: %s couldn't be read", path.c_str());
      SetStatus("Not loaded: " + job->Folder + " couldn't be read");
      job->Failed = true;
      std::lock_guard<std::mutex> lock(g_Mutex);
      g_Load.reset();
      return;
    }
    {
      std::lock_guard<std::mutex> lock(job->Mutex);
      job->Entries = std::move(entries);
      job->Sections = std::move(sections);
      job->Parts->Objects.resize(job->Entries.size());
    }
    job->Indexed = true;
    size_t count = job->Parts->Objects.size();
    for (size_t i = 0; i < count && !g_shutdown; ++i)
    {
      std::string hash;
      {
        std::lock_guard<std::mutex> lock(job->Mutex);
        hash = job->Entries[i].Hash;
        if (hash.empty())
        {
          job->Entries[i].Ready = true;
          continue;
        }
      }
      // Not too far ahead of what's made
      for (;;)
      {
        {
          std::lock_guard<std::mutex> lock(job->Mutex);
          if (job->Ahead < MaxLoadAhead || job->Next >= i)
            break;
        }
        Sleep(5);
        if (g_shutdown)
          return;
      }
      std::vector<unsigned char> data;
      HANDLE part = CreateFileA(PartPath(hash).c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
        nullptr);
      char partMagic[8] = {};
      bool read = part != INVALID_HANDLE_VALUE && ReadAll(part, partMagic, sizeof(partMagic)) &&
        !memcmp(partMagic, PartMagic, sizeof(partMagic)) && ReadBlock(part, data, MaxPartBytes);
      if (part != INVALID_HANDLE_VALUE)
        CloseHandle(part);
      std::lock_guard<std::mutex> lock(job->Mutex);
      LoadEntry& e = job->Entries[i];
      e.Bad = !read;
      if (read)
      {
        job->Ahead += data.size();
        e.Data.swap(data);
      }
      e.Ready = true;
    }
    job->ReadDone = true;
  }

  // Render thread: one loaded part made (its bytes let go); null if it can't be
  ComPtr<IUnknown> Make(ID3D11Device* device, LoadEntry const& e, replay_store::LoadParts const& made)
  {
    ComPtr<IUnknown> out;
    HRESULT hr = E_FAIL;
    switch (e.Kind)
    {
    case Kind_Buffer:
    {
      D3D11_BUFFER_DESC d{};
      if (!DescOf(e.Desc, d) || e.Data.size() != d.ByteWidth)
        break;
      d.BindFlags &= ~(UINT)(D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_STREAM_OUTPUT | D3D11_BIND_RENDER_TARGET);
      d.Usage = d.BindFlags ? D3D11_USAGE_IMMUTABLE : D3D11_USAGE_DEFAULT;
      d.CPUAccessFlags = 0;
      d.MiscFlags &= (UINT)(D3D11_RESOURCE_MISC_BUFFER_STRUCTURED | D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS |
        D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS);
      D3D11_SUBRESOURCE_DATA init{ e.Data.data(), 0, 0 };
      ComPtr<ID3D11Buffer> buffer;
      hr = device->CreateBuffer(&d, &init, buffer.GetAddressOf());
      out = buffer;
      break;
    }
    case Kind_Texture1D: case Kind_Texture2D: case Kind_Texture3D:
    {
      TexShape t{};
      D3D11_TEXTURE1D_DESC d1{};
      D3D11_TEXTURE2D_DESC d2{};
      D3D11_TEXTURE3D_DESC d3{};
      if (e.Kind == Kind_Texture1D && DescOf(e.Desc, d1))
        t = { 1, d1.Width, 1, 1, d1.MipLevels, d1.ArraySize, d1.Format };
      else if (e.Kind == Kind_Texture2D && DescOf(e.Desc, d2))
        t = { 2, d2.Width, d2.Height, 1, d2.MipLevels, d2.ArraySize, d2.Format };
      else if (e.Kind == Kind_Texture3D && DescOf(e.Desc, d3))
        t = { 3, d3.Width, d3.Height, d3.Depth, d3.MipLevels, 1, d3.Format };
      else
        break;
      if (!t.Mips || !t.Layers || TexBytes(t) != e.Data.size())
        break;
      std::vector<D3D11_SUBRESOURCE_DATA> init;
      size_t at = 0;
      for (UINT layer = 0; layer < t.Layers; ++layer)
        for (UINT mip = 0; mip < t.Mips; ++mip)
        {
          size_t rowBytes = 0;
          UINT rows = 0;
          LevelRows(t.Format, t.Width >> mip, t.Height >> mip, rowBytes, rows);
          D3D11_SUBRESOURCE_DATA s{ e.Data.data() + at, (UINT)rowBytes, (UINT)(rowBytes * rows) };
          init.push_back(s);
          at += rowBytes * rows * (std::max)(t.Depth >> mip, 1u);
        }
      if (e.Kind == Kind_Texture1D)
      {
        d1.Usage = D3D11_USAGE_IMMUTABLE;
        d1.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        d1.CPUAccessFlags = 0;
        d1.MiscFlags = 0;
        ComPtr<ID3D11Texture1D> texture;
        hr = device->CreateTexture1D(&d1, init.data(), texture.GetAddressOf());
        out = texture;
      }
      else if (e.Kind == Kind_Texture2D)
      {
        d2.Usage = D3D11_USAGE_IMMUTABLE;
        d2.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        d2.CPUAccessFlags = 0;
        d2.MiscFlags &= (UINT)D3D11_RESOURCE_MISC_TEXTURECUBE;
        d2.SampleDesc.Count = 1;
        d2.SampleDesc.Quality = 0;
        ComPtr<ID3D11Texture2D> texture;
        hr = device->CreateTexture2D(&d2, init.data(), texture.GetAddressOf());
        out = texture;
      }
      else
      {
        d3.Usage = D3D11_USAGE_IMMUTABLE;
        d3.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        d3.CPUAccessFlags = 0;
        d3.MiscFlags = 0;
        ComPtr<ID3D11Texture3D> texture;
        hr = device->CreateTexture3D(&d3, init.data(), texture.GetAddressOf());
        out = texture;
      }
      break;
    }
    case Kind_View:
    {
      D3D11_SHADER_RESOURCE_VIEW_DESC d{};
      ComPtr<ID3D11Resource> resource = replay_store::PartAs<ID3D11Resource>(made, e.Ref);
      if (!DescOf(e.Desc, d) || !resource)
        break;
      ComPtr<ID3D11ShaderResourceView> view;
      hr = device->CreateShaderResourceView(resource.Get(), &d, view.GetAddressOf());
      out = view;
      break;
    }
    case Kind_Sampler:
    {
      D3D11_SAMPLER_DESC d{};
      ComPtr<ID3D11SamplerState> state;
      if (DescOf(e.Desc, d))
        hr = device->CreateSamplerState(&d, state.GetAddressOf());
      out = state;
      break;
    }
    case Kind_Blend:
    {
      D3D11_BLEND_DESC d{};
      ComPtr<ID3D11BlendState> state;
      if (DescOf(e.Desc, d))
        hr = device->CreateBlendState(&d, state.GetAddressOf());
      out = state;
      break;
    }
    case Kind_Depth:
    {
      D3D11_DEPTH_STENCIL_DESC d{};
      ComPtr<ID3D11DepthStencilState> state;
      if (DescOf(e.Desc, d))
        hr = device->CreateDepthStencilState(&d, state.GetAddressOf());
      out = state;
      break;
    }
    case Kind_Raster:
    {
      D3D11_RASTERIZER_DESC d{};
      ComPtr<ID3D11RasterizerState> state;
      if (DescOf(e.Desc, d))
        hr = device->CreateRasterizerState(&d, state.GetAddressOf());
      out = state;
      break;
    }
    case Kind_Vs:
    {
      ComPtr<ID3D11VertexShader> shader;
      if (!e.Data.empty())
        hr = device->CreateVertexShader(e.Data.data(), e.Data.size(), nullptr, shader.GetAddressOf());
      out = shader;
      break;
    }
    case Kind_Ps:
    {
      ComPtr<ID3D11PixelShader> shader;
      if (!e.Data.empty())
        hr = device->CreatePixelShader(e.Data.data(), e.Data.size(), nullptr, shader.GetAddressOf());
      out = shader;
      break;
    }
    case Kind_Layout:
    {
      std::vector<D3D11_INPUT_ELEMENT_DESC> elements;
      for (LayoutElement const& l : e.Elements)
      {
        elements.push_back(l.Desc);
        elements.back().SemanticName = l.Semantic.c_str();
      }
      ComPtr<ID3D11InputLayout> layout;
      if (!elements.empty() && !e.Data.empty())
        hr = device->CreateInputLayout(elements.data(), (UINT)elements.size(), e.Data.data(), e.Data.size(), layout.GetAddressOf());
      out = layout;
      break;
    }
    default:
      break;
    }
    if (FAILED(hr))
      out.Reset();
    return out;
  }

  // Render thread: a save's buffers and textures copied and read back
  void ReadBack(ID3D11Device* device, ID3D11DeviceContext* context, SaveJob& job)
  {
    replay_store::SaveParts& parts = *job.Request.Parts;
    std::lock_guard<std::mutex> lock(parts.Mutex);
    job.Total = (int)parts.List.size();
    // The first part not read yet is the one RunSave waits for (it writes them
    // in order): it's copied whatever is staged or queued. 2026-10-28 01:48: a
    // part left out for the budgets while later ones were read filled the
    // queue with parts waiting behind it, and the save waited forever (1532
    // parts written, no Recording.ctr)
    bool head = true;
    for (Part& p : parts.List)
    {
      if (!p.HasData || p.Read || p.Failed || !p.Resource)
        continue;
      bool first = head;
      head = false;
      if (!p.Staging)
      {
        // At most MaxStaging on the graphics card and MaxQueued waiting to be written
        if (!first && job.Staged + p.Bytes > MaxStaging && job.Staged)
          continue;
        if (!first && job.Queued + p.Bytes > MaxQueued && job.Queued)
          continue;
        HRESULT hr = E_FAIL;
        if (p.Kind == Kind_Buffer)
        {
          D3D11_BUFFER_DESC d{};
          DescOf(p.Desc, d);
          D3D11_BUFFER_DESC s{};
          s.ByteWidth = d.ByteWidth;
          s.Usage = D3D11_USAGE_STAGING;
          s.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
          ComPtr<ID3D11Buffer> staging;
          hr = device->CreateBuffer(&s, nullptr, staging.GetAddressOf());
          p.Staging = staging;
        }
        else if (p.Kind == Kind_Texture2D)
        {
          D3D11_TEXTURE2D_DESC d{};
          DescOf(p.Desc, d);
          d.Usage = D3D11_USAGE_STAGING;
          d.BindFlags = 0;
          d.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
          d.MiscFlags &= (UINT)D3D11_RESOURCE_MISC_TEXTURECUBE;
          ComPtr<ID3D11Texture2D> staging;
          hr = device->CreateTexture2D(&d, nullptr, staging.GetAddressOf());
          p.Staging = staging;
        }
        else if (p.Kind == Kind_Texture1D)
        {
          D3D11_TEXTURE1D_DESC d{};
          DescOf(p.Desc, d);
          d.Usage = D3D11_USAGE_STAGING;
          d.BindFlags = 0;
          d.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
          d.MiscFlags = 0;
          ComPtr<ID3D11Texture1D> staging;
          hr = device->CreateTexture1D(&d, nullptr, staging.GetAddressOf());
          p.Staging = staging;
        }
        else if (p.Kind == Kind_Texture3D)
        {
          D3D11_TEXTURE3D_DESC d{};
          DescOf(p.Desc, d);
          d.Usage = D3D11_USAGE_STAGING;
          d.BindFlags = 0;
          d.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
          d.MiscFlags = 0;
          ComPtr<ID3D11Texture3D> staging;
          hr = device->CreateTexture3D(&d, nullptr, staging.GetAddressOf());
          p.Staging = staging;
        }
        if (FAILED(hr) || !p.Staging)
        {
          p.Staging.Reset();
          p.Failed = true;
          p.Resource.Reset();
          continue;
        }
        context->CopyResource(p.Staging.Get(), p.Resource.Get());
        p.StagedAt = g_Frame;
        job.Staged += p.Bytes;
        continue;
      }
      if (g_Frame - p.StagedAt < ReadAfterFrames)
        continue;
      // Read: each level and layer, rows packed tight
      bool drawing = false, ok = true;
      try
      {
        p.Data.resize(p.Bytes);
      }
      catch (std::bad_alloc const&)
      {
        ok = false;
      }
      if (ok && p.Kind == Kind_Buffer)
      {
        D3D11_MAPPED_SUBRESOURCE m{};
        HRESULT hr = context->Map(p.Staging.Get(), 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &m);
        drawing = hr == DXGI_ERROR_WAS_STILL_DRAWING;
        ok = SUCCEEDED(hr) && m.pData;
        if (ok)
        {
          memcpy(p.Data.data(), m.pData, p.Bytes);
          context->Unmap(p.Staging.Get(), 0);
        }
      }
      else if (ok)
      {
        size_t at = 0;
        for (UINT layer = 0; layer < p.Tex.Layers && ok; ++layer)
          for (UINT mip = 0; mip < p.Tex.Mips && ok; ++mip)
          {
            UINT sub = D3D11CalcSubresource(mip, layer, p.Tex.Mips);
            D3D11_MAPPED_SUBRESOURCE m{};
            HRESULT hr = context->Map(p.Staging.Get(), sub, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &m);
            drawing = hr == DXGI_ERROR_WAS_STILL_DRAWING;
            ok = SUCCEEDED(hr) && m.pData;
            if (!ok)
              break;
            size_t rowBytes = 0;
            UINT rows = 0;
            LevelRows(p.Tex.Format, p.Tex.Width >> mip, p.Tex.Height >> mip, rowBytes, rows);
            UINT depth = (std::max)(p.Tex.Depth >> mip, 1u);
            for (UINT z = 0; z < depth; ++z)
              for (UINT r = 0; r < rows; ++r)
              {
                memcpy(p.Data.data() + at, (BYTE const*)m.pData + (size_t)z * m.DepthPitch + (size_t)r * m.RowPitch, rowBytes);
                at += rowBytes;
              }
            context->Unmap(p.Staging.Get(), sub);
          }
        ok = ok && at == p.Bytes;
      }
      if (!ok && drawing && g_Frame - p.StagedAt < GiveUpFrames)
        continue;   // the copy isn't done yet: next frame
      job.Staged -= (std::min)(job.Staged, p.Bytes);
      p.Staging.Reset();
      p.Resource.Reset();
      if (ok)
      {
        p.Read = true;
        job.Queued += p.Bytes;
        ++job.ReadCount;
      }
      else
      {
        p.Data.clear();
        p.Data.shrink_to_fit();
        p.Failed = true;
      }
    }
  }

  // Render thread: a load's parts made, CreateBudgetMs at most this frame
  void MakeLoaded(ID3D11Device* device, LoadJob& job)
  {
    if (!job.Indexed || job.Made)
      return;
    auto start = std::chrono::steady_clock::now();
    size_t count = job.Parts->Objects.size();
    while (job.Next < count)
    {
      LoadEntry entry;
      {
        std::lock_guard<std::mutex> lock(job.Mutex);
        LoadEntry& e = job.Entries[job.Next.load()];
        if (!e.Ready)
          break;
        entry.Kind = e.Kind;
        entry.Ref = e.Ref;
        entry.Desc = e.Desc;
        entry.Elements = e.Elements;
        entry.Data.swap(e.Data);
        entry.Bad = e.Bad;
        job.Ahead -= (std::min)(job.Ahead, entry.Data.size());
      }
      ComPtr<IUnknown> object;
      if (!entry.Bad)
        object = Make(device, entry, *job.Parts);
      if (!object)
        ++job.Missing;
      job.Parts->Objects[job.Next.load()] = object;
      ++job.Next;
      std::chrono::duration<double, std::milli> took = std::chrono::steady_clock::now() - start;
      if (took.count() > CreateBudgetMs)
        break;
    }
    if (job.Next >= count && job.ReadDone)
    {
      job.Made = true;
      char text[300];
      sprintf_s(text, "Loaded %s (%d parts, %d of them couldn't be made)", job.Folder.c_str(), (int)count, job.Missing);
      util::log::Write("Replay recordings: %s", text);
      std::lock_guard<std::mutex> lock(g_Mutex);
      g_Status = text;
    }
    else if (job.Next % 16 == 0)
    {
      char text[300];
      sprintf_s(text, "Loading %s: %d of %d parts made", job.Folder.c_str(), (int)job.Next, (int)count);
      std::lock_guard<std::mutex> lock(g_Mutex);
      g_Status = text;
    }
  }

  // The store's thread: a recording's folder deleted, then the parts no other recording uses
  void RunDelete(std::string folder)
  {
    std::string dir = std::string(RecordingsFolder) + "\\" + folder;
    DeleteFileA((dir + "\\" + RecordingFile).c_str());
    DeleteFileA((dir + "\\" + RecordingFile + ".tmp").c_str());
    bool removed = RemoveDirectoryA(dir.c_str()) != FALSE;
    // The hashes the others use
    std::set<std::string> used;
    bool allRead = true;
    WIN32_FIND_DATAA found{};
    HANDLE find = FindFirstFileA((std::string(RecordingsFolder) + "\\*").c_str(), &found);
    if (find != INVALID_HANDLE_VALUE)
    {
      do
      {
        if (!(found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || found.cFileName[0] == '.' || !_stricmp(found.cFileName, "Parts"))
          continue;
        std::string path = std::string(RecordingsFolder) + "\\" + found.cFileName + "\\" + RecordingFile;
        HANDLE file = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE)
          continue;   // no recording in it (a save going on writes .tmp first: its parts are kept by the tmp check below)
        char magic[8] = {};
        bool ok = ReadAll(file, magic, sizeof(magic)) && !memcmp(magic, Magic, sizeof(magic));
        bool indexed = false;
        while (ok && !indexed)
        {
          unsigned tag = 0;
          std::vector<unsigned char> bytes;
          ok = ReadAll(file, &tag, sizeof(tag)) && ReadBlock(file, bytes, MaxSectionBytes);
          if (ok && tag == TagParts)
          {
            std::vector<LoadEntry> entries;
            replay_store::In in(bytes.data(), bytes.size());
            ok = ReadIndex(in, entries);
            for (LoadEntry const& e : entries)
              if (!e.Hash.empty())
                used.insert(e.Hash);
            indexed = ok;
          }
        }
        allRead = allRead && indexed;
        CloseHandle(file);
      } while (FindNextFileA(find, &found));
      FindClose(find);
    }
    int deleted = 0;
    unsigned long long freed = 0;
    // Not while a save is writing (its parts aren't in a file's index yet), nor if a recording couldn't be read
    bool saving = false;
    {
      std::lock_guard<std::mutex> lock(g_Mutex);
      saving = g_Save != nullptr;
    }
    if (allRead && !saving)
    {
      find = FindFirstFileA((std::string(PartsFolder) + "\\*.part").c_str(), &found);
      if (find != INVALID_HANDLE_VALUE)
      {
        do
        {
          std::string name = found.cFileName;
          std::string hash = name.substr(0, name.size() - 5);
          if (used.count(hash))
            continue;
          std::string path = std::string(PartsFolder) + "\\" + name;
          unsigned long long size = SizeOf(path);
          if (DeleteFileA(path.c_str()))
          {
            ++deleted;
            freed += size;
          }
        } while (FindNextFileA(find, &found));
        FindClose(find);
      }
    }
    g_PartsBytes -= (std::min)(g_PartsBytes.load(), freed);
    char text[300];
    _snprintf_s(text, _TRUNCATE, "Deleted %s%s; %d parts no other recording uses deleted (%.1f MB)%s", folder.c_str(),
      removed ? "" : " (its folder had other files: left)", deleted, freed / 1048576.0,
      allRead && !saving ? "" : "; parts kept: a recording couldn't be read, or one is being saved");
    util::log::Write("Replay recordings: %s", text);
    SetStatus(text);
    std::lock_guard<std::mutex> lock(g_Mutex);
    g_ListStale = true;
  }

  // The store's thread: a recording's folder renamed (saves, loads and deletes
  // run on this thread too, so none is using it meanwhile)
  void RunRename(std::string folder, std::string name)
  {
    std::string clean = Clean(name);
    char text[400];
    if (clean.empty() || !_stricmp(clean.c_str(), "Parts"))
      sprintf_s(text, "Not renamed: \"%s\" can't be a recording's name", name.c_str());
    else if (clean == folder)
      text[0] = 0;
    else
    {
      std::string from = std::string(RecordingsFolder) + "\\" + folder, to = std::string(RecordingsFolder) + "\\" + clean;
      // Another recording's name (a name differing only in case is the same folder's to Windows)
      DWORD there = GetFileAttributesA(to.c_str());
      if (there != INVALID_FILE_ATTRIBUTES && _stricmp(clean.c_str(), folder.c_str()))
        sprintf_s(text, "Not renamed: there's already a recording named %s", clean.c_str());
      else if (MoveFileExA(from.c_str(), to.c_str(), 0))
      {
        sprintf_s(text, "Renamed %s to %s", folder.c_str(), clean.c_str());
        util::log::Write("Replay recordings: %s", text);
        std::lock_guard<std::mutex> lock(g_Mutex);
        for (auto& renamed : g_Renamed)
          if (renamed.second == folder)
            renamed.second = clean;
        g_Renamed[folder] = clean;
        g_ListStale = true;
      }
      else
        sprintf_s(text, "Not renamed: %s couldn't be renamed (error %lu)", folder.c_str(), GetLastError());
    }
    if (text[0])
      SetStatus(text);
  }

  void RunList()
  {
    std::vector<replay_store::Listed> list;
    unsigned long long partsBytes = 0;
    WIN32_FIND_DATAA found{};
    HANDLE find = FindFirstFileA((std::string(RecordingsFolder) + "\\*").c_str(), &found);
    if (find != INVALID_HANDLE_VALUE)
    {
      do
      {
        if (!(found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || found.cFileName[0] == '.' || !_stricmp(found.cFileName, "Parts"))
          continue;
        std::string path = std::string(RecordingsFolder) + "\\" + found.cFileName + "\\" + RecordingFile;
        HANDLE file = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE)
          continue;
        char magic[8] = {};
        unsigned tag = 0;
        std::vector<unsigned char> bytes;
        replay_store::Listed one;
        one.Folder = found.cFileName;
        one.Bytes = SizeOf(path);
        if (ReadAll(file, magic, sizeof(magic)) && !memcmp(magic, Magic, sizeof(magic)) && ReadAll(file, &tag, sizeof(tag)) &&
            tag == TagInfo && ReadBlock(file, bytes, 1 << 20))
        {
          replay_store::In in(bytes.data(), bytes.size());
          std::string where;
          if (in.Text(where) && in.Text(one.When) && in.Get(one.Seconds) && in.Text(one.Summary))
            list.push_back(one);
        }
        CloseHandle(file);
      } while (FindNextFileA(find, &found));
      FindClose(find);
    }
    find = FindFirstFileA((std::string(PartsFolder) + "\\*.part").c_str(), &found);
    if (find != INVALID_HANDLE_VALUE)
    {
      do
        partsBytes += ((unsigned long long)found.nFileSizeHigh << 32) | found.nFileSizeLow;
      while (FindNextFileA(find, &found));
      FindClose(find);
    }
    // Newest first, by when they were saved (a renamed one keeps its place)
    std::sort(list.begin(), list.end(), [](replay_store::Listed const& a, replay_store::Listed const& b) {
      return a.When != b.When ? a.When > b.When : a.Folder < b.Folder;
    });
    g_PartsBytes = partsBytes;
    std::lock_guard<std::mutex> lock(g_Mutex);
    g_List.swap(list);
  }
}

// ---- Out / In ----

void replay_store::Out::Frames(void const* data, size_t rows, size_t rowBytes, size_t elementBytes)
{
  if ((elementBytes != 1 && elementBytes != 2 && elementBytes != 4) || !rowBytes || rowBytes % elementBytes)
    elementBytes = 1;
  Put((unsigned long long)rows);
  Put((unsigned long long)rowBytes);
  Put((unsigned)elementBytes);
  size_t columns = rowBytes ? rowBytes / elementBytes : 0, count = rows * columns;
  size_t at = Data.size();
  Data.resize(at + rows * rowBytes);
  unsigned char* out = Data.data() + at;
  unsigned char const* in = (unsigned char const*)data;
  size_t i = 0;
  for (size_t c = 0; c < columns; ++c)
  {
    unsigned prev = 0;
    for (size_t r = 0; r < rows; ++r, ++i)
    {
      unsigned v = 0;
      memcpy(&v, in + r * rowBytes + c * elementBytes, elementBytes);
      unsigned d = v - prev;
      prev = v;
      for (size_t b = 0; b < elementBytes; ++b)
        out[b * count + i] = (unsigned char)(d >> (8 * b));
    }
  }
}

bool replay_store::In::Frames(std::vector<unsigned char>& out, size_t maxBytes)
{
  unsigned long long rows = 0, rowBytes = 0;
  unsigned elementBytes = 0;
  if (!Get(rows) || !Get(rowBytes) || !Get(elementBytes))
    return false;
  if ((elementBytes != 1 && elementBytes != 2 && elementBytes != 4) || (rowBytes && rowBytes % elementBytes) || rows > maxBytes ||
      rowBytes > maxBytes || (rowBytes && rows > maxBytes / rowBytes) || rows * rowBytes > m_Left)
    return m_Ok = false;
  size_t total = (size_t)(rows * rowBytes), columns = rowBytes ? (size_t)rowBytes / elementBytes : 0, count = (size_t)rows * columns;
  out.resize(total);
  size_t i = 0;
  for (size_t c = 0; c < columns; ++c)
  {
    unsigned prev = 0;
    for (size_t r = 0; r < rows; ++r, ++i)
    {
      unsigned d = 0;
      for (size_t b = 0; b < elementBytes; ++b)
        d |= (unsigned)m_P[b * count + i] << (8 * b);
      unsigned v = prev + d;
      prev = v;
      memcpy(out.data() + r * rowBytes + c * elementBytes, &v, elementBytes);
    }
  }
  m_P += total;
  m_Left -= total;
  return true;
}

// ---- Keys ----

long long replay_store::Keys::Of(__int64 address)
{
  if (!address)
    return 0;
  auto it = m_Map.find(address);
  if (it != m_Map.end())
    return it->second;
  long long key = (long long)m_Map.size() + 1;
  m_Map.emplace(address, key);
  return key;
}

__int64 replay_store::AddressOf(long long key)
{
  if (key <= 0 || key >= MaxKeys)
    return 0;
  // Zeroed, with room past the last key's for reads at an offset
  std::call_once(g_KeyOnce, [] {
    g_KeyMemory = (BYTE*)VirtualAlloc(nullptr, KeySpacing * (MaxKeys + 16), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
  });
  return g_KeyMemory ? (__int64)(g_KeyMemory + key * KeySpacing) : 0;
}

// ---- Parts ----

std::shared_ptr<replay_store::SaveParts> replay_store::NewSaveParts()
{
  return std::make_shared<SaveParts>();
}

int replay_store::AddPart(SaveParts& parts, IUnknown* object)
{
  std::lock_guard<std::mutex> lock(parts.Mutex);
  try
  {
    return AddLocked(parts, object);
  }
  catch (std::bad_alloc const&)
  {
    return -2;
  }
}

IUnknown* replay_store::PartObject(LoadParts const& parts, int id)
{
  return id >= 0 && (size_t)id < parts.Objects.size() ? parts.Objects[id].Get() : nullptr;
}

// ---- Saving and loading ----

void replay_store::StartSave(SaveRequest&& request)
{
  auto job = std::make_shared<SaveJob>();
  SYSTEMTIME now{};
  GetLocalTime(&now);
  char when[64];
  sprintf_s(when, "%04d-%02d-%02d %02d-%02d-%02d", now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond);
  job->When = when;
  job->Folder = Clean(std::string(when) + " " + (request.Where.empty() ? "Recording" : request.Where));
  job->Request = std::move(request);
  if (!job->Request.Parts)
    job->Request.Parts = NewSaveParts();
  SetStatus("Saving " + job->Folder + ": waiting for the graphics card's copies");
  util::log::Write("Replay recordings: saving the recording (%.1f s) to Cinematic Tools\\Recordings\\%s", job->Request.Seconds,
    job->Folder.c_str());
  Queue([job] { RunSave(job); });
}

bool replay_store::StartLoad(std::string const& folder)
{
  {
    std::lock_guard<std::mutex> lock(g_Mutex);
    if (g_Load || g_LoadedJob)
      return false;
    auto job = std::make_shared<LoadJob>();
    job->Folder = folder;
    g_Load = job;
    g_Status = "Loading " + folder;
  }
  util::log::Write("Replay recordings: loading Cinematic Tools\\Recordings\\%s", folder.c_str());
  std::shared_ptr<LoadJob> job;
  {
    std::lock_guard<std::mutex> lock(g_Mutex);
    job = g_Load;
  }
  Queue([job] { RunLoad(job); });
  return true;
}

bool replay_store::TakeLoaded(Loaded& out)
{
  std::shared_ptr<LoadJob> job;
  {
    std::lock_guard<std::mutex> lock(g_Mutex);
    job.swap(g_LoadedJob);
  }
  if (!job)
    return false;
  out.Folder = job->Folder;
  out.Sections = std::move(job->Sections);
  out.Parts = job->Parts;
  out.PartsMissing = job->Missing;
  return true;
}

bool replay_store::Loading()
{
  std::lock_guard<std::mutex> lock(g_Mutex);
  return g_Load != nullptr || g_LoadedJob != nullptr;
}

void replay_store::List(std::vector<Listed>& out)
{
  bool stale = false;
  {
    std::lock_guard<std::mutex> lock(g_Mutex);
    stale = g_ListStale;
    g_ListStale = false;
    out = g_List;
  }
  if (stale)
    Queue(RunList);
}

void replay_store::Refresh()
{
  std::lock_guard<std::mutex> lock(g_Mutex);
  g_ListStale = true;
}

void replay_store::Delete(std::string const& folder)
{
  if (folder.empty() || folder.find_first_of("\\/") != std::string::npos || folder[0] == '.' || !_stricmp(folder.c_str(), "Parts"))
    return;
  Queue([folder] { RunDelete(folder); });
}

void replay_store::Rename(std::string const& folder, std::string const& name)
{
  if (folder.empty() || folder.find_first_of("\\/") != std::string::npos || folder[0] == '.' || !_stricmp(folder.c_str(), "Parts"))
    return;
  Queue([folder, name] { RunRename(folder, name); });
}

std::string replay_store::RenamedTo(std::string const& folder)
{
  std::lock_guard<std::mutex> lock(g_Mutex);
  auto it = g_Renamed.find(folder);
  return it != g_Renamed.end() ? it->second : "";
}

const char* replay_store::Status(char* buffer, size_t size)
{
  std::lock_guard<std::mutex> lock(g_Mutex);
  strncpy_s(buffer, size, g_Status.c_str(), _TRUNCATE);
  return buffer;
}

unsigned long long replay_store::PartsBytes()
{
  return g_PartsBytes.load();
}

// ---- Hooks ----

void replay_store::NoteShader(void const* object, void const* code, size_t size)
{
  if (!object || !code || !size || g_shutdown)
    return;
  try
  {
    std::lock_guard<std::mutex> lock(g_CodeMutex);
    Code kept = SharedCode(code, size);
    if (kept)
      g_Code[(__int64)object] = kept;
    else
      g_Code.erase((__int64)object);
  }
  catch (std::bad_alloc const&)
  {
  }
}

void replay_store::Present(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
  if (!pDevice || !pContext || g_shutdown)
    return;
  if (!g_Hooked)
  {
    g_Hooked = true;
    void** table = *(void***)pDevice;
    if (!util::hooks::CreateGameHook("ReplayStoreCreateInputLayout", (__int64)table[CreateInputLayoutSlot], (PVOID)hCreateInputLayout,
        (LPVOID*)&oCreateInputLayout))
      util::log::Warning("Replay recordings: the device's CreateInputLayout couldn't be hooked: kept draws can't be saved");
  }
  ++g_Frame;
  std::shared_ptr<SaveJob> save;
  std::shared_ptr<LoadJob> load;
  {
    std::lock_guard<std::mutex> lock(g_Mutex);
    save = g_Save;
    load = g_Load;
  }
  if (save && save->Started)
    ReadBack(pDevice, pContext, *save);
  if (load)
  {
    MakeLoaded(pDevice, *load);
    if (load->Made || load->Failed)
    {
      std::lock_guard<std::mutex> lock(g_Mutex);
      if (load->Made)
        g_LoadedJob = load;
      g_Load.reset();
    }
  }
}
