# Demo ejecutable: el editor real del plugin en una ventana de escritorio, sin audio propio.
# No es un plugin: es una app de escritorio.
#
# Se compila solo con -DRTXX_WITH_JUCE=ON, porque necesita JUCE y el envoltorio del plugin.

juce_add_gui_app(RetroexxDemo
  PRODUCT_NAME "RetroexxDemo"
  COMPANY_NAME "Vexx Audio"
  BUNDLE_ID "com.vexxaudio.retroexxdemo"
  VERSION "0.1.0"
  MICROPHONE_PERMISSION_ENABLED FALSE)

# Las fuentes del envoltorio (PluginProcessor/PluginEditor/ui) vienen de la variable que
# publica src/plugin/CMakeLists.txt. Si faltara, el enlace fallaria sin main: se comprueba.
if(NOT DEFINED RTXX_PLUGIN_SOURCES)
  message(FATAL_ERROR "RTXX_PLUGIN_SOURCES no esta definida: la demo necesita el envoltorio")
endif()
message(STATUS "RetroexxDemo: ${RTXX_PLUGIN_SOURCES}")

target_sources(RetroexxDemo PRIVATE
  ${RTXX_PLUGIN_SOURCES}
  "${CMAKE_CURRENT_SOURCE_DIR}/DemoMain.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/DemoComponent.cpp")

target_include_directories(RetroexxDemo PRIVATE
  "${CMAKE_SOURCE_DIR}/src/plugin"
  "${CMAKE_SOURCE_DIR}/src")

add_dependencies(RetroexxDemo rtxx_ui_assets)
target_compile_definitions(RetroexxDemo PRIVATE
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_COREGRAPHICS_RENDER_WITH_MULTIPLE_PAINT_CALLS=1)   # igual que el plugin

target_link_libraries(RetroexxDemo
  PRIVATE
    rtxxcore
    rtxx_ui_assets
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_extra
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
