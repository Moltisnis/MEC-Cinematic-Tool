#include "FaithOutfit.h"
#include "ReplayAssetLog.h"
#include "../Main.h"
#include "../Util/Util.h"
#include "../Util/GameMessages.h"
#include <Windows.h>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <vector>

namespace
{
  const __int64 CustomizeCtorRva = 0x3393EB0;      // ServerCustomizeCharacterEntity's constructor (its factory 0x1433939E0 the one caller)
  const __int64 CustomizeEventRva = 0x339B1C0;     // its event handler, vtable slot 4
  const __int64 CustomizeVTableRva = 0x1B524E0;
  const __int64 CustomizeTypeRva = 0x339C7F0;      // slot 0, its getType
  const __int64 EntityDeleteRva = 0x3118AB0;       // slot 7, the entities' deleting destructor (resets the vtable)
  const __int64 ServerUpdateRva = 0x33446F0;       // Server::update(server, info): the server job 0x142A30540 and the main loop's single-threaded call
  const int EntityDataOffset = 0x28;               // Entity: + 0x20 its bus, + 0x28 its data (base constructor 0x143160100)
  const int PeerFlagsOffset = 0x10;                // DataBusPeer.Flags
  const int CustomizeDataOffset = 0x20;            // CustomizeCharacterEntityData.CustomizeCharacterData
  const int AssetNameOffset = 0x10;                // Asset.Name
  const unsigned int ApplyEvent = 0x0C487271;      // "Apply"
  const size_t MaxNoted = 64;

  struct Outfit
  {
    unsigned int Flags;   // its CustomizeCharacterEntityData's in pf_spspawn_01
    const char* Asset;    // the PamCustomizeCharacterData's name, after the last '/'
    const char* Name;     // the spawner's name for it, for the menu and the log
  };
  // The spawner's seven, in the order of its progression flag (1-7)
  const Outfit Outfits[] = {
    { 1699013922u, "C_Faith_Prologue", "Release: civilian clothes with the gridlink (Civilian)" },
    { 1704202080u, "C_Faith_Prologue_NoGridlink", "Release: civilian clothes, no gridlink (CivilianNoGridLink)" },
    { 1688617031u, "C_Faith_NoTattoo", "runner, no tattoo (Runner)" },
    { 1685891460u, "C_Faith_NoTat_MagRope1", "runner with the MagRope (RunnerWithSwing)" },
    { 1690491225u, "C_Faith_NoTat_MagRope2", "runner with the MagRope (RunnerWithPullUp)" },
    { 1711066455u, "C_Faith_NoTat_MagRope3", "runner with the MagRope (RunnerWithPullDown)" },
    { 1689729674u, "C_Faith_WithTattoo", "runner with the tattoo (RunnerWithTattoo)" } };
  const int OutfitCount = (int)_countof(Outfits);
  // The outfit with the arm tattoo and its twin without it: the same parts but
  // the arms (see SetNoTattoo in FaithOutfit.h)
  const int WithTattooOutfit = 6;   // C_Faith_WithTattoo
  const int NoTattooTwin = 5;       // C_Faith_NoTat_MagRope3

  // Her accessories (GAME > Faith: No MagRope, No bag, Add the gridlink; see
  // FaithOutfit.h, and Research_Faith_Parts_2026-11-08.md outside the repo).
  // applyCustomization 0x14336CF00 is the character's vtable + 0x340
  // (PamServerCharacterEntity 0x141C3FCB0 and ServerCharacterEntity
  // 0x141B49E50 both point at it; no direct callers). Its args: + 0 the
  // character, + 8 its player, + 0x10 the PamCustomizeCharacterData, + 0x18 a
  // byte that skips the parts (0x14336C860 returns at once; the health
  // override after it still runs). Through 0x14336C860 it takes one unlock
  // from each of the data's VisualGroups (+ 0x18: a pointer to the first
  // element, the count a u32 right before it; each element points at that
  // group's Visual array, laid out the same), only if that unlock is exactly
  // a VisualUnlockAsset. With ClearVisualState (all 7 outfits) her server
  // customization component's set becomes those; the set goes to the client
  // through the network registry and the client rebuilds all her parts from
  // it. Nothing keeps the data, its arrays or the args after the call and
  // nothing is ref-counted on the way [code, 2026-11-08], so a copy on the
  // hook's stack, for the call, is enough. Only the game's own groups and
  // unlocks are handed over (an unlock the registry doesn't know would be
  // sent as junk).
  const __int64 ApplyCustomizationRva = 0x336CF00;
  const __int64 PamServerCharacterVTableRva = 0x1C3FCB0;
  const __int64 ServerCharacterVTableRva = 0x1B49E50;
  const int ApplyCustomizationSlot = 0x340;
  const __int64 ApplyGroupsRva = 0x336C860;              // reads the groups (checked, not hooked)
  const __int64 PamCustomizeDataVTableRva = 0x1C883E0;   // PamCustomizeCharacterData's vtable
  const __int64 PamCustomizeDataGetTypeRva = 0x4750A20;  // its slot 0 (TypeInfo 0x14286B5F0)
  const __int64 VisualUnlockAssetTypeRva = 0x281E990;    // the only class 0x14336C860 takes from a group
  const __int64 PlayerExtentOffsetRva = 0x2109550;       // u32: CharacterServerPlayerExtent's place in the player
  const unsigned int PlayerExtentOffset = 0x1540;
  const int ArgsPlayerOffset = 0x08;
  const int ArgsDataOffset = 0x10;
  const int ArgsSkipOffset = 0x18;
  const int ArgsSize = 0x20;
  const int CustomizeDataSize = 0x70;                    // PamCustomizeCharacterData (reflected)
  const int VisualGroupsOffset = 0x18;                   // CustomizeCharacterData.VisualGroups
  const int UnlockIdentifierOffset = 0x28;               // UnlockAssetBase.Identifier
  const int ExtentOriginalBegin = 0x08;                  // the extent's "original visual state" list
  const int ExtentOriginalEnd = 0x10;
  const unsigned int MaxGroups = 32;                     // her server set holds 32 (a 33rd crashes 0x143366D40)
  const unsigned int MaxSourceGroups = 64;               // an outfit has 15 at most: more isn't one

