// Espacio (fase 5 del ROADMAP): cResoX — 12 resonadores en paralelo + 4 allpass + pre-delay.
//
// Fija las ecuaciones cerradas de src/blocks/ResoX.cpp para que no deriven. La comparación
// completa (T60 por banda, cola, pico, Amount, pre-delay, estéreo y estructura modal) está
// en la suite de medidas.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#include "blocks/ResoX.h"
#include "model/ParamStore.h"

namespace {

int failures = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FALLO: %s\n", what);
        ++failures;
    }
}
void near(double a, double b, const char* what, double tol = 1e-6) {
    if (std::fabs(a - b) > tol) {
        std::printf("FALLO: %s (%.9f != %.9f)\n", what, a, b);
        ++failures;
    }
}

constexpr int kRate = 44100;

// Store con el nodo Space encendido y los valores de fábrica (mismos ids que el gate).
rtxx::ParamStore makeStore() {
    rtxx::ParamStore p;
    p.resetToDefaults();
    p.setHost(35, 1.0f);   // SPAC Enable
    p.setHost(5, 1.0f);    // SPAC Amount = 100 % wet
    p.setHost(36, 0.8f);   // SPAC Decay
    p.setHost(37, 0.0f);   // Filter Lo  = 100 Hz
    p.setHost(38, 1.0f);   // Filter Hi  = 16 kHz
    p.setHost(39, 0.0f);   // PreDelay = 0
    p.setHost(40, 1.0f);   // StereoMode = estéreo
    return p;
}

std::vector<float> noise(double seconds, float amp = 0.25f) {
    const int n = static_cast<int>(seconds * kRate);
    std::vector<float> x(static_cast<size_t>(n));
    unsigned long long s = 12345u;
    for (int i = 0; i < n; ++i) {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        const double u = static_cast<double>((s >> 33) & 0x7fffffff) / 1073741823.5 - 1.0;
        x[static_cast<size_t>(i)] = static_cast<float>(amp * u);
    }
    return x;
}

std::vector<float> run(rtxx::iBlock& block, const std::vector<float>& in, int nch = 2) {
    rtxx::PrepareContext ctx;
    ctx.sampleRate = kRate;
    block.prepare(ctx);
    std::vector<float> out(in.size(), 0.0f);
    for (int start = 0; start < static_cast<int>(in.size()); start += rtxx::kQuantum) {
        const int n = std::min(rtxx::kQuantum, static_cast<int>(in.size()) - start);
        rtxx::PortData pin;
        pin.numChannels = nch;
        pin.numSamples = n;
        pin.ch[0] = const_cast<float*>(in.data() + start);
        pin.ch[1] = const_cast<float*>(in.data() + start);
        rtxx::PortData pout;
        pout.numChannels = nch;
        pout.numSamples = n;
        pout.ch[0] = out.data() + start;
        pout.ch[1] = out.data() + start;
        block.process(&pin, pout);
    }
    return out;
}

// Leyes de la red -------------------------------------------------------------

void testGeometry() {
// f_k = 440·2^((22+k−69)/12): MIDI 22..33 (29,14…55,00 Hz). ajustado por medida por el
// espaciado de peine: 29,115 / 30,87 / … / 54,99 Hz ⇒ ±1,5 cents.
    const double midi = 440.0 * std::pow(2.0, (22 - 69) / 12.0);
    near(rtxx::ResoXBlock::lineFreqHz(0), midi, "f_0 = MIDI 22");
    near(rtxx::ResoXBlock::lineFreqHz(11), 440.0 * std::pow(2.0, (33 - 69) / 12.0), "f_11 = MIDI 33");
    near(rtxx::ResoXBlock::lineFreqHz(0), 29.13524, "29,14 Hz", 1e-4);
    near(rtxx::ResoXBlock::lineFreqHz(11), 54.99999, "55,00 Hz", 1e-4);
    // La relación entre líneas consecutivas es un semitono (2^(1/12)).
    near(rtxx::ResoXBlock::lineFreqHz(1) / rtxx::ResoXBlock::lineFreqHz(0),
         std::pow(2.0, 1.0 / 12.0), "un semitono entre líneas", 1e-12);

    // Longitudes de los 4 allpass (segundos) = las de Freeverb.
    near(rtxx::ResoXBlock::allpassSeconds(0), 0.0126077095, "allpass 0");
    near(rtxx::ResoXBlock::allpassSeconds(1), 0.01, "allpass 1");
    near(rtxx::ResoXBlock::allpassSeconds(2), 0.0077324263, "allpass 2");
    near(rtxx::ResoXBlock::allpassSeconds(3), 0.0051020407, "allpass 3");
    // En muestras a 44,1 kHz: 556 / 441 / 341 / 225 (las de Freeverb).
    const double sr = 44100.0;
    near(std::floor(sr * rtxx::ResoXBlock::allpassSeconds(0) + 0.5), 556.0, "556 muestras", 0);
    near(std::floor(sr * rtxx::ResoXBlock::allpassSeconds(2) + 0.5), 341.0, "341 muestras", 0);
}

