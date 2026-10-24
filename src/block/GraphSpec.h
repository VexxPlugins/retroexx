#pragma once
// Especificación del grafo. Fichero generado por la herramienta de generación del grafo;
// no editar a mano: se regenera.

namespace rtxx {

struct NodeSpec {
    const char* name;       // "Digital"
    const char* className;  // "cBitColor"
    const char* config;     // texto de config opcional ("" si none)
};

struct EdgeSpec {
    const char* from;       // "Wobble.out"
    const char* to;         // "DistMix.in"
    const char* condition;  // "Dist_Enable*Dist_InGain*Dist_MixInternal" ("" = siempre)
};

struct GraphSpec {
    static const NodeSpec* nodes();
    static int nodeCount();
    static const EdgeSpec* edges();
    static int edgeCount();
};

} // namespace rtxx
