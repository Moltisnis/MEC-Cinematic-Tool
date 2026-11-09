#include "DogenDoor.h"
#include "DogenGlow.h"
#include "../Main.h"
#include "../Util/Util.h"
#include <Windows.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <initializer_list>

// Dogen's place (2026-11-03, offline; Research_Dogen_Apartment_2026-11-03.md). Every Dogen briefing (Benefactor,
// Family Matters, Kingdom) has its mission interaction at this door and teleports Faith back out in front of it after
// the cutscene. Behind it the city's zone centralcityprops_zs_zs_397 places Dogen's kitchen (the cutscene set
// objects/cutscene/05_benefactor_dogen/architecture/kitchen: kitchen01, its counter, corridor and back room), all with
// collision, so the inside is there in free roam. The door itself is no game door (those are prefabs, opened by an
// Ant animation from their pf_doorlogic): it's a plain member of the zone's static model group, solid and shut.
//
// The game takes a member's instance out of its collision with the group's physics component
// (ClientStaticModelGroupPhysicsComponent, vtable 0x141B1CE08), as its Enabled event does (0x1431856E0): slot 60
// (0x1434DB840) clears the part's bit on each of the component's bodies (the body record's slot 82, 0x1434EFFA0:
// the bits at record + 0xB8, then hknpWorld 0x143F3CD00 for the body), slot 59 (0x1434DB9D0, the record's slot 81,
// 0x1434F0100) sets it again. The rest of the Enabled event (its queued work and the parts' components) is left out.
// Faith moves by the client's physics (the lair's windows, 2026-10-27), which is what this changes; the server's copy
// of the door stays.
//
// What draws a member placed by its physics is one object per instance, the component's for its first part (slot 66,
// 0x1434DC6D0: [component + 0x68][part]). The member's Visible (the group's handler, then 0x14318F730) hands the
// renderer a description of it when it's shown (0x1436C40B0; 0x1436D2EC0 takes it away), keeping the renderer's
// handle at + 0x8C (-1 while not shown); the description's transform is the object's own rows at + 0x10 (right, up,
// forward, position: 0x143769330). So the door swings by turning those rows round its hinge and giving it Visible
// false, then true, each frame of the swing: the game draws it again where the rows now say. Its collision goes as it
// starts to open and comes back once it's shut again. Open it lies against the entrance's side wall (the vestibule
// is the door's depth beside its hinge), whose own collision is there.
//
// The rooms are inside the building anc_area02_building03_edited (zone centralcitybuildings_zs_zs_5), whose own
// collision (one part of its zone's body: 137 convex blocks) fills them with two boxes drawn nowhere: its block 0 the
// corridor, its block 37 the kitchen, the back room and the building's walls round them, the street front included
// (2026-11-03: the user, with noclip, "the weird collision inside"; Research_Dogen_Apartment_2026-11-03.md section 3).
// The zone's physics is loaded as it's stored (a Havok packfile), so both blocks are at fixed places from its root
// shape (the zone body's shape). Once each copy is found as the file has it (both blocks' vertices and planes, byte
// for byte), block 37 is made the street front's slab only (its inner side moved out to the vestibule's wall) and
// block 0 a 5 cm cube inside the corridor's outer wall; then the body's caches are renewed as the game does after
// its collision changes (0x143F3CD00).
//
// A second door is swung the same way (the user, 2026-11-03 21:1x, standing at it: "make the door faith is next to
// now same as the dogens door"): in The View, the closed-off door on a roof's catwalk (zone centralcityprops_zs_zs_417,
// 960.64, 104.64, 170.08), the member objects/props/door/door_closedoff_01/door_closedoff_01_mesh, its instance 5 of 7
// (the member's first physics part 438, 2 a instance: parts 448 - 449; of the 121 door_closedoff_01 members in
// sp_maincity's files no other has 7 instances from 438). Its building, vw_area01_building08a (zone
// centralcitybuildings_zs_zs_109), has the doorway through its wall, drawn and solid (1.28 x 2.56 m, 0.33 m deep), and
// behind it an open roof at the door's floor (15 x 15 m, drawn and solid; its parapets, scaffolding and air-con units
// clear of the door's swing). The door's mesh holds its own frame strip and its origin is its foot's middle, not its
// hinge: its handle is on the catwalk side at its right, so it's hinged at its left edge and swings into the roof
// (Research_Dogen_Apartment_2026-11-03.md section 7).
namespace
{
  // A door the tools swing: the member of a zone's static model group that's it (its mesh's file name, its instance
  // count, placed by its physics, its first physics part and parts a instance) and which of its instances; another
  // member of the same group that has to be there with it (1 instance, its 1 part; nullptr none); the instance's
  // placement in the data (right, up, forward, position: the rows the tools find in its draw object before they touch
  // it, within RowsTurnEpsilon / RowsPlaceEpsilon); its middle; the ways along it and through it to the side it opens
  // to (x, z); its floor; its hinge in its own frame (x along its right, z along its forward); which way its right
  // turns as it opens (-1 toward -forward, its body lying along -right from the hinge; 1 toward forward, its body along
  // +right).
  struct DoorPlace
  {
    const char* Name;   // in the log
    const char* Mesh;
    int Instances;
    int Instance;
    unsigned int FirstPart;
    unsigned int Parts;
    const char* AnchorMesh;
    unsigned int AnchorPart;
    float Rows[4][3];
    float Middle[3];
    float Along[2];
    float Through[2];
    float Floor;
    float Hinge[2];
    float Turn;
  };
  const int DoorCount = 2;
  const DoorPlace Doors[DoorCount] = {
    // Dogen's: the member objects/props/door/door_oneway_01/door_oneway_right_01_mesh of zone 397's group, 1
    // instance, physics parts 290 - 292 (PhysicsPartRange.First, 3 a instance), placed by the zone's physics
    // (staticmodelgroup_physics_win32, compound instance 268: at 186.608, 66.5, 305.598, its right 0.83147, 0, 0.55557,
    // its collision from there to 1.14 m along -right, 2.5 m tall), with kitchen01 (1 instance, part 597) in its group
    // (objects/cutscene/05_benefactor_dogen/architecture/kitchen/kitchen01_mesh and objects/props/door/door_oneway_01/
    // door_oneway_right_01_mesh: no other mesh in the game has either file name). Its hinge is its origin [data +
    // inferred]: the game's one-way door prefab (pf_door_oneway_right_16_01) puts the same door 0.56 m off its frame's
    // middle, at the frame's edge, and here the origin is at the frame's left post. The handle is 1 m from it. It
    // swings to its forward (the inside): the entrance is a vestibule the door's width (1.14 m) deep beside the hinge,
    // so open it lies along the vestibule's side wall. The game's own one-way doors are opened from their -forward side
    // (pf_doorlogic's triggers at z -0.4 / -0.8), here the street.
    { "Dogen's door", "door_oneway_right_01_mesh", 1, 0, 290, 3, "kitchen01_mesh", 597,
      { { 0.83147f, 0.f, 0.55557f }, { 0.f, 1.f, 0.f }, { -0.55557f, 0.f, 0.83147f }, { 186.608f, 66.5f, 305.598f } },
      { 186.138f, 67.75f, 305.284f }, { 0.83147f, 0.55557f }, { -0.55557f, 0.83147f }, 66.5f, { 0.f, 0.f }, -1.f },
    // The View's (above): zone 417's compound instance 435 (part 448). Its mesh is 1.28 m wide, x -0.64 .. 0.64, its
    // faces from z -0.162 (the handle) to its back at 0.00149; hinged at its back left corner (the handle's at its
    // right), it swings to its forward, into the roof: onto the catwalk (1.9 m deep) it would stand across the way
    // from the stairs to the doorway. Its middle 1.28 m over its origin.
    { "The View's roof door", "door_closedoff_01_mesh", 7, 5, 438, 2, nullptr, 0,
      { { 0.6343933f, 0.f, -0.7730104f }, { 0.f, 1.f, 0.f }, { 0.7730104f, 0.f, 0.6343933f },
        { 960.6376f, 104.6418f, 170.0821f } },
      { 960.6376f, 105.9218f, 170.0821f }, { 0.6343933f, -0.7730104f }, { 0.7730104f, 0.6343933f }, 104.6418f,
      { -0.64f, 0.00149f }, 1.f } };
  const float RowsTurnEpsilon = 2e-3f, RowsPlaceEpsilon = 0.02f;
  // Both swing 88 degrees, not 90. Dogen's: so its push bar (0.09 m out of its face) stays 5 mm or more off the
  // vestibule's side wall and its far edge 5 cm off the vestibule's inner wall. The View's: at 90 its back would lie in
  // the left jamb's own plane; at 88 none of its vertices is in that wall (offline, every vertex of its mesh).
  const float OpenAngle = 88.f * 3.14159265f / 180.f;
  const ULONGLONG OpenMs = 600, ShutMs = 800;
  // Open while her feet are this near it (along it; through it, + to the inside) at the floor's height, so it's open
  // by the time she reaches it, running too; shut once she's been ShutDistance from its middle (or off that height)
  // for ShutAfterMs
  const float OpenAlong = 1.5f;
  const float OpenOutside = 3.0f, OpenInside = 3.5f;
  const float OpenBelow = 0.8f, OpenAbove = 1.8f;
  const float ShutDistance = 5.0f;
  const float ShutBelow = 2.f, ShutAbove = 4.f;
  const ULONGLONG ShutAfterMs = 1500;
  const ULONGLONG MostStepMs = 50;       // one frame's swing at most (a hitch doesn't jump it)
  // The zone's group looked for this often while she's this near
  const float SearchDistance = 60.f;
  const ULONGLONG SearchMs = 2000;
  // A switch that didn't take is tried again this long after, at most this many times
  const ULONGLONG RetryMs = 500;
  const int MostFailures = 3;

