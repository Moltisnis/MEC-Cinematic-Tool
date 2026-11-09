#pragma once
// The game's character skeletons: each bone's name and parent (-1: none), in the game's bone order, from its
// SkeletonAssets' BoneNames and Hierarchy (characters/skeletons/*, read 2026-11-08 offline; see
// Research_Pose_Editor_2026-11-08.md). Made by a script from the game's data: not to be edited by hand.
// skeleton_female_heels has skeleton_male's bones and parents (only bone 161 is named FacePosesAnimation);
// skeleton_female_balc and skeleton_female_heels_balc are skeleton_male_balc's.
namespace pose_skeletons
{
  struct Skeleton
  {
    const char* Asset;          // the game's SkeletonAsset
    const char* Label;          // for the menu
    int Bones;
    const char* const* Names;
    const short* Parents;
    int HeldRoot;               // the bone a pose holds, with every bone below it (Hips on people)
  };

  const char* const FaithNames[182] = {
    "Reference", "AITrajectory", "Trajectory", "TrajectoryEnd", "Hips", "Spine",
    "Spine1", "Spine2", "LeftShoulder", "LeftArm", "LeftArmRoll", "LeftElbowRoll",
    "LeftForeArm", "LeftForeArmRoll", "LeftForeArmRoll1", "LeftHand", "LeftHandRing0", "LeftHandRing1",
    "LeftHandRing2", "LeftHandRing3", "LeftHandRing4", "LeftHandPinky0", "LeftHandPinky1", "LeftHandPinky2",
    "LeftHandPinky3", "LeftHandPinky4", "LeftHandIndex0", "LeftHandIndex1", "LeftHandIndex2", "LeftHandIndex3",
    "LeftHandIndex4", "LeftHandThumb1", "LeftHandThumb2", "LeftHandThumb3", "LeftHandThumb4", "LeftHandMiddle0",
    "LeftHandMiddle1", "LeftHandMiddle2", "LeftHandMiddle3", "LeftHandMiddle4", "LeftHandThumbBase", "LeftHandIndexBase",
    "LeftHandPinkyBase", "LeftForeArmRoll2", "LeftRadiusTwistHolder", "LeftUlnaTwistHolder", "LeftRadiusTwist", "LeftUlnaTwist",
    "LeftArmRoll1", "LeftArmBend", "Neck", "Neck1", "Head", "HeadEnd",
    "Face", "Jaw", "LeftLowerLip", "LeftLowerInnerLip", "LowerLip", "LowerInnerLip",
    "RightLowerLip", "RightLowerInnerLip", "Tongue", "TongueTip", "Chin", "LeftLowCheek",
    "RightLowCheek", "LeftEye", "LeftIris", "RightEye", "RightIris", "LeftUpCheek",
    "LeftUpInnerCheek", "RightUpInnerCheek", "RightUpCheek", "LeftCheek", "RightCheek", "LeftMouth",
    "LeftInnerMouth", "LeftMiddleCrease", "LeftUpperLip", "LeftUpperInnerLip", "UpperLip", "UpperInnerLip",
    "RightUpperLip", "RightUpperInnerLip", "RightMouth", "RightInnerMouth", "RightMiddleCrease", "LeftUpEyelid",
    "RightUpEyelid", "LeftLowEyelid", "RightLowEyelid", "LeftInnerEyebrow", "LeftOuterEyebrow", "RightInnerEyebrow",
    "RightOuterEyebrow", "LeftNose", "RightNose", "LeftCrease", "RightCrease", "LeftLowMiddleEyebrow",
    "RightMiddleEyebrow", "LeftLowEyelidCrease", "LeftLowOuterEyebrow", "NoseTip", "RightLowOuterEyebrow", "LeftMiddleEyebrow",
    "RightLowMiddleEyebrow", "RightLowEyelidCrease", "LowNose", "Throat", "LeftArmPit", "LeftArmPitLow",
    "NeckCollar", "LeftDeltoidBulge", "RightShoulder", "RightArm", "RightArmRoll", "RightElbowRoll",
    "RightForeArm", "RightForeArmRoll", "RightForeArmRoll1", "RightHand", "RightHandRing0", "RightHandRing1",
    "RightHandRing2", "RightHandRing3", "RightHandRing4", "RightHandPinky0", "RightHandPinky1", "RightHandPinky2",
    "RightHandPinky3", "RightHandPinky4", "RightHandIndex0", "RightHandIndex1", "RightHandIndex2", "RightHandIndex3",
    "RightHandIndex4", "RightHandThumb1", "RightHandThumb2", "RightHandThumb3", "RightHandThumb4", "RightHandMiddle0",
    "RightHandMiddle1", "RightHandMiddle2", "RightHandMiddle3", "RightHandMiddle4", "RightHandThumbBase", "RightHandIndexBase",
    "RightHandPinkyBase", "Wep_Root", "RightForeArmRoll2", "RightRadiusTwistHolder", "RightUlnaTwistHolder", "RightRadiusTwist",
    "RightUlnaTwist", "RightArmRoll1", "RightArmBend", "RightArmPit", "RightArmPitLow", "RightDeltoidBulge",
    "LeftUpLeg", "LeftUpLegRoll", "LeftKneeRoll", "LeftHipsRoll", "LeftLeg", "LeftFoot",
    "LeftToeBase", "LeftToe", "RightUpLeg", "RightUpLegRoll", "RightKneeRoll", "RightHipsRoll",
    "RightLeg", "RightFoot", "RightToeBase", "RightToe", "CameraBase", "CameraJoint",
    "Connect", "ConnectEnd",
  };
  const short FaithParents[182] = {
    -1, 0, 1, 2, 1, 4, 5, 6, 7, 8, 9, 9, 9, 12, 12, 12, 15, 16, 17, 18, 19, 15, 21, 22,
    23, 24, 15, 26, 27, 28, 29, 15, 31, 32, 33, 15, 35, 36, 37, 38, 15, 15, 15, 12, 43, 43, 12, 12,
    9, 9, 7, 50, 51, 52, 52, 54, 55, 56, 55, 58, 55, 60, 55, 62, 55, 55, 55, 54, 67, 54, 69, 54,
    54, 54, 54, 54, 54, 54, 77, 77, 54, 80, 54, 82, 54, 84, 54, 86, 86, 54, 54, 54, 54, 54, 54, 54,
    54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 51, 7, 112, 7, 7, 7, 116, 117, 117,
    117, 120, 120, 120, 123, 124, 125, 126, 127, 123, 129, 130, 131, 132, 123, 134, 135, 136, 137, 123, 139, 140, 141, 123,
    143, 144, 145, 146, 123, 123, 123, 123, 120, 152, 152, 120, 120, 117, 117, 7, 159, 7, 4, 162, 162, 162, 162, 166,
    167, 168, 4, 170, 170, 170, 170, 174, 175, 176, 1, 178, 1, 180,
  };

