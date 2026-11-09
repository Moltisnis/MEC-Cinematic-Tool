#include "Perf.h"
#include "Util.h"
#include "ImGuiEXT.h"
#include "../imgui/imgui.h"
#include "../Camera/RoadTraffic.h"   // the crowd test (road traffic session's)
#include <Windows.h>
#include <algorithm>
#include <atomic>
#include <string>
#include <vector>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace
{
  const char* const SectionNames[perf::SectionCount] = { "camera hook", "visual environment mix", "depth copy",
    "drawing passes", "menu" };
  const char* const GpuNames[2] = { "drawing passes", "menu" };
  const float PhaseSeconds = 10.f;
  const int Phases = 3;
  const int GpuRing = 6;   // query sets per kind, read back a few frames later

  long long Ticks()
  {
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return t.QuadPart;
  }
  double TicksPerMs()
  {
    static double perMs = 0.0;
    if (perMs == 0.0)
    {
      LARGE_INTEGER f;
      QueryPerformanceFrequency(&f);
      perMs = (double)f.QuadPart / 1000.0;
    }
    return perMs;
  }

  // CPU time per section, any thread
  std::atomic<long long> g_SectionTicks[perf::SectionCount];

  // GPU timing, render thread only
  struct GpuQuery
  {
    ComPtr<ID3D11Query> Disjoint, Begin, End;
    bool Pending = false;
  };
  GpuQuery g_Gpu[2][GpuRing];
  int g_GpuNext[2] = {};
  int g_GpuOpen[2] = { -1, -1 };
  bool g_GpuMade = false, g_GpuFailed = false;
  double g_GpuMs[2] = {};        // read back since the last snapshot
  int g_GpuCount[2] = {};

  // Frames and the running numbers, render thread only
  long long g_LastFrame = 0;
  struct Window
  {
    std::vector<float> FrameMs;
    long long SectionStart[perf::SectionCount] = {};
    // (where each section's time was when it ended: a test's phase; one not
    // ended is measured up to now)
    long long SectionEnd[perf::SectionCount] = {};
    bool Ended = false;
    double GpuMs[2] = {};
    int GpuCount[2] = {};
    long long Start = 0;
  };
  Window g_Live;                 // the last second, for the menu's line
  std::string g_LiveText;

  // The test, render thread only
  int g_Phase = -1;              // -1 not running; 0, 1, 2
  Window g_PhaseWindow[Phases];
  std::string g_Result;

  // CT.log's frame line every TenMs (2026-11-03, the user: "find a way to
  // improve performance in fps even with rain/snow enabled"): the frames, the
  // tools' CPU and GPU, and the game's own Present calls (render thread only)
  const double TenMs = 10000.0;
  Window g_Ten;
  long long g_PresentTicks = 0, g_PresentMost = 0;
  int g_PresentCalls = 0;
  DWORD g_FrameThread = 0;   // FrameStart's (a Present on another thread, an overlay's, isn't counted)

  // The camera hook's parts (LapStart, Lap, AddLap): its thread only
  struct LapPart { char const* Name; long long Ticks, Most, Frame; };
  const int MostLapParts = 48;
  LapPart g_Laps[MostLapParts];
  int g_LapParts = 0, g_LapFrames = 0;
  long long g_LapLast = 0, g_LapWindow = 0;
  DWORD g_LapThread = 0;     // LapStart's (a part timed on another thread isn't counted)

  LapPart* LapFor(char const* name)
  {
    for (int i = 0; i < g_LapParts; ++i)
      if (g_Laps[i].Name == name)
        return &g_Laps[i];
    if (g_LapParts >= MostLapParts)
      return nullptr;
    g_Laps[g_LapParts] = LapPart{ name, 0, 0, 0 };
    return &g_Laps[g_LapParts++];
  }

  // The parts' times a frame since the last line, the dearest first
  void LogLaps(double seconds)
  {
    if (g_LapFrames <= 0 || g_LapParts <= 0)
      return;
    LapPart sorted[MostLapParts];
    std::copy(g_Laps, g_Laps + g_LapParts, sorted);
    std::sort(sorted, sorted + g_LapParts, [](LapPart const& a, LapPart const& b) { return a.Ticks > b.Ticks; });
    double total = 0.0, under = 0.0;
    std::string parts;
    for (int i = 0; i < g_LapParts; ++i)
    {
      double ms = sorted[i].Ticks / TicksPerMs() / g_LapFrames;
      total += ms;
      if (ms < 0.002)
      {
        under += ms;
        continue;
      }
      char part[160];
      sprintf_s(part, "%s%s %.3f (most %.2f)", parts.empty() ? "" : "; ", sorted[i].Name, ms, sorted[i].Most / TicksPerMs());
      parts += part;
    }
    util::log::Write("Camera hook (performance, diagnostic): %d frames in %.0f s; the tools' work in it %.3f ms a frame: %s%s",
      g_LapFrames, seconds, total, parts.c_str(), under > 0.0 ? (std::string("; the rest, under 0.002 each: ") +
      std::to_string(under).substr(0, 5)).c_str() : "");
  }

  void Begin(Window& w)
  {
    w.FrameMs.clear();
    for (int s = 0; s < perf::SectionCount; ++s)
      w.SectionStart[s] = g_SectionTicks[s].load(std::memory_order_relaxed);
    w.GpuMs[0] = w.GpuMs[1] = 0.0;
    w.GpuCount[0] = w.GpuCount[1] = 0;
    w.Start = Ticks();
    w.Ended = false;
  }

  // A test's phase over: its sections' times up to here
  void End(Window& w)
  {
    for (int s = 0; s < perf::SectionCount; ++s)
      w.SectionEnd[s] = g_SectionTicks[s].load(std::memory_order_relaxed);
    w.Ended = true;
  }

  struct Numbers
  {
    int Frames = 0;
    float Avg = 0.f, P50 = 0.f, P95 = 0.f, P99 = 0.f, Max = 0.f;
    float Cpu[perf::SectionCount] = {};
    float CpuTotal = 0.f;
    float Gpu[2] = { -1.f, -1.f };   // -1 not measured
  };

  Numbers Measure(Window const& w)
  {
    Numbers n;
    n.Frames = (int)w.FrameMs.size();
    if (!n.Frames)
      return n;
    std::vector<float> sorted = w.FrameMs;
    std::sort(sorted.begin(), sorted.end());
    double sum = 0.0;
    for (float v : sorted)
      sum += v;
    n.Avg = (float)(sum / n.Frames);
    auto at = [&](float q) { return sorted[(std::min)((size_t)(q * (n.Frames - 1) + 0.5f), sorted.size() - 1)]; };
    n.P50 = at(0.5f);
    n.P95 = at(0.95f);
    n.P99 = at(0.99f);
    n.Max = sorted.back();
    for (int s = 0; s < perf::SectionCount; ++s)
    {
      long long d = (w.Ended ? w.SectionEnd[s] : g_SectionTicks[s].load(std::memory_order_relaxed)) - w.SectionStart[s];
      n.Cpu[s] = (float)(d / TicksPerMs() / n.Frames);
      n.CpuTotal += n.Cpu[s];
    }
    for (int k = 0; k < 2; ++k)
      if (w.GpuCount[k] > 0)
        n.Gpu[k] = (float)(w.GpuMs[k] / w.GpuCount[k]);
    return n;
  }

  std::string Describe(Numbers const& n)
  {
    char line[512];
    sprintf_s(line, "%d frames, %.1f fps; frame %.2f ms (median %.2f, 95 %% %.2f, 99 %% %.2f, longest %.2f); the tools' CPU %.3f ms "
      "a frame; their GPU: passes %s, menu %s", n.Frames, n.Avg > 0.f ? 1000.f / n.Avg : 0.f, n.Avg, n.P50, n.P95, n.P99,
      n.Max, n.CpuTotal, n.Gpu[0] >= 0.f ? (std::to_string(n.Gpu[0]).substr(0, 5) + " ms").c_str() : "none drawn",
      n.Gpu[1] >= 0.f ? (std::to_string(n.Gpu[1]).substr(0, 5) + " ms").c_str() : "none drawn");
    std::string s = line;
    s += "\n    CPU by part (ms a frame):";
    for (int k = 0; k < perf::SectionCount; ++k)
    {
      char part[96];
      sprintf_s(part, " %s %.3f;", SectionNames[k], n.Cpu[k]);
      s += part;
    }
    return s;
  }

  // The frame line (FrameStart, every TenMs)
  void LogTen(double seconds)
  {
    Numbers n = Measure(g_Ten);
    if (!n.Frames)
      return;
    char gpu[96];
    if (n.Gpu[0] >= 0.f || n.Gpu[1] >= 0.f)
      sprintf_s(gpu, "passes %.3f ms, menu %.3f ms a frame", n.Gpu[0] > 0.f ? n.Gpu[0] : 0.f, n.Gpu[1] > 0.f ? n.Gpu[1] : 0.f);
    else
      strcpy_s(gpu, "not measured");
    util::log::Write("Frame (performance, diagnostic): %d frames in %.0f s, %.1f fps; frame %.2f ms (median %.2f, 95 %% %.2f, "
      "99 %% %.2f, longest %.2f); the game's Present call %.2f ms a frame (most %.2f; long while the graphics card is behind "
      "and the game waits on it); the tools' CPU %.3f ms a frame (camera hook %.3f, visual environment mix %.3f, depth copy "
      "%.3f, drawing at Present %.3f, menu %.3f); their GPU at Present: %s (the wet ground in the game's lighting has its own "
      "line)", n.Frames, seconds, n.Avg > 0.f ? 1000.f / n.Avg : 0.f, n.Avg, n.P50, n.P95, n.P99, n.Max,
      g_PresentCalls ? g_PresentTicks / TicksPerMs() / g_PresentCalls : 0.0, g_PresentMost / TicksPerMs(), n.CpuTotal,
      n.Cpu[perf::SectionCameraHook], n.Cpu[perf::SectionVeMix], n.Cpu[perf::SectionDepthClear], n.Cpu[perf::SectionPasses],
      n.Cpu[perf::SectionMenu], gpu);
  }

  void MakeGpu(ID3D11Device* pDevice)
  {
    g_GpuMade = true;
    D3D11_QUERY_DESC disjoint{ D3D11_QUERY_TIMESTAMP_DISJOINT, 0 }, stamp{ D3D11_QUERY_TIMESTAMP, 0 };
    for (int k = 0; k < 2; ++k)
      for (int i = 0; i < GpuRing; ++i)
        if (FAILED(pDevice->CreateQuery(&disjoint, g_Gpu[k][i].Disjoint.GetAddressOf())) ||
            FAILED(pDevice->CreateQuery(&stamp, g_Gpu[k][i].Begin.GetAddressOf())) ||
            FAILED(pDevice->CreateQuery(&stamp, g_Gpu[k][i].End.GetAddressOf())))
        {
          g_GpuFailed = true;
          util::log::Warning("Performance: the GPU timing queries couldn't be made: the tools' GPU time isn't measured");
          return;
        }
  }

  // The finished queries read back, never waited for
  void ReadGpu(ID3D11DeviceContext* pContext)
  {
    for (int k = 0; k < 2; ++k)
      for (int i = 0; i < GpuRing; ++i)
      {
        GpuQuery& q = g_Gpu[k][i];
        if (!q.Pending)
          continue;
        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT dj{};
        UINT64 b = 0, e = 0;
        if (pContext->GetData(q.Disjoint.Get(), &dj, sizeof(dj), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK ||
            pContext->GetData(q.Begin.Get(), &b, sizeof(b), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK ||
            pContext->GetData(q.End.Get(), &e, sizeof(e), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK)
          continue;
        q.Pending = false;
        if (dj.Disjoint || !dj.Frequency || e < b)
          continue;
        double ms = (double)(e - b) * 1000.0 / (double)dj.Frequency;
        g_Live.GpuMs[k] += ms;
        ++g_Live.GpuCount[k];
        g_Ten.GpuMs[k] += ms;
        ++g_Ten.GpuCount[k];
        if (g_Phase >= 0)
        {
          g_PhaseWindow[g_Phase].GpuMs[k] += ms;
          ++g_PhaseWindow[g_Phase].GpuCount[k];
        }
      }
  }

  void FinishTest()
  {
    Numbers n[Phases];
    for (int p = 0; p < Phases; ++p)
      n[p] = Measure(g_PhaseWindow[p]);
    float withAvg = (n[0].Avg + n[2].Avg) * 0.5f;
    float drawing = withAvg - n[1].Avg;
    char summary[640];
    sprintf_s(summary, "With the tools' drawing the frame took %.2f ms on average (phases 1 and 3), with it paused %.2f ms: "
      "the drawing passes cost about %.2f ms a frame (%s). The tools' own CPU time was %.3f ms a frame (all parts, on the "
      "game's and the render threads). Phases 1 and 3 differ by %.2f ms, how much the frame time wanders by itself: "
      "smaller differences than that aren't the tools'.", withAvg, n[1].Avg, drawing,
      drawing > 0.f ? "slower with it" : "no slower with it", (n[0].CpuTotal + n[2].CpuTotal) * 0.5f, fabsf(n[0].Avg - n[2].Avg));
    g_Result = summary;
    util::log::Write("Performance test (MISC > Performance), %.0f s a phase:", PhaseSeconds);
    const char* phaseNames[Phases] = { "1, everything as set", "2, the tools' drawing passes paused", "3, everything as set" };
    for (int p = 0; p < Phases; ++p)
      util::log::Write("  Phase %s: %s", phaseNames[p], Describe(n[p]).c_str());
    util::log::Write("  %s", summary);
  }
}

perf::Scope::Scope(Section section) : m_Section(section), m_Start(Ticks())
{
}

perf::Scope::~Scope()
{
  g_SectionTicks[m_Section].fetch_add(Ticks() - m_Start, std::memory_order_relaxed);
}

void perf::AddTicks(Section section, long long ticks)
{
  g_SectionTicks[section].fetch_add(ticks, std::memory_order_relaxed);
}

long long perf::Now()
{
  return Ticks();
}

void perf::AddPresentTicks(long long ticks)
{
  if (GetCurrentThreadId() != g_FrameThread)
    return;
  g_PresentTicks += ticks;
  g_PresentMost = (std::max)(g_PresentMost, ticks);
  ++g_PresentCalls;
}

void perf::LapStart()
{
  long long now = Ticks();
  g_LapThread = GetCurrentThreadId();
  for (int i = 0; i < g_LapParts; ++i)
  {
    g_Laps[i].Most = (std::max)(g_Laps[i].Most, g_Laps[i].Frame);
    g_Laps[i].Frame = 0;
  }
  if (!g_LapWindow)
    g_LapWindow = now;
  else if ((now - g_LapWindow) / TicksPerMs() >= TenMs)
  {
    LogLaps((now - g_LapWindow) / TicksPerMs() / 1000.0);
    for (int i = 0; i < g_LapParts; ++i)
      g_Laps[i].Ticks = g_Laps[i].Most = 0;
    g_LapFrames = 0;
    g_LapWindow = now;
  }
  ++g_LapFrames;
  g_LapLast = now;
}

void perf::Lap(char const* part)
{
  if (GetCurrentThreadId() != g_LapThread)
    return;
  long long now = Ticks();
  AddLap(part, now - g_LapLast);
  g_LapLast = now;
}

void perf::AddLap(char const* part, long long ticks)
{
  if (GetCurrentThreadId() != g_LapThread)
    return;
  if (LapPart* p = LapFor(part))
  {
    p->Ticks += ticks;
    p->Frame += ticks;
  }
}

void perf::FrameStart(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
  long long now = Ticks();
  g_FrameThread = GetCurrentThreadId();
  if (!g_GpuMade && pDevice)
    MakeGpu(pDevice);
  if (!g_GpuFailed && pContext)
    ReadGpu(pContext);
  if (g_LastFrame)
  {
    float ms = (float)((now - g_LastFrame) / TicksPerMs());
    // A frame longer than a second is a pause (a loading screen, alt-tab), not a frame
    if (ms < 1000.f)
    {
      g_Live.FrameMs.push_back(ms);
      g_Ten.FrameMs.push_back(ms);
      if (g_Phase >= 0)
        g_PhaseWindow[g_Phase].FrameMs.push_back(ms);
      road_traffic::CrowdTestFrame(ms);   // the crowd test's frames (road traffic session's)
    }
  }
  g_LastFrame = now;
  // CT.log's frame line, every TenMs
  if (!g_Ten.Start)
    Begin(g_Ten);
  else if ((now - g_Ten.Start) / TicksPerMs() >= TenMs)
  {
    LogTen((now - g_Ten.Start) / TicksPerMs() / 1000.0);
    Begin(g_Ten);
    g_PresentTicks = g_PresentMost = 0;
    g_PresentCalls = 0;
  }
  // The menu's line, once a second
  if (!g_Live.Start)
    Begin(g_Live);
  else if ((now - g_Live.Start) / TicksPerMs() >= 1000.0)
  {
    Numbers n = Measure(g_Live);
    char line[256];
    sprintf_s(line, "%.0f fps (frame %.2f ms); the tools: CPU %.3f ms, GPU %s a frame", n.Avg > 0.f ? 1000.f / n.Avg : 0.f, n.Avg,
      n.CpuTotal, n.Gpu[0] >= 0.f || n.Gpu[1] >= 0.f ?
      (std::to_string((n.Gpu[0] > 0.f ? n.Gpu[0] : 0.f) + (n.Gpu[1] > 0.f ? n.Gpu[1] : 0.f)).substr(0, 5) + " ms").c_str() :
      "not measured yet");
    g_LiveText = line;
    Begin(g_Live);
  }
  // The test's phases
  if (g_Phase >= 0 && (now - g_PhaseWindow[g_Phase].Start) / TicksPerMs() >= PhaseSeconds * 1000.0)
  {
    // (until 2026-11-03 a phase's CPU was measured up to the test's end: the
    // first two phases' numbers were 3 and 2 times too much)
    End(g_PhaseWindow[g_Phase]);
    if (++g_Phase >= Phases)
    {
      g_Phase = -1;
      FinishTest();
    }
    else
    {
      Begin(g_PhaseWindow[g_Phase]);
      if (g_Phase == 1)
        util::log::Write("Performance test: phase 2, the tools' drawing passes paused");
      else
        util::log::Write("Performance test: phase 3, everything as set");
    }
  }
}

void perf::GpuBegin(ID3D11DeviceContext* pContext, int which)
{
  if (!g_GpuMade || g_GpuFailed || !pContext || which < 0 || which > 1)
    return;
  GpuQuery& q = g_Gpu[which][g_GpuNext[which]];
  if (q.Pending)
  {
    g_GpuOpen[which] = -1;   // the ring is still waiting on the GPU: this frame isn't measured
    return;
  }
  pContext->Begin(q.Disjoint.Get());
  pContext->End(q.Begin.Get());
  g_GpuOpen[which] = g_GpuNext[which];
}

void perf::GpuEnd(ID3D11DeviceContext* pContext, int which)
{
  if (!g_GpuMade || g_GpuFailed || !pContext || which < 0 || which > 1 || g_GpuOpen[which] < 0)
    return;
  GpuQuery& q = g_Gpu[which][g_GpuOpen[which]];
  pContext->End(q.End.Get());
  pContext->End(q.Disjoint.Get());
  q.Pending = true;
  g_GpuOpen[which] = -1;
  g_GpuNext[which] = (g_GpuNext[which] + 1) % GpuRing;
}

bool perf::DrawingPaused()
{
  return g_Phase == 1;
}

bool perf::TestRunning()
{
  return g_Phase >= 0;
}

void perf::DrawUI()
{
  ImGui::TextDisabled("Now: %s", g_LiveText.empty() ? "measuring..." : g_LiveText.c_str());
  ImGui::HelpMarker("Measured every second: the game's frames (from one Present to the next), the tools' own CPU time inside "
    "the game's hooks that run every frame (the camera, the visual environment mix, the depth copy, the drawing at "
    "Present, this menu) and the GPU time of what the tools draw (timed by the GPU itself). Other hooks that only run "
    "now and then (sounds, effects, streaming) aren't in it.");
  // The performance test's and the crowd test's buttons: commented out of the
  // menu (2026-11-04, the user's list)
#if 0
  if (g_Phase < 0)
  {
    if (ImGui::Button("Run a performance test (30 s)"))
    {
      g_Result.clear();
      g_Phase = 0;
      Begin(g_PhaseWindow[0]);
      util::log::Write("Performance test: phase 1, everything as set (%.0f s a phase)", PhaseSeconds);
    }
    ImGui::HelpMarker("Three phases of 10 s: everything as set, then the tools' drawing passes paused (the wet surfaces and "
      "the rain drawn far off, the green screen, the Panini projection), then everything as set again. Keep the camera "
      "still and the game in the same place for it (the frame time changes with what's on screen); the menu can be "
      "closed once it's started. The numbers go to CT.log and show here. Right after it, with the wet surfaces drawn, "
      "their pass is timed part by part over 3 frames (a short hitch; CT.log's \"the wet look's parts\").");
  }
  else
  {
    float elapsed = (float)((Ticks() - g_PhaseWindow[g_Phase].Start) / TicksPerMs() / 1000.0);
    ImGui::TextColored(ImVec4(1.f, 0.8f, 0.2f, 1.f), "Testing: phase %d of %d (%s), %.0f s left in it", g_Phase + 1, Phases,
      g_Phase == 1 ? "the tools' drawing paused" : "everything as set", (std::max)(0.f, PhaseSeconds - elapsed));
  }
  if (!g_Result.empty())
    ImGui::TextWrapped("Last test: %s", g_Result.c_str());
  road_traffic::DrawCrowdTest();   // the crowd test (road traffic session's)
#endif
}
