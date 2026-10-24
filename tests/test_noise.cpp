// Ruido (fase 6 del ROADMAP): `cNoiseFX` (generador propio de los 16 tipos) y `cSimpleComp`
// (compresor de ducking con el programa como sidechain).
//
// Fija lo que NO depende de la calibración: interruptores, SILENCIO del bloque
// cuando está apagado (el seco viaja por su propia arista, no por el generador), que el material
// es nuestro y determinista, que los dos canales son independientes y que el compresor atenúa el
// ruido con el programa. Los descriptores de los 16 tipos y las leyes de Amount/Tone/Follow/Duck
// se miden en la suite de medidas.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "blocks/Noise.h"
#include "blocks/SimpleComp.h"

namespace {

int failures = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FALLO: %s\n", what);
        ++failures;
    }
}
void near(double a, double b, const char* what, double tol = 1e-6) {
    if (std::fabs(a - b) > tol) {
        std::printf("FALLO: %s (%.9f != %.9f)\n", what, a, b);
        ++failures;
    }
}

struct FakeParams : rtxx::ParamSource {
    std::map<std::string, float> v;
    float get(const char* id) const override {
        auto it = v.find(id);
        return it == v.end() ? 0.0f : it->second;
    }
};

FakeParams noiseParams(int type, float levelDb, bool enabled = true) {
    FakeParams p;
    p.v["Noise_Enable"] = enabled ? 1.0f : 0.0f;
    p.v["Noise_NoiseType"] = static_cast<float>(type);
    p.v["Noise_NoiseLevel"] = levelDb;
    p.v["Noise_Tone"] = 0.0f;
    p.v["Noise_Follow"] = 0.0f;
    return p;
}

struct RunResult {
    std::vector<float> l, r;
};

// Procesa `seconds` de entrada (mono repetida en los dos canales) por un bloque.
RunResult runNoise(rtxx::iBlock& blk, double seconds, const std::vector<float>* in = nullptr) {
    const int n = static_cast<int>(seconds * 44100.0);
    std::vector<float> x(static_cast<size_t>(n), 0.0f);
    if (in) x = *in;
    RunResult y;
    y.l.assign(static_cast<size_t>(n), 0.0f);
    y.r.assign(static_cast<size_t>(n), 0.0f);
    blk.prepare(rtxx::PrepareContext{44100.0, rtxx::kQuantum});
    for (int start = 0; start < n; start += rtxx::kQuantum) {
        const int m = std::min(rtxx::kQuantum, n - start);
        rtxx::PortData pi;
        pi.numChannels = 2;
        pi.numSamples = m;
        pi.ch[0] = x.data() + start;
        pi.ch[1] = x.data() + start;
        rtxx::PortData po;
        po.numChannels = 2;
        po.numSamples = m;
        po.ch[0] = y.l.data() + start;
        po.ch[1] = y.r.data() + start;
        blk.process(&pi, po);
    }
    return y;
}

double rmsDb(const std::vector<float>& v, size_t from = 0) {
    double s = 0.0;
    size_t n = 0;
    for (size_t i = from; i < v.size(); ++i) {
        s += static_cast<double>(v[i]) * v[i];
        ++n;
    }
    return n ? 10.0 * std::log10(std::max(s / static_cast<double>(n), 1e-30)) : -300.0;
}

// --- Generador --------------------------------------------------------------------

void testPassthrough() {
    const std::vector<float> in = [] {
        std::vector<float> v(22050);
        for (size_t i = 0; i < v.size(); ++i) v[i] = 0.1f * std::sin(0.01f * static_cast<float>(i));
        return v;
    }();

    {   // Enable = 0 ⇒ CERO (no el seco: el seco va por su propia arista en el grafo).
        FakeParams p = noiseParams(11, 0.0f, false);
        rtxx::NoiseBlock blk(&p);
        const RunResult y = runNoise(blk, 0.5, &in);
        bool zero = true;
        for (size_t i = 0; i < in.size(); ++i) zero = zero && (y.l[i] == 0.0f);
        check(zero, "Noise_Enable = 0 ⇒ el generador devuelve silencio (el seco va aparte)");
    }
    {   // Nivel −120 dB ⇒ también silencio.
        FakeParams p = noiseParams(11, -120.0f);
        rtxx::NoiseBlock blk(&p);
        const RunResult y = runNoise(blk, 0.5, &in);
        bool zero = true;
        for (size_t i = 0; i < in.size(); ++i) zero = zero && (std::fabs(y.l[i]) < 1e-7f);
        check(zero, "nivel −120 dB ⇒ silencio");
    }
}

