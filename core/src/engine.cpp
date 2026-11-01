#include "mk/engine.hpp"

#include "mk/calendar.hpp"
#include "mk/dependency_graph.hpp"
#include "mk/formula.hpp"
#include "mk/slicing.hpp"
#include "mk/time_derive.hpp"

#include <algorithm>
#include <cstdint>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace mk {
namespace {

constexpr const char* engine_version = "0.1.0";

using Context = std::map<std::string, Rational>;

const std::set<std::string> built_in_variables{
    "base_amount",
    "worked_days",
    "worked_minutes",
    "norm_days",
    "norm_minutes",
    "absent_paid",
    "absent_unpaid",
    "absent_no_leave",
    "days_in_month",
    "working_days_in_month",
    "minutes_in_month_norm",
    "accruals_taxable",
    "deductions_pre_tax",
    "contrib_base",
    "ndfl_base"
};

Rational rational_minutes(int64_t value) {
    return Rational(value);
}

Money round_rubles_to_money(const Rational& value) {
    return Money((value * Rational(100)).round_half_up_to_int64());
}

Rational money_to_rational(Money value) {
    return Rational(value.cents, 100);
}

void add_error(
    std::vector<ValidationError>& errors,
    const std::string& field,
    const std::string& message) {
    errors.push_back({field, message});
}

std::map<int64_t, const WorkSchedule*> schedule_index(
    const CalcInput& input) {
    std::map<int64_t, const WorkSchedule*> result;

    for (const WorkSchedule& schedule : input.work_schedules) {
        result.emplace(schedule.id, &schedule);
    }

    return result;
}

std::map<int64_t, const PayrollPreset*> preset_index(
    const CalcInput& input) {
    std::map<int64_t, const PayrollPreset*> result;

    for (const PayrollPreset& preset : input.payroll_presets) {
        result.emplace(preset.id, &preset);
    }

    return result;
}

std::vector<CalcItem> selected_items(
    const CalcInput& input,
    const EmployeeSlice& employee) {
    std::map<int64_t, const CalcItem*> by_id;

    for (const CalcItem& item : input.calc_items) {
        by_id[item.id] = &item;
    }

    std::vector<CalcItem> result;

    if (!employee.payroll_preset_id.has_value()) {
        result = input.calc_items;
    } else {
        const auto presets = preset_index(input);
        const auto preset = presets.find(*employee.payroll_preset_id);

        if (preset == presets.end()) {
            return result;
        }

        for (const int64_t id : preset->second->calc_item_ids) {
            const auto item = by_id.find(id);
            if (item != by_id.end()) {
                result.push_back(*item->second);
            }
        }
    }

    return result;
}

std::vector<CalcItem> items_with_builtins(
    const std::vector<CalcItem>& items) {
    std::vector<CalcItem> result = items;
    int64_t id = -1;

    for (const std::string& name : built_in_variables) {
        CalcItem item;
        item.id = id--;
        item.code = name;
        item.formula = "0";
        item.stage = 1;
        result.push_back(item);
    }

    return result;
}

std::vector<CalcItem> sort_selected_items(
    const std::vector<CalcItem>& items) {
    const std::vector<CalcItem> augmented =
        items_with_builtins(items);
    const std::vector<CalcItem> sorted =
        sort_calc_items(augmented);

    std::set<std::string> names;
    for (const CalcItem& item : items) {
        names.insert(item.code);
    }

    std::vector<CalcItem> result;
    for (const CalcItem& item : sorted) {
        if (names.find(item.code) != names.end()) {
            result.push_back(item);
        }
    }

    return result;
}

bool role_stage_is_valid(const CalcItem& item) {
    switch (item.role) {
    case CalcRole::Accrual:
        return item.stage == 1;
    case CalcRole::PreTaxDeduction:
        return item.stage == 3;
    case CalcRole::IncomeTax:
    case CalcRole::EmployerContribution:
        return item.stage == 4;
    case CalcRole::PostTaxDeduction:
        return item.stage == 5;
    }

    return false;
}

void validate_formula_variables(
    const CalcInput& input,
    std::vector<ValidationError>& errors) {
    std::set<std::string> item_names;

    for (const CalcItem& item : input.calc_items) {
        item_names.insert(item.code);
    }

    for (const CalcItem& item : input.calc_items) {
        try {
            const Formula formula = parse(item.formula);

            for (const std::string& variable : extract_variables(formula)) {
                if (built_in_variables.find(variable) == built_in_variables.end() &&
                    item_names.find(variable) == item_names.end()) {
                    add_error(
                        errors,
                        "calc_items." + item.code + ".formula",
                        "unknown variable: " + variable);
                }
            }
        } catch (const CalcError& error) {
            add_error(
                errors,
                "calc_items." + item.code + ".formula",
                error.what());
        }
    }
}

}

