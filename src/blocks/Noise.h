#pragma once
// cNoiseFX (nodo `NoiseFX.Noise` / `NoisePostFX.Noise`): generador de ruido de la sección NOIS.
//
// Estructura del contenedor:
//
//     in → Noise(cNoiseFX) → SComp(cSimpleComp) → WaveViz → out
//     in → SComp.inSidechain
//
// y la lista de TIPOS y sus parámetros:
//     Noise_Enable (host 7), Noise_NoiseType (host 8, índice 0..15), Noise_Tone (host 9,
//     bipolar −12..+12 dB, default 0,5), Noise_Follow (host 10, 0..100 %), Noise_Routing
//     (host 11, pre/post), Noise_RandomSeekEnable, Noise_GateLevel, Noise_NoiseLevel
//     (dB de −78 a 0 con fitMiddle −12, macro 1:1 con MacroNoise).
//
// MATERIAL PROPIO. Los 16 tipos se SINTETIZAN aquí con nuestro propio PRNG y nuestros propios
// filtros: no se usa ni un sample de terceros. Lo que se reproduce es el COMPORTAMIENTO
// (nivel RMS por tipo, pendiente espectral, hum, densidad de clics, rango dinámico), y eso es
// lo que comprueba la suite de medidas. Los números de `kRecipes` salen de esa campaña.
//
// Declarado: el DSP exacto del motor (filtros, PRNG, forma de los clics) es desconocido;
// lo que hay aquí es una síntesis propia ajustada a descriptores medidos, no una copia.

#include <cstdint>
#include <vector>

#include "block/Block.h"

namespace rtxx {

// Biquad RBJ (paso-alto, paso-bajo, peaking). Estado por canal.
struct Biquad {
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    float z1 = 0.0f, z2 = 0.0f;
    void reset() { z1 = z2 = 0.0f; }
    void highpass(double sr, double f, double q = 0.70710678);
    void lowpass(double sr, double f, double q = 0.70710678);
    void peaking(double sr, double f, double q, double gainDb);
    void lowShelf(double sr, double f, double q, double gainDb);
    void highShelf(double sr, double f, double q, double gainDb);
    float process(float x) {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

class NoiseBlock : public iBlock {
public:
    explicit NoiseBlock(const ParamSource* params);
    void prepare(const PrepareContext& ctx) override;
    void reset() override;
    void process(const PortData* in, PortData& out) override;

// Leyes MEDIDAS:
    //   Follow  ganancia_dB = clamp(L_prog − b(v), −inf, gmax(v))   (b y gmax tabulados)
    //   Tone    tilt de 2.º orden, H(x) = (2+jx)/(1+2jx), x = f/800 Hz, ±12,04 dB
    static float followB(float follow);      // b(v)
    static float followGmax(float follow);   // techo del boost (dB)
    static float followAttackSeconds(float follow);
    static float followReleaseSeconds(float follow);
    static float toneTiltAmount(float toneDb);   // dB del modelo → (1−2v) con v = v(toneDb)
    static const char* typeName(int index);
    static int typeCount();

private:
    struct Recipe {
        const char* name;
        // Trim por tipo: se suma al nivel del control (dB del modelo). Se calibra para que el RMS
        // a Amount = 1 caiga en el ajustado por medida ⇒ caracteriza la SONORIDAD
        // relativa de cada tipo, no su color.
        float trimDb;
        float whiteMix;     // pesos de las tres fuentes continuas (white/pink/brown)
        float pinkMix;
        float brownMix;
        float humAmp;       // 60 Hz + armónicos (DC/VHS/CASS/APOLLO), 0 = sin hum
        float humHz;        // fundamental del hum (50 o 60)
        float crackleRate;  // clics por segundo (0 = ninguno)
        float crackleAmp;
        float quantRate;    // sample&hold a esta tasa en Hz (0 = continuo)
        float hpHz;         // paso-alto de la cadena
        float lpHz;         // paso-bajo de la cadena
        float peakHz;       // primer peaking (0 = sin peaking)
        float peakQ;
        float peakDb;
        float peak2Hz;      // segundo peaking (0 = sin peaking)
        float peak2Q;
        float peak2Db;
        float wobbleDepth;  // LFO lento de amplitud (0..1)
        float wobbleHz;
    };
    static const Recipe& recipe(int index);

    const ParamSource* params_ = nullptr;
    double sr_ = 44100.0;
    int nch_ = 2;
    int type_ = 0;
    int lastType_ = -1;      // para re-armar la receta/estado cuando cambia el tipo
    float levelDb_ = -78.0f;
    float targetDb_ = -78.0f;

    // PRNG propio (xorshift128+): estado por canal.
    uint64_t s0_[2] = {0x9e3779b97f4a7c15ull, 0xbf58476d1ce4e5b9ull};
    uint64_t s1_[2] = {0x94d049bb133111ebull, 0xd1b54a32d192ed03ull};

    // Fuentes y filtros por canal.
    float pink_[2][7] = {};
    float brown_[2] = {};
    float humPhase_[2] = {0.0f, 0.0f};
    float crackleEnv_[2] = {0.0f, 0.0f};
    float cracklePolarity_[2] = {1.0f, -1.0f};
    float quantHold_[2] = {};
    float quantPhase_[2] = {};
    float wobblePhase_[2] = {0.0f, 0.0f};
    Biquad hp_[2], lp_[2], peak_[2], peak2_[2];
    Biquad toneA_[2], toneB_[2];     // dos etapas del tilt de `Noise_Tone`
    float followGainDb_[2] = {};     // ganancia de Follow suavizada (dB)
    float lastTone_ = 1e9f;
    float tiltDb_ = 0.0f;

    // Suavizados de 1-polo (los mismos que usa el resto de bloques).
    float toneCoef_ = 0.0f;      // suavizado del nivel (20 ms)
    float followEnv_[2] = {};
};

} // namespace rtxx
