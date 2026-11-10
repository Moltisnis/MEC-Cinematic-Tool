#include "WetLook.h"
#include "SsrOrder.h"
#include "GreenPlane.h"
#include "GameFrame.h"
#include "GbufferWet.h"
#include "GroundMarks.h"
#include "Weather.h"
#include "DryShapes.h"
#include "SnowBodies.h"
#include "D3DStateBackup.h"
#include "../Util/GameState.h"
#include "../Util/Util.h"
#include "../Util/Perf.h"
#include "../Camera/CameraCollision.h"
#include <cstdlib>
#include "WetLookVs.h"
#include "WetLookPs.h"
#include "WetLookRainPs.h"
#include "WetLookSnowGroundPs.h"
#include "WetLookQueryPs.h"
#include "WetLookSnowVs.h"
#include "WetLookSnowPs.h"
#include "WetLookIceVs.h"
#include "WetLookIcePs.h"
#include "IceTable.h"
#include "LandingPadTable.h"
#include <DirectXMath.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <mutex>
#include <thread>
#include <vector>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;
using namespace DirectX;

// The game has no wet look of its own: its wet streets are separate materials
// (objects/props/citystreetset_01/shaders/ss_curbmasked_01_wet_release and the
// like) and nothing in the game makes a surface wet (2026-10-25, offline). So
// it's drawn here, over the game's finished frame at Present (before the green
// screen, the Panini projection and the menu): from a copy of the frame and
// the green screen's copy of the game's depth (greenplane::TakeFrameDepth),
// what faces up is darkened and reflects the frame, with puddles and rain
// ripples (WetLook.hlsl). Where the weather's squares say something is over a
// point it stays dry. The lightning's flash is drawn here too (whether or
// not the wet look is on), so it shows whatever the visual environment does
// with the values the weather gives it, and so are its bolt (the weather's,
// its segments put on the frame here each frame) and the rain in the distance.

namespace
{
  // A lightning burst's bolts (up to 6, each up to 64 segments, see
  // weather::GetBolts): each with its box on the frame, so the pass only
  // looks at a bolt's segments where its light can be
  const int MostBolts = 6;
  const int MostBoltSegments = MostBolts * 64;
  // The scene copy's level the rain's drops and their light are taken from:
  // about this many pixels wide
  const float BlurLevelWidth = 60.f;
  // While the camera is in Faith's head (dry::Near): the wet look fades in
  // from DryNearFrom to DryNearTo metres from it (her own arms and legs in
  // front of it are nearer), over what's no lower than her feet
  const float DryNearFrom = 0.6f;
  const float DryNearTo = 1.0f;
  // The game's interiors given to the pass (WetLook.hlsl's InteriorAt): the
  // nearest within this far of the camera (the snow on the ground reaches
  // 1000 m at most; a box counts by its corners' sphere)
  const int MostInteriorBoxes = 32;
  const int MostInteriorPrisms = 2;
  const float InteriorReach = 1100.f;
  // The flakes (WetLook.hlsl's SnowVsMain): how many at most in each box
  // round the camera, and the boxes' sizes (m). Their wrap and the snow's
  // drift's (weather's SnowDriftWrap, 9600 m) are whole boxes, so nothing jumps
  // when it wraps.
  // (2026-10-26, the user: "too much of a light snowing": two and a half times
  // the first build's; a share of them by the intensity, a third at 1)
  // (2026-10-26, the user: "can we make the snow slider more intense": up to
  // 210,000, 2.5 x the 84,000 before; intensity / 5 of them, 1.5 x as many
  // at each place on the slider as before, weather::GetSnow)
  // (15:xx, the user: "Slider fog should let you add more fog same with
  // snowfall": up to 310,000, intensity / 7.4 of them, so each place on the
  // slider up to 5 keeps what it had, and 5 - 7.5 adds half as many again)
  const int SnowNearFlakes = 220000;
  const int SnowFarFlakes = 90000;
  const float SnowNearBox = 16.f;
  const float SnowFarBox = 48.f;

  // Faith's steps (the user, 2026-10-26: "the rain peddles should react with
  // faith's shoes and create new ones"; ripples and footprints chosen). Her
  // steps and landings all come through InPuddle (the water footsteps' test):
  // from each one in a puddle rings go out (WetLook.hlsl's StepRings) for
  // StepRingSeconds. Her shoes get wet in a puddle (all of ShoeWet) or on
  // the wet ground (ShoeWetFromGround: the user, 2026-10-26, "wet footsteps
  // on dry floor doesn't work": walking in from the rain without stepping in
  // a puddle left none, as only a puddle wetted them); each step on dry
  // ground after leaves a print (PrintsAt) as wet as they are. They dry
  // evenly over ShoeWetSeconds from the last wet step (2026-10-26, the user:
  // "Increase the seconds that faith can leave footsteps after rain to 10
  // seconds"; before, each print dried them by a quarter, so they stopped
  // after 6 - 8 steps, 3 - 4 s), each print fainter, none under ShoeDryAt;
  // the prints dry over PrintSeconds. A step's sound and its effect both ask: one
  // within StepSameMs and StepSameMetres of the last is the same step.
  // How wet the ground is at a step is a share (PuddleAt's groundWet), and
  // her shoes take up as much as there is and leave a print where they're
  // wetter than the ground (see SoakUp, NoteStep).
  // The ice over the city's water while it snows (WetLook.hlsl's IceAt,
  // weather::GetIce): the nearest planes within IceReach, MostIcePlanes at
  // most (WetLook.hlsl's MostIcePlanes)
  const int MostIcePlanes = 48;
  // Faith's prints in the snow (2026-10-26, the user: "can we do the same
  // thing as the water footseps but with snow?"; WetLook.hlsl's SnowPrintsAt):
  // each of her steps and landings where the snow lies (InSnow) presses a
  // print, her last MostSnowPrints kept; the falling snow fills each over
  // SnowPrintFillSeconds at intensity 1 (faster in heavier snow; not at all
  // while it doesn't snow); pressed SnowPrintDepth deep, deeper as it piles
  // up (up to SnowPrintDeepest)
  const int MostSnowPrints = 64;
  // The KrugerSec's boot prints in the snow (2026-10-27, the user: "Ksec
  // should leave behind snow footsteps"; WetLook.hlsl's SnowPrintsAt, its
  // BootSole): each step of theirs where the snow lies (OtherSnowStep), the
  // last MostOtherSnowPrints kept, filled as hers are
  const int MostOtherSnowPrints = 48;
  // The water's own triangles under the ice (weather::GetIceTriangles): the
  // nearest MostIceTris within IceReach (WetLook.hlsl's MostIceTris)
  const int MostIceTris = 96;
  // Her slides through the snow (2026-10-26, the user: "just like footprints
  // there should be a slide trail"; WetLook.hlsl's SnowTrailAt): while the
  // game plays her slide effect near her (NoteSlide, at least every
  // SnowSlideKeepMs) her feet's path is kept as stretches of SnowTrailStep
  // metres, SnowTrailHalfWidth each side, MostSnowTrails at most, filled by
  // the falling snow as her prints are
  const int MostSnowTrails = 48;
  const float SnowTrailStep = 0.35f;
  // (her hips' half width, 2026-10-26 15:xx, the user: "snow slides should
  // have faiths body outline instead of circles": the trough as wide as her
  // hips, her legs at its end, WetLook.hlsl's SnowTrailAt; ANSUR II women's
  // hip breadth about 37 cm. Followed while she's still sliding: within
  // SnowSlideKeepMs of the slide's effect and her feet moving faster than
  // SnowSlideSpeed, low: the 06:2x build followed her for 0.7 s after each
  // effect only, so a slide was a few round dots. Its first stretch starts
  // SnowSlideHipsBehind behind her feet, where her hips were.)
  const float SnowTrailHalfWidth = 0.19f;
  const ULONGLONG SnowSlideKeepMs = 1500;
  // (her slide's trail and her roll's print give way where her body pressed
  // the snow by its own mesh within this, SnowBodies.cpp)
  const ULONGLONG BodyPressedMs = 300;
  // Her body's stamps in the rain's water (SnowBodies.cpp, WetLook.hlsl's
  // WaterStampAt) last this long: a little more than her slide's trail
  // (SlideTrailSeconds)
  const float WaterStampSeconds = 7.f;
  const float SnowSlideSpeed = 1.5f;
  const float SnowSlideHipsBehind = 0.9f;
  const ULONGLONG SnowHandSameMs = 300;
  const float SnowHandSameMetres = 0.15f;
  // Her rolls (2026-10-26, the user: "Rolling/jump rolling should also leave
  // faiths shape on the snow"; WetLook.hlsl's RollBody): the game's
  // SkillrollForward impact where the snow lies (NoteRoll) starts one, from
  // the contact the game looked up for it; her feet are followed for
  // SnowRollFollowMs and the way it lies is from there to them (her last
  // steps' way until they're SnowRollMinMetres off). Her shoulder's mark
  // SnowRollShoulderOn past where it began, her hip's SnowRollLength past
  // that; MostSnowRolls kept, filled by the falling snow as her prints are.
  // One within SnowRollSameMs and 2 m of the last is the same roll.
  const int MostSnowRolls = 8;
  const ULONGLONG SnowRollFollowMs = 900;
  const float SnowRollMinMetres = 0.3f;
  const float SnowRollShoulderOn = 0.15f;
  const float SnowRollLength = 0.6f;
  const ULONGLONG SnowRollSameMs = 1200;
  // Jumps (2026-10-26, the user: "jumping should leave two footprints
  // behind"): the game has no take-off event (its foot events are steps,
  // landings, slides, rolls, wall runs, climbing and springboards:
  // Characters/FaithCharacter's impacts), so they're seen from her feet's
  // height each frame: rising faster than SnowJumpRise within SnowJumpAfterMs
  // of a step in the snow, that step's other foot is pressed beside it; a
  // step in the snow after falling faster than SnowLandFall (within
  // SnowLandWithinMs) is a landing, both feet side by side, pressed deeper
  // (SnowLandDeeper)
  const float SnowJumpRise = 2.5f;
  const ULONGLONG SnowJumpAfterMs = 400;
  const float SnowLandFall = 3.f;
  const ULONGLONG SnowLandWithinMs = 350;
  const float SnowLandDeeper = 1.4f;
  const float SnowFeetApart = 0.12f;    // each foot from her middle (m)
  const float SnowPrintFillSeconds = 300.f;
  const float SnowPrintDepth = 0.03f;
  const float SnowPrintDeepest = 0.2f;
  const float IceReach = 600.f;
  const int MostStepRings = 12;
  // A landing's ring (2026-10-26, the user: "what i mean about jumping is
  // ripple"): bigger than a step's (1), a hard one more, each lasting longer
  // (StepRingSeconds x (0.7 + 0.3 x its size))
  const float SoftLandingRing = 1.8f, HardLandingRing = 2.6f;
  // ...and bigger the harder she came down (2026-10-26, the user: "Depending
  // from how far faith is jumping and rolling/falling should leave a bigger
  // ripple"; see FallScale): a landing's and a roll's x (1 + 0.4 sqrt(h -
  // 0.3)), h the height a fall gives her speed (her feet's fastest down in
  // the FallWindowMs before, and a quarter of her speed along the ground:
  // (down^2 + along^2 / 4) / 2g), at most MostFallScale; no ring bigger than
  // MostRingSize. Her feet's speed from each frame's place (MostFallSamples
  // kept).
  const double FallWindowMs = 300.0;
  const float MostFallScale = 2.5f, MostRingSize = 5.f;
  const int MostFallSamples = 16;
  // Other people's steps and the cars' wheels in the puddles (2026-10-26, the
  // user: "Research how can ksec and other people leave ripples /
  // vechicles"): the MostOtherRings nearest the camera (a new one takes the
  // place of one over, or of the farthest when it's nearer), within
  // OtherRingReach, a step's ring as Faith's (1), a landing's as hers, a
  // wheel's WheelRing. Where they come from, see wetlook::OtherStep. A step
  // made from someone's moves (DryShapes.cpp) isn't made within
  // OtherOwnReach and OtherOwnMs of one of the game's own impacts (KSec's,
  // its AI people's), MostOwnSteps of those kept.
  const int MostOtherRings = 24;
  const float OtherRingReach = 40.f, WheelRing = 1.5f;
  // A downed body hitting the ground (2026-10-26, the user: "downed ksec
  // should leave a body ripple when they enter ragdoll physics and hit the
  // ground"; see wetlook::NoteRagdollKnock): its ring BodyRing; a knock
  // within BodySameMetres and BodySameMs of the last is the same; where it
  // hit the ground swept for on the game thread (LandBodies), MostBodyKnocks
  // waiting at most
  const float BodyRing = 2.4f, BodySameMetres = 1.f;
  const ULONGLONG BodySameMs = 500;
  const int MostBodyKnocks = 8;
  const float OtherOwnReach = 1.2f;
  const ULONGLONG OtherOwnMs = 1500;
  const int MostOwnSteps = 16;
  // A little splash at every ripple (2026-10-26, the user: "also we need
  // little splash of water in every ripple"; WetLook.hlsl's SplashesAt):
  // while a ring is young (its drops up and back, about 0.3 s x sqrt(its
  // size)), the MostSplashes nearest the camera
  const int MostSplashes = 16;
  // Her own splash, the game's (2026-10-27, the user: "also on puddles use
  // faiths splash fx when she steps on in-game puddles"): the game plays
  // FX/Movement/FX_Foot_Water (FX_FootLandSoft_Water, FX_FootLandHard_Water)
  // wherever her step's effect has the water's material: in the tools'
  // puddles (Hooks.cpp's WetStep makes it so) and on the game's own puddle
  // decals (objects/decals/waterpuddle, whose collision is the wet concrete,
  // map 47, with the water's effects; the 02:16 CT.log of 2026-10-27 has all
  // 23 of its water steps on them). Its splash (5 white sprites 0.35 m across
  // thrown up for 0.55 s +- 30 %, and 20 drops) doesn't write the depth, so
  // the wet look took it for the ground under it and laid the puddle over it
  // (darker, blurred, mirrored). For HerSplashSeconds x sqrt(its size) the
  // wet look lets it through (WetLook.hlsl's HerSplashAt), and no splash of
  // the tools' is drawn for that step: hers is. A step's ring and her splash
  // are the same step within HerSplashSameMs and HerSplashSameMetres.
  const int MostHerSplashes = 8;
  const float HerSplashSeconds = 0.8f;
  const float HerSplashSoft = 1.5f, HerSplashHard = 2.f;   // a landing's size
  const ULONGLONG HerSplashSameMs = 400;
  const float HerSplashSameMetres = 0.8f;
  // Her rings on the game's own water (2026-10-27, the user: "dropping from a
  // big jump should create a ripple if there is a puddle on the landing pad":
  // the 21:41 and 21:47 CT.log has her landings on the game's puddle decals,
  // material 47, where the tools draw no puddle, so no ring was made): a step
  // or landing whose surface the game gave as its water's (Hooks.cpp's
  // WetStep) rings there as in a puddle, a landing's as big (FallScale), when
  // no ring of the tools' is there for it. The shader draws them without a
  // puddle of its own, within 0.9 - 1.4 m of where she stepped (the decals are
  // 2.6 - 7.7 m across; their outlines aren't known). MostGameWaterRings
  // kept, sent with her rings (g_StepRingSize.z 1).
  const int MostGameWaterRings = 4;
  // Her landings on the Runners' landing pads (2026-10-28, the user: "landing
  // on a jumppad should leave a ripple", "i mean landingpads", a screenshot of
  // a soft-landing tarp; LandingPadTable.h): a tarp sags, too far from level
  // for a puddle, and the game's soft landing may not come as a step, so none
  // rang. Each frame (see NoteLandingPad), her feet within a tarp's box
  // (LandingPadSide each way along it, LandingPadOver over or under it) that
  // came down at LandingPadFall or more within FallWindowMs and now go down
  // slower than LandingPadStill: a landing's ring there (HardLandingRing,
  // FallScale), on whatever of its tarp faces up (WetLook.hlsl's StepRings,
  // g_StepRingSize.z 2), at most one each LandingPadSameMs. MostPadRings kept.
  const int MostPadRings = 2;
  // The KrugerSec down in the rain's water (2026-10-28, the user: "ksec
  // ragdoll/dying should leave behind their model ripples"): the lying ones
  // round the camera, as SnowBodies' LyingBodies has them (capsules from
  // their bones, when they lay down, their last knock), MostRippleBodies of
  // them sent each frame for WetLook.hlsl's BodyRipplesAt; their waves reach
  // RippleBodyReach past their body. Their knocks (NoteRagdollKnock) go to
  // NoteKnock; a knock's own round ring (BodyRing) isn't made where one of them
  // lies within RippleBodyKnockReach.
  const int MostRippleBodies = 3;
  const float RippleBodyReach = 4.5f, RippleBodyKnockReach = 1.5f;
  const float LandingPadSide = 0.5f, LandingPadOver = 1.f;
  const float LandingPadFall = 4.f, LandingPadStill = 1.f;   // m/s
  const ULONGLONG LandingPadSameMs = 1500;
  // The game's rain kept out under the roofs (weather::DryBoxes): no
  // raindrops' rings there (WetLook.hlsl's RainKeptOut), MostRainBoxes at most
  // (Weather.cpp's MaxBoxes)
  const int MostRainBoxes = 64;
  // Two rings for a landing (2026-10-26, the user: "when faith is jumping
  // there should be two puddles coming out of here leg", meaning ripples):
  // one at the first foot's contact, the other LandingFeetApart to her side
  // until the other foot's own contact comes in (within LandingSameMs and
  // LandingOtherReach of the first, at least LandingSameContact from it: the
  // same contact is asked twice, its sound's and its effect's), then there
  const float LandingFeetApart = 0.26f;
  // A roll in a puddle (wetlook::NoteRainRoll, 2026-10-26, the user:
  // "Rolling/jump rolling should also do ripples if there is a puddle"): two
  // rings this big, the second this far on the way she goes (her body over
  // the ground as she rolls), a pair every so often at most
  const float RollRingSize = 1.8f, RollRingAhead = 0.5f;
  const ULONGLONG RollRingEveryMs = 200;
  // Her wet hands' prints (wetlook::NoteRainHand, 2026-10-26, the user: "Can
  // you make the handprints also work for rain?"): her hands wet from a
  // puddle (all) or the wet ground (at least HandWetFromGround), drying
  // evenly over HandWetSeconds from the last wet touch, as her shoes do
  // (2026-10-26, the user: "Do the same for handprints as well": before,
  // they halved in 30 s and lost 0.4 a print), each print fainter; drier
  // than HandDryAt, none, and none where what she touches is as wet as her
  // hand (2026-11-03: only on what's drier). MostHandPrints kept, each fading
  // over HandPrintSeconds.
  // A touch in a puddle rings it (HandRingSize).
  const int MostHandPrints = 16;
  const float HandWetFromGround = 0.7f, HandWetSeconds = 10.f, HandDryAt = 0.03f;
  const float HandPrintSeconds = 60.f, HandRingSize = 0.6f;
  const ULONGLONG HandSameMs = 250;
  const ULONGLONG LandingSameMs = 300;
  const float LandingOtherReach = 0.6f, LandingSameContact = 0.06f;
  // Faith sliding through a puddle (wetlook::NoteSlide): a ring every
  // SlideRingEveryMs at most, lasting SlideRingSeconds, the MostSlideRings
  // newest
  const int MostSlideRings = 16;
  const ULONGLONG SlideRingEveryMs = 150;
  const float SlideRingSeconds = 1.1f;
  // Its trail (WetLook.hlsl's SlideWake): in a puddle or on the wet ground,
  // the points of her slide kept this long (2026-10-26, the user: "water
  // slide ripple effect should take a while to dissapear and should ripple
  // for a lot longer": its waves now go on for 6 s, were 1.6; 4 s before)
  const float SlideTrailSeconds = 6.5f;
  const int MostPrints = 24;
  const float StepRingSeconds = 1.6f;
  const float PrintSeconds = 60.f;
  const float ShoeWetFromGround = 0.7f;
  const float ShoeWetSeconds = 10.f;    // her shoes dry over this from her last wet step
  const float ShoeDryAt = 0.03f;        // drier than this: no more prints
  // At least this share of the wet look's wetness (PuddleAt's groundWet):
  // out under the open sky, in the rain (see SoakUp)
  const float OpenRainWet = 0.999f;
  // Which way along her transform's forward row she faces (see HerForward):
  // told by her moves over the ground (faster than FacingRunSpeed, within 45
  // degrees of the row), one a frame, MostFacingEvidence kept each way, sure
  // from FacingSure more one way than the other
  const float FacingRunSpeed = 2.5f;
  const int FacingSure = 20, MostFacingEvidence = 200;
  // Nothing taken up from ground under SoakFrom wet, and less the nearer it
  // is to it (see SoakUp)
  const float SoakFrom = 0.3f;
  // Her prints fade out as they come near being made over (of the
  // MostPrints kept, the PrintFadeSlots oldest, the oldest faintest), not
  // gone at once when a new one takes the place (2026-10-26, the user:
  // "footsteps should fade out instead of getting removed in an instance");
  // her handprints the same (HandPrintFadeSlots of MostHandPrints)
  const int PrintFadeSlots = 8, HandPrintFadeSlots = 5;
  // Her prints on the wet ground where there's no puddle (2026-10-26, the
  // user: "footsteps should appear on most concretes/sands if it has not a
  // puddle"; "i meant footprints"): on concrete and stone the water pressed
  // out of her sole's shape, in gravel pressed in, the rain filling them in
  // over PressedPrintSeconds. Not made since 2026-11-03 (the user: "remove
  // the footsteps/handprints from wet surfaces it should only be on dry"):
  // prints only on what's drier than her shoes, see StepWater
  const float PressedPrintSeconds = 8.f;
  // What a print is (StepMark's Kind): her wet shoes on drier ground, pressed
  // into wet concrete or stone, into wet gravel (those two no longer made),
  // on a wall she ran along
  enum PrintKind { PrintWetShoes = 0, PrintPressed = 1, PrintGravel = 2, PrintWall = 3 };
  const ULONGLONG StepSameMs = 250;
  const float StepSameMetres = 0.25f;

  // Rain dripping off roof edges (2026-10-26, the user: rain "come out of
  // gutters"; the game has no gutters, so it drips where a roof ends). From
  // the weather's squares (weather::GetShelter: per 2 m square looked at, its
  // floor and its roof): where a square with a roof meets one open to the
  // sky, three streams along their shared side (placed by a hash, so they
  // stay where they are), from the roof down to the open square's floor; the
  // MostDrips nearest within DripReach, each letting go DripRateMin -
  // DripRateMax drops a second (x the rain's strength), drawn and their
  // rings made by WetLook.hlsl's DripsAt and DripRings.
  const int MostDrips = 48;
  const float DripReach = 30.f;
  const float DripRateMin = 0.6f, DripRateMax = 2.2f;
  const float DripHalfWidth = 0.004f;    // m
  const float DripFallMin = 1.f, DripFallMax = 40.f;

  // The drainpipes' water (2026-10-26, the user: "and drainpipes"; the
  // outlets from weather::GetPipes, see Weather.cpp's UpdatePipes): the
  // MostPipes nearest on the frame within PipeDrawReach, each a stream
  // PipeHalfWidth wide at its outlet down to where it lands, drawn and its
  // rings made by WetLook.hlsl's PipesAt and PipeRings
  const int MostPipes = 16;
  const int MostPipeOutlets = 64;
  const float PipeDrawReach = 45.f;
  const float PipeHalfWidth = 0.018f;    // m

  struct Constants
  {
    XMFLOAT4X4 ViewProjection;   // camera-relative (the camera at 0)
    XMFLOAT4 CameraRight, CameraUp, CameraBack;
    XMFLOAT4 Camera;
    XMFLOAT4 ProjectionXY, ProjectionZW, ProjectionT;
    XMFLOAT4 Depth;
    XMFLOAT4 Frame;
    XMFLOAT4 Wet;
    XMFLOAT4 Rain;
    XMFLOAT4 Flash;
    XMFLOAT4 DistantRain;
    XMFLOAT4 OpenSkyGrid;
    XMFLOAT4 Bolt;
    XMFLOAT4 ShelterGrid;
    XMFLOAT4 Fog;
    XMFLOAT4 FogMisc;
    XMFLOAT4 BoltBox[MostBolts];
    XMFLOAT4 BoltRange[MostBolts];
    XMFLOAT4 BoltSegment[MostBoltSegments];
    XMFLOAT4 BoltWidth[MostBoltSegments];
    XMFLOAT4 DryNear;
    XMFLOAT4 DryCount;
    XMFLOAT4 DryBox[dry::MostBoxes * 4];
    XMFLOAT4 Snow;        // the snow (weather::GetSnow): how much falls, how many flakes, their size, its clock
    XMFLOAT4 SnowDrift;   // how far the wind has carried it (x, z), snow on the ground, how far
    XMFLOAT4 SnowMisc;    // 1 kept out of buildings
    XMFLOAT4 SnowBoxes;   // the flakes: how many in the near box, the far box; the boxes' sizes
    XMFLOAT4 SnowTiming;  // the snow's clock's period; 1 the frame before is known
    XMFLOAT4 SnowPrevious;              // the camera now from where it was the frame before
    XMFLOAT4X4 PrevViewProjection;      // the frame before's, relative to its own camera
    XMFLOAT4 InteriorCount;             // the game's interiors near the camera: boxes, prisms
    XMFLOAT4 InteriorBox[MostInteriorBoxes * 4];
    XMFLOAT4 InteriorPrism[MostInteriorPrisms * 9];
    XMFLOAT4 QueryGrid;   // PsQuery's squares round Faith's feet (see QuerySide)
    XMFLOAT4 SnowWind;    // the flakes: how fast the wind carries them (x, z), the frame's time
    XMFLOAT4 StepCounts;  // Faith's steps (see MostStepRings): rings, prints, a ring's seconds
    XMFLOAT4 StepRing[MostStepRings];
    XMFLOAT4 Print[MostPrints];
    XMFLOAT4 PrintDir[MostPrints];
    XMFLOAT4 IceCount;    // the ice over the water (see MostIcePlanes): planes, how far it has formed
    XMFLOAT4 IcePlane[MostIcePlanes * 4];
    XMFLOAT4 SnowPile;    // the snow on the ground: how deep it has piled (m), Faith's prints in it, how deep they're pressed (m)
    XMFLOAT4 SnowPrint[MostSnowPrints];
    XMFLOAT4 SnowPrintDir[MostSnowPrints];
    XMFLOAT4 DripCount;   // drips off roof edges (see MostDrips): how many, 1 it rains, a drop's half width (pixels at 1 m)
    XMFLOAT4 Drip[MostDrips];
    XMFLOAT4 DripInfo[MostDrips];
    XMFLOAT4 DripLand[MostDrips];
    XMFLOAT4 PipeCount;   // the drainpipes' water (see MostPipes): how many, how hard it runs, the stream's half width and pixels a metre (both at 1 m)
    XMFLOAT4 Pipe[MostPipes];
    XMFLOAT4 PipeInfo[MostPipes];
    XMFLOAT4 PipeLand[MostPipes];
    XMFLOAT4 StepRingSize[MostStepRings];   // per ring in StepRing: how big (1 a step), its seconds
    XMFLOAT4 SlideCount;  // Faith sliding through a puddle (see MostSlideRings): rings, a ring's seconds
    XMFLOAT4 SlideRing[MostSlideRings];
    XMFLOAT4 IceTri[MostIceTris * 4];      // the ice's water triangles (see MostIceTris; how many in IceCount.z)
    XMFLOAT4 SnowTrailCount;               // Faith's slides through the snow (see MostSnowTrails): stretches
    XMFLOAT4 SnowTrail[MostSnowTrails];
    XMFLOAT4 SnowTrailEnd[MostSnowTrails];
    XMFLOAT4 SnowRollCount;                // Faith's rolls in the snow (see MostSnowRolls): how many
    XMFLOAT4 SnowRoll[MostSnowRolls];
    XMFLOAT4 SnowRollWay[MostSnowRolls];
    XMFLOAT4 Haze;        // the haze in the rain or the snow (weather::GetHaze): how thick, 1 the snow's, its veils' drift (x, z)
    XMFLOAT4 HandCount;   // Faith's wet handprints (see MostHandPrints): how many
    XMFLOAT4 HandPrint[MostHandPrints];
    XMFLOAT4 HandFinger[MostHandPrints];
    XMFLOAT4 HandThumb[MostHandPrints];
    XMFLOAT4 Options;     // 1 the depth copy can be gathered (WetLook.hlsl's FarthestThrough)
    XMFLOAT4 HazeView;    // the haze (see HazeAt): how much the camera is out in the open
    XMFLOAT4 OtherCounts; // others' rings (see MostOtherRings) and the splashes (MostSplashes): how many of each, pixels a metre at 1 m, how much the camera looks down
    XMFLOAT4 OtherRing[MostOtherRings];
    XMFLOAT4 OtherRingSize[MostOtherRings];
    XMFLOAT4 Splash[MostSplashes];
    XMFLOAT4 SplashInfo[MostSplashes];
    XMFLOAT4 RainBoxCount;  // the game's rain kept out under the roofs (see MostRainBoxes): how many
    XMFLOAT4 RainBoxMin[MostRainBoxes];
    XMFLOAT4 RainBoxMax[MostRainBoxes];
    XMFLOAT4 SnowHandKeep[2];   // her hands' last touches (see g_HandKeep): where (relative to the camera), how much kept
    XMFLOAT4 SnowFeetKeep;      // her feet (dry::FaithFeet, relative to the camera), 1 known: her shoes kept as drawn (WetLook.hlsl's FeetKeptAt)
    XMFLOAT4 SnowStampCount;    // the stamps (SnowBodies.cpp: bodies pressed into the snow by their own meshes), the KrugerSec's boot prints
    XMFLOAT4 SnowStamp[snowbody::MostStamps];       // per stamp: its corner (relative to the camera), how much the snow hasn't filled
    XMFLOAT4 SnowStampAxis[snowbody::MostStamps];   // its long side's way (x, z), the ground's rise along it, its slice
    XMFLOAT4 OtherSnowPrint[MostOtherSnowPrints];   // the KrugerSec's boot prints (see MostOtherSnowPrints): where, how much is left
    XMFLOAT4 OtherSnowPrintDir[MostOtherSnowPrints];  // the way the toe points (x, z), that boot's outer side
    XMFLOAT4 HandStampCount;    // her handprints from her own hand (SnowBodies.cpp's hand stamps): how many in HandStampAt
    XMFLOAT4 HandStampAt[snowbody::MostHandStamps];   // its middle (relative to the camera), w how much is left (+ in the snow, - a wet one)
    XMFLOAT4 HandStampU[snowbody::MostHandStamps];    // its U axis (unit, in the surface's plane; along her fingers), w its slice
    XMFLOAT4 HandStampV[snowbody::MostHandStamps];    // its V axis (U x V the surface's facing)
    XMFLOAT4 BodyRippleCount;   // the KrugerSec down in the water (see MostRippleBodies): how many
    XMFLOAT4 BodyRipple[MostRippleBodies];     // first piece, pieces, seconds since he lay down, since his last knock (1e9 none)
    XMFLOAT4 BodyRippleAt[MostRippleBodies];   // his middle (relative to the camera), how far out his waves can be
    XMFLOAT4 BodySegA[MostRippleBodies * snowbody::LyingSegments];   // a capsule's end (relative), its radius
    XMFLOAT4 BodySegB[MostRippleBodies * snowbody::LyingSegments];   // its other end
    XMFLOAT4 GameFrame;         // the game's own G-buffer (GameFrame.h): x 1 copied this frame (t6 - t8, b1)
    XMFLOAT4 SnowShoe[4];       // her feet by her bones (snowbody::FaithShoes, relative to the camera), w 1 known: left ankle, left toe base, right ankle, right toe base
    XMFLOAT4 MarksWindow;       // the ground's marks (GroundMarks.h): x the map's uv per metre, yz its window's middle (world x, z), w its reach
    XMFLOAT4 MarksOn;           // x 1: its red is the snow's, y 1: its green is the water's (t9, s1)
    // What of a list can reach a pixel (see FrameBox, PlaceBox; WetLook.hlsl's g_DripBounds): on the frame xy least, zw
    // most (pixels); round the camera [0] least, [1] most (relative to the camera)
    XMFLOAT4 DripBounds;        // the drips' streams
    XMFLOAT4 PipeBounds;        // the drainpipes' water and its splash
    XMFLOAT4 SplashBounds;      // the little splashes at the ripples
    XMFLOAT4 HerSplashBounds;   // her own splash, the game's
    XMFLOAT4 DripLandBounds[2]; // where the drips land
    XMFLOAT4 PipeLandBounds[2]; // where the drainpipes' water lands
    XMFLOAT4 PrintBounds[2];    // her footprints
    // The kept-dry boxes by where they reach (MakeDryGrid; WetLook.hlsl's g_DryGrid, g_DryCells)
    XMFLOAT4 DryGrid;
    XMFLOAT4 DryCells[512];
    // The interiors' boxes by where they reach (MakeInteriorGrid; WetLook.hlsl's g_InteriorGrid, g_InteriorCells)
    XMFLOAT4 InteriorGrid;
    XMFLOAT4 InteriorCells[256];
    // The falling water's streams by the frame's tiles (FallingTiles; WetLook.hlsl's g_FallingGrid, g_FallingTiles)
    XMFLOAT4 FallingGrid;
    XMFLOAT4 FallingTiles[64];
    // What of a list can reach a point (MakeListBounds; WetLook.hlsl's g_SnowPrintBounds and the others): [0] least,
    // [1] most (relative to the camera)
    XMFLOAT4 SnowPrintBounds[2];   // her prints in the snow
    XMFLOAT4 BootPrintBounds[2];   // the KrugerSec's boot prints
    XMFLOAT4 HandStampBounds[2];   // her hand's stamps
    XMFLOAT4 SnowTrailBounds[2];   // her slides' trails
    XMFLOAT4 SnowRollBounds[2];    // her rolls
    XMFLOAT4 SnowStampBounds[2];   // the bodies' stamps
    XMFLOAT4 RainBoxBounds[2];     // the game's rain kept out under the roofs
  };
  // Where fxc's reflection (the WetLook*.h headers' comments) puts them in WetParams
  static_assert(offsetof(Constants, Snow) == 16960, "WetParams: g_Snow moved");
  static_assert(offsetof(Constants, QueryGrid) == 19472, "WetParams: g_QueryGrid moved");
  static_assert(offsetof(Constants, SnowWind) == 19488, "WetParams: g_SnowWind moved");
  static_assert(offsetof(Constants, StepCounts) == 19504, "WetParams: g_StepCounts moved");
  static_assert(offsetof(Constants, IceCount) == 20480, "WetParams: g_IceCount moved");
  static_assert(offsetof(Constants, SnowPile) == 23568, "WetParams: g_SnowPile moved");
  static_assert(offsetof(Constants, SnowPrintDir) == 24608, "WetParams: g_SnowPrintDir moved");
  static_assert(offsetof(Constants, PipeCount) == 27952, "WetParams: g_PipeCount moved");
  static_assert(offsetof(Constants, StepRingSize) == 28736, "WetParams: g_StepRingSize moved");
  static_assert(offsetof(Constants, IceTri) == 29200, "WetParams: g_IceTri moved");
  static_assert(offsetof(Constants, SnowTrailEnd) == 36128, "WetParams: g_SnowTrailEnd moved");
  static_assert(offsetof(Constants, SnowRollCount) == 36896, "WetParams: g_SnowRollCount moved");
  static_assert(offsetof(Constants, SnowRollWay) == 37040, "WetParams: g_SnowRollWay moved");
  static_assert(offsetof(Constants, Haze) == 37168, "WetParams: g_Haze moved");
  static_assert(offsetof(Constants, HandCount) == 37184, "WetParams: g_HandCount moved");
  static_assert(offsetof(Constants, Options) == 37968, "WetParams: g_Options moved");
  static_assert(offsetof(Constants, HazeView) == 37984, "WetParams: g_HazeView moved");
  static_assert(offsetof(Constants, OtherCounts) == 38000, "WetParams: g_OtherCounts moved");
  static_assert(offsetof(Constants, RainBoxCount) == 39296, "WetParams: g_RainBoxCount moved");
  static_assert(offsetof(Constants, SnowHandKeep) == 41360, "WetParams: g_SnowHandKeep moved");
  static_assert(offsetof(Constants, SnowFeetKeep) == 41392, "WetParams: g_SnowFeetKeep moved");
  static_assert(offsetof(Constants, SnowStampCount) == 41408, "WetParams: g_SnowStampCount moved");
  static_assert(offsetof(Constants, SnowStampAxis) == 41680, "WetParams: g_SnowStampAxis moved");
  static_assert(offsetof(Constants, OtherSnowPrint) == 41936, "WetParams: g_OtherSnowPrint moved");
  static_assert(offsetof(Constants, OtherSnowPrintDir) == 42704, "WetParams: g_OtherSnowPrintDir moved");
  static_assert(offsetof(Constants, HandStampCount) == 43472, "WetParams: g_HandStampCount moved");
  static_assert(offsetof(Constants, HandStampU) == 44000, "WetParams: g_HandStampU moved");
  static_assert(offsetof(Constants, HandStampV) == 44512, "WetParams: g_HandStampV moved");
  static_assert(offsetof(Constants, BodyRippleCount) == 45024, "WetParams: g_BodyRippleCount moved");
  static_assert(MostRippleBodies * snowbody::LyingSegments == 48, "WetParams: g_BodySegA's size");
  static_assert(offsetof(Constants, BodySegB) == offsetof(Constants, BodySegA) + 16 * 48, "WetParams: g_BodySegB moved");
  static_assert(offsetof(Constants, GameFrame) == 46672, "WetParams: g_GameFrame moved");
  static_assert(offsetof(Constants, SnowShoe) == 46688, "WetParams: g_SnowShoe moved");
  static_assert(offsetof(Constants, MarksWindow) == 46752, "WetParams: g_MarksWindow moved");
  static_assert(offsetof(Constants, DripBounds) == 46784, "WetParams: g_DripBounds moved");
  static_assert(offsetof(Constants, DripLandBounds) == 46848, "WetParams: g_DripLandBounds moved");
  static_assert(offsetof(Constants, PrintBounds) == 46912, "WetParams: g_PrintBounds moved");
  static_assert(offsetof(Constants, DryGrid) == 46944, "WetParams: g_DryGrid moved");
  static_assert(offsetof(Constants, DryCells) == 46960, "WetParams: g_DryCells moved");
  static_assert(offsetof(Constants, InteriorGrid) == 55152, "WetParams: g_InteriorGrid moved");
  static_assert(offsetof(Constants, InteriorCells) == 55168, "WetParams: g_InteriorCells moved");
  static_assert(offsetof(Constants, FallingGrid) == 59264, "WetParams: g_FallingGrid moved");
  static_assert(offsetof(Constants, FallingTiles) == 59280, "WetParams: g_FallingTiles moved");
  static_assert(offsetof(Constants, SnowPrintBounds) == 60304, "WetParams: g_SnowPrintBounds moved");
  static_assert(offsetof(Constants, RainBoxBounds) == 60496, "WetParams: g_RainBoxBounds moved");
  static_assert(sizeof(Constants) == 60528, "WetParams: its size changed");

