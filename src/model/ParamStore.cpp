#include "model/ParamStore.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "model/ParamSpec.h"

namespace rtxx {
namespace {

// Leyes de conversión host→modelo que NO son la identidad:
//   GainDb36  dB = (v - 0.5) * 36   → ganancia lineal 10^(dB/20).
// Verificada en 5 puntos del recorrido, error < 0.001 dB.
//   Scale200  Width = v * 200 (%)  → el bloque cStereoWidth usa v = Width/200.
// Rango del control `Width_Width` (min 0, max 200).
//   KnobAnyGain12  EQ_Post_Tone_Amount = 10^(dB/20) con la escala `knobToAnyGain`
//             (curve 0.2, min/max del spec invertidos: mindB=+12, maxdB=-12).
// Ley:
//               v >= 0.5: dB = 20*log10(max) * rsmul(2v-1, c)
//               v <  0.5: dB = 20*log10(min) * (1 - rsmul(2v, 1-c)),  rsmul(x,c) = c*x/(((2c-1)*x)+1-c)
// Con la cadena Tone aislada, el nivel plano de salida sigue esta ley dentro
// de 0.01 dB en 22 puntos de v (0/0.15/0.5/0.85/1 → +12/…/0/…/-12).
//             Es la ganancia de compensación de la arista EQ_Post_Tone.out → Magnetic.in
// ("EQ_Post_Tone_Amount").
// Fit3 y = min + (max-min)*rsmul(v, c), c = (middle-min)/(max-min). Es la ley de los tres
// controles de// velocidad del LFO: Wobble_Rate (0.1/1/4 Hz), Wobble_Rate2 (6/10/20) y
// Magnetic_Rate (6/20/30). Verificada midiendo la frecuencia real del LFO.
//   Scale100  el modelo guarda 0..100 (%) y el bloque usa /100.
enum class LawKind { GainDb36, Scale200, FreqLog, FreqLogRange, KnobAnyGain12, DistTypeIndex, MacroAmountCopy, Fit3, Scale100, MacroFactor, NoiseLevel, ToneBipolar12, IndexRange, FilterLoMacro, MagnitudeGlobal };

struct LawEntry {
    int hostId;
    const char* modelId;   // nullptr: la ley sólo alimenta parámetros derivados (MacroFactor)
    LawKind kind;
    // Sólo Fit3: min, middle y max del control.
    float p0 = 0.0f, p1 = 0.0f, p2 = 0.0f;
};

// DIST Type (host 22) es un ÍNDICE de opción, no el valor del modelo: el parámetro
// declara un rango de 0..5 con seis opciones en este orden, y cada una lleva su valor
// interno de `Dist_Type`:
//   0 TubePair → 5 | 1 IronTransformer → 7 | 2 Crunch → 0
//   3 Heavy   → 1 | 4 Zap → 2             | 5 AirPressure → 6
// Ajustado por medida (21 = 1 Dist, resto de macros a 0, seno 1 kHz −20 dBFS, host 3 = 1):
// `auhost` con 22 = 0..5 da seis respuestas distintas (THD −14.5 / −33.3 / −1.6 / +13.5 /
// −34.2 / −31.2 dB) y valores intermedios del mismo índice (0.2/0.4 = 0.0) ⇒ el parámetro
// se redondea al índice. `Dist_Type` interno 3, 4, 8 y 9 no son alcanzables desde el host.
const int kDistTypeByIndex[] = {5, 7, 0, 1, 2, 6};

const LawEntry kLaws[] = {
    {48, "InLevel", LawKind::GainDb36},    // MAST In Gain
    {57, "OutLevel", LawKind::GainDb36},   // MAST Out Gain
    {56, "Width_Width", LawKind::Scale200}, // MAST Out Width
    // Frecuencias de corte: ley "log" (min 20, max 20000) => 20*1000^v.
    // El modelo guarda Hz; el DSL de plugchain las usa directamente (ver PlugChainBlock).
    {50, "EQ_Post_CutLoFreq", LawKind::FreqLog},
    {51, "EQ_Post_CutHiFreq", LawKind::FreqLog},
    // MAST EQ Tone: ganancia de compensación de la cadena Tone (ver KnobAnyGain12 arriba).
    {54, "EQ_Post_Tone_Amount", LawKind::KnobAnyGain12},
    // DIST Type: índice de opción → valor interno (tabla de arriba).
    {22, "Dist_Type", LawKind::DistTypeIndex},
    // DIST Amount: MacroDistAmount → Dist_Amount, con
    // `Dist_Amount = { defaultValue = 1, macroControlName = "MacroDist" }` cableado como
    // `syncMul("Dist_Amount_Unscaled", "MacroDistAmount", "Dist_Amount")`.
    // El factor `Dist_Amount_Unscaled` no existe en el modelo (vale 1.0 de fábrica y ningún
    // knob del host lo mueve) ⇒ Dist_Amount = MacroDistAmount = host 3.
    // [?] Magnitude (host 0, `MasterMacroScaler`) multiplica a MacroDistAmount dentro del
    // macro; nuestra ley no lo aplica porque Magnitude sigue sin ley propia.
    {3, "Dist_Amount", LawKind::MacroAmountCopy},
    // Macros de módulo que no tienen parámetro de modelo propio: su ley es el factor que
    // ParamStore::syncDerived aplica a los params derivados (Depth/Depth2 de Wobble y
    // WearDepth/Depth/DropOutsAmount de Magnetic). El producto es
    // MacroXAmount_Scaled = MacroXAmount · MasterMacroScaler.
    // [?] El factor Magnitude (host 0) NO se aplica porque host 0 sigue sin ley propia:
    // residual declarado. Con Magnitude al 100 % —el estado de fábrica— el factor es 1.
    // El factor del macro de módulo (host 2/6) sí se aplica y está verificado: con WOBB
    // Amount a 0 no hay modulación; con 0.5 la excursión se reduce a la mitad.
    {2, nullptr, LawKind::MacroFactor},
    {6, nullptr, LawKind::MacroFactor},
    // Velocidades del LFO: `fit3`.
    {16, "Wobble_Rate", LawKind::Fit3, 0.1f, 1.0f, 4.0f},
    {18, "Wobble_Rate2", LawKind::Fit3, 6.0f, 10.0f, 20.0f},
    {45, "Magnetic_Rate", LawKind::Fit3, 6.0f, 20.0f, 30.0f},
    // Mezclas y controles en % (el modelo guarda 0..100, el bloque usa /100).
    {19, "Wobble_Mix", LawKind::Scale100},
    {46, "Magnetic_DropOutsAmount_GUI", LawKind::Scale100},
    // Digital: los dos filtros son `log` 20..20000 Hz (misma clase de
    // spec que los cortes del EQ, verificados en la fase 2) y la GUI los pinta con
    // `displayScale = {20, 20000, "log"}`. OJO: esto es la ley host→modelo;
    // lo que el BLOQUE hace después con ese valor no está verificado y queda declarado como
    // provisional en src/blocks/BitColor.h (al mover FilterLo a 0,5 el corte real es
    // ~145 Hz, no 632). AliasSmoothing/Mix son porcentajes
    // MAGNITUDE (host 0): no escribe ningún parámetro del modelo, es el ESCALADOR GLOBAL de
    // los seis macros de módulo. Verificado: `DIST Amount 0.25 con Magnitude 1` es bit a bit
    // igual que `Amount 0.5 con Magnitude 0.5`. Ver `macro()` en syncDerived y el `vEff` del
    // bucle de leyes.
    {0, nullptr, LawKind::MagnitudeGlobal},
    // `Digital_FilterLo` va MULTIPLICADO por el macro del módulo: el cruce de estado del
    // motor es `f·(1 − s13)` con `s13 = 1 − macro`, o sea `f·macro`.
    // Verificado en el codo de la curva (nominal 632 Hz): macro 0,20 ⇒ 126 Hz predicho /
    // 136 medido; macro 0,35 ⇒ 221 predicho / 222 medido. Con `f·macro` el caso «filtros
    // abiertos» (f = 20 Hz) sigue siendo inaudible, como debe.
    {29, "Digital_FilterLo", LawKind::FilterLoMacro},
    {30, "Digital_FilterHi", LawKind::FreqLog},
    {32, "Digital_AliasSmoothing", LawKind::Scale100},
    {33, "Digital_Mix", LawKind::Scale100},
    // DIGI R<>B Balance y DIGI Amount no escriben un parámetro del modelo: reparten los dos
    // derivados Digital_DegradeSR / Digital_BitDepth en syncDerived (ver allí).
    {28, nullptr, LawKind::MacroFactor},
    {4, nullptr, LawKind::MacroFactor},
    // --- Espacio (fase 5) -------------------------------------------------------------
    // SPAC Amount → MacroSpaceAmount → Space_Level es la MISMA cadena de macro que
    // Dist_Amount (`macroControlName = "MacroSpace"`, documentado como 1:1 con el macro).
    // [?] Igual que en Dist, el factor Magnitude (host 0) no
    // se aplica porque host 0 sigue sin ley propia.
    {5, "Space_Level", LawKind::MacroAmountCopy},
    // SPAC Decay: `Decay` es 0..1 PURO (sin spec de rango, default 0,8) y el 0..100 de la
    // presentación es un ×100. Ley ajustada por medida:
    // 1/T60(1 kHz) = 0,847 − 0,833·v (v = valor del host) ⇒ lineal, sin curva.
    {36, "Space_Decay", LawKind::Scale100},
    // SPAC Filter Lo/Hi → Dampen/Soften: rango efectivo 100..16000 Hz (el clamp del DSP,
    // no el 22050 del spec). La CURVA se eligió comparando tres candidatas
    // contra el T60 por banda medido en 74 frecuencias:
    // log puro 100·160^v da 8,8 % rms en Filter Lo y **5,9 % en Filter Hi** (la mejor de
    // las tres en Hi y segunda en Lo); la inversión libre del corte da 73/416/1350/4476 Hz en Hi
    // y 312/1147/4532 en Lo, pegada al log puro (100·160^v = 100/355/1265/4499/16000). El
    // El `fitMiddle = 1000` declarado NO se usa: el valor efectivo es 2000, no 1000.
    {37, "Space_Dampen", LawKind::FreqLogRange, 100.0f, 16000.0f},
    {38, "Space_Soften", LawKind::FreqLogRange, 100.0f, 16000.0f},
    // SPAC PreDelay: 0..100 ms, consumido en ms por el DSP
    // Ley MEDIDA con el impulso en 1 s:
    // 0/24,94/49,93/74,88/99,86 ms para v = 0/0,25/0,5/0,75/1 ⇒ lineal `ms = 100·v`, error
    // ≤ 0,14 ms. (La campaña anterior daba 0…20,5 ms porque medía el transitorio del 1-polo de
    // suavizado, τ = 6 999 muestras, aún sin converger.)
    {39, "Space_PreDelay", LawKind::Scale100},
    // --- Ruido (fase 6) ---------------------------------------------------------------
    // NOIS Amount → `Noise_NoiseLevel` en dB. La spec de la tabla declara
    // `min = -78`, `fitMiddle = -12`, `max = 0` y `macroControlName = "MacroNoise"` con el
    // comentario `-- macro param controls this 1:1` ⇒ la curva es la `fit3` del macro.
    {1, "Noise_NoiseLevel", LawKind::NoiseLevel, -78.0f, -12.0f, 0.0f},
    // NOIS Type: índice de opción 0..15 (el plugin declara rango 0..15) ⇒ se redondea.
    {8, "Noise_NoiseType", LawKind::IndexRange, 0.0f, 0.0f, 15.0f},
    // NOIS Tone: `Noise_Tone_Unscaled` es bipolar −12..+12 dB con el
    // centro en 0,5 ⇒ dB = 24·(v − 0,5).
    {9, "Noise_Tone", LawKind::ToneBipolar12},
    // NOIS Follow y NOIS Duck son porcentajes: `Noise_Follow` y
    // `SComp_Threshold`.
    {10, "Noise_Follow", LawKind::Scale100},
    {12, "SComp_Threshold", LawKind::Scale100},
    // DIST Mix (25): el grafo del motor NO usa `Dist_Mix` (0..100 del DSL) sino
    // `Dist_MixInternal` (0..1) en las dos condiciones que reparten el seco y el húmedo
    // (GraphSpec: `Dist.in` recibe `…*Dist_MixInternal` y `DistMix.in` recibe
    // `!Dist_MixInternal*Dist_Enable`). ajustado por medida: el mando del host es el
    // propio `Dist_MixInternal` (identidad 0..1), así que el seco va a (1−m) y la ENTRADA
    // de la cadena húmeda a m — no es un crossfade a la salida, por eso el húmedo cambia
    // de carácter con el mando. Comprobado en el caso Mix de la suite.
    {25, "Dist_MixInternal", LawKind::MacroAmountCopy},
    // DIST Filter Lo/Hi (23/24) NO tienen ley todavía, y está MEDIDO por qué: el corte
    // −3 dB por medida sigue la curva log 20..20000 del DSL (136/600/3200 Hz para
    // v = 0,25/0,5/0,75 con Filter Lo; 158/648/3276 con Filter Hi, frente a
    // 113/632/3557 de la ley), pero al escribirla en el modelo el emparejamiento con el
    // de filtro del bloque Dist (`a1`/`a2` sobre `lo`/`band`) no queda identificada con un
    // solo polo. Se deja sin ley (el bloque usa los valores del DSL), igual que los filtros
    // del Digital.
};

// Los macros de módulo no son parámetros de modelo: son el interruptor del macro y se
// sincronizan a los `syncedEnableParams` (MacroNoiseEnable → Noise_Enable y SComp_Enable;
// el resto, uno a uno).
struct EnableEntry {
    int hostId;
    const char* models[2];
};

const EnableEntry kEnables[] = {
    {7, {"Noise_Enable", "SComp_Enable"}},
    {14, {"Wobble_Enable", nullptr}},
    {21, {"Dist_Enable", nullptr}},
    {27, {"Digital_Enable", nullptr}},
    {35, {"Space_Enable", nullptr}},
    {42, {"Magnetic_Enable", nullptr}},
};

float gainFromDbLaw(float v) {
    const float db = (v - 0.5f) * 36.0f;
    return std::pow(10.0f, db / 20.0f);
}

// `fit3`: y(0) = min, y(0.5) = middle, y(1) = max. Ley de las velocidades del LFO.
float fit3Law(float v, float lo, float mid, float hi);

// índice de opción del host (0..5, continuo) → valor interno de Dist_Type.
int distTypeIndex(float v) {
    int idx = static_cast<int>(v * 5.0f + 0.5f);
    if (idx < 0) idx = 0;
    if (idx > 5) idx = 5;
    return idx;
}
float distTypeForHost(float v) {
    return static_cast<float>(kDistTypeByIndex[distTypeIndex(v)]);
}

// rsmul(x, c) de la casa: la curva de eje normalizado que comparten `sym`, `fit3` y
// `knobToAnyGain`.
float rsmul(float x, float c) { return c * x / ((2.0f * c - 1.0f) * x + 1.0f - c); }

// `fit3`: y(0) = min, y(0.5) = middle, y(1) = max. Ley de las velocidades del LFO.
float fit3Law(float v, float lo, float mid, float hi) {
    const float c = (mid - lo) / (hi - lo);
    return lo + (hi - lo) * rsmul(v, c);
}

// Escala `knobToAnyGain` (tipo 11) con la spec de EQ_Post_Tone_Amount:
// createKnobAnyGainParamSpec(0, 12, -12, 0.2) → min = 10^(+12/20), max = 10^(-12/20), c = 0.2.
float toneAmountLaw(float v) {
    constexpr float kMinAmp = 3.98107171f;  // FromDecibel(+12)
    constexpr float kMaxAmp = 0.251188636f; // FromDecibel(-12)
    constexpr float kCurve = 0.2f;
    const float db = (v >= 0.5f)
                         ? 20.0f * std::log10(kMaxAmp) * rsmul(2.0f * v - 1.0f, kCurve)
                         : 20.0f * std::log10(kMinAmp) * (1.0f - rsmul(2.0f * v, 1.0f - kCurve));
    return std::pow(10.0f, db / 20.0f);
}

// "InLevel_Unscaled" -> "InLevel". El sufijo es la marca del proxy normalizado del host.
std::string modelIdForHost(const HostParamSpec& spec) {
    std::string id(spec.modelId);
    const std::string suffix = "_Unscaled";
    if (id.size() > suffix.size() && id.compare(id.size() - suffix.size(), suffix.size(), suffix) == 0)
        id.resize(id.size() - suffix.size());
    return id;
}

} // namespace

ParamStore::ParamStore() {
    for (int i = 0; i < ParamTable::count(); ++i) {
        const ParamSpec& s = ParamTable::all()[i];
        model_[static_cast<size_t>(modelCount_)].id = s.id;
        model_[static_cast<size_t>(modelCount_)].value = s.defaultValue;
        ++modelCount_;
    }
    // El motor crea `OutGainEnable` fuera del modelo de parámetros (el arranque de la GUI
    // lo pone a 0 si falla la autorización). Aquí no hay puerta de licencia:
    // la arista Width.out -> outAudio queda siempre habilitada.
    model_[static_cast<size_t>(modelCount_)].id = "OutGainEnable";
    model_[static_cast<size_t>(modelCount_)].value = 1.0f;
    ++modelCount_;
    hostValues_.fill(0.0f);
    // El estado de fábrica no es "todo a cero": varios parámetros del modelo
    // se declaran con el DEFAULT en dominio host (p. ej. `Digital_FilterHi`
    // default 1 sobre un rango 20..20000 Hz), así que sin este sync un `ParamStore` recién
    // construido deja un paso-bajo a 20 Hz. Medido con tests/test_digital.cpp. Los 58
    // parámetros del host se aplican con sus leyes y luego syncDerived().
    resetToDefaults();
}

void ParamStore::resetToDefaults() {
    for (int i = 0; i < modelCount_; ++i) {
        const ParamSpec* s = ParamTable::find(model_[static_cast<size_t>(i)].id.c_str());
        model_[static_cast<size_t>(i)].value = s ? s->defaultValue
                                                 : (model_[static_cast<size_t>(i)].id == "OutGainEnable" ? 1.0f : 0.0f);
    }
    // El estado de fábrica del motor enciende DIST/NOIS/MAGN; ese estado sale de los
    // defaults de los macros del host, así que se aplican después del modelo.
    for (int id = 0; id < HostParamTable::count(); ++id) {
        const HostParamSpec* spec = HostParamTable::all() + id;
        setHost(spec->id, spec->defaultValue);
    }
    syncDerived();
}

// Parámetros derivados del módulo EQ. Son `audioSyncCopy` /
// `audioSyncFromTo` sobre el valor NORMALIZADO del control, y el DSL de plugchain los
// consume así (0..1 con su propia escala min/max). Sin esto, el intérprete del DSL ve 0
// y las etapas quedan apagadas.
void ParamStore::syncDerived() {
    auto norm = [&](int hostId) { return hostValues_[static_cast<size_t>(hostId)]; };
    // `Magnitude` (host 0) es el escalador global de los macros y MULTIPLICA en lineal:
    // Verificado: `DIST Amount 0.25 con Magnitude 1` es BIT A BIT igual que
    // `DIST Amount 0.5 con Magnitude 0.5` (ΔRMS 0.00 dB en las nueve bandas de octava).
    // Por eso el macro efectivo es `knob · Magnitude` y
    // no hay ninguna curva de por medio.
    auto macro = [&](int hostId) {
        return hostValues_[static_cast<size_t>(hostId)] * hostValues_[0];
    };
    // Frecuencias: copia del valor normalizado del control de corte.
    setModel("EQ_Post_CutHard_LoFreq", norm(50));
    setModel("EQ_Post_CutSoft_LoFreq", norm(50));
    setModel("EQ_Post_CutHard_HiFreq", norm(51));
    setModel("EQ_Post_CutSoft_HiFreq", norm(51));
    // Q de los cortes duros. Son los defaults del control (`CutHard_LoQ` 0.8,
    // `CutHard_HiQ` 0.5) SIN interpolar: el DSL los vuelve a escalar con `q 0.1 5 log`,
    // de donde salen q = 2.291 y q = 0.707. Verificado en modo Hard:
    // lp12 x2 a 8.78 kHz con 0.707 → 0.03 dB rms (con 0.8, 0.72 dB rms); hp12 a 62 Hz
    // con 2.291 → 0.12 dB rms (con 0.8, 2.7 dB rms). El `audioSyncFromTo` de
    // `CutHard_LoQ` (0.5..0.8 según la frecuencia) NO se aplica en el audio.
    setModel("EQ_Post_CutHard_LoQ", 0.8f);
    setModel("EQ_Post_CutHard_HiQ", 0.5f);
    // Modos: 0 = Soft (default), 1 = Hard.
    const float loMode = norm(52), hiMode = norm(53), toneMode = norm(55);
    setModel("EQ_Post_CutHard_Lo_Enable", loMode);
    setModel("EQ_Post_CutSoft_Lo_Enable", 1.0f - loMode);
    setModel("EQ_Post_CutHard_Hi_Enable", hiMode);
    setModel("EQ_Post_CutSoft_Hi_Enable", 1.0f - hiMode);
    setModel("EQ_Post_Tone_Tilt_Enable", 1.0f - toneMode);
    setModel("EQ_Post_Tone_Mid_Enable", toneMode);
    // Tilt = valor NORMALIZADO del control Tone (audioSyncCopy de EQ_Post_Tone_Amount),
    // no la ganancia lineal: el DSL lo vuelve a escalar con `db Tilt -24 24 sqr`, y el
    // barrido por medida da exactamente 24*x*|x| con x = 2v-1 (v=0 → -24, 0.25 → -6,
    // 0.5 → 0, 0.85 → +11.76, 1 → +24; error < 0.01 dB en 22 puntos).
    setModel("EQ_Post_Tone_Tilt", norm(54));

    // --- modulación (fase 4) ----------------------------------------------------------
    // El DSP de cWobbler NO ve Wobble_Xfade: el grafo cablea
    // `Wobble_Xfade = audioSyncTriangleCrossFade{Wobble_Depth_Unscaled, Wobble_Depth2_Unscaled}`,
    // así que el reparto wow/flutter se hace aquí, sobre el valor normalizado del cruce
    // triangular (el mismo `min(1,2x)` que usa el bloque para FilterXfade):
    // Depth = max(Depth)·A, A = min(1, 2(1−x))
    // Depth2 = max(Depth2)·B, B = min(1, 2x)
    // y después el factor del macro de módulo (MacroWobbleAmount_Scaled, host 2).
    const float xfadeW = norm(15);
    const float macroWobble = macro(2);
    const float wowA = std::min(1.0f, 2.0f * (1.0f - xfadeW));
    const float wowB = std::min(1.0f, 2.0f * xfadeW);
    setModel("Wobble_Depth", 0.8f * wowA * macroWobble);
    setModel("Wobble_Depth2", 0.2f * wowB * macroWobble);
    // Wobble_StereoPhase = audioSyncFromTo{Wobble_Stereo_Enable, 0,1, 0,0.4, 0.5}
    // 0.4 ciclos de desfase a la derecha, sólo en el motor A.
    setModel("Wobble_StereoPhase", 0.4f * norm(17));
    // LfoNoise/LfoNoiseScaler: el ruido aditivo del wobbler sale del generador de *flux*
    // (seedStart 139), no reconstruido ⇒ se quedan a 0 (estado de fábrica con Flux = 0). [?]
    setModel("Wobble_LfoNoise", 0.0f);
    setModel("Wobble_LfoNoiseScaler", 0.0f);

    // cFlutter: `Magnetic_Xfade = audioSyncTriangleCrossFade{Magnetic_WearDepth_Unscaled,
    // Magnetic_Depth_Unscaled}` ⇒ A reparte el desgaste y B la AM del LFO.
    const float xfadeM = norm(43);
    const float macroMagnetic = macro(6);
    const float magA = std::min(1.0f, 2.0f * (1.0f - xfadeM));
    const float magB = std::min(1.0f, 2.0f * xfadeM);
    setModel("Magnetic_WearDepth", magA * macroMagnetic);
    setModel("Magnetic_Depth", magB * macroMagnetic);
    // `Magnetic_WearGain` es una señal de *flux* (`fluxParam` con
    // `bipolarNoise = true`, Flux 0.44) que no generamos, pero su efecto sí es medible: el
    // desgaste atenúa el nivel de forma casi lineal con el macro. Ley ajustada por medida
    // (ruido rosa −20 dBFS, Magnetic solo, resto apagado):
    //   amount 0 → −24,07 dBFS | 0,25 → −25,38 (−1,26) | 0,5 → −26,81 (−2,65)
    //   0,75 → −28,31 (−4,11) | 1,0 → −29,69 (−5,44)
    // Con `wearGain = 1 − WearDepth·smoothstep(g)` y WearDepth = amount, eso pide
    // smoothstep(g) ≈ 0,52 constante, o sea g ≈ 0,5 (smoothstep(0,5) = 0,5). Se usa ese
    // VALOR MEDIO estático en lugar del ruido de flux: la atenuación queda a ±0,4 dB del
    setModel("Magnetic_WearGain", 0.5f);
    // `Magnetic_DropOutSteerSig` también es de flux (seedStart 151). En el DSP el dropout cae
    // cuando la señal baja del mínimo; con el generador sin reconstruir se deja en su MÁXIMO,
    // que es el estado «sin dropout» (con 0 el canal quedaba mudo en cuanto se subía
    // `MAGN Dropouts`: medido, −300 dBFS donde fábrica pierde 0,15 dB).
    setModel("Magnetic_DropOutSteerSig", 1.0f);
    // Dropouts: Mult1 = fromTo(MacroMagneticAmount, 0,1 → 0,1, curva
    // 0.98) — se queda cerca de 1 y sólo cae al final del recorrido del macro —,
    // Mult2 = Mult1·MasterMacroScaler (no aplicado, ver arriba) y
    // DropOutsAmount = DropOutsAmount_GUI·Mult1·Mult2. Min/Max son `fromTo` de Amount.
    // [?] La curva 0.98 del `syncFromTo` se toma como el `sym` de la casa; sin el generador
    // de *flux* que mueve DropOutSteerSig el modo sólo es medible como ganancia estática.
    const float dropMult1 = rsmul(macroMagnetic, 0.98f);
    const float dropAmount = norm(46) * dropMult1 * dropMult1;
    setModel("Magnetic_DropOutsAmountMult1", dropMult1);
    setModel("Magnetic_DropOutsAmountMult2", dropMult1 * dropMult1);
    setModel("Magnetic_DropOutsAmount", dropAmount);
    setModel("Magnetic_DropOutsMin", 0.1f + 0.58f * dropAmount);
    setModel("Magnetic_DropOutsMax", 0.25f + 0.63f * dropAmount);

    // --- Digital (fase 7) ------------------------------------------------------------
    // BitDepth y DegradeSR llevan `macroControlName = "MacroDigital"`
    // con `fitMiddle` (24/6/1 bits y 44100/200/44.1 Hz) y `Digital_Xfade` es el
    // `audioSyncTriangleCrossFade` de los dos `_Unscaled`, igual que en Wobble. Verificado:
    // con el balance a R (x=0) el módulo retiene a 32·fit3(macro) Hz
    // y no toca los bits; con el balance a B (x=1) cuantiza a fit3(macro) bits y no reduce
    // la tasa (imágenes espectrales a 32·DegradeSR, p. ej. 1411 Hz con macro 1 y x=0).
    {
        const float macroDigi = std::clamp(macro(4), 0.0f, 1.0f);
        const float x = std::clamp(norm(28), 0.0f, 1.0f);
        const float sideA = std::min(1.0f, 2.0f * (1.0f - x));   // reducción de tasa
        const float sideB = std::min(1.0f, 2.0f * x);           // reducción de bits
        setModel("Digital_DegradeSR", fit3Law(std::clamp(macroDigi * sideA, 0.0f, 1.0f),
                                             44100.0f, 200.0f, 44.1f));
        setModel("Digital_BitDepth", fit3Law(std::clamp(macroDigi * sideB, 0.0f, 1.0f),
                                            24.0f, 6.0f, 1.0f));
                                            // `Digital_Focus = audioSyncFromTo{Digital_FocusMode, 0,1, 1,0, 0.5}`:
        // modo 0 ⇒ Focus = 1, modo 1 ⇒ Focus = 0. El bloque usa Focus para el término directo de
        // la mezcla; el número de etapas de filtro ligado al mismo valor
        // NO está confirmado (medido: 1 polo con FocusMode = 1) y la
        // posición de los filtros queda declarada como no identificada en BitColor.h.
        setModel("Digital_Focus", 1.0f - std::clamp(norm(31), 0.0f, 1.0f));
    }
}

float* ParamStore::findModel(const std::string& id) {
    for (int i = 0; i < modelCount_; ++i)
        if (model_[static_cast<size_t>(i)].id == id) return &model_[static_cast<size_t>(i)].value;
    return nullptr;
}

const float* ParamStore::findModel(const std::string& id) const {
    for (int i = 0; i < modelCount_; ++i)
        if (model_[static_cast<size_t>(i)].id == id) return &model_[static_cast<size_t>(i)].value;
    return nullptr;
}

bool ParamStore::hasModel(const std::string& id) const { return findModel(id) != nullptr; }

bool ParamStore::modelRange(const std::string& id, float& lo, float& hi) const {
    const ParamSpec* s = ParamTable::find(id.c_str());
    if (!s) return false;
    lo = s->minValue;
    hi = s->maxValue;
    return true;
}

void ParamStore::setModel(const std::string& id, float value) {
    if (float* p = findModel(id)) *p = value;
}

float ParamStore::get(const char* id) const {
    const float* p = findModel(id);
    return p ? *p : 0.0f;
}

const HostParamSpec* ParamStore::hostSpec(int id) {
    if (id < 0 || id >= HostParamTable::count()) return nullptr;
    return HostParamTable::all() + id;
}

float ParamStore::hostValue(int id) const {
    if (id < 0 || id >= HostParamTable::count()) return 0.0f;
    return hostValues_[static_cast<size_t>(id)];
}

LawStatus ParamStore::lawStatus(int hostId) const {
    const HostParamSpec* spec = hostSpec(hostId);
    if (!spec) return LawStatus::Unverified;
    for (const auto& l : kLaws)
        if (l.hostId == hostId) return LawStatus::Verified;
    for (const auto& e : kEnables)
        if (e.hostId == hostId) return LawStatus::Verified;
    // Identidad por construcción: mismo id en el modelo y rango 0..1 en ambos dominios.
    const std::string mid = modelIdForHost(*spec);
    if (const ParamSpec* ms = ParamTable::find(mid.c_str()))
        if (ms->minValue == 0.0f && ms->maxValue == 1.0f) return LawStatus::Identity;
    return LawStatus::Unverified;
}

const char* ParamStore::lawName(int hostId) const {
    for (const auto& l : kLaws)
        if (l.hostId == hostId) {
            switch (l.kind) {
                case LawKind::GainDb36: return "dB=(v-0.5)*36";
                case LawKind::Scale200: return "v*200";
                case LawKind::FreqLog: return "20*1000^v";
                case LawKind::FreqLogRange: return "min*(max/min)^v";
                case LawKind::KnobAnyGain12: return "knobToAnyGain c=0.2 (+12..-12 dB)";
                case LawKind::DistTypeIndex: return "índice de tipo → Dist_Type";
                case LawKind::MacroAmountCopy: return "MacroDistAmount → Dist_Amount";
                case LawKind::Fit3: return "fit3 (min/fitMiddle/max)";
                case LawKind::Scale100: return "v*100";
                case LawKind::MacroFactor: return "macro de módulo → params derivados";
                case LawKind::NoiseLevel: return "fit3(-78,-12,0) dB";
                case LawKind::ToneBipolar12: return "dB=24*(v-0.5)";
                case LawKind::IndexRange: return "round(v*max)";
                case LawKind::FilterLoMacro: return "20*1000^v * MacroDigitalAmount";
                case LawKind::MagnitudeGlobal: return "escala lineal de los 6 macros";
            }
        }
    for (const auto& e : kEnables)
        if (e.hostId == hostId) return "macro-enable";
    switch (lawStatus(hostId)) {
        case LawStatus::Identity: return "identidad 0..1";
        case LawStatus::Verified: return "verificada";
        case LawStatus::Unverified: return "sin verificar";
    }
    return "?";
}

bool ParamStore::setHost(int id, float normalized) {
    const HostParamSpec* spec = hostSpec(id);
    if (!spec) return false;
    const float v = normalized < 0.0f ? 0.0f : (normalized > 1.0f ? 1.0f : normalized);
    hostValues_[static_cast<size_t>(spec->id)] = v;

    // Magnitude no mapea a un parámetro: re-aplica los seis macros con el factor nuevo.
    if (spec->id == 0) {
        for (int i = 1; i <= 6; ++i) setHost(i, hostValues_[static_cast<size_t>(i)]);
        syncDerived();
        return true;
    }

    for (const auto& e : kEnables) {
        if (e.hostId != spec->id) continue;
        for (const char* m : e.models)
            if (m) setModel(m, v);
        syncDerived();
        return true;
    }

    for (const auto& l : kLaws) {
        if (l.hostId != spec->id) continue;
        // Los seis macros de módulo (host 1..6) van escalados por `Magnitude` (host 0).
        const bool isMacro = (l.hostId >= 1 && l.hostId <= 6);
        const float vEff = isMacro ? v * hostValues_[0] : v;
        float value = vEff;
        if (l.kind == LawKind::GainDb36) value = gainFromDbLaw(vEff);
        if (l.kind == LawKind::Scale200) value = vEff * 200.0f;
        if (l.kind == LawKind::FreqLog) value = 20.0f * std::pow(1000.0f, vEff);
        if (l.kind == LawKind::FilterLoMacro)
            value = 20.0f * std::pow(1000.0f, v) * hostValues_[4];   // × MacroDigitalAmount
        if (l.kind == LawKind::FreqLogRange)
            value = l.p0 * std::pow(l.p1 / l.p0, vEff);
        if (l.kind == LawKind::KnobAnyGain12) value = toneAmountLaw(vEff);
        if (l.kind == LawKind::DistTypeIndex) value = distTypeForHost(vEff);
        if (l.kind == LawKind::MacroAmountCopy) value = vEff;
        if (l.kind == LawKind::Fit3) value = fit3Law(vEff, l.p0, l.p1, l.p2);
        if (l.kind == LawKind::Scale100) value = vEff * 100.0f;
        if (l.kind == LawKind::NoiseLevel) value = fit3Law(vEff, l.p0, l.p1, l.p2);
        if (l.kind == LawKind::ToneBipolar12) value = 24.0f * (vEff - 0.5f);
        if (l.kind == LawKind::IndexRange) value = std::round(vEff * l.p2);
        if (l.modelId) setModel(l.modelId, value);   // MacroFactor: sólo derivados
        syncDerived();
        return true;
    }

    const std::string mid = modelIdForHost(*spec);
    if (const ParamSpec* ms = ParamTable::find(mid.c_str())) {
        if (ms->minValue == 0.0f && ms->maxValue == 1.0f) {
            setModel(mid, v);
            // Los identidad también mueven derivados: `EQ_Post_Tone_Mode` (55) elige
            // entre Tilt y Mid y `CutLo/Hi_Mode` (52/53) entre suave y duro. Sin este
            // sync el bloque se queda con los enables del último sync (medido: con sólo
            // 55=1 la medida cambia de shelf a campana y nosotros seguíamos con shelf).
            syncDerived();
            return true;
        }
        // Mismo nombre en el modelo pero otro rango: falta la ley. Se guarda el valor
        // del host y se deja constancia; no se inventa la conversión.
        return false;
    }

    // Enums con el mismo rango que su modelo (p. ej. Noise_NoiseType 0..15).
    if (spec->isEnum) {
        setModel(mid, std::round(v * spec->max));
        return true;
    }
    return false;
}

ParamStore::Coverage ParamStore::coverage() const {
    Coverage c;
    c.total = HostParamTable::count();
    for (int i = 0; i < c.total; ++i) {
        switch (lawStatus(i)) {
            case LawStatus::Verified: ++c.verified; break;
            case LawStatus::Identity: ++c.identity; break;
            case LawStatus::Unverified: ++c.unverified; break;
        }
    }
    return c;
}

} // namespace rtxx
