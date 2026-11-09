#pragma once
// The Runners' landing pads in the city (2026-10-28, the user: "landing on a jumppad should leave a ripple", "i mean
// landingpads", with a screenshot of one): the soft-landing tarps (objects/props/construction/constructiontent/tarp01,
// with the game's PamSoftLandingEntity in pf_softlanding_constructiontent_01), placed in the city's zones as static
// model group members (their transforms from the zones' group collision, staticmodelgroup_physics_win32's hknp
// instances). Generated offline from the game's data by the weather session's tarps_find.py (read-only). Per pad its
// tarp's box (its mesh's bounds where it's placed): the middle and its three half axes (world, metres). WetLook.cpp
// rings the tarp where Faith comes down on it (NoteLandingPad).
struct LandingPadData { float Middle[3]; float Half[3][3]; };
const LandingPadData LandingPads[] = {
  { { -73.329f, 55.653f, 134.535f }, { { -2.257f, 0.000f, 3.044f }, { 0.000f, 0.536f, 0.000f }, { -4.090f, 0.000f, -3.033f } } },   // centralcityprops_zs_zs_150
  { { 81.906f, 55.493f, 138.211f }, { { -3.151f, 0.000f, -2.106f }, { 0.000f, 0.536f, 0.000f }, { 2.829f, 0.000f, -4.233f } } },   // centralcityprops_zs_zs_194
  { { 160.552f, 48.133f, -52.600f }, { { 3.151f, 0.000f, 2.106f }, { 0.000f, 0.536f, 0.000f }, { -2.829f, 0.000f, 4.233f } } },   // centralcityprops_zs_zs_203
  { { -296.722f, 62.919f, 77.368f }, { { -3.645f, 0.000f, -1.038f }, { 0.000f, 0.536f, 0.000f }, { 1.395f, 0.000f, -4.897f } } },   // centralcityprops_zs_zs_211
  { { -435.075f, -0.187f, 306.005f }, { { 3.717f, 0.000f, 0.739f }, { 0.000f, 0.536f, 0.000f }, { -0.993f, 0.000f, 4.994f } } },   // centralcityprops_zs_zs_234
  { { -590.165f, 66.377f, 343.511f }, { { -1.100f, 0.000f, 3.627f }, { 0.000f, 0.536f, 0.000f }, { -4.872f, 0.000f, -1.478f } } },   // centralcityprops_zs_zs_235
  { { -364.264f, 11.013f, 859.331f }, { { -2.724f, 0.000f, 2.635f }, { 0.000f, 0.536f, 0.000f }, { -3.540f, 0.000f, -3.659f } } },   // centralcityprops_zs_zs_269
  { { -15.303f, 44.617f, 605.673f }, { { 0.000f, 0.000f, -3.790f }, { 0.000f, 0.536f, 0.000f }, { 5.091f, 0.000f, 0.000f } } },   // centralcityprops_zs_zs_315
  { { -15.303f, 44.617f, 598.473f }, { { 0.000f, 0.000f, -3.790f }, { 0.000f, 0.536f, 0.000f }, { 5.091f, 0.000f, 0.000f } } },   // centralcityprops_zs_zs_315
  { { 965.966f, 75.032f, 106.757f }, { { 2.404f, 0.000f, -2.929f }, { 0.000f, 0.536f, 0.000f }, { 3.936f, 0.000f, 3.230f } } },   // centralcityprops_zs_zs_417
  { { 1098.982f, 90.718f, 59.972f }, { { 2.420f, 0.000f, -2.917f }, { 0.000f, 0.536f, 0.000f }, { 3.919f, 0.000f, 3.251f } } },   // centralcityprops_zs_zs_420
  { { 389.608f, 63.051f, 99.828f }, { { 2.077f, -0.000f, 3.170f }, { 0.000f, 0.536f, -0.000f }, { -4.259f, 0.000f, 2.790f } } },   // centralcityprops_zs_zs_442
  { { 270.289f, 62.163f, 311.118f }, { { 2.926f, 0.000f, 1.212f }, { 0.000f, 0.448f, 0.000f }, { -1.628f, 0.000f, 3.931f } } },   // centralcityprops_zs_zs_486
  { { -321.188f, -26.666f, 837.727f }, { { 2.282f, 0.000f, 3.025f }, { 0.000f, 0.536f, 0.000f }, { -4.065f, 0.000f, 3.066f } } },   // centralcityprops_zs_zs_605
  { { -266.626f, 19.333f, 753.655f }, { { -2.724f, 0.000f, 2.635f }, { 0.000f, 0.536f, 0.000f }, { -3.540f, 0.000f, -3.659f } } },   // centralcityprops_zs_zs_612
  { { -442.460f, 27.013f, 842.194f }, { { 0.081f, -0.000f, 3.789f }, { 0.000f, 0.536f, 0.000f }, { -5.090f, 0.000f, 0.109f } } },   // centralcityprops_zs_zs_612
  { { -163.361f, 64.154f, -21.107f }, { { 4.082f, 0.000f, -1.691f }, { 0.000f, 0.625f, 0.000f }, { 2.272f, 0.000f, 5.484f } } },   // centralcityprops_zs_zs_642
  { { 399.786f, 65.196f, 299.493f }, { { 3.501f, 0.000f, 1.450f }, { 0.000f, 0.536f, 0.000f }, { -1.948f, 0.000f, 4.704f } } },   // centralcityprops_zs_zs_759
};
const int LandingPadCount = (int)(sizeof(LandingPads) / sizeof(LandingPads[0]));
