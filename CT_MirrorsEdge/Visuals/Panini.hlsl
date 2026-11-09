// The Panini projection (CAMERA > Camera moves > Panini projection), drawn
// over the game's finished frame at Present (see Panini.cpp). Compiled into
// PaniniVs.h and PaniniPs.h with the Windows SDK's fxc:
//   fxc /nologo /O3 /T vs_4_0 /E VsMain /Vn g_PaniniVs /Fh PaniniVs.h Panini.hlsl
//   fxc /nologo /O3 /T ps_4_0 /E PsMain /Vn g_PaniniPs /Fh PaniniPs.h Panini.hlsl
//
// The frame is a normal (rectilinear) view; each pixel of the Panini view is
// looked up in it. The Panini view of a direction at horizontal angle phi is
// S sin(phi), S tan(theta) with S = (1 + d) / (d + cos(phi)) and theta its
// angle up from the cylinder (d = 0: the normal view; d = 1: the classic
// Panini). Panini() below goes back from a Panini view position to where it
// is in the normal view (the same maths as Unity HDRP's PaniniProjection).

cbuffer PaniniParams : register(b0)
{
  // x, y: the normal view's extent (tan of half the field of view, x times the
  // aspect); z: the distance d; w: the scale (below 1 crops to fill the screen)
  float4 g_Params;
};

Texture2D g_Frame : register(t0);
SamplerState g_Sampler : register(s0);

struct VsOut
{
  float4 Position : SV_POSITION;
  float2 Uv : TEXCOORD0;
};

// One triangle over the whole screen, from the vertex id alone
VsOut VsMain(uint id : SV_VertexID)
{
  VsOut o;
  float2 uv = float2((id << 1) & 2, id & 2);
  o.Uv = uv;
  o.Position = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
  return o;
}

float2 Panini(float2 viewPos, float d)
{
  float viewDist = 1.0 + d;
  float viewHypSq = viewPos.x * viewPos.x + viewDist * viewDist;
  float isectD = viewPos.x * d;
  float isectDiscrim = viewHypSq - isectD * isectD;
  float cylDistMinusD = (-isectD * viewPos.x + viewDist * sqrt(isectDiscrim)) / viewHypSq;
  float cylDist = cylDistMinusD + d;
  float2 cylPos = viewPos * (cylDist / viewDist);
  return cylPos / cylDistMinusD;
}

float4 PsMain(VsOut i) : SV_TARGET
{
  float2 viewPos = (2.0 * i.Uv - 1.0) * g_Params.xy * g_Params.w;
  float2 normalPos = Panini(viewPos, g_Params.z);
  float2 uv = normalPos / g_Params.xy * 0.5 + 0.5;
  if (any(uv < 0.0) || any(uv > 1.0))
    return float4(0.0, 0.0, 0.0, 1.0);
  return float4(g_Frame.SampleLevel(g_Sampler, uv, 0).rgb, 1.0);
}
