#pragma once
// Shoegaze Veil + Underworld effects. Pure C++, preallocated, audio-thread safe.
#include "Biquad.h"
#include <vector>

namespace jz
{
// Reverse Bloom: records chunks and plays each one backwards, windowed, so hits swell in.
class ReverseBloom
{
public:
    void prepare (double sr) { maxLen = (int) (sr * 3.0); rec.assign ((size_t) maxLen * 2, 0); play.assign ((size_t) maxLen * 2, 0); len = maxLen / 2; pos = 0; }

    // chunkSamples is applied at the next chunk boundary.
    void process (const float* inL, const float* inR, float* outL, float* outR, int n, int chunkSamples)
    {
        for (int i = 0; i < n; ++i)
        {
            if (pos >= len)
            {
                std::swap (rec, play);
                playLen = len;
                len = std::max (256, std::min (chunkSamples, maxLen));
                pos = 0;
            }
            rec[(size_t) pos * 2] = inL[i];
            rec[(size_t) pos * 2 + 1] = inR[i];
            if (pos < playLen)
            {
                const int r = playLen - 1 - pos;
                const float w = std::sin (kPi * (float) pos / (float) playLen);
                outL[i] += play[(size_t) r * 2] * w;
                outR[i] += play[(size_t) r * 2 + 1] * w;
            }
            ++pos;
        }
    }

private:
    std::vector<float> rec, play;
    int maxLen = 1, len = 1, playLen = 0, pos = 0;
};

// Octave-up pitch shifter (two crossfaded read heads) used for the Shimmer feedback loop.
class OctaveUp
{
public:
    void prepare (double) { buf.assign (kSize * 2, 0); w = 0; ph = 0; }

    void process (const float* inL, const float* inR, float* outL, float* outR, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            buf[(size_t) w * 2] = inL[i];
            buf[(size_t) w * 2 + 1] = inR[i];
            float l = 0, r = 0;
            for (int head = 0; head < 2; ++head)
            {
                float p = ph + 0.5f * (float) head;
                if (p >= 1.0f) p -= 1.0f;
                const float delay = (1.0f - p) * kWin;       // shrinking delay = reading at 2x speed
                const float g = std::sin (kPi * p);
                const int idx = ((w - (int) delay - 1) % kSize + kSize) % kSize;
                l += buf[(size_t) idx * 2] * g;
                r += buf[(size_t) idx * 2 + 1] * g;
            }
            outL[i] = l * 0.7f;
            outR[i] = r * 0.7f;
            ph += 1.0f / kWin;
            if (ph >= 1.0f) ph -= 1.0f;
            w = (w + 1) % kSize;
        }
    }

private:
    static constexpr int kSize = 8192;
    static constexpr float kWin = 3000.0f;
    std::vector<float> buf;
    int w = 0;
    float ph = 0;
};

// Mono-in-stereo delay line with linear interpolation.
class Delay
{
public:
    void prepare (int maxSamples) { size = maxSamples + 2; buf.assign ((size_t) size * 2, 0); w = 0; }
    void write (float l, float r) { buf[(size_t) w * 2] = l; buf[(size_t) w * 2 + 1] = r; w = (w + 1) % size; }
    void read (float d, float& l, float& r) const
    {
        d = std::fmin (std::fmax (d, 1.0f), (float) (size - 2));
        const float pos = (float) w - d;
        const float fl = std::floor (pos);
        const float fr = pos - fl;
        const int a = (((int) fl % size) + size) % size, b = (a + 1) % size;
        l = buf[(size_t) a * 2] + fr * (buf[(size_t) b * 2] - buf[(size_t) a * 2]);
        r = buf[(size_t) a * 2 + 1] + fr * (buf[(size_t) b * 2 + 1] - buf[(size_t) a * 2 + 1]);
    }

private:
    std::vector<float> buf;
    int size = 2, w = 0;
};

// Smear: dark, synced ping-pong echo that blurs the break.
class Smear
{
public:
    void prepare (double sampleRate) { sr = sampleRate; line.prepare ((int) (sr * 2.5)); lp.reset(); }

    void process (float* L, float* R, int n, float amount, double bpm)
    {
        if (amount <= 0.0f) return;
        const float d = (float) (0.75 * 60.0 / std::fmax (bpm, 40.0) * sr); // dotted 1/8
        lp.setLP (sr, 2500, 0.6f);
        for (int i = 0; i < n; ++i)
        {
            float l, r;
            line.read (d, l, r);
            const float fb = 0.35f + 0.25f * amount;
            line.write (lp.process (L[i] + r * fb, 0), lp.process (R[i] + l * fb, 1)); // cross-feed = ping-pong
            L[i] += l * amount * 0.6f;
            R[i] += r * amount * 0.6f;
        }
    }

private:
    double sr = 48000;
    Delay line;
    Biquad lp;
};

// Tape Haze for the wet bus: slow wow + flutter on a short delay, then darkening.
class TapeHaze
{
public:
    void prepare (double sampleRate) { sr = sampleRate; line.prepare ((int) (sr * 0.05)); lp.reset(); a = b = 0; }

    void process (float* L, float* R, int n, float amount)
    {
        if (amount <= 0.0f) return;
        lp.setLP (sr, 12000.0f - 8500.0f * amount, 0.6f);
        const float base = (float) (0.012 * sr);
        for (int i = 0; i < n; ++i)
        {
            a += 0.55f / (float) sr; b += 6.3f / (float) sr;
            if (a > 1) a -= 1;
            if (b > 1) b -= 1;
            const float d = base * (1.0f + amount * (0.4f * std::sin (2 * kPi * a) + 0.06f * std::sin (2 * kPi * b)));
            line.write (L[i], R[i]);
            float l, r;
            line.read (d, l, r);
            L[i] = lp.process (l, 0);
            R[i] = lp.process (r, 1);
        }
    }

private:
    double sr = 48000;
    Delay line;
    Biquad lp;
    float a = 0, b = 0;
};

// Wall: fuzz that turns the kit into a wall of noise. amount 0..1.
inline float wall (float x, float amount)
{
    const float g = 1.0f + 40.0f * amount;
    return x + amount * (std::tanh (x * g) * 0.6f - x);
}

// Underworld crunch: bit depth + sample-and-hold downsampling.
struct Crusher
{
    float holdL = 0, holdR = 0, count = 0;

    void process (float* L, float* R, int n, float bits, float factor)
    {
        const float q = std::pow (2.0f, bits - 1.0f);
        for (int i = 0; i < n; ++i)
        {
            if ((count += 1.0f) >= factor)
            {
                count -= factor;
                holdL = std::round (L[i] * q) / q;
                holdR = std::round (R[i] * q) / q;
            }
            L[i] = holdL; R[i] = holdR;
        }
    }
};
} // namespace jz
