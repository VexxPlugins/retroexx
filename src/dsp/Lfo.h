// Retroexx — LFO de fase normalizada (wobble / flutter / bit-color).
//
// C++17, sin dependencias, sin JUCE.
//
// Propiedades:
// * Fase inicial = 3/4 (0.75), sembrada en los cuatro LFO (L/R × dos pares).
// * Avance por muestra:
//        phase += inc ;  while (phase >= 1) phase -= 1 ;  lfo = shape(phase)
//    (incremento primero, evaluación después; wrap a [0,1)).
// * Selección de forma:
// shape = table11[ clamp((int)WaveShape - 1, 0, 10) ]; fallback ParabolicSine
// * Forma por defecto: seno PARABÓLICO de 4 tramos (quiebres 0.25/0.5/0.75, factores
// ±1.001195) con un pulido final (57.34, −19.56). NO es sinf().
// * Warp: WaveWarp/WaveWarp2 entran como (1 − warp), cuantizado a 2^-14; en la forma 1
// el warp desplaza el punto de plegado de la fase (0.5 − warp).
//
// ESTADO DE LAS FORMAS:
//   fijadas   -> ParabolicSine (estructura), Triangle, Square, SawUp, SawDown
//   TODO(medir) -> WarpedSine (álgebra del warp), Stairs2/2Inv/4/4Inv/Wide/Alt
// (sólo hay constantes sueltas: 0.66667, 0.33301, 0.33001, −0.33001,
//                   0.8330, 0.6, 0.2; falta el mapa umbral->nivel)
// y el pulido final del seno parabólico (57.34 / −19.56).
//
#pragma once

#include <cmath>

#include "Math.h"

namespace rtxx {
namespace dsp {

enum class LfoShape : int {
// Fallback: forma por defecto del motor.
    ParabolicSine = 0,
    // Tabla de 11 del motor (índice 1..11 = valor de WaveShape).
    WarpedSine = 1, // seno con warp asimétrico
    Triangle = 2, //
    Square = 3, //
    SawUp = 4, //
    SawDown = 5, //
    Stairs2 = 6, // TODO(medir)
    Stairs2Inv = 7, // TODO(medir)
    Stairs4 = 8, // TODO(medir)
    Stairs4Inv = 9, // TODO(medir)
    StairsWide = 10, // TODO(medir)
    StairsAlt = 11, // TODO(medir)
};

class Lfo {
public:
static constexpr int kTableSize = 11; // tabla
    static constexpr float kInitialPhase = 0.75f;    // 0x3f400000
    static constexpr float kParabolicScale = 1.001195f;  // 0x3f80272a

    Lfo() { reset(); }

    void reset(float phase = kInitialPhase) {
        phase_ = static_cast<double>(phase);
        wrap();
        primed_ = false;
        last_ = 0.0f;
    }

    // --- forma ---------------------------------------------------------------
    void setShape(LfoShape s) { shape_ = s; }
    LfoShape shape() const { return shape_; }

// Mapeo del motor: idx = (int)WaveShape - 1; si 0..10 -> tabla; si no -> fallback.
    static LfoShape shapeFromParam(int waveShape) {
        const int idx = waveShape - 1;
        if (idx < 0 || idx >= kTableSize) return LfoShape::ParabolicSine;
        return static_cast<LfoShape>(idx + 1);
    }

    void setShapeFromParam(int waveShape) { setShape(shapeFromParam(waveShape)); }

// WaveWarp del usuario (0..1). El motor trabaja con (1 - warp) cuantizado a 2^-14.
    void setWarp(float waveWarp) { warp_ = quantizeParam(1.0f - waveWarp); }
    float warp() const { return warp_; }

    // --- avance --------------------------------------------------------------
    void setRate(double rateHz, double fs) {
        inc_ = (fs > 0.0) ? rateHz / fs : 0.0;
    }

    void setPhaseInc(double incPerSample) { inc_ = incPerSample; }
    double phaseInc() const { return inc_; }

    double phase() const { return phase_; }

    void advance(int nSamples) {
        phase_ += inc_ * static_cast<double>(nSamples);
        wrap();
    }

