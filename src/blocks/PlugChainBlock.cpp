#include "blocks/PlugChainBlock.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "block/PlugChainEval.h"

namespace rtxx {
namespace {

// Adaptador: resuelve los nombres locales del DSL contra el modelo con prefijo de módulo.
class PrefixedParams : public ParamSource {
public:
    explicit PrefixedParams(const PlugChainBlock* self) : self_(self) {}
    float get(const char* id) const override { return self_->lookup(id); }

private:
    const PlugChainBlock* self_;
};

PlugChainBlock::Kind kindFromName(const std::string& k) {
    if (k == "gain") return PlugChainBlock::Kind::Gain;
    if (k == "hp12") return PlugChainBlock::Kind::Hp12;
    if (k == "lp12") return PlugChainBlock::Kind::Lp12;
    if (k == "hp6") return PlugChainBlock::Kind::Hp6;
    if (k == "lp6") return PlugChainBlock::Kind::Lp6;
    if (k == "bpPeak") return PlugChainBlock::Kind::BpPeak;
    if (k == "hpShelf") return PlugChainBlock::Kind::HpShelf;
    if (k == "distort") return PlugChainBlock::Kind::Distort;
    if (k == "magicDistort") return PlugChainBlock::Kind::MagicDistort;
    return PlugChainBlock::Kind::Unsupported;
}

// El motor barre los denormales con el idioma `+ 1e-4 - 1e-4` en los acumuladores de las
// etapas; se replica porque es lo que hace El motor y
// porque los estados de estas etapas decaen a cero en pasajes silenciosos.
inline float denorm(float x) { return (x + 1e-4f) - 1e-4f; }

// Saturador de la casa de las dos etapas de TubePair: el atan toma el
// argumento ya negado (`fnmul s0, s9, s0`), así que el lado positivo sale lineal invertido y el
// negativo comprimido y también invertido. Es una función IMPAR: no rectifica.
inline float saturator(float v) { return (v > 0.0f) ? -v : std::atan(-v); }

inline float dbToGain(double db) { return static_cast<float>(std::pow(10.0, db / 20.0)); }

// Coeficiente del one-pole `a = 1 - exp(-2π·f/sr)` con el tope en 0.49999 del motor
float onePoleCoeff(double freq, double sr) {
    double x = freq / sr;
    const double arg = (x >= 0.49999) ? -dsp::Biquad::kPi : -2.0 * dsp::Biquad::kPi * x;
    return static_cast<float>(1.0 - std::exp(arg));
}

// Polo `c = exp(-2π·min(0.499·sr, f)/sr)` de las secciones de magicDistort
// (: todas usan `fminnm(sr*0.499, f)`).
float poleCoeff(double freq, double sr) {
    const double f = std::min(0.499 * sr, freq);
    return static_cast<float>(std::exp(-(f * 6.283185) / sr));
}

// Frecuencias del DSL: `log` y `logcut` son geométricas sobre [lo, hi]. Se distingue
// `logcut` sólo para poder medirlo por separado; hoy las dos se resuelven igual y el
// gate de la fase 2 lo comprueba la suite de medidas.
double freqLaw(double v, double lo, double hi) {
    if (v <= 0.0) return lo;
    if (v >= 1.0) return hi;
    return lo * std::pow(hi / lo, v);
}

// `sqr`: bipolar al cuadrado. v=0..1 -> x=-1..1 -> signo(x)*x^2 escalado.
// Para rangos simétricos (los `db` del DSL son -24..24) queda x*|x|*hi.
double sqrLaw(double v, double lo, double hi) {
    const double x = 2.0 * v - 1.0;
    const double sq = x * (x < 0.0 ? -x : x);
    if (std::fabs(lo + hi) < 1e-9) return sq * hi;
    return sq >= 0.0 ? sq * hi : -sq * -lo;
}

// `sym c`: eje normalizado con la curva de la casa (escala-del-modelo.md:187-219,
// t = c·v/((2c-1)·v + 1 - c), y = min + (max-min)·t. Con c = 0.5 es lineal;
// con c = 0.15 el 15 % del recorrido ya está en la mitad del eje. El DSL escribe el
// parámetro de curva como el token siguiente a la palabra clave (`sym 0.15`, `sym 0.4`).
double symLaw(double v, double lo, double hi, double c) {
    const double den = (2.0 * c - 1.0) * v + 1.0 - c;
    const double t = std::fabs(den) < 1e-12 ? v : c * v / den;
    return lo + (hi - lo) * t;
}

// Coeficientes del one-pole de 6 dB/octava de la casa (hp6/lp6): un polo IIR con
// g = 1 - exp(-2*pi*fc/fs), SIN prewarping bilineal.
//   LP: H = g / (1 - (1-g) z^-1)
//   HP: H = 1 - LP = (1-g)(1 - z^-1) / (1 - (1-g) z^-1)
// ajustado por medida con la cadena suave del master (hp6@62 Hz + lp6@8.8 kHz):
// coincide a 0.03 dB rms / 0.15 dB peor caso en 40 Hz..16 kHz. Con el biquad prewarpado
// el error llega a 18 dB en 20 kHz (el bilineal empina la caída hacia Nyquist y este
// filtro no). El one-pole barato del informe escala-del-modelo.md §5.4 era la pista
// correcta; aquí queda confirmado por ajuste de la curva completa.
struct CheapOnePole {
    double b0, b1, a1;
};

CheapOnePole cheapLowPass(double fs, double freq) {
    const double g = 1.0 - std::exp(-2.0 * dsp::Biquad::kPi * freq / fs);
    return {g, 0.0, -(1.0 - g)};
}

CheapOnePole cheapHighPass(double fs, double freq) {
    const double g = 1.0 - std::exp(-2.0 * dsp::Biquad::kPi * freq / fs);
    return {1.0 - g, -(1.0 - g), -(1.0 - g)};
}

} // namespace

// ---------------------------------------------------------------------------------------
// Secciones de 1er orden de magicDistort.
// ---------------------------------------------------------------------------------------
void PlugChainBlock::OnePoleSection::setHighPass(float coeff) {
    a = coeff;
    b = (coeff + 1.0f) * 0.5f;
    c = -b;
}

void PlugChainBlock::OnePoleSection::setLowShelf(float coeff) {
    a = coeff;
    b = (1.0f - coeff) / 1.12f;
    c = b * 0.12f;
}

// ---------------------------------------------------------------------------------------
// cAESingleBandDistortion — envolvente común + shapes 0/1/2.
// ---------------------------------------------------------------------------------------
void PlugChainBlock::DistortDsp::configure(const DistParams& p, double sr) {
    const float mixN = p.mix * 0.01f;
    dry = 1.0f - mixN;                       // P+0x38
    distGain = dbToGain(p.distDB) * mixN;    // P+0x3c
    passGain = dbToGain(p.passDB) * mixN;    // P+0x40
    d = dbToGain(-static_cast<double>(p.refLevel));  // P+0x44
    a1 = onePoleCoeff(p.lowFreq, sr);
    a2 = onePoleCoeff(std::max(p.lowFreq, p.highFreq), sr);
    a3 = onePoleCoeff(p.distLP, sr);
    shape = p.shape;
    k = 0.0f;
    scale = 1.0f;
    thirdK = 0.0f;
    dcOffset = 0.0f;
    switch (shape) {
        case 0:  // Crunch: k = 0.5·50^(a/100); norm = k<1 ? 1 : sqrt(k/(3k-2))
            k = 0.5f * std::exp(p.amount * 0.01f * 3.912023f);
            scale = (k < 1.0f) ? 1.0f : std::sqrt(k / (3.0f * k - 2.0f));
            scale /= d;
            break;
        case 1:  // Hard: k = 10^(-0.026·a); scale = (k+1)²·√3/(k+3)/d; c = (k/3)·scale/(k+k/3)
            k = std::exp((p.amount * -0.013f) * 4.6051702f);
            thirdK = k / 3.0f;
            scale = (((k + 1.0f) * (k + 1.0f) * 1.7320508f) / (k + 3.0f)) / d;
            dcOffset = (thirdK * scale) / (k + thirdK);
            break;
        case 2:  // Zap: k = d·12^(a/100)·0.2; scale = 0.707/d
            k = d * std::exp(p.amount * 0.01f * 2.4849067f) * 0.2f;
            scale = 0.707f / d;
            break;
        default:
            // [?] shape 3 (BitCrush) del DSP: el DSL nunca lo escribe (`shape Type 3` no
            // existe en plugChainDist) y no se implementa aquí. La etapa queda como
            // identidad y el conteo de cobertura no la da por hecha.
            break;
    }
}

void PlugChainBlock::DistortDsp::reset() {
    for (int c = 0; c < kMaxChannels; ++c) lo[c] = band[c] = wet[c] = 0.0f;
}

void PlugChainBlock::DistortDsp::clearEnvelope() {
    for (int c = 0; c < kMaxChannels; ++c) lo[c] = band[c] = wet[c] = 0.0f;
}

float PlugChainBlock::DistortDsp::process(int ch, float x) {
    const float loNew = lo[ch] + a1 * denorm(x - lo[ch]);
    const float bandNew = band[ch] + a2 * denorm((x - loNew) - band[ch]);
    lo[ch] = loNew;
    band[ch] = bandNew;

    float s;
    switch (shape) {
        case 0: {
            const float u = d * bandNew;
            const float t = 1.0f - std::fabs(u);
            s = u * (k * t * t + 1.0f);
            s = std::min(1.0f, std::max(-1.0f, s));
            s *= scale;
            break;
        }
        case 1: {
            float v = thirdK + d * bandNew;
            v = scale * v / (std::fabs(v) + k) - dcOffset;
            s = v;
            break;
        }
        case 2: {
            const float v = k * bandNew + 0.25f;
            const float fr = std::fabs(v - std::trunc(v)) - 0.5f;
            s = scale * (fr * fr * (std::fabs(32.0f * fr) - 24.0f) + 1.0f);
            break;
        }
        default:
            s = x;
            break;
    }
    wet[ch] = wet[ch] + a3 * denorm(s - wet[ch]);
    return x * dry + (x - bandNew) * passGain + wet[ch] * distGain;
}

// ---------------------------------------------------------------------------------------
// cAEMagicDistortion — update, mezcla 3-tap y sub-DSP por shape.
// ---------------------------------------------------------------------------------------
void PlugChainBlock::MagicDistortDsp::configure(const DistParams& p, double sr) {
    const float a01 = p.amount * 0.01f;
    mixTarget = p.mix * 0.01f;               // P+0x38
    distGain = dbToGain(p.distDB);           // P+0x3c (sin la mezcla)
    passGain = dbToGain(p.passDB);           // P+0x40
    a1 = onePoleCoeff(p.lowFreq, sr);
    a2 = onePoleCoeff(std::max(p.lowFreq, p.highFreq), sr);
    a3 = onePoleCoeff(p.distLP, sr);
    shape = p.shape;
    legacy = p.legacy;
    switch (shape) {
        case 0: {  // TubePair
            g1Target = 0.3f * std::exp(a01 * 3.0f);          // P+0x53c = 0.3·exp(0.03·amount)
            g2Target = (a01 + 1.0f) / g1Target;              // P+0x540
            tpG1.setLowShelf(poleCoeff(50.0, sr));           // P+0x504 (suaviza g1)
            tpG2.setLowShelf(poleCoeff(50.0, sr));           // P+0x520 (suaviza g2)
            tpHpFix.setHighPass(poleCoeff(25.0, sr)); // P+0x4b0 = 25 Hz
            tpHpGap.setHighPass(poleCoeff(7.0 + 50.0 * a01, sr));      // P+0x4cc = 7+0.5·amount
            tpShelf.setLowShelf(poleCoeff(9000.0 - 5000.0 * a01, sr)); // P+0x4e8 = 9000-50·amount
            break;
        }
        case 1: {  // AirPressure
            const float gainA = std::exp(5.0f * a01) * 0.3f;   // 0.3·exp(0.05·amount)
            apGainTarget = static_cast<float>(std::pow(10.0, 1.0 + a01 * 0.5)) * gainA;  // P+0x298
            apCoefBTarget = static_cast<float>(std::pow(10.0, (4.0f * a01 * a01 - 14.0f * a01) * 0.05f)) *
                            ((a01 + 1.2f + 4.0f * a01 * a01) * 0.125f) / gainA;            // P+0x29c
            apCoefATarget = std::exp2(-(a01 * 4050.0f * 0.33f + 1.0f) / static_cast<float>(sr));  // P+0x2a0
            apGain.setLowShelf(poleCoeff(50.0, sr));    // P+0x2c4 (suaviza la ganancia)
            apCoefB.setLowShelf(poleCoeff(50.0, sr));   // P+0x2e0 (suaviza P+0x29c)
            apCoefA.setLowShelf(poleCoeff(50.0, sr));   // P+0x2fc (suaviza P+0x2a0)
            apHp20.setHighPass(poleCoeff(20.0, sr));    // P+0x2a8
            apDelaySamples = static_cast<float>(sr / 44100.0);   // P+0x338
            apLegacyGain = static_cast<float>(44100.0 / sr);     // P+0x33c
            break;
        }
        case 2: {  // IronTransformer
            ironGainTarget = 0.03f * std::exp(0.02f * p.amount);              // P+0x94
            ironCoefBTarget = (0.005f * p.amount + 1.2f) / ironGainTarget;    // P+0x98
            // Fuga del integrador (P+0x9c = 2^(-(0,25·amount+1)/sr)). En la rama legacy el
            // motor la calcula en el dominio de 44.1 kHz y la reescala con sr/44100
            // a 44.1 kHz las dos ramas coinciden.
            const double leakNum = 0.25 * static_cast<double>(p.amount) + 1.0;
            if (legacy) {
                const double ratio = sr / 44100.0;
                const double leakAt44 = std::exp2(-leakNum / 44100.0);
                ironCoefATarget = static_cast<float>(1.0 - std::pow(std::max(0.0, 1.0 - leakAt44), ratio));
                ironGainTarget = static_cast<float>(ironGainTarget / ratio);
                ironCoefBTarget = static_cast<float>(ironCoefBTarget * ratio);
            } else {
                ironCoefATarget = static_cast<float>(std::exp2(-leakNum / sr));
            }
            ironGainSmooth.setLowShelf(poleCoeff(50.0, sr));    // P+0xdc (suaviza P+0x94)
            ironCoefASmooth.setLowShelf(poleCoeff(50.0, sr));   // P+0x114 (suaviza P+0x9c)
            ironCoefBSmooth.setLowShelf(poleCoeff(50.0, sr));   // P+0xf8 (suaviza P+0x98)
            ironLp.setLowShelf(poleCoeff(20000.0 - 12000.0 * a01, sr));  // P+0xa4
            // Shelf de 5+350·a01 Hz con ganancia 10^(-9·a01/20):
            // dos polos distintos, uno en la frecuencia y otro en frecuencia/ganancia,
            //   A = c2, B = (1-c2)/(gain·(1-c1)), C = -c1·B  ⇒  ganancia DC = 1/gain.
            const float shelfGain = static_cast<float>(std::pow(10.0, -9.0 * a01 / 20.0));
            const float shelfFreq = 5.0 + 350.0 * a01;
            const float poleLo = poleCoeff(shelfFreq, sr);
            const float poleHi = poleCoeff(shelfFreq / shelfGain, sr);
            const float shelfB = (1.0f - poleLo) / (shelfGain * (1.0f - poleHi));
            ironShelf.set(poleLo, shelfB, -poleHi * shelfB);   // P+0xc0
            break;
        }
        default:
            // [?] shapes 3/4/5 de magicDistort (Bits, Balloon, ClippedBalloon) existen en el
            // dispatcher pero el DSL no los pide.
            break;
    }
}

void PlugChainBlock::MagicDistortDsp::reset() {
    clearEnvelope();
    for (int c = 0; c < kMaxChannels; ++c) {
        mixH1[c] = mixH2[c] = 0.0f;
        apAcc[c] = 0.0f;
        apDelayPrev[c] = 0.0f;
        ironAcc[c] = 0.0f;
        ironAtan[c] = 0.0f;
        for (int i = 0; i < kDelaySize; ++i) apDelay[c][i] = 0.0f;
    }
    tpG1.reset();
    tpG2.reset();
    tpHpFix.reset();
    tpHpGap.reset();
    tpShelf.reset();
    apGain.reset();
    apCoefA.reset();
    apCoefB.reset();
    apHp20.reset();
    ironGainSmooth.reset();
    ironCoefASmooth.reset();
    ironCoefBSmooth.reset();
    ironLp.reset();
    ironShelf.reset();
    apWrite = 0;
}

void PlugChainBlock::MagicDistortDsp::clearEnvelope() {
    for (int c = 0; c < kMaxChannels; ++c) lo[c] = band[c] = wet[c] = 0.0f;
}

float PlugChainBlock::MagicDistortDsp::tubePair(int ch, float band) {
    const float g1 = tpG1.process(ch, g1Target);
    const float g2 = tpG2.process(ch, g2Target);
    const float v1 = g1 * band;
    const float sat1 = saturator(v1);
    float y = tpHpFix.process(ch, g2 * sat1);
    const float v2 = g1 * y;
    const float sat2 = saturator(v2);
    y = tpHpGap.process(ch, g2 * sat2);
    return tpShelf.process(ch, y);
}

float PlugChainBlock::MagicDistortDsp::airPressure(int ch, float band) {
    const float u = apGain.process(ch, apGainTarget) * band;
    float w;
    if (legacy) {
        // Retardo fraccionario de sr/44100 muestras con Catmull-Rom de 4 puntos sobre un
        // ring de 16 muestras por canal. A 44.1 kHz el retardo es 1 muestra
        // exacta y coincide con la rama moderna.
        apWrite = (apWrite + 1) & (kDelaySize - 1);
        apDelay[ch][apWrite] = u;
        const int intDelay = static_cast<int>(apDelaySamples);
        const float frac = apDelaySamples - static_cast<float>(intDelay);
        auto at = [&](int off) {
            return apDelay[ch][(apWrite - intDelay + off) & (kDelaySize - 1)];
        };
        const float y1 = at(1), y0 = at(0), ym1 = at(-1), ym2 = at(-2);
        const float interp =
            y0 + frac * 0.5f *
                     ((ym1 - y1) +
                      frac * (((y1 + y1 + y0 * -5.0f + ym1 * 4.0f) - ym2) +
                              frac * ((y0 * 3.0f + (ym2 - ym1 * 3.0f)) - y1)));
        w = std::atan(u - interp) * apLegacyGain;
    } else {
        const float prev = apDelayPrev[ch];
        apDelayPrev[ch] = u;
        w = std::atan(u - prev);
    }
    // La fuga va DENTRO del acumulador (se reescribe en el estado, no actúa sólo como
    // ganancia de salida). Aplicarla sólo a la salida deja la recursión sin fuga y el
    // compander conserva el grave que debería recortar.
    apAcc[ch] = (apAcc[ch] + w) * apCoefA.process(ch, apCoefATarget);
    float y = apAcc[ch] * apCoefB.process(ch, apCoefBTarget);
    return apHp20.process(ch, y);
}

float PlugChainBlock::MagicDistortDsp::ironTransformer(int ch, float band) {
    const float g = ironGainSmooth.process(ch, ironGainTarget);
    ironAcc[ch] += g * band;
    ironAcc[ch] *= ironCoefASmooth.process(ch, ironCoefATarget);
    const float z = std::atan(ironAcc[ch]);
    const float dz = z - ironAtan[ch];
    ironAtan[ch] = z;
    const float dy = dz * ironCoefBSmooth.process(ch, ironCoefBTarget);
    const float y = ironLp.process(ch, dy);
    return ironShelf.process(ch, y);
}

float PlugChainBlock::MagicDistortDsp::process(int ch, float x) {
    const float loNew = lo[ch] + a1 * (x - lo[ch]);
    const float bandNew = band[ch] + a2 * ((x - loNew) - band[ch]);
    lo[ch] = loNew;
    band[ch] = bandNew;

    float shaped;
    switch (shape) {
        case 0: shaped = tubePair(ch, bandNew); break;
        case 1: shaped = airPressure(ch, bandNew); break;
        case 2: shaped = ironTransformer(ch, bandNew); break;
        default: shaped = bandNew; break;
    }
    wet[ch] = wet[ch] + a3 * denorm(shaped - wet[ch]);

// Mezcla suavizada 3-tap.
    const float m = mixTarget * mixC1 + mixH2[ch] * mixC2 + mixH1[ch] * mixC0;
    mixH2[ch] = mixTarget;
    mixH1[ch] = m;
    const float outv = x * (1.0f - m) + (x - bandNew) * m * passGain + wet[ch] * m * distGain;
    return outv;
}

PlugChainBlock::PlugChainBlock(const ParamStore* params, const std::string& text, const std::string& prefix)
    : params_(params), prefix_(prefix) {
    std::string err;
    if (!parsePlugChain(text, chain_, err)) parseError_ = err;
    stages_.reserve(chain_.nodes.size());
    for (const Node& n : chain_.nodes) {
        Stage s;
        s.kindName = n.kind;
        s.kind = kindFromName(n.kind);
        s.node = &n;
        stages_.push_back(s);
    }
}

PlugChainBlock::~PlugChainBlock() = default;

int PlugChainBlock::stageCount() const { return static_cast<int>(stages_.size()); }

int PlugChainBlock::implementedStageCount() const {
    int n = 0;
    for (const auto& s : stages_) n += (s.kind != Kind::Unsupported) ? 1 : 0;
    return n;
}

int PlugChainBlock::unsupportedStageCount() const {
    return stageCount() - implementedStageCount();
}

std::string PlugChainBlock::stageKind(int i) const {
    return (i >= 0 && i < stageCount()) ? stages_[static_cast<size_t>(i)].kindName : std::string();
}

float PlugChainBlock::lookup(const std::string& id) const {
    if (!params_) return 0.0f;
    if (params_->hasModel(id)) return params_->get(id.c_str());
    const std::string p = prefix_ + id;
    if (params_->hasModel(p)) return params_->get(p.c_str());
    return 0.0f;
}

double PlugChainBlock::resolveParam(const ParamLine* line, double fallback) const {
    if (!line || line->args.empty()) return fallback;
    const Arg& a = line->args[0];
    if (a.kind == ArgKind::Number) return a.number;
    if (a.kind == ArgKind::Param || a.kind == ArgKind::ScopedParam) return lookup(a.text);
    return fallback;
}

double PlugChainBlock::lineValue(const ParamLine* line, double fallback) const {
    if (!line || line->args.empty()) return fallback;
    if (line->args[0].kind == ArgKind::Number) return line->args[0].number;
    if (line->args.size() < 3) return resolveParam(line, fallback);

    const char* scale = "lin";
    double curve = 0.5;
    for (size_t i = 0; i < line->args.size(); ++i) {
        if (line->args[i].kind != ArgKind::Keyword) continue;
        scale = line->args[i].text.c_str();
        // La curva de `sym` viaja como el número que sigue a la palabra de escala
        // (`db (Amount) 10 12 sym 0.15`): el token posterior es el parámetro c.
        if (i + 1 < line->args.size() && line->args[i + 1].kind == ArgKind::Number)
            curve = line->args[i + 1].number;
    }

    // Si el parámetro del modelo ya está en unidades reales (rango distinto de 0..1) se
    // usa tal cual: el DSL no vuelve a escalarlo. Si es normalizado, se aplica
    // min/max/scale de la propia línea. Regla verificada en el gate de la fase 2.
    float mlo = 0.0f, mhi = 1.0f;
    const bool real = params_->modelRange(line->args[0].text, mlo, mhi) &&
                      !(mlo == 0.0f && mhi == 1.0f);
    if (real) return resolveParam(line, fallback);
    return applyScale(resolveParam(line, fallback), line->args[1].number, line->args[2].number, scale, curve);
}

double PlugChainBlock::applyScale(double v, double lo, double hi, const char* scale, double curve) {
    const std::string s = scale ? scale : "";
    if (s == "log" || s == "logcut") return freqLaw(v, lo, hi);
    if (s == "sqr") return sqrLaw(v, lo, hi);
    if (s == "sym") return symLaw(v, lo, hi, curve);
    if (s == "lin" || s.empty()) return lo + (hi - lo) * v;
    return lo + (hi - lo) * v;
}

void PlugChainBlock::resolveDistParams(const Node& node, Kind kind, DistParams& dp) const {
// Defaults del bloque distorsionador:
    // mix = 100 (m = 1), passDB = 0 (ganancia 1), distLP = 20000, refLevel = 0 (d = 1).
    dp = DistParams();
    const ParamLine* amount = node.find("amount");
    const ParamLine* shapeLine = node.find("shape");

    // `amount` sin min/max en el DSL (`amount Amount` en las shapes 0/2/5) llega normalizado
    // 0..1 y la clase lo espera en 0..100: escala implícita `lin` sobre el rango declarado
    // de la clase. Con min/max explícitos se aplica la curva de la línea
    // (`amount Amount 0 75 lin` en AirPressure, `amount Amount 0 100 sym 0.4` en Iron).
    if (amount && amount->args.size() < 3)
        dp.amount = static_cast<float>(resolveParam(amount, 0.0) * 100.0);
    else
        dp.amount = static_cast<float>(lineValue(amount, 0.0));

    // `shape` es constante en el DSL (`shape Type n n`): el valor de la línea.
    dp.shape = static_cast<int>(std::lround(lineValue(shapeLine, 0.0)));

    dp.lowFreq = static_cast<float>(lineValue(node.find("lowFreq"), 20.0));
    dp.highFreq = static_cast<float>(lineValue(node.find("highFreq"), 20000.0));
    dp.refLevel = static_cast<float>(lineValue(node.find("refLevel"), 0.0));
    dp.distDB = static_cast<float>(lineValue(node.find("distDB"), 0.0));
    dp.passDB = static_cast<float>(lineValue(node.find("passDB"), 0.0));
    dp.distLP = static_cast<float>(lineValue(node.find("distLP"), 20000.0));
    dp.mix = static_cast<float>(lineValue(node.find("mix"), 100.0));
    if (dp.lowFreq < 1.0f) dp.lowFreq = 1.0f;
    if (dp.highFreq < 1.0f) dp.highFreq = 1.0f;
    if (dp.distLP < 1.0f) dp.distLP = 1.0f;
    // `useLegacyBehaviour` (P+0x80) no lo escribe el DSL: default 0 ⇒ rama legacy activa.
    dp.legacy = (kind == Kind::MagicDistort);
}

void PlugChainBlock::prepare(const PrepareContext& ctx) {
    sampleRate_ = ctx.sampleRate > 0.0 ? ctx.sampleRate : 44100.0;
    reset();
}

void PlugChainBlock::reset() {
    for (auto& s : stages_) {
        for (auto& b : s.bq) b.reset();
        s.lastFreq = -1.0;
        s.lastQ = -1.0;
        s.lastDb = 0.0;
        s.lastActive = false;
        s.lastDpValid = false;
        s.dist.reset();
        s.magic.reset();
    }
}

void PlugChainBlock::updateStage(Stage& s) {
    if (!s.node || !params_) return;

    // Las condiciones [enabled] usan el nombre local del módulo ("CutSoft_Lo_Enable");
    // se evalúan contra un ParamSource que antepone el prefijo.
    PrefixedParams prefixed(this);
    const StageEvaluation ev = evaluateStage(*s.node, prefixed);
    s.active = ev.active;
    if (s.kind == Kind::Unsupported) return;

    if (s.kind == Kind::Gain) {
        // `db` con su escala: constante (`db -12`), sobre el rango de la línea
        // (`db Amount -19 0`, `db Amount 0 22`) o con curva. `gain` (multiplicador lineal)
        // y `legacyReset` del DSL no se usan: El motor aplica la ganancia por muestra.
        s.gain = static_cast<float>(std::pow(10.0, lineValue(s.node->find("db"), 0.0) / 20.0));
        return;
    }

    if (s.kind == Kind::Distort || s.kind == Kind::MagicDistort) {
        resolveDistParams(*s.node, s.kind, s.dp);
        if (s.lastDpValid && std::memcmp(&s.dp, &s.lastDp, sizeof(DistParams)) == 0) return;
        s.lastDp = s.dp;
        s.lastDpValid = true;
        if (s.kind == Kind::Distort) s.dist.configure(s.dp, sampleRate_);
        else s.magic.configure(s.dp, sampleRate_);
        return;
    }

    const ParamLine* freqLine = s.node->find("freq");
    const ParamLine* qLine = s.node->find("q");
    const ParamLine* dbLine = s.node->find("db");

    double freq = lineValue(freqLine, 1000.0);
    if (freq < 1.0) freq = 1.0;
    if (freq > 0.49 * sampleRate_) freq = 0.49 * sampleRate_;

    // `q` se escala igual que `freq`: el `q 0.1 5 log` del master en modo duro da
    // q = 0.1*50^v, es decir 0.707 con CutHard_HiQ = 0.5 y 2.291 con CutHard_LoQ = 0.8.
    // ajustado por medida: lp12 x2 a 8.78 kHz con q = 0.707 → 0.03 dB rms; con
    // q = 0.8 (sin escalar) el error era 0.72 dB rms / 2.15 dB peor caso.
    double q = lineValue(qLine, 0.70710678);
    if (q < 0.05) q = 0.05;

    // `db` con la curva de su línea: `sqr` en el EQ de 2 bandas y el Tone, `sym 0.15`
    // en los bpPeak que acompañan a las shapes 1 de la cadena Dist.
    const double db = lineValue(dbLine, 0.0);

    if (s.lastActive == s.active && s.lastFreq == freq && s.lastQ == q && s.lastDb == db && s.lastFreq > 0.0)
        return;

    for (auto& b : s.bq) {
        switch (s.kind) {
            case Kind::Hp12: b.setHighPass(sampleRate_, freq, q, 2); break;
            case Kind::Lp12: b.setLowPass(sampleRate_, freq, q, 2); break;
            case Kind::Hp6: {
                const CheapOnePole f = cheapHighPass(sampleRate_, freq);
                b.setCoefficients(f.b0, f.b1, 0.0, f.a1, 0.0);
                break;
            }
            case Kind::Lp6: {
                const CheapOnePole f = cheapLowPass(sampleRate_, freq);
                b.setCoefficients(f.b0, f.b1, 0.0, f.a1, 0.0);
                break;
            }
            case Kind::BpPeak: b.setPeaking(sampleRate_, freq, db, q); break;
            case Kind::HpShelf: b.setHighShelf(sampleRate_, freq, db, q); break;
            default: b.setBypass(); break;
        }
    }
    s.lastFreq = freq;
    s.lastQ = q;
    s.lastDb = db;
    s.lastActive = s.active;
}

void PlugChainBlock::process(const PortData* in, PortData& out) {
    const int n = out.numSamples > 0 ? out.numSamples : (in ? in->numSamples : 0);
    if (n <= 0) return;

    for (int c = 0; c < out.numChannels; ++c) {
        const bool has = (in && c < in->numChannels && in->ch[c]);
        const float* src = has ? in->ch[c] : nullptr;
        for (int i = 0; i < n; ++i) out.ch[c][i] = src ? src[i] : 0.0f;
    }

    for (auto& s : stages_) {
        updateStage(s);
        if (!s.active) {
        // El `bypass` del motor va invertido (`[enabled]` fuera de rango ⇒ bypass) y en
        // bypass sólo limpia la envolvente sin tocar el buffer (:22-31,
        // 19-28). Type 4 no activa ninguna etapa ⇒ cadena unidad.
            if (s.kind == Kind::Distort) s.dist.clearEnvelope();
            if (s.kind == Kind::MagicDistort) s.magic.clearEnvelope();
            continue;
        }
        if (s.kind == Kind::Gain) {
        if (s.gain == 1.0f) continue; // El motor no multiplica cuando la ganancia es 1
            for (int c = 0; c < out.numChannels; ++c)
                for (int i = 0; i < n; ++i) out.ch[c][i] *= s.gain;
            continue;
        }
        if (s.kind == Kind::Distort) {
            for (int c = 0; c < out.numChannels; ++c) {
                const int ch = c < kMaxChannels ? c : kMaxChannels - 1;
                for (int i = 0; i < n; ++i) out.ch[c][i] = s.dist.process(ch, out.ch[c][i]);
            }
            continue;
        }
        if (s.kind == Kind::MagicDistort) {
            for (int c = 0; c < out.numChannels; ++c) {
                const int ch = c < kMaxChannels ? c : kMaxChannels - 1;
                for (int i = 0; i < n; ++i) out.ch[c][i] = s.magic.process(ch, out.ch[c][i]);
            }
            continue;
        }
        if (s.kind == Kind::Unsupported) continue;
        for (int c = 0; c < out.numChannels; ++c) {
            dsp::Biquad& b = s.bq[c < kMaxChannels ? c : kMaxChannels - 1];
            for (int i = 0; i < n; ++i) out.ch[c][i] = b.process(out.ch[c][i]);
        }
    }
}

} // namespace rtxx