  const char* const EnemyNames[163] = {
    "Reference", "AITrajectory", "Trajectory", "TrajectoryEnd", "Hips", "Spine",
    "Spine1", "Spine2", "LeftShoulder", "LeftArm", "LeftArmRoll", "LeftElbowRoll",
    "LeftForeArm", "LeftForeArmRoll", "LeftForeArmRoll1", "LeftHand", "LeftHandRing0", "LeftHandRing1",
    "LeftHandRing2", "LeftHandRing3", "LeftHandRing4", "LeftHandPinky0", "LeftHandPinky1", "LeftHandPinky2",
    "LeftHandPinky3", "LeftHandPinky4", "LeftHandIndex0", "LeftHandIndex1", "LeftHandIndex2", "LeftHandIndex3",
    "LeftHandIndex4", "LeftHandThumb1", "LeftHandThumb2", "LeftHandThumb3", "LeftHandThumb4", "LeftHandMiddle0",
    "LeftHandMiddle1", "LeftHandMiddle2", "LeftHandMiddle3", "LeftHandMiddle4", "LeftForeArmRoll2", "LeftArmRoll1",
    "Neck", "Neck1", "Head", "HeadEnd", "Face", "Jaw",
    "LeftLowerLip", "LeftLowerInnerLip", "LowerLip", "LowerInnerLip", "RightLowerLip", "RightLowerInnerLip",
    "Tongue", "TongueTip", "Chin", "LeftLowCheek", "RightLowCheek", "LeftEye",
    "LeftIris", "RightEye", "RightIris", "LeftUpCheek", "LeftUpInnerCheek", "RightUpInnerCheek",
    "RightUpCheek", "LeftCheek", "RightCheek", "LeftMouth", "LeftInnerMouth", "LeftMiddleCrease",
    "LeftUpperLip", "LeftUpperInnerLip", "UpperLip", "UpperInnerLip", "RightUpperLip", "RightUpperInnerLip",
    "RightMouth", "RightInnerMouth", "RightMiddleCrease", "LeftUpEyelid", "RightUpEyelid", "LeftLowEyelid",
    "RightLowEyelid", "LeftInnerEyebrow", "LeftOuterEyebrow", "RightInnerEyebrow", "RightOuterEyebrow", "LeftNose",
    "RightNose", "LeftCrease", "RightCrease", "LeftLowMiddleEyebrow", "RightMiddleEyebrow", "LeftLowEyelidCrease",
    "LeftLowOuterEyebrow", "NoseTip", "RightLowOuterEyebrow", "LeftMiddleEyebrow", "RightLowMiddleEyebrow", "RightLowEyelidCrease",
    "LowNose", "Throat", "LeftArmPit", "RightShoulder", "RightArm", "RightArmRoll",
    "RightElbowRoll", "RightForeArm", "RightForeArmRoll", "RightForeArmRoll1", "RightHand", "RightHandRing0",
    "RightHandRing1", "RightHandRing2", "RightHandRing3", "RightHandRing4", "RightHandPinky0", "RightHandPinky1",
    "RightHandPinky2", "RightHandPinky3", "RightHandPinky4", "RightHandIndex0", "RightHandIndex1", "RightHandIndex2",
    "RightHandIndex3", "RightHandIndex4", "RightHandThumb1", "RightHandThumb2", "RightHandThumb3", "RightHandThumb4",
    "RightHandMiddle0", "RightHandMiddle1", "RightHandMiddle2", "RightHandMiddle3", "RightHandMiddle4", "Wep_Root",
    "RightForeArmRoll2", "RightArmRoll1", "RightArmPit", "LeftUpLeg", "LeftUpLegRoll", "LeftKneeRoll",
    "LeftHipsRoll", "LeftLeg", "LeftFoot", "LeftToeBase", "LeftToe", "LeftAnkle",
    "RightUpLeg", "RightUpLegRoll", "RightKneeRoll", "RightHipsRoll", "RightLeg", "RightFoot",
    "RightToeBase", "RightToe", "RightAnkle", "CameraBase", "CameraJoint", "Connect",
    "ConnectEnd",
  };
  const short EnemyParents[163] = {
    -1, 0, 1, 2, 1, 4, 5, 6, 7, 8, 9, 9, 9, 12, 12, 12, 15, 16, 17, 18, 19, 15, 21, 22,
    23, 24, 15, 26, 27, 28, 29, 15, 31, 32, 33, 15, 35, 36, 37, 38, 12, 9, 7, 42, 43, 44, 44, 46,
    47, 48, 47, 50, 47, 52, 47, 54, 47, 47, 47, 46, 59, 46, 61, 46, 46, 46, 46, 46, 46, 46, 69, 69,
    46, 72, 46, 74, 46, 76, 46, 78, 78, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46,
    46, 46, 46, 46, 46, 46, 46, 43, 7, 7, 105, 106, 106, 106, 109, 109, 109, 112, 113, 114, 115, 116, 112, 118,
    119, 120, 121, 112, 123, 124, 125, 126, 112, 128, 129, 130, 112, 132, 133, 134, 135, 112, 109, 106, 7, 4, 141, 141,
    141, 141, 145, 146, 147, 145, 4, 150, 150, 150, 150, 154, 155, 156, 154, 1, 159, 1, 161,
  };

