#include "DogenGlow.h"
#include "DogenGlowPs.h"
#include "DogenGlowVs.h"
#include "ReplayGameDraws.h"
#include "../Main.h"
#include "../Util/Perf.h"
#include "../Util/Util.h"
#include <atomic>
#include <cstring>
#include <d3d11_1.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

// Dogen's kitchen lamps lit the game's way (2026-11-03, the user: "Next thing is about dogens light we should make the
// seem like they emit light just like any other light in the game find a way"; Research_Dogen_Apartment_2026-11-03.md).
//
// How the game's own lamps glow (offline, its data and shaders): their meshes have an emissive material
// (light_int_surface_01_mesh: ss_object_emissive, EmissiveIntensity 100; _04: EmissiveIntensity 300, EmissiveColor
// (0.72, 0.94, 0.98)), and its G-buffer shaders add EmissiveColor x EmissiveIntensity to the light map's irradiance in
// their target 3 (R11G11B10_FLOAT), times exposureMultipliers.x (sp_maincity shaderdb, offset 42633456). Nothing of
// the game's lights draws a visible shape: no light prop of the game's sets EmissiveShapeEnable (1,520 files read).
// light_int_surface_05, Dogen's lamps, has one material, ss_object, with no emissive.
//
// So the same is done here: as the frame's first lit pass begins (the G-buffer whole, before its lights read it: the
// moment the wet ground goes into targets 0 and 1, GbufferWet.cpp), each lamp's face, 3 mm under its panel, is drawn
// into target 3, added to what's there, only where the G-buffer's depth shows it. Placed with the frame's own camera:
// a copy of the view constants the G-buffer pass drew with (the game's vertex shaders: world - cameraPos through
// crViewProjMatrix). The glow: white, at the game's own ceiling strip's EmissiveIntensity, 100
// (light_int_surface_01_mesh; the user, 11-03 after seeing the first: "the bloom should be white and less strong").
// It was 388 x the lamps' 7,000 K colour (0.9102, 0.9, 1): what 1,000 lm gives over the panel's face as a flat emitter.
// At night it's dimmed with the lamps' light (2026-11-08; DogenDoor.cpp's LampShare), through the draw's blend factor.
namespace
{
  // ReplayGameDraws' passes
  const int PassNone = -1, PassMain = 0, PassLit = 2;
  const int GbufferTargets = 4, GlowTarget = 3;
  const DXGI_FORMAT GlowTargetFormat = DXGI_FORMAT_R11G11B10_FLOAT;
  // The game's view constants (its shaders' b2, 1120 bytes; Research_Snow section 23)
  const UINT ViewConstantsSlot = 2, ViewConstantsBytes = 1120;

  // The lamps' faces (zone 397's light_int_surface_05 placements, the data's rows; DogenDoor.cpp checks lamp 0's
  // origin in memory first): the mesh's face, x -0.16 .. 0.16 along its right, 0 .. 2.5605 along its up, at 0.08
  // along its forward (straight down), drawn at 0.083
  const int Lamps = 6;
  const float Corners[Lamps * 4][3] = {
    // the back room
    { 173.7598f, 69.6368f, 310.8963f }, { 173.5820f, 69.6368f, 311.1623f }, { 171.6308f, 69.6368f, 309.4737f }, { 171.4530f, 69.6368f, 309.7398f },
    { 175.2270f, 69.9369f, 308.1243f }, { 175.0493f, 69.9369f, 308.3904f }, { 173.0981f, 69.9369f, 306.7018f }, { 172.9203f, 69.9369f, 306.9678f },
    { 182.1173f, 69.9369f, 307.2440f }, { 181.9396f, 69.9369f, 307.5100f }, { 179.9884f, 69.9369f, 305.8214f }, { 179.8106f, 69.9369f, 306.0875f },
    { 177.7604f, 69.9369f, 304.3328f }, { 177.5827f, 69.9369f, 304.5989f }, { 175.6315f, 69.9369f, 302.9102f }, { 175.4537f, 69.9369f, 303.1763f },
    // the back room
    { 176.9526f, 69.6368f, 313.0296f }, { 176.7748f, 69.6368f, 313.2957f }, { 174.8236f, 69.6368f, 311.6071f }, { 174.6458f, 69.6368f, 311.8732f },
    { 179.5839f, 69.9369f, 311.0355f }, { 179.4062f, 69.9369f, 311.3016f }, { 177.4550f, 69.9369f, 309.6129f }, { 177.2772f, 69.9369f, 309.8790f } };
  const float GlowLuminance = 100.f;
  const float GlowColor[3] = { 1.f, 1.f, 1.f };