std::vector<ValidationError> validate(const CalcInput& input) {
    std::vector<ValidationError> errors;

    if (input.period.date_from > input.period.date_to) {
        add_error(
            errors,
            "period",
            "date_from must not be later than date_to");
    }

    std::set<int64_t> employee_ids;
    for (const EmployeeSlice& employee : input.employees) {
        if (!employee_ids.insert(employee.employee_id).second) {
            add_error(
                errors,
                "employees",
                "duplicate employee id");
        }

        if (employee.rate.cents < 0) {
            add_error(
                errors,
                "employees.rate",
                "rate must not be negative");
        }
    }

    std::set<int64_t> schedule_ids;
    for (const WorkSchedule& schedule : input.work_schedules) {
        if (!schedule_ids.insert(schedule.id).second) {
            add_error(
                errors,
                "work_schedules",
                "duplicate schedule id");
        }

        try {
            (void)generate_norm_calendar(
                schedule,
                input.period.date_from.year(),
                input.period.date_from.month());
        } catch (const std::exception& error) {
            add_error(errors, "work_schedules", error.what());
        }
    }

    const std::map<int64_t, const WorkSchedule*> schedules =
        schedule_index(input);

    for (const EmployeeSlice& employee : input.employees) {
        if (schedules.find(employee.work_schedule_id) == schedules.end()) {
            add_error(
                errors,
                "employees.work_schedule_id",
                "unknown work schedule");
        }
    }

    std::set<int64_t> preset_ids;
    for (const PayrollPreset& preset : input.payroll_presets) {
        if (!preset_ids.insert(preset.id).second) {
            add_error(errors, "payroll_presets", "duplicate preset id");
        }
    }

    const std::map<int64_t, const PayrollPreset*> presets =
        preset_index(input);

    for (const EmployeeSlice& employee : input.employees) {
        if (employee.payroll_preset_id.has_value() &&
            presets.find(*employee.payroll_preset_id) == presets.end()) {
            add_error(
                errors,
                "employees.payroll_preset_id",
                "unknown payroll preset");
        }
    }

    std::set<int64_t> item_ids;
    std::set<std::string> item_codes;

    for (const CalcItem& item : input.calc_items) {
        if (!item_ids.insert(item.id).second) {
            add_error(errors, "calc_items", "duplicate calc item id");
        }

        if (!item_codes.insert(item.code).second) {
            add_error(errors, "calc_items", "duplicate calc item code");
        }

        if (!role_stage_is_valid(item)) {
            add_error(
                errors,
                "calc_items." + item.code + ".stage",
                "calc item role does not match stage");
        }
    }

    validate_formula_variables(input, errors);

    try {
        (void)sort_calc_items(items_with_builtins(input.calc_items));
    } catch (const CalcError& error) {
        add_error(errors, "calc_items", error.what());
    }

    return errors;
}