  // Each part's unlocks (UnlockAssetBase.Identifier), third and first person
  // together [data]
  const unsigned int MagRopeUnlocks[] = {
    0x783EECBA, 0xB81AB7A2,   // Unlock_Faith_MagRope, _1p: the gauntlet on her right forearm
    0x7F0CAFF8, 0xE7D72397,   // Unlock_Faith_MagR_Pickup_1, _1p: the module on it (it rests on the gauntlet)
    0x4FE12B49, 0x0341E6BA,   // MagR_Pickup_2, _1p
    0x1A88D566, 0xF0A4EEF5 }; // MagR_Pickup_3, _1p (free roam's)
  const unsigned int BagUnlocks[] = { 0xA748E4F4, 0x2FDAE210 };       // Unlock_Faith_Toolbag, Toolbag1p: the sling bag
  const unsigned int GridlinkUnlocks[] = { 0x766DA707, 0x1AC385AF };  // Unlock_Faith_GridLink, _1p: the wrist band
  const int GridlinkOutfit = 0;   // C_Faith_Prologue, the only outfit with them

  // A VisualGroups array as the game reads it: the count right before the
  // first element
  struct GroupArray
  {
    unsigned int Pad;
    unsigned int Count;
    __int64 Groups[MaxGroups];
  };
  static_assert(offsetof(GroupArray, Count) == 4 && offsetof(GroupArray, Groups) == 8, "the count must sit right before the groups");

  // The Shard's train ride dresses her itself (layer11_theshard_trainride_flow
  // #28, linked to the spawner's character [data]) in this one, with the same
  // MagRope and bag unlocks: they're left out of it too. It has no
  // first-person parts, so the gridlink isn't added to it, and the tools
  // can't dress her in it again (it isn't the spawner's).
  const char* const PartsOnlyAsset = "C_Faith_NoTat_MagRope3_NPC_InGHair";
  const int PartsOnlyOutfit = 100;
  const int NotHers = -2;   // not a player's character, or the call skips the parts

  // Which of the ticks' parts an outfit has (bits)
  const int HasMagRope = 1;
  const int HasBag = 2;
  const int HasGridlink = 4;

  // The event the tools send: + 8 its id. The handler reads its type only when
  // the outfit's "Characters" link finds no character, for a ServerPlayerEvent
  // (class ids 0x5DB..0x5DD) to dress that player's character: this type's
  // class id (+ 0x28) is 0, so then nothing is done.
  __declspec(align(16)) BYTE g_EventType[0x40] = {};
  __int64 __fastcall EventGetType(void*) { return (__int64)g_EventType; }
  void* g_EventVTable[8] = { (void*)&EventGetType, (void*)&EventGetType, (void*)&EventGetType, (void*)&EventGetType,
    (void*)&EventGetType, (void*)&EventGetType, (void*)&EventGetType, (void*)&EventGetType };

  struct Noted
  {
    __int64 Entity;
    int Outfit;
  };
  std::mutex g_NotedMutex;
  std::vector<Noted> g_Noted;   // the spawner's outfit entities, as the game made them (under g_NotedMutex)

  bool g_Available = false;
  std::atomic<int> g_Choice{ faith_outfit::Choice_Game };
  std::atomic<bool> g_Pending{ false };   // the next server update dresses her
  std::atomic<bool> g_GameSeen{ false };  // the game dressed her since its spawner was made
  std::atomic<int> g_GameOutfit{ -1 };    // what the game last dressed her in
  std::atomic<int> g_Worn{ -1 };          // what she wears, as far as the tools know
  std::atomic<int> g_ReplayOutfit{ -1 };  // the replay's (SetReplayOutfit), over the menu's choice
  std::atomic<bool> g_NoTattoo{ false };  // GAME > Faith > No arm tattoo
  std::atomic<int> g_WornTold{ -1 };      // g_Worn as WornOutfit tells it: the tattoo outfit for its twin worn in its place
  std::atomic<bool> g_Parts[faith_outfit::Part_Count] = {};  // GAME > Faith's accessory ticks
  std::atomic<bool> g_Redress{ false };   // the next server update dresses her again in what she wears (her parts changed)
  bool g_PartsAvailable = false;          // the applyCustomization hook is in
  // Her last outfit as applyCustomization gave it (on the server's thread):
  // which (0 - 6, PartsOnlyOutfit, -1 another), the ticks' parts it has and
  // the ones she was given. A tick change dresses her again only in the
  // spawner's outfit she wears, and only if her parts would change.
  std::atomic<int> g_HerLastOutfit{ -1 };
  std::atomic<int> g_HerSourceParts{ 0 };
  std::atomic<int> g_HerGivenParts{ 0 };
  std::atomic<bool> g_HerGridlinkMissing{ false };  // her last apply wanted the gridlink and found neither group
  std::atomic<bool> g_GridlinkMissingLogged{ false };
  std::atomic<bool> g_TooManyLogged{ false };
  thread_local bool t_Sending = false;    // the tools' own Apply is going through the handler

  // Which of the spawner's outfits a ServerCustomizeCharacterEntity is: its
  // data's Flags, and its outfit's name to be sure. -1: none, or the entity
  // isn't one any more (the destructor puts the base vtable back).
  int OutfitOf(__int64 entity)
  {
    __int64 base = (__int64)g_gameHandle;
    if (entity < 0x10000)
      return -1;
    __try
    {
      if (*(__int64*)entity != base + CustomizeVTableRva)
        return -1;
      __int64 data = *(__int64*)(entity + EntityDataOffset);
      if (data < 0x10000)
        return -1;
      unsigned int flags = *(unsigned int*)(data + PeerFlagsOffset);
      for (int i = 0; i < OutfitCount; ++i)
      {
        if (Outfits[i].Flags != flags)
          continue;
        __int64 asset = *(__int64*)(data + CustomizeDataOffset);
        if (asset < 0x10000)
          return -1;
        const char* name = *(const char**)(asset + AssetNameOffset);
        if (!name)
          return -1;
        const char* last = strrchr(name, '/');
        return _stricmp(last ? last + 1 : name, Outfits[i].Asset) == 0 ? i : -1;
      }
      return -1;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return -1;
    }
  }

