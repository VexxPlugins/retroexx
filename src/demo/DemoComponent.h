#pragma once
// Ventana de la demo: el editor del plugin tal cual, sin audio propio. El plugin que se edita
// es el MISMO AudioProcessor que se publica como AU/VST3: no hay una copia del motor.

#include <juce_audio_utils/juce_audio_utils.h>

#include <memory>

#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace rtxx::demo {

class DemoComponent : public juce::Component {
public:
    DemoComponent();
    ~DemoComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    RetroexxAudioProcessor processor_;
    std::unique_ptr<juce::AudioProcessorEditor> editor_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DemoComponent)
};

}  // namespace rtxx::demo
