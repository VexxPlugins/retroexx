// Modulación (fase 4 del ROADMAP): cWobbler (retardo) y cFlutter (amplitud).
//
// Fija las ecuaciones cerradas para que no deriven. La comparación en cents está en la
// suite de medidas.
//
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#include "blocks/Flutter.h"
#include "blocks/Wobbler.h"
#include "model/ParamStore.h"

namespace {

int failures = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FALLO: %s\n", what);
        ++failures;
    }
}
void near(float a, float b, const char* what, float tol = 1e-5f) {
    if (std::fabs(a - b) > tol) {
        std::printf("FALLO: %s (%.7f != %.7f)\n", what, static_cast<double>(a),
                    static_cast<double>(b));
        ++failures;
    }
}

constexpr int kRate = 44100;
constexpr int kFrames = 16384;

// Procesa `frames` muestras por el bloque, en lotes de quantum, y devuelve el canal 0.
std::vector<float> run(rtxx::iBlock& block, const std::vector<float>& in, double sr = kRate) {
    rtxx::PrepareContext ctx;
    ctx.sampleRate = sr;
    block.prepare(ctx);
    std::vector<float> out(in.size(), 0.0f);
    const int block_ = rtxx::kQuantum;
    for (int start = 0; start < static_cast<int>(in.size()); start += block_) {
        const int n = std::min(block_, static_cast<int>(in.size()) - start);
        rtxx::PortData pin;
        pin.numChannels = 1;
        pin.numSamples = n;
        pin.ch[0] = const_cast<float*>(in.data() + start);
        rtxx::PortData pout;
        pout.numChannels = 1;
        pout.numSamples = n;
        pout.ch[0] = out.data() + start;
        block.process(&pin, pout);
    }
    return out;
}

std::vector<float> sine(double seconds, double amp = 0.1, double freq = 1000.0) {
    const int n = static_cast<int>(seconds * kRate);
    std::vector<float> x(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i)
        x[static_cast<size_t>(i)] =
            static_cast<float>(amp * std::sin(2.0 * M_PI * freq * i / kRate));
    return x;
}

std::vector<float> impulse(int frames, int at = 0) {
    std::vector<float> x(static_cast<size_t>(frames), 0.0f);
    x[static_cast<size_t>(at)] = 1.0f;
    return x;
}

int argmaxAbs(const std::vector<float>& x, int from, int to) {
    int best = from;
    for (int i = from; i < to && i < static_cast<int>(x.size()); ++i)
        if (std::fabs(x[static_cast<size_t>(i)]) > std::fabs(x[static_cast<size_t>(best)])) best = i;
    return best;
}

// Envolvente de pico con ventana deslizante (para medir la AM del cFlutter). La ventana
// tiene que cubrir un periodo del tono (44 muestras a 1 kHz) pero ser corta frente al del
// LFO: con 64 muestras a 6 Hz el error del máximo sostenido es 0.9 %.
void envelope(const std::vector<float>& y, int from, float& lo, float& hi, int w = 64) {
    lo = 1e9f;
    hi = 0.0f;
    for (int i = from; i + w < static_cast<int>(y.size()); i += 64) {
        float m = 0.0f;
        for (int k = 0; k < w; ++k) m = std::max(m, std::fabs(y[static_cast<size_t>(i + k)]));
        lo = std::min(lo, m);
        hi = std::max(hi, m);
    }
}

void modelParams(rtxx::ParamStore& p, float enable, float depth, float depth2, float mix,
                 float rate, float stereoPhase) {
    p.setModel("Wobble_Enable", enable);
    p.setModel("Wobble_Depth", depth);
    p.setModel("Wobble_Depth2", depth2);
    p.setModel("Wobble_Mix", mix);
    p.setModel("Wobble_Rate", rate);
    p.setModel("Wobble_Rate2", 0.0f);
    p.setModel("Wobble_StereoPhase", stereoPhase);
    p.setModel("Wobble_LfoNoise", 0.0f);
    p.setModel("Wobble_LfoNoiseScaler", 0.0f);
}

} // namespace

