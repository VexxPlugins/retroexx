#include "PlugChain.h"

#include <cctype>
#include <set>
#include <cstdlib>
#include <sstream>

namespace rtxx {

namespace {

bool isNumberToken(const std::string& t) {
    if (t.empty()) return false;
    size_t i = 0;
    if (t[i] == '+' || t[i] == '-') ++i;
    bool digit = false, dot = false;
    for (; i < t.size(); ++i) {
        const char c = t[i];
        if (std::isdigit(static_cast<unsigned char>(c))) { digit = true; continue; }
        if (c == '.' && !dot) { dot = true; continue; }
        if (c == 'e' || c == 'E') return false;  // no aparece en el DSL; si aparece, que se note
        return false;
    }
    return digit;
}

bool isIdentifierToken(const std::string& t) {
    if (t.empty()) return false;
    if (!(std::isalpha(static_cast<unsigned char>(t[0])) || t[0] == '_')) return false;
    for (char c : t)
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_')) return false;
    return true;
}

std::vector<std::string> splitTabs(const std::string& line) {
    std::vector<std::string> cols;
    std::string cur;
    for (char c : line) {
        if (c == '\t') { cols.push_back(cur); cur.clear(); }
        else cur.push_back(c);
    }
    cols.push_back(cur);
    // Quita espacios/CR de cada columna, pero NO tabs internos (no los hay).
    for (auto& c : cols) {
        while (!c.empty() && (c.back() == ' ' || c.back() == '\r')) c.pop_back();
        size_t i = 0;
        while (i < c.size() && c[i] == ' ') ++i;
        if (i) c.erase(0, i);
    }
    return cols;
}

} // namespace

bool Arg::isKeyword(const char* kw) const {
    return kind == ArgKind::Keyword && text == kw;
}

const ParamLine* Node::find(const std::string& name) const {
    for (const auto& p : params)
        if (p.name == name) return &p;
    return nullptr;
}

std::vector<const ParamLine*> Node::conditions() const {
    std::vector<const ParamLine*> out;
    for (const auto& p : params)
        if (p.isCondition()) out.push_back(&p);
    return out;
}

Arg classifyArg(const std::string& token) {
    Arg a;
    a.text = token;
    if (isNumberToken(token)) {
        a.kind = ArgKind::Number;
        a.number = std::strtod(token.c_str(), nullptr);
        return a;
    }
    if (token.size() >= 2 && token.front() == '(' && token.back() == ')') {
        a.kind = ArgKind::ScopedParam;
        a.text = token.substr(1, token.size() - 2);
        return a;
    }
    if (isIdentifierToken(token)) {
    // Palabras de escala del DSL. En las cuatro cadenas del motor no colisionan con
        // ningún nombre de parámetro (los parámetros son freq/q/db/gain/amount/shape/lowFreq/
        // highFreq/refLevel/distDB/distLP/Type/Enable/legacyReset/[enabled]).
        static const std::set<std::string> kScaleWords = {"log", "lin", "sqr", "sym", "logcut"};
        if (kScaleWords.count(token) != 0) {
            a.kind = ArgKind::Keyword;
            return a;
        }
        a.kind = ArgKind::Param;
        return a;
    }
    // Palabras de escala y cualquier otro token alfabético se tratan como keyword.
    a.kind = ArgKind::Keyword;
    return a;
}

bool parsePlugChain(const std::string& text, Chain& out, std::string& error) {
    out = Chain{};
    std::istringstream in(text);
    std::string raw;
    int lineNo = 0;
    Node* current = nullptr;

    while (std::getline(in, raw)) {
        ++lineNo;
        out.lineCount = lineNo;
        std::string line = raw;
        if (!line.empty() && line.back() == '\r') line.pop_back();

        // Líneas vacías y comentarios (';') se ignoran.
        bool blank = true;
        for (char c : line)
            if (c != ' ' && c != '\t') { blank = false; break; }
        if (blank) continue;
        if (line[0] == ';') { ++out.commentCount; continue; }

        const auto cols = splitTabs(line);

        // Cabecera "Plug Param Value Min Max Scale": no es una etapa.
        if (cols.size() >= 2 && cols[0] == "Plug" && cols[1] == "Param") continue;

        if (cols[0] == "-") {
            if (!current) {
                error = "línea " + std::to_string(lineNo) + ": continuación '-' sin etapa abierta";
                return false;
            }
            if (cols.size() < 2 || cols[1].empty()) {
                error = "línea " + std::to_string(lineNo) + ": falta el nombre del parámetro";
                return false;
            }
            ParamLine p;
            p.name = cols[1];
            p.line = lineNo;
            for (size_t i = 2; i < cols.size(); ++i) {
                if (cols[i].empty()) continue;
                p.args.push_back(classifyArg(cols[i]));
            }
            current->params.push_back(std::move(p));
            continue;
        }

        // Nueva etapa: cols[0] = tipo, cols[1] = primer parámetro.
        if (cols[0].empty()) {
            error = "línea " + std::to_string(lineNo) + ": falta el tipo de etapa";
            return false;
        }
        if (cols.size() < 2 || cols[1].empty()) {
            error = "línea " + std::to_string(lineNo) + ": la etapa '" + cols[0] +
                    "' no declara ningún parámetro";
            return false;
        }
        Node n;
        n.kind = cols[0];
        n.line = lineNo;
        ParamLine p;
        p.name = cols[1];
        p.line = lineNo;
        for (size_t i = 2; i < cols.size(); ++i) {
            if (cols[i].empty()) continue;
            p.args.push_back(classifyArg(cols[i]));
        }
        n.params.push_back(std::move(p));
        out.nodes.push_back(std::move(n));
        current = &out.nodes.back();
    }

    if (out.nodes.empty()) {
        error = "el texto no contiene ninguna etapa";
        return false;
    }
    return true;
}

} // namespace rtxx
