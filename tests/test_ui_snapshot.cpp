// Captura renderizada del editor (fase 9): crea el AudioProcessor y el editor reales sin
// abrir ventana, comprueba la geometria (nada fuera del editor, nada solapado) y escribe
// el PNG en test/ui/editor.png para revisarlo a ojo.
//
// No es una prueba de "se dibuja algo": falla si falta una fuente del contrato, si un
// control se sale del editor o si dos controles comparten pixeles.
#include <juce_gui_extra/juce_gui_extra.h>

#include <cmath>
#include <vector>

#include "ui/Fonts.h"
#include "HostModules.h"
#include "PluginEditor.h"
#include "model/FactoryPresets.h"
#include "PluginProcessor.h"
#include "dsp/Check.h"
#include "ui/Theme.h"
#include "ui/ParamKnob.h"
#include "ui/ParamSelector.h"
#include "ui/PowerOn.h"

namespace {

juce::Rectangle<int> boundsInEditor(juce::Component& component, juce::Component& editor) {
    juce::Point<int> offset(0, 0);
    juce::Component* current = &component;
    while (current != nullptr && current != &editor) {
        offset += current->getPosition();
        current = current->getParentComponent();
    }
    if (current == nullptr) return {};
    return juce::Rectangle<int>(offset.x, offset.y, component.getWidth(), component.getHeight());
}

void checkHeaderPalette(const juce::Image& image, juce::Rectangle<int> band, int redLineY) {
    // Paleta v3: chapa casi negra, banda de grafito, y lecturas en LCD verde-azulado de
    // 7 segmentos. Nada de crema: el plugin es oscuro de arriba a abajo.
    const juce::Colour window = rtxx::ui::colours::window;

    const juce::Colour corner = image.getPixelAt(2, 2);
    CHECK(std::abs(corner.getRed() - window.getRed()) <= 8);
    CHECK(std::abs(corner.getBlue() - window.getBlue()) <= 8);
    CHECK(corner.getBlue() >= corner.getRed());  // sesgo azul, como la piel medida

    // Linea roja bajo el header (el rojo estructural del editor; ya no hay footer).
    CHECK(image.getPixelAt(image.getWidth() / 2, redLineY) == rtxx::ui::colours::redLine);
    CHECK(image.getPixelAt(image.getWidth() / 2, redLineY + 1) == rtxx::ui::colours::redLine);

    // La banda del MASTER: oscura (no puede haber una fila clara cruzándola). Se mira una
    // franja del centro de la banda, lejos de los knobs.
    int brightBandRows = 0;
    for (int y = band.getY(); y < band.getBottom(); ++y) {
        const juce::Colour pixel = image.getPixelAt(band.getX() + 4, y);  // margen izquierdo
        if (pixel.getBrightness() > 0.45f) ++brightBandRows;
    }
    CHECK(brightBandRows == 0);

    // LCD verde-azulado: tiene que haber segmentos encendidos (G y B altos, R bajo) tanto
    // en los modulos como en el MASTER. Se cuentan en toda la imagen, no en el header.
    // Cada modulo tiene su color y el rojo es el dominante: la
    // columna NOISE. Se cuentan los pixeles con dominante roja dentro de su panel.
    int lcdPixels = 0;
    int redPixels = 0;
    int goldPixels = 0;
    for (int y = 0; y < image.getHeight(); ++y) {
        for (int x = 0; x < image.getWidth(); ++x) {
            const juce::Colour pixel = image.getPixelAt(x, y);
            const auto r = pixel.getRed(), g = pixel.getGreen(), b = pixel.getBlue();
            if (g > 130 && b > 110 && r < g - 40) ++lcdPixels;   // verde/azul de los LCD
            if (r > 110 && r > g + 45 && r > b + 45) ++redPixels;
            if (pixel == rtxx::ui::colours::accent) ++goldPixels;
        }
    }
    std::printf("  color: %d px de LCD, %d px rojos, %d px dorados, %d filas claras en la banda\n",
                lcdPixels, redPixels, goldPixels, brightBandRows);
    CHECK(lcdPixels > 400);   // aros VFD de la banda + lecturas en segmentos
    CHECK(redPixels > 1200);  // columna NOISE (lavado, LED, aros y digitos) + lineas rojas
    // Dorado SOLO en la marca (VEXX-DASH90 §3.3): subtitulo VEXX AUDIO del header y pie de
    // marca (~42 px exactos). Antes contaba tambien los LED dorados de los ON/OFF (110 px);
    // DASH-90 los cambio por LED cuadrados del color de cada modulo.
    CHECK(goldPixels > 30);
}}  // namespace

