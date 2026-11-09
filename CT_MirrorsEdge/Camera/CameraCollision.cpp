#include "CameraCollision.h"
#include "../Main.h"
#include "../Util/Util.h"
#include <DirectXMath.h>
#include <Windows.h>
#include <cstring>

using namespace DirectX;

namespace
{
  // The game's sphere sweep (2026-10-25, offline): rcx = in {from, to,
  // radius}, r8 = the physics world, r9 = out; rdx isn't read. Over a radius
  // of 1e-6 it makes a sphere (0x1434DB570) and casts it (0x1434E2A10), else
  // it casts a ray (0x1434E27F0), both with collision filter 9 and one hit.
  const __int64 SphereSweepRva = 0x3A24890;
  const __int64 SweepSphereCallRva = 0x3A2493E;   // call 0x1434DB570
  const __int64 SweepCastCallRva = 0x3A249BD;     // call 0x1434E2A10
  const __int64 SweepRayCallRva = 0x3A24AFB;      // call 0x1434E27F0
  const __int64 MakeSphereRva = 0x34DB570;
  const __int64 CastSphereRva = 0x34E2A10;
  const __int64 CastRayRva = 0x34E27F0;
  // The client physics world: [[[[[game world] + 0x50] + 0xF0]] + 0x18]
  // (0x1434EE940 reads the last three), the game world at [0x142401CA0] (its
  // load at 0x14393BED4, before the game's own sweeps)
  const __int64 GameWorldRva = 0x2401CA0;
  const __int64 GameWorldLoadRva = 0x393BED4;
  const __int64 PhysicsWorldOfRva = 0x34EE940;

  struct alignas(16) SweepIn
  {
    float From[4];
    float To[4];
    float Radius;
    float Unused[3];
  };
  // + 0x00 the contact, + 0x10 its normal, + 0x20 the body's id, + 0x24 the
  // fraction of the way, + 0x28 the distance, + 0x2C hit; + 0x30 a word
  // written as 0x101
  struct alignas(16) SweepOut
  {
    float Place[4];
    float Normal[4];
    unsigned int Id;
    float Fraction;
    float Distance;
    unsigned char Hit;
    unsigned char Rest[0x13];
  };
  static_assert(sizeof(SweepOut) == 0x40, "the sweep writes 0x32 bytes");

  typedef void(__fastcall* tSweep)(SweepIn*, __int64, __int64, SweepOut*);

  int g_State = 0;          // 0 unchecked, 1 the code is as read, -1 not (or the sweep failed)
  const float Skin = 0.01f; // kept off what it touches
  const int MaxSlides = 3;

  bool CallTargets(BYTE const* base, __int64 callRva, __int64 targetRva)
  {
    return base[callRva] == 0xE8 && callRva + 5 + *(int const*)(base + callRva + 1) == targetRva;
  }

  bool CheckCode()
  {
    BYTE const* base = (BYTE const*)g_gameHandle;
    static const BYTE sweep[] = { 0x48, 0x89, 0xE0, 0x55, 0x56, 0x57, 0x41, 0x56, 0x41, 0x57, 0x48, 0x8D, 0xA8, 0x08, 0xFF,
      0xFF, 0xFF, 0x48, 0x81, 0xEC, 0xD0, 0x01, 0x00, 0x00 };
    static const BYTE worldOf[] = { 0x48, 0x8B, 0x81, 0xF0, 0x00, 0x00, 0x00, 0x48, 0x8B, 0x00, 0x48, 0x8B, 0x40, 0x18, 0xC3 };
    if (memcmp(base + SphereSweepRva, sweep, sizeof(sweep)) || memcmp(base + PhysicsWorldOfRva, worldOf, sizeof(worldOf)) ||
        !CallTargets(base, SweepSphereCallRva, MakeSphereRva) || !CallTargets(base, SweepCastCallRva, CastSphereRva) ||
        !CallTargets(base, SweepRayCallRva, CastRayRva))
      return false;
    // mov rdi, [rip -> the game world]
    BYTE const* load = base + GameWorldLoadRva;
    return load[0] == 0x48 && load[1] == 0x8B && load[2] == 0x3D && GameWorldLoadRva + 7 + *(int const*)(load + 3) == GameWorldRva;
  }

