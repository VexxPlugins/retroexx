#pragma once
// Knob de un parámetro del host: dial propio (no LookAndFeel) con valor numérico visible
// y los cuatro estados que exige DESIGN.md §6: hover, focus, active y disabled.
//
// El valor que se escribe es el del parámetro (0..1 normalizado en el host); el texto lo
// genera el propio parámetro (getText), así que editor y host no pueden discrepar.

#include <juce_audio_processors/juce_audio_processors.h>

namespace rtxx::ui {

// Dos acabados, los dos oscuros: el knob pequeno de los modulos y el grande del MASTER
// (acero un punto mas claro). La lectura del valor no depende del cuerpo: siempre va en
// una ventana de LCD verde-azulada con digitos de 7 segmentos.
enum class DialFinish { Dark, Steel };

class ParamKnob : public juce::Component {
public:
    int hostId() const { return hostId_; }
    ParamKnob(juce::RangedAudioParameter& parameter, int hostId,
              DialFinish finish = DialFinish::Dark, juce::Colour accent = juce::Colour());
    ~ParamKnob() override;

    // Oculta la leyenda del knob (la banda de knobs grandes ya escribe el nombre del modulo
    // debajo: repetir "AMOUNT" seria ruido).
    void setCaptionVisible(bool visible) { captionVisible_ = visible; repaint(); }

    // Oculta la ventana de LCD bajo el dial. Las columnas leen el valor en su pantalla
    // (ordenador de viaje) y la banda/tira lo escriben fuera: el dial llena el control.
    void setReadoutVisible(bool visible) { readoutVisible_ = visible; repaint(); }

    void paint(juce::Graphics& g) override;
    void mouseEnter(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    void focusGained(FocusChangeType) override;
    void focusLost(FocusChangeType) override;
    bool keyPressed(const juce::KeyPress& key) override;

private:
    void setValueFromUi(float denormalised, bool completeGesture);
    bool controlEnabled() const;

    int hostId_ = -1;
    juce::RangedAudioParameter& parameter_;
    juce::NormalisableRange<float> range_;
    float value_ = 0.0f;        // denormalizado: 0..1, o índice en los enums
    float gestureStartValue_ = 0.0f;
    bool hover_ = false;
    bool active_ = false;
    bool focused_ = false;
    juce::String caption_;
    DialFinish finish_ = DialFinish::Dark;
    juce::Colour accent_;  // color del modulo: aro, digitos y LED
    bool captionVisible_ = true;
    bool readoutVisible_ = true;
    std::unique_ptr<juce::ParameterAttachment> attachment_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParamKnob)
};

}  // namespace rtxx::ui
