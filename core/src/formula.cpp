#include "mk/formula.hpp"

#include <cstdint>
#include <limits>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace mk {

class FormulaNode {
public:
    virtual ~FormulaNode() = default;

    virtual Rational evaluate(
        const std::map<std::string, Rational>& context) const = 0;

    virtual void collect_variables(
        std::set<std::string>& variables) const = 0;
};

namespace {

enum class TokenKind {
    end,
    number,
    identifier,
    plus,
    minus,
    bang,
    star,
    slash,
    percent,
    greater,
    less,
    greater_equal,
    less_equal,
    equal_equal,
    not_equal,
    ampersand,
    pipe,
    left_parenthesis,
    right_parenthesis,
    comma
};

struct Token {
    TokenKind kind;
    std::string text;
    std::size_t position;
};

[[noreturn]] void syntax_error(
    std::size_t position,
    const std::string& message) {
    throw CalcError(
        "syntax error at position " + std::to_string(position) + ": " + message);
}

bool is_digit(char character) {
    return character >= '0' && character <= '9';
}

bool is_identifier_start(char character) {
    return (character >= 'a' && character <= 'z') || character == '_';
}

bool is_identifier_character(char character) {
    return is_identifier_start(character) ||
           (character >= '0' && character <= '9');
}

class Lexer {
public:
    explicit Lexer(const std::string& source) : source_(source), position_(0) {}

    Token next() {
        skip_spaces();

        if (position_ == source_.size()) {
            return {TokenKind::end, {}, position_};
        }

        const std::size_t start = position_;
        const char character = source_[position_];

        if (is_digit(character)) {
            while (position_ < source_.size() &&
                   is_digit(source_[position_])) {
                ++position_;
            }

            if (position_ < source_.size() && source_[position_] == '.') {
                ++position_;
                const std::size_t fraction_start = position_;
                while (position_ < source_.size() &&
                       is_digit(source_[position_])) {
                    ++position_;
                }
                if (position_ == fraction_start) {
                    syntax_error(
                        position_,
                        "expected digits after decimal point");
                }
            }

            return {
                TokenKind::number,
                source_.substr(start, position_ - start),
                start};
        }

        if (is_identifier_start(character)) {
            ++position_;
            while (position_ < source_.size() &&
                   is_identifier_character(source_[position_])) {
                ++position_;
            }
            return {
                TokenKind::identifier,
                source_.substr(start, position_ - start),
                start};
        }

        ++position_;
        switch (character) {
        case '+':
            return {TokenKind::plus, {}, start};
        case '-':
            return {TokenKind::minus, {}, start};
        case '!':
            if (position_ < source_.size() && source_[position_] == '=') {
                ++position_;
                return {TokenKind::not_equal, {}, start};
            }
            return {TokenKind::bang, {}, start};
        case '*':
            return {TokenKind::star, {}, start};
        case '/':
            return {TokenKind::slash, {}, start};
        case '%':
            return {TokenKind::percent, {}, start};
        case '>':
            if (position_ < source_.size() && source_[position_] == '=') {
                ++position_;
                return {TokenKind::greater_equal, {}, start};
            }
            return {TokenKind::greater, {}, start};
        case '<':
            if (position_ < source_.size() && source_[position_] == '=') {
                ++position_;
                return {TokenKind::less_equal, {}, start};
            }
            return {TokenKind::less, {}, start};
        case '=':
            if (position_ < source_.size() && source_[position_] == '=') {
                ++position_;
                return {TokenKind::equal_equal, {}, start};
            }
            break;
        case '&':
            return {TokenKind::ampersand, {}, start};
        case '|':
            return {TokenKind::pipe, {}, start};
        case '(':
            return {TokenKind::left_parenthesis, {}, start};
        case ')':
            return {TokenKind::right_parenthesis, {}, start};
        case ',':
            return {TokenKind::comma, {}, start};
        default:
            break;
        }

        syntax_error(start, "unexpected character");
    }

private:
    void skip_spaces() {
        while (position_ < source_.size()) {
            const char character = source_[position_];
            if (character != ' ' && character != '\t' &&
                character != '\n' && character != '\r' &&
                character != '\f' && character != '\v') {
                break;
            }
            ++position_;
        }
    }

