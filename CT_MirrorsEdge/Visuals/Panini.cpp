#include "Panini.h"
#include "../Util/Util.h"
#include "PaniniVs.h"
#include "PaniniPs.h"
#include "D3DStateBackup.h"

#include <atomic>
#include <cmath>
#include <mutex>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace
{
  struct Params
  {
    bool On;
    float Distance, Crop, FovY, Aspect;
  };
  std::mutex g_Mutex;
  Params g_Params{};                     // under g_Mutex
  std::atomic<bool> g_Failed{ false };   // its shaders or states couldn't be made: never drawn

  // Render thread only
  ComPtr<ID3D11VertexShader> g_Vs;
  ComPtr<ID3D11PixelShader> g_Ps;
  ComPtr<ID3D11Buffer> g_Constants;
  ComPtr<ID3D11SamplerState> g_Sampler;
  ComPtr<ID3D11RasterizerState> g_Rasterizer;
  ComPtr<ID3D11BlendState> g_Blend;
  ComPtr<ID3D11DepthStencilState> g_Depth;
  ComPtr<ID3D11Texture2D> g_Copy;        // the frame, read by the shader
  ComPtr<ID3D11ShaderResourceView> g_CopyView;
  D3D11_TEXTURE2D_DESC g_CopyDesc{};
  bool g_Made = false;
  bool g_LoggedDrawn = false, g_LoggedSkipped = false;

  float Clamp01(float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }

  Params Read()
  {
    std::lock_guard<std::mutex> lock(g_Mutex);
    return g_Params;
  }

  bool Active(Params const& p)
  {
    return p.On && !g_Failed.load(std::memory_order_relaxed) && p.FovY > 0.05f && p.FovY < 3.f && p.Aspect > 0.3f &&
      p.Aspect < 5.f;
  }

  // The normal view's extent (tan of half the field of view across and up)
  // and the scale of the Panini view's screen: cropped by crop towards where
  // the Panini view of the normal view's edge just fills it
  void Extents(Params const& p, float& extX, float& extY, float& scale)
  {
    extY = tanf(p.FovY * 0.5f);
    extX = extY * p.Aspect;
    float d = Clamp01(p.Distance);
    float c = 1.f / sqrtf(extX * extX + 1.f);   // cos of the edge's horizontal angle
    float fit = c * (1.f + d) / (c + d);
    scale = 1.f + (fit - 1.f) * Clamp01(p.Crop);
  }

  // A view format for a typeless one (the copy is made in the swapchain's own)
  DXGI_FORMAT ViewFormat(DXGI_FORMAT format)
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

  bool IsTypeless(DXGI_FORMAT format)
  {
    return ViewFormat(format) != format;
  }

  bool Make(ID3D11Device* pDevice)
  {
    D3D11_BUFFER_DESC constants{};
    constants.ByteWidth = 16;
    constants.Usage = D3D11_USAGE_DYNAMIC;
    constants.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    constants.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    D3D11_SAMPLER_DESC sampler{};
    sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampler.MaxLOD = D3D11_FLOAT32_MAX;

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

    HRESULT hr = pDevice->CreateVertexShader(g_PaniniVs, sizeof(g_PaniniVs), nullptr, g_Vs.GetAddressOf());
    if (SUCCEEDED(hr)) hr = pDevice->CreatePixelShader(g_PaniniPs, sizeof(g_PaniniPs), nullptr, g_Ps.GetAddressOf());
    if (SUCCEEDED(hr)) hr = pDevice->CreateBuffer(&constants, nullptr, g_Constants.GetAddressOf());
    if (SUCCEEDED(hr)) hr = pDevice->CreateSamplerState(&sampler, g_Sampler.GetAddressOf());
    if (SUCCEEDED(hr)) hr = pDevice->CreateRasterizerState(&rasterizer, g_Rasterizer.GetAddressOf());
    if (SUCCEEDED(hr)) hr = pDevice->CreateBlendState(&blend, g_Blend.GetAddressOf());
    if (SUCCEEDED(hr)) hr = pDevice->CreateDepthStencilState(&depth, g_Depth.GetAddressOf());
    if (FAILED(hr))
    {
      g_Failed = true;
      util::log::Error("Panini projection: its shaders or render states couldn't be made (HRESULT 0x%X), it's off", hr);
      return false;
    }
    g_Made = true;
    return true;
  }

  // The frame's copy, made again when the swapchain's size or format changes
  bool MakeCopy(ID3D11Device* pDevice, D3D11_TEXTURE2D_DESC const& back)
  {
    if (g_Copy && back.Width == g_CopyDesc.Width && back.Height == g_CopyDesc.Height && back.Format == g_CopyDesc.Format)
      return true;
    g_CopyView.Reset();
    g_Copy.Reset();
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
    view.Format = ViewFormat(back.Format);
    view.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    view.Texture2D.MipLevels = 1;
    HRESULT hr = pDevice->CreateTexture2D(&desc, nullptr, g_Copy.GetAddressOf());
    if (SUCCEEDED(hr))
      hr = pDevice->CreateShaderResourceView(g_Copy.Get(), &view, g_CopyView.GetAddressOf());
    if (FAILED(hr))
    {
      g_CopyView.Reset();
      g_Copy.Reset();
      if (!g_LoggedSkipped)
      {
        g_LoggedSkipped = true;
        util::log::Warning("Panini projection: no copy of the frame could be made (%u x %u, format %d, HRESULT 0x%X), not drawn",
          back.Width, back.Height, (int)back.Format, hr);
      }
      return false;
    }
    g_CopyDesc = desc;
    return true;
  }

  D3DStateBackup g_Backup;   // render thread only (big: its class instance lists)
}