  std::atomic<bool> g_Wanted{ false };
  std::atomic<float> g_Share{ 1.f };   // the glow's share of the day's: the draw's blend factor
  bool g_HookAsked = false;

  // This frame's
  int g_MainPasses = 0;
  ComPtr<ID3D11RenderTargetView> g_Target;   // the frame's first G-buffer pass's target 3
  ComPtr<ID3D11Texture2D> g_Depth;           // and its depth
  D3D11_VIEWPORT g_Viewport{};
  bool g_ViewCopied = false;
  bool g_Tried = false;

  // Made once
  ComPtr<ID3D11DeviceContext1> g_Context1;
  bool g_Context1Asked = false;
  ComPtr<ID3D11VertexShader> g_Vs;
  ComPtr<ID3D11PixelShader> g_Ps;
  ComPtr<ID3D11Buffer> g_Constants, g_View;
  UINT g_ViewWidth = 0;
  ComPtr<ID3D11BlendState> g_Blend;
  ComPtr<ID3D11DepthStencilState> g_NoDepth;
  ComPtr<ID3D11RasterizerState> g_Raster;
  bool g_MakeFailed = false;
  // The depth's view, for the texture it was made for (by its address: the view holds it alive)
  ComPtr<ID3D11ShaderResourceView> g_DepthView;
  ID3D11Texture2D* g_DepthViewOf = nullptr;

  // The log
  bool g_LoggedDrawn = false;
  char const* g_LastSkip = nullptr;

  void NoteSkip(char const* why)
  {
    if (why == g_LastSkip)
      return;
    g_LastSkip = why;
    util::log::Write("Dogen's kitchen: the lamps' glow not drawn this frame: %s", why);
  }

  bool Make(ID3D11Device* device)
  {
    if (g_Vs)
      return true;
    if (g_MakeFailed)
      return false;
    g_MakeFailed = true;
    if (FAILED(device->CreateVertexShader(g_DogenGlowVs, sizeof(g_DogenGlowVs), nullptr, g_Vs.GetAddressOf())) ||
        FAILED(device->CreatePixelShader(g_DogenGlowPs, sizeof(g_DogenGlowPs), nullptr, g_Ps.GetAddressOf())))
    {
      g_Vs.Reset();
      util::log::Warning("Dogen's kitchen: the lamps' glow shaders couldn't be made: not drawn");
      return false;
    }
    // The corners and the glow, made once (they never change)
    alignas(16) float constants[(Lamps * 4 + 1) * 4] = {};
    for (int i = 0; i < Lamps * 4; ++i)
      memcpy(&constants[i * 4], Corners[i], sizeof(Corners[i]));
    for (int c = 0; c < 3; ++c)
      constants[Lamps * 4 * 4 + c] = GlowColor[c] * GlowLuminance;
    D3D11_BUFFER_DESC b{};
    b.ByteWidth = sizeof(constants);
    b.Usage = D3D11_USAGE_IMMUTABLE;
    b.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    D3D11_SUBRESOURCE_DATA data{ constants, 0, 0 };
    // Added to what's there (the game's target 3 has no alpha), times the blend factor: the share of the day's glow
    D3D11_BLEND_DESC blend{};
    blend.RenderTarget[0].BlendEnable = TRUE;
    blend.RenderTarget[0].SrcBlend = D3D11_BLEND_BLEND_FACTOR;
    blend.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
    blend.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blend.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
    blend.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
    blend.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blend.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN |
      D3D11_COLOR_WRITE_ENABLE_BLUE;
    // (the depth is tested in the pixel shader, against a view of the G-buffer's)
    D3D11_DEPTH_STENCIL_DESC depth{};
    depth.DepthEnable = FALSE;
    depth.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    depth.DepthFunc = D3D11_COMPARISON_ALWAYS;
    D3D11_RASTERIZER_DESC raster{};
    raster.FillMode = D3D11_FILL_SOLID;
    raster.CullMode = D3D11_CULL_NONE;
    raster.DepthClipEnable = TRUE;
    if (FAILED(device->CreateBuffer(&b, &data, g_Constants.GetAddressOf())) ||
        FAILED(device->CreateBlendState(&blend, g_Blend.GetAddressOf())) ||
        FAILED(device->CreateDepthStencilState(&depth, g_NoDepth.GetAddressOf())) ||
        FAILED(device->CreateRasterizerState(&raster, g_Raster.GetAddressOf())))
    {
      g_Vs.Reset();
      util::log::Warning("Dogen's kitchen: the lamps' glow states couldn't be made: not drawn");
      return false;
    }
    g_MakeFailed = false;
    return true;
  }

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

