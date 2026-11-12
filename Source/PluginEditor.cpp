#include "PluginEditor.h"

namespace
{
constexpr int kW = 1200, kH = 820;
const char* kRoman[] = { "I", "II", "III", "IV" };
}

JosinatorEditor::JosinatorEditor (JosinatorProcessor& p)
    : AudioProcessorEditor (&p), proc (p), tomes (p), grid (p)
{
    setLookAndFeel (&lnf);
    auto& s = proc.apvts;

    addAndMakeVisible (tomes);
    grid.setColour (juce::Slider::rotarySliderFillColourId, gothic::blood);
    addAndMakeVisible (grid);

    for (auto& k : jz::kKits) kitBox.addItem (juce::String ("KIT: ") + juce::String (k.name).toUpperCase(), kitBox.getNumItems() + 1);
    addAndMakeVisible (kitBox);
    kitAttachment = std::make_unique<APVTS::ComboBoxAttachment> (s, "kit", kitBox);

    for (int i = 0; i < JosinatorProcessor::kSlots; ++i)
    {
        slotButtons[i].setButtonText (kRoman[i]);
        slotButtons[i].setColour (juce::TextButton::buttonColourId, gothic::keyTop);
        slotButtons[i].setTooltip ("Pattern " + juce::String (kRoman[i]));
        slotButtons[i].onClick = [this, i] { proc.setParam ("slot", (float) i); };
        addAndMakeVisible (slotButtons[i]);
    }

    clearButton.setColour (juce::TextButton::buttonColourId, gothic::keyTop);
    clearButton.setTooltip ("Erase this pattern");
    clearButton.onClick = [this] { proc.clearSlot(); grid.repaint(); };
    summonButton.setColour (juce::TextButton::buttonColourId, gothic::blood);
    summonButton.setColour (juce::TextButton::textColourOffId, gothic::ground);
    summonButton.setTooltip ("Summon a fresh variation of the current tome");
    summonButton.onClick = [this] { proc.summonBreak(); grid.repaint(); };
    addAndMakeVisible (clearButton);
    addAndMakeVisible (summonButton);

    for (auto [id, name] : std::vector<std::pair<const char*, const char*>> {
             { "swing", "Swing" }, { "drag", "Drag" }, { "drift", "Drift" },
             { "ghosts", "Ghosts" }, { "fills", "Fills" }, { "wander", "Wander" } })
    {
        fleshKnobs.push_back (knobs.add (new GothicKnob (s, id, name, gothic::gold)));
        addAndMakeVisible (fleshKnobs.back());
    }

    halfTime.setColour (juce::ToggleButton::tickColourId, gothic::blood);
    halfTime.setTooltip ("Half-time: every step lasts an 1/8");
    halfTimeAttachment = std::make_unique<APVTS::ButtonAttachment> (s, "halftime", halfTime);
    addAndMakeVisible (halfTime);

    addFx ("SHOEGAZE VEIL", "veil_on", { { "bloom", "Bloom" }, { "shimmer", "Shimmer" }, { "wall", "Wall" },
                                         { "haze", "Haze" }, { "breath", "Breath" }, { "nave", "Nave" } });
    addFx ("UNDERWORLD", "under_on", { { "crunch", "Crunch" }, { "chop", "Chop" }, { "pitch", "Pitch" },
                                       { "lopass", "Lo-Pass" }, { "smear", "Smear" } }, 4);
    addFx ("OUTPUT", "", { { "width", "Width" }, { "mix", "Mix" }, { "gain", "Gain" } });

    // shortcut: fixed window size; add a scaled content component if resizing is wanted.
    setSize (kW, kH);
    startTimerHz (30);
}

JosinatorEditor::~JosinatorEditor() { setLookAndFeel (nullptr); }

void JosinatorEditor::addFx (const juce::String& title, const juce::String& onId,
                             std::vector<std::pair<juce::String, juce::String>> defs, int toggleAt)
{
    FxPanel f;
    f.title = title;
    if (onId.isNotEmpty())
    {
        f.power = std::make_unique<juce::ToggleButton>();
        f.power->setColour (juce::ToggleButton::tickColourId, gothic::blood);
        f.power->setTooltip (title + " on/off");
        addAndMakeVisible (*f.power);
        f.powerAttachment = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, onId, *f.power);
    }
    for (auto& [id, name] : defs)
    {
        if ((int) f.cells.size() == toggleAt) f.cells.push_back (&halfTime);
        auto* k = knobs.add (new GothicKnob (proc.apvts, id, name, gothic::blood));
        addAndMakeVisible (k);
        f.cells.push_back (k);
    }
    fx.push_back (std::move (f));
}

void JosinatorEditor::timerCallback()
{
    const int step = proc.playStep.load(), slot = proc.currentSlot();
    if (step != lastStep || slot != lastSlot)
    {
        lastStep = step; lastSlot = slot;
        grid.repaint();
        repaint (lcdArea.getUnion (slotButtons[0].getBounds().getUnion (slotButtons[3].getBounds()).expanded (0, 16)));
    }
    tomes.repaint();
}

