#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <random>

namespace
{
    struct ScaleMark
    {
        float db;
        float normPos;
        const char* label; // nullptr for minor tick
        bool isRed;
    };

    // Symmetric, balanced VU Meter Scale Table (-20 dB to +3 dB)
    // Eliminates the old gap between -20 and -10 and places -7..-5 right in the upper-center Sweet Spot!
    constexpr ScaleMark kVuScaleMarks[] = {
        { -20.0f, 0.000f, "-20", false },
        { -17.5f, 0.060f, nullptr, false },
        { -15.0f, 0.125f, "-15", false },
        { -12.5f, 0.188f, nullptr, false },
        { -10.0f, 0.255f, "-10", false },
        {  -8.5f, 0.318f, nullptr, false },
        {  -7.0f, 0.380f, "-7",  false },
        {  -6.0f, 0.440f, nullptr, false },
        {  -5.0f, 0.500f, "-5",  false }, // Exact top-center (0 deg)
        {  -4.0f, 0.560f, nullptr, false },
        {  -3.0f, 0.620f, "-3",  false },
        {  -2.0f, 0.690f, "-2",  false },
        {  -1.0f, 0.755f, "-1",  false },
        {  -0.5f, 0.790f, nullptr, false },
        {   0.0f, 0.825f, "0",   false },
        {   1.0f, 0.885f, "+1",  true  },
        {   2.0f, 0.942f, "+2",  true  },
        {   3.0f, 1.000f, "+3",  true  }
    };
}

//==============================================================================
Vibe2ALookAndFeel::Vibe2ALookAndFeel()
{
    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff1c1916));
    setColour (juce::ComboBox::textColourId,       juce::Colour (0xffe8dcc8));
    setColour (juce::ComboBox::outlineColourId,    juce::Colour (0xff5c5245));
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1f1b18));
    setColour (juce::PopupMenu::textColourId,       juce::Colour (0xffe8dcc8));
    setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xffb87b28));
}

