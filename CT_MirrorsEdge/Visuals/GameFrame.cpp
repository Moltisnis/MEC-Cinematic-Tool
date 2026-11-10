#include "GameFrame.h"
#include "GbufferWet.h"
#include "SsrOrder.h"
#include "../Camera/ReplayGameDraws.h"
#include "../Camera/DogenGlow.h"
#include "../Util/Util.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <d3d11_1.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace
{
  // ReplayGameDraws' Pass
  const int PassMain = 0, PassDecal = 1, PassLit = 2;
  // The G-buffer's targets copied: its normals, base colour and material
  const int Copied = 3;
  // Their views: what each target is read as where its texture is typeless
  // (the game's own targets' formats, ReplayGameDraws' GbufferFormats)
  const DXGI_FORMAT ViewFormats[Copied] = { DXGI_FORMAT_R10G10B10A2_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
    DXGI_FORMAT_R8G8B8A8_UNORM };
  // viewConstants: the shaders' b2, at least this long (to its last field)
  const int ViewConstantsSlot = 2;
  const UINT ViewConstantsBytes = 1120;

  // The G-buffer pass's depth textures, the last few seen (any thread reads
  // them; a new one takes the next place round)
  const int SceneDepths = 4;
  std::atomic<__int64> g_SceneDepths[SceneDepths] = {};
  int g_SceneDepthNext = 0;   // render thread
  // Those given up on (ForgetSceneDepths), render thread: not learned again
  __int64 g_Forgotten[SceneDepths] = {};

  // Render thread
  bool g_Hooked = false;
  bool g_Wanted = false;
  bool g_WantAll = true;   // the three targets, else the first two (see Want)
  bool g_CopiedAll = false;   // the last copy had the material's too (else what's in its copy is older)
  int g_MainPasses = 0;                          // G-buffer passes begun this frame
  bool g_Copied = false;                         // this frame's copies made
  bool g_ViewCopied = false;                     // this frame's view constants copied (the first G-buffer pass's end)
  bool g_LitSeen = false;                        // this frame's first lit pass begun after a G-buffer pass
  bool g_LoggedNoLit = false;
  ComPtr<ID3D11Texture2D> g_Source[Copied];      // this frame's G-buffer targets
  ComPtr<ID3D11Texture2D> g_Copy[Copied];
  ComPtr<ID3D11ShaderResourceView> g_CopyView[Copied];
  D3D11_TEXTURE2D_DESC g_CopyDesc[Copied]{};
  ComPtr<ID3D11Buffer> g_View;
  UINT g_ViewWidth = 0;
  ComPtr<ID3D11DeviceContext1> g_Context1;
  bool g_Context1Asked = false;
  bool g_LoggedCopied = false, g_LoggedFailed = false, g_LoggedSeveral = false;
  int g_LoggedDepths = 0, g_LoggedForgot = 0;
  // Frames in a row with no G-buffer pass (see FrameEnd)
  const int FramesWithoutGbuffer = 30;
  int g_FramesWithout = 0;

  bool IsTypeless(DXGI_FORMAT format)
  {
    switch (format)
    {
    case DXGI_FORMAT_R10G10B10A2_TYPELESS: case DXGI_FORMAT_R8G8B8A8_TYPELESS: case DXGI_FORMAT_B8G8R8A8_TYPELESS:
    case DXGI_FORMAT_R16G16B16A16_TYPELESS: case DXGI_FORMAT_R32G32B32A32_TYPELESS:
      return true;
    default:
      return false;
    }
  }

  // Told once; false
  bool Failed(char const* what, HRESULT hr)
  {
    if (!g_LoggedFailed)
    {
      g_LoggedFailed = true;
      util::log::Warning("Snow: the game's G-buffer couldn't be copied (%s, 0x%X): the snow takes its facing and light from the "
        "depth and the picture as before", what, hr);
    }
    return false;
  }

  // A target's copy, made again when its texture's size or format changes
  bool CopyTarget(ID3D11DeviceContext* pContext, int i)
  {
    if (!g_Source[i])
      return Failed("a target wasn't seen", S_OK);
    D3D11_TEXTURE2D_DESC d{};
    g_Source[i]->GetDesc(&d);
    if (d.SampleDesc.Count != 1 || d.ArraySize != 1)
      return Failed("a target is multisampled or an array", S_OK);
    D3D11_TEXTURE2D_DESC& have = g_CopyDesc[i];
    if (!g_Copy[i] || have.Width != d.Width || have.Height != d.Height || have.Format != d.Format || have.MipLevels != d.MipLevels)
    {
      g_CopyView[i].Reset();
      g_Copy[i].Reset();
      have = D3D11_TEXTURE2D_DESC{};
      ComPtr<ID3D11Device> device;
      pContext->GetDevice(device.GetAddressOf());
      D3D11_TEXTURE2D_DESC desc = d;
      desc.Usage = D3D11_USAGE_DEFAULT;
      desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
      desc.CPUAccessFlags = 0;
      desc.MiscFlags = 0;
      D3D11_SHADER_RESOURCE_VIEW_DESC view{};
      view.Format = IsTypeless(d.Format) ? ViewFormats[i] : d.Format;
      view.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
      view.Texture2D.MipLevels = 1;
      HRESULT hr = device ? device->CreateTexture2D(&desc, nullptr, g_Copy[i].GetAddressOf()) : E_FAIL;
      if (SUCCEEDED(hr))
        hr = device->CreateShaderResourceView(g_Copy[i].Get(), &view, g_CopyView[i].GetAddressOf());
      if (FAILED(hr))
      {
        g_CopyView[i].Reset();
        g_Copy[i].Reset();
        return Failed("its copy couldn't be made", hr);
      }
      have = d;
    }
    pContext->CopyResource(g_Copy[i].Get(), g_Source[i].Get());
    return true;
  }

  // The view constants bound for the pass's draws (b2), copied whole; the
  // game binds its own buffer for them (from its start, as the replay's
  // copy of them found)
  bool CopyViewConstants(ID3D11DeviceContext* pContext)
  {
    if (!g_Context1Asked)
    {
      g_Context1Asked = true;
      pContext->QueryInterface(__uuidof(ID3D11DeviceContext1), (void**)g_Context1.GetAddressOf());
    }
    if (!g_Context1)
      return Failed("no D3D 11.1 context", S_OK);
    ID3D11Buffer* view = nullptr;
    UINT first = 0, num = 0;
    g_Context1->PSGetConstantBuffers1(ViewConstantsSlot, 1, &view, &first, &num);
    if (!view)
      g_Context1->VSGetConstantBuffers1(ViewConstantsSlot, 1, &view, &first, &num);
    if (!view)
      return Failed("no view constants bound", S_OK);
    D3D11_BUFFER_DESC d{};
    view->GetDesc(&d);
    bool ok = first == 0 && d.ByteWidth >= ViewConstantsBytes && d.ByteWidth <= 65536;
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
      HRESULT hr = device ? device->CreateBuffer(&c, nullptr, g_View.GetAddressOf()) : E_FAIL;
      if (FAILED(hr))
      {
        view->Release();
        return Failed("the view constants' copy couldn't be made", hr);
      }
      g_ViewWidth = d.ByteWidth;
    }
    if (ok)
      pContext->CopyResource(g_View.Get(), view);
    else
      Failed("the view constants are bound as a window or are too short", S_OK);
    view->Release();
    return ok;
  }
}

