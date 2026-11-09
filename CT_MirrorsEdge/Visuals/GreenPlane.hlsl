// The green screen (VISUALS > Green screen): a flat rectangle of one color
// standing in the world, drawn over the game's finished frame at Present (see
// GreenPlane.cpp). Compiled into GreenPlaneVs.h and GreenPlanePs.h with the
// Windows SDK's fxc:
//   fxc /nologo /O3 /T vs_4_0 /E VsMain /Vn g_GreenPlaneVs /Fh GreenPlaneVs.h GreenPlane.hlsl
//   fxc /nologo /O3 /T ps_4_0 /E PsMain /Vn g_GreenPlanePs /Fh GreenPlanePs.h GreenPlane.hlsl
//
// The rectangle is projected with the game's own view and projection, and
// each of its pixels is drawn only where it's nearer than what the game drew
// there (a copy of the game's depth): whatever stands in front of it stays.
// Or only where the game drew the sky: everything else stays, wherever it is.
// And the whole sky in its color, the same way.

cbuffer GreenPlaneParams : register(b0)
{
  row_major float4x4 g_ViewProjection;   // world to the game's clip space (row vectors, as the game's)
  float4 g_Corners[4];                   // world, xyz
  // rgb; w: 0 the plane, hiding what's behind it; 1 the plane over the sky
  // only; 2 the whole sky (one triangle over the whole screen)
  float4 g_Color;
  // x, y: the depth copy's size over the frame's; z: 1 when the game's depth
  // is reversed (nearer = bigger), 0 when it isn't; w: the depth 100 km away
  // (anything farther is the sky)
  float4 g_Depth;
};

Texture2D<float> g_SceneDepth : register(t0);

float4 VsMain(uint id : SV_VertexID) : SV_POSITION
{
  if (g_Color.w > 1.5)
    return float4(id == 2 ? 3.0 : -1.0, id == 1 ? 3.0 : -1.0, 0.0, 1.0);
  // Two triangles: 0 1 2, 2 1 3
  static const uint corner[6] = { 0, 1, 2, 2, 1, 3 };
  return mul(float4(g_Corners[corner[id]].xyz, 1.0), g_ViewProjection);
}

float4 PsMain(float4 position : SV_POSITION) : SV_TARGET
{
  int2 texel = int2(position.xy * g_Depth.xy);
  float scene = g_SceneDepth.Load(int3(texel, 0));
  bool reversed = g_Depth.z > 0.5;
  if (g_Color.w > 0.5)
  {
    // Only the sky: wherever the game drew nothing (or something farther
    // than 100 km), whatever the plane's own distance
    bool sky = reversed ? scene <= g_Depth.w : scene >= g_Depth.w;
    if (!sky)
      discard;
  }
  else
  {
    bool nearer = reversed ? position.z >= scene : position.z <= scene;
    if (!nearer)
      discard;
  }
  return float4(g_Color.rgb, 1.0);
}
