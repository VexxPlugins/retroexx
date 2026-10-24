#include "Skin.h"

#include <cmath>
#include <vector>

#include "Theme.h"

namespace rtxx::ui::skin {
namespace {

// xorshift128+: rapido, deterministico y sin dependencias. Semilla fija para que la
// misma build dibuje exactamente la misma textura (el test de captura lo aprovecha).
struct Rng {
    juce::uint32 x = 0x9e3779b9u, y = 0x243f6a88u, z = 0xb7e15162u, w = 0xdeadbeefu;
    juce::uint32 next() {
        const juce::uint32 t = x ^ (x << 11);
        x = y;
        y = z;
        z = w;
        w = w ^ (w >> 19) ^ t ^ (t >> 8);
        return w;
    }
    float uniform() { return static_cast<float>(next() >> 8) * (1.0f / 16777216.0f); }
};

juce::Image makeNoise(int size, float strength) {
    juce::Image image(juce::Image::ARGB, size, size, true);
    juce::Image::BitmapData data(image, juce::Image::BitmapData::writeOnly);
    Rng rng;
    for (int y = 0; y < size; ++y) {
        auto* row = reinterpret_cast<juce::PixelARGB*>(data.getLinePointer(y));
        for (int x = 0; x < size; ++x) {
            // Ruido centrado en 0.5 para poderlo pintar con opacidad baja encima de
            // cualquier relleno (multiply-ish): lo que varia es el brillo, no el tono.
            const float n = 0.5f + (rng.uniform() - 0.5f) * strength;
            const auto v = static_cast<juce::uint8>(juce::jlimit(0.0f, 1.0f, n) * 255.0f);
            row[x] = juce::PixelARGB(255, v, v, v);
        }
    }
    return image;
}

}  // namespace

const juce::Image& noiseTile(int size, float strength) {
    static juce::Image tile = makeNoise(128, 1.0f);
    if (tile.getWidth() != size || !juce::approximatelyEqual(strength, 1.0f))
        tile = makeNoise(size, strength);
    return tile;
}

void paintWindow(juce::Graphics& g, juce::Rectangle<int> bounds) {
    // Plastico negro texturizado (el cuadro del Fiat): relleno plano y grano fino.
    g.setColour(colours::plastic);
    g.fillRect(bounds);
    g.setTiledImageFill(noiseTile(), 0, 0, 0.07f);
    g.fillRect(bounds);
}

void paintScreen(juce::Graphics& g, juce::Rectangle<int> bounds) {
    g.setColour(colours::well);
    g.fillRect(bounds);

    // Sombra interior arriba (la pantalla esta hundida respecto a la carcasa).
    juce::ColourGradient topShadow(juce::Colours::black.withAlpha(0.6f), 0.0f,
                                   static_cast<float>(bounds.getY()), juce::Colours::transparentBlack,
                                   0.0f, static_cast<float>(bounds.getY() + 18), false);
    g.setGradientFill(topShadow);
    g.fillRect(bounds.withHeight(18));

    g.setColour(colours::wellEdge);
    g.drawRect(bounds, 1);
}

void paintBand(juce::Graphics& g, juce::Rectangle<int> bounds) {
    g.setColour(colours::bandPlate);
    g.fillRect(bounds);
    g.setTiledImageFill(noiseTile(), 0, 0, 0.05f);
    g.fillRect(bounds);

    // Filo claro arriba (la banda va montada sobre la carcasa) y sombra abajo.
    g.setColour(juce::Colours::white.withAlpha(0.05f));
    g.fillRect(bounds.getX(), bounds.getY(), bounds.getWidth(), 1);
    juce::ColourGradient bottomShadow(
        juce::Colours::transparentBlack, 0.0f, static_cast<float>(bounds.getBottom() - 12),
        juce::Colours::black.withAlpha(0.45f), 0.0f, static_cast<float>(bounds.getBottom()),
        false);
    g.setGradientFill(bottomShadow);
    g.fillRect(bounds.withTop(bounds.getBottom() - 12));
}

void paintLcd(juce::Graphics& g, juce::Rectangle<float> bounds, bool lit, juce::Colour edge) {
    const float radius = juce::jlimit(2.0f, 6.0f, bounds.getHeight() * 0.2f);
    g.setColour(colours::glass);
    g.fillRoundedRectangle(bounds, radius);

    // Cristal hundido: sombra interior desde arriba.
    {
        juce::Graphics::ScopedSaveState state(g);
        juce::Path clip;
        clip.addRoundedRectangle(bounds, radius);
        g.reduceClipRegion(clip);
        const float depth = juce::jmin(10.0f, bounds.getHeight() * 0.45f);
        juce::ColourGradient inset(juce::Colours::black.withAlpha(0.9f), 0.0f, bounds.getY(),
                                   juce::Colours::transparentBlack, 0.0f, bounds.getY() + depth,
                                   false);
        g.setGradientFill(inset);
        g.fillRect(bounds.withHeight(depth));
    }

    const juce::Colour frame = edge.isTransparent() ? colours::glassEdge : edge;
    g.setColour(lit ? frame : frame.withMultipliedAlpha(0.5f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), radius, 1.0f);
}

void paintGlass(juce::Graphics& g, juce::Rectangle<float> bounds) {
    const float radius = juce::jlimit(2.0f, 6.0f, bounds.getHeight() * 0.2f);
    juce::Graphics::ScopedSaveState state(g);
    juce::Path clip;
    clip.addRoundedRectangle(bounds.reduced(1.0f), radius);
    g.reduceClipRegion(clip);

    // Scanlines en una sola llamada (las pantallas se repintan a 60 Hz).
    juce::RectangleList<float> scanlines;
    for (float y = bounds.getY() + 2.0f; y < bounds.getBottom(); y += 3.0f)
        scanlines.addWithoutMerging({bounds.getX(), y, bounds.getWidth(), 1.0f});
    g.setColour(juce::Colours::black.withAlpha(0.28f));
    g.fillRectList(scanlines);

    // Reflejo: banda a 115 grados que ocupa el primer 30 % del ancho.
    const float w = bounds.getWidth() * 0.30f;
    const float lean = bounds.getHeight() * std::tan(juce::degreesToRadians(25.0f));
    juce::Path sheen;
    sheen.startNewSubPath(bounds.getX(), bounds.getY());
    sheen.lineTo(bounds.getX() + w, bounds.getY());
    sheen.lineTo(bounds.getX() + w - lean, bounds.getBottom());
    sheen.lineTo(bounds.getX() - lean, bounds.getBottom());
    sheen.closeSubPath();
    g.setColour(juce::Colours::white.withAlpha(0.045f));
    g.fillPath(sheen);
}

juce::Rectangle<float> paintKey(juce::Graphics& g, juce::Rectangle<float> bounds, float radius,
                                bool hover, float drop) {
    const juce::Rectangle<float> face = bounds.withTrimmedBottom(drop);
    g.setColour(juce::Colours::black);
    g.fillRoundedRectangle(face.translated(0.0f, drop), radius);
    juce::ColourGradient body(colours::keyTop, 0.0f, face.getY(), colours::keyBottom, 0.0f,
                              face.getBottom(), false);
    g.setGradientFill(body);
    g.fillRoundedRectangle(face, radius);
    if (hover) {
        g.setColour(juce::Colours::white.withAlpha(0.06f));
        g.fillRoundedRectangle(face, radius);
    }
    g.setColour(juce::Colours::white.withAlpha(0.08f));
    g.fillRect(face.getX() + radius, face.getY() + 1.0f, face.getWidth() - radius * 2.0f, 1.0f);
    g.setColour(colours::keyEdge);
    g.drawRoundedRectangle(face.reduced(0.5f), radius, 1.0f);
    return face;
}

void paintKeyLight(juce::Graphics& g, juce::Rectangle<float> light, juce::Colour colour, bool on) {
    if (on) {
        g.setColour(colour.withAlpha(0.14f));
        g.fillRoundedRectangle(light.expanded(4.0f), 5.0f);
        g.setColour(colour.withAlpha(0.30f));
        g.fillRoundedRectangle(light.expanded(1.5f), 3.0f);
    }
    g.setColour(on ? colour : colour.withAlpha(0.15f));   // apagado: 15 % (§6)
    g.fillRoundedRectangle(light, 2.0f);
}

void paintHatch(juce::Graphics& g, juce::Rectangle<float> bounds, float amount) {
    if (amount <= 0.0f) return;
    juce::Graphics::ScopedSaveState state(g);
    g.reduceClipRegion(bounds.toNearestIntEdges());
    const float period = 8.0f * std::sqrt(2.0f);  // 4 + 4 px medidos en perpendicular
    const float span = bounds.getWidth() + bounds.getHeight();
    juce::Path dark, light;
    for (float t = -bounds.getHeight(); t < span; t += period) {
        // Franjas a 135 grados: bajan hacia la derecha.
        auto band = [&](juce::Path& p, float offset) {
            const float x0 = bounds.getX() + t + offset;
            p.startNewSubPath(x0, bounds.getY());
            p.lineTo(x0 + period * 0.5f, bounds.getY());
            p.lineTo(x0 + period * 0.5f + bounds.getHeight(), bounds.getBottom());
            p.lineTo(x0 + bounds.getHeight(), bounds.getBottom());
            p.closeSubPath();
        };
        band(dark, 0.0f);
        band(light, period * 0.5f);
    }
    g.setColour(juce::Colours::black.withAlpha(0.58f * amount));
    g.fillPath(dark);
    g.setColour(juce::Colours::white.withAlpha(0.025f * amount));
    g.fillPath(light);
}

void paintEdge(juce::Graphics& g, int y, int x, int width, bool lightAbove) {
    g.setColour(lightAbove ? juce::Colours::white.withAlpha(0.07f)
                           : juce::Colours::black.withAlpha(0.45f));
    g.fillRect(x, y, width, 1);
    g.setColour(lightAbove ? juce::Colours::black.withAlpha(0.45f)
                           : juce::Colours::white.withAlpha(0.07f));
    g.fillRect(x, y + 1, width, 1);
}

}  // namespace rtxx::ui::skin
