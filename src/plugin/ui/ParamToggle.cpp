#include "ParamToggle.h"

#include "Skin.h"
#include "Theme.h"

namespace rtxx::ui {
ParamToggle::ParamToggle(juce::RangedAudioParameter& parameter, int hostId, juce::Colour accent)
    : hostId_(hostId),
      parameter_(parameter),
      value_(parameter.getValue()),
      accent_(accent.getAlpha() > 0 ? accent : colours::vfd) {
    setWantsKeyboardFocus(true);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    setName(parameter.getName(64));
    attachment_ = std::make_unique<juce::ParameterAttachment>(
        parameter_, [this](float denormalised) {
            value_ = denormalised;
            repaint();
        });
    attachment_->sendInitialUpdate();
}

ParamToggle::~ParamToggle() = default;

bool ParamToggle::controlEnabled() const { return isEnabled(); }

void ParamToggle::flip() {
    const bool on = value_ > 0.5f;
    value_ = on ? 0.0f : 1.0f;
    attachment_->setValueAsCompleteGesture(value_);
    repaint();
}

void ParamToggle::paint(juce::Graphics& g) {
    const bool enabled = controlEnabled();
    const bool on = enabled && isOn();
    const auto face = skin::paintKey(g, getLocalBounds().toFloat(), 5.0f, enabled && hover_, 2.0f);
    skin::paintKeyLight(g, face.withSizeKeepingCentre(12.0f, 12.0f), accent_, on);
    if (enabled && focused_) {
        g.setColour(colours::vfd.withAlpha(0.8f));
        g.drawRoundedRectangle(face.expanded(2.0f), 6.0f, 2.0f);
    }
}

void ParamToggle::mouseEnter(const juce::MouseEvent&) {
    hover_ = true;
    repaint();
}

void ParamToggle::mouseExit(const juce::MouseEvent&) {
    hover_ = false;
    repaint();
}

void ParamToggle::mouseUp(const juce::MouseEvent& e) {
    if (!controlEnabled()) return;
    if (getLocalBounds().contains(e.getPosition())) {
        grabKeyboardFocus();
        flip();
    }
}

void ParamToggle::focusGained(FocusChangeType) {
    focused_ = true;
    repaint();
}

void ParamToggle::focusLost(FocusChangeType) {
    focused_ = false;
    repaint();
}

bool ParamToggle::keyPressed(const juce::KeyPress& key) {
    if (!controlEnabled()) return false;
    if (key == juce::KeyPress::returnKey || key == juce::KeyPress::spaceKey) {
        flip();
        return true;
    }
    return false;
}

}  // namespace rtxx::ui