int main() {
    juce::ScopedJuceInitialiser_GUI guiInit;

    CHECK(rtxx::ui::fontsLoaded());

    rtxx::RetroexxAudioProcessor processor;
    CHECK(processor.engineError().isEmpty());
    processor.setRateAndBufferSizeDetails(44100.0, 512);
    processor.prepareToPlay(44100.0, 512);

    // Audio real antes de capturar: asi los vumetros muestran niveles de verdad y el
    // PNG no sale con las tiras apagadas (que seria una captura mentirosa del estado).
    {
        juce::AudioBuffer<float> block(2, 512);
        juce::MidiBuffer midi;
        for (int b = 0; b < 40; ++b) {
            block.clear();
            for (int i = 0; i < block.getNumSamples(); ++i) {
                const float v = 0.25f * std::sin(juce::MathConstants<float>::twoPi * 1000.0f *
                                                static_cast<float>(b * 512 + i) / 44100.0f);
                block.setSample(0, i, v);
                block.setSample(1, i, v);
            }
            processor.processBlock(block, midi);
        }
        int litModules = 0;
        for (int i = 0; i < rtxx::ModuleTable::count(); ++i)
            if (processor.moduleLevel(i) > 0.001f) ++litModules;
        std::printf("  vumetros con audio: %d de %d modulos\n", litModules,
                    rtxx::ModuleTable::count());
        CHECK(litModules >= 6);  // todos menos los que tienen enable apagado por defecto
    }

    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    CHECK(editor != nullptr);
    if (editor == nullptr) return check::finish("test_ui_snapshot");

    auto* retroexxEditor = dynamic_cast<rtxx::RetroexxEditor*>(editor.get());
    CHECK(retroexxEditor != nullptr);
    if (retroexxEditor == nullptr) return check::finish("test_ui_snapshot");

    editor->setSize(editor->getWidth(), editor->getHeight());

    // Geometria: cada knob dentro del editor y sin solaparse con otro.
    const juce::Rectangle<int> editorBounds(0, 0, editor->getWidth(), editor->getHeight());
    std::vector<juce::Rectangle<int>> knobBounds;
    int knobCount = 0;
    for (int panelIndex = 0; panelIndex < retroexxEditor->panelCount(); ++panelIndex) {
        auto* panel = retroexxEditor->panel(panelIndex);
        CHECK(panel != nullptr);
        if (panel == nullptr) continue;
        CHECK(panel->getBounds().getRight() <= editorBounds.getRight());
        CHECK(panel->getBounds().getBottom() <= editorBounds.getBottom());
        knobCount += panel->knobCount();
        for (int k = 0; k < panel->knobCount(); ++k) {
            const auto bounds = boundsInEditor(*panel->knob(k), *editor);
            CHECK(bounds.getWidth() >= 40);
            CHECK(bounds.getHeight() >= 60);
            CHECK(editorBounds.contains(bounds));
            knobBounds.push_back(bounds);
        }
    }
    // La banda de knobs grandes (6 macros) y la tira inferior (GAIN/EQ/WIDTH/GAIN) tambien
    // tienen mandos: entre todo tiene que estar la superficie completa del host.
    int bandKnobs = 0, stripKnobs = 0;
    if (auto* band = retroexxEditor->macroBand()) {
        bandKnobs = band->macroCount();
        for (int i = 0; i < band->macroCount(); ++i) {
            const auto bounds = boundsInEditor(*band->macroKnob(i), *editor);
            CHECK(editorBounds.contains(bounds));
            knobBounds.push_back(bounds);
        }
    }
    if (auto* strip = retroexxEditor->masterStrip()) {
        stripKnobs = strip->knobCount();
        for (int i = 0; i < strip->knobCount(); ++i) {
            const auto bounds = boundsInEditor(*strip->knob(i), *editor);
            CHECK(editorBounds.contains(bounds));
            knobBounds.push_back(bounds);
        }
    }
    if (auto* fader = retroexxEditor->magnitudeFader()) {
        const auto bounds = boundsInEditor(*fader, *editor);
        CHECK(editorBounds.contains(bounds));
    }
    // Cobertura REAL: cada parametro del host editado por ALGUN control del editor, una vez.
    // No se cuentan knobs (ahora hay selectores, botones y deslizadores): se cuentan ids.
    std::vector<int> seen(static_cast<size_t>(rtxx::HostParamTable::count()), 0);
    auto mark = [&seen](int id) {
        if (id >= 0 && id < static_cast<int>(seen.size())) ++seen[static_cast<size_t>(id)];
    };
    for (int i = 0; i < retroexxEditor->panelCount(); ++i)
        for (int id : retroexxEditor->panel(i)->hostIds()) mark(id);
    if (auto* band = retroexxEditor->macroBand())
        for (int i = 0; i < band->macroCount(); ++i) {
            mark(band->macroKnob(i)->hostId());
            if (band->macroEnable(i) != nullptr) mark(band->macroEnable(i)->hostId());
        }
    if (auto* strip = retroexxEditor->masterStrip())
        for (int id : strip->hostIds()) mark(id);
    if (auto* fader = retroexxEditor->magnitudeFader()) mark(fader->hostId());

    // Cada id UNA vez: el Enable de modulo vive solo en la banda (la columna no tiene
    // interruptor, como El motor).
    int missing = 0, duplicated = 0;
    for (int id = 0; id < static_cast<int>(seen.size()); ++id) {
        const int times = seen[static_cast<size_t>(id)];
        if (times == 0) {
            std::printf("  falta el parametro %d en la GUI\n", id);
            ++missing;
        } else if (times > 1) {
            std::printf("  el parametro %d aparece %d veces\n", id, times);
            ++duplicated;
        }
    }
    std::printf("  %d knobs + %d controles en 6 columnas; banda %d, tira %d; cobertura %d/%d\n",
                knobCount, (int)retroexxEditor->panel(0)->hostIds().size(), bandKnobs, stripKnobs,
                (int)seen.size() - missing, (int)seen.size());
    CHECK(missing == 0);
    CHECK(duplicated == 0);

    int overlaps = 0;
    for (size_t i = 0; i < knobBounds.size(); ++i)
        for (size_t j = i + 1; j < knobBounds.size(); ++j)
            if (knobBounds[i].intersects(knobBounds[j])) ++overlaps;
    CHECK(overlaps == 0);

    // --- ventana del preset: selector, LOAD y SAVE en el header -----------------------
    {
        const auto boxBounds = boundsInEditor(retroexxEditor->presetBox(), *editor);
        const auto saveBounds = boundsInEditor(retroexxEditor->savePresetButton(), *editor);
        const auto loadBounds = boundsInEditor(retroexxEditor->loadPresetButton(), *editor);
        CHECK(editorBounds.contains(boxBounds));
        CHECK(editorBounds.contains(saveBounds));
        CHECK(editorBounds.contains(loadBounds));
        CHECK(!boxBounds.intersects(saveBounds));
        CHECK(!boxBounds.intersects(loadBounds));
        CHECK(!loadBounds.intersects(saveBounds));
        CHECK(retroexxEditor->presetWindow().contains(boxBounds));
        CHECK(boxBounds.getBottom() <= retroexxEditor->screenArea().getY());  // todo en el header
        // No pueden pisar ningun mando.
        int clashes = 0;
        for (const auto& knob : knobBounds)
            if (knob.intersects(boxBounds) || knob.intersects(saveBounds) ||
                knob.intersects(loadBounds))
                ++clashes;
        CHECK(clashes == 0);
        // Los 10 presets de fábrica están en la lista (ids 1..10).
        CHECK(retroexxEditor->presetBox().getNumItems() >= rtxx::factoryPresetCount());
        std::printf("  selector de presets: %dx%d con %d entradas, boton %dx%d\n",
                    boxBounds.getWidth(), boxBounds.getHeight(),
                    retroexxEditor->presetBox().getNumItems(), saveBounds.getWidth(),
                    saveBounds.getHeight());
    }

// Distribucion del motor: 6 columnas en UNA fila, banda debajo y tira debajo.
    {
        const auto screen = retroexxEditor->screenArea();
        const auto band = retroexxEditor->bandArea();
        const auto strip = retroexxEditor->stripArea();
        CHECK(screen.getBottom() <= band.getY());
        CHECK(band.getBottom() <= strip.getY());
        int rows = 0;
        for (int i = 0; i < retroexxEditor->panelCount(); ++i) {
            const auto b = retroexxEditor->panel(i)->getBounds();
            CHECK(b.getY() == retroexxEditor->panel(0)->getBounds().getY());  // una sola fila
            (void)rows;
        }
        std::printf("  bandas: pantalla %d px, banda %d px, tira %d px\n", screen.getHeight(),
                    band.getHeight(), strip.getHeight());
        CHECK(band.getHeight() >= 150);
        CHECK(strip.getHeight() >= 90);
    }

    const juce::Image image = editor->createComponentSnapshot(editorBounds);
    CHECK(image.isValid());
    CHECK(image.getWidth() == editorBounds.getWidth());
    CHECK(image.getHeight() == editorBounds.getHeight());
    checkHeaderPalette(image, retroexxEditor->bandArea(), 107);

    const juce::File outputDirectory =
        juce::File(juce::String(RTXX_SOURCE_DIR)).getChildFile("test/ui");
    const juce::File outputFile = outputDirectory.getChildFile("editor.png");
    const bool created = outputDirectory.createDirectory().wasOk();
    CHECK(created || outputDirectory.isDirectory());
    // FileOutputStream abre en modo append: sin borrar, cada pasada corrompe el PNG.
    outputFile.deleteFile();
    juce::FileOutputStream stream(outputFile);
    CHECK(stream.openedOk());
    if (stream.openedOk()) {
        juce::PNGImageFormat png;
        const bool written = png.writeImageToStream(image, stream);
        stream.flush();
        CHECK(written);
        std::printf("  captura: %s (%dx%d)\n", outputFile.getFullPathName().toRawUTF8(),
                    image.getWidth(), image.getHeight());
    }

    // Trip computer: sin raton en la prueba, se fija a mano un control por columna (selector,
    // crossfade, rango FOCUS, knob, slider) y se escribe una segunda captura para revisarla.
    {
        const int pick[] = {0, 0, 1, 3, 0, 2};  // NOISE TYPE, WOW/FLUTTER, DIST FOCUS, SMOOTH, DECAY, RATE
        int active = 0;
        for (int i = 0; i < retroexxEditor->panelCount() && i < 6; ++i) {
            retroexxEditor->panel(i)->showTripComputer(pick[i]);
            if (retroexxEditor->panel(i)->tripComputerActive()) ++active;
        }
        CHECK(active == retroexxEditor->panelCount());
        const juce::Image trip = editor->createComponentSnapshot(editorBounds);
        const juce::File tripFile = outputDirectory.getChildFile("editor_trip.png");
        tripFile.deleteFile();
        juce::FileOutputStream tripStream(tripFile);
        CHECK(tripStream.openedOk());
        if (tripStream.openedOk()) {
            juce::PNGImageFormat png;
            CHECK(png.writeImageToStream(trip, tripStream));
            tripStream.flush();
        }
        for (int i = 0; i < retroexxEditor->panelCount(); ++i)
            retroexxEditor->panel(i)->showTripComputer(-1);
    }

    // Barrido de encendido (VEXX-DASH90 §9): con el barrido en su fase "todo encendido" tiene
    // que haber bastantes mas pixeles de fosforo que en reposo, y al acabar vuelve a reposo.
    {
        auto vfdPixels = [](const juce::Image& im) {
            int n = 0;
            for (int y = 0; y < im.getHeight(); y += 2)
                for (int x = 0; x < im.getWidth(); x += 2) {
                    const auto p = im.getPixelAt(x, y);
                    if (p.getGreen() > 130 && p.getBlue() > 110 && p.getRed() < p.getGreen() - 40) ++n;
                }
            return n;
        };
        const int resting = vfdPixels(editor->createComponentSnapshot(editorBounds));
        rtxx::ui::power::start();
        const int sweeping = vfdPixels(editor->createComponentSnapshot(editorBounds));
        rtxx::ui::power::advance(1000.0);
        CHECK(!rtxx::ui::power::active());
        std::printf("  power-on: %d px de fosforo en reposo, %d durante el barrido\n", resting,
                    sweeping);
        CHECK(sweeping > resting * 3 / 2);
    }

    // Foco de teclado (§6): el foco por Tab abre el ordenador de viaje de su columna; el foco
    // por clic (directo) no, o la lectura se quedaria pegada al ultimo control tocado.
    {
        auto* panel = retroexxEditor->panel(0);
        CHECK(panel->knobCount() > 0);
        if (panel->knobCount() > 0) {
            auto* knob = panel->knob(0);
            knob->focusGained(juce::Component::focusChangedByMouseClick);
            panel->updateTripComputer();
            const bool afterClick = panel->tripComputerActive();
            knob->focusLost(juce::Component::focusChangedByMouseClick);
            knob->focusGained(juce::Component::focusChangedByTabKey);
            panel->updateTripComputer();
            const bool afterTab = panel->tripComputerActive();
            knob->focusLost(juce::Component::focusChangedByTabKey);
            panel->updateTripComputer();
            std::printf("  foco: clic -> lectura %d, Tab -> lectura %d, sin foco -> lectura %d\n",
                        afterClick, afterTab, panel->tripComputerActive());
            CHECK(!afterClick && afterTab && !panel->tripComputerActive());
        }
    }

    // Selector de tipo con teclado: dos pasos a la derecha desde TUBEPAIR (0) dejan DIST en
    // CRUNCH (2 de 0..5), y la flecha izquierda vuelve uno.
    {
        rtxx::ui::ParamSelector* selector = nullptr;
        for (auto* child : retroexxEditor->panel(2)->getChildren())
            if (auto* s = dynamic_cast<rtxx::ui::ParamSelector*>(child)) selector = s;
        CHECK(selector != nullptr);
        auto* type = processor.hostParameter(22);
        if (selector != nullptr && type != nullptr) {
            type->setValueNotifyingHost(0.0f);
            selector->keyPressed(juce::KeyPress(juce::KeyPress::rightKey));
            selector->keyPressed(juce::KeyPress(juce::KeyPress::rightKey));
            const int afterRight = juce::roundToInt(type->getValue() * 5.0f);
            selector->keyPressed(juce::KeyPress(juce::KeyPress::leftKey));
            const int afterLeft = juce::roundToInt(type->getValue() * 5.0f);
            std::printf("  selector DIST: dos a la derecha -> %d, uno a la izquierda -> %d\n",
                        afterRight, afterLeft);
            CHECK(afterRight == 2 && afterLeft == 1);
            type->setValueNotifyingHost(0.0f);
        }
    }

    // Visualizadores por parametros (fase 3): DIGITAL encendido con reduccion visible y DIST
    // en CRUNCH; captura aparte para revisar los trazos. Tiene que aparecer el azul de DIGITAL
    // (#8FC8FF) en su pantalla, que con el modulo apagado esta al 7 %.
    {
        auto set = [&processor](int hostId, float value) {
            if (auto* p = processor.hostParameter(hostId)) p->setValueNotifyingHost(value);
        };
        auto digitalPixels = [&]() {
            const juce::Image im = editor->createComponentSnapshot(editorBounds);
            const auto area = retroexxEditor->panel(3)->getBounds();
            int n = 0;
            for (int y = area.getY(); y < area.getY() + area.getHeight() / 3; ++y)
                for (int x = area.getX(); x < area.getRight(); ++x) {
                    const auto p = im.getPixelAt(x, y);
                    if (p.getBlue() > 200 && p.getGreen() > 160 && p.getRed() > 110) ++n;
                }
            return std::make_pair(n, im);
        };
        const int off = digitalPixels().first;
        set(27, 1.0f);   // DIGI Enable
        set(4, 0.6f);    // DIGI Amount
        set(28, 0.5f);   // R<>B Balance
        set(22, 0.4f);   // DIST Type = CRUNCH (2 de 0..5)
        for (int i = 0; i < retroexxEditor->panelCount(); ++i)
            retroexxEditor->panel(i)->advanceAnimation(1.0f);
        const auto [lit, vizImage] = digitalPixels();
        std::printf("  viz DIGITAL: %d px de trazo apagado, %d encendido\n", off, lit);
        CHECK(lit > 150 && off < 20);
        const juce::File vizFile = outputDirectory.getChildFile("editor_viz.png");
        vizFile.deleteFile();
        juce::FileOutputStream vizStream(vizFile);
        CHECK(vizStream.openedOk());
        if (vizStream.openedOk()) {
            juce::PNGImageFormat png;
            CHECK(png.writeImageToStream(vizImage, vizStream));
            vizStream.flush();
        }
    }

    // Visualizadores por audio (fase 4): con SPACE encendido, audio real por el motor y las
    // tomas vaciadas por el panel, las pantallas de NOISE, SPACE y MAGNETIC tienen que
    // llevar trazo (pixeles brillantes en su pantalla).
    {
        if (auto* p = processor.hostParameter(35)) p->setValueNotifyingHost(1.0f);   // SPACE Enable
        if (auto* p = processor.hostParameter(5)) p->setValueNotifyingHost(0.5f);    // SPACE Amount
        juce::AudioBuffer<float> block(2, 512);
        juce::MidiBuffer midi;
        for (int b = 0; b < 12; ++b) {
            for (int i = 0; i < block.getNumSamples(); ++i) {
                const float v = 0.25f * std::sin(juce::MathConstants<float>::twoPi * 220.0f *
                                                static_cast<float>(b * 512 + i) / 44100.0f);
                block.setSample(0, i, v);
                block.setSample(1, i, v);
            }
            processor.processBlock(block, midi);
            for (int i = 0; i < retroexxEditor->panelCount(); ++i)
                retroexxEditor->panel(i)->advanceAnimation(1.0f / 30.0f);
        }
        const juce::Image audio = editor->createComponentSnapshot(editorBounds);
        auto tracePixels = [&audio, &retroexxEditor](int panel) {
            const auto area = retroexxEditor->panel(panel)->getBounds();
            int n = 0;
            for (int y = area.getY() + 20; y < area.getY() + area.getHeight() / 3; ++y)
                for (int x = area.getX() + 8; x < area.getRight() - 8; ++x) {
                    const auto c = audio.getPixelAt(x, y);
                    if (std::max({c.getRed(), c.getGreen(), c.getBlue()}) > 150) ++n;
                }
            return n;
        };
        const int noise = tracePixels(0), space = tracePixels(4), magnetic = tracePixels(5);
        std::printf("  viz por audio: NOISE %d px, SPACE %d px, MAGNETIC %d px\n", noise, space,
                    magnetic);
        CHECK(noise > 100 && space > 100 && magnetic > 100);
        const juce::File audioFile = outputDirectory.getChildFile("editor_audio.png");
        audioFile.deleteFile();
        juce::FileOutputStream audioStream(audioFile);
        CHECK(audioStream.openedOk());
        if (audioStream.openedOk()) {
            juce::PNGImageFormat png;
            CHECK(png.writeImageToStream(audio, audioStream));
            audioStream.flush();
        }
    }

    // Rendimiento (fase 5): un segundo a 60 Hz con audio real. Por fotograma: el motor
    // procesa 1/60 s, las columnas avanzan sus animaciones y vacian sus tomas, y se repinta
    // lo que cada columna pidio (lastDirty), una pasada por zona. Render por software sobre
    // una imagen a 1x y a 2x (Retina) con CoreGraphics, el mismo renderizador de la ventana.
    {
        juce::AudioBuffer<float> block(2, 735);
        juce::MidiBuffer midi;
        int frameIndex = 0;
        auto runSecond = [&](float scale) {
            juce::Image canvas(juce::Image::ARGB, juce::roundToInt(static_cast<float>(editor->getWidth()) * scale),
                               juce::roundToInt(static_cast<float>(editor->getHeight()) * scale), true);
            int repaints = 0;
            double paintMs = 0.0, workMs = 0.0;
            for (int frame = 0; frame < 60; ++frame, ++frameIndex) {
                for (int i = 0; i < block.getNumSamples(); ++i) {
                    const float v = 0.25f * std::sin(juce::MathConstants<float>::twoPi * 220.0f *
                                                    static_cast<float>(frameIndex * 735 + i) / 44100.0f);
                    block.setSample(0, i, v);
                    block.setSample(1, i, v);
                }
                processor.processBlock(block, midi);
                const double t0 = juce::Time::getMillisecondCounterHiRes();
                for (int i = 0; i < retroexxEditor->panelCount(); ++i) {
                    retroexxEditor->panel(i)->updateTripComputer();
                    retroexxEditor->panel(i)->advanceAnimation(1.0f / 60.0f);
                }
                const double t1 = juce::Time::getMillisecondCounterHiRes();
                for (int i = 0; i < retroexxEditor->panelCount(); ++i) {
                    auto* panel = retroexxEditor->panel(i);
                    for (const auto& r : panel->lastDirty()) {
                        juce::Graphics g(canvas);
                        g.addTransform(juce::AffineTransform::scale(scale));
                        g.reduceClipRegion(r + panel->getPosition());
                        editor->paintEntireComponent(g, false);
                        ++repaints;
                    }
                }
                workMs += t1 - t0;
                paintMs += juce::Time::getMillisecondCounterHiRes() - t1;
            }
            const double percent = (workMs + paintMs) / 1000.0 * 100.0;
            std::printf("  rendimiento %.0fx: 60 fotogramas, %d zonas, %.1f ms de pintado + %.1f ms de timer = %.2f %% de un nucleo\n",
                        static_cast<double>(scale), repaints, paintMs, workMs, percent);
            return percent;
        };
        // Informativo, sin CHECK: el reloj de pared da falsos fallos en ctest y esta medida
        // no incluye la composicion de la ventana (Metal/CoreAnimation), que en la demo real
        // es la mayor parte. La CPU real se mide con RetroexxDemo (DESIGN.md v10).
        runSecond(1.0f);   // calienta las capas en cache a 1x
        runSecond(1.0f);
        runSecond(2.0f);   // rehace las capas a 2x
        runSecond(2.0f);
    }

    // El editor tiene que morir antes que el processor (sus attachments escuchan a los
    // parametros de este).
    editor.reset();
    return check::finish("test_ui_snapshot");
}
