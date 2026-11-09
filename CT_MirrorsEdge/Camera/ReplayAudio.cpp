#include "ReplayAudio.h"
#include "ReplayStore.h"
#include "../Main.h"
#include "../Util/Util.h"
#include <Windows.h>
#include <objbase.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <audiopolicy.h>
#include <audioclientactivationparams.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cwchar>
#include <memory>
#include <mutex>
#include <new>
#include <thread>
#include <vector>

// The replay's sound track (2026-10-27). The user, after the first playback
// with stand-ins (the Release mission recorded, played once it was over):
// "ksec ... have no voicelines". The replay played its people's voice lines
// and a cutscene's lines again through the game's own sound system
// (util::hooks' voice line and sound asset replay), but those lines' waves are
// in the mission's sound banks, which go with the mission: that playback said
// 2 of 31 lines again and gave none of 9781 cutscene sounds ("not loaded any
// more"). So the replay now also keeps the sound itself:
// - While it records, Windows captures this process's own sound (process
//   loopback: ActivateAudioInterfaceAsync on VIRTUAL_AUDIO_DEVICE_PROCESS_
//   LOOPBACK with this process's id, Windows 10 2004 and later; the game's
//   mix as it rendered it, plus the tools' own thunder and rain), as 48 kHz
//   16-bit stereo (Windows converts), kept in memory in blocks of a second
//   (192 KB a second, at most MaxSeconds; blocks, so it never copies the
//   whole track as it grows). While nothing comes (the game silent), silence is added so
//   the track keeps up with the clock. At each of the replay's frames its
//   clock and how much of the track there was are noted; after the recording
//   those notes give each moment of the replay's clock its place in the track
//   (smoothed over SmoothSeconds, since the track comes in packets).
// - While it plays, every session of this process in Windows' mixer (the
//   game's sound, on any output) is muted, and the track is played on a
//   stream of the tools' own (its own session, TrackSession, so it isn't
//   muted with them), on the output the game's sound was on, at the replay's
//   time: it follows the replay's clock (handed over each frame and carried
//   on between frames), jumps when it's more than JumpSeconds off (a seek, a
//   pause, a loop), goes faster or slower with the replay's speed (and its
//   pitch with it), and is silent while paused, past the track's ends, or
//   when the replay isn't played in real time (a fixed playback FPS). When
//   it stops, the sessions it muted are unmuted. A file in the tools' folder
//   (MutedMarker) says sessions are muted, so if the game closes or crashes
//   meanwhile, the next start unmutes them (Init).
// What it can't do: the sound is as it was heard then, from where the camera
// was then (not from a free camera moved since), and everything is in it
// (music, the UI's sounds) as it was.
//
// The dialog only (2026-11-01; the user: "The game's sound as recorded should
// only record dialog of the mission unless we find a better way and replays
// play the dialog of the npc/character/mission character live"): once it
// stops recording, the track keeps only the spans of the dialog said
// meanwhile (KeepOnly: a mission's narrative lines and a cutscene's
// voice-over, a little before and after each); the rest is silence (which the saved recording's
// packing makes almost nothing). While it plays the game's own sound isn't
// muted any more: the replay says the recorded lines live through the game's
// voice-over system where their waves are still loaded (util::hooks), and the
// track is heard only while a line it can't say covers the time (SetPlayTime's
// gate, faded in and out over GateFadeSeconds). Mixed into the dialog's spans
// is what else was heard then (Windows gives the game's whole mix, not its
// voices one by one).
namespace
{
  const int SampleRate = 48000;
  const int Channels = 2;
  const double MaxSeconds = 11 * 60.0;            // the replay stops recording at 10 minutes
  const double JumpSeconds = 0.06;                // further off the replay's time than this: the track jumps to it
  const double MaxCarrySeconds = 0.25;            // the replay's time carried on between its frames, at most
  const double SmoothSeconds = 0.25;              // each side of a clock note, for its place in the track
  const DWORD WaitMs = 50;
  const REFERENCE_TIME CaptureBuffer = 1000000;   // 100 ms, in 100 ns
  const REFERENCE_TIME RenderBuffer = 400000;     // 40 ms
  const DWORD RecoverMs = 180000;                 // how long Init's thread looks for the game's sessions to unmute
  const double GateFadeSeconds = 0.03;            // the track faded in and out at the dialog's spans
  const char* MutedMarker = ".\\Cinematic Tools\\CT_ReplayMuted.txt";
  // The track's own session in Windows' mixer: {6D3F2A71-4C1E-4B8A-9E52-1F7A3C905B24}
  const GUID TrackSession = { 0x6D3F2A71, 0x4C1E, 0x4B8A, { 0x9E, 0x52, 0x1F, 0x7A, 0x3C, 0x90, 0x5B, 0x24 } };
  const wchar_t* TrackSessionText = L"{6D3F2A71-4C1E-4B8A-9E52-1F7A3C905B24}";

