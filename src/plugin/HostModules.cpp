#include "HostModules.h"

namespace rtxx {
namespace {

// Colores medidos de las columnas del motor (muestreo de la captura y de la piel):
// NOISE #4B2220 (vino), WOBBLE #463222 (ambar), MAGNETIC #343744 (azul pizarra). Los demas
// se derivan de la misma familia para que la fila se lea como un aparato, no como un
// arcoiris: el rojo manda (es el modulo que el usuario mira primero).
//
// Los mandos de cada columna son todos menos el Amount (macroHostId), que es el knob grande
// de la banda inferior.
const ModuleSpec kModules[] = {
    {"NOISE", 7, 1, {8, 9, 10, 11, 12, 13, 0, 0}, 6, 0xffff4a2au},
    {"WOBBLE", 14, 2, {15, 16, 17, 18, 19, 20, 0, 0}, 6, 0xffffb23cu},
    {"DIST", 21, 3, {22, 23, 24, 25, 26, 0, 0, 0}, 5, 0xffe8d45au},
    {"DIGITAL", 27, 4, {28, 29, 30, 31, 32, 33, 34, 0}, 7, 0xff8fc8ffu},
    {"SPACE", 35, 5, {36, 37, 38, 39, 40, 41, 0, 0}, 6, 0xff3cf2c4u},
    {"MAGNETIC", 42, 6, {43, 44, 45, 46, 47, 0, 0, 0}, 5, 0xff9db4ffu},
};

// --- distribucion de cada columna ------------------------------------------------------
// Coordenadas del motor dentro de la columna: knobs y botones en la
// rejilla de celdas de 40 x 40 en (15|75, 148|212); selector/crossfade/DECAY en y = 103;
// FOCUS en (17, 145) de 40 x 110 (91 en DIGITAL, que pone su CUT debajo); FLUX en la fila
// comun y = 277 (la misma altura en las seis columnas).
constexpr ControlSpec knob(int id, const char* caption, short x, short y) {
    return {ControlKind::Knob, id, -1, "", nullptr, nullptr, caption, x, y, 40, 40, -1};
}
constexpr ControlSpec button(int id, const char* text, const char* caption, short x, short y,
                             short w = 40, short h = 40) {
    return {ControlKind::Button, id, -1, text, nullptr, nullptr, caption, x, y, w, h, -1};
}
constexpr ControlSpec selector(int id) {
    return {ControlKind::Selector, id, -1, "", nullptr, nullptr, "TYPE", 10, 103, 104, 23, -1};
}
constexpr ControlSpec crossfade(int id, const char* lo, const char* hi) {
    return {ControlKind::Fader, id, -1, "", lo, hi, "", 17, 104, 96, 20, -1};
}
constexpr ControlSpec decay(int id) {
    return {ControlKind::Fader, id, -1, "", nullptr, nullptr, "DECAY", 17, 104, 96, 20, -1};
}
constexpr ControlSpec flux(int id) {
    return {ControlKind::Fader, id, -1, "", nullptr, nullptr, "FLUX", 37, 277, 56, 18, 297};
}
constexpr ControlSpec focus(int idLo, int idHi, short h = 110) {
    return {ControlKind::Range, idLo, idHi, "", nullptr, nullptr, "FOCUS", 17, 145, 40, h, 257};
}

const ColumnLayout kColumns[] = {
    {6,
     {selector(8), knob(9, "TONE", 15, 148), button(11, "POST", "ROUTING", 75, 148),
      knob(10, "FOLLOW", 15, 212), knob(12, "DUCK", 75, 212), flux(13)}},
    {6,
     {crossfade(15, "WOW", "FLUTTER"), knob(16, "RATE", 15, 148), knob(18, "RATE", 75, 148),
      button(17, "ST", "STEREO", 15, 212), knob(19, "MIX", 75, 212), flux(20)}},
    {4, {selector(22), focus(23, 24), knob(25, "MIX", 75, 180), flux(26)}},
    {6,
     {crossfade(28, "RATE", "BITS"), focus(29, 30, 91), button(31, "CUT", "", 19, 240, 36, 13),
      knob(32, "SMOOTH", 75, 148), knob(33, "MIX", 75, 212), flux(34)}},
    {5,
     {decay(36), focus(37, 38), knob(39, "PREDELAY", 75, 148),
      button(40, "ST", "STEREO", 75, 212), flux(41)}},
    {5,
     {crossfade(43, "WEAR", "FLUTTER"), button(44, "ST", "STEREO", 15, 148),
      knob(45, "RATE", 75, 148), knob(46, "DROPOUTS", 45, 212), flux(47)}},
};

} // namespace

const ColumnLayout& columnLayout(int moduleIndex) {
    const int count = static_cast<int>(sizeof(kColumns) / sizeof(kColumns[0]));
    const int index = moduleIndex < 0 ? 0 : (moduleIndex >= count ? count - 1 : moduleIndex);
    return kColumns[index];
}

const ModuleSpec* ModuleTable::all() { return kModules; }
int ModuleTable::count() { return static_cast<int>(sizeof(kModules) / sizeof(kModules[0])); }

} // namespace rtxx