void Vibe2ALookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                          float sliderPos, float rotaryStartAngle,
                                          float rotaryEndAngle, juce::Slider& slider)
{
    const int style = slider.getProperties().getWithDefault ("knobStyle", 0);
    const auto bounds = juce::Rectangle<float> (static_cast<float> (x), static_cast<float> (y),
                                                static_cast<float> (width), static_cast<float> (height));
    const float cx = bounds.getCentreX();
    const float cy = bounds.getCentreY();
    const float maxRadius = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    if (style == 0)
    {
        // =========================================================================
        // STYLE 0: ULTRA-CRISP 3D SCALLOPED BLACK BAKELITE KNOB (GAIN & PEAK RED.)
        // 100% Vector + PBR multi-layered shading -> Zero pixelation at any DPI!
        // =========================================================================
        const float outerScaleR = maxRadius * 0.95f;
        const float tickInnerR  = maxRadius * 0.77f;
        const float tickMajorR  = maxRadius * 0.86f;
        const float tickMinorR  = maxRadius * 0.82f;

        // 1. Draw Precision Outer Scale Ticks & Numbers (0, 10, 20 .. 100)
        g.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
        for (int i = 0; i <= 20; ++i)
        {
            const float norm = static_cast<float> (i) / 20.0f;
            const float a = rotaryStartAngle + norm * (rotaryEndAngle - rotaryStartAngle);
            const float sinA = std::sin (a);
            const float cosA = -std::cos (a);
            const bool isMajor = (i % 2 == 0);

            const float r1 = tickInnerR;
            const float r2 = isMajor ? tickMajorR : tickMinorR;

            g.setColour (juce::Colour (isMajor ? 0xff221e1a : 0xff554d44));
            g.drawLine (cx + sinA * r1, cy + cosA * r1,
                        cx + sinA * r2, cy + cosA * r2, isMajor ? 1.8f : 1.1f);

            if (isMajor)
            {
                const int valLabel = i * 5; // 0, 10, 20 ... 100
                const float tx = cx + sinA * outerScaleR;
                const float ty = cy + cosA * outerScaleR;
                g.setColour (juce::Colour (0xff2c2620));
                g.drawText (juce::String (valLabel),
                            juce::Rectangle<float> (tx - 13.0f, ty - 7.0f, 26.0f, 14.0f),
                            juce::Justification::centred, false);
            }
        }

        const float knobR = maxRadius * 0.68f;

        // 2. Fixed-Light Directional 3D Drop Shadow (cast toward bottom-right)
        for (int s = 5; s >= 1; --s)
        {
            const float sf = static_cast<float> (s);
            g.setColour (juce::Colours::black.withAlpha (0.09f * (6.0f - sf) / 5.0f));
            g.fillEllipse (cx - knobR + sf * 1.4f, cy - knobR + sf * 2.2f, knobR * 2.0f, knobR * 2.0f);
        }

        // 3. Build 12-Flute Scalloped Bakelite Silhouette Path (Analytical Smooth Curve)
        auto createScallopPath = [cx, cy] (float baseR, float amp, float rotAngle, float offsetX = 0.0f, float offsetY = 0.0f)
        {
            juce::Path p;
            constexpr int numSteps = 144;
            for (int i = 0; i <= numSteps; ++i)
            {
                const float theta = juce::MathConstants<float>::twoPi * static_cast<float> (i) / static_cast<float> (numSteps);
                // 12 deep ergonomic flutes rotating with the knob angle
                const float wave = std::cos (12.0f * (theta - rotAngle));
                const float r = baseR + amp * wave - 0.25f * amp * std::cos (24.0f * (theta - rotAngle));
                const float px = cx + offsetX + std::sin (theta) * r;
                const float py = cy + offsetY - std::cos (theta) * r;
                if (i == 0) p.startNewSubPath (px, py);
                else        p.lineTo (px, py);
            }
            p.closeSubPath();
            return p;
        };

        // Outer beveled base skirt
        juce::Path outerSkirt = createScallopPath (knobR, knobR * 0.068f, angle);
        juce::ColourGradient bakeliteBodyGrad (
            juce::Colour (0xff4a433d), cx - knobR * 0.55f, cy - knobR * 0.55f,
            juce::Colour (0xff0e0c0a), cx + knobR * 0.65f, cy + knobR * 0.65f, true);
        bakeliteBodyGrad.addColour (0.45, juce::Colour (0xff26221e));
        bakeliteBodyGrad.addColour (0.80, juce::Colour (0xff151210));
        g.setGradientFill (bakeliteBodyGrad);
        g.fillPath (outerSkirt);

        // Specular 3D rim lighting on the scalloped edges
        juce::ColourGradient rimLightGrad (
            juce::Colour (0xaa9e9284), cx - knobR * 0.7f, cy - knobR * 0.7f,
            juce::Colour (0xdd050404), cx + knobR * 0.7f, cy + knobR * 0.7f, false);
        g.setGradientFill (rimLightGrad);
        g.strokePath (outerSkirt, juce::PathStrokeType (1.6f));

        // 4. Individual 3D Ridge Highlights for the 12 Scallop Flutes
        for (int flute = 0; flute < 12; ++flute)
        {
            const float fAngle = angle + static_cast<float> (flute) * (juce::MathConstants<float>::twoPi / 12.0f);
            const float valleyAngle = fAngle + (juce::MathConstants<float>::pi / 12.0f);
            const float vx1 = cx + std::sin (valleyAngle) * (knobR * 0.52f);
            const float vy1 = cy - std::cos (valleyAngle) * (knobR * 0.52f);
            const float vx2 = cx + std::sin (valleyAngle) * (knobR * 0.92f);
            const float vy2 = cy - std::cos (valleyAngle) * (knobR * 0.92f);

            // Light intensity depends on orientation relative to top-left light source (-0.78 rad)
            const float lightAlign = std::cos (valleyAngle + 0.75f);
            if (lightAlign > 0.0f)
                g.setColour (juce::Colour (0xff786d61).withAlpha (0.28f * lightAlign));
            else
                g.setColour (juce::Colours::black.withAlpha (0.42f * (-lightAlign)));

            g.drawLine (vx1, vy1, vx2, vy2, 1.5f);
        }

        // 5. Inner Molded Bakelite Step Ring
        const float innerStepR = knobR * 0.72f;
        juce::ColourGradient innerStepGrad (
            juce::Colour (0xff12100e), cx - innerStepR, cy - innerStepR,
            juce::Colour (0xff3c352f), cx + innerStepR, cy + innerStepR, false);
        g.setGradientFill (innerStepGrad);
        g.fillEllipse (cx - innerStepR, cy - innerStepR, innerStepR * 2.0f, innerStepR * 2.0f);
        g.setColour (juce::Colour (0x88000000));
        g.drawEllipse (cx - innerStepR, cy - innerStepR, innerStepR * 2.0f, innerStepR * 2.0f, 1.4f);

        // 6. Spun-Aluminum Metallic Center Cap (Conical Brushed Sheen)
        const float capR = knobR * 0.39f;
        juce::ColourGradient capGrad (
            juce::Colour (0xfff4efe6), cx - capR * 0.6f, cy - capR * 0.6f,
            juce::Colour (0xff6e675f), cx + capR * 0.7f, cy + capR * 0.7f, true);
        capGrad.addColour (0.45, juce::Colour (0xffb8b0a4));
        g.setGradientFill (capGrad);
        g.fillEllipse (cx - capR, cy - capR, capR * 2.0f, capR * 2.0f);

        // Concentric spun-metal micro-rings on the silver cap
        g.setColour (juce::Colour (0x30ffffff));
        g.drawEllipse (cx - capR * 0.75f, cy - capR * 0.75f, capR * 1.5f, capR * 1.5f, 0.8f);
        g.setColour (juce::Colour (0x35000000));
        g.drawEllipse (cx - capR * 0.45f, cy - capR * 0.45f, capR * 0.9f, capR * 0.9f, 0.8f);
        g.setColour (juce::Colour (0xff221e1b));
        g.drawEllipse (cx - capR, cy - capR, capR * 2.0f, capR * 2.0f, 1.3f);

        // 7. Engraved Crisp Vintage Ivory Indicator Line
        const float sinP = std::sin (angle);
        const float cosP = -std::cos (angle);
        const float pStart = capR * 1.05f;
        const float pEnd   = knobR * 0.96f;

        // Recessed shadow around indicator groove
        g.setColour (juce::Colours::black.withAlpha (0.75f));
        g.drawLine (cx + sinP * pStart + 1.0f, cy + cosP * pStart + 1.0f,
                    cx + sinP * pEnd + 1.0f,   cy + cosP * pEnd + 1.0f, 3.4f);

        // Crisp ivory-white inlaid pointer line
        g.setColour (juce::Colour (0xfff5ebd6));
        g.drawLine (cx + sinP * pStart, cy + cosP * pStart,
                    cx + sinP * pEnd,   cy + cosP * pEnd, 2.5f);
    }
    else if (style == 1)
    {
        // =========================================================================
        // STYLE 1: BRUSHED SILVER ALUMINUM HARDWARE KNOB (ATTACK, RELEASE, DRIVE, MIX)
        // =========================================================================
        const float tickR1 = maxRadius * 0.78f;
        const float tickR2 = maxRadius * 0.90f;

        for (int i = 0; i <= 10; ++i)
        {
            const float norm = static_cast<float> (i) / 10.0f;
            const float a = rotaryStartAngle + norm * (rotaryEndAngle - rotaryStartAngle);
            const float sinA = std::sin (a);
            const float cosA = -std::cos (a);
            const bool isMajor = (i % 2 == 0);
            g.setColour (isMajor ? juce::Colour (0xffcbbba2) : juce::Colour (0xff786e60));
            g.drawLine (cx + sinA * tickR1, cy + cosA * tickR1,
                        cx + sinA * (isMajor ? tickR2 : (tickR1 + tickR2) * 0.5f),
                        cy + cosA * (isMajor ? tickR2 : (tickR1 + tickR2) * 0.5f),
                        isMajor ? 1.5f : 1.0f);
        }

        const float knobR = maxRadius * 0.66f;

        // Drop shadow
        for (int s = 4; s >= 1; --s)
        {
            const float sf = static_cast<float> (s);
            g.setColour (juce::Colours::black.withAlpha (0.14f * (5.0f - sf) / 4.0f));
            g.fillEllipse (cx - knobR + sf * 1.1f, cy - knobR + sf * 1.8f, knobR * 2.0f, knobR * 2.0f);
        }

        // Knurled outer dark-chrome skirt (24 fine teeth)
        juce::Path knurlPath;
        constexpr int kSteps = 120;
        for (int i = 0; i <= kSteps; ++i)
        {
            const float t = juce::MathConstants<float>::twoPi * static_cast<float> (i) / static_cast<float> (kSteps);
            const float r = knobR + (knobR * 0.035f) * std::cos (24.0f * (t - angle));
            const float px = cx + std::sin (t) * r;
            const float py = cy - std::cos (t) * r;
            if (i == 0) knurlPath.startNewSubPath (px, py);
            else        knurlPath.lineTo (px, py);
        }
        knurlPath.closeSubPath();

        juce::ColourGradient skirtGrad (
            juce::Colour (0xff8a847c), cx - knobR, cy - knobR,
            juce::Colour (0xff24211e), cx + knobR, cy + knobR, false);
        g.setGradientFill (skirtGrad);
        g.fillPath (knurlPath);
        g.setColour (juce::Colour (0xff141210));
        g.strokePath (knurlPath, juce::PathStrokeType (1.2f));

        // Brushed Aluminum Top Face
        const float faceR = knobR * 0.80f;
        juce::ColourGradient faceGrad (
            juce::Colour (0xfff0ebe1), cx - faceR * 0.65f, cy - faceR * 0.65f,
            juce::Colour (0xff6e685f), cx + faceR * 0.75f, cy + faceR * 0.75f, true);
        faceGrad.addColour (0.5, juce::Colour (0xffb8b0a4));
        g.setGradientFill (faceGrad);
        g.fillEllipse (cx - faceR, cy - faceR, faceR * 2.0f, faceR * 2.0f);

        // Specular bevel highlight ring
        g.setColour (juce::Colour (0xaaffffff));
        g.drawEllipse (cx - faceR + 0.8f, cy - faceR + 0.8f, (faceR - 0.8f) * 2.0f, (faceR - 0.8f) * 2.0f, 1.0f);
        g.setColour (juce::Colour (0xff2a2520));
        g.drawEllipse (cx - faceR, cy - faceR, faceR * 2.0f, faceR * 2.0f, 1.2f);

        // Crisp dark-engraved indicator line
        const float sinP = std::sin (angle);
        const float cosP = -std::cos (angle);
        g.setColour (juce::Colour (0xff181512));
        g.drawLine (cx + sinP * (faceR * 0.20f), cy + cosP * (faceR * 0.20f),
                    cx + sinP * (faceR * 0.92f), cy + cosP * (faceR * 0.92f), 2.3f);
    }
    else if (style == 2)
    {
        // =========================================================================
        // STYLE 2: CLA-2A AUTHENTIC "HI FREQ <-> [ FLAT ]" SLOTTED TRIMMER + CURVED ARROW
        // Matches the user's reference photo (image_a7a3e6.png)
        // =========================================================================
        const float screwR = maxRadius * 0.56f;

        // 1. Curved White Arrow above the knob (sweeping clockwise toward [ FLAT ])
        const float arcR = screwR * 1.34f;
        juce::Path arrowArc;
        const float startArc = -0.72f; // top-left
        const float endArc   =  0.78f; // top-right
        arrowArc.addCentredArc (cx, cy, arcR, arcR, 0.0f, startArc, endArc, true);
        g.setColour (juce::Colour (0xffdcd6cc));
        g.strokePath (arrowArc, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));

        // Arrowhead at top-right end of the curved arc
        const float tipX = cx + std::sin (endArc) * arcR;
        const float tipY = cy - std::cos (endArc) * arcR;
        juce::Path arrowHead;
        arrowHead.addTriangle (tipX + 2.5f, tipY + 3.0f,
                               tipX - 6.5f, tipY - 1.5f,
                               tipX + 1.0f, tipY - 6.5f);
        g.fillPath (arrowHead);

        // 2. Dark Recessed Bezel Shadow
        g.setColour (juce::Colours::black.withAlpha (0.65f));
        g.fillEllipse (cx - screwR - 2.5f, cy - screwR - 1.5f, (screwR + 2.5f) * 2.0f, (screwR + 2.5f) * 2.0f);

        // 3. Outer Bronze/Steel Collar Ring
        juce::ColourGradient collarGrad (
            juce::Colour (0xff9e9482), cx - screwR, cy - screwR,
            juce::Colour (0xff3a342c), cx + screwR, cy + screwR, false);
        g.setGradientFill (collarGrad);
        g.fillEllipse (cx - screwR, cy - screwR, screwR * 2.0f, screwR * 2.0f);
        g.setColour (juce::Colour (0xff1a1714));
        g.drawEllipse (cx - screwR, cy - screwR, screwR * 2.0f, screwR * 2.0f, 1.3f);

        // 4. Inner Brushed Silver Screw Head
        const float innerR = screwR * 0.82f;
        juce::ColourGradient headGrad (
            juce::Colour (0xffece7df), cx - innerR * 0.6f, cy - innerR * 0.6f,
            juce::Colour (0xff8c857b), cx + innerR * 0.7f, cy + innerR * 0.7f, false);
        g.setGradientFill (headGrad);
        g.fillEllipse (cx - innerR, cy - innerR, innerR * 2.0f, innerR * 2.0f);

        // Highlight crescent on top-left of screw head
        g.setColour (juce::Colour (0xccffffff));
        g.drawEllipse (cx - innerR + 0.8f, cy - innerR + 0.8f, (innerR - 0.8f) * 2.0f, (innerR - 0.8f) * 2.0f, 1.0f);

        // 5. Full-Diameter Machined Screwdriver Slot (Rotates with parameter!)
        // When sliderPos == 1.0 ([ FLAT ]), slot is vertical (0 rad) or angled toward FLAT
        const float slotAngle = (sliderPos - 0.5f) * 2.1f; // Sweeps cleanly across the screw face
        const float sinS = std::sin (slotAngle);
        const float cosS = -std::cos (slotAngle);
        const float slotLen = innerR * 0.92f;

        // Slot highlight edge
        g.setColour (juce::Colour (0x88ffffff));
        g.drawLine (cx - sinS * slotLen + 1.0f, cy - cosS * slotLen + 1.0f,
                    cx + sinS * slotLen + 1.0f, cy + cosS * slotLen + 1.0f, 2.6f);

        // Deep dark screwdriver slot
        g.setColour (juce::Colour (0xff14110f));
        g.drawLine (cx - sinS * slotLen, cy - cosS * slotLen,
                    cx + sinS * slotLen, cy + cosS * slotLen, 2.6f);
    }
}

