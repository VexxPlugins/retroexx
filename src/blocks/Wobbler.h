#pragma once
// cWobbler (nodo `Wobble` del grafo): wow/flutter de retardo, dos motores de LFO (A = wow,
// B = flutter) sobre una línea de retardo fraccional por canal.
//
// Ecuaciones cerradas (todas por muestra, por canal `ch`):
// D = 2·floor(0.02·sr) + 1 1765 @44.1 kHz (40.02 ms)
// c = exp(−2π·10/sr), k = 1−c one-pole sobre el retardo (k+c = 1)
// L = forma(fase) ∈ [−1,+1] tabla de 11 formas (más abajo)
// mod = clamp( clamp(m + Depth·(L_A+1)/2, 0, Mix) + Depth2·(L_B+1)/2, 0, 1)
// d[ch] = k·(b + mod·D) + c·d_prev[ch], b = 1 (0 si Depth == Depth2 == 0)
// y = interpolación de 4 puntos en d Catmull-Rom si d ≥ 1, lineal si d < 1
// out = ((1−Mix)·x + Mix·y)·G, G = 10^(dB/20)
//
// El término aditivo `m` (LfoNoise·LfoNoiseScaler) y su ganancia `G` dependen del
// generador de *flux* de la capa de sync, que no está implementado: aquí m = 0 y G = 1
// (estado de fábrica, LfoNoise = 0 y Flux = 0) y se marca [?] en el ROADMAP.
//
// `Rate`/`Rate2` llegan ya en Hz desde el modelo (ley `fit3`, ver
// ParamStore::syncDerived); el DSP no aplica ninguna curva extra.
#include <vector>

#include "block/Block.h"
#include "model/ParamStore.h"

namespace rtxx {

// Tabla de formas del LFO, compartida por cWobbler y cFlutter.
namespace lfo {

enum class Shape : int {
FallbackSine = 0, // la que se usa si WaveShape ∉ [1,11] (estado de fábrica)
WarpedMorphSine, // slot 1
Triangle, // slot 2
Square, // slot 3
SawUp, // slot 4
SawDown, // slot 5
Stairs3Up, // slot 6
Stairs3Down, // slot 7
Stairs4Up, // slot 8
Stairs4Down, // slot 9
Stairs6Up, // slot 10
Stairs6Down // slot 11
};

constexpr int kTableSize = 11;

// Selección del motor: v = (int)WaveShape; slot = v−1 si 1 ≤ v ≤ 11; si no,.
Shape shapeFromWaveShape(int waveShape);

// Plegado de fase a [0,1).
float fold01(float phase);

// Warp `(fase, w)`: identidad si w == 0.5 (el caso de fábrica, WaveWarp = 0.5)
// o si la fase es 0/1; si no, deformación racional de dos tramos con punto de ruptura w.
// [?] El álgebra exacta del tramo deformado no está recuperada y WaveWarp no es control
// de usuario, así que aquí sólo se garantiza w = 0.5.
float warp(float phase, float w);

// Seno por defecto: u = (φ − cercano(0.25,0.75))², y = ±(u·(57.341507u −
// 19.564743) + 0.9988062) (pico 0.9988, ceros exactos en 0/0.5/1).
float fallbackSine(float phase);

// Valor de la forma `s` en `phase` (fase ya plegada). `warp`/`morph` sólo se usan en
// WarpedMorphSine.
float evaluate(Shape s, float phase, float warp, float morph);

} // namespace lfo

// LFO de fase normalizada de un motor y canal. Orden del motor: primero avanza la
// fase (`φ += Rate/sr`, wrap restando 1.0) y después evalúa la forma en φ (+ offset).
class ModLfo {
public:
    void prepare(double sampleRate) { sampleRate_ = sampleRate > 0.0 ? sampleRate : 44100.0; }
    void reset(float phase = 0.0f) { phase_ = fold(static_cast<double>(phase)); }

