#pragma once
// Identidad del producto. Un solo sitio para el nombre, la marca, los códigos de
// plugin y el magic del estado. Nada de cadenas de marca repartidas por el código.

namespace rtxx {

inline constexpr const char* kProductName = "Retroexx";
inline constexpr const char* kManufacturer = "Vexx Audio";
inline constexpr const char* kVersion = "0.1.0";
inline constexpr const char* kBuildName = "Retroexx";

// Identificadores de plugin (AU / VST3). Cuatro caracteres, ASCII imprimible.
inline constexpr const char* kPluginCode = "Rtxx";        // subtype AU / VST3 unique id
inline constexpr const char* kManufacturerCode = "Vexx";  // manufacturer / vendor
inline constexpr const char* kBundleId = "com.vexxaudio.retroexx";

// Magic del blob de estado. NO es el del motor: no cargamos presets de
// ni los emitimos. Ver PLAN.md §0.
inline constexpr const char* kStateMagic = "RTXX";

// Descripción corta para el instalador y el motor.
inline constexpr const char* kDescription = "Character, noise and space processor";

} // namespace rtxx
