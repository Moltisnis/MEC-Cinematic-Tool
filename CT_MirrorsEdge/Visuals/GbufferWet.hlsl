// VISUALS > Weather > Wet surfaces > "In the game's own lighting": the wet
// ground written into the game's own G-buffer before the game lights it
// (GbufferWet.cpp; 2026-11-02, the user: "Lets go for A option", the fix
// Research_ScreenSpace_2026-11-02.md section 7.A ranks first). Compiled into
// GbufferWetInfoPs.h and GbufferWetPs.h with the Windows SDK's fxc:
//   fxc /nologo /O3 /T ps_4_1 /E GbufferWetInfoPs /Vn g_GbufferWetInfoPs /Fh GbufferWetInfoPs.h GbufferWet.hlsl
//   fxc /nologo /O3 /T ps_4_1 /E GbufferWetPs /Vn g_GbufferWetPs /Fh GbufferWetPs.h GbufferWet.hlsl
// and drawn with WetLook.hlsl's full-screen triangle (WetLookVs.h), the
// first into the tools' own info target, the second into the game's targets.
//
// The wet look at Present darkens the finished frame and traces a reflection
// of its own over it. Here the same wet ground, where WetLook.hlsl's PsMain
// has it and by its own functions (dry under the weather's squares' roofs and
// in the game's interiors, people and vehicles kept out, puddles where its
// noise is highest and its flatness, hole, stair and edge tests let them lie,
// her prints, the tyre tracks, the water down the slopes), goes into what the
// game lights:
// - target 0 z, the smoothness: the water's film's once there's enough of it
//   (the wet ground's 0.5 - 0.75 by Reflections, a puddle's 0.85 - 0.98),
//   never lower than the game's. The game's screen-space reflections take a
//   surface from 0.6 (all of it from 0.7, the city's RoughnessFade), its cube
//   maps and lights any;
// - target 0 xy and target 1 a, the facing: where the depth's surface is
//   level and clean, the water lies on its plane (over the game's facing
//   close to it: a surface's own relief, gravel, stays; FilmCoversLeast);
//   in a puddle flattened to
//   level and rippled (the rain's drops, the rings of steps, drips and
//   drainpipes, her slide's and the bodies' waves), on the water down a slope
//   tilted by its rivulets; written the game's own way (in one of its 6
//   bases, see GameEncode) and only where it reads back as written;
// - target 1 rgb, the base colour: darker, as the wet look darkened the
//   frame (x 0.55 at full Wetness on the ground, x 0.67 more in a puddle,
//   x 0.82 more under the water down a slope).
// The game's own screen-space reflections, cube maps and lights then show
// the wet ground: no outline of what stands in front is mixed into it (the
// G-buffer has none of the frame's antialiasing or motion blur), and where a
// reflection finds nothing the city's cube maps fill it. The wet look at
// Present leaves its ground's darkening, sheen and reflection out then (its
// g_Options.y, WetLook.hlsl's PsMain), and reads where it's open and kept dry
// from this pass's info (g_Options.z, GbufferWetInfoPs).
//
// Its constants (b0) are the wet look's of the last frame drawn with this
// frame's camera's turn and projection (wetlook::TakeGbufferPass): what's
// worked out from this frame's depth is relative to this frame's camera.
// Where the camera is (g_Camera.xyz) stays the last frame's, which the wet
// look's lists are relative to (prints, rings, the boxes kept dry, the
// interiors, the rain's boxes): their point is "here", this frame's camera's
// place (g_NowCamera, b3) and the pixel's offset from it, less the last one.
// Its depth (t1) is the game's own of this frame, its G-buffer (t6, t7) the
// game's targets 0 and 1 copied just before (the targets are written), its
// bases (b1) the game's viewConstants of this frame, the info (t10) the first
// pass's, the ground's nodes (t11, b4) WetLook.hlsl's NodesPs's, drawn just
// before both.

// (none of the snow's parts in it: WetLook.hlsl's SnowDrawn)
#define RAIN_ONLY 1
#include "WetLook.hlsl"

cbuffer GbufferNow : register(b3)
{
  float4 g_NowCamera;   // xyz: where this frame's camera is (world)
};

// A facing (world) in the game's basis k: the inverse of GameSurfaceAt's
// (its bases' rows are a turn, so their transpose undoes them)
float3 GameLocal(float3 n, int k)
{
  int i = k * 3;
  return n.x * g_NormalBasis[i].xyz + n.y * g_NormalBasis[i + 1].xyz + n.z * g_NormalBasis[i + 2].xyz;
}

