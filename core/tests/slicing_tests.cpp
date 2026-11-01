#include "test_framework.hpp"

#include "mk/slicing.hpp"

#include <optional>
#include <vector>

namespace {

mk::EmploymentEvent complete_event(
    int64_t employee_id,
    const mk::Date& date,
    mk::EventKind kind) {
    mk::EmploymentEvent event;
    event.employee_id = employee_id;
    event.date = date;
    event.kind = kind;
    event.personnel_number = "001";
    event.full_name = "Иванов Иван";
    event.org_id = 1;
    event.department_id = 2;
    event.position_id = 3;
    event.employment_kind_id = 4;
    event.work_schedule_id = 5;
    event.payroll_preset_id = 6;
    event.rate = mk::Money(100000);
    event.rate_unit = mk::RateUnit::Month;
    return event;
}

}

TEST(slicing_hire_before_period_is_visible) {
    const std::vector<mk::EmploymentEvent> events{
        complete_event(
            1,
            mk::Date(2025, 12, 15),
            mk::EventKind::Hire)
    };

    const std::optional<mk::EmployeeSlice> result =
        mk::resolve_slice(events, mk::Date(2026, 1, 1));

    REQUIRE(result.has_value());
    REQUIRE_EQUAL(result->employee_id, int64_t{1});
    REQUIRE_EQUAL(result->position_id, int64_t{3});
    REQUIRE_EQUAL(result->department_id, int64_t{2});
    REQUIRE_EQUAL(result->work_schedule_id, int64_t{5});
    REQUIRE_EQUAL(result->rate, mk::Money(100000));
    REQUIRE_EQUAL(result->rate_unit, mk::RateUnit::Month);
}

TEST(slicing_transfer_after_period_is_ignored) {
    std::vector<mk::EmploymentEvent> events{
        complete_event(
            1,
            mk::Date(2025, 12, 15),
            mk::EventKind::Hire),
        complete_event(
            1,
            mk::Date(2026, 1, 10),
            mk::EventKind::Transfer)
    };

    events[1].position_id = 99;

    const std::optional<mk::EmployeeSlice> result =
        mk::resolve_slice(events, mk::Date(2026, 1, 1));

    REQUIRE(result.has_value());
    REQUIRE_EQUAL(result->position_id, int64_t{3});
}

TEST(slicing_termination_before_period_excludes_employee) {
    const std::vector<mk::EmploymentEvent> events{
        complete_event(
            1,
            mk::Date(2025, 12, 15),
            mk::EventKind::Hire),
        complete_event(
            1,
            mk::Date(2025, 12, 20),
            mk::EventKind::Termination)
    };

    const std::optional<mk::EmployeeSlice> result =
        mk::resolve_slice(events, mk::Date(2026, 1, 1));

    REQUIRE(!result.has_value());
}

TEST(slicing_selects_latest_event) {
    std::vector<mk::EmploymentEvent> events{
        complete_event(
            1,
            mk::Date(2025, 12, 15),
            mk::EventKind::Hire),
        complete_event(
            1,
            mk::Date(2025, 12, 20),
            mk::EventKind::Transfer)
    };

    events[1].department_id = 20;

    const std::optional<mk::EmployeeSlice> result =
        mk::resolve_slice(events, mk::Date(2026, 1, 1));

    REQUIRE(result.has_value());
    REQUIRE_EQUAL(result->department_id, int64_t{20});
}

TEST(slicing_missing_rate_returns_empty) {
    mk::EmploymentEvent event = complete_event(
        1,
        mk::Date(2025, 12, 15),
        mk::EventKind::Hire);
    event.rate.reset();

    const std::optional<mk::EmployeeSlice> result =
        mk::resolve_slice({event}, mk::Date(2026, 1, 1));

    REQUIRE(!result.has_value());
}

TEST(slicing_missing_required_field_returns_empty) {
    mk::EmploymentEvent event = complete_event(
        1,
        mk::Date(2025, 12, 15),
        mk::EventKind::Hire);
    event.work_schedule_id.reset();

    const std::optional<mk::EmployeeSlice> result =
        mk::resolve_slice({event}, mk::Date(2026, 1, 1));

    REQUIRE(!result.has_value());
}

TEST(slicing_same_date_uses_later_event) {
    std::vector<mk::EmploymentEvent> events{
        complete_event(
            1,
            mk::Date(2025, 12, 15),
            mk::EventKind::Hire),
        complete_event(
            1,
            mk::Date(2025, 12, 15),
            mk::EventKind::ChangeSalary)
    };

    events[0].rate = mk::Money(100000);
    events[1].rate = mk::Money(125000);

    const std::optional<mk::EmployeeSlice> result =
        mk::resolve_slice(events, mk::Date(2026, 1, 1));

    REQUIRE(result.has_value());
    REQUIRE_EQUAL(result->rate, mk::Money(125000));
}

TEST(slicing_empty_events_returns_empty) {
    const std::optional<mk::EmployeeSlice> result =
        mk::resolve_slice({}, mk::Date(2026, 1, 1));

    REQUIRE(!result.has_value());
}
