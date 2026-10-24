#pragma once
// Presets de USUARIO: guardar y cargar el estado del plugin en disco.
//
// Los de fábrica son los 10 de `src/model/FactoryPresets.cpp` y viven como programas del host;
// esto es lo otro que un usuario espera: guardar lo que tiene entre manos con un nombre y
// volver a cargarlo. El formato es el MISMO blob de estado del host (`PluginProcessor::
// getStateInformation`, que ya lleva el magic "RTXX", la versión y un checksum), así que cargar
// un preset es exactamente lo mismo que restaurar un estado guardado por el DAW: no hay un
// segundo formato que mantener en paralelo.
//
// Nada de esto toca los presets del motor: son ficheros del usuario, en su carpeta.

#include <juce_core/juce_core.h>

#include <vector>

namespace rtxx {

class PresetManager {
public:
    // Raíz configurable para poder probarlo sin escribir en la carpeta del usuario.
    explicit PresetManager(juce::File root = defaultRoot());

    static juce::File defaultRoot();

    struct Entry {
        juce::String name;   // nombre visible (sin extensión)
        juce::File file;     // fichero en disco
    };

    // Presets de usuario que hay ahora mismo, ordenados por nombre.
    std::vector<Entry> scan() const;

    // Guarda `state` como `<name>.rtxxpreset`. Devuelve false y deja el motivo en `error`
    // si el nombre queda vacío después de sanearlo o si no se puede escribir.
    bool save(const juce::String& name, const juce::MemoryBlock& state, juce::String& error) const;

    // Carga el blob de un preset. Falso si el fichero no se puede leer o está vacío.
    bool load(const juce::File& file, juce::MemoryBlock& state, juce::String& error) const;

    // Borra un preset de usuario.
    bool remove(const juce::File& file) const;

    juce::File root() const { return root_; }

    static constexpr const char* kExtension = ".rtxxpreset";

    // Sanea un nombre para que sea un fichero válido (la UI lo usa para saber cómo se
    // llamará el preset que acaba de guardar).
    static juce::String sanitiseName(const juce::String& name);

private:
    juce::File root_;
};

}  // namespace rtxx
