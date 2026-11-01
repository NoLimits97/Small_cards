#include "test_framework.hpp"

#include "mk/calendar.hpp"

#include <string>
#include <vector>

namespace {

mk::WorkSchedule five_day_schedule() {
    mk::WorkSchedule schedule;
    schedule.kind = mk::ScheduleKind::FiveDay;
    schedule.normative_minutes_per_day = 480;
    return schedule;
}

mk::WorkSchedule shift_schedule(
    const std::string& cycle,
    const mk::Date& anchor_date) {
    mk::WorkSchedule schedule;
    schedule.kind = mk::ScheduleKind::Shift;
    schedule.normative_minutes_per_day = 720;
    schedule.shift_cycle = cycle;
    schedule.anchor_date = anchor_date;
    return schedule;
}

}

TEST(calendar_five_day_january_2026) {
    const std::vector<mk::NormDay> calendar =
        mk::generate_norm_calendar(
            five_day_schedule(),
            2026,
            1);

    REQUIRE_EQUAL(calendar.size(), std::size_t{31});

    int working_days = 0;
    for (const mk::NormDay& day : calendar) {
        if (day.working_day) {
            ++working_days;
            REQUIRE_EQUAL(day.normative_minutes, 480);
        } else {
            REQUIRE_EQUAL(day.normative_minutes, 0);
        }
    }

    REQUIRE_EQUAL(working_days, 22);
    REQUIRE_EQUAL(calendar.front().date, mk::Date(2026, 1, 1));
    REQUIRE_EQUAL(calendar.back().date, mk::Date(2026, 1, 31));
}

TEST(calendar_february_leap_year) {
    const std::vector<mk::NormDay> calendar =
        mk::generate_norm_calendar(
            five_day_schedule(),
            2024,
            2);

    REQUIRE_EQUAL(calendar.size(), std::size_t{29});
    REQUIRE_EQUAL(calendar.front().date, mk::Date(2024, 2, 1));
    REQUIRE_EQUAL(calendar.back().date, mk::Date(2024, 2, 29));

    for (const mk::NormDay& day : calendar) {
        REQUIRE_EQUAL(
            day.normative_minutes,
            day.working_day ? 480 : 0);
    }
}

TEST(calendar_shift_wwrr) {
    const mk::WorkSchedule schedule =
        shift_schedule("WWRR", mk::Date(2026, 1, 1));

    const std::vector<mk::NormDay> calendar =
        mk::generate_norm_calendar(schedule, 2026, 1);

    REQUIRE_EQUAL(calendar.size(), std::size_t{31});

    for (std::size_t index = 0; index < calendar.size(); ++index) {
        const bool expected_working =
            index % 4 == 0 || index % 4 == 1;

        REQUIRE_EQUAL(calendar[index].working_day, expected_working);
        REQUIRE_EQUAL(
            calendar[index].normative_minutes,
            expected_working ? 720 : 0);
    }
}

TEST(calendar_shift_cycle_crosses_month_boundary) {
    const mk::WorkSchedule schedule =
        shift_schedule("WWRR", mk::Date(2026, 1, 30));

    const std::vector<mk::NormDay> calendar =
        mk::generate_norm_calendar(schedule, 2026, 2);

    REQUIRE_EQUAL(calendar.size(), std::size_t{28});

    REQUIRE_EQUAL(calendar[0].date, mk::Date(2026, 2, 1));
    REQUIRE_EQUAL(calendar[0].working_day, false);
    REQUIRE_EQUAL(calendar[1].working_day, false);
    REQUIRE_EQUAL(calendar[2].working_day, true);
    REQUIRE_EQUAL(calendar[3].working_day, true);
}

TEST(calendar_shift_dates_before_anchor) {
    const mk::WorkSchedule schedule =
        shift_schedule("WWRR", mk::Date(2026, 1, 10));

    const std::vector<mk::NormDay> calendar =
        mk::generate_norm_calendar(schedule, 2026, 1);

    REQUIRE_EQUAL(calendar[0].date, mk::Date(2026, 1, 1));
    REQUIRE_EQUAL(calendar[0].working_day, false);
    REQUIRE_EQUAL(calendar[1].working_day, true);
    REQUIRE_EQUAL(calendar[2].working_day, true);
    REQUIRE_EQUAL(calendar[3].working_day, false);
}

TEST(calendar_rejects_invalid_parameters) {
    mk::WorkSchedule schedule = five_day_schedule();

    REQUIRE_THROWS(
        mk::generate_norm_calendar(schedule, 2026, 0));
    REQUIRE_THROWS(
        mk::generate_norm_calendar(schedule, 2026, 13));

    schedule.normative_minutes_per_day = 0;
    REQUIRE_THROWS(
        mk::generate_norm_calendar(schedule, 2026, 1));

    schedule = shift_schedule("", mk::Date(2026, 1, 1));
    REQUIRE_THROWS(
        mk::generate_norm_calendar(schedule, 2026, 1));

    schedule = shift_schedule("WWXR", mk::Date(2026, 1, 1));
    REQUIRE_THROWS(
        mk::generate_norm_calendar(schedule, 2026, 1));

    schedule = shift_schedule("WWRR", mk::Date(2026, 1, 1));
    schedule.anchor_date.reset();
    REQUIRE_THROWS(
        mk::generate_norm_calendar(schedule, 2026, 1));
}
