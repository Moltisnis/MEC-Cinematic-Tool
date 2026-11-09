#pragma once

// The game's own engine messages, heard the way the game's code hears them
// (2026-11-08; GridNode, github.com/CodeNameMeteor/Gridnode, does the same; read
// in the game's code here). A listener of the tools' own is registered with the
// client's and the server's message managers for one category, "PamSocialPlay"
// (Dashes are its "named challenges"). Nothing is hooked or patched, and
// nothing is sent: it only hears what the game sends anyway.
namespace game_messages
{
  enum DashKind { DashStarted = 1, DashFinished = 2, DashCancelled = 3 };

  // A Dash message as it came
  struct DashEvent
  {
    int Kind;                 // DashKind
    int Manager;              // 1 the client's message manager, 2 the server's
    unsigned long long Tick;  // GetTickCount64 when it came
    unsigned Fields[8];       // the message's own fields from + 0x48, raw (Started and Cancelled have 2, Finished 8)
  };

  // From the camera hook, on the game's thread: the client's manager kept
  // registered (again when the game makes a new one), and what came logged
  void ClientUpdate();
  // From the server's update, on its thread: the server's manager the same way
  void ServerUpdate();
  // The Dash messages since the last call, oldest first (at most max); any thread
  int TakeDashEvents(DashEvent* out, int max);
  // A listener is registered with at least one of the managers
  bool Listening();
  char const* KindName(int kind);
}
