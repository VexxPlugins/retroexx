// Test de src/dsp/Biquad.h
//   - peaking en la frecuencia central da exactamente gainDb (+-0.01 dB)
//   - LP/HP a la frecuencia de corte dan -3.01 dB (+-0.1), orden 1 y 2
//   - DC y Nyquist en rango esperado
//   - estabilidad con q alto (q=10): respuesta a impulso finita y decreciente
//   - shelves: ganancia en DC/Nyquist igual al valor pedido
//   - prewarpG/prewarpK coherentes con A = pi*f/fs, K = A/tan(A)
#include <algorithm>
#include <cmath>

#include "Check.h"
#include "dsp/Biquad.h"

using namespace rtxx::dsp;

namespace {

double impulsePeakAfter(Biquad& bq, int nSamples, int from, int to) {
    bq.reset();
    double peak = 0.0;
    for (int i = 0; i < nSamples; ++i) {
        const double y = bq.processD(i == 0 ? 1.0 : 0.0);
        if (i >= from && i <= to) peak = std::max(peak, std::fabs(y));
    }
    return peak;
}

bool allFinite(Biquad& bq, int nSamples) {
    bq.reset();
    bool ok = true;
    for (int i = 0; i < nSamples; ++i) {
        const double y = bq.processD(i == 0 ? 1.0 : 0.0);
        if (!std::isfinite(y)) ok = false;
        if (std::fabs(y) > 1e6) ok = false;
    }
    return ok;
}

}  // namespace