  unsigned int EventId(__int64 event)
  {
    __try
    {
      return *(unsigned int*)(event + 8);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return 0;
    }
  }

  void Note(__int64 entity, int outfit)
  {
    std::lock_guard<std::mutex> lock(g_NotedMutex);
    // The ones gone (a level unloaded) and an old one at the same address
    std::vector<Noted> kept;
    for (Noted const& noted : g_Noted)
      if (noted.Entity != entity && OutfitOf(noted.Entity) == noted.Outfit)
        kept.push_back(noted);
    g_Noted.swap(kept);
    if (g_Noted.size() < MaxNoted)
      g_Noted.push_back({ entity, outfit });
  }

  int NotedOutfit(__int64 entity)
  {
    std::lock_guard<std::mutex> lock(g_NotedMutex);
    for (Noted const& noted : g_Noted)
      if (noted.Entity == entity)
        return noted.Outfit;
    return -1;
  }

  // The live entity for an outfit (the newest if there were several), 0 if none
  __int64 FindOutfit(int outfit)
  {
    std::lock_guard<std::mutex> lock(g_NotedMutex);
    for (size_t i = g_Noted.size(); i-- > 0;)
      if (g_Noted[i].Outfit == outfit && OutfitOf(g_Noted[i].Entity) == outfit)
        return g_Noted[i].Entity;
    return 0;
  }

  typedef void(__fastcall* tCustomizeEvent)(__int64, __int64);
  tCustomizeEvent oCustomizeEvent = nullptr;

