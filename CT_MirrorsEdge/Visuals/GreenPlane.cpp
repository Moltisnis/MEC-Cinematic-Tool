#include "GreenPlane.h"
#include "D3DStateBackup.h"
#include "GameFrame.h"
#include "../Util/Util.h"
#include "GreenPlaneVs.h"
#include "GreenPlanePs.h"

#include <DirectXMath.h>
#include <atomic>
#include <cmath>
#include <mutex>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;
using namespace DirectX;

namespace
{
  // The main render view: the camera's transform (a LinearTransform, looking
  // along -row 2) and the projection (row vectors, [2][3] = -1: view space
  // looks down -Z), as the game draws with them
  const int RenderViewTransformOffset = 0x40;
  const int RenderViewProjectionOffset = 0x360;
  // A depth target is taken for the scene's when it's one 2D texture, not
  // multisampled, shaped like the frame (its sides' ratio within this) and at
  // least this much of the frame's area; the biggest such one wins
  const float SceneDepthAspectSlack = 0.03f;
  const float SceneDepthMinArea = 0.2f;
  const ULONGLONG LogIntervalMs = 1000;
  // The sky: where the game's depth is farther than this (it drew nothing
  // there). The game's projection isn't quite an infinite one ([2][2]
  // 2.4e-6): its depth comes down to 0, the value it clears to, at about 25
  // km (its far plane) and nothing farther is drawn. 100 km was taken before
  // (2026-10-25 15:26): its depth is below 0, so nothing was ever the sky.
  const float SkyDistance = 20000.f;
  // What one draw is (g_Color.w in GreenPlane.hlsl)
  const float DrawPlane = 0.f, DrawPlaneOverSky = 1.f, DrawWholeSky = 2.f;

  struct Constants
  {
    XMFLOAT4X4 ViewProjection;
    XMFLOAT4 Corners[4];
    XMFLOAT4 Color;
    XMFLOAT4 Depth;   // the depth copy's size over the frame's (x, y), reversed (z)
  };

  // The menu's, under g_Mutex
  std::mutex g_Mutex;
  greenplane::Settings g_Settings{ false, 5.f, 10.f, 6.f, 0.f, 0.f, { 0.f, 1.f, 0.f }, false, false };
  bool g_PlaceRequested = false;
  bool g_Placed = false;
  XMFLOAT3 g_PlaceCamera{};
  XMFLOAT3 g_PlaceLook{ 0.f, 0.f, 1.f };

  // The camera as the camera hook last saw it, under g_Mutex (read straight
  // from the render view at Present it was half written now and then while
  // it moved: 38 of 91 seconds logged had the plane not drawn, 2026-10-25
  // 15:27)
  bool g_ViewOk = false;
  float g_ViewTransform[16]{};
  float g_ViewProjection[16]{};
  unsigned int g_ViewSeq = 0;
  // The camera of the frame being drawn (2026-10-25 22:xx), under g_Mutex:
  // the hook's last one as it was when the game first cleared the scene's
  // depth in this frame (the render thread starts drawing the frame the game
  // thread handed over; the game thread's next camera comes later in its next
  // frame). Taken at Present instead, the game thread had often moved on to
  // the next frame's camera while the depth was still this frame's: the wet
  // surfaces' puddles, their dry patches and the flash swam and flickered
  // as the camera moved (the user, 22:0x). Given up at the end of each Present
  // (FrameEnd); without a clear in a frame the hook's last one is used.
  bool g_FrameViewOk = false;
  float g_FrameViewTransform[16]{};
  float g_FrameViewProjection[16]{};
  unsigned int g_FrameViewSeq = 0;
  // Diagnostic (temporary): frames where the hook's camera had moved on by Present
  int g_ViewFrames = 0, g_ViewMovedOn = 0;
  ULONGLONG g_ViewLoggedAt = 0;

  // Under g_Mutex: the camera to draw this frame with (see g_FrameViewOk)
  bool FrameViewLocked(float* transform, float* projection)
  {
    if (g_FrameViewOk)
    {
      memcpy(transform, g_FrameViewTransform, sizeof(g_FrameViewTransform));
      memcpy(projection, g_FrameViewProjection, sizeof(g_FrameViewProjection));
      return true;
    }
    memcpy(transform, g_ViewTransform, sizeof(g_ViewTransform));
    memcpy(projection, g_ViewProjection, sizeof(g_ViewProjection));
    return g_ViewOk;
  }

  std::atomic<bool> g_On{ false };      // read by the clear hook
  std::atomic<unsigned> g_DepthWanted{ 0 };   // by something else drawn at Present (see WantDepth): who, as bits
  bool DepthActive() { return g_On.load(std::memory_order_relaxed) || g_DepthWanted.load(std::memory_order_relaxed) != 0; }
  std::atomic<bool> g_Failed{ false };  // its shaders or states couldn't be made: never drawn