  // The game world and its client physics world now (false: none, a level
  // loading)
  bool FindWorld(__int64& gameWorld, __int64& physicsWorld)
  {
    __try
    {
      gameWorld = *(__int64 const*)((__int64)g_gameHandle + GameWorldRva);
      if (gameWorld < 0x10000)
        return false;
      __int64 p = *(__int64 const*)(gameWorld + 0x50);
      if (p < 0x10000)
        return false;
      p = *(__int64 const*)(p + 0xF0);
      if (p < 0x10000)
        return false;
      p = *(__int64 const*)p;
      if (p < 0x10000)
        return false;
      physicsWorld = *(__int64 const*)(p + 0x18);
      if (physicsWorld < 0x10000)
        return false;
      // Its Havok world, and that one's collision filter (what the casts read)
      __int64 havok = *(__int64 const*)(physicsWorld + 0x10);
      return havok >= 0x10000 && *(__int64 const*)(havok + 0x150) >= 0x10000;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
  }

  bool CallSweep(SweepIn* in, __int64 gameWorld, __int64 physicsWorld, SweepOut* out)
  {
    __try
    {
      ((tSweep)((__int64)g_gameHandle + SphereSweepRva))(in, gameWorld, physicsWorld, out);
      return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
  }
}

bool collision::Available()
{
  if (g_State == 0)
  {
    g_State = CheckCode() ? 1 : -1;
    if (g_State < 0)
      util::log::Warning("Camera collision: the game's sphere sweep isn't as it was read (another game version?), the camera "
        "won't stop at walls");
  }
  __int64 gameWorld = 0, physicsWorld = 0;
  return g_State > 0 && FindWorld(gameWorld, physicsWorld);
}

bool collision::Sweep(float const* from, float const* to, float radius, float* fraction, float* normal)
{
  *fraction = 1.f;
  if (!Available())
    return false;
  __int64 gameWorld = 0, physicsWorld = 0;
  if (!FindWorld(gameWorld, physicsWorld))
    return false;
  SweepIn in{};
  memcpy(in.From, from, 3 * sizeof(float));
  memcpy(in.To, to, 3 * sizeof(float));
  in.Radius = radius;
  SweepOut out{};
  if (!CallSweep(&in, gameWorld, physicsWorld, &out))
  {
    g_State = -1;
    util::log::Warning("Camera collision: the game's sphere sweep failed, the camera won't stop at walls until the game "
      "restarts");
    return false;
  }
  if (!out.Hit)
    return false;
  *fraction = out.Fraction < 0.f ? 0.f : (out.Fraction > 1.f ? 1.f : out.Fraction);
  memcpy(normal, out.Normal, 3 * sizeof(float));
  return true;
}

bool collision::Move(float const* from, float* to, float radius, float* normal)
{
  if (!Available())
    return false;
  __int64 gameWorld = 0, physicsWorld = 0;
  if (!FindWorld(gameWorld, physicsWorld))
    return false;

  XMVECTOR place = XMVectorSet(from[0], from[1], from[2], 0.f);
  XMVECTOR target = XMVectorSet(to[0], to[1], to[2], 0.f);
  bool touched = false;
  for (int i = 0; i < MaxSlides; ++i)
  {
    XMVECTOR delta = target - place;
    float length = XMVectorGetX(XMVector3Length(delta));
    if (length < 1e-4f)
      break;
    SweepIn in{};
    XMStoreFloat3((XMFLOAT3*)in.From, place);
    XMStoreFloat3((XMFLOAT3*)in.To, target);
    in.Radius = radius;
    SweepOut out{};
    if (!CallSweep(&in, gameWorld, physicsWorld, &out))
    {
      g_State = -1;
      util::log::Warning("Camera collision: the game's sphere sweep failed, the camera won't stop at walls until the game "
        "restarts");
      return touched;
    }
    if (!out.Hit)
    {
      place = target;
      break;
    }
    XMVECTOR wall = XMVectorSet(out.Normal[0], out.Normal[1], out.Normal[2], 0.f);
    float wallLength = XMVectorGetX(XMVector3Length(wall));
    XMVECTOR direction = delta / length;
    wall = wallLength > 1e-4f ? wall / wallLength : -direction;
    // Already touching it and going away from it (or along it): free
    if (XMVectorGetX(XMVector3Dot(delta, wall)) >= 0.f)
    {
      place = target;
      break;
    }
    touched = true;
    XMStoreFloat3((XMFLOAT3*)normal, wall);
    float fraction = out.Fraction < 0.f ? 0.f : (out.Fraction > 1.f ? 1.f : out.Fraction);
    float travel = fraction * length - Skin;
    if (travel > 0.f)
      place += direction * travel;
    // The rest of the move along the surface
    XMVECTOR rest = target - place;
    rest -= wall * XMVectorGetX(XMVector3Dot(rest, wall));
    target = place + rest;
  }
  XMStoreFloat3((XMFLOAT3*)to, place);
  return touched;
}
