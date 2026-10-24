#include "blocks/ResoX.h"

#include <algorithm>
#include <cmath>

#include "dsp/Math.h"

namespace rtxx {

namespace {

// Longitudes de los 4 allpass (segundos), las de Freeverb
// (0.0126077095, 0.01, 0.0077324263, 0.0051020407) más 0.00026077096 si el prepare usa
// ±spread/2 = ±0.00013038548 s por canal.
constexpr double kAllpassL[4] = {0.0126077095, 0.01, 0.0077324263, 0.0051020407};
constexpr double kAllpassSpread = 0.0005215419;   // 23 muestras @44,1 kHz

// Nota MIDI de la primera línea: 22 (A#0 = 29,14 Hz); el prepare dimensiona los buffers
// con las notas 22..33 en ese orden.
constexpr int kFirstNote = 22;

// Ley de Decay a T60 (1 kHz) ajustada por medida: 1/T60 = 0,847 − 0,833·Decay
// (5 puntos, error < 0,02 s).
constexpr float kInvT60At0 = 0.847f;
constexpr float kInvT60Slope = 0.833f;

// Fracción de nivel del pre-delay cuando el camino de allpass está activo.
constexpr float kAllpassInput = 0.031000f;

// Mezcla de los dos caminos del lazo: `w_k = A·u_k + B·z_k`. A y B no son observables
// directamente: los valores de abajo son los que mejor reproducen el T60 por banda
// (ajustados por medida con la suite).
constexpr float kLoopA = 0.300000f;
constexpr float kLoopB = 0.700000f;
// Corrección de la ley Decay → T60: el T60 del lazo sale algo más corto que el objetivo
// porque el filtro del lazo (Dampen/Soften) resta ganancia en cada paso. Factor único
// ajustado por medida.
constexpr float kT60Scale = 1.060000f;

} // namespace

double ResoXBlock::lineFreqHz(int k) {
    return 440.0 * std::pow(2.0, (kFirstNote + k - 69.0) / 12.0);
}

double ResoXBlock::allpassSeconds(int i) {
    return kAllpassL[i < 0 ? 0 : (i > 3 ? 3 : i)];
}

double ResoXBlock::t60FromDecay(float decay) {
    const float d = dsp::clamp(decay, 0.0f, 0.9995f);
    const float inv = kInvT60At0 - kInvT60Slope * d;
    // A Decay = 1 la ley da T60 → ∞ (realimentación unitaria). Se acota a 200 s para que
    // el estado no explote.
    return inv > 1.0f / 200.0f ? 1.0 / inv : 200.0;
}

float ResoXBlock::loopGain(float t60, int samples, double sr) {
    if (samples < 1 || t60 <= 0.0f || sr <= 0.0) return 0.0f;
    const double g = std::pow(10.0, -3.0 * static_cast<double>(samples) /
                                        (static_cast<double>(sr) * t60));
    return static_cast<float>(dsp::clamp(g, 0.0, 0.99999));
}

void ResoXBlock::dryWet(float level, float& dry, float& wet) {
// Ley ajustado por medida: con
// m = rsmul(v, 0,15)
    //   wet = sin(π/2·m),  dry = cos(π/2·m)
    // el residuo es 1,6e-6 dB en 11 puntos del host y seco² + wet² = 1,0000 exacto.
    const float v = dsp::clamp(level, 0.0f, 1.0f);
    const float ang = static_cast<float>(M_PI) * 0.5f * rsmul(v, 0.15f);
    wet = std::sin(ang);
    dry = std::cos(ang);
}

float ResoXBlock::filterGain(float hz, double sr) {
    const float f = static_cast<float>(dsp::clamp(hz, 1.0f, 20000.0f) / sr);
    return std::exp(-2.0f * static_cast<float>(M_PI) * f);
}

void ResoXBlock::prepare(const PrepareContext& ctx) {
    sampleRate_ = ctx.sampleRate > 0.0 ? ctx.sampleRate : 44100.0;
    const double sr = sampleRate_;

// 1-polo que suaviza el pre-delay: τ = 6 999 muestras (158,7 ms). Verificado: con el
// impulso en 704 muestras el retardo alcanza el 9,6 % del objetivo (903 de 4 410) y con
// el impulso en 1 s ya es el 97,6 % (4 306), que es exactamente 1 − e^(−m/6999).
    pdCoef_ = 1.0f - std::exp(-1.0f / 6999.0f);

    for (int ch = 0; ch < kCh; ++ch) {
        for (int k = 0; k < kLines; ++k) {
            Line& L = lines_[ch][k];
            // Capacidad: se dimensiona con la nota de unísono, pero el canal R tiene sus
            // líneas MÁS LARGAS (espaciado de peine MIDI/1,0227 = −39 cents), así que la
            // capacidad tiene que admitir el detune máximo o el canal R se recortaría.
            // 1,035 = 2^(0,6/12) con margen. [?] El margen exacto no está verificado; el
            // espaciado sí.
            const int cap = static_cast<int>(sr / lineFreqHz(k) * 1.035) + 2;
            L.buf.assign(static_cast<size_t>(cap), 0.0f);
            L.size = cap;
            L.idx = 0;
            L.xPrev = L.yPrev = L.uPrev = 0.0f;
            L.gain = 0.0f;
        }
        for (int i = 0; i < kAllpass; ++i) {
            Allpass& A = ap_[ch][i];
            // Se reserva el máximo (largo + spread/2); el largo efectivo se fija en process()
            // porque depende de StereoMode.
            const int n = std::max(2, static_cast<int>(sr * (kAllpassL[i] + kAllpassSpread * 0.5) + 0.5));
            A.buf.assign(static_cast<size_t>(n), 0.0f);
            A.size = std::max(2, static_cast<int>(sr * kAllpassL[i] + 0.5));
            A.idx = 0;
        }
        std::fill(pd_[ch], pd_[ch] + kPdSize, 0.0f);
        pdIdx_[ch] = 0;
        pdDelay_[ch] = 1.0f;
    }
    reset();
}

void ResoXBlock::reset() {
    for (int ch = 0; ch < kCh; ++ch) {
        for (int k = 0; k < kLines; ++k) {
            Line& L = lines_[ch][k];
            std::fill(L.buf.begin(), L.buf.end(), 0.0f);
            L.idx = 0;
            L.xPrev = L.yPrev = L.uPrev = 0.0f;
        }
        for (int i = 0; i < kAllpass; ++i) {
            Allpass& A = ap_[ch][i];
            std::fill(A.buf.begin(), A.buf.end(), 0.0f);
            A.idx = 0;
        }
        std::fill(pd_[ch], pd_[ch] + kPdSize, 0.0f);
        pdIdx_[ch] = 0;
        pdDelay_[ch] = 1.0f;
    }
}

void ResoXBlock::process(const PortData* in, PortData& out) {
    const int n = out.numSamples > 0 ? out.numSamples : (in ? in->numSamples : 0);
    if (n <= 0 || !in || in->numChannels < 1) return;
    const int nch = in->numChannels < kCh ? in->numChannels : kCh;

    const float* src[kCh] = {in->ch[0], nch > 1 ? in->ch[1] : in->ch[0]};
    float* dst[kCh] = {out.ch[0], nch > 1 ? out.ch[1] : out.ch[0]};

// Gate del motor: Enable < 0.5 ⇒ return.
    const float enable = params_ ? params_->get("Space_Enable") : 0.0f;
    if (enable < 0.5f) {
        for (int i = 0; i < n; ++i)
            for (int c = 0; c < nch; ++c) dst[c][i] = src[c][i];
        return;
    }

    const double sr = sampleRate_;

    // --- coeficientes por bloque -----------------------------------------------------
    const float decay = params_->get("Space_Decay") * 0.01f;      // modelo 0..100 → 0..1
    const float t60 = static_cast<float>(t60FromDecay(decay)) * kT60Scale;

    // Dampen (host 37) = paso-alto; Soften (host 38) = paso-bajo. Clamp [100, 16000] Hz, que es
    // el del DSP del motor, no el 22050 del spec.
    const float dampenHz = dsp::clamp(params_->get("Space_Dampen"), 100.0f, 16000.0f);
    const float softenHz = dsp::clamp(params_->get("Space_Soften"), 100.0f, 16000.0f);
    const float gd = filterGain(dampenHz, sr);      // polo del paso-alto
    const float cd = (1.0f + gd) * 0.5f;             // coeficiente directo (TPT)
    const float gs = filterGain(softenHz, sr);      // polo del paso-bajo
    const float cs = 1.0f - gs;

    // Detune estéreo: Δ = StereoMode · ReverbMode(1) · StereoDetune, en semitonos
    // El modo 0 deja los dos canales idénticos (medido: L = R bit a bit).
    const float stereoMode = params_->get("Space_StereoMode");
    const float detune = params_->get("Space_StereoDetune");
    const double delta = (stereoMode >= 0.5f ? 1.0 : 0.0) * detune;

// El spread de los allpass también depende del modo: en mono los dos
    // canales son idénticos, y así se mide (L = R bit a bit con StereoMode = 0).
    const double spread = (stereoMode >= 0.5f ? kAllpassSpread : 0.0);
    for (int ch = 0; ch < nch; ++ch) {
        const double semis = (ch == 0 ? +delta : -delta);
        for (int i = 0; i < kAllpass; ++i) {
            Allpass& A = ap_[ch][i];
            const double sec = kAllpassL[i] + (ch == 0 ? -spread * 0.5 : spread * 0.5);
            int n = std::max(2, static_cast<int>(sr * sec + 0.5));
            if (n > static_cast<int>(A.buf.size())) n = static_cast<int>(A.buf.size());
            if (n != A.size) {
                A.size = n;
                A.idx = 0;
                std::fill(A.buf.begin(), A.buf.end(), 0.0f);
            }
        }
        for (int k = 0; k < kLines; ++k) {
            Line& L = lines_[ch][k];
            const double f = 440.0 * std::pow(2.0, (kFirstNote + k + semis - 69.0) / 12.0);
            int len = static_cast<int>(sr / f + 0.5);
            if (len < 2) len = 2;
            if (len > L.size) len = L.size;   // sólo puede acortarse respecto del unisono
            L.size = len;
            if (L.idx >= L.size) L.idx = 0;
            L.gain = loopGain(t60, L.size, sr);
        }
        if (ch == 0) {
            diag_.lines = kLines;
            for (int k = 0; k < kLines; ++k) diag_.samples[k] = lines_[0][k].size;
            for (int i = 0; i < kAllpass; ++i) diag_.allpass[i] = ap_[0][i].size;
        }
    }

// Pre-delay: ms → muestras, con suavizado 1-polo y clamp [1, 0,1·sr].
// Ley del host MEDIDA: ms = 100·v lineal (0/24,94/49,93/74,88/99,86 ms).
    const float preDelayMs = params_->get("Space_PreDelay");   // modelo en ms (0..100)
    const float target = preDelayMs * 0.001f * static_cast<float>(sr);
    const float targetClamped = dsp::clamp(target, 1.0f, static_cast<float>(sr) * 0.1f);

    // Mezcla seco/húmedo desde Level (host 5 → Space_Level).
    float dry = 1.0f, wet = 0.0f;
    dryWet(params_->get("Space_Level"), dry, wet);

    // `ReverbMode` y `DoAllPass` son internos y valen 1,0 de fábrica; no están en el modelo
    // (ParamTable no los tiene) ni el host los expone, así que se leen si algún día existen.
    auto flagOn = [this](const char* id) {
        return params_->hasModel(id) ? params_->get(id) >= 0.5f : true;
    };
    const bool allpassOn = flagOn("Space_ReverbMode") && flagOn("Space_DoAllPass");
    const float netIn = allpassOn ? kAllpassInput : 1.0f;

    // --- bucle de audio ---------------------------------------------------------------
    for (int i = 0; i < n; ++i) {
        for (int ch = 0; ch < nch; ++ch) {
            const float x = src[ch][i];

            // Pre-delay: escribe la entrada y lee con interpolación de 4 puntos.
            // UN solo 1-polo por canal: `state` y `d = clamp(state+1)`. Con
            // dos polos en cascada el retardo tardaba el doble y el onset no seguía a la medida.
            pdDelay_[ch] += (targetClamped - pdDelay_[ch]) * pdCoef_;
            float d = dsp::clamp(pdDelay_[ch], 1.0f, static_cast<float>(kPdSize - 4));

            float* pb = pd_[ch];
            pb[pdIdx_[ch] & (kPdSize - 1)] = x;
            const int iFlo = static_cast<int>(d);
            const float f = d - static_cast<float>(iFlo);
            const int i0 = (pdIdx_[ch] - iFlo) & (kPdSize - 1);
            const float hm1 = pb[(i0 - 1) & (kPdSize - 1)];
            const float h0 = pb[i0];
            const float h1 = pb[(i0 + 1) & (kPdSize - 1)];
            const float h2 = pb[(i0 + 2) & (kPdSize - 1)];
            // Catmull-Rom.
            const float inPd = h0 + 0.5f * f * ((h1 - hm1) +
                                  f * ((2.0f * hm1 - 5.0f * h0 + 4.0f * h1 - h2) +
                                  f * (3.0f * (h0 - h1) + h2 - hm1)));
            pdIdx_[ch] = (pdIdx_[ch] + 1) & (kPdSize - 1);

            // 12 resonadores en paralelo (realimentación diagonal, sin matriz).
            float wetSum = 0.0f;
            for (int k = 0; k < kLines; ++k) {
                Line& L = lines_[ch][k];
                const float xk = L.buf[static_cast<size_t>(L.idx)];
                const float yk = (xk - L.xPrev) * cd + L.yPrev * gd;
                L.xPrev = xk;
                L.yPrev = yk;
                const float zk = 0.6f * xk + 0.4f * yk;
                const float uk = zk * cs + L.uPrev * gs;
                L.uPrev = uk;
                const float wk = kLoopA * uk + kLoopB * zk;
                L.buf[static_cast<size_t>(L.idx)] = inPd * netIn - wk * L.gain;
                wetSum += wk;
                if (++L.idx >= L.size) L.idx = 0;
            }
            // 4 allpass en serie.
            float y = wetSum;
            if (allpassOn) {
                for (int a = 0; a < kAllpass; ++a) {
                    Allpass& A = ap_[ch][a];
                    const float v = A.buf[static_cast<size_t>(A.idx)];
                    A.buf[static_cast<size_t>(A.idx)] = y + v * 0.5f;
                    if (++A.idx >= A.size) A.idx = 0;
                    y = v - y;
                }
            }
            dst[ch][i] = dry * x + wet * y;
        }
    }
}

} // namespace rtxx