  // The game's depth, under g_DepthMutex (the clear hook may run on another thread)
  std::mutex g_DepthMutex;
  ComPtr<ID3D11Texture2D> g_SceneDepth;
  D3D11_TEXTURE2D_DESC g_SceneDepthDesc{};
  ComPtr<ID3D11Texture2D> g_DepthCopy;
  ComPtr<ID3D11ShaderResourceView> g_DepthCopyView;
  D3D11_TEXTURE2D_DESC g_DepthCopyDesc{};
  UINT g_FrameWidth = 0, g_FrameHeight = 0;
  int g_ClearsThisFrame = 0;   // the scene depth's depth clears since the last Present
  int g_CopiedAt = -1;         // this frame: -1 not yet, 0 at Present, n before its nth clear
  // This frame's, once taken (TakeFrameDepth before Apply): Apply uses the same
  bool g_FrameTaken = false;
  ComPtr<ID3D11ShaderResourceView> g_FrameView;
  D3D11_TEXTURE2D_DESC g_FrameDesc{};
  int g_FrameClears = 0, g_FrameCopiedAt = -1;
  // Diagnostic (temporary): what was seen, logged once a second while it's on
  int g_DepthClearsSeen = 0, g_LastClears = 0, g_LastCopiedAt = -1;
  bool g_LoggedFound = false, g_LoggedCopyFailed = false;
  // Diagnostic (temporary): the game's depth at three places (the top middle,
  // the middle, the bottom middle), read back a frame or more later
  ComPtr<ID3D11Texture2D> g_Probe;
  D3D11_TEXTURE2D_DESC g_ProbeDesc{};
  bool g_ProbePending = false;
  float g_ProbeValues[3] = { -1.f, -1.f, -1.f };

  // Render thread only
  ComPtr<ID3D11VertexShader> g_Vs;
  ComPtr<ID3D11PixelShader> g_Ps;
  ComPtr<ID3D11Buffer> g_Constants;
  ComPtr<ID3D11RasterizerState> g_Rasterizer;
  ComPtr<ID3D11BlendState> g_Blend;
  ComPtr<ID3D11DepthStencilState> g_Depth;
  bool g_Made = false;
  bool g_Hooked = false;
  ULONGLONG g_LoggedAt = 0;
  D3DStateBackup g_Backup;   // big: its class instance lists

  // A depth format's typeless family and the view that reads its depth
  bool DepthFormats(DXGI_FORMAT format, DXGI_FORMAT& typeless, DXGI_FORMAT& view)
  {
    switch (format)
    {
    case DXGI_FORMAT_R24G8_TYPELESS: case DXGI_FORMAT_D24_UNORM_S8_UINT: case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
      typeless = DXGI_FORMAT_R24G8_TYPELESS;
      view = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
      return true;
    case DXGI_FORMAT_R32G8X24_TYPELESS: case DXGI_FORMAT_D32_FLOAT_S8X24_UINT: case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
      typeless = DXGI_FORMAT_R32G8X24_TYPELESS;
      view = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
      return true;
    case DXGI_FORMAT_R32_TYPELESS: case DXGI_FORMAT_D32_FLOAT: case DXGI_FORMAT_R32_FLOAT:
      typeless = DXGI_FORMAT_R32_TYPELESS;
      view = DXGI_FORMAT_R32_FLOAT;
      return true;
    case DXGI_FORMAT_R16_TYPELESS: case DXGI_FORMAT_D16_UNORM: case DXGI_FORMAT_R16_UNORM:
      typeless = DXGI_FORMAT_R16_TYPELESS;
      view = DXGI_FORMAT_R16_UNORM;
      return true;
    default:
      return false;
    }
  }

  // A view format for a typeless swapchain one
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

  // Under g_DepthMutex: a target shaped like the frame cleared that isn't
  // the G-buffer pass's, told once each (the first few)
  void NoteOtherDepth(ID3D11Texture2D* pTexture, D3D11_TEXTURE2D_DESC const& desc)
  {
    static void* s_told[8] = {};
    static int s_toldCount = 0;
    for (int i = 0; i < s_toldCount; ++i)
      if (s_told[i] == pTexture)
        return;
    if (s_toldCount >= 8)
      return;
    s_told[s_toldCount++] = pTexture;
    util::log::Write("Green screen: another depth target shaped like the frame was cleared (0x%p, %u x %u, format %d): not the "
      "G-buffer pass's, so left alone", pTexture, desc.Width, desc.Height, (int)desc.Format);
  }

  // Under g_DepthMutex: such clears left alone this frame, and frames in a
  // row where only such ones were cleared (see NoteStaleDepth)
  int g_OtherClearsThisFrame = 0, g_StaleFrames = 0, g_StaleLogged = 0;
  const int StaleDepthFrames = 30;

  // Under g_DepthMutex, once a frame (Apply's end): none of the G-buffer
  // pass's depth targets cleared for StaleDepthFrames frames while another
  // one shaped like the frame was (the game made its targets again and its
  // G-buffer pass isn't told apart yet, or clears its depth some other way):
  // those are given up on (gameframe::ForgetSceneDepths), so the scene's
  // depth is taken the old way and the wet look, the haze, the puddles and
  // the snow never wait on a target that isn't cleared any more
  void NoteStaleDepth(int sceneClears)
  {
    if (sceneClears == 0 && g_OtherClearsThisFrame > 0)
    {
      if (++g_StaleFrames == StaleDepthFrames)
      {
        gameframe::ForgetSceneDepths();
        if (g_StaleLogged++ < 4)
          util::log::Write("Green screen: the G-buffer pass's depth targets weren't cleared for %d frames while another one "
            "shaped like the frame was: given up on, the scene's depth taken the old way", StaleDepthFrames);
      }
    }
    else
      g_StaleFrames = 0;
    g_OtherClearsThisFrame = 0;
  }

