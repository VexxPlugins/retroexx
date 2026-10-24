#include <cmath>
#include <cstdio>
#include <map>
#include <string>

#include "block/Condition.h"

namespace {

struct FakeParams : rtxx::ParamSource {
    std::map<std::string, float> v;
    float get(const char* id) const override {
        auto it = v.find(id);
        return it == v.end() ? 0.0f : it->second;
    }
};

int failures = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FALLO: %s\n", what);
        ++failures;
    }
}

void checkNear(float a, float b, const char* what, float tol = 1e-6f) {
    if (std::fabs(a - b) > tol) {
        std::printf("FALLO: %s (%.6f != %.6f)\n", what, a, b);
        ++failures;
    }
}

} // namespace

int main() {
    FakeParams p;
    p.v["Dist_Enable"] = 1.0f;
    p.v["Dist_InGain"] = 0.5f;
    p.v["Dist_OutGain"] = 0.5f;
    p.v["Dist_MixInternal"] = 0.25f;
    p.v["OutLevel"] = 1.0f;
    p.v["OutGainEnable"] = 1.0f;

    // Constante implícita: condición vacía = 1.
    checkNear(rtxx::Condition("").evaluate(p), 1.0f, "condición vacía = 1");

    // Producto tal como aparece en el grafo real.
    {
        rtxx::Condition c("Dist_Enable*Dist_InGain*Dist_MixInternal");
        check(c.valid(), "expresión del grafo válida");
        checkNear(c.evaluate(p), 1.0f * 0.5f * 0.25f, "producto de tres términos");
    }

    // `!x` = 1 - x (crossfade continuo, NO negación lógica).
    {
        rtxx::Condition c("!Dist_Enable");
        checkNear(c.evaluate(p), 0.0f, "!1 = 0");
        p.v["Dist_Enable"] = 0.0f;
        checkNear(c.evaluate(p), 1.0f, "!0 = 1");
        p.v["Dist_Enable"] = 0.4f;
        checkNear(c.evaluate(p), 0.6f, "!0.4 = 0.6 (continuo)");
        p.v["Dist_Enable"] = 1.0f;
    }

    // Precedencia y paréntesis.
    checkNear(rtxx::Condition("!Dist_MixInternal*Dist_Enable").evaluate(p), 0.75f,
              "!mix*enable = 0.75");
    checkNear(rtxx::Condition("(Dist_InGain+Dist_OutGain)*2").evaluate(p), 2.0f, "paréntesis");
    checkNear(rtxx::Condition("OutLevel*OutGainEnable").evaluate(p), 1.0f, "salida");

    // Identificadores recolectados.
    {
        rtxx::Condition c("A*B*!C");
        check(c.identifiers().size() == 3, "recolecta 3 identificadores");
    }

    // Expresión inválida se reporta, no revienta.
    {
        rtxx::Condition c("A*");
        check(!c.valid(), "expresión inválida detectada");
        check(!c.error().empty(), "mensaje de error presente");
    }

    // Parámetro ausente = 0 (no silencioso: el test lo fija a propósito).
    checkNear(rtxx::Condition("NoExiste").evaluate(p), 0.0f, "parámetro ausente = 0");

    if (failures == 0) std::printf("test_condition: OK\n");
    return failures == 0 ? 0 : 1;
}
