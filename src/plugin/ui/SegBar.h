#pragma once
// Barra segmentada de VFD (VEXX-DASH90 §5.2): segmentos discretos de 2 px de separacion y
// radio 1,5 px. Encendido: fosforo con halo; apagado: fosforo al 10 %. Es el "tacometro
// escalonado" de los cuadros de los 90; ningun relleno continuo.

#include <juce_graphics/juce_graphics.h>

namespace rtxx::ui {

enum class SegBarMode {
    Fill,   // alturas iguales, se encienden de izquierda a derecha (FLUX, DECAY)
    Wedge,  // altura del 30 % al 100 % de izquierda a derecha (MAGNITUDE)
    XFade,  // cuna doble centrada; se enciende del centro al marcador (crossfades A <-> B)
    Range,  // alturas iguales, encendidos entre lo y hi (CUT)
};

namespace segbar {

// Barra horizontal de `count` segmentos en `area`. `value` (0..1) para Fill/Wedge/XFade;
// `lo`/`hi` (0..1) para Range. `lit` = false apaga todo a sus fantasmas.
void draw(juce::Graphics& g, juce::Rectangle<float> area, SegBarMode mode, int count,
          float value, float lo, float hi, juce::Colour colour, bool lit = true);

// Columna vertical del FOCUS: segmentos de 7 px de alto con paso ~9,6 px, encendidos entre
// lo y hi (0 = abajo). Devuelve las y de los marcadores (hi, lo) en coordenadas de `area`.
juce::Point<float> drawColumn(juce::Graphics& g, juce::Rectangle<float> area, float lo, float hi,
                              juce::Colour colour, bool lit = true);

// Marcador triangular de un tirador (`pointer`), con la punta hacia `tip`.
void drawMarker(juce::Graphics& g, juce::Point<float> tip, float size, float angleRadians);

}  // namespace segbar
}  // namespace rtxx::ui
