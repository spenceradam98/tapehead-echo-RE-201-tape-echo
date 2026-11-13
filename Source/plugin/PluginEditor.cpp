#include "PluginEditor.h"

using namespace juce;

namespace
{
const Colour kPlate (0xff151515), kInk (0xffe9e4d6), kInkDim (0xffa9a493), kAmber (0xffffb347), kCaseGreen (0xff3e4a3d),
             kChrome (0xffd9dadc), kRed (0xffe0342a);
constexpr float kStartAngle = MathConstants<float>::pi * -0.75f, kEndAngle = MathConstants<float>::pi * 0.75f;
constexpr int W = 1060, H = 520;
}

// ============================================================================ LookAndFeel
FerroLookAndFeel::FerroLookAndFeel()
{
    setColour (ComboBox::backgroundColourId, Colour (0xff222222));
    setColour (ComboBox::textColourId, kInk);
    setColour (ComboBox::outlineColourId, Colour (0xff555555));
    setColour (ComboBox::arrowColourId, kInkDim);
    setColour (PopupMenu::backgroundColourId, Colour (0xff1c1c1c));
    setColour (PopupMenu::textColourId, kInk);
    setColour (PopupMenu::highlightedBackgroundColourId, kAmber.withAlpha (0.35f));
    setColour (TextButton::textColourOffId, kInk);
}

void FerroLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float start, float end, Slider& s)
{
    const bool isMode = s.getProperties()["mode"];
    const bool small = s.getProperties()["small"];
    auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f);
    const float d = jmin (b.getWidth(), b.getHeight());
    auto c = b.getCentre();
    const float angle = start + pos * (end - start);

    // drop shadow
    g.setColour (Colours::black.withAlpha (0.55f));
    g.fillEllipse (Rectangle<float> (d, d).withCentre (c.translated (2.0f, 4.0f)));

    if (isMode)
    {
        // large black selector with a chrome skirt and a pointer bar
        auto skirt = Rectangle<float> (d, d).withCentre (c);
        g.setGradientFill (ColourGradient (Colour (0xfff2f2f2), skirt.getX(), skirt.getY(), Colour (0xff6d6f73), skirt.getRight(), skirt.getBottom(), false));
        g.fillEllipse (skirt);
        auto body = skirt.reduced (d * 0.12f);
        g.setGradientFill (ColourGradient (Colour (0xff3a3a3a), body.getX(), body.getY(), Colour (0xff050505), body.getRight(), body.getBottom(), false));
        g.fillEllipse (body);
        // knurled edge
        g.setColour (Colours::white.withAlpha (0.06f));
        for (int i = 0; i < 48; ++i)
        {
            const float a = MathConstants<float>::twoPi * i / 48.0f;
            g.drawLine (Line<float> (c.getPointOnCircumference (body.getWidth() * 0.5f, a), c.getPointOnCircumference (body.getWidth() * 0.43f, a)), 1.0f);
        }
        // pointer bar (chicken-head style)
        Path bar;
        bar.addRoundedRectangle (-d * 0.07f, -d * 0.40f, d * 0.14f, d * 0.80f, d * 0.05f);
        g.setGradientFill (ColourGradient (Colour (0xff2b2b2b), c.x - d * 0.2f, c.y, Colour (0xff111111), c.x + d * 0.2f, c.y, false));
        g.fillPath (bar, AffineTransform::rotation (angle).translated (c));
        Path line;
        line.addRectangle (-1.6f, -d * 0.39f, 3.2f, d * 0.22f);
        g.setColour (kInk);
        g.fillPath (line, AffineTransform::rotation (angle).translated (c));
        return;
    }

    // chrome skirt with knurled black cap, white index line (1970s studio knob)
    auto skirt = Rectangle<float> (d, d).withCentre (c);
    g.setGradientFill (ColourGradient (Colour (0xffffffff), skirt.getX(), skirt.getY(), Colour (0xff5e6064), skirt.getRight(), skirt.getBottom(), false));
    g.fillEllipse (skirt);
    g.setColour (Colours::black.withAlpha (0.4f));
    g.drawEllipse (skirt, 1.0f);
    // index mark on the skirt
    g.setColour (Colour (0xff151515));
    g.drawLine (Line<float> (c.getPointOnCircumference (d * 0.5f, angle), c.getPointOnCircumference (d * 0.36f, angle)), small ? 2.0f : 3.0f);

    auto cap = skirt.reduced (d * 0.17f);
    g.setGradientFill (ColourGradient (Colour (0xff404040), cap.getX(), cap.getY(), Colour (0xff080808), cap.getRight(), cap.getBottom(), false));
    g.fillEllipse (cap);
    g.setColour (Colours::white.withAlpha (0.07f));
    const int ridges = small ? 18 : 28;
    for (int i = 0; i < ridges; ++i)
    {
        const float a = angle + MathConstants<float>::twoPi * i / (float) ridges;
        g.drawLine (Line<float> (c.getPointOnCircumference (cap.getWidth() * 0.5f, a), c.getPointOnCircumference (cap.getWidth() * 0.42f, a)), 1.2f);
    }
    auto top = cap.reduced (cap.getWidth() * 0.12f);
    g.setGradientFill (ColourGradient (Colour (0xff2e2e2e), top.getX(), top.getY(), Colour (0xff111111), top.getRight(), top.getBottom(), false));
    g.fillEllipse (top);
    g.setColour (kInk);
    g.drawLine (Line<float> (c.getPointOnCircumference (top.getWidth() * 0.15f, angle), c.getPointOnCircumference (top.getWidth() * 0.5f, angle)), 2.2f);
}

