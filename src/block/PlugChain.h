#pragma once
#include <string>
#include <vector>

namespace rtxx {

// Intérprete del DSL "plugchain" con el que El motor describe las cadenas de etapas
// (módulo Dist, EQ_Post, EQ_Post_Tone, EQ de 2 bandas).
//
// El formato es tabulado y se escribe a mano en el Lua. Cabecera de 6 columnas:
//
//   Plug  Param  Value  Min  Max  Scale
//
// Cada línea tabulada es:  <plug> \t <param> \t <arg0> \t <arg1> \t ...
//   - La primera línea de un nodo trae el tipo de etapa en la columna "Plug" y, en la misma
//     línea, su primer parámetro. Las siguientes líneas llevan "-" en "Plug" y continúan el nodo.
//   - Una línea con el parámetro "[enabled]" es una CONDICIÓN de activación del nodo, no un
//     parámetro de audio. Su forma es  [enabled] <param> [<lo> [<hi>]]:
//       * sin números        -> el nodo se activa si <param> != 0
//       * un número          -> se activa si <param> == lo
//       * dos números        -> se activa si lo <= <param> <= hi
//     Esto es lo que permite que UNA cadena sirva para los 8 tipos de distorsión.
//   - Los argumentos de un parámetro pueden ser números ("-12", "0.4", "20000"), referencias a
//     parámetro del modelo ("Amount", "FreqLo", "Type"), referencias cualificadas entre paréntesis
//     ("(Amount)") o palabras de escala ("lin", "log", "sqr", "sym", "logcut").
//
// IMPORTANTE: el parser NO adivina la semántica del escalado. Preserva los argumentos tal cual y
// los clasifica; la interpretación de cada etapa vive en su evaluador y sólo se implementa cuando
// se puede validar por medición. Lo que no esté resuelto se marca explícitamente.

enum class ArgKind {
    Number,       // literal numérico
    Param,        // identificador de parámetro del modelo
    ScopedParam,  // identificador entre paréntesis, "(Amount)" — semántica pendiente de medir
    Keyword       // palabra de escala: lin, log, sqr, sym, logcut, ...
};

struct Arg {
    ArgKind kind = ArgKind::Number;
    double number = 0.0;
    std::string text;   // tal cual aparece, sin paréntesis
    bool isKeyword(const char* kw) const;
};

struct ParamLine {
    std::string name;            // "db", "freq", "[enabled]", ...
    std::vector<Arg> args;
    int line = 0;
    bool isCondition() const { return name == "[enabled]"; }
};

struct Node {
    std::string kind;            // "gain", "bpPeak", "hp12", "lp12", "hpShelf", "distort", ...
    std::vector<ParamLine> params;
    int line = 0;

    const ParamLine* find(const std::string& name) const;
    // Condiciones [enabled] del nodo.
    std::vector<const ParamLine*> conditions() const;
};

struct Chain {
    std::vector<Node> nodes;
    int lineCount = 0;
    int commentCount = 0;
};

// Devuelve false y describe el problema en `error` si el texto no respeta el formato.
bool parsePlugChain(const std::string& text, Chain& out, std::string& error);

// Clasifica un token de argumento según su forma léxica (sin interpretar semántica).
Arg classifyArg(const std::string& token);

} // namespace rtxx
