#pragma once
#include <d3d11.h>

// What the tools cost the game (MISC > Performance, 2026-10-26; the user:
// "make a test and see if we impact the performance of the game with all of
// the stuff we implemented"). Measured, not estimated:
// - the game's frame times, from one Present to the next;
// - the tools' own CPU time inside the game's hooks that run every frame
//   (Section), on the thread the game runs them on;
// - the GPU time of what the tools draw at Present (their passes and the
//   menu), with D3D11 timestamp queries read back a few frames later (never
//   waited for).
// The test: three phases of PhaseSeconds, the second with the tools'
// drawing passes paused (the wet surfaces, the green screen, the Panini
// projection), so the frame times with and without them can be compared.
namespace perf
{
  enum Section
  {
    SectionCameraHook,    // the camera hook: the camera, the weather, replays, streaming, the lights (game thread)
    SectionVeMix,         // the visual environment mix hook: the rainy look, the rain's strength (game thread)
    SectionDepthClear,    // the depth clear hook: the depth copy for the passes (render thread)
    SectionPasses,        // the drawing passes at Present (render thread)
    SectionMenu,          // the menu at Present (render thread)
    SectionCount
  };

  // Adds its time to a section (any thread)
  class Scope
  {
  public:
    explicit Scope(Section section);
    ~Scope();
  private:
    Section m_Section;
    long long m_Start;
  };

  // Adds time measured elsewhere (QueryPerformanceCounter ticks) to a section
  void AddTicks(Section section, long long ticks);
  // QueryPerformanceCounter's now (ticks)
  long long Now();
  // The game's own Present call (the frame handed on to the graphics card;
  // it waits there while the card is behind), timed in the Present hooks
  // (render thread). In CT.log's 10-s frame line.
  void AddPresentTicks(long long ticks);
  // The camera hook's parts, timed apart (diagnostic, 2026-11-03, the user:
  // "find a way to improve performance in fps even with rain/snow enabled";
  // the 11-02 snow test had the hook at 5.8 ms a frame, its parts unknown):
  // LapStart at its start each frame, Lap after each part (the time since the
  // one before), AddLap for a part timed on its own; every 10 s each part's
  // time a frame in CT.log. The camera hook's thread only; a part by its
  // name's address (one string for each).
  void LapStart();
  void Lap(char const* part);
  void AddLap(char const* part, long long ticks);
  // Render thread, the Present hook, before anything is drawn: a frame
  void FrameStart(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
  // Around what the tools draw: 0 the passes, 1 the menu (render thread)
  void GpuBegin(ID3D11DeviceContext* pContext, int which);
  void GpuEnd(ID3D11DeviceContext* pContext, int which);
  // During the test's second phase the drawing passes are skipped
  bool DrawingPaused();
  // Whether a performance test runs now (any of its phases; the wet look
  // times its parts once one is over, see WetLook.cpp's BreakdownSkips)
  bool TestRunning();
  // MISC tab (render thread)
  void DrawUI();
}