void FerroLookAndFeel::drawToggleButton (Graphics& g, ToggleButton& b, bool, bool)
{
    // bat-handle toggle switch, label above
    auto r = b.getLocalBounds().toFloat();
    const bool on = b.getToggleState();
    auto plate = Rectangle<float> (26.0f, 34.0f).withCentre ({ r.getCentreX(), r.getBottom() - 21.0f });
    g.setColour (Colour (0xff9c9ea2));
    g.fillRoundedRectangle (plate, 3.0f);
    g.setColour (Colour (0xff2a2a2a));
    g.fillEllipse (Rectangle<float> (12.0f, 12.0f).withCentre (plate.getCentre()));
    const float dir = on ? -1.0f : 1.0f;
    auto tip = plate.getCentre().translated (0.0f, dir * 15.0f);
    Path bat;
    bat.startNewSubPath (plate.getCentre());
    bat.lineTo (tip);
    g.setColour (Colour (0xffe8e8e8));
    g.strokePath (bat, PathStrokeType (5.0f, PathStrokeType::curved, PathStrokeType::rounded));
    g.fillEllipse (Rectangle<float> (9.0f, 9.0f).withCentre (tip));
    g.setColour (b.getProperties()["dark"] ? Colour (0xff1e1e1e) : kInk);
    g.setFont (FontOptions (11.0f, Font::bold));
    g.drawText (b.getButtonText(), r.removeFromTop (14.0f), Justification::centred);
}

void FerroLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    g.setGradientFill (ColourGradient (Colour (down ? 0xff4a4a4a : 0xff3a3a3a), r.getX(), r.getY(), Colour (0xff141414), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (highlighted ? kAmber.withAlpha (0.6f) : Colour (0xff606060));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

// ============================================================================ Editor
FerroEditor::FerroEditor (FerroProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

    input = &addKnob ("input", "INPUT VOLUME", 0);
    mode = &addKnob ("mode", "MODE SELECTOR", 2);
    rate = &addKnob ("rate", "REPEAT RATE", 0);
    intensity = &addKnob ("intensity", "INTENSITY", 0);
    echoVol = &addKnob ("echoVol", "ECHO VOLUME", 0);
    reverbVol = &addKnob ("reverbVol", "REVERB VOLUME", 0);
    bass = &addKnob ("bass", "BASS", 1);
    treble = &addKnob ("treble", "TREBLE", 1);
    wow = &addKnob ("wowFlutter", "WOW / FLUTTER", 3);
    age = &addKnob ("tapeAge", "TAPE AGE", 3);
    noise = &addKnob ("noise", "NOISE", 3);
    output = &addKnob ("output", "OUTPUT", 3);

    for (auto* t : { &echoOn, &splice, &dry }) addAndMakeVisible (*t);
    splice.getProperties().set ("dark", true);
    dry.getProperties().set ("dark", true);
    echoOnAttach = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "echoOn", echoOn);
    spliceAttach = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "splice", splice);
    dryAttach = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "dry", dry);
    echoOn.setTooltip ("Echo / Normal: down stops feeding the tape, the repeats already on it keep going");
    power.setTooltip ("Power cycle: erases the tape loop and the spring (stops runaway oscillation)");

    addAndMakeVisible (power);
    power.onClick = [this] { proc.requestPowerCycle(); powerFlash = 12; };

    int n = 0; auto* ps = fe::factoryPresets (n);
    for (int i = 0; i < n; ++i) presetBox.addItem (ps[i].name, i + 1);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, dontSendNotification);
    presetBox.onChange = [this] { proc.setCurrentProgram (presetBox.getSelectedId() - 1); };
    addAndMakeVisible (presetBox);

    // case texture: speckled vinyl
    caseTexture = Image (Image::ARGB, W, H, true);
    {
        Graphics tg (caseTexture);
        Random rnd (201);
        for (int i = 0; i < 26000; ++i)
        {
            tg.setColour ((rnd.nextBool() ? Colours::black : Colours::white).withAlpha (0.05f + 0.06f * rnd.nextFloat()));
            tg.fillRect ((float) rnd.nextInt (W), (float) rnd.nextInt (H), 1.5f, 1.5f);
        }
    }

    setSize (W, H);
    startTimerHz (30);
}

FerroEditor::~FerroEditor() { setLookAndFeel (nullptr); }

FerroEditor::Knob& FerroEditor::addKnob (const String& id, const String& label, int kind)
{
    auto k = std::make_unique<Knob>();
    k->label = label;
    k->kind = kind;
    auto& s = k->slider;
    s.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    s.setRotaryParameters (kStartAngle, kEndAngle, true);
    s.setPopupDisplayEnabled (true, true, this);
    if (kind == 2) { s.getProperties().set ("mode", true); s.setMouseDragSensitivity (120); }
    if (kind == 3) s.getProperties().set ("small", true);
    addAndMakeVisible (s);
    k->attach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, s);
    if (kind == 2)   // snap the rotary to its 12 detents
        s.setRange (1.0, 12.0, 1.0);
    if (kind != 2) s.setDoubleClickReturnValue (true, proc.apvts.getParameter (id)->convertFrom0to1 (proc.apvts.getParameter (id)->getDefaultValue()));
    knobs.push_back (std::move (k));
    return *knobs.back();
}

void FerroEditor::resized()
{
    const int knobY = 222, kd = 68;
    input->slider.setBounds (Rectangle<int> (kd, kd).withCentre ({ 92, knobY }));
    mode->slider.setBounds (Rectangle<int> (118, 118).withCentre ({ 278, knobY + 6 }));
    Knob* row[] = { rate, intensity, echoVol, reverbVol, bass, treble };
    for (int i = 0; i < 6; ++i)
        row[i]->slider.setBounds (Rectangle<int> (kd, kd).withCentre ({ 474 + i * 104, knobY }));

    echoOn.setBounds (Rectangle<int> (60, 56).withCentre ({ 62, 352 }));
    power.setBounds (Rectangle<int> (64, 26).withCentre ({ 138, 362 }));

    // service strip
    const int sy = 470, sd = 46;
    Knob* svc[] = { wow, age, noise, output };
    for (int i = 0; i < 4; ++i)
        svc[i]->slider.setBounds (Rectangle<int> (sd, sd).withCentre ({ 100 + i * 112, sy + 6 }));
    splice.setBounds (Rectangle<int> (60, 56).withCentre ({ 560, sy + 2 }));
    dry.setBounds (Rectangle<int> (60, 56).withCentre ({ 630, sy + 2 }));
    presetBox.setBounds (760, sy - 6, 260, 26);
}