  ID3D11ShaderResourceView* DepthView(ID3D11Device* device, ID3D11Texture2D* texture)
  {
    if (g_DepthView && g_DepthViewOf == texture)
      return g_DepthView.Get();
    g_DepthView.Reset();
    g_DepthViewOf = nullptr;
    D3D11_TEXTURE2D_DESC d{};
    texture->GetDesc(&d);
    DXGI_FORMAT format = DepthViewFormat(d.Format);
    if (format == DXGI_FORMAT_UNKNOWN || !(d.BindFlags & D3D11_BIND_SHADER_RESOURCE) || d.SampleDesc.Count != 1 ||
        d.ArraySize != 1)
      return nullptr;
    D3D11_SHADER_RESOURCE_VIEW_DESC view{};
    view.Format = format;
    view.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    view.Texture2D.MipLevels = 1;
    if (FAILED(device->CreateShaderResourceView(texture, &view, g_DepthView.GetAddressOf())))
      return nullptr;
    g_DepthViewOf = texture;
    return g_DepthView.Get();
  }

  // The view constants the G-buffer pass drew with, copied while they're still bound (as GameFrame.cpp does)
  bool CopyView(ID3D11DeviceContext* pContext)
  {
    ID3D11Buffer* view = nullptr;
    UINT first = 0, num = 0;
    g_Context1->PSGetConstantBuffers1(ViewConstantsSlot, 1, &view, &first, &num);
    if (!view)
      g_Context1->VSGetConstantBuffers1(ViewConstantsSlot, 1, &view, &first, &num);
    if (!view)
      return false;
    D3D11_BUFFER_DESC d{};
    view->GetDesc(&d);
    bool ok = first == 0 && d.ByteWidth >= ViewConstantsBytes && d.ByteWidth <= 65536 &&
      (d.BindFlags & D3D11_BIND_CONSTANT_BUFFER);
    if (ok && (!g_View || g_ViewWidth != d.ByteWidth))
    {
      g_View.Reset();
      g_ViewWidth = 0;
      ComPtr<ID3D11Device> device;
      pContext->GetDevice(device.GetAddressOf());
      D3D11_BUFFER_DESC c{};
      c.ByteWidth = d.ByteWidth;
      c.Usage = D3D11_USAGE_DEFAULT;
      c.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
      ok = device && SUCCEEDED(device->CreateBuffer(&c, nullptr, g_View.GetAddressOf()));
      if (ok)
        g_ViewWidth = d.ByteWidth;
    }
    if (ok)
      pContext->CopyResource(g_View.Get(), view);
    view->Release();
    return ok;
  }

  // What the draw changes, put back after it
  struct Saved
  {
    ID3D11VertexShader* Vs = nullptr;
    ID3D11PixelShader* Ps = nullptr;
    ID3D11GeometryShader* Gs = nullptr;
    ID3D11HullShader* Hs = nullptr;
    ID3D11DomainShader* Ds = nullptr;
    ID3D11Buffer* VsCb[2] = {};
    UINT VsFirst[2] = {}, VsNum[2] = {};
    ID3D11Buffer* PsCb[2] = {};
    UINT PsFirst[2] = {}, PsNum[2] = {};
    // (all of the pixel shader's first views: binding the target unbinds any view of its texture, which goes back)
    static const UINT ViewSlots = 16;
    ID3D11ShaderResourceView* PsViews[ViewSlots] = {};
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
    float BlendFactor[4] = {};
    UINT SampleMask = 0;
    ID3D11RasterizerState* Raster = nullptr;
    UINT ViewportCount = 0;
    D3D11_VIEWPORT Viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] = {};
    ID3D11RenderTargetView* Rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
    ID3D11DepthStencilView* Dsv = nullptr;

