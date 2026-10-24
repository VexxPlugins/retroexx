#pragma once
// Preferencia del sistema "Reducir movimiento" (VEXX-DASH90 P6). Con ella activa no hay
// barrido de encendido, el sombreado de modulo apagado entra sin fundido y el FLUX no late.
// Se lee una vez por proceso.

namespace rtxx::ui {

bool reduceMotion();

}  // namespace rtxx::ui
