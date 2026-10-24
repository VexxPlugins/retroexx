#include "Fonts.h"

#include <array>

#include "BinaryData.h"

namespace rtxx::ui {
namespace {

struct OrbitronWeight {
    int weight;
    const char* data;
    int size;
};

const std::array<OrbitronWeight, 5>& orbitronData() {
    static const std::array<OrbitronWeight, 5> data{{
        {500, BinaryData::Orbitron500_ttf, BinaryData::Orbitron500_ttfSize},
        {600, BinaryData::Orbitron600_ttf, BinaryData::Orbitron600_ttfSize},
        {700, BinaryData::Orbitron700_ttf, BinaryData::Orbitron700_ttfSize},
        {800, BinaryData::Orbitron800_ttf, BinaryData::Orbitron800_ttfSize},
        {900, BinaryData::Orbitron900_ttf, BinaryData::Orbitron900_ttfSize},
    }};
    return data;
}

juce::Typeface::Ptr typefaceForOrbitron(int weight) {
    const auto& table = orbitronData();
    int best = 0;
    for (int i = 0; i < static_cast<int>(table.size()); ++i)
        if (std::abs(table[static_cast<size_t>(i)].weight - weight) <
            std::abs(table[static_cast<size_t>(best)].weight - weight))
            best = i;
    // Un Typeface por peso, compartido: createSystemTypefaceFor cuesta parsear el TTF.
    // Se reserva en el heap y no se libera: JUCE destruye sus cachés de typeface en
    // shutdownJuce_GUI y un static con Typeface::Ptr vivo se destruiria despues.
    static auto& cache = *new std::array<juce::Typeface::Ptr, 5>();
    auto& slot = cache[static_cast<size_t>(best)];
    if (slot == nullptr)
        slot = juce::Typeface::createSystemTypefaceFor(
            table[static_cast<size_t>(best)].data,
            static_cast<size_t>(table[static_cast<size_t>(best)].size));
    return slot;
}

juce::Typeface::Ptr rajdhaniTypeface() {
    static auto& typeface = *new juce::Typeface::Ptr(juce::Typeface::createSystemTypefaceFor(
        BinaryData::Rajdhani500_ttf, static_cast<size_t>(BinaryData::Rajdhani500_ttfSize)));
    return typeface;
}

juce::Font fromTypeface(const juce::Typeface::Ptr& typeface, float height) {
    if (typeface == nullptr) return juce::Font(juce::FontOptions(height));
    return juce::Font(juce::FontOptions(typeface).withHeight(height));
}

}  // namespace

juce::Font orbitron(float height, int weight) {
    return fromTypeface(typefaceForOrbitron(weight), height);
}

juce::Font rajdhani(float height) { return fromTypeface(rajdhaniTypeface(), height); }

bool fontsLoaded() {
    return typefaceForOrbitron(600) != nullptr && rajdhaniTypeface() != nullptr;
}

}  // namespace rtxx::ui
