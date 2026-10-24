#pragma once
// Una columna de la pantalla: replica la columna del motor control a control.
//
// Cada control va en la posicion ABSOLUTA que tiene en la tabla (tabla `columnLayout`
// de HostModules.cpp, coordenadas del motor x ui::geometry::kScale). El panel escribe
// las leyendas debajo de cada control; los controles no llevan LCD propio: el valor se lee
// en la pantalla de la columna, que pasa a "ordenador de viaje" mientras el raton esta
// sobre un control o lo arrastra (VEXX-DASH90 §7).
//
// El Enable del modulo NO esta aqui: vive en la banda de knobs grandes, como en el
// motor. La columna solo lo escucha para atenuarse cuando el modulo esta apagado.

#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>

#include "HostModules.h"
#include "block/VizTap.h"
#include "ParamButton.h"
#include "ParamFader.h"
#include "ParamKnob.h"
#include "ParamRangeSlider.h"
#include "ParamSelector.h"
#include "Theme.h"

namespace rtxx::ui {

// Acceso a los parametros del processor por id de host (0..57).
using HostParamLookup = std::function<juce::RangedAudioParameter&(int hostId)>;

class ModulePanel : public juce::Component {
public:
    ModulePanel(const ModuleSpec& spec, const HostParamLookup& lookup);
    ~ModulePanel() override;

// Rect de la columna `index` en El motor (lienzo 797 x 615): x = 15 + 128 i, y = 74,
    // 124 de ancho y 310 de alto (hasta el fondo de la pantalla, FLUX incluido).
    static juce::Rectangle<int> boundsInEditor(int index);

    void paint(juce::Graphics& g) override;
    void paintOverChildren(juce::Graphics& g) override;
    void resized() override;

    // Animaciones de la piel (VEXX-DASH90 §9): fundido del rayado de apagado (150 ms) y pulso
    // del halo de FLUX (1,85 s). Lo llama el timer del editor con el tiempo transcurrido.
    void advanceAnimation(float seconds);

    // Zonas (coordenadas del panel) que repinto el ultimo advanceAnimation;
    // las mide el banco de rendimiento del test de captura.
    const juce::RectangleList<int>& lastDirty() const { return lastDirty_; }

    // Ids de host que edita esta columna (para el test de cobertura).
    const std::vector<int>& hostIds() const { return hostIds_; }

    // Nivel (RMS) de la salida del modulo, 0..1; lo usa el parpadeo de DIST.
    void setLevel(float normalised);

    // Toma de audio del motor para el viz de NOISE, SPACE y MAGNETIC (nullptr en el resto).
    void setVizTap(VizTap* tap);

    // Ordenador de viaje: mira que control tiene el raton encima o se esta arrastrando y,
    // si cambia el control o su valor, repinta la pantalla. Lo llama el timer del editor.
    void updateTripComputer();
    // Fija la lectura en el control `index` de la columna (-1 la apaga). La usa
    // updateTripComputer y el test de captura, que no tiene raton.
    void showTripComputer(int index);
    bool tripComputerActive() const { return tripIndex_ >= 0; }

    bool isOn() const;

    int knobCount() const { return knobs_.size(); }
    ParamKnob* knob(int index) const { return knobs_[index]; }

    void setControlsEnabled(bool shouldBeEnabled);

private:
    // Pantalla de la columna como hijo opaco: su repintado (viz a 60 Hz, ordenador de viaje)
    // no vuelve a pintar la carcasa ni los controles (fase 5). Pinta el panel.
    class Screen : public juce::Component {
    public:
        explicit Screen(ModulePanel& panel) : panel_(panel) {
            setOpaque(true);
            setInterceptsMouseClicks(false, false);
            setName("viz");
        }
        void paint(juce::Graphics& g) override { panel_.paintScreen(g); }

    private:
        ModulePanel& panel_;
    };

    struct Entry {
        juce::Component* component;
        const ControlSpec* spec;
        juce::RangedAudioParameter* a;
        juce::RangedAudioParameter* b;
    };

    void paintCaptions(juce::Graphics& g, bool on) const;
    void paintTripComputer(juce::Graphics& g) const;
    void paintScreen(juce::Graphics& g) const;
    void paintFlux(juce::Graphics& g, bool on) const;
    void paintViz(juce::Graphics& g, juce::Rectangle<float> area, bool on) const;
    float vizValue(size_t index) const { return vizParams_[index]->getValue(); }
    void applyControlAlpha();
    void markDirty(juce::Rectangle<int> area);
    float fluxOpacity() const;
    float distFlicker() const;

    ModuleSpec spec_;
    int moduleIndex_ = 0;   // indice en ModuleTable: lo fija el constructor, no la direccion
    float level_ = 0.0f;

    juce::RangedAudioParameter* enable_ = nullptr;
    std::unique_ptr<juce::ParameterAttachment> enableAttachment_;
    juce::OwnedArray<ParamKnob> knobs_;
    juce::OwnedArray<ParamButton> buttons_;
    juce::OwnedArray<ParamFader> faders_;
    juce::OwnedArray<ParamRangeSlider> ranges_;
    juce::OwnedArray<ParamSelector> selectors_;
    std::vector<Entry> entries_;
    std::vector<int> hostIds_;

    Screen screen_{*this};
    // Capa fija de la pantalla (§8): el fondo LCD, a la escala fisica de la ventana; se rehace
    // si cambia el tamano, la escala o el encendido.
    mutable juce::Image screenUnder_;
    mutable float screenScale_ = 0.0f;
    mutable bool screenOn_ = false;
    juce::Rectangle<int> displayArea_;
    juce::Rectangle<int> podArea_;

    int fluxEntry_ = -1;     // la fila FLUX (comun a todas las columnas)
    float hatch_ = 0.0f;     // 0 = encendido, 1 = rayado completo
    float fluxPhase_ = 0.0f; // fase del pulso de FLUX, 0..1

    // Visualizador por parametros (fase 3: WOBBLE, DIST, DIGITAL; vacio en el resto).
    juce::RangedAudioParameter* amount_ = nullptr;
    std::vector<juce::RangedAudioParameter*> vizParams_;
    float wowPhase_ = 0.0f, flutterPhase_ = 0.0f;   // radianes
    float distClock_ = 0.0f;                        // reloj del parpadeo, 0..0,7 s
    float vizSignature_ = -1.0f;

    // Viz por audio (fase 4): ultimas kHistory tramas de la toma en anillo, y su copia en
    // orden para pintar (preasignadas: el timer no reserva memoria).
    static constexpr int kHistory = 512;
    VizTap* tap_ = nullptr;
    std::vector<VizFrame> history_;
    mutable std::vector<VizFrame> ordered_;
    int historyHead_ = 0;
    float sinceRefresh_ = 0.0f;   // con "reducir movimiento" se refresca una vez por segundo
    float vizClock_ = 0.0f;       // segundos acumulados desde el ultimo paso de los viz (15 Hz)
    int pendingFrames_ = 0;       // tramas llegadas desde el ultimo repintado
    int fluxGlow_ = -1;           // alfa del halo de FLUX ya pintado (8 bits), -1 sin halo
    juce::RectangleList<int> lastDirty_;

    int tripIndex_ = -1;
    juce::String tripName_;
    juce::String tripValue_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModulePanel)
};

}  // namespace rtxx::ui
