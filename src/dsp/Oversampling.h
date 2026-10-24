// Retroexx — oversampling.
//
// C++17, sin dependencias, sin JUCE.
//
//
//   El plugin apunta SIEMPRE a ~192 kHz internos. El factor se elige como
//   el que minimiza |192000 - N*fs| sobre N en {1,2,4,8} (Tube/Tape) o {2,4}
// (SideSaturation). Regla de elección (TubeDist):
//
// d2 = |192000 - 2*sr|; si d2 > 19999 -> d2 = 20000
//        d4 = |192000 - 4*sr|
//        n  = (d2 <= d4) ? (d2>>5 < 625 ? 1 : 0) : 2
//        d8 = |192000 - 8*sr|
//        si min(d2,d4) <= d8 -> n = n ; si no n = 3
//        factor = 1 << n
//
//   Con sr >= 22.05 kHz eso coincide con el argmin exacto de |192000 - N*fs|
//   (comprobado en los tests). Para sr muy bajas el clamp de 20 kHz hace que el
// motor prefiera 1x; aquí se implementa el argmin, que es la regla declarada
// en el contrato del frente.
//
//   Instanciación observada: juce::dsp::Oversampling<float>(numCh, factor, 1,1,1) +
//   initProcessing(64); objeto de 0x1C0 bytes. El tipo de filtro de JUCE
//   (equiripple FIR vs polyphase IIR) NO se pudo determinar estáticamente
//   (informe §2 y §12.9). Aquí se implementa un FIR de media banda equiripple-like
//   de 2 etapas por factor 2 (sinc enventanada Blackman, 65 taps), que es la
//   alternativa declarada en el contrato. DIFERENCIA: no es la tabla exacta de
//   JUCE (Oversampling2TimesEquirippleFIR); ripple de banda pasante y
//   atenuación de banda eliminada son los del diseño ventaneado.
//
#pragma once

#include <cmath>
#include <cstddef>
#include <vector>

#include "Math.h"

namespace rtxx {
namespace dsp {

// Objetivo de frecuencia interna del motor.
constexpr double kOversamplingTargetFs = 192000.0;

// Conjuntos de factores observados en los bloques.
constexpr int kFactorsTube[4] = {1, 2, 4, 8};   // TubeDist
constexpr int kFactorsSidechain[2] = {2, 4};    // SideSaturation (nunca 1x)

inline int chooseFactor(double fs, const int* allowed, int nAllowed) {
    if (allowed == nullptr || nAllowed <= 0 || !(fs > 0.0)) return 1;
    int best = allowed[0];
    double bestD = std::fabs(kOversamplingTargetFs - static_cast<double>(best) * fs);
    for (int i = 1; i < nAllowed; ++i) {
        const double d =
            std::fabs(kOversamplingTargetFs - static_cast<double>(allowed[i]) * fs);
        if (d < bestD) {  // empate -> se queda el primero (= el menor factor)
            bestD = d;
            best = allowed[i];
        }
    }
    return best;
}

template <std::size_t N>
inline int chooseFactor(double fs, const int (&allowed)[N]) {
    return chooseFactor(fs, allowed, static_cast<int>(N));
}

class Oversampling {
public:
    static constexpr int kTaps = 65;                       // media banda, fase lineal
    static constexpr int kGroupDelay = (kTaps - 1) / 2;    // 32 muestras por etapa
    static constexpr int kMaxStages = 3;                   // 2^3 = 8x

    Oversampling() { reset(); }

// Elige el factor con la regla del motor y prepara el estado.
    void prepare(int numChannels, int maxBlock, double fs, const int* allowed, int nAllowed) {
        fs_ = fs > 0.0 ? fs : 48000.0;
        const int f = chooseFactor(fs_, allowed, nAllowed);
        int stages = 0;
        while ((1 << stages) < f && stages < kMaxStages) ++stages;
        prepareStages(numChannels, maxBlock, stages);
    }

    // Permite forzar el factor (útil para tests y para bloques que lo reciben del motor).
    void prepareWithFactor(int numChannels, int maxBlock, double fs, int factor) {
        fs_ = fs > 0.0 ? fs : 48000.0;
        int stages = 0;
        while ((1 << stages) < factor && stages < kMaxStages) ++stages;
        prepareStages(numChannels, maxBlock, stages);
    }

    template <std::size_t N>
    void prepare(int numChannels, int maxBlock, double fs, const int (&allowed)[N]) {
        prepare(numChannels, maxBlock, fs, allowed, static_cast<int>(N));
    }

    void reset() {
        for (auto& v : upState_) v.reset();
        for (auto& v : downState_) v.reset();
    }

    int stages() const { return stages_; }
    int factor() const { return 1 << stages_; }
    int maxBlock() const { return maxBlock_; }
    int channels() const { return numChannels_; }
    double inputRate() const { return fs_; }
    double internalRate() const { return fs_ * static_cast<double>(factor()); }

    // Latencia de ida y vuelta, en muestras de la frecuencia de entrada.
    // Cada etapa aporta (kTaps-1)/2 muestras de retardo de grupo a la frecuencia de la
    // etapa (2^(s+1)*fs), es decir 32/2^(s+1) muestras de host por filtro: subida y
    // bajada => 32/2^s muestras de host por etapa. Total = suma_s 32>>s.
    //   1 etapa (2x) -> 32 ;  2 etapas (4x) -> 48 ;  3 etapas (8x) -> 56
    int latencySamples() const {
        int lat = 0;
        for (int s = 0; s < stages_; ++s) lat += kGroupDelay >> s;
        return lat;
    }

