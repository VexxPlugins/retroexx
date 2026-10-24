#pragma once
// cBitColor (nodo `Digital` del grafo): reducción de resolución (bit crush) y de frecuencia
// de muestreo (sample-and-hold), con los filtros de foco y el limitador de slew de antialias.
//
// Ecuaciones cerradas (todas por muestra y canal):
// bitsS = suavizado a 320 Hz de BitDepth
// SCALE = 2^(bitsS−1) (el armónico de cuantización cae un bit por debajo de 2^bits)
// gain = 1 − 0.8·(1 − (1 − 2^(−0.57·(bitsS−1)))^1.5)
// sesgo = 0.5 − 0.499·RoundError (half-away-from-zero)
// q(x) = trunc(x·SCALE + signo(x)·sesgo) / SCALE · gain
// S&H = anillo de 32 posiciones (`& 0x1f`), avance `factor` por muestra
//            factor = clamp((lo + (hi−lo)·(s·0.5+0.5))⁴, 0, 8)
//            lo = ((degSR·(1−depth)·32)/sr)^0.25, hi = ((degSR·(1+depth)·32)/sr)^0.25
// ⇒ la tasa efectiva de retención es 32·DegradeSR
// objetivo = filtrado/(1 − 0.77·Feedback) + 1
// mezcla S&H = w = clamp(frac/(AliasSmoothing+1e-5), 0, 1)
// antialias AA2 = limitador de slew con paso 1/(k²·0.008·sr), k = AA2·A·(1−u)
// antialias AA3 = one-pole con esquina ((w·0.04)/(1−0.92w−0.04))·19800+200
// filtros FilterLo (paso-alto TPT) y FilterHi (one-pole paso-bajo)
// salida = x + Mix·((Focus·(x − filtrado) + OutGain·gananciaPrevio·q) − x)
//
// Host→modelo: con A = min(1,2(1−x)) y B = min(1,2x) sobre el balance `Digital_Xfade`,
// `DegradeSR = fit3(macro·A, 44100, 200, 44.1)` y `BitDepth = fit3(macro·B, 24, 6, 1)`.
//
// [?] No implementado: el previo (PreampDrive/SlewRate/Knee/PhaseReversal) — sus parámetros
// no existen en el modelo y `PreampDriveEnable` vale 0 de fábrica, así que la etapa no se
// ejecuta; las señales de *flux* (los suavizados a 320/10 Hz de DegradeSR, BitDepth,
// FilterLo/Hi, Focus y mix); y el resto de la ley del mapeo de tipo.
//
// [?] POSICIÓN DE LOS FILTROS, PROVISIONAL. Con los filtros abiertos (20 Hz / 20 kHz, valores
// de fábrica) el bloque no colorea la señal; con `DIGI Filter Lo/Hi` en posiciones intermedias
// el mapeo `log 20..20000` NO está verificado: el corte real se mueve con el estado del macro,
// lo que apunta a que al bloque le entra el valor NORMALIZADO mezclado con el cruce interno.
// No se declara verificado.
#include <cmath>

#include "block/Block.h"
#include "model/ParamStore.h"

namespace rtxx {

class BitColorBlock : public iBlock {
public:
    explicit BitColorBlock(const ParamStore* params) : params_(params) {}

    void prepare(const PrepareContext& ctx) override;
    void reset() override;
    void process(const PortData* in, PortData& out) override;

