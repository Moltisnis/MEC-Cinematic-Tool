#include "Thunder.h"
#include "ThunderSynth.h"
#include "RainSynth.h"
#include "WindSynth.h"
#include "Weather.h"
#include "../Main.h"
#include "../Util/Util.h"
#include "../Util/GameState.h"
#include <Windows.h>
#include <ShlObj.h>
#include <xaudio2.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <deque>
#include <fstream>
#include <string>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

// Thunder made here (the game's, me_sound/world/shared/bigworld/thunder, is
// only loaded with the Release mission's level, levels/sp/sp_maincity/release,
// layer13_release_audio; 2026-10-25, offline): made by ThunderSynth.h, played
// on a thread of the tools' own through XAudio2 (xaudio2_9.dll, Windows' own,
// loaded when first needed), so its volume is the tools', not the game's.
//
// Where it's heard from (2026-10-25 20:xx): from the strike's way, turned with
// the camera (the left and right speakers, and the back ones when Windows' audio
// has them), never fully to one side (thunder comes from all round as it
// echoes); and muffled the more the camera is inside (a low pass down to
// 350 Hz and half the volume, from how many of the squares round the camera
// the weather found under a roof, see weather::GetListener), following it as
// it goes in and out over about a third of a second.
//
// The rain's sound of the tools (2026-10-25 20:xx, see weather::Update): a
// looping stereo rain (RainSynth.h, 8 s, made when first wanted) on its own
// voice, its volume following what the weather asks (fading over a second),
// muffled the same way as the thunder when the camera is inside (down to
// 600 Hz and 40 % of its volume).
//
// The game's own volume (2026-10-25 20:5x): the rain and the thunder are
// played through Windows' audio, so the game's volume slider didn't change
// them (the user: "make it scale with the game's volume"). The game keeps its
// audio options in its profile, Documents\Mirrors Edge Catalyst\settings\
// PROF_SAVE_profile, a text file of "name value" lines it writes when the
// options change (GstAudio.Volume 1.000000, GstAudio.MusicVolume,
// GstAudio.DialogueVolume; GstAudio isn't a settings class in the game's type
// info). Every 2 s the file's time is looked at, and when it changed
// GstAudio.Volume is read again; both sounds are multiplied by it.
//
// The wind's sound while it snows (2026-10-26, see weather::Update): a looping
// stereo wind (WindSynth.h, 24 s, made when first wanted) on its own voice,
// its volume following what the weather asks over WindFollowSeconds (so the
// gusts the weather passes on are heard swelling and dying), a low pass from
// WindDullCutoff (a soft wind) to WindBrightCutoff (a blizzard) by its
// strength, and muffled indoors down to WindInsideCutoff and WindInsideVolume
// of its volume, from what the weather says of the camera (the roof squares
// and the game's interiors).

namespace
{
  const int SampleRate = thunder_synth::SampleRate;
  const size_t MaxPlaying = 12;   // (a lightning burst is up to 6 strikes, each with its thunder)
  const float OpenCutoff = 20000.f;   // Hz, out in the open (no filtering heard)
  const float InsideCutoff = 350.f;   // well inside
  const float InsideVolume = 0.5f;    // well inside, times the volume
  const float SidePan = 0.75f;        // how far to one side a strike from that side is put (1 all the way)
  const float FollowSeconds = 0.3f;   // how fast the muffling follows the camera in and out
  const int UpdateMs = 30;
  const float RainInsideCutoff = 600.f;
  const float RainInsideVolume = 0.4f;
  const float RainFadeSeconds = 1.f;
  const float RainLoopSeconds = 8.f;
  const float WindLoopSeconds = 24.f;
  const float WindFollowSeconds = 0.8f;
  const float WindDullCutoff = 700.f;
  const float WindBrightCutoff = 6000.f;
  const float WindInsideCutoff = 260.f;
  const float WindInsideVolume = 0.35f;
  // Heard only while the game's window is in front (the user, 2026-10-26:
  // "make the lighting strikes only be hearable when you focus the MEC
  // window"): Windows' audio plays whatever window is in front, so the tools'
  // master voice goes down to nothing over FocusFadeSeconds while another
  // window is, and back up when the game's is again. What's due meanwhile
  // still plays, unheard, so the strikes and their thunder stay in step.
  const float FocusFadeSeconds = 0.15f;

