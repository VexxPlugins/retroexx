#include "ModuleViz.h"

#include <cmath>

#include "Fonts.h"
#include "HostParamDisplay.h"
#include "Theme.h"

namespace rtxx::ui::viz {
namespace {

constexpr float kTwoPi = juce::MathConstants<float>::twoPi;

// Trazo en dos pasadas (§8): halo ancho al 22 % y trazo fino al 100 %.
void strokeTwoPass(juce::Graphics& g, const juce::Path& path, juce::Colour colour, float halo,
                   float line) {
    g.setColour(colour.withMultipliedAlpha(0.22f));
    g.strokePath(path, juce::PathStrokeType(halo, juce::PathStrokeType::curved,
                                            juce::PathStrokeType::rounded));
    g.setColour(colour);
    g.strokePath(path, juce::PathStrokeType(line, juce::PathStrokeType::curved,
                                            juce::PathStrokeType::rounded));
}

// Amplitud a 0..1 en escala dB con suelo de -60 dBFS, conservando el signo.
float dbScale(float v) {
    const float a = std::abs(v);
    const float db = a > 0.001f ? 20.0f * std::log10(a) : -60.0f;
    return std::copysign((db + 60.0f) / 60.0f, v);
}

// Profundidad visible de un parametro de cantidad: raiz para que el 10 % ya se lea.
float depth(float amount) { return std::sqrt(juce::jlimit(0.0f, 1.0f, amount)); }

}  // namespace

float wowCycles(float rate) { return 1.0f + 3.0f * rate; }
float flutterCycles(float rate) { return 6.0f + 18.0f * rate; }
// §9: de lento (9 s por ancho) a rapido (2,2 s por ancho).
float scrollSeconds(float rate) { return 9.0f - 6.8f * juce::jlimit(0.0f, 1.0f, rate); }

void paintWobble(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour,
                 float amount, float balance, float wowRate, float flutterRate, float mix,
                 float wowPhase, float flutterPhase) {
    const float half = area.getHeight() * 0.42f;
    const float scale = depth(amount) * juce::jlimit(0.0f, 1.0f, mix);
    const float wowAmp = half * scale * (1.0f - balance);
    const float flutterAmp = half * 0.5f * scale * balance;
    const float wowK = kTwoPi * wowCycles(wowRate) / area.getWidth();
    const float flutterK = kTwoPi * flutterCycles(flutterRate) / area.getWidth();

    juce::Path wow, flutter;
    const float cy = area.getCentreY();
    for (float x = 0.0f; x <= area.getWidth(); x += 3.0f) {
        const float px = area.getX() + x;
        const float yw = cy - wowAmp * std::sin(wowK * x + wowPhase);
        const float yf = cy + flutterAmp * std::sin(flutterK * x + flutterPhase);
        if (x == 0.0f) {
            wow.startNewSubPath(px, yw);
            flutter.startNewSubPath(px, yf);
        } else {
            wow.lineTo(px, yw);
            flutter.lineTo(px, yf);
        }
    }
    // Juntas a inglete: con curvas suaves y muestreo de 3 px no se distinguen de las redondas
    // y el trazador las resuelve mucho mas barato (se repinta a 60 Hz).
    g.setColour(colour.withMultipliedAlpha(0.22f));
    g.strokePath(wow, juce::PathStrokeType(4.0f, juce::PathStrokeType::mitered));
    g.setColour(colour);
    g.strokePath(wow, juce::PathStrokeType(1.6f, juce::PathStrokeType::mitered));
    g.setColour(colours::traceLight.withMultipliedAlpha(0.85f * colour.getFloatAlpha()));
    g.strokePath(flutter, juce::PathStrokeType(1.1f, juce::PathStrokeType::mitered));
}

void paintDigital(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour,
                  float amount, float balance, float smooth, float mix) {
    const float half = area.getHeight() * 0.42f;
    const float cy = area.getCentreY();
    const float k = kTwoPi * 2.0f / area.getWidth();   // dos ciclos a lo ancho
    const float d = depth(amount);
    const float hold = 1.0f + 18.0f * d * (1.0f - balance);                     // px por muestra
    const float levels = juce::jmax(1.0f, std::round(std::exp2(7.0f * (1.0f - d * balance))));
    const auto quantise = [levels](float v) { return std::round(v * levels) / levels; };
    const auto sine = [k](float x) { return std::sin(k * x); };

    juce::Path ghost, steps;
    for (float x = 0.0f; x <= area.getWidth(); x += 1.0f) {
        const float px = area.getX() + x;
        // Muestreo y retencion: el valor de la muestra al principio de su celda.
        const float x0 = std::floor(x / hold) * hold;
        const float held = quantise(sine(x0));
        // SMOOTH: lleva la retencion hacia la recta entre muestras.
        const float next = quantise(sine(x0 + hold));
        const float linear = held + (next - held) * (x - x0) / hold;
        const float wet = held + juce::jlimit(0.0f, 1.0f, smooth) * (linear - held);
        const float y = cy - half * (sine(x) + juce::jlimit(0.0f, 1.0f, mix) * (wet - sine(x)));
        if (x == 0.0f) {
            ghost.startNewSubPath(px, cy - half * sine(x));
            steps.startNewSubPath(px, y);
        } else {
            ghost.lineTo(px, cy - half * sine(x));
            steps.lineTo(px, y);
        }
    }
    juce::Path dashed;
    const float dashes[] = {2.0f, 3.0f};
    juce::PathStrokeType(1.0f).createDashedStroke(dashed, ghost, dashes, 2);
    g.setColour(colour.withMultipliedAlpha(0.25f));
    g.fillPath(dashed);

    g.setColour(colour.withMultipliedAlpha(0.20f));
    g.strokePath(steps, juce::PathStrokeType(4.0f, juce::PathStrokeType::mitered));
    g.setColour(colour);
    g.strokePath(steps, juce::PathStrokeType(1.6f, juce::PathStrokeType::mitered));
}

void paintDist(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour, int type,
               float drive, float flicker) {
    // Pictograma de un trazo en una caja de 72 x 48 centrada; `body` es la carcasa (tenue) y
    // `filament` lo que se enciende con el drive.
    const auto box = area.withSizeKeepingCentre(72.0f, 48.0f).translated(0.0f, -6.0f);
    const float x = box.getX(), y = box.getY(), w = box.getWidth(), h = box.getHeight();
    juce::Path body, filament;
    switch (juce::jlimit(0, 5, type)) {
        case 0:   // TUBEPAIR: dos valvulas con su filamento en zigzag
            for (int i = 0; i < 2; ++i) {
                const float cx = x + w * (i == 0 ? 0.3f : 0.7f);
                body.addRoundedRectangle(cx - 11.0f, y + 2.0f, 22.0f, h - 8.0f, 10.0f);
                body.startNewSubPath(cx - 7.0f, y + h - 6.0f);
                body.lineTo(cx - 7.0f, y + h);
                body.startNewSubPath(cx + 7.0f, y + h - 6.0f);
                body.lineTo(cx + 7.0f, y + h);
                filament.startNewSubPath(cx - 5.0f, y + h - 12.0f);
                for (int s = 0; s < 4; ++s)
                    filament.lineTo(cx + (s % 2 == 0 ? 5.0f : -5.0f), y + h - 16.0f - 5.0f * static_cast<float>(s));
            }
            break;
        case 1:   // IRON: transformador, dos bobinas sobre el nucleo
            body.addRectangle(x + w * 0.5f - 2.0f, y, 4.0f, h);
            for (int side = 0; side < 2; ++side) {
                const float cx = x + w * (side == 0 ? 0.28f : 0.72f);
                filament.startNewSubPath(cx, y + 4.0f);
                for (int s = 0; s < 4; ++s) {
                    const float top = y + 4.0f + 10.0f * static_cast<float>(s);
                    filament.cubicTo(cx + (side == 0 ? -12.0f : 12.0f), top,
                                     cx + (side == 0 ? -12.0f : 12.0f), top + 10.0f, cx, top + 10.0f);
                }
            }
            break;
        case 2: {  // CRUNCH: transistor (circulo, base, colector y emisor)
            const float cx = x + w * 0.5f, cy = y + h * 0.5f;
            body.addEllipse(cx - 22.0f, cy - 22.0f, 44.0f, 44.0f);
            body.startNewSubPath(cx - 34.0f, cy);
            body.lineTo(cx - 8.0f, cy);
            filament.startNewSubPath(cx - 8.0f, cy - 12.0f);
            filament.lineTo(cx - 8.0f, cy + 12.0f);
            filament.startNewSubPath(cx - 8.0f, cy - 5.0f);
            filament.lineTo(cx + 10.0f, cy - 18.0f);
            filament.startNewSubPath(cx - 8.0f, cy + 5.0f);
            filament.lineTo(cx + 10.0f, cy + 18.0f);
            filament.addArrow({cx + 2.0f, cy + 11.0f, cx + 10.0f, cy + 18.0f}, 0.0f, 7.0f, 6.0f);
            break;
        }
        case 3:   // HEAVY: senoide recortada contra dos rieles
            body.startNewSubPath(x, y + 8.0f);
            body.lineTo(x + w, y + 8.0f);
            body.startNewSubPath(x, y + h - 8.0f);
            body.lineTo(x + w, y + h - 8.0f);
            for (float t = 0.0f; t <= w; t += 1.0f) {
                const float v = juce::jlimit(-1.0f, 1.0f, 2.2f * std::sin(kTwoPi * 1.5f * t / w));
                const float py = y + h * 0.5f - v * (h * 0.5f - 8.0f);
                if (t == 0.0f) filament.startNewSubPath(x + t, py);
                else filament.lineTo(x + t, py);
            }
            break;
        case 4:   // ZAP: rayo
            body.addRoundedRectangle(x + 8.0f, y, w - 16.0f, h, 6.0f);
            filament.startNewSubPath(x + w * 0.58f, y + 4.0f);
            filament.lineTo(x + w * 0.36f, y + h * 0.54f);
            filament.lineTo(x + w * 0.54f, y + h * 0.54f);
            filament.lineTo(x + w * 0.42f, y + h - 4.0f);
            filament.lineTo(x + w * 0.66f, y + h * 0.42f);
            filament.lineTo(x + w * 0.48f, y + h * 0.42f);
            filament.closeSubPath();
            break;
        default: {  // AIR: manometro con su aguja
            const float cx = x + w * 0.5f, cy = y + h - 6.0f, r = h - 10.0f;
            body.addCentredArc(cx, cy, r, r, 0.0f, -juce::MathConstants<float>::halfPi * 1.2f,
                               juce::MathConstants<float>::halfPi * 1.2f, true);
            for (int s = 0; s <= 6; ++s) {
                const float a = -1.2f * juce::MathConstants<float>::halfPi +
                                2.4f * juce::MathConstants<float>::halfPi * static_cast<float>(s) / 6.0f;
                body.startNewSubPath(cx + (r - 6.0f) * std::sin(a), cy - (r - 6.0f) * std::cos(a));
                body.lineTo(cx + r * std::sin(a), cy - r * std::cos(a));
            }
            // La aguja marca el drive.
            const float a = (-1.2f + 2.4f * juce::jlimit(0.0f, 1.0f, drive)) * juce::MathConstants<float>::halfPi;
            filament.startNewSubPath(cx, cy);
            filament.lineTo(cx + (r - 9.0f) * std::sin(a), cy - (r - 9.0f) * std::cos(a));
            filament.addEllipse(cx - 3.0f, cy - 3.0f, 6.0f, 6.0f);
            break;
        }
    }

    g.setColour(colour.withMultipliedAlpha(0.45f));
    g.strokePath(body, juce::PathStrokeType(1.3f, juce::PathStrokeType::curved,
                                            juce::PathStrokeType::rounded));
    // Filamento: fantasma al 18 % con drive 0, pleno con drive 1; el parpadeo resta encima.
    const float glow = (0.18f + 0.82f * juce::jlimit(0.0f, 1.0f, drive)) * (1.0f - flicker);
    strokeTwoPass(g, filament, colour.withMultipliedAlpha(glow), 4.0f, 1.5f);

    // Contexto abajo: tipo y drive, como el del ordenador de viaje (§7).
    g.setColour(colours::vfdDim.withMultipliedAlpha(colour.getFloatAlpha()));
    g.setFont(rajdhani(9.5f).withExtraKerningFactor(0.24f));
    g.drawText(hostParamChoiceName(22, juce::jlimit(0, 5, type)) +
                   juce::String::fromUTF8(" \xc2\xb7 DRIVE ") +
                   juce::String(juce::roundToInt(drive * 100.0f)),
               area.withTrimmedTop(area.getHeight() - 12.0f), juce::Justification::bottomLeft,
               false);
}

void paintNoise(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour,
                const VizFrame* frames, int count) {
    if (count < 2) return;
    // Una trama por pixel, la mas nueva a la derecha: un trazo vertical del maximo al minimo
    // de cada trama, para que los chasquidos (picos de una sola trama) se vean. Con
    // rectangulos y no con un Path: el zigzag de 340 vertices costaba 5 ms por fotograma.
    const int columns = juce::jmin(count, static_cast<int>(area.getWidth()));
    const float half = area.getHeight() * 0.46f, cy = area.getCentreY();
    // Rectangulos de pixel entero (sin antialias) y halo de 2 px arriba y abajo: las columnas
    // vecinas ya cierran el halo en horizontal.
    juce::RectangleList<int> halo, trace;
    const int right = juce::roundToInt(area.getRight());
    for (int i = 0; i < columns; ++i) {
        const VizFrame& f = frames[count - columns + i];
        const int top = juce::roundToInt(cy - half * dbScale(std::max(f.outMax[0], f.outMax[1])));
        const int bottom = juce::roundToInt(cy - half * dbScale(std::min(f.outMin[0], f.outMin[1])));
        const int x = right - (columns - i);
        trace.addWithoutMerging({x, top, 1, bottom - top + 1});
        halo.addWithoutMerging({x, top - 2, 1, bottom - top + 5});
    }
    g.setColour(colour.withMultipliedAlpha(0.22f));
    g.fillRectList(halo);
    g.setColour(colour);
    g.fillRectList(trace);
}

void paintMagnetic(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour,
                   const VizFrame* frames, int count) {
    if (count < 2) return;
    // Ventanas de 4 tramas (256 muestras) por punto: la ganancia de una sola trama es
    // demasiado nerviosa para leerse como curva.
    constexpr int kWindow = 4;
    const int points = juce::jmin(count / kWindow, static_cast<int>(area.getWidth() / 1.5f));
    const float step = area.getWidth() / static_cast<float>(juce::jmax(1, points - 1));
    for (int ch = 0; ch < 2; ++ch) {
        // Banda de cada curva: L en la mitad de arriba, R en la de abajo; 0 dB arriba de la
        // banda y -24 dB abajo.
        const auto band = ch == 0 ? area.withHeight(area.getHeight() * 0.5f)
                                  : area.withTrimmedTop(area.getHeight() * 0.5f);
        juce::Path curve;
        bool open = false;
        for (int p = 0; p < points; ++p) {
            double in = 0.0, out = 0.0;
            for (int k = 0; k < kWindow; ++k) {
                const VizFrame& f = frames[count - (points - p) * kWindow + k];
                in += static_cast<double>(f.inRms[ch]) * f.inRms[ch];
                out += static_cast<double>(f.outRms[ch]) * f.outRms[ch];
            }
            const float gainDb = in > 1.0e-10 ? 10.0f * static_cast<float>(std::log10(out / in + 1.0e-12))
                                              : -100.0f;
            if (gainDb < -24.0f) {   // entrada muda o dropout: hueco
                open = false;
                continue;
            }
            const float t = juce::jlimit(0.0f, 1.0f, -gainDb / 24.0f);
            const float x = area.getX() + step * static_cast<float>(p);
            const float y = band.getY() + 4.0f + t * (band.getHeight() - 8.0f);
            if (!open) curve.startNewSubPath(x, y);
            else curve.lineTo(x, y);
            open = true;
        }
        const juce::Colour c = ch == 0 ? colours::traceWarm.withAlpha(colour.getFloatAlpha()) : colour;
        g.setColour(c.withMultipliedAlpha(0.20f));
        g.strokePath(curve, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
        g.setColour(c);
        g.strokePath(curve, juce::PathStrokeType(1.4f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
    }
}

void paintSpace(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour,
                const VizFrame* frames, int count) {
    constexpr int kPoints = 12;
    const int group = count / kPoints;
    const float r0 = juce::jmin(area.getWidth() * 0.25f, area.getHeight() * 0.5f) - 4.0f;
    for (int ch = 0; ch < 2; ++ch) {
        const juce::Point<float> centre(area.getX() + area.getWidth() * (ch == 0 ? 0.28f : 0.72f),
                                        area.getCentreY());
        juce::Path ring;
        for (int k = 0; k < kPoints; ++k) {
            double energy = 0.0;
            for (int i = 0; i < group; ++i) {
                const float v = frames[k * group + i].outRms[ch];
                energy += static_cast<double>(v) * v;
            }
            const float rms = group > 0 ? static_cast<float>(std::sqrt(energy / group)) : 0.0f;
            const float level = juce::jmax(0.0f, dbScale(rms));   // 0 en silencio
            const float r = r0 * (0.35f + 0.65f * level);
            const float a = juce::MathConstants<float>::twoPi * static_cast<float>(k) / kPoints;
            const juce::Point<float> p(centre.x + r * std::cos(a), centre.y + r * std::sin(a));
            if (k == 0) ring.startNewSubPath(p);
            else ring.lineTo(p);
        }
        ring.closeSubPath();
        strokeTwoPass(g, ring, colour, 3.5f, 1.5f);
    }
}

}  // namespace rtxx::ui::viz
