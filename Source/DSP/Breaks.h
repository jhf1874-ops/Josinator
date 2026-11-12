#pragma once
// The Tomes: original break patterns written to capture the *feel* of classic breaks
// (swing, ghost notes, lazy snares). No recordings are sampled or transcribed.
#include "Drums.h"
#include <cstring>

namespace jz
{
constexpr int kSteps = 16;

// Velocity per cell: 0 = empty, 1 = soft, 2 = medium, 3 = hard.
struct Pattern
{
    uint8_t v[kNumLanes][kSteps] {};

    static Pattern fromStrings (const char* const lanes[kNumLanes])
    {
        Pattern p;
        for (int l = 0; l < kNumLanes; ++l)
            for (int s = 0; s < kSteps && lanes[l][s]; ++s)
                p.v[l][s] = (uint8_t) (lanes[l][s] >= '1' && lanes[l][s] <= '3' ? lanes[l][s] - '0' : 0);
        return p;
    }
    bool operator== (const Pattern& o) const { return std::memcmp (v, o.v, sizeof v) == 0; }
};

struct Tome
{
    const char* name;
    const char* feel;
    float bpm;      // used when running standalone with no host tempo
    float swing;    // 0..1, set on load
    bool halfTime;
    const char* lanes[kNumLanes]; // Kick, Snare, Ghost, Hats, Wash, Toms, Rim
};

constexpr int kNumTomes = 6;
inline const Tome kTomes[kNumTomes] = {
    { "Drowned Choir", "88 SWUNG", 88, 0.55f, false,
      { "3.2...3...2..1..", "....3.......3...", "......1..1....1.", "3121312131213121",
        "3...............", "................", "...........1...." } },
    { "Cathedral Drag", "84 LATE SNR", 84, 0.35f, false,
      { "3......2..3.....", "....3.......3..1", "..1....1......1.", "2.2.3.2.2.2.3.2.",
        "2.......2.......", "................", "..1.......1....." } },
    { "Ashfall Shuffle", "94 SHUFFLE", 94, 0.70f, false,
      { "3..2..2...3..2..", "....3.......3...", ".1.1...1.1.1...1", "3121312131213121",
        "................", "..............23", "................" } },
    { "Wraith Two-Step", "80 HALF-TIME", 80, 0.45f, true,
      { "3.....2...3.....", "........3.......", "...1......1..1..", "3.2.3.2.3.2.3.21",
        "3...............", "................", "......1.......1." } },
    { "Velvet Ruin", "102 STRAIGHT", 102, 0.0f, false,
      { "3...3.2.3...3..2", "....3.......3...", "................", "2222322222223222",
        "2.1.2.1.2.1.2.1.", "................", "................" } },
    { "Moth Lantern", "90 GHOSTED", 90, 0.50f, false,
      { "3.....3..2......", "....3.......3...", "1.1..11.1.1...11", "3.1.3.1.3.1.3.1.",
        "................", "...........2....", "...1.....1......" } },
};

inline Pattern tomePattern (int i) { return Pattern::fromStrings (kTomes[i < 0 || i >= kNumTomes ? 0 : i].lanes); }

// "Summon Break": a fresh variation of a tome that keeps its backbeat.
inline Pattern summon (int tome, Rng& r)
{
    Pattern p = tomePattern (tome);
    auto& k = p.v[kKick];
    // move or add kicks on the off-steps, never touching the downbeat
    for (int s = 1; s < kSteps; ++s)
        if (k[s] && r.next01() < 0.3f)
        {
            const int to = s + (r.next01() < 0.5f ? -1 : 1);
            if (to > 0 && to < kSteps && ! k[to]) { k[to] = k[s]; k[s] = 0; }
        }
    if (r.next01() < 0.5f) { const int s = 1 + r.nextInt (kSteps - 1); if (! k[s] && s != 4 && s != 12) k[s] = 2; }

    // reroll ghosts around (not on) the backbeat
    for (int s = 0; s < kSteps; ++s)
        if (! p.v[kSnare][s] && s % 4 != 0)
            p.v[kGhost][s] = r.next01() < 0.28f ? 1 : 0;

    // hat accents and the odd dropped hat
    for (int s = 0; s < kSteps; ++s)
        if (p.v[kHats][s])
            p.v[kHats][s] = (uint8_t) (r.next01() < 0.08f ? 0 : 1 + r.nextInt (3));

    if (r.next01() < 0.4f) p.v[kWash][r.next01() < 0.5f ? 0 : 8] = 3;
    if (r.next01() < 0.35f) { p.v[kToms][14] = 2; p.v[kToms][15] = 3; }
    for (int s = 0; s < kSteps; ++s)
        if (! p.v[kRim][s] && r.next01() < 0.06f) p.v[kRim][s] = 1;
    return p;
}
} // namespace jz