  typedef HRESULT(WINAPI* tActivateAudioInterfaceAsync)(LPCWSTR, REFIID, PROPVARIANT*,
    IActivateAudioInterfaceCompletionHandler*, IActivateAudioInterfaceAsyncOperation**);

  struct ClockNote
  {
    double Clock;
    long long Frame;     // frames of the track captured by then
    double Offset;       // Frame - Clock * SampleRate, smoothed (after the recording)
  };

  const long long BlockFrames = SampleRate;       // a second
  std::mutex g_Mutex;                       // g_Blocks while recording, g_Problem
  std::vector<std::unique_ptr<short[]>> g_Blocks;   // interleaved stereo, BlockFrames each
  std::mutex g_NotesMutex;                  // g_Notes while recording
  std::vector<ClockNote> g_Notes;
  std::atomic<long long> g_Frames{ 0 };     // frames in g_Blocks
  std::atomic<bool> g_Recording{ false };
  std::atomic<bool> g_Capturing{ false };   // Windows' capture is running
  std::atomic<bool> g_Full{ false };
  std::atomic<bool> g_Playing{ false };
  std::atomic<bool> g_Rendering{ false };   // the track's stream is running
  std::atomic<int> g_MutedSessions{ 0 };
  std::thread g_CaptureThread, g_PlayThread;
  char g_Problem[200] = "";                 // the last one, under g_Mutex

  struct PlayState
  {
    double Time;
    bool Paused;
    double Speed;
    LONGLONG At;     // QueryPerformanceCounter when handed over (0: not yet)
    bool Gate;       // heard now (a line the replay can't say live)
  };
  std::mutex g_PlayMutex;
  PlayState g_PlayState{};

  void SetProblem(char const* text, HRESULT hr)
  {
    std::lock_guard<std::mutex> lock(g_Mutex);
    sprintf_s(g_Problem, "%s (HRESULT 0x%08X)", text, (unsigned int)hr);
  }

  // The completion of Windows' activation: only signals (agile, so it may be
  // called on any thread)
  class ActivateHandler : public IActivateAudioInterfaceCompletionHandler, public IAgileObject
  {
  public:
    ActivateHandler() : m_Refs(1), m_Done(CreateEventW(nullptr, TRUE, FALSE, nullptr)) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** out) override
    {
      if (!out)
        return E_POINTER;
      if (riid == __uuidof(IUnknown) || riid == __uuidof(IActivateAudioInterfaceCompletionHandler))
        *out = static_cast<IActivateAudioInterfaceCompletionHandler*>(this);
      else if (riid == __uuidof(IAgileObject))
        *out = static_cast<IAgileObject*>(this);
      else
      {
        *out = nullptr;
        return E_NOINTERFACE;
      }
      AddRef();
      return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return (ULONG)InterlockedIncrement(&m_Refs); }
    ULONG STDMETHODCALLTYPE Release() override
    {
      ULONG refs = (ULONG)InterlockedDecrement(&m_Refs);
      if (refs == 0)
        delete this;
      return refs;
    }
    HRESULT STDMETHODCALLTYPE ActivateCompleted(IActivateAudioInterfaceAsyncOperation*) override
    {
      if (m_Done)
        SetEvent(m_Done);
      return S_OK;
    }
    HANDLE Done() const { return m_Done; }

