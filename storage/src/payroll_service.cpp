#include "mk/payroll_service.hpp"

#include "mk/calendar.hpp"
#include "mk/repositories.hpp"
#include "mk/slicing.hpp"
#include "mk/time_derive.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>
#include <utility>

namespace mk {
namespace {

const WorkSchedule* find_schedule(
    const std::vector<WorkSchedule>& schedules,
    int64_t schedule_id) {
    const auto value = std::find_if(
        schedules.begin(),
        schedules.end(),
        [schedule_id](const WorkSchedule& schedule) {
            return schedule.id == schedule_id;
        });
    return value == schedules.end() ? nullptr : &*value;
}

const PayrollPreset* find_preset(
    const std::vector<PayrollPreset>& presets,
    int64_t preset_id) {
    const auto value = std::find_if(
        presets.begin(),
        presets.end(),
        [preset_id](const PayrollPreset& preset) {
            return preset.id == preset_id;
        });
    return value == presets.end() ? nullptr : &*value;
}

std::string position_name(
    const std::vector<Position>& positions,
    int64_t position_id) {
    const auto value = std::find_if(
        positions.begin(),
        positions.end(),
        [position_id](const Position& position) {
            return position.id == position_id;
        });
    return value == positions.end()
        ? std::string()
        : value->name;
}

void append_unique(
    std::vector<std::string>& target,
    const std::string& value) {
    if (std::find(target.begin(), target.end(), value) == target.end()) {
        target.push_back(value);
    }
}

EmploymentEvent hire_event_from_employee(const Employee& employee) {
    EmploymentEvent event;
    event.employee_id = employee.id;
    event.date = employee.hire_date;
    event.kind = EventKind::Hire;
    event.personnel_number = employee.personnel_number;
    event.full_name = employee.full_name;
    event.org_id = employee.org_id;
    event.department_id = employee.department_id;
    event.position_id = employee.position_id;
    event.employment_kind_id = employee.employment_kind_id;
    event.work_schedule_id = employee.work_schedule_id;
    event.payroll_preset_id = employee.payroll_preset_id;
    event.rate = employee.rate;
    event.rate_unit = employee.rate_unit;
    return event;
}

struct WorkStats {
    int64_t worked_days = 0;
    int64_t worked_minutes = 0;
};

WorkStats work_stats(
    const Employee& employee,
    const std::vector<EmploymentEvent>& events,
    const WorkSchedule& schedule,
    const std::vector<TimeFact>& time_facts,
    const std::vector<AbsenceKind>& absence_kinds,
    int32_t year,
    int32_t month,
    const Date& date_from) {
    WorkStats result;

    const std::optional<EmployeeSlice> slice =
        resolve_slice(events, date_from);
    const int64_t schedule_id = slice.has_value()
        ? slice->work_schedule_id
        : schedule.id;
    if (schedule_id != schedule.id) {
        return result;
    }

    std::vector<TimeFact> employee_facts;
    for (const TimeFact& fact : time_facts) {
        if (fact.employee_id == employee.id) {
            employee_facts.push_back(fact);
        }
    }

    const std::vector<NormDay> calendar =
        generate_norm_calendar(schedule, year, month);
    const std::vector<DerivedTime> derived =
        derive_time(
            employee.id,
            calendar,
            employee_facts,
            absence_kinds);

    for (const DerivedTime& value : derived) {
        result.worked_minutes += value.worked_minutes;
        if (value.worked_minutes > 0) {
            ++result.worked_days;
        }
    }

    return result;
}

std::vector<EmploymentEvent> events_for(
    const std::vector<EmploymentEvent>& events,
    int64_t employee_id) {
    std::vector<EmploymentEvent> result;
    for (const EmploymentEvent& event : events) {
        if (event.employee_id == employee_id) {
            result.push_back(event);
        }
    }
    return result;
}

std::vector<std::string> unique_reasons(
    const std::vector<std::string>& reasons) {
    std::vector<std::string> result;
    for (const std::string& reason : reasons) {
        append_unique(result, reason);
    }
    return result;
}

}

std::vector<PayrollNormInfo> payroll_norms(
    Database& database,
    int64_t org_id,
    int32_t year,
    int32_t month) {
    const std::vector<Employee> employees =
        list_employees(database, org_id);
    const std::vector<WorkSchedule> schedules =
        list_work_schedules(database);

    std::map<int64_t, PayrollNormInfo> grouped;

    for (const Employee& employee : employees) {
        const WorkSchedule* schedule = find_schedule(
            schedules, employee.work_schedule_id);
        if (schedule == nullptr) {
            continue;
        }

        auto value = grouped.find(schedule->id);
        if (value == grouped.end()) {
            PayrollNormInfo info;
            info.schedule_id = schedule->id;
            info.schedule_name = schedule->name;

            const std::vector<NormDay> calendar =
                generate_norm_calendar(*schedule, year, month);
            for (const NormDay& day : calendar) {
                if (day.working_day) {
                    ++info.working_days;
                    info.norm_minutes += day.normative_minutes;
                }
            }

            value = grouped.emplace(schedule->id, info).first;
        }

        ++value->second.employee_count;
    }

    std::vector<PayrollNormInfo> result;
    result.reserve(grouped.size());
    for (const auto& value : grouped) {
        result.push_back(value.second);
    }
    return result;
}

bool payroll_has_timesheet(
    Database& database,
    int64_t org_id,
    int32_t year,
    int32_t month) {
    const Date from(year, month, 1);
    const Date to(
        year,
        month,
        Date::days_in_month(year, month));

    for (const Employee& employee :
         list_employees(database, org_id)) {
        if (!list_time_facts(
                database,
                employee.id,
                from,
                to)
                 .empty()) {
            return true;
        }
    }

    return false;
}

PayrollReport run_payroll(
    Database& database,
    int64_t org_id,
    int32_t year,
    int32_t month) {
    PayrollReport report;
    report.org_id = org_id;
    report.year = year;
    report.month = month;
    report.date_from = Date(year, month, 1);
    report.date_to = Date(
        year,
        month,
        Date::days_in_month(year, month));

    const std::vector<Employee> employees =
        list_employees(database, org_id);
    const std::vector<Position> positions =
        list_positions(database);

    CalcInput input =
        build_calc_input(database, org_id, year, month);
    report.has_timesheet = !input.time_facts.empty();
    report.norms =
        payroll_norms(database, org_id, year, month);

    std::set<int64_t> employees_with_events;
    for (const EmploymentEvent& event : input.employment_events) {
        employees_with_events.insert(event.employee_id);
    }

    // Сотрудники, созданные без кадрового события, получают срез из
    // текущей карточки сотрудника: другого источника данных в проекте нет.
    for (const Employee& employee : employees) {
        if (employees_with_events.find(employee.id) ==
            employees_with_events.end()) {
            input.employment_events.push_back(
                hire_event_from_employee(employee));
        }
    }

    std::map<int64_t, std::string> unsupported;
    for (const Employee& employee : employees) {
        if (employee.termination_date.has_value() &&
            *employee.termination_date < report.date_from) {
            unsupported[employee.id] = "уволен до начала расчётного периода";
        } else if (employee.termination_date.has_value() &&
                   *employee.termination_date <= report.date_to) {
            unsupported[employee.id] =
                "уволен в течение месяца — неполный месяц "
                "не рассчитывается";
        } else if (employee.rate.cents <= 0) {
            unsupported[employee.id] = "не задана ставка (оклад)";
        } else if (employee.rate_unit != RateUnit::Month) {
            unsupported[employee.id] =
                "ставка за день или час не поддерживается формулой оклада";
        } else if (employee.hire_date > report.date_from) {
            unsupported[employee.id] =
                "принят не с начала месяца — неполный месяц "
                "не рассчитывается";
        } else if (find_schedule(
                       input.work_schedules,
                       employee.work_schedule_id) == nullptr) {
            unsupported[employee.id] = "график работы не найден";
        } else if (employee.payroll_preset_id.has_value() &&
                   find_preset(
                       input.payroll_presets,
                       *employee.payroll_preset_id) == nullptr) {
            unsupported[employee.id] = "набор начислений не найден";
        }
    }

    input.employees.erase(
        std::remove_if(
            input.employees.begin(),
            input.employees.end(),
            [&unsupported](const EmployeeSlice& employee) {
                return unsupported.find(employee.employee_id) !=
                    unsupported.end();
            }),
        input.employees.end());

    CalcOutput output;
    output.input_snapshot = input;
    if (!input.employees.empty()) {
        output = calculate(input);
    }
    report.output = output;

    if (input.calc_items.empty()) {
        report.warnings.push_back(
            "в базе не настроены начисления");
    }

    std::map<int64_t, const Calculation*> calculations;
    for (const Calculation& calculation : output.calculations) {
        calculations[calculation.employee_id] = &calculation;
    }

    for (const Employee& employee : employees) {
        PayrollRow row;
        row.employee_id = employee.id;
        row.personnel_number = employee.personnel_number;
        row.full_name = employee.full_name;
        row.position_name =
            position_name(positions, employee.position_id);
        row.rate_unit = employee.rate_unit;

        const std::vector<EmploymentEvent> employee_events =
            events_for(input.employment_events, employee.id);

        std::optional<EmployeeSlice> slice =
            resolve_slice(employee_events, report.date_from);
        int64_t schedule_id = employee.work_schedule_id;
        if (slice.has_value()) {
            schedule_id = slice->work_schedule_id;
        }

        row.has_time_facts = std::any_of(
            input.time_facts.begin(),
            input.time_facts.end(),
            [employee](const TimeFact& fact) {
                return fact.employee_id == employee.id;
            });

        const WorkSchedule* schedule =
            find_schedule(input.work_schedules, schedule_id);
        if (schedule != nullptr) {
            const WorkStats stats = work_stats(
                employee,
                employee_events,
                *schedule,
                input.time_facts,
                input.absence_kinds,
                year,
                month,
                report.date_from);
            row.worked_days = stats.worked_days;
            row.worked_minutes = stats.worked_minutes;
        }

        const auto unsupported_reason = unsupported.find(employee.id);
        if (unsupported_reason != unsupported.end()) {
            row.status = CalcStatus::Incomplete;
            row.reasons.push_back(unsupported_reason->second);
            report.rows.push_back(std::move(row));
            continue;
        }

        const auto calculation = calculations.find(employee.id);
        if (calculation == calculations.end()) {
            row.status = CalcStatus::Incomplete;
            row.reasons.push_back("расчёт не выполнен");
            report.rows.push_back(std::move(row));
            continue;
        }

        const Calculation& value = *calculation->second;
        row.status = value.status;
        row.reasons = unique_reasons(value.incomplete_reasons);
        row.accrual = value.total_accrual;
        row.pre_tax_deduction =
            value.total_pre_tax_deduction;
        row.income_tax = value.income_tax;
        row.post_tax_deduction =
            value.total_post_tax_deduction;
        row.deductions =
            value.total_pre_tax_deduction +
            value.income_tax +
            value.total_post_tax_deduction;
        row.net = value.net_amount;

        row.calculated =
            value.status != CalcStatus::Incomplete &&
            !value.lines.empty();

        if (value.lines.empty()) {
            append_unique(row.reasons, "начисления не настроены");
        }

        report.rows.push_back(std::move(row));
    }

    return report;
}

void save_payroll(Database& database, const PayrollReport& report) {
    std::set<int64_t> calculated_ids;
    for (const PayrollRow& row : report.rows) {
        if (row.calculated) {
            calculated_ids.insert(row.employee_id);
        }
    }

    if (calculated_ids.empty()) {
        throw std::invalid_argument(
            "нет рассчитанных сотрудников для сохранения");
    }

    CalcOutput output;
    output.input_snapshot = report.output.input_snapshot;
    for (const Calculation& calculation :
         report.output.calculations) {
        if (calculated_ids.find(calculation.employee_id) !=
            calculated_ids.end()) {
            output.calculations.push_back(calculation);
        }
    }

    if (output.calculations.empty()) {
        throw std::invalid_argument(
            "нет рассчитанных сотрудников для сохранения");
    }

    save_calculation(database, output);
}

}
