#!/bin/sh
# Compila y ejecuta los tests de src/dsp.
#
#   clang++ -std=c++17 -O2 -Wall -Wextra -I src
#
# -Werror: no se ignora ningún warning (requisito del frente).
# Cada test es un ejecutable independiente; el script falla con código != 0 si
# alguno falla o si aparece un warning.
set -eu

HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)
OUT="$HERE/build"   # artefactos dentro de src/dsp (patrón 'build/' del .gitignore raíz)
CXX=${CXX:-clang++}
CXXFLAGS="-std=c++17 -O2 -Wall -Wextra -Werror -I $ROOT/src"

TESTS="test_math test_biquad test_delayline test_lfo test_smoother test_oversampling"

mkdir -p "$OUT"

echo "== compilando (src/dsp) =="
for t in $TESTS; do
    printf '  %-20s' "$t"
    # shellcheck disable=SC2086
    $CXX $CXXFLAGS "$ROOT/tests/dsp/$t.cpp" -o "$OUT/$t"
    echo "ok"
done

echo "== ejecutando =="
fail=0
for t in $TESTS; do
    if "$OUT/$t"; then
        :
    else
        echo "  FALLO: $t"
        fail=1
    fi
done

if [ "$fail" -ne 0 ]; then
    echo "== RESULTADO: fallo =="
    exit 1
fi
echo "== RESULTADO: todos los tests de dsp pasan =="
exit 0
