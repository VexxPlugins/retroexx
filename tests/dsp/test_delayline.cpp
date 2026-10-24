// Test de src/dsp/DelayLine.h
// - ring de 16384 con máscara 0x3FFF
//   - retardo entero: la señal sale idéntica (error < 1e-7)
//   - retardo 0.5: Catmull-Rom exacta para una rampa
//   - wrap-around del ring
//   - readAuto conmuta en d = 1 (Catmull-Rom si d >= 1, lineal si d < 1)
//   - coeficientes -5, 4, 3, 0.5 del Hermite
#include <cmath>

#include "Check.h"
#include "dsp/DelayLine.h"

using namespace rtxx::dsp;

int main() {
// --- dimensionado del motor -------------------------------------------
    CHECK(DelayLine::kSize == 16384);
    CHECK(DelayLine::kMask == 0x3FFF);
    CHECK((DelayLine::kSize & (DelayLine::kSize - 1)) == 0);  // potencia de 2

    // --- retardo entero: salida idéntica -------------------------------------
    {
        DelayLine dl;
        float written[300];
        for (int i = 0; i < 300; ++i) {
            const float x = std::sin(0.05f * static_cast<float>(i));
            written[i] = x;
            dl.write(x);
            // retardo entero d -> written[i-d]
            for (int d = 0; d <= 10; ++d) {
                const float expect = (i - d >= 0) ? written[i - d] : 0.0f;
                CHECK_NEAR(dl.readAt(d), expect, 0.0);
                CHECK_NEAR(dl.readLinear(static_cast<float>(d)), expect, 1e-7);
                CHECK_NEAR(dl.readCatmullRom(static_cast<float>(d)), expect, 1e-7);
                CHECK_NEAR(dl.readAuto(static_cast<float>(d)), expect, 1e-7);
            }
        }
    }

    // --- Catmull-Rom exacta para una rampa y para una parábola ---------------
    {
        DelayLine dl;
        for (int i = 0; i < 200; ++i) dl.write(static_cast<float>(i));
        // en el instante actual, buf[widx - d] = 199 - d
        for (float d = 1.0f; d < 40.0f; d += 0.25f) {
            const float expect = 199.0f - d;
            CHECK_NEAR(dl.readCatmullRom(d), expect, 1e-5);
            CHECK_NEAR(dl.readLinear(d), expect, 1e-5);
        }
        // el caso explícito 0.5
        CHECK_NEAR(dl.readCatmullRom(5.5f), 199.0f - 5.5f, 1e-6);
        CHECK_NEAR(dl.readCatmullRom(20.5f), 199.0f - 20.5f, 1e-6);
    }

    // --- coeficientes -5, 4, 3, 0.5: verificación numérica del polinomio -----
    {
        DelayLine dl;
        // señal cuadrática: y[n] = n^2. El Hermite de Catmull-Rom reproduce polinomios de
        // grado <= 3, así que debe dar EXACTAMENTE (199-d)^2 (el lineal no).
        for (int i = 0; i < 200; ++i) dl.write(static_cast<float>(i) * static_cast<float>(i));

        const float ds[] = {3.25f, 5.5f, 7.75f, 12.5f, 30.5f};
        for (float d : ds) {
            const float expect = (199.0f - d) * (199.0f - d);
            CHECK_NEAR(dl.readCatmullRom(d), expect, 0.05);
            CHECK(std::fabs(dl.readLinear(d) - expect) > 0.01f);  // el lineal se desvía
        }

// el polinomio del motor, evaluado a mano, coincide término a término
        const float d = 7.5f;
        const int di = static_cast<int>(d);
        const float t = d - static_cast<float>(di);
        const float y1 = dl.readAt(di);      // (199-di)^2
        const float y2 = dl.readAt(di - 1);
        const float y0 = dl.readAt(di + 1);
        const float ym = dl.readAt(di + 2);
        const float A = 2.0f * y2 - 5.0f * y1 + 4.0f * y0 - ym;  // -5, 4
        const float B = 3.0f * y1 + ym - 3.0f * y0 - y2;        // 3
        const float expect = y1 + 0.5f * t * ((y0 - y2) + t * (A + t * B));  // 0.5
        CHECK_NEAR(dl.readCatmullRom(d), expect, 1e-2);
    }

    // --- wrap-around ---------------------------------------------------------
    {
        DelayLine dl;
        const int total = DelayLine::kSize * 2 + 1234;
        // se guarda sólo la cola necesaria para comparar
        for (int i = 0; i < total; ++i) {
            dl.write(static_cast<float>(i));
            if (i >= total - 1) {
                CHECK_NEAR(dl.readAt(0), static_cast<float>(i), 0.0);
                CHECK_NEAR(dl.readAt(1), static_cast<float>(i - 1), 0.0);
                CHECK_NEAR(dl.readAt(DelayLine::kSize - 1), static_cast<float>(i - (DelayLine::kSize - 1)), 0.0);
                CHECK_NEAR(dl.readLinear(100.5f), static_cast<float>(i) - 100.5f, 1e-3);
            }
        }
        // lectura del extremo del ring
        CHECK(dl.writeIndex() == total % DelayLine::kSize);
    }

    // --- readAuto conmuta en d = 1 -------------------------------------------
    {
        DelayLine dl;
        for (int i = 0; i < 64; ++i) dl.write(std::sin(0.7f * static_cast<float>(i)));
        const float ds[] = {0.0f, 0.25f, 0.999f, 1.0f, 1.0001f, 2.0f, 3.75f, 17.5f};
        for (float d : ds) {
            const float autoV = dl.readAuto(d);
            const float ref = (d >= 1.0f) ? dl.readCatmullRom(d) : dl.readLinear(d);
            CHECK(autoV == ref);
        }
        // justo en el umbral: d = 1 usa Catmull-Rom (que en d entero == muestra exacta)
        CHECK(dl.readAuto(1.0f) == dl.readCatmullRom(1.0f));
        CHECK(dl.readAuto(0.999f) == dl.readLinear(0.999f));
        CHECK(dl.readAuto(0.999f) != dl.readCatmullRom(0.999f));
    }

    // --- clear() -------------------------------------------------------------
    {
        DelayLine dl;
        for (int i = 0; i < 100; ++i) dl.write(1.0f);
        dl.clear();
        CHECK(dl.readAt(0) == 0.0f);
        CHECK(dl.readAt(99) == 0.0f);
        CHECK(dl.writeIndex() == 0);
    }

    return check::finish("test_delayline");
}