    // `rateHz` ya está en Hz (el parámetro del modelo).
    void setRate(float rateHz) { inc_ = static_cast<double>(rateHz) / sampleRate_; }
    void setShapeFromWaveShape(int waveShape) { shape_ = lfo::shapeFromWaveShape(waveShape); }
    // El motor instala el warp con (1 − WaveWarp); WaveWarp = 0.5 ⇒ identidad.
    void setWaveWarp(float waveWarp) { warp_ = 1.0f - waveWarp; }
    void setMorph(float morph) { morph_ = morph; }

    // Avanza una muestra y devuelve la forma evaluada en (fase + offset).
    float next(float phaseOffset = 0.0f) {
        phase_ += inc_;
        phase_ = fold(phase_);
        return valueAt(phaseOffset);
    }

    // Forma en la fase actual desplazada, sin avanzar (el canal 1 del wobbler usa el mismo
    // motor con el offset de StereoPhase).
    float valueAt(float phaseOffset) const {
        return lfo::evaluate(shape_, lfo::fold01(static_cast<float>(phase_) + phaseOffset), warp_,
                             morph_);
    }

    float phase() const { return static_cast<float>(phase_); }
    lfo::Shape shape() const { return shape_; }

private:
    static double fold(double p) {
        p -= static_cast<double>(static_cast<long long>(p));
        if (p >= 1.0) p -= 1.0;
        if (p < 0.0) p += 1.0;
        return p;
    }

    double sampleRate_ = 44100.0;
    double phase_ = 0.0;
    double inc_ = 0.0;
    lfo::Shape shape_ = lfo::Shape::FallbackSine;
    float warp_ = 0.5f;
    float morph_ = 0.5f;
};

class WobblerBlock : public iBlock {
public:
    explicit WobblerBlock(const ParamStore* params) : params_(params) {}

    void prepare(const PrepareContext& ctx) override;
    void reset() override;
    void process(const PortData* in, PortData& out) override;

    // --- leyes cerradas, expuestas para los tests (tests/test_modulation.cpp) ---
    // D = 2·floor(0.02·sr) + 1: 1765 @44.1k, 1921 @48k, 3529 @88.2k, 3841 @96k.
    static int delayRangeSamples(double sampleRate);
    // c = exp(−2π·10/sr) del one-pole que suaviza el retardo.
    static float smoothingCoef(double sampleRate);
    // mod = clamp(clamp(m + Depth·(L_A+1)/2, 0, Mix) + Depth2·(L_B+1)/2, 0, 1).
    static float modulation(float depth, float depth2, float la, float lb, float mix,
                            float noise = 0.0f);
                            // Interpolación del motor: Catmull-Rom de 4 puntos para d ≥ 1, lineal para d < 1.
                            // (a,b,c,e) =, f = d − floor(d).
    static float catmullRom(float a, float b, float c, float e, float f);
    static float linearInterp(float b, float c, float f);

    // Retardo suavizado de la última muestra procesada (diagnóstico y tests).
    float currentDelay(int ch) const { return d_[ch < 0 ? 0 : (ch > 1 ? 1 : ch)]; }

    // Tamaño del anillo: next_pow2(D) (2048 @44.1 kHz).
    int ringSize() const { return static_cast<int>(ring_[0].size()); }

private:
    void clearRings();

    const ParamStore* params_ = nullptr;
    double sampleRate_ = 44100.0;

    int rangeSamples_ = 1765;   // D  (0xf30)
    float coefC_ = 0.0f;       // c = exp(−2π·10/sr)  (0x10a8)
    float coefK_ = 1.0f;       // k = 1 − c           (0x10a0)

    std::vector<float> ring_[kMaxChannels];
    int mask_ = 0;             // n − 1 (0x7ff @44.1 kHz)
    int write_[kMaxChannels] = {0, 0};
    float d_[kMaxChannels] = {1.0f, 1.0f};   // retardo suavizado por canal (0x108c)

    ModLfo lfoA_;   // wow     (Rate,  fase 0)
    ModLfo lfoB_;   // flutter (Rate2, fase 0)

// Fade de salida de 10 bloques al desactivar (0x1524: 640 muestras = 14.5 ms).
    static constexpr int kFadeBlocks = 10;
    int fadeBlocks_ = 0;
    bool active_ = false;
};

} // namespace rtxx
