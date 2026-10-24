#include "PlugChainEval.h"

#include <algorithm>
#include <set>

#include "model/ParamSpec.h"

namespace rtxx {

namespace {

// ParamSource devuelve 0 para un parámetro inexistente; ParamTable distingue "vale 0" de
// "no existe" para no confundir un valor real con un hueco del modelo.
bool modelHasParam(const std::string& name) {
    return ParamTable::find(name.c_str()) != nullptr;
}

bool truthy(double v) { return v != 0.0; }

// El DSL sólo usa comparaciones con enteros (Type 0..7), así que la comparación es exacta
// sobre el valor redondeado a la mitad (evita 4.9999999 != 5).
long asIndex(double v) { return static_cast<long>(v < 0 ? v - 0.5 : v + 0.5); }

} // namespace

bool isKnownStageKind(const std::string& kind) {
    static const std::set<std::string> kKnown = {
        "gain", "bpPeak", "hp6", "lp6", "hp12", "lp12", "hpShelf", "lpShelf",
        "distort", "magicDistort", "tilt", "pan"
    };
    return kKnown.count(kind) != 0;
}

bool staticParamValue(const ParamLine& line, double& valueOut) {
    if (line.args.empty()) return false;
    const Arg& a = line.args[0];
    if (a.kind == ArgKind::Number) {
        valueOut = a.number;
        return true;
    }
    return false;
}

StageEvaluation evaluateStage(const Node& node, const ParamSource& params) {
    StageEvaluation ev;
    ev.active = true;  // sin condiciones, la etapa está activa

    for (const ParamLine* c : node.conditions()) {
        const auto& args = c->args;
        if (args.empty()) {
            ev.error = "condición [enabled] sin argumentos (línea " + std::to_string(c->line) + ")";
            ev.active = false;
            return ev;
        }

        // Caso constante: [enabled] 1
        if (args[0].kind == ArgKind::Number) {
            if (!truthy(args[0].number)) ev.active = false;
            continue;
        }

        if (args[0].kind != ArgKind::Param && args[0].kind != ArgKind::ScopedParam) {
            ev.error = "condición [enabled] con primer argumento no reconocido en la línea " +
                       std::to_string(c->line);
            ev.active = false;
            return ev;
        }

        const std::string& name = args[0].text;
        const bool known = modelHasParam(name);
        if (!known) ev.missingParams.push_back(name);
        const double v = params.get(name.c_str());

        if (args.size() == 1) {
            // [enabled] Enable -> distinto de 0
            if (!truthy(v)) ev.active = false;
        } else if (args.size() == 2) {
            // [enabled] Type 1 -> igualdad
            if (args[1].kind != ArgKind::Number) {
                ev.error = "condición [enabled] " + name + " con segundo argumento no numérico";
                ev.active = false;
                return ev;
            }
            if (asIndex(v) != asIndex(args[1].number)) ev.active = false;
        } else if (args.size() >= 3) {
            // [enabled] Type 0 7 -> rango inclusivo
            if (args[1].kind != ArgKind::Number || args[2].kind != ArgKind::Number) {
                ev.error = "condición [enabled] " + name + " con rango no numérico";
                ev.active = false;
                return ev;
            }
            double lo = args[1].number, hi = args[2].number;
            if (lo > hi) std::swap(lo, hi);
            const long vi = asIndex(v);
            if (vi < asIndex(lo) || vi > asIndex(hi)) ev.active = false;
        }
    }
    return ev;
}

std::vector<StageEvaluation> evaluateChain(const Chain& chain, const ParamSource& params) {
    std::vector<StageEvaluation> out;
    out.reserve(chain.nodes.size());
    for (const auto& n : chain.nodes) out.push_back(evaluateStage(n, params));
    return out;
}

} // namespace rtxx