// And back, as GameSurfaceAt (and the game's own lighting) reads it
float3 GameWorld(float3 local, int k)
{
  int i = k * 3;
  return float3(dot(local, g_NormalBasis[i].xyz), dot(local, g_NormalBasis[i + 1].xyz), dot(local, g_NormalBasis[i + 2].xyz));
}

// The facing n as the game writes it: target 0's xy and its basis (target
// 1's a x 6). The pixel's own basis while n is within about 70 degrees of
// its z (as the game picked it for the surface), else the one of the 6 n is
// most along. False where it wouldn't read back as n within 2.5 degrees
// (the bases not a turn, say): the facing is then left as the game drew it.
bool GameEncode(float3 n, int own, out float2 xy, out int basis)
{
  basis = own;
  float3 local = GameLocal(n, own);
  if (local.z < 0.35)
  {
    float best = -2.0;
    [unroll] for (int k = 0; k < 6; ++k)
    {
      float3 l = GameLocal(n, k);
      if (l.z > best)
      {
        best = l.z;
        basis = k;
        local = l;
      }
    }
  }
  xy = saturate(local.xy * 0.5 + 0.5);
  float2 back = xy * 2.0 - 1.0;
  float3 again = GameWorld(float3(back, sqrt(saturate(1.0 - dot(back, back)))), basis);
  return local.z >= 0.0 && dot(again, n) > 0.999;
}

// Where a point of the frame is: relative to this frame's camera (now: what's
// read round it on the depth) and to the last frame's (here: the wet look's
// lists'; see above)
void PlaceAt(float2 uv, float depth, out float3 now, out float3 world, out float3 here)
{
  now = RelativeAt(uv, depth);
  world = now + g_NowCamera.xyz;
  here = world - g_Camera.xyz;
}

// What's out in the open and what's kept dry at each of the frame's points,
// worked out once a frame, before GbufferWetPs, into the tools' own target
// (GbufferWet.cpp's info), for that pass and for the wet look at Present (its
// g_Options.z, WetInfoAt; 2026-11-02, the user said "Yes, speed it up": both
// passes worked them out, the Present one's shelter and interiors 0.76 ms
// and its kept-dry boxes 1.15 ms in the 23:26 breakdown). x how much is out
// in the open (1 - the weather's squares' shelter, the game's interiors in
// it), y how much is kept dry (KeptDryKind's x: Faith, people, vehicles), z how
// much of that by Faith or a person (its y), w the game's interiors there.
// The kept only where the ground would be wet, as PsMain works it out. Not
// the sky (left as it was).
// Both passes' parts can be left out for the performance test's breakdown
// (GbufferWet.cpp's InfoLeftOut, WetLeftOut: WetLook.hlsl's g_Skip, b2; 0 in
// every frame's own draws, all of them drawn), each with an if: HLSL's ?: and
// && work out both sides.
float4 GbufferWetInfoPs(float4 position : SV_POSITION) : SV_Target0
{
  float2 uv = position.xy * g_Frame.xy;
  float depth = DepthAt(uv);
  if (IsSky(depth))
    discard;
  if (Skipped(SkipAll))
    discard;
  float3 now, world, here;
  PlaceAt(uv, depth, now, world, here);
  float distance = length(now);
  float interiorHere = 0.0, outside = 1.0;
  [branch] if (!Skipped(SkipShelter))
  {
    interiorHere = InteriorHere(world);
    outside = 1.0 - smoothstep(0.25, 0.75, ShelterWith(world, interiorHere));
  }
  float wetFade = saturate(1.0 - distance / g_Rain.y) * FogAt(now);
  float2 kept = 0.0;
  [branch] if (g_Wet.x * wetFade * outside > 0.002 && !Skipped(SkipKept))
    kept = KeptDryKind(here, distance);
  return float4(outside, kept, interiorHere);
}

// How far the game's own facing may be off the depth's level, clean plane for
// the water's film to lie flat over it (GbufferWetPs's flatten; cosines):
// fully within 12 degrees (a decal's or a stain's turn, a model's triangles':
// the 11-03 roof box's top up to about 11, its lines; dump 20261103_022739),
// none past 25, where it's the surface's own relief, more than a film covers,
// and the game's facing stays (gravel's stones, 34 degrees off at the median,
// were all laid flat: the gravel one smooth mirror, 2026-11-05, the user's
// G-buffer dumps 20261105_161518 / _161520; a vent's rim, a box's bevel).
// Modelled on the three dumps (scratchpad flatten_model.py): the gravel kept
// as the game drew it, its concrete path, the box tops and vents of
// 20261105_161550 and the 11-03 box top moved over 1 degree on under 1.5 %
static const float FilmCoversMost = 0.978148;    // cos 12 degrees
static const float FilmCoversLeast = 0.906308;   // cos 25 degrees

