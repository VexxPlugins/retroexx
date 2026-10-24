// Test de src/dsp/Oversampling.h
//   - chooseFactor: argmin de |192000 - N*fs| sobre el conjunto permitido
//   - no está hardcodeado: 1x/2x/4x/8x y 2x/4x
//   - FIR de media banda: suma 1, simetría, taps pares nulos (half-band)
//   - roundtrip subida/bajada: DC exacto y seno de 1 kHz con error < 1 %
//   - factor 1 = paso directo
#include <algorithm>
#include <cmath>
#include <vector>

#include "Check.h"
#include "dsp/Oversampling.h"

using namespace rtxx::dsp;

namespace {

double rmsError(const std::vector<float>& a, const std::vector<float>& b, int from, int to) {
    double acc = 0.0;
    int n = 0;
    for (int i = from; i < to; ++i) {
        const double d = static_cast<double>(a[i]) - static_cast<double>(b[i]);
        acc += d * d;
        ++n;
    }
    return n > 0 ? std::sqrt(acc / n) : 0.0;
}

}  // namespace

int main() {
    // --- regla de elección del factor ---------------------------------------
    {
    // objetivo 192 kHz
        CHECK_NEAR(kOversamplingTargetFs, 192000.0, 0.0);
        CHECK(kFactorsTube[0] == 1 && kFactorsTube[1] == 2 && kFactorsTube[2] == 4 &&
              kFactorsTube[3] == 8);
        CHECK(kFactorsSidechain[0] == 2 && kFactorsSidechain[1] == 4);

        struct Case {
            double fs;
            int tube;
            int side;
        };
        const Case cases[] = {
            {22050.0, 8, 4},    // |192000-8*22050| = 15600
            {32000.0, 4, 4},    // 4*32000 y 8*32000 empatan a 64000 -> gana el menor
            {44100.0, 4, 4},    // 4*44100 = 176400 -> 15600
            {48000.0, 4, 4},    // 192000 exacto
            {88200.0, 2, 2},    // 2*88200 = 176400 -> 15600
            {96000.0, 2, 2},    // 192000 exacto
            {176400.0, 1, 2},   // 1x exacto; sidechain no admite 1x -> 2x (15600)
            {192000.0, 1, 2},   // 1x exacto; sidechain -> 2x
        };
        for (const Case& c : cases) {
            const int tube = chooseFactor(c.fs, kFactorsTube);
            const int side = chooseFactor(c.fs, kFactorsSidechain);
            if (tube != c.tube) {
                std::printf("  chooseFactor(%.0f, tube) = %d, esperado %d\n", c.fs, tube, c.tube);
                ++check::failures();
            }
            if (side != c.side) {
                std::printf("  chooseFactor(%.0f, side) = %d, esperado %d\n", c.fs, side,
                            c.side);
                ++check::failures();
            }
        }

        // coincide con el argmin exhaustivo en todo el rango útil de fs
        for (double fs = 22050.0; fs <= 192000.0; fs += 137.0) {
            int best = 0;
            double bestD = 1e30;
            for (int n : kFactorsTube) {
                const double d = std::fabs(192000.0 - static_cast<double>(n) * fs);
                if (d < bestD) {
                    bestD = d;
                    best = n;
                }
            }
            CHECK(chooseFactor(fs, kFactorsTube) == best);
        }

        // los conjuntos por puntero y por array coinciden; casos degenerados -> 1
        CHECK(chooseFactor(48000.0, kFactorsTube, 4) == 4);
        CHECK(chooseFactor(48000.0, nullptr, 4) == 1);
        CHECK(chooseFactor(48000.0, kFactorsTube, 0) == 1);
        CHECK(chooseFactor(0.0, kFactorsTube, 4) == 1);
    }

    // --- FIR de media banda --------------------------------------------------
    {
        const std::vector<float>& h = Oversampling::coefficients();
        CHECK(h.size() == static_cast<size_t>(Oversampling::kTaps));
        CHECK(Oversampling::kTaps == 65);
        CHECK(Oversampling::kGroupDelay == 32);
        double sum = 0.0;
        for (float v : h) sum += v;
        CHECK_NEAR(sum, 1.0, 1e-6);
        // simetría (fase lineal)
        for (int i = 0; i < Oversampling::kTaps; ++i) {
            CHECK_NEAR(h[static_cast<size_t>(i)],
                       h[static_cast<size_t>(Oversampling::kTaps - 1 - i)], 1e-7);
        }
        // half-band: taps pares (excepto el centro) nulos
        for (int i = 0; i < Oversampling::kTaps; ++i) {
            const int k = i - Oversampling::kGroupDelay;
            if (k != 0 && (k % 2) == 0) CHECK_NEAR(h[static_cast<size_t>(i)], 0.0, 1e-7);
        }
        CHECK_NEAR(h[static_cast<size_t>(Oversampling::kGroupDelay)], 0.5, 1e-3);
    }

    // --- factor 1: paso directo ---------------------------------------------
    {
        Oversampling os;
        os.prepareWithFactor(1, 64, 192000.0, 1);
        CHECK(os.factor() == 1);
        CHECK(os.stages() == 0);
        CHECK(os.latencySamples() == 0);
        float in[64], out[64];
        for (int i = 0; i < 64; ++i) in[i] = std::sin(0.1f * static_cast<float>(i));
        const float* up = os.upsample(0, in, 64);
        CHECK(up != nullptr);
        for (int i = 0; i < 64; ++i) CHECK(up[i] == in[i]);
        os.downsample(0, up, 64, out);
        for (int i = 0; i < 64; ++i) CHECK(out[i] == in[i]);
    }

    // --- DC: la ida y vuelta conserva el nivel exacto ------------------------
    {
        for (int factor : kFactorsTube) {
            Oversampling os;
            os.prepareWithFactor(1, 64, 48000.0, factor);
            CHECK(os.factor() == factor);
            CHECK(os.internalRate() == 48000.0 * factor);

            float in[64], out[64];
            for (int i = 0; i < 64; ++i) in[i] = 0.5f;
            float last = 0.0f;
            for (int blk = 0; blk < 200; ++blk) {
                const float* up = os.upsample(0, in, 64);
                os.downsample(0, up, 64, out);
                last = out[63];
            }
            CHECK_NEAR(last, 0.5, 1e-4);
        }
    }

    // --- roundtrip de un seno de 1 kHz @48k ---------------------------------
    {
        const double fs = 48000.0;
        const int total = 4800;
        const int block = 64;
        for (int factor : kFactorsTube) {
            Oversampling os;
            os.prepareWithFactor(1, block, fs, factor);
            const int lat = os.latencySamples();

            std::vector<float> in(static_cast<size_t>(total), 0.0f);
            std::vector<float> out(static_cast<size_t>(total), 0.0f);
            // dos tonos no armónicos respecto al bloque: evita ambigüedad de periodo al
            // medir el desfase (un solo seno de 1 kHz repite cada 48 muestras)
            for (int i = 0; i < total; ++i) {
                in[static_cast<size_t>(i)] = static_cast<float>(
                    0.3 * std::sin(2.0 * 3.14159265358979 * 997.0 * i / fs) +
                    0.2 * std::sin(2.0 * 3.14159265358979 * 3123.0 * i / fs));
            }
            for (int b = 0; b < total / block; ++b) {
                float* outPtr = out.data() + b * block;
                const float* up = os.upsample(0, in.data() + b * block, block);
                CHECK(up != nullptr);
                os.downsample(0, up, block, outPtr);
            }
            // comparar con la entrada retardada 'lat' muestras. Antes se comprueba que la
            // latencia declarada es exactamente la del sistema (búsqueda del desfase).
            int bestLag = 0;
            double bestErr = 1e30;
            for (int lag = 0; lag <= 4 * Oversampling::kGroupDelay; ++lag) {
                double acc = 0.0;
                for (int i = 512; i < total; ++i) {
                    const double ref = (i - lag >= 0) ? in[static_cast<size_t>(i - lag)] : 0.0;
                    const double d = static_cast<double>(out[static_cast<size_t>(i)]) - ref;
                    acc += d * d;
                }
                if (acc < bestErr) {
                    bestErr = acc;
                    bestLag = lag;
                }
            }
            if (bestLag != lat) {
                std::printf("  factor %d: latencia medida %d, declarada %d\n", factor, bestLag,
                            lat);
                ++check::failures();
            }

            std::vector<float> ref(out.size(), 0.0f);
            for (int i = lat; i < total; ++i) ref[static_cast<size_t>(i)] = in[static_cast<size_t>(i - lat)];
            const double err = rmsError(out, ref, lat + 128, total);
            if (!(err < 0.005)) {  // < 1 % de la amplitud (0.5)
                std::printf("  roundtrip factor %d: rms=%.6f\n", factor, err);
                ++check::failures();
            }
            // la amplitud se conserva
            double peak = 0.0;
            for (int i = lat + 128; i < total; ++i) {
                peak = std::max(peak, std::fabs(static_cast<double>(out[static_cast<size_t>(i)])));
            }
            CHECK_NEAR(peak, 0.5, 0.02);
        }
    }

    // --- elección automática por la regla (prepare con conjunto permitido) --
    {
        Oversampling os;
        os.prepare(2, 64, 44100.0, kFactorsTube);
        CHECK(os.factor() == 4);          // regla del motor a 44.1k
        CHECK(os.stages() == 2);
        CHECK(os.channels() == 2);
        CHECK(os.latencySamples() == 48);  // 2 etapas: 32 + 16

        Oversampling sc;
        sc.prepare(2, 64, 44100.0, kFactorsSidechain);
        CHECK(sc.factor() == 4);          // SideSaturation nunca baja de 2x
        sc.prepare(2, 64, 192000.0, kFactorsSidechain);
        CHECK(sc.factor() == 2);
    }

    // --- canales independientes y llamadas fuera de rango -------------------
    {
        Oversampling os;
        os.prepare(2, 64, 48000.0, kFactorsTube);
        float inL[64], inR[64], outL[64], outR[64];
        for (int i = 0; i < 64; ++i) {
            inL[i] = 0.25f;
            inR[i] = -0.25f;
        }
        float lastL = 0.0f, lastR = 0.0f;
        for (int blk = 0; blk < 100; ++blk) {
            const float* upL = os.upsample(0, inL, 64);
            const float* upR = os.upsample(1, inR, 64);
            os.downsample(0, upL, 64, outL);
            os.downsample(1, upR, 64, outR);
            lastL = outL[63];
            lastR = outR[63];
        }
        CHECK_NEAR(lastL, 0.25, 1e-3);
        CHECK_NEAR(lastR, -0.25, 1e-3);
        CHECK(os.upsample(5, inL, 64) == nullptr);   // canal inválido
        CHECK(os.upsample(-1, inL, 64) == nullptr);
        CHECK(os.upsample(0, inL, 1000) == nullptr); // bloque mayor que maxBlock
        os.reset();                                  // no debe romper
    }

    return check::finish("test_oversampling");
}