  // Under g_DepthMutex
  bool IsSceneShaped(D3D11_TEXTURE2D_DESC const& desc)
  {
    if (desc.ArraySize != 1 || desc.SampleDesc.Count != 1 || !g_FrameWidth || !g_FrameHeight || !desc.Width || !desc.Height)
      return false;
    float frameAspect = (float)g_FrameWidth / (float)g_FrameHeight;
    float aspect = (float)desc.Width / (float)desc.Height;
    float area = (float)desc.Width * (float)desc.Height;
    return fabsf(aspect - frameAspect) <= frameAspect * SceneDepthAspectSlack &&
      area >= SceneDepthMinArea * (float)g_FrameWidth * (float)g_FrameHeight;
  }

  // Under g_DepthMutex: the scene's depth copied (the copy made again when
  // its size or format changes), on the context the game is using
  bool CopyDepth(ID3D11DeviceContext* pContext)
  {
    if (!g_SceneDepth || !pContext)
      return false;
    DXGI_FORMAT typeless, view;
    if (!DepthFormats(g_SceneDepthDesc.Format, typeless, view))
    {
      if (!g_LoggedCopyFailed)
      {
        g_LoggedCopyFailed = true;
        util::log::Warning("Green screen: the game's depth target has a format the tools can't read (%d), not drawn",
          (int)g_SceneDepthDesc.Format);
      }
      return false;
    }
    if (!g_DepthCopy || g_DepthCopyDesc.Width != g_SceneDepthDesc.Width || g_DepthCopyDesc.Height != g_SceneDepthDesc.Height ||
        g_DepthCopyDesc.MipLevels != g_SceneDepthDesc.MipLevels || g_DepthCopyDesc.Format != typeless)
    {
      g_DepthCopyView.Reset();
      g_DepthCopy.Reset();
      ComPtr<ID3D11Device> pDevice;
      pContext->GetDevice(pDevice.GetAddressOf());
      D3D11_TEXTURE2D_DESC desc{};
      desc.Width = g_SceneDepthDesc.Width;
      desc.Height = g_SceneDepthDesc.Height;
      desc.MipLevels = g_SceneDepthDesc.MipLevels;
      desc.ArraySize = 1;
      desc.Format = typeless;
      desc.SampleDesc.Count = 1;
      desc.Usage = D3D11_USAGE_DEFAULT;
      desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
      D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc{};
      viewDesc.Format = view;
      viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
      viewDesc.Texture2D.MipLevels = 1;
      HRESULT hr = pDevice ? pDevice->CreateTexture2D(&desc, nullptr, g_DepthCopy.GetAddressOf()) : E_FAIL;
      if (SUCCEEDED(hr))
        hr = pDevice->CreateShaderResourceView(g_DepthCopy.Get(), &viewDesc, g_DepthCopyView.GetAddressOf());
      if (FAILED(hr))
      {
        g_DepthCopyView.Reset();
        g_DepthCopy.Reset();
        if (!g_LoggedCopyFailed)
        {
          g_LoggedCopyFailed = true;
          util::log::Warning("Green screen: no copy of the game's depth could be made (%u x %u, format %d, HRESULT 0x%X), not drawn",
            desc.Width, desc.Height, (int)typeless, hr);
        }
        return false;
      }
      g_DepthCopyDesc = desc;
    }
    pContext->CopyResource(g_DepthCopy.Get(), g_SceneDepth.Get());
    return true;
  }

  bool Make(ID3D11Device* pDevice)
  {
    D3D11_BUFFER_DESC constants{};
    constants.ByteWidth = sizeof(Constants);
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

    HRESULT hr = pDevice->CreateVertexShader(g_GreenPlaneVs, sizeof(g_GreenPlaneVs), nullptr, g_Vs.GetAddressOf());
    if (SUCCEEDED(hr)) hr = pDevice->CreatePixelShader(g_GreenPlanePs, sizeof(g_GreenPlanePs), nullptr, g_Ps.GetAddressOf());
    if (SUCCEEDED(hr)) hr = pDevice->CreateBuffer(&constants, nullptr, g_Constants.GetAddressOf());
    if (SUCCEEDED(hr)) hr = pDevice->CreateRasterizerState(&rasterizer, g_Rasterizer.GetAddressOf());
    if (SUCCEEDED(hr)) hr = pDevice->CreateBlendState(&blend, g_Blend.GetAddressOf());
    if (SUCCEEDED(hr)) hr = pDevice->CreateDepthStencilState(&depth, g_Depth.GetAddressOf());
    if (FAILED(hr))
    {
      g_Failed = true;
      util::log::Error("Green screen: its shaders or render states couldn't be made (HRESULT 0x%X), it's off", hr);
      return false;
    }
    g_Made = true;
    return true;
  }