  private:
    ~ActivateHandler()
    {
      if (m_Done)
        CloseHandle(m_Done);
    }
    LONG m_Refs;
    HANDLE m_Done;
  };

  // An audio client capturing this process's own sound
  IAudioClient* ActivateLoopback(HRESULT& hr)
  {
    HMODULE dll = LoadLibraryW(L"Mmdevapi.dll");
    tActivateAudioInterfaceAsync activate = dll ?
      (tActivateAudioInterfaceAsync)GetProcAddress(dll, "ActivateAudioInterfaceAsync") : nullptr;
    if (!activate)
    {
      hr = E_NOTIMPL;
      return nullptr;
    }
    AUDIOCLIENT_ACTIVATION_PARAMS params{};
    params.ActivationType = AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK;
    params.ProcessLoopbackParams.TargetProcessId = GetCurrentProcessId();
    params.ProcessLoopbackParams.ProcessLoopbackMode = PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE;
    PROPVARIANT variant{};
    variant.vt = VT_BLOB;
    variant.blob.cbSize = sizeof(params);
    variant.blob.pBlobData = (BYTE*)&params;
    ActivateHandler* handler = new (std::nothrow) ActivateHandler();
    if (!handler || !handler->Done())
    {
      if (handler)
        handler->Release();
      hr = E_OUTOFMEMORY;
      return nullptr;
    }
    IActivateAudioInterfaceAsyncOperation* operation = nullptr;
    hr = activate(VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK, __uuidof(IAudioClient), &variant, handler, &operation);
    IAudioClient* client = nullptr;
    if (SUCCEEDED(hr))
    {
      if (WaitForSingleObject(handler->Done(), 5000) != WAIT_OBJECT_0)
        hr = HRESULT_FROM_WIN32(ERROR_TIMEOUT);
      else
      {
        HRESULT result = E_FAIL;
        IUnknown* unknown = nullptr;
        hr = operation->GetActivateResult(&result, &unknown);
        if (SUCCEEDED(hr))
          hr = result;
        if (SUCCEEDED(hr) && unknown)
          hr = unknown->QueryInterface(__uuidof(IAudioClient), (void**)&client);
        if (unknown)
          unknown->Release();
      }
    }
    if (operation)
      operation->Release();
    handler->Release();
    if (FAILED(hr) && client)
    {
      client->Release();
      client = nullptr;
    }
    return client;
  }

  WAVEFORMATEX TrackFormat()
  {
    WAVEFORMATEX format{};
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = Channels;
    format.nSamplesPerSec = SampleRate;
    format.wBitsPerSample = 16;
    format.nBlockAlign = (WORD)(Channels * 2);
    format.nAvgBytesPerSec = SampleRate * format.nBlockAlign;
    return format;
  }

  // Frames to the track (data null: silence), within MaxSeconds and memory
  void Append(BYTE const* data, UINT32 frames)
  {
    if (!frames)
      return;
    std::lock_guard<std::mutex> lock(g_Mutex);
    long long have = g_Frames.load();
    if ((double)(have + frames) > MaxSeconds * SampleRate)
    {
      g_Full = true;
      return;
    }
    short const* in = (short const*)data;
    long long left = frames;
    while (left > 0)
    {
      size_t block = (size_t)(have / BlockFrames);
      long long at = have % BlockFrames;
      if (block >= g_Blocks.size())
      {
        try
        {
          g_Blocks.emplace_back(new short[(size_t)BlockFrames * Channels]());
        }
        catch (std::bad_alloc const&)
        {
          g_Full = true;
          return;
        }
      }
      long long count = (std::min)(left, BlockFrames - at);
      short* out = g_Blocks[block].get() + at * Channels;
      if (in)
      {
        memcpy(out, in, (size_t)count * Channels * sizeof(short));
        in += count * Channels;
      }
      else
        memset(out, 0, (size_t)count * Channels * sizeof(short));
      have += count;
      left -= count;
      g_Frames = have;
    }
  }

  // One frame's samples (only read once the recording is over)
  short const* TrackFrame(long long frame)
  {
    return g_Blocks[(size_t)(frame / BlockFrames)].get() + (frame % BlockFrames) * Channels;
  }

  void Capture()
  {
    HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    HRESULT hr = S_OK;
    IAudioClient* client = ActivateLoopback(hr);
    IAudioCaptureClient* capture = nullptr;
    HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    WAVEFORMATEX format = TrackFormat();
    if (client)
      hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK |
        AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY, CaptureBuffer, 0, &format, nullptr);
    if (client && SUCCEEDED(hr))
      hr = event ? client->SetEventHandle(event) : E_HANDLE;
    if (client && SUCCEEDED(hr))
      hr = client->GetService(__uuidof(IAudioCaptureClient), (void**)&capture);
    if (client && SUCCEEDED(hr))
      hr = client->Start();
    LARGE_INTEGER frequency{}, start{};
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&start);
    if (!client || FAILED(hr) || !capture)
    {
      SetProblem("Windows' capture of the game's sound couldn't be started", hr);
      util::log::Warning("Replay: Windows' capture of the game's own sound couldn't be started (HRESULT 0x%08X): the recording "
        "has no sound track", (unsigned int)hr);
    }
    else
    {
      g_Capturing = true;
      util::log::Write("Replay: the game's own sound is recorded too (Windows' capture of this process, %d Hz stereo)", SampleRate);
    }
    while (g_Capturing && g_Recording && !g_shutdown)
    {
      WaitForSingleObject(event, WaitMs);
      UINT32 packet = 0;
      while (SUCCEEDED(capture->GetNextPacketSize(&packet)) && packet > 0)
      {
        BYTE* data = nullptr;
        UINT32 frames = 0;
        DWORD flags = 0;
        if (FAILED(capture->GetBuffer(&data, &frames, &flags, nullptr, nullptr)))
          break;
        Append((flags & AUDCLNT_BUFFERFLAGS_SILENT) ? nullptr : data, frames);
        capture->ReleaseBuffer(frames);
      }
      // Nothing coming (the game silent): silence, so the track keeps up
      LARGE_INTEGER now{};
      QueryPerformanceCounter(&now);
      long long due = (now.QuadPart - start.QuadPart) * SampleRate / frequency.QuadPart;
      long long behind = due - g_Frames.load();
      if (behind > SampleRate / 5)
        Append(nullptr, (UINT32)(behind - SampleRate / 10));
    }
    g_Capturing = false;
    if (client)
      client->Stop();
    if (capture)
      capture->Release();
    if (client)
      client->Release();
    if (event)
      CloseHandle(event);
    if (SUCCEEDED(com))
      CoUninitialize();
  }

  // After the recording: each clock note's place in the track, smoothed
  void SmoothNotes()
  {
    size_t count = g_Notes.size();
    std::vector<double> raw(count);
    for (size_t i = 0; i < count; ++i)
      raw[i] = (double)g_Notes[i].Frame - g_Notes[i].Clock * SampleRate;
    size_t lo = 0, hi = 0;
    double sum = 0.0;
    for (size_t i = 0; i < count; ++i)
    {
      while (hi < count && g_Notes[hi].Clock <= g_Notes[i].Clock + SmoothSeconds)
        sum += raw[hi++];
      while (lo < hi && g_Notes[lo].Clock < g_Notes[i].Clock - SmoothSeconds)
        sum -= raw[lo++];
      g_Notes[i].Offset = hi > lo ? sum / (double)(hi - lo) : raw[i];
    }
  }

  // The track's frame for a moment of the replay's clock (-1: none)
  double FrameAt(double clock)
  {
    if (g_Notes.empty())
      return -1.0;
    auto it = std::upper_bound(g_Notes.begin(), g_Notes.end(), clock,
      [](double c, ClockNote const& note) { return c < note.Clock; });
    double offset;
    if (it == g_Notes.begin())
      offset = it->Offset;
    else if (it == g_Notes.end())
      offset = g_Notes.back().Offset;
    else
    {
      ClockNote const& before = *(it - 1);
      double span = it->Clock - before.Clock;
      double t = span > 0.0 ? (clock - before.Clock) / span : 0.0;
      offset = before.Offset + (it->Offset - before.Offset) * t;
    }
    return clock * SampleRate + offset;
  }

  // Every session of this process in Windows' mixer, on every output, but the
  // track's own: each one's volume control (AddRef'd), its control, and
  // whether it's playing
  struct GameSession
  {
    ISimpleAudioVolume* Volume;
    bool Active;
    wchar_t* Device;   // CoTaskMemFree'd by the caller
  };
  std::vector<GameSession> GameSessions()
  {
    std::vector<GameSession> sessions;
    IMMDeviceEnumerator* enumerator = nullptr;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
        (void**)&enumerator)))
      return sessions;
    IMMDeviceCollection* devices = nullptr;
    UINT deviceCount = 0;
    if (SUCCEEDED(enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &devices)))
      devices->GetCount(&deviceCount);
    DWORD pid = GetCurrentProcessId();
    for (UINT d = 0; d < deviceCount; ++d)
    {
      IMMDevice* device = nullptr;
      IAudioSessionManager2* manager = nullptr;
      IAudioSessionEnumerator* list = nullptr;
      int count = 0;
      if (SUCCEEDED(devices->Item(d, &device)) &&
          SUCCEEDED(device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, (void**)&manager)) &&
          SUCCEEDED(manager->GetSessionEnumerator(&list)))
        list->GetCount(&count);
      for (int s = 0; s < count; ++s)
      {
        IAudioSessionControl* control = nullptr;
        IAudioSessionControl2* control2 = nullptr;
        DWORD owner = 0;
        LPWSTR id = nullptr;
        if (SUCCEEDED(list->GetSession(s, &control)) &&
            SUCCEEDED(control->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&control2)) &&
            SUCCEEDED(control2->GetProcessId(&owner)) && owner == pid &&
            SUCCEEDED(control2->GetSessionInstanceIdentifier(&id)) && id && !wcsstr(id, TrackSessionText))
        {
          ISimpleAudioVolume* volume = nullptr;
          AudioSessionState state = AudioSessionStateInactive;
          control->GetState(&state);
          LPWSTR deviceId = nullptr;
          if (SUCCEEDED(control->QueryInterface(__uuidof(ISimpleAudioVolume), (void**)&volume)))
          {
            device->GetId(&deviceId);
            sessions.push_back({ volume, state == AudioSessionStateActive, deviceId });
          }
        }
        if (id)
          CoTaskMemFree(id);
        if (control2)
          control2->Release();
        if (control)
          control->Release();
      }
      if (list)
        list->Release();
      if (manager)
        manager->Release();
      if (device)
        device->Release();
    }
    if (devices)
      devices->Release();
    enumerator->Release();
    return sessions;
  }

  void FreeSessions(std::vector<GameSession>& sessions)
  {
    for (GameSession& s : sessions)
    {
      if (s.Volume)
        s.Volume->Release();
      if (s.Device)
        CoTaskMemFree(s.Device);
    }
    sessions.clear();
  }

  void WriteMarker()
  {
    FILE* file = nullptr;
    if (fopen_s(&file, MutedMarker, "w") == 0 && file)
    {
      fputs("The replay muted the game's sound in Windows' mixer while it played its sound track; the tools unmute it at "
        "the next start if this file is still here.\n", file);
      fclose(file);
    }
  }

  void Play()
  {
    HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    // The output the game's sound is on (its sessions aren't muted any more:
    // the replay's live sounds and dialog play through them)
    std::vector<GameSession> sessions = GameSessions();
    std::vector<ISimpleAudioVolume*> muted;
    wchar_t const* output = nullptr;
    for (GameSession& s : sessions)
      if (s.Active && !output)
        output = s.Device;
    if (!output && !sessions.empty())
      output = sessions.front().Device;
    g_MutedSessions = (int)muted.size();
    if (!muted.empty())
      WriteMarker();

    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDevice* device = nullptr;
    IAudioClient* client = nullptr;
    IAudioRenderClient* render = nullptr;
    HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    UINT32 bufferFrames = 0;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
      (void**)&enumerator);
    if (SUCCEEDED(hr))
      hr = output ? enumerator->GetDevice(output, &device) : enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
    if (SUCCEEDED(hr))
      hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&client);
    WAVEFORMATEX format = TrackFormat();
    if (SUCCEEDED(hr))
      hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM |
        AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY, RenderBuffer, 0, &format, &TrackSession);
    if (SUCCEEDED(hr))
      hr = event ? client->SetEventHandle(event) : E_HANDLE;
    if (SUCCEEDED(hr))
      hr = client->GetBufferSize(&bufferFrames);
    if (SUCCEEDED(hr))
      hr = client->GetService(__uuidof(IAudioRenderClient), (void**)&render);
    if (SUCCEEDED(hr))
      hr = client->Start();
    if (FAILED(hr) || !render)
    {
      SetProblem("the sound track couldn't be played", hr);
      util::log::Warning("Replay: the sound track couldn't be played (HRESULT 0x%08X)", (unsigned int)hr);
    }
    else
    {
      g_Rendering = true;
      util::log::Write("Replay: playing its sound track (%.1f s) for the dialog the game can't say live; the game's own sound "
        "isn't muted%s", g_Frames.load() / (double)SampleRate, output ? "" : " (on the default output)");
    }
    LARGE_INTEGER frequency{};
    QueryPerformanceFrequency(&frequency);
    double readPos = -1.0;
    double gain = 0.0;   // the gate's, faded
    double fadeStep = 1.0 / (GateFadeSeconds * SampleRate);
    long long total = g_Frames.load();
    while (g_Rendering && g_Playing && !g_shutdown)
    {
      WaitForSingleObject(event, WaitMs);
      UINT32 padding = 0;
      if (FAILED(client->GetCurrentPadding(&padding)))
        break;
      UINT32 avail = bufferFrames > padding ? bufferFrames - padding : 0;
      if (!avail)
        continue;
      BYTE* out = nullptr;
      if (FAILED(render->GetBuffer(avail, &out)))
        break;
      PlayState state;
      {
        std::lock_guard<std::mutex> lock(g_PlayMutex);
        state = g_PlayState;
      }
      bool silent = state.Paused || state.Speed <= 0.0 || state.At == 0 || (!state.Gate && gain <= 0.0);
      if (!silent)
      {
        // Where the replay is now, and when what's written now is heard
        LARGE_INTEGER now{};
        QueryPerformanceCounter(&now);
        double since = (std::min)((double)(now.QuadPart - state.At) / (double)frequency.QuadPart, MaxCarrySeconds);
        double clock = state.Time + (since + (double)padding / SampleRate) * state.Speed;
        double target = FrameAt(clock);
        if (target < 0.0 || target >= (double)(total - 1))
          silent = true;
        else
        {
          if (readPos < 0.0 || fabs(readPos - target) > JumpSeconds * SampleRate)
            readPos = target;
          short* dst = (short*)out;
          for (UINT32 i = 0; i < avail; ++i)
          {
            long long at = (long long)readPos;
            if (at < 0 || at + 1 >= total)
            {
              memset(dst + (size_t)i * Channels, 0, (size_t)(avail - i) * Channels * sizeof(short));
              break;
            }
            double f = readPos - (double)at;
            short const* first = TrackFrame(at);
            short const* second = TrackFrame(at + 1);
            gain = state.Gate ? (std::min)(1.0, gain + fadeStep) : (std::max)(0.0, gain - fadeStep);
            for (int c = 0; c < Channels; ++c)
            {
              double a = first[c], b = second[c];
              dst[(size_t)i * Channels + c] = (short)lrint((a + (b - a) * f) * gain);
            }
            readPos += state.Speed;
          }
        }
      }
      if (silent)
      {
        readPos = -1.0;
        gain = 0.0;
      }
      render->ReleaseBuffer(avail, silent ? AUDCLNT_BUFFERFLAGS_SILENT : 0);
    }
    if (client)
      client->Stop();
    g_Rendering = false;
    // The game's sound back
    for (ISimpleAudioVolume* volume : muted)
    {
      volume->SetMute(FALSE, nullptr);
      volume->Release();
    }
    if (!muted.empty())
    {
      DeleteFileA(MutedMarker);
      util::log::Write("Replay: the game's own sound unmuted (%d sessions)", (int)muted.size());
    }
    g_MutedSessions = 0;
    FreeSessions(sessions);
    if (render)
      render->Release();
    if (client)
      client->Release();
    if (device)
      device->Release();
    if (enumerator)
      enumerator->Release();
    if (event)
      CloseHandle(event);
    if (SUCCEEDED(com))
      CoUninitialize();
  }

  // A playback cut short left the game muted: its sessions unmuted once
  // they're there (the game makes them as its sound starts)
  void Recover()
  {
    HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    DWORD started = GetTickCount();
    while (!g_shutdown && GetTickCount() - started < RecoverMs)
    {
      std::vector<GameSession> sessions = GameSessions();
      int unmuted = 0;
      for (GameSession& s : sessions)
      {
        BOOL was = FALSE;
        if (SUCCEEDED(s.Volume->GetMute(&was)) && was && SUCCEEDED(s.Volume->SetMute(FALSE, nullptr)))
          ++unmuted;
      }
      bool found = !sessions.empty();
      FreeSessions(sessions);
      if (found)
      {
        DeleteFileA(MutedMarker);
        util::log::Write("Replay: a playback of its sound track was cut short last time (the game closed or crashed while it "
          "played): the game's own sound unmuted (%d sessions)", unmuted);
        break;
      }
      Sleep(2000);
    }
    if (SUCCEEDED(com))
      CoUninitialize();
  }

  void JoinCapture()
  {
    if (g_CaptureThread.joinable())
      g_CaptureThread.join();
  }

  void JoinPlay()
  {
    if (g_PlayThread.joinable())
      g_PlayThread.join();
  }
}