  typedef HRESULT(__stdcall* tXAudio2Create)(IXAudio2**, UINT32, XAUDIO2_PROCESSOR);

  struct Request
  {
    std::chrono::steady_clock::time_point Due;
    float Distance, Volume, DirX, DirZ;
    unsigned int Seed;
  };
  struct Playing
  {
    IXAudio2SourceVoice* Voice;
    std::unique_ptr<std::vector<float>> Samples;
    float Distance, Volume, DirX, DirZ;
    float Inside;   // as heard now (follows the camera's)
  };

  std::mutex g_Mutex;
  std::condition_variable g_Wake;
  std::deque<Request> g_Requests;     // under g_Mutex
  bool g_Started = false;             // under g_Mutex: the thread runs
  std::atomic<bool> g_Tried{ false };
  std::atomic<bool> g_Available{ false };
  std::atomic<unsigned int> g_Seed{ 0x9E3779B9u };
  std::atomic<float> g_RainWanted{ 0.f };   // the rain's volume asked for (see thunder::SetRain)
  std::atomic<float> g_WindWanted{ 0.f };   // the wind's, its strength and the camera's inside (thunder::SetWind)
  std::atomic<float> g_WindStrength{ 0.f };
  std::atomic<float> g_WindInside{ 0.f };

  // The game's own volume (see above), thread only
  struct GameVolume
  {
    std::wstring Path;
    FILETIME Written{};
    ULONGLONG CheckedTick = 0;
    float Value = 1.f;
    bool Logged = false;
  };

  std::wstring ProfilePath()
  {
    typedef HRESULT(__stdcall* tKnownFolder)(REFKNOWNFOLDERID, DWORD, HANDLE, PWSTR*);
    HMODULE shell = LoadLibraryW(L"shell32.dll");
    tKnownFolder known = shell ? (tKnownFolder)GetProcAddress(shell, "SHGetKnownFolderPath") : nullptr;
    PWSTR documents = nullptr;
    std::wstring path;
    if (known && SUCCEEDED(known(FOLDERID_Documents, 0, nullptr, &documents)) && documents)
      path = std::wstring(documents) + L"\\Mirrors Edge Catalyst\\settings\\PROF_SAVE_profile";
    if (documents)
      CoTaskMemFree(documents);
    return path;
  }

  void UpdateGameVolume(GameVolume& g)
  {
    ULONGLONG now = GetTickCount64();
    if (g.CheckedTick && now - g.CheckedTick < 2000)
      return;
    g.CheckedTick = now;
    if (g.Path.empty())
      g.Path = ProfilePath();
    WIN32_FILE_ATTRIBUTE_DATA info{};
    if (g.Path.empty() || !GetFileAttributesExW(g.Path.c_str(), GetFileExInfoStandard, &info))
      return;
    if (CompareFileTime(&info.ftLastWriteTime, &g.Written) == 0)
      return;
    g.Written = info.ftLastWriteTime;
    std::ifstream file(g.Path);
    std::string line;
    while (std::getline(file, line))
    {
      const char* key = "GstAudio.Volume ";
      if (line.compare(0, strlen(key), key) != 0)
        continue;
      float v = (float)atof(line.c_str() + strlen(key));
      if (v >= 0.f && v <= 1.f)
      {
        if (!g.Logged || v != g.Value)
          util::log::Write("Weather: the game's volume (GstAudio.Volume in its profile) %.2f: the rain and thunder follow it", v);
        g.Value = v;
        g.Logged = true;
      }
      break;
    }
  }

  // The rain's own voice (thread only)
  struct RainVoice
  {
    IXAudio2SourceVoice* Voice = nullptr;
    std::vector<float> Samples;
    float Volume = 0.f;   // as heard now
    float Inside = 0.f;
  };

