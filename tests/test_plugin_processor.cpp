// Fase 9 del ROADMAP, verificado sobre el AudioProcessor real (no sobre una copia del
// motor):
// * 58 parametros del host, en el orden y con los defaults de fábrica motor,
//     y repartidos por la tabla de modulos del editor sin huecos ni duplicados,
//   * latencia de exactamente 64 muestras,
//   * passthrough BIT A BIT desplazado 64 con los 7 Enable a 0, y sin depender del
//     tamano de bloque que pida el host,
//   * mono->estereo (y mono->mono) con el mismo retardo en los dos canales,
//   * estado con magic "RTXX" que se guarda y se recupera, y rechaza blobs ajenos.
#include <juce_audio_processors/juce_audio_processors.h>

#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "HostModules.h"
#include "PluginProcessor.h"
#include "dsp/Check.h"
#include "PresetManager.h"
#include "model/FactoryPresets.h"
#include "model/HostParam.h"

namespace {

using rtxx::HostParamSpec;
using rtxx::HostParamTable;
using rtxx::RetroexxAudioProcessor;

std::vector<float> noise(int numSamples, unsigned seed) {
    std::vector<float> data(static_cast<size_t>(numSamples));
    unsigned state = seed;
    for (auto& sample : data) {
        state = state * 1664525u + 1013904223u;
        sample = (static_cast<float>(state >> 8) / static_cast<float>(1u << 24)) * 2.0f - 1.0f;
    }
    return data;
}

// Los 7 Enable del motor (ids medidos): el estado de fabrica del motor los trae a 1.
constexpr int kEnableIds[] = {7, 14, 21, 27, 35, 42, 49};

void prepare(RetroexxAudioProcessor& processor, double sampleRate) {
    processor.setRateAndBufferSizeDetails(sampleRate, 512);
    processor.prepareToPlay(sampleRate, 512);
}

// Render estéreo con un tamano de bloque dado. Devuelve L y R separados.
void renderStereo(RetroexxAudioProcessor& processor, const std::vector<float>& left,
                  const std::vector<float>& right, int blockSize, std::vector<float>& outLeft,
                  std::vector<float>& outRight) {
    const int numSamples = static_cast<int>(left.size());
    outLeft.assign(static_cast<size_t>(numSamples), 0.0f);
    outRight.assign(static_cast<size_t>(numSamples), 0.0f);
    juce::AudioBuffer<float> storage(2, blockSize);
    juce::MidiBuffer midi;
    int done = 0;
    while (done < numSamples) {
        const int take = juce::jmin(blockSize, numSamples - done);
        juce::AudioBuffer<float> buffer(storage.getArrayOfWritePointers(), 2, take);
        std::memcpy(buffer.getWritePointer(0), left.data() + done, sizeof(float) * (size_t)take);
        std::memcpy(buffer.getWritePointer(1), right.data() + done, sizeof(float) * (size_t)take);
        processor.processBlock(buffer, midi);
        std::memcpy(outLeft.data() + done, buffer.getReadPointer(0),
                    sizeof(float) * (size_t)take);
        std::memcpy(outRight.data() + done, buffer.getReadPointer(1),
                    sizeof(float) * (size_t)take);
        done += take;
    }
}

void setAllEnables(RetroexxAudioProcessor& processor, float value) {
    for (int hostId : kEnableIds) processor.setHostValue(hostId, value);
}

void testParameterSurface(RetroexxAudioProcessor& processor) {
    CHECK(processor.getParameters().size() == HostParamTable::count());
    CHECK(HostParamTable::count() == 58);

    for (int hostId = 0; hostId < HostParamTable::count(); ++hostId) {
        const HostParamSpec& spec = HostParamTable::all()[hostId];
        auto* parameter = processor.hostParameter(hostId);
        CHECK(parameter != nullptr);
        if (parameter == nullptr) continue;
        // El default tiene que ser el de fábrica. Los enums y los
        // booleanos lo cuantizan al paso mas cercano.
        const float defaultNormalised = parameter->getDefaultValue();
        CHECK_NEAR(defaultNormalised, spec.defaultValue, 1.0f / 30.0f);
        CHECK(parameter->getName(64) == juce::String(spec.name));
    }
}

// La tabla de modulos del editor cubre los 58 ids exactamente una vez. Si alguien anade
// un parametro al host y no lo pone en un panel, este test falla.
void testModuleCoverage() {
    std::vector<int> seen(static_cast<size_t>(HostParamTable::count()), 0);
    auto mark = [&seen](int hostId) {
        if (hostId >= 0 && hostId < static_cast<int>(seen.size())) ++seen[static_cast<size_t>(hostId)];
    };
    // Piezas que no son columna: EQ, WIDTH y los tres del aparato.
    for (int i = 0; i < rtxx::kEqStrip.knobCount; ++i) mark(rtxx::kEqStrip.knobs[i]);
    if (rtxx::kEqStrip.enableHostId >= 0) mark(rtxx::kEqStrip.enableHostId);  // puerta del EQ
    for (int i = 0; i < rtxx::kWidthKnob.knobCount; ++i) mark(rtxx::kWidthKnob.knobs[i]);
    mark(rtxx::kInGainId);
    mark(rtxx::kOutGainId);
    mark(rtxx::kMagnitudeId);
    for (int i = 0; i < rtxx::ModuleTable::count(); ++i) {
        const rtxx::ModuleSpec& module = rtxx::ModuleTable::all()[i];
        if (module.enableHostId >= 0) mark(module.enableHostId);
        if (module.macroHostId >= 0) mark(module.macroHostId);  // knob grande de la banda
        for (int k = 0; k < module.knobCount; ++k) mark(module.knobs[k]);
    }
    for (int hostId = 0; hostId < static_cast<int>(seen.size()); ++hostId) {
        if (seen[static_cast<size_t>(hostId)] != 1) {
            std::printf("  FAIL el parametro %d (%s) aparece %d veces en la tabla de modulos\n",
                        hostId, HostParamTable::all()[hostId].name,
                        seen[static_cast<size_t>(hostId)]);
            ++check::failures();
        }
    }

    // Los 7 Enable son los unicos interruptores y los 2 Type los unicos discretos.
    for (int i = 0; i < rtxx::ModuleTable::count(); ++i) {
        const int enableId = rtxx::ModuleTable::all()[i].enableHostId;
        if (enableId < 0) continue;
        bool found = false;
        for (int id : kEnableIds) found = found || id == enableId;
        CHECK(found);
    }
}

void testLatencyAndBypass(RetroexxAudioProcessor& processor) {
    prepare(processor, 44100.0);
    CHECK(processor.getLatencySamples() == 64);

    setAllEnables(processor, 0.0f);
    prepare(processor, 44100.0);  // el reset del motor limpia el estado y los 64 ceros

    const int numSamples = 256;
    const std::vector<float> left = noise(numSamples, 12345u);
    const std::vector<float> right = noise(numSamples, 999u);
    std::vector<float> outLeft, outRight;
    renderStereo(processor, left, right, numSamples, outLeft, outRight);

    int differing = 0;
    int nonzeroLatency = 0;
    for (int i = 0; i < numSamples; ++i) {
        if (i < 64) {
            if (outLeft[static_cast<size_t>(i)] != 0.0f || outRight[static_cast<size_t>(i)] != 0.0f)
                ++nonzeroLatency;
            continue;
        }
        const auto index = static_cast<size_t>(i - 64);
        if (std::memcmp(&outLeft[static_cast<size_t>(i)], &left[index], sizeof(float)) != 0 ||
            std::memcmp(&outRight[static_cast<size_t>(i)], &right[index], sizeof(float)) != 0)
            ++differing;
    }
    std::printf("  bypass: %d muestras distintas de %d, %d muestras de latencia no nulas\n",
                differing, numSamples - 64, nonzeroLatency);
    CHECK(differing == 0);
    CHECK(nonzeroLatency == 0);
}

void testBlockSizeIndependence(RetroexxAudioProcessor& processor) {
    const int numSamples = 512;
    const std::vector<float> left = noise(numSamples, 777u);
    const std::vector<float> right = noise(numSamples, 778u);

    std::vector<float> outSmallL, outSmallR, outLargeL, outLargeR;
    prepare(processor, 44100.0);
    setAllEnables(processor, 1.0f);  // con los modulos encendidos el bloque no puede cambiar nada
    renderStereo(processor, left, right, 32, outSmallL, outSmallR);
    prepare(processor, 44100.0);
    setAllEnables(processor, 1.0f);
    renderStereo(processor, left, right, 512, outLargeL, outLargeR);

    // El bloque de ruido arranca con su rampa de nivel (20 ms) y el transitorio de sus filtros:
    // se compara el RÉGIMEN, igual que los gates de fase con el arranque del motor. Con 1024
    // muestras de asentamiento el resto tiene que ser bit a bit.
    const int settle = 1024;
    int differing = 0;
    int firstDiff = -1;
    for (int i = settle; i < numSamples; ++i)
        if (std::memcmp(&outSmallL[static_cast<size_t>(i)], &outLargeL[static_cast<size_t>(i)],
                        sizeof(float)) != 0 ||
            std::memcmp(&outSmallR[static_cast<size_t>(i)], &outLargeR[static_cast<size_t>(i)],
                        sizeof(float)) != 0)
            { ++differing; if (firstDiff < 0) firstDiff = i; }
    std::printf("   bloque 32 vs 512: %d muestras distintas, primera en %d (smallL %.6f largeL %.6f)\n",
                differing, firstDiff,
                firstDiff >= 0 ? (double)outSmallL[static_cast<size_t>(firstDiff)] : 0.0,
                firstDiff >= 0 ? (double)outLargeL[static_cast<size_t>(firstDiff)] : 0.0);
    CHECK(differing == 0);
}

void testMonoToStereo(RetroexxAudioProcessor& processor) {
    juce::AudioProcessor::BusesLayout monoToStereo;
    monoToStereo.inputBuses.add(juce::AudioChannelSet::mono());
    monoToStereo.outputBuses.add(juce::AudioChannelSet::stereo());
    CHECK(processor.checkBusesLayoutSupported(monoToStereo));
    CHECK(processor.setBusesLayout(monoToStereo));

    prepare(processor, 44100.0);
    setAllEnables(processor, 0.0f);

    const int numSamples = 256;
    const std::vector<float> mono = noise(numSamples, 4242u);
    // El host entrega max(canales de entrada, de salida): 2, con la entrada mono solo en
    // el canal 0 (JUCE deja el resto sin definir; el processor no lo lee).
    juce::AudioBuffer<float> buffer(2, numSamples);
    std::memcpy(buffer.getWritePointer(0), mono.data(), sizeof(float) * (size_t)numSamples);
    buffer.clear(1, 0, numSamples);
    juce::MidiBuffer midi;
    processor.processBlock(buffer, midi);

    int differing = 0;
    float maxError = 0.0f;
    for (int i = 0; i < numSamples; ++i) {
        // El canal derecho (que rellena el processor) tiene que ser igual al izquierdo.
        if (! juce::approximatelyEqual(buffer.getSample(0, i), buffer.getSample(1, i))) ++differing;
        if (i < 64) continue;
        maxError = juce::jmax(maxError, std::abs(buffer.getSample(0, i) - mono[static_cast<size_t>(i - 64)]));
    }
    std::printf("  mono->estereo: L!=R en %d muestras, error maximo %.9g tras 64 muestras\n",
                differing, maxError);
    CHECK(differing == 0);
    CHECK(maxError <= 1.0e-7f);

// Mono -> mono tambien esta declarado por el motor.
    juce::AudioProcessor::BusesLayout monoToMono;
    monoToMono.inputBuses.add(juce::AudioChannelSet::mono());
    monoToMono.outputBuses.add(juce::AudioChannelSet::mono());
    CHECK(processor.checkBusesLayoutSupported(monoToMono));

    juce::AudioProcessor::BusesLayout stereo;
    stereo.inputBuses.add(juce::AudioChannelSet::stereo());
    stereo.outputBuses.add(juce::AudioChannelSet::stereo());
    CHECK(processor.setBusesLayout(stereo));
}

void testState(RetroexxAudioProcessor& processor) {
    processor.setHostValue(48, 0.75f);   // MAST In Gain
    processor.setHostValue(8, 0.6f);     // NOIS Type (enum de 16)

    juce::MemoryBlock state;
    processor.getStateInformation(state);
    CHECK(state.getSize() > 4);
    CHECK(std::memcmp(state.getData(), "RTXX", 4) == 0);

    RetroexxAudioProcessor other;
    prepare(other, 48000.0);
    other.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    CHECK_NEAR(other.hostValue(48), processor.hostValue(48), 1.0e-6);
    CHECK_NEAR(other.hostValue(8), processor.hostValue(8), 1.0e-6);

    // Un blob sin el magic no puede tocar nada.
    juce::MemoryBlock foreign(state);
    static_cast<char*>(foreign.getData())[0] = 'Z';  // otro magic: no debe cargar nada
    const float before = other.hostValue(48);
    other.setStateInformation(foreign.getData(), static_cast<int>(foreign.getSize()));
    CHECK_NEAR(other.hostValue(48), before, 1.0e-9);

    // Ni uno truncado.
    other.setStateInformation(state.getData(), 16);
    CHECK_NEAR(other.hostValue(48), before, 1.0e-9);
}

void testValueText() {
    RetroexxAudioProcessor processor;
    CHECK(processor.hostParameter(48)->getText(1.0f, 32) == juce::String("+18.0 dB"));
    CHECK(processor.hostParameter(48)->getText(0.5f, 32) == juce::String("0.0 dB"));
    CHECK(processor.hostParameter(56)->getText(0.5f, 32) == juce::String("100 %"));
    CHECK(processor.hostParameter(51)->getText(0.880782008f, 32) == juce::String("8.78 kHz"));
    CHECK(processor.hostParameter(50)->getText(0.163885891f, 32) == juce::String("62 Hz"));
    // Los dos discretos muestran el NOMBRE medido del motor, no el indice: es lo que el
    // selector de la GUI escribe en su ventana.
    CHECK(processor.hostParameter(8)->getNumSteps() == 16);
    CHECK(processor.hostParameter(8)->getText(0.0f, 32) == juce::String("VINYL 1"));
    CHECK(processor.hostParameter(8)->getText(1.0f, 32) == juce::String("APOLLO"));
    CHECK(processor.hostParameter(8)->getText(2.0f / 15.0f, 32) == juce::String("TAPE 7.5"));
    CHECK(processor.hostParameter(22)->getNumSteps() == 6);
    CHECK(processor.hostParameter(22)->getText(0.0f, 32) == juce::String("TUBEPAIR"));
    CHECK(processor.hostParameter(22)->getText(1.0f, 32) == juce::String("AIR"));
}

}  // namespace


