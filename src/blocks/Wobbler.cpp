#include "blocks/Wobbler.h"

#include <cmath>

#include "dsp/Math.h"

namespace rtxx {

namespace lfo {

Shape shapeFromWaveShape(int waveShape) {
const int idx = waveShape - 1; // (uint)(v-1) < 0xb
    if (idx < 0 || idx >= kTableSize) return Shape::FallbackSine;
    return static_cast<Shape>(idx + 1);
}

float fold01(float phase) {
    float x = phase - std::floor(phase);
    if (x >= 1.0f) x = 0.0f;
    if (x < 0.0f) x = 0.0f;
    return x;
}

float warp(float phase, float w) {
    const float p = fold01(phase);
    // con w == 0.5 (1 − WaveWarp con WaveWarp = 0.5) es la identidad.
    if (w == 0.5f || p == 0.0f) return p;
    // [?] Tramo deformado no recuperado (WaveWarp no es control de usuario: no existe en
    // la tabla). Se usa el reparto lineal de dos tramos con el mismo punto de ruptura,
    // que coincide con la identidad en w = 0.5; no lo usa ningún caso medido.
    const float b = dsp::clamp(w, 0.001f, 0.999f);
    return p < b ? 0.5f * p / b : 0.5f + 0.5f * (p - b) / (1.0f - b);
}

float fallbackSine(float phase) {
// u = (φ − cercano(0.25,0.75))², y = signo(φ)·(u·(57.341507u − 19.564743) +
    // 0.9988062). Ceros exactos en 0/0.5/1, extremos ±0.9988062 en 0.25/0.75.
    const float p = fold01(phase);
    const float d = p < 0.5f ? p - 0.25f : p - 0.75f;
    const float u = d * d;
    const float y = u * (57.341507f * u - 19.564743f) + 0.9988062f;
    return p < 0.5f ? y : -y;
}

namespace {

// Escalones de la tabla §2 del informe: niveles repartidos en tramos iguales de la fase.
// 3 niveles: −1/0/+1 (tramos 1/3 y 2/3); 4: −1/−1/3/+1/3/+1; 6: −1/−0.6/−0.2/+0.2/+0.6/+1.
float stairsUp(float p, int levels) {
    if (levels == 3) return p < 1.0f / 3.0f ? -1.0f : (p < 2.0f / 3.0f ? 0.0f : 1.0f);
    const float span = 2.0f / static_cast<float>(levels - 1);
    const int k = dsp::clamp(static_cast<int>(p * static_cast<float>(levels)), 0, levels - 1);
    return -1.0f + span * static_cast<float>(k);
}

float stairsDown(float p, int levels) { return -stairsUp(p, levels); }

} // namespace

float evaluate(Shape s, float phase, float w, float morph) {
    (void)w;   // el warp sólo entra por la fase ya deformada (ver warpedMorphSine)
    const float p = fold01(phase);
    switch (s) {
        case Shape::FallbackSine:
            return fallbackSine(p);
        case Shape::WarpedMorphSine:
            // [?] El tramo intermedio del *morph* (0 < m < 1) no está recuperado; los dos
            // extremos sí (m ≤ 0 → seno, m ≥ 1 → ±1). Como WaveShape no es control de
            // usuario, el estado de fábrica es el fallback y este slot no se mide.
            if (morph <= 0.0f) return fallbackSine(p);
            if (morph >= 1.0f) return p < 0.5f ? 1.0f : -1.0f;
            return fallbackSine(warp(p, morph));
        case Shape::Triangle:
            if (p < 0.25f) return 4.0f * p;
            if (p < 0.75f) return 4.0f * (0.5f - p);
            return 4.0f * (p - 1.0f);
        case Shape::Square:
            return p < 0.5f ? 1.0f : -1.0f;
        case Shape::SawUp:
            return 2.0f * (p - 0.5f);
        case Shape::SawDown:
            return 2.0f * (0.5f - p);
        case Shape::Stairs3Up:
            return stairsUp(p, 3);
        case Shape::Stairs3Down:
            return stairsDown(p, 3);
        case Shape::Stairs4Up:
            return stairsUp(p, 4);
        case Shape::Stairs4Down:
            return stairsDown(p, 4);
        case Shape::Stairs6Up:
            return stairsUp(p, 6);
        case Shape::Stairs6Down:
            return stairsDown(p, 6);
    }
    return fallbackSine(p);
}

} // namespace lfo

int WobblerBlock::delayRangeSamples(double sampleRate) {
// D = 2·floor(0.02·sr) + 1: 40,02 ms de recorrido máximo a cualquier sr.
    const double sr = sampleRate > 0.0 ? sampleRate : 44100.0;
    const int half = static_cast<int>(0.02 * sr);
    return 2 * half + 1;
}

float WobblerBlock::smoothingCoef(double sampleRate) {
    const double sr = sampleRate > 0.0 ? sampleRate : 44100.0;
    return static_cast<float>(std::exp(-2.0 * M_PI * 10.0 / sr));
}

float WobblerBlock::modulation(float depth, float depth2, float la, float lb, float mix,
                               float noise) {
                               // en el orden del motor: ruido + wow, recorte a [0,Mix], flutter,
    // recorte a [0,1].
    float m = noise + depth * (la + 1.0f) * 0.5f;
    m = dsp::clamp(m, 0.0f, mix);
    m += depth2 * (lb + 1.0f) * 0.5f;
    return dsp::clamp(m, 0.0f, 1.0f);
}

float WobblerBlock::catmullRom(float a, float b, float c, float e, float f) {
// con (a,b,c,e) =.
    return c + f * 0.5f *
                   ((b - e) + f * (((2.0f * e - 5.0f * c + 4.0f * b) - a) +
                                   f * ((3.0f * c + (a - 3.0f * b)) - e)));
}

float WobblerBlock::linearInterp(float b, float c, float f) { return c + f * (b - c); }

void WobblerBlock::prepare(const PrepareContext& ctx) {
    sampleRate_ = ctx.sampleRate > 0.0 ? ctx.sampleRate : 44100.0;
    rangeSamples_ = delayRangeSamples(sampleRate_);
    coefC_ = smoothingCoef(sampleRate_);
    coefK_ = 1.0f - coefC_;

// Anillo del motor: n = next_pow2(D), máscara n−1 (2048/0x7ff @44.1 kHz).
    int n = 1;
    while (n < rangeSamples_) n <<= 1;
    mask_ = n - 1;
    for (int c = 0; c < kMaxChannels; ++c) {
        ring_[c].assign(static_cast<size_t>(n), 0.0f);
        write_[c] = 0;
    }

    lfoA_.prepare(sampleRate_);
    lfoB_.prepare(sampleRate_);
    reset();
}

void WobblerBlock::clearRings() {
    for (int c = 0; c < kMaxChannels; ++c)
        for (size_t i = 0; i < ring_[c].size(); ++i) ring_[c][i] = 0.0f;
    for (int c = 0; c < kMaxChannels; ++c) {
        write_[c] = 0;
        d_[c] = 1.0f;
    }
}

void WobblerBlock::reset() {
    clearRings();
    // Fase inicial ajustado por medida (no 0): con Rate en 0.1/1 Hz el retardo arranca
    // en su MÍNIMO (L = −1) y sube por el flanco ascendente, que es la forma en φ = 0.75
    // El informe previo decía 0; la medición manda.
    lfoA_.reset(0.75f);
    lfoB_.reset(0.75f);
    fadeBlocks_ = 0;
    active_ = false;
}

void WobblerBlock::process(const PortData* in, PortData& out) {
    const int n = out.numSamples > 0 ? out.numSamples : (in ? in->numSamples : 0);
    if (n <= 0 || !in || in->numChannels < 1) return;
    const int nch = in->numChannels < kMaxChannels ? in->numChannels : kMaxChannels;

// Parámetros del bloque.
    const float enable = params_ ? params_->get("Wobble_Enable") : 0.0f;
    const float depth = dsp::clamp(params_ ? params_->get("Wobble_Depth") : 0.0f, 0.0f, 0.8f);
    const float depth2 = dsp::clamp(params_ ? params_->get("Wobble_Depth2") : 0.0f, 0.0f, 0.2f);
    const float mix = dsp::clamp((params_ ? params_->get("Wobble_Mix") : 100.0f) / 100.0f, 0.0f, 1.0f);
    const float rate = params_ ? params_->get("Wobble_Rate") : 0.0f;
    const float rate2 = params_ ? params_->get("Wobble_Rate2") : 0.0f;
    const float stereoPhase = dsp::clamp(params_ ? params_->get("Wobble_StereoPhase") : 0.0f, 0.0f, 1.0f);

    lfoA_.setRate(rate);
    lfoB_.setRate(rate2);
    // WaveShape / WaveWarp / WaveShapeMorph no son parámetros de la tabla (no hay control
    // de usuario ni id de modelo): el estado de fábrica es la forma por defecto y warp neutro.
    lfoA_.setShapeFromWaveShape(0);
    lfoB_.setShapeFromWaveShape(0);
    lfoA_.setWaveWarp(0.5f);
    lfoB_.setWaveWarp(0.5f);

    const float* src[kMaxChannels] = {in->ch[0], nch > 1 ? in->ch[1] : in->ch[0]};
    float* dst[kMaxChannels] = {out.ch[0], nch > 1 ? out.ch[1] : out.ch[0]};

    // Ciclo de vida: Enable < 0.5 ⇒ rampa seca de 10 bloques y off (0x1524 = 640 muestras);
    // al reactivar se vacía el anillo (cola de silencio de D muestras).
    float wetGate = 1.0f;
    if (enable < 0.5f) {
        if (active_) {
            if (fadeBlocks_ <= 0) fadeBlocks_ = kFadeBlocks;
            wetGate = static_cast<float>(fadeBlocks_) / static_cast<float>(kFadeBlocks);
            if (--fadeBlocks_ <= 0) active_ = false;
        } else {
            wetGate = 0.0f;
        }
    } else if (!active_) {
        clearRings();
        active_ = true;
    }
    if (!active_) {   // bypass seco
        for (int i = 0; i < n; ++i)
            for (int c = 0; c < nch; ++c) dst[c][i] = src[c][i];
        return;
    }

    const float b = (depth == 0.0f && depth2 == 0.0f) ? 0.0f : 1.0f;
    const float D = static_cast<float>(rangeSamples_);

    for (int i = 0; i < n; ++i) {
        const float la0 = lfoA_.next(0.0f);
        const float lb0 = lfoB_.next(0.0f);
        const float la1 = stereoPhase != 0.0f ? lfoA_.valueAt(stereoPhase) : la0;
        for (int c = 0; c < nch; ++c) {
            const float la = (c == 1) ? la1 : la0;
            const float mod = modulation(depth, depth2, la, lb0, mix);
            const float d = coefK_ * (b + mod * D) + coefC_ * d_[c];
            d_[c] = d;

            const float x = src[c][i];
            write_[c] = (write_[c] + 1) & mask_;
            ring_[c][static_cast<size_t>(write_[c])] = x;

            const int di = static_cast<int>(d);
            const float f = d - static_cast<float>(di);
            const int idx = (write_[c] - di) & mask_;
            const float cSample = ring_[c][static_cast<size_t>(idx)];
            float y;
            if (d >= 1.0f) {
                const float a = ring_[c][static_cast<size_t>((idx - 2) & mask_)];
                const float bm = ring_[c][static_cast<size_t>((idx - 1) & mask_)];
                const float e = ring_[c][static_cast<size_t>((idx + 1) & mask_)];
                y = catmullRom(a, bm, cSample, e, f);
            } else {
                const float bm = ring_[c][static_cast<size_t>((idx - 1) & mask_)];
                y = linearInterp(bm, cSample, f);
            }
            // Mezcla seco/húmedo (G = 1: el término LfoNoise que la escala depende del flujo
            // de *flux*, no reconstruido ⇒ [?]).
            const float mixed = (1.0f - mix) * x + mix * y;
            dst[c][i] = (1.0f - wetGate) * x + wetGate * mixed;
        }
    }
}

} // namespace rtxx
