#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

// The thunder's sound (played by Thunder.cpp; also built on its own offline to
// listen to it). What's heard of a strike: close by, a crack (a bright, tearing
// burst over a fraction of a second) and a low boom; then the rumble, rolls of
// low noise rising and dying away as the sound from farther along the
// lightning's branches arrives, each wavering; the air takes the highs out of
// it the farther it travels, and the city's walls send it back a few times.
// Everything is made of smooth pieces: noise through filters, under envelopes
// that rise and fall without steps, so nothing clicks. (The 2026-10-25 18:06
// version summed hundreds of N-waves, pulses with a jump at each end, then
// limited the loudest moments within 3 ms: its highs were as impulsive as
// static (kurtosis 10 - 16, smooth noise is 3) and sounded bit-crushed. The
// one before rounded its peaks off with tanh.)
namespace thunder_synth
{
  const int SampleRate = 48000;
  const float Pi = 3.14159265f;
  // How loud each part is against the others (their filtered noise's own
  // levels differ: measured offline, see the research notes)
  const float RumbleLow = 4.f;
  const float RumbleMid = 1.f;
  const float CrackLevel = 0.8f;
  const float BoomLevel = 7.f;

  struct Random
  {
    unsigned int State;
    float Next() // 0 - 1
    {
      State ^= State << 13;
      State ^= State >> 17;
      State ^= State << 5;
      return (State & 0xFFFFFF) / 16777215.f;
    }
    float Signed() { return Next() * 2.f - 1.f; }
    float Between(float lo, float hi) { return lo + (hi - lo) * Next(); }
  };

  // A second-order filter (the Audio EQ Cookbook's low and high pass)
  struct Biquad
  {
    float B0, B1, B2, A1, A2;
    float X1 = 0.f, X2 = 0.f, Y1 = 0.f, Y2 = 0.f;
    float Run(float x)
    {
      float y = B0 * x + B1 * X1 + B2 * X2 - A1 * Y1 - A2 * Y2;
      X2 = X1; X1 = x; Y2 = Y1; Y1 = y;
      return y;
    }
  };
  inline Biquad MakeFilter(float hz, bool high)
  {
    const float q = 0.7071f;
    float w = 2.f * Pi * (std::min)(hz, SampleRate * 0.45f) / SampleRate;
    float alpha = sinf(w) / (2.f * q), c = cosf(w), a0 = 1.f + alpha;
    Biquad f{};
    if (high)
    {
      f.B0 = (1.f + c) / 2.f / a0; f.B1 = -(1.f + c) / a0; f.B2 = f.B0;
    }
    else
    {
      f.B0 = (1.f - c) / 2.f / a0; f.B1 = (1.f - c) / a0; f.B2 = f.B0;
    }
    f.A1 = -2.f * c / a0; f.A2 = (1.f - alpha) / a0;
    return f;
  }

  // A rise over attack seconds (half a cosine) and an exponential fall with
  // the time constant decay; t seconds from its start
  inline float Swell(float t, float attack, float decay)
  {
    if (t <= 0.f)
      return 0.f;
    if (t < attack)
      return 0.5f - 0.5f * cosf(Pi * t / attack);
    return expf(-(t - attack) / decay);
  }

  // A slow random wavering about 1 (noise low-passed twice under hz, scaled
  // so it strays depth from 1 on average, never below 0), times what's in out
  inline void Waver(Random& r, float hz, float depth, std::vector<float>& out)
  {
    Biquad a = MakeFilter(hz, false), b = a;
    std::vector<float> w(out.size());
    double sum = 0.0;
    for (size_t n = 0; n < w.size(); ++n)
    {
      w[n] = b.Run(a.Run(r.Signed()));
      sum += (double)w[n] * w[n];
    }
    float scale = sum > 0.0 ? depth / (float)sqrt(sum / (double)w.size()) : 0.f;
    for (size_t n = 0; n < w.size(); ++n)
      out[n] *= (std::max)(0.f, 1.f + w[n] * scale);
  }