  const char* const MaleNames[162] = {
    "Reference", "AITrajectory", "Trajectory", "TrajectoryEnd", "Hips", "Spine",
    "Spine1", "Spine2", "LeftShoulder", "LeftArm", "LeftArmRoll", "LeftElbowRoll",
    "LeftForeArm", "LeftForeArmRoll", "LeftForeArmRoll1", "LeftHand", "LeftHandRing0", "LeftHandRing1",
    "LeftHandRing2", "LeftHandRing3", "LeftHandRing4", "LeftHandPinky0", "LeftHandPinky1", "LeftHandPinky2",
    "LeftHandPinky3", "LeftHandPinky4", "LeftHandIndex0", "LeftHandIndex1", "LeftHandIndex2", "LeftHandIndex3",
    "LeftHandIndex4", "LeftHandThumb1", "LeftHandThumb2", "LeftHandThumb3", "LeftHandThumb4", "LeftHandMiddle0",
    "LeftHandMiddle1", "LeftHandMiddle2", "LeftHandMiddle3", "LeftHandMiddle4", "LeftForeArmRoll2", "LeftArmRoll1",
    "Neck", "Neck1", "Head", "HeadEnd", "Face", "Jaw",
    "LeftLowerLip", "LeftLowerInnerLip", "LowerLip", "LowerInnerLip", "RightLowerLip", "RightLowerInnerLip",
    "Tongue", "TongueTip", "Chin", "LeftLowCheek", "RightLowCheek", "LeftEye",
    "LeftIris", "RightEye", "RightIris", "LeftUpCheek", "LeftUpInnerCheek", "RightUpInnerCheek",
    "RightUpCheek", "LeftCheek", "RightCheek", "LeftMouth", "LeftInnerMouth", "LeftMiddleCrease",
    "LeftUpperLip", "LeftUpperInnerLip", "UpperLip", "UpperInnerLip", "RightUpperLip", "RightUpperInnerLip",
    "RightMouth", "RightInnerMouth", "RightMiddleCrease", "LeftUpEyelid", "RightUpEyelid", "LeftLowEyelid",
    "RightLowEyelid", "LeftInnerEyebrow", "LeftOuterEyebrow", "RightInnerEyebrow", "RightOuterEyebrow", "LeftNose",
    "RightNose", "LeftCrease", "RightCrease", "LeftLowMiddleEyebrow", "RightMiddleEyebrow", "LeftLowEyelidCrease",
    "LeftLowOuterEyebrow", "NoseTip", "RightLowOuterEyebrow", "LeftMiddleEyebrow", "RightLowMiddleEyebrow", "RightLowEyelidCrease",
    "LowNose", "Throat", "LeftArmPit", "RightShoulder", "RightArm", "RightArmRoll",
    "RightElbowRoll", "RightForeArm", "RightForeArmRoll", "RightForeArmRoll1", "RightHand", "RightHandRing0",
    "RightHandRing1", "RightHandRing2", "RightHandRing3", "RightHandRing4", "RightHandPinky0", "RightHandPinky1",
    "RightHandPinky2", "RightHandPinky3", "RightHandPinky4", "RightHandIndex0", "RightHandIndex1", "RightHandIndex2",
    "RightHandIndex3", "RightHandIndex4", "RightHandThumb1", "RightHandThumb2", "RightHandThumb3", "RightHandThumb4",
    "RightHandMiddle0", "RightHandMiddle1", "RightHandMiddle2", "RightHandMiddle3", "RightHandMiddle4", "Wep_Root",
    "RightForeArmRoll2", "RightArmRoll1", "RightArmPit", "LeftUpLeg", "LeftUpLegRoll", "LeftKneeRoll",
    "LeftHipsRoll", "LeftLeg", "LeftFoot", "LeftToeBase", "LeftToe", "RightUpLeg",
    "RightUpLegRoll", "RightKneeRoll", "RightHipsRoll", "RightLeg", "RightFoot", "RightToeBase",
    "RightToe", "CameraBase", "CameraJoint", "Connect", "ConnectEnd", "BlendshapeDriver / FacePosesAnimation",
  };
  const short MaleParents[162] = {
    -1, 0, 1, 2, 1, 4, 5, 6, 7, 8, 9, 9, 9, 12, 12, 12, 15, 16, 17, 18, 19, 15, 21, 22,
    23, 24, 15, 26, 27, 28, 29, 15, 31, 32, 33, 15, 35, 36, 37, 38, 12, 9, 7, 42, 43, 44, 44, 46,
    47, 48, 47, 50, 47, 52, 47, 54, 47, 47, 47, 46, 59, 46, 61, 46, 46, 46, 46, 46, 46, 46, 69, 69,
    46, 72, 46, 74, 46, 76, 46, 78, 78, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46,
    46, 46, 46, 46, 46, 46, 46, 43, 7, 7, 105, 106, 106, 106, 109, 109, 109, 112, 113, 114, 115, 116, 112, 118,
    119, 120, 121, 112, 123, 124, 125, 126, 112, 128, 129, 130, 112, 132, 133, 134, 135, 112, 109, 106, 7, 4, 141, 141,
    141, 141, 145, 146, 147, 4, 149, 149, 149, 149, 153, 154, 155, 1, 157, 1, 159, 1,
  };

