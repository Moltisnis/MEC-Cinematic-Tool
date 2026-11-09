#pragma once

// The free camera stopping at walls: its move each frame is swept as a ball
// through the game's own physics and stopped where the ball touches
// something, the rest of the move sliding along it. The sweep is the game's
// own (0x143A24890, the one its raycast entities use: a sphere cast with
// collision filter 9, a ray when the radius is about 0), on the game's client
// physics world ([[[[[0x142401CA0] + 0x50] + 0xF0]] + 0x18], as 0x14393BED4
// and 0x1434EE940 get it). Game thread only (the camera hook).
namespace collision
{
  // The game's code as it was read, and a physics world there now
  bool Available();
  // to: where the camera would go from from this frame; moved back to where
  // the ball of radius can get to, sliding along what's in the way. normal:
  // the last surface touched (when it returns true)
  bool Move(float const* from, float* to, float radius, float* normal);
  // One sweep of a ball from from to to: true when it touches something on
  // the way, fraction (0 - 1) of the way there, the surface's normal
  bool Sweep(float const* from, float const* to, float radius, float* fraction, float* normal);
}