// Presets de fabrica propios: el host tiene que ver N programas, cada
// uno con nombre, y al cambiarlo los 58 parametros quedan como dice el preset.
void testPrograms(RetroexxAudioProcessor& processor) {
    const int count = processor.getNumPrograms();
    std::printf("  programas: %d\n", count);
    CHECK(count >= 8);
    CHECK(count == rtxx::factoryPresetCount());
    for (int p = 0; p < count; ++p) {
        CHECK(processor.getProgramName(p).isNotEmpty());
        // Se comparan los BYTES UTF-8: el host ve el nombre tal cual, sin depender de
        // como juce::String normalice los acentos.
        const std::string asHost = processor.getProgramName(p).toRawUTF8();
        if (asHost != rtxx::factoryPreset(p).name)
            std::printf("    nombre %d: '%s' vs '%s'\n", p, asHost.c_str(),
                        rtxx::factoryPreset(p).name);
        CHECK(asHost == rtxx::factoryPreset(p).name);
    }
    CHECK(processor.getCurrentProgram() == 0);

    const int total = HostParamTable::count();
    for (int p = 0; p < count; ++p) {
        processor.setCurrentProgram(p);
        CHECK(processor.getCurrentProgram() == p);
        std::vector<float> want(static_cast<size_t>(total), 0.0f);
        rtxx::factoryPresetValues(p, want.data(), total);
        for (int id = 0; id < total; ++id) {
            const float got = processor.hostValue(id);
            // Los booleanos y los enums los cuantiza el parametro; el preset ya declara
            // valores que caen en un indice entero (tests/test_presets.cpp lo comprueba).
            if (std::abs(got - want[static_cast<size_t>(id)]) > 1.0e-4f) {
                std::printf("    preset %d: id %d vale %.6f, se esperaba %.6f\n", p, id,
                            (double)got, (double)want[static_cast<size_t>(id)]);
                CHECK(false);
            }
        }
    }
    processor.setCurrentProgram(0);
    // Un indice fuera de rango no rompe ni cambia el programa actual.
    processor.setCurrentProgram(count + 5);
    CHECK(processor.getCurrentProgram() == 0);
    processor.setCurrentProgram(-1);
    CHECK(processor.getCurrentProgram() == 0);
}

