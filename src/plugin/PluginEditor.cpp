#include "PluginEditor.h"

#include <cmath>

#include "BinaryData.h"
#include "ui/Fonts.h"
#include "ui/PowerOn.h"
#include "ui/ReduceMotion.h"
#include "HostModules.h"
#include "PluginProcessor.h"
#include "ui/SegmentDisplay.h"
#include "ui/Skin.h"
#include "ui/Theme.h"
#include "product/ProductInfo.h"

namespace rtxx {
namespace {

// Medidas del editor: el lienzo del motor (797x615) por ui::geometry::kScale.
constexpr int kEditorWidth = 1240;
constexpr int kEditorHeight = 957;
constexpr int kHeaderHeight = 107;   // la linea roja va justo debajo (107..109)
constexpr int kStripTop = 836;       // tira IN | EQ | OUT hasta el borde inferior
constexpr int kLogoSize = 40;

constexpr int kPageMargin = ui::metrics::pageMargin;

// Header.
const juce::Rectangle<int> kPresetWindow(386, 44, 342, 50);
const juce::Rectangle<int> kPresetBox(390, 58, 334, 34);
const juce::Rectangle<int> kPrevButton(732, 45, 34, 23);
const juce::Rectangle<int> kNextButton(732, 70, 34, 23);
const juce::Rectangle<int> kLoadButton(772, 47, 48, 44);
const juce::Rectangle<int> kSaveButton(824, 47, 48, 44);
const juce::Rectangle<int> kMagnitude(918, 40, 240, 52);


}  // namespace

RetroexxEditor::RetroexxEditor(RetroexxAudioProcessor& owner)
    : juce::AudioProcessorEditor(owner), processor_(owner) {
    setOpaque(true);
    setResizable(false, false);
    juce::LookAndFeel::setDefaultLookAndFeel(&lookAndFeel_);

    logoMark_ = juce::Drawable::createFromImageData(BinaryData::logomark_svg,
                                                   BinaryData::logomark_svgSize);

    const ui::HostParamLookup lookup = [this](int hostId) -> juce::RangedAudioParameter& {
        return *processor_.hostParameter(hostId);
    };
    band_ = std::make_unique<ui::MacroBand>(lookup);
    addAndMakeVisible(*band_);
    strip_ = std::make_unique<ui::MasterStrip>(lookup);
    addAndMakeVisible(*strip_);
    magnitude_ = std::make_unique<ui::ParamFader>(lookup(kMagnitudeId), ui::colours::vfd,
                                                  kMagnitudeId);
    magnitude_->setBarMode(ui::SegBarMode::Wedge);
    magnitude_->setLabel("MAGNITUDE");
    addAndMakeVisible(*magnitude_);

    for (int i = 0; i < ModuleTable::count(); ++i) {
        auto* panel = modulePanels_.add(new ui::ModulePanel(ModuleTable::all()[i], lookup));
        // Indice del motor: la columna 5 (MAGNETIC) es el nodo 6, el 5 es el EQ.
        panel->setVizTap(processor_.vizTap(i == 5 ? 6 : i));
        addAndMakeVisible(*panel);
    }

    // Si el grafo no cerró, no hay audio que editar: los controles pasan a disabled
    // (DESIGN.md §6 exige ese estado) y la ventana del preset dice el motivo.
    if (!processor_.engineError().isEmpty()) {
        band_->setControlsEnabled(false);
        strip_->setControlsEnabled(false);
        magnitude_->setEnabled(false);
        for (auto* module : modulePanels_) module->setControlsEnabled(false);
    }

    // --- presets: los de fábrica son programas del host, los de usuario son ficheros ----
    presetBox_.setTooltip("Presets de fábrica y del usuario");
    presetBox_.onChange = [this] { applyPresetSelection(presetBox_.getSelectedId()); };
    addAndMakeVisible(presetBox_);
    saveButton_.setTooltip("Guardar el estado actual como preset de usuario");
    saveButton_.onClick = [this] { saveCurrentAsPreset(); };
    addAndMakeVisible(saveButton_);
    loadButton_.setTooltip("Abrir la lista de presets");
    loadButton_.onClick = [this] { presetBox_.showPopup(); };
    addAndMakeVisible(loadButton_);
    prevButton_.setTooltip("Preset anterior");
    prevButton_.onClick = [this] { stepPreset(-1); };
    addAndMakeVisible(prevButton_);
    nextButton_.setTooltip("Preset siguiente");
    nextButton_.onClick = [this] { stepPreset(1); };
    addAndMakeVisible(nextButton_);
    refreshPresetList();
    syncPresetBoxToHost();

    // setSize después de construir los paneles: resized() los coloca.
    setSize(kEditorWidth, kEditorHeight);
    updateStatusText();
    updateMeters();     // primer nivel sin esperar al timer
    band_->refreshReadouts();   // primeras lecturas sin esperar al timer
    strip_->refreshReadouts();
    startTimerHz(60);  // animaciones; cada zona se repinta solo si cambia lo que dibuja
}

RetroexxEditor::~RetroexxEditor() {
    juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
}

ui::ModulePanel* RetroexxEditor::panel(int index) const {
    const int moduleIndex = index;
    if (moduleIndex < 0 || moduleIndex >= modulePanels_.size()) return nullptr;
    return modulePanels_[moduleIndex];
}

void RetroexxEditor::refreshPresetList() {
    updatingBox_ = true;
    presetBox_.clear(juce::dontSendNotification);
    // ids 1..N: fábrica (== programas del host). Después, separador y los del usuario.
    const int factory = processor_.getNumPrograms();
    for (int p = 0; p < factory; ++p)
        presetBox_.addItem(processor_.getProgramName(p), p + 1);
    userPresets_ = presets_.scan();
    if (!userPresets_.empty()) {
        presetBox_.addSeparator();
        for (size_t i = 0; i < userPresets_.size(); ++i)
            presetBox_.addItem(userPresets_[i].name,
                               factory + 1 + static_cast<int>(i));
    }
    updatingBox_ = false;
}

void RetroexxEditor::applyPresetSelection(int itemId) {
    if (updatingBox_ || itemId <= 0) return;
    const int factory = processor_.getNumPrograms();
    if (itemId <= factory) {
        processor_.setCurrentProgram(itemId - 1);
        lastProgramShown_ = itemId - 1;
        return;
    }
    const size_t index = static_cast<size_t>(itemId - factory - 1);
    if (index >= userPresets_.size()) return;
    juce::MemoryBlock state;
    juce::String error;
    if (!presets_.load(userPresets_[index].file, state, error)) {
        message_ = "preset: " + error;
        repaint(presetWindow_);
        return;
    }
    processor_.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
}

void RetroexxEditor::saveCurrentAsPreset() {
    auto* window = new juce::AlertWindow("Guardar preset",
                                         "Nombre del preset (se guarda en "
                                             + presets_.root().getFullPathName() + "):",
                                         juce::MessageBoxIconType::NoIcon, this);
    window->addTextEditor("name", processor_.getProgramName(processor_.getCurrentProgram()));
    window->addButton("Guardar", 1, juce::KeyPress(juce::KeyPress::returnKey));
    window->addButton("Cancelar", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    window->enterModalState(true, juce::ModalCallbackFunction::create(
        [this, window](int result) {
            std::unique_ptr<juce::AlertWindow> owner(window);
            if (result != 1) return;
            const juce::String name = owner->getTextEditorContents("name");
            juce::MemoryBlock state;
            processor_.getStateInformation(state);
            juce::String error;
            if (!presets_.save(name, state, error)) {
                message_ = "preset: " + error;
                repaint(presetWindow_);
                return;
            }
            refreshPresetList();
            // Deja seleccionado el preset recién guardado.
            for (size_t i = 0; i < userPresets_.size(); ++i)
                if (userPresets_[i].name == PresetManager::sanitiseName(name)) {
                    updatingBox_ = true;
                    presetBox_.setSelectedId(processor_.getNumPrograms() + 1 + static_cast<int>(i),
                                             juce::dontSendNotification);
                    updatingBox_ = false;
                }
            message_ = "preset guardado: " + name;
            repaint(presetWindow_);
        }),
        false);
}

void RetroexxEditor::stepPreset(int delta) {
    const int count = presetBox_.getNumItems();
    if (count <= 0) return;
    const int current = presetBox_.getSelectedItemIndex();
    const int next = current < 0 ? 0 : (current + delta + count) % count;
    presetBox_.setSelectedItemIndex(next, juce::sendNotificationSync);
}

void RetroexxEditor::syncPresetBoxToHost() {
    const int program = processor_.getCurrentProgram();
    if (program == lastProgramShown_ && presetBox_.getSelectedId() > 0) return;
    lastProgramShown_ = program;
    updatingBox_ = true;
    presetBox_.setSelectedId(program + 1, juce::dontSendNotification);
    updatingBox_ = false;
}

void RetroexxEditor::updateStatusText() {
    // "OS 1x" es el estado real: el motor procesa a la frecuencia del host (no hay etapa
    // de oversampling instanciada todavía; el factor de src/dsp/Oversampling.h entrará
    // con la fase 8). No se muestra un factor que el audio no esté usando.
    // Solo lo que NO se puede dibujar como lectura digital: el error del motor.
    const juce::String text = processor_.engineError().isEmpty()
                                  ? juce::String()
                                  : "engine: " + processor_.engineError();
    if (text == statusText_) return;
    statusText_ = text;
    repaint(presetWindow_);
}

void RetroexxEditor::updateMeters() {
    // Vumetros: el nivel sale del motor (RMS por nodo), en dBFS mapeado a 0..1 con -60 dB
    // como suelo. Se repinta solo la tira de cada columna.
    for (int i = 0; i < modulePanels_.size(); ++i) {
        const float rms = processor_.moduleLevel(i);
        const float db = juce::Decibels::gainToDecibels(rms, -60.0f);
        modulePanels_[i]->setLevel(juce::jlimit(0.0f, 1.0f, (db + 60.0f) / 60.0f));
    }
}

void RetroexxEditor::timerCallback() {
    // Barrido de encendido la primera vez que el editor se ve (§9).
    if (!poweredOn_ && isShowing()) {
        poweredOn_ = true;
        if (!ui::reduceMotion()) ui::power::start();
    }
    if (ui::power::active()) {
        ui::power::advance(1000.0 / 60.0);
        repaint();
    }

    updateStatusText();
    syncPresetBoxToHost();
    updateMeters();
    for (auto* module : modulePanels_) {
        module->updateTripComputer();
        module->advanceAnimation(1.0f / 60.0f);
    }
    band_->refreshReadouts();
    strip_->refreshReadouts();
}

void RetroexxEditor::resized() {
    if (band_ == nullptr) return;

// Posiciones absolutas del motor x kScale (ver HostModules.h y DESIGN.md).
    headerArea_ = getLocalBounds().withHeight(kHeaderHeight);
    screenArea_ = ui::geometry::scaled(12.0f, 78.0f, 776.0f, 307.0f);
    bandArea_ = ui::geometry::scaled(8.0f, 385.0f, 777.0f, 124.0f);
    stripArea_ = juce::Rectangle<int>(0, kStripTop, getWidth(), getHeight() - kStripTop);
    brandArea_ = juce::Rectangle<int>(0, bandArea_.getBottom(), getWidth(),
                                      kStripTop - bandArea_.getBottom());

    for (int i = 0; i < modulePanels_.size(); ++i)
        modulePanels_[i]->setBounds(ui::ModulePanel::boundsInEditor(i));
    band_->setBounds(ui::MacroBand::boundsInEditor());
    strip_->setBounds(stripArea_);

    presetWindow_ = kPresetWindow;
    presetBox_.setBounds(kPresetBox);
    prevButton_.setBounds(kPrevButton.reduced(8, 5));
    nextButton_.setBounds(kNextButton.reduced(8, 5));
    loadButton_.setBounds(kLoadButton);
    saveButton_.setBounds(kSaveButton);
    magnitude_->setBounds(kMagnitude);

    const juce::Font wordmarkFont = ui::orbitron(ui::typeHeadline, 800).withExtraKerningFactor(0.14f);
    const juce::Font subtitleFont = ui::orbitron(10.0f, 600).withExtraKerningFactor(0.30f);
    const float wordmarkWidth =
        juce::GlyphArrangement::getStringWidth(wordmarkFont, juce::String("RETROEXX"));
    const float subtitleWidth =
        juce::GlyphArrangement::getStringWidth(subtitleFont, juce::String("VEXX AUDIO"));
    const int textWidth = static_cast<int>(std::ceil(juce::jmax(wordmarkWidth, subtitleWidth))) + 4;
    const int gap = 12;
    const int groupX = kPageMargin;
    const int groupY = (kHeaderHeight - kLogoSize) / 2;

    logoArea_ = juce::Rectangle<int>(groupX, groupY, kLogoSize, kLogoSize);
    const int textX = groupX + kLogoSize + gap;
    const int wordmarkHeight = 26;
    wordmarkArea_ = juce::Rectangle<int>(textX, groupY - 3, textWidth, wordmarkHeight);
    subtitleArea_ = juce::Rectangle<int>(textX, wordmarkArea_.getBottom() + 1, textWidth, 14);
}

void RetroexxEditor::paintHeader(juce::Graphics& g) const {
    // Tira elevada del header en plasticHi (VEXX-DASH90 §10).
    g.setColour(ui::colours::plasticHi);
    g.fillRect(headerArea_);
    if (logoMark_ != nullptr)
        logoMark_->drawWithin(g, logoArea_.toFloat(), juce::RectanglePlacement::centred, 1.0f);

    const juce::Font wordmarkFont = ui::orbitron(ui::typeHeadline, 800).withExtraKerningFactor(0.14f);
    g.setFont(wordmarkFont);
    g.setColour(ui::colours::ink);
    g.drawText("RETROEXX", wordmarkArea_, juce::Justification::centredLeft, false);

    const juce::Font subtitleFont = ui::orbitron(10.0f, 600).withExtraKerningFactor(0.30f);
    g.setFont(subtitleFont);
    g.setColour(ui::colours::accent);
    g.drawText("VEXX AUDIO", subtitleArea_, juce::Justification::centredLeft, false);

    // Ventana VFD del preset: linea de estado arriba (PRESET, el ultimo mensaje o el error
    // del motor en ambar) y el selector debajo.
    ui::skin::paintLcd(g, presetWindow_.toFloat(), true);
    const bool engineOk = statusText_.isEmpty();
    const juce::String line = !engineOk ? statusText_
                              : message_.isNotEmpty() ? message_.toUpperCase()
                                                      : juce::String("PRESET");
    g.setColour(engineOk ? ui::colours::lcdInkDim : ui::colours::amber);
    g.setFont(ui::rajdhani(10.0f).withExtraKerningFactor(0.20f));
    g.drawText(line, presetWindow_.withTrimmedLeft(10).withTrimmedRight(8).withHeight(14),
               juce::Justification::centredLeft, true);

// Filo del header: luz arriba, y la linea ROJA de 2 px.
    ui::skin::paintEdge(g, headerArea_.getBottom() - 2, 0, getWidth(), false);
    g.setColour(ui::colours::redLine);
    g.fillRect(0, headerArea_.getBottom(), getWidth(), ui::metrics::accentLine);
}

void RetroexxEditor::paintOverChildren(juce::Graphics& g) {
    // Cristal de la ventana del preset por encima del selector (capas 4-5 de §5.5).
    ui::skin::paintGlass(g, presetWindow_.toFloat());
}

void RetroexxEditor::paint(juce::Graphics& g) {
    ui::skin::paintWindow(g, getLocalBounds());

    // Pantalla con las 6 columnas, banda de knobs grandes y tira IN | EQ | OUT.
    ui::skin::paintScreen(g, screenArea_);
    ui::skin::paintBand(g, bandArea_);
    g.setColour(ui::colours::strip);
    g.fillRect(stripArea_);
    g.setColour(ui::colours::hairline);
    g.fillRect(stripArea_.getX(), stripArea_.getY(), stripArea_.getWidth(), 1);

    // Pie de marca (VEXX-DASH90 §10): VEXX AUDIO en brandGold, pequeno y centrado; la
    // version a la derecha en tinta apagada.
    g.setColour(ui::colours::brandGold);
    g.setFont(ui::orbitron(10.0f, 600).withExtraKerningFactor(0.40f));
    g.drawText("VEXX AUDIO", brandArea_, juce::Justification::centred, false);
    g.setColour(ui::colours::inkMuted);
    g.setFont(ui::rajdhani(ui::typeSmall));
    g.drawText(juce::String("v") + kVersion, brandArea_.withTrimmedRight(kPageMargin),
               juce::Justification::centredRight, false);

    paintHeader(g);
}

}  // namespace rtxx