void Vibe2ALookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                          bool shouldDrawButtonAsHighlighted, bool)
{
    const auto bounds = button.getLocalBounds().toFloat();
    const float boxSize = 15.0f;
    const float boxY = bounds.getCentreY() - boxSize * 0.5f;
    const juce::Rectangle<float> boxRect (2.0f, boxY, boxSize, boxSize);

    // 3D recessed metallic toggle housing
    g.setColour (juce::Colour (0xff14110f));
    g.fillRoundedRectangle (boxRect, 3.0f);
    g.setColour (shouldDrawButtonAsHighlighted ? juce::Colour (0xffa89882) : juce::Colour (0xff6e6355));
    g.drawRoundedRectangle (boxRect, 3.0f, 1.3f);

    if (button.getToggleState())
    {
        // Glowing warm vintage amber indicator + crisp check
        juce::ColourGradient ledGrad (
            juce::Colour (0xffffb84d), boxRect.getCentreX(), boxRect.getCentreY(),
            juce::Colour (0xffb85d14), boxRect.getX(), boxRect.getY(), true);
        g.setGradientFill (ledGrad);
        g.fillRoundedRectangle (boxRect.reduced (2.5f), 2.0f);

        juce::Path tick;
        tick.startNewSubPath (boxRect.getX() + 3.5f, boxRect.getCentreY() + 0.5f);
        tick.lineTo          (boxRect.getCentreX() - 0.5f, boxRect.getBottom() - 3.5f);
        tick.lineTo          (boxRect.getRight() - 3.2f, boxRect.getY() + 3.5f);
        g.setColour (juce::Colour (0xff1a1209));
        g.strokePath (tick, juce::PathStrokeType (2.0f));
    }

    const bool isUpperPanel = (button.getComponentID() == "vu_toggle");
    g.setColour (isUpperPanel ? juce::Colour (0xff3c352c) : juce::Colour (0xffdfd5c5));
    g.setFont (juce::Font (juce::FontOptions (11.5f, juce::Font::bold)));
    g.drawText (button.getButtonText(),
                juce::Rectangle<float> (boxRect.getRight() + 6.0f, 0.0f, bounds.getWidth() - boxRect.getRight() - 6.0f, bounds.getHeight()),
                juce::Justification::centredLeft, false);
}

void Vibe2ALookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                      int, int, int, int, juce::ComboBox& box)
{
    const juce::Rectangle<float> r (0.0f, 0.0f, static_cast<float> (width), static_cast<float> (height));
    g.setColour (juce::Colour (0xff151210));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (juce::Colour (0xff6e6252));
    g.drawRoundedRectangle (r.reduced (0.6f), 4.0f, 1.2f);

    // Down arrow chevron
    juce::Path arrow;
    const float ax = width - 15.0f;
    const float ay = height * 0.5f;
    arrow.startNewSubPath (ax - 4.5f, ay - 2.5f);
    arrow.lineTo          (ax,        ay + 2.5f);
    arrow.lineTo          (ax + 4.5f, ay - 2.5f);
    g.setColour (juce::Colour (0xffe5d8c3));
    g.strokePath (arrow, juce::PathStrokeType (1.8f));
    juce::ignoreUnused (box);
}

