#pragma once

// The game's own pause and loading screen, read from its UI system (any
// thread; see GameState.cpp)
namespace gamestate
{
  // Paused by the game's UI: its pause menu or anything else that pauses the
  // game the same way
  bool Paused();
  // The game's loading screen is up
  bool Loading();
}
