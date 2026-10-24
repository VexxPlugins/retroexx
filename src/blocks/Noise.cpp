#include "blocks/Noise.h"

#include <algorithm>
#include <cmath>

#include "block/Port.h"

namespace rtxx {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr float kSmoothMs = 20.0f;   // suavizado de nivel/tono (1-polo)
// Normalización de las tres fuentes a RMS 0,5 (medido con el PRNG del bloque).
constexpr float kW = 0.5f / 0.5776f;   // blanco uniforme en [−1,1)
constexpr float kP = 0.5f / 0.5720f;   // rosa (Kellet de 7 polos)
constexpr float kB = 0.5f / 0.9169f;   // marrón (integrador con fuga)

} // namespace

// --- Biquads (RBJ, forma directa II traspuesta) ----------------------------------

void Biquad::highpass(double sr, double f, double q) {
    const double w = 2.0 * kPi * std::min(f, 0.49 * sr) / sr, cw = std::cos(w), sw = std::sin(w);
    const double alpha = sw / (2.0 * q), a0 = 1.0 + alpha;
    b0 = static_cast<float>((1.0 + cw) * 0.5 / a0);
    b1 = static_cast<float>(-(1.0 + cw) / a0);
    b2 = b0;
    a1 = static_cast<float>(-2.0 * cw / a0);
    a2 = static_cast<float>((1.0 - alpha) / a0);
}

void Biquad::lowpass(double sr, double f, double q) {
    const double w = 2.0 * kPi * std::min(f, 0.49 * sr) / sr, cw = std::cos(w), sw = std::sin(w);
    const double alpha = sw / (2.0 * q), a0 = 1.0 + alpha;
    b0 = static_cast<float>((1.0 - cw) * 0.5 / a0);
    b1 = static_cast<float>((1.0 - cw) / a0);
    b2 = b0;
    a1 = static_cast<float>(-2.0 * cw / a0);
    a2 = static_cast<float>((1.0 - alpha) / a0);
}

void Biquad::peaking(double sr, double f, double q, double gainDb) {
    const double A = std::pow(10.0, gainDb / 40.0);
    const double w = 2.0 * kPi * std::min(f, 0.49 * sr) / sr, cw = std::cos(w), sw = std::sin(w);
    const double alpha = sw / (2.0 * q), a0 = 1.0 + alpha / A;
    b0 = static_cast<float>((1.0 + alpha * A) / a0);
    b1 = static_cast<float>(-2.0 * cw / a0);
    b2 = static_cast<float>((1.0 - alpha * A) / a0);
    a1 = static_cast<float>(-2.0 * cw / a0);
    a2 = static_cast<float>((1.0 - alpha / A) / a0);
}

void Biquad::lowShelf(double sr, double f, double q, double gainDb) {
    const double A = std::pow(10.0, gainDb / 40.0);
    const double w = 2.0 * kPi * std::min(f, 0.49 * sr) / sr, cw = std::cos(w), sw = std::sin(w);
    const double alpha = sw / (2.0 * q), sa = 2.0 * std::sqrt(A) * alpha;
    const double a0 = (A + 1.0) + (A - 1.0) * cw + sa;
    b0 = static_cast<float>(A * ((A + 1.0) - (A - 1.0) * cw + sa) / a0);
    b1 = static_cast<float>(2.0 * A * ((A - 1.0) - (A + 1.0) * cw) / a0);
    b2 = static_cast<float>(A * ((A + 1.0) - (A - 1.0) * cw - sa) / a0);
    a1 = static_cast<float>(-2.0 * ((A - 1.0) + (A + 1.0) * cw) / a0);
    a2 = static_cast<float>(((A + 1.0) + (A - 1.0) * cw - sa) / a0);
}

