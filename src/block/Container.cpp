#include "Container.h"

#include <cmath>

#include <algorithm>
#include <sstream>

namespace rtxx {

namespace {

// "A.puerto" -> node "A", port "puerto"; "puerto" -> contenedor.
void splitPath(const std::string& path, std::string& node, std::string& port) {
    const auto dot = path.find('.');
    if (dot == std::string::npos) {
        node.clear();
        port = path;
    } else {
        node = path.substr(0, dot);
        port = path.substr(dot + 1);
    }
}

} // namespace

Container::Container() = default;
Container::~Container() = default;

void Container::addBlock(const std::string& name, std::unique_ptr<iBlock> block) {
    Node n;
    n.name = name;
    n.block = std::move(block);
    nodes_.push_back(std::move(n));
    built_ = false;
}

void Container::addEdge(const std::string& from, const std::string& to, const std::string& condition) {
    Edge e;
    e.from = from;
    e.to = to;
    e.conditionText = condition;
    e.condition = Condition(condition);
    edges_.push_back(std::move(e));
    built_ = false;
}

int Container::findNode(const std::string& name) const {
    for (size_t i = 0; i < nodes_.size(); ++i)
        if (nodes_[i].name == name) return static_cast<int>(i);
    return -1;
}

void Container::ensurePorts(Node& n) {
    if (n.portBuffers.empty()) {
        n.portBuffers["in"];
        n.portBuffers["out"];
        std::vector<std::string> extra;
        if (n.block) n.block->declarePorts(extra);
        for (const auto& p : extra) n.portBuffers[p];
    }
}

Buffer* Container::resolveBuffer(const std::string& path, int& nodeIndex, std::string& portName) {
    std::string node, port;
    splitPath(path, node, port);
    portName = port;
    if (node.empty()) {
        nodeIndex = -1;
        if (port == "in") return &in_;
        if (port == "out") return &out_;
        return nullptr;
    }
    nodeIndex = findNode(node);
    if (nodeIndex < 0) return nullptr;
    ensurePorts(nodes_[static_cast<size_t>(nodeIndex)]);
    auto& bufs = nodes_[static_cast<size_t>(nodeIndex)].portBuffers;
    auto it = bufs.find(port);
    if (it == bufs.end()) return nullptr;
    return &it->second;
}

bool Container::build(std::string& error) {
    levels_.clear();
    levels_.reserve(nodes_.size());
    for (size_t i = 0; i < nodes_.size(); ++i)
        levels_.push_back(std::make_unique<std::atomic<float>>(0.0f));
    taps_.assign(nodes_.size(), nullptr);
    buildError_.clear();

    // 1. Validar expresiones.
    for (auto& e : edges_) {
        if (!e.condition.valid()) {
            error = "condición inválida en arista " + e.from + " -> " + e.to + ": " + e.condition.error();
            buildError_ = error;
            return false;
        }
    }

    // 2. Resolver extremos.
    for (auto& e : edges_) {
        Buffer* src = resolveBuffer(e.from, e.fromNode, e.fromPort);
        if (!src) {
            error = "origen inexistente: " + e.from;
            buildError_ = error;
            return false;
        }
        Buffer* dst = resolveBuffer(e.to, e.toNode, e.toPort);
        if (!dst) {
            error = "destino inexistente: " + e.to;
            buildError_ = error;
            return false;
        }
    }

    // 3. Orden topológico sobre las aristas entre nodos (Kahn). Un ciclo cae a orden de declaración.
    const int n = static_cast<int>(nodes_.size());
    std::vector<int> indeg(static_cast<size_t>(n), 0);
    std::vector<std::vector<int>> adj(static_cast<size_t>(n));
    for (const auto& e : edges_) {
        if (e.fromNode >= 0 && e.toNode >= 0) {
            adj[static_cast<size_t>(e.fromNode)].push_back(e.toNode);
            ++indeg[static_cast<size_t>(e.toNode)];
        }
    }
    std::vector<int> ready;
    for (int i = 0; i < n; ++i)
        if (indeg[static_cast<size_t>(i)] == 0) ready.push_back(i);
    std::sort(ready.begin(), ready.end());
    order_.clear();
    std::vector<int> queue = ready;
    while (!queue.empty()) {
        const int v = queue.front();
        queue.erase(queue.begin());
        order_.push_back(nodes_[static_cast<size_t>(v)].name);
        for (int w : adj[static_cast<size_t>(v)]) {
            if (--indeg[static_cast<size_t>(w)] == 0) queue.push_back(w);
        }
    }
    if (static_cast<int>(order_.size()) != n) {
        order_.clear();
        for (const auto& nd : nodes_) order_.push_back(nd.name);
        buildError_ = "ciclo en el grafo: se usa el orden de declaración";
    }

    // Los contenedores ANIDADOS también se resuelven aquí: si no, un hijo de un hijo se queda
    // sin `order_` y no procesa nada (su salida es cero). Medido en tests/test_container.cpp.
    for (auto& nd : nodes_) {
        if (auto* child = dynamic_cast<Container*>(nd.block.get())) {
            // El hijo necesita el MODELO: sin él todas sus condiciones valen 0 y no propaga
            // nada (su salida queda a cero). `Engine` sólo llama a setParams en los
            // contenedores de primer nivel.
            child->setParams(params_);
            std::string subError;
            if (!child->build(subError)) {
                error = (nd.name.empty() ? std::string() : nd.name + ": ") + subError;
                buildError_ = error;
                return false;
            }
        }
    }

    built_ = true;
    error = buildError_;
    return true;
}

void Container::prepare(const PrepareContext& ctx) {
    ctx_ = ctx;
    const int chans = 2;
    in_.setChannels(chans);
    out_.setChannels(chans);
    for (auto& nd : nodes_) {
        ensurePorts(nd);
        for (auto& kv : nd.portBuffers) kv.second.setChannels(chans);
        nd.block->prepare(ctx);
    }
}

void Container::reset() {
    in_.clear();
    out_.clear();
    for (auto& nd : nodes_) {
        for (auto& kv : nd.portBuffers) kv.second.clear();
        nd.block->reset();
    }
}

PortData* Container::port(const std::string& name) {
    // Devuelve vistas persistentes sólo para "in"/"out"; los puertos internos se resuelven en process.
    static thread_local PortData view;
    if (name == "in") {
        view = in_.view(kQuantum);
        return &view;
    }
    if (name == "out") {
        view = out_.view(kQuantum);
        return &view;
    }
    return nullptr;
}

float Container::nodeLevel(const std::string& name) const {
    const int idx = findNode(name);
    if (idx < 0 || static_cast<size_t>(idx) >= levels_.size()) return 0.0f;
    return levels_[static_cast<size_t>(idx)]->load(std::memory_order_relaxed);
}

bool Container::setVizTap(const std::string& name, VizTap* tap) {
    const int idx = findNode(name);
    if (idx < 0 || static_cast<size_t>(idx) >= taps_.size()) return false;
    taps_[static_cast<size_t>(idx)] = tap;
    return true;
}

float Container::edgeCoefficient(int index) const {
    if (index < 0 || index >= static_cast<int>(edges_.size())) return 0.0f;
    if (!params_) return 0.0f;
    return edges_[static_cast<size_t>(index)].condition.evaluate(*params_);
}

void Container::process(const PortData* in, PortData& out) {
    const int n = (in && in->numSamples > 0) ? in->numSamples : kQuantum;

    // Entrada del contenedor.
    if (in) {
        const int chans = in->numChannels < kMaxChannels ? in->numChannels : kMaxChannels;
        in_.setChannels(chans);
        for (int c = 0; c < chans; ++c)
            for (int i = 0; i < n; ++i) in_.channel(c)[i] = in->ch[c][i];
    } else {
        in_.clearUsed(n);
    }
    out_.clearUsed(n);

    // Limpiar entradas de los bloques: las aristas acumulan (crossfade).
    for (auto& nd : nodes_)
        for (auto& kv : nd.portBuffers)
            if (kv.first != "out") kv.second.clearUsed(n);

    auto applyIncomingEdges = [&](int nodeIndex) {
        for (auto& e : edges_) {
            // Aristas hacia el contenedor se aplican al final; hacia un nodo, antes de ejecutarlo.
            if (e.toNode == nodeIndex && nodeIndex >= 0) {
                Buffer* src = resolveBuffer(e.from, e.fromNode, e.fromPort);
                Buffer* dst = resolveBuffer(e.to, e.toNode, e.toPort);
                const float c = params_ ? e.condition.evaluate(*params_) : 0.0f;
                dst->accumulateFrom(*src, c, n);
            }
        }
    };

    for (size_t oi = 0; oi < order_.size(); ++oi) {
        const int idx = findNode(order_[oi]);
        if (idx < 0) continue;
        applyIncomingEdges(idx);
        Node& nd = nodes_[static_cast<size_t>(idx)];
        PortData inView = nd.portBuffers["in"].view(n);
        PortData outView = nd.portBuffers["out"].view(n);
        // Puertos internos declarados ("inSidechain"): el bloque recibe la vista antes de
        // procesar (la vista vive en `portViews`, así sigue válida durante process()).
        for (auto& kv : nd.portBuffers) {
            if (kv.first == "in" || kv.first == "out") continue;
            nd.portViews[kv.first] = kv.second.view(n);
            nd.block->attachPort(kv.first, &nd.portViews[kv.first]);
        }
        nd.block->process(&inView, outView);

        // Vumetro: RMS de la salida del nodo en este quantum. Barato (64 muestras por
        // canal) y no influye en el audio; solo se calcula si alguien mira.
        if (metering_.load(std::memory_order_relaxed) &&
            static_cast<size_t>(idx) < levels_.size()) {
            const int chans = outView.numChannels > 0 ? outView.numChannels : 1;
            double sum = 0.0;
            for (int c = 0; c < chans; ++c)
                for (int i = 0; i < n; ++i) sum += (double)outView.ch[c][i] * (double)outView.ch[c][i];
            const float rms = static_cast<float>(std::sqrt(sum / (double)(chans * n)));
            levels_[static_cast<size_t>(idx)]->store(rms, std::memory_order_relaxed);

            // Trama del visualizador: min/max y RMS por canal de la salida, RMS de la
            // entrada. Mono se copia a los dos canales.
            if (VizTap* tap = taps_[static_cast<size_t>(idx)]) {
                VizFrame frame{};
                for (int c = 0; c < 2; ++c) {
                    const float* o = outView.ch[c < outView.numChannels ? c : 0];
                    const float* x = inView.ch[c < inView.numChannels ? c : 0];
                    float lo = 0.0f, hi = 0.0f;
                    double so = 0.0, si = 0.0;
                    for (int i = 0; i < n; ++i) {
                        lo = std::min(lo, o[i]);
                        hi = std::max(hi, o[i]);
                        so += (double)o[i] * (double)o[i];
                        if (inView.numChannels > 0) si += (double)x[i] * (double)x[i];
                    }
                    frame.outMin[c] = lo;
                    frame.outMax[c] = hi;
                    frame.outRms[c] = static_cast<float>(std::sqrt(so / (double)n));
                    frame.inRms[c] = static_cast<float>(std::sqrt(si / (double)n));
                }
                tap->push(frame);
            }
        }
    }

    // Aristas hacia el puerto de salida del contenedor.
    for (auto& e : edges_) {
        if (e.toNode == -1) {
            Buffer* src = resolveBuffer(e.from, e.fromNode, e.fromPort);
            Buffer* dst = resolveBuffer(e.to, e.toNode, e.toPort);
            const float c = params_ ? e.condition.evaluate(*params_) : 0.0f;
            dst->accumulateFrom(*src, c, n);
        }
    }

    out.numChannels = out_.channels();
    out.numSamples = n;
    // COPIA, no reasignación de punteros: `out` es la vista del buffer del padre (o de un
    // contenedor que nos contiene) y reasignar `out.ch[c]` sólo cambiaba la vista local, así
    // que la salida de un contenedor ANIDADO se perdía. Medido: el contenedor `NoiseFX`
    // generaba ruido (pico 0,27 en su `out_`) y en la raíz su puerto salía a cero.
    for (int c = 0; c < out.numChannels; ++c) {
        if (out.ch[c] == out_.channel(c)) continue;   // la vista ya apunta al mismo buffer
        for (int i = 0; i < n; ++i) out.ch[c][i] = out_.channel(c)[i];
    }
}

} // namespace rtxx
