// Fase 0 del ROADMAP: el motor tiene que comportarse como El motor en lo básico,
// medido y no supuesto:
// * latencia de exactamente 64 muestras (kAudioUnitProperty_Latency del motor),
//   * en bypass la salida es la entrada desplazada 64 muestras BIT A BIT,
//   * el resultado no depende del tamaño de bloque que pida el host.
#include <cstdio>
#include <cstring>
#include <vector>

#include "engine/Engine.h"
#include "model/ParamStore.h"

namespace {

int failures = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FALLO: %s\n", what);
        ++failures;
    }
}

// Bypass: los 7 Enable a 0, igual que el caso de referencia por medida.
void setBypass(rtxx::ParamStore& p) {
    p.resetToDefaults();
    for (int id : {7, 14, 21, 27, 35, 42, 49}) p.setHost(id, 0.0f);
}

std::vector<float> lcg(int n, unsigned seed) {
    std::vector<float> v(static_cast<size_t>(n));
    unsigned s = seed;
    for (auto& x : v) {
        s = s * 1664525u + 1013904223u;
        x = (static_cast<float>(s >> 8) / static_cast<float>(1u << 24)) * 2.0f - 1.0f;
    }
    return v;
}

// Renderiza con un tamaño de bloque dado y devuelve la salida entrelazada.
std::vector<float> render(rtxx::Engine& e, const std::vector<float>& l, const std::vector<float>& r,
                          int block) {
    const int n = static_cast<int>(l.size());
    std::vector<float> out(static_cast<size_t>(n) * 2, 0.0f);
    std::vector<float> bl(static_cast<size_t>(block)), br(static_cast<size_t>(block));
    std::vector<float> ol(static_cast<size_t>(block)), orr(static_cast<size_t>(block));
    int done = 0;
    while (done < n) {
        const int take = (done + block <= n) ? block : (n - done);
        for (int i = 0; i < take; ++i) {
            bl[static_cast<size_t>(i)] = l[static_cast<size_t>(done + i)];
            br[static_cast<size_t>(i)] = r[static_cast<size_t>(done + i)];
        }
        rtxx::PortData pin;
        pin.numChannels = 2;
        pin.numSamples = take;
        pin.ch[0] = bl.data();
        pin.ch[1] = br.data();
        rtxx::PortData pout;
        pout.numChannels = 2;
        pout.numSamples = take;
        pout.ch[0] = ol.data();
        pout.ch[1] = orr.data();
        e.process(pin, pout, take);
        for (int i = 0; i < take; ++i) {
            out[static_cast<size_t>(done + i) * 2] = ol[static_cast<size_t>(i)];
            out[static_cast<size_t>(done + i) * 2 + 1] = orr[static_cast<size_t>(i)];
        }
        done += take;
    }
    return out;
}

} // namespace

// El vumetro por nodo es un atomic que escribe el hilo de audio: no puede alterar ni un
// bit de la senal. Se compara el mismo render con el medidor apagado y encendido.
void testMeteringDoesNotAffectAudio() {
    const int n = 4096;
    const std::vector<float> l = lcg(n, 777u);
    const std::vector<float> r = lcg(n, 778u);

    // Dos motores IDENTICOS (misma construccion y mismo prepare, que es lo que garantiza
    // partir del mismo estado): uno con el vumetro apagado y otro encendido.
    auto runWith = [&](bool metering, rtxx::Engine& e) {
        rtxx::ParamStore p;
        p.resetToDefaults();
        std::string err;
        check(e.build(err), "build para el test del vumetro");
        e.prepare(44100.0, 64);
        e.setMeteringEnabled(metering);
        return render(e, l, r, 64);
    };

    rtxx::ParamStore pOff, pOn;
    rtxx::Engine eOff(pOff), eOn(pOn);
    const auto off = runWith(false, eOff);
    const auto on = runWith(true, eOn);

    check(off.size() == on.size(), "misma longitud con el vumetro encendido");
    bool identical = true;
    for (size_t i = 0; i < off.size(); ++i)
        if (off[i] != on[i]) identical = false;
    check(identical, "el vumetro no cambia la senal (bit a bit)");

    std::printf("   niveles por modulo: NOISE %.4f WOBBLE %.4f DIST %.4f DIGITAL %.4f\n",
                (double)eOn.moduleLevel(0), (double)eOn.moduleLevel(1),
                (double)eOn.moduleLevel(2), (double)eOn.moduleLevel(3));
    check(eOn.moduleLevel(3) > 0.0f, "el vumetro de DIGITAL mide algo con audio");
    check(eOff.moduleLevel(3) == 0.0f, "con el vumetro apagado no se mide nada");
}

