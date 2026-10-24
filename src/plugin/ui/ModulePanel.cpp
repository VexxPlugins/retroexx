#include "ModulePanel.h"

#include <cmath>

#include "Fonts.h"
#include "HostParamDisplay.h"
#include "KeyboardFocus.h"
#include "ModuleViz.h"
#include "ReduceMotion.h"
#include "SegmentDisplay.h"
#include "Skin.h"

namespace rtxx::ui {
namespace {

using geometry::scaled;

// Zonas de la columna en coordenadas del motor (columna de 124 x 310):
//   pantalla (viz / ordenador de viaje) arriba, carcasa con los mandos debajo.
constexpr float kDisplay[] = {2.0f, 6.0f, 120.0f, 82.0f};
constexpr float kPod[] = {0.0f, 92.0f, 124.0f, 218.0f};
constexpr float kCaptionHeight = 10.0f;

// Columnas con visualizador por parametros y los ids que lee (el Amount va aparte).
constexpr int kWobble = 1, kDist = 2, kDigital = 3;
// Columnas con visualizador por audio (toma del motor).
constexpr int kNoise = 0, kSpace = 4, kMagnetic = 5;
// Los viz y el halo de FLUX avanzan a 15 Hz (VEXX-DASH90 §8); el fundido del rayado va a la
// frecuencia del timer.
constexpr float kVizStep = 1.0f / 15.0f;

// Los botones de 40 x 40 son teclas de 32 x 32 centradas en su celda (la celda es la del
// knob vecino, para que las leyendas queden alineadas).
juce::Rectangle<int> controlBounds(const ControlSpec& s) {
    if (s.kind == ControlKind::Button && s.w == 40 && s.h == 40)
        return scaled(s.x + 4.0f, s.y + 4.0f, 32.0f, 32.0f);
    return scaled(s.x, s.y, s.w, s.h);
}

float captionTop(const ControlSpec& s) {
    return s.captionY >= 0 ? static_cast<float>(s.captionY)
                           : static_cast<float>(s.y + s.h) + 2.0f;
}

// Texto del valor partido en numero (segmentos) y unidad (texto), como el resto de lecturas.
void splitValue(const juce::String& text, juce::String& number, juce::String& unit) {
    int split = text.length();
    for (int i = 0; i < text.length(); ++i) {
        const auto c = text[i];
        if (!(juce::CharacterFunctions::isDigit(c) || c == '-' || c == '+' || c == '.' ||
              c == ' ')) {
            split = i;
            break;
        }
    }
    number = text.substring(0, split).trim();
    unit = text.substring(split).trim();
}

}  // namespace

juce::Rectangle<int> ModulePanel::boundsInEditor(int index) {
    return scaled(15.0f + 128.0f * static_cast<float>(index), 74.0f, 124.0f, 310.0f);
}

ModulePanel::ModulePanel(const ModuleSpec& spec, const HostParamLookup& lookup) : spec_(spec) {
    setOpaque(false);
    setName(spec.name);
    addAndMakeVisible(screen_);
    const juce::Colour accent(spec.accent);
    // OJO: `spec_` es una COPIA. El indice se calcula del parametro (que si apunta dentro de
    // ModuleTable::all()); calcularlo de &spec_ daba una diferencia de punteros basura y la
    // columna acababa con la distribucion de otro modulo.
    moduleIndex_ = static_cast<int>(&spec - ModuleTable::all());
    const ColumnLayout& layout = columnLayout(moduleIndex_);

    // El Enable solo se escucha (la vista que lo edita es la de la banda).
    if (spec_.enableHostId >= 0) {
        enable_ = &lookup(spec_.enableHostId);
        enableAttachment_ = std::make_unique<juce::ParameterAttachment>(
            *enable_, [this](float) { repaint(); });
    }

    for (int i = 0; i < layout.count; ++i) {
        const ControlSpec& item = layout.items[i];
        Entry entry{nullptr, &item, &lookup(item.idA),
                    item.idB >= 0 ? &lookup(item.idB) : nullptr};
        switch (item.kind) {
            case ControlKind::Knob: {
                auto* knob = new ParamKnob(*entry.a, item.idA, DialFinish::Dark, accent);
                knob->setCaptionVisible(false);
                knob->setReadoutVisible(false);
                entry.component = knobs_.add(knob);
                break;
            }
            case ControlKind::Selector:
                entry.component = selectors_.add(new ParamSelector(*entry.a, accent, item.idA));
                break;
            case ControlKind::Button:
                entry.component = buttons_.add(new ParamButton(*entry.a, item.text, accent, item.idA));
                break;
            case ControlKind::Fader: {
                auto* fader = new ParamFader(*entry.a, accent, item.idA);
                fader->setReadoutVisible(false);
                fader->setBarMode(item.endLo != nullptr && item.endHi != nullptr
                                      ? SegBarMode::XFade
                                      : SegBarMode::Fill);
                entry.component = faders_.add(fader);
                break;
            }
            case ControlKind::Range: {
                auto* range = new ParamRangeSlider(*entry.a, *entry.b, "", accent, true, item.idA,
                                                   item.idB);
                range->setReadoutVisible(false);
                entry.component = ranges_.add(range);
                break;
            }
        }
        if (item.caption != nullptr && juce::String(item.caption) == "FLUX") {
            fluxEntry_ = static_cast<int>(entries_.size());
            entry.component->setName("FLUX");
        }
        addAndMakeVisible(entry.component);
        hostIds_.push_back(item.idA);
        if (item.idB >= 0) hostIds_.push_back(item.idB);
        entries_.push_back(entry);
    }

    if (spec_.macroHostId >= 0) amount_ = &lookup(spec_.macroHostId);
    if (moduleIndex_ == kWobble) {
        for (int id : {15, 16, 18, 19}) vizParams_.push_back(&lookup(id));   // balance, rates, mix
    } else if (moduleIndex_ == kDist) {
        vizParams_.push_back(&lookup(22));                                  // tipo
    } else if (moduleIndex_ == kDigital) {
        for (int id : {28, 32, 33}) vizParams_.push_back(&lookup(id));      // balance, smooth, mix
    }

    if (enableAttachment_ != nullptr) enableAttachment_->sendInitialUpdate();
    hatch_ = isOn() ? 0.0f : 1.0f;
    applyControlAlpha();
}

void ModulePanel::applyControlAlpha() {
    // Modulo apagado: controles al 40 % (§6).
    for (const auto& entry : entries_) entry.component->setAlpha(1.0f - 0.6f * hatch_);
}

void ModulePanel::markDirty(juce::Rectangle<int> area) {
    lastDirty_.add(area);
    repaint(area);
}

float ModulePanel::fluxOpacity() const {
    // Halo que late con el flux: opacidad 0,35..0,95 (ease-in-out de 1,85 s), profundidad
    // proporcional al valor. Reducir movimiento: brillo fijo a media altura del pulso.
    const float flux = entries_[static_cast<size_t>(fluxEntry_)].a->getValue();
    const float pulse = reduceMotion() ? 0.5f
                                       : 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * fluxPhase_);
    return (0.35f + 0.60f * pulse) * juce::jmin(1.0f, 0.4f + flux);
}