void testDecayAndGain() {
// Ley MEDIDA: 1/T60(1 kHz) = 0,847 − 0,833·Decay ⇒ T60 = 5,537 s a Decay 0,8.
    near(rtxx::ResoXBlock::t60FromDecay(0.8f), 1.0 / (0.847 - 0.833 * 0.8), "T60 a Decay 0,8");
    near(rtxx::ResoXBlock::t60FromDecay(0.0f), 1.0 / 0.847, "T60 a Decay 0");
    check(rtxx::ResoXBlock::t60FromDecay(0.5f) > rtxx::ResoXBlock::t60FromDecay(0.25f),
          "el T60 crece con Decay");
    check(rtxx::ResoXBlock::t60FromDecay(1.0f) > 50.0, "Decay 1 se acota a un T60 largo");

    // Ganancia de lazo por línea: G = 10^(−3·N/(sr·T60)); con T60 = 5,537 s y N = 1328.
    const double g = rtxx::ResoXBlock::loopGain(5.537f, 1328, 44100.0);
    near(g, std::pow(10.0, -3.0 * 1328.0 / (44100.0 * 5.537)), "G de la línea", 1e-6);
    check(g > 0.9 && g < 1.0, "G está por debajo de 1 (estable)");
    // Una línea MÁS LARGA necesita MENOS ganancia de lazo para el mismo T60 (menos pasadas
    // por segundo ⇒ cada pasada puede perder más).
    check(rtxx::ResoXBlock::loopGain(5.537f, 1513, 44100.0) <
              rtxx::ResoXBlock::loopGain(5.537f, 802, 44100.0),
          "G decrece con la longitud de la línea");
    // Sin retardo no hay lazo.
    near(rtxx::ResoXBlock::loopGain(5.537f, 0, 44100.0), 0.0, "N = 0 ⇒ G = 0");
}

void testMix() {
// Ley MEDIDA: wet = sin(π/2·rsmul(v, 0,15)), dry = cos(·), residuo 1,6e-6 dB.
    float dry = 0.0f, wet = 0.0f;
    rtxx::ResoXBlock::dryWet(0.0f, dry, wet);
    near(wet, 0.0, "Amount 0 ⇒ seco puro", 1e-7);
    near(dry, 1.0, "Amount 0 ⇒ seco unidad", 1e-7);
    rtxx::ResoXBlock::dryWet(1.0f, dry, wet);
    near(wet, 1.0, "Amount 1 ⇒ wet puro", 1e-7);
    near(dry, 0.0, "Amount 1 ⇒ sin seco", 1e-7);
    for (float v : {0.1f, 0.25f, 0.5f, 0.75f, 0.9f}) {
        rtxx::ResoXBlock::dryWet(v, dry, wet);
        near(dry * dry + wet * wet, 1.0, "potencia constante", 1e-6);
        const float m = rtxx::ResoXBlock::rsmul(v, 0.15f);
        near(wet, std::sin(M_PI * 0.5 * m), "wet = sin(π/2·rsmul)", 1e-6);
    }
    // rsmul: 0 → 0, 1 → 1, 0,5 → c.
    near(rtxx::ResoXBlock::rsmul(0.0f, 0.15f), 0.0, "rsmul(0) = 0", 1e-7);
    near(rtxx::ResoXBlock::rsmul(1.0f, 0.15f), 1.0, "rsmul(1) = 1", 1e-6);
    near(rtxx::ResoXBlock::rsmul(0.5f, 0.15f), 0.15, "rsmul(0,5) = c", 1e-7);

    // Coeficiente de los filtros del lazo: g = exp(−2π·f/sr).
    near(rtxx::ResoXBlock::filterGain(100.0f, 44100.0), std::exp(-2.0 * M_PI * 100.0 / 44100.0),
         "g a 100 Hz", 1e-6);
    near(rtxx::ResoXBlock::filterGain(16000.0f, 44100.0), std::exp(-2.0 * M_PI * 16000.0 / 44100.0),
         "g a 16 kHz", 1e-6);
}

