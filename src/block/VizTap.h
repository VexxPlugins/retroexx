#pragma once
// Toma de audio para los visualizadores (VEXX-DASH90 §8): cola SPSC preasignada de tramas,
// una por quantum de 64 muestras. El hilo de audio solo escribe (push) y el de la UI solo
// lee (pop). Sin cerrojos ni reservas de memoria en el hilo de audio; si la UI no lee y la
// cola se llena, la trama nueva se descarta (no se pisa la que la UI puede estar leyendo).
#include <array>
#include <atomic>
#include <cstdint>

namespace rtxx {

struct VizFrame {
    float outMin[2];   // min / max de la salida del nodo, por canal (L, R)
    float outMax[2];
    float outRms[2];   // RMS de la salida, por canal
    float inRms[2];    // RMS de la entrada, por canal
};

class VizTap {
public:
    static constexpr std::uint32_t kCapacity = 2048;   // potencia de 2

    // Hilo de audio.
    void push(const VizFrame& frame) {
        const std::uint32_t w = write_.load(std::memory_order_relaxed);
        if (w - read_.load(std::memory_order_acquire) >= kCapacity) return;   // llena
        frames_[w & (kCapacity - 1)] = frame;
        write_.store(w + 1, std::memory_order_release);
    }

    // Hilo de la UI. Llama a `sink(frame)` por cada trama pendiente, en orden.
    template <typename Sink>
    int pop(Sink&& sink) {
        const std::uint32_t r = read_.load(std::memory_order_relaxed);
        const std::uint32_t w = write_.load(std::memory_order_acquire);
        for (std::uint32_t i = r; i != w; ++i) sink(frames_[i & (kCapacity - 1)]);
        read_.store(w, std::memory_order_release);
        return static_cast<int>(w - r);
    }

private:
    std::array<VizFrame, kCapacity> frames_{};
    std::atomic<std::uint32_t> write_{0};
    std::atomic<std::uint32_t> read_{0};
};

} // namespace rtxx