  // Apply through the outfit's own handler (the game's code past the hook)
  bool SendApply(__int64 entity)
  {
    __declspec(align(16)) BYTE event[0x40] = {};
    *(void**)event = g_EventVTable;
    *(unsigned int*)(event + 8) = ApplyEvent;
    __try
    {
      if (*(__int64*)entity != (__int64)g_gameHandle + CustomizeVTableRva)
        return false;
      oCustomizeEvent(entity, (__int64)event);
      return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
  }

  // The outfit the replay or the menu's choice wants, -1 for none known yet
  // (before the No arm tattoo tick: see WantedOutfit)
  int ChosenOutfit()
  {
    int replay = g_ReplayOutfit.load();
    if (replay >= 0)
      return replay;
    switch (g_Choice.load())
    {
    case faith_outfit::Choice_Release: return 0;
    case faith_outfit::Choice_ReleaseNoGridlink: return 1;
    default: return g_GameOutfit.load();
    }
  }

  // With the No arm tattoo tick, the tattoo outfit's twin in its place
  int Untattooed(int outfit)
  {
    return outfit == WithTattooOutfit && g_NoTattoo.load() ? NoTattooTwin : outfit;
  }

  int WantedOutfit()
  {
    return Untattooed(ChosenOutfit());
  }

  // The ticks' parts an outfit with these ones gives her now
  int ExpectedParts(int source)
  {
    int expected = source;
    if (g_Parts[faith_outfit::Part_NoMagRope])
      expected &= ~HasMagRope;
    if (g_Parts[faith_outfit::Part_NoBag])
      expected &= ~HasBag;
    // (the gridlink's groups not found at her last outfit: it can't be added,
    // so dressing her again wouldn't change it)
    if (g_Parts[faith_outfit::Part_AddGridlink] && ((source & HasGridlink) || !g_HerGridlinkMissing.load()))
      expected |= HasGridlink;
    return expected;
  }

  const char* HerOutfitName(int outfit)
  {
    if (outfit >= 0 && outfit < OutfitCount)
      return Outfits[outfit].Asset;
    return outfit == PartsOnlyOutfit ? PartsOnlyAsset : "an outfit that isn't one of the spawner's";
  }

  // From the server update, on its thread, before its frame
  void Dress()
  {
    g_Pending = false;
    // Not before the game has dressed her since its spawner was made: until
    // then she may not be spawned, and its own Apply comes 0.1 s after she is
    // (then this comes again, see hCustomizeEvent)
    if (!g_GameSeen)
      return;
    int chosen = ChosenOutfit();
    int outfit = Untattooed(chosen);
    if (outfit < 0)
      return;
    // The replay's recording is told the chosen one: the tattoo outfit for its
    // twin worn in its place (see WornOutfit)
    int told = chosen;
    // A part tick changed: dressed again in what she wears (applyCustomization
    // gives the parts as the ticks have them, see hApplyCustomization)
    bool again = outfit == g_Worn;
    bool redress = g_Redress.exchange(false);   // another outfit gets the ticks' parts anyway
    if (again && !redress)
    {
      g_WornTold = told;
      return;
    }
    if (again)
    {
      // Only over the spawner's outfit she wears (a mission can dress her in
      // its own, e.g. The Shard's train ride: sending the spawner's would
      // replace it), and only if her parts would change (a rebuild restarts
      // her hair)
      int last = g_HerLastOutfit.load();
      if (last != outfit)
      {
        g_WornTold = told;
        util::log::Write("Faith's parts: her last outfit is %s, not the spawner's %s, so she isn't dressed again: the "
          "ticks take effect when the game dresses her next", HerOutfitName(last), Outfits[outfit].Asset);
        return;
      }
      if (ExpectedParts(g_HerSourceParts.load()) == g_HerGivenParts.load())
      {
        g_WornTold = told;
        util::log::Write("Faith's parts: %s already gives her the parts as the ticks have them, not dressed again",
          Outfits[outfit].Asset);
        return;
      }
    }
    __int64 entity = FindOutfit(outfit);
    if (!entity)
    {
      util::log::Warning("Faith's outfit: the spawner's %s isn't there (%s), she's left as she is",
        Outfits[outfit].Asset, Outfits[outfit].Name);
      return;
    }
    t_Sending = true;
    bool sent = SendApply(entity);
    t_Sending = false;
    if (sent)
    {
      g_Worn = outfit;
      g_WornTold = told;
      util::log::Write("Faith's outfit: dressed %sin %s, %s%s%s (Apply sent to the spawner's outfit entity 0x%I64X, "
        "server update thread %u)", again ? "again (her parts) " : "", Outfits[outfit].Asset, Outfits[outfit].Name,
        outfit != chosen ? ", in place of C_Faith_WithTattoo: the arm tattoo off" : "",
        g_ReplayOutfit.load() == chosen ? ", what she wore in the replay's recording" : "", entity, GetCurrentThreadId());
    }
    else
      util::log::Warning("Faith's outfit: the Apply to %s (entity 0x%I64X) failed, she's left as she is",
        Outfits[outfit].Asset, entity);
  }

  // The outfit (0 - 6) a PamCustomizeCharacterData is, by its name, or
  // PartsOnlyOutfit (if asked for); -1 if it's none of those
  int OutfitByData(__int64 data, bool partsOnly)
  {
    __try
    {
      if (data < 0x10000 || *(__int64*)data != (__int64)g_gameHandle + PamCustomizeDataVTableRva)
        return -1;
      const char* name = *(const char**)(data + AssetNameOffset);
      if (!name)
        return -1;
      const char* last = strrchr(name, '/');
      last = last ? last + 1 : name;
      for (int i = 0; i < OutfitCount; ++i)
        if (_stricmp(last, Outfits[i].Asset) == 0)
          return i;
      return partsOnly && _stricmp(last, PartsOnlyAsset) == 0 ? PartsOnlyOutfit : -1;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return -1;
    }
  }

  // A group's unlock if it has exactly one and it's a VisualUnlockAsset (the
  // game takes it then, see 0x14336C860 + 0x440); 0 otherwise
  __int64 SingleUnlock(__int64 group)
  {
    typedef __int64(__fastcall* tGetType)(__int64);
    __try
    {
      if (group < 0x10000 || *(unsigned int*)(group - 4) != 1)
        return 0;
      __int64 unlock = *(__int64*)group;
      if (unlock < 0x10000)
        return 0;
      tGetType getType = **(tGetType**)unlock;
      return getType(unlock) == (__int64)g_gameHandle + VisualUnlockAssetTypeRva ? unlock : 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return 0;
    }
  }

  bool InList(unsigned int id, const unsigned int* ids, size_t count)
  {
    for (size_t i = 0; i < count; ++i)
      if (ids[i] == id)
        return true;
    return false;
  }

  // The outfit an applyCustomization call gives a player's character: 0 - 6
  // (the spawner's), PartsOnlyOutfit, -1 another one; NotHers if it isn't a
  // player's character or the call skips the parts
  int HerOutfitApply(__int64 args, __int64* data)
  {
    __try
    {
      if (args < 0x10000 || *(BYTE*)(args + ArgsSkipOffset) != 0 || *(__int64*)(args + ArgsPlayerOffset) < 0x10000)
        return NotHers;
      *data = *(__int64*)(args + ArgsDataOffset);
      return OutfitByData(*data, true);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return NotHers;
    }
  }

  // C_Faith_Prologue's two gridlink groups (third, first person) from the
  // spawner's outfit entity, read each time (the data is the level's)
  void ReadGridlinkGroups(__int64 entity, __int64* out)
  {
    out[0] = out[1] = 0;
    __try
    {
      __int64 data = *(__int64*)(*(__int64*)(entity + EntityDataOffset) + CustomizeDataOffset);
      if (OutfitByData(data, false) != GridlinkOutfit)
        return;
      __int64 groups = *(__int64*)(data + VisualGroupsOffset);
      unsigned int count = groups >= 0x10000 ? *(unsigned int*)(groups - 4) : 0;
      if (count > MaxSourceGroups)
        return;
      for (unsigned int i = 0; i < count; ++i)
      {
        __int64 group = *(__int64*)(groups + 8 * (__int64)i);
        __int64 unlock = SingleUnlock(group);
        if (!unlock)
          continue;
        unsigned int id = *(unsigned int*)(unlock + UnlockIdentifierOffset);
        for (int k = 0; k < 2; ++k)
          if (id == GridlinkUnlocks[k])
            out[k] = group;
      }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      out[0] = out[1] = 0;
    }
  }

  enum EditResult
  {
    Edit_None,         // given as it is: nothing to change, or not readable
    Edit_Made,         // newArgs hand the call the edited copy
    Edit_FirstOutfit,  // her first outfit (the game keeps it as her original): given as it is
    Edit_TooMany       // more groups than her set holds
  };

  struct EditCounts
  {
    int Dropped;          // groups left out
    int Added;            // gridlink groups added
    unsigned int Groups;  // groups in the outfit
    int Source;           // the ticks' parts the outfit has (HasMagRope ...)
    int Given;            // the ones she's given (set with Edit_Made only)
  };

  // The outfit's copy with the ticks' parts left out or added: copy (its
  // 0x70 bytes, VisualGroups pointing at array), and newArgs (the call's args
  // pointing at copy). All on the hook's stack, read only during the call.
  int EditCustomization(__int64 args, __int64 data, const bool* parts, const __int64* gridlink, BYTE* newArgs,
    BYTE* copy, GroupArray* array, EditCounts* counts)
  {
    __try
    {
      __int64 source = *(__int64*)(data + VisualGroupsOffset);
      if (source < 0x10000)
        return Edit_None;
      unsigned int count = *(unsigned int*)(source - 4);
      if (count > MaxSourceGroups)
        return Edit_None;
      counts->Groups = count;
      bool hasGridlink[2] = { false, false };
      unsigned int kept = 0;
      bool tooMany = false;
      int given = 0;
      for (unsigned int i = 0; i < count; ++i)
      {
        __int64 group = *(__int64*)(source + 8 * (__int64)i);
        __int64 unlock = SingleUnlock(group);
        if (unlock)
        {
          unsigned int id = *(unsigned int*)(unlock + UnlockIdentifierOffset);
          bool magRope = InList(id, MagRopeUnlocks, _countof(MagRopeUnlocks));
          bool bag = InList(id, BagUnlocks, _countof(BagUnlocks));
          counts->Source |= (magRope ? HasMagRope : 0) | (bag ? HasBag : 0);
          if ((magRope && parts[faith_outfit::Part_NoMagRope]) || (bag && parts[faith_outfit::Part_NoBag]))
          {
            ++counts->Dropped;
            continue;
          }
          for (int k = 0; k < 2; ++k)
          {
            if (id != GridlinkUnlocks[k])
              continue;
            hasGridlink[k] = true;
            counts->Source |= HasGridlink;
          }
          given |= (magRope ? HasMagRope : 0) | (bag ? HasBag : 0) |
            (id == GridlinkUnlocks[0] || id == GridlinkUnlocks[1] ? HasGridlink : 0);
        }
        if (kept >= MaxGroups)
          tooMany = true;
        else
          array->Groups[kept++] = group;
      }
      if (parts[faith_outfit::Part_AddGridlink])
      {
        for (int k = 0; k < 2; ++k)
        {
          if (hasGridlink[k] || !gridlink[k])
            continue;
          if (kept >= MaxGroups)
          {
            tooMany = true;
            continue;
          }
          array->Groups[kept++] = gridlink[k];
          ++counts->Added;
          given |= HasGridlink;
        }
      }
      if (tooMany)
        return Edit_TooMany;
      if (!counts->Dropped && !counts->Added)
        return Edit_None;
      // Her very first outfit is the one the game keeps as her original (the
      // player's extent records its unlocks only while its list is empty, for
      // a later RestoreToOriginalVisualState or an apply with no data, which
      // give that list back): given as it is
      if (*(unsigned int*)((__int64)g_gameHandle + PlayerExtentOffsetRva) != PlayerExtentOffset)
        return Edit_None;
      __int64 extent = *(__int64*)(args + ArgsPlayerOffset) + PlayerExtentOffset;
      if (*(__int64*)(extent + ExtentOriginalBegin) == *(__int64*)(extent + ExtentOriginalEnd))
        return Edit_FirstOutfit;
      array->Pad = 0;
      array->Count = kept;
      memcpy(copy, (const void*)data, CustomizeDataSize);
      *(__int64*)(copy + VisualGroupsOffset) = (__int64)array->Groups;
      memcpy(newArgs, (const void*)args, ArgsSize);
      *(__int64*)(newArgs + ArgsDataOffset) = (__int64)copy;
      counts->Given = given;
      return Edit_Made;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return Edit_None;
    }
  }
}

// Each ServerCustomizeCharacterEntity as the game makes it: the spawner's are noted
typedef __int64(__fastcall* tCustomizeCtor)(__int64);
tCustomizeCtor oCustomizeCtor = nullptr;
__int64 __fastcall hCustomizeCtor(__int64 createInfo)
{
  __int64 entity = oCustomizeCtor(createInfo);
  if (entity && !g_shutdown)
  {
    int outfit = OutfitOf(entity);
    if (outfit >= 0)
    {
      Note(entity, outfit);
      // A new spawner (a level load): wait for the game to dress her again
      g_GameSeen = false;
      g_Worn = -1;
      g_WornTold = -1;
    }
  }
  return entity;
}

// Every event to one: the game's own Apply on the spawner's tells what it
// dresses her in; if the menu wants another, the next server update does it
void __fastcall hCustomizeEvent(__int64 entity, __int64 event)
{
  if (t_Sending || g_shutdown)
  {
    oCustomizeEvent(entity, event);
    return;
  }
  int outfit = EventId(event) == ApplyEvent ? NotedOutfit(entity) : -1;
  // The game dresses her: applyCustomization gives the parts as the ticks
  // have them now, so a redress asked for before isn't needed (one asked for
  // during this call, for her first outfit, stays)
  if (outfit >= 0)
    g_Redress = false;
  oCustomizeEvent(entity, event);
  if (outfit < 0)
    return;
  bool changed = outfit != g_GameOutfit;
  g_GameOutfit = outfit;
  g_Worn = outfit;
  g_WornTold = outfit;
  g_GameSeen = true;
  if (changed)
    util::log::Write("Faith's outfit: the game dressed her in %s, %s (thread %u)", Outfits[outfit].Asset,
      Outfits[outfit].Name, GetCurrentThreadId());
  // The menu's choice, the replay's or the No arm tattoo tick: as the game
  // dressed her is the wanted one unless one of them has another (the tick:
  // the tattoo outfit's twin, at the next server update)
  if (WantedOutfit() != outfit)
    g_Pending = true;
}

// Every character's outfit (applyCustomization, on the server's thread): with
// a part tick on, her outfits from the spawner (and the train ride's) get the
// edited copy for the call. Each of hers is noted (g_HerLastOutfit ...), so a
// tick change knows whether dressing her again would change anything.
typedef __int64(__fastcall* tApplyCustomization)(__int64, __int64);
tApplyCustomization oApplyCustomization = nullptr;
__int64 __fastcall hApplyCustomization(__int64 character, __int64 args)
{
  if (g_shutdown)
    return oApplyCustomization(character, args);
  __int64 data = 0;
  int outfit = HerOutfitApply(args, &data);
  if (outfit == NotHers)
    return oApplyCustomization(character, args);
  if (outfit < 0)
  {
    // Hers, but an outfit the tools don't know: given as it is
    g_HerLastOutfit = -1;
    g_HerSourceParts = 0;
    g_HerGivenParts = 0;
    return oApplyCustomization(character, args);
  }
  bool parts[faith_outfit::Part_Count];
  for (int i = 0; i < faith_outfit::Part_Count; ++i)
    parts[i] = g_Parts[i].load();
  // The gridlink's groups from C_Faith_Prologue, not for the train ride's
  // outfit (g_NotedMutex isn't held across the outfit handler's call, so it
  // can be taken here)
  __int64 gridlink[2] = { 0, 0 };
  if (parts[faith_outfit::Part_AddGridlink] && outfit != PartsOnlyOutfit)
  {
    __int64 prologue = FindOutfit(GridlinkOutfit);
    if (prologue)
      ReadGridlinkGroups(prologue, gridlink);
    if ((!gridlink[0] || !gridlink[1]) && !g_GridlinkMissingLogged.exchange(true))
      util::log::Warning("Faith's parts: the gridlink's parts weren't found in the spawner's C_Faith_Prologue (%s, %s), "
        "what's missing isn't added", gridlink[0] ? "third person found" : "third person missing",
        gridlink[1] ? "first person found" : "first person missing");
  }
  __declspec(align(16)) BYTE newArgs[ArgsSize];
  __declspec(align(16)) BYTE copy[CustomizeDataSize];
  GroupArray array;
  EditCounts counts = {};
  int result = EditCustomization(args, data, parts, gridlink, newArgs, copy, &array, &counts);
  g_HerLastOutfit = outfit;
  g_HerSourceParts = counts.Source;
  g_HerGivenParts = result == Edit_Made ? counts.Given : counts.Source;
  g_HerGridlinkMissing = parts[faith_outfit::Part_AddGridlink] && outfit != PartsOnlyOutfit && !gridlink[0] && !gridlink[1];
  if (result == Edit_FirstOutfit)
  {
    util::log::Write("Faith's parts: %s is her first outfit (the game keeps it as her original), given as it is; "
      "dressed again at the next server update or the game's next outfit, whichever comes first", HerOutfitName(outfit));
    g_Redress = true;
    g_Pending = true;
  }
  else if (result == Edit_TooMany && !g_TooManyLogged.exchange(true))
    util::log::Warning("Faith's parts: %s would have more than %u parts, given as it is", HerOutfitName(outfit), MaxGroups);
  if (result != Edit_Made)
    return oApplyCustomization(character, args);
  util::log::Write("Faith's parts: %s given with %d of its %u parts left out and %d added (No MagRope %s, No bag %s, "
    "Add the gridlink %s; thread %u)", HerOutfitName(outfit), counts.Dropped, counts.Groups, counts.Added,
    parts[faith_outfit::Part_NoMagRope] ? "on" : "off", parts[faith_outfit::Part_NoBag] ? "on" : "off",
    parts[faith_outfit::Part_AddGridlink] ? "on" : "off", GetCurrentThreadId());
  return oApplyCustomization(character, (__int64)newArgs);
}

typedef bool(__fastcall* tServerUpdate)(__int64, __int64);
tServerUpdate oServerUpdate = nullptr;
bool __fastcall hServerUpdate(__int64 server, __int64 info)
{
  if (g_Pending && !g_shutdown)
    Dress();
  // The replay's stand-ins, on this thread too (see the hooks' AskStandIn)
  util::hooks::ReplayServerUpdate();
  // And its mission's world (see ReplayAssetLog's missions)
  replay_assetlog::ServerUpdate();
  // The game's Dash messages heard on the server's manager too (see game_messages)
  game_messages::ServerUpdate();
  return oServerUpdate(server, info);
}

namespace
{
  // The code the hooks and the Apply rely on, read before anything is hooked
  bool CodeKnown()
  {
    BYTE const* pBase = (BYTE const*)g_gameHandle;
    // push rdi; sub rsp, 0x30; mov qword [rsp + 0x20], -2; mov [rsp + 0x40], rbx; mov rbx, rcx;
    // mov edx, 0xE6FFA20F; lea edx, [rdx + 0x19005E21] (size 0x30)
    static const BYTE ctor[] = { 0x57, 0x48, 0x83, 0xEC, 0x30, 0x48, 0xC7, 0x44, 0x24, 0x20, 0xFE, 0xFF, 0xFF, 0xFF, 0x48,
      0x89, 0x5C, 0x24, 0x40, 0x48, 0x89, 0xCB, 0xBA, 0x0F, 0xA2, 0xFF, 0xE6, 0x8D, 0x92, 0x21, 0x5E, 0x00, 0x19 };
    // the vtable stored at + 0x5B: lea rax, [rip - 0x1841A32]; mov [rdi], rax
    static const BYTE ctorVTable[] = { 0x48, 0x8D, 0x05, 0xCE, 0xE5, 0x7B, 0xFE, 0x48, 0x89, 0x07 };
    // push rbp; push rdi; push r12; push r14; push r15; lea rbp, [rsp - 0x1660]
    static const BYTE handler[] = { 0x55, 0x57, 0x41, 0x54, 0x41, 0x56, 0x41, 0x57, 0x48, 0x8D, 0xAC, 0x24, 0xA0, 0xE9,
      0xFF, 0xFF };
    // + 0x46: mov r15, rdx; mov r14, rcx; cmp dword [rdx + 8], 0x0C487271; jne; mov rdi, [rcx + 0x20]; mov rax, [rcx + 0x28]
    static const BYTE apply[] = { 0x49, 0x89, 0xD7, 0x49, 0x89, 0xCE, 0x81, 0x7A, 0x08, 0x71, 0x72, 0x48, 0x0C, 0x0F,
      0x85, 0x38, 0x03, 0x00, 0x00, 0x48, 0x8B, 0x79, 0x20, 0x48, 0x8B, 0x41, 0x28 };
    // + 0x1B0: the character's customize: mov rax, [r8]; lea rdx, [rsp + 0x30]; mov rcx, r8; call [rax + 0x340]
    static const BYTE customize[] = { 0x49, 0x8B, 0x00, 0x48, 0x8D, 0x54, 0x24, 0x30, 0x4C, 0x89, 0xC1, 0xFF, 0x90, 0x40,
      0x03, 0x00, 0x00 };
    // + 0x27F: no character linked: mov rax, [r15]; mov rcx, r15; call [rax]; movzx edx, word [rax + 0x28]
    static const BYTE fallback[] = { 0x49, 0x8B, 0x07, 0x4C, 0x89, 0xF9, 0xFF, 0x10, 0x0F, 0xB7, 0x50, 0x28 };
    // mov [rsp + 8], rbx; push rdi; sub rsp, 0x20; mov rbx, rdx; mov rdi, rcx; call; ...; mov al, 1; ...; ret
    static const BYTE update[] = { 0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x89, 0xD3, 0x48,
      0x89, 0xCF, 0xE8 };
    static const BYTE updateEnd[] = { 0x48, 0x8B, 0x5C, 0x24, 0x30, 0xB0, 0x01, 0x48, 0x83, 0xC4, 0x20, 0x5F, 0xC3 };
    BYTE const* vtable = pBase + CustomizeVTableRva;
    return !memcmp(pBase + CustomizeCtorRva, ctor, sizeof(ctor)) &&
      !memcmp(pBase + CustomizeCtorRva + 0x5B, ctorVTable, sizeof(ctorVTable)) &&
      !memcmp(pBase + CustomizeEventRva, handler, sizeof(handler)) &&
      !memcmp(pBase + CustomizeEventRva + 0x46, apply, sizeof(apply)) &&
      !memcmp(pBase + CustomizeEventRva + 0x1B0, customize, sizeof(customize)) &&
      !memcmp(pBase + CustomizeEventRva + 0x27F, fallback, sizeof(fallback)) &&
      !memcmp(pBase + ServerUpdateRva, update, sizeof(update)) &&
      !memcmp(pBase + ServerUpdateRva + 0x2B, updateEnd, sizeof(updateEnd)) &&
      *(__int64 const*)vtable == (__int64)pBase + CustomizeTypeRva &&
      *(__int64 const*)(vtable + 4 * 8) == (__int64)pBase + CustomizeEventRva &&
      *(__int64 const*)(vtable + 7 * 8) == (__int64)pBase + EntityDeleteRva;
  }

