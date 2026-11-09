#include "LightManager.h"
#include "../Main.h"
#include "../Util/Util.h"
#include "../imgui/imgui.h"
#include "../Util/ImGuiEXT.h"
#include "../Visuals/Panini.h"
#include "../Camera/CameraCollision.h"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <DirectXMath.h>
#include <Psapi.h>

using namespace DirectX;

namespace
{
  // Module-relative addresses the original tools hardcoded (absolute in the
  // original, the exe loads at 0x140000000). They have no signatures, so the
  // light manager is only enabled for the game build they came from.
  const __int64 SphereEntityVTable = 0x1BFF178;
  const __int64 SphereDataVTable   = 0x1BFA8F8;
  const __int64 SpotEntityVTable   = 0x1BFF240;
  const __int64 SpotDataVTable     = 0x1BFA930;
  const __int64 EntityInterface    = 0x1BFF168; // Stored at entity + 0xB0
  const DWORD SupportedTimeStamp   = 0x5762E3B7;

  const int SphereEntitySize = 0x250;
  const int SpotEntitySize = 0x270;
  const int DataSize = 0x110;
  static_assert(DataSize == LightManager::LightDataSize, "the undo's copy of a light's data is its whole block");

  // Light data (entity + 0x28) fields, named as the game's reflection has
  // them (PbrAnalyticLightEntityData and the sphere / spot classes)
  const int ColorOffset              = 0x70; // float3
  const int ParticleColorOffset      = 0x80; // float3, ParticleColorScale
  const int RadiosityColorOffset     = 0x90; // float3, RadiosityColorScale (bounce light)
  const int LightColorModeOffset     = 0xA0; // int: 0 RGB, 1 Temperature (the game's runtime reads neither,
  const int TemperatureOffset        = 0xA4; // int, Kelvin                 its data has the colour in Color)
  const int IntensityOffset          = 0xA8;
  const int AttenuationRadiusOffset  = 0xAC;
  const int AttenuationOffsetOffset  = 0xB0;
  const int LightUnitOffset          = 0xB4; // 0 luminous power (lumens), 1 luminance (nits)
  const int ShadowRadiusOffset       = 0xC0;
  const int ShadowCullDistanceOffset = 0xC4;
  const int ShadowFadeDistanceOffset = 0xC8;
  const int ShadowDimmerOffset       = 0xCC;
  const int DimmerOffset             = 0xD0;
  const int EmissiveShapeOffset      = 0xE0; // bool
  const int DiffuseOffset            = 0xE1; // bool
  const int SpecularOffset           = 0xE2; // bool
  const int CastShadowsOffset        = 0xE3; // bool, CastShadowsEnable
  const int RadiosityOffset          = 0xE5; // bool
  const int SizeOffset               = 0xF0; // SphereRadius (sphere), DiscRadius (spot)
  const int InnerAngleOffset         = 0xF4; // spot, the whole cone's angle
  const int OuterAngleOffset         = 0xF8; // spot, likewise

  // The entity: its transform (rows right, up, backward, position; a spot
  // shines along -row 2, the way a camera looks: its shadow view takes the
  // light's transform as a render view's) and its Enabled byte
  const int EntityTransformOffset = 0x40;
  const int EntityEnabledOffset = 0xA0;

  const int RenderViewOffset = 0x58;
  const int RenderViewTransform = 0x40;
  const int RenderViewProjection = 0x360;

  // The game's render lights ([0x142568CE0], what 0x1436E4F80 adds to): an
  // entity's handle (+ 0x80, a word; 0xFFFF none) is the index of its
  // light's 0xC00-byte entry at + 0x48 (begin) .. + 0x50 (end); freed
  // indices (words) are at + 0x68 .. + 0x70. An entry keeps its own index
  // at + 0xB4 (0x1436E51BF, put back by each update at 0x143707581) and the
  // light's position (+ 0x30), radius (+ 0x70) and shape (+ 0x90) as
  // 0x1436EBD50 built them. A full rebuild of the game's lights (0x1436EA240)
  // empties it and makes only the game's own again, so a tools' light's
  // handle can then be another light's.
  const __int64 RenderLightsRva = 0x2568CE0;
  const int RenderLightSize = 0xC00;
  const int EntityHandleOffset = 0x80;
  const int EntityQueuedOffset = 0xA2;   // set while UpdateLight's queue has it
  const int EntityShapeOffset = 0x200;
  const WORD NoHandle = 0xFFFF;
  // How often the handles are checked, and how long a light has to be
  // unchanged first (until the game has taken a change it's still the old one)
  const ULONGLONG HandleCheckMs = 1000;
  const ULONGLONG HandleQuietMs = 1000;
  // Checks in a row that find a light's render light gone before it's made
  // again (one could read the game's lists while they change)
  const int HandleMissesToRemake = 2;
  // A light still "queued" (+ 0xA2) this long after it was sent: the game's
  // queue was full and dropped it (0x1437623E0 sets + 0x18 and returns, the
  // byte UpdateLight set stays), so UpdateLight would never take it again
  const ULONGLONG QueuedStuckMs = 5000;
  // A light turned off for good: looked at from this long after its off was
  // sent (the game has taken it by then) until it's gone, or given up on
  const ULONGLONG LeavingWaitMs = 1000;
  const ULONGLONG LeavingGiveUpMs = 6000;
  // How long the tab shows what the last removal came to
  const ULONGLONG LeftShownMs = 20000;
  // The picked spot's aiming handle: along its beam, this share of its
  // distance from the camera (within these, metres), so it's on screen
  const float AimHandleShare = 0.15f, AimHandleMin = 0.3f, AimHandleMax = 4.f;

  // New lights: the game's own lights' brightness (its sphere lights' median
  // is 300 lumens, its spots' 650; the original tools' 10 lumens is less than
  // a candle), a little in front of the camera so their dot can be grabbed
  const float SphereIntensity = 800.f;
  const float SpotIntensity = 1500.f;
  const float NewLightAhead = 2.f;
  // ... or on what the camera looks at, when the game's own ray (the free
  // camera's sweep, collision::Sweep) finds something within this: a sphere
  // this share of its light radius off the surface, a spot this share of it
  // back toward the camera, so the light reaches the surface. Nearer than the
  // last: half way there.
  const float SurfaceReach = 60.f;
  const float SurfaceSphereShare = 0.5f, SurfaceSpotShare = 0.6f;
  const float SurfaceNearest = 1.f;
  // The menu's Undo: how many steps it keeps
  const size_t MaxUndo = 50;
  // and their light radius (the user's, 2026-11-01; the looks set their own)
  const float NewLightRadius = 1.f;
  // What most of the game's lights have (80 m; the original tools had 20 m
  // and a fade of 18 m)
  const float ShadowCullDistance = 80.f;

  // How far a drag with the wheel can take a light, from the camera
  const float DragNearest = 0.25f, DragFarthest = 2000.f;
  const float WheelStep = 1.15f;

