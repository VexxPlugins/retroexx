#include "PresetManager.h"

#include <algorithm>

namespace rtxx {
namespace {

// Caracteres que no queremos en un nombre de fichero multiplataforma.
juce::String stripName(const juce::String& text) {
    juce::String out;
    for (auto c : text)
        if (juce::CharacterFunctions::isLetterOrDigit(c) || c == ' ' || c == '-' || c == '_' ||
            c == '(' || c == ')' || c == '+')
            out += c;
    return out.trim();
}

}  // namespace

PresetManager::PresetManager(juce::File root) : root_(std::move(root)) {}

juce::File PresetManager::defaultRoot() {
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Vexx Audio")
        .getChildFile("Retroexx")
        .getChildFile("Presets");
}

juce::String PresetManager::sanitiseName(const juce::String& name) {
    juce::String clean = stripName(name);
    if (clean.length() > 64) clean = clean.substring(0, 64).trim();
    return clean;
}

std::vector<PresetManager::Entry> PresetManager::scan() const {
    std::vector<Entry> out;
    if (!root_.isDirectory()) return out;
    for (const auto& f : root_.findChildFiles(juce::File::findFiles, false, juce::String("*") + kExtension)) {
        Entry e;
        e.name = f.getFileNameWithoutExtension();
        e.file = f;
        out.push_back(e);
    }
    std::sort(out.begin(), out.end(), [](const Entry& a, const Entry& b) {
        return a.name.compareIgnoreCase(b.name) < 0;
    });
    return out;
}

bool PresetManager::save(const juce::String& name, const juce::MemoryBlock& state,
                         juce::String& error) const {
    const juce::String clean = sanitiseName(name);
    if (clean.isEmpty()) {
        error = "el nombre no deja ningún carácter válido";
        return false;
    }
    if (state.getSize() == 0) {
        error = "el estado está vacío";
        return false;
    }
    const auto result = root_.createDirectory();
    if (result.failed()) {
        error = "no se pudo crear " + root_.getFullPathName() + ": " + result.getErrorMessage();
        return false;
    }
    const juce::File file = root_.getChildFile(clean + kExtension);
    if (!file.replaceWithData(state.getData(), state.getSize())) {
        error = "no se pudo escribir " + file.getFullPathName();
        return false;
    }
    return true;
}

bool PresetManager::load(const juce::File& file, juce::MemoryBlock& state, juce::String& error) const {
    if (!file.existsAsFile()) {
        error = "no existe " + file.getFullPathName();
        return false;
    }
    if (!file.loadFileAsData(state) || state.getSize() == 0) {
        error = "no se pudo leer " + file.getFullPathName();
        return false;
    }
    return true;
}

bool PresetManager::remove(const juce::File& file) const {
    return file.existsAsFile() && file.deleteFile();
}

}  // namespace rtxx
