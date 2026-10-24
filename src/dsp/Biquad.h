// Retroexx — biquad / un-polo con estado (transpuesta directa II).
//
// C++17, sin dependencias, sin JUCE. Coeficientes en doble precisión.
//
//
// * (f, fs, c[6]) = diseñador bilineal PREWARPADO de la casa:
//        A = pi*f          ; K = A / tan(pi*f/fs)          (=> K/A = 1/tan(A))
//        D = 1/(A+K)^2
//        c0 = A^2*D   c1 = 2*A^2*D   c2 = K^2*D   c3 = -2*K^2*D
//        c4 = 2*(A^2-K^2)*D          c5 = (A-K)^2*D
//    Con L = A/(A+K), H = K/(A+K) eso es [L^2, 2L^2, H^2, -2H^2, 2(L^2-H^2), (L-H)^2],
//    el juego de coeficientes de un biquad de 2 polos con amortiguamiento fijo,
//    equivalente a un biquad Butterworth prewarpado. Como los coeficientes son
//    homogéneos en A, la forma que se implementa aquí es la misma con
//    A = pi*f/fs  y  g = tan(A) = tan(pi*f/fs)  (es decir, K = A/g).
//
// * (f_norm, Q, ganancia, dst, tipo) = diseñador RBJ de la casa:
//        w0 = 2*pi*f_norm (si f_norm <= 0.49; si no, se satura a 3.0787609 = 0.98*pi)
//        alpha = sin(w0)/(2Q)
//        tipo 0 = LP, 2 = HP, 4/5 = peaking, 6 = low shelf, 7 = high shelf
//    Los peaking y shelves del DSL usan ESTA forma (RBJ), con A = 10^(dB/40).
//    Se han verificado las expresiones algebraicas del peaking (caso 4/5) y del
// low shelf (caso 6); ver más abajo.
//
// Orden: el DSL usa hp6/lp6 = 1er orden (6 dB/oct, un polo) y hp12/lp12 = 2do orden.
//
#pragma once

#include <cmath>
#include <complex>

#include "Math.h"

namespace rtxx {
namespace dsp {

class Biquad {
public:
    Biquad() { reset(); }

    // --- estado ---------------------------------------------------------------
    void reset() {
        z1_ = 0.0;
        z2_ = 0.0;
    }

    // --- coeficientes (double) ------------------------------------------------
    void setCoefficients(double b0, double b1, double b2, double a1, double a2) {
        b0_ = b0;
        b1_ = b1;
        b2_ = b2;
        a1_ = a1;
        a2_ = a2;
    }

    void setBypass() { setCoefficients(1.0, 0.0, 0.0, 0.0, 0.0); }

    double b0() const { return b0_; }
    double b1() const { return b1_; }
    double b2() const { return b2_; }
    double a1() const { return a1_; }
    double a2() const { return a2_; }

// --- diseño: peaking RBJ ------------------------
    // b0 = 1 + alpha*A    b1 = -2cos(w0)    b2 = 1 - alpha*A
    // a0 = 1 + alpha/A    a1 = -2cos(w0)    a2 = 1 - alpha/A
    void setPeaking(double fs, double freq, double gainDb, double q) {
        const double w0 = twoPiW0(fs, freq);
        const double alpha = std::sin(w0) / (2.0 * maxQ(q));
        const double A = rb20Amplitude(gainDb);
        const double c = std::cos(w0);
        const double a0 = 1.0 + alpha / A;
        setCoefficients((1.0 + alpha * A) / a0, (-2.0 * c) / a0, (1.0 - alpha * A) / a0,
                        (-2.0 * c) / a0, (1.0 - alpha / A) / a0);
    }

// --- diseño: shelves RBJ --------------------
// alpha = sin(w0)/(2Q), A = 10^(dB/40), s = 2*sqrt(A)*alpha (convención del motor,
    // que parameteriza los shelves por Q y no por el factor S de la tabla RBJ).
    void setLowShelf(double fs, double freq, double gainDb, double q) {
        const double w0 = twoPiW0(fs, freq);
        const double alpha = std::sin(w0) / (2.0 * maxQ(q));
        const double A = rb20Amplitude(gainDb);
        const double c = std::cos(w0);
        const double s = 2.0 * std::sqrt(A) * alpha;
        const double a0 = (A + 1.0) + (A - 1.0) * c + s;
        setCoefficients(A * ((A + 1.0) - (A - 1.0) * c + s) / a0,
                        (2.0 * A * ((A - 1.0) - (A + 1.0) * c)) / a0,
                        A * ((A + 1.0) - (A - 1.0) * c - s) / a0,
                        (-2.0 * ((A - 1.0) + (A + 1.0) * c)) / a0,
                        ((A + 1.0) + (A - 1.0) * c - s) / a0);
    }

