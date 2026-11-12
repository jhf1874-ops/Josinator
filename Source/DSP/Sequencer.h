#pragma once
// Step sequencer with the "Flesh" humanizer: every bar is re-cast so no two loops are identical.
#include "Breaks.h"
#include <cstdint>

namespace jz
{
struct Humanize
{
    float swing = 0;     // 0..1: delays odd 16ths by up to half a step
    float dragMs = 0;    // snare/ghost/rim sit behind the beat
    float drift = 0;     // velocity randomness 0..1
    float ghosts = 0;    // extra ghost-note density 0..1
    float fills = 0;     // chance of a fill at the end of a bar
    float wanderMs = 0;  // random lateness per hit (only ever late, so no lookahead is needed)
    bool halfTime = false;
};

struct BlockHit { int offset; int lane; float vel; float tune; };

class Sequencer
{
public:
    static constexpr int kMaxOut = 128;

    void prepare (double sampleRate) { sr = sampleRate; now = 0; pending = 0; lastBar = INT64_MIN; play = -1; }

    // Schedules this block's steps, then returns (in `out`, sorted) every hit that lands in it.
    int process (int n, double ppq, double bpm, bool running, const Pattern& pat, const Humanize& h, BlockHit* out)
    {
        play = -1;
        if (running && bpm > 1.0)
        {
            const double stepBeats = h.halfTime ? 0.5 : 0.25;
            const double spb = 60.0 * sr / bpm; // samples per beat
            const double ppqEnd = ppq + n / spb;
            // Decide by rounded sample position so a step on a block edge lands in exactly one block.
            for (int64_t k = (int64_t) std::floor (ppq / stepBeats) - 1; (double) k * stepBeats <= ppqEnd + stepBeats; ++k)
            {
                const int64_t at = std::llround ((double) k * stepBeats * spb - ppq * spb);
                if (at < 0 || at >= n) continue;
                const int step = (int) (((k % kSteps) + kSteps) % kSteps);
                const int64_t barNo = (k - step) / kSteps;
                if (barNo != lastBar || step == 0) { rollBar (pat, h); lastBar = barNo; } // step 0 also catches loop jumps
                schedule (now + at, step, stepBeats * spb, h);
            }
            play = (int) (((int64_t) std::floor (ppq / stepBeats) % kSteps + kSteps) % kSteps);
        }

        int count = 0;
        for (int i = 0; i < pending;)
        {
            if (queue[i].time < now + n)
            {
                if (count < kMaxOut)
                    out[count++] = { (int) std::max<int64_t> (0, queue[i].time - now), queue[i].lane, queue[i].vel, queue[i].tune };
                queue[i] = queue[--pending];
            }
            else ++i;
        }
        for (int i = 1; i < count; ++i) // insertion sort by offset
            for (int j = i; j > 0 && out[j].offset < out[j - 1].offset; --j) std::swap (out[j], out[j - 1]);
        now += n;
        return count;
    }

    int currentStep() const { return play; }
    const Pattern& currentBar() const { return bar; }

private:
    struct Pending { int64_t time; int lane; float vel; float tune; };

    void rollBar (const Pattern& pat, const Humanize& h)
    {
        bar = pat;
        for (int s = 0; s < kSteps; ++s)
            if (s % 4 != 0 && ! bar.v[kSnare][s] && ! bar.v[kGhost][s] && rng.next01() < h.ghosts * 0.25f)
                bar.v[kGhost][s] = 1;

        if (rng.next01() < h.fills * 0.5f)
        {
            static const char* const fills[3] = { "..23", "2233", "1123" };
            const char* f = fills[rng.nextInt (3)];
            const bool onSnare = rng.next01() < 0.35f;
            for (int i = 0; i < 4; ++i)
            {
                const int s = 12 + i;
                if (f[i] == '.') continue;
                bar.v[onSnare ? kGhost : kToms][s] = (uint8_t) (f[i] - '0');
                if (rng.next01() < 0.5f) bar.v[kHats][s] = 0;
            }
        }
    }

    void schedule (int64_t at, int step, double stepSamples, const Humanize& h)
    {
        static constexpr float level[4] = { 0.0f, 0.32f, 0.62f, 1.0f };
        for (int lane = 0; lane < kNumLanes; ++lane)
        {
            const int v = bar.v[lane][step];
            if (v == 0 || pending >= kMaxQueue) continue;
            float vel = level[v] * (1.0f + h.drift * 0.35f * rng.nextBi());
            if (lane == kGhost) vel *= 0.55f;
            double delay = (step & 1) ? h.swing * stepSamples * 0.5 : 0.0;
            if (lane == kSnare || lane == kGhost || lane == kRim)
                delay += h.dragMs * 0.001 * sr * (lane == kSnare ? 1.0 : 0.6);
            delay += h.wanderMs * 0.001 * sr * rng.next01();
            const float tune = lane == kToms ? 1.25f - 0.5f * (float) step / (kSteps - 1) : 1.0f;
            queue[pending++] = { at + (int64_t) delay, lane, std::fmin (std::fmax (vel, 0.05f), 1.0f), tune };
        }
    }

    static constexpr int kMaxQueue = 256;
    double sr = 48000;
    int64_t now = 0, lastBar = INT64_MIN;
    int pending = 0, play = -1;
    Pending queue[kMaxQueue] {};
    Pattern bar;
    Rng rng;
};
} // namespace jz