    void Save(ID3D11DeviceContext1* c)
    {
      c->VSGetShader(&Vs, nullptr, nullptr);
      c->PSGetShader(&Ps, nullptr, nullptr);
      c->GSGetShader(&Gs, nullptr, nullptr);
      c->HSGetShader(&Hs, nullptr, nullptr);
      c->DSGetShader(&Ds, nullptr, nullptr);
      c->VSGetConstantBuffers1(0, 2, VsCb, VsFirst, VsNum);
      c->PSGetConstantBuffers1(0, 2, PsCb, PsFirst, PsNum);
      c->PSGetShaderResources(0, ViewSlots, PsViews);
      c->IAGetInputLayout(&Layout);
      c->IAGetPrimitiveTopology(&Topology);
      c->IAGetVertexBuffers(0, 1, &Vb, &Stride, &Offset);
      c->IAGetIndexBuffer(&Ib, &IbFormat, &IbOffset);
      c->OMGetDepthStencilState(&Depth, &StencilRef);
      c->OMGetBlendState(&Blend, BlendFactor, &SampleMask);
      c->RSGetState(&Raster);
      ViewportCount = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
      c->RSGetViewports(&ViewportCount, Viewports);
      c->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, Rtvs, &Dsv);
    }

    // A constant buffer slot as it was bound: with its 11.1 window, or whole when it has none
    static void SetCb(ID3D11DeviceContext1* c, bool vs, UINT slot, ID3D11Buffer* b, UINT first, UINT num)
    {
      if (b && !num)
        vs ? c->VSSetConstantBuffers(slot, 1, &b) : c->PSSetConstantBuffers(slot, 1, &b);
      else
        vs ? c->VSSetConstantBuffers1(slot, 1, &b, &first, &num) : c->PSSetConstantBuffers1(slot, 1, &b, &first, &num);
    }

    void Restore(ID3D11DeviceContext1* c)
    {
      // (the targets first, so nothing bound as a target is bound to be read when the views go back)
      ID3D11ShaderResourceView* none[ViewSlots] = {};
      c->PSSetShaderResources(0, ViewSlots, none);
      c->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, Rtvs, Dsv);
      c->VSSetShader(Vs, nullptr, 0);
      c->PSSetShader(Ps, nullptr, 0);
      c->GSSetShader(Gs, nullptr, 0);
      c->HSSetShader(Hs, nullptr, 0);
      c->DSSetShader(Ds, nullptr, 0);
      for (UINT i = 0; i < 2; ++i)
      {
        SetCb(c, true, i, VsCb[i], VsFirst[i], VsNum[i]);
        SetCb(c, false, i, PsCb[i], PsFirst[i], PsNum[i]);
      }
      c->PSSetShaderResources(0, ViewSlots, PsViews);
      c->IASetInputLayout(Layout);
      c->IASetPrimitiveTopology(Topology);
      c->IASetVertexBuffers(0, 1, &Vb, &Stride, &Offset);
      c->IASetIndexBuffer(Ib, IbFormat, IbOffset);
      c->OMSetDepthStencilState(Depth, StencilRef);
      c->OMSetBlendState(Blend, BlendFactor, SampleMask);
      c->RSSetState(Raster);
      c->RSSetViewports(ViewportCount, Viewports);
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
      for (auto p : PsViews) release(p);
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

  void Draw(ID3D11DeviceContext* pContext)
  {
    if (perf::DrawingPaused())
      return NoteSkip("the performance test leaves the passes out");
    if (!g_Target || !g_Depth)
      return NoteSkip("the G-buffer pass's target 3 or depth weren't seen");
    if (!g_ViewCopied)
      return NoteSkip("the G-buffer pass's view constants weren't copied");
    ComPtr<ID3D11Device> device;
    pContext->GetDevice(device.GetAddressOf());
    if (!device || !Make(device.Get()))
      return;
    D3D11_TEXTURE2D_DESC depthDesc{};
    g_Depth->GetDesc(&depthDesc);
    // (the frame drawn from the viewport's corner, the depth read by the pixel's place)
    if (g_Viewport.TopLeftX != 0.f || g_Viewport.TopLeftY != 0.f || g_Viewport.Width < 16.f || g_Viewport.Height < 16.f ||
        g_Viewport.Width > (float)depthDesc.Width || g_Viewport.Height > (float)depthDesc.Height)
      return NoteSkip("the G-buffer pass's viewport isn't the depth's from its corner");
    ID3D11ShaderResourceView* depthView = DepthView(device.Get(), g_Depth.Get());
    if (!depthView)
      return NoteSkip("the game's depth can't be read");

    Saved saved;
    saved.Save(g_Context1.Get());
    D3D11_VIEWPORT viewport = g_Viewport;
    viewport.MinDepth = 0.f;
    viewport.MaxDepth = 1.f;
    ID3D11Buffer* constants[2] = { g_Constants.Get(), g_View.Get() };
    ID3D11Buffer* noBuffer = nullptr;
    UINT zero = 0;
    float share = g_Share.load(std::memory_order_relaxed);
    const float blendFactor[4] = { share, share, share, 1.f };
    pContext->OMSetRenderTargets(1, g_Target.GetAddressOf(), nullptr);
    pContext->RSSetViewports(1, &viewport);
    pContext->RSSetState(g_Raster.Get());
    pContext->OMSetBlendState(g_Blend.Get(), blendFactor, 0xFFFFFFFF);
    pContext->OMSetDepthStencilState(g_NoDepth.Get(), 0);
    pContext->IASetInputLayout(nullptr);
    pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    pContext->IASetVertexBuffers(0, 1, &noBuffer, &zero, &zero);
    pContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_R16_UINT, 0);
    pContext->VSSetShader(g_Vs.Get(), nullptr, 0);
    pContext->GSSetShader(nullptr, nullptr, 0);
    pContext->HSSetShader(nullptr, nullptr, 0);
    pContext->DSSetShader(nullptr, nullptr, 0);
    pContext->PSSetShader(g_Ps.Get(), nullptr, 0);
    pContext->VSSetConstantBuffers(0, 2, constants);
    pContext->PSSetConstantBuffers(0, 2, constants);
    // (the view after the target: binding a target unbinds a view of the same texture)
    pContext->PSSetShaderResources(0, 1, &depthView);
    pContext->Draw(Lamps * 6, 0);
    saved.Restore(g_Context1.Get());
    g_LastSkip = nullptr;
    if (!g_LoggedDrawn)
    {
      g_LoggedDrawn = true;
      util::log::Write("Dogen's kitchen: the lamps' glow added into the game's G-buffer target 3 as its first lit pass began "
        "(%.0f x %.0f, depth format %d), as the game's emissive materials put theirs: its lights, reflections and bloom take it",
        g_Viewport.Width, g_Viewport.Height, (int)depthDesc.Format);
    }
  }
}

