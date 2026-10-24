#pragma once
// Motor de audio de Retroexx: el grafo completo del motor, con quantum interno de
// 64 muestras y latencia de un quantum, igual que el motor (`kAudioUnitProperty_Latency`
// = 64/44100 = 0.00145124716553 s, medido).
//
// Lo que el motor garantiza:
//  * El host puede pedir cualquier tamaño de bloque; internamente se acumula a 64.
//  * La salida va exactamente 64 muestras por detrás de la entrada: en bypass el
// resultado es bit-idéntico al motor desplazado 64 (BASELINE §2).
//
// El grafo no se escribe a mano: se instancia desde src/gen/GraphSpec.gen.cpp, que es
// la traducción del la tabla del motor.
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "block/Container.h"
#include "block/Port.h"
#include "model/ParamStore.h"

namespace rtxx {

class Engine {
public:
    explicit Engine(ParamStore& params);
    ~Engine();

    // Instancia los 13 nodos de nivel superior y sus dos sub-contenedores de ruido.
    // Devuelve false y deja `error` con el motivo si el grafo no cierra.
    bool build(std::string& error);

    void prepare(double sampleRate, int maxBlockSize);
    void reset();

    // Procesa `numSamples` del host. `in` y `out` pueden ser el mismo buffer.
    void process(const PortData& in, PortData& out, int numSamples);

    static int latencySamples() { return kQuantum; }

    ParamStore& params() { return params_; }
    Container& graph() { return *root_; }

    // Nombres de los nodos instanciados, en orden de construcción (para el reporte).
    const std::vector<std::string>& nodeNames() const { return nodeNames_; }
    // Aristas totales: las del contenedor raíz más las de los sub-contenedores de ruido.
    int edgeCount() const;

    // Nivel (RMS) de la salida de un modulo, en el orden de audio de `ModuleTable`:
    // 0 NOISE, 1 WOBBLE, 2 DIST, 3 DIGITAL, 4 SPACE, 5 EQ, 6 MAGNETIC, 7 WIDTH.
    float moduleLevel(int moduleIndex) const;
    float outputLevel() const { return root_ ? root_->nodeLevel("Width") : 0.0f; }
    void setMeteringEnabled(bool enabled);
    // Toma de audio del visualizador del modulo (mismo indice que moduleLevel): solo
    // NOISE (0), SPACE (4) y MAGNETIC (6) la tienen; el resto devuelve nullptr.
    VizTap* vizTap(int moduleIndex);

    // Estado de implementación de cada nodo: "implemented" o "identity". Es la fuente
    // de verdad de la cobertura del ROADMAP (no una tabla aparte que pueda mentir).
    struct BlockStatus {
        std::string node;
        std::string className;
        bool implemented;
    };
    const std::vector<BlockStatus>& blockStatus() const { return blockStatus_; }

private:
    std::unique_ptr<iBlock> makeBlock(const std::string& className, const std::string& config);

    void runQuantum();
    void pushToFifo(const float* const* src, int n);
    void popFromFifo(float* const* dst, int n);

    ParamStore& params_;
    std::unique_ptr<Container> root_;
    std::map<std::string, Container*> containers_;  // sub-contenedores por nombre de nodo
    std::vector<std::string> nodeNames_;
    std::vector<BlockStatus> blockStatus_;
    bool built_ = false;

    // --- pipeline de 64 muestras ---
    float acc_[kMaxChannels][kQuantum] = {};
    int accFill_ = 0;
    float cur_[kMaxChannels][kQuantum] = {};
    // Cola de salida: arranca con 64 ceros, que son la latencia del plugin.
    static constexpr int kFifoCapacity = 4 * kQuantum;
    float fifo_[kMaxChannels][kFifoCapacity] = {};
    int fifoRead_ = 0;
    int fifoCount_ = 0;
    bool prepared_ = false;

    VizTap noiseTap_, spaceTap_, magneticTap_;
};

} // namespace rtxx