void FerroEditor::drawScale (Graphics& g, const Knob& k)
{
    auto b = k.slider.getBounds().toFloat();
    auto c = b.getCentre();
    const float r = b.getWidth() * 0.5f;
    if (k.kind != 3)
    {
        g.setColour (kInk);
        g.setFont (FontOptions (12.5f, Font::bold));
        if (k.kind == 2)
            g.drawFittedText (k.label, Rectangle<int> ((int) c.x - 70, (int) (b.getBottom() + 6), 140, 14), Justification::centred, 1);
        else
            g.drawFittedText (k.label, Rectangle<int> ((int) c.x - 70, (int) (b.getY() - 36), 140, 14), Justification::centred, 1);
    }

    if (k.kind == 2) return;   // the mode selector has its own ring

    const int steps = k.kind == 1 ? 10 : 10;
    for (int i = 0; i <= steps; ++i)
    {
        const float a = kStartAngle + (kEndAngle - kStartAngle) * i / (float) steps;
        const float r1 = r + 3.0f, r2 = r + (i % 5 == 0 ? 9.0f : 6.0f);
        g.setColour (kInkDim);
        g.drawLine (Line<float> (c.getPointOnCircumference (r1, a), c.getPointOnCircumference (r2, a)), 1.2f);
        if (k.kind == 3) continue;
        String txt;
        if (k.kind == 0 && (i % 2 == 0)) txt = String (i);
        if (k.kind == 1 && (i == 0 || i == 5 || i == 10)) txt = i == 0 ? "-" : (i == 5 ? "0" : "+");
        if (txt.isNotEmpty())
        {
            g.setColour (kInk);
            g.setFont (FontOptions (10.5f));
            auto pt = c.getPointOnCircumference (r + 13.0f, a);
            g.drawText (txt, Rectangle<float> (22, 12).withCentre (pt), Justification::centred);
        }
    }
}

void FerroEditor::drawHeadChart (Graphics& g, Rectangle<float> area)
{
    // the head chart printed under the mode selector: which heads and reverb each position uses
    const float labelW = 46.0f;
    const float cw = (area.getWidth() - labelW) / 12.0f, rh = area.getHeight() / 5.0f;
    const int sel = (int) std::lround (mode->slider.getValue());
    g.setFont (FontOptions (9.5f, Font::bold));
    const char* rows[] = { "MODE", "HEAD 1", "HEAD 2", "HEAD 3", "REVERB" };
    for (int rI = 0; rI < 5; ++rI)
    {
        g.setColour (kInkDim);
        g.drawText (rows[rI], Rectangle<float> (area.getX(), area.getY() + rI * rh, labelW, rh), Justification::centredLeft);
    }
    for (int m = 0; m < 12; ++m)
    {
        const float x = area.getX() + labelW + m * cw;
        if (m + 1 == sel)
        {
            g.setColour (kAmber.withAlpha (0.16f));
            g.fillRoundedRectangle (Rectangle<float> (x + 1, area.getY(), cw - 2, area.getHeight()), 3.0f);
        }
        g.setColour (m + 1 == sel ? kAmber : kInk);
        g.drawText (String (m + 1), Rectangle<float> (x, area.getY(), cw, rh), Justification::centred);
        const auto& mi = fe::kModes[m];
        const bool on[4] = { mi.head[0], mi.head[1], mi.head[2], mi.reverb };
        for (int rI = 0; rI < 4; ++rI)
            if (on[rI])
            {
                g.setColour (m + 1 == sel ? kAmber : kInk);
                g.fillEllipse (Rectangle<float> (6.0f, 6.0f).withCentre ({ x + cw * 0.5f, area.getY() + (rI + 1.5f) * rh }));
            }
    }
}

