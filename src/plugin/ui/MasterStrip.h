#pragma once
// Tira inferior del aparato: GAIN de entrada, EQ (corte bajo/alto con sus modos y el Tone),
// WIDTH y GAIN de salida. Es la barra que El motor pone debajo de los knobs grandes, con
// la misma particion: IN | EQ | OUT, rotulos verticales y los modos como pares de teclas
// apiladas.
//
// No inventa controles: los 9 parametros que no son de ninguna columna (48, 50..57) mas el
// Enable del EQ, que es la puerta de todo el grupo.

#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>

#include "HostModules.h"
#include "ModulePanel.h"
#include "ParamToggle.h"

namespace rtxx::ui {

class MasterStrip : public juce::Component {
public:
    explicit MasterStrip(const HostParamLookup& lookup);

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Repinta las lecturas de los knobs cuyo texto cambio (lo llama el timer del editor).
    void refreshReadouts();

    int knobCount() const { return static_cast<int>(knobs_.size()); }
    ParamKnob* knob(int index) const { return knobs_[index]; }
    // Ids de host que edita la tira (para el test de cobertura de los 58).
    const std::vector<int>& hostIds() const { return hostIds_; }
    void setControlsEnabled(bool shouldBeEnabled);

private:
    struct Readout {
        ParamKnob* knob;
        juce::RangedAudioParameter* parameter;
        const char* label;
        juce::String text;
    };
    juce::Rectangle<int> readoutArea(const Readout& readout) const;

    std::unique_ptr<ParamKnob> inGain_;
    std::unique_ptr<ParamKnob> outGain_;
    std::unique_ptr<ParamKnob> width_;
    std::unique_ptr<ParamKnob> tone_;
    std::unique_ptr<ParamToggle> eqEnable_;
    std::unique_ptr<ParamRangeSlider> eqCut_;   // CUT: dos tiradores (lo/hi)
    juce::OwnedArray<ParamButton> modeButtons_;// modo del corte bajo, del alto y del Tone
    std::vector<int> hostIds_;
    // Vista NO propietaria de todos los knobs (los duenos son los unique_ptr). Un
    // OwnedArray aqui borraria dos veces.
    std::vector<ParamKnob*> knobs_;
    std::vector<Readout> readouts_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MasterStrip)
};

}  // namespace rtxx::ui