  void PlaceRain(RainVoice& rain, float wanted, float inside, float dt, float gameVolume)
  {
    float follow = dt <= 0.f ? 1.f : (std::min)(1.f, dt / RainFadeSeconds);
    rain.Volume += (wanted - rain.Volume) * follow;
    float insideFollow = dt <= 0.f ? 1.f : (std::min)(1.f, dt / FollowSeconds);
    rain.Inside += (inside - rain.Inside) * insideFollow;
    float cutoff = OpenCutoff * powf(RainInsideCutoff / OpenCutoff, rain.Inside);
    XAUDIO2_FILTER_PARAMETERS filter{ LowPassFilter, (std::min)(XAUDIO2_MAX_FILTER_FREQUENCY,
      2.f * sinf(3.14159265f * cutoff / SampleRate)), 1.f };
    rain.Voice->SetFilterParameters(&filter);
    rain.Voice->SetVolume(rain.Volume * gameVolume * (1.f - (1.f - RainInsideVolume) * rain.Inside));
  }

  // The wind's own voice (thread only)
  struct WindVoice
  {
    IXAudio2SourceVoice* Voice = nullptr;
    std::vector<float> Samples;
    float Volume = 0.f;     // as heard now
    float Strength = 0.f;
    float Inside = 0.f;
  };

  void PlaceWind(WindVoice& wind, float wanted, float strength, float inside, float dt, float gameVolume)
  {
    float follow = dt <= 0.f ? 1.f : (std::min)(1.f, dt / WindFollowSeconds);
    wind.Volume += (wanted - wind.Volume) * follow;
    wind.Strength += (strength - wind.Strength) * follow;
    float insideFollow = dt <= 0.f ? 1.f : (std::min)(1.f, dt / FollowSeconds);
    wind.Inside += (inside - wind.Inside) * insideFollow;
    float open = WindDullCutoff * powf(WindBrightCutoff / WindDullCutoff, wind.Strength);
    float cutoff = open * powf(WindInsideCutoff / open, wind.Inside);
    XAUDIO2_FILTER_PARAMETERS filter{ LowPassFilter, (std::min)(XAUDIO2_MAX_FILTER_FREQUENCY,
      2.f * sinf(3.14159265f * cutoff / SampleRate)), 1.f };
    wind.Voice->SetFilterParameters(&filter);
    wind.Voice->SetVolume(wind.Volume * gameVolume * (1.f - (1.f - WindInsideVolume) * wind.Inside));
  }

  // A playing thunder's filter, volume and speakers for where the camera is
  // and which way it looks now
  void Place(Playing& p, unsigned int channels, float const* right, float const* back, float inside, float dt, float gameVolume)
  {
    float follow = dt <= 0.f ? 1.f : (std::min)(1.f, dt / FollowSeconds);
    p.Inside += (inside - p.Inside) * follow;
    float cutoff = OpenCutoff * powf(InsideCutoff / OpenCutoff, p.Inside);
    XAUDIO2_FILTER_PARAMETERS filter{ LowPassFilter, (std::min)(XAUDIO2_MAX_FILTER_FREQUENCY,
      2.f * sinf(3.14159265f * cutoff / SampleRate)), 1.f };
    p.Voice->SetFilterParameters(&filter);
    p.Voice->SetVolume(p.Volume * gameVolume * (1.f - (1.f - InsideVolume) * p.Inside));
    // Left and right from the camera's right axis, front and back from its
    // back axis (it looks along -back)
    float side = p.DirX * right[0] + p.DirZ * right[2];
    float front = -(p.DirX * back[0] + p.DirZ * back[2]);
    float pan = (std::max)(-1.f, (std::min)(1.f, side)) * SidePan;
    float angle = (pan + 1.f) * 3.14159265f / 4.f;
    float left = cosf(angle), rightGain = sinf(angle);
    float matrix[8] = {};
    if (channels == 1)
      matrix[0] = 1.f;
    else if (channels >= 4)
    {
      // Front pair and back pair (quad: FL FR BL BR; 5.1 and 7.1: FL FR FC LFE BL BR ...)
      float toFront = sqrtf((std::max)(0.f, (std::min)(1.f, 0.5f + 0.5f * front)));
      float toBack = sqrtf(1.f - toFront * toFront);
      int backLeft = channels == 4 ? 2 : 4;
      matrix[0] = left * toFront;
      matrix[1] = rightGain * toFront;
      matrix[backLeft] = left * toBack;
      matrix[backLeft + 1] = rightGain * toBack;
    }
    else
    {
      matrix[0] = left;
      matrix[1] = rightGain;
    }
    p.Voice->SetOutputMatrix(nullptr, 1, channels, matrix);
  }

