# Mirror's Edge Catalyst Cinematic Tools, with keyframed camera tracks

A rebuild of Hattiwatti's Mirror's Edge Catalyst cinematic tools on his
[CinematicTools template](https://github.com/Hattiwatti/CinematicTools), with an in-game menu:

- free camera (mouse, keyboard, gamepad) and **keyframed camera tracks**: position, rotation,
  field of view, motion blur and time of day, with pause, live preview and a fixed playback rate
  for frame dumping
- play as Faith while a track plays, **follow Faith** or NPCs, crowds, cars, drones, boats, trains,
  aircraft, security cameras and moving props
- **load the city around the camera**, so a far-away camera isn't surrounded by missing props
- time freeze, HUD toggle, time of day, Fix LOD, draw and shadow distance, motion blur, visual
  effects (color, DoF, grain, light, tonemap, vignette, fog with a greenscreen) and lights

## Installing

1. Copy `CT_MirrorsEdge.dll` and `usp10.dll` (from a release, or `dist/` after building) next to
   `MirrorsEdgeCatalyst.exe`.
2. Start the game and press **F5** for the menu, **F10** for the free camera.

No injector is needed: the game loads `usp10.dll`, which loads the tools. It works next to
Catalyst Clarity; another mod with its own `usp10.dll` can't be used at the same time. Windows 10
or 11. To uninstall, delete the two files and the `Cinematic Tools` folder.

Settings are saved in `Cinematic Tools\config.ini`, the log in `Cinematic Tools\CT.log` (the
previous one is kept as `CT.previous.log`, with `[CRASH]` lines at the end if the game crashed).

## Building

Visual Studio 2022 (or the Build Tools) with the C++ workload and a Windows 10/11 SDK:

```powershell
powershell -ExecutionPolicy Bypass -File build.ps1
```

This fills `dist/` with `CT_MirrorsEdge.dll` and `usp10.dll` (static CRT, no redistributable
needed). The C4244 warnings from `Input/InputSystem.cpp` come from the template.

## Controls

Defaults, rebindable in MISC → *Change hotkeys...*

| Key | Action |
|---|---|
| F5 | Menu |
| F10 | Free camera on / off |
| W A S D / Space / Left Ctrl | Move |
| Mouse, arrow keys | Look (with the menu open: hold the right mouse button outside it) |
| Numpad 1 / 3 | Roll |
| Page Up / Page Down, mouse wheel | Field of view |
| Numpad + / − | Add keyframe / delete the last one |
| Numpad \* | Play / stop the track |
| Numpad 5 | Follow what the camera looks at (again: let go) |
| Delete | Freeze time |
| Home | Hide HUD |
| End | Block / unblock game input |

Numpad 1, 3 and 5 need Num Lock on. The gamepad works too. Grey `(?)` marks in the menu explain
each option.

## Making a camera move

1. Press F10, fly to the first shot and press Numpad +. Repeat for each shot.
2. In the menu (CAMERA → *Camera tracks*), open a keyframe to change its position, angles, FOV,
   time to the next one and how it gets there (Smooth, Linear, Ease in/out or Hold). The arrows on
   a row move a keyframe earlier or later.
3. Press Numpad \* (or *Play*) to play; *Pause* holds the camera on that frame.
4. For recordings, set *Fixed playback FPS* to the capture frame rate.

Keyframes also store motion blur and the time of day: tick them in *Keyframe options* and the
track sets them too (06:00 → 18:00 → 06:00 is a full day and night). *Faith keeps control while a
track plays* (on by default) lets you play as Faith during playback.

Tracks and lights aren't saved when the game closes.

## Menu tabs

- **CAMERA**: camera on / off; camera tracks and keyframes (shot sequences, the track's path with
  dots to drag, a preview camera); moves round a focus (orbit, aim, dolly zoom, auto-focus); how the
  camera moves (heavy camera, fly level, speed by height, camera collision); camera shake; letterbox
  and guide lines; wide fields of view (Panini); motion blur and camera speeds. *Show Faith's body*
  shows her third-person body, hair included.
- **GAME**: game speed; time of day and HUD; Faith (outfit, no arm tattoo, no MagRope, no bag, the
  gridlink); *Pose*, a pose editor; *Faith in the streets* (*Solid street*); detail; level
  streaming (*Load the world around the camera*, districts, the real Shard from anywhere); sun
  shadows. The time of day controls are disabled while MECMP is open, because MECMP syncs the time
  of day.
- **FOLLOW**: *Follow Faith*, or click something in the *Nearby* list to follow it (the camera goes
  next to it), or aim at it and use *Attach to what the camera looks at*. *Turn with it*, *Tilt with
  it too* and *Smoothing* change how the camera follows. The list's groups (People, Cars, Drones,
  Boats, Trains, Aircraft, Security cameras, Other) come from the names of the game's own models.
  Trains and aircraft are listed wherever they are, and a security camera puts the camera at its
  lens.