void gameframe::Want(ID3D11DeviceContext* pImmediate, bool copies, bool all)
{
  g_Wanted = copies;
  g_WantAll = all;
  if (!g_Hooked && pImmediate)
  {
    g_Hooked = true;
    // (the replay's hook of the immediate context's render target changes:
    // put in once, whoever asks first)
    replay_gamedraws::Hook(pImmediate);
    util::log::Write("Snow: the game's render target changes are watched (its G-buffer pass: the scene's depth, and its normals "
      "and colours for the snow)");
  }
}

void gameframe::OnPass(ID3D11DeviceContext* pContext, int from, int to, UINT count, ID3D11RenderTargetView* const* targets,
  ID3D11DepthStencilView* depth)
{
  if (!pContext)
    return;
  // (the screen-space reflections' place in the frame, diagnostic: SsrOrder.h)
  if (util::DiagnosticsOn())
    ssrorder::NotePass(from, to);
  if (to == PassMain && from != PassMain)
  {
    // A G-buffer pass begins: the frame's first gives its targets and depth
    if (++g_MainPasses == 1)
    {
      for (int i = 0; i < Copied; ++i)
      {
        g_Source[i].Reset();
        ComPtr<ID3D11Resource> resource;
        if (targets && i < (int)count && targets[i])
        {
          targets[i]->GetResource(resource.GetAddressOf());
          if (resource)
            resource.As(&g_Source[i]);
        }
      }
      if (depth)
      {
        ComPtr<ID3D11Resource> resource;
        ComPtr<ID3D11Texture2D> texture;
        depth->GetResource(resource.GetAddressOf());
        if (resource && SUCCEEDED(resource.As(&texture)) && texture)
        {
          __int64 id = (__int64)texture.Get();
          bool known = false;
          for (auto const& seen : g_SceneDepths)
            known = known || seen.load(std::memory_order_relaxed) == id;
          for (__int64 forgotten : g_Forgotten)
            known = known || forgotten == id;
          if (!known)
          {
            D3D11_TEXTURE2D_DESC d{};
            texture->GetDesc(&d);
            if (g_LoggedDepths < 16)
            {
              ++g_LoggedDepths;
              util::log::Write("Snow: the game's G-buffer pass draws with the depth target 0x%I64X (%u x %u, format %d): the "
                "scene's depth is taken from the G-buffer pass's targets only", id, d.Width, d.Height, (int)d.Format);
            }
            g_SceneDepths[g_SceneDepthNext].store(id, std::memory_order_relaxed);
            g_SceneDepthNext = (g_SceneDepthNext + 1) % SceneDepths;
          }
        }
      }
    }
    else if (!g_LoggedSeveral && util::DiagnosticsOn())
    {
      g_LoggedSeveral = true;
      util::log::Write("Snow (diagnostic): the game drew a second G-buffer pass in a frame (the frame's first is the one used)");
    }
  }
  // The frame's first G-buffer pass ends: the view constants it drew with,
  // copied while they're still bound (the view's own, the same all frame)
  bool leaving = (from == PassMain || from == PassDecal) && to != PassMain && to != PassDecal;
  if (leaving && g_MainPasses >= 1 && g_Wanted && !g_ViewCopied)
    g_ViewCopied = CopyViewConstants(pContext);
  // The frame's first lit pass begins: the G-buffer is whole (its decals and
  // the two G-buffer passes the game draws after them, CT.log's "Screen-space
  // reflections (diagnostic)" of 2026-11-02 21:36), its targets copied. They
  // were copied as the first G-buffer pass ended, before all of those, so the
  // copy had no decals and what the later passes draw had what was under it
  // (the SSR session's finding, 2026-11-02).
  if (to == PassLit && from != PassLit && g_MainPasses >= 1 && !g_LitSeen)
  {
    g_LitSeen = true;
    if (g_Wanted && g_ViewCopied)
    {
      bool ok = true;
      int copies = g_WantAll ? Copied : 2;
      for (int i = 0; i < copies && ok; ++i)
        ok = CopyTarget(pContext, i);
      g_CopiedAll = ok && copies == Copied;
      g_Copied = ok;
      if (ok && !g_LoggedCopied)
      {
        g_LoggedCopied = true;
        util::log::Write("Snow: the game's G-buffer copied for the snow and the wet look as its first lit pass began (%u x %u; "
          "formats %d, %d, %d)", g_CopyDesc[0].Width, g_CopyDesc[0].Height, (int)g_CopyDesc[0].Format,
          (int)g_CopyDesc[1].Format, (int)g_CopyDesc[2].Format);
      }
    }
  }
  // The wet ground written into the game's G-buffer (GbufferWet.h), after
  // the copies (its pass reads them)
  gbufferwet::OnPass(pContext, from, to, count, targets, depth);
  // Dogen's kitchen lamps' glow added into the G-buffer's target 3 at the same point, after the copies (targets 0 - 2
  // only), while Faith is near them (Camera/DogenGlow.h; the Dogen-door session, with the snow session's go-ahead)
  dogenglow::OnPass(pContext, from, to, count, targets, depth);
}