    // --- evaluación ----------------------------------------------------------
    // Valor de la forma para una fase arbitraria (plegada a [0,1)).
    float valueAt(float phase) const { return evaluate(shape_, phase, warp_); }

// Una muestra: incrementa la fase y devuelve la forma.
    inline float processSample() {
        phase_ += inc_;
        wrap();
        last_ = valueAt(static_cast<float>(phase_));
        primed_ = true;
        return last_;
    }

    // Un bloque completo (incremento por bloque, sin interpolar dentro del bloque).
    void processBlock(float* out, int n) {
        advance(n);
        const float v = valueAt(static_cast<float>(phase_));
        for (int i = 0; i < n; ++i) out[i] = v;
        last_ = v;
        primed_ = true;
    }

    float lastValue() const { return last_; }
    bool primed() const { return primed_; }

// Cruce por cero del LFO: El motor re-sortea la deriva de fase ahí
// (: if signbit(lfo) != signbit(lfoPrev)...).
    bool crossedZero(float previous) const {
        return std::signbit(previous) != std::signbit(last_);
    }

    // --- formas --------------------------------------------------------------
    // Seno parabólico de 4 tramos. Segmentos cuadráticos con quiebres en
    // 0.25/0.5/0.75, extremos +-1 en 0.25/0.75 y ceros en 0/0.5/1; derivada continua.
    // TODO(medir): el pulido final del motor (57.34 / -19.56) no está recuperado;
    // aquí se aplica el factor observado 1.001195 y se recorta a [-1,1].
    static float parabolicSine(float p) {
        const float x = fold01(p);
        float y;
        if (x < 0.25f) {
            const float s = 1.0f - 4.0f * x;   // 1 -> 0
            y = 1.0f - s * s;
        } else if (x < 0.5f) {
            const float s = 4.0f * x - 1.0f;   // 0 -> 1
            y = 1.0f - s * s;
        } else if (x < 0.75f) {
            const float s = 3.0f - 4.0f * x;   // 1 -> 0
            y = -(1.0f - s * s);
        } else {
            const float s = 4.0f * x - 3.0f;   // 0 -> 1
            y = -(1.0f - s * s);
        }
        return clamp(y * kParabolicScale, -1.0f, 1.0f);
    }

// Triangular: 4*fold(p), picos en 0.25 (+1) y 0.75 (-1).
    static float triangle(float p) {
        const float x = fold01(p);
        if (x < 0.25f) return 4.0f * x;
        if (x < 0.75f) return 1.0f - 4.0f * (x - 0.25f);
        return -1.0f + 4.0f * (x - 0.75f);
    }

    static float square(float p) { return fold01(p) < 0.5f ? 1.0f : -1.0f; }

    static float sawUp(float p) { return 2.0f * (fold01(p) - 0.5f); }

    static float sawDown(float p) { return 2.0f * (0.5f - fold01(p)); }

// TODO(medir): las escaleras dependen de un mapa umbral->nivel que no está
// recuperado; se usan las constantes ajustadas y se documenta como provisional.
    static float stairs2(float p) {
        const float x = fold01(p);
        const float lvl = 0.33001f;    // 0x3ea8f5c3
        const float t0 = 0.33301f;     // 0x3eaa7efa
        const float t1 = 0.66667f;     // 0x3f2ac083
        return x < t0 ? -lvl : (x < t1 ? lvl : -lvl);
    }

    static float stairs2Inv(float p) { return -stairs2(p); }

    static float stairs4(float p) {
        const float x = fold01(p);
        const float lo = 0.8330f;      // 0x3f553f7d
        const float hi = 0.33001f;     // 0x3ea8f5c3
        if (x < 0.25f) return -lo;
        if (x < 0.5f) return -hi;
        if (x < 0.75f) return hi;
        return lo;
    }

    static float stairs4Inv(float p) {
        const float x = fold01(p);
        const float lo = 0.8330f;
        const float hi = 0.33001f;
        if (x < 0.25f) return lo;
        if (x < 0.5f) return hi;
        if (x < 0.75f) return -hi;
        return -lo;
    }

