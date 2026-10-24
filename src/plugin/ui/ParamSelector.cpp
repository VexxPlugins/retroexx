#include "ParamSelector.h"

#include "Fonts.h"
#include "SegmentDisplay.h"
#include "Skin.h"
#include "KeyboardFocus.h"
#include "Theme.h"

namespace rtxx::ui {
namespace {
constexpr int kArrowWidth = 30;   // tecla de 26 x 28 con 4 px de margen (§5.7)
}

ParamSelector::ParamSelector(juce::RangedAudioParameter& parameter, juce::Colour accent,
                             int hostId)
    : parameter_(parameter),
      range_(parameter.getNormalisableRange()),
      accent_(accent),
      hostId_(hostId) {
    setWantsKeyboardFocus(true);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    setName(parameter.getName(64));
    attachment_ = std::make_unique<juce::ParameterAttachment>(
        parameter_, [this](float) { repaint(); });
    attachment_->sendInitialUpdate();
}

ParamSelector::~ParamSelector() = default;

int ParamSelector::stepCount() const {
    const float step = range_.interval > 0.0f ? range_.interval : 1.0f;
    return juce::jmax(1, juce::roundToInt((range_.end - range_.start) / step) + 1);
}

juce::Rectangle<int> ParamSelector::arrowArea(bool left) const {
    return left ? juce::Rectangle<int>(0, 0, kArrowWidth, getHeight())
                : juce::Rectangle<int>(getWidth() - kArrowWidth, 0, kArrowWidth, getHeight());
}

void ParamSelector::setIndex(int index, bool completeGesture) {
    const int count = stepCount();
    const int clamped = juce::jlimit(0, count - 1, index);
    const float step = range_.interval > 0.0f ? range_.interval : 1.0f;
    const float value = juce::jlimit(range_.start, range_.end, range_.start + clamped * step);
    if (completeGesture)
        attachment_->setValueAsCompleteGesture(value);
    else
        attachment_->setValueAsPartOfGesture(value);
    repaint();
}

void ParamSelector::paint(juce::Graphics& g) {
    // VEXX-DASH90 §5.7: ventana VFD con el tipo en Rajdhani de 15 px en el color del modulo
    // y con halo; flechas en teclas planas de 26 x 28 con el chevron del modulo.
    const bool on = isEnabled();
    const juce::Colour colour = accent_.getAlpha() > 0 ? accent_ : colours::vfd;
    const juce::Rectangle<float> bounds = getLocalBounds().toFloat();
    skin::paintLcd(g, bounds, on);

    for (int side = 0; side < 2; ++side) {
        const bool left = side == 0;
        const auto key = arrowArea(left).toFloat().withSizeKeepingCentre(
            26.0f, juce::jmin(28.0f, bounds.getHeight() - 4.0f));
        g.setColour(colours::keyFlat.brighter(hover_ && on ? 0.06f : 0.0f));
        g.fillRoundedRectangle(key, 4.0f);
        const float cx = key.getCentreX();
        const float cy = key.getCentreY();
        const float dir = left ? 1.0f : -1.0f;
        juce::Path chevron;
        chevron.startNewSubPath(cx + 2.5f * dir, cy - 5.0f);
        chevron.lineTo(cx - 2.5f * dir, cy);
        chevron.lineTo(cx + 2.5f * dir, cy + 5.0f);
        g.setColour(on ? colour : colour.withAlpha(0.35f));
        g.strokePath(chevron, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
    }

    const juce::Rectangle<int> window(kArrowWidth, 0, getWidth() - kArrowWidth * 2, getHeight());
    const juce::String text = parameter_.getText(parameter_.getValue(), 32).trim();
    const juce::Colour lit = on ? colour : colour.withAlpha(0.45f);
    if (SegmentDisplay::isSegmentable(text)) {
        const float digits = static_cast<float>(window.getHeight()) - 14.0f;
        const float width = SegmentDisplay::measure(text, digits);
        SegmentDisplay::draw(
            g, juce::Rectangle<float>(window.getCentreX() - width * 0.5f,
                                      static_cast<float>(window.getY()), width,
                                      static_cast<float>(window.getHeight())),
            text, digits, lit, on);
    } else {
        g.setFont(rajdhani(15.0f).withExtraKerningFactor(0.10f));
        if (on) {
            g.setColour(colour.withAlpha(0.30f));
            g.drawFittedText(text.toUpperCase(), window.translated(0, 1), juce::Justification::centred,
                             1, 0.85f);
        }
        g.setColour(lit);
        g.drawFittedText(text.toUpperCase(), window, juce::Justification::centred, 1, 0.85f);
    }
    skin::paintGlass(g, bounds);
    if (on && focused_) {
        g.setColour(colours::vfd.withAlpha(0.8f));
        g.drawRoundedRectangle(bounds.expanded(2.0f), 6.0f, 2.0f);
    }
}

void ParamSelector::mouseDoubleClick(const juce::MouseEvent& e) {
    // Doble clic en el nombre (no en las flechas, que pasan de tipo): tipo por defecto (§6).
    if (!isEnabled() || e.x < kArrowWidth || e.x > getWidth() - kArrowWidth) return;
    const float step = range_.interval > 0.0f ? range_.interval : 1.0f;
    setIndex(juce::roundToInt((range_.convertFrom0to1(parameter_.getDefaultValue()) - range_.start) / step), true);
}

void ParamSelector::mouseDown(const juce::MouseEvent& e) {
    if (!isEnabled()) return;
    grabKeyboardFocus();
    const float step = range_.interval > 0.0f ? range_.interval : 1.0f;
    const int current = juce::roundToInt((range_.convertFrom0to1(parameter_.getValue()) - range_.start) / step);
    if (e.x < kArrowWidth)
        setIndex(current - 1, true);
    else if (e.x > getWidth() - kArrowWidth)
        setIndex(current + 1, true);
}

void ParamSelector::mouseEnter(const juce::MouseEvent&) {
    hover_ = true;
    repaint();
}

void ParamSelector::mouseExit(const juce::MouseEvent&) {
    hover_ = false;
    repaint();
}

void ParamSelector::focusGained(FocusChangeType type) {
    markFocus(*this, type, true);
    focused_ = true;
    repaint();
}

void ParamSelector::focusLost(FocusChangeType type) {
    markFocus(*this, type, false);
    focused_ = false;
    repaint();
}

bool ParamSelector::keyPressed(const juce::KeyPress& key) {
    if (!isEnabled()) return false;
    const float step = range_.interval > 0.0f ? range_.interval : 1.0f;
    const int current = juce::roundToInt((range_.convertFrom0to1(parameter_.getValue()) - range_.start) / step);
    if (key == juce::KeyPress::leftKey) setIndex(current - 1, true);
    else if (key == juce::KeyPress::rightKey) setIndex(current + 1, true);
    else return false;
    return true;
}

}  // namespace rtxx::ui
