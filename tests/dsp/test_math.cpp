// Test de src/dsp/Math.h
//   - quantizeParam == round(v*16384)/16384, resolución 2^-14, idempotente
//   - clamp / lerp
//   - dbToGain / gainToDb recíprocos
#include <cmath>

#include "Check.h"
#include "dsp/Math.h"

using namespace rtxx::dsp;

int main() {
    // --- quantizeParam: implementación exacta del contrato ---------------------
    CHECK_NEAR(kParamQuantScale, 6.103515625e-05, 1e-18);  // 0x38800000
    CHECK_NEAR(kParamQuantSteps, 16384.0f, 0.0f);

    const float samples[] = {0.0f, 1.0f, -1.0f, 0.5f, 0.30001f, 0.1234567f,
                             -0.7777777f, 2.5f, -3.14159f};
    for (float v : samples) {
        const float expect = std::round(v * 16384.0f) / 16384.0f;
        CHECK(quantizeParam(v) == expect);
        CHECK_NEAR(quantizeParam(v), v, 1.0f / 32768.0f);       // error <= 2^-15
        CHECK(quantizeParam(quantizeParam(v)) == quantizeParam(v));  // idempotente
        // el resultado es múltiplo exacto de 2^-14
        const float scaled = quantizeParam(v) * 16384.0f;
        CHECK_NEAR(scaled, std::round(scaled), 1e-3);
    }

    // La resolución es exactamente 2^-14: el escalón más pequeño cambia el valor.
    const float a = quantizeParam(0.4f);
    const float b = quantizeParam(0.4f + 1.0f / 16384.0f);
    CHECK_NEAR(b - a, 1.0f / 16384.0f, 1e-7);

    // --- clamp ----------------------------------------------------------------
    CHECK(clamp(5.0f, 0.0f, 1.0f) == 1.0f);
    CHECK(clamp(-5.0f, 0.0f, 1.0f) == 0.0f);
    CHECK(clamp(0.25f, 0.0f, 1.0f) == 0.25f);
    CHECK(clamp01(2.0f) == 1.0f);
    CHECK(clamp(7, 0, 10) == 7);
    CHECK(clamp(-7, 0, 10) == 0);
    CHECK(clamp(2.5, 0.0, 1.0) == 1.0);

    // --- lerp -----------------------------------------------------------------
    CHECK_NEAR(lerp(0.0f, 10.0f, 0.0f), 0.0f, 0.0);
    CHECK_NEAR(lerp(0.0f, 10.0f, 1.0f), 10.0f, 0.0);
    CHECK_NEAR(lerp(0.0f, 10.0f, 0.25f), 2.5f, 1e-6);
    CHECK_NEAR(lerp(-2.0, 2.0, 0.5), 0.0, 1e-12);

    // --- dB <-> ganancia ------------------------------------------------------
    CHECK_NEAR(dbToGain(0.0f), 1.0f, 1e-7);
    CHECK_NEAR(dbToGain(6.0205999f), 2.0f, 1e-6);
    CHECK_NEAR(dbToGain(-6.0205999f), 0.5f, 1e-6);
    CHECK_NEAR(dbToGain(-12.0f), 0.2511886f, 1e-6);   // = 10^(-12/20), constante del informe
    CHECK_NEAR(dbToGain(-9.0f), 0.3548134f, 1e-6);    // = 10^(-9/20)
    CHECK_NEAR(gainToDb(1.0f), 0.0f, 1e-6);
    CHECK_NEAR(gainToDb(2.0f), 6.0205999f, 1e-5);
    for (float db = -40.0f; db <= 40.0f; db += 3.5f) {
        CHECK_NEAR(gainToDb(dbToGain(db)), db, 1e-4);
    }
    // rb20Amplitude = 10^(dB/40): la "A" de las fórmulas RBJ del motor
    CHECK_NEAR(rb20Amplitude(12.0), std::pow(10.0, 0.3), 1e-12);
    CHECK_NEAR(rb20Amplitude(0.0), 1.0, 1e-12);

    return check::finish("test_math");
}