  const char* const FemaleNames[169] = {
    "Reference", "AITrajectory", "Trajectory", "TrajectoryEnd", "Hips", "Spine",
    "Spine1", "Spine2", "LeftShoulder", "LeftArm", "LeftArmRoll", "LeftElbowRoll",
    "LeftForeArm", "LeftForeArmRoll", "LeftForeArmRoll1", "LeftHand", "LeftHandRing0", "LeftHandRing1",
    "LeftHandRing2", "LeftHandRing3", "LeftHandRing4", "LeftHandPinky0", "LeftHandPinky1", "LeftHandPinky2",
    "LeftHandPinky3", "LeftHandPinky4", "LeftHandIndex0", "LeftHandIndex1", "LeftHandIndex2", "LeftHandIndex3",
    "LeftHandIndex4", "LeftHandThumb1", "LeftHandThumb2", "LeftHandThumb3", "LeftHandThumb4", "LeftHandMiddle0",
    "LeftHandMiddle1", "LeftHandMiddle2", "LeftHandMiddle3", "LeftHandMiddle4", "LeftForeArmRoll2", "LeftArmRoll1",
    "Neck", "Neck1", "Head", "HeadEnd", "Face", "Jaw",
    "LeftLowerLip", "LeftLowerInnerLip", "LowerLip", "LowerInnerLip", "RightLowerLip", "RightLowerInnerLip",
    "Tongue", "TongueTip", "Chin", "LeftLowCheek", "RightLowCheek", "LeftEye",
    "LeftIris", "RightEye", "RightIris", "LeftUpCheek", "LeftUpInnerCheek", "RightUpInnerCheek",
    "RightUpCheek", "LeftCheek", "RightCheek", "LeftMouth", "LeftInnerMouth", "LeftMiddleCrease",
    "LeftUpperLip", "LeftUpperInnerLip", "UpperLip", "UpperInnerLip", "RightUpperLip", "RightUpperInnerLip",
    "RightMouth", "RightInnerMouth", "RightMiddleCrease", "LeftUpEyelid", "RightUpEyelid", "LeftLowEyelid",
    "RightLowEyelid", "LeftInnerEyebrow", "LeftOuterEyebrow", "RightInnerEyebrow", "RightOuterEyebrow", "LeftNose",
    "RightNose", "LeftCrease", "RightCrease", "LeftLowMiddleEyebrow", "RightMiddleEyebrow", "LeftLowEyelidCrease",
    "LeftLowOuterEyebrow", "NoseTip", "RightLowOuterEyebrow", "LeftMiddleEyebrow", "RightLowMiddleEyebrow", "RightLowEyelidCrease",
    "LowNose", "ponyTail_ROOT", "ponyTail_01", "ponyTail_02", "ponyTail_03", "ponyTail_04",
    "ponyTail_05", "Throat", "LeftArmPit", "RightShoulder", "RightArm", "RightArmRoll",
    "RightElbowRoll", "RightForeArm", "RightForeArmRoll", "RightForeArmRoll1", "RightHand", "RightHandRing0",
    "RightHandRing1", "RightHandRing2", "RightHandRing3", "RightHandRing4", "RightHandPinky0", "RightHandPinky1",
    "RightHandPinky2", "RightHandPinky3", "RightHandPinky4", "RightHandIndex0", "RightHandIndex1", "RightHandIndex2",
    "RightHandIndex3", "RightHandIndex4", "RightHandThumb1", "RightHandThumb2", "RightHandThumb3", "RightHandThumb4",
    "RightHandMiddle0", "RightHandMiddle1", "RightHandMiddle2", "RightHandMiddle3", "RightHandMiddle4", "Wep_Root",
    "RightForeArmRoll2", "RightArmRoll1", "RightArmPit", "LeftUpLeg", "LeftUpLegRoll", "LeftKneeRoll",
    "LeftHipsRoll", "LeftLeg", "LeftFoot", "LeftToeBase", "LeftToe", "LeftAnkle",
    "RightUpLeg", "RightUpLegRoll", "RightKneeRoll", "RightHipsRoll", "RightLeg", "RightFoot",
    "RightToeBase", "RightToe", "RightAnkle", "CameraBase", "CameraJoint", "Connect",
    "ConnectEnd",
  };
  const short FemaleParents[169] = {
    -1, 0, 1, 2, 1, 4, 5, 6, 7, 8, 9, 9, 9, 12, 12, 12, 15, 16, 17, 18, 19, 15, 21, 22,
    23, 24, 15, 26, 27, 28, 29, 15, 31, 32, 33, 15, 35, 36, 37, 38, 12, 9, 7, 42, 43, 44, 44, 46,
    47, 48, 47, 50, 47, 52, 47, 54, 47, 47, 47, 46, 59, 46, 61, 46, 46, 46, 46, 46, 46, 46, 69, 69,
    46, 72, 46, 74, 46, 76, 46, 78, 78, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46,
    46, 46, 46, 46, 46, 46, 46, 44, 103, 104, 105, 106, 107, 43, 7, 7, 111, 112, 112, 112, 115, 115, 115, 118,
    119, 120, 121, 122, 118, 124, 125, 126, 127, 118, 129, 130, 131, 132, 118, 134, 135, 136, 118, 138, 139, 140, 141, 118,
    115, 112, 7, 4, 147, 147, 147, 147, 151, 152, 153, 151, 4, 156, 156, 156, 156, 160, 161, 162, 160, 1, 165, 1,
    167,
  };

