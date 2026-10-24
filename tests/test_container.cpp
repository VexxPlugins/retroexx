// Regresión: un contenedor ANIDADO tiene que entregar su salida al buffer del padre.
//
// Bug encontrado en la fase 6: `Container::process` reasignaba los punteros de la vista
// (`out.ch[c] = out_.channel(c)`) en vez de copiar. Como la vista es un `PortData` local del
// contenedor padre, la salida del hijo se perdía y el ruido del bloque `NoiseFX` no llegaba al
// grafo (el seco sí, porque va por otra arista). No se notaba antes porque todos los
// contenedores anidados eran identidad y su salida era 0.
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>

#include "block/Container.h"
#include "blocks/Identity.h"

namespace {

int failures = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FALLO: %s\n", what);
        ++failures;
    }
}

struct FakeParams : rtxx::ParamSource {
    float get(const char*) const override { return 1.0f; }
};

// Contenedor que duplica la señal: in -> Gain.in, Gain.out -> out, con el bloque
// `Identity` sustituido por un doblador para poder distinguir la copia del motor.
class Doubler : public rtxx::iBlock {
public:
    void process(const rtxx::PortData* in, rtxx::PortData& out) override {
        const int n = out.numSamples;
        for (int c = 0; c < out.numChannels; ++c)
            for (int i = 0; i < n; ++i) out.ch[c][i] = 2.0f * (in && in->ch[c] ? in->ch[c][i] : 0.0f);
    }
};

} // namespace

int main() {
    rtxx::PrepareContext ctx{44100.0, rtxx::kQuantum};
    FakeParams params;

    // --- Caso 0: un solo contenedor (aisla la copia de la anidación) -------------------
    {
        auto one = std::make_unique<rtxx::Container>();
        one->addBlock("Gain", std::make_unique<Doubler>());
        one->addEdge("in", "Gain.in", "");
        one->addEdge("Gain.out", "out", "");
        one->setParams(&params);
        std::string e;
        check(one->build(e), "contenedor simple compila");
        one->prepare(ctx);
        std::vector<float> i1(static_cast<size_t>(rtxx::kQuantum), 0.25f);
        std::vector<float> o1(static_cast<size_t>(rtxx::kQuantum), 0.0f);
        rtxx::PortData pi, po;
        pi.numChannels = 2; pi.numSamples = rtxx::kQuantum; pi.ch[0] = i1.data(); pi.ch[1] = i1.data();
        po.numChannels = 2; po.numSamples = rtxx::kQuantum; po.ch[0] = o1.data(); po.ch[1] = o1.data();
        one->process(&pi, po);
        std::printf("   [caso 0] out[0] = %.6f (esperado 0,5)\n", (double)o1[0]);
        check(std::fabs(o1[0] - 0.5f) < 1e-6f, "un solo contenedor dobla");
    }

    // --- Caso 1: contenedor anidado simple (interno dobla, externo pasa) --------------
    auto outer = std::make_unique<rtxx::Container>();
    auto inner = std::make_unique<rtxx::Container>();
    inner->addBlock("Gain", std::make_unique<Doubler>());
    inner->addEdge("in", "Gain.in", "");
    inner->addEdge("Gain.out", "out", "");
    outer->addBlock("Inner", std::move(inner));
    outer->addEdge("in", "Inner.in", "");
    outer->addEdge("Inner.out", "out", "");
    outer->setParams(&params);
    std::string err;
    check(outer->build(err), "el grafo anidado compila");
    outer->prepare(ctx);

    std::vector<float> in(static_cast<size_t>(rtxx::kQuantum), 0.25f);
    std::vector<float> out(static_cast<size_t>(rtxx::kQuantum), 0.0f);
    rtxx::PortData pin, pout;
    pin.numChannels = 2;
    pin.numSamples = rtxx::kQuantum;
    pin.ch[0] = in.data();
    pin.ch[1] = in.data();
    pout.numChannels = 2;
    pout.numSamples = rtxx::kQuantum;
    pout.ch[0] = out.data();
    pout.ch[1] = out.data();
    outer->process(&pin, pout);

    std::printf("   out[0] = %.6f (esperado 0,5)\n", (double)out[0]);
    check(std::fabs(out[0] - 0.5f) < 1e-6f, "el contenedor anidado dobla la señal (0,5)");
    check(std::fabs(out[rtxx::kQuantum - 1] - 0.5f) < 1e-6f, "dobla también la última muestra");

    // --- Caso 2: el anidado con una arista condicionada a 1 ---------------------------
    auto outer2 = std::make_unique<rtxx::Container>();
    auto inner2 = std::make_unique<rtxx::Container>();
    inner2->addBlock("Gain", std::make_unique<Doubler>());
    inner2->addEdge("in", "Gain.in", "");
    inner2->addEdge("Gain.out", "out", "Some_Enable");
    outer2->addBlock("Inner", std::move(inner2));
    outer2->addEdge("in", "Inner.in", "");
    outer2->addEdge("Inner.out", "out", "");
    outer2->setParams(&params);   // FakeParams devuelve 1.0 para todo
    err.clear();
    check(outer2->build(err), "compila con condición");
    outer2->prepare(ctx);
    std::fill(out.begin(), out.end(), 0.0f);
    outer2->process(&pin, pout);
    check(std::fabs(out[0] - 0.5f) < 1e-6f, "la arista condicionada también propaga");

    if (failures == 0) {
        std::printf("PASS test_container\n");
        return 0;
    }
    std::printf("FAIL test_container (%d comprobaciones)\n", failures);
    return 1;
}
