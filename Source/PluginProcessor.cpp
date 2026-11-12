#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
using Float = juce::AudioParameterFloat;

std::unique_ptr<Float> fparam (const char* id, const char* name, juce::NormalisableRange<float> r,
                               float def, juce::String suffix = {}, int decimals = 1)
{
    return std::make_unique<Float> (juce::ParameterID { id, 1 }, name, r, def,
        juce::AudioParameterFloatAttributes().withLabel (suffix).withStringFromValueFunction (
            [decimals, suffix] (float x, int) { return juce::String (x, decimals) + suffix; }));
}

std::unique_ptr<Float> pct (const char* id, const char* name, float def)
{
    return std::make_unique<Float> (juce::ParameterID { id, 1 }, name, juce::NormalisableRange<float> (0.f, 1.f), def,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (
            [] (float x, int) { return juce::String (juce::roundToInt (x * 100.f)) + "%"; }));
}

std::unique_ptr<juce::AudioParameterBool> on (const char* id, const char* name, bool def)
{
    return std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, def);
}

// GM-style drum notes so the kit can also be played from FL's piano roll.
int laneForNote (int note, float& tune)
{
    tune = 1.0f;
    switch (note)
    {
        case 35: case 36: return jz::kKick;
        case 38: return jz::kSnare;
        case 40: return jz::kGhost;
        case 37: case 39: return jz::kRim;
        case 42: case 44: case 46: return jz::kHats;
        case 49: case 51: case 52: case 55: case 57: case 59: return jz::kWash;
        case 41: case 43: case 45: case 47: case 48: case 50:
            tune = std::pow (2.0f, (float) (note - 45) / 12.0f);
            return jz::kToms;
        default: break;
    }
    // FL's preview, typing keyboard and default piano-roll notes sit around C5 (60):
    // the octave from C5 plays the kit on white keys (black keys repeat the key below).
    if (note >= 60 && note < 72)
    {
        static constexpr int lanes[12] = { jz::kKick, jz::kKick, jz::kSnare, jz::kSnare, jz::kGhost, jz::kHats,
                                           jz::kHats, jz::kWash, jz::kWash, jz::kToms, jz::kToms, jz::kRim };
        return lanes[note - 60];
    }
    return -1;
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout JosinatorProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    using R = juce::NormalisableRange<float>;

    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "slot", 1 }, "Pattern", juce::StringArray { "I", "II", "III", "IV" }, 0));
    juce::StringArray kits;
    for (auto& k : jz::kKits) kits.add (k.name);
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "kit", 1 }, "Kit", kits, 0));

    // Flesh: humanize
    l.add (pct ("swing", "Swing", 0.55f),
           fparam ("drag", "Drag", R (0.f, 40.f, 0.1f), 12.f, "ms", 0),
           pct ("drift", "Velocity Drift", 0.18f),
           pct ("ghosts", "Ghosts", 0.4f),
           pct ("fills", "Fills", 0.25f),
           fparam ("wander", "Wander", R (0.f, 20.f, 0.1f), 6.f, "ms", 0));

    // Shoegaze Veil
    l.add (on ("veil_on", "Veil On", true),
           pct ("bloom", "Reverse Bloom", 0.6f),
           pct ("shimmer", "Shimmer", 0.45f),
           pct ("wall", "Wall", 0.5f),
           pct ("haze", "Haze", 0.35f),
           pct ("breath", "Breath", 0.4f),
           pct ("nave", "Nave", 0.75f));

    // Underworld
    l.add (on ("under_on", "Underworld On", true),
           pct ("crunch", "Crunch", 0.4f),
           pct ("chop", "Chop", 0.3f),
           fparam ("pitch", "Pitch", R (-12.f, 12.f, 0.1f), -3.f, "st"),
           fparam ("lopass", "Lo-Pass", R (300.f, 20000.f, 1.f, 0.3f), 6800.f, "Hz", 0),
           on ("halftime", "Half-Time", false),
           pct ("smear", "Smear", 0.2f));

    // Output
    l.add (fparam ("width", "Width", R (0.f, 2.f, 0.01f), 1.4f, "", 2),
           pct ("mix", "Mix", 1.0f),
           fparam ("gain", "Gain", R (-24.f, 12.f, 0.1f), 0.f, "dB"));
    return l;
}

JosinatorProcessor::JosinatorProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    for (int s = 0; s < kSlots; ++s)
        setPattern (s, jz::tomePattern (s)); // four different starting breaks
    uiRng.s = (uint32_t) juce::Time::currentTimeMillis() | 1u;
}

