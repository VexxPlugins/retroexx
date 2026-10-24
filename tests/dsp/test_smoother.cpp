// Test de src/dsp/Smoother.h
//   - OnePole con a = exp(-2*pi*tc/fs) (fórmula observada del retardo del wobble, tc = 32)
//   - convención tau: alcanza el 63.2 % en tc muestras
// - constantes auxiliares (1-a, (1+a)/2) del motor
// - Ramp lineal: llega exacta en N pasos
#include <cmath>

#include "Check.h"
#include "dsp/Smoother.h"

using namespace rtxx::dsp;

int main() {
// --- OnePole: fórmula del motor ---------------------------------------
    {
        const float fs = 44100.0f;
        OnePole s;
        s.prepare(fs);  // tc = 32 por defecto
        CHECK_NEAR(OnePole::kDefaultTimeConstantSamples, 32.0f, 0.0f);

        const float expectA = std::exp(-2.0f * 3.14159265f * 32.0f / fs);
        CHECK_NEAR(s.coefA(), expectA, 1e-7);
        CHECK_NEAR(s.coefOneMinusA(), 1.0f - expectA, 1e-7);   // 0x48c
        CHECK_NEAR(s.coefHalf(), (1.0f + expectA) * 0.5f, 1e-7);  // 0x490 (forma TPT)
        // a 44.1k, 1-a = 4.5485e-3 (tc = 32 muestras -> ~0.73 ms de constante)
        CHECK_NEAR(s.coefOneMinusA(), 4.5485e-3f, 1e-5);

        // respuesta al escalón: y[n] = (1-a)*sum a^k = 1 - a^n
        s.reset(0.0f);
        float y = 0.0f;
        for (int n = 1; n <= 32; ++n) {
            y = s.process(1.0f);
            if (n == 32) {
                CHECK_NEAR(y, 1.0f - std::pow(expectA, 32.0f), 1e-5);
            }
        }
        // converge a 1
        for (int n = 0; n < 20000; ++n) y = s.process(1.0f);
        CHECK_NEAR(y, 1.0f, 1e-4);
        // estable con entrada alternante
        s.reset(0.0f);
        for (int n = 0; n < 10000; ++n) {
            y = s.process((n & 1) ? 1.0f : -1.0f);
            CHECK(std::isfinite(y));
            CHECK(std::fabs(y) <= 1.0f);
        }
    }

    // --- 63.2 % en tc (convención tau) --------------------------------------
    {
        OnePole s;
        s.setTauSamples(32.0f);
        CHECK_NEAR(s.coefA(), std::exp(-1.0f / 32.0f), 1e-7);
        s.reset(0.0f);
        float y = 0.0f;
        for (int n = 0; n < 32; ++n) y = s.process(1.0f);
        CHECK_NEAR(y, 0.632120559f, 1e-4);  // 1 - 1/e

        OnePole s2;
        s2.setTauSamples(220.0f);
        s2.reset(0.0f);
        for (int n = 0; n < 220; ++n) y = s2.process(1.0f);
        CHECK_NEAR(y, 0.632120559f, 1e-4);

// y la fórmula del motor NO da el 63.2 % en tc (documentado; ver DELIVERY.md)
        OnePole s3;
        s3.prepare(44100.0f, 32.0f);
        s3.reset(0.0f);
        for (int n = 0; n < 32; ++n) y = s3.process(1.0f);
        CHECK_LT(y, 0.2f);  // a^32 = exp(-2*pi*32*32/44100) = 0.8642 -> y = 0.1358
        CHECK_NEAR(y, 1.0f - std::exp(-2.0f * 3.14159265f * 32.0f * 32.0f / 44100.0f), 1e-4);
    }

    // --- processTpt: integrador/DC-blocker ----------------------------------
    {
        OnePole s;
        s.setCoefA(0.9f);
        s.reset(0.0f);
        // la suma de las salidas tpt converge al valor de entrada (integrador)
        float acc = 0.0f;
        for (int n = 0; n < 5000; ++n) acc += s.processTpt(1.0f);
        CHECK_NEAR(acc, 1.0f, 1e-3);
        CHECK_NEAR(s.value(), 1.0f, 1e-3);
    }

    // --- Ramp ----------------------------------------------------------------
    {
        Ramp r;
        CHECK(Ramp::kDefaultSteps == 64);
        r.start(0.0f, 64.0f);  // 64 pasos -> +1 por paso
        CHECK_NEAR(r.value(), 0.0f, 0.0);
        for (int k = 0; k < 64; ++k) {
            const float v = r.next();
            CHECK_NEAR(v, static_cast<float>(k), 1e-6);
        }
        CHECK(r.done());
        CHECK_NEAR(r.value(), 64.0f, 0.0);  // llega exacta al objetivo
        CHECK_NEAR(r.next(), 64.0f, 0.0);   // y se queda ahí

        // N arbitrario
        r.start(-2.0f, 3.0f, 10);
        CHECK_NEAR(r.next(), -2.0f, 0.0);
        for (int k = 1; k < 10; ++k) r.next();
        CHECK_NEAR(r.value(), 3.0f, 0.0);

        // N = 1: un solo paso
        r.start(5.0f, 7.0f, 1);
        CHECK(!r.done());
        CHECK_NEAR(r.value(), 5.0f, 0.0);
        r.next();
        CHECK(r.done());
        CHECK_NEAR(r.value(), 7.0f, 0.0);

        // pasos inválidos -> 1
        r.start(1.0f, 2.0f, 0);
        CHECK(r.steps() == 1);
        CHECK_NEAR(r.value(), 1.0f, 0.0);
        r.next();
        CHECK_NEAR(r.value(), 2.0f, 0.0);

        // rampa descendente
        r.start(1.0f, 0.0f, 64);
        for (int k = 0; k < 64; ++k) {
            const float v = r.next();
            CHECK_NEAR(v, 1.0f - static_cast<float>(k) / 64.0f, 1e-6);
        }
        CHECK_NEAR(r.value(), 0.0f, 0.0);
    }

    return check::finish("test_smoother");
}