    // --- leyes cerradas, expuestas para los tests (tests/test_digital.cpp) ---
    // Sesgo del redondeo: 0.5 − 0.499·RoundError (RoundError ∈ [−1,1]).
    static float biasMagnitude(float roundError) { return 0.5f - 0.499f * roundError; }
    // Escala del cuantizador y ganancia de compensación. El exponente efectivo es
    // (bits−1): el 3.º armónico cae 6.7 dB más alto que con 2^bits, o sea un bit efectivo
    // menos, y la ganancia cuadra con esa misma corrección (0.200 a 1 bit y 0.840 a 6 bits,
    // frente a 0.20/0.8395 de la fórmula).
    static float quantScale(float bits) { return std::pow(2.0f, bits - 1.0f); }
    static float quantGain(float bits) {
        const float inv = std::pow(2.0f, -(bits - 1.0f));
        float a = 1.0f - std::pow(inv, 0.57f);
        a = a < 0.0f ? 0.0f : (a > 1.0f ? 1.0f : a);
        return 1.0f - 0.8f * (1.0f - std::pow(a, 1.5f));
    }
    // Cuantizador half-away-from-zero con el sesgo del parámetro.
    static float quantize(float x, float scale, float bias) {
        const float s = x < 0.0f ? -bias : bias;
        return std::trunc(x * scale + s) / scale;
    }
    // Factor de avance del sample-and-hold: la tasa efectiva de
    // retención es 32·DegradeSR.
    static float holdFactor(float degradeSR, float depth, double sampleRate, float lfo01);
    // Peso del antialias: A = AliasSmoothing·(f·0.8)/(f·0.6+0.2), f = clamp((degSR−20)/170).
    static float aliasWeight(float degradeSR, float aliasSmooth);
    // Coeficientes del antialias (AA2 = paso máximo del slew; AA3 = g del one-pole).
    static float aa2MaxStep(float aa2, float aliasWeight, float bits, double sampleRate);
    static float aa3Coef(float aa3, float aliasWeight, float bits, double sampleRate);
    // Coeficientes de los dos filtros: paso-alto TPT y paso-bajo one-pole.
    static void filterCoefs(float hz, double sampleRate, float& c, float& g, float& tpt);
    // Posición normalizada de un parámetro cuyo min > max (el "amount" de degradación).
    static float normDescending(float v, float lo, float hi) {
        return hi > lo ? 0.0f : (hi - v) / (hi - lo);
    }
    // Factor de cruce interno que abre los filtros en el modo Focus.
    static float internalCrossfade(float degradeSR, float bits);

    // Diagnóstico para los tests.
    int ringIndex(int ch) const { return idx_[ch < 0 ? 0 : (ch > 1 ? 1 : ch)]; }

private:
static constexpr int kRing = 32; // anillo de retención de 32 posiciones

    const ParamStore* params_ = nullptr;
    double sampleRate_ = 44100.0;

// Coeficientes de bloque.
    float hpC_ = 0.0f, hpG_ = 0.0f, hpA_ = 0.0f;   // FilterLo (paso-alto TPT)
    float lpC_ = 0.0f, lpG_ = 0.0f;               // FilterHi (one-pole paso-bajo)
    float aa2MaxStep_ = 10000.0f; // paso máximo del limitador de slew
    float aa3G_ = 0.0f, aa3Pole_ = 1.0f; // esquina del one-pole AA3
    float bitsScale_ = 1.0f, bitsInvScale_ = 1.0f, quantGain_ = 1.0f;
    float biasMag_ = 0.989f;
    float kFeedback_ = 1.0f, fbRetention_ = 0.0f;
    float mixOut_ = 1.0f, focus_ = 1.0f;
    float aliasNorm_ = 0.0f;                     // AliasSmoothing normalizado
    float outGain_ = 1.0f;
    float holdFactor_ = 32.0f;                   // avance del anillo por muestra
    float lfoPhase_ = 0.0f;

    // Estado por canal.
    float ring_[kMaxChannels][kRing] = {};
    int idx_[kMaxChannels] = {0, 0};
    float acc_[kMaxChannels] = {0.0f, 0.0f};
    float hpX_[kMaxChannels] = {0.0f, 0.0f}, hpY_[kMaxChannels] = {0.0f, 0.0f};
    float hpX2_[kMaxChannels] = {0.0f, 0.0f}, hpY2_[kMaxChannels] = {0.0f, 0.0f};
    float lpY_[kMaxChannels] = {0.0f, 0.0f}, lpY2_[kMaxChannels] = {0.0f, 0.0f};
    float slew_[kMaxChannels] = {0.0f, 0.0f}, postLp_[kMaxChannels] = {0.0f, 0.0f};
    float bitsState_ = 0.0f;   // suavizado a 320 Hz de BitDepth
};

} // namespace rtxx