    const std::string& source_;
    std::size_t position_;
};

uint64_t parse_unsigned_digits(
    const std::string& text,
    std::size_t begin,
    std::size_t end) {
    uint64_t result = 0;
    const uint64_t limit =
        static_cast<uint64_t>(std::numeric_limits<int64_t>::max());

    for (std::size_t index = begin; index < end; ++index) {
        const uint64_t digit =
            static_cast<uint64_t>(text[index] - '0');
        if (result > (limit - digit) / 10U) {
            throw CalcError("literal overflow");
        }
        result = result * 10U + digit;
    }
    return result;
}

uint64_t checked_scale_multiply(uint64_t value, uint64_t factor) {
    const uint64_t limit =
        static_cast<uint64_t>(std::numeric_limits<int64_t>::max());
    if (value > limit / factor) {
        throw CalcError("literal overflow");
    }
    return value * factor;
}

Rational parse_number(const std::string& text) {
    const std::size_t dot = text.find('.');
    if (dot == std::string::npos) {
        return Rational(static_cast<int64_t>(
            parse_unsigned_digits(text, 0, text.size())));
    }

    std::string digits = text.substr(0, dot) + text.substr(dot + 1);
    std::size_t fraction_digits = text.size() - dot - 1;

    const std::size_t first_nonzero = digits.find_first_not_of('0');
    if (first_nonzero == std::string::npos) {
        return Rational{};
    }
    digits.erase(0, first_nonzero);

    while (fraction_digits > 0 && !digits.empty() &&
           digits.back() == '0') {
        digits.pop_back();
        --fraction_digits;
    }

    if (digits.empty()) {
        return Rational{};
    }

    uint64_t numerator = parse_unsigned_digits(0 + digits, 0, digits.size());

    while (fraction_digits > 0 && numerator % 2U == 0U) {
        numerator /= 2U;
        --fraction_digits;
    }

    std::size_t five_reductions = 0;
    while (five_reductions < fraction_digits && numerator % 5U == 0U) {
        numerator /= 5U;
        ++five_reductions;
    }
    fraction_digits -= five_reductions;

    uint64_t denominator = 1;
    for (std::size_t index = 0; index < fraction_digits; ++index) {
        denominator = checked_scale_multiply(denominator, 2U);
        denominator = checked_scale_multiply(denominator, 5U);
    }

    return Rational(
        static_cast<int64_t>(numerator),
        static_cast<int64_t>(denominator));
}

class LiteralNode final : public FormulaNode {
public:
    explicit LiteralNode(Rational value) : value_(value) {}

    Rational evaluate(
        const std::map<std::string, Rational>&) const override {
        return value_;
    }

    void collect_variables(std::set<std::string>&) const override {}

private:
    Rational value_;
};

class VariableNode final : public FormulaNode {
public:
    explicit VariableNode(std::string name) : name_(std::move(name)) {}

    Rational evaluate(
        const std::map<std::string, Rational>& context) const override {
        const auto found = context.find(name_);
        if (found == context.end()) {
            throw CalcError("unknown variable: " + name_);
        }
        return found->second;
    }

    void collect_variables(std::set<std::string>& variables) const override {
        variables.insert(name_);
    }

private:
    std::string name_;
};

enum class UnaryOperator {
    plus,
    minus,
    logical_not
};

class UnaryNode final : public FormulaNode {
public:
    UnaryNode(
        UnaryOperator operation,
        std::unique_ptr<FormulaNode> operand)
        : operation_(operation), operand_(std::move(operand)) {}

    Rational evaluate(
        const std::map<std::string, Rational>& context) const override {
        const Rational value = operand_->evaluate(context);
        switch (operation_) {
        case UnaryOperator::plus:
            return value;
        case UnaryOperator::minus:
            return -value;
        case UnaryOperator::logical_not:
            return value == Rational{} ? Rational{1} : Rational{};
        }
        throw CalcError("invalid unary operator");
    }

    void collect_variables(std::set<std::string>& variables) const override {
        operand_->collect_variables(variables);
    }

private:
    UnaryOperator operation_;
    std::unique_ptr<FormulaNode> operand_;
};

enum class BinaryOperator {
    multiply,
    divide,
    modulo,
    add,
    subtract,
    greater,
    less,
    greater_equal,
    less_equal,
    equal,
    not_equal,
    logical_and,
    logical_or
};

class BinaryNode final : public FormulaNode {
public:
    BinaryNode(
        BinaryOperator operation,
        std::unique_ptr<FormulaNode> left,
        std::unique_ptr<FormulaNode> right)
        : operation_(operation),
          left_(std::move(left)),
          right_(std::move(right)) {}