  // A render view's camera transform and projection (false: no camera in the
  // world, near the origin: a loading screen, a respawn)
  bool ReadView(__int64 pRenderView, float* transform, float* projection)
  {
    __try
    {
      if (!pRenderView)
        return false;
      memcpy(transform, (void const*)(pRenderView + RenderViewTransformOffset), 16 * sizeof(float));
      memcpy(projection, (void const*)(pRenderView + RenderViewProjectionOffset), 16 * sizeof(float));
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
    return fabsf(transform[12]) + fabsf(transform[13]) + fabsf(transform[14]) > 2.f;
  }

  // Under g_DepthMutex. Diagnostic (temporary): the depth probe read back if
  // it's there yet, and another one taken once it is
  float ProbeTexel(BYTE const* p, DXGI_FORMAT format)
  {
    switch (format)
    {
    case DXGI_FORMAT_R32G8X24_TYPELESS: case DXGI_FORMAT_R32_TYPELESS: return *(float const*)p;
    case DXGI_FORMAT_R24G8_TYPELESS: return (float)(*(UINT32 const*)p & 0xFFFFFF) / 16777215.f;
    case DXGI_FORMAT_R16_TYPELESS: return (float)*(UINT16 const*)p / 65535.f;
    default: return -1.f;
    }
  }

  void Probe(ID3D11DeviceContext* pContext, bool takeAnother)
  {
    if (g_ProbePending && g_Probe)
    {
      D3D11_MAPPED_SUBRESOURCE mapped;
      HRESULT hr = pContext->Map(g_Probe.Get(), 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &mapped);
      if (SUCCEEDED(hr))
      {
        UINT texelBytes = g_ProbeDesc.Format == DXGI_FORMAT_R32G8X24_TYPELESS ? 8 : (g_ProbeDesc.Format == DXGI_FORMAT_R16_TYPELESS ? 2 : 4);
        for (int i = 0; i < 3; ++i)
          g_ProbeValues[i] = ProbeTexel((BYTE const*)mapped.pData + i * texelBytes, g_ProbeDesc.Format);
        pContext->Unmap(g_Probe.Get(), 0);
        g_ProbePending = false;
      }
      else if (hr != DXGI_ERROR_WAS_STILL_DRAWING)
        g_ProbePending = false;
    }
    if (!takeAnother || g_ProbePending || !g_DepthCopy)
      return;
    if (!g_Probe || g_ProbeDesc.Format != g_DepthCopyDesc.Format)
    {
      g_Probe.Reset();
      ComPtr<ID3D11Device> pDevice;
      pContext->GetDevice(pDevice.GetAddressOf());
      D3D11_TEXTURE2D_DESC desc{};
      desc.Width = 3;
      desc.Height = 1;
      desc.MipLevels = 1;
      desc.ArraySize = 1;
      desc.Format = g_DepthCopyDesc.Format;
      desc.SampleDesc.Count = 1;
      desc.Usage = D3D11_USAGE_STAGING;
      desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
      if (!pDevice || FAILED(pDevice->CreateTexture2D(&desc, nullptr, g_Probe.GetAddressOf())))
        return;
      g_ProbeDesc = desc;
    }
    UINT x = g_DepthCopyDesc.Width / 2;
    UINT ys[3] = { 2, g_DepthCopyDesc.Height / 2, g_DepthCopyDesc.Height - 3 };
    for (UINT i = 0; i < 3; ++i)
    {
      D3D11_BOX box{ x, ys[i], 0, x + 1, ys[i] + 1, 1 };
      pContext->CopySubresourceRegion(g_Probe.Get(), 0, i, 0, 0, g_DepthCopy.Get(), 0, &box);
    }
    g_ProbePending = true;
  }

  XMFLOAT3 Look(float const* transform)
  {
    XMFLOAT3 look;
    XMStoreFloat3(&look, XMVector3Normalize(XMVectorSet(-transform[8], -transform[9], -transform[10], 0.f)));
    return look;
  }

  // Its corners in the world: standing up straight, facing the camera it was
  // put down from (turned by the setting), its middle Distance along where
  // that camera looked and Raise up
  void Corners(greenplane::Settings const& s, XMFLOAT3 camera, XMFLOAT3 look, XMFLOAT4* corners, XMFLOAT3& center)
  {
    center = XMFLOAT3(camera.x + look.x * s.Distance, camera.y + look.y * s.Distance + s.Raise, camera.z + look.z * s.Distance);
    float flat = sqrtf(look.x * look.x + look.z * look.z);
    float fx = flat > 1e-4f ? -look.x / flat : 0.f, fz = flat > 1e-4f ? -look.z / flat : -1.f;   // towards that camera
    float turn = s.TurnDegrees * 0.01745329f;
    float c = cosf(turn), sn = sinf(turn);
    float facingX = fx * c - fz * sn, facingZ = fx * sn + fz * c;
    float rightX = facingZ, rightZ = -facingX;   // across it, level
    float w = s.Width * 0.5f, h = s.Height * 0.5f;
    for (int i = 0; i < 4; ++i)
    {
      float sx = (i & 1) ? w : -w, sy = (i & 2) ? h : -h;
      corners[i] = XMFLOAT4(center.x + rightX * sx, center.y + sy, center.z + rightZ * sx, 1.f);
    }
  }
}

void greenplane::Set(Settings const& settings)
{
  std::lock_guard<std::mutex> lock(g_Mutex);
  g_Settings = settings;
}

void greenplane::RequestPlace()
{
  std::lock_guard<std::mutex> lock(g_Mutex);
  g_PlaceRequested = true;
}

bool greenplane::IsPlaced()
{
  std::lock_guard<std::mutex> lock(g_Mutex);
  return g_Placed;
}

bool greenplane::GetPlace(float* center)
{
  std::lock_guard<std::mutex> lock(g_Mutex);
  if (!g_Placed)
    return false;
  XMFLOAT4 corners[4];
  XMFLOAT3 middle;
  Corners(g_Settings, g_PlaceCamera, g_PlaceLook, corners, middle);
  center[0] = middle.x;
  center[1] = middle.y;
  center[2] = middle.z;
  return true;
}

namespace
{
  // Under g_DepthMutex, render thread: the scene's depth for this frame,
  // copied now unless it was before a second clear; taken once a frame (the
  // first of TakeFrameDepth and Apply), the second gets the same
  void TakeDepthOnce(ID3D11DeviceContext* pContext, D3D11_TEXTURE2D_DESC const& back, ComPtr<ID3D11ShaderResourceView>& view,
    D3D11_TEXTURE2D_DESC& desc, int& clears, int& copiedAt)
  {
    if (!g_FrameTaken)
    {
      g_FrameWidth = back.Width;
      g_FrameHeight = back.Height;
      if (g_SceneDepth && g_CopiedAt < 0 && CopyDepth(pContext))
        g_CopiedAt = 0;
      g_FrameView.Reset();
      g_FrameDesc = D3D11_TEXTURE2D_DESC{};
      if (g_CopiedAt >= 0)
      {
        g_FrameView = g_DepthCopyView;
        g_FrameDesc = g_DepthCopyDesc;
      }
      if (util::DiagnosticsOn())
        Probe(pContext, g_CopiedAt >= 0);
      g_FrameClears = g_ClearsThisFrame;
      g_FrameCopiedAt = g_CopiedAt;
      g_ClearsThisFrame = 0;
      g_CopiedAt = -1;
      g_FrameTaken = true;
    }
    view = g_FrameView;
    desc = g_FrameDesc;
    clears = g_FrameClears;
    copiedAt = g_FrameCopiedAt;
  }
}

void greenplane::WantDepth(bool want, unsigned user)
{
  if (want)
    g_DepthWanted.fetch_or(user, std::memory_order_relaxed);
  else
    g_DepthWanted.fetch_and(~user, std::memory_order_relaxed);
}

bool greenplane::TakeFrameDepth(IDXGISwapChain* pSwapChain, ID3D11Device* pDevice, ID3D11DeviceContext* pContext, FrameDepth& out)
{
  if (!g_DepthWanted.load(std::memory_order_relaxed) || !pSwapChain || !pDevice || !pContext)
    return false;
  if (!g_Hooked)
  {
    g_Hooked = true;
    if (!util::hooks::HookDepthClears(pContext))
      util::log::Warning("Green screen: the game's depth clears couldn't be hooked, its depth is taken at Present only");
  }
  ComPtr<ID3D11Texture2D> pBack;
  if (FAILED(pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)pBack.GetAddressOf())) || !pBack)
    return false;
  D3D11_TEXTURE2D_DESC back{};
  pBack->GetDesc(&back);
  bool view;
  {
    std::lock_guard<std::mutex> lock(g_Mutex);
    view = FrameViewLocked(out.Transform, out.Projection);
    // Diagnostic (temporary): how often the old way (the hook's last camera
    // at Present) would have had another frame's camera
    if (util::DiagnosticsOn())
    {
      if (g_FrameViewOk)
      {
        ++g_ViewFrames;
        if (g_ViewSeq != g_FrameViewSeq)
          ++g_ViewMovedOn;
      }
      ULONGLONG now = GetTickCount64();
      if (now - g_ViewLoggedAt >= 10000 && g_ViewFrames > 0)
      {
        util::log::Write("Wet surfaces (diagnostic): the frame's camera taken at its depth clear in %d frames of the last 10 s; "
          "in %d of them the game's camera had already moved on to the next frame by Present", g_ViewFrames, g_ViewMovedOn);
        g_ViewLoggedAt = now;
        g_ViewFrames = g_ViewMovedOn = 0;
      }
    }
  }
  ComPtr<ID3D11ShaderResourceView> pDepthView;
  D3D11_TEXTURE2D_DESC depthDesc{};
  int clears, copiedAt;
  {
    std::lock_guard<std::mutex> lock(g_DepthMutex);
    TakeDepthOnce(pContext, back, pDepthView, depthDesc, clears, copiedAt);
  }
  if (!view || !pDepthView || !back.Width || !back.Height)
    return false;
  // As Apply works it out (see there)
  float* projection = out.Projection;
  auto depthAhead = [&](float distance, float& w)
  {
    float z = -distance * projection[10] + projection[14];
    w = -distance * projection[11] + projection[15];
    return w != 0.f ? z / w : 0.f;
  };
  float w1 = 0.f, w100 = 0.f, wSky = 0.f;
  float z1 = depthAhead(1.f, w1), z100 = depthAhead(100.f, w100), zSky = depthAhead(SkyDistance, wSky);
  if (!(w1 > 0.f && w100 > 0.f && z1 != z100))
    return false;
  out.Reversed = z1 > z100;
  out.SkyDepth = out.Reversed ? (zSky > 0.f ? zSky : 0.f) : (zSky < 1.f ? zSky : 1.f);
  out.View = pDepthView.Get();   // kept alive by g_FrameView / g_DepthCopyView for the frame
  out.Scale[0] = (float)depthDesc.Width / (float)back.Width;
  out.Scale[1] = (float)depthDesc.Height / (float)back.Height;
  out.Width = back.Width;
  out.Height = back.Height;
  out.DepthWidth = depthDesc.Width;
  out.DepthHeight = depthDesc.Height;
  return true;
}