  // The wet ground in the game's own G-buffer (GbufferWet.cpp; 2026-11-02,
  // the user: "Lets go for A option"), render thread: the constants as the
  // last Apply wrote them (filled: that Apply got as far as writing them),
  // the views its pass had with them, and the copy its pass reads, with this
  // frame's camera (see wetlook::TakeGbufferPass)
  Constants g_LastConstants{};
  bool g_LastFilled = false;
  ComPtr<ID3D11ShaderResourceView> g_LastViews[4];   // t2, t3, t4, t9
  ComPtr<ID3D11Buffer> g_GbufferConstants;

  std::mutex g_Mutex;
  // The timing (see wetlook::NoteTime): per part its sum, most and how many,
  // since the last line in the log
  struct TimeSum { double Total, Most; int Count; };
  std::mutex g_TimeMutex;
  TimeSum g_Times[wetlook::TimeParts] = {};
  ULONGLONG g_TimeLogTick = 0;
  int g_TimeFrames = 0;
  // The GPU's: timestamps round the pass and round the ice and the snow's
  // flakes, read GpuTimeSlots frames later (never waited for)
  const int GpuTimeSlots = 4;
  struct GpuTimes { ComPtr<ID3D11Query> Disjoint, Begin, Pass, End; bool Pending, Snow; };
  GpuTimes g_GpuTimes[GpuTimeSlots];
  int g_GpuTimeNext = 0;
  int g_GpuTimesState = 0;   // 0 not made, 1 made, -1 couldn't be
  wetlook::Settings g_Settings{ false, 0.8f, 0.35f, 2.5f, 0.8f, true, 200.f };
  // What the last frame drawn gave the shader for its puddles (InPuddle);
  // under g_Mutex
  struct DrawnWet
  {
    float Camera[3];
    XMFLOAT4 Wet;    // its g_Wet: wetness, puddles (x the rain), puddle size
    XMFLOAT4 Rain;   // its g_Rain: y Wet up to, z 1 the wet look on
    XMFLOAT4 Fog;    // its g_Fog
    float FogMin;    // its g_FogMisc.y
    bool FogOn;      // its g_FogMisc.z
    float Right[3];    // the camera's right and the way it looks (for FootSide)
    float Forward[3];
    float Up[3];
  };
  DrawnWet g_DrawnWet{};

  // Faith's steps (see MostStepRings), under g_Mutex
  struct StepMark
  {
    float At[3];
    float Dir[2];       // prints: the way she was going (x, z)
    float Out[2];       // prints: that foot's outer side (x, z; see FootSide)
    float Wet;          // prints: how wet when made
    ULONGLONG Tick;     // 0: none
    float Size;         // rings: how big (1 a step, see SoftLandingRing)
    int Kind;           // prints: which (PrintKind)
  };
  StepMark g_StepRings[MostStepRings] = {};
  // Her own splashes, the game's (see MostHerSplashes)
  StepMark g_HerSplashes[MostHerSplashes] = {};
  int g_HerSplashNext = 0;
  int g_HerSplashCount = 0;
  StepMark g_GameWaterRings[MostGameWaterRings] = {};   // see MostGameWaterRings
  int g_GameWaterRingNext = 0;
  ULONGLONG g_GameWaterRingLoggedMs = 0;
  StepMark g_PadRings[MostPadRings] = {};   // see MostPadRings
  ULONGLONG g_RippleBodiesLoggedTick = 0;   // see MostRippleBodies
  // A lying KrugerSec's times on the rings' clock (2026-10-28, the user:
  // "These ripples don't freeze while the game is paused, unlike Faith's step
  // rings. Can we make them freeze?"): snowbody's DownAt and KnockAt
  // (GetTickCount64) each taken onto g_PlayMs when it's first seen, his
  // waves' ages from that, so they stop while the game is paused. The least
  // lately seen of them taken for a new body. Render thread
  struct RippleClock { __int64 Owner; ULONGLONG DownAt, KnockAt, PlayDown, PlayKnock, Seen; };
  RippleClock g_RippleClocks[2 * MostRippleBodies] = {};
  int g_PadRingNext = 0;
  ULONGLONG g_PadRingTick = 0, g_PadLoggedMs = 0, g_PadNearLoggedMs = 0;
  ULONGLONG g_HerSplashLoggedMs = 0;
  // Diagnostic (2026-11-02, the user's screenshot of a pool partly ice, partly
  // water: "You can still see the snow interacting with the water z fighting
  // on water fix it"): while the ice is drawn (set each frame by Apply), at
  // Faith's step in the game's water, the piece of the ice under her (see
  // IcePieceUnder), once in 30 s at most
  std::atomic<bool> g_IcedNow{ false };
  ULONGLONG g_IceStepLoggedMs = 0;

  // The ice's piece under a point (world; IceTable.h's own, read only): a
  // pool's rectangle or one of the water's own triangles over or under it,
  // across the ground, the nearest in height within 2 m. height: the
  // piece's there; triangle: it's one of the water's triangles (else a
  // pool's rectangle). False: none (no ice is drawn there).
  bool IcePieceUnder(float const* at, float& height, bool& triangle)
  {
    bool found = false;
    float best = 2.f;
    for (icetable::IcePlaneData const& p : icetable::IcePlanes)
    {
      if (p.Type != icetable::Pool)
        continue;
      // (its place across the ground in its two half sides: the 2 x 2 solved)
      float dx = at[0] - p.Centre[0], dz = at[2] - p.Centre[2];
      float det = p.A[0] * p.B[2] - p.B[0] * p.A[2];
      if (fabsf(det) < 1e-6f)
        continue;
      float u = (dx * p.B[2] - p.B[0] * dz) / det, v = (p.A[0] * dz - dx * p.A[2]) / det;
      if (fabsf(u) > 1.f || fabsf(v) > 1.f)
        continue;
      float y = p.Centre[1] + p.A[1] * u + p.B[1] * v;
      if (fabsf(y - at[1]) < best)
      {
        best = fabsf(y - at[1]);
        height = y;
        triangle = false;
        found = true;
      }
    }
    for (icetable::IceTriangleData const& r : icetable::IceTriangles)
    {
      float e0x = r.B[0] - r.A[0], e0z = r.B[2] - r.A[2], e1x = r.C[0] - r.A[0], e1z = r.C[2] - r.A[2];
      float det = e0x * e1z - e1x * e0z;
      if (fabsf(det) < 1e-6f)
        continue;
      float px = at[0] - r.A[0], pz = at[2] - r.A[2];
      float u = (px * e1z - e1x * pz) / det, v = (e0x * pz - px * e0z) / det;
      if (u < 0.f || v < 0.f || u + v > 1.f)
        continue;
      float y = r.A[1] + (r.B[1] - r.A[1]) * u + (r.C[1] - r.A[1]) * v;
      if (fabsf(y - at[1]) < best)
      {
        best = fabsf(y - at[1]);
        height = y;
        triangle = true;
        found = true;
      }
    }
    return found;
  }
  StepMark g_SlideRings[MostSlideRings] = {};
  int g_SlideRingNext = 0;
  float g_LastSlide[3] = {};
  ULONGLONG g_LastSlideTick = 0;
  int g_LastStepRing = -1;   // the last step's ring (a landing's other foot makes it bigger)
  // Each foot's last step (see NoteStep's foot), under g_Mutex: where, when,
  // whether in a puddle, its ring
  float g_FootLast[2][3] = {};
  ULONGLONG g_FootLastTick[2] = {};
  bool g_FootLastPuddle[2] = {};
  int g_FootLastRing[2] = { -1, -1 };
  // A landing's two rings, one from each leg (see NoteLanding)
  int g_LandingRing[2] = { -1, -1 };
  ULONGLONG g_LandingTick = 0;
  ULONGLONG g_LastRollTick = 0;        // under g_Mutex (see NoteRainRoll)
  // Her wet hands and their prints (see MostHandPrints), under g_Mutex
  struct HandMark
  {
    float At[3];
    float Finger[3];   // the way her fingers point, in the surface's plane
    float Thumb[3];    // her thumb's side, in it
    float Wet;         // how wet her hand was (PsMain draws as much of it as is wetter than the surface)
    ULONGLONG Tick;    // 0: none
    int Stamp;         // her own hand's stamp for it (snowbody::NoteHandTouch; -1 none, 0 never a real one)
  };
  HandMark g_HandPrints[MostHandPrints] = {};
  int g_HandPrintNext = 0;
  float g_HandWet = 0.f;
  ULONGLONG g_HandWetTick = 0, g_HandPrintsLoggedTick = 0;
  float g_RainHand[2][3] = {};
  ULONGLONG g_RainHandTick[2] = {};
  bool g_LandingOtherSeen = false;
  StepMark g_Prints[MostPrints] = {};
  int g_StepRingNext = 0, g_PrintNext = 0;
  float g_LastStep[3] = {};
  float g_LastDir[2] = { 0.f, 1.f };
  // Which foot her steps are (see FootSide), under g_Mutex
  float g_StepBefore[3] = {};          // the step before her last
  ULONGLONG g_StepBeforeTick = 0;
  float g_WalkDir[2] = { 0.f, 1.f };   // the way she walks, over two steps
  float g_FootSide = 1.f;              // 1 her right foot, -1 her left
  float g_FootOutward[2] = { 1.f, 0.f };
  float g_Handedness = 0.f;            // see FootSide (0: not known yet)
  // Her transform's forward row (see HerForward), under g_Mutex
  float g_FacingRaw[2] = {};
  bool g_FacingRawKnown = false;
  int g_FacingEvidence = 0;
  bool g_FacingLogged = false;
  ULONGLONG g_LastStepTick = 0;
  float g_ShoeWet = 0.f;                // how wet her shoes are (see ShoeWetFromGround)
  // The rings' and the prints' clock (ms): real time, standing still while
  // the game is paused (Apply moves it on, see gamestate::Paused), so they
  // wait with the game's world
  std::atomic<ULONGLONG> g_PlayMs{ 1000 };
  // (from TimeNow, the steady clock, the ms left over carried on: see the
  // drawn rain's clock in Apply)
  double g_PlayFrom = 0.0, g_PlayCarry = 0.0;   // render thread
  // The drawn clock (the constants' Camera.w, s) and when it last moved (ms;
  // render thread). Apply's early-outs (the game's loading screen, its menus)
  // keep its base current, so it doesn't jump when drawing goes on.
  double g_DrawnClock = 0.0, g_DrawnClockMs = 0.0;
  // Her steps on a wall she runs along (see wetlook::NoteWallStep), under
  // g_Mutex: each foot's last, when
  float g_WallStep[2][3] = {};
  ULONGLONG g_WallStepTick[2] = {};
  // Her feet's moves each frame (see FallScale), under g_Mutex
  struct FallSample { double Ms; float Down, Along; };
  FallSample g_FallSamples[MostFallSamples] = {};
  int g_FallNext = 0;
  float g_FallLast[3] = {};
  double g_FallLastMs = 0.0;
  ULONGLONG g_FallLoggedTick = 0;
  // Other people's and the cars' rings, the game's own impacts of theirs
  // lately (see MostOtherRings), under g_Mutex
  StepMark g_OtherRings[MostOtherRings] = {};
  struct OwnStep { float At[3]; ULONGLONG Tick; };
  OwnStep g_OwnSteps[MostOwnSteps] = {};
  // Downed bodies' knocks waiting for the ground under them (see BodyRing),
  // under g_Mutex
  struct BodyKnock { float At[3]; ULONGLONG Tick; bool Waiting; };
  BodyKnock g_BodyKnocks[MostBodyKnocks] = {};
  int g_BodyKnockNext = 0;
  float g_LastBody[3] = {};
  ULONGLONG g_LastBodyTick = 0;
  int g_OwnStepNext = 0;
  ULONGLONG g_OtherLoggedTick[2] = {};
  ULONGLONG g_WallPrintLoggedTick = 0;
  ULONGLONG g_ShoeWetTick = 0;          // under g_Mutex: when her shoes were last wetted (g_PlayMs)
  // How wet her shoes are now: as wet as they were made, drying evenly over
  // ShoeWetSeconds since (see ShoeWetSeconds). g_Mutex held.
  float ShoeWetNow()
  {
    float since = g_ShoeWetTick ? (float)(g_PlayMs.load() - g_ShoeWetTick) / 1000.f : 1e9f;
    return g_ShoeWet * (std::max)(0.f, 1.f - since / ShoeWetSeconds);
  }
  // How wet her hands are now (as her shoes: drying evenly over
  // HandWetSeconds since they were last wetted). g_Mutex held.
  float HandWetNow()
  {
    float since = g_HandWetTick ? (float)(g_PlayMs.load() - g_HandWetTick) / 1000.f : 1e9f;
    return g_HandWet * (std::max)(0.f, 1.f - since / HandWetSeconds);
  }
  // Her shoes wetted (a puddle 1, the wet ground ShoeWetFromGround): at
  // least that wet from now. g_Mutex held.
  void WetShoes(float wet)
  {
    g_ShoeWet = (std::max)(ShoeWetNow(), wet);
    g_ShoeWetTick = g_PlayMs.load();
  }
  // Her shoes or hands (level: as wet as they were made, at tick; now: as
  // wet as they are now) on what's as wet as wet (0 - 1, the share of the
  // wet look's wetness drawn there: PuddleAt's groundWet), which gives them
  // fromWet where it's all wet and less where it's less: as wet as it gives
  // them, when that's wetter; out in the rain (all wet) they don't dry
  // either, kept as wet as they are. Where it's less wet (a roof's edge)
  // they go on drying, so they're never kept wetter there than it gives
  // them. Returns how wet they are now. (2026-10-26, the user: "We also need
  // a better algorithm for what is wet and dry detection for the
  // footprints": before, half wet was wet and less was dry; at a roof's
  // edge, where the roof squares' 2 m soften the line under the roof, her
  // steps went wet, dry, wet, her shoes were wetted again at each wet one,
  // and the dry ones left prints on ground drawn up to half wet.) g_Mutex
  // held.
  float SoakUp(float& level, ULONGLONG& tick, float now, float wet, float fromWet)
  {
    // (nothing from ground barely wet, under SoakFrom: a roof square's soft
    // edge round a door wetted them a little, and they left a faint print or
    // two indoors: CT.log 19:16:44 and 19:17:16, "as wet as 0.07" and "0.18"
    // on ground at 0.00; 2026-10-26, the user: "indoors there is still one
    // footstep spawning")
    float take = fromWet * (std::min)((std::max)((wet - SoakFrom) / (1.f - SoakFrom), 0.f), 1.f);
    if (wet < OpenRainWet && take < now)
      return now;
    level = (std::max)(now, take);
    tick = g_PlayMs.load();
    return level;
  }
  bool g_LastStepPuddle = false;        // her last step was in a puddle (its rings made)
  // Her prints in the snow (see MostSnowPrints), under g_Mutex
  struct SnowPrintMark
  {
    float At[3];
    float Dir[2];       // the way she was going (x, z); a hand's: the way its fingers point
    float Out[2];       // that foot's outer side (ShoeSole's); a hand's: its thumb's side
    float Left;         // how much of it the snow hasn't filled (0: none)
    bool Hand;          // a handprint (WetLook.hlsl's HandShape)
    int Stamp;          // a handprint's own hand's stamp (snowbody::NoteHandTouch; -1 none)
  };
  SnowPrintMark g_SnowPrints[MostSnowPrints] = {};
  int g_SnowPrintNext = 0;
  // The KrugerSec's boot prints (see MostOtherSnowPrints), under g_Mutex
  struct OtherSnowPrintMark
  {
    float At[3];
    float Dir[2];       // the way its toe points (x, z)
    float Out[2];       // that boot's outer side
    float Left;         // how much of it the snow hasn't filled (0: none)
    ULONGLONG Tick;
  };
  OtherSnowPrintMark g_OtherSnowPrints[MostOtherSnowPrints] = {};
  int g_OtherSnowPrintNext = 0;
  ULONGLONG g_OtherSnowPrintsLoggedTick = 0;
  ULONGLONG g_SnowHandsLoggedTick = 0;
  ULONGLONG g_SnowStampsPlayMs = 0;     // render thread: the play clock when the stamps were last filled
  float g_SnowLastStep[3] = {};
  float g_SnowLastDir[2] = { 0.f, 1.f };
  ULONGLONG g_SnowLastStepTick = 0;
  ULONGLONG g_SnowPrintsLoggedTick = 0;
  ULONGLONG g_SnowPrintsPlayMs = 0;     // render thread: the play clock when the prints were last filled
  float g_SnowPrevStep[3] = {};         // her step in the snow before the last
  ULONGLONG g_SnowPrevStepTick = 0;
  float g_SnowFootSide = 1.f;           // her last step in the snow: 1 her right foot, -1 her left
  float g_SnowFootOut[2] = { 1.f, 0.f };  // its outer side
  float g_SnowHand[2][3] = {};          // her hands' last prints (left, right)
  // Where her hands last touched (left, right; snow or not) and when (the
  // play clock, ms; 0 never): what's just over a touch is her hand, kept as
  // the game drew it (WetLook.hlsl's HandKeptAt). Under g_Mutex.
  float g_HandKeep[2][3] = {};
  ULONGLONG g_HandKeepMs[2] = {};
  ULONGLONG g_SnowHandTick[2] = {};
  bool g_SnowJumpPressed = false;       // her last step's other foot pressed (a take-off, or a landing's two)
  ULONGLONG g_SnowFellTick = 0;         // when her feet last fell faster than SnowLandFall
  // Her slides' trails in the snow (see MostSnowTrails), under g_Mutex
  struct SnowTrailMark
  {
    float Start[3];
    float End[3];
    float Left;         // how much of it the snow hasn't filled (0: none)
    unsigned int Trail; // which slide (her last stretch of each is her legs)
    unsigned int Seq;   // its order
  };
  unsigned int g_SnowTrailId = 0, g_SnowTrailSeq = 0;
  bool g_SnowTrailFresh = false;        // her slide's first stretch still to come (her hips' start with it)
  SnowTrailMark g_SnowTrails[MostSnowTrails] = {};
  int g_SnowTrailNext = 0;
  float g_SnowTrailLast[3] = {};        // where her trail got to
  ULONGLONG g_SnowTrailLastTick = 0;
  ULONGLONG g_SnowTrailLoggedTick = 0;
  ULONGLONG g_SnowSlideTick = 0;        // her slide effect last played where the snow lies
  // Her rolls in the snow (see MostSnowRolls), under g_Mutex
  struct SnowRollMark
  {
    float Start[3];     // the contact the game looked up for it
    float Way[2];       // the way she rolled (x, z; unit)
    float Left;         // how much of it the snow hasn't filled (0: none)
    ULONGLONG Tick;     // when it began
    bool Logged;        // its distance from her feet said (one a while)
  };
  SnowRollMark g_SnowRolls[MostSnowRolls] = {};
  int g_SnowRollNext = 0;
  int g_SnowRollFollowing = -1;         // the roll her feet still give the way of (-1 none)
  ULONGLONG g_SnowRollsLoggedTick = 0;
  // Her feet the frame before (render thread; under g_Mutex as the steps are)
  float g_FeetPrev[3] = {};
  ULONGLONG g_FeetPrevTick = 0;
  ULONGLONG g_PrintsLoggedTick = 0;

  // What the frame's surface says about puddles round Faith's feet, for her
  // steps (InPuddle): WetLook.hlsl's PsQuery works out FlatAround x
  // PuddleSurface (no puddles on holed metal, stairs, narrow tops) for
  // QuerySide x QuerySide squares of QueryCell metres round her feet each
  // frame the puddles are drawn, read back a couple of frames later (never
  // waited for) and kept by square for SurfaceKeepMs: her steps are often
  // out of the frame, on ground it showed a moment before.
  const int QuerySide = 32;
  const float QueryCell = 0.25f;
  const int QueryCopies = 3;
  const ULONGLONG SurfaceKeepMs = 10000;
  const int SurfaceSide = 64;            // squares kept: a 16 x 16 m window round wherever she's been last
  struct SurfaceSquare
  {
    int X, Z;           // which square (world x, z / QueryCell, floored)
    float Value;        // FlatAround x PuddleSurface there
    float Y;            // its surface's height (world)
    ULONGLONG Tick;     // when (0: never)
  };
  SurfaceSquare g_SurfaceSquares[SurfaceSide * SurfaceSide] = {};   // under g_Mutex
  // To the right of them in the same target, SlopeEndsAcross x
  // SlopeEndsDown cells over the frame ask where the rain running down a
  // slope ends at a drop (WetLook.hlsl's SlopeEndAt, the same numbers there;
  // 2026-10-26, the user: "also after the water slope ends they should be
  // water dripping from the end of it"), read back with them. Each end found
  // is kept (MostSlopeEnds at most; one within SlopeEndSame of another is the
  // same) for SlopeEndKeepMs from when it was last found; where its drops
  // land is swept for on the game thread (LandSlopeEnds, at most
  // SlopeEndLandsPerFrame a frame: none under SlopeDripFallMin, the least
  // the drips' shader draws), then it drips as the roofs' edges do (MakeDrips).
  const int SlopeEndsAcross = 64, SlopeEndsDown = 36;
  const int QueryWidth = QuerySide + SlopeEndsAcross;
  const int QueryHeight = QuerySide > SlopeEndsDown ? QuerySide : SlopeEndsDown;
  // And under the squares (row HazeSkyRow, WetLook.hlsl's), the sky's light
  // by the horizon for the haze's colour (WetLook.hlsl's HazeSkyAt, HazeTint):
  // read back with them and smoothed over HazeSkySeconds (slower the less of
  // the frame was sky), held while no sky is seen
  const int HazeSkyRow = 33;
  const float HazeSkySeconds = 1.f;
  const int MostSlopeEnds = 64;
  const float SlopeEndSame = 0.6f;
  const ULONGLONG SlopeEndKeepMs = 20000;
  const int SlopeEndLandsPerFrame = 8;
  const float SlopeDripFallMin = 0.5f;
  struct SlopeEnd
  {
    float At[3];       // where its water lets go (world)
    float Bottom;      // where its drops land (world y; NaN: nowhere, or not swept for yet)
    bool Landed;       // swept for
    ULONGLONG Seen;    // when last found (0: a free one)
  };
  SlopeEnd g_SlopeEnds[MostSlopeEnds] = {};   // under g_Mutex
  // The sky's light for the haze (see HazeSkyRow), under g_Mutex
  float g_HazeSky[3] = {};
  bool g_HazeSkyKnown = false;
  double g_HazeSkyMs = 0.0;
  std::atomic<bool> g_Drawn{ false };
  std::atomic<bool> g_Failed{ false };

  // Render thread only
  ComPtr<ID3D11VertexShader> g_Vs;
  ComPtr<ID3D11PixelShader> g_Ps;
  // The pass's copy without the snow on the ground (WetLook.hlsl's RAIN_ONLY,
  // WetLookRainPs.h), drawn while none lies (snow.Cover not over 0.001, the
  // pass's own test): the same in the rain, fewer of the GPU's registers kept
  // for each pixel (see WetLook.hlsl's SnowDrawn). None if it couldn't be
  // made: the whole one then.
  ComPtr<ID3D11PixelShader> g_RainPs;
  // And its copy without the rain's parts (WetLook.hlsl's SNOW_ONLY,
  // WetLookSnowGroundPs.h), drawn while snow lies and nothing of the rain's
  // is on this frame (Apply's rainPartsOn: the wet look, the drips, the
  // drainpipes, the splashes, the distant rain): then the same as the whole
  // one, with fewer registers kept for each pixel (see WetLook.hlsl's
  // WetDrawn). None if it couldn't be made: the whole one then.
  ComPtr<ID3D11PixelShader> g_SnowGroundPs;
  ComPtr<ID3D11PixelShader> g_QueryPs;
  ComPtr<ID3D11Texture2D> g_Query;
  ComPtr<ID3D11RenderTargetView> g_QueryTarget;
  ComPtr<ID3D11Texture2D> g_QueryCopy[QueryCopies];
  struct QueryCopyInfo
  {
    bool Pending;
    bool Squares;           // Faith's squares asked about (else only the slopes' ends)
    int CornerX, CornerZ;   // the grid's corner square
    float CameraX, CameraY, CameraZ;   // the frame's camera (the query's points are relative to it)
  };
  QueryCopyInfo g_QueryCopyInfo[QueryCopies] = {};
  int g_QueryNext = 0;
  bool g_QueryMade = false;
  ComPtr<ID3D11Buffer> g_Constants;
  ComPtr<ID3D11SamplerState> g_Sampler;
  ComPtr<ID3D11SamplerState> g_WrapSampler;   // the ground's marks' map (s1): it wraps round
  ComPtr<ID3D11RasterizerState> g_Rasterizer;
  ComPtr<ID3D11BlendState> g_Blend;
  ComPtr<ID3D11DepthStencilState> g_Depth;
  // The snow's flakes, drawn over the pass (alpha blended); none if they
  // couldn't be made (the rest still is)
  ComPtr<ID3D11VertexShader> g_SnowVs;
  ComPtr<ID3D11PixelShader> g_SnowPs;
  ComPtr<ID3D11BlendState> g_SnowBlend;
  bool g_LoggedFlakes = false;
  // The ice over the city's water (WetLook.hlsl's IceVsMain / IcePsMain):
  // every piece of IceTable.h as triangles, made once (world places: x y z
  // and the tolerance; a rectangle's metres across and up, its kind, a
  // fall's foot; a rectangle's half sides), drawn over the pass with the
  // flakes' blend; none if they couldn't be made
  ComPtr<ID3D11VertexShader> g_IceVs;
  ComPtr<ID3D11PixelShader> g_IcePs;
  ComPtr<ID3D11InputLayout> g_IceLayout;
  ComPtr<ID3D11Buffer> g_IceVertices;
  UINT g_IceVertexCount = 0;
  struct IceVertexData { float At[4]; float Local[4]; float Shape[4]; };
  // The frame before's camera, for the flakes' motion blur
  XMFLOAT4X4 g_PrevViewProjection;
  float g_PrevCamera[3] = {};
  ULONGLONG g_PrevTick = 0;
  double g_PrevMs = 0.0;   // the same, from the steady clock (TimeNow), for the frame's time
  // The squares round the camera open to the sky (weather::GetOpenSky): the
  // rain the tools draw falls only there
  ComPtr<ID3D11Texture2D> g_OpenSky;
  ComPtr<ID3D11ShaderResourceView> g_OpenSkyView;
  int g_OpenSkyCount = 0;
  std::vector<float> g_OpenSkyCells;   // render thread
  // The weather's squares for the wet surfaces and the flash (weather::GetShelter)
  ComPtr<ID3D11Texture2D> g_Shelter;
  ComPtr<ID3D11ShaderResourceView> g_ShelterView;
  int g_ShelterCount = 0;
  std::vector<float> g_ShelterCells;   // render thread
  std::vector<float> g_ShelterEdges;   // render thread: where the roofs end (weather::GetShelter's edges)
  ComPtr<ID3D11Texture2D> g_SceneCopy;
  ComPtr<ID3D11ShaderResourceView> g_SceneCopyView;
  D3D11_TEXTURE2D_DESC g_SceneCopyDesc{};
  UINT g_SceneCopyLevels = 1;
  // Its shaders and states (see Make), made once from the game's first
  // frame (see StartMaking): 0 not begun, 1 being made, 2 made, -1 failed
  std::atomic<int> g_MakeState{ 0 };
  bool g_LoggedDrawn = false;
  D3DStateBackup g_Backup;   // big: its class instance lists

  DXGI_FORMAT TypedFormat(DXGI_FORMAT format)
  {
    switch (format)
    {
    case DXGI_FORMAT_R8G8B8A8_TYPELESS: return DXGI_FORMAT_R8G8B8A8_UNORM;
    case DXGI_FORMAT_B8G8R8A8_TYPELESS: return DXGI_FORMAT_B8G8R8A8_UNORM;
    case DXGI_FORMAT_B8G8R8X8_TYPELESS: return DXGI_FORMAT_B8G8R8X8_UNORM;
    case DXGI_FORMAT_R10G10B10A2_TYPELESS: return DXGI_FORMAT_R10G10B10A2_UNORM;
    case DXGI_FORMAT_R16G16B16A16_TYPELESS: return DXGI_FORMAT_R16G16B16A16_FLOAT;
    default: return format;
    }
  }