    Rational evaluate(
        const std::map<std::string, Rational>& context) const override {
        const Rational left = left_->evaluate(context);

        if (operation_ == BinaryOperator::logical_and &&
            left == Rational{}) {
            return Rational{};
        }
        if (operation_ == BinaryOperator::logical_or &&
            left != Rational{}) {
            return Rational{1};
        }

        const Rational right = right_->evaluate(context);

        switch (operation_) {
        case BinaryOperator::multiply:
            return left * right;
        case BinaryOperator::divide:
            return left / right;
        case BinaryOperator::modulo:
            if (left.denominator() != 1 || right.denominator() != 1) {
                throw CalcError("modulo requires integer operands");
            }
            if (right.numerator() == 0) {
                throw CalcError("division by zero");
            }
            if (left.numerator() == std::numeric_limits<int64_t>::min() &&
                right.numerator() == -1) {
                return Rational{};
            }
            return Rational(left.numerator() % right.numerator());
        case BinaryOperator::add:
            return left + right;
        case BinaryOperator::subtract:
            return left - right;
        case BinaryOperator::greater:
            return left > right ? Rational{1} : Rational{};
        case BinaryOperator::less:
            return left < right ? Rational{1} : Rational{};
        case BinaryOperator::greater_equal:
            return left >= right ? Rational{1} : Rational{};
        case BinaryOperator::less_equal:
            return left <= right ? Rational{1} : Rational{};
        case BinaryOperator::equal:
            return left == right ? Rational{1} : Rational{};
        case BinaryOperator::not_equal:
            return left != right ? Rational{1} : Rational{};
        case BinaryOperator::logical_and:
            return right == Rational{} ? Rational{} : Rational{1};
        case BinaryOperator::logical_or:
            return right == Rational{} ? Rational{} : Rational{1};
        }
        throw CalcError("invalid binary operator");
    }

    void collect_variables(std::set<std::string>& variables) const override {
        left_->collect_variables(variables);
        right_->collect_variables(variables);
    }

private:
    BinaryOperator operation_;
    std::unique_ptr<FormulaNode> left_;
    std::unique_ptr<FormulaNode> right_;
};

class CallNode final : public FormulaNode {
public:
    CallNode(
        std::string name,
        std::vector<std::unique_ptr<FormulaNode>> arguments)
        : name_(std::move(name)),
          arguments_(std::move(arguments)) {}

    Rational evaluate(
        const std::map<std::string, Rational>& context) const override {
        if (name_ == "if") {
            const Rational condition = arguments_[0]->evaluate(context);
            return condition != Rational{}
                ? arguments_[1]->evaluate(context)
                : arguments_[2]->evaluate(context);
        }

        if (name_ == "min") {
            const Rational first = arguments_[0]->evaluate(context);
            const Rational second = arguments_[1]->evaluate(context);
            return first < second ? first : second;
        }
        if (name_ == "max") {
            const Rational first = arguments_[0]->evaluate(context);
            const Rational second = arguments_[1]->evaluate(context);
            return first > second ? first : second;
        }
        if (name_ == "abs") {
            const Rational value = arguments_[0]->evaluate(context);
            return value < Rational{} ? -value : value;
        }
        if (name_ == "round") {
            return Rational(
                arguments_[0]->evaluate(context).round_half_up_to_int64());
        }

        throw CalcError("unknown function: " + name_);
    }

    void collect_variables(std::set<std::string>& variables) const override {
        for (const auto& argument : arguments_) {
            argument->collect_variables(variables);
        }
    }

private:
    std::string name_;
    std::vector<std::unique_ptr<FormulaNode>> arguments_;
};

class Parser {
public:
    explicit Parser(const std::string& source) : lexer_(source) {
        current_ = lexer_.next();
    }

    std::unique_ptr<FormulaNode> parse() {
        if (current_.kind == TokenKind::end) {
            syntax_error(current_.position, "expected expression");
        }

        std::unique_ptr<FormulaNode> result = parse_or();
        if (current_.kind != TokenKind::end) {
            syntax_error(current_.position, "unexpected token");
        }
        return result;
    }

private:
    void consume(TokenKind kind) {
        if (current_.kind != kind) {
            syntax_error(current_.position, "unexpected token");
        }
        current_ = lexer_.next();
    }

    std::unique_ptr<FormulaNode> parse_or() {
        auto result = parse_and();
        while (current_.kind == TokenKind::pipe) {
            consume(TokenKind::pipe);
            result = std::make_unique<BinaryNode>(
                BinaryOperator::logical_or,
                std::move(result),
                parse_and());
        }
        return result;
    }

    std::unique_ptr<FormulaNode> parse_and() {
        auto result = parse_comparison();
        while (current_.kind == TokenKind::ampersand) {
            consume(TokenKind::ampersand);
            result = std::make_unique<BinaryNode>(
                BinaryOperator::logical_and,
                std::move(result),
                parse_comparison());
        }
        return result;
    }

    std::unique_ptr<FormulaNode> parse_comparison() {
        auto result = parse_additive();
        for (;;) {
            BinaryOperator operation;
            switch (current_.kind) {
            case TokenKind::greater:
                operation = BinaryOperator::greater;
                break;
            case TokenKind::less:
                operation = BinaryOperator::less;
                break;
            case TokenKind::greater_equal:
                operation = BinaryOperator::greater_equal;
                break;
            case TokenKind::less_equal:
                operation = BinaryOperator::less_equal;
                break;
            case TokenKind::equal_equal:
                operation = BinaryOperator::equal;
                break;
            case TokenKind::not_equal:
                operation = BinaryOperator::not_equal;
                break;
            default:
                return result;
            }
            consume(current_.kind);
            result = std::make_unique<BinaryNode>(
                operation,
                std::move(result),
                parse_additive());
        }
    }

