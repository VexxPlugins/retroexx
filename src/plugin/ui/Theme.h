#pragma once
// Tokens de la estetica del editor. Ningun componente escribe un color literal: todos salen
// de aqui, y el test de captura comprueba que la paleta usada es esta.
//
// v3 (2026-10-22, encargo del dueno de la marca): "estetica oscura, valores digitales tipo
// cuadro de coche de los 90, azul verdoso". Se mantiene el chasis oscuro medido del
// motor (fondo, pantalla hundida, filos) y se sustituye:
//
//   * la franja CREMA por una banda de grafito: todo el plugin es oscuro,
//   * el arco azul hielo y el texto de los valores por un DISPLAY DE 7 SEGMENTOS
//     verde-azulado (VFD de cuadro de instrumentos), dibujado con geometria
//     (ui/SegmentDisplay.h), no con una fuente,
//   * el LED de estado de cada modulo sigue en dorado de marca: es el unico elemento
//     calido y por eso marca estado (encendido) sin competir con las lecturas.
//
// Procedencia de los tonos: la chapa y la pantalla siguen siendo las medidas de la piel
// del motor. El verde-azulado del
// display sale de la familia de los VFD de cuadro (verde-cian sobre negro verdoso), con el
// contraste calculado y anotado al lado.

#include <juce_graphics/juce_graphics.h>

