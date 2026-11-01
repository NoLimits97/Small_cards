#ifndef MK_FORMULA_HPP
#define MK_FORMULA_HPP

#include "mk/types.hpp"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace mk {

/*
 * Ошибки синтаксиса содержат позицию байта с нулевым индексом в исходной
 * строке. Ошибки вычисления используют CalcError без обязательной позиции.
 */
class FormulaNode;

class Formula {
public:
    Formula();
    ~Formula();

    Formula(Formula&&) noexcept;
    Formula& operator=(Formula&&) noexcept;

    Formula(const Formula&) = delete;
    Formula& operator=(const Formula&) = delete;

private:
    explicit Formula(std::unique_ptr<FormulaNode> root);

    std::unique_ptr<FormulaNode> root_;

    friend Formula parse(const std::string& source);
    friend Rational evaluate(
        const Formula& formula,
        const std::map<std::string, Rational>& context);
    friend std::vector<std::string> extract_variables(const Formula& formula);
};

Formula parse(const std::string& source);

Rational evaluate(
    const Formula& formula,
    const std::map<std::string, Rational>& context);

std::vector<std::string> extract_variables(const Formula& formula);

}

#endif
