// Retroexx — línea de retardo con interpolación (wobble/espacio).
//
// C++17, sin dependencias, sin JUCE.
//
// Dimensionado con la modulación al máximo:
//
//   sr = 44100:  m = (int)(sr*0.01) = 441 ; modRange = 1 + 2*m = 883
//                maxd = (int)(sr*0.3 + 0.5 + modRange + 0.5) = 14114
//                N = 1<<k  mínimo con N >= maxd  ->  N = 16384, máscara 0x3FFF
//   escritura:   widx = (widx + 1) & mask ;  buf[widx] = x
//   lectura:     di = (int)d ; frac = d - di ; rp = widx - di
//                si d >= 1:  Hermite 4 puntos (Catmull-Rom) leyendo hacia atrás
//                    y1 = buf[rp], y2 = buf[rp+1], y0 = buf[rp-1], ym = buf[rp-2]
//                    A  = 2*y2 - 5*y1 + 4*y0 - ym
//                    B  =  3*y1 +   ym - 3*y0 - y2
//                    out = y1 + 0.5*t*((y0 - y2) + t*(A + t*B))     <- coefs -5, 4, 3, 0.5
//                si d <  1:  lineal de 2 puntos
//                    a1 = buf[rp], a0 = buf[rp-1], out = a1 + t*(a0 - a1)
//
// Nota sobre el eje: con d = m + t, la lectura interpola entre buf[rp] (retardo m) y
// buf[rp-1] (retardo m+1); t = 0 devuelve exactamente buf[rp].
//
#pragma once

#include "Math.h"

namespace rtxx {
namespace dsp {

class DelayLine {
public:
// Ring del motor a 44.1 kHz: 16384 muestras, máscara 0x3FFF.
    static constexpr int kSize = 16384;
    static constexpr int kMask = kSize - 1;  // 0x3FFF

    DelayLine() { clear(); }

    void clear() {
        for (int i = 0; i < kSize; ++i) buf_[i] = 0.0f;
        widx_ = 0;
    }

// Escritura por muestra, en el orden exacto del motor:
    // primero se avanza el puntero y después se guarda.
    inline void write(float x) {
        widx_ = (widx_ + 1) & kMask;
        buf_[widx_] = x;
    }

    inline float readAt(int delay) const {
        const int d = delay < 0 ? 0 : delay;
        return buf_[(widx_ - d) & kMask];
    }

    int writeIndex() const { return widx_; }

// Interpolación lineal de 2 puntos.
    float readLinear(float d) const {
        const float dd = d < 0.0f ? 0.0f : d;
        const int di = static_cast<int>(dd);
        const float t = dd - static_cast<float>(di);
        const float a1 = readAt(di);
        const float a0 = readAt(di + 1);
        return a1 + t * (a0 - a1);
    }

// Catmull-Rom de 4 puntos.
// Sólo válido para d >= 1 (como en El motor, donde el Hermite vive en la rama
    // d >= 1); para d < 1 usar readLinear() o readAuto().
    float readCatmullRom(float d) const {
        const float dd = d < 0.0f ? 0.0f : d;
        const int di = static_cast<int>(dd);
        const float t = dd - static_cast<float>(di);

        const float y1 = readAt(di);      // buf[rp]
        const float y2 = readAt(di - 1);  // buf[rp+1]
        const float y0 = readAt(di + 1);  // buf[rp-1]
        const float ym = readAt(di + 2);  // buf[rp-2]

        const float A = 2.0f * y2 - 5.0f * y1 + 4.0f * y0 - ym;
        const float B = 3.0f * y1 + ym - 3.0f * y0 - y2;
        return y1 + 0.5f * t * ((y0 - y2) + t * (A + t * B));
    }

// Regla del motor: Catmull-Rom si d >= 1 muestra, lineal si d < 1.
    float readAuto(float d) const {
        return d >= 1.0f ? readCatmullRom(d) : readLinear(d);
    }

private:
    float buf_[kSize] = {};
    int widx_ = 0;
};

}  // namespace dsp
}  // namespace rtxx
