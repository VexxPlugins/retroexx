#include "PluginProcessor.h"

#include <cstring>
#include <limits>

#include "HostParamDisplay.h"
#include "PluginEditor.h"
#include "product/ProductInfo.h"
#include "model/FactoryPresets.h"

namespace rtxx {
namespace {

// Los 7 Enable del motor. Son booleanos (no flotantes) porque en el motor son pasos discretos
// y en el grafo son puertas: Macro*Enable -> *_Enable (ParamStore, kEnables).
bool isEnableParam(int hostId) {
    switch (hostId) {
        case 7:   // NOIS Enable
        case 14:  // WOBB Enable
        case 21:  // DIST Enable
        case 27:  // DIGI Enable
        case 35:  // SPAC Enable
        case 42:  // MAGN Enable
        case 49:  // MAST EQ Enable
            return true;
        default:
            return false;
    }
}

// Enums del motor (la tabla del host: max 15 y max 5). El AU no publica etiquetas
// (value_strings_available = false): los indices son el texto honesto.
int enumSize(int hostId) { return hostParamChoiceCount(hostId); }

constexpr int kStateHeaderInts = 3;                // magic, version, count
constexpr int kStateChecksumInts = 1;

unsigned int fnv1a(const void* data, size_t size) {
    const auto* bytes = static_cast<const unsigned char*>(data);
    unsigned int hash = 2166136261u;
    for (size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= 16777619u;
    }
    return hash;
}

}  // namespace

RetroexxAudioProcessor::RetroexxAudioProcessor()
    : juce::AudioProcessor(BusesProperties()
                               .withInput("Input", juce::AudioChannelSet::stereo(), true)
                               .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      name_(kProductName),
      engine_(store_) {
    std::string error;
    if (!engine_.build(error)) engineError_ = juce::String(error);
    store_.resetToDefaults();

    for (int hostId = 0; hostId < HostParamTable::count(); ++hostId) {
        const HostParamSpec& spec = HostParamTable::all()[hostId];
        // El id del parametro es el id del host (0..57): el plugin publica ese numero como
        // AudioUnitParameterID (JUCE_FORCE_USE_LEGACY_PARAM_IDS en el target), asi que el
        // mapa del host y HostParamTable son el mismo indice, igual que en el motor.
        const juce::ParameterID parameterId{juce::String(hostId), 1};
        const juce::String parameterName(spec.name);
        juce::AudioProcessorParameter* parameter = nullptr;

        if (isEnableParam(hostId)) {
            parameter = new juce::AudioParameterBool(parameterId, parameterName,
                                                     spec.defaultValue >= 0.5f);
        } else if (const int choices = enumSize(hostId); choices > 0) {
        // Los nombres de los tipos son los medidos del motor (VINYL 2, TUBEPAIR...),
            // no "0", "1", "2": es lo que el selector tiene que mostrar.
            juce::StringArray labels;
            for (int index = 0; index < choices; ++index)
                labels.add(hostParamChoiceName(hostId, index));
            const int defaultIndex =
                juce::roundToInt(spec.defaultValue * static_cast<float>(choices - 1));
            parameter = new juce::AudioParameterChoice(parameterId, parameterName, labels,
                                                       defaultIndex);
        } else {
            auto attributes =
                juce::AudioParameterFloatAttributes()
                    .withStringFromValueFunction([hostId](float value, int) {
                        return hostParamValueText(hostId, value);
                    })
                    .withValueFromStringFunction(
                        [hostId](const juce::String& text) {
                            bool parsed = false;
                            const float value = hostParamValueFromText(hostId, text, parsed);
                            return parsed ? value : 0.0f;
                        });
            parameter = new juce::AudioParameterFloat(
                parameterId, parameterName, juce::NormalisableRange<float>(0.0f, 1.0f),
                spec.defaultValue, attributes);
        }
        addParameter(parameter);
        hostParams_.push_back(static_cast<juce::RangedAudioParameter*>(parameter));
    }

    lastPushed_.assign(hostParams_.size(), std::numeric_limits<float>::quiet_NaN());
    syncParamsToStore();
}

RetroexxAudioProcessor::~RetroexxAudioProcessor() = default;

float RetroexxAudioProcessor::moduleLevel(int moduleIndex) const {
    return engine_.moduleLevel(moduleIndex);
}

juce::RangedAudioParameter* RetroexxAudioProcessor::hostParameter(int hostId) const {
    if (hostId < 0 || hostId >= static_cast<int>(hostParams_.size())) return nullptr;
    return hostParams_[static_cast<size_t>(hostId)];
}

float RetroexxAudioProcessor::hostValue(int hostId) const {
    auto* parameter = hostParameter(hostId);
    return parameter != nullptr ? parameter->getValue() : 0.0f;
}

void RetroexxAudioProcessor::setHostValue(int hostId, float normalised) {
    auto* parameter = hostParameter(hostId);
    if (parameter == nullptr) return;
    parameter->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, normalised));
    // Se guarda el valor que el parámetro aceptó (un booleano o un enum lo cuantizan).
    const float accepted = parameter->getValue();
    store_.setHost(hostId, accepted);
    lastPushed_[static_cast<size_t>(hostId)] = accepted;
}