  // The code the parts' hook relies on (see the parts' constants), read
  // before it's made
  bool PartsCodeKnown()
  {
    BYTE const* pBase = (BYTE const*)g_gameHandle;
    __int64 base = (__int64)g_gameHandle;
    // applyCustomization: mov [rsp + 8], rbx; push rdi; sub rsp, 0x20; mov rdi, rcx; mov rcx, [rdx + 8] (the player)
    static const BYTE apply[] = { 0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x89, 0xCF, 0x48, 0x8B,
      0x4A, 0x08 };
    // 0x14336C860 + 0x3E: cmp byte [rdx + 0x18], 0 (the args' skip-the-parts byte)
    static const BYTE skip[] = { 0x80, 0x7A, 0x18, 0x00 };
    // + 0x2AA: mov r14, [r13 + 8]; cmp r14, [r13 + 0x10] (the extent's original list)
    static const BYTE original[] = { 0x4D, 0x8B, 0x75, 0x08, 0x4D, 0x3B, 0x75, 0x10 };
    // + 0x3BF: mov rax, [r13 + 0x10]; cmp [r13 + 8], rax (it's recorded only while empty)
    static const BYTE record[] = { 0x49, 0x8B, 0x45, 0x10, 0x49, 0x39, 0x45, 0x08 };
    // + 0x3F3: mov r14, [r14 + 0x18]; mov eax, [r14 - 4] (VisualGroups and its count)
    static const BYTE groups[] = { 0x4D, 0x8B, 0x76, 0x18, 0x41, 0x8B, 0x46, 0xFC };
    // + 0x40D: mov rax, [r14]; cmp dword [rax - 4], 0 (a group's Visual array and its count)
    static const BYTE visual[] = { 0x49, 0x8B, 0x06, 0x83, 0x78, 0xFC, 0x00 };
    // + 0x440: mov rax, [rcx]; call [rax]; movzx edx, word [rax + 0x28] (the unlock's class)
    static const BYTE type[] = { 0x48, 0x8B, 0x01, 0xFF, 0x10, 0x0F, 0xB7, 0x50, 0x28 };
    // PamCustomizeCharacterData's getType: lea rax, [rip - 0x1EE5437] (TypeInfo 0x14286B5F0); ret
    static const BYTE getType[] = { 0x48, 0x8D, 0x05, 0xC9, 0xAB, 0x11, 0xFE, 0xC3 };
    BYTE const* groupsCode = pBase + ApplyGroupsRva;
    return !memcmp(pBase + ApplyCustomizationRva, apply, sizeof(apply)) &&
      !memcmp(groupsCode + 0x3E, skip, sizeof(skip)) &&
      !memcmp(groupsCode + 0x2AA, original, sizeof(original)) &&
      !memcmp(groupsCode + 0x3BF, record, sizeof(record)) &&
      !memcmp(groupsCode + 0x3F3, groups, sizeof(groups)) &&
      !memcmp(groupsCode + 0x40D, visual, sizeof(visual)) &&
      !memcmp(groupsCode + 0x440, type, sizeof(type)) &&
      !memcmp(pBase + PamCustomizeDataGetTypeRva, getType, sizeof(getType)) &&
      *(__int64 const*)(pBase + PamCustomizeDataVTableRva) == base + PamCustomizeDataGetTypeRva &&
      *(__int64 const*)(pBase + PamServerCharacterVTableRva + ApplyCustomizationSlot) == base + ApplyCustomizationRva &&
      *(__int64 const*)(pBase + ServerCharacterVTableRva + ApplyCustomizationSlot) == base + ApplyCustomizationRva;
  }
}

void faith_outfit::Init()
{
  __int64 base = (__int64)g_gameHandle;
  if (!CodeKnown())
  {
    util::log::Warning("Faith's outfit: the game's character customizing code isn't what the tools know, GAME > Faith "
      "can't change her outfit");
    return;
  }
  // The handler and the update before the constructor: nothing is noted
  // unless all three are in
  bool hooked = util::hooks::CreateGameHook("CustomizeCharacterEvent", base + CustomizeEventRva, hCustomizeEvent,
    (LPVOID*)&oCustomizeEvent);
  hooked = hooked && util::hooks::CreateGameHook("ServerUpdate", base + ServerUpdateRva, hServerUpdate,
    (LPVOID*)&oServerUpdate);
  hooked = hooked && util::hooks::CreateGameHook("CustomizeCharacterConstructor", base + CustomizeCtorRva,
    hCustomizeCtor, (LPVOID*)&oCustomizeCtor);
  g_Available = hooked;
  if (!hooked)
  {
    util::log::Warning("Faith's outfit: not all its hooks are in, GAME > Faith can't change her outfit");
    return;
  }
  // Her parts (the ticks dress her again through the outfits' Apply above)
  if (!PartsCodeKnown())
  {
    util::log::Warning("Faith's parts: the game's applyCustomization code isn't what the tools know, GAME > Faith "
      "can't take her MagRope or bag off or add the gridlink");
    return;
  }
  g_PartsAvailable = util::hooks::CreateGameHook("ApplyCustomization", base + ApplyCustomizationRva,
    hApplyCustomization, (LPVOID*)&oApplyCustomization);
  if (!g_PartsAvailable)
    util::log::Warning("Faith's parts: its hook isn't in, GAME > Faith can't take her MagRope or bag off or add the "
      "gridlink");
}

bool faith_outfit::IsAvailable()
{
  return g_Available;
}

void faith_outfit::SetChoice(int choice)
{
  if (choice < 0 || choice >= Choice_Count || choice == g_Choice)
    return;
  g_Choice = choice;
  util::log::Write("Faith's outfit: %s chosen", ChoiceName(choice));
  g_Pending = true;
}

int faith_outfit::GetChoice()
{
  return g_Choice;
}

int faith_outfit::GameOutfit()
{
  return g_GameOutfit.load();
}

// The Release mission sets the spawner's flag to 1 at its start and to 2
// further on (see above); nothing else dresses her in those
bool faith_outfit::IsReleaseOutfit(int outfit)
{
  return outfit == 0 || outfit == 1;
}

int faith_outfit::WornOutfit()
{
  // The tattoo outfit for its twin worn in its place (see FaithOutfit.h)
  return g_WornTold.load();
}

void faith_outfit::SetReplayOutfit(int outfit)
{
  if (outfit < -1 || outfit >= OutfitCount)
    return;
  if (g_ReplayOutfit.exchange(outfit) != outfit)
    g_Pending = true;   // the next server update dresses her (see Dress)
}

const char* faith_outfit::OutfitName(int outfit)
{
  return outfit >= 0 && outfit < OutfitCount ? Outfits[outfit].Name : "not known";
}

const char* faith_outfit::ChoiceName(int choice)
{
  switch (choice)
  {
  // The menu's names (2026-11-01): Runner is what the game has her in
  case Choice_Release: return "Prologue with gridlink";
  case Choice_ReleaseNoGridlink: return "Prologue";
  default: return "Runner";
  }
}

void faith_outfit::SetNoTattoo(bool on)
{
  if (g_NoTattoo.exchange(on) == on)
    return;
  util::log::Write("Faith's outfit: arm tattoo %s", on ? "off (C_Faith_WithTattoo's twin without it, "
    "C_Faith_NoTat_MagRope3, dresses her in its place)" : "as her outfit has it");
  g_Pending = true;   // the next server update dresses her (see Dress)
}

bool faith_outfit::GetNoTattoo()
{
  return g_NoTattoo.load();
}

bool faith_outfit::ArePartsAvailable()
{
  return g_Available && g_PartsAvailable;
}

void faith_outfit::SetPart(int part, bool on)
{
  if (part < 0 || part >= Part_Count || g_Parts[part].exchange(on) == on)
    return;
  bool available = ArePartsAvailable();
  util::log::Write("Faith's parts: %s %s%s", PartLabel(part), on ? "ticked" : "unticked",
    available ? "" : " (kept for the config: the parts can't be changed, see above)");
  if (!available)
    return;
  // The next server update dresses her again in what she wears if that
  // changes her parts (see Dress)
  g_Redress = true;
  g_Pending = true;
}

bool faith_outfit::GetPart(int part)
{
  return part >= 0 && part < Part_Count && g_Parts[part].load();
}

const char* faith_outfit::PartLabel(int part)
{
  switch (part)
  {
  case Part_NoMagRope: return "No MagRope";
  case Part_NoBag: return "No bag";
  case Part_AddGridlink: return "Add the gridlink";
  default: return "?";
  }
}

const char* faith_outfit::PartSetting(int part)
{
  switch (part)
  {
  case Part_NoMagRope: return "NoMagRope";
  case Part_NoBag: return "NoBag";
  case Part_AddGridlink: return "AddGridlink";
  default: return "";
  }
}

const char* faith_outfit::GetStatus(char* buffer, size_t size)
{
  int worn = g_Worn, game = g_GameOutfit, replay = g_ReplayOutfit;
  if (!g_Available)
    sprintf_s(buffer, size, "Not available (see CT.log)");
  else if (worn == NoTattooTwin && g_WornTold == WithTattooOutfit)
    sprintf_s(buffer, size, "She wears %s, the arm tattoo off (in place of %s)", Outfits[worn].Name,
      Outfits[WithTattooOutfit].Name);
  else if (replay >= 0 && g_GameSeen)
    sprintf_s(buffer, size, "The replay has her in %s, as recorded%s", Outfits[replay].Name,
      worn == replay ? "" : " (at the next update)");
  else if (!g_GameSeen)
    sprintf_s(buffer, size, "Waiting for the game to dress her (it does when she spawns)");
  else if (worn >= 0 && worn == game)
    sprintf_s(buffer, size, "She wears %s, as the game has her", Outfits[worn].Name);
  else if (worn >= 0)
    sprintf_s(buffer, size, "She wears %s; the game has her in %s", Outfits[worn].Name,
      game >= 0 ? Outfits[game].Name : "?");
  else
    sprintf_s(buffer, size, "Not known what she wears");
  return buffer;
}
