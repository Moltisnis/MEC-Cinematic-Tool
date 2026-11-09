// The replay's own drawing of a recorded model the game doesn't have any more
// (a mission's doors and gates once the mission is over; see
// ReplayShapes.cpp), over the game's finished frame at Present. Compiled into
// ReplayShapesVs.h and ReplayShapesPs.h with the Windows SDK's fxc:
//   fxc /nologo /O3 /T vs_4_0 /E VsMain /Vn g_ReplayShapesVs /Fh ReplayShapesVs.h ReplayShapes.hlsl
//   fxc /nologo /O3 /T ps_4_0 /E PsMain /Vn g_ReplayShapesPs /Fh ReplayShapesPs.h ReplayShapes.hlsl
//
// The model's vertices are its own mesh's, as the game had them (their
// position and first texture coordinates), placed with its recorded
// transform and projected with the game's own view and projection. Each pixel
// is drawn only where it's nearer than what the game drew there (a copy of
// the game's depth, as VISUALS > Images and the green screen do), and nearer
// than the model's own nearer parts (the replay's own depth target). Its
// colour is its colour texture as the game drew it with (kept while it
// recorded; an sRGB view, so it's read in linear light and written back in the
// frame's gamma), or one plain colour where none was kept; lit from a fixed
// direction by each face's own facing (worked out from the pixel's
// neighbours), both sides alike.

cbuffer ShapeParams : register(b0)
{
  row_major float4x4 g_ViewProjection;   // world to the game's clip space (row vectors, as the game's)
  row_major float4x4 g_World;            // the model's transform (its rows: right, up, forward, place)
  // x, y: the depth copy's size over the frame's; z: unused; w: how far the
  // model may be behind the game's surface and still be drawn (metres;
  // negative since 2026-11-03: how far in front of it it must be)
  float4 g_Depth;
  // The game's projection's [2][2], [2][3], [3][2], [3][3]: a depth back to
  // the distance along the view
  float4 g_Projection;
  float4 g_Light;                        // xyz: towards the light (world), w: the light from all round
  float4 g_Color;                        // rgb: the plain colour (the frame's gamma); a: 1 to use the colour texture
};

Texture2D<float> g_SceneDepth : register(t0);
Texture2D<float4> g_ColorMap : register(t1);
SamplerState g_Sampler : register(s0);

struct VsIn
{
  float4 Position : POSITION;   // xyz (a half3 is read with whatever follows it as w)
  float2 Uv : TEXCOORD0;        // its first texture coordinates (anything when it has none: then unused)
};

struct VsOut
{
  float4 Position : SV_POSITION;
  float3 World : TEXCOORD0;
  float2 Uv : TEXCOORD1;
};

VsOut VsMain(VsIn i)
{
  VsOut o;
  float4 world = mul(float4(i.Position.xyz, 1.0), g_World);
  o.World = world.xyz;
  o.Position = mul(float4(world.xyz, 1.0), g_ViewProjection);
  o.Uv = i.Uv;
  return o;
}

// A depth's distance along the view (the depth is z / w of the point
// (0, 0, -distance) through the projection)
float Distance(float depth)
{
  float below = g_Projection.x - depth * g_Projection.y;
  float above = g_Projection.z - depth * g_Projection.w;
  return below > 1e-12 ? above / below : 1e30;
}

float4 PsMain(VsOut i) : SV_TARGET
{
  // The face's facing, before anything is discarded
  float3 facing = cross(ddx(i.World), ddy(i.World));
  float len = length(facing);
  float3 n = len > 1e-12 ? facing / len : float3(0.0, 1.0, 0.0);
  float3 albedo = g_ColorMap.Sample(g_Sampler, i.Uv).rgb;
  int2 texel = int2(i.Position.xy * g_Depth.xy);
  float scene = g_SceneDepth.Load(int3(texel, 0));
  if (Distance(i.Position.z) > Distance(scene) + g_Depth.w)
    discard;
  float lit = g_Light.w + (1.0 - g_Light.w) * abs(dot(n, g_Light.xyz));
  if (g_Color.a > 0.5)
    return float4(pow(saturate(albedo * lit), 1.0 / 2.2), 1.0);
  return float4(g_Color.rgb * lit, 1.0);
}
