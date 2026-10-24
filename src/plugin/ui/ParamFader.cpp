#include "ParamFader.h"

#include <cmath>

#include "Fonts.h"
#include "SegBar.h"
#include "SegmentDisplay.h"
#include "Skin.h"
#include "KeyboardFocus.h"
#include "Theme.h"

namespace rtxx::ui {
namespace {

constexpr int kReadoutWidth = 58;
constexpr float kBarHeight = 16.0f;   // MAGNITUDE (canvas: 224 x 16)

}  // namespace

ParamFader::ParamFader(juce::RangedAudioParameter& parameter, juce::Colour accent,
                       int hostId)
    : parameter_(parameter),
      hostId_(hostId),
      range_(parameter.getNormalisableRange()),
      accent_(accent) {
    setWantsKeyboardFocus(true);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    setName(parameter.getName(64));
    attachment_ = std::make_unique<juce::ParameterAttachment>(
        parameter_, [this](float) { repaint(); });
    attachment_->sendInitialUpdate();
}

ParamFader::~ParamFader() = default;

void ParamFader::setReadoutVisible(bool visible) {
    readoutVisible_ = visible;
    repaint();
}

void ParamFader::setBarMode(SegBarMode mode) {
    barMode_ = mode;
    repaint();
}

juce::Rectangle<float> ParamFader::trackBounds() const {
    const float w = static_cast<float>(getWidth());
    const float h = static_cast<float>(getHeight());
    if (!readoutVisible_) {
        // Columna: barra sola, centrada. La cuna doble del crossfade es mas alta (22 px).
        const float barH = juce::jmin(h, barMode_ == SegBarMode::XFade ? 22.0f : 12.0f);
        return {0.0f, (h - barH) * 0.5f, w, barH};
    }
    // Con lectura (MAGNITUDE): barra arriba, ventana VFD a la derecha y rotulo debajo.
    return {0.0f, 6.0f, w - static_cast<float>(kReadoutWidth) - 8.0f, kBarHeight};
}

void ParamFader::setLabel(juce::String label) {
    label_ = std::move(label);
    repaint();
}

float ParamFader::normalised() const {
    return juce::jlimit(0.0f, 1.0f, range_.convertTo0to1(parameter_.getValue()));
}

void ParamFader::setFromX(float x, bool completeGesture) {
    const juce::Rectangle<float> track = trackBounds();
    const float t = juce::jlimit(0.0f, 1.0f, (x - track.getX()) / juce::jmax(1.0f, track.getWidth()));
    const float value = range_.convertFrom0to1(range_.snapToLegalValue(t));
    if (completeGesture)
        attachment_->setValueAsCompleteGesture(value);
    else
        attachment_->setValueAsPartOfGesture(value);
    repaint();
}

void ParamFader::paint(juce::Graphics& g) {
    const bool on = isEnabled();
    const juce::Colour colour = accent_.getAlpha() > 0 ? accent_ : colours::vfd;
    const float t = normalised();
    const juce::Rectangle<float> track = trackBounds();

    // Barra segmentada: ~6 px por segmento en las columnas (87 px -> 14), 8 px en MAGNITUDE
    // (224 -> 28) y 21 en los crossfades, como el canvas.
    const int count = barMode_ == SegBarMode::XFade
                          ? 21
                          : juce::jmax(4, juce::roundToInt(track.getWidth() /
                                                           (readoutVisible_ ? 8.0f : 6.2f)));
    segbar::draw(g, track, barMode_, count, t, 0.0f, 1.0f, colour, on);

    if (focused_) {
        g.setColour(colours::vfd.withAlpha(0.8f));
        g.drawRoundedRectangle(track.expanded(3.0f), 3.0f, 2.0f);
    }
    if (!readoutVisible_) return;

    // Lectura fija (control maestro): ventana VFD con el valor en 7 segmentos.
    const juce::Rectangle<int> window(getWidth() - kReadoutWidth, 2, kReadoutWidth, 24);
    skin::paintLcd(g, window.toFloat(), on);
    const juce::String text = parameter_.getText(parameter_.getValue(), 16).trim();
    const juce::Colour lit = on ? colour : colours::vfdDim;
    int split = text.length();
    for (int i = 0; i < text.length(); ++i) {
        const auto c = text[i];
        if (!(juce::CharacterFunctions::isDigit(c) || c == '-' || c == '+' || c == '.' ||
              c == ' ')) {
            split = i;
            break;
        }
    }
    const juce::String number = text.substring(0, split).trim();
    const juce::String unit = text.substring(split).trim().toUpperCase();
    const juce::Font unitFont = rajdhani(9.0f);
    const float unitWidth = juce::GlyphArrangement::getStringWidth(unitFont, unit);
    const float right = static_cast<float>(window.getRight()) - 6.0f - unitWidth;
    if (!number.isEmpty() && SegmentDisplay::isSegmentable(number)) {
        const float digits = 14.0f;
        const float numberWidth = SegmentDisplay::measure(number, digits);
        SegmentDisplay::draw(g,
                             juce::Rectangle<float>(right - 3.0f - numberWidth,
                                                    static_cast<float>(window.getY()), numberWidth,
                                                    static_cast<float>(window.getHeight())),
                             number, digits, lit, on);
        g.setColour(colours::vfdDim);
        g.setFont(unitFont);
        g.drawText(unit, juce::Rectangle<float>(right, static_cast<float>(window.getY()),
                                                unitWidth + 2.0f,
                                                static_cast<float>(window.getHeight()) - 3.0f),
                   juce::Justification::bottomLeft, false);
    } else {
        g.setColour(lit);
        g.setFont(rajdhani(9.5f));
        g.drawFittedText(text.toUpperCase(), window.reduced(3, 0), juce::Justification::centred,
                         1, 0.8f);
    }
    skin::paintGlass(g, window.toFloat());

    // Rotulo serigrafiado bajo la barra.
    g.setColour(colours::inkHeader);
    g.setFont(orbitron(13.0f, 700).withExtraKerningFactor(0.24f));
    g.drawText(label_.toUpperCase(),
               juce::Rectangle<float>(track.getX(), track.getBottom() + 6.0f, track.getWidth(), 18.0f),
               juce::Justification::centred, false);
}

void ParamFader::mouseEnter(const juce::MouseEvent&) {
    hover_ = true;
    repaint();
}

void ParamFader::mouseExit(const juce::MouseEvent&) {
    hover_ = false;
    repaint();
}

void ParamFader::mouseDown(const juce::MouseEvent& e) {
    if (!isEnabled()) return;
    active_ = true;
    grabKeyboardFocus();
    attachment_->beginGesture();
    setFromX(static_cast<float>(e.x), false);
}

void ParamFader::mouseDrag(const juce::MouseEvent& e) {
    if (!active_) return;
    setFromX(static_cast<float>(e.x), false);
}

void ParamFader::mouseUp(const juce::MouseEvent&) {
    if (!active_) return;
    attachment_->endGesture();
    active_ = false;
    repaint();
}

void ParamFader::mouseDoubleClick(const juce::MouseEvent&) {
    if (!isEnabled()) return;
    attachment_->setValueAsCompleteGesture(
        range_.convertFrom0to1(parameter_.getDefaultValue()));
    repaint();
}

void ParamFader::focusGained(FocusChangeType type) {
    markFocus(*this, type, true);
    focused_ = true;
    repaint();
}

void ParamFader::focusLost(FocusChangeType type) {
    markFocus(*this, type, false);
    focused_ = false;
    repaint();
}

bool ParamFader::keyPressed(const juce::KeyPress& key) {
    if (!isEnabled()) return false;
    const float step = range_.interval > 0.0f ? range_.interval : (range_.end - range_.start) * 0.01f;
    float value = parameter_.getValue();
    if (key == juce::KeyPress::leftKey) value -= step;
    else if (key == juce::KeyPress::rightKey) value += step;
    else if (key == juce::KeyPress::homeKey) value = range_.start;
    else if (key == juce::KeyPress::endKey) value = range_.end;
    else return false;
    attachment_->setValueAsCompleteGesture(
        range_.snapToLegalValue(juce::jlimit(range_.start, range_.end, value)));
    repaint();
    return true;
}

}  // namespace rtxx::ui