int gameframe::IsSceneDepth(__int64 texture)
{
  bool any = false;
  for (auto const& seen : g_SceneDepths)
  {
    __int64 id = seen.load(std::memory_order_relaxed);
    if (id && id == texture)
      return 1;
    any = any || id != 0;
  }
  return any ? 0 : -1;
}

void gameframe::ForgetSceneDepths()
{
  for (int i = 0; i < SceneDepths; ++i)
  {
    g_Forgotten[i] = g_SceneDepths[i].load(std::memory_order_relaxed);
    g_SceneDepths[i].store(0, std::memory_order_relaxed);
  }
}

bool gameframe::Take(Frame& out)
{
  out = Frame{};
  if (!g_Copied || !g_View)
    return false;
  out.Normals = g_CopyView[0].Get();
  out.Colour = g_CopyView[1].Get();
  // (the material's only when the last copy had it: an older one, from
  // before the rain alone, isn't this frame's; see Want's all)
  out.Material = g_CopiedAll ? g_CopyView[2].Get() : nullptr;
  out.View = g_View.Get();
  out.Width = g_CopyDesc[0].Width;
  out.Height = g_CopyDesc[0].Height;
  return out.Normals && out.Colour && (out.Material || !g_WantAll);
}

namespace
{
  // The camera check (see CheckCamera): the viewConstants copy's first 46
  // registers (crViewProjMatrix 14 - 17, one row each; cameraPos 45), read
  // back with the tools' cameras of the frame before, the frame and the
  // frame after
  const int CameraChecks = 4;
  const UINT CameraCheckBytes = 46 * 16;
  const ULONGLONG CameraCheckEveryMs = 500, CameraLogEveryMs = 10000;
  const int CameraCheckWait = 3;   // frames before it's read back
  struct CameraCheckSlot
  {
    ComPtr<ID3D11Buffer> Staging;
    bool Pending = false;
    int Waited = 0;
    float Transform[3][16] = {};
    float Projection[3][16] = {};
    bool Have[3] = {};
  };
  CameraCheckSlot g_CameraSlots[CameraChecks];
  float g_LastCameraT[16] = {}, g_LastCameraP[16] = {};
  bool g_HaveLastCamera = false;
  ULONGLONG g_CameraQueuedAt = 0, g_CameraLoggedAt = 0;
  // over the last 10 s: per candidate (0 the frame before's, 1 the frame's,
  // 2 the frame after's) the pixel error's sum, most and count; how often
  // each was the closest; the camera's place's difference (the frame's)
  double g_CameraErrSum[3] = {};
  float g_CameraErrMost[3] = {};
  int g_CameraErrCount[3] = {}, g_CameraBest[3] = {}, g_CameraChecked = 0;
  double g_CameraPlaceSum = 0.0;
  float g_CameraPlaceMost = 0.f;
  bool g_CameraFailedLogged = false;

