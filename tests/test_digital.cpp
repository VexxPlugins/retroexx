// Digital (fase 7 del ROADMAP): cBitColor — cuantización de bits y sample-and-hold.
//
// Fija las ecuaciones cerradas de src/blocks/BitColor.cpp para que no deriven.
//
// La ley de POSICIÓN de los filtros (Digital_FilterLo/Hi, Digital_Focus) NO se fija aquí
// como verificada: con FilterLo = 0.5 el corte real no es el que predice `log 20..20000` y
// además se mueve con el estado. El bloque usa el spec como mapeo provisional y ese punto
// queda declarado como abierto [?]; estos tests sólo comprueban la aritmética de los
// coeficientes.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#include "blocks/BitColor.h"
#include "model/ParamStore.h"

namespace {

int failures = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FALLO: %s\n", what);
        ++failures;
    }
}
void near(float a, float b, const char* what, float tol = 1e-6f) {
    if (std::fabs(a - b) > tol) {
        std::printf("FALLO: %s (%.9f != %.9f)\n", what, static_cast<double>(a),
                    static_cast<double>(b));
        ++failures;
    }
}

constexpr int kRate = 44100;

std::vector<float> sine(double seconds, double amp = 0.5, double freq = 1000.0) {
    const int n = static_cast<int>(seconds * kRate);
    std::vector<float> x(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i)
        x[static_cast<size_t>(i)] =
            static_cast<float>(amp * std::sin(2.0 * M_PI * freq * i / kRate));
    return x;
}

