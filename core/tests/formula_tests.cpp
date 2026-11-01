#include "test_framework.hpp"

#include "mk/formula.hpp"

#include <cstdint>
#include <limits>
#include <map>
#include <string>
#include <vector>

using mk::Formula;
using mk::Rational;

namespace {

Rational evaluate_formula(
    const std::string& source,
    const std::map<std::string, Rational>& context) {
    return mk::evaluate(mk::parse(source), context);
}

Rational evaluate_formula(const std::string& source) {
    return evaluate_formula(
        source,
        std::map<std::string, Rational>{});
}

template <typename Function>
void require_calc_error(Function&& function) {
    bool caught_calc_error = false;

    try {
        function();
    } catch (const mk::CalcError&) {
        caught_calc_error = true;
    } catch (...) {
        throw test_framework::Failure(
            "expected CalcError, got another exception");
    }

    if (!caught_calc_error) {
        throw test_framework::Failure("expected CalcError");
    }
}

template <typename Function>
void require_position(
    Function&& function,
    const std::string& position) {
    try {
        function();
        throw test_framework::Failure("expected CalcError");
    } catch (const mk::CalcError& error) {
        REQUIRE(
            std::string(error.what()).find(position) !=
            std::string::npos);
    }
}

}

TEST(formula_arithmetic_and_precedence) {
    REQUIRE_EQUAL(evaluate_formula("1 + 2 * 3"), Rational{7});
    REQUIRE_EQUAL(evaluate_formula("-2 * 3"), Rational{-6});
    REQUIRE_EQUAL(evaluate_formula("+2"), Rational{2});
    REQUIRE_EQUAL(evaluate_formula("10 - 3 - 2"), Rational{5});
    REQUIRE_EQUAL(evaluate_formula("(1 + 2) * 3"), Rational{9});
    REQUIRE_EQUAL(evaluate_formula("20 / 5 % 3"), Rational{1});
    REQUIRE_EQUAL(evaluate_formula("1 | 0 & 0"), Rational{1});
    REQUIRE_EQUAL(evaluate_formula("2 > 1 & 0"), Rational{});
}

TEST(formula_unary_logic) {
    REQUIRE_EQUAL(evaluate_formula("!0"), Rational{1});
    REQUIRE_EQUAL(evaluate_formula("!2"), Rational{});
    REQUIRE_EQUAL(evaluate_formula("!-2"), Rational{});
}

TEST(formula_decimal_exactness) {
    REQUIRE_EQUAL(
        evaluate_formula("0.1 + 0.2"),
        (Rational{3, 10}));

    REQUIRE_EQUAL(
        evaluate_formula("0.0000000000000000002"),
        (Rational{1, 5000000000000000000LL}));

    REQUIRE_EQUAL(
        evaluate_formula(
            "base_amount * 15 / 17",
            {{"base_amount", Rational{5000000}}}),
        (Rational{75000000, 17}));
}

TEST(formula_operators_and_functions) {
    REQUIRE_EQUAL(
        evaluate_formula("6 / 4"),
        (Rational{3, 2}));
    REQUIRE_EQUAL(evaluate_formula("7 % 3"), Rational{1});
    REQUIRE_EQUAL(evaluate_formula("-7 % 3"), Rational{-1});
    REQUIRE_EQUAL(evaluate_formula("2 > 1"), Rational{1});
    REQUIRE_EQUAL(evaluate_formula("2 < 1"), Rational{});
    REQUIRE_EQUAL(evaluate_formula("2 >= 2"), Rational{1});
    REQUIRE_EQUAL(evaluate_formula("2 <= 1"), Rational{});
    REQUIRE_EQUAL(evaluate_formula("2 == 2"), Rational{1});
    REQUIRE_EQUAL(evaluate_formula("2 != 2"), Rational{});
    REQUIRE_EQUAL(evaluate_formula("1 & 2"), Rational{1});
    REQUIRE_EQUAL(evaluate_formula("0 | 2"), Rational{1});
    REQUIRE_EQUAL(evaluate_formula("min(4, 2)"), Rational{2});
    REQUIRE_EQUAL(evaluate_formula("max(4, 2)"), Rational{4});
    REQUIRE_EQUAL(evaluate_formula("abs(-5)"), Rational{5});
    REQUIRE_EQUAL(evaluate_formula("round(1.5"), Rational{2});
    REQUIRE_EQUAL(evaluate_formula("round(-1.5"), Rational{-2});
}

