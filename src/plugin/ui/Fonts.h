#pragma once
// Tipografia de DESIGN.md §2: Orbitron (titulos, modulos, wordmark) y Rajdhani Medium
// (cuerpo, valores, metadatos). Las dos son OFL y viven en ui/assets/fonts/ con su
// licencia; se compilan a BinaryData (rtxx_ui_assets) y se cargan desde memoria.
//
// Si el TTF no llega a BinaryData, `fontsLoaded()` lo dice y se usa la sans del sistema:
// la UI sigue siendo legible y el fallo se reporta, no se disimula.

#include <juce_graphics/juce_graphics.h>

namespace rtxx::ui {

// Peso 500..900 de Orbitron (los pesos intermedios caen al mas cercano disponible).
juce::Font orbitron(float height, int weight = 600);

// Rajdhani Medium 500, la unica variante que usa el contrato.
juce::Font rajdhani(float height);

// false si alguna de las dos familias no se pudo cargar (se esta usando la del sistema).
bool fontsLoaded();

}  // namespace rtxx::ui
