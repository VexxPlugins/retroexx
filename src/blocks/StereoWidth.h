#pragma once
// cStereoWidth (nodo `Width` del grafo): matriz M/S con la ley ajustado por medida.
//
// Ley: con v = Width/200 normalizado,
//   v <= 0.5 :  S = v,            W = 0.5                      (estrechar: lineal)
//   v >  0.5 :  S = 0.5*4^(v-0.5), W = 0.25/S                   (ensanchar: W*S = 1/4)
//   outL = W*(L+R) - S*(R-L)
//   outR = W*(L+R) + S*(R-L)
// Con v = 0.5 la salida es la identidad exacta (W = S = 0.5), que es el default de
// fábrica del motor. En v = 0 queda mono (S = 0).
//
// slot 19 de la vitrina ) tiene exactamente esta estructura
// `mid = (W+frac)*(L+R)`, `side = (sign+frac)*(R-L)`, `outL = mid - side`,
// `outR = mid + side`; el valor sale del proxy normalizado *16384 con rampa de punto
// fijo. La ley de W/S no es legible en el vectorizado, así que se midió.
#include "block/Block.h"
#include "model/ParamStore.h"

namespace rtxx {

class StereoWidthBlock : public iBlock {
public:
    explicit StereoWidthBlock(const ParamStore* params) : params_(params) {}

    void prepare(const PrepareContext& ctx) override;
    void reset() override;
    void process(const PortData* in, PortData& out) override;

    // Expuestos para los tests de la ley.
    static float sideFor(float v);
    static float midFor(float v);

private:
    const ParamStore* params_ = nullptr;
    double sampleRate_ = 44100.0;
    float coef_ = 1.0f;   // rampa de un polo (20 Hz, igual que la de cGain)
    float side_ = 0.5f;
    float mid_ = 0.5f;
    bool primed_ = false;
};

} // namespace rtxx
