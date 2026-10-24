#pragma once
// Lectura/escritura de WAV sin dependencias. Sólo lo que necesita la herramienta de
// medida: float32 IEEE y PCM16 (formato 1) de entrada.
#include <string>
#include <vector>

namespace rtxx {

struct AudioFile {
    std::vector<float> samples;  // entrelazado
    int channels = 0;
    int sampleRate = 0;
    int frames = 0;
};

bool readWav(const std::string& path, AudioFile& out, std::string& error);
bool writeWavF32(const std::string& path, const float* interleaved, int frames, int channels,
                 int sampleRate, std::string& error);

// Deja `frames` muestras del canal `ch` de un AudioFile en `dst`.
void extractChannel(const AudioFile& f, int ch, std::vector<float>& dst);

} // namespace rtxx
