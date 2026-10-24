#include "ParamButton.h"

#include "Fonts.h"
#include "Skin.h"
#include "KeyboardFocus.h"
#include "Theme.h"

namespace rtxx::ui {

ParamButton::ParamButton(juce::RangedAudioParameter& parameter, juce::String text,
                         juce::Colour accent, int hostId)
    : parameter_(parameter), text_(std::move(text)), accent_(accent), hostId_(hostId) {
    setWantsKeyboardFocus(true);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    setName(parameter.getName(64));
    attachment_ = std::make_unique<juce::ParameterAttachment>(
        parameter_, [this](float) { repaint(); });
    attachment_->sendInitialUpdate();
}

ParamButton::~ParamButton() = default;

void ParamButton::setButtonText(juce::String text) {
    text_ = std::move(text);
    repaint();
}

void ParamButton::flip() {
    attachment_->setValueAsCompleteGesture(isOn() ? 0.0f : 1.0f);
}

void ParamButton::setStacked(ModeIcon top, ModeIcon bottom) {
    stacked_ = true;
    icons_[0] = top;
    icons_[1] = bottom;
    repaint();
}

namespace {
// Curva de respuesta de cada modo, dibujada en una caja de 26 x 14 (la de la tecla).
juce::Path modePath(ModeIcon icon, juce::Rectangle<float> box) {
    const float x0 = box.getX(), y0 = box.getY(), w = box.getWidth(), h = box.getHeight();
    auto at = [&](float u, float v) { return juce::Point<float>(x0 + u * w, y0 + v * h); };
    juce::Path p;
    switch (icon) {
        case ModeIcon::CutLoSoft:   // pendiente suave que sube a la meseta
            p.startNewSubPath(at(0.0f, 0.9f));
            p.cubicTo(at(0.3f, 0.9f), at(0.35f, 0.15f), at(0.6f, 0.15f));
            p.lineTo(at(1.0f, 0.15f));
            break;
        case ModeIcon::CutLoHard:   // pared casi vertical
            p.startNewSubPath(at(0.25f, 1.0f));
            p.cubicTo(at(0.35f, 1.0f), at(0.35f, 0.15f), at(0.5f, 0.15f));
            p.lineTo(at(1.0f, 0.15f));
            break;
        case ModeIcon::CutHiSoft:
            p.startNewSubPath(at(0.0f, 0.15f));
            p.lineTo(at(0.4f, 0.15f));
            p.cubicTo(at(0.65f, 0.15f), at(0.7f, 0.9f), at(1.0f, 0.9f));
            break;
        case ModeIcon::CutHiHard:
            p.startNewSubPath(at(0.0f, 0.15f));
            p.lineTo(at(0.5f, 0.15f));
            p.cubicTo(at(0.65f, 0.15f), at(0.65f, 1.0f), at(0.75f, 1.0f));
            break;
        case ModeIcon::ToneTilt:    // recta inclinada
            p.startNewSubPath(at(0.0f, 0.85f));
            p.lineTo(at(1.0f, 0.15f));
            break;
        case ModeIcon::ToneMid:     // campana en medios
            p.startNewSubPath(at(0.0f, 0.8f));
            p.cubicTo(at(0.35f, 0.8f), at(0.35f, 0.1f), at(0.5f, 0.1f));
            p.cubicTo(at(0.65f, 0.1f), at(0.65f, 0.8f), at(1.0f, 0.8f));
            break;
    }
    return p;
}
}  // namespace

void ParamButton::paintStacked(juce::Graphics& g) {
    // Par de teclas de modo de 44 x 27 con el icono de la pendiente: vfd si esta activa,
    // inkMuted si no (§5.4).
    const bool enabled = isEnabled();
    const juce::Colour colour = accent_.getAlpha() > 0 ? accent_ : colours::vfd;
    const float half = static_cast<float>(getHeight()) * 0.5f;
    for (int i = 0; i < 2; ++i) {
        const bool selected = (i == 1) == isOn();
        const juce::Rectangle<float> cell(0.0f, half * static_cast<float>(i),
                                          static_cast<float>(getWidth()), half);
        const auto face = skin::paintKey(g, cell.withTrimmedBottom(1.0f), 4.0f,
                                         enabled && hover_, 2.0f);
        const auto iconBox = face.withSizeKeepingCentre(juce::jmin(26.0f, face.getWidth() - 8.0f),
                                                        juce::jmin(14.0f, face.getHeight() - 6.0f));
        const juce::Path icon = modePath(icons_[i], iconBox);
        const juce::PathStrokeType stroke(1.8f, juce::PathStrokeType::curved,
                                          juce::PathStrokeType::rounded);
        if (selected && enabled) {
            g.setColour(colour.withAlpha(0.25f));
            g.strokePath(icon, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));
        }
        g.setColour(selected && enabled ? colour : colours::inkMuted);
        g.strokePath(icon, stroke);
    }
    if (enabled && focused_) {
        g.setColour(colours::vfd.withAlpha(0.8f));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 5.0f, 2.0f);
    }
}

