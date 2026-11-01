#include "test_framework.hpp"

#include "mk/time_derive.hpp"

#include <cstdint>
#include <stdexcept>
#include <vector>

namespace {

mk::NormDay working_day(int32_t year, int32_t month, int32_t day) {
    return {
        mk::Date(year, month, day),
        480,
        true
    };
}

mk::NormDay non_working_day(int32_t year, int32_t month, int32_t day) {
    return {
        mk::Date(year, month, day),
        0,
        false
    };
}

mk::TimeFact time_fact(
    int64_t employee_id,
    int32_t year,
    int32_t month,
    int32_t day,
    int32_t worked_minutes) {
    mk::TimeFact fact;
    fact.employee_id = employee_id;
    fact.date = mk::Date(year, month, day);
    fact.worked_minutes = worked_minutes;
    return fact;
}

mk::AbsenceKind absence_kind(int64_t id, bool paid) {
    mk::AbsenceKind kind;
    kind.id = id;
    kind.paid = paid;
    return kind;
}

}

TEST(derive_time_working_day_without_fact) {
    const std::vector<mk::DerivedTime> result =
        mk::derive_time(
            1,
            {working_day(2026, 1, 12)},
            {},
            {});

    REQUIRE_EQUAL(result.size(), std::size_t{1});
    REQUIRE_EQUAL(result[0].employee_id, int64_t{1});
    REQUIRE_EQUAL(result[0].normative_minutes, int32_t{480});
    REQUIRE_EQUAL(result[0].worked_minutes, int32_t{0});
    REQUIRE_EQUAL(result[0].absence_minutes, int32_t{480});
}

TEST(derive_time_partially_worked_day) {
    const std::vector<mk::DerivedTime> result =
        mk::derive_time(
            1,
            {working_day(2026, 1, 12)},
            {time_fact(1, 2026, 1, 12, 300)},
            {});

    REQUIRE_EQUAL(result[0].worked_minutes, int32_t{300});
    REQUIRE_EQUAL(result[0].absence_minutes, int32_t{180});
}

TEST(derive_time_fully_worked_day) {
    const std::vector<mk::DerivedTime> result =
        mk::derive_time(
            1,
            {working_day(2026, 1, 12)},
            {time_fact(1, 2026, 1, 12, 480)},
            {});

    REQUIRE_EQUAL(result[0].worked_minutes, int32_t{480});
    REQUIRE_EQUAL(result[0].absence_minutes, int32_t{0});
}

TEST(derive_time_overworked_day) {
    const std::vector<mk::DerivedTime> result =
        mk::derive_time(
            1,
            {working_day(2026, 1, 12)},
            {time_fact(1, 2026, 1, 12, 600)},
            {});

    REQUIRE_EQUAL(result[0].worked_minutes, int32_t{600});
    REQUIRE_EQUAL(result[0].absence_minutes, int32_t{0});
}

TEST(derive_time_non_working_day) {
    const std::vector<mk::DerivedTime> result =
        mk::derive_time(
            1,
            {non_working_day(2026, 1, 11)},
            {time_fact(1, 2026, 1, 11, 120)},
            {});

    REQUIRE_EQUAL(result[0].normative_minutes, int32_t{0});
    REQUIRE_EQUAL(result[0].worked_minutes, int32_t{120});
    REQUIRE_EQUAL(result[0].absence_minutes, int32_t{0});
}

TEST(derive_time_ignores_other_employee) {
    const std::vector<mk::DerivedTime> result =
        mk::derive_time(
            1,
            {working_day(2026, 1, 12)},
            {time_fact(2, 2026, 1, 12, 480)},
            {});

    REQUIRE_EQUAL(result[0].worked_minutes, int32_t{0});
    REQUIRE_EQUAL(result[0].absence_minutes, int32_t{480});
}

TEST(derive_time_sums_multiple_facts) {
    const std::vector<mk::DerivedTime> result =
        mk::derive_time(
            1,
            {working_day(2026, 1, 12)},
            {
                time_fact(1, 2026, 1, 12, 200),
                time_fact(1, 2026, 1, 12, 280)
            },
            {});

    REQUIRE_EQUAL(result[0].worked_minutes, int32_t{480});
    REQUIRE_EQUAL(result[0].absence_minutes, int32_t{0});
}

TEST(derive_time_validates_absence_kind) {
    mk::TimeFact fact = time_fact(1, 2026, 1, 12, 0);
    fact.absence_kind_id = 10;

    REQUIRE_THROWS(
        mk::derive_time(
            1,
            {working_day(2026, 1, 12)},
            {fact},
            {absence_kind(11, true)}));
}

TEST(derive_time_accepts_known_paid_and_unpaid_absence_kinds) {
    mk::TimeFact paid_fact = time_fact(1, 2026, 1, 12, 0);
    paid_fact.absence_kind_id = 10;

    mk::TimeFact unpaid_fact = time_fact(1, 2026, 1, 13, 0);
    unpaid_fact.absence_kind_id = 11;

    const std::vector<mk::DerivedTime> result =
        mk::derive_time(
            1,
            {
                working_day(2026, 1, 12),
                working_day(2026, 1, 13)
            },
            {paid_fact, unpaid_fact},
            {
                absence_kind(10, true),
                absence_kind(11, false)
            });

    REQUIRE_EQUAL(result.size(), std::size_t{2});
    REQUIRE_EQUAL(result[0].absence_minutes, int32_t{480});
    REQUIRE_EQUAL(result[1].absence_minutes, int32_t{480});
}

TEST(derive_time_rejects_negative_worked_minutes) {
    REQUIRE_THROWS(
        mk::derive_time(
            1,
            {working_day(2026, 1, 12)},
            {time_fact(1, 2026, 1, 12, -1)},
            {}));
}

TEST(derive_time_empty_calendar) {
    const std::vector<mk::DerivedTime> result =
        mk::derive_time(
            1,
            {},
            {
                time_fact(1, 2026, 1, 12, 480)
            },
            {});

    REQUIRE(result.empty());
}

TEST(derive_time_rejects_non_working_day_normative_minutes) {
    mk::NormDay invalid_day{
        mk::Date(2026, 1, 11),
        480,
        false
    };

    REQUIRE_THROWS(
        mk::derive_time(
            1,
            {invalid_day},
            {},
            {}));
}