void FerroEditor::drawVu (Graphics& g, Rectangle<float> r)
{
    // amber-lit VU meter, -20..+3 VU
    g.setColour (Colour (0xff0a0a0a));
    g.fillRoundedRectangle (r, 5.0f);
    auto face = r.reduced (6.0f);
    g.setGradientFill (ColourGradient (Colour (0xfff7d58f), face.getCentreX(), face.getY(), Colour (0xffd08a2c), face.getCentreX(), face.getBottom(), false));
    g.fillRoundedRectangle (face, 3.0f);

    auto pivot = Point<float> (face.getCentreX(), face.getBottom() + face.getHeight() * 0.55f);
    const float rad = face.getHeight() * 1.22f;
    auto vuToAngle = [] (float vu) { return jmap (vu, -20.0f, 3.0f, -0.80f, 0.80f); };
    for (int vu : { -20, -10, -7, -5, -3, -2, -1, 0, 1, 2, 3 })
    {
        const float a = vuToAngle ((float) vu);
        g.setColour (vu > 0 ? Colour (0xffb3241c) : Colour (0xff2a1d10));
        g.drawLine (Line<float> (pivot.getPointOnCircumference (rad - 8.0f, a), pivot.getPointOnCircumference (rad, a)), 1.5f);
        if (vu == -20 || vu == -10 || vu == -5 || vu == 0 || vu == 3 || vu == -3)
        {
            g.setFont (FontOptions (9.0f, Font::bold));
            g.drawText (vu > 0 ? "+" + String (vu) : String (vu),
                        Rectangle<float> (24, 10).withCentre (pivot.getPointOnCircumference (rad + 7.0f, a)), Justification::centred);
        }
    }
    g.setColour (Colour (0xffb3241c));
    Path redArc;
    redArc.addCentredArc (pivot.x, pivot.y, rad - 3.0f, rad - 3.0f, 0.0f, vuToAngle (0.0f), vuToAngle (3.0f), true);
    g.strokePath (redArc, PathStrokeType (3.0f));
    g.setColour (Colour (0xff2a1d10));
    g.setFont (FontOptions (13.0f, Font::bold));
    g.drawText ("VU", face.withTrimmedTop (face.getHeight() * 0.55f), Justification::centred);

    const float a = vuToAngle (jlimit (-22.0f, 4.0f, vuNeedle));
    g.saveState();
    g.reduceClipRegion (face.toNearestInt());
    g.setColour (Colour (0xff111111));
    g.drawLine (Line<float> (pivot.getPointOnCircumference (rad * 0.42f, a), pivot.getPointOnCircumference (rad + 2.0f, a)), 1.6f);
    g.restoreState();
    g.setColour (Colours::white.withAlpha (0.10f));
    g.fillRoundedRectangle (face.withHeight (face.getHeight() * 0.35f), 3.0f);
}