void RetroexxAudioProcessor::setCurrentProgram(int index) {
    // Los presets son NUESTROS (src/model/FactoryPresets.cpp). Se aplican por la superficie
    // del host con notificacion, para que el host lo vea como cambio de programa.
    if (index < 0 || index >= factoryPresetCount()) return;
    const int total = static_cast<int>(hostParams_.size());
    std::vector<float> values(static_cast<size_t>(total), 0.0f);
    factoryPresetValues(index, values.data(), total);
    for (int id = 0; id < total; ++id) setHostValue(id, values[static_cast<size_t>(id)]);
    currentProgram_ = index;
}

const juce::String RetroexxAudioProcessor::getProgramName(int index) {
    if (index < 0 || index >= factoryPresetCount()) return {};
    return juce::String::fromUTF8(factoryPreset(index).name);
}

int RetroexxAudioProcessor::getNumPrograms() {
    const int count = factoryPresetCount();
    return count > 0 ? count : 1;
}

void RetroexxAudioProcessor::syncParamsToStore() {
    // Solo se empuja lo que cambió: setHost recorre las leyes y los derivados, y en un
    // bloque normal no cambia ningún parámetro.
    for (int hostId = 0; hostId < static_cast<int>(hostParams_.size()); ++hostId) {
        const float value = hostParams_[static_cast<size_t>(hostId)]->getValue();
        float& last = lastPushed_[static_cast<size_t>(hostId)];
        if (! juce::approximatelyEqual(value, last)) {
            store_.setHost(hostId, value);
            last = value;
        }
    }
}

void RetroexxAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    const int maxBlock = juce::jmax(1, samplesPerBlock);
    // Si el grafo no cierra (solo puede pasar si se rompe GraphSpec), no hay motor que
    // preparar: processBlock devuelve silencio y engineError() lo declara.
    if (engineError_.isEmpty()) {
        engine_.prepare(sampleRate, maxBlock);
        engine_.setMeteringEnabled(true);  // el editor dibuja 8 vumetros
    }
    monoInputRight_.setSize(1, maxBlock, false, true, true);
    monoOutputRight_.setSize(1, maxBlock, false, true, true);
    setLatencySamples(Engine::latencySamples()); // 64, igual que el motor
    syncParamsToStore();
}

void RetroexxAudioProcessor::releaseResources() {}

bool RetroexxAudioProcessor::isBusesLayoutSupported(const BusesLayout& layout) const {
// Los mismos pares que declara el motor: [1,1] [1,2] [2,2].
    const int numIn = layout.getNumChannels(true, 0);
    const int numOut = layout.getNumChannels(false, 0);
    return (numIn == 1 && (numOut == 1 || numOut == 2)) || (numIn == 2 && numOut == 2);
}

void RetroexxAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                          juce::MidiBuffer& midi) {
    juce::ignoreUnused(midi);
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    if (numSamples <= 0) return;

    syncParamsToStore();
    if (!engineError_.isEmpty()) {
        buffer.clear();
        return;
    }

    const auto* inputBus = getBus(true, 0);
    const auto* outputBus = getBus(false, 0);
    const int numIn = inputBus != nullptr ? inputBus->getNumberOfChannels() : 0;
    const int numOut = outputBus != nullptr ? outputBus->getNumberOfChannels() : 0;
    if (numIn <= 0 || numOut <= 0) {
        buffer.clear();
        return;
    }

    // Un host puede entregar mas muestras que el maxBlockSize declarado: el buffer
    // desechable del canal derecho crece, y el de la entrada mono si hace falta.
    if (monoOutputRight_.getNumSamples() < numSamples)
        monoOutputRight_.setSize(1, numSamples, false, false, true);

    float* left = buffer.getWritePointer(0);
    float* rightOut = numOut > 1 ? buffer.getWritePointer(1)
                                 : monoOutputRight_.getWritePointer(0);

    // El motor es estéreo. Mono->estereo duplica la entrada; mono->mono procesa el canal
    // duplicado y descarta el derecho (PortData no acepta punteros nulos de salida).
    const float* rightIn = left;
    if (numIn > 1) {
        rightIn = buffer.getReadPointer(1);
    } else if (numOut > 1) {
        if (monoInputRight_.getNumSamples() < numSamples)
            monoInputRight_.setSize(1, numSamples, false, false, true);
        std::memcpy(monoInputRight_.getWritePointer(0), buffer.getReadPointer(0),
                    sizeof(float) * static_cast<size_t>(numSamples));
        rightIn = monoInputRight_.getReadPointer(0);
    }

    PortData engineIn;
    engineIn.numChannels = 2;
    engineIn.numSamples = numSamples;
    engineIn.ch[0] = left;
    engineIn.ch[1] = const_cast<float*>(rightIn);

    PortData engineOut;
    engineOut.numChannels = 2;
    engineOut.numSamples = numSamples;
    engineOut.ch[0] = left;
    engineOut.ch[1] = rightOut;
    engine_.process(engineIn, engineOut, numSamples);

    for (int channel = numOut; channel < buffer.getNumChannels(); ++channel)
        buffer.clear(channel, 0, numSamples);
}

juce::AudioProcessorEditor* RetroexxAudioProcessor::createEditor() {
    return new RetroexxEditor(*this);
}

void RetroexxAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    const int count = HostParamTable::count();
    juce::MemoryOutputStream out(destData, false);
    // El magic va como bytes literales: kStateMagic de ProductInfo.h ("RTXX"), no un
    // entero con el orden de bytes de la maquina de turno.
    out.write(kStateMagic, 4);
    out.writeInt(kStateVersion);
    out.writeInt(count);
    for (int hostId = 0; hostId < count; ++hostId) {
        const float value = hostValue(hostId);
        unsigned int bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        out.writeInt(static_cast<int>(bits));
    }
    // v2: programa seleccionado (para que al restaurar el estado la UI y el host sigan
    // mostrando el mismo preset). Un estado v1 se sigue cargando: versión distinta de 2
    // ⇒ se lee el formato viejo y el programa se queda en 0.
    if (kStateVersion >= 2) out.writeInt(currentProgram_);
    const unsigned int checksum = fnv1a(out.getData(), static_cast<size_t>(out.getPosition()));
    out.writeInt(static_cast<int>(checksum));
}

void RetroexxAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    const int count = HostParamTable::count();
    // v1: cabecera + params + checksum. v2: además el programa, antes del checksum.
    const bool v2 = kStateVersion >= 2;
    const int expectedBytes =
        (kStateHeaderInts + count + (v2 ? 1 : 0) + kStateChecksumInts) * (int)sizeof(int);
    if (data == nullptr || sizeInBytes != expectedBytes) return;

    juce::MemoryInputStream in(data, static_cast<size_t>(sizeInBytes), false);
    char magic[4] = {};
    in.read(magic, 4);
    const int version = in.readInt();
    const int storedCount = in.readInt();
    if (std::memcmp(magic, kStateMagic, 4) != 0 || version != kStateVersion ||
        storedCount != count)
        return;

    std::vector<unsigned int> bits(static_cast<size_t>(count));
    for (int hostId = 0; hostId < count; ++hostId)
        bits[static_cast<size_t>(hostId)] = static_cast<unsigned int>(in.readInt());
    int storedProgram = 0;
    if (v2) storedProgram = in.readInt();
    const unsigned int storedChecksum = static_cast<unsigned int>(in.readInt());
    const unsigned int actualChecksum =
        fnv1a(data, static_cast<size_t>(sizeInBytes) - sizeof(int));
    if (storedChecksum != actualChecksum) return;

    for (int hostId = 0; hostId < count; ++hostId) {
        float value = 0.0f;
        std::memcpy(&value, &bits[static_cast<size_t>(hostId)], sizeof(value));
        setHostValue(hostId, value);
    }
    currentProgram_ = (storedProgram >= 0 && storedProgram < getNumPrograms()) ? storedProgram : 0;
}

}  // namespace rtxx

// El AU y el VST3 instancian el processor por aquí (juce_audio_plugin_client).
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new rtxx::RetroexxAudioProcessor();
}
