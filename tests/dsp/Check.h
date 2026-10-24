// Comprobaciones mínimas para los tests de src/dsp.
// No es un framework: 4 macros y un contador. Cada test es un ejecutable propio.
#pragma once

#include <cmath>
#include <cstdio>

namespace check {

inline int& failures() {
    static int f = 0;
    return f;
}

inline int finish(const char* name) {
    if (failures() == 0) {
        std::printf("PASS %s\n", name);
        return 0;
    }
    std::printf("FAIL %s (%d comprobaciones)\n", name, failures());
    return 1;
}

}  // namespace check

#define CHECK(cond)                                                             \
    do {                                                                        \
        if (!(cond)) {                                                          \
            std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);       \
            ++check::failures();                                                \
        }                                                                       \
    } while (0)

#define CHECK_NEAR(a, b, tol)                                                             \
    do {                                                                                  \
        const double _ca = static_cast<double>(a);                                        \
        const double _cb = static_cast<double>(b);                                        \
        const double _ct = static_cast<double>(tol);                                      \
        if (!(std::fabs(_ca - _cb) <= _ct)) {                                             \
            std::printf("  FAIL %s:%d  %s=%.9g  %s=%.9g  |d|=%.3g > %.3g\n", __FILE__,    \
                        __LINE__, #a, _ca, #b, _cb, std::fabs(_ca - _cb), _ct);           \
            ++check::failures();                                                          \
        }                                                                                 \
    } while (0)

#define CHECK_LT(a, b)                                                      \
    do {                                                                    \
        const double _ca = static_cast<double>(a);                          \
        const double _cb = static_cast<double>(b);                          \
        if (!(_ca < _cb)) {                                                 \
            std::printf("  FAIL %s:%d  %s=%.9g  no es < %s=%.9g\n", __FILE__, \
                        __LINE__, #a, _ca, #b, _cb);                        \
            ++check::failures();                                            \
        }                                                                   \
    } while (0)