    void setHighShelf(double fs, double freq, double gainDb, double q) {
        const double w0 = twoPiW0(fs, freq);
        const double alpha = std::sin(w0) / (2.0 * maxQ(q));
        const double A = rb20Amplitude(gainDb);
        const double c = std::cos(w0);
        const double s = 2.0 * std::sqrt(A) * alpha;
        const double a0 = (A + 1.0) - (A - 1.0) * c + s;
        setCoefficients(A * ((A + 1.0) + (A - 1.0) * c + s) / a0,
                        (-2.0 * A * ((A - 1.0) + (A + 1.0) * c)) / a0,
                        A * ((A + 1.0) + (A - 1.0) * c - s) / a0,
                        (2.0 * ((A - 1.0) - (A + 1.0) * c)) / a0,
                        ((A + 1.0) - (A - 1.0) * c - s) / a0);
    }

// --- diseño: LP/HP prewarpados ------------------
    // order 1 -> un polo (6 dB/oct, lp6/hp6);  order 2 -> biquad (lp12/hp12).
    // g = tan(pi*f/fs)  (== tan(A) con A = pi*f/fs, es decir K = A/g)
    void setLowPass(double fs, double freq, double q, int order) {
        const double g = prewarpG(fs, freq);
        if (order <= 1) {
            const double d = 1.0 + g;
            setCoefficients(g / d, g / d, 0.0, (g - 1.0) / d, 0.0);
        } else {
            const double k = 1.0 / maxQ(q);
            const double d = 1.0 + k * g + g * g;
            setCoefficients(g * g / d, 2.0 * g * g / d, g * g / d,
                            2.0 * (g * g - 1.0) / d, (1.0 - k * g + g * g) / d);
        }
    }

    void setHighPass(double fs, double freq, double q, int order) {
        const double g = prewarpG(fs, freq);
        if (order <= 1) {
            const double d = 1.0 + g;
            setCoefficients(1.0 / d, -1.0 / d, 0.0, (g - 1.0) / d, 0.0);
        } else {
            const double k = 1.0 / maxQ(q);
            const double d = 1.0 + k * g + g * g;
            setCoefficients(1.0 / d, -2.0 / d, 1.0 / d,
                            2.0 * (g * g - 1.0) / d, (1.0 - k * g + g * g) / d);
        }
    }

    // --- proceso: forma directa II transpuesta --------------------------------
    inline float process(float x) {
        const double in = static_cast<double>(x);
        const double y = b0_ * in + z1_;
        z1_ = b1_ * in - a1_ * y + z2_;
        z2_ = b2_ * in - a2_ * y;
        return static_cast<float>(y);
    }

    inline double processD(double x) {
        const double y = b0_ * x + z1_;
        z1_ = b1_ * x - a1_ * y + z2_;
        z2_ = b2_ * x - a2_ * y;
        return y;
    }

    // --- respuesta en frecuencia (para tests y para verificar el diseño) ------
    double magnitude(double freq, double fs) const {
        const double w = 2.0 * kPi * (freq / fs);
        const std::complex<double> z = std::polar(1.0, -w);
        const std::complex<double> num = b0_ + b1_ * z + b2_ * z * z;
        const std::complex<double> den = 1.0 + a1_ * z + a2_ * z * z;
        return std::abs(num / den);
    }

    double magnitudeDb(double freq, double fs) const { return gainToDb(magnitude(freq, fs)); }

    // --- utilidades de diseño -------------------------------------------------
    static constexpr double kPi = 3.14159265358979323846;

// g = tan(pi*f/fs): prewarping bilineal del motor (A = pi*f/fs, K = A/g).
    static double prewarpG(double fs, double freq) {
        const double a = kPi * clamp(freq, 1.0e-6, 0.4999 * fs) / fs;
        return std::tan(a);
    }

    // A = pi*f/fs y K = A/tan(A) — forma literal del informe (equivalente a g = tan(A)).
    static double prewarpA(double fs, double freq) {
        return kPi * clamp(freq, 1.0e-6, 0.4999 * fs) / fs;
    }

    static double prewarpK(double fs, double freq) {
        const double a = prewarpA(fs, freq);
        return a / std::tan(a);
    }

private:
// w0 = 2*pi*f/fs, con la guarda observada en (f_norm > 0.49 -> 0.98*pi).
    static double twoPiW0(double fs, double freq) {
        double fn = freq / fs;
        if (fn > 0.49) fn = 0.49;
        if (fn < 0.0) fn = 0.0;
        return 2.0 * kPi * fn;
    }

    static double maxQ(double q) { return q < 1.0e-4 ? 1.0e-4 : q; }

    double b0_ = 1.0, b1_ = 0.0, b2_ = 0.0, a1_ = 0.0, a2_ = 0.0;
    double z1_ = 0.0, z2_ = 0.0;
};

}  // namespace dsp
}  // namespace rtxx
