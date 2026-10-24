#pragma once
// cSimpleComp (nodo `NoiseFX.SComp` / `NoisePostFX.SComp`): compresor de "ducking" del ruido.
//
//
//     Noise.out → SComp.in           (el ruido, señal principal)
//     in        → SComp.inSidechain  (el programa, que abre el compresor)
//     SComp.out → WaveViz → out
//
// y el control que lo gobierna:
// `NOIS Duck` addAutomationParamINORDER('NOIS Duck', 'SComp_Threshold')
// el knob "Duck" escribe `SComp_Threshold` (0..100 %)
// la tabla "The noise level is pushed down by incoming audio peaks,
//                                 mimicking the effect of a 'pumping' compressor with slow attack"
// `syncedEnableParams = { "Noise_Enable", "SComp_Enable" }`
//
// Parámetros del modelo que NO mueve el host:
// Ratio 0,5 · Averaging 0 · GainComp 0 · Attack 0 · Release 0,2 · HiPassSidechain 1,0.
//
// Salida: `out = ruido·g`. El sidechain es SÓLO el detector: el seco viaja por su propia
// arista, así que sumarlo aquí lo duplicaría
// (medido: con `NOIS Amount` bajo el "ruido" diferencial caía al nivel del programa).
//
// Forma exacta de las curvas de SComp_Threshold/Ratio/Attack/Release: no está identificada.
// Se calibran con medidas de la suite y así se declara.

#include <vector>

#include "block/Block.h"
#include "blocks/Noise.h"

namespace rtxx {

class SimpleCompBlock : public iBlock {
public:
    explicit SimpleCompBlock(const ParamSource* params);
    void prepare(const PrepareContext& ctx) override;
    void reset() override;
    void process(const PortData* in, PortData& out) override;
    void declarePorts(std::vector<std::string>& ports) const override;
    void attachPort(const std::string& name, PortData* view) override;

// Leyes MEDIDAS:
    //   atenuación_dB = 0.637·(L_side − T(d)),  T(d) = −4.04 − 47.31·d  dBFS,  ratio 2.76:1
    //   attack 5–9 ms, release 156–245 ms, sidechain paso-alto de 2.º orden fc 50.5 Hz Q 0.55.
    static float thresholdDbFromParam(float d);  // d = SComp_Threshold/100 (0..1) → umbral dBFS
    static float duckAttenuationDb(float sideDb, float d);   // atenuación (≥ 0 dB)
    static float releaseSeconds(float d);

private:
    const ParamSource* params_ = nullptr;
    double sr_ = 44100.0;
    float env_[2] = {};        // envolvente del sidechain (pico, con attack/release)
    float envDb_[2] = {};
    float gain_[2] = {1.0f, 1.0f};
    float duckDb_[2] = {};     // atenuación suavizada (dB)
    const PortData* sc_ = nullptr;   // vista de "inSidechain" (el programa)
    Biquad scHp_[2];           // paso-alto del sidechain (2.º orden, fc 50,5 Hz Q 0,55)
    float scHpHz_ = 0.0f;
};

} // namespace rtxx
