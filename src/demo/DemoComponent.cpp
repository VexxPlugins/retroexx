#include "DemoComponent.h"

#include "ui/Skin.h"

namespace rtxx::demo {
namespace {

constexpr int kEditorWidth = 1240;
constexpr int kEditorHeight = 957;

}  // namespace

DemoComponent::DemoComponent() {
    setOpaque(true);
    setSize(kEditorWidth, kEditorHeight);

    // El editor del plugin, tal cual se publica.
    editor_.reset(processor_.createEditor());
    addAndMakeVisible(*editor_);
}

DemoComponent::~DemoComponent() {
    editor_.reset();  // el editor escucha a los parametros: muere antes que el processor
}

void DemoComponent::paint(juce::Graphics& g) {
    ui::skin::paintWindow(g, getLocalBounds());
}

void DemoComponent::resized() {
    if (editor_ != nullptr) editor_->setBounds(0, 0, kEditorWidth, kEditorHeight);
}

}  // namespace rtxx::demo
