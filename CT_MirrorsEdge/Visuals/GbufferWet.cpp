#include "GbufferWet.h"
#include "GameFrame.h"
#include "GbufferWetInfoPs.h"
#include "GbufferWetPs.h"
#include "GreenPlane.h"
#include "WetLook.h"
#include "WetLookNodesPs.h"
#include "WetLookVs.h"
#include "../Util/Perf.h"
#include "../Util/Util.h"

#include <atomic>
#include <cmath>
#include <cstdio>
#include <d3d11_1.h>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

// The wet look's wet ground in the game's own G-buffer (GbufferWet.h). Where
// the frame's first lit pass begins (the replay's render target hook, through
// gameframe::OnPass), with the wet look's constants of the last frame drawn
// and this frame's camera:
// 1. the copies of the game's targets 0 and 1 (its normals and smoothness,
//    its base colour and the normal's basis) and of its view constants that
//    gameframe makes there just before (GameFrame.h) taken;
// 2. one full-screen triangle (WetLook.hlsl's NodesPs) works out the
//    ground's nodes round the camera into their own small target (where a
//    puddle can lie, at fixed points of the world; the wet look at Present
//    reads them too, NodesThisFrame); a second (GbufferWet.hlsl's
//    GbufferWetInfoPs) where it's open and what's kept dry at each point into
//    the tools' own info target (the wet look at Present reads it too,
//    InfoThisFrame); a third (GbufferWetPs) reads those, the copies, the
//    game's depth and the wet look's lists, and writes the wet ground's
//    smoothness, facing and darker colour into those two targets (pixels it
//    leaves dry aren't written);
// 3. every state it changed put back as it was (the constant buffers with
//    their 11.1 windows); the game's own call that began its lit pass then
//    binds its targets.
// Not drawn (the wet look at Present draws the ground as before): while the
// menu's tick is off, the wet look isn't on or wasn't drawn the frame before
// (wetlook::TakeGbufferPass), snow lies, the performance test leaves the
// passes out, no G-buffer pass or copy of it was seen in the frame, this
// frame's camera isn't known yet, or the scene's depth was cleared again
// before it (what's in it then isn't the scene's).
// What the game's own render target hook sees of the pass's own target
// changes: two targets and no depth, a pass of none of its kinds, the same as
// the "other" pass it was drawn in (so nothing of the replay's or the snow's
// is set off by it).
namespace
{
  // ReplayGameDraws' Pass
  const int PassNone = -1, PassMain = 0, PassDecal = 1, PassLit = 2;
  // The G-buffer's targets written: its normals (target 0) and its colour (1)
  const int Targets = 2;
  // The slots the pass binds and puts back (t10 the info, WetLook.hlsl's
  // g_WetInfo; t11 and b4 the ground's nodes, its g_Nodes and g_NodeGrid)
  const UINT CbSlots = 5, ViewSlots = 12, SamplerSlots = 2;
  const UINT InfoSlot = 10, NodeSlot = 11, NodeCbSlot = 4;
  // The ground's nodes (NodesPs, GroundGoesOn): this many a side, this far
  // apart (m), round the camera (64 m each way)
  const int NodeSide = 256;
  const float NodeSpacing = 0.5f;
  // Its GPU time in the log every LogMs (diagnostic)
  const ULONGLONG LogMs = 10000;
  const int TimeSlots = 4;

  std::atomic<bool> g_On{ true };
  // Its shaders and states (made on a worker thread, as the wet look's, see
  // Make): 0 not yet, 1 being made, 2 made, -1 failed
  std::atomic<int> g_MakeState{ 0 };
  ComPtr<ID3D11VertexShader> g_Vs;
  ComPtr<ID3D11PixelShader> g_Ps;
  ComPtr<ID3D11PixelShader> g_InfoPs;
  ComPtr<ID3D11BlendState> g_Blend;
  ComPtr<ID3D11DepthStencilState> g_Depth;
  ComPtr<ID3D11RasterizerState> g_Rasterizer;
  ComPtr<ID3D11Buffer> g_Now;   // b3: where this frame's camera is
  // The ground's nodes: their shader, their target (R32G32: the surface's
  // height, how broad it is), their constants (b4: the lattice, the camera
  // they're relative to); whether drawn this frame (render thread)
  ComPtr<ID3D11PixelShader> g_NodesPs;
  ComPtr<ID3D11Texture2D> g_Nodes;
  ComPtr<ID3D11RenderTargetView> g_NodesTarget;
  ComPtr<ID3D11ShaderResourceView> g_NodesView;
  ComPtr<ID3D11Buffer> g_NodeConstants;
  bool g_NodesDrawn = false;

  // Render thread: this frame's G-buffer pass, as last seen
  ComPtr<ID3D11RenderTargetView> g_TargetViews[Targets];
  ComPtr<ID3D11RenderTargetView> g_MaterialView;   // its target 2, only for the diagnostic dump
  ComPtr<ID3D11Texture2D> g_SceneDepth;
  D3D11_VIEWPORT g_Viewport{};
  bool g_SawGbuffer = false;    // a G-buffer pass ended this frame
  bool g_Tried = false;         // the frame's first lit pass after it seen (drawn or not: never later, see OnPass)
  bool g_Drawn = false;         // the pass drawn this frame
  bool g_InfoWhole = false;     // and its info covers the depth's texels, as the wet look at Present reads it (InfoThisFrame)
  // The info target (GbufferWetInfoPs), the depth's size
  ComPtr<ID3D11Texture2D> g_Info;
  ComPtr<ID3D11RenderTargetView> g_InfoTarget;
  ComPtr<ID3D11ShaderResourceView> g_InfoView;
  UINT g_InfoWidth = 0, g_InfoHeight = 0;
  ComPtr<ID3D11DeviceContext1> g_Context1;
  bool g_Context1Asked = false;
  // A view of the game's depth targets the G-buffer pass draws with (the
  // game may take turns with a few): the last few made
  const int DepthViews = 4;
  struct DepthView { ID3D11Texture2D* Texture; ComPtr<ID3D11ShaderResourceView> View; };
  DepthView g_DepthViews[DepthViews];
  int g_DepthViewNext = 0;

  // The GPU's time for the pass, read back a few frames later: the ground's
  // nodes (Begin - Nodes), its info part (Nodes - Middle) and the wet
  // ground's (Middle - End); logged every LogMs
  struct TimeSlot { ComPtr<ID3D11Query> Disjoint, Begin, Nodes, Middle, End; bool Pending; };
  TimeSlot g_Times[TimeSlots];
  int g_TimeNext = 0;
  double g_TimeSum = 0.0, g_TimeMost = 0.0, g_TimeInfoSum = 0.0, g_TimeNodesSum = 0.0;
  int g_TimeCount = 0, g_FramesDrawn = 0;
  ULONGLONG g_TimeLoggedAt = 0;

  // The log: why it wasn't drawn, each reason once until it's drawn again
  // (at most MostSkipLines of them)
  const int MostSkipLines = 40;
  char const* g_LastSkip = nullptr;
  int g_SkipLines = 0;
  bool g_LoggedDrawn = false;

  // The diagnostic dump (RequestDump; 2026-11-03, the user: "It's the same"
  // after B67B44C4FAE7's fix for the patches and lines on a roof box's top,
  // which was worked out from a screenshot alone): the next frame drawn, the
  // game's G-buffer as it was before the pass (gameframe's copies of targets 0
  // and 1, its target 2, its depth, its view constants) and as the pass left
  // it (targets 0 and 1, the info), copied on the graphics card into staging
  // copies, read back DumpAfterFrames later and written as they are (each
  // row's texels one after another, no padding) into Cinematic Tools\
  // GbufferDump\, with a text file saying what each one is. Nothing of the
  // game's is changed.
  const int DumpAfterFrames = 3;
  std::atomic<bool> g_DumpAsked{ false };
  struct DumpItem
  {
    std::string Name;
    ComPtr<ID3D11Resource> Staging;
    bool Texture;
    UINT Width, Height, TexelBytes, Bytes;   // a texture's size and texel; a buffer's bytes
    DXGI_FORMAT Format;
  };
  std::vector<DumpItem> g_Dump;   // render thread: staged, waiting to be read back
  int g_DumpFrames = 0;
  char g_DumpStamp[32] = "";
  char g_DumpNote[1024] = "";

