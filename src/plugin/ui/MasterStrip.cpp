#include "MasterStrip.h"

#include "Fonts.h"
#include "SegmentDisplay.h"
#include "Theme.h"

namespace rtxx::ui {
namespace {

// Posiciones dentro de la tira (la tira empieza en y = 836 del editor y ocupa todo el
// ancho). Salen del grupo "IN-OUT STUFF" del motor (12, 541) x kScale.
constexpr int kKnobSize = 68;
constexpr int kKnobTop = 12;
constexpr int kCaptionTop = 85;
const juce::Rectangle<int> kInGain(112, kKnobTop, kKnobSize, kKnobSize);
const juce::Rectangle<int> kTone(689, kKnobTop, kKnobSize, kKnobSize);
const juce::Rectangle<int> kWidth(973, kKnobTop, kKnobSize, kKnobSize);
const juce::Rectangle<int> kOutGain(1095, kKnobTop, kKnobSize, kKnobSize);
const juce::Rectangle<int> kEqEnable(228, 20, ParamToggle::preferredWidth,
                                     ParamToggle::preferredHeight);
const juce::Rectangle<int> kCut(384, 14, 196, 86);
const juce::Rectangle<int> kModes[] = {{334, 20, 44, 56}, {590, 20, 44, 56}, {766, 20, 44, 56}};
constexpr int kSeparators[] = {42, 199, 866};

ParamKnob* makeKnob(const HostParamLookup& lookup, int id) {
    auto* knob = new ParamKnob(lookup(id), id, DialFinish::Dark, colours::lcdInk);
    knob->setCaptionVisible(false);
    knob->setReadoutVisible(false);
    return knob;
}

}  // namespace

MasterStrip::MasterStrip(const HostParamLookup& lookup) {
    setOpaque(false);

    // GAIN de entrada y de salida: los extremos del aparato.
    inGain_.reset(makeKnob(lookup, kInGainId));
    outGain_.reset(makeKnob(lookup, kOutGainId));
    width_.reset(makeKnob(lookup, kWidthKnob.knobs[0]));
    addAndMakeVisible(*inGain_);
    addAndMakeVisible(*outGain_);
    addAndMakeVisible(*width_);
    knobs_.push_back(inGain_.get());
    knobs_.push_back(width_.get());
    knobs_.push_back(outGain_.get());
    hostIds_.push_back(kInGainId);
    hostIds_.push_back(kWidthKnob.knobs[0]);
    hostIds_.push_back(kOutGainId);

    // EQ: es un grupo con su puerta, un CUT de dos tiradores, sus dos modos y el Tone con el
    // suyo. Los ids 50..55 son: Cut Lo, Cut Hi, modo lo, modo hi, Tone y modo del Tone.
    if (kEqStrip.enableHostId >= 0) {
        addAndMakeVisible(*(eqEnable_ = std::make_unique<ParamToggle>(lookup(kEqStrip.enableHostId),
                                                                     kEqStrip.enableHostId)));
        hostIds_.push_back(kEqStrip.enableHostId);
    }
    eqCut_ = std::make_unique<ParamRangeSlider>(lookup(50), lookup(51), "CUT", colours::lcdInk,
                                                false, 50, 51);
    addAndMakeVisible(*eqCut_);
    hostIds_.push_back(50);
    hostIds_.push_back(51);

    tone_.reset(makeKnob(lookup, 54));
    addAndMakeVisible(*tone_);
    knobs_.push_back(tone_.get());
    hostIds_.push_back(54);

// Modos: par de teclas apiladas; arriba = 0, abajo = 1.
    const int modeIds[] = {52, 53, 55};
    const ModeIcon icons[][2] = {{ModeIcon::CutLoSoft, ModeIcon::CutLoHard},
                                 {ModeIcon::CutHiSoft, ModeIcon::CutHiHard},
                                 {ModeIcon::ToneTilt, ModeIcon::ToneMid}};
    for (int i = 0; i < 3; ++i) {
        auto* button = new ParamButton(lookup(modeIds[i]), "", colours::lcdInk, modeIds[i]);
        button->setStacked(icons[i][0], icons[i][1]);
        addAndMakeVisible(modeButtons_.add(button));
        hostIds_.push_back(modeIds[i]);
    }

    readouts_ = {{inGain_.get(), &lookup(kInGainId), "GAIN", {}},
                 {tone_.get(), &lookup(54), "TONE", {}},
                 {width_.get(), &lookup(kWidthKnob.knobs[0]), "WIDTH", {}},
                 {outGain_.get(), &lookup(kOutGainId), "GAIN", {}}};
}

void MasterStrip::setControlsEnabled(bool shouldBeEnabled) {
    for (auto* knob : knobs_) knob->setEnabled(shouldBeEnabled);
    if (eqEnable_ != nullptr) eqEnable_->setEnabled(shouldBeEnabled);
    if (eqCut_ != nullptr) eqCut_->setEnabled(shouldBeEnabled);
    for (auto* button : modeButtons_) button->setEnabled(shouldBeEnabled);
}

juce::Rectangle<int> MasterStrip::readoutArea(const Readout& readout) const {
    const auto knob = readout.knob->getBounds();
    return {knob.getCentreX() - 60, kCaptionTop, 120, 16};
}

void MasterStrip::refreshReadouts() {
    for (auto& readout : readouts_) {
        const juce::String text =
            readout.parameter->getText(readout.parameter->getValue(), 16).trim();
        if (text == readout.text) continue;
        readout.text = text;
        repaint(readoutArea(readout));
    }
}

void MasterStrip::paint(juce::Graphics& g) {
    // Separadores y rotulos verticales: IN | EQ | OUT.
    g.setColour(colours::hairline);
    for (int x : kSeparators) g.fillRect(x, 14, 1, 76);

    const juce::Font sectionFont = orbitron(11.0f, 700).withExtraKerningFactor(0.30f);
    auto vertical = [&](const juce::String& text, int x, int centreY) {
        juce::Graphics::ScopedSaveState state(g);
        g.addTransform(juce::AffineTransform::rotation(-juce::MathConstants<float>::halfPi,
                                                       static_cast<float>(x),
                                                       static_cast<float>(centreY)));
        g.setColour(colours::inkMuted);
        g.setFont(sectionFont);
        g.drawText(text, juce::Rectangle<int>(x - 40, centreY - 7, 80, 14),
                   juce::Justification::centred, false);
    };
    vertical("IN", 26, 50);
    vertical("EQ", 272, 50);
    vertical("OUT", 848, 50);

    // Leyenda + valor bajo cada knob: "GAIN 0.0 DB", el numero en segmentos.
    const juce::Font labelFont = rajdhani(12.0f).withExtraKerningFactor(0.18f);
    const juce::Font unitFont = rajdhani(9.0f);
    for (const auto& readout : readouts_) {
        const auto area = readoutArea(readout);
        juce::String number, unit;
        {
            int split = readout.text.length();
            for (int c = 0; c < readout.text.length(); ++c) {
                const auto ch = readout.text[c];
                if (!(juce::CharacterFunctions::isDigit(ch) || ch == '.' || ch == '-' ||
                      ch == '+' || ch == ' ')) {
                    split = c;
                    break;
                }
            }
            number = readout.text.substring(0, split).trim().removeCharacters("+");
            unit = readout.text.substring(split).trim().toUpperCase();
        }
        const float digits = 11.0f;
        const float labelWidth = juce::GlyphArrangement::getStringWidth(labelFont, readout.label);
        const float numberWidth = SegmentDisplay::measure(number, digits);
        const float unitWidth = juce::GlyphArrangement::getStringWidth(unitFont, unit);
        const float total = labelWidth + 6.0f + numberWidth + 3.0f + unitWidth;
        float x = static_cast<float>(area.getCentreX()) - total * 0.5f;
        const auto row = area.toFloat();
        g.setColour(isEnabled() ? colours::inkMuted : colours::inkDim);
        g.setFont(labelFont);
        g.drawText(readout.label, row.withX(x).withWidth(labelWidth + 2.0f),
                   juce::Justification::centredLeft, false);
        x += labelWidth + 6.0f;
        if (SegmentDisplay::isSegmentable(number))
            SegmentDisplay::draw(g, row.withX(x).withWidth(numberWidth), number, digits,
                                 colours::lcdInk, true);
        x += numberWidth + 3.0f;
        g.setColour(colours::lcdInk.withAlpha(0.62f));
        g.setFont(unitFont);
        g.drawText(unit, row.withX(x).withWidth(unitWidth + 2.0f),
                   juce::Justification::centredLeft, false);
    }

    // MODE bajo cada par de teclas.
    g.setColour(colours::inkMuted);
    g.setFont(rajdhani(11.0f).withExtraKerningFactor(0.18f));
    for (const auto& mode : kModes)
        g.drawText("MODE", juce::Rectangle<int>(mode.getCentreX() - 32, kCaptionTop, 64, 16),
                   juce::Justification::centred, false);
}

void MasterStrip::resized() {
    inGain_->setBounds(kInGain);
    tone_->setBounds(kTone);
    width_->setBounds(kWidth);
    outGain_->setBounds(kOutGain);
    if (eqEnable_ != nullptr) eqEnable_->setBounds(kEqEnable);
    if (eqCut_ != nullptr) eqCut_->setBounds(kCut);
    for (int i = 0; i < modeButtons_.size(); ++i) modeButtons_[i]->setBounds(kModes[i]);
}

}  // namespace rtxx::ui