void ParamButton::paint(juce::Graphics& g) {
    if (stacked_) {
        paintStacked(g);
        return;
    }
    const bool enabled = isEnabled();
    const bool on = enabled && isOn();
    const juce::Colour colour = accent_.getAlpha() > 0 ? accent_ : colours::vfd;
    const auto face = skin::paintKey(g, getLocalBounds().toFloat(),
                                     getHeight() >= 40 ? 7.0f : 4.0f, enabled && hover_,
                                     getHeight() >= 40 ? 3.0f : 2.0f);
    const juce::Font font = orbitron(8.0f, 700).withExtraKerningFactor(0.10f);
    g.setFont(font);

    if (face.getHeight() >= 30.0f) {
        // Tecla de modulo: ventanita de 26 x 7 y el texto corto debajo.
        const juce::Rectangle<float> light(face.getCentreX() - 13.0f, face.getCentreY() - 9.0f,
                                           26.0f, 7.0f);
        skin::paintKeyLight(g, light, colour, on);
        g.setColour(on ? colours::ink : colours::inkLabel);
        g.drawText(text_, face.withTop(light.getBottom() + 4.0f).withHeight(12.0f),
                   juce::Justification::centredTop, false);
    } else {
        // Tecla baja (CUT del DIGITAL): el texto es la luz.
        if (on) {
            g.setColour(colour.withAlpha(0.28f));
            g.drawText(text_, face.translated(0.0f, 0.5f), juce::Justification::centred, false);
        }
        g.setColour(on ? colour : colours::inkLabel);
        g.drawText(text_, face, juce::Justification::centred, false);
    }
    if (enabled && focused_) {
        g.setColour(colours::vfd.withAlpha(0.8f));
        g.drawRoundedRectangle(face.expanded(2.0f), 8.0f, 2.0f);
    }
}

void ParamButton::mouseDown(const juce::MouseEvent& e) {
    if (!isEnabled()) return;
    grabKeyboardFocus();
    if (stacked_) {
        // Cada tecla del par escribe su valor (no conmuta): como un radio.
        attachment_->setValueAsCompleteGesture(e.y >= getHeight() / 2 ? 1.0f : 0.0f);
        return;
    }
    flip();
}

void ParamButton::mouseEnter(const juce::MouseEvent&) {
    hover_ = true;
    repaint();
}

void ParamButton::mouseExit(const juce::MouseEvent&) {
    hover_ = false;
    repaint();
}

void ParamButton::focusGained(FocusChangeType type) {
    markFocus(*this, type, true);
    focused_ = true;
    repaint();
}

void ParamButton::focusLost(FocusChangeType type) {
    markFocus(*this, type, false);
    focused_ = false;
    repaint();
}

bool ParamButton::keyPressed(const juce::KeyPress& key) {
    if (!isEnabled()) return false;
    if (key == juce::KeyPress::returnKey || key == juce::KeyPress::spaceKey) {
        flip();
        return true;
    }
    return false;
}

}  // namespace rtxx::ui
