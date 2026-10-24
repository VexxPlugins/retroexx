// Retroexx — primitivas numéricas comunes.
//
// C++17, sin dependencias externas, sin JUCE.
//
// Cuantización de parámetros: el valor de trabajo es round(v*16384)/16384, con resolución
// 1/16384 (≈ −84 dB) y vuelta exacta a float."
//
#pragma once

#include <cmath>

namespace rtxx {
namespace dsp {

// --- cuantización de parámetros ------------------------------------------------
// 16384 = 2^14 y su inversa es exacta en binario (6.10352e-05).
constexpr float kParamQuantSteps = 16384.0f;
constexpr float kParamQuantScale = 1.0f / kParamQuantSteps;
constexpr float kParamQuantResolution = 1.0f / 16384.0f;     // 2^-14

// Todo float que entra a un bloque pasa por aquí (contrato del frente dsp).
inline float quantizeParam(float v) {
    return std::round(v * kParamQuantSteps) / kParamQuantSteps;
}

// --- utilidades ---------------------------------------------------------------
inline float clamp(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

inline double clamp(double v, double lo, double hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

inline int clamp(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

inline float clamp01(float v) { return clamp(v, 0.0f, 1.0f); }

// dB <-> ganancia lineal de amplitud (20*log10).
// El motor usa expf(dB*0.025*2.3025851) (= 10^(dB/40), amplitud "A" de RBJ) dentro de
// los diseñadores de filtro; para ganancias de señal la convención es 10^(dB/20).
constexpr float kDbPerLnScale = 0.05f;

inline float dbToGain(float db) { return std::pow(10.0f, db * kDbPerLnScale); }
inline double dbToGain(double db) { return std::pow(10.0, db * 0.05); }

inline float gainToDb(float gain) {
    const float g = gain < 1e-30f ? 1e-30f : gain;
    return 20.0f * std::log10(g);
}

inline double gainToDb(double gain) {
    const double g = gain < 1e-30 ? 1e-30 : gain;
    return 20.0 * std::log10(g);
}

inline float lerp(float a, float b, float t) { return a + (b - a) * t; }
inline double lerp(double a, double b, double t) { return a + (b - a) * t; }

// 10^(dB/40): amplitud "A" de las fórmulas RBJ (peaking/shelf) tal como la calcula el
// motor en (expf(x*0.025*2.3025851)).
inline double rb20Amplitude(double gainDb) { return std::pow(10.0, gainDb / 40.0); }

inline bool isFinite(float v) { return std::isfinite(v); }

}  // namespace dsp
}  // namespace rtxx