  void Run()
  {
    HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    IXAudio2* engine = nullptr;
    IXAudio2MasteringVoice* master = nullptr;
    HMODULE dll = LoadLibraryW(L"xaudio2_9.dll");
    if (!dll)
      dll = LoadLibraryW(L"xaudio2_8.dll");
    tXAudio2Create create = dll ? (tXAudio2Create)GetProcAddress(dll, "XAudio2Create") : nullptr;
    HRESULT hr = create ? create(&engine, 0, XAUDIO2_DEFAULT_PROCESSOR) : E_FAIL;
    if (SUCCEEDED(hr))
      hr = engine->CreateMasteringVoice(&master);
    unsigned int channels = 2;
    if (SUCCEEDED(hr))
    {
      XAUDIO2_VOICE_DETAILS details{};
      master->GetVoiceDetails(&details);
      channels = (std::max)(1u, (std::min)(8u, (unsigned int)details.InputChannels));
    }
    g_Available = SUCCEEDED(hr);
    g_Tried = true;
    if (FAILED(hr))
      util::log::Warning("Weather: Windows' audio (XAudio2) couldn't be opened for the thunder (HRESULT 0x%X): no thunder heard", hr);
    else
      util::log::Write("Weather: thunder plays through Windows' audio (XAudio2, %u speakers)", channels);

    std::vector<Playing> playing;
    WAVEFORMATEX format{};
    format.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
    format.nChannels = 1;
    format.nSamplesPerSec = SampleRate;
    format.wBitsPerSample = 32;
    format.nBlockAlign = 4;
    format.nAvgBytesPerSec = SampleRate * 4;
    auto last = std::chrono::steady_clock::now();
    RainVoice rain;
    WindVoice wind;
    GameVolume gameVolume;
    float focus = 1.f, focusSet = 1.f;   // the master voice's volume (see FocusFadeSeconds)
    // The game paused (its pause menu) or loading (the user, 2026-10-26:
    // "lighting sound sould be muted when in pause menu"): the tools' sound
    // engine is stopped, so a thunder already playing waits where it is and
    // one that comes due meanwhile is started then, both heard once the game
    // goes on (gamestate::Paused / Loading)
    bool held = false;
    WAVEFORMATEX rainFormat = format;
    rainFormat.nChannels = 2;
    rainFormat.nBlockAlign = 8;
    rainFormat.nAvgBytesPerSec = SampleRate * 8;
    while (!g_shutdown)
    {
      Request due{};
      bool have = false;
      {
        std::unique_lock<std::mutex> lock(g_Mutex);
        auto now = std::chrono::steady_clock::now();
        auto until = now + std::chrono::milliseconds(playing.empty() && !rain.Voice && g_RainWanted.load() <= 0.f &&
          !wind.Voice && g_WindWanted.load() <= 0.f ? 100 : UpdateMs);
        for (Request const& q : g_Requests)
          if (q.Due < until)
            until = q.Due;
        if (until > now)
          g_Wake.wait_until(lock, until);
        now = std::chrono::steady_clock::now();
        for (auto it = g_Requests.begin(); it != g_Requests.end(); ++it)
          if (it->Due <= now)
          {
            due = *it;
            g_Requests.erase(it);
            have = true;
            break;
          }
      }
      auto now = std::chrono::steady_clock::now();
      float dt = std::chrono::duration<float>(now - last).count();
      last = now;
      // Where the camera is and looks (the weather's lock, not this one's)
      float right[3], back[3], inside = 0.f;
      weather::GetListener(right, back, inside);
      UpdateGameVolume(gameVolume);
      // Only with the game's window in front (see FocusFadeSeconds); before
      // its window is known, as before
      HWND game = g_gameHwnd;
      float focusWanted = !game || GetForegroundWindow() == game ? 1.f : 0.f;
      focus += (focusWanted - focus) * (std::min)(1.f, dt / FocusFadeSeconds);
      if (fabsf(focus - focusWanted) < 0.01f)
        focus = focusWanted;
      if (master && focus != focusSet && SUCCEEDED(master->SetVolume(focus)))
        focusSet = focus;
      bool paused = gamestate::Paused(), loading = !paused && gamestate::Loading();
      if (engine && (paused || loading) != held)
      {
        held = paused || loading;
        if (held)
          engine->StopEngine();
        else
          engine->StartEngine();
        util::log::Write("Weather: %s", held ? (paused ? "the game is paused: the thunder waits" :
          "the game's loading screen is up: the thunder waits") : "the game goes on: so does the thunder");
      }
      // The rain's voice: made when first wanted (its loop made then), let go
      // once it's faded out
      float rainWanted = g_RainWanted.load();
      if (engine && rainWanted > 0.f && !rain.Voice)
      {
        if (rain.Samples.empty())
          rain_synth::MakeLoop(RainLoopSeconds, 0x51F15EEDu, rain.Samples);
        if (SUCCEEDED(engine->CreateSourceVoice(&rain.Voice, &rainFormat, XAUDIO2_VOICE_USEFILTER)))
        {
          XAUDIO2_BUFFER buffer{};
          buffer.AudioBytes = (UINT32)(rain.Samples.size() * sizeof(float));
          buffer.pAudioData = (BYTE const*)rain.Samples.data();
          buffer.LoopCount = XAUDIO2_LOOP_INFINITE;
          rain.Volume = 0.f;
          rain.Inside = inside;
          PlaceRain(rain, 0.f, inside, 0.f, gameVolume.Value);
          if (SUCCEEDED(rain.Voice->SubmitSourceBuffer(&buffer)) && SUCCEEDED(rain.Voice->Start(0)))
            util::log::Write("Weather: the tools' rain sound plays through Windows' audio");
          else
          {
            rain.Voice->DestroyVoice();
            rain.Voice = nullptr;
          }
        }
      }
      if (rain.Voice)
      {
        PlaceRain(rain, rainWanted, inside, dt, gameVolume.Value);
        if (rainWanted <= 0.f && rain.Volume < 0.002f)
        {
          rain.Voice->DestroyVoice();
          rain.Voice = nullptr;
          util::log::Write("Weather: the tools' rain sound stopped");
        }
      }
      // The wind's voice: the same way as the rain's
      float windWanted = g_WindWanted.load(), windStrength = g_WindStrength.load(), windInside = g_WindInside.load();
      if (engine && windWanted > 0.f && !wind.Voice)
      {
        if (wind.Samples.empty())
          wind_synth::MakeLoop(WindLoopSeconds, 0x3A1D5EEDu, wind.Samples);
        if (SUCCEEDED(engine->CreateSourceVoice(&wind.Voice, &rainFormat, XAUDIO2_VOICE_USEFILTER)))
        {
          XAUDIO2_BUFFER buffer{};
          buffer.AudioBytes = (UINT32)(wind.Samples.size() * sizeof(float));
          buffer.pAudioData = (BYTE const*)wind.Samples.data();
          buffer.LoopCount = XAUDIO2_LOOP_INFINITE;
          wind.Volume = 0.f;
          wind.Strength = windStrength;
          wind.Inside = windInside;
          PlaceWind(wind, 0.f, windStrength, windInside, 0.f, gameVolume.Value);
          if (SUCCEEDED(wind.Voice->SubmitSourceBuffer(&buffer)) && SUCCEEDED(wind.Voice->Start(0)))
            util::log::Write("Weather: the tools' wind sound plays through Windows' audio");
          else
          {
            wind.Voice->DestroyVoice();
            wind.Voice = nullptr;
          }
        }
      }
      if (wind.Voice)
      {
        PlaceWind(wind, windWanted, windStrength, windInside, dt, gameVolume.Value);
        if (windWanted <= 0.f && wind.Volume < 0.002f)
        {
          wind.Voice->DestroyVoice();
          wind.Voice = nullptr;
          util::log::Write("Weather: the tools' wind sound stopped");
        }
      }
      // Finished ones let go, the others placed for the camera now
      for (size_t i = 0; i < playing.size();)
      {
        XAUDIO2_VOICE_STATE state{};
        playing[i].Voice->GetState(&state, 0);
        if (state.BuffersQueued == 0)
        {
          util::log::Write("Weather: thunder from %.0f m played", playing[i].Distance);
          playing[i].Voice->DestroyVoice();
          playing.erase(playing.begin() + i);
        }
        else
        {
          Place(playing[i], channels, right, back, inside, dt, gameVolume.Value);
          ++i;
        }
      }
      if (!have || !engine || playing.size() >= MaxPlaying)
        continue;
      auto samples = std::make_unique<std::vector<float>>();
      thunder_synth::Make(due.Distance, due.Seed, *samples);
      IXAudio2SourceVoice* voice = nullptr;
      if (FAILED(engine->CreateSourceVoice(&voice, &format, XAUDIO2_VOICE_USEFILTER)))
        continue;
      XAUDIO2_BUFFER buffer{};
      buffer.Flags = XAUDIO2_END_OF_STREAM;
      buffer.AudioBytes = (UINT32)(samples->size() * sizeof(float));
      buffer.pAudioData = (BYTE const*)samples->data();
      float closeness = 1.f - (std::min)((std::max)(due.Distance, 0.f) / 3000.f, 1.f);
      Playing p{ voice, nullptr, due.Distance, (std::max)(0.f, (std::min)(due.Volume, 1.f)) * (0.55f + 0.45f * closeness),
        due.DirX, due.DirZ, inside };
      // Placed before it starts, so it doesn't begin from the wrong side
      Place(p, channels, right, back, inside, 0.f, gameVolume.Value);
      if (FAILED(voice->SubmitSourceBuffer(&buffer)) || FAILED(voice->Start(0)))
      {
        voice->DestroyVoice();
        continue;
      }
      float side = due.DirX * right[0] + due.DirZ * right[2];
      util::log::Write("Weather: thunder from %.0f m playing (%.1f s, from the %s, the camera %.0f %% inside)", due.Distance,
        samples->size() / (float)SampleRate, side > 0.33f ? "right" : side < -0.33f ? "left" : "front or back", 100.f * inside);
      p.Samples = std::move(samples);
      playing.push_back(std::move(p));
    }
    for (Playing& p : playing)
      p.Voice->DestroyVoice();
    if (rain.Voice)
      rain.Voice->DestroyVoice();
    if (wind.Voice)
      wind.Voice->DestroyVoice();
    if (master)
      master->DestroyVoice();
    if (engine)
      engine->Release();
    if (SUCCEEDED(com))
      CoUninitialize();
  }
}

