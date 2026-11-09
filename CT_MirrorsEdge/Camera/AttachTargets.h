#pragma once
#include <DirectXMath.h>

// What the free camera can follow besides Faith: entities (characters,
// vehicles, drones, props, static models: trains, aircraft, security cameras
// and models that move) and the members of swarms (the city's traffic and
// crowds). Found in the game's own per-class instance lists and read from
// memory without calling anything on them.
//
// Everything here runs on the game's thread (the camera hook).
namespace attach
{
  const int MaxCandidates = 48;
  const int MaxClasses = 64;

  // For the menu's filters
  enum class Group { People, Cars, Drones, Boats, Trains, Aircraft, Cameras, Other };

  // What a target is, from the names of its models: a static model's mesh
  // ("objects/props/monorail/monorail_train_01_Mesh"), a swarm's appearance
  // items ("Levels/SP/SP_MainCity/Crowds/Cars/CarBackdropSport_01")
  enum NamedKind : unsigned char
  {
    Named_None,           // not known from a name: its class (and for a swarm, how it moves) tells
    Named_Crowd,          // a swarm of people (any folder but Cars, Boats and Drones)
    Named_RoadTraffic,    // Crowds/Cars: cars, trucks and motorcycles in one swarm
    Named_Drone,          // Crowds/Drones: flying traffic, news drones, VTOL helicopters
    Named_Boat,
    Named_Train,          // the monorail trains' carriages
    Named_Airplane,
    Named_Blimp,
    Named_Vtol,           // a VTOL aircraft model
    Named_SecurityCamera,
    Named_Lift
  };

  struct Target
  {
    __int64 Entity;       // the entity, or a swarm member's SwarmEntity
    __int64 VTable;       // its vtable when it was picked: another one means it's gone
    __int64 Boids;        // swarm member: the swarm's CrowdBoidsComponent
    __int64 BoidsVTable;
    __int64 Data;         // static model: its data when it was picked, 0 for the rest
    int Agent;            // swarm member: its record in the boids list, -1 for entities
    int Class;            // see ClassName / KindOf
    unsigned char SwarmType; // swarm member: 0 not known (nothing moved), 1 traffic, 2 crowd
    unsigned char Named;  // see NamedKind
    bool HasBox;          // static model: BoxMin / BoxMax are its mesh's box, local
    DirectX::XMFLOAT3 BoxMin;
    DirectX::XMFLOAT3 BoxMax;
    char Label[40];       // static model: its mesh's file name; swarm member: its models' folder
    float Distance;       // from the camera when found
    float Angle;          // from the middle of the view when found, radians
    float Score;          // lower is a better pick
    DirectX::XMFLOAT3 Position; // where it was when found
    // Swarm member: the way it's going (smoothed), and the record's position
    // when it last changed, to move smoothly between the swarm's updates
    DirectX::XMFLOAT3 Heading;
    DirectX::XMFLOAT3 Seen;
    double SeenTime;
    float Interval;
  };

  struct View
  {
    DirectX::XMFLOAT3 Camera;
    DirectX::XMFLOAT3 Look;         // unit vector
    __int64 Skip;                   // Faith's soldier
    DirectX::XMFLOAT3 SkipPosition; // and where she is, in case her entity isn't that pointer
    bool HasSkipPosition;
  };

  struct Scan
  {
    // Best first. Static models are kept apart: they only count if they move.
    Target Best[MaxCandidates];
    int BestCount;
    Target Static[MaxCandidates];
    int StaticCount;
    // What's in the level, for the log
    int Found[MaxClasses];
    int Swarms;
    int NoTransform;
    int Errors;
    int Dropped;  // ScanNearby: within the radius but no room left in out
  };

  // Looks the classes up the first time (they're static data in the exe).
  // From Faith's class, or the build the tools were made for. False if not found.
  bool FindClasses(__int64 pFaithSoldier, bool& fromFaith);
  int ClassCount();
  const char* ClassName(int index);
  const char* Kind(int index);
  bool IsStatic(int index);
  bool IsSwarm(int index);

  // What it is ("character", "crowd", "train", ...), which filter it's under,
  // and how high above its origin its middle is
  const char* KindOf(Target const& target);
  Group GroupOf(Target const& target);
  float AimHeight(Target const& target);
  // Its mesh's file name or its swarm's folder, else its class's name
  const char* LabelOf(Target const& target);

  // Listed and picked even when it doesn't move (static models usually only
  // count once they move): trains, aircraft and security cameras
  bool ListedWhenStill(Target const& target);
  // So few and so fast that they're listed wherever they are in the level,
  // and picked further away: trains and aircraft
  bool ListedAtAnyDistance(Target const& target);
  // A static model that's only listed once it moves
  bool OnlyWhenMoving(Target const& target);
  // A static model's data (a Target's Data) drawn only in the game's runner's
  // vision layer: PamStaticModelEntityData's PrimaryInRVLayer (the Dash's
  // start, checkpoint arrows and finish); false for any other data
  bool PrimaryInRvLayer(__int64 data);
  inline bool IsSecurityCamera(Target const& target) { return target.Named == Named_SecurityCamera; }
  // Its route starts again somewhere else (a train, an airplane): a jump
  // isn't the end of it
  bool KeepsThroughJumps(Target const& target);

  // Everything in front of the camera that could be followed
  void ScanView(View const& view, Scan& scan);

  // Everything within the radius of the camera (and what's listed at any
  // distance), in no order. Gives how many were written to out (at most max).
  // Fills scan's counts, not its lists.
  int ScanNearby(View const& view, float radius, Target* out, int max, Scan& scan);

  // The target's transform now, if it still exists. A swarm member's is made
  // from where it is and the way it's moving.
  bool ReadTransform(Target& target, DirectX::XMFLOAT4X4& transform);

  // Moved further than this in one frame: not the same object any more
  float JumpLimit(Target const& target);

  // Same object (a swarm member is its swarm and record)
  inline bool SameTarget(Target const& a, Target const& b) { return a.Entity == b.Entity && a.Agent == b.Agent; }
}
