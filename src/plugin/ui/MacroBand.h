#pragma once
// Banda de knobs grandes: un knob por columna, con el Amount (macro) de cada modulo y su
// cuadrado de encendido encima, como la franja inferior del motor.
//
// Los seis macros NO se repiten en las columnas: viven aqui, que es donde el usuario los
// busca. El encendido de cada modulo tambien vive SOLO aqui (en El motor la columna no
// tiene interruptor). Cada placa va alineada con su columna de la pantalla.
//
// Cada placa: encendido arriba a la izquierda, lectura fija del Amount en 7 segmentos arriba
// a la derecha, knob grande con aro VFD y el nombre del modulo grabado debajo.

#include <juce_gui_basics/juce_gui_basics.h>

#include "HostModules.h"
#include "ModulePanel.h"
#include "ParamToggle.h"

namespace rtxx::ui {

class MacroBand : public juce::Component {
public:
    explicit MacroBand(const HostParamLookup& lookup);

// Rect de la banda en el editor.
    static juce::Rectangle<int> boundsInEditor();

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Repinta las lecturas cuyo texto cambio (lo llama el timer del editor).
    void refreshReadouts();

    int macroCount() const { return knobs_.size(); }
    ParamKnob* macroKnob(int index) const { return knobs_[index]; }
    ParamToggle* macroEnable(int index) const { return enables_[index]; }
    void setControlsEnabled(bool shouldBeEnabled);

private:
    juce::Rectangle<int> plateArea(int index) const;
    juce::Rectangle<int> readoutArea(int index) const;

    juce::OwnedArray<ParamKnob> knobs_;
    juce::OwnedArray<ParamToggle> enables_;
    std::vector<juce::RangedAudioParameter*> amounts_;
    std::vector<juce::RangedAudioParameter*> enableParams_;
    juce::StringArray readouts_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MacroBand)
};

}  // namespace rtxx::ui
