#pragma once
#include <atomic>
#include <mutex>
#include <string>
#include <vector>
#include <Windows.h>

// Adds sphere and spot lights to the scene, like the original tools'
// Light Manager. The original builds the game's light entity structures in
// its own memory (with the game's vtables) and hands them to the game's
// UpdateLight function. The game's own fields and what its renderer does
// with them: Research_Lights_2026-10-26.md (outside the repo).
class LightManager
{
public:
  enum LightType { Sphere = 0, Spot = 1 };

  // Whether the game still has a light's render light (see CheckHandles)
  struct HandleCheck
  {
    ULONGLONG ChangedAt;  // when it last went to the game
    int Misses;           // checks in a row that found its render light gone
    float SentPlace[3];   // where it was then, and its radius (the game's
    float SentRadius;     // render light has these once it's taken it)
  };

  struct Light
  {
    int Id;
    LightType Type;
    BYTE* pEntity;
    BYTE* pData;
    bool Removed;
    char Name[48];
    bool CustomName;    // typed in the menu: a look doesn't rename it
    HandleCheck Check;
    bool PlaceOnSurface;  // to be put on what the camera looks at (game thread, PlaceOnSurfaces)
  };

  // A light's data block (PbrSphere/SpotLightEntityData, 0x110 bytes)
  static const int LightDataSize = 0x110;

  LightManager();
  ~LightManager();

  // Game thread, from the camera update hook (every render view). Light
  // changes are sent to the game from here, and while the menu is open the
  // main view's camera is kept for the lights' dots.
  void OnGameUpdate();
  // Render thread, from the Present hook, with the menu open: each light's
  // dot over the picture, dragged with the mouse (before the menu's windows
  // and before the camera's drawn track path, so a dot gets a click first)
  void DrawOverlay();
  // Render thread, from the Present hook
  void DrawUI();
  // Render thread: true once after a dot was clicked, for the menu to open
  // the Lights header
  bool TakeOpenRequest();
  // Any thread: Ctrl is held with the cursor on a light's dot (menu open,
  // the last DrawOverlay within 150 ms), so Ctrl + the wheel is the light's
  // radius; the free camera's Ctrl "move down" waits then (CameraManager)
  bool HoldsCtrl();

  // Lights made from code (Camera/DogenDoor.cpp's lamps): not in the menu's
  // list, its dots or Remove all. Any thread; they take the manager's lock,
  // so not from inside its own calls. rows: right, up, backward, position,
  // the entity's own transform (a spot shines along -row 2, the way a camera
  // looks). color null: the colour of temperatureK (the game's own Kelvin
  // table); given, that colour. intensity in lumens, attenuationRadius in
  // metres. Like the menu's lights, one the game drops (a full rebuild of its
  // lights, a loading screen) is made again (see CheckHandles). Returns its
  // id, 0 when the light manager is off.
  int AddFixedLight(LightType type, float const rows[16], float const color[3], int temperatureK,
    float intensity, float attenuationRadius);
  void RemoveFixedLight(int id);
  // A light made from code given a new intensity (lumens, kept within 0 -
  // 1e8), sent with the next update (the game reads it through the entity's
  // Intensity property, data + 0xA8, each time it builds the light). False if
  // it isn't one, or it's being removed. Unchanged, nothing is sent.
  bool SetFixedLightIntensity(int id, float intensity);

private:
  // The main render view's camera: rows right, up, backward, position, and
  // its projection (row vectors)
  struct View
  {
    float Transform[16];
    float Projection[16];
  };

  struct FixedLight
  {
    int Id;
    LightType Type;
    BYTE* pEntity;
    BYTE* pData;
    HandleCheck Check;
    bool Removed;       // its off is waiting to be sent
  };

  // A light turned off for good (Remove, Remove all, RemoveFixedLight),
  // looked at after its off was sent until the game has let it go
  struct Leaving
  {
    BYTE* pEntity;
    WORD Handle;        // its render light when the off was sent (0xFFFF none)
    float Place[3];     // what that render light had
    float Radius;
    int Shape;
    ULONGLONG SentAt;
    char Name[64];
  };

  // One step the menu's Undo takes back: lights moved (a dot's drag, a spot's
  // aim, the Place sliders and buttons: their transform before) or removed
  // (all of each light, to make it again)
  struct UndoItem
  {
    int Id;
    LightType Type;
    char Name[48];
    bool CustomName;
    float Transform[16];
    BYTE Data[LightDataSize];
    BYTE Enabled;
  };
  struct UndoStep
  {
    bool Removal;
    char Label[96];
    std::vector<UndoItem> Items;
  };