int main() {
    const double fs = 48000.0;

    // --- prewarping: A = pi*f/fs y K = A/tan(A) -------------------------------
    {
        const double f = 1000.0;
        const double A = Biquad::prewarpA(fs, f);
        const double K = Biquad::prewarpK(fs, f);
        CHECK_NEAR(A, Biquad::kPi * f / fs, 1e-15);
        CHECK_NEAR(K, A / std::tan(A), 1e-15);
        // g = tan(A) = A/K  y además g = w0/(2*tan(w0/2)) con w0 = 2A
        CHECK_NEAR(Biquad::prewarpG(fs, f), std::tan(A), 1e-12);
        CHECK_NEAR(Biquad::prewarpG(fs, f), A / K, 1e-12);
        const double w0 = 2.0 * A;
        // K = A/tan(A) equivale a w0/(2*tan(w0/2)); y g = tan(A) = A/K
        CHECK_NEAR(K, w0 / (2.0 * std::tan(w0 * 0.5)), 1e-10);
        CHECK_NEAR(2.0 * K, w0 / std::tan(w0 * 0.5), 1e-10);
        CHECK_NEAR(Biquad::prewarpG(fs, f), 1.0 / (K / A), 1e-12);
    }

    // --- peaking: exactamente gainDb en la frecuencia central ------------------
    {
        const double freqs[] = {100.0, 1000.0, 5000.0, 12000.0};
        const double gains[] = {-24.0, -12.0, -6.0, -1.0, 0.0, 3.0, 12.0, 24.0};
        const double qs[] = {0.5, 0.7071, 1.0, 4.0};
        for (double f : freqs) {
            for (double g : gains) {
                for (double q : qs) {
                    Biquad bq;
                    bq.setPeaking(fs, f, g, q);
                    CHECK_NEAR(bq.magnitudeDb(f, fs), g, 0.01);
                }
            }
        }
    }

    // --- LP/HP 2do orden: -3.01 dB en la frecuencia de corte (Q = 1/sqrt2) -----
    {
        const double q = 1.0 / std::sqrt(2.0);
        const double freqs[] = {50.0, 200.0, 1000.0, 5000.0, 15000.0};
        for (double f : freqs) {
            Biquad lp, hp;
            lp.setLowPass(fs, f, q, 2);
            hp.setHighPass(fs, f, q, 2);
            CHECK_NEAR(lp.magnitudeDb(f, fs), -3.0103, 0.1);
            CHECK_NEAR(hp.magnitudeDb(f, fs), -3.0103, 0.1);
        }
    }

    // --- LP/HP 1er orden (lp6/hp6): -3.01 dB en la frecuencia de corte ---------
    {
        const double freqs[] = {50.0, 200.0, 1000.0, 5000.0, 15000.0};
        for (double f : freqs) {
            Biquad lp, hp;
            lp.setLowPass(fs, f, 0.7071, 1);
            hp.setHighPass(fs, f, 0.7071, 1);
            CHECK_NEAR(lp.magnitudeDb(f, fs), -3.0103, 0.1);
            CHECK_NEAR(hp.magnitudeDb(f, fs), -3.0103, 0.1);
        }
        // pendiente asintótica: 6 dB/oct frente a 12 dB/oct del 2do orden
        // (se mide lejos de Nyquist para no mezclar el warping bilineal)
        Biquad lp1, lp2;
        lp1.setLowPass(fs, 100.0, 0.7071, 1);
        lp2.setLowPass(fs, 100.0, 1.0 / std::sqrt(2.0), 2);
        const double d1 = lp1.magnitudeDb(1600.0, fs) - lp1.magnitudeDb(800.0, fs);
        const double d2 = lp2.magnitudeDb(1600.0, fs) - lp2.magnitudeDb(800.0, fs);
        CHECK_NEAR(d1, -6.02, 0.4);
        CHECK_NEAR(d2, -12.04, 0.8);
    }

    // --- DC y Nyquist --------------------------------------------------------
    {
        Biquad lp, hp;
        lp.setLowPass(fs, 1000.0, 1.0 / std::sqrt(2.0), 2);
        hp.setHighPass(fs, 1000.0, 1.0 / std::sqrt(2.0), 2);
        CHECK_NEAR(lp.magnitudeDb(0.0, fs), 0.0, 0.01);        // DC pasa
        CHECK_LT(lp.magnitudeDb(24000.0, fs), -60.0);          // Nyquist anulado
        CHECK_LT(hp.magnitudeDb(0.0, fs), -60.0);              // DC anulado
        CHECK_NEAR(hp.magnitudeDb(24000.0, fs), 0.0, 0.01);    // Nyquist pasa

        Biquad lp1, hp1;
        lp1.setLowPass(fs, 1000.0, 0.7071, 1);
        hp1.setHighPass(fs, 1000.0, 0.7071, 1);
        CHECK_NEAR(lp1.magnitudeDb(0.0, fs), 0.0, 0.01);
        CHECK_LT(lp1.magnitudeDb(24000.0, fs), -25.0);
        CHECK_LT(hp1.magnitudeDb(0.0, fs), -25.0);
        CHECK_NEAR(hp1.magnitudeDb(24000.0, fs), 0.0, 0.01);
    }

    // --- shelves -------------------------------------------------------------
    {
        Biquad ls, hs;
        ls.setLowShelf(fs, 200.0, 12.0, 0.7071);
        hs.setHighShelf(fs, 5000.0, -12.0, 0.7071);
        CHECK_NEAR(ls.magnitudeDb(0.0, fs), 12.0, 0.05);
        CHECK_LT(ls.magnitudeDb(20000.0, fs), 0.5);
        CHECK_NEAR(hs.magnitudeDb(24000.0, fs), -12.0, 0.5);
        CHECK_LT(std::fabs(hs.magnitudeDb(20.0, fs)), 0.5);
        // en la frecuencia de codo, un shelf RBJ da la mitad en dB (+-3 dB de 12)
        CHECK_NEAR(ls.magnitudeDb(200.0, fs), 6.0, 1.0);
    }

    // --- estabilidad con q = 10 ----------------------------------------------
    {
        const double q = 10.0;
        for (int order = 1; order <= 2; ++order) {
            for (int type = 0; type < 2; ++type) {
                Biquad bq;
                if (type == 0) {
                    bq.setLowPass(fs, 1000.0, q, order);
                } else {
                    bq.setHighPass(fs, 1000.0, q, order);
                }
                CHECK(allFinite(bq, 200000));
                // la energía decae: el pico tardío es despreciable
                const double tail = impulsePeakAfter(bq, 200000, 150000, 199999);
                CHECK_LT(tail, 1e-3);
                CHECK_LT(bq.magnitudeDb(1000.0, fs), 30.0);  // resonancia acotada
            }
        }
        // peaking con q=10 también estable
        Biquad pk;
        pk.setPeaking(fs, 2000.0, 24.0, 10.0);
        CHECK(allFinite(pk, 200000));
        CHECK_LT(impulsePeakAfter(pk, 200000, 150000, 199999), 1e-3);
        // shelf con q extremo
        Biquad sh;
        sh.setLowShelf(fs, 100.0, 24.0, 10.0);
        CHECK(allFinite(sh, 200000));
    }

    // --- proceso en float: sin denormales ni NaN -----------------------------
    {
        Biquad bq;
        bq.setLowPass(fs, 22000.0, 0.7071, 2);
        bq.reset();
        for (int i = 0; i < 48000; ++i) {
            const float y = bq.process(i == 0 ? 1.0f : 0.0f);
            CHECK(std::isfinite(y));
        }
    }

    return check::finish("test_biquad");
}