  // The model groups (ClientStaticModelGroupEntity, as CameraManager.cpp reads them): their ClassInfo's instance
  // lists (+ 0x60, 4 lists 0x10 apart, the link at entity + 0x40), the group's data at + 0x30 (MemberDatas at + 0x90,
  // 0x68 bytes each), its member records at [+ 0xE0] (0x130 each, made flag + 0x120) and their count at + 0xE8. In a
  // member's data (0x1431856E0 reads them): its InstanceTransforms + 0x08 (count at - 4; none for a member placed by
  // its physics), MeshAsset + 0x38 (its name at + 0x10), InstanceCount + 0x40, its first physics part + 0x48 and its
  // parts a instance + 0x50. A member's record: its data + 0x18, the group's physics component + 0x20.
  const DWORD GameTimeStamp = 0x5762E3B7;
  const __int64 ModelGroupClassRva = 0x27ED720;
  const __int64 ModelGroupVTableRva = 0x1AFBE90;
  const int ClassInstanceListsOffset = 0x60;
  const int EntityLinkOffset = 0x40;
  const int GroupDataOffset = 0x30;
  const int GroupRecordsOffset = 0xE0;
  const int GroupMemberCountOffset = 0xE8;
  const int DataMembersOffset = 0x90;
  const int MemberDataSize = 0x68;
  const int MemberTransformsOffset = 0x08;
  const int MemberMeshOffset = 0x38;
  const int MemberInstancesOffset = 0x40;
  const int MemberFirstPartOffset = 0x48;
  const int MemberPartsOffset = 0x50;
  const int AssetNameOffset = 0x10;
  const int RecordSize = 0x130;
  const int RecordDataOffset = 0x18;
  const int RecordPhysicsOffset = 0x20;
  const int RecordMadeOffset = 0x120;
  const int MaxGroupMembers = 4096;
  const int MaxGroupsWalked = 20000;
  // The group's physics component: its bodies' ids (u32) [+ 0x30], their count (u16) + 0x60, their manager + 0x58;
  // a body's record [[[manager + 0xF0] + ((id >> 16) & 0xFF) * 8] + 0x130][id & 0xFFFF] (0x1434EE790, 0x1434EC740).
  // A record's parts: one bit each from + 0xB8 (there when there are 64 or fewer, else a pointer there to them),
  // how many + 0xC0.
  const __int64 PhysicsComponentVTableRva = 0x1B1CE08;
  const __int64 PartOnRva = 0x34DB9D0;
  const __int64 PartOffRva = 0x34DB840;
  const int PartOnSlot = 59, PartOffSlot = 60;
  const __int64 BodyWorldRva = 0x34EE790;
  const __int64 BodyPartOffRva = 0x34EC740;
  const __int64 BodyPartOnRva = 0x34EC8C0;
  const __int64 RecordPartOnRva = 0x34F0100;
  const __int64 RecordPartOffRva = 0x34EFFA0;
  const int RecordPartOnSlot = 81, RecordPartOffSlot = 82;
  const int ComponentBodiesOffset = 0x30;
  const int ComponentManagerOffset = 0x58;
  const int ComponentBodyCountOffset = 0x60;
  const int ManagerWorldsOffset = 0xF0;
  const int WorldRecordsOffset = 0x130;
  const int RecordPartBitsOffset = 0xB8;
  const int RecordPartCountOffset = 0xC0;
  const int MaxBodies = 16;
  typedef void(__fastcall* tPartSwitch)(__int64, unsigned int);
  // The zone groups' bodies have the static kind of record (vtable 0x141BABBA0, read live 2026-11-03), not
  // 0x141BAC160: its part switches (slots 81 / 82, jmp 0x1434EC800 / 0x1434EC670) keep the bits at + 0x68 (how many
  // + 0x70) and its Havok id at + 0x50 (both end mov rcx, [r11 + 0x30]; mov edx, [r11 + 0x50]; mov rcx, [rcx + 0xE0];
  // jmp 0x143F3CD00)
  const __int64 StaticRecordPartOnRva = 0x34F00E0;
  const __int64 StaticRecordPartOffRva = 0x34EFF90;
  const __int64 StaticPartOnRva = 0x34EC800;
  const __int64 StaticPartOffRva = 0x34EC670;
  const int StaticRecordPartBitsOffset = 0x68;
  const int StaticRecordPartCountOffset = 0x70;
  const int StaticRecordHavokIdOffset = 0x50;
  const int RecordHavokIdOffset = 0x58;            // the other kind's (the lair windows)
  const int RecordWorldOffset = 0x30;              // a record's physics world ([+ 0xE0] its hknpWorld)
  const int WorldHavokOffset = 0xE0;
  // hknpWorld: bodies [+ 0x20], 0x90 each, how many there's room for + 0x2C (low 30 bits); a body's shape + 0x50,
  // its id + 0x60 (GroundLevel.cpp's)
  const int HavokBodiesOffset = 0x20;
  const int HavokCapacityOffset = 0x2C;
  const int HavokBodySize = 0x90;
  const int BodyShapeOffset = 0x50;
  const int BodyIdOffset = 0x60;
  // The game's renewal of a body's caches after its collision changed: 0x143F3CD00(hknpWorld, id)
  const __int64 BodyChangedRva = 0x3F3CD00;
  typedef void(__fastcall* tBodyChanged)(__int64, unsigned int);

