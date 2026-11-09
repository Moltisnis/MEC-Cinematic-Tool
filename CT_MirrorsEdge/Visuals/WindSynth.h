#pragma once
#include "ThunderSynth.h"
#include <algorithm>
#include <cmath>
#include <vector>

// The wind's sound while it snows (VISUALS > Snow > Wind sound, 2026-10-26,
// the user: "there should be a wind sound playing when outside and muffled
// when indoors and the wind sound should get more intense if it's a blizzard
// same with the snow"), played by Thunder.cpp's thread through Windows'
// audio. Wind heard all round is air rushing past everything (noise, deep and
// soft: a body under 900 Hz) rising and falling in gusts, and in the gusts a
// howl, air past edges and corners (noise through narrow band passes whose
// pitch drifts slowly between about 250 and 900 Hz), and in the strongest a
// thin whistle (about 2 kHz). Each ear gets its own, so it's wide. Made once
// as a loop: its last second is faded into its first, so it repeats without a
// seam. How loud and how bright it is now, and the muffling indoors, are done
// when it plays (its volume and a low pass, from how hard it snows and the
// snow's wind's speed with its gusts).
namespace wind_synth
{
  using thunder_synth::Biquad;
  using thunder_synth::MakeFilter;
  using thunder_synth::Random;
  using thunder_synth::Pi;
  const int SampleRate = thunder_synth::SampleRate;

  // A band pass (the Audio EQ Cookbook's, 0 dB at its middle), q its narrowness
  inline void SetBandPass(Biquad& f, float hz, float q)
  {
    float w = 2.f * Pi * (std::min)(hz, SampleRate * 0.45f) / SampleRate;
    float alpha = sinf(w) / (2.f * q), c = cosf(w), a0 = 1.f + alpha;
    f.B0 = alpha / a0; f.B1 = 0.f; f.B2 = -alpha / a0;
    f.A1 = -2.f * c / a0; f.A2 = (1.f - alpha) / a0;
  }

  // Slow noise, -1 - 1 about: white noise through two low passes at hz,
  // scaled to an RMS of 0.5
  inline std::vector<float> Slow(Random& r, size_t total, float hz)
  {
    std::vector<float> out(total);
    Biquad a = MakeFilter(hz, false), b = a;
    double sum = 0.0;
    for (size_t n = 0; n < total; ++n)
    {
      out[n] = b.Run(a.Run(r.Signed()));
      sum += (double)out[n] * out[n];
    }
    float scale = sum > 0.0 ? 0.5f / (float)sqrt(sum / (double)total) : 0.f;
    for (float& x : out)
      x *= scale;
    return out;
  }

  // A looping stereo wind, seconds long, interleaved left / right
  inline void MakeLoop(float seconds, unsigned int seed, std::vector<float>& out)
  {
    Random r{ seed ? seed : 1u };
    size_t count = (size_t)(seconds * SampleRate);
    size_t fade = SampleRate;
    size_t total = count + fade;
    std::vector<float> channel[2];
    for (int c = 0; c < 2; ++c)
    {
      // Its gusts (every 8 s or so) and the howl's drifting pitch
      std::vector<float> gust = Slow(r, total, 0.12f), drift = Slow(r, total, 0.07f), flutter = Slow(r, total, 1.5f);
      std::vector<float> body(total), howl(total), whistle(total);
      // The body: pink noise (Paul Kellet's), 60 - 900 Hz
      float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
      Biquad high = MakeFilter(60.f, true), low = MakeFilter(900.f, false), low2 = low;
      Biquad bandA{}, bandB{}, bandW{};
      for (size_t n = 0; n < total; ++n)
      {
        float g = (std::max)(0.15f, 1.f + 0.9f * gust[n]);
        float white = r.Signed();
        b0 = 0.99886f * b0 + white * 0.0555179f;
        b1 = 0.99332f * b1 + white * 0.0750759f;
        b2 = 0.96900f * b2 + white * 0.1538520f;
        b3 = 0.86650f * b3 + white * 0.3104856f;
        b4 = 0.55000f * b4 + white * 0.5329522f;
        b5 = -0.7616f * b5 - white * 0.0168980f;
        float pink = b0 + b1 + b2 + b3 + b4 + b5 + b6 + white * 0.5362f;
        b6 = white * 0.115926f;
        body[n] = low2.Run(low.Run(high.Run(pink))) * g;
        // The howl and the whistle: their pitch moved every 32 samples
        if (n % 32 == 0)
        {
          float pitch = 470.f * powf(2.f, 0.9f * drift[n] + 0.08f * flutter[n]);
          SetBandPass(bandA, pitch, 9.f);
          SetBandPass(bandB, pitch * 1.62f, 12.f);
          SetBandPass(bandW, 2100.f * powf(2.f, 0.3f * drift[n]), 20.f);
        }
        float w2 = r.Signed();
        howl[n] = (bandA.Run(w2) + 0.6f * bandB.Run(w2)) * g * g;
        whistle[n] = bandW.Run(r.Signed()) * (std::max)(0.f, g - 1.2f) * (std::max)(0.f, g - 1.2f);
      }
      // Each to its level (RMS: the body 0.1, the howl 0.045, the whistle 0.012), together
      double bs = 0.0, hs = 0.0, ws = 0.0;
      for (size_t n = 0; n < total; ++n)
      {
        bs += (double)body[n] * body[n];
        hs += (double)howl[n] * howl[n];
        ws += (double)whistle[n] * whistle[n];
      }
      float bg = bs > 0.0 ? 0.1f / (float)sqrt(bs / total) : 0.f;
      float hg = hs > 0.0 ? 0.045f / (float)sqrt(hs / total) : 0.f;
      float wg = ws > 0.0 ? 0.012f / (float)sqrt(ws / total) : 0.f;
      channel[c].resize(count);
      for (size_t n = 0; n < count; ++n)
      {
        float x = body[n] * bg + howl[n] * hg + whistle[n] * wg;
        if (n < fade)
        {
          // The end faded into the start (equal power)
          float f = (float)n / fade;
          float tail = body[count + n] * bg + howl[count + n] * hg + whistle[count + n] * wg;
          x = x * sqrtf(f) + tail * sqrtf(1.f - f);
        }
        channel[c][n] = x;
      }
    }
    // Interleaved, the peak kept under 0.9 (one gain for the whole loop)
    float peak = 0.f;
    for (int c = 0; c < 2; ++c)
      for (float x : channel[c])
        peak = (std::max)(peak, fabsf(x));
    float gain = peak > 0.9f ? 0.9f / peak : 1.f;
    out.resize(count * 2);
    for (size_t n = 0; n < count; ++n)
    {
      out[n * 2] = channel[0][n] * gain;
      out[n * 2 + 1] = channel[1][n] * gain;
    }
  }
}