//==============================================================================
Vibe2AProAudioProcessorEditor::Vibe2AProAudioProcessorEditor (Vibe2AProAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setLookAndFeel (&customLookAndFeel);

    // 1. Setup Top Preset Selector Bar
    const auto& presets = audioProcessor.getFactoryPresets();
    for (size_t i = 0; i < presets.size(); ++i)
        presetComboBox.addItem (presets[i].name, static_cast<int> (i) + 1);

    presetComboBox.setSelectedId (audioProcessor.getCurrentProgram() + 1, juce::dontSendNotification);
    presetComboBox.onChange = [this]
    {
        const int idx = presetComboBox.getSelectedId() - 1;
        if (idx >= 0)
            audioProcessor.loadFactoryPreset (idx);
    };
    addAndMakeVisible (presetComboBox);

    auto styleNavBtn = [this] (juce::TextButton& b, int delta)
    {
        b.setColour (juce::TextButton::buttonColourId,  juce::Colour (0xff1c1916));
        b.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffe8dcc8));
        b.onClick = [this, delta]
        {
            const int total = presetComboBox.getNumItems();
            int nextIdx = (presetComboBox.getSelectedItemIndex() + delta + total) % total;
            presetComboBox.setSelectedItemIndex (nextIdx, juce::sendNotification);
        };
        addAndMakeVisible (b);
    };
    styleNavBtn (prevPresetBtn, -1);
    styleNavBtn (nextPresetBtn, +1);

    // 2. Setup Upper Panel Main Bakelite Knobs (GAIN & PEAK REDUCTION)
    auto setupRotary = [this] (juce::Slider& s, int knobStyle)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.getProperties().set ("knobStyle", knobStyle);
        addAndMakeVisible (s);
    };

    setupRotary (gainSlider, 0);
    setupRotary (peakReductionSlider, 0);

    // 3. Setup VU Mode Toggle & Selectable Ratio Buttons (2:1, 4:1, 8:1, LIMIT)
    vuModeToggle.setComponentID ("vu_toggle");
    addAndMakeVisible (vuModeToggle);

    auto setupRatioBtn = [this] (juce::TextButton& btn, int ratioIdx)
    {
        btn.setClickingTogglesState (false);
        btn.onClick = [this, ratioIdx]
        {
            if (auto* param = audioProcessor.getAPVTS().getParameter ("ratio_mode"))
                param->setValueNotifyingHost (param->convertTo0to1 (static_cast<float> (ratioIdx)));
            repaint();
        };
        addAndMakeVisible (btn);
    };
    setupRatioBtn (ratio2Btn,   0);
    setupRatioBtn (ratio4Btn,   1);
    setupRatioBtn (ratio8Btn,   2);
    setupRatioBtn (ratioLimBtn, 3);

    // 4. Setup Lower Panel Precision Controls
    // ATTACK, RELEASE, HI FREQ / [FLAT], 12AX7 TUBE DRIVE, PARALLEL MIX, FAST T4B
    setupRotary (attackSlider,      1);
    setupRotary (releaseSlider,     1);
    setupRotary (hiFreqFlatSlider,  2); // Style 2 = CLA-2A Slotted Screw + Curved Arrow
    setupRotary (tubeDriveSlider,   1);
    setupRotary (parallelMixSlider, 1);

    addAndMakeVisible (fastT4BToggle);

    // 5. Connect APVTS Attachments
    auto& apvts = audioProcessor.getAPVTS();
    gainAttachment          = std::make_unique<SliderAttachment> (apvts, "gain",           gainSlider);
    peakReductionAttachment = std::make_unique<SliderAttachment> (apvts, "peak_reduction", peakReductionSlider);
    attackAttachment        = std::make_unique<SliderAttachment> (apvts, "attack_ms",      attackSlider);
    releaseAttachment       = std::make_unique<SliderAttachment> (apvts, "release_ms",     releaseSlider);
    hiFreqFlatAttachment    = std::make_unique<SliderAttachment> (apvts, "hi_freq_flat",   hiFreqFlatSlider);
    tubeDriveAttachment     = std::make_unique<SliderAttachment> (apvts, "tube_drive",     tubeDriveSlider);
    parallelMixAttachment   = std::make_unique<SliderAttachment> (apvts, "parallel_mix",   parallelMixSlider);
    fastT4BAttachment       = std::make_unique<ButtonAttachment> (apvts, "fast_t4b",       fastT4BToggle);
    vuModeAttachment        = std::make_unique<ButtonAttachment> (apvts, "vu_mode",        vuModeToggle);

    setSize (860, 560);
    startTimerHz (60);
}

Vibe2AProAudioProcessorEditor::~Vibe2AProAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

//==============================================================================
float Vibe2AProAudioProcessorEditor::dbToNormalizedMeterPos (float db) noexcept
{
    const float clamped = juce::jlimit (-20.0f, 3.0f, db);
    constexpr int numMarks = static_cast<int> (sizeof (kVuScaleMarks) / sizeof (kVuScaleMarks[0]));

    for (int i = 0; i < numMarks - 1; ++i)
    {
        if (clamped >= kVuScaleMarks[i].db && clamped <= kVuScaleMarks[i + 1].db)
        {
            const float t = (clamped - kVuScaleMarks[i].db) / (kVuScaleMarks[i + 1].db - kVuScaleMarks[i].db);
            return kVuScaleMarks[i].normPos + t * (kVuScaleMarks[i + 1].normPos - kVuScaleMarks[i].normPos);
        }
    }
    return clamped <= -20.0f ? 0.0f : 1.0f;
}

float Vibe2AProAudioProcessorEditor::normalizedMeterPosToAngle (float normPos) noexcept
{
    // Sweeps symmetrically from -46 degrees (-0.8028 rad) to +46 degrees (+0.8028 rad)
    constexpr float maxSweepRad = juce::MathConstants<float>::pi * 46.0f / 180.0f;
    return (juce::jlimit (0.0f, 1.0f, normPos) * 2.0f - 1.0f) * maxSweepRad;
}