  // The building's fill blocks (zone 5's staticmodelgroup_physics_win32, offline): the member
  // objects/architecture/anchor/buildings/area02/clustered/anc_area02_building03_edited_mesh, 1 instance, part 610
  // (1 a instance); its blocks 0 and 37, hknpConvexPolytopeShape boxes (8 vertices at + 0x50, 8 planes at + 0xD0,
  // their headers at + 0x30 / + 0x40: count 8, offset 32 / 144), at these distances from the zone's root shape in its
  // first packfile (pf0) or second (pf1); the same bytes in both. New: block 37's inner side (the building's local
  // z 0.01) at 13.655 (the kitchen frame's 1.35, the vestibule's wall), its other sides as they are; block 0 a 5 cm cube
  // at the kitchen frame's (-2.85 .. -2.80, 1.00 .. 1.05, -1.00 .. -0.95), inside the corridor's outer wall (its block
  // 8, x -2.87 .. -2.57). Block 36 (2026-11-03 11:xx, the user: the ceiling "feels it's a bit low"): the building's
  // mass over the corridor, kitchen frame x -2.87 .. -0.97, y 2.58 .. 15.41, z -12.30 .. 2.61, whose bottom was 0.95 m
  // under the corridor's drawn ceiling (zone 397's kitchen01_corridor01 block 15, 3.53 .. 3.67): its bottom (the
  // building's local y 69.077) raised to 70.03 (the kitchen frame's 3.53), its other sides as they are. Under it the
  // corridor's own walls (its blocks 8, 9, 11, up to 3.51) and ceiling stay. Over the vestibule (z 1.35 .. 2.61) that
  // leaves the band 2.56 .. 3.53 above the door with no collision from the alcove outside (no other block of the
  // building's can take it: a compound's blocks only collide inside their bounds as the file has them).
  const char BuildingMesh[] = "anc_area02_building03_edited_mesh";
  const unsigned int BuildingPart = 610;
  const int FillBlocks = 3;
  const __int64 FillFromRoot[2][FillBlocks] = { { 1232384, 1250144, 1249664 }, { 1233792, 1251552, 1251072 } };   // [pf0 / pf1][block 0 / 37 / 36]
  const int FillVertsOffset = 0x50, FillPlanesOffset = 0xD0;
  const int FillVertsHeader = 0x30, FillPlanesHeader = 0x40;
  const unsigned short FillVertsCount = 8, FillVertsRelative = 32, FillPlanesCount = 8, FillPlanesRelative = 144;
  const BYTE FillOldVerts0[128] = {
    0xE8, 0x7A, 0xF4, 0x3F, 0x03, 0x1D, 0x8A, 0x42, 0x7A, 0x1B, 0x5A, 0x41, 0x00, 0x00, 0x00, 0x3F,
    0xE8, 0x7A, 0xF4, 0x3F, 0x89, 0x08, 0x85, 0x42, 0x7A, 0x1B, 0x5A, 0x41, 0x01, 0x00, 0x00, 0x3F,
    0xE8, 0x7A, 0xF4, 0x3F, 0x03, 0x1D, 0x8A, 0x42, 0x00, 0xD6, 0x23, 0x3C, 0x02, 0x00, 0x00, 0x3F,
    0xC0, 0xD5, 0x23, 0x3C, 0x03, 0x1D, 0x8A, 0x42, 0x7A, 0x1B, 0x5A, 0x41, 0x03, 0x00, 0x00, 0x3F,
    0xE8, 0x7A, 0xF4, 0x3F, 0x89, 0x08, 0x85, 0x42, 0x00, 0xD6, 0x23, 0x3C, 0x04, 0x00, 0x00, 0x3F,
    0xC0, 0xD5, 0x23, 0x3C, 0x03, 0x1D, 0x8A, 0x42, 0x00, 0xD6, 0x23, 0x3C, 0x05, 0x00, 0x00, 0x3F,
    0xC0, 0xD5, 0x23, 0x3C, 0x89, 0x08, 0x85, 0x42, 0x7A, 0x1B, 0x5A, 0x41, 0x06, 0x00, 0x00, 0x3F,
    0xC0, 0xD5, 0x23, 0x3C, 0x89, 0x08, 0x85, 0x42, 0x00, 0xD6, 0x23, 0x3C, 0x07, 0x00, 0x00, 0x3F,
  };
  const BYTE FillOldPlanes0[128] = {
    0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xE8, 0x7A, 0xF4, 0xBF,
    0x00, 0x00, 0x80, 0xBF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xC0, 0xD5, 0x23, 0x3C,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xBF, 0x00, 0x00, 0x00, 0x80, 0x89, 0x08, 0x85, 0x42,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x03, 0x1D, 0x8A, 0xC2,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xFF, 0xFF, 0x7F, 0xBF, 0xFF, 0xD5, 0x23, 0x3C,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x7F, 0x3F, 0x79, 0x1B, 0x5A, 0xC1,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xEE, 0xFF, 0x7F, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xEE, 0xFF, 0x7F, 0xFF,
  };
  const BYTE FillNewVerts0[128] = {
    0x0A, 0xD7, 0xA3, 0x3D, 0x9A, 0x19, 0x87, 0x42, 0x14, 0xAE, 0x35, 0x41, 0x00, 0x00, 0x00, 0x3F,
    0x0A, 0xD7, 0xA3, 0x3D, 0x00, 0x00, 0x87, 0x42, 0x14, 0xAE, 0x35, 0x41, 0x01, 0x00, 0x00, 0x3F,
    0x0A, 0xD7, 0xA3, 0x3D, 0x9A, 0x19, 0x87, 0x42, 0x48, 0xE1, 0x34, 0x41, 0x02, 0x00, 0x00, 0x3F,
    0x8F, 0xC2, 0xF5, 0x3C, 0x9A, 0x19, 0x87, 0x42, 0x14, 0xAE, 0x35, 0x41, 0x03, 0x00, 0x00, 0x3F,
    0x0A, 0xD7, 0xA3, 0x3D, 0x00, 0x00, 0x87, 0x42, 0x48, 0xE1, 0x34, 0x41, 0x04, 0x00, 0x00, 0x3F,
    0x8F, 0xC2, 0xF5, 0x3C, 0x9A, 0x19, 0x87, 0x42, 0x48, 0xE1, 0x34, 0x41, 0x05, 0x00, 0x00, 0x3F,
    0x8F, 0xC2, 0xF5, 0x3C, 0x00, 0x00, 0x87, 0x42, 0x14, 0xAE, 0x35, 0x41, 0x06, 0x00, 0x00, 0x3F,
    0x8F, 0xC2, 0xF5, 0x3C, 0x00, 0x00, 0x87, 0x42, 0x48, 0xE1, 0x34, 0x41, 0x07, 0x00, 0x00, 0x3F,
  };
  const BYTE FillNewPlanes0[128] = {
    0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0A, 0xD7, 0xA3, 0xBD,
    0x00, 0x00, 0x80, 0xBF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x8F, 0xC2, 0xF5, 0x3C,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xBF, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x87, 0x42,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x9A, 0x19, 0x87, 0xC2,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xFF, 0xFF, 0x7F, 0xBF, 0x48, 0xE1, 0x34, 0x41,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x7F, 0x3F, 0x14, 0xAE, 0x35, 0xC1,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xEE, 0xFF, 0x7F, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xEE, 0xFF, 0x7F, 0xFF,
  };
  const BYTE FillOldVerts37[128] = {
    0x99, 0x99, 0x75, 0x41, 0x33, 0xB3, 0x8E, 0x42, 0x5A, 0x96, 0x6E, 0x41, 0x00, 0x00, 0x00, 0x3F,
    0x99, 0x99, 0x75, 0x41, 0x89, 0x08, 0x85, 0x42, 0x5A, 0x96, 0x6E, 0x41, 0x01, 0x00, 0x00, 0x3F,
    0x99, 0x99, 0x75, 0x41, 0x33, 0xB3, 0x8E, 0x42, 0x00, 0xCC, 0x23, 0x3C, 0x02, 0x00, 0x00, 0x3F,
    0x38, 0x0A, 0xF7, 0x3F, 0x33, 0xB3, 0x8E, 0x42, 0x5A, 0x96, 0x6E, 0x41, 0x03, 0x00, 0x00, 0x3F,
    0x99, 0x99, 0x75, 0x41, 0x89, 0x08, 0x85, 0x42, 0x00, 0xCC, 0x23, 0x3C, 0x04, 0x00, 0x00, 0x3F,
    0x38, 0x0A, 0xF7, 0x3F, 0x33, 0xB3, 0x8E, 0x42, 0x00, 0xCC, 0x23, 0x3C, 0x05, 0x00, 0x00, 0x3F,
    0x38, 0x0A, 0xF7, 0x3F, 0x89, 0x08, 0x85, 0x42, 0x5A, 0x96, 0x6E, 0x41, 0x06, 0x00, 0x00, 0x3F,
    0x38, 0x0A, 0xF7, 0x3F, 0x89, 0x08, 0x85, 0x42, 0x00, 0xCC, 0x23, 0x3C, 0x07, 0x00, 0x00, 0x3F,
  };
  const BYTE FillOldPlanes37[128] = {
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x7F, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x32, 0xB3, 0x8E, 0xC2,
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x7F, 0xBF, 0x00, 0x00, 0x00, 0x80, 0x88, 0x08, 0x85, 0x42,
    0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x99, 0x99, 0x75, 0xC1,
    0x00, 0x00, 0x80, 0xBF, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x38, 0x0A, 0xF7, 0x3F,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x80, 0xBF, 0x00, 0xCC, 0x23, 0x3C,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x5A, 0x96, 0x6E, 0xC1,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xEE, 0xFF, 0x7F, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xEE, 0xFF, 0x7F, 0xFF,
  };
  const BYTE FillNewVerts37[128] = {
    0x99, 0x99, 0x75, 0x41, 0x33, 0xB3, 0x8E, 0x42, 0x5A, 0x96, 0x6E, 0x41, 0x00, 0x00, 0x00, 0x3F,
    0x99, 0x99, 0x75, 0x41, 0x89, 0x08, 0x85, 0x42, 0x5A, 0x96, 0x6E, 0x41, 0x01, 0x00, 0x00, 0x3F,
    0x99, 0x99, 0x75, 0x41, 0x33, 0xB3, 0x8E, 0x42, 0xE1, 0x7A, 0x5A, 0x41, 0x02, 0x00, 0x00, 0x3F,
    0x38, 0x0A, 0xF7, 0x3F, 0x33, 0xB3, 0x8E, 0x42, 0x5A, 0x96, 0x6E, 0x41, 0x03, 0x00, 0x00, 0x3F,
    0x99, 0x99, 0x75, 0x41, 0x89, 0x08, 0x85, 0x42, 0xE1, 0x7A, 0x5A, 0x41, 0x04, 0x00, 0x00, 0x3F,
    0x38, 0x0A, 0xF7, 0x3F, 0x33, 0xB3, 0x8E, 0x42, 0xE1, 0x7A, 0x5A, 0x41, 0x05, 0x00, 0x00, 0x3F,
    0x38, 0x0A, 0xF7, 0x3F, 0x89, 0x08, 0x85, 0x42, 0x5A, 0x96, 0x6E, 0x41, 0x06, 0x00, 0x00, 0x3F,
    0x38, 0x0A, 0xF7, 0x3F, 0x89, 0x08, 0x85, 0x42, 0xE1, 0x7A, 0x5A, 0x41, 0x07, 0x00, 0x00, 0x3F,
  };
  const BYTE FillNewPlanes37[128] = {
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x7F, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x32, 0xB3, 0x8E, 0xC2,
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x7F, 0xBF, 0x00, 0x00, 0x00, 0x80, 0x88, 0x08, 0x85, 0x42,
    0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x99, 0x99, 0x75, 0xC1,
    0x00, 0x00, 0x80, 0xBF, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x38, 0x0A, 0xF7, 0x3F,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x80, 0xBF, 0xE1, 0x7A, 0x5A, 0x41,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x5A, 0x96, 0x6E, 0xC1,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xEE, 0xFF, 0x7F, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xEE, 0xFF, 0x7F, 0xFF,
  };
  const BYTE FillOldVerts36[128] = {
    0xE4, 0x7A, 0xF4, 0x3F, 0xEA, 0xD1, 0xA3, 0x42, 0x57, 0x96, 0x6E, 0x41, 0x00, 0x00, 0x00, 0x3F,
    0xE4, 0x7A, 0xF4, 0x3F, 0x40, 0x27, 0x8A, 0x42, 0x57, 0x96, 0x6E, 0x41, 0x01, 0x00, 0x00, 0x3F,
    0xE4, 0x7A, 0xF4, 0x3F, 0xEA, 0xD1, 0xA3, 0x42, 0x00, 0xD4, 0x23, 0x3C, 0x02, 0x00, 0x00, 0x3F,
    0x40, 0xD6, 0x23, 0x3C, 0xEA, 0xD1, 0xA3, 0x42, 0x57, 0x96, 0x6E, 0x41, 0x03, 0x00, 0x00, 0x3F,
    0xE4, 0x7A, 0xF4, 0x3F, 0x40, 0x27, 0x8A, 0x42, 0x00, 0xD4, 0x23, 0x3C, 0x04, 0x00, 0x00, 0x3F,
    0x40, 0xD6, 0x23, 0x3C, 0xEA, 0xD1, 0xA3, 0x42, 0x00, 0xD4, 0x23, 0x3C, 0x05, 0x00, 0x00, 0x3F,
    0x40, 0xD6, 0x23, 0x3C, 0x40, 0x27, 0x8A, 0x42, 0x57, 0x96, 0x6E, 0x41, 0x06, 0x00, 0x00, 0x3F,
    0x40, 0xD6, 0x23, 0x3C, 0x40, 0x27, 0x8A, 0x42, 0x00, 0xD4, 0x23, 0x3C, 0x07, 0x00, 0x00, 0x3F,
  };
  const BYTE FillOldPlanes36[128] = {
    0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xE4, 0x7A, 0xF4, 0xBF,
    0x00, 0x00, 0x80, 0xBF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x40, 0xD6, 0x23, 0x3C,
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x7F, 0xBF, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x27, 0x8A, 0x42,
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x7F, 0x3F, 0x00, 0x00, 0x00, 0x00, 0xE9, 0xD1, 0xA3, 0xC2,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x80, 0xBF, 0x00, 0xD4, 0x23, 0x3C,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x57, 0x96, 0x6E, 0xC1,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xEE, 0xFF, 0x7F, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xEE, 0xFF, 0x7F, 0xFF,
  };
  const BYTE FillNewVerts36[128] = {
    0xE4, 0x7A, 0xF4, 0x3F, 0xEA, 0xD1, 0xA3, 0x42, 0x57, 0x96, 0x6E, 0x41, 0x00, 0x00, 0x00, 0x3F,
    0xE4, 0x7A, 0xF4, 0x3F, 0x5C, 0x0F, 0x8C, 0x42, 0x57, 0x96, 0x6E, 0x41, 0x01, 0x00, 0x00, 0x3F,
    0xE4, 0x7A, 0xF4, 0x3F, 0xEA, 0xD1, 0xA3, 0x42, 0x00, 0xD4, 0x23, 0x3C, 0x02, 0x00, 0x00, 0x3F,
    0x40, 0xD6, 0x23, 0x3C, 0xEA, 0xD1, 0xA3, 0x42, 0x57, 0x96, 0x6E, 0x41, 0x03, 0x00, 0x00, 0x3F,
    0xE4, 0x7A, 0xF4, 0x3F, 0x5C, 0x0F, 0x8C, 0x42, 0x00, 0xD4, 0x23, 0x3C, 0x04, 0x00, 0x00, 0x3F,
    0x40, 0xD6, 0x23, 0x3C, 0xEA, 0xD1, 0xA3, 0x42, 0x00, 0xD4, 0x23, 0x3C, 0x05, 0x00, 0x00, 0x3F,
    0x40, 0xD6, 0x23, 0x3C, 0x5C, 0x0F, 0x8C, 0x42, 0x57, 0x96, 0x6E, 0x41, 0x06, 0x00, 0x00, 0x3F,
    0x40, 0xD6, 0x23, 0x3C, 0x5C, 0x0F, 0x8C, 0x42, 0x00, 0xD4, 0x23, 0x3C, 0x07, 0x00, 0x00, 0x3F,
  };
  const BYTE FillNewPlanes36[128] = {
    0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xE4, 0x7A, 0xF4, 0xBF,
    0x00, 0x00, 0x80, 0xBF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x40, 0xD6, 0x23, 0x3C,
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x7F, 0xBF, 0x00, 0x00, 0x00, 0x80, 0x5C, 0x0F, 0x8C, 0x42,
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x7F, 0x3F, 0x00, 0x00, 0x00, 0x00, 0xEA, 0xD1, 0xA3, 0xC2,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x80, 0xBF, 0x00, 0xD4, 0x23, 0x3C,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x57, 0x96, 0x6E, 0xC1,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xEE, 0xFF, 0x7F, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xEE, 0xFF, 0x7F, 0xFF,
  };
  const BYTE* const FillOld[FillBlocks][2] = { { FillOldVerts0, FillOldPlanes0 }, { FillOldVerts37, FillOldPlanes37 },
    { FillOldVerts36, FillOldPlanes36 } };
  const BYTE* const FillNew[FillBlocks][2] = { { FillNewVerts0, FillNewPlanes0 }, { FillNewVerts37, FillNewPlanes37 },
    { FillNewVerts36, FillNewPlanes36 } };
  const ULONGLONG FillCheckMs = 3000;      // looked at again this often (a zone streamed in again comes as the file has it)

