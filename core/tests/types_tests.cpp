#include "test_framework.hpp"

#include "mk/types.hpp"

#include <cstdint>
#include <limits>

using mk::Date;
using mk::Money;
using mk::Rational;

TEST(money_arithmetic) {
    REQUIRE_EQUAL((Money{5000000} + Money{-715000}), Money{4285000});
    REQUIRE_EQUAL((Money{} + Money{}), Money{});
    REQUIRE_EQUAL((Money{10} - Money{0}), Money{10});
}

TEST(money_overflow) {
    REQUIRE_THROWS((Money{std::numeric_limits<int64_t>::max()} + Money{1}));
    REQUIRE_THROWS((Money{std::numeric_limits<int64_t>::min()} - Money{1}));
}

TEST(rational_reduction_and_arithmetic) {
    REQUIRE_EQUAL((Rational{2, 4}), (Rational{1, 2}));
    REQUIRE_EQUAL((Rational{1, 2} + Rational{1, 3}), (Rational{5, 6}));
    REQUIRE_EQUAL((Rational{1, 2} - Rational{1, 3}), (Rational{1, 6}));
    REQUIRE_EQUAL((Rational{1, -2}), (Rational{-1, 2}));
    REQUIRE_EQUAL((Rational{0, -9}), (Rational{0, 1}));
}

TEST(rational_division_and_overflow) {
    REQUIRE_THROWS((Rational{1} / Rational{0}));
    REQUIRE_THROWS(
        (Rational{std::numeric_limits<int64_t>::max()} * Rational{2}));
    REQUIRE_THROWS(
        (Rational{1} / Rational{std::numeric_limits<int64_t>::min()}));
}

TEST(rational_rounding) {
    REQUIRE_EQUAL(
        (Rational{5000000} * Rational{15} / Rational{17}).round_half_up_to_int64(),
        int64_t{4411765});
    REQUIRE_EQUAL(
        (Rational{-44117655, 10}).round_half_up_to_int64(),
        int64_t{-4411766});
    REQUIRE_EQUAL((Rational{3, 2}).round_half_up_to_int64(), int64_t{2});
    REQUIRE_EQUAL((Rational{-3, 2}).round_half_up_to_int64(), int64_t{-2});
    REQUIRE_EQUAL((Rational{14, 10}).round_half_up_to_int64(), int64_t{1});
    REQUIRE_EQUAL((Rational{-14, 10}).round_half_up_to_int64(), int64_t{-1});
    REQUIRE_EQUAL(
        (Rational{std::numeric_limits<int64_t>::min()}).round_half_up_to_int64(),
        std::numeric_limits<int64_t>::min());
}

TEST(rational_negation_and_comparison) {
    REQUIRE_THROWS((-Rational{std::numeric_limits<int64_t>::min()}));
    REQUIRE(
        (Rational{std::numeric_limits<int64_t>::max(), 2} <
         Rational{std::numeric_limits<int64_t>::max(), 1}));

    const Rational a{5000000};
    const Rational b{15};
    const Rational c{17};

    REQUIRE_EQUAL((a * b / c), ((a / c) * b));
}

TEST(date_month_lengths) {
    REQUIRE_EQUAL(Date::days_in_month(2024, 2), 29);
    REQUIRE_EQUAL(Date::days_in_month(2026, 2), 28);
    REQUIRE_EQUAL(Date::days_in_month(1900, 2), 28);
    REQUIRE_EQUAL(Date::days_in_month(2000, 2), 29);
}

TEST(date_round_trip) {
    const Date first{1970, 1, 1};
    const Date leap{2024, 2, 29};
    const Date last{2026, 12, 31};

    REQUIRE_EQUAL(first.ordinal(), 0);
    REQUIRE_EQUAL(Date{1969, 12, 31}.ordinal(), -1);

    REQUIRE_EQUAL(Date::from_ordinal(first.ordinal()), first);
    REQUIRE_EQUAL(Date::from_ordinal(leap.ordinal()), leap);
    REQUIRE_EQUAL(Date::from_ordinal(last.ordinal()), last);
}

TEST(date_arithmetic_and_validation) {
    REQUIRE_EQUAL((Date{2024, 1, 31} + 1), (Date{2024, 2, 1}));
    REQUIRE_EQUAL((Date{2023, 12, 31} + 1), (Date{2024, 1, 1}));
    REQUIRE_EQUAL(
        (Date{2024, 2, 1} - Date{2024, 1, 31}),
        1);
    REQUIRE_EQUAL(
        Date::first_of_month(2026, 2),
        (Date{2026, 2, 1}));

    REQUIRE_THROWS((Date{2024, 2, 30}));
    REQUIRE_THROWS((Date{0, 1, 1}));
    REQUIRE_THROWS(
        Date::from_ordinal(std::numeric_limits<int32_t>::max()));
}
