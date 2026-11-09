// The ground's marks round the camera (GroundMarks.cpp): a top-down map,
// MarksTexels across at MarksTexel metres each, wrapping round (a world
// texel's place is its number modulo MarksTexels), red how far the snow is
// pressed there (in the rain: a wet tyre track), green how stirred the water
// is (0 - 1), alpha the ground's height there (the map's HeightLow +
// MarksHeightRange x it, GroundMarks.h). Compiled into
// GroundMarksVs.h, GroundMarksPs.h, GroundMarksFillVs.h and
// GroundMarksFillPs.h with the Windows SDK's fxc:
//   fxc /nologo /O3 /T vs_4_0 /E StampVs /Vn g_GroundMarksVs /Fh GroundMarksVs.h GroundMarks.hlsl
//   fxc /nologo /O3 /T ps_4_0 /E StampPs /Vn g_GroundMarksPs /Fh GroundMarksPs.h GroundMarks.hlsl
//   fxc /nologo /O3 /T vs_4_0 /E FillVs /Vn g_GroundMarksFillVs /Fh GroundMarksFillVs.h GroundMarks.hlsl
//   fxc /nologo /O3 /T ps_4_0 /E FillPs /Vn g_GroundMarksFillPs /Fh GroundMarksFillPs.h GroundMarks.hlsl
// - Stamps (StampVs / StampPs), drawn with MAX (their height over what was
//   there, only where they leave a mark): a step (a shoe's sole) or a
//   wheel's path since the frame before (a capsule its tyre's width), each a
//   quad four times (at its place and moved a map across each way, so a
//   stamp over the map's edge goes on round it).
// - The fill (FillPs, the whole map, subtracted): the snow falling into the
//   marks, the water settling.

static const int MostStamps = 256;

cbuffer Marks : register(b0)
{
  float4 g_Map;              // x texels across, y metres a texel, z red written (1 / 0), w green
  float4 g_Fill;             // x taken from red, y from green (FillPs)
  float4 g_StampA[MostStamps];   // xy its middle (a step) or its start (a wheel), in the map's texels (0 - g_Map.x); zw a wheel's end
  float4 g_StampB[MostStamps];   // x half its width (m), y half its length (a step, m), z its kind (0 a wheel, 1 a step), w how much red (0 - 1)
  float4 g_StampC[MostStamps];   // xy the way a step points (unit), z how much green (0 - 1), w the ground's height (0 - 1, see above)
};

struct StampVertex
{
  float4 position : SV_POSITION;
  float2 local : TEXCOORD0;   // the point from the stamp's middle (a step: along, across) or from its start (a wheel), metres
  nointerpolation float4 shape : TEXCOORD1;   // x half width, y half length (a step) or the path's length (a wheel), z kind, w how much
  nointerpolation float2 way : TEXCOORD2;     // a wheel: its path's way (unit, map axes)
  nointerpolation float2 more : TEXCOORD3;    // x how much green, y the ground's height
};

StampVertex StampVs(uint id : SV_VertexID, uint instance : SV_InstanceID)
{
  StampVertex o;
  int s = (int)(instance / 4);
  uint copy = instance % 4;
  float4 a = g_StampA[s], b = g_StampB[s];
  float texel = g_Map.y, across = g_Map.x;
  // Its corners, in metres from its middle, along and across its way
  float2 way, middle;
  float halfLength, halfWidth = b.x;
  float wheelLength = 0.0;
  if (b.z > 0.5)
  {
    way = g_StampC[s].xy;
    middle = a.xy;
    halfLength = b.y;
  }
  else
  {
    float2 path = (a.zw - a.xy) * texel;
    wheelLength = length(path);
    way = wheelLength > 1e-4 ? path / wheelLength : float2(1.0, 0.0);
    middle = (a.xy + a.zw) * 0.5;
    halfLength = wheelLength * 0.5 + halfWidth;
  }
  float2 side = float2(-way.y, way.x);
  // (a little over its own size, for its soft edge)
  float2 corner = float2(id == 1 || id == 2 || id == 4 ? 1.0 : -1.0, id == 2 || id == 4 || id == 5 ? 1.0 : -1.0);
  float2 metres = corner * float2(halfLength, halfWidth) * 1.25;
  float2 at = middle + (way * metres.x + side * metres.y) / texel;
  // The copies moved a map across, towards the map's middle
  float2 shift = float2(middle.x < across * 0.5 ? across : -across, middle.y < across * 0.5 ? across : -across);
  at += float2(copy & 1 ? shift.x : 0.0, copy & 2 ? shift.y : 0.0);
  o.position = float4(at.x / across * 2.0 - 1.0, 1.0 - at.y / across * 2.0, 0.0, 1.0);
  // (a wheel's point from its start along its way: its middle is half its path on)
  o.local = b.z > 0.5 ? metres : float2(metres.x + wheelLength * 0.5, metres.y);
  o.shape = float4(halfWidth, b.z > 0.5 ? halfLength : wheelLength, b.z, b.w);
  o.way = way;
  o.more = g_StampC[s].zw;
  return o;
}

float4 StampPs(StampVertex v) : SV_TARGET
{
  float halfWidth = v.shape.x;
  float amount;
  if (v.shape.z > 0.5)
  {
    // A shoe's sole: an oval, narrower at the heel (the back 40 %), soft over
    // its last 15 %
    float along = v.local.x / v.shape.y;
    float width = halfWidth * lerp(0.72, 1.0, smoothstep(-0.55, 0.05, along));
    float r = length(float2(along, v.local.y / width));
    amount = 1.0 - smoothstep(0.85, 1.0, r);
  }
  else
  {
    // A wheel: its tyre's width round the path, soft over its edge's 30 %
    float t = clamp(v.local.x, 0.0, v.shape.y);
    float off = length(float2(v.local.x - t, v.local.y));
    amount = 1.0 - smoothstep(halfWidth * 0.7, halfWidth, off);
  }
  float red = amount * v.shape.w * g_Map.z, green = amount * v.more.x * g_Map.w;
  // Only where it leaves a mark (its height overwrites what was there: its
  // faint fringe, or a step that presses nothing, would move another's mark
  // to its own height; the weather session, 2026-11-01)
  clip(max(red, green) - 0.02);
  return float4(red, green, 0.0, v.more.y);
}

// The fill: a triangle over the whole map, its value taken from what's there
float4 FillVs(uint id : SV_VertexID) : SV_POSITION
{
  return float4(id == 2 ? 3.0 : -1.0, id == 1 ? 3.0 : -1.0, 0.0, 1.0);
}

float4 FillPs(float4 position : SV_POSITION) : SV_TARGET
{
  return float4(g_Fill.x, g_Fill.y, 0.0, 0.0);
}