//==============================================================================
void Vibe2AProAudioProcessorEditor::generateWeatheredChassisTexture (int w, int h)
{
    if (w <= 0 || h <= 0)
        return;

    weatheredChassisImage = juce::Image (juce::Image::ARGB, w, h, true);
    juce::Graphics g (weatheredChassisImage);

    std::mt19937 rng (0x56494245); // Deterministic seed so scratches/wear stay rock-solid
    std::uniform_real_distribution<float> uni01 (0.0f, 1.0f);

    const float headerH = 44.0f;
    const float upperH  = 312.0f;
    const float lowerY  = headerH + upperH;
    const float lowerH  = static_cast<float> (h) - lowerY;

    // 1. Top Dark Anodized Header Bar
    juce::ColourGradient headerGrad (
        juce::Colour (0xff201c18), 0.0f, 0.0f,
        juce::Colour (0xff120f0d), 0.0f, headerH, false);
    g.setGradientFill (headerGrad);
    g.fillRect (0.0f, 0.0f, static_cast<float> (w), headerH);

    // 2. Upper Vintage Teletronix Cream/Putty Enamel Faceplate (with 3D radial lighting & edge vignette)
    juce::ColourGradient upperGrad (
        juce::Colour (0xffddd7c6), static_cast<float> (w) * 0.5f, headerH + upperH * 0.38f,
        juce::Colour (0xffbdb5a2), static_cast<float> (w) * 0.5f, headerH + upperH * 1.05f, true);
    upperGrad.addColour (0.65, juce::Colour (0xffcfc8b6));
    g.setGradientFill (upperGrad);
    g.fillRect (0.0f, headerH, static_cast<float> (w), upperH);

    // 3. Lower Dark Charcoal-Bronze Aged Rack Panel
    juce::ColourGradient lowerGrad (
        juce::Colour (0xff2e2823), static_cast<float> (w) * 0.5f, lowerY,
        juce::Colour (0xff1a1613), static_cast<float> (w) * 0.5f, static_cast<float> (h), false);
    g.setGradientFill (lowerGrad);
    g.fillRect (0.0f, lowerY, static_cast<float> (w), lowerH);

    // 4. Fine Brushed Horizontal Metal Grain Across Both Panels
    for (int y = static_cast<int> (headerH); y < h; ++y)
    {
        const float alpha = (y < static_cast<int> (lowerY)) ? 0.022f : 0.032f;
        const bool bright = (uni01 (rng) > 0.5f);
        g.setColour ((bright ? juce::Colours::white : juce::Colours::black).withAlpha (alpha * uni01 (rng)));
        g.drawHorizontalLine (y, 0.0f, static_cast<float> (w));
    }

    // 5. Realistic Micro-Scratches & Aged Patina ("Eskitilmiş / Yıpranmış Metal Dokusu")
    // Inspired by the user's CLA-2A reference photo (image_a7a3e6.png)
    for (int i = 0; i < 420; ++i)
    {
        const float sx = uni01 (rng) * static_cast<float> (w);
        const float sy = headerH + uni01 (rng) * static_cast<float> (h - headerH);
        const float len = 6.0f + uni01 (rng) * 38.0f;
        const float ang = (uni01 (rng) - 0.5f) * 1.4f;
        const float ex = sx + std::cos (ang) * len;
        const float ey = sy + std::sin (ang) * len;

        const bool onUpper = (sy < lowerY);
        const float scratchAlpha = onUpper ? (0.04f + 0.08f * uni01 (rng)) : (0.05f + 0.11f * uni01 (rng));

        // Paired dark groove + bright metallic glint next to it for true 3D scratch depth
        g.setColour (juce::Colours::black.withAlpha (scratchAlpha * 0.8f));
        g.drawLine (sx, sy, ex, ey, 0.8f);
        g.setColour (juce::Colour (0xffe8e2d5).withAlpha (scratchAlpha));
        g.drawLine (sx, sy + 0.8f, ex, ey + 0.8f, 0.7f);
    }

    // 6. Worn / Chipped Paint & Exposed Raw Steel Along All Chassis Borders ("Yırtılmış / Aşınmış Kenarlar")
    auto drawWeatheredEdgeChip = [&] (float cx, float cy, float rx, float ry, bool darkBase)
    {
        juce::Path chip;
        constexpr int pts = 14;
        for (int p = 0; p <= pts; ++p)
        {
            const float a = juce::MathConstants<float>::twoPi * static_cast<float> (p) / static_cast<float> (pts);
            const float jitter = 0.55f + 0.65f * uni01 (rng);
            const float px = cx + std::cos (a) * rx * jitter;
            const float py = cy + std::sin (a) * ry * jitter;
            if (p == 0) chip.startNewSubPath (px, py);
            else        chip.lineTo (px, py);
        }
        chip.closeSubPath();

        // Dark oxidized under-layer
        g.setColour (juce::Colour (0xff181512).withAlpha (0.65f));
        g.fillPath (chip);

        // Exposed raw brushed steel center inside the chipped paint
        g.setColour ((darkBase ? juce::Colour (0xff8a847a) : juce::Colour (0xff6e685f)).withAlpha (0.55f));
        g.strokePath (chip, juce::PathStrokeType (1.1f));
    };

    // Scatter authentic paint wear chips along the top/bottom/left/right borders and panel seam
    for (int i = 0; i < 95; ++i)
    {
        const float edgeSel = uni01 (rng);
        if (edgeSel < 0.30f)
        {
            // Left or Right rack border wear
            const float ex = (uni01 (rng) < 0.5f) ? (4.0f + uni01 (rng) * 10.0f) : (w - 4.0f - uni01 (rng) * 10.0f);
            const float ey = headerH + 8.0f + uni01 (rng) * (h - headerH - 16.0f);
            drawWeatheredEdgeChip (ex, ey, 3.0f + uni01 (rng) * 7.0f, 5.0f + uni01 (rng) * 14.0f, ey > lowerY);
        }
        else if (edgeSel < 0.65f)
        {
            // Seam between Upper Cream Faceplate and Lower Dark Panel
            const float ex = 10.0f + uni01 (rng) * (w - 20.0f);
            const float ey = lowerY + (uni01 (rng) - 0.5f) * 5.0f;
            drawWeatheredEdgeChip (ex, ey, 5.0f + uni01 (rng) * 14.0f, 2.0f + uni01 (rng) * 3.5f, true);
        }
        else
        {
            // Top / Bottom chassis lip wear
            const float ex = 10.0f + uni01 (rng) * (w - 20.0f);
            const float ey = (uni01 (rng) < 0.5f) ? (headerH + 2.0f + uni01 (rng) * 4.0f) : (h - 3.0f - uni01 (rng) * 5.0f);
            drawWeatheredEdgeChip (ex, ey, 6.0f + uni01 (rng) * 15.0f, 2.5f + uni01 (rng) * 4.0f, ey > lowerY);
        }
    }

    // 7. Deep 3D Beveled Outer Steel Frame & Panel Seam Shadows
    // Header bottom bevel
    g.setColour (juce::Colours::black.withAlpha (0.85f));
    g.fillRect (0.0f, headerH - 2.0f, static_cast<float> (w), 3.0f);
    g.setColour (juce::Colour (0x66ffffff));
    g.fillRect (0.0f, headerH + 1.0f, static_cast<float> (w), 1.5f);

    // Seam between Upper and Lower panels (3D metal lip + cast shadow)
    g.setColour (juce::Colour (0x88ffffff));
    g.fillRect (6.0f, lowerY - 3.0f, static_cast<float> (w) - 12.0f, 1.5f);
    g.setColour (juce::Colour (0xff0d0b09));
    g.fillRect (0.0f, lowerY - 1.5f, static_cast<float> (w), 3.5f);
    juce::ColourGradient seamShadow (
        juce::Colours::black.withAlpha (0.55f), 0.0f, lowerY + 2.0f,
        juce::Colours::transparentBlack,        0.0f, lowerY + 14.0f, false);
    g.setGradientFill (seamShadow);
    g.fillRect (0.0f, lowerY + 2.0f, static_cast<float> (w), 12.0f);

    // Outer 3D Rack Bevel Border
    g.setColour (juce::Colour (0xff0c0a08));
    g.drawRect (0, 0, w, h, 3);
    g.setColour (juce::Colour (0x44ffffff));
    g.drawRect (3, 3, w - 6, h - 6, 1);

    // 8. 3D Slotted Rack Screws with Circular Screwdriver Scratches Around Them
    auto draw3DHardwareScrew = [&] (float sx, float sy, float r, float slotAngle)
    {
        // Circular paint-wear scratch ring around screw hole
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.drawEllipse (sx - r * 1.35f, sy - r * 1.35f, r * 2.7f, r * 2.7f, 1.2f);
        g.setColour (juce::Colour (0x55d8d0c0));
        g.drawEllipse (sx - r * 1.22f, sy - r * 1.22f, r * 2.44f, r * 2.44f, 0.9f);

        // Recessed hole shadow
        g.setColour (juce::Colours::black.withAlpha (0.65f));
        g.fillEllipse (sx - r + 1.2f, sy - r + 1.8f, r * 2.0f, r * 2.0f);

        // Metallic domed screw head
        juce::ColourGradient screwGrad (
            juce::Colour (0xffd8d2c7), sx - r * 0.6f, sy - r * 0.6f,
            juce::Colour (0xff4e4840), sx + r * 0.7f, sy + r * 0.7f, true);
        g.setGradientFill (screwGrad);
        g.fillEllipse (sx - r, sy - r, r * 2.0f, r * 2.0f);

        g.setColour (juce::Colour (0xff221e1a));
        g.drawEllipse (sx - r, sy - r, r * 2.0f, r * 2.0f, 1.1f);

        // Screwdriver slot + bottom edge highlight
        const float dx = std::cos (slotAngle) * (r * 0.78f);
        const float dy = std::sin (slotAngle) * (r * 0.78f);
        g.setColour (juce::Colour (0x88ffffff));
        g.drawLine (sx - dx, sy - dy + 1.0f, sx + dx, sy + dy + 1.0f, 1.8f);
        g.setColour (juce::Colour (0xff1c1815));
        g.drawLine (sx - dx, sy - dy, sx + dx, sy + dy, 2.0f);
    };

    draw3DHardwareScrew (24.0f,       headerH + 22.0f,      8.5f,  0.42f);
    draw3DHardwareScrew (w - 24.0f,   headerH + 22.0f,      8.5f, -0.31f);
    draw3DHardwareScrew (24.0f,       lowerY - 22.0f,       8.5f,  1.15f);
    draw3DHardwareScrew (w - 24.0f,   lowerY - 22.0f,       8.5f, -0.85f);
    draw3DHardwareScrew (24.0f,       h - 22.0f,            8.5f,  0.28f);
    draw3DHardwareScrew (w - 24.0f,   h - 22.0f,            8.5f,  0.74f);
}

