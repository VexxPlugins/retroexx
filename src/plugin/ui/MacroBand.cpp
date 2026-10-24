#include "MacroBand.h"

#include <cmath>

#include "Fonts.h"
#include "SegmentDisplay.h"
#include "Skin.h"
#include "Theme.h"

namespace rtxx::ui {
namespace {
constexpr int kPlateTop = 4;
constexpr int kPlateHeight = 185;
constexpr int kKnobSize = 140;
constexpr int kKnobTop = 28;   // dentro de la placa
}  // namespace

juce::Rectangle<int> MacroBand::boundsInEditor() {
    return geometry::scaled(8.0f, 385.0f, 777.0f, 124.0f);
}

MacroBand::MacroBand(const HostParamLookup& lookup) {
    setOpaque(false);
    const int count = ModuleTable::count();
    for (int i = 0; i < count; ++i) {
        const ModuleSpec& spec = ModuleTable::all()[i];
        // Los knobs grandes son del aparato, no del modulo: aro en el VFD verde-azulado.
        auto* knob = new ParamKnob(lookup(spec.macroHostId), spec.macroHostId,
                                   DialFinish::Steel, colours::lcdInk);
        knob->setCaptionVisible(false);   // el nombre del modulo lo escribe la banda
        knob->setReadoutVisible(false);   // la lectura va en la ventana de la placa
        addAndMakeVisible(knobs_.add(knob));
        amounts_.push_back(&lookup(spec.macroHostId));
        enableParams_.push_back(spec.enableHostId >= 0 ? &lookup(spec.enableHostId) : nullptr);
        readouts_.add({});
        if (spec.enableHostId >= 0)
            addAndMakeVisible(enables_.add(
                new ParamToggle(lookup(spec.enableHostId), spec.enableHostId,
                                juce::Colour(spec.accent))));
    }
}

void MacroBand::setControlsEnabled(bool shouldBeEnabled) {
    for (auto* knob : knobs_) knob->setEnabled(shouldBeEnabled);
    for (auto* toggle : enables_) toggle->setEnabled(shouldBeEnabled);
}

juce::Rectangle<int> MacroBand::plateArea(int index) const {
    // Misma x y ancho que la columna de la pantalla: cada placa queda bajo su modulo.
    const auto column = ModulePanel::boundsInEditor(index);
    return {column.getX() - getX(), kPlateTop, column.getWidth(), kPlateHeight};
}

juce::Rectangle<int> MacroBand::readoutArea(int index) const {
    const auto plate = plateArea(index);
    return {plate.getRight() - 68, plate.getY() + 8, 58, 26};
}

void MacroBand::refreshReadouts() {
    for (int i = 0; i < static_cast<int>(amounts_.size()); ++i) {
        const auto* amount = amounts_[static_cast<size_t>(i)];
        const auto* enable = enableParams_[static_cast<size_t>(i)];
        const juce::String text = amount->getText(amount->getValue(), 16).trim();
        const juce::String state = (enable == nullptr || enable->getValue() > 0.5f) ? "on" : "off";
        const juce::String key = text + "|" + state;
        if (key == readouts_[i]) continue;
        // El encendido tambien cambia el grabado del nombre: se repinta la placa entera.
        const bool stateChanged = !readouts_[i].endsWith("|" + state);
        readouts_.set(i, key);
        repaint(stateChanged ? plateArea(i) : readoutArea(i));
    }
}

void MacroBand::paint(juce::Graphics& g) {
    const int count = knobs_.size();
    for (int i = 0; i < count; ++i) {
        const auto plate = plateArea(i).toFloat();
        const auto* enable = enableParams_[static_cast<size_t>(i)];
        const bool on = enable == nullptr || enable->getValue() > 0.5f;

        // Placa elevada sobre la banda.
        juce::ColourGradient body(colours::bandHi, 0.0f, plate.getY(), colours::band, 0.0f,
                                  plate.getBottom(), false);
        g.setGradientFill(body);
        g.fillRoundedRectangle(plate, 6.0f);
        g.setColour(colours::bandCellEdge);
        g.drawRoundedRectangle(plate.reduced(0.5f), 6.0f, 1.0f);

        // Lectura fija del Amount: numero en segmentos y la unidad en texto.
        const auto window = readoutArea(i);
        skin::paintLcd(g, window.toFloat(), on);
        const juce::String text = readouts_[i].upToLastOccurrenceOf("|", false, false);
        int split = text.length();
        for (int c = 0; c < text.length(); ++c)
            if (!(juce::CharacterFunctions::isDigit(text[c]) || text[c] == '.' || text[c] == '-' ||
                  text[c] == ' ')) {
                split = c;
                break;
            }
        const juce::String number = text.substring(0, split).trim();
        const juce::String unit = text.substring(split).trim();
        const juce::Colour lit = on ? colours::lcdInk : colours::lcdInkDim;
        const float digits = 13.0f;
        const float numberWidth = SegmentDisplay::measure(number, digits);
        const float right = static_cast<float>(window.getRight()) - 16.0f;
        if (SegmentDisplay::isSegmentable(number))
            SegmentDisplay::draw(g,
                                 juce::Rectangle<float>(right - numberWidth,
                                                        static_cast<float>(window.getY()),
                                                        numberWidth,
                                                        static_cast<float>(window.getHeight())),
                                 number, digits, lit, on);
        g.setColour(lit.withAlpha(0.85f));
        g.setFont(rajdhani(9.0f));
        g.drawText(unit, juce::Rectangle<float>(right + 2.0f, static_cast<float>(window.getY()),
                                                12.0f, static_cast<float>(window.getHeight())),
                   juce::Justification::centredLeft, false);

        skin::paintGlass(g, window.toFloat());

        // Nombre del modulo grabado bajo el knob.
        g.setColour(on ? colours::bandInk : colours::bandInkMuted);
        g.setFont(orbitron(14.0f, 600).withExtraKerningFactor(0.24f));
        g.drawText(ModuleTable::all()[i].name,
                   juce::Rectangle<float>(plate.getX(), plate.getY() + 160.0f, plate.getWidth(), 20.0f),
                   juce::Justification::centred, false);
    }
}

void MacroBand::resized() {
    for (int i = 0; i < knobs_.size(); ++i) {
        const auto plate = plateArea(i);
        if (i < enables_.size())
            enables_[i]->setBounds(plate.getX() + 10, plate.getY() + 8,
                                   ParamToggle::preferredWidth, ParamToggle::preferredHeight);
        knobs_[i]->setBounds(plate.getX() + (plate.getWidth() - kKnobSize) / 2,
                             plate.getY() + kKnobTop - 10, kKnobSize, kKnobSize);
    }
}

}  // namespace rtxx::ui