bool greenplane::FrameCamera(float* transform, float* projection, int& clears)
{
  {
    std::lock_guard<std::mutex> lock(g_DepthMutex);
    clears = g_ClearsThisFrame;
  }
  // (after the depth's lock, never both held)
  std::lock_guard<std::mutex> lock(g_Mutex);
  if (!g_FrameViewOk)
    return false;
  memcpy(transform, g_FrameViewTransform, sizeof(g_FrameViewTransform));
  memcpy(projection, g_FrameViewProjection, sizeof(g_FrameViewProjection));
  return true;
}

void greenplane::SetView(__int64 pRenderView)
{
  if (!DepthActive())
    return;
  float transform[16], projection[16];
  bool ok = ReadView(pRenderView, transform, projection);
  std::lock_guard<std::mutex> lock(g_Mutex);
  g_ViewOk = ok;
  if (ok)
  {
    memcpy(g_ViewTransform, transform, sizeof(transform));
    memcpy(g_ViewProjection, projection, sizeof(projection));
    if (util::DiagnosticsOn())   // diagnostics only (see TakeFrameDepth)
      ++g_ViewSeq;
  }
}

void greenplane::OnDepthClear(ID3D11DeviceContext* pContext, ID3D11DepthStencilView* pView, UINT flags)
{
  if (!DepthActive() || !pView || !(flags & D3D11_CLEAR_DEPTH))
    return;
  ComPtr<ID3D11Resource> pResource;
  pView->GetResource(pResource.GetAddressOf());
  ComPtr<ID3D11Texture2D> pTexture;
  if (!pResource || FAILED(pResource.As(&pTexture)))
    return;
  D3D11_TEXTURE2D_DESC desc{};
  pTexture->GetDesc(&desc);
  bool first = false;
  {
  std::lock_guard<std::mutex> lock(g_DepthMutex);
  if (util::DiagnosticsOn())   // diagnostics only (see Apply's line)
    ++g_DepthClearsSeen;
  if (!IsSceneShaped(desc))
    return;
  // Once the game's G-buffer pass has been seen, only the depth targets it
  // draws with are the scene's (gameframe::IsSceneDepth). Before, the
  // biggest target shaped like the frame won, and one as big as the scene's
  // but cleared for something else (2026-11-01, the user: "Drone takedowns
  // remove the snow shader": the disrupted drone's screen distortion is the
  // likeliest, not seen in a log yet) took its place: the snow, the wet look
  // and the green screen then had another pass's depth, nothing or the sky.
  int scene = gameframe::IsSceneDepth((__int64)pTexture.Get());
  if (scene == 0)
  {
    ++g_OtherClearsThisFrame;
    NoteOtherDepth(pTexture.Get(), desc);
    return;
  }
  if (g_SceneDepth.Get() != pTexture.Get())
  {
    // The biggest one shaped like the frame is the scene's (the G-buffer's
    // own, once known, whatever its size)
    if (scene < 0 && g_SceneDepth && (UINT64)desc.Width * desc.Height < (UINT64)g_SceneDepthDesc.Width * g_SceneDepthDesc.Height)
      return;
    g_SceneDepth = pTexture;
    g_SceneDepthDesc = desc;
    g_ClearsThisFrame = 0;
    g_CopiedAt = -1;
    // (the first few times: the game may take turns with two)
    static int s_switchesLogged = 0;
    if (!g_LoggedFound || (scene > 0 && s_switchesLogged < 8))
    {
      g_LoggedFound = true;
      s_switchesLogged += scene > 0;
      util::log::Write("Green screen: the game's depth target taken for the scene's: 0x%p, %u x %u, format %d (the frame %u x %u)%s",
        pTexture.Get(), desc.Width, desc.Height, (int)desc.Format, g_FrameWidth, g_FrameHeight,
        scene > 0 ? " (the G-buffer pass's)" : "");
    }
  }
  ++g_ClearsThisFrame;
  first = g_ClearsThisFrame == 1;
  // Cleared again in the same frame (a pass drawn on top, the first-person
  // one say): the scene's depth is copied before it's gone
  if (g_ClearsThisFrame >= 2 && g_CopiedAt < 0 && CopyDepth(pContext))
    g_CopiedAt = g_ClearsThisFrame;
  }
  // The frame's camera (see g_FrameViewOk), after the depth's lock (never both held)
  if (first)
  {
    std::lock_guard<std::mutex> lock(g_Mutex);
    g_FrameViewOk = g_ViewOk;
    memcpy(g_FrameViewTransform, g_ViewTransform, sizeof(g_ViewTransform));
    memcpy(g_FrameViewProjection, g_ViewProjection, sizeof(g_ViewProjection));
    if (util::DiagnosticsOn())   // diagnostics only (see TakeFrameDepth)
      g_FrameViewSeq = g_ViewSeq;
  }
}