  const char* const EnemyLowNames[83] = {
    "Reference", "AITrajectory", "Trajectory", "TrajectoryEnd", "Hips", "Spine",
    "Spine1", "Spine2", "LeftShoulder", "LeftArm", "LeftArmRoll", "LeftElbowRoll",
    "LeftForeArm", "LeftForeArmRoll", "LeftForeArmRoll1", "LeftHand", "LeftHandRing0", "LeftHandRing1",
    "LeftHandRing2", "LeftHandRing3", "LeftHandPinky0", "LeftHandPinky1", "LeftHandPinky2", "LeftHandPinky3",
    "LeftHandIndex0", "LeftHandIndex1", "LeftHandIndex2", "LeftHandIndex3", "LeftHandThumb1", "LeftHandThumb2",
    "LeftHandThumb3", "LeftHandMiddle0", "LeftHandMiddle1", "LeftHandMiddle2", "LeftHandMiddle3", "LeftForeArmRoll2",
    "Neck", "Neck1", "Head", "HeadEnd", "RightShoulder", "RightArm",
    "RightArmRoll", "RightElbowRoll", "RightForeArm", "RightForeArmRoll", "RightForeArmRoll1", "RightHand",
    "RightHandRing0", "RightHandRing1", "RightHandRing2", "RightHandRing3", "RightHandPinky0", "RightHandPinky1",
    "RightHandPinky2", "RightHandPinky3", "RightHandIndex0", "RightHandIndex1", "RightHandIndex2", "RightHandIndex3",
    "RightHandThumb1", "RightHandThumb2", "RightHandThumb3", "RightHandMiddle0", "RightHandMiddle1", "RightHandMiddle2",
    "RightHandMiddle3", "Wep_Root", "RightForeArmRoll2", "LeftUpLeg", "LeftUpLegRoll", "LeftKneeRoll",
    "LeftLeg", "LeftFoot", "LeftToeBase", "RightUpLeg", "RightUpLegRoll", "RightKneeRoll",
    "RightLeg", "RightFoot", "RightToeBase", "Connect", "ConnectEnd",
  };
  const short EnemyLowParents[83] = {
    -1, 0, 1, 2, 1, 4, 5, 6, 7, 8, 9, 9, 9, 12, 12, 12, 15, 16, 17, 18, 15, 20, 21, 22,
    15, 24, 25, 26, 15, 28, 29, 15, 31, 32, 33, 12, 7, 36, 37, 38, 7, 40, 41, 41, 41, 44, 44, 44,
    47, 48, 49, 50, 47, 52, 53, 54, 47, 56, 57, 58, 47, 60, 61, 47, 63, 64, 65, 47, 44, 4, 69, 69,
    69, 72, 73, 4, 75, 75, 75, 78, 79, 1, 81,
  };