  // The kitchen's ceiling lamps (the user: "check if we can enable the lights inside the kitchen"): zone 397's member
  // objects/props/light/light_int_surface_05/light_int_surface_05 (no other mesh has the file name), 6 instances drawn
  // only (2.56 m strips facing down: 4 in the kitchen 3.48 m up, 2 in the back room 3.18 m up), every one placed with
  // the look light_int_surface_05_dark (InstanceObjectVariation 1542045776: DiffuseColor 0.032, near black), so
  // they're switched off. Given no variation (0x14376C4B0: name hash 0) they have the mesh's own look. Lamp 0's origin
  // as the data has it, checked against its in-memory transform (+ 0x08: 4 rows of 4 floats each) first.
  const char LampMesh[] = "light_int_surface_05_mesh";
  const int LampCount = 6;
  const float Lamp0Origin[3] = { 173.6709f, 69.7198f, 311.0293f };
  const float LampPlaceEpsilon = 0.02f;
  // Their light: a spot light 15 cm under each lamp's strip, shining straight down (the Lights session's fixed lights,
  // not in its menu: BuildLight's 80 degree cone with its inner angle left 0, so it fades out all the way from its
  // middle), at the game's own strip lamp colour (pfl_light_int_surface_01: 7000 K), 1,350 lm each (the user, 11-03:
  // "increase the lights strength to 1,350lm"; before it 1,000 lm, the user's "too bright ... spotlights facing down at
  // the ground with 1k lm with a smooth radius"; the first build's spheres were 800 - 2,500 lm). The panels' glow
  // (DogenGlow.cpp) has its own value. The cone's edge meets the floor 4.3 m away in the kitchen (3.3 m under it),
  // 3.9 m in the back room (3.0 m); the reach, 6 m and 4.8 m, is past that and short of the building's outside: a ray
  // of the cone (40 degrees off straight down) gets a distance h sideways only after h / sin 40 = 1.56 h, and the
  // nearest outside planes are 4.09 m (the street front) and 3.14 m (the back room's back). Made while Faith is within
  // LampLightNear of the door, taken away past LampLightFar.
  struct LampLight
  {
    float Place[3];
    float Lumens;
    float Reach;
  };
  const LampLight LampLights[LampCount] = {
    { { 172.607f, 69.49f, 310.318f }, 1350.f, 4.8f },   // the back room
    { { 174.074f, 69.79f, 307.546f }, 1350.f, 6.f },
    { { 180.964f, 69.79f, 306.666f }, 1350.f, 6.f },
    { { 176.607f, 69.79f, 303.755f }, 1350.f, 6.f },
    { { 175.799f, 69.49f, 312.452f }, 1350.f, 4.8f },   // the back room
    { { 178.431f, 69.79f, 310.457f }, 1350.f, 6.f } };
  const int LampTemperature = 7000;
  const float LampLightNear = 40.f, LampLightFar = 60.f;
  // Dimmer at night (the user, 2026-11-08: "Lights should be getting dimmer during night time in dogans apparment"):
  // their light and their panels' glow LampNightShare of the day's from LampNightFrom to LampDawnFrom, the day's from
  // LampDayFrom to LampDimFrom, eased in between, by the game's clock (seconds, as the tools' time of day menu reads it:
  // VisualsManager::GetCurrentTimeOfDay; not known, the day's). The hours are the game's sun's: its time of day logic
  // (pf_tod_sunmooncelectialandoutdoorlightlogic) has the outdoor light 0.6 from 7:00 to 17:00, 0 before 6:15 and
  // after 17:45 (Research_Weather_2026-10-25.md). Read every LampDimMs while they're lit; their lights given a new
  // intensity when it moved LampDimStep of the day's or more, or reached the night's or the day's.
  const float LampNightShare = 0.4f;
  const float LampDimFrom = 17.f, LampNightFrom = 17.75f, LampDawnFrom = 6.25f, LampDayFrom = 7.f;   // hours
  const ULONGLONG LampDimMs = 2000;
  const float LampDimStep = 0.02f;
  // The draw object: [component + 0x68][part] (slot 66, 0x1434DC6D0); its rows at + 0x10, the renderer's handle at
  // + 0x8C (the Visible handler 0x14318F730: its part, its handle, the description 0x143769330 makes, the handle kept)
  const __int64 PartObjectRva = 0x34DC6D0;
  const int PartObjectSlot = 66;
  const int ComponentPartObjectsOffset = 0x68;
  const __int64 VisibleRva = 0x318F730;
  const __int64 DescribeRva = 0x3769330;
  const int ObjectRowsOffset = 0x10;
  const int ObjectHandleOffset = 0x8C;

  struct Door
  {
    __int64 Group;
    __int64 Data;
    int Members;          // the group's member count
    int Member;           // the door's
    int Anchor;           // the other member's (kitchen01's), -1 none
    __int64 Object;       // its draw object, once its rows were found as the data has them
    float Rows[16];       // those rows as the game had them
    bool Bad;             // its draw object's rows weren't the data's: left alone
  };
  // Each door's state, by Doors
  struct DoorState
  {
    Door Found;
    bool Open;            // wanted open
    bool Shut;            // shut as the game has it, last read
    float Swing;          // 0 shut, 1 open (its rows as drawn)
    ULONGLONG LastTick;
    ULONGLONG AwaySince;
    ULONGLONG LastSearch;
    ULONGLONG LastSwitch;
    int OpenFailures, ShutFailures;
    bool NotFoundLogged;
    bool ShowFailLogged;
    bool CantDraw;        // it couldn't be drawn swung: it stays shut and solid
    bool PartsUnknownLogged;
    bool ObjectLogged;
  };
  DoorState g_Doors[DoorCount] = {};
  int g_Code = 0;         // 0 not checked, 1 the code the tools know, -1 not
  // The lamps: the group and data they were given their own look in; their lights (ids, 0 none)
  __int64 g_LampsGroup = 0, g_LampsData = 0;
  bool g_LampsLogged = false;
  bool g_LampsLit = false;    // the look was given (the lamps' member found)
  int g_LampLights[LampCount] = {};
  // Their share of the day's brightness: as last read from the clock (the clock then, and when), as their lights were
  // given it, as last logged
  float g_LampShare = 1.f, g_LampClock = -1.f, g_LampsGiven = 1.f, g_LampShareLogged = -1.f;
  ULONGLONG g_LampShareAt = 0;
  // The building's fills
  __int64 g_FillGroup = 0, g_FillData = 0;
  int g_FillMember = -1;
  ULONGLONG g_LastFill = 0;
  bool g_FillLogged = false;