  // One strike's thunder heard distance metres away, mono at SampleRate
  inline void Make(float distance, unsigned int seed, std::vector<float>& out)
  {
    Random r{ seed ? seed : 1u };
    float closeness = 1.f - (std::min)((std::max)(distance, 0.f) / 3000.f, 1.f);   // 1 overhead, 0 3 km away
    float farness = 1.f - closeness;
    float seconds = 5.5f + 6.f * farness + 2.f * r.Next();
    size_t count = (size_t)(seconds * SampleRate);
    out.assign(count, 0.f);

    // The rolls: the first soon, the others (the branches farther along)
    // spread over the first half, each a swell of low noise
    struct Roll { float Start, Attack, Decay, Level; Biquad Low1, Low2, Mid1, Mid2; float Mids; };
    std::vector<Roll> rolls;
    int rollCount = 4 + (int)(r.Next() * 5.f);
    for (int i = 0; i < rollCount; ++i)
    {
      Roll roll{};
      roll.Start = i == 0 ? r.Between(0.f, 0.08f) : r.Between(0.15f, 0.5f) * seconds;
      roll.Attack = r.Between(0.04f, 0.15f) + 0.25f * farness * r.Next();
      roll.Decay = r.Between(0.35f, 0.9f) + 1.6f * farness * r.Next();
      roll.Level = i == 0 ? 1.f : r.Between(0.35f, 0.9f);
      float low = r.Between(70.f, 130.f) + 60.f * closeness;
      roll.Low1 = MakeFilter(low, false);
      roll.Low2 = roll.Low1;
      // A band of mids in each, so it's heard on any speaker, not only felt
      roll.Mid1 = MakeFilter(r.Between(160.f, 260.f), true);
      roll.Mid2 = MakeFilter(r.Between(500.f, 900.f) * (0.6f + 0.6f * closeness), false);
      roll.Mids = 0.25f + 0.35f * closeness;
      rolls.push_back(roll);
    }
    // The crack (close by): grains of bright noise, each faded in and out,
    // tearing over the first 0.2 - 0.6 s and dying away; and a low boom
    struct Grain { float Start, Length, Level; };
    std::vector<Grain> grains;
    float crackLevel = (std::max)(0.f, (closeness - 0.45f) / 0.55f);
    if (crackLevel > 0.f)
    {
      float tear = r.Between(0.2f, 0.6f);
      int grainCount = 14 + (int)(r.Next() * 20.f);
      for (int i = 0; i < grainCount; ++i)
      {
        float t = tear * r.Next() * r.Next();
        grains.push_back({ t, r.Between(0.006f, 0.03f), crackLevel * expf(-t / (0.35f * tear)) * r.Between(0.4f, 1.f) });
      }
    }
    Biquad crackHigh = MakeFilter(700.f, true), crackLow = MakeFilter(5000.f, false);
    Biquad boom1 = MakeFilter(55.f, false), boom2 = boom1;

    // The rumble, wavering quickly and slowly
    for (size_t n = 0; n < count; ++n)
    {
      float t = (float)n / SampleRate;
      float sample = 0.f;
      for (Roll& roll : rolls)
      {
        float level = roll.Level * Swell(t - roll.Start, roll.Attack, roll.Decay);
        float white = r.Signed();
        float low = roll.Low2.Run(roll.Low1.Run(white));
        float mid = roll.Mid2.Run(roll.Mid1.Run(white));
        sample += (low * RumbleLow + mid * roll.Mids * RumbleMid) * level;
      }
      out[n] = sample;
    }
    Waver(r, 9.f, 0.45f, out);
    Waver(r, 2.f, 0.3f, out);
    // The crack and the boom on top
    if (crackLevel > 0.f)
      for (size_t n = 0; n < count; ++n)
      {
        float t = (float)n / SampleRate;
        float bright = crackLow.Run(crackHigh.Run(r.Signed()));
        float grainLevel = 0.f;
        for (Grain const& g : grains)
        {
          float u = (t - g.Start) / g.Length;
          if (u > 0.f && u < 1.f)
            grainLevel += g.Level * (0.5f - 0.5f * cosf(2.f * Pi * u));
        }
        out[n] += bright * grainLevel * CrackLevel;
        out[n] += boom2.Run(boom1.Run(r.Signed())) * BoomLevel * crackLevel * Swell(t, 0.03f, 0.5f);
      }

    // The air: the farther, the duller; the lowest rumble under hearing out
    float cutoff = 400.f + 4600.f * closeness * closeness;
    Biquad air1 = MakeFilter(cutoff, false), air2 = air1;
    Biquad low1 = MakeFilter(28.f, true), low2 = low1;
    for (size_t n = 0; n < count; ++n)
      out[n] = low2.Run(low1.Run(air2.Run(air1.Run(out[n]))));

    // The city's echoes: a few later, softer, duller copies
    const float echoDelay[5] = { 0.09f, 0.21f, 0.37f, 0.62f, 0.95f };
    const float echoGain[5] = { 0.3f, 0.24f, 0.18f, 0.13f, 0.08f };
    std::vector<float> dry(out);
    for (int e = 0; e < 5; ++e)
    {
      size_t d = (size_t)(echoDelay[e] * r.Between(0.8f, 1.2f) * SampleRate);
      Biquad dull = MakeFilter(cutoff * 0.5f, false);
      float g = echoGain[e] * r.Between(0.7f, 1.3f);
      for (size_t n = d; n < count; ++n)
        out[n] += g * dull.Run(dry[n - d]);
    }

    // Faded in (2 ms) and out (the last 1.5 s), then its peak at 0.9: one gain
    // for the whole sound, nothing limited or clipped
    size_t fadeIn = SampleRate / 500, fadeOut = (size_t)(1.5f * SampleRate);
    float peak = 0.f;
    for (size_t n = 0; n < count; ++n)
    {
      float f = 1.f;
      if (n < fadeIn)
        f *= (float)n / fadeIn;
      if (n + fadeOut > count)
        f *= 0.5f - 0.5f * cosf(Pi * (float)(count - n) / fadeOut);
      out[n] *= f;
      peak = (std::max)(peak, fabsf(out[n]));
    }
    float gain = peak > 1e-9f ? 0.9f / peak : 1.f;
    for (float& x : out)
      x *= gain;
  }
}