void replay_audio::Init()
{
  if (GetFileAttributesA(MutedMarker) != INVALID_FILE_ATTRIBUTES)
    std::thread(Recover).detach();
}

bool replay_audio::StartRecording()
{
  StopPlaying();
  g_Recording = false;
  JoinCapture();
  Forget();
  g_Full = false;
  {
    std::lock_guard<std::mutex> lock(g_Mutex);
    g_Problem[0] = 0;
  }
  g_Recording = true;
  try
  {
    g_CaptureThread = std::thread(Capture);
  }
  catch (...)
  {
    g_Recording = false;
    return false;
  }
  return true;
}

void replay_audio::NoteClock(double clock)
{
  if (!g_Recording)
    return;
  std::lock_guard<std::mutex> lock(g_NotesMutex);
  try
  {
    g_Notes.push_back({ clock, g_Frames.load(), 0.0 });
  }
  catch (std::bad_alloc const&)
  {
  }
}

void replay_audio::StopRecording()
{
  if (!g_Recording && !g_CaptureThread.joinable())
    return;
  g_Recording = false;
  JoinCapture();
  std::lock_guard<std::mutex> lock(g_NotesMutex);
  SmoothNotes();
  if (g_Frames > 0)
    util::log::Write("Replay: its sound track: %.1f s (%.1f MB)%s", g_Frames.load() / (double)SampleRate,
      g_Frames.load() * Channels * sizeof(short) / 1048576.0, g_Full ? ", cut at its limit or for want of memory" : "");
}