float ModulePanel::distFlicker() const {
    // Parpadeo con la entrada: steps(3) de 0,7 s hasta el 82 %, proporcional al nivel.
    const int step = reduceMotion() ? 0 : static_cast<int>(distClock_ / 0.7f * 3.0f) % 3;
    return 0.09f * static_cast<float>(step) * juce::jmin(1.0f, level_ * 2.0f);
}

void ModulePanel::advanceAnimation(float seconds) {
    // Cada zona se repinta solo si su dibujo cambia (fase 5): el timer va a 60 Hz.
    lastDirty_.clear();
    const bool on = isOn();
    const float target = on ? 0.0f : 1.0f;
    if (!juce::approximatelyEqual(hatch_, target)) {
        const float step = reduceMotion() ? 1.0f : seconds / 0.15f;
        hatch_ = target > hatch_ ? juce::jmin(target, hatch_ + step) : juce::jmax(target, hatch_ - step);
        applyControlAlpha();
        markDirty(getLocalBounds());
    }

    // Viz y FLUX: un paso cada kVizStep, con el tiempo acumulado.
    vizClock_ += seconds;
    const bool vizTick = vizClock_ >= kVizStep - 0.001f;
    const float vizSeconds = vizTick ? vizClock_ : 0.0f;
    if (vizTick) vizClock_ = 0.0f;

    // FLUX: el halo se repinta cuando cambia su alfa en 8 bits (cambio visible).
    if (fluxEntry_ >= 0 && vizTick) {
        const bool glowing = on && entries_[static_cast<size_t>(fluxEntry_)].a->getValue() > 0.0f;
        if (glowing && !reduceMotion()) fluxPhase_ = std::fmod(fluxPhase_ + vizSeconds / 1.85f, 1.0f);
        const int glow = glowing ? juce::roundToInt(255.0f * 0.15f * fluxOpacity()) : -1;
        if (glow != fluxGlow_) {
            fluxGlow_ = glow;
            markDirty(entries_[static_cast<size_t>(fluxEntry_)].component->getBounds().expanded(16, 10));
        }
    }

    // Viz por audio: se vacia siempre la cola (que no se llene); con el modulo apagado las
    // tramas se tiran y el viz queda congelado (§8). SPACE se redibuja al completar cada uno
    // de sus 12 grupos; con "reducir movimiento", todo una vez por segundo.
    if (tap_ != nullptr) {
        const int arrived = tap_->pop([this, on](const VizFrame& f) {
            if (!on) return;
            history_[static_cast<size_t>(historyHead_)] = f;
            historyHead_ = (historyHead_ + 1) % kHistory;
        });
        if (on) pendingFrames_ += arrived;
        sinceRefresh_ += seconds;
        const int needed = moduleIndex_ == kSpace ? kHistory / 12 : 1;
        const bool due = reduceMotion() ? sinceRefresh_ >= 1.0f : vizTick && pendingFrames_ >= needed;
        if (on && pendingFrames_ > 0 && due && tripIndex_ < 0) {
            sinceRefresh_ = 0.0f;
            pendingFrames_ = 0;
            markDirty(displayArea_);
        }
    }

    // Visualizador por parametros: avanza solo con el modulo encendido (apagado se congela,
    // §8) y sin "reducir movimiento"; se repinta si cambia lo que dibuja.
    if (vizParams_.empty() || amount_ == nullptr) return;
    const bool animate = on && !reduceMotion();
    if (animate && moduleIndex_ == kWobble) {
        const auto advance = [vizSeconds](float& phase, float cycles, float rate) {
            phase = std::fmod(phase + juce::MathConstants<float>::twoPi * cycles * vizSeconds /
                                          viz::scrollSeconds(rate),
                              juce::MathConstants<float>::twoPi);
        };
        advance(wowPhase_, viz::wowCycles(vizValue(1)), vizValue(1));
        advance(flutterPhase_, viz::flutterCycles(vizValue(2)), vizValue(2));
    }
    if (animate && moduleIndex_ == kDist) distClock_ = std::fmod(distClock_ + vizSeconds, 0.7f);

    float signature = amount_->getValue() + (on ? 10.0f : 0.0f);
    if (moduleIndex_ == kDist) signature += std::round(distFlicker() * 255.0f) * 100.0f;
    for (size_t i = 0; i < vizParams_.size(); ++i)
        signature += vizParams_[i]->getValue() * static_cast<float>(100000 * (i + 1));
    const bool moving = animate && moduleIndex_ == kWobble && vizTick;
    if (moving || !juce::approximatelyEqual(signature, vizSignature_)) {
        vizSignature_ = signature;
        if (tripIndex_ < 0) markDirty(displayArea_);
    }
}

