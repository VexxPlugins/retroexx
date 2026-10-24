#pragma once
// Dominio de audio de Retroexx: quantum interno fijo de 64 muestras (observado en El motor:
// buffers de 64 float por canal, constante sr/64 en prepare).
namespace rtxx {

inline constexpr int kQuantum = 64;
inline constexpr int kMaxChannels = 2;

// Vista no propietaria de un buffer multicanal. Nunca asigna.
struct PortData {
    float* ch[kMaxChannels] = {nullptr, nullptr};
    int numChannels = 0;
    int numSamples = 0;
};

} // namespace rtxx
