#pragma once
#include <cstring>

#include "Port.h"

namespace rtxx {

// Buffer interno de un puerto: kMaxChannels x kQuantum. Propietario, sin asignación dinámica.
class Buffer {
public:
    void setChannels(int n) {
        numChannels = n < 1 ? 1 : (n > kMaxChannels ? kMaxChannels : n);
        clear();
    }
    void clear() {
        for (int c = 0; c < kMaxChannels; ++c) std::memset(data[c], 0, sizeof(data[c]));
    }
    void clearUsed(int n) {
        for (int c = 0; c < numChannels; ++c) std::memset(data[c], 0, sizeof(float) * static_cast<size_t>(n));
    }

    PortData view(int n) {
        PortData p;
        p.numChannels = numChannels;
        p.numSamples = n;
        for (int c = 0; c < kMaxChannels; ++c) p.ch[c] = data[c];
        return p;
    }
    PortData const_view(int n) const {
        PortData p;
        p.numChannels = numChannels;
        p.numSamples = n;
        for (int c = 0; c < kMaxChannels; ++c) p.ch[c] = const_cast<float*>(data[c]);
        return p;
    }

    // dst += gain * src, sobre `n` muestras y los canales activos de dst.
    void accumulateFrom(const Buffer& src, float gain, int n) {
        if (gain == 0.0f) return;
        const int chans = numChannels < src.numChannels ? numChannels : src.numChannels;
        for (int c = 0; c < chans; ++c) {
            const float* s = src.data[c];
            float* d = data[c];
            for (int i = 0; i < n; ++i) d[i] += gain * s[i];
        }
    }

    float* channel(int c) { return data[c]; }
    const float* channel(int c) const { return data[c]; }
    int channels() const { return numChannels; }

    int numChannels = 1;

private:
    float data[kMaxChannels][kQuantum] = {};
};

} // namespace rtxx