namespace rtxx::ui {

namespace colours {

// Tokens de ../../../../VEXX-DASH90.md §3 (la regla de la linea Vexx). Los nombres
// historicos (window, screen, lcdInk...) quedan como alias de su token DASH-90.

// --- carcasa y superficies (§3.1) ---------------------------------------------------
inline const juce::Colour plastic{0xff0b0c0e};     // fondo de ventana
inline const juce::Colour plasticHi{0xff101114};   // header, tiras elevadas
inline const juce::Colour strip{0xff0e0f11};       // tira inferior de I/O
inline const juce::Colour well{0xff060708};        // pantalla hundida de las columnas
inline const juce::Colour wellEdge{0xff191c21};    // filo de la pantalla hundida
inline const juce::Colour podTop{0xff16181c};      // carcasa de modulo, arriba
inline const juce::Colour podBottom{0xff0f1013};   // carcasa de modulo, abajo
inline const juce::Colour bandPlate{0xff121110};   // banda de macros
inline const juce::Colour bandCellTop{0xff1a1816}; // placa de cada macro, arriba
inline const juce::Colour bandCellEdge{0xff262320};
inline const juce::Colour keyTop{0xff25282d};      // teclas
inline const juce::Colour keyBottom{0xff141619};
inline const juce::Colour keyEdge{0xff2c3036};
inline const juce::Colour keyFlat{0xff16191d};     // flechas del selector de tipo
inline const juce::Colour separator{0xff262a30};   // separadores de 1 px

// --- cristal (§3.2) -------------------------------------------------------------------
inline const juce::Colour glass{0xff030605};
inline const juce::Colour glassDeep{0xff020403};
inline const juce::Colour glassEdge{0xff1e2a27};

// --- fosforos (§3.3) ------------------------------------------------------------------
inline const juce::Colour vfd{0xff3cf2c4};          // 14,23:1 sobre glass
inline const juce::Colour vfdDim{0xff26987b};       // 5,68:1 (vfd al 62 %)
inline const juce::Colour vfdGhost{0xff071712};     // segmentos apagados (no es texto)
inline const juce::Colour vfdWhite{0xffeffffa};     // marcador del crossfade (casi blanco)
inline const juce::Colour phosphorRed{0xffff4a2a};  // 6,07:1; ruido y clip
inline const juce::Colour amber{0xffffb23c};        // 11,32:1; avisos
inline const juce::Colour structRed{0xffd2402f};    // filete bajo el header
inline const juce::Colour brandGold{0xffffc94a};    // solo VEXX AUDIO y nucleo del logo
inline const juce::Colour traceWarm{0xffff8a3c};    // 2.ª curva de un viz (§3.4)
inline const juce::Colour traceLight{0xffffe2b0};   // flutter, detalle fino (§3.4)

// --- tinta fuera del cristal (§3.5) -----------------------------------------------------
inline const juce::Colour ink{0xffe7e3d8};         // 15,26:1 sobre plastic
inline const juce::Colour inkLabel{0xffa9b0ba};    // 8,13:1 sobre podTop: leyendas
inline const juce::Colour inkHeader{0xffaeb5bf};   // 9,14:1: rotulos del header
inline const juce::Colour inkKey{0xffc9ced6};      // 10,46:1: texto de teclas
inline const juce::Colour inkMuted{0xff7e8692};    // 5,22:1 sobre strip: MODE, IN/EQ/OUT
inline const juce::Colour inkBand{0xffcfc9ba};     // 11,42:1: nombre grabado en la banda
inline const juce::Colour inkBandOff{0xff8a857b};  // 5,14:1: nombre de modulo apagado
inline const juce::Colour pointer{0xffece7da};     // puntero de knob, marcadores

// --- alias historicos -----------------------------------------------------------------
inline const juce::Colour window = plastic;
inline const juce::Colour windowDeep = well;
inline const juce::Colour screen = well;
inline const juce::Colour screenHi = wellEdge;
inline const juce::Colour plate = podBottom;
inline const juce::Colour hairline = separator;
inline const juce::Colour band = bandPlate;
inline const juce::Colour bandDeep = well;
inline const juce::Colour bandHi = bandCellTop;
inline const juce::Colour bandInk = inkBand;
inline const juce::Colour bandInkMuted = inkBandOff;
inline const juce::Colour inkDim = inkMuted;        // controles de un modulo apagado
inline const juce::Colour lcdBack = glass;
inline const juce::Colour lcdFrame = glassEdge;
inline const juce::Colour lcdInk = vfd;
inline const juce::Colour lcdInkDim = vfdDim;
inline const juce::Colour led = vfd;
inline const juce::Colour ledDim = vfdGhost;
inline const juce::Colour accent = brandGold;
inline const juce::Colour detail = brandGold;
inline const juce::Colour redLine = structRed;
inline const juce::Colour hot = phosphorRed;       // clip y errores

// --- knob (§3.1 / §5.3: plastico negro, luz arriba a la izquierda) --------------------
inline const juce::Colour knobBodyHi{0xff3b3f46};
inline const juce::Colour knobBodyMid{0xff1a1c20};
inline const juce::Colour knobBodyLo{0xff0e0f12};
inline const juce::Colour knobEdge{0xff2a2e35};
inline const juce::Colour knobCapHi{0xff2e3238};
inline const juce::Colour knobCapLo{0xff17191d};

}  // namespace colours

// Rejilla de 8, esquinas rectas en paneles y 2 px en controles.
namespace metrics {
inline constexpr int grid = 8;
inline constexpr int panelRadius = 0;
inline constexpr int controlRadius = 2;
inline constexpr int accentLine = 2;
inline constexpr int footerBar = 3;
inline constexpr int panelPadding = 12;
inline constexpr int panelGap = 16;
inline constexpr int pageMargin = 24;
}  // namespace metrics

// Geometria del motor llevada a la ventana de 1240 px
// con UN solo factor. Se convierten las dos esquinas del rect (no x + ancho): asi dos
// controles que en El motor se tocan siguen tocandose despues del redondeo.
namespace geometry {
inline constexpr float kScale = 1240.0f / 797.0f;
inline int scaled(float v) { return juce::roundToInt(v * kScale); }
inline juce::Rectangle<int> scaled(float x, float y, float w, float h) {
    const int x0 = scaled(x), y0 = scaled(y);
    return {x0, y0, scaled(x + w) - x0, scaled(y + h) - y0};
}
}  // namespace geometry

// Escala 1.25 sobre 12 px.
inline constexpr float typeBody = 12.0f;
inline constexpr float typeSmall = 10.0f;      // metadatos, unidades
inline constexpr float typeTiny = 9.0f;        // estados (ON/OFF)
inline constexpr float typeValue = 11.5f;      // valores (texto, no segmentos)
inline constexpr float typeModule = 11.5f;     // titulo de modulo
inline constexpr float typeHeadline = 22.0f;   // wordmark

}  // namespace rtxx::ui
