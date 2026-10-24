#include "Condition.h"

#include <cctype>
#include <stdexcept>

namespace rtxx {

struct Condition::Node {
    enum class Kind { Constant, Param, Not, Mul, Add } kind = Kind::Constant;
    float value = 0.0f;
    std::string name;
    std::shared_ptr<const Node> a, b;
};

namespace {

class Parser {
public:
    Parser(const std::string& s, std::vector<std::string>& ids) : s_(s), ids_(ids) {}

    std::shared_ptr<const Condition::Node> parse() {
        auto n = parseAdd();
        skip();
        if (pos_ != s_.size()) fail("resto inesperado: '" + s_.substr(pos_) + "'");
        return n;
    }

    const std::string& error() const { return error_; }
    bool ok() const { return error_.empty(); }

private:
    using Node = Condition::Node;
    using Ptr = std::shared_ptr<const Node>;

    void skip() {
        while (pos_ < s_.size() && (s_[pos_] == ' ' || s_[pos_] == '\t')) ++pos_;
    }
    void fail(const std::string& m) {
        if (error_.empty()) error_ = m;
    }
    bool accept(char c) {
        skip();
        if (pos_ < s_.size() && s_[pos_] == c) { ++pos_; return true; }
        return false;
    }

    Ptr parseAdd() {
        auto lhs = parseMul();
        while (lhs && accept('+')) {
            auto rhs = parseMul();
            if (!rhs) return nullptr;
            auto n = std::make_shared<Node>();
            n->kind = Node::Kind::Add;
            n->a = lhs;
            n->b = rhs;
            lhs = n;
        }
        return lhs;
    }

    Ptr parseMul() {
        auto lhs = parseUnary();
        while (lhs && accept('*')) {
            auto rhs = parseUnary();
            if (!rhs) return nullptr;
            auto n = std::make_shared<Node>();
            n->kind = Node::Kind::Mul;
            n->a = lhs;
            n->b = rhs;
            lhs = n;
        }
        return lhs;
    }

    Ptr parseUnary() {
        skip();
        if (pos_ < s_.size() && s_[pos_] == '!') {
            ++pos_;
            auto inner = parseUnary();
            if (!inner) return nullptr;
            auto n = std::make_shared<Node>();
            n->kind = Node::Kind::Not;
            n->a = inner;
            return n;
        }
        return parsePrimary();
    }

    Ptr parsePrimary() {
        skip();
        if (accept('(')) {
            auto inner = parseAdd();
            if (!inner) return nullptr;
            if (!accept(')')) { fail("falta ')'"); return nullptr; }
            return inner;
        }
        skip();
        if (pos_ < s_.size() && (std::isdigit(static_cast<unsigned char>(s_[pos_])) || s_[pos_] == '.')) {
            size_t start = pos_;
            while (pos_ < s_.size() &&
                   (std::isdigit(static_cast<unsigned char>(s_[pos_])) || s_[pos_] == '.')) ++pos_;
            auto n = std::make_shared<Node>();
            n->kind = Node::Kind::Constant;
            n->value = std::stof(s_.substr(start, pos_ - start));
            return n;
        }
        if (pos_ < s_.size() &&
            (std::isalpha(static_cast<unsigned char>(s_[pos_])) || s_[pos_] == '_')) {
            size_t start = pos_;
            while (pos_ < s_.size() &&
                   (std::isalnum(static_cast<unsigned char>(s_[pos_])) || s_[pos_] == '_' ||
                    s_[pos_] == '.')) ++pos_;
            auto n = std::make_shared<Node>();
            n->kind = Node::Kind::Param;
            n->name = s_.substr(start, pos_ - start);
            ids_.push_back(n->name);
            return n;
        }
        fail("token inesperado en '" + s_.substr(pos_) + "'");
        return nullptr;
    }

    const std::string& s_;
    std::vector<std::string>& ids_;
    size_t pos_ = 0;
    std::string error_;
};

float evalNode(const Condition::Node& n, const ParamSource& p) {
    switch (n.kind) {
        case Condition::Node::Kind::Constant: return n.value;
        case Condition::Node::Kind::Param:    return p.get(n.name.c_str());
        case Condition::Node::Kind::Not:      return Condition::applyNot(evalNode(*n.a, p));
        case Condition::Node::Kind::Mul:      return evalNode(*n.a, p) * evalNode(*n.b, p);
        case Condition::Node::Kind::Add:      return evalNode(*n.a, p) + evalNode(*n.b, p);
    }
    return 0.0f;
}

} // namespace

Condition::Condition(std::string expr) {
    if (expr.empty()) {
        auto n = std::make_shared<Node>();
        n->kind = Node::Kind::Constant;
        n->value = 1.0f;
        root_ = n;
        return;
    }
    Parser p(expr, identifiers_);
    root_ = p.parse();
    if (!p.ok()) {
        error_ = p.error();
        root_ = nullptr;
    }
}

float Condition::evaluate(const ParamSource& params) const {
    if (!root_) throw std::runtime_error("Condition inválida: " + error_);
    return evalNode(*root_, params);
}

} // namespace rtxx
