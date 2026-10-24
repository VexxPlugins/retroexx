#pragma once
// FOCUS / CUT: un deslizador de DOS tiradores sobre un solo parametro doble, como el del
// motor (el rango de frecuencias del Dist, del Digital, del Space y el corte del EQ).
//
// Los dos tiradores no se cruzan nunca: cada uno se queda a un paso minimo del otro. Se
// puede dibujar vertical (columnas) u horizontal (tira del EQ).

#include <juce_audio_processors/juce_audio_processors.h>

namespace rtxx::ui {

class ParamRangeSlider : public juce::Component {
public:
    int hostId() const { return hostId_; }
    int hostIdHi() const { return hostIdHi_; }
    ParamRangeSlider(juce::RangedAudioParameter& low, juce::RangedAudioParameter& high,
                     juce::String label, juce::Colour accent, bool vertical = true,
                     int hostIdLo = -1, int hostIdHi = -1);
    ~ParamRangeSlider() override;

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    void mouseEnter(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void focusGained(FocusChangeType) override;
    void focusLost(FocusChangeType) override;
    bool keyPressed(const juce::KeyPress& key) override;

    static constexpr int preferredHeight = 96;

    // Sin lectura ni leyenda: el carril ocupa todo el control (FOCUS de las columnas).
    void setReadoutVisible(bool visible) { readoutVisible_ = visible; repaint(); }

private:
    float normalised(const juce::RangedAudioParameter& parameter) const;
    void setNormalised(int handle, float value, bool completeGesture);
    juce::Rectangle<float> trackArea() const;
    float positionFor(int handle) const;

    int hostId_ = -1;
    int hostIdHi_ = -1;
    juce::RangedAudioParameter& low_;
    juce::RangedAudioParameter& high_;
    juce::NormalisableRange<float> lowRange_;
    juce::NormalisableRange<float> highRange_;
    std::unique_ptr<juce::ParameterAttachment> lowAttachment_;
    std::unique_ptr<juce::ParameterAttachment> highAttachment_;
    juce::String label_;
    juce::Colour accent_;
    bool vertical_ = true;
    bool readoutVisible_ = true;
    int activeHandle_ = -1;
    bool hover_ = false;
    bool focused_ = false;


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParamRangeSlider)
};

}  // namespace rtxx::ui
