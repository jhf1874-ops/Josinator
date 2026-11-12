// Standalone DSP self-test (no JUCE). Run via: ctest --test-dir build -C Release
#include "Sequencer.h"
#include "Veil.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>

static void sane (const float* v, int n, const char* what)
{
    for (int i = 0; i < n; ++i)
        if (! std::isfinite (v[i]) || std::fabs (v[i]) > 8.0f) { std::printf ("FAIL: %s produced %f\n", what, v[i]); std::exit (1); }
}

static int hitsIn (const jz::Pattern& p)
{
    int c = 0;
    for (auto& lane : p.v) for (auto x : lane) c += x ? 1 : 0;
    return c;
}

int main()
{
    const double sr = 48000; const int block = 512;
    std::vector<float> L (block), R (block);

    // 1) Every voice in every kit sounds, stays finite, and dies away.
    for (int k = 0; k < jz::kNumKits; ++k)
        for (int lane = 0; lane < jz::kNumLanes; ++lane)
        {
            jz::DrumKit kit; kit.prepare (sr);
            kit.trigger (lane, 1.0f, k, 1.0f, 1.0f);
            float peak = 0;
            for (int b = 0; b < 600; ++b) // ~6.4 s
            {
                std::fill (L.begin(), L.end(), 0.0f); std::fill (R.begin(), R.end(), 0.0f);
                kit.render (L.data(), R.data(), block);
                sane (L.data(), block, "drum voice");
                for (float x : L) peak = std::fmax (peak, std::fabs (x));
            }
            assert (peak > 0.01f);
            assert (std::fabs (L[block - 1]) < 1e-4f);
        }

    // 2) With humanize off, a bar of the host transport fires exactly the pattern's hits, on the grid.
    for (int t = 0; t < jz::kNumTomes; ++t)
    {
        jz::Sequencer seq; seq.prepare (sr);
        const auto pat = jz::tomePattern (t);
        jz::Humanize h;
        jz::BlockHit out[jz::Sequencer::kMaxOut];
        const double bpm = 90.0, spb = 60.0 * sr / bpm;
        int hits = 0;
        const int blocks = (int) std::ceil (4.0 * spb / block); // exactly one bar
        double ppq = 0;
        for (int b = 0; b < blocks; ++b)
        {
            const int n = std::min (block, (int) std::ceil (4.0 * spb - b * block));
            hits += seq.process (n, ppq, bpm, true, pat, h, out);
            ppq += n / spb;
        }
        if (hits != hitsIn (pat)) std::fprintf (stderr, "tome %d: %d vs %d\n", t, hits, hitsIn (pat));
        assert (hits == hitsIn (pat));
    }

    // 3) Swing makes off-steps late; a stopped transport is silent.
    {
        jz::Sequencer seq; seq.prepare (sr);
        jz::Pattern p; p.v[jz::kHats][1] = 3;
        jz::Humanize h; h.swing = 1.0f;
        jz::BlockHit out[jz::Sequencer::kMaxOut];
        const double bpm = 120, spb = 60.0 * sr / bpm;
        int firedAt = -1;
        for (int b = 0; b < 40 && firedAt < 0; ++b)
            if (seq.process (block, b * block / spb, bpm, true, p, h, out) > 0) firedAt = b * block + out[0].offset;
        const int grid = (int) (0.25 * spb), expected = grid + (int) (0.125 * spb);
        assert (std::abs (firedAt - expected) <= 1);

        jz::Sequencer idle; idle.prepare (sr);
        assert (idle.process (block, 0, bpm, false, p, h, out) == 0);
    }

    // 4) Summon keeps the backbeat and always yields a playable pattern.
    {
        jz::Rng r;
        for (int i = 0; i < 200; ++i)
        {
            const auto p = jz::summon (i % jz::kNumTomes, r);
            const auto base = jz::tomePattern (i % jz::kNumTomes);
            assert (std::memcmp (p.v[jz::kSnare], base.v[jz::kSnare], jz::kSteps) == 0);
            assert (p.v[jz::kKick][0] == base.v[jz::kKick][0]);
        }
    }

    // 5) Veil + Underworld at extreme settings stay finite.
    {
        jz::ReverseBloom bloom; bloom.prepare (sr);
        jz::OctaveUp shim; shim.prepare (sr);
        jz::Smear smear; smear.prepare (sr);
        jz::TapeHaze haze; haze.prepare (sr);
        jz::Crusher crush;
        std::vector<float> wL (block), wR (block), sL (block), sR (block);
        jz::Rng r;
        for (int b = 0; b < 800; ++b)
        {
            for (int i = 0; i < block; ++i) { L[(size_t) i] = r.nextBi(); R[(size_t) i] = r.nextBi(); }
            std::fill (wL.begin(), wL.end(), 0.0f); std::fill (wR.begin(), wR.end(), 0.0f);
            bloom.process (L.data(), R.data(), wL.data(), wR.data(), block, 3000 + b * 37);
            shim.process (wL.data(), wR.data(), sL.data(), sR.data(), block);
            haze.process (wL.data(), wR.data(), block, 1.0f);
            smear.process (L.data(), R.data(), block, 1.0f, 70.0);
            crush.process (L.data(), R.data(), block, 4.0f, 20.0f);
            for (int i = 0; i < block; ++i) L[(size_t) i] = jz::wall (L[(size_t) i], 1.0f);
            sane (L.data(), block, "underworld"); sane (wL.data(), block, "bloom/haze"); sane (sL.data(), block, "shimmer");
        }
    }

    std::puts ("ALL DSP SELF-TESTS PASSED");
    return 0;
}