void thunder::Play(float delay, float distance, float volume, float dirX, float dirZ)
{
  if (g_shutdown)
    return;
  std::lock_guard<std::mutex> lock(g_Mutex);
  if (!g_Started)
  {
    g_Started = true;
    std::thread(Run).detach();
  }
  if (g_Requests.size() < 16)
  {
    unsigned int seed = g_Seed.fetch_add(0x6D2B79F5u) * 2654435761u + GetTickCount();
    g_Requests.push_back({ std::chrono::steady_clock::now() + std::chrono::milliseconds((long long)((std::max)(delay, 0.f) * 1000.f)),
      distance, volume, dirX, dirZ, seed });
  }
  g_Wake.notify_one();
}

void thunder::SetRain(float volume)
{
  float v = (std::max)(0.f, (std::min)(volume, 1.f));
  if (g_RainWanted.exchange(v) == v || g_shutdown)
    return;
  std::lock_guard<std::mutex> lock(g_Mutex);
  if (v > 0.f && !g_Started)
  {
    g_Started = true;
    std::thread(Run).detach();
  }
  g_Wake.notify_one();
}

void thunder::SetWind(float volume, float strength, float inside)
{
  float v = (std::max)(0.f, (std::min)(volume, 1.f));
  g_WindStrength = (std::max)(0.f, (std::min)(strength, 1.f));
  g_WindInside = (std::max)(0.f, (std::min)(inside, 1.f));
  if (g_WindWanted.exchange(v) == v || g_shutdown)
    return;
  std::lock_guard<std::mutex> lock(g_Mutex);
  if (v > 0.f && !g_Started)
  {
    g_Started = true;
    std::thread(Run).detach();
  }
  g_Wake.notify_one();
}

bool thunder::Available()
{
  return g_Available.load();
}

bool thunder::Tried()
{
  return g_Tried.load();
}