  // The game's code the door is switched with, as read offline (2026-11-03, the decrypted image)
  bool CodeKnown()
  {
    BYTE const* base = (BYTE const*)g_gameHandle;
    IMAGE_DOS_HEADER const* pDos = (IMAGE_DOS_HEADER const*)base;
    IMAGE_NT_HEADERS const* pNt = (IMAGE_NT_HEADERS const*)(base + pDos->e_lfanew);
    if (pNt->FileHeader.TimeDateStamp != GameTimeStamp)
      return false;
    // The component's part switches: for each of its bodies (movzx ebp, word [rcx + 0x60]; ids [rdi + 0x30]),
    // its world (call 0x1434EE790 with [rdi + 0x58]), then the body's part (call 0x1434EC740 / 0x1434EC8C0)
    static const BYTE switchHead[] = { 0x48, 0x89, 0x6C, 0x24, 0x20, 0x56, 0x57, 0x41, 0x56, 0x48, 0x83, 0xEC, 0x20, 0x0F,
      0xB7, 0x69, 0x60, 0x31, 0xF6, 0x41, 0x89, 0xD6, 0x48, 0x89, 0xCF, 0x85, 0xED, 0x74, 0x4C, 0x48, 0x89, 0x5C, 0x24, 0x48,
      0x48, 0x8B, 0x47, 0x30, 0x48, 0x8D, 0x1C, 0xB5, 0x00, 0x00, 0x00, 0x00, 0x48, 0x8D, 0x54, 0x24, 0x40, 0x8B, 0x0C, 0x03,
      0x89, 0x4C, 0x24, 0x40, 0x48, 0x8B, 0x4F, 0x58 };
    static const BYTE switchMiddle[] = { 0x48, 0x8B, 0x4F, 0x30, 0x45, 0x89, 0xF0, 0x8B, 0x14, 0x0B, 0x48, 0x89, 0xC1, 0x89,
      0x54, 0x24, 0x50, 0x48, 0x8D, 0x54, 0x24, 0x50 };
    // A body's part: movzx edx, word [rdx]; mov rax, [rcx + 0x130]; mov rcx, [rax + rdx * 8]; mov edx, r8d;
    // mov rax, [rcx]; jmp [rax + 0x290] (off) / [rax + 0x288] (on)
    static const BYTE bodyPart[] = { 0x0F, 0xB7, 0x12, 0x48, 0x8B, 0x81, 0x30, 0x01, 0x00, 0x00, 0x48, 0x8B, 0x0C, 0xD0, 0x44,
      0x89, 0xC2, 0x48, 0x8B, 0x01, 0xFF, 0xA0 };
    // A body's world: mov eax, [rdx]; shr eax, 0x10; movzx edx, al; mov rax, [rcx + 0xF0]; mov rax, [rax + rdx * 8]; ret
    static const BYTE bodyWorld[] = { 0x8B, 0x02, 0xC1, 0xE8, 0x10, 0x0F, 0xB6, 0xD0, 0x48, 0x8B, 0x81, 0xF0, 0x00, 0x00, 0x00,
      0x48, 0x8B, 0x04, 0xD0, 0xC3 };
    // The record's part bit: lea r8, [rcx + 0xB8]; ...; mov eax, [r8 + 8]; shr edx, 3; and r10d, 7; sub rax, 0x40; and
    // the select of the bits' place (inline when 64 or fewer, else [r8])
    static const BYTE recordBit[] = { 0x53, 0x48, 0x83, 0xEC, 0x20, 0x4C, 0x8D, 0x81, 0xB8, 0x00, 0x00, 0x00, 0x48, 0x89, 0xCB,
      0x41, 0x89, 0xD2, 0x41, 0x8B, 0x40, 0x08, 0xC1, 0xEA, 0x03, 0x41, 0x83, 0xE2, 0x07, 0x48, 0x83, 0xE8, 0x40, 0x41, 0x0F,
      0xB6, 0xCA, 0x49, 0x89, 0xC1, 0x49, 0xF7, 0xD9, 0x49, 0x09, 0xC1, 0x48, 0x83, 0xC8, 0x01, 0x48, 0xF7, 0xD8, 0x49, 0xC1,
      0xF9, 0x3F, 0x48, 0xC1, 0xF8, 0x3F, 0x49, 0x21, 0xC1, 0x49, 0x8B, 0x00, 0x4C, 0x29, 0xC0, 0x49, 0x21, 0xC1 };
    auto callsTo = [&](__int64 at, __int64 target) {
      return base[at] == 0xE8 && at + 5 + *(int const*)(base + at + 1) == target;
    };
    for (__int64 rva : { PartOffRva, PartOnRva })
    {
      if (memcmp(base + rva, switchHead, sizeof(switchHead)) || !callsTo(rva + 62, BodyWorldRva) ||
        memcmp(base + rva + 67, switchMiddle, sizeof(switchMiddle)))
        return false;
    }
    if (!callsTo(PartOffRva + 89, BodyPartOffRva) || !callsTo(PartOnRva + 89, BodyPartOnRva))
      return false;
    if (memcmp(base + BodyPartOffRva, bodyPart, sizeof(bodyPart)) || *(int const*)(base + BodyPartOffRva + 22) != RecordPartOffSlot * 8 ||
      memcmp(base + BodyPartOnRva, bodyPart, sizeof(bodyPart)) || *(int const*)(base + BodyPartOnRva + 22) != RecordPartOnSlot * 8 ||
      memcmp(base + BodyWorldRva, bodyWorld, sizeof(bodyWorld)) ||
      memcmp(base + RecordPartOffRva, recordBit, sizeof(recordBit)) || memcmp(base + RecordPartOnRva, recordBit, sizeof(recordBit)))
      return false;
    // The part's draw object: mov rax, [rcx + 0x68]; mov edx, edx; mov rax, [rax + rdx * 8]; ret
    static const BYTE partObject[] = { 0x48, 0x8B, 0x41, 0x68, 0x89, 0xD2, 0x48, 0x8B, 0x04, 0xD0, 0xC3 };
    // The description: the object's rows (+ 0x10 .. + 0x50) first, as they are
    static const BYTE describe[] = { 0x0F, 0x28, 0x41, 0x10, 0x49, 0x89, 0xD0, 0x0F, 0x29, 0x02, 0x0F, 0x28, 0x49, 0x20, 0x0F,
      0x29, 0x4A, 0x10, 0x0F, 0x28, 0x41, 0x30, 0x0F, 0x29, 0x42, 0x20, 0x0F, 0x28, 0x49, 0x40, 0x0F, 0x29, 0x4A, 0x30 };
    // The Visible handler: a member placed by its physics gets the component's object for its first part (imul edx,
    // [rax + 0x50]; add edx, [rax + 0x48]; call [r8 + 0x210]), reads its handle (mov r15d, [rbx + 0x8C]), describes it
    // (call 0x143769330) and keeps the handle the renderer gives back (mov [rbx + 0x8C], esi)
    static const BYTE visiblePart[] = { 0x0F, 0xAF, 0x50, 0x50, 0x03, 0x50, 0x48, 0x41, 0xFF, 0x90, 0x10, 0x02, 0x00, 0x00 };
    static const BYTE visibleHandle[] = { 0x44, 0x8B, 0xBB, 0x8C, 0x00, 0x00, 0x00 };
    static const BYTE visibleKeep[] = { 0x89, 0xB3, 0x8C, 0x00, 0x00, 0x00 };
    if (memcmp(base + PartObjectRva, partObject, sizeof(partObject)) || memcmp(base + DescribeRva, describe, sizeof(describe)) ||
      memcmp(base + VisibleRva + 0x45, visiblePart, sizeof(visiblePart)) ||
      memcmp(base + VisibleRva + 0xE1, visibleHandle, sizeof(visibleHandle)) || !callsTo(VisibleRva + 0x109, DescribeRva) ||
      memcmp(base + VisibleRva + 0x23C, visibleKeep, sizeof(visibleKeep)))
      return false;
    // The static record: its slots jump to the part bit functions (mov eax, [rcx + 0x70]; lea r9, [rcx + 0x68];
    // mov r10d, edx ...), which end giving the body's world and id (+ 0x30, + 0x50) to 0x143F3CD00
    static const BYTE staticBits[] = { 0x8B, 0x41, 0x70, 0x4C, 0x8D, 0x49, 0x68, 0x41, 0x89, 0xD2 };
    static const BYTE staticTail[] = { 0x49, 0x8B, 0x4B, 0x30, 0x41, 0x8B, 0x53, 0x50, 0x48, 0x8B, 0x89, 0xE0, 0x00, 0x00, 0x00, 0xE9 };
    // 0x143F3CD00: mov [rsp + 8], rbx; mov [rsp + 0x10], rbp; mov [rsp + 0x18], rsi; push rdi; sub rsp, 0x20;
    // movsxd rbx, edx; mov r8, rcx
    static const BYTE bodyChanged[] = { 0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x6C, 0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18,
      0x57, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x63, 0xDA, 0x49, 0x89, 0xC8 };
    auto jumpsTo = [&](__int64 at, __int64 target) {
      return base[at] == 0xE9 && at + 5 + *(int const*)(base + at + 1) == target;
    };
    if (!jumpsTo(StaticRecordPartOffRva, StaticPartOffRva) || !jumpsTo(StaticRecordPartOnRva, StaticPartOnRva) ||
      memcmp(base + StaticPartOffRva, staticBits, sizeof(staticBits)) || memcmp(base + StaticPartOnRva, staticBits, sizeof(staticBits)) ||
      memcmp(base + StaticPartOffRva + 0xA2, staticTail, sizeof(staticTail)) || memcmp(base + StaticPartOnRva + 0xA0, staticTail, sizeof(staticTail)) ||
      base + StaticPartOffRva + 0xA2 + 0x14 + *(int const*)(base + StaticPartOffRva + 0xA2 + 0x10) != base + BodyChangedRva ||
      base + StaticPartOnRva + 0xA0 + 0x14 + *(int const*)(base + StaticPartOnRva + 0xA0 + 0x10) != base + BodyChangedRva ||
      memcmp(base + BodyChangedRva, bodyChanged, sizeof(bodyChanged)))
      return false;
    __int64 const* componentVTable = (__int64 const*)(base + PhysicsComponentVTableRva);
    return componentVTable[PartOnSlot] == (__int64)base + PartOnRva && componentVTable[PartOffSlot] == (__int64)base + PartOffRva &&
      componentVTable[PartObjectSlot] == (__int64)base + PartObjectRva;
  }

  // Whether a mesh's name (its last part, any case) is this one. Game memory: only inside __try.
  bool NameIs(const char* pName, const char* wanted)
  {
    if ((__int64)pName < 0x10000)
      return false;
    const char* pLast = pName;
    for (size_t i = 0; pName[i] && i < 512; ++i)
      if (pName[i] == '/') pLast = pName + i + 1;
    size_t i = 0;
    for (; wanted[i]; ++i)
      if (tolower((unsigned char)pLast[i]) != wanted[i])
        return false;
    return pLast[i] == 0;
  }

  // A member's data, if the group is a model group with that many members. Game memory: only inside __try.
  __int64 MemberData(__int64 group, int k, __int64& data, int& count)
  {
    __int64 base = (__int64)g_gameHandle;
    if (*(__int64 const*)group != base + ModelGroupVTableRva)
      return 0;
    data = *(__int64 const*)(group + GroupDataOffset);
    count = *(int const*)(group + GroupMemberCountOffset);
    if (data < 0x10000 || count <= 0 || count > MaxGroupMembers || k < 0 || k >= count)
      return 0;
    __int64 members = *(__int64 const*)(data + DataMembersOffset);
    return members < 0x10000 ? 0 : members + (__int64)k * MemberDataSize;
  }

  const char* MeshName(__int64 member)
  {
    __int64 mesh = *(__int64 const*)(member + MemberMeshOffset);
    return mesh < 0x10000 ? nullptr : *(const char* const*)(mesh + AssetNameOffset);
  }

  // A member as the data has it: its instance count, its parts from first, placed by its physics, the mesh. The
  // numbers first (a search reads every member of every group). Game memory: only inside __try.
  bool IsMember(__int64 member, int instances, unsigned int first, unsigned int parts, const char* mesh)
  {
    if (*(int const*)(member + MemberInstancesOffset) != instances ||
      *(unsigned int const*)(member + MemberFirstPartOffset) != first || *(unsigned int const*)(member + MemberPartsOffset) != parts)
      return false;
    __int64 transforms = *(__int64 const*)(member + MemberTransformsOffset);
    if (transforms >= 0x10000 && *(int const*)(transforms - 4) != 0)
      return false;
    return NameIs(MeshName(member), mesh);
  }
  bool IsDoorMember(DoorPlace const& place, __int64 member)
  {
    return IsMember(member, place.Instances, place.FirstPart, place.Parts, place.Mesh);
  }
  bool IsAnchorMember(DoorPlace const& place, __int64 member)
  {
    return IsMember(member, 1, place.AnchorPart, 1, place.AnchorMesh);
  }

  // The door's group: the door in it, and its other member if it has one. Game memory: only inside __try.
  bool ReadDoorGroup(DoorPlace const& place, __int64 group, Door& out)
  {
    __int64 data = 0;
    int count = 0;
    if (!MemberData(group, 0, data, count))
      return false;
    int door = -1, anchor = -1;
    bool anchorWanted = place.AnchorMesh != nullptr;
    for (int k = 0; k < count && (door < 0 || (anchorWanted && anchor < 0)); ++k)
    {
      __int64 member = MemberData(group, k, data, count);
      if (!member)
        return false;
      if (anchorWanted && anchor < 0 && IsAnchorMember(place, member))
        anchor = k;
      else if (door < 0 && IsDoorMember(place, member))
        door = k;
    }
    if (door < 0 || (anchorWanted && anchor < 0))
      return false;
    out = {};
    out.Group = group;
    out.Data = data;
    out.Members = count;
    out.Member = door;
    out.Anchor = anchor;
    return true;
  }