void Biquad::highShelf(double sr, double f, double q, double gainDb) {
    const double A = std::pow(10.0, gainDb / 40.0);
    const double w = 2.0 * kPi * std::min(f, 0.49 * sr) / sr, cw = std::cos(w), sw = std::sin(w);
    const double alpha = sw / (2.0 * q), sa = 2.0 * std::sqrt(A) * alpha;
    const double a0 = (A + 1.0) - (A - 1.0) * cw + sa;
    b0 = static_cast<float>(A * ((A + 1.0) + (A - 1.0) * cw + sa) / a0);
    b1 = static_cast<float>(-2.0 * A * ((A - 1.0) + (A + 1.0) * cw) / a0);
    b2 = static_cast<float>(A * ((A + 1.0) + (A - 1.0) * cw - sa) / a0);
    a1 = static_cast<float>(2.0 * ((A - 1.0) - (A + 1.0) * cw) / a0);
    a2 = static_cast<float>(((A + 1.0) - (A - 1.0) * cw - sa) / a0);
}

// --- Leyes ------------------------------------------------------------------------

// --- Leyes de Follow y Tone --------------------------------------------

// `b(v)`: offset entre el nivel del ruido y el del programa, en la zona proporcional. Tabla
// medida cada 0,05 desde 0,20 (por debajo de 0,15 el control no hace nada). No tiene forma
// cerrada: se interpola linealmente.
float NoiseBlock::followB(float follow) {
    static const float v[] = {0.20f, 0.25f, 0.30f, 0.35f, 0.40f, 0.45f, 0.50f, 0.55f,
                              0.60f, 0.65f, 0.70f, 0.75f, 0.80f, 0.85f, 0.90f, 0.95f, 1.00f};
    static const float b[] = {5.882f, 2.352f, -0.082f, -1.598f, -2.433f, -2.566f, -2.376f,
                              -2.627f, -2.881f, -3.161f, -3.458f, -3.785f, -4.157f, -4.591f,
                              -5.127f, -5.830f, -6.885f};
    const float x = std::min(std::max(follow, v[0]), v[16]);
    for (int i = 1; i < 17; ++i)
        if (x <= v[i]) {
            const float t = (x - v[i - 1]) / (v[i] - v[i - 1]);
            return b[i - 1] + t * (b[i] - b[i - 1]);
        }
    return b[16];
}

// `gmax(v)`: techo del boost (dB). Por encima de 0,55 no se alcanza con programa ≤ −10 dBFS
// (se extrapola con la tendencia medida).
float NoiseBlock::followGmax(float follow) {
    static const float v[] = {0.20f, 0.25f, 0.30f, 0.35f, 0.40f, 0.45f, 0.50f, 0.55f,
                              0.60f, 0.70f, 0.80f, 0.90f, 1.00f};
    static const float g[] = {0.0f, 1.02f, 2.00f, 3.40f, 5.20f, 7.28f, 9.53f, 9.44f,
                              9.23f, 8.68f, 7.99f, 7.04f, 5.29f};
    const float x = std::min(std::max(follow, v[0]), v[12]);
    for (int i = 1; i < 13; ++i)
        if (x <= v[i]) {
            const float t = (x - v[i - 1]) / (v[i] - v[i - 1]);
            return g[i - 1] + t * (g[i] - g[i - 1]);
        }
    return g[12];
}

// Constantes de tiempo del VCA de Follow (subida 20–65 ms, bajada 35…715 ms según v).
float NoiseBlock::followAttackSeconds(float follow) {
    const float x = std::min(std::max(follow, 0.0f), 1.0f);
    return 0.020f + 0.045f * x;
}

float NoiseBlock::followReleaseSeconds(float follow) {
    const float x = std::min(std::max(follow, 0.0f), 1.0f);
    static const float t[] = {0.035f, 0.147f, 0.319f, 0.715f};   // v = 0,25 / 0,5 / 0,75 / 1
    if (x <= 0.25f) return t[0];
    if (x >= 1.0f) return t[3];
    if (x <= 0.5f) return t[0] + (x - 0.25f) / 0.25f * (t[1] - t[0]);
    if (x <= 0.75f) return t[1] + (x - 0.5f) / 0.25f * (t[2] - t[1]);
    return t[2] + (x - 0.75f) / 0.25f * (t[3] - t[2]);
}

