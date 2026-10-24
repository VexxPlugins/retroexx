// Verifica el intérprete del DSL contra las CUATRO cadenas reales del motor (no fixtures).
// Si esto cambia, el módulo Dist deja de tener 8 tipos y el master EQ deja de ser el mismo.
#include <cmath>
#include <cstdio>
#include <map>
#include <string>

#include "block/PlugChain.h"
#include "block/PlugChainEval.h"
#include "block/PlugChainText.h"
#include "model/ParamSpec.h"

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
    if (!ok) { std::printf("FALLO: %s\n", what); ++failures; }
}
void checkEq(long a, long b, const char* what) {
    if (a != b) { std::printf("FALLO: %s (%ld != %ld)\n", what, a, b); ++failures; }
}

rtxx::Chain parse(const char* text, const char* label) {
    rtxx::Chain c;
    std::string err;
    if (!rtxx::parsePlugChain(text, c, err)) {
        std::printf("FALLO: parse de %s: %s\n", label, err.c_str());
        ++failures;
    }
    return c;
}

// Cuenta etapas activas para un Type y Enable dados.
long activeCount(const rtxx::Chain& c, int type, int enable) {
    FakeParams p;
    p.v["Enable"] = static_cast<float>(enable);
    p.v["Type"] = static_cast<float>(type);
    // Los nombres que usa el DSL en el masterFilter/tone
    p.v["CutHard_Lo_Enable"] = 1.0f;
    p.v["CutHard_Hi_Enable"] = 1.0f;
    p.v["CutSoft_Lo_Enable"] = 1.0f;
    p.v["CutSoft_Hi_Enable"] = 1.0f;
    p.v["Tilt_Enable"] = 1.0f;
    p.v["Mid_Enable"] = 1.0f;
    const auto ev = rtxx::evaluateChain(c, p);
    long n = 0;
    for (const auto& e : ev) if (e.active) ++n;
    return n;
}

} // namespace

