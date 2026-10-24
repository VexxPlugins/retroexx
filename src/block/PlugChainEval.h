#pragma once
#include <string>
#include <vector>

#include "Block.h"
#include "PlugChain.h"

namespace rtxx {

// Resultado de evaluar las condiciones [enabled] de una etapa.
//
// Formas observadas en el DSL real (las cuatro cadenas): 
//   [enabled] 1                  -> constante; la etapa está siempre activa
//   [enabled] Enable             -> activa si el parámetro es distinto de 0
//   [enabled] Type 1             -> activa si el parámetro vale exactamente 1
//   [enabled] Type 0 7           -> activa si el parámetro cae en [0, 7] (inclusive)
// Todas las condiciones de una etapa se combinan con AND.
//
// Un parámetro que la etapa necesita y que no existe en el modelo se trata como 0, y se registra
// en `missingParams` para no confundir "no resuelto" con "vale cero".
struct StageEvaluation {
    bool active = false;
    std::vector<std::string> missingParams;
    std::string error;   // no vacío si alguna condición no tiene forma reconocida
};

StageEvaluation evaluateStage(const Node& node, const ParamSource& params);

// Evalúa todas las etapas de una cadena en orden.
std::vector<StageEvaluation> evaluateChain(const Chain& chain, const ParamSource& params);

// Tipos de etapa que el DSL usa realmente.
bool isKnownStageKind(const std::string& kind);

// Valor estático de un parámetro: sólo se resuelve si el argumento 0 es un número o si el
// parámetro no admite mapeo. Devuelve false si depende del modelo (pendiente de medir la escala).
bool staticParamValue(const ParamLine& line, double& valueOut);

} // namespace rtxx