void JosinatorEditor::drawPanel (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title, bool arched) const
{
    using namespace gothic;
    auto f = r.toFloat();
    juce::Path p;
    if (arched)
    {
        // pointed gothic arch over the panel
        const float rise = 60.0f;
        p.startNewSubPath (f.getX(), f.getBottom());
        p.lineTo (f.getX(), f.getY() + rise);
        p.quadraticTo (f.getX(), f.getY() + rise * 0.2f, f.getCentreX(), f.getY());
        p.quadraticTo (f.getRight(), f.getY() + rise * 0.2f, f.getRight(), f.getY() + rise);
        p.lineTo (f.getRight(), f.getBottom());
        p.closeSubPath();
    }
    else
        p.addRoundedRectangle (f, 6);
    g.setColour (panel);
    g.fillPath (p);
    g.setColour (edge);
    g.strokePath (p, juce::PathStrokeType (1.0f));

    g.setFont (cinzel (14.0f));
    g.setColour (gold);
    auto t = r.reduced (14, 0).withTop (r.getY() + (arched ? 34 : 10)).withHeight (20);
    g.drawText (title, t, arched ? juce::Justification::centred : juce::Justification::centredLeft);
}

void JosinatorEditor::paint (juce::Graphics& g)
{
    using namespace gothic;
    g.fillAll (ground);

    // screw strips
    for (auto y : { 0, kH - 18 })
    {
        g.setColour (juce::Colour (0xff14101a));
        g.fillRect (0, y, kW, 18);
        screw (g, { 17, (float) y + 9 });
        screw (g, { (float) kW - 17, (float) y + 9 });
    }
    g.setColour (juce::Colour (0xff5a4a2e));
    g.drawHorizontalLine (9, (float) kW / 2 - 110, (float) kW / 2 - 14);
    g.drawHorizontalLine (9, (float) kW / 2 + 14, (float) kW / 2 + 110);
    juce::Path diamond;
    diamond.addPolygon ({ (float) kW / 2, 9 }, 4, 4.5f, juce::MathConstants<float>::pi / 4);
    g.fillPath (diamond);
    g.setFont (cinzel (10.0f));
    g.setColour (juce::Colour (0xff8a7f96));
    g.drawText ("FORGED IN THE NAVE  -  MCMLXXXVI", 0, kH - 18, kW, 18, juce::Justification::centred);

    // header plate
    g.setColour (plate);
    g.fillRect (0, 18, kW, 96);
    g.setColour (edge);
    g.drawHorizontalLine (113, 0, (float) kW);

    // cathedral-window emblem
    {
        const float x = 24, y = 30;
        juce::Path e;
        e.startNewSubPath (x + 3, y + 71); e.lineTo (x + 3, y + 30);
        e.cubicTo (x + 3, y + 14, x + 14, y + 5, x + 27, y + 1);
        e.cubicTo (x + 40, y + 5, x + 51, y + 14, x + 51, y + 30); e.lineTo (x + 51, y + 71);
        e.startNewSubPath (x + 10, y + 71); e.lineTo (x + 10, y + 32);
        e.cubicTo (x + 10, y + 20, x + 18, y + 12, x + 27, y + 8);
        e.cubicTo (x + 36, y + 12, x + 44, y + 20, x + 44, y + 32); e.lineTo (x + 44, y + 71);
        e.addEllipse (x + 17, y + 24, 20, 20);
        e.addEllipse (x + 23, y + 30, 8, 8);
        e.startNewSubPath (x + 27, y + 24); e.lineTo (x + 27, y + 44);
        e.startNewSubPath (x + 17, y + 34); e.lineTo (x + 37, y + 34);
        e.startNewSubPath (x + 10, y + 52); e.lineTo (x + 44, y + 52);
        e.startNewSubPath (x + 10, y + 60); e.lineTo (x + 44, y + 60);
        g.setColour (gold);
        g.strokePath (e, juce::PathStrokeType (1.3f));
    }
    g.setFont (blackletter (50.0f));
    g.setColour (text);
    g.drawText ("Josinator", 96, 28, 360, 54, juce::Justification::centredLeft);
    g.setFont (cinzel (12.0f));
    g.setColour (muted);
    g.drawText ("JSN-XIII   RHYTHM CATHEDRAL", 98, 82, 360, 16, juce::Justification::centredLeft);

    // amber screen
    {
        auto r = lcdArea.toFloat();
        g.setColour (shadow);
        g.fillRoundedRectangle (r, 5);
        g.setColour (lcd);
        g.fillRoundedRectangle (r.reduced (3), 3);
        auto t = lcdArea.reduced (14, 8);
        const int slot = proc.currentSlot();
        auto l1 = t.removeFromTop (t.getHeight() / 2);
        g.setFont (pixel (25.0f));
        g.setColour (amber);
        g.drawText ("PTN " + juce::String (kRoman[juce::jlimit (0, 3, slot)]) + "  "
                        + juce::String (jz::kTomes[proc.tome.load()].name).toUpperCase(), l1, juce::Justification::centredLeft);
        g.drawText (juce::String (proc.shownBpm.load(), 1), l1, juce::Justification::centredRight);
        g.setFont (pixel (19.0f));
        g.setColour (amberDim);
        auto pv = [this] (const char* id) { return proc.apvts.getRawParameterValue (id)->load(); };
        g.drawText ("SWG " + juce::String (juce::roundToInt (pv ("swing") * 100)) + "  DRG +" + juce::String (juce::roundToInt (pv ("drag")))
                        + "  GHST " + juce::String (juce::roundToInt (pv ("ghosts") * 100)), t, juce::Justification::centredLeft);
        g.drawText (proc.wrapperType == juce::AudioProcessor::wrapperType_Standalone ? "FREE RUN" : "BPM SYNC", t, juce::Justification::centredRight);
    }

    // pattern slot LEDs
    for (int i = 0; i < JosinatorProcessor::kSlots; ++i)
    {
        auto b = slotButtons[i].getBounds().toFloat();
        led (g, { b.getCentreX(), b.getY() - 9 }, 8, blood, proc.currentSlot() == i);
    }

    drawPanel (g, tomePanel, "TOMES", true);
    drawPanel (g, kitPanel, "RELIQUARY");
    drawPanel (g, ritualPanel, "THE RITUAL");
    drawPanel (g, fleshPanel, "FLESH  -  HUMANIZE");
    g.setFont (juce::FontOptions (12.0f));
    g.setColour (muted);
    g.drawText ("click to place  -  drag for velocity", ritualPanel.getX() + 150, ritualPanel.getY() + 10, 260, 20, juce::Justification::centredLeft);
    g.drawText ("re-rolls every bar, so no two loops repeat", fleshPanel.getX() + 200, fleshPanel.getY() + 10, 320, 20, juce::Justification::centredLeft);
    g.drawText ("Velvet Crypt  -  Rust Abbey  -  Moth Room", kitPanel.getX() + 14, kitPanel.getBottom() - 30, kitPanel.getWidth() - 20, 20, juce::Justification::centredLeft);

    for (auto& f : fx) drawPanel (g, f.bounds, f.title);
}