bool replay_audio::IsRecording()
{
  return g_Recording && g_Capturing;
}

bool replay_audio::HasTrack()
{
  return !g_Recording && g_Frames > SampleRate / 10 && !g_Notes.empty();
}

double replay_audio::TrackSeconds()
{
  return g_Frames.load() / (double)SampleRate;
}

void replay_audio::Forget()
{
  StopPlaying();
  {
    std::lock_guard<std::mutex> lock(g_Mutex);
    std::vector<std::unique_ptr<short[]>>().swap(g_Blocks);
    g_Frames = 0;
  }
  std::lock_guard<std::mutex> lock(g_NotesMutex);
  std::vector<ClockNote>().swap(g_Notes);
}

bool replay_audio::StartPlaying()
{
  StopPlaying();
  if (!HasTrack())
    return false;
  {
    std::lock_guard<std::mutex> lock(g_PlayMutex);
    g_PlayState = PlayState{};
  }
  {
    std::lock_guard<std::mutex> lock(g_Mutex);
    g_Problem[0] = 0;
  }
  g_Playing = true;
  try
  {
    g_PlayThread = std::thread(Play);
  }
  catch (...)
  {
    g_Playing = false;
    return false;
  }
  return true;
}

void replay_audio::SetPlayTime(double time, bool paused, double speed, bool gate)
{
  if (!g_Playing)
    return;
  LARGE_INTEGER now{};
  QueryPerformanceCounter(&now);
  std::lock_guard<std::mutex> lock(g_PlayMutex);
  g_PlayState = PlayState{ time, paused, speed, now.QuadPart, gate };
}

