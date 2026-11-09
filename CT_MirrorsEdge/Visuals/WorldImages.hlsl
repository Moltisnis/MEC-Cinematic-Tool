// VISUALS > Images: a picture (JPG / PNG) standing in the world, drawn over
// the game's finished frame at Present (see WorldImages.cpp). Compiled into
// WorldImagesVs.h and WorldImagesPs.h with the Windows SDK's fxc:
//   fxc /nologo /O3 /T vs_4_0 /E VsMain /Vn g_WorldImagesVs /Fh WorldImagesVs.h WorldImages.hlsl
//   fxc /nologo /O3 /T ps_4_0 /E PsMain /Vn g_WorldImagesPs /Fh WorldImagesPs.h WorldImages.hlsl
//
// The rectangle is projected with the game's own view and projection, and
// each of its pixels is drawn only where it's nearer than what the game drew
// there (a copy of the game's depth), as the green screen does: whatever
// stands in front of it stays in front.

cbuffer WorldImageParams : register(b0)
{
  row_major float4x4 g_ViewProjection;   // world to the game's clip space (row vectors, as the game's)
  // world xyz: 0 bottom left, 1 bottom right, 2 top left, 3 top right (as
  // the picture is seen from its front)
  float4 g_Corners[4];
  // x: how see-through (1 solid), y: brightness, z: 1 to mirror it left to
  // right, w: unused
  float4 g_Look;
  // x, y: the depth copy's size over the frame's; z: unused; w: how far the
  // picture may be behind the game's surface and still be drawn (metres, so
  // one put flat on a wall doesn't flicker with it)
  float4 g_Depth;
  // The game's projection's [2][2], [2][3], [3][2], [3][3]: a depth back to
  // the distance along the view
  float4 g_Projection;
};

Texture2D<float> g_SceneDepth : register(t0);
Texture2D<float4> g_Image : register(t1);
SamplerState g_Sampler : register(s0);

struct VsOut
{
  float4 Position : SV_POSITION;
  float2 Uv : TEXCOORD0;
};

VsOut VsMain(uint id : SV_VertexID)
{
  // Two triangles: 0 1 2, 2 1 3
  static const uint corner[6] = { 0, 1, 2, 2, 1, 3 };
  uint c = corner[id];
  VsOut o;
  o.Position = mul(float4(g_Corners[c].xyz, 1.0), g_ViewProjection);
  float u = (c & 1) ? 1.0 : 0.0;
  o.Uv = float2(g_Look.z > 0.5 ? 1.0 - u : u, (c & 2) ? 0.0 : 1.0);
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
  int2 texel = int2(i.Position.xy * g_Depth.xy);
  float scene = g_SceneDepth.Load(int3(texel, 0));
  if (Distance(i.Position.z) > Distance(scene) + g_Depth.w)
    discard;
  float4 color = g_Image.Sample(g_Sampler, i.Uv);
  color.rgb *= g_Look.y;
  color.a *= g_Look.x;
  if (color.a <= 0.002)
    discard;
  return color;
}
