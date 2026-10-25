# Envoltorio de plugin (fase 9 del ROADMAP): JUCE 8.0.13, AU + VST3.
#
# JUCE no se versiona: third_party/ esta en .gitignore. El árbol de trabajo lo trae de
# la release fijada (tag 8.0.13, sha256 del .tar.gz en CONTINUAR.md §Fase 9). Si falta,
# se descarga el MISMO tag con FetchContent para que un clon limpio tambien compile.

set(RTXX_JUCE_TAG "8.0.13")
set(RTXX_JUCE_DIR "${CMAKE_SOURCE_DIR}/third_party/JUCE")

if(EXISTS "${RTXX_JUCE_DIR}/CMakeLists.txt")
  add_subdirectory("${RTXX_JUCE_DIR}" "${CMAKE_BINARY_DIR}/third_party/juce" EXCLUDE_FROM_ALL)
else()
  include(FetchContent)
  FetchContent_Declare(juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG ${RTXX_JUCE_TAG}
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(juce)
endif()

# --- Recursos de la UI: fuentes OFL y logo. Un solo origen (los ficheros de assets/),
# compilados a BinaryData y enlazados tanto al plugin como a las herramientas de test.
juce_add_binary_data(rtxx_ui_assets
  SOURCES
    ui/assets/fonts/Orbitron-500.ttf
    ui/assets/fonts/Orbitron-600.ttf
    ui/assets/fonts/Orbitron-700.ttf
    ui/assets/fonts/Orbitron-800.ttf
    ui/assets/fonts/Orbitron-900.ttf
    ui/assets/fonts/Rajdhani-500.ttf
    ui/assets/logomark.svg)

# CACHE INTERNAL: la demo (src/demo) compila las mismas fuentes del envoltorio, y una
# variable normal no cruza de un directorio a otro.
# Rutas ABSOLUTAS: la demo (src/demo) compila estas mismas fuentes desde otro
# directorio, y un nombre relativo se resolveria contra el suyo.
set(RTXX_PLUGIN_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/PluginProcessor.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/PluginEditor.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/HostParamDisplay.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/HostModules.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/PresetManager.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/ui/Fonts.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/ui/ParamKnob.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/ui/ParamToggle.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/ui/ModulePanel.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/ui/Skin.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/ui/HardwareLookAndFeel.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/ui/SegmentDisplay.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/ui/SegBar.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/ui/ReduceMotion.mm"
  "${CMAKE_CURRENT_SOURCE_DIR}/ui/ModuleViz.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/ui/ParamFader.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/ui/ParamRangeSlider.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/ui/ParamButton.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/ui/ParamSelector.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/ui/MacroBand.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/ui/MasterStrip.cpp"
  CACHE INTERNAL "Fuentes del envoltorio del plugin (editor + procesador)")

# Plugin. Codigos de src/product/ProductInfo.h; los cuatro caracteres se repiten aqui
# porque juce_add_plugin los necesita en tiempo de configuracion, no en C++.
juce_add_plugin(Retroexx
  VERSION 0.1.0
  COMPANY_NAME "Vexx Audio"
  BUNDLE_ID "com.vexxaudio.retroexx"
  PLUGIN_MANUFACTURER_CODE Vexx
  PLUGIN_CODE Rtxx
  PRODUCT_NAME "Retroexx"
  PLUGIN_NAME "Retroexx"
  DESCRIPTION "Character, noise and space processor"
  FORMATS AU VST3 Standalone
  AU_MAIN_TYPE kAudioUnitType_Effect
  VST3_CATEGORIES Fx
  IS_SYNTH FALSE
  IS_MIDI_EFFECT FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE
  # El standalone abre el dispositivo de entrada para procesar audio en vivo: macOS exige
  # declarar el uso del microfono o mata la app al pedirlo.
  MICROPHONE_PERMISSION_ENABLED TRUE
  MICROPHONE_PERMISSION_TEXT "Retroexx processes live audio from the input device you choose.")

# Nunca hubo un VST2 de este plugin: los ids de parametro no tienen que sobrevivir a un
# VST2 historico (el aviso de JUCE es solo para proyectos que ya publicaron VST2).
# LEGACY_PARAM_IDS: el AU publica el id de parametro tal cual (0..57) en vez de un hash,
# de modo que AudioUnitParameterID == id de HostParamTable.
target_compile_definitions(Retroexx
  PUBLIC
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_FORCE_USE_LEGACY_PARAM_IDS=1
    # macOS: pinta cada zona sucia por separado (capa Metal). Sin esto CoreGraphics junta
    # las pantallas animadas en su rectangulo envolvente y repinta casi toda la ventana a 60 Hz.
    JUCE_COREGRAPHICS_RENDER_WITH_MULTIPLE_PAINT_CALLS=1)

target_sources(Retroexx PRIVATE ${RTXX_PLUGIN_SOURCES})
target_include_directories(Retroexx PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
# El header BinaryData.h se genera al compilar rtxx_ui_assets: hay que ordenar los
# targets para que exista antes de compilar Fonts.cpp.
add_dependencies(Retroexx rtxx_ui_assets)
target_link_libraries(Retroexx
  PRIVATE
    rtxxcore
    rtxx_ui_assets
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_extra
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

# --- Pruebas del envoltorio: ejercitan el AudioProcessor real (no una copia del motor)
# y renderizan el editor a PNG sin abrir ventana. Son aplicaciones de consola, no
# plugins: comparten los mismos fuentes y se registran en ctest.
foreach(rtxx_plugin_test test_plugin_processor test_ui_snapshot)
  juce_add_console_app(${rtxx_plugin_test}
    PRODUCT_NAME ${rtxx_plugin_test}
    VERSION "0.1.0")
  target_sources(${rtxx_plugin_test} PRIVATE
    ${RTXX_PLUGIN_SOURCES}
    "${CMAKE_SOURCE_DIR}/tests/${rtxx_plugin_test}.cpp")
  add_dependencies(${rtxx_plugin_test} rtxx_ui_assets)
  target_include_directories(${rtxx_plugin_test}
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/src")
  target_link_libraries(${rtxx_plugin_test}
    PRIVATE
      rtxxcore
      rtxx_ui_assets
      juce::juce_audio_utils
      juce::juce_dsp
      juce::juce_gui_extra
    PUBLIC
      juce::juce_recommended_config_flags
      juce::juce_recommended_warning_flags)
  add_test(NAME ${rtxx_plugin_test} COMMAND ${rtxx_plugin_test})
endforeach()

# La captura del editor se escribe en test/ui/ del arbol de fuentes (ruta del codigo,
# no del build).
target_compile_definitions(test_ui_snapshot PRIVATE RTXX_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