void panini::Set(bool on, float distance, float crop, float fovY, float aspect)
{
  std::lock_guard<std::mutex> lock(g_Mutex);
  g_Params = { on, distance, crop, fovY, aspect };
}

void panini::Apply(IDXGISwapChain* pSwapChain, ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
  Params p = Read();
  if (!Active(p) || !pSwapChain || !pDevice || !pContext)
    return;
  if (!g_Made && !Make(pDevice))
    return;

  ComPtr<ID3D11Texture2D> pBack;
  if (FAILED(pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)pBack.GetAddressOf())) || !pBack)
    return;
  D3D11_TEXTURE2D_DESC back{};
  pBack->GetDesc(&back);
  if (back.SampleDesc.Count != 1 || back.ArraySize != 1)
  {
    if (!g_LoggedSkipped)
    {
      g_LoggedSkipped = true;
      util::log::Warning("Panini projection: the game's frame is multisampled or layered (%u samples, %u layers), not drawn",
        back.SampleDesc.Count, back.ArraySize);
    }
    return;
  }
  if (!MakeCopy(pDevice, back))
    return;

  ComPtr<ID3D11RenderTargetView> pTarget;
  D3D11_RENDER_TARGET_VIEW_DESC targetDesc{};
  targetDesc.Format = ViewFormat(back.Format);
  targetDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
  if (FAILED(pDevice->CreateRenderTargetView(pBack.Get(), IsTypeless(back.Format) ? &targetDesc : nullptr,
      pTarget.GetAddressOf())))
    return;

  float extX, extY, scale;
  Extents(p, extX, extY, scale);
  D3D11_MAPPED_SUBRESOURCE mapped;
  if (FAILED(pContext->Map(g_Constants.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    return;
  float params[4] = { extX, extY, Clamp01(p.Distance), scale };
  memcpy(mapped.pData, params, sizeof(params));
  pContext->Unmap(g_Constants.Get(), 0);

  g_Backup.Save(pContext);

  pContext->CopyResource(g_Copy.Get(), pBack.Get());

  D3D11_VIEWPORT viewport{ 0.f, 0.f, (float)back.Width, (float)back.Height, 0.f, 1.f };
  ID3D11Buffer* noBuffer = nullptr;
  UINT zero = 0;
  const float blendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
  pContext->OMSetRenderTargets(1, pTarget.GetAddressOf(), nullptr);
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
  pContext->PSSetShader(g_Ps.Get(), nullptr, 0);
  pContext->PSSetShaderResources(0, 1, g_CopyView.GetAddressOf());
  pContext->PSSetSamplers(0, 1, g_Sampler.GetAddressOf());
  pContext->PSSetConstantBuffers(0, 1, g_Constants.GetAddressOf());
  pContext->Draw(3, 0);

  g_Backup.Restore(pContext);

  if (!g_LoggedDrawn)
  {
    g_LoggedDrawn = true;
    util::log::Write("Panini projection: drawn over the game's frame (%u x %u, format %d)", back.Width, back.Height,
      (int)back.Format);
  }
}

void panini::ToPanini(float& x, float& y)
{
  Params p = Read();
  if (!Active(p))
    return;
  float d = Clamp01(p.Distance);
  float c = 1.f / sqrtf(1.f + x * x);     // cos of its horizontal angle
  float s = (1.f + d) / (d + c);
  x *= s * c;
  y *= s * c;
}

float panini::ScreenScale()
{
  Params p = Read();
  if (!Active(p))
    return 1.f;
  float extX, extY, scale;
  Extents(p, extX, extY, scale);
  return scale;
}