void testSignalIsOwnAndDeterministic() {
    FakeParams p = noiseParams(11, 0.0f);   // WHITE a 0 dB
    rtxx::NoiseBlock a(&p), b(&p);
    const RunResult ya = runNoise(a, 1.0);
    const RunResult yb = runNoise(b, 1.0);

    // Determinista: dos bloques iguales dan la MISMA señal (PRNG con semilla fija).
    bool same = true;
    for (size_t i = 0; i < ya.l.size(); ++i) same = same && (ya.l[i] == yb.l[i]);
    check(same, "el ruido es determinista (misma semilla ⇒ misma señal)");

    // Con energía y sin desbordar.
    const double rms = rmsDb(ya.l, 22050);
    check(rms > -60.0 && rms < 6.0, "el ruido tiene energía razonable");
    double peak = 0.0;
    for (float v : ya.l) peak = std::max(peak, static_cast<double>(std::fabs(v)));
    check(peak < 2.0, "el ruido no desborda");

    // Los dos canales son independientes (correlación baja).
    double num = 0.0, dl = 0.0, dr = 0.0;
    for (size_t i = 22050; i < ya.l.size(); ++i) {
        num += static_cast<double>(ya.l[i]) * ya.r[i];
        dl += static_cast<double>(ya.l[i]) * ya.l[i];
        dr += static_cast<double>(ya.r[i]) * ya.r[i];
    }
    const double corr = num / std::sqrt(std::max(dl * dr, 1e-30));
    check(std::fabs(corr) < 0.2, "los dos canales son independientes");
    std::printf("   WHITE a 0 dB: rms %.1f dBFS, pico %.3f, corr L/R %+.3f\n", rms, peak, corr);
}

// Curva de 1/3 de octava (dB, normalizada a 1 kHz) de una señal: es el descriptor que distingue
// los tipos.
std::vector<double> thirdOctave(const std::vector<float>& x) {
    static const double fc[] = {125, 250, 500, 1000, 2000, 4000, 8000};
    std::vector<double> out;
    const size_t n = 1 << 14;
    std::vector<double> re(n), im(n);
    for (size_t i = 0; i < n && i < x.size(); ++i) {
        const double w = 0.5 - 0.5 * std::cos(2.0 * M_PI * static_cast<double>(i) / n);
        re[i] = x[x.size() - n + i] * w;
    }
    // DFT directa en las 7 frecuencias centrales (bandas de 1/3 de octava por suma de bins).
    for (double f : fc) {
        double acc = 0.0;
        for (int k = -2; k <= 2; ++k) {
            const double fk = f * std::pow(2.0, k / 12.0);
            double sr = 0.0, si = 0.0;
            for (size_t i = 0; i < n; i += 4) {
                const double ph = 2.0 * M_PI * fk * static_cast<double>(i) / 44100.0;
                sr += re[i] * std::cos(ph);
                si += re[i] * std::sin(ph);
            }
            acc += sr * sr + si * si;
        }
        out.push_back(10.0 * std::log10(std::max(acc, 1e-30)));
    }
    const double ref = out[3];
    for (double& v : out) v -= ref;
    return out;
}

void testTypesDiffer() {
    // Los 16 tipos tienen que sonar DISTINTO: dos con la misma curva de 1/3 de octava serían una
    // fila duplicada en la tabla de recetas.
    std::vector<std::vector<double>> curves;
    for (int t = 0; t < rtxx::NoiseBlock::typeCount(); ++t) {
        FakeParams p = noiseParams(t, 0.0f);
        rtxx::NoiseBlock blk(&p);
        const RunResult y = runNoise(blk, 1.0);
        curves.push_back(thirdOctave(y.l));
    }
    int duplicates = 0;
    for (size_t i = 0; i < curves.size(); ++i)
        for (size_t j = i + 1; j < curves.size(); ++j) {
            double d = 0.0;
            for (size_t k = 0; k < curves[i].size(); ++k) d += std::fabs(curves[i][k] - curves[j][k]);
            if (d / static_cast<double>(curves[i].size()) < 0.5) ++duplicates;
        }
    check(duplicates == 0, "los 16 tipos tienen color distinto (ningún par con la misma curva)");
    check(rtxx::NoiseBlock::typeCount() == 16, "16 tipos");
    check(std::string(rtxx::NoiseBlock::typeName(11)) == "WHITE", "el índice 11 es WHITE");
}

