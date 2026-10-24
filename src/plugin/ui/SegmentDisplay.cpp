#include "SegmentDisplay.h"

#include <cmath>

#include "PowerOn.h"
#include "Theme.h"

namespace rtxx::ui {
namespace {

// Mascaras: bit 0 = a (arriba), 1 = b (arriba derecha), 2 = c (abajo derecha),
// 3 = d (abajo), 4 = e (abajo izquierda), 5 = f (arriba izquierda), 6 = g (centro).
// Es la tabla clasica de 7 segmentos (la misma que usa cualquier display de los 80/90).
struct Glyph {
    char c;
    int mask;
};

constexpr Glyph kGlyphs[] = {
    {'0', 0b0111111}, {'1', 0b0000110}, {'2', 0b1011011}, {'3', 0b1001111},
    {'4', 0b1100110}, {'5', 0b1101101}, {'6', 0b1111101}, {'7', 0b0000111},
    {'8', 0b1111111}, {'9', 0b1101111}, {'A', 0b1110111}, {'b', 0b1111100},
    {'C', 0b0111001}, {'d', 0b1011110}, {'E', 0b1111001}, {'F', 0b1110001},
    {'H', 0b1110110}, {'L', 0b0111000}, {'n', 0b1010100}, {'o', 0b1011100},
    {'P', 0b1110011}, {'r', 0b1010000}, {'S', 0b1101101}, {'t', 0b1111000},
    {'U', 0b0111110}, {'-', 0b1000000}, {'_', 0b0001000},
};

constexpr float kAspect = 0.56f;   // ancho / alto de un digito
constexpr float kGapRatio = 0.16f; // separacion entre digitos, relativa al ancho

}  // namespace

int SegmentDisplay::maskFor(char c) {
    for (const auto& glyph : kGlyphs)
        if (glyph.c == c) return glyph.mask;
    return 0;
}

bool SegmentDisplay::isSegmentable(const juce::String& text) {
    for (auto c : text)
        if (c != '.' && c != ' ' && c != ':' && maskFor(static_cast<char>(c)) == 0) return false;
    return !text.isEmpty();
}

float SegmentDisplay::measure(const juce::String& text, float height) {
    const float digitWidth = height * kAspect;
    float width = 0.0f;
    for (auto c : text) {
        if (c == '.') {
            width += digitWidth * 0.34f;
        } else if (c == ' ') {
            width += digitWidth * 0.5f;
        } else {
            width += digitWidth + digitWidth * kGapRatio;
        }
    }
    return width;
}

float SegmentDisplay::draw(juce::Graphics& g, juce::Rectangle<float> area,
                           const juce::String& text, float height, juce::Colour colour,
                           bool glow, bool italic) {
    const float digitWidth = height * kAspect;
    const float gap = digitWidth * kGapRatio;
    const float thickness = juce::jmax(1.4f, height * 0.12f);
    const float inset = thickness * 0.55f;

    float x = area.getX();
    const float top = area.getCentreY() - height * 0.5f;

    juce::Graphics::ScopedSaveState state(g);
    if (italic) {
        const float cy = area.getCentreY();
        g.addTransform(juce::AffineTransform::translation(0.0f, -cy)
                           .sheared(-std::tan(juce::degreesToRadians(8.0f)), 0.0f)
                           .translated(0.0f, cy));
    }

    // Un segmento: hexagono alargado con las puntas cortadas, como en un VFD real.
    auto segment = [&](float sx, float sy, float w, float h) {
        const float cut = juce::jmin(w, h) * 0.5f;
        juce::Path path;
        if (w >= h) {
            path.startNewSubPath(sx + cut, sy);
            path.lineTo(sx + w - cut, sy);
            path.lineTo(sx + w, sy + h * 0.5f);
            path.lineTo(sx + w - cut, sy + h);
            path.lineTo(sx + cut, sy + h);
            path.lineTo(sx, sy + h * 0.5f);
        } else {
            path.startNewSubPath(sx + w * 0.5f, sy);
            path.lineTo(sx + w, sy + cut);
            path.lineTo(sx + w, sy + h - cut);
            path.lineTo(sx + w * 0.5f, sy + h);
            path.lineTo(sx, sy + h - cut);
            path.lineTo(sx, sy + cut);
        }
        path.closeSubPath();
        return path;
    };

    juce::Path lit, ghost;
    const float textLength = static_cast<float>(juce::jmax(1, text.length()));
    int index = 0;
    for (auto c : text) {
        const bool sweep = power::forced(static_cast<float>(index++) / textLength);
        if (c == '.') {
            // Coma decimal: cuadradito T x T pegado a la linea base.
            const float size = thickness;
            lit.addRectangle(x + digitWidth * 0.04f - gap * 0.5f, top + height - size, size, size);
            x += digitWidth * 0.34f;
            continue;
        }
        if (c == ' ') {
            x += digitWidth * 0.5f;
            continue;
        }
        const int mask = sweep ? 0b1111111 : maskFor(static_cast<char>(c));
        const float w = digitWidth;
        const float h = height;
        const float gapLong = juce::jmax(0.4f, 0.18f * thickness);
        const juce::Path segments[7] = {
            segment(x + inset, top, w - inset * 2.0f, thickness),                          // a
            segment(x + w - thickness, top + inset, thickness, h * 0.5f - inset - gapLong), // b
            segment(x + w - thickness, top + h * 0.5f + gapLong, thickness,
                    h * 0.5f - inset - gapLong),                                           // c
            segment(x + inset, top + h - thickness, w - inset * 2.0f, thickness),          // d
            segment(x, top + h * 0.5f + gapLong, thickness, h * 0.5f - inset - gapLong),   // e
            segment(x, top + inset, thickness, h * 0.5f - inset - gapLong),                // f
            segment(x + inset, top + h * 0.5f - thickness * 0.5f, w - inset * 2.0f,
                    thickness),                                                            // g
        };
        for (int i = 0; i < 7; ++i)
            (mask & (1 << i) ? lit : ghost).addPath(segments[i]);
        x += w + gap;
    }

    // Fantasmas: el VFD apagado sigue viendose.
    g.setColour(colour.withAlpha(0.07f));
    g.fillPath(ghost);
    if (glow) {
        // Halo de dos pasadas (1,8 T ancha y 0,5 T cercana), sin DropShadow por fotograma.
        g.setColour(colour.withAlpha(0.10f));
        g.strokePath(lit, juce::PathStrokeType(thickness * 1.8f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
        g.setColour(colour.withAlpha(0.28f));
        g.strokePath(lit, juce::PathStrokeType(thickness * 0.5f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
    }
    g.setColour(colour);
    g.fillPath(lit);
    return x - area.getX();
}

}  // namespace rtxx::ui
