#include "test_framework.hpp"

#include "mk/engine.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace {

mk::CalcInput base_input() {
    mk::CalcInput input;
    input.period.id = 1;
    input.period.date_from = mk::Date(2026, 1, 1);
    input.period.date_to = mk::Date(2026, 1, 31);

    mk::EmployeeSlice employee;
    employee.employee_id = 1;
    employee.work_schedule_id = 10;
    employee.rate = mk::Money(5000000);
    input.employees.push_back(employee);

    mk::WorkSchedule schedule;
    schedule.id = 10;
    schedule.normative_minutes_per_day = 480;
    input.work_schedules.push_back(schedule);

    for (int day = 1; day <= 31; ++day) {
        mk::TimeFact fact;
        fact.employee_id = 1;
        fact.date = mk::Date(2026, 1, day);
        fact.worked_minutes = 480;
        input.time_facts.push_back(fact);
    }

    mk::EmploymentEvent hire;
    hire.employee_id = 1;
    hire.date = mk::Date(2025, 1, 1);
    hire.kind = mk::EventKind::Hire;
    hire.position_id = 1;
    hire.department_id = 1;
    hire.work_schedule_id = 10;
    hire.rate = mk::Money(5000000);
    hire.rate_unit = mk::RateUnit::Month;
    input.employment_events.push_back(hire);

    return input;
}

mk::CalcItem item(
    int64_t id,
    const std::string& code,
    const std::string& formula,
    mk::CalcRole role,
    int32_t stage,
    bool taxable = false,
    mk::RoundingPolicy rounding = mk::RoundingPolicy::PerLine) {
    mk::CalcItem result;
    result.id = id;
    result.code = code;
    result.formula = formula;
    result.role = role;
    result.stage = stage;
    result.taxable = taxable;
    result.rounding_policy = rounding;
    return result;
}

}

TEST(engine_salary_fraction) {
    mk::CalcInput input = base_input();
    input.calc_items.push_back(
        item(1, "salary", "base_amount * 15 / 17",
             mk::CalcRole::Accrual, 1, true));

    const mk::CalcOutput output = mk::calculate(input);

    REQUIRE_EQUAL(output.calculations.size(), std::size_t{1});
    REQUIRE_EQUAL(
        output.calculations[0].total_accrual,
        mk::Money(4411765));
}

TEST(engine_salary_and_bonus) {
    mk::CalcInput input = base_input();
    input.calc_items.push_back(
        item(1, "salary", "base_amount", mk::CalcRole::Accrual, 1, true));
    input.calc_items.push_back(
        item(2, "bonus", "salary * 0.1",
             mk::CalcRole::Accrual, 1, true));

    const mk::CalcOutput output = mk::calculate(input);

    REQUIRE_EQUAL(
        output.calculations[0].total_accrual,
        mk::Money(5500000));
}

TEST(engine_income_tax) {
    mk::CalcInput input = base_input();
    input.calc_items.push_back(
        item(1, "salary", "base_amount",
             mk::CalcRole::Accrual, 1, true));
    input.calc_items.push_back(
        item(2, "tax", "ndfl_base * 0.13",
             mk::CalcRole::IncomeTax, 4));

    const mk::CalcOutput output = mk::calculate(input);

    REQUIRE_EQUAL(
        output.calculations[0].income_tax,
        mk::Money(650000));
    REQUIRE_EQUAL(
        output.calculations[0].net_amount,
        mk::Money(4350000));
}

TEST(engine_absence_formula) {
    mk::CalcInput input = base_input();
    input.calc_items.clear();
    input.calc_items.push_back(
        item(1, "salary", "if(absent_no_leave == 0, base_amount, 0)",
             mk::CalcRole::Accrual, 1, true));

    input.time_facts.clear();
    for (int day = 1; day <= 31; ++day) {
        mk::TimeFact fact;
        fact.employee_id = 1;
        fact.date = mk::Date(2026, 1, day);
        fact.worked_minutes = 480;
        if (day == 15) {
            fact.worked_minutes = 0;
        }
        input.time_facts.push_back(fact);
    }

    const mk::CalcOutput output = mk::calculate(input);

    REQUIRE_EQUAL(output.calculations[0].total_accrual, mk::Money{});
}