void ModulePanel::paintViz(juce::Graphics& g, juce::Rectangle<float> area, bool on) const {
    // Apagado: ultimo estado al 7 % bajo el rayado (§8).
    if (!on) g.beginTransparencyLayer(0.07f);
    const juce::Colour accent(spec_.accent);
    if (tap_ != nullptr) {
        // Historia en orden, de la trama mas vieja a la mas nueva.
        for (int i = 0; i < kHistory; ++i)
            ordered_[static_cast<size_t>(i)] = history_[static_cast<size_t>((historyHead_ + i) % kHistory)];
        const VizFrame* frames = ordered_.data();
        if (moduleIndex_ == kNoise) viz::paintNoise(g, area, accent, frames, kHistory);
        else if (moduleIndex_ == kSpace) viz::paintSpace(g, area, accent, frames, kHistory);
        else if (moduleIndex_ == kMagnetic) viz::paintMagnetic(g, area, accent, frames, kHistory);
        if (!on) g.endTransparencyLayer();
        return;
    }
    const float amount = amount_->getValue();
    if (moduleIndex_ == kWobble) {
        viz::paintWobble(g, area, accent, amount, vizValue(0), vizValue(1), vizValue(2),
                         vizValue(3), wowPhase_, flutterPhase_);
    } else if (moduleIndex_ == kDigital) {
        viz::paintDigital(g, area, accent, amount, vizValue(0), vizValue(1), vizValue(2));
    } else if (moduleIndex_ == kDist) {
        viz::paintDist(g, area, accent, juce::roundToInt(vizValue(0) * 5.0f), amount, distFlicker());
    }
    if (!on) g.endTransparencyLayer();
}

