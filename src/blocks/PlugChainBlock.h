#pragma once
// Etapas del DSL `plugchain` (fases 2 y 3 del ROADMAP).
//
// La cadena de cada nodo plugchain (EQ_Post, EQ_Post_Tone, Dist, EQ de 2 bandas) es texto
// tabulado que El motor interpreta en tiempo de creación. Aquí se interpreta con el
// parser ya verificado (`parsePlugChain`) y se instancian las etapas de FILTRO, GANANCIA
// y DISTORSIÓN.
//
// Fase 2: `gain`, `hp12`, `lp12`, `hp6`, `lp6`, `bpPeak`, `hpShelf`.
// Fase 3: `distort`
// y `magicDistort`, con la
//
// Los coeficientes de filtro salen de `dsp::Biquad`, con diseño bilineal prewarpado.
#include <string>
#include <vector>

#include "block/Block.h"
#include "block/PlugChain.h"
#include "dsp/Biquad.h"
#include "model/ParamStore.h"

namespace rtxx {

class PlugChainBlock : public iBlock {
public:
    // `text` es la cadena del DSL tal cual (kPlugChainMasterFilter, ...).
    // `prefix` es el prefijo de módulo con el que el DSL nombra los parámetros
    // ("EQ_Post_", "EQ_Post_Tone_", "Dist_"): en el modelo llevan ese prefijo.
    PlugChainBlock(const ParamStore* params, const std::string& text,
                   const std::string& prefix = std::string());
    ~PlugChainBlock() override;

    void prepare(const PrepareContext& ctx) override;
    void reset() override;
    void process(const PortData* in, PortData& out) override;

    // Diagnóstico (para tests y para el reporte de cobertura).
    int stageCount() const;
    int implementedStageCount() const;
    int unsupportedStageCount() const;
    const std::string& parseError() const { return parseError_; }
    // Nombre del tipo de etapa i ("hp12", "gain", "distort", ...).
    std::string stageKind(int i) const;
    // Valor de un parámetro local del módulo (con prefijo si hace falta).
    float lookup(const std::string& id) const;

    enum class Kind { Gain, Hp12, Lp12, Hp6, Lp6, BpPeak, HpShelf, Distort, MagicDistort, Unsupported };

// Valores del DSL ya resueltos y en las unidades que espera el DSP del motor
    // (amount 0..100, frecuencias en Hz, niveles en dB, mix en %).
    struct DistParams {
        float amount = 0.0f;
        float lowFreq = 20.0f;
        float highFreq = 20000.0f;
        float distLP = 20000.0f;
        float refLevel = 0.0f;
        float distDB = 0.0f;
        float passDB = 0.0f;
        float mix = 100.0f;
        int shape = 0;
        bool legacy = true;   // magicDistort: useLegacyBehaviour == 0 ⇒ rama legacy
    };

