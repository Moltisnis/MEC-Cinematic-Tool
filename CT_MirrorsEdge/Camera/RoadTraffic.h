#pragma once

// Road traffic (2026-11-03): the city's road cars follow their lanes on the road, keep a gap to the car in their way
// and never show inside another car; the rules (RoadTrafficCore.h) run after the game's own car step, in it. The
// walking people keep a body apart and step aside for each other (WalkersCore.h, after the game's walker step), and
// by the time of day all the cars and people show in the day and fewer at night. The road cars are 2000 (the level's
// 1210) and the three big walking crowds get twice the level's members (raised in the level data as the game makes
// them); an unseen car shows again only out of sight. No menu entry (a hidden QoL feature, like Dogen's door).
// Research_Road_Traffic_2026-11-03.md has the decompile and the tests.
namespace road_traffic
{
  // The hooks on the game's car step, walker step, crowd update and crowd factories, before the level
  // (Main::Initialize)
  void Init();

  // The crowd test (MISC > Performance, 2026-11-03): the frame time with the crowds as set, with the level's, and as
  // set again, in one place, with the crowds' CPU (RoadTraffic.cpp). Render thread: each frame's time from
  // perf::FrameStart (frames under a second), the button and its state in the menu.
  void CrowdTestFrame(float ms);
  bool CrowdTestRunning();
  void DrawCrowdTest();
}