int main() {
// --- 1. Estructura real de las cuatro cadenas del motor ---
    const auto dist = parse(rtxx::kPlugChainDist, "plugChainDist");
    const auto master = parse(rtxx::kPlugChainMasterFilter, "plugchainMasterFilter");
    const auto tone = parse(rtxx::kPlugChainToneControl, "plugchainToneControl");
    const auto eq2 = parse(rtxx::kPlugChain2BandEQ, "plugchain2BandEQ");

    checkEq(static_cast<long>(dist.nodes.size()), 20, "Dist tiene 20 etapas");
    checkEq(static_cast<long>(master.nodes.size()), 5, "masterFilter tiene 5 etapas");
    checkEq(static_cast<long>(tone.nodes.size()), 2, "toneControl tiene 2 etapas");
    checkEq(static_cast<long>(eq2.nodes.size()), 4, "2bandEQ tiene 4 etapas");

    // Tipos de etapa del masterFilter: hp12, lp12, lp12, hp6, lp6 (eso es lo que lo hace
    // no ser un simple filtro: dos cortes duros arriba y suaves).
    check(master.nodes.size() == 5 && master.nodes[0].kind == "hp12", "master[0] = hp12");
    check(master.nodes.size() > 1 && master.nodes[1].kind == "lp12", "master[1] = lp12");
    check(master.nodes.size() > 2 && master.nodes[2].kind == "lp12", "master[2] = lp12");
    check(master.nodes.size() > 3 && master.nodes[3].kind == "hp6", "master[3] = hp6");
    check(master.nodes.size() > 4 && master.nodes[4].kind == "lp6", "master[4] = lp6");

    check(tone.nodes.size() == 2 && tone.nodes[0].kind == "hpShelf", "tone[0] = hpShelf");
    check(tone.nodes.size() > 1 && tone.nodes[1].kind == "bpPeak", "tone[1] = bpPeak");

    // --- 2. Todos los tipos de etapa son conocidos ---
    for (const auto* c : {&dist, &master, &tone, &eq2}) {
        for (const auto& n : c->nodes) {
            if (!rtxx::isKnownStageKind(n.kind)) {
                std::printf("FALLO: tipo de etapa no reconocido: %s\n", n.kind.c_str());
                ++failures;
            }
        }
    }

    // --- 3. Condiciones [enabled]: el mecanismo de los 8 tipos ---
    // Con Type fuera de 0..7, el Dist no debería activar etapas específicas de tipo, pero sí
    // las dos etapas de rango completo (gain Type 0..7 al principio y al final).
    checkEq(activeCount(dist, 0, 1), 7, "Dist Type 0 (Crunch) activa 7 etapas");
    checkEq(activeCount(dist, 5, 1), 5, "Dist Type 5 (TubePair) activa 5 etapas");
    checkEq(activeCount(dist, 6, 1), 3, "Dist Type 6 (AirPressure) activa 3 etapas");
    checkEq(activeCount(dist, 7, 1), 5, "Dist Type 7 (IronTransformer) activa 5 etapas");
    checkEq(activeCount(dist, 4, 1), 2, "Dist Type 4 no tiene etapa propia: sólo los 2 gain de rango");

    // Con Enable = 0 no se activa nada, aunque el Type sea válido.
    checkEq(activeCount(dist, 0, 0), 0, "Dist con Enable=0 no activa ninguna etapa");

    // El masterFilter con todos sus Enable a 1 activa sus 5 etapas; con los corte apagados, 0.
    {
        FakeParams p;
        p.v["CutHard_Lo_Enable"] = 1; p.v["CutHard_Hi_Enable"] = 1;
        p.v["CutSoft_Lo_Enable"] = 1; p.v["CutSoft_Hi_Enable"] = 1;
        auto ev = rtxx::evaluateChain(master, p);
        long n = 0; for (auto& e : ev) if (e.active) ++n;
        checkEq(n, 5, "masterFilter activa 5 etapas con todo encendido");

        FakeParams q;  // todos a 0
        auto ev2 = rtxx::evaluateChain(master, q);
        long m = 0; for (auto& e : ev2) if (e.active) ++m;
        checkEq(m, 0, "masterFilter inactivo con los 4 Enable a 0");
    }

    // --- 4. La condición constante [enabled] 1 ---
    {
        const auto conds = master.nodes[0].conditions();
        checkEq(static_cast<long>(conds.size()), 2, "master[0] tiene dos condiciones");
        check(conds.size() == 2 && conds[0]->args.size() == 1 &&
              conds[0]->args[0].kind == rtxx::ArgKind::Number && conds[0]->args[0].number == 1.0,
              "la primera condición del master es la constante 1");
    }

    // --- 5. Clasificación de argumentos sobre el texto real ---
    {
        // `- freq 900`            -> número
        // `- db (Amount) 10 12`   -> ScopedParam + dos números
        // `- lowFreq FreqLo 20 20000 log` -> Param + números + keyword
        const rtxx::Node* distortNode = nullptr;
        for (const auto& n : dist.nodes)
            if (n.kind == "distort") { distortNode = &n; break; }
        check(distortNode != nullptr, "hay una etapa distort en el Dist");
        if (distortNode) {
            const auto* low = distortNode->find("lowFreq");
            check(low != nullptr && low->args.size() == 4, "lowFreq tiene 4 argumentos");
            if (low && low->args.size() == 4) {
                check(low->args[0].kind == rtxx::ArgKind::Param && low->args[0].text == "FreqLo",
                      "lowFreq[0] es el parámetro FreqLo");
                check(low->args[1].kind == rtxx::ArgKind::Number && low->args[1].number == 20.0,
                      "lowFreq[1] = 20");
                check(low->args[2].kind == rtxx::ArgKind::Number && low->args[2].number == 20000.0,
                      "lowFreq[2] = 20000");
                check(low->args[3].isKeyword("log"), "lowFreq[3] = keyword log");
            }
        }
        // Un (Amount) entre paréntesis.
        bool sawScoped = false;
        for (const auto& n : dist.nodes)
            for (const auto& p : n.params)
                for (const auto& a : p.args)
                    if (a.kind == rtxx::ArgKind::ScopedParam) sawScoped = true;
        check(sawScoped, "aparece al menos un parámetro cualificado '(X)'");
    }

    // --- 6. Valores estáticos: el masterFilter tiene freq/q constantes ---
    {
        // En el masterFilter `freq` NO es estático: viene del modelo (CutHard_LoFreq) con rango
        // 20..20000 y escala logcut. `q` sí es constante.
        const auto* freq = master.nodes[0].find("freq");
        double v = 0;
        check(freq != nullptr && !rtxx::staticParamValue(*freq, v),
              "freq del master depende del modelo, no es estático");
        check(freq != nullptr && freq->args.size() == 4 &&
              freq->args[0].kind == rtxx::ArgKind::Param && freq->args[0].text == "CutHard_LoFreq" &&
              freq->args[1].number == 20.0 && freq->args[2].number == 20000.0 &&
              freq->args[3].isKeyword("logcut"),
              "freq del master = CutHard_LoFreq con rango 20..20000 logcut");

        // `q` del master tampoco: es CutHard_LoQ con rango 0.1..5 log.
        const auto* q = master.nodes[0].find("q");
        check(q != nullptr && !rtxx::staticParamValue(*q, v),
              "q del master depende del modelo (CutHard_LoQ 0.1..5 log)");

        // Estáticos de verdad: los hay en el Dist y en el Tone.
        const auto* bpp = dist.nodes[2].find("freq");   // bpPeak con freq 900
        check(bpp != nullptr && rtxx::staticParamValue(*bpp, v) && std::fabs(v - 900.0) < 1e-9,
              "bpPeak del Dist tiene freq 900 Hz constante");
        const auto* toneFreq = tone.nodes[0].find("freq");
        check(toneFreq != nullptr && rtxx::staticParamValue(*toneFreq, v) &&
              std::fabs(v - 800.0) < 1e-9, "hpShelf del Tone tiene freq 800 Hz constante");
        const auto* toneQ = tone.nodes[0].find("q");
        check(toneQ != nullptr && rtxx::staticParamValue(*toneQ, v) && std::fabs(v - 0.5) < 1e-9,
              "hpShelf del Tone tiene q 0.5 constante");
    }

    // --- 7. Errores de formato se detectan ---
    {
        rtxx::Chain c; std::string err;
        check(!rtxx::parsePlugChain("- freq 900\n", c, err), "continuación sin etapa -> error");
        check(!rtxx::parsePlugChain("; solo comentarios\n", c, err), "texto sin etapas -> error");
        check(rtxx::parsePlugChain("gain\t[enabled]\t1\n", c, err), "una sola etapa constante parsea");
    }

    if (failures == 0) std::printf("test_plugchain: OK\n");
    return failures == 0 ? 0 : 1;
}