  UINT TexelBytes(DXGI_FORMAT format)
  {
    switch (format)
    {
    case DXGI_FORMAT_R32G8X24_TYPELESS: case DXGI_FORMAT_D32_FLOAT_S8X24_UINT: case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
    case DXGI_FORMAT_R32G32B32A32_FLOAT:
      return 16;
    case DXGI_FORMAT_R16G16B16A16_FLOAT: case DXGI_FORMAT_R16G16B16A16_TYPELESS: case DXGI_FORMAT_R32G32_FLOAT:
      return 8;
    case DXGI_FORMAT_R10G10B10A2_TYPELESS: case DXGI_FORMAT_R10G10B10A2_UNORM: case DXGI_FORMAT_R8G8B8A8_TYPELESS:
    case DXGI_FORMAT_R8G8B8A8_UNORM: case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: case DXGI_FORMAT_R24G8_TYPELESS:
    case DXGI_FORMAT_D24_UNORM_S8_UINT: case DXGI_FORMAT_R32_TYPELESS: case DXGI_FORMAT_R32_FLOAT: case DXGI_FORMAT_D32_FLOAT:
    case DXGI_FORMAT_R11G11B10_FLOAT: case DXGI_FORMAT_B8G8R8A8_UNORM: case DXGI_FORMAT_B8G8R8A8_TYPELESS:
      return 4;
    default:
      return 0;
    }
  }

  // A staging copy of a texture or buffer of the game's (or the tools'), made
  // now on the graphics card; skipped (logged in the text file) if it can't be
  void StageForDump(ID3D11Device* device, ID3D11DeviceContext* pContext, char const* name, ID3D11Resource* source)
  {
    if (!source)
      return;
    DumpItem item{};
    item.Name = name;
    D3D11_RESOURCE_DIMENSION kind = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    source->GetType(&kind);
    HRESULT hr = E_FAIL;
    if (kind == D3D11_RESOURCE_DIMENSION_TEXTURE2D)
    {
      ComPtr<ID3D11Texture2D> texture;
      if (FAILED(source->QueryInterface(__uuidof(ID3D11Texture2D), (void**)texture.GetAddressOf())))
        return;
      D3D11_TEXTURE2D_DESC d{};
      texture->GetDesc(&d);
      item.Texture = true;
      item.Width = d.Width;
      item.Height = d.Height;
      item.Format = d.Format;
      item.TexelBytes = TexelBytes(d.Format);
      if (!item.TexelBytes || d.SampleDesc.Count != 1 || d.ArraySize != 1)
        return;
      d.MipLevels = 1;
      d.Usage = D3D11_USAGE_STAGING;
      d.BindFlags = 0;
      d.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
      d.MiscFlags = 0;
      ComPtr<ID3D11Texture2D> staging;
      hr = device->CreateTexture2D(&d, nullptr, staging.GetAddressOf());
      if (SUCCEEDED(hr))
      {
        pContext->CopySubresourceRegion(staging.Get(), 0, 0, 0, 0, texture.Get(), 0, nullptr);
        item.Staging = staging;
      }
    }
    else if (kind == D3D11_RESOURCE_DIMENSION_BUFFER)
    {
      ComPtr<ID3D11Buffer> buffer;
      if (FAILED(source->QueryInterface(__uuidof(ID3D11Buffer), (void**)buffer.GetAddressOf())))
        return;
      D3D11_BUFFER_DESC d{};
      buffer->GetDesc(&d);
      item.Texture = false;
      item.Bytes = d.ByteWidth;
      D3D11_BUFFER_DESC s{};
      s.ByteWidth = d.ByteWidth;
      s.Usage = D3D11_USAGE_STAGING;
      s.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
      ComPtr<ID3D11Buffer> staging;
      hr = device->CreateBuffer(&s, nullptr, staging.GetAddressOf());
      if (SUCCEEDED(hr))
      {
        pContext->CopyResource(staging.Get(), buffer.Get());
        item.Staging = staging;
      }
    }
    if (item.Staging)
      g_Dump.push_back(item);
  }

  ComPtr<ID3D11Resource> ResourceOf(ID3D11View* view)
  {
    ComPtr<ID3D11Resource> resource;
    if (view)
      view->GetResource(resource.GetAddressOf());
    return resource;
  }

  // Render thread, DumpAfterFrames after it was staged: read back, then
  // written on a worker thread (the files are some 250 MB at 4K)
  void WriteDump(ID3D11DeviceContext* pContext)
  {
    struct Out { std::string Path; std::vector<unsigned char> Bytes; };
    auto outs = std::make_shared<std::vector<Out>>();
    std::string text = std::string("Wet surfaces in the game's lighting: G-buffer dump ") + g_DumpStamp + "\n" + g_DumpNote +
      "name, kind, width, height, DXGI format, bytes a texel (rows tight, no padding) or bytes\n";
    CreateDirectoryA(".\\Cinematic Tools\\GbufferDump", nullptr);
    for (DumpItem const& item : g_Dump)
    {
      D3D11_MAPPED_SUBRESOURCE mapped{};
      if (FAILED(pContext->Map(item.Staging.Get(), 0, D3D11_MAP_READ, 0, &mapped)))
      {
        text += item.Name + ", not read back\n";
        continue;
      }
      Out out;
      out.Path = std::string(".\\Cinematic Tools\\GbufferDump\\") + g_DumpStamp + "_" + item.Name + ".raw";
      char line[256];
      if (item.Texture)
      {
        size_t row = (size_t)item.Width * item.TexelBytes;
        out.Bytes.resize(row * item.Height);
        for (UINT y = 0; y < item.Height; ++y)
          memcpy(out.Bytes.data() + row * y, (unsigned char const*)mapped.pData + (size_t)mapped.RowPitch * y, row);
        sprintf_s(line, "%s, texture, %u, %u, %d, %u\n", item.Name.c_str(), item.Width, item.Height, (int)item.Format,
          item.TexelBytes);
      }
      else
      {
        out.Bytes.assign((unsigned char const*)mapped.pData, (unsigned char const*)mapped.pData + item.Bytes);
        sprintf_s(line, "%s, buffer, %u\n", item.Name.c_str(), item.Bytes);
      }
      pContext->Unmap(item.Staging.Get(), 0);
      text += line;
      outs->push_back(std::move(out));
    }
    // (what the squares' and the constants' parts hold, for reading them)
    char shelterNote[512];
    sprintf_s(shelterNote, "shelter: the weather's squares (t3), a row of squares along x, rows along z; per square x 1 looked at "
      "(0 not yet), y the first surface under where it was looked from (-1e9 none), z the first roof over it (1e9 none), w "
      "where from (world y)\nwetparams: the wet look's constants (b0), g_ShelterGrid at byte %d: xy the squares' first "
      "corner (world x, z), z a square's size (m), w how many a side (0 none)\n", wetlook::ShelterGridOffset());
    text += shelterNote;
    Out info;
    info.Path = std::string(".\\Cinematic Tools\\GbufferDump\\") + g_DumpStamp + "_info.txt";
    info.Bytes.assign(text.begin(), text.end());
    outs->push_back(std::move(info));
    g_Dump.clear();
    std::string stamp = g_DumpStamp;
    std::thread([outs, stamp]()
    {
      size_t total = 0;
      bool ok = true;
      for (Out const& out : *outs)
      {
        FILE* f = nullptr;
        if (fopen_s(&f, out.Path.c_str(), "wb") != 0 || !f)
        {
          ok = false;
          continue;
        }
        ok = fwrite(out.Bytes.data(), 1, out.Bytes.size(), f) == out.Bytes.size() && ok;
        fclose(f);
        total += out.Bytes.size();
      }
      util::log::Write("Wet surfaces in the game's lighting (diagnostic): the G-buffer dump written to Cinematic Tools\\GbufferDump\\"
        "%s_*.raw (%zu files, %.0f MB)%s", stamp.c_str(), outs->size(), total / 1048576.0, ok ? "" : "; some couldn't be written");
    }).detach();
  }

  void NoteSkip(char const* why)
  {
    if (g_LastSkip == why || g_SkipLines >= MostSkipLines)
      return;
    g_LastSkip = why;
    ++g_SkipLines;
    util::log::Write("Wet surfaces in the game's lighting: not drawn this frame (%s); the wet look draws the ground at Present "
      "as before", why);
  }

  // What a depth texture's format is read as (GreenPlane.cpp's the same)
  DXGI_FORMAT DepthViewFormat(DXGI_FORMAT format)
  {
    switch (format)
    {
    case DXGI_FORMAT_R24G8_TYPELESS: case DXGI_FORMAT_D24_UNORM_S8_UINT: case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
      return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    case DXGI_FORMAT_R32G8X24_TYPELESS: case DXGI_FORMAT_D32_FLOAT_S8X24_UINT: case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
      return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
    case DXGI_FORMAT_R32_TYPELESS: case DXGI_FORMAT_D32_FLOAT: case DXGI_FORMAT_R32_FLOAT:
      return DXGI_FORMAT_R32_FLOAT;
    case DXGI_FORMAT_R16_TYPELESS: case DXGI_FORMAT_D16_UNORM: case DXGI_FORMAT_R16_UNORM:
      return DXGI_FORMAT_R16_UNORM;
    default:
      return DXGI_FORMAT_UNKNOWN;
    }
  }

