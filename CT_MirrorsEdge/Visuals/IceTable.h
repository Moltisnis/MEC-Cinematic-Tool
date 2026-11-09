#pragma once

// VISUALS > Snow > Freeze: the city's water drawn as ice while it snows
// (2026-10-26, the user: "frozen water should have a froze shader like in
// real life", "make it find any instance of water", "the shader overextends
// here", then "these waterfalls are still running and the frozen water has
// some weird alpha issues"). Generated offline from the game's data (the snow
// session's scratchpad: tools/icewater.py, waterparts.py;
// Research_Snow_2026-10-26.md sections 12 - 15): every mesh drawn with one of
// the game's water shaders, placed in the city's zones (StaticModelGroup
// members, objects in layers), through each instance's transform (scale
// included). Drawn as they are, every frame (WetLook.cpp's ice pass):
// - IcePlanes: a mesh that's all water and a rectangle (a stream down a
//   wall, waterplane_01's pools, a running-water wall, The View's splash
//   pools): its bounding box's two long sides.
// - IceTriangles: the water sections inside bigger meshes (the ground's
//   canals and harbour, the plazas' pools, the station's, the far sea), the
//   Anchor's triangular pool and the fountain features' curved falls, as
//   their own triangles, so the ice is where the water is. Foot: the lowest
//   point of that instance's water (a fall's icicles hang there).
// Left out: waterplane_01's interior and Kingdom variations (indoors), the
// game's interior meshes (Kruger's office, the Kingdom) and the puddle decals.
namespace icetable
{
  enum Kind : unsigned char { Stream = 0, Wall = 1, Pool = 2 };
  // A, B: half sides (a stream's across and up; a pool's along its two
  // sides); Tolerance: how far the frame's surface may be in front of it
  // and still be that water
  struct IcePlaneData { float Centre[3]; float A[3]; float B[3]; float Tolerance; Kind Type; };
  const IcePlaneData IcePlanes[] = {
    { { 335.18f, 76.75f, 385.27f }, { 0.125f, 0.000f, -0.628f }, { 0.000f, 5.260f, 0.000f }, 0.00f, Stream },   // waterstream_straight_800_128 props 394
    { { 335.43f, 76.75f, 384.03f }, { 0.125f, 0.000f, -0.628f }, { 0.000f, 5.260f, 0.000f }, 0.00f, Stream },   // waterstream_straight_800_128 props 394
    { { 205.55f, 82.11f, 277.30f }, { -0.532f, 0.000f, -0.356f }, { 0.000f, 5.260f, 0.000f }, 0.00f, Stream },   // waterstream_straight_800_128 props 397
    { { 206.60f, 82.11f, 278.00f }, { -0.532f, 0.000f, -0.356f }, { 0.000f, 5.260f, 0.000f }, 0.00f, Stream },   // waterstream_straight_800_128 props 397
    { { 209.20f, 82.11f, 279.74f }, { -0.532f, 0.000f, -0.356f }, { 0.000f, 5.260f, 0.000f }, 0.00f, Stream },   // waterstream_straight_800_128 props 397
    { { 210.29f, 82.11f, 280.46f }, { -0.532f, 0.000f, -0.356f }, { 0.000f, 5.260f, 0.000f }, 0.00f, Stream },   // waterstream_straight_800_128 props 397
    { { 216.02f, 82.11f, 284.29f }, { -0.532f, 0.000f, -0.356f }, { 0.000f, 5.260f, 0.000f }, 0.00f, Stream },   // waterstream_straight_800_128 props 397
    { { 217.10f, 82.11f, 285.02f }, { -0.532f, 0.000f, -0.356f }, { 0.000f, 5.260f, 0.000f }, 0.00f, Stream },   // waterstream_straight_800_128 props 397
    { { 219.19f, 82.11f, 286.41f }, { -0.532f, 0.000f, -0.356f }, { 0.000f, 5.260f, 0.000f }, 0.00f, Stream },   // waterstream_straight_800_128 props 397
    { { 220.20f, 82.11f, 287.09f }, { -0.532f, 0.000f, -0.356f }, { 0.000f, 5.260f, 0.000f }, 0.00f, Stream },   // waterstream_straight_800_128 props 397
    { { 300.75f, 66.77f, 259.64f }, { 0.591f, 0.000f, 0.245f }, { 0.000f, 5.260f, 0.000f }, 0.00f, Stream },   // waterstream_straight_800_128 props 401
    { { 301.92f, 66.77f, 260.13f }, { 0.591f, 0.000f, 0.245f }, { 0.000f, 5.260f, 0.000f }, 0.00f, Stream },   // waterstream_straight_800_128 props 401
    { { 306.42f, 66.77f, 261.94f }, { 0.591f, 0.000f, 0.245f }, { 0.000f, 5.260f, 0.000f }, 0.00f, Stream },   // waterstream_straight_800_128 props 401
    { { 307.59f, 66.77f, 262.43f }, { 0.591f, 0.000f, 0.245f }, { 0.000f, 5.260f, 0.000f }, 0.00f, Stream },   // waterstream_straight_800_128 props 401
    { { 312.02f, 66.77f, 264.22f }, { 0.591f, 0.000f, 0.245f }, { 0.000f, 5.260f, 0.000f }, 0.00f, Stream },   // waterstream_straight_800_128 props 401
    { { 313.19f, 66.77f, 264.71f }, { 0.591f, 0.000f, 0.245f }, { 0.000f, 5.260f, 0.000f }, 0.00f, Stream },   // waterstream_straight_800_128 props 401
    { { 317.58f, 66.77f, 266.56f }, { 0.591f, 0.000f, 0.245f }, { 0.000f, 5.260f, 0.000f }, 0.00f, Stream },   // waterstream_straight_800_128 props 401
    { { 318.74f, 66.77f, 267.05f }, { 0.591f, 0.000f, 0.245f }, { 0.000f, 5.260f, 0.000f }, 0.00f, Stream },   // waterstream_straight_800_128 props 401
    { { 566.80f, 81.16f, 11.79f }, { -0.245f, 0.000f, -0.591f }, { 0.000f, 2.804f, 0.000f }, 0.00f, Stream },   // waterstream_straight_300_128 props 430
    { { 575.15f, 81.16f, 8.34f }, { -0.245f, 0.000f, -0.591f }, { 0.000f, 2.804f, 0.000f }, 0.00f, Stream },   // waterstream_straight_300_128 props 430
    { { 500.68f, 61.33f, 108.86f }, { 0.289f, 0.000f, 0.571f }, { 0.000f, 2.804f, 0.000f }, 0.00f, Stream },   // waterstream_straight_300_128 props 431
    { { 501.81f, 61.40f, 104.71f }, { 2.357f, 0.000f, -1.000f }, { 0.000f, 3.016f, 0.000f }, 0.00f, Stream },   // waterstream_straight_600_1024 props 431
    { { 506.07f, 59.34f, 94.93f }, { 0.250f, 0.000f, 0.589f }, { 0.000f, 1.565f, 0.000f }, 0.00f, Stream },   // waterstream_short_01 props 431
    { { 507.87f, 59.34f, 99.86f }, { 0.250f, 0.000f, 0.589f }, { 0.000f, 1.565f, 0.000f }, 0.00f, Stream },   // waterstream_short_01 props 431
    { { 508.37f, 59.34f, 101.04f }, { 0.250f, 0.000f, 0.589f }, { 0.000f, 1.565f, 0.000f }, 0.00f, Stream },   // waterstream_short_01 props 431
    { { 509.94f, 59.34f, 104.75f }, { 0.250f, 0.000f, 0.589f }, { 0.000f, 1.565f, 0.000f }, 0.00f, Stream },   // waterstream_short_01 props 431
    { { 510.44f, 59.34f, 105.93f }, { 0.250f, 0.000f, 0.589f }, { 0.000f, 1.565f, 0.000f }, 0.00f, Stream },   // waterstream_short_01 props 431
    { { 511.83f, 59.34f, 109.57f }, { 0.245f, 0.000f, 0.591f }, { 0.000f, 1.565f, 0.000f }, 0.00f, Stream },   // waterstream_short_01 props 431
    { { 512.31f, 59.34f, 110.74f }, { 0.245f, 0.000f, 0.591f }, { 0.000f, 1.565f, 0.000f }, 0.00f, Stream },   // waterstream_short_01 props 431
    { { 514.19f, 59.34f, 115.29f }, { 0.245f, 0.000f, 0.591f }, { 0.000f, 1.565f, 0.000f }, 0.00f, Stream },   // waterstream_short_01 props 431
    { { 514.68f, 59.34f, 116.45f }, { 0.245f, 0.000f, 0.591f }, { 0.000f, 1.565f, 0.000f }, 0.00f, Stream },   // waterstream_short_01 props 431
    { { 524.57f, 62.43f, 105.58f }, { 0.245f, 0.000f, 0.591f }, { 0.000f, 1.497f, 0.000f }, 0.00f, Stream },   // waterstream_straight_300_128 props 431
    { { 525.07f, 62.48f, 106.79f }, { 0.245f, 0.000f, 0.591f }, { 0.000f, 1.497f, 0.000f }, 0.00f, Stream },   // waterstream_straight_300_128 props 431
    { { 525.71f, 62.48f, 108.33f }, { 0.245f, 0.000f, 0.591f }, { 0.000f, 1.497f, 0.000f }, 0.00f, Stream },   // waterstream_straight_300_128 props 431
    { { 526.21f, 62.48f, 109.54f }, { 0.245f, 0.000f, 0.591f }, { 0.000f, 1.497f, 0.000f }, 0.00f, Stream },   // waterstream_straight_300_128 props 431
    { { 527.01f, 62.48f, 111.47f }, { 0.245f, 0.000f, 0.591f }, { 0.000f, 1.497f, 0.000f }, 0.00f, Stream },   // waterstream_straight_300_128 props 431
    { { 527.49f, 62.48f, 112.64f }, { 0.245f, 0.000f, 0.591f }, { 0.000f, 1.497f, 0.000f }, 0.00f, Stream },   // waterstream_straight_300_128 props 431
    { { 263.37f, 73.24f, 324.27f }, { 0.591f, 0.000f, 0.245f }, { 0.000f, 1.497f, 0.000f }, 0.00f, Stream },   // waterstream_straight_300_128 props 486
    { { 264.53f, 73.24f, 324.75f }, { 0.591f, 0.000f, 0.245f }, { 0.000f, 1.497f, 0.000f }, 0.00f, Stream },   // waterstream_straight_300_128 props 486
    { { 268.34f, 73.24f, 326.33f }, { 0.591f, 0.000f, 0.245f }, { 0.000f, 1.497f, 0.000f }, 0.00f, Stream },   // waterstream_straight_300_128 props 486
    { { 269.54f, 73.24f, 326.83f }, { 0.591f, 0.000f, 0.245f }, { 0.000f, 1.497f, 0.000f }, 0.00f, Stream },   // waterstream_straight_300_128 props 486
    { { 308.29f, 76.73f, 368.24f }, { -0.125f, 0.000f, 0.628f }, { 0.000f, 5.260f, 0.000f }, 0.00f, Stream },   // waterstream_straight_800_128 props 486
    { { 308.53f, 76.73f, 367.01f }, { -0.125f, 0.000f, 0.628f }, { 0.000f, 5.260f, 0.000f }, 0.00f, Stream },   // waterstream_straight_800_128 props 486
    { { 309.56f, 76.73f, 361.82f }, { -0.125f, 0.000f, 0.628f }, { 0.000f, 5.260f, 0.000f }, 0.00f, Stream },   // waterstream_straight_800_128 props 486
    { { 309.81f, 76.73f, 360.59f }, { -0.125f, 0.000f, 0.628f }, { 0.000f, 5.260f, 0.000f }, 0.00f, Stream },   // waterstream_straight_800_128 props 486
    { { 312.84f, 76.74f, 345.32f }, { -0.125f, 0.000f, 0.628f }, { 0.000f, 5.260f, 0.000f }, 0.00f, Stream },   // waterstream_straight_800_128 props 486
    { { 313.09f, 76.74f, 344.07f }, { -0.125f, 0.000f, 0.628f }, { 0.000f, 5.260f, 0.000f }, 0.00f, Stream },   // waterstream_straight_800_128 props 486
    { { 313.83f, 76.74f, 340.34f }, { -0.125f, 0.000f, 0.628f }, { 0.000f, 5.260f, 0.000f }, 0.00f, Stream },   // waterstream_straight_800_128 props 486
    { { 314.08f, 76.74f, 339.10f }, { -0.125f, 0.000f, 0.628f }, { 0.000f, 5.260f, 0.000f }, 0.00f, Stream },   // waterstream_straight_800_128 props 486
    { { 336.47f, 76.75f, 378.79f }, { 0.125f, 0.000f, -0.628f }, { 0.000f, 5.260f, 0.000f }, 0.00f, Stream },   // waterstream_straight_800_128 props 486
    { { 336.72f, 76.75f, 377.53f }, { 0.125f, 0.000f, -0.628f }, { 0.000f, 5.260f, 0.000f }, 0.00f, Stream },   // waterstream_straight_800_128 props 486
    { { 337.68f, 76.75f, 372.70f }, { 0.125f, 0.000f, -0.628f }, { 0.000f, 5.260f, 0.000f }, 0.00f, Stream },   // waterstream_straight_800_128 props 486
    { { 337.92f, 76.75f, 371.47f }, { 0.125f, 0.000f, -0.628f }, { 0.000f, 5.260f, 0.000f }, 0.00f, Stream },   // waterstream_straight_800_128 props 486
    { { 338.53f, 76.75f, 368.44f }, { 0.125f, 0.000f, -0.628f }, { 0.000f, 5.260f, 0.000f }, 0.00f, Stream },   // waterstream_straight_800_128 props 486
    { { 338.77f, 76.75f, 367.22f }, { 0.125f, 0.000f, -0.628f }, { 0.000f, 5.260f, 0.000f }, 0.00f, Stream },   // waterstream_straight_800_128 props 486
    { { 342.08f, 76.72f, 350.60f }, { 0.125f, 0.000f, -0.628f }, { 0.000f, 5.260f, 0.000f }, 0.00f, Stream },   // waterstream_straight_800_128 props 486
    { { 342.32f, 76.72f, 349.36f }, { 0.125f, 0.000f, -0.628f }, { 0.000f, 5.260f, 0.000f }, 0.00f, Stream },   // waterstream_straight_800_128 props 486
    { { 343.06f, 76.72f, 345.64f }, { 0.125f, 0.000f, -0.628f }, { 0.000f, 5.260f, 0.000f }, 0.00f, Stream },   // waterstream_straight_800_128 props 486
    { { 343.31f, 76.72f, 344.38f }, { 0.125f, 0.000f, -0.628f }, { 0.000f, 5.260f, 0.000f }, 0.00f, Stream },   // waterstream_straight_800_128 props 486
    { { 323.88f, 66.77f, 269.18f }, { 0.591f, 0.000f, 0.245f }, { 0.000f, 5.260f, 0.000f }, 0.00f, Stream },   // waterstream_straight_800_128 props 708
    { { 325.04f, 66.77f, 269.66f }, { 0.591f, 0.000f, 0.245f }, { 0.000f, 5.260f, 0.000f }, 0.00f, Stream },   // waterstream_straight_800_128 props 708
    { { 412.00f, 77.44f, 453.43f }, { 0.614f, 0.000f, 0.181f }, { 0.000f, 2.572f, 0.000f }, 0.00f, Stream },   // waterstream_straight_500_128 props 738
    { { 414.95f, 77.41f, 454.09f }, { 0.614f, 0.000f, 0.180f }, { 0.000f, 2.572f, 0.000f }, 0.00f, Stream },   // waterstream_straight_500_128 props 738
    { { 417.47f, 77.41f, 454.83f }, { 0.614f, 0.000f, 0.180f }, { 0.000f, 2.572f, 0.000f }, 0.00f, Stream },   // waterstream_straight_500_128 props 738
    { { 751.48f, 77.41f, -66.45f }, { 2.836f, 0.000f, -3.455f }, { 0.000f, 4.240f, 0.000f }, 0.00f, Wall },   // anc_runningwaterwall_01 props 444
    { { 902.67f, 73.16f, -11.08f }, { -4.936f, 0.000f, 0.486f }, { 0.000f, 3.640f, 0.000f }, 0.00f, Wall },   // anc_runningwaterwall_01 props 448
    { { 910.84f, 72.52f, 0.24f }, { -0.366f, 0.000f, -3.716f }, { 0.000f, 2.347f, 0.000f }, 0.00f, Wall },   // anc_runningwaterwall_01 props 448
    { { 923.15f, 82.80f, -77.48f }, { -0.276f, 0.000f, -2.802f }, { 0.000f, 4.561f, 0.000f }, 0.00f, Wall },   // anc_runningwaterwall_01 props 61
    { { 930.99f, 82.81f, -79.58f }, { 7.641f, 0.000f, -0.753f }, { 0.000f, 4.806f, 0.000f }, 0.00f, Wall },   // anc_runningwaterwall_01 props 61
    { { 931.25f, 82.81f, -76.99f }, { -7.641f, 0.000f, 0.753f }, { 0.000f, 4.806f, 0.000f }, 0.00f, Wall },   // anc_runningwaterwall_01 props 61
    { { 939.11f, 82.68f, -79.06f }, { 0.307f, 0.000f, 3.120f }, { 0.000f, 4.679f, 0.000f }, 0.00f, Wall },   // anc_runningwaterwall_01 props 61
    { { 1204.15f, 57.03f, -136.18f }, { 6.044f, 0.000f, 9.045f }, { 0.000f, 2.400f, 0.000f }, 0.00f, Wall },   // anc_runningwaterwall_01 props 655
    { { 839.31f, -1.28f, 256.96f }, { 114.474f, 0.000f, -4.313f }, { 8.205f, 0.000f, 217.764f }, 0.00f, Pool },   // waterplane_01 ground 11
    { { 1093.76f, 63.82f, -29.20f }, { 1.639f, 0.000f, 1.345f }, { -3.400f, 0.000f, 4.143f }, 0.00f, Pool },   // waterplane_01 (theview) props 134
    { { 1128.73f, 79.32f, -0.35f }, { 4.680f, 0.000f, 3.882f }, { -2.452f, 0.000f, 2.955f }, 0.00f, Pool },   // waterplane_01 (poolwater) props 134
    { { 1129.37f, 68.96f, -17.16f }, { -2.564f, 0.000f, 3.091f }, { -3.177f, 0.000f, -2.635f }, 0.00f, Pool },   // waterplane_01 (theview) props 134
    { { 1146.05f, 74.09f, -4.07f }, { 7.859f, 0.000f, 6.519f }, { -1.839f, 0.000f, 2.217f }, 0.00f, Pool },   // waterplane_01 (theview) props 134
    { { 200.72f, 38.33f, 369.25f }, { 2.481f, 0.000f, 0.629f }, { -0.851f, 0.000f, 3.354f }, 0.00f, Pool },   // waterplane_01 (anchor) props 343
    { { 353.13f, 71.40f, 348.66f }, { -2.860f, 0.000f, -0.569f }, { 1.821f, 0.000f, -9.156f }, 0.00f, Pool },   // waterplane_01 (anchor) props 394
    { { 182.97f, 76.84f, 285.34f }, { -3.805f, 0.000f, -2.542f }, { 1.798f, 0.000f, -2.691f }, 0.00f, Pool },   // waterplane_01 (anchor) props 397
    { { 207.85f, 76.83f, 278.69f }, { -3.276f, 0.000f, -2.189f }, { 0.597f, 0.000f, -0.894f }, 0.00f, Pool },   // waterplane_01 (anchor) props 397
    { { 217.75f, 76.83f, 285.30f }, { -3.276f, 0.000f, -2.189f }, { 0.597f, 0.000f, -0.894f }, 0.00f, Pool },   // waterplane_01 (anchor) props 397
    { { 306.24f, 75.47f, 208.62f }, { 2.324f, 0.000f, 1.553f }, { -2.260f, 0.000f, 3.382f }, 0.00f, Pool },   // waterplane_01 (anchor) props 401
    { { 312.54f, 61.32f, 264.66f }, { 18.650f, 0.000f, 7.725f }, { -0.276f, 0.000f, 0.666f }, 0.00f, Pool },   // waterplane_01 (anchor) props 401
    { { 950.09f, 102.24f, 152.77f }, { -4.809f, 0.000f, 5.860f }, { -0.904f, 0.000f, -0.742f }, 0.00f, Pool },   // waterplane_01 (poolwater) props 417
    { { 1020.34f, 76.62f, 73.17f }, { -3.009f, 0.000f, -2.470f }, { 4.462f, 0.000f, -5.437f }, 0.00f, Pool },   // waterplane_01 (theview) props 419
    { { 1022.51f, 76.62f, 67.60f }, { -1.196f, 0.000f, -0.982f }, { 0.697f, 0.000f, -0.849f }, 0.00f, Pool },   // vw_watersplash_01 props 419
    { { 1025.34f, 76.62f, 69.92f }, { -1.196f, 0.000f, -0.982f }, { 0.697f, 0.000f, -0.849f }, 0.00f, Pool },   // vw_watersplash_01 props 419
    { { 588.03f, 64.94f, 76.04f }, { 4.179f, 0.000f, -1.686f }, { 1.208f, 0.000f, 2.994f }, 0.00f, Pool },   // waterplane_01 (anchor) props 423
    { { 597.32f, 69.17f, 41.78f }, { 4.312f, 0.000f, -1.740f }, { 1.237f, 0.000f, 3.066f }, 0.00f, Pool },   // waterplane_01 (anchor) props 423
    { { 600.82f, 69.17f, 50.44f }, { 4.312f, 0.000f, -1.740f }, { 1.237f, 0.000f, 3.066f }, 0.00f, Pool },   // waterplane_01 (anchor) props 423
    { { 608.45f, 71.53f, 74.66f }, { 9.180f, 0.000f, -3.705f }, { 4.253f, 0.000f, 10.538f }, 0.00f, Pool },   // waterplane_01 (anchor) props 423
    { { 608.50f, 87.08f, 37.21f }, { 4.183f, 0.000f, -1.688f }, { 1.208f, 0.000f, 2.994f }, 0.00f, Pool },   // waterplane_01 (anchor) props 423
    { { 615.39f, 87.08f, 54.29f }, { 4.183f, 0.000f, -1.688f }, { 1.208f, 0.000f, 2.994f }, 0.00f, Pool },   // waterplane_01 (anchor) props 423
    { { 620.93f, 71.53f, 58.57f }, { 2.708f, 0.000f, -1.093f }, { 0.463f, 0.000f, 1.147f }, 0.00f, Pool },   // waterplane_01 (anchor) props 423
    { { 620.98f, 71.53f, 65.07f }, { 4.922f, 0.000f, -1.986f }, { 1.801f, 0.000f, 4.463f }, 0.00f, Pool },   // waterplane_01 (anchor) props 423
    { { 553.77f, 73.33f, -7.13f }, { 4.256f, 0.026f, -1.751f }, { 1.257f, 0.022f, 3.055f }, 0.00f, Pool },   // waterplane_01 (anchor) props 430
    { { 557.00f, 73.38f, 0.89f }, { 4.256f, 0.026f, -1.751f }, { 1.257f, 0.022f, 3.055f }, 0.00f, Pool },   // waterplane_01 (anchor) props 430
    { { 562.36f, 65.19f, 15.88f }, { 1.265f, 0.000f, -0.511f }, { 1.798f, 0.000f, 4.454f }, 0.00f, Pool },   // waterplane_01 (anchor) props 430
    { { 566.63f, 72.88f, 23.20f }, { 0.915f, 0.000f, 2.268f }, { -1.048f, -0.000f, 0.423f }, 0.00f, Pool },   // waterplane_01 (anchor) props 430
    { { 568.73f, 65.50f, 14.92f }, { 4.256f, 0.026f, -1.751f }, { 1.257f, 0.022f, 3.055f }, 0.00f, Pool },   // waterplane_01 (anchor) props 430
    { { 570.20f, 78.33f, 10.85f }, { 9.382f, 0.000f, -3.786f }, { 1.246f, 0.000f, 3.088f }, 0.00f, Pool },   // waterplane_01 (anchor) props 430
    { { 575.51f, 78.34f, 19.04f }, { 7.470f, 0.000f, -3.015f }, { 1.283f, 0.000f, 3.180f }, 0.00f, Pool },   // waterplane_01 (anchor) props 430
    { { 578.87f, 72.88f, 24.30f }, { 3.011f, 0.000f, 7.461f }, { -9.098f, -0.000f, 3.671f }, 0.00f, Pool },   // waterplane_01 (anchor) props 430
    { { 579.69f, 78.33f, 11.86f }, { 1.575f, 0.000f, -0.635f }, { 0.621f, 0.000f, 1.540f }, 0.00f, Pool },   // waterplane_01 (anchor) props 430
    { { 580.93f, 65.21f, 18.04f }, { 12.903f, 0.000f, -5.207f }, { 5.130f, 0.000f, 12.713f }, 0.00f, Pool },   // waterplane_01 (anchor) props 430
    { { 598.21f, 61.38f, 10.99f }, { 4.309f, 0.000f, -1.739f }, { 5.050f, 0.000f, 12.514f }, 0.00f, Pool },   // waterplane_01 (anchor) props 430
    { { 500.55f, 58.32f, 102.23f }, { 7.622f, 0.000f, -4.004f }, { 3.929f, 0.000f, 7.480f }, 0.00f, Pool },   // waterplane_01 (anchor) props 431
    { { 513.79f, 60.88f, 91.76f }, { 7.550f, 0.000f, -3.047f }, { 0.239f, 0.000f, 0.594f }, 0.00f, Pool },   // waterplane_01 (anchor) props 431
    { { 514.68f, 74.28f, 107.30f }, { 3.010f, 0.000f, -1.214f }, { 1.716f, 0.000f, 4.252f }, 0.00f, Pool },   // waterplane_01 (anchor) props 431
    { { 517.59f, 58.96f, 128.71f }, { 2.003f, 0.000f, -0.808f }, { 0.239f, 0.000f, 0.593f }, 0.00f, Pool },   // waterplane_01 (anchor) props 431
    { { 518.71f, 74.28f, 117.27f }, { 3.010f, 0.000f, -1.214f }, { 1.716f, 0.000f, 4.252f }, 0.00f, Pool },   // waterplane_01 (anchor) props 431
    { { 520.33f, 60.84f, 108.36f }, { 7.594f, 0.000f, -3.065f }, { 5.002f, 0.000f, 12.395f }, 0.00f, Pool },   // waterplane_01 (anchor) props 431
    { { 524.74f, 64.77f, 110.05f }, { 3.562f, 0.000f, -1.438f }, { 2.156f, 0.000f, 5.342f }, 0.00f, Pool },   // waterplane_01 (anchor) props 431
    { { 527.17f, 60.88f, 124.87f }, { 7.569f, 0.000f, -3.055f }, { 0.247f, 0.000f, 0.612f }, 0.00f, Pool },   // waterplane_01 (anchor) props 431
    { { 530.96f, 59.77f, 97.41f }, { -4.121f, -0.000f, -10.211f }, { 0.594f, 0.000f, -0.240f }, 0.00f, Pool },   // waterplane_01 (anchor) props 431
    { { 536.44f, 59.77f, 109.08f }, { 5.875f, 0.000f, -2.371f }, { 0.695f, 0.000f, 1.723f }, 0.00f, Pool },   // waterplane_01 (anchor) props 431
    { { 539.96f, 59.77f, 81.21f }, { 16.504f, 0.000f, -6.661f }, { 0.240f, 0.000f, 0.594f }, 0.00f, Pool },   // waterplane_01 (anchor) props 431
    { { 544.30f, 59.77f, 109.92f }, { 1.609f, 0.000f, 3.987f }, { -0.594f, -0.000f, 0.240f }, 0.00f, Pool },   // waterplane_01 (anchor) props 431
    { { 544.69f, 59.88f, 84.67f }, { 5.511f, 0.000f, -2.224f }, { 0.436f, 0.000f, 1.082f }, 0.00f, Pool },   // waterplane_01 (anchor) props 431
    { { 551.74f, 59.77f, 108.85f }, { -0.938f, -0.000f, -2.325f }, { 6.173f, 0.000f, -2.491f }, 0.00f, Pool },   // waterplane_01 (anchor) props 431
    { { 553.32f, 59.77f, 114.32f }, { 16.503f, 0.000f, -6.660f }, { 0.247f, 0.000f, 0.612f }, 0.00f, Pool },   // waterplane_01 (anchor) props 431
    { { 562.53f, 59.77f, 91.34f }, { -6.436f, -0.000f, -15.947f }, { 0.609f, 0.000f, -0.246f }, 0.00f, Pool },   // waterplane_01 (anchor) props 431
    { { 567.78f, 66.58f, 56.99f }, { 3.019f, 0.000f, -1.218f }, { 1.683f, 0.000f, 4.170f }, 0.00f, Pool },   // waterplane_01 (anchor) props 431
    { { 574.30f, 64.94f, 81.59f }, { 4.183f, 0.000f, -1.688f }, { 1.208f, 0.000f, 2.994f }, 0.00f, Pool },   // waterplane_01 (anchor) props 431
    { { 583.99f, 64.82f, 81.98f }, { 0.845f, 0.000f, -0.341f }, { 0.288f, 0.000f, 0.714f }, 0.00f, Pool },   // waterplane_01 (anchor) props 431
    { { 585.60f, 59.03f, 85.96f }, { 0.845f, 0.000f, -0.341f }, { 1.565f, 0.000f, 3.879f }, 0.00f, Pool },   // waterplane_01 (anchor) props 431
    { { 587.86f, 53.36f, 91.80f }, { 0.763f, 0.000f, -0.308f }, { 0.768f, 0.000f, 1.903f }, 0.00f, Pool },   // waterplane_01 (anchor) props 431
    { { 556.61f, 66.56f, 37.90f }, { 3.727f, 0.000f, -2.441f }, { 1.692f, 0.000f, 2.583f }, 0.00f, Pool },   // waterplane_01 (anchor) props 432
    { { 725.14f, 70.65f, -68.14f }, { 0.759f, 0.000f, -0.925f }, { 18.445f, 0.000f, 15.138f }, 0.00f, Pool },   // waterplane_01 (theview) props 444
    { { 761.07f, 73.17f, -59.00f }, { 3.654f, 0.000f, -4.452f }, { 11.379f, 0.000f, 9.338f }, 0.00f, Pool },   // waterplane_01 (theview) props 444
    { { 773.95f, 71.17f, -70.62f }, { 2.360f, 0.000f, -2.876f }, { 2.829f, 0.000f, 2.322f }, 0.00f, Pool },   // waterplane_01 (theview) props 444
    { { 780.98f, 71.23f, -64.85f }, { 2.360f, 0.000f, -2.876f }, { 3.154f, 0.000f, 2.588f }, 0.00f, Pool },   // waterplane_01 (theview) props 444
    { { 803.85f, 64.90f, -116.67f }, { 0.302f, 0.000f, 0.247f }, { -0.778f, 0.000f, 0.948f }, 0.00f, Pool },   // waterplane_01 (theview) props 444
    { { 805.08f, 70.67f, -32.75f }, { -1.021f, 0.000f, -0.838f }, { 0.594f, 0.000f, -0.724f }, 0.00f, Pool },   // vw_watersplash_01 props 446
    { { 806.77f, 70.67f, -34.01f }, { -1.343f, 0.000f, -1.102f }, { 0.788f, 0.000f, -0.960f }, 0.00f, Pool },   // vw_watersplash_01 props 446
    { { 807.38f, 70.62f, -33.09f }, { 3.299f, 0.000f, -4.020f }, { 4.824f, 0.000f, 3.958f }, 0.00f, Pool },   // waterplane_01 (theview) props 446
    { { 845.60f, 78.12f, -0.06f }, { 1.732f, 0.000f, -0.171f }, { 0.514f, 0.000f, 5.215f }, 0.00f, Pool },   // waterplane_01 (theview) props 446
    { { 851.66f, 80.37f, 2.08f }, { 2.807f, 0.000f, -0.276f }, { 1.188f, 0.000f, 12.062f }, 0.00f, Pool },   // waterplane_01 (theview) props 446
    { { 867.67f, 69.48f, -3.08f }, { 2.727f, 0.000f, -0.269f }, { 0.610f, 0.000f, 6.190f }, 0.00f, Pool },   // waterplane_01 (theview) props 447
    { { 880.97f, 69.48f, 11.73f }, { -0.103f, 0.000f, -1.049f }, { 0.745f, 0.000f, -0.073f }, 0.00f, Pool },   // vw_watersplash_01 props 447
    { { 881.13f, 69.48f, 13.30f }, { -0.103f, 0.000f, -1.049f }, { 0.745f, 0.000f, -0.073f }, 0.00f, Pool },   // vw_watersplash_01 props 447
    { { 876.38f, 69.48f, -0.34f }, { 5.832f, 0.000f, -0.574f }, { 1.466f, 0.000f, 14.888f }, 0.00f, Pool },   // waterplane_01 (theview) props 448
    { { 898.96f, 69.54f, -12.15f }, { 1.287f, 0.000f, -0.127f }, { 0.090f, 0.000f, 0.913f }, 0.00f, Pool },   // vw_watersplash_01 props 448
    { { 902.35f, 69.52f, -13.35f }, { 7.981f, 0.000f, -0.786f }, { 0.447f, 0.000f, 4.538f }, 0.00f, Pool },   // waterplane_01 (theview) props 448
    { { 905.97f, 69.54f, -12.84f }, { 1.287f, 0.000f, -0.127f }, { 0.090f, 0.000f, 0.913f }, 0.00f, Pool },   // vw_watersplash_01 props 448
    { { 906.26f, 70.18f, 0.60f }, { 5.235f, 0.000f, -0.516f }, { 0.394f, 0.000f, 4.001f }, 0.00f, Pool },   // waterplane_01 (theview) props 448
    { { 962.14f, 64.99f, 73.16f }, { 0.775f, 0.000f, 0.636f }, { -0.451f, 0.000f, 0.550f }, 0.00f, Pool },   // vw_watersplash_01 props 448
    { { 966.53f, 64.98f, 73.58f }, { 3.083f, 0.000f, -3.757f }, { 9.462f, 0.000f, 7.765f }, 0.00f, Pool },   // waterplane_01 (theview) props 448
    { { 968.09f, 64.99f, 78.05f }, { 0.775f, 0.000f, 0.636f }, { -0.451f, 0.000f, 0.550f }, 0.00f, Pool },   // vw_watersplash_01 props 448
    { { 972.62f, 65.14f, 52.29f }, { 4.200f, 0.000f, -5.117f }, { 2.659f, 0.000f, 2.182f }, 0.00f, Pool },   // waterplane_01 (theview) props 448
    { { 984.28f, 66.42f, 24.71f }, { 4.098f, 0.000f, -4.994f }, { 8.905f, 0.000f, 7.308f }, 0.00f, Pool },   // waterplane_01 (theview) props 448
    { { 1000.01f, 67.35f, 48.45f }, { 1.871f, 0.000f, 1.535f }, { -9.205f, 0.000f, 11.216f }, 0.00f, Pool },   // waterplane_01 (theview) props 448
    { { 420.08f, 69.66f, 72.73f }, { 0.896f, -0.000f, 1.368f }, { -10.039f, 0.000f, 6.576f }, 0.00f, Pool },   // waterplane_01 props 477
    { { 265.62f, 71.53f, 325.93f }, { -1.902f, 0.000f, 4.592f }, { -5.813f, 0.000f, -2.408f }, 0.00f, Pool },   // waterplane_01 (anchor) props 486
    { { 299.86f, 71.70f, 352.60f }, { -6.096f, 0.000f, -1.213f }, { 1.966f, 0.000f, -9.881f }, 0.00f, Pool },   // waterplane_01 (anchor) props 486
    { { 309.06f, 71.39f, 363.81f }, { -1.084f, 0.000f, -0.216f }, { 1.211f, 0.000f, -6.089f }, 0.00f, Pool },   // waterplane_01 (anchor) props 486
    { { 310.01f, 66.43f, 305.15f }, { -4.706f, 0.000f, -0.936f }, { 0.499f, 0.000f, -2.511f }, 0.00f, Pool },   // waterplane_01 (anchor) props 486
    { { 313.44f, 71.39f, 341.81f }, { -1.084f, 0.000f, -0.216f }, { 0.945f, 0.000f, -4.748f }, 0.00f, Pool },   // waterplane_01 (anchor) props 486
    { { 336.84f, 71.44f, 376.45f }, { -2.366f, 0.000f, -0.471f }, { 2.400f, 0.000f, -12.067f }, 0.00f, Pool },   // waterplane_01 (anchor) props 486
    { { 342.69f, 71.39f, 347.49f }, { -1.044f, 0.000f, -0.208f }, { 0.999f, 0.000f, -5.021f }, 0.00f, Pool },   // waterplane_01 (anchor) props 486
    { { -339.24f, -46.27f, 776.20f }, { -1.607f, 0.000f, 1.799f }, { -0.129f, -0.000f, -0.115f }, 0.00f, Pool },   // waterplane_01 (omnistat) props 595
    { { -339.01f, -46.29f, 779.89f }, { -2.123f, -0.000f, -1.910f }, { 0.119f, -0.000f, -0.133f }, 0.00f, Pool },   // waterplane_01 (omnistat) props 595
    { { -335.18f, -46.28f, 780.10f }, { -1.719f, 0.000f, 1.911f }, { -0.117f, -0.000f, -0.105f }, 0.00f, Pool },   // waterplane_01 (omnistat) props 595
    { { 915.84f, 77.68f, -76.79f }, { 7.409f, 0.000f, -0.730f }, { 0.321f, 0.000f, 3.259f }, 0.00f, Pool },   // waterplane_01 (theview) props 61
    { { 922.05f, 78.40f, -77.40f }, { 1.199f, 0.000f, -0.118f }, { 0.290f, 0.000f, 2.941f }, 0.00f, Pool },   // waterplane_01 (theview) props 61
    { { 930.82f, 78.00f, -82.09f }, { 7.986f, 0.000f, -0.787f }, { 0.256f, 0.000f, 2.602f }, 0.00f, Pool },   // waterplane_01 (theview) props 61
    { { 931.56f, 78.00f, -74.47f }, { 7.996f, 0.000f, -0.788f }, { 0.258f, 0.000f, 2.622f }, 0.00f, Pool },   // waterplane_01 (theview) props 61
    { { 940.80f, 77.70f, -81.84f }, { 1.301f, 0.000f, -0.128f }, { 0.091f, 0.000f, 0.923f }, 0.00f, Pool },   // vw_watersplash_01 props 61
    { { 941.31f, 77.70f, -76.68f }, { 1.301f, 0.000f, -0.128f }, { 0.091f, 0.000f, 0.923f }, 0.00f, Pool },   // vw_watersplash_01 props 61
    { { 946.23f, 77.68f, -79.78f }, { 7.409f, 0.000f, -0.730f }, { 0.321f, 0.000f, 3.259f }, 0.00f, Pool },   // waterplane_01 (theview) props 61
    { { 951.56f, 77.70f, -82.90f }, { 1.301f, 0.000f, -0.128f }, { 0.091f, 0.000f, 0.923f }, 0.00f, Pool },   // vw_watersplash_01 props 61
    { { 952.07f, 77.70f, -77.74f }, { 1.301f, 0.000f, -0.128f }, { 0.091f, 0.000f, 0.923f }, 0.00f, Pool },   // vw_watersplash_01 props 61
    { { 1155.59f, 47.03f, -165.68f }, { 0.601f, 0.000f, 0.900f }, { -0.607f, 0.000f, 0.406f }, 0.00f, Pool },   // vw_watersplash_01 props 655
    { { 1156.90f, 47.03f, -171.20f }, { 0.900f, 0.000f, -0.601f }, { 0.406f, 0.000f, 0.607f }, 0.00f, Pool },   // vw_watersplash_01 props 655
    { { 1161.18f, 47.03f, -164.79f }, { 0.900f, 0.000f, -0.601f }, { 0.406f, 0.000f, 0.607f }, 0.00f, Pool },   // vw_watersplash_01 props 655
    { { 1162.52f, 47.03f, -170.30f }, { 0.601f, 0.000f, 0.900f }, { -0.607f, 0.000f, 0.406f }, 0.00f, Pool },   // vw_watersplash_01 props 655
    { { 1168.51f, 47.02f, -163.22f }, { 8.304f, 0.000f, 12.428f }, { -8.373f, 0.000f, 5.595f }, 0.00f, Pool },   // waterplane_01 (theview) props 655
    { { 1174.95f, 47.03f, -153.96f }, { 0.601f, 0.000f, 0.900f }, { -0.607f, 0.000f, 0.406f }, 0.00f, Pool },   // vw_watersplash_01 props 655
    { { 1196.16f, 52.12f, -172.26f }, { 11.193f, 0.000f, 16.751f }, { -6.186f, 0.000f, 4.133f }, 0.00f, Pool },   // waterplane_01 (theview) props 655
    { { 1196.46f, 51.94f, -130.81f }, { 7.926f, 0.000f, 11.862f }, { -6.402f, 0.000f, 4.278f }, 0.00f, Pool },   // waterplane_01 (theview) props 655
    { { 1205.02f, 54.53f, -148.36f }, { 0.684f, 0.000f, 1.053f }, { -0.710f, 0.000f, 0.461f }, 0.00f, Pool },   // vw_watersplash_01 props 655
    { { 1210.57f, 54.53f, -141.73f }, { 7.193f, 0.000f, 10.765f }, { -7.932f, 0.000f, 5.300f }, 0.00f, Pool },   // waterplane_01 (theview) props 655
    { { 1210.93f, 57.11f, -117.58f }, { -7.838f, 0.000f, 5.237f }, { -1.544f, 0.000f, -2.311f }, 0.00f, Pool },   // waterplane_01 (theview) props 655
    { { 1211.23f, 68.74f, -30.52f }, { -0.490f, -0.000f, 1.156f }, { -0.780f, -0.000f, -0.330f }, 0.00f, Pool },   // vw_watersplash_01 props 655
    { { 1213.85f, 68.74f, -36.69f }, { -0.490f, -0.000f, 1.156f }, { -0.780f, -0.000f, -0.330f }, 0.00f, Pool },   // vw_watersplash_01 props 655
    { { 1215.55f, 54.53f, -132.61f }, { 0.684f, 0.000f, 1.053f }, { -0.710f, 0.000f, 0.461f }, 0.00f, Pool },   // vw_watersplash_01 props 655
    { { 1217.85f, 68.73f, -31.74f }, { -6.042f, 0.000f, -2.503f }, { 1.898f, -0.000f, -4.582f }, 0.00f, Pool },   // waterplane_01 (theview) props 655
    { { 1221.74f, 68.74f, -26.22f }, { -0.479f, -0.000f, 1.161f }, { -0.783f, -0.000f, -0.323f }, 0.00f, Pool },   // vw_watersplash_01 props 655
    { { 1222.35f, 64.91f, -65.81f }, { -2.568f, 0.000f, -1.064f }, { 2.541f, -0.000f, -6.135f }, 0.00f, Pool },   // waterplane_01 (theview) props 655
    { { 1224.30f, 68.74f, -32.44f }, { -0.479f, -0.000f, 1.161f }, { -0.783f, -0.000f, -0.323f }, 0.00f, Pool },   // vw_watersplash_01 props 655
    { { 1225.53f, 67.38f, -49.97f }, { -3.653f, 0.000f, -1.513f }, { 2.363f, -0.000f, -5.706f }, 0.00f, Pool },   // waterplane_01 (theview) props 655
    { { 1231.62f, 64.55f, -66.94f }, { 2.796f, 0.000f, -6.750f }, { 2.458f, -0.000f, 1.018f }, 0.00f, Pool },   // waterplane_01 (theview) props 655
    { { 1239.60f, 67.44f, -64.19f }, { -3.603f, 0.000f, -1.492f }, { 1.018f, -0.000f, -2.458f }, 0.00f, Pool },   // waterplane_01 (theview) props 655
    { { 1223.82f, 54.28f, -300.53f }, { 0.200f, -0.000f, -1.240f }, { 0.836f, -0.000f, 0.135f }, 0.00f, Pool },   // vw_watersplash_01 props 658
    { { 1224.57f, 54.28f, -307.47f }, { 1.687f, 0.000f, -8.739f }, { 1.159f, 0.000f, 0.224f }, 0.00f, Pool },   // waterplane_01 (theview) props 658
    { { 1226.42f, 54.28f, -314.01f }, { 0.200f, -0.000f, -1.240f }, { 0.836f, -0.000f, 0.135f }, 0.00f, Pool },   // vw_watersplash_01 props 658
    { { 1149.01f, 53.60f, -248.15f }, { -1.303f, 0.000f, -0.252f }, { 0.178f, 0.000f, -0.924f }, 0.00f, Pool },   // vw_watersplash_01 props 659
    { { 1150.98f, 53.56f, -259.21f }, { 3.472f, 0.000f, -17.986f }, { 10.499f, 0.000f, 2.027f }, 0.00f, Pool },   // waterplane_01 (theview) props 659
    { { 1152.70f, 53.60f, -247.43f }, { -1.303f, 0.000f, -0.252f }, { 0.178f, 0.000f, -0.924f }, 0.00f, Pool },   // vw_watersplash_01 props 659
    { { 1153.81f, 58.56f, -306.15f }, { 1.421f, 0.000f, -7.359f }, { 4.217f, 0.000f, 0.814f }, 0.00f, Pool },   // waterplane_01 (theview) props 659
    { { 1156.51f, 53.61f, -246.68f }, { -1.303f, 0.000f, -0.252f }, { 0.178f, 0.000f, -0.924f }, 0.00f, Pool },   // vw_watersplash_01 props 659
    { { 1158.35f, 58.58f, -314.85f }, { 0.350f, 0.000f, -1.811f }, { 1.679f, 0.000f, 0.324f }, 0.00f, Pool },   // vw_watersplash_01 props 659
    { { 1172.06f, 58.56f, -322.15f }, { 2.213f, 0.000f, -11.463f }, { 18.828f, 0.000f, 3.635f }, 0.00f, Pool },   // waterplane_01 (theview) props 659
    { { 986.35f, 71.57f, -72.20f }, { 0.925f, 0.000f, 9.390f }, { -2.120f, 0.000f, 0.209f }, 0.00f, Pool },   // waterplane_01 (theview) props 664
    { { 991.84f, 71.57f, -73.53f }, { 0.461f, 0.000f, 4.678f }, { -2.408f, 0.000f, 0.237f }, 0.00f, Pool },   // waterplane_01 (theview) props 664
    { { 998.82f, 70.88f, -63.27f }, { 0.397f, 0.000f, 4.034f }, { -6.522f, 0.000f, 0.642f }, 0.00f, Pool },   // waterplane_01 (theview) props 664
    { { 874.57f, 214.71f, -439.80f }, { -1.630f, 0.000f, 1.819f }, { -0.866f, 0.000f, -0.776f }, 0.00f, Pool },   // waterplane_01 (theview) props 689
    { { 884.34f, 212.81f, -433.41f }, { -1.608f, 0.000f, 1.795f }, { -2.621f, 0.000f, -2.348f }, 0.00f, Pool },   // waterplane_01 (theview) props 689
    { { 899.59f, 23.75f, -469.21f }, { 3.402f, -0.000f, -4.645f }, { 6.195f, 0.000f, 4.537f }, 0.00f, Pool },   // waterplane_01 (theview) props 689
    { { 778.60f, 40.83f, -288.24f }, { 0.950f, 0.000f, 1.456f }, { -4.773f, 0.000f, 3.116f }, 0.00f, Pool },   // waterplane_01 (theview) props 691
    { { 807.44f, 38.27f, -307.10f }, { 0.918f, 0.000f, 1.407f }, { -6.548f, 0.000f, 4.274f }, 0.00f, Pool },   // waterplane_01 (theview) props 691
    { { 823.10f, 36.98f, -320.40f }, { 3.302f, 0.000f, 5.058f }, { -7.704f, 0.000f, 5.029f }, 0.00f, Pool },   // waterplane_01 (theview) props 691
    { { 850.05f, 40.33f, -335.31f }, { 0.787f, 0.000f, 1.206f }, { -5.476f, 0.000f, 3.575f }, 0.00f, Pool },   // waterplane_01 (theview) props 691
    { { 850.05f, 40.82f, -344.08f }, { 0.787f, 0.000f, 1.206f }, { -7.620f, 0.000f, 4.974f }, 0.00f, Pool },   // waterplane_01 (theview) props 691
    { { 875.90f, 36.97f, -354.87f }, { 3.302f, 0.000f, 5.058f }, { -9.713f, 0.000f, 6.341f }, 0.00f, Pool },   // waterplane_01 (theview) props 691
    { { 891.38f, 38.27f, -370.61f }, { 0.787f, 0.000f, 1.206f }, { -5.376f, 0.000f, 3.509f }, 0.00f, Pool },   // waterplane_01 (theview) props 691
    { { 903.81f, 40.80f, -370.00f }, { 0.787f, 0.000f, 1.206f }, { -8.441f, 0.000f, 5.510f }, 0.00f, Pool },   // waterplane_01 (theview) props 691
    { { 922.76f, 38.25f, -386.67f }, { 2.886f, 0.000f, 4.421f }, { -3.145f, 0.000f, 2.053f }, 0.00f, Pool },   // waterplane_01 (theview) props 691
    { { 205.07f, 89.64f, 477.37f }, { 1.019f, 0.000f, -4.070f }, { 4.705f, 0.000f, 1.178f }, 0.00f, Pool },   // waterplane_01 (anchor) props 699
    { { 212.63f, 89.64f, 476.41f }, { 0.346f, 0.000f, -1.381f }, { 2.178f, 0.000f, 0.546f }, 0.00f, Pool },   // waterplane_01 (anchor) props 699
    { { 337.17f, 76.45f, 163.15f }, { -9.311f, 0.000f, -6.221f }, { 4.110f, 0.000f, -6.151f }, 0.00f, Pool },   // waterplane_01 (anchor) props 711
    { { 346.86f, 77.33f, 149.51f }, { -2.455f, 0.000f, -3.675f }, { 1.018f, 0.000f, -0.680f }, 0.00f, Pool },   // waterplane_01 (anchor) props 711
    { { 348.60f, 69.72f, 146.70f }, { -1.686f, 0.000f, -2.523f }, { 1.858f, 0.000f, -1.241f }, 0.00f, Pool },   // waterplane_01 (anchor) props 711
    { { 720.37f, 71.00f, -148.71f }, { 3.587f, 0.000f, -4.136f }, { 13.055f, 0.000f, 11.323f }, 0.00f, Pool },   // waterplane_01 (theview) props 731
    { { 412.25f, 29.44f, 483.23f }, { -0.909f, 0.000f, -2.928f }, { 3.897f, 0.000f, -1.210f }, 0.00f, Pool },   // waterplane_01 props 738
    { { 414.59f, 74.90f, 453.96f }, { -4.309f, 0.000f, -1.277f }, { 0.917f, 0.000f, -3.096f }, 0.00f, Pool },   // waterplane_01 (anchor) props 738
    { { 422.47f, 29.44f, 480.06f }, { -0.909f, 0.000f, -2.928f }, { 3.897f, 0.000f, -1.210f }, 0.00f, Pool },   // waterplane_01 props 738
    { { 320.37f, 71.07f, 424.06f }, { 0.291f, 0.000f, 0.088f }, { -2.693f, -0.000f, 8.879f }, 0.00f, Pool },   // waterplane_01 (theview) props 740
    { { 354.10f, 37.90f, 473.73f }, { 1.444f, 0.000f, -4.758f }, { 3.574f, 0.000f, 1.084f }, 0.00f, Pool },   // waterplane_01 props 740
    { { 354.97f, 23.80f, 444.58f }, { 1.444f, 0.000f, -4.758f }, { 3.574f, 0.000f, 1.084f }, 0.00f, Pool },   // waterplane_01 props 740
    { { 369.96f, 23.80f, 449.13f }, { 1.444f, 0.000f, -4.758f }, { 3.574f, 0.000f, 1.084f }, 0.00f, Pool },   // waterplane_01 props 740
    { { 251.61f, 86.84f, 439.46f }, { 1.119f, 0.000f, -4.469f }, { 3.196f, 0.000f, 0.801f }, 0.00f, Pool },   // waterplane_01 (theview) props 743
    { { 301.61f, 44.91f, 432.31f }, { 4.788f, 0.000f, 1.452f }, { -1.084f, -0.000f, 3.574f }, 0.00f, Pool },   // waterplane_01 props 743
    { { 355.26f, 94.93f, 580.67f }, { 8.436f, 0.000f, 0.831f }, { -0.836f, 0.000f, 8.481f }, 0.00f, Pool },   // waterplane_01 (anchor) props 751
    { { 380.90f, 58.63f, 288.99f }, { -5.004f, 0.000f, -2.073f }, { 1.211f, 0.000f, -2.923f }, 0.00f, Pool },   // waterplane_01 (anchor) props 759
    { { 393.68f, 53.84f, 289.76f }, { -3.010f, 0.000f, -1.247f }, { 1.710f, 0.000f, -4.127f }, 0.00f, Pool },   // waterplane_01 (anchor) props 759
    { { 440.87f, 55.85f, 303.91f }, { -2.719f, 0.000f, -6.171f }, { 8.994f, 0.000f, -3.962f }, 0.00f, Pool },   // waterplane_01 (anchor) props 759
    { { 442.59f, 55.87f, 291.22f }, { -1.528f, 0.000f, -3.470f }, { 3.220f, 0.000f, -1.418f }, 0.00f, Pool },   // waterplane_01 (anchor) props 759
  };

  // A water triangle's corners (world), its instance's foot (y), its tolerance (the far sea's waves)
  struct IceTriangleData { float A[3]; float B[3]; float C[3]; float Foot; float Tolerance; };
  const IceTriangleData IceTriangles[] = {
    { { 5035.63f, -0.95f, -3147.73f }, { 4921.64f, -0.95f, -3427.41f }, { 4878.96f, -0.95f, -3313.38f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 5193.64f, -0.95f, -3134.10f }, { 4921.64f, -0.95f, -3427.41f }, { 5035.63f, -0.95f, -3147.73f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 5020.01f, -0.95f, -3123.06f }, { 5193.64f, -0.95f, -3134.10f }, { 5035.63f, -0.95f, -3147.73f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 5197.93f, -0.95f, -2778.39f }, { 5193.64f, -0.95f, -3134.10f }, { 5020.01f, -0.95f, -3123.06f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 5197.93f, -0.95f, -2778.39f }, { 7950.77f, -0.95f, -3202.42f }, { 5193.64f, -0.95f, -3134.10f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 5193.64f, -0.95f, -3134.10f }, { 7950.77f, -0.95f, -3202.42f }, { 5198.50f, -0.95f, -3203.42f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 5020.01f, -0.95f, -3123.06f }, { 4808.96f, -0.95f, -3133.38f }, { 5197.93f, -0.95f, -2778.39f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 4808.96f, -0.95f, -3133.38f }, { 5020.01f, -0.95f, -3123.06f }, { 4859.67f, -0.95f, -3272.41f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 7950.77f, -0.95f, -3202.42f }, { 5197.93f, -0.95f, -2778.39f }, { 7966.63f, -0.95f, -3104.35f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 8009.94f, -0.95f, -3403.91f }, { 5198.50f, -0.95f, -3203.42f }, { 7950.77f, -0.95f, -3202.42f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 7966.63f, -0.95f, -3104.35f }, { 5197.93f, -0.95f, -2778.39f }, { 8213.43f, -0.95f, -2604.62f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 5114.13f, -0.95f, -1674.81f }, { 8213.43f, -0.95f, -2604.62f }, { 5197.93f, -0.95f, -2778.39f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 5198.50f, -0.95f, -3203.42f }, { 8009.94f, -0.95f, -3403.91f }, { 5383.17f, -0.95f, -4621.63f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 5383.17f, -0.95f, -4621.63f }, { 4947.18f, -0.95f, -3493.41f }, { 5198.50f, -0.95f, -3203.42f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 8769.60f, -0.95f, -5712.47f }, { 5383.17f, -0.95f, -4621.63f }, { 8009.94f, -0.95f, -3403.91f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 5114.13f, -0.95f, -1674.81f }, { 8351.63f, -0.95f, -2159.47f }, { 8213.43f, -0.95f, -2604.62f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 6609.48f, -2.06f, -106.18f }, { 8351.63f, -0.95f, -2159.47f }, { 5114.13f, -0.95f, -1674.81f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 8769.60f, -0.95f, -5712.47f }, { 5208.10f, -0.95f, -4761.69f }, { 5383.17f, -0.95f, -4621.63f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 5208.10f, -0.95f, -4761.69f }, { 8769.60f, -0.95f, -5712.47f }, { 9088.24f, -0.95f, -7793.37f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 5208.10f, -0.95f, -4761.69f }, { 9088.24f, -0.95f, -7793.37f }, { 9043.84f, -0.95f, -10107.47f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 9047.08f, -0.95f, -10180.09f }, { 5208.10f, -0.95f, -4761.69f }, { 9043.84f, -0.95f, -10107.47f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 8306.32f, -0.95f, -1140.11f }, { 8351.63f, -0.95f, -2159.47f }, { 6609.48f, -2.06f, -106.18f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 8351.63f, -0.95f, -2159.47f }, { 8306.32f, -0.95f, -1140.11f }, { 8419.99f, -0.95f, -1640.09f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 8306.32f, -0.95f, -1140.11f }, { 6609.48f, -2.06f, -106.18f }, { 8056.91f, -0.95f, -679.94f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 8056.91f, -0.95f, -679.94f }, { 6609.48f, -2.06f, -106.18f }, { 6687.48f, -2.06f, -59.87f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 8056.91f, -0.95f, -679.94f }, { 6687.48f, -2.06f, -59.87f }, { 6711.62f, -2.06f, -3.54f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 6711.62f, -2.06f, -3.54f }, { 7156.68f, -0.95f, 425.77f }, { 8056.91f, -0.95f, -679.94f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -7937.41f, -0.95f, -10120.19f }, { 5208.10f, -0.95f, -4761.69f }, { 9047.08f, -0.95f, -10180.09f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -8933.49f, -0.95f, -13667.58f }, { -7937.41f, -0.95f, -10120.20f }, { 9047.08f, -0.95f, -10180.09f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 7156.68f, -0.95f, 425.77f }, { 6711.62f, -2.06f, -3.54f }, { 6695.15f, -2.06f, 62.21f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -8997.75f, -0.95f, -12954.24f }, { -7937.41f, -0.95f, -10120.20f }, { -8933.49f, -0.95f, -13667.58f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -7937.41f, -0.95f, -10120.19f }, { -8997.75f, -0.95f, -12954.24f }, { -8490.03f, -0.95f, -10507.90f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 6383.24f, -2.06f, 825.77f }, { 7156.68f, -0.95f, 425.77f }, { 6695.15f, -2.06f, 62.21f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 7156.68f, -0.95f, 425.77f }, { 6383.24f, -2.06f, 825.77f }, { 6451.78f, -0.95f, 1284.55f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 6695.15f, -2.06f, 62.21f }, { 6186.25f, -2.06f, 678.58f }, { 6383.24f, -2.06f, 825.77f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 6451.78f, -0.95f, 1284.55f }, { 6383.24f, -2.06f, 825.77f }, { 6420.60f, -0.95f, 1334.63f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 5883.10f, -2.06f, 1442.87f }, { 6420.60f, -0.95f, 1334.63f }, { 6383.24f, -2.06f, 825.77f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 6420.60f, -0.95f, 1334.63f }, { 5883.10f, -2.06f, 1442.87f }, { 6397.79f, -0.95f, 1400.06f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 5883.10f, -2.06f, 1442.87f }, { 6359.80f, -0.95f, 1464.68f }, { 6397.79f, -0.95f, 1400.06f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 6359.80f, -0.95f, 1464.68f }, { 5883.10f, -2.06f, 1442.87f }, { 5779.52f, -0.95f, 2193.81f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 5579.24f, -2.06f, 1799.77f }, { 5779.52f, -0.95f, 2193.81f }, { 5883.10f, -2.06f, 1442.87f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 5475.24f, -2.06f, 1709.77f }, { 5579.24f, -2.06f, 1799.77f }, { 5883.10f, -2.06f, 1442.87f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 5475.24f, -2.06f, 1709.77f }, { 5883.10f, -2.06f, 1442.87f }, { 5758.24f, -2.06f, 1345.77f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 5550.24f, -2.06f, 1832.77f }, { 5779.52f, -0.95f, 2193.81f }, { 5579.24f, -2.06f, 1799.77f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 5582.80f, -0.95f, 2200.09f }, { 5779.52f, -0.95f, 2193.81f }, { 5550.24f, -2.06f, 1832.77f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 5699.83f, -0.95f, 2229.23f }, { 5779.52f, -0.95f, 2193.81f }, { 5582.80f, -0.95f, 2200.09f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 5779.52f, -0.95f, 2193.81f }, { 5699.83f, -0.95f, 2229.23f }, { 5752.06f, -0.95f, 2217.12f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 5550.24f, -2.06f, 1832.77f }, { 5463.10f, -0.95f, 2146.74f }, { 5582.80f, -0.95f, 2200.09f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 5463.10f, -0.95f, 2146.74f }, { 5550.24f, -2.06f, 1832.77f }, { 5388.55f, -0.95f, 2101.49f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 5388.55f, -0.95f, 2101.49f }, { 5550.24f, -2.06f, 1832.77f }, { 5336.77f, -0.95f, 2051.88f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 5550.24f, -2.06f, 1832.77f }, { 5036.71f, -0.95f, 1734.30f }, { 5336.77f, -0.95f, 2051.88f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 5036.71f, -0.95f, 1734.30f }, { 5550.24f, -2.06f, 1832.77f }, { 5083.24f, -2.06f, 1465.77f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 5036.71f, -0.95f, 1734.30f }, { 5083.24f, -2.06f, 1465.77f }, { 4833.64f, -0.95f, 1599.90f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 4663.23f, -2.06f, 1296.05f }, { 4833.64f, -0.95f, 1599.90f }, { 5083.24f, -2.06f, 1465.77f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 4833.64f, -0.95f, 1599.90f }, { 4663.23f, -2.06f, 1296.05f }, { 4645.51f, -0.95f, 1508.25f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 4663.23f, -2.06f, 1296.05f }, { 4412.92f, -0.95f, 1439.23f }, { 4645.51f, -0.95f, 1508.25f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 4663.23f, -2.06f, 1296.05f }, { 4252.86f, -0.95f, 1411.31f }, { 4412.92f, -0.95f, 1439.23f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 3856.17f, -2.06f, 1110.05f }, { 4252.86f, -0.95f, 1411.31f }, { 4663.23f, -2.06f, 1296.05f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 4252.85f, -0.95f, 1411.31f }, { 3856.17f, -2.06f, 1110.05f }, { 4007.86f, -0.95f, 1441.31f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 4007.86f, -0.95f, 1441.31f }, { 4060.86f, -0.95f, 1480.31f }, { 4252.86f, -0.95f, 1411.31f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 3856.17f, -2.06f, 1110.05f }, { 3752.86f, -0.95f, 1566.31f }, { 4007.86f, -0.95f, 1441.31f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 2821.36f, -0.95f, 1393.31f }, { 3752.86f, -0.95f, 1566.31f }, { 3856.17f, -2.06f, 1110.05f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 3752.86f, -0.95f, 1566.31f }, { 2821.36f, -0.95f, 1393.31f }, { 3597.86f, -0.95f, 1751.31f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 3856.17f, -2.06f, 1110.05f }, { 3084.56f, -0.95f, 502.50f }, { 2821.36f, -0.95f, 1393.31f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 3084.56f, -0.95f, 502.50f }, { 3856.17f, -2.06f, 1110.05f }, { 3596.95f, -0.95f, 530.43f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 3596.95f, -0.95f, 530.43f }, { 3856.17f, -2.06f, 1110.05f }, { 3866.87f, -2.06f, 1073.02f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 3866.87f, -2.06f, 1073.02f }, { 3617.54f, -0.95f, 496.67f }, { 3596.95f, -0.95f, 530.43f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 3084.56f, -0.95f, 502.50f }, { 2715.36f, -0.95f, 1287.31f }, { 2821.36f, -0.95f, 1393.31f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 3597.86f, -0.95f, 1751.31f }, { 2821.36f, -0.95f, 1393.31f }, { 2843.36f, -0.95f, 1509.31f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 2715.36f, -0.95f, 1287.31f }, { 3084.56f, -0.95f, 502.50f }, { 2531.88f, -0.95f, 483.24f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 2865.36f, -0.95f, 1705.31f }, { 3597.86f, -0.95f, 1751.31f }, { 2843.36f, -0.95f, 1509.31f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 3597.86f, -0.95f, 1751.31f }, { 2865.36f, -0.95f, 1705.31f }, { 3382.86f, -0.95f, 1968.31f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 2865.36f, -0.95f, 1705.31f }, { 2843.36f, -0.95f, 1509.31f }, { 2829.36f, -0.95f, 1565.31f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 2715.36f, -0.95f, 1287.31f }, { 2531.88f, -0.95f, 483.24f }, { 2384.17f, -0.95f, 561.24f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 3866.87f, -2.06f, 1073.02f }, { 4503.89f, -2.06f, 1000.82f }, { 3617.54f, -0.95f, 496.67f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 4503.89f, -2.06f, 1000.82f }, { 3866.87f, -2.06f, 1073.02f }, { 4446.94f, -2.06f, 1192.23f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 2847.36f, -0.95f, 1821.31f }, { 3382.86f, -0.95f, 1968.31f }, { 2865.36f, -0.95f, 1705.31f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 3382.86f, -0.95f, 1968.31f }, { 2847.36f, -0.95f, 1821.31f }, { 3216.86f, -0.95f, 2118.31f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 3216.86f, -0.95f, 2118.31f }, { 2847.36f, -0.95f, 1821.31f }, { 3050.86f, -0.95f, 2190.31f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 2747.36f, -0.95f, 1962.31f }, { 3050.86f, -0.95f, 2190.31f }, { 2847.36f, -0.95f, 1821.31f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 2105.19f, -1.00f, 924.52f }, { 2715.36f, -0.95f, 1287.31f }, { 2384.17f, -0.95f, 561.24f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 2105.19f, -1.00f, 924.52f }, { 2384.17f, -0.95f, 561.24f }, { 2144.24f, -0.95f, 686.73f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 3050.86f, -0.95f, 2190.31f }, { 2747.36f, -0.95f, 1962.31f }, { 2952.86f, -0.95f, 2284.31f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 2952.86f, -0.95f, 2284.31f }, { 2747.36f, -0.95f, 1962.31f }, { 2876.86f, -0.95f, 2378.31f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 2496.36f, -0.95f, 2241.31f }, { 2876.86f, -0.95f, 2378.31f }, { 2747.36f, -0.95f, 1962.31f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 2418.36f, -0.95f, 2360.31f }, { 2876.86f, -0.95f, 2378.31f }, { 2496.36f, -0.95f, 2241.31f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 2418.36f, -0.95f, 2360.31f }, { 2402.36f, -0.95f, 2435.31f }, { 2876.86f, -0.95f, 2378.31f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 2402.36f, -0.95f, 2435.31f }, { 2807.86f, -0.95f, 2661.31f }, { 2876.86f, -0.95f, 2378.31f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 2144.24f, -0.95f, 686.73f }, { 2032.80f, -1.00f, 813.64f }, { 2105.19f, -1.00f, 924.52f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 2144.24f, -0.95f, 686.73f }, { 2012.23f, -0.95f, 757.41f }, { 2032.80f, -1.00f, 813.64f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 2807.86f, -0.95f, 2661.31f }, { 2402.36f, -0.95f, 2435.31f }, { 2757.86f, -0.95f, 2796.31f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 2012.23f, -0.95f, 757.41f }, { 1965.44f, -1.00f, 760.90f }, { 2032.80f, -1.00f, 813.64f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1965.44f, -1.00f, 760.90f }, { 2012.23f, -0.95f, 757.41f }, { 1982.90f, -0.95f, 737.10f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 2757.86f, -0.95f, 2796.31f }, { 2402.36f, -0.95f, 2435.31f }, { 2622.11f, -0.95f, 2897.31f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 2375.36f, -0.95f, 2435.31f }, { 2622.11f, -0.95f, 2897.31f }, { 2402.36f, -0.95f, 2435.31f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1908.90f, -0.95f, 682.10f }, { 1965.44f, -1.00f, 760.90f }, { 1982.90f, -0.95f, 737.10f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1873.52f, -1.00f, 695.51f }, { 1965.44f, -1.00f, 760.90f }, { 1908.90f, -0.95f, 682.10f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 2622.11f, -0.95f, 2897.31f }, { 2375.36f, -0.95f, 2435.31f }, { 1885.30f, -0.95f, 3008.29f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 2247.36f, -0.95f, 2407.31f }, { 1885.30f, -0.95f, 3008.29f }, { 2375.36f, -0.95f, 2435.31f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1875.90f, -0.95f, 649.10f }, { 1873.52f, -1.00f, 695.52f }, { 1908.90f, -0.95f, 682.10f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1873.52f, -1.00f, 695.51f }, { 1875.90f, -0.95f, 649.10f }, { 1843.75f, -1.00f, 662.45f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 2093.36f, -0.95f, 2407.31f }, { 1885.30f, -0.95f, 3008.29f }, { 2247.36f, -0.95f, 2407.31f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1859.90f, -0.95f, 616.10f }, { 1843.75f, -1.00f, 662.45f }, { 1875.90f, -0.95f, 649.10f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1843.75f, -1.00f, 662.45f }, { 1859.90f, -0.95f, 616.10f }, { 1824.56f, -1.00f, 604.38f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1154.17f, -1.00f, 2307.24f }, { 1885.30f, -0.95f, 3008.29f }, { 2093.36f, -0.95f, 2407.31f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1885.30f, -0.95f, 3008.29f }, { 1154.17f, -1.00f, 2307.24f }, { 1858.15f, -0.95f, 3025.80f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 2045.36f, -0.95f, 2222.31f }, { 1154.17f, -1.00f, 2307.24f }, { 2093.36f, -0.95f, 2407.31f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1850.90f, -0.95f, 585.35f }, { 1824.56f, -1.00f, 604.38f }, { 1859.90f, -0.95f, 616.10f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1824.56f, -1.00f, 604.38f }, { 1850.90f, -0.95f, 585.35f }, { 1815.91f, -1.00f, 553.41f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1815.91f, -1.00f, 553.41f }, { 1850.90f, -0.95f, 585.35f }, { 1842.90f, -0.95f, 538.60f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1842.90f, -0.95f, 538.60f }, { 1811.15f, -1.00f, 515.28f }, { 1815.91f, -1.00f, 553.41f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1811.15f, -1.00f, 515.28f }, { 1842.90f, -0.95f, 538.60f }, { 1839.90f, -0.95f, 511.10f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1811.15f, -1.00f, 515.28f }, { 1839.90f, -0.95f, 511.10f }, { 1802.44f, -1.00f, 467.70f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1154.17f, -1.00f, 2307.24f }, { 2045.36f, -0.95f, 2222.31f }, { 1246.18f, -1.00f, 2118.56f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1967.36f, -0.95f, 1993.31f }, { 1246.18f, -1.00f, 2118.56f }, { 2045.36f, -0.95f, 2222.31f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1828.90f, -0.95f, 457.85f }, { 1802.44f, -1.00f, 467.70f }, { 1839.90f, -0.95f, 511.10f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1812.90f, -0.95f, 420.85f }, { 1802.44f, -1.00f, 467.70f }, { 1828.90f, -0.95f, 457.85f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1802.44f, -1.00f, 467.70f }, { 1812.90f, -0.95f, 420.85f }, { 1786.12f, -1.00f, 430.91f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1787.90f, -0.95f, 380.10f }, { 1786.12f, -1.00f, 430.91f }, { 1812.90f, -0.95f, 420.85f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1786.12f, -1.00f, 430.91f }, { 1787.90f, -0.95f, 380.10f }, { 1747.56f, -0.95f, 388.17f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1779.90f, -0.95f, 369.60f }, { 1747.56f, -0.95f, 388.17f }, { 1787.90f, -0.95f, 380.10f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1747.56f, -0.95f, 388.17f }, { 1779.90f, -0.95f, 369.60f }, { 1755.90f, -0.95f, 354.60f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1723.90f, -0.95f, 326.85f }, { 1747.56f, -0.95f, 388.17f }, { 1755.90f, -0.95f, 354.60f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1747.56f, -0.95f, 388.17f }, { 1723.90f, -0.95f, 326.85f }, { 1682.69f, -0.95f, 332.08f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1686.90f, -0.95f, 289.22f }, { 1682.69f, -0.95f, 332.08f }, { 1723.90f, -0.95f, 326.85f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1682.69f, -0.95f, 332.08f }, { 1686.90f, -0.95f, 289.22f }, { 1627.35f, -0.95f, 292.91f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1638.90f, -0.95f, 259.35f }, { 1627.35f, -0.95f, 292.91f }, { 1686.90f, -0.95f, 289.22f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1563.90f, -0.95f, 232.72f }, { 1627.35f, -0.95f, 292.91f }, { 1638.90f, -0.95f, 259.35f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1627.35f, -0.95f, 292.91f }, { 1563.90f, -0.95f, 232.72f }, { 1545.20f, -0.95f, 268.48f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1545.20f, -0.95f, 268.48f }, { 1563.90f, -0.95f, 232.72f }, { 1513.90f, -0.95f, 215.60f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1513.90f, -0.95f, 215.60f }, { 1500.37f, -0.95f, 274.45f }, { 1545.20f, -0.95f, 268.48f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1443.91f, -0.95f, 177.95f }, { 1500.37f, -0.95f, 274.45f }, { 1513.90f, -0.95f, 215.60f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1500.37f, -0.95f, 274.45f }, { 1443.91f, -0.95f, 177.95f }, { 1386.39f, -0.95f, 224.18f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1500.37f, -0.95f, 274.45f }, { 1386.39f, -0.95f, 224.18f }, { 1387.94f, -0.95f, 274.33f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1344.07f, -0.95f, 117.48f }, { 1386.39f, -0.95f, 224.18f }, { 1443.91f, -0.95f, 177.95f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1586.86f, -0.95f, 108.31f }, { 1344.07f, -0.95f, 117.48f }, { 1443.91f, -0.95f, 177.95f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1386.39f, -0.95f, 224.18f }, { 1344.07f, -0.95f, 117.48f }, { 1335.62f, -0.95f, 218.93f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1335.62f, -0.95f, 218.93f }, { 1344.07f, -0.95f, 117.48f }, { 1304.83f, -0.95f, 164.97f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1344.07f, -0.95f, 117.48f }, { 1586.86f, -0.95f, 108.31f }, { 1310.61f, -0.95f, -134.87f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1788.88f, -0.95f, -169.04f }, { 1310.61f, -0.95f, -134.87f }, { 1586.86f, -0.95f, 108.31f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1264.70f, -0.95f, -68.89f }, { 1344.07f, -0.95f, 117.48f }, { 1310.61f, -0.95f, -134.87f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1344.07f, -0.95f, 117.48f }, { 1264.70f, -0.95f, -68.89f }, { 1249.23f, -0.95f, 39.16f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1237.89f, -0.95f, -5.70f }, { 1249.23f, -0.95f, 39.16f }, { 1264.70f, -0.95f, -68.89f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1788.88f, -0.95f, -169.04f }, { 1281.58f, -0.95f, -257.78f }, { 1310.61f, -0.95f, -134.87f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1310.61f, -0.95f, -134.87f }, { 1281.58f, -0.95f, -257.78f }, { 1296.88f, -0.95f, -156.15f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1281.59f, -0.95f, -257.78f }, { 1788.88f, -0.95f, -169.04f }, { 1821.57f, -0.95f, -398.14f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1788.88f, -0.95f, -169.04f }, { 1909.03f, -0.95f, -323.22f }, { 1821.57f, -0.95f, -398.14f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1242.17f, -0.95f, 32.32f }, { 1249.23f, -0.95f, 39.16f }, { 1237.89f, -0.95f, -5.70f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1249.23f, -0.95f, 39.16f }, { 1242.17f, -0.95f, 32.32f }, { 1245.51f, -0.95f, 36.04f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1242.17f, -0.95f, 32.32f }, { 1237.89f, -0.95f, -5.70f }, { 1239.25f, -0.95f, 28.02f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1237.89f, -0.95f, -5.70f }, { 1235.43f, -0.95f, 19.11f }, { 1239.25f, -0.95f, 28.02f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1235.43f, -0.95f, 19.11f }, { 1237.89f, -0.95f, -5.70f }, { 1236.00f, -0.95f, -1.21f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1821.57f, -0.95f, -398.14f }, { 1258.35f, -0.95f, -333.35f }, { 1281.58f, -0.95f, -257.78f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1251.53f, -0.95f, -288.85f }, { 1281.58f, -0.95f, -257.78f }, { 1258.35f, -0.95f, -333.35f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1239.26f, -0.95f, 28.02f }, { 1235.43f, -0.95f, 19.11f }, { 1237.00f, -0.95f, 23.67f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1259.41f, -0.95f, -443.92f }, { 1258.35f, -0.95f, -333.35f }, { 1821.57f, -0.95f, -398.14f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1259.41f, -0.95f, -443.92f }, { 1821.57f, -0.95f, -398.14f }, { 1825.88f, -0.95f, -439.04f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1258.35f, -0.95f, -333.35f }, { 1259.41f, -0.95f, -443.92f }, { 1252.93f, -0.95f, -334.28f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1235.43f, -0.95f, 19.11f }, { 1236.00f, -0.95f, -1.21f }, { 1234.14f, -0.95f, 8.68f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1235.43f, -0.95f, 19.11f }, { 1234.14f, -0.95f, 8.68f }, { 1234.31f, -0.95f, 13.81f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1234.14f, -0.95f, 8.68f }, { 1236.00f, -0.95f, -1.21f }, { 1234.60f, -0.95f, 3.65f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1259.41f, -0.95f, -443.92f }, { 1825.88f, -0.95f, -439.04f }, { 1210.14f, -0.95f, -516.57f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1259.41f, -0.95f, -443.92f }, { 1210.14f, -0.95f, -516.57f }, { 1256.26f, -0.95f, -444.50f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1210.14f, -0.95f, -516.57f }, { 1169.60f, -0.95f, -460.49f }, { 1256.26f, -0.95f, -444.50f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1169.61f, -0.95f, -460.49f }, { 1210.14f, -0.95f, -516.57f }, { 1165.99f, -0.95f, -461.17f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1825.88f, -0.95f, -439.04f }, { 1204.10f, -0.95f, -522.80f }, { 1210.14f, -0.95f, -516.57f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 3420.20f, -0.95f, -3126.74f }, { 1204.10f, -0.95f, -522.80f }, { 1825.88f, -0.95f, -439.04f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 2904.92f, -0.95f, -714.03f }, { 3420.20f, -0.95f, -3126.74f }, { 1825.88f, -0.95f, -439.04f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 2904.92f, -0.95f, -714.03f }, { 1825.88f, -0.95f, -439.04f }, { 2146.36f, -0.95f, -424.19f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 2146.36f, -0.95f, -424.19f }, { 2723.62f, -0.95f, -579.33f }, { 2904.92f, -0.95f, -714.03f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 2723.62f, -0.95f, -579.33f }, { 2146.36f, -0.95f, -424.19f }, { 2227.75f, -0.95f, -425.69f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 2249.85f, -0.95f, -426.72f }, { 2723.62f, -0.95f, -579.33f }, { 2227.75f, -0.95f, -425.69f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 2723.62f, -0.95f, -579.33f }, { 2249.85f, -0.95f, -426.72f }, { 2641.56f, -0.95f, -532.04f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 2340.85f, -0.95f, -430.72f }, { 2641.56f, -0.95f, -532.04f }, { 2249.85f, -0.95f, -426.72f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 3420.20f, -0.95f, -3126.74f }, { 2904.92f, -0.95f, -714.03f }, { 3222.92f, -0.95f, -975.88f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 3222.92f, -0.95f, -975.88f }, { 2904.92f, -0.95f, -714.03f }, { 2924.60f, -0.95f, -720.78f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 2641.56f, -0.95f, -532.04f }, { 2340.85f, -0.95f, -430.72f }, { 2592.09f, -0.95f, -500.06f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 3420.20f, -0.95f, -3126.74f }, { 3222.92f, -0.95f, -975.88f }, { 3392.24f, -0.95f, -1146.90f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 2592.09f, -0.95f, -500.06f }, { 2340.85f, -0.95f, -430.72f }, { 2509.72f, -0.95f, -461.26f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 2398.34f, -0.95f, -433.59f }, { 2509.72f, -0.95f, -461.26f }, { 2340.85f, -0.95f, -430.72f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 3585.17f, -0.95f, -1384.96f }, { 3420.20f, -0.95f, -3126.74f }, { 3392.24f, -0.95f, -1146.90f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 2509.72f, -0.95f, -461.25f }, { 2398.34f, -0.95f, -433.59f }, { 2451.43f, -0.95f, -442.07f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 3664.40f, -0.95f, -1510.30f }, { 3420.20f, -0.95f, -3126.74f }, { 3585.17f, -0.95f, -1384.96f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 3420.20f, -0.95f, -3126.74f }, { 3664.40f, -0.95f, -1510.30f }, { 3845.64f, -0.95f, -2106.66f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 3845.64f, -0.95f, -2106.66f }, { 3664.40f, -0.95f, -1510.30f }, { 3811.22f, -0.95f, -1863.38f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 3811.22f, -0.95f, -1863.38f }, { 3664.40f, -0.95f, -1510.30f }, { 3736.95f, -0.95f, -1641.71f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 3420.20f, -0.95f, -3126.74f }, { 3845.64f, -0.95f, -2106.66f }, { 3845.64f, -0.95f, -2366.66f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 3420.20f, -0.95f, -3126.74f }, { 3845.64f, -0.95f, -2366.66f }, { 3812.60f, -0.95f, -2590.41f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 3781.23f, -0.95f, -2762.06f }, { 3420.20f, -0.95f, -3126.74f }, { 3812.60f, -0.95f, -2590.41f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 3224.71f, -0.95f, -3458.06f }, { 1204.10f, -0.95f, -522.80f }, { 3420.20f, -0.95f, -3126.74f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 3383.72f, -0.95f, -3308.74f }, { 3224.71f, -0.95f, -3458.06f }, { 3420.20f, -0.95f, -3126.74f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1204.10f, -0.95f, -522.80f }, { 3224.71f, -0.95f, -3458.06f }, { 298.12f, -0.95f, -876.29f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1204.10f, -0.95f, -522.80f }, { 298.12f, -0.95f, -876.29f }, { 573.73f, -1.02f, -634.01f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 573.73f, -1.02f, -634.01f }, { 1144.70f, -0.95f, -464.48f }, { 1204.10f, -0.95f, -522.80f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 298.12f, -0.95f, -876.29f }, { 3224.71f, -0.95f, -3458.06f }, { -183.34f, -1.00f, -1209.12f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 298.12f, -0.95f, -876.29f }, { -183.34f, -1.00f, -1209.12f }, { 177.65f, -0.95f, -909.60f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 177.65f, -0.95f, -909.60f }, { -183.34f, -1.00f, -1209.12f }, { -224.14f, -0.95f, -1052.69f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -183.34f, -1.00f, -1209.12f }, { 3224.71f, -0.95f, -3458.06f }, { -478.86f, -1.00f, -1407.62f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -259.14f, -0.95f, -870.19f }, { 177.65f, -0.95f, -909.60f }, { -224.14f, -0.95f, -1052.69f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 177.65f, -0.95f, -909.60f }, { -259.14f, -0.95f, -870.19f }, { -85.50f, -0.95f, -534.19f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -478.86f, -1.00f, -1407.62f }, { 3224.71f, -0.95f, -3458.06f }, { -792.33f, -1.00f, -1699.18f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -792.33f, -1.00f, -1699.18f }, { 3224.71f, -0.95f, -3458.06f }, { -1086.35f, -0.95f, -2057.59f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -259.14f, -0.95f, -870.19f }, { -158.66f, -0.95f, -541.65f }, { -85.50f, -0.95f, -534.19f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -158.66f, -0.95f, -541.65f }, { -259.14f, -0.95f, -870.19f }, { -295.14f, -0.95f, -831.69f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -158.66f, -0.95f, -541.65f }, { -148.61f, -0.95f, -538.85f }, { -85.50f, -0.95f, -534.19f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -148.61f, -0.95f, -538.85f }, { -158.66f, -0.95f, -541.65f }, { -153.29f, -0.95f, -539.64f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -158.66f, -0.95f, -541.65f }, { -295.14f, -0.95f, -831.69f }, { -309.42f, -0.95f, -609.04f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -309.42f, -0.95f, -609.04f }, { -295.14f, -0.95f, -831.69f }, { -375.14f, -0.95f, -641.69f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -1086.35f, -0.95f, -2057.59f }, { -4892.14f, -0.95f, -4363.69f }, { -4006.14f, -0.95f, -3462.69f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -1086.35f, -0.95f, -2057.59f }, { -4006.14f, -0.95f, -3462.69f }, { -3997.14f, -0.95f, -3433.69f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -1086.35f, -0.95f, -2057.59f }, { -3997.14f, -0.95f, -3433.69f }, { -4340.95f, -0.95f, -3078.35f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -1086.35f, -0.95f, -2057.59f }, { -4340.95f, -0.95f, -3078.35f }, { -5280.93f, -0.95f, -2745.40f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -5280.93f, -0.95f, -2745.40f }, { -5453.58f, -0.95f, -2669.73f }, { -1086.35f, -0.95f, -2057.59f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -5453.58f, -0.95f, -2669.73f }, { -5127.73f, -0.95f, -871.12f }, { -1086.35f, -0.95f, -2057.59f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -5127.73f, -0.95f, -871.12f }, { -3929.11f, -0.95f, -814.15f }, { -1086.35f, -0.95f, -2057.59f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -1086.35f, -0.95f, -2057.59f }, { -7937.41f, -0.95f, -10120.20f }, { -4892.14f, -0.95f, -4363.69f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 5208.10f, -0.95f, -4761.69f }, { -7937.41f, -0.95f, -10120.20f }, { -1086.35f, -0.95f, -2057.59f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 3224.71f, -0.95f, -3458.06f }, { 5208.10f, -0.95f, -4761.69f }, { -1086.35f, -0.95f, -2057.59f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -3929.11f, -0.95f, -814.15f }, { -1565.86f, -1.00f, -1620.72f }, { -1086.35f, -0.95f, -2057.59f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -1565.86f, -1.00f, -1620.72f }, { -1180.04f, -0.95f, -1932.28f }, { -1086.35f, -0.95f, -2057.59f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -4424.15f, -0.95f, -3094.69f }, { -5280.93f, -0.95f, -2745.40f }, { -4340.95f, -0.95f, -3078.35f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -5453.58f, -0.95f, -2669.73f }, { -5280.93f, -0.95f, -2745.40f }, { -5353.17f, -0.95f, -2721.44f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -5586.32f, -0.95f, -2418.59f }, { -5127.73f, -0.95f, -871.12f }, { -5453.58f, -0.95f, -2669.73f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -5586.32f, -0.95f, -2418.59f }, { -5453.58f, -0.95f, -2669.73f }, { -5537.22f, -0.95f, -2569.09f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -3929.11f, -0.95f, -814.15f }, { -5127.73f, -0.95f, -871.12f }, { -4334.99f, -0.95f, -785.20f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -1565.86f, -1.00f, -1620.72f }, { -3929.11f, -0.95f, -814.15f }, { -3851.89f, -0.95f, -761.60f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -1180.04f, -0.95f, -1932.28f }, { -1565.86f, -1.00f, -1620.72f }, { -1309.39f, -0.95f, -1769.65f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -1565.86f, -1.00f, -1620.72f }, { -3851.89f, -0.95f, -761.60f }, { -1732.59f, -1.00f, -1195.64f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -1309.39f, -0.95f, -1769.65f }, { -1565.86f, -1.00f, -1620.72f }, { -1411.86f, -1.00f, -1688.72f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -1732.59f, -1.00f, -1195.64f }, { -3851.89f, -0.95f, -761.60f }, { -1787.25f, -1.00f, -1024.98f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -5606.59f, -0.95f, -1356.67f }, { -5127.73f, -0.95f, -871.12f }, { -5586.32f, -0.95f, -2418.59f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -5127.73f, -0.95f, -871.12f }, { -4781.30f, -0.95f, -791.36f }, { -4334.99f, -0.95f, -785.20f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -5127.73f, -0.95f, -871.12f }, { -5606.59f, -0.95f, -1356.67f }, { -5338.56f, -0.95f, -953.84f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -5338.56f, -0.95f, -953.84f }, { -5606.59f, -0.95f, -1356.67f }, { -5476.12f, -0.95f, -1051.13f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -5476.12f, -0.95f, -1051.13f }, { -5606.59f, -0.95f, -1356.67f }, { -5559.02f, -0.95f, -1158.00f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -3851.89f, -0.95f, -761.60f }, { -1789.25f, -1.00f, -890.98f }, { -1787.25f, -1.00f, -1024.98f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -1746.91f, -1.00f, -917.63f }, { -1787.25f, -1.00f, -1024.98f }, { -1789.25f, -1.00f, -890.98f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -1977.47f, -1.00f, -70.71f }, { -1789.25f, -1.00f, -890.98f }, { -3851.89f, -0.95f, -761.60f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -1789.25f, -1.00f, -890.98f }, { -1977.47f, -1.00f, -70.71f }, { -1741.57f, -1.00f, -825.00f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -1574.79f, -1.00f, -272.70f }, { -1741.57f, -1.00f, -825.00f }, { -1977.47f, -1.00f, -70.71f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -1977.47f, -1.00f, -70.71f }, { -3851.89f, -0.95f, -761.60f }, { -2897.47f, -1.00f, -114.71f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -1741.57f, -1.00f, -825.00f }, { -1574.79f, -1.00f, -272.70f }, { -1499.12f, -1.00f, -471.04f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -2897.47f, -1.00f, -114.71f }, { -3851.89f, -0.95f, -761.60f }, { -3042.12f, -1.00f, -67.04f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -1741.57f, -1.00f, -825.00f }, { -1499.12f, -1.00f, -471.04f }, { -1476.48f, -1.00f, -586.75f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -1741.57f, -1.00f, -825.00f }, { -1476.48f, -1.00f, -586.75f }, { -1470.42f, -1.00f, -692.07f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -3105.79f, -1.00f, 46.30f }, { -3042.12f, -1.00f, -67.04f }, { -3851.89f, -0.95f, -761.60f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -3851.89f, -0.95f, -761.60f }, { -3973.14f, -0.95f, 62.31f }, { -3105.79f, -1.00f, 46.30f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -3042.12f, -1.00f, -67.04f }, { -3105.79f, -1.00f, 46.30f }, { -3091.47f, -1.00f, 46.29f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -3973.14f, -0.95f, 62.31f }, { -3103.76f, -1.00f, 133.26f }, { -3105.79f, -1.00f, 46.30f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -4046.19f, -0.95f, 409.31f }, { -3103.76f, -1.00f, 133.26f }, { -3973.14f, -0.95f, 62.31f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -3103.76f, -1.00f, 133.26f }, { -4046.19f, -0.95f, 409.31f }, { -3014.78f, -1.00f, 652.62f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -3103.76f, -1.00f, 133.26f }, { -3014.78f, -1.00f, 652.62f }, { -2980.77f, -1.00f, 283.94f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -2980.77f, -1.00f, 283.94f }, { -2985.26f, -1.00f, 142.76f }, { -3103.76f, -1.00f, 133.26f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -3014.78f, -1.00f, 652.62f }, { -2946.78f, -1.00f, 392.62f }, { -2980.77f, -1.00f, 283.94f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -3014.78f, -1.00f, 652.62f }, { -4046.19f, -0.95f, 409.31f }, { -3014.77f, -1.00f, 799.94f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -4157.15f, -0.95f, 1216.31f }, { -3014.77f, -1.00f, 799.94f }, { -4046.19f, -0.95f, 409.31f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -3014.77f, -1.00f, 799.94f }, { -4157.14f, -0.95f, 1216.31f }, { -2924.76f, -1.00f, 1065.26f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -2924.76f, -1.00f, 1065.26f }, { -4157.14f, -0.95f, 1216.31f }, { -4293.27f, -0.95f, 2417.91f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -2924.76f, -1.00f, 1065.26f }, { -4293.27f, -0.95f, 2417.91f }, { -3789.03f, -0.95f, 2773.17f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -3789.03f, -0.95f, 2773.17f }, { -2591.77f, -1.00f, 1232.91f }, { -2924.76f, -1.00f, 1065.26f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -2924.76f, -1.00f, 1065.26f }, { -2591.77f, -1.00f, 1232.91f }, { -2687.10f, -1.00f, 1157.60f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -2591.77f, -1.00f, 1232.91f }, { -3789.03f, -0.95f, 2773.17f }, { -3387.14f, -0.95f, 3056.31f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -2717.14f, -0.95f, 3136.31f }, { -2591.77f, -1.00f, 1232.91f }, { -3387.14f, -0.95f, 3056.31f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -2591.77f, -1.00f, 1232.91f }, { -2717.14f, -0.95f, 3136.31f }, { -1881.43f, -1.00f, 1438.58f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -1881.43f, -1.00f, 1438.58f }, { -2431.43f, -1.00f, 1212.58f }, { -2591.77f, -1.00f, 1232.91f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -2717.14f, -0.95f, 3136.31f }, { -1436.21f, -2.06f, 2831.12f }, { -1881.43f, -1.00f, 1438.58f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -1436.21f, -2.06f, 2831.12f }, { -2717.14f, -0.95f, 3136.31f }, { -1659.64f, -0.95f, 3098.81f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -1436.21f, -2.06f, 2831.12f }, { -1659.64f, -0.95f, 3098.81f }, { -1606.21f, -2.06f, 3061.12f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -1139.64f, -0.95f, 3458.81f }, { -1606.21f, -2.06f, 3061.12f }, { -1659.64f, -0.95f, 3098.81f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -1606.21f, -2.06f, 3061.12f }, { -1139.64f, -0.95f, 3458.81f }, { -1081.21f, -2.06f, 3426.12f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -866.21f, -2.06f, 4176.12f }, { -1081.21f, -2.06f, 3426.12f }, { -1139.64f, -0.95f, 3458.81f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -1139.64f, -0.95f, 3458.81f }, { -899.64f, -0.95f, 4238.81f }, { -866.21f, -2.06f, 4176.12f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -899.64f, -0.95f, 4238.81f }, { 166.71f, -2.06f, 3979.27f }, { -866.21f, -2.06f, 4176.12f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -899.64f, -0.95f, 4238.81f }, { 1138.15f, -0.95f, 3845.80f }, { 166.71f, -2.06f, 3979.27f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1138.15f, -0.95f, 3845.80f }, { 143.33f, -2.06f, 3855.27f }, { 166.71f, -2.06f, 3979.27f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1138.15f, -0.95f, 3845.80f }, { 53.83f, -2.06f, 3511.27f }, { 143.33f, -2.06f, 3855.27f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 143.33f, -2.06f, 3855.27f }, { 53.83f, -2.06f, 3511.27f }, { 3.08f, -2.06f, 3823.27f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 53.83f, -2.06f, 3511.27f }, { 1138.15f, -0.95f, 3845.80f }, { 525.66f, -1.00f, 2702.29f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 525.66f, -1.00f, 2702.29f }, { 1138.15f, -0.95f, 3845.80f }, { 1858.15f, -0.95f, 3025.80f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 53.83f, -2.06f, 3511.27f }, { 525.66f, -1.00f, 2702.29f }, { -91.68f, -2.06f, 3423.27f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 53.83f, -2.06f, 3511.27f }, { -91.68f, -2.06f, 3423.27f }, { 5.02f, -2.06f, 3503.27f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -159.20f, -2.06f, 3797.27f }, { 5.02f, -2.06f, 3503.27f }, { -91.68f, -2.06f, 3423.27f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 525.66f, -1.00f, 2702.29f }, { -141.92f, -2.06f, 3411.27f }, { -91.68f, -2.06f, 3423.27f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 5.02f, -2.06f, 3503.27f }, { -159.20f, -2.06f, 3797.27f }, { -49.17f, -2.06f, 3817.27f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 525.66f, -1.00f, 2702.29f }, { 1858.15f, -0.95f, 3025.80f }, { 539.61f, -1.00f, 2678.97f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -141.92f, -2.06f, 3411.27f }, { 525.66f, -1.00f, 2702.29f }, { -319.54f, -2.06f, 3559.27f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 525.66f, -1.00f, 2702.29f }, { 269.58f, -1.00f, 2461.14f }, { -319.54f, -2.06f, 3559.27f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -319.54f, -2.06f, 3559.27f }, { -198.92f, -2.06f, 3715.27f }, { -141.92f, -2.06f, 3411.27f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1142.90f, -1.00f, 2322.89f }, { 539.61f, -1.00f, 2678.97f }, { 1858.15f, -0.95f, 3025.80f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1858.15f, -0.95f, 3025.80f }, { 1154.17f, -1.00f, 2307.24f }, { 1142.90f, -1.00f, 2322.89f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -198.92f, -2.06f, 3715.27f }, { -319.54f, -2.06f, 3559.27f }, { -253.92f, -2.06f, 3705.27f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -767.79f, -2.06f, 3067.27f }, { -319.54f, -2.06f, 3559.27f }, { 269.58f, -1.00f, 2461.14f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -319.54f, -2.06f, 3559.27f }, { -767.79f, -2.06f, 3067.27f }, { -771.14f, -0.95f, 3449.31f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -771.14f, -0.95f, 3449.31f }, { -767.79f, -2.06f, 3067.27f }, { -821.29f, -2.06f, 3435.27f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 539.61f, -1.00f, 2678.97f }, { 1142.90f, -1.00f, 2322.89f }, { 669.99f, -1.00f, 2509.64f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 669.99f, -1.00f, 2509.64f }, { 466.38f, -1.00f, 2546.21f }, { 539.61f, -1.00f, 2678.97f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 384.61f, -1.00f, 2526.97f }, { 539.61f, -1.00f, 2678.97f }, { 466.38f, -1.00f, 2546.21f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 384.61f, -1.00f, 2526.97f }, { 466.38f, -1.00f, 2546.21f }, { 411.25f, -1.00f, 2492.46f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 466.38f, -1.00f, 2546.21f }, { 669.99f, -1.00f, 2509.64f }, { 566.59f, -1.00f, 2398.65f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 669.99f, -1.00f, 2509.64f }, { 1142.90f, -1.00f, 2322.89f }, { 763.32f, -1.00f, 2341.48f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1142.90f, -1.00f, 2322.89f }, { 778.33f, -1.00f, 2288.09f }, { 763.32f, -1.00f, 2341.48f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1142.90f, -1.00f, 2322.89f }, { 901.23f, -1.00f, 2080.91f }, { 778.33f, -1.00f, 2288.09f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 468.95f, -1.00f, 2240.35f }, { 763.32f, -1.00f, 2341.48f }, { 778.33f, -1.00f, 2288.09f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 778.33f, -1.00f, 2288.09f }, { 452.23f, -1.00f, 2176.91f }, { 468.95f, -1.00f, 2240.35f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 246.88f, -1.00f, 2403.24f }, { 468.95f, -1.00f, 2240.35f }, { 452.23f, -1.00f, 2176.91f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 468.95f, -1.00f, 2240.35f }, { 246.88f, -1.00f, 2403.24f }, { 269.58f, -1.00f, 2461.14f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 64.34f, -1.00f, 2315.05f }, { 269.58f, -1.00f, 2461.14f }, { 246.88f, -1.00f, 2403.24f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -66.76f, -1.00f, 2275.26f }, { 269.58f, -1.00f, 2461.14f }, { 64.34f, -1.00f, 2315.05f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 269.58f, -1.00f, 2461.14f }, { -66.76f, -1.00f, 2275.26f }, { -767.79f, -2.06f, 3067.27f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -66.76f, -1.00f, 2275.26f }, { 64.34f, -1.00f, 2315.05f }, { -43.69f, -1.00f, 2277.61f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -43.69f, -1.00f, 2277.61f }, { 64.34f, -1.00f, 2315.05f }, { -5.44f, -1.00f, 2281.25f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -423.77f, -1.00f, 2108.91f }, { -767.79f, -2.06f, 3067.27f }, { -66.76f, -1.00f, 2275.26f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -423.77f, -1.00f, 2108.91f }, { -66.76f, -1.00f, 2275.26f }, { -336.80f, -1.00f, 2113.23f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -74.53f, -1.00f, 2254.68f }, { -336.80f, -1.00f, 2113.23f }, { -66.76f, -1.00f, 2275.26f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -571.10f, -1.00f, 1995.60f }, { -767.79f, -2.06f, 3067.27f }, { -423.77f, -1.00f, 2108.91f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -571.10f, -1.00f, 1995.60f }, { -1436.21f, -2.06f, 2831.12f }, { -767.79f, -2.06f, 3067.27f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -336.80f, -1.00f, 2113.23f }, { -74.53f, -1.00f, 2254.68f }, { -290.74f, -1.00f, 2101.52f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -1104.12f, -1.00f, 1775.24f }, { -1436.21f, -2.06f, 2831.12f }, { -571.10f, -1.00f, 1995.60f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -742.76f, -1.00f, 1835.27f }, { -1104.12f, -1.00f, 1775.24f }, { -571.10f, -1.00f, 1995.60f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -1104.12f, -1.00f, 1775.24f }, { -742.76f, -1.00f, 1835.26f }, { -978.12f, -1.00f, 1765.24f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -1436.21f, -2.06f, 2831.12f }, { -1104.12f, -1.00f, 1775.24f }, { -1292.38f, -1.00f, 1812.12f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -742.76f, -1.00f, 1835.27f }, { -851.44f, -1.00f, 1745.25f }, { -978.12f, -1.00f, 1765.24f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -1324.76f, -1.00f, 1803.27f }, { -1436.21f, -2.06f, 2831.12f }, { -1292.38f, -1.00f, 1812.12f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -1881.43f, -1.00f, 1438.58f }, { -1436.21f, -2.06f, 2831.12f }, { -1324.76f, -1.00f, 1803.26f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -1472.76f, -1.00f, 1485.27f }, { -1881.43f, -1.00f, 1438.58f }, { -1324.76f, -1.00f, 1803.26f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -1324.76f, -1.00f, 1803.27f }, { -1394.76f, -1.00f, 1527.26f }, { -1472.76f, -1.00f, 1485.26f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -1495.47f, -1.00f, 1458.67f }, { -1881.43f, -1.00f, 1438.58f }, { -1472.76f, -1.00f, 1485.26f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -1881.43f, -1.00f, 1438.58f }, { -1495.47f, -1.00f, 1458.67f }, { -1826.15f, -1.00f, 1390.25f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -1666.77f, -1.00f, 933.94f }, { -1826.15f, -1.00f, 1390.25f }, { -1495.47f, -1.00f, 1458.67f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -1826.15f, -1.00f, 1390.25f }, { -1666.77f, -1.00f, 933.94f }, { -1841.16f, -1.00f, 1372.96f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -2205.15f, -1.00f, 1226.25f }, { -1841.16f, -1.00f, 1372.96f }, { -1666.77f, -1.00f, 933.94f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -2205.15f, -1.00f, 1226.25f }, { -1666.77f, -1.00f, 933.94f }, { -1737.10f, -1.00f, 813.60f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -1841.16f, -1.00f, 1372.96f }, { -2205.15f, -1.00f, 1226.25f }, { -1886.48f, -1.00f, 1412.97f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -2205.15f, -1.00f, 1226.25f }, { -1737.10f, -1.00f, 813.60f }, { -1901.77f, -1.00f, 694.91f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 3559.17f, -0.95f, -202.73f }, { 3884.32f, -0.95f, -684.40f }, { 3536.84f, -0.95f, -211.07f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 3597.20f, -0.95f, 13.23f }, { 3884.32f, -0.95f, -684.40f }, { 3559.17f, -0.95f, -202.73f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 3884.32f, -0.95f, -684.40f }, { 3597.20f, -0.95f, 13.23f }, { 4362.97f, -0.95f, -845.36f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 3597.20f, -0.95f, 13.23f }, { 5243.89f, -0.95f, 147.81f }, { 4362.97f, -0.95f, -845.36f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 5243.89f, -0.95f, 147.81f }, { 4732.64f, -0.95f, -1207.66f }, { 4362.97f, -0.95f, -845.36f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 4362.97f, -0.95f, -845.36f }, { 4732.64f, -0.95f, -1207.66f }, { 4485.32f, -0.95f, -1122.40f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 5243.89f, -0.95f, 147.81f }, { 4993.70f, -0.95f, -1301.85f }, { 4732.64f, -0.95f, -1207.66f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 4993.70f, -0.95f, -1301.85f }, { 5243.89f, -0.95f, 147.81f }, { 5783.89f, -0.95f, -252.19f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 5243.89f, -0.95f, 147.81f }, { 3597.20f, -0.95f, 13.23f }, { 4793.89f, -0.95f, 797.81f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 3617.54f, -0.95f, 496.67f }, { 4793.89f, -0.95f, 797.81f }, { 3597.20f, -0.95f, 13.23f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 3425.54f, -0.95f, 19.58f }, { 3617.54f, -0.95f, 496.67f }, { 3597.20f, -0.95f, 13.23f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 5783.89f, -0.95f, -252.19f }, { 5114.13f, -0.95f, -1674.81f }, { 4993.70f, -0.95f, -1301.85f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 5114.13f, -0.95f, -1674.81f }, { 5783.89f, -0.95f, -252.19f }, { 6423.89f, -0.95f, -202.19f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 3617.54f, -0.95f, 496.67f }, { 3425.54f, -0.95f, 19.58f }, { 3154.49f, -0.95f, 467.26f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 4793.89f, -0.95f, 797.81f }, { 3617.54f, -0.95f, 496.67f }, { 4503.89f, -2.06f, 1000.82f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 4793.89f, -0.95f, 797.81f }, { 4503.89f, -2.06f, 1000.82f }, { 4728.81f, -0.95f, 1048.10f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 6609.48f, -2.06f, -106.18f }, { 5114.13f, -0.95f, -1674.81f }, { 6423.89f, -0.95f, -202.19f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 2037.21f, -1.00f, 1186.55f }, { 2105.19f, -1.00f, 924.52f }, { 2025.85f, -1.00f, 1144.25f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 2037.21f, -1.00f, 1186.55f }, { 2437.36f, -0.95f, 1359.31f }, { 2105.19f, -1.00f, 924.52f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 2715.36f, -0.95f, 1287.31f }, { 2105.19f, -1.00f, 924.52f }, { 2437.36f, -0.95f, 1359.31f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 2437.36f, -0.95f, 1359.31f }, { 2037.21f, -1.00f, 1186.55f }, { 2183.36f, -0.95f, 1397.31f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1791.54f, -1.00f, 1331.58f }, { 2183.36f, -0.95f, 1397.31f }, { 2037.21f, -1.00f, 1186.55f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 2501.36f, -0.95f, 1409.31f }, { 2715.36f, -0.95f, 1287.31f }, { 2437.36f, -0.95f, 1359.31f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 2539.36f, -0.95f, 1453.31f }, { 2715.36f, -0.95f, 1287.31f }, { 2501.36f, -0.95f, 1409.31f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 2183.36f, -0.95f, 1397.31f }, { 1791.54f, -1.00f, 1331.58f }, { 1706.23f, -1.00f, 1448.91f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1960.36f, -0.95f, 1865.31f }, { 2183.36f, -0.95f, 1397.31f }, { 1706.23f, -1.00f, 1448.91f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1694.65f, -1.00f, 1465.96f }, { 1960.36f, -0.95f, 1865.31f }, { 1706.23f, -1.00f, 1448.91f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1706.23f, -1.00f, 1448.91f }, { 1597.39f, -1.00f, 1414.73f }, { 1694.65f, -1.00f, 1465.96f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1597.39f, -1.00f, 1414.73f }, { 1706.23f, -1.00f, 1448.91f }, { 1594.23f, -1.00f, 1389.91f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1960.36f, -0.95f, 1865.31f }, { 1694.65f, -1.00f, 1465.96f }, { 1588.14f, -1.00f, 1671.07f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1594.23f, -1.00f, 1389.91f }, { 1459.54f, -1.00f, 1512.58f }, { 1597.39f, -1.00f, 1414.73f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1472.28f, -1.00f, 1528.67f }, { 1597.39f, -1.00f, 1414.73f }, { 1459.54f, -1.00f, 1512.58f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1459.54f, -1.00f, 1512.58f }, { 1375.17f, -1.00f, 1595.42f }, { 1472.28f, -1.00f, 1528.67f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1135.70f, -1.00f, 1734.57f }, { 1375.17f, -1.00f, 1595.42f }, { 1459.54f, -1.00f, 1512.58f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1135.70f, -1.00f, 1734.57f }, { 1497.47f, -1.00f, 1771.80f }, { 1375.17f, -1.00f, 1595.42f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1497.47f, -1.00f, 1771.80f }, { 1135.70f, -1.00f, 1734.57f }, { 1267.18f, -1.00f, 1993.56f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1520.09f, -1.00f, 1749.14f }, { 1960.36f, -0.95f, 1865.31f }, { 1588.14f, -1.00f, 1671.07f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1967.36f, -0.95f, 1993.31f }, { 1960.36f, -0.95f, 1865.31f }, { 1520.09f, -1.00f, 1749.14f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1960.36f, -0.95f, 1865.31f }, { 1967.36f, -0.95f, 1993.31f }, { 2012.36f, -0.95f, 1940.31f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1497.47f, -1.00f, 1771.80f }, { 1967.36f, -0.95f, 1993.31f }, { 1520.09f, -1.00f, 1749.14f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1246.18f, -1.00f, 2118.56f }, { 1497.47f, -1.00f, 1771.80f }, { 1267.18f, -1.00f, 1993.56f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1497.47f, -1.00f, 1771.80f }, { 1246.18f, -1.00f, 2118.56f }, { 1967.36f, -0.95f, 1993.31f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1588.14f, -1.00f, 1671.07f }, { 1438.09f, -1.00f, 1623.14f }, { 1520.09f, -1.00f, 1749.14f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1438.09f, -1.00f, 1623.14f }, { 1588.14f, -1.00f, 1671.07f }, { 1511.46f, -1.00f, 1564.09f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1206.85f, -1.00f, 2068.21f }, { 1246.18f, -1.00f, 2118.56f }, { 1267.18f, -1.00f, 1993.56f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1154.17f, -1.00f, 2307.24f }, { 1246.18f, -1.00f, 2118.56f }, { 1227.90f, -1.00f, 2136.89f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1227.90f, -1.00f, 2136.89f }, { 1015.17f, -1.00f, 2162.20f }, { 1154.17f, -1.00f, 2307.24f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1227.90f, -1.00f, 2136.89f }, { 1124.24f, -1.00f, 2016.23f }, { 1015.17f, -1.00f, 2162.20f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -4892.15f, -0.95f, -4363.69f }, { -7937.41f, -0.95f, -10120.20f }, { -5537.14f, -0.95f, -4918.69f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -7937.41f, -0.95f, -10120.19f }, { -7598.69f, -0.95f, -6614.82f }, { -5537.14f, -0.95f, -4918.69f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { -7598.69f, -0.95f, -6614.82f }, { -7937.41f, -0.95f, -10120.20f }, { -8211.91f, -0.95f, -7780.07f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 3694.19f, -0.95f, -3542.38f }, { 3769.56f, -0.95f, -3635.71f }, { 3624.18f, -0.95f, -3596.74f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 3710.52f, -0.95f, -3698.19f }, { 3624.18f, -0.95f, -3596.74f }, { 3769.56f, -0.95f, -3635.71f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 3710.52f, -0.95f, -3698.19f }, { 3298.19f, -0.95f, -3464.38f }, { 3624.18f, -0.95f, -3596.74f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 3542.08f, -0.95f, -3502.26f }, { 3624.18f, -0.95f, -3596.74f }, { 3298.19f, -0.95f, -3464.38f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 3298.19f, -0.95f, -3464.38f }, { 3528.30f, -0.95f, -3479.46f }, { 3542.08f, -0.95f, -3502.26f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 3298.19f, -0.95f, -3464.38f }, { 3424.67f, -0.95f, -3349.89f }, { 3528.30f, -0.95f, -3479.46f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 3298.19f, -0.95f, -3464.38f }, { 3710.52f, -0.95f, -3698.19f }, { 3224.71f, -0.95f, -3458.06f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 3762.48f, -0.95f, -3748.52f }, { 3224.71f, -0.95f, -3458.06f }, { 3710.52f, -0.95f, -3698.19f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 5208.10f, -0.95f, -4761.69f }, { 3224.71f, -0.95f, -3458.06f }, { 3762.48f, -0.95f, -3748.52f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 4096.63f, -0.95f, -3776.80f }, { 5208.10f, -0.95f, -4761.69f }, { 3762.48f, -0.95f, -3748.52f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 5208.10f, -0.95f, -4761.69f }, { 4096.63f, -0.95f, -3776.80f }, { 4322.95f, -0.95f, -3811.02f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 4707.85f, -0.95f, -3629.74f }, { 5208.10f, -0.95f, -4761.69f }, { 4322.95f, -0.95f, -3811.02f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 4223.27f, -0.95f, -3768.70f }, { 4322.95f, -0.95f, -3811.02f }, { 4096.63f, -0.95f, -3776.80f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 4111.49f, -0.95f, -3685.19f }, { 4223.26f, -0.95f, -3768.70f }, { 4096.63f, -0.95f, -3776.80f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 4322.94f, -0.95f, -3811.02f }, { 4223.26f, -0.95f, -3768.70f }, { 4313.27f, -0.95f, -3765.39f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 4223.27f, -0.95f, -3768.70f }, { 4111.49f, -0.95f, -3685.19f }, { 4222.28f, -0.95f, -3701.87f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 692.45f, -0.95f, -337.58f }, { 625.63f, -0.94f, -342.45f }, { 674.75f, -0.95f, -327.68f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 625.63f, -0.94f, -342.45f }, { 692.45f, -0.95f, -337.58f }, { 600.57f, -0.94f, -371.20f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 600.57f, -0.94f, -371.20f }, { 692.45f, -0.95f, -337.58f }, { 565.61f, -1.02f, -482.20f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 692.45f, -0.95f, -337.58f }, { 564.11f, -1.02f, -513.15f }, { 565.61f, -1.02f, -482.20f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 565.61f, -1.02f, -482.20f }, { 564.11f, -1.02f, -513.15f }, { 562.45f, -1.02f, -492.27f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 692.45f, -0.95f, -337.58f }, { 573.73f, -1.02f, -634.01f }, { 564.11f, -1.02f, -513.15f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 573.73f, -1.02f, -634.01f }, { 692.45f, -0.95f, -337.58f }, { 705.13f, -0.95f, -330.84f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 717.22f, -0.95f, -323.47f }, { 573.73f, -1.02f, -634.01f }, { 705.13f, -0.95f, -330.84f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1144.70f, -0.95f, -464.48f }, { 573.73f, -1.02f, -634.01f }, { 717.22f, -0.95f, -323.47f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1144.70f, -0.95f, -464.48f }, { 717.22f, -0.95f, -323.47f }, { 728.37f, -0.95f, -315.44f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 738.73f, -0.95f, -306.86f }, { 1144.70f, -0.95f, -464.48f }, { 728.37f, -0.95f, -315.44f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 747.86f, -0.95f, -297.43f }, { 1144.70f, -0.95f, -464.48f }, { 738.73f, -0.95f, -306.85f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1144.70f, -0.95f, -464.48f }, { 747.86f, -0.95f, -297.43f }, { 1120.83f, -0.95f, -353.07f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1121.30f, -0.95f, -352.97f }, { 1144.70f, -0.95f, -464.48f }, { 1120.83f, -0.95f, -353.07f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 756.28f, -0.95f, -287.33f }, { 1120.83f, -0.95f, -353.07f }, { 747.86f, -0.95f, -297.43f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 763.76f, -0.95f, -276.39f }, { 1120.83f, -0.95f, -353.07f }, { 756.28f, -0.95f, -287.33f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1120.83f, -0.95f, -353.07f }, { 763.76f, -0.95f, -276.39f }, { 1120.73f, -0.95f, -352.60f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 770.22f, -0.95f, -264.57f }, { 1120.73f, -0.95f, -352.60f }, { 763.76f, -0.95f, -276.39f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 775.57f, -0.95f, -251.96f }, { 1120.73f, -0.95f, -352.60f }, { 770.22f, -0.95f, -264.57f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1120.73f, -0.95f, -352.60f }, { 775.57f, -0.95f, -251.96f }, { 1122.49f, -0.95f, -326.77f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 775.57f, -0.95f, -251.96f }, { 1099.01f, -0.95f, -220.50f }, { 1122.49f, -0.95f, -326.77f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1099.01f, -0.95f, -220.50f }, { 775.57f, -0.95f, -251.96f }, { 850.20f, -0.95f, -155.45f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 850.20f, -0.95f, -155.45f }, { 775.57f, -0.95f, -251.96f }, { 779.91f, -0.95f, -238.63f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 850.20f, -0.95f, -155.45f }, { 779.91f, -0.95f, -238.63f }, { 783.01f, -0.95f, -224.73f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1099.01f, -0.95f, -220.50f }, { 850.20f, -0.95f, -155.45f }, { 908.53f, -0.95f, -123.11f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 779.91f, -0.95f, -238.63f }, { 782.69f, -0.95f, -224.67f }, { 783.01f, -0.95f, -224.73f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1099.01f, -0.95f, -220.50f }, { 908.53f, -0.95f, -123.11f }, { 953.10f, -0.95f, -108.84f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1099.01f, -0.95f, -220.50f }, { 953.10f, -0.95f, -108.84f }, { 1056.33f, -0.95f, -118.36f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1056.33f, -0.95f, -118.36f }, { 1126.94f, -0.95f, -156.32f }, { 1099.01f, -0.95f, -220.50f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1056.33f, -0.95f, -118.36f }, { 953.10f, -0.95f, -108.84f }, { 953.13f, -0.95f, -108.52f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1126.94f, -0.95f, -156.32f }, { 1056.33f, -0.95f, -118.36f }, { 1102.69f, -0.95f, -88.67f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 1102.69f, -0.95f, -88.67f }, { 1056.33f, -0.95f, -118.36f }, { 1102.59f, -0.95f, -88.36f }, -2.06f, 0.30f },   // ground_water_01 backdrops
    { { 310.87f, 1.50f, -219.17f }, { 312.15f, 1.50f, -218.79f }, { 313.42f, 1.50f, -225.50f }, 1.50f, 0.00f },   // ground_bdstatue_05 backdrops
    { { 309.68f, 1.50f, -219.79f }, { 310.87f, 1.50f, -219.17f }, { 313.42f, 1.50f, -225.50f }, 1.50f, 0.00f },   // ground_bdstatue_05 backdrops
    { { 312.15f, 1.50f, -218.79f }, { 313.48f, 1.50f, -218.67f }, { 313.42f, 1.50f, -225.50f }, 1.50f, 0.00f },   // ground_bdstatue_05 backdrops
    { { 308.64f, 1.50f, -220.63f }, { 309.68f, 1.50f, -219.79f }, { 313.42f, 1.50f, -225.50f }, 1.50f, 0.00f },   // ground_bdstatue_05 backdrops
    { { 313.48f, 1.50f, -218.67f }, { 314.81f, 1.50f, -218.82f }, { 313.42f, 1.50f, -225.50f }, 1.50f, 0.00f },   // ground_bdstatue_05 backdrops
    { { 307.78f, 1.50f, -221.65f }, { 308.64f, 1.50f, -220.63f }, { 313.42f, 1.50f, -225.50f }, 1.50f, 0.00f },   // ground_bdstatue_05 backdrops
    { { 314.81f, 1.50f, -218.82f }, { 316.09f, 1.50f, -219.22f }, { 313.42f, 1.50f, -225.50f }, 1.50f, 0.00f },   // ground_bdstatue_05 backdrops
    { { 307.14f, 1.50f, -222.83f }, { 307.78f, 1.50f, -221.65f }, { 313.42f, 1.50f, -225.50f }, 1.50f, 0.00f },   // ground_bdstatue_05 backdrops
    { { 316.09f, 1.50f, -219.22f }, { 317.26f, 1.50f, -219.86f }, { 313.42f, 1.50f, -225.50f }, 1.50f, 0.00f },   // ground_bdstatue_05 backdrops
    { { 306.74f, 1.50f, -224.10f }, { 307.14f, 1.50f, -222.83f }, { 313.42f, 1.50f, -225.50f }, 1.50f, 0.00f },   // ground_bdstatue_05 backdrops
    { { 317.26f, 1.50f, -219.86f }, { 318.29f, 1.50f, -220.71f }, { 313.42f, 1.50f, -225.50f }, 1.50f, 0.00f },   // ground_bdstatue_05 backdrops
    { { 306.60f, 1.50f, -225.43f }, { 306.74f, 1.50f, -224.10f }, { 313.42f, 1.50f, -225.50f }, 1.50f, 0.00f },   // ground_bdstatue_05 backdrops
    { { 318.29f, 1.50f, -220.71f }, { 319.13f, 1.50f, -221.76f }, { 313.42f, 1.50f, -225.50f }, 1.50f, 0.00f },   // ground_bdstatue_05 backdrops
    { { 306.72f, 1.50f, -226.77f }, { 306.60f, 1.50f, -225.43f }, { 313.42f, 1.50f, -225.50f }, 1.50f, 0.00f },   // ground_bdstatue_05 backdrops
    { { 319.13f, 1.50f, -221.76f }, { 319.75f, 1.50f, -222.94f }, { 313.42f, 1.50f, -225.50f }, 1.50f, 0.00f },   // ground_bdstatue_05 backdrops
    { { 307.09f, 1.50f, -228.05f }, { 306.72f, 1.50f, -226.77f }, { 313.42f, 1.50f, -225.50f }, 1.50f, 0.00f },   // ground_bdstatue_05 backdrops
    { { 319.75f, 1.50f, -222.94f }, { 320.12f, 1.50f, -224.23f }, { 313.42f, 1.50f, -225.50f }, 1.50f, 0.00f },   // ground_bdstatue_05 backdrops
    { { 307.71f, 1.50f, -229.24f }, { 307.09f, 1.50f, -228.05f }, { 313.42f, 1.50f, -225.50f }, 1.50f, 0.00f },   // ground_bdstatue_05 backdrops
    { { 320.12f, 1.50f, -224.22f }, { 320.24f, 1.50f, -225.56f }, { 313.42f, 1.50f, -225.50f }, 1.50f, 0.00f },   // ground_bdstatue_05 backdrops
    { { 308.55f, 1.50f, -230.28f }, { 307.71f, 1.50f, -229.24f }, { 313.42f, 1.50f, -225.50f }, 1.50f, 0.00f },   // ground_bdstatue_05 backdrops
    { { 320.24f, 1.50f, -225.56f }, { 320.10f, 1.50f, -226.89f }, { 313.42f, 1.50f, -225.50f }, 1.50f, 0.00f },   // ground_bdstatue_05 backdrops
    { { 309.58f, 1.50f, -231.13f }, { 308.55f, 1.50f, -230.28f }, { 313.42f, 1.50f, -225.50f }, 1.50f, 0.00f },   // ground_bdstatue_05 backdrops
    { { 320.10f, 1.50f, -226.89f }, { 319.70f, 1.50f, -228.16f }, { 313.42f, 1.50f, -225.50f }, 1.50f, 0.00f },   // ground_bdstatue_05 backdrops
    { { 310.75f, 1.50f, -231.78f }, { 309.58f, 1.50f, -231.13f }, { 313.42f, 1.50f, -225.50f }, 1.50f, 0.00f },   // ground_bdstatue_05 backdrops
    { { 319.70f, 1.50f, -228.16f }, { 319.06f, 1.50f, -229.34f }, { 313.42f, 1.50f, -225.50f }, 1.50f, 0.00f },   // ground_bdstatue_05 backdrops
    { { 312.03f, 1.50f, -232.18f }, { 310.75f, 1.50f, -231.78f }, { 313.42f, 1.50f, -225.50f }, 1.50f, 0.00f },   // ground_bdstatue_05 backdrops
    { { 319.06f, 1.50f, -229.34f }, { 318.20f, 1.50f, -230.36f }, { 313.42f, 1.50f, -225.50f }, 1.50f, 0.00f },   // ground_bdstatue_05 backdrops
    { { 313.36f, 1.50f, -232.32f }, { 312.03f, 1.50f, -232.18f }, { 313.42f, 1.50f, -225.50f }, 1.50f, 0.00f },   // ground_bdstatue_05 backdrops
    { { 318.20f, 1.50f, -230.36f }, { 317.16f, 1.50f, -231.20f }, { 313.42f, 1.50f, -225.50f }, 1.50f, 0.00f },   // ground_bdstatue_05 backdrops
    { { 314.69f, 1.50f, -232.20f }, { 313.36f, 1.50f, -232.32f }, { 313.42f, 1.50f, -225.50f }, 1.50f, 0.00f },   // ground_bdstatue_05 backdrops
    { { 317.16f, 1.50f, -231.20f }, { 315.97f, 1.50f, -231.82f }, { 313.42f, 1.50f, -225.50f }, 1.50f, 0.00f },   // ground_bdstatue_05 backdrops
    { { 315.98f, 1.50f, -231.82f }, { 314.69f, 1.50f, -232.20f }, { 313.42f, 1.50f, -225.50f }, 1.50f, 0.00f },   // ground_bdstatue_05 backdrops
    { { 938.86f, -0.36f, 100.31f }, { 939.15f, -0.36f, 100.55f }, { 940.21f, -0.36f, 98.97f }, -0.36f, 0.00f },   // ground_bdstatue_05 centralcitybuildings_zs_zs_109
    { { 938.62f, -0.36f, 100.03f }, { 938.86f, -0.36f, 100.31f }, { 940.21f, -0.36f, 98.97f }, -0.36f, 0.00f },   // ground_bdstatue_05 centralcitybuildings_zs_zs_109
    { { 939.15f, -0.36f, 100.55f }, { 939.48f, -0.36f, 100.73f }, { 940.21f, -0.36f, 98.97f }, -0.36f, 0.00f },   // ground_bdstatue_05 centralcitybuildings_zs_zs_109
    { { 938.45f, -0.36f, 99.70f }, { 938.62f, -0.36f, 100.03f }, { 940.21f, -0.36f, 98.97f }, -0.36f, 0.00f },   // ground_bdstatue_05 centralcitybuildings_zs_zs_109
    { { 939.48f, -0.36f, 100.73f }, { 939.83f, -0.36f, 100.84f }, { 940.21f, -0.36f, 98.97f }, -0.36f, 0.00f },   // ground_bdstatue_05 centralcitybuildings_zs_zs_109
    { { 938.34f, -0.36f, 99.34f }, { 938.45f, -0.36f, 99.70f }, { 940.21f, -0.36f, 98.97f }, -0.36f, 0.00f },   // ground_bdstatue_05 centralcitybuildings_zs_zs_109
    { { 939.83f, -0.36f, 100.84f }, { 940.21f, -0.36f, 100.87f }, { 940.21f, -0.36f, 98.97f }, -0.36f, 0.00f },   // ground_bdstatue_05 centralcitybuildings_zs_zs_109
    { { 938.30f, -0.36f, 98.97f }, { 938.34f, -0.36f, 99.34f }, { 940.21f, -0.36f, 98.97f }, -0.36f, 0.00f },   // ground_bdstatue_05 centralcitybuildings_zs_zs_109
    { { 940.21f, -0.36f, 100.87f }, { 940.58f, -0.36f, 100.84f }, { 940.21f, -0.36f, 98.97f }, -0.36f, 0.00f },   // ground_bdstatue_05 centralcitybuildings_zs_zs_109
    { { 938.34f, -0.36f, 98.60f }, { 938.30f, -0.36f, 98.97f }, { 940.21f, -0.36f, 98.97f }, -0.36f, 0.00f },   // ground_bdstatue_05 centralcitybuildings_zs_zs_109
    { { 940.58f, -0.36f, 100.84f }, { 940.93f, -0.36f, 100.73f }, { 940.21f, -0.36f, 98.97f }, -0.36f, 0.00f },   // ground_bdstatue_05 centralcitybuildings_zs_zs_109
    { { 938.45f, -0.36f, 98.24f }, { 938.34f, -0.36f, 98.60f }, { 940.21f, -0.36f, 98.97f }, -0.36f, 0.00f },   // ground_bdstatue_05 centralcitybuildings_zs_zs_109
    { { 940.93f, -0.36f, 100.73f }, { 941.26f, -0.36f, 100.55f }, { 940.21f, -0.36f, 98.97f }, -0.36f, 0.00f },   // ground_bdstatue_05 centralcitybuildings_zs_zs_109
    { { 938.62f, -0.36f, 97.91f }, { 938.45f, -0.36f, 98.24f }, { 940.21f, -0.36f, 98.97f }, -0.36f, 0.00f },   // ground_bdstatue_05 centralcitybuildings_zs_zs_109
    { { 941.26f, -0.36f, 100.55f }, { 941.55f, -0.36f, 100.31f }, { 940.21f, -0.36f, 98.97f }, -0.36f, 0.00f },   // ground_bdstatue_05 centralcitybuildings_zs_zs_109
    { { 938.86f, -0.36f, 97.62f }, { 938.62f, -0.36f, 97.91f }, { 940.21f, -0.36f, 98.97f }, -0.36f, 0.00f },   // ground_bdstatue_05 centralcitybuildings_zs_zs_109
    { { 941.55f, -0.36f, 100.31f }, { 941.79f, -0.36f, 100.03f }, { 940.21f, -0.36f, 98.97f }, -0.36f, 0.00f },   // ground_bdstatue_05 centralcitybuildings_zs_zs_109
    { { 939.15f, -0.36f, 97.39f }, { 938.86f, -0.36f, 97.62f }, { 940.21f, -0.36f, 98.97f }, -0.36f, 0.00f },   // ground_bdstatue_05 centralcitybuildings_zs_zs_109
    { { 941.79f, -0.36f, 100.03f }, { 941.96f, -0.36f, 99.70f }, { 940.21f, -0.36f, 98.97f }, -0.36f, 0.00f },   // ground_bdstatue_05 centralcitybuildings_zs_zs_109
    { { 939.48f, -0.36f, 97.21f }, { 939.15f, -0.36f, 97.39f }, { 940.21f, -0.36f, 98.97f }, -0.36f, 0.00f },   // ground_bdstatue_05 centralcitybuildings_zs_zs_109
    { { 941.97f, -0.36f, 99.70f }, { 942.07f, -0.36f, 99.34f }, { 940.21f, -0.36f, 98.97f }, -0.36f, 0.00f },   // ground_bdstatue_05 centralcitybuildings_zs_zs_109
    { { 939.83f, -0.36f, 97.10f }, { 939.48f, -0.36f, 97.21f }, { 940.21f, -0.36f, 98.97f }, -0.36f, 0.00f },   // ground_bdstatue_05 centralcitybuildings_zs_zs_109
    { { 942.07f, -0.36f, 99.34f }, { 942.11f, -0.36f, 98.97f }, { 940.21f, -0.36f, 98.97f }, -0.36f, 0.00f },   // ground_bdstatue_05 centralcitybuildings_zs_zs_109
    { { 940.21f, -0.36f, 97.06f }, { 939.83f, -0.36f, 97.10f }, { 940.21f, -0.36f, 98.97f }, -0.36f, 0.00f },   // ground_bdstatue_05 centralcitybuildings_zs_zs_109
    { { 942.11f, -0.36f, 98.97f }, { 942.07f, -0.36f, 98.60f }, { 940.21f, -0.36f, 98.97f }, -0.36f, 0.00f },   // ground_bdstatue_05 centralcitybuildings_zs_zs_109
    { { 940.58f, -0.36f, 97.10f }, { 940.21f, -0.36f, 97.06f }, { 940.21f, -0.36f, 98.97f }, -0.36f, 0.00f },   // ground_bdstatue_05 centralcitybuildings_zs_zs_109
    { { 942.07f, -0.36f, 98.60f }, { 941.96f, -0.36f, 98.24f }, { 940.21f, -0.36f, 98.97f }, -0.36f, 0.00f },   // ground_bdstatue_05 centralcitybuildings_zs_zs_109
    { { 940.93f, -0.36f, 97.21f }, { 940.58f, -0.36f, 97.10f }, { 940.21f, -0.36f, 98.97f }, -0.36f, 0.00f },   // ground_bdstatue_05 centralcitybuildings_zs_zs_109
    { { 941.97f, -0.36f, 98.24f }, { 941.79f, -0.36f, 97.91f }, { 940.21f, -0.36f, 98.97f }, -0.36f, 0.00f },   // ground_bdstatue_05 centralcitybuildings_zs_zs_109
    { { 941.26f, -0.36f, 97.39f }, { 940.93f, -0.36f, 97.21f }, { 940.21f, -0.36f, 98.97f }, -0.36f, 0.00f },   // ground_bdstatue_05 centralcitybuildings_zs_zs_109
    { { 941.79f, -0.36f, 97.91f }, { 941.55f, -0.36f, 97.62f }, { 940.21f, -0.36f, 98.97f }, -0.36f, 0.00f },   // ground_bdstatue_05 centralcitybuildings_zs_zs_109
    { { 941.55f, -0.36f, 97.62f }, { 941.26f, -0.36f, 97.39f }, { 940.21f, -0.36f, 98.97f }, -0.36f, 0.00f },   // ground_bdstatue_05 centralcitybuildings_zs_zs_109
    { { 945.69f, -0.36f, 92.39f }, { 945.97f, -0.36f, 92.63f }, { 947.03f, -0.36f, 91.05f }, -0.36f, 0.00f },   // ground_bdstatue_05 centralcitybuildings_zs_zs_109
    { { 945.45f, -0.36f, 92.11f }, { 945.69f, -0.36f, 92.39f }, { 947.03f, -0.36f, 91.05f }, -0.36f, 0.00f },   // ground_bdstatue_05 centralcitybuildings_zs_zs_109
    { { 945.98f, -0.36f, 92.63f }, { 946.30f, -0.36f, 92.81f }, { 947.03f, -0.36f, 91.05f }, -0.36f, 0.00f },   // ground_bdstatue_05 centralcitybuildings_zs_zs_109
    { { 945.27f, -0.36f, 91.78f }, { 945.45f, -0.36f, 92.11f }, { 947.03f, -0.36f, 91.05f }, -0.36f, 0.00f },   // ground_bdstatue_05 centralcitybuildings_zs_zs_109
    { { 946.30f, -0.36f, 92.81f }, { 946.66f, -0.36f, 92.91f }, { 947.03f, -0.36f, 91.05f }, -0.36f, 0.00f },   // ground_bdstatue_05 centralcitybuildings_zs_zs_109
    { { 945.16f, -0.36f, 91.42f }, { 945.27f, -0.36f, 91.78f }, { 947.03f, -0.36f, 91.05f }, -0.36f, 0.00f },   // ground_bdstatue_05 centralcitybuildings_zs_zs_109
    { { 946.66f, -0.36f, 92.92f }, { 947.03f, -0.36f, 92.95f }, { 947.03f, -0.36f, 91.05f }, -0.36f, 0.00f },   // ground_bdstatue_05 centralcitybuildings_zs_zs_109
    { { 945.13f, -0.36f, 91.05f }, { 945.17f, -0.36f, 91.42f }, { 947.03f, -0.36f, 91.05f }, -0.36f, 0.00f },   // ground_bdstatue_05 centralcitybuildings_zs_zs_109
    { { 947.03f, -0.36f, 92.95f }, { 947.40f, -0.36f, 92.91f }, { 947.03f, -0.36f, 91.05f }, -0.36f, 0.00f },   // ground_bdstatue_05 centralcitybuildings_zs_zs_109
    { { 945.16f, -0.36f, 90.68f }, { 945.13f, -0.36f, 91.05f }, { 947.03f, -0.36f, 91.05f }, -0.36f, 0.00f },   // ground_bdstatue_05 centralcitybuildings_zs_zs_109
    { { 947.40f, -0.36f, 92.92f }, { 947.76f, -0.36f, 92.81f }, { 947.03f, -0.36f, 91.05f }, -0.36f, 0.00f },   // ground_bdstatue_05 centralcitybuildings_zs_zs_109
    { { 945.27f, -0.36f, 90.32f }, { 945.17f, -0.36f, 90.68f }, { 947.03f, -0.36f, 91.05f }, -0.36f, 0.00f },   // ground_bdstatue_05 centralcitybuildings_zs_zs_109
    { { 947.76f, -0.36f, 92.81f }, { 948.09f, -0.36f, 92.63f }, { 947.03f, -0.36f, 91.05f }, -0.36f, 0.00f },   // ground_bdstatue_05 centralcitybuildings_zs_zs_109
    { { 945.45f, -0.36f, 89.99f }, { 945.27f, -0.36f, 90.32f }, { 947.03f, -0.36f, 91.05f }, -0.36f, 0.00f },   // ground_bdstatue_05 centralcitybuildings_zs_zs_109
    { { 948.09f, -0.36f, 92.63f }, { 948.38f, -0.36f, 92.39f }, { 947.03f, -0.36f, 91.05f }, -0.36f, 0.00f },   // ground_bdstatue_05 centralcitybuildings_zs_zs_109
    { { 945.69f, -0.36f, 89.70f }, { 945.45f, -0.36f, 89.99f }, { 947.03f, -0.36f, 91.05f }, -0.36f, 0.00f },   // ground_bdstatue_05 centralcitybuildings_zs_zs_109
    { { 948.38f, -0.36f, 92.39f }, { 948.62f, -0.36f, 92.11f }, { 947.03f, -0.36f, 91.05f }, -0.36f, 0.00f },   // ground_bdstatue_05 centralcitybuildings_zs_zs_109
    { { 945.98f, -0.36f, 89.47f }, { 945.69f, -0.36f, 89.70f }, { 947.03f, -0.36f, 91.05f }, -0.36f, 0.00f },   // ground_bdstatue_05 centralcitybuildings_zs_zs_109
    { { 948.62f, -0.36f, 92.11f }, { 948.79f, -0.36f, 91.78f }, { 947.03f, -0.36f, 91.05f }, -0.36f, 0.00f },   // ground_bdstatue_05 centralcitybuildings_zs_zs_109
    { { 946.30f, -0.36f, 89.29f }, { 945.97f, -0.36f, 89.46f }, { 947.03f, -0.36f, 91.05f }, -0.36f, 0.00f },   // ground_bdstatue_05 centralcitybuildings_zs_zs_109
    { { 948.79f, -0.36f, 91.78f }, { 948.90f, -0.36f, 91.42f }, { 947.03f, -0.36f, 91.05f }, -0.36f, 0.00f },   // ground_bdstatue_05 centralcitybuildings_zs_zs_109
    { { 946.66f, -0.36f, 89.18f }, { 946.30f, -0.36f, 89.29f }, { 947.03f, -0.36f, 91.05f }, -0.36f, 0.00f },   // ground_bdstatue_05 centralcitybuildings_zs_zs_109
    { { 948.90f, -0.36f, 91.42f }, { 948.94f, -0.36f, 91.05f }, { 947.03f, -0.36f, 91.05f }, -0.36f, 0.00f },   // ground_bdstatue_05 centralcitybuildings_zs_zs_109
    { { 947.03f, -0.36f, 89.14f }, { 946.66f, -0.36f, 89.18f }, { 947.03f, -0.36f, 91.05f }, -0.36f, 0.00f },   // ground_bdstatue_05 centralcitybuildings_zs_zs_109
    { { 948.94f, -0.36f, 91.05f }, { 948.90f, -0.36f, 90.68f }, { 947.03f, -0.36f, 91.05f }, -0.36f, 0.00f },   // ground_bdstatue_05 centralcitybuildings_zs_zs_109
    { { 947.40f, -0.36f, 89.18f }, { 947.03f, -0.36f, 89.14f }, { 947.03f, -0.36f, 91.05f }, -0.36f, 0.00f },   // ground_bdstatue_05 centralcitybuildings_zs_zs_109
    { { 948.90f, -0.36f, 90.68f }, { 948.79f, -0.36f, 90.32f }, { 947.03f, -0.36f, 91.05f }, -0.36f, 0.00f },   // ground_bdstatue_05 centralcitybuildings_zs_zs_109
    { { 947.76f, -0.36f, 89.29f }, { 947.40f, -0.36f, 89.18f }, { 947.03f, -0.36f, 91.05f }, -0.36f, 0.00f },   // ground_bdstatue_05 centralcitybuildings_zs_zs_109
    { { 948.79f, -0.36f, 90.32f }, { 948.62f, -0.36f, 89.99f }, { 947.03f, -0.36f, 91.05f }, -0.36f, 0.00f },   // ground_bdstatue_05 centralcitybuildings_zs_zs_109
    { { 948.09f, -0.36f, 89.47f }, { 947.76f, -0.36f, 89.29f }, { 947.03f, -0.36f, 91.05f }, -0.36f, 0.00f },   // ground_bdstatue_05 centralcitybuildings_zs_zs_109
    { { 948.62f, -0.36f, 89.99f }, { 948.38f, -0.36f, 89.70f }, { 947.03f, -0.36f, 91.05f }, -0.36f, 0.00f },   // ground_bdstatue_05 centralcitybuildings_zs_zs_109
    { { 948.38f, -0.36f, 89.70f }, { 948.09f, -0.36f, 89.46f }, { 947.03f, -0.36f, 91.05f }, -0.36f, 0.00f },   // ground_bdstatue_05 centralcitybuildings_zs_zs_109
    { { 953.26f, 102.58f, 149.79f }, { 953.23f, 102.50f, 149.79f }, { 953.25f, 102.58f, 149.81f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 953.23f, 102.50f, 149.79f }, { 953.22f, 102.50f, 149.80f }, { 953.25f, 102.58f, 149.81f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 953.37f, 102.70f, 149.87f }, { 953.41f, 102.71f, 149.89f }, { 953.38f, 102.70f, 149.85f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 953.38f, 102.70f, 149.85f }, { 953.41f, 102.71f, 149.89f }, { 953.42f, 102.71f, 149.88f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 953.21f, 102.40f, 149.79f }, { 953.20f, 102.29f, 149.79f }, { 953.20f, 102.40f, 149.80f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 953.20f, 102.29f, 149.79f }, { 953.19f, 102.29f, 149.80f }, { 953.20f, 102.40f, 149.80f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 953.29f, 102.65f, 149.83f }, { 953.33f, 102.68f, 149.84f }, { 953.30f, 102.65f, 149.81f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 953.30f, 102.65f, 149.81f }, { 953.33f, 102.68f, 149.84f }, { 953.34f, 102.68f, 149.83f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 953.19f, 102.29f, 149.80f }, { 953.05f, 102.40f, 149.99f }, { 953.20f, 102.40f, 149.80f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 953.05f, 102.40f, 149.99f }, { 953.19f, 102.29f, 149.80f }, { 953.04f, 102.29f, 149.98f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 953.04f, 102.29f, 149.98f }, { 952.90f, 102.29f, 150.16f }, { 953.05f, 102.40f, 149.99f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 952.90f, 102.40f, 150.17f }, { 953.05f, 102.40f, 149.99f }, { 952.90f, 102.29f, 150.16f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 953.22f, 102.50f, 149.80f }, { 953.08f, 102.58f, 150.01f }, { 953.25f, 102.58f, 149.81f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 953.08f, 102.58f, 150.01f }, { 953.22f, 102.50f, 149.80f }, { 953.06f, 102.50f, 150.00f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 953.06f, 102.50f, 150.00f }, { 952.90f, 102.50f, 150.19f }, { 953.08f, 102.58f, 150.01f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 952.92f, 102.58f, 150.22f }, { 953.08f, 102.58f, 150.01f }, { 952.90f, 102.50f, 150.19f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 953.29f, 102.65f, 149.83f }, { 953.15f, 102.68f, 150.07f }, { 953.33f, 102.68f, 149.84f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 953.15f, 102.68f, 150.06f }, { 953.29f, 102.65f, 149.83f }, { 953.11f, 102.65f, 150.04f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 952.94f, 102.65f, 150.25f }, { 953.15f, 102.68f, 150.07f }, { 953.11f, 102.65f, 150.04f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 953.15f, 102.68f, 150.06f }, { 952.94f, 102.65f, 150.25f }, { 952.96f, 102.68f, 150.29f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 953.37f, 102.70f, 149.87f }, { 953.22f, 102.71f, 150.12f }, { 953.41f, 102.71f, 149.89f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 953.22f, 102.71f, 150.12f }, { 953.37f, 102.70f, 149.87f }, { 953.18f, 102.70f, 150.09f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 952.99f, 102.70f, 150.32f }, { 953.22f, 102.71f, 150.12f }, { 953.18f, 102.70f, 150.09f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 953.22f, 102.71f, 150.12f }, { 952.99f, 102.70f, 150.32f }, { 953.02f, 102.71f, 150.35f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 952.89f, 102.29f, 150.18f }, { 952.88f, 102.40f, 150.19f }, { 952.90f, 102.40f, 150.17f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 952.90f, 102.40f, 150.17f }, { 952.90f, 102.29f, 150.16f }, { 952.89f, 102.29f, 150.18f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 952.89f, 102.50f, 150.21f }, { 952.90f, 102.58f, 150.23f }, { 952.92f, 102.58f, 150.22f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 952.92f, 102.58f, 150.22f }, { 952.90f, 102.50f, 150.19f }, { 952.89f, 102.50f, 150.21f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 952.93f, 102.65f, 150.27f }, { 952.96f, 102.68f, 150.29f }, { 952.94f, 102.65f, 150.25f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 952.96f, 102.68f, 150.29f }, { 952.93f, 102.65f, 150.27f }, { 952.95f, 102.68f, 150.30f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 952.98f, 102.70f, 150.34f }, { 953.02f, 102.71f, 150.35f }, { 952.99f, 102.70f, 150.32f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 953.02f, 102.71f, 150.35f }, { 952.98f, 102.70f, 150.34f }, { 953.01f, 102.71f, 150.37f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 953.34f, 102.67f, 149.83f }, { 953.30f, 102.64f, 149.81f }, { 953.30f, 102.65f, 149.81f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 953.30f, 102.65f, 149.81f }, { 953.34f, 102.68f, 149.83f }, { 953.34f, 102.67f, 149.83f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 952.95f, 102.67f, 150.30f }, { 952.93f, 102.65f, 150.27f }, { 952.93f, 102.64f, 150.27f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 952.93f, 102.65f, 150.27f }, { 952.95f, 102.67f, 150.30f }, { 952.95f, 102.68f, 150.30f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 953.22f, 102.40f, 149.79f }, { 953.20f, 102.29f, 149.79f }, { 953.21f, 102.40f, 149.79f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 953.20f, 102.29f, 149.79f }, { 953.22f, 102.40f, 149.79f }, { 953.21f, 102.29f, 149.79f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 952.89f, 102.40f, 150.19f }, { 952.89f, 102.29f, 150.18f }, { 952.89f, 102.29f, 150.18f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 952.89f, 102.29f, 150.18f }, { 952.89f, 102.40f, 150.19f }, { 952.88f, 102.40f, 150.19f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 953.27f, 102.58f, 149.80f }, { 953.24f, 102.49f, 149.79f }, { 953.23f, 102.50f, 149.79f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 953.23f, 102.50f, 149.79f }, { 953.26f, 102.58f, 149.80f }, { 953.27f, 102.58f, 149.80f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 952.91f, 102.58f, 150.24f }, { 952.89f, 102.50f, 150.21f }, { 952.89f, 102.49f, 150.21f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 952.89f, 102.50f, 150.21f }, { 952.91f, 102.58f, 150.24f }, { 952.90f, 102.58f, 150.23f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 953.42f, 102.70f, 149.88f }, { 953.38f, 102.69f, 149.85f }, { 953.38f, 102.70f, 149.85f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 953.38f, 102.70f, 149.85f }, { 953.42f, 102.71f, 149.88f }, { 953.42f, 102.70f, 149.88f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 953.01f, 102.70f, 150.37f }, { 952.98f, 102.70f, 150.34f }, { 952.98f, 102.69f, 150.34f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 952.98f, 102.70f, 150.34f }, { 953.01f, 102.70f, 150.37f }, { 953.01f, 102.71f, 150.37f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 953.30f, 102.64f, 149.81f }, { 953.27f, 102.58f, 149.80f }, { 953.26f, 102.58f, 149.80f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 953.26f, 102.58f, 149.79f }, { 953.30f, 102.65f, 149.81f }, { 953.30f, 102.64f, 149.81f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 953.30f, 102.65f, 149.81f }, { 953.26f, 102.58f, 149.80f }, { 953.29f, 102.65f, 149.83f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 953.26f, 102.58f, 149.79f }, { 953.25f, 102.58f, 149.81f }, { 953.29f, 102.65f, 149.83f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 953.12f, 102.65f, 150.04f }, { 953.29f, 102.65f, 149.83f }, { 953.25f, 102.58f, 149.81f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 953.12f, 102.65f, 150.04f }, { 953.25f, 102.58f, 149.81f }, { 953.08f, 102.58f, 150.01f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 952.92f, 102.58f, 150.22f }, { 953.11f, 102.65f, 150.04f }, { 953.08f, 102.58f, 150.01f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 953.12f, 102.65f, 150.04f }, { 952.92f, 102.58f, 150.22f }, { 952.94f, 102.65f, 150.25f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 952.94f, 102.65f, 150.25f }, { 952.90f, 102.58f, 150.23f }, { 952.93f, 102.65f, 150.27f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 952.90f, 102.58f, 150.23f }, { 952.94f, 102.65f, 150.25f }, { 952.92f, 102.58f, 150.22f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 952.93f, 102.65f, 150.27f }, { 952.90f, 102.58f, 150.23f }, { 952.93f, 102.64f, 150.27f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 952.91f, 102.58f, 150.24f }, { 952.93f, 102.64f, 150.27f }, { 952.90f, 102.58f, 150.23f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 953.24f, 102.49f, 149.79f }, { 953.21f, 102.40f, 149.79f }, { 953.23f, 102.50f, 149.79f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 953.21f, 102.40f, 149.79f }, { 953.24f, 102.49f, 149.79f }, { 953.22f, 102.40f, 149.79f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 953.23f, 102.50f, 149.79f }, { 953.21f, 102.40f, 149.79f }, { 953.22f, 102.50f, 149.80f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 953.21f, 102.40f, 149.79f }, { 953.20f, 102.40f, 149.80f }, { 953.22f, 102.50f, 149.80f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 953.06f, 102.50f, 150.00f }, { 953.22f, 102.50f, 149.80f }, { 953.20f, 102.40f, 149.80f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 953.06f, 102.50f, 150.00f }, { 953.20f, 102.40f, 149.80f }, { 953.05f, 102.40f, 149.99f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 952.90f, 102.40f, 150.17f }, { 953.06f, 102.50f, 150.00f }, { 953.05f, 102.40f, 149.99f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 953.06f, 102.50f, 150.00f }, { 952.90f, 102.40f, 150.17f }, { 952.90f, 102.50f, 150.19f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 952.90f, 102.50f, 150.19f }, { 952.88f, 102.40f, 150.19f }, { 952.89f, 102.50f, 150.21f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 952.88f, 102.40f, 150.19f }, { 952.90f, 102.50f, 150.19f }, { 952.90f, 102.40f, 150.17f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 952.89f, 102.50f, 150.21f }, { 952.88f, 102.40f, 150.19f }, { 952.89f, 102.49f, 150.21f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 952.89f, 102.49f, 150.21f }, { 952.88f, 102.40f, 150.19f }, { 952.89f, 102.40f, 150.19f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 953.38f, 102.69f, 149.85f }, { 953.34f, 102.67f, 149.83f }, { 953.34f, 102.68f, 149.83f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 953.34f, 102.68f, 149.83f }, { 953.38f, 102.70f, 149.85f }, { 953.38f, 102.69f, 149.85f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 953.33f, 102.68f, 149.84f }, { 953.37f, 102.70f, 149.87f }, { 953.34f, 102.68f, 149.83f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 953.34f, 102.68f, 149.83f }, { 953.37f, 102.70f, 149.87f }, { 953.38f, 102.70f, 149.85f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 953.33f, 102.68f, 149.84f }, { 953.18f, 102.70f, 150.09f }, { 953.37f, 102.70f, 149.87f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 953.33f, 102.68f, 149.84f }, { 953.15f, 102.68f, 150.07f }, { 953.18f, 102.70f, 150.09f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 952.96f, 102.68f, 150.29f }, { 953.18f, 102.70f, 150.09f }, { 953.15f, 102.68f, 150.07f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 953.18f, 102.70f, 150.09f }, { 952.96f, 102.68f, 150.29f }, { 952.99f, 102.70f, 150.32f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 952.95f, 102.68f, 150.30f }, { 952.99f, 102.70f, 150.32f }, { 952.96f, 102.68f, 150.29f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 952.99f, 102.70f, 150.32f }, { 952.95f, 102.68f, 150.30f }, { 952.98f, 102.70f, 150.34f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 952.98f, 102.70f, 150.34f }, { 952.95f, 102.68f, 150.30f }, { 952.98f, 102.69f, 150.34f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 952.95f, 102.67f, 150.30f }, { 952.98f, 102.69f, 150.34f }, { 952.95f, 102.68f, 150.30f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 953.65f, 102.71f, 150.09f }, { 953.42f, 102.71f, 149.88f }, { 953.41f, 102.71f, 149.89f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 953.42f, 102.71f, 149.88f }, { 953.65f, 102.71f, 150.09f }, { 953.66f, 102.71f, 150.08f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 953.41f, 102.71f, 149.89f }, { 953.46f, 102.71f, 150.32f }, { 953.65f, 102.71f, 150.09f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 953.22f, 102.71f, 150.12f }, { 953.46f, 102.71f, 150.32f }, { 953.41f, 102.71f, 149.89f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 953.02f, 102.71f, 150.35f }, { 953.46f, 102.71f, 150.32f }, { 953.22f, 102.71f, 150.12f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 953.46f, 102.71f, 150.32f }, { 953.02f, 102.71f, 150.35f }, { 953.27f, 102.71f, 150.56f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 953.01f, 102.71f, 150.37f }, { 953.26f, 102.71f, 150.57f }, { 953.02f, 102.71f, 150.35f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 953.02f, 102.71f, 150.35f }, { 953.26f, 102.71f, 150.57f }, { 953.27f, 102.71f, 150.56f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 953.42f, 102.71f, 149.88f }, { 953.66f, 102.71f, 150.08f }, { 953.42f, 102.70f, 149.88f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 953.42f, 102.70f, 149.88f }, { 953.66f, 102.71f, 150.08f }, { 953.66f, 102.70f, 150.08f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 953.01f, 102.70f, 150.37f }, { 953.26f, 102.71f, 150.57f }, { 953.01f, 102.71f, 150.37f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 953.26f, 102.71f, 150.57f }, { 953.01f, 102.70f, 150.37f }, { 953.26f, 102.70f, 150.57f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 948.19f, 102.58f, 155.98f }, { 948.16f, 102.50f, 155.97f }, { 948.18f, 102.58f, 155.99f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 948.16f, 102.50f, 155.97f }, { 948.15f, 102.50f, 155.99f }, { 948.18f, 102.58f, 155.99f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 948.29f, 102.70f, 156.05f }, { 948.33f, 102.71f, 156.07f }, { 948.31f, 102.70f, 156.04f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 948.30f, 102.70f, 156.04f }, { 948.33f, 102.71f, 156.07f }, { 948.34f, 102.71f, 156.06f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 948.14f, 102.40f, 155.97f }, { 948.13f, 102.29f, 155.97f }, { 948.13f, 102.40f, 155.98f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 948.13f, 102.29f, 155.97f }, { 948.12f, 102.29f, 155.99f }, { 948.13f, 102.40f, 155.98f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 948.22f, 102.65f, 156.01f }, { 948.25f, 102.68f, 156.03f }, { 948.23f, 102.65f, 156.00f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 948.23f, 102.65f, 156.00f }, { 948.25f, 102.68f, 156.03f }, { 948.27f, 102.68f, 156.01f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 948.12f, 102.29f, 155.99f }, { 947.97f, 102.40f, 156.17f }, { 948.13f, 102.40f, 155.98f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 947.97f, 102.40f, 156.17f }, { 948.12f, 102.29f, 155.99f }, { 947.97f, 102.29f, 156.17f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 947.97f, 102.29f, 156.17f }, { 947.82f, 102.29f, 156.34f }, { 947.97f, 102.40f, 156.17f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 947.82f, 102.40f, 156.36f }, { 947.97f, 102.40f, 156.17f }, { 947.82f, 102.29f, 156.34f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 948.15f, 102.50f, 155.99f }, { 948.01f, 102.58f, 156.20f }, { 948.18f, 102.58f, 155.99f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 948.01f, 102.58f, 156.20f }, { 948.15f, 102.50f, 155.99f }, { 947.99f, 102.50f, 156.18f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 947.99f, 102.50f, 156.18f }, { 947.83f, 102.50f, 156.38f }, { 948.01f, 102.58f, 156.20f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 947.84f, 102.58f, 156.40f }, { 948.01f, 102.58f, 156.20f }, { 947.83f, 102.50f, 156.38f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 948.22f, 102.65f, 156.01f }, { 948.07f, 102.68f, 156.25f }, { 948.25f, 102.68f, 156.03f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 948.07f, 102.68f, 156.25f }, { 948.21f, 102.65f, 156.01f }, { 948.04f, 102.65f, 156.22f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 947.86f, 102.65f, 156.44f }, { 948.07f, 102.68f, 156.25f }, { 948.04f, 102.65f, 156.22f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 948.07f, 102.68f, 156.25f }, { 947.86f, 102.65f, 156.44f }, { 947.89f, 102.68f, 156.47f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 948.29f, 102.70f, 156.05f }, { 948.14f, 102.71f, 156.31f }, { 948.33f, 102.71f, 156.07f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 948.14f, 102.71f, 156.31f }, { 948.29f, 102.70f, 156.05f }, { 948.11f, 102.70f, 156.28f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 947.92f, 102.70f, 156.51f }, { 948.14f, 102.71f, 156.31f }, { 948.11f, 102.70f, 156.28f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 948.14f, 102.71f, 156.31f }, { 947.92f, 102.70f, 156.51f }, { 947.95f, 102.71f, 156.54f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 947.81f, 102.29f, 156.36f }, { 947.81f, 102.40f, 156.37f }, { 947.82f, 102.40f, 156.36f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 947.82f, 102.40f, 156.36f }, { 947.82f, 102.29f, 156.34f }, { 947.81f, 102.29f, 156.36f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 947.81f, 102.50f, 156.39f }, { 947.83f, 102.58f, 156.42f }, { 947.84f, 102.58f, 156.40f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 947.84f, 102.58f, 156.40f }, { 947.83f, 102.50f, 156.38f }, { 947.81f, 102.50f, 156.39f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 947.85f, 102.65f, 156.45f }, { 947.89f, 102.68f, 156.47f }, { 947.86f, 102.65f, 156.44f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 947.89f, 102.68f, 156.47f }, { 947.85f, 102.65f, 156.45f }, { 947.87f, 102.68f, 156.49f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 947.91f, 102.70f, 156.52f }, { 947.95f, 102.71f, 156.54f }, { 947.92f, 102.70f, 156.51f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 947.95f, 102.71f, 156.54f }, { 947.91f, 102.70f, 156.52f }, { 947.94f, 102.71f, 156.55f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 948.27f, 102.67f, 156.01f }, { 948.23f, 102.64f, 156.00f }, { 948.23f, 102.65f, 156.00f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 948.23f, 102.65f, 156.00f }, { 948.27f, 102.68f, 156.01f }, { 948.27f, 102.67f, 156.01f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 947.88f, 102.67f, 156.49f }, { 947.85f, 102.65f, 156.45f }, { 947.85f, 102.64f, 156.45f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 947.85f, 102.65f, 156.45f }, { 947.88f, 102.67f, 156.49f }, { 947.87f, 102.68f, 156.49f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 948.14f, 102.40f, 155.97f }, { 948.13f, 102.29f, 155.97f }, { 948.14f, 102.40f, 155.97f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 948.13f, 102.29f, 155.97f }, { 948.14f, 102.40f, 155.97f }, { 948.13f, 102.29f, 155.98f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 947.81f, 102.40f, 156.38f }, { 947.81f, 102.29f, 156.36f }, { 947.81f, 102.29f, 156.36f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 947.81f, 102.29f, 156.36f }, { 947.81f, 102.40f, 156.37f }, { 947.81f, 102.40f, 156.37f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 948.19f, 102.58f, 155.98f }, { 948.16f, 102.49f, 155.97f }, { 948.16f, 102.50f, 155.97f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 948.16f, 102.50f, 155.97f }, { 948.19f, 102.58f, 155.98f }, { 948.19f, 102.58f, 155.98f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 947.83f, 102.58f, 156.42f }, { 947.81f, 102.50f, 156.39f }, { 947.82f, 102.49f, 156.39f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 947.81f, 102.50f, 156.39f }, { 947.83f, 102.58f, 156.42f }, { 947.83f, 102.58f, 156.42f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 948.34f, 102.70f, 156.06f }, { 948.31f, 102.69f, 156.04f }, { 948.31f, 102.70f, 156.04f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 948.30f, 102.70f, 156.04f }, { 948.34f, 102.71f, 156.06f }, { 948.34f, 102.70f, 156.06f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 947.94f, 102.70f, 156.56f }, { 947.91f, 102.70f, 156.52f }, { 947.91f, 102.69f, 156.52f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 947.91f, 102.70f, 156.52f }, { 947.94f, 102.70f, 156.55f }, { 947.94f, 102.71f, 156.55f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 948.23f, 102.64f, 156.00f }, { 948.19f, 102.58f, 155.98f }, { 948.19f, 102.58f, 155.98f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 948.19f, 102.58f, 155.98f }, { 948.23f, 102.65f, 156.00f }, { 948.23f, 102.64f, 156.00f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 948.23f, 102.65f, 156.00f }, { 948.19f, 102.58f, 155.98f }, { 948.21f, 102.65f, 156.01f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 948.19f, 102.58f, 155.98f }, { 948.18f, 102.58f, 155.99f }, { 948.21f, 102.65f, 156.01f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 948.04f, 102.65f, 156.22f }, { 948.21f, 102.65f, 156.01f }, { 948.18f, 102.58f, 155.99f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 948.04f, 102.65f, 156.22f }, { 948.18f, 102.58f, 155.99f }, { 948.01f, 102.58f, 156.20f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 947.84f, 102.58f, 156.40f }, { 948.04f, 102.65f, 156.22f }, { 948.01f, 102.58f, 156.20f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 948.04f, 102.65f, 156.22f }, { 947.84f, 102.58f, 156.40f }, { 947.86f, 102.65f, 156.44f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 947.86f, 102.65f, 156.44f }, { 947.83f, 102.58f, 156.42f }, { 947.85f, 102.65f, 156.45f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 947.83f, 102.58f, 156.42f }, { 947.86f, 102.65f, 156.44f }, { 947.84f, 102.58f, 156.40f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 947.85f, 102.65f, 156.45f }, { 947.83f, 102.58f, 156.42f }, { 947.85f, 102.64f, 156.45f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 947.83f, 102.58f, 156.42f }, { 947.85f, 102.64f, 156.45f }, { 947.83f, 102.58f, 156.42f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 948.16f, 102.49f, 155.97f }, { 948.14f, 102.40f, 155.97f }, { 948.16f, 102.50f, 155.97f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 948.14f, 102.40f, 155.97f }, { 948.16f, 102.49f, 155.97f }, { 948.14f, 102.40f, 155.97f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 948.16f, 102.50f, 155.97f }, { 948.14f, 102.40f, 155.97f }, { 948.15f, 102.50f, 155.99f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 948.14f, 102.40f, 155.97f }, { 948.13f, 102.40f, 155.98f }, { 948.15f, 102.50f, 155.99f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 947.99f, 102.50f, 156.18f }, { 948.15f, 102.50f, 155.99f }, { 948.13f, 102.40f, 155.98f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 947.99f, 102.50f, 156.18f }, { 948.13f, 102.40f, 155.98f }, { 947.97f, 102.40f, 156.17f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 947.82f, 102.40f, 156.36f }, { 947.99f, 102.50f, 156.18f }, { 947.97f, 102.40f, 156.17f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 947.99f, 102.50f, 156.18f }, { 947.82f, 102.40f, 156.36f }, { 947.83f, 102.50f, 156.38f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 947.83f, 102.50f, 156.38f }, { 947.81f, 102.40f, 156.37f }, { 947.81f, 102.50f, 156.39f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 947.81f, 102.40f, 156.37f }, { 947.83f, 102.50f, 156.38f }, { 947.82f, 102.40f, 156.36f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 947.81f, 102.50f, 156.39f }, { 947.81f, 102.40f, 156.37f }, { 947.82f, 102.49f, 156.39f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 947.82f, 102.49f, 156.39f }, { 947.81f, 102.40f, 156.37f }, { 947.81f, 102.40f, 156.37f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 948.31f, 102.69f, 156.04f }, { 948.27f, 102.67f, 156.01f }, { 948.27f, 102.68f, 156.01f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 948.27f, 102.68f, 156.01f }, { 948.31f, 102.70f, 156.04f }, { 948.31f, 102.69f, 156.04f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 948.25f, 102.68f, 156.03f }, { 948.29f, 102.70f, 156.05f }, { 948.27f, 102.68f, 156.01f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 948.27f, 102.68f, 156.01f }, { 948.29f, 102.70f, 156.05f }, { 948.31f, 102.70f, 156.04f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 948.25f, 102.68f, 156.03f }, { 948.11f, 102.70f, 156.28f }, { 948.29f, 102.70f, 156.05f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 948.25f, 102.68f, 156.03f }, { 948.07f, 102.68f, 156.25f }, { 948.11f, 102.70f, 156.28f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 947.89f, 102.68f, 156.47f }, { 948.11f, 102.70f, 156.28f }, { 948.07f, 102.68f, 156.25f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 948.11f, 102.70f, 156.28f }, { 947.89f, 102.68f, 156.47f }, { 947.92f, 102.70f, 156.51f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 947.88f, 102.68f, 156.49f }, { 947.92f, 102.70f, 156.51f }, { 947.89f, 102.68f, 156.47f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 947.92f, 102.70f, 156.51f }, { 947.87f, 102.68f, 156.49f }, { 947.91f, 102.70f, 156.52f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 947.91f, 102.70f, 156.52f }, { 947.87f, 102.68f, 156.49f }, { 947.91f, 102.69f, 156.52f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 947.88f, 102.67f, 156.49f }, { 947.91f, 102.69f, 156.52f }, { 947.87f, 102.68f, 156.49f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 948.58f, 102.71f, 156.28f }, { 948.34f, 102.71f, 156.06f }, { 948.33f, 102.71f, 156.07f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 948.34f, 102.71f, 156.06f }, { 948.58f, 102.71f, 156.28f }, { 948.59f, 102.71f, 156.26f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 948.33f, 102.71f, 156.07f }, { 948.39f, 102.71f, 156.51f }, { 948.58f, 102.71f, 156.28f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 948.14f, 102.71f, 156.31f }, { 948.39f, 102.71f, 156.51f }, { 948.33f, 102.71f, 156.07f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 947.95f, 102.71f, 156.54f }, { 948.39f, 102.71f, 156.51f }, { 948.14f, 102.71f, 156.31f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 948.39f, 102.71f, 156.51f }, { 947.95f, 102.71f, 156.54f }, { 948.19f, 102.71f, 156.74f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 947.94f, 102.71f, 156.55f }, { 948.18f, 102.71f, 156.76f }, { 947.95f, 102.71f, 156.54f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 947.95f, 102.71f, 156.54f }, { 948.18f, 102.71f, 156.76f }, { 948.19f, 102.71f, 156.74f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 948.34f, 102.71f, 156.06f }, { 948.59f, 102.71f, 156.26f }, { 948.34f, 102.70f, 156.06f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 948.34f, 102.70f, 156.06f }, { 948.59f, 102.71f, 156.26f }, { 948.59f, 102.70f, 156.26f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 947.94f, 102.70f, 156.56f }, { 948.18f, 102.71f, 156.76f }, { 947.94f, 102.71f, 156.55f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 948.18f, 102.71f, 156.76f }, { 947.94f, 102.70f, 156.55f }, { 948.18f, 102.70f, 156.76f }, 102.29f, 0.00f },   // waterfeature_02 props 417
    { { 1022.14f, 77.22f, 67.18f }, { 1022.12f, 77.05f, 67.24f }, { 1022.17f, 77.22f, 67.20f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.12f, 77.05f, 67.24f }, { 1022.15f, 77.05f, 67.26f }, { 1022.17f, 77.22f, 67.20f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.28f, 77.45f, 66.97f }, { 1022.33f, 77.48f, 66.89f }, { 1022.25f, 77.45f, 66.94f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.25f, 77.45f, 66.94f }, { 1022.33f, 77.48f, 66.89f }, { 1022.30f, 77.48f, 66.87f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.12f, 76.85f, 67.28f }, { 1022.13f, 76.63f, 67.30f }, { 1022.15f, 76.85f, 67.30f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.13f, 76.63f, 67.30f }, { 1022.15f, 76.63f, 67.32f }, { 1022.15f, 76.85f, 67.30f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.20f, 77.35f, 67.12f }, { 1022.23f, 77.41f, 67.05f }, { 1022.17f, 77.35f, 67.10f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.17f, 77.35f, 67.10f }, { 1022.23f, 77.41f, 67.05f }, { 1022.20f, 77.41f, 67.02f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.15f, 76.63f, 67.32f }, { 1022.52f, 76.85f, 67.60f }, { 1022.15f, 76.85f, 67.30f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.52f, 76.85f, 67.60f }, { 1022.15f, 76.63f, 67.32f }, { 1022.51f, 76.63f, 67.61f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.51f, 76.63f, 67.61f }, { 1022.87f, 76.63f, 67.91f }, { 1022.52f, 76.85f, 67.60f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.89f, 76.85f, 67.91f }, { 1022.52f, 76.85f, 67.60f }, { 1022.87f, 76.63f, 67.91f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.15f, 77.05f, 67.26f }, { 1022.58f, 77.22f, 67.54f }, { 1022.17f, 77.22f, 67.20f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.58f, 77.22f, 67.54f }, { 1022.15f, 77.05f, 67.26f }, { 1022.54f, 77.05f, 67.58f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.54f, 77.05f, 67.58f }, { 1022.93f, 77.05f, 67.90f }, { 1022.58f, 77.22f, 67.54f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.99f, 77.22f, 67.87f }, { 1022.58f, 77.22f, 67.54f }, { 1022.93f, 77.05f, 67.90f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.20f, 77.35f, 67.12f }, { 1022.68f, 77.41f, 67.41f }, { 1022.23f, 77.41f, 67.05f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.68f, 77.41f, 67.41f }, { 1022.20f, 77.35f, 67.12f }, { 1022.63f, 77.35f, 67.47f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1023.05f, 77.35f, 67.83f }, { 1022.68f, 77.41f, 67.41f }, { 1022.63f, 77.35f, 67.47f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.68f, 77.41f, 67.41f }, { 1023.05f, 77.35f, 67.83f }, { 1023.12f, 77.41f, 67.78f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.28f, 77.45f, 66.97f }, { 1022.79f, 77.48f, 67.27f }, { 1022.33f, 77.48f, 66.89f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.79f, 77.48f, 67.27f }, { 1022.28f, 77.45f, 66.97f }, { 1022.74f, 77.45f, 67.34f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1023.19f, 77.45f, 67.72f }, { 1022.79f, 77.48f, 67.27f }, { 1022.74f, 77.45f, 67.34f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.79f, 77.48f, 67.27f }, { 1023.19f, 77.45f, 67.71f }, { 1023.26f, 77.48f, 67.66f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.90f, 76.63f, 67.93f }, { 1022.92f, 76.85f, 67.94f }, { 1022.89f, 76.85f, 67.91f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.89f, 76.85f, 67.91f }, { 1022.87f, 76.63f, 67.91f }, { 1022.90f, 76.63f, 67.93f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.96f, 77.05f, 67.92f }, { 1023.02f, 77.22f, 67.90f }, { 1022.98f, 77.22f, 67.87f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.99f, 77.22f, 67.87f }, { 1022.93f, 77.05f, 67.90f }, { 1022.96f, 77.05f, 67.92f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1023.08f, 77.35f, 67.85f }, { 1023.12f, 77.41f, 67.78f }, { 1023.05f, 77.35f, 67.83f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1023.12f, 77.41f, 67.78f }, { 1023.08f, 77.35f, 67.85f }, { 1023.15f, 77.41f, 67.80f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1023.22f, 77.45f, 67.74f }, { 1023.26f, 77.48f, 67.66f }, { 1023.19f, 77.45f, 67.71f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1023.26f, 77.48f, 67.66f }, { 1023.22f, 77.45f, 67.74f }, { 1023.29f, 77.48f, 67.68f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.20f, 77.39f, 67.02f }, { 1022.17f, 77.33f, 67.10f }, { 1022.17f, 77.35f, 67.10f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.17f, 77.35f, 67.10f }, { 1022.20f, 77.41f, 67.02f }, { 1022.20f, 77.39f, 67.02f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1023.16f, 77.39f, 67.80f }, { 1023.08f, 77.35f, 67.85f }, { 1023.09f, 77.33f, 67.85f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1023.08f, 77.35f, 67.85f }, { 1023.16f, 77.39f, 67.80f }, { 1023.15f, 77.41f, 67.80f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.12f, 76.84f, 67.27f }, { 1022.13f, 76.63f, 67.30f }, { 1022.12f, 76.85f, 67.27f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.13f, 76.63f, 67.30f }, { 1022.12f, 76.85f, 67.27f }, { 1022.13f, 76.63f, 67.29f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.93f, 76.84f, 67.93f }, { 1022.90f, 76.63f, 67.93f }, { 1022.90f, 76.63f, 67.92f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.90f, 76.63f, 67.93f }, { 1022.93f, 76.85f, 67.93f }, { 1022.92f, 76.85f, 67.94f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.14f, 77.20f, 67.17f }, { 1022.13f, 77.03f, 67.23f }, { 1022.12f, 77.05f, 67.24f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.12f, 77.05f, 67.24f }, { 1022.14f, 77.22f, 67.18f }, { 1022.14f, 77.20f, 67.17f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1023.02f, 77.20f, 67.89f }, { 1022.96f, 77.05f, 67.92f }, { 1022.96f, 77.03f, 67.92f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.96f, 77.05f, 67.92f }, { 1023.02f, 77.20f, 67.89f }, { 1023.02f, 77.22f, 67.90f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.30f, 77.45f, 66.87f }, { 1022.25f, 77.43f, 66.94f }, { 1022.25f, 77.45f, 66.94f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.25f, 77.45f, 66.94f }, { 1022.30f, 77.48f, 66.87f }, { 1022.30f, 77.45f, 66.87f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1023.29f, 77.45f, 67.68f }, { 1023.22f, 77.45f, 67.74f }, { 1023.22f, 77.43f, 67.74f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1023.22f, 77.45f, 67.74f }, { 1023.29f, 77.45f, 67.68f }, { 1023.29f, 77.48f, 67.68f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.17f, 77.33f, 67.10f }, { 1022.14f, 77.20f, 67.17f }, { 1022.14f, 77.22f, 67.18f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.14f, 77.22f, 67.18f }, { 1022.17f, 77.35f, 67.10f }, { 1022.17f, 77.33f, 67.10f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.17f, 77.35f, 67.10f }, { 1022.14f, 77.22f, 67.18f }, { 1022.20f, 77.35f, 67.12f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.14f, 77.22f, 67.18f }, { 1022.17f, 77.22f, 67.20f }, { 1022.20f, 77.35f, 67.12f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.63f, 77.35f, 67.47f }, { 1022.20f, 77.35f, 67.12f }, { 1022.17f, 77.22f, 67.20f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.63f, 77.35f, 67.47f }, { 1022.17f, 77.22f, 67.20f }, { 1022.58f, 77.22f, 67.54f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.99f, 77.22f, 67.87f }, { 1022.63f, 77.35f, 67.47f }, { 1022.58f, 77.22f, 67.54f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.63f, 77.35f, 67.47f }, { 1022.98f, 77.22f, 67.87f }, { 1023.05f, 77.35f, 67.83f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1023.05f, 77.35f, 67.83f }, { 1023.02f, 77.22f, 67.90f }, { 1023.08f, 77.35f, 67.85f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1023.01f, 77.22f, 67.90f }, { 1023.05f, 77.35f, 67.83f }, { 1022.98f, 77.22f, 67.87f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1023.08f, 77.35f, 67.85f }, { 1023.02f, 77.22f, 67.90f }, { 1023.09f, 77.33f, 67.85f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1023.02f, 77.20f, 67.89f }, { 1023.09f, 77.33f, 67.85f }, { 1023.02f, 77.22f, 67.90f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.13f, 77.03f, 67.23f }, { 1022.12f, 76.85f, 67.27f }, { 1022.12f, 77.05f, 67.24f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.12f, 76.85f, 67.28f }, { 1022.13f, 77.03f, 67.23f }, { 1022.12f, 76.85f, 67.27f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.12f, 77.05f, 67.24f }, { 1022.12f, 76.85f, 67.27f }, { 1022.15f, 77.05f, 67.26f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.12f, 76.85f, 67.28f }, { 1022.15f, 76.85f, 67.30f }, { 1022.15f, 77.05f, 67.26f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.54f, 77.05f, 67.58f }, { 1022.15f, 77.05f, 67.26f }, { 1022.15f, 76.85f, 67.30f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.54f, 77.05f, 67.58f }, { 1022.15f, 76.85f, 67.30f }, { 1022.52f, 76.85f, 67.60f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.89f, 76.85f, 67.91f }, { 1022.54f, 77.05f, 67.58f }, { 1022.52f, 76.85f, 67.60f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.54f, 77.05f, 67.58f }, { 1022.89f, 76.85f, 67.91f }, { 1022.93f, 77.05f, 67.90f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.93f, 77.05f, 67.90f }, { 1022.92f, 76.85f, 67.94f }, { 1022.96f, 77.05f, 67.92f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.92f, 76.85f, 67.94f }, { 1022.93f, 77.05f, 67.90f }, { 1022.89f, 76.85f, 67.91f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.96f, 77.05f, 67.92f }, { 1022.92f, 76.85f, 67.94f }, { 1022.96f, 77.03f, 67.92f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.96f, 77.03f, 67.92f }, { 1022.92f, 76.85f, 67.94f }, { 1022.93f, 76.85f, 67.93f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.25f, 77.43f, 66.94f }, { 1022.20f, 77.39f, 67.02f }, { 1022.20f, 77.41f, 67.02f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.20f, 77.41f, 67.02f }, { 1022.25f, 77.45f, 66.94f }, { 1022.25f, 77.43f, 66.94f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.23f, 77.41f, 67.05f }, { 1022.28f, 77.45f, 66.97f }, { 1022.20f, 77.41f, 67.02f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.20f, 77.41f, 67.02f }, { 1022.28f, 77.45f, 66.97f }, { 1022.25f, 77.45f, 66.94f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.23f, 77.41f, 67.05f }, { 1022.74f, 77.45f, 67.34f }, { 1022.28f, 77.45f, 66.97f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.23f, 77.41f, 67.05f }, { 1022.68f, 77.41f, 67.41f }, { 1022.74f, 77.45f, 67.34f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1023.12f, 77.41f, 67.78f }, { 1022.74f, 77.45f, 67.34f }, { 1022.68f, 77.41f, 67.41f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.74f, 77.45f, 67.34f }, { 1023.12f, 77.41f, 67.78f }, { 1023.19f, 77.45f, 67.71f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1023.15f, 77.41f, 67.80f }, { 1023.19f, 77.45f, 67.71f }, { 1023.12f, 77.41f, 67.78f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1023.19f, 77.45f, 67.72f }, { 1023.15f, 77.41f, 67.80f }, { 1023.22f, 77.45f, 67.74f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1023.22f, 77.45f, 67.74f }, { 1023.15f, 77.41f, 67.80f }, { 1023.22f, 77.43f, 67.74f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1023.16f, 77.39f, 67.80f }, { 1023.22f, 77.43f, 67.74f }, { 1023.15f, 77.41f, 67.80f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.73f, 77.48f, 66.40f }, { 1022.30f, 77.48f, 66.87f }, { 1022.33f, 77.48f, 66.89f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.30f, 77.48f, 66.87f }, { 1022.73f, 77.48f, 66.40f }, { 1022.70f, 77.48f, 66.38f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.33f, 77.48f, 66.89f }, { 1023.19f, 77.48f, 66.78f }, { 1022.73f, 77.48f, 66.40f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.79f, 77.48f, 67.27f }, { 1023.19f, 77.48f, 66.78f }, { 1022.33f, 77.48f, 66.89f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1023.26f, 77.48f, 67.66f }, { 1023.19f, 77.48f, 66.78f }, { 1022.79f, 77.48f, 67.27f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1023.19f, 77.48f, 66.78f }, { 1023.26f, 77.48f, 67.66f }, { 1023.66f, 77.48f, 67.16f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1023.29f, 77.48f, 67.68f }, { 1023.69f, 77.48f, 67.19f }, { 1023.26f, 77.48f, 67.66f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1023.26f, 77.48f, 67.66f }, { 1023.69f, 77.48f, 67.19f }, { 1023.66f, 77.48f, 67.16f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.30f, 77.48f, 66.87f }, { 1022.70f, 77.48f, 66.38f }, { 1022.30f, 77.45f, 66.87f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1022.30f, 77.45f, 66.87f }, { 1022.70f, 77.48f, 66.38f }, { 1022.70f, 77.45f, 66.38f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1023.29f, 77.45f, 67.68f }, { 1023.69f, 77.48f, 67.19f }, { 1023.29f, 77.48f, 67.68f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1023.69f, 77.48f, 67.19f }, { 1023.29f, 77.45f, 67.68f }, { 1023.69f, 77.45f, 67.19f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1024.98f, 77.22f, 69.51f }, { 1024.97f, 77.05f, 69.57f }, { 1025.01f, 77.22f, 69.53f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1024.97f, 77.05f, 69.57f }, { 1024.99f, 77.05f, 69.60f }, { 1025.01f, 77.22f, 69.53f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.12f, 77.45f, 69.30f }, { 1025.17f, 77.48f, 69.23f }, { 1025.10f, 77.45f, 69.28f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.10f, 77.45f, 69.28f }, { 1025.17f, 77.48f, 69.23f }, { 1025.14f, 77.48f, 69.20f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1024.96f, 76.85f, 69.61f }, { 1024.97f, 76.63f, 69.63f }, { 1024.99f, 76.85f, 69.63f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1024.97f, 76.63f, 69.63f }, { 1025.00f, 76.63f, 69.66f }, { 1024.99f, 76.85f, 69.63f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.04f, 77.35f, 69.46f }, { 1025.08f, 77.41f, 69.38f }, { 1025.01f, 77.35f, 69.43f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.02f, 77.35f, 69.44f }, { 1025.08f, 77.41f, 69.38f }, { 1025.05f, 77.41f, 69.36f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.00f, 76.63f, 69.66f }, { 1025.36f, 76.85f, 69.94f }, { 1024.99f, 76.85f, 69.63f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.37f, 76.85f, 69.94f }, { 1025.00f, 76.63f, 69.66f }, { 1025.36f, 76.63f, 69.95f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.36f, 76.63f, 69.95f }, { 1025.71f, 76.63f, 70.24f }, { 1025.36f, 76.85f, 69.94f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.74f, 76.85f, 70.25f }, { 1025.36f, 76.85f, 69.94f }, { 1025.71f, 76.63f, 70.24f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1024.99f, 77.05f, 69.59f }, { 1025.42f, 77.22f, 69.87f }, { 1025.01f, 77.22f, 69.53f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.42f, 77.22f, 69.87f }, { 1024.99f, 77.05f, 69.60f }, { 1025.38f, 77.05f, 69.91f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.38f, 77.05f, 69.92f }, { 1025.77f, 77.05f, 70.23f }, { 1025.42f, 77.22f, 69.87f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.83f, 77.22f, 70.21f }, { 1025.42f, 77.22f, 69.87f }, { 1025.77f, 77.05f, 70.23f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.04f, 77.35f, 69.46f }, { 1025.52f, 77.41f, 69.75f }, { 1025.08f, 77.41f, 69.38f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.52f, 77.41f, 69.75f }, { 1025.04f, 77.35f, 69.46f }, { 1025.47f, 77.35f, 69.81f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.90f, 77.35f, 70.16f }, { 1025.52f, 77.41f, 69.75f }, { 1025.47f, 77.35f, 69.81f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.52f, 77.41f, 69.75f }, { 1025.90f, 77.35f, 70.16f }, { 1025.97f, 77.41f, 70.11f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.12f, 77.45f, 69.30f }, { 1025.64f, 77.48f, 69.61f }, { 1025.17f, 77.48f, 69.23f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.64f, 77.48f, 69.61f }, { 1025.13f, 77.45f, 69.30f }, { 1025.58f, 77.45f, 69.68f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1026.04f, 77.45f, 70.05f }, { 1025.64f, 77.48f, 69.61f }, { 1025.58f, 77.45f, 69.68f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.64f, 77.48f, 69.61f }, { 1026.04f, 77.45f, 70.05f }, { 1026.10f, 77.48f, 69.99f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.74f, 76.63f, 70.27f }, { 1025.77f, 76.85f, 70.27f }, { 1025.74f, 76.85f, 70.25f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.74f, 76.85f, 70.25f }, { 1025.71f, 76.63f, 70.24f }, { 1025.74f, 76.63f, 70.27f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.80f, 77.05f, 70.26f }, { 1025.86f, 77.22f, 70.23f }, { 1025.83f, 77.22f, 70.21f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.83f, 77.22f, 70.21f }, { 1025.77f, 77.05f, 70.23f }, { 1025.80f, 77.05f, 70.26f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.93f, 77.35f, 70.19f }, { 1025.97f, 77.41f, 70.11f }, { 1025.90f, 77.35f, 70.16f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.97f, 77.41f, 70.11f }, { 1025.93f, 77.35f, 70.18f }, { 1026.00f, 77.41f, 70.14f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1026.07f, 77.45f, 70.07f }, { 1026.10f, 77.48f, 69.99f }, { 1026.04f, 77.45f, 70.05f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1026.10f, 77.48f, 69.99f }, { 1026.07f, 77.45f, 70.07f }, { 1026.13f, 77.48f, 70.01f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.05f, 77.39f, 69.36f }, { 1025.02f, 77.33f, 69.43f }, { 1025.01f, 77.35f, 69.43f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.02f, 77.35f, 69.44f }, { 1025.05f, 77.41f, 69.36f }, { 1025.05f, 77.39f, 69.36f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1026.00f, 77.39f, 70.14f }, { 1025.93f, 77.35f, 70.18f }, { 1025.93f, 77.33f, 70.18f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.93f, 77.35f, 70.19f }, { 1026.00f, 77.39f, 70.14f }, { 1026.00f, 77.41f, 70.14f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1024.97f, 76.84f, 69.60f }, { 1024.97f, 76.63f, 69.63f }, { 1024.96f, 76.85f, 69.61f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1024.97f, 76.63f, 69.63f }, { 1024.97f, 76.85f, 69.60f }, { 1024.98f, 76.63f, 69.63f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.77f, 76.84f, 70.26f }, { 1025.74f, 76.63f, 70.27f }, { 1025.75f, 76.63f, 70.26f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.74f, 76.63f, 70.27f }, { 1025.77f, 76.85f, 70.26f }, { 1025.77f, 76.85f, 70.27f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1024.99f, 77.20f, 69.51f }, { 1024.97f, 77.03f, 69.57f }, { 1024.97f, 77.05f, 69.57f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1024.97f, 77.05f, 69.57f }, { 1024.98f, 77.22f, 69.51f }, { 1024.99f, 77.20f, 69.51f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.86f, 77.20f, 70.23f }, { 1025.80f, 77.05f, 70.26f }, { 1025.81f, 77.03f, 70.25f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.80f, 77.05f, 70.26f }, { 1025.86f, 77.20f, 70.23f }, { 1025.86f, 77.22f, 70.23f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.14f, 77.45f, 69.20f }, { 1025.10f, 77.43f, 69.28f }, { 1025.10f, 77.45f, 69.28f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.10f, 77.45f, 69.28f }, { 1025.14f, 77.48f, 69.20f }, { 1025.14f, 77.45f, 69.20f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1026.13f, 77.45f, 70.01f }, { 1026.07f, 77.45f, 70.07f }, { 1026.07f, 77.43f, 70.07f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1026.07f, 77.45f, 70.07f }, { 1026.13f, 77.45f, 70.01f }, { 1026.13f, 77.48f, 70.01f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.02f, 77.33f, 69.43f }, { 1024.99f, 77.20f, 69.51f }, { 1024.98f, 77.22f, 69.51f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1024.98f, 77.22f, 69.51f }, { 1025.01f, 77.35f, 69.43f }, { 1025.02f, 77.33f, 69.43f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.02f, 77.35f, 69.44f }, { 1024.98f, 77.22f, 69.51f }, { 1025.04f, 77.35f, 69.46f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1024.98f, 77.22f, 69.51f }, { 1025.01f, 77.22f, 69.53f }, { 1025.04f, 77.35f, 69.46f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.47f, 77.35f, 69.81f }, { 1025.04f, 77.35f, 69.46f }, { 1025.01f, 77.22f, 69.53f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.47f, 77.35f, 69.81f }, { 1025.01f, 77.22f, 69.53f }, { 1025.42f, 77.22f, 69.87f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.83f, 77.22f, 70.21f }, { 1025.47f, 77.35f, 69.81f }, { 1025.42f, 77.22f, 69.87f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.47f, 77.35f, 69.81f }, { 1025.83f, 77.22f, 70.21f }, { 1025.90f, 77.35f, 70.16f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.90f, 77.35f, 70.16f }, { 1025.86f, 77.22f, 70.23f }, { 1025.93f, 77.35f, 70.18f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.86f, 77.22f, 70.23f }, { 1025.90f, 77.35f, 70.16f }, { 1025.83f, 77.22f, 70.21f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.93f, 77.35f, 70.19f }, { 1025.86f, 77.22f, 70.23f }, { 1025.93f, 77.33f, 70.18f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.86f, 77.20f, 70.23f }, { 1025.93f, 77.33f, 70.18f }, { 1025.86f, 77.22f, 70.23f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1024.97f, 77.03f, 69.57f }, { 1024.96f, 76.85f, 69.61f }, { 1024.97f, 77.05f, 69.57f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1024.96f, 76.85f, 69.61f }, { 1024.97f, 77.03f, 69.57f }, { 1024.97f, 76.85f, 69.60f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1024.97f, 77.05f, 69.57f }, { 1024.96f, 76.85f, 69.61f }, { 1024.99f, 77.05f, 69.60f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1024.96f, 76.85f, 69.61f }, { 1024.99f, 76.85f, 69.63f }, { 1024.99f, 77.05f, 69.60f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.38f, 77.05f, 69.92f }, { 1024.99f, 77.05f, 69.60f }, { 1024.99f, 76.85f, 69.63f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.38f, 77.05f, 69.92f }, { 1024.99f, 76.85f, 69.63f }, { 1025.36f, 76.85f, 69.94f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.74f, 76.85f, 70.25f }, { 1025.38f, 77.05f, 69.91f }, { 1025.36f, 76.85f, 69.94f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.38f, 77.05f, 69.92f }, { 1025.74f, 76.85f, 70.25f }, { 1025.77f, 77.05f, 70.23f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.77f, 77.05f, 70.23f }, { 1025.77f, 76.85f, 70.27f }, { 1025.80f, 77.05f, 70.26f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.77f, 76.85f, 70.27f }, { 1025.77f, 77.05f, 70.23f }, { 1025.74f, 76.85f, 70.25f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.80f, 77.05f, 70.26f }, { 1025.77f, 76.85f, 70.27f }, { 1025.81f, 77.03f, 70.25f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.81f, 77.03f, 70.25f }, { 1025.77f, 76.85f, 70.27f }, { 1025.77f, 76.85f, 70.26f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.10f, 77.43f, 69.28f }, { 1025.05f, 77.39f, 69.36f }, { 1025.05f, 77.41f, 69.36f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.05f, 77.41f, 69.36f }, { 1025.10f, 77.45f, 69.28f }, { 1025.10f, 77.43f, 69.28f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.08f, 77.41f, 69.38f }, { 1025.13f, 77.45f, 69.30f }, { 1025.05f, 77.41f, 69.36f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.05f, 77.41f, 69.36f }, { 1025.13f, 77.45f, 69.30f }, { 1025.10f, 77.45f, 69.28f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.08f, 77.41f, 69.38f }, { 1025.58f, 77.45f, 69.68f }, { 1025.13f, 77.45f, 69.30f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.08f, 77.41f, 69.38f }, { 1025.52f, 77.41f, 69.75f }, { 1025.58f, 77.45f, 69.68f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.97f, 77.41f, 70.11f }, { 1025.58f, 77.45f, 69.68f }, { 1025.52f, 77.41f, 69.75f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.58f, 77.45f, 69.67f }, { 1025.97f, 77.41f, 70.11f }, { 1026.04f, 77.45f, 70.05f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1026.00f, 77.41f, 70.14f }, { 1026.04f, 77.45f, 70.05f }, { 1025.97f, 77.41f, 70.11f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1026.04f, 77.45f, 70.05f }, { 1026.00f, 77.41f, 70.14f }, { 1026.07f, 77.45f, 70.07f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1026.07f, 77.45f, 70.07f }, { 1026.00f, 77.41f, 70.14f }, { 1026.07f, 77.43f, 70.07f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1026.00f, 77.39f, 70.14f }, { 1026.07f, 77.43f, 70.07f }, { 1026.00f, 77.41f, 70.14f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.57f, 77.48f, 68.73f }, { 1025.14f, 77.48f, 69.20f }, { 1025.17f, 77.48f, 69.23f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.14f, 77.48f, 69.20f }, { 1025.57f, 77.48f, 68.73f }, { 1025.55f, 77.48f, 68.71f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.17f, 77.48f, 69.23f }, { 1026.04f, 77.48f, 69.12f }, { 1025.57f, 77.48f, 68.73f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.64f, 77.48f, 69.61f }, { 1026.04f, 77.48f, 69.12f }, { 1025.17f, 77.48f, 69.23f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1026.10f, 77.48f, 69.99f }, { 1026.04f, 77.48f, 69.12f }, { 1025.64f, 77.48f, 69.61f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1026.04f, 77.48f, 69.12f }, { 1026.10f, 77.48f, 69.99f }, { 1026.50f, 77.48f, 69.50f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1026.13f, 77.48f, 70.02f }, { 1026.53f, 77.48f, 69.52f }, { 1026.10f, 77.48f, 69.99f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1026.10f, 77.48f, 69.99f }, { 1026.53f, 77.48f, 69.52f }, { 1026.50f, 77.48f, 69.50f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.14f, 77.48f, 69.20f }, { 1025.55f, 77.48f, 68.71f }, { 1025.14f, 77.45f, 69.20f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1025.14f, 77.45f, 69.20f }, { 1025.55f, 77.48f, 68.71f }, { 1025.55f, 77.45f, 68.71f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1026.13f, 77.45f, 70.01f }, { 1026.53f, 77.48f, 69.52f }, { 1026.13f, 77.48f, 70.01f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 1026.54f, 77.48f, 69.52f }, { 1026.13f, 77.45f, 70.01f }, { 1026.54f, 77.45f, 69.52f }, 76.63f, 0.00f },   // waterfeature_02 props 419
    { { 791.71f, 1.12f, -77.82f }, { 792.26f, 1.12f, -78.69f }, { 787.55f, 1.12f, -81.06f }, 1.12f, 0.00f },   // ground_bdstatue_05 props 444
    { { 791.00f, 1.12f, -77.07f }, { 791.71f, 1.12f, -77.82f }, { 787.55f, 1.12f, -81.06f }, 1.12f, 0.00f },   // ground_bdstatue_05 props 444
    { { 792.26f, 1.12f, -78.69f }, { 792.64f, 1.12f, -79.66f }, { 787.55f, 1.12f, -81.06f }, 1.12f, 0.00f },   // ground_bdstatue_05 props 444
    { { 790.15f, 1.12f, -76.47f }, { 791.00f, 1.12f, -77.07f }, { 787.55f, 1.12f, -81.06f }, 1.12f, 0.00f },   // ground_bdstatue_05 props 444
    { { 792.64f, 1.12f, -79.66f }, { 792.81f, 1.12f, -80.68f }, { 787.55f, 1.12f, -81.06f }, 1.12f, 0.00f },   // ground_bdstatue_05 props 444
    { { 789.21f, 1.12f, -76.05f }, { 790.15f, 1.12f, -76.47f }, { 787.55f, 1.12f, -81.06f }, 1.12f, 0.00f },   // ground_bdstatue_05 props 444
    { { 792.81f, 1.12f, -80.68f }, { 792.79f, 1.12f, -81.71f }, { 787.55f, 1.12f, -81.06f }, 1.12f, 0.00f },   // ground_bdstatue_05 props 444
    { { 788.20f, 1.12f, -75.82f }, { 789.21f, 1.12f, -76.05f }, { 787.55f, 1.12f, -81.06f }, 1.12f, 0.00f },   // ground_bdstatue_05 props 444
    { { 792.79f, 1.12f, -81.72f }, { 792.56f, 1.12f, -82.72f }, { 787.55f, 1.12f, -81.06f }, 1.12f, 0.00f },   // ground_bdstatue_05 props 444
    { { 787.16f, 1.12f, -75.80f }, { 788.20f, 1.12f, -75.82f }, { 787.55f, 1.12f, -81.06f }, 1.12f, 0.00f },   // ground_bdstatue_05 props 444
    { { 792.56f, 1.12f, -82.72f }, { 792.14f, 1.12f, -83.67f }, { 787.55f, 1.12f, -81.06f }, 1.12f, 0.00f },   // ground_bdstatue_05 props 444
    { { 786.14f, 1.12f, -75.97f }, { 787.16f, 1.12f, -75.80f }, { 787.55f, 1.12f, -81.06f }, 1.12f, 0.00f },   // ground_bdstatue_05 props 444
    { { 792.14f, 1.12f, -83.67f }, { 791.54f, 1.12f, -84.52f }, { 787.55f, 1.12f, -81.06f }, 1.12f, 0.00f },   // ground_bdstatue_05 props 444
    { { 785.17f, 1.12f, -76.34f }, { 786.14f, 1.12f, -75.97f }, { 787.55f, 1.12f, -81.06f }, 1.12f, 0.00f },   // ground_bdstatue_05 props 444
    { { 791.54f, 1.12f, -84.52f }, { 790.79f, 1.12f, -85.23f }, { 787.55f, 1.12f, -81.06f }, 1.12f, 0.00f },   // ground_bdstatue_05 props 444
    { { 784.30f, 1.12f, -76.90f }, { 785.18f, 1.12f, -76.34f }, { 787.55f, 1.12f, -81.06f }, 1.12f, 0.00f },   // ground_bdstatue_05 props 444
    { { 790.79f, 1.12f, -85.23f }, { 789.92f, 1.12f, -85.78f }, { 787.55f, 1.12f, -81.06f }, 1.12f, 0.00f },   // ground_bdstatue_05 props 444
    { { 783.55f, 1.12f, -77.61f }, { 784.30f, 1.12f, -76.90f }, { 787.55f, 1.12f, -81.06f }, 1.12f, 0.00f },   // ground_bdstatue_05 props 444
    { { 789.92f, 1.12f, -85.78f }, { 788.95f, 1.12f, -86.15f }, { 787.55f, 1.12f, -81.06f }, 1.12f, 0.00f },   // ground_bdstatue_05 props 444
    { { 782.95f, 1.12f, -78.46f }, { 783.55f, 1.12f, -77.61f }, { 787.55f, 1.12f, -81.06f }, 1.12f, 0.00f },   // ground_bdstatue_05 props 444
    { { 788.95f, 1.12f, -86.16f }, { 787.93f, 1.12f, -86.33f }, { 787.55f, 1.12f, -81.06f }, 1.12f, 0.00f },   // ground_bdstatue_05 props 444
    { { 782.53f, 1.12f, -79.40f }, { 782.95f, 1.12f, -78.46f }, { 787.55f, 1.12f, -81.06f }, 1.12f, 0.00f },   // ground_bdstatue_05 props 444
    { { 787.93f, 1.12f, -86.33f }, { 786.89f, 1.12f, -86.30f }, { 787.55f, 1.12f, -81.06f }, 1.12f, 0.00f },   // ground_bdstatue_05 props 444
    { { 782.30f, 1.12f, -80.41f }, { 782.53f, 1.12f, -79.40f }, { 787.55f, 1.12f, -81.06f }, 1.12f, 0.00f },   // ground_bdstatue_05 props 444
    { { 786.89f, 1.12f, -86.30f }, { 785.88f, 1.12f, -86.08f }, { 787.55f, 1.12f, -81.06f }, 1.12f, 0.00f },   // ground_bdstatue_05 props 444
    { { 782.28f, 1.12f, -81.45f }, { 782.30f, 1.12f, -80.41f }, { 787.55f, 1.12f, -81.06f }, 1.12f, 0.00f },   // ground_bdstatue_05 props 444
    { { 785.88f, 1.12f, -86.08f }, { 784.94f, 1.12f, -85.66f }, { 787.55f, 1.12f, -81.06f }, 1.12f, 0.00f },   // ground_bdstatue_05 props 444
    { { 782.45f, 1.12f, -82.47f }, { 782.28f, 1.12f, -81.45f }, { 787.55f, 1.12f, -81.06f }, 1.12f, 0.00f },   // ground_bdstatue_05 props 444
    { { 784.94f, 1.12f, -85.66f }, { 784.09f, 1.12f, -85.06f }, { 787.55f, 1.12f, -81.06f }, 1.12f, 0.00f },   // ground_bdstatue_05 props 444
    { { 782.83f, 1.12f, -83.43f }, { 782.45f, 1.12f, -82.47f }, { 787.55f, 1.12f, -81.06f }, 1.12f, 0.00f },   // ground_bdstatue_05 props 444
    { { 784.09f, 1.12f, -85.06f }, { 783.38f, 1.12f, -84.31f }, { 787.55f, 1.12f, -81.06f }, 1.12f, 0.00f },   // ground_bdstatue_05 props 444
    { { 783.38f, 1.12f, -84.31f }, { 782.83f, 1.12f, -83.43f }, { 787.55f, 1.12f, -81.06f }, 1.12f, 0.00f },   // ground_bdstatue_05 props 444
    { { 806.45f, 77.24f, -34.56f }, { 806.42f, 77.18f, -34.49f }, { 806.42f, 77.20f, -34.48f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.41f, 77.20f, -34.48f }, { 806.45f, 77.26f, -34.56f }, { 806.45f, 77.24f, -34.56f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.41f, 77.20f, -34.48f }, { 806.48f, 77.26f, -34.54f }, { 806.45f, 77.26f, -34.56f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.44f, 77.20f, -34.46f }, { 806.48f, 77.26f, -34.54f }, { 806.42f, 77.20f, -34.48f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.41f, 77.20f, -34.48f }, { 806.38f, 77.07f, -34.41f }, { 806.44f, 77.20f, -34.46f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.38f, 77.07f, -34.41f }, { 806.42f, 77.20f, -34.48f }, { 806.42f, 77.18f, -34.49f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.42f, 77.18f, -34.48f }, { 806.39f, 77.05f, -34.41f }, { 806.38f, 77.07f, -34.41f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.44f, 77.20f, -34.46f }, { 806.92f, 77.26f, -34.17f }, { 806.48f, 77.26f, -34.54f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.48f, 77.26f, -34.54f }, { 806.53f, 77.30f, -34.62f }, { 806.45f, 77.26f, -34.56f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.48f, 77.26f, -34.54f }, { 806.98f, 77.30f, -34.24f }, { 806.53f, 77.30f, -34.62f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.48f, 77.26f, -34.54f }, { 806.92f, 77.26f, -34.17f }, { 806.98f, 77.30f, -34.24f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.92f, 77.26f, -34.17f }, { 806.44f, 77.20f, -34.46f }, { 806.87f, 77.20f, -34.11f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.87f, 77.20f, -34.11f }, { 806.44f, 77.20f, -34.46f }, { 806.41f, 77.07f, -34.38f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.38f, 77.07f, -34.41f }, { 806.41f, 77.07f, -34.38f }, { 806.44f, 77.20f, -34.46f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.30f, 77.20f, -33.76f }, { 806.92f, 77.26f, -34.17f }, { 806.87f, 77.20f, -34.11f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.37f, 77.26f, -33.80f }, { 806.98f, 77.30f, -34.24f }, { 806.92f, 77.26f, -34.17f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.92f, 77.26f, -34.17f }, { 807.30f, 77.20f, -33.76f }, { 807.37f, 77.26f, -33.80f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.04f, 77.33f, -34.31f }, { 806.53f, 77.30f, -34.62f }, { 806.98f, 77.30f, -34.24f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.44f, 77.30f, -33.87f }, { 807.04f, 77.33f, -34.31f }, { 806.98f, 77.30f, -34.24f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.98f, 77.30f, -34.24f }, { 807.37f, 77.26f, -33.80f }, { 807.44f, 77.30f, -33.87f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.38f, 77.07f, -34.41f }, { 806.37f, 76.90f, -34.34f }, { 806.41f, 77.07f, -34.38f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.37f, 76.90f, -34.34f }, { 806.38f, 77.07f, -34.41f }, { 806.39f, 77.05f, -34.41f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.87f, 77.20f, -34.11f }, { 806.41f, 77.07f, -34.38f }, { 806.82f, 77.07f, -34.05f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.23f, 77.07f, -33.71f }, { 806.87f, 77.20f, -34.11f }, { 806.82f, 77.07f, -34.05f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.87f, 77.20f, -34.11f }, { 807.23f, 77.07f, -33.71f }, { 807.30f, 77.20f, -33.76f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.37f, 76.90f, -34.34f }, { 806.40f, 76.90f, -34.32f }, { 806.41f, 77.07f, -34.38f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.39f, 76.90f, -34.32f }, { 806.82f, 77.07f, -34.05f }, { 806.41f, 77.07f, -34.38f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.33f, 77.20f, -33.73f }, { 807.37f, 77.26f, -33.80f }, { 807.30f, 77.20f, -33.76f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.26f, 77.07f, -33.69f }, { 807.30f, 77.20f, -33.76f }, { 807.23f, 77.07f, -33.71f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.30f, 77.20f, -33.76f }, { 807.26f, 77.07f, -33.69f }, { 807.33f, 77.20f, -33.73f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.37f, 77.26f, -33.80f }, { 807.33f, 77.20f, -33.73f }, { 807.40f, 77.26f, -33.78f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.40f, 77.26f, -33.78f }, { 807.44f, 77.30f, -33.87f }, { 807.37f, 77.26f, -33.80f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.44f, 77.30f, -33.87f }, { 807.40f, 77.26f, -33.78f }, { 807.47f, 77.30f, -33.84f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.47f, 77.30f, -33.84f }, { 807.50f, 77.33f, -33.93f }, { 807.44f, 77.30f, -33.87f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.04f, 77.33f, -34.31f }, { 807.44f, 77.30f, -33.87f }, { 807.50f, 77.33f, -33.93f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.39f, 77.06f, -34.41f }, { 806.37f, 76.88f, -34.35f }, { 806.37f, 76.90f, -34.34f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.82f, 77.07f, -34.05f }, { 806.40f, 76.90f, -34.32f }, { 806.78f, 76.90f, -34.00f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.23f, 77.07f, -33.71f }, { 806.82f, 77.07f, -34.05f }, { 807.17f, 76.90f, -33.68f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.78f, 76.90f, -34.00f }, { 807.17f, 76.90f, -33.68f }, { 806.82f, 77.07f, -34.05f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.20f, 76.90f, -33.66f }, { 807.26f, 77.07f, -33.69f }, { 807.23f, 77.07f, -33.71f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.23f, 77.07f, -33.71f }, { 807.17f, 76.90f, -33.68f }, { 807.20f, 76.90f, -33.66f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.37f, 76.90f, -34.34f }, { 806.36f, 76.70f, -34.31f }, { 806.40f, 76.90f, -34.32f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.78f, 76.90f, -34.00f }, { 806.40f, 76.90f, -34.32f }, { 806.39f, 76.70f, -34.28f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.36f, 76.70f, -34.31f }, { 806.39f, 76.70f, -34.28f }, { 806.40f, 76.90f, -34.32f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.78f, 76.90f, -34.00f }, { 806.39f, 76.70f, -34.28f }, { 806.76f, 76.70f, -33.98f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.14f, 76.70f, -33.67f }, { 806.78f, 76.90f, -34.00f }, { 806.76f, 76.70f, -33.98f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.78f, 76.90f, -34.00f }, { 807.14f, 76.70f, -33.67f }, { 807.17f, 76.90f, -33.68f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.17f, 76.90f, -33.68f }, { 807.17f, 76.70f, -33.65f }, { 807.20f, 76.90f, -33.66f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.17f, 76.70f, -33.65f }, { 807.17f, 76.90f, -33.68f }, { 807.14f, 76.70f, -33.67f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.36f, 76.70f, -34.31f }, { 806.37f, 76.48f, -34.28f }, { 806.39f, 76.70f, -34.28f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.40f, 76.48f, -34.26f }, { 806.76f, 76.70f, -33.98f }, { 806.39f, 76.70f, -34.28f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.37f, 76.48f, -34.28f }, { 806.40f, 76.48f, -34.26f }, { 806.39f, 76.70f, -34.28f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.76f, 76.70f, -33.98f }, { 806.40f, 76.48f, -34.26f }, { 806.76f, 76.48f, -33.97f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.14f, 76.70f, -33.67f }, { 806.76f, 76.70f, -33.98f }, { 807.11f, 76.48f, -33.67f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.76f, 76.48f, -33.97f }, { 807.11f, 76.48f, -33.67f }, { 806.76f, 76.70f, -33.98f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.14f, 76.48f, -33.65f }, { 807.17f, 76.70f, -33.65f }, { 807.14f, 76.70f, -33.67f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.14f, 76.70f, -33.67f }, { 807.11f, 76.48f, -33.67f }, { 807.14f, 76.48f, -33.65f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.37f, 76.48f, -34.28f }, { 806.37f, 76.24f, -34.27f }, { 806.40f, 76.48f, -34.26f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.40f, 76.24f, -34.25f }, { 806.76f, 76.48f, -33.97f }, { 806.40f, 76.48f, -34.26f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.37f, 76.24f, -34.27f }, { 806.40f, 76.24f, -34.25f }, { 806.40f, 76.48f, -34.26f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.76f, 76.48f, -33.97f }, { 806.40f, 76.24f, -34.25f }, { 806.75f, 76.24f, -33.96f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.10f, 76.24f, -33.67f }, { 806.76f, 76.48f, -33.97f }, { 806.75f, 76.24f, -33.96f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.76f, 76.48f, -33.97f }, { 807.10f, 76.24f, -33.67f }, { 807.11f, 76.48f, -33.67f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.11f, 76.48f, -33.67f }, { 807.13f, 76.24f, -33.65f }, { 807.14f, 76.48f, -33.65f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.13f, 76.24f, -33.65f }, { 807.11f, 76.48f, -33.67f }, { 807.10f, 76.24f, -33.67f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.10f, 76.24f, -33.67f }, { 807.10f, 75.55f, -33.67f }, { 807.13f, 76.24f, -33.65f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.10f, 76.24f, -33.67f }, { 806.75f, 76.24f, -33.96f }, { 807.10f, 75.55f, -33.67f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.37f, 76.24f, -34.27f }, { 806.37f, 75.55f, -34.27f }, { 806.40f, 76.24f, -34.25f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.75f, 76.24f, -33.96f }, { 806.40f, 76.24f, -34.25f }, { 806.40f, 75.55f, -34.25f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.40f, 76.24f, -34.25f }, { 806.37f, 75.55f, -34.27f }, { 806.40f, 75.55f, -34.25f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.75f, 76.24f, -33.96f }, { 806.40f, 75.55f, -34.25f }, { 806.75f, 75.55f, -33.96f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.75f, 76.24f, -33.96f }, { 806.75f, 75.55f, -33.96f }, { 807.10f, 75.55f, -33.67f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.13f, 76.24f, -33.65f }, { 807.10f, 75.55f, -33.67f }, { 807.13f, 75.55f, -33.65f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.10f, 75.55f, -33.67f }, { 806.75f, 75.55f, -33.96f }, { 807.10f, 74.86f, -33.67f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.10f, 75.55f, -33.67f }, { 807.10f, 74.86f, -33.67f }, { 807.13f, 75.55f, -33.65f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.37f, 75.55f, -34.27f }, { 806.37f, 74.86f, -34.27f }, { 806.40f, 75.55f, -34.25f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.75f, 75.55f, -33.96f }, { 806.40f, 75.55f, -34.25f }, { 806.40f, 74.86f, -34.25f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.40f, 75.55f, -34.25f }, { 806.37f, 74.86f, -34.27f }, { 806.40f, 74.86f, -34.25f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.75f, 75.55f, -33.96f }, { 806.40f, 74.86f, -34.25f }, { 806.75f, 74.86f, -33.96f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.75f, 75.55f, -33.96f }, { 806.75f, 74.86f, -33.96f }, { 807.10f, 74.86f, -33.67f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.37f, 75.55f, -34.27f }, { 806.37f, 76.24f, -34.27f }, { 806.38f, 76.24f, -34.28f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.37f, 74.86f, -34.27f }, { 806.37f, 75.55f, -34.27f }, { 806.38f, 75.55f, -34.28f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.37f, 75.55f, -34.27f }, { 806.38f, 76.24f, -34.28f }, { 806.38f, 75.55f, -34.28f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.13f, 75.55f, -33.65f }, { 807.10f, 74.86f, -33.67f }, { 807.13f, 74.86f, -33.65f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.10f, 74.86f, -33.67f }, { 807.10f, 74.17f, -33.67f }, { 807.13f, 74.86f, -33.65f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.10f, 74.86f, -33.67f }, { 806.75f, 74.86f, -33.96f }, { 807.10f, 74.17f, -33.67f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.13f, 75.55f, -33.65f }, { 807.14f, 76.24f, -33.66f }, { 807.13f, 76.24f, -33.65f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.13f, 74.86f, -33.65f }, { 807.14f, 75.55f, -33.66f }, { 807.13f, 75.55f, -33.65f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.14f, 76.24f, -33.66f }, { 807.13f, 75.55f, -33.65f }, { 807.14f, 75.55f, -33.66f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.37f, 74.86f, -34.27f }, { 806.37f, 74.17f, -34.27f }, { 806.40f, 74.86f, -34.25f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.37f, 74.17f, -34.27f }, { 806.37f, 74.86f, -34.27f }, { 806.38f, 74.86f, -34.28f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.37f, 74.86f, -34.27f }, { 806.38f, 75.55f, -34.28f }, { 806.38f, 74.86f, -34.28f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.40f, 74.86f, -34.25f }, { 806.37f, 74.17f, -34.27f }, { 806.40f, 74.17f, -34.25f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.75f, 74.86f, -33.96f }, { 806.40f, 74.86f, -34.25f }, { 806.40f, 74.17f, -34.25f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.75f, 74.86f, -33.96f }, { 806.40f, 74.17f, -34.25f }, { 806.75f, 74.17f, -33.96f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.75f, 74.86f, -33.96f }, { 806.75f, 74.17f, -33.96f }, { 807.10f, 74.17f, -33.67f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.13f, 74.86f, -33.65f }, { 807.10f, 74.17f, -33.67f }, { 807.13f, 74.17f, -33.65f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.14f, 75.55f, -33.66f }, { 807.13f, 74.86f, -33.65f }, { 807.14f, 74.86f, -33.66f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.13f, 74.17f, -33.65f }, { 807.14f, 74.86f, -33.66f }, { 807.13f, 74.86f, -33.65f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.10f, 74.17f, -33.67f }, { 807.10f, 73.48f, -33.67f }, { 807.13f, 74.17f, -33.65f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.10f, 74.17f, -33.67f }, { 806.75f, 74.17f, -33.96f }, { 807.10f, 73.48f, -33.67f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.37f, 74.17f, -34.27f }, { 806.38f, 74.86f, -34.28f }, { 806.38f, 74.17f, -34.28f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.37f, 74.17f, -34.27f }, { 806.37f, 73.48f, -34.27f }, { 806.40f, 74.17f, -34.25f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.37f, 73.48f, -34.27f }, { 806.37f, 74.17f, -34.27f }, { 806.38f, 74.17f, -34.28f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.40f, 74.17f, -34.25f }, { 806.37f, 73.48f, -34.27f }, { 806.40f, 73.48f, -34.25f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.75f, 74.17f, -33.96f }, { 806.40f, 74.17f, -34.25f }, { 806.40f, 73.48f, -34.25f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.75f, 74.17f, -33.96f }, { 806.40f, 73.48f, -34.25f }, { 806.75f, 73.48f, -33.96f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.75f, 74.17f, -33.96f }, { 806.75f, 73.48f, -33.96f }, { 807.10f, 73.48f, -33.67f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.14f, 74.86f, -33.66f }, { 807.13f, 74.17f, -33.65f }, { 807.14f, 74.17f, -33.66f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.13f, 73.48f, -33.65f }, { 807.14f, 74.17f, -33.66f }, { 807.13f, 74.17f, -33.65f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.13f, 74.17f, -33.65f }, { 807.10f, 73.48f, -33.67f }, { 807.13f, 73.48f, -33.65f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.10f, 73.48f, -33.67f }, { 807.10f, 72.80f, -33.67f }, { 807.13f, 73.48f, -33.65f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.10f, 73.48f, -33.67f }, { 806.75f, 73.48f, -33.96f }, { 807.10f, 72.80f, -33.67f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.37f, 73.48f, -34.27f }, { 806.38f, 74.17f, -34.28f }, { 806.38f, 73.48f, -34.28f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.37f, 72.80f, -34.27f }, { 806.37f, 73.48f, -34.27f }, { 806.38f, 73.48f, -34.28f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.37f, 73.48f, -34.27f }, { 806.37f, 72.80f, -34.27f }, { 806.40f, 73.48f, -34.25f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.40f, 73.48f, -34.25f }, { 806.37f, 72.80f, -34.27f }, { 806.40f, 72.80f, -34.25f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.75f, 73.48f, -33.96f }, { 806.40f, 73.48f, -34.25f }, { 806.40f, 72.80f, -34.25f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.75f, 73.48f, -33.96f }, { 806.40f, 72.80f, -34.25f }, { 806.75f, 72.80f, -33.96f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.75f, 73.48f, -33.96f }, { 806.75f, 72.80f, -33.96f }, { 807.10f, 72.80f, -33.67f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.14f, 74.17f, -33.66f }, { 807.13f, 73.48f, -33.65f }, { 807.14f, 73.48f, -33.66f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.13f, 73.48f, -33.65f }, { 807.10f, 72.80f, -33.67f }, { 807.13f, 72.80f, -33.65f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.13f, 72.80f, -33.65f }, { 807.14f, 73.48f, -33.66f }, { 807.13f, 73.48f, -33.65f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.10f, 72.80f, -33.67f }, { 807.10f, 72.10f, -33.67f }, { 807.13f, 72.80f, -33.65f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.10f, 72.80f, -33.67f }, { 806.75f, 72.80f, -33.96f }, { 807.10f, 72.10f, -33.67f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.37f, 72.80f, -34.27f }, { 806.38f, 73.48f, -34.28f }, { 806.38f, 72.79f, -34.28f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.37f, 72.10f, -34.27f }, { 806.37f, 72.80f, -34.27f }, { 806.38f, 72.79f, -34.28f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.37f, 72.80f, -34.27f }, { 806.37f, 72.10f, -34.27f }, { 806.40f, 72.80f, -34.25f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.40f, 72.80f, -34.25f }, { 806.37f, 72.10f, -34.27f }, { 806.40f, 72.10f, -34.25f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.75f, 72.80f, -33.96f }, { 806.40f, 72.80f, -34.25f }, { 806.40f, 72.10f, -34.25f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.75f, 72.80f, -33.96f }, { 806.40f, 72.10f, -34.25f }, { 806.75f, 72.10f, -33.96f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.75f, 72.80f, -33.96f }, { 806.75f, 72.10f, -33.96f }, { 807.10f, 72.10f, -33.67f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.14f, 73.48f, -33.66f }, { 807.13f, 72.80f, -33.65f }, { 807.14f, 72.79f, -33.66f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.13f, 72.80f, -33.65f }, { 807.10f, 72.10f, -33.67f }, { 807.13f, 72.10f, -33.65f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.13f, 72.10f, -33.65f }, { 807.14f, 72.79f, -33.66f }, { 807.13f, 72.80f, -33.65f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.10f, 72.10f, -33.67f }, { 807.10f, 71.42f, -33.67f }, { 807.13f, 72.10f, -33.65f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.10f, 72.10f, -33.67f }, { 806.75f, 72.10f, -33.96f }, { 807.10f, 71.42f, -33.67f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.37f, 72.10f, -34.27f }, { 806.38f, 72.79f, -34.28f }, { 806.38f, 72.10f, -34.28f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.37f, 71.42f, -34.27f }, { 806.37f, 72.10f, -34.27f }, { 806.38f, 72.10f, -34.28f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.37f, 72.10f, -34.27f }, { 806.37f, 71.42f, -34.27f }, { 806.40f, 72.10f, -34.25f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.40f, 72.10f, -34.25f }, { 806.37f, 71.42f, -34.27f }, { 806.40f, 71.42f, -34.25f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.75f, 72.10f, -33.96f }, { 806.40f, 72.10f, -34.25f }, { 806.40f, 71.42f, -34.25f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.75f, 72.10f, -33.96f }, { 806.40f, 71.42f, -34.25f }, { 806.75f, 71.42f, -33.96f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.75f, 72.10f, -33.96f }, { 806.75f, 71.42f, -33.96f }, { 807.10f, 71.42f, -33.67f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.14f, 72.79f, -33.66f }, { 807.13f, 72.10f, -33.65f }, { 807.14f, 72.10f, -33.66f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.13f, 72.10f, -33.65f }, { 807.10f, 71.42f, -33.67f }, { 807.13f, 71.42f, -33.65f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.13f, 71.42f, -33.65f }, { 807.14f, 72.10f, -33.66f }, { 807.13f, 72.10f, -33.65f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.10f, 71.42f, -33.67f }, { 807.10f, 70.73f, -33.67f }, { 807.13f, 71.42f, -33.65f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.10f, 71.42f, -33.67f }, { 806.75f, 70.73f, -33.96f }, { 807.10f, 70.73f, -33.67f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.10f, 71.42f, -33.67f }, { 806.75f, 71.42f, -33.96f }, { 806.75f, 70.73f, -33.96f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.75f, 71.42f, -33.96f }, { 806.40f, 71.42f, -34.25f }, { 806.40f, 70.73f, -34.25f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.75f, 71.42f, -33.96f }, { 806.40f, 70.73f, -34.25f }, { 806.75f, 70.73f, -33.96f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.37f, 71.42f, -34.27f }, { 806.37f, 70.73f, -34.27f }, { 806.40f, 71.42f, -34.25f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.40f, 71.42f, -34.25f }, { 806.37f, 70.73f, -34.27f }, { 806.40f, 70.73f, -34.25f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.37f, 71.42f, -34.27f }, { 806.38f, 72.10f, -34.28f }, { 806.38f, 71.42f, -34.28f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.37f, 70.72f, -34.27f }, { 806.37f, 71.42f, -34.27f }, { 806.38f, 71.42f, -34.28f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.14f, 72.10f, -33.66f }, { 807.13f, 71.42f, -33.65f }, { 807.14f, 71.42f, -33.66f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.13f, 71.42f, -33.65f }, { 807.10f, 70.73f, -33.67f }, { 807.13f, 70.73f, -33.65f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.13f, 70.72f, -33.65f }, { 807.14f, 71.42f, -33.66f }, { 807.13f, 71.42f, -33.65f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.37f, 70.72f, -34.27f }, { 806.38f, 71.42f, -34.28f }, { 806.38f, 70.73f, -34.28f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.14f, 71.42f, -33.66f }, { 807.13f, 70.73f, -33.65f }, { 807.14f, 70.73f, -33.66f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.37f, 76.88f, -34.35f }, { 806.36f, 76.70f, -34.31f }, { 806.37f, 76.90f, -34.34f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.36f, 76.70f, -34.31f }, { 806.37f, 76.88f, -34.35f }, { 806.37f, 76.70f, -34.31f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.37f, 76.70f, -34.31f }, { 806.37f, 76.48f, -34.28f }, { 806.36f, 76.70f, -34.31f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.37f, 76.48f, -34.28f }, { 806.37f, 76.70f, -34.31f }, { 806.38f, 76.48f, -34.29f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.38f, 76.48f, -34.29f }, { 806.37f, 76.24f, -34.27f }, { 806.37f, 76.48f, -34.28f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.37f, 76.24f, -34.27f }, { 806.38f, 76.48f, -34.29f }, { 806.38f, 76.24f, -34.28f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.40f, 77.24f, -33.78f }, { 807.33f, 77.20f, -33.73f }, { 807.33f, 77.18f, -33.74f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.33f, 77.20f, -33.73f }, { 807.40f, 77.24f, -33.78f }, { 807.40f, 77.26f, -33.78f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.33f, 77.20f, -33.73f }, { 807.26f, 77.07f, -33.69f }, { 807.33f, 77.18f, -33.74f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.26f, 77.06f, -33.69f }, { 807.33f, 77.18f, -33.74f }, { 807.26f, 77.07f, -33.69f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.20f, 76.90f, -33.66f }, { 807.26f, 77.05f, -33.69f }, { 807.26f, 77.07f, -33.69f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.26f, 77.06f, -33.69f }, { 807.20f, 76.90f, -33.66f }, { 807.21f, 76.88f, -33.66f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.20f, 76.90f, -33.66f }, { 807.17f, 76.70f, -33.65f }, { 807.21f, 76.88f, -33.66f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.21f, 76.88f, -33.66f }, { 807.17f, 76.70f, -33.65f }, { 807.17f, 76.70f, -33.65f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.47f, 77.30f, -33.84f }, { 807.40f, 77.26f, -33.78f }, { 807.47f, 77.28f, -33.84f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.40f, 77.24f, -33.78f }, { 807.47f, 77.28f, -33.84f }, { 807.40f, 77.26f, -33.78f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.14f, 76.48f, -33.65f }, { 807.17f, 76.70f, -33.65f }, { 807.17f, 76.70f, -33.65f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.17f, 76.70f, -33.65f }, { 807.14f, 76.48f, -33.65f }, { 807.15f, 76.48f, -33.66f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.13f, 76.24f, -33.65f }, { 807.15f, 76.48f, -33.66f }, { 807.14f, 76.48f, -33.65f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.15f, 76.48f, -33.66f }, { 807.13f, 76.24f, -33.65f }, { 807.14f, 76.24f, -33.66f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.50f, 77.33f, -33.93f }, { 807.47f, 77.30f, -33.84f }, { 807.53f, 77.33f, -33.90f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.47f, 77.30f, -33.84f }, { 807.53f, 77.30f, -33.90f }, { 807.53f, 77.33f, -33.90f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.53f, 77.30f, -33.90f }, { 807.47f, 77.30f, -33.84f }, { 807.47f, 77.28f, -33.84f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.53f, 77.33f, -33.90f }, { 807.94f, 77.33f, -34.39f }, { 807.50f, 77.33f, -33.93f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.53f, 77.30f, -33.90f }, { 807.94f, 77.33f, -34.39f }, { 807.53f, 77.33f, -33.90f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.93f, 77.33f, -34.39f }, { 807.53f, 77.30f, -33.90f }, { 807.94f, 77.30f, -34.39f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.50f, 77.33f, -33.93f }, { 807.94f, 77.33f, -34.39f }, { 807.91f, 77.33f, -34.42f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.44f, 77.33f, -34.80f }, { 807.50f, 77.33f, -33.93f }, { 807.91f, 77.33f, -34.42f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.50f, 77.33f, -33.93f }, { 807.44f, 77.33f, -34.80f }, { 807.04f, 77.33f, -34.31f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 807.04f, 77.33f, -34.31f }, { 807.44f, 77.33f, -34.80f }, { 806.57f, 77.33f, -34.69f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.57f, 77.33f, -34.69f }, { 807.44f, 77.33f, -34.80f }, { 806.97f, 77.33f, -35.18f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.53f, 77.30f, -34.62f }, { 807.04f, 77.33f, -34.31f }, { 806.57f, 77.33f, -34.69f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.53f, 77.30f, -34.62f }, { 806.57f, 77.33f, -34.69f }, { 806.50f, 77.30f, -34.64f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.97f, 77.33f, -35.18f }, { 806.54f, 77.33f, -34.71f }, { 806.57f, 77.33f, -34.69f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.50f, 77.30f, -34.64f }, { 806.57f, 77.33f, -34.69f }, { 806.54f, 77.33f, -34.71f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.54f, 77.33f, -34.71f }, { 806.97f, 77.33f, -35.18f }, { 806.95f, 77.33f, -35.21f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.45f, 77.26f, -34.56f }, { 806.53f, 77.30f, -34.62f }, { 806.50f, 77.30f, -34.64f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.50f, 77.30f, -34.64f }, { 806.54f, 77.33f, -34.71f }, { 806.54f, 77.30f, -34.72f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.54f, 77.33f, -34.71f }, { 806.95f, 77.33f, -35.21f }, { 806.54f, 77.30f, -34.72f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.54f, 77.30f, -34.72f }, { 806.95f, 77.33f, -35.21f }, { 806.95f, 77.30f, -35.21f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.54f, 77.30f, -34.72f }, { 806.50f, 77.28f, -34.64f }, { 806.50f, 77.30f, -34.64f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.45f, 77.26f, -34.56f }, { 806.50f, 77.30f, -34.64f }, { 806.50f, 77.28f, -34.64f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 806.50f, 77.28f, -34.64f }, { 806.45f, 77.24f, -34.56f }, { 806.45f, 77.26f, -34.56f }, 70.73f, 0.00f },   // waterfeature_01 props 446
    { { 805.37f, 71.31f, -32.39f }, { 805.39f, 71.14f, -32.45f }, { 805.34f, 71.31f, -32.41f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 805.39f, 71.14f, -32.45f }, { 805.36f, 71.14f, -32.47f }, { 805.34f, 71.31f, -32.41f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 805.23f, 71.54f, -32.18f }, { 805.18f, 71.57f, -32.11f }, { 805.26f, 71.54f, -32.16f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 805.26f, 71.54f, -32.16f }, { 805.18f, 71.57f, -32.11f }, { 805.21f, 71.57f, -32.08f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 805.39f, 70.94f, -32.49f }, { 805.38f, 70.72f, -32.51f }, { 805.36f, 70.94f, -32.51f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 805.38f, 70.72f, -32.51f }, { 805.35f, 70.72f, -32.54f }, { 805.36f, 70.94f, -32.51f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 805.31f, 71.44f, -32.34f }, { 805.28f, 71.50f, -32.26f }, { 805.34f, 71.44f, -32.31f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 805.34f, 71.44f, -32.31f }, { 805.28f, 71.50f, -32.26f }, { 805.31f, 71.50f, -32.24f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 805.35f, 70.72f, -32.54f }, { 804.99f, 70.94f, -32.82f }, { 805.36f, 70.94f, -32.51f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 804.99f, 70.94f, -32.82f }, { 805.35f, 70.72f, -32.54f }, { 805.00f, 70.72f, -32.83f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 805.00f, 70.72f, -32.83f }, { 804.64f, 70.72f, -33.12f }, { 804.99f, 70.94f, -32.82f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 804.62f, 70.94f, -33.12f }, { 804.99f, 70.94f, -32.82f }, { 804.64f, 70.72f, -33.12f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 805.36f, 71.14f, -32.48f }, { 804.93f, 71.31f, -32.75f }, { 805.34f, 71.31f, -32.41f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 804.93f, 71.31f, -32.75f }, { 805.36f, 71.14f, -32.47f }, { 804.97f, 71.14f, -32.79f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 804.97f, 71.14f, -32.79f }, { 804.58f, 71.14f, -33.11f }, { 804.93f, 71.31f, -32.75f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 804.52f, 71.31f, -33.09f }, { 804.93f, 71.31f, -32.75f }, { 804.58f, 71.14f, -33.11f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 805.31f, 71.44f, -32.34f }, { 804.83f, 71.50f, -32.63f }, { 805.28f, 71.50f, -32.26f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 804.83f, 71.50f, -32.63f }, { 805.31f, 71.44f, -32.34f }, { 804.88f, 71.44f, -32.69f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 804.45f, 71.44f, -33.04f }, { 804.83f, 71.50f, -32.63f }, { 804.88f, 71.44f, -32.69f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 804.83f, 71.50f, -32.63f }, { 804.45f, 71.44f, -33.04f }, { 804.38f, 71.50f, -32.99f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 805.23f, 71.54f, -32.18f }, { 804.72f, 71.57f, -32.49f }, { 805.18f, 71.57f, -32.11f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 804.72f, 71.57f, -32.49f }, { 805.23f, 71.54f, -32.18f }, { 804.77f, 71.54f, -32.55f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 804.32f, 71.54f, -32.93f }, { 804.72f, 71.57f, -32.49f }, { 804.77f, 71.54f, -32.55f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 804.72f, 71.57f, -32.49f }, { 804.32f, 71.54f, -32.93f }, { 804.25f, 71.57f, -32.87f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 804.61f, 70.72f, -33.15f }, { 804.59f, 70.94f, -33.15f }, { 804.61f, 70.94f, -33.13f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 804.62f, 70.94f, -33.12f }, { 804.64f, 70.72f, -33.12f }, { 804.61f, 70.72f, -33.15f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 804.55f, 71.14f, -33.14f }, { 804.49f, 71.31f, -33.11f }, { 804.52f, 71.31f, -33.09f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 804.52f, 71.31f, -33.09f }, { 804.58f, 71.14f, -33.11f }, { 804.55f, 71.14f, -33.14f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 804.42f, 71.44f, -33.06f }, { 804.38f, 71.50f, -32.99f }, { 804.45f, 71.44f, -33.04f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 804.38f, 71.50f, -32.99f }, { 804.42f, 71.44f, -33.06f }, { 804.35f, 71.50f, -33.02f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 804.29f, 71.54f, -32.95f }, { 804.25f, 71.57f, -32.87f }, { 804.32f, 71.54f, -32.93f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 804.25f, 71.57f, -32.87f }, { 804.29f, 71.54f, -32.95f }, { 804.22f, 71.57f, -32.89f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 805.30f, 71.48f, -32.23f }, { 805.34f, 71.42f, -32.31f }, { 805.34f, 71.44f, -32.31f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 805.34f, 71.44f, -32.31f }, { 805.31f, 71.50f, -32.24f }, { 805.30f, 71.48f, -32.23f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 804.35f, 71.48f, -33.02f }, { 804.42f, 71.44f, -33.06f }, { 804.42f, 71.42f, -33.06f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 804.42f, 71.44f, -33.06f }, { 804.35f, 71.48f, -33.02f }, { 804.35f, 71.50f, -33.02f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 805.38f, 70.94f, -32.48f }, { 805.38f, 70.72f, -32.51f }, { 805.39f, 70.94f, -32.49f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 805.38f, 70.72f, -32.51f }, { 805.38f, 70.94f, -32.48f }, { 805.38f, 70.72f, -32.51f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 804.58f, 70.94f, -33.14f }, { 804.61f, 70.72f, -33.15f }, { 804.60f, 70.72f, -33.14f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 804.61f, 70.72f, -33.15f }, { 804.58f, 70.94f, -33.14f }, { 804.59f, 70.94f, -33.15f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 805.37f, 71.30f, -32.39f }, { 805.38f, 71.12f, -32.45f }, { 805.39f, 71.14f, -32.45f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 805.39f, 71.14f, -32.45f }, { 805.37f, 71.31f, -32.39f }, { 805.37f, 71.29f, -32.39f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 804.49f, 71.30f, -33.11f }, { 804.55f, 71.14f, -33.14f }, { 804.54f, 71.12f, -33.13f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 804.55f, 71.14f, -33.14f }, { 804.49f, 71.29f, -33.11f }, { 804.49f, 71.31f, -33.11f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 805.21f, 71.54f, -32.08f }, { 805.26f, 71.52f, -32.16f }, { 805.26f, 71.54f, -32.16f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 805.26f, 71.54f, -32.16f }, { 805.21f, 71.57f, -32.08f }, { 805.21f, 71.54f, -32.08f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 804.22f, 71.54f, -32.89f }, { 804.29f, 71.54f, -32.95f }, { 804.28f, 71.52f, -32.95f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 804.29f, 71.54f, -32.95f }, { 804.22f, 71.54f, -32.89f }, { 804.22f, 71.57f, -32.89f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 805.34f, 71.42f, -32.31f }, { 805.37f, 71.29f, -32.39f }, { 805.37f, 71.31f, -32.39f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 805.37f, 71.31f, -32.39f }, { 805.34f, 71.44f, -32.31f }, { 805.34f, 71.42f, -32.31f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 805.34f, 71.44f, -32.31f }, { 805.37f, 71.31f, -32.39f }, { 805.31f, 71.44f, -32.34f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 805.37f, 71.31f, -32.39f }, { 805.34f, 71.31f, -32.41f }, { 805.31f, 71.44f, -32.34f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 804.88f, 71.44f, -32.69f }, { 805.31f, 71.44f, -32.34f }, { 805.34f, 71.31f, -32.41f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 804.88f, 71.44f, -32.69f }, { 805.34f, 71.31f, -32.41f }, { 804.93f, 71.31f, -32.75f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 804.52f, 71.31f, -33.09f }, { 804.88f, 71.44f, -32.69f }, { 804.93f, 71.31f, -32.75f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 804.88f, 71.44f, -32.69f }, { 804.52f, 71.31f, -33.09f }, { 804.45f, 71.44f, -33.04f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 804.45f, 71.44f, -33.04f }, { 804.49f, 71.31f, -33.11f }, { 804.42f, 71.44f, -33.06f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 804.49f, 71.31f, -33.11f }, { 804.45f, 71.44f, -33.04f }, { 804.52f, 71.31f, -33.09f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 804.42f, 71.44f, -33.06f }, { 804.49f, 71.31f, -33.11f }, { 804.42f, 71.42f, -33.06f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 804.49f, 71.30f, -33.11f }, { 804.42f, 71.42f, -33.06f }, { 804.49f, 71.31f, -33.11f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 805.38f, 71.12f, -32.45f }, { 805.39f, 70.94f, -32.49f }, { 805.39f, 71.14f, -32.45f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 805.39f, 70.94f, -32.49f }, { 805.38f, 71.12f, -32.45f }, { 805.38f, 70.94f, -32.48f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 805.39f, 71.14f, -32.45f }, { 805.39f, 70.94f, -32.49f }, { 805.36f, 71.14f, -32.47f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 805.39f, 70.94f, -32.49f }, { 805.36f, 70.94f, -32.51f }, { 805.36f, 71.14f, -32.47f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 804.97f, 71.14f, -32.79f }, { 805.36f, 71.14f, -32.47f }, { 805.36f, 70.94f, -32.51f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 804.97f, 71.14f, -32.79f }, { 805.36f, 70.94f, -32.51f }, { 804.99f, 70.94f, -32.82f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 804.62f, 70.94f, -33.12f }, { 804.97f, 71.14f, -32.79f }, { 804.99f, 70.94f, -32.82f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 804.97f, 71.14f, -32.79f }, { 804.61f, 70.94f, -33.13f }, { 804.58f, 71.14f, -33.11f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 804.58f, 71.14f, -33.11f }, { 804.59f, 70.94f, -33.15f }, { 804.55f, 71.14f, -33.14f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 804.59f, 70.94f, -33.15f }, { 804.58f, 71.14f, -33.11f }, { 804.61f, 70.94f, -33.13f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 804.55f, 71.14f, -33.14f }, { 804.59f, 70.94f, -33.15f }, { 804.54f, 71.12f, -33.13f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 804.54f, 71.12f, -33.13f }, { 804.59f, 70.94f, -33.15f }, { 804.58f, 70.94f, -33.14f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 805.25f, 71.52f, -32.16f }, { 805.30f, 71.48f, -32.23f }, { 805.31f, 71.50f, -32.24f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 805.31f, 71.50f, -32.24f }, { 805.26f, 71.54f, -32.16f }, { 805.26f, 71.52f, -32.16f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 805.28f, 71.50f, -32.26f }, { 805.23f, 71.54f, -32.18f }, { 805.31f, 71.50f, -32.24f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 805.31f, 71.50f, -32.24f }, { 805.23f, 71.54f, -32.18f }, { 805.26f, 71.54f, -32.16f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 805.28f, 71.50f, -32.26f }, { 804.77f, 71.54f, -32.55f }, { 805.23f, 71.54f, -32.18f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 805.28f, 71.50f, -32.26f }, { 804.83f, 71.50f, -32.63f }, { 804.77f, 71.54f, -32.55f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 804.38f, 71.50f, -32.99f }, { 804.77f, 71.54f, -32.55f }, { 804.83f, 71.50f, -32.63f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 804.77f, 71.54f, -32.55f }, { 804.38f, 71.50f, -32.99f }, { 804.32f, 71.54f, -32.93f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 804.35f, 71.50f, -33.02f }, { 804.32f, 71.54f, -32.93f }, { 804.38f, 71.50f, -32.99f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 804.32f, 71.54f, -32.93f }, { 804.35f, 71.50f, -33.02f }, { 804.29f, 71.54f, -32.95f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 804.29f, 71.54f, -32.95f }, { 804.35f, 71.50f, -33.02f }, { 804.28f, 71.52f, -32.95f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 804.35f, 71.48f, -33.02f }, { 804.28f, 71.52f, -32.95f }, { 804.35f, 71.50f, -33.02f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 804.78f, 71.57f, -31.61f }, { 805.21f, 71.57f, -32.08f }, { 805.18f, 71.57f, -32.11f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 805.21f, 71.57f, -32.08f }, { 804.78f, 71.57f, -31.61f }, { 804.81f, 71.57f, -31.59f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 805.18f, 71.57f, -32.10f }, { 804.31f, 71.57f, -32.00f }, { 804.78f, 71.57f, -31.61f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 804.72f, 71.57f, -32.49f }, { 804.31f, 71.57f, -32.00f }, { 805.18f, 71.57f, -32.11f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 804.25f, 71.57f, -32.87f }, { 804.31f, 71.57f, -32.00f }, { 804.72f, 71.57f, -32.49f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 804.31f, 71.57f, -32.00f }, { 804.25f, 71.57f, -32.87f }, { 803.85f, 71.57f, -32.38f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 804.22f, 71.57f, -32.89f }, { 803.82f, 71.57f, -32.40f }, { 804.25f, 71.57f, -32.87f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 804.25f, 71.57f, -32.87f }, { 803.82f, 71.57f, -32.40f }, { 803.85f, 71.57f, -32.38f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 805.21f, 71.57f, -32.08f }, { 804.81f, 71.57f, -31.59f }, { 805.21f, 71.54f, -32.08f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 805.21f, 71.54f, -32.08f }, { 804.81f, 71.57f, -31.59f }, { 804.81f, 71.54f, -31.59f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 804.22f, 71.54f, -32.89f }, { 803.82f, 71.57f, -32.40f }, { 804.22f, 71.57f, -32.89f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 803.82f, 71.57f, -32.40f }, { 804.22f, 71.54f, -32.89f }, { 803.82f, 71.54f, -32.40f }, 70.72f, 0.00f },   // waterfeature_02 props 446
    { { 881.00f, 70.06f, 11.19f }, { 880.94f, 69.89f, 11.22f }, { 881.00f, 70.06f, 11.22f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 880.94f, 69.89f, 11.22f }, { 880.95f, 69.89f, 11.25f }, { 881.00f, 70.06f, 11.22f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.24f, 70.28f, 11.14f }, { 881.32f, 70.31f, 11.12f }, { 881.24f, 70.28f, 11.10f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.24f, 70.28f, 11.10f }, { 881.32f, 70.31f, 11.12f }, { 881.32f, 70.31f, 11.08f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 880.91f, 69.70f, 11.24f }, { 880.90f, 69.48f, 11.26f }, { 880.92f, 69.70f, 11.28f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 880.90f, 69.48f, 11.26f }, { 880.91f, 69.48f, 11.30f }, { 880.92f, 69.70f, 11.28f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.07f, 70.18f, 11.19f }, { 881.15f, 70.25f, 11.16f }, { 881.07f, 70.18f, 11.16f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.07f, 70.18f, 11.16f }, { 881.15f, 70.25f, 11.16f }, { 881.15f, 70.25f, 11.12f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 880.90f, 69.48f, 11.30f }, { 880.96f, 69.70f, 11.75f }, { 880.92f, 69.70f, 11.28f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 880.96f, 69.70f, 11.75f }, { 880.91f, 69.48f, 11.30f }, { 880.95f, 69.48f, 11.75f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 880.95f, 69.48f, 11.75f }, { 880.99f, 69.48f, 12.20f }, { 880.96f, 69.70f, 11.75f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.01f, 69.70f, 12.22f }, { 880.96f, 69.70f, 11.75f }, { 880.99f, 69.48f, 12.20f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 880.95f, 69.89f, 11.25f }, { 881.05f, 70.06f, 11.74f }, { 881.00f, 70.06f, 11.22f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.05f, 70.06f, 11.74f }, { 880.95f, 69.89f, 11.25f }, { 880.99f, 69.89f, 11.75f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 880.99f, 69.89f, 11.74f }, { 881.04f, 69.89f, 12.24f }, { 881.05f, 70.06f, 11.74f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.10f, 70.06f, 12.26f }, { 881.05f, 70.06f, 11.74f }, { 881.04f, 69.89f, 12.24f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.07f, 70.18f, 11.19f }, { 881.21f, 70.25f, 11.72f }, { 881.15f, 70.25f, 11.16f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.21f, 70.25f, 11.72f }, { 881.07f, 70.18f, 11.19f }, { 881.13f, 70.18f, 11.73f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.18f, 70.18f, 12.27f }, { 881.21f, 70.25f, 11.72f }, { 881.13f, 70.18f, 11.73f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.21f, 70.25f, 11.72f }, { 881.18f, 70.18f, 12.27f }, { 881.26f, 70.25f, 12.29f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.24f, 70.28f, 11.14f }, { 881.38f, 70.31f, 11.71f }, { 881.32f, 70.31f, 11.12f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.38f, 70.31f, 11.71f }, { 881.24f, 70.28f, 11.14f }, { 881.30f, 70.28f, 11.72f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.35f, 70.28f, 12.29f }, { 881.38f, 70.31f, 11.71f }, { 881.30f, 70.28f, 11.72f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.38f, 70.31f, 11.71f }, { 881.35f, 70.28f, 12.29f }, { 881.44f, 70.31f, 12.29f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.00f, 69.48f, 12.24f }, { 881.01f, 69.70f, 12.26f }, { 881.01f, 69.70f, 12.22f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.01f, 69.70f, 12.22f }, { 880.99f, 69.48f, 12.20f }, { 881.00f, 69.48f, 12.24f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.05f, 69.89f, 12.28f }, { 881.10f, 70.06f, 12.29f }, { 881.10f, 70.06f, 12.26f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.10f, 70.06f, 12.26f }, { 881.04f, 69.89f, 12.24f }, { 881.05f, 69.89f, 12.27f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.18f, 70.18f, 12.31f }, { 881.26f, 70.25f, 12.29f }, { 881.18f, 70.18f, 12.27f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.26f, 70.25f, 12.29f }, { 881.18f, 70.18f, 12.31f }, { 881.26f, 70.25f, 12.33f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.36f, 70.28f, 12.33f }, { 881.44f, 70.31f, 12.29f }, { 881.35f, 70.28f, 12.29f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.44f, 70.31f, 12.29f }, { 881.36f, 70.28f, 12.33f }, { 881.44f, 70.31f, 12.33f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.15f, 70.22f, 11.12f }, { 881.07f, 70.16f, 11.16f }, { 881.07f, 70.18f, 11.16f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.07f, 70.18f, 11.16f }, { 881.15f, 70.25f, 11.12f }, { 881.15f, 70.22f, 11.12f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.27f, 70.22f, 12.33f }, { 881.18f, 70.18f, 12.31f }, { 881.19f, 70.16f, 12.31f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.18f, 70.18f, 12.31f }, { 881.27f, 70.22f, 12.33f }, { 881.26f, 70.25f, 12.33f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 880.92f, 69.69f, 11.24f }, { 880.90f, 69.48f, 11.26f }, { 880.91f, 69.70f, 11.24f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 880.90f, 69.48f, 11.26f }, { 880.92f, 69.69f, 11.24f }, { 880.91f, 69.47f, 11.26f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.02f, 69.69f, 12.26f }, { 881.00f, 69.48f, 12.24f }, { 881.01f, 69.47f, 12.24f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.00f, 69.48f, 12.24f }, { 881.02f, 69.69f, 12.26f }, { 881.01f, 69.70f, 12.26f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.00f, 70.04f, 11.19f }, { 880.95f, 69.87f, 11.22f }, { 880.94f, 69.89f, 11.22f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 880.94f, 69.89f, 11.22f }, { 880.99f, 70.06f, 11.19f }, { 881.00f, 70.04f, 11.19f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.11f, 70.04f, 12.29f }, { 881.05f, 69.89f, 12.27f }, { 881.05f, 69.87f, 12.27f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.05f, 69.89f, 12.28f }, { 881.11f, 70.04f, 12.29f }, { 881.10f, 70.06f, 12.29f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.32f, 70.28f, 11.08f }, { 881.24f, 70.26f, 11.10f }, { 881.24f, 70.28f, 11.10f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.24f, 70.28f, 11.10f }, { 881.32f, 70.31f, 11.08f }, { 881.32f, 70.28f, 11.08f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.44f, 70.28f, 12.33f }, { 881.36f, 70.28f, 12.33f }, { 881.36f, 70.26f, 12.33f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.36f, 70.28f, 12.33f }, { 881.44f, 70.28f, 12.33f }, { 881.44f, 70.31f, 12.33f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.08f, 70.16f, 11.16f }, { 881.00f, 70.04f, 11.19f }, { 880.99f, 70.06f, 11.19f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.00f, 70.06f, 11.19f }, { 881.07f, 70.18f, 11.16f }, { 881.07f, 70.16f, 11.16f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.07f, 70.18f, 11.16f }, { 880.99f, 70.06f, 11.19f }, { 881.07f, 70.18f, 11.19f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.00f, 70.06f, 11.19f }, { 881.00f, 70.06f, 11.22f }, { 881.07f, 70.18f, 11.19f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.13f, 70.18f, 11.73f }, { 881.07f, 70.18f, 11.19f }, { 881.00f, 70.06f, 11.22f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.13f, 70.18f, 11.73f }, { 881.00f, 70.06f, 11.22f }, { 881.05f, 70.06f, 11.74f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.10f, 70.06f, 12.26f }, { 881.13f, 70.18f, 11.73f }, { 881.05f, 70.06f, 11.74f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.13f, 70.18f, 11.73f }, { 881.10f, 70.06f, 12.26f }, { 881.18f, 70.18f, 12.27f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.18f, 70.18f, 12.27f }, { 881.10f, 70.06f, 12.29f }, { 881.18f, 70.18f, 12.31f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.10f, 70.06f, 12.29f }, { 881.18f, 70.18f, 12.27f }, { 881.10f, 70.06f, 12.26f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.18f, 70.18f, 12.31f }, { 881.10f, 70.06f, 12.29f }, { 881.19f, 70.16f, 12.31f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.11f, 70.04f, 12.29f }, { 881.19f, 70.16f, 12.31f }, { 881.10f, 70.06f, 12.29f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 880.95f, 69.87f, 11.22f }, { 880.91f, 69.70f, 11.24f }, { 880.94f, 69.89f, 11.22f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 880.91f, 69.70f, 11.24f }, { 880.95f, 69.87f, 11.22f }, { 880.92f, 69.69f, 11.24f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 880.94f, 69.89f, 11.22f }, { 880.91f, 69.70f, 11.24f }, { 880.95f, 69.89f, 11.25f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 880.91f, 69.70f, 11.24f }, { 880.92f, 69.70f, 11.28f }, { 880.95f, 69.89f, 11.25f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 880.99f, 69.89f, 11.74f }, { 880.95f, 69.89f, 11.25f }, { 880.92f, 69.70f, 11.28f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 880.99f, 69.89f, 11.74f }, { 880.92f, 69.70f, 11.28f }, { 880.96f, 69.70f, 11.75f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.01f, 69.70f, 12.22f }, { 880.99f, 69.89f, 11.75f }, { 880.96f, 69.70f, 11.75f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 880.99f, 69.89f, 11.74f }, { 881.01f, 69.70f, 12.22f }, { 881.04f, 69.89f, 12.24f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.04f, 69.89f, 12.24f }, { 881.01f, 69.70f, 12.26f }, { 881.05f, 69.89f, 12.27f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.01f, 69.70f, 12.26f }, { 881.04f, 69.89f, 12.24f }, { 881.01f, 69.70f, 12.22f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.05f, 69.89f, 12.28f }, { 881.01f, 69.70f, 12.26f }, { 881.05f, 69.87f, 12.27f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.05f, 69.87f, 12.27f }, { 881.01f, 69.70f, 12.26f }, { 881.02f, 69.69f, 12.26f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.24f, 70.26f, 11.10f }, { 881.15f, 70.22f, 11.12f }, { 881.15f, 70.25f, 11.12f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.15f, 70.25f, 11.12f }, { 881.24f, 70.28f, 11.10f }, { 881.24f, 70.26f, 11.10f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.15f, 70.25f, 11.16f }, { 881.24f, 70.28f, 11.14f }, { 881.15f, 70.25f, 11.12f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.15f, 70.25f, 11.12f }, { 881.24f, 70.28f, 11.14f }, { 881.24f, 70.28f, 11.10f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.15f, 70.25f, 11.16f }, { 881.30f, 70.28f, 11.72f }, { 881.24f, 70.28f, 11.14f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.15f, 70.25f, 11.16f }, { 881.21f, 70.25f, 11.72f }, { 881.30f, 70.28f, 11.72f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.26f, 70.25f, 12.29f }, { 881.30f, 70.28f, 11.72f }, { 881.21f, 70.25f, 11.72f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.30f, 70.28f, 11.72f }, { 881.26f, 70.25f, 12.29f }, { 881.35f, 70.28f, 12.29f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.26f, 70.25f, 12.33f }, { 881.35f, 70.28f, 12.29f }, { 881.26f, 70.25f, 12.29f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.35f, 70.28f, 12.29f }, { 881.26f, 70.25f, 12.33f }, { 881.36f, 70.28f, 12.33f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.36f, 70.28f, 12.33f }, { 881.26f, 70.25f, 12.33f }, { 881.36f, 70.26f, 12.33f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.27f, 70.22f, 12.33f }, { 881.36f, 70.26f, 12.33f }, { 881.26f, 70.25f, 12.33f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.94f, 70.31f, 11.06f }, { 881.32f, 70.31f, 11.08f }, { 881.32f, 70.31f, 11.12f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.32f, 70.31f, 11.08f }, { 881.94f, 70.31f, 11.06f }, { 881.94f, 70.31f, 11.02f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.32f, 70.31f, 11.12f }, { 882.00f, 70.31f, 11.65f }, { 881.94f, 70.31f, 11.06f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.38f, 70.31f, 11.71f }, { 882.00f, 70.31f, 11.65f }, { 881.32f, 70.31f, 11.12f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.44f, 70.31f, 12.29f }, { 882.00f, 70.31f, 11.65f }, { 881.38f, 70.31f, 11.71f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 882.00f, 70.31f, 11.65f }, { 881.44f, 70.31f, 12.29f }, { 882.06f, 70.31f, 12.23f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.44f, 70.31f, 12.33f }, { 882.06f, 70.31f, 12.27f }, { 881.44f, 70.31f, 12.29f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.44f, 70.31f, 12.29f }, { 882.06f, 70.31f, 12.27f }, { 882.06f, 70.31f, 12.23f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.32f, 70.31f, 11.08f }, { 881.94f, 70.31f, 11.02f }, { 881.32f, 70.28f, 11.08f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.32f, 70.28f, 11.08f }, { 881.94f, 70.31f, 11.02f }, { 881.94f, 70.28f, 11.02f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.44f, 70.28f, 12.33f }, { 882.06f, 70.31f, 12.27f }, { 881.44f, 70.31f, 12.33f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 882.06f, 70.31f, 12.27f }, { 881.44f, 70.28f, 12.33f }, { 882.06f, 70.28f, 12.27f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.15f, 70.06f, 12.78f }, { 881.10f, 69.89f, 12.81f }, { 881.16f, 70.06f, 12.82f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.10f, 69.89f, 12.81f }, { 881.10f, 69.89f, 12.85f }, { 881.16f, 70.06f, 12.82f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.40f, 70.28f, 12.73f }, { 881.48f, 70.31f, 12.71f }, { 881.39f, 70.28f, 12.70f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.39f, 70.28f, 12.70f }, { 881.48f, 70.31f, 12.71f }, { 881.48f, 70.31f, 12.68f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.07f, 69.70f, 12.83f }, { 881.06f, 69.48f, 12.86f }, { 881.07f, 69.70f, 12.87f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.06f, 69.48f, 12.86f }, { 881.06f, 69.48f, 12.89f }, { 881.07f, 69.70f, 12.87f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.23f, 70.18f, 12.79f }, { 881.31f, 70.25f, 12.75f }, { 881.23f, 70.18f, 12.75f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.23f, 70.18f, 12.75f }, { 881.31f, 70.25f, 12.75f }, { 881.30f, 70.25f, 12.72f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.06f, 69.48f, 12.89f }, { 881.12f, 69.70f, 13.34f }, { 881.07f, 69.70f, 12.87f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.12f, 69.70f, 13.34f }, { 881.06f, 69.48f, 12.89f }, { 881.11f, 69.48f, 13.34f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.11f, 69.48f, 13.34f }, { 881.15f, 69.48f, 13.79f }, { 881.12f, 69.70f, 13.34f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.17f, 69.70f, 13.81f }, { 881.12f, 69.70f, 13.34f }, { 881.15f, 69.48f, 13.79f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.10f, 69.89f, 12.85f }, { 881.21f, 70.06f, 13.33f }, { 881.16f, 70.06f, 12.82f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.21f, 70.06f, 13.33f }, { 881.10f, 69.89f, 12.85f }, { 881.15f, 69.89f, 13.34f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.15f, 69.89f, 13.34f }, { 881.20f, 69.89f, 13.83f }, { 881.21f, 70.06f, 13.33f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.26f, 70.06f, 13.85f }, { 881.21f, 70.06f, 13.33f }, { 881.20f, 69.89f, 13.83f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.23f, 70.18f, 12.79f }, { 881.36f, 70.25f, 13.32f }, { 881.31f, 70.25f, 12.75f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.36f, 70.25f, 13.32f }, { 881.23f, 70.18f, 12.78f }, { 881.28f, 70.18f, 13.32f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.34f, 70.18f, 13.86f }, { 881.36f, 70.25f, 13.32f }, { 881.28f, 70.18f, 13.32f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.36f, 70.25f, 13.32f }, { 881.34f, 70.18f, 13.86f }, { 881.42f, 70.25f, 13.88f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.40f, 70.28f, 12.73f }, { 881.54f, 70.31f, 13.30f }, { 881.48f, 70.31f, 12.71f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.54f, 70.31f, 13.30f }, { 881.40f, 70.28f, 12.73f }, { 881.45f, 70.28f, 13.31f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.51f, 70.28f, 13.88f }, { 881.54f, 70.31f, 13.30f }, { 881.45f, 70.28f, 13.31f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.54f, 70.31f, 13.30f }, { 881.51f, 70.28f, 13.88f }, { 881.60f, 70.31f, 13.89f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.15f, 69.48f, 13.83f }, { 881.17f, 69.70f, 13.85f }, { 881.17f, 69.70f, 13.81f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.17f, 69.70f, 13.81f }, { 881.15f, 69.48f, 13.79f }, { 881.15f, 69.48f, 13.83f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.20f, 69.89f, 13.87f }, { 881.26f, 70.06f, 13.89f }, { 881.26f, 70.06f, 13.85f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.26f, 70.06f, 13.85f }, { 881.20f, 69.89f, 13.83f }, { 881.20f, 69.89f, 13.87f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.34f, 70.18f, 13.90f }, { 881.42f, 70.25f, 13.88f }, { 881.34f, 70.18f, 13.86f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.42f, 70.25f, 13.88f }, { 881.34f, 70.18f, 13.90f }, { 881.42f, 70.25f, 13.92f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.51f, 70.28f, 13.92f }, { 881.60f, 70.31f, 13.89f }, { 881.51f, 70.28f, 13.88f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.60f, 70.31f, 13.89f }, { 881.51f, 70.28f, 13.92f }, { 881.60f, 70.31f, 13.93f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.31f, 70.22f, 12.72f }, { 881.23f, 70.16f, 12.75f }, { 881.23f, 70.18f, 12.75f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.23f, 70.18f, 12.75f }, { 881.30f, 70.25f, 12.72f }, { 881.31f, 70.22f, 12.72f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.42f, 70.22f, 13.92f }, { 881.34f, 70.18f, 13.90f }, { 881.35f, 70.16f, 13.90f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.34f, 70.18f, 13.90f }, { 881.42f, 70.22f, 13.92f }, { 881.42f, 70.25f, 13.92f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.08f, 69.69f, 12.83f }, { 881.06f, 69.48f, 12.86f }, { 881.07f, 69.70f, 12.83f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.06f, 69.48f, 12.86f }, { 881.08f, 69.69f, 12.83f }, { 881.07f, 69.47f, 12.85f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.18f, 69.69f, 13.85f }, { 881.15f, 69.48f, 13.83f }, { 881.16f, 69.47f, 13.83f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.15f, 69.48f, 13.83f }, { 881.18f, 69.69f, 13.85f }, { 881.17f, 69.70f, 13.85f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.16f, 70.04f, 12.78f }, { 881.11f, 69.87f, 12.81f }, { 881.10f, 69.89f, 12.81f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.10f, 69.89f, 12.81f }, { 881.15f, 70.06f, 12.78f }, { 881.16f, 70.04f, 12.78f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.27f, 70.04f, 13.89f }, { 881.20f, 69.89f, 13.87f }, { 881.21f, 69.87f, 13.87f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.20f, 69.89f, 13.87f }, { 881.27f, 70.04f, 13.89f }, { 881.26f, 70.06f, 13.89f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.48f, 70.28f, 12.68f }, { 881.39f, 70.26f, 12.70f }, { 881.39f, 70.28f, 12.70f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.39f, 70.28f, 12.70f }, { 881.48f, 70.31f, 12.68f }, { 881.48f, 70.28f, 12.68f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.60f, 70.28f, 13.93f }, { 881.51f, 70.28f, 13.92f }, { 881.52f, 70.26f, 13.92f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.51f, 70.28f, 13.92f }, { 881.60f, 70.28f, 13.93f }, { 881.60f, 70.31f, 13.93f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.23f, 70.16f, 12.75f }, { 881.16f, 70.04f, 12.78f }, { 881.15f, 70.06f, 12.78f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.15f, 70.06f, 12.78f }, { 881.23f, 70.18f, 12.75f }, { 881.23f, 70.16f, 12.75f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.23f, 70.18f, 12.75f }, { 881.15f, 70.06f, 12.78f }, { 881.23f, 70.18f, 12.78f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.15f, 70.06f, 12.78f }, { 881.16f, 70.06f, 12.82f }, { 881.23f, 70.18f, 12.78f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.28f, 70.18f, 13.32f }, { 881.23f, 70.18f, 12.78f }, { 881.16f, 70.06f, 12.82f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.28f, 70.18f, 13.32f }, { 881.16f, 70.06f, 12.82f }, { 881.21f, 70.06f, 13.33f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.26f, 70.06f, 13.85f }, { 881.28f, 70.18f, 13.32f }, { 881.21f, 70.06f, 13.33f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.28f, 70.18f, 13.32f }, { 881.26f, 70.06f, 13.85f }, { 881.34f, 70.18f, 13.86f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.34f, 70.18f, 13.86f }, { 881.26f, 70.06f, 13.89f }, { 881.34f, 70.18f, 13.90f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.26f, 70.06f, 13.89f }, { 881.34f, 70.18f, 13.86f }, { 881.26f, 70.06f, 13.85f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.34f, 70.18f, 13.90f }, { 881.26f, 70.06f, 13.89f }, { 881.35f, 70.16f, 13.90f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.27f, 70.04f, 13.89f }, { 881.35f, 70.16f, 13.90f }, { 881.26f, 70.06f, 13.89f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.11f, 69.87f, 12.81f }, { 881.07f, 69.70f, 12.83f }, { 881.10f, 69.89f, 12.81f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.07f, 69.70f, 12.83f }, { 881.11f, 69.87f, 12.81f }, { 881.08f, 69.69f, 12.83f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.10f, 69.89f, 12.81f }, { 881.07f, 69.70f, 12.83f }, { 881.10f, 69.89f, 12.85f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.07f, 69.70f, 12.83f }, { 881.07f, 69.70f, 12.87f }, { 881.10f, 69.89f, 12.85f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.15f, 69.89f, 13.34f }, { 881.10f, 69.89f, 12.85f }, { 881.07f, 69.70f, 12.87f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.15f, 69.89f, 13.34f }, { 881.07f, 69.70f, 12.87f }, { 881.12f, 69.70f, 13.34f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.17f, 69.70f, 13.81f }, { 881.15f, 69.89f, 13.34f }, { 881.12f, 69.70f, 13.34f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.15f, 69.89f, 13.34f }, { 881.17f, 69.70f, 13.81f }, { 881.20f, 69.89f, 13.83f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.20f, 69.89f, 13.83f }, { 881.17f, 69.70f, 13.85f }, { 881.20f, 69.89f, 13.87f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.17f, 69.70f, 13.85f }, { 881.20f, 69.89f, 13.83f }, { 881.17f, 69.70f, 13.81f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.20f, 69.89f, 13.87f }, { 881.17f, 69.70f, 13.85f }, { 881.21f, 69.87f, 13.87f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.21f, 69.87f, 13.87f }, { 881.17f, 69.70f, 13.85f }, { 881.18f, 69.69f, 13.85f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.39f, 70.26f, 12.70f }, { 881.31f, 70.22f, 12.72f }, { 881.30f, 70.25f, 12.72f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.30f, 70.25f, 12.72f }, { 881.39f, 70.28f, 12.70f }, { 881.39f, 70.26f, 12.70f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.31f, 70.25f, 12.75f }, { 881.40f, 70.28f, 12.73f }, { 881.30f, 70.25f, 12.72f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.30f, 70.25f, 12.72f }, { 881.40f, 70.28f, 12.73f }, { 881.39f, 70.28f, 12.70f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.31f, 70.25f, 12.75f }, { 881.45f, 70.28f, 13.31f }, { 881.40f, 70.28f, 12.73f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.31f, 70.25f, 12.75f }, { 881.36f, 70.25f, 13.32f }, { 881.45f, 70.28f, 13.31f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.42f, 70.25f, 13.88f }, { 881.45f, 70.28f, 13.31f }, { 881.36f, 70.25f, 13.32f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.45f, 70.28f, 13.31f }, { 881.42f, 70.25f, 13.88f }, { 881.51f, 70.28f, 13.88f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.42f, 70.25f, 13.92f }, { 881.51f, 70.28f, 13.88f }, { 881.42f, 70.25f, 13.88f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.51f, 70.28f, 13.88f }, { 881.42f, 70.25f, 13.92f }, { 881.51f, 70.28f, 13.92f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.51f, 70.28f, 13.92f }, { 881.42f, 70.25f, 13.92f }, { 881.52f, 70.26f, 13.92f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.42f, 70.22f, 13.92f }, { 881.52f, 70.26f, 13.92f }, { 881.42f, 70.25f, 13.92f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 882.10f, 70.31f, 12.65f }, { 881.48f, 70.31f, 12.68f }, { 881.48f, 70.31f, 12.71f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.48f, 70.31f, 12.68f }, { 882.10f, 70.31f, 12.65f }, { 882.10f, 70.31f, 12.61f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.48f, 70.31f, 12.71f }, { 882.16f, 70.31f, 13.24f }, { 882.10f, 70.31f, 12.65f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.54f, 70.31f, 13.30f }, { 882.16f, 70.31f, 13.24f }, { 881.48f, 70.31f, 12.71f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.60f, 70.31f, 13.89f }, { 882.16f, 70.31f, 13.24f }, { 881.54f, 70.31f, 13.30f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 882.16f, 70.31f, 13.24f }, { 881.60f, 70.31f, 13.89f }, { 882.22f, 70.31f, 13.83f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.60f, 70.31f, 13.93f }, { 882.22f, 70.31f, 13.86f }, { 881.60f, 70.31f, 13.89f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.60f, 70.31f, 13.89f }, { 882.22f, 70.31f, 13.86f }, { 882.22f, 70.31f, 13.83f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.48f, 70.31f, 12.68f }, { 882.10f, 70.31f, 12.61f }, { 881.48f, 70.28f, 12.68f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.48f, 70.28f, 12.68f }, { 882.10f, 70.31f, 12.61f }, { 882.10f, 70.28f, 12.61f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 881.60f, 70.28f, 13.93f }, { 882.22f, 70.31f, 13.86f }, { 881.60f, 70.31f, 13.93f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 882.22f, 70.31f, 13.86f }, { 881.60f, 70.28f, 13.93f }, { 882.22f, 70.28f, 13.86f }, 69.47f, 0.00f },   // waterfeature_02 props 447
    { { 968.34f, 65.46f, 78.36f }, { 968.35f, 65.32f, 78.31f }, { 968.32f, 65.46f, 78.34f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 968.36f, 65.33f, 78.31f }, { 968.33f, 65.32f, 78.29f }, { 968.32f, 65.46f, 78.34f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 968.23f, 65.64f, 78.52f }, { 968.20f, 65.66f, 78.58f }, { 968.26f, 65.64f, 78.54f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 968.25f, 65.64f, 78.54f }, { 968.20f, 65.66f, 78.58f }, { 968.22f, 65.66f, 78.60f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 968.36f, 65.18f, 78.28f }, { 968.35f, 65.00f, 78.27f }, { 968.34f, 65.18f, 78.27f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 968.35f, 65.00f, 78.27f }, { 968.33f, 65.00f, 78.25f }, { 968.34f, 65.18f, 78.27f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 968.30f, 65.56f, 78.40f }, { 968.27f, 65.61f, 78.46f }, { 968.32f, 65.56f, 78.42f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 968.32f, 65.56f, 78.42f }, { 968.27f, 65.61f, 78.46f }, { 968.29f, 65.61f, 78.48f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 968.33f, 65.00f, 78.25f }, { 968.05f, 65.18f, 78.03f }, { 968.34f, 65.18f, 78.27f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 968.05f, 65.18f, 78.03f }, { 968.33f, 65.00f, 78.25f }, { 968.06f, 65.00f, 78.02f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 968.06f, 65.00f, 78.02f }, { 967.78f, 65.00f, 77.80f }, { 968.05f, 65.18f, 78.03f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 967.76f, 65.18f, 77.80f }, { 968.05f, 65.18f, 78.03f }, { 967.78f, 65.00f, 77.80f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 968.33f, 65.33f, 78.30f }, { 968.01f, 65.46f, 78.08f }, { 968.32f, 65.46f, 78.34f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 968.01f, 65.46f, 78.08f }, { 968.33f, 65.32f, 78.29f }, { 968.03f, 65.32f, 78.05f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 968.03f, 65.33f, 78.05f }, { 967.73f, 65.32f, 77.80f }, { 968.01f, 65.46f, 78.08f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 967.69f, 65.46f, 77.83f }, { 968.01f, 65.46f, 78.08f }, { 967.73f, 65.32f, 77.80f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 968.30f, 65.56f, 78.40f }, { 967.93f, 65.61f, 78.18f }, { 968.27f, 65.61f, 78.46f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 967.93f, 65.61f, 78.18f }, { 968.30f, 65.56f, 78.40f }, { 967.97f, 65.56f, 78.13f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 967.64f, 65.56f, 77.86f }, { 967.93f, 65.61f, 78.18f }, { 967.97f, 65.56f, 78.13f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 967.93f, 65.61f, 78.18f }, { 967.64f, 65.56f, 77.86f }, { 967.58f, 65.61f, 77.90f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 968.23f, 65.64f, 78.52f }, { 967.84f, 65.66f, 78.29f }, { 968.20f, 65.66f, 78.58f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 967.84f, 65.66f, 78.28f }, { 968.23f, 65.64f, 78.52f }, { 967.88f, 65.64f, 78.23f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 967.53f, 65.64f, 77.95f }, { 967.84f, 65.66f, 78.29f }, { 967.88f, 65.64f, 78.23f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 967.84f, 65.66f, 78.28f }, { 967.53f, 65.64f, 77.95f }, { 967.48f, 65.66f, 77.99f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 967.76f, 65.00f, 77.78f }, { 967.74f, 65.18f, 77.78f }, { 967.76f, 65.18f, 77.79f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 967.76f, 65.18f, 77.80f }, { 967.78f, 65.00f, 77.80f }, { 967.76f, 65.00f, 77.78f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 967.71f, 65.33f, 77.78f }, { 967.67f, 65.46f, 77.81f }, { 967.69f, 65.46f, 77.82f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 967.69f, 65.46f, 77.83f }, { 967.73f, 65.32f, 77.80f }, { 967.71f, 65.32f, 77.78f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 967.62f, 65.56f, 77.84f }, { 967.58f, 65.61f, 77.90f }, { 967.64f, 65.56f, 77.86f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 967.58f, 65.61f, 77.90f }, { 967.62f, 65.56f, 77.84f }, { 967.56f, 65.61f, 77.88f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 967.51f, 65.64f, 77.93f }, { 967.48f, 65.66f, 77.99f }, { 967.53f, 65.64f, 77.95f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 967.48f, 65.66f, 77.99f }, { 967.51f, 65.64f, 77.93f }, { 967.46f, 65.66f, 77.97f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 968.29f, 65.59f, 78.48f }, { 968.32f, 65.54f, 78.42f }, { 968.32f, 65.56f, 78.42f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 968.32f, 65.56f, 78.42f }, { 968.29f, 65.61f, 78.48f }, { 968.29f, 65.59f, 78.48f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 967.56f, 65.59f, 77.88f }, { 967.62f, 65.56f, 77.84f }, { 967.61f, 65.54f, 77.84f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 967.62f, 65.56f, 77.84f }, { 967.56f, 65.59f, 77.88f }, { 967.56f, 65.61f, 77.88f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 968.35f, 65.17f, 78.29f }, { 968.35f, 65.00f, 78.27f }, { 968.36f, 65.18f, 78.28f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 968.35f, 65.00f, 78.27f }, { 968.35f, 65.17f, 78.29f }, { 968.35f, 65.00f, 78.27f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 967.74f, 65.17f, 77.78f }, { 967.76f, 65.00f, 77.78f }, { 967.75f, 65.00f, 77.78f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 967.76f, 65.00f, 77.78f }, { 967.73f, 65.17f, 77.78f }, { 967.74f, 65.18f, 77.78f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 968.34f, 65.45f, 78.36f }, { 968.35f, 65.32f, 78.32f }, { 968.35f, 65.32f, 78.31f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 968.36f, 65.33f, 78.31f }, { 968.34f, 65.46f, 78.36f }, { 968.34f, 65.45f, 78.36f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 967.66f, 65.45f, 77.81f }, { 967.71f, 65.32f, 77.78f }, { 967.71f, 65.32f, 77.79f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 967.71f, 65.33f, 77.78f }, { 967.66f, 65.45f, 77.81f }, { 967.67f, 65.46f, 77.81f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 968.22f, 65.64f, 78.60f }, { 968.25f, 65.62f, 78.54f }, { 968.26f, 65.64f, 78.54f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 968.25f, 65.64f, 78.54f }, { 968.22f, 65.66f, 78.60f }, { 968.22f, 65.64f, 78.60f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 967.46f, 65.64f, 77.97f }, { 967.51f, 65.64f, 77.93f }, { 967.51f, 65.62f, 77.93f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 967.51f, 65.64f, 77.93f }, { 967.46f, 65.64f, 77.97f }, { 967.46f, 65.66f, 77.97f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 968.32f, 65.54f, 78.42f }, { 968.34f, 65.45f, 78.36f }, { 968.34f, 65.46f, 78.36f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 968.34f, 65.46f, 78.36f }, { 968.32f, 65.56f, 78.42f }, { 968.32f, 65.54f, 78.42f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 968.32f, 65.56f, 78.42f }, { 968.34f, 65.46f, 78.36f }, { 968.30f, 65.56f, 78.40f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 968.34f, 65.46f, 78.36f }, { 968.32f, 65.46f, 78.34f }, { 968.30f, 65.56f, 78.40f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 967.97f, 65.56f, 78.13f }, { 968.30f, 65.56f, 78.40f }, { 968.32f, 65.46f, 78.34f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 967.97f, 65.56f, 78.13f }, { 968.32f, 65.46f, 78.34f }, { 968.01f, 65.46f, 78.08f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 967.69f, 65.46f, 77.83f }, { 967.97f, 65.56f, 78.13f }, { 968.01f, 65.46f, 78.08f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 967.97f, 65.56f, 78.13f }, { 967.69f, 65.46f, 77.82f }, { 967.64f, 65.56f, 77.86f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 967.64f, 65.56f, 77.86f }, { 967.67f, 65.46f, 77.81f }, { 967.62f, 65.56f, 77.84f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 967.67f, 65.46f, 77.81f }, { 967.64f, 65.56f, 77.86f }, { 967.69f, 65.46f, 77.82f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 967.62f, 65.56f, 77.84f }, { 967.67f, 65.46f, 77.81f }, { 967.61f, 65.54f, 77.84f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 967.66f, 65.45f, 77.81f }, { 967.61f, 65.54f, 77.84f }, { 967.67f, 65.46f, 77.81f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 968.35f, 65.32f, 78.32f }, { 968.36f, 65.18f, 78.28f }, { 968.35f, 65.32f, 78.31f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 968.36f, 65.18f, 78.28f }, { 968.35f, 65.32f, 78.32f }, { 968.35f, 65.17f, 78.29f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 968.36f, 65.33f, 78.31f }, { 968.36f, 65.18f, 78.28f }, { 968.33f, 65.32f, 78.29f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 968.36f, 65.18f, 78.28f }, { 968.34f, 65.18f, 78.27f }, { 968.33f, 65.32f, 78.29f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 968.03f, 65.33f, 78.05f }, { 968.33f, 65.32f, 78.29f }, { 968.34f, 65.18f, 78.27f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 968.03f, 65.33f, 78.05f }, { 968.34f, 65.18f, 78.27f }, { 968.05f, 65.18f, 78.03f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 967.76f, 65.18f, 77.80f }, { 968.03f, 65.32f, 78.05f }, { 968.05f, 65.18f, 78.03f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 968.03f, 65.33f, 78.05f }, { 967.76f, 65.18f, 77.79f }, { 967.73f, 65.32f, 77.80f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 967.73f, 65.33f, 77.80f }, { 967.74f, 65.18f, 77.78f }, { 967.71f, 65.32f, 77.78f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 967.74f, 65.18f, 77.78f }, { 967.73f, 65.32f, 77.80f }, { 967.76f, 65.18f, 77.79f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 967.71f, 65.33f, 77.78f }, { 967.74f, 65.18f, 77.78f }, { 967.71f, 65.32f, 77.79f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 967.71f, 65.32f, 77.79f }, { 967.74f, 65.18f, 77.78f }, { 967.73f, 65.17f, 77.78f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 968.25f, 65.62f, 78.54f }, { 968.29f, 65.59f, 78.48f }, { 968.29f, 65.61f, 78.48f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 968.29f, 65.61f, 78.48f }, { 968.26f, 65.64f, 78.54f }, { 968.25f, 65.62f, 78.54f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 968.27f, 65.61f, 78.46f }, { 968.23f, 65.64f, 78.52f }, { 968.29f, 65.61f, 78.48f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 968.29f, 65.61f, 78.48f }, { 968.23f, 65.64f, 78.52f }, { 968.26f, 65.64f, 78.54f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 968.27f, 65.61f, 78.46f }, { 967.88f, 65.64f, 78.23f }, { 968.23f, 65.64f, 78.52f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 968.27f, 65.61f, 78.46f }, { 967.93f, 65.61f, 78.18f }, { 967.88f, 65.64f, 78.23f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 967.58f, 65.61f, 77.90f }, { 967.88f, 65.64f, 78.23f }, { 967.93f, 65.61f, 78.18f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 967.88f, 65.64f, 78.23f }, { 967.58f, 65.61f, 77.90f }, { 967.53f, 65.64f, 77.95f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 967.56f, 65.61f, 77.88f }, { 967.53f, 65.64f, 77.95f }, { 967.58f, 65.61f, 77.90f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 967.53f, 65.64f, 77.95f }, { 967.56f, 65.61f, 77.88f }, { 967.51f, 65.64f, 77.93f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 967.51f, 65.64f, 77.93f }, { 967.56f, 65.61f, 77.88f }, { 967.51f, 65.62f, 77.93f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 967.56f, 65.59f, 77.88f }, { 967.51f, 65.62f, 77.93f }, { 967.56f, 65.61f, 77.88f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 967.89f, 65.66f, 78.96f }, { 968.22f, 65.66f, 78.60f }, { 968.20f, 65.66f, 78.58f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 968.22f, 65.66f, 78.60f }, { 967.89f, 65.66f, 78.96f }, { 967.91f, 65.66f, 78.97f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 968.20f, 65.66f, 78.58f }, { 967.53f, 65.66f, 78.66f }, { 967.89f, 65.66f, 78.96f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 967.84f, 65.66f, 78.28f }, { 967.53f, 65.66f, 78.66f }, { 968.20f, 65.66f, 78.58f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 967.48f, 65.66f, 77.99f }, { 967.53f, 65.66f, 78.66f }, { 967.84f, 65.66f, 78.29f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 967.53f, 65.66f, 78.66f }, { 967.48f, 65.66f, 77.99f }, { 967.17f, 65.66f, 78.37f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 967.46f, 65.66f, 77.97f }, { 967.15f, 65.66f, 78.35f }, { 967.48f, 65.66f, 77.99f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 967.48f, 65.66f, 77.99f }, { 967.15f, 65.66f, 78.35f }, { 967.17f, 65.66f, 78.37f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 968.22f, 65.66f, 78.60f }, { 967.91f, 65.66f, 78.97f }, { 968.22f, 65.64f, 78.60f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 968.22f, 65.64f, 78.60f }, { 967.91f, 65.66f, 78.97f }, { 967.91f, 65.64f, 78.98f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 967.46f, 65.64f, 77.97f }, { 967.15f, 65.66f, 78.35f }, { 967.46f, 65.66f, 77.97f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 967.15f, 65.66f, 78.35f }, { 967.46f, 65.64f, 77.97f }, { 967.15f, 65.64f, 78.35f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 962.42f, 65.46f, 73.50f }, { 962.43f, 65.32f, 73.45f }, { 962.39f, 65.46f, 73.48f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 962.43f, 65.33f, 73.45f }, { 962.41f, 65.32f, 73.43f }, { 962.39f, 65.46f, 73.48f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 962.31f, 65.64f, 73.66f }, { 962.27f, 65.66f, 73.72f }, { 962.33f, 65.64f, 73.68f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 962.33f, 65.64f, 73.67f }, { 962.27f, 65.66f, 73.72f }, { 962.29f, 65.66f, 73.73f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 962.43f, 65.18f, 73.42f }, { 962.43f, 65.00f, 73.40f }, { 962.41f, 65.18f, 73.40f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 962.42f, 65.00f, 73.40f }, { 962.40f, 65.00f, 73.38f }, { 962.41f, 65.18f, 73.40f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 962.37f, 65.56f, 73.54f }, { 962.34f, 65.61f, 73.60f }, { 962.39f, 65.56f, 73.55f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 962.39f, 65.56f, 73.56f }, { 962.34f, 65.61f, 73.60f }, { 962.37f, 65.61f, 73.61f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 962.40f, 65.00f, 73.38f }, { 962.12f, 65.18f, 73.17f }, { 962.41f, 65.18f, 73.40f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 962.12f, 65.18f, 73.17f }, { 962.40f, 65.00f, 73.38f }, { 962.13f, 65.00f, 73.16f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 962.13f, 65.00f, 73.16f }, { 961.85f, 65.00f, 72.93f }, { 962.12f, 65.18f, 73.17f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 961.84f, 65.18f, 72.93f }, { 962.12f, 65.18f, 73.17f }, { 961.85f, 65.00f, 72.93f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 962.41f, 65.33f, 73.43f }, { 962.08f, 65.46f, 73.22f }, { 962.39f, 65.46f, 73.48f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 962.08f, 65.46f, 73.22f }, { 962.41f, 65.32f, 73.43f }, { 962.11f, 65.32f, 73.19f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 962.11f, 65.33f, 73.19f }, { 961.81f, 65.32f, 72.94f }, { 962.08f, 65.46f, 73.22f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 961.76f, 65.46f, 72.96f }, { 962.08f, 65.46f, 73.22f }, { 961.81f, 65.32f, 72.94f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 962.37f, 65.56f, 73.54f }, { 962.00f, 65.61f, 73.31f }, { 962.34f, 65.61f, 73.60f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 962.00f, 65.61f, 73.31f }, { 962.37f, 65.56f, 73.54f }, { 962.04f, 65.56f, 73.27f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 961.71f, 65.56f, 73.00f }, { 962.00f, 65.61f, 73.31f }, { 962.04f, 65.56f, 73.27f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 962.00f, 65.61f, 73.31f }, { 961.71f, 65.56f, 73.00f }, { 961.66f, 65.61f, 73.03f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 962.31f, 65.64f, 73.66f }, { 961.91f, 65.66f, 73.42f }, { 962.27f, 65.66f, 73.72f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 961.91f, 65.66f, 73.42f }, { 962.31f, 65.64f, 73.66f }, { 961.96f, 65.64f, 73.37f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 961.61f, 65.64f, 73.08f }, { 961.91f, 65.66f, 73.42f }, { 961.96f, 65.64f, 73.37f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 961.91f, 65.66f, 73.42f }, { 961.61f, 65.64f, 73.08f }, { 961.56f, 65.66f, 73.13f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 961.83f, 65.00f, 72.92f }, { 961.81f, 65.18f, 72.91f }, { 961.84f, 65.18f, 72.93f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 961.84f, 65.18f, 72.93f }, { 961.85f, 65.00f, 72.93f }, { 961.83f, 65.00f, 72.91f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 961.78f, 65.33f, 72.92f }, { 961.74f, 65.46f, 72.94f }, { 961.76f, 65.46f, 72.96f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 961.76f, 65.46f, 72.96f }, { 961.81f, 65.32f, 72.94f }, { 961.78f, 65.32f, 72.92f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 961.69f, 65.56f, 72.98f }, { 961.66f, 65.61f, 73.03f }, { 961.71f, 65.56f, 73.00f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 961.66f, 65.61f, 73.03f }, { 961.69f, 65.56f, 72.98f }, { 961.63f, 65.61f, 73.01f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 961.58f, 65.64f, 73.06f }, { 961.56f, 65.66f, 73.13f }, { 961.61f, 65.64f, 73.08f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 961.56f, 65.66f, 73.13f }, { 961.58f, 65.64f, 73.06f }, { 961.53f, 65.66f, 73.11f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 962.37f, 65.59f, 73.62f }, { 962.39f, 65.54f, 73.56f }, { 962.39f, 65.56f, 73.55f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 962.39f, 65.56f, 73.56f }, { 962.37f, 65.61f, 73.61f }, { 962.37f, 65.59f, 73.62f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 961.63f, 65.59f, 73.02f }, { 961.69f, 65.56f, 72.98f }, { 961.69f, 65.54f, 72.98f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 961.69f, 65.56f, 72.98f }, { 961.63f, 65.59f, 73.02f }, { 961.63f, 65.61f, 73.01f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 962.43f, 65.17f, 73.42f }, { 962.43f, 65.00f, 73.40f }, { 962.43f, 65.18f, 73.42f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 962.42f, 65.00f, 73.40f }, { 962.43f, 65.17f, 73.43f }, { 962.42f, 65.00f, 73.41f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 961.81f, 65.17f, 72.92f }, { 961.83f, 65.00f, 72.91f }, { 961.83f, 65.00f, 72.92f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 961.83f, 65.00f, 72.92f }, { 961.81f, 65.17f, 72.92f }, { 961.81f, 65.18f, 72.91f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 962.41f, 65.45f, 73.50f }, { 962.42f, 65.32f, 73.45f }, { 962.43f, 65.32f, 73.45f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 962.43f, 65.33f, 73.45f }, { 962.42f, 65.46f, 73.50f }, { 962.41f, 65.45f, 73.50f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 961.74f, 65.45f, 72.95f }, { 961.78f, 65.32f, 72.92f }, { 961.78f, 65.32f, 72.93f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 961.78f, 65.33f, 72.92f }, { 961.74f, 65.45f, 72.95f }, { 961.74f, 65.46f, 72.94f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 962.29f, 65.64f, 73.73f }, { 962.33f, 65.62f, 73.68f }, { 962.33f, 65.64f, 73.68f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 962.33f, 65.64f, 73.67f }, { 962.29f, 65.66f, 73.73f }, { 962.29f, 65.64f, 73.73f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 961.53f, 65.64f, 73.11f }, { 961.58f, 65.64f, 73.06f }, { 961.58f, 65.62f, 73.06f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 961.58f, 65.64f, 73.06f }, { 961.53f, 65.64f, 73.11f }, { 961.53f, 65.66f, 73.11f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 962.39f, 65.54f, 73.56f }, { 962.41f, 65.45f, 73.50f }, { 962.42f, 65.46f, 73.50f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 962.42f, 65.46f, 73.50f }, { 962.39f, 65.56f, 73.55f }, { 962.39f, 65.54f, 73.56f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 962.39f, 65.56f, 73.56f }, { 962.42f, 65.46f, 73.50f }, { 962.37f, 65.56f, 73.54f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 962.42f, 65.46f, 73.50f }, { 962.39f, 65.46f, 73.48f }, { 962.37f, 65.56f, 73.54f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 962.04f, 65.56f, 73.27f }, { 962.37f, 65.56f, 73.54f }, { 962.39f, 65.46f, 73.48f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 962.04f, 65.56f, 73.27f }, { 962.39f, 65.46f, 73.48f }, { 962.08f, 65.46f, 73.22f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 961.76f, 65.46f, 72.96f }, { 962.04f, 65.56f, 73.27f }, { 962.08f, 65.46f, 73.22f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 962.04f, 65.56f, 73.27f }, { 961.76f, 65.46f, 72.96f }, { 961.71f, 65.56f, 73.00f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 961.71f, 65.56f, 73.00f }, { 961.74f, 65.46f, 72.94f }, { 961.69f, 65.56f, 72.98f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 961.74f, 65.46f, 72.94f }, { 961.71f, 65.56f, 73.00f }, { 961.76f, 65.46f, 72.96f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 961.69f, 65.56f, 72.98f }, { 961.74f, 65.46f, 72.94f }, { 961.69f, 65.54f, 72.98f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 961.74f, 65.45f, 72.95f }, { 961.69f, 65.54f, 72.98f }, { 961.74f, 65.46f, 72.94f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 962.42f, 65.32f, 73.45f }, { 962.43f, 65.18f, 73.42f }, { 962.43f, 65.32f, 73.45f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 962.43f, 65.18f, 73.42f }, { 962.42f, 65.32f, 73.45f }, { 962.43f, 65.17f, 73.43f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 962.43f, 65.33f, 73.45f }, { 962.43f, 65.18f, 73.42f }, { 962.41f, 65.32f, 73.43f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 962.43f, 65.18f, 73.42f }, { 962.41f, 65.18f, 73.40f }, { 962.41f, 65.32f, 73.43f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 962.11f, 65.33f, 73.19f }, { 962.41f, 65.32f, 73.43f }, { 962.41f, 65.18f, 73.40f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 962.11f, 65.33f, 73.19f }, { 962.41f, 65.18f, 73.40f }, { 962.12f, 65.18f, 73.17f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 961.84f, 65.18f, 72.93f }, { 962.11f, 65.32f, 73.19f }, { 962.12f, 65.18f, 73.17f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 962.11f, 65.33f, 73.19f }, { 961.84f, 65.18f, 72.93f }, { 961.81f, 65.32f, 72.94f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 961.81f, 65.33f, 72.94f }, { 961.81f, 65.18f, 72.91f }, { 961.78f, 65.32f, 72.92f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 961.81f, 65.18f, 72.91f }, { 961.81f, 65.32f, 72.94f }, { 961.84f, 65.18f, 72.93f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 961.78f, 65.33f, 72.92f }, { 961.81f, 65.18f, 72.91f }, { 961.78f, 65.32f, 72.93f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 961.78f, 65.32f, 72.93f }, { 961.81f, 65.18f, 72.91f }, { 961.81f, 65.17f, 72.92f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 962.33f, 65.62f, 73.68f }, { 962.37f, 65.59f, 73.62f }, { 962.37f, 65.61f, 73.61f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 962.37f, 65.61f, 73.61f }, { 962.33f, 65.64f, 73.68f }, { 962.33f, 65.62f, 73.68f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 962.35f, 65.61f, 73.60f }, { 962.31f, 65.64f, 73.66f }, { 962.37f, 65.61f, 73.61f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 962.37f, 65.61f, 73.61f }, { 962.31f, 65.64f, 73.66f }, { 962.33f, 65.64f, 73.68f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 962.35f, 65.61f, 73.60f }, { 961.96f, 65.64f, 73.37f }, { 962.31f, 65.64f, 73.66f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 962.35f, 65.61f, 73.60f }, { 962.00f, 65.61f, 73.31f }, { 961.96f, 65.64f, 73.37f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 961.66f, 65.61f, 73.03f }, { 961.96f, 65.64f, 73.37f }, { 962.00f, 65.61f, 73.31f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 961.96f, 65.64f, 73.37f }, { 961.66f, 65.61f, 73.03f }, { 961.61f, 65.64f, 73.08f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 961.63f, 65.61f, 73.01f }, { 961.61f, 65.64f, 73.08f }, { 961.66f, 65.61f, 73.03f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 961.61f, 65.64f, 73.08f }, { 961.63f, 65.61f, 73.01f }, { 961.58f, 65.64f, 73.06f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 961.58f, 65.64f, 73.06f }, { 961.63f, 65.61f, 73.01f }, { 961.58f, 65.62f, 73.06f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 961.63f, 65.59f, 73.02f }, { 961.58f, 65.62f, 73.06f }, { 961.63f, 65.61f, 73.01f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 961.96f, 65.66f, 74.09f }, { 962.29f, 65.66f, 73.73f }, { 962.27f, 65.66f, 73.72f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 962.29f, 65.66f, 73.73f }, { 961.96f, 65.66f, 74.09f }, { 961.98f, 65.66f, 74.11f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 962.27f, 65.66f, 73.72f }, { 961.60f, 65.66f, 73.80f }, { 961.96f, 65.66f, 74.09f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 961.91f, 65.66f, 73.42f }, { 961.60f, 65.66f, 73.80f }, { 962.27f, 65.66f, 73.72f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 961.56f, 65.66f, 73.13f }, { 961.60f, 65.66f, 73.80f }, { 961.91f, 65.66f, 73.42f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 961.60f, 65.66f, 73.80f }, { 961.56f, 65.66f, 73.13f }, { 961.25f, 65.66f, 73.51f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 961.53f, 65.66f, 73.11f }, { 961.22f, 65.66f, 73.49f }, { 961.56f, 65.66f, 73.13f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 961.56f, 65.66f, 73.13f }, { 961.22f, 65.66f, 73.49f }, { 961.25f, 65.66f, 73.51f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 962.29f, 65.66f, 73.73f }, { 961.98f, 65.66f, 74.11f }, { 962.29f, 65.64f, 73.73f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 962.29f, 65.64f, 73.73f }, { 961.98f, 65.66f, 74.11f }, { 961.98f, 65.64f, 74.11f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 961.53f, 65.64f, 73.11f }, { 961.22f, 65.66f, 73.49f }, { 961.53f, 65.66f, 73.11f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 961.22f, 65.66f, 73.49f }, { 961.53f, 65.64f, 73.11f }, { 961.22f, 65.64f, 73.49f }, 65.00f, 0.00f },   // waterfeature_02 props 448
    { { 906.52f, 70.11f, -12.79f }, { 906.48f, 69.94f, -12.85f }, { 906.48f, 70.12f, -12.79f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 906.49f, 69.94f, -12.85f }, { 906.45f, 69.94f, -12.84f }, { 906.48f, 70.12f, -12.79f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 906.56f, 70.34f, -12.54f }, { 906.58f, 70.37f, -12.46f }, { 906.60f, 70.34f, -12.55f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 906.60f, 70.34f, -12.55f }, { 906.58f, 70.37f, -12.46f }, { 906.62f, 70.37f, -12.46f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 906.46f, 69.75f, -12.88f }, { 906.44f, 69.52f, -12.89f }, { 906.42f, 69.75f, -12.87f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 906.44f, 69.52f, -12.89f }, { 906.40f, 69.52f, -12.88f }, { 906.42f, 69.75f, -12.87f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 906.51f, 70.24f, -12.71f }, { 906.54f, 70.31f, -12.63f }, { 906.55f, 70.24f, -12.71f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 906.55f, 70.24f, -12.71f }, { 906.54f, 70.31f, -12.63f }, { 906.58f, 70.31f, -12.64f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 906.40f, 69.52f, -12.88f }, { 905.94f, 69.75f, -12.82f }, { 906.42f, 69.75f, -12.87f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 905.94f, 69.75f, -12.82f }, { 906.40f, 69.52f, -12.88f }, { 905.94f, 69.52f, -12.84f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 905.94f, 69.52f, -12.84f }, { 905.48f, 69.52f, -12.79f }, { 905.94f, 69.75f, -12.82f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 905.46f, 69.75f, -12.78f }, { 905.94f, 69.75f, -12.82f }, { 905.48f, 69.52f, -12.79f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 906.45f, 69.94f, -12.84f }, { 905.95f, 70.12f, -12.74f }, { 906.48f, 70.12f, -12.79f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 905.95f, 70.11f, -12.74f }, { 906.45f, 69.94f, -12.84f }, { 905.95f, 69.94f, -12.79f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 905.95f, 69.94f, -12.79f }, { 905.45f, 69.94f, -12.74f }, { 905.95f, 70.12f, -12.74f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 905.43f, 70.11f, -12.68f }, { 905.95f, 70.12f, -12.74f }, { 905.45f, 69.94f, -12.74f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 906.51f, 70.24f, -12.71f }, { 905.97f, 70.31f, -12.58f }, { 906.54f, 70.31f, -12.63f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 905.97f, 70.31f, -12.58f }, { 906.51f, 70.24f, -12.71f }, { 905.96f, 70.24f, -12.66f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 905.41f, 70.24f, -12.60f }, { 905.97f, 70.31f, -12.58f }, { 905.96f, 70.24f, -12.66f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 905.97f, 70.31f, -12.58f }, { 905.41f, 70.24f, -12.60f }, { 905.39f, 70.31f, -12.52f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 906.56f, 70.34f, -12.54f }, { 905.99f, 70.37f, -12.40f }, { 906.58f, 70.37f, -12.46f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 905.99f, 70.37f, -12.40f }, { 906.56f, 70.34f, -12.54f }, { 905.98f, 70.34f, -12.48f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 905.39f, 70.34f, -12.43f }, { 905.99f, 70.37f, -12.40f }, { 905.98f, 70.34f, -12.48f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 905.99f, 70.37f, -12.40f }, { 905.39f, 70.34f, -12.43f }, { 905.39f, 70.37f, -12.34f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 905.44f, 69.52f, -12.79f }, { 905.42f, 69.75f, -12.77f }, { 905.46f, 69.75f, -12.78f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 905.46f, 69.75f, -12.78f }, { 905.48f, 69.52f, -12.79f }, { 905.44f, 69.52f, -12.79f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 905.41f, 69.94f, -12.74f }, { 905.39f, 70.12f, -12.68f }, { 905.43f, 70.12f, -12.68f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 905.43f, 70.11f, -12.68f }, { 905.45f, 69.94f, -12.74f }, { 905.41f, 69.94f, -12.74f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 905.37f, 70.24f, -12.60f }, { 905.39f, 70.31f, -12.52f }, { 905.41f, 70.24f, -12.60f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 905.39f, 70.31f, -12.52f }, { 905.37f, 70.24f, -12.60f }, { 905.35f, 70.31f, -12.52f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 905.35f, 70.34f, -12.42f }, { 905.39f, 70.37f, -12.34f }, { 905.39f, 70.34f, -12.43f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 905.39f, 70.37f, -12.34f }, { 905.35f, 70.34f, -12.42f }, { 905.35f, 70.37f, -12.34f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 906.58f, 70.28f, -12.63f }, { 906.55f, 70.22f, -12.71f }, { 906.55f, 70.24f, -12.71f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 906.55f, 70.24f, -12.71f }, { 906.58f, 70.31f, -12.64f }, { 906.58f, 70.28f, -12.63f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 905.36f, 70.28f, -12.51f }, { 905.37f, 70.24f, -12.60f }, { 905.37f, 70.22f, -12.59f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 905.37f, 70.24f, -12.60f }, { 905.35f, 70.28f, -12.51f }, { 905.35f, 70.31f, -12.52f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 906.46f, 69.74f, -12.87f }, { 906.44f, 69.52f, -12.89f }, { 906.46f, 69.75f, -12.87f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 906.44f, 69.52f, -12.89f }, { 906.46f, 69.74f, -12.87f }, { 906.44f, 69.52f, -12.88f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 905.43f, 69.74f, -12.76f }, { 905.44f, 69.52f, -12.79f }, { 905.45f, 69.52f, -12.78f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 905.44f, 69.52f, -12.79f }, { 905.43f, 69.74f, -12.76f }, { 905.42f, 69.75f, -12.77f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 906.52f, 70.10f, -12.79f }, { 906.49f, 69.93f, -12.84f }, { 906.48f, 69.94f, -12.85f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 906.49f, 69.94f, -12.85f }, { 906.52f, 70.12f, -12.79f }, { 906.52f, 70.10f, -12.79f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 905.39f, 70.10f, -12.67f }, { 905.41f, 69.94f, -12.74f }, { 905.41f, 69.93f, -12.73f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 905.41f, 69.94f, -12.74f }, { 905.39f, 70.10f, -12.67f }, { 905.39f, 70.12f, -12.68f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 906.62f, 70.34f, -12.46f }, { 906.60f, 70.32f, -12.54f }, { 906.60f, 70.34f, -12.55f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 906.60f, 70.34f, -12.55f }, { 906.62f, 70.37f, -12.46f }, { 906.62f, 70.34f, -12.46f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 905.35f, 70.34f, -12.33f }, { 905.35f, 70.34f, -12.42f }, { 905.35f, 70.32f, -12.42f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 905.35f, 70.34f, -12.42f }, { 905.35f, 70.34f, -12.33f }, { 905.35f, 70.37f, -12.34f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 906.55f, 70.22f, -12.71f }, { 906.52f, 70.10f, -12.79f }, { 906.52f, 70.12f, -12.79f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 906.52f, 70.11f, -12.79f }, { 906.55f, 70.24f, -12.71f }, { 906.55f, 70.22f, -12.71f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 906.55f, 70.24f, -12.71f }, { 906.52f, 70.12f, -12.79f }, { 906.51f, 70.24f, -12.71f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 906.52f, 70.11f, -12.79f }, { 906.48f, 70.12f, -12.79f }, { 906.51f, 70.24f, -12.71f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 905.96f, 70.24f, -12.66f }, { 906.51f, 70.24f, -12.71f }, { 906.48f, 70.12f, -12.79f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 905.96f, 70.24f, -12.66f }, { 906.48f, 70.12f, -12.79f }, { 905.95f, 70.12f, -12.74f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 905.43f, 70.11f, -12.68f }, { 905.96f, 70.24f, -12.66f }, { 905.95f, 70.12f, -12.74f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 905.96f, 70.24f, -12.66f }, { 905.43f, 70.12f, -12.68f }, { 905.41f, 70.24f, -12.60f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 905.41f, 70.24f, -12.60f }, { 905.39f, 70.12f, -12.68f }, { 905.37f, 70.24f, -12.60f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 905.39f, 70.11f, -12.68f }, { 905.41f, 70.24f, -12.60f }, { 905.43f, 70.12f, -12.68f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 905.37f, 70.24f, -12.60f }, { 905.39f, 70.12f, -12.68f }, { 905.37f, 70.22f, -12.59f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 905.39f, 70.10f, -12.67f }, { 905.37f, 70.22f, -12.59f }, { 905.39f, 70.12f, -12.68f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 906.49f, 69.93f, -12.84f }, { 906.46f, 69.75f, -12.87f }, { 906.48f, 69.94f, -12.85f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 906.46f, 69.75f, -12.88f }, { 906.49f, 69.93f, -12.84f }, { 906.46f, 69.74f, -12.87f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 906.49f, 69.94f, -12.85f }, { 906.46f, 69.75f, -12.87f }, { 906.45f, 69.94f, -12.84f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 906.46f, 69.75f, -12.88f }, { 906.42f, 69.75f, -12.87f }, { 906.45f, 69.94f, -12.84f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 905.95f, 69.94f, -12.79f }, { 906.45f, 69.94f, -12.84f }, { 906.42f, 69.75f, -12.87f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 905.95f, 69.94f, -12.79f }, { 906.42f, 69.75f, -12.87f }, { 905.94f, 69.75f, -12.82f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 905.46f, 69.75f, -12.78f }, { 905.95f, 69.94f, -12.79f }, { 905.94f, 69.75f, -12.82f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 905.95f, 69.94f, -12.79f }, { 905.46f, 69.75f, -12.78f }, { 905.45f, 69.94f, -12.74f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 905.45f, 69.94f, -12.74f }, { 905.42f, 69.75f, -12.77f }, { 905.41f, 69.94f, -12.74f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 905.42f, 69.75f, -12.77f }, { 905.45f, 69.94f, -12.74f }, { 905.46f, 69.75f, -12.78f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 905.41f, 69.94f, -12.74f }, { 905.42f, 69.75f, -12.77f }, { 905.41f, 69.93f, -12.73f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 905.41f, 69.93f, -12.73f }, { 905.42f, 69.75f, -12.77f }, { 905.43f, 69.74f, -12.76f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 906.60f, 70.32f, -12.54f }, { 906.58f, 70.28f, -12.63f }, { 906.58f, 70.31f, -12.64f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 906.58f, 70.31f, -12.64f }, { 906.60f, 70.34f, -12.55f }, { 906.60f, 70.32f, -12.54f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 906.54f, 70.31f, -12.63f }, { 906.56f, 70.34f, -12.54f }, { 906.58f, 70.31f, -12.64f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 906.58f, 70.31f, -12.64f }, { 906.56f, 70.34f, -12.54f }, { 906.60f, 70.34f, -12.55f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 906.54f, 70.31f, -12.63f }, { 905.98f, 70.34f, -12.48f }, { 906.56f, 70.34f, -12.54f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 906.54f, 70.31f, -12.63f }, { 905.97f, 70.31f, -12.58f }, { 905.98f, 70.34f, -12.48f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 905.39f, 70.31f, -12.52f }, { 905.98f, 70.34f, -12.48f }, { 905.97f, 70.31f, -12.58f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 905.98f, 70.34f, -12.48f }, { 905.39f, 70.31f, -12.52f }, { 905.39f, 70.34f, -12.43f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 905.35f, 70.31f, -12.52f }, { 905.39f, 70.34f, -12.43f }, { 905.39f, 70.31f, -12.52f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 905.39f, 70.34f, -12.43f }, { 905.35f, 70.31f, -12.52f }, { 905.35f, 70.34f, -12.42f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 905.35f, 70.34f, -12.42f }, { 905.35f, 70.31f, -12.52f }, { 905.35f, 70.32f, -12.42f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 905.36f, 70.28f, -12.51f }, { 905.35f, 70.32f, -12.42f }, { 905.35f, 70.31f, -12.52f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 906.65f, 70.37f, -11.82f }, { 906.62f, 70.37f, -12.46f }, { 906.58f, 70.37f, -12.46f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 906.62f, 70.37f, -12.46f }, { 906.65f, 70.37f, -11.82f }, { 906.68f, 70.37f, -11.83f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 906.59f, 70.37f, -12.46f }, { 906.05f, 70.37f, -11.77f }, { 906.65f, 70.37f, -11.82f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 905.99f, 70.37f, -12.40f }, { 906.05f, 70.37f, -11.77f }, { 906.58f, 70.37f, -12.46f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 905.39f, 70.37f, -12.34f }, { 906.05f, 70.37f, -11.77f }, { 905.99f, 70.37f, -12.40f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 906.05f, 70.37f, -11.77f }, { 905.39f, 70.37f, -12.34f }, { 905.45f, 70.37f, -11.71f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 905.35f, 70.37f, -12.34f }, { 905.41f, 70.37f, -11.70f }, { 905.39f, 70.37f, -12.34f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 905.39f, 70.37f, -12.34f }, { 905.41f, 70.37f, -11.70f }, { 905.45f, 70.37f, -11.71f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 906.62f, 70.37f, -12.46f }, { 906.68f, 70.37f, -11.83f }, { 906.62f, 70.34f, -12.46f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 906.62f, 70.34f, -12.46f }, { 906.68f, 70.37f, -11.83f }, { 906.68f, 70.34f, -11.83f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 905.35f, 70.34f, -12.33f }, { 905.41f, 70.37f, -11.70f }, { 905.35f, 70.37f, -12.34f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 905.41f, 70.37f, -11.70f }, { 905.35f, 70.34f, -12.33f }, { 905.41f, 70.34f, -11.70f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 899.51f, 70.11f, -12.10f }, { 899.48f, 69.94f, -12.16f }, { 899.47f, 70.12f, -12.10f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 899.48f, 69.94f, -12.16f }, { 899.44f, 69.94f, -12.15f }, { 899.47f, 70.12f, -12.10f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 899.56f, 70.34f, -11.85f }, { 899.58f, 70.37f, -11.77f }, { 899.59f, 70.34f, -11.86f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 899.60f, 70.34f, -11.86f }, { 899.58f, 70.37f, -11.77f }, { 899.62f, 70.37f, -11.77f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 899.45f, 69.75f, -12.19f }, { 899.43f, 69.52f, -12.20f }, { 899.42f, 69.75f, -12.18f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 899.43f, 69.52f, -12.20f }, { 899.40f, 69.52f, -12.19f }, { 899.42f, 69.75f, -12.18f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 899.50f, 70.24f, -12.02f }, { 899.54f, 70.31f, -11.94f }, { 899.54f, 70.24f, -12.02f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 899.54f, 70.24f, -12.03f }, { 899.54f, 70.31f, -11.94f }, { 899.57f, 70.31f, -11.95f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 899.40f, 69.52f, -12.19f }, { 898.94f, 69.75f, -12.13f }, { 899.42f, 69.75f, -12.18f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 898.94f, 69.75f, -12.13f }, { 899.40f, 69.52f, -12.19f }, { 898.94f, 69.52f, -12.15f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 898.94f, 69.52f, -12.15f }, { 898.48f, 69.52f, -12.10f }, { 898.94f, 69.75f, -12.13f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 898.46f, 69.75f, -12.09f }, { 898.94f, 69.75f, -12.13f }, { 898.48f, 69.52f, -12.10f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 899.44f, 69.94f, -12.15f }, { 898.95f, 70.12f, -12.05f }, { 899.47f, 70.12f, -12.10f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 898.95f, 70.11f, -12.05f }, { 899.44f, 69.94f, -12.15f }, { 898.94f, 69.94f, -12.10f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 898.94f, 69.94f, -12.10f }, { 898.44f, 69.94f, -12.05f }, { 898.95f, 70.12f, -12.05f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 898.42f, 70.11f, -11.99f }, { 898.95f, 70.12f, -12.05f }, { 898.44f, 69.94f, -12.05f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 899.50f, 70.24f, -12.02f }, { 898.96f, 70.31f, -11.89f }, { 899.54f, 70.31f, -11.94f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 898.96f, 70.31f, -11.89f }, { 899.50f, 70.24f, -12.02f }, { 898.95f, 70.24f, -11.97f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 898.40f, 70.24f, -11.91f }, { 898.96f, 70.31f, -11.89f }, { 898.95f, 70.24f, -11.97f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 898.96f, 70.31f, -11.89f }, { 898.40f, 70.24f, -11.91f }, { 898.39f, 70.31f, -11.83f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 899.56f, 70.34f, -11.85f }, { 898.98f, 70.37f, -11.71f }, { 899.58f, 70.37f, -11.77f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 898.98f, 70.37f, -11.71f }, { 899.56f, 70.34f, -11.85f }, { 898.97f, 70.34f, -11.79f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 898.38f, 70.34f, -11.74f }, { 898.98f, 70.37f, -11.71f }, { 898.97f, 70.34f, -11.79f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 898.98f, 70.37f, -11.71f }, { 898.38f, 70.34f, -11.74f }, { 898.38f, 70.37f, -11.65f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 898.44f, 69.52f, -12.10f }, { 898.42f, 69.75f, -12.08f }, { 898.46f, 69.75f, -12.09f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 898.46f, 69.75f, -12.09f }, { 898.48f, 69.52f, -12.10f }, { 898.44f, 69.52f, -12.10f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 898.40f, 69.94f, -12.05f }, { 898.38f, 70.12f, -11.99f }, { 898.42f, 70.12f, -11.99f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 898.42f, 70.11f, -11.99f }, { 898.44f, 69.94f, -12.05f }, { 898.40f, 69.94f, -12.05f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 898.36f, 70.24f, -11.91f }, { 898.39f, 70.31f, -11.83f }, { 898.40f, 70.24f, -11.91f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 898.39f, 70.31f, -11.83f }, { 898.36f, 70.24f, -11.91f }, { 898.35f, 70.31f, -11.83f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 898.35f, 70.34f, -11.73f }, { 898.38f, 70.37f, -11.65f }, { 898.38f, 70.34f, -11.74f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 898.38f, 70.37f, -11.65f }, { 898.34f, 70.34f, -11.73f }, { 898.34f, 70.37f, -11.64f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 899.57f, 70.28f, -11.94f }, { 899.54f, 70.22f, -12.02f }, { 899.54f, 70.24f, -12.02f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 899.54f, 70.24f, -12.03f }, { 899.57f, 70.31f, -11.95f }, { 899.57f, 70.28f, -11.94f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 898.35f, 70.28f, -11.82f }, { 898.36f, 70.24f, -11.91f }, { 898.36f, 70.22f, -11.90f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 898.36f, 70.24f, -11.91f }, { 898.35f, 70.28f, -11.82f }, { 898.35f, 70.31f, -11.83f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 899.46f, 69.74f, -12.18f }, { 899.43f, 69.52f, -12.20f }, { 899.45f, 69.75f, -12.18f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 899.43f, 69.52f, -12.20f }, { 899.46f, 69.74f, -12.18f }, { 899.43f, 69.52f, -12.19f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 898.42f, 69.74f, -12.07f }, { 898.44f, 69.52f, -12.10f }, { 898.44f, 69.52f, -12.09f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 898.44f, 69.52f, -12.10f }, { 898.42f, 69.74f, -12.07f }, { 898.42f, 69.75f, -12.08f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 899.51f, 70.10f, -12.10f }, { 899.48f, 69.93f, -12.15f }, { 899.48f, 69.94f, -12.16f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 899.48f, 69.94f, -12.16f }, { 899.51f, 70.12f, -12.10f }, { 899.51f, 70.10f, -12.10f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 898.38f, 70.10f, -11.98f }, { 898.40f, 69.94f, -12.05f }, { 898.40f, 69.93f, -12.04f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 898.40f, 69.94f, -12.05f }, { 898.38f, 70.10f, -11.98f }, { 898.38f, 70.12f, -11.99f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 899.62f, 70.34f, -11.77f }, { 899.59f, 70.32f, -11.85f }, { 899.59f, 70.34f, -11.86f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 899.60f, 70.34f, -11.86f }, { 899.62f, 70.37f, -11.77f }, { 899.62f, 70.34f, -11.77f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 898.34f, 70.34f, -11.64f }, { 898.34f, 70.34f, -11.73f }, { 898.34f, 70.32f, -11.73f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 898.35f, 70.34f, -11.73f }, { 898.34f, 70.34f, -11.64f }, { 898.34f, 70.37f, -11.64f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 899.54f, 70.22f, -12.02f }, { 899.51f, 70.10f, -12.10f }, { 899.51f, 70.12f, -12.10f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 899.51f, 70.11f, -12.10f }, { 899.54f, 70.24f, -12.02f }, { 899.54f, 70.22f, -12.02f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 899.54f, 70.24f, -12.03f }, { 899.51f, 70.12f, -12.10f }, { 899.50f, 70.24f, -12.02f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 899.51f, 70.11f, -12.10f }, { 899.47f, 70.12f, -12.10f }, { 899.50f, 70.24f, -12.02f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 898.95f, 70.24f, -11.97f }, { 899.50f, 70.24f, -12.02f }, { 899.47f, 70.12f, -12.10f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 898.95f, 70.24f, -11.97f }, { 899.47f, 70.12f, -12.10f }, { 898.95f, 70.12f, -12.05f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 898.42f, 70.11f, -11.99f }, { 898.95f, 70.24f, -11.97f }, { 898.95f, 70.12f, -12.05f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 898.95f, 70.24f, -11.97f }, { 898.42f, 70.12f, -11.99f }, { 898.40f, 70.24f, -11.91f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 898.40f, 70.24f, -11.91f }, { 898.38f, 70.12f, -11.99f }, { 898.36f, 70.24f, -11.91f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 898.38f, 70.11f, -11.99f }, { 898.40f, 70.24f, -11.91f }, { 898.42f, 70.12f, -11.99f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 898.36f, 70.24f, -11.91f }, { 898.38f, 70.12f, -11.99f }, { 898.36f, 70.22f, -11.90f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 898.38f, 70.10f, -11.98f }, { 898.36f, 70.22f, -11.90f }, { 898.38f, 70.12f, -11.99f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 899.48f, 69.93f, -12.15f }, { 899.45f, 69.75f, -12.18f }, { 899.48f, 69.94f, -12.16f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 899.45f, 69.75f, -12.19f }, { 899.48f, 69.93f, -12.15f }, { 899.46f, 69.74f, -12.18f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 899.48f, 69.94f, -12.16f }, { 899.45f, 69.75f, -12.18f }, { 899.44f, 69.94f, -12.15f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 899.45f, 69.75f, -12.19f }, { 899.42f, 69.75f, -12.18f }, { 899.44f, 69.94f, -12.15f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 898.94f, 69.94f, -12.10f }, { 899.44f, 69.94f, -12.15f }, { 899.42f, 69.75f, -12.18f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 898.94f, 69.94f, -12.10f }, { 899.42f, 69.75f, -12.18f }, { 898.94f, 69.75f, -12.13f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 898.46f, 69.75f, -12.09f }, { 898.94f, 69.94f, -12.10f }, { 898.94f, 69.75f, -12.13f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 898.94f, 69.94f, -12.10f }, { 898.46f, 69.75f, -12.09f }, { 898.44f, 69.94f, -12.05f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 898.44f, 69.94f, -12.05f }, { 898.42f, 69.75f, -12.08f }, { 898.40f, 69.94f, -12.05f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 898.42f, 69.75f, -12.08f }, { 898.44f, 69.94f, -12.05f }, { 898.46f, 69.75f, -12.09f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 898.40f, 69.94f, -12.05f }, { 898.42f, 69.75f, -12.08f }, { 898.40f, 69.93f, -12.04f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 898.40f, 69.93f, -12.04f }, { 898.42f, 69.75f, -12.08f }, { 898.42f, 69.74f, -12.07f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 899.60f, 70.32f, -11.85f }, { 899.57f, 70.28f, -11.94f }, { 899.57f, 70.31f, -11.95f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 899.57f, 70.31f, -11.95f }, { 899.59f, 70.34f, -11.86f }, { 899.59f, 70.32f, -11.85f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 899.54f, 70.31f, -11.94f }, { 899.56f, 70.34f, -11.85f }, { 899.57f, 70.31f, -11.95f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 899.57f, 70.31f, -11.95f }, { 899.56f, 70.34f, -11.85f }, { 899.59f, 70.34f, -11.86f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 899.54f, 70.31f, -11.94f }, { 898.97f, 70.34f, -11.79f }, { 899.56f, 70.34f, -11.85f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 899.54f, 70.31f, -11.94f }, { 898.96f, 70.31f, -11.89f }, { 898.97f, 70.34f, -11.79f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 898.39f, 70.31f, -11.83f }, { 898.97f, 70.34f, -11.79f }, { 898.96f, 70.31f, -11.89f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 898.97f, 70.34f, -11.79f }, { 898.39f, 70.31f, -11.83f }, { 898.38f, 70.34f, -11.74f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 898.35f, 70.31f, -11.83f }, { 898.38f, 70.34f, -11.74f }, { 898.39f, 70.31f, -11.83f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 898.38f, 70.34f, -11.74f }, { 898.35f, 70.31f, -11.83f }, { 898.34f, 70.34f, -11.73f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 898.35f, 70.34f, -11.73f }, { 898.35f, 70.31f, -11.83f }, { 898.34f, 70.32f, -11.73f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 898.35f, 70.28f, -11.82f }, { 898.34f, 70.32f, -11.73f }, { 898.35f, 70.31f, -11.83f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 899.64f, 70.37f, -11.13f }, { 899.62f, 70.37f, -11.77f }, { 899.58f, 70.37f, -11.77f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 899.62f, 70.37f, -11.77f }, { 899.64f, 70.37f, -11.13f }, { 899.68f, 70.37f, -11.14f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 899.58f, 70.37f, -11.77f }, { 899.04f, 70.37f, -11.08f }, { 899.64f, 70.37f, -11.13f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 898.98f, 70.37f, -11.71f }, { 899.04f, 70.37f, -11.08f }, { 899.58f, 70.37f, -11.77f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 898.38f, 70.37f, -11.65f }, { 899.04f, 70.37f, -11.08f }, { 898.98f, 70.37f, -11.71f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 899.04f, 70.37f, -11.07f }, { 898.38f, 70.37f, -11.65f }, { 898.44f, 70.37f, -11.02f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 898.34f, 70.37f, -11.64f }, { 898.40f, 70.37f, -11.01f }, { 898.38f, 70.37f, -11.65f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 898.38f, 70.37f, -11.65f }, { 898.40f, 70.37f, -11.01f }, { 898.44f, 70.37f, -11.02f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 899.62f, 70.37f, -11.77f }, { 899.68f, 70.37f, -11.14f }, { 899.62f, 70.34f, -11.77f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 899.62f, 70.34f, -11.77f }, { 899.68f, 70.37f, -11.14f }, { 899.68f, 70.34f, -11.14f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 898.34f, 70.34f, -11.64f }, { 898.40f, 70.37f, -11.01f }, { 898.34f, 70.37f, -11.64f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 898.40f, 70.37f, -11.01f }, { 898.34f, 70.34f, -11.64f }, { 898.40f, 70.34f, -11.01f }, 69.52f, 0.00f },   // waterfeature_02 props 448
    { { 941.83f, 78.27f, -76.63f }, { 941.80f, 78.09f, -76.68f }, { 941.80f, 78.27f, -76.62f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.80f, 78.09f, -76.68f }, { 941.76f, 78.09f, -76.68f }, { 941.80f, 78.27f, -76.62f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.88f, 78.50f, -76.38f }, { 941.90f, 78.52f, -76.29f }, { 941.92f, 78.49f, -76.38f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.92f, 78.50f, -76.38f }, { 941.90f, 78.52f, -76.29f }, { 941.94f, 78.52f, -76.30f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.78f, 77.89f, -76.71f }, { 941.75f, 77.67f, -76.72f }, { 941.74f, 77.90f, -76.71f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.75f, 77.67f, -76.72f }, { 941.72f, 77.67f, -76.72f }, { 941.74f, 77.90f, -76.71f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.83f, 78.39f, -76.55f }, { 941.86f, 78.46f, -76.47f }, { 941.86f, 78.39f, -76.55f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.86f, 78.39f, -76.55f }, { 941.86f, 78.46f, -76.47f }, { 941.90f, 78.46f, -76.47f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.72f, 77.67f, -76.72f }, { 941.26f, 77.90f, -76.66f }, { 941.74f, 77.90f, -76.71f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.26f, 77.89f, -76.66f }, { 941.72f, 77.67f, -76.72f }, { 941.26f, 77.67f, -76.67f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.26f, 77.67f, -76.67f }, { 940.80f, 77.67f, -76.63f }, { 941.26f, 77.90f, -76.66f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.78f, 77.89f, -76.61f }, { 941.26f, 77.90f, -76.66f }, { 940.80f, 77.67f, -76.63f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.76f, 78.09f, -76.68f }, { 941.27f, 78.27f, -76.57f }, { 941.80f, 78.27f, -76.62f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.27f, 78.27f, -76.57f }, { 941.76f, 78.09f, -76.68f }, { 941.26f, 78.09f, -76.63f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.26f, 78.09f, -76.63f }, { 940.76f, 78.09f, -76.58f }, { 941.27f, 78.27f, -76.57f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.74f, 78.27f, -76.52f }, { 941.27f, 78.27f, -76.57f }, { 940.76f, 78.09f, -76.58f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.83f, 78.39f, -76.55f }, { 941.28f, 78.46f, -76.41f }, { 941.86f, 78.46f, -76.47f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.28f, 78.46f, -76.41f }, { 941.83f, 78.39f, -76.55f }, { 941.28f, 78.39f, -76.49f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.73f, 78.39f, -76.44f }, { 941.28f, 78.46f, -76.41f }, { 941.28f, 78.39f, -76.49f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.28f, 78.46f, -76.41f }, { 940.73f, 78.39f, -76.44f }, { 940.71f, 78.46f, -76.36f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.88f, 78.50f, -76.38f }, { 941.30f, 78.52f, -76.23f }, { 941.90f, 78.52f, -76.29f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.30f, 78.52f, -76.23f }, { 941.88f, 78.49f, -76.38f }, { 941.29f, 78.49f, -76.32f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.71f, 78.50f, -76.26f }, { 941.30f, 78.52f, -76.23f }, { 941.29f, 78.49f, -76.32f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.30f, 78.52f, -76.23f }, { 940.71f, 78.49f, -76.26f }, { 940.70f, 78.52f, -76.18f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.76f, 77.67f, -76.62f }, { 940.74f, 77.90f, -76.61f }, { 940.78f, 77.90f, -76.61f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.78f, 77.89f, -76.61f }, { 940.80f, 77.67f, -76.63f }, { 940.76f, 77.67f, -76.63f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.72f, 78.09f, -76.58f }, { 940.70f, 78.27f, -76.52f }, { 940.74f, 78.27f, -76.52f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.74f, 78.27f, -76.52f }, { 940.76f, 78.09f, -76.58f }, { 940.72f, 78.09f, -76.58f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.69f, 78.39f, -76.44f }, { 940.71f, 78.46f, -76.36f }, { 940.73f, 78.39f, -76.44f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.71f, 78.46f, -76.36f }, { 940.69f, 78.39f, -76.44f }, { 940.67f, 78.46f, -76.35f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.67f, 78.50f, -76.26f }, { 940.70f, 78.52f, -76.18f }, { 940.71f, 78.49f, -76.26f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.70f, 78.52f, -76.17f }, { 940.67f, 78.49f, -76.26f }, { 940.66f, 78.52f, -76.17f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.90f, 78.43f, -76.47f }, { 941.86f, 78.37f, -76.55f }, { 941.86f, 78.39f, -76.55f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.86f, 78.39f, -76.55f }, { 941.90f, 78.46f, -76.47f }, { 941.90f, 78.43f, -76.47f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.67f, 78.43f, -76.35f }, { 940.69f, 78.39f, -76.44f }, { 940.69f, 78.37f, -76.43f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.69f, 78.39f, -76.44f }, { 940.67f, 78.43f, -76.35f }, { 940.67f, 78.46f, -76.35f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.78f, 77.89f, -76.70f }, { 941.75f, 77.67f, -76.72f }, { 941.78f, 77.90f, -76.71f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.75f, 77.67f, -76.72f }, { 941.78f, 77.89f, -76.70f }, { 941.76f, 77.67f, -76.71f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.74f, 77.89f, -76.60f }, { 940.76f, 77.67f, -76.63f }, { 940.76f, 77.67f, -76.62f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.76f, 77.67f, -76.62f }, { 940.74f, 77.89f, -76.60f }, { 940.74f, 77.90f, -76.61f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.83f, 78.25f, -76.62f }, { 941.80f, 78.08f, -76.67f }, { 941.80f, 78.09f, -76.68f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.80f, 78.09f, -76.68f }, { 941.83f, 78.27f, -76.63f }, { 941.83f, 78.25f, -76.62f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.70f, 78.25f, -76.51f }, { 940.72f, 78.09f, -76.58f }, { 940.72f, 78.08f, -76.57f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.72f, 78.09f, -76.58f }, { 940.70f, 78.25f, -76.51f }, { 940.70f, 78.27f, -76.52f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.94f, 78.49f, -76.30f }, { 941.92f, 78.47f, -76.38f }, { 941.92f, 78.49f, -76.38f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.92f, 78.50f, -76.38f }, { 941.94f, 78.52f, -76.30f }, { 941.94f, 78.49f, -76.30f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.66f, 78.49f, -76.17f }, { 940.67f, 78.49f, -76.26f }, { 940.67f, 78.47f, -76.26f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.67f, 78.50f, -76.26f }, { 940.66f, 78.49f, -76.17f }, { 940.66f, 78.52f, -76.17f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.86f, 78.37f, -76.55f }, { 941.83f, 78.25f, -76.62f }, { 941.83f, 78.27f, -76.63f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.83f, 78.27f, -76.63f }, { 941.86f, 78.39f, -76.55f }, { 941.86f, 78.37f, -76.55f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.86f, 78.39f, -76.55f }, { 941.83f, 78.27f, -76.63f }, { 941.83f, 78.39f, -76.55f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.83f, 78.27f, -76.63f }, { 941.80f, 78.27f, -76.62f }, { 941.83f, 78.39f, -76.55f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.28f, 78.39f, -76.49f }, { 941.83f, 78.39f, -76.55f }, { 941.80f, 78.27f, -76.62f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.28f, 78.39f, -76.49f }, { 941.80f, 78.27f, -76.62f }, { 941.27f, 78.27f, -76.57f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.74f, 78.27f, -76.52f }, { 941.28f, 78.39f, -76.49f }, { 941.27f, 78.27f, -76.57f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.28f, 78.39f, -76.49f }, { 940.74f, 78.27f, -76.52f }, { 940.73f, 78.39f, -76.44f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.73f, 78.39f, -76.44f }, { 940.70f, 78.27f, -76.52f }, { 940.69f, 78.39f, -76.44f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.70f, 78.27f, -76.52f }, { 940.73f, 78.39f, -76.44f }, { 940.74f, 78.27f, -76.52f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.69f, 78.39f, -76.44f }, { 940.70f, 78.27f, -76.52f }, { 940.69f, 78.37f, -76.43f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.70f, 78.25f, -76.51f }, { 940.69f, 78.37f, -76.43f }, { 940.70f, 78.27f, -76.52f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.80f, 78.08f, -76.67f }, { 941.78f, 77.90f, -76.71f }, { 941.80f, 78.09f, -76.68f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.78f, 77.89f, -76.71f }, { 941.80f, 78.08f, -76.67f }, { 941.78f, 77.89f, -76.70f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.80f, 78.09f, -76.68f }, { 941.78f, 77.90f, -76.71f }, { 941.76f, 78.09f, -76.68f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.78f, 77.89f, -76.71f }, { 941.74f, 77.90f, -76.71f }, { 941.76f, 78.09f, -76.68f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.26f, 78.09f, -76.63f }, { 941.76f, 78.09f, -76.68f }, { 941.74f, 77.90f, -76.71f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.26f, 78.09f, -76.63f }, { 941.74f, 77.90f, -76.71f }, { 941.26f, 77.90f, -76.66f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.78f, 77.89f, -76.61f }, { 941.26f, 78.09f, -76.63f }, { 941.26f, 77.90f, -76.66f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.26f, 78.09f, -76.63f }, { 940.78f, 77.90f, -76.61f }, { 940.76f, 78.09f, -76.58f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.76f, 78.09f, -76.58f }, { 940.74f, 77.90f, -76.61f }, { 940.72f, 78.09f, -76.58f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.74f, 77.89f, -76.61f }, { 940.76f, 78.09f, -76.58f }, { 940.78f, 77.90f, -76.61f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.72f, 78.09f, -76.58f }, { 940.74f, 77.90f, -76.61f }, { 940.72f, 78.08f, -76.57f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.72f, 78.08f, -76.57f }, { 940.74f, 77.90f, -76.61f }, { 940.74f, 77.89f, -76.60f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.92f, 78.47f, -76.38f }, { 941.90f, 78.43f, -76.47f }, { 941.90f, 78.46f, -76.47f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.90f, 78.46f, -76.47f }, { 941.92f, 78.49f, -76.38f }, { 941.92f, 78.47f, -76.38f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.86f, 78.46f, -76.47f }, { 941.88f, 78.49f, -76.38f }, { 941.90f, 78.46f, -76.47f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.90f, 78.46f, -76.47f }, { 941.88f, 78.49f, -76.38f }, { 941.92f, 78.49f, -76.38f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.86f, 78.46f, -76.47f }, { 941.29f, 78.49f, -76.32f }, { 941.88f, 78.49f, -76.38f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.86f, 78.46f, -76.47f }, { 941.28f, 78.46f, -76.41f }, { 941.29f, 78.49f, -76.32f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.71f, 78.46f, -76.36f }, { 941.29f, 78.49f, -76.32f }, { 941.28f, 78.46f, -76.41f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.29f, 78.50f, -76.32f }, { 940.71f, 78.46f, -76.36f }, { 940.71f, 78.49f, -76.26f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.67f, 78.46f, -76.35f }, { 940.71f, 78.49f, -76.26f }, { 940.71f, 78.46f, -76.36f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.71f, 78.50f, -76.26f }, { 940.67f, 78.46f, -76.35f }, { 940.67f, 78.49f, -76.26f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.67f, 78.50f, -76.26f }, { 940.67f, 78.46f, -76.35f }, { 940.67f, 78.47f, -76.26f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.67f, 78.43f, -76.35f }, { 940.67f, 78.47f, -76.26f }, { 940.67f, 78.46f, -76.35f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.96f, 78.52f, -75.66f }, { 941.94f, 78.52f, -76.30f }, { 941.90f, 78.52f, -76.29f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.94f, 78.52f, -76.30f }, { 941.96f, 78.52f, -75.66f }, { 942.00f, 78.52f, -75.66f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.90f, 78.52f, -76.29f }, { 941.36f, 78.52f, -75.60f }, { 941.96f, 78.52f, -75.66f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.30f, 78.52f, -76.23f }, { 941.36f, 78.52f, -75.60f }, { 941.90f, 78.52f, -76.29f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.70f, 78.52f, -76.17f }, { 941.36f, 78.52f, -75.60f }, { 941.30f, 78.52f, -76.23f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.36f, 78.52f, -75.60f }, { 940.70f, 78.52f, -76.18f }, { 940.77f, 78.52f, -75.54f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.66f, 78.52f, -76.17f }, { 940.73f, 78.52f, -75.54f }, { 940.70f, 78.52f, -76.18f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.70f, 78.52f, -76.17f }, { 940.73f, 78.52f, -75.54f }, { 940.77f, 78.52f, -75.54f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.94f, 78.52f, -76.30f }, { 942.00f, 78.52f, -75.66f }, { 941.94f, 78.49f, -76.30f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.94f, 78.49f, -76.30f }, { 942.00f, 78.52f, -75.66f }, { 942.00f, 78.49f, -75.66f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.66f, 78.49f, -76.17f }, { 940.73f, 78.52f, -75.54f }, { 940.66f, 78.52f, -76.17f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.73f, 78.52f, -75.54f }, { 940.66f, 78.49f, -76.17f }, { 940.73f, 78.49f, -75.54f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.16f, 78.27f, -81.89f }, { 940.20f, 78.09f, -81.84f }, { 940.20f, 78.27f, -81.89f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.20f, 78.09f, -81.84f }, { 940.23f, 78.09f, -81.84f }, { 940.20f, 78.27f, -81.89f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.12f, 78.50f, -82.14f }, { 940.10f, 78.52f, -82.22f }, { 940.08f, 78.49f, -82.14f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.08f, 78.50f, -82.14f }, { 940.10f, 78.52f, -82.22f }, { 940.06f, 78.52f, -82.22f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.22f, 77.89f, -81.81f }, { 940.24f, 77.67f, -81.80f }, { 940.26f, 77.90f, -81.81f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.24f, 77.67f, -81.80f }, { 940.28f, 77.67f, -81.80f }, { 940.26f, 77.90f, -81.81f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.17f, 78.39f, -81.97f }, { 940.14f, 78.46f, -82.05f }, { 940.13f, 78.39f, -81.97f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.13f, 78.39f, -81.97f }, { 940.14f, 78.46f, -82.05f }, { 940.10f, 78.46f, -82.04f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.28f, 77.67f, -81.80f }, { 940.74f, 77.90f, -81.86f }, { 940.26f, 77.90f, -81.81f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.74f, 77.89f, -81.86f }, { 940.28f, 77.67f, -81.80f }, { 940.74f, 77.67f, -81.84f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.74f, 77.67f, -81.84f }, { 941.20f, 77.67f, -81.89f }, { 940.74f, 77.90f, -81.86f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.22f, 77.89f, -81.91f }, { 940.74f, 77.90f, -81.86f }, { 941.20f, 77.67f, -81.89f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.23f, 78.09f, -81.84f }, { 940.73f, 78.27f, -81.95f }, { 940.20f, 78.27f, -81.89f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.73f, 78.27f, -81.95f }, { 940.23f, 78.09f, -81.84f }, { 940.73f, 78.09f, -81.89f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.73f, 78.09f, -81.89f }, { 941.23f, 78.09f, -81.94f }, { 940.73f, 78.27f, -81.95f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.25f, 78.27f, -82.00f }, { 940.73f, 78.27f, -81.95f }, { 941.23f, 78.09f, -81.94f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.17f, 78.39f, -81.97f }, { 940.71f, 78.46f, -82.10f }, { 940.14f, 78.46f, -82.05f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.71f, 78.46f, -82.11f }, { 940.17f, 78.39f, -81.97f }, { 940.72f, 78.39f, -82.02f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.27f, 78.39f, -82.08f }, { 940.71f, 78.46f, -82.10f }, { 940.72f, 78.39f, -82.02f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.71f, 78.46f, -82.11f }, { 941.27f, 78.39f, -82.08f }, { 941.29f, 78.46f, -82.16f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.12f, 78.50f, -82.14f }, { 940.69f, 78.52f, -82.28f }, { 940.10f, 78.52f, -82.22f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.69f, 78.52f, -82.28f }, { 940.12f, 78.49f, -82.14f }, { 940.70f, 78.49f, -82.20f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.29f, 78.50f, -82.25f }, { 940.69f, 78.52f, -82.28f }, { 940.70f, 78.49f, -82.20f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.69f, 78.52f, -82.28f }, { 941.29f, 78.49f, -82.25f }, { 941.29f, 78.52f, -82.34f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.24f, 77.67f, -81.89f }, { 941.26f, 77.90f, -81.91f }, { 941.22f, 77.90f, -81.90f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.22f, 77.89f, -81.91f }, { 941.20f, 77.67f, -81.89f }, { 941.24f, 77.67f, -81.89f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.27f, 78.09f, -81.94f }, { 941.29f, 78.27f, -82.00f }, { 941.25f, 78.27f, -82.00f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.25f, 78.27f, -82.00f }, { 941.23f, 78.09f, -81.94f }, { 941.27f, 78.09f, -81.94f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.31f, 78.39f, -82.08f }, { 941.29f, 78.46f, -82.16f }, { 941.27f, 78.39f, -82.08f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.29f, 78.46f, -82.16f }, { 941.31f, 78.39f, -82.08f }, { 941.33f, 78.46f, -82.17f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.33f, 78.50f, -82.26f }, { 941.29f, 78.52f, -82.34f }, { 941.29f, 78.49f, -82.25f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.29f, 78.52f, -82.34f }, { 941.33f, 78.49f, -82.26f }, { 941.33f, 78.52f, -82.35f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.10f, 78.43f, -82.05f }, { 940.13f, 78.37f, -81.97f }, { 940.13f, 78.39f, -81.97f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.13f, 78.39f, -81.97f }, { 940.10f, 78.46f, -82.04f }, { 940.10f, 78.43f, -82.05f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.33f, 78.43f, -82.17f }, { 941.31f, 78.39f, -82.08f }, { 941.31f, 78.37f, -82.09f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.31f, 78.39f, -82.08f }, { 941.33f, 78.43f, -82.17f }, { 941.33f, 78.46f, -82.17f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.22f, 77.89f, -81.81f }, { 940.24f, 77.67f, -81.80f }, { 940.22f, 77.90f, -81.81f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.24f, 77.67f, -81.80f }, { 940.22f, 77.89f, -81.82f }, { 940.24f, 77.67f, -81.80f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.25f, 77.89f, -81.92f }, { 941.24f, 77.67f, -81.89f }, { 941.23f, 77.67f, -81.90f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.24f, 77.67f, -81.89f }, { 941.25f, 77.89f, -81.92f }, { 941.26f, 77.90f, -81.91f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.16f, 78.25f, -81.90f }, { 940.19f, 78.08f, -81.84f }, { 940.20f, 78.09f, -81.84f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.20f, 78.09f, -81.84f }, { 940.16f, 78.27f, -81.89f }, { 940.16f, 78.25f, -81.90f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.29f, 78.25f, -82.01f }, { 941.27f, 78.09f, -81.94f }, { 941.27f, 78.08f, -81.95f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.27f, 78.09f, -81.94f }, { 941.29f, 78.25f, -82.01f }, { 941.29f, 78.27f, -82.00f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.06f, 78.49f, -82.22f }, { 940.08f, 78.47f, -82.14f }, { 940.08f, 78.49f, -82.14f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.08f, 78.50f, -82.14f }, { 940.06f, 78.52f, -82.22f }, { 940.06f, 78.49f, -82.22f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.33f, 78.49f, -82.35f }, { 941.33f, 78.49f, -82.26f }, { 941.33f, 78.47f, -82.26f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.33f, 78.50f, -82.26f }, { 941.33f, 78.49f, -82.35f }, { 941.33f, 78.52f, -82.35f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.13f, 78.37f, -81.97f }, { 940.16f, 78.25f, -81.90f }, { 940.16f, 78.27f, -81.89f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.16f, 78.27f, -81.89f }, { 940.13f, 78.39f, -81.97f }, { 940.13f, 78.37f, -81.97f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.13f, 78.39f, -81.97f }, { 940.16f, 78.27f, -81.89f }, { 940.17f, 78.39f, -81.97f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.16f, 78.27f, -81.89f }, { 940.20f, 78.27f, -81.89f }, { 940.17f, 78.39f, -81.97f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.72f, 78.39f, -82.03f }, { 940.17f, 78.39f, -81.97f }, { 940.20f, 78.27f, -81.89f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.72f, 78.39f, -82.03f }, { 940.20f, 78.27f, -81.89f }, { 940.73f, 78.27f, -81.95f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.25f, 78.27f, -82.00f }, { 940.72f, 78.39f, -82.02f }, { 940.73f, 78.27f, -81.95f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.72f, 78.39f, -82.03f }, { 941.25f, 78.27f, -82.00f }, { 941.27f, 78.39f, -82.08f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.27f, 78.39f, -82.08f }, { 941.29f, 78.27f, -82.00f }, { 941.31f, 78.39f, -82.08f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.29f, 78.27f, -82.00f }, { 941.27f, 78.39f, -82.08f }, { 941.25f, 78.27f, -82.00f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.31f, 78.39f, -82.08f }, { 941.29f, 78.27f, -82.00f }, { 941.31f, 78.37f, -82.09f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.29f, 78.25f, -82.01f }, { 941.31f, 78.37f, -82.09f }, { 941.29f, 78.27f, -82.00f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.19f, 78.08f, -81.84f }, { 940.22f, 77.90f, -81.81f }, { 940.20f, 78.09f, -81.84f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.22f, 77.89f, -81.81f }, { 940.19f, 78.08f, -81.84f }, { 940.22f, 77.89f, -81.82f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.20f, 78.09f, -81.84f }, { 940.22f, 77.90f, -81.81f }, { 940.23f, 78.09f, -81.84f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.22f, 77.89f, -81.81f }, { 940.26f, 77.90f, -81.81f }, { 940.23f, 78.09f, -81.84f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.73f, 78.09f, -81.89f }, { 940.23f, 78.09f, -81.84f }, { 940.26f, 77.90f, -81.81f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.73f, 78.09f, -81.89f }, { 940.26f, 77.90f, -81.81f }, { 940.74f, 77.90f, -81.86f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.22f, 77.89f, -81.91f }, { 940.73f, 78.09f, -81.89f }, { 940.74f, 77.90f, -81.86f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.73f, 78.09f, -81.89f }, { 941.22f, 77.90f, -81.90f }, { 941.23f, 78.09f, -81.94f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.24f, 78.09f, -81.94f }, { 941.26f, 77.90f, -81.91f }, { 941.27f, 78.09f, -81.94f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.25f, 77.89f, -81.91f }, { 941.23f, 78.09f, -81.94f }, { 941.22f, 77.90f, -81.90f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.27f, 78.09f, -81.94f }, { 941.26f, 77.90f, -81.91f }, { 941.27f, 78.08f, -81.95f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.27f, 78.08f, -81.95f }, { 941.26f, 77.90f, -81.91f }, { 941.25f, 77.89f, -81.92f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.08f, 78.47f, -82.14f }, { 940.10f, 78.43f, -82.05f }, { 940.10f, 78.46f, -82.04f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.10f, 78.46f, -82.05f }, { 940.08f, 78.49f, -82.14f }, { 940.08f, 78.47f, -82.14f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.14f, 78.46f, -82.05f }, { 940.12f, 78.49f, -82.14f }, { 940.10f, 78.46f, -82.04f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.10f, 78.46f, -82.05f }, { 940.12f, 78.49f, -82.14f }, { 940.08f, 78.49f, -82.14f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.14f, 78.46f, -82.05f }, { 940.70f, 78.49f, -82.20f }, { 940.12f, 78.49f, -82.14f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.14f, 78.46f, -82.05f }, { 940.71f, 78.46f, -82.10f }, { 940.70f, 78.49f, -82.20f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.29f, 78.46f, -82.16f }, { 940.70f, 78.49f, -82.20f }, { 940.71f, 78.46f, -82.10f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.70f, 78.50f, -82.20f }, { 941.29f, 78.46f, -82.16f }, { 941.29f, 78.49f, -82.25f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.33f, 78.46f, -82.17f }, { 941.29f, 78.49f, -82.25f }, { 941.29f, 78.46f, -82.16f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.29f, 78.50f, -82.25f }, { 941.33f, 78.46f, -82.17f }, { 941.33f, 78.49f, -82.26f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.33f, 78.50f, -82.26f }, { 941.33f, 78.46f, -82.17f }, { 941.33f, 78.47f, -82.26f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.33f, 78.43f, -82.17f }, { 941.33f, 78.47f, -82.26f }, { 941.33f, 78.46f, -82.17f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.03f, 78.52f, -82.86f }, { 940.06f, 78.52f, -82.22f }, { 940.10f, 78.52f, -82.22f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.06f, 78.52f, -82.22f }, { 940.03f, 78.52f, -82.86f }, { 940.00f, 78.52f, -82.85f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.10f, 78.52f, -82.22f }, { 940.63f, 78.52f, -82.92f }, { 940.03f, 78.52f, -82.86f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.69f, 78.52f, -82.28f }, { 940.63f, 78.52f, -82.92f }, { 940.10f, 78.52f, -82.22f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.29f, 78.52f, -82.34f }, { 940.63f, 78.52f, -82.92f }, { 940.69f, 78.52f, -82.28f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.63f, 78.52f, -82.92f }, { 941.29f, 78.52f, -82.34f }, { 941.23f, 78.52f, -82.98f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.33f, 78.52f, -82.35f }, { 941.27f, 78.52f, -82.98f }, { 941.29f, 78.52f, -82.34f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.29f, 78.52f, -82.34f }, { 941.27f, 78.52f, -82.98f }, { 941.23f, 78.52f, -82.98f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.06f, 78.52f, -82.22f }, { 940.00f, 78.52f, -82.85f }, { 940.06f, 78.49f, -82.22f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 940.06f, 78.49f, -82.22f }, { 940.00f, 78.52f, -82.85f }, { 940.00f, 78.49f, -82.86f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.33f, 78.49f, -82.35f }, { 941.27f, 78.52f, -82.98f }, { 941.33f, 78.52f, -82.35f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 941.27f, 78.52f, -82.98f }, { 941.33f, 78.49f, -82.35f }, { 941.27f, 78.49f, -82.98f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 952.63f, 78.25f, -77.68f }, { 952.60f, 78.08f, -77.74f }, { 952.59f, 78.26f, -77.68f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 952.60f, 78.08f, -77.74f }, { 952.56f, 78.08f, -77.73f }, { 952.59f, 78.26f, -77.68f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 952.68f, 78.48f, -77.43f }, { 952.70f, 78.51f, -77.35f }, { 952.72f, 78.48f, -77.44f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 952.72f, 78.48f, -77.44f }, { 952.70f, 78.51f, -77.35f }, { 952.74f, 78.51f, -77.35f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 952.58f, 77.89f, -77.77f }, { 952.55f, 77.66f, -77.78f }, { 952.54f, 77.89f, -77.76f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 952.55f, 77.66f, -77.78f }, { 952.52f, 77.66f, -77.77f }, { 952.54f, 77.89f, -77.76f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 952.63f, 78.38f, -77.60f }, { 952.66f, 78.45f, -77.52f }, { 952.66f, 78.38f, -77.60f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 952.66f, 78.38f, -77.61f }, { 952.66f, 78.45f, -77.52f }, { 952.70f, 78.45f, -77.53f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 952.52f, 77.66f, -77.77f }, { 952.06f, 77.89f, -77.71f }, { 952.54f, 77.89f, -77.76f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 952.06f, 77.89f, -77.71f }, { 952.52f, 77.66f, -77.77f }, { 952.06f, 77.66f, -77.73f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 952.06f, 77.66f, -77.73f }, { 951.60f, 77.66f, -77.68f }, { 952.06f, 77.89f, -77.71f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 951.58f, 77.89f, -77.67f }, { 952.06f, 77.89f, -77.71f }, { 951.60f, 77.66f, -77.68f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 952.56f, 78.08f, -77.73f }, { 952.07f, 78.26f, -77.63f }, { 952.59f, 78.26f, -77.68f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 952.07f, 78.25f, -77.63f }, { 952.56f, 78.08f, -77.73f }, { 952.06f, 78.08f, -77.68f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 952.06f, 78.08f, -77.68f }, { 951.56f, 78.08f, -77.63f }, { 952.07f, 78.26f, -77.63f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 951.54f, 78.25f, -77.57f }, { 952.07f, 78.26f, -77.63f }, { 951.56f, 78.08f, -77.63f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 952.63f, 78.38f, -77.60f }, { 952.08f, 78.45f, -77.47f }, { 952.66f, 78.45f, -77.52f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 952.08f, 78.45f, -77.47f }, { 952.63f, 78.38f, -77.60f }, { 952.08f, 78.38f, -77.55f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 951.52f, 78.38f, -77.49f }, { 952.08f, 78.45f, -77.47f }, { 952.08f, 78.38f, -77.55f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 952.08f, 78.45f, -77.47f }, { 951.52f, 78.38f, -77.49f }, { 951.51f, 78.45f, -77.41f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 952.68f, 78.48f, -77.43f }, { 952.10f, 78.51f, -77.29f }, { 952.70f, 78.51f, -77.35f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 952.10f, 78.51f, -77.29f }, { 952.68f, 78.48f, -77.43f }, { 952.09f, 78.48f, -77.37f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 951.50f, 78.48f, -77.32f }, { 952.10f, 78.51f, -77.29f }, { 952.09f, 78.48f, -77.37f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 952.10f, 78.51f, -77.29f }, { 951.51f, 78.48f, -77.32f }, { 951.50f, 78.51f, -77.23f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 951.56f, 77.66f, -77.68f }, { 951.54f, 77.89f, -77.66f }, { 951.58f, 77.89f, -77.67f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 951.58f, 77.89f, -77.67f }, { 951.60f, 77.66f, -77.68f }, { 951.56f, 77.66f, -77.68f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 951.52f, 78.08f, -77.63f }, { 951.50f, 78.26f, -77.57f }, { 951.54f, 78.26f, -77.57f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 951.54f, 78.25f, -77.57f }, { 951.56f, 78.08f, -77.63f }, { 951.52f, 78.08f, -77.63f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 951.49f, 78.38f, -77.49f }, { 951.51f, 78.45f, -77.41f }, { 951.52f, 78.38f, -77.49f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 951.51f, 78.45f, -77.41f }, { 951.49f, 78.38f, -77.49f }, { 951.47f, 78.45f, -77.41f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 951.47f, 78.48f, -77.31f }, { 951.50f, 78.51f, -77.23f }, { 951.51f, 78.48f, -77.32f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 951.50f, 78.51f, -77.23f }, { 951.47f, 78.48f, -77.31f }, { 951.46f, 78.51f, -77.23f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 952.70f, 78.42f, -77.53f }, { 952.66f, 78.36f, -77.60f }, { 952.66f, 78.38f, -77.60f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 952.66f, 78.38f, -77.61f }, { 952.70f, 78.45f, -77.53f }, { 952.70f, 78.42f, -77.52f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 951.47f, 78.42f, -77.40f }, { 951.49f, 78.38f, -77.49f }, { 951.49f, 78.36f, -77.48f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 951.49f, 78.38f, -77.49f }, { 951.47f, 78.42f, -77.40f }, { 951.47f, 78.45f, -77.41f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 952.58f, 77.88f, -77.76f }, { 952.55f, 77.66f, -77.78f }, { 952.58f, 77.89f, -77.77f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 952.55f, 77.66f, -77.78f }, { 952.58f, 77.88f, -77.76f }, { 952.55f, 77.66f, -77.77f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 951.54f, 77.88f, -77.65f }, { 951.56f, 77.66f, -77.68f }, { 951.56f, 77.66f, -77.67f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 951.56f, 77.66f, -77.68f }, { 951.54f, 77.88f, -77.65f }, { 951.54f, 77.89f, -77.66f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 952.63f, 78.24f, -77.67f }, { 952.60f, 78.07f, -77.73f }, { 952.60f, 78.08f, -77.74f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 952.60f, 78.08f, -77.74f }, { 952.63f, 78.26f, -77.68f }, { 952.63f, 78.24f, -77.68f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 951.50f, 78.24f, -77.56f }, { 951.52f, 78.08f, -77.63f }, { 951.52f, 78.07f, -77.62f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 951.52f, 78.08f, -77.63f }, { 951.50f, 78.24f, -77.56f }, { 951.50f, 78.26f, -77.57f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 952.74f, 78.48f, -77.35f }, { 952.72f, 78.46f, -77.43f }, { 952.72f, 78.48f, -77.44f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 952.72f, 78.48f, -77.44f }, { 952.74f, 78.51f, -77.35f }, { 952.74f, 78.48f, -77.35f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 951.46f, 78.48f, -77.22f }, { 951.47f, 78.48f, -77.31f }, { 951.47f, 78.46f, -77.31f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 951.47f, 78.48f, -77.31f }, { 951.46f, 78.48f, -77.22f }, { 951.46f, 78.51f, -77.23f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 952.66f, 78.36f, -77.60f }, { 952.63f, 78.24f, -77.68f }, { 952.63f, 78.26f, -77.68f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 952.63f, 78.25f, -77.68f }, { 952.66f, 78.38f, -77.60f }, { 952.66f, 78.36f, -77.60f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 952.66f, 78.38f, -77.61f }, { 952.63f, 78.26f, -77.68f }, { 952.63f, 78.38f, -77.60f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 952.63f, 78.25f, -77.68f }, { 952.59f, 78.26f, -77.68f }, { 952.63f, 78.38f, -77.60f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 952.08f, 78.38f, -77.55f }, { 952.63f, 78.38f, -77.60f }, { 952.59f, 78.26f, -77.68f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 952.08f, 78.38f, -77.55f }, { 952.59f, 78.26f, -77.68f }, { 952.07f, 78.26f, -77.63f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 951.54f, 78.25f, -77.57f }, { 952.08f, 78.38f, -77.55f }, { 952.07f, 78.26f, -77.63f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 952.08f, 78.38f, -77.55f }, { 951.54f, 78.26f, -77.57f }, { 951.52f, 78.38f, -77.49f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 951.52f, 78.38f, -77.49f }, { 951.50f, 78.26f, -77.57f }, { 951.49f, 78.38f, -77.49f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 951.50f, 78.25f, -77.57f }, { 951.52f, 78.38f, -77.49f }, { 951.54f, 78.26f, -77.57f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 951.49f, 78.38f, -77.49f }, { 951.50f, 78.26f, -77.57f }, { 951.49f, 78.36f, -77.48f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 951.50f, 78.24f, -77.56f }, { 951.49f, 78.36f, -77.48f }, { 951.50f, 78.26f, -77.57f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 952.60f, 78.07f, -77.73f }, { 952.58f, 77.89f, -77.77f }, { 952.60f, 78.08f, -77.74f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 952.58f, 77.89f, -77.77f }, { 952.60f, 78.07f, -77.73f }, { 952.58f, 77.88f, -77.76f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 952.60f, 78.08f, -77.74f }, { 952.58f, 77.89f, -77.77f }, { 952.56f, 78.08f, -77.73f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 952.58f, 77.89f, -77.77f }, { 952.54f, 77.89f, -77.76f }, { 952.56f, 78.08f, -77.73f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 952.06f, 78.08f, -77.68f }, { 952.56f, 78.08f, -77.73f }, { 952.54f, 77.89f, -77.76f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 952.06f, 78.08f, -77.68f }, { 952.54f, 77.89f, -77.76f }, { 952.06f, 77.89f, -77.71f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 951.58f, 77.89f, -77.67f }, { 952.06f, 78.08f, -77.68f }, { 952.06f, 77.89f, -77.71f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 952.06f, 78.08f, -77.68f }, { 951.58f, 77.89f, -77.67f }, { 951.56f, 78.08f, -77.63f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 951.56f, 78.08f, -77.63f }, { 951.54f, 77.89f, -77.66f }, { 951.52f, 78.08f, -77.63f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 951.54f, 77.89f, -77.66f }, { 951.56f, 78.08f, -77.63f }, { 951.58f, 77.89f, -77.67f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 951.52f, 78.08f, -77.63f }, { 951.54f, 77.89f, -77.66f }, { 951.52f, 78.07f, -77.62f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 951.52f, 78.07f, -77.62f }, { 951.54f, 77.89f, -77.66f }, { 951.54f, 77.88f, -77.65f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 952.72f, 78.46f, -77.43f }, { 952.70f, 78.42f, -77.52f }, { 952.70f, 78.45f, -77.53f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 952.70f, 78.45f, -77.53f }, { 952.72f, 78.48f, -77.44f }, { 952.72f, 78.46f, -77.43f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 952.66f, 78.45f, -77.52f }, { 952.68f, 78.48f, -77.43f }, { 952.70f, 78.45f, -77.53f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 952.70f, 78.45f, -77.53f }, { 952.68f, 78.48f, -77.43f }, { 952.72f, 78.48f, -77.44f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 952.66f, 78.45f, -77.52f }, { 952.09f, 78.48f, -77.37f }, { 952.68f, 78.48f, -77.43f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 952.66f, 78.45f, -77.52f }, { 952.08f, 78.45f, -77.47f }, { 952.09f, 78.48f, -77.37f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 951.51f, 78.45f, -77.41f }, { 952.09f, 78.48f, -77.37f }, { 952.08f, 78.45f, -77.47f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 952.09f, 78.48f, -77.38f }, { 951.51f, 78.45f, -77.41f }, { 951.51f, 78.48f, -77.32f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 951.47f, 78.45f, -77.41f }, { 951.51f, 78.48f, -77.32f }, { 951.51f, 78.45f, -77.41f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 951.50f, 78.48f, -77.32f }, { 951.47f, 78.45f, -77.41f }, { 951.47f, 78.48f, -77.31f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 951.47f, 78.48f, -77.31f }, { 951.47f, 78.45f, -77.41f }, { 951.47f, 78.46f, -77.31f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 951.47f, 78.42f, -77.40f }, { 951.47f, 78.46f, -77.31f }, { 951.47f, 78.45f, -77.41f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 952.76f, 78.51f, -76.72f }, { 952.74f, 78.51f, -77.35f }, { 952.70f, 78.51f, -77.35f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 952.74f, 78.51f, -77.35f }, { 952.76f, 78.51f, -76.71f }, { 952.80f, 78.51f, -76.72f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 952.70f, 78.51f, -77.35f }, { 952.16f, 78.51f, -76.66f }, { 952.76f, 78.51f, -76.71f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 952.10f, 78.51f, -77.29f }, { 952.16f, 78.51f, -76.66f }, { 952.70f, 78.51f, -77.35f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 951.50f, 78.51f, -77.23f }, { 952.16f, 78.51f, -76.66f }, { 952.10f, 78.51f, -77.29f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 952.16f, 78.51f, -76.66f }, { 951.50f, 78.51f, -77.23f }, { 951.56f, 78.51f, -76.60f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 951.46f, 78.51f, -77.22f }, { 951.53f, 78.51f, -76.59f }, { 951.50f, 78.51f, -77.23f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 951.50f, 78.51f, -77.23f }, { 951.53f, 78.51f, -76.59f }, { 951.56f, 78.51f, -76.60f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 952.74f, 78.51f, -77.35f }, { 952.80f, 78.51f, -76.72f }, { 952.74f, 78.48f, -77.35f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 952.74f, 78.48f, -77.35f }, { 952.80f, 78.51f, -76.72f }, { 952.80f, 78.48f, -76.72f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 951.46f, 78.48f, -77.22f }, { 951.53f, 78.51f, -76.59f }, { 951.46f, 78.51f, -77.23f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 951.52f, 78.51f, -76.59f }, { 951.46f, 78.48f, -77.22f }, { 951.53f, 78.48f, -76.59f }, 77.66f, 0.00f },   // waterfeature_02 props 61
    { { 950.98f, 78.27f, -82.95f }, { 951.01f, 78.09f, -82.90f }, { 951.02f, 78.27f, -82.96f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 951.01f, 78.09f, -82.90f }, { 951.05f, 78.09f, -82.90f }, { 951.02f, 78.27f, -82.96f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 950.93f, 78.50f, -83.20f }, { 950.91f, 78.52f, -83.29f }, { 950.90f, 78.49f, -83.20f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 950.90f, 78.50f, -83.20f }, { 950.91f, 78.52f, -83.29f }, { 950.88f, 78.52f, -83.29f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 951.04f, 77.89f, -82.87f }, { 951.06f, 77.67f, -82.86f }, { 951.07f, 77.90f, -82.88f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 951.06f, 77.67f, -82.86f }, { 951.10f, 77.67f, -82.86f }, { 951.07f, 77.90f, -82.88f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 950.99f, 78.39f, -83.04f }, { 950.95f, 78.46f, -83.11f }, { 950.95f, 78.39f, -83.03f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 950.95f, 78.39f, -83.03f }, { 950.95f, 78.46f, -83.11f }, { 950.92f, 78.46f, -83.11f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 951.10f, 77.67f, -82.86f }, { 951.55f, 77.90f, -82.92f }, { 951.07f, 77.90f, -82.88f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 951.55f, 77.89f, -82.92f }, { 951.10f, 77.67f, -82.86f }, { 951.56f, 77.67f, -82.91f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 951.55f, 77.67f, -82.91f }, { 952.02f, 77.67f, -82.95f }, { 951.55f, 77.90f, -82.92f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 952.03f, 77.89f, -82.97f }, { 951.55f, 77.90f, -82.92f }, { 952.02f, 77.67f, -82.95f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 951.05f, 78.09f, -82.91f }, { 951.55f, 78.27f, -83.01f }, { 951.02f, 78.27f, -82.96f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 951.54f, 78.27f, -83.01f }, { 951.05f, 78.09f, -82.90f }, { 951.55f, 78.09f, -82.95f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 951.55f, 78.09f, -82.95f }, { 952.05f, 78.09f, -83.00f }, { 951.55f, 78.27f, -83.01f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 952.07f, 78.27f, -83.06f }, { 951.55f, 78.27f, -83.01f }, { 952.05f, 78.09f, -83.00f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 950.99f, 78.39f, -83.04f }, { 951.53f, 78.46f, -83.17f }, { 950.95f, 78.46f, -83.11f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 951.53f, 78.46f, -83.17f }, { 950.99f, 78.39f, -83.04f }, { 951.54f, 78.39f, -83.09f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 952.09f, 78.39f, -83.14f }, { 951.53f, 78.46f, -83.17f }, { 951.54f, 78.39f, -83.09f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 951.53f, 78.46f, -83.17f }, { 952.09f, 78.39f, -83.14f }, { 952.10f, 78.46f, -83.23f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 950.93f, 78.50f, -83.20f }, { 951.51f, 78.52f, -83.35f }, { 950.91f, 78.52f, -83.29f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 951.51f, 78.52f, -83.35f }, { 950.93f, 78.49f, -83.20f }, { 951.52f, 78.49f, -83.26f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 952.11f, 78.50f, -83.32f }, { 951.51f, 78.52f, -83.35f }, { 951.52f, 78.49f, -83.26f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 951.51f, 78.52f, -83.35f }, { 952.11f, 78.49f, -83.32f }, { 952.11f, 78.52f, -83.41f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 952.05f, 77.67f, -82.96f }, { 952.07f, 77.90f, -82.97f }, { 952.03f, 77.90f, -82.97f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 952.03f, 77.89f, -82.97f }, { 952.02f, 77.67f, -82.95f }, { 952.05f, 77.67f, -82.96f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 952.09f, 78.09f, -83.01f }, { 952.11f, 78.27f, -83.07f }, { 952.07f, 78.27f, -83.06f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 952.07f, 78.27f, -83.06f }, { 952.05f, 78.09f, -83.00f }, { 952.09f, 78.09f, -83.01f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 952.13f, 78.39f, -83.15f }, { 952.10f, 78.46f, -83.23f }, { 952.09f, 78.39f, -83.14f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 952.10f, 78.46f, -83.23f }, { 952.13f, 78.39f, -83.15f }, { 952.14f, 78.46f, -83.23f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 952.15f, 78.50f, -83.32f }, { 952.11f, 78.52f, -83.41f }, { 952.11f, 78.49f, -83.32f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 952.11f, 78.52f, -83.41f }, { 952.15f, 78.49f, -83.32f }, { 952.15f, 78.52f, -83.41f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 950.92f, 78.43f, -83.11f }, { 950.95f, 78.37f, -83.04f }, { 950.95f, 78.39f, -83.03f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 950.95f, 78.39f, -83.03f }, { 950.92f, 78.46f, -83.11f }, { 950.92f, 78.43f, -83.11f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 952.14f, 78.43f, -83.23f }, { 952.13f, 78.39f, -83.15f }, { 952.13f, 78.37f, -83.15f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 952.13f, 78.39f, -83.15f }, { 952.14f, 78.43f, -83.23f }, { 952.14f, 78.46f, -83.23f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 951.04f, 77.89f, -82.88f }, { 951.06f, 77.67f, -82.86f }, { 951.04f, 77.90f, -82.87f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 951.06f, 77.67f, -82.86f }, { 951.04f, 77.89f, -82.88f }, { 951.06f, 77.67f, -82.87f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 952.07f, 77.89f, -82.98f }, { 952.05f, 77.67f, -82.96f }, { 952.05f, 77.67f, -82.97f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 952.05f, 77.67f, -82.96f }, { 952.07f, 77.89f, -82.98f }, { 952.07f, 77.90f, -82.97f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 950.98f, 78.25f, -82.96f }, { 951.01f, 78.08f, -82.91f }, { 951.01f, 78.09f, -82.90f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 951.01f, 78.09f, -82.90f }, { 950.98f, 78.27f, -82.96f }, { 950.98f, 78.25f, -82.96f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 952.11f, 78.25f, -83.07f }, { 952.09f, 78.09f, -83.01f }, { 952.09f, 78.08f, -83.02f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 952.09f, 78.09f, -83.01f }, { 952.11f, 78.25f, -83.07f }, { 952.11f, 78.27f, -83.07f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 950.88f, 78.49f, -83.29f }, { 950.90f, 78.47f, -83.20f }, { 950.90f, 78.49f, -83.20f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 950.90f, 78.50f, -83.20f }, { 950.88f, 78.52f, -83.29f }, { 950.88f, 78.49f, -83.29f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 952.15f, 78.49f, -83.41f }, { 952.15f, 78.49f, -83.32f }, { 952.15f, 78.47f, -83.33f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 952.15f, 78.50f, -83.32f }, { 952.15f, 78.49f, -83.41f }, { 952.15f, 78.52f, -83.41f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 950.95f, 78.37f, -83.04f }, { 950.98f, 78.25f, -82.96f }, { 950.98f, 78.27f, -82.96f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 950.98f, 78.27f, -82.95f }, { 950.95f, 78.39f, -83.03f }, { 950.95f, 78.37f, -83.04f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 950.95f, 78.39f, -83.03f }, { 950.98f, 78.27f, -82.96f }, { 950.99f, 78.39f, -83.04f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 950.98f, 78.27f, -82.95f }, { 951.02f, 78.27f, -82.96f }, { 950.99f, 78.39f, -83.04f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 951.54f, 78.39f, -83.09f }, { 950.99f, 78.39f, -83.04f }, { 951.02f, 78.27f, -82.96f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 951.54f, 78.39f, -83.09f }, { 951.02f, 78.27f, -82.96f }, { 951.55f, 78.27f, -83.01f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 952.07f, 78.27f, -83.06f }, { 951.54f, 78.39f, -83.09f }, { 951.55f, 78.27f, -83.01f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 951.54f, 78.39f, -83.09f }, { 952.07f, 78.27f, -83.06f }, { 952.09f, 78.39f, -83.14f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 952.09f, 78.39f, -83.14f }, { 952.11f, 78.27f, -83.07f }, { 952.13f, 78.39f, -83.15f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 952.11f, 78.27f, -83.07f }, { 952.09f, 78.39f, -83.14f }, { 952.07f, 78.27f, -83.06f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 952.13f, 78.39f, -83.15f }, { 952.11f, 78.27f, -83.07f }, { 952.13f, 78.37f, -83.15f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 952.11f, 78.25f, -83.07f }, { 952.13f, 78.37f, -83.15f }, { 952.11f, 78.27f, -83.07f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 951.01f, 78.08f, -82.91f }, { 951.04f, 77.90f, -82.87f }, { 951.01f, 78.09f, -82.90f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 951.04f, 77.89f, -82.87f }, { 951.01f, 78.08f, -82.91f }, { 951.04f, 77.89f, -82.88f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 951.01f, 78.09f, -82.90f }, { 951.04f, 77.90f, -82.87f }, { 951.05f, 78.09f, -82.90f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 951.04f, 77.89f, -82.87f }, { 951.07f, 77.90f, -82.88f }, { 951.05f, 78.09f, -82.90f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 951.55f, 78.09f, -82.95f }, { 951.05f, 78.09f, -82.90f }, { 951.07f, 77.90f, -82.88f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 951.55f, 78.09f, -82.95f }, { 951.07f, 77.90f, -82.88f }, { 951.55f, 77.90f, -82.92f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 952.03f, 77.89f, -82.97f }, { 951.55f, 78.09f, -82.95f }, { 951.55f, 77.90f, -82.92f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 951.55f, 78.09f, -82.95f }, { 952.03f, 77.90f, -82.97f }, { 952.05f, 78.09f, -83.00f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 952.05f, 78.09f, -83.00f }, { 952.07f, 77.90f, -82.97f }, { 952.09f, 78.09f, -83.01f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 952.07f, 77.89f, -82.97f }, { 952.05f, 78.09f, -83.00f }, { 952.03f, 77.90f, -82.97f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 952.09f, 78.09f, -83.01f }, { 952.07f, 77.90f, -82.97f }, { 952.09f, 78.08f, -83.02f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 952.09f, 78.08f, -83.02f }, { 952.07f, 77.90f, -82.97f }, { 952.07f, 77.89f, -82.98f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 950.90f, 78.47f, -83.20f }, { 950.92f, 78.43f, -83.11f }, { 950.92f, 78.46f, -83.11f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 950.92f, 78.46f, -83.11f }, { 950.90f, 78.49f, -83.20f }, { 950.90f, 78.47f, -83.20f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 950.96f, 78.46f, -83.11f }, { 950.93f, 78.49f, -83.20f }, { 950.92f, 78.46f, -83.11f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 950.92f, 78.46f, -83.11f }, { 950.93f, 78.49f, -83.20f }, { 950.90f, 78.49f, -83.20f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 950.96f, 78.46f, -83.11f }, { 951.52f, 78.49f, -83.26f }, { 950.93f, 78.49f, -83.20f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 950.96f, 78.46f, -83.11f }, { 951.53f, 78.46f, -83.17f }, { 951.52f, 78.49f, -83.26f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 952.10f, 78.46f, -83.23f }, { 951.52f, 78.49f, -83.26f }, { 951.53f, 78.46f, -83.17f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 951.52f, 78.50f, -83.26f }, { 952.10f, 78.46f, -83.23f }, { 952.11f, 78.49f, -83.32f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 952.14f, 78.46f, -83.23f }, { 952.11f, 78.49f, -83.32f }, { 952.10f, 78.46f, -83.23f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 952.11f, 78.50f, -83.32f }, { 952.14f, 78.46f, -83.23f }, { 952.15f, 78.49f, -83.32f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 952.15f, 78.50f, -83.32f }, { 952.14f, 78.46f, -83.23f }, { 952.15f, 78.47f, -83.33f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 952.14f, 78.43f, -83.23f }, { 952.15f, 78.47f, -83.33f }, { 952.14f, 78.46f, -83.23f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 950.85f, 78.52f, -83.92f }, { 950.88f, 78.52f, -83.29f }, { 950.91f, 78.52f, -83.29f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 950.88f, 78.52f, -83.29f }, { 950.85f, 78.52f, -83.92f }, { 950.81f, 78.52f, -83.92f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 950.91f, 78.52f, -83.29f }, { 951.45f, 78.52f, -83.98f }, { 950.85f, 78.52f, -83.92f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 951.51f, 78.52f, -83.35f }, { 951.45f, 78.52f, -83.98f }, { 950.91f, 78.52f, -83.29f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 952.11f, 78.52f, -83.41f }, { 951.45f, 78.52f, -83.98f }, { 951.51f, 78.52f, -83.35f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 951.45f, 78.52f, -83.98f }, { 952.11f, 78.52f, -83.41f }, { 952.05f, 78.52f, -84.04f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 952.15f, 78.52f, -83.41f }, { 952.09f, 78.52f, -84.04f }, { 952.11f, 78.52f, -83.41f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 952.11f, 78.52f, -83.41f }, { 952.09f, 78.52f, -84.04f }, { 952.05f, 78.52f, -84.04f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 950.88f, 78.52f, -83.29f }, { 950.81f, 78.52f, -83.92f }, { 950.88f, 78.49f, -83.29f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 950.88f, 78.49f, -83.29f }, { 950.81f, 78.52f, -83.92f }, { 950.81f, 78.49f, -83.92f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 952.15f, 78.49f, -83.41f }, { 952.09f, 78.52f, -84.04f }, { 952.15f, 78.52f, -83.41f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { 952.09f, 78.52f, -84.04f }, { 952.15f, 78.49f, -83.41f }, { 952.09f, 78.49f, -84.05f }, 77.67f, 0.00f },   // waterfeature_02 props 61
    { { -282.80f, 0.56f, 270.40f }, { -282.85f, 0.56f, 269.88f }, { -285.44f, 0.56f, 270.40f }, 0.56f, 0.00f },   // ground_bdstatue_05 props 619
    { { -282.85f, 0.56f, 270.92f }, { -282.80f, 0.56f, 270.40f }, { -285.44f, 0.56f, 270.40f }, 0.56f, 0.00f },   // ground_bdstatue_05 props 619
    { { -282.85f, 0.56f, 269.88f }, { -283.00f, 0.56f, 269.39f }, { -285.44f, 0.56f, 270.40f }, 0.56f, 0.00f },   // ground_bdstatue_05 props 619
    { { -283.00f, 0.56f, 271.41f }, { -282.85f, 0.56f, 270.92f }, { -285.44f, 0.56f, 270.40f }, 0.56f, 0.00f },   // ground_bdstatue_05 props 619
    { { -283.00f, 0.56f, 269.39f }, { -283.24f, 0.56f, 268.93f }, { -285.44f, 0.56f, 270.40f }, 0.56f, 0.00f },   // ground_bdstatue_05 props 619
    { { -283.25f, 0.56f, 271.87f }, { -283.00f, 0.56f, 271.41f }, { -285.44f, 0.56f, 270.40f }, 0.56f, 0.00f },   // ground_bdstatue_05 props 619
    { { -283.25f, 0.56f, 268.93f }, { -283.57f, 0.56f, 268.53f }, { -285.44f, 0.56f, 270.40f }, 0.56f, 0.00f },   // ground_bdstatue_05 props 619
    { { -283.57f, 0.56f, 272.27f }, { -283.24f, 0.56f, 271.87f }, { -285.44f, 0.56f, 270.40f }, 0.56f, 0.00f },   // ground_bdstatue_05 props 619
    { { -283.57f, 0.56f, 268.53f }, { -283.97f, 0.56f, 268.20f }, { -285.44f, 0.56f, 270.40f }, 0.56f, 0.00f },   // ground_bdstatue_05 props 619
    { { -283.97f, 0.56f, 272.60f }, { -283.57f, 0.56f, 272.27f }, { -285.44f, 0.56f, 270.40f }, 0.56f, 0.00f },   // ground_bdstatue_05 props 619
    { { -283.97f, 0.56f, 268.20f }, { -284.43f, 0.56f, 267.96f }, { -285.44f, 0.56f, 270.40f }, 0.56f, 0.00f },   // ground_bdstatue_05 props 619
    { { -284.43f, 0.56f, 272.84f }, { -283.97f, 0.56f, 272.60f }, { -285.44f, 0.56f, 270.40f }, 0.56f, 0.00f },   // ground_bdstatue_05 props 619
    { { -284.43f, 0.56f, 267.96f }, { -284.92f, 0.56f, 267.81f }, { -285.44f, 0.56f, 270.40f }, 0.56f, 0.00f },   // ground_bdstatue_05 props 619
    { { -284.93f, 0.56f, 272.99f }, { -284.43f, 0.56f, 272.84f }, { -285.44f, 0.56f, 270.40f }, 0.56f, 0.00f },   // ground_bdstatue_05 props 619
    { { -284.93f, 0.56f, 267.81f }, { -285.44f, 0.56f, 267.76f }, { -285.44f, 0.56f, 270.40f }, 0.56f, 0.00f },   // ground_bdstatue_05 props 619
    { { -285.44f, 0.56f, 273.04f }, { -284.92f, 0.56f, 272.99f }, { -285.44f, 0.56f, 270.40f }, 0.56f, 0.00f },   // ground_bdstatue_05 props 619
    { { -285.44f, 0.56f, 267.76f }, { -285.96f, 0.56f, 267.81f }, { -285.44f, 0.56f, 270.40f }, 0.56f, 0.00f },   // ground_bdstatue_05 props 619
    { { -285.95f, 0.56f, 272.99f }, { -285.44f, 0.56f, 273.04f }, { -285.44f, 0.56f, 270.40f }, 0.56f, 0.00f },   // ground_bdstatue_05 props 619
    { { -285.95f, 0.56f, 267.81f }, { -286.45f, 0.56f, 267.96f }, { -285.44f, 0.56f, 270.40f }, 0.56f, 0.00f },   // ground_bdstatue_05 props 619
    { { -286.45f, 0.56f, 272.84f }, { -285.96f, 0.56f, 272.99f }, { -285.44f, 0.56f, 270.40f }, 0.56f, 0.00f },   // ground_bdstatue_05 props 619
    { { -286.45f, 0.56f, 267.96f }, { -286.91f, 0.56f, 268.20f }, { -285.44f, 0.56f, 270.40f }, 0.56f, 0.00f },   // ground_bdstatue_05 props 619
    { { -286.91f, 0.56f, 272.60f }, { -286.45f, 0.56f, 272.84f }, { -285.44f, 0.56f, 270.40f }, 0.56f, 0.00f },   // ground_bdstatue_05 props 619
    { { -286.91f, 0.56f, 268.20f }, { -287.31f, 0.56f, 268.53f }, { -285.44f, 0.56f, 270.40f }, 0.56f, 0.00f },   // ground_bdstatue_05 props 619
    { { -287.31f, 0.56f, 272.27f }, { -286.91f, 0.56f, 272.60f }, { -285.44f, 0.56f, 270.40f }, 0.56f, 0.00f },   // ground_bdstatue_05 props 619
    { { -287.31f, 0.56f, 268.53f }, { -287.64f, 0.56f, 268.93f }, { -285.44f, 0.56f, 270.40f }, 0.56f, 0.00f },   // ground_bdstatue_05 props 619
    { { -287.63f, 0.56f, 271.87f }, { -287.31f, 0.56f, 272.27f }, { -285.44f, 0.56f, 270.40f }, 0.56f, 0.00f },   // ground_bdstatue_05 props 619
    { { -287.63f, 0.56f, 268.93f }, { -287.88f, 0.56f, 269.39f }, { -285.44f, 0.56f, 270.40f }, 0.56f, 0.00f },   // ground_bdstatue_05 props 619
    { { -287.88f, 0.56f, 271.41f }, { -287.64f, 0.56f, 271.87f }, { -285.44f, 0.56f, 270.40f }, 0.56f, 0.00f },   // ground_bdstatue_05 props 619
    { { -287.88f, 0.56f, 269.39f }, { -288.03f, 0.56f, 269.88f }, { -285.44f, 0.56f, 270.40f }, 0.56f, 0.00f },   // ground_bdstatue_05 props 619
    { { -288.03f, 0.56f, 270.92f }, { -287.88f, 0.56f, 271.41f }, { -285.44f, 0.56f, 270.40f }, 0.56f, 0.00f },   // ground_bdstatue_05 props 619
    { { -288.03f, 0.56f, 269.88f }, { -288.08f, 0.56f, 270.40f }, { -285.44f, 0.56f, 270.40f }, 0.56f, 0.00f },   // ground_bdstatue_05 props 619
    { { -288.08f, 0.56f, 270.40f }, { -288.03f, 0.56f, 270.92f }, { -285.44f, 0.56f, 270.40f }, 0.56f, 0.00f },   // ground_bdstatue_05 props 619
    { { 1224.59f, 69.35f, -32.91f }, { 1224.53f, 69.17f, -32.91f }, { 1224.57f, 69.35f, -32.88f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.53f, 69.17f, -32.91f }, { 1224.51f, 69.17f, -32.88f }, { 1224.57f, 69.35f, -32.88f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.83f, 69.58f, -32.84f }, { 1224.92f, 69.60f, -32.82f }, { 1224.85f, 69.58f, -32.87f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.85f, 69.58f, -32.87f }, { 1224.92f, 69.60f, -32.82f }, { 1224.93f, 69.60f, -32.85f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.49f, 68.98f, -32.91f }, { 1224.47f, 68.76f, -32.89f }, { 1224.47f, 68.98f, -32.87f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.47f, 68.76f, -32.89f }, { 1224.45f, 68.76f, -32.86f }, { 1224.47f, 68.98f, -32.87f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.66f, 69.48f, -32.87f }, { 1224.74f, 69.54f, -32.86f }, { 1224.67f, 69.48f, -32.91f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.67f, 69.48f, -32.91f }, { 1224.74f, 69.54f, -32.86f }, { 1224.75f, 69.54f, -32.90f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.45f, 68.76f, -32.86f }, { 1224.29f, 68.98f, -32.42f }, { 1224.47f, 68.98f, -32.87f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.29f, 68.98f, -32.42f }, { 1224.45f, 68.76f, -32.86f }, { 1224.28f, 68.76f, -32.43f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.28f, 68.76f, -32.43f }, { 1224.10f, 68.76f, -32.00f }, { 1224.29f, 68.98f, -32.42f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.11f, 68.98f, -31.98f }, { 1224.29f, 68.98f, -32.42f }, { 1224.10f, 68.76f, -32.00f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.51f, 69.17f, -32.88f }, { 1224.37f, 69.35f, -32.39f }, { 1224.57f, 69.35f, -32.88f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.37f, 69.35f, -32.39f }, { 1224.51f, 69.17f, -32.88f }, { 1224.32f, 69.17f, -32.41f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.32f, 69.17f, -32.41f }, { 1224.13f, 69.17f, -31.95f }, { 1224.37f, 69.35f, -32.39f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.17f, 69.35f, -31.90f }, { 1224.37f, 69.35f, -32.39f }, { 1224.13f, 69.17f, -31.95f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.66f, 69.48f, -32.87f }, { 1224.52f, 69.54f, -32.33f }, { 1224.74f, 69.54f, -32.86f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.52f, 69.54f, -32.33f }, { 1224.66f, 69.48f, -32.87f }, { 1224.44f, 69.48f, -32.36f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.23f, 69.48f, -31.85f }, { 1224.52f, 69.54f, -32.33f }, { 1224.44f, 69.48f, -32.36f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.52f, 69.54f, -32.33f }, { 1224.23f, 69.48f, -31.85f }, { 1224.30f, 69.54f, -31.80f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.83f, 69.58f, -32.84f }, { 1224.69f, 69.60f, -32.26f }, { 1224.92f, 69.60f, -32.82f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.68f, 69.60f, -32.26f }, { 1224.83f, 69.58f, -32.84f }, { 1224.61f, 69.58f, -32.29f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.38f, 69.58f, -31.75f }, { 1224.69f, 69.60f, -32.26f }, { 1224.61f, 69.58f, -32.29f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.68f, 69.60f, -32.26f }, { 1224.38f, 69.58f, -31.75f }, { 1224.46f, 69.60f, -31.70f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.09f, 68.76f, -31.97f }, { 1224.09f, 68.98f, -31.94f }, { 1224.10f, 68.98f, -31.98f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.11f, 68.98f, -31.98f }, { 1224.10f, 68.76f, -32.00f }, { 1224.09f, 68.76f, -31.97f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.11f, 69.17f, -31.91f }, { 1224.15f, 69.35f, -31.87f }, { 1224.17f, 69.35f, -31.90f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.17f, 69.35f, -31.90f }, { 1224.13f, 69.17f, -31.95f }, { 1224.11f, 69.17f, -31.91f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.22f, 69.48f, -31.81f }, { 1224.30f, 69.54f, -31.80f }, { 1224.23f, 69.48f, -31.85f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.30f, 69.54f, -31.80f }, { 1224.22f, 69.48f, -31.81f }, { 1224.28f, 69.54f, -31.76f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.36f, 69.58f, -31.71f }, { 1224.46f, 69.60f, -31.70f }, { 1224.38f, 69.58f, -31.75f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.45f, 69.60f, -31.70f }, { 1224.36f, 69.58f, -31.71f }, { 1224.44f, 69.60f, -31.67f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.76f, 69.52f, -32.90f }, { 1224.67f, 69.46f, -32.90f }, { 1224.67f, 69.48f, -32.91f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.67f, 69.48f, -32.91f }, { 1224.75f, 69.54f, -32.90f }, { 1224.76f, 69.52f, -32.90f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.29f, 69.52f, -31.76f }, { 1224.22f, 69.48f, -31.81f }, { 1224.22f, 69.46f, -31.81f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.22f, 69.48f, -31.81f }, { 1224.29f, 69.52f, -31.76f }, { 1224.28f, 69.54f, -31.76f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.50f, 68.97f, -32.90f }, { 1224.47f, 68.76f, -32.89f }, { 1224.49f, 68.98f, -32.90f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.47f, 68.76f, -32.89f }, { 1224.50f, 68.97f, -32.90f }, { 1224.48f, 68.75f, -32.89f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.10f, 68.97f, -31.94f }, { 1224.09f, 68.76f, -31.97f }, { 1224.09f, 68.75f, -31.96f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.09f, 68.76f, -31.97f }, { 1224.10f, 68.97f, -31.94f }, { 1224.09f, 68.98f, -31.94f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.59f, 69.33f, -32.91f }, { 1224.53f, 69.16f, -32.91f }, { 1224.53f, 69.17f, -32.91f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.53f, 69.17f, -32.91f }, { 1224.59f, 69.35f, -32.91f }, { 1224.59f, 69.33f, -32.91f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.16f, 69.33f, -31.86f }, { 1224.11f, 69.17f, -31.91f }, { 1224.12f, 69.16f, -31.91f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.11f, 69.17f, -31.91f }, { 1224.16f, 69.33f, -31.86f }, { 1224.15f, 69.35f, -31.87f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.93f, 69.58f, -32.85f }, { 1224.85f, 69.55f, -32.87f }, { 1224.85f, 69.58f, -32.87f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.85f, 69.58f, -32.87f }, { 1224.93f, 69.60f, -32.85f }, { 1224.93f, 69.58f, -32.85f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.44f, 69.58f, -31.67f }, { 1224.36f, 69.58f, -31.71f }, { 1224.37f, 69.55f, -31.71f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.36f, 69.58f, -31.71f }, { 1224.44f, 69.58f, -31.67f }, { 1224.44f, 69.60f, -31.67f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.67f, 69.45f, -32.90f }, { 1224.59f, 69.33f, -32.91f }, { 1224.59f, 69.35f, -32.91f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.59f, 69.35f, -32.91f }, { 1224.67f, 69.48f, -32.91f }, { 1224.67f, 69.46f, -32.90f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.67f, 69.48f, -32.91f }, { 1224.59f, 69.35f, -32.91f }, { 1224.66f, 69.48f, -32.87f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.59f, 69.35f, -32.91f }, { 1224.57f, 69.35f, -32.88f }, { 1224.66f, 69.48f, -32.87f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.44f, 69.48f, -32.36f }, { 1224.66f, 69.48f, -32.87f }, { 1224.57f, 69.35f, -32.88f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.44f, 69.48f, -32.36f }, { 1224.57f, 69.35f, -32.88f }, { 1224.37f, 69.35f, -32.39f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.17f, 69.35f, -31.90f }, { 1224.44f, 69.48f, -32.36f }, { 1224.37f, 69.35f, -32.39f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.44f, 69.48f, -32.36f }, { 1224.17f, 69.35f, -31.90f }, { 1224.23f, 69.48f, -31.85f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.23f, 69.48f, -31.85f }, { 1224.15f, 69.35f, -31.87f }, { 1224.22f, 69.48f, -31.81f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.15f, 69.35f, -31.87f }, { 1224.23f, 69.48f, -31.85f }, { 1224.17f, 69.35f, -31.90f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.22f, 69.48f, -31.81f }, { 1224.15f, 69.35f, -31.87f }, { 1224.22f, 69.46f, -31.81f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.16f, 69.33f, -31.86f }, { 1224.22f, 69.46f, -31.81f }, { 1224.15f, 69.35f, -31.87f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.53f, 69.16f, -32.91f }, { 1224.49f, 68.98f, -32.90f }, { 1224.53f, 69.17f, -32.91f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.49f, 68.98f, -32.91f }, { 1224.53f, 69.16f, -32.91f }, { 1224.50f, 68.97f, -32.90f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.53f, 69.17f, -32.91f }, { 1224.49f, 68.98f, -32.90f }, { 1224.51f, 69.17f, -32.88f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.49f, 68.98f, -32.91f }, { 1224.47f, 68.98f, -32.87f }, { 1224.51f, 69.17f, -32.88f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.32f, 69.17f, -32.41f }, { 1224.51f, 69.17f, -32.88f }, { 1224.47f, 68.98f, -32.87f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.32f, 69.17f, -32.41f }, { 1224.47f, 68.98f, -32.87f }, { 1224.29f, 68.98f, -32.42f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.11f, 68.98f, -31.98f }, { 1224.32f, 69.17f, -32.41f }, { 1224.29f, 68.98f, -32.42f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.32f, 69.17f, -32.41f }, { 1224.10f, 68.98f, -31.98f }, { 1224.13f, 69.17f, -31.95f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.13f, 69.17f, -31.95f }, { 1224.09f, 68.98f, -31.94f }, { 1224.11f, 69.17f, -31.91f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.09f, 68.98f, -31.94f }, { 1224.13f, 69.17f, -31.95f }, { 1224.10f, 68.98f, -31.98f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.11f, 69.17f, -31.91f }, { 1224.09f, 68.98f, -31.94f }, { 1224.12f, 69.16f, -31.91f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.12f, 69.16f, -31.91f }, { 1224.09f, 68.98f, -31.94f }, { 1224.10f, 68.97f, -31.94f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.85f, 69.55f, -32.87f }, { 1224.76f, 69.52f, -32.90f }, { 1224.75f, 69.54f, -32.90f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.76f, 69.54f, -32.90f }, { 1224.85f, 69.58f, -32.87f }, { 1224.85f, 69.55f, -32.87f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.74f, 69.54f, -32.86f }, { 1224.83f, 69.58f, -32.84f }, { 1224.75f, 69.54f, -32.90f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.76f, 69.54f, -32.90f }, { 1224.83f, 69.58f, -32.84f }, { 1224.85f, 69.58f, -32.87f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.74f, 69.54f, -32.86f }, { 1224.61f, 69.58f, -32.29f }, { 1224.83f, 69.58f, -32.84f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.74f, 69.54f, -32.86f }, { 1224.52f, 69.54f, -32.33f }, { 1224.61f, 69.58f, -32.29f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.30f, 69.54f, -31.80f }, { 1224.61f, 69.58f, -32.29f }, { 1224.52f, 69.54f, -32.33f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.61f, 69.58f, -32.29f }, { 1224.30f, 69.54f, -31.80f }, { 1224.38f, 69.58f, -31.75f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.28f, 69.54f, -31.76f }, { 1224.38f, 69.58f, -31.75f }, { 1224.30f, 69.54f, -31.80f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.38f, 69.58f, -31.75f }, { 1224.28f, 69.54f, -31.76f }, { 1224.36f, 69.58f, -31.71f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.36f, 69.58f, -31.71f }, { 1224.28f, 69.54f, -31.76f }, { 1224.37f, 69.55f, -31.71f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.29f, 69.52f, -31.76f }, { 1224.37f, 69.55f, -31.71f }, { 1224.28f, 69.54f, -31.76f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1225.50f, 69.60f, -32.57f }, { 1224.93f, 69.60f, -32.85f }, { 1224.92f, 69.60f, -32.82f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.93f, 69.60f, -32.85f }, { 1225.50f, 69.60f, -32.57f }, { 1225.52f, 69.60f, -32.61f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.92f, 69.60f, -32.82f }, { 1225.27f, 69.60f, -32.02f }, { 1225.50f, 69.60f, -32.57f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.68f, 69.60f, -32.26f }, { 1225.27f, 69.60f, -32.02f }, { 1224.92f, 69.60f, -32.82f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.45f, 69.60f, -31.70f }, { 1225.27f, 69.60f, -32.02f }, { 1224.69f, 69.60f, -32.26f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1225.27f, 69.60f, -32.02f }, { 1224.46f, 69.60f, -31.70f }, { 1225.04f, 69.60f, -31.46f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.44f, 69.60f, -31.67f }, { 1225.03f, 69.60f, -31.43f }, { 1224.46f, 69.60f, -31.70f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.45f, 69.60f, -31.70f }, { 1225.03f, 69.60f, -31.43f }, { 1225.04f, 69.60f, -31.46f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.93f, 69.60f, -32.85f }, { 1225.52f, 69.60f, -32.61f }, { 1224.93f, 69.58f, -32.85f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.93f, 69.58f, -32.85f }, { 1225.52f, 69.60f, -32.61f }, { 1225.52f, 69.58f, -32.61f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1224.44f, 69.58f, -31.67f }, { 1225.03f, 69.60f, -31.43f }, { 1224.44f, 69.60f, -31.67f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1225.03f, 69.60f, -31.43f }, { 1224.44f, 69.58f, -31.67f }, { 1225.03f, 69.58f, -31.42f }, 68.75f, 0.00f },   // waterfeature_02 props 655
    { { 1210.96f, 69.33f, -30.06f }, { 1211.02f, 69.15f, -30.06f }, { 1210.97f, 69.33f, -30.09f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1211.02f, 69.15f, -30.06f }, { 1211.03f, 69.15f, -30.09f }, { 1210.97f, 69.33f, -30.09f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1210.71f, 69.56f, -30.13f }, { 1210.63f, 69.58f, -30.16f }, { 1210.70f, 69.56f, -30.10f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1210.70f, 69.56f, -30.10f }, { 1210.63f, 69.58f, -30.16f }, { 1210.62f, 69.58f, -30.12f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1211.06f, 68.96f, -30.07f }, { 1211.08f, 68.74f, -30.08f }, { 1211.07f, 68.96f, -30.10f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1211.08f, 68.74f, -30.08f }, { 1211.09f, 68.74f, -30.12f }, { 1211.07f, 68.96f, -30.10f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1210.89f, 69.46f, -30.10f }, { 1210.81f, 69.52f, -30.11f }, { 1210.88f, 69.46f, -30.07f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1210.88f, 69.46f, -30.07f }, { 1210.81f, 69.52f, -30.11f }, { 1210.79f, 69.52f, -30.07f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1211.09f, 68.74f, -30.11f }, { 1211.26f, 68.96f, -30.55f }, { 1211.07f, 68.96f, -30.10f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1211.26f, 68.96f, -30.55f }, { 1211.09f, 68.74f, -30.12f }, { 1211.27f, 68.74f, -30.54f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1211.27f, 68.74f, -30.54f }, { 1211.45f, 68.74f, -30.97f }, { 1211.26f, 68.96f, -30.55f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1211.44f, 68.96f, -30.99f }, { 1211.26f, 68.96f, -30.55f }, { 1211.45f, 68.74f, -30.97f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1211.03f, 69.15f, -30.09f }, { 1211.17f, 69.33f, -30.58f }, { 1210.97f, 69.33f, -30.09f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1211.17f, 69.33f, -30.58f }, { 1211.03f, 69.15f, -30.09f }, { 1211.23f, 69.15f, -30.56f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1211.23f, 69.15f, -30.56f }, { 1211.42f, 69.15f, -31.02f }, { 1211.17f, 69.33f, -30.58f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1211.38f, 69.33f, -31.07f }, { 1211.17f, 69.33f, -30.58f }, { 1211.42f, 69.15f, -31.02f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1210.89f, 69.46f, -30.10f }, { 1211.03f, 69.52f, -30.64f }, { 1210.81f, 69.52f, -30.11f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1211.03f, 69.52f, -30.64f }, { 1210.89f, 69.46f, -30.10f }, { 1211.10f, 69.46f, -30.61f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1211.31f, 69.46f, -31.12f }, { 1211.03f, 69.52f, -30.64f }, { 1211.10f, 69.46f, -30.61f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1211.03f, 69.52f, -30.64f }, { 1211.31f, 69.46f, -31.12f }, { 1211.25f, 69.52f, -31.18f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1210.71f, 69.56f, -30.13f }, { 1210.86f, 69.58f, -30.71f }, { 1210.63f, 69.58f, -30.16f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1210.86f, 69.58f, -30.71f }, { 1210.72f, 69.56f, -30.13f }, { 1210.94f, 69.56f, -30.68f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1211.17f, 69.56f, -31.22f }, { 1210.86f, 69.58f, -30.71f }, { 1210.94f, 69.56f, -30.68f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1210.86f, 69.58f, -30.71f }, { 1211.17f, 69.56f, -31.22f }, { 1211.09f, 69.58f, -31.27f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1211.46f, 68.74f, -31.00f }, { 1211.46f, 68.96f, -31.03f }, { 1211.44f, 68.96f, -30.99f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1211.44f, 68.96f, -30.99f }, { 1211.45f, 68.74f, -30.97f }, { 1211.46f, 68.74f, -31.00f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1211.43f, 69.15f, -31.06f }, { 1211.39f, 69.33f, -31.11f }, { 1211.38f, 69.33f, -31.07f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1211.38f, 69.33f, -31.07f }, { 1211.42f, 69.15f, -31.02f }, { 1211.43f, 69.15f, -31.06f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1211.33f, 69.46f, -31.16f }, { 1211.25f, 69.52f, -31.18f }, { 1211.31f, 69.46f, -31.12f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1211.25f, 69.52f, -31.18f }, { 1211.33f, 69.46f, -31.16f }, { 1211.26f, 69.52f, -31.21f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1211.18f, 69.56f, -31.26f }, { 1211.09f, 69.58f, -31.27f }, { 1211.17f, 69.56f, -31.22f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1211.09f, 69.58f, -31.27f }, { 1211.18f, 69.56f, -31.26f }, { 1211.11f, 69.58f, -31.30f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1210.79f, 69.50f, -30.08f }, { 1210.87f, 69.44f, -30.07f }, { 1210.88f, 69.46f, -30.07f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1210.88f, 69.46f, -30.07f }, { 1210.79f, 69.52f, -30.07f }, { 1210.79f, 69.50f, -30.08f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1211.26f, 69.50f, -31.21f }, { 1211.33f, 69.46f, -31.16f }, { 1211.32f, 69.44f, -31.16f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1211.33f, 69.46f, -31.16f }, { 1211.26f, 69.50f, -31.21f }, { 1211.26f, 69.52f, -31.21f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1211.05f, 68.95f, -30.07f }, { 1211.08f, 68.74f, -30.08f }, { 1211.06f, 68.96f, -30.07f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1211.08f, 68.74f, -30.08f }, { 1211.05f, 68.95f, -30.07f }, { 1211.07f, 68.73f, -30.09f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1211.45f, 68.95f, -31.03f }, { 1211.46f, 68.74f, -31.00f }, { 1211.45f, 68.73f, -31.01f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1211.46f, 68.74f, -31.00f }, { 1211.45f, 68.95f, -31.03f }, { 1211.46f, 68.96f, -31.03f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1210.95f, 69.31f, -30.06f }, { 1211.01f, 69.14f, -30.06f }, { 1211.02f, 69.15f, -30.06f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1211.02f, 69.15f, -30.06f }, { 1210.96f, 69.33f, -30.06f }, { 1210.95f, 69.31f, -30.06f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1211.39f, 69.31f, -31.11f }, { 1211.43f, 69.15f, -31.06f }, { 1211.43f, 69.14f, -31.06f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1211.43f, 69.15f, -31.06f }, { 1211.39f, 69.31f, -31.11f }, { 1211.39f, 69.33f, -31.11f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1210.62f, 69.56f, -30.12f }, { 1210.70f, 69.53f, -30.10f }, { 1210.70f, 69.56f, -30.10f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1210.70f, 69.56f, -30.10f }, { 1210.62f, 69.58f, -30.12f }, { 1210.61f, 69.56f, -30.12f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1211.11f, 69.56f, -31.30f }, { 1211.18f, 69.56f, -31.26f }, { 1211.18f, 69.53f, -31.26f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1211.18f, 69.56f, -31.26f }, { 1211.10f, 69.56f, -31.30f }, { 1211.11f, 69.58f, -31.30f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1210.87f, 69.44f, -30.07f }, { 1210.95f, 69.31f, -30.06f }, { 1210.96f, 69.33f, -30.06f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1210.96f, 69.33f, -30.06f }, { 1210.88f, 69.46f, -30.07f }, { 1210.87f, 69.44f, -30.07f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1210.88f, 69.46f, -30.07f }, { 1210.96f, 69.33f, -30.06f }, { 1210.89f, 69.46f, -30.10f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1210.96f, 69.33f, -30.06f }, { 1210.97f, 69.33f, -30.09f }, { 1210.89f, 69.46f, -30.10f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1211.10f, 69.46f, -30.61f }, { 1210.89f, 69.46f, -30.10f }, { 1210.97f, 69.33f, -30.09f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1211.10f, 69.46f, -30.61f }, { 1210.97f, 69.33f, -30.09f }, { 1211.17f, 69.33f, -30.58f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1211.38f, 69.33f, -31.07f }, { 1211.10f, 69.46f, -30.61f }, { 1211.17f, 69.33f, -30.58f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1211.10f, 69.46f, -30.61f }, { 1211.38f, 69.33f, -31.07f }, { 1211.31f, 69.46f, -31.12f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1211.31f, 69.46f, -31.12f }, { 1211.39f, 69.33f, -31.11f }, { 1211.33f, 69.46f, -31.16f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1211.39f, 69.33f, -31.11f }, { 1211.31f, 69.46f, -31.12f }, { 1211.38f, 69.33f, -31.07f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1211.33f, 69.46f, -31.16f }, { 1211.39f, 69.33f, -31.11f }, { 1211.32f, 69.44f, -31.16f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1211.39f, 69.31f, -31.11f }, { 1211.32f, 69.44f, -31.16f }, { 1211.39f, 69.33f, -31.11f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1211.01f, 69.14f, -30.06f }, { 1211.06f, 68.96f, -30.07f }, { 1211.02f, 69.15f, -30.06f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1211.06f, 68.96f, -30.07f }, { 1211.01f, 69.14f, -30.06f }, { 1211.05f, 68.95f, -30.07f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1211.02f, 69.15f, -30.06f }, { 1211.06f, 68.96f, -30.07f }, { 1211.03f, 69.15f, -30.09f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1211.06f, 68.96f, -30.07f }, { 1211.07f, 68.96f, -30.10f }, { 1211.03f, 69.15f, -30.09f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1211.23f, 69.15f, -30.56f }, { 1211.03f, 69.15f, -30.09f }, { 1211.07f, 68.96f, -30.10f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1211.23f, 69.15f, -30.56f }, { 1211.07f, 68.96f, -30.10f }, { 1211.26f, 68.96f, -30.55f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1211.44f, 68.96f, -30.99f }, { 1211.23f, 69.15f, -30.56f }, { 1211.26f, 68.96f, -30.55f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1211.23f, 69.15f, -30.56f }, { 1211.44f, 68.96f, -30.99f }, { 1211.42f, 69.15f, -31.02f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1211.42f, 69.15f, -31.02f }, { 1211.46f, 68.96f, -31.03f }, { 1211.43f, 69.15f, -31.06f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1211.46f, 68.96f, -31.03f }, { 1211.42f, 69.15f, -31.02f }, { 1211.44f, 68.96f, -30.99f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1211.43f, 69.15f, -31.06f }, { 1211.46f, 68.96f, -31.03f }, { 1211.43f, 69.14f, -31.06f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1211.43f, 69.14f, -31.06f }, { 1211.46f, 68.96f, -31.03f }, { 1211.45f, 68.95f, -31.03f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1210.70f, 69.53f, -30.10f }, { 1210.79f, 69.50f, -30.08f }, { 1210.79f, 69.52f, -30.07f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1210.79f, 69.52f, -30.07f }, { 1210.70f, 69.56f, -30.10f }, { 1210.70f, 69.53f, -30.10f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1210.81f, 69.52f, -30.11f }, { 1210.72f, 69.56f, -30.13f }, { 1210.79f, 69.52f, -30.07f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1210.79f, 69.52f, -30.07f }, { 1210.72f, 69.56f, -30.13f }, { 1210.70f, 69.56f, -30.10f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1210.81f, 69.52f, -30.11f }, { 1210.94f, 69.56f, -30.68f }, { 1210.72f, 69.56f, -30.13f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1210.81f, 69.52f, -30.11f }, { 1211.03f, 69.52f, -30.64f }, { 1210.94f, 69.56f, -30.68f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1211.25f, 69.52f, -31.18f }, { 1210.94f, 69.56f, -30.68f }, { 1211.03f, 69.52f, -30.64f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1210.94f, 69.56f, -30.68f }, { 1211.25f, 69.52f, -31.18f }, { 1211.17f, 69.56f, -31.22f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1211.26f, 69.52f, -31.21f }, { 1211.17f, 69.56f, -31.22f }, { 1211.25f, 69.52f, -31.18f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1211.17f, 69.56f, -31.22f }, { 1211.26f, 69.52f, -31.21f }, { 1211.18f, 69.56f, -31.26f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1211.18f, 69.56f, -31.26f }, { 1211.26f, 69.52f, -31.21f }, { 1211.18f, 69.53f, -31.26f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1211.26f, 69.50f, -31.21f }, { 1211.18f, 69.53f, -31.26f }, { 1211.26f, 69.52f, -31.21f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1210.04f, 69.58f, -30.40f }, { 1210.62f, 69.58f, -30.12f }, { 1210.63f, 69.58f, -30.16f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1210.62f, 69.58f, -30.12f }, { 1210.04f, 69.58f, -30.40f }, { 1210.03f, 69.58f, -30.36f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1210.63f, 69.58f, -30.16f }, { 1210.27f, 69.58f, -30.95f }, { 1210.04f, 69.58f, -30.40f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1210.86f, 69.58f, -30.71f }, { 1210.27f, 69.58f, -30.95f }, { 1210.63f, 69.58f, -30.16f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1211.09f, 69.58f, -31.27f }, { 1210.27f, 69.58f, -30.95f }, { 1210.86f, 69.58f, -30.71f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1210.27f, 69.58f, -30.95f }, { 1211.09f, 69.58f, -31.27f }, { 1210.50f, 69.58f, -31.51f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1211.11f, 69.58f, -31.30f }, { 1210.52f, 69.58f, -31.55f }, { 1211.09f, 69.58f, -31.27f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1211.09f, 69.58f, -31.27f }, { 1210.52f, 69.58f, -31.55f }, { 1210.50f, 69.58f, -31.51f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1210.62f, 69.58f, -30.12f }, { 1210.03f, 69.58f, -30.36f }, { 1210.61f, 69.56f, -30.12f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1210.62f, 69.56f, -30.12f }, { 1210.03f, 69.58f, -30.36f }, { 1210.03f, 69.56f, -30.36f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1211.11f, 69.56f, -31.30f }, { 1210.52f, 69.58f, -31.55f }, { 1211.11f, 69.58f, -31.30f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1210.52f, 69.58f, -31.55f }, { 1211.10f, 69.56f, -31.30f }, { 1210.52f, 69.56f, -31.55f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1222.01f, 69.37f, -26.71f }, { 1221.94f, 69.19f, -26.71f }, { 1221.99f, 69.37f, -26.68f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1221.94f, 69.19f, -26.71f }, { 1221.93f, 69.19f, -26.67f }, { 1221.99f, 69.37f, -26.68f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1222.25f, 69.60f, -26.63f }, { 1222.34f, 69.62f, -26.61f }, { 1222.26f, 69.60f, -26.67f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1222.26f, 69.60f, -26.67f }, { 1222.34f, 69.62f, -26.61f }, { 1222.35f, 69.62f, -26.65f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1221.91f, 69.00f, -26.70f }, { 1221.89f, 68.78f, -26.69f }, { 1221.89f, 69.00f, -26.67f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1221.89f, 68.78f, -26.69f }, { 1221.87f, 68.78f, -26.65f }, { 1221.89f, 69.00f, -26.67f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1222.08f, 69.50f, -26.67f }, { 1222.16f, 69.56f, -26.66f }, { 1222.09f, 69.50f, -26.70f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1222.09f, 69.50f, -26.70f }, { 1222.16f, 69.56f, -26.66f }, { 1222.17f, 69.56f, -26.69f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1221.87f, 68.78f, -26.65f }, { 1221.71f, 69.00f, -26.22f }, { 1221.89f, 69.00f, -26.67f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1221.71f, 69.00f, -26.22f }, { 1221.87f, 68.78f, -26.65f }, { 1221.70f, 68.78f, -26.23f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1221.70f, 68.78f, -26.23f }, { 1221.52f, 68.78f, -25.80f }, { 1221.71f, 69.00f, -26.22f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1221.52f, 69.00f, -25.77f }, { 1221.71f, 69.00f, -26.22f }, { 1221.52f, 68.78f, -25.80f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1221.93f, 69.19f, -26.67f }, { 1221.79f, 69.37f, -26.19f }, { 1221.99f, 69.37f, -26.68f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1221.79f, 69.37f, -26.19f }, { 1221.93f, 69.19f, -26.67f }, { 1221.74f, 69.19f, -26.21f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1221.74f, 69.19f, -26.21f }, { 1221.55f, 69.19f, -25.74f }, { 1221.79f, 69.37f, -26.19f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1221.59f, 69.37f, -25.70f }, { 1221.79f, 69.37f, -26.19f }, { 1221.55f, 69.19f, -25.74f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1222.08f, 69.50f, -26.67f }, { 1221.94f, 69.56f, -26.12f }, { 1222.16f, 69.56f, -26.66f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1221.94f, 69.56f, -26.12f }, { 1222.08f, 69.50f, -26.67f }, { 1221.86f, 69.50f, -26.16f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1221.65f, 69.50f, -25.64f }, { 1221.94f, 69.56f, -26.12f }, { 1221.86f, 69.50f, -26.16f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1221.94f, 69.56f, -26.12f }, { 1221.65f, 69.50f, -25.64f }, { 1221.72f, 69.56f, -25.59f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1222.25f, 69.60f, -26.63f }, { 1222.10f, 69.62f, -26.06f }, { 1222.34f, 69.62f, -26.61f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1222.11f, 69.62f, -26.06f }, { 1222.25f, 69.60f, -26.63f }, { 1222.02f, 69.60f, -26.09f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1221.80f, 69.60f, -25.54f }, { 1222.10f, 69.62f, -26.06f }, { 1222.02f, 69.60f, -26.09f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1222.11f, 69.62f, -26.06f }, { 1221.80f, 69.60f, -25.54f }, { 1221.87f, 69.62f, -25.50f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1221.51f, 68.78f, -25.76f }, { 1221.51f, 69.00f, -25.74f }, { 1221.52f, 69.00f, -25.77f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1221.52f, 69.00f, -25.77f }, { 1221.52f, 68.78f, -25.80f }, { 1221.50f, 68.78f, -25.76f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1221.53f, 69.19f, -25.71f }, { 1221.57f, 69.37f, -25.66f }, { 1221.59f, 69.37f, -25.70f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1221.59f, 69.37f, -25.70f }, { 1221.55f, 69.19f, -25.74f }, { 1221.53f, 69.19f, -25.71f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1221.64f, 69.50f, -25.61f }, { 1221.72f, 69.56f, -25.59f }, { 1221.65f, 69.50f, -25.64f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1221.72f, 69.56f, -25.59f }, { 1221.64f, 69.50f, -25.61f }, { 1221.70f, 69.56f, -25.56f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1221.78f, 69.60f, -25.51f }, { 1221.87f, 69.62f, -25.50f }, { 1221.80f, 69.60f, -25.54f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1221.87f, 69.62f, -25.50f }, { 1221.78f, 69.60f, -25.51f }, { 1221.86f, 69.62f, -25.46f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1222.18f, 69.53f, -26.69f }, { 1222.09f, 69.48f, -26.70f }, { 1222.09f, 69.50f, -26.70f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1222.09f, 69.50f, -26.70f }, { 1222.17f, 69.56f, -26.69f }, { 1222.18f, 69.53f, -26.69f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1221.70f, 69.53f, -25.55f }, { 1221.64f, 69.50f, -25.61f }, { 1221.64f, 69.48f, -25.61f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1221.64f, 69.50f, -25.61f }, { 1221.70f, 69.53f, -25.55f }, { 1221.70f, 69.56f, -25.56f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1221.92f, 68.99f, -26.70f }, { 1221.89f, 68.78f, -26.69f }, { 1221.91f, 69.00f, -26.70f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1221.89f, 68.78f, -26.69f }, { 1221.92f, 68.99f, -26.70f }, { 1221.90f, 68.77f, -26.68f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1221.52f, 68.99f, -25.73f }, { 1221.50f, 68.78f, -25.76f }, { 1221.51f, 68.77f, -25.76f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1221.51f, 68.78f, -25.76f }, { 1221.52f, 68.99f, -25.73f }, { 1221.51f, 69.00f, -25.74f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1222.01f, 69.35f, -26.71f }, { 1221.95f, 69.18f, -26.70f }, { 1221.94f, 69.19f, -26.71f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1221.94f, 69.19f, -26.71f }, { 1222.01f, 69.37f, -26.71f }, { 1222.01f, 69.35f, -26.71f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1221.58f, 69.35f, -25.66f }, { 1221.53f, 69.19f, -25.71f }, { 1221.54f, 69.18f, -25.70f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1221.53f, 69.19f, -25.71f }, { 1221.58f, 69.35f, -25.66f }, { 1221.57f, 69.37f, -25.66f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1222.35f, 69.60f, -26.65f }, { 1222.27f, 69.57f, -26.67f }, { 1222.26f, 69.60f, -26.67f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1222.26f, 69.60f, -26.67f }, { 1222.35f, 69.62f, -26.65f }, { 1222.35f, 69.60f, -26.65f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1221.86f, 69.60f, -25.46f }, { 1221.78f, 69.60f, -25.51f }, { 1221.79f, 69.57f, -25.51f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1221.78f, 69.60f, -25.51f }, { 1221.86f, 69.60f, -25.46f }, { 1221.86f, 69.62f, -25.46f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1222.09f, 69.47f, -26.70f }, { 1222.01f, 69.35f, -26.71f }, { 1222.01f, 69.37f, -26.71f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1222.01f, 69.37f, -26.71f }, { 1222.09f, 69.50f, -26.70f }, { 1222.09f, 69.48f, -26.70f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1222.09f, 69.50f, -26.70f }, { 1222.01f, 69.37f, -26.71f }, { 1222.08f, 69.50f, -26.67f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1222.01f, 69.37f, -26.71f }, { 1221.99f, 69.37f, -26.68f }, { 1222.08f, 69.50f, -26.67f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1221.86f, 69.50f, -26.16f }, { 1222.08f, 69.50f, -26.67f }, { 1221.99f, 69.37f, -26.68f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1221.86f, 69.50f, -26.16f }, { 1221.99f, 69.37f, -26.68f }, { 1221.79f, 69.37f, -26.19f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1221.59f, 69.37f, -25.70f }, { 1221.86f, 69.50f, -26.16f }, { 1221.79f, 69.37f, -26.19f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1221.86f, 69.50f, -26.16f }, { 1221.59f, 69.37f, -25.70f }, { 1221.65f, 69.50f, -25.64f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1221.65f, 69.50f, -25.64f }, { 1221.57f, 69.37f, -25.66f }, { 1221.64f, 69.50f, -25.61f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1221.57f, 69.37f, -25.66f }, { 1221.65f, 69.50f, -25.64f }, { 1221.59f, 69.37f, -25.70f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1221.64f, 69.50f, -25.61f }, { 1221.57f, 69.37f, -25.66f }, { 1221.64f, 69.48f, -25.61f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1221.58f, 69.35f, -25.66f }, { 1221.64f, 69.48f, -25.61f }, { 1221.57f, 69.37f, -25.66f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1221.95f, 69.18f, -26.70f }, { 1221.91f, 69.00f, -26.70f }, { 1221.94f, 69.19f, -26.71f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1221.91f, 69.00f, -26.70f }, { 1221.95f, 69.18f, -26.70f }, { 1221.92f, 68.99f, -26.70f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1221.94f, 69.19f, -26.71f }, { 1221.91f, 69.00f, -26.70f }, { 1221.93f, 69.19f, -26.67f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1221.91f, 69.00f, -26.70f }, { 1221.89f, 69.00f, -26.67f }, { 1221.93f, 69.19f, -26.67f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1221.74f, 69.19f, -26.21f }, { 1221.93f, 69.19f, -26.67f }, { 1221.89f, 69.00f, -26.67f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1221.74f, 69.19f, -26.21f }, { 1221.89f, 69.00f, -26.67f }, { 1221.71f, 69.00f, -26.22f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1221.52f, 69.00f, -25.77f }, { 1221.74f, 69.19f, -26.21f }, { 1221.71f, 69.00f, -26.22f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1221.74f, 69.19f, -26.21f }, { 1221.52f, 69.00f, -25.77f }, { 1221.55f, 69.19f, -25.74f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1221.55f, 69.19f, -25.74f }, { 1221.51f, 69.00f, -25.74f }, { 1221.53f, 69.19f, -25.71f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1221.51f, 69.00f, -25.74f }, { 1221.55f, 69.19f, -25.74f }, { 1221.52f, 69.00f, -25.77f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1221.53f, 69.19f, -25.71f }, { 1221.51f, 69.00f, -25.74f }, { 1221.54f, 69.18f, -25.70f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1221.54f, 69.18f, -25.70f }, { 1221.51f, 69.00f, -25.74f }, { 1221.52f, 68.99f, -25.73f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1222.27f, 69.57f, -26.67f }, { 1222.18f, 69.53f, -26.69f }, { 1222.17f, 69.56f, -26.69f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1222.17f, 69.56f, -26.69f }, { 1222.26f, 69.60f, -26.67f }, { 1222.27f, 69.57f, -26.67f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1222.16f, 69.56f, -26.66f }, { 1222.25f, 69.60f, -26.63f }, { 1222.17f, 69.56f, -26.69f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1222.17f, 69.56f, -26.69f }, { 1222.25f, 69.60f, -26.63f }, { 1222.26f, 69.60f, -26.67f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1222.16f, 69.56f, -26.66f }, { 1222.02f, 69.60f, -26.09f }, { 1222.25f, 69.60f, -26.63f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1222.16f, 69.56f, -26.66f }, { 1221.94f, 69.56f, -26.12f }, { 1222.02f, 69.60f, -26.09f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1221.72f, 69.56f, -25.59f }, { 1222.02f, 69.60f, -26.09f }, { 1221.94f, 69.56f, -26.12f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1222.02f, 69.60f, -26.09f }, { 1221.72f, 69.56f, -25.59f }, { 1221.80f, 69.60f, -25.54f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1221.70f, 69.56f, -25.55f }, { 1221.80f, 69.60f, -25.54f }, { 1221.72f, 69.56f, -25.59f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1221.80f, 69.60f, -25.54f }, { 1221.70f, 69.56f, -25.56f }, { 1221.78f, 69.60f, -25.51f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1221.78f, 69.60f, -25.51f }, { 1221.70f, 69.56f, -25.56f }, { 1221.79f, 69.57f, -25.51f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1221.70f, 69.53f, -25.55f }, { 1221.79f, 69.57f, -25.51f }, { 1221.70f, 69.56f, -25.56f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1222.92f, 69.62f, -26.37f }, { 1222.35f, 69.62f, -26.65f }, { 1222.34f, 69.62f, -26.61f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1222.35f, 69.62f, -26.65f }, { 1222.92f, 69.62f, -26.37f }, { 1222.94f, 69.62f, -26.40f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1222.34f, 69.62f, -26.61f }, { 1222.69f, 69.62f, -25.81f }, { 1222.92f, 69.62f, -26.37f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1222.11f, 69.62f, -26.06f }, { 1222.69f, 69.62f, -25.81f }, { 1222.34f, 69.62f, -26.61f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1221.87f, 69.62f, -25.50f }, { 1222.69f, 69.62f, -25.81f }, { 1222.10f, 69.62f, -26.06f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1222.69f, 69.62f, -25.81f }, { 1221.87f, 69.62f, -25.50f }, { 1222.46f, 69.62f, -25.26f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1221.86f, 69.62f, -25.46f }, { 1222.45f, 69.62f, -25.22f }, { 1221.87f, 69.62f, -25.50f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1221.87f, 69.62f, -25.50f }, { 1222.45f, 69.62f, -25.22f }, { 1222.46f, 69.62f, -25.26f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1222.35f, 69.62f, -26.65f }, { 1222.94f, 69.62f, -26.40f }, { 1222.35f, 69.60f, -26.65f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1222.35f, 69.60f, -26.65f }, { 1222.94f, 69.62f, -26.40f }, { 1222.94f, 69.60f, -26.40f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1221.86f, 69.60f, -25.46f }, { 1222.45f, 69.62f, -25.22f }, { 1221.86f, 69.62f, -25.46f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1222.45f, 69.62f, -25.22f }, { 1221.86f, 69.60f, -25.46f }, { 1222.45f, 69.60f, -25.22f }, 68.77f, 0.00f },   // waterfeature_02 props 655
    { { 1157.38f, 47.56f, -171.42f }, { 1157.34f, 47.40f, -171.45f }, { 1157.36f, 47.56f, -171.40f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1157.34f, 47.40f, -171.45f }, { 1157.31f, 47.40f, -171.43f }, { 1157.36f, 47.56f, -171.40f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1157.53f, 47.76f, -171.24f }, { 1157.58f, 47.79f, -171.18f }, { 1157.55f, 47.76f, -171.26f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1157.56f, 47.76f, -171.26f }, { 1157.58f, 47.79f, -171.18f }, { 1157.61f, 47.79f, -171.20f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1157.30f, 47.22f, -171.46f }, { 1157.28f, 47.02f, -171.46f }, { 1157.28f, 47.22f, -171.45f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1157.28f, 47.02f, -171.46f }, { 1157.25f, 47.02f, -171.45f }, { 1157.28f, 47.22f, -171.45f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1157.41f, 47.67f, -171.35f }, { 1157.47f, 47.73f, -171.31f }, { 1157.44f, 47.67f, -171.37f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1157.44f, 47.67f, -171.37f }, { 1157.47f, 47.73f, -171.31f }, { 1157.50f, 47.73f, -171.33f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1157.25f, 47.02f, -171.45f }, { 1156.91f, 47.22f, -171.20f }, { 1157.28f, 47.22f, -171.45f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.91f, 47.22f, -171.20f }, { 1157.25f, 47.02f, -171.45f }, { 1156.91f, 47.02f, -171.21f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.91f, 47.02f, -171.21f }, { 1156.56f, 47.02f, -170.98f }, { 1156.91f, 47.22f, -171.20f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.55f, 47.22f, -170.96f }, { 1156.91f, 47.22f, -171.20f }, { 1156.56f, 47.02f, -170.98f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1157.31f, 47.40f, -171.43f }, { 1156.96f, 47.56f, -171.14f }, { 1157.36f, 47.56f, -171.40f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.96f, 47.56f, -171.14f }, { 1157.31f, 47.40f, -171.43f }, { 1156.93f, 47.40f, -171.18f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.93f, 47.40f, -171.18f }, { 1156.55f, 47.40f, -170.93f }, { 1156.96f, 47.56f, -171.14f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.56f, 47.56f, -170.87f }, { 1156.96f, 47.56f, -171.14f }, { 1156.55f, 47.40f, -170.93f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1157.41f, 47.67f, -171.35f }, { 1157.04f, 47.73f, -171.02f }, { 1157.47f, 47.73f, -171.31f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1157.04f, 47.73f, -171.02f }, { 1157.41f, 47.67f, -171.36f }, { 1157.00f, 47.67f, -171.08f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.59f, 47.67f, -170.80f }, { 1157.04f, 47.73f, -171.02f }, { 1157.00f, 47.67f, -171.08f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1157.04f, 47.73f, -171.02f }, { 1156.59f, 47.67f, -170.80f }, { 1156.61f, 47.73f, -170.73f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1157.53f, 47.76f, -171.24f }, { 1157.13f, 47.79f, -170.88f }, { 1157.58f, 47.79f, -171.18f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1157.13f, 47.78f, -170.88f }, { 1157.53f, 47.76f, -171.24f }, { 1157.09f, 47.76f, -170.95f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.64f, 47.76f, -170.65f }, { 1157.13f, 47.79f, -170.88f }, { 1157.09f, 47.76f, -170.95f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1157.13f, 47.78f, -170.88f }, { 1156.64f, 47.76f, -170.65f }, { 1156.68f, 47.79f, -170.58f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.53f, 47.02f, -170.96f }, { 1156.52f, 47.22f, -170.94f }, { 1156.55f, 47.22f, -170.96f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.55f, 47.22f, -170.96f }, { 1156.56f, 47.02f, -170.98f }, { 1156.53f, 47.02f, -170.96f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.53f, 47.40f, -170.91f }, { 1156.53f, 47.56f, -170.85f }, { 1156.56f, 47.56f, -170.87f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.56f, 47.56f, -170.87f }, { 1156.55f, 47.40f, -170.93f }, { 1156.52f, 47.40f, -170.91f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.56f, 47.67f, -170.78f }, { 1156.61f, 47.73f, -170.73f }, { 1156.59f, 47.67f, -170.80f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.61f, 47.73f, -170.73f }, { 1156.56f, 47.67f, -170.78f }, { 1156.58f, 47.73f, -170.71f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.62f, 47.76f, -170.63f }, { 1156.68f, 47.79f, -170.58f }, { 1156.64f, 47.76f, -170.65f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.68f, 47.78f, -170.58f }, { 1156.62f, 47.76f, -170.63f }, { 1156.65f, 47.79f, -170.56f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1157.50f, 47.71f, -171.32f }, { 1157.44f, 47.65f, -171.37f }, { 1157.44f, 47.67f, -171.37f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1157.44f, 47.67f, -171.37f }, { 1157.50f, 47.73f, -171.33f }, { 1157.50f, 47.71f, -171.32f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.58f, 47.71f, -170.71f }, { 1156.56f, 47.67f, -170.78f }, { 1156.56f, 47.65f, -170.78f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.56f, 47.67f, -170.78f }, { 1156.58f, 47.71f, -170.71f }, { 1156.58f, 47.73f, -170.71f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1157.31f, 47.22f, -171.46f }, { 1157.28f, 47.02f, -171.46f }, { 1157.30f, 47.22f, -171.46f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1157.28f, 47.02f, -171.46f }, { 1157.31f, 47.22f, -171.46f }, { 1157.29f, 47.02f, -171.46f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.53f, 47.22f, -170.94f }, { 1156.53f, 47.02f, -170.96f }, { 1156.54f, 47.02f, -170.96f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.53f, 47.02f, -170.96f }, { 1156.53f, 47.22f, -170.94f }, { 1156.52f, 47.22f, -170.94f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1157.39f, 47.54f, -171.42f }, { 1157.34f, 47.39f, -171.45f }, { 1157.34f, 47.40f, -171.45f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1157.34f, 47.40f, -171.45f }, { 1157.38f, 47.56f, -171.42f }, { 1157.39f, 47.54f, -171.42f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.54f, 47.54f, -170.85f }, { 1156.52f, 47.40f, -170.91f }, { 1156.53f, 47.39f, -170.90f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.53f, 47.40f, -170.91f }, { 1156.54f, 47.54f, -170.85f }, { 1156.53f, 47.56f, -170.85f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1157.61f, 47.76f, -171.20f }, { 1157.56f, 47.74f, -171.26f }, { 1157.55f, 47.76f, -171.26f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1157.56f, 47.76f, -171.26f }, { 1157.61f, 47.79f, -171.20f }, { 1157.61f, 47.76f, -171.20f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.65f, 47.76f, -170.56f }, { 1156.62f, 47.76f, -170.63f }, { 1156.62f, 47.74f, -170.63f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.62f, 47.76f, -170.63f }, { 1156.65f, 47.76f, -170.56f }, { 1156.65f, 47.79f, -170.56f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1157.44f, 47.65f, -171.37f }, { 1157.39f, 47.54f, -171.42f }, { 1157.38f, 47.56f, -171.42f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1157.38f, 47.56f, -171.42f }, { 1157.44f, 47.67f, -171.37f }, { 1157.44f, 47.65f, -171.37f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1157.44f, 47.67f, -171.37f }, { 1157.38f, 47.56f, -171.42f }, { 1157.41f, 47.67f, -171.36f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1157.38f, 47.56f, -171.42f }, { 1157.36f, 47.56f, -171.40f }, { 1157.41f, 47.67f, -171.36f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1157.00f, 47.67f, -171.08f }, { 1157.41f, 47.67f, -171.36f }, { 1157.36f, 47.56f, -171.40f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1157.00f, 47.67f, -171.08f }, { 1157.36f, 47.56f, -171.40f }, { 1156.96f, 47.56f, -171.14f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.56f, 47.56f, -170.87f }, { 1157.00f, 47.67f, -171.08f }, { 1156.96f, 47.56f, -171.14f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1157.00f, 47.67f, -171.08f }, { 1156.56f, 47.56f, -170.87f }, { 1156.59f, 47.67f, -170.80f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.59f, 47.67f, -170.80f }, { 1156.53f, 47.56f, -170.85f }, { 1156.56f, 47.67f, -170.78f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.53f, 47.56f, -170.85f }, { 1156.59f, 47.67f, -170.80f }, { 1156.56f, 47.56f, -170.87f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.56f, 47.67f, -170.78f }, { 1156.53f, 47.56f, -170.85f }, { 1156.56f, 47.65f, -170.78f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.54f, 47.54f, -170.85f }, { 1156.56f, 47.65f, -170.78f }, { 1156.53f, 47.56f, -170.85f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1157.34f, 47.39f, -171.44f }, { 1157.30f, 47.22f, -171.46f }, { 1157.34f, 47.40f, -171.45f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1157.30f, 47.22f, -171.46f }, { 1157.34f, 47.39f, -171.45f }, { 1157.31f, 47.22f, -171.46f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1157.34f, 47.40f, -171.45f }, { 1157.30f, 47.22f, -171.46f }, { 1157.31f, 47.40f, -171.43f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1157.30f, 47.22f, -171.46f }, { 1157.28f, 47.22f, -171.45f }, { 1157.31f, 47.40f, -171.43f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.93f, 47.40f, -171.18f }, { 1157.31f, 47.40f, -171.43f }, { 1157.28f, 47.22f, -171.45f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.93f, 47.40f, -171.18f }, { 1157.28f, 47.22f, -171.45f }, { 1156.91f, 47.22f, -171.20f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.55f, 47.22f, -170.96f }, { 1156.93f, 47.40f, -171.18f }, { 1156.91f, 47.22f, -171.20f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.93f, 47.40f, -171.18f }, { 1156.55f, 47.22f, -170.96f }, { 1156.55f, 47.40f, -170.93f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.55f, 47.40f, -170.93f }, { 1156.52f, 47.22f, -170.94f }, { 1156.52f, 47.40f, -170.91f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.53f, 47.22f, -170.94f }, { 1156.55f, 47.40f, -170.93f }, { 1156.55f, 47.22f, -170.96f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.53f, 47.40f, -170.91f }, { 1156.52f, 47.22f, -170.94f }, { 1156.53f, 47.39f, -170.90f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.53f, 47.39f, -170.90f }, { 1156.52f, 47.22f, -170.94f }, { 1156.53f, 47.22f, -170.94f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1157.56f, 47.74f, -171.26f }, { 1157.50f, 47.71f, -171.32f }, { 1157.50f, 47.73f, -171.33f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1157.50f, 47.73f, -171.33f }, { 1157.55f, 47.76f, -171.26f }, { 1157.56f, 47.74f, -171.26f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1157.47f, 47.73f, -171.31f }, { 1157.53f, 47.76f, -171.24f }, { 1157.50f, 47.73f, -171.33f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1157.50f, 47.73f, -171.33f }, { 1157.53f, 47.76f, -171.24f }, { 1157.55f, 47.76f, -171.26f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1157.47f, 47.73f, -171.31f }, { 1157.09f, 47.76f, -170.95f }, { 1157.53f, 47.76f, -171.24f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1157.47f, 47.73f, -171.31f }, { 1157.04f, 47.73f, -171.02f }, { 1157.09f, 47.76f, -170.95f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.61f, 47.73f, -170.73f }, { 1157.09f, 47.76f, -170.95f }, { 1157.04f, 47.73f, -171.02f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1157.09f, 47.76f, -170.95f }, { 1156.61f, 47.73f, -170.73f }, { 1156.64f, 47.76f, -170.65f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.58f, 47.73f, -170.71f }, { 1156.64f, 47.76f, -170.65f }, { 1156.61f, 47.73f, -170.73f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.64f, 47.76f, -170.65f }, { 1156.58f, 47.73f, -170.71f }, { 1156.62f, 47.76f, -170.63f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.62f, 47.76f, -170.63f }, { 1156.58f, 47.73f, -170.71f }, { 1156.62f, 47.74f, -170.63f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.58f, 47.71f, -170.71f }, { 1156.62f, 47.74f, -170.63f }, { 1156.58f, 47.73f, -170.71f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1157.90f, 47.78f, -170.71f }, { 1157.61f, 47.79f, -171.20f }, { 1157.58f, 47.79f, -171.18f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1157.61f, 47.78f, -171.20f }, { 1157.90f, 47.79f, -170.71f }, { 1157.93f, 47.79f, -170.73f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1157.58f, 47.78f, -171.19f }, { 1157.45f, 47.79f, -170.41f }, { 1157.90f, 47.79f, -170.71f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1157.13f, 47.78f, -170.88f }, { 1157.45f, 47.79f, -170.41f }, { 1157.58f, 47.79f, -171.18f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.68f, 47.78f, -170.58f }, { 1157.45f, 47.79f, -170.41f }, { 1157.13f, 47.79f, -170.88f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1157.45f, 47.78f, -170.41f }, { 1156.68f, 47.79f, -170.58f }, { 1157.00f, 47.79f, -170.11f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.65f, 47.78f, -170.56f }, { 1156.97f, 47.79f, -170.09f }, { 1156.68f, 47.79f, -170.58f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.68f, 47.78f, -170.58f }, { 1156.97f, 47.79f, -170.09f }, { 1157.00f, 47.79f, -170.11f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1157.61f, 47.78f, -171.20f }, { 1157.93f, 47.79f, -170.73f }, { 1157.61f, 47.76f, -171.20f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1157.61f, 47.76f, -171.20f }, { 1157.93f, 47.79f, -170.73f }, { 1157.93f, 47.76f, -170.73f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.65f, 47.76f, -170.56f }, { 1156.97f, 47.79f, -170.09f }, { 1156.65f, 47.79f, -170.56f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.97f, 47.78f, -170.09f }, { 1156.65f, 47.76f, -170.56f }, { 1156.97f, 47.76f, -170.09f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1174.73f, 47.56f, -154.47f }, { 1174.70f, 47.40f, -154.42f }, { 1174.75f, 47.56f, -154.44f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1174.70f, 47.40f, -154.42f }, { 1174.72f, 47.40f, -154.40f }, { 1174.75f, 47.56f, -154.44f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1174.91f, 47.77f, -154.62f }, { 1174.97f, 47.79f, -154.67f }, { 1174.89f, 47.77f, -154.64f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1174.89f, 47.77f, -154.64f }, { 1174.97f, 47.79f, -154.67f }, { 1174.95f, 47.79f, -154.70f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1174.69f, 47.23f, -154.39f }, { 1174.69f, 47.03f, -154.37f }, { 1174.71f, 47.23f, -154.37f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1174.69f, 47.03f, -154.37f }, { 1174.71f, 47.03f, -154.34f }, { 1174.71f, 47.23f, -154.37f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1174.80f, 47.67f, -154.50f }, { 1174.85f, 47.73f, -154.56f }, { 1174.78f, 47.67f, -154.53f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1174.78f, 47.67f, -154.53f }, { 1174.85f, 47.73f, -154.56f }, { 1174.83f, 47.73f, -154.59f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1174.71f, 47.03f, -154.34f }, { 1174.95f, 47.23f, -154.00f }, { 1174.71f, 47.23f, -154.37f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1174.95f, 47.23f, -154.00f }, { 1174.71f, 47.03f, -154.34f }, { 1174.94f, 47.03f, -154.00f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1174.94f, 47.03f, -154.00f }, { 1175.17f, 47.03f, -153.65f }, { 1174.95f, 47.23f, -154.00f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1175.19f, 47.23f, -153.64f }, { 1174.95f, 47.23f, -154.00f }, { 1175.17f, 47.03f, -153.65f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1174.72f, 47.40f, -154.40f }, { 1175.01f, 47.56f, -154.05f }, { 1174.75f, 47.56f, -154.44f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1175.01f, 47.56f, -154.05f }, { 1174.72f, 47.40f, -154.40f }, { 1174.97f, 47.40f, -154.02f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1174.97f, 47.40f, -154.02f }, { 1175.22f, 47.40f, -153.64f }, { 1175.01f, 47.56f, -154.05f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1175.28f, 47.56f, -153.65f }, { 1175.01f, 47.56f, -154.05f }, { 1175.22f, 47.40f, -153.64f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1174.80f, 47.67f, -154.50f }, { 1175.13f, 47.73f, -154.13f }, { 1174.85f, 47.73f, -154.56f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1175.13f, 47.73f, -154.13f }, { 1174.80f, 47.67f, -154.50f }, { 1175.07f, 47.67f, -154.09f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1175.35f, 47.67f, -153.67f }, { 1175.13f, 47.73f, -154.13f }, { 1175.07f, 47.67f, -154.09f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1175.13f, 47.73f, -154.13f }, { 1175.35f, 47.67f, -153.67f }, { 1175.42f, 47.73f, -153.70f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1174.91f, 47.77f, -154.62f }, { 1175.27f, 47.79f, -154.22f }, { 1174.97f, 47.79f, -154.67f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1175.27f, 47.79f, -154.22f }, { 1174.91f, 47.77f, -154.62f }, { 1175.20f, 47.77f, -154.17f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1175.50f, 47.77f, -153.73f }, { 1175.27f, 47.79f, -154.22f }, { 1175.20f, 47.77f, -154.17f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1175.27f, 47.79f, -154.22f }, { 1175.50f, 47.77f, -153.73f }, { 1175.57f, 47.79f, -153.77f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1175.19f, 47.03f, -153.62f }, { 1175.21f, 47.23f, -153.61f }, { 1175.19f, 47.23f, -153.64f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1175.19f, 47.23f, -153.64f }, { 1175.17f, 47.03f, -153.65f }, { 1175.19f, 47.03f, -153.62f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1175.24f, 47.40f, -153.61f }, { 1175.30f, 47.56f, -153.62f }, { 1175.28f, 47.56f, -153.65f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1175.28f, 47.56f, -153.65f }, { 1175.22f, 47.40f, -153.64f }, { 1175.24f, 47.40f, -153.61f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1175.37f, 47.67f, -153.64f }, { 1175.42f, 47.73f, -153.70f }, { 1175.35f, 47.67f, -153.67f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1175.42f, 47.73f, -153.69f }, { 1175.37f, 47.67f, -153.64f }, { 1175.44f, 47.73f, -153.67f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1175.52f, 47.77f, -153.70f }, { 1175.57f, 47.79f, -153.77f }, { 1175.50f, 47.77f, -153.73f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1175.57f, 47.79f, -153.77f }, { 1175.52f, 47.77f, -153.70f }, { 1175.59f, 47.79f, -153.74f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1174.83f, 47.71f, -154.59f }, { 1174.78f, 47.65f, -154.53f }, { 1174.78f, 47.67f, -154.53f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1174.78f, 47.67f, -154.53f }, { 1174.83f, 47.73f, -154.59f }, { 1174.83f, 47.71f, -154.59f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1175.44f, 47.71f, -153.67f }, { 1175.37f, 47.67f, -153.64f }, { 1175.37f, 47.65f, -153.65f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1175.37f, 47.67f, -153.64f }, { 1175.44f, 47.71f, -153.67f }, { 1175.44f, 47.73f, -153.67f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1174.69f, 47.22f, -154.40f }, { 1174.69f, 47.03f, -154.37f }, { 1174.69f, 47.23f, -154.39f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1174.69f, 47.03f, -154.37f }, { 1174.69f, 47.22f, -154.40f }, { 1174.70f, 47.02f, -154.37f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1175.21f, 47.22f, -153.62f }, { 1175.19f, 47.03f, -153.62f }, { 1175.20f, 47.02f, -153.63f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1175.19f, 47.03f, -153.62f }, { 1175.22f, 47.22f, -153.62f }, { 1175.21f, 47.23f, -153.61f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1174.74f, 47.54f, -154.47f }, { 1174.71f, 47.39f, -154.43f }, { 1174.70f, 47.40f, -154.42f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1174.70f, 47.40f, -154.42f }, { 1174.73f, 47.56f, -154.47f }, { 1174.74f, 47.54f, -154.47f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1175.30f, 47.54f, -153.62f }, { 1175.24f, 47.40f, -153.61f }, { 1175.25f, 47.39f, -153.62f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1175.24f, 47.40f, -153.61f }, { 1175.30f, 47.54f, -153.63f }, { 1175.30f, 47.56f, -153.62f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1174.95f, 47.77f, -154.70f }, { 1174.89f, 47.74f, -154.64f }, { 1174.89f, 47.77f, -154.64f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1174.89f, 47.77f, -154.64f }, { 1174.95f, 47.79f, -154.70f }, { 1174.95f, 47.77f, -154.70f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1175.59f, 47.77f, -153.74f }, { 1175.52f, 47.77f, -153.70f }, { 1175.52f, 47.74f, -153.70f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1175.52f, 47.77f, -153.70f }, { 1175.59f, 47.77f, -153.74f }, { 1175.59f, 47.79f, -153.74f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1174.78f, 47.66f, -154.53f }, { 1174.74f, 47.54f, -154.47f }, { 1174.73f, 47.56f, -154.47f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1174.73f, 47.56f, -154.47f }, { 1174.78f, 47.67f, -154.53f }, { 1174.78f, 47.65f, -154.53f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1174.78f, 47.67f, -154.53f }, { 1174.73f, 47.56f, -154.47f }, { 1174.80f, 47.67f, -154.50f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1174.73f, 47.56f, -154.47f }, { 1174.75f, 47.56f, -154.44f }, { 1174.80f, 47.67f, -154.50f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1175.07f, 47.67f, -154.09f }, { 1174.80f, 47.67f, -154.50f }, { 1174.75f, 47.56f, -154.44f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1175.07f, 47.67f, -154.09f }, { 1174.75f, 47.56f, -154.44f }, { 1175.01f, 47.56f, -154.05f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1175.28f, 47.56f, -153.65f }, { 1175.07f, 47.67f, -154.09f }, { 1175.01f, 47.56f, -154.05f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1175.07f, 47.67f, -154.09f }, { 1175.28f, 47.56f, -153.65f }, { 1175.35f, 47.67f, -153.67f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1175.35f, 47.67f, -153.67f }, { 1175.30f, 47.56f, -153.62f }, { 1175.37f, 47.67f, -153.64f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1175.30f, 47.56f, -153.62f }, { 1175.35f, 47.67f, -153.67f }, { 1175.28f, 47.56f, -153.65f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1175.37f, 47.67f, -153.64f }, { 1175.30f, 47.56f, -153.62f }, { 1175.37f, 47.65f, -153.65f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1175.30f, 47.54f, -153.62f }, { 1175.37f, 47.65f, -153.65f }, { 1175.30f, 47.56f, -153.62f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1174.71f, 47.39f, -154.43f }, { 1174.69f, 47.23f, -154.39f }, { 1174.70f, 47.40f, -154.42f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1174.69f, 47.23f, -154.39f }, { 1174.71f, 47.39f, -154.43f }, { 1174.69f, 47.22f, -154.40f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1174.70f, 47.40f, -154.42f }, { 1174.69f, 47.23f, -154.39f }, { 1174.72f, 47.40f, -154.40f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1174.69f, 47.23f, -154.39f }, { 1174.71f, 47.23f, -154.37f }, { 1174.72f, 47.40f, -154.40f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1174.97f, 47.40f, -154.02f }, { 1174.72f, 47.40f, -154.40f }, { 1174.71f, 47.23f, -154.37f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1174.97f, 47.40f, -154.02f }, { 1174.71f, 47.23f, -154.37f }, { 1174.95f, 47.23f, -154.00f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1175.19f, 47.23f, -153.64f }, { 1174.97f, 47.40f, -154.02f }, { 1174.95f, 47.23f, -154.00f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1174.97f, 47.40f, -154.02f }, { 1175.19f, 47.23f, -153.64f }, { 1175.22f, 47.40f, -153.64f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1175.22f, 47.40f, -153.64f }, { 1175.21f, 47.23f, -153.61f }, { 1175.24f, 47.40f, -153.61f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1175.21f, 47.23f, -153.61f }, { 1175.22f, 47.40f, -153.64f }, { 1175.19f, 47.23f, -153.64f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1175.24f, 47.40f, -153.61f }, { 1175.21f, 47.23f, -153.61f }, { 1175.25f, 47.39f, -153.62f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1175.25f, 47.39f, -153.62f }, { 1175.21f, 47.23f, -153.61f }, { 1175.22f, 47.22f, -153.62f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1174.89f, 47.74f, -154.64f }, { 1174.83f, 47.71f, -154.59f }, { 1174.83f, 47.73f, -154.59f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1174.83f, 47.73f, -154.59f }, { 1174.89f, 47.77f, -154.64f }, { 1174.89f, 47.74f, -154.64f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1174.85f, 47.73f, -154.56f }, { 1174.91f, 47.77f, -154.62f }, { 1174.83f, 47.73f, -154.59f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1174.83f, 47.73f, -154.59f }, { 1174.91f, 47.77f, -154.62f }, { 1174.89f, 47.77f, -154.64f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1174.85f, 47.73f, -154.56f }, { 1175.20f, 47.77f, -154.17f }, { 1174.91f, 47.77f, -154.62f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1174.85f, 47.73f, -154.56f }, { 1175.13f, 47.73f, -154.13f }, { 1175.20f, 47.77f, -154.17f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1175.42f, 47.73f, -153.69f }, { 1175.20f, 47.77f, -154.17f }, { 1175.13f, 47.73f, -154.13f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1175.20f, 47.77f, -154.17f }, { 1175.42f, 47.73f, -153.70f }, { 1175.50f, 47.77f, -153.73f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1175.44f, 47.73f, -153.67f }, { 1175.50f, 47.77f, -153.73f }, { 1175.42f, 47.73f, -153.70f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1175.50f, 47.77f, -153.73f }, { 1175.44f, 47.73f, -153.67f }, { 1175.52f, 47.77f, -153.70f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1175.52f, 47.77f, -153.70f }, { 1175.44f, 47.73f, -153.67f }, { 1175.52f, 47.74f, -153.70f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1175.44f, 47.71f, -153.67f }, { 1175.52f, 47.74f, -153.70f }, { 1175.44f, 47.73f, -153.67f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1175.44f, 47.79f, -154.99f }, { 1174.95f, 47.79f, -154.70f }, { 1174.97f, 47.79f, -154.67f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1174.95f, 47.79f, -154.70f }, { 1175.44f, 47.79f, -154.99f }, { 1175.42f, 47.79f, -155.01f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1174.97f, 47.79f, -154.67f }, { 1175.74f, 47.79f, -154.54f }, { 1175.44f, 47.79f, -154.99f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1175.27f, 47.79f, -154.22f }, { 1175.74f, 47.79f, -154.54f }, { 1174.97f, 47.79f, -154.67f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1175.57f, 47.79f, -153.77f }, { 1175.74f, 47.79f, -154.54f }, { 1175.27f, 47.79f, -154.22f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1175.74f, 47.79f, -154.53f }, { 1175.57f, 47.79f, -153.77f }, { 1176.05f, 47.79f, -154.08f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1175.59f, 47.79f, -153.74f }, { 1176.06f, 47.79f, -154.06f }, { 1175.57f, 47.79f, -153.77f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1175.57f, 47.79f, -153.77f }, { 1176.06f, 47.79f, -154.06f }, { 1176.05f, 47.79f, -154.08f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1174.95f, 47.79f, -154.70f }, { 1175.42f, 47.79f, -155.01f }, { 1174.95f, 47.77f, -154.70f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1174.95f, 47.77f, -154.70f }, { 1175.42f, 47.79f, -155.01f }, { 1175.43f, 47.77f, -155.01f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1175.59f, 47.77f, -153.74f }, { 1176.06f, 47.79f, -154.06f }, { 1175.59f, 47.79f, -153.74f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1176.06f, 47.79f, -154.06f }, { 1175.59f, 47.77f, -153.74f }, { 1176.07f, 47.77f, -154.06f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1213.53f, 69.33f, -36.27f }, { 1213.59f, 69.15f, -36.27f }, { 1213.54f, 69.33f, -36.30f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.59f, 69.15f, -36.27f }, { 1213.61f, 69.15f, -36.30f }, { 1213.54f, 69.33f, -36.30f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.29f, 69.56f, -36.34f }, { 1213.20f, 69.58f, -36.36f }, { 1213.27f, 69.56f, -36.31f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.27f, 69.56f, -36.31f }, { 1213.20f, 69.58f, -36.36f }, { 1213.19f, 69.58f, -36.33f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.63f, 68.96f, -36.28f }, { 1213.65f, 68.74f, -36.29f }, { 1213.64f, 68.96f, -36.31f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.65f, 68.74f, -36.29f }, { 1213.66f, 68.74f, -36.32f }, { 1213.64f, 68.96f, -36.31f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.46f, 69.46f, -36.31f }, { 1213.38f, 69.52f, -36.32f }, { 1213.45f, 69.46f, -36.27f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.45f, 69.46f, -36.27f }, { 1213.38f, 69.52f, -36.32f }, { 1213.36f, 69.52f, -36.28f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.66f, 68.74f, -36.32f }, { 1213.83f, 68.96f, -36.76f }, { 1213.64f, 68.96f, -36.31f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.83f, 68.96f, -36.76f }, { 1213.66f, 68.74f, -36.32f }, { 1213.84f, 68.74f, -36.75f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.84f, 68.74f, -36.75f }, { 1214.02f, 68.74f, -37.18f }, { 1213.83f, 68.96f, -36.76f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1214.01f, 68.96f, -37.20f }, { 1213.83f, 68.96f, -36.76f }, { 1214.02f, 68.74f, -37.18f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.61f, 69.15f, -36.30f }, { 1213.75f, 69.33f, -36.79f }, { 1213.54f, 69.33f, -36.30f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.75f, 69.33f, -36.79f }, { 1213.61f, 69.15f, -36.30f }, { 1213.80f, 69.15f, -36.77f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.80f, 69.15f, -36.77f }, { 1213.99f, 69.15f, -37.23f }, { 1213.75f, 69.33f, -36.79f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.95f, 69.33f, -37.28f }, { 1213.75f, 69.33f, -36.79f }, { 1213.99f, 69.15f, -37.23f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.46f, 69.46f, -36.31f }, { 1213.60f, 69.52f, -36.85f }, { 1213.38f, 69.52f, -36.32f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.60f, 69.52f, -36.85f }, { 1213.46f, 69.46f, -36.31f }, { 1213.67f, 69.46f, -36.82f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.88f, 69.46f, -37.33f }, { 1213.60f, 69.52f, -36.85f }, { 1213.67f, 69.46f, -36.82f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.60f, 69.52f, -36.85f }, { 1213.88f, 69.46f, -37.33f }, { 1213.82f, 69.52f, -37.38f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.29f, 69.56f, -36.34f }, { 1213.43f, 69.58f, -36.92f }, { 1213.20f, 69.58f, -36.36f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.43f, 69.58f, -36.92f }, { 1213.29f, 69.56f, -36.34f }, { 1213.51f, 69.56f, -36.89f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.74f, 69.56f, -37.43f }, { 1213.43f, 69.58f, -36.92f }, { 1213.51f, 69.56f, -36.89f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.43f, 69.58f, -36.92f }, { 1213.74f, 69.56f, -37.43f }, { 1213.66f, 69.58f, -37.48f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1214.03f, 68.74f, -37.21f }, { 1214.03f, 68.96f, -37.24f }, { 1214.01f, 68.96f, -37.20f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1214.01f, 68.96f, -37.20f }, { 1214.02f, 68.74f, -37.18f }, { 1214.03f, 68.74f, -37.21f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1214.01f, 69.15f, -37.27f }, { 1213.96f, 69.33f, -37.31f }, { 1213.95f, 69.33f, -37.28f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.95f, 69.33f, -37.28f }, { 1213.99f, 69.15f, -37.23f }, { 1214.01f, 69.15f, -37.27f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.90f, 69.46f, -37.37f }, { 1213.82f, 69.52f, -37.38f }, { 1213.88f, 69.46f, -37.33f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.82f, 69.52f, -37.38f }, { 1213.90f, 69.46f, -37.37f }, { 1213.83f, 69.52f, -37.42f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.75f, 69.56f, -37.47f }, { 1213.66f, 69.58f, -37.48f }, { 1213.74f, 69.56f, -37.43f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.66f, 69.58f, -37.48f }, { 1213.75f, 69.56f, -37.47f }, { 1213.68f, 69.58f, -37.51f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.36f, 69.50f, -36.28f }, { 1213.44f, 69.44f, -36.28f }, { 1213.45f, 69.46f, -36.27f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.45f, 69.46f, -36.27f }, { 1213.36f, 69.52f, -36.28f }, { 1213.36f, 69.50f, -36.28f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.83f, 69.50f, -37.42f }, { 1213.90f, 69.46f, -37.37f }, { 1213.90f, 69.44f, -37.37f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.90f, 69.46f, -37.37f }, { 1213.83f, 69.50f, -37.42f }, { 1213.83f, 69.52f, -37.42f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.62f, 68.95f, -36.28f }, { 1213.65f, 68.74f, -36.29f }, { 1213.63f, 68.96f, -36.28f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.65f, 68.74f, -36.29f }, { 1213.62f, 68.95f, -36.28f }, { 1213.64f, 68.73f, -36.29f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1214.02f, 68.95f, -37.24f }, { 1214.03f, 68.74f, -37.21f }, { 1214.02f, 68.73f, -37.22f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1214.03f, 68.74f, -37.21f }, { 1214.02f, 68.95f, -37.24f }, { 1214.03f, 68.96f, -37.24f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.52f, 69.31f, -36.27f }, { 1213.58f, 69.14f, -36.27f }, { 1213.59f, 69.15f, -36.27f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.59f, 69.15f, -36.27f }, { 1213.53f, 69.33f, -36.27f }, { 1213.52f, 69.31f, -36.27f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.96f, 69.31f, -37.32f }, { 1214.01f, 69.15f, -37.27f }, { 1214.00f, 69.14f, -37.27f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1214.01f, 69.15f, -37.27f }, { 1213.96f, 69.31f, -37.32f }, { 1213.96f, 69.33f, -37.31f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.19f, 69.56f, -36.33f }, { 1213.27f, 69.53f, -36.31f }, { 1213.27f, 69.56f, -36.31f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.27f, 69.56f, -36.31f }, { 1213.19f, 69.58f, -36.33f }, { 1213.19f, 69.56f, -36.33f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.68f, 69.56f, -37.51f }, { 1213.75f, 69.56f, -37.47f }, { 1213.75f, 69.53f, -37.47f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.75f, 69.56f, -37.47f }, { 1213.68f, 69.56f, -37.51f }, { 1213.68f, 69.58f, -37.51f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.44f, 69.44f, -36.28f }, { 1213.52f, 69.31f, -36.27f }, { 1213.53f, 69.33f, -36.27f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.53f, 69.33f, -36.27f }, { 1213.45f, 69.46f, -36.27f }, { 1213.44f, 69.44f, -36.28f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.45f, 69.46f, -36.27f }, { 1213.53f, 69.33f, -36.27f }, { 1213.46f, 69.46f, -36.31f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.53f, 69.33f, -36.27f }, { 1213.54f, 69.33f, -36.30f }, { 1213.46f, 69.46f, -36.31f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.67f, 69.46f, -36.82f }, { 1213.46f, 69.46f, -36.31f }, { 1213.54f, 69.33f, -36.30f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.67f, 69.46f, -36.82f }, { 1213.54f, 69.33f, -36.30f }, { 1213.75f, 69.33f, -36.79f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.95f, 69.33f, -37.28f }, { 1213.67f, 69.46f, -36.82f }, { 1213.75f, 69.33f, -36.79f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.67f, 69.46f, -36.82f }, { 1213.95f, 69.33f, -37.28f }, { 1213.88f, 69.46f, -37.33f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.88f, 69.46f, -37.33f }, { 1213.96f, 69.33f, -37.31f }, { 1213.90f, 69.46f, -37.37f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.96f, 69.33f, -37.31f }, { 1213.88f, 69.46f, -37.33f }, { 1213.95f, 69.33f, -37.28f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.90f, 69.46f, -37.37f }, { 1213.96f, 69.33f, -37.31f }, { 1213.90f, 69.44f, -37.37f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.96f, 69.31f, -37.32f }, { 1213.90f, 69.44f, -37.37f }, { 1213.96f, 69.33f, -37.31f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.59f, 69.14f, -36.27f }, { 1213.63f, 68.96f, -36.28f }, { 1213.59f, 69.15f, -36.27f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.63f, 68.96f, -36.28f }, { 1213.58f, 69.14f, -36.27f }, { 1213.62f, 68.95f, -36.28f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.59f, 69.15f, -36.27f }, { 1213.63f, 68.96f, -36.28f }, { 1213.61f, 69.15f, -36.30f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.63f, 68.96f, -36.28f }, { 1213.64f, 68.96f, -36.31f }, { 1213.61f, 69.15f, -36.30f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.80f, 69.15f, -36.77f }, { 1213.61f, 69.15f, -36.30f }, { 1213.64f, 68.96f, -36.31f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.80f, 69.15f, -36.77f }, { 1213.64f, 68.96f, -36.31f }, { 1213.83f, 68.96f, -36.76f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1214.01f, 68.96f, -37.20f }, { 1213.80f, 69.15f, -36.77f }, { 1213.83f, 68.96f, -36.76f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.80f, 69.15f, -36.77f }, { 1214.01f, 68.96f, -37.20f }, { 1213.99f, 69.15f, -37.23f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.99f, 69.15f, -37.23f }, { 1214.03f, 68.96f, -37.24f }, { 1214.01f, 69.15f, -37.27f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1214.03f, 68.96f, -37.24f }, { 1213.99f, 69.15f, -37.23f }, { 1214.01f, 68.96f, -37.20f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1214.01f, 69.15f, -37.27f }, { 1214.03f, 68.96f, -37.24f }, { 1214.00f, 69.14f, -37.27f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1214.00f, 69.14f, -37.27f }, { 1214.03f, 68.96f, -37.24f }, { 1214.02f, 68.95f, -37.24f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.27f, 69.53f, -36.31f }, { 1213.36f, 69.50f, -36.28f }, { 1213.36f, 69.52f, -36.28f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.36f, 69.52f, -36.28f }, { 1213.27f, 69.56f, -36.31f }, { 1213.27f, 69.53f, -36.31f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.38f, 69.52f, -36.32f }, { 1213.29f, 69.56f, -36.34f }, { 1213.36f, 69.52f, -36.28f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.36f, 69.52f, -36.28f }, { 1213.29f, 69.56f, -36.34f }, { 1213.27f, 69.56f, -36.31f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.38f, 69.52f, -36.32f }, { 1213.51f, 69.56f, -36.89f }, { 1213.29f, 69.56f, -36.34f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.38f, 69.52f, -36.32f }, { 1213.60f, 69.52f, -36.85f }, { 1213.51f, 69.56f, -36.89f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.82f, 69.52f, -37.38f }, { 1213.51f, 69.56f, -36.89f }, { 1213.60f, 69.52f, -36.85f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.51f, 69.56f, -36.89f }, { 1213.82f, 69.52f, -37.38f }, { 1213.74f, 69.56f, -37.43f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.83f, 69.52f, -37.42f }, { 1213.74f, 69.56f, -37.43f }, { 1213.82f, 69.52f, -37.38f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.74f, 69.56f, -37.43f }, { 1213.83f, 69.52f, -37.42f }, { 1213.75f, 69.56f, -37.47f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.75f, 69.56f, -37.47f }, { 1213.83f, 69.52f, -37.42f }, { 1213.75f, 69.53f, -37.47f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.83f, 69.50f, -37.42f }, { 1213.75f, 69.53f, -37.47f }, { 1213.83f, 69.52f, -37.42f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1212.62f, 69.58f, -36.61f }, { 1213.19f, 69.58f, -36.33f }, { 1213.20f, 69.58f, -36.36f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.19f, 69.58f, -36.33f }, { 1212.61f, 69.58f, -36.61f }, { 1212.60f, 69.58f, -36.57f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.20f, 69.58f, -36.36f }, { 1212.85f, 69.58f, -37.16f }, { 1212.61f, 69.58f, -36.61f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.43f, 69.58f, -36.92f }, { 1212.85f, 69.58f, -37.16f }, { 1213.20f, 69.58f, -36.36f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.66f, 69.58f, -37.48f }, { 1212.85f, 69.58f, -37.16f }, { 1213.43f, 69.58f, -36.92f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1212.85f, 69.58f, -37.16f }, { 1213.66f, 69.58f, -37.48f }, { 1213.08f, 69.58f, -37.72f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.68f, 69.58f, -37.51f }, { 1213.09f, 69.58f, -37.76f }, { 1213.66f, 69.58f, -37.48f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.66f, 69.58f, -37.48f }, { 1213.09f, 69.58f, -37.76f }, { 1213.08f, 69.58f, -37.72f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.19f, 69.58f, -36.33f }, { 1212.60f, 69.58f, -36.57f }, { 1213.19f, 69.56f, -36.33f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.19f, 69.56f, -36.33f }, { 1212.60f, 69.58f, -36.57f }, { 1212.60f, 69.56f, -36.57f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.68f, 69.56f, -37.51f }, { 1213.09f, 69.58f, -37.76f }, { 1213.68f, 69.58f, -37.51f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1213.09f, 69.58f, -37.76f }, { 1213.68f, 69.56f, -37.51f }, { 1213.09f, 69.56f, -37.76f }, 68.73f, 0.00f },   // waterfeature_02 props 655
    { { 1205.20f, 55.12f, -147.94f }, { 1205.23f, 54.95f, -147.99f }, { 1205.18f, 55.12f, -147.97f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1205.23f, 54.95f, -147.99f }, { 1205.21f, 54.95f, -148.02f }, { 1205.18f, 55.12f, -147.97f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1205.00f, 55.35f, -147.78f }, { 1204.93f, 55.38f, -147.72f }, { 1205.02f, 55.35f, -147.75f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1205.02f, 55.35f, -147.75f }, { 1204.93f, 55.38f, -147.72f }, { 1204.95f, 55.38f, -147.69f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1205.24f, 54.75f, -148.03f }, { 1205.24f, 54.53f, -148.05f }, { 1205.22f, 54.75f, -148.06f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1205.24f, 54.53f, -148.05f }, { 1205.22f, 54.53f, -148.08f }, { 1205.22f, 54.75f, -148.06f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1205.12f, 55.25f, -147.91f }, { 1205.07f, 55.31f, -147.84f }, { 1205.14f, 55.25f, -147.87f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1205.14f, 55.25f, -147.87f }, { 1205.07f, 55.31f, -147.84f }, { 1205.09f, 55.31f, -147.81f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1205.22f, 54.53f, -148.08f }, { 1204.96f, 54.75f, -148.46f }, { 1205.22f, 54.75f, -148.06f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1204.96f, 54.75f, -148.46f }, { 1205.22f, 54.53f, -148.08f }, { 1204.97f, 54.53f, -148.47f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1204.97f, 54.53f, -148.47f }, { 1204.71f, 54.53f, -148.85f }, { 1204.96f, 54.75f, -148.46f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1204.69f, 54.75f, -148.86f }, { 1204.96f, 54.75f, -148.46f }, { 1204.71f, 54.53f, -148.85f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1205.21f, 54.95f, -148.02f }, { 1204.88f, 55.12f, -148.41f }, { 1205.18f, 55.12f, -147.97f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1204.88f, 55.12f, -148.41f }, { 1205.21f, 54.95f, -148.02f }, { 1204.93f, 54.95f, -148.44f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1204.93f, 54.95f, -148.44f }, { 1204.65f, 54.95f, -148.86f }, { 1204.88f, 55.12f, -148.41f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1204.59f, 55.12f, -148.85f }, { 1204.88f, 55.12f, -148.41f }, { 1204.65f, 54.95f, -148.86f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1205.12f, 55.25f, -147.91f }, { 1204.75f, 55.31f, -148.32f }, { 1205.07f, 55.31f, -147.84f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1204.75f, 55.31f, -148.32f }, { 1205.12f, 55.25f, -147.91f }, { 1204.82f, 55.25f, -148.37f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1204.51f, 55.25f, -148.82f }, { 1204.75f, 55.31f, -148.32f }, { 1204.82f, 55.25f, -148.37f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1204.75f, 55.31f, -148.32f }, { 1204.51f, 55.25f, -148.82f }, { 1204.43f, 55.31f, -148.80f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1205.00f, 55.35f, -147.78f }, { 1204.60f, 55.38f, -148.22f }, { 1204.93f, 55.38f, -147.72f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1204.60f, 55.38f, -148.22f }, { 1205.00f, 55.35f, -147.78f }, { 1204.67f, 55.35f, -148.27f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1204.34f, 55.35f, -148.76f }, { 1204.60f, 55.38f, -148.22f }, { 1204.67f, 55.35f, -148.27f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1204.60f, 55.38f, -148.22f }, { 1204.34f, 55.35f, -148.76f }, { 1204.27f, 55.38f, -148.72f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1204.69f, 54.53f, -148.88f }, { 1204.67f, 54.75f, -148.89f }, { 1204.69f, 54.75f, -148.86f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1204.69f, 54.75f, -148.86f }, { 1204.71f, 54.53f, -148.85f }, { 1204.69f, 54.53f, -148.88f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1204.63f, 54.95f, -148.89f }, { 1204.57f, 55.12f, -148.88f }, { 1204.59f, 55.12f, -148.85f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1204.59f, 55.12f, -148.85f }, { 1204.65f, 54.95f, -148.86f }, { 1204.63f, 54.95f, -148.89f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1204.49f, 55.25f, -148.86f }, { 1204.43f, 55.31f, -148.80f }, { 1204.51f, 55.25f, -148.82f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1204.43f, 55.31f, -148.80f }, { 1204.49f, 55.25f, -148.86f }, { 1204.41f, 55.31f, -148.83f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1204.32f, 55.35f, -148.79f }, { 1204.27f, 55.38f, -148.72f }, { 1204.34f, 55.35f, -148.76f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1204.27f, 55.38f, -148.72f }, { 1204.32f, 55.35f, -148.79f }, { 1204.24f, 55.38f, -148.75f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1205.09f, 55.29f, -147.81f }, { 1205.14f, 55.23f, -147.87f }, { 1205.14f, 55.25f, -147.87f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1205.14f, 55.25f, -147.87f }, { 1205.09f, 55.31f, -147.81f }, { 1205.09f, 55.29f, -147.81f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1204.40f, 55.29f, -148.83f }, { 1204.49f, 55.25f, -148.86f }, { 1204.48f, 55.23f, -148.86f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1204.49f, 55.25f, -148.86f }, { 1204.40f, 55.29f, -148.83f }, { 1204.41f, 55.31f, -148.83f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1205.24f, 54.75f, -148.02f }, { 1205.24f, 54.53f, -148.05f }, { 1205.24f, 54.75f, -148.03f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1205.24f, 54.53f, -148.05f }, { 1205.24f, 54.75f, -148.02f }, { 1205.24f, 54.53f, -148.05f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1204.66f, 54.75f, -148.89f }, { 1204.69f, 54.53f, -148.88f }, { 1204.68f, 54.53f, -148.88f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1204.69f, 54.53f, -148.88f }, { 1204.66f, 54.75f, -148.89f }, { 1204.67f, 54.75f, -148.89f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1205.19f, 55.11f, -147.94f }, { 1205.22f, 54.93f, -147.99f }, { 1205.23f, 54.95f, -147.99f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1205.23f, 54.95f, -147.99f }, { 1205.20f, 55.12f, -147.94f }, { 1205.19f, 55.11f, -147.93f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1204.56f, 55.11f, -148.88f }, { 1204.63f, 54.95f, -148.89f }, { 1204.62f, 54.93f, -148.89f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1204.63f, 54.95f, -148.89f }, { 1204.56f, 55.11f, -148.88f }, { 1204.57f, 55.12f, -148.88f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1204.95f, 55.35f, -147.69f }, { 1205.02f, 55.33f, -147.75f }, { 1205.02f, 55.35f, -147.75f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1205.02f, 55.35f, -147.75f }, { 1204.95f, 55.38f, -147.69f }, { 1204.95f, 55.35f, -147.69f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1204.24f, 55.35f, -148.75f }, { 1204.32f, 55.35f, -148.79f }, { 1204.32f, 55.33f, -148.79f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1204.32f, 55.35f, -148.79f }, { 1204.24f, 55.35f, -148.75f }, { 1204.24f, 55.38f, -148.75f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1205.14f, 55.23f, -147.87f }, { 1205.19f, 55.11f, -147.93f }, { 1205.20f, 55.12f, -147.94f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1205.20f, 55.12f, -147.94f }, { 1205.14f, 55.25f, -147.87f }, { 1205.14f, 55.23f, -147.87f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1205.14f, 55.25f, -147.87f }, { 1205.20f, 55.12f, -147.94f }, { 1205.12f, 55.25f, -147.91f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1205.20f, 55.12f, -147.94f }, { 1205.18f, 55.12f, -147.97f }, { 1205.12f, 55.25f, -147.91f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1204.82f, 55.25f, -148.37f }, { 1205.12f, 55.25f, -147.91f }, { 1205.18f, 55.12f, -147.97f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1204.82f, 55.25f, -148.37f }, { 1205.18f, 55.12f, -147.97f }, { 1204.88f, 55.12f, -148.41f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1204.59f, 55.12f, -148.85f }, { 1204.82f, 55.25f, -148.37f }, { 1204.88f, 55.12f, -148.41f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1204.82f, 55.25f, -148.37f }, { 1204.59f, 55.12f, -148.85f }, { 1204.51f, 55.25f, -148.82f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1204.51f, 55.25f, -148.82f }, { 1204.57f, 55.12f, -148.88f }, { 1204.49f, 55.25f, -148.86f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1204.57f, 55.12f, -148.88f }, { 1204.51f, 55.25f, -148.82f }, { 1204.59f, 55.12f, -148.85f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1204.49f, 55.25f, -148.86f }, { 1204.57f, 55.12f, -148.88f }, { 1204.48f, 55.23f, -148.86f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1204.56f, 55.11f, -148.88f }, { 1204.48f, 55.23f, -148.86f }, { 1204.57f, 55.12f, -148.88f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1205.22f, 54.94f, -147.99f }, { 1205.24f, 54.75f, -148.03f }, { 1205.23f, 54.95f, -147.99f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1205.24f, 54.75f, -148.03f }, { 1205.22f, 54.93f, -147.99f }, { 1205.24f, 54.75f, -148.02f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1205.23f, 54.95f, -147.99f }, { 1205.24f, 54.75f, -148.03f }, { 1205.21f, 54.95f, -148.02f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1205.24f, 54.75f, -148.03f }, { 1205.22f, 54.75f, -148.06f }, { 1205.21f, 54.95f, -148.02f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1204.93f, 54.95f, -148.44f }, { 1205.21f, 54.95f, -148.02f }, { 1205.22f, 54.75f, -148.06f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1204.93f, 54.95f, -148.44f }, { 1205.22f, 54.75f, -148.06f }, { 1204.96f, 54.75f, -148.46f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1204.69f, 54.75f, -148.86f }, { 1204.93f, 54.95f, -148.44f }, { 1204.96f, 54.75f, -148.46f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1204.93f, 54.95f, -148.44f }, { 1204.69f, 54.75f, -148.86f }, { 1204.65f, 54.95f, -148.86f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1204.65f, 54.95f, -148.86f }, { 1204.67f, 54.75f, -148.89f }, { 1204.63f, 54.95f, -148.89f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1204.67f, 54.75f, -148.89f }, { 1204.65f, 54.95f, -148.86f }, { 1204.69f, 54.75f, -148.86f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1204.63f, 54.95f, -148.89f }, { 1204.67f, 54.75f, -148.89f }, { 1204.62f, 54.93f, -148.89f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1204.62f, 54.94f, -148.89f }, { 1204.67f, 54.75f, -148.89f }, { 1204.66f, 54.75f, -148.89f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1205.02f, 55.33f, -147.75f }, { 1205.09f, 55.29f, -147.81f }, { 1205.09f, 55.31f, -147.81f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1205.09f, 55.31f, -147.81f }, { 1205.02f, 55.35f, -147.75f }, { 1205.02f, 55.33f, -147.75f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1205.07f, 55.31f, -147.84f }, { 1205.00f, 55.35f, -147.78f }, { 1205.09f, 55.31f, -147.81f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1205.09f, 55.31f, -147.81f }, { 1205.00f, 55.35f, -147.78f }, { 1205.02f, 55.35f, -147.75f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1205.07f, 55.31f, -147.84f }, { 1204.67f, 55.35f, -148.27f }, { 1205.00f, 55.35f, -147.78f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1205.07f, 55.31f, -147.84f }, { 1204.75f, 55.31f, -148.32f }, { 1204.67f, 55.35f, -148.27f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1204.43f, 55.31f, -148.80f }, { 1204.67f, 55.35f, -148.27f }, { 1204.75f, 55.31f, -148.32f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1204.67f, 55.35f, -148.27f }, { 1204.43f, 55.31f, -148.80f }, { 1204.34f, 55.35f, -148.76f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1204.41f, 55.31f, -148.83f }, { 1204.34f, 55.35f, -148.76f }, { 1204.43f, 55.31f, -148.80f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1204.34f, 55.35f, -148.76f }, { 1204.41f, 55.31f, -148.83f }, { 1204.32f, 55.35f, -148.79f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1204.32f, 55.35f, -148.79f }, { 1204.41f, 55.31f, -148.83f }, { 1204.32f, 55.33f, -148.79f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1204.40f, 55.29f, -148.83f }, { 1204.32f, 55.33f, -148.79f }, { 1204.41f, 55.31f, -148.83f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1204.41f, 55.38f, -147.37f }, { 1204.95f, 55.38f, -147.69f }, { 1204.93f, 55.38f, -147.72f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1204.95f, 55.38f, -147.69f }, { 1204.41f, 55.38f, -147.37f }, { 1204.43f, 55.38f, -147.34f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1204.93f, 55.38f, -147.72f }, { 1204.07f, 55.38f, -147.87f }, { 1204.41f, 55.38f, -147.37f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1204.60f, 55.38f, -148.22f }, { 1204.07f, 55.38f, -147.87f }, { 1204.93f, 55.38f, -147.72f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1204.27f, 55.38f, -148.72f }, { 1204.07f, 55.38f, -147.87f }, { 1204.60f, 55.38f, -148.22f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1204.07f, 55.38f, -147.87f }, { 1204.27f, 55.38f, -148.72f }, { 1203.74f, 55.38f, -148.37f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1204.24f, 55.38f, -148.75f }, { 1203.72f, 55.38f, -148.40f }, { 1204.27f, 55.38f, -148.72f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1204.27f, 55.38f, -148.72f }, { 1203.72f, 55.38f, -148.40f }, { 1203.74f, 55.38f, -148.37f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1204.95f, 55.38f, -147.69f }, { 1204.43f, 55.38f, -147.34f }, { 1204.95f, 55.35f, -147.69f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1204.95f, 55.35f, -147.69f }, { 1204.43f, 55.38f, -147.34f }, { 1204.43f, 55.35f, -147.34f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1204.24f, 55.35f, -148.75f }, { 1203.72f, 55.38f, -148.40f }, { 1204.24f, 55.38f, -148.75f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1203.71f, 55.38f, -148.40f }, { 1204.24f, 55.35f, -148.75f }, { 1203.71f, 55.35f, -148.40f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.78f, 55.12f, -132.11f }, { 1215.81f, 54.95f, -132.16f }, { 1215.75f, 55.12f, -132.14f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.81f, 54.95f, -132.16f }, { 1215.79f, 54.95f, -132.19f }, { 1215.75f, 55.12f, -132.14f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.58f, 55.35f, -131.95f }, { 1215.51f, 55.38f, -131.89f }, { 1215.60f, 55.35f, -131.92f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.60f, 55.35f, -131.92f }, { 1215.51f, 55.38f, -131.89f }, { 1215.53f, 55.38f, -131.86f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.82f, 54.75f, -132.19f }, { 1215.82f, 54.53f, -132.22f }, { 1215.80f, 54.75f, -132.23f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.82f, 54.53f, -132.22f }, { 1215.80f, 54.53f, -132.25f }, { 1215.80f, 54.75f, -132.23f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.70f, 55.25f, -132.07f }, { 1215.65f, 55.31f, -132.01f }, { 1215.72f, 55.25f, -132.04f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.72f, 55.25f, -132.04f }, { 1215.65f, 55.31f, -132.01f }, { 1215.67f, 55.31f, -131.98f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.80f, 54.53f, -132.25f }, { 1215.53f, 54.75f, -132.63f }, { 1215.80f, 54.75f, -132.23f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.53f, 54.75f, -132.63f }, { 1215.80f, 54.53f, -132.25f }, { 1215.55f, 54.53f, -132.63f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.55f, 54.53f, -132.63f }, { 1215.29f, 54.53f, -133.02f }, { 1215.53f, 54.75f, -132.63f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.27f, 54.75f, -133.03f }, { 1215.53f, 54.75f, -132.63f }, { 1215.29f, 54.53f, -133.02f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.79f, 54.95f, -132.19f }, { 1215.46f, 55.12f, -132.58f }, { 1215.75f, 55.12f, -132.14f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.46f, 55.12f, -132.58f }, { 1215.79f, 54.95f, -132.19f }, { 1215.51f, 54.95f, -132.61f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.51f, 54.95f, -132.61f }, { 1215.23f, 54.95f, -133.03f }, { 1215.46f, 55.12f, -132.58f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.17f, 55.12f, -133.02f }, { 1215.46f, 55.12f, -132.58f }, { 1215.23f, 54.95f, -133.03f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.70f, 55.25f, -132.07f }, { 1215.33f, 55.31f, -132.49f }, { 1215.65f, 55.31f, -132.01f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.33f, 55.31f, -132.49f }, { 1215.70f, 55.25f, -132.07f }, { 1215.39f, 55.25f, -132.53f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.09f, 55.25f, -132.99f }, { 1215.33f, 55.31f, -132.49f }, { 1215.39f, 55.25f, -132.53f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.33f, 55.31f, -132.49f }, { 1215.09f, 55.25f, -132.99f }, { 1215.01f, 55.31f, -132.97f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.58f, 55.35f, -131.95f }, { 1215.18f, 55.38f, -132.39f }, { 1215.51f, 55.38f, -131.89f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.18f, 55.38f, -132.39f }, { 1215.58f, 55.35f, -131.95f }, { 1215.25f, 55.35f, -132.44f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1214.92f, 55.35f, -132.93f }, { 1215.18f, 55.38f, -132.39f }, { 1215.25f, 55.35f, -132.44f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.18f, 55.38f, -132.39f }, { 1214.92f, 55.35f, -132.93f }, { 1214.84f, 55.38f, -132.89f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.27f, 54.53f, -133.05f }, { 1215.24f, 54.75f, -133.06f }, { 1215.27f, 54.75f, -133.03f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.27f, 54.75f, -133.03f }, { 1215.29f, 54.53f, -133.02f }, { 1215.27f, 54.53f, -133.05f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.21f, 54.95f, -133.06f }, { 1215.14f, 55.12f, -133.05f }, { 1215.17f, 55.12f, -133.02f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.17f, 55.12f, -133.02f }, { 1215.23f, 54.95f, -133.03f }, { 1215.21f, 54.95f, -133.06f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.07f, 55.25f, -133.03f }, { 1215.01f, 55.31f, -132.97f }, { 1215.09f, 55.25f, -132.99f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.01f, 55.31f, -132.97f }, { 1215.07f, 55.25f, -133.03f }, { 1214.98f, 55.31f, -133.00f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1214.90f, 55.35f, -132.96f }, { 1214.84f, 55.38f, -132.89f }, { 1214.92f, 55.35f, -132.93f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1214.84f, 55.38f, -132.89f }, { 1214.90f, 55.35f, -132.96f }, { 1214.82f, 55.38f, -132.92f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.67f, 55.29f, -131.98f }, { 1215.72f, 55.23f, -132.04f }, { 1215.72f, 55.25f, -132.04f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.72f, 55.25f, -132.04f }, { 1215.67f, 55.31f, -131.98f }, { 1215.67f, 55.29f, -131.98f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1214.98f, 55.29f, -133.00f }, { 1215.07f, 55.25f, -133.03f }, { 1215.06f, 55.23f, -133.02f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.07f, 55.25f, -133.03f }, { 1214.98f, 55.29f, -133.00f }, { 1214.98f, 55.31f, -133.00f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.82f, 54.75f, -132.19f }, { 1215.82f, 54.53f, -132.22f }, { 1215.82f, 54.75f, -132.20f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.82f, 54.53f, -132.22f }, { 1215.82f, 54.75f, -132.19f }, { 1215.81f, 54.53f, -132.21f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.24f, 54.75f, -133.06f }, { 1215.27f, 54.53f, -133.05f }, { 1215.26f, 54.53f, -133.05f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.27f, 54.53f, -133.05f }, { 1215.24f, 54.75f, -133.06f }, { 1215.24f, 54.75f, -133.06f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.77f, 55.11f, -132.10f }, { 1215.80f, 54.93f, -132.16f }, { 1215.81f, 54.95f, -132.16f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.81f, 54.95f, -132.16f }, { 1215.78f, 55.12f, -132.11f }, { 1215.77f, 55.11f, -132.10f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.14f, 55.11f, -133.05f }, { 1215.21f, 54.95f, -133.06f }, { 1215.20f, 54.93f, -133.06f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.21f, 54.95f, -133.06f }, { 1215.14f, 55.11f, -133.05f }, { 1215.14f, 55.12f, -133.05f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.53f, 55.35f, -131.86f }, { 1215.60f, 55.33f, -131.92f }, { 1215.60f, 55.35f, -131.92f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.60f, 55.35f, -131.92f }, { 1215.53f, 55.38f, -131.86f }, { 1215.53f, 55.35f, -131.86f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1214.82f, 55.35f, -132.92f }, { 1214.90f, 55.35f, -132.96f }, { 1214.90f, 55.33f, -132.96f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1214.90f, 55.35f, -132.96f }, { 1214.82f, 55.35f, -132.92f }, { 1214.82f, 55.38f, -132.92f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.72f, 55.23f, -132.04f }, { 1215.77f, 55.11f, -132.10f }, { 1215.78f, 55.12f, -132.11f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.78f, 55.12f, -132.11f }, { 1215.72f, 55.25f, -132.04f }, { 1215.72f, 55.23f, -132.04f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.72f, 55.25f, -132.04f }, { 1215.78f, 55.12f, -132.11f }, { 1215.70f, 55.25f, -132.07f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.78f, 55.12f, -132.11f }, { 1215.75f, 55.12f, -132.14f }, { 1215.70f, 55.25f, -132.07f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.39f, 55.25f, -132.53f }, { 1215.70f, 55.25f, -132.07f }, { 1215.75f, 55.12f, -132.14f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.39f, 55.25f, -132.53f }, { 1215.75f, 55.12f, -132.14f }, { 1215.46f, 55.12f, -132.58f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.17f, 55.12f, -133.02f }, { 1215.39f, 55.25f, -132.53f }, { 1215.46f, 55.12f, -132.58f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.39f, 55.25f, -132.53f }, { 1215.17f, 55.12f, -133.02f }, { 1215.09f, 55.25f, -132.99f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.09f, 55.25f, -132.99f }, { 1215.14f, 55.12f, -133.05f }, { 1215.07f, 55.25f, -133.03f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.14f, 55.12f, -133.05f }, { 1215.09f, 55.25f, -132.99f }, { 1215.17f, 55.12f, -133.02f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.07f, 55.25f, -133.03f }, { 1215.14f, 55.12f, -133.05f }, { 1215.06f, 55.23f, -133.02f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.14f, 55.11f, -133.05f }, { 1215.06f, 55.23f, -133.02f }, { 1215.14f, 55.12f, -133.05f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.80f, 54.94f, -132.16f }, { 1215.82f, 54.75f, -132.20f }, { 1215.81f, 54.95f, -132.16f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.82f, 54.75f, -132.19f }, { 1215.80f, 54.93f, -132.16f }, { 1215.82f, 54.75f, -132.19f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.81f, 54.95f, -132.16f }, { 1215.82f, 54.75f, -132.20f }, { 1215.79f, 54.95f, -132.19f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.82f, 54.75f, -132.19f }, { 1215.80f, 54.75f, -132.23f }, { 1215.79f, 54.95f, -132.19f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.51f, 54.95f, -132.61f }, { 1215.79f, 54.95f, -132.19f }, { 1215.80f, 54.75f, -132.23f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.51f, 54.95f, -132.61f }, { 1215.80f, 54.75f, -132.23f }, { 1215.53f, 54.75f, -132.63f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.27f, 54.75f, -133.03f }, { 1215.51f, 54.95f, -132.61f }, { 1215.53f, 54.75f, -132.63f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.51f, 54.95f, -132.61f }, { 1215.27f, 54.75f, -133.03f }, { 1215.23f, 54.95f, -133.03f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.23f, 54.95f, -133.03f }, { 1215.24f, 54.75f, -133.06f }, { 1215.21f, 54.95f, -133.06f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.24f, 54.75f, -133.06f }, { 1215.23f, 54.95f, -133.03f }, { 1215.27f, 54.75f, -133.03f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.21f, 54.95f, -133.06f }, { 1215.24f, 54.75f, -133.06f }, { 1215.20f, 54.93f, -133.06f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.20f, 54.94f, -133.06f }, { 1215.24f, 54.75f, -133.06f }, { 1215.24f, 54.75f, -133.06f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.60f, 55.33f, -131.91f }, { 1215.67f, 55.29f, -131.98f }, { 1215.67f, 55.31f, -131.98f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.67f, 55.31f, -131.98f }, { 1215.60f, 55.35f, -131.92f }, { 1215.60f, 55.33f, -131.92f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.65f, 55.31f, -132.01f }, { 1215.58f, 55.35f, -131.95f }, { 1215.67f, 55.31f, -131.98f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.67f, 55.31f, -131.98f }, { 1215.58f, 55.35f, -131.95f }, { 1215.60f, 55.35f, -131.92f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.65f, 55.31f, -132.01f }, { 1215.25f, 55.35f, -132.44f }, { 1215.58f, 55.35f, -131.95f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.65f, 55.31f, -132.01f }, { 1215.33f, 55.31f, -132.49f }, { 1215.25f, 55.35f, -132.44f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.01f, 55.31f, -132.97f }, { 1215.25f, 55.35f, -132.44f }, { 1215.33f, 55.31f, -132.49f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.25f, 55.35f, -132.44f }, { 1215.01f, 55.31f, -132.97f }, { 1214.92f, 55.35f, -132.93f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1214.98f, 55.31f, -133.00f }, { 1214.92f, 55.35f, -132.93f }, { 1215.01f, 55.31f, -132.97f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1214.92f, 55.35f, -132.93f }, { 1214.98f, 55.31f, -133.00f }, { 1214.90f, 55.35f, -132.96f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1214.90f, 55.35f, -132.96f }, { 1214.98f, 55.31f, -133.00f }, { 1214.90f, 55.33f, -132.96f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1214.98f, 55.29f, -133.00f }, { 1214.90f, 55.33f, -132.96f }, { 1214.98f, 55.31f, -133.00f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1214.98f, 55.38f, -131.54f }, { 1215.53f, 55.38f, -131.86f }, { 1215.51f, 55.38f, -131.89f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.53f, 55.38f, -131.86f }, { 1214.98f, 55.38f, -131.54f }, { 1215.00f, 55.38f, -131.50f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.51f, 55.38f, -131.89f }, { 1214.65f, 55.38f, -132.04f }, { 1214.98f, 55.38f, -131.54f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.18f, 55.38f, -132.39f }, { 1214.65f, 55.38f, -132.04f }, { 1215.51f, 55.38f, -131.89f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1214.84f, 55.38f, -132.89f }, { 1214.65f, 55.38f, -132.04f }, { 1215.18f, 55.38f, -132.39f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1214.65f, 55.38f, -132.04f }, { 1214.84f, 55.38f, -132.89f }, { 1214.32f, 55.38f, -132.54f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1214.82f, 55.38f, -132.92f }, { 1214.29f, 55.38f, -132.57f }, { 1214.84f, 55.38f, -132.89f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1214.84f, 55.38f, -132.89f }, { 1214.29f, 55.38f, -132.57f }, { 1214.32f, 55.38f, -132.54f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.53f, 55.38f, -131.86f }, { 1215.00f, 55.38f, -131.50f }, { 1215.53f, 55.35f, -131.86f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1215.53f, 55.35f, -131.86f }, { 1215.00f, 55.38f, -131.50f }, { 1215.00f, 55.35f, -131.50f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1214.82f, 55.35f, -132.92f }, { 1214.29f, 55.38f, -132.57f }, { 1214.82f, 55.38f, -132.92f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1214.29f, 55.38f, -132.57f }, { 1214.82f, 55.35f, -132.92f }, { 1214.29f, 55.35f, -132.57f }, 54.53f, 0.00f },   // waterfeature_02 props 655
    { { 1155.37f, 47.56f, -166.17f }, { 1155.34f, 47.40f, -166.12f }, { 1155.39f, 47.56f, -166.14f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.34f, 47.40f, -166.12f }, { 1155.36f, 47.40f, -166.09f }, { 1155.39f, 47.56f, -166.14f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.55f, 47.76f, -166.31f }, { 1155.61f, 47.79f, -166.36f }, { 1155.53f, 47.76f, -166.34f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.53f, 47.76f, -166.34f }, { 1155.61f, 47.79f, -166.36f }, { 1155.59f, 47.79f, -166.39f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.33f, 47.22f, -166.09f }, { 1155.33f, 47.02f, -166.06f }, { 1155.35f, 47.22f, -166.06f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.33f, 47.02f, -166.06f }, { 1155.35f, 47.02f, -166.04f }, { 1155.35f, 47.22f, -166.06f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.44f, 47.67f, -166.20f }, { 1155.48f, 47.73f, -166.25f }, { 1155.42f, 47.67f, -166.22f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.42f, 47.67f, -166.22f }, { 1155.48f, 47.73f, -166.25f }, { 1155.47f, 47.73f, -166.28f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.35f, 47.02f, -166.04f }, { 1155.59f, 47.22f, -165.70f }, { 1155.35f, 47.22f, -166.06f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.59f, 47.22f, -165.70f }, { 1155.35f, 47.02f, -166.04f }, { 1155.58f, 47.02f, -165.69f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.58f, 47.02f, -165.69f }, { 1155.81f, 47.02f, -165.35f }, { 1155.59f, 47.22f, -165.70f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.83f, 47.22f, -165.34f }, { 1155.59f, 47.22f, -165.70f }, { 1155.81f, 47.02f, -165.35f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.36f, 47.40f, -166.09f }, { 1155.65f, 47.56f, -165.74f }, { 1155.39f, 47.56f, -166.14f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.65f, 47.56f, -165.74f }, { 1155.36f, 47.40f, -166.09f }, { 1155.61f, 47.40f, -165.71f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.61f, 47.40f, -165.71f }, { 1155.86f, 47.40f, -165.34f }, { 1155.65f, 47.56f, -165.74f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.92f, 47.56f, -165.35f }, { 1155.65f, 47.56f, -165.74f }, { 1155.86f, 47.40f, -165.34f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.44f, 47.67f, -166.20f }, { 1155.77f, 47.73f, -165.82f }, { 1155.48f, 47.73f, -166.25f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.77f, 47.73f, -165.82f }, { 1155.44f, 47.67f, -166.20f }, { 1155.71f, 47.67f, -165.78f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.99f, 47.67f, -165.37f }, { 1155.77f, 47.73f, -165.82f }, { 1155.71f, 47.67f, -165.78f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.77f, 47.73f, -165.82f }, { 1155.99f, 47.67f, -165.37f }, { 1156.06f, 47.73f, -165.39f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.55f, 47.76f, -166.31f }, { 1155.91f, 47.79f, -165.91f }, { 1155.61f, 47.79f, -166.36f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.91f, 47.78f, -165.91f }, { 1155.55f, 47.76f, -166.31f }, { 1155.84f, 47.76f, -165.87f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.14f, 47.76f, -165.43f }, { 1155.91f, 47.79f, -165.91f }, { 1155.84f, 47.76f, -165.87f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.91f, 47.78f, -165.91f }, { 1156.14f, 47.76f, -165.43f }, { 1156.21f, 47.79f, -165.46f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.83f, 47.02f, -165.32f }, { 1155.85f, 47.22f, -165.31f }, { 1155.83f, 47.22f, -165.34f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.83f, 47.22f, -165.34f }, { 1155.81f, 47.02f, -165.35f }, { 1155.83f, 47.02f, -165.32f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.88f, 47.40f, -165.31f }, { 1155.94f, 47.56f, -165.32f }, { 1155.92f, 47.56f, -165.35f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.92f, 47.56f, -165.35f }, { 1155.86f, 47.40f, -165.34f }, { 1155.88f, 47.40f, -165.31f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.01f, 47.67f, -165.34f }, { 1156.06f, 47.73f, -165.39f }, { 1155.99f, 47.67f, -165.37f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.06f, 47.73f, -165.39f }, { 1156.01f, 47.67f, -165.34f }, { 1156.08f, 47.73f, -165.36f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.16f, 47.76f, -165.40f }, { 1156.21f, 47.79f, -165.46f }, { 1156.14f, 47.76f, -165.43f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.21f, 47.78f, -165.46f }, { 1156.16f, 47.76f, -165.40f }, { 1156.23f, 47.79f, -165.43f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.47f, 47.71f, -166.28f }, { 1155.42f, 47.65f, -166.23f }, { 1155.42f, 47.67f, -166.22f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.42f, 47.67f, -166.22f }, { 1155.47f, 47.73f, -166.28f }, { 1155.47f, 47.71f, -166.28f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.08f, 47.71f, -165.36f }, { 1156.01f, 47.67f, -165.34f }, { 1156.01f, 47.65f, -165.34f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.01f, 47.67f, -165.34f }, { 1156.08f, 47.71f, -165.36f }, { 1156.08f, 47.73f, -165.36f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.34f, 47.22f, -166.09f }, { 1155.33f, 47.02f, -166.06f }, { 1155.33f, 47.22f, -166.09f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.33f, 47.02f, -166.06f }, { 1155.33f, 47.22f, -166.09f }, { 1155.34f, 47.02f, -166.07f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.86f, 47.22f, -165.31f }, { 1155.83f, 47.02f, -165.32f }, { 1155.84f, 47.02f, -165.32f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.83f, 47.02f, -165.32f }, { 1155.86f, 47.22f, -165.31f }, { 1155.85f, 47.22f, -165.31f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.38f, 47.54f, -166.17f }, { 1155.35f, 47.39f, -166.12f }, { 1155.34f, 47.40f, -166.12f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.34f, 47.40f, -166.12f }, { 1155.37f, 47.56f, -166.17f }, { 1155.38f, 47.54f, -166.17f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.94f, 47.54f, -165.32f }, { 1155.88f, 47.40f, -165.31f }, { 1155.89f, 47.39f, -165.31f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.88f, 47.40f, -165.31f }, { 1155.94f, 47.54f, -165.32f }, { 1155.94f, 47.56f, -165.32f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.59f, 47.76f, -166.39f }, { 1155.53f, 47.74f, -166.34f }, { 1155.53f, 47.76f, -166.34f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.53f, 47.76f, -166.34f }, { 1155.59f, 47.79f, -166.39f }, { 1155.59f, 47.76f, -166.39f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.23f, 47.76f, -165.43f }, { 1156.16f, 47.76f, -165.40f }, { 1156.16f, 47.74f, -165.40f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.16f, 47.76f, -165.40f }, { 1156.23f, 47.76f, -165.43f }, { 1156.23f, 47.79f, -165.43f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.42f, 47.65f, -166.23f }, { 1155.38f, 47.54f, -166.17f }, { 1155.37f, 47.56f, -166.17f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.37f, 47.56f, -166.17f }, { 1155.42f, 47.67f, -166.22f }, { 1155.42f, 47.65f, -166.23f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.42f, 47.67f, -166.22f }, { 1155.37f, 47.56f, -166.17f }, { 1155.44f, 47.67f, -166.20f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.37f, 47.56f, -166.17f }, { 1155.39f, 47.56f, -166.14f }, { 1155.44f, 47.67f, -166.20f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.71f, 47.67f, -165.78f }, { 1155.44f, 47.67f, -166.20f }, { 1155.39f, 47.56f, -166.14f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.71f, 47.67f, -165.78f }, { 1155.39f, 47.56f, -166.14f }, { 1155.65f, 47.56f, -165.74f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.92f, 47.56f, -165.35f }, { 1155.71f, 47.67f, -165.78f }, { 1155.65f, 47.56f, -165.74f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.71f, 47.67f, -165.78f }, { 1155.92f, 47.56f, -165.35f }, { 1155.99f, 47.67f, -165.37f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.99f, 47.67f, -165.37f }, { 1155.94f, 47.56f, -165.32f }, { 1156.01f, 47.67f, -165.34f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.94f, 47.56f, -165.32f }, { 1155.99f, 47.67f, -165.37f }, { 1155.92f, 47.56f, -165.35f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.01f, 47.67f, -165.34f }, { 1155.94f, 47.56f, -165.32f }, { 1156.01f, 47.65f, -165.34f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.94f, 47.54f, -165.32f }, { 1156.01f, 47.65f, -165.34f }, { 1155.94f, 47.56f, -165.32f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.35f, 47.39f, -166.12f }, { 1155.33f, 47.22f, -166.09f }, { 1155.34f, 47.40f, -166.12f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.33f, 47.22f, -166.09f }, { 1155.35f, 47.39f, -166.12f }, { 1155.33f, 47.22f, -166.09f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.34f, 47.40f, -166.12f }, { 1155.33f, 47.22f, -166.09f }, { 1155.36f, 47.40f, -166.09f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.33f, 47.22f, -166.09f }, { 1155.35f, 47.22f, -166.06f }, { 1155.36f, 47.40f, -166.09f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.61f, 47.40f, -165.71f }, { 1155.36f, 47.40f, -166.09f }, { 1155.35f, 47.22f, -166.06f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.61f, 47.40f, -165.71f }, { 1155.35f, 47.22f, -166.06f }, { 1155.59f, 47.22f, -165.70f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.83f, 47.22f, -165.34f }, { 1155.61f, 47.40f, -165.71f }, { 1155.59f, 47.22f, -165.70f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.61f, 47.40f, -165.71f }, { 1155.83f, 47.22f, -165.34f }, { 1155.86f, 47.40f, -165.34f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.86f, 47.40f, -165.34f }, { 1155.85f, 47.22f, -165.31f }, { 1155.88f, 47.40f, -165.31f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.85f, 47.22f, -165.31f }, { 1155.86f, 47.40f, -165.34f }, { 1155.83f, 47.22f, -165.34f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.88f, 47.40f, -165.31f }, { 1155.85f, 47.22f, -165.31f }, { 1155.89f, 47.39f, -165.31f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.89f, 47.39f, -165.31f }, { 1155.85f, 47.22f, -165.31f }, { 1155.86f, 47.22f, -165.31f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.53f, 47.74f, -166.34f }, { 1155.47f, 47.71f, -166.28f }, { 1155.47f, 47.73f, -166.28f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.47f, 47.73f, -166.28f }, { 1155.53f, 47.76f, -166.34f }, { 1155.53f, 47.74f, -166.34f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.48f, 47.73f, -166.25f }, { 1155.55f, 47.76f, -166.31f }, { 1155.47f, 47.73f, -166.28f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.47f, 47.73f, -166.28f }, { 1155.55f, 47.76f, -166.31f }, { 1155.53f, 47.76f, -166.34f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.48f, 47.73f, -166.25f }, { 1155.84f, 47.76f, -165.87f }, { 1155.55f, 47.76f, -166.31f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.48f, 47.73f, -166.25f }, { 1155.77f, 47.73f, -165.82f }, { 1155.84f, 47.76f, -165.87f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.06f, 47.73f, -165.39f }, { 1155.84f, 47.76f, -165.87f }, { 1155.77f, 47.73f, -165.82f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.84f, 47.76f, -165.87f }, { 1156.06f, 47.73f, -165.39f }, { 1156.14f, 47.76f, -165.43f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.08f, 47.73f, -165.36f }, { 1156.14f, 47.76f, -165.43f }, { 1156.06f, 47.73f, -165.39f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.14f, 47.76f, -165.43f }, { 1156.08f, 47.73f, -165.36f }, { 1156.16f, 47.76f, -165.40f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.16f, 47.76f, -165.40f }, { 1156.08f, 47.73f, -165.36f }, { 1156.16f, 47.74f, -165.40f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.08f, 47.71f, -165.36f }, { 1156.16f, 47.74f, -165.40f }, { 1156.08f, 47.73f, -165.36f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.08f, 47.78f, -166.68f }, { 1155.59f, 47.79f, -166.39f }, { 1155.61f, 47.79f, -166.36f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.59f, 47.78f, -166.39f }, { 1156.08f, 47.79f, -166.68f }, { 1156.06f, 47.79f, -166.71f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.61f, 47.78f, -166.36f }, { 1156.38f, 47.79f, -166.23f }, { 1156.08f, 47.79f, -166.68f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.91f, 47.78f, -165.91f }, { 1156.38f, 47.79f, -166.23f }, { 1155.61f, 47.79f, -166.36f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.21f, 47.78f, -165.46f }, { 1156.38f, 47.79f, -166.23f }, { 1155.91f, 47.79f, -165.91f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.38f, 47.78f, -166.23f }, { 1156.21f, 47.79f, -165.46f }, { 1156.68f, 47.79f, -165.78f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.23f, 47.78f, -165.43f }, { 1156.70f, 47.79f, -165.75f }, { 1156.21f, 47.79f, -165.46f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.21f, 47.78f, -165.46f }, { 1156.70f, 47.79f, -165.75f }, { 1156.68f, 47.79f, -165.78f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.59f, 47.78f, -166.39f }, { 1156.06f, 47.79f, -166.71f }, { 1155.59f, 47.76f, -166.39f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1155.59f, 47.76f, -166.39f }, { 1156.06f, 47.79f, -166.71f }, { 1156.07f, 47.76f, -166.71f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.23f, 47.76f, -165.43f }, { 1156.70f, 47.79f, -165.75f }, { 1156.23f, 47.79f, -165.43f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1156.70f, 47.78f, -165.75f }, { 1156.23f, 47.76f, -165.43f }, { 1156.71f, 47.76f, -165.75f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1160.71f, 47.56f, -164.60f }, { 1160.76f, 47.40f, -164.57f }, { 1160.74f, 47.56f, -164.62f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1160.76f, 47.40f, -164.57f }, { 1160.79f, 47.40f, -164.59f }, { 1160.74f, 47.56f, -164.62f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1160.57f, 47.76f, -164.78f }, { 1160.52f, 47.79f, -164.83f }, { 1160.54f, 47.76f, -164.76f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1160.54f, 47.76f, -164.76f }, { 1160.52f, 47.79f, -164.83f }, { 1160.49f, 47.79f, -164.82f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1160.79f, 47.22f, -164.56f }, { 1160.82f, 47.02f, -164.56f }, { 1160.82f, 47.22f, -164.57f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1160.82f, 47.02f, -164.56f }, { 1160.84f, 47.02f, -164.57f }, { 1160.82f, 47.22f, -164.57f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1160.68f, 47.67f, -164.66f }, { 1160.63f, 47.73f, -164.71f }, { 1160.66f, 47.67f, -164.65f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1160.66f, 47.67f, -164.65f }, { 1160.63f, 47.73f, -164.71f }, { 1160.60f, 47.73f, -164.69f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1160.84f, 47.02f, -164.57f }, { 1161.18f, 47.22f, -164.81f }, { 1160.82f, 47.22f, -164.57f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.18f, 47.22f, -164.81f }, { 1160.84f, 47.02f, -164.57f }, { 1161.19f, 47.02f, -164.80f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.19f, 47.02f, -164.81f }, { 1161.53f, 47.02f, -165.04f }, { 1161.18f, 47.22f, -164.81f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.54f, 47.22f, -165.06f }, { 1161.18f, 47.22f, -164.81f }, { 1161.53f, 47.02f, -165.04f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1160.79f, 47.40f, -164.59f }, { 1161.14f, 47.56f, -164.88f }, { 1160.74f, 47.56f, -164.62f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.14f, 47.56f, -164.88f }, { 1160.79f, 47.40f, -164.59f }, { 1161.17f, 47.40f, -164.84f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.17f, 47.40f, -164.84f }, { 1161.54f, 47.40f, -165.09f }, { 1161.14f, 47.56f, -164.88f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.53f, 47.56f, -165.15f }, { 1161.14f, 47.56f, -164.88f }, { 1161.54f, 47.40f, -165.09f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1160.68f, 47.67f, -164.66f }, { 1161.06f, 47.73f, -165.00f }, { 1160.63f, 47.73f, -164.71f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.06f, 47.73f, -165.00f }, { 1160.68f, 47.67f, -164.66f }, { 1161.10f, 47.67f, -164.94f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.51f, 47.67f, -165.22f }, { 1161.06f, 47.73f, -165.00f }, { 1161.10f, 47.67f, -164.94f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.06f, 47.73f, -165.00f }, { 1161.51f, 47.67f, -165.22f }, { 1161.49f, 47.73f, -165.29f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1160.57f, 47.76f, -164.78f }, { 1160.97f, 47.79f, -165.14f }, { 1160.52f, 47.79f, -164.83f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1160.97f, 47.78f, -165.13f }, { 1160.57f, 47.76f, -164.78f }, { 1161.01f, 47.76f, -165.07f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.45f, 47.76f, -165.37f }, { 1160.97f, 47.79f, -165.14f }, { 1161.01f, 47.76f, -165.07f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1160.97f, 47.78f, -165.13f }, { 1161.45f, 47.76f, -165.37f }, { 1161.42f, 47.79f, -165.44f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.56f, 47.02f, -165.06f }, { 1161.57f, 47.22f, -165.08f }, { 1161.54f, 47.22f, -165.06f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.54f, 47.22f, -165.06f }, { 1161.53f, 47.02f, -165.04f }, { 1161.56f, 47.02f, -165.06f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.57f, 47.40f, -165.11f }, { 1161.56f, 47.56f, -165.17f }, { 1161.53f, 47.56f, -165.15f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.53f, 47.56f, -165.15f }, { 1161.54f, 47.40f, -165.09f }, { 1161.57f, 47.40f, -165.11f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.54f, 47.67f, -165.24f }, { 1161.49f, 47.73f, -165.29f }, { 1161.51f, 47.67f, -165.22f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.49f, 47.73f, -165.29f }, { 1161.54f, 47.67f, -165.24f }, { 1161.52f, 47.73f, -165.31f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.48f, 47.76f, -165.38f }, { 1161.42f, 47.79f, -165.44f }, { 1161.45f, 47.76f, -165.37f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.42f, 47.78f, -165.44f }, { 1161.48f, 47.76f, -165.38f }, { 1161.45f, 47.79f, -165.46f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1160.60f, 47.71f, -164.69f }, { 1160.65f, 47.65f, -164.65f }, { 1160.66f, 47.67f, -164.65f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1160.66f, 47.67f, -164.65f }, { 1160.60f, 47.73f, -164.69f }, { 1160.60f, 47.71f, -164.70f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.52f, 47.71f, -165.31f }, { 1161.54f, 47.67f, -165.24f }, { 1161.54f, 47.65f, -165.24f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.54f, 47.67f, -165.24f }, { 1161.52f, 47.71f, -165.31f }, { 1161.52f, 47.73f, -165.31f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1160.79f, 47.22f, -164.56f }, { 1160.82f, 47.02f, -164.56f }, { 1160.79f, 47.22f, -164.56f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1160.82f, 47.02f, -164.56f }, { 1160.79f, 47.22f, -164.56f }, { 1160.81f, 47.02f, -164.56f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.57f, 47.22f, -165.08f }, { 1161.56f, 47.02f, -165.06f }, { 1161.56f, 47.02f, -165.06f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.56f, 47.02f, -165.06f }, { 1161.57f, 47.22f, -165.08f }, { 1161.57f, 47.22f, -165.08f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1160.71f, 47.54f, -164.60f }, { 1160.76f, 47.39f, -164.57f }, { 1160.76f, 47.40f, -164.57f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1160.76f, 47.40f, -164.57f }, { 1160.71f, 47.56f, -164.60f }, { 1160.71f, 47.54f, -164.60f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.56f, 47.54f, -165.17f }, { 1161.57f, 47.40f, -165.11f }, { 1161.57f, 47.39f, -165.12f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.57f, 47.40f, -165.11f }, { 1161.56f, 47.54f, -165.17f }, { 1161.56f, 47.56f, -165.17f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1160.49f, 47.76f, -164.82f }, { 1160.54f, 47.74f, -164.76f }, { 1160.54f, 47.76f, -164.76f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1160.54f, 47.76f, -164.76f }, { 1160.49f, 47.79f, -164.82f }, { 1160.49f, 47.76f, -164.82f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.45f, 47.76f, -165.46f }, { 1161.48f, 47.76f, -165.38f }, { 1161.48f, 47.74f, -165.39f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.48f, 47.76f, -165.38f }, { 1161.45f, 47.76f, -165.46f }, { 1161.45f, 47.79f, -165.46f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1160.65f, 47.65f, -164.65f }, { 1160.71f, 47.54f, -164.60f }, { 1160.71f, 47.56f, -164.60f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1160.71f, 47.56f, -164.60f }, { 1160.66f, 47.67f, -164.65f }, { 1160.65f, 47.65f, -164.65f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1160.66f, 47.67f, -164.65f }, { 1160.71f, 47.56f, -164.60f }, { 1160.68f, 47.67f, -164.66f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1160.71f, 47.56f, -164.60f }, { 1160.74f, 47.56f, -164.62f }, { 1160.68f, 47.67f, -164.66f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.10f, 47.67f, -164.94f }, { 1160.68f, 47.67f, -164.66f }, { 1160.74f, 47.56f, -164.62f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.10f, 47.67f, -164.94f }, { 1160.74f, 47.56f, -164.62f }, { 1161.14f, 47.56f, -164.88f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.53f, 47.56f, -165.15f }, { 1161.10f, 47.67f, -164.94f }, { 1161.14f, 47.56f, -164.88f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.10f, 47.67f, -164.94f }, { 1161.53f, 47.56f, -165.15f }, { 1161.51f, 47.67f, -165.22f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.51f, 47.67f, -165.22f }, { 1161.56f, 47.56f, -165.17f }, { 1161.54f, 47.67f, -165.24f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.56f, 47.56f, -165.16f }, { 1161.51f, 47.67f, -165.22f }, { 1161.53f, 47.56f, -165.15f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.54f, 47.67f, -165.24f }, { 1161.56f, 47.56f, -165.17f }, { 1161.54f, 47.65f, -165.24f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.56f, 47.54f, -165.17f }, { 1161.54f, 47.65f, -165.24f }, { 1161.56f, 47.56f, -165.17f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1160.76f, 47.39f, -164.57f }, { 1160.79f, 47.22f, -164.56f }, { 1160.76f, 47.40f, -164.57f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1160.79f, 47.22f, -164.56f }, { 1160.76f, 47.39f, -164.57f }, { 1160.79f, 47.22f, -164.56f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1160.76f, 47.40f, -164.57f }, { 1160.79f, 47.22f, -164.56f }, { 1160.79f, 47.40f, -164.59f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1160.79f, 47.22f, -164.56f }, { 1160.82f, 47.22f, -164.57f }, { 1160.79f, 47.40f, -164.59f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.17f, 47.40f, -164.84f }, { 1160.79f, 47.40f, -164.59f }, { 1160.82f, 47.22f, -164.57f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.17f, 47.40f, -164.84f }, { 1160.82f, 47.22f, -164.57f }, { 1161.18f, 47.22f, -164.81f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.54f, 47.22f, -165.06f }, { 1161.17f, 47.40f, -164.84f }, { 1161.18f, 47.22f, -164.81f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.17f, 47.40f, -164.84f }, { 1161.54f, 47.22f, -165.06f }, { 1161.54f, 47.40f, -165.09f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.54f, 47.40f, -165.09f }, { 1161.57f, 47.22f, -165.08f }, { 1161.57f, 47.40f, -165.11f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.57f, 47.22f, -165.08f }, { 1161.54f, 47.40f, -165.09f }, { 1161.54f, 47.22f, -165.06f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.57f, 47.40f, -165.11f }, { 1161.57f, 47.22f, -165.08f }, { 1161.57f, 47.39f, -165.12f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.57f, 47.39f, -165.12f }, { 1161.57f, 47.22f, -165.08f }, { 1161.57f, 47.22f, -165.08f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1160.54f, 47.74f, -164.76f }, { 1160.60f, 47.71f, -164.70f }, { 1160.60f, 47.73f, -164.69f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1160.60f, 47.73f, -164.69f }, { 1160.54f, 47.76f, -164.76f }, { 1160.54f, 47.74f, -164.76f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1160.62f, 47.73f, -164.71f }, { 1160.57f, 47.76f, -164.78f }, { 1160.60f, 47.73f, -164.69f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1160.60f, 47.73f, -164.69f }, { 1160.57f, 47.76f, -164.78f }, { 1160.54f, 47.76f, -164.76f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1160.62f, 47.73f, -164.71f }, { 1161.01f, 47.76f, -165.07f }, { 1160.57f, 47.76f, -164.78f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1160.62f, 47.73f, -164.71f }, { 1161.06f, 47.73f, -165.00f }, { 1161.01f, 47.76f, -165.07f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.49f, 47.73f, -165.29f }, { 1161.01f, 47.76f, -165.07f }, { 1161.06f, 47.73f, -165.00f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.01f, 47.76f, -165.07f }, { 1161.49f, 47.73f, -165.29f }, { 1161.45f, 47.76f, -165.37f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.52f, 47.73f, -165.31f }, { 1161.45f, 47.76f, -165.37f }, { 1161.49f, 47.73f, -165.29f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.45f, 47.76f, -165.37f }, { 1161.52f, 47.73f, -165.31f }, { 1161.48f, 47.76f, -165.38f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.48f, 47.76f, -165.38f }, { 1161.52f, 47.73f, -165.31f }, { 1161.48f, 47.74f, -165.39f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.52f, 47.71f, -165.31f }, { 1161.48f, 47.74f, -165.39f }, { 1161.52f, 47.73f, -165.31f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1160.20f, 47.78f, -165.31f }, { 1160.49f, 47.79f, -164.82f }, { 1160.52f, 47.79f, -164.83f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1160.49f, 47.78f, -164.82f }, { 1160.20f, 47.79f, -165.31f }, { 1160.17f, 47.79f, -165.29f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1160.52f, 47.78f, -164.83f }, { 1160.65f, 47.79f, -165.61f }, { 1160.20f, 47.79f, -165.31f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1160.97f, 47.78f, -165.13f }, { 1160.65f, 47.79f, -165.61f }, { 1160.52f, 47.79f, -164.83f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.42f, 47.78f, -165.44f }, { 1160.65f, 47.79f, -165.61f }, { 1160.97f, 47.79f, -165.14f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1160.65f, 47.78f, -165.61f }, { 1161.42f, 47.79f, -165.44f }, { 1161.10f, 47.79f, -165.91f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.45f, 47.78f, -165.46f }, { 1161.13f, 47.79f, -165.93f }, { 1161.42f, 47.79f, -165.44f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.42f, 47.78f, -165.44f }, { 1161.13f, 47.79f, -165.93f }, { 1161.10f, 47.79f, -165.91f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1160.49f, 47.78f, -164.82f }, { 1160.17f, 47.79f, -165.29f }, { 1160.49f, 47.76f, -164.82f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1160.49f, 47.76f, -164.82f }, { 1160.17f, 47.79f, -165.29f }, { 1160.17f, 47.76f, -165.29f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.45f, 47.76f, -165.46f }, { 1161.13f, 47.79f, -165.93f }, { 1161.45f, 47.79f, -165.46f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.13f, 47.78f, -165.93f }, { 1161.45f, 47.76f, -165.46f }, { 1161.13f, 47.76f, -165.93f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.73f, 47.56f, -169.85f }, { 1162.76f, 47.40f, -169.90f }, { 1162.71f, 47.56f, -169.88f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.76f, 47.40f, -169.90f }, { 1162.74f, 47.40f, -169.93f }, { 1162.71f, 47.56f, -169.88f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.55f, 47.76f, -169.71f }, { 1162.49f, 47.79f, -169.66f }, { 1162.57f, 47.76f, -169.68f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.57f, 47.76f, -169.68f }, { 1162.49f, 47.79f, -169.66f }, { 1162.51f, 47.79f, -169.63f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.77f, 47.22f, -169.93f }, { 1162.77f, 47.02f, -169.95f }, { 1162.75f, 47.22f, -169.96f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.77f, 47.02f, -169.96f }, { 1162.75f, 47.02f, -169.98f }, { 1162.75f, 47.22f, -169.96f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.66f, 47.67f, -169.82f }, { 1162.61f, 47.73f, -169.76f }, { 1162.68f, 47.67f, -169.80f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.68f, 47.67f, -169.79f }, { 1162.61f, 47.73f, -169.76f }, { 1162.63f, 47.73f, -169.74f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.75f, 47.02f, -169.98f }, { 1162.51f, 47.22f, -170.32f }, { 1162.75f, 47.22f, -169.96f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.51f, 47.22f, -170.32f }, { 1162.75f, 47.02f, -169.98f }, { 1162.52f, 47.02f, -170.33f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.52f, 47.02f, -170.33f }, { 1162.29f, 47.02f, -170.67f }, { 1162.51f, 47.22f, -170.32f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.27f, 47.22f, -170.68f }, { 1162.51f, 47.22f, -170.32f }, { 1162.29f, 47.02f, -170.67f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.74f, 47.40f, -169.93f }, { 1162.44f, 47.56f, -170.28f }, { 1162.71f, 47.56f, -169.88f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.44f, 47.56f, -170.28f }, { 1162.74f, 47.40f, -169.93f }, { 1162.49f, 47.40f, -170.31f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.49f, 47.40f, -170.31f }, { 1162.23f, 47.40f, -170.68f }, { 1162.44f, 47.56f, -170.28f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.18f, 47.56f, -170.67f }, { 1162.44f, 47.56f, -170.28f }, { 1162.23f, 47.40f, -170.68f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.66f, 47.67f, -169.82f }, { 1162.32f, 47.73f, -170.20f }, { 1162.61f, 47.73f, -169.76f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.32f, 47.73f, -170.20f }, { 1162.66f, 47.67f, -169.82f }, { 1162.38f, 47.67f, -170.24f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.11f, 47.67f, -170.65f }, { 1162.32f, 47.73f, -170.20f }, { 1162.38f, 47.67f, -170.24f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.32f, 47.73f, -170.20f }, { 1162.11f, 47.67f, -170.65f }, { 1162.03f, 47.73f, -170.63f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.55f, 47.76f, -169.71f }, { 1162.19f, 47.79f, -170.11f }, { 1162.49f, 47.79f, -169.66f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.19f, 47.78f, -170.11f }, { 1162.55f, 47.76f, -169.71f }, { 1162.25f, 47.76f, -170.15f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.96f, 47.76f, -170.59f }, { 1162.19f, 47.79f, -170.11f }, { 1162.25f, 47.76f, -170.15f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.19f, 47.78f, -170.11f }, { 1161.96f, 47.76f, -170.59f }, { 1161.89f, 47.79f, -170.56f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.27f, 47.02f, -170.70f }, { 1162.25f, 47.22f, -170.71f }, { 1162.27f, 47.22f, -170.68f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.27f, 47.22f, -170.68f }, { 1162.29f, 47.02f, -170.67f }, { 1162.27f, 47.02f, -170.70f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.21f, 47.40f, -170.71f }, { 1162.16f, 47.56f, -170.70f }, { 1162.18f, 47.56f, -170.67f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.18f, 47.56f, -170.67f }, { 1162.23f, 47.40f, -170.68f }, { 1162.21f, 47.40f, -170.71f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.09f, 47.67f, -170.68f }, { 1162.03f, 47.73f, -170.63f }, { 1162.11f, 47.67f, -170.65f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.03f, 47.73f, -170.63f }, { 1162.09f, 47.67f, -170.68f }, { 1162.01f, 47.73f, -170.66f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.94f, 47.76f, -170.62f }, { 1161.89f, 47.79f, -170.56f }, { 1161.96f, 47.76f, -170.59f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.89f, 47.78f, -170.56f }, { 1161.94f, 47.76f, -170.62f }, { 1161.87f, 47.79f, -170.59f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.63f, 47.71f, -169.74f }, { 1162.68f, 47.65f, -169.79f }, { 1162.68f, 47.67f, -169.80f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.68f, 47.67f, -169.79f }, { 1162.63f, 47.73f, -169.74f }, { 1162.63f, 47.71f, -169.74f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.01f, 47.71f, -170.66f }, { 1162.09f, 47.67f, -170.68f }, { 1162.08f, 47.65f, -170.68f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.09f, 47.67f, -170.68f }, { 1162.01f, 47.71f, -170.66f }, { 1162.01f, 47.73f, -170.66f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.76f, 47.22f, -169.93f }, { 1162.77f, 47.02f, -169.95f }, { 1162.77f, 47.22f, -169.93f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.77f, 47.02f, -169.96f }, { 1162.76f, 47.22f, -169.93f }, { 1162.76f, 47.02f, -169.95f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.24f, 47.22f, -170.71f }, { 1162.27f, 47.02f, -170.70f }, { 1162.26f, 47.02f, -170.70f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.27f, 47.02f, -170.70f }, { 1162.24f, 47.22f, -170.71f }, { 1162.25f, 47.22f, -170.71f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.72f, 47.54f, -169.85f }, { 1162.75f, 47.39f, -169.90f }, { 1162.76f, 47.40f, -169.90f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.76f, 47.40f, -169.90f }, { 1162.73f, 47.56f, -169.85f }, { 1162.72f, 47.54f, -169.85f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.15f, 47.54f, -170.70f }, { 1162.21f, 47.40f, -170.71f }, { 1162.21f, 47.39f, -170.71f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.21f, 47.40f, -170.71f }, { 1162.15f, 47.54f, -170.70f }, { 1162.16f, 47.56f, -170.70f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.51f, 47.76f, -169.63f }, { 1162.57f, 47.74f, -169.68f }, { 1162.57f, 47.76f, -169.68f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.57f, 47.76f, -169.68f }, { 1162.51f, 47.79f, -169.63f }, { 1162.51f, 47.76f, -169.63f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.87f, 47.76f, -170.59f }, { 1161.94f, 47.76f, -170.62f }, { 1161.94f, 47.74f, -170.62f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.94f, 47.76f, -170.62f }, { 1161.87f, 47.76f, -170.59f }, { 1161.87f, 47.79f, -170.59f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.67f, 47.65f, -169.79f }, { 1162.72f, 47.54f, -169.85f }, { 1162.73f, 47.56f, -169.85f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.73f, 47.56f, -169.85f }, { 1162.68f, 47.67f, -169.80f }, { 1162.68f, 47.65f, -169.79f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.68f, 47.67f, -169.79f }, { 1162.73f, 47.56f, -169.85f }, { 1162.66f, 47.67f, -169.82f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.73f, 47.56f, -169.85f }, { 1162.71f, 47.56f, -169.88f }, { 1162.66f, 47.67f, -169.82f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.38f, 47.67f, -170.24f }, { 1162.66f, 47.67f, -169.82f }, { 1162.71f, 47.56f, -169.88f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.38f, 47.67f, -170.24f }, { 1162.71f, 47.56f, -169.88f }, { 1162.44f, 47.56f, -170.28f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.18f, 47.56f, -170.67f }, { 1162.38f, 47.67f, -170.24f }, { 1162.44f, 47.56f, -170.28f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.38f, 47.67f, -170.24f }, { 1162.18f, 47.56f, -170.67f }, { 1162.11f, 47.67f, -170.65f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.11f, 47.67f, -170.65f }, { 1162.16f, 47.56f, -170.70f }, { 1162.09f, 47.67f, -170.68f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.16f, 47.56f, -170.70f }, { 1162.11f, 47.67f, -170.65f }, { 1162.18f, 47.56f, -170.67f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.09f, 47.67f, -170.68f }, { 1162.16f, 47.56f, -170.70f }, { 1162.08f, 47.65f, -170.68f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.15f, 47.54f, -170.70f }, { 1162.08f, 47.65f, -170.68f }, { 1162.16f, 47.56f, -170.70f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.75f, 47.39f, -169.90f }, { 1162.77f, 47.22f, -169.93f }, { 1162.76f, 47.40f, -169.90f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.77f, 47.22f, -169.93f }, { 1162.75f, 47.39f, -169.90f }, { 1162.76f, 47.22f, -169.93f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.76f, 47.40f, -169.90f }, { 1162.77f, 47.22f, -169.93f }, { 1162.74f, 47.40f, -169.93f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.77f, 47.22f, -169.93f }, { 1162.75f, 47.22f, -169.96f }, { 1162.74f, 47.40f, -169.93f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.49f, 47.40f, -170.31f }, { 1162.74f, 47.40f, -169.93f }, { 1162.75f, 47.22f, -169.96f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.49f, 47.40f, -170.31f }, { 1162.75f, 47.22f, -169.96f }, { 1162.51f, 47.22f, -170.32f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.27f, 47.22f, -170.68f }, { 1162.49f, 47.40f, -170.31f }, { 1162.51f, 47.22f, -170.32f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.49f, 47.40f, -170.31f }, { 1162.27f, 47.22f, -170.68f }, { 1162.23f, 47.40f, -170.68f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.23f, 47.40f, -170.68f }, { 1162.25f, 47.22f, -170.71f }, { 1162.21f, 47.40f, -170.71f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.25f, 47.22f, -170.71f }, { 1162.23f, 47.40f, -170.68f }, { 1162.27f, 47.22f, -170.68f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.21f, 47.40f, -170.71f }, { 1162.25f, 47.22f, -170.71f }, { 1162.21f, 47.39f, -170.71f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.21f, 47.39f, -170.71f }, { 1162.25f, 47.22f, -170.71f }, { 1162.24f, 47.22f, -170.71f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.57f, 47.74f, -169.68f }, { 1162.63f, 47.71f, -169.74f }, { 1162.63f, 47.73f, -169.74f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.63f, 47.73f, -169.74f }, { 1162.57f, 47.76f, -169.68f }, { 1162.57f, 47.74f, -169.68f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.61f, 47.73f, -169.76f }, { 1162.55f, 47.76f, -169.71f }, { 1162.63f, 47.73f, -169.74f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.63f, 47.73f, -169.74f }, { 1162.55f, 47.76f, -169.71f }, { 1162.57f, 47.76f, -169.68f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.61f, 47.73f, -169.76f }, { 1162.25f, 47.76f, -170.15f }, { 1162.55f, 47.76f, -169.71f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.61f, 47.73f, -169.76f }, { 1162.32f, 47.73f, -170.20f }, { 1162.25f, 47.76f, -170.15f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.03f, 47.73f, -170.63f }, { 1162.25f, 47.76f, -170.15f }, { 1162.32f, 47.73f, -170.20f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.25f, 47.76f, -170.15f }, { 1162.03f, 47.73f, -170.63f }, { 1161.96f, 47.76f, -170.59f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.02f, 47.73f, -170.66f }, { 1161.96f, 47.76f, -170.59f }, { 1162.03f, 47.73f, -170.63f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.96f, 47.76f, -170.59f }, { 1162.01f, 47.73f, -170.66f }, { 1161.94f, 47.76f, -170.62f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.94f, 47.76f, -170.62f }, { 1162.01f, 47.73f, -170.66f }, { 1161.94f, 47.74f, -170.62f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.01f, 47.71f, -170.66f }, { 1161.94f, 47.74f, -170.62f }, { 1162.01f, 47.73f, -170.66f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.01f, 47.78f, -169.34f }, { 1162.51f, 47.79f, -169.63f }, { 1162.49f, 47.79f, -169.66f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.51f, 47.78f, -169.63f }, { 1162.01f, 47.79f, -169.34f }, { 1162.03f, 47.79f, -169.31f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.49f, 47.78f, -169.66f }, { 1161.71f, 47.79f, -169.79f }, { 1162.01f, 47.79f, -169.34f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.19f, 47.78f, -170.11f }, { 1161.71f, 47.79f, -169.79f }, { 1162.49f, 47.79f, -169.66f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.89f, 47.78f, -170.56f }, { 1161.71f, 47.79f, -169.79f }, { 1162.19f, 47.79f, -170.11f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.71f, 47.78f, -169.79f }, { 1161.89f, 47.79f, -170.56f }, { 1161.41f, 47.79f, -170.24f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.87f, 47.78f, -170.59f }, { 1161.39f, 47.79f, -170.27f }, { 1161.89f, 47.79f, -170.56f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.89f, 47.78f, -170.56f }, { 1161.39f, 47.79f, -170.27f }, { 1161.41f, 47.79f, -170.24f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.51f, 47.78f, -169.63f }, { 1162.03f, 47.79f, -169.31f }, { 1162.51f, 47.76f, -169.63f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1162.51f, 47.76f, -169.63f }, { 1162.03f, 47.79f, -169.31f }, { 1162.03f, 47.76f, -169.31f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.87f, 47.76f, -170.59f }, { 1161.39f, 47.79f, -170.27f }, { 1161.87f, 47.79f, -170.59f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1161.39f, 47.78f, -170.27f }, { 1161.87f, 47.76f, -170.59f }, { 1161.39f, 47.76f, -170.27f }, 47.02f, 0.00f },   // waterfeature_02 props 655
    { { 1226.63f, 54.84f, -314.54f }, { 1226.57f, 54.66f, -314.52f }, { 1226.62f, 54.84f, -314.50f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.57f, 54.66f, -314.52f }, { 1226.56f, 54.66f, -314.49f }, { 1226.62f, 54.84f, -314.50f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.88f, 55.07f, -314.51f }, { 1226.97f, 55.09f, -314.51f }, { 1226.89f, 55.07f, -314.55f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.89f, 55.07f, -314.55f }, { 1226.97f, 55.09f, -314.51f }, { 1226.97f, 55.09f, -314.54f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.53f, 54.47f, -314.51f }, { 1226.51f, 54.24f, -314.49f }, { 1226.53f, 54.47f, -314.47f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.51f, 54.24f, -314.49f }, { 1226.51f, 54.24f, -314.45f }, { 1226.53f, 54.47f, -314.47f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.70f, 54.96f, -314.51f }, { 1226.79f, 55.03f, -314.52f }, { 1226.71f, 54.96f, -314.55f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.71f, 54.96f, -314.55f }, { 1226.79f, 55.03f, -314.52f }, { 1226.79f, 55.03f, -314.55f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.51f, 54.24f, -314.45f }, { 1226.43f, 54.47f, -314.00f }, { 1226.53f, 54.47f, -314.47f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.43f, 54.47f, -314.00f }, { 1226.51f, 54.24f, -314.45f }, { 1226.42f, 54.24f, -314.00f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.42f, 54.24f, -314.00f }, { 1226.33f, 54.24f, -313.55f }, { 1226.43f, 54.47f, -314.00f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.34f, 54.47f, -313.52f }, { 1226.43f, 54.47f, -314.00f }, { 1226.33f, 54.24f, -313.55f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.56f, 54.66f, -314.49f }, { 1226.52f, 54.84f, -313.98f }, { 1226.62f, 54.84f, -314.50f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.52f, 54.84f, -313.98f }, { 1226.56f, 54.66f, -314.49f }, { 1226.46f, 54.66f, -313.99f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.46f, 54.66f, -313.99f }, { 1226.37f, 54.66f, -313.50f }, { 1226.52f, 54.84f, -313.98f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.42f, 54.84f, -313.46f }, { 1226.52f, 54.84f, -313.98f }, { 1226.37f, 54.66f, -313.50f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.70f, 54.96f, -314.51f }, { 1226.68f, 55.03f, -313.95f }, { 1226.79f, 55.03f, -314.52f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.68f, 55.03f, -313.95f }, { 1226.70f, 54.96f, -314.51f }, { 1226.60f, 54.96f, -313.97f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.49f, 54.96f, -313.42f }, { 1226.68f, 55.03f, -313.95f }, { 1226.60f, 54.96f, -313.97f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.68f, 55.03f, -313.95f }, { 1226.49f, 54.96f, -313.42f }, { 1226.57f, 55.03f, -313.38f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.88f, 55.07f, -314.51f }, { 1226.85f, 55.09f, -313.92f }, { 1226.97f, 55.09f, -314.51f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.85f, 55.09f, -313.92f }, { 1226.88f, 55.07f, -314.51f }, { 1226.77f, 55.07f, -313.93f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.66f, 55.07f, -313.35f }, { 1226.85f, 55.09f, -313.92f }, { 1226.77f, 55.07f, -313.93f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.85f, 55.09f, -313.92f }, { 1226.66f, 55.07f, -313.35f }, { 1226.74f, 55.09f, -313.32f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.33f, 54.24f, -313.51f }, { 1226.33f, 54.47f, -313.49f }, { 1226.34f, 54.47f, -313.52f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.34f, 54.47f, -313.52f }, { 1226.33f, 54.24f, -313.55f }, { 1226.33f, 54.24f, -313.51f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.36f, 54.66f, -313.46f }, { 1226.41f, 54.84f, -313.42f }, { 1226.42f, 54.84f, -313.46f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.42f, 54.84f, -313.46f }, { 1226.37f, 54.66f, -313.50f }, { 1226.36f, 54.66f, -313.46f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.49f, 54.96f, -313.38f }, { 1226.57f, 55.03f, -313.38f }, { 1226.49f, 54.96f, -313.42f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.57f, 55.03f, -313.38f }, { 1226.49f, 54.96f, -313.38f }, { 1226.56f, 55.03f, -313.34f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.65f, 55.07f, -313.31f }, { 1226.74f, 55.09f, -313.32f }, { 1226.66f, 55.07f, -313.35f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.74f, 55.09f, -313.32f }, { 1226.65f, 55.07f, -313.32f }, { 1226.73f, 55.09f, -313.29f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.80f, 55.00f, -314.55f }, { 1226.71f, 54.94f, -314.54f }, { 1226.71f, 54.96f, -314.55f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.71f, 54.96f, -314.55f }, { 1226.79f, 55.03f, -314.55f }, { 1226.80f, 55.00f, -314.55f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.56f, 55.00f, -313.34f }, { 1226.49f, 54.96f, -313.38f }, { 1226.49f, 54.94f, -313.38f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.49f, 54.96f, -313.38f }, { 1226.56f, 55.00f, -313.34f }, { 1226.56f, 55.03f, -313.34f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.54f, 54.46f, -314.51f }, { 1226.51f, 54.24f, -314.49f }, { 1226.53f, 54.47f, -314.51f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.51f, 54.24f, -314.49f }, { 1226.54f, 54.46f, -314.51f }, { 1226.52f, 54.24f, -314.49f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.34f, 54.46f, -313.48f }, { 1226.33f, 54.24f, -313.51f }, { 1226.33f, 54.24f, -313.51f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.33f, 54.24f, -313.51f }, { 1226.34f, 54.46f, -313.48f }, { 1226.33f, 54.47f, -313.49f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.63f, 54.82f, -314.54f }, { 1226.57f, 54.65f, -314.52f }, { 1226.57f, 54.66f, -314.52f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.57f, 54.66f, -314.52f }, { 1226.63f, 54.84f, -314.54f }, { 1226.63f, 54.82f, -314.54f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.42f, 54.82f, -313.42f }, { 1226.36f, 54.66f, -313.46f }, { 1226.37f, 54.65f, -313.46f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.36f, 54.66f, -313.46f }, { 1226.42f, 54.82f, -313.42f }, { 1226.41f, 54.84f, -313.42f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.98f, 55.07f, -314.54f }, { 1226.89f, 55.04f, -314.55f }, { 1226.89f, 55.07f, -314.55f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.89f, 55.07f, -314.55f }, { 1226.97f, 55.09f, -314.54f }, { 1226.98f, 55.07f, -314.54f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.73f, 55.07f, -313.29f }, { 1226.65f, 55.07f, -313.32f }, { 1226.65f, 55.04f, -313.31f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.65f, 55.07f, -313.31f }, { 1226.73f, 55.07f, -313.29f }, { 1226.73f, 55.09f, -313.29f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.71f, 54.94f, -314.54f }, { 1226.63f, 54.82f, -314.54f }, { 1226.63f, 54.84f, -314.54f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.63f, 54.84f, -314.54f }, { 1226.71f, 54.96f, -314.55f }, { 1226.71f, 54.94f, -314.54f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.71f, 54.96f, -314.55f }, { 1226.63f, 54.84f, -314.54f }, { 1226.70f, 54.96f, -314.51f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.63f, 54.84f, -314.54f }, { 1226.62f, 54.84f, -314.50f }, { 1226.70f, 54.96f, -314.51f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.60f, 54.96f, -313.96f }, { 1226.70f, 54.96f, -314.51f }, { 1226.62f, 54.84f, -314.50f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.60f, 54.96f, -313.96f }, { 1226.62f, 54.84f, -314.50f }, { 1226.52f, 54.84f, -313.98f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.42f, 54.84f, -313.46f }, { 1226.60f, 54.96f, -313.97f }, { 1226.52f, 54.84f, -313.98f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.60f, 54.96f, -313.96f }, { 1226.42f, 54.84f, -313.46f }, { 1226.49f, 54.96f, -313.42f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.49f, 54.96f, -313.42f }, { 1226.41f, 54.84f, -313.42f }, { 1226.49f, 54.96f, -313.38f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.41f, 54.84f, -313.42f }, { 1226.49f, 54.96f, -313.42f }, { 1226.42f, 54.84f, -313.46f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.49f, 54.96f, -313.38f }, { 1226.41f, 54.84f, -313.42f }, { 1226.49f, 54.94f, -313.38f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.42f, 54.82f, -313.42f }, { 1226.49f, 54.94f, -313.38f }, { 1226.41f, 54.84f, -313.42f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.57f, 54.65f, -314.52f }, { 1226.53f, 54.47f, -314.51f }, { 1226.57f, 54.66f, -314.52f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.53f, 54.47f, -314.51f }, { 1226.57f, 54.65f, -314.52f }, { 1226.54f, 54.46f, -314.51f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.57f, 54.66f, -314.52f }, { 1226.53f, 54.47f, -314.51f }, { 1226.56f, 54.66f, -314.49f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.53f, 54.47f, -314.51f }, { 1226.53f, 54.47f, -314.47f }, { 1226.56f, 54.66f, -314.49f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.46f, 54.66f, -313.99f }, { 1226.56f, 54.66f, -314.49f }, { 1226.53f, 54.47f, -314.47f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.46f, 54.66f, -313.99f }, { 1226.53f, 54.47f, -314.47f }, { 1226.43f, 54.47f, -314.00f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.34f, 54.47f, -313.52f }, { 1226.46f, 54.66f, -313.99f }, { 1226.43f, 54.47f, -314.00f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.46f, 54.66f, -313.99f }, { 1226.34f, 54.47f, -313.52f }, { 1226.37f, 54.66f, -313.50f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.37f, 54.66f, -313.50f }, { 1226.33f, 54.47f, -313.49f }, { 1226.36f, 54.66f, -313.46f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.33f, 54.47f, -313.49f }, { 1226.37f, 54.66f, -313.50f }, { 1226.34f, 54.47f, -313.52f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.36f, 54.66f, -313.46f }, { 1226.33f, 54.47f, -313.49f }, { 1226.37f, 54.65f, -313.46f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.37f, 54.65f, -313.46f }, { 1226.33f, 54.47f, -313.49f }, { 1226.34f, 54.46f, -313.48f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.89f, 55.04f, -314.55f }, { 1226.80f, 55.00f, -314.55f }, { 1226.79f, 55.03f, -314.55f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.79f, 55.03f, -314.55f }, { 1226.89f, 55.07f, -314.55f }, { 1226.89f, 55.04f, -314.55f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.79f, 55.03f, -314.52f }, { 1226.88f, 55.07f, -314.51f }, { 1226.79f, 55.03f, -314.55f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.79f, 55.03f, -314.55f }, { 1226.88f, 55.07f, -314.51f }, { 1226.89f, 55.07f, -314.55f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.79f, 55.03f, -314.52f }, { 1226.77f, 55.07f, -313.93f }, { 1226.88f, 55.07f, -314.51f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.79f, 55.03f, -314.52f }, { 1226.68f, 55.03f, -313.95f }, { 1226.77f, 55.07f, -313.93f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.57f, 55.03f, -313.38f }, { 1226.77f, 55.07f, -313.93f }, { 1226.68f, 55.03f, -313.95f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.77f, 55.07f, -313.93f }, { 1226.57f, 55.03f, -313.38f }, { 1226.66f, 55.07f, -313.35f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.56f, 55.03f, -313.35f }, { 1226.66f, 55.07f, -313.35f }, { 1226.57f, 55.03f, -313.38f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.66f, 55.07f, -313.35f }, { 1226.56f, 55.03f, -313.34f }, { 1226.65f, 55.07f, -313.32f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.65f, 55.07f, -313.31f }, { 1226.56f, 55.03f, -313.34f }, { 1226.65f, 55.04f, -313.31f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.56f, 55.00f, -313.34f }, { 1226.65f, 55.04f, -313.31f }, { 1226.56f, 55.03f, -313.34f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1227.59f, 55.09f, -314.39f }, { 1226.97f, 55.09f, -314.54f }, { 1226.97f, 55.09f, -314.51f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.97f, 55.09f, -314.54f }, { 1227.59f, 55.09f, -314.39f }, { 1227.60f, 55.09f, -314.42f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.97f, 55.09f, -314.51f }, { 1227.48f, 55.09f, -313.80f }, { 1227.59f, 55.09f, -314.39f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.85f, 55.09f, -313.92f }, { 1227.48f, 55.09f, -313.80f }, { 1226.97f, 55.09f, -314.51f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.74f, 55.09f, -313.32f }, { 1227.48f, 55.09f, -313.80f }, { 1226.85f, 55.09f, -313.92f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1227.48f, 55.09f, -313.80f }, { 1226.74f, 55.09f, -313.32f }, { 1227.36f, 55.09f, -313.20f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.73f, 55.09f, -313.29f }, { 1227.36f, 55.09f, -313.17f }, { 1226.74f, 55.09f, -313.32f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.74f, 55.09f, -313.32f }, { 1227.36f, 55.09f, -313.17f }, { 1227.36f, 55.09f, -313.20f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.97f, 55.09f, -314.54f }, { 1227.60f, 55.09f, -314.42f }, { 1226.98f, 55.07f, -314.54f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.98f, 55.07f, -314.54f }, { 1227.60f, 55.09f, -314.42f }, { 1227.60f, 55.07f, -314.42f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1226.73f, 55.07f, -313.29f }, { 1227.36f, 55.09f, -313.17f }, { 1226.73f, 55.09f, -313.29f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1227.36f, 55.09f, -313.17f }, { 1226.73f, 55.07f, -313.29f }, { 1227.36f, 55.07f, -313.17f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1224.03f, 54.84f, -301.02f }, { 1223.97f, 54.66f, -301.01f }, { 1224.02f, 54.84f, -300.99f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1223.97f, 54.66f, -301.01f }, { 1223.96f, 54.66f, -300.97f }, { 1224.02f, 54.84f, -300.99f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1224.28f, 55.07f, -301.00f }, { 1224.37f, 55.09f, -300.99f }, { 1224.29f, 55.07f, -301.04f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1224.29f, 55.07f, -301.04f }, { 1224.37f, 55.09f, -300.99f }, { 1224.38f, 55.09f, -301.03f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1223.93f, 54.47f, -301.00f }, { 1223.92f, 54.24f, -300.98f }, { 1223.93f, 54.47f, -300.96f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1223.92f, 54.24f, -300.98f }, { 1223.91f, 54.24f, -300.94f }, { 1223.93f, 54.47f, -300.96f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1224.11f, 54.96f, -301.00f }, { 1224.19f, 55.03f, -301.00f }, { 1224.11f, 54.96f, -301.03f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1224.11f, 54.96f, -301.03f }, { 1224.19f, 55.03f, -301.00f }, { 1224.20f, 55.03f, -301.04f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1223.91f, 54.24f, -300.94f }, { 1223.83f, 54.47f, -300.48f }, { 1223.93f, 54.47f, -300.96f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1223.84f, 54.47f, -300.49f }, { 1223.91f, 54.24f, -300.94f }, { 1223.82f, 54.24f, -300.49f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1223.82f, 54.24f, -300.49f }, { 1223.73f, 54.24f, -300.03f }, { 1223.83f, 54.47f, -300.48f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1223.74f, 54.47f, -300.01f }, { 1223.83f, 54.47f, -300.48f }, { 1223.73f, 54.24f, -300.03f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1223.96f, 54.66f, -300.97f }, { 1223.92f, 54.84f, -300.47f }, { 1224.02f, 54.84f, -300.99f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1223.92f, 54.84f, -300.47f }, { 1223.96f, 54.66f, -300.97f }, { 1223.87f, 54.66f, -300.48f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1223.87f, 54.66f, -300.48f }, { 1223.77f, 54.66f, -299.98f }, { 1223.92f, 54.84f, -300.47f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1223.82f, 54.84f, -299.95f }, { 1223.92f, 54.84f, -300.47f }, { 1223.77f, 54.66f, -299.98f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1224.11f, 54.96f, -301.00f }, { 1224.08f, 55.03f, -300.44f }, { 1224.19f, 55.03f, -301.00f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1224.08f, 55.03f, -300.44f }, { 1224.10f, 54.96f, -301.00f }, { 1224.00f, 54.96f, -300.45f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1223.89f, 54.96f, -299.91f }, { 1224.08f, 55.03f, -300.44f }, { 1224.00f, 54.96f, -300.45f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1224.08f, 55.03f, -300.44f }, { 1223.89f, 54.96f, -299.91f }, { 1223.97f, 55.03f, -299.87f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1224.28f, 55.07f, -301.00f }, { 1224.26f, 55.09f, -300.40f }, { 1224.37f, 55.09f, -300.99f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1224.26f, 55.09f, -300.40f }, { 1224.28f, 55.07f, -301.00f }, { 1224.17f, 55.07f, -300.42f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1224.06f, 55.07f, -299.84f }, { 1224.26f, 55.09f, -300.40f }, { 1224.17f, 55.07f, -300.42f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1224.26f, 55.09f, -300.40f }, { 1224.06f, 55.07f, -299.84f }, { 1224.14f, 55.09f, -299.81f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1223.73f, 54.24f, -300.00f }, { 1223.74f, 54.47f, -299.97f }, { 1223.74f, 54.47f, -300.01f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1223.74f, 54.47f, -300.01f }, { 1223.73f, 54.24f, -300.03f }, { 1223.73f, 54.24f, -300.00f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1223.76f, 54.66f, -299.95f }, { 1223.81f, 54.84f, -299.91f }, { 1223.82f, 54.84f, -299.95f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1223.82f, 54.84f, -299.95f }, { 1223.77f, 54.66f, -299.98f }, { 1223.76f, 54.66f, -299.95f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1223.89f, 54.96f, -299.87f }, { 1223.97f, 55.03f, -299.87f }, { 1223.89f, 54.96f, -299.91f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1223.97f, 55.03f, -299.87f }, { 1223.89f, 54.96f, -299.87f }, { 1223.96f, 55.03f, -299.83f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1224.05f, 55.07f, -299.80f }, { 1224.14f, 55.09f, -299.81f }, { 1224.06f, 55.07f, -299.84f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1224.14f, 55.09f, -299.81f }, { 1224.05f, 55.07f, -299.80f }, { 1224.13f, 55.09f, -299.77f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1224.20f, 55.00f, -301.04f }, { 1224.12f, 54.94f, -301.03f }, { 1224.11f, 54.96f, -301.03f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1224.11f, 54.96f, -301.03f }, { 1224.20f, 55.03f, -301.04f }, { 1224.20f, 55.00f, -301.04f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1223.96f, 55.00f, -299.83f }, { 1223.89f, 54.96f, -299.87f }, { 1223.89f, 54.94f, -299.87f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1223.89f, 54.96f, -299.87f }, { 1223.96f, 55.00f, -299.83f }, { 1223.96f, 55.03f, -299.83f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1223.94f, 54.46f, -300.99f }, { 1223.92f, 54.24f, -300.98f }, { 1223.93f, 54.47f, -300.99f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1223.92f, 54.24f, -300.98f }, { 1223.94f, 54.46f, -300.99f }, { 1223.93f, 54.24f, -300.98f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1223.74f, 54.46f, -299.97f }, { 1223.73f, 54.24f, -300.00f }, { 1223.74f, 54.24f, -299.99f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1223.73f, 54.24f, -300.00f }, { 1223.74f, 54.46f, -299.97f }, { 1223.74f, 54.47f, -299.97f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1224.04f, 54.82f, -301.02f }, { 1223.98f, 54.65f, -301.01f }, { 1223.97f, 54.66f, -301.01f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1223.97f, 54.66f, -301.01f }, { 1224.03f, 54.84f, -301.02f }, { 1224.04f, 54.82f, -301.02f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1223.82f, 54.82f, -299.91f }, { 1223.76f, 54.66f, -299.95f }, { 1223.77f, 54.65f, -299.94f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1223.76f, 54.66f, -299.95f }, { 1223.82f, 54.82f, -299.91f }, { 1223.81f, 54.84f, -299.91f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1224.38f, 55.07f, -301.03f }, { 1224.29f, 55.04f, -301.04f }, { 1224.29f, 55.07f, -301.04f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1224.29f, 55.07f, -301.04f }, { 1224.38f, 55.09f, -301.03f }, { 1224.38f, 55.07f, -301.03f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1224.13f, 55.07f, -299.77f }, { 1224.05f, 55.07f, -299.80f }, { 1224.05f, 55.04f, -299.80f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1224.05f, 55.07f, -299.80f }, { 1224.14f, 55.07f, -299.77f }, { 1224.13f, 55.09f, -299.77f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1224.12f, 54.94f, -301.03f }, { 1224.04f, 54.82f, -301.02f }, { 1224.03f, 54.84f, -301.02f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1224.03f, 54.84f, -301.02f }, { 1224.11f, 54.96f, -301.03f }, { 1224.12f, 54.94f, -301.03f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1224.11f, 54.96f, -301.03f }, { 1224.03f, 54.84f, -301.02f }, { 1224.10f, 54.96f, -301.00f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1224.03f, 54.84f, -301.02f }, { 1224.02f, 54.84f, -300.99f }, { 1224.10f, 54.96f, -301.00f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1224.00f, 54.96f, -300.45f }, { 1224.10f, 54.96f, -301.00f }, { 1224.02f, 54.84f, -300.99f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1224.00f, 54.96f, -300.45f }, { 1224.02f, 54.84f, -300.99f }, { 1223.92f, 54.84f, -300.47f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1223.82f, 54.84f, -299.95f }, { 1224.00f, 54.96f, -300.45f }, { 1223.92f, 54.84f, -300.47f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1224.00f, 54.96f, -300.45f }, { 1223.82f, 54.84f, -299.95f }, { 1223.89f, 54.96f, -299.91f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1223.89f, 54.96f, -299.91f }, { 1223.81f, 54.84f, -299.91f }, { 1223.89f, 54.96f, -299.87f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1223.81f, 54.84f, -299.91f }, { 1223.89f, 54.96f, -299.91f }, { 1223.82f, 54.84f, -299.95f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1223.89f, 54.96f, -299.87f }, { 1223.81f, 54.84f, -299.91f }, { 1223.89f, 54.94f, -299.87f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1223.82f, 54.82f, -299.91f }, { 1223.89f, 54.94f, -299.87f }, { 1223.81f, 54.84f, -299.91f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1223.98f, 54.65f, -301.01f }, { 1223.93f, 54.47f, -300.99f }, { 1223.97f, 54.66f, -301.01f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1223.93f, 54.47f, -301.00f }, { 1223.98f, 54.65f, -301.01f }, { 1223.94f, 54.46f, -300.99f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1223.97f, 54.66f, -301.01f }, { 1223.93f, 54.47f, -300.99f }, { 1223.96f, 54.66f, -300.97f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1223.93f, 54.47f, -301.00f }, { 1223.93f, 54.47f, -300.96f }, { 1223.96f, 54.66f, -300.97f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1223.87f, 54.66f, -300.48f }, { 1223.96f, 54.66f, -300.97f }, { 1223.93f, 54.47f, -300.96f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1223.87f, 54.66f, -300.48f }, { 1223.93f, 54.47f, -300.96f }, { 1223.83f, 54.47f, -300.48f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1223.74f, 54.47f, -300.01f }, { 1223.87f, 54.66f, -300.48f }, { 1223.83f, 54.47f, -300.48f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1223.87f, 54.66f, -300.48f }, { 1223.74f, 54.47f, -300.01f }, { 1223.77f, 54.66f, -299.98f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1223.77f, 54.66f, -299.98f }, { 1223.74f, 54.47f, -299.97f }, { 1223.76f, 54.66f, -299.95f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1223.74f, 54.47f, -299.97f }, { 1223.77f, 54.66f, -299.98f }, { 1223.74f, 54.47f, -300.01f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1223.76f, 54.66f, -299.95f }, { 1223.74f, 54.47f, -299.97f }, { 1223.77f, 54.65f, -299.94f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1223.77f, 54.65f, -299.94f }, { 1223.74f, 54.47f, -299.97f }, { 1223.74f, 54.46f, -299.97f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1224.29f, 55.04f, -301.04f }, { 1224.20f, 55.00f, -301.04f }, { 1224.20f, 55.03f, -301.04f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1224.19f, 55.03f, -301.04f }, { 1224.29f, 55.07f, -301.04f }, { 1224.29f, 55.04f, -301.04f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1224.19f, 55.03f, -301.00f }, { 1224.28f, 55.07f, -301.00f }, { 1224.20f, 55.03f, -301.04f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1224.19f, 55.03f, -301.04f }, { 1224.28f, 55.07f, -301.00f }, { 1224.29f, 55.07f, -301.04f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1224.19f, 55.03f, -301.00f }, { 1224.17f, 55.07f, -300.42f }, { 1224.28f, 55.07f, -301.00f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1224.19f, 55.03f, -301.00f }, { 1224.08f, 55.03f, -300.44f }, { 1224.17f, 55.07f, -300.42f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1223.97f, 55.03f, -299.87f }, { 1224.17f, 55.07f, -300.42f }, { 1224.08f, 55.03f, -300.44f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1224.17f, 55.07f, -300.42f }, { 1223.97f, 55.03f, -299.87f }, { 1224.06f, 55.07f, -299.84f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1223.96f, 55.03f, -299.83f }, { 1224.06f, 55.07f, -299.84f }, { 1223.97f, 55.03f, -299.87f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1224.06f, 55.07f, -299.84f }, { 1223.96f, 55.03f, -299.83f }, { 1224.05f, 55.07f, -299.80f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1224.05f, 55.07f, -299.80f }, { 1223.96f, 55.03f, -299.83f }, { 1224.05f, 55.04f, -299.80f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1223.96f, 55.00f, -299.83f }, { 1224.05f, 55.04f, -299.80f }, { 1223.96f, 55.03f, -299.83f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1224.99f, 55.09f, -300.87f }, { 1224.38f, 55.09f, -301.03f }, { 1224.37f, 55.09f, -300.99f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1224.38f, 55.09f, -301.03f }, { 1224.99f, 55.09f, -300.87f }, { 1225.00f, 55.09f, -300.91f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1224.37f, 55.09f, -301.00f }, { 1224.88f, 55.09f, -300.28f }, { 1224.99f, 55.09f, -300.87f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1224.26f, 55.09f, -300.40f }, { 1224.88f, 55.09f, -300.28f }, { 1224.37f, 55.09f, -300.99f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1224.14f, 55.09f, -299.81f }, { 1224.88f, 55.09f, -300.28f }, { 1224.26f, 55.09f, -300.40f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1224.88f, 55.09f, -300.28f }, { 1224.14f, 55.09f, -299.81f }, { 1224.77f, 55.09f, -299.69f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1224.13f, 55.09f, -299.77f }, { 1224.76f, 55.09f, -299.65f }, { 1224.14f, 55.09f, -299.81f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1224.14f, 55.09f, -299.81f }, { 1224.76f, 55.09f, -299.65f }, { 1224.77f, 55.09f, -299.69f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1224.38f, 55.09f, -301.03f }, { 1225.00f, 55.09f, -300.91f }, { 1224.38f, 55.07f, -301.03f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1224.38f, 55.07f, -301.03f }, { 1225.00f, 55.09f, -300.91f }, { 1225.00f, 55.07f, -300.91f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1224.13f, 55.07f, -299.77f }, { 1224.76f, 55.09f, -299.65f }, { 1224.13f, 55.09f, -299.77f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1224.76f, 55.09f, -299.65f }, { 1224.14f, 55.07f, -299.77f }, { 1224.76f, 55.07f, -299.65f }, 54.24f, 0.00f },   // waterfeature_02 props 658
    { { 1159.05f, 66.28f, -315.63f }, { 1158.94f, 66.20f, -315.62f }, { 1158.94f, 66.23f, -315.62f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.94f, 66.23f, -315.62f }, { 1159.05f, 66.31f, -315.63f }, { 1159.05f, 66.28f, -315.63f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.94f, 66.23f, -315.62f }, { 1159.04f, 66.31f, -315.59f }, { 1159.05f, 66.31f, -315.63f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.93f, 66.23f, -315.58f }, { 1159.04f, 66.31f, -315.59f }, { 1158.94f, 66.23f, -315.62f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.94f, 66.23f, -315.62f }, { 1158.83f, 66.06f, -315.61f }, { 1158.93f, 66.23f, -315.58f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.83f, 66.06f, -315.61f }, { 1158.94f, 66.23f, -315.62f }, { 1158.94f, 66.20f, -315.62f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.94f, 66.20f, -315.62f }, { 1158.84f, 66.04f, -315.61f }, { 1158.83f, 66.06f, -315.61f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.93f, 66.23f, -315.58f }, { 1158.89f, 66.31f, -314.85f }, { 1159.04f, 66.31f, -315.59f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1159.04f, 66.31f, -315.59f }, { 1159.16f, 66.36f, -315.58f }, { 1159.05f, 66.31f, -315.63f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1159.04f, 66.31f, -315.59f }, { 1159.01f, 66.36f, -314.82f }, { 1159.16f, 66.36f, -315.58f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1159.04f, 66.31f, -315.59f }, { 1158.89f, 66.31f, -314.85f }, { 1159.01f, 66.36f, -314.82f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.89f, 66.31f, -314.85f }, { 1158.93f, 66.23f, -315.58f }, { 1158.79f, 66.23f, -314.87f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.79f, 66.23f, -314.87f }, { 1158.93f, 66.23f, -315.58f }, { 1158.82f, 66.06f, -315.57f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.83f, 66.06f, -315.61f }, { 1158.82f, 66.06f, -315.57f }, { 1158.93f, 66.23f, -315.58f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.65f, 66.23f, -314.16f }, { 1158.89f, 66.31f, -314.85f }, { 1158.79f, 66.23f, -314.87f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.74f, 66.31f, -314.10f }, { 1159.01f, 66.36f, -314.82f }, { 1158.89f, 66.31f, -314.85f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.89f, 66.31f, -314.85f }, { 1158.65f, 66.23f, -314.16f }, { 1158.74f, 66.31f, -314.10f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1159.12f, 66.40f, -314.80f }, { 1159.16f, 66.36f, -315.58f }, { 1159.01f, 66.36f, -314.82f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.86f, 66.36f, -314.06f }, { 1159.12f, 66.40f, -314.80f }, { 1159.01f, 66.36f, -314.82f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1159.01f, 66.36f, -314.82f }, { 1158.74f, 66.31f, -314.10f }, { 1158.86f, 66.36f, -314.07f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.83f, 66.06f, -315.61f }, { 1158.75f, 65.83f, -315.59f }, { 1158.82f, 66.06f, -315.57f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.75f, 65.83f, -315.59f }, { 1158.83f, 66.06f, -315.61f }, { 1158.84f, 66.04f, -315.61f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.79f, 66.23f, -314.87f }, { 1158.82f, 66.06f, -315.57f }, { 1158.68f, 66.06f, -314.89f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.55f, 66.06f, -314.21f }, { 1158.79f, 66.23f, -314.87f }, { 1158.68f, 66.06f, -314.89f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.79f, 66.23f, -314.87f }, { 1158.55f, 66.06f, -314.21f }, { 1158.65f, 66.23f, -314.16f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.75f, 65.83f, -315.59f }, { 1158.74f, 65.83f, -315.55f }, { 1158.82f, 66.06f, -315.57f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.74f, 65.83f, -315.55f }, { 1158.68f, 66.06f, -314.89f }, { 1158.82f, 66.06f, -315.57f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.64f, 66.23f, -314.11f }, { 1158.74f, 66.31f, -314.10f }, { 1158.65f, 66.23f, -314.16f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.54f, 66.06f, -314.16f }, { 1158.65f, 66.23f, -314.16f }, { 1158.55f, 66.06f, -314.21f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.65f, 66.23f, -314.16f }, { 1158.54f, 66.06f, -314.16f }, { 1158.64f, 66.23f, -314.11f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.74f, 66.31f, -314.10f }, { 1158.64f, 66.23f, -314.11f }, { 1158.73f, 66.31f, -314.05f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.73f, 66.31f, -314.05f }, { 1158.86f, 66.36f, -314.07f }, { 1158.74f, 66.31f, -314.10f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.86f, 66.36f, -314.06f }, { 1158.73f, 66.31f, -314.05f }, { 1158.85f, 66.36f, -314.02f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.85f, 66.36f, -314.01f }, { 1158.97f, 66.40f, -314.03f }, { 1158.86f, 66.36f, -314.07f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1159.12f, 66.40f, -314.80f }, { 1158.86f, 66.36f, -314.07f }, { 1158.97f, 66.40f, -314.03f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.84f, 66.04f, -315.61f }, { 1158.76f, 65.82f, -315.59f }, { 1158.75f, 65.83f, -315.59f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.68f, 66.06f, -314.89f }, { 1158.74f, 65.83f, -315.55f }, { 1158.61f, 65.83f, -314.90f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.55f, 66.06f, -314.21f }, { 1158.68f, 66.06f, -314.89f }, { 1158.48f, 65.83f, -314.25f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.61f, 65.83f, -314.90f }, { 1158.48f, 65.83f, -314.25f }, { 1158.68f, 66.06f, -314.89f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.47f, 65.83f, -314.20f }, { 1158.54f, 66.06f, -314.16f }, { 1158.55f, 66.06f, -314.21f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.55f, 66.06f, -314.21f }, { 1158.48f, 65.83f, -314.25f }, { 1158.47f, 65.83f, -314.20f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.75f, 65.83f, -315.59f }, { 1158.70f, 65.58f, -315.57f }, { 1158.74f, 65.83f, -315.55f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.61f, 65.83f, -314.90f }, { 1158.74f, 65.83f, -315.55f }, { 1158.69f, 65.58f, -315.53f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.70f, 65.58f, -315.57f }, { 1158.69f, 65.58f, -315.53f }, { 1158.74f, 65.83f, -315.55f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.61f, 65.83f, -314.90f }, { 1158.69f, 65.58f, -315.53f }, { 1158.57f, 65.58f, -314.91f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.45f, 65.58f, -314.29f }, { 1158.61f, 65.83f, -314.90f }, { 1158.57f, 65.58f, -314.91f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.61f, 65.83f, -314.90f }, { 1158.45f, 65.58f, -314.29f }, { 1158.48f, 65.83f, -314.25f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.48f, 65.83f, -314.25f }, { 1158.44f, 65.58f, -314.24f }, { 1158.47f, 65.83f, -314.20f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.44f, 65.58f, -314.24f }, { 1158.48f, 65.83f, -314.25f }, { 1158.45f, 65.58f, -314.29f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.70f, 65.58f, -315.57f }, { 1158.68f, 65.29f, -315.55f }, { 1158.69f, 65.58f, -315.53f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.67f, 65.29f, -315.50f }, { 1158.57f, 65.58f, -314.91f }, { 1158.69f, 65.58f, -315.53f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.68f, 65.29f, -315.55f }, { 1158.67f, 65.29f, -315.51f }, { 1158.69f, 65.58f, -315.53f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.57f, 65.58f, -314.91f }, { 1158.67f, 65.29f, -315.51f }, { 1158.55f, 65.29f, -314.91f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.45f, 65.58f, -314.29f }, { 1158.57f, 65.58f, -314.91f }, { 1158.44f, 65.29f, -314.32f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.55f, 65.29f, -314.91f }, { 1158.44f, 65.29f, -314.32f }, { 1158.57f, 65.58f, -314.91f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.43f, 65.29f, -314.27f }, { 1158.44f, 65.58f, -314.24f }, { 1158.45f, 65.58f, -314.29f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.45f, 65.58f, -314.29f }, { 1158.44f, 65.29f, -314.32f }, { 1158.43f, 65.29f, -314.27f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.68f, 65.29f, -315.55f }, { 1158.67f, 64.98f, -315.54f }, { 1158.67f, 65.29f, -315.51f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.66f, 64.98f, -315.50f }, { 1158.55f, 65.29f, -314.91f }, { 1158.67f, 65.29f, -315.51f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.67f, 64.98f, -315.54f }, { 1158.66f, 64.98f, -315.49f }, { 1158.67f, 65.29f, -315.51f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.55f, 65.29f, -314.91f }, { 1158.66f, 64.98f, -315.49f }, { 1158.54f, 64.98f, -314.91f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.43f, 64.98f, -314.33f }, { 1158.55f, 65.29f, -314.91f }, { 1158.54f, 64.98f, -314.91f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.55f, 65.29f, -314.91f }, { 1158.43f, 64.98f, -314.33f }, { 1158.44f, 65.29f, -314.32f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.44f, 65.29f, -314.32f }, { 1158.42f, 64.98f, -314.28f }, { 1158.43f, 65.29f, -314.27f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.42f, 64.98f, -314.28f }, { 1158.44f, 65.29f, -314.32f }, { 1158.43f, 64.98f, -314.33f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.43f, 64.98f, -314.33f }, { 1158.43f, 64.07f, -314.33f }, { 1158.42f, 64.98f, -314.28f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.43f, 64.98f, -314.33f }, { 1158.54f, 64.98f, -314.91f }, { 1158.43f, 64.07f, -314.33f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.67f, 64.98f, -315.54f }, { 1158.67f, 64.07f, -315.54f }, { 1158.66f, 64.98f, -315.49f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.54f, 64.98f, -314.91f }, { 1158.66f, 64.98f, -315.49f }, { 1158.66f, 64.07f, -315.49f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.66f, 64.98f, -315.50f }, { 1158.67f, 64.07f, -315.54f }, { 1158.66f, 64.07f, -315.49f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.54f, 64.98f, -314.91f }, { 1158.66f, 64.07f, -315.49f }, { 1158.54f, 64.07f, -314.91f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.54f, 64.98f, -314.91f }, { 1158.54f, 64.07f, -314.91f }, { 1158.43f, 64.07f, -314.33f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.42f, 64.98f, -314.28f }, { 1158.43f, 64.07f, -314.33f }, { 1158.42f, 64.07f, -314.28f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.43f, 64.07f, -314.33f }, { 1158.54f, 64.07f, -314.91f }, { 1158.43f, 63.17f, -314.33f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.43f, 64.07f, -314.33f }, { 1158.43f, 63.17f, -314.33f }, { 1158.42f, 64.07f, -314.28f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.67f, 64.07f, -315.54f }, { 1158.67f, 63.17f, -315.54f }, { 1158.66f, 64.07f, -315.49f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.54f, 64.07f, -314.91f }, { 1158.66f, 64.07f, -315.49f }, { 1158.66f, 63.17f, -315.49f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.66f, 64.07f, -315.50f }, { 1158.67f, 63.17f, -315.54f }, { 1158.66f, 63.17f, -315.49f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.54f, 64.07f, -314.91f }, { 1158.66f, 63.17f, -315.49f }, { 1158.54f, 63.17f, -314.91f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.54f, 64.07f, -314.91f }, { 1158.54f, 63.17f, -314.91f }, { 1158.43f, 63.17f, -314.33f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.67f, 64.07f, -315.54f }, { 1158.67f, 64.98f, -315.54f }, { 1158.68f, 64.97f, -315.54f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.67f, 63.17f, -315.54f }, { 1158.67f, 64.07f, -315.54f }, { 1158.68f, 64.07f, -315.54f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.67f, 64.07f, -315.54f }, { 1158.68f, 64.97f, -315.54f }, { 1158.68f, 64.07f, -315.54f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.42f, 64.07f, -314.28f }, { 1158.43f, 63.17f, -314.33f }, { 1158.42f, 63.17f, -314.28f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.43f, 63.17f, -314.33f }, { 1158.43f, 62.27f, -314.33f }, { 1158.42f, 63.17f, -314.28f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.43f, 63.17f, -314.33f }, { 1158.54f, 63.17f, -314.91f }, { 1158.43f, 62.27f, -314.33f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.42f, 64.07f, -314.28f }, { 1158.43f, 64.97f, -314.28f }, { 1158.42f, 64.98f, -314.28f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.42f, 63.17f, -314.28f }, { 1158.43f, 64.07f, -314.28f }, { 1158.42f, 64.07f, -314.28f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.43f, 64.97f, -314.28f }, { 1158.42f, 64.07f, -314.28f }, { 1158.43f, 64.07f, -314.28f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.67f, 63.17f, -315.54f }, { 1158.67f, 62.27f, -315.54f }, { 1158.66f, 63.17f, -315.49f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.67f, 62.27f, -315.54f }, { 1158.67f, 63.17f, -315.54f }, { 1158.68f, 63.17f, -315.54f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.67f, 63.17f, -315.54f }, { 1158.68f, 64.07f, -315.54f }, { 1158.68f, 63.17f, -315.54f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.66f, 63.17f, -315.50f }, { 1158.67f, 62.27f, -315.54f }, { 1158.66f, 62.27f, -315.49f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.54f, 63.17f, -314.91f }, { 1158.66f, 63.17f, -315.49f }, { 1158.66f, 62.27f, -315.49f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.54f, 63.17f, -314.91f }, { 1158.66f, 62.27f, -315.49f }, { 1158.54f, 62.27f, -314.91f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.54f, 63.17f, -314.91f }, { 1158.54f, 62.27f, -314.91f }, { 1158.43f, 62.27f, -314.33f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.42f, 63.17f, -314.28f }, { 1158.43f, 62.27f, -314.33f }, { 1158.42f, 62.27f, -314.28f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.43f, 64.07f, -314.28f }, { 1158.42f, 63.17f, -314.28f }, { 1158.43f, 63.17f, -314.28f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.42f, 62.27f, -314.28f }, { 1158.43f, 63.17f, -314.28f }, { 1158.42f, 63.17f, -314.28f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.43f, 62.27f, -314.33f }, { 1158.43f, 61.37f, -314.33f }, { 1158.42f, 62.27f, -314.28f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.43f, 62.27f, -314.33f }, { 1158.54f, 62.27f, -314.91f }, { 1158.43f, 61.37f, -314.33f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.67f, 62.27f, -315.54f }, { 1158.68f, 63.17f, -315.54f }, { 1158.68f, 62.27f, -315.54f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.67f, 62.27f, -315.54f }, { 1158.67f, 61.37f, -315.54f }, { 1158.66f, 62.27f, -315.49f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.67f, 61.37f, -315.54f }, { 1158.67f, 62.27f, -315.54f }, { 1158.68f, 62.27f, -315.54f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.66f, 62.27f, -315.50f }, { 1158.67f, 61.37f, -315.54f }, { 1158.66f, 61.37f, -315.49f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.54f, 62.27f, -314.91f }, { 1158.66f, 62.27f, -315.49f }, { 1158.66f, 61.37f, -315.49f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.54f, 62.27f, -314.91f }, { 1158.66f, 61.37f, -315.49f }, { 1158.54f, 61.37f, -314.91f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.54f, 62.27f, -314.91f }, { 1158.54f, 61.37f, -314.91f }, { 1158.43f, 61.37f, -314.33f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.43f, 63.17f, -314.28f }, { 1158.42f, 62.27f, -314.28f }, { 1158.43f, 62.27f, -314.28f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.42f, 61.37f, -314.28f }, { 1158.43f, 62.27f, -314.28f }, { 1158.42f, 62.27f, -314.28f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.42f, 62.27f, -314.28f }, { 1158.43f, 61.37f, -314.33f }, { 1158.42f, 61.37f, -314.28f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.43f, 61.37f, -314.33f }, { 1158.43f, 60.47f, -314.33f }, { 1158.42f, 61.37f, -314.28f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.43f, 61.37f, -314.33f }, { 1158.54f, 61.37f, -314.91f }, { 1158.43f, 60.47f, -314.33f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.67f, 61.37f, -315.54f }, { 1158.68f, 62.27f, -315.54f }, { 1158.68f, 61.37f, -315.54f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.67f, 60.47f, -315.54f }, { 1158.67f, 61.37f, -315.54f }, { 1158.68f, 61.37f, -315.54f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.67f, 61.37f, -315.54f }, { 1158.67f, 60.47f, -315.54f }, { 1158.66f, 61.37f, -315.49f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.66f, 61.37f, -315.50f }, { 1158.67f, 60.47f, -315.54f }, { 1158.66f, 60.47f, -315.49f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.54f, 61.37f, -314.91f }, { 1158.66f, 61.37f, -315.49f }, { 1158.66f, 60.47f, -315.49f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.54f, 61.37f, -314.91f }, { 1158.66f, 60.47f, -315.49f }, { 1158.54f, 60.47f, -314.91f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.54f, 61.37f, -314.91f }, { 1158.54f, 60.47f, -314.91f }, { 1158.43f, 60.47f, -314.33f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.43f, 62.27f, -314.28f }, { 1158.42f, 61.37f, -314.28f }, { 1158.43f, 61.37f, -314.28f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.42f, 61.37f, -314.28f }, { 1158.43f, 60.47f, -314.33f }, { 1158.42f, 60.47f, -314.28f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.42f, 60.47f, -314.28f }, { 1158.43f, 61.37f, -314.28f }, { 1158.42f, 61.37f, -314.28f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.43f, 60.47f, -314.33f }, { 1158.43f, 59.57f, -314.33f }, { 1158.42f, 60.47f, -314.28f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.43f, 60.47f, -314.33f }, { 1158.54f, 60.47f, -314.91f }, { 1158.43f, 59.57f, -314.33f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.67f, 60.47f, -315.54f }, { 1158.68f, 61.37f, -315.54f }, { 1158.68f, 60.47f, -315.54f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.67f, 59.57f, -315.54f }, { 1158.67f, 60.47f, -315.54f }, { 1158.68f, 60.47f, -315.54f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.67f, 60.47f, -315.54f }, { 1158.67f, 59.57f, -315.54f }, { 1158.66f, 60.47f, -315.49f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.66f, 60.47f, -315.50f }, { 1158.67f, 59.57f, -315.54f }, { 1158.66f, 59.57f, -315.49f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.54f, 60.47f, -314.91f }, { 1158.66f, 60.47f, -315.49f }, { 1158.66f, 59.57f, -315.49f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.54f, 60.47f, -314.91f }, { 1158.66f, 59.57f, -315.49f }, { 1158.54f, 59.57f, -314.91f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.54f, 60.47f, -314.91f }, { 1158.54f, 59.57f, -314.91f }, { 1158.43f, 59.57f, -314.33f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.43f, 61.37f, -314.28f }, { 1158.42f, 60.47f, -314.28f }, { 1158.43f, 60.47f, -314.28f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.42f, 60.47f, -314.28f }, { 1158.43f, 59.57f, -314.33f }, { 1158.42f, 59.57f, -314.28f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.42f, 59.57f, -314.28f }, { 1158.43f, 60.47f, -314.28f }, { 1158.42f, 60.47f, -314.28f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.43f, 59.57f, -314.33f }, { 1158.43f, 58.67f, -314.33f }, { 1158.42f, 59.57f, -314.28f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.43f, 59.57f, -314.33f }, { 1158.54f, 59.57f, -314.91f }, { 1158.43f, 58.67f, -314.33f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.67f, 59.57f, -315.54f }, { 1158.68f, 60.47f, -315.54f }, { 1158.68f, 59.57f, -315.54f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.67f, 58.67f, -315.54f }, { 1158.67f, 59.57f, -315.54f }, { 1158.68f, 59.57f, -315.54f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.67f, 59.57f, -315.54f }, { 1158.67f, 58.67f, -315.54f }, { 1158.66f, 59.57f, -315.49f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.66f, 59.57f, -315.50f }, { 1158.67f, 58.67f, -315.54f }, { 1158.66f, 58.67f, -315.49f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.54f, 59.57f, -314.91f }, { 1158.66f, 59.57f, -315.49f }, { 1158.66f, 58.67f, -315.49f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.54f, 59.57f, -314.91f }, { 1158.66f, 58.67f, -315.49f }, { 1158.54f, 58.67f, -314.91f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.54f, 59.57f, -314.91f }, { 1158.54f, 58.67f, -314.91f }, { 1158.43f, 58.67f, -314.33f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.43f, 60.47f, -314.28f }, { 1158.42f, 59.57f, -314.28f }, { 1158.43f, 59.57f, -314.28f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.42f, 59.57f, -314.28f }, { 1158.43f, 58.67f, -314.33f }, { 1158.42f, 58.67f, -314.28f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.42f, 58.67f, -314.28f }, { 1158.43f, 59.57f, -314.28f }, { 1158.42f, 59.57f, -314.28f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.43f, 58.67f, -314.33f }, { 1158.43f, 57.76f, -314.33f }, { 1158.42f, 58.67f, -314.28f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.43f, 58.67f, -314.33f }, { 1158.54f, 57.76f, -314.91f }, { 1158.43f, 57.76f, -314.33f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.43f, 58.67f, -314.33f }, { 1158.54f, 58.67f, -314.91f }, { 1158.54f, 57.76f, -314.91f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.54f, 58.67f, -314.91f }, { 1158.66f, 58.67f, -315.49f }, { 1158.66f, 57.76f, -315.49f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.54f, 58.67f, -314.91f }, { 1158.66f, 57.76f, -315.49f }, { 1158.54f, 57.76f, -314.91f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.67f, 58.67f, -315.54f }, { 1158.67f, 57.76f, -315.54f }, { 1158.66f, 58.67f, -315.49f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.66f, 58.67f, -315.50f }, { 1158.67f, 57.76f, -315.54f }, { 1158.66f, 57.76f, -315.49f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.67f, 58.67f, -315.54f }, { 1158.68f, 59.57f, -315.54f }, { 1158.68f, 58.67f, -315.54f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.67f, 57.76f, -315.54f }, { 1158.67f, 58.67f, -315.54f }, { 1158.68f, 58.67f, -315.54f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.43f, 59.57f, -314.28f }, { 1158.42f, 58.67f, -314.28f }, { 1158.43f, 58.67f, -314.28f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.42f, 58.67f, -314.28f }, { 1158.43f, 57.76f, -314.33f }, { 1158.42f, 57.76f, -314.28f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.42f, 57.76f, -314.28f }, { 1158.43f, 58.67f, -314.28f }, { 1158.42f, 58.67f, -314.28f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.67f, 57.76f, -315.54f }, { 1158.68f, 58.67f, -315.54f }, { 1158.68f, 57.76f, -315.54f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.43f, 58.67f, -314.28f }, { 1158.42f, 57.76f, -314.28f }, { 1158.43f, 57.76f, -314.28f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.76f, 65.82f, -315.59f }, { 1158.70f, 65.58f, -315.57f }, { 1158.75f, 65.83f, -315.59f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.70f, 65.58f, -315.57f }, { 1158.76f, 65.82f, -315.59f }, { 1158.71f, 65.57f, -315.57f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.71f, 65.57f, -315.57f }, { 1158.68f, 65.29f, -315.55f }, { 1158.70f, 65.58f, -315.57f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.68f, 65.29f, -315.55f }, { 1158.71f, 65.57f, -315.57f }, { 1158.69f, 65.28f, -315.55f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.69f, 65.28f, -315.55f }, { 1158.67f, 64.98f, -315.54f }, { 1158.68f, 65.29f, -315.55f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.67f, 64.98f, -315.54f }, { 1158.69f, 65.28f, -315.55f }, { 1158.68f, 64.97f, -315.54f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.74f, 66.28f, -314.05f }, { 1158.64f, 66.23f, -314.11f }, { 1158.64f, 66.20f, -314.10f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.64f, 66.23f, -314.11f }, { 1158.74f, 66.28f, -314.05f }, { 1158.73f, 66.31f, -314.05f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.64f, 66.23f, -314.11f }, { 1158.54f, 66.06f, -314.16f }, { 1158.64f, 66.20f, -314.10f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.55f, 66.04f, -314.15f }, { 1158.64f, 66.20f, -314.10f }, { 1158.54f, 66.06f, -314.16f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.47f, 65.83f, -314.20f }, { 1158.55f, 66.04f, -314.15f }, { 1158.54f, 66.06f, -314.16f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.55f, 66.04f, -314.15f }, { 1158.47f, 65.83f, -314.20f }, { 1158.48f, 65.82f, -314.20f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.47f, 65.83f, -314.20f }, { 1158.44f, 65.58f, -314.24f }, { 1158.48f, 65.82f, -314.20f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.48f, 65.82f, -314.20f }, { 1158.44f, 65.58f, -314.24f }, { 1158.45f, 65.57f, -314.24f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.85f, 66.36f, -314.01f }, { 1158.73f, 66.31f, -314.05f }, { 1158.85f, 66.33f, -314.01f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.74f, 66.28f, -314.05f }, { 1158.85f, 66.33f, -314.01f }, { 1158.73f, 66.31f, -314.05f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.43f, 65.29f, -314.27f }, { 1158.45f, 65.57f, -314.24f }, { 1158.44f, 65.58f, -314.24f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.45f, 65.57f, -314.24f }, { 1158.43f, 65.29f, -314.27f }, { 1158.44f, 65.28f, -314.27f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.42f, 64.98f, -314.28f }, { 1158.44f, 65.28f, -314.27f }, { 1158.43f, 65.29f, -314.27f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.44f, 65.28f, -314.27f }, { 1158.42f, 64.98f, -314.28f }, { 1158.43f, 64.97f, -314.28f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.97f, 66.40f, -314.03f }, { 1158.85f, 66.36f, -314.02f }, { 1158.96f, 66.40f, -313.98f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.85f, 66.36f, -314.01f }, { 1158.96f, 66.36f, -313.98f }, { 1158.96f, 66.40f, -313.98f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.96f, 66.36f, -313.98f }, { 1158.85f, 66.36f, -314.02f }, { 1158.85f, 66.33f, -314.01f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.96f, 66.40f, -313.98f }, { 1159.77f, 66.40f, -313.82f }, { 1158.97f, 66.40f, -314.03f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.96f, 66.36f, -313.98f }, { 1159.77f, 66.40f, -313.82f }, { 1158.96f, 66.40f, -313.98f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1159.77f, 66.40f, -313.82f }, { 1158.96f, 66.36f, -313.98f }, { 1159.78f, 66.36f, -313.82f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.97f, 66.40f, -314.03f }, { 1159.77f, 66.40f, -313.82f }, { 1159.78f, 66.40f, -313.87f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1159.94f, 66.40f, -314.64f }, { 1158.97f, 66.40f, -314.03f }, { 1159.78f, 66.40f, -313.87f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1158.97f, 66.40f, -314.03f }, { 1159.94f, 66.40f, -314.64f }, { 1159.12f, 66.40f, -314.80f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1159.12f, 66.40f, -314.80f }, { 1159.94f, 66.40f, -314.64f }, { 1159.27f, 66.40f, -315.57f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1159.27f, 66.40f, -315.57f }, { 1159.94f, 66.40f, -314.64f }, { 1160.09f, 66.40f, -315.41f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1159.16f, 66.36f, -315.58f }, { 1159.12f, 66.40f, -314.80f }, { 1159.27f, 66.40f, -315.57f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1159.16f, 66.36f, -315.58f }, { 1159.27f, 66.40f, -315.57f }, { 1159.17f, 66.36f, -315.63f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1160.09f, 66.40f, -315.41f }, { 1159.28f, 66.40f, -315.62f }, { 1159.27f, 66.40f, -315.57f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1159.17f, 66.36f, -315.63f }, { 1159.27f, 66.40f, -315.57f }, { 1159.28f, 66.40f, -315.62f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1159.28f, 66.40f, -315.62f }, { 1160.09f, 66.40f, -315.41f }, { 1160.10f, 66.40f, -315.46f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1159.05f, 66.31f, -315.63f }, { 1159.16f, 66.36f, -315.58f }, { 1159.17f, 66.36f, -315.63f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1159.17f, 66.36f, -315.63f }, { 1159.28f, 66.40f, -315.62f }, { 1159.28f, 66.36f, -315.62f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1159.28f, 66.40f, -315.62f }, { 1160.10f, 66.40f, -315.46f }, { 1159.28f, 66.36f, -315.62f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1159.28f, 66.36f, -315.62f }, { 1160.10f, 66.40f, -315.46f }, { 1160.10f, 66.36f, -315.46f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1159.28f, 66.36f, -315.62f }, { 1159.17f, 66.33f, -315.63f }, { 1159.17f, 66.36f, -315.63f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1159.05f, 66.31f, -315.63f }, { 1159.17f, 66.36f, -315.63f }, { 1159.17f, 66.33f, -315.63f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1159.17f, 66.33f, -315.63f }, { 1159.05f, 66.28f, -315.63f }, { 1159.05f, 66.31f, -315.63f }, 57.76f, 0.00f },   // waterfeature_01 props 659
    { { 1155.99f, 54.18f, -246.89f }, { 1156.01f, 54.01f, -246.83f }, { 1156.03f, 54.18f, -246.88f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1156.01f, 54.01f, -246.83f }, { 1156.05f, 54.01f, -246.82f }, { 1156.03f, 54.18f, -246.88f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1156.02f, 54.41f, -247.14f }, { 1156.02f, 54.44f, -247.23f }, { 1155.98f, 54.41f, -247.15f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1155.98f, 54.41f, -247.15f }, { 1156.02f, 54.44f, -247.23f }, { 1155.99f, 54.44f, -247.24f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1156.02f, 53.81f, -246.79f }, { 1156.04f, 53.59f, -246.78f }, { 1156.06f, 53.81f, -246.79f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1156.04f, 53.59f, -246.78f }, { 1156.08f, 53.59f, -246.77f }, { 1156.06f, 53.81f, -246.79f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1156.02f, 54.31f, -246.96f }, { 1156.01f, 54.37f, -247.05f }, { 1155.99f, 54.31f, -246.97f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1155.99f, 54.31f, -246.97f }, { 1156.01f, 54.37f, -247.05f }, { 1155.98f, 54.37f, -247.05f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1156.08f, 53.59f, -246.77f }, { 1156.53f, 53.81f, -246.69f }, { 1156.06f, 53.81f, -246.79f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1156.53f, 53.81f, -246.69f }, { 1156.08f, 53.59f, -246.77f }, { 1156.53f, 53.59f, -246.68f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1156.53f, 53.59f, -246.68f }, { 1156.99f, 53.59f, -246.59f }, { 1156.53f, 53.81f, -246.69f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1157.01f, 53.81f, -246.60f }, { 1156.53f, 53.81f, -246.69f }, { 1156.99f, 53.59f, -246.59f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1156.05f, 54.01f, -246.82f }, { 1156.55f, 54.18f, -246.78f }, { 1156.03f, 54.18f, -246.88f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1156.55f, 54.18f, -246.78f }, { 1156.05f, 54.01f, -246.82f }, { 1156.54f, 54.01f, -246.72f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1156.54f, 54.01f, -246.72f }, { 1157.04f, 54.01f, -246.63f }, { 1156.55f, 54.18f, -246.78f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1157.07f, 54.18f, -246.68f }, { 1156.55f, 54.18f, -246.78f }, { 1157.04f, 54.01f, -246.63f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1156.02f, 54.31f, -246.96f }, { 1156.58f, 54.37f, -246.94f }, { 1156.01f, 54.37f, -247.05f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1156.58f, 54.38f, -246.94f }, { 1156.02f, 54.31f, -246.96f }, { 1156.57f, 54.31f, -246.86f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1157.11f, 54.31f, -246.75f }, { 1156.58f, 54.37f, -246.94f }, { 1156.57f, 54.31f, -246.86f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1156.58f, 54.38f, -246.94f }, { 1157.11f, 54.31f, -246.75f }, { 1157.15f, 54.37f, -246.83f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1156.02f, 54.41f, -247.14f }, { 1156.62f, 54.44f, -247.11f }, { 1156.02f, 54.44f, -247.23f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1156.62f, 54.44f, -247.12f }, { 1156.02f, 54.41f, -247.14f }, { 1156.60f, 54.41f, -247.03f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1157.18f, 54.41f, -246.92f }, { 1156.62f, 54.44f, -247.11f }, { 1156.60f, 54.41f, -247.03f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1156.62f, 54.44f, -247.12f }, { 1157.18f, 54.41f, -246.92f }, { 1157.21f, 54.44f, -247.00f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1157.02f, 53.59f, -246.59f }, { 1157.05f, 53.81f, -246.60f }, { 1157.01f, 53.81f, -246.60f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1157.01f, 53.81f, -246.60f }, { 1156.99f, 53.59f, -246.59f }, { 1157.02f, 53.59f, -246.59f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1157.07f, 54.01f, -246.62f }, { 1157.11f, 54.18f, -246.67f }, { 1157.07f, 54.18f, -246.68f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1157.07f, 54.18f, -246.68f }, { 1157.04f, 54.01f, -246.63f }, { 1157.07f, 54.01f, -246.62f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1157.15f, 54.31f, -246.75f }, { 1157.15f, 54.37f, -246.83f }, { 1157.11f, 54.31f, -246.75f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1157.15f, 54.38f, -246.83f }, { 1157.15f, 54.31f, -246.75f }, { 1157.19f, 54.37f, -246.82f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1157.22f, 54.41f, -246.91f }, { 1157.21f, 54.44f, -247.00f }, { 1157.18f, 54.41f, -246.92f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1157.21f, 54.44f, -247.00f }, { 1157.22f, 54.41f, -246.91f }, { 1157.25f, 54.44f, -246.99f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1155.98f, 54.35f, -247.06f }, { 1155.99f, 54.29f, -246.98f }, { 1155.99f, 54.31f, -246.97f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1155.99f, 54.31f, -246.97f }, { 1155.98f, 54.37f, -247.05f }, { 1155.98f, 54.35f, -247.06f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1157.19f, 54.35f, -246.82f }, { 1157.15f, 54.31f, -246.75f }, { 1157.15f, 54.29f, -246.75f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1157.15f, 54.31f, -246.75f }, { 1157.19f, 54.35f, -246.82f }, { 1157.19f, 54.37f, -246.82f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1156.03f, 53.81f, -246.80f }, { 1156.04f, 53.59f, -246.78f }, { 1156.02f, 53.81f, -246.79f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1156.04f, 53.59f, -246.78f }, { 1156.03f, 53.81f, -246.80f }, { 1156.04f, 53.59f, -246.78f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1157.05f, 53.81f, -246.60f }, { 1157.02f, 53.59f, -246.59f }, { 1157.03f, 53.59f, -246.60f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1157.02f, 53.59f, -246.59f }, { 1157.05f, 53.81f, -246.60f }, { 1157.05f, 53.81f, -246.60f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1156.00f, 54.17f, -246.89f }, { 1156.01f, 54.00f, -246.83f }, { 1156.01f, 54.01f, -246.83f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1156.01f, 54.01f, -246.83f }, { 1155.99f, 54.18f, -246.89f }, { 1156.00f, 54.17f, -246.89f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1157.11f, 54.17f, -246.68f }, { 1157.07f, 54.01f, -246.62f }, { 1157.07f, 54.00f, -246.63f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1157.07f, 54.01f, -246.62f }, { 1157.11f, 54.17f, -246.68f }, { 1157.11f, 54.18f, -246.67f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1155.99f, 54.41f, -247.24f }, { 1155.98f, 54.39f, -247.15f }, { 1155.98f, 54.41f, -247.15f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1155.98f, 54.41f, -247.15f }, { 1155.99f, 54.44f, -247.24f }, { 1155.99f, 54.41f, -247.24f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1157.24f, 54.41f, -246.99f }, { 1157.22f, 54.41f, -246.91f }, { 1157.22f, 54.39f, -246.91f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1157.22f, 54.41f, -246.91f }, { 1157.25f, 54.41f, -246.99f }, { 1157.25f, 54.44f, -246.99f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1155.99f, 54.29f, -246.97f }, { 1156.00f, 54.17f, -246.89f }, { 1155.99f, 54.18f, -246.89f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1155.99f, 54.18f, -246.89f }, { 1155.99f, 54.31f, -246.97f }, { 1155.99f, 54.29f, -246.98f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1155.99f, 54.31f, -246.97f }, { 1155.99f, 54.18f, -246.89f }, { 1156.02f, 54.31f, -246.96f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1155.99f, 54.18f, -246.89f }, { 1156.03f, 54.18f, -246.88f }, { 1156.02f, 54.31f, -246.96f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1156.57f, 54.31f, -246.86f }, { 1156.02f, 54.31f, -246.96f }, { 1156.03f, 54.18f, -246.88f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1156.57f, 54.31f, -246.86f }, { 1156.03f, 54.18f, -246.88f }, { 1156.55f, 54.18f, -246.78f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1157.07f, 54.18f, -246.68f }, { 1156.57f, 54.31f, -246.86f }, { 1156.55f, 54.18f, -246.78f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1156.57f, 54.31f, -246.86f }, { 1157.07f, 54.18f, -246.68f }, { 1157.11f, 54.31f, -246.75f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1157.11f, 54.31f, -246.75f }, { 1157.11f, 54.18f, -246.67f }, { 1157.15f, 54.31f, -246.75f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1157.11f, 54.18f, -246.67f }, { 1157.11f, 54.31f, -246.75f }, { 1157.07f, 54.18f, -246.68f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1157.15f, 54.31f, -246.75f }, { 1157.11f, 54.18f, -246.67f }, { 1157.15f, 54.29f, -246.75f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1157.11f, 54.17f, -246.68f }, { 1157.15f, 54.29f, -246.75f }, { 1157.11f, 54.18f, -246.67f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1156.01f, 54.00f, -246.84f }, { 1156.02f, 53.81f, -246.79f }, { 1156.01f, 54.01f, -246.83f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1156.02f, 53.81f, -246.79f }, { 1156.01f, 54.00f, -246.83f }, { 1156.03f, 53.81f, -246.80f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1156.01f, 54.01f, -246.83f }, { 1156.02f, 53.81f, -246.79f }, { 1156.05f, 54.01f, -246.82f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1156.02f, 53.81f, -246.79f }, { 1156.06f, 53.81f, -246.79f }, { 1156.05f, 54.01f, -246.82f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1156.54f, 54.01f, -246.72f }, { 1156.05f, 54.01f, -246.82f }, { 1156.06f, 53.81f, -246.79f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1156.54f, 54.01f, -246.72f }, { 1156.06f, 53.81f, -246.79f }, { 1156.53f, 53.81f, -246.69f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1157.01f, 53.81f, -246.60f }, { 1156.54f, 54.01f, -246.72f }, { 1156.53f, 53.81f, -246.69f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1156.54f, 54.01f, -246.72f }, { 1157.01f, 53.81f, -246.60f }, { 1157.04f, 54.01f, -246.63f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1157.04f, 54.01f, -246.63f }, { 1157.05f, 53.81f, -246.60f }, { 1157.07f, 54.01f, -246.62f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1157.05f, 53.81f, -246.59f }, { 1157.04f, 54.01f, -246.63f }, { 1157.01f, 53.81f, -246.60f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1157.07f, 54.01f, -246.62f }, { 1157.05f, 53.81f, -246.60f }, { 1157.07f, 54.00f, -246.63f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1157.07f, 54.00f, -246.63f }, { 1157.05f, 53.81f, -246.60f }, { 1157.05f, 53.81f, -246.60f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1155.98f, 54.39f, -247.15f }, { 1155.98f, 54.35f, -247.06f }, { 1155.98f, 54.37f, -247.05f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1155.98f, 54.38f, -247.06f }, { 1155.98f, 54.41f, -247.15f }, { 1155.98f, 54.39f, -247.15f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1156.01f, 54.38f, -247.05f }, { 1156.02f, 54.41f, -247.14f }, { 1155.98f, 54.37f, -247.05f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1155.98f, 54.38f, -247.06f }, { 1156.02f, 54.41f, -247.14f }, { 1155.98f, 54.41f, -247.15f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1156.01f, 54.38f, -247.05f }, { 1156.60f, 54.41f, -247.03f }, { 1156.02f, 54.41f, -247.14f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1156.01f, 54.38f, -247.05f }, { 1156.58f, 54.37f, -246.94f }, { 1156.60f, 54.41f, -247.03f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1157.15f, 54.38f, -246.83f }, { 1156.60f, 54.41f, -247.03f }, { 1156.58f, 54.37f, -246.94f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1156.60f, 54.41f, -247.03f }, { 1157.15f, 54.37f, -246.83f }, { 1157.18f, 54.41f, -246.92f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1157.19f, 54.38f, -246.82f }, { 1157.18f, 54.41f, -246.92f }, { 1157.15f, 54.37f, -246.83f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1157.18f, 54.41f, -246.92f }, { 1157.19f, 54.37f, -246.82f }, { 1157.22f, 54.41f, -246.91f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1157.22f, 54.41f, -246.91f }, { 1157.19f, 54.37f, -246.82f }, { 1157.22f, 54.39f, -246.91f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1157.19f, 54.35f, -246.82f }, { 1157.22f, 54.39f, -246.91f }, { 1157.19f, 54.37f, -246.82f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1156.14f, 54.44f, -247.85f }, { 1155.99f, 54.44f, -247.24f }, { 1156.02f, 54.44f, -247.23f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1155.99f, 54.44f, -247.24f }, { 1156.14f, 54.44f, -247.85f }, { 1156.11f, 54.44f, -247.86f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1156.03f, 54.44f, -247.23f }, { 1156.74f, 54.44f, -247.74f }, { 1156.14f, 54.44f, -247.85f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1156.62f, 54.44f, -247.12f }, { 1156.74f, 54.44f, -247.74f }, { 1156.02f, 54.44f, -247.23f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1157.21f, 54.44f, -247.00f }, { 1156.74f, 54.44f, -247.74f }, { 1156.62f, 54.44f, -247.11f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1156.74f, 54.44f, -247.74f }, { 1157.21f, 54.44f, -247.00f }, { 1157.33f, 54.44f, -247.62f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1157.24f, 54.44f, -246.99f }, { 1157.37f, 54.44f, -247.62f }, { 1157.21f, 54.44f, -247.00f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1157.21f, 54.44f, -247.00f }, { 1157.37f, 54.44f, -247.62f }, { 1157.33f, 54.44f, -247.62f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1155.99f, 54.44f, -247.24f }, { 1156.11f, 54.44f, -247.86f }, { 1155.99f, 54.41f, -247.24f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1155.99f, 54.41f, -247.24f }, { 1156.11f, 54.44f, -247.86f }, { 1156.11f, 54.41f, -247.86f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1157.24f, 54.41f, -246.99f }, { 1157.37f, 54.44f, -247.62f }, { 1157.25f, 54.44f, -246.99f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1157.37f, 54.44f, -247.62f }, { 1157.25f, 54.41f, -246.99f }, { 1157.37f, 54.41f, -247.62f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1148.45f, 54.18f, -248.34f }, { 1148.47f, 54.01f, -248.28f }, { 1148.49f, 54.18f, -248.34f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1148.47f, 54.01f, -248.28f }, { 1148.50f, 54.01f, -248.28f }, { 1148.49f, 54.18f, -248.34f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1148.48f, 54.41f, -248.60f }, { 1148.48f, 54.44f, -248.68f }, { 1148.44f, 54.41f, -248.60f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1148.44f, 54.41f, -248.60f }, { 1148.48f, 54.44f, -248.68f }, { 1148.45f, 54.44f, -248.69f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1148.48f, 53.81f, -248.25f }, { 1148.50f, 53.59f, -248.23f }, { 1148.52f, 53.81f, -248.24f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1148.50f, 53.59f, -248.23f }, { 1148.54f, 53.59f, -248.22f }, { 1148.52f, 53.81f, -248.24f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1148.48f, 54.31f, -248.42f }, { 1148.47f, 54.37f, -248.50f }, { 1148.45f, 54.31f, -248.43f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1148.45f, 54.31f, -248.43f }, { 1148.47f, 54.37f, -248.50f }, { 1148.44f, 54.37f, -248.51f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1148.54f, 53.59f, -248.22f }, { 1148.99f, 53.81f, -248.15f }, { 1148.52f, 53.81f, -248.24f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1148.99f, 53.81f, -248.15f }, { 1148.54f, 53.59f, -248.22f }, { 1148.99f, 53.59f, -248.14f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1148.99f, 53.59f, -248.14f }, { 1149.45f, 53.59f, -248.05f }, { 1148.99f, 53.81f, -248.15f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1149.47f, 53.81f, -248.06f }, { 1148.99f, 53.81f, -248.15f }, { 1149.45f, 53.59f, -248.05f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1148.51f, 54.01f, -248.28f }, { 1149.01f, 54.18f, -248.24f }, { 1148.49f, 54.18f, -248.34f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1149.01f, 54.18f, -248.24f }, { 1148.50f, 54.01f, -248.28f }, { 1149.00f, 54.01f, -248.18f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1149.00f, 54.01f, -248.18f }, { 1149.49f, 54.01f, -248.09f }, { 1149.01f, 54.18f, -248.24f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1149.53f, 54.18f, -248.14f }, { 1149.01f, 54.18f, -248.24f }, { 1149.49f, 54.01f, -248.09f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1148.48f, 54.31f, -248.42f }, { 1149.04f, 54.37f, -248.39f }, { 1148.47f, 54.37f, -248.50f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1149.04f, 54.38f, -248.39f }, { 1148.48f, 54.31f, -248.42f }, { 1149.03f, 54.31f, -248.31f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1149.57f, 54.31f, -248.21f }, { 1149.04f, 54.37f, -248.39f }, { 1149.03f, 54.31f, -248.31f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1149.04f, 54.38f, -248.39f }, { 1149.57f, 54.31f, -248.21f }, { 1149.61f, 54.37f, -248.28f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1148.48f, 54.41f, -248.60f }, { 1149.07f, 54.44f, -248.57f }, { 1148.48f, 54.44f, -248.68f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1149.08f, 54.44f, -248.57f }, { 1148.48f, 54.41f, -248.60f }, { 1149.06f, 54.41f, -248.48f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1149.64f, 54.41f, -248.37f }, { 1149.07f, 54.44f, -248.57f }, { 1149.06f, 54.41f, -248.48f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1149.08f, 54.44f, -248.57f }, { 1149.64f, 54.41f, -248.37f }, { 1149.67f, 54.44f, -248.46f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1149.48f, 53.59f, -248.04f }, { 1149.51f, 53.81f, -248.05f }, { 1149.47f, 53.81f, -248.06f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1149.47f, 53.81f, -248.06f }, { 1149.45f, 53.59f, -248.05f }, { 1149.48f, 53.59f, -248.04f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1149.53f, 54.01f, -248.08f }, { 1149.57f, 54.18f, -248.13f }, { 1149.53f, 54.18f, -248.14f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1149.53f, 54.18f, -248.14f }, { 1149.49f, 54.01f, -248.09f }, { 1149.53f, 54.01f, -248.08f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1149.61f, 54.31f, -248.20f }, { 1149.61f, 54.37f, -248.28f }, { 1149.57f, 54.31f, -248.21f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1149.61f, 54.38f, -248.28f }, { 1149.61f, 54.31f, -248.20f }, { 1149.65f, 54.37f, -248.28f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1149.68f, 54.41f, -248.37f }, { 1149.67f, 54.44f, -248.46f }, { 1149.64f, 54.41f, -248.37f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1149.67f, 54.44f, -248.46f }, { 1149.68f, 54.41f, -248.37f }, { 1149.70f, 54.44f, -248.45f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1148.44f, 54.35f, -248.51f }, { 1148.45f, 54.29f, -248.43f }, { 1148.45f, 54.31f, -248.43f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1148.45f, 54.31f, -248.43f }, { 1148.44f, 54.37f, -248.51f }, { 1148.44f, 54.35f, -248.51f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1149.65f, 54.35f, -248.28f }, { 1149.61f, 54.31f, -248.20f }, { 1149.61f, 54.29f, -248.21f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1149.61f, 54.31f, -248.20f }, { 1149.65f, 54.35f, -248.28f }, { 1149.65f, 54.37f, -248.28f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1148.48f, 53.81f, -248.26f }, { 1148.50f, 53.59f, -248.23f }, { 1148.48f, 53.81f, -248.25f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1148.50f, 53.59f, -248.23f }, { 1148.49f, 53.81f, -248.26f }, { 1148.50f, 53.59f, -248.24f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1149.51f, 53.81f, -248.06f }, { 1149.48f, 53.59f, -248.04f }, { 1149.48f, 53.59f, -248.05f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1149.48f, 53.59f, -248.04f }, { 1149.51f, 53.81f, -248.06f }, { 1149.51f, 53.81f, -248.05f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1148.45f, 54.17f, -248.35f }, { 1148.47f, 54.00f, -248.29f }, { 1148.47f, 54.01f, -248.28f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1148.47f, 54.01f, -248.28f }, { 1148.45f, 54.18f, -248.34f }, { 1148.46f, 54.17f, -248.35f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1149.57f, 54.17f, -248.13f }, { 1149.53f, 54.01f, -248.08f }, { 1149.53f, 54.00f, -248.09f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1149.53f, 54.01f, -248.08f }, { 1149.57f, 54.17f, -248.14f }, { 1149.57f, 54.18f, -248.13f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1148.45f, 54.41f, -248.69f }, { 1148.44f, 54.39f, -248.61f }, { 1148.44f, 54.41f, -248.60f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1148.44f, 54.41f, -248.60f }, { 1148.45f, 54.44f, -248.69f }, { 1148.45f, 54.41f, -248.69f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1149.70f, 54.41f, -248.45f }, { 1149.68f, 54.41f, -248.37f }, { 1149.68f, 54.39f, -248.37f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1149.68f, 54.41f, -248.37f }, { 1149.70f, 54.41f, -248.45f }, { 1149.70f, 54.44f, -248.45f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1148.45f, 54.29f, -248.43f }, { 1148.46f, 54.17f, -248.35f }, { 1148.45f, 54.18f, -248.34f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1148.45f, 54.18f, -248.34f }, { 1148.45f, 54.31f, -248.43f }, { 1148.45f, 54.29f, -248.43f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1148.45f, 54.31f, -248.43f }, { 1148.45f, 54.18f, -248.34f }, { 1148.48f, 54.31f, -248.42f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1148.45f, 54.18f, -248.34f }, { 1148.49f, 54.18f, -248.34f }, { 1148.48f, 54.31f, -248.42f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1149.03f, 54.31f, -248.31f }, { 1148.48f, 54.31f, -248.42f }, { 1148.49f, 54.18f, -248.34f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1149.03f, 54.31f, -248.31f }, { 1148.49f, 54.18f, -248.34f }, { 1149.01f, 54.18f, -248.24f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1149.53f, 54.18f, -248.14f }, { 1149.03f, 54.31f, -248.31f }, { 1149.01f, 54.18f, -248.24f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1149.03f, 54.31f, -248.31f }, { 1149.53f, 54.18f, -248.14f }, { 1149.57f, 54.31f, -248.21f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1149.57f, 54.31f, -248.21f }, { 1149.57f, 54.18f, -248.13f }, { 1149.61f, 54.31f, -248.20f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1149.57f, 54.18f, -248.13f }, { 1149.57f, 54.31f, -248.21f }, { 1149.53f, 54.18f, -248.14f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1149.61f, 54.31f, -248.20f }, { 1149.57f, 54.18f, -248.13f }, { 1149.61f, 54.29f, -248.21f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1149.57f, 54.17f, -248.13f }, { 1149.61f, 54.29f, -248.21f }, { 1149.57f, 54.18f, -248.13f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1148.47f, 54.00f, -248.29f }, { 1148.48f, 53.81f, -248.25f }, { 1148.47f, 54.01f, -248.28f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1148.48f, 53.81f, -248.25f }, { 1148.47f, 54.00f, -248.29f }, { 1148.49f, 53.81f, -248.26f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1148.47f, 54.01f, -248.28f }, { 1148.48f, 53.81f, -248.25f }, { 1148.50f, 54.01f, -248.28f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1148.48f, 53.81f, -248.25f }, { 1148.52f, 53.81f, -248.24f }, { 1148.50f, 54.01f, -248.28f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1149.00f, 54.01f, -248.18f }, { 1148.50f, 54.01f, -248.28f }, { 1148.52f, 53.81f, -248.24f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1149.00f, 54.01f, -248.18f }, { 1148.52f, 53.81f, -248.24f }, { 1148.99f, 53.81f, -248.15f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1149.47f, 53.81f, -248.06f }, { 1149.00f, 54.01f, -248.18f }, { 1148.99f, 53.81f, -248.15f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1149.00f, 54.01f, -248.18f }, { 1149.47f, 53.81f, -248.06f }, { 1149.49f, 54.01f, -248.09f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1149.49f, 54.01f, -248.09f }, { 1149.51f, 53.81f, -248.05f }, { 1149.53f, 54.01f, -248.08f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1149.51f, 53.81f, -248.05f }, { 1149.49f, 54.01f, -248.09f }, { 1149.47f, 53.81f, -248.06f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1149.53f, 54.01f, -248.08f }, { 1149.51f, 53.81f, -248.05f }, { 1149.53f, 54.00f, -248.09f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1149.53f, 54.00f, -248.09f }, { 1149.51f, 53.81f, -248.05f }, { 1149.51f, 53.81f, -248.06f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1148.44f, 54.39f, -248.61f }, { 1148.44f, 54.35f, -248.51f }, { 1148.44f, 54.37f, -248.51f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1148.44f, 54.38f, -248.51f }, { 1148.44f, 54.41f, -248.60f }, { 1148.44f, 54.39f, -248.61f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1148.47f, 54.38f, -248.50f }, { 1148.48f, 54.41f, -248.60f }, { 1148.44f, 54.37f, -248.51f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1148.44f, 54.38f, -248.51f }, { 1148.48f, 54.41f, -248.60f }, { 1148.44f, 54.41f, -248.60f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1148.47f, 54.38f, -248.50f }, { 1149.06f, 54.41f, -248.48f }, { 1148.48f, 54.41f, -248.60f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1148.47f, 54.38f, -248.50f }, { 1149.04f, 54.37f, -248.39f }, { 1149.06f, 54.41f, -248.48f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1149.61f, 54.38f, -248.28f }, { 1149.06f, 54.41f, -248.48f }, { 1149.04f, 54.37f, -248.39f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1149.06f, 54.41f, -248.49f }, { 1149.61f, 54.37f, -248.28f }, { 1149.64f, 54.41f, -248.37f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1149.65f, 54.38f, -248.28f }, { 1149.64f, 54.41f, -248.37f }, { 1149.61f, 54.37f, -248.28f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1149.64f, 54.41f, -248.37f }, { 1149.65f, 54.37f, -248.28f }, { 1149.68f, 54.41f, -248.37f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1149.68f, 54.41f, -248.37f }, { 1149.65f, 54.37f, -248.28f }, { 1149.68f, 54.39f, -248.37f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1149.65f, 54.35f, -248.28f }, { 1149.68f, 54.39f, -248.37f }, { 1149.65f, 54.37f, -248.28f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1148.60f, 54.44f, -249.31f }, { 1148.45f, 54.44f, -248.69f }, { 1148.48f, 54.44f, -248.68f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1148.45f, 54.44f, -248.69f }, { 1148.60f, 54.44f, -249.31f }, { 1148.57f, 54.44f, -249.32f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1148.48f, 54.44f, -248.68f }, { 1149.20f, 54.44f, -249.19f }, { 1148.60f, 54.44f, -249.31f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1149.08f, 54.44f, -248.57f }, { 1149.20f, 54.44f, -249.19f }, { 1148.48f, 54.44f, -248.68f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1149.67f, 54.44f, -248.46f }, { 1149.20f, 54.44f, -249.19f }, { 1149.07f, 54.44f, -248.57f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1149.19f, 54.44f, -249.19f }, { 1149.67f, 54.44f, -248.46f }, { 1149.79f, 54.44f, -249.08f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1149.70f, 54.44f, -248.45f }, { 1149.82f, 54.44f, -249.07f }, { 1149.67f, 54.44f, -248.46f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1149.67f, 54.44f, -248.46f }, { 1149.82f, 54.44f, -249.07f }, { 1149.79f, 54.44f, -249.08f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1148.45f, 54.44f, -248.69f }, { 1148.57f, 54.44f, -249.32f }, { 1148.45f, 54.41f, -248.69f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1148.45f, 54.41f, -248.69f }, { 1148.57f, 54.44f, -249.32f }, { 1148.57f, 54.41f, -249.32f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1149.70f, 54.41f, -248.45f }, { 1149.82f, 54.44f, -249.07f }, { 1149.70f, 54.44f, -248.45f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1149.83f, 54.44f, -249.07f }, { 1149.70f, 54.41f, -248.45f }, { 1149.83f, 54.41f, -249.07f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1152.22f, 54.18f, -247.62f }, { 1152.24f, 54.01f, -247.56f }, { 1152.26f, 54.18f, -247.61f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1152.24f, 54.01f, -247.56f }, { 1152.28f, 54.01f, -247.55f }, { 1152.26f, 54.18f, -247.61f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1152.25f, 54.41f, -247.87f }, { 1152.25f, 54.44f, -247.96f }, { 1152.21f, 54.41f, -247.88f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1152.21f, 54.41f, -247.88f }, { 1152.25f, 54.44f, -247.96f }, { 1152.22f, 54.44f, -247.96f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1152.25f, 53.81f, -247.52f }, { 1152.27f, 53.59f, -247.50f }, { 1152.29f, 53.81f, -247.51f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1152.27f, 53.59f, -247.50f }, { 1152.31f, 53.59f, -247.50f }, { 1152.29f, 53.81f, -247.51f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1152.25f, 54.31f, -247.69f }, { 1152.24f, 54.37f, -247.78f }, { 1152.22f, 54.31f, -247.70f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1152.22f, 54.31f, -247.70f }, { 1152.24f, 54.37f, -247.78f }, { 1152.21f, 54.37f, -247.78f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1152.31f, 53.59f, -247.50f }, { 1152.76f, 53.81f, -247.42f }, { 1152.29f, 53.81f, -247.51f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1152.76f, 53.81f, -247.42f }, { 1152.31f, 53.59f, -247.50f }, { 1152.76f, 53.59f, -247.41f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1152.76f, 53.59f, -247.41f }, { 1153.22f, 53.59f, -247.32f }, { 1152.76f, 53.81f, -247.42f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1153.24f, 53.81f, -247.33f }, { 1152.76f, 53.81f, -247.42f }, { 1153.22f, 53.59f, -247.32f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1152.28f, 54.01f, -247.55f }, { 1152.78f, 54.18f, -247.51f }, { 1152.26f, 54.18f, -247.61f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1152.78f, 54.18f, -247.51f }, { 1152.28f, 54.01f, -247.55f }, { 1152.77f, 54.01f, -247.45f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1152.77f, 54.01f, -247.45f }, { 1153.26f, 54.01f, -247.36f }, { 1152.78f, 54.18f, -247.51f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1153.30f, 54.18f, -247.41f }, { 1152.78f, 54.18f, -247.51f }, { 1153.26f, 54.01f, -247.36f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1152.25f, 54.31f, -247.69f }, { 1152.81f, 54.37f, -247.67f }, { 1152.24f, 54.37f, -247.78f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1152.81f, 54.38f, -247.67f }, { 1152.25f, 54.31f, -247.69f }, { 1152.80f, 54.31f, -247.59f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1153.34f, 54.31f, -247.48f }, { 1152.81f, 54.37f, -247.67f }, { 1152.80f, 54.31f, -247.59f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1152.81f, 54.38f, -247.67f }, { 1153.34f, 54.31f, -247.48f }, { 1153.38f, 54.37f, -247.56f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1152.25f, 54.41f, -247.87f }, { 1152.85f, 54.44f, -247.84f }, { 1152.25f, 54.44f, -247.96f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1152.85f, 54.44f, -247.84f }, { 1152.25f, 54.41f, -247.87f }, { 1152.83f, 54.41f, -247.76f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1153.41f, 54.41f, -247.65f }, { 1152.85f, 54.44f, -247.84f }, { 1152.83f, 54.41f, -247.76f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1152.85f, 54.44f, -247.84f }, { 1153.41f, 54.41f, -247.65f }, { 1153.44f, 54.44f, -247.73f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1153.25f, 53.59f, -247.31f }, { 1153.28f, 53.81f, -247.32f }, { 1153.24f, 53.81f, -247.33f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1153.24f, 53.81f, -247.33f }, { 1153.22f, 53.59f, -247.32f }, { 1153.25f, 53.59f, -247.31f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1153.30f, 54.01f, -247.35f }, { 1153.34f, 54.18f, -247.40f }, { 1153.30f, 54.18f, -247.41f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1153.30f, 54.18f, -247.41f }, { 1153.26f, 54.01f, -247.36f }, { 1153.30f, 54.01f, -247.35f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1153.38f, 54.31f, -247.47f }, { 1153.38f, 54.37f, -247.56f }, { 1153.34f, 54.31f, -247.48f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1153.38f, 54.38f, -247.56f }, { 1153.38f, 54.31f, -247.47f }, { 1153.42f, 54.37f, -247.55f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1153.45f, 54.41f, -247.64f }, { 1153.44f, 54.44f, -247.73f }, { 1153.41f, 54.41f, -247.65f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1153.44f, 54.44f, -247.73f }, { 1153.45f, 54.41f, -247.64f }, { 1153.47f, 54.44f, -247.72f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1152.21f, 54.35f, -247.78f }, { 1152.22f, 54.29f, -247.70f }, { 1152.22f, 54.31f, -247.70f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1152.22f, 54.31f, -247.70f }, { 1152.21f, 54.37f, -247.78f }, { 1152.21f, 54.35f, -247.78f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1153.42f, 54.35f, -247.55f }, { 1153.38f, 54.31f, -247.47f }, { 1153.38f, 54.29f, -247.48f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1153.38f, 54.31f, -247.47f }, { 1153.42f, 54.35f, -247.55f }, { 1153.42f, 54.37f, -247.55f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1152.26f, 53.81f, -247.53f }, { 1152.27f, 53.59f, -247.50f }, { 1152.25f, 53.81f, -247.52f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1152.27f, 53.59f, -247.50f }, { 1152.26f, 53.81f, -247.53f }, { 1152.27f, 53.59f, -247.51f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1153.28f, 53.81f, -247.33f }, { 1153.25f, 53.59f, -247.31f }, { 1153.25f, 53.59f, -247.32f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1153.25f, 53.59f, -247.31f }, { 1153.28f, 53.81f, -247.33f }, { 1153.28f, 53.81f, -247.32f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1152.23f, 54.17f, -247.62f }, { 1152.24f, 54.00f, -247.56f }, { 1152.24f, 54.01f, -247.56f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1152.24f, 54.01f, -247.56f }, { 1152.22f, 54.18f, -247.62f }, { 1152.23f, 54.17f, -247.62f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1153.34f, 54.17f, -247.41f }, { 1153.30f, 54.01f, -247.35f }, { 1153.30f, 54.00f, -247.36f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1153.30f, 54.01f, -247.35f }, { 1153.34f, 54.17f, -247.41f }, { 1153.34f, 54.18f, -247.40f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1152.22f, 54.41f, -247.97f }, { 1152.21f, 54.39f, -247.88f }, { 1152.21f, 54.41f, -247.88f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1152.21f, 54.41f, -247.88f }, { 1152.22f, 54.44f, -247.96f }, { 1152.22f, 54.41f, -247.96f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1153.47f, 54.41f, -247.72f }, { 1153.45f, 54.41f, -247.64f }, { 1153.45f, 54.39f, -247.64f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1153.45f, 54.41f, -247.64f }, { 1153.47f, 54.41f, -247.72f }, { 1153.47f, 54.44f, -247.72f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1152.22f, 54.29f, -247.70f }, { 1152.23f, 54.17f, -247.62f }, { 1152.22f, 54.18f, -247.62f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1152.22f, 54.18f, -247.62f }, { 1152.22f, 54.31f, -247.70f }, { 1152.22f, 54.29f, -247.70f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1152.22f, 54.31f, -247.70f }, { 1152.22f, 54.18f, -247.62f }, { 1152.25f, 54.31f, -247.69f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1152.22f, 54.18f, -247.62f }, { 1152.26f, 54.18f, -247.61f }, { 1152.25f, 54.31f, -247.69f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1152.80f, 54.31f, -247.59f }, { 1152.25f, 54.31f, -247.69f }, { 1152.26f, 54.18f, -247.61f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1152.80f, 54.31f, -247.59f }, { 1152.26f, 54.18f, -247.61f }, { 1152.78f, 54.18f, -247.51f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1153.30f, 54.18f, -247.41f }, { 1152.80f, 54.31f, -247.59f }, { 1152.78f, 54.18f, -247.51f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1152.80f, 54.31f, -247.59f }, { 1153.30f, 54.18f, -247.41f }, { 1153.34f, 54.31f, -247.48f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1153.34f, 54.31f, -247.48f }, { 1153.34f, 54.18f, -247.40f }, { 1153.38f, 54.31f, -247.47f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1153.34f, 54.18f, -247.40f }, { 1153.34f, 54.31f, -247.48f }, { 1153.30f, 54.18f, -247.41f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1153.38f, 54.31f, -247.47f }, { 1153.34f, 54.18f, -247.40f }, { 1153.38f, 54.29f, -247.48f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1153.34f, 54.17f, -247.41f }, { 1153.38f, 54.29f, -247.48f }, { 1153.34f, 54.18f, -247.40f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1152.24f, 54.00f, -247.56f }, { 1152.25f, 53.81f, -247.52f }, { 1152.24f, 54.01f, -247.56f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1152.25f, 53.81f, -247.52f }, { 1152.24f, 54.00f, -247.56f }, { 1152.26f, 53.81f, -247.53f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1152.24f, 54.01f, -247.56f }, { 1152.25f, 53.81f, -247.52f }, { 1152.28f, 54.01f, -247.55f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1152.25f, 53.81f, -247.52f }, { 1152.29f, 53.81f, -247.51f }, { 1152.28f, 54.01f, -247.55f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1152.77f, 54.01f, -247.45f }, { 1152.28f, 54.01f, -247.55f }, { 1152.29f, 53.81f, -247.51f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1152.77f, 54.01f, -247.45f }, { 1152.29f, 53.81f, -247.51f }, { 1152.76f, 53.81f, -247.42f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1153.24f, 53.81f, -247.33f }, { 1152.77f, 54.01f, -247.45f }, { 1152.76f, 53.81f, -247.42f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1152.77f, 54.01f, -247.45f }, { 1153.24f, 53.81f, -247.33f }, { 1153.26f, 54.01f, -247.36f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1153.27f, 54.01f, -247.36f }, { 1153.28f, 53.81f, -247.32f }, { 1153.30f, 54.01f, -247.35f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1153.28f, 53.81f, -247.32f }, { 1153.26f, 54.01f, -247.36f }, { 1153.24f, 53.81f, -247.33f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1153.30f, 54.01f, -247.35f }, { 1153.28f, 53.81f, -247.32f }, { 1153.30f, 54.00f, -247.36f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1153.30f, 54.00f, -247.36f }, { 1153.28f, 53.81f, -247.32f }, { 1153.28f, 53.81f, -247.33f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1152.21f, 54.39f, -247.88f }, { 1152.21f, 54.35f, -247.78f }, { 1152.21f, 54.37f, -247.78f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1152.21f, 54.38f, -247.78f }, { 1152.21f, 54.41f, -247.88f }, { 1152.21f, 54.39f, -247.88f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1152.24f, 54.38f, -247.78f }, { 1152.25f, 54.41f, -247.87f }, { 1152.21f, 54.37f, -247.78f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1152.21f, 54.38f, -247.78f }, { 1152.25f, 54.41f, -247.87f }, { 1152.21f, 54.41f, -247.88f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1152.24f, 54.38f, -247.78f }, { 1152.83f, 54.41f, -247.76f }, { 1152.25f, 54.41f, -247.87f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1152.24f, 54.38f, -247.78f }, { 1152.81f, 54.37f, -247.67f }, { 1152.83f, 54.41f, -247.76f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1153.38f, 54.38f, -247.56f }, { 1152.83f, 54.41f, -247.76f }, { 1152.81f, 54.37f, -247.67f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1152.83f, 54.41f, -247.76f }, { 1153.38f, 54.37f, -247.56f }, { 1153.41f, 54.41f, -247.65f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1153.42f, 54.38f, -247.55f }, { 1153.41f, 54.41f, -247.65f }, { 1153.38f, 54.37f, -247.56f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1153.41f, 54.41f, -247.65f }, { 1153.42f, 54.37f, -247.55f }, { 1153.45f, 54.41f, -247.64f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1153.45f, 54.41f, -247.64f }, { 1153.42f, 54.37f, -247.55f }, { 1153.45f, 54.39f, -247.64f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1153.42f, 54.35f, -247.55f }, { 1153.45f, 54.39f, -247.64f }, { 1153.42f, 54.37f, -247.55f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1152.38f, 54.44f, -248.58f }, { 1152.22f, 54.44f, -247.96f }, { 1152.25f, 54.44f, -247.96f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1152.22f, 54.44f, -247.96f }, { 1152.37f, 54.44f, -248.58f }, { 1152.34f, 54.44f, -248.59f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1152.25f, 54.44f, -247.96f }, { 1152.97f, 54.44f, -248.47f }, { 1152.37f, 54.44f, -248.58f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1152.85f, 54.44f, -247.84f }, { 1152.97f, 54.44f, -248.47f }, { 1152.25f, 54.44f, -247.96f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1153.44f, 54.44f, -247.73f }, { 1152.97f, 54.44f, -248.47f }, { 1152.85f, 54.44f, -247.84f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1152.97f, 54.44f, -248.47f }, { 1153.44f, 54.44f, -247.73f }, { 1153.56f, 54.44f, -248.35f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1153.47f, 54.44f, -247.72f }, { 1153.60f, 54.44f, -248.34f }, { 1153.44f, 54.44f, -247.73f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1153.44f, 54.44f, -247.73f }, { 1153.60f, 54.44f, -248.34f }, { 1153.56f, 54.44f, -248.35f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1152.22f, 54.44f, -247.96f }, { 1152.34f, 54.44f, -248.59f }, { 1152.22f, 54.41f, -247.96f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1152.22f, 54.41f, -247.97f }, { 1152.34f, 54.44f, -248.59f }, { 1152.34f, 54.41f, -248.59f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1153.47f, 54.41f, -247.72f }, { 1153.60f, 54.44f, -248.34f }, { 1153.47f, 54.44f, -247.72f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 1153.60f, 54.44f, -248.34f }, { 1153.47f, 54.41f, -247.72f }, { 1153.60f, 54.41f, -248.35f }, 53.59f, 0.00f },   // waterfeature_02 props 659
    { { 451.01f, 55.88f, 307.97f }, { 444.77f, 55.88f, 310.73f }, { 451.65f, 55.88f, 309.39f }, 55.88f, 0.00f },   // anc_triangular_waterplane props 759
    { { 451.65f, 55.88f, 309.39f }, { 444.77f, 55.88f, 310.73f }, { 447.97f, 55.88f, 318.09f }, 55.88f, 0.00f },   // anc_triangular_waterplane props 759
    { { 444.77f, 55.88f, 310.73f }, { 438.05f, 55.88f, 315.11f }, { 447.97f, 55.88f, 318.09f }, 55.88f, 0.00f },   // anc_triangular_waterplane props 759
    { { 438.05f, 55.88f, 315.11f }, { 444.77f, 55.88f, 310.73f }, { 437.46f, 55.88f, 314.05f }, 55.88f, 0.00f },   // anc_triangular_waterplane props 759
    { { -76.10f, -0.33f, 274.46f }, { -54.85f, -0.43f, 267.74f }, { -65.94f, -0.33f, 265.61f }, -0.58f, 0.00f },   // ground_station_01 ground 1
    { { -65.94f, -0.33f, 265.61f }, { -54.85f, -0.43f, 267.74f }, { -53.60f, -0.33f, 255.34f }, -0.58f, 0.00f },   // ground_station_01 ground 1
    { { -53.59f, -0.33f, 255.34f }, { -54.85f, -0.43f, 267.74f }, { -46.80f, -0.33f, 250.14f }, -0.58f, 0.00f },   // ground_station_01 ground 1
    { { -46.80f, -0.33f, 250.14f }, { -54.85f, -0.43f, 267.74f }, { -45.53f, -0.32f, 249.34f }, -0.58f, 0.00f },   // ground_station_01 ground 1
    { { -80.29f, 0.11f, 467.66f }, { -92.38f, 0.16f, 440.28f }, { -86.65f, 0.16f, 466.49f }, -0.58f, 0.00f },   // ground_station_01 ground 1
    { { -92.38f, 0.16f, 440.28f }, { -103.23f, 0.16f, 452.95f }, { -86.65f, 0.16f, 466.49f }, -0.58f, 0.00f },   // ground_station_01 ground 1
    { { -86.65f, 0.16f, 466.49f }, { -103.23f, 0.16f, 452.95f }, { -93.08f, 0.16f, 465.10f }, -0.58f, 0.00f },   // ground_station_01 ground 1
    { { -93.08f, 0.16f, 465.10f }, { -103.23f, 0.16f, 452.95f }, { -99.12f, 0.16f, 463.37f }, -0.58f, 0.00f },   // ground_station_01 ground 1
    { { -103.23f, 0.16f, 452.95f }, { -103.97f, 0.16f, 454.05f }, { -99.12f, 0.16f, 463.37f }, -0.58f, 0.00f },   // ground_station_01 ground 1
    { { -99.12f, 0.16f, 463.37f }, { -103.97f, 0.16f, 454.05f }, { -101.85f, 0.16f, 462.42f }, -0.58f, 0.00f },   // ground_station_01 ground 1
    { { -103.97f, 0.16f, 454.05f }, { -104.64f, 0.16f, 455.36f }, { -101.85f, 0.16f, 462.42f }, -0.58f, 0.00f },   // ground_station_01 ground 1
    { { -56.77f, -0.01f, 457.97f }, { -92.38f, 0.16f, 440.28f }, { -80.29f, 0.11f, 467.66f }, -0.58f, 0.00f },   // ground_station_01 ground 1
    { { -56.77f, -0.01f, 457.97f }, { -90.25f, 0.16f, 428.12f }, { -92.38f, 0.16f, 440.28f }, -0.58f, 0.00f },   // ground_station_01 ground 1
    { { -101.85f, 0.16f, 462.42f }, { -104.64f, 0.16f, 455.36f }, { -103.51f, 0.16f, 461.39f }, -0.58f, 0.00f },   // ground_station_01 ground 1
    { { -104.64f, 0.16f, 455.36f }, { -105.02f, 0.16f, 456.76f }, { -103.51f, 0.16f, 461.39f }, -0.58f, 0.00f },   // ground_station_01 ground 1
    { { -80.29f, 0.11f, 467.66f }, { -58.47f, -0.01f, 463.20f }, { -56.77f, -0.01f, 457.97f }, -0.58f, 0.00f },   // ground_station_01 ground 1
    { { -105.02f, 0.16f, 456.76f }, { -105.08f, 0.16f, 458.13f }, { -103.51f, 0.16f, 461.39f }, -0.58f, 0.00f },   // ground_station_01 ground 1
    { { -105.08f, 0.16f, 458.13f }, { -104.36f, 0.16f, 460.50f }, { -103.51f, 0.16f, 461.39f }, -0.58f, 0.00f },   // ground_station_01 ground 1
    { { -105.08f, 0.16f, 458.13f }, { -104.88f, 0.16f, 459.39f }, { -104.36f, 0.16f, 460.50f }, -0.58f, 0.00f },   // ground_station_01 ground 1
    { { -5.55f, 0.05f, 481.02f }, { -5.35f, -0.01f, 480.42f }, { -58.47f, -0.01f, 463.20f }, -0.58f, 0.00f },   // ground_station_01 ground 1
    { { -5.55f, 0.05f, 481.02f }, { -58.47f, -0.01f, 463.20f }, { -80.29f, 0.11f, 467.66f }, -0.58f, 0.00f },   // ground_station_01 ground 1
    { { -13.47f, -0.49f, 257.26f }, { -52.30f, -0.58f, 267.33f }, { -6.44f, -0.57f, 261.06f }, -0.58f, 0.00f },   // ground_station_01 ground 1
    { { -13.47f, -0.49f, 257.26f }, { -20.46f, -0.48f, 253.94f }, { -52.30f, -0.58f, 267.33f }, -0.58f, 0.00f },   // ground_station_01 ground 1
    { { -20.45f, -0.48f, 253.94f }, { -27.95f, -0.48f, 250.80f }, { -52.30f, -0.58f, 267.33f }, -0.58f, 0.00f },   // ground_station_01 ground 1
    { { -38.18f, -0.48f, 247.04f }, { -52.30f, -0.58f, 267.33f }, { -27.95f, -0.48f, 250.80f }, -0.58f, 0.00f },   // ground_station_01 ground 1
    { { -39.62f, -0.48f, 246.76f }, { -52.30f, -0.58f, 267.33f }, { -38.18f, -0.48f, 247.04f }, -0.58f, 0.00f },   // ground_station_01 ground 1
    { { -40.95f, -0.48f, 246.87f }, { -52.30f, -0.58f, 267.33f }, { -39.62f, -0.48f, 246.76f }, -0.58f, 0.00f },   // ground_station_01 ground 1
    { { -42.40f, -0.48f, 247.48f }, { -52.30f, -0.58f, 267.33f }, { -40.95f, -0.48f, 246.87f }, -0.58f, 0.00f },   // ground_station_01 ground 1
    { { -90.92f, 0.00f, 339.03f }, { -81.09f, -0.03f, 319.60f }, { -96.98f, 0.00f, 330.97f }, -0.58f, 0.00f },   // ground_station_01 ground 1
    { { -96.98f, 0.00f, 330.97f }, { -81.09f, -0.03f, 319.60f }, { -97.91f, 0.00f, 329.13f }, -0.58f, 0.00f },   // ground_station_01 ground 1
    { { -97.91f, 0.00f, 329.13f }, { -81.09f, -0.03f, 319.60f }, { -98.39f, 0.00f, 327.42f }, -0.58f, 0.00f },   // ground_station_01 ground 1
    { { -98.39f, 0.00f, 327.42f }, { -81.09f, -0.03f, 319.60f }, { -98.71f, 0.00f, 325.20f }, -0.58f, 0.00f },   // ground_station_01 ground 1
    { { -98.71f, 0.00f, 325.20f }, { -81.09f, -0.03f, 319.60f }, { -98.79f, 0.00f, 323.08f }, -0.58f, 0.00f },   // ground_station_01 ground 1
    { { -98.79f, 0.00f, 323.07f }, { -81.09f, -0.03f, 319.60f }, { -98.62f, 0.00f, 320.86f }, -0.58f, 0.00f },   // ground_station_01 ground 1
    { { -98.62f, 0.00f, 320.86f }, { -81.09f, -0.03f, 319.60f }, { -98.22f, 0.00f, 318.70f }, -0.58f, 0.00f },   // ground_station_01 ground 1
    { { -98.22f, 0.00f, 318.70f }, { -81.09f, -0.03f, 319.60f }, { -97.06f, -0.00f, 315.02f }, -0.58f, 0.00f },   // ground_station_01 ground 1
    { { -81.09f, -0.03f, 319.60f }, { -95.66f, -0.00f, 311.55f }, { -97.06f, -0.00f, 315.02f }, -0.58f, 0.00f },   // ground_station_01 ground 1
    { { -81.09f, -0.03f, 319.60f }, { -89.95f, -0.00f, 299.97f }, { -95.66f, -0.00f, 311.55f }, -0.58f, 0.00f },   // ground_station_01 ground 1
    { { -88.85f, -0.17f, 297.72f }, { -79.85f, -0.19f, 317.15f }, { -56.17f, -0.28f, 270.34f }, -0.58f, 0.00f },   // ground_station_01 ground 1
    { { -88.85f, -0.17f, 297.72f }, { -56.17f, -0.28f, 270.34f }, { -78.94f, -0.17f, 277.59f }, -0.58f, 0.00f },   // ground_station_01 ground 1
    { { 844.00f, 8.15f, 328.49f }, { 966.75f, 8.35f, 419.11f }, { 951.32f, 8.15f, 302.67f }, 4.75f, 0.00f },   // anansitower_plaza ground 11
    { { 1157.50f, 8.35f, 407.24f }, { 951.32f, 8.15f, 302.67f }, { 966.75f, 8.35f, 419.11f }, 4.75f, 0.00f },   // anansitower_plaza ground 11
    { { 951.32f, 8.15f, 302.67f }, { 1157.50f, 8.35f, 407.24f }, { 964.82f, 8.15f, 250.17f }, 4.75f, 0.00f },   // anansitower_plaza ground 11
    { { 1157.50f, 8.35f, 407.24f }, { 1123.25f, 8.15f, 269.17f }, { 964.82f, 8.15f, 250.17f }, 4.75f, 0.00f },   // anansitower_plaza ground 11
    { { 1123.25f, 8.15f, 269.17f }, { 1157.50f, 8.35f, 407.24f }, { 1279.25f, 8.35f, 391.99f }, 4.75f, 0.00f },   // anansitower_plaza ground 11
    { { 1279.25f, 8.35f, 391.99f }, { 1171.00f, 8.15f, 274.86f }, { 1123.25f, 8.15f, 269.17f }, 4.75f, 0.00f },   // anansitower_plaza ground 11
    { { 1171.00f, 8.15f, 274.86f }, { 1279.25f, 8.35f, 391.99f }, { 1339.25f, 8.35f, 384.49f }, 4.75f, 0.00f },   // anansitower_plaza ground 11
    { { 596.50f, 11.29f, 382.74f }, { 765.25f, 11.29f, 296.61f }, { 713.63f, 11.29f, 257.49f }, 4.75f, 0.00f },   // anansitower_plaza ground 11
    { { 765.25f, 11.29f, 296.61f }, { 596.50f, 11.29f, 382.74f }, { 710.13f, 11.29f, 448.99f }, 4.75f, 0.00f },   // anansitower_plaza ground 11
    { { 710.13f, 11.29f, 448.99f }, { 596.50f, 11.29f, 382.74f }, { 635.75f, 11.29f, 452.99f }, 4.75f, 0.00f },   // anansitower_plaza ground 11
    { { 562.75f, 13.85f, 399.49f }, { 455.00f, 13.85f, 433.61f }, { 515.25f, 13.85f, 451.99f }, 4.75f, 0.00f },   // anansitower_plaza ground 11
    { { 515.25f, 13.85f, 451.99f }, { 455.00f, 13.85f, 433.61f }, { 463.00f, 13.85f, 456.49f }, 4.75f, 0.00f },   // anansitower_plaza ground 11
    { { 562.75f, 13.85f, 399.49f }, { 496.25f, 13.85f, 254.67f }, { 455.00f, 13.85f, 433.61f }, 4.75f, 0.00f },   // anansitower_plaza ground 11
    { { 562.75f, 13.85f, 399.49f }, { 512.75f, 13.85f, 249.11f }, { 496.25f, 13.85f, 254.67f }, 4.75f, 0.00f },   // anansitower_plaza ground 11
    { { 825.32f, 5.59f, 332.74f }, { 734.88f, 5.95f, 448.99f }, { 965.57f, 5.92f, 437.49f }, 4.75f, 0.00f },   // anansitower_plaza ground 11
    { { 734.88f, 5.95f, 448.99f }, { 825.32f, 5.59f, 332.74f }, { 770.50f, 5.59f, 346.11f }, 4.75f, 0.00f },   // anansitower_plaza ground 11
    { { 718.63f, 4.75f, 185.22f }, { 725.00f, 4.75f, 208.75f }, { 797.75f, 4.75f, 216.64f }, 4.75f, 0.00f },   // anansitower_plaza ground 11
    { { 797.75f, 4.75f, 216.64f }, { 759.63f, 4.75f, 151.17f }, { 718.63f, 4.75f, 185.22f }, 4.75f, 0.00f },   // anansitower_plaza ground 11
    { { 759.63f, 4.75f, 151.17f }, { 726.00f, 4.75f, 166.53f }, { 718.63f, 4.75f, 185.22f }, 4.75f, 0.00f },   // anansitower_plaza ground 11
    { { 857.97f, 4.75f, 43.61f }, { 759.63f, 4.75f, 151.17f }, { 797.75f, 4.75f, 216.64f }, 4.75f, 0.00f },   // anansitower_plaza ground 11
    { { 757.38f, 4.75f, -34.77f }, { 759.63f, 4.75f, 151.17f }, { 857.97f, 4.75f, 43.61f }, 4.75f, 0.00f },   // anansitower_plaza ground 11
    { { 759.63f, 4.75f, 151.17f }, { 655.13f, 4.75f, 28.49f }, { 726.00f, 4.75f, 166.53f }, 4.75f, 0.00f },   // anansitower_plaza ground 11
    { { 759.63f, 4.75f, 151.17f }, { 757.38f, 4.75f, -34.76f }, { 655.13f, 4.75f, 28.49f }, 4.75f, 0.00f },   // anansitower_plaza ground 11
    { { 901.54f, 11.60f, 137.55f }, { 948.88f, 11.50f, 234.58f }, { 951.85f, 11.50f, 178.81f }, 4.75f, 0.00f },   // anansitower_plaza ground 11
    { { 816.19f, 11.70f, 219.02f }, { 948.88f, 11.50f, 234.58f }, { 901.54f, 11.60f, 137.55f }, 4.75f, 0.00f },   // anansitower_plaza ground 11
    { { 887.39f, 11.70f, 19.86f }, { 816.19f, 11.70f, 219.02f }, { 901.54f, 11.60f, 137.55f }, 4.75f, 0.00f },   // anansitower_plaza ground 11
    { { 887.39f, 11.70f, 19.86f }, { 901.54f, 11.60f, 137.55f }, { 960.16f, 11.60f, 55.24f }, 4.75f, 0.00f },   // anansitower_plaza ground 11
    { { 912.66f, 11.67f, 17.98f }, { 887.39f, 11.70f, 19.86f }, { 960.16f, 11.60f, 55.24f }, 4.75f, 0.00f },   // anansitower_plaza ground 11
    { { 1449.08f, 0.70f, 284.79f }, { 1399.14f, 0.70f, 300.54f }, { 1416.14f, 0.70f, 335.88f }, 0.70f, 0.00f },   // ground_anchor_02 ground 11
    { { 1416.14f, 0.70f, 335.88f }, { 1515.87f, 0.70f, 311.35f }, { 1449.08f, 0.70f, 284.79f }, 0.70f, 0.00f },   // ground_anchor_02 ground 11
    { { 1389.23f, 0.70f, 407.44f }, { 1515.87f, 0.70f, 311.35f }, { 1416.14f, 0.70f, 335.88f }, 0.70f, 0.00f },   // ground_anchor_02 ground 11
    { { 1389.23f, 0.70f, 407.44f }, { 1416.14f, 0.70f, 335.88f }, { 1411.00f, 0.70f, 337.86f }, 0.70f, 0.00f },   // ground_anchor_02 ground 11
    { { 1309.52f, 0.70f, 434.09f }, { 1389.23f, 0.70f, 407.44f }, { 1411.00f, 0.70f, 337.86f }, 0.70f, 0.00f },   // ground_anchor_02 ground 11
    { { 1382.47f, 0.70f, 305.97f }, { 1309.51f, 0.70f, 434.09f }, { 1411.00f, 0.70f, 337.86f }, 0.70f, 0.00f },   // ground_anchor_02 ground 11
    { { 1411.00f, 0.70f, 337.86f }, { 1393.64f, 0.70f, 302.28f }, { 1382.47f, 0.70f, 305.97f }, 0.70f, 0.00f },   // ground_anchor_02 ground 11
    { { 1309.52f, 0.70f, 434.09f }, { 1382.47f, 0.70f, 305.97f }, { 1131.26f, 0.70f, 436.82f }, 0.70f, 0.00f },   // ground_anchor_02 ground 11
    { { 1382.47f, 0.70f, 305.97f }, { 1350.30f, 0.70f, 288.16f }, { 1131.26f, 0.70f, 436.82f }, 0.70f, 0.00f },   // ground_anchor_02 ground 11
    { { 1267.71f, 0.70f, 242.71f }, { 1131.26f, 0.70f, 436.82f }, { 1350.30f, 0.70f, 288.16f }, 0.70f, 0.00f },   // ground_anchor_02 ground 11
    { { 1235.84f, 0.70f, 268.12f }, { 1131.26f, 0.70f, 436.82f }, { 1267.71f, 0.70f, 242.71f }, 0.70f, 0.00f },   // ground_anchor_02 ground 11
    { { 1194.18f, 0.70f, 246.18f }, { 1131.26f, 0.70f, 436.82f }, { 1235.84f, 0.70f, 268.12f }, 0.70f, 0.00f },   // ground_anchor_02 ground 11
    { { 1131.26f, 0.70f, 436.81f }, { 1194.18f, 0.70f, 246.18f }, { 1149.14f, 0.70f, 248.68f }, 0.70f, 0.00f },   // ground_anchor_02 ground 11
    { { 1149.14f, 0.70f, 248.68f }, { 1194.18f, 0.70f, 246.18f }, { 1180.08f, 0.70f, 232.49f }, 0.70f, 0.00f },   // ground_anchor_02 ground 11
    { { 738.21f, 0.16f, -107.52f }, { 850.13f, 0.16f, -43.17f }, { 750.45f, 0.16f, -122.48f }, -2.37f, 0.00f },   // ground_marina_01 ground 13
    { { 850.13f, 0.16f, -43.17f }, { 738.21f, 0.16f, -107.52f }, { 844.50f, 0.16f, -21.90f }, -2.37f, 0.00f },   // ground_marina_01 ground 13
    { { 894.56f, 0.16f, -47.32f }, { 850.13f, 0.16f, -43.17f }, { 844.50f, 0.16f, -21.90f }, -2.37f, 0.00f },   // ground_marina_01 ground 13
    { { 894.56f, 0.16f, -47.32f }, { 844.50f, 0.16f, -21.90f }, { 946.16f, 0.16f, -29.94f }, -2.37f, 0.00f },   // ground_marina_01 ground 13
    { { 971.34f, 0.15f, -47.21f }, { 894.56f, 0.16f, -47.32f }, { 946.16f, 0.16f, -29.94f }, -2.37f, 0.00f },   // ground_marina_01 ground 13
    { { 970.81f, 0.15f, -54.44f }, { 894.56f, 0.16f, -47.32f }, { 971.34f, 0.15f, -47.21f }, -2.37f, 0.00f },   // ground_marina_01 ground 13
    { { 1037.84f, 0.15f, -54.08f }, { 971.34f, 0.15f, -47.21f }, { 946.16f, 0.16f, -29.94f }, -2.37f, 0.00f },   // ground_marina_01 ground 13
    { { 1026.45f, 0.15f, 32.10f }, { 1037.84f, 0.15f, -54.08f }, { 946.16f, 0.16f, -29.94f }, -2.37f, 0.00f },   // ground_marina_01 ground 13
    { { 1037.84f, 0.15f, -54.08f }, { 1026.45f, 0.15f, 32.10f }, { 1055.19f, 0.15f, -4.16f }, -2.37f, 0.00f },   // ground_marina_01 ground 13
    { { 1055.19f, 0.15f, -4.16f }, { 1101.34f, 0.15f, -1.10f }, { 1037.84f, 0.15f, -54.08f }, -2.37f, 0.00f },   // ground_marina_01 ground 13
    { { 1101.34f, 0.15f, -1.10f }, { 1055.19f, 0.15f, -4.16f }, { 1090.00f, 0.15f, 12.39f }, -2.37f, 0.00f },   // ground_marina_01 ground 13
    { { 1090.00f, 0.15f, 12.39f }, { 1055.19f, 0.15f, -4.16f }, { 1084.26f, 0.15f, 18.55f }, -2.37f, 0.00f },   // ground_marina_01 ground 13
    { { 1084.26f, 0.15f, 18.55f }, { 1098.06f, 0.15f, 18.66f }, { 1090.00f, 0.15f, 12.39f }, -2.37f, 0.00f },   // ground_marina_01 ground 13
    { { 1098.06f, 0.15f, 18.66f }, { 1084.26f, 0.15f, 18.55f }, { 1093.85f, 0.15f, 26.10f }, -2.37f, 0.00f },   // ground_marina_01 ground 13
    { { 1084.26f, 0.15f, 18.55f }, { 1055.24f, 0.15f, 73.02f }, { 1093.85f, 0.15f, 26.10f }, -2.37f, 0.00f },   // ground_marina_01 ground 13
    { { 1055.24f, 0.15f, 73.02f }, { 1084.26f, 0.15f, 18.55f }, { 1046.02f, 0.15f, 65.99f }, -2.37f, 0.00f },   // ground_marina_01 ground 13
    { { 1125.71f, 0.15f, 50.94f }, { 1098.06f, 0.15f, 18.66f }, { 1093.85f, 0.15f, 26.10f }, -2.37f, 0.00f },   // ground_marina_01 ground 13
    { { 1146.47f, 0.15f, 56.35f }, { 1098.06f, 0.15f, 18.66f }, { 1125.71f, 0.15f, 50.94f }, -2.37f, 0.00f },   // ground_marina_01 ground 13
    { { 1008.27f, 0.15f, 112.81f }, { 1055.24f, 0.15f, 73.02f }, { 1046.02f, 0.15f, 65.99f }, -2.37f, 0.00f },   // ground_marina_01 ground 13
    { { 1055.24f, 0.15f, 73.02f }, { 1008.27f, 0.15f, 112.81f }, { 1012.39f, 0.15f, 125.10f }, -2.37f, 0.00f },   // ground_marina_01 ground 13
    { { 959.72f, 0.15f, 82.05f }, { 1012.39f, 0.15f, 125.10f }, { 1008.27f, 0.15f, 112.81f }, -2.37f, 0.00f },   // ground_marina_01 ground 13
    { { 1008.27f, 0.15f, 112.81f }, { 957.19f, 0.15f, 70.85f }, { 959.72f, 0.15f, 82.05f }, -2.37f, 0.00f },   // ground_marina_01 ground 13
    { { 959.72f, 0.15f, 82.05f }, { 957.19f, 0.15f, 70.85f }, { 952.75f, 0.15f, 76.36f }, -2.37f, 0.00f },   // ground_marina_01 ground 13
    { { 952.75f, 0.15f, 76.36f }, { 941.33f, 0.15f, 104.05f }, { 959.72f, 0.15f, 82.05f }, -2.37f, 0.00f },   // ground_marina_01 ground 13
    { { 952.75f, 0.15f, 76.36f }, { 934.28f, 0.15f, 98.39f }, { 941.33f, 0.15f, 104.05f }, -2.37f, 0.00f },   // ground_marina_01 ground 13
    { { 957.19f, 0.15f, 70.85f }, { 906.34f, 0.15f, 38.42f }, { 952.75f, 0.15f, 76.36f }, -2.37f, 0.00f },   // ground_marina_01 ground 13
    { { 906.34f, 0.15f, 38.42f }, { 957.19f, 0.15f, 70.85f }, { 910.94f, 0.15f, 32.80f }, -2.37f, 0.00f },   // ground_marina_01 ground 13
    { { 1134.48f, 0.15f, 83.92f }, { 1146.47f, 0.15f, 56.35f }, { 1125.71f, 0.15f, 50.94f }, -2.37f, 0.00f },   // ground_marina_01 ground 13
    { { 1134.48f, 0.15f, 83.92f }, { 1164.53f, 0.15f, 45.45f }, { 1146.47f, 0.15f, 56.35f }, -2.37f, 0.00f },   // ground_marina_01 ground 13
    { { 1146.47f, 0.15f, 56.35f }, { 1164.53f, 0.15f, 45.45f }, { 1170.86f, 0.15f, 37.34f }, -2.37f, 0.00f },   // ground_marina_01 ground 13
    { { 1183.05f, 0.15f, 9.04f }, { 1146.47f, 0.15f, 56.35f }, { 1170.86f, 0.15f, 37.34f }, -2.37f, 0.00f },   // ground_marina_01 ground 13
    { { 1183.05f, 0.15f, 9.04f }, { 1170.86f, 0.15f, 37.34f }, { 1189.15f, 0.15f, 13.92f }, -2.37f, 0.00f },   // ground_marina_01 ground 13
    { { 1164.53f, 0.15f, 45.45f }, { 1264.76f, 0.15f, 110.58f }, { 1170.86f, 0.15f, 37.34f }, -2.37f, 0.00f },   // ground_marina_01 ground 13
    { { 1164.53f, 0.15f, 45.45f }, { 1298.16f, 0.15f, 149.67f }, { 1264.76f, 0.15f, 110.58f }, -2.37f, 0.00f },   // ground_marina_01 ground 13
    { { 1304.69f, 0.15f, 141.73f }, { 1264.76f, 0.15f, 110.58f }, { 1298.16f, 0.15f, 149.67f }, -2.37f, 0.00f },   // ground_marina_01 ground 13
    { { 1120.81f, 0.15f, 101.43f }, { 1185.58f, 0.15f, 123.34f }, { 1134.48f, 0.15f, 83.93f }, -2.37f, 0.00f },   // ground_marina_01 ground 13
    { { 1120.81f, 0.15f, 101.43f }, { 1134.48f, 0.15f, 83.93f }, { 1125.71f, 0.15f, 50.94f }, -2.37f, 0.00f },   // ground_marina_01 ground 13
    { { 1120.81f, 0.15f, 101.43f }, { 1250.04f, 0.15f, 202.46f }, { 1185.58f, 0.15f, 123.34f }, -2.37f, 0.00f },   // ground_marina_01 ground 13
    { { 1061.11f, 0.15f, 177.85f }, { 1120.81f, 0.15f, 101.43f }, { 1125.71f, 0.15f, 50.94f }, -2.37f, 0.00f },   // ground_marina_01 ground 13
    { { 1061.11f, 0.15f, 177.85f }, { 1125.71f, 0.15f, 50.94f }, { 1038.49f, 0.15f, 159.72f }, -2.37f, 0.00f },   // ground_marina_01 ground 13
    { { 1286.71f, 0.15f, 156.62f }, { 1185.58f, 0.15f, 123.34f }, { 1250.04f, 0.15f, 202.46f }, -2.37f, 0.00f },   // ground_marina_01 ground 13
    { { 1286.94f, 0.15f, 155.08f }, { 1185.58f, 0.15f, 123.34f }, { 1286.71f, 0.15f, 156.62f }, -2.37f, 0.00f },   // ground_marina_01 ground 13
    { { 1185.58f, 0.15f, 123.34f }, { 1286.94f, 0.15f, 155.08f }, { 1210.16f, 0.15f, 93.06f }, -2.37f, 0.00f },   // ground_marina_01 ground 13
    { { 1185.58f, 0.15f, 123.34f }, { 1210.16f, 0.15f, 93.06f }, { 1208.76f, 0.15f, 93.34f }, -2.37f, 0.00f },   // ground_marina_01 ground 13
    { { 674.75f, 0.32f, -327.68f }, { 674.59f, 0.79f, -327.97f }, { 674.59f, 0.31f, -327.97f }, -2.37f, 0.00f },   // ground_marina_01 ground 13
    { { 674.59f, 0.79f, -327.97f }, { 674.75f, 0.32f, -327.68f }, { 674.86f, 0.79f, -327.57f }, -2.37f, 0.00f },   // ground_marina_01 ground 13
    { { 677.91f, 0.15f, -323.07f }, { 674.86f, 0.79f, -327.57f }, { 674.75f, 0.32f, -327.68f }, -2.37f, 0.00f },   // ground_marina_01 ground 13
    { { 674.86f, 0.79f, -327.57f }, { 677.91f, 0.15f, -323.07f }, { 677.91f, 0.79f, -323.07f }, -2.37f, 0.00f },   // ground_marina_01 ground 13
    { { 677.91f, 0.15f, -323.07f }, { 674.75f, 0.32f, -327.68f }, { 674.75f, -2.37f, -327.68f }, -2.37f, 0.00f },   // ground_marina_01 ground 13
    { { 1351.57f, 1.90f, 169.64f }, { 1351.47f, 1.90f, 168.61f }, { 1346.29f, 1.90f, 169.64f }, 1.90f, 0.00f },   // ground_bdstatue_05 ground 13
    { { 1351.47f, 1.90f, 170.67f }, { 1351.57f, 1.90f, 169.64f }, { 1346.29f, 1.90f, 169.64f }, 1.90f, 0.00f },   // ground_bdstatue_05 ground 13
    { { 1351.47f, 1.90f, 168.61f }, { 1351.17f, 1.90f, 167.61f }, { 1346.29f, 1.90f, 169.64f }, 1.90f, 0.00f },   // ground_bdstatue_05 ground 13
    { { 1351.17f, 1.90f, 171.66f }, { 1351.47f, 1.90f, 170.67f }, { 1346.29f, 1.90f, 169.64f }, 1.90f, 0.00f },   // ground_bdstatue_05 ground 13
    { { 1351.17f, 1.90f, 167.61f }, { 1350.68f, 1.90f, 166.70f }, { 1346.29f, 1.90f, 169.64f }, 1.90f, 0.00f },   // ground_bdstatue_05 ground 13
    { { 1350.68f, 1.90f, 172.57f }, { 1351.17f, 1.90f, 171.66f }, { 1346.29f, 1.90f, 169.64f }, 1.90f, 0.00f },   // ground_bdstatue_05 ground 13
    { { 1350.68f, 1.90f, 166.70f }, { 1350.03f, 1.90f, 165.90f }, { 1346.29f, 1.90f, 169.64f }, 1.90f, 0.00f },   // ground_bdstatue_05 ground 13
    { { 1350.03f, 1.90f, 173.37f }, { 1350.68f, 1.90f, 172.57f }, { 1346.29f, 1.90f, 169.64f }, 1.90f, 0.00f },   // ground_bdstatue_05 ground 13
    { { 1350.03f, 1.90f, 165.90f }, { 1349.23f, 1.90f, 165.25f }, { 1346.29f, 1.90f, 169.64f }, 1.90f, 0.00f },   // ground_bdstatue_05 ground 13
    { { 1349.22f, 1.90f, 174.03f }, { 1350.03f, 1.90f, 173.37f }, { 1346.29f, 1.90f, 169.64f }, 1.90f, 0.00f },   // ground_bdstatue_05 ground 13
    { { 1349.22f, 1.90f, 165.25f }, { 1348.31f, 1.90f, 164.76f }, { 1346.29f, 1.90f, 169.64f }, 1.90f, 0.00f },   // ground_bdstatue_05 ground 13
    { { 1348.31f, 1.90f, 174.51f }, { 1349.23f, 1.90f, 174.03f }, { 1346.29f, 1.90f, 169.64f }, 1.90f, 0.00f },   // ground_bdstatue_05 ground 13
    { { 1348.31f, 1.90f, 164.76f }, { 1347.32f, 1.90f, 164.46f }, { 1346.29f, 1.90f, 169.64f }, 1.90f, 0.00f },   // ground_bdstatue_05 ground 13
    { { 1347.32f, 1.90f, 174.82f }, { 1348.31f, 1.90f, 174.51f }, { 1346.29f, 1.90f, 169.64f }, 1.90f, 0.00f },   // ground_bdstatue_05 ground 13
    { { 1347.32f, 1.90f, 164.46f }, { 1346.29f, 1.90f, 164.35f }, { 1346.29f, 1.90f, 169.64f }, 1.90f, 0.00f },   // ground_bdstatue_05 ground 13
    { { 1346.29f, 1.90f, 174.92f }, { 1347.32f, 1.90f, 174.82f }, { 1346.29f, 1.90f, 169.64f }, 1.90f, 0.00f },   // ground_bdstatue_05 ground 13
    { { 1346.29f, 1.90f, 164.35f }, { 1345.26f, 1.90f, 164.46f }, { 1346.29f, 1.90f, 169.64f }, 1.90f, 0.00f },   // ground_bdstatue_05 ground 13
    { { 1345.26f, 1.90f, 174.82f }, { 1346.29f, 1.90f, 174.92f }, { 1346.29f, 1.90f, 169.64f }, 1.90f, 0.00f },   // ground_bdstatue_05 ground 13
    { { 1345.26f, 1.90f, 164.46f }, { 1344.27f, 1.90f, 164.76f }, { 1346.29f, 1.90f, 169.64f }, 1.90f, 0.00f },   // ground_bdstatue_05 ground 13
    { { 1344.27f, 1.90f, 174.51f }, { 1345.26f, 1.90f, 174.82f }, { 1346.29f, 1.90f, 169.64f }, 1.90f, 0.00f },   // ground_bdstatue_05 ground 13
    { { 1344.27f, 1.90f, 164.76f }, { 1343.36f, 1.90f, 165.25f }, { 1346.29f, 1.90f, 169.64f }, 1.90f, 0.00f },   // ground_bdstatue_05 ground 13
    { { 1343.36f, 1.90f, 174.03f }, { 1344.27f, 1.90f, 174.51f }, { 1346.29f, 1.90f, 169.64f }, 1.90f, 0.00f },   // ground_bdstatue_05 ground 13
    { { 1343.36f, 1.90f, 165.25f }, { 1342.56f, 1.90f, 165.90f }, { 1346.29f, 1.90f, 169.64f }, 1.90f, 0.00f },   // ground_bdstatue_05 ground 13
    { { 1342.56f, 1.90f, 173.37f }, { 1343.36f, 1.90f, 174.03f }, { 1346.29f, 1.90f, 169.64f }, 1.90f, 0.00f },   // ground_bdstatue_05 ground 13
    { { 1342.56f, 1.90f, 165.90f }, { 1341.90f, 1.90f, 166.70f }, { 1346.29f, 1.90f, 169.64f }, 1.90f, 0.00f },   // ground_bdstatue_05 ground 13
    { { 1341.90f, 1.90f, 172.57f }, { 1342.56f, 1.90f, 173.37f }, { 1346.29f, 1.90f, 169.64f }, 1.90f, 0.00f },   // ground_bdstatue_05 ground 13
    { { 1341.90f, 1.90f, 166.70f }, { 1341.41f, 1.90f, 167.61f }, { 1346.29f, 1.90f, 169.64f }, 1.90f, 0.00f },   // ground_bdstatue_05 ground 13
    { { 1341.41f, 1.90f, 171.66f }, { 1341.90f, 1.90f, 172.57f }, { 1346.29f, 1.90f, 169.64f }, 1.90f, 0.00f },   // ground_bdstatue_05 ground 13
    { { 1341.41f, 1.90f, 167.61f }, { 1341.11f, 1.90f, 168.61f }, { 1346.29f, 1.90f, 169.64f }, 1.90f, 0.00f },   // ground_bdstatue_05 ground 13
    { { 1341.11f, 1.90f, 170.67f }, { 1341.41f, 1.90f, 171.66f }, { 1346.29f, 1.90f, 169.64f }, 1.90f, 0.00f },   // ground_bdstatue_05 ground 13
    { { 1341.11f, 1.90f, 168.61f }, { 1341.01f, 1.90f, 169.64f }, { 1346.29f, 1.90f, 169.64f }, 1.90f, 0.00f },   // ground_bdstatue_05 ground 13
    { { 1341.01f, 1.90f, 169.64f }, { 1341.11f, 1.90f, 170.67f }, { 1346.29f, 1.90f, 169.64f }, 1.90f, 0.00f },   // ground_bdstatue_05 ground 13
    { { -153.50f, 3.61f, -534.73f }, { -207.47f, 3.61f, -596.93f }, { -156.00f, 3.61f, -535.67f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -207.47f, 3.61f, -596.93f }, { -249.72f, 3.61f, -576.58f }, { -156.00f, 3.61f, -535.67f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -249.72f, 3.61f, -576.58f }, { -207.47f, 3.61f, -596.93f }, { -282.26f, 3.61f, -606.84f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -249.72f, 3.61f, -576.58f }, { -282.26f, 3.61f, -606.84f }, { -264.62f, 3.61f, -583.71f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -264.62f, 3.61f, -583.71f }, { -282.26f, 3.61f, -606.84f }, { -276.44f, 3.61f, -590.85f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -276.44f, 3.61f, -590.85f }, { -282.26f, 3.61f, -606.84f }, { -282.00f, 3.61f, -594.64f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -144.56f, 3.61f, -532.24f }, { -92.05f, 3.61f, -529.35f }, { -90.02f, 3.61f, -535.08f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -142.08f, 3.61f, -574.71f }, { -144.55f, 3.61f, -532.24f }, { -90.02f, 3.61f, -535.08f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -144.56f, 3.61f, -532.24f }, { -142.08f, 3.61f, -574.71f }, { -152.00f, 3.61f, -581.02f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -152.00f, 3.61f, -581.02f }, { -150.07f, 3.61f, -533.43f }, { -144.55f, 3.61f, -532.24f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -150.07f, 3.61f, -533.42f }, { -152.00f, 3.61f, -581.02f }, { -163.29f, 3.61f, -586.14f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -163.29f, 3.61f, -586.14f }, { -179.99f, 3.61f, -592.03f }, { -150.07f, 3.61f, -533.43f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -179.99f, 3.61f, -592.03f }, { -199.10f, 3.61f, -595.82f }, { -150.07f, 3.61f, -533.43f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -204.91f, 3.61f, -596.59f }, { -150.07f, 3.61f, -533.43f }, { -199.10f, 3.61f, -595.82f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -89.03f, 3.61f, -469.19f }, { -67.60f, 3.61f, -436.25f }, { -79.93f, 3.61f, -464.69f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -79.93f, 3.61f, -464.69f }, { -67.60f, 3.61f, -436.25f }, { -68.57f, 3.61f, -457.81f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -68.57f, 3.61f, -457.81f }, { -67.60f, 3.61f, -436.25f }, { -53.43f, 3.61f, -446.62f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -89.03f, 3.61f, -469.19f }, { -73.86f, 3.61f, -439.52f }, { -67.60f, 3.61f, -436.25f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -98.62f, 3.61f, -472.33f }, { -73.86f, 3.61f, -439.52f }, { -89.02f, 3.61f, -469.19f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -53.43f, 3.61f, -446.62f }, { -67.60f, 3.61f, -436.25f }, { -50.34f, 3.61f, -443.47f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -115.59f, 3.61f, -476.74f }, { -73.86f, 3.61f, -439.52f }, { -98.62f, 3.61f, -472.33f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -185.88f, 3.61f, -489.10f }, { -73.86f, 3.61f, -439.52f }, { -115.59f, 3.61f, -476.73f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -50.34f, 3.61f, -443.47f }, { -67.60f, 3.61f, -436.25f }, { -47.94f, 3.61f, -439.89f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -47.94f, 3.61f, -439.89f }, { -67.60f, 3.61f, -436.25f }, { -46.25f, 3.61f, -435.16f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -46.25f, 3.61f, -435.16f }, { -67.60f, 3.61f, -436.25f }, { -43.44f, 3.61f, -421.56f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -171.43f, 3.61f, -454.38f }, { -80.01f, 3.61f, -441.02f }, { -192.18f, 3.61f, -489.99f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -184.72f, 3.61f, -454.98f }, { -171.43f, 3.61f, -454.38f }, { -192.18f, 3.61f, -489.99f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -198.30f, 3.61f, -455.10f }, { -184.72f, 3.61f, -454.98f }, { -192.18f, 3.61f, -489.99f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -198.30f, 3.61f, -455.10f }, { -192.18f, 3.61f, -489.99f }, { -202.60f, 3.61f, -490.92f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -317.17f, 3.61f, -453.07f }, { -198.30f, 3.61f, -455.10f }, { -202.60f, 3.61f, -490.92f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -317.17f, 3.61f, -453.07f }, { -202.60f, 3.61f, -490.92f }, { -218.16f, 3.61f, -490.66f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -317.17f, 3.61f, -453.07f }, { -218.16f, 3.61f, -490.66f }, { -321.30f, 3.61f, -481.72f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -303.98f, 3.61f, -426.52f }, { -198.32f, 3.61f, -454.26f }, { -316.71f, 3.61f, -451.93f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -276.44f, 3.61f, -590.85f }, { -264.62f, 3.92f, -583.71f }, { -264.62f, 3.61f, -583.71f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -264.62f, 3.92f, -583.71f }, { -276.44f, 3.61f, -590.85f }, { -276.44f, 3.92f, -590.85f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -207.47f, 3.61f, -596.93f }, { -153.50f, 3.61f, -534.73f }, { -153.50f, 3.92f, -534.73f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -153.50f, 3.92f, -534.73f }, { -207.47f, 3.92f, -596.93f }, { -207.47f, 3.61f, -596.93f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -153.50f, 3.61f, -534.73f }, { -156.00f, 3.61f, -535.67f }, { -156.00f, 3.92f, -535.67f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -153.50f, 3.61f, -534.73f }, { -156.00f, 3.92f, -535.67f }, { -153.50f, 3.92f, -534.73f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -282.26f, 3.92f, -606.84f }, { -207.47f, 3.61f, -596.93f }, { -207.47f, 3.92f, -596.93f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -207.47f, 3.61f, -596.93f }, { -282.26f, 3.92f, -606.84f }, { -282.26f, 3.61f, -606.84f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -156.00f, 3.61f, -535.67f }, { -249.72f, 3.61f, -576.58f }, { -249.72f, 3.92f, -576.58f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -156.00f, 3.61f, -535.67f }, { -249.72f, 3.92f, -576.58f }, { -156.00f, 3.92f, -535.67f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -264.62f, 3.61f, -583.71f }, { -249.72f, 3.92f, -576.58f }, { -249.72f, 3.61f, -576.58f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -249.72f, 3.92f, -576.58f }, { -264.62f, 3.61f, -583.71f }, { -264.62f, 3.92f, -583.71f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -282.00f, 3.61f, -594.64f }, { -282.26f, 3.92f, -606.84f }, { -282.00f, 3.92f, -594.64f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -282.26f, 3.92f, -606.84f }, { -282.00f, 3.61f, -594.64f }, { -282.26f, 3.61f, -606.84f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -282.00f, 3.61f, -594.64f }, { -276.44f, 3.92f, -590.85f }, { -276.44f, 3.61f, -590.85f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -276.44f, 3.92f, -590.85f }, { -282.00f, 3.61f, -594.64f }, { -282.00f, 3.92f, -594.64f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -144.56f, 3.61f, -532.24f }, { -150.07f, 3.61f, -533.43f }, { -150.07f, 3.92f, -533.43f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -144.56f, 3.61f, -532.24f }, { -150.07f, 3.92f, -533.43f }, { -144.55f, 3.92f, -532.24f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -152.00f, 3.92f, -581.02f }, { -163.29f, 3.92f, -586.14f }, { -163.29f, 3.61f, -586.14f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -152.00f, 3.92f, -581.02f }, { -163.29f, 3.61f, -586.14f }, { -152.00f, 3.61f, -581.02f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -92.05f, 3.61f, -529.35f }, { -144.55f, 3.61f, -532.24f }, { -144.55f, 3.92f, -532.24f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -92.05f, 3.61f, -529.35f }, { -144.55f, 3.92f, -532.24f }, { -92.05f, 3.92f, -529.35f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -92.05f, 3.61f, -529.35f }, { -90.02f, 3.92f, -535.08f }, { -90.02f, 3.61f, -535.08f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -90.02f, 3.92f, -535.08f }, { -92.05f, 3.61f, -529.35f }, { -92.05f, 3.92f, -529.35f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -90.02f, 3.92f, -535.08f }, { -142.08f, 3.92f, -574.71f }, { -142.08f, 3.61f, -574.71f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -90.02f, 3.92f, -535.08f }, { -142.08f, 3.61f, -574.71f }, { -90.02f, 3.61f, -535.08f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -142.08f, 3.92f, -574.71f }, { -152.00f, 3.92f, -581.02f }, { -152.00f, 3.61f, -581.02f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -142.08f, 3.92f, -574.71f }, { -152.00f, 3.61f, -581.02f }, { -142.08f, 3.61f, -574.71f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -179.99f, 3.92f, -592.03f }, { -163.29f, 3.61f, -586.14f }, { -163.29f, 3.92f, -586.14f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -163.29f, 3.61f, -586.14f }, { -179.99f, 3.92f, -592.03f }, { -179.99f, 3.61f, -592.03f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -199.10f, 3.92f, -595.82f }, { -179.99f, 3.61f, -592.03f }, { -179.99f, 3.92f, -592.03f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -179.99f, 3.61f, -592.03f }, { -199.10f, 3.92f, -595.82f }, { -199.10f, 3.61f, -595.82f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -199.10f, 3.92f, -595.82f }, { -204.91f, 3.61f, -596.59f }, { -199.10f, 3.61f, -595.82f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -204.91f, 3.61f, -596.59f }, { -199.10f, 3.92f, -595.82f }, { -204.91f, 3.92f, -596.59f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -150.07f, 3.61f, -533.42f }, { -204.91f, 3.92f, -596.59f }, { -150.07f, 3.92f, -533.43f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -204.91f, 3.92f, -596.59f }, { -150.07f, 3.61f, -533.43f }, { -204.91f, 3.61f, -596.59f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -79.93f, 3.92f, -464.69f }, { -89.02f, 3.92f, -469.19f }, { -89.02f, 3.61f, -469.19f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -79.93f, 3.92f, -464.69f }, { -89.02f, 3.61f, -469.19f }, { -79.93f, 3.61f, -464.69f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -67.60f, 3.61f, -436.25f }, { -73.86f, 3.61f, -439.52f }, { -73.86f, 3.92f, -439.52f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -67.60f, 3.61f, -436.25f }, { -73.86f, 3.92f, -439.52f }, { -67.60f, 3.92f, -436.25f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -50.34f, 3.61f, -443.47f }, { -53.43f, 3.92f, -446.62f }, { -53.43f, 3.61f, -446.62f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -53.43f, 3.92f, -446.62f }, { -50.34f, 3.61f, -443.47f }, { -50.34f, 3.92f, -443.47f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -68.57f, 3.92f, -457.81f }, { -53.43f, 3.61f, -446.62f }, { -53.43f, 3.92f, -446.62f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -53.43f, 3.61f, -446.62f }, { -68.57f, 3.92f, -457.81f }, { -68.57f, 3.61f, -457.81f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -79.93f, 3.92f, -464.69f }, { -68.57f, 3.61f, -457.81f }, { -68.57f, 3.92f, -457.81f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -68.57f, 3.61f, -457.81f }, { -79.93f, 3.92f, -464.69f }, { -79.93f, 3.61f, -464.69f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -46.25f, 3.61f, -435.16f }, { -47.94f, 3.92f, -439.89f }, { -47.94f, 3.61f, -439.89f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -47.94f, 3.92f, -439.89f }, { -46.25f, 3.61f, -435.16f }, { -46.25f, 3.92f, -435.16f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -43.45f, 3.61f, -421.56f }, { -67.60f, 3.61f, -436.25f }, { -67.60f, 3.92f, -436.25f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -43.45f, 3.61f, -421.56f }, { -67.60f, 3.92f, -436.25f }, { -43.44f, 3.93f, -421.56f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -43.45f, 3.61f, -421.56f }, { -46.25f, 3.92f, -435.16f }, { -46.25f, 3.61f, -435.16f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -46.25f, 3.92f, -435.16f }, { -43.44f, 3.61f, -421.56f }, { -43.44f, 3.93f, -421.56f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -47.94f, 3.61f, -439.89f }, { -50.34f, 3.92f, -443.47f }, { -50.34f, 3.61f, -443.47f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -50.34f, 3.92f, -443.47f }, { -47.94f, 3.61f, -439.89f }, { -47.94f, 3.92f, -439.89f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -89.03f, 3.92f, -469.19f }, { -98.62f, 3.92f, -472.33f }, { -98.62f, 3.61f, -472.33f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -89.03f, 3.92f, -469.19f }, { -98.62f, 3.61f, -472.33f }, { -89.02f, 3.61f, -469.19f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -98.62f, 3.92f, -472.33f }, { -115.59f, 3.93f, -476.73f }, { -115.59f, 3.61f, -476.73f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -98.62f, 3.92f, -472.33f }, { -115.59f, 3.61f, -476.73f }, { -98.62f, 3.61f, -472.33f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -185.88f, 3.61f, -489.10f }, { -73.86f, 3.92f, -439.52f }, { -73.86f, 3.61f, -439.52f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -73.86f, 3.92f, -439.52f }, { -185.88f, 3.61f, -489.10f }, { -185.88f, 3.92f, -489.10f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -185.88f, 3.92f, -489.10f }, { -115.59f, 3.61f, -476.73f }, { -115.59f, 3.93f, -476.73f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -115.59f, 3.61f, -476.74f }, { -185.88f, 3.92f, -489.10f }, { -185.88f, 3.61f, -489.10f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -171.43f, 3.61f, -454.38f }, { -80.01f, 3.92f, -441.02f }, { -80.01f, 3.61f, -441.02f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -80.01f, 3.92f, -441.02f }, { -171.43f, 3.61f, -454.38f }, { -171.43f, 3.92f, -454.38f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -80.01f, 3.92f, -441.02f }, { -192.18f, 3.92f, -489.99f }, { -192.18f, 3.61f, -489.99f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -80.01f, 3.92f, -441.02f }, { -192.18f, 3.61f, -489.99f }, { -80.01f, 3.61f, -441.02f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -171.43f, 3.61f, -454.38f }, { -184.72f, 3.61f, -454.98f }, { -184.72f, 3.92f, -454.98f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -171.43f, 3.61f, -454.38f }, { -184.72f, 3.92f, -454.98f }, { -171.43f, 3.92f, -454.38f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -198.30f, 3.61f, -455.10f }, { -184.72f, 3.92f, -454.98f }, { -184.72f, 3.61f, -454.98f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -184.72f, 3.92f, -454.98f }, { -198.30f, 3.61f, -455.10f }, { -198.30f, 3.92f, -455.10f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -202.60f, 3.92f, -490.92f }, { -192.18f, 3.61f, -489.99f }, { -192.18f, 3.92f, -489.99f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -192.18f, 3.61f, -489.99f }, { -202.60f, 3.92f, -490.92f }, { -202.60f, 3.61f, -490.92f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -317.17f, 3.61f, -453.07f }, { -198.30f, 3.92f, -455.10f }, { -198.30f, 3.61f, -455.10f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -198.30f, 3.92f, -455.10f }, { -317.17f, 3.61f, -453.07f }, { -317.17f, 3.92f, -453.07f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -218.16f, 3.92f, -490.66f }, { -321.30f, 3.92f, -481.72f }, { -321.30f, 3.61f, -481.72f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -218.16f, 3.92f, -490.66f }, { -321.30f, 3.61f, -481.72f }, { -218.16f, 3.61f, -490.66f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -317.17f, 3.61f, -453.07f }, { -321.30f, 3.92f, -481.72f }, { -317.17f, 3.92f, -453.07f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -321.30f, 3.92f, -481.72f }, { -317.17f, 3.61f, -453.07f }, { -321.30f, 3.61f, -481.72f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -218.16f, 3.92f, -490.66f }, { -202.60f, 3.61f, -490.92f }, { -202.60f, 3.92f, -490.92f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -202.60f, 3.61f, -490.92f }, { -218.16f, 3.92f, -490.66f }, { -218.16f, 3.61f, -490.66f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -198.32f, 3.61f, -454.26f }, { -303.98f, 3.61f, -426.52f }, { -303.98f, 3.92f, -426.52f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -198.32f, 3.61f, -454.26f }, { -303.98f, 3.92f, -426.52f }, { -198.32f, 3.92f, -454.26f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -198.32f, 3.92f, -454.26f }, { -316.71f, 3.92f, -451.93f }, { -316.71f, 3.61f, -451.93f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -198.32f, 3.92f, -454.26f }, { -316.71f, 3.61f, -451.93f }, { -198.32f, 3.61f, -454.26f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -303.98f, 3.92f, -426.52f }, { -316.71f, 3.61f, -451.93f }, { -316.71f, 3.92f, -451.93f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { -316.71f, 3.61f, -451.93f }, { -303.98f, 3.92f, -426.52f }, { -303.98f, 3.61f, -426.52f }, 3.61f, 0.00f },   // ground_anchor_04_detail03 ground 16
    { { 28.87f, 0.37f, 547.33f }, { 23.72f, 0.37f, 547.24f }, { 28.77f, 0.37f, 548.13f }, 0.37f, 0.00f },   // ground_bdstatue_06 ground 6
    { { 28.87f, 0.37f, 547.33f }, { 28.80f, 0.37f, 546.53f }, { 23.72f, 0.37f, 547.24f }, 0.37f, 0.00f },   // ground_bdstatue_06 ground 6
    { { 23.72f, 0.37f, 547.24f }, { 28.59f, 0.37f, 548.92f }, { 28.77f, 0.37f, 548.13f }, 0.37f, 0.00f },   // ground_bdstatue_06 ground 6
    { { 23.72f, 0.37f, 547.24f }, { 28.80f, 0.37f, 546.53f }, { 28.64f, 0.37f, 545.74f }, 0.37f, 0.00f },   // ground_bdstatue_06 ground 6
    { { 23.72f, 0.37f, 547.24f }, { 28.25f, 0.37f, 549.65f }, { 28.59f, 0.37f, 548.92f }, 0.37f, 0.00f },   // ground_bdstatue_06 ground 6
    { { 23.72f, 0.37f, 547.24f }, { 28.64f, 0.37f, 545.74f }, { 28.33f, 0.37f, 544.99f }, 0.37f, 0.00f },   // ground_bdstatue_06 ground 6
    { { 27.83f, 0.37f, 550.34f }, { 28.25f, 0.37f, 549.65f }, { 23.72f, 0.37f, 547.24f }, 0.37f, 0.00f },   // ground_bdstatue_06 ground 6
    { { 27.94f, 0.37f, 544.29f }, { 23.72f, 0.37f, 547.24f }, { 28.33f, 0.37f, 544.99f }, 0.37f, 0.00f },   // ground_bdstatue_06 ground 6
    { { 27.83f, 0.37f, 550.34f }, { 23.72f, 0.37f, 547.24f }, { 27.28f, 0.37f, 550.93f }, 0.37f, 0.00f },   // ground_bdstatue_06 ground 6
    { { 27.94f, 0.37f, 544.29f }, { 27.41f, 0.37f, 543.68f }, { 23.72f, 0.37f, 547.24f }, 0.37f, 0.00f },   // ground_bdstatue_06 ground 6
    { { 23.72f, 0.37f, 547.24f }, { 26.67f, 0.37f, 551.46f }, { 27.28f, 0.37f, 550.93f }, 0.37f, 0.00f },   // ground_bdstatue_06 ground 6
    { { 23.72f, 0.37f, 547.24f }, { 27.41f, 0.37f, 543.68f }, { 26.82f, 0.37f, 543.13f }, 0.37f, 0.00f },   // ground_bdstatue_06 ground 6
    { { 23.72f, 0.37f, 547.24f }, { 25.96f, 0.37f, 551.85f }, { 26.67f, 0.37f, 551.46f }, 0.37f, 0.00f },   // ground_bdstatue_06 ground 6
    { { 23.72f, 0.37f, 547.24f }, { 26.82f, 0.37f, 543.13f }, { 26.13f, 0.37f, 542.71f }, 0.37f, 0.00f },   // ground_bdstatue_06 ground 6
    { { 25.22f, 0.37f, 552.16f }, { 25.96f, 0.37f, 551.85f }, { 23.72f, 0.37f, 547.24f }, 0.37f, 0.00f },   // ground_bdstatue_06 ground 6
    { { 25.40f, 0.37f, 542.37f }, { 23.72f, 0.37f, 547.24f }, { 26.13f, 0.37f, 542.71f }, 0.37f, 0.00f },   // ground_bdstatue_06 ground 6
    { { 25.22f, 0.37f, 552.16f }, { 23.72f, 0.37f, 547.24f }, { 24.43f, 0.37f, 552.32f }, 0.37f, 0.00f },   // ground_bdstatue_06 ground 6
    { { 25.40f, 0.37f, 542.37f }, { 24.61f, 0.37f, 542.19f }, { 23.72f, 0.37f, 547.24f }, 0.37f, 0.00f },   // ground_bdstatue_06 ground 6
    { { 23.72f, 0.37f, 547.24f }, { 23.63f, 0.37f, 552.39f }, { 24.43f, 0.37f, 552.32f }, 0.37f, 0.00f },   // ground_bdstatue_06 ground 6
    { { 23.72f, 0.37f, 547.24f }, { 24.61f, 0.37f, 542.19f }, { 23.81f, 0.37f, 542.09f }, 0.37f, 0.00f },   // ground_bdstatue_06 ground 6
    { { 23.72f, 0.37f, 547.24f }, { 22.83f, 0.37f, 552.29f }, { 23.63f, 0.37f, 552.39f }, 0.37f, 0.00f },   // ground_bdstatue_06 ground 6
    { { 23.72f, 0.37f, 547.24f }, { 23.81f, 0.37f, 542.09f }, { 23.01f, 0.37f, 542.16f }, 0.37f, 0.00f },   // ground_bdstatue_06 ground 6
    { { 22.04f, 0.37f, 552.11f }, { 22.83f, 0.37f, 552.29f }, { 23.72f, 0.37f, 547.24f }, 0.37f, 0.00f },   // ground_bdstatue_06 ground 6
    { { 22.21f, 0.37f, 542.31f }, { 23.72f, 0.37f, 547.24f }, { 23.01f, 0.37f, 542.16f }, 0.37f, 0.00f },   // ground_bdstatue_06 ground 6
    { { 22.04f, 0.37f, 552.11f }, { 23.72f, 0.37f, 547.24f }, { 21.31f, 0.37f, 551.77f }, 0.37f, 0.00f },   // ground_bdstatue_06 ground 6
    { { 22.21f, 0.37f, 542.31f }, { 21.47f, 0.37f, 542.63f }, { 23.72f, 0.37f, 547.24f }, 0.37f, 0.00f },   // ground_bdstatue_06 ground 6
    { { 23.72f, 0.37f, 547.24f }, { 20.62f, 0.37f, 551.35f }, { 21.31f, 0.37f, 551.77f }, 0.37f, 0.00f },   // ground_bdstatue_06 ground 6
    { { 23.72f, 0.37f, 547.24f }, { 21.47f, 0.37f, 542.63f }, { 20.77f, 0.37f, 543.02f }, 0.37f, 0.00f },   // ground_bdstatue_06 ground 6
    { { 23.72f, 0.37f, 547.24f }, { 20.03f, 0.37f, 550.80f }, { 20.62f, 0.37f, 551.35f }, 0.37f, 0.00f },   // ground_bdstatue_06 ground 6
    { { 23.72f, 0.37f, 547.24f }, { 20.77f, 0.37f, 543.02f }, { 20.16f, 0.37f, 543.55f }, 0.37f, 0.00f },   // ground_bdstatue_06 ground 6
    { { 19.50f, 0.37f, 550.19f }, { 20.03f, 0.37f, 550.80f }, { 23.72f, 0.37f, 547.24f }, 0.37f, 0.00f },   // ground_bdstatue_06 ground 6
    { { 19.61f, 0.37f, 544.14f }, { 23.72f, 0.37f, 547.24f }, { 20.16f, 0.37f, 543.55f }, 0.37f, 0.00f },   // ground_bdstatue_06 ground 6
    { { 19.50f, 0.37f, 550.19f }, { 23.72f, 0.37f, 547.24f }, { 19.11f, 0.37f, 549.49f }, 0.37f, 0.00f },   // ground_bdstatue_06 ground 6
    { { 19.61f, 0.37f, 544.14f }, { 19.19f, 0.37f, 544.83f }, { 23.72f, 0.37f, 547.24f }, 0.37f, 0.00f },   // ground_bdstatue_06 ground 6
    { { 23.72f, 0.37f, 547.24f }, { 18.79f, 0.37f, 548.74f }, { 19.11f, 0.37f, 549.49f }, 0.37f, 0.00f },   // ground_bdstatue_06 ground 6
    { { 23.72f, 0.37f, 547.24f }, { 19.19f, 0.37f, 544.83f }, { 18.85f, 0.37f, 545.56f }, 0.37f, 0.00f },   // ground_bdstatue_06 ground 6
    { { 23.72f, 0.37f, 547.24f }, { 18.85f, 0.37f, 545.56f }, { 18.67f, 0.37f, 546.35f }, 0.37f, 0.00f },   // ground_bdstatue_06 ground 6
    { { 18.57f, 0.37f, 547.15f }, { 23.72f, 0.37f, 547.24f }, { 18.67f, 0.37f, 546.35f }, 0.37f, 0.00f },   // ground_bdstatue_06 ground 6
    { { 18.57f, 0.37f, 547.15f }, { 18.64f, 0.37f, 547.95f }, { 23.72f, 0.37f, 547.24f }, 0.37f, 0.00f },   // ground_bdstatue_06 ground 6
    { { 23.72f, 0.37f, 547.24f }, { 18.64f, 0.37f, 547.95f }, { 18.79f, 0.37f, 548.74f }, 0.37f, 0.00f },   // ground_bdstatue_06 ground 6
    { { 287.69f, 9.97f, -48.42f }, { 208.85f, 9.99f, 109.02f }, { 288.19f, 9.97f, -47.42f }, 4.19f, 0.00f },   // anc_area02_plaza01 ground 8
    { { 287.69f, 9.97f, -48.42f }, { 202.53f, 9.99f, 119.12f }, { 208.85f, 9.99f, 109.02f }, 4.19f, 0.00f },   // anc_area02_plaza01 ground 8
    { { 208.85f, 9.99f, 109.02f }, { 202.53f, 9.99f, 119.12f }, { 204.22f, 9.99f, 118.13f }, 4.19f, 0.00f },   // anc_area02_plaza01 ground 8
    { { 252.90f, 9.98f, 38.18f }, { 218.22f, 9.99f, 109.94f }, { 219.72f, 9.99f, 109.06f }, 4.19f, 0.00f },   // anc_area02_plaza01 ground 8
    { { 219.72f, 9.99f, 109.06f }, { 254.24f, 9.98f, 37.89f }, { 252.90f, 9.98f, 38.18f }, 4.19f, 0.00f },   // anc_area02_plaza01 ground 8
    { { 277.78f, 9.97f, -72.99f }, { 189.75f, 9.99f, 48.72f }, { 190.82f, 9.99f, 48.47f }, 4.19f, 0.00f },   // anc_area02_plaza01 ground 8
    { { 190.82f, 9.99f, 48.47f }, { 278.64f, 9.97f, -72.31f }, { 277.78f, 9.97f, -72.99f }, 4.19f, 0.00f },   // anc_area02_plaza01 ground 8
    { { 394.11f, 9.98f, -230.75f }, { 277.78f, 9.97f, -72.99f }, { 278.64f, 9.97f, -72.31f }, 4.19f, 0.00f },   // anc_area02_plaza01 ground 8
    { { 278.64f, 9.97f, -72.31f }, { 396.10f, 9.98f, -231.66f }, { 394.11f, 9.98f, -230.75f }, 4.19f, 0.00f },   // anc_area02_plaza01 ground 8
    { { 341.86f, 9.97f, -124.53f }, { 291.27f, 9.97f, -88.59f }, { 341.67f, 9.97f, -123.34f }, 4.19f, 0.00f },   // anc_area02_plaza01 ground 8
    { { 289.55f, 9.97f, -86.26f }, { 341.67f, 9.97f, -123.34f }, { 291.27f, 9.97f, -88.59f }, 4.19f, 0.00f },   // anc_area02_plaza01 ground 8
    { { 378.23f, 9.98f, -141.50f }, { 416.47f, 9.98f, -167.10f }, { 417.49f, 9.98f, -169.06f }, 4.19f, 0.00f },   // anc_area02_plaza01 ground 8
    { { 416.48f, 9.98f, -167.10f }, { 378.23f, 9.98f, -141.50f }, { 378.15f, 9.98f, -140.27f }, 4.19f, 0.00f },   // anc_area02_plaza01 ground 8
    { { 373.66f, 9.98f, -200.17f }, { 350.22f, 9.98f, -168.42f }, { 379.17f, 9.98f, -184.35f }, 4.19f, 0.00f },   // anc_area02_plaza01 ground 8
    { { 373.66f, 9.98f, -200.17f }, { 379.17f, 9.98f, -184.35f }, { 439.18f, 9.98f, -217.37f }, 4.19f, 0.00f },   // anc_area02_plaza01 ground 8
    { { 153.40f, 9.99f, 214.46f }, { 30.70f, 9.99f, 274.68f }, { 117.76f, 9.99f, 284.66f }, 4.19f, 0.00f },   // anc_area02_plaza01 ground 8
    { { 159.93f, 9.99f, 183.20f }, { 30.70f, 9.99f, 274.68f }, { 153.40f, 9.99f, 214.46f }, 4.19f, 0.00f },   // anc_area02_plaza01 ground 8
    { { 155.00f, 9.99f, 212.59f }, { 123.20f, 9.99f, 275.14f }, { 125.96f, 9.99f, 272.18f }, 4.19f, 0.00f },   // anc_area02_plaza01 ground 8
    { { 125.96f, 9.99f, 272.18f }, { 177.47f, 9.99f, 170.80f }, { 155.00f, 9.99f, 212.59f }, 4.19f, 0.00f },   // anc_area02_plaza01 ground 8
    { { 161.46f, 9.99f, 181.96f }, { 155.00f, 9.99f, 212.59f }, { 177.47f, 9.99f, 170.80f }, 4.19f, 0.00f },   // anc_area02_plaza01 ground 8
    { { 177.18f, 9.99f, 196.57f }, { 193.45f, 9.99f, 161.16f }, { 176.41f, 9.99f, 196.22f }, 4.19f, 0.00f },   // anc_area02_plaza01 ground 8
    { { 177.18f, 9.99f, 196.57f }, { 194.89f, 9.99f, 160.14f }, { 193.45f, 9.99f, 161.16f }, 4.19f, 0.00f },   // anc_area02_plaza01 ground 8
    { { 144.22f, 9.99f, 264.38f }, { 172.73f, 9.99f, 205.72f }, { 144.88f, 9.99f, 261.09f }, 4.19f, 0.00f },   // anc_area02_plaza01 ground 8
    { { 171.97f, 9.99f, 205.36f }, { 144.88f, 9.99f, 261.09f }, { 172.73f, 9.99f, 205.72f }, 4.19f, 0.00f },   // anc_area02_plaza01 ground 8
    { { 255.16f, 9.98f, 33.50f }, { 320.98f, 9.97f, 13.65f }, { 292.42f, 9.97f, -43.60f }, 4.19f, 0.00f },   // anc_area02_plaza01 ground 8
    { { 320.98f, 9.97f, 13.65f }, { 255.16f, 9.98f, 33.50f }, { 320.78f, 9.97f, 18.23f }, 4.19f, 0.00f },   // anc_area02_plaza01 ground 8
    { { 320.78f, 9.97f, 18.23f }, { 371.82f, 9.98f, -81.52f }, { 320.98f, 9.97f, 13.65f }, 4.19f, 0.00f },   // anc_area02_plaza01 ground 8
    { { 372.12f, 9.98f, -86.82f }, { 320.98f, 9.97f, 13.65f }, { 371.82f, 9.98f, -81.52f }, 4.19f, 0.00f },   // anc_area02_plaza01 ground 8
    { { 289.25f, 9.97f, -53.14f }, { 287.12f, 9.97f, -82.96f }, { 279.49f, 9.97f, -72.63f }, 4.19f, 0.00f },   // anc_area02_plaza01 ground 8
    { { 287.12f, 9.97f, -82.96f }, { 289.25f, 9.97f, -53.14f }, { 313.89f, 9.97f, -101.59f }, 4.19f, 0.00f },   // anc_area02_plaza01 ground 8
    { { 320.66f, 9.97f, 9.63f }, { 318.10f, 9.97f, -96.75f }, { 293.21f, 9.97f, -45.24f }, 4.19f, 0.00f },   // anc_area02_plaza01 ground 8
    { { 318.10f, 9.97f, -96.75f }, { 320.66f, 9.97f, 9.63f }, { 339.87f, 9.97f, -111.98f }, 4.19f, 0.00f },   // anc_area02_plaza01 ground 8
    { { 73.74f, 4.19f, 376.95f }, { 117.17f, 10.41f, 289.39f }, { 28.01f, 10.40f, 279.28f }, 4.19f, 0.00f },   // anc_area02_plaza01 ground 8
    { { 160.62f, 9.51f, 179.90f }, { 87.70f, 9.51f, 189.58f }, { 26.84f, 9.51f, 273.53f }, 4.19f, 0.00f },   // anc_area02_plaza01 ground 8
    { { 87.70f, 9.51f, 189.57f }, { 160.61f, 9.51f, 179.90f }, { 168.18f, 9.51f, 142.44f }, 4.19f, 0.00f },   // anc_area02_plaza01 ground 8
    { { 169.57f, 9.51f, 141.59f }, { 162.14f, 9.51f, 178.67f }, { 179.59f, 9.51f, 166.61f }, 4.19f, 0.00f },   // anc_area02_plaza01 ground 8
    { { 179.59f, 9.51f, 166.61f }, { 201.92f, 9.51f, 122.66f }, { 169.57f, 9.51f, 141.59f }, 4.19f, 0.00f },   // anc_area02_plaza01 ground 8
    { { 424.34f, 9.50f, -182.16f }, { 443.21f, 9.50f, -218.29f }, { 381.00f, 9.49f, -184.14f }, 4.19f, 0.00f },   // anc_area02_plaza01 ground 8
    { { 381.00f, 9.49f, -184.15f }, { 378.76f, 9.49f, -149.73f }, { 424.34f, 9.50f, -182.16f }, 4.19f, 0.00f },   // anc_area02_plaza01 ground 8
    { { 268.81f, 9.51f, 104.17f }, { 216.06f, 9.51f, 114.40f }, { 196.58f, 9.51f, 154.72f }, 4.19f, 0.00f },   // anc_area02_plaza01 ground 8
    { { 216.06f, 9.51f, 114.40f }, { 268.81f, 9.51f, 104.17f }, { 277.06f, 9.51f, 78.80f }, 4.19f, 0.00f },   // anc_area02_plaza01 ground 8
    { { 341.27f, 9.49f, -156.30f }, { 292.89f, 9.49f, -90.78f }, { 342.03f, 9.49f, -125.64f }, 4.19f, 0.00f },   // anc_area02_plaza01 ground 8
    { { 342.03f, 9.49f, -125.64f }, { 347.41f, 9.50f, -159.81f }, { 341.27f, 9.50f, -156.30f }, 4.19f, 0.00f },   // anc_area02_plaza01 ground 8
    { { 294.29f, 9.99f, 70.27f }, { 319.49f, 9.97f, 20.76f }, { 308.67f, 9.98f, 23.17f }, 4.19f, 0.00f },   // anc_area02_plaza01 ground 8
    { { 209.66f, 18.79f, 167.06f }, { 246.59f, 18.79f, 140.56f }, { 246.27f, 18.79f, 140.37f }, 4.19f, 0.00f },   // anc_area02_plaza01 ground 8
    { { 246.59f, 18.79f, 140.56f }, { 209.66f, 18.79f, 167.06f }, { 163.99f, 18.79f, 279.08f }, 4.19f, 0.00f },   // anc_area02_plaza01 ground 8
    { { 209.66f, 18.79f, 167.06f }, { 163.15f, 18.79f, 278.68f }, { 163.99f, 18.79f, 279.08f }, 4.19f, 0.00f },   // anc_area02_plaza01 ground 8
    { { 263.79f, 9.98f, -13.46f }, { 288.45f, 9.97f, -51.56f }, { 265.87f, 9.98f, -18.00f }, 4.19f, 0.00f },   // anc_area02_plaza01 ground 8
    { { 288.45f, 9.97f, -51.56f }, { 261.35f, 9.98f, -20.15f }, { 265.87f, 9.98f, -18.00f }, 4.19f, 0.00f },   // anc_area02_plaza01 ground 8
    { { 243.94f, 9.98f, 36.11f }, { 288.45f, 9.97f, -51.56f }, { 263.79f, 9.98f, -13.46f }, 4.19f, 0.00f },   // anc_area02_plaza01 ground 8
    { { 243.94f, 9.98f, 36.11f }, { 263.79f, 9.98f, -13.46f }, { 259.22f, 9.98f, -15.52f }, 4.19f, 0.00f },   // anc_area02_plaza01 ground 8
    { { 261.36f, 9.98f, -20.15f }, { 288.45f, 9.97f, -51.56f }, { 278.54f, 9.97f, -71.43f }, 4.19f, 0.00f },   // anc_area02_plaza01 ground 8
    { { 261.36f, 9.98f, -20.15f }, { 257.64f, 9.98f, -15.91f }, { 259.22f, 9.98f, -15.52f }, 4.19f, 0.00f },   // anc_area02_plaza01 ground 8
    { { 278.54f, 9.97f, -71.43f }, { 257.64f, 9.98f, -15.91f }, { 261.35f, 9.98f, -20.15f }, 4.19f, 0.00f },   // anc_area02_plaza01 ground 8
    { { 259.22f, 9.98f, -15.53f }, { 255.78f, 9.98f, -11.21f }, { 243.94f, 9.98f, 36.11f }, 4.19f, 0.00f },   // anc_area02_plaza01 ground 8
    { { 255.78f, 9.98f, -11.21f }, { 259.22f, 9.98f, -15.52f }, { 257.64f, 9.98f, -15.91f }, 4.19f, 0.00f },   // anc_area02_plaza01 ground 8
    { { 278.54f, 9.97f, -71.43f }, { 252.89f, 9.98f, -17.76f }, { 257.64f, 9.98f, -15.91f }, 4.19f, 0.00f },   // anc_area02_plaza01 ground 8
    { { 278.54f, 9.97f, -71.43f }, { 191.56f, 9.99f, 48.30f }, { 252.89f, 9.98f, -17.76f }, 4.19f, 0.00f },   // anc_area02_plaza01 ground 8
    { { 251.02f, 9.98f, -13.14f }, { 191.56f, 9.99f, 48.30f }, { 243.94f, 9.98f, 36.11f }, 4.19f, 0.00f },   // anc_area02_plaza01 ground 8
    { { 251.02f, 9.98f, -13.14f }, { 243.94f, 9.98f, 36.11f }, { 255.78f, 9.98f, -11.21f }, 4.19f, 0.00f },   // anc_area02_plaza01 ground 8
    { { 252.89f, 9.98f, -17.76f }, { 191.56f, 9.99f, 48.30f }, { 251.02f, 9.98f, -13.14f }, 4.19f, 0.00f },   // anc_area02_plaza01 ground 8
    { { 249.16f, -0.22f, -271.30f }, { 148.12f, -0.22f, -474.01f }, { 145.03f, -0.22f, -470.36f }, -0.23f, 0.00f },   // ground_anchor_05 ground 9
    { { 258.06f, -0.22f, -264.31f }, { 148.12f, -0.22f, -474.01f }, { 249.16f, -0.22f, -271.30f }, -0.23f, 0.00f },   // ground_anchor_05 ground 9
    { { 87.07f, -0.23f, -401.87f }, { 220.28f, -0.22f, -293.96f }, { 114.12f, -0.23f, -434.03f }, -0.23f, 0.00f },   // ground_anchor_05 ground 9
    { { 223.54f, -0.23f, -291.41f }, { 142.28f, -0.22f, -467.15f }, { 115.13f, -0.23f, -435.26f }, -0.23f, 0.00f },   // ground_anchor_05 ground 9
    { { 243.59f, -0.22f, -275.66f }, { 142.28f, -0.22f, -467.15f }, { 223.54f, -0.22f, -291.41f }, -0.23f, 0.00f },   // ground_anchor_05 ground 9
    { { 413.62f, -64.07f, -343.21f }, { 316.02f, -64.07f, -298.71f }, { 383.62f, -64.07f, -313.71f }, -64.39f, 0.00f },   // ground_anchor_08 ground 9
    { { 316.02f, -64.07f, -298.71f }, { 413.62f, -64.07f, -343.21f }, { 183.93f, -64.07f, -463.90f }, -64.39f, 0.00f },   // ground_anchor_08 ground 9
    { { 316.02f, -64.07f, -298.71f }, { 183.93f, -64.07f, -463.90f }, { 152.68f, -64.07f, -426.46f }, -64.39f, 0.00f },   // ground_anchor_08 ground 9
    { { 183.93f, -64.07f, -463.90f }, { 413.62f, -64.07f, -343.21f }, { 225.87f, -63.98f, -514.09f }, -64.39f, 0.00f },   // ground_anchor_08 ground 9
    { { 225.87f, -63.98f, -514.09f }, { 413.62f, -64.07f, -343.21f }, { 456.37f, -64.23f, -385.34f }, -64.39f, 0.00f },   // ground_anchor_08 ground 9
    { { 477.49f, -64.39f, -414.09f }, { 225.87f, -63.98f, -514.09f }, { 456.37f, -64.23f, -385.34f }, -64.39f, 0.00f },   // ground_anchor_08 ground 9
    { { 225.87f, -63.98f, -514.09f }, { 477.49f, -64.39f, -414.09f }, { 220.87f, -63.98f, -563.18f }, -64.39f, 0.00f },   // ground_anchor_08 ground 9
    { { 220.87f, -63.98f, -563.18f }, { 477.49f, -64.39f, -414.09f }, { 355.74f, -64.14f, -513.65f }, -64.39f, 0.00f },   // ground_anchor_08 ground 9
    { { 495.62f, -63.57f, -565.29f }, { 355.74f, -64.14f, -513.65f }, { 477.49f, -64.39f, -414.09f }, -64.39f, 0.00f },   // ground_anchor_08 ground 9
    { { 276.01f, -63.98f, -607.34f }, { 220.87f, -63.98f, -563.18f }, { 355.74f, -64.14f, -513.65f }, -64.39f, 0.00f },   // ground_anchor_08 ground 9
    { { 265.83f, -63.98f, -612.28f }, { 220.87f, -63.98f, -563.18f }, { 276.01f, -63.98f, -607.34f }, -64.39f, 0.00f },   // ground_anchor_08 ground 9
    { { 355.74f, -64.14f, -513.65f }, { 293.31f, -63.95f, -604.75f }, { 276.01f, -63.98f, -607.34f }, -64.39f, 0.00f },   // ground_anchor_08 ground 9
    { { 355.74f, -64.14f, -513.65f }, { 495.62f, -63.57f, -565.29f }, { 415.12f, -63.73f, -597.48f }, -64.39f, 0.00f },   // ground_anchor_08 ground 9
    { { 293.31f, -63.95f, -604.75f }, { 365.81f, -63.57f, -686.15f }, { 276.01f, -63.98f, -607.34f }, -64.39f, 0.00f },   // ground_anchor_08 ground 9
    { { 293.31f, -63.95f, -604.75f }, { 415.12f, -63.73f, -597.48f }, { 365.81f, -63.57f, -686.15f }, -64.39f, 0.00f },   // ground_anchor_08 ground 9
    { { 365.81f, -63.57f, -686.15f }, { 415.12f, -63.73f, -597.48f }, { 495.62f, -63.57f, -565.29f }, -64.39f, 0.00f },   // ground_anchor_08 ground 9
    { { -279.78f, -0.06f, -580.26f }, { -270.78f, -0.06f, -558.01f }, { -164.28f, -0.06f, -530.01f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { -164.28f, -0.06f, -530.01f }, { -270.78f, -0.06f, -558.01f }, { -262.78f, -0.06f, -545.26f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { -164.28f, -0.06f, -530.01f }, { -262.78f, -0.06f, -545.26f }, { -250.78f, -0.06f, -534.01f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { -164.28f, -0.06f, -530.01f }, { -250.78f, -0.06f, -534.01f }, { -234.28f, -0.06f, -524.26f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { -164.28f, -0.06f, -530.01f }, { -234.28f, -0.06f, -524.26f }, { -178.28f, -0.06f, -497.64f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { -178.28f, -0.06f, -497.64f }, { -171.78f, -0.06f, -495.01f }, { -164.28f, -0.06f, -530.01f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { -171.78f, -0.06f, -495.01f }, { -165.78f, -0.06f, -493.26f }, { -164.28f, -0.06f, -530.01f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { -165.78f, -0.06f, -493.26f }, { -109.28f, -0.06f, -480.76f }, { -164.28f, -0.06f, -530.01f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { -164.28f, -0.06f, -530.01f }, { -109.28f, -0.06f, -480.76f }, { -94.03f, -0.06f, -476.64f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { 11.47f, -0.06f, -434.76f }, { -41.78f, -0.06f, -445.51f }, { 20.72f, -0.06f, -399.01f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { 8.97f, -0.06f, -442.14f }, { -41.78f, -0.06f, -445.51f }, { 11.47f, -0.06f, -434.76f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { 4.97f, -0.06f, -449.51f }, { -41.78f, -0.06f, -445.51f }, { 8.97f, -0.06f, -442.14f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { 1.47f, -0.06f, -454.39f }, { -41.78f, -0.06f, -445.51f }, { 4.97f, -0.06f, -449.51f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { -3.28f, -0.06f, -458.51f }, { -41.78f, -0.06f, -445.51f }, { 1.47f, -0.06f, -454.39f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { -78.03f, -0.06f, -515.76f }, { -41.78f, -0.06f, -445.51f }, { -3.28f, -0.06f, -458.51f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { -85.78f, -0.06f, -520.26f }, { -41.78f, -0.06f, -445.51f }, { -78.03f, -0.06f, -515.76f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { -85.78f, -0.06f, -520.26f }, { -63.53f, -0.06f, -460.51f }, { -41.78f, -0.06f, -445.51f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { -85.78f, -0.06f, -520.26f }, { -76.03f, -0.06f, -468.14f }, { -63.53f, -0.06f, -460.51f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { -85.78f, -0.06f, -520.26f }, { -86.28f, -0.06f, -473.39f }, { -76.03f, -0.06f, -468.14f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { -86.28f, -0.06f, -473.39f }, { -85.78f, -0.06f, -520.26f }, { -92.53f, -0.06f, -522.01f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { -86.28f, -0.06f, -473.39f }, { -92.53f, -0.06f, -522.01f }, { -99.78f, -0.06f, -523.01f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { -86.28f, -0.06f, -473.39f }, { -99.78f, -0.06f, -523.01f }, { -152.28f, -0.06f, -526.26f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { -152.28f, -0.06f, -526.26f }, { -158.28f, -0.06f, -527.51f }, { -86.28f, -0.06f, -473.39f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { -76.03f, 0.10f, -468.14f }, { -86.28f, -0.06f, -473.39f }, { -86.28f, 0.10f, -473.39f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { -86.28f, -0.06f, -473.39f }, { -76.03f, 0.10f, -468.14f }, { -76.03f, -0.06f, -468.14f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { -63.53f, -0.06f, -460.51f }, { -76.03f, -0.06f, -468.14f }, { -76.03f, 0.10f, -468.14f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { -63.53f, -0.06f, -460.51f }, { -76.03f, 0.10f, -468.14f }, { -63.53f, 0.10f, -460.51f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { -85.78f, 0.10f, -520.26f }, { -92.53f, 0.10f, -522.01f }, { -92.53f, -0.06f, -522.01f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { -85.78f, 0.10f, -520.26f }, { -92.53f, -0.06f, -522.01f }, { -85.78f, -0.06f, -520.26f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { -78.03f, -0.06f, -515.76f }, { -85.78f, 0.10f, -520.26f }, { -85.78f, -0.06f, -520.26f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { -85.78f, 0.10f, -520.26f }, { -78.03f, -0.06f, -515.76f }, { -78.03f, 0.10f, -515.76f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { 1.47f, 0.10f, -454.39f }, { 4.97f, -0.06f, -449.51f }, { 4.97f, 0.10f, -449.51f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { 4.97f, -0.06f, -449.51f }, { 1.47f, 0.10f, -454.39f }, { 1.47f, -0.06f, -454.39f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { -3.28f, 0.10f, -458.51f }, { 1.47f, -0.06f, -454.39f }, { 1.47f, 0.10f, -454.39f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { 1.47f, -0.06f, -454.39f }, { -3.28f, 0.10f, -458.51f }, { -3.28f, -0.06f, -458.51f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { -78.03f, 0.10f, -515.76f }, { -3.28f, -0.06f, -458.51f }, { -3.28f, 0.10f, -458.51f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { -3.28f, -0.06f, -458.51f }, { -78.03f, 0.10f, -515.76f }, { -78.03f, -0.06f, -515.76f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { -41.78f, -0.06f, -445.51f }, { -63.53f, -0.06f, -460.51f }, { -63.53f, 0.10f, -460.51f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { -41.78f, -0.06f, -445.51f }, { -63.53f, 0.10f, -460.51f }, { -41.78f, 0.10f, -445.51f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { 20.72f, -0.06f, -399.01f }, { -41.78f, -0.06f, -445.51f }, { -41.78f, 0.10f, -445.51f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { 20.72f, -0.06f, -399.01f }, { -41.78f, 0.10f, -445.51f }, { 20.72f, 0.10f, -399.01f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { -99.78f, 0.10f, -523.01f }, { -152.28f, 0.10f, -526.26f }, { -152.28f, -0.06f, -526.26f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { -99.78f, 0.10f, -523.01f }, { -152.28f, -0.06f, -526.26f }, { -99.78f, -0.06f, -523.01f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { -92.53f, -0.06f, -522.01f }, { -99.78f, 0.10f, -523.01f }, { -99.78f, -0.06f, -523.01f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { -99.78f, 0.10f, -523.01f }, { -92.53f, -0.06f, -522.01f }, { -92.53f, 0.10f, -522.01f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { 11.47f, 0.10f, -434.76f }, { 20.72f, -0.06f, -399.01f }, { 20.72f, 0.10f, -399.01f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { 20.72f, -0.06f, -399.01f }, { 11.47f, 0.10f, -434.76f }, { 11.47f, -0.06f, -434.76f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { 8.97f, -0.06f, -442.14f }, { 11.47f, -0.06f, -434.76f }, { 11.47f, 0.10f, -434.76f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { 11.47f, 0.10f, -434.76f }, { 8.97f, 0.10f, -442.14f }, { 8.97f, -0.06f, -442.14f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { 4.97f, -0.06f, -449.51f }, { 8.97f, -0.06f, -442.14f }, { 8.97f, 0.10f, -442.14f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { 8.97f, 0.10f, -442.14f }, { 4.97f, 0.10f, -449.51f }, { 4.97f, -0.06f, -449.51f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { -158.28f, 0.10f, -527.51f }, { -152.28f, -0.06f, -526.26f }, { -152.28f, 0.10f, -526.26f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { -152.28f, -0.06f, -526.26f }, { -158.28f, 0.10f, -527.51f }, { -158.28f, -0.06f, -527.51f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { -158.28f, -0.06f, -527.51f }, { -86.28f, 0.10f, -473.39f }, { -86.28f, -0.06f, -473.39f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { -86.28f, 0.10f, -473.39f }, { -158.28f, -0.06f, -527.51f }, { -158.28f, 0.10f, -527.51f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { -279.78f, 0.10f, -580.26f }, { -270.78f, -0.06f, -558.01f }, { -279.78f, -0.06f, -580.26f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { -270.78f, -0.06f, -558.01f }, { -279.78f, 0.10f, -580.26f }, { -270.78f, 0.10f, -558.01f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { -262.78f, 0.10f, -545.26f }, { -270.78f, -0.06f, -558.01f }, { -270.78f, 0.10f, -558.01f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { -270.78f, -0.06f, -558.01f }, { -262.78f, 0.10f, -545.26f }, { -262.78f, -0.06f, -545.26f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { -250.78f, 0.10f, -534.01f }, { -262.78f, -0.06f, -545.26f }, { -262.78f, 0.10f, -545.26f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { -262.78f, -0.06f, -545.26f }, { -250.78f, 0.10f, -534.01f }, { -250.78f, -0.06f, -534.01f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { -234.28f, 0.09f, -524.26f }, { -250.78f, -0.06f, -534.01f }, { -250.78f, 0.10f, -534.01f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { -250.78f, -0.06f, -534.01f }, { -234.28f, 0.08f, -524.26f }, { -234.28f, -0.06f, -524.26f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { -178.28f, -0.06f, -497.64f }, { -234.28f, -0.06f, -524.26f }, { -234.28f, 0.08f, -524.26f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { -178.28f, -0.06f, -497.64f }, { -234.28f, 0.08f, -524.26f }, { -178.28f, 0.10f, -497.64f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { -171.78f, -0.06f, -495.01f }, { -178.28f, -0.06f, -497.64f }, { -178.28f, 0.10f, -497.64f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { -171.78f, -0.06f, -495.01f }, { -178.28f, 0.10f, -497.64f }, { -171.78f, 0.10f, -495.01f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { -165.78f, -0.06f, -493.26f }, { -171.78f, -0.06f, -495.01f }, { -171.78f, 0.10f, -495.01f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { -165.78f, -0.06f, -493.26f }, { -171.78f, 0.10f, -495.01f }, { -165.78f, 0.10f, -493.26f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { -109.28f, -0.06f, -480.76f }, { -165.78f, -0.06f, -493.26f }, { -165.78f, 0.10f, -493.26f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { -109.28f, -0.06f, -480.76f }, { -165.78f, 0.10f, -493.26f }, { -109.28f, 0.10f, -480.76f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { -94.03f, 0.10f, -476.64f }, { -109.28f, -0.06f, -480.76f }, { -109.28f, 0.10f, -480.76f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { -109.28f, -0.06f, -480.76f }, { -94.03f, 0.10f, -476.64f }, { -94.03f, -0.06f, -476.64f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { -279.78f, 0.10f, -580.26f }, { -164.28f, -0.06f, -530.01f }, { -164.28f, 0.10f, -530.01f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { -164.28f, -0.06f, -530.01f }, { -279.78f, 0.10f, -580.26f }, { -279.78f, -0.06f, -580.26f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { -164.28f, 0.10f, -530.01f }, { -94.03f, -0.06f, -476.64f }, { -94.03f, 0.10f, -476.64f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
    { { -94.03f, -0.06f, -476.64f }, { -164.28f, 0.10f, -530.01f }, { -164.28f, -0.06f, -530.01f }, -0.06f, 0.00f },   // ground_anchor_04_a ground 9
  };
}