    std::unique_ptr<FormulaNode> parse_additive() {
        auto result = parse_multiplicative();
        while (current_.kind == TokenKind::plus ||
               current_.kind == TokenKind::minus) {
            const TokenKind kind = current_.kind;
            consume(kind);
            result = std::make_unique<BinaryNode>(
                kind == TokenKind::plus
                    ? BinaryOperator::add
                    : BinaryOperator::subtract,
                std::move(result),
                parse_multiplicative());
        }
        return result;
    }

    std::unique_ptr<FormulaNode> parse_multiplicative() {
        auto result = parse_unary();
        while (current_.kind == TokenKind::star ||
               current_.kind == TokenKind::slash ||
               current_.kind == TokenKind::percent) {
            const TokenKind kind = current_.kind;
            consume(kind);
            BinaryOperator operation = BinaryOperator::multiply;
            if (kind == TokenKind::slash) {
                operation = BinaryOperator::divide;
            } else if (kind == TokenKind::percent) {
                operation = BinaryOperator::modulo;
            }
            result = std::make_unique<BinaryNode>(
                operation,
                std::move(result),
                parse_unary());
        }
        return result;
    }

    std::unique_ptr<FormulaNode> parse_unary() {
        if (current_.kind == TokenKind::plus ||
            current_.kind == TokenKind::minus ||
            current_.kind == TokenKind::bang) {
            const TokenKind kind = current_.kind;
            consume(kind);
            return std::make_unique<UnaryNode>(
                kind == TokenKind::plus
                    ? UnaryOperator::plus
                    : kind == TokenKind::minus
                        ? UnaryOperator::minus
                        : UnaryOperator::logical_not,
                parse_unary());
        }
        return parse_primary();
    }

    std::unique_ptr<FormulaNode> parse_primary() {
        if (current_.kind == TokenKind::number) {
            const std::string text = current_.text;
            consume(TokenKind::number);
            return std::make_unique<LiteralNode>(parse_number(text));
        }

        if (current_.kind == TokenKind::identifier) {
            const std::string name = current_.text;
            const std::size_t name_position = current_.position;
            consume(TokenKind::identifier);

            if (current_.kind != TokenKind::left_parenthesis) {
                return std::make_unique<VariableNode>(name);
            }

            consume(TokenKind::left_parenthesis);
            std::vector<std::unique_ptr<FormulaNode>> arguments;
            if (current_.kind != TokenKind::right_parenthesis) {
                for (;;) {
                    arguments.push_back(parse_or());
                    if (current_.kind != TokenKind::comma) {
                        break;
                    }
                    consume(TokenKind::comma);
                }
            }

            if (current_.kind != TokenKind::right_parenthesis) {
                syntax_error(current_.position, "expected ')'");
            }
            consume(TokenKind::right_parenthesis);

            const std::size_t expected =
                name == "min" || name == "max" ? 2 :
                name == "abs" || name == "round" ? 1 :
                name == "if" ? 3 : 0;

            if (expected == 0) {
                syntax_error(name_position, "unknown function: " + name);
            }
            if (arguments.size() != expected) {
                syntax_error(
                    name_position,
                    "invalid argument count for function: " + name);
            }

            return std::make_unique<CallNode>(
                name,
                std::move(arguments));
        }

        if (current_.kind == TokenKind::left_parenthesis) {
            consume(TokenKind::left_parenthesis);
            auto result = parse_or();
            if (current_.kind != TokenKind::right_parenthesis) {
                syntax_error(current_.position, "expected ')'");
            }
            consume(TokenKind::right_parenthesis);
            return result;
        }

        syntax_error(current_.position, "expected expression");
    }

    Lexer lexer_;
    Token current_{};
};

}

Formula::Formula() = default;

Formula::Formula(std::unique_ptr<FormulaNode> root)
    : root_(std::move(root)) {}

Formula::~Formula() = default;

Formula::Formula(Formula&&) noexcept = default;

Formula& Formula::operator=(Formula&&) noexcept = default;

Formula parse(const std::string& source) {
    return Formula(Parser(source).parse());
}

Rational evaluate(
    const Formula& formula,
    const std::map<std::string, Rational>& context) {
    if (!formula.root_) {
        throw CalcError("empty formula");
    }
    return formula.root_->evaluate(context);
}

std::vector<std::string> extract_variables(const Formula& formula) {
    if (!formula.root_) {
        throw CalcError("empty formula");
    }

    std::set<std::string> names;
    formula.root_->collect_variables(names);
    return std::vector<std::string>(names.begin(), names.end());
}

}