  // Where the game's camera (view: its registers) draws 9 points 20 m ahead of
  // the tools' camera (t, p) against where the tools' does (ndc at +-0.8, 0),
  // in pixels at most; -1 if one is behind it
  float CameraError(float const* view, float const* t, float const* p, float width, float height)
  {
    float const* place = view + 45 * 4;
    float most = 0.f;
    for (int i = 0; i < 9; ++i)
    {
      float x = 0.8f * (float)(i % 3 - 1), y = 0.8f * (float)(i / 3 - 1);
      float vz = -20.f;
      float w = vz * p[11] + p[15];
      if (p[0] == 0.f || p[5] == 0.f)
        return -1.f;
      float vx = (x * w - vz * p[8] - p[12]) / p[0], vy = (y * w - vz * p[9] - p[13]) / p[5];
      float rel[4];
      for (int k = 0; k < 3; ++k)
        rel[k] = t[12 + k] + vx * t[k] + vy * t[4 + k] + vz * t[8 + k] - place[k];
      rel[3] = 1.f;
      float clip[4];
      for (int r = 0; r < 4; ++r)
      {
        float const* row = view + (14 + r) * 4;
        clip[r] = rel[0] * row[0] + rel[1] * row[1] + rel[2] * row[2] + rel[3] * row[3];
      }
      if (!(clip[3] > 1e-4f))
        return -1.f;
      float dx = (clip[0] / clip[3] - x) * width * 0.5f, dy = (clip[1] / clip[3] - y) * height * 0.5f;
      float e = sqrtf(dx * dx + dy * dy);
      if (!std::isfinite(e))
        return -1.f;
      most = (std::max)(most, e);
    }
    return most;
  }
}

