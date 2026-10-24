cmake_minimum_required(VERSION 3.22)
project(retroexx LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

# --- Núcleo de DSP: C++17 puro, sin JUCE y sin dependencias externas.
# Todo lo que no sea el envoltorio de plugin vive aquí para poder probarse sin host.
file(GLOB_RECURSE RTXX_CORE_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/block/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/blocks/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/engine/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/io/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/model/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/gen/*.cpp"
)
list(FILTER RTXX_CORE_SOURCES EXCLUDE REGEX "/build/")

add_library(rtxxcore STATIC ${RTXX_CORE_SOURCES})
target_include_directories(rtxxcore PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_options(rtxxcore PRIVATE -Wall -Wextra -Wpedantic)

# --- Tests: un ejecutable por fichero de tests/. Falla el build si alguno falla.
enable_testing()
file(GLOB_RECURSE RTXX_TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp")
list(FILTER RTXX_TEST_SOURCES EXCLUDE REGEX "/build/")
# Los tests del envoltorio necesitan JUCE y se registran en src/plugin/CMakeLists.txt
# (solo con -DRTXX_WITH_JUCE=ON); el resto del proyecto no depende de JUCE.
list(FILTER RTXX_TEST_SOURCES EXCLUDE REGEX "/(test_plugin_|test_ui_)")
foreach(test_src ${RTXX_TEST_SOURCES})
  get_filename_component(test_name "${test_src}" NAME_WE)
  add_executable(${test_name} "${test_src}")
  target_link_libraries(${test_name} PRIVATE rtxxcore)
  target_compile_options(${test_name} PRIVATE -Wall -Wextra)
  add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

# --- Envoltorio de plugin (JUCE). Se activa aparte para no bloquear el núcleo.
# JUCE 8.0.13 en third_party/JUCE (no versionado; ver src/plugin/CMakeLists.txt).
option(RTXX_WITH_JUCE "Compilar el plugin AU/VST3 con JUCE" OFF)
if(RTXX_WITH_JUCE)
  add_subdirectory(src/plugin)
  add_subdirectory(src/demo)
endif()

# --- Herramienta de medida: render offline del motor (WAV de entrada y de salida).
add_executable(rtxx_render "${CMAKE_CURRENT_SOURCE_DIR}/tools/rtxx_render.cpp")
target_link_libraries(rtxx_render PRIVATE rtxxcore)
target_compile_options(rtxx_render PRIVATE -Wall -Wextra)
set_target_properties(rtxx_render PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