TEST(engine_cycle_is_rejected) {
    mk::CalcInput input = base_input();
    input.calc_items.push_back(
        item(1, "a", "b + 1", mk::CalcRole::Accrual, 1));
    input.calc_items.push_back(
        item(2, "b", "a + 1", mk::CalcRole::Accrual, 1));

    REQUIRE_THROWS(mk::calculate(input));
}

TEST(engine_unknown_variable_is_rejected) {
    mk::CalcInput input = base_input();
    input.calc_items.push_back(
        item(1, "salary", "unknown_value",
             mk::CalcRole::Accrual, 1));

    REQUIRE_THROWS(mk::calculate(input));
}

TEST(engine_empty_timesheet_is_incomplete) {
    mk::CalcInput input = base_input();
    input.time_facts.clear();
    input.calc_items.push_back(
        item(1, "salary", "base_amount",
             mk::CalcRole::Accrual, 1, true));

    const mk::CalcOutput output = mk::calculate(input);

    REQUIRE_EQUAL(
        output.calculations[0].status,
        mk::CalcStatus::Incomplete);
    REQUIRE(!output.calculations[0].incomplete_reasons.empty());
}

TEST(engine_negative_net) {
    mk::CalcInput input = base_input();
    input.calc_items.push_back(
        item(1, "salary", "base_amount",
             mk::CalcRole::Accrual, 1, true));
    input.calc_items.push_back(
        item(2, "deduction", "base_amount * 2",
             mk::CalcRole::PostTaxDeduction, 5));

    const mk::CalcOutput output = mk::calculate(input);

    REQUIRE_EQUAL(
        output.calculations[0].status,
        mk::CalcStatus::NegativeNet);
    REQUIRE_EQUAL(
        output.calculations[0].net_amount,
        mk::Money(-5000000));
}

TEST(engine_formula_snapshot_and_version) {
    mk::CalcInput input = base_input();
    input.calc_items.push_back(
        item(7, "salary", "base_amount",
             mk::CalcRole::Accrual, 1));

    const mk::CalcOutput output = mk::calculate(input);

    REQUIRE_EQUAL(
        output.calculations[0].lines[0].calc_item_id,
        int64_t{7});
    REQUIRE_EQUAL(
        output.calculations[0].lines[0].formula_snapshot,
        std::string("base_amount"));
    REQUIRE_EQUAL(
        output.calculations[0].engine_version,
        std::string("0.1.0"));
}

TEST(engine_carry_and_per_line) {
    mk::CalcInput carry_input = base_input();
    carry_input.calc_items.push_back(
        item(1, "a", "0.005",
             mk::CalcRole::Accrual, 1, false,
             mk::RoundingPolicy::Carry));
    carry_input.calc_items.push_back(
        item(2, "b", "0.005",
             mk::CalcRole::Accrual, 1, false,
             mk::RoundingPolicy::Carry));

    const mk::CalcOutput carry_output = mk::calculate(carry_input);
    REQUIRE_EQUAL(
        carry_output.calculations[0].total_accrual,
        mk::Money(1));

    mk::CalcInput per_line_input = base_input();
    per_line_input.calc_items.push_back(
        item(1, "a", "0.005",
             mk::CalcRole::Accrual, 1));
    per_line_input.calc_items.push_back(
        item(2, "b", "0.005",
             mk::CalcRole::Accrual, 1));

    const mk::CalcOutput per_line_output =
        mk::calculate(per_line_input);

    REQUIRE_EQUAL(
        per_line_output.calculations[0].total_accrual,
        mk::Money(2));
}

TEST(engine_contributions_do_not_reduce_net) {
    mk::CalcInput input = base_input();
    input.calc_items.push_back(
        item(1, "salary", "base_amount",
             mk::CalcRole::Accrual, 1, true));
    input.calc_items.push_back(
        item(2, "contrib", "base_amount * 0.3",
             mk::CalcRole::EmployerContribution, 4));

    const mk::CalcOutput output = mk::calculate(input);

    REQUIRE_EQUAL(
        output.calculations[0].total_employer_contribution,
        mk::Money(1500000));
    REQUIRE_EQUAL(
        output.calculations[0].net_amount,
        mk::Money(5000000));
}
