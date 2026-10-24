// Leyes de parámetro (fase 1 del ROADMAP). Cada ley está verificada por medida; el test
// fija la ecuación para que no derive.
#include <cmath>
#include <cstdio>
#include <cstring>

#include "blocks/StereoWidth.h"
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
        std::printf("FALLO: %s (%.7f != %.7f)\n", what, static_cast<double>(a), static_cast<double>(b));
        ++failures;
    }
}

} // namespace

int main() {
// 1. Estado de fábrica: el motor arranca con los módulos encendidos (BASELINE §5).
    rtxx::ParamStore p;
    p.resetToDefaults();
    near(p.get("Noise_Enable"), 1.0f, "default Noise_Enable = 1 (macro NOIS encendido)");
    near(p.get("Dist_Enable"), 1.0f, "default Dist_Enable = 1");
    near(p.get("Magnetic_Enable"), 1.0f, "default Magnetic_Enable = 1");
    near(p.get("InLevel"), 1.0f, "default InLevel = 1 (0 dB)");
    near(p.get("OutLevel"), 1.0f, "default OutLevel = 1 (0 dB)");
    near(p.get("Width_Width"), 100.0f, "default Width = 100 %");
    near(p.get("OutGainEnable"), 1.0f, "OutGainEnable siempre 1 (sin puerta de licencia)");

    // 2. Ley de ganancia: dB = (v - 0.5) * 36  →  lineal 10^(dB/20).
    for (float v : {0.0f, 0.25f, 0.5f, 0.75f, 1.0f}) {
        check(p.setHost(48, v), "setHost(48) aceptado");
        const float want = std::pow(10.0f, ((v - 0.5f) * 36.0f) / 20.0f);
        near(p.get("InLevel"), want, "InLevel = 10^(((v-0.5)*36)/20)", 1e-5f);
    }
    near(p.get("InLevel"), 7.943282f, "InLevel en v=1 es +18 dB", 1e-3f);
    p.setHost(48, 0.0f);
    near(p.get("InLevel"), 0.1258925f, "InLevel en v=0 es -18 dB", 1e-4f);
    p.setHost(48, 0.5f);
    near(p.get("InLevel"), 1.0f, "InLevel en v=0.5 es exactamente 1");

    check(p.setHost(57, 1.0f), "setHost(57) aceptado");
    near(p.get("OutLevel"), 7.943282f, "OutLevel usa la misma ley", 1e-3f);

// 3. Enables: el macro sincroniza los params del modelo que lista la tabla.
    p.setHost(7, 0.0f);
    near(p.get("Noise_Enable"), 0.0f, "MacroNoiseEnable apaga Noise_Enable");
    near(p.get("SComp_Enable"), 0.0f, "MacroNoiseEnable apaga SComp_Enable");
    p.setHost(7, 1.0f);
    near(p.get("Noise_Enable"), 1.0f, "MacroNoiseEnable enciende Noise_Enable");
    p.setHost(21, 0.0f);
    near(p.get("Dist_Enable"), 0.0f, "MacroDistEnable apaga Dist_Enable");
    p.setHost(49, 0.0f);
    near(p.get("EQ_Post_Enable"), 0.0f, "EQ_Post_Enable es directo");

    // 4. Width: el modelo guarda 0..200 (%) y el bloque usa /200.
    check(p.setHost(56, 0.25f), "setHost(56) aceptado");
    near(p.get("Width_Width"), 50.0f, "Width = v*200");
    check(p.setHost(56, 2.0f), "setHost recorta por arriba");
    near(p.get("Width_Width"), 200.0f, "Width recortado a 200");
    check(p.setHost(56, -1.0f), "setHost recorta por abajo");
    near(p.get("Width_Width"), 0.0f, "Width recortado a 0");

// 4b. Magnitude (host 0) escala los seis macros en LINEAL: ajustado por medida,
    //     `DIST Amount 0.25 con Magnitude 1` es bit a bit igual que `Amount 0.5 con
    //     Magnitude 0.5`. Aquí se comprueba en el modelo, que es de donde sale el audio.
    {
        rtxx::ParamStore m;
        m.setHost(21, 1.0f);
        check(m.setHost(0, 1.0f), "setHost(0) aceptado (tiene ley)");
        m.setHost(3, 0.25f);
        const float quarter = m.get("Dist_Amount");
        check(m.setHost(0, 0.5f), "setHost(0) a 0.5");
        m.setHost(3, 0.5f);
        near(m.get("Dist_Amount"), quarter, "Amount 0.5 con Magnitude 0.5 == Amount 0.25");
        check(m.lawStatus(0) == rtxx::LawStatus::Verified, "0 (Magnitude) tiene ley verificada");
        m.setHost(0, 0.0f);
        near(m.get("Dist_Amount"), 0.0f, "Magnitude 0 anula el macro");
        near(m.get("Noise_NoiseLevel"), -78.0f, "Magnitude 0 deja el ruido en -78 dB");
        m.setHost(0, 1.0f);
    }

    // 5. Cobertura: se declara lo que falta, no se esconde.
    const rtxx::ParamStore::Coverage cov = p.coverage();
    check(cov.total == 58, "58 parámetros de host");
    check(cov.verified >= 9, "al menos 9 leyes verificadas explícitas");
    check(cov.unverified > 0, "hay parámetros aún sin ley (se reportan)");
    check(p.lawStatus(48) == rtxx::LawStatus::Verified, "48 tiene ley verificada");
    check(p.lawStatus(56) == rtxx::LawStatus::Verified, "56 tiene ley verificada");
    check(p.lawStatus(34) == rtxx::LawStatus::Unverified, "34 (DIGI Flux) aún sin ley");
    check(!p.setHost(34, 0.5f), "setHost de una ley sin verificar devuelve false");
    near(p.hostValue(34), 0.5f, "aun así el valor del host se guarda");

    // 5b. DIST Mix (host 25) va a `Dist_MixInternal` (0..1), NO a `Dist_Mix` (0..100 del DSL):
    //     el grafo reparte el seco con (1-m) y la ENTRADA de la cadena húmeda con m. Medido
    // con la suite de medidas (caso Mix): coincide a -53 dB de error.
    {
        check(p.setHost(25, 0.0f), "setHost(25) aceptado");
        near(p.get("Dist_MixInternal"), 0.0f, "DIST Mix 0 => solo seco");
        check(p.setHost(25, 0.5f), "setHost(25) a 0,5");
        near(p.get("Dist_MixInternal"), 0.5f, "DIST Mix 0,5 => mitad");
        check(p.setHost(25, 1.0f), "setHost(25) a 1");
        near(p.get("Dist_MixInternal"), 1.0f, "DIST Mix 1 => solo húmedo");
        check(p.lawStatus(25) == rtxx::LawStatus::Verified, "25 (DIST Mix) tiene ley verificada");
    }

// 6. DIST Type (host 22) es un índice de opción sobre la tabla DistTypeSpec, no el
// valor del modelo. Los seis índices que el plugin expone, uno a uno.
    {
        const int expect[6] = {5, 7, 0, 1, 2, 6};
        for (int idx = 0; idx < 6; ++idx) {
            check(p.setHost(22, static_cast<float>(idx) / 5.0f), "setHost(22) aceptado");
            near(p.get("Dist_Type"), static_cast<float>(expect[idx]),
                 "DIST Type: índice de host → Dist_Type interno");
        }
        check(p.lawStatus(22) == rtxx::LawStatus::Verified, "22 tiene ley verificada");
        check(p.lawStatus(3) == rtxx::LawStatus::Verified, "3 (DIST Amount) tiene ley verificada");
        // El índice se redondea: el plugin recorre 6 posiciones enteras de 0 a 5.
        p.setHost(22, 0.0f);
        near(p.get("Dist_Type"), 5.0f, "índice 0 = TubePair (Dist_Type 5)");
        // el plugin redondea al índice: en su dominio (0..5) 0.4 sigue en 0 y 0.6 ya es 1.
        p.setHost(22, 0.08f);
        near(p.get("Dist_Type"), 5.0f, "0.08 (0.4) sigue siendo el índice 0");
        p.setHost(22, 0.12f);
        near(p.get("Dist_Type"), 7.0f, "0.12 (0.6) es el índice 1 (IronTransformer)");
        p.setHost(22, 1.0f);
        near(p.get("Dist_Type"), 6.0f, "índice 5 = AirPressure (Dist_Type 6)");
    }

// 7. DIST Amount (host 3) es el macro MacroDist: la tabla declara
    //    `Dist_Amount = { defaultValue = 1, macroControlName = "MacroDist" }` y
    // el grafo cablea `syncMul(Dist_Amount_Unscaled, MacroDistAmount,
    //    Dist_Amount)`. Con el factor fijo a 1, Dist_Amount = el valor del host.
    for (float v : {0.0f, 0.25f, 0.5f, 1.0f}) {
        check(p.setHost(3, v), "setHost(3) aceptado");
        near(p.get("Dist_Amount"), v, "Dist_Amount = MacroDistAmount (host 3)");
    }

    // 8. Ley del ancho (la que aplica el bloque): S y W con W*S = 1/4 al ensanchar.
    using rtxx::StereoWidthBlock;
    near(StereoWidthBlock::midFor(0.0f), 0.5f, "W(0) = 0.5");
    near(StereoWidthBlock::sideFor(0.0f), 0.0f, "S(0) = 0 (mono)");
    near(StereoWidthBlock::sideFor(0.25f), 0.25f, "S(0.25) = 0.25");
    near(StereoWidthBlock::midFor(0.5f), 0.5f, "W(0.5) = 0.5 (neutro)");
    near(StereoWidthBlock::sideFor(0.5f), 0.5f, "S(0.5) = 0.5 (neutro)");
    near(StereoWidthBlock::sideFor(0.75f), 0.70710678f, "S(0.75) = 0.5*sqrt(2)");
    near(StereoWidthBlock::sideFor(1.0f), 1.0f, "S(1) = 1");
    near(StereoWidthBlock::midFor(1.0f), 0.25f, "W(1) = 0.25");
    for (float v : {0.55f, 0.7f, 0.9f}) {
        near(StereoWidthBlock::midFor(v) * StereoWidthBlock::sideFor(v), 0.25f, "W*S = 1/4 al ensanchar", 1e-6f);
    }

    // 9. Modulación (fase 4): leyes host→modelo que el DSP no ve.
    // Velocidades del LFO con `fit3` (sin `type` y con
    //    `fitMiddle`): y(0) = min, y(0.5) = middle, y(1) = max.
    {
        const float min16 = 0.1f, mid16 = 1.0f, max16 = 4.0f;
        p.setHost(16, 0.0f);
        near(p.get("Wobble_Rate"), min16, "fit3: Wobble_Rate(0) = 0.1 Hz");
        p.setHost(16, 0.5f);
        near(p.get("Wobble_Rate"), mid16, "fit3: Wobble_Rate(0.5) = 1 Hz");
        p.setHost(16, 1.0f);
        near(p.get("Wobble_Rate"), max16, "fit3: Wobble_Rate(1) = 4 Hz");
        p.setHost(16, 0.25f);
        // c = (1−0.1)/3.9 = 0.230769 y rsmul(0.25) = 0.090909 ⇒ 0.454545 Hz.
        near(p.get("Wobble_Rate"), 0.4545454f, "fit3: Wobble_Rate(0.25) = 0.4545 Hz", 1e-5f);
        check(p.lawStatus(16) == rtxx::LawStatus::Verified, "16 tiene ley verificada (fit3)");
        p.setHost(18, 0.0f);
        near(p.get("Wobble_Rate2"), 6.0f, "fit3: Wobble_Rate2(0) = 6 Hz");
        p.setHost(18, 0.5f);
        near(p.get("Wobble_Rate2"), 10.0f, "fit3: Wobble_Rate2(0.5) = 10 Hz");
        p.setHost(18, 1.0f);
        near(p.get("Wobble_Rate2"), 20.0f, "fit3: Wobble_Rate2(1) = 20 Hz");
        p.setHost(45, 0.0f);
        near(p.get("Magnetic_Rate"), 6.0f, "fit3: Magnetic_Rate(0) = 6 Hz");
        p.setHost(45, 0.5f);
        near(p.get("Magnetic_Rate"), 20.0f, "fit3: Magnetic_Rate(0.5) = 20 Hz");
        p.setHost(45, 1.0f);
        near(p.get("Magnetic_Rate"), 30.0f, "fit3: Magnetic_Rate(1) = 30 Hz");

        // Porcentajes: el modelo guarda 0..100 y el bloque divide por 100.
        p.setHost(19, 1.0f);
        near(p.get("Wobble_Mix"), 100.0f, "Wobble_Mix = v·100 (host 19)");
        p.setHost(19, 0.3f);
        near(p.get("Wobble_Mix"), 30.0f, "Wobble_Mix = v·100", 1e-4f);

// Cruce triangular Wobble_Xfade → Depth / Depth2 y macro de módulo.
        p.setHost(2, 1.0f);   // MacroWobbleAmount
        p.setHost(15, 0.0f);
        near(p.get("Wobble_Depth"), 0.8f, "Xfade 0 ⇒ Depth = 0.8 (sólo wow)");
        near(p.get("Wobble_Depth2"), 0.0f, "Xfade 0 ⇒ Depth2 = 0");
        p.setHost(15, 0.5f);
        near(p.get("Wobble_Depth"), 0.8f, "Xfade 0.5 ⇒ Depth = 0.8 (los dos al máximo)");
        near(p.get("Wobble_Depth2"), 0.2f, "Xfade 0.5 ⇒ Depth2 = 0.2");
        p.setHost(15, 1.0f);
        near(p.get("Wobble_Depth"), 0.0f, "Xfade 1 ⇒ Depth = 0 (sólo flutter)");
        near(p.get("Wobble_Depth2"), 0.2f, "Xfade 1 ⇒ Depth2 = 0.2");
        p.setHost(2, 0.0f);
        near(p.get("Wobble_Depth"), 0.0f, "MacroWobbleAmount = 0 ⇒ Depth = 0");
        near(p.get("Wobble_Depth2"), 0.0f, "MacroWobbleAmount = 0 ⇒ Depth2 = 0");
        p.setHost(2, 1.0f);
        p.setHost(15, 0.25f);
        near(p.get("Wobble_Depth2"), 0.1f, "Xfade 0.25 ⇒ Depth2 = 0.2·min(1,2x) = 0.1", 1e-6f);

        // Desfase estéreo = 0.4 ciclos × Wobble_Stereo_Enable (audioSyncFromTo).
        p.setHost(17, 1.0f);
        near(p.get("Wobble_StereoPhase"), 0.4f, "Stereo 1 ⇒ StereoPhase = 0.4");
        p.setHost(17, 0.5f);
        near(p.get("Wobble_StereoPhase"), 0.2f, "Stereo 0.5 ⇒ StereoPhase = 0.2", 1e-6f);

        // Magnetic_Xfade reparte desgaste (A) y AM del LFO (B); el macro escala los dos.
        p.setHost(6, 1.0f);   // MacroMagneticAmount
        p.setHost(43, 0.0f);
        near(p.get("Magnetic_WearDepth"), 1.0f, "Xfade 0 ⇒ WearDepth = 1 (sólo desgaste)");
        near(p.get("Magnetic_Depth"), 0.0f, "Xfade 0 ⇒ Depth = 0");
        p.setHost(43, 1.0f);
        near(p.get("Magnetic_WearDepth"), 0.0f, "Xfade 1 ⇒ WearDepth = 0");
        near(p.get("Magnetic_Depth"), 1.0f, "Xfade 1 ⇒ Depth = 1 (sólo AM)");
        p.setHost(6, 0.5f);
        near(p.get("Magnetic_Depth"), 0.5f, "el macro de módulo escala el Depth");

// Dropouts: Mult1 = rsmul(macro, 0.98), Amount = GUI·Mult1².
        p.setHost(6, 1.0f);
        p.setHost(46, 1.0f);
        const float mult1 = p.get("Magnetic_DropOutsAmountMult1");
        near(mult1, 1.0f, "Mult1(1) = 1", 1e-6f);
        near(p.get("Magnetic_DropOutsAmount"), mult1 * mult1,
             "DropOutsAmount = GUI·Mult1²", 1e-6f);
        near(p.get("Magnetic_DropOutsMin"), 0.1f + 0.58f, "DropOutsMin = 0.1 + 0.58·Amount", 1e-6f);
        near(p.get("Magnetic_DropOutsMax"), 0.25f + 0.63f, "DropOutsMax = 0.25 + 0.63·Amount",
             1e-6f);
        p.setHost(46, 0.0f);
        near(p.get("Magnetic_DropOutsAmount"), 0.0f, "GUI 0 ⇒ sin dropouts");
    }

    if (failures == 0) std::printf("test_param_laws: OK\n");
    return failures == 0 ? 0 : 1;
}