void dogenglow::SetWanted(bool wanted)
{
  g_Wanted.store(wanted, std::memory_order_relaxed);
}

void dogenglow::SetShare(float share)
{
  g_Share.store(share < 0.f ? 0.f : (share > 1.f ? 1.f : share), std::memory_order_relaxed);
}

void dogenglow::OnPass(ID3D11DeviceContext* pContext, int from, int to, UINT count, ID3D11RenderTargetView* const* targets,
  ID3D11DepthStencilView* depth)
{
  if (!pContext || !g_Wanted.load(std::memory_order_relaxed))
    return;
  if (!g_Context1Asked)
  {
    g_Context1Asked = true;
    pContext->QueryInterface(__uuidof(ID3D11DeviceContext1), (void**)g_Context1.GetAddressOf());
    if (!g_Context1)
      util::log::Warning("Dogen's kitchen: no D3D 11.1 context: the lamps' glow isn't drawn");
  }
  if (!g_Context1)
    return;
  // The frame's first G-buffer pass begins: its target 3 and depth
  if (to == PassMain && from != PassMain && ++g_MainPasses == 1)
  {
    g_Target.Reset();
    g_Depth.Reset();
    if (targets && count >= (UINT)GbufferTargets && targets[GlowTarget] && depth)
    {
      D3D11_RENDER_TARGET_VIEW_DESC d{};
      targets[GlowTarget]->GetDesc(&d);
      if (d.Format == GlowTargetFormat && d.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2D)
      {
        g_Target = targets[GlowTarget];
        ComPtr<ID3D11Resource> resource;
        depth->GetResource(resource.GetAddressOf());
        if (resource)
          resource.As(&g_Depth);
      }
    }
  }
  // It ends (its constants and viewport still bound)
  if (from == PassMain && to != PassMain && g_MainPasses == 1 && !g_ViewCopied)
  {
    UINT viewports = 1;
    D3D11_VIEWPORT viewport{};
    pContext->RSGetViewports(&viewports, &viewport);
    if (viewports == 1)
      g_Viewport = viewport;
    g_ViewCopied = viewports == 1 && CopyView(pContext);
  }
  // The frame's first lit pass after it, from no pass (as GbufferWet's: a later one is after the screen-space
  // reflections), and only that one
  if (to == PassLit && g_MainPasses >= 1 && !g_Tried)
  {
    g_Tried = true;
    if (from == PassNone)
      Draw(pContext);
    else
      NoteSkip("the pass before the first lit one was of another kind");
  }
}

void dogenglow::FrameEnd()
{
  g_MainPasses = 0;
  g_Tried = false;
  g_ViewCopied = false;
  g_Target.Reset();
  g_Depth.Reset();
  // The game's render target changes watched once the glow is first wanted (with no weather on nothing else may
  // have asked; replay_gamedraws::Hook puts it in once, whoever asks first)
  if (!g_HookAsked && g_Wanted.load(std::memory_order_relaxed) && g_d3d11Context)
  {
    g_HookAsked = true;
    replay_gamedraws::Hook(g_d3d11Context);
    util::log::Write("Dogen's kitchen: the game's render target changes watched for the lamps' glow");
  }
}