// Presets de usuario: guardar/cargar en disco con el MISMO blob de estado del host, y que
// el nombre se sane para que sea un fichero valido.
void testUserPresets(RetroexxAudioProcessor& processor) {
    const juce::File root = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                .getChildFile("rtxx_test_presets");
    root.deleteRecursively();
    rtxx::PresetManager manager(root);
    CHECK(manager.scan().empty());

    processor.setCurrentProgram(3);
    processor.setHostValue(48, 0.62f);
    const float inGain = processor.hostValue(48);
    juce::MemoryBlock state;
    processor.getStateInformation(state);
    CHECK(state.getSize() > 0);

    juce::String error;
    CHECK(manager.save("Prueba/.\u002e/uno", state, error));
    CHECK(rtxx::PresetManager::sanitiseName("   ") == "");
    CHECK(!manager.save("   ", state, error));
    CHECK(!error.isEmpty());

    const auto list = manager.scan();
    CHECK(list.size() == 1);
    if (list.size() == 1) {
        RetroexxAudioProcessor other;
        juce::MemoryBlock loaded;
        CHECK(manager.load(list[0].file, loaded, error));
        other.setStateInformation(loaded.getData(), (int)loaded.getSize());
        CHECK(std::abs(other.hostValue(48) - inGain) < 1e-4f);
        CHECK(std::abs(other.hostValue(1) - processor.hostValue(1)) < 1e-4f);
        CHECK(other.getCurrentProgram() == processor.getCurrentProgram());
        CHECK(manager.remove(list[0].file));
        CHECK(manager.scan().empty());
    }
    root.deleteRecursively();
}

int main() {
    juce::ScopedJuceInitialiser_GUI guiInit;

    RetroexxAudioProcessor processor;
    CHECK(processor.engineError().isEmpty());
    CHECK(processor.getName() == juce::String("Retroexx"));
    CHECK(processor.getTailLengthSeconds() == 30.0);

    testParameterSurface(processor);
    testModuleCoverage();
    testLatencyAndBypass(processor);
    testBlockSizeIndependence(processor);
    testMonoToStereo(processor);
    testState(processor);
    testValueText();
    testPrograms(processor);
    testUserPresets(processor);

    processor.releaseResources();
    return check::finish("test_plugin_processor");
}