  const char* const PersonLowNames[82] = {
    "Reference", "AITrajectory", "Trajectory", "TrajectoryEnd", "Hips", "Spine",
    "Spine1", "Spine2", "LeftShoulder", "LeftArm", "LeftArmRoll", "LeftElbowRoll",
    "LeftForeArm", "LeftForeArmRoll", "LeftForeArmRoll1", "LeftHand", "LeftHandRing0", "LeftHandRing1",
    "LeftHandRing2", "LeftHandRing3", "LeftHandPinky0", "LeftHandPinky1", "LeftHandPinky2", "LeftHandPinky3",
    "LeftHandIndex0", "LeftHandIndex1", "LeftHandIndex2", "LeftHandIndex3", "LeftHandThumb1", "LeftHandThumb2",
    "LeftHandThumb3", "LeftHandMiddle0", "LeftHandMiddle1", "LeftHandMiddle2", "LeftHandMiddle3", "LeftForeArmRoll2",
    "Neck", "Neck1", "Head", "HeadEnd", "RightShoulder", "RightArm",
    "RightArmRoll", "RightElbowRoll", "RightForeArm", "RightForeArmRoll", "RightForeArmRoll1", "RightHand",
    "RightHandRing0", "RightHandRing1", "RightHandRing2", "RightHandRing3", "RightHandPinky0", "RightHandPinky1",
    "RightHandPinky2", "RightHandPinky3", "RightHandIndex0", "RightHandIndex1", "RightHandIndex2", "RightHandIndex3",
    "RightHandThumb1", "RightHandThumb2", "RightHandThumb3", "RightHandMiddle0", "RightHandMiddle1", "RightHandMiddle2",
    "RightHandMiddle3", "RightForeArmRoll2", "LeftUpLeg", "LeftUpLegRoll", "LeftKneeRoll", "LeftLeg",
    "LeftFoot", "LeftToeBase", "RightUpLeg", "RightUpLegRoll", "RightKneeRoll", "RightLeg",
    "RightFoot", "RightToeBase", "Connect", "ConnectEnd",
  };
  const short PersonLowParents[82] = {
    -1, 0, 1, 2, 1, 4, 5, 6, 7, 8, 9, 9, 9, 12, 12, 12, 15, 16, 17, 18, 15, 20, 21, 22,
    15, 24, 25, 26, 15, 28, 29, 15, 31, 32, 33, 12, 7, 36, 37, 38, 7, 40, 41, 41, 41, 44, 44, 44,
    47, 48, 49, 50, 47, 52, 53, 54, 47, 56, 57, 58, 47, 60, 61, 47, 63, 64, 65, 44, 4, 68, 68, 68,
    71, 72, 4, 74, 74, 74, 77, 78, 1, 80,
  };

