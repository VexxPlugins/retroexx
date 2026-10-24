#include "HardwareLookAndFeel.h"

#include "Fonts.h"
#include "Skin.h"
#include "Theme.h"

namespace rtxx::ui {

HardwareLookAndFeel::HardwareLookAndFeel() {
    setColour(juce::ResizableWindow::backgroundColourId, colours::window);
    setColour(juce::ComboBox::backgroundColourId, colours::windowDeep);
    setColour(juce::ComboBox::textColourId, colours::ink);
    setColour(juce::ComboBox::outlineColourId, colours::hairline);
    setColour(juce::ComboBox::arrowColourId, colours::inkMuted);
    setColour(juce::PopupMenu::backgroundColourId, colours::windowDeep);
    setColour(juce::PopupMenu::textColourId, colours::ink);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, colours::vfd.withAlpha(0.18f));
    setColour(juce::PopupMenu::highlightedTextColourId, colours::vfd);
    setColour(juce::TextButton::buttonColourId, colours::windowDeep);
    setColour(juce::TextButton::buttonOnColourId, colours::accent);
    setColour(juce::TextButton::textColourOffId, colours::ink);
    setColour(juce::TextButton::textColourOnId, colours::bandInk);
    setColour(juce::Label::textColourId, colours::ink);
    setColour(juce::AlertWindow::backgroundColourId, colours::window);
    setColour(juce::AlertWindow::textColourId, colours::ink);
    setColour(juce::AlertWindow::outlineColourId, colours::hairline);
    setColour(juce::TextEditor::backgroundColourId, colours::windowDeep);
    setColour(juce::TextEditor::textColourId, colours::ink);
    setColour(juce::TextEditor::outlineColourId, colours::hairline);
    setColour(juce::TextEditor::focusedOutlineColourId, colours::vfd);
    setColour(juce::TextEditor::highlightColourId, colours::vfd.withAlpha(0.30f));
}

void HardwareLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool,
                                        int, int, int, int, juce::ComboBox& box) {
    const juce::Rectangle<float> area(0.0f, 0.0f, static_cast<float>(width),
                                      static_cast<float>(height));
    // El selector de presets va dentro de la ventana VFD del header: sin carcasa propia, solo
    // el filo de foco.
    if (box.hasKeyboardFocus(true)) {
        g.setColour(colours::vfd.withAlpha(0.8f));
        g.drawRoundedRectangle(area.reduced(1.0f), 4.0f, 2.0f);
    }

    // Flecha: chevron de 2 trazos, sin glifo de fuente.
    const float cx = static_cast<float>(width) - 16.0f;
    const float cy = static_cast<float>(height) * 0.5f;
    juce::Path chevron;
    chevron.startNewSubPath(cx - 4.0f, cy - 2.0f);
    chevron.lineTo(cx, cy + 2.5f);
    chevron.lineTo(cx + 4.0f, cy - 2.0f);
    g.setColour(box.isEnabled() ? colours::vfdDim : colours::inkMuted);
    g.strokePath(chevron, juce::PathStrokeType(1.4f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
}

void HardwareLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label) {
    label.setBounds(10, 1, box.getWidth() - 30, box.getHeight() - 2);
    label.setFont(getComboBoxFont(box));
    label.setColour(juce::Label::textColourId, colours::vfd);
    label.setJustificationType(juce::Justification::centredLeft);
}

juce::Font HardwareLookAndFeel::getComboBoxFont(juce::ComboBox&) {
    return rajdhani(17.0f).withExtraKerningFactor(0.06f);
}

void HardwareLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                                const juce::Colour&, bool highlighted,
                                                bool down) {
    // Tecla de VEXX-DASH90 §5.4 (LOAD, SAVE): al pulsar baja y pierde la sombra.
    auto area = button.getLocalBounds().toFloat();
    if (down) area = area.withTrimmedTop(2.0f);
    skin::paintKey(g, area, 5.0f, highlighted && button.isEnabled(), down ? 0.0f : 2.0f);
    if (button.hasKeyboardFocus(true)) {
        g.setColour(colours::vfd.withAlpha(0.8f));
        g.drawRoundedRectangle(area.expanded(1.0f), 6.0f, 2.0f);
    }
}

void HardwareLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button, bool,
                                         bool down) {
    g.setFont(getTextButtonFont(button, button.getHeight()));
    g.setColour(button.isEnabled() ? colours::inkKey : colours::inkMuted);
    g.drawFittedText(button.getButtonText().toUpperCase(),
                     button.getLocalBounds().withTrimmedBottom(down ? 0 : 2).withTrimmedTop(down ? 2 : 0),
                     juce::Justification::centred, 1, 0.85f);
}

juce::Font HardwareLookAndFeel::getTextButtonFont(juce::TextButton&, int) {
    return orbitron(10.0f, 600).withExtraKerningFactor(0.12f);
}

void HardwareLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height) {
    g.fillAll(colours::windowDeep);
    g.setColour(colours::hairline);
    g.drawRect(0, 0, width, height, 1);
}

void HardwareLookAndFeel::drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                                             bool isSeparator, bool isActive, bool isHighlighted,
                                             bool isTicked, bool, const juce::String& text,
                                             const juce::String&, const juce::Drawable*,
                                             const juce::Colour*) {
    if (isSeparator) {
        g.setColour(colours::hairline);
        g.fillRect(area.reduced(8, 0).withHeight(1));
        return;
    }
    if (isHighlighted && isActive) {
        g.setColour(colours::vfd.withAlpha(0.18f));
        g.fillRect(area);
    }
    g.setColour(!isActive ? colours::inkMuted : (isHighlighted ? colours::vfd : colours::ink));
    g.setFont(getPopupMenuFont());
    g.drawFittedText(text, area.reduced(10, 0), juce::Justification::centredLeft, 1, 0.85f);
    if (isTicked) {
        g.setColour(colours::vfd);
        g.fillEllipse(static_cast<float>(area.getRight() - 14), static_cast<float>(area.getCentreY()) - 2.0f,
                      4.0f, 4.0f);
    }
}

juce::Font HardwareLookAndFeel::getPopupMenuFont() { return rajdhani(13.0f); }

void HardwareLookAndFeel::fillTextEditorBackground(juce::Graphics& g, int width, int height,
                                                   juce::TextEditor&) {
    g.setColour(colours::windowDeep);
    g.fillRect(0, 0, width, height);
}

void HardwareLookAndFeel::drawTextEditorOutline(juce::Graphics& g, int width, int height,
                                                juce::TextEditor& editor) {
    g.setColour(editor.hasKeyboardFocus(true) ? colours::vfd : colours::hairline);
    g.drawRect(0, 0, width, height, 1);
}

}  // namespace rtxx::ui
