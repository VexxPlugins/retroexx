#pragma once
// cFlutter (nodo `Magnetic` del grafo): modulación de amplitud (no hay línea de retardo;
// el *pitch* lo pone cWobbler).
//
// Ecuaciones cerradas por muestra y canal:
// L = forma(fase) ∈ [−1,+1] misma tabla que cWobbler
// w = smoothstep((|DepthSteerSig|+0.1)/1.2), smoothstep(x) = x²(3−2x)
// DepthEff = Depth·(1 − w)
// comp = 1 + 0.41253754·DepthEff
// g_LFO = 1 + DepthEff·(L−1)/2 ∈ [1−DepthEff, 1] AM
// g_wear = clamp(1 − WearDepth·w_hat, 0.031, 1) mono y estéreo
// y = x·comp·g_LFO·g_wear·g_drop
//
// [!] Abierto y por eso NO se implementa: (a) el prefiltro SVF que sólo se aplica cuando
// hay *dropouts* activos; (b) las señales de *flux* (DepthSteerSig, WearGain, WearPan,
// DropOutSteerSig) las genera un nodo de sync con semilla fija que no está implementado;
// aquí valen lo que dicen los parámetros del modelo (0.5/0/0/0 de fábrica), que es el
// estado sin flujo.
#include "block/Block.h"
#include "blocks/Wobbler.h"   // lfo: tabla de 11 formas (idéntica en las dos clases)
#include "model/ParamStore.h"

namespace rtxx {

class FlutterBlock : public iBlock {
public:
    explicit FlutterBlock(const ParamStore* params) : params_(params) {}

    void prepare(const PrepareContext& ctx) override;
    void reset() override;
    void process(const PortData* in, PortData& out) override;

    // --- leyes cerradas, expuestas para los tests ---
    // smoothstep(x) = x²(3−2x), con x = (|sig|+0.1)/1.2 si |sig| ≤ 1.1 (si no 1).
    static float smoothstepForSteer(float steer);
    // DepthEff = Depth·(1 − w(DepthSteerSig)).
    static float depthEffective(float depth, float steerSig);
    // Compensación de ganancia.
    static float compensation(float depthEff) { return 1.0f + 0.41253754f * depthEff; }
    // AM del LFO: 1 + DepthEff·(L−1)/2.
    static float lfoGain(float depthEff, float l) { return 1.0f + depthEff * (l - 1.0f) * 0.5f; }
    // Desgaste de ganancia: mono, y las dos ramas del modo estéreo (WearPan ∈ [−1,1]).
    static float wearGain(float wearDepth, float wearGain);
    static float wearGainLeft(float wearDepth, float wearGain, float wearPan);
    static float wearGainRight(float wearDepth, float wearGain, float wearPan);
    // Dropouts deterministas: p = (clamp(sig,dmin,dmax) − dmin)/(dmax − dmin) y las dos
    // ganancias de canal; 1 si el módulo de dropouts está a 0.
    static float dropoutGainLeft(float amount, float steerSig, float dmin, float dmax, float wearPan);
    static float dropoutGainRight(float amount, float steerSig, float dmin, float dmax, float wearPan);

private:
    const ParamStore* params_ = nullptr;
    double sampleRate_ = 44100.0;
    ModLfo lfo_;
};

} // namespace rtxx
