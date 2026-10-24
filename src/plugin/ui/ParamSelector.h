#pragma once
// Selector de tipo del motor: flecha izquierda, valor en una ventana de LCD y flecha
// derecha. Es lo que El motor usa para VINYL 2 / TUBEPAIR; los nombres salen del propio
// parametro (no de una lista aparte que pueda desincronizarse).

#include <juce_audio_processors/juce_audio_processors.h>

namespace rtxx::ui {

class ParamSelector : public juce::Component {
public:
    int hostId() const { return hostId_; }
    ParamSelector(juce::RangedAudioParameter& parameter, juce::Colour accent,
                  int hostId = -1);
    ~ParamSelector() override;

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    void mouseEnter(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void focusGained(FocusChangeType) override;
    void focusLost(FocusChangeType) override;
    bool keyPressed(const juce::KeyPress& key) override;

    static constexpr int preferredHeight = 26;

private:
    int stepCount() const;
    void setIndex(int index, bool completeGesture);
    juce::Rectangle<int> arrowArea(bool left) const;

    int hostId_ = -1;
    int hostIdHi_ = -1;
    juce::RangedAudioParameter& parameter_;
    juce::NormalisableRange<float> range_;
    std::unique_ptr<juce::ParameterAttachment> attachment_;
    juce::Colour accent_;
    bool hover_ = false;
    bool focused_ = false;


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParamSelector)
};

}  // namespace rtxx::ui
