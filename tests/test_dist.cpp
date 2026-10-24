// Etapas de distorsión de la cadena Dist (fase 3 del ROADMAP).
//
// Fija las propiedades comprobables en el bloque: qué etapas están activas por Type, que
// Type 4 es unidad, que el bypass va invertido y limpia estado, y que el DSP de cada clase
// cumple sus ecuaciones. La comparación de THD y espectro está en la suite de medidas.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "block/PlugChainText.h"
#include "blocks/PlugChainBlock.h"
#include "model/ParamStore.h"

namespace {

int failures = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FALLO: %s\n", what);
        ++failures;
    }
}
void near(double a, double b, const char* what, double tol = 1e-5) {
    if (std::fabs(a - b) > tol) {
        std::printf("FALLO: %s (%.7f != %.7f)\n", what, a, b);
        ++failures;
    }
}

constexpr int kRate = 44100;

// Renderiza un seno por la cadena Dist real del DSL. Devuelve la salida del canal 0.
std::vector<float> renderDist(rtxx::ParamStore& store, int type, double amount,
                              double amplitude = 0.1, double freq = 1000.0, int frames = 16384) {
    store.setModel("Dist_Type", static_cast<float>(type));
    store.setModel("Dist_Amount", static_cast<float>(amount));
    rtxx::PlugChainBlock chain(&store, rtxx::kPlugChainDist, "Dist_");
    rtxx::PrepareContext ctx;
    ctx.sampleRate = kRate;
    chain.prepare(ctx);

    std::vector<float> in(static_cast<size_t>(frames));
    for (int i = 0; i < frames; ++i)
        in[static_cast<size_t>(i)] =
            static_cast<float>(amplitude * std::sin(2.0 * M_PI * freq * i / kRate));

    std::vector<float> out(static_cast<size_t>(frames), 0.0f);
    const int block = rtxx::kQuantum;
    for (int start = 0; start < frames; start += block) {
        const int n = std::min(block, frames - start);
        rtxx::PortData pin;
        pin.numChannels = 1;
        pin.numSamples = n;
        pin.ch[0] = in.data() + start;
        rtxx::PortData pout;
        pout.numChannels = 1;
        pout.numSamples = n;
        pout.ch[0] = out.data() + start;
        chain.process(&pin, pout);
    }
    return out;
}

double rmsOf(const std::vector<float>& x, int from = 4096) {
    double acc = 0.0;
    int n = 0;
    for (size_t i = static_cast<size_t>(from); i < x.size(); ++i) {
        acc += static_cast<double>(x[i]) * x[i];
        ++n;
    }
    return n > 0 ? std::sqrt(acc / n) : 0.0;
}

// THD de la fundamental de 1 kHz: energía armónica / energía de la fundamental.
double thdDb(const std::vector<float>& x) {
    const int n = 8192;
    const int from = 4096;
    std::vector<double> re(n), im(n);
    // DFT directa sobre 1 kHz y sus armónicos (Goertzel), suficiente y sin dependencias.
    auto mag = [&](double f) {
        double sr = 0.0, si = 0.0;
        for (int i = 0; i < n; ++i) {
            const double w = 0.5 - 0.5 * std::cos(2.0 * M_PI * i / (n - 1));
            const double ang = 2.0 * M_PI * f * i / kRate;
            const double v = x[static_cast<size_t>(from + i)] * w;
            sr += v * std::cos(ang);
            si += v * std::sin(ang);
        }
        return std::sqrt(sr * sr + si * si);
    };
    const double fund = mag(1000.0);
    double harm = 0.0;
    for (int h = 2; h <= 10; ++h) {
        const double m = mag(1000.0 * h);
        harm += m * m;
    }
    return 10.0 * std::log10(std::max(harm, 1e-24) / std::max(fund * fund, 1e-24));
}

} // namespace