int main() {
    const int n = 5000;
    const int lat = rtxx::Engine::latencySamples();
    check(lat == 64, "latencia declarada == 64");

    const std::vector<float> l = lcg(n, 12345u);
    std::vector<float> r = lcg(n, 54321u);

    // 1. Grafo completo instanciado desde GraphSpec.
    {
        rtxx::ParamStore p;
        setBypass(p);
        rtxx::Engine e(p);
        std::string err;
        check(e.build(err), ("build del grafo: " + err).c_str());
        check(e.nodeNames().size() == 19, "19 nodos instanciados (incluye hijos de ruido)");
        check(e.edgeCount() == 28, "28 aristas instanciadas (raíz + sub-contenedores)");
    }

    // 2. Bypass bit-exacto con un tamaño de bloque que es divisor exacto.
    std::vector<float> base;
    {
        rtxx::ParamStore p;
        setBypass(p);
        rtxx::Engine e(p);
        std::string err;
        check(e.build(err), "build para el caso base");
        e.prepare(44100.0, 64);
        base = render(e, l, r, 64);
        int bad = 0;
        for (int i = lat; i < n; ++i) {
            if (std::memcmp(&base[static_cast<size_t>(i) * 2], &l[static_cast<size_t>(i - lat)], 4) != 0) ++bad;
            if (std::memcmp(&base[static_cast<size_t>(i) * 2 + 1], &r[static_cast<size_t>(i - lat)], 4) != 0) ++bad;
        }
        check(bad == 0, "bypass bit-exacto a 64 muestras con bloque 64");
        // La latencia son 64 ceros exactos.
        int nz = 0;
        for (int i = 0; i < lat; ++i)
            if (base[static_cast<size_t>(i) * 2] != 0.0f || base[static_cast<size_t>(i) * 2 + 1] != 0.0f) ++nz;
        check(nz == 0, "los primeros 64 frames son silencio");
    }

    // 3. El resultado no depende del bloque del host (37, 1, 4096).
    for (int block : {37, 1, 4096}) {
        rtxx::ParamStore p;
        setBypass(p);
        rtxx::Engine e(p);
        std::string err;
        check(e.build(err), "build para bloque variable");
        e.prepare(44100.0, block);
        const std::vector<float> out = render(e, l, r, block);
        const bool same = std::memcmp(out.data(), base.data(), out.size() * sizeof(float)) == 0;
        if (!same) std::printf("FALLO: bloque %d difiere del bloque 64\n", block);
        check(same, "misma salida con cualquier tamaño de bloque");
    }

    // 4. reset() devuelve el motor al mismo estado: dos corridas idénticas.
    {
        rtxx::ParamStore p;
        setBypass(p);
        rtxx::Engine e(p);
        std::string err;
        check(e.build(err), "build para el test de reset");
        e.prepare(44100.0, 64);
        const std::vector<float> a = render(e, l, r, 64);
        e.prepare(44100.0, 64);
        const std::vector<float> b = render(e, l, r, 64);
        check(std::memcmp(a.data(), b.data(), a.size() * sizeof(float)) == 0, "reset determinista");
    }

    testMeteringDoesNotAffectAudio();

    if (failures == 0) std::printf("test_engine: OK\n");
    return failures == 0 ? 0 : 1;
}
