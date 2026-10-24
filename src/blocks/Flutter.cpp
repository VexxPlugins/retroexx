#include "blocks/Flutter.h"

#include <cmath>

#include "dsp/Math.h"

namespace rtxx {

namespace {
constexpr float kWearFloor = 0.031f;        // límite inferior de la ganancia de desgaste
} // namespace

float FlutterBlock::smoothstepForSteer(float steer) {
    const float a = std::fabs(steer);
    if (a > 1.1f) return 1.0f;
    const float x = (a + 0.1f) / 1.2f;
    return x * x * (3.0f - 2.0f * x);
}

float FlutterBlock::depthEffective(float depth, float steerSig) {
    return depth * (1.0f - smoothstepForSteer(steerSig));
}

float FlutterBlock::wearGain(float wearDepth, float wearGain) {
    const float g = 1.0f - wearDepth * smoothstepForSteer(wearGain);
    return dsp::clamp(g, kWearFloor, 1.0f);
}

float FlutterBlock::wearGainLeft(float wearDepth, float wearGain, float wearPan) {
    const float keep = 1.0f - dsp::clamp(wearPan, 0.0f, 1.0f);
    const float g = 1.0f - wearDepth * smoothstepForSteer(wearGain) * keep;
    return dsp::clamp(g, kWearFloor, 1.0f);
}

float FlutterBlock::wearGainRight(float wearDepth, float wearGain, float wearPan) {
    const float keep = 1.0f + (wearPan <= 0.0f ? wearPan : 0.0f);
    const float g = 1.0f - wearDepth * smoothstepForSteer(wearGain) * keep;
    return dsp::clamp(g, kWearFloor, 1.0f);
}

float FlutterBlock::dropoutGainLeft(float amount, float steerSig, float dmin, float dmax,
                                    float wearPan) {
    if (amount == 0.0f) return 1.0f;
    // `steerSig` es `Magnetic_DropOutSteerSig`, una señal de *flux*: el dropout cae cuando la
    // señal baja del mínimo (p = 0 ⇒ ganancia 0). Como el generador de flux no está
    // reconstruido, `ParamStore` deja esa señal en su MÁXIMO (= sin dropout); si llegara a 0
    // el canal se quedaría mudo (medido: −300 dBFS con `MAGN Dropouts` > 0).
    const float p = (dmax > dmin) ? (dsp::clamp(steerSig, dmin, dmax) - dmin) / (dmax - dmin) : 0.0f;
    const float pan = dsp::clamp(wearPan * 0.5f, -1.0f, 1.0f);
    return dsp::clamp(1.0f - (1.0f + (pan < 0.0f ? pan : 0.0f)) * (1.0f - p), 0.0f, 1.0f);
}

float FlutterBlock::dropoutGainRight(float amount, float steerSig, float dmin, float dmax,
                                     float wearPan) {
    if (amount == 0.0f) return 1.0f;
    const float p = (dmax > dmin) ? (dsp::clamp(steerSig, dmin, dmax) - dmin) / (dmax - dmin) : 0.0f;
    const float pan = dsp::clamp(wearPan * 0.5f, -1.0f, 1.0f);
    return dsp::clamp(1.0f - (1.0f - (pan > 0.0f ? pan : 0.0f)) * (1.0f - p), 0.0f, 1.0f);
}

void FlutterBlock::prepare(const PrepareContext& ctx) {
    sampleRate_ = ctx.sampleRate > 0.0 ? ctx.sampleRate : 44100.0;
    lfo_.prepare(sampleRate_);
    reset();
}

void FlutterBlock::reset() {
// El cFlutter arranca la fase en 0.25.
    lfo_.reset(0.25f);
}

void FlutterBlock::process(const PortData* in, PortData& out) {
    const int n = out.numSamples > 0 ? out.numSamples : (in ? in->numSamples : 0);
    if (n <= 0 || !in || in->numChannels < 1) return;
    const int nch = in->numChannels < kMaxChannels ? in->numChannels : kMaxChannels;

    const float* src[kMaxChannels] = {in->ch[0], nch > 1 ? in->ch[1] : in->ch[0]};
    float* dst[kMaxChannels] = {out.ch[0], nch > 1 ? out.ch[1] : out.ch[0]};

// Gate del motor: Enable < 0.5, o Depth == 0 y WearDepth == 0 ⇒ return (bypass).
    const float enable = params_ ? params_->get("Magnetic_Enable") : 0.0f;
    const float depth = dsp::clamp(params_ ? params_->get("Magnetic_Depth") : 0.0f, 0.0f, 1.0f);
    const float wearDepth = dsp::clamp(params_ ? params_->get("Magnetic_WearDepth") : 0.0f, 0.0f, 1.0f);
    if (enable < 0.5f || (depth == 0.0f && wearDepth == 0.0f)) {
        for (int i = 0; i < n; ++i)
            for (int c = 0; c < nch; ++c) dst[c][i] = src[c][i];
        return;
    }

    const float rate = params_ ? params_->get("Magnetic_Rate") : 0.0f;
    const float steer = params_ ? params_->get("Magnetic_DepthSteerSig") : 0.5f;
    const float wearGainSig = params_ ? params_->get("Magnetic_WearGain") : 0.0f;
    const float wearPan = params_ ? params_->get("Magnetic_WearPan") : 0.0f;
    const bool wearPanEnable = (params_ ? params_->get("Magnetic_WearPanEnable") : 0.0f) >= 0.5f;
    const float dropAmount = dsp::clamp(params_ ? params_->get("Magnetic_DropOutsAmount") : 0.0f, 0.0f, 1.0f);
    const float dropMin = dsp::clamp(params_ ? params_->get("Magnetic_DropOutsMin") : 0.0f, 0.0f, 1.0f);
    const float dropMax = dsp::clamp(params_ ? params_->get("Magnetic_DropOutsMax") : 0.0f, 0.0f, 1.0f);
    const float dropSig = params_ ? params_->get("Magnetic_DropOutSteerSig") : 0.0f;

    lfo_.setRate(rate);
    // WaveShape / WaveWarp / WaveShapeMorph no existen en la tabla (sin control de
    // usuario ni id de modelo) ⇒ forma por defecto y warp neutro.
    lfo_.setShapeFromWaveShape(0);
    lfo_.setWaveWarp(0.5f);

    const float depthEff = depthEffective(depth, steer);
    const float comp = compensation(depthEff);
    const float wearMono = wearGain(wearDepth, wearGainSig);
    float wear[kMaxChannels] = {wearMono, wearMono};
    if (wearPanEnable && nch > 1) {
        wear[0] = wearGainLeft(wearDepth, wearGainSig, wearPan);
        wear[1] = wearGainRight(wearDepth, wearGainSig, wearPan);
    }
    const float drop[kMaxChannels] = {
        dropoutGainLeft(dropAmount, dropSig, dropMin, dropMax, wearPan),
        dropoutGainRight(dropAmount, dropSig, dropMin, dropMax, wearPan)};

    // El offset de fase del canal 1 es StereoPhase·0.5 (0x2e8, sólo un motor).
    const float stereoPhase = dsp::clamp(params_ ? params_->get("Magnetic_StereoPhase") : 0.0f,
                                        0.0f, 1.0f) * 0.5f;

    for (int i = 0; i < n; ++i) {
        const float l0 = lfo_.next(0.0f);
        const float l1 = stereoPhase != 0.0f ? lfo_.valueAt(stereoPhase) : l0;
        for (int c = 0; c < nch; ++c) {
            const float l = (c == 1) ? l1 : l0;
            dst[c][i] = src[c][i] * comp * lfoGain(depthEff, l) * wear[c] * drop[c];
        }
    }
}

} // namespace rtxx
