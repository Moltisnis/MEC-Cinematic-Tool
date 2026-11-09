#pragma once
#include <d3d11.h>
#include <string>
#include <vector>

// What the replay's recording had loaded and drawn, and what's missing when
// it plays (2026-10-27; the user: "log every mesh / model that loads during
// the mission loading and check what misses on the replay", "and texture").
// While it records: every mesh that comes into or goes out of the game's
// mesh table (with its time, and whether a loading screen was up), every
// texture the game makes, and which meshes its G-buffer pass draws with which
// textures. While it plays: the same draws, the game's and the replay's own.
// When playback stops: the meshes drawn while recording at the times played
// that weren't drawn then, and those drawn with other textures. All of it in
// Cinematic Tools\ReplayAssets.log, the totals in CT.log. See the .cpp.
namespace replay_assetlog
{
  // Game thread, every frame: what the replay does (0 nothing, 1 records,
  // 2 plays), its clock (the recording's time), a loading screen up
  void Update(int mode, double clock, bool loading);
  // Render thread, each Present: the device's CreateTexture2D hooked once;
  // the frame's draws handed over
  void Present(ID3D11Device* pDevice);
  // Render thread, from the replay's draw hooks: a draw of the game's (only
  // those in its main G-buffer pass, in a sampled frame, are looked at); and
  // one of the replay's own there (ReplayGameDraws)
  void OnDraw(ID3D11DeviceContext* pContext, UINT indexCount, UINT startIndex);
  void OnReplayDraw(ID3D11Buffer* vb, UINT startIndex, UINT indexCount, ID3D11ShaderResourceView* const* views, int count);
  // The missions running while it recorded (their names), for a saved
  // recording; a loaded one's put back
  std::vector<std::string> RecordedMissions();
  void SetRecordedMissions(std::vector<std::string> const& names);
  // The REPLAY tick "The mission's world from the game (test)": while a
  // recording of a mission plays, the game is asked to stream that mission's
  // world in (see the .cpp's mission's world)
  bool MissionWorld();
  void SetMissionWorld(bool on);
  // Game thread, each frame: where the real Faith (her soldier) is, and how
  // far she is from the recording's own path (FLT_MAX: no path); for the
  // mission's world's guards
  void SetFaithPlace(bool known, float const* place, float routeClearance);
  // Server update thread (FaithOutfit's Server::update hook), before each
  // server frame
  void ServerUpdate();
}
