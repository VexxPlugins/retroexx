#pragma once
// Texto del DSL "plugchain" del motor, generado por la herramienta de cadenas.
// por tools/gen_plugchain.py. El motor también parte de estas cadenas y las parsea
// en tiempo de creación del bloque, así que replicar el texto (y no una estructura ya
// interpretada) es lo que conserva la fidelidad de la cadena Dist y del master.
//
// Gramática (por línea, separada por tabuladores):
//   <kind> [enabled] <value>   <param> <nombre> [<min> <max> <escala> [<sim> <x>]]
// donde <kind> es hp6|lp6|hp12|lp12|hpShelf|lpShelf|bpPeak|bpNotch|distort|magicDistort|gain|...

namespace rtxx {

extern const char* const kPlugChainDist;          // cadena del módulo Dist (8 tipos)
extern const char* const kPlugChain2BandEQ;       // EQ de 2 bandas (comentada en el grafo, se conserva)
extern const char* const kPlugChainMasterFilter;  // EQ_Post: cortes duros y suaves
extern const char* const kPlugChainToneControl;   // EQ_Post_Tone: Tilt / Mid

} // namespace rtxx
