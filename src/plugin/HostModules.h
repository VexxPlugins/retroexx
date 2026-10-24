#pragma once
// Reparto de los 58 parametros del host en las piezas del editor, con la MISMA distribucion
// que El motor:
//
//   header  : logo, selector de preset, fader de MAGNITUDE
//   pantalla: 6 columnas en UNA fila (NOISE WOBBLE DIST DIGITAL SPACE MAGNETIC), cada una con
//             su pantalla (viz / ordenador de viaje) y sus mandos (sin el Amount ni el Enable)
//   banda   : 6 knobs GRANDES, uno por columna = el Amount (macro) de cada modulo, con su
// Enable
//   tira    : GAIN de entrada, EQ (Cut Lo / Cut Hi / modos / Tone / modo), WIDTH y GAIN de
// salida, como la barra inferior del motor
//
// Los ids son los de src/model/HostParam.h y entre las cuatro piezas cubren los 58
// exactamente una vez (tests/test_plugin_processor.cpp lo comprueba; no es una lista que
// pueda mentir).
//
// El orden de las columnas es el orden REAL del audio: la tabla encadena
// ToStereo -> NoiseFX -> Wobble -> Dist -> Digital -> Space -> EQ_Post -> Magnetic -> Width
// (ver src/gen/GraphSpec.gen.cpp, aristas).

#include "model/HostParam.h"

namespace rtxx {

// Tipo de control, calcado de lo que El motor pone en cada sitio: selector para los
// tipos, boton iluminado para POST/STEREO/CUT/MODE, deslizador para FLUX/DECAY/BALANCE y
// deslizador de dos tiradores para FOCUS/CUT.
enum class ControlKind { Knob, Selector, Button, Fader, Range };

// Un control de una columna, colocado como en El motor. `idA` es el parametro; `idB` el
// segundo (rango) o -1. `text` es el texto del boton, `caption` la leyenda que la columna
// escribe debajo y `endLo`/`endHi` los extremos de un crossfade.
//
// `x, y, w, h` son coordenadas del motor dentro de su columna de 124 x 275 (lienzo de
// 797 x 615, la tabla); el panel las pasa a pixeles con ui::geometry::scaled(). Los
// knobs y botones ocupan su celda de 40 x 40; la leyenda va debajo, fuera del control, a la
// altura `captionY` (-1 = justo debajo de la celda).
struct ControlSpec {
    ControlKind kind;
    int idA;
    int idB;
    const char* text;
    const char* endLo;
    const char* endHi;
    const char* caption;
    short x, y, w, h;
    short captionY;
};

struct ColumnLayout {
    int count;
    ControlSpec items[8];
};

// Distribucion de cada columna, leida de la tabla (posiciones absolutas):
//   NOISE    : TYPE (selector) | TONE + POST (boton) | FOLLOW + DUCK | FLUX
//   WOBBLE   : WOW<->FLUTTER | RATE + RATE | STEREO (boton) + MIX | FLUX
//   DIST     : TYPE (selector) | FOCUS (rango) + MIX | FLUX
//   DIGITAL  : RATE<->BITS | FOCUS (rango) + CUT (boton) | SMOOTH + MIX | FLUX
//   SPACE    : DECAY | FOCUS (rango) + PREDELAY + STEREO (boton) | FLUX
//   MAGNETIC : WEAR<->FLUTTER | STEREO (boton) + RATE | DROPOUTS | FLUX
const ColumnLayout& columnLayout(int moduleIndex);

struct ModuleSpec {
    const char* name;    // titulo de la columna
    int enableHostId;    // id del parametro Enable, -1 si el modulo no lo tiene
    int macroHostId;     // id del Amount (knob grande de la banda), -1 si no tiene
    int knobs[8];        // ids de los mandos de la columna, en orden de lectura
    int knobCount;
    // Color del modulo (0xAARRGGBB). Sale de medir las columnas del motor: cada una
    // esta tenida de un tono (NOISE vino, WOBBLE ambar, SPACE verde...) y sus knobs llevan
    // el aro de ese color. Ese tono pinta el lavado de la columna, el titulo, el aro del
    // dial, los digitos del LCD y el anillo del knob grande.
    unsigned int accent;
};

// Las 6 columnas de la pantalla, en orden de audio. El Amount de cada una (ids 1..6) NO va
// en la columna: va en la banda de knobs grandes, como en El motor.
struct ModuleTable {
    static const ModuleSpec* all();
    static int count();
};

// Parametros que no son de ninguna columna. Tres piezas:
//   kEqStrip   : los 6 mandos del EQ (corte alto/bajo, sus modos y el Tone)
//   kWidthKnob : el ancho de imagen
//   kInGainId / kOutGainId / kMagnitudeId : los tres del aparato
inline constexpr ModuleSpec kEqStrip{"EQ", 49, -1, {50, 51, 52, 53, 54, 55, 0, 0}, 6,
                                     0xffe0d9c7u};
inline constexpr ModuleSpec kWidthKnob{"WIDTH", -1, -1, {56, 0, 0, 0, 0, 0, 0, 0}, 1,
                                       0xffc8cbd2u};
inline constexpr int kInGainId = 48;
inline constexpr int kOutGainId = 57;
inline constexpr int kMagnitudeId = 0;

} // namespace rtxx