//==============================================================================
void Vibe2AProAudioProcessorEditor::drawCalibratedVUMeter (juce::Graphics& g, juce::Rectangle<float> bounds)
{
    // 1. Heavy 3D Molded Bakelite Outer Bezel
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (bounds.translated (3.0f, 5.0f), 12.0f);

    juce::ColourGradient bezelGrad (
        juce::Colour (0xff3a3530), bounds.getX(), bounds.getY(),
        juce::Colour (0xff110f0d), bounds.getRight(), bounds.getBottom(), false);
    g.setGradientFill (bezelGrad);
    g.fillRoundedRectangle (bounds, 10.0f);

    g.setColour (juce::Colour (0x66ffffff));
    g.drawRoundedRectangle (bounds.reduced (1.0f), 9.5f, 1.2f);

    // 2. Inner Illuminated Vintage Amber VU Face
    const auto faceRect = bounds.reduced (9.0f);
    const bool inSweetSpot = (! audioProcessor.isOutputMeterMode()) && (displayedSweetSpot > 0.35f);

    juce::ColourGradient faceGrad (
        juce::Colour (inSweetSpot ? 0xfffff5d6 : 0xfffff1cf), faceRect.getCentreX(), faceRect.getY() + 18.0f,
        juce::Colour (inSweetSpot ? 0xffdf9b38 : 0xffd6963e), faceRect.getCentreX(), faceRect.getBottom(), true);
    faceGrad.addColour (0.65, juce::Colour (0xfff2cf88));
    g.setGradientFill (faceGrad);
    g.fillRoundedRectangle (faceRect, 6.0f);

    // Pivot point for the needle & scale arcs (below the bottom edge of the meter window)
    const float pivotX = faceRect.getCentreX();
    const float pivotY = faceRect.getBottom() + 34.0f;
    const float arcRadius = faceRect.getHeight() * 0.94f;

    g.saveState();
    g.reduceClipRegion (faceRect.toNearestInt());

    // 3. Draw "-7 to -5 dB" CLA-2A Sweet Spot Golden Zone Highlight Band on the Scale!
    const float sweetStartAng = normalizedMeterPosToAngle (dbToNormalizedMeterPos (-7.0f));
    const float sweetEndAng   = normalizedMeterPosToAngle (dbToNormalizedMeterPos (-5.0f));

    juce::Path sweetArc;
    sweetArc.addCentredArc (pivotX, pivotY, arcRadius + 4.0f, arcRadius + 4.0f, 0.0f, sweetStartAng, sweetEndAng, true);
    g.setColour (juce::Colour (inSweetSpot ? 0xffd95b00 : 0xffb86b1b).withAlpha (inSweetSpot ? 0.42f : 0.22f));
    g.strokePath (sweetArc, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));

    // Subtle red overload arc from 0 to +3 dB
    const float zeroAng  = normalizedMeterPosToAngle (dbToNormalizedMeterPos (0.0f));
    const float plus3Ang = normalizedMeterPosToAngle (dbToNormalizedMeterPos (3.0f));
    juce::Path redArc;
    redArc.addCentredArc (pivotX, pivotY, arcRadius - 1.5f, arcRadius - 1.5f, 0.0f, zeroAng, plus3Ang, true);
    g.setColour (juce::Colour (0xffb8241a).withAlpha (0.75f));
    g.strokePath (redArc, juce::PathStrokeType (2.2f));

    // Main black scale baseline arc (-20 to 0 dB)
    const float minus20Ang = normalizedMeterPosToAngle (0.0f);
    juce::Path mainArc;
    mainArc.addCentredArc (pivotX, pivotY, arcRadius - 1.5f, arcRadius - 1.5f, 0.0f, minus20Ang, zeroAng, true);
    g.setColour (juce::Colour (0xff2b2219).withAlpha (0.75f));
    g.strokePath (mainArc, juce::PathStrokeType (1.5f));

    // 4. Draw Symmetric, Accurately Calibrated Scale Ticks & Labels (-20, -15, -10, -7, -5, -3, -2, -1, 0, +1, +2, +3)
    constexpr int numMarks = static_cast<int> (sizeof (kVuScaleMarks) / sizeof (kVuScaleMarks[0]));
    for (int i = 0; i < numMarks; ++i)
    {
        const auto& mark = kVuScaleMarks[i];
        const float ang = normalizedMeterPosToAngle (mark.normPos);
        const float sinA = std::sin (ang);
        const float cosA = -std::cos (ang);

        const bool isMajor = (mark.label != nullptr);
        const bool isSweetMark = (std::abs (mark.db - (-7.0f)) < 0.1f || std::abs (mark.db - (-5.0f)) < 0.1f);

        const float rInner = arcRadius - 2.0f;
        const float rOuter = arcRadius + (isMajor ? 11.5f : 6.0f);

        juce::Colour tickCol = mark.isRed ? juce::Colour (0xffb82016)
                             : (isSweetMark ? juce::Colour (0xff8a3800) : juce::Colour (0xff221a12));

        g.setColour (tickCol);
        g.drawLine (pivotX + sinA * rInner, pivotY + cosA * rInner,
                    pivotX + sinA * rOuter, pivotY + cosA * rOuter,
                    isMajor ? 1.85f : 1.1f);

        if (isMajor)
        {
            const float labelR = arcRadius + 22.5f;
            const float lx = pivotX + sinA * labelR;
            const float ly = pivotY + cosA * labelR;

            g.setFont (juce::Font (juce::FontOptions (isSweetMark ? 11.0f : 10.2f, juce::Font::bold)));
            g.setColour (tickCol);
            g.drawText (mark.label,
                        juce::Rectangle<float> (lx - 15.0f, ly - 8.0f, 30.0f, 16.0f),
                        juce::Justification::centred, false);
        }
    }

    // Sweet Spot label above -7..-5 zone
    g.setFont (juce::Font (juce::FontOptions (8.5f, juce::Font::bold)));
    g.setColour (juce::Colour (inSweetSpot ? 0xffb33c00 : 0xff7a5220));
    g.drawText ("SWEET SPOT",
                juce::Rectangle<float> (pivotX - 42.0f, faceRect.getY() + 4.0f, 84.0f, 12.0f),
                juce::Justification::centred, false);

    // Legend on dial face
    g.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
    g.setColour (juce::Colour (0xff4a3822));
    g.drawText ("TELE-OPTO T4B VU",
                juce::Rectangle<float> (faceRect.getX(), faceRect.getBottom() - 34.0f, faceRect.getWidth(), 15.0f),
                juce::Justification::centred, false);

    // 5. Draw Physical VU Needle (Mapped via the exact same lookup table as the ticks!)
    const float needleNorm = dbToNormalizedMeterPos (displayedMeterDb);
    const float needleAng  = normalizedMeterPosToAngle (needleNorm);
    const float sinN = std::sin (needleAng);
    const float cosN = -std::cos (needleAng);
    const float needleLen = arcRadius + 16.0f;

    // Soft needle shadow cast onto the paper dial
    g.setColour (juce::Colours::black.withAlpha (0.28f));
    g.drawLine (pivotX + 3.0f, pivotY + 3.0f,
                pivotX + sinN * needleLen + 3.5f, pivotY + cosN * needleLen + 3.5f, 2.6f);

    // Tapered precision black needle + spade tip
    g.setColour (juce::Colour (0xff16120e));
    g.drawLine (pivotX, pivotY,
                pivotX + sinN * needleLen, pivotY + cosN * needleLen, 2.2f);

    // Needle pivot dome at bottom center
    g.setColour (juce::Colour (0xff1a1612));
    g.fillEllipse (pivotX - 18.0f, faceRect.getBottom() - 9.0f, 36.0f, 22.0f);

    // 6. Live Digital Precision dB Badge at Bottom of Meter Face
    const juce::String modeStr = audioProcessor.isOutputMeterMode() ? "OUT: " : "GR: ";
    const juce::String dbStr   = juce::String (displayedMeterDb, 1) + " dB";
    const juce::String sweetBadge = inSweetSpot ? "  [SWEET SPOT]" : "";

    juce::Rectangle<float> readoutPill (faceRect.getX() + 8.0f, faceRect.getBottom() - 18.0f, 118.0f, 14.5f);
    g.setColour (juce::Colour (0xcc1c1610));
    g.fillRoundedRectangle (readoutPill, 3.5f);
    g.setColour (inSweetSpot ? juce::Colour (0xffffb84d) : juce::Colour (0xffe8d8b8));
    g.setFont (juce::Font (juce::FontOptions (9.8f, juce::Font::bold)));
    g.drawText (modeStr + dbStr + sweetBadge, readoutPill, juce::Justification::centred, false);

    // 7. Inner 3D Recessed Top Shadow & Curved Glass Reflection Overlay
    juce::ColourGradient innerTopShadow (
        juce::Colours::black.withAlpha (0.42f), faceRect.getX(), faceRect.getY(),
        juce::Colours::transparentBlack,        faceRect.getX(), faceRect.getY() + 18.0f, false);
    g.setGradientFill (innerTopShadow);
    g.fillRect (faceRect.getX(), faceRect.getY(), faceRect.getWidth(), 18.0f);

    // Curved glass reflection sheen across upper half
    juce::Path glassSheen;
    glassSheen.startNewSubPath (faceRect.getX(), faceRect.getY());
    glassSheen.lineTo (faceRect.getRight(), faceRect.getY());
    glassSheen.lineTo (faceRect.getRight(), faceRect.getY() + faceRect.getHeight() * 0.36f);
    glassSheen.quadraticTo (faceRect.getCentreX(), faceRect.getY() + faceRect.getHeight() * 0.52f,
                            faceRect.getX(), faceRect.getY() + faceRect.getHeight() * 0.42f);
    glassSheen.closeSubPath();

    juce::ColourGradient sheenGrad (
        juce::Colours::white.withAlpha (0.20f), faceRect.getX(), faceRect.getY(),
        juce::Colours::white.withAlpha (0.02f), faceRect.getX(), faceRect.getY() + faceRect.getHeight() * 0.5f, false);
    g.setGradientFill (sheenGrad);
    g.fillPath (glassSheen);

    g.restoreState();

    // Inner metal rim border
    g.setColour (juce::Colour (0xff0e0c0a));
    g.drawRoundedRectangle (faceRect, 6.0f, 1.5f);
}

