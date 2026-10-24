#pragma once
// Barrido de encendido (VEXX-DASH90 §9): al abrir el editor todos los segmentos se
// encienden 300 ms y luego caen en 250 ms, de izquierda a derecha dentro de cada display.
//
// Estado del hilo de mensajes (todo el pintado ocurre ahi). Lo arranca el editor la primera
// vez que se muestra; una captura sin ventana (el test) nunca lo ve activo.

namespace rtxx::ui::power {

inline double& elapsedMs() {
    static double elapsed = -1.0;   // < 0: inactivo
    return elapsed;
}

inline void start() { elapsedMs() = 0.0; }
inline bool active() { return elapsedMs() >= 0.0; }

inline void advance(double ms) {
    if (!active()) return;
    elapsedMs() += ms;
    if (elapsedMs() > 550.0) elapsedMs() = -1.0;
}

// true si el segmento en la posicion `position` (0 = izquierda, 1 = derecha de su display)
// tiene que verse encendido por el barrido.
inline bool forced(float position) {
    const double t = elapsedMs();
    if (t < 0.0) return false;
    if (t < 300.0) return true;
    return static_cast<double>(position) >= (t - 300.0) / 250.0;
}

}  // namespace rtxx::ui::power