struct GbufferOut
{
  float4 Normals : SV_Target0;   // the game's target 0
  float4 Colour : SV_Target1;    // its target 1 (sRGB: written as linear)
};

GbufferOut GbufferWetPs(float4 position : SV_POSITION)
{
  int2 px = int2(position.xy);
  float4 g0 = g_GameNormals.Load(int3(px, 0));
  float4 g1 = g_GameColour.Load(int3(px, 0));
  GbufferOut o;
  o.Normals = g0;
  o.Colour = g1;
  float2 uv = position.xy * g_Frame.xy;
  float depth = DepthAt(uv);
  if (IsSky(depth) || g_Rain.z < 0.5)
    discard;
  if (Skipped(SkipAll))
    discard;
  float3 now, world, here;
  PlaceAt(uv, depth, now, world, here);
  float distance = length(now);
  float3 view = -now / max(distance, 1e-3);

  // Dry where something is over it, into the fog as the ground goes, out to
  // Wet up to; not on Faith, people, vehicles or trains (the G-buffer's
  // outlines are exact, so none of KeptDryAt's softening round them): the
  // info pass's (GbufferWetInfoPs)
  float4 info = g_WetInfo.Load(int3(px, 0));
  float outside = info.x, keptHere = info.y, interiorHere = info.w;
  float wetFade = saturate(1.0 - distance / g_Rain.y) * FogAt(now);
  float wet = g_Wet.x * wetFade * outside * (1.0 - keptHere);
  if (Skipped(SkipWet))
    discard;
  // (nothing more to do where it's dry and none of her prints or the tyre
  // tracks can reach: their own boxes, PrintsAt's and MarksAt's; the depth's
  // facing and the rest not read there)
  bool printsNear = g_StepCounts.y > 0.5 && !Skipped(SkipMarks) && !OffBox(here, g_PrintBounds[0], g_PrintBounds[1]);
  bool marksNear = g_MarksOn.z > 0.5 && !Skipped(SkipMarks) && length(world.xz - g_MarksWindow.yz) < g_MarksWindow.w;
  if (wet <= 0.002 && !printsNear && !marksNear)
    discard;

  // Which way it faces: the depth's (where puddles and slopes go, as PsMain),
  // and for how much of the wet ground's darkening and gloss and the plane
  // the water lies on, the surface's own: where the depth's is level and
  // clean (its plane, no jump round it) that plane, elsewhere the game's
  // own facing, exact at every pixel. (Until 2026-11-02 the depth's with its
  // jumps taken out, and the game's only where the depth's leaned: on the thin
  // and small things, vents' slats, planks and their gaps, the edges of a
  // box's top, the jumps left them dry in stripes and patches; the user, "the
  // rain still looks bad on top of vents/boxes/woodplanks", with a screenshot
  // of a roof box's top.)
  float smooth = 1.0;
  float3 normal = float3(0.0, 1.0, 0.0);
  [branch] if (!Skipped(SkipFacing))
    normal = FrameNormal(uv, now, distance, smooth);
  float ground = smoothstep(0.55, 0.85, normal.y) * smooth;
  float3 gameNormal = normal;
  float gameSmooth = 0.0;
  bool haveGame = false;
  [branch] if (!Skipped(SkipFacing))
    haveGame = GameSurfaceAt(px, gameNormal, gameSmooth) && dot(gameNormal, view) > 0.0;
  float lean = 1.0 - smoothstep(0.93, 0.97, normal.y);
  float plane = (1.0 - lean) * smooth;
  float3 shadeNormal = haveGame ? normalize(lerp(gameNormal, normal, plane)) : normal;
  float shadeGround = haveGame ? smoothstep(0.55, 0.85, shadeNormal.y) : ground;

  // Her wet shoes' prints and where she pressed the water out (PrintsAt),
  // the cars' wet tyre tracks (MarksAt), as PsMain has them
  float4 prints = 0.0;
  [branch] if (printsNear)
    prints = PrintsAt(here);
  // (her wet shoes' only where the ground is drier: as PsMain, the pressed
  // ones aren't made since 2026-11-03)
  if (ground > 0.3)
    wet = max(wet, prints.x * ground * g_Wet.x);
  float2 marksHere = 0.0;
  [branch] if ((g_MarksOn.y > 0.5 || g_MarksOn.z > 0.5) && !Skipped(SkipMarks))
    marksHere = MarksAt(world);
  if (g_MarksOn.z > 0.5 && ground > 0.3)
    wet = max(wet, marksHere.x * ground * max(g_Wet.x, g_Wet.y) * wetFade * (1.0 - keptHere));
  if (wet <= 0.002)
    discard;

  // The puddles and the water down the slopes, as PsMain finds them
  float wall = (1.0 - shadeGround) * 0.3;
  float n = Noise(world.xz / g_Wet.z) * 0.7 + Noise(world.xz / (g_Wet.z * 0.37) + 17.0) * 0.3;
  float pixelsPerMetre = g_ProjectionXY.y * 0.5 / (max(distance, 0.1) * g_Frame.y);
  float3 slopeNormal = normal;
  float onSlope = 0.0;
  [branch] if ((g_DripCount.y > 0.5 || g_Wet.y > 0.001) && !Skipped(SkipEdgeTests))
    onSlope = SlopeAt(uv, now, pixelsPerMetre, slopeNormal);
  [branch] if (onSlope > 0.001 && !Skipped(SkipPuddleTests))
    onSlope *= PuddleSurface(now, slopeNormal, distance);
  float level = smoothstep(0.95, 0.98, normal.y);
  float puddle = g_Wet.y > 0.001 ? smoothstep(1.0 - g_Wet.y, 1.0 - g_Wet.y + 0.14, n) * ground * level * (1.0 - onSlope) : 0.0;
  if (puddle > 0.001 && !Skipped(SkipPuddleTests))
  {
    float2 inGrid = (world.xz - g_ShelterGrid.xy) / g_ShelterGrid.z;
    bool pastSquares = g_ShelterGrid.w >= 1.0 && (any(inGrid < 0.0) || any(inGrid >= g_ShelterGrid.w));
    [branch] if (!pastSquares)
      puddle *= saturate((KnownOpenWith(world, interiorHere) - 0.5) * 2.0);
  }
  [branch] if (puddle > 0.001 && !Skipped(SkipPuddleTests))
    puddle *= FlatAround(uv, now, normal);
  // (not on what they can't lie on, by the ground's nodes drawn just before
  // this pass, GbufferWet.cpp: see WetLook.hlsl's PuddleGround, NodesPs)
  [branch] if (puddle > 0.001 && !Skipped(SkipPuddleTests))
    puddle *= PuddleGround(now, normal, distance, world);
  // Her slide's wake, the bodies down in the water, the crowd's and the
  // traffic's way through it (as PsMain)
  float3 wake = 0.0;
  [branch] if (g_SlideCount.x > 1.5 && !Skipped(SkipRings))
    wake = SlideWake(here, distance);
  wake.yz *= smoothstep(0.6, 0.8, normal.y);
  [branch] if (g_BodyRippleCount.x > 0.5 && !Skipped(SkipRings))
  {
    float3 bodies = BodyRipplesAt(here, distance);
    wake.x = max(wake.x, bodies.x);
    wake.yz += bodies.yz * smoothstep(0.6, 0.8, normal.y);
  }
  [branch] if (g_SnowStampCount.z > 1.5 && g_SnowStampCount.x > 0.5 && !Skipped(SkipRings))
    wake.x = max(wake.x, BodyPressedAt(here));
  if (g_MarksOn.y > 0.5 && (puddle > 0.001 || wet > 0.001))
    wake.x = max(wake.x, marksHere.y * smoothstep(0.6, 0.8, normal.y));
  if (wake.x > 0.001)
  {
    puddle *= 1.0 - 0.6 * wake.x;
    wet *= 1.0 - 0.35 * wake.x;
  }
  float facing = saturate(shadeGround + wall);
  float wetPuddle = puddle * wet;
  // (a puddle x 0.67 darker than the wet ground round it, 0.7 until
  // 2026-11-08, the user: "can we make the puddles from rain a little more
  // visible? tiny bit like 10%": its difference from the ground 10 % more)
  float darken = lerp(1.0, 0.55, wet * facing) * lerp(1.0, 0.67, wetPuddle);
  // The water's film: once there's enough of it (from 30 % of Wetness), its
  // smoothness and its plane are the surface's whatever the material under it
  // had. A partly wet surface kept some of the game's own of each, and a
  // decal's or a stain's smoothness and facing differ from the surface round
  // it: on either side of where the screen-space reflections start (0.6 -
  // 0.7) they showed as hard-edged patches, and the facing's turn over a
  // model's triangles as lines across it (the user's screenshot, above).
  float film = smoothstep(0.0, 0.3, wet);

  // The surface the light and the reflections see: the game's own facing
  // (its bumps too) off the level, the water's plane on it, flat in the
  // puddles, rippled in the rain. The plane only over what the water's film
  // covers: the game's facing within FilmCoversMost of it, left as the game
  // drew it past FilmCoversLeast (see there)
  float fine = haveGame ? smoothstep(FilmCoversLeast, FilmCoversMost, dot(gameNormal, normal)) : 1.0;
  float flatten = plane * film * fine;
  float3 baseNormal = haveGame ? normalize(lerp(gameNormal, normal, flatten)) : normal;
  float3 surface = normalize(lerp(baseNormal, float3(0.0, 1.0, 0.0), puddle));
  float2 slope = 0.0;
  float rainHere = 0.0;
  [branch] if (g_Rain.x > 0.0 && puddle > 0.01 && !Skipped(SkipRipples))
    rainHere = RainLandsAt(here, distance);
  [branch] if (rainHere > 0.001)
    slope = Ripples(world.xz, g_Camera.w, RippleDensity()).xy * rainHere * puddle;
  float filmWater = max(puddle, g_DripCount.y > 0.5 ? onSlope * RunningWater() : 0.0);
  [branch] if (g_StepCounts.x + g_OtherCounts.x > 0.5 && (filmWater > 0.01 || g_StepCounts.w > 0.5) && !Skipped(SkipRings))
    slope += StepRings(here, filmWater, normal.y, distance);
  if (any(wake.yz != 0.0))
    slope += wake.yz * max(filmWater, 0.4);
  [branch] if (g_DripCount.x > 0.5 && puddle > 0.01 && !Skipped(SkipRings))
    slope += DripRings(here, g_Camera.w) * puddle;
  [branch] if (g_PipeCount.x > 0.5 && puddle > 0.01 && !Skipped(SkipRings))
    slope += PipeRings(here, g_Camera.w) * puddle;
  if (any(slope != 0.0))
    surface = normalize(surface + float3(slope.x, 0.0, slope.y) * 1.5);
  // The rain running down the slopes (FlowWaves): its rivulets tilt the
  // surface, the ground under it darker and smoother
  float sheet = 0.0;
  if (g_DripCount.y > 0.5 && onSlope > 0.001 && g_Wet.x > 0.98)
  {
    float flow;
    float3 fall;
    float2 tilt;
    FlowWaves(world, slopeNormal, g_Camera.w, fall, flow, tilt);
    sheet = flow * onSlope * saturate(wet * 1.5) * (1.0 - 0.6 * wake.x) * RunningWater();
    surface = normalize(surface + float3(tilt.x, 0.0, tilt.y) * sheet);
    darken *= lerp(1.0, 0.82, sheet);
  }

  // Smoother, never rougher than the game drew it: the film's 0.5 - 0.75 by
  // Reflections where it faces up (the city's screen-space reflections from
  // 0.6, all of them from 0.7; walls a third as much), a puddle 0.85 - 0.98,
  // the water down a slope 0.9
  float smoothness = g0.z;
  smoothness = max(smoothness, lerp(smoothness, lerp(0.5, 0.75, g_Wet.w), film * facing));
  smoothness = max(smoothness, lerp(smoothness, lerp(0.85, 0.98, g_Wet.w), wetPuddle));
  smoothness = max(smoothness, lerp(smoothness, 0.9, sheet * 0.75 * (1.0 - puddle)));
  o.Normals.z = smoothness;
  o.Colour.rgb = g1.rgb * darken;
  // The facing, where the water changed it: from the game's own, or (where
  // that couldn't be read: a decal's blend of two bases, say) the depth's
  // plane where that's level and clean
  bool changed = puddle > 0.001 || any(slope != 0.0) || sheet > 0.001 || flatten > 0.001;
  if (changed && (haveGame || plane > 0.5))
  {
    float2 xy;
    int basis;
    if (GameEncode(surface, clamp((int)round(g1.a * 6.0), 0, 5), xy, basis))
    {
      o.Normals.xy = xy;
      // (the basis as the game writes it, its own value: g1.a may be a
      // decal's blend of two)
      o.Colour.a = basis / 6.0;
    }
  }
  return o;
}
