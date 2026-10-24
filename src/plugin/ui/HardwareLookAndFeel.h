#pragma once
// LookAndFeel de la estetica hardware: combo de presets, boton de guardar y menus.
//
// Sin esto, JUCE pinta el combo y los botones con el LookAndFeel_V4 claro y el editor queda
// a medias: chapa negra con controles de Mac de 2015 en medio. Todo sale de ui::colours.

#include <juce_gui_basics/juce_gui_basics.h>

namespace rtxx::ui {

class HardwareLookAndFeel : public juce::LookAndFeel_V4 {
public:
    HardwareLookAndFeel();

    void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox& box) override;
    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;

    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour, bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override;
    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                        bool shouldDrawButtonAsHighlighted,
                        bool shouldDrawButtonAsDown) override;
    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override;

    void drawPopupMenuBackground(juce::Graphics& g, int width, int height) override;
    void drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                           bool isSeparator, bool isActive, bool isHighlighted,
                           bool isTicked, bool hasSubMenu, const juce::String& text,
                           const juce::String& shortcutKeyText,
                           const juce::Drawable* icon,
                           const juce::Colour* textColour) override;
    juce::Font getPopupMenuFont() override;

    void fillTextEditorBackground(juce::Graphics& g, int width, int height,
                                  juce::TextEditor& editor) override;
    void drawTextEditorOutline(juce::Graphics& g, int width, int height,
                               juce::TextEditor& editor) override;
};

}  // namespace rtxx::ui