void replay_audio::KeepOnly(std::vector<std::pair<double, double>> const& spans, double before, double after)
{
  if (g_Recording || g_Frames <= 0)
    return;
  std::lock_guard<std::mutex> notes(g_NotesMutex);
  if (g_Notes.empty())
    return;
  long long total = g_Frames.load();
  // The frames kept: each span's, widened, on the track's clock
  std::vector<std::pair<long long, long long>> keep;
  for (auto const& s : spans)
  {
    long long from = (long long)FrameAt(s.first - before), to = (long long)FrameAt(s.second + after);
    from = (std::max)(0LL, from);
    to = (std::min)(total, to);
    if (to > from)
      keep.push_back({ from, to });
  }
  std::sort(keep.begin(), keep.end());
  long long kept = 0, at = 0;
  std::lock_guard<std::mutex> lock(g_Mutex);
  auto silence = [&](long long from, long long to) {
    for (long long f = from; f < to;)
    {
      size_t block = (size_t)(f / BlockFrames);
      long long in = f % BlockFrames, n = (std::min)((long long)BlockFrames - in, to - f);
      if (block < g_Blocks.size() && g_Blocks[block])
        memset(g_Blocks[block].get() + in * Channels, 0, (size_t)n * Channels * sizeof(short));
      f += n;
    }
  };
  for (auto const& k : keep)
  {
    if (k.first > at)
      silence(at, k.first);
    if (k.second > at)
    {
      kept += k.second - (std::max)(at, k.first);
      at = k.second;
    }
  }
  if (at < total)
    silence(at, total);
  util::log::Write("Replay: its sound track keeps the dialog only: %.1f s of %.1f s in %zu spans (the voice lines and a cutscene's "
    "voice-over, %.1f s before and %.1f s after each); the rest is silence", kept / (double)SampleRate, total / (double)SampleRate,
    keep.size(), before, after);
}

