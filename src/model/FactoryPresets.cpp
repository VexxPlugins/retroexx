#include "model/FactoryPresets.h"

#include <cmath>
#include <cstdio>
#include <string>

#include "model/HostParam.h"

namespace rtxx {
namespace {

// Ids de host (medidos, `src/model/HostParams.gen.cpp`), con nombre para que los presets
// se lean sin contar columnas.
enum HostId : int {
    kMagnitude = 0,
    kNoisAmount = 1,
    kWobbAmount = 2,
    kDistAmount = 3,
    kDigiAmount = 4,
    kSpacAmount = 5,
    kMagnAmount = 6,
    kNoisEnable = 7,
    kNoisType = 8,
    kNoisTone = 9,
    kNoisFollow = 10,
    kNoisRouting = 11,
    kNoisDuck = 12,
    kNoisFlux = 13,
    kWobbEnable = 14,
    kWobbXfade = 15,
    kWobbWowRate = 16,
    kWobbStereo = 17,
    kWobbFlutterRate = 18,
    kWobbMix = 19,
    kWobbFlux = 20,
    kDistEnable = 21,
    kDistType = 22,
    kDistFilterLo = 23,
    kDistFilterHi = 24,
    kDistMix = 25,
    kDistFlux = 26,
    kDigiEnable = 27,
    kDigiRbBalance = 28,
    kDigiFilterLo = 29,
    kDigiFilterHi = 30,
    kDigiFilterMode = 31,
    kDigiSmooth = 32,
    kDigiMix = 33,
    kDigiFlux = 34,
    kSpacEnable = 35,
    kSpacDecay = 36,
    kSpacFilterLo = 37,
    kSpacFilterHi = 38,
    kSpacPreDelay = 39,
    kSpacStereo = 40,
    kSpacFlux = 41,
    kMagnEnable = 42,
    kMagnXfade = 43,
    kMagnStereo = 44,
    kMagnFlutter = 45,
    kMagnDropouts = 46,
    kMagnFlux = 47,
    kMastInGain = 48,
    kMastEqEnable = 49,
    kMastCutLo = 50,
    kMastCutHi = 51,
    kMastCutLoMode = 52,
    kMastCutHiMode = 53,
    kMastTone = 54,
    kMastToneMode = 55,
    kMastWidth = 56,
    kMastOutGain = 57,
};

// Índices de opción normalizados. El AU declara el rango del enum (p. ej. 0..15 para el
// tipo de ruido) y `ParamStore::setHost` normaliza a 0..1 y redondea, así que aquí se
// escribe la fracción.
constexpr float noiseType(int index) { return static_cast<float>(index) / 15.0f; }
constexpr float distType(int index) { return static_cast<float>(index) / 5.0f; }

// Tipos de ruido, en el orden medido del host (`NoiseTypeSpec`):
//   0 VINYL 1, 1 VINYL 2, 2 TAPE 7.5, 3 TAPE 15, 4 TUBE, 5 DC, 6 MUFF, 7 RADIO,
//   8 VHS, 9 CASS, 10 8-BIT, 11 WHITE, 12 PINK, 13 STUDIO 1, 14 STUDIO 2, 15 APOLLO.
// Tipos de distorsión, en el orden del host (`DistTypeSpec`):
//   0 TUBEPAIR, 1 IRON, 2 CRUNCH, 3 HEAVY, 4 ZAP, 5 AIR.

const FactoryPreset kPresets[] = {
{"Init", "Los defaults de fábrica, sin tocar nada.",
     {}},

    {"Vinilo 45", "Disco gastado: ruido de surco, wow corto y un realce de agudos suave.",
     {{kNoisAmount, 0.35f},
      {kNoisType, noiseType(0)},
      {kNoisTone, 0.42f},
      {kNoisFollow, 0.06f},
      {kNoisDuck, 0.34f},
      {kWobbAmount, 0.14f},
      {kWobbXfade, 0.15f},
      {kWobbWowRate, 0.20f},
      {kWobbFlutterRate, 0.30f},
      {kDistAmount, 0.0f},
      {kMagnAmount, 0.08f},
      {kMastTone, 0.58f},
      {kMastCutLo, 0.16f},
      {kMastCutHi, 0.86f}}},

    {"Cinta 15 ips", "Cinta abierta: siseo fino, wow y flutter, saturación de tubo pareada.",
     {{kNoisAmount, 0.30f},
      {kNoisType, noiseType(3)},
      {kNoisTone, 0.50f},
      {kNoisFollow, 0.10f},
      {kNoisDuck, 0.30f},
      {kWobbAmount, 0.32f},
      {kWobbXfade, 0.45f},
      {kWobbWowRate, 0.17f},
      {kWobbFlutterRate, 0.35f},
      {kDistAmount, 0.18f},
      {kDistType, distType(0)},
      {kMagnAmount, 0.22f},
      {kMagnXfade, 0.50f},
      {kMagnFlutter, 0.45f},
      {kMagnDropouts, 0.12f},
      {kMastTone, 0.62f},
      {kMastCutHi, 0.80f}}},

    {"Radio AM", "Banda estrecha y comprimida: ruido de radio y saturación dura.",
     {{kNoisAmount, 0.22f},
      {kNoisType, noiseType(7)},
      {kNoisTone, 0.55f},
      {kNoisDuck, 0.45f},
      {kWobbAmount, 0.05f},
      {kDistAmount, 0.55f},
      {kDistType, distType(3)},
      {kMastCutLo, 0.22f},
      {kMastCutHi, 0.55f},
      {kMastTone, 0.60f}}},

    {"8-Bit consola", "Consola de 8 bits: cuantización de muestra y retención corta.",
     {{kNoisAmount, 0.16f},
      {kNoisType, noiseType(10)},
      {kNoisTone, 0.50f},
      {kWobbAmount, 0.08f},
      {kDigiEnable, 1.0f},
      {kDigiAmount, 0.50f},
      {kDigiRbBalance, 0.32f},
      {kDigiSmooth, 0.22f},
      {kDigiMix, 0.85f},
      {kDistAmount, 0.10f},
      {kMastCutHi, 0.72f}}},

    {"Cassette cálido", "Cinta de casete: ruido de arrastre, dropouts y una sala pequeña.",
     {{kNoisAmount, 0.26f},
      {kNoisType, noiseType(9)},
      {kNoisTone, 0.47f},
      {kNoisDuck, 0.35f},
      {kWobbAmount, 0.28f},
      {kWobbXfade, 0.40f},
      {kWobbWowRate, 0.18f},
      {kWobbFlutterRate, 0.33f},
      {kMagnAmount, 0.28f},
      {kMagnXfade, 0.55f},
      {kMagnDropouts, 0.20f},
      {kSpacEnable, 1.0f},
      {kSpacAmount, 0.22f},
      {kSpacDecay, 0.55f},
      {kSpacPreDelay, 0.18f},
      {kMastTone, 0.60f}}},

    {"Tubo saturado", "Etapa de tubo al frente: crunch con graves recortados.",
     {{kNoisAmount, 0.20f},
      {kNoisType, noiseType(4)},
      {kNoisTone, 0.60f},
      {kNoisDuck, 0.40f},
      {kDistAmount, 0.60f},
      {kDistType, distType(2)},
      {kMastInGain, 0.56f},
      {kMastTone, 0.60f},
      {kMastOutGain, 0.48f}}},

    {"Ambiente ancho", "Cola larga con pre-delay y anchura de salida: casi sin color.",
     {{kNoisAmount, 0.12f},
      {kNoisType, noiseType(13)},
      {kNoisTone, 0.50f},
      {kWobbAmount, 0.04f},
      {kSpacEnable, 1.0f},
      {kSpacAmount, 0.52f},
      {kSpacDecay, 0.85f},
      {kSpacPreDelay, 0.35f},
      {kMastWidth, 0.85f},
      {kMastTone, 0.55f}}},

    {"Maqueta", "Mezcla de ensayo pasada a casete: ruido de estudio, dropouts y hierro.",
     {{kNoisAmount, 0.28f},
      {kNoisType, noiseType(14)},
      {kNoisTone, 0.45f},
      {kNoisDuck, 0.30f},
      {kWobbAmount, 0.24f},
      {kWobbXfade, 0.50f},
      {kDistAmount, 0.30f},
      {kDistType, distType(1)},
      {kMagnAmount, 0.34f},
      {kMagnDropouts, 0.25f},
      {kSpacEnable, 1.0f},
      {kSpacAmount, 0.30f},
      {kSpacDecay, 0.55f},
      {kMastCutLo, 0.20f},
      {kMastCutHi, 0.70f}}},

    {"Limpio", "Referencia sin color: sólo el EQ de master, para comparar A/B.",
     {{kNoisAmount, 0.0f},
      {kWobbAmount, 0.0f},
      {kDistAmount, 0.0f},
      {kDigiAmount, 0.0f},
      {kSpacAmount, 0.0f},
      {kMagnAmount, 0.0f},
      {kDigiEnable, 0.0f},
      {kSpacEnable, 0.0f},
      {kMastEqEnable, 1.0f},
      {kMastTone, 0.52f},
      {kMastCutLo, 0.10f},
      {kMastCutHi, 0.90f},
      {kMastWidth, 0.50f},
      {kMastOutGain, 0.50f}}},
};

constexpr int kPresetCount = static_cast<int>(sizeof(kPresets) / sizeof(kPresets[0]));

// Módulos con Amount: si el preset los abre, su Enable tiene que estar en 1 (si no, el
// preset se oiría como si estuviera apagado).
struct ModuleEnable {
    int amountId;
    int enableId;
    const char* name;
};
const ModuleEnable kModules[] = {
    {kNoisAmount, kNoisEnable, "NOIS"},
    {kWobbAmount, kWobbEnable, "WOBB"},
    {kDistAmount, kDistEnable, "DIST"},
    {kDigiAmount, kDigiEnable, "DIGI"},
    {kSpacAmount, kSpacEnable, "SPAC"},
    {kMagnAmount, kMagnEnable, "MAGN"},
};

} // namespace

int factoryPresetCount() { return kPresetCount; }

const FactoryPreset& factoryPreset(int index) {
    if (index < 0 || index >= kPresetCount) return kPresets[0];
    return kPresets[index];
}

void factoryPresetValues(int index, float* out, int count) {
    if (out == nullptr) return;
    const HostParamSpec* specs = HostParamTable::all();
    const int total = HostParamTable::count();
    for (int i = 0; i < count; ++i)
        out[i] = i < total ? specs[i].defaultValue : 0.0f;
    for (const PresetOverride& o : factoryPreset(index).overrides) {
        if (o.hostId >= 0 && o.hostId < count) out[o.hostId] = o.value;
    }
}

bool validateFactoryPresets(std::vector<std::string>& errors) {
    const int total = HostParamTable::count();
    const size_t before = errors.size();

    for (int i = 0; i < kPresetCount; ++i) {
        const FactoryPreset& p = kPresets[i];
        if (p.name == nullptr || p.name[0] == '\0') {
            errors.push_back("preset " + std::to_string(i) + ": nombre vacio");
            continue;
        }
        if (p.description == nullptr || p.description[0] == '\0')
            errors.push_back(std::string(p.name) + ": sin descripcion");
        for (int j = 0; j < i; ++j) {
            if (std::string(p.name) == kPresets[j].name)
                errors.push_back(std::string(p.name) + ": nombre repetido");
        }
        for (const PresetOverride& o : p.overrides) {
            if (o.hostId < 0 || o.hostId >= total) {
                errors.push_back(std::string(p.name) + ": id de host fuera de rango " +
                                  std::to_string(o.hostId));
                continue;
            }
            if (!(o.value >= 0.0f && o.value <= 1.0f)) {
                errors.push_back(std::string(p.name) + ": valor fuera de 0..1 en " +
                                  HostParamTable::all()[o.hostId].name);
                continue;
            }
            const HostParamSpec& spec = HostParamTable::all()[o.hostId];
            if (spec.isEnum) {
                // El enum tiene que caer en un índice entero, si no el redondeo del
                // ParamStore lo lleva a un tipo vecino sin que se note.
                const float scaled = o.value * spec.max;
                if (std::fabs(scaled - std::round(scaled)) > 1e-4f)
                    errors.push_back(std::string(p.name) + ": " + spec.name +
                                      " no cae en un indice entero");
            }
        }
        // Enable coherente con el Amount de cada módulo.
        const int nOverrides = static_cast<int>(p.overrides.size());
        auto valueOf = [&p](int id, float fallback) {
            for (const PresetOverride& o : p.overrides)
                if (o.hostId == id) return o.value;
            return fallback;
        };
        if (nOverrides >= 0) {
            for (const ModuleEnable& m : kModules) {
                const float amount = valueOf(m.amountId, HostParamTable::all()[m.amountId].defaultValue);
                const float enable = valueOf(m.enableId, HostParamTable::all()[m.enableId].defaultValue);
                if (amount > 0.0f && enable < 0.5f)
                    errors.push_back(std::string(p.name) + ": " + m.name +
                                     " tiene Amount > 0 con Enable apagado");
            }
        }
    }
    return errors.size() == before;
}

} // namespace rtxx
