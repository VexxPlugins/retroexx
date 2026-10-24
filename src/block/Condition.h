#pragma once
#include <memory>
#include <string>
#include <vector>

#include "Block.h"

namespace rtxx {

// Expresión de coeficiente de una arista del grafo, tal como la escribe el Lua del motor:
//   "Dist_Enable*Dist_InGain*Dist_MixInternal"
//   "!Dist_Enable"            -> 1 - Dist_Enable
//   "OutLevel*OutGainEnable"
//
// Importante: los términos NO son booleanos. `Dist_InGain` y `Dist_OutGain`
// son ganancias en dB con default 1, `Dist_MixInternal` es un mix 0..1 (`audioSyncMul`), y
// `InLevel`/`OutLevel` son ganancias +-18 dB. Por eso el resultado de la expresión se usa como
// COEFICIENTE que multiplica la señal en la arista, y `!x` se evalúa como `1 - x`
// (crossfade continuo), no como negación lógica.
class Condition {
public:
    // `expr` vacío = siempre 1.0
    explicit Condition(std::string expr = {});

    bool valid() const { return error_.empty(); }
    const std::string& error() const { return error_; }

    // Evalúa con el modelo de parámetros actual. Lanza si la expresión no compiló.
    float evaluate(const ParamSource& params) const;

    // Identificadores referenciados (para validar contra la tabla de parámetros).
    const std::vector<std::string>& identifiers() const { return identifiers_; }

    static float applyNot(float x) { return 1.0f - x; }

    // Nodo del AST. Público para que la evaluación libre de Condition.cpp pueda nombrarlo.
    struct Node;

private:
    std::shared_ptr<const Node> root_;
    std::vector<std::string> identifiers_;
    std::string error_;
};

} // namespace rtxx
