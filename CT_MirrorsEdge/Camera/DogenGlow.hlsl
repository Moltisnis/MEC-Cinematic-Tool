// Dogen's kitchen lamps' glow (DogenGlow.cpp): each lamp panel's face drawn into the game's G-buffer target 3
// (R11G11B10_FLOAT), added to what's there, as the game's own emissive G-buffer shaders write theirs: o3 =
// (light map irradiance + EmissiveColor x EmissiveIntensity) x exposureMultipliers.x (sp_maincity shaderdb, offset
// 42633456). Placed with the game's own camera of this frame: its vertex shaders take a world position less
// cameraPos (viewConstants register 45) through crViewProjMatrix (registers 14 - 17, one row each), offset 6247752.
//
// fxc /T vs_4_0 /E VsMain /Vn g_DogenGlowVs /Fh DogenGlowVs.h DogenGlow.hlsl
// fxc /T ps_4_0 /E PsMain /Vn g_DogenGlowPs /Fh DogenGlowPs.h DogenGlow.hlsl

cbuffer GlowConstants : register(b0)
{
  float4 g_Corners[24];   // 4 a lamp: its face's corners (world), the 2nd and 3rd opposite
  float4 g_Glow;          // rgb: the glow (colour x luminance), before the exposure
};

// A copy of the game's viewConstants (its shaders' b2), taken as the G-buffer pass ended
cbuffer ViewConstants : register(b1)
{
  float4 g_View[70];
};

Texture2D<float> g_Depth : register(t0);   // the G-buffer pass's depth

struct Vertex
{
  float4 Position : SV_Position;
  float Reversed : TEXCOORD0;   // 1: nearer is a bigger depth
};

static const uint Corner[6] = { 0, 1, 2, 2, 1, 3 };

float4 Project(float3 relative)
{
  float4 p = float4(relative, 1.f);
  return float4(dot(p, g_View[14]), dot(p, g_View[15]), dot(p, g_View[16]), dot(p, g_View[17]));
}

Vertex VsMain(uint id : SV_VertexID)
{
  float3 relative = g_Corners[(id / 6) * 4 + Corner[id % 6]].xyz - g_View[45].xyz;
  Vertex o;
  o.Position = Project(relative);
  // Which way the game's depth runs: the same direction twice as far
  float4 farther = Project(relative * 2.f);
  o.Reversed = farther.z / farther.w < o.Position.z / o.Position.w ? 1.f : 0.f;
  return o;
}

float4 PsMain(Vertex i) : SV_Target
{
  // Only where the face is nearer than what the G-buffer holds (it lies 3 mm under the panel's own face)
  float scene = g_Depth.Load(int3(i.Position.xy, 0));
  bool seen = i.Reversed > 0.5f ? i.Position.z >= scene : i.Position.z <= scene;
  if (!seen)
    discard;
  return float4(g_Glow.rgb * g_View[49].x, 0.f);
}
