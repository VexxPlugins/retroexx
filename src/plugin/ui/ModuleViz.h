#pragma once
// Visualizadores por parametros (VEXX-DASH90 §8): WOBBLE, DIGITAL y DIST. Todo sale de los
// valores de los parametros (normalizados 0..1) y, en DIST, del nivel real del modulo; no
// hay datos inventados. Trazo en dos pasadas (halo ancho + trazo fino).
//
// Las leyes de rate del motor no estan medidas: la velocidad y el numero de ciclos son
// monotonos con el parametro dentro de los margenes de §9, no una lectura en Hz.

#include <juce_graphics/juce_graphics.h>

#include "block/VizTap.h"

namespace rtxx::ui::viz {

// Dos LFO superpuestos: WOW lento y grueso en el color del modulo, FLUTTER rapido y fino en
// el trazo secundario claro. `wowPhase`/`flutterPhase` en radianes (las avanza el panel).
void paintWobble(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour,
                 float amount, float balance, float wowRate, float flutterRate, float mix,
                 float wowPhase, float flutterPhase);

// Ciclos de cada LFO a lo ancho y segundos que tarda el trazo en cruzar la pantalla.
float wowCycles(float rate);
float flutterCycles(float rate);
float scrollSeconds(float rate);

// Senoide muestreada (RATE) y cuantizada (BITS) segun el balance, suavizada por SMOOTH y
// mezclada por MIX, con la senoide continua en fantasma punteado.
void paintDigital(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour,
                  float amount, float balance, float smooth, float mix);

// Pictograma del tipo (0..5: TUBEPAIR, IRON, CRUNCH, HEAVY, ZAP, AIR) cuyo filamento brilla
// con el drive; `flicker` (0..1) es la opacidad extra que quita el parpadeo de entrada.
void paintDist(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour, int type,
               float drive, float flicker);

// --- por audio (fase 4): `frames` son las ultimas tramas de la toma, de la mas vieja a la
// mas nueva, una por quantum de 64 muestras.

// Osciloscopio de la salida del ruido: min/max por trama, en escala dB (suelo -60 dBFS) para
// que el ruido de fondo y los chasquidos se lean a la vez.
void paintNoise(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour,
                const VizFrame* frames, int count);

// Dos curvas de ganancia (salida / entrada en dB): L arriba en `traceWarm`, R abajo en el
// color del modulo. Donde la entrada calla o la ganancia cae por debajo de -24 dB (dropout)
// la curva se corta.
void paintMagnetic(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour,
                   const VizFrame* frames, int count);

// Dos anillos de 12 puntos (L y R): cada punto es la energia de un doceavo de la historia;
// la cola de la reverb los hace respirar.
void paintSpace(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour,
                const VizFrame* frames, int count);

}  // namespace rtxx::ui::viz
