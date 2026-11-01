#include "mk/slicing.hpp"

#include <optional>
#include <vector>

namespace mk {

std::optional<EmployeeSlice> resolve_slice(
    const std::vector<EmploymentEvent>& events,
    const Date& period_first_day) {
    const EmploymentEvent* selected_event = nullptr;

    for (const EmploymentEvent& event : events) {
        if (event.date <= period_first_day &&
            (selected_event == nullptr || selected_event->date <= event.date)) {
            selected_event = &event;
        }
    }

    if (selected_event == nullptr ||
        selected_event->kind == EventKind::Termination) {
        return std::nullopt;
    }

    if (!selected_event->position_id.has_value() ||
        !selected_event->department_id.has_value() ||
        !selected_event->work_schedule_id.has_value() ||
        !selected_event->rate.has_value() ||
        !selected_event->rate_unit.has_value()) {
        return std::nullopt;
    }

    EmployeeSlice result;
    result.employee_id = selected_event->employee_id;
    result.position_id = *selected_event->position_id;
    result.department_id = *selected_event->department_id;
    result.work_schedule_id = *selected_event->work_schedule_id;
    result.rate = *selected_event->rate;
    result.rate_unit = *selected_event->rate_unit;

    if (selected_event->personnel_number.has_value()) {
        result.personnel_number = *selected_event->personnel_number;
    }

    if (selected_event->full_name.has_value()) {
        result.full_name = *selected_event->full_name;
    }

    if (selected_event->org_id.has_value()) {
        result.org_id = *selected_event->org_id;
    }

    if (selected_event->employment_kind_id.has_value()) {
        result.employment_kind_id = *selected_event->employment_kind_id;
    }

    if (selected_event->payroll_preset_id.has_value()) {
        result.payroll_preset_id = *selected_event->payroll_preset_id;
    }

    return result;
}

}
