#include "mk/calendar.hpp"

#include <cstdint>
#include <vector>

namespace mk {
namespace {

int64_t positive_modulo(int64_t value, int64_t divisor) {
    const int64_t remainder = value % divisor;
    return remainder < 0 ? remainder + divisor : remainder;
}

bool is_weekday(Date date) {
    const int64_t monday_based_day =
        positive_modulo(
            static_cast<int64_t>(date.ordinal()) + 3,
            7);

    return monday_based_day < 5;
}

void validate_schedule(
    const WorkSchedule& schedule,
    int32_t year,
    int32_t month) {
    (void)Date::days_in_month(year, month);

    if (schedule.normative_minutes_per_day <= 0) {
        throw CalcError("normative minutes per day must be positive");
    }

    if (schedule.kind == ScheduleKind::FiveDay) {
        return;
    }

    if (schedule.kind != ScheduleKind::Shift) {
        throw CalcError("invalid schedule kind");
    }

    if (schedule.shift_cycle.empty()) {
        throw CalcError("shift cycle must not be empty");
    }

    for (const char character : schedule.shift_cycle) {
        if (character != 'W' && character != 'R') {
            throw CalcError("shift cycle must contain only W and R");
        }
    }

    if (!schedule.anchor_date.has_value()) {
        throw CalcError("shift schedule requires an anchor date");
    }
}

}

std::vector<NormDay> generate_norm_calendar(
    const WorkSchedule& schedule,
    int32_t year,
    int32_t month) {
    validate_schedule(schedule, year, month);

    const int32_t days_in_month = Date::days_in_month(year, month);
    const Date first_date = Date::first_of_month(year, month);

    std::vector<NormDay> result;
    result.reserve(static_cast<std::size_t>(days_in_month));

    for (int32_t day = 1; day <= days_in_month; ++day) {
        const Date date = first_date + (day - 1);
        bool working_day = false;

        if (schedule.kind == ScheduleKind::FiveDay) {
            working_day = is_weekday(date);
        } else {
            const int64_t offset =
                static_cast<int64_t>(date.ordinal()) -
                static_cast<int64_t>(
                    schedule.anchor_date->ordinal());

            const int64_t cycle_length =
                static_cast<int64_t>(schedule.shift_cycle.size());

            const int64_t cycle_position =
                positive_modulo(offset, cycle_length);

            working_day =
                schedule.shift_cycle[
                    static_cast<std::size_t>(cycle_position)] == 'W';
        }

        result.push_back({
            date,
            working_day ? schedule.normative_minutes_per_day : 0,
            working_day
        });
    }

    return result;
}

}