// El modelo guarda `Noise_Tone` en dB (±12) y el host es 0..1 con centro 0,5: v = 0,5 + dB/24.
float NoiseBlock::toneTiltAmount(float toneDb) {
    const float v = std::min(std::max(0.5f + toneDb / 24.0f, 0.0f), 1.0f);
    return 1.0f - 2.0f * v;   // +1 ⇒ +12,04 dB en graves; −1 ⇒ −12,04
}

const char* NoiseBlock::typeName(int index) {
    static const char* names[16] = {
        "VINYL 1", "VINYL 2", "TAPE 7.5", "TAPE 15", "TUBE", "DC", "MUFF", "RADIO",
        "VHS", "CASS", "8-BIT", "WHITE", "PINK", "STUDIO 1", "STUDIO 2", "APOLLO"};
    return names[(index < 0 || index > 15) ? 0 : index];
}

int NoiseBlock::typeCount() { return 16; }

// --- Recetas (MATERIAL PROPIO) ----------------------------------------------------
//
// Una fila por tipo (el índice es el valor del host 8). `trimDb` es el ajuste de sonoridad que
// hace que el RMS a Amount = 1 caiga en el por medida (se calibra con la; 0 = pendiente
// de calibrar). El resto de campos describe el COLOR (pesos de las fuentes, hum, clics,
// cuantización, filtros y LFO lento).
//
// Todo esto es nuestra síntesis: no hay ni un sample del motor. Lo que se copia es el
// comportamiento medido (nivel, pendiente espectral, hum, densidad de clics, rango dinámico),
// que es justo lo que comprueba la suite de medidas.
const NoiseBlock::Recipe& NoiseBlock::recipe(int index) {
    static const Recipe kRecipes[16] = {
        // Columnas: nombre, trim(dB), white, pink, brown, humAmp, humHz, crackRate, crackAmp,
        // quantRate, hpHz, lpHz, peak1Hz, peak1Q, peak1dB, peak2Hz, peak2Q, peak2dB, wobDepth, wobHz.
        // Pesos y filtros ajustados contra la tabla de 1/3 de octava por medida (
        // /tmp/rtxx_noise/fit2.py); hum, clics y LFO copiados de los descriptores medidos.
        {"VINYL 1", -12.4f, 0.0f, 0.09f, 0.91f, 0.0f, 60.0f, 4.0f, 0.55f, 0.0f, 150.0f, 14000.0f, 60.0f, 1.2f, 6.0f, 160.0f, 1.2f, 6.0f, 0.05f, 0.7f},
        {"VINYL 2", -11.7f, 0.39f, 0.06f, 0.56f, 0.0f, 60.0f, 16.0f, 0.45f, 0.0f, 20.0f, 3000.0f, 80.0f, 1.2f, 8.0f, 12000.0f, 1.2f, 6.0f, 0.04f, 0.5f},
        {"TAPE 7.5", -11.8f, 0.26f, 0.0f, 0.74f, 0.6f, 60.0f, 0.0f, 0.0f, 0.0f, 20.0f, 8000.0f, 80.0f, 1.2f, 8.0f, 1200.0f, 1.2f, 6.0f, 0.35f, 5.43f},
        {"TAPE 15", -13.7f, 0.37f, 0.0f, 0.63f, 0.4f, 60.0f, 0.0f, 0.0f, 0.0f, 20.0f, 11000.0f, 80.0f, 1.2f, 8.0f, 8000.0f, 1.2f, 5.0f, 0.35f, 5.43f},
        {"TUBE", -9.6f, 0.52f, 0.06f, 0.42f, 0.25f, 50.0f, 5.3f, 0.1f, 0.0f, 150.0f, 20000.0f, 60.0f, 1.2f, 6.0f, 80.0f, 1.2f, 8.0f, 0.03f, 0.4f},
        {"DC", -9.4f, 0.45f, 0.09f, 0.45f, 0.45f, 50.0f, 0.0f, 0.0f, 0.0f, 250.0f, 8000.0f, 315.0f, 1.2f, 4.0f, 630.0f, 1.2f, 6.0f, 0.0f, 0.0f},
        {"MUFF", -7.0f, 0.52f, 0.06f, 0.42f, 0.0f, 60.0f, 1.0f, 0.06f, 0.0f, 100.0f, 2000.0f, 315.0f, 1.2f, 4.0f, 8000.0f, 1.2f, 5.0f, 0.02f, 0.3f},
        {"RADIO", -5.9f, 0.0f, 0.0f, 1.0f, 0.0f, 60.0f, 1.0f, 0.25f, 0.0f, 400.0f, 1300.0f, 500.0f, 1.2f, 6.0f, 1200.0f, 1.2f, 6.0f, 0.05f, 0.6f},
        {"VHS", -8.2f, 0.71f, 0.0f, 0.29f, 0.0f, 60.0f, 0.0f, 0.0f, 0.0f, 150.0f, 4000.0f, 8000.0f, 1.2f, 5.0f, 12000.0f, 1.2f, 6.0f, 0.05f, 1.7f},
        {"CASS", -12.1f, 0.81f, 0.1f, 0.1f, 0.0f, 60.0f, 0.0f, 0.0f, 0.0f, 40.0f, 8000.0f, 160.0f, 1.2f, 6.0f, 12000.0f, 1.2f, 6.0f, 0.1f, 0.9f},
        {"8-BIT", -10.2f, 0.17f, 0.0f, 0.83f, 0.0f, 60.0f, 0.0f, 0.0f, 8000.0f, 150.0f, 6000.0f, 630.0f, 1.2f, 6.0f, 1200.0f, 1.2f, 6.0f, 0.0f, 0.0f},
        {"WHITE", -9.7f, 0.71f, 0.0f, 0.29f, 0.0f, 60.0f, 0.0f, 0.0f, 0.0f, 150.0f, 8000.0f, 0.0f, 1.2f, 0.0f, 60.0f, 1.2f, 6.0f, 0.0f, 0.0f},
        {"PINK", -5.6f, 0.09f, 0.0f, 0.91f, 0.0f, 60.0f, 0.0f, 0.0f, 0.0f, 150.0f, 3000.0f, 0.0f, 1.2f, 0.0f, 8000.0f, 1.2f, 5.0f, 0.0f, 0.0f},
        {"STUDIO 1", -14.4f, 0.11f, 0.0f, 0.89f, 0.15f, 60.0f, 15.3f, 0.12f, 0.0f, 20.0f, 20000.0f, 80.0f, 1.2f, 8.0f, 160.0f, 1.2f, 6.0f, 0.04f, 0.2f},
        {"STUDIO 2", -14.4f, 0.0f, 0.0f, 1.0f, 0.0f, 60.0f, 7.0f, 0.1f, 0.0f, 20.0f, 4000.0f, 160.0f, 1.2f, 6.0f, 500.0f, 1.2f, 6.0f, 0.05f, 0.3f},
        {"APOLLO", -4.4f, 0.09f, 0.0f, 0.91f, 0.0f, 60.0f, 2.0f, 0.12f, 0.0f, 250.0f, 1300.0f, 1200.0f, 1.2f, 6.0f, 8000.0f, 1.2f, 5.0f, 0.05f, 0.1f},
    };
    return kRecipes[(index < 0 || index > 15) ? 0 : index];
}

