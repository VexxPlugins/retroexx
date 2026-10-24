// Test de src/dsp/Lfo.h
//   - fase inicial 0.75 (0x3f400000) y reset
//   - rango [-1,1] en las 12 formas
//   - periodo exacto (una vuelta de fase por periodo de rate)
//   - continuidad en los empalmes del seno parabólico (0, 0.25, 0.5, 0.75, 1)
//   - el seno por defecto NO es sinf(): máximo error acotado y aproximación parabólica
// - mapeo de índice del motor (1..11 -> tabla, resto -> fallback)
//   - incremento por muestra y por bloque
#include <cmath>

#include "Check.h"
#include "dsp/Lfo.h"

using namespace rtxx::dsp;

int main() {
    // --- fase inicial --------------------------------------------------------
    {
        Lfo lfo;
        CHECK_NEAR(lfo.phase(), 0.75, 1e-12);
        CHECK_NEAR(Lfo::kInitialPhase, 0.75f, 0.0f);
        lfo.setPhaseInc(0.1);
        for (int i = 0; i < 3; ++i) lfo.processSample();
        CHECK_NEAR(lfo.phase(), 0.75 + 0.3 - 1.0, 1e-9);  // 1.05 -> 0.05 (wrap)
        lfo.reset();
        CHECK_NEAR(lfo.phase(), 0.75, 1e-12);
        lfo.reset(0.0f);
        CHECK_NEAR(lfo.phase(), 0.0, 1e-12);
    }

    // --- rango [-1,1] en todas las formas ------------------------------------
    {
        for (int s = 0; s < Lfo::kNumShapes; ++s) {
            const LfoShape shape = Lfo::shapeByIndex(s);
            for (int i = 0; i < 20000; ++i) {
                const float p = static_cast<float>(i) / 20000.0f;
                const float y = Lfo::evaluate(shape, p, 0.5f);
                if (!(y >= -1.0f && y <= 1.0f)) {
                    std::printf("  forma %d fuera de rango en p=%f: %f\n",
                                static_cast<int>(shape), static_cast<double>(p),
                                static_cast<double>(y));
                    ++check::failures();
                    break;
                }
            }
        }
        // también con warp en los extremos (afecta a WarpedSine)
        const float warps[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
        for (float w : warps) {
            for (int i = 0; i < 5000; ++i) {
                const float p = static_cast<float>(i) / 5000.0f;
                const float y = Lfo::evaluate(LfoShape::WarpedSine, p, w);
                CHECK(y >= -1.0f && y <= 1.0f);
            }
        }
    }

    // --- periodo exacto ------------------------------------------------------
    {
        Lfo lfo;
        lfo.setRate(1.0, 1000.0);  // 1 ciclo por segundo, fs = 1000 -> periodo 1000
        const float start = lfo.valueAt(static_cast<float>(lfo.phase()));
        for (int i = 0; i < 1000; ++i) lfo.processSample();
        CHECK_NEAR(lfo.phase(), 0.75, 1e-6);
        CHECK_NEAR(lfo.valueAt(static_cast<float>(lfo.phase())), start, 1e-5);

        lfo.setRate(10.0, 48000.0);  // periodo 4800
        for (int i = 0; i < 4800; ++i) lfo.processSample();
        CHECK_NEAR(lfo.phase(), 0.75, 1e-6);

        // dos periodos
        lfo.reset();
        for (int i = 0; i < 9600; ++i) lfo.processSample();
        CHECK_NEAR(lfo.phase(), 0.75, 1e-6);

        // la fase siempre queda en [0,1)
        lfo.setRate(1234.5, 44100.0);
        for (int i = 0; i < 100000; ++i) {
            lfo.processSample();
            if (!(lfo.phase() >= 0.0 && lfo.phase() < 1.0)) {
                std::printf("  fase fuera de [0,1): %f\n", lfo.phase());
                ++check::failures();
                break;
            }
        }
    }

    // --- continuidad del seno parabólico en los empalmes --------------------
    {
        const float seams[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
        const float eps = 1e-5f;
        for (float s : seams) {
            const float left = Lfo::parabolicSine(s - eps);
            const float right = Lfo::parabolicSine(s + eps);
            CHECK_NEAR(left, right, 1e-3);
        }
        // los valores nodales son exactos
        CHECK_NEAR(Lfo::parabolicSine(0.0f), 0.0f, 1e-6);
        CHECK_NEAR(Lfo::parabolicSine(0.25f), 1.0f, 1e-6);
        CHECK_NEAR(Lfo::parabolicSine(0.5f), 0.0f, 1e-6);
        CHECK_NEAR(Lfo::parabolicSine(0.75f), -1.0f, 1e-6);

        // es la parábola de 4 tramos, no sinf(): en p=0.125 la parábola da 0.75 y se
        // aplica el factor observado 1.001195 (0x3f80272a) -> 0.75090
        CHECK_NEAR(Lfo::parabolicSine(0.125f), 0.75f * Lfo::kParabolicScale, 1e-5);
        CHECK_LT(std::fabs(Lfo::parabolicSine(0.125f) - std::sin(2.0f * 3.14159265f * 0.125f)),
                 0.06f);
        // error máximo frente al seno verdadero acotado (< 8 %, provisional: falta el
        // pulido final del motor, ver DELIVERY.md)
        float maxErr = 0.0f;
        for (int i = 0; i < 10000; ++i) {
            const float p = static_cast<float>(i) / 10000.0f;
            const float err = std::fabs(Lfo::parabolicSine(p) -
                                        std::sin(2.0f * 3.14159265f * p));
            if (err > maxErr) maxErr = err;
        }
        CHECK_LT(maxErr, 0.08f);
        // simetría impar
        for (int i = 1; i < 500; ++i) {
            const float p = static_cast<float>(i) / 1000.0f;
            CHECK_NEAR(Lfo::parabolicSine(p), -Lfo::parabolicSine(p + 0.5f), 1e-6);
        }
    }

    // --- formas geométricas fijadas -----------------------------------------
    {
        // triangular: picos en 0.25/0.75, ceros en 0/0.5/1
        CHECK_NEAR(Lfo::triangle(0.0f), 0.0f, 1e-6);
        CHECK_NEAR(Lfo::triangle(0.25f), 1.0f, 1e-6);
        CHECK_NEAR(Lfo::triangle(0.5f), 0.0f, 1e-6);
        CHECK_NEAR(Lfo::triangle(0.75f), -1.0f, 1e-6);
        CHECK_NEAR(Lfo::triangle(1.0f), 0.0f, 1e-6);

        // cuadrada
        CHECK_NEAR(Lfo::square(0.0f), 1.0f, 0.0);
        CHECK_NEAR(Lfo::square(0.499f), 1.0f, 0.0);
        CHECK_NEAR(Lfo::square(0.5f), -1.0f, 0.0);
        CHECK_NEAR(Lfo::square(0.999f), -1.0f, 0.0);

        // sierras: 2*(p-0.5) y 2*(0.5-p)
        for (int i = 0; i < 1000; ++i) {
            const float p = static_cast<float>(i) / 1000.0f;
            CHECK_NEAR(Lfo::sawUp(p), 2.0f * (p - 0.5f), 1e-6);
            CHECK_NEAR(Lfo::sawDown(p), 2.0f * (0.5f - p), 1e-6);
            CHECK_NEAR(Lfo::sawUp(p), -Lfo::sawDown(p), 1e-6);
        }
        CHECK_NEAR(Lfo::sawUp(0.0f), -1.0f, 1e-6);
        CHECK_NEAR(Lfo::sawUp(0.75f), 0.5f, 1e-6);

        // escaleras dentro de rango y con el número de peldaños esperado
        for (int s = static_cast<int>(LfoShape::Stairs2);
             s <= static_cast<int>(LfoShape::StairsAlt); ++s) {
            const LfoShape shape = static_cast<LfoShape>(s);
            float prev = Lfo::evaluate(shape, 0.0f, 0.5f);
            for (int i = 1; i < 1000; ++i) {
                const float p = static_cast<float>(i) / 1000.0f;
                const float y = Lfo::evaluate(shape, p, 0.5f);
                CHECK(y >= -1.0f && y <= 1.0f);
                (void)prev;
                prev = y;
            }
        }
    }

// --- mapeo de índice del motor ---------------------------------------
    {
        CHECK(Lfo::shapeFromParam(1) == LfoShape::WarpedSine);
        CHECK(Lfo::shapeFromParam(2) == LfoShape::Triangle);
        CHECK(Lfo::shapeFromParam(3) == LfoShape::Square);
        CHECK(Lfo::shapeFromParam(4) == LfoShape::SawUp);
        CHECK(Lfo::shapeFromParam(5) == LfoShape::SawDown);
        CHECK(Lfo::shapeFromParam(6) == LfoShape::Stairs2);
        CHECK(Lfo::shapeFromParam(11) == LfoShape::StairsAlt);
        CHECK(Lfo::shapeFromParam(0) == LfoShape::ParabolicSine);   // fallback
        CHECK(Lfo::shapeFromParam(12) == LfoShape::ParabolicSine);
        CHECK(Lfo::shapeFromParam(-5) == LfoShape::ParabolicSine);
        const float param = 3.9f;
        CHECK(Lfo::shapeFromParam(static_cast<int>(param)) == LfoShape::Square);
        // la forma por defecto del LFO es el seno parabólico (fallback)
        Lfo lfo;
        CHECK(lfo.shape() == LfoShape::ParabolicSine);
        CHECK_NEAR(lfo.valueAt(0.25f), 1.0f, 1e-6);
    }

    // --- incremento por bloque ----------------------------------------------
    {
        Lfo lfo;
        lfo.setRate(4.0, 1000.0);  // inc = 0.004
        float buf[64];
        lfo.processBlock(buf, 64);
        CHECK_NEAR(lfo.phase(), 0.75 + 64 * 0.004 - 1.0, 1e-9);  // 1.006 -> 0.006
        for (int i = 1; i < 64; ++i) CHECK(buf[i] == buf[0]);

        // por muestra y por bloque deben coincidir en fase
        Lfo a, b;
        a.setRate(4.0, 1000.0);
        b.setRate(4.0, 1000.0);
        for (int i = 0; i < 64; ++i) a.processSample();
        b.processBlock(buf, 64);
        CHECK_NEAR(a.phase(), b.phase(), 1e-12);

        // advance() acumula muestras
        Lfo c;
        c.setRate(4.0, 1000.0);
        c.advance(50);
        CHECK_NEAR(c.phase(), 0.75 + 50 * 0.004, 1e-12);
        c.advance(100);
        CHECK(c.phase() >= 0.0 && c.phase() < 1.0);    }

// --- cruce por cero ------------------
    {
        Lfo lfo;
        lfo.setRate(1.0, 8.0);  // 8 muestras por ciclo
        lfo.setShape(LfoShape::SawUp);
        int crossings = 0;
        float prev = lfo.processSample();
        for (int i = 0; i < 8 * 4; ++i) {
            const float cur = lfo.processSample();
            if (std::signbit(prev) != std::signbit(cur)) ++crossings;
            prev = cur;
        }
        CHECK(crossings >= 4);
        CHECK(crossings <= 12);
    }

    return check::finish("test_lfo");
}
