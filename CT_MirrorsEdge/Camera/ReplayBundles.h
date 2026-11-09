#pragma once
#include <string>

namespace replay_store { class Out; class In; }

// The recorded mission's bundles loaded again while the replay plays
// (2026-10-27; the user: "Load the bundle again ... lets do this for the replay
// implement it"; Research_Mission_Loading_2026-10-27.md). While it records, the
// sub-level bundles the game's bundle loader has in are noted once a second
// with the request the game made for each (name, kind, heap, parent) and when
// they were in. While it plays, the bundles that were in at the time played
// (and a little ahead) are asked for again through the game's own loader, the
// same request, and let go when the playback is past them, when it stops, at
// a loading screen, or when the option is turned off. The game's loader does
// the loading on its own thread, as for its own sub-levels; the bundles' meshes,
// textures and sound data are the game's own again. Their entities (the
// mission's world) aren't made: the replay draws and plays what it recorded.
// See the .cpp.
namespace replay_bundles
{
  // The game's loader code is what the tools know (its hooks are in)
  bool Available();
  // Game thread, every frame: what the replay does (0 nothing, 1 records, 2
  // plays), its clock (the recording's time), the option, a loading screen up
  void Update(int mode, double clock, bool enabled, bool loading);
  // A new recording or the recording forgotten: what was noted goes
  void Forget();
  // While it plays: bundles wanted now that aren't in yet (the replay's start
  // waits on them, at most WaitSeconds)
  bool Waiting();
  // For the menu
  void Status(int& recorded, int& held, int& in);

  // Saved recordings (ReplayStore.h): the bundles noted written, and read
  // back in place of these (after Forget); the mission's name for the save's
  // folder, from them ("" when none is a mission's own)
  void Save(replay_store::Out& out);
  bool Load(replay_store::In& in);
  std::string Where();
}
