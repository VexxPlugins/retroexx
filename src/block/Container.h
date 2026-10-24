#pragma once
#include <atomic>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Block.h"
#include "Buffer.h"
#include "Condition.h"
#include "VizTap.h"

namespace rtxx {

// Contenedor de bloques con puertos direccionados por nombre ("in", "out", "SComp.inSidechain").
//
// Semántica de aristas:
//  - La condición de una arista es un COEFICIENTE, no un booleano:
//      Wobble.out -> Dist.in      con "Dist_Enable*Dist_InGain*Dist_MixInternal"
//      Dist.out   -> DistMix.in   con "Dist_Enable*Dist_OutGain"
//      Wobble.out -> DistMix.in   con "!Dist_MixInternal*Dist_Enable"
//    Las tres se ACUMULAN en el destino, y así sale el crossfade seco/húmedo del módulo Dist.
//  - `!x` = 1 - x (crossfade continuo), porque `Dist_MixInternal` es un mix 0..1.
class Container : public iBlock {
public:
    Container();
    ~Container() override;

    void addBlock(const std::string& name, std::unique_ptr<iBlock> block);
    void addEdge(const std::string& from, const std::string& to, const std::string& condition);

    // Fija el modelo de parámetros usado para evaluar las condiciones.
    void setParams(const ParamSource* p) { params_ = p; }

    // Resuelve y valida el grafo. Devuelve false y llena `error` si algo no cierra.
    bool build(std::string& error);

    void prepare(const PrepareContext& ctx) override;
    void reset() override;
    void process(const PortData* in, PortData& out) override;

    PortData* port(const std::string& name) override;

    // --- introspección para tests ---
    int blockCount() const { return static_cast<int>(nodes_.size()); }
    int edgeCount() const { return static_cast<int>(edges_.size()); }
    const std::vector<std::string>& order() const { return order_; }
    const std::string& buildError() const { return buildError_; }
    // Coeficiente de una arista por índice (útil para testear sin audio).
    float edgeCoefficient(int index) const;

    // --- medidor por nodo (lo usa el editor para los vumetros de cada modulo) ---
    // RMS de la salida del nodo en el último quantum, escrito por el hilo de audio y
    // leído por el de la UI: atómico, sin bloqueos. NO toca el audio.
    float nodeLevel(const std::string& name) const;
    void setMeteringEnabled(bool enabled) { metering_.store(enabled); }
    // Toma de audio para el visualizador de un nodo (despues de build). Solo escribe con el
    // medidor activo; el tap lo posee quien lo pasa y tiene que vivir mas que el contenedor.
    bool setVizTap(const std::string& name, VizTap* tap);

private:
    struct Node {
        std::string name;
        std::unique_ptr<iBlock> block;
        std::map<std::string, Buffer> portBuffers;  // "in", "out", "inSidechain", ...
        std::map<std::string, PortData> portViews;  // vistas vivas durante process()
    };
    struct Edge {
        std::string from;
        std::string to;
        std::string conditionText;
        Condition condition;
        int fromNode = -1;
        std::string fromPort;
        int toNode = -1;  // -1 = el propio contenedor
        std::string toPort;
    };

    std::vector<std::unique_ptr<std::atomic<float>>> levels_;  // RMS por nodo
    std::atomic<bool> metering_{false};
    std::vector<VizTap*> taps_;   // por nodo, nullptr si no hay viz

    int findNode(const std::string& name) const;
    Buffer* resolveBuffer(const std::string& path, int& nodeIndex, std::string& portName);
    void ensurePorts(Node& n);

    std::vector<Node> nodes_;
    std::vector<Edge> edges_;
    std::vector<std::string> order_;
    Buffer in_;
    Buffer out_;
    const ParamSource* params_ = nullptr;
    PrepareContext ctx_;
    std::string buildError_;
    bool built_ = false;
};

} // namespace rtxx
