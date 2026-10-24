#include "blocks/SimpleComp.h"

#include <algorithm>
#include <cmath>

#include "block/Port.h"

namespace rtxx {
namespace {
constexpr float kMinDb = -120.0f;
} // namespace

// --- Leyes MEDIDAS --------------------------------------------------

// El umbral efectivo baja 47,31 dBFS por unidad del control (residuo 0,078 dB en 7 puntos).
float SimpleCompBlock::thresholdDbFromParam(float d) {
    const float x = std::min(std::max(d, 0.0f), 1.0f);
    return -4.04f - 47.31f * x;
}

// Atenuación estática: 0,637 dB por dB de exceso sobre el umbral ⇒ ratio 2,76:1.
float SimpleCompBlock::duckAttenuationDb(float sideDb, float d) {
    const float over = sideDb - thresholdDbFromParam(d);
    return over > 0.0f ? 0.637f * over : 0.0f;
}

// Release medido: 156–245 ms según el control (crece hasta ~0,5 y baja).
float SimpleCompBlock::releaseSeconds(float d) {
    const float x = std::min(std::max(d, 0.0f), 1.0f);
    return 0.156f + 0.089f * std::sin(static_cast<float>(M_PI) * std::min(1.0f, x * 2.0f));
}

// --- Bloque -----------------------------------------------------------------------

SimpleCompBlock::SimpleCompBlock(const ParamSource* params) : params_(params) {}

void SimpleCompBlock::prepare(const PrepareContext& ctx) {
    sr_ = ctx.sampleRate;
    reset();
}

void SimpleCompBlock::reset() {
    for (int ch = 0; ch < 2; ++ch) {
        env_[ch] = 0.0f;
        envDb_[ch] = kMinDb;
        duckDb_[ch] = 0.0f;
        gain_[ch] = 1.0f;
        scHp_[ch].reset();
    }
    scHpHz_ = 0.0f;
}

void SimpleCompBlock::declarePorts(std::vector<std::string>& ports) const {
    ports.push_back("inSidechain");
}

void SimpleCompBlock::attachPort(const std::string& name, PortData* view) {
    if (name == "inSidechain") sc_ = view;
}

void SimpleCompBlock::process(const PortData* in, PortData& out) {
    const int n = out.numSamples;
    const int nch = std::min(out.numChannels, 2);
    if (!in) {
        // Sin entrada principal no hay ruido que atenuar: se devuelve el sidechain tal cual.
        for (int ch = 0; ch < nch; ++ch)
            for (int i = 0; i < n; ++i) out.ch[ch][i] = 0.0f;
        return;
    }

    const bool enabled = params_ && params_->get("SComp_Enable") >= 0.5f;
    // El control del host ("NOIS Duck") escribe SComp_Threshold en %.
    const float duck = std::min(std::max(params_ ? params_->get("SComp_Threshold") / 100.0f : 0.0f,
                                         0.0f), 1.0f);
    const bool scHp = params_ && params_->get("SComp_HiPassSidechain") >= 0.5f;

    if (scHp && scHpHz_ == 0.0f) {
        // Paso-alto de 2.º orden medido: fc 50,5 Hz, Q 0,55 (−4,0 dB a 60 Hz).
        for (int ch = 0; ch < 2; ++ch) scHp_[ch].highpass(sr_, 50.5, 0.55);
        scHpHz_ = 50.5f;
    }

    // Attack 5–9 ms y release 156–245 ms (medidos).
    const float atk = 0.007f;
    const float rel = releaseSeconds(duck);
    const float atkCoef = 1.0f - std::exp(-1.0f / (atk * static_cast<float>(sr_)));
    const float relCoef = 1.0f - std::exp(-1.0f / (rel * static_cast<float>(sr_)));

    // El sidechain es el PROGRAMA y se usa SÓLO como detector. La salida es el ruido atenuado:
    // el seco NO se suma aquí porque el grafo ya lo lleva por la arista directa
    // `ToStereo.out → Wobble.in`. Sumarlo aquí lo duplicaba: medido,
    // con `NOIS Amount` bajo el "ruido" diferencial caía al nivel del programa (era el seco).
    const bool haveSc = sc_ && sc_->numSamples >= n;
    for (int i = 0; i < n; ++i) {
        for (int ch = 0; ch < nch; ++ch) {
            const float dry = haveSc && sc_->ch[ch] ? sc_->ch[ch][i] : 0.0f;
            const float sc = (scHp && scHpHz_ > 0.0f) ? scHp_[ch].process(dry) : dry;
            const float a = std::fabs(sc);
            // Seguidor de pico con attack/release asimétricos sobre el PROGRAMA.
            env_[ch] += (a - env_[ch]) * (a > env_[ch] ? atkCoef : relCoef);
            const float db = env_[ch] > 1.0e-6f ? 20.0f * std::log10(env_[ch]) : kMinDb;
            // Atenuación estática medida (ratio 2,76:1), suavizada con el attack/release del
            // seguidor. Deshabilitado ⇒ el ruido no pasa (ganancia 0 exacta).
            if (!enabled) {
                duckDb_[ch] = 0.0f;
                gain_[ch] = 0.0f;
                envDb_[ch] = db;
                continue;
            }
            const float target = duckAttenuationDb(db, duck);
            const float coef = target > duckDb_[ch] ? atkCoef : relCoef;
            duckDb_[ch] += (target - duckDb_[ch]) * coef;
            gain_[ch] = std::pow(10.0f, -duckDb_[ch] * 0.05f);
            envDb_[ch] = db;
        }
        for (int ch = 0; ch < nch; ++ch) out.ch[ch][i] = in->ch[ch][i] * gain_[ch];
    }
    (void)envDb_;
}

} // namespace rtxx