// --- Bloque -----------------------------------------------------------------------

NoiseBlock::NoiseBlock(const ParamSource* params) : params_(params) {}

void NoiseBlock::prepare(const PrepareContext& ctx) {
    sr_ = ctx.sampleRate;
    toneCoef_ = 1.0f - std::exp(-1.0f / static_cast<float>(kSmoothMs * 0.001 * sr_));
    reset();
}

void NoiseBlock::reset() {
    for (int ch = 0; ch < 2; ++ch) {
        std::fill(std::begin(pink_[ch]), std::end(pink_[ch]), 0.0f);
        brown_[ch] = 0.0f;
        humPhase_[ch] = 0.0f;
        crackleEnv_[ch] = 0.0f;
        quantHold_[ch] = 0.0f;
        quantPhase_[ch] = 0.0f;
        wobblePhase_[ch] = (ch == 0 ? 0.0f : 0.37f);
        followEnv_[ch] = 0.0f;
        hp_[ch].reset();
        lp_[ch].reset();
        peak_[ch].reset();
        peak2_[ch].reset();
        // Estados del PRNG distintos por canal (secuencias independientes).
        s0_[ch] = 0x9e3779b97f4a7c15ull + 0x1234567ull * static_cast<uint64_t>(ch + 1);
        s1_[ch] = 0xbf58476d1ce4e5b9ull + 0x7654321ull * static_cast<uint64_t>(ch + 1);
    }
    lastType_ = -1;
    levelDb_ = -78.0f;
    targetDb_ = -78.0f;
}

