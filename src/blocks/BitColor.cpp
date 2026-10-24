#include "blocks/BitColor.h"

#include "dsp/Math.h"

namespace rtxx {

namespace {
// One-pole de 320 Hz de los suavizados del bloque (: las dos primeras parejas de
// coeficientes son 320/sr; las otras dos son 10/sr).
float g320(double sr) { return 1.0f - std::exp(-2.0f * static_cast<float>(M_PI) * 320.0f /
                                               static_cast<float>(sr)); }
                                               // Seno del LFO del DegradeSR.
float lfoSine(float phase) {
    float p = phase - std::floor(phase);
    const float t = p + p - 1.0f;
    float u = (std::fabs(t) - 0.5f) * static_cast<float>(M_PI);
    u = u * u;
    float y = u * (u * 0.03679168f - 0.49558073f) + 0.9988062f;
    return t > 0.0f ? -y : y;
}
} // namespace

float BitColorBlock::aliasWeight(float degradeSR, float aliasSmooth) {
// f = clamp((degSR−20)/170, 0, 1) y A = AliasSmoothing·(f·0.8)/(f·0.6+0.2).
    float f = 0.0f;
    if (degradeSR >= 20.0f) f = degradeSR <= 190.0f ? (degradeSR - 20.0f) / 170.0f : 1.0f;
    return aliasSmooth * ((f * 0.8f) / (f * 0.6f + 0.2f));
}

float BitColorBlock::aa2MaxStep(float aa2, float aliasWeight, float bits, double sr) {
// u = clamp((bits−1)/3) para 1..4, 1 por encima; paso = 1/(k²·0.008·sr).
    float u = 1.0f;
    if (bits < 4.0f) u = bits >= 1.0f ? (bits - 1.0f) / 3.0f : 0.0f;
    const float k = aa2 * aliasWeight * (1.0f - u);
    if (k < 1e-5f) return 10000.0f;
    return 1.0f / (k * k * 0.008f * static_cast<float>(sr));
}

float BitColorBlock::aa3Coef(float aa3, float aliasWeight, float bits, double sr) {
// u y v son las aperturas por profundidad de bits.
    float u = 1.0f;
    if (bits < 4.0f) u = bits >= 1.0f ? (bits - 1.0f) / 3.0f : 0.0f;
    float v = 0.0f;
    if (bits >= 4.0f) v = bits <= 10.0f ? (bits - 4.0f) / 6.0f : 1.0f;
    const float w = 1.0f - aa3 * aliasWeight * u * (1.0f - v);
    const float corner = ((w * 0.04f) / (1.0f - 0.92f * w - 0.04f)) * 19800.0f + 200.0f;
    const float c = std::exp(-2.0f * static_cast<float>(M_PI) * corner / static_cast<float>(sr));
    return 1.0f - c;
}

void BitColorBlock::filterCoefs(float hz, double sr, float& c, float& g, float& tpt) {
    const float f = hz / static_cast<float>(sr);
    c = std::exp(-2.0f * static_cast<float>(M_PI) * f);
    g = 1.0f - c;
    tpt = (c + 1.0f) * 0.5f;
}

float BitColorBlock::holdFactor(float degradeSR, float depth, double sr, float lfo01) {
// y 147-166.
    const double s = sr > 0.0 ? sr : 44100.0;
    const double d = dsp::clamp(depth, 0.0f, 1.0f);
    const double lo = std::pow((static_cast<double>(degradeSR) * (1.0 - d) * 32.0) / s, 0.25);
    const double hi = std::pow((static_cast<double>(degradeSR) * (1.0 + d) * 32.0) / s, 0.25);
    const double mixed = lo + (hi - lo) * static_cast<double>(dsp::clamp(lfo01, 0.0f, 1.0f));
    const double f = mixed * mixed * mixed * mixed;
    return static_cast<float>(dsp::clamp(static_cast<float>(f), 0.0f, 8.0f));
}

float BitColorBlock::internalCrossfade(float degradeSR, float bits) {
// posición normalizada invertida (min > max en las dos) y triángulo
    // en 0.5..1; el factor es el mínimo de los dos.
    const float nSR = dsp::clamp((44100.0f - degradeSR) / (44100.0f - 44.1f), 0.0f, 1.0f);
    const float nB = dsp::clamp((24.0f - bits) / (24.0f - 1.0f), 0.0f, 1.0f);
    const float tSR = dsp::clamp(2.0f * nSR - 1.0f, 0.0f, 1.0f);
    const float tB = dsp::clamp(2.0f * nB - 1.0f, 0.0f, 1.0f);
    return tSR < tB ? tSR : tB;
}

void BitColorBlock::prepare(const PrepareContext& ctx) {
    sampleRate_ = ctx.sampleRate > 0.0 ? ctx.sampleRate : 44100.0;
    // Los parámetros del modelo son del ParamStore: se leen por bloque en process(), como el
    // motor. Aquí sólo se calcula lo que depende del sample rate.
    reset();
}

void BitColorBlock::reset() {
    for (int c = 0; c < kMaxChannels; ++c) {
        for (int i = 0; i < kRing; ++i) ring_[c][i] = 1.0f;   // el anillo vive desplazado +1
        idx_[c] = 0;
        acc_[c] = 0.0f;
        hpX_[c] = hpY_[c] = hpX2_[c] = hpY2_[c] = 0.0f;
        lpY_[c] = lpY2_[c] = 0.0f;
        slew_[c] = postLp_[c] = 0.0f;
    }
    lfoPhase_ = 0.0f;
    bitsState_ = 0.0f;
}

void BitColorBlock::process(const PortData* in, PortData& out) {
    const int n = out.numSamples > 0 ? out.numSamples : (in ? in->numSamples : 0);
    if (n <= 0 || !in || in->numChannels < 1) return;
    const int nch = in->numChannels < kMaxChannels ? in->numChannels : kMaxChannels;

    const float* src[kMaxChannels] = {in->ch[0], nch > 1 ? in->ch[1] : in->ch[0]};
    float* dst[kMaxChannels] = {out.ch[0], nch > 1 ? out.ch[1] : out.ch[0]};

// Gate del motor: Enable < 0.5 ⇒ return (bypass seco).
    const float enable = params_ ? params_->get("Digital_Enable") : 0.0f;
    if (enable < 0.5f) {
        for (int i = 0; i < n; ++i)
            for (int c = 0; c < nch; ++c) dst[c][i] = src[c][i];
        return;
    }

    const float fb = dsp::clamp(params_->get("Digital_Feedback"), 0.0f, 1.0f);
    fbRetention_ = fb;
    kFeedback_ = 1.0f / (1.0f - 0.77f * fb);

    const float degSR = dsp::clamp(params_->get("Digital_DegradeSR"), 10.0f, 44100.0f);
    const float depth = dsp::clamp(params_->get("Digital_DegradeModDepth"), 0.0f, 1.0f);
    const float rate = dsp::clamp(params_->get("Digital_DegradeModRate"), 0.0f, 1.0f);

// LFO del DegradeSR: avanza UNA vez por bloque y modula lo/hi.
    const float inc = ((rate * 0.0005681818f) /
                       (1.0f - rate * 0.99886364f - 0.0005681818f)) * 440.0f * 64.0f /
                      static_cast<float>(sampleRate_);
    lfoPhase_ += inc;
    lfoPhase_ -= std::floor(lfoPhase_);
    holdFactor_ = holdFactor(degSR, depth, sampleRate_, lfoSine(lfoPhase_) * 0.5f + 0.5f);

// Bits: suavizado a 320 Hz antes de derivar la escala.
    const float bitsTarget = dsp::clamp(params_->get("Digital_BitDepth"), 1.0f, 24.0f);
    bitsState_ += (bitsTarget - bitsState_) * g320(sampleRate_);
    const float bits = bitsState_;
    bitsScale_ = quantScale(bits);
    bitsInvScale_ = 1.0f / bitsScale_;
    quantGain_ = quantGain(bits);
    biasMag_ = biasMagnitude(dsp::clamp(params_->get("Digital_RoundError"), -1.0f, 1.0f));

    // Antialias y mezcla.
    aliasNorm_ = dsp::clamp(params_->get("Digital_AliasSmoothing") / 100.0f, 0.0f, 1.0f);
    const float A = aliasWeight(degSR, aliasNorm_);
    aa2MaxStep_ = aa2MaxStep(dsp::clamp(params_->get("Digital_AA2"), 0.0f, 1.0f), A, bits,
                             sampleRate_);
    aa3G_ = aa3Coef(dsp::clamp(params_->get("Digital_AA3"), 0.0f, 1.0f), A, bits, sampleRate_);
    aa3Pole_ = 1.0f - aa3G_;

    // Filtros: FilterLo (paso-alto) y FilterHi (paso-bajo). En el modo Focus = 0 el cruce
    // interno los abre, en el dominio normalizado del parámetro log.
    focus_ = dsp::clamp(params_->get("Digital_Focus"), 0.0f, 1.0f);
    float nLo = std::log(dsp::clamp(params_->get("Digital_FilterLo"), 20.0f, 20000.0f) / 20.0f) /
                std::log(1000.0f);
    float nHi = std::log(dsp::clamp(params_->get("Digital_FilterHi"), 20.0f, 20000.0f) / 20.0f) /
                std::log(1000.0f);
    if (focus_ < 0.5f) {
        const float xf = internalCrossfade(degSR, bits);
        nLo *= (1.0f - xf);
        nHi += xf * (1.0f - nHi);
    }
    float c = 0.0f, g = 0.0f, tpt = 0.0f;
    filterCoefs(20.0f * std::pow(1000.0f, dsp::clamp(nLo, 0.0f, 1.0f)), sampleRate_, c, g, tpt);
    hpC_ = c;
    hpG_ = g;
    hpA_ = tpt;
    filterCoefs(20.0f * std::pow(1000.0f, dsp::clamp(nHi, 0.0f, 1.0f)), sampleRate_, c, g, tpt);
    lpC_ = c;
    lpG_ = g;

    const float bitsHigh = dsp::clamp((bits - 21.0f) / 2.5f, 0.0f, 1.0f);
    mixOut_ = dsp::clamp(params_->get("Digital_Mix") / 100.0f, 0.0f, 1.0f) *
              (1.0f - aa3Pole_ * bitsHigh);
              outGain_ = 1.0f; // Digital_OutGain no existe en la tabla; el previo está apagado.

    for (int i = 0; i < n; ++i) {
        for (int ch = 0; ch < nch; ++ch) {
            const float x = src[ch][i];

            // FilterLo: paso-alto TPT (y = A·(u − u₋₁) + c·y₋₁).
            float f = hpA_ * (x - hpX_[ch]) + hpC_ * hpY_[ch];
            hpX_[ch] = x;
            hpY_[ch] = f;
            if (focus_ < 0.5f) {   // segunda etapa en el otro modo de foco
                const float f2 = hpA_ * (f - hpX2_[ch]) + hpC_ * hpY2_[ch];
                hpX2_[ch] = f;
                hpY2_[ch] = f2;
                f = f2;
            }
            // FilterHi: one-pole paso-bajo (y = g·x + c·y₋₁).
            float w = lpG_ * f + lpC_ * lpY_[ch];
            lpY_[ch] = w;
            if (focus_ < 0.5f) {
                const float w2 = lpG_ * w + lpC_ * lpY2_[ch];
                lpY2_[ch] = w2;
                w = w2;
            }
            const float filtered = w;

            // Sample-and-hold: anillo de 32 posiciones que avanza `factor` por muestra.
            acc_[ch] += holdFactor_;
            const int steps = static_cast<int>(acc_[ch]);
            if (steps > 0) {
                const float target = filtered * kFeedback_ + 1.0f;
                int k = idx_[ch];
                for (int s = 0; s < steps; ++s) {
                    k = (k + 1) & (kRing - 1);
                    ring_[ch][k] = target + (ring_[ch][k] - target) * fbRetention_;
                }
                idx_[ch] = k;
                acc_[ch] -= static_cast<float>(steps);
            }

            // Cuantizador con el cruce de AliasSmoothing entre la muestra retenida y la anterior.
            const float wgt = dsp::clamp(acc_[ch] / (aliasNorm_ + 1e-5f), 0.0f, 1.0f);
            const float xa = ring_[ch][idx_[ch]] - 1.0f;
            const float xb = ring_[ch][(idx_[ch] + kRing - 1) & (kRing - 1)] - 1.0f;
            const float qa = std::trunc(xa * bitsScale_ + (xa < 0.0f ? -biasMag_ : biasMag_)) *
                             bitsInvScale_ * quantGain_;
            const float qb = std::trunc(xb * bitsScale_ + (xb < 0.0f ? -biasMag_ : biasMag_)) *
                             bitsInvScale_ * quantGain_;
            float q = qb + wgt * (qa - qb);

// AA2: limitador de slew; AA3: one-pole posterior.
            if (std::fabs(q) < 1e-9f) q = 0.0f;
            const float d = q - slew_[ch];
            const float step = std::fabs(d) < aa2MaxStep_ ? std::fabs(d) : aa2MaxStep_;
            slew_[ch] += (d < 0.0f ? -step : step);
            postLp_[ch] += aa3G_ * (slew_[ch] - postLp_[ch]);

            const float wet = focus_ * (x - filtered) + outGain_ * postLp_[ch];
            dst[ch][i] = x + mixOut_ * (wet - x);
        }
    }
}

} // namespace rtxx