    // Sube 'n' muestras de un canal. Devuelve n*factor muestras en un buffer interno.
    const float* upsample(int ch, const float* in, int n) {
        if (ch < 0 || ch >= numChannels_ || n > maxBlock_) return nullptr;
        const int ci = ch * kMaxStages;
        if (stages_ == 0) {
            float* dst = upOut_[ch].data();
            for (int i = 0; i < n; ++i) dst[i] = in[i];
            return dst;
        }
        const float* cur = in;
        int curN = n;
        for (int s = 0; s < stages_; ++s) {
            float* dst = (s == stages_ - 1)
                             ? upOut_[ch].data()
                             : scratch_.data() + scratchOffset(ch, s);
            upsample2(upState_[ci + s], cur, curN, dst);
            cur = dst;
            curN *= 2;
        }
        return upOut_[ch].data();
    }

    // Baja 'n*factor' muestras a 'n' muestras de un canal.
    void downsample(int ch, const float* os, int n, float* out) {
        if (ch < 0 || ch >= numChannels_ || n > maxBlock_) return;
        const int ci = ch * kMaxStages;
        if (stages_ == 0) {
            for (int i = 0; i < n; ++i) out[i] = os[i];
            return;
        }
        const float* cur = os;
        int curN = n << stages_;
        for (int s = stages_ - 1; s >= 0; --s) {
            const int outN = curN / 2;
            float* dst = (s == 0) ? out : scratch_.data() + scratchOffset(ch, s);
            downsample2(downState_[ci + s], cur, outN, dst);
            cur = dst;
            curN = outN;
        }
    }

    // Coeficientes del FIR de media banda (sinc * Blackman, suma = 1).
    static const std::vector<float>& coefficients() {
        static const std::vector<float> h = designHalfBand();
        return h;
    }

private:
    struct FirState {
        float buf[kTaps] = {};
        int pos = 0;
        void reset() {
            for (int i = 0; i < kTaps; ++i) buf[i] = 0.0f;
            pos = 0;
        }
        inline void push(float x) {
            pos = (pos + 1 == kTaps) ? 0 : pos + 1;
            buf[pos] = x;
        }
        inline float at(int j) const {
            int i = pos - j;
            if (i < 0) i += kTaps;
            return buf[i];
        }
        inline float convolve(const float* h) const {
            double acc = 0.0;
            for (int j = 0; j < kTaps; ++j) acc += static_cast<double>(h[j]) * at(j);
            return static_cast<float>(acc);
        }
    };

    static std::vector<float> designHalfBand() {
        const int n = kTaps;
        const int m = kGroupDelay;
        const double pi = 3.14159265358979323846;
        std::vector<float> h(n, 0.0f);
        double sum = 0.0;
        for (int i = 0; i < n; ++i) {
            const double k = static_cast<double>(i - m);
            double s;
            if (k == 0.0) {
                s = 0.5;
            } else {
                s = 0.5 * std::sin(pi * 0.5 * k) / (pi * 0.5 * k);
            }
            const double wn = 0.42 - 0.5 * std::cos(2.0 * pi * i / (n - 1)) +
                              0.08 * std::cos(4.0 * pi * i / (n - 1));
            h[i] = static_cast<float>(s * wn);
            sum += static_cast<double>(h[i]);
        }
        for (int i = 0; i < n; ++i) h[i] = static_cast<float>(h[i] / sum);
        return h;
    }

    void prepareStages(int numChannels, int maxBlock, int stages) {
        numChannels_ = numChannels > 0 ? numChannels : 1;
        maxBlock_ = maxBlock > 0 ? maxBlock : 1;
        stages_ = stages < 0 ? 0 : (stages > kMaxStages ? kMaxStages : stages);
        const int factor = 1 << stages_;

        upState_.assign(static_cast<size_t>(numChannels_ * kMaxStages), FirState());
        downState_.assign(static_cast<size_t>(numChannels_ * kMaxStages), FirState());

        // Dos buffers de trabajo por canal; el stride se fija con el factor máximo (8x)
        // para que el índice sea independiente del factor activo.
        const size_t scratchLen = static_cast<size_t>(numChannels_) * 2u *
                                  static_cast<size_t>(maxBlock_) *
                                  static_cast<size_t>(1 << kMaxStages);
        scratch_.assign(scratchLen, 0.0f);

        upOut_.assign(static_cast<size_t>(numChannels_), std::vector<float>());
        for (auto& v : upOut_) v.assign(static_cast<size_t>(maxBlock_) * factor, 0.0f);
        reset();
    }

    size_t scratchOffset(int ch, int stage) const {
        return (static_cast<size_t>(ch) * 2u + static_cast<size_t>(stage & 1)) *
               static_cast<size_t>(maxBlock_) * static_cast<size_t>(1 << kMaxStages);
    }

    static void upsample2(FirState& st, const float* in, int n, float* out) {
        const float* h = coefficients().data();
        for (int i = 0; i < n; ++i) {
            for (int k = 0; k < 2; ++k) {
                st.push(k == 0 ? in[i] : 0.0f);
                out[2 * i + k] = 2.0f * st.convolve(h);  // x2 por el zero-stuffing
            }
        }
    }

    static void downsample2(FirState& st, const float* in, int outN, float* out) {
        const float* h = coefficients().data();
        for (int i = 0; i < outN; ++i) {
            st.push(in[2 * i]);
            const float y = st.convolve(h);
            st.push(in[2 * i + 1]);
            (void)st.convolve(h);
            out[i] = y;
        }
    }

    double fs_ = 48000.0;
    int numChannels_ = 0;
    int maxBlock_ = 1;
    int stages_ = 0;
    std::vector<FirState> upState_;
    std::vector<FirState> downState_;
    std::vector<float> scratch_;
    std::vector<std::vector<float>> upOut_;
};

}  // namespace dsp
}  // namespace rtxx
