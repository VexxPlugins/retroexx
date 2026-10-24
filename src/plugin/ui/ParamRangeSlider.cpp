#include "ParamRangeSlider.h"

#include <cmath>

#include "Fonts.h"
#include "SegBar.h"
#include "SegmentDisplay.h"
#include "Skin.h"
#include "KeyboardFocus.h"
#include "Theme.h"

namespace rtxx::ui {
namespace {
// FOCUS: columna de segmentos de 22 px con 6 px de aire arriba y abajo (canvas).
constexpr float kColumnWidth = 22.0f;
// CUT: ventana VFD de 54 px de alto; la barra va a 21 px de su borde superior.
constexpr float kWindowTop = 6.0f;
constexpr float kWindowHeight = 54.0f;
constexpr float kBarTop = 21.0f;
constexpr float kBarHeight = 12.0f;

// "62 Hz" -> numero en segmentos + unidad.
void splitValue(const juce::String& text, juce::String& number, juce::String& unit) {
    int split = text.length();
    for (int i = 0; i < text.length(); ++i) {
        const auto c = text[i];
        if (!(juce::CharacterFunctions::isDigit(c) || c == '-' || c == '.' || c == ' ')) {
            split = i;
            break;
        }
    }
    number = text.substring(0, split).trim();
    unit = text.substring(split).trim().toUpperCase();
}
}  // namespace

ParamRangeSlider::ParamRangeSlider(juce::RangedAudioParameter& low,
                                   juce::RangedAudioParameter& high, juce::String label,
                                   juce::Colour accent, bool vertical, int hostIdLo,
                                   int hostIdHi)
    : low_(low),
      high_(high),
      lowRange_(low.getNormalisableRange()),
      highRange_(high.getNormalisableRange()),
      label_(std::move(label)),
      accent_(accent),
      vertical_(vertical) {
    hostId_ = hostIdLo;
    hostIdHi_ = hostIdHi;
    setWantsKeyboardFocus(true);
    setMouseCursor(vertical ? juce::MouseCursor::UpDownResizeCursor
                            : juce::MouseCursor::LeftRightResizeCursor);
    lowAttachment_ = std::make_unique<juce::ParameterAttachment>(
        low_, [this](float) { repaint(); });
    highAttachment_ = std::make_unique<juce::ParameterAttachment>(
        high_, [this](float) { repaint(); });
    lowAttachment_->sendInitialUpdate();
    highAttachment_->sendInitialUpdate();
}

ParamRangeSlider::~ParamRangeSlider() = default;

float ParamRangeSlider::normalised(const juce::RangedAudioParameter& parameter) const {
    const auto& range = &parameter == &low_ ? lowRange_ : highRange_;
    return juce::jlimit(0.0f, 1.0f, range.convertTo0to1(parameter.getValue()));
}

juce::Rectangle<float> ParamRangeSlider::trackArea() const {
    const juce::Rectangle<float> area = getLocalBounds().toFloat();
    if (vertical_)
        return area.withSizeKeepingCentre(kColumnWidth, area.getHeight() - 12.0f);
    return {8.0f, kWindowTop + kBarTop, area.getWidth() - 16.0f, kBarHeight};
}

float ParamRangeSlider::positionFor(int handle) const {
    const auto track = trackArea();
    const float t = handle == 0 ? normalised(low_) : normalised(high_);
    return vertical_ ? track.getBottom() - track.getHeight() * t
                     : track.getX() + track.getWidth() * t;
}

void ParamRangeSlider::setNormalised(int handle, float value, bool completeGesture) {
    const float clamped = juce::jlimit(0.0f, 1.0f, value);
    auto& parameter = handle == 0 ? low_ : high_;
    auto& range = handle == 0 ? lowRange_ : highRange_;
    const float denormalised = range.snapToLegalValue(range.convertFrom0to1(clamped));
    auto& attachment = handle == 0 ? *lowAttachment_ : *highAttachment_;
    if (completeGesture)
        attachment.setValueAsCompleteGesture(denormalised);
    else
        attachment.setValueAsPartOfGesture(denormalised);
    repaint();
}

void ParamRangeSlider::paint(juce::Graphics& g) {
    // VEXX-DASH90 §5.2: FOCUS = columna de segmentos encendidos entre lo y hi, con el
    // marcador alto a la izquierda y el bajo a la derecha; CUT = barra `range` en su ventana
    // VFD, con un marcador bajo cada extremo.
    const bool on = isEnabled();
    const juce::Colour colour = accent_.getAlpha() > 0 ? accent_ : colours::vfd;
    const auto track = trackArea();
    const float lo = normalised(low_);
    const float hi = normalised(high_);

    if (vertical_) {
        const auto well = track.expanded(10.0f, 5.0f);
        skin::paintLcd(g, well, on);
        const auto marks = segbar::drawColumn(g, track, lo, hi, colour, on);
        segbar::drawMarker(g, {track.getX() - 2.0f, marks.x}, 6.0f, juce::MathConstants<float>::pi);
        segbar::drawMarker(g, {track.getRight() + 2.0f, marks.y}, 6.0f, 0.0f);
        skin::paintGlass(g, well);
        if (focused_) {
            g.setColour(colours::vfd.withAlpha(0.8f));
            g.drawRoundedRectangle(well.expanded(2.0f), 6.0f, 2.0f);
        }
        return;
    }

    const juce::Rectangle<float> window(0.0f, kWindowTop, static_cast<float>(getWidth()),
                                        kWindowHeight);
    skin::paintLcd(g, window, on);
    const int count = juce::jmax(4, juce::roundToInt(track.getWidth() / 6.0f));
    segbar::draw(g, track, SegBarMode::Range, count, 0.0f, lo, hi, colour, on);
    segbar::drawMarker(g, {track.getX() + track.getWidth() * lo, track.getBottom() + 2.0f}, 7.0f,
                       juce::MathConstants<float>::halfPi);
    segbar::drawMarker(g, {track.getX() + track.getWidth() * hi, track.getBottom() + 2.0f}, 7.0f,
                       juce::MathConstants<float>::halfPi);

    if (readoutVisible_) {
        // Los dos extremos en segmentos de 9 px, abajo a la izquierda y abajo a la derecha.
        const juce::Font unitFont = rajdhani(8.0f);
        auto value = [&](const juce::RangedAudioParameter& p, bool right) {
            juce::String number, unit;
            splitValue(p.getText(p.getValue(), 12).trim(), number, unit);
            if (!SegmentDisplay::isSegmentable(number)) return;
            const float digits = 9.0f;
            const float numberWidth = SegmentDisplay::measure(number, digits);
            const float unitWidth = juce::GlyphArrangement::getStringWidth(unitFont, unit);
            const float total = numberWidth + 3.0f + unitWidth;
            const float x = right ? window.getRight() - 8.0f - total : window.getX() + 8.0f;
            const juce::Rectangle<float> row(x, window.getY() + 4.0f, total, 13.0f);
            SegmentDisplay::draw(g, row.withWidth(numberWidth), number, digits,
                                 on ? colours::vfd : colours::vfdDim, on);
            g.setColour(colours::vfdDim);
            g.setFont(unitFont);
            g.drawText(unit, row.withTrimmedLeft(numberWidth + 3.0f),
                       juce::Justification::centredLeft, false);
        };
        value(low_, false);
        value(high_, true);
    }
    skin::paintGlass(g, window);
    if (focused_) {
        g.setColour(colours::vfd.withAlpha(0.8f));
        g.drawRoundedRectangle(window.expanded(2.0f), 6.0f, 2.0f);
    }

    if (readoutVisible_ && label_.isNotEmpty()) {
        g.setColour(colours::inkMuted);
        g.setFont(rajdhani(11.0f).withExtraKerningFactor(0.18f));
        g.drawText(label_, getLocalBounds().removeFromBottom(16), juce::Justification::centred,
                   false);
    }
}

void ParamRangeSlider::mouseDown(const juce::MouseEvent& e) {
    if (!isEnabled()) return;
    grabKeyboardFocus();
    const auto track = trackArea();
    const float clickPos = vertical_ ? static_cast<float>(e.y) : static_cast<float>(e.x);
    const float lowPos = positionFor(0);
    const float highPos = positionFor(1);
    // El tirador mas cercano al clic es el que se agarra.
    activeHandle_ = std::abs(clickPos - lowPos) <= std::abs(clickPos - highPos) ? 0 : 1;
    auto& attachment = activeHandle_ == 0 ? *lowAttachment_ : *highAttachment_;
    attachment.beginGesture();
    const float t = vertical_ ? (track.getBottom() - clickPos) / juce::jmax(1.0f, track.getHeight())
                             : (clickPos - track.getX()) / juce::jmax(1.0f, track.getWidth());
    setNormalised(activeHandle_, t, false);
}

void ParamRangeSlider::mouseDrag(const juce::MouseEvent& e) {
    if (activeHandle_ < 0) return;
    const auto track = trackArea();
    const float p = vertical_ ? static_cast<float>(e.y) : static_cast<float>(e.x);
    const float t = vertical_ ? (track.getBottom() - p) / juce::jmax(1.0f, track.getHeight())
                              : (p - track.getX()) / juce::jmax(1.0f, track.getWidth());
    // El de abajo no puede pasar al de arriba: un paso de separacion minima.
    const float other = normalised(activeHandle_ == 0 ? high_ : low_);
    const float separation = 1.0f / 128.0f;
    const float limited = activeHandle_ == 0 ? juce::jmin(t, other - separation)
                                             : juce::jmax(t, other + separation);
    setNormalised(activeHandle_, limited, false);
}

void ParamRangeSlider::mouseDoubleClick(const juce::MouseEvent&) {
    // Doble clic: los dos tiradores vuelven a su valor por defecto (§6).
    if (!isEnabled()) return;
    setNormalised(0, low_.getDefaultValue(), true);
    setNormalised(1, high_.getDefaultValue(), true);
}

void ParamRangeSlider::mouseUp(const juce::MouseEvent&) {
    if (activeHandle_ < 0) return;
    auto& attachment = activeHandle_ == 0 ? *lowAttachment_ : *highAttachment_;
    attachment.endGesture();
    activeHandle_ = -1;
    repaint();
}

void ParamRangeSlider::mouseEnter(const juce::MouseEvent&) {
    hover_ = true;
    repaint();
}

void ParamRangeSlider::mouseExit(const juce::MouseEvent&) {
    hover_ = false;
    repaint();
}

void ParamRangeSlider::focusGained(FocusChangeType type) {
    markFocus(*this, type, true);
    focused_ = true;
    repaint();
}

void ParamRangeSlider::focusLost(FocusChangeType type) {
    markFocus(*this, type, false);
    focused_ = false;
    repaint();
}

bool ParamRangeSlider::keyPressed(const juce::KeyPress& key) {
    if (!isEnabled()) return false;
    int handle = 0;
    float delta = 0.0f;
    if (key == juce::KeyPress::leftKey || key == juce::KeyPress::downKey) {
        handle = 0;
        delta = -1.0f / 128.0f;
    } else if (key == juce::KeyPress::rightKey || key == juce::KeyPress::upKey) {
        handle = 0;
        delta = 1.0f / 128.0f;
    } else {
        return false;
    }
    setNormalised(handle, normalised(low_) + delta, true);
    return true;
}

}  // namespace rtxx::ui