void Vibe2AProAudioProcessorEditor::drawValueReadoutBox (juce::Graphics& g, juce::Rectangle<float> bounds,
                                                         const juce::String& text, bool highlight)
{
    g.setColour (juce::Colour (0xff14110e));
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (highlight ? juce::Colour (0xffd4882a) : juce::Colour (0xff52483d));
    g.drawRoundedRectangle (bounds, 4.0f, 1.1f);

    g.setColour (highlight ? juce::Colour (0xffffc266) : juce::Colour (0xffe8dcc8));
    g.setFont (juce::Font (juce::FontOptions (11.5f, juce::Font::bold)));
    g.drawText (text, bounds, juce::Justification::centred, false);
}

//==============================================================================
void Vibe2AProAudioProcessorEditor::paint (juce::Graphics& g)
{
    // 1. Draw Pre-Rendered 3D Weathered & Scratched Vintage Hardware Chassis
    if (weatheredChassisImage.isValid())
        g.drawImageAt (weatheredChassisImage, 0, 0);

    // 2. Top Header Bar Title & Preset Label
    g.setColour (juce::Colour (0xfff0e4ce));
    g.setFont (juce::Font (juce::FontOptions (17.0f, juce::Font::bold)));
    g.drawText ("VIBE-2A PRO", 18, 0, 220, 44, juce::Justification::centredLeft, false);

    g.setColour (juce::Colour (0xffb89a6c));
    g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
    g.drawText ("T4B ELECTRO-OPTICAL TUBE COMPRESSOR", 148, 0, 260, 44, juce::Justification::centredLeft, false);

    g.setColour (juce::Colour (0xffcbbba2));
    g.setFont (juce::Font (juce::FontOptions (11.5f, juce::Font::bold)));
    g.drawText ("PRESET:", 455, 0, 60, 44, juce::Justification::centredRight, false);

    // 3. Upper Panel Section Labels (GAIN & PEAK REDUCTION)
    g.setColour (juce::Colour (0xff241f1a));
    g.setFont (juce::Font (juce::FontOptions (14.5f, juce::Font::bold)));
    g.drawText ("GAIN",           48, 56, 220, 22, juce::Justification::centred, false);
    g.drawText ("PEAK REDUCTION", 592, 56, 220, 22, juce::Justification::centred, false);

    // Digital / Engraved Readout Badges under GAIN & PEAK REDUCTION
    drawValueReadoutBox (g, juce::Rectangle<float> (122.0f, 300.0f, 72.0f, 22.0f),
                         juce::String (gainSlider.getValue(), 1));
    drawValueReadoutBox (g, juce::Rectangle<float> (666.0f, 300.0f, 72.0f, 22.0f),
                         juce::String (peakReductionSlider.getValue(), 1),
                         displayedSweetSpot > 0.35f);

    // 4. Center Calibrated VU Meter (-20 to +3 dB)
    drawCalibratedVUMeter (g, juce::Rectangle<float> (290.0f, 58.0f, 280.0f, 166.0f));

    // 5. Glowing 12AX7 / T4B Vacuum Tube Grille Window Below VU Meter
    // Glow intensifies dynamically when the compressor sits in the -7 to -5 dB Sweet Spot!
    const juce::Rectangle<float> grilleRect (355.0f, 230.0f, 150.0f, 28.0f);
    g.setColour (juce::Colour (0xff120e0b));
    g.fillRoundedRectangle (grilleRect, 5.0f);

    const float glowIntensity = juce::jlimit (0.20f, 1.0f,
        0.28f + 0.45f * displayedSweetSpot + 0.27f * static_cast<float> (tubeDriveSlider.getValue() * 0.01));

    juce::ColourGradient tubeGlow (
        juce::Colour (0xffff9d2e).withAlpha (glowIntensity), grilleRect.getCentreX(), grilleRect.getCentreY(),
        juce::Colour (0xff2a1004).withAlpha (0.15f),         grilleRect.getX(),       grilleRect.getY(), true);
    g.setGradientFill (tubeGlow);
    g.fillRoundedRectangle (grilleRect.reduced (2.0f), 4.0f);

    // Horizontal dark metal vent bars across the glowing tube window
    g.setColour (juce::Colour (0xff16120f));
    for (float gy = grilleRect.getY() + 5.0f; gy < grilleRect.getBottom() - 3.0f; gy += 4.5f)
        g.fillRect (grilleRect.getX() + 4.0f, gy, grilleRect.getWidth() - 8.0f, 2.0f);

    g.setColour (juce::Colour (0xff52483e));
    g.drawRoundedRectangle (grilleRect, 5.0f, 1.3f);

    // 6. Selectable Ratio Hardware Bank (2:1, 4:1, 8:1, LIMIT)
    const int activeRatio = static_cast<int> (audioProcessor.getAPVTS().getRawParameterValue ("ratio_mode")->load());
    g.setColour (juce::Colour (0xff2c251e));
    g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
    g.drawText ("RATIO:", 288, 297, 50, 24, juce::Justification::centredLeft, false);

    juce::TextButton* ratioBtns[4] = { &ratio2Btn, &ratio4Btn, &ratio8Btn, &ratioLimBtn };
    for (int i = 0; i < 4; ++i)
    {
        const bool isSelected = (i == activeRatio);
        ratioBtns[i]->setColour (juce::TextButton::buttonColourId,
                                 isSelected ? juce::Colour (0xffc97a1d) : juce::Colour (0xff26211c));
        ratioBtns[i]->setColour (juce::TextButton::textColourOffId,
                                 isSelected ? juce::Colour (0xff140e08) : juce::Colour (0xffdfd3c0));
    }

    // =========================================================================
    // 7. LOWER PANEL PRECISION CONTROLS & CLA-2A "HI FREQ <-> [ FLAT ]" MODULE
    // =========================================================================
    g.setColour (juce::Colour (0xffe8dcc8));
    g.setFont (juce::Font (juce::FontOptions (11.5f, juce::Font::bold)));

    // Column 2: ATTACK
    g.drawText ("ATTACK", 182, 374, 116, 18, juce::Justification::centred, false);
    drawValueReadoutBox (g, juce::Rectangle<float> (202.0f, 502.0f, 76.0f, 21.0f),
                         juce::String (attackSlider.getValue(), 1) + " ms");

    // Column 3: RELEASE
    g.drawText ("RELEASE", 312, 374, 116, 18, juce::Justification::centred, false);
    drawValueReadoutBox (g, juce::Rectangle<float> (332.0f, 502.0f, 76.0f, 21.0f),
                         juce::String ( static_cast<int> (releaseSlider.getValue()) ) + " ms");

    // Column 4: CLA-2A Style "HI FREQ <-> [ FLAT ]" Recessed Sub-Panel (Matches image_a7a3e6.png!)
    const juce::Rectangle<float> hfPlate (436.0f, 392.0f, 152.0f, 102.0f);
    g.setColour (juce::Colour (0xff1c1815));
    g.fillRoundedRectangle (hfPlate, 6.0f);
    g.setColour (juce::Colour (0xff52483d));
    g.drawRoundedRectangle (hfPlate, 6.0f, 1.2f);

    g.setColour (juce::Colour (0xffe8dcc8));
    g.setFont (juce::Font (juce::FontOptions (11.5f, juce::Font::bold)));
    g.drawText ("SIDECHAIN R37", 436, 374, 152, 18, juce::Justification::centred, false);

    // "HI FREQ" on the left of the slotted screw, "[ FLAT ]" on the right (clickable!)
    const float hfVal = static_cast<float> (hiFreqFlatSlider.getValue());
    const bool isFlatActive = (hfVal >= 97.5f);

    g.setColour (isFlatActive ? juce::Colour (0xffa89f91) : juce::Colour (0xffffb84d));
    g.setFont (juce::Font (juce::FontOptions (11.5f, juce::Font::bold)));
    g.drawText ("HI FREQ", 440, 434, 48, 22, juce::Justification::centredLeft, false);

    flatLabelClickArea = juce::Rectangle<float> (536.0f, 432.0f, 48.0f, 24.0f);
    g.setColour (isFlatActive ? juce::Colour (0xffffc266) : juce::Colour (0xffcfc5b4));
    g.drawText ("[ FLAT ]", flatLabelClickArea, juce::Justification::centred, false);

    const juce::String hfReadoutText = isFlatActive
        ? "[ FLAT ]"
        : ("HF +" + juce::String ((100.0f - hfVal) * 0.14f, 1) + " dB");
    drawValueReadoutBox (g, juce::Rectangle<float> (470.0f, 502.0f, 84.0f, 21.0f),
                         hfReadoutText, ! isFlatActive);

    // Column 5: 12AX7 TUBE DRIVE
    g.setColour (juce::Colour (0xffe8dcc8));
    g.setFont (juce::Font (juce::FontOptions (11.5f, juce::Font::bold)));
    g.drawText ("12AX7 TUBE DRIVE", 596, 374, 124, 18, juce::Justification::centred, false);
    drawValueReadoutBox (g, juce::Rectangle<float> (620.0f, 502.0f, 76.0f, 21.0f),
                         juce::String (tubeDriveSlider.getValue(), 1) + " %");

    // Column 6: PARALLEL MIX
    g.setColour (juce::Colour (0xffe8dcc8));
    g.setFont (juce::Font (juce::FontOptions (11.5f, juce::Font::bold)));
    g.drawText ("PARALLEL MIX", 726, 374, 118, 18, juce::Justification::centred, false);
    drawValueReadoutBox (g, juce::Rectangle<float> (747.0f, 502.0f, 76.0f, 21.0f),
                         juce::String (parallelMixSlider.getValue(), 1) + " %");

    // Left Status Box on Lower Panel (Live Sweet Spot & Mode Indicator)
    const juce::Rectangle<float> statusBox (22.0f, 435.0f, 148.0f, 88.0f);
    g.setColour (juce::Colour (0xff151210));
    g.fillRoundedRectangle (statusBox, 5.0f);
    g.setColour (displayedSweetSpot > 0.35f ? juce::Colour (0xffd4882a) : juce::Colour (0xff4a4137));
    g.drawRoundedRectangle (statusBox, 5.0f, 1.2f);

    g.setColour (juce::Colour (0xffb8aa96));
    g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
    g.drawText ("LIVE T4B CELL STATUS", statusBox.getX() + 6.0f, statusBox.getY() + 6.0f, statusBox.getWidth() - 12.0f, 14.0f,
                juce::Justification::centred, false);

    g.setColour (displayedSweetSpot > 0.35f ? juce::Colour (0xffffb84d) : juce::Colour (0xffe8dcc8));
    g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    g.drawText ("GR: " + juce::String (displayedGrDb, 1) + " dB",
                statusBox.getX() + 6.0f, statusBox.getY() + 26.0f, statusBox.getWidth() - 12.0f, 20.0f,
                juce::Justification::centred, false);

    const juce::String characterState = (displayedSweetSpot > 0.35f)
        ? "SWEET SPOT (-7..-5 dB)"
        : (displayedGrDb < -8.0f ? "DEEP OPTO CLAMP" : "CLEAN TUBE WARMTH");
    g.setColour (displayedSweetSpot > 0.35f ? juce::Colour (0xffffc266) : juce::Colour (0xff9c9080));
    g.setFont (juce::Font (juce::FontOptions (9.5f, juce::Font::bold)));
    g.drawText (characterState,
                statusBox.getX() + 4.0f, statusBox.getY() + 52.0f, statusBox.getWidth() - 8.0f, 26.0f,
                juce::Justification::centred, false);
}