void FerroEditor::drawTapeWindow (Graphics& g, Rectangle<float> r)
{
    // smoked window onto the transport: loop moving past erase, record, 3 playback heads, capstan
    g.setColour (Colour (0xff0b0b0b));
    g.fillRoundedRectangle (r, 6.0f);
    auto win = r.reduced (5.0f);
    g.setGradientFill (ColourGradient (Colour (0xff23211e), win.getX(), win.getY(), Colour (0xff0e0d0c), win.getX(), win.getBottom(), false));
    g.fillRoundedRectangle (win, 4.0f);

    const auto& e = proc.getEngine();
    const auto& mi = fe::kModes[jlimit (1, 12, (int) std::lround (mode->slider.getValue())) - 1];
    const bool hasSplice = splice.getToggleState();
    const float tapeAge = (float) age->slider.getValue() / 10.0f;

    const float tapeY = win.getY() + win.getHeight() * 0.42f, tapeH = 16.0f;
    const float x0 = win.getX() + 80.0f, unitPx = (win.getWidth() - 200.0f) / (float) fe::Engine::kCapstanDist;   // record head at x0
    // tape
    g.setColour (Colour (0xff4a2c17));
    g.fillRect (Rectangle<float> (win.getX(), tapeY - tapeH * 0.5f, win.getWidth(), tapeH));
    g.setColour (Colour (0xff6b4123));
    g.fillRect (Rectangle<float> (win.getX(), tapeY - tapeH * 0.5f, win.getWidth(), 2.0f));

    // marks that travel with the tape: oxide texture, worn spots, and the splice
    g.saveState();
    g.reduceClipRegion (win.toNearestInt());
    const double L = fe::Engine::kLoopLength;
    const double leftUnit = (win.getX() - x0) / unitPx, rightUnit = (win.getRight() - x0) / unitPx;
    // a point on the tape at loop coordinate q is at x = x0 + (tapePos - q) * unitPx (tape moves left -> right)
    for (double u = std::floor (leftUnit * 6.0) / 6.0; u < rightUnit; u += 1.0 / 6.0)
    {
        const double q = tapePos - u;
        const double frac = q * 6.0 - std::floor (q * 6.0);
        const float x = (float) (x0 + (u + frac / 6.0) * unitPx);
        g.setColour (Colour (0xff3a2212));
        g.drawVerticalLine ((int) x, tapeY - tapeH * 0.5f + 3, tapeY + tapeH * 0.5f - 2);
    }
    for (int k = -1; k <= 1; ++k)
    {
        // splice at loop coordinate 0
        if (! hasSplice) break;
        const double q = std::round ((tapePos - leftUnit) / L + k) * L;
        const float x = (float) (x0 + (tapePos - q) * unitPx);
        g.setColour (Colour (0xffd8d2c0));
        g.fillRect (Rectangle<float> (x - 1.5f, tapeY - tapeH * 0.5f, 3.0f, tapeH));
    }
    g.restoreState();

    // heads
    struct HeadDraw { float unit; const char* name; int idx; };
    const HeadDraw heads[] = { { -0.55f, "ERASE", -2 }, { 0.0f, "REC", -1 }, { 1.0f, "1", 0 }, { (float) fe::Engine::kHeadDist[1], "2", 1 }, { (float) fe::Engine::kHeadDist[2], "3", 2 } };
    for (auto& h : heads)
    {
        const float x = x0 + h.unit * unitPx;
        const bool active = h.idx >= 0 && mi.head[h.idx];
        auto hb = Rectangle<float> (22.0f, 26.0f).withCentre ({ x, tapeY + tapeH * 0.5f + 12.0f });
        g.setGradientFill (ColourGradient (kChrome, hb.getX(), hb.getY(), Colour (0xff6b6c70), hb.getRight(), hb.getBottom(), false));
        g.fillRoundedRectangle (hb, 4.0f);
        if (active)
        {
            g.setColour (kAmber.withAlpha (0.35f));
            g.fillEllipse (hb.expanded (10.0f, 6.0f));
            g.setColour (kAmber);
            g.drawRoundedRectangle (hb, 4.0f, 1.5f);
        }
        g.setColour (h.idx >= 0 ? (active ? kAmber : kInkDim) : kInkDim);
        g.setFont (FontOptions (h.idx >= 0 ? 12.0f : 9.5f, Font::bold));
        g.drawText (h.idx >= 0 ? String ("PB ") + h.name : String (h.name), Rectangle<float> (60, 12).withCentre ({ x, hb.getBottom() + 9.0f }), Justification::centred);
        if (h.idx >= 0)
        {
            g.setFont (FontOptions (10.5f));
            g.setColour (active ? kInk : kInkDim.withAlpha (0.6f));
            const double live = e.getCurrentHeadDelayMs (h.idx);
            const double ms = live > 0 ? live : fe::Engine::headDelayMs (h.idx, rate->slider.getValue() / 10.0);
            g.drawText (String (roundToInt (ms)) + " ms", Rectangle<float> (70, 12).withCentre ({ x, win.getY() + 12.0f }), Justification::centred);
        }
    }
    // capstan + pinch roller
    const float cx = x0 + (float) fe::Engine::kCapstanDist * unitPx;
    auto cap = Rectangle<float> (14.0f, 14.0f).withCentre ({ cx, tapeY + tapeH * 0.5f + 7.0f });
    g.setColour (kChrome);
    g.fillEllipse (cap);
    auto roller = Rectangle<float> (30.0f, 30.0f).withCentre ({ cx, tapeY - tapeH * 0.5f - 15.0f });
    g.setColour (Colour (0xff1d1d1d));
    g.fillEllipse (roller);
    g.setColour (Colour (0xff555555));
    const float rot = (float) (tapePos * 2.4);
    for (int i = 0; i < 3; ++i)
        g.drawLine (Line<float> (roller.getCentre(), roller.getCentre().getPointOnCircumference (12.0f, rot + i * 2.094f)), 2.0f);
    g.setColour (kInkDim);
    g.setFont (FontOptions (9.5f, Font::bold));
    g.drawText ("CAPSTAN", Rectangle<float> (60, 12).withCentre ({ cx, cap.getBottom() + 22.0f }), Justification::centred);

    if (tapeAge > 0.55f)
    {
        g.setColour (kRed.withAlpha (0.8f));
        g.setFont (FontOptions (10.0f, Font::bold));
        g.drawText ("WORN TAPE", win.removeFromRight (90).removeFromTop (16).translated (-6, 4), Justification::centredRight);
    }
}

