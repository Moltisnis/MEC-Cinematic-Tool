#pragma once

// Dogen's door (2026-11-03): the one-way door into Dogen's kitchen in Anchor (186, 67, 305) swings open as Faith comes
// to it and shuts again once she's gone, so she can walk in. No menu entry (the user: an easter egg). The same for the
// closed-off door on a roof's catwalk in The View (961, 105, 170), onto the roof behind it. Research_Dogen_
// Apartment_2026-11-03.md has the data and the decompile.
namespace dogen
{
  // Each frame on the game's thread (the camera hook): Faith's feet, or none
  void Update(bool hasFaith, float const* feet);
}
