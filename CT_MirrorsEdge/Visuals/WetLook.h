#pragma once
#include <d3d11.h>

// VISUALS > Weather > Wet surfaces (see WetLook.cpp): the ground made to look
// wet, drawn over the game's finished frame at Present, under the green
// screen.
namespace wetlook
{
  struct Settings
  {
    bool On;
    float Wetness;       // 0 - 1
    float Puddles;       // 0 - 1: how much of the ground
    float PuddleSize;    // metres
    float Reflections;   // 0 - 1
    bool Ripples;        // in the puddles while it rains
    float FadeDistance;  // metres: none past it
  };

  // The menu (any thread)
  void Set(Settings const& settings);
  // Whether it was drawn on the last frame (for the menu)
  bool Drawn();
  // How far the wet look reaches (metres; 0 while it's off), any thread
  float Reach();
  // Faith's footsteps (game thread): whether a point on a surface (world;
  // up: the y of the surface's normal, either way round) is in a puddle of
  // the last frame drawn, by the sums WetLook.hlsl draws them with
  bool InPuddle(float const* world, float up);
  // The same for a point of the ground (world, facing up), nothing else done
  // (a car's wheel: its wet tyre track, DryShapes.cpp); any thread, not with
  // the wet look's own lock held
  bool PuddleUnder(float const* world);
  // The same, and whether it's on the wet look's wet ground outside (its
  // wetGround): 2 in a puddle, 1 on the wet ground off them, 0 neither. Each
  // step asked for leaves its ring or print as InPuddle's do. impact: 1 a
  // step, 2 a soft landing, 3 a hard one (a landing's ring is bigger)
  // foot: which (1 her right, -1 her left, 0 not known): a step is the same
  // step asked again (its sound's and its effect's) only for the same foot
  // surface: what she stepped on, for her prints pressed into the wet
  // ground (1 concrete or stone, 2 gravel, 0 the rest: none)
  int StepWater(float const* world, float up, int impact = 1, int foot = 0, int surface = 1);
  // Any thread: the game plays her own water splash for a step or landing of
  // hers at world (Hooks.cpp: its effect's surface has the water's effects;
  // impact as StepWater's; material: the surface's, byte 2, for the log).
  // The wet look lets it through, and draws no splash of its own for it.
  // gameWater: the surface was the game's own water before the tools' (its
  // puddle decals, its water): a ring there too (see MostGameWaterRings)
  void NoteHerSplash(float const* world, int impact, unsigned int material, bool gameWater);
  // Any thread: a step of hers on a wall she runs along (normal: its
  // facing, either way round; foot as StepWater's): her wet shoes' print
  // there
  void NoteWallStep(float const* world, float const* normal, int foot);
  // Any thread: another's step (impact as StepWater's; 5 a car's wheel) at a
  // point: its ring in a puddle there. fromMoves: made from their moves (the
  // tracker's, DryShapes.cpp), not the game's own impact
  void OtherStep(float const* world, float up, int impact, bool fromMoves);
  // Any thread: a downed body's knock (its ragdoll's sound, Hooks.cpp) at a
  // point: its ring where it hit the ground, in a puddle there (see
  // LandBodies). Game thread (dry::Update): the knocks waiting, onto the
  // ground under them
  void NoteRagdollKnock(float const* at);
  void LandBodies();
  // Faith sliding (game thread or the effects'): where the game played her
  // slide's effect; in a puddle drawn there, a ring of her slide's wake
  void NoteSlide(float const* world);
  // The same for the snow on the ground (VISUALS > Snow): whether the last
  // frame drawn had snow lying at the point
  // (foot: which, as StepWater's: 1 her right, -1 her left, 0 not known)
  bool InSnow(float const* world, float up, int foot = 0);
  // Any thread: whether the snow lies on the ground at a point (as InSnow
  // has it, facing up) and how deep it's piled there (layer, m; SnowBodies.cpp:
  // the snow pressed in by bodies)
  bool SnowLayerAt(float const* world, float& layer);
  // Any thread: a KrugerSec's (or another's) step at a point (foot: 1 their
  // right, -1 their left, 0 not known; up as InSnow's): where the snow lies
  // there, their boot's print
  void OtherSnowStep(float const* world, float up, int foot);
  // Her slide's effect played at a point (any thread; from NoteSlide): where
  // the snow lies there, her slide's trail goes on through it
  void NoteSnowSlide(float const* world);
  // One of her hands touched a point (any thread; from the impact handlers:
  // a hand landing, climbing, scuffing; hand 1 her right, -1 her left; up:
  // the y of the surface's normal, either way round): where the snow lies
  // there, its print
  // stamp: her own hand's stamp for it (snowbody::NoteHandTouch; -1 none):
  // its print is that once it's pressed, else WetLook.hlsl's HandShape
  void NoteHand(float const* world, float up, int hand, int stamp = -1);
  // She rolled (any thread; from the impact handlers: the game's
  // SkillrollForward, at the contact it looked up for it; up as above):
  // where the snow lies there, her body's print
  void NoteRoll(float const* world, float up);
  // The same touches in the rain (the same threads): her hand's (normal: the
  // surface's, either way round; hand 1 her right, -1 her left) wets her
  // hands in a puddle or on the wet ground and, wet, leaves its print on
  // what's dry (the ground or a wall); her roll rings the puddle it's in
  void NoteRainHand(float const* world, float const* normal, int hand, int stamp = -1);
  // Timing the rain's and snow's work (2026-10-26, the user: "find a way to
  // improve the performance of rain/snow (GPU/CPU improvements)"; the log's
  // "Weather (performance, diagnostic)" every 10 s while it rains or snows):
  // a part's time (ms; any thread), ScopedTime timing its scope
  enum TimePart { TimeWeather, TimeRoofs, TimePipes, TimePeople, TimeSetUp, TimeDrips, TimePipesDrawn, TimeGpuPass,
    TimeGpuSnow, TimePeopleScan, TimeParts };
  void NoteTime(int part, double ms);
  double TimeNow();   // ms, a steady clock
  struct ScopedTime
  {
    int Part;
    double Start;
    explicit ScopedTime(int part) : Part(part), Start(TimeNow()) {}
    ~ScopedTime() { NoteTime(Part, TimeNow() - Start); }
  };
  void NoteRainRoll(float const* world, float up);
  // Game thread (dry::Update): where the drips off the ends of the slopes
  // the rain runs down land, a few a frame (see WetLook.cpp's
  // SlopeEndsAcross)
  void LandSlopeEnds();
  // Render thread, from the Present hook, on the game's swapchain only,
  // before the green screen
  void Apply(IDXGISwapChain* pSwapChain, ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
  // The wet ground in the game's own G-buffer (GbufferWet.cpp), render
  // thread, between Presents: what its pass reads, from the last frame drawn
  // (false: the wet look isn't on, it wasn't drawn at the last Present, or
  // snow lies). Its constants (b0) are that frame's with this frame's camera's
  // turn and projection (transform, projection: greenplane::FrameCamera's)
  // and the G-buffer pass's viewport (width, height, from its corner); where
  // the camera is stays that frame's (GbufferWet.hlsl). The views and
  // samplers are the wet look's own, alive until its next Apply.
  struct GbufferPass
  {
    ID3D11Buffer* Constants;              // b0
    ID3D11ShaderResourceView* Views[4];   // t2 the open sky, t3 the weather's squares, t4 the bodies' stamps, t9 the ground's marks
    ID3D11SamplerState* Samplers[2];      // s0, s1
  };
  bool TakeGbufferPass(ID3D11DeviceContext* pContext, float const* transform, float const* projection, UINT width,
    UINT height, GbufferPass& out);
  // Where g_ShelterGrid (the weather's squares' corner, size and count) is in
  // those constants, in bytes: for the G-buffer dump's text file
  int ShelterGridOffset();
}
