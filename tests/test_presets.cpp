// Presets de fabrica (fase 9 del ROADMAP, lo unico que quedaba pendiente del envoltorio).
//
// Son NUESTROS: nombre y valores propios sobre la superficie de 58 parametros. Este test
// fija lo que no depende de la calibracion:
//  1. La definicion es coherente (ids en rango, valores 0..1, enums enteros, nombres
//     unicos, Enable encendido si el Amount lo esta).
//  2. El preset 0 es EXACTAMENTE los defaults medidos, y cada preset aplica por
//     `ParamStore::setHost` sin que ninguna ley quede sin verificar (si un preset tocara
//     un parametro sin ley, el preset mentiria: se ve como `setHost == false`).
//  3. Cada preset suena distinto: se renderiza el mismo programa con cada uno y se
//     comparan RMS y centroide espectral por pares.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "engine/Engine.h"
#include "model/FactoryPresets.h"
#include "model/HostParam.h"
#include "model/ParamStore.h"

namespace {

int failures = 0;
void check(bool ok, const std::string& what) {
    if (!ok) {
        std::printf("FALLO: %s\n", what.c_str());
        ++failures;
    }
}

std::vector<float> renderPreset(int index, int numSamples) {
    rtxx::ParamStore store;
    const int total = rtxx::HostParamTable::count();
    std::vector<float> values(static_cast<size_t>(total), 0.0f);
    rtxx::factoryPresetValues(index, values.data(), total);
    // setHost ya sincroniza los derivados en cada escritura (ParamStore.cpp).
    for (int id = 0; id < total; ++id) store.setHost(id, values[static_cast<size_t>(id)]);

    rtxx::Engine engine(store);
    std::string error;
    if (!engine.build(error)) {
        std::printf("FALLO: el grafo no construye: %s\n", error.c_str());
        ++failures;
        return {};
    }
    engine.prepare(44100.0, 512);

    // Programa deterministico: suma de tres senos con envolvente suave (sin ruido, para que
    // dos renders del mismo preset sean identicos).
    std::vector<float> in(static_cast<size_t>(numSamples), 0.0f);
    for (int i = 0; i < numSamples; ++i) {
        const double t = static_cast<double>(i) / 44100.0;
        const double env = std::min(1.0, t / 0.05);
        in[static_cast<size_t>(i)] = static_cast<float>(
            env * 0.12 * (std::sin(2.0 * M_PI * 220.0 * t) +
                          0.6 * std::sin(2.0 * M_PI * 660.0 * t) +
                          0.3 * std::sin(2.0 * M_PI * 1760.0 * t)));
    }

    std::vector<float> l(static_cast<size_t>(numSamples), 0.0f);
    std::vector<float> r(static_cast<size_t>(numSamples), 0.0f);
    rtxx::PortData pin;
    pin.numChannels = 1;
    pin.numSamples = 0;
    pin.ch[0] = in.data();
    rtxx::PortData pout;
    pout.numChannels = 2;
    pout.numSamples = 0;
    pout.ch[0] = l.data();
    pout.ch[1] = r.data();

    const int block = 256;
    for (int pos = 0; pos + block <= numSamples; pos += block) {
        rtxx::PortData bi = pin;
        bi.numSamples = block;
        bi.ch[0] = in.data() + pos;
        rtxx::PortData bo = pout;
        bo.numSamples = block;
        bo.ch[0] = l.data() + pos;
        bo.ch[1] = r.data() + pos;
        engine.process(bi, bo, block);
    }
    // La latencia (64) hace que la cola quede dentro de la ventana: no hace falta recortar.
    return l;
}

struct Metrics {
    double rms = 0.0;
    double centroid = 0.0;
};

Metrics measure(const std::vector<float>& x, double sr = 44100.0) {
    Metrics m;
    // Se descarta el arranque (rampa del bloque de ruido, 20 ms) y se mide el resto.
    const size_t start = std::min(x.size(), static_cast<size_t>(0.10 * sr));
    double sum = 0.0;
    for (size_t i = start; i < x.size(); ++i) sum += static_cast<double>(x[i]) * x[i];
    const size_t n = x.size() > start ? x.size() - start : 0;
    m.rms = n > 0 ? std::sqrt(sum / static_cast<double>(n)) : 0.0;

    // Centroide por diferencia de primer orden (|x[n] - x[n-1]| mide contenido agudo sin
    // necesitar FFT): se normaliza por el RMS para que sea independiente del nivel.
    double dsum = 0.0;
    for (size_t i = start + 1; i < x.size(); ++i) {
        const double d = static_cast<double>(x[i]) - static_cast<double>(x[i - 1]);
        dsum += d * d;
    }
    const double dRms = n > 1 ? std::sqrt(dsum / static_cast<double>(n - 1)) : 0.0;
    m.centroid = m.rms > 1e-9 ? dRms / m.rms : 0.0;
    return m;
}

} // namespace