void replay_audio::StopPlaying()
{
  g_Playing = false;
  JoinPlay();
}

bool replay_audio::IsPlaying()
{
  return g_Playing && g_Rendering;
}

const char* replay_audio::Status(char* buffer, size_t size)
{
  char problem[200];
  {
    std::lock_guard<std::mutex> lock(g_Mutex);
    strcpy_s(problem, g_Problem);
  }
  double seconds = g_Frames.load() / (double)SampleRate;
  if (problem[0])
    sprintf_s(buffer, size, "Not working: %s (see CT.log).", problem);
  else if (g_Recording && g_Capturing)
    sprintf_s(buffer, size, "Recording the game's sound: %.0f s.", seconds);
  else if (g_Recording)
    sprintf_s(buffer, size, "Starting Windows' capture of the game's sound...");
  else if (g_Playing && g_Rendering)
    sprintf_s(buffer, size, "Playing its sound track for the dialog the game can't say live (the game's own sound isn't muted).");
  else if (g_Playing)
    sprintf_s(buffer, size, "Starting its sound track...");
  else if (HasTrack())
    sprintf_s(buffer, size, "A sound track of %.0f s (%.1f MB).", seconds, seconds * SampleRate * Channels * 2 / 1048576.0);
  else
    sprintf_s(buffer, size, "No sound track yet: it's recorded with the next recording.");
  return buffer;
}