ModulePanel::~ModulePanel() = default;

bool ModulePanel::isOn() const { return enable_ == nullptr || enable_->getValue() > 0.5f; }

void ModulePanel::setVizTap(VizTap* tap) {
    tap_ = tap;
    if (tap_ == nullptr) return;
    history_.assign(kHistory, VizFrame{});
    ordered_.assign(kHistory, VizFrame{});
    historyHead_ = 0;
}

void ModulePanel::setLevel(float normalised) {
    const float clamped = juce::jlimit(0.0f, 1.0f, normalised);
    level_ = clamped;   // sin repintar: el parpadeo de DIST lo mira en advanceAnimation
}

void ModulePanel::setControlsEnabled(bool shouldBeEnabled) {
    for (const auto& entry : entries_) entry.component->setEnabled(shouldBeEnabled);
}

void ModulePanel::updateTripComputer() {
    // Raton (encima o arrastrando) y, si no hay ninguno, el control con foco de TECLADO (Tab;
    // el foco de un clic no cuenta: se quedaria pegada la lectura del ultimo control tocado).
    int index = -1;
    for (size_t i = 0; i < entries_.size(); ++i)
        if (entries_[i].component->isMouseOverOrDragging(true)) {
            index = static_cast<int>(i);
            break;
        }
    if (index < 0)
        for (size_t i = 0; i < entries_.size(); ++i)
            if (focusedByKeyboard(*entries_[i].component)) {
                index = static_cast<int>(i);
                break;
            }
    showTripComputer(index);
}

void ModulePanel::showTripComputer(int index) {
    if (index >= static_cast<int>(entries_.size())) index = -1;
    juce::String name, value;
    if (index >= 0) {
        const Entry& entry = entries_[static_cast<size_t>(index)];
        const ControlSpec& s = *entry.spec;
        if (s.endLo != nullptr && s.endHi != nullptr)
            name = juce::String(s.endLo) + " / " + s.endHi;
        else if (s.caption != nullptr && *s.caption != 0)
            name = s.caption;
        else
            name = rtxx::hostParamShortName(s.idA).toUpperCase();
        value = entry.a->getText(entry.a->getValue(), 24).trim();
        if (entry.b != nullptr) {
            // Rango: con la misma unidad se lee "0-100 %" en segmentos; si no, en texto.
            const juce::String high = entry.b->getText(entry.b->getValue(), 24).trim();
            juce::String loNumber, loUnit, hiNumber, hiUnit;
            splitValue(value, loNumber, loUnit);
            splitValue(high, hiNumber, hiUnit);
            if (loUnit == hiUnit && loNumber.isNotEmpty() && hiNumber.isNotEmpty())
                value = loNumber + "-" + hiNumber + (loUnit.isEmpty() ? "" : " " + loUnit);
            else
                value << " - " << high;
        }
    }
    if (index == tripIndex_ && name == tripName_ && value == tripValue_) return;
    tripIndex_ = index;
    tripName_ = name;
    tripValue_ = value;
    markDirty(displayArea_);
}

