#pragma once
// Envoltorio de plugin (fase 9 del ROADMAP): AudioProcessor de JUCE sobre el motor de
// src/engine.
//
// Lo que el host ve:
//  * 58 parametros, uno por fila de HostParamTable (id 0..57), con el default medido en
// el motor (la tabla del host). Los 7 Enable son booleanos y los 2 Type son
// discretos, (la tabla del host: NOIS Type max 15, DIST Type max 5).
//  * 1 bus de entrada y 1 de salida, admitiendo mono->mono, mono->estereo y estereo
// (los mismos pares que declara el motor: [1,1] [1,2] [2,2]).
// * Latencia de 64 muestras (kAudioUnitProperty_Latency del motor = 64/44100 s).
//  * Estado propio con magic "RTXX" (ProductInfo.h): no se cargan ni se emiten presets
// de.

#include <juce_audio_processors/juce_audio_processors.h>

#include <vector>

#include "engine/Engine.h"
#include "model/ParamStore.h"

namespace rtxx {

class RetroexxAudioProcessor : public juce::AudioProcessor {
public:
    RetroexxAudioProcessor();
    ~RetroexxAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layout) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return name_; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    // Tail de 30 s: kAudioUnitProperty_TailTime del motor (la tabla del host).
    double getTailLengthSeconds() const override { return 30.0; }

    // Presets de FABRICA propios (src/model/FactoryPresets.cpp): no se cargan ni se
    // emiten los del motor. El 0 es "Init" (los defaults de fábrica).
    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram_; }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Superficie del host. getParameters() devuelve los parametros en orden de id de
    // host, que es lo que usan el editor y los tests.
    juce::RangedAudioParameter* hostParameter(int hostId) const;

    // Vumetro por modulo (RMS de su salida), en el orden de `ModuleTable`. El editor lo
    // lee a 30 Hz; el hilo de audio escribe atomics, sin cerrojos.
    float moduleLevel(int moduleIndex) const;
    float hostValue(int hostId) const;
    void setHostValue(int hostId, float normalised);
    ParamStore& params() { return store_; }
    const Engine& engine() const { return engine_; }
    // Toma de audio del visualizador (indice del motor, como moduleLevel).
    VizTap* vizTap(int moduleIndex) { return engine_.vizTap(moduleIndex); }
    const juce::String& engineError() const { return engineError_; }

    // Formato del blob de estado (ver PluginProcessor.cpp).
    // v2 añade el programa seleccionado al final del blob (v1 se sigue leyendo).
    static constexpr int kStateVersion = 2;

private:
    void syncParamsToStore();

    juce::String name_;
    ParamStore store_;
    Engine engine_;
    juce::String engineError_;
    std::vector<juce::RangedAudioParameter*> hostParams_;
    std::vector<float> lastPushed_;
    int currentProgram_ = 0;
    juce::AudioBuffer<float> monoInputRight_;
    juce::AudioBuffer<float> monoOutputRight_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RetroexxAudioProcessor)
};

} // namespace rtxx
