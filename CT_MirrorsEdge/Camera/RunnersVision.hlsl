// The game's runner's vision layer, mixed into the finished frame by the replay (2026-11-08; ReplayGameDraws.cpp,
// ApplyRunnersVision). The Dash's start, checkpoint arrows and finish are drawn by the game only into its runner's
// vision mask (one R8 target with the scene's depth; PamStaticModelEntityData.PrimaryInRVLayer), which its tonemap
// pass mixes into the picture in red. The replay draws the recorded markers' own kept draws into a mask of its own
// and mixes it in here the way the game's tonemap does. Every one of the game's 1792 tonemap pixel shaders that read
// runnersVisionAlphaMaskTexture (systems/shaderprogramdb, read with fxc /dumpbin) ends with:
//   c    the tonemapped colour, gamma encoded (what the tonemap writes), m the mask
//   a    = c + (1 - c) 0.4
//   a    = a + preBlendAmount (rv - a)
//   b    = 1 - (1 - a) / rv + rv postAddAmount
//   t    = (max(b.r + 0.4 rv.r, m), b.g + 0.4 rv.g, b.b + 0.4 rv.b)
//   out  = c + m (t - c)
// rv = max(runnersVisionColor, 0.001); the game sets runnersVisionColor, preBlendAmount and postAddAmount from its
// visual environment's RunnersVisionComponentData (OverlayColor, AlphaBlendAmount, AdditiveBlendAmount). Some of
// the game's shaders add a little noise after it; the frame's alpha isn't the tonemap's here, and is kept.
// Compiled into RunnersVisionVs.h and RunnersVisionPs.h with the Windows SDK's fxc:
//   fxc /nologo /O3 /T vs_4_0 /E VsMain /Vn g_RunnersVisionVs /Fh RunnersVisionVs.h RunnersVision.hlsl
//   fxc /nologo /O3 /T ps_4_0 /E PsMain /Vn g_RunnersVisionPs /Fh RunnersVisionPs.h RunnersVision.hlsl

cbuffer RunnersVisionParams : register(b0)
{
  float4 g_Color;     // rgb: runnersVisionColor (OverlayColor)
  float4 g_Amounts;   // x: preBlendAmount (AlphaBlendAmount), y: postAddAmount (AdditiveBlendAmount),
                      // z: 1 when the frame's views are sRGB ones (read and written linear)
  float4 g_MaskScale; // xy: the mask's size over the frame's (the game's render size: 1 at a resolution scale of 100 %)
};

Texture2D g_Frame : register(t0);          // a copy of the finished frame
Texture2D<float> g_Mask : register(t1);    // the replay's mask, the game's render size (the frame's at 100 %)

struct VsOut
{
  float4 Position : SV_POSITION;
};

// One triangle over the whole screen, from the vertex id alone
VsOut VsMain(uint id : SV_VertexID)
{
  VsOut o;
  float2 uv = float2((id << 1) & 2, id & 2);
  o.Position = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
  return o;
}

// The game's own sRGB encoding (its tonemap's: 1.055 x^(1/2.4) - 0.055, x 12.92 at or below 0.003131)
float3 Encode(float3 x)
{
  float3 curve = 1.055 * pow(abs(x), 1.0 / 2.4) - 0.055;
  return float3(x.r <= 0.003131 ? x.r * 12.92 : curve.r, x.g <= 0.003131 ? x.g * 12.92 : curve.g,
    x.b <= 0.003131 ? x.b * 12.92 : curve.b);
}

float3 Decode(float3 x)
{
  float3 curve = pow((x + 0.055) / 1.055, 2.4);
  return float3(x.r <= 0.04045 ? x.r / 12.92 : curve.r, x.g <= 0.04045 ? x.g / 12.92 : curve.g,
    x.b <= 0.04045 ? x.b / 12.92 : curve.b);
}

float4 PsMain(VsOut i) : SV_TARGET
{
  int3 at = int3(int2(i.Position.xy), 0);
  float4 frame = g_Frame.Load(at);
  // (the mask's texel the pixel's centre falls in: the pixel itself at 100 %)
  float m = g_Mask.Load(int3(int2(i.Position.xy * g_MaskScale.xy), 0));
  // Nothing of the layer here: the frame as it was, exactly
  if (m <= 0.0)
    return frame;
  bool srgbViews = g_Amounts.z > 0.5;
  float3 c = srgbViews ? Encode(saturate(frame.rgb)) : frame.rgb;
  float3 rv = max(g_Color.rgb, 0.001);
  float3 a = c + (1.0 - c) * 0.4;
  a = a + g_Amounts.x * (rv - a);
  float3 b = 1.0 - (1.0 - a) / rv;
  b = b + rv * g_Amounts.y;
  float3 t = float3(max(b.r + rv.r * 0.4, m), b.g + rv.g * 0.4, b.b + rv.b * 0.4);
  // Clamped as the game's target is when it's written
  float3 o = saturate(c + m * (t - c));
  return float4(srgbViews ? Decode(o) : o, frame.a);
}