void ModulePanel::paint(juce::Graphics& g) {
    const bool on = isOn();
    const juce::Colour accent(spec_.accent);

// Carcasa del modulo: redondeada arriba como la del motor, filo y linea superior del
    // color del modulo (el color no rellena la carcasa: VEXX-DASH90 §3.4).
    {
        const auto pod = podArea_.toFloat();
        juce::Path shape;
        shape.addRoundedRectangle(pod.getX(), pod.getY(), pod.getWidth(), pod.getHeight(), 12.0f,
                                  12.0f, true, true, false, false);
        juce::ColourGradient body(colours::podTop, 0.0f, pod.getY(), colours::podBottom, 0.0f,
                                  pod.getBottom(), false);
        g.setGradientFill(body);
        g.fillPath(shape);
        g.setColour(accent.withAlpha(on ? 0.30f : 0.12f));
        g.strokePath(shape, juce::PathStrokeType(1.0f));
        g.setColour(accent.withAlpha(on ? 0.75f : 0.20f));
        g.fillRect(juce::Rectangle<float>(pod.getX() + 14.0f, pod.getY() + 1.0f,
                                          pod.getWidth() - 28.0f, 2.0f));
    }

    paintCaptions(g, on);
    paintFlux(g, on);
}

void ModulePanel::paintScreen(juce::Graphics& g) const {
    // La pantalla pinta en coordenadas del panel (las de displayArea_).
    g.setOrigin(-displayArea_.getPosition());
    const bool on = isOn();
    const juce::Colour accent(spec_.accent);

    const float scale = g.getInternalContext().getPhysicalPixelScaleFactor();
    if (!juce::approximatelyEqual(scale, screenScale_) || on != screenOn_ || !screenUnder_.isValid() ||
        screenUnder_.getWidth() != juce::roundToInt(static_cast<float>(displayArea_.getWidth()) * scale)) {
        screenScale_ = scale;
        screenOn_ = on;
        const auto layer = [this, scale](juce::Image::PixelFormat format, auto&& paintLayer) {
            juce::Image image(format,
                              juce::roundToInt(static_cast<float>(displayArea_.getWidth()) * scale),
                              juce::roundToInt(static_cast<float>(displayArea_.getHeight()) * scale), true);
            juce::Graphics lg(image);
            lg.addTransform(juce::AffineTransform::scale(scale));
            paintLayer(lg, displayArea_.withZeroOrigin().toFloat());
            return image;
        };
        // Fondo: el pozo de la pantalla en las esquinas redondeadas y el LCD hundido.
        // Opaco (RGB): se copia sin mezclar.
        screenUnder_ = layer(juce::Image::RGB, [on, accent](juce::Graphics& lg, juce::Rectangle<float> r) {
            lg.fillAll(colours::screen);
            skin::paintLcd(lg, r, on, accent.withAlpha(0.30f));
        });
    }
    const auto toScreen = juce::AffineTransform::scale(1.0f / scale)
                              .translated(displayArea_.getPosition().toFloat());
    g.drawImageTransformed(screenUnder_, toScreen);

    // Contenido: lectura de viaje o nombre del modulo y su visualizador.
    if (tripIndex_ >= 0) {
        paintTripComputer(g);
    } else {
        // Al 100 %: al 55 % (canvas v1) NOISE daba 2,43:1 (VEXX-DASH90 §5.6).
        g.setColour(on ? accent : accent.withAlpha(0.35f));
        g.setFont(rajdhani(9.5f).withExtraKerningFactor(0.28f));
        g.drawText(spec_.name, displayArea_.reduced(9, 6).withHeight(12),
                   juce::Justification::topLeft, false);
        paintViz(g, displayArea_.toFloat().reduced(8.0f, 6.0f).withTrimmedTop(12.0f), on);
    }

    // Cristal directo: mezclar una capa ARGB en caché costaba más que dibujarlo (medido a 2x).
    skin::paintGlass(g, displayArea_.toFloat());
}