  bool Make(ID3D11Device* device)
  {
    D3D11_BLEND_DESC blend{};
    for (int i = 0; i < Targets; ++i)
    {
      blend.RenderTarget[i].BlendEnable = FALSE;
      blend.RenderTarget[i].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    }
    D3D11_DEPTH_STENCIL_DESC depth{};
    depth.DepthEnable = FALSE;
    depth.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    depth.DepthFunc = D3D11_COMPARISON_ALWAYS;
    depth.StencilEnable = FALSE;
    D3D11_RASTERIZER_DESC rasterizer{};
    rasterizer.FillMode = D3D11_FILL_SOLID;
    rasterizer.CullMode = D3D11_CULL_NONE;
    rasterizer.DepthClipEnable = TRUE;
    D3D11_BUFFER_DESC now{};
    now.ByteWidth = 16;
    now.Usage = D3D11_USAGE_DYNAMIC;
    now.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    now.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    D3D11_BUFFER_DESC nodeConstants = now;
    nodeConstants.ByteWidth = 32;
    D3D11_TEXTURE2D_DESC nodes{};
    nodes.Width = nodes.Height = NodeSide;
    nodes.MipLevels = nodes.ArraySize = 1;
    nodes.Format = DXGI_FORMAT_R32G32_FLOAT;
    nodes.SampleDesc.Count = 1;
    nodes.Usage = D3D11_USAGE_DEFAULT;
    nodes.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    HRESULT hr = device->CreateVertexShader(g_WetLookVs, sizeof(g_WetLookVs), nullptr, g_Vs.GetAddressOf());
    if (SUCCEEDED(hr)) hr = device->CreatePixelShader(g_GbufferWetPs, sizeof(g_GbufferWetPs), nullptr, g_Ps.GetAddressOf());
    if (SUCCEEDED(hr))
      hr = device->CreatePixelShader(g_GbufferWetInfoPs, sizeof(g_GbufferWetInfoPs), nullptr, g_InfoPs.GetAddressOf());
    if (SUCCEEDED(hr)) hr = device->CreatePixelShader(g_WetLookNodesPs, sizeof(g_WetLookNodesPs), nullptr, g_NodesPs.GetAddressOf());
    if (SUCCEEDED(hr)) hr = device->CreateBlendState(&blend, g_Blend.GetAddressOf());
    if (SUCCEEDED(hr)) hr = device->CreateDepthStencilState(&depth, g_Depth.GetAddressOf());
    if (SUCCEEDED(hr)) hr = device->CreateRasterizerState(&rasterizer, g_Rasterizer.GetAddressOf());
    if (SUCCEEDED(hr)) hr = device->CreateBuffer(&now, nullptr, g_Now.GetAddressOf());
    if (SUCCEEDED(hr)) hr = device->CreateBuffer(&nodeConstants, nullptr, g_NodeConstants.GetAddressOf());
    if (SUCCEEDED(hr)) hr = device->CreateTexture2D(&nodes, nullptr, g_Nodes.GetAddressOf());
    if (SUCCEEDED(hr)) hr = device->CreateRenderTargetView(g_Nodes.Get(), nullptr, g_NodesTarget.GetAddressOf());
    if (SUCCEEDED(hr)) hr = device->CreateShaderResourceView(g_Nodes.Get(), nullptr, g_NodesView.GetAddressOf());
    if (FAILED(hr))
      util::log::Error("Wet surfaces in the game's lighting: its shader or states couldn't be made (HRESULT 0x%X): the wet look "
        "draws the ground at Present as before", hr);
    return SUCCEEDED(hr);
  }

  // Made on a worker thread the first time it's wanted (the wet look's
  // StartMaking: the driver's work on a big shader held up a frame), in that
  // frame if the device is single-threaded
  void StartMaking(ID3D11DeviceContext* pContext)
  {
    int expected = 0;
    if (g_MakeState.load(std::memory_order_acquire) != 0 || !g_MakeState.compare_exchange_strong(expected, 1))
      return;
    ComPtr<ID3D11Device> device;
    pContext->GetDevice(device.GetAddressOf());
    if (!device)
    {
      g_MakeState.store(0, std::memory_order_release);
      return;
    }
    bool worker = !(device->GetCreationFlags() & D3D11_CREATE_DEVICE_SINGLETHREADED);
    auto make = [device, worker]()
    {
      double start = wetlook::TimeNow();
      bool made = Make(device.Get());
      g_MakeState.store(made ? 2 : -1, std::memory_order_release);
      util::log::Write("Wet surfaces in the game's lighting: its shaders and states made in %.0f ms, %s", wetlook::TimeNow() - start,
        worker ? "on a worker thread" : "in this frame (the game's device is single-threaded)");
    };
    if (worker)
      std::thread(make).detach();
    else
      make();
  }

  // A view of the scene's depth (the G-buffer pass's), made once for each
  // texture (null: it can't be read; the game makes its depth targets
  // readable, its lights read them)
  ID3D11ShaderResourceView* DepthViewOf(ID3D11Device* device, ID3D11Texture2D* texture)
  {
    for (DepthView const& seen : g_DepthViews)
      if (seen.Texture == texture && seen.View)
        return seen.View.Get();
    D3D11_TEXTURE2D_DESC d{};
    texture->GetDesc(&d);
    DXGI_FORMAT format = DepthViewFormat(d.Format);
    if (format == DXGI_FORMAT_UNKNOWN || !(d.BindFlags & D3D11_BIND_SHADER_RESOURCE) || d.SampleDesc.Count != 1 ||
        d.ArraySize != 1)
    {
      static bool s_told = false;
      if (!s_told)
      {
        s_told = true;
        util::log::Warning("Wet surfaces in the game's lighting: the game's depth (format %d, bind flags 0x%X, %u samples, %u "
          "slices) can't be read: not drawn", (int)d.Format, d.BindFlags, d.SampleDesc.Count, d.ArraySize);
      }
      return nullptr;
    }
    D3D11_SHADER_RESOURCE_VIEW_DESC view{};
    view.Format = format;
    view.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    view.Texture2D.MipLevels = 1;
    DepthView& slot = g_DepthViews[g_DepthViewNext];
    slot.View.Reset();
    slot.Texture = nullptr;
    if (FAILED(device->CreateShaderResourceView(texture, &view, slot.View.GetAddressOf())) || !slot.View)
      return nullptr;
    // (the texture by its address only: a view holds it alive)
    slot.Texture = texture;
    g_DepthViewNext = (g_DepthViewNext + 1) % DepthViews;
    return slot.View.Get();
  }

  // The info target, the depth texture's size (made again when that changes)
  bool MakeInfo(ID3D11Device* device, UINT width, UINT height)
  {
    if (g_Info && g_InfoWidth == width && g_InfoHeight == height)
      return true;
    g_InfoView.Reset();
    g_InfoTarget.Reset();
    g_Info.Reset();
    g_InfoWidth = g_InfoHeight = 0;
    D3D11_TEXTURE2D_DESC d{};
    d.Width = width;
    d.Height = height;
    d.MipLevels = d.ArraySize = 1;
    d.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    d.SampleDesc.Count = 1;
    d.Usage = D3D11_USAGE_DEFAULT;
    d.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    HRESULT hr = device->CreateTexture2D(&d, nullptr, g_Info.GetAddressOf());
    if (SUCCEEDED(hr))
      hr = device->CreateRenderTargetView(g_Info.Get(), nullptr, g_InfoTarget.GetAddressOf());
    if (SUCCEEDED(hr))
      hr = device->CreateShaderResourceView(g_Info.Get(), nullptr, g_InfoView.GetAddressOf());
    if (FAILED(hr))
    {
      g_InfoView.Reset();
      g_InfoTarget.Reset();
      g_Info.Reset();
      return false;
    }
    g_InfoWidth = width;
    g_InfoHeight = height;
    return true;
  }

