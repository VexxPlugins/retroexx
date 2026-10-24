#include "io/WavFile.h"

#include <cstdint>
#include <cstring>
#include <fstream>

namespace rtxx {
namespace {

uint32_t le32(const unsigned char* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}
uint16_t le16(const unsigned char* p) {
    return static_cast<uint16_t>(p[0] | (p[1] << 8));
}

void put32(std::vector<unsigned char>& v, uint32_t x) {
    v.push_back(static_cast<unsigned char>(x & 0xff));
    v.push_back(static_cast<unsigned char>((x >> 8) & 0xff));
    v.push_back(static_cast<unsigned char>((x >> 16) & 0xff));
    v.push_back(static_cast<unsigned char>((x >> 24) & 0xff));
}
void put16(std::vector<unsigned char>& v, uint16_t x) {
    v.push_back(static_cast<unsigned char>(x & 0xff));
    v.push_back(static_cast<unsigned char>((x >> 8) & 0xff));
}

} // namespace

bool readWav(const std::string& path, AudioFile& out, std::string& error) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        error = "no se puede abrir " + path;
        return false;
    }
    std::vector<unsigned char> buf((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (buf.size() < 44 || std::memcmp(buf.data(), "RIFF", 4) != 0 || std::memcmp(buf.data() + 8, "WAVE", 4) != 0) {
        error = "no es un WAV RIFF: " + path;
        return false;
    }
    int format = 0, channels = 0, bits = 0;
    int sampleRate = 0;
    size_t pos = 12;
    const unsigned char* data = nullptr;
    size_t dataBytes = 0;
    while (pos + 8 <= buf.size()) {
        const char* id = reinterpret_cast<const char*>(buf.data() + pos);
        const uint32_t size = le32(buf.data() + pos + 4);
        const size_t body = pos + 8;
        if (body + size > buf.size()) break;
        if (std::memcmp(id, "fmt ", 4) == 0 && size >= 16) {
            format = le16(buf.data() + body);
            channels = le16(buf.data() + body + 2);
            sampleRate = static_cast<int>(le32(buf.data() + body + 4));
            bits = le16(buf.data() + body + 14);
            if (format == 0xFFFE && size >= 40) format = le16(buf.data() + body + 24);  // WAVE_FORMAT_EXTENSIBLE
        } else if (std::memcmp(id, "data", 4) == 0) {
            data = buf.data() + body;
            dataBytes = size;
        }
        pos = body + size + (size & 1);
    }
    if (!data || channels <= 0) {
        error = "WAV sin bloque data/fmt válido: " + path;
        return false;
    }
    out.channels = channels;
    out.sampleRate = sampleRate;
    const int bytesPerSample = bits / 8;
    if (bytesPerSample <= 0) {
        error = "WAV con bits por muestra inválidos";
        return false;
    }
    out.frames = static_cast<int>(dataBytes / (static_cast<size_t>(bytesPerSample) * static_cast<size_t>(channels)));
    out.samples.assign(static_cast<size_t>(out.frames) * static_cast<size_t>(channels), 0.0f);

    if (format == 3 && bits == 32) {
        std::memcpy(out.samples.data(), data, static_cast<size_t>(out.frames * channels) * 4);
    } else if (format == 1 && bits == 16) {
        const int16_t* s = reinterpret_cast<const int16_t*>(data);
        for (size_t i = 0; i < out.samples.size(); ++i) out.samples[i] = static_cast<float>(s[i]) / 32768.0f;
    } else if (format == 1 && bits == 24) {
        for (size_t i = 0; i < out.samples.size(); ++i) {
            const unsigned char* p = data + i * 3;
            int32_t v = (static_cast<int32_t>(p[2]) << 24) | (static_cast<int32_t>(p[1]) << 16) |
                        (static_cast<int32_t>(p[0]) << 8);
            out.samples[i] = static_cast<float>(v) / 2147483648.0f;
        }
    } else {
        error = "formato WAV no soportado (format=" + std::to_string(format) +
                ", bits=" + std::to_string(bits) + "): usa float32 o PCM16/24";
        return false;
    }
    return true;
}

bool writeWavF32(const std::string& path, const float* interleaved, int frames, int channels,
                 int sampleRate, std::string& error) {
    const uint32_t dataBytes = static_cast<uint32_t>(frames) * static_cast<uint32_t>(channels) * 4u;
    std::vector<unsigned char> hdr;
    hdr.reserve(44);
    hdr.insert(hdr.end(), {'R', 'I', 'F', 'F'});
    put32(hdr, 36u + dataBytes);
    hdr.insert(hdr.end(), {'W', 'A', 'V', 'E', 'f', 'm', 't', ' '});
    put32(hdr, 16);
    put16(hdr, 3);  // IEEE float
    put16(hdr, static_cast<uint16_t>(channels));
    put32(hdr, static_cast<uint32_t>(sampleRate));
    put32(hdr, static_cast<uint32_t>(sampleRate * channels * 4));
    put16(hdr, static_cast<uint16_t>(channels * 4));
    put16(hdr, 32);
    hdr.insert(hdr.end(), {'d', 'a', 't', 'a'});
    put32(hdr, dataBytes);

    std::ofstream f(path, std::ios::binary);
    if (!f) {
        error = "no se puede escribir " + path;
        return false;
    }
    f.write(reinterpret_cast<const char*>(hdr.data()), static_cast<std::streamsize>(hdr.size()));
    f.write(reinterpret_cast<const char*>(interleaved), static_cast<std::streamsize>(dataBytes));
    if (!f) {
        error = "fallo al escribir " + path;
        return false;
    }
    return true;
}

void extractChannel(const AudioFile& f, int ch, std::vector<float>& dst) {
    dst.resize(static_cast<size_t>(f.frames));
    for (int i = 0; i < f.frames; ++i)
        dst[static_cast<size_t>(i)] = f.samples[static_cast<size_t>(i) * static_cast<size_t>(f.channels) +
                                                              static_cast<size_t>(ch)];
}

} // namespace rtxx