  bool Make(ID3D11Device* pDevice)
  {
    D3D11_BUFFER_DESC constants{};
    constants.ByteWidth = sizeof(Constants);
    constants.Usage = D3D11_USAGE_DYNAMIC;
    constants.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    constants.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    D3D11_SAMPLER_DESC sampler{};
    sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler.MaxLOD = D3D11_FLOAT32_MAX;

    D3D11_RASTERIZER_DESC rasterizer{};
    rasterizer.FillMode = D3D11_FILL_SOLID;
    rasterizer.CullMode = D3D11_CULL_NONE;
    rasterizer.DepthClipEnable = TRUE;

    D3D11_BLEND_DESC blend{};
    blend.RenderTarget[0].BlendEnable = FALSE;
    blend.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    D3D11_DEPTH_STENCIL_DESC depth{};
    depth.DepthEnable = FALSE;
    depth.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    depth.DepthFunc = D3D11_COMPARISON_ALWAYS;

    HRESULT hr = pDevice->CreateVertexShader(g_WetLookVs, sizeof(g_WetLookVs), nullptr, g_Vs.GetAddressOf());
    if (SUCCEEDED(hr)) hr = pDevice->CreatePixelShader(g_WetLookPs, sizeof(g_WetLookPs), nullptr, g_Ps.GetAddressOf());
    if (SUCCEEDED(hr)) hr = pDevice->CreateBuffer(&constants, nullptr, g_Constants.GetAddressOf());
    if (SUCCEEDED(hr)) hr = pDevice->CreateSamplerState(&sampler, g_Sampler.GetAddressOf());
    D3D11_SAMPLER_DESC wrap = sampler;
    wrap.AddressU = wrap.AddressV = wrap.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    if (SUCCEEDED(hr)) hr = pDevice->CreateSamplerState(&wrap, g_WrapSampler.GetAddressOf());
    if (SUCCEEDED(hr)) hr = pDevice->CreateRasterizerState(&rasterizer, g_Rasterizer.GetAddressOf());
    if (SUCCEEDED(hr)) hr = pDevice->CreateBlendState(&blend, g_Blend.GetAddressOf());
    if (SUCCEEDED(hr)) hr = pDevice->CreateDepthStencilState(&depth, g_Depth.GetAddressOf());
    if (FAILED(hr))
    {
      g_Failed = true;
      util::log::Error("Wet surfaces: its shaders or render states couldn't be made (HRESULT 0x%X), it's off", hr);
      return false;
    }
    if (FAILED(pDevice->CreatePixelShader(g_WetLookRainPs, sizeof(g_WetLookRainPs), nullptr, g_RainPs.GetAddressOf())))
    {
      g_RainPs.Reset();
      util::log::Warning("Wet surfaces: its pass's copy without the snow couldn't be made: the whole one is drawn in the rain too");
    }
    if (FAILED(pDevice->CreatePixelShader(g_WetLookSnowGroundPs, sizeof(g_WetLookSnowGroundPs), nullptr,
        g_SnowGroundPs.GetAddressOf())))
    {
      g_SnowGroundPs.Reset();
      util::log::Warning("Wet surfaces: its pass's copy without the rain couldn't be made: the whole one is drawn in the snow too");
    }
    // The snow's flakes: over what's there, by their own strength
    D3D11_BLEND_DESC snowBlend{};
    snowBlend.RenderTarget[0].BlendEnable = TRUE;
    snowBlend.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    snowBlend.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    snowBlend.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    snowBlend.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
    snowBlend.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
    snowBlend.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    snowBlend.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN |
      D3D11_COLOR_WRITE_ENABLE_BLUE;
    HRESULT snowHr = pDevice->CreateVertexShader(g_WetLookSnowVs, sizeof(g_WetLookSnowVs), nullptr, g_SnowVs.GetAddressOf());
    if (SUCCEEDED(snowHr)) snowHr = pDevice->CreatePixelShader(g_WetLookSnowPs, sizeof(g_WetLookSnowPs), nullptr, g_SnowPs.GetAddressOf());
    if (SUCCEEDED(snowHr)) snowHr = pDevice->CreateBlendState(&snowBlend, g_SnowBlend.GetAddressOf());
    if (FAILED(snowHr))
    {
      g_SnowVs.Reset();
      g_SnowPs.Reset();
      g_SnowBlend.Reset();
      util::log::Error("Snow: the flakes' shaders couldn't be made (HRESULT 0x%X): no falling snow", snowHr);
    }
    // The ice's pieces (see g_IceVs): a rectangle's two triangles (its
    // corners at the middle +- its half sides), each triangle of the water's
    // own as it is
    {
      std::vector<IceVertexData> ice;
      for (icetable::IcePlaneData const& p : icetable::IcePlanes)
      {
        float a = sqrtf(p.A[0] * p.A[0] + p.A[1] * p.A[1] + p.A[2] * p.A[2]);
        float b = sqrtf(p.B[0] * p.B[0] + p.B[1] * p.B[1] + p.B[2] * p.B[2]);
        const float corner[6][2] = { { -1.f, -1.f }, { 1.f, -1.f }, { 1.f, 1.f }, { -1.f, -1.f }, { 1.f, 1.f }, { -1.f, 1.f } };
        for (auto const& k : corner)
        {
          IceVertexData v{};
          for (int i = 0; i < 3; ++i)
            v.At[i] = p.Centre[i] + p.A[i] * k[0] + p.B[i] * k[1];
          v.At[3] = p.Tolerance;
          v.Local[0] = a * k[0];
          v.Local[1] = b * k[1];
          v.Local[2] = (float)p.Type;
          v.Shape[0] = a;
          v.Shape[1] = b;
          ice.push_back(v);
        }
      }
      for (icetable::IceTriangleData const& t : icetable::IceTriangles)
        for (float const* corner : { t.A, t.B, t.C })
        {
          IceVertexData v{};
          memcpy(v.At, corner, 3 * sizeof(float));
          v.At[3] = t.Tolerance;
          v.Local[2] = 3.f;
          v.Local[3] = t.Foot;
          ice.push_back(v);
        }
      D3D11_BUFFER_DESC buffer{};
      buffer.ByteWidth = (UINT)(ice.size() * sizeof(IceVertexData));
      buffer.Usage = D3D11_USAGE_IMMUTABLE;
      buffer.BindFlags = D3D11_BIND_VERTEX_BUFFER;
      D3D11_SUBRESOURCE_DATA data{ ice.data(), 0, 0 };
      const D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 } };
      HRESULT iceHr = pDevice->CreateVertexShader(g_WetLookIceVs, sizeof(g_WetLookIceVs), nullptr, g_IceVs.GetAddressOf());
      if (SUCCEEDED(iceHr)) iceHr = pDevice->CreatePixelShader(g_WetLookIcePs, sizeof(g_WetLookIcePs), nullptr, g_IcePs.GetAddressOf());
      if (SUCCEEDED(iceHr)) iceHr = pDevice->CreateInputLayout(layout, 3, g_WetLookIceVs, sizeof(g_WetLookIceVs), g_IceLayout.GetAddressOf());
      if (SUCCEEDED(iceHr)) iceHr = pDevice->CreateBuffer(&buffer, &data, g_IceVertices.GetAddressOf());
      if (FAILED(iceHr))
      {
        g_IceVs.Reset();
        g_IcePs.Reset();
        g_IceLayout.Reset();
        g_IceVertices.Reset();
        util::log::Error("Snow: the ice's shaders or pieces couldn't be made (HRESULT 0x%X): no ice over the water", iceHr);
      }
      else
      {
        g_IceVertexCount = (UINT)ice.size();
        util::log::Write("Snow: the ice over the water made: %zu rectangles and %zu triangles of the city's water (%u corners)",
          sizeof(icetable::IcePlanes) / sizeof(icetable::IcePlanes[0]),
          sizeof(icetable::IceTriangles) / sizeof(icetable::IceTriangles[0]), g_IceVertexCount);
      }
    }
    // The query for Faith's steps (see QuerySide): without it her steps use
    // what they can work out themselves
    D3D11_TEXTURE2D_DESC query{};
    query.Width = (UINT)QueryWidth;
    query.Height = (UINT)QueryHeight;
    query.MipLevels = 1;
    query.ArraySize = 1;
    query.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    query.SampleDesc.Count = 1;
    query.Usage = D3D11_USAGE_DEFAULT;
    query.BindFlags = D3D11_BIND_RENDER_TARGET;
    D3D11_TEXTURE2D_DESC copy = query;
    copy.Usage = D3D11_USAGE_STAGING;
    copy.BindFlags = 0;
    copy.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    hr = pDevice->CreatePixelShader(g_WetLookQueryPs, sizeof(g_WetLookQueryPs), nullptr, g_QueryPs.GetAddressOf());
    if (SUCCEEDED(hr)) hr = pDevice->CreateTexture2D(&query, nullptr, g_Query.GetAddressOf());
    if (SUCCEEDED(hr)) hr = pDevice->CreateRenderTargetView(g_Query.Get(), nullptr, g_QueryTarget.GetAddressOf());
    for (int i = 0; i < QueryCopies && SUCCEEDED(hr); ++i)
      hr = pDevice->CreateTexture2D(&copy, nullptr, g_QueryCopy[i].GetAddressOf());
    g_QueryMade = SUCCEEDED(hr);
    if (!g_QueryMade)
      util::log::Warning("Wet surfaces: the query for Faith's steps couldn't be made (HRESULT 0x%X): her steps don't see "
        "which puddles holed metal and stairs keep away", hr);
    return true;
  }

  // Its shaders and states made once, on a worker thread from the game's
  // first frame (Apply calls this each frame; the device's own calls are
  // safe from any thread unless the game made it single-threaded, when
  // they're made in that first frame instead). (2026-11-01, the user: "game
  // lags the first time when enabling rain/snow improve that": they were
  // made in the frame any of it first came on, and the graphics driver
  // turning the pass's shader, about 740 KB and 24,000 instructions, and the
  // six others into its own code held that frame up.)
  void StartMaking(ID3D11Device* pDevice)
  {
    int expected = 0;
    if (!pDevice || g_MakeState.load(std::memory_order_acquire) != 0 || !g_MakeState.compare_exchange_strong(expected, 1))
      return;
    ComPtr<ID3D11Device> device(pDevice);
    bool worker = !(pDevice->GetCreationFlags() & D3D11_CREATE_DEVICE_SINGLETHREADED);
    auto make = [device, worker]()
    {
      double start = wetlook::TimeNow();
      bool made = Make(device.Get());
      double took = wetlook::TimeNow() - start;
      g_MakeState.store(made ? 2 : -1, std::memory_order_release);
      util::log::Write("Wet surfaces: its shaders and states made in %.0f ms, %s%s", took,
        worker ? "on a worker thread from the game's first frame" : "in the game's first frame (its device is single-threaded)",
        made ? "" : " (failed: it's off)");
    };
    if (worker)
      std::thread(make).detach();
    else
      make();
  }

  // Each of its shaders drawn once as soon as they're made, in the game's
  // first frames (its logos or its menu): the graphics driver turns a shader
  // into its own code at its first draw, not when it's made (2026-11-01, CT.log
  // 02:42 - 02:45: made in 10 ms on the worker thread, then the first 10 s of
  // snow had one frame of 154 ms of the wet look's work on the render thread,
  // 0.1 - 0.2 ms otherwise). Into a target 1 pixel across in the frame's own
  // format (the query's into its own), with the same states as the pass, the
  // constants all 0: every loop's count is then 0 or its fixed one, clamped
  // (8 - 48 at most). The game's state saved and put back as for the pass.
  // Render thread, once.
  bool g_WarmedUp = false;

  void WarmUp(IDXGISwapChain* pSwapChain, ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
  {
    g_WarmedUp = true;
    double start = wetlook::TimeNow();
    DXGI_SWAP_CHAIN_DESC chain{};
    if (FAILED(pSwapChain->GetDesc(&chain)))
      return;
    D3D11_TEXTURE2D_DESC one{};
    one.Width = one.Height = 1;
    one.MipLevels = one.ArraySize = 1;
    one.Format = TypedFormat(chain.BufferDesc.Format);
    one.SampleDesc.Count = 1;
    one.Usage = D3D11_USAGE_DEFAULT;
    one.BindFlags = D3D11_BIND_RENDER_TARGET;
    ComPtr<ID3D11Texture2D> pixel;
    ComPtr<ID3D11RenderTargetView> pixelTarget;
    D3D11_MAPPED_SUBRESOURCE mapped;
    if (FAILED(pDevice->CreateTexture2D(&one, nullptr, pixel.GetAddressOf())) ||
        FAILED(pDevice->CreateRenderTargetView(pixel.Get(), nullptr, pixelTarget.GetAddressOf())) ||
        FAILED(pContext->Map(g_Constants.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
      util::log::Warning("Wet surfaces: its shaders couldn't be drawn ahead (the driver turns them into its own code the "
        "first time any of it comes on instead)");
      return;
    }
    memset(mapped.pData, 0, sizeof(Constants));
    pContext->Unmap(g_Constants.Get(), 0);
    g_Backup.Save(pContext);
    ID3D11ShaderResourceView* vsSaved[4] = {};
    ID3D11SamplerState* vsSampler = nullptr;
    pContext->VSGetShaderResources(0, 4, vsSaved);
    pContext->VSGetSamplers(0, 1, &vsSampler);
    ID3D11ShaderResourceView* none[9] = {};
    ID3D11Buffer* noBuffer = nullptr;
    UINT zero = 0;
    const float blendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
    D3D11_VIEWPORT viewport{ 0.f, 0.f, 1.f, 1.f, 0.f, 1.f };
    pContext->OMSetRenderTargets(1, pixelTarget.GetAddressOf(), nullptr);
    pContext->RSSetViewports(1, &viewport);
    pContext->RSSetState(g_Rasterizer.Get());
    pContext->OMSetDepthStencilState(g_Depth.Get(), 0);
    pContext->IASetInputLayout(nullptr);
    pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    pContext->IASetVertexBuffers(0, 1, &noBuffer, &zero, &zero);
    pContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_R16_UINT, 0);
    pContext->GSSetShader(nullptr, nullptr, 0);
    pContext->HSSetShader(nullptr, nullptr, 0);
    pContext->DSSetShader(nullptr, nullptr, 0);
    pContext->PSSetShaderResources(0, 9, none);
    pContext->VSSetShaderResources(0, 4, none);
    pContext->PSSetSamplers(0, 1, g_Sampler.GetAddressOf());
    pContext->VSSetSamplers(0, 1, g_Sampler.GetAddressOf());
    pContext->PSSetConstantBuffers(0, 1, g_Constants.GetAddressOf());
    pContext->VSSetConstantBuffers(0, 1, g_Constants.GetAddressOf());
    // The pass
    pContext->OMSetBlendState(g_Blend.Get(), blendFactor, 0xFFFFFFFF);
    pContext->VSSetShader(g_Vs.Get(), nullptr, 0);
    pContext->PSSetShader(g_Ps.Get(), nullptr, 0);
    pContext->Draw(3, 0);
    int drawn = 1;
    // and its copy for the rain (see g_RainPs)
    if (g_RainPs)
    {
      pContext->PSSetShader(g_RainPs.Get(), nullptr, 0);
      pContext->Draw(3, 0);
      ++drawn;
    }
    // and for the snow (see g_SnowGroundPs)
    if (g_SnowGroundPs)
    {
      pContext->PSSetShader(g_SnowGroundPs.Get(), nullptr, 0);
      pContext->Draw(3, 0);
      ++drawn;
    }
    // The ice (one triangle of its pieces)
    if (g_IceVs && g_IcePs && g_IceLayout && g_IceVertices && g_SnowBlend)
    {
      UINT stride = sizeof(IceVertexData), offset = 0;
      pContext->OMSetBlendState(g_SnowBlend.Get(), blendFactor, 0xFFFFFFFF);
      pContext->IASetInputLayout(g_IceLayout.Get());
      pContext->IASetVertexBuffers(0, 1, g_IceVertices.GetAddressOf(), &stride, &offset);
      pContext->VSSetShader(g_IceVs.Get(), nullptr, 0);
      pContext->PSSetShader(g_IcePs.Get(), nullptr, 0);
      pContext->Draw(3, 0);
      pContext->IASetInputLayout(nullptr);
      pContext->IASetVertexBuffers(0, 1, &noBuffer, &zero, &zero);
      ++drawn;
    }
    // The flakes (one)
    if (g_SnowVs && g_SnowPs && g_SnowBlend)
    {
      pContext->OMSetBlendState(g_SnowBlend.Get(), blendFactor, 0xFFFFFFFF);
      pContext->VSSetShader(g_SnowVs.Get(), nullptr, 0);
      pContext->PSSetShader(g_SnowPs.Get(), nullptr, 0);
      pContext->Draw(6, 0);
      ++drawn;
    }
    // The query, into its own target (its next pass draws all of it again)
    if (g_QueryMade && g_QueryPs && g_QueryTarget)
    {
      pContext->OMSetRenderTargets(1, g_QueryTarget.GetAddressOf(), nullptr);
      pContext->OMSetBlendState(g_Blend.Get(), blendFactor, 0xFFFFFFFF);
      pContext->VSSetShader(g_Vs.Get(), nullptr, 0);
      pContext->PSSetShader(g_QueryPs.Get(), nullptr, 0);
      pContext->Draw(3, 0);
      ++drawn;
    }
    pContext->PSSetShaderResources(0, 9, none);
    pContext->VSSetShaderResources(0, 4, vsSaved);
    pContext->VSSetSamplers(0, 1, &vsSampler);
    for (ID3D11ShaderResourceView* v : vsSaved)
      if (v)
        v->Release();
    if (vsSampler)
      vsSampler->Release();
    g_Backup.Restore(pContext);
    util::log::Write("Wet surfaces: its %d shader pairs drawn once (a pixel) in the game's first frames, in %.0f ms: the driver has "
      "made them its own before any of it comes on", drawn, wetlook::TimeNow() - start);
  }

  // Diagnostic (2026-11-01, the user: "Drone takedowns remove the snow
  // shader"): why the pass isn't drawn while some of it is on, in the log
  // when that starts and when it's drawn again; at most MostSkipLines a
  // minute. Render thread.
  const int MostSkipLines = 12;
  char const* g_SkipWhy = nullptr;
  double g_SkipSince = 0.0, g_SkipLinesFrom = 0.0;
  int g_SkipLines = 0;

  bool SkipLineAllowed(double now)
  {
    if (now - g_SkipLinesFrom > 60000.0)
    {
      g_SkipLinesFrom = now;
      g_SkipLines = 0;
    }
    return g_SkipLines++ < MostSkipLines;
  }

  void NoteSkip(char const* why)
  {
    if (!util::DiagnosticsOn() || why == g_SkipWhy)
      return;
    double now = wetlook::TimeNow();
    if (SkipLineAllowed(now))
      util::log::Write("Weather (diagnostic): the rain, snow and wet look not drawn: %s", why);
    g_SkipWhy = why;
    g_SkipSince = now;
  }

  void NoteDrawing()
  {
    if (!util::DiagnosticsOn() || !g_SkipWhy)
      return;
    double now = wetlook::TimeNow();
    if (SkipLineAllowed(now))
      util::log::Write("Weather (diagnostic): drawn again after %.2f s (%s)", (now - g_SkipSince) / 1000.0, g_SkipWhy);
    g_SkipWhy = nullptr;
  }

  // A slope's end found on the frame (see SlopeEndsAcross): the one kept
  // within SlopeEndSame of it seen again, else kept in a free place or the
  // one seen longest ago. g_Mutex held.
  void NoteSlopeEnd(float const* at, ULONGLONG now)
  {
    int free = -1, oldest = 0;
    ULONGLONG oldestSeen = 0;
    for (int k = 0; k < MostSlopeEnds; ++k)
    {
      SlopeEnd& e = g_SlopeEnds[k];
      if (!e.Seen || now - e.Seen >= SlopeEndKeepMs)
      {
        if (free < 0)
          free = k;
        continue;
      }
      float dx = e.At[0] - at[0], dz = e.At[2] - at[2];
      if (dx * dx + dz * dz < SlopeEndSame * SlopeEndSame && fabsf(e.At[1] - at[1]) < 0.5f)
      {
        e.Seen = now;
        return;
      }
      if (!oldestSeen || e.Seen < oldestSeen)
      {
        oldest = k;
        oldestSeen = e.Seen;
      }
    }
    SlopeEnd& e = g_SlopeEnds[free >= 0 ? free : oldest];
    memcpy(e.At, at, sizeof(e.At));
    e.Bottom = NAN;
    e.Landed = false;
    e.Seen = now;
  }

  // The sky's light seen (see HazeSkyRow): share, how much of the frame's
  // points were sky. g_Mutex held.
  void NoteHazeSky(float const* rgb, float share)
  {
    double ms = wetlook::TimeNow();
    float dt = g_HazeSkyMs > 0.0 ? (float)((ms - g_HazeSkyMs) / 1000.0) : 0.f;
    g_HazeSkyMs = ms;
    if (!g_HazeSkyKnown)
    {
      memcpy(g_HazeSky, rgb, sizeof(g_HazeSky));
      g_HazeSkyKnown = true;
      return;
    }
    float k = 1.f - expf(-(std::min)(dt, 0.5f) * (std::min)(share * 4.f, 1.f) / HazeSkySeconds);
    for (int i = 0; i < 3; ++i)
      g_HazeSky[i] += (rgb[i] - g_HazeSky[i]) * k;
  }

  // A read-back query (see QuerySide) into the squares kept, and the
  // slopes' ends found (see SlopeEndsAcross)
  void TakeQuery(ID3D11DeviceContext* pContext, int slot)
  {
    QueryCopyInfo& info = g_QueryCopyInfo[slot];
    if (!info.Pending)
      return;
    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = pContext->Map(g_QueryCopy[slot].Get(), 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &mapped);
    if (hr == DXGI_ERROR_WAS_STILL_DRAWING)
      return;
    info.Pending = false;
    if (FAILED(hr))
      return;
    ULONGLONG now = GetTickCount64();
    {
      std::lock_guard<std::mutex> lock(g_Mutex);
      for (int j = 0; j < SlopeEndsDown; ++j)
      {
        float const* row = (float const*)((BYTE const*)mapped.pData + (size_t)j * mapped.RowPitch);
        for (int i = 0; i < SlopeEndsAcross; ++i)
        {
          float const* e = row + (size_t)(QuerySide + i) * 4;
          if (!(e[3] > 0.5f) || !std::isfinite(e[0] + e[1] + e[2]))
            continue;
          float at[3] = { e[0] + info.CameraX, e[1] + info.CameraY, e[2] + info.CameraZ };
          NoteSlopeEnd(at, now);
        }
      }
      // the sky's light for the haze (see HazeSkyRow)
      {
        float const* sky = (float const*)((BYTE const*)mapped.pData + (size_t)HazeSkyRow * mapped.RowPitch);
        if (sky[3] > 0.f && std::isfinite(sky[0] + sky[1] + sky[2]))
          NoteHazeSky(sky, sky[3]);
      }
      for (int j = 0; j < QuerySide && info.Squares; ++j)
      {
        float const* row = (float const*)((BYTE const*)mapped.pData + (size_t)j * mapped.RowPitch);
        for (int i = 0; i < QuerySide; ++i)
        {
          float value = row[i * 4], y = row[i * 4 + 1];
          if (!(value >= 0.f) || !std::isfinite(y))
            continue;
          int x = info.CornerX + i, z = info.CornerZ + j;
          SurfaceSquare& sq = g_SurfaceSquares[(x & (SurfaceSide - 1)) * SurfaceSide + (z & (SurfaceSide - 1))];
          sq = { x, z, value, y + info.CameraY, now };
        }
      }
    }
    pContext->Unmap(g_QueryCopy[slot].Get(), 0);
  }

  // The open sky's squares into their texture (count x count, two floats
  // each); false: none to give (no texture then)
  bool UploadOpenSky(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, std::vector<float> const& cells, int count)
  {
    if (count <= 0 || cells.size() != (size_t)count * count * 2)
      return false;
    if (!g_OpenSky || g_OpenSkyCount != count)
    {
      g_OpenSkyView.Reset();
      g_OpenSky.Reset();
      D3D11_TEXTURE2D_DESC desc{};
      desc.Width = desc.Height = (UINT)count;
      desc.MipLevels = 1;
      desc.ArraySize = 1;
      desc.Format = DXGI_FORMAT_R32G32_FLOAT;
      desc.SampleDesc.Count = 1;
      desc.Usage = D3D11_USAGE_DYNAMIC;
      desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
      desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
      if (FAILED(pDevice->CreateTexture2D(&desc, nullptr, g_OpenSky.GetAddressOf())) ||
          FAILED(pDevice->CreateShaderResourceView(g_OpenSky.Get(), nullptr, g_OpenSkyView.GetAddressOf())))
      {
        g_OpenSkyView.Reset();
        g_OpenSky.Reset();
        return false;
      }
      g_OpenSkyCount = count;
    }
    D3D11_MAPPED_SUBRESOURCE mapped;
    if (FAILED(pContext->Map(g_OpenSky.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
      return false;
    for (int row = 0; row < count; ++row)
      memcpy((BYTE*)mapped.pData + row * mapped.RowPitch, cells.data() + row * count * 2, count * 2 * sizeof(float));
    pContext->Unmap(g_OpenSky.Get(), 0);
    return true;
  }

  // WetLook.hlsl's Hash, Noise and smoothstep, in floats as the GPU has them
  float Frac(float x)
  {
    return x - floorf(x);
  }

  float Hash(float px, float py)
  {
    float qx = Frac(px * 0.1031f), qy = Frac(py * 0.1031f), qz = Frac(px * 0.1031f);
    float d = qx * (qy + 33.33f) + qy * (qz + 33.33f) + qz * (qx + 33.33f);
    qx += d;
    qy += d;
    qz += d;
    return Frac((qx + qy) * qz);
  }

  float Noise(float px, float py)
  {
    float ix = floorf(px), iy = floorf(py);
    float fx = px - ix, fy = py - iy;
    float ux = fx * fx * (3.f - 2.f * fx), uy = fy * fy * (3.f - 2.f * fy);
    float a = Hash(ix, iy), b = Hash(ix + 1.f, iy), c = Hash(ix, iy + 1.f), d = Hash(ix + 1.f, iy + 1.f);
    float first = a + (b - a) * ux, second = c + (d - c) * ux;
    return first + (second - first) * uy;
  }

  // WetLook.hlsl's GNoise (gradient noise, 0 - 1 about)
  float GNoise(float px, float py)
  {
    float ix = floorf(px), iy = floorf(py);
    float fx = px - ix, fy = py - iy;
    float ux = fx * fx * fx * (fx * (fx * 6.f - 15.f) + 10.f), uy = fy * fy * fy * (fy * (fy * 6.f - 15.f) + 10.f);
    auto corner = [](float hx, float hy, float dx, float dy)
    {
      float a = Hash(hx, hy) * 6.2832f;
      return cosf(a) * dx + sinf(a) * dy;
    };
    float a = corner(ix, iy, fx, fy), b = corner(ix + 1.f, iy, fx - 1.f, fy);
    float c = corner(ix, iy + 1.f, fx, fy - 1.f), e = corner(ix + 1.f, iy + 1.f, fx - 1.f, fy - 1.f);
    float first = a + (b - a) * ux, second = c + (e - c) * ux;
    return 0.5f + 0.75f * (first + (second - first) * uy);
  }

  float SmoothStep(float from, float to, float x)
  {
    float t = (std::min)((std::max)((x - from) / (to - from), 0.f), 1.f);
    return t * t * (3.f - 2.f * t);
  }

  // The weather's squares into their texture (count x count, four floats
  // each); false: none to give
  bool UploadShelter(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, std::vector<float> const& cells, int count)
  {
    if (count <= 0 || cells.size() != (size_t)count * count * 4)
      return false;
    if (!g_Shelter || g_ShelterCount != count)
    {
      g_ShelterView.Reset();
      g_Shelter.Reset();
      D3D11_TEXTURE2D_DESC desc{};
      desc.Width = desc.Height = (UINT)count;
      desc.MipLevels = 1;
      desc.ArraySize = 1;
      desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
      desc.SampleDesc.Count = 1;
      desc.Usage = D3D11_USAGE_DYNAMIC;
      desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
      desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
      if (FAILED(pDevice->CreateTexture2D(&desc, nullptr, g_Shelter.GetAddressOf())) ||
          FAILED(pDevice->CreateShaderResourceView(g_Shelter.Get(), nullptr, g_ShelterView.GetAddressOf())))
      {
        g_ShelterView.Reset();
        g_Shelter.Reset();
        return false;
      }
      g_ShelterCount = count;
    }
    D3D11_MAPPED_SUBRESOURCE mapped;
    if (FAILED(pContext->Map(g_Shelter.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
      return false;
    for (int row = 0; row < count; ++row)
      memcpy((BYTE*)mapped.pData + row * mapped.RowPitch, cells.data() + row * count * 4, count * 4 * sizeof(float));
    pContext->Unmap(g_Shelter.Get(), 0);
    return true;
  }

  // The game's height fog (2026-10-25 23:5x, read from its shaders and code):
  // its sky / fog pass (systems/shaderprogramdb, the ps_5_0 using
  // g_heightFogCoefficients and g_minHeightFogTransmittance) lets through, of
  // a point d metres away and h metres over the camera,
  //   T = max(2^-|max(d - start, 0) x k(h)|, MinHeightFogTransmittance),
  //   k(h) = (C.z / (1 + 2^(h C.x + C.y)) - C.w) / h   (|h| under 0.01: 0.01),
  // and puts the fog's colour (the sky's light x HeightFogColorMult + Add)
  // over the rest. C is worked out by 0x143700FF0 (the fog set-up 0x1437065E0
  // calls it while [0x142569270] is 0, read live: 0) from the fog block
  // (state + 0x620): s = 4 (its argument, 0x141A01E6C), A' = (1 - follow) x
  // HeightFogAltitude + follow x min(altitude, the camera's height),
  // C.x = s / HeightFogDepth, C.y = (camera y - A') x C.x,
  // C.z = 5.6439 x 5.7708 / VisibilityRange / C.x (0x141BF6808, 0x141BF680C;
  // log2 50 so that at the fog's base it's 2 % at the visibility range),
  // C.w = C.z / (1 + 2^C.y). Its start is taken as 0 (the sky's
  // FogStartDistance is 0 in all eight time of day environments; which field
  // the set-up gives the shader for it isn't traced). The pass fogs what it
  // adds the same way, so the wet ground, the rain drawn far off and the
  // bolts fade into the fog as the game's own world does.
  bool FogCoefficients(weather::FogParams const& f, float cameraY, XMFLOAT4& c)
  {
    if (!f.On || !(f.Depth > 0.f) || !(f.SeenUpTo > 0.f))
      return false;
    const float s = 4.f;
    float follow = (std::max)(0.f, (std::min)(f.FollowCamera, 1.f));
    float base = (1.f - follow) * f.Altitude + follow * (std::min)(f.Altitude, cameraY);
    float cx = s / f.Depth;
    float cy = (cameraY - base) * cx;
    float cz = 5.64385986f * 5.77078009f / f.SeenUpTo / cx;
    float e = 1.f + exp2f(cy);
    float cw = std::isfinite(e) ? (std::max)(0.f, cz / e) : 0.f;
    c = XMFLOAT4(cx, cy, cz, cw);
    return std::isfinite(cz);
  }

  float FogAt(XMFLOAT4 const& c, float minT, float x, float y, float z)
  {
    float h = fabsf(y) < 0.01f ? 0.01f : y;
    float k = (c.z / (1.f + exp2f(h * c.x + c.y)) - c.w) / h;
    float d = sqrtf(x * x + y * y + z * z);
    return (std::max)(exp2f(-fabsf(d * k)), minT);
  }

  // The frame copied (the pass reads it while it draws over it)
  bool CopyScene(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, ID3D11Texture2D* pBack, D3D11_TEXTURE2D_DESC const& back)
  {
    // With all its levels (made each frame by the GPU): the rain's drops take
    // their light from blurred ones
    if (!g_SceneCopy || g_SceneCopyDesc.Width != back.Width || g_SceneCopyDesc.Height != back.Height ||
        g_SceneCopyDesc.Format != TypedFormat(back.Format))
    {
      g_SceneCopyView.Reset();
      g_SceneCopy.Reset();
      D3D11_TEXTURE2D_DESC desc{};
      desc.Width = back.Width;
      desc.Height = back.Height;
      desc.MipLevels = 0;
      desc.ArraySize = 1;
      desc.Format = TypedFormat(back.Format);
      desc.SampleDesc.Count = 1;
      desc.Usage = D3D11_USAGE_DEFAULT;
      desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
      desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
      D3D11_SHADER_RESOURCE_VIEW_DESC view{};
      view.Format = desc.Format;
      view.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
      view.Texture2D.MipLevels = (UINT)-1;
      HRESULT hr = pDevice->CreateTexture2D(&desc, nullptr, g_SceneCopy.GetAddressOf());
      if (SUCCEEDED(hr))
        hr = pDevice->CreateShaderResourceView(g_SceneCopy.Get(), &view, g_SceneCopyView.GetAddressOf());
      if (FAILED(hr))
      {
        g_SceneCopyView.Reset();
        g_SceneCopy.Reset();
        g_Failed = true;
        util::log::Error("Wet surfaces: no copy of the frame could be made (%u x %u, format %d, HRESULT 0x%X), it's off",
          back.Width, back.Height, (int)back.Format, hr);
        return false;
      }
      g_SceneCopy->GetDesc(&g_SceneCopyDesc);
      g_SceneCopyLevels = g_SceneCopyDesc.MipLevels ? g_SceneCopyDesc.MipLevels : 1;
    }
    pContext->CopySubresourceRegion(g_SceneCopy.Get(), 0, 0, 0, 0, pBack, 0, nullptr);
    pContext->GenerateMips(g_SceneCopyView.Get());
    return true;
  }
}

void wetlook::Set(Settings const& settings)
{
  std::lock_guard<std::mutex> lock(g_Mutex);
  g_Settings = settings;
}

bool wetlook::Drawn()
{
  return g_Drawn.load(std::memory_order_relaxed);
}

float wetlook::Reach()
{
  // While snow is chosen the snow on the ground is what the pass lays on the
  // ground (the wet look gives way to it), so it's kept off people and
  // vehicles as far as it reaches
  weather::SnowFall snow = weather::GetSnow();
  if (snow.Chosen)
    return snow.Cover > 0.f ? snow.CoverUpTo : 0.f;
  std::lock_guard<std::mutex> lock(g_Mutex);
  return g_Settings.On && g_Settings.Wetness > 0.f ? (std::max)(g_Settings.FadeDistance, 1.f) : 0.f;
}

static bool PuddleAt(float const* world, float up, bool* wetGround, float* groundWet = nullptr);

namespace
{
  // A fixed hash of a square, its side and a stream (0 - 1)
  float DripHash(int x, int z, int side, int k)
  {
    unsigned int h = (unsigned int)x * 73856093u ^ (unsigned int)z * 19349663u ^ (unsigned int)(side * 4 + k) * 83492791u;
    h ^= h >> 13;
    h *= 0x5BD1E995u;
    h ^= h >> 15;
    return (h & 0xFFFFFF) / 16777215.f;
  }

  // What of a list can reach a pixel or a point (WetLook.hlsl's g_DripBounds
  // and the others; 2026-11-02, the user: "New rain performance test can you
  // read it and find any improvements?"): the box round all of its own, grown
  // as each is put in the constants; outside it the pass doesn't go through
  // the list. Each one's own is the shader's, a pixel or 5 cm more each way
  // (the GPU's arithmetic a little off the CPU's). Until then every pixel of
  // the frame went through all the drips (48 while it rains on the roofs), the
  // drainpipes, the splashes and her footprints.
  struct FrameBox
  {
    float Least[2] = { 1e9f, 1e9f }, Most[2] = { -1e9f, -1e9f };
    // a box on the frame by two corners (pixels)
    void Add(float x0, float y0, float x1, float y1)
    {
      Least[0] = (std::min)(Least[0], (std::min)(x0, x1));
      Least[1] = (std::min)(Least[1], (std::min)(y0, y1));
      Most[0] = (std::max)(Most[0], (std::max)(x0, x1));
      Most[1] = (std::max)(Most[1], (std::max)(y0, y1));
    }
    XMFLOAT4 Get() const { return XMFLOAT4(Least[0], Least[1], Most[0], Most[1]); }
  };
  struct PlaceBox
  {
    float Least[3] = { 1e9f, 1e9f, 1e9f }, Most[3] = { -1e9f, -1e9f, -1e9f };
    // a point (relative to the camera) and how far its own reaches across
    // (x, z) and up and down
    void Add(float x, float y, float z, float across, float up)
    {
      float at[3] = { x, y, z }, reach[3] = { across, up, across };
      for (int i = 0; i < 3; ++i)
      {
        Least[i] = (std::min)(Least[i], at[i] - reach[i]);
        Most[i] = (std::max)(Most[i], at[i] + reach[i]);
      }
    }
    void Put(XMFLOAT4* two) const
    {
      two[0] = XMFLOAT4(Least[0], Least[1], Least[2], 0.f);
      two[1] = XMFLOAT4(Most[0], Most[1], Most[2], 0.f);
    }
  };
  // All of the frame and the world: each list's until it's made in a frame
  void PutEverywhere(XMFLOAT4& onFrame)
  {
    onFrame = XMFLOAT4(-1e9f, -1e9f, 1e9f, 1e9f);
  }
  void PutEverywhere(XMFLOAT4* two)
  {
    two[0] = XMFLOAT4(-1e9f, -1e9f, -1e9f, 0.f);
    two[1] = XMFLOAT4(1e9f, 1e9f, 1e9f, 0.f);
  }

  // The falling water's streams by the frame's tiles (WetLook.hlsl's FallingBits; 2026-11-03, the user: "find a way to
  // improve performance in fps even with rain/snow enabled"): the frame in FallingSide x FallingSide tiles, each stream
  // (its own box on the frame, as its FrameBox has it: the shader's, a pixel or more out) listed in every tile it comes
  // over, a pixel wider. A pixel goes through its tile's only, which gives what going through all of them gives: a
  // stream whose box doesn't reach a pixel adds nothing there (each list takes its most). Words: 0, 1 the drips' 0 - 31
  // and 32 - 63, 2 the drainpipes', 3 the splashes'. A list not made since Begin (List): all of it in every tile.
  const int FallingSide = 8;   // WetLook.hlsl's g_FallingGrid.z
  static_assert(MostDrips <= 64 && MostPipes <= 32 && MostSplashes <= 32, "WetParams: g_FallingTiles' words");
  struct FallingTiles
  {
    unsigned int Bits[FallingSide * FallingSide][4] = {};
    bool Listed[4] = {};
    float Width = 0.f, Height = 0.f;
    // The frame's start (its size in pixels)
    void Begin(float width, float height)
    {
      memset(Bits, 0, sizeof(Bits));
      for (bool& l : Listed)
        l = false;
      Width = width;
      Height = height;
    }
    // A list made this frame (its streams added as they're put in the constants)
    void List(int word)
    {
      Listed[word] = true;
    }
    // A stream: its bit in its word, its box on the frame by two corners; in every tile when its box isn't a number
    void Add(int word, int bit, float x0, float y0, float x1, float y1)
    {
      float tw = Width / FallingSide, th = Height / FallingSide;
      int a[2] = { 0, 0 }, z[2] = { FallingSide - 1, FallingSide - 1 };
      float least[2] = { (std::min)(x0, x1) - 1.f, (std::min)(y0, y1) - 1.f }, most[2] = { (std::max)(x0, x1) + 1.f,
        (std::max)(y0, y1) + 1.f }, size[2] = { tw, th };
      for (int i = 0; i < 2; ++i)
        if (fabsf(least[i]) < 1e7f && fabsf(most[i]) < 1e7f && size[i] > 0.f)
        {
          a[i] = (std::max)((std::min)((int)floorf(least[i] / size[i]), FallingSide - 1), 0);
          z[i] = (std::max)((std::min)((int)floorf(most[i] / size[i]), FallingSide - 1), 0);
        }
      for (int y = a[1]; y <= z[1]; ++y)
        for (int x = a[0]; x <= z[0]; ++x)
          Bits[y * FallingSide + x][word] |= 1u << bit;
    }
    // Into the constants (a list not made: all of it everywhere; no frame's size: no tiles)
    void Put(Constants* c) const
    {
      for (int i = 0; i < FallingSide * FallingSide; ++i)
      {
        unsigned int words[4];
        for (int w = 0; w < 4; ++w)
          words[w] = Listed[w] ? Bits[i][w] : 0xFFFFFFFFu;
        memcpy(&c->FallingTiles[i], words, sizeof(words));
      }
      c->FallingGrid = Width > 0.f && Height > 0.f ?
        XMFLOAT4(Width / FallingSide, Height / FallingSide, (float)FallingSide, 0.f) : XMFLOAT4(1.f, 1.f, 0.f, 0.f);
    }
  };
  FallingTiles g_Falling;   // render thread

  // What of a list can reach a point (WetLook.hlsl's g_SnowPrintBounds and the others; 2026-11-03, the user: "find a
  // way to improve performance in fps even with rain/snow enabled", then "make sure nothing breaks"): the box round all
  // of its own, from the list as written in the constants just before they go out, each one's box the shader's own test
  // that lets it through, ListMargin wider each way. Outside it none is let through, so the shader skips the loop and
  // gives what going through it gave. Empty: least over most (none reaches). Anything not a number in a list: the
  // whole world (its loop gone through as before).
  const float ListMargin = 0.05f;   // m (the GPU's own sums)
  struct ListBox
  {
    float Least[3] = { 1e30f, 1e30f, 1e30f }, Most[3] = { -1e30f, -1e30f, -1e30f };
    bool Everywhere = false;
    void Add(float x0, float y0, float z0, float x1, float y1, float z1)
    {
      float a[3] = { x0, y0, z0 }, b[3] = { x1, y1, z1 };
      for (int i = 0; i < 3; ++i)
      {
        if (!std::isfinite(a[i]) || !std::isfinite(b[i]))
          Everywhere = true;
        Least[i] = (std::min)(Least[i], (std::min)(a[i], b[i]) - ListMargin);
        Most[i] = (std::max)(Most[i], (std::max)(a[i], b[i]) + ListMargin);
      }
    }
    // a point and how far round it its own reaches (x and z across, y up and down)
    void Around(XMFLOAT4 const& at, float across, float up)
    {
      Add(at.x - across, at.y - up, at.z - across, at.x + across, at.y + up, at.z + across);
    }
    void Put(XMFLOAT4* two) const
    {
      if (Everywhere)
        return PutEverywhere(two);
      two[0] = XMFLOAT4(Least[0], Least[1], Least[2], 0.f);
      two[1] = XMFLOAT4(Most[0], Most[1], Most[2], 0.f);
    }
  };
  // (a list's count as written, no more than its room)
  int ListCount(float count, int room)
  {
    return count > 0.f ? (std::min)((int)count, room) : 0;
  }
  void MakeListBounds(Constants* c)
  {
    // Her prints and the boots (SnowPrintsAt: within 0.35 m up or down and 0.3 m across), her hand's stamps (within
    // 0.3 m of it)
    ListBox prints, boots, hands;
    for (int k = 0, n = ListCount(c->SnowPile.y, MostSnowPrints); k < n; ++k)
      prints.Around(c->SnowPrint[k], 0.3f, 0.35f);
    for (int k = 0, n = ListCount(c->SnowStampCount.y, MostOtherSnowPrints); k < n; ++k)
      boots.Around(c->OtherSnowPrint[k], 0.3f, 0.35f);
    for (int k = 0, n = ListCount(c->HandStampCount.x, (int)(sizeof(c->HandStampAt) / sizeof(c->HandStampAt[0]))); k < n; ++k)
      hands.Around(c->HandStampAt[k], 0.3f, 0.3f);
    prints.Put(c->SnowPrintBounds);
    boots.Put(c->BootPrintBounds);
    hands.Put(c->HandStampBounds);
    // Her slides' stretches (SnowTrailAt: within its half width and 0.6 m of the stretch across and along it, a
    // rectangle inside the box round its ends that much x 1.4143 out; up and down 0.6 m and half its rise round its
    // middle)
    ListBox trails;
    for (int k = 0, n = ListCount(c->SnowTrailCount.x, MostSnowTrails); k < n; ++k)
    {
      XMFLOAT4 const& a = c->SnowTrail[k];
      XMFLOAT4 const& e = c->SnowTrailEnd[k];
      float out = (fabsf(e.w) + 0.6f) * 1.4143f;
      float middle = (a.y + e.y) * 0.5f, up = 0.6f + 0.5f * fabsf(a.y - e.y);
      trails.Add((std::min)(a.x, e.x) - out, middle - up, (std::min)(a.z, e.z) - out, (std::max)(a.x, e.x) + out, middle + up,
        (std::max)(a.z, e.z) + out);
    }
    trails.Put(c->SnowTrailBounds);
    // Her rolls (SnowRollsAt: within 1 m across and 0.5 m up or down)
    ListBox rolls;
    for (int k = 0, n = ListCount(c->SnowRollCount.x, MostSnowRolls); k < n; ++k)
      rolls.Around(c->SnowRoll[k], 1.f, 0.5f);
    rolls.Put(c->SnowRollBounds);
    // The bodies' stamps (SnowStampsAt: its texels -4 - 132 across and -4 - 260 along, 2 cm each, from its corner; up
    // and down 0.5 m round the ground it rises along): the box round that parallelogram's corners
    ListBox stamps;
    for (int k = 0, n = ListCount(c->SnowStampCount.x, (int)(sizeof(c->SnowStamp) / sizeof(c->SnowStamp[0]))); k < n; ++k)
    {
      XMFLOAT4 const& corner = c->SnowStamp[k];
      XMFLOAT4 const& axis = c->SnowStampAxis[k];
      float l2 = axis.x * axis.x + axis.y * axis.y;
      if (!(l2 > 1e-8f))
      {
        stamps.Everywhere = true;
        continue;
      }
      float across[2] = { -axis.y, axis.x }, along[2] = { axis.x, axis.y };
      float u[2] = { -4.f * 0.02f, 132.f * 0.02f }, v[2] = { -4.f * 0.02f, 260.f * 0.02f };
      float rise[2] = { corner.y + axis.z * v[0], corner.y + axis.z * v[1] };
      for (int i = 0; i < 2; ++i)
        for (int j = 0; j < 2; ++j)
        {
          float x = corner.x + (across[0] * u[i] + along[0] * v[j]) / l2, z = corner.z + (across[1] * u[i] + along[1] * v[j]) / l2;
          stamps.Add(x, (std::min)(rise[0], rise[1]) - 0.5f, z, x, (std::max)(rise[0], rise[1]) + 0.5f, z);
        }
    }
    stamps.Put(c->SnowStampBounds);
    // The game's rain kept out (RainKeptOut: inside a box's height, within 10 cm outside its sides)
    ListBox rain;
    for (int k = 0, n = ListCount(c->RainBoxCount.x, MostRainBoxes); k < n; ++k)
    {
      XMFLOAT4 const& least = c->RainBoxMin[k];
      XMFLOAT4 const& most = c->RainBoxMax[k];
      rain.Add(least.x - 0.1f, least.y, least.z - 0.1f, most.x + 0.1f, most.y, most.z + 0.1f);
    }
    rain.Put(c->RainBoxBounds);
  }

  // Boxes by where they reach (WetLook.hlsl's KeptDryKind and InteriorAt, g_DryGrid and g_InteriorGrid; 2026-11-03, the
  // user: "find a way to improve performance in fps even with rain/snow enabled"): x, z round the camera in GridSide
  // cells a side over all of them, each box (its 4 float4s as written in the constants: its middle, then its axes over
  // its half sizes, w the half size) listed in every cell its sphere (its half sizes' length and extra[k], the shader's
  // reach) comes over, GridMargin wider: bits[cell * words + k / 32]. A pixel goes through the boxes of its cell only,
  // which gives what going through all of them gives: a box whose sphere doesn't reach a point is skipped there at once.
  // False (no grid, w 0: every box gone through) without boxes, or for not a number.
  const int GridSide = 32;          // WetLook.hlsl's DryGridSide, InteriorGridSide
  const float GridMargin = 0.25f;   // m (the GPU's own sums, a point on a cell's edge)
  bool BoxGrid(XMFLOAT4 const* boxes, int count, float const* extra, unsigned int* bits, int words, XMFLOAT4& grid)
  {
    memset(bits, 0, sizeof(unsigned int) * GridSide * GridSide * words);
    grid = XMFLOAT4(0.f, 0.f, 1.f, 0.f);
    if (count <= 0 || count > words * 32)
      return false;
    float reach[64];
    float least[2] = { 1e30f, 1e30f }, most[2] = { -1e30f, -1e30f };
    for (int k = 0; k < count; ++k)
    {
      XMFLOAT4 const& middle = boxes[k * 4];
      float a = boxes[k * 4 + 1].w, b = boxes[k * 4 + 2].w, d = boxes[k * 4 + 3].w;
      reach[k] = sqrtf(a * a + b * b + d * d) + extra[k] + GridMargin;
      least[0] = (std::min)(least[0], middle.x - reach[k]);
      most[0] = (std::max)(most[0], middle.x + reach[k]);
      least[1] = (std::min)(least[1], middle.z - reach[k]);
      most[1] = (std::max)(most[1], middle.z + reach[k]);
    }
    // (square cells, a hair over the boxes' extent so the farthest edge is in
    // the last cell; not a number, or too far to tell apart: no grid)
    float size = (std::max)((std::max)(most[0] - least[0], most[1] - least[1]) / GridSide, 0.5f) * 1.001f;
    if (!(size < 1e5f) || !(fabsf(least[0]) < 1e6f) || !(fabsf(least[1]) < 1e6f))
      return false;
    for (int k = 0; k < count; ++k)
    {
      XMFLOAT4 const& middle = boxes[k * 4];
      int x0 = (std::max)((int)floorf((middle.x - reach[k] - least[0]) / size), 0);
      int x1 = (std::min)((int)floorf((middle.x + reach[k] - least[0]) / size), GridSide - 1);
      int z0 = (std::max)((int)floorf((middle.z - reach[k] - least[1]) / size), 0);
      int z1 = (std::min)((int)floorf((middle.z + reach[k] - least[1]) / size), GridSide - 1);
      for (int z = z0; z <= z1; ++z)
        for (int x = x0; x <= x1; ++x)
          bits[(z * GridSide + x) * words + (k >> 5)] |= 1u << (k & 31);
    }
    grid = XMFLOAT4(least[0], least[1], size, (float)GridSide);
    return true;
  }

  // The kept-dry boxes' grid (KeptDryKind's reach: their feet under them too,
  // w's size less 1 when over 0.5); two cells a float4, their bits as they are
  // (the shader reads them with asuint)
  void MakeDryGrid(Constants* c, int count)
  {
    static_assert(dry::MostBoxes <= 64, "WetParams: g_DryCells has 2 words a cell");
    float extra[dry::MostBoxes] = {};
    for (int k = 0; k < count && k < dry::MostBoxes; ++k)
    {
      float w = c->DryBox[k * 4].w;
      extra[k] = (std::max)(fabsf(w) > 0.5f ? fabsf(w) - 1.f : 0.f, 0.f);
    }
    static unsigned int bits[GridSide * GridSide * 2];   // render thread
    static_assert(sizeof(bits) == sizeof(Constants::DryCells), "WetParams: g_DryCells' size");
    BoxGrid(c->DryBox, count, extra, bits, 2, c->DryGrid);
    memcpy(c->DryCells, bits, sizeof(bits));
  }

  // The interiors' boxes' grid (InteriorAt's reach: InteriorSphereExtra over
  // their half sizes' length); four cells a float4
  const float InteriorSphereExtra = 1.5f;   // WetLook.hlsl's InteriorFloorMargin + InteriorFloorFade
  void MakeInteriorGrid(Constants* c, int count)
  {
    static_assert(MostInteriorBoxes <= 32, "WetParams: g_InteriorCells has 1 word a cell");
    float extra[MostInteriorBoxes];
    for (float& e : extra)
      e = InteriorSphereExtra;
    static unsigned int bits[GridSide * GridSide];   // render thread
    static_assert(sizeof(bits) == sizeof(Constants::InteriorCells), "WetParams: g_InteriorCells' size");
    BoxGrid(c->InteriorBox, count, extra, bits, 1, c->InteriorGrid);
    memcpy(c->InteriorCells, bits, sizeof(bits));
  }

  // The drips off the roof edges round the camera into the constants (see
  // MostDrips): their streams on the frame, how they fall, where they land
  void MakeDrips(Constants* c, std::vector<float> const& cells, std::vector<float> const& edges, int count, float size,
    float cornerX, float cornerZ,
    float const* t, XMMATRIX const& viewProjection, float const* p, float width, float height, float strength, bool raining)
  {
    wetlook::ScopedTime timed(wetlook::TimeDrips);
    struct Candidate { float Top[3], Bottom[3], Distance, Rate, Phase; };
    static std::vector<Candidate> found;   // render thread
    found.clear();
    g_Falling.List(0);
    g_Falling.List(1);
    if (raining && count > 0 && cells.size() == (size_t)count * count * 4)
    {
      auto at = [&](int x, int z) { return &cells[((size_t)z * count + x) * 4]; };
      const int dx[4] = { 1, -1, 0, 0 }, dz[4] = { 0, 0, 1, -1 };
      for (int z = 0; z < count; ++z)
        for (int x = 0; x < count; ++x)
        {
          float const* roofed = at(x, z);
          if (roofed[0] < 0.5f || roofed[2] >= 1e8f)
            continue;
          for (int side = 0; side < 4; ++side)
          {
            int nx = x + dx[side], nz = z + dz[side];
            if (nx < 0 || nz < 0 || nx >= count || nz >= count)
              continue;
            float const* open = at(nx, nz);
            if (open[0] < 0.5f || open[2] < 1e8f || open[1] <= -1e8f)
              continue;
            // Where its roof ends on that side (Weather.cpp's g_DripEdges,
            // the edges' sweeps): from the square's middle out to where the
            // open sky begins, the drips just past it; not known, none
            // (2026-11-01, the user: "I can still see some rain dripping
            // indoor from edges": they fell from the squares' shared side,
            // up to a square in from a building's edge, under its roof)
            float out = edges.size() == cells.size() ? edges[((size_t)z * count + x) * 4 + side] : -1.f;
            if (!(out >= 0.f))
              continue;
            float lineX = cornerX + (x + 0.5f) * size + dx[side] * (out + 0.05f);
            float lineZ = cornerZ + (z + 0.5f) * size + dz[side] * (out + 0.05f);
            // (landing on the roofed square's floor when its roof ends in it)
            float top = roofed[2], bottom = out < 0.5f * size && roofed[1] > -1e8f ? roofed[1] : open[1];
            if (top - bottom < DripFallMin || top - bottom > DripFallMax)
              continue;
            // Not in one of the game's named interiors (2026-10-26, the user:
            // "there are still some water droplets indoors": the lair's roof
            // isn't found by the sweeps, so inside it squares looked open):
            // not the roof it drips from, the square it lands in or its fall
            // at the side's middle, as the wet look's InteriorAt keeps them dry
            {
              float sideX = dx[side] ? lineX : cornerX + (x + 0.5f) * size;
              float sideZ = dz[side] ? lineZ : cornerZ + (z + 0.5f) * size;
              float ox = sideX - t[12], oz = sideZ - t[14];
              if (ox * ox + oz * oz > (DripReach + size) * (DripReach + size))
                continue;
              float points[5][3] = {
                { cornerX + (x + 0.5f) * size, top - 1.f, cornerZ + (z + 0.5f) * size },
                { cornerX + (nx + 0.5f) * size, bottom + 1.f, cornerZ + (nz + 0.5f) * size },
                { sideX, top - 0.3f, sideZ }, { sideX, 0.5f * (top + bottom), sideZ }, { sideX, bottom + 0.3f, sideZ } };
              bool inside = false;
              for (int i = 0; i < 5 && !inside; ++i)
                inside = weather::InteriorAmountAt(points[i]) > 0.05f;
              if (inside)
                continue;
            }
            for (int k = 0; k < 3; ++k)
            {
              // Along the shared side (a quarter, half, three quarters of
              // it, moved a little by the hash), 5 cm out from under the roof
              float along = ((float)k + 0.5f + (DripHash(x, z, side, k) - 0.5f) * 0.6f) / 3.f;
              float px, pz;
              if (dx[side])
              {
                px = lineX;
                pz = cornerZ + (z + along) * size;
              }
              else
              {
                px = cornerX + (x + along) * size;
                pz = lineZ;
              }
              float ex = px - t[12], ez = pz - t[14];
              float distance = sqrtf(ex * ex + ez * ez);
              if (distance > DripReach)
                continue;
              Candidate cd{ { px, top, pz }, { px, bottom, pz }, distance,
                DripRateMin + (DripRateMax - DripRateMin) * DripHash(z, x, side, k + 3),
                DripHash(x + 7, z - 3, side, k + 6) };
              found.push_back(cd);
            }
          }
        }
    }
    // And off the ends of the slopes the rain runs down (see SlopeEndsAcross):
    // those found lately, where it's known where their drops land
    if (raining)
    {
      ULONGLONG now = GetTickCount64();
      std::lock_guard<std::mutex> lock(g_Mutex);
      for (SlopeEnd const& e : g_SlopeEnds)
      {
        if (!e.Seen || !e.Landed || !std::isfinite(e.Bottom) || now - e.Seen >= SlopeEndKeepMs)
          continue;
        float ex = e.At[0] - t[12], ez = e.At[2] - t[14];
        float distance = sqrtf(ex * ex + ez * ez);
        if (distance > DripReach)
          continue;
        int hx = (int)floorf(e.At[0] * 4.f), hz = (int)floorf(e.At[2] * 4.f);
        found.push_back({ { e.At[0], e.At[1], e.At[2] }, { e.At[0], e.Bottom, e.At[2] }, distance,
          DripRateMin + (DripRateMax - DripRateMin) * DripHash(hx, hz, 5, 0), DripHash(hz, hx, 6, 1) });
      }
    }
    std::sort(found.begin(), found.end(), [](Candidate const& a, Candidate const& b) { return a.Distance < b.Distance; });
    int drips = 0;
    FrameBox onFrame;
    PlaceBox landing;
    for (Candidate const& cd : found)
    {
      if (drips >= MostDrips)
        break;
      XMVECTOR ends[2];
      float w[2];
      for (int e = 0; e < 2; ++e)
      {
        float const* q = e ? cd.Bottom : cd.Top;
        ends[e] = XMVector4Transform(XMVectorSet(q[0] - t[12], q[1] - t[13], q[2] - t[14], 1.f), viewProjection);
        w[e] = XMVectorGetW(ends[e]);
      }
      // Both ends in front of the camera (a stream partly behind it is left out)
      if (w[0] < 0.3f || w[1] < 0.3f)
        continue;
      float screen[4];
      for (int e = 0; e < 2; ++e)
      {
        screen[e * 2] = (XMVectorGetX(ends[e]) / w[e] * 0.5f + 0.5f) * width;
        screen[e * 2 + 1] = (0.5f - XMVectorGetY(ends[e]) / w[e] * 0.5f) * height;
      }
      // On the frame at all (with some room)
      if ((std::max)(screen[0], screen[2]) < -8.f || (std::min)(screen[0], screen[2]) > width + 8.f ||
          (std::max)(screen[1], screen[3]) < -8.f || (std::min)(screen[1], screen[3]) > height + 8.f)
        continue;
      c->Drip[drips] = XMFLOAT4(screen[0], screen[1], screen[2], screen[3]);
      c->DripInfo[drips] = XMFLOAT4(w[0], w[1], cd.Rate * (std::max)(0.3f, (std::min)(strength, 1.5f)), cd.Phase);
      c->DripLand[drips] = XMFLOAT4(cd.Bottom[0] - t[12], cd.Bottom[1] - t[13], cd.Bottom[2] - t[14], cd.Top[1] - cd.Bottom[1]);
      // (DripsAt's box round its stream, 4 pixels out; DripRings' round
      // where it lands, 0.45 m across and 0.3 m up and down)
      onFrame.Add((std::min)(screen[0], screen[2]) - 5.f, (std::min)(screen[1], screen[3]) - 5.f,
        (std::max)(screen[0], screen[2]) + 5.f, (std::max)(screen[1], screen[3]) + 5.f);
      g_Falling.Add(drips >> 5, drips & 31, (std::min)(screen[0], screen[2]) - 5.f, (std::min)(screen[1], screen[3]) - 5.f,
        (std::max)(screen[0], screen[2]) + 5.f, (std::max)(screen[1], screen[3]) + 5.f);
      landing.Add(cd.Bottom[0] - t[12], cd.Bottom[1] - t[13], cd.Bottom[2] - t[14], 0.5f, 0.35f);
      ++drips;
    }
    c->DripCount = XMFLOAT4((float)drips, raining ? 1.f : 0.f, DripHalfWidth * p[0] * 0.5f * width, 0.f);
    c->DripBounds = onFrame.Get();
    landing.Put(c->DripLandBounds);
  }

  // The drainpipes' water round the camera into the constants (see
  // MostPipes): each stream on the frame, how far it falls, where it lands
  void MakePipes(Constants* c, float const* t, XMMATRIX const& viewProjection, float const* p, float width, float height,
    bool drawn)
  {
    wetlook::ScopedTime timed(wetlook::TimePipesDrawn);
    g_Falling.List(2);
    static float outlets[MostPipeOutlets][4];   // render thread
    float flow = 0.f;
    int count = drawn ? weather::GetPipes(outlets, MostPipeOutlets, flow) : 0;
    struct Candidate { int Index; float Distance; };
    Candidate found[MostPipeOutlets];
    int n = 0;
    for (int i = 0; i < count; ++i)
    {
      float ex = outlets[i][0] - t[12], ey = outlets[i][1] - t[13], ez = outlets[i][2] - t[14];
      float distance = sqrtf(ex * ex + ey * ey + ez * ez);
      if (distance <= PipeDrawReach)
        found[n++] = { i, distance };
    }
    std::sort(found, found + n, [](Candidate const& a, Candidate const& b) { return a.Distance < b.Distance; });
    int pipes = 0;
    FrameBox onFrame;
    PlaceBox landing;
    float perMetre = p[0] * 0.5f * width;   // pixels a metre at 1 m (PipeCount's w)
    for (int f = 0; f < n && pipes < MostPipes && flow > 0.f; ++f)
    {
      float const* o = outlets[found[f].Index];
      float ends[2][3] = { { o[0], o[1], o[2] }, { o[0], o[3], o[2] } };
      XMVECTOR clip[2];
      float w[2];
      for (int e = 0; e < 2; ++e)
      {
        clip[e] = XMVector4Transform(XMVectorSet(ends[e][0] - t[12], ends[e][1] - t[13], ends[e][2] - t[14], 1.f), viewProjection);
        w[e] = XMVectorGetW(clip[e]);
      }
      // Both ends in front of the camera (one partly behind it is left out)
      if (w[0] < 0.3f || w[1] < 0.3f)
        continue;
      float screen[4];
      for (int e = 0; e < 2; ++e)
      {
        screen[e * 2] = (XMVectorGetX(clip[e]) / w[e] * 0.5f + 0.5f) * width;
        screen[e * 2 + 1] = (0.5f - XMVectorGetY(clip[e]) / w[e] * 0.5f) * height;
      }
      // On the frame at all, with room for its splash
      float room = 0.4f * p[0] * 0.5f * width / w[1] + 8.f;
      if ((std::max)(screen[0], screen[2]) < -room || (std::min)(screen[0], screen[2]) > width + room ||
          (std::max)(screen[1], screen[3]) < -room || (std::min)(screen[1], screen[3]) > height + room)
        continue;
      unsigned int h = (unsigned int)(int)floorf(o[0] * 20.f) * 73856093u ^ (unsigned int)(int)floorf(o[2] * 20.f) * 19349663u;
      h ^= h >> 13;
      h *= 0x5BD1E995u;
      h ^= h >> 15;
      c->Pipe[pipes] = XMFLOAT4(screen[0], screen[1], screen[2], screen[3]);
      c->PipeInfo[pipes] = XMFLOAT4(w[0], w[1], o[1] - o[3], (h & 0xFFFFFF) / 16777215.f);
      c->PipeLand[pipes] = XMFLOAT4(o[0] - t[12], o[3] - t[13], o[2] - t[14], 0.f);
      // (PipesAt's box round its stream and its splash, as far out as it
      // pads it there; PipeRings' round where it lands, 0.6 m across and 0.3 m
      // up and down)
      float splashPx = 0.35f * perMetre / (std::max)(w[1], 0.3f);
      float widthPx = PipeHalfWidth * perMetre / (std::max)((std::min)(w[0], w[1]), 0.3f) * 1.5f + 2.f;
      float pad = (std::max)(splashPx, widthPx) + 2.f;
      onFrame.Add((std::min)(screen[0], screen[2]) - pad, (std::min)(screen[1], screen[3]) - pad,
        (std::max)(screen[0], screen[2]) + pad, (std::max)(screen[1], screen[3]) + pad);
      g_Falling.Add(2, pipes, (std::min)(screen[0], screen[2]) - pad, (std::min)(screen[1], screen[3]) - pad,
        (std::max)(screen[0], screen[2]) + pad, (std::max)(screen[1], screen[3]) + pad);
      landing.Add(o[0] - t[12], o[3] - t[13], o[2] - t[14], 0.65f, 0.35f);
      ++pipes;
    }
    float pixels = p[0] * 0.5f * width;
    c->PipeCount = XMFLOAT4((float)pipes, flow, PipeHalfWidth * pixels, pixels);
    c->PipeBounds = onFrame.Get();
    landing.Put(c->PipeLandBounds);
  }
}

namespace
{

  // Her right across a way (seen from above), as FootSide has it: the
  // camera's right turned from the way it looks (g_Handedness). g_Mutex held.
  void RightAcross(float const* way, float* right)
  {
    right[0] = g_Handedness >= 0.f ? -way[1] : way[1];
    right[1] = g_Handedness >= 0.f ? way[0] : -way[0];
  }

  // The way she faces over the ground (x, z, unit), for her prints' and
  // handprints' shape (2026-10-27, the user: "Check if snow/rain footprints
  // and handprints get placed where the shoe/hand faces and not the camera
  // if not fix it"): her transform's forward row (dry::FaithForward), the
  // way along it she runs (see FacingSure). They were laid the way her last
  // two steps went (the camera only told her right from her left), stale
  // while she stood, turned on the spot, stepped aside, after a jump or a
  // wall run; that's kept for while this isn't sure. False then. g_Mutex
  // held.
  bool HerForward(float* way)
  {
    if (!g_FacingRawKnown || abs(g_FacingEvidence) < FacingSure)
      return false;
    float s = g_FacingEvidence > 0 ? 1.f : -1.f;
    way[0] = g_FacingRaw[0] * s;
    way[1] = g_FacingRaw[1] * s;
    return true;
  }

  // A footprint's way and its foot's outer side (side: 1 her right foot, -1
  // her left, 0 not known: FootSide's): the way she faces (HerForward), else
  // as before, the way she walked (FootSide's). g_Mutex held.
  void PrintWay(int side, float* way, float* out)
  {
    if (HerForward(way))
    {
      float right[2];
      RightAcross(way, right);
      float s = side != 0 ? (side > 0 ? 1.f : -1.f) : g_FootSide;
      out[0] = right[0] * s;
      out[1] = right[1] * s;
      return;
    }
    way[0] = g_WalkDir[0];
    way[1] = g_WalkDir[1];
    out[0] = g_FootOutward[0];
    out[1] = g_FootOutward[1];
  }

  // Which foot a new step is, for its print's shape (ShoeSole), and the way
  // she walks. Over her last two steps (the step before last to this one, so
  // her feet's zigzag is left out) this step lands to the right or the left
  // of her last one, across the way she walks. Her right as the camera has
  // it: its right turned from the way it looks (seen from above), so the
  // world's axes can't swap it; taken while the camera is upright and not
  // looking straight down, kept after. Not told (turning, standing, her first
  // steps): the other foot of the last. g_Mutex held.
  void FootSide(float const* world, ULONGLONG now, int known)
  {
    DrawnWet const& w = g_DrawnWet;
    float fx = w.Forward[0], fz = w.Forward[2];
    if (fx * fx + fz * fz > 0.09f && w.Up[1] > 0.3f)
      g_Handedness = fx * w.Right[2] - fz * w.Right[0] >= 0.f ? 1.f : -1.f;
    // (the foot the game says it is, when it says: its bone)
    float side = known != 0 ? (float)known : -g_FootSide;
    if (g_StepBeforeTick && now - g_StepBeforeTick < 2500)
    {
      float wx = world[0] - g_StepBefore[0], wz = world[2] - g_StepBefore[2];
      float walk = sqrtf(wx * wx + wz * wz);
      if (walk > 0.3f && walk < 5.f)
      {
        g_WalkDir[0] = wx / walk;
        g_WalkDir[1] = wz / walk;
      }
    }
    // her right across the way she walks
    float rx = g_Handedness >= 0.f ? -g_WalkDir[1] : g_WalkDir[1];
    float rz = g_Handedness >= 0.f ? g_WalkDir[0] : -g_WalkDir[0];
    if (known == 0 && g_Handedness != 0.f && g_LastStepTick && now - g_LastStepTick < 2000)
    {
      float across = (world[0] - g_LastStep[0]) * rx + (world[2] - g_LastStep[2]) * rz;
      if (fabsf(across) > 0.06f)
        side = across > 0.f ? 1.f : -1.f;
    }
    g_FootSide = side;
    g_FootOutward[0] = rx * side;
    g_FootOutward[1] = rz * side;
  }

  // A landing in a puddle (see LandingFeetApart): its two rings. g_Mutex held.
  void NoteLanding(float const* world, float size, ULONGLONG now)
  {
    if (g_LandingTick && now - g_LandingTick < LandingSameMs && g_LandingRing[0] >= 0 && g_LandingRing[1] >= 0)
    {
      StepMark& first = g_StepRings[g_LandingRing[0]];
      StepMark& other = g_StepRings[g_LandingRing[1]];
      float dx = world[0] - first.At[0], dz = world[2] - first.At[2];
      float apart = sqrtf(dx * dx + dz * dz);
      if (apart < LandingOtherReach && fabsf(world[1] - first.At[1]) < 0.5f)
      {
        // The same landing: its size the biggest asked, the other foot's ring
        // where that foot came down
        first.Size = other.Size = (std::max)(first.Size, size);
        if (apart >= LandingSameContact && !g_LandingOtherSeen)
        {
          memcpy(other.At, world, sizeof(other.At));
          g_LandingOtherSeen = true;
        }
        return;
      }
    }
    float side[2] = { -g_LastDir[1], g_LastDir[0] };   // across the way she was going
    for (int k = 0; k < 2; ++k)
    {
      g_LandingRing[k] = g_StepRingNext;
      StepMark& r = g_StepRings[g_StepRingNext];
      g_StepRingNext = (g_StepRingNext + 1) % MostStepRings;
      memcpy(r.At, world, sizeof(r.At));
      if (k == 1)
      {
        r.At[0] += side[0] * LandingFeetApart;
        r.At[2] += side[1] * LandingFeetApart;
      }
      r.Tick = g_PlayMs.load();
      r.Size = size;
    }
    g_LandingTick = now;
    g_LandingOtherSeen = false;
  }

  // How much bigger a landing's or a roll's ring is for how hard she came
  // down (see FallWindowMs). g_Mutex held.
  float FallScale()
  {
    double ms = wetlook::TimeNow();
    float down = 0.f, along = 0.f;
    for (FallSample const& s : g_FallSamples)
      if (s.Ms > 0.0 && ms - s.Ms <= FallWindowMs)
      {
        down = (std::max)(down, s.Down);
        along = (std::max)(along, s.Along);
      }
    float h = (down * down + 0.25f * along * along) / 19.6f;
    float scale = (std::min)(1.f + 0.4f * sqrtf((std::max)(h - 0.3f, 0.f)), MostFallScale);
    ULONGLONG now = GetTickCount64();
    if (scale > 1.05f && (!g_FallLoggedTick || now - g_FallLoggedTick > 10000))
    {
      g_FallLoggedTick = now;
      util::log::Write("Wet surfaces: Faith comes down at %.1f m/s (%.1f m/s along): her ring %.2f times bigger", down, along,
        scale);
    }
    return scale;
  }

  // Faith come down on one of the Runners' landing pads (see MostPadRings):
  // a ring on its tarp. g_Mutex held; each frame a new place of her feet is
  // taken (ms: its time, TimeNow). Landing near one and not on it is said in
  // the log (its box from the game's data could be off)
  void NoteLandingPad(float const* feet, double ms)
  {
    if (!g_Drawn.load(std::memory_order_relaxed) || g_DrawnWet.Rain.z < 0.5f)
      return;
    FallSample const& last = g_FallSamples[(g_FallNext + MostFallSamples - 1) % MostFallSamples];
    if (last.Ms != ms || last.Down > LandingPadStill)
      return;
    float down = 0.f;
    for (FallSample const& s : g_FallSamples)
      if (s.Ms > 0.0 && ms - s.Ms <= FallWindowMs)
        down = (std::max)(down, s.Down);
    ULONGLONG play = g_PlayMs.load();
    if (down < LandingPadFall || (g_PadRingTick && play - g_PadRingTick < LandingPadSameMs))
      return;
    int on = -1;
    float nearest = 1e9f;
    for (int i = 0; i < LandingPadCount && on < 0; ++i)
    {
      LandingPadData const& p = LandingPads[i];
      float d[3] = { feet[0] - p.Middle[0], feet[1] - p.Middle[1], feet[2] - p.Middle[2] };
      nearest = (std::min)(nearest, sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]));
      bool inside = true;
      for (int a = 0; a < 3 && inside; ++a)
      {
        float const* h = p.Half[a];
        float length = sqrtf(h[0] * h[0] + h[1] * h[1] + h[2] * h[2]);
        if (length < 1e-4f)
          continue;
        float along = (d[0] * h[0] + d[1] * h[1] + d[2] * h[2]) / length;
        float pad = fabsf(h[1]) > 0.7f * length ? LandingPadOver : LandingPadSide;
        inside = fabsf(along) <= length + pad;
      }
      if (inside)
        on = i;
    }
    ULONGLONG now = GetTickCount64();
    if (on < 0)
    {
      if (nearest < 12.f && (!g_PadNearLoggedMs || now - g_PadNearLoggedMs > 30000))
      {
        g_PadNearLoggedMs = now;
        util::log::Write("Wet surfaces: Faith lands at %.2f %.2f %.2f (%.1f m/s down), %.1f m from a landing pad's middle, "
          "not on it as its box has it", feet[0], feet[1], feet[2], down, nearest);
      }
      return;
    }
    float size = (std::min)(HardLandingRing * FallScale(), MostRingSize);
    StepMark& r = g_PadRings[g_PadRingNext];
    g_PadRingNext = (g_PadRingNext + 1) % MostPadRings;
    memcpy(r.At, feet, sizeof(r.At));
    r.Tick = play;
    r.Size = size;
    g_PadRingTick = play;
    if (!g_PadLoggedMs || now - g_PadLoggedMs > 30000)
    {
      g_PadLoggedMs = now;
      util::log::Write("Wet surfaces: Faith lands on a landing pad (its tarp at %.2f %.2f %.2f) at %.2f %.2f %.2f, %.1f m/s "
        "down: its ring drawn on the tarp (%.2f times a step's)", LandingPads[on].Middle[0], LandingPads[on].Middle[1],
        LandingPads[on].Middle[2], feet[0], feet[1], feet[2], down, size);
    }
  }

  // A print of hers kept (see MostPrints): where, which (PrintKind), how
  // wet, the way she was going and her foot's outer side (x, z; none on a
  // wall). g_Mutex held.
  void AddPrint(float const* at, int kind, float wet, float const* dir, float const* out)
  {
    StepMark& p = g_Prints[g_PrintNext];
    g_PrintNext = (g_PrintNext + 1) % MostPrints;
    memcpy(p.At, at, sizeof(p.At));
    p.Dir[0] = dir[0];
    p.Dir[1] = dir[1];
    p.Out[0] = out ? out[0] : 0.f;
    p.Out[1] = out ? out[1] : 0.f;
    p.Wet = (std::min)(wet, 1.f);
    p.Kind = kind;
    p.Tick = g_PlayMs.load();
  }

  // A step of Faith's (see MostStepRings): puddle, in a puddle; groundWet,
  // how wet the ground is there (PuddleAt's); size, its ring's (1 a step,
  // see SoftLandingRing); surface, what it is (see wetlook::StepWater).
  // g_Mutex held.
  void NoteStep(float const* world, bool puddle, float groundWet, float size, int foot, int surface)
  {
    ULONGLONG now = GetTickCount64();
    // A landing in a puddle: a ring from each leg (see NoteLanding); it also
    // counts as her last step (a step's contact right after it isn't a new one)
    if (puddle && size > 1.f)
    {
      NoteLanding(world, size, now);
      memcpy(g_LastStep, world, sizeof(g_LastStep));
      g_LastStepTick = now;
      g_LastStepPuddle = true;
      g_LastStepRing = g_LandingRing[0];
      WetShoes(1.f);
      return;
    }
    float dx = world[0] - g_LastStep[0], dz = world[2] - g_LastStep[2];
    float moved = sqrtf(dx * dx + dz * dz);
    // The same step asked again (its sound's and its effect's): within
    // StepSameMs and StepSameMetres of that foot's last (2026-10-26, the user:
    // "Rain footsteps leave only 1 footprint behind when it should always be
    // 2": it was any foot's last, so where both feet come down together, a
    // landing or her stopping with her feet 20 cm apart, the second was taken
    // for the first asked again and left no print); the foot not known, any
    // foot's as before
    int f = foot > 0 ? 1 : foot < 0 ? 0 : -1;
    float const* last = f >= 0 ? g_FootLast[f] : g_LastStep;
    ULONGLONG lastTick = f >= 0 ? g_FootLastTick[f] : g_LastStepTick;
    bool lastPuddle = f >= 0 ? g_FootLastPuddle[f] : g_LastStepPuddle;
    int lastRing = f >= 0 ? g_FootLastRing[f] : g_LastStepRing;
    float sx = world[0] - last[0], sz = world[2] - last[2];
    bool same = lastTick && now - lastTick < StepSameMs && sqrtf(sx * sx + sz * sz) < StepSameMetres;
    if (same)
    {
      // only a puddle found the second time counts, and a bigger ring (a
      // landing) makes its ring bigger
      if (puddle && lastPuddle && lastRing >= 0)
        g_StepRings[lastRing].Size = (std::max)(g_StepRings[lastRing].Size, size);
      if (!puddle || lastPuddle)
        return;
    }
    else
    {
      // The way she's going: from her last step, when it's a step away
      if (g_LastStepTick && now - g_LastStepTick < 2000 && moved > 0.2f && moved < 3.f)
      {
        g_LastDir[0] = dx / moved;
        g_LastDir[1] = dz / moved;
      }
      // which foot (see FootSide), before this step becomes the last
      FootSide(world, now, foot);
      memcpy(g_StepBefore, g_LastStep, sizeof(g_StepBefore));
      g_StepBeforeTick = g_LastStepTick;
      memcpy(g_LastStep, world, sizeof(g_LastStep));
      g_LastStepTick = now;
      if (f >= 0)
      {
        memcpy(g_FootLast[f], world, sizeof(g_FootLast[f]));
        g_FootLastTick[f] = now;
      }
    }
    g_LastStepPuddle = puddle;
    if (f >= 0)
      g_FootLastPuddle[f] = puddle;
    if (puddle)
    {
      g_LastStepRing = g_StepRingNext;
      if (f >= 0)
        g_FootLastRing[f] = g_StepRingNext;
      StepMark& r = g_StepRings[g_StepRingNext];
      g_StepRingNext = (g_StepRingNext + 1) % MostStepRings;
      memcpy(r.At, world, sizeof(r.At));
      r.Tick = g_PlayMs.load();
      r.Size = size;
      WetShoes(1.f);
      return;
    }
    // Her shoes on the wet ground (see SoakUp), and a print where they're
    // wetter than the ground by ShoeDryAt: on the dry ground, and on the
    // ground less wet at a roof's edge (PsMain draws a print where it's
    // wetter than the wet look there, pixel by pixel); never out in the
    // rain. None on the wet ground (2026-11-03, the user: "remove the
    // footsteps/handprints from wet surfaces it should only be on dry": until
    // then pressed into it there, PrintPressed and PrintGravel; `surface` is
    // no longer used for prints)
    float shoeWet = ShoeWetNow();
    if (groundWet > 0.f)
      shoeWet = SoakUp(g_ShoeWet, g_ShoeWetTick, shoeWet, groundWet, ShoeWetFromGround);
    if (shoeWet - groundWet < ShoeDryAt)
      return;
    if (!g_PrintsLoggedTick || now - g_PrintsLoggedTick > 30000)
    {
      g_PrintsLoggedTick = now;
      util::log::Write("Wet surfaces: Faith's wet shoes leave prints on the drier ground at %.2f %.2f %.2f (as wet as %.2f, "
        "the ground %.2f)", world[0], world[1], world[2], shoeWet, groundWet);
    }
    float way[2], out[2];
    PrintWay(foot, way, out);
    AddPrint(world, PrintWetShoes, shoeWet, way, out);
  }
}

int wetlook::StepWater(float const* world, float up, int impact, int foot, int surface)
{
  bool wetGround = false;
  float groundWet = 0.f;
  bool wet = PuddleAt(world, up, &wetGround, &groundWet);
  // Only her steps on what faces up while the wet look is drawn
  if (fabsf(up) >= 0.7f && g_Drawn.load(std::memory_order_relaxed))
  {
    std::lock_guard<std::mutex> lock(g_Mutex);
    if (g_DrawnWet.Rain.z >= 0.5f)
    {
      float size = impact == 3 ? HardLandingRing : impact == 2 ? SoftLandingRing : 1.f;
      if (impact >= 2)
        size = (std::min)(size * FallScale(), MostRingSize);
      NoteStep(world, wet, groundWet, size, foot, surface);
    }
  }
  return wet ? 2 : wetGround ? 1 : 0;
}

void wetlook::NoteHerSplash(float const* world, int impact, unsigned int material, bool gameWater)
{
  std::lock_guard<std::mutex> lock(g_Mutex);
  StepMark& s = g_HerSplashes[g_HerSplashNext];
  g_HerSplashNext = (g_HerSplashNext + 1) % MostHerSplashes;
  memcpy(s.At, world, sizeof(s.At));
  s.Tick = g_PlayMs.load();
  s.Size = impact == 3 ? HerSplashHard : impact == 2 ? HerSplashSoft : 1.f;
  ++g_HerSplashCount;
  // On the game's own water, while the wet look is drawn: a ring there when
  // none of the tools' is for this step (see MostGameWaterRings)
  if (gameWater && g_Drawn.load(std::memory_order_relaxed) && g_DrawnWet.Rain.z >= 0.5f)
  {
    bool ringed = false;
    for (StepMark const& r : g_StepRings)
    {
      float dx = r.At[0] - world[0], dz = r.At[2] - world[2];
      if (r.Tick && (r.Tick > s.Tick ? r.Tick - s.Tick : s.Tick - r.Tick) <= HerSplashSameMs &&
        dx * dx + dz * dz < HerSplashSameMetres * HerSplashSameMetres && fabsf(r.At[1] - world[1]) < 0.5f)
        ringed = true;
    }
    if (!ringed)
    {
      StepMark& r = g_GameWaterRings[g_GameWaterRingNext];
      g_GameWaterRingNext = (g_GameWaterRingNext + 1) % MostGameWaterRings;
      memcpy(r.At, world, sizeof(r.At));
      r.Tick = s.Tick;
      float size = impact == 3 ? HardLandingRing : impact == 2 ? SoftLandingRing : 1.f;
      if (impact >= 2)
        size = (std::min)(size * FallScale(), MostRingSize);
      r.Size = size;
      ULONGLONG logMs = GetTickCount64();
      if (!g_GameWaterRingLoggedMs || logMs - g_GameWaterRingLoggedMs > 30000)
      {
        g_GameWaterRingLoggedMs = logMs;
        util::log::Write("Wet surfaces: Faith's %s on the game's own water at %.2f %.2f %.2f (material %u): its ring drawn "
          "there (%.2f times a step's)", impact == 1 ? "step" : "landing", world[0], world[1], world[2], material, size);
      }
    }
  }
  ULONGLONG ms = GetTickCount64();
  // (the ice under her step in the game's water, see IcePieceUnder)
  if (util::DiagnosticsOn() && material == 16 && g_IcedNow.load(std::memory_order_relaxed) &&
      (!g_IceStepLoggedMs || ms - g_IceStepLoggedMs > 30000))
  {
    g_IceStepLoggedMs = ms;
    float height = 0.f;
    bool triangle = false;
    if (IcePieceUnder(world, height, triangle))
      util::log::Write("Snow (diagnostic): Faith's %s in the game's water at %.2f %.2f %.2f: the ice there is %s at "
        "%.2f m, %.0f cm %s her", impact == 1 ? "step" : "landing", world[0], world[1], world[2],
        triangle ? "one of the water's own triangles" : "a pool's rectangle", height, fabsf(height - world[1]) * 100.f,
        height > world[1] ? "over" : "under");
    else
      util::log::Write("Snow (diagnostic): Faith's %s in the game's water at %.2f %.2f %.2f: no piece of the ice within "
        "2 m over or under it (the ice table has no water there: none drawn)", impact == 1 ? "step" : "landing",
        world[0], world[1], world[2]);
  }
  if (!g_HerSplashLoggedMs || ms - g_HerSplashLoggedMs > 30000)
  {
    g_HerSplashLoggedMs = ms;
    util::log::Write("Wet surfaces: the game plays Faith's own water splash for her %s at %.2f %.2f %.2f (the surface's "
      "material %u: %s): the wet look lets it through; %d so far", impact == 1 ? "step" : "landing", world[0], world[1],
      world[2], material, material == 47 ? "the wet concrete, as the game's puddles" : "the water", g_HerSplashCount);
  }
}

namespace
{
  float WetShareAt(float const* world);
}

void wetlook::NoteWallStep(float const* world, float const* normal, int foot)
{
  // Her wet shoes' print on a wall she runs along (2026-10-26, the user:
  // "Footsteps should appear on walls as well if faith wallrides"; "i meant
  // footprints"): the game's FootPlantWallLeft / Right (see Hooks.cpp's
  // FootImpactIds), on what faces within 45 degrees of level, the print in
  // its plane, the toe the way she runs (along the wall, level, the way the
  // camera looks), as wet as her shoes are, only where they're wetter than
  // the wall by ShoeDryAt (none on a wall out in the rain: 2026-11-03, the
  // user, "it should only be on dry"; PsMain draws it where it's wetter,
  // pixel by pixel), while the wet look is drawn. A step's sound and its
  // effect both ask: the same foot's within StepSameMs and StepSameMetres is
  // the same step.
  if (!g_Drawn.load(std::memory_order_relaxed))
    return;
  float n[3] = { normal[0], normal[1], normal[2] };
  float length = sqrtf(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
  if (!(length > 0.5f && length < 2.f))
    return;
  for (float& v : n)
    v /= length;
  if (fabsf(n[1]) > 0.7f)
    return;
  // (turned towards her)
  float feet[3];
  if (dry::FaithFeet(feet))
  {
    float to[3] = { feet[0] - world[0], feet[1] + 1.f - world[1], feet[2] - world[2] };
    if (to[0] * n[0] + to[1] * n[1] + to[2] * n[2] < 0.f)
      for (float& v : n)
        v = -v;
  }
  float along[2] = { n[2], -n[0] };
  float al = sqrtf(along[0] * along[0] + along[1] * along[1]);
  if (al < 1e-3f)
    return;
  along[0] /= al;
  along[1] /= al;
  // How wet the wall is there, as for her hands (see WetShareAt; before
  // g_Mutex, which it takes)
  float off[3] = { world[0] + n[0] * 0.15f, world[1] + n[1] * 0.15f, world[2] + n[2] * 0.15f };
  float wallWet = WetShareAt(off);
  std::lock_guard<std::mutex> lock(g_Mutex);
  if (g_DrawnWet.Rain.z < 0.5f)
    return;
  // (the way she faces along it, HerForward; the camera's while that isn't
  // known)
  float faces[2];
  if (!HerForward(faces))
  {
    faces[0] = g_DrawnWet.Forward[0];
    faces[1] = g_DrawnWet.Forward[2];
  }
  if (along[0] * faces[0] + along[1] * faces[1] < 0.f)
  {
    along[0] = -along[0];
    along[1] = -along[1];
  }
  ULONGLONG now = GetTickCount64();
  int f = foot > 0 ? 1 : 0;
  float dx = world[0] - g_WallStep[f][0], dy = world[1] - g_WallStep[f][1], dz = world[2] - g_WallStep[f][2];
  if (g_WallStepTick[f] && now - g_WallStepTick[f] < StepSameMs && dx * dx + dy * dy + dz * dz < StepSameMetres * StepSameMetres)
    return;
  memcpy(g_WallStep[f], world, sizeof(g_WallStep[f]));
  g_WallStepTick[f] = now;
  float shoeWet = ShoeWetNow();
  if (shoeWet - wallWet < ShoeDryAt)
    return;
  float at[3] = { world[0] + n[0] * 0.005f, world[1] + n[1] * 0.005f, world[2] + n[2] * 0.005f };
  if (!g_WallPrintLoggedTick || now - g_WallPrintLoggedTick > 30000)
  {
    g_WallPrintLoggedTick = now;
    util::log::Write("Wet surfaces: Faith's wet shoes leave a print on the drier wall she runs along at %.2f %.2f %.2f (as "
      "wet as %.2f, the wall %.2f, facing %.2f %.2f %.2f)", at[0], at[1], at[2], shoeWet, wallWet, n[0], n[1], n[2]);
  }
  AddPrint(at, PrintWall, shoeWet, along, nullptr);
}

void wetlook::OtherStep(float const* world, float up, int impact, bool fromMoves)
{
  // Other people's steps and the cars' wheels (see MostOtherRings): a ring
  // where one is in a puddle drawn there (PuddleAt), while the wet look is
  // drawn, on what faces up, within OtherRingReach of the camera.
  // - The game's own impacts (fromMoves false): KSec's and the game's AI
  //   people's (Characters/AINpc's PamMovementImpactSoundEntityData: the
  //   same FootPlantLeft / Right / LandHard as Faith's, on their skeleton's
  //   feet, 146 and 155; Hooks.cpp). Kept a while (MostOwnSteps) so the
  //   moves below leave them be.
  // - From the moves of those the tracker follows (DryShapes.cpp, fromMoves
  //   true): the crowd and the citizens (Characters/AINpc/AICitizen has no
  //   impacts), a step each stride; the road's cars, their back wheels.
  if (!g_Drawn.load(std::memory_order_relaxed) || fabsf(up) < 0.7f)
    return;
  float camera[3];
  {
    std::lock_guard<std::mutex> lock(g_Mutex);
    if (g_DrawnWet.Rain.z < 0.5f)
      return;
    memcpy(camera, g_DrawnWet.Camera, sizeof(camera));
    ULONGLONG now = GetTickCount64();
    if (fromMoves)
    {
      for (OwnStep const& o : g_OwnSteps)
      {
        float dx = world[0] - o.At[0], dy = world[1] - o.At[1], dz = world[2] - o.At[2];
        if (o.Tick && now - o.Tick < OtherOwnMs && dx * dx + dy * dy + dz * dz < OtherOwnReach * OtherOwnReach)
          return;
      }
    }
    else
    {
      OwnStep& o = g_OwnSteps[g_OwnStepNext];
      g_OwnStepNext = (g_OwnStepNext + 1) % MostOwnSteps;
      memcpy(o.At, world, sizeof(o.At));
      o.Tick = now;
    }
  }
  float dx = world[0] - camera[0], dy = world[1] - camera[1], dz = world[2] - camera[2];
  float distance = sqrtf(dx * dx + dy * dy + dz * dz);
  if (distance > OtherRingReach)
    return;
  bool wetGround = false;
  if (!PuddleAt(world, up, &wetGround))
    return;
  std::lock_guard<std::mutex> lock(g_Mutex);
  ULONGLONG play = g_PlayMs.load();
  float size = impact == 3 ? HardLandingRing : impact == 2 ? SoftLandingRing : impact == 5 ? WheelRing :
    impact == 6 ? BodyRing : 1.f;
  // (a place: one whose ring is over, else the farthest from the camera, if
  // farther than this)
  int slot = -1;
  float farthest = distance;
  for (int i = 0; i < MostOtherRings; ++i)
  {
    StepMark const& r = g_OtherRings[i];
    float life = StepRingSeconds * (0.7f + 0.3f * (std::max)(r.Size, 1.f));
    if (!r.Tick || (float)(play - r.Tick) / 1000.f >= life)
    {
      slot = i;
      break;
    }
    float rx = r.At[0] - camera[0], ry = r.At[1] - camera[1], rz = r.At[2] - camera[2];
    float rd = sqrtf(rx * rx + ry * ry + rz * rz);
    if (rd > farthest)
    {
      farthest = rd;
      slot = i;
    }
  }
  if (slot < 0)
    return;
  StepMark& r = g_OtherRings[slot];
  memcpy(r.At, world, sizeof(r.At));
  r.Tick = play;
  r.Size = size;
  ULONGLONG now = GetTickCount64();
  ULONGLONG& logged = g_OtherLoggedTick[fromMoves ? 1 : 0];
  if (!logged || now - logged > 30000)
  {
    logged = now;
    util::log::Write("Wet surfaces: %s in a puddle at %.2f %.2f %.2f, %.0f m from the camera: its ring drawn",
      fromMoves ? (impact == 5 ? "a car's wheel (from its moves)" : "someone's step (from their moves)") :
      impact == 6 ? "a downed body's knock (its ragdoll's sound)" : "someone's own step (the game's impact)", world[0], world[1],
      world[2], distance);
  }
}

void wetlook::NoteRagdollKnock(float const* at)
{
  // (a KrugerSec lying there: his body's waves start again, see
  // MostRippleBodies; snowbody's own lock, before g_Mutex)
  snowbody::NoteKnock(at);
  // (the same knock asked again, or the body's next bounce: the first only)
  ULONGLONG now = GetTickCount64();
  std::lock_guard<std::mutex> lock(g_Mutex);
  float dx = at[0] - g_LastBody[0], dy = at[1] - g_LastBody[1], dz = at[2] - g_LastBody[2];
  if (g_LastBodyTick && now - g_LastBodyTick < BodySameMs && dx * dx + dy * dy + dz * dz < BodySameMetres * BodySameMetres)
    return;
  memcpy(g_LastBody, at, sizeof(g_LastBody));
  g_LastBodyTick = now;
  BodyKnock& k = g_BodyKnocks[g_BodyKnockNext];
  g_BodyKnockNext = (g_BodyKnockNext + 1) % MostBodyKnocks;
  memcpy(k.At, at, sizeof(k.At));
  k.Tick = now;
  k.Waiting = true;
}

void wetlook::LandBodies()
{
  // The knocks waiting (see BodyRing): the first surface facing up under
  // each (from 0.5 m over it to 3 m under), its ring there
  if (!collision::Available())
    return;
  float at[MostBodyKnocks][3];
  int n = 0;
  {
    std::lock_guard<std::mutex> lock(g_Mutex);
    ULONGLONG now = GetTickCount64();
    for (BodyKnock& k : g_BodyKnocks)
      if (k.Waiting)
      {
        k.Waiting = false;
        if (now - k.Tick < 2000)
          memcpy(at[n++], k.At, sizeof(at[0]));
      }
  }
  // (not where a KrugerSec lies: his body's own waves, see MostRippleBodies)
  snowbody::LyingBody lying[MostRippleBodies];
  int found = n ? snowbody::LyingBodies(lying, MostRippleBodies) : 0;
  for (int i = 0; i < n; ++i)
  {
    bool byBody = false;
    for (int b = 0; b < found && !byBody; ++b)
      for (int k = 0; k < lying[b].Segments && k < snowbody::LyingSegments && !byBody; ++k)
      {
        float mx = 0.5f * (lying[b].A[k][0] + lying[b].B[k][0]) - at[i][0];
        float my = 0.5f * (lying[b].A[k][1] + lying[b].B[k][1]) - at[i][1];
        float mz = 0.5f * (lying[b].A[k][2] + lying[b].B[k][2]) - at[i][2];
        byBody = mx * mx + my * my + mz * mz < RippleBodyKnockReach * RippleBodyKnockReach;
      }
    if (byBody)
      continue;
    float from[3] = { at[i][0], at[i][1] + 0.5f, at[i][2] }, to[3] = { at[i][0], at[i][1] - 3.f, at[i][2] };
    float fraction = 1.f, normal[3] = {};
    if (!collision::Sweep(from, to, 0.f, &fraction, normal) || normal[1] < 0.7f)
      continue;
    float ground[3] = { at[i][0], from[1] + (to[1] - from[1]) * fraction, at[i][2] };
    if (std::isfinite(ground[1]))
      OtherStep(ground, normal[1], 6, false);
  }
}

void wetlook::NoteSlide(float const* world)
{
  // In the snow: her slide's trail (the snow session's; never with puddles:
  // the wet look is off while snow is chosen)
  NoteSnowSlide(world);
  // In a puddle drawn there or on the wet ground (as a step's, on what faces
  // up): its rings show only in a puddle, its trail on both
  bool wetGround = false;
  bool puddle = PuddleAt(world, 1.f, &wetGround);
  if (!puddle && !wetGround)
    return;
  std::lock_guard<std::mutex> lock(g_Mutex);
  ULONGLONG now = g_PlayMs.load();
  float dx = world[0] - g_LastSlide[0], dz = world[2] - g_LastSlide[2];
  // At most one every SlideRingEveryMs; standing still, one every 0.4 s
  if (g_LastSlideTick && (now - g_LastSlideTick < SlideRingEveryMs ||
      (dx * dx + dz * dz < 0.15f * 0.15f && now - g_LastSlideTick < 400)))
    return;
  if (!g_LastSlideTick || now - g_LastSlideTick > 2000)
    util::log::Write("Wet surfaces: Faith slides %s at %.2f %.2f %.2f: its trail drawn", puddle ? "through a puddle" : "on the wet ground",
      world[0], world[1], world[2]);
  memcpy(g_LastSlide, world, sizeof(g_LastSlide));
  g_LastSlideTick = now;
  StepMark& r = g_SlideRings[g_SlideRingNext];
  g_SlideRingNext = (g_SlideRingNext + 1) % MostSlideRings;
  memcpy(r.At, world, sizeof(r.At));
  r.Tick = now;
  r.Size = 1.f;
  if (puddle)
    WetShoes(1.f);
}

void wetlook::NoteRainRoll(float const* world, float up)
{
  // In a puddle drawn there (see RollRingSize); on the wet ground her shoes
  // get wet (as much as it's wet, see SoakUp)
  bool wetGround = false;
  float groundWet = 0.f;
  bool puddle = PuddleAt(world, up, &wetGround, &groundWet);
  if (!puddle && groundWet <= 0.f)
    return;
  std::lock_guard<std::mutex> lock(g_Mutex);
  if (!puddle)
  {
    SoakUp(g_ShoeWet, g_ShoeWetTick, ShoeWetNow(), groundWet, ShoeWetFromGround);
    return;
  }
  ULONGLONG now = g_PlayMs.load();
  if (g_LastRollTick && now - g_LastRollTick < RollRingEveryMs)
    return;
  if (!g_LastRollTick || now - g_LastRollTick > 2000)
    util::log::Write("Wet surfaces: Faith rolls through a puddle at %.2f %.2f %.2f: its rings drawn", world[0], world[1], world[2]);
  g_LastRollTick = now;
  // (bigger the harder she came down, see FallScale)
  float rollSize = (std::min)(RollRingSize * FallScale(), MostRingSize);
  for (int k = 0; k < 2; ++k)
  {
    StepMark& r = g_StepRings[g_StepRingNext];
    g_StepRingNext = (g_StepRingNext + 1) % MostStepRings;
    memcpy(r.At, world, sizeof(r.At));
    if (k == 1)
    {
      r.At[0] += g_WalkDir[0] * RollRingAhead;
      r.At[2] += g_WalkDir[1] * RollRingAhead;
    }
    r.Tick = now;
    r.Size = k == 0 ? rollSize : rollSize * 0.8f;
  }
  WetShoes(1.f);
}

namespace
{
  // How wet a point is, whatever way its surface faces (as PuddleAt's
  // groundWet: while the wet look is on and drawn, the share of its
  // wetness drawn there, out from under the roofs)
  float WetShareAt(float const* world)
  {
    DrawnWet w;
    {
      std::lock_guard<std::mutex> lock(g_Mutex);
      w = g_DrawnWet;
    }
    if (!g_Drawn.load(std::memory_order_relaxed) || w.Rain.z < 0.5f || w.Wet.x <= 0.f)
      return 0.f;
    return 1.f - SmoothStep(0.25f, 0.75f, weather::ShelterAt(world));
  }

  void Normalize3(float* v)
  {
    float l = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (l > 1e-6f)
      for (int i = 0; i < 3; ++i)
        v[i] /= l;
  }
}

void wetlook::NoteRainHand(float const* world, float const* normal, int hand, int stamp)
{
  // The surface's normal, towards Faith (it may come either way round)
  float n[3] = { normal[0], normal[1], normal[2] };
  float length = sqrtf(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
  if (!(length > 0.5f && length < 2.f))
    return;
  for (float& v : n)
    v /= length;
  float feet[3];
  if (dry::FaithFeet(feet))
  {
    float to[3] = { feet[0] - world[0], feet[1] + 1.f - world[1], feet[2] - world[2] };
    if (to[0] * n[0] + to[1] * n[1] + to[2] * n[2] < 0.f)
      for (float& v : n)
        v = -v;
  }
  else if (fabsf(n[1]) >= 0.7f && n[1] < 0.f)
    for (float& v : n)
      v = -v;
  bool ground = n[1] >= 0.7f;
  // How wet it is there: on the ground as for her steps (a puddle; how wet
  // the ground is), on a wall the same share of the wet look's wetness
  bool puddle = false;
  float hereWet = 0.f;
  if (ground)
  {
    bool wetGround = false;
    puddle = PuddleAt(world, n[1], &wetGround, &hereWet);
  }
  else
  {
    float off[3] = { world[0] + n[0] * 0.15f, world[1] + n[1] * 0.15f, world[2] + n[2] * 0.15f };
    hereWet = WetShareAt(off);
  }
  std::lock_guard<std::mutex> lock(g_Mutex);
  ULONGLONG now = GetTickCount64();
  // The same touch asked again (its sound's and its effect's)
  int h = hand > 0 ? 1 : 0;
  float dx = world[0] - g_RainHand[h][0], dy = world[1] - g_RainHand[h][1], dz = world[2] - g_RainHand[h][2];
  if (g_RainHandTick[h] && now - g_RainHandTick[h] < HandSameMs && dx * dx + dy * dy + dz * dz < 0.15f * 0.15f)
    return;
  memcpy(g_RainHand[h], world, sizeof(g_RainHand[h]));
  g_RainHandTick[h] = now;
  // How wet her hands are now: as wet as they were made, drying evenly over
  // HandWetSeconds since (the play clock: waiting while the game is paused)
  ULONGLONG play = g_PlayMs.load();
  float handWet = g_HandWetTick ? g_HandWet * (std::max)(0.f, 1.f - (float)(play - g_HandWetTick) / 1000.f / HandWetSeconds) : 0.f;
  if (puddle)
  {
    StepMark& r = g_StepRings[g_StepRingNext];
    g_StepRingNext = (g_StepRingNext + 1) % MostStepRings;
    memcpy(r.At, world, sizeof(r.At));
    r.Tick = g_PlayMs.load();
    r.Size = HandRingSize;
    g_HandWet = 1.f;
    g_HandWetTick = play;
    return;
  }
  // Her hands on what's wet (see SoakUp), and a print where they're wetter
  // than it by HandDryAt, on the ground and on a wall or a pole alike (it
  // darkens on top of the wet look, where a footprint is drawn instead of
  // it). The print keeps how wet her hand was: PsMain draws it as much as
  // it's wetter than the surface, pixel by pixel. (Until 2026-11-03 on a
  // wall or a pole however wet that was, the user, 2026-10-26: "if faith's
  // hands are wet they should also leave handprints on the walls/poles she
  // touches"; then: "remove the footsteps/handprints from wet surfaces it
  // should only be on dry".)
  if (hereWet > 0.f)
    handWet = SoakUp(g_HandWet, g_HandWetTick, handWet, hereWet, HandWetFromGround);
  float over = handWet - hereWet;
  if (over < HandDryAt)
    return;
  // Its print: fingers the way she goes on the ground, up on a wall; the
  // thumb on her inner side (her right as FootSide has it: the camera's
  // right turned from its forward), both in the surface's plane
  // (on the ground the way she faces, HerForward; the way she walked while
  // that isn't known)
  float way[2] = { g_WalkDir[0], g_WalkDir[1] };
  HerForward(way);
  float finger[3] = { ground ? way[0] : 0.f, ground ? 0.f : 1.f, ground ? way[1] : 0.f };
  float along = finger[0] * n[0] + finger[1] * n[1] + finger[2] * n[2];
  for (int i = 0; i < 3; ++i)
    finger[i] -= n[i] * along;
  Normalize3(finger);
  float facing[2] = { ground ? way[0] : -n[0], ground ? way[1] : -n[2] };
  float fl = sqrtf(facing[0] * facing[0] + facing[1] * facing[1]);
  if (fl < 1e-3f)
  {
    facing[0] = g_WalkDir[0];
    facing[1] = g_WalkDir[1];
    fl = 1.f;
  }
  facing[0] /= fl;
  facing[1] /= fl;
  float right[3] = { g_Handedness >= 0.f ? -facing[1] : facing[1], 0.f, g_Handedness >= 0.f ? facing[0] : -facing[0] };
  float side = hand > 0 ? -1.f : 1.f;   // the right hand's thumb is on her left
  float thumb[3] = { right[0] * side, 0.f, right[2] * side };
  float tn = thumb[0] * n[0] + thumb[1] * n[1] + thumb[2] * n[2];
  float tf = thumb[0] * finger[0] + thumb[1] * finger[1] + thumb[2] * finger[2];
  for (int i = 0; i < 3; ++i)
    thumb[i] -= n[i] * tn + finger[i] * tf;
  Normalize3(thumb);
  if (thumb[0] * thumb[0] + thumb[1] * thumb[1] + thumb[2] * thumb[2] < 0.5f ||
      finger[0] * finger[0] + finger[1] * finger[1] + finger[2] * finger[2] < 0.5f)
    return;
  HandMark& p = g_HandPrints[g_HandPrintNext];
  g_HandPrintNext = (g_HandPrintNext + 1) % MostHandPrints;
  for (int i = 0; i < 3; ++i)
  {
    p.At[i] = world[i] + n[i] * 0.005f;
    p.Finger[i] = finger[i];
    p.Thumb[i] = thumb[i];
  }
  p.Wet = handWet;
  p.Tick = g_PlayMs.load();
  p.Stamp = stamp;
  if (!g_HandPrintsLoggedTick || now - g_HandPrintsLoggedTick > 30000)
  {
    g_HandPrintsLoggedTick = now;
    util::log::Write("Wet surfaces: Faith's wet %s hand leaves a print on the drier %s at %.2f %.2f %.2f (as wet as %.2f, "
      "it %.2f)", hand > 0 ? "right" : "left", ground ? "ground" : "wall", world[0], world[1], world[2], handWet, hereWet);
  }
}

void wetlook::LandSlopeEnds()
{
  // The slopes' ends not swept for yet (see SlopeEndsAcross), a few a frame:
  // straight down from where the water lets go, at least SlopeDripFallMin
  // to the first thing under it, at most DripFallMax
  if (!collision::Available())
    return;
  int index[SlopeEndLandsPerFrame];
  float at[SlopeEndLandsPerFrame][3];
  int n = 0;
  {
    std::lock_guard<std::mutex> lock(g_Mutex);
    for (int k = 0; k < MostSlopeEnds && n < SlopeEndLandsPerFrame; ++k)
      if (g_SlopeEnds[k].Seen && !g_SlopeEnds[k].Landed)
      {
        index[n] = k;
        memcpy(at[n], g_SlopeEnds[k].At, sizeof(at[n]));
        ++n;
      }
  }
  if (!n)
    return;
  float bottom[SlopeEndLandsPerFrame];
  for (int k = 0; k < n; ++k)
  {
    bottom[k] = NAN;
    float to[3] = { at[k][0], at[k][1] - DripFallMax, at[k][2] };
    float fraction = 1.f, normal[3] = {};
    if (collision::Sweep(at[k], to, 0.f, &fraction, normal))
    {
      float landing = at[k][1] - DripFallMax * fraction;
      if (std::isfinite(landing) && at[k][1] - landing >= SlopeDripFallMin)
        bottom[k] = landing;
    }
  }
  std::lock_guard<std::mutex> lock(g_Mutex);
  for (int k = 0; k < n; ++k)
  {
    // (still that one: not taken for another found since)
    SlopeEnd& e = g_SlopeEnds[index[k]];
    if (!e.Landed && !memcmp(e.At, at[k], sizeof(e.At)))
    {
      e.Bottom = bottom[k];
      e.Landed = true;
    }
  }
  static ULONGLONG s_logged = 0;
  ULONGLONG now = GetTickCount64();
  if (!s_logged || now - s_logged > 30000)
  {
    int drips = 0, kept = 0;
    for (SlopeEnd const& e : g_SlopeEnds)
      if (e.Seen && now - e.Seen < SlopeEndKeepMs)
      {
        ++kept;
        drips += e.Landed && std::isfinite(e.Bottom) ? 1 : 0;
      }
    s_logged = now;
    util::log::Write("Wet surfaces: %d ends of the slopes the rain runs down found lately, %d of them dripping (the first at "
      "%.2f %.2f %.2f, %s)", kept, drips, at[0][0], at[0][1], at[0][2],
      std::isfinite(bottom[0]) ? "landing under it" : "nothing far enough under it");
  }
}

double wetlook::TimeNow()
{
  static LARGE_INTEGER frequency = {};
  if (!frequency.QuadPart)
    QueryPerformanceFrequency(&frequency);
  LARGE_INTEGER now;
  QueryPerformanceCounter(&now);
  return (double)now.QuadPart * 1000.0 / (double)frequency.QuadPart;
}

void wetlook::NoteTime(int part, double ms)
{
  if (part < 0 || part >= TimeParts || !(ms >= 0.0))
    return;
  std::lock_guard<std::mutex> lock(g_TimeMutex);
  TimeSum& t = g_Times[part];
  t.Total += ms;
  t.Most = (std::max)(t.Most, ms);
  ++t.Count;
}

namespace
{
  // The GPU's timing (see GpuTimeSlots): a slot's results if they're in
  void ReadGpuTimes(ID3D11DeviceContext* pContext, GpuTimes& g)
  {
    if (!g.Pending)
      return;
    D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint{};
    UINT64 begin = 0, pass = 0, end = 0;
    if (pContext->GetData(g.Disjoint.Get(), &disjoint, sizeof(disjoint), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK ||
        pContext->GetData(g.Begin.Get(), &begin, sizeof(begin), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK ||
        pContext->GetData(g.Pass.Get(), &pass, sizeof(pass), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK ||
        pContext->GetData(g.End.Get(), &end, sizeof(end), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK)
      return;
    g.Pending = false;
    if (disjoint.Disjoint || !disjoint.Frequency || pass < begin || end < pass)
      return;
    double perTick = 1000.0 / (double)disjoint.Frequency;
    wetlook::NoteTime(wetlook::TimeGpuPass, (double)(pass - begin) * perTick);
    if (g.Snow)
      wetlook::NoteTime(wetlook::TimeGpuSnow, (double)(end - pass) * perTick);
  }

  // This frame's slot, its timing begun (null: none this frame)
  GpuTimes* BeginGpuTimes(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
  {
    if (g_GpuTimesState == 0)
    {
      g_GpuTimesState = 1;
      D3D11_QUERY_DESC disjoint{ D3D11_QUERY_TIMESTAMP_DISJOINT, 0 }, stamp{ D3D11_QUERY_TIMESTAMP, 0 };
      for (GpuTimes& g : g_GpuTimes)
        if (FAILED(pDevice->CreateQuery(&disjoint, g.Disjoint.GetAddressOf())) ||
            FAILED(pDevice->CreateQuery(&stamp, g.Begin.GetAddressOf())) ||
            FAILED(pDevice->CreateQuery(&stamp, g.Pass.GetAddressOf())) ||
            FAILED(pDevice->CreateQuery(&stamp, g.End.GetAddressOf())))
          g_GpuTimesState = -1;
    }
    if (g_GpuTimesState < 0)
      return nullptr;
    GpuTimes& g = g_GpuTimes[g_GpuTimeNext];
    ReadGpuTimes(pContext, g);
    if (g.Pending)
      return nullptr;   // not back yet: this frame isn't timed
    g_GpuTimeNext = (g_GpuTimeNext + 1) % GpuTimeSlots;
    pContext->Begin(g.Disjoint.Get());
    pContext->End(g.Begin.Get());
    return &g;
  }

  // Every 10 s while it rains or snows: the times in the log, each part's
  // mean a time it ran and its most
  void LogTimes()
  {
    ULONGLONG now = GetTickCount64();
    ++g_TimeFrames;
    if (!g_TimeLogTick)
      g_TimeLogTick = now;
    if (now - g_TimeLogTick < 10000)
      return;
    TimeSum t[wetlook::TimeParts];
    int frames = g_TimeFrames;
    {
      std::lock_guard<std::mutex> lock(g_TimeMutex);
      memcpy(t, g_Times, sizeof(t));
      memset(g_Times, 0, sizeof(g_Times));
    }
    g_TimeLogTick = now;
    g_TimeFrames = 0;
    if (!weather::IsRaining(nullptr) && weather::GetSnow().Amount <= 0.f)
      return;
    auto mean = [&](int p) { return t[p].Count ? t[p].Total / t[p].Count : 0.0; };
    util::log::Write("Weather (performance, diagnostic): %d frames in 10 s. Game thread: weather %.3f ms a frame (most %.2f; "
      "roofs %.3f, most %.2f; pipes %.3f, most %.2f), people kept dry %.3f (most %.2f; their scan %.2f ms each, %d in "
      "10 s, most %.2f). Render thread: the wet look's work "
      "%.3f ms (most %.2f; drips %.3f, pipes %.3f). GPU: the wet look's pass %.3f ms (most %.2f, %d timed), the ice and the "
      "snow's flakes %.3f ms (most %.2f)", frames, mean(wetlook::TimeWeather), t[wetlook::TimeWeather].Most,
      mean(wetlook::TimeRoofs), t[wetlook::TimeRoofs].Most, mean(wetlook::TimePipes), t[wetlook::TimePipes].Most,
      mean(wetlook::TimePeople), t[wetlook::TimePeople].Most, mean(wetlook::TimePeopleScan), t[wetlook::TimePeopleScan].Count,
      t[wetlook::TimePeopleScan].Most, mean(wetlook::TimeSetUp), t[wetlook::TimeSetUp].Most,
      mean(wetlook::TimeDrips), mean(wetlook::TimePipesDrawn), mean(wetlook::TimeGpuPass), t[wetlook::TimeGpuPass].Most,
      t[wetlook::TimeGpuPass].Count, mean(wetlook::TimeGpuSnow), t[wetlook::TimeGpuSnow].Most);
  }
}

bool wetlook::InPuddle(float const* world, float up)
{
  return StepWater(world, up) == 2;
}

bool wetlook::PuddleUnder(float const* world)
{
  bool wetGround = false;
  return PuddleAt(world, 1.f, &wetGround);
}

static bool PuddleAt(float const* world, float up, bool* wetGround, float* groundWet)
{
  *wetGround = false;
  if (groundWet)
    *groundWet = 0.f;
  DrawnWet w;
  {
    std::lock_guard<std::mutex> lock(g_Mutex);
    w = g_DrawnWet;
  }
  // PsMain's puddle at the point with the last frame drawn's values, each of
  // its soft steps taken at its middle: the wet look on and drawn, puddles
  // there (they go with the rain: none while it doesn't)
  if (!g_Drawn.load(std::memory_order_relaxed) || w.Rain.z < 0.5f || w.Wet.x <= 0.f)
    return false;
  // On what faces up (its ground, smoothstep(0.55, 0.85, normal.y), at least
  // half). Its other test, flat 4 pixels round (no grates, no steps), needs
  // the frame's depth and isn't made here.
  if (fabsf(up) < 0.7f)
    return false;
  // How wet the ground is there (groundWet, for her shoes and hands, see
  // SoakUp): the share of the wet look's wetness drawn there, its outside
  // (1 - smoothstep(0.25, 0.75, shelter), from under the roofs). Not the
  // camera's distance or the fog, as before 2026-10-26: they draw it fainter
  // from where the camera is, they don't dry it. The wet ground (her steps'
  // sounds, her slides' trails): at least half.
  float knownOpen = 0.f;
  float outside = 1.f - SmoothStep(0.25f, 0.75f, weather::ShelterAt(world, &knownOpen));
  if (groundWet)
    *groundWet = outside;
  if (outside < 0.5f)
    return false;
  *wetGround = true;
  // A puddle: drawn there (within Wet up to of the camera, out of the fog),
  // only while it rains
  float dx = world[0] - w.Camera[0], dy = world[1] - w.Camera[1], dz = world[2] - w.Camera[2];
  float fog = w.FogOn ? FogAt(w.Fog, w.FogMin, dx, dy, dz) : 1.f;
  float wet = w.Wet.x * (std::max)(1.f - sqrtf(dx * dx + dy * dy + dz * dz) / w.Rain.y, 0.f) * outside * fog;
  if (wet <= 0.002f || w.Wet.y <= 0.001f)
    return false;
  // Inside a puddle's edge (its smoothstep over 0.14 of the noise, at least
  // half), where the squares know it's open to the sky (its
  // saturate((KnownOpenAt - 0.5) x 2), at least half), and drawn there
  float n = Noise(world[0] / w.Wet.z, world[2] / w.Wet.z) * 0.7f +
    Noise(world[0] / (w.Wet.z * 0.37f) + 17.f, world[2] / (w.Wet.z * 0.37f) + 17.f) * 0.3f;
  float edge = SmoothStep(1.f - w.Wet.y, 1.f - w.Wet.y + 0.14f, n);
  float open = (std::min)((std::max)((knownOpen - 0.5f) * 2.f, 0.f), 1.f);
  if (!(edge >= 0.5f && open >= 0.5f && edge * open * wet > 0.001f))
    return false;
  // And where the frame showed that square's surface lately (see
  // QuerySide), only if a puddle can lie there: not on holed metal, stairs
  // or narrow tops (at least half, as the rest). Not seen lately: as above.
  int x = (int)floorf(world[0] / QueryCell), z = (int)floorf(world[2] / QueryCell);
  std::lock_guard<std::mutex> lock(g_Mutex);
  SurfaceSquare const& sq = g_SurfaceSquares[(x & (SurfaceSide - 1)) * SurfaceSide + (z & (SurfaceSide - 1))];
  if (sq.Tick && sq.X == x && sq.Z == z && GetTickCount64() - sq.Tick <= SurfaceKeepMs && fabsf(sq.Y - world[1]) < 0.5f)
    return sq.Value >= 0.5f;
  return true;
}

// PsMain's snow on the ground (its snow block and SnowGround) at a point,
// each of its soft steps taken at its middle, with the last frame drawn's
// values: snow chosen, some settled, and the pass drawn on the last frame.
// Not with g_Mutex held.
static bool SnowLiesAt(float const* world, float up, weather::SnowFall const& snow)
{
  if (!g_Drawn.load(std::memory_order_relaxed) || !snow.Chosen || snow.Cover <= 0.001f)
    return false;
  DrawnWet w;
  {
    std::lock_guard<std::mutex> lock(g_Mutex);
    w = g_DrawnWet;
  }
  // On what faces up (its flat, smoothstep(0.35, 0.8, normal.y), at least
  // half). Its flatness 4 pixels round needs the frame's depth: not made here.
  float flat = SmoothStep(0.35f, 0.8f, fabsf(up));
  if (flat < 0.5f)
    return false;
  // Out from under the roofs and where the squares know it's open (its open,
  // min(outside, saturate((KnownOpenAt - 0.25) x 2)), at least half). Past
  // the squares it counts as open there; here it doesn't (a room seen
  // through a window can't be told from the open: no snow steps indoors).
  float knownOpen = 0.f;
  float outside = 1.f - SmoothStep(0.25f, 0.75f, weather::ShelterAt(world, &knownOpen));
  float open = (std::min)(outside, (std::min)((std::max)((knownOpen - 0.25f) * 2.f, 0.f), 1.f));
  if (open < 0.5f)
    return false;
  // Within its reach of the camera, out of the fog
  float dx = world[0] - w.Camera[0], dy = world[1] - w.Camera[1], dz = world[2] - w.Camera[2];
  float fog = w.FogOn ? FogAt(w.Fog, w.FogMin, dx, dy, dz) : 1.f;
  float settled = flat * open * (std::max)(1.f - sqrtf(dx * dx + dy * dy + dz * dz) / (std::max)(snow.CoverUpTo, 1.f), 0.f) * fog;
  if (settled <= 0.002f)
    return false;
  // In a patch (SnowGround's, at least half; without its parallax, a few
  // centimetres at most; none where 2 - 8 cm lie, as deep as SnowDepthAt has
  // it there)
  float c = snow.Cover;
  // (WetLook.hlsl's SnowPatchNoise)
  float qx = world[0] * 0.8f - world[2] * 0.6f, qz = world[0] * 0.6f + world[2] * 0.8f;
  float n = GNoise(world[0] / 2.2f, world[2] / 2.2f) * 0.7f + GNoise(qx / 0.9f + 23.f, qz / 0.9f + 23.f) * 0.3f;
  float patch = (std::max)(SmoothStep(1.f - c - 0.16f, 1.f - c + 0.16f, n * 0.85f + flat * 0.15f), SmoothStep(0.85f, 1.f, c));
  float depthHere = snow.Pile * (0.45f + 1.1f * Noise(world[0] / 6.f + 101.f, world[2] / 6.f + 101.f));
  patch = (std::max)(patch, SmoothStep(0.02f, 0.08f, depthHere));
  return patch >= 0.5f && settled * patch > 0.002f;
}

namespace
{
  // A print in the snow (see MostSnowPrints); g_Mutex held
  void AddSnowPrint(float const* at, float const* dir, float const* out, float left, bool hand = false, int stamp = -1)
  {
    SnowPrintMark& p = g_SnowPrints[g_SnowPrintNext];
    g_SnowPrintNext = (g_SnowPrintNext + 1) % MostSnowPrints;
    memcpy(p.At, at, sizeof(p.At));
    p.Dir[0] = dir[0];
    p.Dir[1] = dir[1];
    p.Out[0] = out[0];
    p.Out[1] = out[1];
    p.Left = left;
    p.Hand = hand;
    p.Stamp = stamp;
  }

  // Her right across a way (seen from above), as the rain's FootSide has it:
  // the camera's right turned from the way it looks, taken while it's upright
  // and not looking straight down (g_Handedness). g_Mutex held.
  void SnowRight(float const* way, float* right)
  {
    DrawnWet const& w = g_DrawnWet;
    float fx = w.Forward[0], fz = w.Forward[2];
    if (fx * fx + fz * fz > 0.09f && w.Up[1] > 0.3f)
      g_Handedness = fx * w.Right[2] - fz * w.Right[0] >= 0.f ? 1.f : -1.f;
    right[0] = g_Handedness >= 0.f ? -way[1] : way[1];
    right[1] = g_Handedness >= 0.f ? way[0] : -way[0];
  }

  // Which foot a new step in the snow is (as the rain's FootSide): the way
  // she walks over her last two steps (without her feet's zigzag), and
  // whether this step lands to its right or left of her last one; not told
  // (turning, standing, her first steps): the other foot of the last. Before
  // the steps move on; g_Mutex held.
  void SnowFoot(float const* world, ULONGLONG now, float* walk, float* outward, int known)
  {
    // (the way she faces, when it's known: see HerForward; the foot, when
    // the game says which: its bone, see wetlook::InSnow's foot)
    walk[0] = g_SnowLastDir[0];
    walk[1] = g_SnowLastDir[1];
    if (HerForward(walk))
    {
    }
    else if (g_SnowPrevStepTick && now - g_SnowPrevStepTick < 2500)
    {
      float wx = world[0] - g_SnowPrevStep[0], wz = world[2] - g_SnowPrevStep[2];
      float l = sqrtf(wx * wx + wz * wz);
      if (l > 0.3f && l < 5.f)
      {
        walk[0] = wx / l;
        walk[1] = wz / l;
      }
    }
    float right[2];
    SnowRight(walk, right);
    float side = known != 0 ? (known > 0 ? 1.f : -1.f) : -g_SnowFootSide;
    if (known == 0 && g_SnowLastStepTick && now - g_SnowLastStepTick < 2000)
    {
      float across = (world[0] - g_SnowLastStep[0]) * right[0] + (world[2] - g_SnowLastStep[2]) * right[1];
      if (fabsf(across) > 0.06f)
        side = across > 0.f ? 1.f : -1.f;
    }
    g_SnowFootSide = side;
    outward[0] = right[0] * side;
    outward[1] = right[1] * side;
    g_SnowFootOut[0] = outward[0];
    g_SnowFootOut[1] = outward[1];
  }

  // A stretch of her slide's trail in the snow (see MostSnowTrails); g_Mutex held
  void AddSnowTrail(float const* from, float const* to)
  {
    SnowTrailMark& s = g_SnowTrails[g_SnowTrailNext];
    g_SnowTrailNext = (g_SnowTrailNext + 1) % MostSnowTrails;
    memcpy(s.Start, from, sizeof(s.Start));
    memcpy(s.End, to, sizeof(s.End));
    s.Left = 1.f;
    s.Trail = g_SnowTrailId;
    s.Seq = ++g_SnowTrailSeq;
  }

  // Her slide's trail goes on to a point where the snow lies (see
  // MostSnowTrails): a stretch from the last point once it's SnowTrailStep
  // away (a new trail if the last point is older than SnowSlideKeepMs or
  // more than 3 m off: she's slid elsewhere). g_Mutex held.
  void ExtendSnowTrail(float const* at, ULONGLONG now)
  {
    float dx = at[0] - g_SnowTrailLast[0], dy = at[1] - g_SnowTrailLast[1], dz = at[2] - g_SnowTrailLast[2];
    float moved = sqrtf(dx * dx + dz * dz);
    if (!g_SnowTrailLastTick || now - g_SnowTrailLastTick > SnowSlideKeepMs || moved > 3.f || fabsf(dy) > 1.f)
    {
      memcpy(g_SnowTrailLast, at, sizeof(g_SnowTrailLast));
      g_SnowTrailLastTick = now;
      ++g_SnowTrailId;
      g_SnowTrailFresh = true;
      if (!g_SnowTrailLoggedTick || now - g_SnowTrailLoggedTick > 30000)
      {
        g_SnowTrailLoggedTick = now;
        util::log::Write("Snow: Faith slides through the snow at %.2f %.2f %.2f: her trail drawn", at[0], at[1], at[2]);
      }
      return;
    }
    if (moved < SnowTrailStep)
      return;
    if (g_SnowTrailFresh)
    {
      // where her hips were as she began: behind her feet, the way she slides
      float hips[3] = { g_SnowTrailLast[0] - dx / moved * SnowSlideHipsBehind, g_SnowTrailLast[1],
        g_SnowTrailLast[2] - dz / moved * SnowSlideHipsBehind };
      AddSnowTrail(hips, g_SnowTrailLast);
      g_SnowTrailFresh = false;
    }
    AddSnowTrail(g_SnowTrailLast, at);
    memcpy(g_SnowTrailLast, at, sizeof(g_SnowTrailLast));
    g_SnowTrailLastTick = now;
  }
}

bool wetlook::InSnow(float const* world, float up, int foot)
{
  weather::SnowFall snow = weather::GetSnow();
  if (!SnowLiesAt(world, up, snow))
    return false;
  // Her print there (see MostSnowPrints): one per step (a step's sound and
  // effect, or the same step asked again, within StepSameMs and
  // StepSameMetres, count once); the way she's going from her last step,
  // when it's a step away. A landing (she was falling, see SnowLandFall):
  // both feet side by side, deeper.
  ULONGLONG now = GetTickCount64();
  std::lock_guard<std::mutex> lock(g_Mutex);
  float mx = world[0] - g_SnowLastStep[0], mz = world[2] - g_SnowLastStep[2];
  float moved = sqrtf(mx * mx + mz * mz);
  if (g_SnowLastStepTick && now - g_SnowLastStepTick < StepSameMs && moved < StepSameMetres)
    return true;
  if (g_SnowLastStepTick && now - g_SnowLastStepTick < 2000 && moved > 0.2f && moved < 3.f)
  {
    g_SnowLastDir[0] = mx / moved;
    g_SnowLastDir[1] = mz / moved;
  }
  // (her shoe's sole, which foot and its outer side: SnowFoot, before the
  // steps move on)
  float walk[2], outward[2];
  SnowFoot(world, now, walk, outward, foot);
  memcpy(g_SnowPrevStep, g_SnowLastStep, sizeof(g_SnowPrevStep));
  g_SnowPrevStepTick = g_SnowLastStepTick;
  memcpy(g_SnowLastStep, world, sizeof(g_SnowLastStep));
  g_SnowLastStepTick = now;
  g_SnowJumpPressed = false;
  bool landing = g_SnowFellTick && now - g_SnowFellTick < SnowLandWithinMs;
  if (landing)
  {
    float right[2];
    SnowRight(walk, right);
    float leftOut[2] = { -right[0], -right[1] };
    float leftFoot[3] = { world[0] - right[0] * SnowFeetApart, world[1], world[2] - right[1] * SnowFeetApart };
    float rightFoot[3] = { world[0] + right[0] * SnowFeetApart, world[1], world[2] + right[1] * SnowFeetApart };
    AddSnowPrint(leftFoot, walk, leftOut, SnowLandDeeper);
    AddSnowPrint(rightFoot, walk, right, SnowLandDeeper);
    g_SnowFellTick = 0;
    g_SnowJumpPressed = true;   // (both feet are down already)
  }
  else
    AddSnowPrint(world, walk, outward, 1.f);
  if (!g_SnowPrintsLoggedTick || now - g_SnowPrintsLoggedTick > 30000)
  {
    g_SnowPrintsLoggedTick = now;
    util::log::Write("Snow: Faith's %s prints in the snow (at %.2f %.2f %.2f, the snow %.0f cm deep there)",
      landing ? "landing leaves two" : "steps leave", world[0], world[1], world[2],
      100.f * snow.Pile * (0.45f + 1.1f * Noise(world[0] / 6.f + 101.f, world[2] / 6.f + 101.f)));
  }
  return true;
}

void wetlook::NoteHand(float const* world, float up, int hand, int stamp)
{
  // (her hand over the touch kept as drawn, see g_HandKeep; with or without
  // snow there)
  {
    std::lock_guard<std::mutex> lock(g_Mutex);
    int h = hand > 0 ? 1 : 0;
    memcpy(g_HandKeep[h], world, sizeof(g_HandKeep[h]));
    g_HandKeepMs[h] = g_PlayMs.load();
  }
  // Her hand's print where the snow lies (WetLook.hlsl's HandShape): one per
  // touch of each hand (within SnowHandSameMs and SnowHandSameMetres of its
  // last, the same touch), palm down, its fingers the way she's going, its
  // thumb on her other hand's side
  weather::SnowFall snow = weather::GetSnow();
  if (!SnowLiesAt(world, up, snow))
    return;
  ULONGLONG now = GetTickCount64();
  std::lock_guard<std::mutex> lock(g_Mutex);
  int h = hand > 0 ? 1 : 0;
  float dx = world[0] - g_SnowHand[h][0], dz = world[2] - g_SnowHand[h][2];
  if (g_SnowHandTick[h] && now - g_SnowHandTick[h] < SnowHandSameMs && dx * dx + dz * dz < SnowHandSameMetres * SnowHandSameMetres)
    return;
  memcpy(g_SnowHand[h], world, sizeof(g_SnowHand[h]));
  g_SnowHandTick[h] = now;
  float right[2];
  // (its fingers the way she faces, see HerForward; the way she walked while
  // that isn't known)
  float way[2] = { g_SnowLastDir[0], g_SnowLastDir[1] };
  HerForward(way);
  SnowRight(way, right);
  float thumb[2] = { hand > 0 ? -right[0] : right[0], hand > 0 ? -right[1] : right[1] };
  AddSnowPrint(world, way, thumb, 1.f, true, stamp);
  // (its own log, not the steps': 2026-10-27, whether her hands' touches come
  // when she lands is wanted from it)
  if (!g_SnowHandsLoggedTick || now - g_SnowHandsLoggedTick > 30000)
  {
    g_SnowHandsLoggedTick = now;
    util::log::Write("Snow: Faith's %s hand leaves a print in the snow at %.2f %.2f %.2f", hand > 0 ? "right" : "left",
      world[0], world[1], world[2]);
  }
}

void wetlook::NoteRoll(float const* world, float up)
{
  // Her body's print where the snow lies (see MostSnowRolls), the way she was
  // going until her feet show the way she rolls
  weather::SnowFall snow = weather::GetSnow();
  if (!SnowLiesAt(world, up, snow))
    return;
  ULONGLONG now = GetTickCount64();
  std::lock_guard<std::mutex> lock(g_Mutex);
  SnowRollMark const& last = g_SnowRolls[(g_SnowRollNext + MostSnowRolls - 1) % MostSnowRolls];
  float dx = world[0] - last.Start[0], dz = world[2] - last.Start[2];
  if (last.Tick && now - last.Tick < SnowRollSameMs && dx * dx + dz * dz < 4.f)
    return;
  SnowRollMark& r = g_SnowRolls[g_SnowRollNext];
  g_SnowRollFollowing = g_SnowRollNext;
  g_SnowRollNext = (g_SnowRollNext + 1) % MostSnowRolls;
  memcpy(r.Start, world, sizeof(r.Start));
  r.Way[0] = g_SnowLastDir[0];
  r.Way[1] = g_SnowLastDir[1];
  r.Left = 1.f;
  r.Tick = now;
  r.Logged = g_SnowRollsLoggedTick && now - g_SnowRollsLoggedTick < 30000;
  if (!r.Logged)
    g_SnowRollsLoggedTick = now;
}

void wetlook::NoteSnowSlide(float const* world)
{
  weather::SnowFall snow = weather::GetSnow();
  if (!SnowLiesAt(world, 1.f, snow))
    return;
  ULONGLONG now = GetTickCount64();
  // (her trail's own shape only while her body doesn't press the snow by its
  // mesh, SnowBodies.cpp)
  ULONGLONG pressedAt = snowbody::FaithPressedAt();
  bool bodyPressed = pressedAt && (pressedAt > now || now - pressedAt < BodyPressedMs);
  std::lock_guard<std::mutex> lock(g_Mutex);
  g_SnowSlideTick = now;
  if (!bodyPressed)
    ExtendSnowTrail(world, now);
}

bool wetlook::SnowLayerAt(float const* world, float& layer)
{
  weather::SnowFall snow = weather::GetSnow();
  if (!SnowLiesAt(world, 1.f, snow))
    return false;
  // (WetLook.hlsl's SnowDepthAt)
  layer = snow.Pile * (0.45f + 1.1f * Noise(world[0] / 6.f + 101.f, world[2] / 6.f + 101.f));
  return true;
}

void wetlook::OtherSnowStep(float const* world, float up, int foot)
{
  weather::SnowFall snow = weather::GetSnow();
  if (!SnowLiesAt(world, up, snow))
    return;
  // The way their toe points: from their foot's own bones now
  // (SnowBodies.cpp; before g_Mutex: its lock is never taken under it), else
  // from their step before (a print of theirs 0.3 - 2 m off within 2.5 s)
  float way[2];
  bool fromBones = snowbody::OtherFootWay(world, foot, way);
  ULONGLONG now = GetTickCount64();
  std::lock_guard<std::mutex> lock(g_Mutex);
  OtherSnowPrintMark const* before = nullptr;
  float beforeDistance = 1e9f;
  for (OtherSnowPrintMark const& o : g_OtherSnowPrints)
  {
    if (o.Left <= 0.f || !o.Tick)
      continue;
    float dx = world[0] - o.At[0], dz = world[2] - o.At[2];
    float d = sqrtf(dx * dx + dz * dz);
    // (the same step asked again)
    if (now - o.Tick < 400 && d < 0.12f)
      return;
    if (now - o.Tick < 2500 && d > 0.3f && d < 2.f && fabsf(world[1] - o.At[1]) < 0.5f && d < beforeDistance)
    {
      before = &o;
      beforeDistance = d;
    }
  }
  if (!fromBones)
  {
    if (!before)
      return;
    way[0] = (world[0] - before->At[0]) / beforeDistance;
    way[1] = (world[2] - before->At[2]) / beforeDistance;
  }
  // (its outer side: that foot's side of the way it points, as hers, see
  // SnowRight; a foot not known is taken as their right)
  float right[2];
  SnowRight(way, right);
  float side = foot < 0 ? -1.f : 1.f;
  OtherSnowPrintMark& p = g_OtherSnowPrints[g_OtherSnowPrintNext];
  g_OtherSnowPrintNext = (g_OtherSnowPrintNext + 1) % MostOtherSnowPrints;
  memcpy(p.At, world, sizeof(p.At));
  p.Dir[0] = way[0];
  p.Dir[1] = way[1];
  p.Out[0] = right[0] * side;
  p.Out[1] = right[1] * side;
  p.Left = 1.f;
  p.Tick = now;
  if (!g_OtherSnowPrintsLoggedTick || now - g_OtherSnowPrintsLoggedTick > 30000)
  {
    g_OtherSnowPrintsLoggedTick = now;
    util::log::Write("Snow: a KrugerSec's step leaves a boot print in the snow at %.2f %.2f %.2f (its toe's way from %s)",
      world[0], world[1], world[2], fromBones ? "their foot's bones" : "their step before");
  }
}

// The performance test's breakdown of the wet look's pass (2026-11-02, the
// user said "Yes, add it" to a per-part GPU breakdown of the wet look in the
// performance test): once a test is over (perf::TestRunning), in
// BreakdownFrames frames BreakdownEveryMs apart, the frame's pass drawn
// again BreakdownVariants times into a frame of its own (never shown), each
// time with more of its parts left out (WetLook.hlsl's g_Skip, b2:
// BreakdownSkips, each leaving out what the one before did and one more),
// timed by the GPU. A part's cost is the difference, the mean over those
// frames; one line in CT.log. Every frame's own drawing has all its parts
// (BreakdownSkips(0), 0). The reflections are left out first, so their cost
// isn't in the parts after (the puddles' tests let fewer pixels reflect).
// Drawn with the copy of the pass the frame's was (the one without the snow's
// code while no snow lies, see g_RainPs), and the whole one too then, timed
// on its own: what that copy saves.
namespace
{
  // (the parts in the order they're left out, WetLook.hlsl's Skip* bits: each
  // draw leaves out the ones before it; the last draw all of it, SkipAll, the
  // frame only read. 15 parts since 2026-11-02, the user: "New rain
  // performance test can you read it and find any improvements?": the first
  // breakdown's 7 told 3.9 ms of the pass's 8.5 as one part, and two of its
  // parts weren't left out at all, see WetLook.hlsl's Skipped)
  // (20 parts with the snow's own 5, 65536u - 1048576u, before the rest of
  // it: since 2026-11-02, the user: "Check the snow performance test and find
  // a way to improve performance")
  const unsigned int BreakdownLeftOut[] = { 1u, 2u, 4u, 8u, 64u, 128u, 16u, 65536u, 131072u, 262144u, 524288u, 1048576u,
    32768u, 256u, 512u, 1024u, 2048u, 4096u, 8192u, 16384u };
  const int BreakdownVariants = (int)(sizeof(BreakdownLeftOut) / sizeof(BreakdownLeftOut[0])) + 2;
  const char* const BreakdownParts[] = { "its reflections",
    "the puddles' surface tests (holes, stairs, edges, flat round)",
    "the edge and slope tests (outlines in front, slopes, what rises from a puddle)",
    "the kept-dry boxes (Faith, people, vehicles, trains; their own wet look)",
    "the rain's rings and drops on the puddles",
    "the other rings and waves (steps, drips, drainpipes, her slide, bodies, the tracks' water)",
    "the rest of the wet ground (darkening, puddles, their blur and sheen, the water down slopes, tyre tracks)",
    "the snow's piled side (up the foot of walls and curbs)",
    "what's pressed into the snow (her prints, slides, rolls and stamps, the crowd's and the traffic's marks)",
    "the snow's light (the game's G-buffer, or the frame round it)",
    "the snow's edges and what's kept out of it (Faith, people, vehicles, her hands and shoes)",
    "the snow's facing (the depth's round it, the G-buffer's)",
    "the rest of the snow on the ground (its drifts, grain and glints, how open it is, the fog)",
    "the lightning (its flash and its bolts)", "the rain in the distance", "the haze",
    "the drips, the drainpipes' water and the splashes",
    "her footprints, the ground's marks, her handprints and her own splash", "the shelter and the interiors",
    "the frame's facing (the depth's and the G-buffer's)", "the rest of the pass (the depth, the point's place)",
    "the pass itself (reading the frame)" };
  static_assert(sizeof(BreakdownParts) / sizeof(BreakdownParts[0]) == BreakdownVariants, "a name for each part");
  // A draw's switches (b2's x)
  unsigned int BreakdownSkips(int variant)
  {
    if (variant >= BreakdownVariants - 1)
      return 32u;
    unsigned int skips = 0u;
    for (int v = 0; v < variant; ++v)
      skips |= BreakdownLeftOut[v];
    return skips;
  }
  const int BreakdownFrames = 3;
  const ULONGLONG BreakdownEveryMs = 300, BreakdownGiveUpMs = 15000;
  struct BreakdownQueries
  {
    ComPtr<ID3D11Query> Disjoint;
    ComPtr<ID3D11Query> Stamp[BreakdownVariants + 2];   // (the last: the whole copy's end, see DrawBreakdown)
    bool Pending = false;
    bool Other = false;   // the whole copy drawn too
  };
  // Render thread
  ComPtr<ID3D11Buffer> g_SkipBuffers[BreakdownVariants];   // b2, one per variant
  int g_SkipBuffersState = 0;                               // 0 not made, 1 made, -1 couldn't be
  ComPtr<ID3D11Texture2D> g_BreakdownFrame;
  ComPtr<ID3D11RenderTargetView> g_BreakdownView;
  BreakdownQueries g_BreakdownQueries[BreakdownFrames];
  int g_BreakdownQueriesState = 0;
  bool g_PerfTestWas = false;
  int g_BreakdownDrawn = -1;   // frames drawn (-1: none asked for)
  int g_BreakdownRead = 0, g_BreakdownTimed = 0;
  double g_BreakdownMs[BreakdownVariants] = {};
  double g_BreakdownOtherMs = 0.0;   // the whole copy's (see DrawBreakdown)
  int g_BreakdownOtherTimed = 0;
  ULONGLONG g_BreakdownAskedAt = 0, g_BreakdownLastAt = 0;
  bool g_BreakdownSnowCopy = false;   // the copy it drew was the snow's (g_SnowGroundPs), not the rain's
  bool g_BreakdownGbuffer = false;    // the wet ground was in the game's G-buffer then (GbufferWet.h): not in this pass

  // A variant's switches (b2), the buffers made once; null if they couldn't
  // be (b2 then given none: its constants read 0, everything drawn)
  ID3D11Buffer* SkipBuffer(ID3D11Device* pDevice, int variant)
  {
    if (g_SkipBuffersState == 0 && pDevice)
    {
      g_SkipBuffersState = 1;
      for (int v = 0; v < BreakdownVariants; ++v)
      {
        unsigned int bits[4] = { BreakdownSkips(v), 0u, 0u, 0u };
        D3D11_BUFFER_DESC desc{ sizeof(bits), D3D11_USAGE_IMMUTABLE, D3D11_BIND_CONSTANT_BUFFER, 0, 0, 0 };
        D3D11_SUBRESOURCE_DATA data{ bits, 0, 0 };
        if (FAILED(pDevice->CreateBuffer(&desc, &data, g_SkipBuffers[v].GetAddressOf())))
          g_SkipBuffersState = -1;
      }
      if (g_SkipBuffersState < 0)
        util::log::Warning("Wet surfaces: the performance test's breakdown buffers couldn't be made: no breakdown");
    }
    return g_SkipBuffersState > 0 ? g_SkipBuffers[variant].Get() : nullptr;
  }

  void EndBreakdown(char const* why)
  {
    if (why)
      util::log::Write("Performance test: the wet look's parts not timed: %s", why);
    g_BreakdownDrawn = -1;
    g_BreakdownView.Reset();
    g_BreakdownFrame.Reset();
  }

  // Each frame, at the start of the wet look's work: a test just over asks
  // for the breakdown; what's come back from the GPU added up; once all of it
  // is in, the line in the log
  void UpdateBreakdown(ID3D11DeviceContext* pContext)
  {
    bool testing = perf::TestRunning();
    ULONGLONG now = GetTickCount64();
    if (g_PerfTestWas && !testing)
    {
      g_BreakdownDrawn = 0;
      g_BreakdownRead = g_BreakdownTimed = 0;
      memset(g_BreakdownMs, 0, sizeof(g_BreakdownMs));
      g_BreakdownOtherMs = 0.0;
      g_BreakdownOtherTimed = 0;
      g_BreakdownAskedAt = now;
      g_BreakdownLastAt = 0;
    }
    g_PerfTestWas = testing;
    if (g_BreakdownDrawn < 0 || !pContext)
      return;
    for (BreakdownQueries& q : g_BreakdownQueries)
    {
      if (!q.Pending)
        continue;
      D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint{};
      UINT64 stamps[BreakdownVariants + 2] = {};
      bool back = pContext->GetData(q.Disjoint.Get(), &disjoint, sizeof(disjoint), D3D11_ASYNC_GETDATA_DONOTFLUSH) == S_OK;
      for (int s = 0; s <= BreakdownVariants + 1 && back; ++s)
        back = pContext->GetData(q.Stamp[s].Get(), &stamps[s], sizeof(UINT64), D3D11_ASYNC_GETDATA_DONOTFLUSH) == S_OK;
      if (!back)
        continue;
      q.Pending = false;
      ++g_BreakdownRead;
      bool ordered = true;
      for (int s = 0; s <= BreakdownVariants; ++s)
        ordered = ordered && stamps[s + 1] >= stamps[s];
      if (disjoint.Disjoint || !disjoint.Frequency || !ordered)
        continue;
      double perTick = 1000.0 / (double)disjoint.Frequency;
      for (int v = 0; v < BreakdownVariants; ++v)
        g_BreakdownMs[v] += (double)(stamps[v + 1] - stamps[v]) * perTick;
      ++g_BreakdownTimed;
      if (q.Other)
      {
        g_BreakdownOtherMs += (double)(stamps[BreakdownVariants + 1] - stamps[BreakdownVariants]) * perTick;
        ++g_BreakdownOtherTimed;
      }
    }
    if (g_BreakdownDrawn >= BreakdownFrames && g_BreakdownRead >= g_BreakdownDrawn)
    {
      if (!g_BreakdownTimed)
      {
        EndBreakdown("the GPU's timings didn't hold (its clock changed while it timed them)");
        return;
      }
      double ms[BreakdownVariants];
      for (int v = 0; v < BreakdownVariants; ++v)
        ms[v] = g_BreakdownMs[v] / g_BreakdownTimed;
      char parts[4096];
      int at = 0;
      for (int v = 0; v < BreakdownVariants && at >= 0 && at < (int)sizeof(parts); ++v)
      {
        double cost = v + 1 < BreakdownVariants ? ms[v] - ms[v + 1] : ms[v];
        int n = sprintf_s(parts + at, sizeof(parts) - at, "%s%s %.2f", v ? "; " : "", BreakdownParts[v], cost);
        at = n < 0 ? -1 : at + n;
      }
      char copy[256] = "";
      if (g_BreakdownOtherTimed)
        sprintf_s(copy, sizeof(copy), g_BreakdownSnowCopy ?
          " (its copy without the rain's code, drawn while snow lies and none of the rain's is on; the whole pass with "
          "it %.2f)" : " (its copy without the snow's code, drawn while no snow lies; the whole pass with it %.2f)",
          g_BreakdownOtherMs / g_BreakdownOtherTimed);
      util::log::Write("Performance test, the wet look's parts (GPU ms, the mean of %d frames right after the test, every "
        "part timed in the same frame): the whole pass %.2f%s: %s%s", g_BreakdownTimed, ms[0], copy, at > 0 ? parts : "",
        g_BreakdownGbuffer ? " (the wet ground was drawn in the game's own G-buffer then, GbufferWet.cpp: its darkening, "
        "puddles, rings and reflections aren't in this pass, and where it's open and kept dry is read from its info, so "
        "those parts here are about 0; that pass has its own line, "
        "\"Wet surfaces in the game's lighting (performance, diagnostic)\")" : "");
      EndBreakdown(nullptr);
      return;
    }
    if (now - g_BreakdownAskedAt > BreakdownGiveUpMs)
      EndBreakdown(g_BreakdownDrawn ? "the GPU's timings didn't come back" :
        "the wet surfaces weren't drawn in the 15 s after it (no rain or wet look on)");
  }

  // After the frame's own pass and its ice and flakes, the frame's state still
  // set: the breakdown's frame (see BreakdownSkips), the pass part by part,
  // if one is asked for and it's time. pass: the copy of the pass the frame's
  // was drawn with (see g_RainPs).
  void DrawBreakdown(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, D3D11_TEXTURE2D_DESC const& back,
    D3D11_VIEWPORT const& viewport, ID3D11PixelShader* pass)
  {
    if (g_BreakdownDrawn < 0 || g_BreakdownDrawn >= BreakdownFrames)
      return;
    ULONGLONG now = GetTickCount64();
    if (g_BreakdownLastAt && now - g_BreakdownLastAt < BreakdownEveryMs)
      return;
    if (!SkipBuffer(pDevice, 0))
    {
      EndBreakdown("its buffers couldn't be made");
      return;
    }
    if (g_BreakdownQueriesState == 0)
    {
      g_BreakdownQueriesState = 1;
      D3D11_QUERY_DESC disjoint{ D3D11_QUERY_TIMESTAMP_DISJOINT, 0 }, stamp{ D3D11_QUERY_TIMESTAMP, 0 };
      for (BreakdownQueries& q : g_BreakdownQueries)
      {
        if (FAILED(pDevice->CreateQuery(&disjoint, q.Disjoint.GetAddressOf())))
          g_BreakdownQueriesState = -1;
        for (ComPtr<ID3D11Query>& s : q.Stamp)
          if (FAILED(pDevice->CreateQuery(&stamp, s.GetAddressOf())))
            g_BreakdownQueriesState = -1;
      }
    }
    if (g_BreakdownQueriesState < 0)
    {
      EndBreakdown("the GPU's timing queries couldn't be made");
      return;
    }
    BreakdownQueries* slot = nullptr;
    for (BreakdownQueries& q : g_BreakdownQueries)
      if (!q.Pending)
      {
        slot = &q;
        break;
      }
    if (!slot)
      return;
    // Its own frame, the frame's size and format, made for the breakdown and
    // let go after it
    DXGI_FORMAT format = TypedFormat(back.Format);
    D3D11_TEXTURE2D_DESC have{};
    if (g_BreakdownFrame)
      g_BreakdownFrame->GetDesc(&have);
    if (!g_BreakdownFrame || have.Width != back.Width || have.Height != back.Height || have.Format != format)
    {
      g_BreakdownView.Reset();
      g_BreakdownFrame.Reset();
      D3D11_TEXTURE2D_DESC desc{};
      desc.Width = back.Width;
      desc.Height = back.Height;
      desc.MipLevels = 1;
      desc.ArraySize = 1;
      desc.Format = format;
      desc.SampleDesc.Count = 1;
      desc.Usage = D3D11_USAGE_DEFAULT;
      desc.BindFlags = D3D11_BIND_RENDER_TARGET;
      if (FAILED(pDevice->CreateTexture2D(&desc, nullptr, g_BreakdownFrame.GetAddressOf())) ||
          FAILED(pDevice->CreateRenderTargetView(g_BreakdownFrame.Get(), nullptr, g_BreakdownView.GetAddressOf())))
      {
        EndBreakdown("its frame couldn't be made");
        return;
      }
    }
    UINT zero = 0;
    ID3D11Buffer* noBuffer = nullptr;
    const float blendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
    pContext->OMSetRenderTargets(1, g_BreakdownView.GetAddressOf(), nullptr);
    pContext->RSSetViewports(1, &viewport);
    pContext->OMSetBlendState(g_Blend.Get(), blendFactor, 0xFFFFFFFF);
    pContext->IASetInputLayout(nullptr);
    pContext->IASetVertexBuffers(0, 1, &noBuffer, &zero, &zero);
    pContext->VSSetShader(g_Vs.Get(), nullptr, 0);
    pContext->PSSetShader(pass, nullptr, 0);
    pContext->Begin(slot->Disjoint.Get());
    pContext->End(slot->Stamp[0].Get());
    for (int v = 0; v < BreakdownVariants; ++v)
    {
      ID3D11Buffer* skip = SkipBuffer(pDevice, v);
      pContext->PSSetConstantBuffers(2, 1, &skip);
      pContext->Draw(3, 0);
      pContext->End(slot->Stamp[v + 1].Get());
    }
    ID3D11Buffer* everything = SkipBuffer(pDevice, 0);
    pContext->PSSetConstantBuffers(2, 1, &everything);
    // and the whole copy, with the snow's code, when the frame's was the one
    // without it: what that copy saves
    slot->Other = pass != g_Ps.Get();
    if (slot->Other)
      g_BreakdownSnowCopy = pass == g_SnowGroundPs.Get();
    g_BreakdownGbuffer = g_LastConstants.Options.y > 0.5f;
    if (slot->Other)
    {
      pContext->PSSetShader(g_Ps.Get(), nullptr, 0);
      pContext->Draw(3, 0);
      pContext->PSSetShader(pass, nullptr, 0);
    }
    pContext->End(slot->Stamp[BreakdownVariants + 1].Get());
    pContext->End(slot->Disjoint.Get());
    slot->Pending = true;
    ++g_BreakdownDrawn;
    g_BreakdownLastAt = now;
  }
}

int wetlook::ShelterGridOffset()
{
  return (int)offsetof(Constants, ShelterGrid);
}

bool wetlook::TakeGbufferPass(ID3D11DeviceContext* pContext, float const* transform, float const* projection, UINT width,
  UINT height, GbufferPass& out)
{
  out = GbufferPass{};
  Constants const& last = g_LastConstants;
  // (the wet look on, rain not snow, and something wet: its pass's own
  // switches as the last Apply wrote them)
  if (!g_LastFilled || !pContext || last.Rain.z < 0.5f || (last.Wet.x <= 0.f && last.Wet.y <= 0.f) || last.SnowDrift.z > 0.001f ||
      !width || !height || projection[0] == 0.f || projection[5] == 0.f)
    return false;
  if (!g_GbufferConstants)
  {
    ComPtr<ID3D11Device> device;
    pContext->GetDevice(device.GetAddressOf());
    D3D11_BUFFER_DESC d{};
    d.ByteWidth = sizeof(Constants);
    d.Usage = D3D11_USAGE_DYNAMIC;
    d.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    d.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (!device || FAILED(device->CreateBuffer(&d, nullptr, g_GbufferConstants.GetAddressOf())))
      return false;
  }
  D3D11_MAPPED_SUBRESOURCE mapped;
  if (FAILED(pContext->Map(g_GbufferConstants.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    return false;
  Constants* c = (Constants*)mapped.pData;
  memcpy(c, &last, sizeof(Constants));
  // This frame's camera's turn and projection, as Apply works them out (where
  // the camera is, Camera, stays the last frame's: GbufferWet.hlsl)
  XMFLOAT4X4 turn;
  memcpy(&turn, transform, sizeof(turn));
  turn._41 = turn._42 = turn._43 = 0.f;
  turn._44 = 1.f;
  XMMATRIX viewProjection = XMMatrixInverse(nullptr, XMLoadFloat4x4(&turn)) * XMLoadFloat4x4((XMFLOAT4X4 const*)projection);
  XMStoreFloat4x4(&c->ViewProjection, viewProjection);
  c->CameraRight = XMFLOAT4(transform[0], transform[1], transform[2], 0.f);
  c->CameraUp = XMFLOAT4(transform[4], transform[5], transform[6], 0.f);
  c->CameraBack = XMFLOAT4(transform[8], transform[9], transform[10], 0.f);
  c->ProjectionXY = XMFLOAT4(projection[0], projection[5], projection[8], projection[9]);
  c->ProjectionZW = XMFLOAT4(projection[10], projection[11], projection[14], projection[15]);
  c->ProjectionT = XMFLOAT4(projection[12], projection[13], 0.f, 0.f);
  // (the game's own depth, read at the G-buffer pass's own size; reversed
  // and the sky's depth as the last frame's, its projection's depth terms)
  c->Depth = XMFLOAT4(1.f, 1.f, last.Depth.z, last.Depth.w);
  c->Frame = XMFLOAT4(1.f / (float)width, 1.f / (float)height, last.Frame.z, last.Frame.w);
  // (its G-buffer: the copies GbufferWet.cpp binds, t6 and t7)
  c->GameFrame = XMFLOAT4(1.f, last.GameFrame.y, last.GameFrame.z, last.GameFrame.w);
  pContext->Unmap(g_GbufferConstants.Get(), 0);
  out.Constants = g_GbufferConstants.Get();
  for (int i = 0; i < 4; ++i)
    out.Views[i] = g_LastViews[i].Get();
  out.Samplers[0] = g_Sampler.Get();
  out.Samplers[1] = g_WrapSampler.Get();
  return true;
}

void wetlook::Apply(IDXGISwapChain* pSwapChain, ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
  // (its shaders made ahead, see StartMaking, and drawn once, see WarmUp)
  StartMaking(pDevice);
  if (!g_WarmedUp && pSwapChain && pDevice && pContext && !g_Failed.load(std::memory_order_relaxed) &&
      g_MakeState.load(std::memory_order_acquire) == 2)
    WarmUp(pSwapChain, pDevice, pContext);
  if (util::DiagnosticsOn())
    LogTimes();
  // (the performance test's breakdown, see BreakdownSkips)
  UpdateBreakdown(pContext);
  // (diagnostic: where the game's screen-space reflections run, SsrOrder.h)
  if (util::DiagnosticsOn())
    ssrorder::AtPresent(pContext);
  // (the constants for the G-buffer's wet ground only once written below)
  g_LastFilled = false;
  ScopedTime timed(TimeSetUp);
  // Faith's feet each frame, for how hard she comes down (see FallScale):
  // her speed down and along the ground from each new place (a jump of more
  // than 5 m in a frame, a cut, left out)
  {
    float feet[3];
    if (dry::FaithFeet(feet))
    {
      double ms = TimeNow();
      // (out in the rain, see below: her chest under the open sky)
      float chest[3] = { feet[0], feet[1] + 1.4f, feet[2] };
      bool inRain = weather::IsRaining(nullptr) && weather::ShelterAt(chest) < 0.25f;
      float facing[2];
      bool facingKnown = dry::FaithForward(facing);
      std::lock_guard<std::mutex> lock(g_Mutex);
      // Her right from her left, as the camera has it (see FootSide), each
      // frame; and her transform's forward row (see HerForward)
      {
        DrawnWet const& w = g_DrawnWet;
        float fx = w.Forward[0], fz = w.Forward[2];
        if (fx * fx + fz * fz > 0.09f && w.Up[1] > 0.3f)
          g_Handedness = fx * w.Right[2] - fz * w.Right[0] >= 0.f ? 1.f : -1.f;
      }
      if (facingKnown)
      {
        memcpy(g_FacingRaw, facing, sizeof(g_FacingRaw));
        g_FacingRawKnown = true;
      }
      // Out in the rain her hands and shoes stay wet (see SoakUp; 2026-10-26,
      // the user: "if faith's hands are wet they should also leave
      // handprints on the walls/poles she touches": only what she touched
      // wetted them, so her hands were dry in the rain)
      if (inRain && g_DrawnWet.Rain.z >= 0.5f)
      {
        SoakUp(g_ShoeWet, g_ShoeWetTick, ShoeWetNow(), 1.f, ShoeWetFromGround);
        SoakUp(g_HandWet, g_HandWetTick, HandWetNow(), 1.f, HandWetFromGround);
      }
      if (feet[0] != g_FallLast[0] || feet[1] != g_FallLast[1] || feet[2] != g_FallLast[2])
      {
        float dt = (float)((ms - g_FallLastMs) / 1000.0);
        float dx = feet[0] - g_FallLast[0], dy = feet[1] - g_FallLast[1], dz = feet[2] - g_FallLast[2];
        if (g_FallLastMs > 0.0 && dt > 0.002f && dt < 0.2f && dx * dx + dy * dy + dz * dz < 25.f)
        {
          FallSample& s = g_FallSamples[g_FallNext];
          g_FallNext = (g_FallNext + 1) % MostFallSamples;
          s.Ms = ms;
          s.Down = -dy / dt;
          s.Along = sqrtf(dx * dx + dz * dz) / dt;
          // (which way along her forward row she faces: the way she runs)
          if (g_FacingRawKnown && s.Along > FacingRunSpeed)
          {
            float d = (dx * g_FacingRaw[0] + dz * g_FacingRaw[1]) / (s.Along * dt);
            if (fabsf(d) > 0.7f)
              g_FacingEvidence = (std::max)(-MostFacingEvidence, (std::min)(MostFacingEvidence, g_FacingEvidence + (d > 0.f ? 1 : -1)));
            if (!g_FacingLogged && abs(g_FacingEvidence) >= FacingSure)
            {
              g_FacingLogged = true;
              util::log::Write("Wet surfaces: Faith faces %s her transform's forward row (from %d of her moves): her prints "
                "and handprints laid the way she faces", g_FacingEvidence > 0 ? "along" : "against", abs(g_FacingEvidence));
            }
          }
        }
        memcpy(g_FallLast, feet, sizeof(g_FallLast));
        g_FallLastMs = ms;
        // (come down on a landing pad: see NoteLandingPad)
        NoteLandingPad(feet, ms);
      }
    }
  }
  // The rings' and prints' clock (see g_PlayMs), from the steady clock
  // (see the drawn rain's clock below)
  {
    double ms = TimeNow();
    if (g_PlayFrom > 0.0 && !gamestate::Paused())
    {
      g_PlayCarry += (std::min)(ms - g_PlayFrom, 250.0);
      ULONGLONG whole = (ULONGLONG)g_PlayCarry;
      g_PlayMs += whole;
      g_PlayCarry -= (double)whole;
    }
    g_PlayFrom = ms;
  }
  Settings s;
  {
    std::lock_guard<std::mutex> lock(g_Mutex);
    s = g_Settings;
  }
  // Snow (VISUALS > Snow): falling, and on the ground in place of the wet look
  weather::SnowFall snow = weather::GetSnow();
  bool snowing = snow.Amount > 0.f, snowed = snow.Cover > 0.001f;
  // Her feet in the snow, each frame (see SnowJumpRise, MostSnowTrails):
  // falling (a landing next), rising off a step in the snow (a jump: its
  // other foot pressed), and her slide's trail followed between its effects
  {
    float feet[3];
    ULONGLONG tick = GetTickCount64();
    bool known = snowed && !gamestate::Paused() && dry::FaithFeet(feet);
    bool sliding = false;
    {
      std::lock_guard<std::mutex> lock(g_Mutex);
      // (still sliding: her slide's effect lately, her feet moving fast and low)
      float speed = 0.f, rise = 0.f;
      if (known && g_FeetPrevTick && tick > g_FeetPrevTick && tick - g_FeetPrevTick < 200)
      {
        float dt = (tick - g_FeetPrevTick) / 1000.f;
        float hx = feet[0] - g_FeetPrev[0], hz = feet[2] - g_FeetPrev[2];
        speed = sqrtf(hx * hx + hz * hz) / dt;
        rise = fabsf(feet[1] - g_FeetPrev[1]) / dt;
      }
      sliding = known && g_SnowSlideTick && tick - g_SnowSlideTick < SnowSlideKeepMs && speed > SnowSlideSpeed &&
        speed < 25.f && rise < 2.f;
    }
    bool slideSnow = sliding && SnowLiesAt(feet, 1.f, snow);
    // (where her body pressed the snow by its own mesh just now,
    // SnowBodies.cpp: her slide's trail and her roll's print give way to it)
    ULONGLONG pressedAt = snowbody::FaithPressedAt();
    bool bodyPressed = pressedAt && (pressedAt > tick || tick - pressedAt < BodyPressedMs);
    std::lock_guard<std::mutex> lock(g_Mutex);
    if (known && g_FeetPrevTick && tick > g_FeetPrevTick && tick - g_FeetPrevTick < 200)
    {
      float dt = (tick - g_FeetPrevTick) / 1000.f;
      float hx = feet[0] - g_FeetPrev[0], hz = feet[2] - g_FeetPrev[2];
      float vy = (feet[1] - g_FeetPrev[1]) / dt;
      if (hx * hx + hz * hz < 25.f)
      {
        if (vy < -SnowLandFall)
          g_SnowFellTick = tick;
        if (vy > SnowJumpRise && !g_SnowJumpPressed && g_SnowLastStepTick && tick - g_SnowLastStepTick < SnowJumpAfterMs)
        {
          // The other foot beside her last step: on the side away from the
          // step before it (that was the other foot)
          // (on her last step's inner side: its outer side's other way)
          float side[2] = { -g_SnowFootOut[0], -g_SnowFootOut[1] };
          float other[3] = { g_SnowLastStep[0] + side[0] * 2.f * SnowFeetApart - g_SnowLastDir[0] * 0.05f,
            g_SnowLastStep[1], g_SnowLastStep[2] + side[1] * 2.f * SnowFeetApart - g_SnowLastDir[1] * 0.05f };
          AddSnowPrint(other, g_SnowLastDir, side, 1.15f);
          g_SnowJumpPressed = true;
        }
      }
    }
    // Her roll's way (see MostSnowRolls): from where it began to her feet
    if (g_SnowRollFollowing >= 0)
    {
      SnowRollMark& r = g_SnowRolls[g_SnowRollFollowing];
      if (tick - r.Tick > SnowRollFollowMs || r.Left <= 0.f)
        g_SnowRollFollowing = -1;
      else if (bodyPressed)
      {
        r.Left = 0.f;
        g_SnowRollFollowing = -1;
      }
      else if (known)
      {
        float rx = feet[0] - r.Start[0], rz = feet[2] - r.Start[2];
        float l = sqrtf(rx * rx + rz * rz);
        if (!r.Logged)
        {
          // (where the game's contact for it is against her feet: told once a while)
          r.Logged = true;
          util::log::Write("Snow: Faith rolls in the snow at %.2f %.2f %.2f (%.2f m from her feet, %.2f m above them): her "
            "body's print", r.Start[0], r.Start[1], r.Start[2], l, r.Start[1] - feet[1]);
        }
        if (l > SnowRollMinMetres && l < 6.f && fabsf(feet[1] - r.Start[1]) < 1.5f)
        {
          r.Way[0] = rx / l;
          r.Way[1] = rz / l;
        }
      }
    }
    if (known)
    {
      memcpy(g_FeetPrev, feet, sizeof(g_FeetPrev));
      g_FeetPrevTick = tick;
    }
    else
      g_FeetPrevTick = 0;
    if (bodyPressed)
    {
      // (her trail so far gives way to her body's own print)
      for (SnowTrailMark& stretch : g_SnowTrails)
        if (stretch.Trail == g_SnowTrailId)
          stretch.Left = 0.f;
      g_SnowTrailLastTick = 0;
    }
    else if (slideSnow)
      ExtendSnowTrail(feet, tick);
  }
  // (and the ice over the water, still melting for a few seconds after)
  bool iced = weather::IceAmount() > 0.f;
  g_IcedNow.store(iced, std::memory_order_relaxed);
  bool wet = s.On && s.Wetness > 0.f && !snow.Chosen;
  float flash = weather::LightningFlash();
  weather::DistantRain rain = weather::GetDistantRain();
  // Render thread only
  static float boltA[MostBoltSegments][3], boltB[MostBoltSegments][3], boltWidth[MostBoltSegments], boltBright[MostBoltSegments];
  static int boltOf[MostBoltSegments];
  float boltDistance[MostBolts] = {};
  int boltCount = weather::GetBolts(boltA, boltB, boltWidth, boltBright, boltOf, boltDistance, MostBoltSegments, MostBolts);
  bool bolt = boltCount > 0;
  bool ok = !g_Failed.load(std::memory_order_relaxed) && pSwapChain && pDevice && pContext;
  // The depth is kept while the wet look, the lightning or the rain may need
  // it (it takes a frame or two to come)
  greenplane::WantDepth(ok && (wet || weather::ThunderOn() || flash > 0.f || rain.Amount > 0.f || snowing || snowed || iced));
  // The game's G-buffer pass watched while any of it is on (the scene's
  // depth is that pass's, see GreenPlane's OnDepthClear), and copied for the
  // snow while snow lies (GameFrame.h) and for the wet look while it's drawn
  // (its shading by the game's own smooth facing, WetLook.hlsl's
  // shadeNormal: 2026-11-01, the user: "you can see the triangles of the mesh")
  bool anyOn = ok && (wet || flash > 0.f || rain.Amount > 0.f || bolt || snowing || snowed || iced);
  gameframe::Want(anyOn ? pContext : nullptr, anyOn && (snowed || wet), snowed);
  if (!ok || (!wet && flash <= 0.f && rain.Amount <= 0.f && !bolt && !snowing && !snowed && !iced))
  {
    g_Drawn = false;
    return;
  }
  // Nothing over the game's loading screen (the user, 2026-10-26: "snow/rain
  // should not be visible on loadinscreens"): the wet look, the rain and
  // snow, the flash and the bolts all wait until it's gone
  // (gamestate::Loading, the loading screen's own flag)
  static bool s_loadingLogged = false;
  bool loading = gamestate::Loading();
  if (loading != s_loadingLogged)
  {
    s_loadingLogged = loading;
    util::log::Write("Weather: %s", loading ? "the game's loading screen is up: no rain, snow or wet look drawn over it" :
      "the game's loading screen is gone: the rain, snow and wet look drawn again");
  }
  if (loading)
  {
    g_Drawn = false;
    g_DrawnClockMs = TimeNow();
    return;
  }
  // Nor over the game's menus (the user, 2026-10-26: "blizzard ticked makes
  // the map menu all white make rain/snow not affect main menu and map
  // menu"): while its UI has the game paused (gamestate::Paused: its pause
  // menu, the map, anything that pauses it the same way). The tools' own
  // freeze and game speed don't pause it so, so a photo keeps its weather.
  static bool s_pausedLogged = false;
  bool paused = gamestate::Paused();
  if (paused != s_pausedLogged)
  {
    s_pausedLogged = paused;
    util::log::Write("Weather: %s", paused ? "the game is paused by its menu or map: no rain, snow or wet look drawn over it" :
      "the game's menu or map is gone: the rain, snow and wet look drawn again");
  }
  if (paused)
  {
    g_Drawn = false;
    g_DrawnClockMs = TimeNow();
    return;
  }
  // (its shaders still being made: nothing drawn yet, the frame not held up)
  if (g_MakeState.load(std::memory_order_acquire) != 2)
  {
    NoteSkip("its shaders are still being made (on a worker thread)");
    g_Drawn = false;
    return;
  }

  greenplane::FrameDepth frame{};
  ComPtr<ID3D11Texture2D> pBack;
  D3D11_TEXTURE2D_DESC back{};
  bool depthTaken = greenplane::TakeFrameDepth(pSwapChain, pDevice, pContext, frame);
  bool ready = depthTaken && SUCCEEDED(pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)pBack.GetAddressOf())) && pBack;
  if (ready)
  {
    pBack->GetDesc(&back);
    ready = back.SampleDesc.Count == 1 && back.Width == frame.Width && back.Height == frame.Height;
  }
  if (!ready || !CopyScene(pDevice, pContext, pBack.Get(), back))
  {
    NoteSkip(!depthTaken ? "the game's depth or camera wasn't there this frame (greenplane::TakeFrameDepth)" :
      !ready ? "the frame isn't the game's depth's size" : "the frame couldn't be copied");
    g_Drawn = false;
    return;
  }

  ComPtr<ID3D11RenderTargetView> pTarget;
  D3D11_RENDER_TARGET_VIEW_DESC targetDesc{};
  targetDesc.Format = TypedFormat(back.Format);
  targetDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
  if (FAILED(pDevice->CreateRenderTargetView(pBack.Get(), targetDesc.Format != back.Format ? &targetDesc : nullptr,
      pTarget.GetAddressOf())))
  {
    NoteSkip("the frame's render target view couldn't be made");
    g_Drawn = false;
    return;
  }

  // The constants: the camera's turn alone (positions relative to it, see
  // WetLook.hlsl) and the projection's own terms
  float const* t = frame.Transform;
  float const* p = frame.Projection;
  XMFLOAT4X4 turn;
  memcpy(&turn, t, sizeof(turn));
  turn._41 = turn._42 = turn._43 = 0.f;
  turn._44 = 1.f;
  XMMATRIX viewProjection = XMMatrixInverse(nullptr, XMLoadFloat4x4(&turn)) * XMLoadFloat4x4((XMFLOAT4X4 const*)p);
  if (p[0] == 0.f || p[5] == 0.f)
  {
    NoteSkip("the game's projection was empty");
    g_Drawn = false;
    return;
  }
  // Where something is over a point: the weather's squares
  int shelterCount = 0;
  float shelterSize = 0.f, shelterX = 0.f, shelterZ = 0.f;
  bool shelterGiven = weather::GetShelter(g_ShelterCells, shelterCount, shelterSize, shelterX, shelterZ, &g_ShelterEdges) &&
    UploadShelter(pDevice, pContext, g_ShelterCells, shelterCount);
  // Where the rain may be drawn: the open sky's squares when the weather
  // looks for roofs (rain kept out of buildings), anywhere when it doesn't,
  // nowhere until they're known
  int skyCount = 0;
  float skySize = 0.f, skyX = 0.f, skyZ = 0.f;
  bool skyKnown = rain.Amount > 0.f && weather::GetOpenSky(g_OpenSkyCells, skyCount, skySize, skyX, skyZ);
  bool skyGiven = skyKnown && UploadOpenSky(pDevice, pContext, g_OpenSkyCells, skyCount);
  float skyGrid = !weather::GetState().KeepOut ? -1.f : skyGiven ? (float)skyCount : 0.f;
  bool raining = weather::IsRaining(nullptr);
  // Her handprints from her own hand (SnowBodies.cpp's hand stamps, t5): its
  // texture's changed slices sent (before g_Mutex: snowbody's own lock may be
  // taken under it, never the other way round)
  ID3D11ShaderResourceView* handView = snowbody::TakeHands(pContext);
  // The game's G-buffer copied this frame (GameFrame.h), for the snow's
  // facing on models and its light (WetLook.hlsl's GameSurfaceAt and
  // GameLightAt); without it, as before. Told once if snow has lain 10 s
  // with no copy (its G-buffer pass not seen). Only a copy the depth copy's
  // size is taken: the shader reads it at the depth's texels (DepthTexel),
  // the game's render size, the frame's at a resolution scale of 100 %
  // (until 2026-11-10 only one the frame's size was, and it was read at the
  // frame's pixels: an outside tester's game drew at 5120 x 2880 for a 2560
  // x 1440 frame, and the snow never had the G-buffer).
  gameframe::Frame gbuffer{};
  bool haveGbuffer = (snowed || wet) && gameframe::Take(gbuffer) && gbuffer.Width == frame.DepthWidth &&
    gbuffer.Height == frame.DepthHeight;
  // (diagnostic: the camera drawn with against the game's own of the frame)
  if (haveGbuffer && util::DiagnosticsOn())
    gameframe::CheckCamera(pDevice, pContext, t, p, (float)back.Width, (float)back.Height);
  // The ground's marks (GroundMarks.h): the crowd's steps and the traffic's
  // wheels queued since drawn into its map, red where the snow lies, green
  // where the wet look is drawn; filled on the play clock as her prints are
  // (see SnowPrintFillSeconds; not while the game is paused), the water's
  // within groundmarks::WaterMarkSeconds
  groundmarks::Map marks{};
  bool haveMarks = false;
  {
    static ULONGLONG s_marksPlayMs = 0;
    ULONGLONG play = g_PlayMs.load();
    float seconds = s_marksPlayMs ? (play - s_marksPlayMs) / 1000.f : 0.f;
    s_marksPlayMs = play;
    // (red: the snow filling the marks, or in the rain the wet tracks drying)
    float redFill = snowed ? (snowing ? seconds * snow.Amount * snow.Intensity / SnowPrintFillSeconds : 0.f) :
      seconds / groundmarks::WetTrackSeconds;
    float waterFill = seconds / groundmarks::WaterMarkSeconds;
    float cameraAt[3] = { t[12], t[13], t[14] };
    haveMarks = groundmarks::Update(pDevice, pContext, cameraAt, snowed, wet, redFill, waterFill, marks);
  }
  {
    static double s_snowWithout = 0.0;
    static bool s_toldWithout = false, s_toldWith = false;
    double ms = TimeNow();
    if (!snowed || haveGbuffer)
      s_snowWithout = 0.0;
    else if (s_snowWithout == 0.0)
      s_snowWithout = ms;
    if (!s_toldWithout && s_snowWithout > 0.0 && ms - s_snowWithout > 10000.0)
    {
      s_toldWithout = true;
      util::log::Warning("Snow: no copy of the game's G-buffer for 10 s while snow lies: the snow's facing and light from the "
        "depth and the picture round it, as before");
    }
    if (!s_toldWith && haveGbuffer)
    {
      s_toldWith = true;
      util::log::Write("Snow: the game's own G-buffer used for the snow (its facing at edges and on models, its light without "
        "the ground's texture or reflections)");
    }
  }
  D3D11_MAPPED_SUBRESOURCE mapped;
  if (FAILED(pContext->Map(g_Constants.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    return;
  // (written into the tools' own copy, then into the buffer at once: the
  // next frame's wet ground in the game's G-buffer reads that copy, see
  // wetlook::TakeGbufferPass, and the buffer's memory is slow to read back)
  Constants* c = &g_LastConstants;
  // (what of each list can reach a pixel: all of the frame and the world
  // until it's made, see FrameBox)
  PutEverywhere(c->DripBounds);
  // (the falling water's tiles: a list not made this frame goes through all of it)
  g_Falling.Begin((float)back.Width, (float)back.Height);
  PutEverywhere(c->PipeBounds);
  PutEverywhere(c->SplashBounds);
  PutEverywhere(c->HerSplashBounds);
  PutEverywhere(c->DripLandBounds);
  PutEverywhere(c->PipeLandBounds);
  PutEverywhere(c->PrintBounds);
  // (a handprint pressed by her own hand goes to the pass as its stamp,
  // HandStampAt; while it's being pressed it's drawn neither way; one that
  // nothing went into, or without the texture, as HandShape. Gives whether
  // it's taken care of that way.)
  int handStamps = 0;
  auto handStamped = [&](int stamp, float left) -> bool
  {
    if (!handView || stamp < 0)
      return false;
    int state = snowbody::HandStampState(stamp);
    if (state == 0)
      return true;
    snowbody::HandStamp hs;
    if (state < 0 || !snowbody::HandStampOf(stamp, hs))
      return false;
    if (handStamps < snowbody::MostHandStamps)
    {
      c->HandStampAt[handStamps] = XMFLOAT4(hs.Middle[0] - t[12], hs.Middle[1] - t[13], hs.Middle[2] - t[14], left);
      c->HandStampU[handStamps] = XMFLOAT4(hs.U[0], hs.U[1], hs.U[2], (float)hs.Slice);
      c->HandStampV[handStamps++] = XMFLOAT4(hs.V[0], hs.V[1], hs.V[2], 0.f);
    }
    return true;
  };
  XMStoreFloat4x4(&c->ViewProjection, viewProjection);
  c->CameraRight = XMFLOAT4(t[0], t[1], t[2], 0.f);
  c->CameraUp = XMFLOAT4(t[4], t[5], t[6], 0.f);
  c->CameraBack = XMFLOAT4(t[8], t[9], t[10], 0.f);
  // The drawn rain's and the ripples' clock: real seconds, standing still
  // while the game is paused (its world stands still then too; see
  // g_HeldSince in Weather.cpp), wrapped at 1000 s as before. From the
  // steady clock (TimeNow): it was GetTickCount64's, which moves in steps of
  // about 15.6 ms, so at the game's 100 - 110 frames a second (CT.log's
  // performance lines) it stood still one frame and jumped the next, and all
  // that moves by it (the water running down the slopes, the rain's streaks,
  // the ripples, the drips) went in jerks (2026-10-26, the user: "Rain down
  // slopes still laggy")
  {
    double ms = TimeNow();
    if (g_DrawnClockMs > 0.0 && !gamestate::Paused())
      g_DrawnClock = fmod(g_DrawnClock + (std::min)((ms - g_DrawnClockMs) / 1000.0, 0.25), 1000.0);
    g_DrawnClockMs = ms;
    c->Camera = XMFLOAT4(t[12], t[13], t[14], (float)g_DrawnClock);
  }
  c->ProjectionXY = XMFLOAT4(p[0], p[5], p[8], p[9]);
  c->ProjectionZW = XMFLOAT4(p[10], p[11], p[14], p[15]);
  c->ProjectionT = XMFLOAT4(p[12], p[13], 0.f, 0.f);
  c->Depth = XMFLOAT4(frame.Scale[0], frame.Scale[1], frame.Reversed ? 1.f : 0.f, frame.SkyDepth);
  float lastLevel = (float)(g_SceneCopyLevels - 1);
  float blurLevel = (std::min)(lastLevel, (std::max)(0.f, log2f((float)back.Width / BlurLevelWidth)));
  c->Frame = XMFLOAT4(1.f / (float)back.Width, 1.f / (float)back.Height, blurLevel, lastLevel);
  // (y Snow brightness; z the same for the snow without the G-buffer, in the
  // picture's values, 1 at its default)
  {
    float bright = (std::max)(0.1f, snow.Brightness);
    c->GameFrame = XMFLOAT4(haveGbuffer ? 1.f : 0.f, bright, powf(bright / weather::SnowBrightnessDefault, 1.f / 2.2f), 0.f);
    c->MarksWindow = XMFLOAT4(marks.PerMetre, marks.Middle[0], marks.Middle[1], haveMarks ? marks.Reach : -1.f);
    c->MarksOn = XMFLOAT4(haveMarks && snowed ? 1.f : 0.f, haveMarks && wet ? 1.f : 0.f, haveMarks && wet && !snowed ? 1.f : 0.f,
      haveMarks ? marks.HeightLow : 0.f);
  }
  // Puddles as many as it rains (the user, 2026-10-25 23:5x: "if there is
  // less rain there should be less puddles"): x the rain's strength now
  // (the forecast's intensity x Strength against the Release mission's 0.7;
  // none without rain)
  // With Random rain the ground as wet as the forecast's rain has made it
  // and its puddles as full (weather::GetWetting; 2026-10-27, the user:
  // "wetness should be random and depending on the amount of rain")
  weather::Wetting wetting = weather::GetWetting();
  float wetness = s.Wetness * wetting.Ground;
  float puddles = s.Puddles * (wetting.Random ? wetting.Puddles : (std::max)(0.f, (std::min)(rain.Strength, 1.f)));
  c->Wet = XMFLOAT4(wetness, puddles, s.PuddleSize > 0.1f ? s.PuddleSize : 0.1f, s.Reflections);
  c->Rain = XMFLOAT4(s.Ripples && raining ? 1.f : 0.f, s.FadeDistance > 1.f ? s.FadeDistance : 1.f, wet ? 1.f : 0.f, 0.f);
  c->Flash = XMFLOAT4(flash, 0.75f, 0.82f, 1.f);
  c->DistantRain = XMFLOAT4(rain.Amount, rain.Strength, 0.f, 0.f);
  c->OpenSkyGrid = XMFLOAT4(skyX, skyZ, skySize > 0.f ? skySize : 1.f, skyGrid);
  // The query for Faith's steps this frame (see QuerySide)
  bool query = false;
  int queryX = 0, queryZ = 0;
  // The fog this frame (see FogCoefficients)
  weather::FogParams fog = weather::GetFog();
  XMFLOAT4 fogC(0.f, 0.f, 0.f, 0.f);
  bool fogOn = FogCoefficients(fog, t[13], fogC);
  // The bolts' segments on the frame: each end put through the camera's
  // view and projection (a part behind the camera cut off), its core as wide
  // as it is at that distance (at least a pixel); each bolt's segments one
  // after another, with the box round them its glow can reach
  int segments = 0, bolts = 0;
  float boxMin[2] = {}, boxMax[2] = {}, widest = 0.f;
  int boltFirst = 0, boltNow = -1, boltFirstIndex = 0;
  auto closeBolt = [&]()
  {
    if (boltNow < 0 || segments == boltFirst || bolts >= MostBolts)
      return;
    float pad = 40.f * widest + 4.f;
    c->BoltBox[bolts] = XMFLOAT4(boxMin[0] - pad, boxMin[1] - pad, boxMax[0] + pad, boxMax[1] + pad);
    // How much of it the fog lets through, at the middle of its first segment
    // (its top, in the clouds, and its foot are much the same distance away)
    float through = 1.f;
    if (fogOn)
    {
      float const* q = boltA[boltFirstIndex];
      through = FogAt(fogC, fog.MinTransmittance, q[0] - t[12], q[1] - t[13], q[2] - t[14]);
    }
    c->BoltRange[bolts] = XMFLOAT4((float)boltFirst, (float)(segments - boltFirst), boltDistance[boltNow], through);
    ++bolts;
  };
  for (int i = 0; bolt && i < boltCount; ++i)
  {
    if (boltOf[i] != boltNow)
    {
      closeBolt();
      if (bolts >= MostBolts)
        break;
      boltNow = boltOf[i];
      boltFirst = segments;
      boltFirstIndex = i;
      widest = 0.f;
      boxMin[0] = boxMin[1] = 1e9f;
      boxMax[0] = boxMax[1] = -1e9f;
    }
    XMVECTOR ends[2];
    float w[2];
    for (int e = 0; e < 2; ++e)
    {
      float const* q = e ? boltB[i] : boltA[i];
      ends[e] = XMVector4Transform(XMVectorSet(q[0] - t[12], q[1] - t[13], q[2] - t[14], 1.f), viewProjection);
      w[e] = XMVectorGetW(ends[e]);
    }
    const float nearW = 1.f;
    if (w[0] < nearW && w[1] < nearW)
      continue;
    if (w[0] < nearW || w[1] < nearW)
    {
      int behind = w[0] < nearW ? 0 : 1;
      float f = (nearW - w[behind]) / (w[1 - behind] - w[behind]);
      ends[behind] = XMVectorLerp(ends[behind], ends[1 - behind], f);
      w[behind] = nearW;
    }
    float screen[4];
    for (int e = 0; e < 2; ++e)
    {
      screen[e * 2] = (XMVectorGetX(ends[e]) / w[e] * 0.5f + 0.5f) * (float)back.Width;
      screen[e * 2 + 1] = (0.5f - XMVectorGetY(ends[e]) / w[e] * 0.5f) * (float)back.Height;
    }
    float halfWidth = (std::max)(boltWidth[i] * p[0] * 0.5f * (float)back.Width / (0.5f * (w[0] + w[1])), 0.9f);
    c->BoltSegment[segments] = XMFLOAT4(screen[0], screen[1], screen[2], screen[3]);
    c->BoltWidth[segments] = XMFLOAT4(halfWidth, boltBright[i], 0.f, 0.f);
    for (int e = 0; e < 2; ++e)
    {
      boxMin[0] = (std::min)(boxMin[0], screen[e * 2]);
      boxMin[1] = (std::min)(boxMin[1], screen[e * 2 + 1]);
      boxMax[0] = (std::max)(boxMax[0], screen[e * 2]);
      boxMax[1] = (std::max)(boxMax[1], screen[e * 2 + 1]);
    }
    widest = (std::max)(widest, halfWidth);
    ++segments;
  }
  closeBolt();
  c->Bolt = XMFLOAT4(bolts ? 1.f : 0.f, 0.f, (float)segments, (float)bolts);
  c->ShelterGrid = XMFLOAT4(shelterX, shelterZ, shelterSize > 0.f ? shelterSize : 1.f, shelterGiven ? (float)shelterCount : 0.f);
  c->Fog = fogC;
  // The drips off the roof edges, and whether it rains (for the running water)
  MakeDrips(c, g_ShelterCells, g_ShelterEdges, shelterGiven ? shelterCount : 0, shelterSize, shelterX, shelterZ, t, viewProjection, p,
    (float)back.Width, (float)back.Height, rain.Strength, wet && rain.Strength > 0.01f);
  // Whether the depth copy's format can be gathered (Gather: 2 x 2 texels at
  // once, WetLook.hlsl's FarthestThrough), asked of the device once a format
  // (and whether this pass draws the ground's nodes itself, see c->Options.w)
  bool nodesHere = false;
  {
    static DXGI_FORMAT s_format = DXGI_FORMAT_UNKNOWN;
    static bool s_gather = false;
    D3D11_SHADER_RESOURCE_VIEW_DESC depthDesc{};
    if (frame.View)
      frame.View->GetDesc(&depthDesc);
    if (depthDesc.Format != s_format)
    {
      s_format = depthDesc.Format;
      UINT support = 0;
      s_gather = s_format != DXGI_FORMAT_UNKNOWN && SUCCEEDED(pDevice->CheckFormatSupport(s_format, &support)) &&
        (support & D3D11_FORMAT_SUPPORT_SHADER_GATHER) != 0;
      util::log::Write("Wet surfaces: the depth copy (format %d) %s gathered", (int)s_format, s_gather ? "can be" : "can't be");
    }
    // (y: the wet ground written into the game's own G-buffer this frame,
    // GbufferWet.h: the pass leaves its ground's darkening, sheen and
    // reflection out, WetLook.hlsl's PsMain)
    // (z: its info, where it's open and kept dry, read from it (t10, bound
    // below) and not worked out again)
    // (w: the ground's nodes, where a puddle can lie (WetLook.hlsl's
    // PuddleGround): that pass drew them this frame, or this one does while
    // there are puddles, see nodesHere)
    nodesHere = !gbufferwet::NodesThisFrame(nullptr) && wet && c->Wet.y > 0.001f && gbufferwet::NodesReady(pContext);
    c->Options = XMFLOAT4(s_gather ? 1.f : 0.f, gbufferwet::DrawnThisFrame() ? 1.f : 0.f,
      gbufferwet::InfoThisFrame() ? 1.f : 0.f, gbufferwet::NodesThisFrame(nullptr) || nodesHere ? 1.f : 0.f);
  }
  // The haze in the rain or the snow (see HazeAt), and the sky's light for
  // its colour (see HazeSkyRow; y -1 not known yet)
  bool hazing = false;
  {
    weather::Haze haze = weather::GetHaze();
    hazing = haze.Density > 0.f;
    c->Haze = XMFLOAT4(haze.Density, haze.Snow ? 1.f : 0.f, haze.Drift[0], haze.Drift[1]);
    std::lock_guard<std::mutex> lock(g_Mutex);
    c->HazeView = g_HazeSkyKnown ? XMFLOAT4(haze.Outside, g_HazeSky[0], g_HazeSky[1], g_HazeSky[2]) :
      XMFLOAT4(haze.Outside, -1.f, 0.f, 0.f);
  }
  // The drainpipes' water (as long as it runs, also while it eases out)
  MakePipes(c, t, viewProjection, p, (float)back.Width, (float)back.Height, wet);
  c->FogMisc = XMFLOAT4(0.f, fog.MinTransmittance, fogOn ? 1.f : 0.f, 0.f);
  // What the wet look leaves alone (people, vehicles, trains: DryShapes.cpp),
  // relative to this frame's camera like the rest: per box its middle, then
  // its axes divided by its half sizes (w the half size); the middle's w 1 +
  // how far under its floor their feet are for Faith's or a person's
  // (WetLook.hlsl's KeptDryKind), 0 the rest
  {
    static dry::Box boxes[dry::MostBoxes];
    dry::Near close{};
    // (also for the rain the tools draw, kept out of them: RainAt)
    int dryCount = wet || snowed || rain.Amount > 0.f ? dry::Get(boxes, dry::MostBoxes, close) : 0;
    for (int k = 0; k < dryCount; ++k)
    {
      dry::Box const& b = boxes[k];
      c->DryBox[k * 4] = XMFLOAT4(b.Middle[0] - t[12], b.Middle[1] - t[13], b.Middle[2] - t[14],
        b.Body ? 1.f + b.FeetBelow : b.Floored ? -(1.f + b.FeetBelow) : 0.f);
      for (int i = 0; i < 3; ++i)
      {
        float h = (std::max)(b.Half[i], 0.01f);
        c->DryBox[k * 4 + 1 + i] = XMFLOAT4(b.Axis[i][0] / h, b.Axis[i][1] / h, b.Axis[i][2] / h, h);
      }
    }
    c->DryCount = XMFLOAT4((float)dryCount, 0.f, 0.f, 0.f);
    // and by where they reach (a pixel goes through its cell's only)
    MakeDryGrid(c, dryCount);
    // the game's rain kept out under the roofs (see MostRainBoxes), relative
    // to this frame's camera
    {
      static float mins[MostRainBoxes][3], maxs[MostRainBoxes][3];   // render thread
      int n = raining ? weather::DryBoxes(mins, maxs, MostRainBoxes) : 0;
      for (int i = 0; i < n; ++i)
      {
        c->RainBoxMin[i] = XMFLOAT4(mins[i][0] - t[12], mins[i][1] - t[13], mins[i][2] - t[14], 0.f);
        c->RainBoxMax[i] = XMFLOAT4(maxs[i][0] - t[12], maxs[i][1] - t[13], maxs[i][2] - t[14], 0.f);
      }
      c->RainBoxCount = XMFLOAT4((float)n, 0.f, 0.f, 0.f);
    }
    // The KrugerSec down in the water (see MostRippleBodies; LyingBodies
    // takes snowbody's own lock, so before g_Mutex)
    {
      snowbody::LyingBody lying[MostRippleBodies];
      int found = wet ? snowbody::LyingBodies(lying, MostRippleBodies) : 0;
      int bodies = 0, pieces = 0;
      ULONGLONG tick = GetTickCount64(), playNow = g_PlayMs.load();
      for (int i = 0; i < found; ++i)
      {
        snowbody::LyingBody const& b = lying[i];
        int count = (std::min)(b.Segments, snowbody::LyingSegments);
        if (count <= 0 || !b.DownAt)
          continue;
        float middle[3] = {};
        for (int k = 0; k < count; ++k)
          for (int j = 0; j < 3; ++j)
            middle[j] += 0.5f * (b.A[k][j] + b.B[k][j]) / count;
        float out = 0.f;
        for (int k = 0; k < count; ++k)
        {
          float ax = b.A[k][0] - middle[0], az = b.A[k][2] - middle[2], bx = b.B[k][0] - middle[0], bz = b.B[k][2] - middle[2];
          out = (std::max)(out, (std::max)(sqrtf(ax * ax + az * az), sqrtf(bx * bx + bz * bz)) + b.Radius[k]);
        }
        for (int k = 0; k < count; ++k)
        {
          c->BodySegA[pieces + k] = XMFLOAT4(b.A[k][0] - t[12], b.A[k][1] - t[13], b.A[k][2] - t[14], b.Radius[k]);
          c->BodySegB[pieces + k] = XMFLOAT4(b.B[k][0] - t[12], b.B[k][1] - t[13], b.B[k][2] - t[14], 0.f);
        }
        // (on the rings' clock, see RippleClock)
        RippleClock* clock = nullptr;
        for (RippleClock& k : g_RippleClocks)
          if (k.Owner == b.Owner)
            clock = &k;
        if (!clock)
        {
          clock = &g_RippleClocks[0];
          for (RippleClock& k : g_RippleClocks)
            if (k.Seen < clock->Seen)
              clock = &k;
          *clock = RippleClock{ b.Owner };
        }
        clock->Seen = tick;
        if (clock->DownAt != b.DownAt)
        {
          clock->DownAt = b.DownAt;
          clock->PlayDown = playNow - (std::min)(tick - (std::min)(b.DownAt, tick), playNow);
        }
        if (clock->KnockAt != b.KnockAt)
        {
          clock->KnockAt = b.KnockAt;
          clock->PlayKnock = b.KnockAt ? playNow - (std::min)(tick - (std::min)(b.KnockAt, tick), playNow) : 0;
        }
        float down = (float)(playNow - (std::min)(clock->PlayDown, playNow)) / 1000.f;
        float knock = clock->KnockAt ? (float)(playNow - (std::min)(clock->PlayKnock, playNow)) / 1000.f : 1e9f;
        c->BodyRipple[bodies] = XMFLOAT4((float)pieces, (float)count, down, knock);
        c->BodyRippleAt[bodies++] = XMFLOAT4(middle[0] - t[12], middle[1] - t[13], middle[2] - t[14], out + RippleBodyReach);
        pieces += count;
      }
      c->BodyRippleCount = XMFLOAT4((float)bodies, 0.f, 0.f, 0.f);
      if (bodies && (!g_RippleBodiesLoggedTick || tick - g_RippleBodiesLoggedTick > 30000))
      {
        g_RippleBodiesLoggedTick = tick;
        util::log::Write("Wet surfaces: %d KrugerSec down in the rain's water: waves out from their bodies' outline (%d pieces)",
          bodies, pieces);
      }
    }
    // Faith's steps: their rings and her wet prints (see MostStepRings)
    {
      int rings = 0, prints = 0;
      ULONGLONG now = g_PlayMs.load();
      std::lock_guard<std::mutex> lock(g_Mutex);
      for (StepMark const& r : g_StepRings)
      {
        float age = r.Tick ? (now - r.Tick) / 1000.f : 1e9f;
        float size = (std::max)(r.Size, 1.f);
        float life = StepRingSeconds * (0.7f + 0.3f * size);
        if (wet && age < life)
        {
          c->StepRing[rings] = XMFLOAT4(r.At[0] - t[12], r.At[1] - t[13], r.At[2] - t[14], age);
          c->StepRingSize[rings++] = XMFLOAT4(size, life, 0.f, 0.f);
        }
      }
      // and hers on the game's own water (see MostGameWaterRings), z 1
      bool onGameWater = false;
      for (StepMark const& r : g_GameWaterRings)
      {
        float age = r.Tick ? (now - r.Tick) / 1000.f : 1e9f;
        float size = (std::max)(r.Size, 1.f);
        float life = StepRingSeconds * (0.7f + 0.3f * size);
        if (wet && age < life && rings < MostStepRings)
        {
          c->StepRing[rings] = XMFLOAT4(r.At[0] - t[12], r.At[1] - t[13], r.At[2] - t[14], age);
          c->StepRingSize[rings++] = XMFLOAT4(size, life, 1.f, 0.f);
          onGameWater = true;
        }
      }
      // and hers on the landing pads' tarps (see MostPadRings), z 2 (drawn
      // without a puddle too)
      for (StepMark const& r : g_PadRings)
      {
        float age = r.Tick ? (now - r.Tick) / 1000.f : 1e9f;
        float size = (std::max)(r.Size, 1.f);
        float life = StepRingSeconds * (0.7f + 0.3f * size);
        if (wet && age < life && rings < MostStepRings)
        {
          c->StepRing[rings] = XMFLOAT4(r.At[0] - t[12], r.At[1] - t[13], r.At[2] - t[14], age);
          c->StepRingSize[rings++] = XMFLOAT4(size, life, 2.f, 0.f);
          onGameWater = true;
        }
      }
      // (oldest first: SlideTrail joins each point to the next)
      int slides = 0;
      for (int i = 0; i < MostSlideRings; ++i)
      {
        StepMark const& r = g_SlideRings[(g_SlideRingNext + i) % MostSlideRings];
        float age = r.Tick ? (now - r.Tick) / 1000.f : 1e9f;
        if (wet && age < SlideTrailSeconds)
          c->SlideRing[slides++] = XMFLOAT4(r.At[0] - t[12], r.At[1] - t[13], r.At[2] - t[14], age);
      }
      c->SlideCount = XMFLOAT4((float)slides, SlideRingSeconds, 0.f, SlideTrailSeconds);
      // (oldest first: the PrintFadeSlots oldest fading out before they're
      // made over; which kind in w's range, see WetLook.hlsl's PrintsAt)
      PlaceBox printBox;
      for (int i = 0; i < MostPrints; ++i)
      {
        StepMark const& p = g_Prints[(g_PrintNext + i) % MostPrints];
        float age = p.Tick ? (now - p.Tick) / 1000.f : 1e9f;
        bool pressed = p.Kind == PrintPressed || p.Kind == PrintGravel;
        float left = p.Wet * (1.f - age / (pressed ? PressedPrintSeconds : PrintSeconds)) *
          (std::min)(1.f, (float)i / (float)PrintFadeSlots);
        if (wet && left > 0.01f)
        {
          float w = p.Kind == PrintPressed ? -left : p.Kind == PrintGravel ? -2.f - left : left;
          bool wall = p.Kind == PrintWall;
          c->Print[prints] = XMFLOAT4(p.At[0] - t[12], p.At[1] - t[13], p.At[2] - t[14], w);
          // (PrintsAt's sphere round it, 0.245 m)
          printBox.Add(p.At[0] - t[12], p.At[1] - t[13], p.At[2] - t[14], 0.3f, 0.3f);
          c->PrintDir[prints++] = XMFLOAT4(p.Dir[0], p.Dir[1], wall ? 0.f : p.Out[0], wall ? 0.f : p.Out[1]);
        }
      }
      c->StepCounts = XMFLOAT4((float)rings, (float)prints, StepRingSeconds, onGameWater ? 1.f : 0.f);
      printBox.Put(c->PrintBounds);
      // other people's and the cars' rings (see MostOtherRings)
      int others = 0;
      for (StepMark const& r : g_OtherRings)
      {
        float age = r.Tick ? (now - r.Tick) / 1000.f : 1e9f;
        float size = (std::max)(r.Size, 1.f);
        float life = StepRingSeconds * (0.7f + 0.3f * size);
        if (wet && age < life)
        {
          c->OtherRing[others] = XMFLOAT4(r.At[0] - t[12], r.At[1] - t[13], r.At[2] - t[14], age);
          c->OtherRingSize[others++] = XMFLOAT4(size, life, 0.f, 0.f);
        }
      }
      // a little splash at each young ring, hers and the others' (see
      // MostSplashes), and her own the game plays (see MostHerSplashes; hers:
      // not drawn, let through): where it is on the frame, nearest first
      struct SplashFound { float Screen[2], Depth, Age, Size, Seed, Hers; };
      SplashFound splashes[MostStepRings + MostOtherRings + MostHerSplashes];
      int found = 0;
      auto splashOf = [&](StepMark const& r, bool hers)
      {
        if (!wet || !r.Tick)
          return;
        float size = (std::max)(r.Size, 0.4f);
        float age = (now - r.Tick) / 1000.f;
        if (age > (hers ? HerSplashSeconds : 0.35f) * sqrtf(size))
          return;
        XMVECTOR clip = XMVector4Transform(XMVectorSet(r.At[0] - t[12], r.At[1] - t[13], r.At[2] - t[14], 1.f), viewProjection);
        float w = XMVectorGetW(clip);
        if (w < 0.3f)
          return;
        SplashFound& s = splashes[found++];
        s.Screen[0] = (XMVectorGetX(clip) / w * 0.5f + 0.5f) * (float)back.Width;
        s.Screen[1] = (0.5f - XMVectorGetY(clip) / w * 0.5f) * (float)back.Height;
        s.Depth = w;
        s.Age = age;
        s.Size = size;
        s.Seed = (float)((unsigned int)(r.Tick * 2654435761ULL) % 997u) * 0.1f;
        s.Hers = hers ? 1.f : 0.f;
      };
      // (none of the tools' for her step the game splashes for itself)
      auto played = [&](StepMark const& r)
      {
        for (StepMark const& h : g_HerSplashes)
        {
          float dx = h.At[0] - r.At[0], dz = h.At[2] - r.At[2];
          if (h.Tick && (h.Tick > r.Tick ? h.Tick - r.Tick : r.Tick - h.Tick) <= HerSplashSameMs &&
            dx * dx + dz * dz < HerSplashSameMetres * HerSplashSameMetres && fabsf(h.At[1] - r.At[1]) < 0.5f)
            return true;
        }
        return false;
      };
      for (StepMark const& r : g_StepRings)
        if (!played(r))
          splashOf(r, false);
      for (StepMark const& r : g_OtherRings)
        splashOf(r, false);
      for (StepMark const& h : g_HerSplashes)
        splashOf(h, true);
      std::sort(splashes, splashes + found, [](SplashFound const& a, SplashFound const& b) { return a.Depth < b.Depth; });
      int shown = (std::min)(found, MostSplashes);
      for (int i = 0; i < shown; ++i)
      {
        c->Splash[i] = XMFLOAT4(splashes[i].Screen[0], splashes[i].Screen[1], splashes[i].Depth, splashes[i].Age);
        c->SplashInfo[i] = XMFLOAT4(splashes[i].Size, splashes[i].Seed, splashes[i].Hers, 0.f);
      }
      // (how much the camera looks down: the splash's spread over the ground
      // shows that much up and down the frame)
      c->OtherCounts = XMFLOAT4((float)others, (float)shown, p[5] * 0.5f * (float)back.Height, fabsf(t[9]));
      // (SplashesAt's box round each of the tools', as far as its drops can
      // go; HerSplashAt's round each of hers, as far as her splash's region
      // reaches: 0.6 across, 1 up the frame, the camera's look down x 0.6 +
      // 0.25 down it, x its pixels a metre x the root of its size)
      {
        float perMetre1 = p[5] * 0.5f * (float)back.Height, down = fabsf(t[9]);
        FrameBox drops, hers;
        g_Falling.List(3);
        for (int i = 0; i < shown; ++i)
        {
          SplashFound const& s = splashes[i];
          float root = sqrtf(s.Size), perMetre = perMetre1 / (std::max)(s.Depth, 0.3f);
          if (s.Hers > 0.5f)
          {
            float k = perMetre * root;
            hers.Add(s.Screen[0] - 0.6f * k - 2.f, s.Screen[1] - k - 2.f, s.Screen[0] + 0.6f * k + 2.f,
              s.Screen[1] + (0.6f * down + 0.25f) * k + 2.f);
            g_Falling.Add(3, i, s.Screen[0] - 0.6f * k - 2.f, s.Screen[1] - k - 2.f, s.Screen[0] + 0.6f * k + 2.f,
              s.Screen[1] + (0.6f * down + 0.25f) * k + 2.f);
          }
          else
          {
            float dropPx = (std::max)(0.7f, 0.0035f * root * perMetre);
            float reach = 0.4f * root * perMetre + dropPx + 2.f + 2.f;
            drops.Add(s.Screen[0] - reach, s.Screen[1] - reach, s.Screen[0] + reach, s.Screen[1] + reach);
            g_Falling.Add(3, i, s.Screen[0] - reach, s.Screen[1] - reach, s.Screen[0] + reach, s.Screen[1] + reach);
          }
        }
        c->SplashBounds = drops.Get();
        c->HerSplashBounds = hers.Get();
      }
      // her wet handprints (see MostHandPrints)
      int hands = 0;
      // (oldest first, the HandPrintFadeSlots oldest fading out, as her
      // footprints)
      for (int i = 0; i < MostHandPrints; ++i)
      {
        HandMark const& p = g_HandPrints[(g_HandPrintNext + i) % MostHandPrints];
        float age = p.Tick ? (now - p.Tick) / 1000.f : 1e9f;
        float left = p.Wet * (1.f - age / HandPrintSeconds) * (std::min)(1.f, (float)i / (float)HandPrintFadeSlots);
        if (wet && left > 0.01f)
        {
          // (her own hand's, HandStampAt: a wet one's w below 0)
          if (handStamped(p.Stamp, -left))
            continue;
          c->HandPrint[hands] = XMFLOAT4(p.At[0] - t[12], p.At[1] - t[13], p.At[2] - t[14], left);
          c->HandFinger[hands] = XMFLOAT4(p.Finger[0], p.Finger[1], p.Finger[2], 0.f);
          c->HandThumb[hands++] = XMFLOAT4(p.Thumb[0], p.Thumb[1], p.Thumb[2], 0.f);
        }
      }
      c->HandCount = XMFLOAT4((float)hands, 0.f, 0.f, 0.f);
    }
    // The query round Faith's feet (see QuerySide): while puddles are drawn
    // and she's known
    float feet[3];
    query = g_QueryMade && wet && puddles > 0.001f && dry::FaithFeet(feet);
    if (query)
    {
      queryX = (int)floorf(feet[0] / QueryCell) - QuerySide / 2;
      queryZ = (int)floorf(feet[2] / QueryCell) - QuerySide / 2;
      c->QueryGrid = XMFLOAT4(queryX * QueryCell - t[12], queryZ * QueryCell - t[14], QueryCell, feet[1] - t[13]);
    }
    else
      c->QueryGrid = XMFLOAT4(0.f, 0.f, 0.f, 0.f);
    c->DryNear = XMFLOAT4(DryNearFrom, DryNearTo, close.Floor - t[13], (wet || snowed) && close.On ? 1.f : 0.f);
  }
  // The snow (see weather::GetSnow and WetLook.hlsl's SnowAt)
  c->Snow = XMFLOAT4(snow.Amount, snow.Density, snow.Size, snow.Seconds);
  c->SnowDrift = XMFLOAT4(snow.Drift[0], snow.Drift[1], snow.Cover, snow.CoverUpTo > 1.f ? snow.CoverUpTo : 1.f);
  // (y: the flakes' streaks' longest, pixels: a blizzard's wind drives them
  // across the frame)
  c->SnowMisc = XMFLOAT4(snow.KeepOut ? 1.f : 0.f, snow.Blizzard ? 90.f : 40.f, 0.f, 0.f);
  // The flakes, and the frame before's camera for their motion blur (known
  // when it was drawn within 0.2 s and the camera moved less than 10 m: not
  // after a cut, a jump or a pause)
  float moved[3] = { t[12] - g_PrevCamera[0], t[13] - g_PrevCamera[1], t[14] - g_PrevCamera[2] };
  bool previous = g_PrevTick && GetTickCount64() - g_PrevTick < 200 &&
    moved[0] * moved[0] + moved[1] * moved[1] + moved[2] * moved[2] < 100.f;
  c->SnowBoxes = XMFLOAT4((float)SnowNearFlakes, (float)SnowFarFlakes, SnowNearBox, SnowFarBox);
  c->SnowTiming = XMFLOAT4(snow.Period > 1.f ? snow.Period : 1.f, previous ? 1.f : 0.f, 0.f, 0.f);
  c->SnowPrevious = XMFLOAT4(moved[0], moved[1], moved[2], 0.f);
  // The wind's speed and the frame's time (since the last drawn, at most 0.1 s)
  float frameSeconds = g_PrevMs > 0.0 ? (std::min)((float)((TimeNow() - g_PrevMs) / 1000.0), 0.1f) : 1.f / 60.f;
  // (no wind blur on the flakes while the game is paused: they stand still)
  c->SnowWind = XMFLOAT4(snow.Wind[0], snow.Wind[1], gamestate::Paused() ? 0.f : frameSeconds, snow.Turbulence);
  c->PrevViewProjection = g_PrevViewProjection;
  // The game's interiors near the camera (WetLook.hlsl's InteriorAt),
  // relative to this frame's camera: per box its middle, then its axes
  // divided by its half sizes (w the half size); per prism its points, its
  // floor and top
  {
    static weather::InteriorBox iboxes[MostInteriorBoxes];
    static weather::InteriorPrism iprisms[MostInteriorPrisms];
    int boxCount = 0, prismCount = 0;
    weather::GetInteriors(t + 12, InteriorReach, iboxes, MostInteriorBoxes, boxCount, iprisms, MostInteriorPrisms, prismCount);
    for (int k = 0; k < boxCount; ++k)
    {
      weather::InteriorBox const& b = iboxes[k];
      c->InteriorBox[k * 4] = XMFLOAT4(b.Middle[0] - t[12], b.Middle[1] - t[13], b.Middle[2] - t[14], b.Generic ? 1.f : 0.f);
      for (int i = 0; i < 3; ++i)
      {
        float h = (std::max)(b.Half[i], 0.01f);
        c->InteriorBox[k * 4 + 1 + i] = XMFLOAT4(b.Axis[i][0] / h, b.Axis[i][1] / h, b.Axis[i][2] / h, h);
      }
    }
    for (int p = 0; p < prismCount; ++p)
    {
      weather::InteriorPrism const& q = iprisms[p];
      int n = (std::min)(q.Count, 8);
      c->InteriorPrism[p * 9] = XMFLOAT4((float)n, q.Floor - t[13], q.Floor + q.Height - t[13], q.Generic ? 1.f : 0.f);
      for (int i = 0; i < 8; ++i)
        c->InteriorPrism[p * 9 + 1 + i] = i < n ? XMFLOAT4(q.Points[i][0] - t[12], q.Points[i][1] - t[14], 0.f, 0.f) :
          XMFLOAT4(0.f, 0.f, 0.f, 0.f);
    }
    c->InteriorCount = XMFLOAT4((float)boxCount, (float)prismCount, 0.f, 0.f);
    // and by where they reach (a point goes through its cell's only)
    MakeInteriorGrid(c, boxCount);
  }
  // The ice over the water: how far it has formed (its pieces are a vertex
  // buffer of their own, see g_IceVs)
  c->IceCount = XMFLOAT4(0.f, weather::IceAmount(), 0.f, 0.f);
  // Her shoes (WetLook.hlsl's FeetKeptAt), over where her feet are now
  // (dry::FaithFeet before g_Mutex is taken: dry's own lock is never taken
  // under it)
  {
    float feet[3];
    bool known = dry::FaithFeet(feet);
    c->SnowFeetKeep = known ? XMFLOAT4(feet[0] - t[12], feet[1] - t[13], feet[2] - t[14], 1.f) : XMFLOAT4(0.f, 0.f, 0.f, 0.f);
    // (and by her own foot bones, WetLook.hlsl's ShoesKeptAt: wherever her
    // feet are, her legs out ahead too; snowbody's lock, never under g_Mutex
    // here)
    float shoes[4][3];
    bool shoesKnown = snowed && snowbody::FaithShoes(shoes);
    for (int k = 0; k < 4; ++k)
      c->SnowShoe[k] = shoesKnown ? XMFLOAT4(shoes[k][0] - t[12], shoes[k][1] - t[13], shoes[k][2] - t[14], 1.f) :
        XMFLOAT4(0.f, 0.f, 0.f, 0.f);
  }
  // The snow pressed in by bodies (SnowBodies.cpp; WetLook.hlsl's
  // SnowStampsAt), filled as her prints are (before g_Mutex: its own lock is
  // never taken under it)
  ID3D11ShaderResourceView* stampView = nullptr;
  {
    ULONGLONG play = g_PlayMs.load();
    float seconds = g_SnowStampsPlayMs ? (play - g_SnowStampsPlayMs) / 1000.f : 0.f;
    g_SnowStampsPlayMs = play;
    // (in the snow, or the wet look's water: see snowbody::Mode)
    int mode = snow.Chosen ? (snowed ? snowbody::ModeSnow : snowbody::ModeNone) : wet ? snowbody::ModeWater :
      snowbody::ModeNone;
    float fill = mode == snowbody::ModeWater ? seconds / WaterStampSeconds :
      snowing ? seconds * snow.Amount * snow.Intensity / SnowPrintFillSeconds : 0.f;
    snowbody::Stamp stamps[snowbody::MostStamps];
    int count = 0;
    stampView = snowbody::Take(pContext, fill, mode, stamps, count);
    if (!stampView)
      count = 0;
    for (int k = 0; k < count; ++k)
    {
      snowbody::Stamp const& s = stamps[k];
      c->SnowStamp[k] = XMFLOAT4(s.Corner[0] - t[12], s.Corner[1] - t[13], s.Corner[2] - t[14], s.Left);
      c->SnowStampAxis[k] = XMFLOAT4(s.Along[0], s.Along[1], s.Rise, (float)s.Slice);
    }
    c->SnowStampCount = XMFLOAT4((float)count, 0.f, (float)mode, 0.f);
  }
  // Faith's prints in the snow (see MostSnowPrints), filled by the falling
  // snow on the play clock (not while the game is paused), relative to this
  // frame's camera
  {
    ULONGLONG play = g_PlayMs.load();
    float seconds = g_SnowPrintsPlayMs ? (play - g_SnowPrintsPlayMs) / 1000.f : 0.f;
    g_SnowPrintsPlayMs = play;
    float fill = snowing ? seconds * snow.Amount * snow.Intensity / SnowPrintFillSeconds : 0.f;
    int prints = 0;
    std::lock_guard<std::mutex> lock(g_Mutex);
    for (SnowPrintMark& p : g_SnowPrints)
    {
      if (p.Left <= 0.f)
        continue;
      p.Left = snowed ? p.Left - fill : 0.f;
      if (p.Left > 0.01f)
      {
        // (her own hand's, HandStampAt)
        if (p.Hand && handStamped(p.Stamp, p.Left))
          continue;
        // (a handprint's w below 0)
        c->SnowPrint[prints] = XMFLOAT4(p.At[0] - t[12], p.At[1] - t[13], p.At[2] - t[14], p.Hand ? -p.Left : p.Left);
        c->SnowPrintDir[prints++] = XMFLOAT4(p.Dir[0], p.Dir[1], p.Out[0], p.Out[1]);
      }
    }
    // The KrugerSec's boot prints, filled the same way
    int others = 0;
    for (OtherSnowPrintMark& o : g_OtherSnowPrints)
    {
      if (o.Left <= 0.f)
        continue;
      o.Left = snowed ? o.Left - fill : 0.f;
      if (o.Left > 0.01f)
      {
        c->OtherSnowPrint[others] = XMFLOAT4(o.At[0] - t[12], o.At[1] - t[13], o.At[2] - t[14], o.Left);
        c->OtherSnowPrintDir[others++] = XMFLOAT4(o.Dir[0], o.Dir[1], o.Out[0], o.Out[1]);
      }
    }
    c->SnowStampCount.y = (float)others;
    c->HandStampCount = XMFLOAT4((float)handStamps, 0.f, 0.f, 0.f);
    c->SnowPile = XMFLOAT4(snowed ? snow.Pile : 0.f, (float)prints,
      (std::min)(SnowPrintDepth + snow.Pile * 0.6f, SnowPrintDeepest), 0.f);
    // Her hands over their last touches (see g_HandKeep): kept for 0.8 s after
    // a touch, gone by 1.5 s (on the play clock: held while the game's paused)
    for (int h = 0; h < 2; ++h)
    {
      float age = g_HandKeepMs[h] && play >= g_HandKeepMs[h] ? (float)(play - g_HandKeepMs[h]) / 1000.f : 1e9f;
      float kept = age < 0.8f ? 1.f : age < 1.5f ? 1.f - (age - 0.8f) / 0.7f : 0.f;
      c->SnowHandKeep[h] = XMFLOAT4(g_HandKeep[h][0] - t[12], g_HandKeep[h][1] - t[13], g_HandKeep[h][2] - t[14], kept);
    }
    // Her slides' trails, filled the same way
    // (each slide's newest stretch is its last: her legs, w below 0)
    int trails = 0;
    for (SnowTrailMark& s : g_SnowTrails)
    {
      if (s.Left <= 0.f)
        continue;
      s.Left = snowed ? s.Left - fill : 0.f;
      if (s.Left > 0.01f)
      {
        bool last = true;
        for (SnowTrailMark const& o : g_SnowTrails)
          last &= !(o.Left > 0.01f && o.Trail == s.Trail && o.Seq > s.Seq);
        c->SnowTrail[trails] = XMFLOAT4(s.Start[0] - t[12], s.Start[1] - t[13], s.Start[2] - t[14], s.Left);
        c->SnowTrailEnd[trails++] = XMFLOAT4(s.End[0] - t[12], s.End[1] - t[13], s.End[2] - t[14],
          last ? -SnowTrailHalfWidth : SnowTrailHalfWidth);
      }
    }
    c->SnowTrailCount = XMFLOAT4((float)trails, 0.f, 0.f, 0.f);
    // Her rolls, filled the same way: their middle between the shoulder's
    // mark and the hip's; the way x the length between them, and the
    // shoulder's side (her right: not checked against her roll's animation)
    int rolls = 0;
    for (SnowRollMark& r : g_SnowRolls)
    {
      if (r.Left <= 0.f)
        continue;
      r.Left = snowed ? r.Left - fill : 0.f;
      if (r.Left > 0.01f)
      {
        float right[2];
        SnowRight(r.Way, right);
        float on = SnowRollShoulderOn + SnowRollLength * 0.5f;
        c->SnowRoll[rolls] = XMFLOAT4(r.Start[0] + r.Way[0] * on - t[12], r.Start[1] - t[13], r.Start[2] + r.Way[1] * on - t[14],
          r.Left);
        c->SnowRollWay[rolls++] = XMFLOAT4(r.Way[0] * SnowRollLength, r.Way[1] * SnowRollLength, right[0], right[1]);
      }
    }
    c->SnowRollCount = XMFLOAT4((float)rolls, 0.f, 0.f, 0.f);
  }
  // Whether anything of the rain's is drawn this frame (see g_SnowGroundPs):
  // the pass's own switches for those parts, read back as just written, so
  // the snow's copy is drawn only where it gives what the whole one would
  bool rainPartsOn = c->Rain.z > 0.5f || c->DripCount.x > 0.5f || c->PipeCount.x > 0.5f || c->OtherCounts.y > 0.5f ||
    c->DistantRain.x > 0.f;
  // (the falling water's tiles, every list made by now: FallingTiles)
  g_Falling.Put(c);
  // (what of the snow's pressed lists and the rain's kept-out boxes can reach a point, as they're written now)
  MakeListBounds(c);
  memcpy(mapped.pData, &g_LastConstants, sizeof(Constants));
  // (kept for the next frame's wet ground in the game's G-buffer, with the
  // views the pass has, see wetlook::TakeGbufferPass)
  g_LastViews[0] = skyGiven ? g_OpenSkyView.Get() : nullptr;
  g_LastViews[1] = shelterGiven ? g_ShelterView.Get() : nullptr;
  g_LastViews[2] = stampView;
  g_LastViews[3] = haveMarks ? marks.View : nullptr;
  g_LastFilled = true;
  pContext->Unmap(g_Constants.Get(), 0);

  g_Backup.Save(pContext);
  // The ground's nodes, when the wet ground's pass in the G-buffer didn't
  // draw them this frame (see c->Options.w): with these constants and the
  // depth copy, round this camera (the pass's own state set again below; b4
  // the game's put back after)
  ID3D11Buffer* gameB4 = nullptr;
  pContext->PSGetConstantBuffers(4, 1, &gameB4);
  if (nodesHere)
  {
    float camera[3] = { t[12], t[13], t[14] };
    gbufferwet::DrawNodes(pContext, g_Constants.Get(), frame.View, camera);
  }
  D3D11_VIEWPORT viewport{ 0.f, 0.f, (float)back.Width, (float)back.Height, 0.f, 1.f };
  ID3D11Buffer* noBuffer = nullptr;
  UINT zero = 0;
  const float blendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
  // (t4: the snow pressed in by bodies, see stampView)
  // (t5: her handprints from her own hand, see handView)
  // (t6 - t8 and b1: the game's G-buffer and view constants, see gbuffer;
  // the game's own b1 put back after)
  // (t9 and s1: the ground's marks, see marks; the game's own s1 put back after)
  ID3D11ShaderResourceView* views[10] = { g_SceneCopyView.Get(), frame.View, skyGiven ? g_OpenSkyView.Get() : nullptr,
    shelterGiven ? g_ShelterView.Get() : nullptr, stampView, handView, haveGbuffer ? gbuffer.Normals : nullptr,
    haveGbuffer ? gbuffer.Colour : nullptr, haveGbuffer ? gbuffer.Material : nullptr, haveMarks ? marks.View : nullptr };
  ID3D11Buffer* gameB1 = nullptr;
  pContext->PSGetConstantBuffers(1, 1, &gameB1);
  ID3D11SamplerState* gameS1 = nullptr;
  pContext->PSGetSamplers(1, 1, &gameS1);
  pContext->OMSetRenderTargets(1, pTarget.GetAddressOf(), nullptr);
  pContext->RSSetViewports(1, &viewport);
  pContext->RSSetState(g_Rasterizer.Get());
  pContext->OMSetBlendState(g_Blend.Get(), blendFactor, 0xFFFFFFFF);
  pContext->OMSetDepthStencilState(g_Depth.Get(), 0);
  pContext->IASetInputLayout(nullptr);
  pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  pContext->IASetVertexBuffers(0, 1, &noBuffer, &zero, &zero);
  pContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_R16_UINT, 0);
  pContext->VSSetShader(g_Vs.Get(), nullptr, 0);
  pContext->GSSetShader(nullptr, nullptr, 0);
  pContext->HSSetShader(nullptr, nullptr, 0);
  pContext->DSSetShader(nullptr, nullptr, 0);
  // (its copy without the snow on the ground while none lies, see g_RainPs;
  // without the rain's parts while snow lies and none of them is on, see
  // g_SnowGroundPs)
  ID3D11PixelShader* pass = !snowed ? (g_RainPs ? g_RainPs.Get() : g_Ps.Get()) :
    !rainPartsOn && g_SnowGroundPs ? g_SnowGroundPs.Get() : g_Ps.Get();
  pContext->PSSetShader(pass, nullptr, 0);
  pContext->PSSetShaderResources(0, 10, views);
  // (t10: where it's open and kept dry, from the wet ground's pass in the
  // game's G-buffer this frame, GbufferWet.h; with c->Options.z)
  {
    ID3D11ShaderResourceView* info = gbufferwet::InfoThisFrame();
    pContext->PSSetShaderResources(10, 1, &info);
  }
  // (t11 and b4: the ground's nodes and their lattice, with c->Options.w)
  {
    ID3D11Buffer* nodeConstants = nullptr;
    ID3D11ShaderResourceView* nodes = gbufferwet::NodesThisFrame(&nodeConstants);
    pContext->PSSetShaderResources(11, 1, &nodes);
    pContext->PSSetConstantBuffers(4, 1, &nodeConstants);
  }
  {
    ID3D11SamplerState* samplers[2] = { g_Sampler.Get(), g_WrapSampler.Get() };
    pContext->PSSetSamplers(0, 2, samplers);
  }
  pContext->PSSetConstantBuffers(0, 1, g_Constants.GetAddressOf());
  {
    ID3D11Buffer* gameView = haveGbuffer ? gbuffer.View : nullptr;
    pContext->PSSetConstantBuffers(1, 1, &gameView);
  }
  // (b2: the performance test's breakdown's switches, everything drawn for
  // the frame's own, see BreakdownSkips; the game's own b2 put back after)
  ID3D11Buffer* gameB2 = nullptr;
  pContext->PSGetConstantBuffers(2, 1, &gameB2);
  {
    ID3D11Buffer* everything = SkipBuffer(pDevice, 0);
    pContext->PSSetConstantBuffers(2, 1, &everything);
  }
  GpuTimes* gpuTimes = util::DiagnosticsOn() ? BeginGpuTimes(pDevice, pContext) : nullptr;   // diagnostics only
  NoteDrawing();
  pContext->Draw(3, 0);
  if (gpuTimes)
    pContext->End(gpuTimes->Pass.Get());
  // The ice over the water (see g_IceVs): all of its pieces, over the pass
  // and under the flakes; the full-screen triangle's state put back after
  if (iced && g_IceVs && g_IcePs && g_IceLayout && g_IceVertices && g_SnowBlend)
  {
    UINT stride = sizeof(IceVertexData), offset = 0;
    pContext->OMSetBlendState(g_SnowBlend.Get(), blendFactor, 0xFFFFFFFF);
    pContext->IASetInputLayout(g_IceLayout.Get());
    pContext->IASetVertexBuffers(0, 1, g_IceVertices.GetAddressOf(), &stride, &offset);
    pContext->VSSetShader(g_IceVs.Get(), nullptr, 0);
    pContext->VSSetConstantBuffers(0, 1, g_Constants.GetAddressOf());
    // (b1: the game's viewConstants as copied with its G-buffer, its own
    // camera of the frame, which the ice is drawn with, see IceVsMain; the
    // game's own b1 put back after)
    ID3D11Buffer* gameVsB1 = nullptr;
    pContext->VSGetConstantBuffers(1, 1, &gameVsB1);
    {
      ID3D11Buffer* gameView = haveGbuffer ? gbuffer.View : nullptr;
      pContext->VSSetConstantBuffers(1, 1, &gameView);
    }
    pContext->PSSetShader(g_IcePs.Get(), nullptr, 0);
    pContext->Draw(g_IceVertexCount, 0);
    pContext->VSSetConstantBuffers(1, 1, &gameVsB1);
    if (gameVsB1)
      gameVsB1->Release();
    pContext->IASetInputLayout(nullptr);
    pContext->IASetVertexBuffers(0, 1, &noBuffer, &zero, &zero);
    pContext->VSSetShader(g_Vs.Get(), nullptr, 0);
    pContext->PSSetShader(pass, nullptr, 0);
    pContext->OMSetBlendState(g_Blend.Get(), blendFactor, 0xFFFFFFFF);
  }
  // The snow's flakes over it: six corners each, made in the vertex shader
  // from the flake's number (no vertex buffer); its textures and sampler
  // the vertex shader reads put back after, as they were
  bool flakes = snowing && g_SnowVs && g_SnowPs && g_SnowBlend;
  if (flakes)
  {
    ID3D11ShaderResourceView* vsSaved[4] = {};
    ID3D11SamplerState* vsSampler = nullptr;
    pContext->VSGetShaderResources(0, 4, vsSaved);
    pContext->VSGetSamplers(0, 1, &vsSampler);
    pContext->OMSetBlendState(g_SnowBlend.Get(), blendFactor, 0xFFFFFFFF);
    pContext->VSSetShader(g_SnowVs.Get(), nullptr, 0);
    pContext->VSSetShaderResources(0, 4, views);
    pContext->VSSetSamplers(0, 1, g_Sampler.GetAddressOf());
    pContext->VSSetConstantBuffers(0, 1, g_Constants.GetAddressOf());
    pContext->PSSetShader(g_SnowPs.Get(), nullptr, 0);
    pContext->Draw(6 * (SnowNearFlakes + SnowFarFlakes), 0);
    pContext->VSSetShaderResources(0, 4, vsSaved);
    pContext->VSSetSamplers(0, 1, &vsSampler);
    for (ID3D11ShaderResourceView* v : vsSaved)
      if (v)
        v->Release();
    if (vsSampler)
      vsSampler->Release();
  }
  if (gpuTimes)
  {
    pContext->End(gpuTimes->End.Get());
    pContext->End(gpuTimes->Disjoint.Get());
    gpuTimes->Pending = true;
    gpuTimes->Snow = flakes || iced;
  }
  // The performance test's breakdown, if one is asked for (see BreakdownSkips)
  DrawBreakdown(pDevice, pContext, back, viewport, pass);
  // The query round Faith's feet (see QuerySide), and for the slopes' ends
  // while it rains (see SlopeEndsAcross): the full-screen triangle's shaders
  // and blend again, the same constants and textures
  // (and while snow lies on the ground, haze or not: the snow's light is the
  // sky's by the horizon, g_HazeView.yzw, see HazeSkyAt; 2026-10-27, the snow
  // session, the user: "snow ... just looks like a gray thing on the ground")
  bool queryPass = query || (g_QueryMade && ((wet && raining) || hazing || snowed));
  if (queryPass)
  {
    D3D11_VIEWPORT queryViewport{ 0.f, 0.f, (float)QueryWidth, (float)QueryHeight, 0.f, 1.f };
    pContext->OMSetRenderTargets(1, g_QueryTarget.GetAddressOf(), nullptr);
    pContext->RSSetViewports(1, &queryViewport);
    pContext->OMSetBlendState(g_Blend.Get(), blendFactor, 0xFFFFFFFF);
    pContext->VSSetShader(g_Vs.Get(), nullptr, 0);
    pContext->PSSetShader(g_QueryPs.Get(), nullptr, 0);
    pContext->Draw(3, 0);
  }
  ID3D11ShaderResourceView* none[12] = {};
  pContext->PSSetShaderResources(0, 12, none);
  pContext->PSSetConstantBuffers(1, 1, &gameB1);
  if (gameB1)
    gameB1->Release();
  pContext->PSSetConstantBuffers(2, 1, &gameB2);
  if (gameB2)
    gameB2->Release();
  pContext->PSSetConstantBuffers(4, 1, &gameB4);
  if (gameB4)
    gameB4->Release();
  pContext->PSSetSamplers(1, 1, &gameS1);
  if (gameS1)
    gameS1->Release();
  g_Backup.Restore(pContext);
  if (queryPass)
  {
    int slot = g_QueryNext;
    g_QueryNext = (g_QueryNext + 1) % QueryCopies;
    // What's in that copy from before goes first (if it's come back)
    TakeQuery(pContext, slot);
    g_QueryCopyInfo[slot].Pending = false;
    pContext->CopyResource(g_QueryCopy[slot].Get(), g_Query.Get());
    g_QueryCopyInfo[slot] = { true, query, queryX, queryZ, t[12], t[13], t[14] };
  }
  // The copy made longest ago, if it's come back
  if (g_QueryMade)
    TakeQuery(pContext, g_QueryNext);
  // This frame's camera for the next one's motion blur
  XMStoreFloat4x4(&g_PrevViewProjection, viewProjection);
  memcpy(g_PrevCamera, t + 12, sizeof(g_PrevCamera));
  g_PrevTick = GetTickCount64();
  g_PrevMs = TimeNow();
  if (flakes && !g_LoggedFlakes)
  {
    g_LoggedFlakes = true;
    util::log::Write("Snow: flakes drawn (%d near, %d far at most, boxes %.0f and %.0f m; the clock's period %.0f s)",
      SnowNearFlakes, SnowFarFlakes, SnowNearBox, SnowFarBox, snow.Period);
  }

  {
    std::lock_guard<std::mutex> lock(g_Mutex);
    memcpy(g_DrawnWet.Camera, t + 12, sizeof(g_DrawnWet.Camera));
    g_DrawnWet.Wet = XMFLOAT4(wetness, puddles, s.PuddleSize > 0.1f ? s.PuddleSize : 0.1f, s.Reflections);
    g_DrawnWet.Rain = XMFLOAT4(0.f, s.FadeDistance > 1.f ? s.FadeDistance : 1.f, wet ? 1.f : 0.f, 0.f);
    g_DrawnWet.Fog = fogC;
    g_DrawnWet.FogMin = fog.MinTransmittance;
    g_DrawnWet.FogOn = fogOn;
    for (int i = 0; i < 3; ++i)
    {
      g_DrawnWet.Right[i] = t[i];
      g_DrawnWet.Up[i] = t[4 + i];
      g_DrawnWet.Forward[i] = -t[8 + i];
    }
  }
  g_Drawn = true;
  if (!g_LoggedDrawn)
  {
    g_LoggedDrawn = true;
    util::log::Write("Wet surfaces: drawn (the frame %u x %u, format %d, its copy %u levels; the depth %s; the squares over "
      "it %s)", back.Width, back.Height, (int)back.Format, g_SceneCopyLevels, frame.Reversed ? "reversed" : "not reversed",
      shelterGiven ? "known" : "not known yet");
  }
  static ULONGLONG s_flashLogged = 0;
  if (flash > 0.f && GetTickCount64() - s_flashLogged > 2000)
  {
    s_flashLogged = GetTickCount64();
    util::log::Write("Weather: lightning flash drawn on the frame (brightness %.2f; %d bolt%s: %d of %d segments on the "
      "frame)", flash, bolts, bolts == 1 ? "" : "s", segments, boltCount);
  }
  static int s_rainLogged = -2;   // the last kind said (see skyGrid), -2 none
  int rainKind = skyGrid < 0.f ? -1 : skyGiven ? 1 : 0;
  if (rain.Amount > 0.f && rainKind != s_rainLogged)
  {
    s_rainLogged = rainKind;
    util::log::Write("Weather: rain in the distance drawn (amount %.2f, strength %.2f; %s)", rain.Amount, rain.Strength,
      skyGrid < 0.f ? "rain isn't kept out of buildings: drawn anywhere" : skyGiven ?
      "only over the squares known to be open to the sky" : "none until the squares open to the sky are known");
  }
  // The snow: said when it starts being drawn and when it stops, at most
  // every 5 s
  static int s_snowLogged = 0;   // the last said: bit 1 falling, bit 2 on the ground
  static ULONGLONG s_snowLoggedTick = 0;
  int snowKind = (snowing ? 1 : 0) | (snowed ? 2 : 0);
  if (snowKind != s_snowLogged && GetTickCount64() - s_snowLoggedTick > 5000)
  {
    s_snowLogged = snowKind;
    s_snowLoggedTick = GetTickCount64();
    util::log::Write("Weather: snow drawn: %s, %s (%s; the squares over it %s)", snowing ? "falling" : "not falling",
      snowed ? "on the ground" : "none on the ground", snow.KeepOut ? "kept out of buildings" : "anywhere",
      shelterGiven ? "known" : "not known yet");
  }
}
