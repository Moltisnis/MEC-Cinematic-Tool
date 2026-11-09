// The lens distortion (VISUALS > Filmic effects > Lens distortion), drawn over
// the finished frame at Present (see LensDistortion.cpp). Compiled into
// LensDistortionVs.h and LensDistortionPs.h with the Windows SDK's fxc:
//   fxc /nologo /O3 /T vs_4_0 /E VsMain /Vn g_LensDistortionVs /Fh LensDistortionVs.h LensDistortion.hlsl
//   fxc /nologo /O3 /T ps_4_0 /E PsMain /Vn g_LensDistortionPs /Fh LensDistortionPs.h LensDistortion.hlsl
//
// The game's own, from its tonemap pixel shader (systems/shaderprogramdb, the
// permutations that use filmicLensDistortParams; 2026-10-26): each pixel is
// looked up at t f + 0.5, t its place from the middle (uv - 0.5, no aspect),
// f = (1 + r2 gain) z, or (1 + r2 (gain + cubic sqrt(r2))) w when the cubic
// gain isn't 0, r2 = dot(t, t). z and w are worked out on the CPU (see
// LensDistortion.cpp).

cbuffer LensParams : register(b0)
{
  // x: the gain, y: the cubic gain, z: the gain's scale, w: the cubic's scale
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

float4 PsMain(VsOut i) : SV_TARGET
{
  float2 t = i.Uv - 0.5;
  float r2 = dot(t, t);
  float f;
  if (abs(g_Params.y) < 0.0001)
    f = (1.0 + r2 * g_Params.x) * g_Params.z;
  else
    f = (1.0 + r2 * (g_Params.x + g_Params.y * sqrt(r2))) * g_Params.w;
  return float4(g_Frame.SampleLevel(g_Sampler, t * f + 0.5, 0).rgb, 1.0);
}
