#include "mk/time_derive.hpp"

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace mk {
namespace {

const AbsenceKind* find_absence_kind(
    int64_t absence_kind_id,
    const std::vector<AbsenceKind>& absence_kinds) {
    for (const AbsenceKind& absence_kind : absence_kinds) {
        if (absence_kind.id == absence_kind_id) {
            return &absence_kind;
        }
    }

    return nullptr;
}

int32_t checked_worked_minutes(int64_t minutes) {
    if (minutes > std::numeric_limits<int32_t>::max()) {
        throw std::invalid_argument("worked minutes overflow");
    }

    return static_cast<int32_t>(minutes);
}

}

std::vector<DerivedTime> derive_time(
    int64_t employee_id,
    const std::vector<NormDay>& norm_days,
    const std::vector<TimeFact>& time_facts,
    const std::vector<AbsenceKind>& absence_kinds) {
    std::vector<DerivedTime> result;
    result.reserve(norm_days.size());

    for (const NormDay& norm_day : norm_days) {
        if (!norm_day.working_day && norm_day.normative_minutes != 0) {
            throw std::invalid_argument(
                "non-working day must have zero normative minutes");
        }

        int64_t worked_minutes = 0;

        for (const TimeFact& time_fact : time_facts) {
            if (time_fact.employee_id != employee_id ||
                time_fact.date != norm_day.date) {
                continue;
            }

            if (time_fact.worked_minutes < 0) {
                throw std::invalid_argument(
                    "worked minutes must not be negative");
            }

            if (time_fact.absence_kind_id.has_value() &&
                find_absence_kind(
                    *time_fact.absence_kind_id,
                    absence_kinds) == nullptr) {
                throw std::invalid_argument("unknown absence kind");
            }

            worked_minutes += time_fact.worked_minutes;

            if (worked_minutes > std::numeric_limits<int32_t>::max()) {
                throw std::invalid_argument("worked minutes overflow");
            }
        }

        const int32_t derived_worked_minutes =
            checked_worked_minutes(worked_minutes);

        int32_t absence_minutes = 0;
        if (norm_day.working_day &&
            derived_worked_minutes < norm_day.normative_minutes) {
            absence_minutes =
                norm_day.normative_minutes - derived_worked_minutes;
        }

        result.push_back({
            employee_id,
            norm_day.date,
            norm_day.normative_minutes,
            derived_worked_minutes,
            absence_minutes
        });
    }

    return result;
}

}
