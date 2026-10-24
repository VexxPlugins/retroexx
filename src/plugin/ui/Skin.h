#pragma once
// Pintura procedural de las superficies: nada de texturas importadas del motor.
//
// VEXX-DASH90: plastico negro con grano fino. El grano es un mosaico de ruido determinista
// (xorshift con semilla fija, para que dos capturas salgan iguales), generado UNA vez y
// cacheado: pintar 1 M de pixeles de ruido en cada repintado costaria milisegundos por frame.

#include <juce_graphics/juce_graphics.h>

namespace rtxx::ui::skin {

// Mosaico de ruido de `size` x `size`, con contraste `strength` (0..1) alrededor del gris
// medio; se usa con setOpacity muy baja y tiled.
const juce::Image& noiseTile(int size = 128, float strength = 1.0f);

// Fondo completo de la ventana: plastico negro con grano.
void paintWindow(juce::Graphics& g, juce::Rectangle<int> bounds);

// Pantalla hundida (well): relleno, sombra superior y filo wellEdge.
void paintScreen(juce::Graphics& g, juce::Rectangle<int> bounds);

// Banda de macros en grafito calido (bandPlate) con grano y sombra inferior.
void paintBand(juce::Graphics& g, juce::Rectangle<int> bounds);

// Ventana VFD (VEXX-DASH90 §5.5), capas 1-2: cristal `glass`, filo de 1 px (`edge`, o
// glassEdge si es transparente) y sombra interior. Va ANTES del contenido.
void paintLcd(juce::Graphics& g, juce::Rectangle<float> bounds, bool lit,
              juce::Colour edge = juce::Colour());

// Capas 4-5 de la ventana VFD: scanlines (1 px negro al 28 % cada 3 px) y reflejo diagonal a
// 115 grados (blanco al 4,5 % en el primer 30 % del ancho). Va DESPUES del contenido: es el
// cristal que hay delante de los digitos.
void paintGlass(juce::Graphics& g, juce::Rectangle<float> bounds);

// Tecla (§5.4): sombra de pulsacion de `drop` px, degradado keyTop -> keyBottom, filo keyEdge
// y brillo interior arriba. `bounds` incluye la sombra (la tecla ocupa bounds - drop abajo).
// Devuelve el rect de la cara de la tecla. `hover` aclara un 6 %.
juce::Rectangle<float> paintKey(juce::Graphics& g, juce::Rectangle<float> bounds, float radius,
                                bool hover, float drop = 3.0f);

// Ventanita iluminada de una tecla o LED cuadrado: el color con halo si esta encendida, al
// 12 % si no.
void paintKeyLight(juce::Graphics& g, juce::Rectangle<float> light, juce::Colour colour, bool on);

// Rayado de modulo apagado (§6): diagonales a 135 grados, 4 px negro al 58 % / 4 px blanco al
// 2,5 %. `amount` 0..1 es el fundido de entrada.
void paintHatch(juce::Graphics& g, juce::Rectangle<float> bounds, float amount);

// Linea de 1 px con luz arriba y sombra abajo: el filo de una pieza mecanizada.
void paintEdge(juce::Graphics& g, int y, int x, int width, bool lightAbove);

}  // namespace rtxx::ui::skin