void ModulePanel::paintOverChildren(juce::Graphics& g) {
    // Rayado de modulo apagado sobre la pantalla y la carcasa, por encima de los controles.
    skin::paintHatch(g, displayArea_.toFloat(), hatch_);
    skin::paintHatch(g, podArea_.toFloat(), hatch_);
}

void ModulePanel::paintFlux(juce::Graphics& g, bool on) const {
    if (fluxEntry_ < 0) return;
    const Entry& entry = entries_[static_cast<size_t>(fluxEntry_)];
    const auto bar = entry.component->getBounds().toFloat().withSizeKeepingCentre(
        static_cast<float>(entry.component->getWidth()), 12.0f);
    const juce::Colour accent(spec_.accent);

    // Halo que late con el flux (fluxOpacity); con flux en 0 no hay halo.
    const float flux = entry.a->getValue();
    if (on && flux > 0.0f) {
        const float opacity = fluxOpacity();
        const auto glow = bar.withTrimmedLeft(-3.0f).withWidth(bar.getWidth() * 0.5f + 16.0f * flux + 8.0f);
        for (int i = 3; i >= 1; --i) {
            const float spread = static_cast<float>(i) * 3.0f;
            g.setColour(accent.withAlpha(0.30f * opacity / static_cast<float>(i + 1)));
            g.fillRoundedRectangle(glow.expanded(spread), 9.0f);
        }
    }

    // Iconos a los lados: raya (sin flux) y onda (con flux), en inkMuted.
    g.setColour(colours::inkMuted);
    g.fillRect(juce::Rectangle<float>(bar.getX() - 19.0f, bar.getCentreY() - 1.0f, 14.0f, 2.0f));
    juce::Path wave;
    const float x0 = bar.getRight() + 5.0f, cy = bar.getCentreY();
    wave.startNewSubPath(x0, cy);
    wave.cubicTo(x0 + 2.0f, cy - 5.0f, x0 + 4.0f, cy - 5.0f, x0 + 5.0f, cy);
    wave.cubicTo(x0 + 6.0f, cy + 5.0f, x0 + 8.0f, cy + 5.0f, x0 + 9.0f, cy);
    wave.cubicTo(x0 + 10.0f, cy - 5.0f, x0 + 12.0f, cy - 5.0f, x0 + 14.0f, cy);
    g.strokePath(wave, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved,
                                            juce::PathStrokeType::rounded));
}

void ModulePanel::paintCaptions(juce::Graphics& g, bool on) const {
    g.setFont(rajdhani(12.0f).withExtraKerningFactor(0.16f));
    const juce::Colour ink = on ? colours::inkLabel : colours::inkMuted;
    g.setColour(ink);
    const int captionH = scaled(kCaptionHeight);
    for (const auto& entry : entries_) {
        const ControlSpec& s = *entry.spec;
        const float top = captionTop(s);
        if (s.endLo != nullptr && s.endHi != nullptr) {
            // Crossfade: un extremo a cada lado, bajo el carril.
            const auto area = scaled(s.x - 7.0f, top, s.w + 14.0f, kCaptionHeight);
            g.drawText(s.endLo, area.withHeight(captionH), juce::Justification::centredLeft, false);
            g.drawText(s.endHi, area.withHeight(captionH), juce::Justification::centredRight, false);
            // Onda del modulo entre los dos extremos (el "<->" del crossfade).
            juce::Path wave;
            const float cx = static_cast<float>(area.getCentreX()) - 9.0f;
            const float cy = static_cast<float>(area.getY()) + static_cast<float>(captionH) * 0.5f;
            wave.startNewSubPath(cx, cy);
            wave.cubicTo(cx + 4.0f, cy - 4.0f, cx + 7.0f, cy - 4.0f, cx + 8.0f, cy);
            wave.cubicTo(cx + 9.0f, cy + 4.0f, cx + 12.0f, cy + 4.0f, cx + 16.0f, cy);
            g.setColour(juce::Colour(spec_.accent).withAlpha(on ? 1.0f : 0.4f));
            g.strokePath(wave, juce::PathStrokeType(1.6f));
            g.setColour(ink);
            continue;
        }
        if (s.caption == nullptr || *s.caption == 0) continue;
        const auto area = scaled(s.x - 10.0f, top, s.w + 20.0f, kCaptionHeight);
        g.drawText(s.caption, area.withHeight(captionH), juce::Justification::centred, false);
    }
}

