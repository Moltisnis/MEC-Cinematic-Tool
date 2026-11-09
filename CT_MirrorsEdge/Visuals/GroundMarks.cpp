#include "GroundMarks.h"
#include "D3DStateBackup.h"
#include "../Util/Util.h"
#include "GroundMarksVs.h"
#include "GroundMarksPs.h"
#include "GroundMarksFillVs.h"
#include "GroundMarksFillPs.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <d3d11_1.h>
#include <mutex>
#include <vector>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace
{
  const int MostStamps = 256;          // a draw's (GroundMarks.hlsl's)
  const size_t MostQueued = 8192;      // queued between frames (the oldest let go past it)
  const int MoveTexels = 32;           // the window moves once the camera has gone this far (about 1 m)
  // Fills smaller than this wait (summed): a 16-bit texel takes whole steps
  const float FillStep = 2.f / 65535.f;
  // Its marks kept to this far from the window's middle (half the map less a
  // margin, so a stamp's quad never reaches round to the other side)
  const float ReachMargin = 1.f;

  struct Queued
  {
    double A[2], B[2];   // world x, z: a step's place (A), a wheel's start and end
    float Y;             // the ground's height there (world)
    float Way[2];        // a step's way
    float HalfWidth, HalfLength;
    float Carried;       // a wheel's water out of a puddle (its wet track, in the rain)
    bool Step;
  };

  struct Constants
  {
    float Map[4];
    float Fill[4];
    float StampA[MostStamps][4];
    float StampB[MostStamps][4];
    float StampC[MostStamps][4];
  };

  // Her shoe's sole's size for the crowd's steps: 27 x 10 cm (the rain
  // session's ShoeSole, WetLook.hlsl; Faith's own measured 24.6 x 8.8)
  const float StepHalfLength = 0.135f, StepHalfWidth = 0.05f;

  std::mutex g_QueueMutex;
  std::vector<Queued> g_Queue;   // under g_QueueMutex

  // Render thread
  bool g_Made = false, g_Failed = false;
  ComPtr<ID3D11Texture2D> g_Map;
  ComPtr<ID3D11RenderTargetView> g_MapTarget;
  ComPtr<ID3D11ShaderResourceView> g_MapView;
  ComPtr<ID3D11Buffer> g_Constants;
  ComPtr<ID3D11VertexShader> g_StampVs, g_FillVs;
  ComPtr<ID3D11PixelShader> g_StampPs, g_FillPs;
  ComPtr<ID3D11BlendState> g_MaxBlend, g_SubtractBlend;
  ComPtr<ID3D11RasterizerState> g_Rasterizer;
  ComPtr<ID3D11DepthStencilState> g_NoDepth;
  ComPtr<ID3D11DeviceContext1> g_Context1;
  D3DStateBackup g_Backup;   // big: its class instance lists
  bool g_On = false;                   // drawn last frame
  bool g_OnSnow = false;               // and red the snow's then (else the wet tracks')
  bool g_WindowKnown = false;
  long long g_Window[2] = {};          // the window's corner (lowest world texel, x and z)
  float g_HeightLow = 0.f;             // the height alpha 0 stands for (GroundMarks.h's MarksHeightRange)
  float g_PendingRed = 0.f, g_PendingWater = 0.f;
  bool g_LoggedMade = false;

  bool Make(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
  {
    g_Made = true;
    D3D11_TEXTURE2D_DESC map{};
    map.Width = map.Height = (UINT)groundmarks::MarksTexels;
    map.MipLevels = map.ArraySize = 1;
    map.Format = DXGI_FORMAT_R16G16B16A16_UNORM;
    map.SampleDesc.Count = 1;
    map.Usage = D3D11_USAGE_DEFAULT;
    map.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    D3D11_BUFFER_DESC constants{};
    constants.ByteWidth = sizeof(Constants);
    constants.Usage = D3D11_USAGE_DYNAMIC;
    constants.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    constants.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    // MAX for the stamps' red and green (a mark never shallower for another
    // over it), their height (alpha) the last one's (only where a mark is
    // left: GroundMarks.hlsl's StampPs); the fill subtracted from
    // red and green (what's there less the fill), the height left
    D3D11_BLEND_DESC maxBlend{};
    maxBlend.RenderTarget[0].BlendEnable = TRUE;
    maxBlend.RenderTarget[0].SrcBlend = maxBlend.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
    maxBlend.RenderTarget[0].BlendOp = D3D11_BLEND_OP_MAX;
    maxBlend.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    maxBlend.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    maxBlend.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    maxBlend.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN |
      D3D11_COLOR_WRITE_ENABLE_ALPHA;
    D3D11_BLEND_DESC subtract = maxBlend;
    subtract.RenderTarget[0].BlendOp = subtract.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_REV_SUBTRACT;
    subtract.RenderTarget[0].SrcBlendAlpha = subtract.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
    subtract.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN;
    D3D11_RASTERIZER_DESC rasterizer{};
    rasterizer.FillMode = D3D11_FILL_SOLID;
    rasterizer.CullMode = D3D11_CULL_NONE;
    rasterizer.DepthClipEnable = TRUE;
    D3D11_DEPTH_STENCIL_DESC depth{};
    depth.DepthEnable = FALSE;
    depth.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    depth.DepthFunc = D3D11_COMPARISON_ALWAYS;
    HRESULT hr = pDevice->CreateTexture2D(&map, nullptr, g_Map.GetAddressOf());
    if (SUCCEEDED(hr)) hr = pDevice->CreateRenderTargetView(g_Map.Get(), nullptr, g_MapTarget.GetAddressOf());
    if (SUCCEEDED(hr)) hr = pDevice->CreateShaderResourceView(g_Map.Get(), nullptr, g_MapView.GetAddressOf());
    if (SUCCEEDED(hr)) hr = pDevice->CreateBuffer(&constants, nullptr, g_Constants.GetAddressOf());
    if (SUCCEEDED(hr)) hr = pDevice->CreateVertexShader(g_GroundMarksVs, sizeof(g_GroundMarksVs), nullptr, g_StampVs.GetAddressOf());
    if (SUCCEEDED(hr)) hr = pDevice->CreatePixelShader(g_GroundMarksPs, sizeof(g_GroundMarksPs), nullptr, g_StampPs.GetAddressOf());
    if (SUCCEEDED(hr)) hr = pDevice->CreateVertexShader(g_GroundMarksFillVs, sizeof(g_GroundMarksFillVs), nullptr, g_FillVs.GetAddressOf());
    if (SUCCEEDED(hr)) hr = pDevice->CreatePixelShader(g_GroundMarksFillPs, sizeof(g_GroundMarksFillPs), nullptr, g_FillPs.GetAddressOf());
    if (SUCCEEDED(hr)) hr = pDevice->CreateBlendState(&maxBlend, g_MaxBlend.GetAddressOf());
    if (SUCCEEDED(hr)) hr = pDevice->CreateBlendState(&subtract, g_SubtractBlend.GetAddressOf());
    if (SUCCEEDED(hr)) hr = pDevice->CreateRasterizerState(&rasterizer, g_Rasterizer.GetAddressOf());
    if (SUCCEEDED(hr)) hr = pDevice->CreateDepthStencilState(&depth, g_NoDepth.GetAddressOf());
    if (SUCCEEDED(hr) && FAILED(pContext->QueryInterface(__uuidof(ID3D11DeviceContext1), (void**)g_Context1.GetAddressOf())))
      hr = E_NOINTERFACE;
    if (FAILED(hr))
    {
      g_Failed = true;
      util::log::Warning("Ground marks: the map couldn't be made (HRESULT 0x%X): the crowd's and the traffic's steps and "
        "wheels leave no prints in the snow nor trails in the water", hr);
      return false;
    }
    return true;
  }

  long long TexelOf(double metres)
  {
    return (long long)floor(metres / (double)groundmarks::MarksTexel);
  }

  // A world texel's place in the map
  float PlaceOf(double metres)
  {
    long long n = groundmarks::MarksTexels;
    double t = metres / (double)groundmarks::MarksTexel;
    long long whole = (long long)floor(t / (double)n);
    return (float)(t - (double)(whole * n));
  }

  // The world texels [from, to) of one axis cleared, over all of the other
  // (rects in the map's own texels, round its edge)
  void ClearBand(bool alongX, long long from, long long to)
  {
    long long n = groundmarks::MarksTexels;
    if (to - from >= n)
    {
      from = 0;
      to = n;
    }
    float zero[4] = { 0.f, 0.f, 0.f, 0.f };
    D3D11_RECT rects[2];
    int count = 0;
    long long a = ((from % n) + n) % n, length = to - from;
    long long first = (std::min)(length, n - a);
    rects[count++] = alongX ? D3D11_RECT{ (LONG)a, 0, (LONG)(a + first), (LONG)n } :
      D3D11_RECT{ 0, (LONG)a, (LONG)n, (LONG)(a + first) };
    if (length > first)
      rects[count++] = alongX ? D3D11_RECT{ 0, 0, (LONG)(length - first), (LONG)n } :
        D3D11_RECT{ 0, 0, (LONG)n, (LONG)(length - first) };
    g_Context1->ClearView(g_MapTarget.Get(), zero, rects, count);
  }
}

void groundmarks::Step(float const* at, float const* way)
{
  Queued q{};
  q.A[0] = at[0];
  q.A[1] = at[2];
  q.Y = at[1];
  q.Way[0] = way[0];
  q.Way[1] = way[1];
  q.HalfLength = StepHalfLength;
  q.HalfWidth = StepHalfWidth;
  q.Step = true;
  std::lock_guard<std::mutex> lock(g_QueueMutex);
  if (g_Queue.size() < MostQueued)
    g_Queue.push_back(q);
}

void groundmarks::Wheel(float const* from, float const* to, float halfWidth, float carried)
{
  Queued q{};
  q.A[0] = from[0];
  q.A[1] = from[2];
  q.B[0] = to[0];
  q.B[1] = to[2];
  q.Y = (from[1] + to[1]) * 0.5f;
  q.HalfWidth = halfWidth;
  q.Carried = carried;
  std::lock_guard<std::mutex> lock(g_QueueMutex);
  if (g_Queue.size() < MostQueued)
    g_Queue.push_back(q);
}

bool groundmarks::Update(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, float const* camera, bool snow, bool water,
  float redFill, float waterFill, Map& out)
{
  out = Map{};
  std::vector<Queued> queued;
  {
    std::lock_guard<std::mutex> lock(g_QueueMutex);
    queued.swap(g_Queue);
  }
  bool on = (snow || water) && pDevice && pContext && camera && !g_Failed;
  if (!on)
  {
    g_On = false;
    return false;
  }
  if (!g_Made && !Make(pDevice, pContext))
    return false;
  const long long n = MarksTexels;
  long long cam[2] = { TexelOf(camera[0]), TexelOf(camera[2]) };
  // (on again after a while off, or from snow to rain or back, or the camera
  // gone MarksHeightDrift up or down: what was there before cleared; red is
  // the snow's or the wet tracks', the heights from the camera's now)
  if (!g_On || !g_WindowKnown || g_OnSnow != snow ||
    fabsf(camera[1] - (g_HeightLow + MarksHeightRange * 0.5f)) > MarksHeightDrift)
  {
    float zero[4] = { 0.f, 0.f, 0.f, 0.f };
    pContext->ClearRenderTargetView(g_MapTarget.Get(), zero);
    g_Window[0] = cam[0] - n / 2;
    g_Window[1] = cam[1] - n / 2;
    g_HeightLow = camera[1] - MarksHeightRange * 0.5f;
    g_WindowKnown = true;
    g_PendingRed = g_PendingWater = 0.f;
    if (!g_LoggedMade)
    {
      g_LoggedMade = true;
      util::log::Write("Ground marks: the map round the camera made (%d x %d texels of %.0f mm, %.0f m across): the crowd's "
        "and the traffic's steps and wheels leave prints in the snow and trails in the water", MarksTexels, MarksTexels,
        MarksTexel * 1000.f, MarksTexels * MarksTexel);
    }
  }
  g_On = true;
  g_OnSnow = snow;
  // The window moved with the camera: the strips coming in cleared
  for (int axis = 0; axis < 2; ++axis)
  {
    long long want = cam[axis] - n / 2;
    long long moved = want - g_Window[axis];
    if (moved >= MoveTexels || moved <= -MoveTexels)
    {
      if (moved > 0)
        ClearBand(axis == 0, g_Window[axis] + n, want + n);
      else
        ClearBand(axis == 0, want, g_Window[axis]);
      g_Window[axis] = want;
    }
  }
  double middle[2] = { (double)(g_Window[0] + n / 2) * MarksTexel, (double)(g_Window[1] + n / 2) * MarksTexel };
  float reach = (float)(n / 2) * MarksTexel - ReachMargin;
  g_PendingRed += redFill;
  g_PendingWater += water ? waterFill : 0.f;
  bool fill = g_PendingRed >= FillStep || g_PendingWater >= FillStep;
  // Its stamps: those whose place is within reach, and their height within
  // the range alpha holds
  std::vector<Queued> stamps;
  stamps.reserve(queued.size());
  for (Queued const& q : queued)
  {
    double dx = q.A[0] - middle[0], dz = q.A[1] - middle[1];
    float height = q.Y - g_HeightLow;
    if (fabs(dx) < reach && fabs(dz) < reach && height >= 0.f && height <= MarksHeightRange)
      stamps.push_back(q);
  }
  if (!stamps.empty() || fill)
  {
    g_Backup.Save(pContext);
    D3D11_VIEWPORT viewport{ 0.f, 0.f, (float)n, (float)n, 0.f, 1.f };
    const float blendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
    pContext->OMSetRenderTargets(1, g_MapTarget.GetAddressOf(), nullptr);
    pContext->RSSetViewports(1, &viewport);
    pContext->RSSetState(g_Rasterizer.Get());
    pContext->OMSetDepthStencilState(g_NoDepth.Get(), 0);
    pContext->IASetInputLayout(nullptr);
    pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    pContext->GSSetShader(nullptr, nullptr, 0);
    pContext->HSSetShader(nullptr, nullptr, 0);
    pContext->DSSetShader(nullptr, nullptr, 0);
    pContext->VSSetConstantBuffers(0, 1, g_Constants.GetAddressOf());
    pContext->PSSetConstantBuffers(0, 1, g_Constants.GetAddressOf());
    // The fill first (the frame's own marks then whole)
    if (fill)
    {
      D3D11_MAPPED_SUBRESOURCE mapped;
      if (SUCCEEDED(pContext->Map(g_Constants.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
      {
        Constants* c = (Constants*)mapped.pData;
        memset(c, 0, offsetof(Constants, StampA));
        c->Fill[0] = (std::min)(g_PendingRed, 1.f);
        c->Fill[1] = (std::min)(g_PendingWater, 1.f);
        pContext->Unmap(g_Constants.Get(), 0);
        g_PendingRed = g_PendingWater = 0.f;
        pContext->OMSetBlendState(g_SubtractBlend.Get(), blendFactor, 0xFFFFFFFF);
        pContext->VSSetShader(g_FillVs.Get(), nullptr, 0);
        pContext->PSSetShader(g_FillPs.Get(), nullptr, 0);
        pContext->Draw(3, 0);
      }
    }
    // The stamps, MostStamps a draw, each a quad four times (GroundMarks.hlsl)
    if (!stamps.empty())
    {
      pContext->OMSetBlendState(g_MaxBlend.Get(), blendFactor, 0xFFFFFFFF);
      pContext->VSSetShader(g_StampVs.Get(), nullptr, 0);
      pContext->PSSetShader(g_StampPs.Get(), nullptr, 0);
      for (size_t first = 0; first < stamps.size(); first += MostStamps)
      {
        size_t count = (std::min)(stamps.size() - first, (size_t)MostStamps);
        D3D11_MAPPED_SUBRESOURCE mapped;
        if (FAILED(pContext->Map(g_Constants.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
          break;
        Constants* c = (Constants*)mapped.pData;
        c->Map[0] = (float)n;
        c->Map[1] = MarksTexel;
        c->Map[2] = snow || water ? 1.f : 0.f;
        c->Map[3] = water ? 1.f : 0.f;
        c->Fill[0] = c->Fill[1] = c->Fill[2] = c->Fill[3] = 0.f;
        for (size_t i = 0; i < count; ++i)
        {
          Queued const& q = stamps[first + i];
          float* a = c->StampA[i];
          float* b = c->StampB[i];
          float* w = c->StampC[i];
          a[0] = PlaceOf(q.A[0]);
          a[1] = PlaceOf(q.A[1]);
          // (a wheel's end in the same copy of the map as its start)
          a[2] = a[0] + (float)((q.B[0] - q.A[0]) / (double)MarksTexel);
          a[3] = a[1] + (float)((q.B[1] - q.A[1]) / (double)MarksTexel);
          b[0] = q.HalfWidth;
          b[1] = q.HalfLength;
          b[2] = q.Step ? 1.f : 0.f;
          // (red: pressed all the way in the snow; in the rain a wheel's wet
          // track as wet as the water it carries, a step none)
          b[3] = snow ? 1.f : q.Step ? 0.f : (std::max)(0.f, (std::min)(q.Carried, 1.f));
          w[0] = q.Way[0];
          w[1] = q.Way[1];
          w[2] = 1.f;
          w[3] = (std::max)(0.f, (std::min)((q.Y - g_HeightLow) / MarksHeightRange, 1.f));
        }
        pContext->Unmap(g_Constants.Get(), 0);
        pContext->DrawInstanced(6, (UINT)count * 4, 0, 0);
      }
    }
    g_Backup.Restore(pContext);
  }
  out.View = g_MapView.Get();
  out.PerMetre = 1.f / ((float)n * MarksTexel);
  out.Middle[0] = (float)middle[0];
  out.Middle[1] = (float)middle[1];
  out.Reach = reach;
  out.HeightLow = g_HeightLow;
  return true;
}

namespace
{
  // Agents' records in a CrowdBoidsComponent (AttachTargets.cpp's own)
  const int ProbeAgentsBeginOffset = 0x70, ProbeAgentsEndOffset = 0x78, ProbeAgentSize = 0x20;
  const int ProbeComponentBytes = 0x100;
  std::mutex g_ProbeMutex;
  int g_Probed = 0;                      // under g_ProbeMutex
  std::atomic<int> g_ProbedQuick{ 0 };   // the same, read without the lock (the traffic's every frame)
  __int64 g_ProbedSwarms[groundmarks::ProbedMembers] = {};
  int g_ProbedSwarmCount = 0;

  // Read-only, inside __try (no C++ objects here): the member's record and,
  // if asked, its component's first bytes
  bool ReadProbe(__int64 boids, int agent, unsigned int* record, unsigned long long* component, bool withComponent)
  {
    __try
    {
      __int64 begin = *(__int64 const*)(boids + ProbeAgentsBeginOffset);
      __int64 end = *(__int64 const*)(boids + ProbeAgentsEndOffset);
      if (!begin || agent < 0 || (__int64)(agent + 1) * ProbeAgentSize > end - begin)
        return false;
      memcpy(record, (void const*)(begin + (__int64)agent * ProbeAgentSize), ProbeAgentSize);
      if (withComponent)
        memcpy(component, (void const*)boids, ProbeComponentBytes);
      return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
  }
}

void groundmarks::ProbeTraffic(__int64 boids, int agent)
{
  if (boids < 0x10000 || g_ProbedQuick.load(std::memory_order_relaxed) >= ProbedMembers)
    return;
  bool withComponent;
  {
    std::lock_guard<std::mutex> lock(g_ProbeMutex);
    if (g_Probed >= ProbedMembers)
      return;
    withComponent = true;
    for (int i = 0; i < g_ProbedSwarmCount; ++i)
      withComponent = withComponent && g_ProbedSwarms[i] != boids;
    g_ProbedQuick.store(++g_Probed, std::memory_order_relaxed);
    if (withComponent && g_ProbedSwarmCount < ProbedMembers)
      g_ProbedSwarms[g_ProbedSwarmCount++] = boids;
  }
  unsigned int record[ProbeAgentSize / 4] = {};
  unsigned long long component[ProbeComponentBytes / 8] = {};
  if (!ReadProbe(boids, agent, record, component, withComponent))
    return;
  float f[ProbeAgentSize / 4];
  memcpy(f, record, sizeof(f));
  util::log::Write("Ground marks (diagnostic): road traffic member %d of 0x%I64X: record %08X %08X %08X %08X %08X %08X %08X %08X "
    "(as floats %.2f %.2f %.2f [%g] %.2f %.2f %.2f [%g])", agent, boids, record[0], record[1], record[2], record[3], record[4],
    record[5], record[6], record[7], f[0], f[1], f[2], f[3], f[4], f[5], f[6], f[7]);
  if (withComponent)
    for (int row = 0; row < ProbeComponentBytes / 8; row += 4)
      util::log::Write("Ground marks (diagnostic):   its CrowdBoidsComponent + 0x%02X: %016I64X %016I64X %016I64X %016I64X",
        row * 8, component[row], component[row + 1], component[row + 2], component[row + 3]);
}