  // The zone's group, from the model groups' instance lists
  bool FindDoor(DoorPlace const& place, Door& out)
  {
    __int64 base = (__int64)g_gameHandle;
    __try
    {
      for (int list = 0; list < 4; ++list)
      {
        __int64 link = *(__int64 const*)(base + ModelGroupClassRva + ClassInstanceListsOffset + list * 0x10);
        for (int n = 0; link >= 0x10000 && n < MaxGroupsWalked; ++n)
        {
          if (ReadDoorGroup(place, link - EntityLinkOffset, out))
            return true;
          link = *(__int64 const*)link;
        }
      }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
    return false;
  }

  // Still that group: the same data and members, the door and its other member where they were
  bool StillDoor(DoorPlace const& place, Door const& door)
  {
    __try
    {
      __int64 data = 0;
      int count = 0;
      __int64 member = MemberData(door.Group, door.Member, data, count);
      if (!member || data != door.Data || count != door.Members || !IsDoorMember(place, member))
        return false;
      if (!place.AnchorMesh)
        return true;
      __int64 anchor = MemberData(door.Group, door.Anchor, data, count);
      return anchor && IsAnchorMember(place, anchor);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
  }

  // The door's instance's first physics part
  unsigned int DoorPart(DoorPlace const& place)
  {
    return place.FirstPart + (unsigned int)place.Instance * place.Parts;
  }

  // The member's physics component, if its record is made and is the member's; 0 if not
  __int64 DoorComponent(Door const& door)
  {
    __int64 base = (__int64)g_gameHandle;
    __try
    {
      __int64 records = *(__int64 const*)(door.Group + GroupRecordsOffset);
      if (records < 0x10000)
        return 0;
      __int64 record = records + (__int64)door.Member * RecordSize;
      __int64 member = *(__int64 const*)(door.Data + DataMembersOffset) + (__int64)door.Member * MemberDataSize;
      if (!*(BYTE const*)(record + RecordMadeOffset) || *(__int64 const*)(record + RecordDataOffset) != member)
        return 0;
      __int64 component = *(__int64 const*)(record + RecordPhysicsOffset);
      if (component < 0x10000 || *(__int64 const*)component != base + PhysicsComponentVTableRva)
        return 0;
      return component;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return 0;
    }
  }

  // The door's parts on the component's bodies: how many bodies, how many with all its parts off, all on. False if
  // a body's record isn't one the tools know.
  bool ReadParts(DoorPlace const& place, __int64 component, int& bodies, int& allOff, int& allOn)
  {
    unsigned int first = DoorPart(place);
    __int64 base = (__int64)g_gameHandle;
    bodies = allOff = allOn = 0;
    __try
    {
      int count = *(unsigned short const*)(component + ComponentBodyCountOffset);
      __int64 ids = *(__int64 const*)(component + ComponentBodiesOffset);
      __int64 manager = *(__int64 const*)(component + ComponentManagerOffset);
      if (count <= 0 || count > MaxBodies || ids < 0x10000 || manager < 0x10000)
        return false;
      for (int b = 0; b < count; ++b)
      {
        unsigned int id = *(unsigned int const*)(ids + b * 4);
        __int64 worlds = *(__int64 const*)(manager + ManagerWorldsOffset);
        __int64 world = worlds >= 0x10000 ? *(__int64 const*)(worlds + ((id >> 16) & 0xFF) * 8) : 0;
        __int64 records = world >= 0x10000 ? *(__int64 const*)(world + WorldRecordsOffset) : 0;
        __int64 record = records >= 0x10000 ? *(__int64 const*)(records + (id & 0xFFFF) * 8) : 0;
        if (record < 0x10000)
          return false;
        __int64 const* vtable = *(__int64 const* const*)record;
        int bitsAt, countAt;
        if (vtable[RecordPartOnSlot] == base + StaticRecordPartOnRva && vtable[RecordPartOffSlot] == base + StaticRecordPartOffRva)
          bitsAt = StaticRecordPartBitsOffset, countAt = StaticRecordPartCountOffset;
        else if (vtable[RecordPartOnSlot] == base + RecordPartOnRva && vtable[RecordPartOffSlot] == base + RecordPartOffRva)
          bitsAt = RecordPartBitsOffset, countAt = RecordPartCountOffset;
        else
          return false;
        unsigned int parts = *(unsigned int const*)(record + countAt);
        if (parts < first + place.Parts)
          return false;
        BYTE const* bits = parts > 64 ? *(BYTE const* const*)(record + bitsAt) : (BYTE const*)(record + bitsAt);
        if ((__int64)bits < 0x10000)
          return false;
        int on = 0;
        for (unsigned int p = first; p < first + place.Parts; ++p)
          on += (bits[p >> 3] >> (p & 7)) & 1;
        ++bodies;
        allOff += on == 0;
        allOn += on == (int)place.Parts;
      }
      return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
  }

  // Its parts off or on, on all its bodies, as the component's own slots do it
  bool SwitchParts(DoorPlace const& place, __int64 component, bool on)
  {
    __int64 base = (__int64)g_gameHandle;
    __try
    {
      tPartSwitch partSwitch = (tPartSwitch)(base + (on ? PartOnRva : PartOffRva));
      for (unsigned int p = DoorPart(place); p < DoorPart(place) + place.Parts; ++p)
        partSwitch(component, p);
      return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
  }

  // The lamps' member in the door's group: its mesh, its 6 instances placed by data (6 transforms), lamp 0 where the
  // data puts it. -1 if not. Game memory: only inside __try.
  int LampMember(Door const& door)
  {
    __int64 data = 0;
    int count = 0;
    for (int k = 0; MemberData(door.Group, k, data, count) && k < count; ++k)
    {
      __int64 member = MemberData(door.Group, k, data, count);
      if (*(int const*)(member + MemberInstancesOffset) != LampCount || !NameIs(MeshName(member), LampMesh))
        continue;
      __int64 transforms = *(__int64 const*)(member + MemberTransformsOffset);
      if (transforms < 0x10000 || *(int const*)(transforms - 4) != LampCount)
        return -1;
      float const* origin = (float const*)(transforms + 12 * sizeof(float));
      for (int c = 0; c < 3; ++c)
        if (fabsf(origin[c] - Lamp0Origin[c]) > LampPlaceEpsilon)
          return -1;
      return k;
    }
    return -1;
  }

  // The lamps given their own look, once a group (the zone streamed in again comes with the dark look)
  void UpdateLamps(Door const& door)
  {
    if (door.Group == g_LampsGroup && door.Data == g_LampsData)
      return;
    int member = -1;
    __try
    {
      member = LampMember(door);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      member = -1;
    }
    g_LampsGroup = door.Group;
    g_LampsData = door.Data;
    int given = 0;
    if (member >= 0)
      for (int i = 0; i < LampCount; ++i)
        given += util::hooks::SetModelGroupMemberVariation(door.Group, member, i, 0) ? 1 : 0;
    g_LampsLit = given == LampCount;
    if (!g_LampsLogged || given < LampCount)
    {
      g_LampsLogged = true;
      util::log::Write("Dogen's kitchen: %s (%d of its %d ceiling lamps given their own look)", member < 0 ?
        "its ceiling lamps aren't where the data has them" : "the ceiling lamps' dark look taken off", given, LampCount);
    }
  }

  // The lamps' share of their day's brightness at this time of day (seconds, 0 midnight; anything else the day's)
  float LampShare(float clock)
  {
    if (!(clock >= 0.f && clock <= 86400.f))
      return 1.f;
    float h = clock / 3600.f, t;
    if (h < LampDawnFrom || h >= LampNightFrom)
      t = 0.f;
    else if (h < LampDayFrom)
      t = (h - LampDawnFrom) / (LampDayFrom - LampDawnFrom);
    else if (h < LampDimFrom)
      t = 1.f;
    else
      t = 1.f - (h - LampDimFrom) / (LampNightFrom - LampDimFrom);
    t = t * t * (3.f - 2.f * t);
    return LampNightShare + (1.f - LampNightShare) * t;
  }

  // Logged as it reaches the night's or the day's, or moves a tenth of the day's
  void LogLampShare(float share, float clock, char const* what)
  {
    if (g_LampShareLogged >= 0.f && fabsf(share - g_LampShareLogged) < 0.1f &&
      !(share != g_LampShareLogged && (share <= LampNightShare || share >= 1.f)))
      return;
    g_LampShareLogged = share;
    if (!(clock >= 0.f && clock <= 86400.f))
    {
      util::log::Write("Dogen's kitchen: the ceiling lamps %s at %.0f %% of the day's (%.0f lm; the game's clock not known)",
        what, 100.f * share, LampLights[0].Lumens * share);
      return;
    }
    int minutes = (int)(clock / 60.f);
    util::log::Write("Dogen's kitchen: the ceiling lamps %s at %.0f %% of the day's (%.0f lm; the game's clock %02d:%02d)",
      what, 100.f * share, LampLights[0].Lumens * share, minutes / 60 % 24, minutes % 60);
  }

  // The lamps' lights: made while Faith is near and the lamps have their own look, taken away when she's far or
  // the zone's group went; dimmer at night
  void UpdateLampLights(bool wanted)
  {
    // (their panels' glow with them, the game's way: DogenGlow.cpp; dimmed with them)
    dogenglow::SetWanted(wanted);
    ULONGLONG now = GetTickCount64();
    if (wanted && (!g_LampShareAt || now - g_LampShareAt >= LampDimMs))
    {
      g_LampShareAt = now;
      VisualsManager* visuals = g_mainHandle ? g_mainHandle->GetVisualsManager() : nullptr;
      g_LampClock = visuals ? visuals->GetCurrentTimeOfDay() : -1.f;
      g_LampShare = LampShare(g_LampClock);
      dogenglow::SetShare(g_LampShare);
    }
    LightManager* lights = g_mainHandle ? g_mainHandle->GetLightManager() : nullptr;
    if (!lights)
      return;
    bool any = false;
    for (int id : g_LampLights)
      any |= id != 0;
    if (wanted && any)
    {
      // Dimmed or brightened with the clock
      float share = g_LampShare;
      if (share == g_LampsGiven || (fabsf(share - g_LampsGiven) < LampDimStep && share > LampNightShare && share < 1.f))
        return;
      int set = 0, lit = 0;
      for (int i = 0; i < LampCount; ++i)
        if (g_LampLights[i])
        {
          ++lit;
          set += lights->SetFixedLightIntensity(g_LampLights[i], LampLights[i].Lumens * share) ? 1 : 0;
        }
      g_LampsGiven = share;
      if (set < lit)
        util::log::Warning("Dogen's kitchen: %d of the %d ceiling lamps' lights couldn't be dimmed", lit - set, lit);
      LogLampShare(share, g_LampClock, "dimmed");
      return;
    }
    if (wanted == any)
      return;
    if (!wanted)
    {
      for (int& id : g_LampLights)
      {
        if (id)
          lights->RemoveFixedLight(id);
        id = 0;
      }
      util::log::Write("Dogen's kitchen: the ceiling lamps' lights taken away");
      return;
    }
    // (none made, e.g. the light manager off: tried again only this often)
    static ULONGLONG s_lastTry = 0;
    if (now - s_lastTry < 5000)
      return;
    s_lastTry = now;
    int made = 0;
    for (int i = 0; i < LampCount; ++i)
    {
      // rows right, up, backward, position: a spot shines along -backward, so backward is up (0, 1, 0) and up is
      // (0, 0, -1) (right x up = backward)
      float rows[16] = { 1.f, 0.f, 0.f, 0.f, 0.f, 0.f, -1.f, 0.f, 0.f, 1.f, 0.f, 0.f,
        LampLights[i].Place[0], LampLights[i].Place[1], LampLights[i].Place[2], 1.f };
      g_LampLights[i] = lights->AddFixedLight(LightManager::Spot, rows, nullptr, LampTemperature,
        LampLights[i].Lumens * g_LampShare, LampLights[i].Reach);
      made += g_LampLights[i] != 0;
    }
    g_LampsGiven = g_LampShare;
    util::log::Write("Dogen's kitchen: %d of the %d ceiling lamps' lights made", made, LampCount);
    g_LampShareLogged = -1.f;
    LogLampShare(g_LampShare, g_LampClock, "made");
  }

  // A body's record from the component's id (0x1434EE790, then [world + 0x130]); 0 if not
  __int64 BodyRecord(__int64 component, int b)
  {
    unsigned int id = *(unsigned int const*)(*(__int64 const*)(component + ComponentBodiesOffset) + b * 4);
    __int64 manager = *(__int64 const*)(component + ComponentManagerOffset);
    __int64 worlds = manager >= 0x10000 ? *(__int64 const*)(manager + ManagerWorldsOffset) : 0;
    __int64 world = worlds >= 0x10000 ? *(__int64 const*)(worlds + ((id >> 16) & 0xFF) * 8) : 0;
    __int64 records = world >= 0x10000 ? *(__int64 const*)(world + WorldRecordsOffset) : 0;
    __int64 record = records >= 0x10000 ? *(__int64 const*)(records + (id & 0xFFFF) * 8) : 0;
    return record >= 0x10000 ? record : 0;
  }

  // A record's Havok body: its hknpWorld, id and shape, if the body is the record's. Game memory: only inside __try.
  bool HavokBody(__int64 record, __int64& havok, unsigned int& id, __int64& shape)
  {
    __int64 base = (__int64)g_gameHandle;
    __int64 const* vtable = *(__int64 const* const*)record;
    int idAt;
    if (vtable[RecordPartOffSlot] == base + StaticRecordPartOffRva)
      idAt = StaticRecordHavokIdOffset;
    else if (vtable[RecordPartOffSlot] == base + RecordPartOffRva)
      idAt = RecordHavokIdOffset;
    else
      return false;
    __int64 world = *(__int64 const*)(record + RecordWorldOffset);
    havok = world >= 0x10000 ? *(__int64 const*)(world + WorldHavokOffset) : 0;
    id = *(unsigned int const*)(record + idAt);
    if (havok < 0x10000 || id >= (*(unsigned int const*)(havok + HavokCapacityOffset) & 0x3FFFFFFF))
      return false;
    __int64 body = *(__int64 const*)(havok + HavokBodiesOffset) + (__int64)id * HavokBodySize;
    shape = *(__int64 const*)(body + BodyShapeOffset);
    return shape >= 0x10000 && *(unsigned int const*)(body + BodyIdOffset) == id;
  }

  // The fill blocks at a root shape, by the first packfile's places or the second's: 1 they're as the file has
  // them, 2 already as the tools make them, 0 neither (the root isn't the zone's, or they were changed by something
  // else). Its place in out. Game memory: only inside __try.
  int FillsAt(__int64 root, __int64 (&out)[FillBlocks])
  {
    for (int pf = 0; pf < 2; ++pf)
    {
      bool old = true, made = true;
      for (int k = 0; k < FillBlocks && (old || made); ++k)
      {
        __int64 block = root + FillFromRoot[pf][k];
        unsigned short const* vh = (unsigned short const*)(block + FillVertsHeader);
        unsigned short const* ph = (unsigned short const*)(block + FillPlanesHeader);
        if (vh[0] != FillVertsCount || vh[1] != FillVertsRelative || ph[0] != FillPlanesCount || ph[1] != FillPlanesRelative)
        {
          old = made = false;
          break;
        }
        old &= !memcmp((void const*)(block + FillVertsOffset), FillOld[k][0], 128) &&
          !memcmp((void const*)(block + FillPlanesOffset), FillOld[k][1], 128);
        made &= !memcmp((void const*)(block + FillVertsOffset), FillNew[k][0], 128) &&
          !memcmp((void const*)(block + FillPlanesOffset), FillNew[k][1], 128);
        out[k] = block;
      }
      if (old)
        return 1;
      if (made)
        return 2;
    }
    return 0;
  }

  // The building's group member: one instance, its one part 610, the mesh
  bool IsBuildingMember(__int64 member) { return IsMember(member, 1, BuildingPart, 1, BuildingMesh); }

  bool FindBuilding(__int64& group, __int64& data, int& member)
  {
    __int64 base = (__int64)g_gameHandle;
    __try
    {
      for (int list = 0; list < 4; ++list)
      {
        __int64 link = *(__int64 const*)(base + ModelGroupClassRva + ClassInstanceListsOffset + list * 0x10);
        for (int n = 0; link >= 0x10000 && n < MaxGroupsWalked; ++n)
        {
          __int64 g = link - EntityLinkOffset, d = 0;
          int count = 0;
          if (MemberData(g, 0, d, count))
            for (int k = 0; k < count; ++k)
            {
              __int64 m = MemberData(g, k, d, count);
              if (m && IsBuildingMember(m))
              {
                group = g, data = d, member = k;
                return true;
              }
            }
          link = *(__int64 const*)link;
        }
      }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
    return false;
  }

  // The building's fill blocks, on each of its group's bodies: made the slab, the cube and the raised mass over the
  // corridor once found as the file has them. Looked at every FillCheckMs while Faith is near.
  void UpdateFills(float distance, ULONGLONG now)
  {
    if (distance > SearchDistance || now - g_LastFill < FillCheckMs)
      return;
    g_LastFill = now;
    __try
    {
      __int64 data = 0;
      int count = 0;
      __int64 member = g_FillGroup ? MemberData(g_FillGroup, g_FillMember, data, count) : 0;
      if (!member || data != g_FillData || !IsBuildingMember(member))
      {
        g_FillGroup = 0;
        if (!FindBuilding(g_FillGroup, g_FillData, g_FillMember))
        {
          g_FillGroup = 0;
          return;
        }
      }
      Door building = {};
      building.Group = g_FillGroup;
      building.Data = g_FillData;
      building.Member = g_FillMember;
      __int64 component = DoorComponent(building);
      if (!component)
        return;
      int bodies = *(unsigned short const*)(component + ComponentBodyCountOffset);
      if (bodies <= 0 || bodies > MaxBodies)
        return;
      int made = 0, already = 0, unknown = 0;
      __int64 shapes[MaxBodies] = {};
      bool madeNow[MaxBodies] = {};
      for (int b = 0; b < bodies; ++b)
      {
        __int64 record = BodyRecord(component, b), havok = 0, shape = 0;
        unsigned int id = 0;
        if (!record || !HavokBody(record, havok, id, shape))
        {
          ++unknown;
          continue;
        }
        // (a body with a shape an earlier one has: its caches renewed too if that one's blocks changed now)
        int earlier = -1;
        for (int k = 0; k < b && earlier < 0; ++k)
          if (shapes[k] == shape) earlier = k;
        shapes[b] = shape;
        __int64 blocks[FillBlocks] = {};
        int state = earlier >= 0 ? (madeNow[earlier] ? 3 : 2) : FillsAt(shape, blocks);
        if (state == 1)
        {
          for (int k = 0; k < FillBlocks; ++k)
          {
            memcpy((void*)(blocks[k] + FillVertsOffset), FillNew[k][0], 128);
            memcpy((void*)(blocks[k] + FillPlanesOffset), FillNew[k][1], 128);
          }
          ((tBodyChanged)((__int64)g_gameHandle + BodyChangedRva))(havok, id);
          madeNow[b] = true;
          ++made;
        }
        else if (state == 3)
        {
          ((tBodyChanged)((__int64)g_gameHandle + BodyChangedRva))(havok, id);
          madeNow[b] = true;
          ++made;
        }
        else if (state == 2)
          ++already;
        else
          ++unknown;
      }
      if (made || !g_FillLogged)
      {
        g_FillLogged = true;
        util::log::Write("Dogen's kitchen: the building's fill blocks %s (%d of its %d bodies made now, %d already, %d not as the "
          "tools know them)", made ? "taken out of the rooms" : unknown == bodies ? "weren't found as the file has them" :
          "are out of the rooms", made, bodies, already, unknown);
      }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      util::log::Warning("Dogen's kitchen: a fault looking at the building's fill blocks");
      g_FillGroup = 0;
    }
  }

  // Its draw object (the component's for its instance's first part), its rows, whether it's shown
  __int64 DoorObject(DoorPlace const& place, __int64 component)
  {
    __try
    {
      __int64 objects = *(__int64 const*)(component + ComponentPartObjectsOffset);
      __int64 object = objects >= 0x10000 ? *(__int64 const*)(objects + (__int64)DoorPart(place) * 8) : 0;
      return object >= 0x10000 ? object : 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return 0;
    }
  }

  bool ReadRows(__int64 object, float (&rows)[16], bool& shown)
  {
    __try
    {
      memcpy(rows, (void const*)(object + ObjectRowsOffset), sizeof(rows));
      shown = *(int const*)(object + ObjectHandleOffset) != -1;
      return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
  }

  bool WriteRows(__int64 object, float const (&rows)[16])
  {
    __try
    {
      memcpy((void*)(object + ObjectRowsOffset), rows, sizeof(rows));
      return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
  }

  // Whether its rows are the data's placement
  bool RowsAreTheData(DoorPlace const& place, float const (&rows)[16])
  {
    for (int r = 0; r < 4; ++r)
      for (int c = 0; c < 3; ++c)
        if (fabsf(rows[r * 4 + c] - place.Rows[r][c]) > (r < 3 ? RowsTurnEpsilon : RowsPlaceEpsilon))
          return false;
    return true;
  }

  // The rows swung open by this much (0 - 1, eased), turned round its up through its hinge: its right and forward go
  // round toward Turn x its forward and -Turn x its right, its place round the hinge with them (a hinge at its origin
  // keeps it); the game's w lanes kept
  void SwungRows(DoorPlace const& place, float const (&shut)[16], float swing, float (&out)[16])
  {
    memcpy(out, shut, sizeof(out));
    if (swing <= 0.f)
      return;
    float eased = swing * swing * (3.f - 2.f * swing);
    float a = OpenAngle * eased, c = cosf(a), s = place.Turn * sinf(a);
    for (int k = 0; k < 3; ++k)
    {
      float right = shut[0 + k], forward = shut[8 + k];
      out[0 + k] = c * right + s * forward;
      out[8 + k] = -s * right + c * forward;
      out[12 + k] = shut[12 + k] + place.Hinge[0] * (right - out[0 + k]) + place.Hinge[1] * (forward - out[8 + k]);
    }
  }

  // Drawn with these rows: taken off the renderer by its Visible, the rows written, given back (the renderer is told
  // them as it shows it again). Only while it's shown; not shown, the rows are only written.
  bool DrawWith(DoorPlace const& place, Door const& door, float const (&rows)[16], bool shown)
  {
    if (!shown)
      return WriteRows(door.Object, rows);
    if (!util::hooks::SetModelGroupMemberVisible(door.Group, door.Member, place.Instance, false))
      return false;
    bool written = WriteRows(door.Object, rows);
    bool back = util::hooks::SetModelGroupMemberVisible(door.Group, door.Member, place.Instance, true);
    return written && back;
  }
}

namespace
{
  // One door: its group found while Faith is near, opened as she comes to it, shut once she's gone
  void UpdateDoor(DoorPlace const& place, DoorState& st, float const* feet, ULONGLONG now)
  {
    float dx = feet[0] - place.Middle[0], dz = feet[2] - place.Middle[2];
    float distance = sqrtf(dx * dx + dz * dz);
    // Far from it with it shut (or not found): nothing to do
    if (distance > SearchDistance && (!st.Found.Group || st.Shut))
      return;
    if (st.Found.Group && !StillDoor(place, st.Found))
    {
      util::log::Write("%s: its zone's model group is gone (streamed out)", place.Name);
      st.Found = {};
      st.Open = false;
      st.Shut = false;
      st.Swing = 0.f;
      st.AwaySince = 0;
    }
    if (!st.Found.Group)
    {
      if (distance > SearchDistance || now - st.LastSearch < SearchMs)
        return;
      st.LastSearch = now;
      if (!FindDoor(place, st.Found))
      {
        if (!st.NotFoundLogged && distance < 20.f)
        {
          st.NotFoundLogged = true;
          util::log::Write("%s: Faith is %.0f m from it and its zone's model group isn't found (not loaded, or not as the "
            "data has it)", place.Name, distance);
        }
        return;
      }
      st.NotFoundLogged = false;
      util::log::Write("%s: found it (group 0x%llX, member %d, instance %d, collision parts %u - %u)", place.Name,
        st.Found.Group, st.Found.Member, place.Instance, DoorPart(place), DoorPart(place) + place.Parts - 1);
    }

    float along = dx * place.Along[0] + dz * place.Along[1];
    float through = dx * place.Through[0] + dz * place.Through[1];
    float up = feet[1] - place.Floor;
    if (fabsf(along) <= OpenAlong && through >= -OpenOutside && through <= OpenInside && up >= -OpenBelow && up <= OpenAbove)
    {
      st.Open = true;
      st.AwaySince = 0;
    }
    else if (distance > ShutDistance || up < -ShutBelow || up > ShutAbove)
    {
      if (!st.AwaySince)
        st.AwaySince = now;
      if (now - st.AwaySince >= ShutAfterMs)
        st.Open = false;
    }
    else
      st.AwaySince = 0;
    ULONGLONG step = st.LastTick ? now - st.LastTick : 0;
    st.LastTick = now;
    if (step > MostStepMs)
      step = MostStepMs;

    __int64 component = DoorComponent(st.Found);
    int bodies = 0, allOff = 0, allOn = 0;
    if (!component || !ReadParts(place, component, bodies, allOff, allOn))
    {
      if (!st.PartsUnknownLogged)
      {
        st.PartsUnknownLogged = true;
        util::log::Warning("%s: %s isn't as the tools know it, it stays as the game has it", place.Name,
          component ? "a body of its collision" : "its member's record or physics component");
      }
      return;
    }

    // Its draw object, its rows as the data has them before anything's done with it
    if (st.Found.Bad)
      return;
    __int64 object = DoorObject(place, component);
    float rows[16];
    bool shown = false;
    if (!object || !ReadRows(object, rows, shown))
      return;
    if (object != st.Found.Object)
    {
      if (st.Found.Object || !RowsAreTheData(place, rows))
      {
        // (a new object while the tools had it swung, or one not where the data puts the door: left as it is, its
        // collision back if the tools had taken it)
        st.Found.Bad = true;
        if (allOn < bodies)
          SwitchParts(place, component, true);
        util::log::Warning("%s: its draw object 0x%llX isn't as the tools know it (rows %.3f %.3f %.3f / %.3f %.3f %.3f / "
          "%.2f %.2f %.2f%s), it stays as the game has it", place.Name, object, rows[0], rows[1], rows[2], rows[8], rows[9],
          rows[10], rows[12], rows[13], rows[14], st.Found.Object ? ", a second one" : "");
        return;
      }
      st.Found.Object = object;
      memcpy(st.Found.Rows, rows, sizeof(rows));
      st.Swing = 0.f;
      if (!st.ObjectLogged)
      {
        st.ObjectLogged = true;
        util::log::Write("%s: its draw object 0x%llX is where the data puts it (%s, %d collision bodies)", place.Name, object,
          shown ? "drawn" : "not drawn yet", bodies);
      }
    }
    bool wanted = st.Open && !st.CantDraw;
    st.Shut = !wanted && st.Swing <= 0.f && allOn == bodies;
    if (st.Shut)
      return;

    // Its collision goes as it starts to open
    if (wanted && allOff < bodies && st.OpenFailures < MostFailures && now - st.LastSwitch >= RetryMs)
    {
      st.LastSwitch = now;
      bool switched = SwitchParts(place, component, false);
      ReadParts(place, component, bodies, allOff, allOn);
      if (switched && allOff == bodies)
      {
        st.OpenFailures = 0;
        util::log::Write("%s: opening (Faith %.1f m along it, %.1f m through it); its collision off on %d of %d bodies",
          place.Name, along, through, allOff, bodies);
      }
      else if (++st.OpenFailures >= MostFailures)
        util::log::Warning("%s: its collision didn't go %d times (off on %d of %d bodies): it stays shut", place.Name,
          MostFailures, allOff, bodies);
    }
    // It swings only once its collision is off (it stays shut, and solid, if that didn't take)
    float target = wanted && allOff == bodies ? 1.f : 0.f;
    if (st.Swing != target && step)
    {
      float delta = (float)step / (float)(target > st.Swing ? OpenMs : ShutMs);
      float swing = target > st.Swing ? (std::min)(target, st.Swing + delta) : (std::max)(target, st.Swing - delta);
      float drawn[16];
      if (swing <= 0.f)
        memcpy(drawn, st.Found.Rows, sizeof(drawn));
      else
        SwungRows(place, st.Found.Rows, swing, drawn);
      if (DrawWith(place, st.Found, drawn, shown))
      {
        st.Swing = swing;
        if (st.Swing >= 1.f || st.Swing <= 0.f)
          util::log::Write("%s: %s (Faith %.1f m from it)", place.Name, st.Swing >= 1.f ? "open" : "shut, drawn where the "
            "game had it", distance);
      }
      else
      {
        // Shut and solid from now on: its rows as the game had them (drawn so the next time the game shows it)
        st.CantDraw = true;
        WriteRows(st.Found.Object, st.Found.Rows);
        st.Swing = 0.f;
        if (!st.ShowFailLogged)
        {
          st.ShowFailLogged = true;
          util::log::Warning("%s: it can't be drawn swung (the model groups' property hook isn't on, or a fault): it stays "
            "shut", place.Name);
        }
      }
    }
    // Its collision back once it's shut
    if (!wanted && st.Swing <= 0.f && allOn < bodies && st.ShutFailures < MostFailures && now - st.LastSwitch >= RetryMs)
    {
      st.LastSwitch = now;
      bool switched = SwitchParts(place, component, true);
      ReadParts(place, component, bodies, allOff, allOn);
      if (switched && allOn == bodies)
      {
        st.ShutFailures = 0;
        util::log::Write("%s: its collision on on %d of %d bodies", place.Name, allOn, bodies);
      }
      else if (++st.ShutFailures >= MostFailures)
        util::log::Warning("%s: its collision didn't come back %d times (on on %d of %d bodies)", place.Name, MostFailures,
          allOn, bodies);
    }
  }
}

void dogen::Update(bool hasFaith, float const* feet)
{
  if (!hasFaith || !feet)
    return;
  if (g_Code == 0)
  {
    bool known = false;
    __try
    {
      known = CodeKnown();
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
    g_Code = known ? 1 : -1;
    if (!known)
      util::log::Warning("Dogen's door: the game's code isn't what the tools know, it stays shut (The View's roof door too)");
  }
  if (g_Code < 0)
    return;

  ULONGLONG now = GetTickCount64();
  // Dogen's kitchen: the building's fills, while she's near its door
  DoorState& dogenDoor = g_Doors[0];
  float dx = feet[0] - Doors[0].Middle[0], dz = feet[2] - Doors[0].Middle[2];
  float distance = sqrtf(dx * dx + dz * dz);
  UpdateFills(distance, now);
  for (int d = 0; d < DoorCount; ++d)
    UpdateDoor(Doors[d], g_Doors[d], feet, now);
  // Its ceiling lamps, in its door's group: their own look once a group, their lights while she's near. None while
  // the group isn't found (a group found again is given the look again), nor while she's far with the door shut.
  if (!dogenDoor.Found.Group)
  {
    g_LampsGroup = g_LampsData = 0;
    g_LampsLit = false;
    UpdateLampLights(false);
    return;
  }
  if (distance > SearchDistance && dogenDoor.Shut)
  {
    UpdateLampLights(false);
    return;
  }
  UpdateLamps(dogenDoor.Found);
  bool any = false;
  for (int id : g_LampLights)
    any |= id != 0;
  UpdateLampLights(g_LampsLit && distance <= (any ? LampLightFar : LampLightNear));
}