void gameframe::CheckCamera(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, float const* transform, float const* projection,
  float width, float height)
{
  if (!pDevice || !pContext || !transform || !projection)
    return;
  // The ones read back a frame ago get this frame's camera as their "after"
  for (CameraCheckSlot& s : g_CameraSlots)
    if (s.Pending && !s.Have[2])
    {
      memcpy(s.Transform[2], transform, sizeof(s.Transform[2]));
      memcpy(s.Projection[2], projection, sizeof(s.Projection[2]));
      s.Have[2] = true;
    }
  // Read back what's there (not waited on: a copy not done yet is tried again)
  for (CameraCheckSlot& s : g_CameraSlots)
  {
    if (!s.Pending || ++s.Waited < CameraCheckWait || !s.Have[2])
      continue;
    D3D11_MAPPED_SUBRESOURCE m{};
    HRESULT hr = pContext->Map(s.Staging.Get(), 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &m);
    if (hr == DXGI_ERROR_WAS_STILL_DRAWING)
      continue;
    s.Pending = false;
    if (FAILED(hr))
      continue;
    float view[46 * 4];
    memcpy(view, m.pData, sizeof(view));
    pContext->Unmap(s.Staging.Get(), 0);
    float err[3] = { -1.f, -1.f, -1.f };
    int best = -1;
    for (int c = 0; c < 3; ++c)
    {
      if (!s.Have[c])
        continue;
      err[c] = CameraError(view, s.Transform[c], s.Projection[c], width, height);
      if (err[c] < 0.f)
        continue;
      g_CameraErrSum[c] += err[c];
      g_CameraErrMost[c] = (std::max)(g_CameraErrMost[c], err[c]);
      ++g_CameraErrCount[c];
      if (best < 0 || err[c] < err[best])
        best = c;
    }
    if (best >= 0)
      ++g_CameraBest[best];
    float const* place = view + 45 * 4;
    float dx = s.Transform[1][12] - place[0], dy = s.Transform[1][13] - place[1], dz = s.Transform[1][14] - place[2];
    float off = sqrtf(dx * dx + dy * dy + dz * dz);
    if (std::isfinite(off))
    {
      g_CameraPlaceSum += off;
      g_CameraPlaceMost = (std::max)(g_CameraPlaceMost, off);
    }
    ++g_CameraChecked;
  }
  // A new one, twice a second, of this frame's copy
  ULONGLONG now = GetTickCount64();
  if (g_Copied && g_View && now - g_CameraQueuedAt >= CameraCheckEveryMs)
  {
    for (CameraCheckSlot& s : g_CameraSlots)
    {
      if (s.Pending)
        continue;
      if (!s.Staging)
      {
        D3D11_BUFFER_DESC d{};
        d.ByteWidth = CameraCheckBytes;
        d.Usage = D3D11_USAGE_STAGING;
        d.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        HRESULT hr = pDevice->CreateBuffer(&d, nullptr, s.Staging.GetAddressOf());
        if (FAILED(hr))
        {
          if (!g_CameraFailedLogged)
          {
            g_CameraFailedLogged = true;
            util::log::Warning("Wet surfaces (diagnostic): the camera check's read-back buffer couldn't be made (HRESULT 0x%X)", hr);
          }
          break;
        }
      }
      D3D11_BOX box{ 0, 0, 0, CameraCheckBytes, 1, 1 };
      pContext->CopySubresourceRegion(s.Staging.Get(), 0, 0, 0, 0, g_View.Get(), 0, &box);
      s.Have[0] = g_HaveLastCamera;
      if (g_HaveLastCamera)
      {
        memcpy(s.Transform[0], g_LastCameraT, sizeof(g_LastCameraT));
        memcpy(s.Projection[0], g_LastCameraP, sizeof(g_LastCameraP));
      }
      memcpy(s.Transform[1], transform, sizeof(s.Transform[1]));
      memcpy(s.Projection[1], projection, sizeof(s.Projection[1]));
      s.Have[1] = true;
      s.Have[2] = false;
      s.Waited = 0;
      s.Pending = true;
      g_CameraQueuedAt = now;
      break;
    }
  }
  memcpy(g_LastCameraT, transform, sizeof(g_LastCameraT));
  memcpy(g_LastCameraP, projection, sizeof(g_LastCameraP));
  g_HaveLastCamera = true;
  if (now - g_CameraLoggedAt >= CameraLogEveryMs && g_CameraChecked > 0)
  {
    auto mean = [](int c) { return g_CameraErrCount[c] ? (float)(g_CameraErrSum[c] / g_CameraErrCount[c]) : -1.f; };
    util::log::Write("Wet surfaces (diagnostic): the tools' camera against the game's own of the frame (its viewConstants), %d "
      "checks in 10 s: a point 20 m ahead drawn %.2f px off on average (most %.2f) with the frame's camera, %.2f (%.2f) with "
      "the frame before's, %.2f (%.2f) with the frame after's; the closest: the frame's %d times, the frame before's %d, "
      "the frame after's %d; the camera's place %.3f m off on average (most %.3f)", g_CameraChecked, mean(1),
      g_CameraErrMost[1], mean(0), g_CameraErrMost[0], mean(2), g_CameraErrMost[2], g_CameraBest[1], g_CameraBest[0],
      g_CameraBest[2], g_CameraPlaceSum / g_CameraChecked, g_CameraPlaceMost);
    g_CameraLoggedAt = now;
    for (int c = 0; c < 3; ++c)
    {
      g_CameraErrSum[c] = 0.0;
      g_CameraErrMost[c] = 0.f;
      g_CameraErrCount[c] = g_CameraBest[c] = 0;
    }
    g_CameraChecked = 0;
    g_CameraPlaceSum = 0.0;
    g_CameraPlaceMost = 0.f;
  }
}