  // The context's state the pass changes, put back exactly after (as the
  // replay's GameState in ReplayGameDraws.cpp: each constant buffer with its
  // 11.1 window)
  struct GameState
  {
    ID3D11VertexShader* Vs = nullptr;
    ID3D11PixelShader* Ps = nullptr;
    ID3D11GeometryShader* Gs = nullptr;
    ID3D11HullShader* Hs = nullptr;
    ID3D11DomainShader* Ds = nullptr;
    ID3D11Buffer* PsCb[CbSlots] = {};
    UINT PsFirst[CbSlots] = {}, PsNum[CbSlots] = {};
    ID3D11ShaderResourceView* PsViews[ViewSlots] = {};
    ID3D11SamplerState* PsSamplers[SamplerSlots] = {};
    ID3D11InputLayout* Layout = nullptr;
    D3D11_PRIMITIVE_TOPOLOGY Topology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
    ID3D11Buffer* Vb = nullptr;
    UINT Stride = 0, Offset = 0;
    ID3D11Buffer* Ib = nullptr;
    DXGI_FORMAT IbFormat = DXGI_FORMAT_UNKNOWN;
    UINT IbOffset = 0;
    ID3D11DepthStencilState* Depth = nullptr;
    UINT StencilRef = 0;
    ID3D11BlendState* Blend = nullptr;
    FLOAT BlendFactor[4] = {};
    UINT SampleMask = 0;
    ID3D11RasterizerState* Raster = nullptr;
    UINT ViewportCount = 0, ScissorCount = 0;
    D3D11_VIEWPORT Viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] = {};
    D3D11_RECT Scissors[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] = {};
    ID3D11RenderTargetView* Rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
    ID3D11DepthStencilView* Dsv = nullptr;