void NoiseBlock::process(const PortData* in, PortData& out) {
    const int n = out.numSamples;
    nch_ = std::min(out.numChannels, 2);

    // Interruptor y tipo. `Noise_Enable` es el interruptor del macro (host 7 → Noise_Enable).
    const bool enabled = params_ && params_->get("Noise_Enable") >= 0.5f;
    const int type = params_ ? static_cast<int>(std::lround(params_->get("Noise_NoiseType"))) : 0;
    // Unidades NATIVAS del modelo: nivel en dB (−78..0) y Tone en dB (−12..+12). La
    // conversión desde el host la hace ParamStore (leyes NoiseLevel / ToneBipolar12).
    const float amountDb = params_ ? params_->get("Noise_NoiseLevel") : -78.0f;
    const float tone = params_ ? params_->get("Noise_Tone") : 0.0f;

    if (type != lastType_) {
        lastType_ = type;
        type_ = type;
        const Recipe& r = recipe(type_);
        for (int ch = 0; ch < 2; ++ch) {
            hp_[ch].highpass(sr_, r.hpHz);
            lp_[ch].lowpass(sr_, r.lpHz);
            if (r.peakHz > 0.0f) peak_[ch].peaking(sr_, r.peakHz, r.peakQ, r.peakDb);
            else { peak_[ch].b0 = 1.0f; peak_[ch].b1 = peak_[ch].b2 = peak_[ch].a1 = peak_[ch].a2 = 0.0f; }
            peak_[ch].reset();
            if (r.peak2Hz > 0.0f) peak2_[ch].peaking(sr_, r.peak2Hz, r.peak2Q, r.peak2Db);
            else { peak2_[ch].b0 = 1.0f; peak2_[ch].b1 = peak2_[ch].b2 = peak2_[ch].a1 = peak2_[ch].a2 = 0.0f; }
            peak2_[ch].reset();
        }
    }

    const Recipe& r = recipe(type_);

    // Nivel objetivo (dB) = nivel del control + trim del tipo.
    targetDb_ = enabled ? amountDb + r.trimDb : -120.0f;
    if (!in) targetDb_ = -120.0f;

    // Apagado (o nivel inaudible) ⇒ CERO, no el seco: el seco lo devuelve el compresor del
    // contenedor, que lo recibe como sidechain y lo suma a su salida (ver SimpleComp.h). Si el
    // generador devolviera el seco, el programa entraría DOS veces en el contenedor y una tercera
    // por la arista directa `ToStereo.out → Wobble.in`.
    if (targetDb_ <= -110.0f) {
        levelDb_ = -120.0f;
        for (int ch = 0; ch < nch_; ++ch) std::fill(out.ch[ch], out.ch[ch] + n, 0.0f);
        return;
    }

// `Noise_Tone` = TILT de 2.º orden medido: H(x) = (2+jx)/(1+2jx) con x = f/800 Hz y
    // ganancia (1−2v)·40·log10|H|. Se implementa como DOS etapas idénticas T_a con
    // T_a(s) = (s + a·ω0)/(a·s + ω0), a = 2^(1−2v): T_2² = H² exacto, y a = 1 es plano.
    if (std::fabs(tone - lastTone_) > 1e-4f) {
        lastTone_ = tone;
        tiltDb_ = tone;
        const float amount = toneTiltAmount(tone);      // 1 … −1
        const double a = std::pow(2.0, static_cast<double>(amount));
        const double w0 = 2.0 * kPi * 800.0;
        const double K = w0 / std::tan(kPi * 800.0 / sr_);   // bilineal con prewarp en 800 Hz
        const double b0 = K + a * w0, b1 = a * w0 - K;
        const double a0 = a * K + w0, a1 = w0 - a * K;
        for (int ch = 0; ch < 2; ++ch) {
            for (Biquad* f : {&toneA_[ch], &toneB_[ch]}) {
                f->b0 = static_cast<float>(b0 / a0);
                f->b1 = static_cast<float>(b1 / a0);
                f->b2 = 0.0f;
                f->a1 = static_cast<float>(a1 / a0);
                f->a2 = 0.0f;
                f->reset();
            }
        }
    }

// `Noise_Follow`: VCA broadband con ganancia en dB = clamp(L_prog − b(v),
    // −inf, gmax(v)), pendiente 1 dB/dB, y constantes de tiempo que dependen de v. Con v ≤ 0,15
    // el control no hace nada. Se aplica DESPUÉS del nivel de Amount.
    const float follow = std::min(std::max(params_ ? params_->get("Noise_Follow") / 100.0f : 0.0f,
                                           0.0f), 1.0f);
    const bool followOn = follow > 0.15f && in != nullptr;
    const float fB = followB(follow), fGmax = followGmax(follow);
    const float fAtk = followOn ? 1.0f - std::exp(-1.0f / (followAttackSeconds(follow) *
                                                          static_cast<float>(sr_))) : 0.0f;
    const float fRel = followOn ? 1.0f - std::exp(-1.0f / (followReleaseSeconds(follow) *
                                                          static_cast<float>(sr_))) : 0.0f;

    for (int i = 0; i < n; ++i) {
        // Suavizado del nivel (1-polo, 20 ms) para que el control no haga zipper.
        levelDb_ += (targetDb_ - levelDb_) * toneCoef_;
        const float gain = levelDb_ <= -119.0f ? 0.0f : std::pow(10.0f, levelDb_ * 0.05f);
        if (followOn) {
            for (int ch = 0; ch < nch_; ++ch) {
                const float a = std::fabs(in->ch[ch][i]);
                followEnv_[ch] += (a - followEnv_[ch]) * (a > followEnv_[ch] ? fAtk : fRel);
                const float progDb = followEnv_[ch] > 1.0e-6f ? 20.0f * std::log10(followEnv_[ch])
                                                             : -120.0f;
                followGainDb_[ch] = std::min(progDb - fB, fGmax);
            }
        }
        for (int ch = 0; ch < nch_; ++ch) {
            if (gain <= 0.0f) { out.ch[ch][i] = 0.0f; continue; }
            // --- fuentes continuas
            // xorshift128+ (dos palabras)
            uint64_t x = s0_[ch];
            const uint64_t y = s1_[ch];
            s0_[ch] = y;
            x ^= x << 23;
            s1_[ch] = x ^ y ^ (x >> 17) ^ (y >> 26);
            const float noise = static_cast<float>((s1_[ch] + y) >> 40) * (1.0f / 8388608.0f) - 1.0f;

            // rosa: filtro de Paul Kellet de 7 polos (aprox. −3 dB/oct)
            float& p0 = pink_[ch][0];
            pink_[ch][1] = 0.99886f * pink_[ch][1] + noise * 0.0555179f;
            pink_[ch][2] = 0.99332f * pink_[ch][2] + noise * 0.0750759f;
            pink_[ch][3] = 0.96900f * pink_[ch][3] + noise * 0.1538520f;
            pink_[ch][4] = 0.86650f * pink_[ch][4] + noise * 0.3104856f;
            pink_[ch][5] = 0.55000f * pink_[ch][5] + noise * 0.5329522f;
            pink_[ch][6] = -0.7616f * pink_[ch][6] - noise * 0.0168980f;
            const float pink = (pink_[ch][1] + pink_[ch][2] + pink_[ch][3] + pink_[ch][4] +
                                pink_[ch][5] + pink_[ch][6] + p0 * 0.5362f + noise * 0.115926f) *
                               0.35f;
            p0 = noise;
            // marrón: integrador con fuga (aprox. −6 dB/oct)
            brown_[ch] = 0.995f * brown_[ch] + 0.05f * noise;
            const float brown = brown_[ch] * 3.5f;

            // Las tres fuentes se normalizan a RMS 0,5 para que los pesos de la receta sean
            // potencias comparables (medido: blanco 0,578, rosa 0,572, marrón 0,917).
            float s = kW * r.whiteMix * noise + kP * r.pinkMix * pink + kB * r.brownMix * brown;

            // hum: 60/50 Hz + armónicos (2..5)
            if (r.humAmp > 0.0f) {
                humPhase_[ch] += static_cast<float>(r.humHz / sr_);
                if (humPhase_[ch] >= 1.0f) humPhase_[ch] -= 1.0f;
                const float ph = humPhase_[ch] * 6.2831853f;
                float h = std::sin(ph) + 0.5f * std::sin(2.0f * ph) + 0.33f * std::sin(3.0f * ph) +
                          0.25f * std::sin(4.0f * ph) + 0.2f * std::sin(5.0f * ph);
                s += r.humAmp * h * 0.4f;
            }

            // clics: proceso de Poisson con envolvente de decaimiento rápido.
            if (r.crackleRate > 0.0f) {
                const float p = r.crackleRate / static_cast<float>(sr_);
                const float u = (static_cast<float>((s1_[ch] >> 40) & 0xffff) + 0.5f) / 65536.0f;
                if (u < p) {
                    cracklePolarity_[ch] = (cracklePolarity_[ch] > 0.0f) ? -1.0f : 1.0f;
                    crackleEnv_[ch] = 1.0f;
                }
                crackleEnv_[ch] *= 0.9993f;   // τ ≈ 32 ms a 44,1 kHz
                s += r.crackleAmp * crackleEnv_[ch] * cracklePolarity_[ch];
            }

            // cuantización / sample&hold (8-BIT)
            if (r.quantRate > 0.0f) {
                quantPhase_[ch] += static_cast<float>(r.quantRate / sr_);
                if (quantPhase_[ch] >= 1.0f) { quantPhase_[ch] -= 1.0f; quantHold_[ch] = s; }
                s = quantHold_[ch];
            }

            // cadena de color
            s = hp_[ch].process(s);
            s = lp_[ch].process(s);
            if (r.peakHz > 0.0f) s = peak_[ch].process(s);
            if (r.peak2Hz > 0.0f) s = peak2_[ch].process(s);
            if (std::fabs(tiltDb_) > 1e-4f) s = toneB_[ch].process(toneA_[ch].process(s));

            // LFO lento de amplitud
            if (r.wobbleDepth > 0.0f) {
                wobblePhase_[ch] += static_cast<float>(r.wobbleHz / sr_);
                if (wobblePhase_[ch] >= 1.0f) wobblePhase_[ch] -= 1.0f;
                s *= 1.0f - r.wobbleDepth * 0.5f * (1.0f + std::sin(wobblePhase_[ch] * 6.2831853f));
            }

            // SÓLO ruido (el seco va por su propia arista): nivel de Amount × VCA de Follow.
            const float fg = followOn ? std::pow(10.0f, followGainDb_[ch] * 0.05f) : 1.0f;
            out.ch[ch][i] = s * gain * fg;
        }
    }
}

} // namespace rtxx