int main() {
    using rtxx::WobblerBlock;

// --- 1. D = 2·floor(0.02·sr)+1 ------------------------------
    near(static_cast<float>(WobblerBlock::delayRangeSamples(44100.0)), 1765.0f, "D @44.1k = 1765");
    near(static_cast<float>(WobblerBlock::delayRangeSamples(48000.0)), 1921.0f, "D @48k = 1921");
    near(static_cast<float>(WobblerBlock::delayRangeSamples(88200.0)), 3529.0f, "D @88.2k = 3529");
    near(static_cast<float>(WobblerBlock::delayRangeSamples(96000.0)), 3841.0f, "D @96k = 3841");
    near(static_cast<float>(WobblerBlock::delayRangeSamples(0.0)), 1765.0f,
         "D con sr inválido cae a 44.1 kHz");

// --- 2. One-pole del retardo: c = exp(−2π·10/sr) ------------------------------
    near(WobblerBlock::smoothingCoef(44100.0), static_cast<float>(std::exp(-2.0 * M_PI * 10.0 /
                                                                          44100.0)),
         "c @44.1k = exp(−2π·10/sr)", 1e-7f);
    near(WobblerBlock::smoothingCoef(0.0), static_cast<float>(std::exp(-2.0 * M_PI * 10.0 / 44100.0)),
         "c con sr inválido cae a 44.1 kHz", 1e-7f);
    near(static_cast<float>(WobblerBlock::smoothingCoef(44100.0)) +
             (1.0f - WobblerBlock::smoothingCoef(44100.0)),
         1.0f, "k + c = 1", 1e-7f);

// --- 3. mod = clamp(clamp(m + Depth·(L+1)/2, 0, Mix) + Depth2·(L2+1)/2, 0, 1) ---
    near(WobblerBlock::modulation(0.8f, 0.0f, 1.0f, 0.0f, 1.0f), 0.8f, "L=+1 con Depth 0.8 → 0.8");
    near(WobblerBlock::modulation(0.8f, 0.0f, -1.0f, 0.0f, 1.0f), 0.0f, "L=−1 → 0");
    near(WobblerBlock::modulation(1.0f, 0.0f, 1.0f, 0.0f, 0.5f), 0.5f,
         "el primer clamp es a Mix, no a 1");
    near(WobblerBlock::modulation(1.0f, 0.2f, 1.0f, 1.0f, 0.5f), 0.7f,
         "Depth2 se suma DESPUÉS del clamp a Mix");
    near(WobblerBlock::modulation(1.0f, 0.5f, 1.0f, 1.0f, 1.0f), 1.0f, "el segundo clamp es a 1");
    near(WobblerBlock::modulation(0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.1f), 0.1f,
         "el ruido entra en el término del wow (antes del clamp)");
    near(WobblerBlock::modulation(0.0f, 0.0f, 0.0f, 0.0f, 1.0f, -0.5f), 0.0f,
         "el ruido negativo se recorta a 0");

// --- 4. Interpolación ------------------------------------
    // Catmull-Rom con (a,b,c,e) = (buf[i−2..i+1]) y f = d − floor(d):
    //   y = c + f/2·((b−e) + f·(2e−5c+4b−a + f·(3c+a−3b−e)))
    near(WobblerBlock::catmullRom(0.0f, 0.0f, 1.0f, 0.0f, 0.0f), 1.0f, "f=0 devuelve c");
    near(WobblerBlock::catmullRom(0.0f, 1.0f, 0.0f, 0.0f, 1.0f), 1.0f, "f=1 devuelve b");
    // Con una rampa x[n]=n los taps son (k−2, k−1, k, k+1) y la interpolación da k − f:
    // La convención es índice de lectura k menos la fracción f, no k + f.
    for (float f : {0.0f, 0.25f, 0.5f, 0.75f}) {
        const float y = WobblerBlock::catmullRom(10.0f - 2.0f, 10.0f - 1.0f, 10.0f, 10.0f + 1.0f, f);
        near(y, 10.0f - f, "rampa: catmull devuelve k − f", 1e-5f);
        near(WobblerBlock::linearInterp(10.0f - 1.0f, 10.0f, f), 10.0f - f,
             "rampa: la lineal también devuelve k − f", 1e-5f);
    }
    near(WobblerBlock::linearInterp(0.0f, 1.0f, 0.0f), 1.0f, "lineal f=0 → c");
    near(WobblerBlock::linearInterp(0.0f, 1.0f, 1.0f), 0.0f, "lineal f=1 → b");

// --- 5. Forma del LFO: fallback ---------------------------------------
    using rtxx::lfo::fallbackSine;
    near(fallbackSine(0.0f), 0.0f, "seno por defecto: 0 en fase 0", 5e-4f);
    near(fallbackSine(0.25f), 0.9988062f, "pico 0.9988062 en 0.25", 1e-6f);
    near(fallbackSine(0.5f), 0.0f, "0 en fase 0.5", 5e-4f);
    near(fallbackSine(0.75f), -0.9988062f, "−0.9988062 en 0.75", 1e-6f);
    near(fallbackSine(1.25f), fallbackSine(0.25f), "periódico (1.25 = 0.25)", 1e-6f);
    near(fallbackSine(-0.25f), fallbackSine(0.75f), "periódico (negativo)", 1e-6f);
    check(rtxx::lfo::shapeFromWaveShape(0) == rtxx::lfo::Shape::FallbackSine,
          "WaveShape 0 → fallback");
    check(rtxx::lfo::shapeFromWaveShape(12) == rtxx::lfo::Shape::FallbackSine,
          "WaveShape 12 → fallback (fuera de 1..11)");
    check(rtxx::lfo::shapeFromWaveShape(-3) == rtxx::lfo::Shape::FallbackSine,
          "WaveShape negativo → fallback");
    check(rtxx::lfo::shapeFromWaveShape(1) == rtxx::lfo::Shape::WarpedMorphSine, "WaveShape 1");
    check(rtxx::lfo::shapeFromWaveShape(11) == rtxx::lfo::Shape::Stairs6Down, "WaveShape 11");
    // Las once formas son deterministas y están en [−1,1].
    for (int w = 1; w <= 11; ++w) {
        const auto s = rtxx::lfo::shapeFromWaveShape(w);
        for (float ph = 0.0f; ph < 1.0f; ph += 0.05f) {
            const float y = rtxx::lfo::evaluate(s, ph, 0.5f, 0.5f);
            check(y >= -1.0001f && y <= 1.0001f, "forma del LFO dentro de [−1,1]");
        }
    }
    check(rtxx::lfo::evaluate(rtxx::lfo::Shape::Triangle, 0.25f, 0.5f, 0.5f) > 0.99f,
          "triangular: +1 en 0.25");
    check(rtxx::lfo::evaluate(rtxx::lfo::Shape::Triangle, 0.75f, 0.5f, 0.5f) < -0.99f,
          "triangular: −1 en 0.75");
    check(rtxx::lfo::evaluate(rtxx::lfo::Shape::Square, 0.25f, 0.5f, 0.5f) > 0.99f,
          "cuadrada: +1 en 0.25");
    check(rtxx::lfo::evaluate(rtxx::lfo::Shape::Square, 0.75f, 0.5f, 0.5f) < -0.99f,
          "cuadrada: −1 en 0.75");

    // --- 6. cWobbler de punta a punta -------------------------------------------------
    {
        rtxx::ParamStore p;
        p.resetToDefaults();
        // Enable = 0 ⇒ passthrough bit a bit (antes de la rampa de 10 bloques).
        modelParams(p, 0.0f, 0.8f, 0.0f, 100.0f, 1.0f, 0.0f);
        WobblerBlock w(&p);
        const auto x = sine(0.2);
        const auto y = run(w, x);
        float dmax = 0.0f;
        for (size_t i = 0; i < x.size(); ++i) dmax = std::max(dmax, std::fabs(x[i] - y[i]));
        near(dmax, 0.0f, "Wobble_Enable = 0 ⇒ passthrough bit-exacto");

        // Rate = 0 deja la fase en su valor de reset (0.75 ⇒ L = −0.9988 ⇒ mod ≈ 0):
        // el retardo converge a 1 y un impulso sale en la muestra 1.
        modelParams(p, 1.0f, 0.8f, 0.0f, 100.0f, 0.0f, 0.0f);
        WobblerBlock wi(&p);
        const auto yi = run(wi, impulse(kFrames));
        const int peak = argmaxAbs(yi, 0, 64);
        check(peak == 1, "con Rate 0 y Depth 0.8 el impulso sale en la muestra 1");
        near(yi[static_cast<size_t>(peak)], 1.0f, "el impulso no se atenúa (k+c=1)", 1e-3f);
        // L en el mínimo = −0.9988062 (pico exacto del fallback) ⇒ mod = 0.8·0.000597 y el
        // retardo de régimen es 1 + 0.000477·1765 = 1.8428 (medido: 1.842795).
        near(wi.currentDelay(0), 1.0f + 0.8f * (1.0f - 0.9988062f) * 0.5f * 1765.0f,
             "retardo de régimen = 1 + Depth·(L+1)/2·D con L en el mínimo", 1e-4f);

// Depth = Depth2 = 0 ⇒ b = 0: el retardo converge a 0, no a 1.
        modelParams(p, 1.0f, 0.0f, 0.0f, 100.0f, 0.0f, 0.0f);
        WobblerBlock wz(&p);
        run(wz, impulse(kFrames));
        check(wz.currentDelay(0) < 0.01f, "Depth = Depth2 = 0 ⇒ el retardo va a 0 (b = 0)");

        // Con Rate 1 Hz el retardo barre [0, 1 + 0.8·D] y el mínimo se toca al principio.
        modelParams(p, 1.0f, 0.8f, 0.0f, 100.0f, 1.0f, 0.0f);
        WobblerBlock ws(&p);
        run(ws, sine(1.0, 0.1));
        const float dEnd = ws.currentDelay(0);
        check(dEnd > 0.0f && dEnd < 1.0f + 0.8f * 1765.0f,
              "el retardo se queda dentro de [0, 1 + 0.8·D]");
        check(ws.ringSize() == 2048, "el anillo es next_pow2(D) = 2048 @44.1 kHz");

        // Mix = 0 ⇒ passthrough bit a bit aunque el bloque esté activo.
        modelParams(p, 1.0f, 0.8f, 0.0f, 0.0f, 1.0f, 0.0f);
        WobblerBlock wm(&p);
        const auto ym = run(wm, x);
        dmax = 0.0f;
        for (size_t i = 0; i < x.size(); ++i) dmax = std::max(dmax, std::fabs(x[i] - ym[i]));
        near(dmax, 0.0f, "Wobble_Mix = 0 ⇒ passthrough bit-exacto");
    }

    // --- 7. cFlutter: leyes del AM y del desgaste -------------------------------------
    using rtxx::FlutterBlock;
    near(FlutterBlock::smoothstepForSteer(0.5f), 0.5f, "smoothstep((0.5+0.1)/1.2) = 0.5", 1e-5f);
    near(FlutterBlock::smoothstepForSteer(1.1f), 1.0f, "|sig| ≥ 1.1 ⇒ w = 1", 1e-5f);
    near(FlutterBlock::smoothstepForSteer(-0.5f), 0.5f, "usa |sig| (simétrico en signo)", 1e-5f);
    near(FlutterBlock::depthEffective(1.0f, 0.5f), 0.5f, "DepthEff = Depth·(1−w)", 1e-5f);
    near(FlutterBlock::depthEffective(1.0f, 2.0f), 0.0f, "w = 1 ⇒ DepthEff = 0", 1e-5f);
    near(FlutterBlock::compensation(0.0f), 1.0f, "comp(0) = 1");
    near(FlutterBlock::compensation(0.5f), 1.0f + 0.41253754f * 0.5f, "comp = 1 + 0.41253754·D");
    near(FlutterBlock::lfoGain(0.5f, 1.0f), 1.0f, "g_LFO(+1) = 1");
    near(FlutterBlock::lfoGain(0.5f, -1.0f), 0.5f, "g_LFO(−1) = 1 − DepthEff");
    for (float l : {-1.0f, -0.5f, 0.0f, 0.5f, 1.0f}) {
        const float g = FlutterBlock::lfoGain(0.4f, l);
        check(g >= 0.6f - 1e-6f && g <= 1.0f + 1e-6f, "g_LFO dentro de [1−DepthEff, 1]");
    }
    near(FlutterBlock::wearGain(0.0f, 2.0f), 1.0f, "sin desgaste la ganancia es 1");
    near(FlutterBlock::wearGain(1.0f, 2.0f), 0.031f, "el desgaste se recorta a 0.031");
    near(FlutterBlock::wearGain(0.5f, 2.0f), 0.5f, "desgaste lineal en WearDepth");
    near(FlutterBlock::wearGainLeft(0.5f, 2.0f, 0.0f), 0.5f, "pan 0 ⇒ izquierda = mono");
    near(FlutterBlock::wearGainRight(0.5f, 2.0f, 0.0f), 0.5f, "pan 0 ⇒ derecha = mono");
    check(FlutterBlock::wearGainLeft(0.5f, 2.0f, 1.0f) > FlutterBlock::wearGainRight(0.5f, 2.0f, 1.0f),
          "pan hacia la derecha deja más desgaste en la izquierda");
    near(FlutterBlock::dropoutGainLeft(0.0f, 0.5f, 0.1f, 0.9f, 0.0f), 1.0f,
         "sin dropouts la ganancia es 1");
    check(FlutterBlock::dropoutGainLeft(1.0f, 0.1f, 0.1f, 0.9f, 0.0f) < 0.01f,
          "dropout con la señal en el mínimo ⇒ ganancia ~0");
    near(FlutterBlock::dropoutGainLeft(1.0f, 0.9f, 0.1f, 0.9f, 0.0f), 1.0f,
         "dropout con la señal en el máximo ⇒ ganancia 1", 1e-6f);

    // --- 8. cFlutter de punta a punta: profundidad del AM ----------------------------
    {
        rtxx::ParamStore p;
        p.resetToDefaults();
        p.setModel("Magnetic_Enable", 1.0f);
        p.setModel("Magnetic_Depth", 1.0f);          // Xfade = 1 en el host reparte aquí
        p.setModel("Magnetic_WearDepth", 0.0f);
        p.setModel("Magnetic_DepthSteerSig", 0.5f);  // w = 0.5 ⇒ DepthEff = 0.5
        p.setModel("Magnetic_WearGain", 0.0f);
        p.setModel("Magnetic_Rate", 6.0f);
        p.setModel("Magnetic_WearPanEnable", 0.0f);
        p.setModel("Magnetic_DropOutsAmount", 0.0f);
        FlutterBlock f(&p);
        const auto x = sine(1.0, 0.1);
        const auto y = run(f, x);
        float lo = 0.0f, hi = 0.0f;
        envelope(y, 4096, lo, hi);
        // g_LFO ∈ [1−0.5, 1] y comp = 1.2063 en los dos: el cociente de la envolvente es
        // 1/(1 − DepthEff·0.9988062) = 1.9976 (el mínimo del fallback no llega a −1).
        near(hi / lo, 1.0f / (1.0f - 0.5f * 0.9988062f),
             "AM: envolvente máx/mín = 1/(1−DepthEff·|L|máx)", 0.02f);

        // Enable = 0 ⇒ passthrough bit a bit.
        p.setModel("Magnetic_Enable", 0.0f);
        FlutterBlock f0(&p);
        const auto y0 = run(f0, x);
        float dmax = 0.0f;
        for (size_t i = 0; i < x.size(); ++i) dmax = std::max(dmax, std::fabs(x[i] - y0[i]));
        near(dmax, 0.0f, "Magnetic_Enable = 0 ⇒ passthrough bit-exacto");

// Depth = WearDepth = 0 ⇒ bypass.
        p.setModel("Magnetic_Enable", 1.0f);
        p.setModel("Magnetic_Depth", 0.0f);
        FlutterBlock fz(&p);
        const auto yz = run(fz, x);
        dmax = 0.0f;
        for (size_t i = 0; i < x.size(); ++i) dmax = std::max(dmax, std::fabs(x[i] - yz[i]));
        near(dmax, 0.0f, "Depth = WearDepth = 0 ⇒ bypass bit-exacto");
    }

    if (failures == 0) std::printf("test_modulation: todas las comprobaciones pasan\n");
    else std::printf("test_modulation: %d fallos\n", failures);
    return failures == 0 ? 0 : 1;
}