void greenplane::Apply(IDXGISwapChain* pSwapChain, ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
  Settings s;
  bool placeRequested, placed, view;
  XMFLOAT3 placeCamera, placeLook;
  alignas(16) float transform[16], projection[16];
  {
    std::lock_guard<std::mutex> lock(g_Mutex);
    s = g_Settings;
    placeRequested = g_PlaceRequested;
    placed = g_Placed;
    placeCamera = g_PlaceCamera;
    placeLook = g_PlaceLook;
    view = FrameViewLocked(transform, projection);
  }
  bool on = (s.On || s.GreenSky) && !g_Failed.load(std::memory_order_relaxed) && pSwapChain && pDevice && pContext;
  g_On.store(on, std::memory_order_relaxed);
  // The frame's depth is this frame's only (see TakeDepthOnce): given up
  // whichever way this returns
  struct FrameEnd
  {
    ~FrameEnd()
    {
      {
        std::lock_guard<std::mutex> lock(g_DepthMutex);
        // (the scene's own clears this frame, wherever they were counted)
        NoteStaleDepth(g_FrameTaken ? g_FrameClears : g_ClearsThisFrame);
        if (!g_FrameTaken)
        {
          g_ClearsThisFrame = 0;
          g_CopiedAt = -1;
        }
        g_FrameTaken = false;
        g_FrameView.Reset();
      }
      std::lock_guard<std::mutex> lock(g_Mutex);
      g_FrameViewOk = false;
    }
  };
  if (!on)
  {
    FrameEnd end;
    return;
  }
  FrameEnd end;
  if (!g_Made && !Make(pDevice))
    return;
  if (!g_Hooked)
  {
    g_Hooked = true;
    if (!util::hooks::HookDepthClears(pContext))
      util::log::Warning("Green screen: the game's depth clears couldn't be hooked, its depth is taken at Present only");
  }

  ComPtr<ID3D11Texture2D> pBack;
  if (FAILED(pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)pBack.GetAddressOf())) || !pBack)
    return;
  D3D11_TEXTURE2D_DESC back{};
  pBack->GetDesc(&back);

  // The plane put where the camera looks when asked
  if (s.On && view && (placeRequested || !placed))
  {
    placeCamera = XMFLOAT3(transform[12], transform[13], transform[14]);
    placeLook = Look(transform);
    placed = true;
    std::lock_guard<std::mutex> lock(g_Mutex);
    g_PlaceCamera = placeCamera;
    g_PlaceLook = placeLook;
    g_Placed = true;
    g_PlaceRequested = false;
    util::log::Write("Green screen: put %.2f m in front of the camera at %.2f %.2f %.2f, facing it", s.Distance, placeCamera.x,
      placeCamera.y, placeCamera.z);
  }

  // The scene's depth: copied now unless it was before a second clear
  ComPtr<ID3D11ShaderResourceView> pDepthView;
  D3D11_TEXTURE2D_DESC depthDesc{};
  int clears, copiedAt;
  {
    std::lock_guard<std::mutex> lock(g_DepthMutex);
    TakeDepthOnce(pContext, back, pDepthView, depthDesc, clears, copiedAt);
    g_LastClears = clears;
    g_LastCopiedAt = copiedAt;
  }

  // The game's projection alone (not the camera: the depth of a point
  // straight ahead): reversed when a nearer point gets the bigger depth; the
  // sky at the depth SkyDistance away, never past what the depth can hold
  auto depthAhead = [&](float distance, float& w)
  {
    float z = -distance * projection[10] + projection[14];
    w = -distance * projection[11] + projection[15];
    return w != 0.f ? z / w : 0.f;
  };
  float w1 = 0.f, w100 = 0.f, wSky = 0.f;
  float z1 = depthAhead(1.f, w1), z100 = depthAhead(100.f, w100), zSky = depthAhead(SkyDistance, wSky);
  bool reversed = z1 > z100;
  float skyDepth = reversed ? (zSky > 0.f ? zSky : 0.f) : (zSky < 1.f ? zSky : 1.f);
  bool sane = view && w1 > 0.f && w100 > 0.f && z1 != z100;
  XMMATRIX cameraMatrix = XMLoadFloat4x4((XMFLOAT4X4 const*)transform);
  XMMATRIX viewProjection = XMMatrixInverse(nullptr, cameraMatrix) * XMLoadFloat4x4((XMFLOAT4X4 const*)projection);
  XMFLOAT3 camera(transform[12], transform[13], transform[14]), look = Look(transform);

  XMFLOAT4 corners[4];
  XMFLOAT3 center;
  Corners(s, placeCamera, placeLook, corners, center);
  bool drawPlane = s.On && placed;
  bool draw = sane && (drawPlane || s.GreenSky) && pDepthView && back.SampleDesc.Count == 1;

  // Diagnostic (temporary): what it saw and did, once a second
  if (util::DiagnosticsOn())
  {
    ULONGLONG now = GetTickCount64();
    if (now - g_LoggedAt >= LogIntervalMs)
    {
      g_LoggedAt = now;
      int seen;
      {
        std::lock_guard<std::mutex> lock(g_DepthMutex);
        seen = g_DepthClearsSeen;
        g_DepthClearsSeen = 0;
      }
      util::log::Write("Green screen (diagnostic): %s (the plane %s%s, the whole sky %s); the scene's depth %u x %u (format %d of the "
        "copy), its depth clears last frame %d, copied %s; %d depth clears seen in the last second; the frame %u x %u; the camera "
        "%.2f %.2f %.2f looking %.2f %.2f %.2f; the projection [0][0] %g [1][1] %g [2][2] %g [2][3] %g [3][2] %g [3][3] %g, depth at "
        "1 m %g, at 100 m %g, at %.0f m %g (%s), the sky taken at %g; the game's depth at the top middle %g, the middle %g, the "
        "bottom middle %g; the plane's middle %.2f %.2f %.2f, %.2f x %.2f m, turned %.1f",
        draw ? "drawn" : "NOT drawn", drawPlane ? "shown" : "not shown", s.KeepWorld ? " over the sky only" : "",
        s.GreenSky ? "green" : "not", depthDesc.Width, depthDesc.Height, (int)depthDesc.Format, clears,
        copiedAt < 0 ? "not at all" : (copiedAt == 0 ? "at Present" : "before a clear"), seen, back.Width, back.Height, camera.x,
        camera.y, camera.z, look.x, look.y, look.z, projection[0], projection[5], projection[10], projection[11], projection[14],
        projection[15], z1, z100, SkyDistance, zSky, reversed ? "reversed" : "not reversed", skyDepth, g_ProbeValues[0],
        g_ProbeValues[1], g_ProbeValues[2], center.x, center.y, center.z, s.Width, s.Height, s.TurnDegrees);
    }
  }
  if (!draw)
    return;

  ComPtr<ID3D11RenderTargetView> pTarget;
  D3D11_RENDER_TARGET_VIEW_DESC targetDesc{};
  targetDesc.Format = TargetFormat(back.Format);
  targetDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
  if (FAILED(pDevice->CreateRenderTargetView(pBack.Get(), targetDesc.Format != back.Format ? &targetDesc : nullptr,
      pTarget.GetAddressOf())))
    return;

  // One draw's constants (what it is: see DrawPlane)
  auto setConstants = [&](float what)
  {
    D3D11_MAPPED_SUBRESOURCE mapped;
    if (FAILED(pContext->Map(g_Constants.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
      return false;
    Constants* c = (Constants*)mapped.pData;
    XMStoreFloat4x4(&c->ViewProjection, viewProjection);
    for (int i = 0; i < 4; ++i)
      c->Corners[i] = corners[i];
    c->Color = XMFLOAT4(s.Color[0], s.Color[1], s.Color[2], what);
    c->Depth = XMFLOAT4((float)depthDesc.Width / (float)back.Width, (float)depthDesc.Height / (float)back.Height,
      reversed ? 1.f : 0.f, skyDepth);
    pContext->Unmap(g_Constants.Get(), 0);
    return true;
  };

  g_Backup.Save(pContext);

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
  pContext->VSSetConstantBuffers(0, 1, g_Constants.GetAddressOf());
  pContext->GSSetShader(nullptr, nullptr, 0);
  pContext->HSSetShader(nullptr, nullptr, 0);
  pContext->DSSetShader(nullptr, nullptr, 0);
  pContext->PSSetShader(g_Ps.Get(), nullptr, 0);
  pContext->PSSetShaderResources(0, 1, pDepthView.GetAddressOf());
  pContext->PSSetConstantBuffers(0, 1, g_Constants.GetAddressOf());
  // The whole sky (one triangle over the screen), then the plane
  if (s.GreenSky && setConstants(DrawWholeSky))
    pContext->Draw(3, 0);
  if (drawPlane && setConstants(s.KeepWorld ? DrawPlaneOverSky : DrawPlane))
    pContext->Draw(6, 0);

  g_Backup.Restore(pContext);
}