// --- Compresor de ducking ---------------------------------------------------------

void testCompressor() {
    // Ruido constante en `in`, programa (tono fuerte / flojo) en el sidechain.
    const int n = 22050;
    std::vector<float> noiseSig(static_cast<size_t>(n), 0.1f);
    std::vector<float> prog(static_cast<size_t>(n), 0.0f);
    for (int i = 0; i < n; ++i)
        prog[static_cast<size_t>(i)] = (i < n / 2) ? 0.5f : 0.001f;   // fuerte ⇒ flojo

    auto runComp = [&](rtxx::SimpleCompBlock& blk) {
        std::vector<float> a(static_cast<size_t>(n)), b(static_cast<size_t>(n));
        blk.prepare(rtxx::PrepareContext{44100.0, rtxx::kQuantum});
        for (int start = 0; start < n; start += rtxx::kQuantum) {
            const int m = std::min(rtxx::kQuantum, n - start);
            rtxx::PortData pin, psc, pout;
            pin.numChannels = 2; pin.numSamples = m;
            pin.ch[0] = noiseSig.data() + start; pin.ch[1] = noiseSig.data() + start;
            psc.numChannels = 2; psc.numSamples = m;
            psc.ch[0] = prog.data() + start; psc.ch[1] = prog.data() + start;
            pout.numChannels = 2; pout.numSamples = m;
            pout.ch[0] = a.data() + start; pout.ch[1] = b.data() + start;
            blk.attachPort("inSidechain", &psc);
            blk.process(&pin, pout);
        }
        return a;
    };

    FakeParams off;
    off.v["SComp_Enable"] = 0.0f;
    rtxx::SimpleCompBlock c1(&off);
    const std::vector<float> y1 = runComp(c1);
    // Deshabilitado ⇒ el ruido no pasa. El seco NO se suma aquí (va por su arista).
    near(y1[static_cast<size_t>(n / 4)], 0.0, "SComp_Enable = 0 ⇒ el ruido no pasa", 1e-6);

    FakeParams on;
    on.v["SComp_Enable"] = 1.0f;
    on.v["SComp_Threshold"] = 20.0f;   // Duck = 20 % → umbral −13,5 dBFS
    on.v["SComp_Ratio"] = 0.9f;
    on.v["SComp_Attack"] = 0.0f;
    on.v["SComp_Release"] = 0.2f;
    on.v["SComp_HiPassSidechain"] = 0.0f;   // sin paso-alto: el sidechain es DC + 0
    rtxx::SimpleCompBlock c2(&on);
    const std::vector<float> y2 = runComp(c2);
    // Con programa fuerte el ruido está atenuado y el seco sigue sumado.
    // La salida del compresor es SÓLO el ruido atenuado (el seco va por su arista).
    auto noiseOnly = [&](size_t from, size_t to) {
        std::vector<float> d(y2.begin() + static_cast<long>(from), y2.begin() + static_cast<long>(to));
        return rmsDb(d);
    };
    // Ventanas lejos de las transiciones: el release medido es de 156–245 ms.
    const double quiet = noiseOnly(static_cast<size_t>(n * 0.85), static_cast<size_t>(n));
    const double loud = noiseOnly(static_cast<size_t>(n * 0.35), static_cast<size_t>(n * 0.45));
    std::printf("   ducking (sólo ruido): programa fuerte %.1f dB · flojo %.1f dB\n", loud, quiet);
    check(quiet > loud + 1.0, "con el programa fuerte el ruido baja (ducking)");
    // La salida es SÓLO el ruido atenuado (el seco no se suma aquí).
    // En el tramo flojo la salida vale ≈ el ruido (0,1) y no el seco (0,5): el seco no se suma.
    const double tail = std::fabs(y2[static_cast<size_t>(n * 0.9)]);
    check(tail > 0.05 && tail < 0.2, "la salida es el ruido, no el seco");
}

} // namespace

int main() {
    testPassthrough();
    testSignalIsOwnAndDeterministic();
    testTypesDiffer();
    testCompressor();
    if (failures == 0) {
        std::printf("PASS test_noise\n");
        return 0;
    }
    std::printf("FAIL test_noise (%d comprobaciones)\n", failures);
    return 1;
}
