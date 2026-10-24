// Generado por la herramienta de grafo. No editar a mano: se regenera.
// Contrato: src/block/GraphSpec.h
//
// Los contenedores `getNoiseFXSpec(post)` están expandidos con prefijo propio:
//   NoiseFX.{Noise,SComp,WaveVizOutputNoise} y NoisePostFX.{...}
// Las condiciones de arista se conservan literales (son expresiones del motor).
// Los `config` que son cadenas del DSL plugchain van como referencia simbolica
// (@plugChainDist, @plugchainMasterFilter, @plugchainToneControl): el texto byte a byte
// esta en src/gen/PlugChainText.gen.cpp (kPlugChainDist, ...), no se duplica aqui.
#include "../block/GraphSpec.h"

namespace rtxx {
namespace {

const NodeSpec kNodes[] = {
    { "ToStereo", "cMonoToStereo", "#i" },
    { "AudioPlayer", "cAudioPlayer", "" },
    // post=false
    { "NoiseFX", "cContainer", "" },
    // hijo de NoiseFX
    { "NoiseFX.Noise", "cNoiseFX", "kpId,990102,types,16,driverprio,5" },
    // hijo de NoiseFX
    { "NoiseFX.SComp", "cSimpleComp", "sidechain" },
    // hijo de NoiseFX
    { "NoiseFX.WaveVizOutputNoise", "cAudioWaveViz", "120" },
    // post=true
    { "NoisePostFX", "cContainer", "" },
    // hijo de NoisePostFX
    { "NoisePostFX.Noise", "cNoiseFX", "kpId,990102,types,16,driverprio,0" },
    // hijo de NoisePostFX
    { "NoisePostFX.SComp", "cSimpleComp", "sidechain" },
    // hijo de NoisePostFX
    { "NoisePostFX.WaveVizOutputNoise", "cAudioWaveViz", "120" },
    { "Wobble", "cWobbler", "size,120" },
    { "Magnetic", "cFlutter", "size,120" },
    // config -> kPlugChainDist
    { "Dist", "cPlugChain", "@plugChainDist" },
    { "DistMix", "cGain", "nada" },
    { "Digital", "cBitColor", "" },
    { "Space", "cResoX", "" },
    // config -> kPlugChainMasterFilter
    { "EQ_Post", "cPlugChain", "@plugchainMasterFilter" },
    // config -> kPlugChainToneControl
    { "EQ_Post_Tone", "cPlugChain", "@plugchainToneControl" },
    { "Width", "cStereoWidth", "" },
};

const EdgeSpec kEdges[] = {
    // --- NoiseFX ---
    { "NoiseFX.in", "NoiseFX.Noise.in", "" },
    { "NoiseFX.Noise.out", "NoiseFX.SComp.in", "" },
    { "NoiseFX.in", "NoiseFX.SComp.inSidechain", "" },
    { "NoiseFX.SComp.out", "NoiseFX.WaveVizOutputNoise.in", "Noise_Enable" },
    { "NoiseFX.WaveVizOutputNoise.out", "NoiseFX.out", "" },
    // --- NoisePostFX ---
    { "NoisePostFX.in", "NoisePostFX.Noise.in", "" },
    { "NoisePostFX.Noise.out", "NoisePostFX.SComp.in", "" },
    { "NoisePostFX.in", "NoisePostFX.SComp.inSidechain", "" },
    { "NoisePostFX.SComp.out", "NoisePostFX.WaveVizOutputNoise.in", "Noise_Enable" },
    { "NoisePostFX.WaveVizOutputNoise.out", "NoisePostFX.out", "" },
    // --- spec ---
    { "inAudio", "ToStereo.inMono", "" },
    { "ToStereo.out", "NoiseFX.in", "InLevel" },
    { "NoiseFX.out", "Wobble.in", "!NoisePost" },
    { "Magnetic.out", "NoisePostFX.in", "" },
    { "NoisePostFX.out", "Width.in", "NoisePost" },
    { "ToStereo.out", "Wobble.in", "InLevel" },
    { "Wobble.out", "DistMix.in", "!Dist_MixInternal*Dist_Enable" },
    { "Wobble.out", "Digital.in", "!Dist_Enable" },
    { "Wobble.out", "Dist.in", "Dist_Enable*Dist_InGain*Dist_MixInternal" },
    { "Dist.out", "DistMix.in", "Dist_Enable*Dist_OutGain" },
    { "DistMix.out", "Digital.in", "" },
    { "Digital.out", "Space.in", "" },
    { "Space.out", "EQ_Post.in", "EQ_Post_Enable" },
    { "EQ_Post.out", "EQ_Post_Tone.in", "EQ_Post_Enable" },
    { "Space.out", "Magnetic.in", "!EQ_Post_Enable" },
    { "EQ_Post_Tone.out", "Magnetic.in", "EQ_Post_Tone_Amount" },
    { "Magnetic.out", "Width.in", "" },
    { "Width.out", "outAudio", "OutLevel*OutGainEnable" },
};

} // namespace

const NodeSpec* GraphSpec::nodes()
{
    return kNodes;
}

int GraphSpec::nodeCount()
{
    return static_cast<int>(sizeof(kNodes) / sizeof(kNodes[0]));
}

const EdgeSpec* GraphSpec::edges()
{
    return kEdges;
}

int GraphSpec::edgeCount()
{
    return static_cast<int>(sizeof(kEdges) / sizeof(kEdges[0]));
}

} // namespace rtxx