    void Save(ID3D11DeviceContext1* c)
    {
      c->VSGetShader(&Vs, nullptr, nullptr);
      c->PSGetShader(&Ps, nullptr, nullptr);
      c->GSGetShader(&Gs, nullptr, nullptr);
      c->HSGetShader(&Hs, nullptr, nullptr);
      c->DSGetShader(&Ds, nullptr, nullptr);
      c->PSGetConstantBuffers1(0, CbSlots, PsCb, PsFirst, PsNum);
      c->PSGetShaderResources(0, ViewSlots, PsViews);
      c->PSGetSamplers(0, SamplerSlots, PsSamplers);
      c->IAGetInputLayout(&Layout);
      c->IAGetPrimitiveTopology(&Topology);
      c->IAGetVertexBuffers(0, 1, &Vb, &Stride, &Offset);
      c->IAGetIndexBuffer(&Ib, &IbFormat, &IbOffset);
      c->OMGetDepthStencilState(&Depth, &StencilRef);
      c->OMGetBlendState(&Blend, BlendFactor, &SampleMask);
      c->RSGetState(&Raster);
      ViewportCount = ScissorCount = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
      c->RSGetViewports(&ViewportCount, Viewports);
      c->RSGetScissorRects(&ScissorCount, Scissors);
      c->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, Rtvs, &Dsv);
    }

    void Restore(ID3D11DeviceContext1* c)
    {
      // (the targets first, so nothing bound as a target is bound to be read
      // when the views go back)
      ID3D11ShaderResourceView* none[ViewSlots] = {};
      c->PSSetShaderResources(0, ViewSlots, none);
      c->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, Rtvs, Dsv);
      c->VSSetShader(Vs, nullptr, 0);
      c->PSSetShader(Ps, nullptr, 0);
      c->GSSetShader(Gs, nullptr, 0);
      c->HSSetShader(Hs, nullptr, 0);
      c->DSSetShader(Ds, nullptr, 0);
      // Each slot as it was bound: with its 11.1 window, or whole when it has none
      for (UINT i = 0; i < CbSlots; ++i)
      {
        if (PsCb[i] && !PsNum[i])
          c->PSSetConstantBuffers(i, 1, &PsCb[i]);
        else
          c->PSSetConstantBuffers1(i, 1, &PsCb[i], &PsFirst[i], &PsNum[i]);
      }
      c->PSSetShaderResources(0, ViewSlots, PsViews);
      c->PSSetSamplers(0, SamplerSlots, PsSamplers);
      c->IASetInputLayout(Layout);
      c->IASetPrimitiveTopology(Topology);
      c->IASetVertexBuffers(0, 1, &Vb, &Stride, &Offset);
      c->IASetIndexBuffer(Ib, IbFormat, IbOffset);
      c->OMSetDepthStencilState(Depth, StencilRef);
      c->OMSetBlendState(Blend, BlendFactor, SampleMask);
      c->RSSetState(Raster);
      c->RSSetViewports(ViewportCount, Viewports);
      c->RSSetScissorRects(ScissorCount, Scissors);
      auto release = [](IUnknown* p) {
        if (p)
          p->Release();
      };
      release(Vs);
      release(Ps);
      release(Gs);
      release(Hs);
      release(Ds);
      for (auto p : PsCb) release(p);
      for (auto p : PsViews) release(p);
      for (auto p : PsSamplers) release(p);
      release(Layout);
      release(Vb);
      release(Ib);
      release(Depth);
      release(Blend);
      release(Raster);
      for (auto p : Rtvs) release(p);
      release(Dsv);
    }
  };

  // The GPU's time for the pass: its queries begun (null: none free yet)
  TimeSlot* BeginTime(ID3D11Device* device, ID3D11DeviceContext* pContext)
  {
    TimeSlot& slot = g_Times[g_TimeNext];
    if (slot.Pending)
      return nullptr;
    if (!slot.Disjoint)
    {
      D3D11_QUERY_DESC disjoint{ D3D11_QUERY_TIMESTAMP_DISJOINT, 0 }, stamp{ D3D11_QUERY_TIMESTAMP, 0 };
      if (FAILED(device->CreateQuery(&disjoint, slot.Disjoint.GetAddressOf())) ||
          FAILED(device->CreateQuery(&stamp, slot.Begin.GetAddressOf())) ||
          FAILED(device->CreateQuery(&stamp, slot.Nodes.GetAddressOf())) ||
          FAILED(device->CreateQuery(&stamp, slot.Middle.GetAddressOf())) ||
          FAILED(device->CreateQuery(&stamp, slot.End.GetAddressOf())))
      {
        slot.Disjoint.Reset();
        return nullptr;
      }
    }
    g_TimeNext = (g_TimeNext + 1) % TimeSlots;
    pContext->Begin(slot.Disjoint.Get());
    pContext->End(slot.Begin.Get());
    return &slot;
  }

  // Render thread, each frame: the times come back (never waited for), and
  // every LogMs in the log
  void ReadTimes(ID3D11DeviceContext* pContext)
  {
    for (TimeSlot& slot : g_Times)
    {
      if (!slot.Pending)
        continue;
      D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint{};
      UINT64 begin = 0, nodes = 0, middle = 0, end = 0;
      if (pContext->GetData(slot.Disjoint.Get(), &disjoint, sizeof(disjoint), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK ||
          pContext->GetData(slot.Begin.Get(), &begin, sizeof(begin), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK ||
          pContext->GetData(slot.Nodes.Get(), &nodes, sizeof(nodes), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK ||
          pContext->GetData(slot.Middle.Get(), &middle, sizeof(middle), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK ||
          pContext->GetData(slot.End.Get(), &end, sizeof(end), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK)
        continue;
      slot.Pending = false;
      if (disjoint.Disjoint || !disjoint.Frequency || nodes < begin || middle < nodes || end < middle)
        continue;
      double ms = (double)(end - begin) * 1000.0 / (double)disjoint.Frequency;
      g_TimeNodesSum += (double)(nodes - begin) * 1000.0 / (double)disjoint.Frequency;
      g_TimeInfoSum += (double)(middle - nodes) * 1000.0 / (double)disjoint.Frequency;
      g_TimeSum += ms;
      g_TimeMost = (std::max)(g_TimeMost, ms);
      ++g_TimeCount;
    }
    ULONGLONG now = GetTickCount64();
    if (!g_TimeLoggedAt)
      g_TimeLoggedAt = now;
    if (now - g_TimeLoggedAt < LogMs)
      return;
    if (g_FramesDrawn > 0)
      util::log::Write("Wet surfaces in the game's lighting (performance, diagnostic): drawn in %d frames of the last %.0f s; GPU: "
        "%.3f ms a frame (most %.2f, %d timed): the ground's nodes %.3f, where it's open and kept dry %.3f, the wet ground %.3f",
        g_FramesDrawn,
        (now - g_TimeLoggedAt) / 1000.0, g_TimeCount ? g_TimeSum / g_TimeCount : 0.0, g_TimeMost, g_TimeCount,
        g_TimeCount ? g_TimeNodesSum / g_TimeCount : 0.0, g_TimeCount ? g_TimeInfoSum / g_TimeCount : 0.0,
        g_TimeCount ? (g_TimeSum - g_TimeNodesSum - g_TimeInfoSum) / g_TimeCount : 0.0);
    g_TimeLoggedAt = now;
    g_TimeSum = g_TimeMost = g_TimeInfoSum = g_TimeNodesSum = 0.0;
    g_TimeCount = g_FramesDrawn = 0;
  }

  // The ground's nodes drawn now (gbufferwet::DrawNodes): the lattice round
  // the camera (snapped to the nodes, so each stays at its place in the world
  // as the camera moves), its constants, then one full-screen triangle over
  // their target with the wet look's constants and the frame's depth
  bool DrawNodesNow(ID3D11DeviceContext* pContext, ID3D11Buffer* constants, ID3D11ShaderResourceView* depth,
    float const camera[3])
  {
    if (g_MakeState.load(std::memory_order_acquire) != 2 || !constants || !depth || !g_NodesTarget)
      return false;
    D3D11_MAPPED_SUBRESOURCE mapped;
    if (FAILED(pContext->Map(g_NodeConstants.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
      return false;
    float* grid = (float*)mapped.pData;
    grid[0] = (floorf(camera[0] / NodeSpacing) - NodeSide / 2) * NodeSpacing;
    grid[1] = (floorf(camera[2] / NodeSpacing) - NodeSide / 2) * NodeSpacing;
    grid[2] = NodeSpacing;
    grid[3] = (float)NodeSide;
    grid[4] = camera[0];
    grid[5] = camera[1];
    grid[6] = camera[2];
    grid[7] = 0.f;
    pContext->Unmap(g_NodeConstants.Get(), 0);
    D3D11_VIEWPORT viewport{ 0.f, 0.f, (float)NodeSide, (float)NodeSide, 0.f, 1.f };
    ID3D11Buffer* noBuffer = nullptr;
    UINT zero = 0;
    const float blendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
    // (their own view unbound first: it may be bound to be read from before)
    ID3D11ShaderResourceView* none = nullptr;
    pContext->PSSetShaderResources(NodeSlot, 1, &none);
    pContext->OMSetRenderTargets(1, g_NodesTarget.GetAddressOf(), nullptr);
    pContext->RSSetViewports(1, &viewport);
    pContext->RSSetState(g_Rasterizer.Get());
    pContext->OMSetBlendState(g_Blend.Get(), blendFactor, 0xFFFFFFFF);
    pContext->OMSetDepthStencilState(g_Depth.Get(), 0);
    pContext->IASetInputLayout(nullptr);
    pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    pContext->IASetVertexBuffers(0, 1, &noBuffer, &zero, &zero);
    pContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_R16_UINT, 0);
    pContext->VSSetShader(g_Vs.Get(), nullptr, 0);
    pContext->GSSetShader(nullptr, nullptr, 0);
    pContext->HSSetShader(nullptr, nullptr, 0);
    pContext->DSSetShader(nullptr, nullptr, 0);
    pContext->PSSetShader(g_NodesPs.Get(), nullptr, 0);
    pContext->PSSetShaderResources(1, 1, &depth);
    pContext->PSSetConstantBuffers(0, 1, &constants);
    pContext->PSSetConstantBuffers(NodeCbSlot, 1, g_NodeConstants.GetAddressOf());
    pContext->Draw(3, 0);
    g_NodesDrawn = true;
    static bool s_logged = false;
    if (!s_logged)
    {
      s_logged = true;
      util::log::Write("Wet surfaces: the ground's nodes drawn (%d x %d, %.1f m apart round the camera): the puddles lie by "
        "them (WetLook.hlsl's PuddleGround), no longer stepping at a box's edges", NodeSide, NodeSide, NodeSpacing);
    }
    return true;
  }

  // The performance test's breakdown of this pass (2026-11-03, the user: "find a way to improve performance in fps even
  // with rain/snow enabled"; the 10-s line had the wet ground 2.9 - 5.0 ms of the GPU's a frame at 4K, one part): once a
  // test is over (perf::TestRunning), in BreakdownFrames frames BreakdownEveryMs apart, both its draws drawn again into
  // targets of their own (the same sizes and formats, never shown), each time with one more of their parts left out
  // (WetLook.hlsl's g_Skip, b2; GbufferWet.hlsl), timed by the GPU. A part's cost is the difference, the mean over those
  // frames; one line in CT.log. Every frame's own draws have all of them (b2 all 0).
  // (WetLook.hlsl's Skip* bits)
  const unsigned int SkipPuddleTests = 2, SkipEdgeTests = 4, SkipKept = 8, SkipWet = 16, SkipAll = 32, SkipRipples = 64,
    SkipRings = 128, SkipMarks = 4096, SkipShelter = 8192, SkipFacing = 16384;
  // (each draw leaves out what the one before did and one more; the last one all of it, SkipAll: only the depth read)
  const unsigned int InfoLeftOut[] = { SkipKept, SkipShelter };
  const unsigned int WetLeftOut[] = { SkipRipples, SkipRings, SkipPuddleTests, SkipEdgeTests, SkipMarks, SkipFacing, SkipWet };
  const int InfoVariants = (int)(sizeof(InfoLeftOut) / sizeof(InfoLeftOut[0])) + 2;
  const int WetVariants = (int)(sizeof(WetLeftOut) / sizeof(WetLeftOut[0])) + 2;
  const int BreakdownDraws = InfoVariants + WetVariants;
  const char* const InfoParts[] = { "the kept-dry boxes", "the shelter and the interiors", "the rest (the point's place, the fog)",
    "reading the depth" };
  const char* const WetParts[] = { "the rain's rings and drops on the puddles",
    "the other rings and waves (her slide, bodies, steps, drips, drainpipes)",
    "the puddles' tests (open sky, flat round, the ground's nodes, the surface on slopes)", "the water down the slopes",
    "her prints and the tyre tracks", "the facing (the depth's, the G-buffer's)",
    "the rest of the wet ground (the puddles' noise, darkening, smoothness, the facing written)",
    "the info read, the point's place, the fog", "reading the depth and the G-buffer" };
  static_assert(sizeof(InfoParts) / sizeof(InfoParts[0]) == InfoVariants, "a name for each part");
  static_assert(sizeof(WetParts) / sizeof(WetParts[0]) == WetVariants, "a name for each part");
  const int BreakdownFrames = 3;
  const ULONGLONG BreakdownEveryMs = 300, BreakdownGiveUpMs = 15000;
  struct BreakdownSlot
  {
    ComPtr<ID3D11Query> Disjoint;
    ComPtr<ID3D11Query> Stamp[BreakdownDraws + 1];
    bool Pending = false;
  };
  // Render thread
  ComPtr<ID3D11Buffer> g_SkipBuffers[BreakdownDraws + 1];   // b2: the info's draws, the wet ground's, then all 0
  int g_SkipBuffersState = 0;                               // 0 not made, 1 made, -1 couldn't be
  ComPtr<ID3D11Texture2D> g_BreakdownInfo, g_BreakdownTargets[Targets];
  ComPtr<ID3D11RenderTargetView> g_BreakdownInfoView, g_BreakdownTargetViews[Targets];
  BreakdownSlot g_BreakdownSlots[BreakdownFrames];
  int g_BreakdownQueriesState = 0;
  bool g_PerfTestWas = false;
  int g_BreakdownDrawn = -1;   // frames drawn (-1: none asked for)
  int g_BreakdownRead = 0, g_BreakdownTimed = 0;
  double g_BreakdownMs[BreakdownDraws] = {};
  ULONGLONG g_BreakdownAskedAt = 0, g_BreakdownLastAt = 0;

  // A draw's switches (b2's x)
  unsigned int BreakdownSkips(unsigned int const* leftOut, int variants, int variant)
  {
    if (variant >= variants - 1)
      return SkipAll;
    unsigned int skips = 0u;
    for (int v = 0; v < variant; ++v)
      skips |= leftOut[v];
    return skips;
  }

  // b2 for a draw: the info's variants, then the wet ground's, then
  // (BreakdownDraws) all 0; the buffers made once. Null if they couldn't be
  // (b2 then given none: its constants read 0, everything drawn).
  ID3D11Buffer* BreakdownSkip(ID3D11Device* pDevice, int draw)
  {
    if (g_SkipBuffersState == 0 && pDevice)
    {
      g_SkipBuffersState = 1;
      for (int d = 0; d <= BreakdownDraws; ++d)
      {
        unsigned int bits[4] = { d < InfoVariants ? BreakdownSkips(InfoLeftOut, InfoVariants, d) : d < BreakdownDraws ?
          BreakdownSkips(WetLeftOut, WetVariants, d - InfoVariants) : 0u, 0u, 0u, 0u };
        D3D11_BUFFER_DESC desc{ sizeof(bits), D3D11_USAGE_IMMUTABLE, D3D11_BIND_CONSTANT_BUFFER, 0, 0, 0 };
        D3D11_SUBRESOURCE_DATA data{ bits, 0, 0 };
        if (FAILED(pDevice->CreateBuffer(&desc, &data, g_SkipBuffers[d].GetAddressOf())))
          g_SkipBuffersState = -1;
      }
      if (g_SkipBuffersState < 0)
        util::log::Warning("Wet surfaces in the game's lighting: the performance test's breakdown buffers couldn't be made: no "
          "breakdown");
    }
    return g_SkipBuffersState > 0 ? g_SkipBuffers[draw].Get() : nullptr;
  }

  void EndBreakdown(char const* why)
  {
    if (why)
      util::log::Write("Performance test: the wet ground's parts in the game's lighting not timed: %s", why);
    g_BreakdownDrawn = -1;
    g_BreakdownInfoView.Reset();
    g_BreakdownInfo.Reset();
    for (int i = 0; i < Targets; ++i)
    {
      g_BreakdownTargetViews[i].Reset();
      g_BreakdownTargets[i].Reset();
    }
  }

  // Each frame at Present (FrameEnd): a test just over asks for the
  // breakdown; what's come back from the GPU added up; once all of it is in,
  // the line in the log
  void UpdateBreakdown(ID3D11DeviceContext* pContext)
  {
    bool testing = perf::TestRunning();
    ULONGLONG now = GetTickCount64();
    if (g_PerfTestWas && !testing)
    {
      g_BreakdownDrawn = 0;
      g_BreakdownRead = g_BreakdownTimed = 0;
      memset(g_BreakdownMs, 0, sizeof(g_BreakdownMs));
      g_BreakdownAskedAt = now;
      g_BreakdownLastAt = 0;
    }
    g_PerfTestWas = testing;
    if (g_BreakdownDrawn < 0 || !pContext)
      return;
    for (BreakdownSlot& slot : g_BreakdownSlots)
    {
      if (!slot.Pending)
        continue;
      D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint{};
      UINT64 stamps[BreakdownDraws + 1] = {};
      bool back = pContext->GetData(slot.Disjoint.Get(), &disjoint, sizeof(disjoint), D3D11_ASYNC_GETDATA_DONOTFLUSH) == S_OK;
      for (int s = 0; s <= BreakdownDraws && back; ++s)
        back = pContext->GetData(slot.Stamp[s].Get(), &stamps[s], sizeof(UINT64), D3D11_ASYNC_GETDATA_DONOTFLUSH) == S_OK;
      if (!back)
        continue;
      slot.Pending = false;
      ++g_BreakdownRead;
      bool ordered = true;
      for (int s = 0; s < BreakdownDraws; ++s)
        ordered = ordered && stamps[s + 1] >= stamps[s];
      if (disjoint.Disjoint || !disjoint.Frequency || !ordered)
        continue;
      double perTick = 1000.0 / (double)disjoint.Frequency;
      for (int d = 0; d < BreakdownDraws; ++d)
        g_BreakdownMs[d] += (double)(stamps[d + 1] - stamps[d]) * perTick;
      ++g_BreakdownTimed;
    }
    if (g_BreakdownDrawn >= BreakdownFrames && g_BreakdownRead >= g_BreakdownDrawn)
    {
      if (!g_BreakdownTimed)
      {
        EndBreakdown("the GPU's timings didn't hold (its clock changed while it timed them)");
        return;
      }
      double ms[BreakdownDraws];
      for (int d = 0; d < BreakdownDraws; ++d)
        ms[d] = g_BreakdownMs[d] / g_BreakdownTimed;
      // (a part's cost: its draw's time less the next one's, which leaves it out too)
      auto describe = [&](int first, int variants, const char* const* names, char* out, size_t size)
      {
        int at = 0;
        out[0] = 0;
        for (int v = 0; v < variants && at >= 0 && at < (int)size; ++v)
        {
          double cost = v + 1 < variants ? ms[first + v] - ms[first + v + 1] : ms[first + v];
          int n = sprintf_s(out + at, size - at, "%s%s %.2f", v ? "; " : "", names[v], cost);
          at = n < 0 ? -1 : at + n;
        }
      };
      char info[1024], wet[2048];
      describe(0, InfoVariants, InfoParts, info, sizeof(info));
      describe(InfoVariants, WetVariants, WetParts, wet, sizeof(wet));
      util::log::Write("Performance test, the wet ground's parts in the game's lighting (GPU ms, the mean of %d frames right after "
        "the test, every part timed in the same frame): where it's open and kept dry %.2f: %s. The wet ground %.2f: %s. (The "
        "ground's nodes before them aren't in it: the 10-s line has them.)", g_BreakdownTimed, ms[0], info, ms[InfoVariants],
        wet);
      EndBreakdown(nullptr);
      return;
    }
    if (now - g_BreakdownAskedAt > BreakdownGiveUpMs)
      EndBreakdown(g_BreakdownDrawn ? "the GPU's timings didn't come back" :
        "the wet ground wasn't drawn in the game's G-buffer in the 15 s after it (no rain or wet look, or snow)");
  }

  // A target of the breakdown's own like one of the game's (its view's
  // format), made again when that changes; false if it can't be
  bool BreakdownTargetLike(ID3D11Device* pDevice, ID3D11RenderTargetView* game, ComPtr<ID3D11Texture2D>& texture,
    ComPtr<ID3D11RenderTargetView>& view)
  {
    if (!game)
      return false;
    ComPtr<ID3D11Resource> resource;
    game->GetResource(resource.GetAddressOf());
    ComPtr<ID3D11Texture2D> gameTexture;
    if (!resource || FAILED(resource.As(&gameTexture)))
      return false;
    D3D11_TEXTURE2D_DESC want{};
    gameTexture->GetDesc(&want);
    D3D11_RENDER_TARGET_VIEW_DESC viewDesc{};
    game->GetDesc(&viewDesc);
    if (viewDesc.ViewDimension != D3D11_RTV_DIMENSION_TEXTURE2D || want.SampleDesc.Count != 1)
      return false;
    D3D11_TEXTURE2D_DESC have{};
    if (texture)
      texture->GetDesc(&have);
    if (texture && view && have.Width == want.Width && have.Height == want.Height && have.Format == want.Format)
      return true;
    view.Reset();
    texture.Reset();
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = want.Width;
    desc.Height = want.Height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = want.Format;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET;
    viewDesc.Texture2D.MipSlice = 0;
    return SUCCEEDED(pDevice->CreateTexture2D(&desc, nullptr, texture.GetAddressOf())) &&
      SUCCEEDED(pDevice->CreateRenderTargetView(texture.Get(), &viewDesc, view.GetAddressOf()));
  }

  // Right after the frame's own two draws, their state still set (its views,
  // constants, viewport): the breakdown's draws, if one is asked for and it's
  // time
  void DrawBreakdown(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, UINT infoWidth, UINT infoHeight)
  {
    if (g_BreakdownDrawn < 0 || g_BreakdownDrawn >= BreakdownFrames || !pDevice)
      return;
    ULONGLONG now = GetTickCount64();
    if (g_BreakdownLastAt && now - g_BreakdownLastAt < BreakdownEveryMs)
      return;
    if (!BreakdownSkip(pDevice, 0))
    {
      EndBreakdown("its buffers couldn't be made");
      return;
    }
    if (g_BreakdownQueriesState == 0)
    {
      g_BreakdownQueriesState = 1;
      D3D11_QUERY_DESC disjoint{ D3D11_QUERY_TIMESTAMP_DISJOINT, 0 }, stamp{ D3D11_QUERY_TIMESTAMP, 0 };
      for (BreakdownSlot& slot : g_BreakdownSlots)
      {
        if (FAILED(pDevice->CreateQuery(&disjoint, slot.Disjoint.GetAddressOf())))
          g_BreakdownQueriesState = -1;
        for (ComPtr<ID3D11Query>& s : slot.Stamp)
          if (FAILED(pDevice->CreateQuery(&stamp, s.GetAddressOf())))
            g_BreakdownQueriesState = -1;
      }
    }
    if (g_BreakdownQueriesState < 0)
    {
      EndBreakdown("the GPU's timing queries couldn't be made");
      return;
    }
    BreakdownSlot* slot = nullptr;
    for (BreakdownSlot& s : g_BreakdownSlots)
      if (!s.Pending)
      {
        slot = &s;
        break;
      }
    if (!slot)
      return;
    // Its own targets: the info's (as MakeInfo's) and two like the game's
    D3D11_TEXTURE2D_DESC have{};
    if (g_BreakdownInfo)
      g_BreakdownInfo->GetDesc(&have);
    if (!g_BreakdownInfo || have.Width != infoWidth || have.Height != infoHeight)
    {
      g_BreakdownInfoView.Reset();
      g_BreakdownInfo.Reset();
      D3D11_TEXTURE2D_DESC desc{};
      desc.Width = infoWidth;
      desc.Height = infoHeight;
      desc.MipLevels = desc.ArraySize = 1;
      desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
      desc.SampleDesc.Count = 1;
      desc.Usage = D3D11_USAGE_DEFAULT;
      desc.BindFlags = D3D11_BIND_RENDER_TARGET;
      if (FAILED(pDevice->CreateTexture2D(&desc, nullptr, g_BreakdownInfo.GetAddressOf())) ||
          FAILED(pDevice->CreateRenderTargetView(g_BreakdownInfo.Get(), nullptr, g_BreakdownInfoView.GetAddressOf())))
      {
        EndBreakdown("its targets couldn't be made");
        return;
      }
    }
    for (int i = 0; i < Targets; ++i)
      if (!BreakdownTargetLike(pDevice, g_TargetViews[i].Get(), g_BreakdownTargets[i], g_BreakdownTargetViews[i]))
      {
        EndBreakdown("its targets couldn't be made like the game's");
        return;
      }
    pContext->Begin(slot->Disjoint.Get());
    pContext->End(slot->Stamp[0].Get());
    pContext->OMSetRenderTargets(1, g_BreakdownInfoView.GetAddressOf(), nullptr);
    pContext->PSSetShader(g_InfoPs.Get(), nullptr, 0);
    for (int d = 0; d < BreakdownDraws; ++d)
    {
      if (d == InfoVariants)
      {
        ID3D11RenderTargetView* targets[Targets] = { g_BreakdownTargetViews[0].Get(), g_BreakdownTargetViews[1].Get() };
        pContext->OMSetRenderTargets(Targets, targets, nullptr);
        pContext->PSSetShader(g_Ps.Get(), nullptr, 0);
      }
      ID3D11Buffer* skip = BreakdownSkip(pDevice, d);
      pContext->PSSetConstantBuffers(2, 1, &skip);
      pContext->Draw(3, 0);
      pContext->End(slot->Stamp[d + 1].Get());
    }
    ID3D11Buffer* everything = BreakdownSkip(pDevice, BreakdownDraws);
    pContext->PSSetConstantBuffers(2, 1, &everything);
    pContext->End(slot->Disjoint.Get());
    slot->Pending = true;
    ++g_BreakdownDrawn;
    g_BreakdownLastAt = now;
  }

  // The frame's first lit pass begins: the wet ground into the G-buffer
  void Draw(ID3D11DeviceContext* pContext)
  {
    if (perf::DrawingPaused())
      return NoteSkip("the performance test leaves the passes out");
    if (!g_TargetViews[0] || !g_TargetViews[1] || !g_SceneDepth)
      return NoteSkip("the G-buffer pass's targets or depth weren't seen");
    if (!g_Context1Asked)
    {
      g_Context1Asked = true;
      pContext->QueryInterface(__uuidof(ID3D11DeviceContext1), (void**)g_Context1.GetAddressOf());
    }
    if (!g_Context1)
      return NoteSkip("no D3D 11.1 context");
    // (gameframe's copies of targets 0 and 1 and of the view constants, made
    // as this lit pass began; the material's not needed)
    gameframe::Frame copies{};
    if (!gameframe::Take(copies) || !copies.Normals || !copies.Colour || !copies.View)
      return NoteSkip("the game's G-buffer wasn't copied this frame (GameFrame.h)");
    alignas(16) float transform[16], projection[16];
    int clears = 0;
    if (!greenplane::FrameCamera(transform, projection, clears))
      return NoteSkip("this frame's camera isn't known yet (greenplane::FrameCamera)");
    if (clears > 1)
      return NoteSkip("the scene's depth was cleared again before the lights");
    // (the frame drawn from the viewport's corner, as the wet look's DepthAt has it)
    UINT width = (UINT)g_Viewport.Width, height = (UINT)g_Viewport.Height;
    D3D11_TEXTURE2D_DESC depthDesc{};
    g_SceneDepth->GetDesc(&depthDesc);
    if (g_Viewport.TopLeftX != 0.f || g_Viewport.TopLeftY != 0.f || width < 16 || height < 16 || width > depthDesc.Width ||
        height > depthDesc.Height || width > copies.Width || height > copies.Height)
      return NoteSkip("the G-buffer pass's viewport isn't the depth's and the copies' from their corner");
    ComPtr<ID3D11Device> device;
    pContext->GetDevice(device.GetAddressOf());
    if (!device)
      return;
    wetlook::GbufferPass inputs{};
    if (!wetlook::TakeGbufferPass(pContext, transform, projection, width, height, inputs))
      return NoteSkip("the wet look isn't on, wasn't drawn the frame before, or it's snow");
    // (its shader made the first time the wet look wants it)
    if (g_MakeState.load(std::memory_order_acquire) == 0)
      StartMaking(pContext);
    if (g_MakeState.load(std::memory_order_acquire) != 2)
      return NoteSkip("its shader is still being made, or couldn't be");
    ID3D11ShaderResourceView* depthView = DepthViewOf(device.Get(), g_SceneDepth.Get());
    if (!depthView)
      return NoteSkip("the game's depth can't be read");
    if (!MakeInfo(device.Get(), depthDesc.Width, depthDesc.Height))
      return NoteSkip("its info target couldn't be made");
    {
      D3D11_MAPPED_SUBRESOURCE mapped;
      if (FAILED(pContext->Map(g_Now.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        return;
      float* now = (float*)mapped.pData;
      now[0] = transform[12];
      now[1] = transform[13];
      now[2] = transform[14];
      now[3] = 0.f;
      pContext->Unmap(g_Now.Get(), 0);
    }
    // (the diagnostic dump, see g_DumpAsked: what the pass reads, as it was)
    bool dumping = g_Dump.empty() && g_DumpAsked.exchange(false);
    if (dumping)
    {
      SYSTEMTIME now{};
      GetLocalTime(&now);
      sprintf_s(g_DumpStamp, "%04u%02u%02u_%02u%02u%02u", now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond);
      sprintf_s(g_DumpNote,
        "viewport %u x %u; camera transform (rows) %.6f %.6f %.6f %.6f / %.6f %.6f %.6f %.6f / %.6f %.6f %.6f %.6f / %.6f %.6f %.6f %.6f\n"
        "projection (rows) %.8g %.8g %.8g %.8g / %.8g %.8g %.8g %.8g / %.8g %.8g %.8g %.8g / %.8g %.8g %.8g %.8g\n",
        width, height, transform[0], transform[1], transform[2], transform[3], transform[4], transform[5], transform[6],
        transform[7], transform[8], transform[9], transform[10], transform[11], transform[12], transform[13], transform[14],
        transform[15], projection[0], projection[1], projection[2], projection[3], projection[4], projection[5],
        projection[6], projection[7], projection[8], projection[9], projection[10], projection[11], projection[12],
        projection[13], projection[14], projection[15]);
      StageForDump(device.Get(), pContext, "normals_before", ResourceOf(copies.Normals).Get());
      StageForDump(device.Get(), pContext, "colour_before", ResourceOf(copies.Colour).Get());
      StageForDump(device.Get(), pContext, "material", ResourceOf(g_MaterialView.Get()).Get());
      StageForDump(device.Get(), pContext, "depth", g_SceneDepth.Get());
      StageForDump(device.Get(), pContext, "viewconstants", copies.View);
      // (the weather's squares the info pass reads for what's under the open
      // sky, t3, and the pass's constants, b0, with the squares' corner, size
      // and count: 2026-11-05, dump 20261105_161550's dry patch at a tall
      // wall's foot came from the squares, and which of them couldn't be told)
      StageForDump(device.Get(), pContext, "shelter", ResourceOf(inputs.Views[1]).Get());
      StageForDump(device.Get(), pContext, "wetparams", inputs.Constants);
      g_DumpFrames = 0;
    }
    TimeSlot* time = BeginTime(device.Get(), pContext);

    GameState saved;
    saved.Save(g_Context1.Get());
    // First the ground's nodes, into their own target (where this frame's
    // camera is: what the depth's places are relative to)
    {
      float camera[3] = { transform[12], transform[13], transform[14] };
      if (!DrawNodesNow(pContext, inputs.Constants, depthView, camera))
      {
        saved.Restore(g_Context1.Get());
        if (time)
        {
          // (the queries closed, their times not read)
          pContext->End(time->Nodes.Get());
          pContext->End(time->Middle.Get());
          pContext->End(time->End.Get());
          pContext->End(time->Disjoint.Get());
        }
        return NoteSkip("the ground's nodes couldn't be drawn");
      }
    }
    if (time)
      pContext->End(time->Nodes.Get());
    ID3D11RenderTargetView* targets[Targets] = { g_TargetViews[0].Get(), g_TargetViews[1].Get() };
    D3D11_VIEWPORT viewport{ 0.f, 0.f, (float)width, (float)height, 0.f, 1.f };
    ID3D11Buffer* noBuffer = nullptr;
    UINT zero = 0;
    const float blendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
    // (t1 the game's depth; t2, t3, t4, t9 the wet look's open sky, squares,
    // bodies' stamps and ground marks; t6, t7 the copies of the targets
    // written; t10 the info and t11 the nodes, once they're drawn; b1 the
    // game's view constants, its normals' bases; b4 the nodes' lattice)
    ID3D11ShaderResourceView* views[ViewSlots] = { nullptr, depthView, inputs.Views[0], inputs.Views[1], inputs.Views[2],
      nullptr, copies.Normals, copies.Colour, nullptr, inputs.Views[3], nullptr, nullptr };
    // (b2: the performance test's switches, all 0, every part drawn; see BreakdownSkip)
    ID3D11Buffer* constants[CbSlots] = { inputs.Constants, copies.View, BreakdownSkip(device.Get(), BreakdownDraws), g_Now.Get(),
      g_NodeConstants.Get() };
    pContext->PSSetShaderResources(0, ViewSlots, views);
    // First where it's open and kept dry, into the info target
    pContext->OMSetRenderTargets(1, g_InfoTarget.GetAddressOf(), nullptr);
    pContext->RSSetViewports(1, &viewport);
    pContext->RSSetState(g_Rasterizer.Get());
    pContext->OMSetBlendState(g_Blend.Get(), blendFactor, 0xFFFFFFFF);
    pContext->OMSetDepthStencilState(g_Depth.Get(), 0);
    pContext->IASetInputLayout(nullptr);
    pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    pContext->IASetVertexBuffers(0, 1, &noBuffer, &zero, &zero);
    pContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_R16_UINT, 0);
    pContext->VSSetShader(g_Vs.Get(), nullptr, 0);
    pContext->GSSetShader(nullptr, nullptr, 0);
    pContext->HSSetShader(nullptr, nullptr, 0);
    pContext->DSSetShader(nullptr, nullptr, 0);
    pContext->PSSetShader(g_InfoPs.Get(), nullptr, 0);
    // (the views again: the target just bound took any of its own that was
    // bound to be read)
    pContext->PSSetShaderResources(0, ViewSlots, views);
    pContext->PSSetSamplers(0, SamplerSlots, inputs.Samplers);
    pContext->PSSetConstantBuffers(0, CbSlots, constants);
    pContext->Draw(3, 0);
    if (time)
      pContext->End(time->Middle.Get());
    // Then the wet ground into the game's targets, the info read (and the
    // views again, as above)
    pContext->OMSetRenderTargets(Targets, targets, nullptr);
    views[InfoSlot] = g_InfoView.Get();
    views[NodeSlot] = g_NodesView.Get();
    pContext->PSSetShaderResources(0, ViewSlots, views);
    pContext->PSSetShader(g_Ps.Get(), nullptr, 0);
    pContext->Draw(3, 0);
    // (the performance test's breakdown, if it's asked for and it's time)
    DrawBreakdown(device.Get(), pContext, depthDesc.Width, depthDesc.Height);
    saved.Restore(g_Context1.Get());
    // (the dump: what the pass left)
    if (dumping)
    {
      StageForDump(device.Get(), pContext, "normals_after", ResourceOf(g_TargetViews[0].Get()).Get());
      StageForDump(device.Get(), pContext, "colour_after", ResourceOf(g_TargetViews[1].Get()).Get());
      StageForDump(device.Get(), pContext, "info", g_Info.Get());
      StageForDump(device.Get(), pContext, "nodes", g_Nodes.Get());
      util::log::Write("Wet surfaces in the game's lighting (diagnostic): the G-buffer dump %s staged (%zu parts), written in "
        "%d frames", g_DumpStamp, g_Dump.size(), DumpAfterFrames);
    }
    if (time)
    {
      pContext->End(time->End.Get());
      pContext->End(time->Disjoint.Get());
      time->Pending = true;
    }
    g_Drawn = true;
    g_InfoWhole = width == depthDesc.Width && height == depthDesc.Height;
    ++g_FramesDrawn;
    g_LastSkip = nullptr;
    if (!g_LoggedDrawn)
    {
      g_LoggedDrawn = true;
      util::log::Write("Wet surfaces in the game's lighting: the wet ground written into the game's G-buffer (%u x %u, its "
        "depth format %d) as its first lit pass began; the game's own lights, cube maps and screen-space reflections show it",
        width, height, (int)depthDesc.Format);
    }
  }
}

void gbufferwet::SetOn(bool on)
{
  g_On.store(on, std::memory_order_relaxed);
}

bool gbufferwet::On()
{
  return g_On.load(std::memory_order_relaxed);
}

void gbufferwet::OnPass(ID3D11DeviceContext* pContext, int from, int to, UINT count, ID3D11RenderTargetView* const* targets,
  ID3D11DepthStencilView* depth)
{
  if (!pContext || !g_On.load(std::memory_order_relaxed))
    return;
  // A G-buffer pass begins: its targets and depth (the frame's last one's kept)
  if (to == PassMain && from != PassMain && targets && count >= (UINT)Targets && targets[0] && targets[1] && depth)
  {
    for (int i = 0; i < Targets; ++i)
      g_TargetViews[i] = targets[i];
    g_MaterialView = count > 2 ? targets[2] : nullptr;
    ComPtr<ID3D11Resource> resource;
    depth->GetResource(resource.GetAddressOf());
    g_SceneDepth.Reset();
    if (resource)
      resource.As(&g_SceneDepth);
  }
  // One ends (its targets and viewport still bound): the viewport it drew with
  if (from == PassMain && to != PassMain)
  {
    g_SawGbuffer = true;
    UINT viewports = 1;
    D3D11_VIEWPORT viewport{};
    pContext->RSGetViewports(&viewports, &viewport);
    if (viewports == 1)
      g_Viewport = viewport;
  }
  // The frame's first lit pass after them (from none: the pass's own target
  // changes are then of the same kind, see above), and only that one: a
  // later one is after the screen-space reflections
  if (to == PassLit && g_SawGbuffer && !g_Tried)
  {
    g_Tried = true;
    if (from == PassNone)
      Draw(pContext);
    else
      NoteSkip("the pass before the first lit one was of another kind");
  }
}

bool gbufferwet::DrawnThisFrame()
{
  return g_Drawn;
}

ID3D11ShaderResourceView* gbufferwet::InfoThisFrame()
{
  return g_Drawn && g_InfoWhole ? g_InfoView.Get() : nullptr;
}

bool gbufferwet::NodesReady(ID3D11DeviceContext* pContext)
{
  if (!pContext)
    return false;
  if (g_MakeState.load(std::memory_order_acquire) == 0)
    StartMaking(pContext);
  return g_MakeState.load(std::memory_order_acquire) == 2;
}

bool gbufferwet::DrawNodes(ID3D11DeviceContext* pContext, ID3D11Buffer* constants, ID3D11ShaderResourceView* depth,
  float const camera[3])
{
  return pContext && DrawNodesNow(pContext, constants, depth, camera);
}

ID3D11ShaderResourceView* gbufferwet::NodesThisFrame(ID3D11Buffer** constants)
{
  if (constants)
    *constants = g_NodesDrawn ? g_NodeConstants.Get() : nullptr;
  return g_NodesDrawn ? g_NodesView.Get() : nullptr;
}

void gbufferwet::FrameEnd()
{
  if (g_Context1)
  {
    ReadTimes(g_Context1.Get());
    UpdateBreakdown(g_Context1.Get());
  }
  // (the diagnostic dump, read back once the graphics card has done it)
  if (!g_Dump.empty() && g_Context1 && ++g_DumpFrames >= DumpAfterFrames)
    WriteDump(g_Context1.Get());
  g_Drawn = false;
  g_NodesDrawn = false;
  g_InfoWhole = false;
  g_Tried = false;
  g_SawGbuffer = false;
  for (auto& view : g_TargetViews)
    view.Reset();
  g_MaterialView.Reset();
  g_SceneDepth.Reset();
}

void gbufferwet::RequestDump()
{
  g_DumpAsked.store(true);
  util::log::Write("Wet surfaces in the game's lighting (diagnostic): a G-buffer dump asked for, taken the next frame it's drawn");
}
