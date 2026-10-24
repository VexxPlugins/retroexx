#pragma once
// Presets de fábrica de Retroexx.
//
// SON NUESTROS, no los del motor: los presets del Retroexx son activos de y no se
// copian (PLAN.md §0). Lo que sí es del motor es la SUPERFICIE de parámetros (la
// tabla medida `HostParams.gen.cpp`) y cada ley de escala (`ParamStore.cpp`), así que un
// preset aquí es simplemente un punto en ese espacio: nombre propio + valores
// normalizados 0..1 por id de host.
//
// Los valores de partida son los defaults de fábrica (`HostParamSpec.defaultValue`),
// y cada preset sólo sobrescribe lo que necesita. El preset 0 es "Init" (sin cambios).
//
// Los presets evitan a propósito las zonas declaradas BLOQUEADAS [?] en el ROADMAP:
// los filtros del Digital (posición sin ley medida) y los filtros del Space se quedan en
// su default 0/1, que es el paso sin corte medido.
#include <vector>

namespace rtxx {

struct PresetOverride {
    int hostId;
    float value;  // normalizado 0..1, el mismo dominio que ParamStore::setHost
};

struct FactoryPreset {
    const char* name;
    const char* description;
    std::vector<PresetOverride> overrides;
};

int factoryPresetCount();
const FactoryPreset& factoryPreset(int index);

// Rellena `out` con `count` valores normalizados (los de HostParamTable) y aplica los
// overrides del preset. `out` debe tener al menos HostParamTable::count() entradas.
void factoryPresetValues(int index, float* out, int count);

// Comprueba la definición de los presets (ids en rango, valores 0..1, nombres únicos).
// Devuelve true y deja el motivo en `error` si algo no cuadra. Lo usa el test.
bool validateFactoryPresets(std::vector<std::string>& errors);

} // namespace rtxx
