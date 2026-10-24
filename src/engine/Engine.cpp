#include "engine/Engine.h"

#include <algorithm>
#include <cstring>
#include <map>

#include "block/GraphSpec.h"
#include "blocks/BitColor.h"
#include "blocks/Flutter.h"
#include "blocks/Identity.h"
#include "blocks/Noise.h"
#include "blocks/PlugChainBlock.h"
#include "blocks/SimpleComp.h"
#include "blocks/ResoX.h"
#include "blocks/StereoWidth.h"
#include "blocks/Wobbler.h"
#include "block/PlugChainText.h"

namespace rtxx {
namespace {

// Nombres de puerto del Lua que nuestro framework escribe distinto. Se traducen uno a
// uno, con la razón declarada; no hay traducción implícita de nada más.
std::string normalizePortPath(const std::string& path) {
    if (path == "inAudio") return "in";    // puerto de entrada del contenedor raíz
    if (path == "outAudio") return "out";  // puerto de salida del contenedor raíz
    // cMonoToStereo recibe por "inMono" en la tabla; nuestro bloque tiene
    // una sola entrada mono, que es la misma cosa.
    static const std::string kMonoIn = ".inMono";
    if (path.size() > kMonoIn.size() &&
        path.compare(path.size() - kMonoIn.size(), kMonoIn.size(), kMonoIn) == 0)
        return path.substr(0, path.size() - kMonoIn.size()) + ".in";
    return path;
}

std::string stripOwner(const std::string& owner, const std::string& path) {
    if (owner.empty()) return normalizePortPath(path);
    const std::string prefix = owner + ".";
    if (path.compare(0, prefix.size(), prefix) == 0) return path.substr(prefix.size());
    return normalizePortPath(path);
}

} // namespace

Engine::Engine(ParamStore& params) : params_(params) {}
Engine::~Engine() = default;

std::unique_ptr<iBlock> Engine::makeBlock(const std::string& className, const std::string& config) {
    // Marca de implementación: cada fase añade su DSP propio. Fase 4: modulación (cWobbler
    // de retardo y cFlutter de amplitud, con las ecuaciones cerradas de
    //
    if (className == "cStereoWidth") return std::make_unique<StereoWidthBlock>(&params_);
    if (className == "cWobbler") return std::make_unique<WobblerBlock>(&params_);
    if (className == "cFlutter") return std::make_unique<FlutterBlock>(&params_);
    if (className == "cBitColor") return std::make_unique<BitColorBlock>(&params_);
    if (className == "cResoX") return std::make_unique<ResoXBlock>(&params_);
    if (className == "cNoiseFX") return std::make_unique<NoiseBlock>(&params_);
    if (className == "cSimpleComp") return std::make_unique<SimpleCompBlock>(&params_);
    // cPlugChain: la cadena concreta la dice el `config` del nodo. Las de filtros y la de
    // Dist (con las etapas `distort`/`magicDistort` de la fase 3) se interpretan con el
    // mismo intérprete del DSL; el prefijo es el que usa el DSL para nombrar los parámetros.
    if (className == "cPlugChain") {
        if (config == "@plugchainMasterFilter")
            return std::make_unique<PlugChainBlock>(&params_, kPlugChainMasterFilter, "EQ_Post_");
        if (config == "@plugchainToneControl")
            return std::make_unique<PlugChainBlock>(&params_, kPlugChainToneControl, "EQ_Post_Tone_");
        if (config == "@plugChainDist")
            return std::make_unique<PlugChainBlock>(&params_, kPlugChainDist, "Dist_");
        return std::make_unique<IdentityBlock>();
    }
    // cSimpleComp recibe la señal del contenedor por `inSidechain` (config "sidechain"
    // en la tabla), además de la entrada normal.
    if (className == "cSimpleComp") return std::make_unique<IdentityBlock>(std::initializer_list<const char*>{"inSidechain"});
    return std::make_unique<IdentityBlock>();
}

bool Engine::build(std::string& error) {
    root_ = std::make_unique<Container>();
    root_->setParams(&params_);
    nodeNames_.clear();
    blockStatus_.clear();

    const NodeSpec* nodes = GraphSpec::nodes();
    const int nodeCount = GraphSpec::nodeCount();

    std::map<std::string, std::string> parentOf;  // nodo -> contenedor padre ("" = raíz)
    for (int i = 0; i < nodeCount; ++i) {
        const std::string name = nodes[i].name;
        const auto dot = name.find('.');
        parentOf[name] = dot == std::string::npos ? std::string() : name.substr(0, dot);
    }

    containers_.clear();
    // Dentro de un contenedor el bloque se llama por su nombre local, sin el prefijo del
    // padre: "NoiseFX.Noise" es el bloque "Noise" del contenedor "NoiseFX".
    auto localName = [&](const std::string& name) -> std::string {
        const std::string& p = parentOf[name];
        return p.empty() ? name : name.substr(p.size() + 1);
    };

    // 1. Sub-contenedores primero: hacen falta como destino de sus hijos.
    for (int i = 0; i < nodeCount; ++i) {
        const NodeSpec& n = nodes[i];
        const std::string name = n.name;
        if (std::string(n.className) != "cContainer") continue;
        Container* owner = parentOf[name].empty() ? root_.get() : containers_[parentOf[name]];
        if (!owner) {
            error = "contenedor padre inexistente para " + name;
            return false;
        }
        auto c = std::make_unique<Container>();
        c->setParams(&params_);
        containers_[name] = c.get();
        owner->addBlock(localName(name), std::move(c));
        nodeNames_.push_back(name);
    }

    // 2. Bloques hoja.
    for (int i = 0; i < nodeCount; ++i) {
        const NodeSpec& n = nodes[i];
        const std::string name = n.name;
        if (std::string(n.className) == "cContainer") continue;
        Container* owner = parentOf[name].empty() ? root_.get() : containers_[parentOf[name]];
        if (!owner) {
            error = "contenedor padre inexistente para " + name;
            return false;
        }
        const std::string cls = n.className;
        const std::string cfg = n.config ? n.config : "";
        owner->addBlock(localName(name), makeBlock(cls, cfg));
        bool implemented = (cls == "cStereoWidth" || cls == "cWobbler" || cls == "cFlutter" ||
                            cls == "cBitColor" || cls == "cResoX" ||
                            cls == "cNoiseFX" || cls == "cSimpleComp");
        if (cls == "cPlugChain" && (cfg == "@plugchainMasterFilter" || cfg == "@plugchainToneControl" ||
                                     cfg == "@plugChainDist"))
            implemented = true;
        blockStatus_.push_back({name, cls, implemented});
        nodeNames_.push_back(name);
    }

    // 3. Aristas. Cada una se registra en el contenedor que posee a sus dos extremos.
    const EdgeSpec* edges = GraphSpec::edges();
    for (int i = 0; i < GraphSpec::edgeCount(); ++i) {
        const std::string from = edges[i].from;
        const std::string to = edges[i].to;
        const std::string cond = edges[i].condition ? edges[i].condition : "";

        // El nodo es todo lo que hay antes del ÚLTIMO punto (el puerto es el último
        // segmento); "NoiseFX.Noise.in" es el nodo "NoiseFX.Noise", no "NoiseFX".
        auto nodeOf = [](const std::string& path) -> std::string {
            const auto dot = path.rfind('.');
            return dot == std::string::npos ? std::string() : path.substr(0, dot);
        };
        const std::string fromNode = nodeOf(from);
        const std::string toNode = nodeOf(to);
        const std::string ownerFrom = fromNode.empty() ? std::string() : parentOf[fromNode];
        const std::string ownerTo = toNode.empty() ? std::string() : parentOf[toNode];

        // El contenedor de la arista es el más profundo de los dos extremos: una arista
        // que baja de un contenedor a su hijo se registra en el hijo.
        std::string owner;
        if (ownerFrom == ownerTo)
            owner = ownerFrom;
        else
            owner = ownerFrom.empty() ? ownerTo : (ownerTo.empty() ? ownerFrom : ownerTo);

        Container* target = owner.empty() ? root_.get() : containers_[owner];
        if (!target) {
            error = "contenedor inexistente para la arista " + from + " -> " + to;
            return false;
        }
        target->addEdge(stripOwner(owner, from), stripOwner(owner, to), cond);
    }

    // 4. Resolver y validar.
    if (!root_->build(error)) return false;
    for (auto& kv : containers_) {
        std::string subError;
        if (!kv.second->build(subError)) {
            error = "en " + kv.first + ": " + subError;
            return false;
        }
    }
    // Tomas de los visualizadores por audio (mismos nodos que moduleLevel).
    root_->setVizTap("NoiseFX", &noiseTap_);
    root_->setVizTap("Space", &spaceTap_);
    root_->setVizTap("Magnetic", &magneticTap_);

    built_ = true;
    return true;
}

float Engine::moduleLevel(int moduleIndex) const {
    // Orden real del audio (HostModules.h): el mismo que dibuja el editor de izquierda a
    // derecha, asi que el indice del panel es el indice de la cadena.
    // Nodo que representa a cada modulo del editor. Dist es `DistMix` (la salida del
    // crossfade seco/humedo, no la rama saturada) y EQ es `EQ_Post_Tone`, el ultimo
    // eslabon del modulo (filtros + tono).
    static const char* kNodes[] = {"NoiseFX", "Wobble",  "DistMix",  "Digital",
                                   "Space",   "EQ_Post_Tone", "Magnetic", "Width"};
    if (moduleIndex < 0 || moduleIndex >= 8 || root_ == nullptr) return 0.0f;
    return root_->nodeLevel(kNodes[moduleIndex]);
}

VizTap* Engine::vizTap(int moduleIndex) {
    switch (moduleIndex) {
        case 0: return &noiseTap_;
        case 4: return &spaceTap_;
        case 6: return &magneticTap_;
        default: return nullptr;
    }
}

void Engine::setMeteringEnabled(bool enabled) {
    if (root_) root_->setMeteringEnabled(enabled);
}

int Engine::edgeCount() const {
    int total = root_ ? root_->edgeCount() : 0;
    for (const auto& kv : containers_) total += kv.second->edgeCount();
    return total;
}

void Engine::prepare(double sampleRate, int maxBlockSize) {
    PrepareContext ctx;
    ctx.sampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    ctx.maxBlockSize = maxBlockSize > 0 ? maxBlockSize : kQuantum;
    root_->prepare(ctx);
    reset();
    prepared_ = true;
}

void Engine::reset() {
    root_->reset();
    std::memset(acc_, 0, sizeof(acc_));
    std::memset(fifo_, 0, sizeof(fifo_));
    accFill_ = 0;
    fifoRead_ = 0;
    // La latencia es un quantum de ceros al principio: eso es exactamente lo que
    // reporta el motor (64 muestras).
    fifoCount_ = kQuantum;
}

void Engine::pushToFifo(const float* const* src, int n) {
    for (int c = 0; c < kMaxChannels; ++c) {
        int w = (fifoRead_ + fifoCount_) % kFifoCapacity;
        for (int i = 0; i < n; ++i) {
            fifo_[c][w] = src[c] ? src[c][i] : 0.0f;
            w = (w + 1) % kFifoCapacity;
        }
    }
    fifoCount_ += n;
}

void Engine::popFromFifo(float* const* dst, int n) {
    for (int i = 0; i < n; ++i) {
        for (int c = 0; c < kMaxChannels; ++c) dst[c][i] = fifo_[c][fifoRead_];
        fifoRead_ = (fifoRead_ + 1) % kFifoCapacity;
    }
    fifoCount_ -= n;
}

void Engine::runQuantum() {
    PortData in;
    in.numChannels = 2;
    in.numSamples = kQuantum;
    for (int c = 0; c < kMaxChannels; ++c) in.ch[c] = acc_[c];

    for (int c = 0; c < kMaxChannels; ++c) std::memset(cur_[c], 0, sizeof(cur_[c]));
    PortData outView;
    outView.numChannels = 2;
    outView.numSamples = kQuantum;
    for (int c = 0; c < kMaxChannels; ++c) outView.ch[c] = cur_[c];

    root_->process(&in, outView);

    const float* src[kMaxChannels] = {outView.ch[0], outView.ch[1]};
    pushToFifo(src, kQuantum);
}

void Engine::process(const PortData& in, PortData& out, int numSamples) {
    if (!prepared_ || numSamples <= 0) return;
    float* dst[kMaxChannels] = {out.ch[0], out.ch[1]};

    int i = 0;
    while (i < numSamples) {
        const int take = std::min(kQuantum - accFill_, numSamples - i);
        for (int c = 0; c < kMaxChannels; ++c) {
            const float* s = (c < in.numChannels && in.ch[c]) ? in.ch[c] + i : nullptr;
            for (int k = 0; k < take; ++k) acc_[c][accFill_ + k] = s ? s[k] : 0.0f;
        }
        accFill_ += take;
        if (accFill_ == kQuantum) {
            accFill_ = 0;
            runQuantum();
        }
        float* target[kMaxChannels] = {dst[0] + i, dst[1] + i};
        popFromFifo(target, take);
        i += take;
    }
    out.numChannels = 2;
    out.numSamples = numSamples;
}

} // namespace rtxx