    // Sección de 1er orden de la casa: coeficientes en base+0x00/0x04/0x08 y dos estados
    // por canal en base+0x0c+4·ch y base+0x14+4·ch.
    //   y[n] = a·y[n-1] + b·u[n] + c·u[n-1]
    struct OnePoleSection {
        float a = 0.0f, b = 0.0f, c = 0.0f;
        float y1[kMaxChannels] = {};
        float u1[kMaxChannels] = {};
        // A = c, B = (1+c)/2, C = -(1+c)/2  → paso-alto/DC-block (P+0x4b0, 0x4cc, 0x2a8)
        void setHighPass(float coeff);
        // A = c, B = (1-c)/1.12, C = 0.12·B → paso-bajo con shelves (P+0x4e8, 0x504, ...)
        void setLowShelf(float coeff);
        void set(float aa, float bb, float cc) { a = aa; b = bb; c = cc; }
        void reset() {
            for (int i = 0; i < kMaxChannels; ++i) y1[i] = u1[i] = 0.0f;
        }
        float process(int ch, float u) {
            const float y = a * y1[ch] + b * u + c * u1[ch];
            y1[ch] = y;
            u1[ch] = u;
            return y;
        }
    };

// cAESingleBandDistortion (shapes 0 Crunch, 1 Hard, 2 Zap). Coeficientes.
    struct DistortDsp {
        float lo[kMaxChannels] = {};
        float band[kMaxChannels] = {};
        float wet[kMaxChannels] = {};
        float dry = 1.0f;        // P+0x38 = 1 - mix/100
        float distGain = 1.0f;   // P+0x3c = 10^(distDB/20) · m
        float passGain = 0.0f;   // P+0x40 = 10^(passDB/20) · m
        float d = 1.0f;          // P+0x44 = 10^(-refLevel/20)
        float a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
        float k = 0.0f;          // P+0x60
        float scale = 1.0f;      // P+0x68 (norm/scale de cada shape)
        float thirdK = 0.0f;     // P+0x64 = k/3 (shape 1)
        float dcOffset = 0.0f;   // P+0x6c = c   (shape 1)
        int shape = 0;
        void configure(const DistParams& p, double sampleRate);
        void reset();
        // Sólo limpia la envolvente, como el bypass del motor.
        void clearEnvelope();
        float process(int ch, float x);
    };

// cAEMagicDistortion (shapes 0 TubePair, 1 AirPressure, 2 IronTransformer). Update.
    struct MagicDistortDsp {
    static constexpr int kDelaySize = 16; // ring de 16 muestras/canal
        float lo[kMaxChannels] = {};
        float band[kMaxChannels] = {};
        float wet[kMaxChannels] = {};
        float mixTarget = 1.0f;   // P+0x38 = mix/100
        float distGain = 1.0f;    // P+0x3c = 10^(distDB/20)
        float passGain = 1.0f;    // P+0x40 = 10^(passDB/20)
        float a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
        // suavizado 3-tap de la mezcla: coeficientes P+0x674/678/67c, historial P+0x680/688.
        // Coeficientes medidos (0.86720848, 0.11890635, 0.01420714); DC = 1.0 exacto,
        // así que con la mezcla
        // quieta el filtro converge a mix/100.
        float mixC0 = 0.8672084808349609f;
        float mixC1 = 0.11856400221586227f;
        float mixC2 = 0.014227662235498428f;
        float mixH1[kMaxChannels] = {}, mixH2[kMaxChannels] = {};
        int shape = 0;
        // --- shape 0 TubePair (base P+0x4b0)
        float g1Target = 0.0f, g2Target = 0.0f;   // P+0x53c / P+0x540
        OnePoleSection tpG1, tpG2, tpHpFix, tpHpGap, tpShelf;
        // --- shape 1 AirPressure (base P+0x288)
        float apGainTarget = 0.0f, apCoefATarget = 0.0f, apCoefBTarget = 0.0f;
        OnePoleSection apGain, apCoefB, apCoefA, apHp20;
        float apAcc[kMaxChannels] = {};
        float apDelay[kMaxChannels][kDelaySize] = {};
        float apDelayPrev[kMaxChannels] = {};
        int apWrite = 0;
        float apDelaySamples = 1.0f;   // P+0x338 = sr/44100
        float apLegacyGain = 1.0f;     // P+0x33c = 44100/sr
        bool legacy = true;
        // --- shape 2 IronTransformer (base P+0x84)
        float ironGainTarget = 0.0f, ironCoefATarget = 0.0f, ironCoefBTarget = 0.0f;
        OnePoleSection ironGainSmooth, ironCoefASmooth, ironCoefBSmooth, ironLp, ironShelf;
        float ironAcc[kMaxChannels] = {};
        float ironAtan[kMaxChannels] = {};
        void configure(const DistParams& p, double sampleRate);
        void reset();
        void clearEnvelope();
        float process(int ch, float x);

    private:
        float tubePair(int ch, float band);
        float airPressure(int ch, float band);
        float ironTransformer(int ch, float band);
    };

private:
    struct Stage {
        Kind kind = Kind::Unsupported;
        std::string kindName;
        const Node* node = nullptr;
        bool active = false;
        float gain = 1.0f;          // para Kind::Gain (lineal)
        dsp::Biquad bq[kMaxChannels];
        // último valor aplicado, para no recalcular coeficientes cada quantum
        double lastFreq = -1.0, lastQ = -1.0, lastDb = 0.0;
        bool lastActive = false;
        // fases 3: parámetros resueltos del DSL y DSP de cada clase
        DistParams dp;
        DistParams lastDp;
        bool lastDpValid = false;
        DistortDsp dist;
        MagicDistortDsp magic;
    };

    void updateStage(Stage& s);
    // Rellena `dp` desde las líneas del DSL de la etapa (distort / magicDistort).
    void resolveDistParams(const Node& node, Kind kind, DistParams& dp) const;
    double resolveParam(const ParamLine* line, double fallback) const;
    // Valor real de una línea del DSL: resuelve el parámetro del modelo y aplica
    // min/max/scale de la línea si el modelo lo guarda normalizado (0..1).
    double lineValue(const ParamLine* line, double fallback) const;
    // `curve` es el número que sigue a la palabra de escala (`sym 0.15`, `sym 0.4`).
    static double applyScale(double v, double lo, double hi, const char* scale, double curve = 0.5);

    const ParamStore* params_ = nullptr;
    std::string prefix_;
    Chain chain_;
    std::vector<Stage> stages_;
    std::string parseError_;
    double sampleRate_ = 44100.0;
};

} // namespace rtxx