// Procesa en lotes de quantum por el bloque y devuelve el canal 0.
std::vector<float> run(rtxx::iBlock& block, const std::vector<float>& in) {
    rtxx::PrepareContext ctx;
    ctx.sampleRate = kRate;
    block.prepare(ctx);
    std::vector<float> out(in.size(), 0.0f);
    for (int start = 0; start < static_cast<int>(in.size()); start += rtxx::kQuantum) {
        const int n = std::min(rtxx::kQuantum, static_cast<int>(in.size()) - start);
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

// Amplitud de una componente concreta (Goertzel sobre un número ENTERO de ciclos: sin fuga).
double component(const std::vector<float>& x, double freq, int from, int frames) {
    const double w = 2.0 * M_PI * freq / kRate;
    double re = 0.0, im = 0.0;
    for (int i = 0; i < frames; ++i) {
        const double s = x[static_cast<size_t>(from + i)];
        re += s * std::cos(w * i);
        im -= s * std::sin(w * i);
    }
    return 2.0 * std::sqrt(re * re + im * im) / frames;
}

// Residuo (dB re la señal) que queda tras descontar una sinusoide ajustada en amplitud y
// fase: mide cuantización y distorsión sin que el desfase del antialias cuente como error.
double sineResidualDb(const std::vector<float>& x, double freq, int from, int frames) {
    const double w = 2.0 * M_PI * freq / kRate;
    double ss = 0.0, sc = 0.0, sig = 0.0;
    for (int i = 0; i < frames; ++i) {
        const double s = std::sin(w * i), c = std::cos(w * i);
        const double v = x[static_cast<size_t>(from + i)];
        ss += v * s;
        sc += v * c;
        sig += v * v;
    }
    const double a = 2.0 * ss / frames, b = 2.0 * sc / frames;
    double res = 0.0;
    for (int i = 0; i < frames; ++i) {
        const double model = a * std::sin(w * i) + b * std::cos(w * i);
        const double v = x[static_cast<size_t>(from + i)] - model;
        res += v * v;
    }
    return 10.0 * std::log10(std::max(res, 1e-30) / std::max(sig, 1e-30));
}

// Store con el módulo Digital encendido y sin cantidades: DegradeSR = 44.1 kHz y 24 bits
rtxx::ParamStore makeStore() {
    rtxx::ParamStore p;
    p.resetToDefaults();    // estado de fábrica (filtros abiertos, sin cantidad)
    p.setHost(27, 1.0f);   // DIGI Enable
    p.setHost(4, 0.0f);    // DIGI Amount = 0 ⇒ sin degradación
    p.setHost(28, 0.5f);   // balance al centro
    return p;
}

// Leyes de cuantización ------------------------------

void testQuantLaws() {
    // Sesgo: 0.5 − 0.499·RoundError, con RoundError ∈ [−1, 1].
    near(rtxx::BitColorBlock::biasMagnitude(-1.0f), 0.999f, "sesgo con RoundError = −1");
    near(rtxx::BitColorBlock::biasMagnitude(0.0f), 0.5f, "sesgo con RoundError = 0");
    near(rtxx::BitColorBlock::biasMagnitude(1.0f), 0.001f, "sesgo con RoundError = +1");
    // El default del parámetro es −0.98 ⇒ casi medio paso a la redonda.
    near(rtxx::BitColorBlock::biasMagnitude(-0.98f), 0.98902f, "sesgo por defecto (−0,98)", 1e-5f);

// Escala: 2^(bits−1). El exponente efectivo es (bits−1) y el 3.º armónico lo confirma
    near(rtxx::BitColorBlock::quantScale(1.0f), 1.0f, "escala a 1 bit");
    near(rtxx::BitColorBlock::quantScale(2.0f), 2.0f, "escala a 2 bits");
    near(rtxx::BitColorBlock::quantScale(6.0f), 32.0f, "escala a 6 bits");
    near(rtxx::BitColorBlock::quantScale(24.0f), 8388608.0f, "escala a 24 bits");

    // Ganancia de compensación: 1 − 0.8·(1 − (1 − 2^(−0.57·(bits−1)))^1.5), medida contra
    // el pico por medida (0,200 a 1 bit y 0,840 a 6 bits).
    near(rtxx::BitColorBlock::quantGain(1.0f), 0.2f, "ganancia a 1 bit");
    near(rtxx::BitColorBlock::quantGain(6.0f), 0.83948f, "ganancia a 6 bits", 1e-5f);
    // A 24 bits tiende a 1 sin llegar (2^−0,57·23 no es 0): 0,999864.
    check(rtxx::BitColorBlock::quantGain(24.0f) > 0.9998f, "ganancia a 24 bits ≈ 1");
    check(rtxx::BitColorBlock::quantGain(3.0f) > rtxx::BitColorBlock::quantGain(1.0f),
          "la ganancia crece con los bits");

    // Cuantizador: trunc(x·escala + signo(x)·sesgo) / escala, es decir medio paso hacia
    // fuera del cero con sesgo 0.5 y truncamiento puro con sesgo 0.
    near(rtxx::BitColorBlock::quantize(0.6f, 4.0f, 0.5f), 0.5f, "cuantiza 0,6 → 0,5");
    near(rtxx::BitColorBlock::quantize(0.6f, 4.0f, 0.0f), 0.5f, "trunca 0,6 → 0,5");
    near(rtxx::BitColorBlock::quantize(-0.6f, 4.0f, 0.0f), -0.5f, "trunca −0,6 → −0,5");
    near(rtxx::BitColorBlock::quantize(-0.375f, 4.0f, 0.0f), -0.25f,
         "truncar −1,5 → −1 escalón (hacia cero)");
    near(rtxx::BitColorBlock::quantize(-0.375f, 4.0f, 0.5f), -0.5f,
         "con sesgo −1,5 → −2 escalones (fuera del cero)");
}

// Sample-and-hold -----------------------------------

void testHoldFactor() {
    // depth = 0: lo = hi ⇒ factor = (degSR·32/sr)^4 · 4 = degSR·32/sr, recortado a 8.
    near(rtxx::BitColorBlock::holdFactor(200.0f, 0.0f, 44100.0, 0.5f), 200.0f * 32.0f / 44100.0f,
         "factor con degSR = 200", 1e-6f);
    near(rtxx::BitColorBlock::holdFactor(1411.2f, 0.0f, 44100.0, 0.0f), 1411.2f * 32.0f / 44100.0f,
         "factor con degSR = 1411,2", 1e-6f);
    // El recorte superior es 8: con degSR = 44,1 kHz el factor sería 32.
    near(rtxx::BitColorBlock::holdFactor(44100.0f, 0.0f, 44100.0, 0.5f), 8.0f, "recorte a 8");
    // depth = 1 abre la ventana [0, 2·degSR·32/sr] según el LFO (0 → 0, 1 → doble).
    near(rtxx::BitColorBlock::holdFactor(1000.0f, 1.0f, 44100.0, 0.0f), 0.0f,
         "depth 1 y LFO al mínimo ⇒ factor 0");
    near(rtxx::BitColorBlock::holdFactor(1000.0f, 1.0f, 44100.0, 1.0f),
         2.0f * 1000.0f * 32.0f / 44100.0f, "depth 1 y LFO al máximo ⇒ doble", 1e-6f);
    // Independencia del sample rate a tasa efectiva fija: el exponente 1/4 y la potencia 4
    // se cancelan, así que el factor escala 1/sr.
    const float a = rtxx::BitColorBlock::holdFactor(300.0f, 0.0f, 44100.0, 0.5f);
    const float b = rtxx::BitColorBlock::holdFactor(300.0f, 0.0f, 48000.0, 0.5f);
    near(a / b, 48000.0f / 44100.0f, "factor ∝ 1/sr", 1e-5f);
}

// Antialias y filtros -------------------------------

void testAliasingLaws() {
    // Peso del antialias: A = AliasSmoothing·(f·0.8)/(f·0.6+0.2), f = clamp((degSR−20)/170).
    near(rtxx::BitColorBlock::aliasWeight(20.0f, 1.0f), 0.0f, "peso con degSR = 20 Hz");
    near(rtxx::BitColorBlock::aliasWeight(190.0f, 1.0f), 1.0f, "peso saturado en degSR = 190");
    near(rtxx::BitColorBlock::aliasWeight(105.0f, 1.0f), 0.8f, "peso con f = 0,5");
    near(rtxx::BitColorBlock::aliasWeight(105.0f, 0.5f), 0.4f, "el peso es lineal en Smooth");
    near(rtxx::BitColorBlock::aliasWeight(10.0f, 1.0f), 0.0f, "por debajo de 20 Hz no hay peso");

    // AA2: limitador de slew con paso 1/(k²·0.008·sr); con bits ≥ 4 la apertura u = 1 y no
    // limita (paso 10000). Con 2 bits, u = 1/3.
    near(rtxx::BitColorBlock::aa2MaxStep(0.5f, 1.0f, 24.0f, 44100.0), 10000.0f,
         "AA2 sin límite con bits ≥ 4");
    const float k = 0.5f * 1.0f * (2.0f / 3.0f);
    near(rtxx::BitColorBlock::aa2MaxStep(0.5f, 1.0f, 2.0f, 44100.0),
         1.0f / (k * k * 0.008f * 44100.0f), "AA2 con 2 bits", 1e-4f);

    // AA3: one-pole con esquina ((w·0.04)/(1 − 0.92w − 0.04))·19800 + 200; con bits ≥ 10 la
    // apertura v = 1 deja w = 1 ⇒ esquina 20000 Hz. Ojo: AA3 = 0 NO cierra el filtro, lo
    // deja en su esquina máxima (el control baja la esquina hacia 200 Hz, no al revés).
    const float c20k = std::exp(-2.0f * static_cast<float>(M_PI) * 20000.0f / 44100.0f);
    near(rtxx::BitColorBlock::aa3Coef(0.65f, 1.0f, 24.0f, 44100.0), 1.0f - c20k,
         "AA3 abierto a 24 bits", 1e-6f);
    near(rtxx::BitColorBlock::aa3Coef(0.0f, 1.0f, 24.0f, 44100.0), 1.0f - c20k,
         "AA3 = 0 con bits altos sigue en 20 kHz", 1e-6f);
    // Con bits bajos y AA3 alto la esquina baja: w = 1 − 0.65·1·u, u = (2−1)/3 = 1/3.
    {
        const float w = 1.0f - 0.65f * (1.0f / 3.0f);
        const float corner = ((w * 0.04f) / (1.0f - 0.92f * w - 0.04f)) * 19800.0f + 200.0f;
        const float c = std::exp(-2.0f * static_cast<float>(M_PI) * corner / 44100.0f);
        near(rtxx::BitColorBlock::aa3Coef(0.65f, 1.0f, 2.0f, 44100.0), 1.0f - c,
             "AA3 con 2 bits baja la esquina", 1e-6f);
        check(corner < 20000.0f, "la esquina de AA3 con 2 bits es menor que 20 kHz");
    }

    // Coeficientes de los dos filtros: c = e^(−2π·f/sr), g = 1 − c, tpt = (c+1)/2.
    float c = 0.0f, g = 0.0f, tpt = 0.0f;
    rtxx::BitColorBlock::filterCoefs(20.0f, 44100.0, c, g, tpt);
    near(c, std::exp(-2.0f * static_cast<float>(M_PI) * 20.0f / 44100.0f), "c a 20 Hz", 1e-7f);
    near(g, 1.0f - c, "g = 1 − c");
    near(tpt, (c + 1.0f) * 0.5f, "tpt = (c + 1)/2");

    // Cruce interno de los filtros: 0 con el bloque sin degradar, 1 con la degradación
    // máxima, y es el mínimo de los dos triángulos.
    near(rtxx::BitColorBlock::internalCrossfade(44100.0f, 24.0f), 0.0f, "cruce abierto = 0");
    near(rtxx::BitColorBlock::internalCrossfade(44.1f, 1.0f), 1.0f, "cruce máximo = 1");
    near(rtxx::BitColorBlock::internalCrossfade(200.0f, 6.0f), 0.565217f, "cruce por el lado de bits",
         1e-4f);
    // Sólo el lado más profundo manda: bajar la tasa no cambia el cruce si los bits ya
    // están más degradados.
    near(rtxx::BitColorBlock::internalCrossfade(200.0f, 6.0f),
         rtxx::BitColorBlock::internalCrossfade(4000.0f, 6.0f), "min de los dos triángulos");
         // Posición normalizada de un parámetro con min > max:
    // vale 1 en el MÍNIMO y 0 en el máximo, porque el recorrido va del máximo al mínimo.
    near(rtxx::BitColorBlock::normDescending(44100.0f, 44100.0f, 44.1f), 1.0f, "norm en el mínimo");
    near(rtxx::BitColorBlock::normDescending(44.1f, 44100.0f, 44.1f), 0.0f, "norm en el máximo");
    near(rtxx::BitColorBlock::normDescending(0.0f, 20.0f, 10.0f), -1.0f,
         "por debajo del rango la normalizada es negativa");
    near(rtxx::BitColorBlock::normDescending(25.0f, 20.0f, 10.0f), 1.5f,
         "por encima del rango extrapola (no recorta)");
}

// Comportamiento del bloque -------------------------------------------------------

void testBypassAndMix() {
    // 0,4 s: el suavizado de bits a 320 Hz parte de 0 y tarda ~0,1 s en asentarse.
    const std::vector<float> in = sine(0.4);

    // Enable = 0: el nodo devuelve la entrada sin tocar un bit.
    {
        rtxx::ParamStore p = makeStore();
        p.setHost(27, 0.0f);
        rtxx::BitColorBlock blk(&p);
        const std::vector<float> out = run(blk, in);
        bool same = true;
        for (size_t i = 0; i < in.size(); ++i) same = same && (out[i] == in[i]);
        check(same, "Enable = 0 ⇒ passthrough bit a bit");
    }
    // Mix = 0 con el bloque encendido y degradando: la salida vuelve a ser la entrada
    // (la mezcla seca/húmeda es lo último que se aplica).
    {
        rtxx::ParamStore p = makeStore();
        p.setHost(4, 0.8f);    // degradación fuerte
        p.setHost(28, 1.0f);   // todo al lado de bits
        p.setHost(33, 0.0f);   // DIGI Mix = 0
        rtxx::BitColorBlock blk(&p);
        const std::vector<float> out = run(blk, in);
        bool same = true;
        for (size_t i = 0; i < in.size(); ++i) same = same && (out[i] == in[i]);
        check(same, "Mix = 0 ⇒ seco bit a bit aunque el bloque procese");
    }
    // Sin degradación (DegradeSR = 44,1 kHz y 24 bits) el bloque debe ser casi transparente:
    // sólo quedan el AA3 en 20 kHz y el cuantizador de 24 bits. El AA3 mete un desfase de
    // ~0,05 rad a 1 kHz, así que comparar muestra a muestra daría 37 dB sin que haya nada
    // malo: se mide el residuo tras quitar la fundamental ajustada en amplitud y fase.
    {
        rtxx::ParamStore p = makeStore();
        rtxx::BitColorBlock blk(&p);
        const std::vector<float> out = run(blk, in);
        const double res = sineResidualDb(out, 1000.0, 8820, 4410);
        std::printf("   residuo sin degradación (1 kHz descontado): %.1f dB\n", res);
        check(res < -60.0, "sin degradación el residuo queda por debajo de −60 dB");
        // El mismo medidor aplicado a 1 bit tiene que ver la cuantización.
        rtxx::ParamStore q = makeStore();
        q.setHost(4, 1.0f);
        q.setHost(28, 1.0f);
        rtxx::BitColorBlock blk2(&q);
        const double res1 = sineResidualDb(run(blk2, in), 1000.0, 8820, 4410);
        std::printf("   residuo con 1 bit: %.1f dB\n", res1);
        check(res1 > -20.0, "con 1 bit el residuo NO baja de −20 dB (el medidor discrimina)");
    }
}

// Cuantización audible: con 1 bit el seno se convierte en una escalera y el 3.º armónico
// sube a ≈ −9,6 dB re la fundamental.
void testQuantizationSpectrum() {
    const std::vector<float> in = sine(1.5);
    const int from = 44100;      // 1 s: el suavizado de bits (320 Hz) ya convergió
    const int frames = 44100 / 10;   // 100 ms = 100 ciclos exactos de 1 kHz

    auto thirdHarmonic = [&](float macro) {
        rtxx::ParamStore p = makeStore();
        p.setHost(4, macro);
        p.setHost(28, 1.0f);   // balance a bits: no hay reducción de tasa
        p.setHost(33, 1.0f);
        rtxx::BitColorBlock blk(&p);
        const std::vector<float> out = run(blk, in);
        const double f0 = component(out, 1000.0, from, frames);
        const double f3 = component(out, 3000.0, from, frames);
        return 20.0 * std::log10(std::max(f3, 1e-12) / std::max(f0, 1e-12));
    };

    const double h3_1bit = thirdHarmonic(1.0f);
    const double h3_24bit = thirdHarmonic(0.0f);
    std::printf("   3.º armónico: 1 bit %+.1f dB, sin degradación %+.1f dB (re fundamental)\n",
                h3_1bit, h3_24bit);
    check(h3_1bit > -14.0 && h3_1bit < -6.0,
          "1 bit deja el 3.º armónico en el entorno de −9,6 dB (medido: −9,6)");
    check(h3_24bit < -60.0, "sin degradación el 3.º armónico queda por debajo de −60 dB");
}

// Retención: con el balance al lado R el bloque mantiene la muestra y la retención deja
// imágenes en k·fs_eff ± f0 con fs_eff = 32·DegradeSR. Aquí se comprueba en el bloque
// suelto.
void testHoldImages() {
    const std::vector<float> in = sine(1.5);
    const int from = 44100;          // 1 s: suavizados asentados
    const int frames = 44100 / 10;   // 100 ms = ciclos enteros de 1 kHz, 5,4 kHz y 7,4 kHz

    auto imageLevel = [&](float macro) {
        rtxx::ParamStore p = makeStore();
        p.setHost(4, macro);
        p.setHost(28, 0.0f);   // balance al lado R: sólo reducción de tasa
        p.setHost(33, 1.0f);
        rtxx::BitColorBlock blk(&p);
        const std::vector<float> out = run(blk, in);
        const double f0 = component(out, 1000.0, from, frames);
        const double up = component(out, 7400.0, from, frames);
        return 20.0 * std::log10(std::max(up, 1e-12) / std::max(f0, 1e-12));
    };

    const double img = imageLevel(0.5f);    // DegradeSR = 200 Hz ⇒ fs_eff = 6400 Hz
    const double clean = imageLevel(0.0f);  // sin reducción de tasa
    std::printf("   imagen de 7,4 kHz con retención: %+.1f dB (sin retención %+.1f dB)\n", img,
                clean);
    check(img > -40.0, "con retención aparece la imagen en 32·DegradeSR + f0");
    check(clean < -60.0, "sin retención no hay imagen");
}

} // namespace

int main() {
    testQuantLaws();
    testHoldFactor();
    testAliasingLaws();
    testBypassAndMix();
    testQuantizationSpectrum();
    testHoldImages();
    if (failures == 0) {
        std::printf("PASS test_digital\n");
        return 0;
    }
    std::printf("FAIL test_digital (%d comprobaciones)\n", failures);
    return 1;
}
