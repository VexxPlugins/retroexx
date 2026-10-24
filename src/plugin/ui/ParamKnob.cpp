#include "ParamKnob.h"

#include <cmath>

#include "Fonts.h"
#include "HostParamDisplay.h"
#include "SegmentDisplay.h"
#include "Skin.h"
#include "PowerOn.h"
#include "KeyboardFocus.h"
#include "Theme.h"

namespace rtxx::ui {
namespace {

// Dial de 270 grados: empieza a las 7:30 y acaba a las 4:30. Convencion de JUCE
// (getPointOnCircumference): 0 grados son las 12 en punto y el sentido es horario.
// 7:30 = 225 grados; la vuelta acaba en 225 + 270 = 495 (4:30).
constexpr float kSweepStartDegrees = 225.0f;
constexpr float kSweepTotalDegrees = 270.0f;
// Tope del dial: en paneles de un solo knob no crece hasta llenar el hueco.
constexpr int kMaxDialSize = 56;
constexpr int kMaxDialSizeSteel = 84;  // los knobs grandes de la franja crema
constexpr float kDragPixelsForFullRange = 180.0f;
constexpr float kPixelsPerEnumStep = 24.0f;

}  // namespace

ParamKnob::ParamKnob(juce::RangedAudioParameter& parameter, int hostId, DialFinish finish,
                     juce::Colour accent)
    : parameter_(parameter),
      hostId_(hostId),
      finish_(finish),
      accent_(accent),
      range_(parameter.getNormalisableRange()),
      value_(range_.convertFrom0to1(parameter.getValue())),
      caption_(rtxx::hostParamShortName(hostId)) {
    setWantsKeyboardFocus(true);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    setName(parameter.getName(64));
    attachment_ = std::make_unique<juce::ParameterAttachment>(
        parameter_, [this](float denormalised) {
            value_ = denormalised;
            repaint();
        });
    attachment_->sendInitialUpdate();
}

ParamKnob::~ParamKnob() = default;

bool ParamKnob::controlEnabled() const { return isEnabled(); }

void ParamKnob::setValueFromUi(float denormalised, bool completeGesture) {
    const float clamped = range_.snapToLegalValue(
        juce::jlimit(range_.start, range_.end, denormalised));
    value_ = clamped;  // respuesta inmediata al arrastre, sin esperar al async del host
    if (completeGesture)
        attachment_->setValueAsCompleteGesture(clamped);
    else
        attachment_->setValueAsPartOfGesture(clamped);
    repaint();
}

void ParamKnob::paint(juce::Graphics& g) {
    const bool on = controlEnabled();
    const auto bounds = getLocalBounds();
    const bool steel = finish_ == DialFinish::Steel;
    // En el knob grande la leyenda va en una sola linea: el mando tiene que dominar.
    const int nameHeight = captionVisible_
                               ? (steel ? juce::roundToInt(typeSmall * 1.4f)
                                        : juce::roundToInt(typeSmall * 1.25f) * 2)
                               : 0;
    // Hueco del valor: la ventana del LCD necesita 16 px de alto como minimo.
    const int valueHeight = readoutVisible_ ? (steel ? 22 : 20) : 0;
    const int dialSpace = juce::jmax(0, bounds.getHeight() - nameHeight - valueHeight - 2);
    // Sin lectura el control ES el dial: no se le pone tope.
    const int dialCap = !readoutVisible_ ? bounds.getWidth()
                        : (finish_ == DialFinish::Steel ? kMaxDialSizeSteel : kMaxDialSize);
    const int dialSize = juce::jmin(bounds.getWidth(), dialSpace, dialCap);
    if (dialSize <= 0) return;

    // El bloque (dial + nombre + valor) se centra en la celda: en un panel de un solo
    // knob el dial no queda pegado al borde superior.
    const int contentHeight = dialSize + nameHeight + valueHeight + 2;
    const int top = juce::jmax(0, (bounds.getHeight() - contentHeight) / 2);
    const juce::Rectangle<int> dial((bounds.getWidth() - dialSize) / 2, top, dialSize, dialSize);
    const juce::Rectangle<int> nameArea(0, dial.getBottom() + 1, bounds.getWidth(), nameHeight);
    const juce::Rectangle<int> valueArea(0, nameArea.getBottom() - 1, bounds.getWidth(),
                                         valueHeight);

    // VEXX-DASH90 §5.3: aro de ticks (21 en los pequenos, 33 en los grandes) y cuerpo de
    // plastico moleteado con tapa. Luz de arriba a la izquierda.
    const auto centre = dial.getCentre().toFloat();
    const float size = static_cast<float>(dialSize);
    const bool big = size >= 100.0f;
    const int tickCount = big ? 33 : 21;
    const float tickLength = big ? 9.0f : 6.0f;
    const float tickWidth = big ? 3.0f : 2.0f;
    const float tickRadius = size * 0.5f - tickLength * 0.5f - 1.0f;
    const float normalised = juce::jlimit(0.0f, 1.0f, range_.convertTo0to1(value_));
    const bool bipolar = range_.start < -1.0e-6f;
    const bool hasAccent = accent_.getAlpha() > 0;
    // Aro: color del modulo en los pequenos, vfd en los maestros.
    const juce::Colour ring = (steel || !hasAccent) ? colours::vfd : accent_;

    for (int i = 0; i < tickCount; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(tickCount - 1);
        constexpr float e = 1.0e-4f;
        const bool valueLit = bipolar ? (normalised >= 0.5f ? (t >= 0.5f - e && t <= normalised + e)
                                                            : (t <= 0.5f + e && t >= normalised - e))
                                      : (normalised > 0.001f && t <= normalised + e);
        const bool lit = power::forced(t) || (on && valueLit);
        const float angle = juce::degreesToRadians(kSweepStartDegrees + kSweepTotalDegrees * t);
        const juce::Point<float> dir(std::sin(angle), -std::cos(angle));
        const juce::Line<float> tick(centre + dir * (tickRadius - tickLength * 0.5f),
                                     centre + dir * (tickRadius + tickLength * 0.5f));
        if (lit) {
            g.setColour(ring.withAlpha(0.22f));
            g.drawLine(tick, tickWidth + (big ? 5.0f : 3.5f));
        }
        g.setColour(lit ? ring : ring.withAlpha(0.13f));
        g.drawLine(tick, tickWidth);
    }

    // Cuerpo: 62 % del diametro (68 % en los grandes), con sombra 0 4px 10px al 70 %.
    const float bodyRadius = size * (big ? 0.68f : 0.62f) * 0.5f;
    const juce::Rectangle<float> bodyArea(centre.x - bodyRadius, centre.y - bodyRadius,
                                          bodyRadius * 2.0f, bodyRadius * 2.0f);
    for (int i = 3; i >= 1; --i) {
        const float spread = static_cast<float>(i) * 2.5f;
        g.setColour(juce::Colours::black.withAlpha(0.22f));
        g.fillEllipse(bodyArea.translated(0.0f, 4.0f).expanded(spread * 0.6f));
    }
    juce::ColourGradient bodyGradient(colours::knobBodyHi, bodyArea.getX() + bodyArea.getWidth() * 0.35f,
                                      bodyArea.getY() + bodyArea.getHeight() * 0.28f,
                                      colours::knobBodyLo, bodyArea.getRight(), bodyArea.getBottom(),
                                      true);
    bodyGradient.addColour(0.62, colours::knobBodyMid);
    g.setGradientFill(bodyGradient);
    g.fillEllipse(bodyArea);

    // Moleteado: cunas de 3 grados blancas al 5 % cada 7 grados.
    {
        juce::Path knurl;
        for (float a = 0.0f; a < 360.0f; a += 7.0f)
            knurl.addPieSegment(bodyArea, juce::degreesToRadians(a), juce::degreesToRadians(a + 3.0f), 0.0f);
        g.setColour(juce::Colours::white.withAlpha(0.05f));
        g.fillPath(knurl);
    }
    g.setColour(colours::knobEdge);
    g.drawEllipse(bodyArea.reduced(0.5f), 1.0f);
    // Brillo interior arriba, sombra interior abajo.
    {
        juce::Path upper, lower;
        upper.addCentredArc(centre.x, centre.y, bodyRadius - 1.5f, bodyRadius - 1.5f, 0.0f,
                          juce::degreesToRadians(-60.0f), juce::degreesToRadians(60.0f), true);
        lower.addCentredArc(centre.x, centre.y, bodyRadius - 2.0f, bodyRadius - 2.0f, 0.0f,
                             juce::degreesToRadians(120.0f), juce::degreesToRadians(240.0f), true);
        g.setColour(juce::Colours::white.withAlpha(0.10f));
        g.strokePath(upper, juce::PathStrokeType(1.0f));
        g.setColour(juce::Colours::black.withAlpha(0.6f));
        g.strokePath(lower, juce::PathStrokeType(2.0f));
    }

    // Tapa: 70 % del cuerpo.
    const float capRadius = bodyRadius * 0.7f;
    const juce::Rectangle<float> capArea(centre.x - capRadius, centre.y - capRadius,
                                         capRadius * 2.0f, capRadius * 2.0f);
    g.setColour(juce::Colours::black.withAlpha(0.8f));
    g.fillEllipse(capArea.translated(0.0f, 1.0f).expanded(0.5f));
    juce::ColourGradient capGradient(colours::knobCapHi, capArea.getX() + capArea.getWidth() * 0.38f,
                                     capArea.getY() + capArea.getHeight() * 0.30f, colours::knobCapLo,
                                     capArea.getRight(), capArea.getBottom(), true);
    g.setGradientFill(capGradient);
    g.fillEllipse(capArea);

    // Puntero: 2,5 px (4 px en los grandes) y 30 % del cuerpo, desde el filo hacia dentro.
    {
        const float pointerAngle =
            juce::degreesToRadians(kSweepStartDegrees + kSweepTotalDegrees * normalised);
        const juce::Point<float> dir(std::sin(pointerAngle), -std::cos(pointerAngle));
        const float outer = bodyRadius * 0.86f;
        const float inner = outer - bodyRadius * 0.6f;
        const juce::Line<float> pointer(centre + dir * inner, centre + dir * outer);
        g.setColour(colours::pointer.withAlpha(on ? 0.5f : 0.2f));
        g.drawLine(pointer, (big ? 4.0f : 2.5f) + 2.0f);
        g.setColour(on ? colours::pointer : colours::pointer.withAlpha(0.5f));
        g.drawLine(pointer, big ? 4.0f : 2.5f);
    }

    // Foco de teclado: anillo de 2 px en vfd al 80 % (§6). Hover: +6 % de brillo.
    if (on && hover_ && !focused_) {
        g.setColour(juce::Colours::white.withAlpha(0.06f));
        g.fillEllipse(bodyArea);
    }
    if (on && focused_) {
        g.setColour(colours::vfd.withAlpha(0.8f));
        g.drawEllipse(bodyArea.expanded(3.0f), 2.0f);
    }

    // Nombre en tinta (contraste AA sobre --surface); el valor en acento, que es el
    // token de "valores destacados" de DESIGN.md §1.
    if (captionVisible_) {
        g.setColour(on ? colours::inkLabel : colours::inkMuted);
        g.setFont(rajdhani(typeSmall).withExtraKerningFactor(0.10f));
        g.drawFittedText(caption_.toUpperCase(), nameArea, juce::Justification::centredTop, 2,
                         0.8f);
    }

    // --- lectura del valor: ventana de LCD con digitos de 7 segmentos ------------------
    // El texto del parametro se parte en numero + unidad: el numero va en segmentos (es lo
    // que se lee de un vistazo) y la unidad en Rajdhani pequena al lado, como en un cuadro
    // de instrumentos. Lo que no es un numero (nombre de tipo, "ON") va entero en texto.
    if (!readoutVisible_) return;
    const juce::String text = parameter_.getText(range_.convertTo0to1(value_), 16).trim();
    int split = text.length();
    for (int i = 0; i < text.length(); ++i) {
        const auto c = text[i];
        if (!(juce::CharacterFunctions::isDigit(c) || c == '-' || c == '+' || c == '.' ||
              c == ',' || c == ' ')) {
            split = i;
            break;
        }
    }
    const juce::String number = text.substring(0, split).trim();
    const juce::String unit = text.substring(split).trim();
    const bool numeric = !number.isEmpty() && SegmentDisplay::isSegmentable(number);

    const juce::Rectangle<int> window = valueArea.withSizeKeepingCentre(
        juce::jmin(valueArea.getWidth(), 34 + (unit.isEmpty() ? 0 : 34)), valueHeight - 1);
    ui::skin::paintLcd(g, window.toFloat(), on);
    const juce::Colour lcdColour =
        accent_.getAlpha() > 0 ? accent_ : colours::lcdInk;
    const juce::Colour lit = on ? lcdColour : lcdColour.withMultipliedBrightness(0.42f);

    if (numeric) {
        // Margen de 3 px arriba y abajo dentro del cristal: los digitos no tocan el marco.
        const float digitsHeight = static_cast<float>(window.getHeight()) - 7.0f;
        const float numberWidth = SegmentDisplay::measure(number, digitsHeight);
        const juce::Font unitFont = rajdhani(8.5f);
        const float unitWidth =
            unit.isEmpty() ? 0.0f : juce::GlyphArrangement::getStringWidth(unitFont, unit);
        const float total = numberWidth + (unit.isEmpty() ? 0.0f : 4.0f + unitWidth);
        const float x = static_cast<float>(window.getCentreX()) - total * 0.5f;
        SegmentDisplay::draw(g,
                             juce::Rectangle<float>(x, static_cast<float>(window.getY()),
                                                    numberWidth,
                                                    static_cast<float>(window.getHeight())),
                             number, digitsHeight, lit, on);
        if (!unit.isEmpty()) {
            g.setColour(lit.withAlpha(on ? 0.85f : 0.7f));
            g.setFont(unitFont);
            g.drawText(unit.toUpperCase(),
                       juce::Rectangle<float>(x + numberWidth + 4.0f,
                                              static_cast<float>(window.getY()),
                                              unitWidth + 2.0f,
                                              static_cast<float>(window.getHeight())),
                       juce::Justification::centredLeft, false);
        }
    } else {
        g.setColour(lit);
        g.setFont(rajdhani(9.5f).withExtraKerningFactor(0.08f));
        g.drawFittedText(text.toUpperCase(), window.reduced(3, 0),
                         juce::Justification::centred, 1, 0.8f);
    }
    ui::skin::paintGlass(g, window.toFloat());
}

void ParamKnob::mouseEnter(const juce::MouseEvent&) {
    hover_ = true;
    repaint();
}

void ParamKnob::mouseExit(const juce::MouseEvent&) {
    hover_ = false;
    repaint();
}

void ParamKnob::mouseDown(const juce::MouseEvent&) {
    if (!controlEnabled()) return;
    active_ = true;
    gestureStartValue_ = value_;
    attachment_->beginGesture();
    grabKeyboardFocus();
    repaint();
}

void ParamKnob::mouseDrag(const juce::MouseEvent& e) {
    if (!active_) return;
    const float pixels = static_cast<float>(-e.getDistanceFromDragStartY());
    float denormalised = gestureStartValue_;
    if (range_.interval > 0.0f) {
        denormalised += std::round(pixels / kPixelsPerEnumStep) * range_.interval;
    } else {
        const float scale = e.mods.isShiftDown() ? 0.15f : 1.0f;
        denormalised += pixels / kDragPixelsForFullRange * (range_.end - range_.start) * scale;
    }
    setValueFromUi(denormalised, false);
}

void ParamKnob::mouseUp(const juce::MouseEvent&) {
    if (!active_) return;
    attachment_->endGesture();
    active_ = false;
    repaint();
}

void ParamKnob::mouseDoubleClick(const juce::MouseEvent&) {
    if (!controlEnabled()) return;
    setValueFromUi(range_.convertFrom0to1(parameter_.getDefaultValue()), true);
}

void ParamKnob::focusGained(FocusChangeType type) {
    markFocus(*this, type, true);
    focused_ = true;
    repaint();
}

void ParamKnob::focusLost(FocusChangeType type) {
    markFocus(*this, type, false);
    focused_ = false;
    repaint();
}

bool ParamKnob::keyPressed(const juce::KeyPress& key) {
    if (!controlEnabled()) return false;
    const float step = range_.interval > 0.0f ? range_.interval
                                             : (range_.end - range_.start) * 0.01f;
    if (key == juce::KeyPress::upKey || key == juce::KeyPress::rightKey) {
        setValueFromUi(value_ + step, true);
        return true;
    }
    if (key == juce::KeyPress::downKey || key == juce::KeyPress::leftKey) {
        setValueFromUi(value_ - step, true);
        return true;
    }
    if (key == juce::KeyPress::homeKey) {
        setValueFromUi(range_.convertFrom0to1(parameter_.getDefaultValue()), true);
        return true;
    }
    return false;
}

}  // namespace rtxx::ui