  const char* const CrowdNames[28] = {
    "Reference", "AITrajectory", "Trajectory", "TrajectoryEnd", "Hips", "Spine",
    "Spine1", "Spine2", "LeftShoulder", "LeftArm", "LeftForeArm", "LeftHand",
    "Neck", "Neck1", "RightShoulder", "RightArm", "RightForeArm", "RightHand",
    "LeftUpLeg", "LeftLeg", "LeftFoot", "RightUpLeg", "RightLeg", "RightFoot",
    "CameraBase", "CameraJoint", "Connect", "ConnectEnd",
  };
  const short CrowdParents[28] = {
    -1, 0, 1, 2, 1, 4, 5, 6, 7, 8, 9, 10, 7, 12, 7, 14, 15, 16, 4, 18, 19, 4, 21, 22,
    1, 24, 1, 26,
  };

  const char* const DroneNames[49] = {
    "Reference", "AITrajectory", "Drone_Base_jt", "SensorMount_01_jt", "SensorHead_jt", "Arm1_LegJoint_01_jt",
    "Arm1_LegJoint_02_jt", "Arm1_Foot_jt", "Arm1_PneumaticEnd_jt", "Arm1_Pneumatic_jt", "Arm1_JointBody_jt", "Arm1_Arm_jt",
    "Arm1_RotorConnection_jt", "Arm1_Rotor1_buffGrp", "Arm1_Rotor1_jt", "Arm1_Rotor_buffGrp", "Arm1_Rotor_jt", "Arm1_ChasisRot_jt",
    "Arm2_LegJoint_01_jt", "Arm2_LegJoint_02_jt", "Arm2_Foot_jt", "Arm2_PneumaticEnd_jt", "Arm2_Pneumatic_jt", "Arm2_JointBody_jt",
    "Arm2_Arm_jt", "Arm2_RotorConnection_jt", "Arm2_Rotor1_buffGrp", "Arm2_Rotor1_jt", "Arm2_Rotor_buffGrp", "Arm2_Rotor_jt",
    "Arm2_ChasisRot_jt", "Arm3_LegJoint_01_jt", "Arm3_LegJoint_02_jt", "Arm3_Foot_jt", "Arm3_PneumaticEnd_jt", "Arm3_Pneumatic_jt",
    "Arm3_JointBody_jt", "Arm3_Arm_jt", "Arm3_RotorConnection_jt", "Arm3_Rotor1_buffGrp", "Arm3_Rotor1_jt", "Arm3_Rotor_buffGrp",
    "Arm3_Rotor_jt", "Arm3_ChasisRot_jt", "SensorHead_aim_jt", "Connect", "ConnectEnd", "Trajectory",
    "TrajectoryEnd",
  };
  const short DroneParents[49] = {
    -1, 0, 1, 2, 3, 2, 5, 6, 6, 5, 2, 2, 11, 12, 13, 12, 15, 2, 2, 18, 19, 19, 18, 2,
    2, 24, 25, 26, 25, 28, 2, 2, 31, 32, 32, 31, 2, 2, 37, 38, 39, 38, 41, 2, 2, 1, 45, 1,
    47,
  };