  bool CheckSupport();
  BYTE* BuildLight(LightType type, Light const* pCopy, BYTE*& pData);
  UndoItem SnapshotOf(Light const& light);
  void PushMoveUndo(Light const& light, float const* before, char const* what);
  void PushUndo(UndoStep const& step);
  void Undo();
  void PlaceOnSurfaces();
  void CreateLight(LightType type, Light const* pCopy = nullptr);
  void RemoveLight(Light& light);
  void MoveToCamera(Light& light);
  void BringInFront(Light& light);
  void AimLikeCamera(Light& light);
  void ApplyLook(Light& light, int look);
  bool GetCameraTransform(float transform[16]);
  void QueueEntity(BYTE* pEntity);
  void QueueUpdate(Light& light);
  HandleCheck* CheckOf(BYTE* pEntity);
  void CheckHandles();
  void CheckHandle(BYTE* pEntity, BYTE* pData, HandleCheck& check, char const* name, ULONGLONG now);
  bool LeavingName(BYTE* pEntity, char* name, size_t size);
  void CheckLeaving(ULONGLONG now);
  void SetPosition(Light& light, float const* position);
  void SetAim(Light& light, float const* forward);
  Light* FindLight(int id);
  void EndDrag();
  void EndAim();
  void DrawLightList(int lightCount);
  void DrawLightSettings(Light& light);

private:
  std::mutex m_Mutex;
  std::vector<Light> m_Lights;
  std::vector<FixedLight> m_FixedLights;
  std::vector<Leaving> m_Leaving;
  // What the last removal came to, shown in the tab for a while (under m_Mutex)
  int m_LeftOff{ 0 }, m_LeftLit{ 0 };
  char m_LeftLitName[64]{};
  ULONGLONG m_LeftAt{ 0 };
  std::vector<BYTE*> m_PendingUpdates;
  std::atomic<bool> m_HasPending{ false };
  int m_NextId;
  int m_NextFixedId{ 1 };
  int m_Selected;
  ULONGLONG m_HandlesCheckedAt{ 0 };   // game thread

  bool m_Supported;
  std::string m_UnsupportedReason;

  __int64 m_pUpdateLight;
  __int64 m_ppGameRenderer;

  // The main view's camera from the game thread, under m_ViewMutex (never
  // held with m_Mutex by the game thread)
  std::mutex m_ViewMutex;
  bool m_ViewOk{ false };
  View m_View{};

  // The dots (render thread, under m_Mutex; always shown with the menu open)
  bool m_DragAlongGround{ false };
  bool m_OpenRequest{ false };
  View m_DrawView{};
  int m_DragId{ 0 };              // the light held with the mouse, 0: none
  bool m_DragUpDown{ false };     // Shift held: only up and down
  float m_DragBase[3]{};          // where the drag measures from (moved by the wheel)
  float m_DragOffsetX{ 0.f }, m_DragOffsetY{ 0.f };  // the dot from the cursor when grabbed
  bool m_DragMoved{ false };
  // A spot aimed by the handle at the end of its line (render thread, under
  // m_Mutex): its id (0: none), the handle's place when grabbed (moved by
  // the wheel), the handle from the cursor when grabbed
  int m_AimId{ 0 };
  float m_AimBase[3]{};
  float m_AimOffsetX{ 0.f }, m_AimOffsetY{ 0.f };
  bool m_AimMoved{ false };
  // The light whose row the cursor is on in the menu's list (its dot is
  // drawn picked out); set by DrawLightList, used by the next DrawOverlay
  int m_ListHoverId{ 0 };
  // When the cursor was last on a light's dot (DrawOverlay; for HoldsCtrl)
  std::atomic<ULONGLONG> m_CursorOnDotAt{ 0 };
  // The menu's "Move" sliders: how far each has been dragged since it was
  // grabbed, 0 when let go (render thread, under m_Mutex)
  float m_Nudge[3]{};

  // Undo (render thread, under m_Mutex): newest last. What a drag or aim
  // started from, and the Place widgets' edit in progress (one step for it)
  std::vector<UndoStep> m_Undo;
  float m_DragBefore[16]{};
  float m_AimBefore[16]{};
  bool m_PlaceEditOpen{ false };
  int m_PlaceEditId{ 0 };

public:
  LightManager(LightManager const&) = delete;
  void operator=(LightManager const&) = delete;
};
