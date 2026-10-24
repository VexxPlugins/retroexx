// Retroexx — suavizado: un-polo exponencial y rampa lineal por bloque.
//
// C++17, sin dependencias, sin JUCE.
//
//
// Suavizado del retardo base del wobble
//       x   = 32.0f / sr
// a = expf(-2*pi*x); = expf
//       k2  = a                      ; [0x494]  coeficiente del un-polo
//       k1sm= 1 - a                  ; [0x48c]
//       k3  = (1 + a) * 0.5f         ; [0x490]  (forma TPT)
//   -> constante de tiempo = 32 muestras (a 44.1 kHz: 0.73 ms)
//
// Rampa del retardo base por bloque
//       buf[k] = D_old*sr*0.001 + k*(D_new - D_old)*sr*0.001/64,  k = 0..63
//   -> interpolación lineal del valor a lo largo de los 64 pasos del bloque.
//   Lo mismo para la profundidad de modulación (M_old + k*(M_new-M_old)/64).
//
// Nota de convención: la fórmula del motor a = exp(-2*pi*tc/fs) NO da el 63.2 % en tc
// (da 1 - exp(-2*pi) tras tc pasos, ~99.8 %); el 63.2 % llega a ~1/(1-a) muestras
// (~219 muestras con tc = 32 @44.1k). Se exponen las dos convenciones: `prepare()` usa la
// fórmula del motor (por defecto) y `setTauSamples()` la convención tau = 1/(1-a),
// que sí cumple el 63.2 % en tc. Ver DELIVERY.md.
//
#pragma once

#include <cmath>

#include "Math.h"

namespace rtxx {
namespace dsp {

class OnePole {
public:
    // tc = 32 muestras: el valor observado en el suavizado del retardo del wobble.
    static constexpr float kDefaultTimeConstantSamples = 32.0f;

    OnePole() { reset(0.0f); }

    void reset(float value = 0.0f) { y_ = value; }

// Fórmula del motor: a = exp(-2*pi*tc/fs) (tc en muestras).
    void prepare(float fs, float tcSamples = kDefaultTimeConstantSamples) {
        setCoefA(std::exp(-kTwoPi * tcSamples / fs));
    }

    // Convención tau = 1/(1-a): alcanza el 63.2 % en 'tauSamples' muestras.
    // (a = exp(-1/tau) es independiente de fs porque tau ya está en muestras.)
    void setTauSamples(float tauSamples) {
        setCoefA(std::exp(-1.0f / (tauSamples < 1.0e-6f ? 1.0e-6f : tauSamples)));
    }

    void setCoefA(float a) {
        a_ = clamp(a, 0.0f, 0.9999999f);
        oneMinusA_ = 1.0f - a_;
    }

    float coefA() const { return a_; }               // [0x494]
    float coefOneMinusA() const { return oneMinusA_; }  // [0x48c]
    float coefHalf() const { return (1.0f + a_) * 0.5f; }  // [0x490], forma TPT
    float value() const { return y_; }

    // y[n] = (1-a)*x[n] + a*y[n-1]
    inline float process(float x) {
        y_ = oneMinusA_ * x + a_ * y_;
        return y_;
    }

    // y[n] = a*(x[n]-y[n-1]) + y[n-1]  (equivalente, usado como bloqueo DC / integrador)
    inline float processTpt(float x) {
        const float t = (x - y_) * oneMinusA_;
        y_ = y_ + t;
        return t;
    }

private:
    static constexpr float kTwoPi = 6.283185307179586f;
    float a_ = 0.0f;
    float oneMinusA_ = 1.0f;
    float y_ = 0.0f;
};

class Ramp {
public:
// El motor rampa el retardo base en 64 pasos por bloque (frame de 64 muestras).
    static constexpr int kDefaultSteps = 64;

    Ramp() { start(0.0f, 0.0f, kDefaultSteps); }

    void start(float from, float to, int steps = kDefaultSteps) {
        from_ = from;
        to_ = to;
        steps_ = steps < 1 ? 1 : steps;
        k_ = 0;
        update();
    }

    int steps() const { return steps_; }
    bool done() const { return k_ >= steps_; }
    float value() const { return value_; }

// Devuelve el valor del paso actual (k = 0..N-1, igual que El motor:
    // buf[k] = from + k*(to-from)/N) y avanza uno. Tras N llamadas value() == to exacto.
    inline float next() {
        const float v = value_;
        if (k_ < steps_) {
            ++k_;
            update();
        }
        return v;
    }

private:
    void update() {
        value_ = (k_ >= steps_)
                     ? to_
                     : from_ + (to_ - from_) * (static_cast<float>(k_) /
                                                static_cast<float>(steps_));
    }

    float from_ = 0.0f;
    float to_ = 0.0f;
    float value_ = 0.0f;
    int steps_ = kDefaultSteps;
    int k_ = 0;
};

}  // namespace dsp
}  // namespace rtxx
