#pragma once
// cResoX (nodo `Space` del grafo): banco de 12 resonadores en paralelo por canal + 4 allpass en
// serie + pre-delay con interpolación de 4 puntos.
//
// NO es `cResoReverbBlock`, que es otra clase (parámetros y topología distintas).
//
// Ecuaciones por muestra y canal:
// x_k = línea_k[lectura] 12 líneas, N_k = sr/f_k
// y_k = (x_k − x_k⁻¹)·c_d + y_k⁻¹·g_d Dampen = paso-alto en el lazo
// z_k = 0,6·x_k + 0,4·y_k
// u_k = z_k·c_s + u_k⁻¹·g_s Soften = paso-bajo en el lazo
// w_k = A·u_k + B·z_k mezcla de los dos caminos del lazo
// línea_k[escritura] = in_pd − w_k·G_k realimentación NEGATIVA
// wet += w_k suma de las 12
// allpass ×4: v = ap[lectura]; ap[escritura] = wet + 0,5·v; wet = v − wet
// salida = dry·x + wet·wet_sum crossfade de 16 muestras
//
// Frecuencias de las líneas (ley de nota MIDI + detune en semitonos):
//   f_k = 440·2^((22 + k − 69)/12)                29,14 … 55,00 Hz (MIDI 22..33)
//   N_k = round(sr/f_k)  (canal 1 con +Δ/−Δ semitonos, Δ = StereoMode·StereoDetune)
// capacidad = floor(sr/f_k⁰) + 1 (los buffers NO son potencia de 2)
//
// Allpass: longitudes {0,0126077095, 0,01, 0,0077324263, 0,0051020407}·sr
// ±(0,00026077096/2)·sr por canal, ganancia 0,5.
//
// Decay: la ganancia de cada línea sale de la ley de T60 ajustada a la medida
//   T60(Decay) = 1/(0,847 − 0,833·Decay)  s     (1 kHz; 1,18 s a Decay 0 y 5,49 s a 0,8)
//   G_k = 10^(−3·N_k/(sr·T60))                 ⇒ decaimiento uniforme en todas las líneas
// y la dependencia por banda sale del filtro del lazo (Dampen/Soften).
//
// [?] Provisional, no verificado: el blend A/B de los dos caminos (se usa 0,3/0,7 porque es
// lo que reproduce el T60 medido), las ganancias por línea `R%d%d_%d` (no expuestas al host
// ⇒ se usa decaimiento uniforme por línea), la ganancia de entrada de la red, la tabla de
// crossfade de 16 muestras, la LFO del pre-delay (`PreWobbleNoise`, generador de flux) y los
// parámetros internos `DualMono`/`EnableFeedBacks` (no expuestos al host; defaults 0 y 1, y
// con `EnableFeedBacks = 1` el motor sólo limpia los medidores por línea, no el lazo).
#include <vector>

#include "block/Block.h"
#include "model/ParamStore.h"

namespace rtxx {

class ResoXBlock : public iBlock {
public:
    explicit ResoXBlock(const ParamStore* params) : params_(params) {}

    void prepare(const PrepareContext& ctx) override;
    void reset() override;
    void process(const PortData* in, PortData& out) override;

    // --- leyes cerradas, expuestas para los tests (tests/test_resox.cpp) ---
    static double lineFreqHz(int k);                        // 440·2^((22+k−69)/12)
    static double allpassSeconds(int i);                    // {0,0126077095, …}
    static double t60FromDecay(float decay);                // 1/(0,847 − 0,833·d)  [medida]
    static float loopGain(float t60, int samples, double sr);// 10^(−3·N/(sr·T60))
    static void dryWet(float level, float& dry, float& wet); // wet = rsmul(level, 0,233)  [medida]
    static float rsmul(float x, float c) {
        return c * x / (((2.0f * c - 1.0f) * x) + 1.0f - c);
    }
    static float filterGain(float hz, double sr);           // g = exp(−2π·hz/sr)

    struct Diag {
        int lines = 0;
        int samples[12] = {};
        int allpass[4] = {};
        int predelaySamples = 0;
    };
    const Diag& diag() const { return diag_; }

private:
    static constexpr int kLines = 12;
    static constexpr int kAllpass = 4;
    static constexpr int kCh = 2;
    static constexpr int kPdSize = 8192;   // 2^13: el buffer del pre-delay es potencia de 2

    struct Line {
        std::vector<float> buf;
        int size = 2;
        int idx = 0;
        float xPrev = 0.0f, yPrev = 0.0f, uPrev = 0.0f, gain = 0.0f;
    };
    struct Allpass {
        std::vector<float> buf;
        int size = 2;
        int idx = 0;
    };

    const ParamStore* params_ = nullptr;
    double sampleRate_ = 44100.0;
    Line lines_[kCh][kLines];
    Allpass ap_[kCh][kAllpass];
    float pd_[kCh][kPdSize] = {};
    int pdIdx_[kCh] = {0, 0};
    float pdDelay_[kCh] = {1.0f, 1.0f};
    float pdCoef_ = 0.01f;          // coeficiente del 1-polo (provisional, [?] offset 0x54)
    Diag diag_;
};

} // namespace rtxx