bool JosinatorProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void JosinatorProcessor::setPattern (int slot, const jz::Pattern& p)
{
    for (int l = 0; l < jz::kNumLanes; ++l)
        for (int s = 0; s < jz::kSteps; ++s)
            setCell (slot, l, s, p.v[l][s]);
}

void JosinatorProcessor::setParam (const juce::String& id, float plainValue)
{
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (plainValue));
        p->endChangeGesture();
    }
}

void JosinatorProcessor::loadTome (int t)
{
    t = juce::jlimit (0, jz::kNumTomes - 1, t);
    tome = t;
    setPattern (currentSlot(), jz::tomePattern (t));
    setParam ("swing", jz::kTomes[t].swing);
    setParam ("halftime", jz::kTomes[t].halfTime ? 1.0f : 0.0f);
}

void JosinatorProcessor::summonBreak() { setPattern (currentSlot(), jz::summon (tome, uiRng)); }
void JosinatorProcessor::clearSlot()   { setPattern (currentSlot(), jz::Pattern {}); }

void JosinatorProcessor::prepareToPlay (double sr, int samplesPerBlock)
{
    seq.prepare (sr);
    kit.prepare (sr);
    smear.prepare (sr);
    bloom.prepare (sr);
    shimmer.prepare (sr);
    haze.prepare (sr);
    lowpass.reset();
    crusher = {};
    reverb.setSampleRate (sr);
    reverb.reset();
    for (auto* b : { &dry, &wet, &bloomBuf, &revBuf, &shimBuf })
    {
        b->setSize (2, samplesPerBlock);
        b->clear();
    }
    breathEnv = 0.0f;
}

void JosinatorProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    const int n = buffer.getNumSamples();
    if (n == 0 || buffer.getNumChannels() < 2) return;
    const double sr = getSampleRate();
    for (auto* b : { &dry, &wet, &bloomBuf, &revBuf, &shimBuf })
        if (b->getNumSamples() < n) b->setSize (2, n, true, true, true); // host sent a bigger block than promised

    // Transport: follow the host; the standalone app free-runs at the tome's tempo.
    double bpm = jz::kTomes[tome.load()].bpm, ppq = freePpq;
    bool running = wrapperType == wrapperType_Standalone;
    if (! running)
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
            {
                if (auto b = pos->getBpm()) bpm = *b;
                running = pos->getIsPlaying();
                if (auto p = pos->getPpqPosition()) ppq = *p;
            }
    freePpq = ppq + n * bpm / (60.0 * sr);
    shownBpm = (float) bpm;

    // Sequencer hits + MIDI notes, merged in time order.
    jz::Pattern pat;
    const int slot = currentSlot();
    for (int l = 0; l < jz::kNumLanes; ++l)
        for (int s = 0; s < jz::kSteps; ++s)
            pat.v[l][s] = (uint8_t) cell (slot, l, s);

    jz::Humanize h;
    h.swing = v ("swing"); h.dragMs = v ("drag"); h.drift = v ("drift");
    h.ghosts = v ("ghosts"); h.fills = v ("fills"); h.wanderMs = v ("wander");
    h.halfTime = v ("halftime") > 0.5f;

    jz::BlockHit hits[jz::Sequencer::kMaxOut + 64];
    int count = seq.process (n, ppq, bpm, running, pat, h, hits);
    playStep = seq.currentStep();
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        float tune;
        const int lane = m.isNoteOn() ? laneForNote (m.getNoteNumber(), tune) : -1;
        if (lane >= 0 && count < (int) std::size (hits))
            hits[count++] = { meta.samplePosition, lane, m.getFloatVelocity(), tune };
    }
    std::stable_sort (hits, hits + count, [] (const auto& a, const auto& b) { return a.offset < b.offset; });

    const bool under = v ("under_on") > 0.5f, veil = v ("veil_on") > 0.5f;
    const float pitch = under ? std::pow (2.0f, v ("pitch") / 12.0f) : 1.0f;
    const int kitIndex = (int) v ("kit");

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);
    float kickAt[jz::Sequencer::kMaxOut + 64];
    int kicks = 0, done = 0;
    for (int i = 0; i < count; ++i)
    {
        const int at = juce::jlimit (0, n, hits[i].offset);
        kit.render (L + done, R + done, at - done);
        done = at;
        kit.trigger (hits[i].lane, hits[i].vel, kitIndex, pitch, hits[i].tune);
        if (hits[i].lane == jz::kKick) kickAt[kicks++] = (float) at;
    }
    kit.render (L + done, R + done, n - done);

    dry.copyFrom (0, 0, L, n);
    dry.copyFrom (1, 0, R, n);

    // Underworld: crunch, chop, lo-pass, smear
    if (under)
    {
        const float crunch = v ("crunch"), chop = v ("chop");
        if (crunch > 0.0f || chop > 0.0f)
            crusher.process (L, R, n, 16.0f - 12.0f * crunch, 1.0f + 23.0f * chop * chop);
        lowpass.setLP (sr, v ("lopass"), 0.8f);
        for (int i = 0; i < n; ++i) { L[i] = lowpass.process (L[i], 0); R[i] = lowpass.process (R[i], 1); }
        smear.process (L, R, n, v ("smear"), bpm);
    }

    // Shoegaze Veil: wall of fuzz on the kit, then a wet bus of reverse bloom + shimmering nave.
    if (veil)
    {
        const float w = v ("wall"), bl = v ("bloom"), sh = v ("shimmer"), nave = v ("nave");
        for (int i = 0; i < n; ++i) { L[i] = jz::wall (L[i], w); R[i] = jz::wall (R[i], w); }

        float* bL = bloomBuf.getWritePointer (0); float* bR = bloomBuf.getWritePointer (1);
        float* rL = revBuf.getWritePointer (0);   float* rR = revBuf.getWritePointer (1);
        float* sL = shimBuf.getWritePointer (0);  float* sR = shimBuf.getWritePointer (1);
        float* wL = wet.getWritePointer (0);      float* wR = wet.getWritePointer (1);
        std::fill (bL, bL + n, 0.0f); std::fill (bR, bR + n, 0.0f);
        bloom.process (L, R, bL, bR, n, (int) (60.0 / bpm * sr));

        for (int i = 0; i < n; ++i) // reverb input: kit + bloom + last block's octave-up tail (the shimmer loop)
        {
            rL[i] = 0.35f * L[i] + bl * bL[i] + sh * 0.8f * sL[i];
            rR[i] = 0.35f * R[i] + bl * bR[i] + sh * 0.8f * sR[i];
        }
        juce::Reverb::Parameters p;
        p.roomSize = 0.55f + 0.44f * nave;
        p.damping = 0.35f;
        p.wetLevel = 1.0f;
        p.dryLevel = 0.0f;
        p.width = 1.0f;
        reverb.setParameters (p);
        reverb.processStereo (rL, rR, n);
        shimmer.process (rL, rR, sL, sR, n);

        for (int i = 0; i < n; ++i)
        {
            wL[i] = rL[i] * nave + bL[i] * bl * 0.6f + sL[i] * sh * 0.5f;
            wR[i] = rR[i] * nave + bR[i] * bl * 0.6f + sR[i] * sh * 0.5f;
        }
        haze.process (wL, wR, n, v ("haze"));

        // Breath: the wet bus ducks on every kick and swells back.
        const float breath = v ("breath");
        const float release = std::exp (-1.0f / (float) (0.25 * 60.0 / bpm * sr));
        int k = 0;
        for (int i = 0; i < n; ++i)
        {
            while (k < kicks && kickAt[k] <= (float) i) { breathEnv = 1.0f; ++k; }
            const float g = 1.0f - breath * 0.9f * breathEnv;
            breathEnv *= release;
            L[i] += wL[i] * g;
            R[i] += wR[i] * g;
        }
    }
    else
        shimBuf.clear();

    // Output: width, mix against the clean kit, gain
    const float width = v ("width"), mix = v ("mix");
    const float gain = juce::Decibels::decibelsToGain (v ("gain"));
    const float* dL = dry.getReadPointer (0);
    const float* dR = dry.getReadPointer (1);
    for (int i = 0; i < n; ++i)
    {
        const float m = 0.5f * (L[i] + R[i]), s = 0.5f * (L[i] - R[i]) * width;
        L[i] = (dL[i] + mix * ((m + s) - dL[i])) * gain;
        R[i] = (dR[i] + mix * ((m - s) - dR[i])) * gain;
    }
}

void JosinatorProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    juce::String pats;
    for (auto& c : cells) pats << juce::String::charToString ((juce::juce_wchar) ('0' + c.load()));
    state.setProperty ("patterns", pats, nullptr);
    state.setProperty ("tome", tome.load(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void JosinatorProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            const auto pats = tree.getProperty ("patterns").toString();
            if (pats.length() == (int) cells.size())
                for (int i = 0; i < pats.length(); ++i)
                    cells[(size_t) i] = (uint8_t) juce::jlimit (0, 3, (int) (pats[i] - '0'));
            tome = juce::jlimit (0, jz::kNumTomes - 1, (int) tree.getProperty ("tome", 0));
            apvts.replaceState (tree);
        }
}

juce::AudioProcessorEditor* JosinatorProcessor::createEditor() { return new JosinatorEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new JosinatorProcessor(); }
