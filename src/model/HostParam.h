#pragma once
// Superficie de automatización del plugin: los 58 parámetros que el host ve.
//
// Es la superficie del host, medida y congelada en `HostParams.gen.cpp`. El modelo interno
// tiene
// 125 parámetros (src/gen/ParamTable.gen.cpp); esta tabla es el subconjunto que se
// expone al host y el mapeo al id de modelo correspondiente.

namespace rtxx {

struct HostParamSpec {
unsigned flags; // flags del motor (AUParameterInfo.flags)
int id; // id estable 0..57
    const char* name;      // nombre visible ("MAST In Gain")
    const char* modelId;   // id de modelo ("InLevel_Unscaled")
    float min;
    float max;
    float defaultValue; // valor normalizado por defecto medido en el motor
    bool isEnum;
};

struct HostParamTable {
    static const HostParamSpec* all();
    static int count();
};

} // namespace rtxx