void gameframe::FrameEnd()
{
  // (Dogen's lamps' glow: its frame's targets let go, Camera/DogenGlow.h)
  dogenglow::FrameEnd();
  // No G-buffer pass seen for FramesWithoutGbuffer frames (a loading screen,
  // a menu, or its targets made again at another size and not told apart
  // yet): the scene's depth is taken as before until one is seen again, so
  // the rain and snow never wait on targets that are gone
  if (g_Hooked && g_MainPasses == 0)
  {
    bool known = false;
    for (auto const& seen : g_SceneDepths)
      known = known || seen.load(std::memory_order_relaxed) != 0;
    if (++g_FramesWithout == FramesWithoutGbuffer && known)
    {
      for (auto& seen : g_SceneDepths)
        seen.store(0, std::memory_order_relaxed);
      if (g_LoggedForgot < 4)
      {
        ++g_LoggedForgot;
        util::log::Write("Snow: no G-buffer pass for %d frames (a loading screen or a menu): the scene's depth taken the old way "
          "until one is seen again", FramesWithoutGbuffer);
      }
    }
  }
  else
    g_FramesWithout = 0;
  // (a G-buffer pass with no lit pass after it: no copies that frame)
  if (g_Hooked && g_MainPasses > 0 && !g_LitSeen && g_Wanted && !g_LoggedNoLit && util::DiagnosticsOn())
  {
    g_LoggedNoLit = true;
    util::log::Write("Snow (diagnostic): a frame had a G-buffer pass and no lit pass after it: the G-buffer wasn't copied in it");
  }
  g_Copied = false;
  g_ViewCopied = false;
  g_LitSeen = false;
  g_MainPasses = 0;
  // (the wet ground in the game's G-buffer: its frame over too)
  gbufferwet::FrameEnd();
  for (auto& source : g_Source)
    source.Reset();
}
