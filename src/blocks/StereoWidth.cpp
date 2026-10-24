#include "blocks/StereoWidth.h"

#include <cmath>

namespace rtxx {

float StereoWidthBlock::sideFor(float v) {
    if (v <= 0.0f) return 0.0f;
    if (v <= 0.5f) return v;
    return 0.5f * std::pow(4.0f, v - 0.5f);
}

float StereoWidthBlock::midFor(float v) {
    if (v <= 0.5f) return 0.5f;
    return 0.25f / sideFor(v);
}

void StereoWidthBlock::prepare(const PrepareContext& ctx) {
    sampleRate_ = ctx.sampleRate > 0.0 ? ctx.sampleRate : 44100.0;
    // Misma constante que el suavizado de ganancia de conexión del motor:
    // coef = 1 - exp(-2*pi*20/sr).
    coef_ = 1.0f - std::exp(static_cast<float>(-2.0 * M_PI * 20.0 / sampleRate_));
    primed_ = false;
}

void StereoWidthBlock::reset() { primed_ = false; }

void StereoWidthBlock::process(const PortData* in, PortData& out) {
    const int n = out.numSamples > 0 ? out.numSamples : (in ? in->numSamples : 0);
    if (n <= 0 || !in || in->numChannels < 1) return;

    const float v = params_ ? params_->get("Width_Width") / 200.0f : 0.5f;
    const float targetSide = sideFor(v);
    const float targetMid = midFor(v);
    if (!primed_) { // El motor arranca ya en el valor (medido: sin transitorio audible)
        side_ = targetSide;
        mid_ = targetMid;
        primed_ = true;
    }

    const float* l = in->ch[0];
    const float* r = in->numChannels > 1 && in->ch[1] ? in->ch[1] : l;
    for (int i = 0; i < n; ++i) {
        side_ += coef_ * (targetSide - side_);
        mid_ += coef_ * (targetMid - mid_);
        const float sum = l[i] + r[i];
        const float diff = r[i] - l[i];
        const float m = mid_ * sum;
        const float s = side_ * diff;
        out.ch[0][i] = m - s;
        if (out.numChannels > 1) out.ch[1][i] = m + s;
    }
}

} // namespace rtxx