void replay_audio::OnProcessExit()
{
  // A recording or playing thread is never joined when the game quits while
  // it runs (it's already stopped by then): a joinable std::thread's
  // destructor calls std::terminate, which ends the closing game with a
  // crash. Detaching only closes its handle.
  if (g_CaptureThread.joinable())
    g_CaptureThread.detach();
  if (g_PlayThread.joinable())
    g_PlayThread.detach();
}

bool replay_audio::Save(replay_store::Out& out)
{
  if (!HasTrack())
    return false;
  std::vector<short> samples;
  std::vector<ClockNote> notes;
  long long frames = 0;
  {
    std::lock_guard<std::mutex> lock(g_Mutex);
    frames = g_Frames.load();
    try
    {
      samples.resize((size_t)frames * Channels);
    }
    catch (std::bad_alloc const&)
    {
      return false;
    }
    for (long long at = 0; at < frames; at += BlockFrames)
    {
      size_t block = (size_t)(at / BlockFrames);
      if (block >= g_Blocks.size())
        return false;
      long long n = (std::min)(BlockFrames, frames - at);
      memcpy(&samples[(size_t)at * Channels], g_Blocks[block].get(), (size_t)n * Channels * sizeof(short));
    }
  }
  {
    std::lock_guard<std::mutex> lock(g_NotesMutex);
    notes = g_Notes;
  }
  out.Put(SampleRate);
  out.Put(Channels);
  out.Vec(notes);
  out.Frames(samples.data(), (size_t)frames, Channels * sizeof(short), sizeof(short));
  return true;
}

bool replay_audio::Load(replay_store::In& in)
{
  int rate = 0, channels = 0;
  std::vector<ClockNote> notes;
  std::vector<unsigned char> bytes;
  if (!in.Get(rate) || !in.Get(channels) || rate != SampleRate || channels != Channels ||
      !in.Vec(notes, (size_t)(MaxSeconds * 1000)) ||
      !in.Frames(bytes, (size_t)(MaxSeconds * SampleRate) * Channels * sizeof(short)) || bytes.size() % (Channels * sizeof(short)))
    return false;
  Forget();
  long long frames = (long long)(bytes.size() / (Channels * sizeof(short)));
  std::vector<std::unique_ptr<short[]>> blocks;
  try
  {
    for (long long at = 0; at < frames; at += BlockFrames)
    {
      std::unique_ptr<short[]> block(new short[(size_t)BlockFrames * Channels]());
      long long n = (std::min)(BlockFrames, frames - at);
      memcpy(block.get(), bytes.data() + (size_t)at * Channels * sizeof(short), (size_t)n * Channels * sizeof(short));
      blocks.push_back(std::move(block));
    }
  }
  catch (std::bad_alloc const&)
  {
    return false;
  }
  {
    std::lock_guard<std::mutex> lock(g_Mutex);
    g_Blocks.swap(blocks);
    g_Frames = frames;
  }
  std::lock_guard<std::mutex> lock(g_NotesMutex);
  g_Notes.swap(notes);
  return true;
}