void FerroEditor::paint (Graphics& g)
{
    // case: dark green vinyl with speckle
    g.fillAll (kCaseGreen);
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.06f), 0, 0, Colours::black.withAlpha (0.25f), 0, (float) H, false));
    g.fillAll();
    g.drawImageAt (caseTexture, 0, 0);

    // black front panel with chrome trim
    auto plate = Rectangle<float> (14.0f, 14.0f, (float) W - 28.0f, 400.0f);
    g.setColour (Colour (0xffb9babd));
    g.fillRoundedRectangle (plate.expanded (3.0f), 6.0f);
    g.setGradientFill (ColourGradient (Colour (0xff1d1d1d), 0, plate.getY(), kPlate, 0, plate.getBottom(), false));
    g.fillRoundedRectangle (plate, 5.0f);
    // fine brushed lines
    g.setColour (Colours::white.withAlpha (0.018f));
    for (float y = plate.getY() + 2; y < plate.getBottom(); y += 3.0f) g.drawHorizontalLine ((int) y, plate.getX() + 4, plate.getRight() - 4);

    drawVu (g, { 30.0f, 28.0f, 196.0f, 104.0f });
    // peak lamp
    {
        auto lamp = Rectangle<float> (16.0f, 16.0f).withCentre ({ 252.0f, 62.0f });
        g.setColour (Colour (0xff3a3a3a)); g.fillEllipse (lamp.expanded (3.0f));
        g.setColour (peakLit ? kRed : Colour (0xff4a1210)); g.fillEllipse (lamp);
        if (peakLit) { g.setColour (kRed.withAlpha (0.3f)); g.fillEllipse (lamp.expanded (8.0f)); }
        g.setColour (kInk); g.setFont (FontOptions (10.0f, Font::bold));
        g.drawText ("PEAK", Rectangle<float> (50, 12).withCentre ({ 252.0f, 86.0f }), Justification::centred);
    }
    drawTapeWindow (g, { 286.0f, 28.0f, (float) W - 316.0f, 104.0f });

    for (auto& k : knobs) drawScale (g, *k);

    // mode selector ring with zone legends
    {
        auto c = mode->slider.getBounds().toFloat().getCentre();
        const float r = mode->slider.getWidth() * 0.5f + 14.0f;
        const int sel = (int) std::lround (mode->slider.getValue());
        for (int m = 1; m <= 12; ++m)
        {
            const float a = kStartAngle + (kEndAngle - kStartAngle) * (m - 1) / 11.0f;
            g.setColour (m == sel ? kAmber : kInk);
            g.setFont (FontOptions (m == sel ? 13.0f : 11.5f, Font::bold));
            g.drawText (String (m), Rectangle<float> (22, 14).withCentre (c.getPointOnCircumference (r, a)), Justification::centred);
        }
        // zone arcs: ECHO (1-4), REVERB + ECHO (5-11), REVERB (12)
        auto arc = [&] (int from, int to, const String& txt, float labelAngleOffset)
        {
            const float a1 = kStartAngle + (kEndAngle - kStartAngle) * (from - 1) / 11.0f - 0.08f;
            const float a2 = kStartAngle + (kEndAngle - kStartAngle) * (to - 1) / 11.0f + 0.08f;
            Path p; p.addCentredArc (c.x, c.y, r + 12.0f, r + 12.0f, 0.0f, a1, a2, true);
            g.setColour (kInkDim);
            g.strokePath (p, PathStrokeType (1.2f));
            g.setFont (FontOptions (9.0f, Font::bold));
            const float am = (a1 + a2) * 0.5f + labelAngleOffset;
            g.drawText (txt, Rectangle<float> (100, 12).withCentre (c.getPointOnCircumference (r + 22.0f, am)), Justification::centred);
        };
        arc (1, 4, "ECHO", 0.0f);
        arc (5, 11, "REVERB + ECHO", 0.0f);
        arc (12, 12, "REV", 0.18f);
    }

    drawHeadChart (g, { 204.0f, 316.0f, 252.0f, 84.0f });

    // power pilot lamp
    {
        auto lamp = Rectangle<float> (12.0f, 12.0f).withCentre ({ 138.0f, 336.0f });
        const bool lit = powerFlash == 0 || (powerFlash / 3) % 2 == 0;
        g.setColour (lit ? kRed : Colour (0xff4a1210)); g.fillEllipse (lamp);
        if (lit) { g.setColour (kRed.withAlpha (0.25f)); g.fillEllipse (lamp.expanded (5.0f)); }
    }
    g.setColour (kInkDim);
    g.setFont (FontOptions (9.0f, Font::bold));
    g.drawText ("NORMAL", Rectangle<float> (60, 12).withCentre ({ 62.0f, 392.0f }), Justification::centred);

    // brand block
    {
        auto br = Rectangle<float> (500.0f, 318.0f, 520.0f, 84.0f);
        g.setColour (kInk);
        g.setFont (FontOptions (40.0f, Font::bold | Font::italic));
        g.drawText ("FERRO ECHO", br.removeFromTop (48.0f), Justification::centredRight);
        g.setColour (kInkDim);
        g.setFont (FontOptions (12.0f, Font::bold));
        g.drawText (String::fromUTF8 ("TAPE LOOP ECHO  \xc2\xb7  3 PLAYBACK HEADS  \xc2\xb7  SPRING REVERB"), br.removeFromTop (18.0f), Justification::centredRight);
    }

    // service strip: brushed aluminium plate
    auto svc = Rectangle<float> (14.0f, 428.0f, (float) W - 28.0f, 80.0f);
    g.setGradientFill (ColourGradient (Colour (0xffc9cacc), 0, svc.getY(), Colour (0xff8f9194), 0, svc.getBottom(), false));
    g.fillRoundedRectangle (svc, 4.0f);
    g.setColour (Colours::black.withAlpha (0.05f));
    for (float y = svc.getY() + 1; y < svc.getBottom(); y += 2.0f) g.drawHorizontalLine ((int) y, svc.getX() + 2, svc.getRight() - 2);
    g.setColour (Colour (0xff2a2a2a));
    g.setFont (FontOptions (10.0f, Font::bold));
    g.drawText ("TAPE SERVICE", Rectangle<float> (svc.getX() + 10, svc.getY() + 4, 120, 12), Justification::centredLeft);
    g.drawText ("PROGRAM", Rectangle<float> (760, svc.getY() + 4, 120, 12), Justification::centredLeft);
}

void FerroEditor::paintOverChildren (Graphics& g)
{
    // service-strip labels are dark ink on aluminium: redraw them over the default light ink
    for (auto* k : { wow, age, noise, output })
    {
        auto b = k->slider.getBounds();
        g.setColour (Colour (0xff1e1e1e));
        g.setFont (FontOptions (10.0f, Font::bold));
        g.drawText (k->label, Rectangle<int> (b.getCentreX() - 60, b.getBottom() - 1, 120, 12), Justification::centred);
    }
}

void FerroEditor::timerCallback()
{
    const auto& e = proc.getEngine();
    const float vu = e.getVu();
    const float db = vu > 1e-5f ? 20.0f * std::log10 (vu / 0.35f) : -40.0f;   // 0 VU = nominal tape level
    vuNeedle += (db - vuNeedle) * 0.28f;                                         // needle ballistics
    peakLit = e.getPeakLamp();
    tapePos = e.getTapePosition();
    if (powerFlash > 0) --powerFlash;
    const int prog = proc.getCurrentProgram() + 1;
    if (presetBox.getSelectedId() != prog) presetBox.setSelectedId (prog, dontSendNotification);
    repaint();
}
