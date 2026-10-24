#pragma once
// Fader horizontal como barra segmentada de VFD (VEXX-DASH90 §5.2): MAGNITUDE en el header
// (cuna con lectura fija) y FLUX, DECAY y crossfades en las columnas (barra sola).
//
// No es un Slider de JUCE con LookAndFeel: la barra y la lectura se dibujan aqui con los
// tokens de Theme.h, y el arrastre/teclado se comportan como en un knob.

#include <juce_audio_processors/juce_audio_processors.h>

#include "SegBar.h"

namespace rtxx::ui {

class ParamFader : public juce::Component {
public:
    int hostId() const { return hostId_; }
    ParamFader(juce::RangedAudioParameter& parameter, juce::Colour accent = juce::Colour(),
               int hostId = -1);
    ~ParamFader() override;

    // Rotulo bajo la barra cuando hay lectura (MAGNITUDE). En las columnas las leyendas y los
    // extremos de un crossfade los escribe el panel.
    void setLabel(juce::String label);

    // Sin lectura ni leyendas: solo carril y tirador. Es el de las columnas, donde la
    // leyenda la escribe el panel y el valor sale en su pantalla (ordenador de viaje).
    void setReadoutVisible(bool visible);

    // Forma de la barra segmentada: Fill (FLUX, DECAY), XFade (crossfades) o Wedge
    // (MAGNITUDE).
    void setBarMode(SegBarMode mode);

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
    float normalised() const;
    juce::Rectangle<float> trackBounds() const;
    void setFromX(float x, bool completeGesture);

    int hostId_ = -1;
    int hostIdHi_ = -1;
    juce::RangedAudioParameter& parameter_;
    juce::NormalisableRange<float> range_;
    std::unique_ptr<juce::ParameterAttachment> attachment_;
    juce::Colour accent_;
    juce::String label_;
    bool readoutVisible_ = true;
    SegBarMode barMode_ = SegBarMode::Fill;
    bool hover_ = false;
    bool active_ = false;
    bool focused_ = false;


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParamFader)
};

}  // namespace rtxx::ui