  const char* const BearNames[47] = {
    "Reference", "AITrajectory", "Trajectory", "TrajectoryEnd", "Connect", "ConnectEnd",
    "Body_Main", "BackFrame_base", "BackFrame_hinge", "BackFrame_end", "Back_Wheel", "Body_01",
    "Body_02", "Neck_01", "Neck_02", "Head", "optic01", "optic02",
    "Right_Hear", "antenna_01", "antenna_02", "antenna_03", "antenna_04", "Left_Hear",
    "Right_Upperarm", "Right_Forearm", "Right_Hand_01", "Right_Hand_02", "Right_Hand_03", "Right_finge02_01",
    "Right_finge02_02", "Right_finge01_01", "Right_finge01_02", "chestGuard_01", "chestGuard_02", "chestGuard_03",
    "chestGuard_04", "Left_Upperarm", "Left_Forearm", "Left_Hand_01", "Left_Hand_02", "Left_Hand_03",
    "Left_finge02_01", "Left_finge02_02", "Left_finge01_01", "Left_finge01_02", "Front_Wheel",
  };
  const short BearParents[47] = {
    -1, 0, 1, 2, 1, 4, 1, 6, 7, 8, 9, 7, 11, 12, 13, 14, 15, 16, 15, 15, 19, 20, 21, 15,
    12, 24, 25, 26, 27, 28, 29, 28, 31, 12, 33, 34, 35, 12, 37, 38, 39, 40, 41, 42, 41, 44, 6,
  };

  const Skeleton Skeletons[] = {
    { "characters/skeletons/skeleton_faith", "Faith", 182, FaithNames, FaithParents, 4 },
    { "characters/skeletons/skeleton_enemy", "KrugerSec", 163, EnemyNames, EnemyParents, 4 },
    { "characters/skeletons/skeleton_male", "man (or woman in heels)", 162, MaleNames, MaleParents, 4 },
    { "characters/skeletons/skeleton_female", "woman", 169, FemaleNames, FemaleParents, 4 },
    { "characters/skeletons/skeleton_enemy_balc", "KrugerSec (low detail)", 83, EnemyLowNames, EnemyLowParents, 4 },
    { "characters/skeletons/skeleton_male_balc", "person (low detail)", 82, PersonLowNames, PersonLowParents, 4 },
    { "characters/skeletons/skeleton_pedestrian", "crowd person", 28, CrowdNames, CrowdParents, 4 },
    { "characters/skeletons/skeleton_drone", "drone", 49, DroneNames, DroneParents, 2 },
    { "characters/skeletons/skeleton_kumabear01", "bear mascot", 47, BearNames, BearParents, 6 },
  };
}
