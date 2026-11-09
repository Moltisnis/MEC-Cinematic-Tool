#pragma once
#include <cstddef>
#include <utility>
#include <vector>

namespace replay_store { class Out; class In; }

// The replay's own sound track (2026-10-27): the game's whole sound, as it was
// heard while recording, played back in place of the game's while the
// recording plays. A mission's voice lines, dialog and music go with the
// mission's sound banks once it's over, so the game can't play them again;
// the track has them as they were. See the .cpp.
namespace replay_audio
{
  // Once at startup: gives the game its sound back if a playback was cut short
  // (the game closed or crashed while it played)
  void Init();

  // Recording: Windows captures this process's own sound (process loopback)
  // from now on. NoteClock: the replay's clock at each of its frames, so the
  // track is played on the same clock.
  bool StartRecording();
  void NoteClock(double clock);
  void StopRecording();
  bool IsRecording();

  // The track recorded: whether there is one, how long, and forgetting it
  bool HasTrack();
  double TrackSeconds();
  void Forget();

  // Playing: the game's own sound muted (each of its sessions in Windows'
  // mixer), the track played at the replay's time (SetPlayTime at each frame:
  // its time, paused, its speed; speed 0 when it isn't played in real time,
  // and then nothing is heard). StopPlaying gives the game its sound back.
  bool StartPlaying();
  // gate: heard now (2026-11-01: the track is heard only while a line the
  // replay can't say live covers the time; the game's own sound isn't muted)
  void SetPlayTime(double time, bool paused, double speed, bool gate = true);
  // Once it stops recording: everything but the spans (the replay's clock,
  // widened by before / after seconds) silenced: the dialog only
  void KeepOnly(std::vector<std::pair<double, double>> const& spans, double before, double after);
  void StopPlaying();
  bool IsPlaying();

  // One line for the menu
  const char* Status(char* buffer, size_t size);

  // The game process is ending (DllMain's process detach, its other threads
  // already stopped by Windows): the recording and playing threads are let go,
  // so their std::thread objects don't end the process with std::terminate
  void OnProcessExit();

  // Saved recordings (ReplayStore.h): the track written (each channel's
  // samples as their change from the one before), and read back in place of
  // the one there; false if there's none to write, or it couldn't be read
  bool Save(replay_store::Out& out);
  bool Load(replay_store::In& in);
}
