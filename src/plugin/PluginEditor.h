#pragma once
// Editor del plugin: geometria del motor escalada a 1240x957, con
// la estetica VEXX DASH-90 (../VEXX-DASH90.md manda en lo visual).
//
// Estructura: header con el logo, la ventana VFD del preset (flechas, LOAD, SAVE) y
// MAGNITUDE, y la linea roja; pantalla con las 6 columnas; banda de knobs grandes; marca;
// tira IN | EQ | OUT. Sin footer: los mensajes salen en la linea de la ventana del preset.

#include <juce_gui_extra/juce_gui_extra.h>

#include "PresetManager.h"
#include "ui/HardwareLookAndFeel.h"
#include "ui/MacroBand.h"
#include "ui/MasterStrip.h"
#include "ui/ModulePanel.h"
#include "ui/ParamFader.h"
#include "ui/Theme.h"

namespace rtxx {

class RetroexxAudioProcessor;

class RetroexxEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit RetroexxEditor(RetroexxAudioProcessor& processor);
    ~RetroexxEditor() override;

    void paint(juce::Graphics& g) override;
    void paintOverChildren(juce::Graphics& g) override;
    void resized() override;

    // Acceso de solo lectura para el test de captura (tests/test_ui_snapshot.cpp):
    // comprueba que ningun control se sale del editor ni se solapa.
    int panelCount() const { return modulePanels_.size(); }
    ui::ModulePanel* panel(int index) const;
    ui::MacroBand* macroBand() const { return band_.get(); }
    ui::MasterStrip* masterStrip() const { return strip_.get(); }
    ui::ParamFader* magnitudeFader() const { return magnitude_.get(); }

// Distribucion del motor (para que el test la compruebe, no solo la dibuje):
    //   header  -> marca, preset, MAGNITUDE
    //   pantalla-> 6 columnas en UNA fila
    //   banda   -> 6 knobs grandes (un Amount por columna)
    //   tira    -> GAIN, EQ, WIDTH, GAIN
    juce::Rectangle<int> screenArea() const { return screenArea_; }
    juce::Rectangle<int> bandArea() const { return bandArea_; }
    juce::Rectangle<int> stripArea() const { return stripArea_; }

    // Selector de presets (fábrica + usuario) y botón de guardar. Expuestos para el test de
    // captura, que comprueba que no se solapan con el resto del editor.
    juce::ComboBox& presetBox() { return presetBox_; }
    juce::TextButton& savePresetButton() { return saveButton_; }
    juce::TextButton& loadPresetButton() { return loadButton_; }
    juce::Rectangle<int> presetWindow() const { return presetWindow_; }
    void refreshPresetList();
    void applyPresetSelection(int itemId);

private:
    void timerCallback() override;
    void updateStatusText();
    void updateMeters();
    void syncPresetBoxToHost();
    void saveCurrentAsPreset();
    void stepPreset(int delta);
    void paintHeader(juce::Graphics& g) const;

    RetroexxAudioProcessor& processor_;
    std::unique_ptr<juce::Drawable> logoMark_;
    std::unique_ptr<ui::MacroBand> band_;
    std::unique_ptr<ui::MasterStrip> strip_;
    std::unique_ptr<ui::ParamFader> magnitude_;
    juce::OwnedArray<ui::ModulePanel> modulePanels_;

    juce::Rectangle<int> headerArea_;
    juce::Rectangle<int> screenArea_;   // pantalla hundida con las 6 columnas
    juce::Rectangle<int> bandArea_;     // banda con los 6 knobs grandes
    juce::Rectangle<int> stripArea_;    // tira GAIN / EQ / WIDTH / GAIN
    juce::Rectangle<int> logoArea_;
    juce::Rectangle<int> presetWindow_; // ventana VFD: linea de estado + selector
    juce::Rectangle<int> brandArea_;    // marca entre la banda y la tira
    juce::Rectangle<int> wordmarkArea_;
    juce::Rectangle<int> subtitleArea_;
    juce::String statusText_;   // error del motor (no dibujable como lectura)
    juce::String message_;      // "preset guardado: ..." y similares (linea del preset)

    // --- presets -------------------------------------------------------------------
    // Los de fábrica son programas del host (ids 1..N); los de usuario, ficheros en disco.
    juce::ComboBox presetBox_;
    juce::ArrowButton prevButton_{"preset anterior", 0.75f, ui::colours::lcdInk};
    juce::ArrowButton nextButton_{"preset siguiente", 0.25f, ui::colours::lcdInk};
    juce::TextButton loadButton_{"LOAD"};
    juce::TextButton saveButton_{"SAVE"};
    PresetManager presets_;
    ui::HardwareLookAndFeel lookAndFeel_;
    std::vector<PresetManager::Entry> userPresets_;
    int lastProgramShown_ = -1;
    bool updatingBox_ = false;
    bool poweredOn_ = false;   // barrido de encendido ya lanzado

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RetroexxEditor)
};

}  // namespace rtxx