void JosinatorEditor::resized()
{
    // header
    lcdArea = { 560, 34, 360, 64 };
    for (int i = 0; i < JosinatorProcessor::kSlots; ++i)
        slotButtons[i].setBounds (952 + i * 52, 56, 44, 44);

    // body columns
    auto body = juce::Rectangle<int> (12, 126, kW - 24, kH - 18 - 12 - 126);
    auto left = body.removeFromLeft (210);
    body.removeFromLeft (12);
    auto right = body.removeFromRight (300);
    body.removeFromRight (12);

    kitPanel = left.removeFromBottom (132);
    left.removeFromBottom (12);
    tomePanel = left;
    tomes.setBounds (tomePanel.reduced (10, 0).withTrimmedTop (66).withTrimmedBottom (10));
    kitBox.setBounds (kitPanel.reduced (12, 0).withTrimmedTop (40).withHeight (44));

    ritualPanel = body.removeFromTop (392);
    body.removeFromTop (12);
    fleshPanel = body;
    auto rp = ritualPanel.reduced (14, 10);
    auto top = rp.removeFromTop (34);
    summonButton.setBounds (top.removeFromRight (120));
    top.removeFromRight (8);
    clearButton.setBounds (top.removeFromRight (76));
    rp.removeFromTop (6);
    grid.setBounds (rp);

    auto fp = fleshPanel.reduced (14, 10).withTrimmedTop (34);
    const int fw = fp.getWidth() / (int) fleshKnobs.size();
    for (auto* k : fleshKnobs) k->setBounds (fp.removeFromLeft (fw).withSizeKeepingCentre (fw - 6, juce::jmin (fp.getHeight(), 120)));

    // FX column: panel heights follow their knob rows
    const int rowH = 96, gap = 12;
    int rowsTotal = 0;
    for (auto& f : fx) rowsTotal += ((int) f.cells.size() + 2) / 3;
    const int spare = (right.getHeight() - gap * ((int) fx.size() - 1) - rowsTotal * rowH - (int) fx.size() * 46) / (int) fx.size();
    for (auto& f : fx)
    {
        const int rows = ((int) f.cells.size() + 2) / 3;
        f.bounds = right.removeFromTop (46 + rows * rowH + spare);
        right.removeFromTop (gap);
        if (f.power) f.power->setBounds (f.bounds.getRight() - 58, f.bounds.getY() + 8, 44, 26);
        auto grid3 = f.bounds.reduced (10, 6).withTrimmedTop (38);
        const int cw = grid3.getWidth() / 3;
        for (size_t i = 0; i < f.cells.size(); ++i)
        {
            auto c = juce::Rectangle<int> (grid3.getX() + (int) (i % 3) * cw, grid3.getY() + (int) (i / 3) * (rowH + spare / juce::jmax (1, rows)), cw, rowH);
            f.cells[i]->setBounds (c.reduced (4, 0));
        }
    }
}