CalcOutput calculate(const CalcInput& input) {
    const std::vector<ValidationError> errors = validate(input);

    if (!errors.empty()) {
        std::ostringstream message;
        message << "invalid calculation configuration";

        for (const ValidationError& error : errors) {
            message << "; " << error.field << ": " << error.message;
        }

        throw std::invalid_argument(message.str());
    }

    CalcOutput output;
    output.input_snapshot = input;

    const std::map<int64_t, const WorkSchedule*> schedules =
        schedule_index(input);

    for (const EmployeeSlice& employee : input.employees) {
        Calculation calculation;
        calculation.employee_id = employee.employee_id;
        calculation.payroll_period_id = input.period.id;
        calculation.engine_version = engine_version;

        const std::vector<EmploymentEvent> employee_events = [&]() {
            std::vector<EmploymentEvent> result;
            for (const EmploymentEvent& event : input.employment_events) {
                if (event.employee_id == employee.employee_id) {
                    result.push_back(event);
                }
            }
            return result;
        }();

        const std::optional<EmployeeSlice> resolved =
            resolve_slice(employee_events, input.period.date_from);

        if (!resolved.has_value()) {
            calculation.status = CalcStatus::Incomplete;
            calculation.incomplete_reasons.push_back(
                "employee кадровый срез не определён");
            output.calculations.push_back(calculation);
            continue;
        }

        const auto schedule = schedules.find(resolved->work_schedule_id);
        if (schedule == schedules.end()) {
            calculation.status = CalcStatus::Incomplete;
            calculation.incomplete_reasons.push_back(
                "рабочий график не найден");
            output.calculations.push_back(calculation);
            continue;
        }

        const std::vector<NormDay> month_calendar =
            generate_norm_calendar(
                *schedule->second,
                input.period.date_from.year(),
                input.period.date_from.month());

        std::vector<NormDay> calendar;
        for (const NormDay& day : month_calendar) {
            if (day.date >= input.period.date_from &&
                day.date <= input.period.date_to) {
                calendar.push_back(day);
            }
        }

        std::vector<TimeFact> employee_facts;
        for (const TimeFact& fact : input.time_facts) {
            if (fact.employee_id == employee.employee_id &&
                fact.date >= input.period.date_from &&
                fact.date <= input.period.date_to) {
                employee_facts.push_back(fact);
            }
        }

        const std::vector<DerivedTime> derived =
            derive_time(
                employee.employee_id,
                calendar,
                employee_facts,
                input.absence_kinds);

        int64_t worked_days = 0;
        int64_t norm_days = 0;
        int64_t norm_minutes = 0;
        int64_t worked_minutes = 0;
        int64_t absent_paid = 0;
        int64_t absent_unpaid = 0;
        int64_t absent_no_leave = 0;
        int64_t working_days_in_month = 0;
        int64_t minutes_in_month_norm = 0;

        for (const NormDay& day : month_calendar) {
            if (day.working_day) {
                ++working_days_in_month;
                minutes_in_month_norm += day.normative_minutes;
            }
        }

        std::set<int32_t> fact_dates;
        for (const TimeFact& fact : employee_facts) {
            fact_dates.insert(fact.date.ordinal());
        }

        for (std::size_t index = 0; index < calendar.size(); ++index) {
            const NormDay& day = calendar[index];
            const DerivedTime& value = derived[index];

            if (day.working_day) {
                ++norm_days;
                norm_minutes += day.normative_minutes;
            }

            worked_minutes += value.worked_minutes;
            if (value.worked_minutes > 0) {
                ++worked_days;
            }

            if (value.absence_minutes > 0) {
                int64_t absence_kind = 0;
                bool has_absence_kind = false;

                for (const TimeFact& fact : employee_facts) {
                    if (fact.date == day.date &&
                        fact.absence_kind_id.has_value()) {
                        if (has_absence_kind &&
                            absence_kind != *fact.absence_kind_id) {
                            throw std::invalid_argument(
                                "multiple absence kinds for one date");
                        }

                        absence_kind = *fact.absence_kind_id;
                        has_absence_kind = true;
                    }
                }

                if (!has_absence_kind) {
                    absent_no_leave += value.absence_minutes;
                } else {
                    bool paid = false;
                    for (const AbsenceKind& kind : input.absence_kinds) {
                        if (kind.id == absence_kind) {
                            paid = kind.paid;
                            break;
                        }
                    }

                    if (paid) {
                        absent_paid += value.absence_minutes;
                    } else {
                        absent_unpaid += value.absence_minutes;
                    }
                }
            }

            if (day.working_day &&
                fact_dates.find(day.date.ordinal()) == fact_dates.end()) {
                calculation.status = CalcStatus::Incomplete;
                calculation.incomplete_reasons.push_back(
                    "отсутствует TimeFact за рабочий день");
            }
        }

        Context context;
        context["base_amount"] = money_to_rational(resolved->rate);
        context["worked_days"] = rational_minutes(worked_days);
        context["worked_minutes"] = rational_minutes(worked_minutes);
        context["norm_days"] = rational_minutes(norm_days);
        context["norm_minutes"] = rational_minutes(norm_minutes);
        context["absent_paid"] = rational_minutes(absent_paid);
        context["absent_unpaid"] = rational_minutes(absent_unpaid);
        context["absent_no_leave"] = rational_minutes(absent_no_leave);
        context["days_in_month"] =
            Rational(Date::days_in_month(
                input.period.date_from.year(),
                input.period.date_from.month()));
        context["working_days_in_month"] =
            rational_minutes(working_days_in_month);
        context["minutes_in_month_norm"] =
            rational_minutes(minutes_in_month_norm);
        context["accruals_taxable"] = Rational{};
        context["deductions_pre_tax"] = Rational{};
        context["contrib_base"] = Rational{};
        context["ndfl_base"] = Rational{};

        const std::vector<CalcItem> items =
            sort_selected_items(selected_items(input, *resolved));

        Rational accruals_taxable;
        Rational deductions_pre_tax;
        Rational contrib_base;
        Rational ndfl;
        Rational contributions;
        Rational deductions_stage_3;
        Rational deductions_stage_5;
        Rational accrued_total;

        for (const CalcItem& item : items) {
            const Rational formula_value =
                evaluate(parse(item.formula), context);

            Rational carried_value = formula_value;
            const Money amount = round_rubles_to_money(formula_value);

            if (item.rounding_policy == RoundingPolicy::PerLine) {
                carried_value = money_to_rational(amount);
            }

            context[item.code] = carried_value;

            calculation.lines.push_back({
                item.id,
                item.code,
                item.role,
                item.stage,
                item.priority,
                amount,
                item.formula,
                item.rounding_policy
            });

            switch (item.role) {
            case CalcRole::Accrual:
                accrued_total += carried_value;
                if (item.taxable) {
                    accruals_taxable += carried_value;
                }
                break;
            case CalcRole::PreTaxDeduction:
                deductions_stage_3 += carried_value;
                deductions_pre_tax += carried_value;
                break;
            case CalcRole::IncomeTax:
                ndfl += carried_value;
                break;
            case CalcRole::EmployerContribution:
                contributions += carried_value;
                break;
            case CalcRole::PostTaxDeduction:
                deductions_stage_5 += carried_value;
                break;
            }

            context["accruals_taxable"] = accruals_taxable;
            context["deductions_pre_tax"] = deductions_pre_tax;
            context["contrib_base"] = contrib_base;
            context["ndfl_base"] =
                accruals_taxable - deductions_pre_tax;

            if (item.included_in_contrib_base) {
                contrib_base += carried_value;
                context["contrib_base"] = contrib_base;
            }
        }

        const Rational net =
            accrued_total -
            deductions_stage_3 -
            ndfl -
            deductions_stage_5;

        calculation.total_accrual =
            round_rubles_to_money(accrued_total);
        calculation.total_pre_tax_deduction =
            round_rubles_to_money(deductions_stage_3);
        calculation.income_tax =
            round_rubles_to_money(ndfl);
        calculation.total_employer_contribution =
            round_rubles_to_money(contributions);
        calculation.total_post_tax_deduction =
            round_rubles_to_money(deductions_stage_5);
        calculation.net_amount =
            round_rubles_to_money(net);

        if (net < Rational{}) {
            calculation.status = CalcStatus::NegativeNet;
        }

        output.calculations.push_back(calculation);
    }

    return output;
}

}