  const float Pi = 3.14159265f;
  const float Identity[16] = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };

  // The colour of a temperature, as the game's own lights in temperature
  // mode have it in their Color: every 100 K step that one of its 4,120 PBR
  // lights uses (the colour of a step is its lower 100 K's), straight lines
  // in between for the steps none uses
  const float TemperatureColors[][4] = {
    { 1000, 1.f, 0.0401f, 0.f }, { 1300, 1.f, 0.1085f, 0.f }, { 1400, 1.f, 0.1303f, 0.f }, { 1800, 1.f, 0.2097f, 0.f },
    { 2200, 1.f, 0.293f, 0.0257f }, { 2500, 1.f, 0.3577f, 0.064f }, { 2600, 1.f, 0.3786f, 0.079f },
    { 2700, 1.f, 0.3992f, 0.095f }, { 2900, 1.f, 0.4394f, 0.1297f }, { 3000, 1.f, 0.4589f, 0.1483f },
    { 3100, 1.f, 0.4781f, 0.1677f }, { 3200, 1.f, 0.497f, 0.1879f }, { 3300, 1.f, 0.5155f, 0.2087f },
    { 3400, 1.f, 0.5336f, 0.2301f }, { 3500, 1.f, 0.5515f, 0.252f }, { 3600, 1.f, 0.5689f, 0.2745f },
    { 3700, 1.f, 0.586f, 0.2974f }, { 3900, 1.f, 0.6193f, 0.3444f }, { 4000, 1.f, 0.6354f, 0.3684f },
    { 4100, 1.f, 0.6511f, 0.3927f }, { 4200, 1.f, 0.6666f, 0.4172f }, { 4300, 1.f, 0.6817f, 0.4419f },
    { 4400, 1.f, 0.6966f, 0.4668f }, { 4500, 1.f, 0.7111f, 0.4919f }, { 4600, 1.f, 0.7253f, 0.517f },
    { 4700, 1.f, 0.7392f, 0.5422f }, { 4800, 1.f, 0.7528f, 0.5675f }, { 5000, 1.f, 0.7792f, 0.618f },
    { 5100, 1.f, 0.7919f, 0.6433f }, { 5200, 1.f, 0.8044f, 0.6685f }, { 5400, 1.f, 0.8286f, 0.7187f },
    { 5500, 1.f, 0.8403f, 0.7437f }, { 5600, 1.f, 0.8518f, 0.7686f }, { 5700, 1.f, 0.863f, 0.7933f },
    { 5800, 1.f, 0.874f, 0.8179f }, { 5900, 1.f, 0.8847f, 0.8424f }, { 6000, 1.f, 0.8952f, 0.8666f },
    { 6100, 1.f, 0.9055f, 0.8907f }, { 6200, 1.f, 0.9156f, 0.9147f }, { 6300, 1.f, 0.9254f, 0.9384f },
    { 6400, 1.f, 0.9351f, 0.9619f }, { 6500, 1.f, 0.9445f, 0.9853f }, { 6600, 0.9917f, 0.9458f, 1.f },
    { 6700, 0.9696f, 0.9336f, 1.f }, { 6900, 0.929f, 0.9107f, 1.f }, { 7000, 0.9102f, 0.9f, 1.f },
    { 7100, 0.8923f, 0.8897f, 1.f }, { 7200, 0.8753f, 0.8799f, 1.f }, { 7400, 0.8437f, 0.8614f, 1.f },
    { 7500, 0.8289f, 0.8527f, 1.f }, { 7600, 0.8149f, 0.8443f, 1.f }, { 7700, 0.8014f, 0.8363f, 1.f },
    { 7900, 0.7762f, 0.8211f, 1.f }, { 8000, 0.7644f, 0.8139f, 1.f }, { 8100, 0.7531f, 0.8069f, 1.f },
    { 8200, 0.7423f, 0.8002f, 1.f }, { 8400, 0.7219f, 0.7875f, 1.f }, { 8700, 0.6941f, 0.77f, 1.f },
    { 9000, 0.6693f, 0.7541f, 1.f }, { 9100, 0.6617f, 0.7492f, 1.f }, { 9200, 0.6543f, 0.7444f, 1.f },
    { 9700, 0.6208f, 0.7224f, 1.f }, { 9900, 0.6089f, 0.7144f, 1.f }, { 10000, 0.6033f, 0.7106f, 1.f } };
  const int MinTemperature = 1000, MaxTemperature = 10000;

  void TemperatureColor(int kelvin, float* color)
  {
    float k = (float)(kelvin / 100 * 100);
    const int count = sizeof(TemperatureColors) / sizeof(TemperatureColors[0]);
    int i = 0;
    while (i < count - 2 && TemperatureColors[i + 1][0] <= k)
      ++i;
    float const* a = TemperatureColors[i];
    float const* b = TemperatureColors[i + 1];
    float t = (k - a[0]) / (b[0] - a[0]);
    t = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
    for (int c = 0; c < 3; ++c)
      color[c] = a[c + 1] + (b[c + 1] - a[c + 1]) * t;
  }

  // Looks to start a light from: about what the real thing gives (the game
  // lights its world in real units, see Research_Lights_2026-10-26.md)
  struct Look
  {
    const char* Name;
    const char* Help;
    int Kelvin;
    float Lumens;
    float Reach;
    float Cone;       // a spot's whole cone, degrees
    float Softness;   // how much of a spot's cone fades out, 0 - 1
  };
  const Look SphereLooks[] = {
    { "Candle", "A small flame: about 12 lm, 1900 K.", 1900, 12.f, 2.f, 0.f, 0.f },
    { "Light bulb", "A household bulb: about 800 lm (a 60 W bulb), 2700 K.", 2700, 800.f, 6.f, 0.f, 0.f },
    { "Ceiling light", "A bright room light: about 3,000 lm, 4000 K.", 4000, 3000.f, 10.f, 0.f, 0.f },
    { "Street lamp", "About 12,000 lm reaching 25 m, 4000 K.", 4000, 12000.f, 25.f, 0.f, 0.f },
    { "Daylight fill", "300,000 lm, 5500 K: strong enough to be seen in daylight a few metres away.",
      5500, 300000.f, 30.f, 0.f, 0.f } };
  const Look SpotLooks[] = {
    { "Torch", "A hand torch: about 300 lm in a 30" "\xC2\xB0" " cone, 6000 K.", 6000, 300.f, 25.f, 30.f, 0.5f },
    { "Desk lamp", "About 450 lm in a wide cone, 3000 K.", 3000, 450.f, 4.f, 100.f, 0.6f },
    { "Car headlight", "About 1,500 lm in a 40" "\xC2\xB0" " cone reaching 50 m, 4500 K.", 4500, 1500.f, 50.f, 40.f, 0.7f },
    { "Stage spotlight", "About 20,000 lm in a narrow 20" "\xC2\xB0" " cone with a sharp edge, 3200 K.",
      3200, 20000.f, 40.f, 20.f, 0.15f },
    { "Film light", "About 100,000 lm, 5600 K: strong enough to be seen in daylight a few metres away.",
      5600, 100000.f, 40.f, 60.f, 0.5f } };
  const int LookCount = 5;

  // A light's brightness a notch of the wheel over its dot, and its light
  // radius with Ctrl held (within the Light radius box's 0.05 - 1000 m)
  const float WheelBrightness = 1.25f;
  const float WheelRadius = 1.15f;
  const float MinRadius = 0.05f, MaxRadius = 1000.f;
  // How fresh the cursor's being on a dot has to be for HoldsCtrl
  const ULONGLONG CursorOnDotMs = 150;
  const float MaxIntensity = 100000000.f;

  template <typename T>
  T& At(BYTE* p, int offset) { return *(T*)(p + offset); }

  // Whether the game's render light for handle is still this light's: 1 it
  // is (its entry has its index and this place and radius, or the other
  // place and radius when one is given, and the shape), 0 it isn't (out of
  // the list, freed, or another light's), -1 the lists couldn't be read
  int HandleIsOurs(WORD handle, float const* place, float radius, float const* otherPlace, float otherRadius,
    int shape)
  {
    __try
    {
      __int64 lights = *(__int64 const*)((__int64)g_gameHandle + RenderLightsRva);
      if (lights < 0x10000)
        return -1;
      BYTE const* begin = *(BYTE const* const*)(lights + 0x48);
      BYTE const* end = *(BYTE const* const*)(lights + 0x50);
      if (!begin || end < begin)
        return -1;
      if ((__int64)handle >= (end - begin) / RenderLightSize)
        return 0;
      WORD const* freeBegin = *(WORD const* const*)(lights + 0x68);
      WORD const* freeEnd = *(WORD const* const*)(lights + 0x70);
      if (freeBegin && freeEnd >= freeBegin && freeEnd - freeBegin <= 0x10000)
        for (WORD const* f = freeBegin; f < freeEnd; ++f)
          if (*f == handle)
            return 0;
      BYTE const* entry = begin + (__int64)handle * RenderLightSize;
      if (*(WORD const*)(entry + 0xB4) != handle || *(int const*)(entry + 0x90) != shape)
        return 0;
      float const* at = (float const*)(entry + 0x30);
      float r = *(float const*)(entry + 0x70);
      bool same = at[0] == place[0] && at[1] == place[1] && at[2] == place[2] && r == radius;
      bool other = otherPlace && at[0] == otherPlace[0] && at[1] == otherPlace[1] && at[2] == otherPlace[2] &&
        r == otherRadius;
      return same || other ? 1 : 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return -1;
    }
  }

  // The game's light update, false if it faulted (a light whose level is gone)
  bool SafeUpdateLight(__int64 pUpdateLight, BYTE* pEntity)
  {
    typedef void(__fastcall* tUpdateLight)(void* pEntity);
    __try
    {
      ((tUpdateLight)pUpdateLight)(pEntity);
      return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
  }

  // The main render view's camera (GameRenderer + 0x58: + 0x40 its
  // transform, + 0x360 its projection, as the green screen reads them).
  // False without one: a loading screen (no camera in the world, near the
  // origin) or no projection yet.
  bool ReadMainView(__int64 ppGameRenderer, float* transform, float* projection)
  {
    __try
    {
      __int64 pGameRenderer = *(__int64*)ppGameRenderer;
      if (!pGameRenderer)
        return false;
      __int64 pRenderView = *(__int64*)(pGameRenderer + RenderViewOffset);
      if (!pRenderView)
        return false;
      memcpy(transform, (void const*)(pRenderView + RenderViewTransform), 16 * sizeof(float));
      memcpy(projection, (void const*)(pRenderView + RenderViewProjection), 16 * sizeof(float));
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
    return projection[0] != 0.f && projection[5] != 0.f &&
      fabsf(transform[12]) + fabsf(transform[13]) + fabsf(transform[14]) > 2.f;
  }

  // Where a point is on the screen, in pixels, with the Panini projection
  // when it's on (as CameraManager::ProjectPoint has it); false behind the
  // camera or right at it
  bool ToScreen(float const* t, float const* projection, XMVECTOR point, float& x, float& y)
  {
    XMVECTOR offset = point - XMVectorSet(t[12], t[13], t[14], 0.f);
    float right = XMVectorGetX(XMVector3Dot(offset, XMVectorSet(t[0], t[1], t[2], 0.f)));
    float up = XMVectorGetX(XMVector3Dot(offset, XMVectorSet(t[4], t[5], t[6], 0.f)));
    float depth = -XMVectorGetX(XMVector3Dot(offset, XMVectorSet(t[8], t[9], t[10], 0.f)));
    if (depth < 0.1f)
      return false;
    float px = right / depth, py = up / depth;
    panini::ToPanini(px, py);
    float scale = panini::ScreenScale();
    float nx = px * projection[0] / scale;
    float ny = py * projection[5] / scale;
    ImVec2 size = ImGui::GetIO().DisplaySize;
    x = (nx * 0.5f + 0.5f) * size.x;
    y = (0.5f - ny * 0.5f) * size.y;
    return true;
  }

  // Where the held light goes for the cursor at x, y: from base, across the
  // picture (the camera's right and up), along the ground (level), or only
  // up and down. Newton steps on ToScreen itself, as the camera's drawn
  // track path does it (CameraManager::PathDragPlace), so it ends up under
  // the cursor where its dot is drawn (the Panini view too); a step is only
  // taken when it brings it nearer, so where the cursor is somewhere it
  // can't go it stops as near as it gets instead of flying off.
  bool DragPlace(float const* t, float const* projection, float const* base, bool upDown, bool ground,
    float x, float y, float* place)
  {
    XMVECTOR origin = XMVectorSet(base[0], base[1], base[2], 0.f);
    XMVECTOR back = XMVector3Normalize(XMVectorSet(t[8], t[9], t[10], 0.f));
    float depth = -XMVectorGetX(XMVector3Dot(origin - XMVectorSet(t[12], t[13], t[14], 0.f), back));
    if (depth < 0.1f)
      return false;
    XMVECTOR axisA, axisB;
    if (upDown)
      axisA = axisB = XMVectorSet(0.f, 1.f, 0.f, 0.f);
    else if (ground)
    {
      axisA = XMVectorSet(1.f, 0.f, 0.f, 0.f);
      axisB = XMVectorSet(0.f, 0.f, 1.f, 0.f);
    }
    else
    {
      axisA = XMVector3Normalize(XMVectorSet(t[0], t[1], t[2], 0.f));
      axisB = XMVector3Normalize(XMVectorSet(t[4], t[5], t[6], 0.f));
    }
    float step = depth * 0.001f;
    float a = 0.f, b = 0.f;
    float sx, sy;
    if (!ToScreen(t, projection, origin, sx, sy))
      return false;
    float error = (x - sx) * (x - sx) + (y - sy) * (y - sy);
    for (int i = 0; i < 16 && error > 0.0025f; ++i)
    {
      XMVECTOR v = origin + axisA * a + axisB * b;
      float ax, ay, bx = 0.f, by = 0.f;
      if (!ToScreen(t, projection, v + axisA * step, ax, ay) ||
          (!upDown && !ToScreen(t, projection, v + axisB * step, bx, by)))
        break;
      float ex = x - sx, ey = y - sy;
      float j11 = (ax - sx) / step, j21 = (ay - sy) / step;  // pixels a metre
      float da, db = 0.f;
      if (upDown)
      {
        // As near the cursor as the line up and down comes
        float jj = j11 * j11 + j21 * j21;
        if (jj < 1e-8f)
          break;
        da = (j11 * ex + j21 * ey) / jj;
      }
      else
      {
        float j12 = (bx - sx) / step, j22 = (by - sy) / step;
        float det = j11 * j22 - j12 * j21;
        if (fabsf(det) < 1e-8f)
          break;
        da = (j22 * ex - j12 * ey) / det;
        db = (j11 * ey - j21 * ex) / det;
      }
      // No more than its distance from the camera at a time
      float length = sqrtf(da * da + db * db);
      if (length > depth)
      {
        da *= depth / length;
        db *= depth / length;
      }
      bool nearer = false;
      for (int halve = 0; halve < 6 && !nearer; ++halve, da *= 0.5f, db *= 0.5f)
      {
        float nx, ny;
        if (!ToScreen(t, projection, v + axisA * da + axisB * db, nx, ny))
          continue;
        float e = (x - nx) * (x - nx) + (y - ny) * (y - ny);
        if (e < error)
        {
          a += da;
          b += db;
          sx = nx;
          sy = ny;
          error = e;
          nearer = true;
        }
      }
      if (!nearer)
        break;
    }
    XMFLOAT3 at;
    XMStoreFloat3(&at, origin + axisA * a + axisB * b);
    place[0] = at.x;
    place[1] = at.y;
    place[2] = at.z;
    return true;
  }

  const char* LightName(LightManager::LightType type)
  {
    return type == LightManager::Spot ? "Spot" : "Sphere";
  }
}

LightManager::LightManager() :
  m_NextId(1),
  m_Selected(-1),
  m_Supported(false)
{
  m_pUpdateLight = util::offsets::GetOffset("OFFSET_UPDATELIGHT");
  m_ppGameRenderer = util::offsets::GetOffset("OFFSET_GAMERENDERER");
  m_Supported = CheckSupport();

  if (m_Supported)
    util::log::Ok("Light manager available");
  else
    util::log::Warning("Light manager disabled: %s", m_UnsupportedReason.c_str());
}

LightManager::~LightManager()
{
  // Light memory is never freed, the game may still point at it
}

bool LightManager::CheckSupport()
{
  if (!m_pUpdateLight || !m_ppGameRenderer)
  {
    m_UnsupportedReason = "OFFSET_UPDATELIGHT or OFFSET_GAMERENDERER wasn't found";
    return false;
  }

  IMAGE_DOS_HEADER* pDos = (IMAGE_DOS_HEADER*)g_gameHandle;
  IMAGE_NT_HEADERS* pNt = (IMAGE_NT_HEADERS*)((BYTE*)g_gameHandle + pDos->e_lfanew);
  if (pNt->FileHeader.TimeDateStamp != SupportedTimeStamp)
  {
    m_UnsupportedReason = "the game build isn't the one the original tools' light addresses are for";
    return false;
  }

  // The vtables must point at code inside the exe
  MODULEINFO info;
  if (!GetModuleInformation(GetCurrentProcess(), g_gameHandle, &info, sizeof(MODULEINFO)))
  {
    m_UnsupportedReason = "GetModuleInformation failed";
    return false;
  }

  __int64 start = (__int64)info.lpBaseOfDll;
  __int64 end = start + info.SizeOfImage;
  for (__int64 rva : { SphereEntityVTable, SphereDataVTable, SpotEntityVTable, SpotDataVTable, EntityInterface })
  {
    __int64* pVTable = (__int64*)(start + rva);
    for (int i = 0; i < 4; ++i)
    {
      if (pVTable[i] < start || pVTable[i] >= end)
      {
        m_UnsupportedReason = "a light vtable doesn't look right";
        return false;
      }
    }
  }

  return true;
}

bool LightManager::GetCameraTransform(float transform[16])
{
  float projection[16];
  return ReadMainView(m_ppGameRenderer, transform, projection);
}

BYTE* LightManager::BuildLight(LightType type, Light const* pCopy, BYTE*& pData)
{
  // Same layout as the original tools' constructors (sub_18000a8e0 /
  // sub_18000abc0 in CT_MirrorsEdge.dll), which match the game's own
  // (0x14375F620, 0x143760520)
  __int64 base = (__int64)g_gameHandle;
  int entitySize = (type == Spot) ? SpotEntitySize : SphereEntitySize;

  BYTE* e = new BYTE[entitySize]();
  BYTE* d = new BYTE[DataSize]();

  // Light data
  At<__int64>(d, 0x00) = base + ((type == Spot) ? SpotDataVTable : SphereDataVTable);
  memcpy(d + 0x20, Identity, 64);
  At<BYTE>(d, 0x60) = 1;
  for (int offset : { ColorOffset, ParticleColorOffset, RadiosityColorOffset })
  {
    At<float>(d, offset + 0) = 1.f;
    At<float>(d, offset + 4) = 1.f;
    At<float>(d, offset + 8) = 1.f;
  }
  At<int>(d, TemperatureOffset) = 4000;
  At<float>(d, IntensityOffset) = type == Spot ? SpotIntensity : SphereIntensity;
  At<float>(d, AttenuationRadiusOffset) = NewLightRadius;
  At<float>(d, ShadowCullDistanceOffset) = ShadowCullDistance;
  At<float>(d, ShadowDimmerOffset) = 1.f;
  At<float>(d, DimmerOffset) = 1.f;
  At<BYTE>(d, DiffuseOffset) = 1;
  At<BYTE>(d, SpecularOffset) = 1;
  At<BYTE>(d, RadiosityOffset) = 1;
  if (type == Spot)
  {
    At<float>(d, OuterAngleOffset) = 80.f;
    At<float>(d, 0x108) = 1.f;   // IESMultiplier
    At<BYTE>(d, 0x10C) = 1;      // UseIESProfileAsMask (no profile)
  }
  else
    At<float>(d, 0x100) = 1.f;   // IESMultiplier
  // A copy: all of the other's settings (the data has no pointers but the
  // vtable, the same for the same type)
  if (pCopy)
    memcpy(d + 0x08, pCopy->pData + 0x08, DataSize - 0x08);

  // Entity
  At<__int64>(e, 0x00) = base + ((type == Spot) ? SpotEntityVTable : SphereEntityVTable);
  At<WORD>(e, 0x18) = 0x121E;
  At<BYTE*>(e, 0x28) = d;
  memcpy(e + EntityTransformOffset, Identity, 64);
  At<__int64>(e, 0x80) = 0xFFFF;   // no render light yet
  At<WORD>(e, 0xA0) = 0x0101;      // enabled
  At<BYTE>(e, 0xA4) = 1;
  At<float>(e, 0xA8) = 1.f;        // a brightness factor (the game's constructor sets 1 too)
  At<__int64>(e, 0xB0) = base + EntityInterface;
  if (pCopy)
    At<BYTE>(e, EntityEnabledOffset) = At<BYTE>(pCopy->pEntity, EntityEnabledOffset);

  // Property table, pointers into the light data
  const int table[][2] = {
    { 0xC0, 0x70 }, { 0xD0, 0xA8 }, { 0xE0, 0xAC }, { 0xF0, 0xCC }, { 0x100, 0xE1 }, { 0x110, 0xE2 },
    { 0x120, 0xE3 }, { 0x130, 0xE4 }, { 0x140, 0xE5 }, { 0x150, 0x90 }, { 0x160, 0xD0 }, { 0x170, 0x80 },
    { 0x180, 0x20 }, { 0x190, 0xD4 }, { 0x1A0, 0xD8 }, { 0x218, 0xF0 } };
  for (auto const& entry : table)
    At<BYTE*>(e, entry[0]) = d + entry[1];

  if (type == Spot)
  {
    At<BYTE*>(e, 0x228) = d + 0xF4;
    At<BYTE*>(e, 0x238) = d + 0xF8;
    At<BYTE*>(e, 0x248) = d + 0x108;
    At<int>(e, 0x200) = 1;         // the shape: spot
  }
  else
    At<BYTE*>(e, 0x228) = d + 0x100;

  At<float>(e, 0x204) = 1.f;       // another brightness factor, 1 in the game's constructor
  // The shadow's "refresh when needed" countdown (0x14376BE50); the game's
  // own lights count it down, these keep it, so their shadow is redrawn with
  // every change (the original tools set it too)
  At<int>(e, 0x20C) = 3;

  pData = d;
  return e;
}

void LightManager::CreateLight(LightType type, Light const* pCopy)
{
  BYTE* d;
  BYTE* e = BuildLight(type, pCopy, d);
  Light light{ m_NextId++, type, e, d, false, {}, false, {}, false };
  if (pCopy)
  {
    sprintf_s(light.Name, "%.38s copy", pCopy->Name);
    light.CustomName = true;
  }
  else
  {
    sprintf_s(light.Name, "%s light %d", LightName(type), light.Id);
    light.PlaceOnSurface = true;
  }

  // A copy goes where the other is, half a metre to the camera's right; a
  // new light a little in front of the camera, pointing the way it looks
  // (then on what the camera looks at, PlaceOnSurfaces, before it's sent)
  float transform[16];
  if (pCopy)
  {
    memcpy(transform, pCopy->pEntity + EntityTransformOffset, 64);
    float view[16];
    bool viewOk = GetCameraTransform(view);
    for (int i = 0; i < 3; ++i)
      transform[12 + i] += 0.5f * (viewOk ? view[i] : (i == 0 ? 1.f : 0.f));
    memcpy(e + EntityTransformOffset, transform, 64);
    memcpy(d + 0x20, transform, 64);
  }
  else if (GetCameraTransform(transform))
  {
    for (int i = 0; i < 3; ++i)
      transform[12 + i] -= transform[8 + i] * NewLightAhead;
    memcpy(e + EntityTransformOffset, transform, 64);
    memcpy(d + 0x20, transform, 64);
  }

  m_Lights.push_back(light);
  m_Selected = (int)m_Lights.size() - 1;
  QueueUpdate(m_Lights.back());

  util::log::Write("Created %s light %d (\"%s\")", type == Spot ? "spot" : "sphere", light.Id, light.Name);
}

void LightManager::RemoveLight(Light& light)
{
  // Off (Enabled, + 0xA0: the game's build then gives no light, 0x14376534C,
  // and its update takes the render light away, 0x143762CF0), then updated
  // once more. The original cleared + 0xA0 .. + 0xA3, which took + 0xA2
  // (still queued in the game) with it: the off check in OnGameUpdate then
  // looked at a render light the game hadn't caught up with yet.
  if (m_DragId == light.Id)
    EndDrag();
  if (m_AimId == light.Id)
    EndAim();
  At<BYTE>(light.pEntity, EntityEnabledOffset) = 0;
  light.Removed = true;
  QueueUpdate(light);
  util::log::Write("Removed light %d (\"%s\")", light.Id, light.Name);
}

void LightManager::MoveToCamera(Light& light)
{
  float transform[16];
  if (!GetCameraTransform(transform))
    return;

  memcpy(light.pEntity + EntityTransformOffset, transform, 64);
  memcpy(light.pData + 0x20, transform, 64);
  QueueUpdate(light);
}

void LightManager::BringInFront(Light& light)
{
  float transform[16];
  if (!GetCameraTransform(transform))
    return;
  float place[3];
  for (int i = 0; i < 3; ++i)
    place[i] = transform[12 + i] - transform[8 + i] * NewLightAhead;
  SetPosition(light, place);
}

void LightManager::AimLikeCamera(Light& light)
{
  // The camera's turn, the light where it is
  float transform[16];
  if (!GetCameraTransform(transform))
    return;
  memcpy(light.pEntity + EntityTransformOffset, transform, 48);
  memcpy(light.pData + 0x20, transform, 48);
  QueueUpdate(light);
}

void LightManager::ApplyLook(Light& light, int look)
{
  Look const& l = (light.Type == Spot ? SpotLooks : SphereLooks)[look];
  BYTE* d = light.pData;
  At<int>(d, LightColorModeOffset) = 1;
  At<int>(d, TemperatureOffset) = l.Kelvin;
  TemperatureColor(l.Kelvin, &At<float>(d, ColorOffset));
  At<int>(d, LightUnitOffset) = 0;
  At<float>(d, IntensityOffset) = l.Lumens;
  At<float>(d, DimmerOffset) = 1.f;
  At<float>(d, AttenuationRadiusOffset) = l.Reach;
  if (light.Type == Spot)
  {
    At<float>(d, OuterAngleOffset) = l.Cone;
    At<float>(d, InnerAngleOffset) = l.Cone * (1.f - l.Softness);
  }
  // Named after it, unless it was given a name
  if (!light.CustomName)
    sprintf_s(light.Name, "%s %d", l.Name, light.Id);
  QueueUpdate(light);
  util::log::Write("Light %d: the \"%s\" look", light.Id, l.Name);
}

void LightManager::SetPosition(Light& light, float const* position)
{
  for (int i = 0; i < 3; ++i)
  {
    At<float>(light.pEntity, EntityTransformOffset + 0x30 + i * 4) = position[i];
    At<float>(light.pData, 0x20 + 0x30 + i * 4) = position[i];
  }
  QueueUpdate(light);
}

void LightManager::SetAim(Light& light, float const* forward)
{
  // Rows right, up, backward (the camera's way: right x up = backward, and a
  // spot shines along -backward), the light's place kept. Level right where
  // it can be; straight up or down, its right as it was.
  XMVECTOR back = -XMVector3Normalize(XMVectorSet(forward[0], forward[1], forward[2], 0.f));
  float* m = &At<float>(light.pEntity, EntityTransformOffset);
  XMVECTOR right = XMVector3Cross(XMVectorSet(0.f, 1.f, 0.f, 0.f), back);
  if (XMVectorGetX(XMVector3Length(right)) < 1e-3f)
  {
    XMVECTOR old = XMVectorSet(m[0], m[1], m[2], 0.f);
    right = old - back * XMVector3Dot(old, back);
    if (XMVectorGetX(XMVector3Length(right)) < 1e-3f)
      right = XMVectorSet(1.f, 0.f, 0.f, 0.f);
  }
  right = XMVector3Normalize(right);
  XMVECTOR up = XMVector3Cross(back, right);
  XMVECTOR rows[3] = { right, up, back };
  for (int r = 0; r < 3; ++r)
  {
    XMFLOAT4 row;
    XMStoreFloat4(&row, XMVectorSetW(rows[r], 0.f));
    memcpy(light.pEntity + EntityTransformOffset + r * 16, &row, 16);
    memcpy(light.pData + 0x20 + r * 16, &row, 16);
  }
  QueueUpdate(light);
}

void LightManager::EndAim()
{
  if (!m_AimId)
    return;
  Light* pLight = FindLight(m_AimId);
  if (pLight && m_AimMoved)
  {
    float const* m = &At<float>(pLight->pEntity, EntityTransformOffset);
    util::log::Write("Light %d (\"%s\") aimed along %.3f %.3f %.3f", pLight->Id, pLight->Name, -m[8], -m[9], -m[10]);
    PushMoveUndo(*pLight, m_AimBefore, "aiming");
  }
  m_AimId = 0;
  m_AimMoved = false;
}

void LightManager::PlaceOnSurfaces()
{
  // Game thread, under m_Mutex: collision::Sweep is the free camera's, only
  // for the camera hook's thread. A light asked to go on what the camera
  // looks at is put there before it's sent (it stays where it is when
  // nothing is within SurfaceReach).
  for (Light& light : m_Lights)
  {
    if (!light.PlaceOnSurface || light.Removed)
      continue;
    light.PlaceOnSurface = false;
    float t[16];
    if (!GetCameraTransform(t))
      continue;
    XMVECTOR from = XMVectorSet(t[12], t[13], t[14], 0.f);
    XMVECTOR look = -XMVector3Normalize(XMVectorSet(t[8], t[9], t[10], 0.f));
    XMFLOAT3 a, b;
    XMStoreFloat3(&a, from);
    XMStoreFloat3(&b, from + look * SurfaceReach);
    float fraction = 1.f;
    float normal[3] = { 0.f, 0.f, 0.f };
    if (!collision::Sweep(&a.x, &b.x, 0.f, &fraction, normal))
    {
      util::log::Write("Light %d (\"%s\"): nothing within %.0f m where the camera looks, it stays %.0f m in front",
        light.Id, light.Name, SurfaceReach, NewLightAhead);
      continue;
    }
    float distance = fraction * SurfaceReach;
    XMVECTOR hit = from + look * distance;
    float radius = At<float>(light.pData, AttenuationRadiusOffset);
    XMVECTOR place;
    if (distance < SurfaceNearest)
      place = from + look * (distance * 0.5f);
    else if (light.Type == Spot)
      // Back toward the camera, still pointing the way it looks
      place = hit - look * (std::min)(radius * SurfaceSpotShare, distance * 0.5f);
    else
    {
      // Off the surface along its normal, on the camera's side (toward the
      // camera when it has none)
      XMVECTOR n = XMVectorSet(normal[0], normal[1], normal[2], 0.f);
      float length = XMVectorGetX(XMVector3Length(n));
      n = length > 0.5f ? n / length : -look;
      if (XMVectorGetX(XMVector3Dot(n, look)) > 0.f)
        n = -n;
      place = hit + n * (std::min)(radius * SurfaceSphereShare, distance * 0.5f);
    }
    XMFLOAT3 p;
    XMStoreFloat3(&p, place);
    SetPosition(light, &p.x);
    util::log::Write("Light %d (\"%s\") put on what the camera looks at, %.1f m away: %.2f %.2f %.2f", light.Id,
      light.Name, distance, p.x, p.y, p.z);
  }
}

LightManager::UndoItem LightManager::SnapshotOf(Light const& light)
{
  UndoItem item{};
  item.Id = light.Id;
  item.Type = light.Type;
  memcpy(item.Name, light.Name, sizeof(item.Name));
  item.CustomName = light.CustomName;
  memcpy(item.Transform, light.pEntity + EntityTransformOffset, sizeof(item.Transform));
  memcpy(item.Data, light.pData, sizeof(item.Data));
  item.Enabled = At<BYTE>(light.pEntity, EntityEnabledOffset);
  return item;
}

void LightManager::PushUndo(UndoStep const& step)
{
  m_Undo.push_back(step);
  if (m_Undo.size() > MaxUndo)
    m_Undo.erase(m_Undo.begin());
}

void LightManager::PushMoveUndo(Light const& light, float const* before, char const* what)
{
  UndoStep step{};
  step.Removal = false;
  sprintf_s(step.Label, "%s \"%.40s\"", what, light.Name);
  UndoItem item = SnapshotOf(light);
  memcpy(item.Transform, before, sizeof(item.Transform));
  step.Items.push_back(item);
  PushUndo(step);
}

void LightManager::Undo()
{
  if (m_Undo.empty())
    return;
  UndoStep step = m_Undo.back();
  m_Undo.pop_back();
  // Any drag or aim stops here without a step of its own (the Undo button is
  // clicked on the menu, so none is going on)
  m_DragId = 0;
  m_DragMoved = false;
  m_AimId = 0;
  m_AimMoved = false;
  m_PlaceEditOpen = false;
  int restored = 0;
  for (UndoItem const& item : step.Items)
  {
    if (!step.Removal)
    {
      // A move: back where it was (its other settings as they are now)
      Light* pLight = FindLight(item.Id);
      if (!pLight)
        continue;
      memcpy(pLight->pEntity + EntityTransformOffset, item.Transform, sizeof(item.Transform));
      memcpy(pLight->pData + 0x20, item.Transform, sizeof(item.Transform));
      QueueUpdate(*pLight);
      for (int i = 0; i < (int)m_Lights.size(); ++i)
        if (m_Lights[i].Id == item.Id)
          m_Selected = i;
      ++restored;
      continue;
    }
    // A removal: made again as it was, with its own id and name (a new
    // entity: the old one stays the game's to let go)
    if (FindLight(item.Id))
      continue;
    BYTE* d;
    BYTE* e = BuildLight(item.Type, nullptr, d);
    memcpy(d + 0x08, item.Data + 0x08, DataSize - 0x08);
    memcpy(e + EntityTransformOffset, item.Transform, sizeof(item.Transform));
    memcpy(d + 0x20, item.Transform, sizeof(item.Transform));
    At<BYTE>(e, EntityEnabledOffset) = item.Enabled;
    Light light{ item.Id, item.Type, e, d, false, {}, item.CustomName, {}, false };
    memcpy(light.Name, item.Name, sizeof(light.Name));
    m_Lights.push_back(light);
    m_Selected = (int)m_Lights.size() - 1;
    QueueUpdate(m_Lights.back());
    ++restored;
  }
  util::log::Write("Lights: undone, %s (%d light%s)", step.Label, restored, restored == 1 ? "" : "s");
}

LightManager::Light* LightManager::FindLight(int id)
{
  for (Light& light : m_Lights)
    if (light.Id == id && !light.Removed)
      return &light;
  return nullptr;
}

void LightManager::QueueEntity(BYTE* pEntity)
{
  for (BYTE* p : m_PendingUpdates)
    if (p == pEntity) return;
  m_PendingUpdates.push_back(pEntity);
  m_HasPending = true;
}

void LightManager::QueueUpdate(Light& light)
{
  QueueEntity(light.pEntity);
}

LightManager::HandleCheck* LightManager::CheckOf(BYTE* pEntity)
{
  for (Light& light : m_Lights)
    if (light.pEntity == pEntity)
      return &light.Check;
  for (FixedLight& fixed : m_FixedLights)
    if (fixed.pEntity == pEntity)
      return &fixed.Check;
  return nullptr;
}

int LightManager::AddFixedLight(LightType type, float const rows[16], float const color[3], int temperatureK,
  float intensity, float attenuationRadius)
{
  if (!m_Supported)
    return 0;
  std::lock_guard<std::mutex> lock(m_Mutex);
  BYTE* d;
  BYTE* e = BuildLight(type, nullptr, d);
  memcpy(e + EntityTransformOffset, rows, 64);
  memcpy(d + 0x20, rows, 64);
  int kelvin = temperatureK < MinTemperature ? MinTemperature : (temperatureK > MaxTemperature ? MaxTemperature : temperatureK);
  At<int>(d, TemperatureOffset) = kelvin;
  At<int>(d, LightColorModeOffset) = color ? 0 : 1;
  if (color)
    memcpy(&At<float>(d, ColorOffset), color, 3 * sizeof(float));
  else
    TemperatureColor(kelvin, &At<float>(d, ColorOffset));
  At<float>(d, IntensityOffset) = intensity;
  At<float>(d, AttenuationRadiusOffset) = attenuationRadius;
  FixedLight fixed{ m_NextFixedId++, type, e, d, {}, false };
  m_FixedLights.push_back(fixed);
  QueueEntity(e);
  util::log::Write("Fixed %s light %d made at %.2f %.2f %.2f (%.0f lm, %.1f m)", type == Spot ? "spot" : "sphere",
    fixed.Id, rows[12], rows[13], rows[14], intensity, attenuationRadius);
  return fixed.Id;
}

void LightManager::RemoveFixedLight(int id)
{
  if (!m_Supported)
    return;
  std::lock_guard<std::mutex> lock(m_Mutex);
  for (FixedLight& fixed : m_FixedLights)
  {
    if (fixed.Id != id || fixed.Removed)
      continue;
    // Off, as a menu light's Remove; sent with the next update, then off the
    // list (the entity is never freed, the game may still point at it)
    At<BYTE>(fixed.pEntity, EntityEnabledOffset) = 0;
    fixed.Removed = true;
    QueueEntity(fixed.pEntity);
    util::log::Write("Fixed light %d removed", id);
    return;
  }
}

bool LightManager::SetFixedLightIntensity(int id, float intensity)
{
  if (!m_Supported)
    return false;
  // As the menu keeps a light's (NaN to 0)
  intensity = !(intensity >= 0.f) ? 0.f : (intensity > MaxIntensity ? MaxIntensity : intensity);
  std::lock_guard<std::mutex> lock(m_Mutex);
  for (FixedLight& fixed : m_FixedLights)
  {
    if (fixed.Id != id || fixed.Removed)
      continue;
    float& now = At<float>(fixed.pData, IntensityOffset);
    if (now != intensity)
    {
      now = intensity;
      QueueEntity(fixed.pEntity);
    }
    return true;
  }
  return false;
}

void LightManager::CheckHandle(BYTE* pEntity, BYTE* pData, HandleCheck& check, char const* name, ULONGLONG now)
{
  // Still "queued" long after it was sent: the game's full queue dropped it
  // (see QueuedStuckMs). The byte is let go and it's sent again.
  if (At<BYTE>(pEntity, EntityQueuedOffset) && now - check.ChangedAt >= QueuedStuckMs)
  {
    At<BYTE>(pEntity, EntityQueuedOffset) = 0;
    check.Misses = 0;
    QueueEntity(pEntity);
    util::log::Write("Lights: %s was still waiting in the game's light queue after %llu s (dropped from it), "
      "it's sent again", name, (now - check.ChangedAt) / 1000);
    return;
  }
  // Only a light the game has (a handle), has taken its last change (not in
  // its queue) and that hasn't changed for a while: its render light then
  // has its place and radius
  WORD handle = At<WORD>(pEntity, EntityHandleOffset);
  if (handle == NoHandle || At<BYTE>(pEntity, EntityQueuedOffset) || now - check.ChangedAt < HandleQuietMs)
  {
    check.Misses = 0;
    return;
  }
  int ours = HandleIsOurs(handle, &At<float>(pEntity, EntityTransformOffset + 0x30),
    At<float>(pData, AttenuationRadiusOffset), nullptr, 0.f, At<int>(pEntity, EntityShapeOffset));
  if (ours != 0)
  {
    check.Misses = 0;
    return;
  }
  if (++check.Misses < HandleMissesToRemake)
    return;
  // The game dropped it (or gave its handle to another light): made again
  // with the next update
  check.Misses = 0;
  At<WORD>(pEntity, EntityHandleOffset) = NoHandle;
  QueueEntity(pEntity);
  util::log::Write("Lights: %s was gone from the game's lights (render light %u), it's made again", name, handle);
}

void LightManager::CheckHandles()
{
  ULONGLONG now = GetTickCount64();
  m_HandlesCheckedAt = now;
  char name[64];
  for (Light& light : m_Lights)
  {
    if (light.Removed)
      continue;
    sprintf_s(name, "light %d (\"%.40s\")", light.Id, light.Name);
    CheckHandle(light.pEntity, light.pData, light.Check, name, now);
  }
  for (FixedLight& fixed : m_FixedLights)
  {
    if (fixed.Removed)
      continue;
    sprintf_s(name, "fixed light %d", fixed.Id);
    CheckHandle(fixed.pEntity, fixed.pData, fixed.Check, name, now);
  }
  CheckLeaving(now);
}

bool LightManager::LeavingName(BYTE* pEntity, char* name, size_t size)
{
  for (Light const& light : m_Lights)
    if (light.pEntity == pEntity)
    {
      if (light.Removed)
        sprintf_s(name, size, "light %d (\"%.40s\")", light.Id, light.Name);
      return light.Removed;
    }
  for (FixedLight const& fixed : m_FixedLights)
    if (fixed.pEntity == pEntity)
    {
      if (fixed.Removed)
        sprintf_s(name, size, "fixed light %d", fixed.Id);
      return fixed.Removed;
    }
  return false;
}

void LightManager::CheckLeaving(ULONGLONG now)
{
  // Each removed light, once the game has had time to take its off: gone
  // when the game has taken its handle back (+ 0x80 0xFFFF, which its update
  // writes as it takes the render light away, 0x143762D0A) and the render
  // light it had is free or another's (HandleIsOurs). What came of the last
  // ones is shown in the tab.
  for (size_t i = 0; i < m_Leaving.size();)
  {
    Leaving& leaving = m_Leaving[i];
    ULONGLONG age = now - leaving.SentAt;
    bool queued = At<BYTE>(leaving.pEntity, EntityQueuedOffset) != 0;
    if (age < LeavingWaitMs || (queued && age < LeavingGiveUpMs))
    {
      ++i;
      continue;
    }
    bool handleBack = At<WORD>(leaving.pEntity, EntityHandleOffset) == NoHandle;
    int ours = leaving.Handle == NoHandle ? 0 :
      HandleIsOurs(leaving.Handle, leaving.Place, leaving.Radius, nullptr, 0.f, leaving.Shape);
    if (handleBack && ours == 0)
    {
      if (leaving.Handle == NoHandle)
        util::log::Write("Lights: %s is off (it had no render light in the game)", leaving.Name);
      else
        util::log::Write("Lights: %s is off in the game (its render light %u is let go)", leaving.Name,
          leaving.Handle);
      ++m_LeftOff;
    }
    else if (age < LeavingGiveUpMs)
    {
      ++i;
      continue;
    }
    else
    {
      util::log::Warning("Lights: %s may still be lit %llu s after it was removed (its handle %s, its render light "
        "%u %s)", leaving.Name, age / 1000, handleBack ? "taken back" : "not taken back", leaving.Handle,
        ours == 1 ? "still its own" : ours == 0 ? "let go" : "couldn't be read");
      ++m_LeftLit;
      strcpy_s(m_LeftLitName, leaving.Name);
    }
    m_LeftAt = now;
    m_Leaving.erase(m_Leaving.begin() + i);
  }
}

void LightManager::OnGameUpdate()
{
  if (!m_Supported) return;

  // The camera for the dots, while they can be drawn
  if (g_mainHandle->GetUI()->IsEnabled())
  {
    View view;
    bool ok = ReadMainView(m_ppGameRenderer, view.Transform, view.Projection);
    std::lock_guard<std::mutex> lock(m_ViewMutex);
    m_ViewOk = ok;
    if (ok)
      m_View = view;
  }

  // Called for every render view, so skip the lock when there's nothing to
  // do: no change to send and no handle check due
  ULONGLONG now = GetTickCount64();
  bool checkDue = now - m_HandlesCheckedAt >= HandleCheckMs;
  if (!m_HasPending && !checkDue) return;

  std::lock_guard<std::mutex> lock(m_Mutex);
  // New lights (and "Where the camera looks") onto what the camera looks at
  PlaceOnSurfaces();
  // Lights the game dropped are queued to be made again
  if (checkDue)
    CheckHandles();
  m_HasPending = false;
  if (m_PendingUpdates.empty()) return;

  for (BYTE* pEntity : m_PendingUpdates)
  {
    HandleCheck* pCheck = CheckOf(pEntity);
    float* place = &At<float>(pEntity, EntityTransformOffset + 0x30);
    float radius = At<float>(At<BYTE*>(pEntity, 0x28), AttenuationRadiusOffset);
    // Still "queued" long after its last send: dropped by the game's full
    // queue (see QueuedStuckMs), so UpdateLight below would do nothing
    if (At<BYTE>(pEntity, EntityQueuedOffset) && pCheck && now - pCheck->ChangedAt >= QueuedStuckMs)
    {
      At<BYTE>(pEntity, EntityQueuedOffset) = 0;
      util::log::Write("Lights: a light was still waiting in the game's light queue after %llu s (dropped from it), "
        "it's sent again", (now - pCheck->ChangedAt) / 1000);
    }
    // A light going off (Remove, its On box) only goes through a handle
    // that's still its own, or the game would take another light off (after
    // a full rebuild of its lights). Looked at twice, it's taken as gone only
    // if both say so; the place and radius it was last sent with count too.
    WORD handle = At<WORD>(pEntity, EntityHandleOffset);
    if (!At<BYTE>(pEntity, EntityEnabledOffset) && handle != NoHandle && !At<BYTE>(pEntity, EntityQueuedOffset))
    {
      auto ours = [&]() { return HandleIsOurs(handle, place, radius, pCheck ? pCheck->SentPlace : nullptr,
        pCheck ? pCheck->SentRadius : 0.f, At<int>(pEntity, EntityShapeOffset)); };
      if (ours() == 0 && ours() == 0)
      {
        At<WORD>(pEntity, EntityHandleOffset) = NoHandle;
        util::log::Write("Lights: a light going off was already gone from the game's lights (render light %u)",
          handle);
      }
    }
    // A light removed for good: looked at until the game has let it go
    // (CheckLeaving); a new batch starts what the tab shows afresh
    char leavingName[64];
    if (LeavingName(pEntity, leavingName, sizeof(leavingName)))
    {
      if (m_Leaving.empty())
      {
        m_LeftOff = m_LeftLit = 0;
        m_LeftLitName[0] = 0;
      }
      Leaving leaving{ pEntity, At<WORD>(pEntity, EntityHandleOffset), { place[0], place[1], place[2] }, radius,
        At<int>(pEntity, EntityShapeOffset), now, {} };
      strcpy_s(leaving.Name, leavingName);
      m_Leaving.push_back(leaving);
    }
    if (pCheck)
    {
      pCheck->ChangedAt = now;
      pCheck->Misses = 0;
      memcpy(pCheck->SentPlace, place, sizeof(pCheck->SentPlace));
      pCheck->SentRadius = radius;
    }
    if (!SafeUpdateLight(m_pUpdateLight, pEntity))
    {
      // Most likely a light from before a level change. Stop touching it.
      util::log::Warning("The game couldn't update a light, it was taken off the list");
      for (Light& light : m_Lights)
        if (light.pEntity == pEntity)
          light.Removed = true;
    }
  }
  m_PendingUpdates.clear();

  // Removed lights stay allocated but leave the lists once their off is sent
  for (size_t i = 0; i < m_FixedLights.size();)
  {
    if (m_FixedLights[i].Removed)
      m_FixedLights.erase(m_FixedLights.begin() + i);
    else
      ++i;
  }
  for (int i = 0; i < (int)m_Lights.size();)
  {
    if (m_Lights[i].Removed)
    {
      if (m_Lights[i].Id == m_DragId)
        m_DragId = 0;
      if (m_Lights[i].Id == m_AimId)
        m_AimId = 0;
      m_Lights.erase(m_Lights.begin() + i);
      // Keep the same light selected
      if (i < m_Selected)
        m_Selected--;
      else if (i == m_Selected)
        m_Selected = -1;
    }
    else
      ++i;
  }
}

void LightManager::EndDrag()
{
  if (!m_DragId)
    return;
  Light* pLight = FindLight(m_DragId);
  if (pLight && m_DragMoved)
  {
    float const* p = &At<float>(pLight->pEntity, EntityTransformOffset + 0x30);
    util::log::Write("Light %d (\"%s\") dragged to %.2f %.2f %.2f", pLight->Id, pLight->Name, p[0], p[1], p[2]);
    PushMoveUndo(*pLight, m_DragBefore, "moving");
  }
  m_DragId = 0;
  m_DragMoved = false;
}

bool LightManager::HoldsCtrl()
{
  ULONGLONG at = m_CursorOnDotAt.load();
  return at && GetTickCount64() - at < CursorOnDotMs && (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
}

bool LightManager::TakeOpenRequest()
{
  std::lock_guard<std::mutex> lock(m_Mutex);
  bool open = m_OpenRequest;
  m_OpenRequest = false;
  return open;
}

void LightManager::DrawOverlay()
{
  if (!m_Supported)
    return;
  std::lock_guard<std::mutex> lock(m_Mutex);
  bool viewOk;
  {
    std::lock_guard<std::mutex> viewLock(m_ViewMutex);
    viewOk = m_ViewOk;
    m_DrawView = m_View;
  }
  int listHoverId = m_ListHoverId;
  m_ListHoverId = 0;
  if (!viewOk || m_Lights.empty())
  {
    EndDrag();
    EndAim();
    return;
  }
  float const* t = m_DrawView.Transform;
  float const* projection = m_DrawView.Projection;
  ImGuiIO& io = ImGui::GetIO();
  float fontSize = ImGui::GetFontSize();
  XMVECTOR camera = XMVectorSet(t[12], t[13], t[14], 0.f);

  // The light held with the mouse follows the cursor (before the dots are
  // drawn, so its dot is drawn where it went); let go, the drag ends
  Light* pHeld = m_DragId ? FindLight(m_DragId) : nullptr;
  if (m_DragId && (!pHeld || !io.MouseDown[0]))
  {
    EndDrag();
    pHeld = nullptr;
  }
  if (pHeld)
  {
    XMVECTOR now = XMLoadFloat3((XMFLOAT3 const*)&At<float>(pHeld->pEntity, EntityTransformOffset + 0x30));
    // Shift held or let go: from where it is now, the other way
    bool upDown = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    float x, y;
    if (upDown != m_DragUpDown && ToScreen(t, projection, now, x, y))
    {
      m_DragUpDown = upDown;
      XMStoreFloat3((XMFLOAT3*)m_DragBase, now);
      m_DragOffsetX = x - io.MousePos.x;
      m_DragOffsetY = y - io.MousePos.y;
    }
    // The wheel: nearer or farther, along the line from the camera
    if (io.MouseWheel != 0.f)
    {
      XMVECTOR base = XMLoadFloat3((XMFLOAT3 const*)m_DragBase);
      XMVECTOR away = base - camera;
      float distance = XMVectorGetX(XMVector3Length(away));
      if (distance > 1e-3f)
      {
        float wanted = distance * powf(WheelStep, io.MouseWheel);
        wanted = wanted < DragNearest ? DragNearest : (wanted > DragFarthest ? DragFarthest : wanted);
        XMStoreFloat3((XMFLOAT3*)m_DragBase, camera + away * (wanted / distance));
      }
      io.MouseWheel = 0.f;
    }
    // Only once it moves: a click that doesn't move it changes nothing
    float place[3];
    if (DragPlace(t, projection, m_DragBase, m_DragUpDown, m_DragAlongGround, io.MousePos.x + m_DragOffsetX,
        io.MousePos.y + m_DragOffsetY, place) &&
        XMVectorGetX(XMVector3Length(XMVectorSet(place[0], place[1], place[2], 0.f) - now)) > 1e-4f)
    {
      SetPosition(*pHeld, place);
      m_DragMoved = true;
    }
  }

  // The spot held by its aiming handle turns to the cursor: the handle goes
  // across the picture at the distance it was taken at (the wheel: nearer or
  // farther), the spot points from where it is to it; let go, it ends
  Light* pAimed = m_AimId ? FindLight(m_AimId) : nullptr;
  if (m_AimId && (!pAimed || pAimed->Type != Spot || !io.MouseDown[0]))
  {
    EndAim();
    pAimed = nullptr;
  }
  if (pAimed)
  {
    if (io.MouseWheel != 0.f)
    {
      XMVECTOR base = XMLoadFloat3((XMFLOAT3 const*)m_AimBase);
      XMVECTOR away = base - camera;
      float distance = XMVectorGetX(XMVector3Length(away));
      if (distance > 1e-3f)
      {
        float wanted = distance * powf(WheelStep, io.MouseWheel);
        wanted = wanted < DragNearest ? DragNearest : (wanted > DragFarthest ? DragFarthest : wanted);
        XMStoreFloat3((XMFLOAT3*)m_AimBase, camera + away * (wanted / distance));
      }
      io.MouseWheel = 0.f;
    }
    float target[3];
    if (DragPlace(t, projection, m_AimBase, false, false, io.MousePos.x + m_AimOffsetX, io.MousePos.y + m_AimOffsetY,
        target))
    {
      float const* m = &At<float>(pAimed->pEntity, EntityTransformOffset);
      float forward[3] = { target[0] - m[12], target[1] - m[13], target[2] - m[14] };
      float length = sqrtf(forward[0] * forward[0] + forward[1] * forward[1] + forward[2] * forward[2]);
      // Only once it turns: a click that doesn't move it changes nothing
      float along = length > 1e-3f ? -(forward[0] * m[8] + forward[1] * m[9] + forward[2] * m[10]) / length : 1.f;
      if (length > 1e-3f && along < 0.99999f)
      {
        SetAim(*pAimed, forward);
        m_AimMoved = true;
      }
    }
  }

  // Where each light is on the screen
  struct Dot
  {
    ImVec2 At;
    bool Shown;
  };
  std::vector<Dot> dots(m_Lights.size());
  for (size_t i = 0; i < m_Lights.size(); ++i)
  {
    float x = 0.f, y = 0.f;
    XMVECTOR place = XMLoadFloat3((XMFLOAT3 const*)&At<float>(m_Lights[i].pEntity, EntityTransformOffset + 0x30));
    dots[i].Shown = !m_Lights[i].Removed && ToScreen(t, projection, place, x, y);
    dots[i].At = ImVec2(x, y);
  }

  // What the cursor is on, off the menu's windows: the nearest dot
  int hover = -1;
  if (!m_DragId && !m_AimId && !io.WantCaptureMouse)
  {
    float reach = fontSize * 0.7f;
    float best = reach * reach;
    for (size_t i = 0; i < dots.size(); ++i)
    {
      if (!dots[i].Shown)
        continue;
      float dx = dots[i].At.x - io.MousePos.x, dy = dots[i].At.y - io.MousePos.y;
      if (dx * dx + dy * dy <= best)
      {
        best = dx * dx + dy * dy;
        hover = (int)i;
      }
    }
  }

  // The wheel over a dot: brighter or dimmer; with Ctrl held, its light
  // radius bigger or smaller
  bool ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
  if (hover >= 0)
    m_CursorOnDotAt = GetTickCount64();
  if (hover >= 0 && io.MouseWheel != 0.f)
  {
    Light& light = m_Lights[hover];
    if (ctrl)
    {
      float& radius = At<float>(light.pData, AttenuationRadiusOffset);
      radius *= powf(WheelRadius, io.MouseWheel);
      radius = !(radius >= MinRadius) ? MinRadius : (radius > MaxRadius ? MaxRadius : radius);
    }
    else
    {
      float& intensity = At<float>(light.pData, IntensityOffset);
      intensity *= powf(WheelBrightness, io.MouseWheel);
      intensity = intensity < 0.01f ? 0.01f : (intensity > MaxIntensity ? MaxIntensity : intensity);
    }
    io.MouseWheel = 0.f;
    QueueUpdate(light);
  }

  // The picked spot's aiming handle: a point along its beam, a share of its
  // distance from the camera so it's on screen
  bool aimShown = false, aimHover = false;
  ImVec2 aimAt;
  XMVECTOR aimTip = XMVectorZero();
  if (m_Selected >= 0 && m_Selected < (int)m_Lights.size() && !m_Lights[m_Selected].Removed &&
      m_Lights[m_Selected].Type == Spot && dots[m_Selected].Shown)
  {
    float const* m = &At<float>(m_Lights[m_Selected].pEntity, EntityTransformOffset);
    XMVECTOR place = XMVectorSet(m[12], m[13], m[14], 0.f);
    XMVECTOR forward = -XMVector3Normalize(XMVectorSet(m[8], m[9], m[10], 0.f));
    float along = XMVectorGetX(XMVector3Length(place - camera)) * AimHandleShare;
    along = along < AimHandleMin ? AimHandleMin : (along > AimHandleMax ? AimHandleMax : along);
    aimTip = place + forward * along;
    float x, y;
    if (ToScreen(t, projection, aimTip, x, y))
    {
      aimShown = true;
      aimAt = ImVec2(x, y);
      // The dots come first where they're both under the cursor
      float dx = x - io.MousePos.x, dy = y - io.MousePos.y;
      float reach = fontSize * 0.6f;
      aimHover = hover < 0 && !m_DragId && !m_AimId && !io.WantCaptureMouse && dx * dx + dy * dy <= reach * reach;
    }
  }

  // A click on a dot (not on the menu): its light is picked in the Lights
  // tab and held. The click is the light's: taken off ImGui's input, so the
  // camera's drawn track path (drawn after) doesn't take it too.
  if (io.MouseClicked[0] && !io.MouseDownOwned[0] && !m_DragId && !m_AimId && aimHover)
  {
    // On the picked spot's aiming handle: it's turned with the mouse
    m_AimId = m_Lights[m_Selected].Id;
    m_AimMoved = false;
    memcpy(m_AimBefore, m_Lights[m_Selected].pEntity + EntityTransformOffset, sizeof(m_AimBefore));
    XMStoreFloat3((XMFLOAT3*)m_AimBase, aimTip);
    m_AimOffsetX = aimAt.x - io.MousePos.x;
    m_AimOffsetY = aimAt.y - io.MousePos.y;
    io.MouseClicked[0] = false;
  }
  if (io.MouseClicked[0] && !io.MouseDownOwned[0] && !m_DragId && !m_AimId && hover >= 0)
  {
    Light& light = m_Lights[hover];
    m_Selected = hover;
    m_OpenRequest = true;
    m_DragId = light.Id;
    m_DragMoved = false;
    memcpy(m_DragBefore, light.pEntity + EntityTransformOffset, sizeof(m_DragBefore));
    m_DragUpDown = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    memcpy(m_DragBase, &At<float>(light.pEntity, EntityTransformOffset + 0x30), sizeof(m_DragBase));
    m_DragOffsetX = dots[hover].At.x - io.MousePos.x;
    m_DragOffsetY = dots[hover].At.y - io.MousePos.y;
    io.MouseClicked[0] = false;
  }

  ImDrawList* pDrawList = ImGui::GetOverlayDrawList();
  const ImU32 shadow = IM_COL32(0, 0, 0, 170);
  const ImU32 white = IM_COL32(255, 255, 255, 255);
  const ImU32 faint = IM_COL32(255, 255, 255, 110);
  const ImU32 offColour = IM_COL32(140, 140, 140, 220);
  ImVec2 screen = io.DisplaySize;
  auto shadowedText = [&](ImVec2 at, ImU32 colour, const char* text)
  {
    pDrawList->AddText(ImVec2(at.x + 1, at.y + 1), shadow, text);
    pDrawList->AddText(at, colour, text);
  };
  for (size_t i = 0; i < m_Lights.size(); ++i)
  {
    if (!dots[i].Shown)
      continue;
    Light& light = m_Lights[i];
    ImVec2 c = dots[i].At;
    bool on = At<BYTE>(light.pEntity, EntityEnabledOffset) != 0;
    bool held = m_DragId == light.Id;
    bool selected = (int)i == m_Selected;
    bool listHovered = light.Id == listHoverId;   // its row in the menu's list is under the cursor
    float size = (held || (int)i == hover || listHovered) ? 1.3f : 1.f;
    float const* m = &At<float>(light.pEntity, EntityTransformOffset);
    XMVECTOR place = XMVectorSet(m[12], m[13], m[14], 0.f);
    float reach = At<float>(light.pData, AttenuationRadiusOffset);
    // Its light radius shown: the picked light's, and the one under the
    // cursor while Ctrl is held (Ctrl + the wheel changes it)
    bool showReach = selected || ((int)i == hover && ctrl);

    if (light.Type == Spot)
    {
      // A faint line out to where its light ends
      XMVECTOR forward = -XMVector3Normalize(XMVectorSet(m[8], m[9], m[10], 0.f));
      float x, y;
      if (showReach && ToScreen(t, projection, place + forward * reach, x, y))
      {
        pDrawList->AddLine(c, ImVec2(x, y), shadow, 3.f);
        pDrawList->AddLine(c, ImVec2(x, y), faint, 1.5f);
        pDrawList->AddCircle(ImVec2(x, y), fontSize * 0.2f, faint, 12, 1.5f);
      }
      // The picked spot: a line to its aiming handle, a square to drag
      if (selected && aimShown)
      {
        bool aimed = m_AimId == light.Id;
        float r = fontSize * ((aimed || aimHover) ? 0.26f : 0.19f);
        const ImU32 handleColour = IM_COL32(110, 200, 255, 240);
        pDrawList->AddLine(c, aimAt, shadow, 4.f);
        pDrawList->AddLine(c, aimAt, on ? white : offColour, 2.f);
        pDrawList->AddRectFilled(ImVec2(aimAt.x - r - 1.5f, aimAt.y - r - 1.5f), ImVec2(aimAt.x + r + 1.5f,
          aimAt.y + r + 1.5f), shadow);
        pDrawList->AddRectFilled(ImVec2(aimAt.x - r, aimAt.y - r), ImVec2(aimAt.x + r, aimAt.y + r), handleColour);
      }
      // Every other spot: a short line the way it shines
      else if (ToScreen(t, projection, place + forward * 0.5f, x, y))
      {
        float dx = x - c.x, dy = y - c.y;
        float length = sqrtf(dx * dx + dy * dy);
        if (length > 1.f)
        {
          float scale = fontSize * 1.8f / length;
          ImVec2 tip(c.x + dx * scale, c.y + dy * scale);
          pDrawList->AddLine(c, tip, shadow, 4.f);
          pDrawList->AddLine(c, tip, on ? white : offColour, 2.f);
        }
      }
    }
    else if (showReach)
    {
      // A faint ring about the size of its reach there, while the camera is
      // outside it
      XMVECTOR offset = place - camera;
      float depth = -XMVectorGetX(XMVector3Dot(offset, XMVectorSet(t[8], t[9], t[10], 0.f)));
      if (XMVectorGetX(XMVector3Length(offset)) > reach && depth > 0.1f)
      {
        float r = reach / depth * projection[5] / panini::ScreenScale() * screen.y * 0.5f;
        if (r > fontSize * 0.6f && r < screen.y * 3.f)
        {
          pDrawList->AddCircle(c, r, shadow, 64, 3.f);
          pDrawList->AddCircle(c, r, faint, 64, 1.5f);
        }
      }
    }

    // The dot: the light's colour (at full brightness), grey when it's off
    ImU32 fill = offColour;
    if (on)
    {
      float const* colour = &At<float>(light.pData, ColorOffset);
      float most = (std::max)((std::max)(colour[0], colour[1]), colour[2]);
      most = most > 0.f ? most : 1.f;
      fill = ImGui::ColorConvertFloat4ToU32(ImVec4(colour[0] / most, colour[1] / most, colour[2] / most, 0.95f));
    }
    pDrawList->AddCircleFilled(c, fontSize * 0.36f * size, shadow, 16);
    pDrawList->AddCircleFilled(c, fontSize * 0.26f * size, fill, 16);
    if (selected || listHovered)
    {
      pDrawList->AddCircle(c, fontSize * 0.5f * size, selected ? white : faint, 20, 2.f);
      // Its name beside it (the hovered or held one's is by the cursor)
      if (!held && (int)i != hover)
        shadowedText(ImVec2(c.x + fontSize * 0.7f, c.y - fontSize * 1.2f), white, light.Name);
    }
  }

  // The picked light out of view: an arrow at the edge of the picture,
  // the way to turn to it, with its name
  if (m_Selected >= 0 && m_Selected < (int)m_Lights.size() && !m_Lights[m_Selected].Removed)
  {
    Dot const& dot = dots[m_Selected];
    bool inView = dot.Shown && dot.At.x >= 0.f && dot.At.x <= screen.x && dot.At.y >= 0.f && dot.At.y <= screen.y;
    if (!inView)
    {
      Light& light = m_Lights[m_Selected];
      XMVECTOR offset = XMLoadFloat3((XMFLOAT3 const*)&At<float>(light.pEntity, EntityTransformOffset + 0x30)) - camera;
      float dx = XMVectorGetX(XMVector3Dot(offset, XMVectorSet(t[0], t[1], t[2], 0.f))) * projection[0];
      float dy = -XMVectorGetX(XMVector3Dot(offset, XMVectorSet(t[4], t[5], t[6], 0.f))) * projection[5];
      float length = sqrtf(dx * dx + dy * dy);
      if (length < 1e-4f)
      {
        // Right behind the camera
        dx = 0.f;
        dy = 1.f;
        length = 1.f;
      }
      dx /= length;
      dy /= length;
      float margin = fontSize * 2.f;
      float halfX = screen.x * 0.5f - margin, halfY = screen.y * 0.5f - margin;
      float along = (std::min)(fabsf(dx) > 1e-6f ? halfX / fabsf(dx) : FLT_MAX,
        fabsf(dy) > 1e-6f ? halfY / fabsf(dy) : FLT_MAX);
      ImVec2 at(screen.x * 0.5f + dx * along, screen.y * 0.5f + dy * along);
      float a = fontSize * 0.75f;
      ImVec2 tip(at.x + dx * a, at.y + dy * a);
      ImVec2 sideA(at.x - dy * a * 0.6f, at.y + dx * a * 0.6f);
      ImVec2 sideB(at.x + dy * a * 0.6f, at.y - dx * a * 0.6f);
      pDrawList->AddTriangleFilled(ImVec2(tip.x + dx * 2.f, tip.y + dy * 2.f), ImVec2(sideA.x - dx * 2.f, sideA.y - dy * 2.f),
        ImVec2(sideB.x - dx * 2.f, sideB.y - dy * 2.f), shadow);
      pDrawList->AddTriangleFilled(tip, sideA, sideB, white);
      ImVec2 textSize = ImGui::CalcTextSize(light.Name);
      ImVec2 label(at.x - dx * fontSize * 1.2f - textSize.x * 0.5f * (1.f + dx),
        at.y - dy * fontSize * 1.2f - textSize.y * 0.5f * (1.f + dy));
      shadowedText(label, white, light.Name);
    }
  }

  // By the cursor: the light's name and what the mouse does to it; while
  // held, how far away it is
  int named = hover >= 0 ? hover : -1;
  if (pHeld)
    for (size_t i = 0; i < m_Lights.size(); ++i)
      if (m_Lights[i].Id == m_DragId)
        named = (int)i;
  if (pAimed || (aimHover && named < 0))
  {
    // On, or turning by, the picked spot's aiming handle
    Light& light = pAimed ? *pAimed : m_Lights[m_Selected];
    char text[200];
    sprintf_s(text, pAimed ? "%s: aiming\nWheel: the handle nearer or farther." :
      "%s: drag to aim it\nWheel while dragging: the handle nearer or farther.", light.Name);
    shadowedText(ImVec2(io.MousePos.x + fontSize * 1.2f, io.MousePos.y + fontSize * 0.6f), white, text);
  }
  else if (named >= 0)
  {
    Light& light = m_Lights[named];
    char text[200];
    if (pHeld)
    {
      XMVECTOR place = XMLoadFloat3((XMFLOAT3 const*)&At<float>(light.pEntity, EntityTransformOffset + 0x30));
      sprintf_s(text, "%s: %.1f m away%s\nShift: only up and down. Wheel: nearer or farther.", light.Name,
        XMVectorGetX(XMVector3Length(place - camera)), m_DragUpDown ? " (up and down)" : "");
    }
    else
      sprintf_s(text, "%s: %.0f %s, light radius %.1f m\nDrag to move it (Shift: only up and down).\n"
        "Wheel: brighter or dimmer. Ctrl + wheel: light radius.", light.Name, At<float>(light.pData, IntensityOffset),
        At<int>(light.pData, LightUnitOffset) == 1 ? "nits" : "lm", At<float>(light.pData, AttenuationRadiusOffset));
    shadowedText(ImVec2(io.MousePos.x + fontSize * 1.2f, io.MousePos.y + fontSize * 0.6f), white, text);
  }
}

void LightManager::DrawUI()
{
  std::lock_guard<std::mutex> lock(m_Mutex);

  if (!m_Supported)
  {
    ImGui::PushTextWrapPos(0.f);
    ImGui::TextDisabled("The light manager is turned off: %s.", m_UnsupportedReason.c_str());
    ImGui::PopTextWrapPos();
    return;
  }

  if (ImGui::Button("New sphere light"))
    CreateLight(Sphere);
  ImGui::ItemTooltip("Shines in every direction, like a bulb. It appears on what the camera is looking at (the "
    "middle of the picture), a little off the surface; with nothing within 60 m, 2 m in front of the camera.");
  ImGui::SameLine();
  if (ImGui::Button("New spot light"))
    CreateLight(Spot);
  ImGui::ItemTooltip("A cone of light, like a torch, pointing the way the camera looks. It appears a little before "
    "what the camera is looking at, so its light lands there; with nothing within 60 m, 2 m in front of the camera.");
  // Undo: the last move (a dot's drag, a spot's aim, the Place boxes and
  // buttons) or removal
  if (!m_Undo.empty())
  {
    ImGui::SameLine();
    if (ImGui::Button("Undo"))
      Undo();
    if (!m_Undo.empty())
      ImGui::ItemTooltip(("Undo " + std::string(m_Undo.back().Label) + ". Moves (dragging a dot or a spot's aiming "
        "square, the Place boxes and buttons) and removals can be undone, up to 50 steps.").c_str());
  }

  // The dots are always on (no tick for them)
  ImGui::Checkbox("Keep height", &m_DragAlongGround);
  ImGui::HelpMarker("While this menu is open each light has a dot on the picture (away from the menu's window):\n"
    "- click a dot to pick its light here;\n"
    "- drag it to move the light across the picture;\n"
    "- hold Shift while dragging to move it only up and down;\n"
    "- turn the mouse wheel while dragging to take it nearer or farther;\n"
    "- turn the wheel over a dot to make it brighter or dimmer;\n"
    "- hold Ctrl and turn the wheel over a dot to make its light radius bigger or smaller (the free camera doesn't "
    "move down for Ctrl meanwhile);\n"
    "- drag the square at the end of the picked spot's line to aim it (the wheel while dragging takes the "
    "square nearer or farther).\n"
    "The picked light shows its light radius, and when it's out of view an arrow at the edge points to it. "
    "With the cursor on a light in the list, its dot is picked out.\n"
    "Keep height ticked, dragging moves the light over the ground at the height it's at, instead of across the "
    "picture. Shift still moves it up and down.");

  // What the last removal came to, from the game's own lists (CheckLeaving):
  // each removed light checked about a second after its off was sent
  int leaving = 0;
  for (Leaving const& l : m_Leaving)
    leaving += l.pEntity ? 1 : 0;
  ULONGLONG now = GetTickCount64();
  if (leaving > 0)
    ImGui::TextDisabled("Checking that the game has let %d removed light%s go...", leaving, leaving == 1 ? "" : "s");
  else if (m_LeftAt && now - m_LeftAt < LeftShownMs && m_LeftOff + m_LeftLit > 0)
  {
    ImGui::PushTextWrapPos(0.f);
    if (m_LeftLit == 0)
      ImGui::TextColored(ImVec4(0.45f, 0.85f, 0.45f, 1.f), "Removed: %s off in the game (checked in its own "
        "list of lights).", m_LeftOff == 1 ? "it's" : m_LeftOff == 2 ? "both are" : "all are");
    else
      ImGui::TextColored(ImVec4(1.f, 0.55f, 0.3f, 1.f), "%d of the removed lights may still be lit (%s): the "
        "game hadn't let it go after 6 s. CT.log has the details.", m_LeftLit, m_LeftLitName);
    ImGui::PopTextWrapPos();
  }

  int lightCount = 0;
  for (Light const& light : m_Lights)
    lightCount += light.Removed ? 0 : 1;

  if (lightCount == 0)
  {
    ImGui::TextDisabled("No lights yet: make one with the buttons above.");
    return;
  }

  ImGui::TextDisabled("%d light%s", lightCount, lightCount == 1 ? "" : "s");
  ImGui::SameLine();
  if (ImGui::ConfirmButton("Remove all"))
  {
    // Each as its own Remove does: off in the game, then off the list once
    // the game has been told (OnGameUpdate), then checked (CheckLeaving)
    EndDrag();
    EndAim();
    UndoStep step{};
    step.Removal = true;
    for (Light& light : m_Lights)
      if (!light.Removed)
      {
        step.Items.push_back(SnapshotOf(light));
        RemoveLight(light);
      }
    if (step.Items.size() == 1)
      sprintf_s(step.Label, "removing \"%.40s\"", step.Items[0].Name);
    else
      sprintf_s(step.Label, "removing all %d lights", (int)step.Items.size());
    PushUndo(step);
    m_Selected = -1;
    return;
  }
  ImGui::ItemTooltip("Turns every light off in the game and takes it off the list. Click twice. A line above "
    "then says once the game has let them all go. Undo brings them all back.");

  DrawLightList(lightCount);

  if (m_Selected < 0 || m_Selected >= (int)m_Lights.size() || m_Lights[m_Selected].Removed)
  {
    ImGui::TextDisabled("Pick a light in the list, or click its dot on the picture.");
    return;
  }

  DrawLightSettings(m_Lights[m_Selected]);
}

void LightManager::DrawLightList(int lightCount)
{
  // A row each: on or off, its colour and its name (click to pick it)
  float rowHeight = ImGui::GetFrameHeightWithSpacing();
  float listHeight = rowHeight * (lightCount < 5 ? lightCount : 5) + ImGui::GetStyle().WindowPadding.y * 2.f;
  ImGui::BeginChild("##LightList", ImVec2(0, listHeight), true);
  for (int i = 0; i < (int)m_Lights.size(); ++i)
  {
    Light& light = m_Lights[i];
    if (light.Removed)
      continue;
    ImGui::PushID(light.Id);
    bool on = At<BYTE>(light.pEntity, EntityEnabledOffset) != 0;
    if (ImGui::Checkbox("##On", &on))
    {
      At<BYTE>(light.pEntity, EntityEnabledOffset) = on ? 1 : 0;
      QueueUpdate(light);
    }
    ImGui::ItemTooltip("On or off");
    ImGui::SameLine();
    float const* c = &At<float>(light.pData, ColorOffset);
    float most = (std::max)((std::max)(c[0], c[1]), c[2]);
    most = most > 0.f ? most : 1.f;
    float swatch = ImGui::GetFrameHeight();
    if (ImGui::ColorButton("##Color", ImVec4(c[0] / most, c[1] / most, c[2] / most, 1.f),
        ImGuiColorEditFlags_NoTooltip, ImVec2(swatch, swatch)))
      m_Selected = i;
    ImGui::SameLine();
    char label[96];
    sprintf_s(label, "%s%s", light.Name, on ? "" : " (off)");
    ImGui::AlignTextToFramePadding();
    if (ImGui::Selectable(label, m_Selected == i))
      m_Selected = i;
    // Its dot picked out on the picture (the next DrawOverlay)
    if (ImGui::IsItemHovered())
      m_ListHoverId = light.Id;
    ImGui::PopID();
  }
  ImGui::EndChild();
}

void LightManager::DrawLightSettings(Light& light)
{
  BYTE* d = light.pData;
  BYTE* e = light.pEntity;
  bool changed = false;
  bool copy = false, remove = false;
  float fontSize = ImGui::GetFontSize();
  ImGui::PushItemWidth(fontSize * 14.f);
  ImGuiColorEditFlags colorFlags = ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float;

  if (ImGui::InputText("Name", light.Name, sizeof(light.Name)))
    light.CustomName = true;

  // A look to start from
  Look const* looks = light.Type == Spot ? SpotLooks : SphereLooks;
  if (ImGui::BeginCombo("Start from", "Pick a look..."))
  {
    for (int k = 0; k < LookCount; ++k)
    {
      if (ImGui::Selectable(looks[k].Name))
        ApplyLook(light, k);
      if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", looks[k].Help);
    }
    ImGui::EndCombo();
  }
  ImGui::HelpMarker("Sets the color, brightness and light radius (and a spot's cone) to about what the real thing gives. "
    "Change anything afterwards.");

  ImGui::SubHeading("Light");
  bool on = At<BYTE>(e, EntityEnabledOffset) != 0;
  if (ImGui::Checkbox("On", &on))
  {
    At<BYTE>(e, EntityEnabledOffset) = on ? 1 : 0;
    changed = true;
  }
  ImGui::HelpMarker("Off keeps the light and its settings, the game just doesn't draw it.");
  ImGui::SameLine();
  changed |= ImGui::Checkbox("Shadows", (bool*)(d + CastShadowsOffset));
  ImGui::HelpMarker("The light casts shadows. Each one costs some performance.");

  // The colour, picked or from a temperature (kept in the light's own
  // LightColorMode and Temperature, which only the tools read)
  bool fromTemperature = At<int>(d, LightColorModeOffset) == 1;
  if (ImGui::Checkbox("Use a color temperature", &fromTemperature))
  {
    At<int>(d, LightColorModeOffset) = fromTemperature ? 1 : 0;
    if (fromTemperature)
    {
      int& kelvin = At<int>(d, TemperatureOffset);
      kelvin = kelvin < MinTemperature ? MinTemperature : (kelvin > MaxTemperature ? MaxTemperature : kelvin);
      TemperatureColor(kelvin, &At<float>(d, ColorOffset));
      changed = true;
    }
  }
  ImGui::HelpMarker("The color of real lights, from warm to cold: a candle is about 1900 K, a light bulb 2700 K, "
    "halogen 3000 K, daylight 5500 to 6500 K and shade 7000 K or more. The colors are the ones the game's own "
    "lights have.");
  if (fromTemperature)
  {
    int& kelvin = At<int>(d, TemperatureOffset);
    if (ImGui::SliderInt("Temperature", &kelvin, MinTemperature, MaxTemperature, "%.0f K"))
    {
      kelvin = kelvin < MinTemperature ? MinTemperature : (kelvin > MaxTemperature ? MaxTemperature : kelvin);
      TemperatureColor(kelvin, &At<float>(d, ColorOffset));
      changed = true;
    }
    float const* c = &At<float>(d, ColorOffset);
    ImGui::SameLine();
    ImGui::ColorButton("##TemperatureColor", ImVec4(c[0], c[1], c[2], 1.f), ImGuiColorEditFlags_NoTooltip);
  }
  else if (ImGui::ColorEdit3("Color", &At<float>(d, ColorOffset), colorFlags))
    changed = true;

  // Candela a lumen on the light's axis: over 4 pi for a sphere, over pi for
  // a spot whatever its cone (the game's 0x143623120 with 0x143623190 /
  // 0x1436231C0); in nits, times its size's area seen from the front (at
  // least 1 cm: in nits no size isn't a point, 0x14362DA30)
  float perLumen = light.Type == Spot ? 1.f / Pi : 1.f / (4.f * Pi);
  float radius = At<float>(d, SizeOffset) > 0.01f ? At<float>(d, SizeOffset) : 0.01f;
  float area = Pi * radius * radius;
  int unit = At<int>(d, LightUnitOffset) == 1 ? 1 : 0;
  changed |= ImGui::DragFloat("Brightness", &At<float>(d, IntensityOffset), 0.02f, 0.f, MaxIntensity,
    unit == 1 ? "%.0f nits" : "%.0f lm", 3.f);
  ImGui::HelpMarker("Drag, or Ctrl+click to type a number. The game lights its world in real units: a 60 W bulb "
    "is about 800 lumens (lm). In daylight it takes tens of thousands of lumens for a light to show.");
  float intensity = At<float>(d, IntensityOffset);
  float candela = (unit == 1 ? intensity * area : intensity * perLumen) * At<float>(d, DimmerOffset);
  // How far it gets (the game's AttenuationRadius); the picked light shows it on the picture
  changed |= ImGui::DragFloat("Light radius", &At<float>(d, AttenuationRadiusOffset), 0.05f, 0.05f, 1000.f, "%.1f m");

  if (light.Type == Spot)
  {
    float& outer = At<float>(d, OuterAngleOffset);
    float& inner = At<float>(d, InnerAngleOffset);
    float softness = outer > 0.f ? 1.f - inner / outer : 1.f;
    softness = softness < 0.f ? 0.f : (softness > 1.f ? 1.f : softness);
    if (ImGui::DragFloat("Cone width", &outer, 0.25f, 1.f, 179.f, "%.0f" UI_DEG))
    {
      inner = outer * (1.f - softness);
      changed = true;
    }
    ImGui::HelpMarker("How wide the cone is, edge to edge (1 to 179" UI_DEG "). Its shadow covers at most 150" UI_DEG
      " of it.");
    float soft = softness * 100.f;
    if (ImGui::SliderFloat("Soft edge", &soft, 0.f, 100.f, "%.0f %%"))
    {
      inner = outer * (1.f - soft / 100.f);
      changed = true;
    }
    ImGui::HelpMarker("How much of the cone fades out to its edge: 0 % a sharp edge, 100 % fading all the way "
      "from the middle.");
  }

  ImGui::SubHeading("Place");
  // Where it was before this frame's Place edits: one undo step for each
  // edit, from when its first change comes to when no Place box is held
  float placeBefore[16];
  memcpy(placeBefore, e + EntityTransformOffset, sizeof(placeBefore));
  bool placeChanged = false, placeActive = false;
  float* now = &At<float>(e, EntityTransformOffset + 0x30);
  float position[3] = { now[0], now[1], now[2] };
  if (ImGui::DragFloat3("Position", position, 0.02f, 0.f, 0.f, "%.2f"))
  {
    SetPosition(light, position);
    placeChanged = true;
  }
  placeActive |= ImGui::IsItemActive();
  ImGui::HelpMarker("Where the light is in the world, in metres (x, y up, z). Drag a number, or Ctrl+click it to "
    "type one.");

  // Sliders that move it from where it is, the way the camera looks: to its
  // right, up, and away from it (both level, so it doesn't go into the
  // ground when the camera looks down); the world's x and z without a camera
  XMVECTOR axes[3] = { XMVectorSet(1.f, 0.f, 0.f, 0.f), XMVectorSet(0.f, 1.f, 0.f, 0.f), XMVectorSet(0.f, 0.f, 1.f, 0.f) };
  float view[16];
  if (GetCameraTransform(view))
  {
    XMVECTOR right = XMVectorSet(view[0], 0.f, view[2], 0.f);
    XMVECTOR away = XMVectorSet(-view[8], 0.f, -view[10], 0.f);
    if (XMVectorGetX(XMVector3Length(right)) > 1e-3f)
      axes[0] = XMVector3Normalize(right);
    if (XMVectorGetX(XMVector3Length(away)) > 1e-3f)
      axes[2] = XMVector3Normalize(away);
  }
  const char* moveIds[3] = { "##MoveRight", "##MoveUp", "##MoveAway" };
  const char* moveFormats[3] = { "right %.2f m", "up %.2f m", "away %.2f m" };
  float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
  float moveWidth = (ImGui::CalcItemWidth() - spacing * 2.f) / 3.f;
  for (int k = 0; k < 3; ++k)
  {
    if (k > 0)
      ImGui::SameLine(0.f, spacing);
    ImGui::PushItemWidth(moveWidth);
    float before = m_Nudge[k];
    if (ImGui::DragFloat(moveIds[k], &m_Nudge[k], 0.01f, 0.f, 0.f, moveFormats[k]))
    {
      XMFLOAT3 moved;
      XMStoreFloat3(&moved, XMVectorSet(now[0], now[1], now[2], 0.f) + axes[k] * (m_Nudge[k] - before));
      SetPosition(light, &moved.x);
      placeChanged = true;
    }
    bool active = ImGui::IsItemActive();
    placeActive |= active;
    if (!active)
      m_Nudge[k] = 0.f;
    ImGui::PopItemWidth();
  }
  ImGui::SameLine(0.f, spacing);
  ImGui::TextUnformatted("Move");
  ImGui::HelpMarker("Drag left or right on a box to slide the light from where it is: to the camera's right, up, "
    "or away from the camera (level with the ground). Each box goes back to 0 when you let go.");
  if (placeChanged && !(m_PlaceEditOpen && m_PlaceEditId == light.Id))
  {
    PushMoveUndo(light, placeBefore, "moving");
    m_PlaceEditOpen = true;
    m_PlaceEditId = light.Id;
  }
  if (!placeActive)
    m_PlaceEditOpen = false;

  // The buttons, each an undo step when it moved the light
  auto placeButton = [&](char const* label, char const* what, auto action)
  {
    if (!ImGui::Button(label))
      return;
    float was[16];
    memcpy(was, e + EntityTransformOffset, sizeof(was));
    action();
    if (memcmp(was, e + EntityTransformOffset, sizeof(was)) != 0 || light.PlaceOnSurface)
      PushMoveUndo(light, was, what);
  };
  placeButton("Where the camera looks", "placing", [&]() { light.PlaceOnSurface = true; QueueUpdate(light); });
  ImGui::ItemTooltip("Puts the light on what the camera is looking at (the middle of the picture), as a new light "
    "is: a sphere a little off the surface, a spot a little before it, pointing the way it did.");
  ImGui::SameLine();
  placeButton("In front of the camera", "placing", [&]() { BringInFront(light); });
  ImGui::ItemTooltip("Brings the light 2 m in front of the camera, pointing the way it did. Handy when it's out of "
    "view.");
  placeButton("At the camera", "placing", [&]() { MoveToCamera(light); });
  ImGui::ItemTooltip("Puts the light where the camera is, pointing the way it looks.");
  if (light.Type == Spot)
  {
    ImGui::SameLine();
    placeButton("Aim like the camera", "aiming", [&]() { AimLikeCamera(light); });
    ImGui::ItemTooltip("Points the spot the way the camera looks, where it is now.");
  }
  if (ImGui::Button("Copy"))
    copy = true;
  ImGui::ItemTooltip("A new light with the same settings, half a metre to the right.");
  ImGui::SameLine();
  if (ImGui::Button("Remove"))
    remove = true;
  ImGui::ItemTooltip("Turns the light off in the game and takes it off the list. Undo brings it back.");

  if (ImGui::TreeNode("Advanced"))
  {
    if (ImGui::Combo("Units", &unit, "Lumens (all the light it gives)\0Nits (how bright its surface is)\0"))
    {
      // The same brightness in the other units
      At<int>(d, LightUnitOffset) = unit;
      At<float>(d, IntensityOffset) *= unit == 1 ? perLumen / area : area / perLumen;
      changed = true;
    }
    ImGui::HelpMarker("Lumens: the light's whole output, as on a bulb's box; half of the game's own sphere lights "
      "have 125 to 950 lm, its spots 200 to 2,500. Nits: how bright the light's surface looks, which goes with "
      "its size (without one the game takes 1 cm).");
    ImGui::TextDisabled("%.0f candela: %.0f lux at 1 m, %.1f at 3 m. Daylight is 10,000 to 100,000 lux.",
      candela, candela, candela / 9.f);
    changed |= ImGui::DragFloat("Size", &At<float>(d, SizeOffset), 0.005f, 0.f, 10.f, "%.3f m");
    ImGui::HelpMarker(light.Type == Spot ?
      "The radius of the light's disc. 0 is a point. Bigger gives softer shadow edges and wider highlights; "
      "in lumens the light's output stays the same." :
      "The radius of the light's ball. 0 is a point. Bigger gives softer highlights; in lumens the light's "
      "output stays the same.");
    // Used by the game: its light update (0x1437064A0) puts it in the GPU
    // light's data (0x143706862) and, when it's off, swaps the light's shape
    // values for an "off" value (0x1437069D7: -1e9, -1e9, 100, 100)
    changed |= ImGui::Checkbox("Show its shape", (bool*)(d + EmissiveShapeOffset));
    ImGui::HelpMarker("The game's EmissiveShapeEnable, which 10 of its own 4,120 lights use. The game does use it: "
      "only with it on does it hand the light's shape on to the graphics card. What that looks like wasn't checked "
      "in game; try it with a Size above 0.");
    changed |= ImGui::DragFloat("Brightness multiplier", &At<float>(d, DimmerOffset), 0.01f, 0.f, 100.f, "%.3f");
    ImGui::HelpMarker("The game's \"dimmer\": multiplies the brightness.");

    changed |= ImGui::Checkbox("Diffuse", (bool*)(d + DiffuseOffset));
    ImGui::SameLine();
    changed |= ImGui::Checkbox("Specular", (bool*)(d + SpecularOffset));
    ImGui::SameLine();
    changed |= ImGui::Checkbox("Bounce light", (bool*)(d + RadiosityOffset));
    ImGui::HelpMarker("What the light lights up. Diffuse: surfaces. Specular: shine and reflections. Bounce "
      "light: the game's radiosity (Enlighten).");

    if (At<BYTE>(d, CastShadowsOffset))
    {
      float& cull = At<float>(d, ShadowCullDistanceOffset);
      changed |= ImGui::DragFloat("Shadow distance", &cull, 0.25f, 0.f, 1000.f, "%.1f m");
      ImGui::HelpMarker("How far away the light's shadow is still drawn (the game's ShadowCullDistance; 80 m for "
        "most of its own lights).");
      changed |= ImGui::DragFloat("Shadow fade", &At<float>(d, ShadowFadeDistanceOffset), 0.1f, 0.f, cull, "%.1f m");
      ImGui::HelpMarker("The shadow fades out over this much before the shadow distance (the game keeps it no "
        "more than the shadow distance).");
      changed |= ImGui::DragFloat("Shadow reach", &At<float>(d, ShadowRadiusOffset), 0.05f, 0.f, 1000.f, "%.2f m");
      ImGui::HelpMarker("How far from the light things cast its shadow. 0, or more than the light radius: the "
        "light radius.");
      changed |= ImGui::DragFloat("Shadow strength", &At<float>(d, ShadowDimmerOffset), 0.01f, 0.f, 100.f, "%.3f");
    }

    changed |= ImGui::DragFloat("Fade start", &At<float>(d, AttenuationOffsetOffset), 0.01f, 0.f, 1000.f, "%.3f");
    ImGui::HelpMarker("The game's attenuation offset.");
    changed |= ImGui::ColorEdit3("Bounce light color", &At<float>(d, RadiosityColorOffset), colorFlags);
    ImGui::HelpMarker("Multiplies the light's color in the bounce light (the game's RadiosityColorScale).");
    changed |= ImGui::ColorEdit3("Particle color", &At<float>(d, ParticleColorOffset), colorFlags);
    ImGui::HelpMarker("Multiplies the light's color on smoke and other particles (the game's ParticleColorScale).");
    ImGui::TreePop();
  }
  ImGui::PopItemWidth();

  if (changed)
  {
    // The menu's own limits for what's sent (2026-11-04): ImGui 1.61 keeps a
    // drag or slider in them only while dragging, a Ctrl+click typed number
    // (as the help invites) isn't clamped, and with the HDR flag a color's
    // drags have no limits at all (ColorEdit4 gives them 0, 0), so a drag to
    // the left took a channel below 0
    auto clamp = [](float& value, float lo, float hi) { value = !(value >= lo) ? lo : (value > hi ? hi : value); };
    for (int offset : { ColorOffset, ParticleColorOffset, RadiosityColorOffset })
    {
      float* c = &At<float>(d, offset);
      for (int i = 0; i < 3; ++i)
        clamp(c[i], 0.f, 1e30f);
      // The original never let a color go fully black (the game drops a
      // light whose colour times intensity is below 0.00001, 0x143765411)
      if (c[0] <= 0 && c[1] <= 0 && c[2] <= 0)
        c[0] = c[1] = c[2] = 0.0001f;
    }
    clamp(At<float>(d, IntensityOffset), 0.f, MaxIntensity);
    clamp(At<float>(d, AttenuationRadiusOffset), 0.05f, 1000.f);
    clamp(At<float>(d, SizeOffset), 0.f, 10.f);
    clamp(At<float>(d, DimmerOffset), 0.f, 100.f);
    clamp(At<float>(d, ShadowCullDistanceOffset), 0.f, 1000.f);
    clamp(At<float>(d, ShadowFadeDistanceOffset), 0.f, At<float>(d, ShadowCullDistanceOffset));
    clamp(At<float>(d, ShadowRadiusOffset), 0.f, 1000.f);
    clamp(At<float>(d, ShadowDimmerOffset), 0.f, 100.f);
    clamp(At<float>(d, AttenuationOffsetOffset), 0.f, 1000.f);
    if (light.Type == Spot)
    {
      // The cone 1 - 179 degrees, its soft edge inside it
      float& outer = At<float>(d, OuterAngleOffset);
      float& inner = At<float>(d, InnerAngleOffset);
      clamp(outer, 1.f, 179.f);
      clamp(inner, 0.f, outer);
    }
    QueueUpdate(light);
  }

  // Last: a new light can move the list (and this light) in memory
  if (copy)
  {
    Light source = light;
    CreateLight(source.Type, &source);
  }
  else if (remove)
  {
    UndoStep step{};
    step.Removal = true;
    sprintf_s(step.Label, "removing \"%.40s\"", light.Name);
    step.Items.push_back(SnapshotOf(light));
    RemoveLight(light);
    PushUndo(step);
  }
}
