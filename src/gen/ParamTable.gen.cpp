// Generado por la herramienta de parámetros. No editar a mano: se regenera.
// Contrato: src/model/ParamSpec.h
//
// Contiene las 125 entradas de estado/preset:
//   * 123 de Config.configParams
//   *   2 de stateVersionSpec (version, savedWith)
// Los macros (8) y los params del sistema Flux (7) NO están aquí — ParamTable::count()
// está fijado a 125 por contrato. Viven en params.json con grupo Macro/Flux.
//
// El orden es el del fichero Lua. OJO: El motor crea los parámetros con `pairs()`
// así que el orden del modelo en el motor no es
// reproducible estáticamente: el cruce por medida debe ser por id, nunca por índice.
#include <cstring>

#include "../model/ParamSpec.h"

namespace rtxx {
namespace {

const ParamSpec kParamSpecs[] = {
    // defaultValue es cadena ('')
    { "AudioPlayer_File", ParamGroup::Misc, 0.0f, 1.0f, 0.0f, false, false },
    // bool
    { "Noise_Enable", ParamGroup::Noise, 0.0f, 1.0f, 0.0f, true, false },
    // default=normalizado, default en dominio normalizado y min/max reales
    { "Noise_Follow", ParamGroup::Noise, 0.0f, 100.0f, 0.0500000007f, false, false },
    { "Noise_NoiseLevel_F_Smooth", ParamGroup::Noise, 0.0f, 1.0f, 0.819999993f, false, false },
    // default=normalizado, default en dominio normalizado y min/max reales
    { "Noise_NoiseLevel", ParamGroup::Noise, -78.0f, 0.0f, 1.0f, false, false },
    // enum:NoiseTypeSpec
    { "Noise_NoiseType", ParamGroup::Noise, 0.0f, 15.0f, 1.0f, false, true },
    // default=normalizado, default en dominio normalizado y min/max reales
    { "Noise_Tone", ParamGroup::Noise, -12.0f, 12.0f, 0.5f, false, false },
    { "Noise_GateLevel", ParamGroup::Noise, 0.0f, 1.0f, 0.0f, false, false },
    // bool
    { "Noise_RandomSeekEnable", ParamGroup::Noise, 0.0f, 1.0f, 1.0f, true, false },
    // bool
    { "NoisePost", ParamGroup::Misc, 0.0f, 1.0f, 0.0f, true, false },
    // bool
    { "SComp_Enable", ParamGroup::SComp, 0.0f, 1.0f, 0.0f, true, false },
    { "SComp_Ratio", ParamGroup::SComp, 0.0f, 1.0f, 0.5f, false, false },
    { "SComp_Averaging", ParamGroup::SComp, 0.0f, 1.0f, 0.0f, false, false },
    { "SComp_GainComp", ParamGroup::SComp, 0.0f, 1.0f, 0.0f, false, false },
    // default=normalizado, default en dominio normalizado y min/max reales
    { "SComp_Threshold", ParamGroup::SComp, 0.0f, 100.0f, 0.0f, false, false },
    { "SComp_Attack", ParamGroup::SComp, 0.0f, 1.0f, 0.0f, false, false },
    { "SComp_Release", ParamGroup::SComp, 0.0f, 1.0f, 0.200000003f, false, false },
    { "SComp_HiPassSidechain", ParamGroup::SComp, 0.0f, 1.0f, 1.0f, false, false },
    { "WaveVizOutputNoise_GainOutput", ParamGroup::WaveViz, 0.0f, 1.0f, 4.0f, false, false },
    // bool
    { "WaveVizOutputNoise_LimitEnable", ParamGroup::WaveViz, 0.0f, 1.0f, 0.0f, true, false },
    // bool
    { "Wobble_Enable", ParamGroup::Wobble, 0.0f, 1.0f, 0.0f, true, false },
    { "Wobble_LfoNoiseScaler", ParamGroup::Wobble, 0.0f, 1.0f, 0.0f, false, false },
    { "Wobble_LfoNoise", ParamGroup::Wobble, 0.0f, 1.0f, 0.0f, false, false },
    // default=normalizado, default en dominio normalizado y min/max reales
    { "Wobble_Depth", ParamGroup::Wobble, 0.0f, 0.800000012f, 1.0f, false, false },
    // default=normalizado, default en dominio normalizado y min/max reales
    { "Wobble_Depth2", ParamGroup::Wobble, 0.0f, 0.200000003f, 1.0f, false, false },
    { "Wobble_Xfade", ParamGroup::Wobble, 0.0f, 1.0f, 0.5f, false, false },
    // default=normalizado, default en dominio normalizado y min/max reales
    { "Wobble_Rate", ParamGroup::Wobble, 0.100000001f, 4.0f, 0.200000003f, false, false },
    // default=normalizado, default en dominio normalizado y min/max reales
    { "Wobble_Rate2", ParamGroup::Wobble, 6.0f, 20.0f, 0.294f, false, false },
    { "Wobble_StereoPhase", ParamGroup::Wobble, 0.0f, 1.0f, 0.0f, false, false },
    // bool
    { "Wobble_Stereo_Enable", ParamGroup::Wobble, 0.0f, 1.0f, 0.0f, true, false },
    // default=normalizado, default en dominio normalizado y min/max reales
    { "Wobble_Mix", ParamGroup::Wobble, 0.0f, 100.0f, 1.0f, false, false },
    // bool
    { "Dist_Enable", ParamGroup::Dist, 0.0f, 1.0f, 0.0f, true, false },
    // enum:DistTypeSpec
    { "Dist_Type", ParamGroup::Dist, 0.0f, 7.0f, 5.0f, false, true },
    { "Dist_Amount", ParamGroup::Dist, 0.0f, 1.0f, 1.0f, false, false },
    { "Dist_InGain", ParamGroup::Dist, 0.0f, 1.0f, 1.0f, false, false },
    { "Dist_OutGain", ParamGroup::Dist, 0.0f, 1.0f, 1.0f, false, false },
    { "Dist_InGain_Feed_F_Gain", ParamGroup::Dist, 0.0f, 1.0f, 1.0f, false, false },
    { "Dist_InGain_Feed_F_Smooth", ParamGroup::Dist, 0.0f, 1.0f, 1.0f, false, false },
    { "Dist_InGain_Feed", ParamGroup::Dist, 0.0f, 1.0f, 0.5f, false, false },
    { "Dist_MixFromAmount", ParamGroup::Dist, 0.0f, 1.0f, 1.0f, false, false },
    // default=normalizado, default en dominio normalizado y min/max reales
    { "Dist_Mix", ParamGroup::Dist, 0.0f, 100.0f, 1.0f, false, false },
    { "Dist_MixInternal", ParamGroup::Dist, 0.0f, 1.0f, 1.0f, false, false },
    // default=normalizado, default en dominio normalizado y min/max reales
    { "Dist_FreqLo", ParamGroup::Dist, 20.0f, 20000.0f, 0.0f, false, false },
    // default=normalizado, default en dominio normalizado y min/max reales
    { "Dist_FreqHi", ParamGroup::Dist, 20.0f, 20000.0f, 1.0f, false, false },
    // bool
    { "Digital_Enable", ParamGroup::Digital, 0.0f, 1.0f, 0.0f, true, false },
    // default=normalizado, default en dominio normalizado y min/max reales
    { "Digital_AliasSmoothing", ParamGroup::Digital, 0.0f, 100.0f, 0.0f, false, false },
    // default=normalizado, default en dominio normalizado y min/max reales
    { "Digital_Mix", ParamGroup::Digital, 0.0f, 100.0f, 1.0f, false, false },
    // enum:CutFocusSpec
    { "Digital_FocusMode", ParamGroup::Digital, 0.0f, 1.0f, 0.0f, false, true },
    { "Digital_Focus", ParamGroup::Digital, 0.0f, 1.0f, 1.0f, false, false },
    { "Digital_DegradeModRate", ParamGroup::Digital, 0.0f, 1.0f, 0.0f, false, false },
    { "Digital_AA2", ParamGroup::Digital, 0.0f, 1.0f, 0.5f, false, false },
    { "Digital_AA3", ParamGroup::Digital, 0.0f, 1.0f, 0.649999976f, false, false },
    // default=real
    { "Digital_RoundError", ParamGroup::Digital, 0.0f, 1.0f, -0.980000019f, false, false },
    { "Digital_DegradeModDepth", ParamGroup::Digital, 0.0f, 1.0f, 0.0f, false, false },
    { "Digital_Feedback", ParamGroup::Digital, 0.0f, 1.0f, 0.0f, false, false },
    // default=normalizado, min>max, sin defaultValue/defaultScaled
    { "Digital_BitDepth", ParamGroup::Digital, 24.0f, 1.0f, 0.0f, false, false },
    { "Digital_DegradeSR_F_Smooth", ParamGroup::Digital, 0.0f, 1.0f, 0.837000012f, false, false },
    // default=normalizado, min>max, sin defaultValue/defaultScaled
    { "Digital_DegradeSR", ParamGroup::Digital, 44100.0f, 44.0999985f, 0.0f, false, false },
    { "Digital_Xfade", ParamGroup::Digital, 0.0f, 1.0f, 0.5f, false, false },
    // default=normalizado, default en dominio normalizado y min/max reales
    { "Digital_FilterLo", ParamGroup::Digital, 20.0f, 20000.0f, 0.0f, false, false },
    // default=normalizado, default en dominio normalizado y min/max reales
    { "Digital_FilterHi", ParamGroup::Digital, 20.0f, 20000.0f, 1.0f, false, false },
    // bool
    { "Space_Enable", ParamGroup::Space, 0.0f, 1.0f, 0.0f, true, false },
    { "Space_Level", ParamGroup::Space, 0.0f, 1.0f, 1.0f, false, false },
    // enum:MonoStereoSpec
    { "Space_StereoMode", ParamGroup::Space, 0.0f, 1.0f, 0.0f, false, true },
    // default=normalizado, default en dominio normalizado y min/max reales
    { "Space_Decay", ParamGroup::Space, 0.0f, 100.0f, 0.800000012f, false, false },
    { "Space_DualMono", ParamGroup::Space, 0.0f, 1.0f, 1.0f, false, false },
    // default=normalizado, default en dominio normalizado y min/max reales
    { "Space_PreDelay", ParamGroup::Space, 0.0f, 100.0f, 0.0f, false, false },
    { "Space_PreWobbleNoiseAmount", ParamGroup::Space, 0.0f, 1.0f, 0.0f, false, false },
    { "Space_PreWobbleNoise", ParamGroup::Space, 0.0f, 1.0f, 0.0f, false, false },
    { "Space_StereoDetune", ParamGroup::Space, 0.0f, 1.0f, 0.391799986f, false, false },
    // default=normalizado, default en dominio normalizado y min/max reales
    { "Space_Dampen", ParamGroup::Space, 100.0f, 16000.0f, 0.0f, false, false },
    // default=normalizado, default en dominio normalizado y min/max reales
    { "Space_Soften", ParamGroup::Space, 100.0f, 16000.0f, 1.0f, false, false },
    { "Space_EnableFeedBacks", ParamGroup::Space, 0.0f, 1.0f, 1.0f, false, false },
    // bool
    { "Magnetic_Enable", ParamGroup::Magnetic, 0.0f, 1.0f, 1.0f, true, false },
    { "Magnetic_Xfade", ParamGroup::Magnetic, 0.0f, 1.0f, 0.5f, false, false },
    { "Magnetic_Depth", ParamGroup::Magnetic, 0.0f, 1.0f, 1.0f, false, false },
    { "Magnetic_DepthSteerSig_F_Gain", ParamGroup::Magnetic, 0.0f, 1.0f, 1.0f, false, false },
    { "Magnetic_DepthSteerSig_F_Smooth", ParamGroup::Magnetic, 0.0f, 1.0f, 0.837000012f, false, false },
    { "Magnetic_DepthSteerSig", ParamGroup::Magnetic, 0.0f, 1.0f, 0.5f, false, false },
    { "Magnetic_WearDepth", ParamGroup::Magnetic, 0.0f, 1.0f, 1.0f, false, false },
    { "Magnetic_DropOutsAmount", ParamGroup::Magnetic, 0.0f, 1.0f, 0.0f, false, false },
    { "Magnetic_DropOutsAmountMult1", ParamGroup::Magnetic, 0.0f, 1.0f, 0.0f, false, false },
    { "Magnetic_DropOutsAmountMult2", ParamGroup::Magnetic, 0.0f, 1.0f, 0.0f, false, false },
    // default=normalizado, default en dominio normalizado y min/max reales
    { "Magnetic_DropOutsAmount_GUI", ParamGroup::Magnetic, 0.0f, 100.0f, 0.0f, false, false },
    { "Magnetic_DropOutsMin", ParamGroup::Magnetic, 0.0f, 1.0f, 0.0f, false, false },
    { "Magnetic_DropOutsMax", ParamGroup::Magnetic, 0.0f, 1.0f, 0.0f, false, false },
    { "Magnetic_DropOutSteerSig_F_Smooth", ParamGroup::Magnetic, 0.0f, 1.0f, 0.0f, false, false },
    { "Magnetic_DropOutSteerSig", ParamGroup::Magnetic, 0.0f, 1.0f, 0.0f, false, false },
    { "Magnetic_WearGain_F_Smooth", ParamGroup::Magnetic, 0.0f, 1.0f, 0.837000012f, false, false },
    { "Magnetic_WearGain", ParamGroup::Magnetic, 0.0f, 1.0f, 0.0f, false, false },
    // bool
    { "Magnetic_WearPanEnable", ParamGroup::Magnetic, 0.0f, 1.0f, 0.0f, true, false },
    { "Magnetic_WearPan", ParamGroup::Magnetic, 0.0f, 1.0f, 0.0f, false, false },
    // default=normalizado, default en dominio normalizado y min/max reales
    { "Magnetic_Rate", ParamGroup::Magnetic, 6.0f, 30.0f, 0.5f, false, false },
    { "EQ_Post_Tone_MidFreq", ParamGroup::EQ, 0.0f, 1.0f, 0.535700023f, false, false },
    { "EQ_Post_Tone_MidGain", ParamGroup::EQ, 0.0f, 1.0f, 0.5f, false, false },
    { "EQ_Post_Tone_MidQ", ParamGroup::EQ, 0.0f, 1.0f, 0.200000003f, false, false },
    { "EQ_Post_Tone_Tilt", ParamGroup::EQ, 0.0f, 1.0f, 0.5f, false, false },
    // enum:TiltMidSpec
    { "EQ_Post_Tone_Mode", ParamGroup::EQ, 0.0f, 1.0f, 0.0f, false, true },
    // bool
    { "EQ_Post_Tone_Tilt_Enable", ParamGroup::EQ, 0.0f, 1.0f, 1.0f, true, false },
    // bool
    { "EQ_Post_Tone_Mid_Enable", ParamGroup::EQ, 0.0f, 1.0f, 0.0f, true, false },
    // default=real, min>max, min>max en el Lua (rango invertido a propósito); se conserva literal
    { "EQ_Post_Tone_Amount", ParamGroup::EQ, 3.98107171f, 0.251188636f, 1.0f, false, false },
    { "EQ_Post_Tone_GainComp", ParamGroup::EQ, 0.0f, 1.0f, 1.0f, false, false },
    // enum:SoftHardSpec
    { "EQ_Post_CutLo_Mode", ParamGroup::EQ, 0.0f, 1.0f, 0.0f, false, true },
    // enum:SoftHardSpec
    { "EQ_Post_CutHi_Mode", ParamGroup::EQ, 0.0f, 1.0f, 0.0f, false, true },
    // bool
    { "EQ_Post_CutHard_Lo_Enable", ParamGroup::EQ, 0.0f, 1.0f, 0.0f, true, false },
    // bool
    { "EQ_Post_CutSoft_Lo_Enable", ParamGroup::EQ, 0.0f, 1.0f, 1.0f, true, false },
    // bool
    { "EQ_Post_CutHard_Hi_Enable", ParamGroup::EQ, 0.0f, 1.0f, 0.0f, true, false },
    // bool
    { "EQ_Post_CutSoft_Hi_Enable", ParamGroup::EQ, 0.0f, 1.0f, 1.0f, true, false },
    { "EQ_Post_CutHard_HiQ", ParamGroup::EQ, 0.0f, 1.0f, 0.5f, false, false },
    { "EQ_Post_CutHard_LoQ", ParamGroup::EQ, 0.0f, 1.0f, 0.800000012f, false, false },
    { "EQ_Post_CutHard_HiFreq", ParamGroup::EQ, 0.0f, 1.0f, 1.0f, false, false },
    { "EQ_Post_CutHard_LoFreq", ParamGroup::EQ, 0.0f, 1.0f, 0.0f, false, false },
    { "EQ_Post_CutSoft_HiFreq", ParamGroup::EQ, 0.0f, 1.0f, 1.0f, false, false },
    { "EQ_Post_CutSoft_LoFreq", ParamGroup::EQ, 0.0f, 1.0f, 0.0f, false, false },
    // default=normalizado, default en dominio normalizado y min/max reales
    { "EQ_Post_CutHiFreq", ParamGroup::EQ, 20.0f, 20000.0f, 1.0f, false, false },
    // default=normalizado, default en dominio normalizado y min/max reales
    { "EQ_Post_CutLoFreq", ParamGroup::EQ, 20.0f, 20000.0f, 0.0f, false, false },
    // bool
    { "EQ_Post_Enable", ParamGroup::EQ, 0.0f, 1.0f, 1.0f, true, false },
    // default=normalizado, default en dominio normalizado y min/max reales
    { "Width_Width", ParamGroup::Width, 0.0f, 200.0f, 0.5f, false, false },
    // bool
    { "Width_Enable", ParamGroup::Width, 0.0f, 1.0f, 1.0f, true, false },
    { "InGain", ParamGroup::Levels, 0.0f, 1.0f, 1.0f, false, false },
    { "OutGain", ParamGroup::Levels, 0.0f, 1.0f, 1.0f, false, false },
    // default=real
    { "InLevel", ParamGroup::Levels, 0.125892535f, 7.94328213f, 1.0f, false, false },
    // default=real
    { "OutLevel", ParamGroup::Levels, 0.125892535f, 7.94328213f, 1.0f, false, false },
    // stateVersionSpec
    { "version", ParamGroup::Misc, 0.0f, 1.0f, 1.0f, false, false },
    // stateVersionSpec, valor no numérico en el Lua (index)
    { "savedWith", ParamGroup::Misc, 0.0f, 1.0f, 0.0f, false, false },
};

} // namespace

const ParamSpec* ParamTable::all()
{
    return kParamSpecs;
}

int ParamTable::count()
{
    return static_cast<int>(sizeof(kParamSpecs) / sizeof(kParamSpecs[0]));
}

const ParamSpec* ParamTable::find(const char* id)
{
    if (id == nullptr) {
        return nullptr;
    }
    for (const ParamSpec& p : kParamSpecs) {
        if (std::strcmp(p.id, id) == 0) {
            return &p;
        }
    }
    return nullptr;
}

} // namespace rtxx
