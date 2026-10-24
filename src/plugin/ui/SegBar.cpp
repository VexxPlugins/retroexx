#include "SegBar.h"

#include <cmath>

#include "PowerOn.h"
#include "Theme.h"

namespace rtxx::ui::segbar {
namespace {

constexpr float kGap = 2.0f;
constexpr float kRadius = 1.5f;

void segment(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour colour, bool on,
             bool marker) {
    if (on || marker) {
        // Halo en dos rectangulos crecidos (sin DropShadow por fotograma).
        g.setColour(colour.withAlpha(0.12f));
        g.fillRoundedRectangle(r.expanded(2.5f), kRadius + 2.5f);
        g.setColour(colour.withAlpha(0.22f));
        g.fillRoundedRectangle(r.expanded(1.0f), kRadius + 1.0f);
        g.setColour(marker ? colours::vfdWhite : colour);
    } else {
        g.setColour(colour.withAlpha(0.10f));
    }
    g.fillRoundedRectangle(r, kRadius);
}

}  // namespace

void draw(juce::Graphics& g, juce::Rectangle<float> area, SegBarMode mode, int count,
          float value, float lo, float hi, juce::Colour colour, bool lit) {
    count = juce::jmax(2, count);
    value = juce::jlimit(0.0f, 1.0f, value);
    const float width = (area.getWidth() - kGap * static_cast<float>(count - 1)) /
                        static_cast<float>(count);
    const float centre = static_cast<float>(count - 1) * 0.5f;
    const int markerIndex = juce::roundToInt(value * static_cast<float>(count - 1));
    constexpr float e = 1.0e-6f;

    for (int i = 0; i < count; ++i) {
        const float t = (static_cast<float>(i) + 0.5f) / static_cast<float>(count);
        float height = area.getHeight();
        bool on = false, marker = false;
        switch (mode) {
            case SegBarMode::Fill:
                on = t <= value + e;
                break;
            case SegBarMode::Wedge:
                height *= 0.3f + 0.7f * static_cast<float>(i) / static_cast<float>(count - 1);
                on = t <= value + e;
                break;
            case SegBarMode::Range:
                on = t >= lo && t <= hi;
                break;
            case SegBarMode::XFade: {
                height *= 0.3f + 0.7f * std::abs(static_cast<float>(i) - centre) / centre;
                const float m = static_cast<float>(markerIndex);
                const float fi = static_cast<float>(i);
                on = m >= centre ? (fi >= centre - 0.01f && fi <= m) : (fi <= centre + 0.01f && fi >= m);
                marker = i == markerIndex;
                break;
            }
        }
        const float x = area.getX() + static_cast<float>(i) * (width + kGap);
        // Barras ancladas abajo; la cuna doble del crossfade, centrada.
        const float y = mode == SegBarMode::XFade ? area.getCentreY() - height * 0.5f
                                                  : area.getBottom() - height;
        segment(g, {x, y, width, height}, colour, (lit && on) || power::forced(t), lit && marker);
    }
}

juce::Point<float> drawColumn(juce::Graphics& g, juce::Rectangle<float> area, float lo, float hi,
                              juce::Colour colour, bool lit) {
    const int count = juce::jmax(2, juce::roundToInt(area.getHeight() / 9.6f));
    const float pitch = area.getHeight() / static_cast<float>(count);
    const float segH = juce::jmin(7.0f, pitch - kGap);
    for (int i = 0; i < count; ++i) {
        const float t = 1.0f - (static_cast<float>(i) + 0.5f) / static_cast<float>(count);
        const bool on = (lit && t >= lo && t <= hi) || power::forced(0.5f);
        const float y = area.getY() + static_cast<float>(i) * pitch + (pitch - segH) * 0.5f;
        segment(g, {area.getX(), y, area.getWidth(), segH}, colour, on, false);
    }
    return {area.getY() + (1.0f - hi) * area.getHeight(), area.getY() + (1.0f - lo) * area.getHeight()};
}

void drawMarker(juce::Graphics& g, juce::Point<float> tip, float size, float angleRadians) {
    juce::Path triangle;
    triangle.addTriangle(0.0f, 0.0f, size, -size * 0.55f, size, size * 0.55f);
    g.setColour(colours::pointer);
    g.fillPath(triangle, juce::AffineTransform::rotation(angleRadians).translated(tip));
}

}  // namespace rtxx::ui::segbar