    static float stairsWide(float p) {
        const float x = fold01(p);
        const float a = 0.8330f;   // 0x3f553f7d
        const float b = 0.33301f;  // 0x3eaa7efa
        if (x < 0.2f) return -a;   // 0x3e4ccccd
        if (x < 0.6f) return -b;   // 0x3f19999a
        if (x < 0.8f) return b;
        return a;
    }

    static float stairsAlt(float p) {
        const float x = fold01(p);
        const float a = 0.8330f;
        const float b = 0.6f;
        const float c = 0.33301f;
        const float d = 0.2f;
        if (x < 0.25f) return -d;
        if (x < 0.5f) return c;
        if (x < 0.75f) return b;
        return a;
    }

// Seno con warp. TODO(medir): álgebra exacta no recuperada.
    // Reconstrucción provisional y declarada: se pliega la semionda en 'fold' y se
    // evalúa un arco parabólico; warp = 0.5 (default) => sin deformar, warp -> 1 =>
    // progresivamente tipo sierra (que es lo que describe el informe).
    static float warpedSine(float p, float warp) {
        const float deform = clamp(2.0f * warp - 1.0f, 0.0f, 1.0f);
        if (deform <= 0.0f) return parabolicSine(p);
        const float fold = clamp(0.5f - 0.5f * deform, 0.02f, 0.5f); // 0.5 - warp
        const float x = fold01(p);
        const bool pos = x < 0.5f;
        const float half = pos ? x : 1.0f - x;  // 0 .. 0.5
        float h;
        if (half < fold) {
            h = half * (0.5f / fold);
        } else {
            h = 0.5f + (half - fold) * (0.5f / (0.5f - fold));
        }
        const float s = 2.0f * clamp(h, 0.0f, 1.0f) - 1.0f;
        const float arch = clamp(1.0f - s * s, -1.0f, 1.0f);
        const float y = pos ? arch : -arch;
        return clamp(y, -1.0f, 1.0f);
    }

    static float evaluate(LfoShape s, float phase, float warp) {
        switch (s) {
            case LfoShape::ParabolicSine: return parabolicSine(phase);
            case LfoShape::WarpedSine: return warpedSine(phase, warp);
            case LfoShape::Triangle: return triangle(phase);
            case LfoShape::Square: return square(phase);
            case LfoShape::SawUp: return sawUp(phase);
            case LfoShape::SawDown: return sawDown(phase);
            case LfoShape::Stairs2: return stairs2(phase);
            case LfoShape::Stairs2Inv: return stairs2Inv(phase);
            case LfoShape::Stairs4: return stairs4(phase);
            case LfoShape::Stairs4Inv: return stairs4Inv(phase);
            case LfoShape::StairsWide: return stairsWide(phase);
            case LfoShape::StairsAlt: return stairsAlt(phase);
        }
        return parabolicSine(phase);
    }

    static constexpr int kNumShapes = 12;  // 1 fallback + tabla de 11

    static LfoShape shapeByIndex(int i) {
        static const LfoShape table[kNumShapes] = {
            LfoShape::ParabolicSine, LfoShape::WarpedSine, LfoShape::Triangle,
            LfoShape::Square,        LfoShape::SawUp,      LfoShape::SawDown,
            LfoShape::Stairs2,       LfoShape::Stairs2Inv, LfoShape::Stairs4,
            LfoShape::Stairs4Inv,    LfoShape::StairsWide, LfoShape::StairsAlt};
        return table[clamp(i, 0, kNumShapes - 1)];
    }

    // Plegado de fase a [0,1).
    static float fold01(float p) {
        float x = p - std::floor(p);
        if (x >= 1.0f) x = 0.0f;
        if (x < 0.0f) x = 0.0f;
        return x;
    }

private:
    void wrap() {
        phase_ -= std::floor(phase_);
        if (phase_ >= 1.0) phase_ = 0.0;
        if (phase_ < 0.0) phase_ = 0.0;
    }

    double phase_ = static_cast<double>(kInitialPhase);
    double inc_ = 0.0;
    float warp_ = 0.5f;
    float last_ = 0.0f;
    bool primed_ = false;
    LfoShape shape_ = LfoShape::ParabolicSine;
};

}  // namespace dsp
}  // namespace rtxx
