// Punto de entrada de la demo: una ventana con el editor real del plugin y una tira de
// transporte para encender/apagar el sonido y cambiar de material.
//
//   RetroexxDemo                      abre la ventana con el editor
//   RetroexxDemo --screenshot <png>   guarda una captura de la ventana completa
#include <juce_gui_extra/juce_gui_extra.h>

#include <cstdio>

#include "DemoComponent.h"
#include "ui/Fonts.h"
#include "ui/HardwareLookAndFeel.h"

namespace {
constexpr const char* kTitle = "Retroexx - demo (Vexx Audio)";
}  // namespace

class DemoWindow : public juce::DocumentWindow {
public:
    DemoWindow()
        : juce::DocumentWindow(kTitle, rtxx::ui::colours::window,
                               juce::DocumentWindow::allButtons) {
        content_ = new rtxx::demo::DemoComponent();
        setUsingNativeTitleBar(true);
        setContentOwned(content_, true);
        setResizable(false, false);
        centreWithSize(getWidth(), getHeight());
        setVisible(true);
    }

    void closeButtonPressed() override {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }

    rtxx::demo::DemoComponent* content() { return content_; }

private:
    rtxx::demo::DemoComponent* content_ = nullptr;
};

// Fuera del namespace anonimo: START_JUCE_APPLICATION genera un main() en el ambito global
// que necesita ver el nombre del tipo tal cual.
class DemoApplication : public juce::JUCEApplication {
public:
    const juce::String getApplicationName() override { return "RetroexxDemo"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    // Varias instancias permitidas: con la demo abierta, `--screenshot` reenviaria la linea
    // de comandos a la ventana que ya corre y no capturaria nada.
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise(const juce::String& commandLine) override {
        juce::LookAndFeel::setDefaultLookAndFeel(&lookAndFeel_);
        juce::ignoreUnused(rtxx::ui::fontsLoaded());

        const auto args = juce::StringArray::fromTokens(commandLine, true);
        if (args.contains("--screenshot")) {
            const int index = args.indexOf("--screenshot");
            const juce::String path = index >= 0 && index + 1 < args.size()
                                          ? args[index + 1]
                                          : juce::String("/tmp/retroexx_demo.png");
            setApplicationReturnValue(screenshot(path) ? 0 : 1);
            quit();
            return;
        }
        window_ = std::make_unique<DemoWindow>();
    }

    void shutdown() override {
        window_.reset();
        juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
    }

    void systemRequestedQuit() override { quit(); }

private:
    bool screenshot(const juce::String& path) {
        rtxx::demo::DemoComponent demo;
        demo.setSize(demo.getWidth(), demo.getHeight());
        demo.resized();
        const juce::Image image = demo.createComponentSnapshot(demo.getLocalBounds());
        if (!image.isValid()) return false;
        const juce::File file(path);
        file.deleteFile();
        juce::FileOutputStream stream(file);
        if (!stream.openedOk()) return false;
        juce::PNGImageFormat png;
        const bool written = png.writeImageToStream(image, stream);
        stream.flush();
        std::printf("  captura: %s (%dx%d)\n", path.toRawUTF8(), image.getWidth(),
                    image.getHeight());
        return written;
    }

    rtxx::ui::HardwareLookAndFeel lookAndFeel_;
    std::unique_ptr<DemoWindow> window_;
};

START_JUCE_APPLICATION(DemoApplication)
