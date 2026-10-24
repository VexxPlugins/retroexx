#pragma once
// Tecla de encendido de un parametro Enable del host (VEXX-DASH90 §5.4): tecla de 26 x 26
// con un LED cuadrado de 12 x 12 en el color de su modulo. Nunca texto ON/OFF.
// No inventa estados: pinta el valor real del parámetro, que es el mismo que cierra la
// arista del grafo (ParamStore::setHost sincroniza Macro*Enable con *_Enable).

#include <juce_audio_processors/juce_audio_processors.h>

namespace rtxx::ui {

class ParamToggle : public juce::Component {
public:
    int hostId() const { return hostId_; }
    explicit ParamToggle(juce::RangedAudioParameter& parameter, int hostId = -1,
                         juce::Colour accent = juce::Colour());
    ~ParamToggle() override;

    void paint(juce::Graphics& g) override;
    void mouseEnter(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void focusGained(FocusChangeType) override;
    void focusLost(FocusChangeType) override;
    bool keyPressed(const juce::KeyPress& key) override;

    static constexpr int preferredWidth = 26;
    static constexpr int preferredHeight = 28;   // 26 de tecla + 2 de sombra de pulsacion

    // Estado real del parametro (lo usa el panel para atenuarse cuando el modulo esta
    // apagado, como hace El motor con las columnas desactivadas).
    bool isOn() const { return value_ > 0.5f; }

private:
    bool controlEnabled() const;
    void flip();

    int hostId_ = -1;
    juce::RangedAudioParameter& parameter_;
    float value_ = 0.0f;
    juce::Colour accent_;
    bool hover_ = false;
    bool focused_ = false;
    std::unique_ptr<juce::ParameterAttachment> attachment_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParamToggle)
};

}  // namespace rtxx::ui
