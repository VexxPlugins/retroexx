// Tabla de la superficie del host. Fichero generado: no editar a mano, se regenera.
//
// La ley de escala de cada entrada NO está aquí: ver ParamStore.cpp.
#include "model/HostParam.h"

namespace rtxx {
namespace {

const HostParamSpec kHostParams[] = {
    {0xca200010u,  0, "Magnitude", "MasterMacroScaler", 0.0f, 1.0f, 1.0f, false},
    {0xca200010u,  1, "NOIS Amount", "MacroNoiseAmount", 0.0f, 1.0f, 0.252380967f, false},
    {0xca200010u,  2, "WOBB Amount", "MacroWobbleAmount", 0.0f, 1.0f, 0.101904765f, false},
    {0xca200010u,  3, "DIST Amount", "MacroDistAmount", 0.0f, 1.0f, 0.290476203f, false},
    {0xca200010u,  4, "DIGI Amount", "MacroDigitalAmount", 0.0f, 1.0f, 0.0f, false},
    {0xca200010u,  5, "SPAC Amount", "MacroSpaceAmount", 0.0f, 1.0f, 0.0f, false},
    {0xca200010u,  6, "MAGN Amount", "MacroMagneticAmount", 0.0f, 1.0f, 0.20238094f, false},
    {0xc8200010u,  7, "NOIS Enable", "MacroNoiseEnable", 0.0f, 1.0f, 1.0f, false},
    {0xc8200010u,  8, "NOIS Type", "Noise_NoiseType", 0.0f, 15.0f, 0.0f, true},
    {0xca200010u,  9, "NOIS Tone", "Noise_Tone_Unscaled", 0.0f, 1.0f, 0.457705259f, false},
    {0xca200010u, 10, "NOIS Follow", "Noise_Follow", 0.0f, 1.0f, 0.0500000007f, false},
    {0xc8200010u, 11, "NOIS Routing", "NoisePost", 0.0f, 1.0f, 1.0f, false},
    {0xca200010u, 12, "NOIS Duck", "SComp_Threshold", 0.0f, 1.0f, 0.299999982f, false},
    {0xca200010u, 13, "NOIS Flux", "Noise_Flux", 0.0f, 1.0f, 0.205384612f, false},
    {0xc8200010u, 14, "WOBB Enable", "MacroWobbleEnable", 0.0f, 1.0f, 1.0f, false},
    {0xca200010u, 15, "WOBB W<>F Balance", "Wobble_Xfade", 0.0f, 1.0f, 0.108695641f, false},
    {0xca200010u, 16, "WOBB Wow Rate", "Wobble_Rate", 0.0f, 1.0f, 0.200000003f, false},
    {0xc8200010u, 17, "WOBB Wow Stereo Mode", "Wobble_Stereo_Enable", 0.0f, 1.0f, 0.0f, false},
    {0xca200010u, 18, "WOBB Flutter Rate", "Wobble_Rate2", 0.0f, 1.0f, 0.289999992f, false},
    {0xca200010u, 19, "WOBB Mix", "Wobble_Mix", 0.0f, 1.0f, 1.0f, false},
    {0xca200010u, 20, "WOBB Flux", "Wobble_Flux", 0.0f, 1.0f, 0.0730769262f, false},
    {0xc8200010u, 21, "DIST Enable", "MacroDistEnable", 0.0f, 1.0f, 1.0f, false},
    {0xc8200010u, 22, "DIST Type", "Dist_Type", 0.0f, 5.0f, 0.0f, false},
    {0xca200010u, 23, "DIST Filter Lo", "Dist_FreqLo", 0.0f, 1.0f, 0.0f, false},
    {0xca200010u, 24, "DIST Filter Hi", "Dist_FreqHi", 0.0f, 1.0f, 1.0f, false},
    {0xca200010u, 25, "DIST Mix", "Dist_Mix", 0.0f, 1.0f, 1.0f, false},
    {0xca200010u, 26, "DIST Flux", "Dist_Flux", 0.0f, 1.0f, 0.0961538479f, false},
    {0xc8200010u, 27, "DIGI Enable", "MacroDigitalEnable", 0.0f, 1.0f, 0.0f, false},
    {0xca200010u, 28, "DIGI R<>B Balance", "Digital_Xfade", 0.0f, 1.0f, 0.5f, false},
    {0xca200010u, 29, "DIGI Filter Lo", "Digital_FilterLo", 0.0f, 1.0f, 0.0f, false},
    {0xca200010u, 30, "DIGI Filter Hi", "Digital_FilterHi", 0.0f, 1.0f, 1.0f, false},
    {0xc8200010u, 31, "DIGI Filter Mode", "Digital_FocusMode", 0.0f, 1.0f, 1.0f, false},
    {0xca200010u, 32, "DIGI Smooth", "Digital_AliasSmoothing", 0.0f, 1.0f, 0.0f, false},
    {0xca200010u, 33, "DIGI Mix", "Digital_Mix", 0.0f, 1.0f, 1.0f, false},
    {0xca200010u, 34, "DIGI Flux", "Digital_Flux", 0.0f, 1.0f, 0.0f, false},
    {0xc8200010u, 35, "SPAC Enable", "MacroSpaceEnable", 0.0f, 1.0f, 0.0f, false},
    {0xca200010u, 36, "SPAC Decay", "Space_Decay", 0.0f, 1.0f, 0.800000012f, false},
    {0xca200010u, 37, "SPAC Filter Lo", "Space_Dampen", 0.0f, 1.0f, 0.0f, false},
    {0xca200010u, 38, "SPAC Filter Hi", "Space_Soften", 0.0f, 1.0f, 1.0f, false},
    {0xca200010u, 39, "SPAC PreDelay", "Space_PreDelay", 0.0f, 1.0f, 0.0f, false},
    {0xc8200010u, 40, "SPAC Stereo Mode", "Space_StereoMode", 0.0f, 1.0f, 1.0f, false},
    {0xca200010u, 41, "SPAC Flux", "Space_Flux", 0.0f, 1.0f, 0.0f, false},
    {0xc8200010u, 42, "MAGN Enable", "MacroMagneticEnable", 0.0f, 1.0f, 1.0f, false},
    {0xca200010u, 43, "MAGN W<>F Balance", "Magnetic_Xfade", 0.0f, 1.0f, 0.0f, false},
    {0xc8200010u, 44, "MAGN Wear Stereo Mode", "Magnetic_WearPanEnable", 0.0f, 1.0f, 1.0f, false},
    {0xca200010u, 45, "MAGN Flutter Rate", "Magnetic_Rate", 0.0f, 1.0f, 0.5f, false},
    {0xca200010u, 46, "MAGN Dropouts", "Magnetic_DropOutsAmount_GUI", 0.0f, 1.0f, 0.0f, false},
    {0xca200010u, 47, "MAGN Flux", "Magnetic_Flux", 0.0f, 1.0f, 0.0f, false},
    {0xca200010u, 48, "MAST In Gain", "InLevel_Unscaled", 0.0f, 1.0f, 0.5f, false},
    {0xc8200010u, 49, "MAST EQ Enable", "EQ_Post_Enable", 0.0f, 1.0f, 1.0f, false},
    {0xca200010u, 50, "MAST EQ Cut Lo", "EQ_Post_CutLoFreq_Unscaled", 0.0f, 1.0f, 0.163885891f, false},
    {0xca200010u, 51, "MAST EQ Cut Hi", "EQ_Post_CutHiFreq_Unscaled", 0.0f, 1.0f, 0.880782008f, false},
    {0xc8200010u, 52, "MAST EQ Cut Lo Mode", "EQ_Post_CutLo_Mode", 0.0f, 1.0f, 0.0f, false},
    {0xc8200010u, 53, "MAST EQ Cut Hi Mode", "EQ_Post_CutHi_Mode", 0.0f, 1.0f, 0.0f, false},
    {0xca200010u, 54, "MAST EQ Tone", "EQ_Post_Tone_Amount_Unscaled", 0.0f, 1.0f, 0.677765846f, false},
    {0xc8200010u, 55, "MAST EQ Tone Mode", "EQ_Post_Tone_Mode", 0.0f, 1.0f, 0.0f, false},
    {0xca200010u, 56, "MAST Out Width", "Width_Width_Unscaled", 0.0f, 1.0f, 0.5f, false},
    {0xca200010u, 57, "MAST Out Gain", "OutLevel_Unscaled", 0.0f, 1.0f, 0.5f, false},
};

} // namespace

const HostParamSpec* HostParamTable::all() { return kHostParams; }
int HostParamTable::count() { return 58; }

} // namespace rtxx
