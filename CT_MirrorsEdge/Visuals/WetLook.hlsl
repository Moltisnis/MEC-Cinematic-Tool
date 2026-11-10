// VISUALS > Weather > Wet surfaces, the lightning's flash and bolt, and the
// rain in the distance: drawn over the game's finished frame at Present (see
// WetLook.cpp). Compiled into WetLookVs.h and WetLookPs.h with the Windows
// SDK's fxc:
//   fxc /nologo /O3 /T vs_4_0 /E VsMain /Vn g_WetLookVs /Fh WetLookVs.h WetLook.hlsl
//   fxc /nologo /O3 /T ps_4_1 /E PsMain /Vn g_WetLookPs /Fh WetLookPs.h WetLook.hlsl
//   fxc /nologo /O3 /T ps_4_1 /E PsMain /D RAIN_ONLY=1 /Vn g_WetLookRainPs /Fh WetLookRainPs.h WetLook.hlsl
//   fxc /nologo /O3 /T ps_4_1 /E PsMain /D SNOW_ONLY=1 /Vn g_WetLookSnowGroundPs /Fh WetLookSnowGroundPs.h WetLook.hlsl
//   fxc /nologo /O3 /T ps_4_1 /E PsQuery /Vn g_WetLookQueryPs /Fh WetLookQueryPs.h WetLook.hlsl
//   fxc /nologo /O3 /T ps_4_1 /E NodesPs /Vn g_WetLookNodesPs /Fh WetLookNodesPs.h WetLook.hlsl
// (ps_4_1 since 2026-10-26: PuddleSurface's FarthestThrough gathers the
// depth's 2 x 2 texels; the other entries as their own notes say; NodesPs,
// the ground's nodes, see GroundGoesOn, is drawn by GbufferWet.cpp)
//
// From a copy of the game's depth each pixel's place is worked out relative
// to the camera, straight from the projection's own terms (a full inverse of
// the view and projection in 32-bit floats, with the game's reversed depth
// ([2][2] about 2.4e-6) and city coordinates in the hundreds of metres, gave
// places off by enough to look like z-fighting: 2026-10-25 17:0x), and which
// way it faces from its nearer neighbours on each side (not across an edge).
// What faces up (the ground, roofs, ledges) is made darker, the way water
// darkens a surface, and reflects the frame around it the more the flatter
// it's seen; puddles (where a noise over the ground is highest) are darker
// still, flat, mirror-like and, in the rain, rippled. Where something is
// over a point (the weather's squares, see ShelterAt) it stays dry. A
// lightning flash brightens the sky most and what's under the open sky, what
// faces up more than the rest; under a roof only near an opening (see
// FlashAt); its bolt is drawn where nothing nearer is in front of it. The
// rain in the distance is drops on sheets fixed in the world (see RainAt),
// drawn only where what the game drew is farther and the weather's squares
// say the sky is open. The snow (VISUALS > Snow) is flakes of its own, drawn
// after this pass (SnowVsMain, SnowPsMain; compiled into WetLookSnowVs.h and
// WetLookSnowPs.h with /T vs_4_0 /E SnowVsMain /Vn g_WetLookSnowVs and /T
// ps_4_0 /E SnowPsMain /Vn g_WetLookSnowPs), and snow lying on what faces up
// in place of the wet look.
//
// 2026-10-25 22:xx (the user: the wet surfaces swim and flicker; a screenshot
// with saw-toothed dry and wet patches and black puddles; the drawn rain's
// colour looks wrong):
// - dry by the weather's squares, each as found (its floor and roof), per
//   pixel and blended between neighbouring squares, not by the rain's boxes
//   (rebuilt as the camera moved, 2 m squares merged, judged by the camera's
//   own height);
// - the camera is the one of the frame the depth is from (GreenPlane.cpp);
// - a reflection that finds nothing on the frame adds nothing (it took the
//   top edge of the frame for the sky: a dark wall there made black puddles);
// - puddles only where the ground is flat 4 pixels round (not on grates);
// - the rain's drops and the light they catch from the frame blurred round
//   them and the whole frame's average (they took three pixels on the top edge
//   of the frame, whatever was there, as the sky's light).

cbuffer WetParams : register(b0)
{
  row_major float4x4 g_ViewProjection;   // camera-relative world (the camera at 0) to the game's clip space
  float4 g_CameraRight;   // xyz: the camera's axes in the world (view x, y, z; it looks along -z)
  float4 g_CameraUp;
  float4 g_CameraBack;
  float4 g_Camera;        // xyz: where the camera is; w: seconds (for the ripples and the rain)
  float4 g_ProjectionXY;  // [0][0], [1][1], [2][0], [2][1]
  float4 g_ProjectionZW;  // [2][2], [2][3], [3][2], [3][3]
  float4 g_ProjectionT;   // [3][0], [3][1]
  float4 g_Depth;         // xy: the depth copy's size over the frame's; z: 1 reversed; w: the sky's depth
  float4 g_Frame;         // xy: 1 / the frame's width, height; z: the scene copy's level ~60 pixels wide; w: its last level
  float4 g_Wet;           // x: wetness 0 - 1; y: puddles 0 - 1; z: puddle size (m); w: reflections 0 - 1
  float4 g_Rain;          // x: ripples 0 - 1; y: fades out by this distance (m); z: 1 the wet look is on
  float4 g_Flash;         // x: the lightning's brightness now (0 none); yzw: its tint
  float4 g_DistantRain;   // x: how much (0 none); y: the rain's strength (1 the Release mission's)
  float4 g_OpenSkyGrid;   // the open sky's squares (g_OpenSky): xy their corner (world x, z), z a square's size (m), w how many a side (-1: no roofs kept out, rain anywhere; 0: not known yet, none)
  float4 g_Bolt;          // x: 1 bolts to draw (0 none); z: their segments; w: how many bolts (a burst's, up to 6)
  float4 g_ShelterGrid;   // the weather's squares (g_Shelter): xy their corner (world x, z), z a square's size (m), w how many a side (0: not known)
  float4 g_Fog;           // the game's height fog's coefficients this frame (see FogAt and WetLook.cpp)
  float4 g_FogMisc;       // x: its start (m); y: the least it lets through; z: 1 on
  float4 g_BoltBox[6];        // per bolt: xy, zw its box on the frame with its glow (pixels)
  float4 g_BoltRange[6];      // per bolt: x its first segment, y how many, z how far away it struck (m), w how much the fog lets through
  float4 g_BoltSegment[384];  // xy, zw: a segment's ends on the frame (pixels)
  float4 g_BoltWidth[384];    // x: its core's half width (pixels); y: how bright now (its strike's flash in it)
  float4 g_DryNear;           // the camera in Faith's head: wet from x to y metres from it; z: her feet (relative y); w: 1 on
  float4 g_DryCount;          // x: how many boxes in g_DryBox
  float4 g_DryBox[256];       // 4 per box (see KeptDry): its middle (relative to the camera; w 1 + how far under its floor their feet are for Faith's or a person's, see KeptDryKind; 0 the rest), its axes / its half sizes (w the half size)
  float4 g_Snow;              // x: how much snow falls now (0 none); y: how many flakes 0 - 1; z: their size (1 the tools'); w: its clock (s)
  float4 g_SnowDrift;         // xy: how far the wind has carried it (world x, z; m); z: snow on the ground 0 - 1; w: none past this (m)
  float4 g_SnowMisc;          // x: 1 only where the weather's squares know the sky is open
  float4 g_SnowBoxes;         // the flakes (SnowVsMain): x how many in the near box, y in the far box; z, w the boxes' sizes (m)
  float4 g_SnowTiming;        // x: the snow's clock's period (s; its motions are whole cycles over it); y: 1 the frame before is known
  float4 g_SnowPrevious;      // xyz: where the camera is now from where it was the frame before (for the flakes' motion blur)
  row_major float4x4 g_PrevViewProjection;   // the frame before's, relative to its own camera
  float4 g_InteriorCount;     // x: how many boxes in g_InteriorBox, y: prisms in g_InteriorPrism
  float4 g_InteriorBox[128];  // the game's interiors (see InteriorAt): 4 per box, its middle (relative to the camera), its axes / its half sizes
  float4 g_InteriorPrism[18]; // 9 per prism: x how many points, y its floor, z its top (relative y); then its points (x, z relative)
  float4 g_QueryGrid;         // PsQuery: xy its corner (x, z), z a square's size (m), w Faith's feet's height (all relative to the camera)
  float4 g_SnowWind;          // the flakes: xy how fast the wind carries them (world x, z; m/s), z the frame's time (s)
  float4 g_StepCounts;        // Faith's steps: x rings in g_StepRing, y prints in g_Print, z a ring's seconds, w 1: one of them on the game's own water (see StepRings)
  float4 g_StepRing[12];      // a step in a puddle: xyz where (relative to the camera), w seconds since
  float4 g_Print[24];         // a wet footprint: xyz where (relative), w how wet now (0 - 1)
  float4 g_PrintDir[24];      // xy: the way she was going (world x, z; unit); zw: that foot's outer side (unit, see ShoeSole)
  float4 g_IceCount;          // the ice over the water (see IceAt): x how many planes in g_IcePlane, y how far it has formed (0 - 1)
  float4 g_IcePlane[192];     // 4 per piece: its middle (relative to the camera) and w its kind (0 stream, 1 wall, 2 pool, 3 box); its two half sides / their length squared, w their length; a flat one's facing and w how far in front of it the frame's surface may be, a box's third half side / its length squared and w its length
  float4 g_SnowPile;          // the snow on the ground (see SnowPileSide, SnowPrintsAt): x how deep it has piled (m), y Faith's prints in g_SnowPrint, z how deep a print is pressed (m)
  float4 g_SnowPrint[64];     // a print of Faith's in the snow: xyz where (relative to the camera), w how much of it is left (0 - 1: the snow fills it)
  float4 g_SnowPrintDir[64];  // xy: the way she was going (world x, z; unit)
  float4 g_DripCount;         // drips off roof edges: x how many in g_Drip; y 1 it rains; z a drop's half width (pixels at 1 m)
  float4 g_Drip[48];          // per stream: xy its top, zw its bottom on the frame (pixels)
  float4 g_DripInfo[48];      // x, y: its top's and bottom's view depth (m); z: drops a second; w: its phase (0 - 1)
  float4 g_DripLand[48];      // xyz: where its drops land (relative to the camera); w: how far they fall (m)
  float4 g_PipeCount;         // the drainpipes' water: x how many in g_Pipe; y how hard it runs (0 - 1); z the stream's half width, w pixels a metre (both at 1 m)
  float4 g_Pipe[16];          // per stream: xy where it comes out of the pipe, zw where it lands, on the frame (pixels)
  float4 g_PipeInfo[16];      // x, y: its top's and bottom's view depth (m); z: how far it falls (m); w: its phase (0 - 1)
  float4 g_PipeLand[16];      // xyz: where it lands (relative to the camera)
  float4 g_StepRingSize[12];  // per ring in g_StepRing: x how big (1 a step; a landing more), y its seconds, z 1 on the game's own water (see StepRings)
  float4 g_SlideCount;        // Faith sliding through a puddle: x how many rings in g_SlideRing, y a ring's seconds
  float4 g_SlideRing[16];     // xyz where (relative to the camera), w seconds since
  float4 g_IceTri[384];       // the ice's water triangles (see IceAt; how many in g_IceCount.z): 4 per triangle: its first corner (relative to the camera) and w how far in front of it the frame's surface may be; the duals of its two sides from that corner (a point's offset dotted with them: how far along each side); its facing
  float4 g_SnowTrailCount;    // Faith's slides through the snow (see SnowTrailAt): x how many stretches in g_SnowTrail
  float4 g_SnowTrail[48];     // per stretch: xyz its start (relative to the camera), w how much of it the snow hasn't filled (0 - 1)
  float4 g_SnowTrailEnd[48];  // xyz its end (relative), w its half width (m)
  float4 g_SnowRollCount;     // Faith's rolls in the snow (see SnowRollsAt): x how many in g_SnowRoll
  float4 g_SnowRoll[8];       // per roll: xyz between her shoulder's mark and her hip's (relative to the camera), w how much of it the snow hasn't filled (0 - 1)
  float4 g_SnowRollWay[8];    // xy the way she rolled (world x, z) x from her shoulder's mark to her hip's (m); zw the side of the shoulder she rolled over (unit)
  float4 g_Haze;              // the haze in the rain or the snow (see HazeAt): x how thick (per metre, 0 none); y 1 the snow's; zw how far the wind has carried its veils (world x, z; m)
  float4 g_HandCount;         // Faith's wet handprints: x how many in g_HandPrint
  float4 g_HandPrint[16];     // xyz where (relative to the camera, on its surface), w how wet now (0 - 1)
  float4 g_HandFinger[16];    // xyz the way her fingers point (in the surface's plane, unit)
  float4 g_HandThumb[16];     // xyz her thumb's side (in it, unit)
  float4 g_Options;           // x: 1 the depth copy can be gathered (2 x 2 texels at once, see FarthestThrough); y: 1 the wet ground is in the game's own G-buffer this frame (GbufferWet.hlsl: PsMain leaves its darkening, sheen and reflection out); z: 1 its pass's info is this frame's (g_WetInfo, t10: where it's open and kept dry, not worked out again here); w: 1 the ground's nodes are this frame's (g_Nodes, t11, and g_NodeGrid, b4: the puddles lie as PuddleGround has it, see GroundGoesOn)
  float4 g_HazeView;          // x: how much the camera is out in the open (0 - 1, eased; see HazeAt); yzw the sky's light by the horizon (see HazeTint; y < 0 not known yet)
  float4 g_OtherCounts;       // x how many in g_OtherRing, y in g_Splash; z pixels a metre at 1 m; w how much the camera looks down (0 - 1)
  float4 g_OtherRing[24];     // as g_StepRing: other people's steps and the cars' wheels (WetLook.cpp's MostOtherRings)
  float4 g_OtherRingSize[24]; // as g_StepRingSize
  float4 g_Splash[16];        // a little splash at a ripple (see SplashesAt): xy on the frame (pixels), z its view depth, w seconds since
  float4 g_SplashInfo[16];    // x how big (its ring's size), y a number for its drops; z 1: her own splash, the game's (see HerSplashAt), not drawn
  float4 g_RainBoxCount;      // x how many in g_RainBoxMin / Max: the game's rain kept out under the roofs (see RainKeptOut)
  float4 g_RainBoxMin[64];    // a box's lowest corner (relative to the camera)
  float4 g_RainBoxMax[64];    // its highest
  float4 g_SnowHandKeep[2];   // Faith's hands' last touches (see HandKeptAt): xyz where (relative to the camera), w how much kept (0 - 1)
  float4 g_SnowFeetKeep;      // Faith's feet (see FeetKeptAt): xyz where (relative to the camera), w 1 known
  float4 g_SnowStampCount;    // x the stamps in g_SnowStamp (see SnowStampsAt), y the KrugerSec's boot prints in g_OtherSnowPrint, z what the stamps are pressed into (1 the snow, 2 the wet look's water: WaterStampAt)
  float4 g_SnowStamp[16];     // a stamp: xyz its texel (0, 0)'s corner on the ground (relative to the camera), w how much of it the snow hasn't filled (0 - 1)
  float4 g_SnowStampAxis[16]; // xy the way its long side runs (world x, z; unit; its across (-y, x)), z the ground's rise along it, w its slice in g_SnowStamps
  float4 g_OtherSnowPrint[48];    // a KrugerSec's boot print (see SnowPrintsAt): xyz where (relative to the camera), w how much of it is left (0 - 1)
  float4 g_OtherSnowPrintDir[48]; // xy the way its toe points (world x, z; unit), zw that boot's outer side (unit)
  float4 g_HandStampCount;    // x her handprints from her own hand in g_HandStampAt (see HandStampPress)
  float4 g_HandStampAt[32];   // xyz its middle (relative to the camera), w how much is left: over 0 in the snow, under 0 a wet one
  float4 g_HandStampU[32];    // xyz its U axis (unit, in the surface's plane, along her fingers), w its slice in g_HandStamps
  float4 g_HandStampV[32];    // xyz its V axis (U x V the surface's facing)
  float4 g_BodyRippleCount;   // the KrugerSec down in the water (see BodyRipplesAt): x how many in g_BodyRipple
  float4 g_BodyRipple[3];     // per body: x its first piece in g_BodySegA/B, y how many, z seconds since he lay down, w since his last knock (1e9 none)
  float4 g_BodyRippleAt[3];   // per body: xyz its middle (relative to the camera), w how far out its waves can be (m)
  float4 g_BodySegA[48];      // a piece of his body, a capsule: xyz one end (relative to the camera), w its radius
  float4 g_BodySegB[48];      // xyz its other end
  float4 g_GameFrame;         // the game's own G-buffer (GameFrame.h): x 1 copied this frame (t6 - t8, b1); y Snow brightness; z the same for SnowLit's
  float4 g_SnowShoe[4];       // her feet by her bones (see ShoesKeptAt), relative to the camera, w 1 known: left ankle, left toe base, right ankle, right toe base
  float4 g_MarksWindow;       // the ground's marks (see MarksAt): x the map's uv per metre, yz its window's middle (world x, z), w how far from it they're kept
  float4 g_MarksOn;           // x 1: its red is the snow's (prints, tracks), y 1: its green is the water's (trails), z 1: its red is the wet tyre tracks, w the height its alpha 0 stands for
  // What of a list can reach a pixel (WetLook.cpp's FrameBox, PlaceBox): the box round all of its own; outside it
  // the list's loop isn't gone through (none of them would have left anything there). On the frame: xy its least
  // corner, zw its most (pixels). Round the camera: [0] xyz its least corner, [1] its most (relative to the camera).
  // All of the frame and the world until the list is made in a frame; least over most when it's empty.
  float4 g_DripBounds;        // the drips' streams (DripsAt)
  float4 g_PipeBounds;        // the drainpipes' water and its splash (PipesAt)
  float4 g_SplashBounds;      // the little splashes at the ripples (SplashesAt)
  float4 g_HerSplashBounds;   // her own splash, the game's (HerSplashAt)
  float4 g_DripLandBounds[2]; // where the drips land (DripRings)
  float4 g_PipeLandBounds[2]; // where the drainpipes' water lands (PipeRings)
  float4 g_PrintBounds[2];    // Faith's footprints (PrintsAt)
  // The kept-dry boxes by where they reach (WetLook.cpp's MakeDryGrid; see KeptDryKind): x, z round the camera in
  // DryGridSide cells a side. xy the grid's least corner (relative to the camera), z a cell's size (m), w DryGridSide
  // (0: no grid, every box gone through)
  float4 g_DryGrid;
  float4 g_DryCells[512];     // per cell (row after row along z): the boxes of g_DryBox that can reach it, bits of boxes 0 - 31 and 32 - 63 (asuint); two cells a float4 (xy, zw)
  // The game's interiors' boxes by where they reach (WetLook.cpp's MakeInteriorGrid; see InteriorAt): as g_DryGrid,
  // InteriorGridSide cells a side (w; 0: no grid, every box gone through)
  float4 g_InteriorGrid;
  float4 g_InteriorCells[256]; // per cell (row after row along z): the boxes of g_InteriorBox that can reach it, bits (asuint); four cells a float4 (x, y, z, w)
  // The falling water's streams by the frame's tiles (WetLook.cpp's FallingTiles; see FallingBits): x, y a tile's width
  // and height (pixels), z FallingSide (0: no tiles, every stream gone through)
  float4 g_FallingGrid;
  float4 g_FallingTiles[64];  // per tile (row after row down the frame): x, y the drips' streams 0 - 31 and 32 - 63, z the drainpipes', w the splashes' (bits, asuint)
  // What of a list can reach a point (WetLook.cpp's MakeListBounds, from the lists as written): [0] xyz the box's least
  // corner, [1] its most (relative to the camera); outside it none of the list's own tests let one through, so its
  // loop isn't gone through. All of the world until the list is made in a frame.
  float4 g_SnowPrintBounds[2];   // Faith's prints in the snow (SnowPrintsAt)
  float4 g_BootPrintBounds[2];   // the KrugerSec's boot prints (SnowPrintsAt)
  float4 g_HandStampBounds[2];   // her hand's stamps (SnowPrintsAt)
  float4 g_SnowTrailBounds[2];   // her slides' trails (SnowTrailAt)
  float4 g_SnowRollBounds[2];    // her rolls (SnowRollsAt)
  float4 g_SnowStampBounds[2];   // the bodies' stamps (SnowStampsAt)
  float4 g_RainBoxBounds[2];     // the game's rain kept out under the roofs (RainKeptOut)
};

Texture2D<float4> g_Scene : register(t0);
Texture2D<float> g_SceneDepth : register(t1);
Texture2D<float2> g_OpenSky : register(t2);   // per square: x 1 open to the sky; y its floor (the first surface under the camera)
// The snow pressed in by bodies (SnowStampsAt; SnowBodies.cpp): per slice a
// top-down mask, 2 cm a texel, 128 across and 256 along, how far down a body
// went into the snow there (0 - 1)
Texture2DArray<float> g_SnowStamps : register(t4);
// Her handprints from her own hand (SnowBodies.cpp's hand stamps, see
// HandStampPress): per slice 64 x 64 texels of 4 mm in the touched surface's
// plane, how much of her hand pressed there (0 - 1)
Texture2DArray<float> g_HandStamps : register(t5);
// The game's own G-buffer (GameFrame.h), copied where its G-buffer and decal
// passes end, while g_GameFrame.x is 1: its normals (xy in one of 6 bases,
// z the smoothness), its base colour (sRGB, read as linear; a the normal's
// basis / 6) and its material (y metal, z reflectance), the depth copy's
// size (the game's render size: the frame's at a resolution scale of 100 %,
// twice it each way at 200 %; WetLook.cpp takes no other), so read at the
// depth's texels (DepthTexel), not the frame's pixels. (2026-11-10: they were
// read at the frame's pixels and taken only at the frame's size, so an
// outside tester's game at 5120 x 2880 for a 2560 x 1440 frame had none.)
// In GbufferWet.hlsl's pass, drawn at the depth's own size, g_Depth is 1 and
// the depth's texels are its pixels.
Texture2D<float4> g_GameNormals : register(t6);
Texture2D<float4> g_GameColour : register(t7);
Texture2D<float4> g_GameMaterial : register(t8);
// The game's viewConstants (its shaders' b2) as copied with them: the 6
// bases at 416, 3 float4s each
cbuffer GameView : register(b1)
{
  float4 g_GameViewFirst[26];   // time .. crPrevViewProjMatrix (crViewProjMatrix: 14 - 17, one row each)
  float4 g_NormalBasis[18];     // normalBasisTransforms[6]
  float4 g_GameViewNext;        // (at 704)
  float4 g_GameCameraPos;       // cameraPos (at 720; Research_Replay_Mission_World, DogenGlow.hlsl)
};
// The performance test's breakdown of PsMain (WetLook.cpp's BreakdownSkips;
// 2026-11-02, the user said "Yes, add it" to a per-part GPU breakdown): x's
// bits, the parts left out. 0, all of them drawn, for every frame's own.
// Each part left out with an if: HLSL's ?: and && work out both sides, so a
// part behind one was drawn all the same (until 2026-11-02 the kept-dry boxes
// and the edge tests, so the breakdown gave them nothing and their cost went
// to the rest of the wet ground's).
cbuffer WetSkip : register(b2)
{
  float4 g_Skip;
};
static const uint SkipReflections = 1, SkipPuddleTests = 2, SkipEdgeTests = 4, SkipKept = 8, SkipWet = 16, SkipAll = 32,
  SkipRipples = 64, SkipRings = 128, SkipLightning = 256, SkipDistantRain = 512, SkipHaze = 1024, SkipFalling = 2048,
  SkipMarks = 4096, SkipShelter = 8192, SkipFacing = 16384, SkipSnow = 32768,
  // (the snow's own parts, left out before the rest of it: its piled side,
  // what's pressed into it, its light, its edges and what's kept out of it,
  // its facing; 2026-11-02, the user: "Check the snow performance test and
  // find a way to improve performance": 20:10's breakdown had the snow as
  // one part)
  SkipSnowSide = 65536, SkipSnowPressed = 131072, SkipSnowLight = 262144, SkipSnowEdges = 524288, SkipSnowFacing = 1048576;
bool Skipped(uint part)
{
  return (asuint(g_Skip.x) & part) != 0;
}

// The pass's copy for the rain (RAIN_ONLY: WetLookRainPs.h, drawn while no
// snow lies, WetLook.cpp's g_RainPs) has none of the snow on the ground in it.
// (2026-11-02, CT.log 19:52's performance test: the wet look's pass 8.5 ms of
// the GPU's at 4K. Its shader was 30,859 instructions and 47 temps by fxc's
// count, the snow's part 12,563 of them and 6 temps over the rest's 41: the
// GPU keeps room for the most a pixel can need, so the rain's pixels were
// worked on fewer at once.)
#ifdef RAIN_ONLY
static const bool SnowDrawn = false;
#else
static const bool SnowDrawn = true;
#endif
// And its copy for the snow (SNOW_ONLY: WetLookSnowGroundPs.h, WetLook.cpp's
// g_SnowGroundPs, drawn while snow lies and nothing of the rain's is on: the
// wet look, the drips, the drainpipes, the splashes, the rain in the
// distance) has none of those in it: the wet ground, the wet prints and
// marks, her own splash, the falling water, the distant rain (2026-11-02,
// the user: "Check the snow performance test and find a way to improve
// performance like the rain session also": CT.log 20:10's test, the pass
// 3.6 - 4.9 ms of the GPU's at 4K with snow lying, the rain's parts of it
// all left unused. fxc's count: 28 temps and about 15,760 instructions
// for the snow's copy, against the whole one's 43 and about 31,000).
#ifdef SNOW_ONLY
static const bool WetDrawn = false;
#else
static const bool WetDrawn = true;
#endif
// The ground's marks round the camera (GroundMarks.cpp; see MarksAt): red
// the snow pressed (in the rain the wet tyre tracks), green the water
// stirred, alpha the ground's height there, wrapping round (g_Wrap)
Texture2D<float4> g_Marks : register(t9);
// What's out in the open and what's kept dry at each of the frame's points,
// as the wet ground's pass in the game's G-buffer worked them out this frame
// (GbufferWet.hlsl's GbufferWetInfoPs; read only while g_Options.z is 1, see
// WetInfoAt): x how much is out in the open, y how much is kept dry
// (KeptDryKind's x), z how much of that by Faith or a person (its y), w the
// game's interiors there (InteriorHere)
Texture2D<float4> g_WetInfo : register(t10);
// The ground's nodes round the camera (NodesPs, drawn by GbufferWet.cpp each
// frame the puddles are: see GroundGoesOn; read only while g_Options.w is 1):
// per node x the height of the surface at it (world), y how broad that
// surface is round it (0 - 1; -1 none seen there)
Texture2D<float2> g_Nodes : register(t11);
cbuffer WetNodes : register(b4)
{
  float4 g_NodeGrid;     // xy: node (0, 0)'s world x and z; z: how far apart they are (m); w: how many a side
  float4 g_NodeOrigin;   // xyz: where the camera the frame's depth is relative to is (world), as NodesPs drew them
};
SamplerState g_Wrap : register(s1);
Texture2D<float4> g_Shelter : register(t3);   // per square: x 1 looked at; y the first surface under where it was looked from (-1e9 none); z the first roof over it (1e9 none); w where from
SamplerState g_Linear : register(s0);

float4 VsMain(uint id : SV_VertexID) : SV_POSITION
{
  // One triangle over the whole frame
  return float4(id == 2 ? 3.0 : -1.0, id == 1 ? 3.0 : -1.0, 0.0, 1.0);
}

// The depth copy's texel at a point of the frame (uv), clamped to the copy
// (2026-11-04, the user: "find anything wrong with depth maps and fix it"):
// a point past the frame's right or bottom edge took texel W or H, outside
// the copy, which reads 0, the sky where the depth runs reversed (the
// game's), while one past the left or top edge took the edge's own. So
// the rings round a pixel that look past the frame (SnowEdgeAt, SnowWideAt,
// DropAt, KeptDryAt's, the light round the snow and the ice) saw the sky
// along the right and bottom edges only: the snow faded there, and in the
// bottom right corner it went (SnowWideAt's 5 cm, up to 48 px). Now past
// any edge it's the edge's own texel. (0.99999 of the way at most: the
// last texel of a frame up to 100,000 pixels across; inside the frame the
// same texels as before.)
int2 DepthTexel(float2 uv)
{
  return int2(min(saturate(uv), 0.99999) / g_Frame.xy * g_Depth.xy);
}

// The depth copy's last texel (its size less one), the G-buffer's copies'
// too (see g_GameNormals); the frame's last pixel at 100 %
int2 DepthLast()
{
  return int2(round(g_Depth.xy / g_Frame.xy)) - 1;
}

// The frame's pixel a depth (or G-buffer) texel is in, kept on the frame
// (the texel itself at 100 %)
int2 FramePixelOf(int2 texel)
{
  return min(int2((float2(texel) + 0.5) / g_Depth.xy), int2(round(1.0 / g_Frame.xy)) - 1);
}

// The point of the frame on the centre of uv's depth texel (DepthTexel's):
// at 100 % a pixel's centre is its texel's
float2 DepthTexelUv(float2 uv)
{
  return (float2(DepthTexel(uv)) + 0.5) / g_Depth.xy * g_Frame.xy;
}

// A step on the frame (uv) of a whole number of the depth's texels, about
// pixels of the frame's own and never under one texel: a point that far from
// another is at the same place in its texel as that one is in its own (the
// depth put on its ray stays in line with it, see RelativeAtTexel), and a
// neighbour is never in the same texel. (2026-11-10, for any resolution
// scale, worked out from the code: a step of a pixel is a texel at 100 % and
// two at 200 %, but under 100 % two pixels side by side read the same texel,
// the same depth on rays a pixel apart, which makes a surface face the
// camera; at 150 % it's one or two texels from pixel to pixel, and the
// points rebuilt go off the surface by a sixth of a pixel one way and the
// other.) pixels: a whole number; at 100 % the step is exactly that many
// pixels, as before.
float2 TexelStep(float pixels)
{
  return max(1.0, round(pixels * g_Depth.xy)) / g_Depth.xy * g_Frame.xy;
}

float DepthAt(float2 uv)
{
  int2 texel = DepthTexel(uv);
  return g_SceneDepth.Load(int3(texel, 0));
}

bool IsSky(float depth)
{
  return g_Depth.z > 0.5 ? depth <= g_Depth.w : depth >= g_Depth.w;
}

// A point of the frame vz along the camera's z (negative: in front),
// relative to the camera (the world's axes)
float3 RelativeAtViewZ(float2 uv, float vz)
{
  float w = vz * g_ProjectionZW.y + g_ProjectionZW.w;
  float2 ndc = float2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);
  float vx = (ndc.x * w - vz * g_ProjectionXY.z - g_ProjectionT.x) / g_ProjectionXY.x;
  float vy = (ndc.y * w - vz * g_ProjectionXY.w - g_ProjectionT.y) / g_ProjectionXY.y;
  return vx * g_CameraRight.xyz + vy * g_CameraUp.xyz + vz * g_CameraBack.xyz;
}

// Where a pixel of the frame is, relative to the camera (the world's axes)
float3 RelativeAt(float2 uv, float depth)
{
  float vz = (g_ProjectionZW.z - depth * g_ProjectionZW.w) / (depth * g_ProjectionZW.y - g_ProjectionZW.x);
  return RelativeAtViewZ(uv, vz);
}

float3 RelativeAtUv(float2 uv)
{
  return RelativeAt(uv, DepthAt(uv));
}

// The frame's point near uv on its depth texel's centre (DepthAt's texel):
// the depth put on the ray it was taken along; sky: seen through to the sky.
// (2026-10-27, the user: "wetness is now disappearing when i look ahead only
// when i look down they appear", "It should be the same no matter the screen
// resolution": put on the ray through uv itself, a point between pixels
// took the depth of the texel round it, up to half a pixel's depth step off
// the surface. Looking down at the ground that's nothing; looking along it,
// the depth changing fast from pixel to pixel, it's a few per cent of the
// way to a point a few pixels off, and FlatAround took the ground for a drop:
// no puddles within about 6 m. At 3840 pixels across its ring fell on whole
// pixels, at 3406 between them.)
float3 RelativeAtTexel(float2 uv, out bool sky)
{
  int2 texel = DepthTexel(uv);
  float d = g_SceneDepth.Load(int3(texel, 0));
  sky = IsSky(d);
  return RelativeAt((float2(texel) + 0.5) / g_Depth.xy * g_Frame.xy, d);
}

// The G-buffer pass's info at a point of the frame (g_WetInfo), on DepthAt's
// texel (it's drawn at the depth's own texels)
float4 WetInfoAt(float2 uv)
{
  int2 texel = DepthTexel(uv);
  return g_WetInfo.Load(int3(texel, 0));
}

// Whether a pixel is outside a box on the frame (xy its least corner, zw its
// most: g_DripBounds and the others)
bool OffFrameBox(float2 pixel, float4 box)
{
  return any(pixel < box.xy) || any(pixel > box.zw);
}

// Which of a list's first count streams can reach a pixel's tile of the frame
// (g_FallingTiles; word 0, 1 the drips' 0 - 31 and 32 - 63, 2 the
// drainpipes', 3 the splashes'): all of them while there are no tiles. A
// stream whose own box doesn't come over a tile adds nothing in it (each
// list's most), so going through these gives what going through all of them
// gives (2026-11-03, the user: "find a way to improve performance in fps even
// with rain/snow enabled": every pixel in the box round them all went through
// all of them, 48 drips while it rains on the roofs).
uint FallingBits(float2 pixel, int word, int count)
{
  uint all = count >= 32 ? 0xFFFFFFFFu : count <= 0 ? 0u : (1u << (uint)count) - 1u;
  if (g_FallingGrid.z < 0.5)
    return all;
  int side = (int)g_FallingGrid.z;
  int2 tile = clamp(int2(pixel / g_FallingGrid.xy), 0, side - 1);
  float4 bits = g_FallingTiles[tile.y * side + tile.x];
  return all & asuint(word == 0 ? bits.x : word == 1 ? bits.y : word == 2 ? bits.z : bits.w);
}

// The lowest set bit's number (a power of 2 is exact as a float, and its
// exponent is that number)
int LowestBit(uint low)
{
  return (int)((asuint((float)low) >> 23) - 127u);
}

// Whether a point (relative to the camera) is outside a box round the camera
// (least, most: g_DripLandBounds and the others)
bool OffBox(float3 rel, float4 least, float4 most)
{
  return any(rel < least.xyz) || any(rel > most.xyz);
}

// How many of the frame's pixels a 4K frame's (2160 high) one is: what's
// measured in pixels round a point is this many times as many, so it covers
// the same share of the frame at any resolution (the user: "It should be the
// same no matter the screen resolution")
float PixelScale()
{
  return 1.0 / (2160.0 * g_Frame.y);
}

float Hash(float2 p)
{
  float3 q = frac(float3(p.xyx) * 0.1031);
  q += dot(q, q.yzx + 33.33);
  return frac((q.x + q.y) * q.z);
}

float Noise(float2 p)
{
  float2 i = floor(p), f = frac(p);
  float2 u = f * f * (3.0 - 2.0 * f);
  return lerp(lerp(Hash(i), Hash(i + float2(1, 0)), u.x), lerp(Hash(i + float2(0, 1)), Hash(i + float2(1, 1)), u.x), u.y);
}

// How much a point (relative to the camera) is inside one of the game's
// interiors, 0 - 1 (2026-10-26, the user: snow "also goes inside the lair").
// They're the volumes the game fades its interior visual environments in by
// (levels/sp/sp_maincity/visualenvironments: each AreaProximityEntity's
// boxes and prisms; Research_Snow_2026-10-26.md), the ones near the camera
// (Weather.cpp's interiors). The lair's roof isn't found by the roof sweeps
// over much of it, so its squares counted as open to the sky; the game's own
// volume says it's inside.
// - Soft (the user, 2026-10-26, a screenshot in the lair: the wet look and
//   the puddles flickering like z-fighting): the volumes were made for
//   lighting and their faces lie on the rooms' own floors and walls (the
//   lair's prism's floor at y 63.37 is its floor), so tested hard a surface
//   on a face came out inside or out pixel by pixel and frame by frame. Each
//   counts fully up to InteriorSideMargin outside its sides (gone
//   InteriorSideFade further out), down to InteriorFloorMargin under its
//   floor (gone InteriorFloorFade further down), and fades over
//   InteriorTopFade across its top.
// - The generic ones (ve_generic_interiortheview: 38 boxes and a prism, a
//   flag in .w) count only as much as the roof squares haven't looked at the
//   point (known, see SquaresKnowAt): they're lighting volumes over whole
//   blocks of The View that reach into open alleys (the user, 2026-10-26, a
//   puddle ending in a straight line at 190 52 -24, the face of #73, 29 x 25
//   x 39 m); where the squares have looked, the roof sweeps decide. The named
//   interiors (the lair and the rest) count everywhere.
static const float InteriorSideMargin = 0.3;
static const float InteriorSideFade = 0.4;
// (InteriorFloorMargin + InteriorFloorFade: WetLook.cpp's InteriorSphereExtra)
static const float InteriorFloorMargin = 1.0;
static const float InteriorFloorFade = 0.5;
static const float InteriorTopFade = 0.6;
static const int MostIcePlanes = 48;    // g_IcePlane: 4 each (WetLook.cpp)
static const int MostSnowPrints = 64;   // g_SnowPrint (WetLook.cpp)
static const int MostIceTris = 96;      // g_IceTri: 4 each (WetLook.cpp)
static const int MostSnowTrails = 48;   // g_SnowTrail (WetLook.cpp)
static const int MostSnowRolls = 8;     // g_SnowRoll (WetLook.cpp)

// From metres inside its sides (negative: outside), its floor and its top
float InteriorAmount(float side, float floorIn, float topIn)
{
  float s = smoothstep(-(InteriorSideMargin + InteriorSideFade), -InteriorSideMargin, side);
  float f = smoothstep(-(InteriorFloorMargin + InteriorFloorFade), -InteriorFloorMargin, floorIn);
  float t = smoothstep(-InteriorTopFade * 0.5, InteriorTopFade * 0.5, topIn);
  return s * f * t;
}

// One of g_InteriorBox's boxes at a point (InteriorAt's, below)
void InteriorBoxAt(int k, float3 rel, float known, inout float inside)
{
  float4 middle = g_InteriorBox[k * 4];
  float weight = middle.w > 0.5 ? 1.0 - known : 1.0;
  if (weight <= 0.0)
    return;
  float3 q = rel - middle.xyz;
  float4 a[3] = { g_InteriorBox[k * 4 + 1], g_InteriorBox[k * 4 + 2], g_InteriorBox[k * 4 + 3] };
  // (outside the sphere round it and its soft margins: nothing, skipped
  // at once; the performance, 2026-10-26: every pixel went through every
  // box in full. WetLook.cpp's MakeInteriorGrid lists a box in the cells
  // this sphere comes over)
  float reach = sqrt(a[0].w * a[0].w + a[1].w * a[1].w + a[2].w * a[2].w) + InteriorFloorMargin + InteriorFloorFade;
  if (dot(q, q) > reach * reach)
    return;
  // Its axis nearest the world's up is its height (its floor and top),
  // the other two its sides; metres inside each face (negative: outside)
  // (not a[upAxis]: an array read by a number worked out as it runs is
  // kept in memory, not in the GPU's registers, in each place this is drawn
  // into the pass: 13 of them)
  float up0 = abs(a[0].y) * a[0].w, up1 = abs(a[1].y) * a[1].w, up2 = abs(a[2].y) * a[2].w;
  int upAxis = up1 >= up0 ? 1 : 0;
  upAxis = up2 > (upAxis == 1 ? up1 : up0) ? 2 : upAxis;
  float side = 1e9, floorIn = 1e9, topIn = 1e9;
  [unroll] for (int i = 0; i < 3; ++i)
  {
    float s = dot(q, a[i].xyz);                 // along it, in its half sizes
    float plus = (1.0 - s) * a[i].w, minus = (1.0 + s) * a[i].w;
    if (i == upAxis)
    {
      bool upward = a[i].y >= 0.0;
      topIn = upward ? plus : minus;
      floorIn = upward ? minus : plus;
    }
    else
      side = min(side, min(plus, minus));
  }
  inside = max(inside, InteriorAmount(side, floorIn, topIn) * weight);
}

// The interiors' grid's cells a side (WetLook.cpp's InteriorGridSide)
static const int InteriorGridSide = 32;

float InteriorAt(float3 rel, float known)
{
  float inside = 0.0;
  // Only the boxes that can reach rel's cell of the grid, in the same order
  // (2026-11-03, the user: "find a way to improve performance in fps even
  // with rain/snow enabled": every point went through all of them, up to 32,
  // for the shelter at every pixel and for every drop of the rain in the
  // distance). The same result: a box whose sphere doesn't reach a point is
  // skipped there at once, and outside the grid none reaches.
  [branch] if (g_InteriorGrid.w > 0.5)
  {
    float2 f = (rel.xz - g_InteriorGrid.xy) / g_InteriorGrid.z;
    if (all(f >= 0.0) && all(f < g_InteriorGrid.w))
    {
      int2 cell = int2(f);
      int i = cell.y * InteriorGridSide + cell.x;
      float4 four = g_InteriorCells[i >> 2];
      int j = i & 3;
      uint left = asuint(j == 0 ? four.x : j == 1 ? four.y : j == 2 ? four.z : four.w);
      [loop] while (left != 0u && inside < 1.0)
      {
        uint low = left & (0u - left);
        left ^= low;
        InteriorBoxAt(LowestBit(low), rel, known, inside);
      }
    }
  }
  else
  {
    int boxes = (int)g_InteriorCount.x;
    [loop] for (int k = 0; k < boxes && inside < 1.0; ++k)
      InteriorBoxAt(k, rel, known, inside);
  }
  int prisms = (int)g_InteriorCount.y;
  [loop] for (int p = 0; p < prisms && inside < 1.0; ++p)
  {
    float4 head = g_InteriorPrism[p * 9];
    float weight = head.w > 0.5 ? 1.0 - known : 1.0;
    float floorIn = rel.y - head.y, topIn = head.z - rel.y;
    if (weight <= 0.0 || floorIn < -(InteriorFloorMargin + InteriorFloorFade) || topIn < -InteriorTopFade * 0.5)
      continue;
    // Inside its outline (a crossing count) and how far from its nearest edge
    int n = (int)head.x;
    bool within = false;
    float edge = 1e9;
    [loop] for (int i = 0; i < n; ++i)
    {
      float2 a = g_InteriorPrism[p * 9 + 1 + i].xy, b = g_InteriorPrism[p * 9 + 1 + (i + 1 < n ? i + 1 : 0)].xy;
      if ((a.y > rel.z) != (b.y > rel.z) && rel.x < (b.x - a.x) * (rel.z - a.y) / (b.y - a.y) + a.x)
        within = !within;
      float2 ab = b - a, ap = rel.xz - a;
      float t = saturate(dot(ap, ab) / max(dot(ab, ab), 1e-6));
      edge = min(edge, length(ap - ab * t));
    }
    inside = max(inside, InteriorAmount(within ? edge : -edge, floorIn, topIn) * weight);
  }
  return inside;
}

// How much is over a point (world y) in one of the weather's squares: 1
// something is (under its roof, or more than 2 m under the first surface
// found looking down: not that surface itself, nor the ground under a railing
// found there), 0 open to the sky. Not looked at yet, or outside the squares:
// open.
float ShelterInSquare(int2 cell, float y)
{
  if (any(cell < 0) || any(cell >= (int)g_ShelterGrid.w))
    return 0.0;
  float4 square = g_Shelter.Load(int3(cell, 0));
  if (square.x < 0.5)
    return 0.0;
  float under = saturate(square.y - 2.0 - y);
  float roofed = square.z < 1e8 ? saturate(square.z + 1.0 - y) : 0.0;
  return max(under, roofed);
}

// How much the squares have looked at a point (world), blended as ShelterAt
// blends them: 0 outside them or not looked at yet (for InteriorAt)
float SquaresKnowAt(float3 world)
{
  if (g_ShelterGrid.w < 1.0)
    return 0.0;
  float2 f = (world.xz - g_ShelterGrid.xy) / g_ShelterGrid.z - 0.5;
  float2 corner = floor(f);
  float2 t = f - corner;
  int2 c = int2(corner);
  float k[4];
  [unroll] for (int i = 0; i < 4; ++i)
  {
    int2 cell = c + int2(i & 1, i >> 1);
    k[i] = any(cell < 0) || any(cell >= (int)g_ShelterGrid.w) ? 0.0 : (g_Shelter.Load(int3(cell, 0)).x < 0.5 ? 0.0 : 1.0);
  }
  return lerp(lerp(k[0], k[1], t.x), lerp(k[2], k[3], t.x), t.y);
}

// The same at any point, blended between the four nearest squares' middles
// (soft over a square, no steps); inside one of the game's interiors, as
// much as it is (InteriorAt)
float ShelterWith(float3 world, float interior)
{
  if (g_ShelterGrid.w < 1.0)
    return interior;
  float2 f = (world.xz - g_ShelterGrid.xy) / g_ShelterGrid.z - 0.5;
  float2 corner = floor(f);
  float2 t = f - corner;
  int2 c = int2(corner);
  float a = ShelterInSquare(c, world.y), b = ShelterInSquare(c + int2(1, 0), world.y);
  float d = ShelterInSquare(c + int2(0, 1), world.y), e = ShelterInSquare(c + int2(1, 1), world.y);
  return max(lerp(lerp(a, b, t.x), lerp(d, e, t.x), t.y), interior);
}

// The game's interiors at a point, as ShelterAt and KnownOpenAt take them
float InteriorHere(float3 world)
{
  return InteriorAt(world - g_Camera.xyz, SquaresKnowAt(world));
}

float ShelterAt(float3 world)
{
  return ShelterWith(world, InteriorHere(world));
}

// How surely a point is under the open sky (1) as far as the weather's
// squares know: looked at and nothing over it; a square not looked at yet,
// or outside the squares (past 50 m), counts as not open (0). Puddles go
// only where this is so (2026-10-26, the user: "rain puddles shouldn't be in
// objects that are behind windows / under windows": a room seen through a
// window in a square not looked at yet, or past the squares, counted as
// open and got puddles).
// Past the squares (50 m out, where the weather can't tell) open, as the
// wet look's puddles and the snow have it there (PsMain's pastSquares): it
// was none there, so the distant rain's drops (RainOpenAt) and the snow's
// flakes (SnowOpenAt) stopped at the squares' edge, and the puddles blended
// into "not open" over the last square: from up high a square round the
// camera, lighter inside (2026-11-01, the user's screenshot: "this is the
// square ... can we make it seamless so there is not a square there?").
// A square not looked at yet is still none.
float KnownOpenInSquare(int2 cell, float y)
{
  if (any(cell < 0) || any(cell >= (int)g_ShelterGrid.w))
    return 1.0;
  if (g_Shelter.Load(int3(cell, 0)).x < 0.5)
    return 0.0;
  return 1.0 - ShelterInSquare(cell, y);
}

float KnownOpenWith(float3 world, float interior)
{
  if (g_ShelterGrid.w < 1.0)
    return 0.0;
  float2 f = (world.xz - g_ShelterGrid.xy) / g_ShelterGrid.z - 0.5;
  float2 corner = floor(f);
  float2 t = f - corner;
  int2 c = int2(corner);
  float a = KnownOpenInSquare(c, world.y), b = KnownOpenInSquare(c + int2(1, 0), world.y);
  float d = KnownOpenInSquare(c + int2(0, 1), world.y), e = KnownOpenInSquare(c + int2(1, 1), world.y);
  // (not inside one of the game's interiors, InteriorAt)
  return min(lerp(lerp(a, b, t.x), lerp(d, e, t.x), t.y), 1.0 - interior);
}

float KnownOpenAt(float3 world)
{
  return KnownOpenWith(world, InteriorHere(world));
}

// How much a lightning flash lights a point: all of it under the open sky;
// under a roof only near an opening (the open sky within 2 - 4 m round it,
// at its height: a window, a door, a roof's edge)
float FlashAt(float3 world, float shelter)
{
  float open = 1.0 - shelter;
  if (shelter < 0.01 || g_ShelterGrid.w < 1.0)
    return open;
  float nearby = 0.0;
  [unroll] for (int k = 0; k < 8; ++k)
  {
    float r = k < 4 ? 2.0 : 4.0;
    float2 offset = (k & 3) == 0 ? float2(r, 0.0) : (k & 3) == 1 ? float2(-r, 0.0) : (k & 3) == 2 ? float2(0.0, r) : float2(0.0, -r);
    nearby += (1.0 - ShelterAt(world + float3(offset.x, 0.0, offset.y))) * (k < 4 ? 0.16 : 0.09);
  }
  return max(open, saturate(nearby));
}

// Raindrops' rings on a puddle, one size of drop: the slope (x, z) of the
// water's surface at p (metres) at time t. A drop falls somewhere in each
// cell (size metres) now and then; its ring spreads to 1.2 cells, a short
// train of waves (three or four crests) behind its front that widens as it
// spreads and dies away (the height squared of what's left of its time).
// (2026-10-25 23:4x, the user: the ripples looked bad: before, one ring a
// drop, all one size on a 0.5 m grid, a thin bright line.)
float3 RippleLayer(float2 p, float t, float size, float seed, float density)
{
  float2 q = p / size;
  float2 cell = floor(q);
  float2 slope = 0;
  float drop = 0.0;
  [unroll] for (int j = -1; j <= 1; ++j)
  [unroll] for (int i = -1; i <= 1; ++i)
  {
    float2 c = cell + float2(i, j);
    float h1 = Hash(c + seed), h2 = Hash(c + seed + 3.7), h3 = Hash(c + seed + 7.9);
    float cycle = t / (0.9 + 0.9 * h1) + h2 * 13.1;
    float phase = frac(cycle);
    // (a drop this time round only as often as it rains: see RippleDensity)
    if (Hash(c + seed + floor(cycle) * 0.618) > density)
      continue;
    float2 d = q - (c + 0.15 + 0.7 * float2(h2, h3));
    float r = length(d) + 1e-4;
    float width = 0.05 + 0.1 * phase;
    float x = (r - phase * 1.2) / width;   // its front at 0, the waves behind it below 0
    float envelope = exp(-0.35 * x * x) * (1.0 - smoothstep(0.0, 1.0, x));
    float fade = (1.0 - phase) * (1.0 - phase);
    // d(height) / d(r): height = 0.06 x width x envelope x sin(4x)
    float wave = 0.06 * (envelope * 4.0 * cos(4.0 * x) - 0.7 * x * exp(-0.35 * x * x) * sin(4.0 * x));
    slope += d / r * wave * fade;
    // the drop itself, a bright speck where it lands as its ring starts
    drop = max(drop, saturate(1.0 - phase / 0.08) * saturate(1.0 - r / 0.05));
  }
  return float3(slope, drop);
}

// The raindrops' rings on the puddles (xy: the water's slope) and their
// drops landing (z: 0 - 1). Only where the drawn rain lands (2026-10-26, the
// user: "water ripples should appear only where drawn rain actually hits and
// not always": every cell rang every time round whatever the rain, on every
// puddle out to the wet look's 200 m, and under the roofs' edges where the
// game's rain is kept out): a cell's drop falls each time round only as often
// as it rains (density: RippleDensity); see RainLandsAt for where.
float3 Ripples(float2 p, float t, float density)
{
  return RippleLayer(p, t, 0.35, 0.0, density) * 0.8 + RippleLayer(p, t * 0.85, 0.8, 31.7, density) * 0.6;
}

// How often a cell's drop falls each time round: as hard as it rains (the
// distant rain's strength, g_DistantRain.y: about 1 heavy rain, all of them;
// a drizzle a few)
float RippleDensity()
{
  return saturate(0.1 + 0.9 * g_DistantRain.y);
}

// How much of the game's rain is kept out at a point (relative to the
// camera): inside one of the boxes it's kept out of under the roofs
// (Weather.cpp's, g_RainBoxMin / Max), soft over 10 cm each way at its sides
float RainKeptOut(float3 rel)
{
  float kept = 0.0;
  // (none keeps it out of a point outside the box round them all and their
  // soft sides, MakeListBounds)
  if (OffBox(rel, g_RainBoxBounds[0], g_RainBoxBounds[1]))
    return kept;
  int n = (int)g_RainBoxCount.x;
  [loop] for (int k = 0; k < n; ++k)
  {
    float3 inside = min(rel - g_RainBoxMin[k].xyz, g_RainBoxMax[k].xyz - rel);
    if (inside.y > 0.0)
      kept = max(kept, smoothstep(-0.1, 0.1, min(inside.x, inside.z)));
  }
  return kept;
}

// How much of the drawn rain lands at a point of the ground (relative to the
// camera, distance from it): the game's own round the camera and the tools'
// out to its reach (RainAt's: 60 m, 80 where the rain isn't kept out of
// buildings), the last 10 m fading; not where the game's is kept out
// (RainKeptOut)
float RainLandsAt(float3 rel, float distance)
{
  float reach = g_OpenSkyGrid.w < 0.0 ? 80.0 : 60.0;
  float here = saturate((reach - distance) / 10.0);
  if (here <= 0.0)
    return 0.0;
  return here * (1.0 - RainKeptOut(rel));
}

// How much of a point (relative to the camera) the game's height fog lets
// through, as the game's sky / fog pass works it out (see WetLook.cpp): 1
// none of it fogged
float FogAt(float3 rel)
{
  if (g_FogMisc.z < 0.5)
    return 1.0;
  float h = abs(rel.y) < 0.01 ? 0.01 : rel.y;
  float k = (g_Fog.z / (1.0 + exp2(h * g_Fog.x + g_Fog.y)) - g_Fog.w) / h;
  float d = max(length(rel) - g_FogMisc.x, 0.0);
  return max(exp2(-abs(d * k)), g_FogMisc.y);
}

// Eight ways round a pixel on the frame
static const float2 RingDirections[8] = { float2(1.0, 0.0), float2(0.7071, 0.7071), float2(0.0, 1.0),
  float2(-0.7071, 0.7071), float2(-1.0, 0.0), float2(-0.7071, -0.7071), float2(0.0, -1.0), float2(0.7071, -0.7071) };

// One of g_DryBox's boxes at a point (KeptDryKind's, below): how much it
// keeps there, and of that by Faith or a person
void KeptDryBox(int k, float3 rel, inout float kept, inout float body)
{
  float4 middle = g_DryBox[k * 4];
  float3 q = rel - middle.xyz;
  float4 ax = g_DryBox[k * 4 + 1], ay = g_DryBox[k * 4 + 2], az = g_DryBox[k * 4 + 3];
  // (one standing on the ground, w's size over 0.5: a body's, w > 0, or a
  // vehicle's or a train's, w < 0, see below; the ground this far under
  // its floor, < 0 over it)
  bool standing = abs(middle.w) > 0.5;
  float feet = standing ? abs(middle.w) - 1.0 : 0.0;
  // (outside the sphere round the box and its feet: skipped at once;
  // WetLook.cpp's MakeDryGrid lists a box in the cells this sphere comes
  // over)
  float reach = sqrt(ax.w * ax.w + ay.w * ay.w + az.w * az.w) + max(feet, 0.0);
  if (dot(q, q) > reach * reach)
    return;
  float3 v = float3(dot(q, ax.xyz), dot(q, ay.xyz), dot(q, az.xyz));
  float3 u = abs(v);
  // How far inside its sides, in metres, faded over the last 25 cm
  // (smoothstep, so where it ends shows no line; 20 cm straight until
  // 2026-10-27, see below)
  float inBox = 0.0;
  if (standing)
  {
    // Faith's or a person's: from their feet up (2026-10-27, the user:
    // "Smooth out the shader when faith ksec interact with snow/rain and
    // the shader is on their model"). The box's floor is 12 cm over the
    // ground they stand on, so the puddle or the snow at their feet stays;
    // their shoes and ankles under it had the ground's look (the puddle's
    // mirror and rings, the snow on them) and a line where the box began.
    // Now what's 4 - 8 cm or more over the ground they stand on within the
    // box's footprint is theirs too, and the floor isn't a side that fades;
    // the ground itself stays the ground's.
    // A vehicle's or a train's the same (w < 0, 2026-11-01, the user: "Cars
    // have a weird square around them with rain": the road under a parked
    // car and round it in its box was kept dry), not a body's wet look.
    float overFeet = (v.y + 1.0) * ay.w + feet;
    float fromTop = (1.0 - v.y) * ay.w;
    if (u.x < 1.0 && u.z < 1.0 && fromTop > 0.0 && overFeet > 0.0)
    {
      float side = min((1.0 - u.x) * ax.w, (1.0 - u.z) * az.w);
      inBox = smoothstep(0.0, 0.25, min(side, fromTop)) * smoothstep(0.04, 0.08, overFeet);
    }
    if (middle.w > 0.5)
      body = max(body, inBox);
  }
  else if (max(u.x, max(u.y, u.z)) < 1.0)
  {
    float3 inside = (1.0 - u) * float3(ax.w, ay.w, az.w);
    inBox = smoothstep(0.0, 0.25, min(inside.x, min(inside.y, inside.z)));
  }
  kept = max(kept, inBox);
}

// The kept-dry grid's cells a side (WetLook.cpp's DryGridSide)
static const int DryGridSide = 32;

// How much a point (relative to the camera) is left as the game drew it:
// Faith, people, vehicles and trains (the user, 2026-10-26: "peddles should
// not affect faith ... or other people", "vechicles bikes trains as well"),
// in boxes round them (DryShapes.cpp), the last 0.2 m inside a box's sides
// fading, so the ground or a wall next to one doesn't end in a hard edge;
// and with the camera in Faith's head, what's right in front of it (her own
// arms and legs), above her feet
// y: how much of that is by Faith or a person (their boxes, g_DryBox's w 1,
// and what's right in front of the camera in her head), who are a little wet
// themselves out in the rain instead (see BodyWetness)
float2 KeptDryKind(float3 rel, float distance)
{
  float kept = 0.0, body = 0.0;
  if (g_DryNear.w > 0.5 && rel.y > g_DryNear.z)
  {
    kept = 1.0 - smoothstep(g_DryNear.x, g_DryNear.y, distance);
    body = kept;
  }
  // Only the boxes that can reach rel's cell of the grid, in the same order
  // (2026-11-03, the user: "find a way to improve performance in fps even
  // with rain/snow enabled": every pixel went through all of them, up to 64,
  // the people and the traffic round the camera). The same result: a box
  // whose sphere doesn't reach a point is skipped there at once, and outside
  // the grid none reaches.
  [branch] if (g_DryGrid.w > 0.5)
  {
    float2 f = (rel.xz - g_DryGrid.xy) / g_DryGrid.z;
    if (all(f >= 0.0) && all(f < g_DryGrid.w))
    {
      int2 cell = int2(f);
      int i = cell.y * DryGridSide + cell.x;
      float4 pair = g_DryCells[i >> 1];
      uint2 bits = (i & 1) != 0 ? asuint(pair.zw) : asuint(pair.xy);
      [loop] for (int part = 0; part < 2 && kept < 1.0; ++part)
      {
        uint left = part == 0 ? bits.x : bits.y;
        [loop] while (left != 0u && kept < 1.0)
        {
          uint low = left & (0u - left);
          left ^= low;
          int k = part * 32 + LowestBit(low);
          KeptDryBox(k, rel, kept, body);
        }
      }
    }
  }
  else
  {
    int count = (int)g_DryCount.x;
    [loop] for (int k = 0; k < count && kept < 1.0; ++k)
      KeptDryBox(k, rel, kept, body);
  }
  return float2(kept, body);
}

float KeptDry(float3 rel, float distance)
{
  return KeptDryKind(rel, distance).x;
}

// What's kept dry at a pixel (see KeptDryKind), its outline as soft as the
// frame's (2026-10-27, the user: "there is a halo around her"): the game
// softens an outline over a pixel or two (its antialiasing) while the depth
// keeps it sharp, so the pixels on Faith's or a person's outline showing
// some of them were taken for the ground behind (darkened and mirrored as
// it: a dark rim, as dark as Wetness makes the ground), and those showing
// some of the ground behind were left as drawn (a light one). Where the
// distance jumps by EdgeJump within EdgeRing pixels round it (at 4K; 8
// points, on their texels' centres), the average of its own and those 8
// points' own. (EdgeJump 5 % at first: the ground far off, its distance
// changing by that much from pixel to pixel when seen along it, counted as
// an outline everywhere, the pass's dearest part 9 times over.)
static const float EdgeRing = 1.5;
static const float EdgeJump = 0.25;

float2 KeptDryAt(float2 uv, float distance, float3 here)
{
  float2 kept = KeptDryKind(here, distance);
  // (nothing kept here: nothing to soften, and its 8 points not looked at;
  // the same as before, for most of the frame's pixels: the pass's cost at
  // 4K, 2026-11-01, the user: "Improve the performance of the Rain it tanks
  // the fps from 130 to 70 at 4k")
  if (kept.x <= 0.0)
    return kept;
  float2 step = max(1.0, EdgeRing * PixelScale()) * g_Frame.xy;
  float3 around[8];
  bool seen[8];
  bool edge = false;
  [unroll] for (int k = 0; k < 8; ++k)
  {
    bool sky;
    around[k] = RelativeAtTexel(uv + RingDirections[k] * step, sky);
    seen[k] = !sky;
    float there = sky ? 1e9 : length(around[k]);
    edge = edge || abs(there - distance) > EdgeJump * distance;
  }
  // (only what's kept itself is softened at its edge, never the ground
  // beside it made drier: 2026-10-27, the user's screenshot, "there is still
  // a weird halo when faith moves in the rain": the floor's pixels next to
  // her were partly kept dry, a thin light line round her on the dark wet
  // floor)
  if (!edge || kept.x <= 0.0)
    return kept;
  float2 sum = kept;
  [loop] for (int j = 0; j < 8; ++j)
    if (seen[j])
      sum += KeptDryKind(around[j], length(around[j]));
  return min(kept, sum / 9.0);
}

// KeptDryAt with what's kept at the pixel and round it read from the G-buffer
// pass's info (WetInfoAt: kept, its yz here) instead of worked out again: the
// same softening at an outline (2026-11-02, the user said "Yes, speed it up":
// the boxes' 1.15 ms of the pass at Present, worked out in the G-buffer's pass
// too)
float2 KeptDryInfoAt(float2 uv, float distance, float2 kept)
{
  if (kept.x <= 0.0)
    return kept;
  float2 step = max(1.0, EdgeRing * PixelScale()) * g_Frame.xy;
  float2 around[8];
  bool seen[8];
  bool edge = false;
  [unroll] for (int k = 0; k < 8; ++k)
  {
    bool sky;
    float2 at = uv + RingDirections[k] * step;
    float3 there = RelativeAtTexel(at, sky);
    seen[k] = !sky;
    float far = sky ? 1e9 : length(there);
    edge = edge || abs(far - distance) > EdgeJump * distance;
    around[k] = sky ? 0.0 : WetInfoAt(at).yz;
  }
  if (!edge)
    return kept;
  float2 sum = kept;
  [unroll] for (int j = 0; j < 8; ++j)
    if (seen[j])
      sum += around[j];
  return min(kept, sum / 9.0);
}

// How wet Faith and the people look out in the rain, of how wet the ground
// there is (see PsMain's bodyWet; 0.7 until 2026-10-27, the user: "ksec and
// faith still don't look wet under rain")
static const float BodyWetness = 1.0;

// Which way the frame's surface at uv faces (towards the camera), from the
// nearer neighbour on each side; smooth: 0 where the neighbours jump away
// (an edge, holes), 1 where they don't. The neighbours a pixel off in whole
// depth texels (TexelStep), at the same place in their texels as uv is in
// its own: on their texels' centres where uv is on its own (PsMain's,
// DepthTexelUv), so here should be there too.
float3 FrameNormal(float2 uv, float3 here, float distance, out float smooth)
{
  float2 step = TexelStep(1.0);
  float3 left = RelativeAtUv(uv - float2(step.x, 0.0)), right = RelativeAtUv(uv + float2(step.x, 0.0));
  float3 up = RelativeAtUv(uv - float2(0.0, step.y)), down = RelativeAtUv(uv + float2(0.0, step.y));
  float3 dx = length(here - left) < length(right - here) ? here - left : right - here;
  float3 dy = length(here - up) < length(down - here) ? here - up : down - here;
  float3 normal = normalize(cross(dy, dx));
  if (dot(normal, -here) < 0.0)
    normal = -normal;
  float jump = max(length(dx), length(dy)) / max(distance, 0.5);
  smooth = saturate(1.0 - (jump - 0.03) * 25.0);
  return normal;
}

// How far a point is off the line through two others (m)
float OffLine(float3 p, float3 a, float3 b)
{
  float3 ab = b - a;
  float l = length(ab);
  return l > 1e-5 ? length(cross(p - a, ab)) / l : length(p - a);
}

// Which way the frame's surface at uv faces, for the snow (2026-10-26, the
// user: "Snow shader still bleeds around edges when viewing it from an
// angle"). FrameNormal takes the nearer neighbour each way. A ledge's front
// face seen at a glancing angle has its pixels far apart, so for its row at
// the top the nearer one up was across the corner, on the top: that row
// faced up and took the top's snow, a strip over the edge seen only from an
// angle. Here each way takes the side whose two pixels (1 and 2 off) are in
// line with this one (the least off the line through them: a plane's are
// exactly in line), so a pixel at a corner faces the way of the surface it's
// on. clean: 1 where the surface bends under 17 degrees at the pixel on its
// in-line side each way, 0 from 37 (a sliver 1 pixel wide, holes: no snow).
// (It was against a pixel's width at its distance, 1 - 2: seen at a glancing
// angle the ground's pixels are 0.2 - 1 m apart, so a bend of a degree or two
// failed it and the snow went off the far ground: the user, "snow effect
// disappears when looking straight when it shouldn't".)
// step: how far its neighbours are (pixels; 1, or 4 over what the snow lies
// deep enough to bury, PsMain), in whole depth texels as FrameNormal's
// (TexelStep: exactly that many pixels at 100 %).
float3 SnowNormal(float2 uv, float3 here, float distance, float step, out float clean)
{
  float2 texels = TexelStep(step);
  float2 sx = float2(texels.x, 0.0), sy = float2(0.0, texels.y);
  float3 l1 = RelativeAtUv(uv - sx), l2 = RelativeAtUv(uv - 2.0 * sx);
  float3 r1 = RelativeAtUv(uv + sx), r2 = RelativeAtUv(uv + 2.0 * sx);
  float3 u1 = RelativeAtUv(uv - sy), u2 = RelativeAtUv(uv - 2.0 * sy);
  float3 d1 = RelativeAtUv(uv + sy), d2 = RelativeAtUv(uv + 2.0 * sy);
  float el = OffLine(here, l1, l2), er = OffLine(here, r1, r2);
  float eu = OffLine(here, u1, u2), ed = OffLine(here, d1, d2);
  float3 dx = el < er ? here - l1 : r1 - here;
  float3 dy = eu < ed ? here - u1 : d1 - here;
  float3 normal = normalize(cross(dy, dx));
  if (dot(normal, -here) < 0.0)
    normal = -normal;
  // (the bend's sine: the distance off the line over the step to that side's
  // neighbour)
  float bendX = el < er ? el / max(length(here - l1), 1e-4) : er / max(length(r1 - here), 1e-4);
  float bendY = eu < ed ? eu / max(length(here - u1), 1e-4) : ed / max(length(d1 - here), 1e-4);
  clean = 1.0 - smoothstep(0.3, 0.6, max(bendX, bendY));
  return normal;
}

// How much the frame's surface at a point on it (at) drops away under the
// plane of a point of a puddle (here, normal): 0 none, 1 more than 7 % of
// the way to it under the plane (none under 2 %) and more than 1 cm (or 0.1 %
// of the distance) under it (none under half that), or the sky past an edge.
// (The least drop 2026-11-03, the user: "the rain still looks bad on top of
// vents/boxes/woodplanks", and the game's G-buffer saved at a roof box: its
// top is four faces leaning 1.6 - 4 degrees, meeting in an X. Across a fold
// the points 3 and 8 pixels off lay 2 - 7 % of the way under the plane, a
// millimetre or so, and the puddles stopped either side of it: dry lines
// from corner to corner. An edge, a curb, a step or a hole drops centimetres
// at least.)
float DropAt(float2 at, float3 here, float3 normal)
{
  bool sky;
  float3 there = RelativeAtTexel(at, sky) - here;
  if (sky)
    return 1.0;
  float under = -dot(there, normal);
  float least = max(0.01, length(here) * 0.001);
  return smoothstep(0.02, 0.07, under / max(length(there), 1e-3)) * smoothstep(least * 0.5, least, under);
}

// How much of a puddle can lie at a pixel by the frame round it (1 all, 0
// none): it ends before the surface drops away round it (an edge, a ledge or
// a curb's top, a hole, a step down, the sky past a roof's edge; see DropAt):
// none where one of 8 points 3 pixels round (at 4K, as much of the frame at
// any resolution: PixelScale) drops, and less of it the more of 4 more 8
// pixels round do (the water thinning to its edge). What
// rises from it doesn't end it (a wall, a curb up, a post, Faith or a person
// standing in it, anything in front of it): the water lies up against it.
// (2026-10-27, the user: "make better puddle edge detection algorithms",
// "there is a halo around her": any point 4 pixels round off the plane either
// way ended it, so every puddle stopped 4 pixels short of whatever stood in
// it or was in front of it, a dry ring round Faith's legs as plain as
// Wetness made the puddle.) The query's puddles at her feet (PsQuery) are
// told the same way, so her steps' rings and splashes agree with what's drawn.
float FlatAround(float2 uv, float3 here, float3 normal)
{
  float pixel = PixelScale();
  float2 nearStep = max(1.5, 3.0 * pixel) * g_Frame.xy, farStep = max(3.0, 8.0 * pixel) * g_Frame.xy;
  float nearDrop = 0.0, farDrops = 0.0;
  [unroll] for (int k = 0; k < 8; ++k)
    nearDrop = max(nearDrop, DropAt(uv + RingDirections[k] * nearStep, here, normal));
  [unroll] for (int f = 1; f < 8; f += 2)
    farDrops += DropAt(uv + RingDirections[f] * farStep, here, normal);
  return (1.0 - nearDrop) * (1.0 - 0.6 * saturate(farDrops / 1.5));
}

// How much rises from a puddle's plane (here, normal) within radius pixels
// round it on the frame (4 points, on their texels' centres; 5 cm, or 0.5 %
// of the distance, over it):
// something standing in it or in front of it, a wall's foot. 0 none, 1 two
// or more of them.
float RisesAround(float2 uv, float3 here, float3 normal, float distance, float radius)
{
  float rise = max(0.05, distance * 0.005);
  float n = 0.0;
  [unroll] for (int k = 0; k < 4; ++k)
  {
    bool sky;
    float3 p = RelativeAtTexel(uv + RingDirections[k * 2] * radius * g_Frame.xy, sky);
    if (!sky && dot(p - here, normal) > rise)
      n += 1.0;
  }
  return saturate(n / 2.0);
}

// How near the outline of something in front of a point's surface is on the
// frame (2026-11-01, the user: "Edge detection doesn't work": the wet look
// over the outlines of what's in front; and "When faith runs there is a weird
// blue hue around her body"). The game's outlines are soft over a pixel or two
// (its antialiasing), and smeared further where something moves (its motion
// blur), while the depth keeps them sharp: the ground's or a wall's pixels
// round something in front showed some of it, and were darkened and mirrored
// as the ground (the sky's blue mirrored over Faith's edge as she ran). 1
// where something in front of the surface (off its plane towards the camera
// by 5 cm or 0.5 % of the distance) and much nearer (by 25 cm or 5 % of the
// distance: an outline, not where a wall meets the ground) is within 3 or 8
// pixels round (4 points each, at 4K, x PixelScale), how much the pixel
// shows of it: its colour (colour, the frame's) taken between the surface's
// colour round it (those of the 8 points that aren't in front, nor the sky)
// and what's in front's own (those that are), from 15 % of the way on, all
// of it from 60 %; none where the two look alike. Until 2026-11-02 it was 1
// within 3 pixels and 0.6 within 8 whatever the pixel showed, so the ground
// round everyone kept dry in a band that wide: a light halo round Faith and
// the KrugerSec on the dark wet floor (the user's screenshot, "there is a
// halo around units faith as well", "with rain enabled"). With none of the
// surface round it on the frame (a gap of a few pixels), as much as its
// colour is what's in front's.
float NearerAround(float2 uv, float3 here, float3 normal, float distance, float3 colour)
{
  float pixel = PixelScale();
  float rise = max(0.05, distance * 0.005);
  float nearerThan = distance - max(0.25, distance * 0.05);
  float inner = max(1.0, 3.0 * pixel), outer = max(2.0, 8.0 * pixel);
  uint nearerBits = 0, seenBits = 0;   // per point (bit k)
  [unroll] for (int k = 0; k < 8; ++k)
  {
    bool sky;
    float3 p = RelativeAtTexel(uv + RingDirections[k < 4 ? k * 2 : (k - 4) * 2 + 1] * (k < 4 ? inner : outer) * g_Frame.xy, sky);
    seenBits |= sky ? 0u : 1u << k;
    nearerBits |= !sky && dot(p - here, normal) > rise && dot(p, p) < nearerThan * nearerThan ? 1u << k : 0u;
  }
  if (nearerBits == 0)
    return 0.0;
  // What the frame shows at those points (their texels, as the depth's)
  float2 last = 1.0 / g_Frame.xy - 1.0;
  float3 front = 0.0, back = 0.0;
  float fronts = 0.0, backs = 0.0;
  [unroll] for (int j = 0; j < 8; ++j)
  {
    float2 at = uv + RingDirections[j < 4 ? j * 2 : (j - 4) * 2 + 1] * (j < 4 ? inner : outer) * g_Frame.xy;
    float3 c = g_Scene.Load(int3(int2(min(saturate(at) / g_Frame.xy, last)), 0)).rgb;
    if (nearerBits & (1u << j))
    {
      front += c;
      fronts += 1.0;
    }
    else if (seenBits & (1u << j))
    {
      back += c;
      backs += 1.0;
    }
  }
  front /= fronts;
  if (backs < 0.5)
    return 1.0 - smoothstep(0.03, 0.12, length(colour - front));
  back /= backs;
  float3 apart = front - back;
  float contrast = dot(apart, apart);
  if (contrast < 1e-5)
    return 0.0;
  float share = dot(colour - back, apart) / contrast;
  return smoothstep(0.15, 0.6, share) * smoothstep(0.001, 0.006, contrast);
}

// The running water's rivulets: this far apart (m)
static const float RunningSpacing = 0.12;

// How much the frame's surface at uv is a slope water runs down (2026-10-26,
// the user: "Slopes are tweaking and flickering ... it should be only on
// slopes": the running water was drawn on every wall and, far off, as a
// sheen, from a 1-pixel normal): a plane (the points 4 pixels round it on it,
// within 1 - 3 % of their distance) leaning 14 - 41 degrees (none under 8 or
// past 53: not on buildings' walls or slanted fronts, the user, 2026-10-26: "Water on
// slopes can move however remove it from buildings"), near enough that its rivulets are at least 3 pixels apart (all of
// it from 6). normal: the plane's, from those 4-pixel steps (the nearer side
// each way), steadier than FrameNormal's.
float SlopeAt(float2 uv, float3 here, float pixelsPerMetre, out float3 normal)
{
  normal = float3(0.0, 1.0, 0.0);
  // (in a 4K frame's pixels, and its points 4 of them round on their texels'
  // centres, so it's the same at any resolution: PixelScale, RelativeAtTexel)
  float pixel = PixelScale();
  float close = saturate(pixelsPerMetre / pixel * RunningSpacing / 3.0 - 1.0);
  if (close <= 0.0)
    return 0.0;
  float2 sx = float2(4.0 * pixel * g_Frame.x, 0.0), sy = float2(0.0, 4.0 * pixel * g_Frame.y);
  bool sky;
  float3 l = RelativeAtTexel(uv - sx, sky) - here, r = RelativeAtTexel(uv + sx, sky) - here;
  float3 u = RelativeAtTexel(uv - sy, sky) - here, d = RelativeAtTexel(uv + sy, sky) - here;
  bool leftNear = length(l) < length(r), upNear = length(u) < length(d);
  float3 dx = leftNear ? -l : r, dy = upNear ? -u : d;
  normal = normalize(cross(dy, dx));
  if (dot(normal, -here) < 0.0)
    normal = -normal;
  // (not leaning as a slope does, level ground or a wall: none, as before,
  // and its other 6 points not looked at; see KeptDryAt's note on the cost)
  float lean = smoothstep(0.6, 0.75, normal.y) * (1.0 - smoothstep(0.97, 0.99, normal.y));
  if (lean <= 0.0)
    return 0.0;
  // The other two on its plane too
  float3 ox = leftNear ? r : l, oy = upNear ? d : u;
  float off = max(abs(dot(ox, normal)) / max(length(ox), 1e-4), abs(dot(oy, normal)) / max(length(oy), 1e-4));
  // (softer towards its edges, 2026-10-27, the user: "the shader should be
  // more smoothed out around the edges": a crease or a drop 4 pixels off cut
  // it within a pixel or two, a hard line; now it thins over 1 - 4 % off its
  // plane there, and by up to 70 % more as 4 points 10 pixels round leave it,
  // so it fades out over several pixels before an edge)
  float plane = saturate(1.0 - (off - 0.01) * 33.0);
  float2 fx = sx * 2.5, fy = sy * 2.5;
  float3 f1 = RelativeAtTexel(uv - fx, sky) - here, f2 = RelativeAtTexel(uv + fx, sky) - here;
  float3 f3 = RelativeAtTexel(uv - fy, sky) - here, f4 = RelativeAtTexel(uv + fy, sky) - here;
  float far = max(max(abs(dot(f1, normal)) / max(length(f1), 1e-4), abs(dot(f2, normal)) / max(length(f2), 1e-4)),
    max(abs(dot(f3, normal)) / max(length(f3), 1e-4), abs(dot(f4, normal)) / max(length(f4), 1e-4)));
  plane *= 1.0 - 0.7 * saturate((far - 0.015) * 15.0);
  return close * plane * lean;
}

// The frame's surface seen through a point (relative to the camera): false
// off the frame or behind the camera; seen through to the sky, a point far
// behind it
bool SurfaceThrough(float3 rel, out float3 surface)
{
  surface = rel;
  float4 clip = mul(float4(rel, 1.0), g_ViewProjection);
  if (clip.w <= 0.1)
    return false;
  float2 at = float2(clip.x / clip.w * 0.5 + 0.5, 0.5 - clip.y / clip.w * 0.5);
  if (any(at < 0.0) || any(at > 1.0))
    return false;
  float d = DepthAt(at);
  surface = IsSky(d) ? rel * 1000.0 : RelativeAt(at, d);
  return true;
}

// Whether a puddle can lie on the frame's surface here, from the surface
// round it (2026-10-26, the user: "rain puddles should not be forming on
// metal stairs with holes", with a screenshot of a perforated metal walkway
// and its stairs: puddles were drawn on the metal between the holes, which
// are 10 - 15 pixels wide there, so FlatAround's 4 pixel points fell on
// metal). 0 on anything with holes, on stairs and on narrow tops, where the
// water would run off; 1 on solid, level ground.
// - Holes: 24 points on three rings round it in its plane, 5, 11 and 20 cm
//   (never under 3, 6 and 12 pixels): those seen through to more than about
//   4 cm (or 0.2 % of the distance) behind its plane are holes or an edge
//   (soft, see below).
//   (The third ring 2026-10-26, the user: "metal stairs with holes still
//   have puddles": where the metal between the holes is wider than the
//   second ring, all 16 points could fall on metal.)
// - Steps: the surface 40 cm away both ways along four directions in its
//   plane: a drop on one side with the other side not level either (a riser
//   up or another drop) is a stair's tread or a narrow top.
// The frame's surface seen through a point (clip: the point in clip space,
// rel the point): of the 2 x 2 depth texels round where it falls, the
// farthest (Gather, where the depth copy can be: g_Options.x), so a hole a
// pixel across is seen whichever way the game's jitter moved the samples;
// else its own texel. False off the frame or behind the camera; seen
// through to the sky, far behind it.
bool FarthestThrough(float4 clip, float3 rel, float2 depthSize, out float3 surface)
{
  surface = rel;
  if (clip.w <= 0.1)
    return false;
  float2 at = float2(clip.x / clip.w * 0.5 + 0.5, 0.5 - clip.y / clip.w * 0.5);
  if (any(at < 0.0) || any(at > 1.0))
    return false;
  float d;
  if (g_Options.x > 0.5)
  {
    float4 four = g_SceneDepth.Gather(g_Linear, at / g_Frame.xy * g_Depth.xy / depthSize);
    d = g_Depth.z > 0.5 ? min(min(four.x, four.y), min(four.z, four.w)) : max(max(four.x, four.y), max(four.z, four.w));
  }
  else
    d = DepthAt(at);
  surface = IsSky(d) ? rel * 1000.0 : RelativeAt(at, d);
  return true;
}

// Holes (2026-10-26, the user: "Water still gathers on stairs with holes /
// metal with wholes"): the first soft version needed a quarter of the
// points' weight seen through before a puddle went, so metal with fewer
// holes kept a faint one. Now an eighth does (6 of the 48), and each point
// reads the farthest of the 2 x 2 depth texels round it (FarthestThrough):
// a hole is seen however the game's jitter moved the samples, so it doesn't
// flicker at its border as the single texel did (the user, before: "There
// is still some water flickering"). Each point counts as much as it lies
// under the plane (0.75 - 1.5 x "below"), the near ring three times, the
// middle twice, the far once. Its points are turned with the world's x, and
// put on the frame from the point's and the plane's axes in clip space (one
// matrix product each, not one a point: the performance, 2026-10-26). The
// stairs' drops count as much likewise (0.75 - 1.5 steps).
float PuddleSurface(float3 here, float3 normal, float distance)
{
  float pixelsPerMetre = g_ProjectionXY.y * 0.5 / (max(distance, 0.1) * g_Frame.y);
  float3 a = float3(1.0, 0.0, 0.0) - normal * normal.x;
  a = dot(a, a) > 1e-4 ? normalize(a) : normalize(cross(normal, float3(0.0, 0.0, 1.0)));
  float3 b = cross(normal, a);
  float below = max(0.04, distance * 0.002);
  uint width, height;
  g_SceneDepth.GetDimensions(width, height);
  float2 depthSize = float2(width, height);
  float4 clipHere = mul(float4(here, 1.0), g_ViewProjection);
  float4 clipA = mul(float4(a, 0.0), g_ViewProjection), clipB = mul(float4(b, 0.0), g_ViewProjection);
  float holes = 0.0;
  // (each ring's points read 4 together and counted after, then whether to
  // go on: point by point, each read waited for the one before to tell
  // whether to go on, 24 reads one after another for every puddle's pixel,
  // much of the pass's cost at 4K; 2026-11-02, the user: "Rain still drops
  // your fps by a lot". The same count, and the same 0 as soon as it's 6:
  // each point only adds)
  [loop] for (int ring = 0; ring < 3; ++ring)
  {
    // (the rings' least sizes in pixels as much of the frame at any
    // resolution: PixelScale)
    float pixels = PixelScale() / pixelsPerMetre;
    float radius = ring == 0 ? max(0.05, 3.0 * pixels) : ring == 1 ? max(0.11, 6.0 * pixels) : max(0.2, 12.0 * pixels);
    float turn = ring == 0 ? 0.0 : ring == 1 ? 0.3927 : 0.1963;
    float weight = 3.0 - ring;
    [unroll] for (int part = 0; part < 2; ++part)
    {
      float some = 0.0;
      [unroll] for (int k = part * 4; k < part * 4 + 4; ++k)
      {
        float angle = turn + k * 0.7854;
        float c = cos(angle), sn = sin(angle);
        float3 surface;
        bool through = FarthestThrough(clipHere + (clipA * c + clipB * sn) * radius, here + (a * c + b * sn) * radius,
          depthSize, surface);
        some += through ? weight * smoothstep(below * 0.75, below * 1.5, -dot(surface - here, normal)) : 0.0;
      }
      holes += some;
      if (holes >= 6.0)
        return 0.0;
    }
  }
  float solid = saturate(1.0 - holes / 6.0);
  float step = max(0.06, distance * 0.003);
  float stairs = 0.0;
  // (read together, as the rings above)
  [unroll] for (int d = 0; d < 4; ++d)
  {
    float angle = d * 0.7854;
    float3 offset = (a * cos(angle) + b * sin(angle)) * 0.4;
    float3 one, other;
    if (!SurfaceThrough(here + offset, one) || !SurfaceThrough(here - offset, other))
      continue;
    float h1 = dot(one - here, normal), h2 = dot(other - here, normal);
    float first = smoothstep(step * 0.75, step * 1.5, -h1) * smoothstep(step * 0.75, step * 1.5, abs(h2));
    float second = smoothstep(step * 0.75, step * 1.5, -h2) * smoothstep(step * 0.75, step * 1.5, abs(h1));
    stairs = max(stairs, max(first, second));
  }
  if (stairs > 0.999)
    return 0.0;
  // Broad enough (2026-10-26, the user: "add a better detection / mesh
  // detection for what should be wet and have a puddle"): a puddle lies on
  // what goes on flat for a metre round (the ground, a roof, a floor), not on
  // a small top the water runs off (a box, a table, a bench, a car's roof, a
  // ledge or a wall's top): the surface 1 m away in 8 directions in its
  // plane, those that drop away (more than 10 cm, or 0.4 % of the distance,
  // under its plane: an edge) counted; from 3 of them on, less of a puddle,
  // none from 4 (a sidewalk's curb is one, a walkway between two drops two).
  // A wall rising beside it doesn't count, nor what's off the frame or
  // behind something.
  float drop = max(0.1, distance * 0.004);
  float edges = 0.0;
  [unroll] for (int e = 0; e < 8; ++e)
  {
    float angle = e * 0.7854 + 0.1963;
    float3 away;
    if (SurfaceThrough(here + (a * cos(angle) + b * sin(angle)) * 1.0, away))
      edges += smoothstep(drop * 0.75, drop * 1.5, -dot(away - here, normal));
  }
  float broad = 1.0 - smoothstep(2.5, 3.5, edges);
  return solid * (1.0 - stairs) * broad;
}

// The ground's nodes (2026-11-03, the user: "the rain still looks bad on top
// of vents/boxes/woodplanks", "It's the same" after B67B44C4FAE7, and the
// game's G-buffer saved at a roof box's top: hard-edged rectangles of puddle
// and of none on it). PuddleSurface's tests read points at fixed offsets
// round each pixel (rings 5 - 20 cm round, 40 cm and 1 m away in its plane),
// and each counts as dropping away or not; as a pixel moves, the point goes
// over a box's edge at once, and the puddle steps there: the box's outline
// moved by each offset, rectangles 99.9 % of the steps the dump had (the
// SSR session's scratchpad model of these tests on the dump's depth matched
// them pixel for pixel). Here what the 1 m test asks (does the surface go on
// flat for a metre round, or is it a small top the water runs off) is worked
// out at fixed points of the world instead, once a frame (NodesPs), and each
// pixel takes it from the 4 x 4 of them round it, smoothly (GroundGoesOn):
// what's read no longer moves with the pixel, so nothing steps.
// Each node: a world lattice point g_NodeGrid.z apart (GbufferWet.cpp: 0.5 m,
// 256 a side round the camera), its surface found down its vertical line
// (probes from 2 m over the camera to 50 m under it, the first the frame
// shows behind its surface; then twice onto the surface at its own x and z),
// and how broad that surface is round it: PuddleSurface's 8 points 1 m round,
// level with it, each that drops away counted (none from 3.5).
static const float NodeProbes[10] = { 2.0, 0.0, -1.0, -2.0, -4.0, -7.0, -12.0, -20.0, -32.0, -50.0 };

float2 NodesPs(float4 position : SV_POSITION) : SV_Target0
{
  float2 xz = g_NodeGrid.xy + floor(position.xy) * g_NodeGrid.z - g_NodeOrigin.xz;
  float height = 0.0;
  bool found = false;
  [loop] for (int k = 0; k < 10; ++k)
  {
    float3 p = float3(xz.x, NodeProbes[k], xz.y), seen;
    if (SurfaceThrough(p, seen) && length(seen) < length(p) - (0.02 + 0.002 * length(p)))
    {
      height = seen.y;
      found = true;
      break;
    }
  }
  if (!found)
    return float2(0.0, -1.0);
  [unroll] for (int r = 0; r < 2; ++r)
  {
    float3 seen;
    if (SurfaceThrough(float3(xz.x, height, xz.y), seen))
      height = seen.y;
  }
  float fall = max(0.1, length(float3(xz.x, height, xz.y)) * 0.004);
  float edges = 0.0;
  [unroll] for (int e = 0; e < 8; ++e)
  {
    float angle = e * 0.7854 + 0.1963;
    float3 seen;
    if (SurfaceThrough(float3(xz.x + cos(angle), height, xz.y + sin(angle)), seen))
      edges += smoothstep(fall * 0.75, fall * 1.5, height - seen.y);
  }
  return float2(height + g_NodeOrigin.y, 1.0 - smoothstep(2.5, 3.5, edges));
}

// A cubic B-spline's 4 weights at f (0 - 1) between the second and the third
// point (they add up to 1, and change smoothly as f does and from one span to
// the next)
float4 BSplineWeights(float f)
{
  float g = 1.0 - f;
  return float4(g * g * g, (3.0 * f - 6.0) * f * f + 4.0, ((-3.0 * f + 3.0) * f + 3.0) * f + 1.0, f * f * f) / 6.0;
}

// How much the ground at a point (world; distance from the camera) goes on
// round it at its own level, from the 4 x 4 nodes round it (see NodesPs),
// weighed by a cubic B-spline: a node higher than it (a wall, a step up,
// Faith, what stands there or in front) all of it, the water lies against
// it; one level with it (within 10 cm or 0.4 % of the distance, all of it
// past 2.5 times that) its broadness; one lower (past an edge, a drop) none.
// 1 on open ground, less towards the edge of a top it drops away from, none
// on a small one; 1 where a node isn't known (none seen there, off the
// lattice), and towards the lattice's edge (from 49 m round the camera) as
// if none were, so nothing changes where it moves on.
float GroundGoesOn(float3 world, float distance)
{
  float2 g = (world.xz - g_NodeGrid.xy) / g_NodeGrid.z;
  float2 cell = floor(g);
  float4 wx = BSplineWeights(g.x - cell.x), wz = BSplineWeights(g.y - cell.y);
  float tolerance = 0.1 + 0.004 * distance;
  int side = (int)g_NodeGrid.w;
  float on = 0.0;
  [unroll] for (int i = 0; i < 4; ++i)
  {
    [unroll] for (int k = 0; k < 4; ++k)
    {
      int2 at = int2(cell) + int2(i - 1, k - 1);
      float node = 1.0;
      if (all(at >= 0) && all(at < side))
      {
        float2 n = g_Nodes.Load(int3(at, 0));
        if (n.y >= 0.0)
        {
          float rising = smoothstep(tolerance, tolerance * 2.5, n.x - world.y);
          float level = 1.0 - smoothstep(tolerance, tolerance * 2.5, world.y - n.x);
          node = rising + (1.0 - rising) * level * n.y;
        }
      }
      on += wx[i] * wz[k] * node;
    }
  }
  float2 centre = g_NodeGrid.xy + 0.5 * g_NodeGrid.w * g_NodeGrid.z;
  return lerp(on, 1.0, smoothstep(0.38, 0.47, length(world.xz - centre) / (g_NodeGrid.w * g_NodeGrid.z)));
}

// How much a height h (m) is a stair's (from step, 0.75 - 1.5 x, to riser,
// none from 1.5 x it)
float StepSized(float h, float step, float riser)
{
  return smoothstep(step * 0.75, step * 1.5, h) * (1.0 - smoothstep(riser, riser * 1.5, h));
}

// PuddleSurface for the puddles while the ground's nodes are this frame's
// (g_Options.w; 2026-11-03, see NodesPs: none of its tests step at a box's
// edge any more):
// - holes: each ring's points in opposite pairs, a pair counted only where
//   both lie under the plane (metal with holes: they're all round), so an
//   edge or a gap on one side (a box's, between two boxes) isn't a hole; none
//   from 3 (6 before: two of a pair now);
// - stairs: the drop on one side and the other side's step only as high as a
//   stair's (6 cm or 0.3 % of the distance to 35 cm or 0.7 %): a box's edge
//   with a vent rising behind it took the box's corners for treads;
// - broad: from the nodes, GroundGoesOn (world: the point's).
float PuddleGround(float3 here, float3 normal, float distance, float3 world)
{
  float pixelsPerMetre = g_ProjectionXY.y * 0.5 / (max(distance, 0.1) * g_Frame.y);
  float3 a = float3(1.0, 0.0, 0.0) - normal * normal.x;
  a = dot(a, a) > 1e-4 ? normalize(a) : normalize(cross(normal, float3(0.0, 0.0, 1.0)));
  float3 b = cross(normal, a);
  float below = max(0.04, distance * 0.002);
  uint width, height;
  g_SceneDepth.GetDimensions(width, height);
  float2 depthSize = float2(width, height);
  float4 clipHere = mul(float4(here, 1.0), g_ViewProjection);
  float4 clipA = mul(float4(a, 0.0), g_ViewProjection), clipB = mul(float4(b, 0.0), g_ViewProjection);
  float holes = 0.0;
  [loop] for (int ring = 0; ring < 3; ++ring)
  {
    float pixels = PixelScale() / pixelsPerMetre;
    float radius = ring == 0 ? max(0.05, 3.0 * pixels) : ring == 1 ? max(0.11, 6.0 * pixels) : max(0.2, 12.0 * pixels);
    float turn = ring == 0 ? 0.0 : ring == 1 ? 0.3927 : 0.1963;
    float weight = 3.0 - ring;
    float under[8];
    [unroll] for (int k = 0; k < 8; ++k)
    {
      float angle = turn + k * 0.7854;
      float c = cos(angle), sn = sin(angle);
      float3 surface;
      bool through = FarthestThrough(clipHere + (clipA * c + clipB * sn) * radius, here + (a * c + b * sn) * radius,
        depthSize, surface);
      under[k] = through ? smoothstep(below * 0.75, below * 1.5, -dot(surface - here, normal)) : 0.0;
    }
    [unroll] for (int j = 0; j < 4; ++j)
      holes += weight * 2.0 * min(under[j], under[j + 4]);
    if (holes >= 3.0)
      return 0.0;
  }
  float solid = saturate(1.0 - holes / 3.0);
  float step = max(0.06, distance * 0.003), riser = max(0.35, distance * 0.007);
  float stairs = 0.0;
  [unroll] for (int d = 0; d < 4; ++d)
  {
    float angle = d * 0.7854;
    float3 offset = (a * cos(angle) + b * sin(angle)) * 0.4;
    float3 one, other;
    if (!SurfaceThrough(here + offset, one) || !SurfaceThrough(here - offset, other))
      continue;
    float h1 = dot(one - here, normal), h2 = dot(other - here, normal);
    float first = StepSized(-h1, step, riser) * StepSized(abs(h2), step, riser);
    float second = StepSized(-h2, step, riser) * StepSized(abs(h1), step, riser);
    stairs = max(stairs, max(first, second));
  }
  if (stairs > 0.999)
    return 0.0;
  return solid * (1.0 - stairs) * GroundGoesOn(world, distance);
}

// How much of a ripple gets from where it started (from: a ring's middle,
// the nearest place on her slide's path; relative to the camera) to a point
// on the frame (here) over the same surface (2026-10-27, the user: "Ripple
// effect should respect edges and not go over them": they went on wherever
// the ground was within 30 cm of their height, down curbs and steps, over
// ledges' and roofs' edges and gaps; the game's own water's on walls and
// whatever stood there): the frame's surface at points on the straight way
// between them (one every 30 cm, 2 - 8); one seen through to more than about
// 5 cm (or 0.2 % of the distance) under its point, farther than it, is an
// edge between them (a drop, a gap: nothing there), and the ripple stops
// there, softly. What's in front of a point (Faith's legs, a wall or a step
// rising) doesn't stop it, nor what's off the frame. Within 6 cm of the
// point's height its start counts as level with it (her steps, landings and
// slides are where the game found the ground, CT.log 2026-10-27 22:59; other
// people's from their moves maybe a little off).
// (least: the least drop that's an edge, 0.05 m; a landing pad's sagging
// tarp more, see StepRings)
float RingReaches(float3 here, float3 from, float distance, float least)
{
  if (abs(from.y - here.y) < 0.06)
    from.y = here.y;
  float3 way = from - here;
  float length2 = dot(way, way);
  if (length2 < 0.01)
    return 1.0;
  float below = max(least, distance * 0.002);
  int n = (int)clamp(ceil(sqrt(length2) / 0.3), 2.0, 8.0);
  float reach = 1.0;
  // (its points read together, not each waiting for the one before: see
  // PuddleSurface's rings. The same: each point only takes away, so once
  // under 0.001 it stays under, 0 as before)
  [unroll] for (int k = 1; k <= 8; ++k)
  {
    if (k > n)
      break;
    float3 at = here + way * (k / (n + 1.0));
    float3 seen;
    bool through = SurfaceThrough(at, seen) && dot(seen, seen) > dot(at, at);
    reach *= through ? 1.0 - smoothstep(below * 0.75, below * 1.5, at.y - seen.y) : 1.0;
  }
  return reach < 0.001 ? 0.0 : reach;
}

// Faith's steps in a puddle (2026-10-26, the user: "the rain peddles should
// react with faith's shoes and create new ones"; ripples and footprints
// chosen). From each step (g_StepRing) a ring goes out at about 0.75 m/s,
// a short train of waves 8.4 cm apart behind its front, fading out over
// g_StepCounts.z seconds and as it spreads: the water's slope (x, z), as
// Ripples gives it. Other people's steps and the cars' wheels the same
// (g_OtherRing).
// One ring (sizeLife: its g_StepRingSize or g_OtherRingSize). A landing's
// (2026-10-26, the user: jumping should make a bigger ripple): stronger, out
// faster and further, its waves longer and lasting longer (size 1, a
// step's, as before).
// (rise: how far over or under its middle a point can be, 0.3 m; on a
// landing pad's sagging tarp more)
float2 StepRing(float4 ring, float2 sizeLife, float3 rel, float rise)
{
  float size = sizeLife.x;
  float2 d = rel.xz - ring.xz;
  float r = length(d);
  float front = 0.06 * size + 0.75 * ring.w * (0.7 + 0.3 * size);
  float behind = front - r;
  if (abs(rel.y - ring.y) > rise || behind < -0.02 || behind > 0.3 * size || r < 1e-3)
    return 0.0;
  float fade = saturate(1.0 - ring.w / sizeLife.y);
  float a = 0.5 * size * fade * fade / (1.0 + 4.0 * front) * exp(-behind * behind / (0.012 * size));
  return d / r * a * sin(behind * 75.0 / sqrt(size));
}

// All the rings at a point, each as much as there's water there to ring
// (film: the tools' puddle or the water down a slope); Faith's on the game's
// own water (g_StepRingSize.z 1, WetLook.cpp's MostGameWaterRings: its puddle
// decals, where the tools draw none) without it, on level ground (level),
// within 0.9 - 1.4 m of where she stepped (2026-10-27, the user: "dropping
// from a big jump should create a ripple if there is a puddle on the landing
// pad"). Each only as far as it gets over the same surface (RingReaches).
// Her landings on the Runners' landing pads (g_StepRingSize.z 2, WetLook.cpp's
// MostPadRings, 2026-10-28, the user: "landing on a jumppad should leave a
// ripple", "i mean landingpads"): on whatever of the tarp faces up (it sags,
// too far from level for the rest), up to 0.8 m over or under where she came
// down, its folds and sag (up to 12 cm off a straight line) not taken for an
// edge; its own edge, where it drops to the roof, is. up: the surface's
// normal's y.
float2 StepRings(float3 rel, float film, float up, float distance)
{
  float2 s = 0.0;
  float level = smoothstep(0.95, 0.98, up), facing = smoothstep(0.6, 0.8, up);
  int n = (int)g_StepCounts.x;
  [loop] for (int k = 0; k < n; ++k)
  {
    float4 sizeLife = g_StepRingSize[k];
    bool pad = sizeLife.z > 1.5;
    float on = film;
    if (pad)
      on = max(film, facing);
    else if (sizeLife.z > 0.5)
      on = max(film, level * (1.0 - smoothstep(0.9, 1.4, length(rel.xz - g_StepRing[k].xz))));
    if (on > 0.001)
    {
      float2 ring = StepRing(g_StepRing[k], sizeLife.xy, rel, pad ? 0.8 : 0.3);
      if (any(ring != 0.0))
        s += ring * on * RingReaches(rel, g_StepRing[k].xyz, distance, pad ? 0.12 : 0.05);
    }
  }
  int m = (int)g_OtherCounts.x;
  [loop] for (int j = 0; j < m; ++j)
  {
    float2 ring = StepRing(g_OtherRing[j], g_OtherRingSize[j].xy, rel, 0.3);
    if (any(ring != 0.0))
      s += ring * film * RingReaches(rel, g_OtherRing[j].xyz, distance, 0.05);
  }
  return s;
}

// Faith sliding through the water (2026-10-26, the user: "faith sliding
// should create a slide ripple", then "water slide is still not visible it
// should push water asside and create ripples from faiths body": the first
// two builds rang only round each slide effect's place, behind her by the
// time they spread, and darkened a streak). Her slide's path (g_SlideRing in
// order, oldest first; points of one slide within 1.5 m and 0.4 s of each
// other), for a point the nearest place on it and how long ago she was there:
// - x: how much of the water is pushed out of her way there: her body 30 cm
//   each side of the path, coming back over SlideBackSeconds;
// - yz: the slope of the waves her body sent out (x, z, as Ripples gives
//   it): each stretch of the path sends waves out to both sides, 1 m/s at
//   first and slowing (its front 0.15 + 1.25 sqrt(age) m out: 3.2 m at 6 s),
//   more crests and longer ones as they spread, dying away over
//   SlideWaveSeconds, so behind her they spread in a V (the wake), and where
//   she is now (the last 0.3 s, within 0.5 m) the water is churned.
// (2026-10-26, the user: "water slide ripple effect should take a while to
// dissapear and should ripple for a lot longer": the water came back over 2
// s, the waves went out at 0.8 m/s and were gone in 1.6 s, out to 1.4 m.)
static const float SlideBackSeconds = 4.0, SlideWaveSeconds = 6.0;

// Faith's body where it went through the water (2026-10-27, the user:
// "waterslideing should have faiths model outline just like snow and
// smoothed out"; SnowBodies.cpp's stamps while the wet look is on,
// g_SnowStampCount.z 2: her own mesh, posed as the game had her, within 6 cm
// of the ground, smoothed 3 cm): x how much of her went through at a point (0
// - 1); y 1 where a stamp of hers is (its shape stands for SlideWake's band
// there), 0 where none is
float2 WaterStampAt(float3 rel)
{
  float2 best = 0.0;
  if (g_SnowStampCount.z < 1.5)
    return best;
  int n = (int)g_SnowStampCount.x;
  [loop] for (int k = 0; k < n; ++k)
  {
    float4 s = g_SnowStamp[k], a = g_SnowStampAxis[k];
    float2 d = rel.xz - s.xz;
    float2 t = float2(dot(d, float2(-a.y, a.x)), dot(d, a.xy)) / 0.02;
    if (any(t < 0.0) || t.x > 128.0 || t.y > 256.0 || abs(rel.y - (s.y + a.z * t.y * 0.02)) > 0.3)
      continue;
    best.x = max(best.x, g_SnowStamps.SampleLevel(g_Linear, float3(t / float2(128.0, 256.0), a.w), 0));
    best.y = 1.0;
  }
  return best;
}

float3 SlideWake(float3 rel, float distance)
{
  float best = 1e9, age = 0.0;
  float2 away = 0.0;
  float3 source = rel;
  int n = (int)g_SlideCount.x;
  [loop] for (int k = 0; k + 1 < n; ++k)
  {
    float4 p = g_SlideRing[k], q = g_SlideRing[k + 1];
    float3 pq = q.xyz - p.xyz;
    if (dot(pq, pq) > 1.5 * 1.5 || abs(p.w - q.w) > 0.4)
      continue;
    float2 pr = rel.xz - p.xz;
    float u = saturate(dot(pr, pq.xz) / max(dot(pq.xz, pq.xz), 1e-4));
    float3 on = p.xyz + pq * u;
    if (abs(rel.y - on.y) > 0.3)
      continue;
    float2 off = rel.xz - on.xz;
    float dist = length(off);
    if (dist < best)
    {
      best = dist;
      age = lerp(p.w, q.w, u);
      away = off / max(dist, 1e-3);
      source = on;
    }
  }
  if (best > 4.2)
    return 0.0;
  // (only as far as it gets from her path over the same surface: see
  // RingReaches)
  float reach = RingReaches(rel, source, distance, 0.05);
  if (reach < 0.001)
    return 0.0;
  // (her body's own shape where it went through the water, WaterStampAt;
  // the band 30 cm each side of the path where her body wasn't read)
  float2 body = WaterStampAt(rel);
  float shape = body.y > 0.5 ? saturate(body.x * 1.5) : saturate(1.0 - best / 0.3);
  float pushed = shape * (1.0 - smoothstep(0.0, SlideBackSeconds, age));
  float2 waves = 0.0;
  float front = 0.15 + 1.25 * sqrt(age);
  float behind = front - best;
  float band = 0.35 + 0.12 * age;
  if (age < SlideWaveSeconds && behind > -0.02 && behind < band)
  {
    float fade = 1.0 - smoothstep(0.0, SlideWaveSeconds, age);
    float a = 0.8 * fade / (1.0 + front) * exp(-behind * behind / (0.015 + 0.012 * age));
    waves = away * a * sin(behind * 90.0 / (1.0 + 0.2 * age));
  }
  if (age < 0.3 && best < 0.5)
  {
    float2 w = rel.xz + g_Camera.xz;
    float churn = (1.0 - age / 0.3) * saturate(1.0 - best / 0.5);
    waves += (float2(Noise(w * 14.0 + g_Camera.w * 6.0), Noise(w * 14.0 + 31.0 - g_Camera.w * 6.0)) - 0.5) * 1.2 * churn;
  }
  return float3(pushed, waves) * reach;
}

// A smooth minimum of two distances (k: how far round the join is rounded)
float SmoothMin(float a, float b, float k)
{
  float h = max(k - abs(a - b), 0.0) / k;
  return min(a, b) - h * h * k * 0.25;
}

// The KrugerSec going down in the rain's water (2026-10-28, the user: "ksec
// ragdoll/dying should leave behind their model ripples"; WetLook.cpp's
// MostRippleBodies, SnowBodies.cpp's LyingBodies: his body as capsules from
// his bones, their radii from his mesh): waves out from his body's outline
// over the ground (the distance from it, the capsules' nearest, smoothly
// joined 8 cm round), as from her slide's path (see SlideWake): one train
// from when he lay down, a smaller one from his last knock (a bounce, a
// kick), each out at about 1 m/s at first and slowing, more crests and
// longer ones as they spread, dying away over SlideWaveSeconds; only as far
// as they get over the same surface (RingReaches). What of him is more than
// 30 cm over the ground here (an arm raised, a body on a ledge) sends none.
// x: 1 under him (the water pushed away), yz: the waves' slope (x, z).
float3 BodyRipplesAt(float3 rel, float distance)
{
  float3 result = 0.0;
  int n = (int)g_BodyRippleCount.x;
  [loop] for (int b = 0; b < n; ++b)
  {
    float4 at = g_BodyRippleAt[b];
    float2 o = rel.xz - at.xz;
    if (dot(o, o) > at.w * at.w || abs(rel.y - at.y) > 1.5)
      continue;
    float4 info = g_BodyRipple[b];
    int first = (int)info.x, last = (int)(info.x + info.y);
    float outline = 1e9, nearest = 1e9;
    float2 away = 0.0;
    float3 source = rel;
    [loop] for (int k = first; k < last; ++k)
    {
      float4 a = g_BodySegA[k];
      float3 ab = g_BodySegB[k].xyz - a.xyz;
      float u = saturate(dot(rel.xz - a.xz, ab.xz) / max(dot(ab.xz, ab.xz), 1e-4));
      float3 c = a.xyz + ab * u;
      if (c.y - a.w > rel.y + 0.3)
        continue;
      float2 off = rel.xz - c.xz;
      float len = length(off);
      float d = len - a.w;
      if (d < nearest)
      {
        nearest = d;
        away = off / max(len, 1e-3);
        source = float3(c.x, c.y - a.w, c.z);
      }
      outline = outline > 1e8 ? d : SmoothMin(outline, d, 0.08);
    }
    if (outline > 1e8)
      continue;
    if (outline < 0.0)
    {
      result.x = max(result.x, saturate(-outline / 0.05));
      continue;
    }
    float reach = RingReaches(rel, source, distance, 0.05);
    if (reach < 0.001)
      continue;
    [unroll] for (int t = 0; t < 2; ++t)
    {
      float age = t == 0 ? info.z : info.w;
      if (age >= SlideWaveSeconds)
        continue;
      float front = 0.05 + 1.25 * sqrt(age);
      float behind = front - outline;
      float band = 0.35 + 0.12 * age;
      if (behind > -0.02 && behind < band)
      {
        float fade = 1.0 - smoothstep(0.0, SlideWaveSeconds, age);
        float strength = (t == 0 ? 1.0 : 0.7) * fade / (1.0 + front) * exp(-behind * behind / (0.015 + 0.012 * age));
        result.yz += away * strength * sin(behind * 90.0 / (1.0 + 0.2 * age)) * reach;
      }
    }
  }
  return result;
}

// Where bodies pressed the rain's water (their stamps, WaterStampAt's: hers
// as she slid, a KrugerSec's where he lay; SnowBodies.cpp), each as much as
// it has left (g_SnowStamp.w, gone over WetLook.cpp's WaterStampSeconds): his
// body's shape left in the water when he's moved on (0 - 1)
float BodyPressedAt(float3 rel)
{
  float best = 0.0;
  int n = (int)g_SnowStampCount.x;
  [loop] for (int k = 0; k < n; ++k)
  {
    float4 s = g_SnowStamp[k], a = g_SnowStampAxis[k];
    float2 d = rel.xz - s.xz;
    float2 t = float2(dot(d, float2(-a.y, a.x)), dot(d, a.xy)) / 0.02;
    if (any(t < 0.0) || t.x > 128.0 || t.y > 256.0 || abs(rel.y - (s.y + a.z * t.y * 0.02)) > 0.3)
      continue;
    best = max(best, saturate(s.w) * g_SnowStamps.SampleLevel(g_Linear, float3(t / float2(128.0, 256.0), a.w), 0));
  }
  return best;
}

// Faith's shoe's sole (2026-10-26, the user: "Rain/snow footsteps should
// have faiths shoes footprint"; she wears trainers): 27 cm long, 10 cm
// across the ball of the foot, a round heel 8 cm across, the two joined
// along the outer side of the foot (the arch, on the inner side, leaves no
// mark). d: the point from the print's middle; f: the way she was going;
// outward: that foot's outer side (unit, world x, z; WetLook.cpp's
// FootSide). x: how much of the sole is there, soft over its last
// centimetre; y: its tread, 0 in a groove - 1 on a lug (chevrons 1.4 cm
// apart over all of it, flex grooves across the ball every 3.5 cm).
float2 ShoeSole(float2 d, float2 f, float2 outward)
{
  float a = dot(d, f), c = dot(d, outward);
  float dBall = (length(float2((a - 0.055) / 0.085, (c - 0.004) / 0.05)) - 1.0) * 0.05;
  float dHeel = length(float2(a + 0.09, c + 0.002)) - 0.04;
  float2 p = float2(a, c) - float2(-0.09, 0.01), band = float2(0.12, 0.006);
  float dBand = length(p - band * saturate(dot(p, band) / dot(band, band))) - 0.028;
  float dist = SmoothMin(SmoothMin(dBall, dBand, 0.02), dHeel, 0.02);
  float sole = saturate(0.5 - dist / 0.01);
  float x = (a - abs(c) * 0.7) / 0.014;
  float lug = smoothstep(0.12, 0.3, abs(x - round(x)));
  float y = (a - 0.01) / 0.035;
  float flex = a > 0.0 ? smoothstep(0.002, 0.004, abs(y - round(y)) * 0.035) : 1.0;
  return float2(sole, lug * flex);
}

// A KrugerSec's boot's sole (2026-10-27, the user: "Ksec should leave behind
// snow footsteps"): an Enforcer's boot as his own mesh has it
// (characters/enemy/enforcer/fullbody/enforcer_fullbody_mesh, LOD0: his
// foot's triangles within 1.5 cm of its lowest point, filled on a 2 mm grid,
// the snow session's scratchpad body\sole.py): 28.4 cm long; a heel block
// 7.2 cm across and 7 cm long at its back, a shank 4.2 cm across, the ball
// and toe 9.4 cm across at most, their middle 1.7 cm to the outer side of the
// heel's. Every KrugerSec's (the other kinds' boots weren't measured). Its
// tread isn't in the mesh (the game draws it with the boot's textures), so it
// has none. d: from its middle; f: the way its toe points; outward: that
// boot's outer side (unit, world x, z). How much of the sole is there, soft
// over its last centimetre.
float BootSole(float2 d, float2 f, float2 outward)
{
  float a = dot(d, f), c = dot(d, outward);
  // (the heel block: a box rounded 1 cm)
  float2 q = abs(float2(a + 0.107, c)) - float2(0.025, 0.026);
  float dHeel = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - 0.01;
  float2 p = float2(a, c) - float2(-0.072, -0.004), band = float2(0.05, 0.01);
  float dShank = length(p - band * saturate(dot(p, band) / dot(band, band))) - 0.021;
  float dBall = (length(float2((a - 0.058) / 0.088, (c - 0.017) / 0.047)) - 1.0) * 0.047;
  float dist = SmoothMin(SmoothMin(dHeel, dShank, 0.015), dBall, 0.015);
  return saturate(0.5 - dist / 0.01);
}

// How Faith's footprints mark a point (g_Print, the way she was going and
// the foot's outer side g_PrintDir): each her shoe's sole (ShoeSole), its
// tread's grooves less marked than its lugs, as strong as the print is now
// (WetLook.cpp's PrintKind, by g_Print's w):
// - x: her wet shoes on drier ground, how wet (w 0 - 1);
// - y: pressed into wet concrete or stone, how much of the water pressed out
//   (w -1 - 0; the lugs press most; not made since 2026-11-03: prints only on
//   what's drier, WetLook.cpp's StepWater);
// - z: pressed into wet gravel (w -3 - -2; not made either);
// - w: her wet shoes on a wall she ran along (g_PrintDir's zw 0: its xy the
//   way along the wall, level, the toe that way; the print in the wall's
//   plane, within 15 cm of it).
float4 PrintsAt(float3 rel)
{
  float4 best = 0.0;
  // (none reaches a point outside the box round them all: 0.25 m round each)
  if (OffBox(rel, g_PrintBounds[0], g_PrintBounds[1]))
    return best;
  int n = (int)g_StepCounts.y;
  [loop] for (int k = 0; k < n; ++k)
  {
    float4 p = g_Print[k];
    float4 dirs = g_PrintDir[k];
    float3 d = rel - p.xyz;
    if (dot(d, d) > 0.06)
      continue;
    if (dot(dirs.zw, dirs.zw) < 0.25)
    {
      float3 along = float3(dirs.x, 0.0, dirs.y), across = float3(-dirs.y, 0.0, dirs.x);
      if (abs(dot(d, across)) > 0.15)
        continue;
      float2 onWall = ShoeSole(float2(dot(d, along), d.y), float2(1.0, 0.0), float2(0.0, -1.0));
      best.w = max(best.w, p.w * onWall.x * (0.55 + 0.45 * onWall.y));
      continue;
    }
    if (abs(d.y) > 0.15 || dot(d.xz, d.xz) > 0.04)
      continue;
    float2 sole = ShoeSole(d.xz, dirs.xy, dirs.zw);
    if (p.w >= 0.0)
      best.x = max(best.x, p.w * sole.x * (0.55 + 0.45 * sole.y));
    else if (p.w > -1.5)
      best.y = max(best.y, -p.w * sole.x * (0.3 + 0.7 * sole.y));
    else
      best.z = max(best.z, (-p.w - 2.0) * sole.x * (0.5 + 0.5 * sole.y));
  }
  return best;
}

// Rain running down a slope (2026-10-26, the user: "Check if it's possible
// for rain to go downslopes"; where, see SlopeAt): a thin sheet of water
// flowing down the slope's fall line, faster the steeper (1.2 m/s at 14
// degrees, 2.2 m/s at 41), its surface in low waves (two trains about 29 and
// 25 cm long, crossing a little, rolling down with it, stronger here and
// weaker there) that bend what it mirrors. The tilt they give the surface's
// normal (along the fall line); flow: how much of a sheet there is (0 - 1,
// more the steeper). It shows only through the reflection, never as lines of
// its own: the first two builds drew bright rivulets and streaks over it,
// lit by what's above on the frame, which looked like smears (the user,
// 2026-10-26: "Smear effect still looks bad and buggy", with a screenshot of
// white streaks fanned over a roof). Measured from the nearest corner of a
// 256 m grid, not the city's origin, so the waves stay put.
// Now (2026-10-26, the user: "water down slopes is really laggy and doesn't
// flow properly it's tweaking"): its 25 - 29 cm waves tilted the sharp
// reflection's aim by up to 9 degrees, so the reflection jumped from one
// thing to another as they moved (shimmering, not flowing), and the sharp
// rays cost the most. Now the waves are 40 - 50 cm long, crests across the
// slope rolling down it, and show mainly as the reflection brightening and
// dimming over them (a sheet of water's sheen moving down), its aim tilted
// only 2 degrees, the reflection a little blurred (see PsMain).
// Now (2026-10-26, the user: "water still doesn't properly flow downwards on
// a slope"): its waves were laid along the fall line and across it, from the
// 256 m grid's corner, and moved by the clock (up to 1000 s) times a speed
// from the pixel's own slope. That slope comes from the frame's depth, a
// little different from one pixel to the next and from frame to frame (the
// game's jitter), and those differences times hundreds of metres and
// seconds put each pixel's waves anywhere: it shimmered in place, worse the
// longer the game ran, instead of running down. Now as games draw running
// water: the pattern is laid on the ground's own x and z (FlowPattern,
// nothing taken from the slope there) and moved down the fall line by at
// most FlowCycle seconds' flow, 0.5 + 1.5 x the slope's sine m/s; two such
// layers half a cycle apart, each faded out as it starts again, so no jump
// shows. The slope only says which way and how fast, so its small
// differences move the pattern a few mm. Returns the waves, -1 - 1 about;
// fall: the way down (unit); flow: how much of a sheet there is (0 - 1,
// more the steeper); tilt: its surface's slope (x, z) along the way down,
// for its shading.
// Now (2026-10-27, the user: "improve the water sliding down slopes shader it
// still looks realy bad"): its pattern was round blots 17 - 42 cm across,
// slid down as they were, darkening the ground in patches and pulsing as
// its two layers took turns every 0.6 s: blots sliding, not a sheet of
// water. Rain running off a slope is a thin sheet in rivulets along the way
// down. Now the pattern is fine (5 and 14 cm) and smeared along the way down
// over FlowStreaks' 4 points StreakStep apart (each pixel's own way, a few
// cm from it, so its small differences don't move the pattern): streaks
// about 35 cm long and a few cm across, running down; the layers take turns
// every FlowCycle / 2 = 1 s; tilt is the rivulets' sides (across the way
// down), and the sheet darkens the ground evenly (PsMain).
// Its speed by how steep the slope is (2026-10-27, the user: "water down
// slopes should have different speed types depending how steep the slope
// is"; it was 0.5 + 1.5 x the slope's sine m/s, 0.9 - 1.5, too alike to
// tell): a slow sheet on a gentle slope (0.35 m/s to about 18 degrees), a
// steady flow on a middling one (about 1 m/s at 25), running fast down a
// steep one (1.75 at 35, 2.45 at 41); and its streaks as long: StreakStep
// 5 cm apart on a gentle slope (20 cm streaks), 15 on a steep one (60 cm).
static const float FlowCycle = 2.0;

float FlowSpeed(float steep)
{
  return 0.35 + 0.9 * smoothstep(0.25, 0.5, steep) + 1.2 * smoothstep(0.45, 0.66, steep);
}

float StreakStep(float steep)
{
  return lerp(0.05, 0.15, smoothstep(0.25, 0.6, steep));
}

// How much of the running water on slopes there is for the weather's
// Wetness (g_Wet.x): only at full wetness (2026-10-27, the user: "Rain down
// slopes should be visible only when wetness is at 100%"), none under 0.98,
// all at 1 (the last 2 % fading in, so the slider doesn't pop). The rings and
// her slide on it, and the drips off its ends (SlopeEndAt), go with it.
float RunningWater()
{
  return smoothstep(0.98, 1.0, g_Wet.x);
}

float FlowPattern(float2 p)
{
  return (Noise(p / 0.05) * 0.65 + Noise(p / 0.14 + 5.3) * 0.35) * 2.0 - 1.0;
}

// The pattern smeared along the way down (way: unit, over the ground; step
// its 4 points apart, see StreakStep): its streaks, -1 - 1 about
float FlowStreaks(float2 q, float2 way, float step)
{
  float sum = 0.0;
  [unroll] for (int i = 0; i < 4; ++i)
    sum += FlowPattern(q - way * (step * i));
  return clamp(sum * 0.5, -1.0, 1.0);
}

float FlowWaves(float3 world, float3 normal, float t, out float3 fall, out float flow, out float2 tilt)
{
  flow = 0.0;
  tilt = 0.0;
  fall = float3(0.0, -1.0, 0.0) + normal * normal.y;
  float steep = length(fall);
  if (steep < 0.08)
    return 0.0;
  fall /= steep;
  flow = smoothstep(0.08, 0.3, steep);
  // Over the ground: the way down (its length how much of the fall line is
  // over the ground) and how far a cycle carries the water
  float2 down = fall.xz;
  float2 way = down / max(length(down), 1e-4);
  float carried = FlowSpeed(steep) * FlowCycle;
  float2 p = world.xz - round(world.xz / 1024.0) * 1024.0;
  float c0 = frac(t / FlowCycle), c1 = frac(t / FlowCycle + 0.5);
  float w0 = 1.0 - abs(2.0 * c0 - 1.0), w1 = 1.0 - w0;
  float2 q0 = p - down * carried * c0, q1 = p - down * carried * c1 + float2(37.1, 11.7);
  // (the rivulets' sides: the streaks' change across the way down)
  float2 across = float2(-way.y, way.x);
  float streak = StreakStep(steep);
  float a0 = FlowStreaks(q0, way, streak), a1 = FlowStreaks(q1, way, streak);
  float b0 = FlowStreaks(q0 + across * 0.015, way, streak), b1 = FlowStreaks(q1 + across * 0.015, way, streak);
  // (the two layers' mix kept as strong as one)
  float keep = rsqrt(w0 * w0 + w1 * w1);
  float side = ((b0 - a0) * w0 + (b1 - a1) * w1) * keep / 0.015;
  tilt = across * clamp(side * 0.003, -0.12, 0.12);
  return (a0 * w0 + a1 * w1) * keep;
}

// Whether falling water (a drip's or a drainpipe's, depthHere: its view
// depth at a pixel) is hidden by the frame's surface there (sceneDepth;
// behind: per metre of view depth, x how far that is along the surface's
// normal, y along the view, see PsMain): more than 5 cm behind it along its
// normal, or 50 cm along the view (2026-10-26, the user: "find any instances
// for rain as well", after the ice's z-fighting: it was 5 cm of view depth,
// which a slant multiplies, so a stream a few cm from a wall or the ground
// came and went with each frame's jitter; along the view too, so a wall seen
// edge-on doesn't show what's far behind it).
bool FallingHidden(float depthHere, float sceneDepth, float2 behind)
{
  float gap = depthHere - sceneDepth;
  return gap * behind.x > 0.05 || gap * behind.y > 0.5;
}

// Rain dripping off roof edges (2026-10-26, the user: rain "come out of
// gutters"; the game has no gutters, so it drips where a roof ends, see
// WetLook.cpp's drips): per stream (g_Drip) drops let go DripInfo.z a second
// and fall as they would (from rest, 9.8 m/s^2), each a short streak that
// stretches as it speeds up, hidden behind what's nearer (sceneDepth: the
// pixel's view depth; see FallingHidden). How bright, 0 - 1.
float DripsAt(float2 pixel, float sceneDepth, float2 behind, float t)
{
  float total = 0.0;
  // (none reaches a pixel outside the box round them all, each its stream's
  // box 4 pixels out: until 2026-11-02 every pixel of the frame went through
  // all of them, 48 while it rains on the roofs)
  if (OffFrameBox(pixel, g_DripBounds))
    return total;
  int n = (int)g_DripCount.x;
  // (only the streams that can reach this pixel's tile, FallingBits: 0 -
  // 31, then 32 - 63)
  uint lo = FallingBits(pixel, 0, n), hi = FallingBits(pixel, 1, n - 32);
  [loop] while ((lo | hi) != 0u)
  {
    uint word = lo != 0u ? lo : hi;
    uint low = word & (0u - word);
    int k = (lo != 0u ? 0 : 32) + LowestBit(low);
    if (lo != 0u)
      lo ^= low;
    else
      hi ^= low;
    float4 seg = g_Drip[k];
    float2 a = seg.xy, b = seg.zw;
    if (pixel.x < min(a.x, b.x) - 4.0 || pixel.x > max(a.x, b.x) + 4.0 || pixel.y < min(a.y, b.y) - 4.0 ||
        pixel.y > max(a.y, b.y) + 4.0)
      continue;
    float2 ab = b - a;
    float u = saturate(dot(pixel - a, ab) / max(dot(ab, ab), 1e-4));
    float4 info = g_DripInfo[k];
    float inv = lerp(1.0 / info.x, 1.0 / info.y, u);
    float depthHere = 1.0 / inv;
    if (FallingHidden(depthHere, sceneDepth, behind))
      continue;
    float halfWidth = max(0.6, g_DripCount.z / depthHere);
    float across = saturate(1.5 - length(pixel - (a + ab * u)) / halfWidth);
    if (across <= 0.0)
      continue;
    float s = (u / info.y) / inv;                     // 0 its top - 1 its bottom, along the stream itself
    float height = max(g_DripLand[k].w, 0.5);
    float fallTime = sqrt(2.0 * height / 9.8);
    float rate = max(info.z, 0.1);
    float since = frac(t * rate + info.w) / rate;     // the newest drop's age (s)
    [unroll] for (int m = 0; m < 4; ++m)
    {
      float age = since + m / rate;
      if (age > fallTime)
        break;
      float at = 0.5 * 9.8 * age * age / height;      // how far down it is, 0 - 1
      float tail = clamp((0.04 + 0.05 * 9.8 * age) / height, 0.005, 0.25);
      float on = saturate(1.0 - abs(s - (at - tail * 0.5)) / (tail * 0.5));
      total = max(total, on * across);
    }
  }
  return total;
}

// Rings where the drips land in a puddle: each drop's ring goes out as a
// step's does (StepRings), smaller and shorter; the water's slope (x, z)
float2 DripRings(float3 rel, float t)
{
  float2 s = 0.0;
  // (none reaches a point outside the box round where they all land)
  if (OffBox(rel, g_DripLandBounds[0], g_DripLandBounds[1]))
    return s;
  int n = (int)g_DripCount.x;
  [loop] for (int k = 0; k < n; ++k)
  {
    float4 land = g_DripLand[k];
    float2 d = rel.xz - land.xz;
    float r = length(d);
    if (r > 0.45 || abs(rel.y - land.y) > 0.3 || r < 1e-3)
      continue;
    float4 info = g_DripInfo[k];
    float rate = max(info.z, 0.1);
    float fallTime = sqrt(2.0 * max(land.w, 0.5) / 9.8);
    // the time since the last drop landed: drops land a fall's time after
    // they're let go
    float landed = frac((t - fallTime) * rate + info.w) / rate;
    float front = 0.03 + 0.5 * landed;
    float behind = front - r;
    if (behind < -0.01 || behind > 0.12)
      continue;
    float fade = saturate(1.0 - landed / 0.8);
    s += d / r * 0.35 * fade * fade * exp(-behind * behind / 0.003) * sin(behind * 110.0);
  }
  return s;
}

// The drainpipes' water (see Weather.cpp's UpdatePipes): per stream
// (g_Pipe) a column of water out of the pipe's foot, falling as water does
// (out at 0.8 m/s, then 9.8 m/s^2): whole near the outlet, breaking into
// drops further down (less the harder it runs), its pattern carried down
// with the water; where it lands a splash, drops thrown up and out round it
// for a third of a second each. Hidden behind what's nearer (sceneDepth: the
// pixel's view depth). How bright, 0 - 1.
float PipesAt(float2 pixel, float sceneDepth, float2 behind, float t)
{
  float total = 0.0;
  // (none reaches a pixel outside the box round them all, as DripsAt)
  if (OffFrameBox(pixel, g_PipeBounds))
    return total;
  int n = (int)g_PipeCount.x;
  float flow = g_PipeCount.y;
  // (only the streams that can reach this pixel's tile, FallingBits)
  uint left = FallingBits(pixel, 2, n);
  [loop] while (left != 0u)
  {
    uint low = left & (0u - left);
    left ^= low;
    int k = LowestBit(low);
    float4 seg = g_Pipe[k];
    float4 info = g_PipeInfo[k];
    float2 a = seg.xy, b = seg.zw;
    float splashPx = 0.35 * g_PipeCount.w / max(info.y, 0.3);
    float widthPx = g_PipeCount.z / max(min(info.x, info.y), 0.3) * 1.5 + 2.0;
    float pad = max(splashPx, widthPx);
    if (pixel.x < min(a.x, b.x) - pad || pixel.x > max(a.x, b.x) + pad || pixel.y < min(a.y, b.y) - pad ||
        pixel.y > max(a.y, b.y) + pad)
      continue;
    // The stream
    float2 ab = b - a;
    float u = saturate(dot(pixel - a, ab) / max(dot(ab, ab), 1e-4));
    float inv = lerp(1.0 / info.x, 1.0 / info.y, u);
    float depthHere = 1.0 / inv;
    if (!FallingHidden(depthHere, sceneDepth, behind))
    {
      float s = (u / info.y) / inv;                     // 0 the outlet - 1 where it lands, along the stream itself
      float fallen = s * info.z;                         // m
      float halfWidth = max(0.6, g_PipeCount.z * (0.6 + 0.6 * flow) / depthHere);
      float across = saturate(1.5 - length(pixel - (a + ab * u)) / halfWidth);
      if (across > 0.0)
      {
        // When the water here left the pipe, so its pattern goes down with it
        float since = (sqrt(0.64 + 19.6 * fallen) - 0.8) / 9.8;
        float pattern = Noise(float2((t - since) * 9.0 + info.w * 40.0, (float)k * 5.3));
        float broken = saturate(fallen / 1.5) * (0.75 - 0.35 * flow);
        float body = smoothstep(broken - 0.1, broken + 0.1, pattern);
        total = max(total, body * across * (0.45 + 0.55 * saturate(flow * 2.0)));
      }
    }
    // The splash where it lands
    float2 fromLand = pixel - b;
    if (dot(fromLand, fromLand) < splashPx * splashPx && info.y <= sceneDepth + 0.3)
    {
      float perMetre = g_PipeCount.w / max(info.y, 0.3);  // pixels a metre there
      float dropPx = max(0.7, 0.006 * perMetre);
      [unroll] for (int m = 0; m < 8; ++m)
      {
        float h1 = Hash(float2(k * 7.0 + m, 3.1)), h2 = Hash(float2(k * 7.0 + m, 9.7)), h3 = Hash(float2(k * 7.0 + m, 21.3));
        float age = frac(t * (2.5 + 1.5 * h1) + h2) * 0.33;
        float away = (0.25 + 0.6 * h3) * age;                  // m out from the water
        float up = (0.6 + 0.9 * h1) * age - 4.9 * age * age;   // m up
        if (up < 0.0)
          continue;
        float angle = h2 * 6.2832;
        float2 at = b + float2(cos(angle) * away, -up - sin(angle) * away * 0.35) * perMetre;
        float on = saturate(1.5 - length(pixel - at) / dropPx);
        total = max(total, on * saturate(flow * 3.0) * 0.8);
      }
    }
  }
  return total;
}

// Rings where the drainpipes' water lands in a puddle: four going out at
// once, 0.15 s apart, as a step's do (StepRings); the water's slope (x, z)
float2 PipeRings(float3 rel, float t)
{
  float2 s = 0.0;
  // (none reaches a point outside the box round where they all land)
  if (OffBox(rel, g_PipeLandBounds[0], g_PipeLandBounds[1]))
    return s;
  int n = (int)g_PipeCount.x;
  [loop] for (int k = 0; k < n; ++k)
  {
    float3 land = g_PipeLand[k].xyz;
    float2 d = rel.xz - land.xz;
    float r = length(d);
    if (r > 0.6 || abs(rel.y - land.y) > 0.3 || r < 1e-3)
      continue;
    [unroll] for (int m = 0; m < 4; ++m)
    {
      float age = frac(t / 0.6 + g_PipeInfo[k].w + m * 0.25) * 0.6;
      float front = 0.05 + 0.8 * age;
      float behind = front - r;
      if (behind < -0.01 || behind > 0.12)
        continue;
      float fade = saturate(1.0 - age / 0.6);
      s += d / r * 0.3 * fade * fade * exp(-behind * behind / 0.003) * sin(behind * 110.0) * g_PipeCount.y;
    }
  }
  return s;
}

// The haze of the falling rain or snow (2026-10-26, the user: "Rain/snow
// should have a haze going on"): the rain or snow between the camera and a
// point scatters light and hides it, the farther the more, what's let
// through falling off as exp(-distance x thickness) (g_Haze.x, as hard as
// it falls: weather::GetHaze). Not even: veils of it 40 - 80 m across drift
// with the wind (g_Haze.zw) and sink, three samples of a slow noise along
// the view (15, 60 and 200 m out, or at the point itself where it's nearer)
// making it thicker here and thinner there (x 0.55 - 1.45). The sky as if
// 1.5 km off. At most 0.85 of what's behind. How much of the haze covers the
// pixel.
// (Each of them always counted, 2026-10-26 19:xx, the user: "fix any haze
// flickering on buildings find the culprit and fix them all the same applies
// for rain": only those within the distance counted, so where a building's
// distance crossed 60 or 200 m the average went from one sample to two or
// from two to three, the haze up to a fifth thicker or thinner across a line
// on its wall, and the lines swept over the buildings as the camera moved.
// A sample at the point itself where it's nearer goes on from the one past
// it, so the haze is even across those distances.)
// The most of what's behind the haze hides: 0.85, up to 0.97 as it's made
// thicker than the sliders' old most allowed (the rain's Haze 2 at full rain,
// 0.007 a metre; Snow haze 3 in a blizzard, 0.0224), twice that more giving
// all of it: so past the old most it's thicker far off too, not only nearer
// (2026-10-27, the user: "lets increase the fog/haze sliders")
float HazeMost()
{
  float oldMost = g_Haze.y > 0.5 ? 0.0224 : 0.007;
  return 0.85 + 0.12 * saturate((g_Haze.x - oldMost) / (2.0 * oldMost));
}

float HazeAt(float3 dir, float distance)
{
  float reach = min(distance, 1500.0);
  float veils = 0.0;
  [unroll] for (int i = 0; i < 3; ++i)
  {
    float at = i == 0 ? 15.0 : i == 1 ? 60.0 : 200.0;
    float3 p = g_Camera.xyz + dir * min(at, reach);
    float2 q = (p.xz - g_Haze.zw) / 55.0;
    veils += Noise(q + float2(p.y / 35.0 + g_Camera.w * 0.03, (float)i * 7.3));
  }
  float thickness = g_Haze.x * (0.55 + 0.9 * veils / 3.0);
  return min(1.0 - exp(-reach * thickness), HazeMost());
}

// The haze's colour (PsMain, and the ice's pass after it, IcePsMain): the
// light of the sky by the horizon (g_HazeView.yzw: the frame's sky, see
// HazeSkyAt and WetLook.cpp's HazeSkySeconds, held while no sky is seen),
// greyer, whiter in the snow: what's far off goes into the sky's own colour,
// as in haze. Lit up by a lightning flash, as the rain round it is. Until
// the sky has been seen: the scene's own light, its average greyer and a
// little lighter, as before.
// (2026-10-26, the user: "Do more research on how haze works and fix most
// issues": it was always the whole frame's average, so it brightened as the
// camera looked up at the sky and darkened as it looked down at a street,
// over everything far off at once; and a flash brightened the sky under up
// to 85 % of a haze that didn't flash, so most of it was hidden.)
float3 HazeTint(float3 average)
{
  // (the snow's a cool white with none of the light's own colour, snow
  // scattering daylight about evenly; the rain's keeps 30 % of the sky's.
  // 2026-10-26, the user: "for som reason city fog is now purple", the
  // snowy look at about 8:00, its morning sky graded pink and purple, of
  // which the snow's haze kept 40 % and the rain's 60 %)
  float3 tint;
  float3 cool = float3(0.96, 0.98, 1.0);
  if (g_HazeView.y >= 0.0)
  {
    float3 sky = g_HazeView.yzw;
    float grey = dot(sky, float3(0.2126, 0.7152, 0.0722));
    tint = g_Haze.y > 0.5 ? grey * cool * 1.1 : lerp(sky, grey.xxx, 0.7) * 0.95;
  }
  else
  {
    float grey = dot(average, float3(0.2126, 0.7152, 0.0722));
    tint = g_Haze.y > 0.5 ? grey * cool * 1.9 + 0.03 : lerp(average, grey.xxx, 0.6) * 1.5 + 0.015;
  }
  return tint * (1.0 + 3.0 * g_Flash.x) + g_Flash.yzw * g_Flash.x * 0.12;
}

// The sky's light by the horizon, for the haze's colour (HazeTint): the
// frame's sky at 8 x 6 points over its upper two thirds (the game's frame a
// little blurred), averaged; w how much of them was sky (0 none: WetLook.cpp
// keeps the light it had). One pixel of PsQuery's target (HazeSkyRow).
static const float HazeSkyRow = 33.0;

float4 HazeSkyAt()
{
  float3 sum = 0.0;
  float n = 0.0;
  [loop] for (int j = 0; j < 6; ++j)
  [loop] for (int i = 0; i < 8; ++i)
  {
    float2 uv = float2((i + 0.5) / 8.0, (j + 0.5) / 6.0 * 0.66);
    if (IsSky(DepthAt(uv)))
    {
      sum += g_Scene.SampleLevel(g_Linear, uv, 3.0).rgb;
      n += 1.0;
    }
  }
  return n > 0.0 ? float4(sum / n, n / 48.0) : float4(0.0, 0.0, 0.0, 0.0);
}

// How far along the camera's view a pixel of the game's depth is (metres, > 0)
float ViewDepth(float depth)
{
  return -(g_ProjectionZW.z - depth * g_ProjectionZW.w) / (depth * g_ProjectionZW.y - g_ProjectionZW.x);
}

// A reflection on the frame, made the way the game's own are (its
// ScreenSpaceRaytrace compute shaders in systems/shaderprogramdb, read
// 2026-10-25 23:4x): the ray is marched across the frame in pixels with
// 1 / depth carried along it (so its depth is right all along), a hit is
// where it goes behind what's drawn there, found exactly by halving, and it
// counts in full only within a thickness that grows with distance, less
// beyond it (the game's thickness fade); it fades near the frame's border and
// for rays that come back towards the camera (the game's border and camera
// fades); rough surfaces see it blurred (the frame's blurred levels, as wide
// as the spread of their rays there). A ray that runs out over the sky drawn
// on the frame reflects that sky. x y z: what's reflected; w: how much of it
// counts (0 none: the surface keeps its own colour). The game falls back on
// its cube maps where its rays miss; those aren't reachable from here.
// One step of TraceReflection's march, gone through as it was one by one
// (see there): k, which of the 4 read together; i the first of them; t its
// place along the ray (0 - 1), there the depth read where that is on the
// frame. Once the march stops (off the frame, out of steps) or crosses behind
// what's drawn, the steps after it are left alone.
void MarchStep(int k, int i, int steps, float t, float there, float2 s0, float2 s1, float2 size, float k0, float k1,
  inout bool stop, inout bool crossed, inout float tCross, inout int crossStep, inout float tPrev, inout float tSky)
{
  if (stop || crossed)
    return;
  float2 at = lerp(s0, s1, t);
  if (i + k > steps || any(at < 1.0) || any(at > size - 1.0))
  {
    stop = true;
    return;
  }
  if (IsSky(there))
  {
    if (tSky < 0.0)
      tSky = t;
    tPrev = t;
    return;
  }
  float rayW = 1.0 / lerp(k0, k1, t);
  // (not within 2 pixels of its start: that's the surface itself)
  if (rayW > ViewDepth(there) && length(at - s0) > 2.0)
  {
    crossed = true;
    tCross = t;
    crossStep = k;
    return;
  }
  tPrev = t;
}

float4 TraceReflection(float2 uv, float3 start, float3 direction, float viewDistance, float roughness)
{
  // The game's fade for rays coming back at the camera first: one it takes
  // all of (looking down at the ground steeper than 60 degrees: every ray
  // of it) counts for nothing whatever it finds, so it isn't marched
  // (2026-11-02, the user: "Rain still drops your fps by a lot"; the same
  // picture: it was marched and then multiplied by 0)
  float3 toPoint = normalize(start);
  float cameraFade = saturate(1.0 + 2.0 * dot(direction, toPoint));
  if (cameraFade <= 0.0)
    return 0;
  float reach = clamp(viewDistance * 1.5 + 20.0, 20.0, 250.0);
  float3 end = start + direction * reach;
  float4 clip0 = mul(float4(start, 1.0), g_ViewProjection);
  float4 clip1 = mul(float4(end, 1.0), g_ViewProjection);
  const float nearW = 0.1;
  if (clip0.w <= nearW)
    return 0;
  if (clip1.w < nearW)
  {
    float cut = (clip0.w - nearW) / (clip0.w - clip1.w);
    end = start + (end - start) * cut;
    reach *= cut;
    clip1 = mul(float4(end, 1.0), g_ViewProjection);
  }
  float2 size = 1.0 / g_Frame.xy;
  float2 s0 = float2(clip0.x / clip0.w * 0.5 + 0.5, 0.5 - clip0.y / clip0.w * 0.5) * size;
  float2 s1 = float2(clip1.x / clip1.w * 0.5 + 0.5, 0.5 - clip1.y / clip1.w * 0.5) * size;
  float k0 = 1.0 / clip0.w, k1 = 1.0 / clip1.w;
  float pixels = length(s1 - s0);
  if (pixels < 1.0)
    return 0;
  // As many steps as it takes to go 3 pixels a step on a puddle (a sharp
  // mirror), 8 on rough wet ground (blurred), 8 - 48 of them: it took 48
  // for every ray however short (the performance, 2026-10-26; the halving
  // after a hit finds its place as before)
  // (rough wet ground's, a blur, in at most 20 steps at most 16 pixels
  // apart: its 48 were most of the pass's cost at 4K, 2026-11-01, the user's
  // "Improve the performance of the Rain"; a puddle's and the running
  // water's as before)
  float rough = saturate((roughness - 0.1) / 0.25);
  float stepPixels = lerp(lerp(3.0, 8.0, saturate(roughness / 0.35)), 16.0, rough);
  int steps = clamp((int)ceil(pixels / stepPixels), 8, (int)lerp(48.0, 20.0, rough));
  float dt = 1.0 / steps;
  float jitter = Hash(uv * size + 0.5);   // a fixed offset per pixel: no banding from the steps
  float tPrev = 0.0, tHit = -1.0, tSky = -1.0, hitBehind = 0.0;
  // (4 steps' depths read together, then gone through in order as before:
  // step by step each read waited for the one before to tell whether to go
  // on, up to 48 one after another for every puddle's pixel, the pass's
  // dearest part at 4K; 2026-11-02, the user: "Rain still drops your fps by
  // a lot". The same steps, tests and halving; the reads past where it
  // stops are thrown away)
  int i = 1;
  [loop] while (i <= steps)
  {
    float4 t = (float4(i, i + 1, i + 2, i + 3) - 1.0 + jitter) * dt;
    float4 there = float4(g_SceneDepth.Load(int3(int2(lerp(s0, s1, t.x) * g_Depth.xy), 0)),
      g_SceneDepth.Load(int3(int2(lerp(s0, s1, t.y) * g_Depth.xy), 0)),
      g_SceneDepth.Load(int3(int2(lerp(s0, s1, t.z) * g_Depth.xy), 0)),
      g_SceneDepth.Load(int3(int2(lerp(s0, s1, t.w) * g_Depth.xy), 0)));
    bool stop = false, crossed = false;
    float tCross = 0.0;
    int crossStep = 0;
    MarchStep(0, i, steps, t.x, there.x, s0, s1, size, k0, k1, stop, crossed, tCross, crossStep, tPrev, tSky);
    MarchStep(1, i, steps, t.y, there.y, s0, s1, size, k0, k1, stop, crossed, tCross, crossStep, tPrev, tSky);
    MarchStep(2, i, steps, t.z, there.z, s0, s1, size, k0, k1, stop, crossed, tCross, crossStep, tPrev, tSky);
    MarchStep(3, i, steps, t.w, there.w, s0, s1, size, k0, k1, stop, crossed, tCross, crossStep, tPrev, tSky);
    if (crossed)
    {
      // Behind what's drawn here: exactly where, by halving
      float lo = tPrev, hi = tCross;
      [unroll] for (int b = 0; b < 6; ++b)
      {
        float mid = 0.5 * (lo + hi);
        float2 m = lerp(s0, s1, mid);
        float md = g_SceneDepth.Load(int3(int2(m * g_Depth.xy), 0));
        if (!IsSky(md) && 1.0 / lerp(k0, k1, mid) > ViewDepth(md))
          hi = mid;
        else
          lo = mid;
      }
      float2 hitAt = lerp(s0, s1, hi);
      float hitDepth = g_SceneDepth.Load(int3(int2(hitAt * g_Depth.xy), 0));
      float sceneW = ViewDepth(hitDepth);
      float behind = 1.0 / lerp(k0, k1, hi) - sceneW;
      float thickness = 0.15 + 0.02 * sceneW;
      if (behind < thickness * 3.0)
      {
        tHit = hi;
        hitBehind = behind / thickness;
        break;
      }
      // Behind something in front of the ray (a railing, a pipe): on past it,
      // from the step after
      tPrev = tCross;
      i += crossStep + 1;
      continue;
    }
    if (stop)
      break;
    i += 4;
  }
  // The game's fades: the frame's border (rays coming back at the camera,
  // cameraFade, above)
  float4 result = 0;
  if (tHit >= 0.0)
  {
    float2 hitAt = lerp(s0, s1, tHit);
    float2 hitUv = hitAt * g_Frame.xy;
    float2 edge = abs(hitUv * 2.0 - 1.0);
    float border = (1.0 - smoothstep(0.85, 1.0, edge.x)) * (1.0 - smoothstep(0.85, 1.0, edge.y));
    float thicknessFade = 1.0 - saturate((hitBehind - 1.0) / 2.0);
    float reachFade = 1.0 - smoothstep(0.85, 1.0, tHit);
    float level = min(g_Frame.w - 1.0, log2(1.0 + roughness * length(hitAt - uv * size) * 0.35));
    result = float4(g_Scene.SampleLevel(g_Linear, hitUv, level).rgb, border * thicknessFade * reachFade * cameraFade);
  }
  else if (tSky >= 0.0)
  {
    // Out over the sky on the frame: that sky, a little blurred
    float2 skyUv = lerp(s0, s1, tSky) * g_Frame.xy;
    float2 edge = abs(skyUv * 2.0 - 1.0);
    float border = (1.0 - smoothstep(0.85, 1.0, edge.x)) * (1.0 - smoothstep(0.85, 1.0, edge.y));
    result = float4(g_Scene.SampleLevel(g_Linear, skyUv, 1.0 + roughness * 4.0).rgb, border * cameraFade);
  }
  return result;
}

// A little splash at every ripple (2026-10-26, the user: "also we need
// little splash of water in every ripple"; g_Splash, WetLook.cpp's
// MostSplashes): 10 drops thrown up from the ring's middle, each up at 0.8 -
// 1.6 m/s and out at 0.3 - 0.8 m/s (x sqrt(the ring's size): a landing's
// higher and wider), falling back as they would (9.8 m/s^2), round as a
// drop 7 mm across (x sqrt(size); never under 0.7 pixel), fainter as they
// fall back. On the frame as seen from where the splash is: its height up
// the frame, its spread over the ground across it and, as much as the
// camera looks down, up and down it. Hidden behind what's nearer
// (sceneDepth: the pixel's view depth). How much, 0 - 1.
float SplashesAt(float2 pixel, float sceneDepth)
{
  float total = 0.0;
  // (none reaches a pixel outside the box round them all, as DripsAt)
  if (OffFrameBox(pixel, g_SplashBounds))
    return total;
  int n = (int)g_OtherCounts.y;
  // (only the splashes that can reach this pixel's tile, FallingBits)
  uint left = FallingBits(pixel, 3, n);
  [loop] while (left != 0u)
  {
    uint low = left & (0u - left);
    left ^= low;
    int k = LowestBit(low);
    // (not her own, the game's: see HerSplashAt)
    if (g_SplashInfo[k].z > 0.5)
      continue;
    float4 s = g_Splash[k];
    if (s.z > sceneDepth + 0.3)
      continue;
    float size = g_SplashInfo[k].x, seed = g_SplashInfo[k].y;
    float root = sqrt(size);
    float perMetre = g_OtherCounts.z / max(s.z, 0.3);
    float dropPx = max(0.7, 0.0035 * root * perMetre);
    float reach = 0.4 * root * perMetre + dropPx + 2.0;
    float2 off = pixel - s.xy;
    if (abs(off.x) > reach || off.y > reach || off.y < -reach)
      continue;
    [loop] for (int m = 0; m < 10; ++m)
    {
      float h1 = Hash(float2(seed, m * 1.37)), h2 = Hash(float2(m * 2.11, seed + 7.3)), h3 = Hash(float2(seed + m * 1.7, 3.1));
      float up = (0.8 + 0.8 * h2) * root, outward = (0.3 + 0.5 * h3) * root;
      float life = 2.0 * up / 9.8;
      if (s.w > life)
        continue;
      float a = 6.2832 * (m + h1) / 10.0;
      float2 spread = float2(cos(a), sin(a)) * outward * s.w;
      float height = up * s.w - 4.9 * s.w * s.w;
      float2 at = s.xy + float2(spread.x, -height - spread.y * g_OtherCounts.w) * perMetre;
      float drop = saturate(1.0 - length(pixel - at) / dropPx);
      total = max(total, drop * (0.45 + 0.55 * (1.0 - s.w / life)));
    }
  }
  return total;
}

// Where her own splash is on the frame, the game's (2026-10-27, the user:
// "also on puddles use faiths splash fx when she steps on in-game puddles";
// g_Splash's with g_SplashInfo.z 1, WetLook.cpp's MostHerSplashes): the
// game's FX_Foot_Water (and the landings') plays where her step's effect has
// the water's material, the tools' puddles and the game's own; its splash is
// thrown up to about half a metre for 0.55 s +- 30 % (sprites 0.35 m across),
// with drops. It doesn't write the depth, so the wet look took it for the
// ground and laid the puddle over it. Here: 0.6 m to either side of her
// step, from 1 m over it (up the frame) to 0.6 m in front of it (down the
// frame as much as the camera looks down), x sqrt(its size) (a landing's
// bigger), soft at the edges, going over the last third of HerSplashSeconds
// x sqrt(size); only where what the frame shows is behind it or at it
// (sceneDepth: the pixel's view depth; something nearer hides it). How
// much, 0 - 1: PsMain lets what's bright in it through.
float HerSplashAt(float2 pixel, float sceneDepth)
{
  float most = 0.0;
  // (none reaches a pixel outside the box round them all, as DripsAt)
  if (OffFrameBox(pixel, g_HerSplashBounds))
    return most;
  int n = (int)g_OtherCounts.y;
  // (only the splashes that can reach this pixel's tile, FallingBits)
  uint left = FallingBits(pixel, 3, n);
  [loop] while (left != 0u)
  {
    uint low = left & (0u - left);
    left ^= low;
    int k = LowestBit(low);
    if (g_SplashInfo[k].z < 0.5)
      continue;
    float4 s = g_Splash[k];
    if (sceneDepth < s.z - 0.6)
      continue;
    float root = sqrt(g_SplashInfo[k].x);
    float life = 0.8 * root;
    float perMetre = g_OtherCounts.z / max(s.z, 0.3);
    float2 off = (pixel - s.xy) / (perMetre * root);
    float front = 0.6 * g_OtherCounts.w;
    float inside = (1.0 - smoothstep(0.45, 0.6, abs(off.x))) * (1.0 - smoothstep(0.8, 1.0, -off.y)) *
      (1.0 - smoothstep(front + 0.1, front + 0.25, off.y));
    most = max(most, inside * (1.0 - smoothstep(0.66 * life, life, s.w)));
  }
  return most;
}

// How much of the rain the tools draw falls at a point (world): as surely as
// the weather's squares know it's under the open sky (KnownOpenAt: at its
// height, so over a roof below it too; soft between the squares' middles;
// none inside the game's interiors), all of it from 3/4; anywhere while the
// weather doesn't keep the rain out of buildings, none until the squares are
// known. (2026-10-26, the user: "rain is still blocky around some meshes":
// before, a whole 2 m square with a roof anywhere over its middle, however
// far under the drop, had none, nor did the 1 m round it; and none was drawn
// in the wet look's boxes round people, cars and trains (KeptDry), so boxes
// were cut out of it round them. What's behind the frame's surface isn't
// drawn anyway, and the last metre in front of it fades out.)
// (out to 4 - 14 m short of the squares' edge, faded over a circle round the
// camera: past the squares it can't be told whether a point is in a building,
// so drops were drawn in rooms seen 50 - 60 m off; and stopped at the squares'
// edge they made a square: 2026-11-01, the user: "You can still see some rain
// inside buildings", and the square seen from up high)
float RainOpenAt(float3 world)
{
  if (g_OpenSkyGrid.w < 0.0)
    return 1.0;
  if (g_OpenSkyGrid.w < 1.0)
    return 0.0;
  float half = 0.5 * g_ShelterGrid.z * g_ShelterGrid.w;
  float inReach = 1.0 - smoothstep(half - 14.0, half - 4.0, length(world.xz - g_Camera.xz));
  return saturate((KnownOpenAt(world) - 0.25) * 2.0) * inReach;
}

// The rain past the game's own, made like it: drops on vertical sheets fixed
// in the world, one every 2.5 m along x and along z, so the rain stays where
// it is as the camera moves (the near drops slide past the far ones). Each
// sheet in columns 0.3 m wide, a column a drop now and then (more the heavier
// the rain), falling at 26 - 34 m/s (the game's streaks fall at 25 - 30 m/s)
// and drawn as long as it falls in a 60th of a second, 1.5 times (the game
// stretches its streaks with their motion), 2 cm wide (spread over a pixel
// when thinner: fainter, not thicker), lit by the sky's light and what's
// behind. From 8 m (the game's own rain is round the camera) out to the
// squares the weather knows (60 m), only in front of what the game drew, and
// only where open to the sky.
float4 RainAt(float2 uv, float3 dir, float sceneDistance)
{
  if (g_DistantRain.x <= 0.0)
    return 0;
  const float nearest = 8.0, spacing = 2.5, columnWidth = 0.3, dropWidth = 0.02;
  float reach = g_OpenSkyGrid.w < 0.0 ? 80.0 : 60.0;
  float farthest = min(sceneDistance, reach);
  if (farthest <= nearest)
    return 0;
  float pixelAngle = 2.0 * g_Frame.x / g_ProjectionXY.x;
  float density = saturate(0.25 + 0.3 * g_DistantRain.y);
  float amount = 0;
  [unroll] for (int family = 0; family < 2; ++family)
  {
    float axisDir = family == 0 ? dir.x : dir.z;
    if (abs(axisDir) < 1e-3)
      continue;
    float axisCamera = family == 0 ? g_Camera.x : g_Camera.z;
    float stepT = spacing / abs(axisDir);
    float first = axisCamera + axisDir * nearest;
    float sheet = axisDir > 0.0 ? ceil(first / spacing) : floor(first / spacing);
    float sheetStep = axisDir > 0.0 ? 1.0 : -1.0;
    float t = (sheet * spacing - axisCamera) / axisDir;
    [loop] for (int i = 0; i < 24; ++i)
    {
      if (t >= farthest)
        break;
      float3 world = dir * t + g_Camera.xyz;
      float across = family == 0 ? world.z : world.x;
      float column = floor(across / columnWidth);
      float h1 = Hash(float2(column, sheet * 1.618 + family * 57.0));
      if (h1 < density)
      {
        float h2 = Hash(float2(column + 11.3, sheet * 2.718 + family * 13.0));
        float h3 = Hash(float2(column - 7.1, sheet * 0.577 + family * 3.0));
        float off = (frac(across / columnWidth) - 0.2 - 0.6 * h2) * columnWidth;
        float width = max(dropWidth, t * pixelAngle * 0.8);
        float cover = saturate(1.0 - abs(off) / width) * (dropWidth / width);
        if (cover > 0.0)
        {
          float speed = 26.0 + 8.0 * h3;
          float streak = speed / 60.0 * 1.5;
          float period = streak * (5.0 + 10.0 * h2);
          float at = frac((world.y + g_Camera.w * speed + h3 * 97.0) / period) * period / streak;   // 0 its foot, 1 its top
          // (as much as falls there, see RainOpenAt; looked at only for a
          // drop there: HLSL's ?: and && work out both sides)
          float open = 0.0;
          [branch] if (at < 1.0)
            open = RainOpenAt(world);
          if (open > 0.0)
          {
            float taper = smoothstep(0.0, 0.2, at) * (1.0 - 0.6 * at);
            float fade = saturate((t - nearest) / 4.0) * saturate((sceneDistance - t) / 1.0) * saturate((reach - t) / 10.0);
            amount += cover * taper * fade * open * FogAt(dir * t);
          }
        }
      }
      t += stepT;
      sheet += sheetStep;
    }
  }
  // A drop shows what's round it, blurred, and catches the light of the
  // whole scene (the frame's average): lighter than what's behind it, and
  // nearly grey, as rain is
  float3 around = g_Scene.SampleLevel(g_Linear, uv, g_Frame.z).rgb;
  float3 average = g_Scene.SampleLevel(g_Linear, float2(0.5, 0.5), g_Frame.w).rgb;
  float3 dropColor = (around * 0.55 + average * 0.45) * 1.3 + 0.02;
  dropColor = lerp(dropColor, dot(dropColor, float3(0.2126, 0.7152, 0.0722)).xxx, 0.6);
  return float4(dropColor, saturate(amount * g_DistantRain.x * 1.5));
}

float Luma(float3 c)
{
  return dot(c, float3(0.2126, 0.7152, 0.0722));
}

// Whether snow may be at a point (world): with "Keep snow out of buildings"
// on, only where the weather's squares know the sky is open (KnownOpenAt: a
// square not looked at yet isn't; past them, it is); off, anywhere
bool SnowOpenAt(float3 world)
{
  return g_SnowMisc.x < 0.5 || KnownOpenAt(world) > 0.5;
}

// The snow (2026-10-26; the game has none: Research_Snow_2026-10-26.md),
// drawn as flakes of their own after the pass (SnowVsMain, SnowPsMain),
// rebuilt after the first test (the user: "looks a bit confusing and motion
// draining"). The first build put flakes on sheets fixed in the world, every
// 0.75 - 5 m, with every flake on a sheet carried the same way and the level
// sheets all falling at 1 m/s: rows and layers, the whole field moving as one
// (strong vection: a wide field of points moving together reads as moving
// yourself), thousands of points under a pixel shimmering, flakes up to the
// lens, and every flake jumping as the camera turned. Now, as real snow:
// - each flake on its own, at a random place in a box round the camera that
//   wraps round as the camera moves (they stay put in the world): a near box
//   (16 m) and a far one (48 m, sparser, from 4 - 6 m out, slightly bigger);
// - each falling at its own 0.5 - 1.3 m/s (unrimed aggregates fall 0.4 - 1.2
//   m/s, even at one size: Locatelli & Hobbs 1974) and going its own way
//   through the air (in turbulence snowflakes follow the eddies:
//   Nemes et al. 2017), on top of the wind carrying all of it (g_SnowDrift).
//   Its eddies (2026-10-26, the user: with the wind on the snow came "from
//   only one direction" and "not from above and left/right": every flake rode
//   the one wind, about as fast as it fell, and its own loop was too small to
//   cross it): a slow one (0.4 - 1.2 m round, every 10 - 25 s) and a quick one
//   (0.1 - 0.35 m, every 2 - 5 s), each an ellipse round an upright axis of its
//   own, turned to a direction of its own and tipped (up and down 0.2 - 0.5
//   of the slow one's size), both x the turbulence (g_SnowWind.w: 0.6 in still
//   air, more as the wind blows and in a blizzard, weather::GetSnow): about
//   0.3 - 1 m/s each way, so near flakes swirl apart, some cross the wind, some
//   drop straight down, a few rise for a moment; and the flutter (1.5 - 4 cm,
//   0.6 - 1.4 a second);
// - every motion whole cycles over the snow's clock's period, so nothing
//   jumps when the clock wraps (g_SnowTiming.x);
// - none nearer than 0.8 m, faded in to 2 m (what rushes past right at the
//   lens drives the sense of motion most); faded out at the boxes' edges;
// - under a pixel: spread over 0.7 px, fainter; far ones faint and few, the
//   fog (the snowy look) doing the rest (less contrast and fine detail in the
//   distance: less vection);
// - stretched along how it moved on the screen over half a frame (the camera
//   mostly), its brightness spread along it, so a turn smears it instead of
//   making every flake jump: at most 40 px;
// - hidden behind what the game drew (soft over 0.3 m), not where the
//   weather's squares know the sky isn't open nor in the game's interiors
//   (SnowOpenAt), faded into the game's fog;
// - lit like the drawn rain: the scene's light (the frame's average) and a
//   little of what's round it.
struct SnowVertex
{
  float4 position : SV_POSITION;
  noperspective float2 along : TEXCOORD0;    // pixels from its middle: x along its streak, y across it
  nointerpolation float4 shape : TEXCOORD1;  // x: half its streak's length, y: its radius (pixels), z: how far along the view (m)
  nointerpolation float4 color : TEXCOORD2;  // rgb: its light; a: how strongly it's drawn
};

// A speed made whole cycles (or whole units of distance) over the snow's
// clock's period
float WholeOverPeriod(float perSecond, float unit)
{
  float period = g_SnowTiming.x;
  return round(perSecond * period / unit) * unit / period;
}

SnowVertex SnowVsMain(uint id : SV_VertexID)
{
  SnowVertex o;
  // A flake not drawn: all six corners in one place, nothing to fill
  o.position = float4(0.0, 0.0, 0.5, 1.0);
  o.along = 0;
  o.shape = 0;
  o.color = 0;
  uint flake = id / 6;
  uint k = id - flake * 6;
  float2 corner = float2(k == 1 || k == 4 || k == 5 ? 1.0 : -1.0, k == 2 || k == 3 || k == 5 ? 1.0 : -1.0);
  bool farBox = flake >= (uint)g_SnowBoxes.x;
  float box = farBox ? g_SnowBoxes.w : g_SnowBoxes.z;
  float seed = (float)flake * 1.37 + (farBox ? 911.0 : 0.0);
  float h0 = Hash(float2(seed, 1.7)), h1 = Hash(float2(seed, 5.3)), h2 = Hash(float2(seed, 9.1));
  float h3 = Hash(float2(seed, 13.9)), h4 = Hash(float2(seed, 17.3)), h5 = Hash(float2(seed, 21.7));
  float h6 = Hash(float2(seed, 25.1)), h7 = Hash(float2(seed, 29.9)), h8 = Hash(float2(seed, 33.3));
  float h9 = Hash(float2(seed, 37.7)), h10 = Hash(float2(seed, 41.1)), h11 = Hash(float2(seed, 45.9));
  // How many fall: each flake its own share, so more come in place as it
  // thickens (faded in over the last 3 % of it)
  float density = saturate(g_Snow.y * g_Snow.x);
  if (h0 >= density)
    return o;
  float strength = saturate((density - h0) / 0.03);
  float clock = g_Snow.w;
  float fall = WholeOverPeriod(0.5 + 0.8 * h1, box);
  // Its own way through the turbulent air (see above; g_SnowWind.w the
  // turbulence): a slow eddy and a quick one, each round its own axis, and
  // the flutter; and its velocity from them now, for its streak
  float h12 = Hash(float2(seed, 49.3)), h13 = Hash(float2(seed, 53.9)), h14 = Hash(float2(seed, 57.1));
  float h15 = Hash(float2(seed, 61.7)), h16 = Hash(float2(seed, 65.3)), h17 = Hash(float2(seed, 69.9));
  float turbulence = g_SnowWind.w;
  float w1 = WholeOverPeriod(0.04 + 0.06 * h2, 1.0) * 6.2832;
  float w3 = WholeOverPeriod(0.2 + 0.25 * h12, 1.0) * 6.2832;
  float w2 = WholeOverPeriod(0.6 + 0.8 * h3, 1.0) * 6.2832;
  float a1 = (0.4 + 0.8 * h4) * turbulence, a3 = (0.1 + 0.25 * h13) * turbulence, a2 = 0.015 + 0.025 * h5;
  float p1 = h6 * 6.2832, p2 = h7 * 6.2832, p3 = h14 * 6.2832;
  float t1 = h15 * 6.2832, t3 = h16 * 6.2832;
  float2 d1 = float2(cos(t1), sin(t1)), d3 = float2(cos(t3), sin(t3));
  float2 q1 = float2(-d1.y, d1.x), q3 = float2(-d3.y, d3.x);
  float s1 = 0.3 + 0.7 * h17, s3 = 1.0 - 0.6 * h17;
  float up1 = 0.2 + 0.3 * h16;
  float c1 = cos(w1 * clock + p1), n1 = sin(w1 * clock + p1);
  float c3 = cos(w3 * clock + p3), n3 = sin(w3 * clock + p3);
  float c2 = cos(w2 * clock + p2), n2 = sin(w2 * clock + p2);
  float3 move;
  move.xz = (d1 * c1 + q1 * n1 * s1) * a1 + (d3 * c3 + q3 * n3 * s3) * a3 + float2(c2, n2) * a2 + g_SnowDrift.xy;
  move.y = -fall * clock + up1 * a1 * n1 + 0.4 * a3 * c3 + 0.3 * a2 * sin(2.0 * w2 * clock + p2);
  float2 own = (q1 * c1 * s1 - d1 * n1) * a1 * w1 + (q3 * c3 * s3 - d3 * n3) * a3 * w3;
  float3 ownVelocity = float3(own.x, up1 * a1 * w1 * c1 - 0.4 * a3 * w3 * n3, own.y);
  float3 rel = frac((float3(h8, h9, h10) * box + move - g_Camera.xyz) / box) * box - 0.5 * box;
  float distance = length(rel);
  float fade = 1.0 - smoothstep(0.30 * box, 0.47 * box, distance);
  if (farBox)
    fade *= smoothstep(0.25 * g_SnowBoxes.z, 0.40 * g_SnowBoxes.z, distance);
  fade *= smoothstep(0.8, 2.0, distance);
  if (fade <= 0.0)
    return o;
  float4 clip = mul(float4(rel, 1.0), g_ViewProjection);
  if (clip.w < 0.3 || any(abs(clip.xy) > clip.w * 1.2))
    return o;
  if (!SnowOpenAt(rel + g_Camera.xyz))
    return o;
  float2 size = 1.0 / g_Frame.xy;
  float2 now = (clip.xy / clip.w * float2(0.5, -0.5) + 0.5) * size;
  // Where it was a frame before: its own motion (the wind, its fall and its
  // eddies: each streak its own way; in a blizzard a long one) and the
  // camera's
  float3 velocity = float3(g_SnowWind.x, -fall, g_SnowWind.y) + ownVelocity;
  float3 earlier = rel - velocity * g_SnowWind.z;
  float2 before = now;
  if (g_SnowTiming.y > 0.5)
  {
    float4 previous = mul(float4(earlier + g_SnowPrevious.xyz, 1.0), g_PrevViewProjection);
    if (previous.w > 0.3)
      before = (previous.xy / previous.w * float2(0.5, -0.5) + 0.5) * size;
  }
  else
  {
    float4 previous = mul(float4(earlier, 1.0), g_ViewProjection);
    if (previous.w > 0.3)
      before = (previous.xy / previous.w * float2(0.5, -0.5) + 0.5) * size;
  }
  float2 streak = (now - before) * 0.5;
  float length2 = length(streak);
  // (at most g_SnowMisc.y px: 40, a blizzard's 90)
  // (4K's pixels, x PixelScale: the same share of the frame at any resolution)
  float longest = (g_SnowMisc.y > 1.0 ? g_SnowMisc.y : 40.0) * PixelScale();
  if (length2 > longest)
  {
    streak *= longest / length2;
    length2 = longest;
  }
  // 6 - 14 mm across x Flake size (the far box's half again), in pixels
  float radius = (0.003 + 0.004 * h11) * g_Snow.z * (farBox ? 1.5 : 1.0);
  float radiusPx = radius * g_ProjectionXY.x * 0.5 * size.x / clip.w;
  float drawn = max(radiusPx, 0.7);
  strength *= (radiusPx / drawn) * (radiusPx / drawn) * (drawn / (drawn + length2));
  strength *= fade * FogAt(rel);
  if (strength < 0.003)
    return o;
  // Its light
  float3 around = g_Scene.SampleLevel(g_Linear, saturate(now * g_Frame.xy), g_Frame.z).rgb;
  float3 average = g_Scene.SampleLevel(g_Linear, float2(0.5, 0.5), g_Frame.w).rgb;
  float light = Luma(average) * 1.3 + Luma(around) * 0.2 + 0.03;
  float3 tint = lerp(float3(0.94, 0.97, 1.0), around / max(Luma(around), 1e-3), 0.2);
  // The quad round its streak (from before to now), a pixel of margin
  float2 u = length2 > 0.01 ? streak / length2 : float2(1.0, 0.0);
  float2 v = float2(-u.y, u.x);
  float2 extent = float2(length2 * 0.5 + drawn + 1.0, drawn + 1.0);
  float2 at = now - streak * 0.5 + u * corner.x * extent.x + v * corner.y * extent.y;
  o.position = float4((at * g_Frame.xy - 0.5) * float2(2.0, -2.0), 0.5, 1.0);
  o.along = corner * extent;
  o.shape = float4(length2 * 0.5, drawn, -dot(rel, g_CameraBack.xyz), 0.0);
  o.color = float4(min(tint * light, 1.0), saturate(strength) * 0.9);
  return o;
}

float4 SnowPsMain(SnowVertex v) : SV_TARGET
{
  // Round, moving along its streak: how far from the streak's middle line
  float d = length(float2(max(abs(v.along.x) - v.shape.x, 0.0), v.along.y));
  float cover = 1.0 - smoothstep(v.shape.y * 0.35, v.shape.y + 0.5, d);
  // Behind what the game drew: not seen (soft over 0.3 m)
  float depth = DepthAt(v.position.xy * g_Frame.xy);
  if (!IsSky(depth))
    cover *= saturate((ViewDepth(depth) - v.shape.z) / 0.3);
  float a = cover * v.color.a;
  if (a < 0.002)
    discard;
  return float4(v.color.rgb, a);
}

// The lightning bolts' light at a pixel (pixels on the frame): each a solid
// core and a glow round it, looked at only inside its box; a bolt is hidden
// where something nearer than its strike is
float BoltAt(float2 pixel, bool sky, float horizontalDistance)
{
  if (g_Bolt.x <= 0.0)
    return 0;
  float total = 0;
  int bolts = (int)g_Bolt.w;
  [loop] for (int k = 0; k < bolts; ++k)
  {
    float4 box = g_BoltBox[k];
    if (any(pixel < box.xy) || any(pixel > box.zw))
      continue;
    if (!sky && horizontalDistance < g_BoltRange[k].z * 0.9)
      continue;
    float core = 0, glow = 0;
    int first = (int)g_BoltRange[k].x;
    int last = first + (int)g_BoltRange[k].y;
    [loop] for (int i = first; i < last; ++i)
    {
      float2 a = g_BoltSegment[i].xy, b = g_BoltSegment[i].zw;
      float2 ab = b - a;
      float t = saturate(dot(pixel - a, ab) / max(dot(ab, ab), 1e-4));
      float d = length(pixel - (a + ab * t));
      float w = g_BoltWidth[i].x;
      float bright = g_BoltWidth[i].y;
      core = max(core, bright * saturate(1.5 - d / w));
      glow += bright * exp(-d / (w * 6.0)) * 0.08;
    }
    // Through the fog: a bolt is bright enough to light it up, so never all gone
    total += (core + min(glow, 0.8)) * lerp(0.3, 1.0, g_BoltRange[k].w);
  }
  return total;
}

// Gradient noise (Perlin's, 2D, the quintic fade): smooth rises and
// falls without value noise's square cells and flat-topped blobs (the
// 06:3x build's snow, the user's screenshot: "i think thats the height
// map"). About 0 - 1, 0.5 on average.
float2 GradientAt(float2 i)
{
  float a = Hash(i) * 6.2832;
  return float2(cos(a), sin(a));
}

float GNoise(float2 p)
{
  float2 i = floor(p), f = frac(p);
  float2 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
  float a = dot(GradientAt(i), f), b = dot(GradientAt(i + float2(1.0, 0.0)), f - float2(1.0, 0.0));
  float c = dot(GradientAt(i + float2(0.0, 1.0)), f - float2(0.0, 1.0)), e = dot(GradientAt(i + 1.0), f - 1.0);
  return 0.5 + 0.75 * lerp(lerp(a, b, u.x), lerp(c, e, u.x), u.y);
}

// The snow's surface (0 - 1 about, x its height in metres, SnowGround):
// wind-shaped drifts 4 - 8 m across, softer swells about 2 m, a little
// unevenness under a metre, each octave turned against the others so no
// grid shows. (The unevenness halved, 2026-10-26, the user: "snow noise
// pattern should be more smoothed out".)
float SnowHeight(float2 p)
{
  float2 q = float2(p.x * 0.8 - p.y * 0.6, p.x * 0.6 + p.y * 0.8);
  float2 r = float2(p.x * 0.28 - p.y * 0.96, p.x * 0.96 + p.y * 0.28);
  return GNoise(p / 5.5) * 0.67 + GNoise(q / 1.9 + 7.0) * 0.29 + GNoise(r / 0.65 + 13.0) * 0.04;
}

// Where the snow lies in patches while it settles (SnowGround; WetLook.cpp's
// SnowLiesAt has the same): gradient noise 2.2 m and, turned, 0.9 m across,
// 0 - 1 about. (2026-10-26, the user: "snow noise pattern should be more
// smoothed out": it was value noise 1.7 m and 0.43 m across, its blobs on a
// square grid and the small ones speckling the patches' edges.)
float SnowPatchNoise(float2 p)
{
  float2 q = float2(p.x * 0.8 - p.y * 0.6, p.x * 0.6 + p.y * 0.8);
  return GNoise(p / 2.2) * 0.7 + GNoise(q / 0.9 + 23.0) * 0.3;
}

float SoleShape(float2 d, float2 f)
{
  // (soft at its edge: a print's sides slope, 06:3x)
  float along = dot(d, f), across = dot(d, float2(-f.y, f.x));
  float2 front = float2((along - 0.055) / 0.10, across / 0.06);
  float2 heel = float2((along + 0.095) / 0.055, across / 0.05);
  return max(saturate((1.0 - dot(front, front)) * 2.5), saturate((1.0 - dot(heel, heel)) * 2.5));
}

// Faith's prints in the snow (2026-10-26, the user: "can we do the same thing
// as the water footseps but with snow?"; g_SnowPrint, the way she was going
// g_SnowPrintDir): each pressed g_SnowPile.z deep, as much as is left of it
// (the falling snow fills it, WetLook.cpp). x: the print's floor seen here;
// y: its inner wall seen here; z: the rim of snow pushed up round it. The ray into a print meets its floor where
// the floor's shape, moved back along the view by its depth, is under it,
// else the far wall.
// How deep the snow lies at a place (2026-10-26, the user: "check if the snow
// can have different variations of height"): the depth it has piled up to
// (g_SnowPile.x) x 0.45 - 1.55 by a slow noise over the ground, its humps and
// hollows 5 - 10 m across, so it lies deeper in some places than others (on
// average as deep as it's piled)
float SnowDepthAt(float2 xz)
{
  return g_SnowPile.x * (0.45 + 1.1 * Noise(xz / 6.0 + 101.0));
}

// Faith's slides through the snow (2026-10-26, the user: "just like
// footprints there should be a slide trail"; g_SnowTrail, WetLook.cpp): each
// stretch a groove as wide as she is (g_SnowTrailEnd.w each side), pressed
// as deep as her prints are, the falling snow filling it as theirs (w). x:
// the groove's floor seen here; y: its inner wall (the floor moved back
// along the view by its depth, as SnowPrintsAt's); z: the snow pushed up
// along its sides (a little brighter).
float3 SnowTrailAt(float3 rel, float3 view, float depth)
{
  // Her body's outline (2026-10-26, the user: "snow slides should have faiths
  // body outline instead of circles"): she slides feet first, on her hips and
  // legs, so the snow is pressed in a trough as wide as her hips (each
  // stretch's half width, g_SnowTrailEnd.w: 19 cm, ANSUR II women's hip
  // breadth about 37 cm), from where her hips were when she began (WetLook.cpp
  // puts the first stretch 0.9 m behind her feet) to her last 0.5 m, which
  // are her two legs side by side (each 15 cm wide, 9 cm either side of her
  // middle) ending at her feet: a trail's last stretch has w < 0. One groove
  // along the whole trail: the nearest of its stretches' outlines makes it
  // (each a distance to its outline, less than 0 inside), the floor seen
  // through it likewise, and how much is left of it is the stretches' near
  // it, weighted by how near.
  // (none of the stretches' own tests lets a point outside the box round
  // them all through, MakeListBounds)
  if (OffBox(rel, g_SnowTrailBounds[0], g_SnowTrailBounds[1]))
    return 0;
  int n = (int)g_SnowTrailCount.x;
  float2 shift = view.xz / max(view.y, 0.3) * depth;
  float nearest = 1e9, nearestFloor = 1e9, leftSum = 0.0, weightSum = 0.0, halfWidth = 0.19;
  [loop] for (int k = 0; k < n; ++k)
  {
    float4 s = g_SnowTrail[k], e = g_SnowTrailEnd[k];
    if (abs(rel.y - (s.y + e.y) * 0.5) > 0.6 + 0.5 * abs(s.y - e.y))
      continue;
    bool last = e.w < 0.0;
    float hw = abs(e.w);
    halfWidth = hw;
    float2 ab = e.xz - s.xz;
    float len = length(ab);
    float2 way = len > 1e-4 ? ab / len : float2(1.0, 0.0);
    float2 across = float2(-way.y, way.x);
    float2 pa = rel.xz - s.xz;
    if (abs(dot(pa, across)) > hw + 0.6 || dot(pa, way) < -hw - 0.6 || dot(pa, way) > len + hw + 0.6)
      continue;
    float2 pf = pa - shift * s.w;
    float dk, dfk;
    if (!last)
    {
      dk = length(pa - ab * saturate(dot(pa, ab) / max(len * len, 1e-6))) - hw;
      dfk = length(pf - ab * saturate(dot(pf, ab) / max(len * len, 1e-6))) - hw;
    }
    else
    {
      // The hips' trough to 0.5 m before her feet, then her legs
      float hipEnd = max(len - 0.5, 0.0);
      float2 hips = way * hipEnd;
      float2 legStart = hips, legEnd = ab;
      float2 legs[2] = { across * 0.09, -across * 0.09 };
      dk = length(pa - hips * saturate(dot(pa, hips) / max(hipEnd * hipEnd, 1e-6))) - hw;
      dfk = length(pf - hips * saturate(dot(pf, hips) / max(hipEnd * hipEnd, 1e-6))) - hw;
      [unroll] for (int j = 0; j < 2; ++j)
      {
        float2 l0 = legStart + legs[j] * 0.8, l1 = legEnd + legs[j];
        float2 lv = l1 - l0;
        float ll = max(dot(lv, lv), 1e-6);
        float2 q = pa - l0, qf = pf - l0;
        dk = SmoothMin(dk, length(q - lv * saturate(dot(q, lv) / ll)) - 0.075, 0.05);
        dfk = SmoothMin(dfk, length(qf - lv * saturate(dot(qf, lv) / ll)) - 0.075, 0.05);
      }
    }
    nearest = min(nearest, dk);
    nearestFloor = min(nearestFloor, dfk);
    float weight = 1.0 - saturate((dk + hw) / (hw + 0.5));
    leftSum += s.w * weight;
    weightSum += weight;
  }
  if (weightSum <= 0.0)
    return 0;
  float left = leftSum / weightSum;
  float inside = smoothstep(0.0, -0.07, nearest);
  float floorSeen = smoothstep(0.0, -0.07, nearestFloor);
  // (its floor rounded: a little darker towards the sides)
  float rounded = 1.0 - 0.5 * saturate(1.0 + nearest / halfWidth) * saturate(1.0 + nearest / halfWidth);
  return float3(left * min(inside, floorSeen) * rounded, left * saturate(inside - floorSeen),
    left * smoothstep(0.16, 0.02, nearest) * (1.0 - inside));
}

// How far down a body went into the snow at a point of a stamp (g_SnowStamps:
// t its texels, across and along; its slice): 0 - 1, bilinear between
// texels; 0 off it
float StampPress(float2 t, float slice)
{
  if (any(t < 0.0) || t.x > 128.0 || t.y > 256.0)
    return 0.0;
  return g_SnowStamps.SampleLevel(g_Linear, float3(t / float2(128.0, 256.0), slice), 0);
}

// The snow pressed in by bodies (2026-10-27, the user: "The snow slide should
// be exactly faiths mesh instead of what it is now", "Ksec ... when they die
// and ragdoll they should leave behind their outline in the snow", "faiths
// snow handprints should be visible when she jump rolls / lands";
// g_SnowStamp, SnowBodies.cpp): where a body's own mesh, posed as the game
// had it, went down into the snow, pressed as far as it went there (0 at the
// snow's top - 1 the ground; as deep as her prints at most, depth), the
// falling snow filling it as theirs (w). x: the floor seen here; y: its inner
// wall (the floor moved back along the view by its depth there, as
// SnowPrintsAt's); z: the snow pushed up round it (within 6 cm).
float3 SnowStampsAt(float3 rel, float3 view, float depth)
{
  float3 best = 0;
  // (as SnowTrailAt's)
  if (OffBox(rel, g_SnowStampBounds[0], g_SnowStampBounds[1]))
    return best;
  int n = (int)g_SnowStampCount.x;
  float2 shiftWorld = view.xz / max(view.y, 0.3) * depth;
  [loop] for (int k = 0; k < n; ++k)
  {
    float4 s = g_SnowStamp[k], a = g_SnowStampAxis[k];
    float2 across = float2(-a.y, a.x);
    float2 d = rel.xz - s.xz;
    float2 t = float2(dot(d, across), dot(d, a.xy)) / 0.02;
    if (any(t < -4.0) || t.x > 132.0 || t.y > 260.0)
      continue;
    // (on the ground it was pressed into: its rise along it)
    if (abs(rel.y - (s.y + a.z * t.y * 0.02)) > 0.5)
      continue;
    float inside = StampPress(t, a.w);
    float2 shift = float2(dot(shiftWorld, across), dot(shiftWorld, a.xy)) / 0.02 * s.w;
    float floorSeen = StampPress(t - shift * inside, a.w);
    float rim = max(max(StampPress(t + float2(3.0, 0.0), a.w), StampPress(t - float2(3.0, 0.0), a.w)),
      max(StampPress(t + float2(0.0, 3.0), a.w), StampPress(t - float2(0.0, 3.0), a.w)));
    best.x = max(best.x, s.w * min(inside, floorSeen));
    best.y = max(best.y, s.w * saturate(inside - floorSeen));
    best.z = max(best.z, s.w * saturate(rim - inside) * (1.0 - inside));
  }
  return best;
}

// The distance to a capsule from a (radius ra) to b (radius rb) on the
// ground, less than 0 inside (the uneven capsule's, I. Quilez's)
float TaperedCapsule(float2 p, float2 a, float2 b, float ra, float rb)
{
  float2 ab = b - a;
  float h = max(length(ab), 1e-4);
  float2 way = ab / h, across = float2(-way.y, way.x);
  float2 q = float2(abs(dot(p - a, across)), dot(p - a, way));
  float k0 = (ra - rb) / h;
  float s = sqrt(max(1.0 - k0 * k0, 0.0));
  float k = dot(q, float2(-k0, s));
  if (k < 0.0)
    return length(q) - ra;
  if (k > s * h)
    return length(q - float2(0.0, h)) - rb;
  return dot(q, float2(s, k0)) - ra;
}

// Her body's print from a roll (2026-10-26, the user: "Rolling/jump rolling
// should also leave faiths shape on the snow"): she rolls over one shoulder
// and across her back to the other hip (the shoulder's mark first, behind,
// the hip's ahead, len apart), so the snow is pressed in her back's shape:
// 26 cm wide at her shoulder blade to 32 cm at her hip (ANSUR II women's
// biacromial breadth about 36 cm and hip breadth 37: the part of the back a
// roll rests on is narrower), with her shoulder and upper arm rounded out at
// its side. Not measured from her model or her roll's animation. d: from its
// middle; way: the way she rolled (unit); side: the shoulder's side (unit).
// The distance to its outline, less than 0 inside.
float RollBody(float2 d, float2 way, float2 side, float len)
{
  float2 shoulder = -way * len * 0.5 + side * 0.07, hip = way * len * 0.5 - side * 0.05;
  float dist = TaperedCapsule(d, shoulder, hip, 0.13, 0.16);
  float2 arm = shoulder - way * 0.03 + side * 0.08;
  return SmoothMin(dist, length(d - arm) - 0.09, 0.05);
}

// Faith's rolls in the snow (g_SnowRoll, WetLook.cpp): each her body's print
// (RollBody) pressed as deep as given, the falling snow filling it as her
// prints (w). x: its floor seen here; y: its inner wall (the floor moved back
// along the view by its depth, as SnowPrintsAt's); z: the snow pushed up
// round it.
float3 SnowRollsAt(float3 rel, float3 view, float depth)
{
  float3 best = 0;
  // (as SnowTrailAt's)
  if (OffBox(rel, g_SnowRollBounds[0], g_SnowRollBounds[1]))
    return best;
  int n = (int)g_SnowRollCount.x;
  float2 shift = view.xz / max(view.y, 0.3) * depth;
  [loop] for (int k = 0; k < n; ++k)
  {
    float4 r = g_SnowRoll[k], w = g_SnowRollWay[k];
    float3 d = rel - r.xyz;
    if (abs(d.y) > 0.5 || dot(d.xz, d.xz) > 1.0)
      continue;
    float len = length(w.xy);
    float2 way = w.xy / max(len, 1e-4);
    float dk = RollBody(d.xz, way, w.zw, len);
    float dfk = RollBody(d.xz - shift * r.w, way, w.zw, len);
    float inside = smoothstep(0.0, -0.05, dk), floorSeen = smoothstep(0.0, -0.05, dfk);
    // (its floor rounded as her back is: a little darker towards the sides)
    float rounded = 1.0 - 0.45 * saturate(1.0 + dk / 0.15) * saturate(1.0 + dk / 0.15);
    best.x = max(best.x, r.w * min(inside, floorSeen) * rounded);
    best.y = max(best.y, r.w * saturate(inside - floorSeen));
    best.z = max(best.z, r.w * smoothstep(0.14, 0.02, dk) * (1.0 - inside));
  }
  return best;
}

// Faith's hand's print (2026-10-26, the user: "faiths hands should also
// leave handprints behind make them model accurate", then "hand prints
// should match faiths hands"): her own hand, measured from the game's data
// (Research_Snow_2026-10-26.md section 16; the snow session's scratchpad,
// hand\): her arms mesh (characters/faith/arms/faith_arms_mesh, its skin and
// glove sections) on her skeleton's bind pose
// (characters/skeletons/skeleton_faith), laid flat (each finger straightened
// along its joints, the thumb laid beside the palm), its outline drawn.
// - Her hand: 17 cm from the heel to the middle finger's tip, 7.2 cm across
//   the knuckles.
// - Her fingers from their knuckle joints: index 8.3 cm, middle 9.2, ring
//   8.7, little 6.8; 17 - 12 mm wide (the little one 15 - 10), turned a
//   little to the little finger's side as hers are.
// - Her thumb: 10 cm from its base, 30 degrees out from her hand, with its
//   pad.
// Made of a rounded palm, tapered fingers, the thumb's pad and the thumb;
// they cover 92 % of her outline's area (what's left is mostly the wrist).
// d: from its middle (her outline's); f: the way her fingers point; thumb:
// the thumb's side (unit, world x, z). Soft over its last 6 mm.
float HandShape(float2 d, float2 f, float2 thumb)
{
  float2 q = float2(dot(d, f), dot(d, thumb));
  // The palm: these corners rounded 8 mm (a convex polygon's distance, I.
  // Quilez's)
  const float2 palm[5] = { float2(-0.0639, -0.0239), float2(-0.0079, -0.0301), float2(0.0003, 0.0260),
    float2(-0.0155, 0.0265), float2(-0.0655, 0.0119) };
  float closest = dot(q - palm[0], q - palm[0]), inside = 1.0;
  int j = 4;
  [unroll] for (int i = 0; i < 5; ++i)
  {
    float2 e = palm[j] - palm[i], w = q - palm[i];
    float2 b = w - e * saturate(dot(w, e) / dot(e, e));
    closest = min(closest, dot(b, b));
    bool3 c = bool3(q.y >= palm[i].y, q.y < palm[j].y, e.x * w.y > e.y * w.x);
    if (all(c) || all(!c))
      inside = -inside;
    j = i;
  }
  float dist = inside * sqrt(closest) - 0.008;
  // Her fingers (index, middle, ring, little): from their knuckle joints to
  // their tips' rounds, their radius at each end
  const float4 fingerLine[4] = { float4(0.0000, 0.0240, 0.0767, 0.0208), float4(0.0020, 0.0054, 0.0861, -0.0067),
    float4(-0.0012, -0.0136, 0.0782, -0.0268), float4(-0.0071, -0.0307, 0.0546, -0.0406) };
  const float2 fingerRadius[4] = { float2(0.0085, 0.0063), float2(0.0088, 0.0065), float2(0.0080, 0.0060),
    float2(0.0073, 0.0050) };
  [unroll] for (int k = 0; k < 4; ++k)
    dist = SmoothMin(dist, TaperedCapsule(q, fingerLine[k].xy, fingerLine[k].zw, fingerRadius[k].x, fingerRadius[k].y),
      0.004);
  // Her thumb's pad, then her thumb
  dist = SmoothMin(dist, TaperedCapsule(q, float2(-0.0608, 0.0086), float2(-0.0229, 0.0328), 0.0125, 0.0095), 0.005);
  dist = SmoothMin(dist, TaperedCapsule(q, float2(-0.0181, 0.0346), float2(0.0192, 0.0584), 0.0080, 0.00525), 0.004);
  return saturate(0.5 - dist / 0.006);
}

// How much of her own hand pressed a point of a hand stamp (2026-10-28, the
// user: "handprints for snow/rain should be faithful to faiths hand";
// g_HandStamps, SnowBodies.cpp: her arms mesh's own hand, its skin and glove,
// posed as the game had it at the touch, where it came within 1.5 cm of its
// nearest point to the surface): t its texels (U, V from its middle, + 32),
// slice; 0 - 1, bilinear; 0 off it
float HandStampPress(float2 t, float slice)
{
  if (any(t < 0.0) || any(t > 64.0))
    return 0.0;
  return g_HandStamps.SampleLevel(g_Linear, float3(t / 64.0, slice), 0);
}

// Faith's wet handprints in the rain (2026-10-26, the user: "Can you make
// the handprints also work for rain?"; g_HandPrint, WetLook.cpp's
// NoteRainHand): her hand (HandShape, the snow's) in the plane of the ground
// or the wall she touched, within 3 cm of it, as wet as the print is now
float HandPrintsAt(float3 rel)
{
  float best = 0.0;
  int n = (int)g_HandCount.x;
  [loop] for (int k = 0; k < n; ++k)
  {
    float4 p = g_HandPrint[k];
    float3 d = rel - p.xyz;
    if (dot(d, d) > 0.04)
      continue;
    float3 f = g_HandFinger[k].xyz, t = g_HandThumb[k].xyz;
    if (abs(dot(d, cross(f, t))) > 0.03)
      continue;
    best = max(best, p.w * HandShape(float2(dot(d, f), dot(d, t)), float2(1.0, 0.0), float2(0.0, 1.0)));
  }
  // and her own hand's (g_HandStampAt w under 0: HandStampPress), within 3 cm
  // of the surface's plane
  int stamped = (int)g_HandStampCount.x;
  [loop] for (int j = 0; j < stamped; ++j)
  {
    float4 s = g_HandStampAt[j];
    if (s.w >= 0.0)
      continue;
    float3 u = g_HandStampU[j].xyz, v = g_HandStampV[j].xyz;
    float3 d = rel - s.xyz;
    if (dot(d, d) > 0.04 || abs(dot(d, cross(u, v))) > 0.03)
      continue;
    best = max(best, -s.w * HandStampPress(float2(dot(d, u), dot(d, v)) / 0.004 + 32.0, g_HandStampU[j].w));
  }
  return best;
}

// Faith's prints in the snow (g_SnowPrint: w how much of it is left, less
// than 0 a handprint's; g_SnowPrintDir: xy the way she was going / her
// fingers point, zw that foot's outer side / the thumb's side): her shoe's
// sole (ShoeSole, 2026-10-26, the user: "Rain/snow footsteps should have
// faiths shoes footprint") or her hand (HandShape), pressed g_SnowPile.z
// deep (a hand half as deep). x: the print's floor seen here; y: its inner
// wall seen here; z: the rim of snow pushed up round it; w: the tread's lugs
// pressed into the floor (1 a lug's mark, 0 a groove's ridge).
float4 SnowPrintsAt(float3 rel, float3 view, float depthScale)
{
  float4 best = float4(0.0, 0.0, 0.0, 1.0);
  // (each list's only where a point is inside the box round what of it can
  // reach, MakeListBounds: outside, none of its own tests lets one through)
  int n = OffBox(rel, g_SnowPrintBounds[0], g_SnowPrintBounds[1]) ? 0 : (int)g_SnowPile.y;
  [loop] for (int k = 0; k < n; ++k)
  {
    float4 p = g_SnowPrint[k];
    float3 d = rel - p.xyz;
    if (abs(d.y) > 0.35 || dot(d.xz, d.xz) > 0.09)
      continue;
    float2 f = g_SnowPrintDir[k].xy, side = g_SnowPrintDir[k].zw;
    bool hand = p.w < 0.0;
    float left = abs(p.w);
    float2 shift = view.xz / max(view.y, 0.3) * g_SnowPile.z * depthScale * left * (hand ? 0.5 : 1.0);
    float shape, floorSeen, wide, tread = 1.0;
    if (hand)
    {
      shape = HandShape(d.xz, f, side);
      floorSeen = HandShape(d.xz - shift, f, side);
      wide = HandShape(d.xz * 0.85, f, side);
    }
    else
    {
      float2 sole = ShoeSole(d.xz, f, side);
      shape = sole.x;
      tread = sole.y;
      floorSeen = ShoeSole(d.xz - shift, f, side).x;
      wide = ShoeSole(d.xz * 0.82, f, side).x;
    }
    if (shape <= 0.0 && wide <= 0.0)
      continue;
    float floorHere = left * min(shape, floorSeen);
    if (floorHere > best.x)
    {
      best.x = floorHere;
      best.w = tread;
    }
    best.y = max(best.y, left * saturate(shape - floorSeen));
    best.z = max(best.z, saturate(left) * saturate(wide - shape));
  }
  // The KrugerSec's boots (g_OtherSnowPrint, WetLook.cpp's OtherSnowStep):
  // BootSole, as deep as her prints
  int others = OffBox(rel, g_BootPrintBounds[0], g_BootPrintBounds[1]) ? 0 : (int)g_SnowStampCount.y;
  [loop] for (int j = 0; j < others; ++j)
  {
    float4 p = g_OtherSnowPrint[j];
    float3 d = rel - p.xyz;
    if (abs(d.y) > 0.35 || dot(d.xz, d.xz) > 0.09)
      continue;
    float2 f = g_OtherSnowPrintDir[j].xy, side = g_OtherSnowPrintDir[j].zw;
    float left = p.w;
    float2 shift = view.xz / max(view.y, 0.3) * g_SnowPile.z * depthScale * left;
    float shape = BootSole(d.xz, f, side);
    float floorSeen = BootSole(d.xz - shift, f, side);
    float wide = BootSole(d.xz * 0.84, f, side);
    if (shape <= 0.0 && wide <= 0.0)
      continue;
    float floorHere = left * min(shape, floorSeen);
    if (floorHere > best.x)
    {
      best.x = floorHere;
      best.w = 1.0;
    }
    best.y = max(best.y, left * saturate(shape - floorSeen));
    best.z = max(best.z, saturate(left) * saturate(wide - shape));
  }
  // Her own hand's prints (g_HandStampAt w over 0: HandStampPress, her hand
  // as the game posed it where it touched; half as deep as a shoe's, as the
  // drawn hand's): its floor through the parallax in the stamp's plane, its
  // inner wall, the snow pushed up round it (within 1.6 cm)
  int stamped = OffBox(rel, g_HandStampBounds[0], g_HandStampBounds[1]) ? 0 : (int)g_HandStampCount.x;
  [loop] for (int h = 0; h < stamped; ++h)
  {
    float4 s = g_HandStampAt[h];
    if (s.w <= 0.0)
      continue;
    float3 u = g_HandStampU[h].xyz, v = g_HandStampV[h].xyz, facing = cross(u, v);
    float3 d = rel - s.xyz;
    if (abs(dot(d, facing)) > 0.35 || dot(d, d) > 0.09)
      continue;
    float left = s.w, slice = g_HandStampU[h].w;
    float3 shift = float3(view.x, 0.0, view.z) / max(view.y, 0.3) * g_SnowPile.z * depthScale * left * 0.5;
    float2 t = float2(dot(d, u), dot(d, v)) / 0.004 + 32.0;
    float2 tShift = float2(dot(shift, u), dot(shift, v)) / 0.004;
    float shape = HandStampPress(t, slice);
    float floorSeen = HandStampPress(t - tShift, slice);
    float wide = max(max(HandStampPress(t + float2(4.0, 0.0), slice), HandStampPress(t - float2(4.0, 0.0), slice)),
      max(HandStampPress(t + float2(0.0, 4.0), slice), HandStampPress(t - float2(0.0, 4.0), slice)));
    if (shape <= 0.0 && wide <= 0.0)
      continue;
    float floorHere = left * min(shape, floorSeen);
    if (floorHere > best.x)
    {
      best.x = floorHere;
      best.w = 1.0;
    }
    best.y = max(best.y, left * saturate(shape - floorSeen));
    best.z = max(best.z, saturate(left) * saturate(wide - shape));
  }
  return best;
}

// The snow's light at a point (SnowGround, and the snow on the ice): its
// colour before its own shading. (2026-10-27, the user: "snow still looks
// blurry and it just looks like a gray thing on the ground find a better
// shader again look up how actual snow is".) Fresh snow reflects 80 - 90 %
// of the light, nearly evenly every way, so under an overcast sky it's about
// as bright as the sky by the horizon: why snow and sky run together in a
// whiteout (F. Kasten, Polarforschung 1963, on the Greenland ice sheet).
// - Its light: the sky's by the horizon as the frame shows it
//   (g_HazeView.yzw: the frame's sky, held steady on the CPU, HazeSkyAt) x
//   0.9: what lights snow in a shadow, and all of it under an overcast sky.
//   Where the sun shows through it, the ground's own light there x 1.9
//   (around: SnowAroundAt, its own surface 6 px round), so the game's own
//   shadows lie on the snow as they lie on the ground, as sharp, and a
//   sunlit patch is brighter. Until the sky's been seen, the ground's own
//   light alone, as before.
// - It was the ground's light alone: on dark asphalt, a fifth of the light,
//   the snow came out a mid grey, the ground's own light blurred and
//   brightened.
// - White, a little cool; a little bluer in shade (less of the sun's light
//   in it).
// (2026-10-27, the user: "Snow becomes grey on edge and if i don't look at
// it directly find a fix", "Shadows bug out the snow effect fix it no
// mistakes": it was a quarter darker and bluer where its spot was darker
// than the frame 128 px round it (average, SceneAround's widest). So a
// shadow wider than that was lit inside as if it weren't there, with a dark
// band along its edge; and by an edge or towards the horizon, where that
// blur took in the sky or the bright distance, the snow came out grey.)
float3 SnowLit(float3 around, float3 average)
{
  float ground = Luma(around);
  if (g_HazeView.y < 0.0)
  {
    float shaded = smoothstep(0.4, 1.0, ground / max(Luma(average), 1e-3));
    float lit = max(ground * 1.9, Luma(average) * 0.85);
    return lerp(float3(0.86, 0.91, 1.0), float3(0.95, 0.97, 1.0), 0.4 + 0.6 * shaded) * min(lit, 1.0);
  }
  float skyLight = dot(g_HazeView.yzw, float3(0.2126, 0.7152, 0.0722)) * 0.9;
  float sunLight = ground * 1.9;
  float sunlit = smoothstep(1.0, 1.3, sunLight / max(skyLight, 1e-3));
  return lerp(float3(0.89, 0.93, 1.0), float3(0.95, 0.97, 1.0), sunlit) * min(max(skyLight, sunLight), 1.0);
}

// The game's own surface at a pixel (its G-buffer, GameFrame.h): its world
// facing and smoothness; false where there's none this frame, or what's
// there isn't a normal (its basis not one of the 6, or its x and y off the
// unit disc: a target the game cleared). (2026-11-01, read from the game's
// own shaders: a city material shader puts the normal's x and y, in the
// basis its normal's cube map picks, x 0.5 + 0.5 into its target 0, and
// that basis / 6 into its colour target's alpha; the sun's light pass takes
// it back with z = sqrt(1 - x^2 - y^2), the world normal's c-th part that
// dotted with the basis' c-th float4.) px: the G-buffer's texel, the depth's
// (DepthTexel: the frame's pixel at 100 %).
bool GameSurfaceAt(int2 px, out float3 n, out float smoothness)
{
  n = float3(0.0, 1.0, 0.0);
  smoothness = 0.0;
  if (g_GameFrame.x < 0.5)
    return false;
  float4 g0 = g_GameNormals.Load(int3(px, 0));
  float basis = g_GameColour.Load(int3(px, 0)).a * 6.0;
  float k = round(basis);
  float2 xy = g0.xy * 2.0 - 1.0;
  if (abs(basis - k) > 0.1 || k > 5.5 || dot(xy, xy) > 1.05)
    return false;
  float3 local = float3(xy, sqrt(saturate(1.0 - dot(xy, xy))));
  int i = (int)k * 3;
  n = float3(dot(local, g_NormalBasis[i].xyz), dot(local, g_NormalBasis[i + 1].xyz), dot(local, g_NormalBasis[i + 2].xyz));
  float length0 = length(n);
  if (length0 < 0.5)
    return false;
  n /= length0;
  smoothness = g0.z;
  return true;
}

// The light reaching a surface, as the game lit it (2026-11-01, the user:
// "snow looks blurry if not looking it directly or on bright places" and
// "check if snow is reflection based if yes find a better way because that
// doesn't work"). It was (SnowLit) the picture of the ground 6 - 20 px
// round, on a quarter-size level, x 1.9: a blur of whatever the ground
// showed, its reflections too, which grow towards a glancing view and are
// brightest where it's bright, and which took it over white there, with no
// shading left. Now: the picture at the pixel (2 x 2) over what the surface
// itself gives back of the light, by the game's G-buffer: its diffuse colour
// (its base colour, none of it a metal's) under its mirror share (Fresnel:
// the reflectance's 0.16 r^2, rising towards 1 at a glancing view as far as
// it's smooth), with what it mirrors taken as about as bright as what lights
// it. So its texture and paint are taken out of the light, and its mirror
// image mostly. In linear light (the picture's values ^ 2.2). trust: how
// far it tells (none where too little is given back to tell, less where it's
// mostly a mirror or a metal). spread: its 2 x 2 pixels that many apart (1:
// side by side; PileLight's further apart, a wider patch of the ground).
// px and spread in the G-buffer's texels (the depth's, DepthTexel), each
// texel's picture the frame's pixel it's in (FramePixelOf; the same at 100 %).
float3 GameLightAt(int2 px, float3 view, float3 n, float smoothness, int spread, out float trust)
{
  float nv = saturate(dot(n, view));
  float glancing = pow(1.0 - nv, 5.0);
  int2 last = DepthLast();
  float3 picture = 0.0, given = 0.0, diffuse = 0.0;
  float mirrored = 0.0, metal = 0.0;
  [unroll] for (int k = 0; k < 4; ++k)
  {
    int3 at = int3(min(px + int2(k & 1, k >> 1) * spread, last), 0);
    float3 base = g_GameColour.Load(at).rgb;
    float4 material = g_GameMaterial.Load(at);
    float3 f0 = lerp((0.16 * material.z * material.z).xxx, base, material.y);
    float3 mirror = f0 + (max(smoothness.xxx, f0) - f0) * glancing;
    given += base * (1.0 - material.y) * (1.0 - mirror) + mirror;
    diffuse += base * (1.0 - material.y);
    picture += pow(saturate(g_Scene.Load(int3(FramePixelOf(at.xy), 0)).rgb), 2.2);
    mirrored += Luma(mirror);
    metal += material.y;
  }
  trust = smoothstep(0.04, 0.1, Luma(given) * 0.25) * (1.0 - smoothstep(0.25, 0.6, mirrored * 0.25)) *
    (1.0 - smoothstep(0.3, 0.8, metal * 0.25));
  // How bright the light is: the picture's brightness over what's given
  // back's. Its colour, the picture over what's given back channel by
  // channel, only where the surface's own colour is near grey (its
  // saturation under 0.15, none from 0.4): on a coloured one a channel it
  // gives back little of was divided by little, and anything else in it (the
  // sky mirrored, a pixel of TAA) came out the light's colour, the opposite
  // of the surface's (2026-11-01, the user: "Faiths shoes turn blue on snow
  // fix it": red shoes, cyan snow)
  float bright = Luma(picture) / max(Luma(given), 0.02);
  float3 tint = clamp((picture / max(given, 0.02)) / max(bright, 1e-4), 0.6, 1.5);
  float most = max(diffuse.r, max(diffuse.g, diffuse.b));
  float saturation = (most - min(diffuse.r, min(diffuse.g, diffuse.b))) / max(most, 1e-3);
  return bright * lerp(float3(1.0, 1.0, 1.0), tint, 1.0 - smoothstep(0.15, 0.4, saturation));
}

// Light rolled off from 0.8 towards 0.97: never over white, so the snow's
// shading still shows where it's brightest
float3 RollOffWhite(float3 c)
{
  return c < 0.8 ? c : 0.8 + 0.17 * (1.0 - exp(-(c - 0.8) / 0.17));
}

// The snow's light from the game's (GameLightAt) at a pixel: rgb its
// colour, a how far it's taken over SnowLit's (1 where the G-buffer's there:
// SnowLit then isn't needed). What fresh snow gives back (0.85) of the light
// there, the light's colour a little greyer (what's left of the ground's
// own where the division isn't exact), back in the picture's values, rolled
// off from 0.8 towards 0.97 so it doesn't go over white and its shading
// shows. Where it can't be told (trust: too little given back, a mirror, a
// metal; or far in the fog, which the picture has in it), the sky's light by
// the horizon (SnowLit's without the ground's): never the ground's picture.
// SnowFromGameLight: from GameLightAt's light and trust (fog: FogAt there).
float4 SnowFromGameLight(float3 light, float trust, float fog)
{
  trust *= fog * fog;
  // (x Snow brightness, g_GameFrame.y, in linear light: 1 fresh snow's own
  // 0.85 of it, 1.75 by default; 2026-11-01, the user: "Snow looks good now
  // but it's not as apparent of an effect now can we increase it?": at 0.85
  // a mid-grey ground's snow was about 0.57 of white where the old light,
  // the ground's picture x 1.9, made it about 0.86)
  float bright = g_GameFrame.y > 0.0 ? g_GameFrame.y : 1.0;
  float3 snow = RollOffWhite(pow(max(0.85 * bright * lerp(Luma(light).xxx, light, 0.7), 0.0), 1.0 / 2.2));
  if (g_HazeView.y < 0.0)
    return float4(snow, trust);
  float skyLight = dot(g_HazeView.yzw, float3(0.2126, 0.7152, 0.0722)) * 0.9;
  float3 sky = RollOffWhite(float3(0.89, 0.93, 1.0) * min(skyLight, 1.0) * pow(bright, 1.0 / 2.2));
  return float4(lerp(sky, snow, trust), 1.0);
}

// (px: the G-buffer's texel, DepthTexel)
float4 SnowLitByGame(int2 px, float3 view, float fog)
{
  float3 n;
  float smoothness;
  if (!GameSurfaceAt(px, n, smoothness) || dot(n, view) < -0.2)
    return 0;
  float trust;
  float3 light = GameLightAt(px, view, n, smoothness, 1, trust);
  return SnowFromGameLight(light, trust, fog);
}

// The light on the piled snow's side (PsMain's, see SnowPileSide): the
// game's light on the ground just in front of its foot, the same for every
// pixel over a foot and going smoothly along it (2026-11-03, the user, with
// a screenshot of a gravel roof: "Snow is still on walls streched fix it no
// mistakes": along a box's foot the pile was streaks a pixel to three wide,
// the gravel's light and dark drawn up the wall. Its light was 3 taps of 2
// x 2 pixels 8 px out from each column's own foot; on gravel every stone is
// lit its own way, so each column took other stones' light, and drew it up
// its whole height.) Now read on a lattice fixed on the frame, 16 px apart
// (at 4K, x PixelScale), each node the game's light over 2 x 2 pixels 4 px
// apart (GameLightAt, SnowFromGameLight), and the 4 nodes round the point
// 8 px out from the foot blended by how near it they are (bilinear): the
// next column takes the same nodes, a little more of one and less of
// another. Only nodes on the ground there count: facing up in the G-buffer
// and within 0.2 m + 1 % of the distance of the foot's height, not the wall,
// a box's top or what's seen past an edge. Where none counts (blended in as
// their share goes under 0.15), or without the G-buffer: SnowLit of the
// frame round the foot (average, SceneAround's B-spline, smooth too) x Snow
// brightness against its default, as SnowGround takes it. footUv: the foot
// on the frame; footY: its height (relative to the camera); fog: FogAt
// there. (The lattice on the frame's pixels, each node read at its pixel's
// G-buffer and depth texel, DepthTexel, its 2 x 2 texels 4 px apart in
// texels: the same at 100 %.)
float4 PileLight(float2 footUv, float footY, float3 view, float fog, float distance, float3 average)
{
  float oldBright = g_GameFrame.z > 0.0 ? g_GameFrame.z : 1.0;
  float3 without = SnowLit(average, average) * oldBright;
  if (g_GameFrame.x < 0.5)
    return float4(without, 1.0);
  float scale = PixelScale();
  float lattice = max(4.0, round(16.0 * scale));
  int spread = (int)max(1.0, round(4.0 * scale * g_Depth.x));
  int2 last = int2(round(1.0 / g_Frame.xy)) - 1;
  // (in pixels, a pixel's centre at its whole number)
  float2 at = footUv / g_Frame.xy + float2(0.0, 8.0 * scale) - 0.5;
  float2 cell = floor(at / lattice);
  float2 f = at / lattice - cell;
  float slack = 0.2 + 0.01 * distance;
  float3 sum = 0.0;
  float weights = 0.0;
  [unroll] for (int k = 0; k < 4; ++k)
  {
    float2 corner = float2(k & 1, k >> 1);
    float2 share = lerp(1.0 - f, f, corner);
    int2 px = clamp(int2((cell + corner) * lattice), 0, last);
    float2 nodeUv = (float2(px) + 0.5) * g_Frame.xy;
    int2 texel = DepthTexel(nodeUv);
    float3 n;
    float smoothness;
    if (!GameSurfaceAt(texel, n, smoothness) || n.y < 0.7)
      continue;
    bool sky;
    float3 p = RelativeAtTexel(nodeUv, sky);
    if (sky || abs(p.y - footY) > slack)
      continue;
    float trust;
    float3 light = GameLightAt(texel, view, n, smoothness, spread, trust);
    float4 snow = SnowFromGameLight(light, trust, fog);
    float w = share.x * share.y;
    sum += lerp(without, snow.rgb, snow.a) * w;
    weights += w;
  }
  float3 nodes = sum / max(weights, 1e-4);
  return float4(lerp(without, nodes, smoothstep(0.0, 0.15, weights)), 1.0);
}

// The ground's marks at a point (world; GroundMarks.cpp's map, one sample):
// x how far the snow is pressed there (in the rain: how wet a tyre track left
// it), y how stirred the water is (0 - 1); only on what's at the mark's own
// height (within 0.4 - 0.8 m of the ground it was made on: the map is seen
// from above, so a step under an awning or a car under a bridge isn't on its
// top), that height the nearest texel's (a mark's edge blended with the
// cleared texels round it isn't at any height; the weather session,
// 2026-11-01); faded out on a circle over the last fifth of the map's reach
// (g_MarksWindow), not cut on its square (seen from high up, a square's edge
// shows: the weather session, 2026-11-01, as the rain's own edge)
static const int MarksMapTexels = 2048;           // GroundMarks.h's MarksTexels
static const float MarksHeightRange = 1200.0;     // and MarksHeightRange
float2 MarksAt(float3 world)
{
  float r = length(world.xz - g_MarksWindow.yz);
  if (r >= g_MarksWindow.w)
    return 0.0;
  float2 uv = world.xz * g_MarksWindow.x;
  float2 marks = g_Marks.SampleLevel(g_Wrap, uv, 0).rg;
  int2 texel = (int2)floor(uv * MarksMapTexels) & (MarksMapTexels - 1);
  float height = g_Marks.Load(int3(texel, 0)).a * MarksHeightRange + g_MarksOn.w;
  float level = 1.0 - smoothstep(0.4, 0.8, abs(world.y - height));
  return marks * level * (1.0 - smoothstep(g_MarksWindow.w * 0.8, g_MarksWindow.w, r));
}

// The snow lying at a point of the ground: rgb its colour, a how much of it
// covers the point (the patches as it settles). Rebuilt 2026-10-26 06:3x
// (the user: the snow "looks really bad", with a screenshot of the ground
// mottled like rippling water: "i think thats the height map"): its relief
// was value noise taken as up to a metre of height and shaded down to
// x 0.72 and blue, so every few decimetres it went from white to blue-grey;
// and its light was the pixel itself (x 1.8), so the ground's own texture
// and reflections showed through. Snow is smooth and bright, its shading
// soft (it's white all through and the overcast sky lights it from
// everywhere). Now:
// - Its surface: SnowHeight (gradient noise) x 5 - 30 cm, more the deeper
//   it lies there (SnowDepthAt): drifts 4 - 8 m across and softer swells,
//   their slopes taken over 15 cm. Lit from the sky, a little more from one
//   side (so the drifts show): +- a few percent; the hollows of the big
//   drifts a little darker (x 0.95).
// - Its light: the frame round the point (around: SnowAroundAt, its own
//   surface 6 pixels round at 4K), x 1.9 (snow is brighter than what it lies on), at least 0.85 of the
//   light of its own surroundings (average: PsMain gives SceneAround's widest,
//   about 128 px round at 4K): so it's in the shadow where the ground is, but
//   no texture under it shows. A cool white, a little bluer where it's in
//   shadow against its surroundings. (That was the whole frame's average
//   until 2026-10-27, the user: "looking up make the shader grey": the
//   bright sky filling the frame raised it, so the snow lower down counted as
//   in shadow, darker and bluer.)
// - Up close a fine grain (x 0.975 - 1.025, 3.5 cm) and glints: one 3 cm
//   cell in 12 has a crystal tilted at random that catches the sky's light
//   towards the camera (its cosine to the 80th), a dot 4 mm across or a
//   pixel. Both fade where a pixel covers more of the ground than they'd
//   show (2 cm and 3 cm), so far snow is smooth, without shimmer.
// - Where a patch ends (while it settles), its edge a touch darker (x 0.93).
//   Once 2 - 8 cm lie there are no patches.
// - Faith's prints (SnowPrintsAt) and slide trails (SnowTrailAt): their
//   floor packed down, only a little greyer (x 0.9 - 0.97); their inner
//   wall in shade (x 0.78 - 0.9); the snow pushed up round them a little
//   brighter.
float4 SnowGround(float3 world, float3 view, float3 normal, float flat, float distance, float3 color, float3 around,
  float3 average, float4 lit)
{
  float pixelAngle = 2.0 * g_Frame.x / g_ProjectionXY.x;
  float footprint = distance * pixelAngle / max(abs(dot(view, normal)), 0.2);
  float detail = saturate(1.0 - footprint / 0.08);
  float depthHere = SnowDepthAt(world.xz);
  float height = 0.05 + 0.25 * saturate(depthHere / 0.6);
  float h0 = SnowHeight(world.xz);
  // (its parallax at most 2 x its height: at 4 x the patches and the grain
  // swam over the ground at a glancing view)
  float2 p = world.xz + view.xz / max(view.y, 0.5) * (h0 - 0.5) * height * detail;
  float c = g_SnowDrift.z;
  float n = SnowPatchNoise(p);
  float patch = smoothstep(1.0 - c - 0.16, 1.0 - c + 0.16, n * 0.85 + flat * 0.15);
  patch = max(patch, smoothstep(0.85, 1.0, c));
  patch = max(patch, smoothstep(0.02, 0.08, depthHere));
  if (patch <= 0.002)
    return 0;
  // Its surface: the drifts' slopes on the ground's own facing, lit from
  // the sky a little more from one side
  const float e = 0.15;
  float hx = SnowHeight(p + float2(e, 0.0)) - SnowHeight(p - float2(e, 0.0));
  float hz = SnowHeight(p + float2(0.0, e)) - SnowHeight(p - float2(0.0, e));
  float3 surface = normalize(normal + float3(-hx, 0.0, -hz) / (2.0 * e) * height);
  const float3 sky = float3(0.29, 0.92, 0.26);
  float shade = 1.0 + 1.1 * (dot(surface, sky) - dot(normal, sky));
  shade *= lerp(0.95, 1.02, saturate(h0 * 1.4 - 0.2));
  // Its edge where a patch ends
  float rim = smoothstep(0.02, 0.3, patch) * (1.0 - smoothstep(0.3, 0.75, patch));
  shade *= 1.0 - 0.07 * rim;
  // Up close, its grain (gradient noise: value noise's square cells showed;
  // only where a pixel is under a third of a grain, so it doesn't crawl)
  float grainDetail = saturate(1.0 - footprint / 0.012);
  float2 grainAt = float2(world.x * 0.6 - world.z * 0.8, world.x * 0.8 + world.z * 0.6);
  shade *= 1.0 + 0.04 * (GNoise(grainAt / 0.035 + 71.0) - 0.5) * grainDetail;
  // Wind ripples (sastrugi: what the wind leaves in lying snow): crests
  // 0.28 m apart, wandering, a centimetre or so high, lit from the sky
  // (+- 1.5 %). Their way turns slowly from place to place (over 25 m) and
  // stays put: not the gusting wind's, which would turn them as it swings.
  // Only near (gone by 2 cm a pixel), so far off the snow stays smooth.
  // (+- 3.5 % and 3 cm until 2026-10-27, the user: "We are almost there turn
  // down the grey stripes a bit".)
  float rippleDetail = saturate(1.0 - footprint / 0.02);
  if (rippleDetail > 0.0)
  {
    float turn = 0.6 + 1.2 * Noise(world.xz / 25.0 + 41.0);
    float crest = dot(world.xz, float2(cos(turn), sin(turn))) / 0.28 + Noise(world.xz / 1.3 + 17.0) * 2.0;
    shade *= 1.0 + 0.015 * cos(crest * 6.2832) * (0.6 + 0.4 * Noise(world.xz / 0.7 + 5.0)) * rippleDetail;
  }
  // Its light (SnowLit: as bright as an overcast sky by the horizon, x 0.9)
  float scene = max(Luma(average), 1e-3);
  // (lit: the game's light there, SnowLitByGame, as far as it's there;
  // without it SnowLit's, x Snow brightness against its default,
  // g_GameFrame.z)
  float oldBright = g_GameFrame.z > 0.0 ? g_GameFrame.z : 1.0;
  float3 snow = (lit.a < 0.999 ? lerp(SnowLit(around, average) * oldBright, lit.rgb, lit.a) : lit.rgb) * shade;
  // Glints
  float2 cell = floor(world.xz / 0.03);
  float g1 = Hash(cell + 3.1), g2 = Hash(cell + 9.7), g3 = Hash(cell + 15.3);
  // (only close by, where a glint is more than a pixel: pixel-sized ones
  // read as noise)
  float glintDetail = saturate(1.0 - footprint / 0.012);
  if (g3 < 0.06 && glintDetail > 0.0)
  {
    float3 facet = normalize(float3(g1 - 0.5, 1.4, g2 - 0.5));
    float3 halfway = normalize(view + float3(0.0, 1.0, 0.0));
    float2 at = frac(world.xz / 0.03) - (0.5 + 0.6 * (float2(g1, g2) - 0.5));
    float dot0 = saturate(1.0 - length(at) * 0.03 / max(0.002, footprint * 0.5));
    float glint = pow(saturate(dot(facet, halfway)), 80.0) * dot0 * glintDetail;
    // (the light round it the game's where that's taken: around isn't
    // worked out then, so the glints were dimmer)
    snow += glint * (scene * 1.5 + Luma(lit.a > 0.5 ? lit.rgb : around) * 0.8);
  }
  // Faith's prints and slide trails (pressed as deep as the snow lies here
  // against how deep it's piled on average)
  float depthScale = g_SnowPile.x > 0.001 ? depthHere / g_SnowPile.x : 1.0;
  [branch] if ((g_SnowPile.y > 0.5 || g_SnowStampCount.y > 0.5 || g_HandStampCount.x > 0.5) && !Skipped(SkipSnowPressed))
  {
    float4 pressed = SnowPrintsAt(world - g_Camera.xyz, view, depthScale);
    // (the tread: its lugs pressed a little deeper, darker; the grooves left
    // as ridges, lighter; only close by)
    float treadDetail = saturate(1.0 - footprint / 0.006);
    float3 floorTint = float3(0.90, 0.93, 0.97) * lerp(1.0, lerp(1.03, 0.93, pressed.w), treadDetail);
    snow = lerp(snow, snow * floorTint, saturate(pressed.x));
    snow = lerp(snow, snow * float3(0.78, 0.83, 0.90), saturate(pressed.y));
    snow *= 1.0 + 0.04 * saturate(pressed.z);
  }
  [branch] if (g_SnowTrailCount.x > 0.5 && !Skipped(SkipSnowPressed))
  {
    float3 trail = SnowTrailAt(world - g_Camera.xyz, view, g_SnowPile.z * depthScale);
    snow = lerp(snow, snow * float3(0.90, 0.93, 0.97), saturate(trail.x));
    snow = lerp(snow, snow * float3(0.78, 0.83, 0.90), saturate(trail.y));
    snow *= 1.0 + 0.05 * saturate(trail.z);
  }
  // Her rolls (SnowRollsAt): a body presses less deep than a foot (x 0.8)
  [branch] if (g_SnowRollCount.x > 0.5 && !Skipped(SkipSnowPressed))
  {
    float3 roll = SnowRollsAt(world - g_Camera.xyz, view, g_SnowPile.z * depthScale * 0.8);
    snow = lerp(snow, snow * float3(0.90, 0.93, 0.97), saturate(roll.x));
    snow = lerp(snow, snow * float3(0.78, 0.83, 0.90), saturate(roll.y));
    snow *= 1.0 + 0.05 * saturate(roll.z);
  }
  // Bodies pressed in by their own meshes (SnowStampsAt): Faith's slides,
  // rolls and hands, the KrugerSec lying in it
  [branch] if (g_SnowStampCount.x > 0.5 && !Skipped(SkipSnowPressed))
  {
    float3 body = SnowStampsAt(world - g_Camera.xyz, view, g_SnowPile.z * depthScale);
    snow = lerp(snow, snow * float3(0.90, 0.93, 0.97), saturate(body.x));
    snow = lerp(snow, snow * float3(0.78, 0.83, 0.90), saturate(body.y));
    snow *= 1.0 + 0.05 * saturate(body.z);
  }
  // The crowd's steps and the traffic's tyre tracks (MarksAt's x; 2026-11-01,
  // the user: "Do the same about snow too"): their floor packed down as her
  // prints' (as deep as the snow lies here against its average), their edge
  // (the map's own soft one, 2 - 3 cm) in shade as a print's wall. One sample:
  // the edge from the mark's own value there. Faded out where a pixel covers
  // more than 8 - 20 cm of the ground (the map's 3 cm texels would shimmer).
  [branch] if (g_MarksOn.x > 0.5 && !Skipped(SkipSnowPressed))
  {
    float marked = MarksAt(world).x * (1.0 - smoothstep(0.08, 0.2, footprint));
    if (marked > 0.002)
    {
      float floorPressed = marked * saturate(depthScale);
      float wall = 4.0 * marked * (1.0 - marked) * saturate(depthScale);
      snow = lerp(snow, snow * float3(0.90, 0.93, 0.97), saturate(floorPressed));
      snow = lerp(snow, snow * float3(0.78, 0.83, 0.90), saturate(wall * 0.7));
    }
  }
  return float4(saturate(snow), patch);
}

// Whether the frame's surface at uv is a top snow lies on (SnowPileSide):
// facing up and seen from above (its points 2 px down and aside about as
// high; turned towards the camera, its y up: a ceiling isn't, the user,
// "snow shouldn't be on sealings"). p: its point (relative to the camera).
// (2026-10-27, the user: "fix the snow's between-pixel sampling
// (RelativeAtTexel) and scale its pixel radii with the resolution": a point
// between pixels took its texel's depth on the ray through itself, off the
// surface by up to half a pixel's depth step, which looking along the
// ground thinned the snow's edges; and its radii were 4K's pixels at any
// resolution.)
bool SnowTopAt(float2 uv, out float3 p)
{
  p = 0;
  if (uv.y >= 1.0)
    return false;
  bool sky, skyBelow, skyAside;
  p = RelativeAtTexel(uv, sky);
  if (sky)
    return false;
  float3 below = RelativeAtTexel(uv + float2(0.0, 2.0 * g_Frame.y), skyBelow);
  float3 aside = RelativeAtTexel(uv + float2(2.0 * g_Frame.x, 0.0), skyAside);
  if (skyBelow || skyAside)
    return false;
  float3 across = cross(aside - p, below - p);
  float l = length(across);
  if (dot(across, -p) < 0.0)
    across = -across;
  return l > 1e-6 && across.y > 0.8 * l;
}

// The side of the snow piled up on what's under the open sky (2026-10-26, the
// user: the snow on the ground should "look like it's piling up"): a layer
// as deep as it lies there (SnowDepthAt) on every snowed top, against the foot
// of what stands on it: a wall, a curb, a box. What's seen there is the
// snow's own surface in front of it, the ground's snow going on up to it (the
// user: "piled up snow looks weird"), so it's drawn as the ground's snow is
// (PsMain: SnowGround at seenAt), solid, its edge soft over 1.5 pixels, the
// edge rising and falling in smooth swells (x 0.85 - 1.15 of the depth, 0.9 m
// across).
// Its foot: down the frame from the pixel to the first top it stands on,
// looked for in 5 steps as far as the depth can be tall there (128 px at most;
// none under 1.5 px), then the row where the top begins found exactly, in
// whole rows, by halving.
// The top must go on below that row (its points 3 px and half the depth's
// height further down on its plane) and be the pixel's own surface's foot
// (within twice the depth and 0.25 m of it across the ground). The pixel is
// under the snow where it's lower above that foot than the snow lies there.
// (2026-10-26 19:xx, the user, with a screenshot of white stripes up a stack
// of boxes: "depth issue is still there": the 16:xx build looked at 4 fixed
// steps down the frame and took the first top it hit, so a thin top, the
// gap between two boxes, was hit only on the rows a step happened to land on
// it: 4 bands, the depth's height / 4 apart, toothed at their ends.)
// (2026-11-03, the user: "Snow depth map still looks weird find a way and
// fix it", the stripes on walls, as in their 11-01 screenshot of a
// parapet's foot: a band of dark columns crossed by fine lines. Three
// things in it made stripes. Its look was the ground's snow at its foot,
// one point for every pixel up a column, so the grain, the glints and the
// ripples there were drawn up the wall as streaks. Its foot was found to
// within a sixth of a step (1.6 px at 4K), so the next pixel up often had
// another row, and the look and the light jumped from row to row. And the
// search went 64 px at most: within about 3.6 m of a wall the pile's top
// was cut there, straight and hard.)
// 0 - 1: how much of the pixel it covers. snowAt: its foot (relative to the
// camera); seenAt: the drift's surface the pixel sees (relative to the
// camera): the snow against a wall leans on it, a slope of about 45 degrees
// from as high as it lies down to the ground's snow, so the ray meets it in
// front of the wall, lower pixels further out; wallward: how high up the
// layer the pixel is (0 at its foot, 1 where the snow meets the wall);
// footUv: its foot on the frame.
float SnowPileSide(float2 uv, float3 here, float distance, out float3 snowAt, out float3 seenAt, out float wallward,
  out float2 footUv)
{
  snowAt = 0;
  seenAt = here;
  wallward = 0;
  footUv = uv;
  // (looked for as far as the deepest it lies anywhere: SnowDepthAt's 1.55 x,
  // its swells' 1.15 x)
  float pixelAngle = 2.0 * g_Frame.x / g_ProjectionXY.x;
  float tall = g_SnowPile.x * 1.8 / max(distance * pixelAngle, 1e-5);
  if (tall < 1.5)
    return 0;
  // (128, 64 and 3 below are 4K's pixels, x PixelScale; whole rows, so every
  // row read is a texel's own and the foot found is the same row for every
  // pixel over it)
  float scale = PixelScale();
  float reachMost = 128.0 * scale;
  bool cut = tall > reachMost;
  float reach = max(1.0, floor(min(tall, reachMost)));
  // (the rows read together where they can be, not each after the one
  // before: 2026-11-02, the user: "Check the snow performance test and find
  // a way to improve performance like the rain session also". The 5 steps
  // all at once, the first top among them taken; the first two halvings'
  // three rows at once; then one row at a time until the top's first row is
  // found: 3 more at 4K, where 128 px has steps of 26 rows (4 at most). Its
  // top's point read again with the rows under it, below.)
  float stepRow[5];
  bool stepTop[5];
  [unroll] for (int s = 0; s < 5; ++s)
  {
    float3 p;
    stepRow[s] = max(1.0, round(reach * (s + 1) / 5.0));
    stepTop[s] = SnowTopAt(uv + float2(0.0, stepRow[s] * g_Frame.y), p);
  }
  // (lo: a row that isn't a top, or the pixel's own; hi: a row that is)
  float lo = 0.0, hi = -1.0;
  [unroll] for (int k = 4; k >= 0; --k)
    if (stepTop[k])
    {
      hi = stepRow[k];
      lo = k > 0 ? stepRow[max(k - 1, 0)] : 0.0;
    }
  if (hi < 0.0)
    return 0;
  float3 unused;
  float mid = floor((lo + hi) * 0.5);
  float midLow = floor((lo + mid) * 0.5), midHigh = floor((mid + hi) * 0.5);
  bool midTop = mid > lo && SnowTopAt(uv + float2(0.0, mid * g_Frame.y), unused);
  bool lowTop = midLow > lo && SnowTopAt(uv + float2(0.0, midLow * g_Frame.y), unused);
  bool highTop = midHigh > mid && SnowTopAt(uv + float2(0.0, midHigh * g_Frame.y), unused);
  if (midTop)
    hi = mid;
  else
    lo = mid;
  float second = midTop ? midLow : midHigh;
  if (midTop ? lowTop : highTop)
    hi = second;
  else
    lo = max(lo, second);
  [unroll] for (int h = 0; h < 4; ++h)
    if (hi - lo > 1.0)
    {
      float row = floor((lo + hi) * 0.5);
      if (SnowTopAt(uv + float2(0.0, row * g_Frame.y), unused))
        hi = row;
      else
        lo = row;
    }
  // (the top's point: where it was found, the frame's surface at that row)
  bool topSky;
  float3 top = RelativeAtTexel(uv + float2(0.0, hi * g_Frame.y), topSky);
  // (a top that goes on: 3 px and half the depth's height below its first row
  // (32 px at most), on its plane)
  float3 b1, b2;
  if (!SnowTopAt(uv + float2(0.0, (hi + 3.0 * scale) * g_Frame.y), b1) ||
      !SnowTopAt(uv + float2(0.0, (hi + max(3.0 * scale, min(reach, 64.0 * scale) * 0.5)) * g_Frame.y), b2))
    return 0;
  float3 topWorld = top + g_Camera.xyz;
  float depthPiled = SnowDepthAt(topWorld.xz);
  float planeSlack = 0.03 + 0.01 * length(top);
  if (abs(b1.y - top.y) > planeSlack + 0.2 * length(b1.xz - top.xz) ||
      abs(b2.y - top.y) > planeSlack + 0.2 * length(b2.xz - top.xz))
    return 0;
  // (the pixel's own foot: near it across the ground, and the pixel over it)
  if (length(here.xz - top.xz) > depthPiled * 2.0 + 0.25)
    return 0;
  float over = here.y - top.y;
  float height = depthPiled * (0.85 + 0.3 * GNoise(topWorld.xz / 0.9 + 37.0));
  if (over < -0.02 || over > height * 1.3)
    return 0;
  float open = min(1.0 - smoothstep(0.25, 0.75, ShelterAt(topWorld)), saturate((KnownOpenAt(topWorld) - 0.25) * 2.0));
  float cover = open * saturate((height - over) / max(distance * pixelAngle * 1.5, 0.005));
  // (where the pile is taller on the frame than the search goes, close by,
  // faded out over the search's last 30 %: the pixels over it can't find
  // their foot, so it ended in a hard straight line there)
  if (cut)
    cover *= 1.0 - smoothstep(0.7 * reach, reach, hi);
  if (cover <= 0.002)
    return 0;
  snowAt = top;
  // (the ray back from the wall, (height - over) / (across + down) of it
  // along, meets the 45 degree drift: across + down at least 0.25, so
  // looking up at it the point stays near)
  float3 dir = here / max(distance, 1e-4);
  seenAt = here - dir * (max(height - over, 0.0) / max(length(dir.xz) - dir.y, 0.25));
  wallward = saturate(over / max(height, 1e-3));
  footUv = uv + float2(0.0, hi * g_Frame.y);
  return cover * (1.0 - KeptDry(top, length(top)));
}

// The frame round a pixel, blurred: the scene copy's level levelsDown under
// its blur level (g_Frame.z; 2 down: about 240 px across, so 16 px at 4K),
// read as a cubic B-spline over its texels (4 reads) instead of straight
// (2026-10-26, the user: "snow noise pattern should be more smoothed out"):
// read straight, that level's texels showed as 16-pixel squares and
// diamonds in the snow's light, which crawled over it as the camera moved.
float3 SceneAround(float2 uv, float levelsDown)
{
  uint level = (uint)max(0.0, floor(g_Frame.z + 0.5) - levelsDown);
  uint width, height, levels;
  g_Scene.GetDimensions(level, width, height, levels);
  float2 size = float2(width, height);
  float2 st = uv * size - 0.5;
  float2 i = floor(st), f = st - i;
  float2 f2 = f * f, f3 = f2 * f;
  float2 w0 = (1.0 - 3.0 * f + 3.0 * f2 - f3) / 6.0;
  float2 w1 = (4.0 - 6.0 * f2 + 3.0 * f3) / 6.0;
  float2 w2 = (1.0 + 3.0 * f + 3.0 * f2 - 3.0 * f3) / 6.0;
  float2 w3 = f3 / 6.0;
  float2 g0 = w0 + w1, g1 = w2 + w3;
  float2 p0 = (i - 1.0 + w1 / g0 + 0.5) / size, p1 = (i + 1.0 + w3 / g1 + 0.5) / size;
  return g0.y * (g0.x * g_Scene.SampleLevel(g_Linear, float2(p0.x, p0.y), level).rgb +
    g1.x * g_Scene.SampleLevel(g_Linear, float2(p1.x, p0.y), level).rgb) +
    g1.y * (g0.x * g_Scene.SampleLevel(g_Linear, float2(p0.x, p1.y), level).rgb +
    g1.x * g_Scene.SampleLevel(g_Linear, float2(p1.x, p1.y), level).rgb);
}

// How much of the snow on the ground (and the ice over the water) is drawn at
// a distance (m): all of it to three quarters of how far it reaches
// (g_SnowDrift.w, "On the ground up to"), fading out over the last quarter
// (2026-10-27, the user, with a screenshot from a rooftop: "when i'm really
// far you can see the frozen water shader / snow layer": it thinned evenly
// from the camera out, half of it at 200 m of the 400 m, so from high up the
// ground was nearly bare and only the ice, drawn however far, stood out)
float SnowReach(float distance)
{
  return 1.0 - smoothstep(0.75 * g_SnowDrift.w, g_SnowDrift.w, distance);
}

// How much a point (relative to the camera) is Faith's hand where it's just
// touched (g_SnowHandKeep, WetLook.cpp's g_HandKeep: her hand impacts'
// contacts, held 0.8 s after, gone by 1.5 s): over the touch by 2.5 cm or
// more (1.5 - 3.5 cm, soft) and under 0.6 m, within 0.27 m of it across
// (0.22 - 0.32). Kept as the game drew it, no snow: what's that close over a
// hand's touch is her hand, not the ground; the snow under it, and its
// print, stay. (2026-10-27, the user: "Faiths hands get white when she
// interacts with the ground they shouldn't". Her own box, dry::FaithShape,
// starts 12 cm over her feet, so the snow and her prints stay round them, and
// the depth's dips under half the snow's depth count as buried: a hand on
// the ground was ground.)
float HandKeptAt(float3 rel)
{
  float kept = 0.0;
  [unroll] for (int k = 0; k < 2; ++k)
  {
    float4 h = g_SnowHandKeep[k];
    if (h.w <= 0.0)
      continue;
    float3 d = rel - h.xyz;
    kept = max(kept, h.w * (1.0 - smoothstep(0.22, 0.32, length(d.xz))) * smoothstep(0.015, 0.035, d.y) *
      (1.0 - smoothstep(0.5, 0.6, d.y)));
  }
  return kept;
}

// How much a point (relative to the camera) is Faith's shoes (g_SnowFeetKeep:
// her feet, her transform's place): over them by 3 cm or more (2 - 4 cm,
// soft), under 20 cm (18 - 24), within 0.43 m of them across (0.38 - 0.48).
// Kept as the game drew it, no snow. The ground at her feet is level with
// them, so its snow and her prints stay. (2026-10-27, the user: "You can also
// see faiths triangles when snow is at her shoes": her own box,
// dry::FaithShape, starts 12 cm over her feet for the snow round them, so the
// snow lay on her shoes' up-facing triangles, one by one.)
// How much a point (relative to the camera) is one of her shoes by her own
// foot bones (g_SnowShoe: each foot's ankle and toe base, SnowBodies.cpp),
// wherever her feet are: with her legs out ahead (sliding, sitting, looking
// down at them) they were past FeetKeptAt's 0.43 m and the snow lay on them
// (2026-11-01, the user: "Faiths shoes turn blue on snow fix it"). Her shoe
// in LOD0's bind pose (scratchpad body\shoe.py): every vertex within 9.9 cm
// of the line from 2.6 cm behind the ankle to 5.9 cm past the toe base; its
// sole 9.1 cm under the ankle, 2.1 cm under the toe base. So: within 10 cm
// of that line (from 3 cm behind to 6.5 cm past; soft to 13 cm), and over
// the sole by 1 - 2.5 cm (the ground she stands on, and its snow round her
// shoe, is the sole's level).
float ShoesKeptAt(float3 rel)
{
  float kept = 0.0;
  [unroll] for (int f = 0; f < 2; ++f)
  {
    float4 ankle = g_SnowShoe[f * 2], toe = g_SnowShoe[f * 2 + 1];
    float3 axis = toe.xyz - ankle.xyz;
    float axisLength = length(axis);
    if (ankle.w <= 0.0 || toe.w <= 0.0 || axisLength < 0.05 || axisLength > 0.3)
      continue;
    float3 way = axis / axisLength;
    float3 a = ankle.xyz - way * 0.03;
    float3 ab = toe.xyz + way * 0.065 - a;
    float along = saturate(dot(rel - a, ab) / dot(ab, ab));
    float off = length(rel - (a + ab * along));
    float sole = min(ankle.y - 0.091, toe.y - 0.021);
    kept = max(kept, (1.0 - smoothstep(0.10, 0.13, off)) * smoothstep(sole + 0.01, sole + 0.025, rel.y));
  }
  return kept;
}

float FeetKeptAt(float3 rel)
{
  float shoes = ShoesKeptAt(rel);
  if (g_SnowFeetKeep.w <= 0.0)
    return shoes;
  float3 d = rel - g_SnowFeetKeep.xyz;
  return max(shoes, g_SnowFeetKeep.w * (1.0 - smoothstep(0.38, 0.48, length(d.xz))) * smoothstep(0.02, 0.04, d.y) *
    (1.0 - smoothstep(0.18, 0.24, d.y)));
}

// The light round a point of the snow (SnowGround's around; 2026-10-26 21:xx,
// the user: "Snow looks blurry"): the frame 6 pixels round it (at 4K; 8 taps
// of the scene copy's level 1, 2 x 2 pixels each, and the point's own), each
// as much as it's on the point's own surface (off its plane by under 2 % of
// the distance + 5 cm). So a shadow on the snow is about 6 px soft, and what
// stands on it doesn't bleed its colour into it. It was SceneAround's 16-px
// blur of everything: shadows on the snow 16 - 32 px soft, and a halo of what
// stood on it round it. Its texture still doesn't show: 2 x 2 texels 6 px
// round. (8 px until 2026-10-27, the user: "snow/ice looks ... blurry /
// fuzzy"; the ice is lit by it too now.) here, normal: the point and its
// surface's facing.
// (2026-10-27, the user: "fix the snow's between-pixel sampling
// (RelativeAtTexel) and scale its pixel radii with the resolution": a point
// between pixels took its texel's depth on the ray through itself, off the
// surface by up to half a pixel's depth step, which looking along the
// ground thinned the snow's edges; and its radii were 4K's pixels at any
// resolution.)
float3 SnowAroundAt(float2 uv, float3 here, float3 normal, float distance)
{
  float2 step = 6.0 * PixelScale() * g_Frame.xy;
  float3 sum = g_Scene.SampleLevel(g_Linear, uv, 1.0).rgb;
  float weights = 1.0;
  float slack = 0.02 * distance + 0.05;
  [unroll] for (int k = 0; k < 8; ++k)
  {
    float a = k * 0.7854 + 0.3927;
    float2 at = saturate(uv + float2(cos(a), sin(a)) * step);
    bool sky;
    float3 p = RelativeAtTexel(at, sky);
    float w = sky ? 0.0 : saturate(1.0 - abs(dot(p - here, normal)) / slack);
    sum += g_Scene.SampleLevel(g_Linear, at, 1.0).rgb * w;
    weights += w;
  }
  return sum / weights;
}

// The light on the snow's own surface round a point (SnowGround's around,
// SnowLit's sunlight; 2026-10-27, the user: "snow still looks off on concrete
// surfeces and gravel fix it high priority no istakes"): SnowAroundAt's 6 px
// round (counted twice) and 8 points 20 px round (at 4K, PixelScale), each the
// scene copy's level 2 (4 x 4 pixels), each as much as it's on the point's own
// surface (off its plane by under 5 cm + 2 % of the distance, and half as
// deep as the snow lies there: the stones of gravel under it count), not the
// sky. In sunlight the snow's light was the ground's own 6 px round, so
// concrete's stains and seams and gravel's light and dark stones showed
// through the snow as its own; deep snow's was the frame 32 px round,
// whatever was on it (a wall, the sky past an edge). A shadow on the snow is
// about 20 px soft. here, normal: the point and its surface's facing;
// depthHere: how deep the snow lies there (SnowDepthAt).
float3 SnowLightAround(float2 uv, float3 here, float3 normal, float distance, float depthHere)
{
  float3 sum = SnowAroundAt(uv, here, normal, distance) * 2.0;
  float weights = 2.0;
  float slack = 0.05 + 0.02 * distance + 0.5 * depthHere;
  float2 step = 20.0 * PixelScale() * g_Frame.xy;
  [unroll] for (int k = 0; k < 8; ++k)
  {
    float2 at = uv + RingDirections[k] * step;
    if (any(at < 0.0) || any(at > 1.0))
      continue;
    bool sky;
    float3 p = RelativeAtTexel(at, sky);
    float w = sky ? 0.0 : saturate(1.0 - abs(dot(p - here, normal)) / slack);
    sum += g_Scene.SampleLevel(g_Linear, at, 2.0).rgb * w;
    weights += w;
  }
  return sum / weights;
}

// How much of a snowed top is round a pixel (2026-10-26, the user: "snow
// should be more smooth around the edge but still no bleed or flicker"): the
// share of 12 points round it (6 at 1 px, 6 at 2.5 px) not dropping away
// from its own plane (under it by less than 10 % of their distance from it),
// so the snow fades over the last 2 - 3 pixels inside an edge instead of
// stopping at one (3 - 4 until 2026-10-26 21:xx, the user: "Snow looks
// blurry"). It only takes snow away (none past the edge: no bleed),
// and as a share of many points it doesn't flip from frame to frame as one
// pixel's test does. (What rises from it, a wall's foot, a curb, something
// standing on it, doesn't count: the snow lies up against it, and the pile's
// side goes on up it, SnowPileSide. The 4 % either way of the 16:xx build
// faded the ground's snow before every wall, a bare strip between it and the
// pile against the wall; the user, "piled up snow looks weird". And 4 % was
// a bend of 2 degrees, which the ground seen at a glancing angle has.)
// buried: a dip smaller than this (m) doesn't count (2026-10-26 19:xx, the
// user: "Snow shader on gravel looks weird": the stones' dips counted, so on
// gravel the snow faded to grey but for round what stands on it; half as
// deep as the snow lies, which fills them, at least 3 cm).
// (2026-10-27, the user: "fix the snow's between-pixel sampling
// (RelativeAtTexel) and scale its pixel radii with the resolution": a point
// between pixels took its texel's depth on the ray through itself, off the
// surface by up to half a pixel's depth step, which looking along the
// ground thinned the snow's edges; and its radii were 4K's pixels at any
// resolution.)
// (The sky seen past a top is an edge too: it fades there.)
float SnowEdgeAt(float2 uv, float3 here, float3 normal, float buried)
{
  float on = 0.0;
  float scale = PixelScale();
  [unroll] for (int k = 0; k < 12; ++k)
  {
    float a = k * 0.5236;
    float2 step = float2(cos(a), sin(a)) * ((k & 1) ? 2.5 : 1.0) * scale;
    bool sky;
    float3 there = RelativeAtTexel(uv + step * g_Frame.xy, sky) - here;
    float drop = max(-dot(there, normal) - buried, 0.0);
    on += sky ? 0.0 : saturate(1.0 - (drop / max(length(there), 1e-3) - 0.1) * 8.0);
  }
  return smoothstep(0.35, 0.95, on / 12.0);
}

// How much of a snowed top is wide enough to hold the snow (2026-11-04,
// the user, a screenshot of a metal staircase: "Snow also looks weird on
// metal railings and thin metal objects fix it no mistakes": the
// handrail's top, the steps' front lips and the stringers' edges, a few
// centimetres across, had snow in jagged bits, some of it over onto their
// sides, where the depth can't tell their facing and the game's own was
// taken). The share of 8 points SnowWideMetres round it (at least 2, at
// most 48 of 4K's pixels, x PixelScale) on its plane or over it, as
// SnowEdgeAt counts them (a dip under buried doesn't count; nor the sky
// past an edge): none from 2.5 of them (a rail: the 2 along it), all from 5
// (a wide top's edge: 3 of them drop away). So nothing under about 7 cm
// across holds snow: rails, posts' tops, pipes, a step's lip; a top's
// corner fades over its last 5 cm.
static const float SnowWideMetres = 0.05;
float SnowWideAt(float2 uv, float3 here, float3 normal, float distance, float buried)
{
  float pixelAngle = 2.0 * g_Frame.x / g_ProjectionXY.x;
  float scale = PixelScale();
  float r = clamp(SnowWideMetres / max(distance * pixelAngle, 1e-5), 2.0 * scale, 48.0 * scale);
  float on = 0.0;
  [unroll] for (int k = 0; k < 8; ++k)
  {
    bool sky;
    float3 there = RelativeAtTexel(uv + RingDirections[k] * r * g_Frame.xy, sky) - here;
    float drop = max(-dot(there, normal) - buried, 0.0);
    on += sky ? 0.0 : saturate(1.0 - (drop / max(length(there), 1e-3) - 0.1) * 8.0);
  }
  return smoothstep(2.5, 5.0, on);
}

// How much a pixel is something the game drew over the ground (2026-10-26,
// the user: "Every object under snow looks grey", "object vfx / UI
// element"): its effects, the Runner's Vision's path and the HUD don't write
// the depth, so the depth says ground under them and the snow (and the ice)
// was laid over them. Told from the frame: much brighter than round it (a
// glow, a spark, the HUD's text), or a strong colour where round it isn't
// (the Runner's Vision's red, a marker): its colour against its luminance,
// against the same for the frame round it (around, 16 px blurred at 4K). A
// floor coloured all over stays as snowy as any (round it is the same colour).
// What's broad and grey (smoke, steam) can't be told from the ground.
float3 ChromaOf(float3 c)
{
  return (c - Luma(c)) / max(Luma(c), 0.05);
}

// How much a pixel glows (2026-10-26, the user: snow "shouldn't affect ...
// light panels"): its brightest channel near full (0.9 - 0.98 of the frame's
// white) and 2.5 - 3.5 times the frame's average: a light panel, a lit sign,
// a lamp. Snow isn't laid over it. Not measured from the game's panels (the
// frame at Present is after its tonemap). facingView: how square on the
// surface is seen (|its normal . the view|): at a glancing angle (under 0.2,
// fully from 0.45) it doesn't count, since what's that bright there is the
// sky's reflection, as strong as the angle is glancing; a panel glows from any
// angle. (The ice doesn't use it: water is never a light panel, and seen a
// certain way its reflection made the ice go (2026-10-26 20:xx, the user:
// "looking at a certain way makes the iced water shader go away").)
float GlowAt(float3 color, float3 average, float facingView)
{
  float brightest = max(color.r, max(color.g, color.b));
  return smoothstep(0.9, 0.98, brightest) * smoothstep(2.5, 3.5, brightest / max(Luma(average), 0.02)) *
    smoothstep(0.2, 0.45, facingView);
}

// (and more than twice as bright as round it, 1.8 - 2.4: 2026-10-27, the
// user: "Shadows bug out the snow effect", "Snow becomes grey on edge". The
// sunlit side of a shadow's edge is at most 2 / (1 + s) times the frame 16
// px round it, s the shadow's light against the sunlit ground's: 1.3 - 1.6
// in the game's shadows, under 1.9 unless they're nearly black. In strong
// sun it was 0.12 - 0.3 over 1.3 x its surroundings, so the snow was taken
// off along every shadow's edge and every lit edge with something darker
// beyond it, up to four fifths of it: a grey strip. A glow, a spark or the
// HUD's text is more than twice as bright as what's round it.)
float OverTheGround(float3 color, float3 around)
{
  float bright = smoothstep(0.12, 0.3, Luma(color) - Luma(around) * 1.3) *
    smoothstep(1.8, 2.4, Luma(color) / max(Luma(around), 0.02));
  float vivid = smoothstep(0.6, 1.2, length(ChromaOf(color) - ChromaOf(around))) * smoothstep(0.04, 0.12, Luma(color));
  return max(bright, vivid);
}

// How much of a pixel's standing out from the frame round it (OverTheGround,
// HazeKeptOff) is the surface's own colour, told by the game's G-buffer:
// paint on the road is in it (its base colour white or yellow where the
// asphalt's is dark), the light on it the same as round it; the HUD, the
// markers, the effects and the Runner's Vision are drawn after the game's
// lighting, over the ground, whose base colour is under them. (2026-11-03,
// the user, a screenshot from high over a street under snow: "cars on
// rain/snow have a box around them make it invisible fix it no mistakes":
// the parking bays' white lines round the parked cars, more than 2.4 times
// as bright as the frame round them, and the zebra crossing's stripes were
// taken for something drawn over the ground: no snow laid on them, grey
// lines in it, and the haze kept off them, sharp lines in it.)
// In linear light (the picture's values ^ 2.2; the base colour's copy is
// read as linear): the light on it, the picture over its base colour,
// against the light round it, around over the base colour's average at
// 16 points 8 and 24 pixels round (at 4K, PixelScale; where the G-buffer
// has a surface: its basis one of the 6), near the blur round it a handful
// of points can get. Within 1.4 times: its own; from 2 times: not. And its
// colour the base colour's (ChromaOf within 0.3, none from 0.6), so a red
// marker over grey asphalt isn't, a yellow line is. 0 where the G-buffer
// isn't there this frame or has no surface at the pixel, as before. px: the
// pixel's G-buffer texel (DepthTexel); the rings' 8 and 24 pixels in its
// texels (x g_Depth: the same at 100 %).
float OwnColourAt(int2 px, float3 color, float3 around)
{
  float3 n;
  float smoothness;
  if (!GameSurfaceAt(px, n, smoothness))
    return 0.0;
  int2 last = DepthLast();
  float3 base = g_GameColour.Load(int3(px, 0)).rgb;
  float pixel = PixelScale();
  float ring = 0.0, counted = 0.0;
  [unroll] for (int r = 0; r < 2; ++r)
  {
    [unroll] for (int k = 0; k < 8; ++k)
    {
      int2 at = clamp(px + int2(round(RingDirections[k] * (r == 0 ? 8.0 : 24.0) * pixel * g_Depth.xy)), 0, last);
      float4 there = g_GameColour.Load(int3(at, 0));
      float basis = there.a * 6.0;
      if (abs(basis - round(basis)) <= 0.1 && basis < 5.5)
      {
        ring += Luma(there.rgb);
        counted += 1.0;
      }
    }
  }
  if (counted < 4.0)
    return 0.0;
  float3 picture = pow(saturate(color), 2.2);
  float lightHere = Luma(picture) / max(Luma(base), 0.02);
  float lightRound = Luma(pow(saturate(around), 2.2)) / max(ring / counted, 0.02);
  float light = 1.0 - smoothstep(1.4, 2.0, lightHere / max(lightRound, 1e-4));
  float hue = 1.0 - smoothstep(0.3, 0.6, length(ChromaOf(picture) - ChromaOf(base)));
  return light * hue;
}

// What of a pixel the haze leaves alone as the game's HUD, its markers and
// effects over the ground (PsMain's haze; OverTheGround is the snow's and
// the ice's): the frame there read 2 x 2 pixels averaged (its copy's level
// 1), so a glint a pixel across can't count, and only plainly brighter than
// round it (0.2 - 0.4 over 1.3 x its luminance; OverTheGround 0.12 - 0.3) or
// plainly coloured where round it isn't. (2026-10-27, the user: "There is
// still some building flickering from haze/fog when looking them at a 45/50
// degree angle": seen at that angle a building's windows mirror the sky,
// brighter than its walls, and from frame to frame (the game's jitter, its
// glints) they went over OverTheGround's line and back, the haze on them
// going from all of it to 15 % and back.)
// The frame's light round a point of the ground (here, normal; distance: how
// far it is), from what of it round it lies on its plane: 16 points 8 and 24
// pixels round (at 4K, PixelScale; on their texels' centres) within 5 cm + 1
// % of the distance of the plane. What stands on it or is in front of it
// (Faith's legs and hands, a person) doesn't count. Its luminance; count: how
// many of the points counted.
float GroundLumaAround(float2 uv, float3 here, float3 normal, float distance, out float count)
{
  float pixel = PixelScale();
  float slack = 0.05 + 0.01 * distance;
  float sum = 0.0;
  count = 0.0;
  [unroll] for (int ring = 0; ring < 2; ++ring)
  {
    float2 step = (ring == 0 ? 8.0 : 24.0) * pixel * g_Frame.xy;
    [unroll] for (int k = 0; k < 8; ++k)
    {
      float2 at = uv + RingDirections[k] * step;
      bool sky;
      float3 p = RelativeAtTexel(at, sky);
      if (!sky && abs(dot(p - here, normal)) < slack)
      {
        sum += Luma(g_Scene.SampleLevel(g_Linear, at, 0.0).rgb);
        count += 1.0;
      }
    }
  }
  return count > 0.0 ? sum / count : 0.0;
}

float HazeKeptOff(float2 uv)
{
  float3 c = g_Scene.SampleLevel(g_Linear, uv, 1.0).rgb;
  float3 around = SceneAround(uv, 2.0);
  float bright = smoothstep(0.2, 0.4, Luma(c) - Luma(around) * 1.3);
  float vivid = smoothstep(0.9, 1.5, length(ChromaOf(c) - ChromaOf(around))) * smoothstep(0.08, 0.2, Luma(c));
  float kept = max(bright, vivid);
  // (not the road's paint, the surface's own colour: OwnColourAt, at the
  // pixel's G-buffer texel, DepthTexel's without its clamp: uv is a pixel's
  // centre, on the frame; with it the rain's copy of the shader took one more
  // register)
  [branch] if (kept > 0.002)
    kept *= 1.0 - OwnColourAt(int2(uv / g_Frame.xy * g_Depth.xy), c, around);
  return kept;
}

// The ice over the city's water while it snows (2026-10-26, the user:
// "frozen water should have a froze shader like in real life", "make it
// find any instance of water", then "There is still some water flickering"
// and, with a screenshot of an indoor fountain, "these waterfalls are still
// running and the frozen water has some weird alpha issues"). The water is
// the game's models moved by their own shaders; the tools draw ice over them
// as their own pass (IceVsMain / IcePsMain, after PsMain): every piece of
// the city's water (IceTable.h: its rectangles and its triangles, the
// fountains' curved falls too) drawn as it is, every frame. Before, PsMain
// met each pixel's ray with the nearest 96 triangles and 48 rectangles, so
// as the camera moved pieces came and went (the flicker), and the fountains'
// falls were boxes, so the walls inside them were iced (the screenshot's
// blotches) with their icicles upside down. Now:
// - a piece's pixel is ice where it's in front of what the frame shows
//   there, or on it (5 cm, 0.2 % of the distance and the piece's own
//   tolerance allowed: the stream lies on its wall, the water on its pool or
//   floor);
// - not inside one of the game's interiors (InteriorAt): water indoors
//   stays water, as the waterfalls there keep running (UpdateFrozenWater
//   leaves them, by the same interiors). Under a bridge or an awning it
//   freezes as the falls over it do.
// The look (IceLook):
// - A stream, a wall or a fall: opaque ice hanging in ridges down it (a few
//   centimetres across, longer streaks in them), frost grains in it, its
//   sides ragged (a stream's water narrows to its edges; a wall's only at its
//   ends), icicles of uneven length at its foot (a rectangle's lower side, a
//   fall's lowest point). Pale blue where it's thick, white along the
//   ridges, the ridges catching the light.
// - A pool: a sheet of opaque ice, frosted white over most of it, clear and
//   dark blue-grey in big patches, fine frost grains, white cracks across it
//   (fading out where a pixel covers more than they'd show), the sky's light
//   glancing off it, a light dusting of snow in patches once snow lies.
// - A pool lit by the sky's light (see IceLook), a stream, a wall or a fall
//   by the light the frame shows round it (as the snow); into the fog as
//   the frame goes; growing in as the ice forms (g_IceCount.y).
struct IceVertex
{
  float4 position : SV_POSITION;
  float3 rel : TEXCOORD0;                     // relative to the camera
  float4 local : TEXCOORD1;                   // x, y: metres across and up a rectangle; z: its kind; w: a fall's foot (y)
  nointerpolation float4 shape : TEXCOORD2;   // x, y: a rectangle's half sides; z: the tolerance
  float3 relGame : TEXCOORD3;                 // relative to the game's own camera of the frame (see IceVsMain)
};

// Drawn with the game's own camera of the frame where its viewConstants'
// copy is there (g_GameFrame.x: copied with its G-buffer this frame): the
// place less cameraPos through crViewProjMatrix, as the game's own vertex
// shaders draw what's in its depth (DogenGlow.hlsl). The ice is the only
// thing drawn here from the world's own shapes and tested against the
// game's depth (IcePsMain); drawn with the camera the camera hook last saw
// (g_ViewProjection), a camera a little off the one the frame was drawn
// with put the frame's surface a little off the water, more and more
// towards one side of the frame, and past a straight line on it more than
// IceOnWater: no ice there (2026-11-04, the user, a screenshot of a pool's
// ice cut along a straight line, which moved with the screen: "first
// person has still the water issue with snow high priority no
// mistakes"; how far off that camera is hasn't been measured). Without
// the copy: the camera hook's, as before.
IceVertex IceVsMain(float4 at : POSITION, float4 local : TEXCOORD0, float4 shape : TEXCOORD1)
{
  IceVertex o;
  o.rel = at.xyz - g_Camera.xyz;
  o.relGame = o.rel;
  o.position = mul(float4(o.rel, 1.0), g_ViewProjection);
  [branch] if (g_GameFrame.x > 0.5)
  {
    o.relGame = at.xyz - g_GameCameraPos.xyz;
    float4 p = float4(o.relGame, 1.0);
    o.position = float4(dot(p, g_GameViewFirst[14]), dot(p, g_GameViewFirst[15]), dot(p, g_GameViewFirst[16]),
      dot(p, g_GameViewFirst[17]));
  }
  o.local = local;
  o.shape = float4(shape.xy, at.w, 0.0);
  return o;
}

// kind: 0 a stream, 1 a wall, 2 a pool, 4 a fall (a triangle that isn't
// level); x, y: metres across and up it (a pool: along its sides); halfA,
// halfB: a rectangle's half sides (1e7 none); foot: a fall's lowest y
float4 IceLook(float kind, float x, float y, float halfA, float halfB, float foot, float distance, float3 dir,
  float3 facingDir, float2 uv)
{
  float pixelAngle = 2.0 * g_Frame.x / g_ProjectionXY.x;
  // Its light (lit), what its ridges and the sky glancing off it take
  // (shine), the snow dusting a pool (dusting).
  // A pool's, once the sky's colour is known (g_HazeView.yzw, held steady on
  // the CPU): the sky's light, as the snow's where its own can't be told
  // (SnowLitByGame's: the sky by the horizon x 0.9, x Snow brightness),
  // the snow on it that snow's colour. Not the frame there: that's the
  // game's water, the frame mirrored in it (2026-11-03, the user: "When i
  // have snow enabled water shader breaks at the edge of the screen", with
  // a screenshot of a pool's ice white on one side of a sharp straight line
  // and flat grey on the other; the line moved with the screen, on all the
  // water). The game mirrors what the frame shows; towards the frame's edge,
  // where what it would mirror is off the frame, it gives a grey instead,
  // and the ice lit by its picture (x 1.6, at least 0.8 of the sky's light)
  // came out white where the mirror reached and grey past it, with the
  // mirror's edge as a line between, and dark where something dark was
  // mirrored.
  // A stream, a wall or a fall (and a pool until the sky's been seen): lit
  // as the snow was, by the frame round it.
  float lit, shine;
  float3 dusting;
  bool pool = kind > 1.5 && kind < 3.5;
  [branch] if (pool && g_HazeView.y >= 0.0)
  {
    float skyLight = min(dot(g_HazeView.yzw, float3(0.2126, 0.7152, 0.0722)) * 0.9, 1.0);
    float bright = pow(g_GameFrame.y > 0.0 ? g_GameFrame.y : 1.0, 1.0 / 2.2);
    lit = min(skyLight * bright, 1.0);
    shine = lit;
    dusting = RollOffWhite(float3(0.89, 0.93, 1.0) * skyLight * bright);
  }
  else
  {
    float3 color = g_Scene.SampleLevel(g_Linear, uv, 0.0).rgb;
    // (lit as the snow is: its own surface 6 px round, SnowAroundAt, and the
    // light of its surroundings, not the whole frame's: a 16-px blur of
    // everything made it fuzzy, and the frame's average went up as the camera
    // looked at the sky; 2026-10-27, the user: "snow/ice looks is foggy and
    // blurry / fuzzy also looking up make the shader grey")
    float3 around = SnowAroundAt(uv, dir * distance, facingDir, distance);
    float3 average = SceneAround(uv, 0.0);
    lit = min(max(max(Luma(color) * 1.6, Luma(around)), Luma(average) * 0.75), 1.0);
    // (no darker than 0.8 of the sky's light by the horizon once it's been seen,
    // as the snow round it: frosted ice is nearly as white; SnowLit)
    if (g_HazeView.y >= 0.0)
      lit = min(max(lit, dot(g_HazeView.yzw, float3(0.2126, 0.7152, 0.0722)) * 0.8), 1.0);
    shine = Luma(average);
    dusting = SnowLit(around, average);
  }
  float growth = saturate(g_IceCount.y * 1.4 - Noise(float2(x / 0.4, y / 0.8) + 3.0) * 0.4);
  float3 ice;
  float cover;
  if (kind < 1.5 || kind > 3.5)
  {
    // A stream, a wall or a fall
    float footprint = distance * pixelAngle / max(abs(dot(dir, facingDir)), 0.2);
    float fine = saturate(1.0 - footprint / 0.03);
    float ridge = Noise(float2(x / 0.07, y / 2.5)) * 0.55 + Noise(float2(x / 0.025 + 13.0, y / 0.9)) * 0.3 * fine +
      Noise(float2(x / 0.25 + 5.0, y / 6.0)) * 0.15 + 0.15 * (1.0 - fine);
    float grain = Noise(float2(x / 0.012, y / 0.05) + 61.0) * fine + 0.5 * (1.0 - fine);
    float side = kind > 3.5 ? 1.0 : kind > 0.5 ? 1.0 - smoothstep(halfA - 0.15, halfA, abs(x)) :
      1.0 - smoothstep(0.55, 1.0, abs(x) / max(halfA, 1e-3) + (Noise(float2(y / 0.3, 3.0)) - 0.5) * 0.3);
    // Icicles at its foot: in 5 cm columns, each ending a random 0 - 35 cm
    // above it and tapering to its tip over 30 cm
    float fromFoot = kind > 3.5 ? y - foot : y + halfB;
    float cell = x / 0.05;
    float acrossCell = abs(frac(cell) - 0.5) * 2.0;
    float tip = Hash(float2(floor(cell), 7.0)) * 0.35;
    float width = saturate((fromFoot - tip) / 0.3) * 1.25;
    float atFoot = 1.0 - smoothstep(width - 0.15, width, acrossCell);
    float top = kind > 3.5 ? 1.0 : smoothstep(halfB, halfB - 0.05, y);
    float3 deep = float3(0.62, 0.78, 0.90), white = float3(0.93, 0.97, 1.0);
    ice = lerp(deep, white, saturate(ridge * 1.3 - 0.05)) * lit * 1.15;
    ice *= 0.92 + 0.12 * grain;
    ice += pow(saturate(ridge * 1.15 - 0.15), 5.0) * 0.8 * shine * float3(0.95, 0.98, 1.0);
    cover = side * atFoot * top * 0.97;
  }
  else
  {
    // A pool
    float2 p = float2(x, y);
    float footprint = distance * pixelAngle / max(abs(dir.y), 0.2);
    float fine = saturate(1.0 - footprint / 0.1);
    float finer = saturate(1.0 - footprint / 0.025);
    float frost = Noise(p / 1.1) * 0.45 + Noise(p / 0.31 + 9.0) * 0.3 * fine + Noise(p / 0.09 + 21.0) * 0.15 * fine +
      Noise(p / 0.025 + 33.0) * 0.1 * finer + 0.275 * (1.0 - fine);
    float clearNoise = Noise(p / 7.0 + 5.0) * 0.7 + Noise(p / 2.5 + 17.0) * 0.3;
    // (its clear patches gone far off, 80 - 250 m: a frozen pool seen from
    // high up was a dark mottle where the snow round it was even; the user,
    // 2026-10-27: "when i'm really far you can see the frozen water shader")
    float clearIce = smoothstep(0.34, 0.2, clearNoise) * (1.0 - smoothstep(80.0, 250.0, distance));
    float3 white = float3(0.84, 0.90, 0.96) * lit;
    float3 clear = float3(0.36, 0.48, 0.58) * lit;
    ice = lerp(white, clear, clearIce * 0.85);
    ice *= 0.95 + 0.1 * frost;
    ice *= 0.96 + 0.08 * (Noise(p / 0.02 + 51.0) * finer + 0.5 * (1.0 - finer));
    float lines = saturate(1.0 - footprint / 0.05);
    float crack = (1.0 - smoothstep(0.0, 0.012, abs(Noise(p / 2.3 + 41.0) - 0.5))) * lines;
    crack = max(crack, (1.0 - smoothstep(0.0, 0.01, abs(Noise(p / 0.8 + 77.0) - 0.5))) * 0.7 * lines);
    ice = lerp(ice, float3(0.95, 0.98, 1.0) * lit * 1.1, crack * 0.75);
    float glance = 0.04 + 0.96 * pow(1.0 - saturate(abs(dir.y)), 5.0);
    ice += shine * float3(0.88, 0.93, 1.0) * glance * 0.6 * (0.4 + 0.6 * clearIce);
    // Snow on it once snow lies: a light dusting in patches at first (the
    // wind clears smooth ice), then as it piles (g_SnowPile.x, 1 - 8 cm)
    // nearly all of it under the snow but for a few patches the wind keeps
    // clear (2026-10-27: a pool stood out of the snow round it as dark ice)
    float buriedIce = smoothstep(0.01, 0.08, g_SnowPile.x);
    float dust = smoothstep(lerp(0.55, -0.1, buriedIce), lerp(0.8, 0.15, buriedIce),
      GNoise(p / 1.6 + 3.0) * 0.7 + Noise(p / 0.3 + 9.0) * 0.3) * saturate(g_SnowDrift.z * 1.5);
    ice = lerp(ice, dusting,
      dust * lerp(0.7, 0.95, buriedIce));
    float edge = min(halfA - abs(x), halfB - abs(y));
    cover = 0.98 * smoothstep(0.0, max(0.03, footprint * 1.5), edge);
  }
  return float4(saturate(ice), cover * growth);
}

// How far the frame's surface may stand off a piece of the ice (towards the
// camera, along the piece's facing; with the piece's own tolerance, and 0.1 %
// of the distance) and still be its water, the ice fading out over
// IceOffFade past that (see IcePsMain)
static const float IceOnWater = 0.15, IceOffFade = 0.1;

float4 IcePsMain(IceVertex v) : SV_TARGET
{
  float2 uv = v.position.xy * g_Frame.xy;
  float distance = length(v.rel);
  float3 dir = v.rel / max(distance, 1e-4);
  // Its facing (towards the camera), from the triangle itself
  float3 facing = cross(ddy(v.rel), ddx(v.rel));
  facing = length(facing) > 1e-12 ? normalize(facing) : float3(0.0, 1.0, 0.0);
  if (dot(facing, -v.rel) < 0.0)
    facing = -facing;
  // In front of what the frame shows there, or on it: the frame's surface
  // there no further in front of the piece, along its facing, than
  // IceOnWater (2026-10-26, the user: "snow water flickers", "it's z
  // fighting close up or at distance": it was measured along the view, at
  // most 5 cm + 0.2 % of the distance. Seen at a slant, a surface h in front
  // of the piece is h / cos(the angle) along the view, so far off, and close
  // where the game's water stands a few cm off the piece (a rectangle of the
  // table is its mesh's middle), it came and went with each frame's jitter.
  // Along the facing it's how far the surface stands off the piece, however
  // it's seen. What it lets in: a low edge (under IceOnWater) in front of the
  // water, seen at a slant, takes the ice where it hides the water.)
  float depth = DepthAt(uv);
  float onWater = 1.0;
  if (!IsSky(depth))
  {
    // (drawn with the game's own camera, see IceVsMain: the frame's surface
    // on the same ray from it as the ice's point, as far along it as its
    // depth against the ice's own, both through the projection's depth
    // terms as RelativeAt takes them; without it, RelativeAt's point)
    float off;
    if (g_GameFrame.x > 0.5)
    {
      float frameZ = (g_ProjectionZW.z - depth * g_ProjectionZW.w) / (depth * g_ProjectionZW.y - g_ProjectionZW.x);
      float iceZ = (g_ProjectionZW.z - v.position.z * g_ProjectionZW.w) / (v.position.z * g_ProjectionZW.y - g_ProjectionZW.x);
      off = dot(v.relGame, facing) * (frameZ / (abs(iceZ) > 1e-6 ? iceZ : 1e-6) - 1.0);
    }
    else
      off = dot(RelativeAt(uv, depth) - v.rel, facing);
    float allowed = IceOnWater + v.shape.z + 0.001 * distance;
    onWater = 1.0 - smoothstep(allowed, allowed + IceOffFade, off);
    if (onWater <= 0.0)
      discard;
  }
  // Not inside one of the game's interiors (InteriorAt, all of them, the
  // ones UpdateFrozenWater leaves the falls running in), whatever is over
  // it. It was under the open sky only (ShelterAt): the weather's squares
  // took the ice off under anything over it (a bridge, the monorail's
  // track, a sign high up), each square 2 m, looked at again every 2 - 5 s,
  // and none past 50 m, so as the camera moved a pool's ice came and went in
  // straight-edged patches while the falls over it stood frozen (2026-11-02,
  // the user, a screenshot of a plaza's pool partly ice, partly water: "You can
  // still see the snow interacting with the water z fighting on water fix
  // it")
  float3 world = v.rel + g_Camera.xyz;
  float open = 1.0 - smoothstep(0.25, 0.75, InteriorAt(v.rel, 0.0));
  if (open < 0.01)
    discard;
  float kind = v.local.z, x = v.local.x, y = v.local.y, halfA = v.shape.x, halfB = v.shape.y;
  if (kind > 2.5)
  {
    // One of the water's own triangles: a pool where it's level, else a fall
    if (abs(facing.y) > 0.6)
    {
      kind = 2.0;
      x = world.x;
      y = world.z;
    }
    else
    {
      kind = 4.0;
      float2 along = normalize(float2(-facing.z, facing.x) + 1e-6);
      x = dot(world.xz, along);
      y = world.y;
    }
    halfA = halfB = 1e7;
  }
  float4 ice = IceLook(kind, x, y, halfA, halfB, v.local.w, distance, dir, facing, uv);
  float3 sceneHere = g_Scene.SampleLevel(g_Linear, uv, 0.0).rgb;
  float3 average = g_Scene.SampleLevel(g_Linear, float2(0.5, 0.5), g_Frame.w).rgb;
  // (not GlowAt: water is never a light panel, and its reflection of the sky
  // seen at a glancing angle counted as one, so the ice went)
  // (as far as the snow on the ground: SnowReach)
  ice.a *= onWater * open * FogAt(v.rel) * SnowReach(distance) * (1.0 - OverTheGround(sceneHere, SceneAround(uv, 2.0)));
  // Hazed as the frame behind it is (PsMain's haze, HazeAt: as much, the same
  // colour). It's drawn after that pass, so in a blizzard's haze the ice
  // stood out sharp where everything round it had gone white (the user,
  // 2026-10-26, with a screenshot: "frozen water is still visible on fog and
  // doesn't blend in with it").
  if (g_Haze.x > 0.0)
    ice.rgb = lerp(ice.rgb, HazeTint(average), HazeAt(dir, distance) * max(open, g_HazeView.x));
  if (ice.a < 0.002)
    discard;
  return ice;
}

// The piled snow's side up the foot of walls, boxes and curbs (SnowPileSide,
// PileLight): not drawn (2026-11-04, the user, with a screenshot of a gravel
// roof's parapet and a skylight's frame, the pile along their foot in light
// and dark bands crossed by fine lines: "Find out why snow still does this
// on the walls", then "Generally there shouldn't be any snow on any
// wall"). The snow lies on what faces up only, up to the wall's foot.
static const bool SnowOnWalls = false;

float4 PsMain(float4 position : SV_POSITION) : SV_TARGET
{
  float2 uv = position.xy * g_Frame.xy;
  float4 scene = g_Scene.Load(int3(position.xy, 0));
  if (Skipped(SkipAll))
    discard;
  float depth = DepthAt(uv);
  float flash = g_Flash.x;
  bool sky = IsSky(depth);
  float3 color = scene.rgb;
  bool changed = false;
  float3 dir;
  float distance;
  float hazeOutside = 1.0;   // the haze only over what's out in the open (see HazeAt)
  float3 sceneNormal = float3(0.0, 1.0, 0.0);   // the frame's surface's facing (see FallingHidden)

  if (sky)
  {
    dir = normalize(RelativeAtViewZ(uv, -1.0));
    distance = 1e9;
    // The sky flashes most
    if (flash > 0.0 && !Skipped(SkipLightning))
    {
      color = color * (1.0 + 4.0 * flash) + g_Flash.yzw * flash * 0.25;
      changed = true;
    }
  }
  else
  {
    // (on its depth texel's centre, as the points round it are taken,
    // RelativeAtTexel, and its facing's neighbours, FrameNormal and
    // SnowNormal from that centre, DepthTexelUv: the pixel's own centre at
    // 100 %; at another resolution scale a pixel's centre is off its
    // texel's, and the depth put on the pixel's ray was off the surface by up
    // to half a texel's step. The G-buffer is read at the same texel,
    // DepthTexel(uv).)
    float2 hereUv = DepthTexelUv(uv);
    float3 here = RelativeAt(hereUv, depth);
    distance = length(here);
    dir = here / max(distance, 1e-3);
    float3 view = -dir;
    float3 world = here + g_Camera.xyz;

    // Which way it faces (see FrameNormal)
    float smooth = 1.0;
    float3 normal = float3(0.0, 1.0, 0.0);
    [branch] if (!Skipped(SkipFacing))
      normal = FrameNormal(hereUv, here, distance, smooth);
    float ground = smoothstep(0.55, 0.85, normal.y) * smooth;
    sceneNormal = normal;
    // How the wet look shades it (how much of the ground's darkening, its
    // mirror's way and share, the lightning's): the game's own facing (its
    // G-buffer, GameSurfaceAt), smooth over a model's triangles. The depth's
    // is flat over each of them, so each was darkened and mirrored on its own
    // (2026-11-01, the user: "Rain shader doesn't apply smoothly (you can see
    // the triangles of the mesh) on some meshes"). With none this frame, the
    // depth's as before. Where puddles, slopes and prints go is still told by
    // the depth's (ground, normal).
    float3 shadeNormal = normal;
    float shadeGround = ground;
    {
      float3 gameNormal;
      float gameSmooth;
      // (only where the depth's facing leans, where its triangles showed: on
      // level ground the depth's is right, and the game's has its decals' and
      // textures' bumps in it, so a decal showed as a patch lighter or darker
      // than the floor round it, overlapping rectangles: 2026-11-01, the
      // user's screenshot, "some meshes have this weird effect with wetness
      // smooth it out and fix it")
      float lean = 1.0 - smoothstep(0.93, 0.97, normal.y);
      // (read only where it leans: HLSL's && works out both sides, so it was
      // read at every pixel)
      // (only for the wet ground's shading: not read when that's in the
      // game's G-buffer, g_Options.y)
      [branch] if (lean > 0.0 && !Skipped(SkipFacing) && g_Options.y < 0.5)
      {
        if (GameSurfaceAt(DepthTexel(uv), gameNormal, gameSmooth) && dot(gameNormal, view) > 0.0)
        {
          shadeNormal = normalize(lerp(normal, gameNormal, lean));
          shadeGround = smoothstep(0.55, 0.85, shadeNormal.y) * lerp(smooth, 1.0, lean);
        }
      }
    }

    // Dry where something is over it (the weather's squares)
    // (the game's interiors here worked out once, for all of them; or read
    // from the G-buffer pass's info when it drew this frame, g_Options.z:
    // its own working out of them, the same sums, see WetInfoAt)
    float interiorHere = 0.0, shelter = 0.0;
    bool infoHere = g_Options.z > 0.5;
    float4 info = 0.0;
    [branch] if (infoHere)
    {
      info = WetInfoAt(uv);
      interiorHere = info.w;
    }
    else
    {
      [branch] if (!Skipped(SkipShelter))
      {
        interiorHere = InteriorHere(world);
        shelter = ShelterWith(world, interiorHere);
      }
    }
    float outside = infoHere ? info.x : 1.0 - smoothstep(0.25, 0.75, shelter);
    // (the haze is the air on the way to the point: with the camera out in
    // the open all it sees is hazed, a building inside one of the game's
    // interiors too, whose walls InteriorAt's margins count as inside: the
    // Anansi tower's 491 m volume left it crisp in the haze, the user,
    // 2026-10-26, "snow haze/fog looks weird"; with the camera inside, only
    // what's out in the open)
    hazeOutside = max(outside, g_HazeView.x);

    // (into the game's fog as the ground itself goes: what the fog covers of
    // it the wet look doesn't darken or reflect)
    float wetFade = WetDrawn && g_Rain.z > 0.5 ? saturate(1.0 - distance / g_Rain.y) * FogAt(here) : 0.0;
    float wet = Skipped(SkipWet) ? 0.0 : g_Wet.x * wetFade * outside;
    // Not on Faith, people, vehicles or trains (see KeptDryAt: their outlines
    // as soft as the frame's); Faith and the people out in the rain a little
    // wet themselves instead (see BodyWetness, below)
    float bodyWet = 0.0, keptHere = 0.0;
    if (wet > 0.002)
    {
      float2 kept = 0.0;
      [branch] if (infoHere)
        kept = KeptDryInfoAt(uv, distance, info.yz);
      else
      {
        [branch] if (!Skipped(SkipKept))
          kept = KeptDryAt(uv, distance, here);
      }
      keptHere = kept.x;
      bodyWet = wet * kept.y * BodyWetness;
      wet *= 1.0 - kept.x;
    }
    // (on what's kept, Faith's and the people's outlines, the depth's facing
    // as before: the game draws a second G-buffer pass in a frame, what it is
    // isn't known, and if her body is in it the copy has what's under her;
    // the depth's keeps her outline out of the mirror)
    if (keptHere > 0.0)
    {
      shadeNormal = normal;
      shadeGround = ground;
    }
    // How wet the surface is here, before her prints and the marks: they
    // show only where they're wetter than it (2026-11-03, the user: "remove
    // the footsteps/handprints from wet surfaces it should only be on dry")
    float surfaceWet = wet;
    // Faith's footprints (see PrintsAt), under a roof too: her wet shoes'
    // as wet as the rest of the wet look where they are, so only where the
    // ground is drier. (Her prints pressed into the wet ground, lighter where
    // the water was pressed out, aren't made since 2026-11-03.)
    float4 prints = 0.0;
    [branch] if (WetDrawn && g_Rain.z > 0.5 && g_StepCounts.y > 0.5 && !Skipped(SkipMarks))
      prints = PrintsAt(here);
    if (ground > 0.3)
      wet = max(wet, prints.x * ground * g_Wet.x);
    // The ground's marks here (MarksAt, its one sample a pixel): the wet tyre
    // tracks of the cars come out of a puddle, as wet as the water they
    // carried, the puddles' (g_Wet.y: they dry slower than the ground, so
    // after the rain they show on a drier road; in a downpour no wetter than
    // it), drying over groundmarks::WetTrackSeconds, faded as the wet ground
    // is from the camera and into the fog, under a roof too as her prints
    // are; darker only where the ground is less wet than they are
    // (2026-11-01, the user said "Yes, add them" to wet tyre tracks after a
    // puddle; the weather session's question and review). Not on what's kept
    // dry. Its stirred water (y) below, with the slide's wake.
    float2 marksHere = 0.0;
    if (WetDrawn && (g_MarksOn.y > 0.5 || g_MarksOn.z > 0.5) && !Skipped(SkipMarks))
      marksHere = MarksAt(world);
    if (g_MarksOn.z > 0.5 && ground > 0.3)
      wet = max(wet, marksHere.x * ground * max(g_Wet.x, g_Wet.y) * wetFade * (1.0 - keptHere));
    // (with the wet ground written into the game's own G-buffer this frame,
    // g_Options.y, GbufferWet.hlsl, the game's own lights, cube maps and
    // screen-space reflections show it: none of it drawn again here)
    if (wet > 0.002 && !Skipped(SkipWet) && g_Options.y < 0.5)
    {
      changed = true;
      float wall = (1.0 - shadeGround) * 0.3;
      // (not over the outline of what's in front, where the pixel shows it:
      // see NearerAround; its darkening at a little under half, its mirror
      // and sheen none)
      float foreground = 0.0;
      [branch] if (!Skipped(SkipEdgeTests))
        foreground = NearerAround(uv, here, normal, distance, scene.rgb);
      float n = Noise(world.xz / g_Wet.z) * 0.7 + Noise(world.xz / (g_Wet.z * 0.37) + 17.0) * 0.3;
      // A slope water runs down (see SlopeAt), near enough to tell: the
      // running water on it, no puddles (none lies on a slope; and the water
      // used to be taken out where puddles were, so with Puddles at 1 it went:
      // the user, 2026-10-26, "when you have puddles at 1 they disappear")
      float pixelsPerMetre = g_ProjectionXY.y * 0.5 / (max(distance, 0.1) * g_Frame.y);
      float3 slopeNormal = normal;
      // (looked at only then: HLSL's ?: works out both sides, so it was at
      // every wet pixel)
      float onSlope = 0.0;
      [branch] if ((g_DripCount.y > 0.5 || g_Wet.y > 0.001) && !Skipped(SkipEdgeTests))
        onSlope = SlopeAt(uv, here, pixelsPerMetre, slopeNormal);
      // (and only on solid, broad slopes: not on a railing, metal with holes,
      // steps or a narrow top, as the puddles, see PuddleSurface; 2026-10-27,
      // the user: "Waterslides effect shouldn't be possible on metal
      // railings/metal holes")
      if (onSlope > 0.001 && !Skipped(SkipPuddleTests))
        onSlope *= PuddleSurface(here, slopeNormal, distance);
      // Puddles only on what's level (none past 18 degrees, all under 11.5),
      // however far: SlopeAt can tell a slope only near (its rivulets' 3
      // pixels), so past about 18 m slopes up to 32 degrees had puddles that
      // went as the camera came near (2026-10-26, the user: "make sure that
      // puddles and waterdown slopes apply properly on every mesh")
      float level = smoothstep(0.95, 0.98, normal.y);
      // (its shore over 0.14 of the noise, 0.07 until 2026-10-27, the user:
      // "the shader should be more smoothed out around the edges"; WetLook.cpp's
      // PuddleAt the same)
      float puddle = g_Wet.y > 0.001 ? smoothstep(1.0 - g_Wet.y, 1.0 - g_Wet.y + 0.14, n) * ground * level * (1.0 - onSlope) : 0.0;
      // Only where it's known to be under the open sky (see KnownOpenAt). Past
      // the squares (50 m out), where it can't be told, as open as the wet
      // look has it there (its outside: the game's interiors kept dry), as the
      // snow does; it had none there, a ring round the camera where they
      // stopped. The squares not known at all: none, as before.
      if (puddle > 0.001)
      {
        float2 inGrid = (world.xz - g_ShelterGrid.xy) / g_ShelterGrid.z;
        bool pastSquares = g_ShelterGrid.w >= 1.0 && (any(inGrid < 0.0) || any(inGrid >= g_ShelterGrid.w));
        // (the squares read only inside them: HLSL's ?: works out both sides)
        [branch] if (!pastSquares)
          puddle *= saturate((KnownOpenWith(world, interiorHere) - 0.5) * 2.0);
      }
      // Puddles only where it's flat 4 pixels round (see FlatAround), and not
      // on anything with holes, on stairs or narrow tops (see PuddleSurface)
      if (puddle > 0.001 && !Skipped(SkipPuddleTests))
        puddle *= FlatAround(uv, here, normal);
      // (by the ground's nodes where they're this frame's, g_Options.w: see
      // PuddleGround, NodesPs)
      if (puddle > 0.001 && !Skipped(SkipPuddleTests))
      {
        [branch] if (g_Options.w > 0.5)
          puddle *= PuddleGround(here, normal, distance, world);
        else
          puddle *= PuddleSurface(here, normal, distance);
      }
      // Faith's slide through the water (see SlideWake): its path pushed
      // shallower (less of a mirror; the wet ground's film wiped, lighter),
      // its waves added below
      float3 wake = 0.0;
      [branch] if (g_SlideCount.x > 1.5 && !Skipped(SkipRings))
        wake = SlideWake(here, distance);
      // (its waves only on what faces up: the ground, the water down a
      // slope; not on walls or the sides of what stands there)
      wake.yz *= smoothstep(0.6, 0.8, normal.y);
      // and the KrugerSec down in it: their waves the same way, the water
      // pushed under them and where their bodies pressed it (see
      // BodyRipplesAt, BodyPressedAt)
      if (g_BodyRippleCount.x > 0.5 && !Skipped(SkipRings))
      {
        float3 bodies = BodyRipplesAt(here, distance);
        wake.x = max(wake.x, bodies.x);
        wake.yz += bodies.yz * smoothstep(0.6, 0.8, normal.y);
      }
      if (g_SnowStampCount.z > 1.5 && g_SnowStampCount.x > 0.5 && !Skipped(SkipRings))
        wake.x = max(wake.x, BodyPressedAt(here));
      // and the crowd's steps and the traffic's wheels through it (see
      // MarksAt; 2026-11-01, the user: "cars/motorbikes should leave behind
      // water trails if on puddles"): the water pushed aside where they went,
      // coming back over groundmarks::WaterMarkSeconds; their rings are
      // StepRings' as before. Only on what faces up.
      if (g_MarksOn.y > 0.5 && (puddle > 0.001 || wet > 0.001) && !Skipped(SkipRings))
        wake.x = max(wake.x, marksHere.y * smoothstep(0.6, 0.8, normal.y));
      if (wake.x > 0.001)
      {
        puddle *= 1.0 - 0.6 * wake.x;
        wet *= 1.0 - 0.35 * wake.x;
      }

      // Darker where wet
      float darken = lerp(1.0, 0.55, wet * saturate(shadeGround + wall) * (1.0 - 0.6 * foreground));
      color *= darken;
      // A puddle on its own, whatever the reflections (the user, 2026-10-25
      // 22:5x: "puddles should be visible no matter the reflection level"):
      // the water smooths the ground's detail away (the frame a few pixels
      // blurred) and darkens it more; its sheen is added below
      float wetPuddle = puddle * wet;
      if (wetPuddle > 0.001)
      {
        // (not where something stands in it or is in front of it within the
        // blur's reach, RisesAround: its colours bled into the water round it,
        // a soft halo round Faith's legs)
        float blurLevel = max(0.0, g_Frame.z - 4.0);
        float3 smoothed = g_Scene.SampleLevel(g_Linear, uv, blurLevel).rgb * darken;
        float rises = 0.0;
        [branch] if (!Skipped(SkipEdgeTests))
          rises = RisesAround(uv, here, normal, distance, exp2(blurLevel) * 1.5);
        float blurred = 0.7 * wetPuddle * (1.0 - rises);
        // (x 0.67 darker, 0.7 until 2026-11-08: the user, "can we make the
        // puddles from rain a little more visible? tiny bit like 10%"; as
        // GbufferWet.hlsl's)
        color = lerp(color, smoothed, blurred) * lerp(1.0, 0.67, wetPuddle);
      }

      // The surface the reflection sees: flat in the puddles, rippled in the rain
      float3 surface = normalize(lerp(shadeNormal, float3(0, 1, 0), puddle));
      float2 slope = 0;
      // (the raindrops' rings and their drops only where the drawn rain
      // lands: see Ripples, RainLandsAt)
      float drops = 0.0;
      float rainHere = 0.0;
      [branch] if (g_Rain.x > 0.0 && puddle > 0.01 && !Skipped(SkipRipples))
        rainHere = RainLandsAt(here, distance);
      [branch] if (rainHere > 0.001)
      {
        float3 rippled = Ripples(world.xz, g_Camera.w, RippleDensity());
        slope = rippled.xy * rainHere * puddle;
        drops = rippled.z * rainHere * puddle;
      }
      // and the steps in it (see StepRings: Faith's, other people's, the
      // cars' wheels), rain or not; and on the water running down a slope
      // (see SlopeAt; 2026-10-26, the user: "Sliding down on a water slope
      // should also leave a ripple": they were only where puddles are, and
      // none lies on a slope)
      float film = max(puddle, g_DripCount.y > 0.5 ? onSlope * RunningWater() : 0.0);
      float2 steps = 0.0;
      if (g_StepCounts.x + g_OtherCounts.x > 0.5 && (film > 0.01 || g_StepCounts.w > 0.5) && !Skipped(SkipRings))
        steps = StepRings(here, film, normal.y, distance);
      slope += steps;
      // and the waves of her slide (see SlideWake), on the wet ground's thin
      // film of water too, and the water running down a slope
      if (any(wake.yz != 0.0))
        slope += wake.yz * max(film, 0.4);
      // and the drips off the roofs landing in it (see DripRings)
      if (g_DripCount.x > 0.5 && puddle > 0.01 && !Skipped(SkipRings))
        slope += DripRings(here, g_Camera.w) * puddle;
      // and the drainpipes' water landing in it (see PipeRings)
      if (g_PipeCount.x > 0.5 && puddle > 0.01 && !Skipped(SkipRings))
        slope += PipeRings(here, g_Camera.w) * puddle;
      if (any(slope != 0.0))
        surface = normalize(surface + float3(slope.x, 0.0, slope.y) * 1.5);
      // The rain running down the slopes (see FlowWaves, SlopeAt), while it
      // rains, as wet as the rest there: a sheet of water whose low waves roll
      // down with it, bending what it mirrors, darker, smoother and mirroring
      // more than the wet ground round it
      float sheet = 0.0, sheetWaves = 0.0;
      if (g_DripCount.y > 0.5 && onSlope > 0.001 && g_Wet.x > 0.98)
      {
        float flow;
        float3 fall;
        float2 tilt;
        sheetWaves = FlowWaves(world, slopeNormal, g_Camera.w, fall, flow, tilt);
        // (pushed aside where she slid, coming back as in a puddle: see
        // SlideWake)
        sheet = flow * onSlope * saturate(wet * 1.5) * (1.0 - 0.6 * wake.x) * RunningWater();
        // its surface tilted by the rivulets' sides (a few degrees across the
        // way down: what it mirrors bends along them), the ground under it
        // darker all over (wet, not in patches)
        surface = normalize(surface + float3(tilt.x, 0.0, tilt.y) * sheet);
        color *= lerp(1.0, 0.82, sheet);
        // its rivulets catching the light on their sides towards the camera,
        // and their thicker streaks a little brighter, running down
        float3 light = g_Scene.SampleLevel(g_Linear, float2(0.5, 0.5), g_Frame.w).rgb;
        float2 toCamera = normalize(view.xz + 1e-4);
        float glint = saturate(dot(tilt, toCamera) * 4.0) + 0.6 * length(tilt) + 0.08 * saturate(sheetWaves);
        color = max(color + light * glint * 0.6 * sheet, 0.0);
      }
      float fresnel = 0.02 + 0.98 * pow(1.0 - saturate(dot(view, surface)), 5.0);
      // The puddle's sheen: the scene's light (the frame's average) on its
      // flat top, the more the flatter it's seen; the rain's rings shaded by
      // which way their slopes face (lighter towards the camera, darker away
      // from it), whatever the reflections (the user, 22:5x: "ripples in the
      // rain should be visible no matter what the reflections")
      if (wetPuddle > 0.001)
      {
        float3 light = g_Scene.SampleLevel(g_Linear, float2(0.5, 0.5), g_Frame.w).rgb;
        float2 toCamera = normalize(view.xz + 1e-4);
        float rings = dot(slope, toCamera) * 2.5 + length(slope) * 1.5;
        // (its sheen 0.44, 0.4 until 2026-11-08: 10 % more, as above)
        color = max(color + light * (fresnel * 0.44 + rings * 0.5) * wetPuddle * (1.0 - foreground), 0.0);
        // and the raindrops landing in it, a bright speck each (see Ripples)
        color += light * drops * 0.6 * wet * (1.0 - foreground);
      }
      // the rings on the water running down a slope, shaded the same way (in
      // a puddle its sheen above has them)
      if (any(steps != 0.0) && puddle < 0.99)
      {
        float3 light = g_Scene.SampleLevel(g_Linear, float2(0.5, 0.5), g_Frame.w).rgb;
        float2 toCamera = normalize(view.xz + 1e-4);
        float waves = dot(steps, toCamera) * 2.5 + length(steps) * 1.5;
        color = max(color + light * waves * 0.5 * wet * (1.0 - puddle), 0.0);
      }
      // the slide's waves on the wet ground off the puddles, shaded the same
      // way (in a puddle its sheen above has them)
      if (any(wake.yz != 0.0) && puddle < 0.99)
      {
        float3 light = g_Scene.SampleLevel(g_Linear, float2(0.5, 0.5), g_Frame.w).rgb;
        float2 toCamera = normalize(view.xz + 1e-4);
        float waves = dot(wake.yz, toCamera) * 2.5 + length(wake.yz) * 1.5;
        color = max(color + light * waves * 0.5 * wet * (1.0 - puddle), 0.0);
      }
      // Rough wet ground reflects less and blurred, a puddle sharp (the
      // game's own: Fresnel taken down with roughness, its reflections
      // blurred by it)
      // (the slopes' sheet of water nearly as smooth as a puddle)
      float smoothWater = sheet * 0.75 * (1.0 - puddle);
      float roughness = lerp(lerp(0.35, 0.02, puddle), 0.1, smoothWater);
      float fresnelRough = 0.02 + (max(1.0 - roughness, 0.02) - 0.02) * pow(1.0 - saturate(dot(view, surface)), 5.0);
      float amount = g_Wet.w * wet * lerp(fresnelRough * shadeGround, max(fresnel, 0.3), max(puddle, smoothWater)) *
        (1.0 + 0.35 * sheetWaves * sheet);
      // (a faint one, under 2.5 - 5 % of the colour, faded out and not
      // traced: most of rough wet ground's near the camera, the pass's dearest
      // part for what could hardly be seen; none over the outline of what's in
      // front, see NearerAround)
      amount *= smoothstep(0.025, 0.05, amount) * (1.0 - foreground);
      if (amount > 0.002 && !Skipped(SkipReflections))
      {
        // The reflection (see TraceReflection): where it finds nothing the
        // surface keeps its own colour, faded into, never cut off
        float4 reflected = TraceReflection(uv, here + surface * 0.03, reflect(-view, surface), distance, roughness);
        color = lerp(color, reflected.rgb, saturate(amount) * reflected.w);
      }
    }

    // Faith and the people out in the rain wet (2026-10-27, the user:
    // "Improve wet faith look", "ksec should be a bit wet under rain"; they
    // were left as the game drew them; then "ksec and faith still don't look
    // wet under rain": x 0.86 and a faint sheen was too little): darker, as
    // wet cloth and skin are (x 0.68 at full Wetness), its colours a little
    // deeper, and glossy: its own highlights, where the game's light falls
    // on it, brighter and sharper (its light cubed: the dark and middling
    // tones hardly, the brightest most), so the gloss follows the game's own
    // shading, smooth over the model. No puddles, rings or mirroring on them.
    // (Its gloss came from the frame's normal at first, the frame's light on
    // what faced up and where it turned away from the view; that normal comes
    // from the depth, flat over each of the model's triangles, so each caught
    // its own patch of light: 2026-10-27, the user: "You can see every
    // triangle of wet faith/ksec find a way and smooth it out".)
    if (bodyWet > 0.002)
    {
      changed = true;
      // (its colours kept as they are and its gloss by its brightness alone,
      // the same on grey: on the outline the pixels show some of what's
      // behind, the sky's blue too, the more as she runs, the game's motion
      // blur smearing it, and deepening each colour and cubing it turned that
      // into a blue rim; 2026-11-01, the user: "When faith runs there is a
      // weird blue hue around her body")
      float3 lit = color;
      float luma = Luma(lit);
      color = lit * (1.0 - 0.32 * bodyWet + luma * luma * 0.6 * bodyWet);
    }

    // Her own splash, the game's, where she steps in water (see
    // HerSplashAt): what of the floor in it is brighter than the floor round
    // it (its white sprites and drops over the darker ground; GroundLumaAround:
    // only what lies on the floor's plane counted) shows as the game drew it,
    // over the wet look. Not on her or anyone kept dry (keptHere), and not
    // where too little of the floor is round it to tell. (Until 2026-10-27
    // the round was the frame blurred 64 - 128 px, her dark legs and hands in
    // it, so the floor beside her counted as brighter and was put back as the
    // game drew it, dry and light: a jagged bright fringe round her outline
    // as she stepped, and she got lighter herself; the user's screenshot,
    // "there is still a weird halo when faith moves in the rain", "she also
    // gets brighter".)
    if (WetDrawn && changed && g_OtherCounts.y > 0.5 && keptHere < 0.999 && !Skipped(SkipMarks))
    {
      float region = HerSplashAt(position.xy, ViewDepth(depth));
      if (region > 0.001)
      {
        float onPlane;
        float groundLuma = GroundLumaAround(uv, here, normal, distance, onPlane);
        if (onPlane >= 3.0)
        {
          float bright = Luma(scene.rgb) - 1.1 * groundLuma;
          color = lerp(color, scene.rgb, region * (1.0 - keptHere) * smoothstep(0.02, 0.12, bright));
        }
      }
    }

    // Faith's wet handprints (see HandPrintsAt), on the ground or a wall,
    // under a roof too: darker where her wet hand was, as much as it's wetter
    // than the surface (none where that's as wet: see surfaceWet)
    if (WetDrawn && g_Rain.z > 0.5 && g_HandCount.x > 0.5 && !Skipped(SkipMarks))
    {
      float hand = saturate(HandPrintsAt(here) * g_Wet.x - surfaceWet);
      if (hand > 0.002)
      {
        color *= lerp(1.0, 0.6, hand);
        changed = true;
      }
    }
    // Her footprints on a wall she ran along (her wet shoes' mark), darker on
    // top of the wet look as her handprints are, as much as they're wetter
    // than the wall (see PrintsAt; the ones pressed into wet gravel aren't
    // made since 2026-11-03)
    float printMark = saturate(prints.w * g_Wet.x - surfaceWet) * (1.0 - ground);
    if (printMark > 0.002)
    {
      color *= lerp(1.0, 0.65, printMark);
      changed = true;
    }

    // (the ice over the water is its own pass now, IcePsMain, drawn over this)
    float iceOnPool = 0.0;

    // Snow on what faces up (snow chosen: the wet look gives way to it; the
    // game has no snow, Research_Snow_2026-10-26.md). Up to a 55 degree slope
    // (more on what's flatter), not under the weather's squares' roofs, not on
    // Faith, people or vehicles (KeptDry), out to its reach, into the fog as
    // the ground goes. Where the squares reach (50 m) only what they know is
    // open (KnownOpenAt: not a room through a window); past them it can't be
    // told and counts as open. Patchy while little has settled (two octaves of
    // noise over the ground), all of it once it's all settled. How white: the
    // light the frame shows there, the pixel itself (x 1.8: snow is whiter
    // than what it lies on) or what's round it, at least some of the scene's,
    // a little of the light's colour in it; so it's in shadow where the ground
    // is.
    // With depth (2026-10-26, the user: "it has no depth now and it looks
    // bland"; see SnowGround): drifts and grains in its height, shading from
    // the sky over it, a darker edge where a patch ends, a little parallax, and
    // glints near the camera.
    // Piling up (2026-10-26, the user: "the snow on the ground look like it's
    // piling up"): as it deepens (g_SnowPile.x) it fills the small gaps the
    // frame's flatness test leaves out (all of them from 8 cm), its drifts
    // grow taller and its patches go (SnowGround), and its layer's side shows
    // over ledges, curbs and steps and against the foot of walls
    // (SnowPileSide). On a frozen pool less of it lies (x 0.3: the wind clears
    // smooth ice).
    // (none of it in the rain's copy, see SnowDrawn)
    if (SnowDrawn && g_SnowDrift.z > 0.001 && !Skipped(SkipSnow))
    {
      // (its smoothness test kept as it piles up: the 06:2x build let deep
      // snow onto the pixels where the frame's surface jumps, so it bled over
      // edges, pixel by pixel: the user, "Snow shader looks pixeladed and
      // bleeds over edges")
      // (and its own facing, SnowNormal: FrameNormal's faced up at a corner
      // seen at a glancing angle, so the snow went over the edge: the user,
      // "Snow shader still bleeds around edges when viewing it from an angle")
      // Where it lies deep enough to bury the ground's own roughness (gravel,
      // cobbles: 3 - 12 cm) its facing is also taken over 4 pixels, so a
      // stone's side doesn't break it up (the user: "Snow shader on gravel
      // looks weird")
      float clean = 0.0;
      float depthHere = SnowDepthAt(world.xz);
      // (and once it covers the ground, g_SnowDrift.z 0.6 - 1, its relief of
      // a few cm counts as buried however little has piled: 2026-10-28, the
      // user, a screenshot of a roof's gravel bed, "This is how gravel looks":
      // grey and speckled stone by stone where the concrete round it was white,
      // since until 3 cm had piled the gravel's stones' facets were taken pixel
      // by pixel for the snow's facing and its bends)
      float coverBuried = smoothstep(0.6, 1.0, g_SnowDrift.z);
      float buried = max(smoothstep(0.03, 0.12, depthHere), coverBuried);
      float3 snowNormal = normal;
      [branch] if (distance < g_SnowDrift.w && !Skipped(SkipSnowFacing))
      {
        snowNormal = SnowNormal(DepthTexelUv(uv), here, distance, 1.0, clean);
        if (buried > 0.0)
        {
          float cleanWide;
          // (its points 12 cm apart on the ground, wider than a stone: at least
          // 4 of 4K's pixels (x PixelScale), at most 48; whole pixels so they
          // are on pixels' centres. Taken only as far as the surface is in line
          // that far, cleanWide: never across an edge; 4 pixels was smaller
          // than a stone close by.)
          float pixelAngle = 2.0 * g_Frame.x / g_ProjectionXY.x;
          float span = clamp(round(0.12 / max(distance * pixelAngle, 1e-5)), max(1.0, round(4.0 * PixelScale())), 48.0);
          float3 wide = SnowNormal(DepthTexelUv(uv), here, distance, span, cleanWide);
          snowNormal = normalize(lerp(snowNormal, wide, buried * cleanWide));
          clean = lerp(clean, max(clean, cleanWide), buried);
        }
      }
      // The game's own facing where the frame's surface isn't clean
      // (GameSurfaceAt): an edge, a rail, a pipe, a small or a far model,
      // where the depth can't tell which way it faces, so no snow lay there
      // or it went over the edge; the G-buffer tells pixel by pixel, to the
      // model's own edge. Where the surface is clean the depth's facing is
      // kept (the game's has its texture's bumps, which the snow covers).
      // (2026-11-01, the user: "make the snow layer look better on
      // model/meshes")
      float3 gameNormal = float3(0.0, 1.0, 0.0);
      float gameSmooth = 0.0;
      bool game = false;
      [branch] if (!Skipped(SkipSnowFacing))
        game = GameSurfaceAt(DepthTexel(uv), gameNormal, gameSmooth) && dot(gameNormal, view) > -0.2;
      float byGame = game && distance < g_SnowDrift.w ? 1.0 - clean : 0.0;
      if (byGame > 0.0)
        snowNormal = normalize(lerp(snowNormal, gameNormal, byGame));
      // (where it's deep enough to bury the stones, not cut where one stone
      // meets the next: FrameNormal's smooth, a pixel's own depth jump, left
      // out as far as the 4-pixel surface is in line (clean). Snow fills the
      // hollows between stones first and lies level once it's a few cm deep;
      // it was holed at every stone's edge: 2026-10-27, the user: "snow on
      // gravel and some surfaces still looks weird".) (Where the game's
      // facing is taken, its own: no depth jump to leave out.)
      float flat = smoothstep(0.35, 0.8, snowNormal.y) * lerp(lerp(smooth, 1.0, buried * clean) * clean, 1.0, byGame);
      float2 inGrid = (world.xz - g_ShelterGrid.xy) / g_ShelterGrid.z;
      bool pastSquares = g_ShelterGrid.w < 1.0 || any(inGrid < 0.0) || any(inGrid >= g_ShelterGrid.w);
      float open = pastSquares ? outside : min(outside, saturate((KnownOpenWith(world, interiorHere) - 0.25) * 2.0));
      // The fog: the snow fogged as the game fogs what's there, towards the
      // sky's colour as the frame shows it (g_HazeView.yzw: its sky averaged
      // over the upper part of the frame, held steady on the CPU), standing
      // in for the fog's own (the snowy look's thick fog is the sky's colour;
      // in clear weather the fog is thin, so it tints only far snow a little),
      // not faded out by it, which let the ground through it: in a blizzard's
      // 120 m, snow 30 m off was half the ground's (2026-11-01, the user:
      // "make the snow on the ground effect stronger no mistakes"). Until
      // the sky's colour is known, faded as before.
      float fogHere = FogAt(here);
      bool fogColourKnown = g_HazeView.y >= 0.0;
      float settled = flat * open * SnowReach(distance) * (fogColourKnown ? 1.0 : fogHere) * (1.0 - 0.7 * iceOnPool);
      // (soft inside its edges, SnowEdgeAt; none on what's too narrow to
      // hold it, SnowWideAt. A drop under 6 cm buried, at least: the road
      // traffic's cars each have a shadow plane under them (their meshes'
      // "Shadow" section, ss_crowdvehicleshadow: 3.46 x 5.64 m, 3.7 cm over
      // the car's lowest point), and at 3 cm its edge was taken for a drop,
      // the snow faded over its outer 1 - 2.5 pixels: a thin dark rectangle
      // round every car, moving with it; 2026-11-04, the user: "Traffic
      // still has the weird square on snow fix it")
      [branch] if (settled > 0.002 && !Skipped(SkipSnowEdges))
      {
        float edgeBuried = max(max(0.06, depthHere * 0.5), 0.04 * coverBuried);
        settled *= SnowEdgeAt(uv, here, snowNormal, edgeBuried);
        [branch] if (settled > 0.002)
          settled *= SnowWideAt(uv, here, snowNormal, distance, edgeBuried);
      }
      [branch] if (settled > 0.002 && !Skipped(SkipSnowEdges))
        settled *= (1.0 - KeptDry(here, distance)) * (1.0 - max(HandKeptAt(here), FeetKeptAt(here)));
      if (settled > 0.002)
      {
        float3 around = SceneAround(uv, 2.0);
        float3 average = g_Scene.SampleLevel(g_Linear, float2(0.5, 0.5), g_Frame.w).rgb;
        // (its light the game's there, SnowLitByGame; where the G-buffer
        // isn't there, from its own surface 6 - 20 px round, SnowLightAround:
        // the ground's own texture, a stone's light and dark, don't show
        // through it; SnowLit)
        float4 lit = 0.0;
        float3 lightAround = 0.0;
        [branch] if (!Skipped(SkipSnowLight))
        {
          lit = SnowLitByGame(DepthTexel(uv), view, FogAt(here));
          if (lit.a < 0.999)
            lightAround = SnowLightAround(uv, here, snowNormal, distance, depthHere);
        }
        float4 snow = SnowGround(world, view, snowNormal, flat, distance, color, lightAround, SceneAround(uv, 0.0), lit);
        // (laid on the road's paint, the surface's own colour: OwnColourAt)
        float overGround = OverTheGround(color, around);
        [branch] if (overGround > 0.002)
          overGround *= 1.0 - OwnColourAt(DepthTexel(uv), color, around);
        snow.a *= (1.0 - overGround) * (1.0 - GlowAt(color, average, abs(dot(view, snowNormal))));
        if (snow.a > 0.002)
        {
          float3 fogged = fogColourKnown ? lerp(g_HazeView.yzw, snow.rgb, fogHere) : snow.rgb;
          color = lerp(color, fogged, settled * snow.a);
          changed = true;
        }
      }
      // The piled layer's side, on what isn't a top itself: the ground's snow
      // going on up the wall's or the curb's foot (SnowPileSide). Switched
      // off (SnowOnWalls).
      [branch] if (SnowOnWalls && g_SnowPile.x > 0.02 && flat < 0.5 && distance < g_SnowDrift.w && !Skipped(SkipSnowSide))
      {
        float3 snowAt, seenAt;
        float wallward;
        float2 footUv;
        // (fogged as the ground's snow is, see fogHere)
        float side = SnowPileSide(uv, here, distance, snowAt, seenAt, wallward, footUv) * SnowReach(distance) *
          (fogColourKnown ? 1.0 : fogHere) * (1.0 - flat * 2.0) * (1.0 - max(HandKeptAt(here), FeetKeptAt(here)));
        if (side > 0.002)
        {
          float3 around = SceneAround(uv, 2.0);
          float3 average = g_Scene.SampleLevel(g_Linear, float2(0.5, 0.5), g_Frame.w).rgb;
          side *= (1.0 - OverTheGround(color, around)) * (1.0 - GlowAt(color, average, abs(dot(view, normal))));
          // The ground's snow going on up to the wall (SnowPileSide), lit as
          // it is (its light from round the pixel, not the wall's own pixel:
          // the wall's texture showed through), a little greyer in the
          // corner against the wall (less of the sky reaches it). Its own
          // pattern was value noise on world.xz / 0.2 + world.y / 0.1:
          // diagonal stripes up the wall.
          // (lit as the ground's snow at its foot is, SnowAroundAt there)
          // (lit as the ground's snow just in front of its foot is: the game's
          // light 8 px further out on the ground. Taken at the foot's own
          // pixel, the wall's and the ground's in one and moved by the game's
          // jitter from frame to frame, its light went from dark to bright
          // column by column: 2026-11-01, the user, with a screenshot of
          // striped snow along a parapet's foot: "snow looks weird on
          // walls/edges and it's flickering". Then 3 taps 6 px apart across,
          // all three or none, then however many: 2026-11-03, "Snow depth map
          // still looks weird find a way and fix it". Still each column's own:
          // on gravel, streaks up the wall, the user: "Snow is still on walls
          // streched fix it no mistakes". Now PileLight's lattice fixed on the
          // frame, the same for every pixel over a foot and smooth along it.)
          float3 footAverage = SceneAround(footUv, 0.0);
          float4 footLit = PileLight(footUv, snowAt.y, view, FogAt(snowAt), length(snowAt), footAverage);
          // (its look where the pixel sees the drift, seenAt, so its grain,
          // glints and ripples go on up it as they lie on the ground, not one
          // point's drawn up the wall in streaks; its light footLit's, all of
          // it: SnowGround's around isn't read then)
          float4 pile = SnowGround(seenAt + g_Camera.xyz, view, float3(0.0, 1.0, 0.0), 1.0, length(seenAt), color, footAverage,
            footAverage, footLit);
          float3 sideSnow = pile.rgb * (1.0 - 0.08 * smoothstep(0.5, 1.0, wallward));
          if (fogColourKnown)
            sideSnow = lerp(g_HazeView.yzw, sideSnow, fogHere);
          color = lerp(color, sideSnow, side * pile.a);
          changed = true;
        }
      }
    }

    // The lightning lights what's under the open sky (what faces up most),
    // under a roof only near an opening
    if (flash > 0.0 && !Skipped(SkipLightning))
    {
      float lit = FlashAt(world, shelter);
      if (lit > 0.001)
      {
        changed = true;
        color = color * (1.0 + flash * (0.3 + 1.2 * saturate(shadeNormal.y)) * lit) + g_Flash.yzw * flash * 0.05 * lit;
      }
    }
  }

  // The haze of the rain or the snow (see HazeAt), lit by the scene's own
  // light (its average, greyer and a little lighter; whiter in the snow)
  if (g_Haze.x > 0.0 && hazeOutside > 0.001 && !Skipped(SkipHaze))
  {
    float haze = HazeAt(dir, sky ? 1500.0 : distance) * hazeOutside;
    // (not over what the game drew over the ground without its depth: its
    // HUD, the markers, the Runner's Vision's path, its effects; the depth
    // says the ground or a wall far off under them, so they were hazed as
    // that was: see HazeKeptOff. A lamp or a lit window counts too, and
    // shines through. Not on the sky, where its bright edge beside a dark
    // building would count.)
    if (!sky && haze > 0.002)
      haze *= 1.0 - 0.85 * HazeKeptOff(uv);
    if (haze > 0.002)
    {
      float3 average = g_Scene.SampleLevel(g_Linear, float2(0.5, 0.5), g_Frame.w).rgb;
      float3 tint = HazeTint(average);
      color = lerp(color, tint, haze);
      changed = true;
    }
  }
  // A metre of view depth behind the frame's surface, along its normal and
  // along the view (see FallingHidden)
  float forward = max(dot(dir, -g_CameraBack.xyz), 1e-3);
  float2 behind = float2(abs(dot(dir, sceneNormal)), 1.0) / forward;
  // Drips off the roof edges (see DripsAt), in front of what's behind them
  if (WetDrawn && g_DripCount.x > 0.5 && !Skipped(SkipFalling))
  {
    float drip = DripsAt(position.xy, sky ? 1e9 : ViewDepth(depth), behind, g_Camera.w);
    if (drip > 0.001)
    {
      float3 light = g_Scene.SampleLevel(g_Linear, float2(0.5, 0.5), g_Frame.w).rgb;
      color = lerp(color, light * 1.6 + 0.05, drip * 0.55);
      changed = true;
    }
  }
  // The drainpipes' water (see PipesAt), in front of what's behind it
  if (WetDrawn && g_PipeCount.x > 0.5 && !Skipped(SkipFalling))
  {
    float water = PipesAt(position.xy, sky ? 1e9 : ViewDepth(depth), behind, g_Camera.w);
    if (water > 0.001)
    {
      float3 light = g_Scene.SampleLevel(g_Linear, float2(0.5, 0.5), g_Frame.w).rgb;
      float3 around = g_Scene.SampleLevel(g_Linear, uv, max(0.0, g_Frame.z - 2.0)).rgb;
      color = lerp(color, max(around * 1.3, light * 1.6) + 0.04, water * 0.6);
      changed = true;
    }
  }
  // A little splash at every ripple (see SplashesAt), in front of what's
  // behind it, lit as the drainpipes' water
  if (WetDrawn && g_OtherCounts.y > 0.5 && !Skipped(SkipFalling))
  {
    float splash = SplashesAt(position.xy, sky ? 1e9 : ViewDepth(depth));
    if (splash > 0.001)
    {
      float3 light = g_Scene.SampleLevel(g_Linear, float2(0.5, 0.5), g_Frame.w).rgb;
      float3 around = g_Scene.SampleLevel(g_Linear, uv, max(0.0, g_Frame.z - 2.0)).rgb;
      color = lerp(color, max(around * 1.3, light * 1.6) + 0.05, splash * 0.65);
      changed = true;
    }
  }
  // The rain in the distance, over the sky and what's far enough
  float4 rain = 0.0;
  [branch] if (WetDrawn && !Skipped(SkipDistantRain))
    rain = RainAt(uv, dir, distance);
  if (rain.a > 0.001)
  {
    color = lerp(color, rain.rgb, rain.a);
    changed = true;
  }
  // The bolt
  float bolt = 0.0;
  [branch] if (!Skipped(SkipLightning))
    bolt = BoltAt(position.xy, sky, length(dir.xz) * distance);
  if (bolt > 0.001)
  {
    color += float3(0.85, 0.9, 1.0) * bolt;
    changed = true;
  }
  if (!changed)
    discard;
  return float4(color, scene.a);
}

// The ends of the slopes the rain runs down (2026-10-26, the user: "also
// after the water slope ends they should be water dripping from the end of
// it"; WetLook.cpp's slope ends, drawn with the roofs' drips): to the right
// of Faith's squares in PsQuery's target, SlopeEndsAcross x SlopeEndsDown
// cells over the frame (the same numbers as WetLook.cpp's). Each looks at
// one point of its cell, picked by a hash that moves on each frame (sixty
// points a second): where the running water is drawn (PsMain's sheet there
// at least a quarter, within SlopeEndReach of the camera), it goes down the
// slope's fall line in SlopeEndStep steps, at most SlopeEndSteps, each from
// where the frame's surface was found (so it follows a curve, and the
// plane's small error doesn't add up). Seen through a step's point: about
// at it (within 5 cm + 0.4 % of the distance, or up to 30 cm farther), the
// slope goes on; nearer (the slope runs into the ground, or under
// something, or something's in front), no end; more than 30 cm (or 2 % of
// the distance) farther, or the sky: the slope ended there, found to 2 cm
// (three halvings). Returns where its water lets go (camera-relative): 6 cm
// on past the end, 2 cm under it; w 1 found, 0 none.
static const int QuerySide = 32;
static const int SlopeEndsAcross = 64, SlopeEndsDown = 36;
static const float SlopeEndReach = 30.0, SlopeEndStep = 0.15;
static const int SlopeEndSteps = 20;

float4 SlopeEndAt(int2 cell)
{
  if (g_DripCount.y < 0.5 || g_Rain.z < 0.5 || g_Wet.x <= 0.0 || cell.y >= SlopeEndsDown)
    return 0;
  float2 jitter = float2(Hash(float2(cell) + frac(g_Camera.w * 0.731) * 1000.0),
    Hash(float2(cell.yx) + 31.7 + frac(g_Camera.w * 0.317) * 1000.0));
  float2 uv = (float2(cell) + jitter) / float2(SlopeEndsAcross, SlopeEndsDown);
  float depth = DepthAt(uv);
  if (IsSky(depth))
    return 0;
  float3 here = RelativeAt(uv, depth);
  float distance = length(here);
  if (distance > SlopeEndReach)
    return 0;
  float3 slopeNormal;
  float onSlope = SlopeAt(uv, here, g_ProjectionXY.y * 0.5 / (max(distance, 0.1) * g_Frame.y), slopeNormal);
  // (only on solid, broad slopes, as PsMain's: see PuddleSurface)
  if (onSlope >= 0.25)
    onSlope *= PuddleSurface(here, slopeNormal, distance);
  if (onSlope < 0.25)
    return 0;
  // Its fall line and how much runs down it (as FlowWaves), as wet as PsMain
  // has it there (out from under the roofs, within Wet up to, out of the fog)
  float3 fall = float3(0.0, -1.0, 0.0) + slopeNormal * slopeNormal.y;
  float steep = length(fall);
  if (steep < 0.08)
    return 0;
  fall /= steep;
  float outside = 1.0 - smoothstep(0.25, 0.75, ShelterAt(here + g_Camera.xyz));
  float wet = g_Wet.x * saturate(1.0 - distance / g_Rain.y) * outside * FogAt(here);
  if (smoothstep(0.08, 0.3, steep) * onSlope * saturate(wet * 1.5) * RunningWater() < 0.25)
    return 0;
  float3 at = here;
  bool ended = false;
  [loop] for (int k = 0; k < SlopeEndSteps; ++k)
  {
    float3 p = at + fall * SlopeEndStep, seen;
    if (!SurfaceThrough(p, seen))
      return 0;
    float lp = length(p), ls = length(seen);
    if (ls > lp + max(0.3, 0.02 * lp))
    {
      ended = true;
      break;
    }
    if (ls < lp - (0.05 + 0.004 * lp))
      return 0;
    at = seen;
  }
  if (!ended)
    return 0;
  float lo = 0.0, hi = SlopeEndStep;
  [unroll] for (int m = 0; m < 3; ++m)
  {
    float mid = 0.5 * (lo + hi);
    float3 p = at + fall * mid, seen;
    bool through = SurfaceThrough(p, seen);
    float lp = length(p);
    if (through && length(seen) <= lp + max(0.3, 0.02 * lp))
      lo = mid;
    else
      hi = mid;
  }
  float2 away = fall.xz / max(length(fall.xz), 1e-4);
  return float4(at + fall * lo + float3(away.x, 0.0, away.y) * 0.06 - float3(0.0, 0.02, 0.0), 1.0);
}

// Faith's steps (wetlook::InPuddle) can't see the frame's depth, so this
// pass works out, for squares round her feet (g_QueryGrid, one pixel each),
// what PsMain's puddles take from the frame's surface there: FlatAround x
// PuddleSurface (x), and the surface's height (y, relative to the camera).
// x is -1 where the frame doesn't show that square's surface (off the frame,
// behind something, the sky).
float4 PsQuery(float4 position : SV_POSITION) : SV_TARGET
{
  float2 square = floor(position.xy);
  // (to the right of the squares, the slopes' ends: see SlopeEndAt)
  if (square.x >= QuerySide)
    return SlopeEndAt(int2(square) - int2(QuerySide, 0));
  // (under them, one for the haze's colour: see HazeSkyAt)
  if (square.x == 0.0 && square.y == HazeSkyRow)
    return HazeSkyAt();
  // (no squares asked about this frame, or under them)
  if (g_QueryGrid.z <= 0.0 || square.y >= QuerySide)
    return float4(-1.0, 0.0, 0.0, 0.0);
  float3 spot = float3(g_QueryGrid.x + (square.x + 0.5) * g_QueryGrid.z, g_QueryGrid.w,
    g_QueryGrid.y + (square.y + 0.5) * g_QueryGrid.z);
  float4 clip = mul(float4(spot, 1.0), g_ViewProjection);
  if (clip.w <= 0.1)
    return float4(-1.0, 0.0, 0.0, 0.0);
  float2 uv = float2(clip.x / clip.w * 0.5 + 0.5, 0.5 - clip.y / clip.w * 0.5);
  if (any(uv < 0.0) || any(uv > 1.0))
    return float4(-1.0, 0.0, 0.0, 0.0);
  float depth = DepthAt(uv);
  if (IsSky(depth))
    return float4(-1.0, 0.0, 0.0, 0.0);
  float3 here = RelativeAt(uv, depth);
  // That square's own surface, not something in front of it or far under it
  if (length(here.xz - spot.xz) > g_QueryGrid.z || abs(here.y - spot.y) > 1.5)
    return float4(-1.0, 0.0, 0.0, 0.0);
  float distance = length(here);
  float smooth;
  float3 normal = FrameNormal(uv, here, distance, smooth);
  // (and not a slope, see SlopeAt: no puddle drawn there)
  float3 slopeNormal;
  float onSlope = SlopeAt(uv, here, g_ProjectionXY.y * 0.5 / (max(distance, 0.1) * g_Frame.y), slopeNormal);
  // (by the ground's nodes where they're this frame's, as PsMain and
  // GbufferWet.hlsl draw the puddles then: see PuddleGround)
  float surface;
  [branch] if (g_Options.w > 0.5)
    surface = PuddleGround(here, normal, distance, here + g_Camera.xyz);
  else
    surface = PuddleSurface(here, normal, distance);
  return float4(FlatAround(uv, here, normal) * surface * (1.0 - onSlope) * smoothstep(0.95, 0.98, normal.y), here.y, 0.0, 0.0);
}
