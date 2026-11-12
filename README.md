# Josinator

A gothic, retro shoegaze drum machine for FL Studio on Mac (VST3 + AU instrument, Apple Silicon and Intel).
Human-feeling breaks with a shoegaze veil: swung, dragging, ghosted drums drowned in reverse bloom, shimmer and fuzz, plus an Underworld section for abstract and experimental hip-hop.

## Install (no Xcode needed)

1. Open this repo's **Actions** tab, click the latest green **build-mac** run, and download **Josinator-mac** at the bottom.
2. Unzip it, then double-click the **Josinator-mac.zip** inside it. You get a `Josinator` folder holding two bundles. On a Mac a plugin bundle *is* a folder, so `Josinator.vst3` may look like a plain folder: copy the whole thing.
   - `Josinator.vst3` → `~/Library/Audio/Plug-Ins/VST3/`
   - `Josinator.component` → `~/Library/Audio/Plug-Ins/Components/`
3. macOS quarantines downloaded plugins, so clear that once in Terminal:

```bash
xattr -cr ~/Library/Audio/Plug-Ins/VST3/Josinator.vst3 ~/Library/Audio/Plug-Ins/Components/Josinator.component
```

4. In FL Studio: Options → Manage plugins → **Find installed plugins**, then star Josinator.

### Or build it yourself

With Xcode and CMake (`brew install cmake`) installed, from this folder:

```bash
cmake -B build -G Xcode
cmake --build build --config Release
```

It installs itself into the plugin folders above, and a standalone app lands in `build/Josinator_artefacts/Release/Standalone/`.

## Use it in FL Studio

Load Josinator into the **Channel Rack** as a generator and press play: it follows FL's tempo and song position. The standalone app free-runs at the tome's tempo.

You can also play the kit from the piano roll using General MIDI drum notes: C1 kick, D1 snare, E1 ghost, C#1 rim, F#1 hats, D#2 wash, and toms on F1 to D2.

## What's inside

| Section | Controls | What it does |
|---|---|---|
| **Tomes** | 6 breaks | Original breaks written to capture classic break feels: Drowned Choir (swung), Cathedral Drag (late snare), Ashfall Shuffle, Wraith Two-Step (half-time), Velvet Ruin (straight), Moth Lantern (ghosted). Clicking a tome loads it into the current pattern and sets its swing. |
| **Reliquary** | Kit | 4 synthesised kits: Bonechapel (dusty room), Velvet Crypt (soft, dark), Rust Abbey (hard, bright), Moth Room (lo-fi). |
| **The Ritual** | 7 lanes × 16 steps, patterns I–IV, Clear, **Summon** | Kick, Snare, Ghost, Hats, Wash, Toms, Rim. Click to place a hit, click again to erase, drag up or down for velocity (3 levels), or right-click to cycle velocity. **Summon** writes a fresh variation of the current tome. |
| **Flesh · Humanize** | Swing, Drag, Drift, Ghosts, Fills, Wander | Every bar is re-cast: swing, a laid-back snare, velocity drift, extra ghost notes, random tom or snare fills, and timing wander. No two loops repeat. |
| **Shoegaze Veil** | Bloom, Shimmer, Wall, Haze, Breath, Nave | Reverse bloom (hits swell in backwards), octave-up shimmer reverb, fuzz wall, tape wow on the wash, kick-ducked "breathing", and a huge cathedral reverb. |
| **Underworld** | Crunch, Chop, Pitch, Lo-Pass, Half-Time, Smear | Bit-crush, sample-rate chop, kit pitch (±12 st), low-pass, half-time steps, and a dark ping-pong smear. |
| **Output** | Width, Mix, Gain | Stereo width, effects mix against the clean kit, level. |

Every knob is automatable in FL. Your patterns are saved with your project.

## Notes

- The sounds are synthesised and the breaks are original, so anything you make with it is clear to release.
- Humanize only ever pushes hits *late* (that's what makes the drag feel human), so nothing needs lookahead.
- DSP self-test: `ctest --test-dir build -C Release`.
- The fonts (UnifrakturMaguntia, Cinzel, VT323) are under the SIL Open Font Licence; see `Resources/Fonts`.
