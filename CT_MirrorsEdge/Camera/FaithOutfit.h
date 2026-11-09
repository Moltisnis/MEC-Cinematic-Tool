#pragma once
#include <cstddef>

// Faith's outfit: free roam's or the Release mission's (GAME > Faith).
//
// The game dresses Faith with the main city's spawner, gameplay/spawners/
// pf_spspawn_01 (in sp_maincity layer0_default). It has seven
// CustomizeCharacterEntityData (server realm), one per outfit, each linked
// ("Characters") to the spawner's CharacterSpawnReference; a progression flag
// picks which one gets the event Apply (0x0C487271): 1 Civilian
// (C_Faith_Prologue), 2 CivilianNoGridLink, 3 Runner (C_Faith_NoTattoo), 4-6
// the MagRope stages, 7 RunnerWithTattoo (C_Faith_WithTattoo, set at the end
// of The Shard). The Release mission sets the flag to 1 at its start and to 2
// further on (layer6_release_meeticarus #197 / #131).
//
// The tools send the same Apply to the outfit's own entity
// (ServerCustomizeCharacterEntity, vtable 0x141B524E0, its handler slot 4
// 0x14339B1C0), from the start of the server's update (0x1433446F0), on the
// thread the game runs it: the handler finds Faith through the spawner and
// calls her ServerCharacterEntity's customize (slot +0x340), as the level
// does. The progression flag and the save aren't touched. The entities are
// noted as the game makes them (their constructor 0x143393EB0), so the tools
// must be loaded before the level (they are, with the loader).
namespace faith_outfit
{
  enum Choice
  {
    Choice_Game,               // as the game dresses her (free roam: its flag)
    Choice_Release,            // C_Faith_Prologue: civilian clothes, the gridlink
    Choice_ReleaseNoGridlink,  // C_Faith_Prologue_NoGridlink
    Choice_Count
  };

  // Checks the game's code and hooks it. Once, right after util::hooks::Init.
  void Init();
  // False if the code wasn't what the tools know (nothing is hooked then)
  bool IsAvailable();

  // From the menu: the next server update dresses her (and again after each
  // time the game dresses her itself: a respawn, a level load, a mission)
  void SetChoice(int choice);
  int GetChoice();
  const char* ChoiceName(int choice);

  // One line for the menu: what she wears now, what the game would have her in
  const char* GetStatus(char* buffer, size_t size);
  // What the game last dressed her in (0 - 6 in the spawner's flag order: 0
  // and 1 the Release mission's; -1 not seen yet), for the replay
  int GameOutfit();
  bool IsReleaseOutfit(int outfit);
  // What she wears as far as the tools know (the game's or the tools'; -1 not
  // known: before the game dressed her after a level load), for the replay's
  // recording. The No arm tattoo tick's twin is told as the tattoo outfit
  // (6), so a recording shows the tattoo or not as the tick is at playback.
  int WornOutfit();
  // The replay's: while a recording plays, she's dressed in what she wore
  // then (0 - 6 as above), over the menu's choice; -1 lets go (she's dressed
  // back as the menu or the game has her)
  void SetReplayOutfit(int outfit);
  const char* OutfitName(int outfit);

  // GAME > Faith > No arm tattoo (2026-11-08). The tattoo is a texture
  // switch, read in the game's data: only the spawner's C_Faith_WithTattoo
  // (RunnerWithTattoo, flag 7, the free roam outfit after the story) has it.
  // Its arms are Unlock_Faith_Arms / Arms1p: faith_arms_mesh with its own
  // materials, whose t_arms_dao (colour), t_arms_nrs and t_arms_ex (SSS) hold
  // the ink. Its twin C_Faith_NoTat_MagRope3 (RunnerWithPullDown, flag 6)
  // lists the same 13 other parts and Unlock_Faith_Arms_NoTattoo /
  // Arms1p_NoTattoo: the same mesh with the variation Faith_Arms_NoTattoo
  // (NameHash 4240144052), t_arms_notattoo_* in the main city's
  // MeshVariationDb. All of it is in the main city's level bundle. With the
  // tick, wherever she'd wear the tattoo outfit (the game's, the replay's),
  // the twin's own Apply dresses her instead, the way the menu's outfits are
  // sent (Dress); unticked, the tattoo outfit's own Apply puts it back. The
  // other outfits have no tattoo and are left alone. Saved ([Misc]
  // NoArmTattoo).
  void SetNoTattoo(bool on);
  bool GetNoTattoo();

  // GAME > Faith: her accessories (2026-11-08). The outfits' separate worn
  // items [data]: the MagRope (the gauntlet on her right forearm and the
  // module on it, 3rd and 1st person), her cross-body sling bag (Toolbag),
  // and the gridlink (the band on her left wrist, only C_Faith_Prologue has
  // it). Her half-finger glove, the sleeve at her right elbow and her shoes
  // are parts of other meshes, with no skin under them: they can't be taken
  // off. The game gives a character an outfit's parts in applyCustomization
  // (0x14336CF00): with a tick on, the tools hand it, for that call, a copy
  // of the outfit whose part list leaves those parts out or adds the
  // gridlink's two from C_Faith_Prologue. The server sends her the list and
  // the client builds her parts from it; nothing keeps the copy. Only for her
  // (a character with a player) and the spawner's seven outfits, plus The
  // Shard train ride's own C_Faith_NoTat_MagRope3_NPC_InGHair (parts left
  // out only); her very first outfit (the one the game keeps as her original)
  // is given as it is and changed at the next server update (or the game's
  // next outfit, whichever comes first). A tick change
  // dresses her again in the spawner's outfit she wears, only if that changes
  // her parts (a rebuild restarts her hair) and only if her last outfit is
  // that one (a mission's own isn't replaced: the ticks wait for the game's
  // next outfit). Saved ([Misc] NoMagRope, NoBag, AddGridlink).
  enum Part
  {
    Part_NoMagRope,
    Part_NoBag,
    Part_AddGridlink,
    Part_Count
  };
  bool ArePartsAvailable();
  void SetPart(int part, bool on);
  bool GetPart(int part);
  const char* PartLabel(int part);    // the menu's
  const char* PartSetting(int part);  // its config key ([Misc])
}
