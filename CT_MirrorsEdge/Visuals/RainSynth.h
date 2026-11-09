#pragma once
#include "ThunderSynth.h"
#include <algorithm>
#include <cmath>
#include <vector>

// The sound of the rain the tools draw (VISUALS > Weather > Rain in the
// distance), played by Thunder.cpp's thread through Windows' audio; also built
// on its own offline to listen to it. Rain heard all round is a wash (many
// drops too far or too many to tell apart: noise, brighter than deep,
// swelling a little) and a patter (the nearer drops: each a few milliseconds of
// bright noise, most of them faint, a few louder). Each ear gets its own, so
// it's wide. Made once as a loop: its last half second is faded into its
// first, so it repeats without a seam. Muffling indoors is done when it plays
// (a low pass).
namespace rain_synth
{
  using thunder_synth::Biquad;
  using thunder_synth::MakeFilter;
  using thunder_synth::Random;
  const int SampleRate = thunder_synth::SampleRate;

  // A looping stereo rain, seconds long, interleaved left / right
  inline void MakeLoop(float seconds, unsigned int seed, std::vector<float>& out)
  {
    Random r{ seed ? seed : 1u };
    size_t count = (size_t)(seconds * SampleRate);
    size_t fade = SampleRate / 2;
    size_t total = count + fade;
    std::vector<float> channel[2];
    for (int c = 0; c < 2; ++c)
    {
      std::vector<float> wash(total, 0.f), patter(total, 0.f);
      // The wash: pink noise (Paul Kellet's filter), 250 Hz - 8 kHz, swelling
      // slowly (noise low-passed at 0.4 Hz, 20 %)
      float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
      Biquad high = MakeFilter(250.f, true), low = MakeFilter(8000.f, false);
      Biquad swellA = MakeFilter(0.4f, false), swellB = swellA;
      std::vector<float> swell(total);
      double swellSum = 0.0;
      for (size_t n = 0; n < total; ++n)
      {
        swell[n] = swellB.Run(swellA.Run(r.Signed()));
        swellSum += (double)swell[n] * swell[n];
      }
      float swellScale = swellSum > 0.0 ? 0.2f / (float)sqrt(swellSum / (double)total) : 0.f;
      for (size_t n = 0; n < total; ++n)
      {
        float white = r.Signed();
        b0 = 0.99886f * b0 + white * 0.0555179f;
        b1 = 0.99332f * b1 + white * 0.0750759f;
        b2 = 0.96900f * b2 + white * 0.1538520f;
        b3 = 0.86650f * b3 + white * 0.3104856f;
        b4 = 0.55000f * b4 + white * 0.5329522f;
        b5 = -0.7616f * b5 - white * 0.0168980f;
        float pink = b0 + b1 + b2 + b3 + b4 + b5 + b6 + white * 0.5362f;
        b6 = white * 0.115926f;
        wash[n] = low.Run(high.Run(pink)) * (std::max)(0.f, 1.f + swell[n] * swellScale);
      }
      // The patter: about 900 drops a second, each 2 - 8 ms of bright noise
      // (its own differences: no lows), up in 0.3 ms and dying away, most of
      // them faint (the 4th power of a chance), a few louder
      size_t drops = (size_t)(900.f * total / SampleRate);
      for (size_t d = 0; d < drops; ++d)
      {
        size_t start = (size_t)(r.Next() * (total - 1));
        size_t length = (size_t)((0.002f + 0.006f * r.Next()) * SampleRate);
        float u = r.Next();
        float level = 0.08f + 0.92f * u * u * u * u;
        float decay = length * 0.25f;
        size_t rise = (size_t)(0.0003f * SampleRate);
        float last = 0.f;
        for (size_t i = 0; i < length && start + i < total; ++i)
        {
          float w = r.Signed();
          float bright = w - last;
          last = w;
          float env = (i < rise ? (float)i / rise : 1.f) * expf(-(float)i / decay);
          patter[start + i] += bright * env * level;
        }
      }
      Biquad patterLow = MakeFilter(11000.f, false);
      for (float& x : patter)
        x = patterLow.Run(x);
      // Each to its level (RMS: the wash 0.1, the patter 0.05), together
      double ws = 0.0, ps = 0.0;
      for (size_t n = 0; n < total; ++n)
      {
        ws += (double)wash[n] * wash[n];
        ps += (double)patter[n] * patter[n];
      }
      float wg = ws > 0.0 ? 0.1f / (float)sqrt(ws / total) : 0.f;
      float pg = ps > 0.0 ? 0.05f / (float)sqrt(ps / total) : 0.f;
      channel[c].resize(count);
      for (size_t n = 0; n < count; ++n)
      {
        float x = wash[n] * wg + patter[n] * pg;
        if (n < fade)
        {
          // The end faded into the start (equal power)
          float f = (float)n / fade;
          float tail = wash[count + n] * wg + patter[count + n] * pg;
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
