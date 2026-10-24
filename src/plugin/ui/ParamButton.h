#pragma once
// Boton iluminado: POST, STEREO, CUT, MODE. En El motor son botones redondos con texto
// que se encienden, no knobs ni interruptores de palanca. Este es el mismo parametro
// booleano que en la columna, pero con la forma que le corresponde.

#include <juce_audio_processors/juce_audio_processors.h>

namespace rtxx::ui {

// Iconos de los modos del EQ, medidos de la tabla: cada modo es un par de teclas
// apiladas, la de arriba vale 0 y la de abajo 1 (CutLo/CutHi: Soft | Hard; Tone: Tilt | Mid).
enum class ModeIcon { CutLoSoft, CutLoHard, CutHiSoft, CutHiHard, ToneTilt, ToneMid };

class ParamButton : public juce::Component {
public:
    int hostId() const { return hostId_; }
    ParamButton(juce::RangedAudioParameter& parameter, juce::String text, juce::Colour accent,
                int hostId = -1);
    ~ParamButton() override;

// Par de teclas apiladas: arriba escribe 0, abajo escribe 1.
    void setStacked(ModeIcon top, ModeIcon bottom);

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseEnter(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void focusGained(FocusChangeType) override;
    void focusLost(FocusChangeType) override;
    bool keyPressed(const juce::KeyPress& key) override;

    void setButtonText(juce::String text);

private:
    bool isOn() const { return parameter_.getValue() > 0.5f; }
    void flip();

    int hostId_ = -1;
    int hostIdHi_ = -1;
    juce::RangedAudioParameter& parameter_;
    std::unique_ptr<juce::ParameterAttachment> attachment_;
    juce::String text_;
    juce::Colour accent_;
    bool hover_ = false;
    bool focused_ = false;
    bool stacked_ = false;
    ModeIcon icons_[2] = {ModeIcon::CutLoSoft, ModeIcon::CutLoHard};

    void paintStacked(juce::Graphics& g);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParamButton)
};

}  // namespace rtxx::ui