void ModulePanel::paintTripComputer(juce::Graphics& g) const {
    const auto area = displayArea_.reduced(10, 8);
    const juce::Colour ink = colours::lcdInk;

    g.setColour(ink);
    g.setFont(rajdhani(13.0f).withExtraKerningFactor(0.30f));
    g.drawText(tripName_, area.withHeight(16), juce::Justification::topLeft, false);

    const auto valueArea = area.withTrimmedTop(18);
    juce::String number, unit;
    splitValue(tripValue_, number, unit);
    if (!number.isEmpty() && !tripValue_.contains(" - ") && SegmentDisplay::isSegmentable(number)) {
        // Valor en 7 segmentos itálicos (-8 grados, el "185" del cuadro del Fiat) y la unidad
        // en texto a su derecha, apoyada en la misma base.
        float digits = juce::jmin(46.0f, static_cast<float>(valueArea.getHeight()) - 6.0f);
        const juce::Font unitFont = rajdhani(15.0f);
        const float unitWidth =
            unit.isEmpty() ? 0.0f
                           : juce::GlyphArrangement::getStringWidth(unitFont, unit.toUpperCase());
        // Si el numero no cabe a ese alto (cursiva incluida), se encoge hasta caber.
        const float room = static_cast<float>(valueArea.getWidth()) - 0.14f * digits -
                           (unit.isEmpty() ? 0.0f : 6.0f + unitWidth);
        const float fullWidth = SegmentDisplay::measure(number, digits);
        if (fullWidth > room) digits *= room / fullWidth;
        const float numberWidth = SegmentDisplay::measure(number, digits);
        const float total = numberWidth + (unit.isEmpty() ? 0.0f : 6.0f + unitWidth);
        const float x = static_cast<float>(valueArea.getCentreX()) - total * 0.5f;
        const float cy = static_cast<float>(valueArea.getCentreY());
        SegmentDisplay::draw(g, juce::Rectangle<float>(x, cy - digits * 0.5f, numberWidth, digits),
                             number, digits, ink, true);
        if (!unit.isEmpty()) {
            g.setColour(ink.withAlpha(0.85f));
            g.setFont(unitFont);
            g.drawText(unit.toUpperCase(),
                       juce::Rectangle<float>(x + numberWidth + 6.0f, cy + digits * 0.5f - 16.0f,
                                              unitWidth + 2.0f, 16.0f),
                       juce::Justification::bottomLeft, false);
        }
    } else {
        // Lo que no es numero (tipo, ON/OFF, un rango con dos valores) va en texto.
        g.setFont(rajdhani(20.0f).withExtraKerningFactor(0.10f));
        g.drawFittedText(tripValue_.toUpperCase(), valueArea, juce::Justification::centred, 2,
                         0.7f);
    }
}

void ModulePanel::resized() {
    displayArea_ = scaled(kDisplay[0], kDisplay[1], kDisplay[2], kDisplay[3]);
    screen_.setBounds(displayArea_);
    podArea_ = scaled(kPod[0], kPod[1], kPod[2], kPod[3]);
    for (const auto& entry : entries_) entry.component->setBounds(controlBounds(*entry.spec));
}

}  // namespace rtxx::ui
