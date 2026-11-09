#pragma once

// Thunder for VISUALS > Weather (see Thunder.cpp). The game's own thunder is
// only in the Release mission's level, so it's made here: a rumble (and a
// crack when close) worked out for each strike and played through Windows'
// own audio (XAudio2), apart from the game's sound.
namespace thunder
{
  // Any thread: one strike's thunder, heard after delay seconds (the light
  // first), from distance metres away (closer: louder, sharper, shorter),
  // the way dirX, dirZ (level, world) from the camera; volume 0 - 1
  void Play(float delay, float distance, float volume, float dirX, float dirZ);
  // Any thread: the tools' rain sound (a looping rain all round, muffled
  // when the camera is inside), volume 0 - 1 (0: off, it fades out)
  void SetRain(float volume);
  // Any thread: the wind's sound while it snows (a looping wind all round,
  // WindSynth.h): volume 0 - 1 (0: off, it fades out); strength 0 - 1 how
  // bright and harsh (a low pass opened up); inside 0 - 1 how much the camera
  // is indoors (muffled and quieter)
  void SetWind(float volume, float strength, float inside);
  // Whether it can be heard: Windows' audio could be opened (false until the
  // first Play has tried)
  bool Available();
  bool Tried();
}