void Vibe2AProAudioProcessorEditor::mouseDown (const juce::MouseEvent& event)
{
    // Clicking "[ FLAT ]" snaps the HI FREQ / FLAT control immediately to 100.0 ([ FLAT ])
    // or toggles back to 65.0 (sweet vocal HF emphasis) if already flat!
    if (flatLabelClickArea.contains (event.position))
    {
        const float currentVal = static_cast<float> (hiFreqFlatSlider.getValue());
        const float newVal = (currentVal >= 97.5f) ? 62.0f : 100.0f;
        hiFreqFlatSlider.setValue (newVal, juce::sendNotificationSync);
        repaint();
        return;
    }

    AudioProcessorEditor::mouseDown (event);
}

void Vibe2AProAudioProcessorEditor::resized()
{
    generateWeatheredChassisTexture (getWidth(), getHeight());

    // Top Preset Navigation Bar
    prevPresetBtn.setBounds  (522, 9, 28, 26);
    presetComboBox.setBounds (554, 9, 260, 26);
    nextPresetBtn.setBounds  (818, 9, 28, 26);

    // Upper Faceplate: Large Scalloped Black Bakelite Knobs (GAIN & PEAK REDUCTION)
    gainSlider.setBounds          (50,  80, 216, 216);
    peakReductionSlider.setBounds (594, 80, 216, 216);

    // Center Controls Below VU Meter: VU Mode Toggle + 4 Selectable Ratio Buttons (2:1, 4:1, 8:1, LIMIT)
    vuModeToggle.setBounds (370, 265, 130, 24);

    ratio2Btn.setBounds   (342, 297, 52, 24);
    ratio4Btn.setBounds   (398, 297, 52, 24);
    ratio8Btn.setBounds   (454, 297, 52, 24);
    ratioLimBtn.setBounds (510, 297, 62, 24);

    // Lower Faceplate: Fast T4B Toggle + 5 Precision Rotary Controls
    fastT4BToggle.setBounds     (22,  394, 150, 26);

    attackSlider.setBounds      (190, 396, 100, 100);
    releaseSlider.setBounds     (320, 396, 100, 100);
    hiFreqFlatSlider.setBounds  (476, 408,  72,  72); // Centered between "HI FREQ" and "[ FLAT ]"
    tubeDriveSlider.setBounds   (608, 396, 100, 100);
    parallelMixSlider.setBounds (735, 396, 100, 100);
}

void Vibe2AProAudioProcessorEditor::timerCallback()
{
    const float targetMeter = audioProcessor.getMeterDisplayDb();
    const float targetGr    = audioProcessor.getLiveGainReductionDb();
    const float targetSweet = audioProcessor.getSweetSpotActivity();

    displayedMeterDb   += 0.35f * (targetMeter - displayedMeterDb);
    displayedGrDb      += 0.35f * (targetGr    - displayedGrDb);
    displayedSweetSpot += 0.25f * (targetSweet - displayedSweetSpot);

    repaint();
}