- **VISUALS**: rain, snow and a green screen; the game's effects (open one and tick *Change this
  effect* to override it: depth of field, filmic effects, colour correction, outdoor light and
  more); *Lights* (sphere and spot lights, with dots to drag on the picture); *Images* (your own
  pictures standing in the world).
- **REPLAY** (experimental): record Faith's run and what's around her, then play it back on her
  body (all of it, not a ghost) while the camera films it again; it can start and stop with the
  camera track. Recordings are saved, and Dashes record themselves.
- **MISC**: performance, text size, mouse cursor, hotkeys, console window.

## Loading the world around the camera

The game only streams the city in around Faith, so a far-away camera sees missing props. *Load
the world around the camera* (GAME → *Level streaming*) also loads it around the free
camera, ahead of it and along the selected track:

- *Stream distance* loads up to 600 m all round, or 1000 m with *Only where the camera looks*.
- What the camera looks at loads first (after what's right around Faith).
- *Districts*: each district of the city (Anchor, The View, Rezoning, Downtown, ...) *Normal*,
  *Always* loaded, or *Never* loaded (unloaded, except right around Faith), with the camera on or
  off.
- *Vistas* (the low-detail copies of the buildings far away): all on or off, or each zone on or
  off. By default a zone's vista is hidden while its real buildings are loaded, so the two don't
  show at once.
- The game crashes when some of its limits run out (zones, meshes, model instances, the renderer's
  draw batches and memory). The tools watch them and hold the camera's zones back in time; the
  menu shows how full each one is.
- In busy parts of the city what's loaded around Faith already fills the renderer's draw batch to
  about 75 %, past the default hold-back of 70 %. *Hold back from* (up to 78 %) lets more load
  there, with less room before a crash.
- *Unrestricted* ignores the renderer's limits for more distance, but **the game can crash**.
- After starting the game, quit to the main menu and continue once: the next level load gets
  more room for model instances.
- Some safe houses may or may not load with this setting.

*Fix LOD* and *Show small objects far away* help with the detail of what is loaded. *Smooth
animations far away* (characters past 10 m move as smoothly as near ones) is on by default, and
*Billboard quality* is at *Much sharper* (the video billboards' full-size video allowed, and their
bigger versions from farther away).

## Credits and license

- [Hattiwatti](https://github.com/Hattiwatti) for the original tools and the CinematicTools
  template
- MEC-TC (Mirror's Edge Catalyst Time Changer) for the time of day address and the stuck clock fix
- [MinHook](https://github.com/TsudaKageyu/minhook), [Dear ImGui](https://github.com/ocornut/imgui)
  and [inih](https://github.com/benhoyt/inih)

MIT, see `LICENSE`. The bundled libraries keep their own licenses (`THIRD_PARTY_NOTICES.md`);
include that file with any release of the DLLs. See `CHANGELOG.md` for what changed in each version.
