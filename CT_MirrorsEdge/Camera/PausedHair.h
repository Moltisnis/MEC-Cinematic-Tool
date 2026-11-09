#pragma once
#include <cstdint>

// GAME > Pose > "Her hair moves while paused": Faith's cloths (her hair) keep
// simulating while time is frozen (Research_Pose_Editor_2026-11-08.md,
// section 16).
//
// Why it stops: the game steps every cloth from its physics step
// (EAClothSimUpdater, vtable 0x141AB68D8, slot 3 = 0x142FA2720 -> the cloth
// manager's 0x142FA3A10), and Freeze time sets the time scale to 0, so there
// are no physics steps. Each frame the game only interpolates the last two
// steps (EAClothUpdater, 0x142FA1960), so a head posed while frozen left the
// hair where it was.
//
// What the tools do while frozen: for each cloth of hers that is in the game's
// cloth list, at full detail (LOD 0), with none of the game's cloth jobs
// running, the game's own two per-cloth steps, on the game's thread: the
// pre-update (0x142FA2760: her root's place) and the sim job's body
// (0x142FA07A0, handed a job record that lists her cloths only: anchors from
// her pose as it's drawn, the world's collision, the EA step, the result to
// the drawn mesh). The EA step takes the time since its last step itself
// (0x143B7C5A0 keeps it at + 0xE0), so the tools step at 60 Hz of real time.
namespace paused_hair
{
  // Every frame on the game's thread (from pose_editor::Update): Faith's
  // soldier (0: none), whether time is frozen, whether the tick is on
  void Update(__int64 soldier, bool timeFrozen, bool wanted);

  // For the menu: what it did last ("" when nothing yet)
  void Status(char* out, size_t size);
}
