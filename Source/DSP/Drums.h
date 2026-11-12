#pragma once
// Synthesised drum voices: no samples, so the plugin is royalty-free and tiny.
#include "Biquad.h"
#include <algorithm>

namespace jz
{
enum Lane { kKick, kSnare, kGhost, kHats, kWash, kToms, kRim, kNumLanes };

struct Kit
{
    const char* name;
    float kickHz, kickBend, kickDecay;     // body pitch, pitch-drop depth, seconds
    float snareHz, snareTone, snareDecay;  // shell pitch, shell vs wires, seconds
    float hatDecay, washDecay;             // seconds
    float tomHz, rimHz;
    float colourHz, drive;                 // kit low-pass, saturation 0..1
};

constexpr int kNumKits = 4;
inline const Kit kKits[kNumKits] = {
    { "Bonechapel",   52, 3.0f, 0.42f, 190, 0.45f, 0.20f, 0.060f, 1.8f, 110, 1700,  9000, 0.25f }, // dusty room
    { "Velvet Crypt", 46, 2.2f, 0.60f, 170, 0.30f, 0.28f, 0.090f, 2.8f,  95, 1400,  6000, 0.15f }, // soft, dark
    { "Rust Abbey",   58, 4.0f, 0.32f, 220, 0.55f, 0.16f, 0.045f, 1.4f, 130, 2100, 14000, 0.55f }, // hard, bright
    { "Moth Room",    50, 2.6f, 0.38f, 200, 0.40f, 0.22f, 0.070f, 2.2f, 105, 1600,  4200, 0.35f }, // lo-fi
};

class Voice
{
public:
    void prepare (double sampleRate, int laneIndex)
    {
        sr = sampleRate; lane = laneIndex; active = false;
        rng.s = 0x1234567u + (uint32_t) laneIndex * 7919u;
    }

    // vel 0..1, pitch = global multiplier, tune = per-hit multiplier (tom fills)
    void trigger (float vel, const Kit& k, float pitch, float tune)
    {
        kit = k; amp = std::pow (std::clamp (vel, 0.0f, 1.0f), 1.3f);
        pm = pitch * tune; t = 0; phase = 0; active = true;
        for (auto& m : metal) m = rng.next01();
        const float dur[kNumLanes] = { k.kickDecay, k.snareDecay, k.snareDecay * 0.6f, k.hatDecay * (0.7f + 0.6f * vel),
                                       k.washDecay, 0.45f, 0.08f };
        decay = dur[lane];
        f1.reset(); f2.reset();
        if (lane == kSnare || lane == kGhost) f1.setHP (sr, 1200, 0.7f);
        if (lane == kHats) f1.setHP (sr, 7000, 0.7f);
        if (lane == kWash) { f1.setHP (sr, 4500, 0.7f); f2.setLP (sr, 12000, 0.7f); }
        if (lane == kRim)  f1.setBP (sr, 2500, 1.5f);
        if (lane == kToms) f1.setLP (sr, 1800, 0.7f);
    }

    bool isActive() const { return active; }

    float next()
    {
        if (! active) return 0.0f;
        const float dt = 1.0f / (float) sr;
        const float env = std::exp (-t * 5.0f / decay); // ~-43 dB at `decay`
        const float noise = rng.nextBi();
        float out = 0;

        switch (lane)
        {
            case kKick:
            {
                const float f = kit.kickHz * pm * (1.0f + kit.kickBend * std::exp (-t * 35.0f));
                phase += f * dt;
                const float click = noise * std::exp (-t * 400.0f) * 0.3f;
                out = std::tanh ((std::sin (2 * kPi * phase) * env + click) * (1.0f + kit.drive * 3.0f));
                break;
            }
            case kSnare:
            case kGhost:
            {
                phase += kit.snareHz * pm * dt;
                const float shell = (std::sin (2 * kPi * phase) + 0.6f * std::sin (2 * kPi * phase * 1.6f))
                                    * std::exp (-t * 25.0f) * kit.snareTone;
                out = shell + f1.process (noise) * env * (1.2f - kit.snareTone);
                break;
            }
            case kHats:
            case kWash:
            {
                out = f2.process (f1.process (metallic() * 0.5f + noise * 0.5f));
                const float swell = lane == kWash ? std::min (1.0f, t * 200.0f) : 1.0f;
                out *= env * swell * (lane == kWash ? 0.8f : 1.0f);
                break;
            }
            case kToms:
            {
                phase += kit.tomHz * pm * (1.0f + 0.6f * std::exp (-t * 20.0f)) * dt;
                out = (std::sin (2 * kPi * phase) + f1.process (noise) * 0.15f) * env;
                break;
            }
            case kRim:
            {
                phase += kit.rimHz * pm * dt;
                out = std::sin (2 * kPi * phase) * std::exp (-t * 90.0f) * 0.6f
                    + f1.process (noise) * std::exp (-t * 200.0f) * 2.0f;
                break;
            }
            default: break;
        }

        t += dt;
        if (t > decay * 1.6f) active = false;
        return out * amp;
    }

private:
    // 808-style metal: six detuned square waves.
    float metallic()
    {
        static constexpr float ratios[6] = { 205.3f, 304.4f, 369.6f, 522.7f, 540.0f, 800.0f };
        float s = 0;
        for (int i = 0; i < 6; ++i)
        {
            metal[i] += ratios[i] * 1.6f * pm / (float) sr;
            if (metal[i] >= 1.0f) metal[i] -= 1.0f;
            s += metal[i] < 0.5f ? 1.0f : -1.0f;
        }
        return s / 6.0f;
    }

    double sr = 48000;
    int lane = 0;
    bool active = false;
    Kit kit = kKits[0];
    float amp = 0, pm = 1, t = 0, decay = 0.2f, phase = 0;
    float metal[6] {};
    Biquad f1, f2;
    Rng rng;
};

// The whole kit: one voice per lane (a new hit chokes the previous one), panned like a real kit.
class DrumKit
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        for (int i = 0; i < kNumLanes; ++i) voices[i].prepare (sr, i);
        colour.reset();
    }

    void trigger (int lane, float vel, int kitIndex, float pitch, float tune)
    {
        kitIdx = std::clamp (kitIndex, 0, kNumKits - 1);
        voices[std::clamp (lane, 0, kNumLanes - 1)].trigger (vel, kKits[kitIdx], pitch, tune);
    }

    // Adds into L/R.
    void render (float* L, float* R, int n)
    {
        static constexpr float pan[kNumLanes] = { 0.0f, 0.05f, 0.1f, 0.35f, -0.3f, -0.2f, -0.4f };
        colour.setLP (sr, kKits[kitIdx].colourHz, 0.6f);
        for (int i = 0; i < n; ++i)
        {
            float l = 0, r = 0;
            for (int v = 0; v < kNumLanes; ++v)
            {
                if (! voices[v].isActive()) continue;
                const float s = voices[v].next();
                l += s * (1.0f - pan[v]);
                r += s * (1.0f + pan[v]);
            }
            L[i] += colour.process (l * 0.5f, 0);
            R[i] += colour.process (r * 0.5f, 1);
        }
    }

private:
    double sr = 48000;
    int kitIdx = 0;
    Voice voices[kNumLanes];
    Biquad colour;
};
} // namespace jz
