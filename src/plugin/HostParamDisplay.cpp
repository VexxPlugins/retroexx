#include "HostParamDisplay.h"

#include <cmath>

namespace rtxx {
namespace {

// Prefijos de modulo que llevan los nombres publicados. Se quitan del knob porque el
// panel ya dice el modulo; la lista es literal (no se adivina por posicion).
const char* const kPrefixes[] = {"MAST EQ ", "MAST ", "NOIS ", "WOBB ", "DIST ", "DIGI ",
                                 "SPAC ", "MAGN "};

const HostParamSpec* specFor(int hostId) {
    if (hostId < 0 || hostId >= HostParamTable::count()) return nullptr;
    return HostParamTable::all() + hostId;
}

} // namespace

juce::String hostParamShortName(int hostId) {
    const HostParamSpec* spec = specFor(hostId);
    if (spec == nullptr) return {};
    juce::String name(spec->name);
    for (const char* prefix : kPrefixes) {
        const juce::String p(prefix);
        if (name.startsWith(p)) return name.substring(p.length());
    }
    return name;
}

ParamUnit hostParamUnit(int hostId) {
    switch (hostId) {
        case 48:  // MAST In Gain
        case 57:  // MAST Out Gain
            return ParamUnit::Db36;
        case 50:  // MAST EQ Cut Lo
        case 51:  // MAST EQ Cut Hi
            return ParamUnit::Hertz;
        case 56:  // MAST Out Width
            return ParamUnit::WidthPercent;
        case 8:   // NOIS Type (0..15)
        case 22:  // DIST Type (0..5)
            return ParamUnit::TypeIndex;
        default:
            return ParamUnit::Percent;
    }
}

int hostParamChoiceCount(int hostId) { return hostId == 8 ? 16 : (hostId == 22 ? 6 : 0); }

juce::String hostParamChoiceName(int hostId, int index) {
    static const char* kNoiseTypes[] = {"VINYL 1", "VINYL 2", "TAPE 7.5", "TAPE 15",
                                        "TUBE",    "DC",      "MUFF",     "RADIO",
                                        "VHS",     "CASS",    "8-BIT",    "WHITE",
                                        "PINK",    "STUDIO 1", "STUDIO 2", "APOLLO"};
    static const char* kDistTypes[] = {"TUBEPAIR", "IRON", "CRUNCH", "HEAVY", "ZAP", "AIR"};
    if (hostId == 8)
        return juce::isPositiveAndBelow(index, 16) ? juce::String(kNoiseTypes[index]) : juce::String();
    if (hostId == 22)
        return juce::isPositiveAndBelow(index, 6) ? juce::String(kDistTypes[index]) : juce::String();
    return juce::String(index);
}

juce::String hostParamValueText(int hostId, float normalised) {
    const float v = normalised < 0.0f ? 0.0f : (normalised > 1.0f ? 1.0f : normalised);
    switch (hostParamUnit(hostId)) {
        case ParamUnit::Db36: {
            const double db = static_cast<double>(v - 0.5f) * 36.0;
            if (std::abs(db) < 0.05) return "0.0 dB";
            return juce::String::formatted("%+.1f dB", db);
        }
        case ParamUnit::Hertz: {
            const double hz = 20.0 * std::pow(1000.0, static_cast<double>(v));
            if (hz < 999.5) return juce::String::formatted("%.0f Hz", hz);
            return juce::String::formatted("%.2f kHz", hz / 1000.0);
        }
        case ParamUnit::WidthPercent:
            return juce::String::formatted("%.0f %%", static_cast<double>(v) * 200.0);
        case ParamUnit::TypeIndex: {
            const HostParamSpec* spec = specFor(hostId);
            const int index = static_cast<int>(std::lround(v * (spec ? spec->max : 0.0f)));
            return juce::String(index);
        }
        case ParamUnit::Percent:
            break;
    }
    return juce::String::formatted("%.0f %%", static_cast<double>(v) * 100.0);
}

float hostParamValueFromText(int hostId, const juce::String& text, bool& parsed) {
    parsed = false;
    const juce::String trimmed = text.trim();
    const double number = trimmed.getDoubleValue();
    const bool hasNumber = trimmed.containsAnyOf("0123456789");
    if (!hasNumber) return 0.0f;

    double normalised = 0.0;
    switch (hostParamUnit(hostId)) {
        case ParamUnit::Db36:
            normalised = number / 36.0 + 0.5;
            break;
        case ParamUnit::Hertz:
            if (!(number > 0.0)) return 0.0f;
            normalised = std::log((trimmed.containsIgnoreCase("khz") ? number * 1000.0 : number) /
                                  20.0) /
                         std::log(1000.0);
            break;
        case ParamUnit::WidthPercent:
            normalised = number / 200.0;
            break;
        case ParamUnit::TypeIndex: {
            const HostParamSpec* spec = specFor(hostId);
            const double maxIndex = spec ? static_cast<double>(spec->max) : 1.0;
            normalised = maxIndex > 0.0 ? number / maxIndex : 0.0;
            break;
        }
        case ParamUnit::Percent:
            normalised = number / 100.0;
            break;
    }
    parsed = true;
    return static_cast<float>(normalised < 0.0 ? 0.0 : (normalised > 1.0 ? 1.0 : normalised));
}

} // namespace rtxx