TEST(formula_modulo_validation) {
    REQUIRE_EQUAL(evaluate_formula("-7 % 3"), Rational{-1});
    REQUIRE_EQUAL(evaluate_formula("7 % -3"), Rational{1});

    require_calc_error(
        [] { (void)evaluate_formula("1.5 % 1"); });
    require_calc_error(
        [] { (void)evaluate_formula("1 % 1.5"); });
    require_calc_error(
        [] { (void)evaluate_formula("1 % 0"); });
    require_calc_error(
        [] { (void)evaluate_formula("1 / 0"); });

    REQUIRE_EQUAL(
        evaluate_formula(
            "minimum % -1",
            {{"minimum", Rational{
                std::numeric_limits<int64_t>::min()}}}),
        Rational{});
}

TEST(formula_short_circuit_and_if) {
    REQUIRE_EQUAL(
        evaluate_formula("if(1, 42, 1 / 0)"),
        Rational{42});
    REQUIRE_EQUAL(
        evaluate_formula("if(0, 1 / 0, 42)"),
        Rational{42});
    REQUIRE_EQUAL(
        evaluate_formula("0 & (1 / 0)"),
        Rational{});
    REQUIRE_EQUAL(
        evaluate_formula("1 | (1 / 0)"),
        Rational{1});

    require_calc_error(
        [] { (void)evaluate_formula("0 | unknown"); });
    require_calc_error(
        [] { (void)evaluate_formula("1 & unknown"); });
}

TEST(formula_variables) {
    const Formula formula = mk::parse(
        "if(flag, first_value, flag) | "
        "(other_value & hidden)");

    REQUIRE_EQUAL(
        mk::extract_variables(formula),
        (std::vector<std::string>{
            "first_value",
            "flag",
            "hidden",
            "other_value"}));

    require_calc_error(
        [] { (void)evaluate_formula("unknown_name"); });
}

TEST(formula_overflow) {
    require_calc_error(
        [] {
            (void)evaluate_formula(
                "9223372036854775807 + 1");
        });

    require_calc_error(
        [] {
            (void)evaluate_formula(
                "abs(minimum)",
                {{"minimum", Rational{
                    std::numeric_limits<int64_t>::min()}}});
        });

    require_calc_error(
        [] {
            (void)evaluate_formula(
                "-minimum",
                {{"minimum", Rational{
                    std::numeric_limits<int64_t>::min()}}});
        });
}

TEST(formula_identifier_validation) {
    require_calc_error(
        [] { (void)mk::parse("Upper"); });
    require_calc_error(
        [] { (void)mk::parse("é"); });
    require_calc_error(
        [] { (void)mk::parse("foo$bar"); });
}

TEST(formula_empty_formula) {
    const Formula formula;

    require_calc_error(
        [&] { (void)mk::evaluate(formula, {}); });
    require_calc_error(
        [&] { (void)mk::extract_variables(formula); });
}

TEST(formula_parser_errors) {
    require_calc_error(
        [] { (void)mk::parse(""); });
    require_calc_error(
        [] { (void)mk::parse("1 +"); });
    require_calc_error(
        [] { (void)mk::parse("1 2"); });
    require_calc_error(
        [] { (void)mk::parse("1 + 2 junk"); });
    require_calc_error(
        [] { (void)mk::parse("1.2.3"); });
    require_calc_error(
        [] { (void)mk::parse("9223372036854775808"); });
    require_calc_error(
        [] {
            (void)mk::parse(
                "0.0000000000000000000000000000000000000000000000000000000000001");
        });

    require_position(
        [] { (void)mk::parse("unknown(1)"); },
        "position 0");

    require_position(
        [] { (void)mk::parse("min(1)"); },
        "position 0");

    require_position(
        [] { (void)mk::parse("1 + @"); },
        "position 4");
}
