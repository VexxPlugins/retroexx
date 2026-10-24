#pragma once
// Marca de "foco de teclado" de un control (VEXX-DASH90 §6): el foco que llega por Tab activa
// el ordenador de viaje; el que llega por un clic no (los controles toman el foco al hacer
// clic y la lectura se quedaria pegada al ultimo control tocado). Cada control la deja en
// sus propiedades y la columna la lee.

#include <juce_gui_basics/juce_gui_basics.h>

namespace rtxx::ui {

inline const juce::Identifier kKeyboardFocus{"rtxxKeyboardFocus"};

inline void markFocus(juce::Component& c, juce::Component::FocusChangeType type, bool gained) {
    c.getProperties().set(kKeyboardFocus, gained && type == juce::Component::focusChangedByTabKey);
}

inline bool focusedByKeyboard(const juce::Component& c) {
    return c.getProperties().getWithDefault(kKeyboardFocus, false);
}

}  // namespace rtxx::ui