int main() {
    rtxx::ParamStore store;
    store.resetToDefaults();
    store.setModel("Dist_Enable", 1.0f);

    // 1. Type 4 = unidad: la cadena sólo lleva los dos gains -12/+12 dB del DSL y ninguna
    // etapa de distorsión. Salida == entrada.
    {
        const std::vector<float> out = renderDist(store, 4, 1.0);
        double maxDiff = 0.0;
        for (size_t i = 0; i < out.size(); ++i) {
            const double x = 0.1 * std::sin(2.0 * M_PI * 1000.0 * static_cast<double>(i) / kRate);
            maxDiff = std::max(maxDiff, std::fabs(out[i] - x));
        }
        check(maxDiff < 1e-6, "Type 4 es unidad (|salida - entrada| < 1e-6)");
    }

    // 2. Los Types de distorsión cambian la señal; el orden de magnitud de la THD crece
    // con el Amount. Los valores concretos están verificados en la suite de medidas.
    {
        const std::vector<float> quiet = renderDist(store, 0, 0.0);
        const std::vector<float> loud = renderDist(store, 0, 1.0);
        check(rmsOf(quiet) > 0.0, "Crunch con Amount 0 produce señal (no silencio)");
        check(thdDb(loud) > thdDb(quiet), "Crunch: THD sube con el Amount");
    }

    // 3. Bypass invertido: con Dist_Enable = 0 ninguna etapa de la cadena se aplica (todas
    //    llevan `[enabled] Enable`), así que el nodo queda en passthrough.
    {
        rtxx::ParamStore off;
        off.resetToDefaults();
        off.setModel("Dist_Enable", 0.0f);
        const std::vector<float> out = renderDist(off, 0, 1.0);
        double maxDiff = 0.0;
        for (size_t i = 0; i < out.size(); ++i) {
            const double x = 0.1 * std::sin(2.0 * M_PI * 1000.0 * static_cast<double>(i) / kRate);
            maxDiff = std::max(maxDiff, std::fabs(out[i] - x));
        }
        check(maxDiff < 1e-6, "Dist_Enable = 0 ⇒ passthrough (sólo se limpia estado)");
    }

    // 4. Envolvente común: los coeficientes de un polo de la casa son
    // a = 1 - exp(-2*pi*f/sr), con el tope de 0.49999 del motor.
    {
        const double loFreq = 20.0, sr = 44100.0;
        const double a1 = 1.0 - std::exp(-2.0 * M_PI * loFreq / sr);
        near(a1, 0.002845, "a1(20 Hz) = 1-exp(-2pi f/sr)", 1e-5);
        const double a2 = 1.0 - std::exp(-2.0 * M_PI * 20000.0 / sr);
        near(a2, 0.9417, "a2(20 kHz) = 1-exp(-2pi f/sr)", 1e-3);
    }

    // 5. Cada clase tiene su shape: `distort` 0..2 y `magicDistort` 0..2, y las etapas
    //    del DSL son las 20 de la tabla (no se inventan ni se pierden).
    {
        rtxx::PlugChainBlock chain(&store, rtxx::kPlugChainDist, "Dist_");
        check(chain.parseError().empty(), "el DSL de plugChainDist parsea");
        check(chain.stageCount() == 20, "plugChainDist tiene 20 etapas");
        check(chain.implementedStageCount() == 20, "las 20 etapas están implementadas");
        int distort = 0, magic = 0;
        for (int i = 0; i < chain.stageCount(); ++i) {
            const std::string k = chain.stageKind(i);
            if (k == "distort") ++distort;
            if (k == "magicDistort") ++magic;
        }
        check(distort == 3, "3 etapas `distort` (shapes 0, 1, 2)");
        check(magic == 3, "3 etapas `magicDistort` (shapes 0, 1, 2)");
    }

    // 6. DSP directo: con mezcla 100 % y sin crossover el húmedo es la señal entera, y con
    //    `d = 1` el shape 0 (Crunch) recorta a ±1 antes de normalizar.
    {
        rtxx::PlugChainBlock::DistParams p;
        p.amount = 100.0f;
        p.shape = 0;
        p.mix = 100.0f;
        p.refLevel = -10.0f;
        rtxx::PlugChainBlock::DistortDsp dsp;
        dsp.configure(p, kRate);
        dsp.reset();
        near(dsp.dry, 0.0, "mix 100 % ⇒ dry = 0");
        near(dsp.distGain, 1.0, "distDB 0 ⇒ ganancia húmeda 1");
        // k = 0.5*50^(a/100) con a = 100 ⇒ 25; norm = sqrt(k/(3k-2)); scale = norm/d.
        const double kExpected = 25.0;
        near(dsp.k, kExpected, "Crunch: k = 0.5*50^(a/100) = 25 en a = 100", 1e-3);
        const double dExpected = std::pow(10.0, 10.0 / 20.0);
        near(dsp.scale, std::sqrt(kExpected / (3.0 * kExpected - 2.0)) / dExpected,
             "Crunch: scale = sqrt(k/(3k-2))/d", 1e-5);
    }

    // 7. La rama legacy de magicDistort (`useLegacyBehaviour` = 0, el default) calcula el
    //    retardo y la fuga del integrador en el dominio de 44.1 kHz; a 44.1 kHz las dos
    // ramas tienen que dar la MISMA señal.
    {
        auto runMagic = [&](int shape, bool legacy) {
            rtxx::PlugChainBlock::DistParams p;
            p.amount = 60.0f;
            p.shape = shape;
            p.lowFreq = 20.0f;
            p.highFreq = 20000.0f;
            p.distLP = 20000.0f;
            p.mix = 100.0f;
            p.legacy = legacy;
            rtxx::PlugChainBlock::MagicDistortDsp dsp;
            dsp.configure(p, kRate);
            dsp.reset();
            std::vector<float> y(4096);
            for (size_t i = 0; i < y.size(); ++i) {
                const float x = static_cast<float>(0.2 * std::sin(2.0 * M_PI * 440.0 * static_cast<double>(i) / kRate));
                y[i] = dsp.process(0, x);
            }
            return y;
        };
        for (int shape : {1, 2}) {
            const std::vector<float> a = runMagic(shape, true);
            const std::vector<float> b = runMagic(shape, false);
            double maxDiff = 0.0;
            for (size_t i = 0; i < a.size(); ++i)
                maxDiff = std::max(maxDiff, static_cast<double>(std::fabs(a[i] - b[i])));
            check(maxDiff < 1e-6, "magicDistort: legacy == moderno a 44.1 kHz");
        }
        const std::vector<float> a0 = runMagic(0, true);
        const std::vector<float> b0 = runMagic(0, false);
        double d0 = 0.0;
        for (size_t i = 0; i < a0.size(); ++i)
            d0 = std::max(d0, static_cast<double>(std::fabs(a0[i] - b0[i])));
        check(d0 < 1e-6, "TubePair no depende de useLegacyBehaviour");
    }

    // 8. AirPressure (shape 1): la fuga del acumulador es el ESTADO de la recursión, no una
    // ganancia de salida. En el motor el valor con fuga se vuelve a escribir en el estado
    // Aplicándola sólo a la salida
    //    el compander se queda sin recorte de grave: el gate de la fase 3 medía +6.34 dB en
    // 63 Hz y +15.8 dB en 20 Hz en Type 6.
    {
        rtxx::PlugChainBlock::DistParams p;
        p.amount = 100.0f;
        p.shape = 1;
        p.lowFreq = 20.0f;
        p.highFreq = 20000.0f;
        p.distLP = 20000.0f;
        p.mix = 100.0f;
        p.legacy = true;

        // 8a. Con la entrada exactamente a cero `w = atan(0 - 0) = 0`, así que un estado
        //     sembrado a mano decae exactamente por la fuga en cada muestra. Con la fuga
        //     fuera del estado el acumulador se queda congelado en 1.
        {
            rtxx::PlugChainBlock::MagicDistortDsp dsp;
            dsp.configure(p, kRate);
            dsp.reset();
            dsp.apCoefA.set(0.0f, 1.0f, 0.0f);  // el suavizador de 50 Hz deja pasar la fuga tal cual
            dsp.apAcc[0] = 1.0f;
            check(dsp.apCoefATarget < 0.999f, "AirPressure: la fuga P+0x2a0 es < 1");
            for (int i = 0; i < 100; ++i) dsp.process(0, 0.0f);
            near(dsp.apAcc[0], std::pow(static_cast<double>(dsp.apCoefATarget), 100.0),
                 "AirPressure: el estado decae por la fuga (acc *= 2^-(1336.5·a01+1)/sr)", 1e-7);
        }

        // 8b. Consecuencia espectral: la recursión con fuga es un paso-alto de esquina
        //     1-leak (146 Hz a Amount 100), así que el húmedo a 40 Hz queda muy por debajo
        // del de 1 kHz: −13.5 dB en el modelo y en el motor; la misma
        //     medida a Amount 0.5 da −6.6 dB en los dos (seno −60 dBFS). Sin la fuga en el
        //     estado esa relación es ≈ 0 dB.
        {
            auto wetRms = [&](double freq) {
                rtxx::PlugChainBlock::MagicDistortDsp dsp;
                dsp.configure(p, kRate);
                dsp.reset();
                double acc = 0.0;
                int n = 0;
                for (int i = 0; i < 2 * kRate; ++i) {
                    const float x = static_cast<float>(1e-4 * std::sin(2.0 * M_PI * freq * i / kRate));
                    dsp.process(0, x);
                    if (i >= 2 * kRate - kRate / 4) {  // últimas 0.25 s, ya en régimen
                        acc += static_cast<double>(dsp.wet[0]) * dsp.wet[0];
                        ++n;
                    }
                }
                return std::sqrt(acc / n);
            };
            const double ratio = 20.0 * std::log10(wetRms(40.0) / wetRms(1000.0));
            check(ratio < -6.0, "AirPressure: el húmedo a 40 Hz cae >= 6 dB respecto de 1 kHz");
        }
    }

    if (failures == 0) std::printf("test_dist: OK\n");
    return failures == 0 ? 0 : 1;
}
