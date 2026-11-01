#include "test_framework.hpp"

#include "mk/model.hpp"

#include <cstdint>
#include <string>

TEST(model_enum_codes_are_stable) {
    REQUIRE_EQUAL(static_cast<int32_t>(mk::RateUnit::Month), 1);
    REQUIRE_EQUAL(static_cast<int32_t>(mk::RateUnit::Day), 2);
    REQUIRE_EQUAL(static_cast<int32_t>(mk::RateUnit::Hour), 3);

    REQUIRE_EQUAL(static_cast<int32_t>(mk::EventKind::Hire), 1);
    REQUIRE_EQUAL(static_cast<int32_t>(mk::EventKind::Transfer), 2);
    REQUIRE_EQUAL(static_cast<int32_t>(mk::EventKind::ChangeSalary), 3);
    REQUIRE_EQUAL(static_cast<int32_t>(mk::EventKind::Termination), 4);

    REQUIRE_EQUAL(static_cast<int32_t>(mk::ScheduleKind::FiveDay), 1);
    REQUIRE_EQUAL(static_cast<int32_t>(mk::ScheduleKind::Shift), 2);

    REQUIRE_EQUAL(static_cast<int32_t>(mk::PeriodStatus::Draft), 1);
    REQUIRE_EQUAL(static_cast<int32_t>(mk::PeriodStatus::Closed), 2);

    REQUIRE_EQUAL(static_cast<int32_t>(mk::CalcStatus::Ok), 1);
    REQUIRE_EQUAL(static_cast<int32_t>(mk::CalcStatus::Incomplete), 2);
    REQUIRE_EQUAL(static_cast<int32_t>(mk::CalcStatus::NegativeNet), 3);

    REQUIRE_EQUAL(static_cast<int32_t>(mk::PayChannel::Bank), 1);
    REQUIRE_EQUAL(static_cast<int32_t>(mk::PayChannel::Cash), 2);

    REQUIRE_EQUAL(static_cast<int32_t>(mk::CalcRole::Accrual), 1);
    REQUIRE_EQUAL(static_cast<int32_t>(mk::CalcRole::PreTaxDeduction), 2);
    REQUIRE_EQUAL(static_cast<int32_t>(mk::CalcRole::IncomeTax), 3);
    REQUIRE_EQUAL(
        static_cast<int32_t>(mk::CalcRole::EmployerContribution),
        4);
    REQUIRE_EQUAL(static_cast<int32_t>(mk::CalcRole::PostTaxDeduction), 5);

    REQUIRE_EQUAL(static_cast<int32_t>(mk::RoundingPolicy::PerLine), 1);
}

TEST(model_structures_can_be_created) {
    mk::Org org{1, "org"};
    mk::Department department{2, org.id, "department"};

    mk::WorkSchedule schedule;
    schedule.id = 3;
    schedule.kind = mk::ScheduleKind::Shift;
    schedule.normative_minutes_per_day = 480;
    schedule.shift_cycle = "WWR";
    schedule.anchor_date = mk::Date(2025, 1, 1);

    mk::CalcItem item;
    item.id = 4;
    item.code = "salary";
    item.name = "Salary";
    item.formula = "salary";
    item.role = mk::CalcRole::Accrual;
    item.rounding_policy = mk::RoundingPolicy::PerLine;

    mk::PayrollPreset preset;
    preset.id = 5;
    preset.code = "default";
    preset.name = "Default";
    preset.calc_item_ids.push_back(item.id);

    mk::Employee employee;
    employee.id = 6;
    employee.org_id = org.id;
    employee.department_id = department.id;
    employee.work_schedule_id = schedule.id;
    employee.payroll_preset_id = preset.id;
    employee.rate = mk::Money(100000);
    employee.rate_unit = mk::RateUnit::Month;

    mk::EmploymentEvent hire;
    hire.employee_id = employee.id;
    hire.date = employee.hire_date;
    hire.kind = mk::EventKind::Hire;
    hire.personnel_number = employee.personnel_number;
    hire.full_name = employee.full_name;
    hire.org_id = employee.org_id;
    hire.department_id = employee.department_id;
    hire.position_id = employee.position_id;
    hire.employment_kind_id = employee.employment_kind_id;
    hire.work_schedule_id = employee.work_schedule_id;
    hire.payroll_preset_id = employee.payroll_preset_id;
    hire.rate = employee.rate;
    hire.rate_unit = employee.rate_unit;

    mk::TimeFact time_fact;
    time_fact.employee_id = employee.id;
    time_fact.date = mk::Date(2025, 1, 2);
    time_fact.worked_minutes = 480;

    mk::CalculationLine line;
    line.calc_item_id = item.id;
    line.calc_item_code = item.code;
    line.role = item.role;
    line.amount = mk::Money(100000);
    line.formula_snapshot = item.formula;
    line.rounding_policy = item.rounding_policy;

    mk::Calculation calculation;
    calculation.employee_id = employee.id;
    calculation.status = mk::CalcStatus::Ok;
    calculation.engine_version = "m2";
    calculation.lines.push_back(line);

    mk::CalcInput input;
    input.employees.push_back(mk::EmployeeSlice{
        employee.id,
        employee.personnel_number,
        employee.full_name,
        employee.org_id,
        employee.department_id,
        employee.position_id,
        employee.employment_kind_id,
        employee.work_schedule_id,
        employee.payroll_preset_id,
        employee.rate,
        employee.rate_unit
    });
    input.employment_events.push_back(hire);
    input.work_schedules.push_back(schedule);
    input.payroll_presets.push_back(preset);
    input.calc_items.push_back(item);
    input.time_facts.push_back(time_fact);

    mk::CalcOutput output;
    output.input_snapshot = input;
    output.calculations.push_back(calculation);

    REQUIRE_EQUAL(output.input_snapshot.employees.size(), std::size_t(1));
    REQUIRE_EQUAL(output.calculations.size(), std::size_t(1));
    REQUIRE_EQUAL(
        output.calculations.front().lines.front().amount,
        mk::Money(100000));
    REQUIRE_EQUAL(
        output.calculations.front().lines.front().formula_snapshot,
        std::string("salary"));
}

TEST(model_optional_event_fields) {
    mk::EmploymentEvent event;
    event.employee_id = 10;
    event.date = mk::Date(2025, 2, 1);
    event.kind = mk::EventKind::ChangeSalary;
    event.rate = mk::Money(125000);
    event.rate_unit = mk::RateUnit::Month;

    REQUIRE(event.rate.has_value());
    REQUIRE(event.rate_unit.has_value());
    REQUIRE(!event.department_id.has_value());
}

TEST(model_time_fact_semantics) {
    mk::TimeFact confirmed;
    confirmed.employee_id = 10;
    confirmed.date = mk::Date(2025, 2, 3);
    confirmed.worked_minutes = 480;

    mk::TimeFact absence;
    absence.employee_id = 10;
    absence.date = mk::Date(2025, 2, 4);
    absence.worked_minutes = 0;
    absence.absence_kind_id = 7;

    REQUIRE(!confirmed.absence_kind_id.has_value());
    REQUIRE(absence.absence_kind_id.has_value());
    REQUIRE_EQUAL(*absence.absence_kind_id, int64_t{7});
}
