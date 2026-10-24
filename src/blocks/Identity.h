#pragma once
// Bloques de audio de Retroexx.
//
// Fase 0 del ROADMAP: todos los nodos existen y están conectados, pero sólo son
// identidad. Cada fase sustituye un `IdentityBlock` por su implementación real; el
// progreso se mide con `tools/coverage_blocks.py`, no con impresiones.
#include <cstring>

#include "block/Block.h"

namespace rtxx {

// Copia la entrada a la salida, sin tocar nada. Es el estado "módulo apagado" del
// motor: sus bloques con Enable=0 son passthrough exacto (verificado bit-exacto
// por medida).
class IdentityBlock : public iBlock {
public:
    IdentityBlock() = default;
    explicit IdentityBlock(std::initializer_list<const char*> extraPorts) {
        for (const char* p : extraPorts) extra_.emplace_back(p);
    }

    void declarePorts(std::vector<std::string>& ports) const override {
        for (const auto& p : extra_) ports.push_back(p);
    }

    void process(const PortData* in, PortData& out) override {
        const int n = out.numSamples > 0 ? out.numSamples : (in ? in->numSamples : 0);
        for (int c = 0; c < out.numChannels; ++c) {
            if (in && c < in->numChannels && in->ch[c])
                std::memcpy(out.ch[c], in->ch[c], sizeof(float) * static_cast<size_t>(n));
            else
                std::memset(out.ch[c], 0, sizeof(float) * static_cast<size_t>(n));
        }
    }

private:
    std::vector<std::string> extra_;
};

} // namespace rtxx
