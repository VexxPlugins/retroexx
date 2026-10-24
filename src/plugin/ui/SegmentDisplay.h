#pragma once
// Display de 7 segmentos dibujado con geometria: la lectura "digital" de los cuadros de
// coche de los 90 (VFD/LCD verde-azulado).
//
// Por que geometria y no una fuente: un 7 segmentos de verdad tiene los segmentos como
// trapecios con su separacion, y la coma decimal ocupa su propia columna. Con una fuente
// "digital" de Google el resultado se parece, pero no escala igual ni se puede verificar
// segmento a segmento. Aqui cada caracter es una mascara de 7 bits, asi que el test puede
// comprobar que "8" enciende los siete y "1" solo dos, sin mirar pixeles a ojo.
//
// Lo que NO es un numero (nombres de tipo, "ON", unidades) se dibuja con Rajdhani dentro
// del mismo marco de LCD: el marco es lo que da el aire de instrumento, no la fuente.

#include <juce_graphics/juce_graphics.h>

namespace rtxx::ui {

class SegmentDisplay {
public:
    // Mascara de segmentos (bit 0 = a arriba, bit 6 = g centro) de un caracter, o 0 si el
    // caracter no es representable en 7 segmentos.
    static int maskFor(char c);

    // Dibuja `text` en 7 segmentos dentro de `area`, alineado a la izquierda y centrado
    // verticalmente. Devuelve el ancho consumido. VEXX-DASH90 §5.1: los segmentos apagados
    // se dibujan siempre (fantasma al 7 %), `glow` anade el halo de dos pasadas del VFD e
    // `italic` inclina 8 grados (el "185" del Fiat; recto solo para tablas densas).
    static float draw(juce::Graphics& g, juce::Rectangle<float> area, const juce::String& text,
                      float height, juce::Colour colour, bool glow = true, bool italic = true);

    // Ancho que ocuparia `text` a esa altura (sin dibujar).
    static float measure(const juce::String& text, float height);

    // Digitos representables (para el test y para decidir si un texto va en segmentos).
    static bool isSegmentable(const juce::String& text);
};

}  // namespace rtxx::ui
