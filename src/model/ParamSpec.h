#pragma once
// Tabla de parámetros. Fichero generado por la herramienta de generación de parámetros;
// no editar a mano.

namespace rtxx {

// Grupo del modelo: los prefijos de Config.configParams más los macros.
enum class ParamGroup : int {
    Noise, SComp, Wobble, Dist, Digital, Space, Magnetic, EQ, Width,
    Levels, WaveViz, Misc, Macro, Flux
};

struct ParamSpec {
    const char* id;      // "Digital_BitDepth"
    ParamGroup group;
    float minValue;
    float maxValue;
    float defaultValue;
    bool isBool;         // se comporta como interruptor (0/1)
    bool isEnum;         // entero con etiquetas en el enum correspondiente
};

struct ParamTable {
    static const ParamSpec* all();
    static int count();
    static const ParamSpec* find(const char* id);
};

} // namespace rtxx
