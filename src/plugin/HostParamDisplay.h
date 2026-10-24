#pragma once
// Presentacion de los 58 parametros del host: etiqueta corta y texto de valor.
//
// La regla es la de DESIGN.md §5 (valores numericos visibles en dB, Hz, % y ms) SIN
// inventar unidades. Solo se muestra una unidad fisica cuando la ley esta medida y
// verificada en src/model/ParamStore.cpp; el resto se muestra como porcentaje del valor
// normalizado del host, que es lo que el motor expone realmente (la tabla del host:
// value_strings_available = false, El motor tampoco publica textos de valor).
//
// Este texto es tambien el que devuelve AudioProcessorParameter::getText, asi que el
// host ve exactamente lo mismo que el editor: no hay dos formatos que puedan divergir.
#include <juce_core/juce_core.h>

#include "model/HostParam.h"

namespace rtxx {

enum class ParamUnit {
    Percent,       // valor normalizado del host: 0..100 %
    Db36,          // dB = (v - 0.5) * 36        (In/Out Gain, ley de la fase 1)
    Hertz,         // Hz = 20 * 1000^v            (Cut Lo/Hi, ley de la fase 2)
    WidthPercent,  // % = v * 200                 (Width, ley de la fase 1)
    TypeIndex      // indice entero del enum del host (NOIS Type 0..15, DIST Type 0..5)
};

// Nombre visible sin el prefijo del modulo: "MAST In Gain" -> "In Gain".
juce::String hostParamShortName(int hostId);

ParamUnit hostParamUnit(int hostId);

// Texto del valor a partir del valor NORMALIZADO (0..1) del host.
juce::String hostParamValueText(int hostId, float normalised);

// Numero de opciones de un parametro discreto (0 si no lo es).
int hostParamChoiceCount(int hostId);

// Nombre de la opcion `index` de un discreto, en el ORDEN DEL HOST, medido del motor:
//   host 8  (NOISE Type): VINYL 1, VINYL 2, TAPE 7.5, TAPE 15, TUBE, DC, MUFF, RADIO, VHS,
//                         CASS, 8-BIT, WHITE, PINK, STUDIO 1, STUDIO 2, APOLLO
//   host 22 (DIST Type) : TUBEPAIR, IRON, CRUNCH, HEAVY, ZAP, AIR
juce::String hostParamChoiceName(int hostId, int index);

// Inversa de hostParamValueText, para que un host que permita escribir el valor
// (kAudioUnitProperty_ParameterValueFromString) acepte justo lo que el editor muestra:
// "+3.2 dB", "1.02 kHz", "125 %", "3". Texto no reconocido -> el valor no cambia.
float hostParamValueFromText(int hostId, const juce::String& text, bool& parsed);

} // namespace rtxx
