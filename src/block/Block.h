#pragma once
#include "Port.h"
#include <string>
#include <vector>

namespace rtxx {

struct PrepareContext {
    double sampleRate = 44100.0;
    int maxBlockSize = kQuantum;
};

// Acceso de solo lectura a parámetros por id de modelo ("Noise_Enable", "Dist_Type", ...).
class ParamSource {
public:
    virtual ~ParamSource() = default;
    virtual float get(const char* id) const = 0;
};

// Bloque de DSP. Un puerto se direcciona por nombre; en un contenedor los hijos se
// direccionan "Hijo.puerto". Devolver nullptr significa "no existe ese puerto".
class iBlock {
public:
    virtual ~iBlock() = default;

    virtual void prepare(const PrepareContext&) {}
    virtual void reset() {}

    // `in` puede ser nullptr si el bloque es generador. Debe escribir `out`.
    virtual void process(const PortData* in, PortData& out) = 0;

    // Puerto propio o de un hijo ("out", "SComp.inSidechain", ...).
    virtual PortData* port(const std::string& name) { (void)name; return nullptr; }

    // Puertos adicionales además de "in"/"out" (p. ej. la entrada de sidechain de un
    // compresor). El contenedor crea un buffer por cada uno; lo que no se declare aquí
    // sigue siendo un error de grafo.
    virtual void declarePorts(std::vector<std::string>& ports) const { (void)ports; }

    // El contenedor entrega la vista de un puerto declarado antes de llamar a process()
    // (la vista sólo es válida durante esa llamada).
    virtual void attachPort(const std::string& name, PortData* view) { (void)name; (void)view; }
};

} // namespace rtxx
