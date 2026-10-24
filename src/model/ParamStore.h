#pragma once
// Estado de parámetros del motor: dos dominios explícitos.
//
// * Modelo (125): ids de la tabla, es lo que leen las condiciones de arista y los
//    bloques ("InLevel" es una ganancia LINEAL, "Dist_MixInternal" un mix 0..1).
// * Host (58): la superficie de automatización medida en el motor; sus valores
//    son normalizados 0..1 y se convierten al dominio del modelo con una LEY.
//
// Política de honestidad: una ley sólo existe aquí si está verificada contra el
// almacenan igual (el plugin no puede rechazarlos) pero NO tocan el modelo, y
// `coverage()` los reporta como no implementados. Nunca se adivina una ley.
#include <array>
#include <string>

#include "block/Block.h"
#include "model/HostParam.h"

namespace rtxx {

// Estado de la ley de conversión host→modelo.
enum class LawStatus {
Verified, // ajustado por medida y con gate en tests/
    Identity,   // el modelo ya está en 0..1: no hay ley que deducir
    Unverified  // el parámetro se guarda pero todavía no afecta al audio
};

class ParamStore : public ParamSource {
public:
    ParamStore();

// Recalcula los parámetros derivados que el DSL del motor consume (copias y
// syncs). Sólo las relaciones verificadas; ver ParamStore.cpp.
    // Es también la capa donde viven los repartos que el DSP no ve: el cruce triangular
    // Wobble_Xfade → Depth/Depth2 y Magnetic_Xfade → WearDepth/Depth, el desfase estéreo
    // y los dropouts de Magnetic.

// Deja todos los parámetros en los defaults de fábrica: en dominio host para los 58
// del host y en dominio de modelo para el resto.
    void resetToDefaults();

    // Superficie del host: id 0..57, valor normalizado 0..1 (se recorta).
    // Devuelve false si el id no existe o si su ley no está verificada (en ese caso
    // el valor queda guardado, pero el modelo no cambia).
    bool setHost(int id, float normalized);
    float hostValue(int id) const;

    // Dominio del modelo.
    void setModel(const std::string& id, float value);
    float get(const char* id) const override;
    bool hasModel(const std::string& id) const;
    // Rango del parámetro en el dominio del modelo (falso si no existe).
    bool modelRange(const std::string& id, float& lo, float& hi) const;

    // Cobertura: cuántos parámetros del host afectan ya al audio.
    struct Coverage {
        int total = 0;
        int verified = 0;
        int identity = 0;
        int unverified = 0;
    };
    Coverage coverage() const;
    LawStatus lawStatus(int hostId) const;
    const char* lawName(int hostId) const;

    static int hostCount() { return HostParamTable::count(); }
    static const HostParamSpec* hostSpec(int id);

private:
    static constexpr int kModelParams = 512;
    struct Entry {
        std::string id;
        float value;
    };
    std::array<Entry, kModelParams> model_{};
    int modelCount_ = 0;
    std::array<float, 64> hostValues_{};

    void syncDerived();
    float* findModel(const std::string& id);
    const float* findModel(const std::string& id) const;
};

} // namespace rtxx