// Comportamiento del bloque --------------------------------------------------------

void testBypassAndMix() {
    const std::vector<float> in = noise(0.25);

    {
        rtxx::ParamStore p = makeStore();
        p.setHost(35, 0.0f);
        rtxx::ResoXBlock blk(&p);
        const std::vector<float> out = run(blk, in);
        bool same = true;
        for (size_t i = 0; i < in.size(); ++i) same = same && (out[i] == in[i]);
        check(same, "SPAC Enable = 0 ⇒ passthrough bit a bit");
    }
    {
        rtxx::ParamStore p = makeStore();
        p.setHost(5, 0.0f);   // Amount = 0 ⇒ sin reverb
        rtxx::ResoXBlock blk(&p);
        const std::vector<float> out = run(blk, in);
        bool same = true;
        for (size_t i = 0; i < in.size(); ++i) same = same && (out[i] == in[i]);
        check(same, "Amount = 0 ⇒ seco bit a bit");
    }
}

void testStereoAndGeometry() {
    const std::vector<float> in = noise(1.0);

    // StereoMode = 0: los dos canales idénticos bit a bit; = 1: distintos.
    {
        rtxx::ParamStore p = makeStore();
        p.setHost(40, 0.0f);
        rtxx::ResoXBlock blk(&p);
        blk.prepare(rtxx::PrepareContext{static_cast<double>(kRate), rtxx::kQuantum});
        std::vector<float> out(in.size(), 0.0f);
        for (int start = 0; start < static_cast<int>(in.size()); start += rtxx::kQuantum) {
            const int n = std::min(rtxx::kQuantum, static_cast<int>(in.size()) - start);
            rtxx::PortData pin;
            pin.numChannels = 2;
            pin.numSamples = n;
            pin.ch[0] = const_cast<float*>(in.data() + start);
            pin.ch[1] = const_cast<float*>(in.data() + start);
            rtxx::PortData pout;
            pout.numChannels = 2;
            pout.numSamples = n;
            pout.ch[0] = out.data() + start;
            std::vector<float> r(static_cast<size_t>(n), 0.0f);
            pout.ch[1] = r.data();
            blk.process(&pin, pout);
            for (int i = 0; i < n; ++i) check(out[start + i] == r[static_cast<size_t>(i)],
                                              "mono: los dos canales coinciden");
        }
        // Geometría: 12 líneas, longitudes decrecientes = round(sr/f_k) sin detune.
        const auto& d = blk.diag();
        check(d.lines == 12, "12 líneas");
        bool ok = true;
        for (int k = 0; k < 12; ++k) {
            const double want = std::floor(44100.0 / rtxx::ResoXBlock::lineFreqHz(k) + 0.5);
            ok = ok && std::fabs(d.samples[k] - want) <= 1.0;
            ok = ok && (k == 0 || d.samples[k] < d.samples[k - 1]);
        }
        check(ok, "longitudes = round(sr/f_k) y decrecientes");
        check(d.samples[11] >= 800 && d.samples[11] <= 806, "la línea más corta es ~802 muestras");
        check(d.allpass[0] == 556 || d.allpass[0] == 557, "primer allpass ~556 muestras");
    }
}

// Estabilidad: con 12 lazos realimentados y Decay alto no puede divergir.
void testStability() {
    const std::vector<float> in = noise(1.0, 0.5f);
    rtxx::ParamStore p = makeStore();
    p.setHost(36, 0.99f);   // Decay casi al máximo (T60 ~ 60 s)
    rtxx::ResoXBlock blk(&p);
    const std::vector<float> out = run(blk, in);
    double peak = 0.0;
    bool finite = true;
    for (float v : out) {
        finite = finite && std::isfinite(v);
        peak = std::max(peak, std::fabs(static_cast<double>(v)));
    }
    check(finite, "la salida es finita con Decay 0,99");
    check(peak < 10.0, "no diverge (pico < 10)");
    std::printf("   pico con Decay 0,99: %.3f\n", peak);
}

} // namespace

int main() {
    testGeometry();
    testDecayAndGain();
    testMix();
    testBypassAndMix();
    testStereoAndGeometry();
    testStability();
    if (failures == 0) {
        std::printf("PASS test_resox\n");
        return 0;
    }
    std::printf("FAIL test_resox (%d comprobaciones)\n", failures);
    return 1;
}