int main() {
    // 1. Definicion.
    check(rtxx::factoryPresetCount() >= 8, "hay al menos 8 presets de fabrica");
    std::vector<std::string> errors;
    const bool okDef = rtxx::validateFactoryPresets(errors);
    for (const std::string& e : errors) std::printf("  definicion: %s\n", e.c_str());
    check(okDef, "la definicion de los presets es coherente");

    // 2. Preset 0 == defaults medidos, y todos los presets aplican sin leyes ausentes.
    const int total = rtxx::HostParamTable::count();
    {
        std::vector<float> values(static_cast<size_t>(total), -1.0f);
        rtxx::factoryPresetValues(0, values.data(), total);
        bool same = true;
        for (int id = 0; id < total; ++id) {
            if (std::fabs(values[static_cast<size_t>(id)] -
                          rtxx::HostParamTable::all()[id].defaultValue) > 1e-6f)
                same = false;
        }
        check(same, "el preset 0 es exactamente los defaults de fábrica");
    }

    // Los OVERRIDES de cada preset son lo que el preset promete: tienen que aplicarse de
    // verdad (si un override cae en un parametro sin ley verificada, el preset mentiría).
    // Los defaults que el modelo aun no aplica NO son culpa del preset: se informan aparte.
    {
        rtxx::ParamStore store;
        std::vector<int> sinLeyDefaults;
        for (int id = 0; id < total; ++id) {
            if (!store.setHost(id, rtxx::HostParamTable::all()[id].defaultValue))
                sinLeyDefaults.push_back(id);
        }
        std::printf("test_presets: ids sin ley aplicada (defaults de fábrica):");
        for (int id : sinLeyDefaults) std::printf(" %d", id);
        std::printf("\n");
    }

    for (int p = 0; p < rtxx::factoryPresetCount(); ++p) {
        std::vector<float> values(static_cast<size_t>(total), 0.0f);
        rtxx::factoryPresetValues(p, values.data(), total);
        rtxx::ParamStore store;
        for (int id = 0; id < total; ++id) store.setHost(id, values[static_cast<size_t>(id)]);

        for (const rtxx::PresetOverride& o : rtxx::factoryPreset(p).overrides) {
            const bool applied = store.setHost(o.hostId, o.value);
            if (!applied)
                check(false, std::string("el preset '") + rtxx::factoryPreset(p).name +
                                 "' fija " + rtxx::HostParamTable::all()[o.hostId].name +
                                 ", que no tiene ley verificada (no haria nada)");
        }
        // Los overrides se conservan tal cual en la superficie del host.
        for (const rtxx::PresetOverride& o : rtxx::factoryPreset(p).overrides) {
            const float got = store.hostValue(o.hostId);
            if (std::fabs(got - o.value) > 1e-4f)
                check(false, std::string("el preset '") + rtxx::factoryPreset(p).name +
                                 "' no conserva " + rtxx::HostParamTable::all()[o.hostId].name);
        }
    }

    // 3. Cada preset suena distinto y finito. La distinción se mide por la ENERGÍA DE LA
    //    DIFERENCIA entre los dos renders (los dos motores son deterministas, así que es
    //    reproducible): dos presets que sólo se diferencien en un 2 % de la señal se
    //    consideran iguales. Comparar sólo RMS no vale: 'Maqueta' y 'Limpio' pueden caer en
    //    el mismo nivel por casualidad y sonar totalmente distintos.
    const int numSamples = 44100;
    const double kMinRelDiff = 0.02;
    std::vector<std::vector<float>> outs(static_cast<size_t>(rtxx::factoryPresetCount()));
    std::vector<Metrics> m(static_cast<size_t>(rtxx::factoryPresetCount()));
    for (int p = 0; p < rtxx::factoryPresetCount(); ++p) {
        const std::vector<float> out = renderPreset(p, numSamples);
        if (out.empty()) continue;
        outs[static_cast<size_t>(p)] = out;
        for (float v : out) {
            if (!std::isfinite(v) || std::fabs(v) > 4.0f) {
                check(false, std::string("el preset '") + rtxx::factoryPreset(p).name +
                                 "' produce muestras no finitas o desbordadas");
                break;
            }
        }
        m[static_cast<size_t>(p)] = measure(out);
        check(m[static_cast<size_t>(p)].rms > 1e-5,
              std::string("el preset '") + rtxx::factoryPreset(p).name + "' no es silencio");
    }
    for (int a = 0; a < rtxx::factoryPresetCount(); ++a) {
        for (int b = a + 1; b < rtxx::factoryPresetCount(); ++b) {
            const std::vector<float>& xa = outs[static_cast<size_t>(a)];
            const std::vector<float>& xb = outs[static_cast<size_t>(b)];
            if (xa.empty() || xa.size() != xb.size()) continue;
            const size_t start = std::min(xa.size(), static_cast<size_t>(0.10 * 44100.0));
            double diff = 0.0, ref = 0.0;
            for (size_t i = start; i < xa.size(); ++i) {
                const double d = static_cast<double>(xa[i]) - static_cast<double>(xb[i]);
                diff += d * d;
                ref += static_cast<double>(xa[i]) * static_cast<double>(xa[i]);
            }
            const double rel = ref > 0.0 ? std::sqrt(diff / ref) : 0.0;
            if (rel < kMinRelDiff) {
                std::printf("  iguales: %s vs %s (diferencia relativa %.4f)\n",
                            rtxx::factoryPreset(a).name, rtxx::factoryPreset(b).name, rel);
            }
            check(rel >= kMinRelDiff,
                  std::string("los presets '") + rtxx::factoryPreset(a).name + "' y '" +
                      rtxx::factoryPreset(b).name + "' suenan distinto");
        }
    }

    // Los dos renders del MISMO preset tienen que ser identicos (el material propio del
    // ruido es determinista: si esto falla, el motor dejo de serlo).
    {
        const std::vector<float> x = renderPreset(1, 8192);
        const std::vector<float> y = renderPreset(1, 8192);
        bool same = x.size() == y.size();
        for (size_t i = 0; same && i < x.size(); ++i) same = x[i] == y[i];
        check(same, "el mismo preset renderiza bit a bit identico (material determinista)");
    }

    if (failures == 0) std::printf("test_presets: OK (%d presets)\n", rtxx::factoryPresetCount());
    return failures == 0 ? 0 : 1;
}
