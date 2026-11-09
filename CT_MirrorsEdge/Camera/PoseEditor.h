#pragma once
#include <cstdint>

// GAME > Pose: Faith, KrugerSec and other people posed bone by bone in the
// game (Research_Pose_Editor_2026-11-08.md).
//
// How: the game's own pose override, the one its Dash ghosts and the tools'
// replay use. Each pose slot of a pose manager has one override pointer (page
// + 0x30); the Ant pose job (0x142FCC7C0 -> 0x142FCF620) blends the bones it
// lists toward it by its weight, after the character's animation and IK, and
// the game then builds the world pose and the skinning matrices from that
// (antFbBuildRenderTransforms, 0x142FCFC30). A held pose lists the held root
// (Hips on people) and every bone below it, weight 1: bones 0 - 3 (where the
// character is: the client entity follows its pose's root), the camera bones
// and Connect stay the game's. Faith's box pose (soldier + 0x340, her culling
// box) gets the same bones.
//
// Only bodies the game animates take an override (its pose job doesn't run
// for the others: taken down, or far away). Whether the game takes it while
// time is frozen wasn't known when this was built: the log says, every 10 s
// while a pose is held ("Pose: ... the game took the pose in N of M frames").
//
// Steps aside (the override taken out, the pose kept) while a replay records
// or plays; never writes over another override (the replay's, a Dash
// ghost's).
namespace pose_editor
{
  // Every frame from the camera hook on the main view, on the game's thread,
  // after the replay's update: Faith's soldier (0: none), whether a replay
  // records or plays, whether time is frozen (for the log)
  void Update(__int64 soldier, bool replayBusy, bool timeFrozen);

  // From the camera hook for every render view, after the free camera is
  // written: the main view, for the bones' dots (only while the menu is open)
  void OnViewWritten();

  // GAME > Pose (inside the GAME tab, at Present)
  void DrawUI();

  // The held pose's bones and turning rings over the picture, with the menu
  // open (before the lights' dots: a click on a bone is the pose's)
  void DrawOverlay();
}
