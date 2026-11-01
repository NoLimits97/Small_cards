#ifndef MK_REPOSITORIES_HPP
#define MK_REPOSITORIES_HPP

#include "mk/database.hpp"
#include "mk/engine.hpp"
#include "mk/model.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace mk {

int64_t create_org(Database&, const Org&);
std::optional<Org> read_org(Database&, int64_t);
void update_org(Database&, const Org&);
void delete_org(Database&, int64_t);
std::vector<Org> list_orgs(Database&);

int64_t create_position(Database&, const Position&);
std::optional<Position> read_position(Database&, int64_t);
void update_position(Database&, const Position&);
void delete_position(Database&, int64_t);
std::vector<Position> list_positions(Database&);

int64_t create_work_schedule(Database&, const WorkSchedule&);
std::optional<WorkSchedule> read_work_schedule(Database&, int64_t);
void update_work_schedule(Database&, const WorkSchedule&);
void delete_work_schedule(Database&, int64_t);
std::vector<WorkSchedule> list_work_schedules(Database&);

int64_t create_employee(Database&, const Employee&);
std::optional<Employee> read_employee(Database&, int64_t);
void update_employee(Database&, const Employee&);
void delete_employee(Database&, int64_t);
std::vector<Employee> list_employees(Database&, int64_t org_id);

int64_t create_employment_event(Database&, const EmploymentEvent&);
std::optional<EmploymentEvent> read_employment_event(Database&, int64_t);
void update_employment_event(Database&, const EmploymentEvent&);
void delete_employment_event(Database&, int64_t);
std::vector<EmploymentEvent> list_employment_events(Database&, int64_t);

int64_t create_time_fact(Database&, const TimeFact&);
std::optional<TimeFact> read_time_fact(Database&, int64_t);
void update_time_fact(Database&, const TimeFact&);
void delete_time_fact(Database&, int64_t);
std::vector<TimeFact> list_time_facts(
    Database&, int64_t employee_id, Date from, Date to);

std::vector<AbsenceKind> list_absence_kinds(Database&);
std::vector<CalcItem> list_calc_items(Database&);
std::vector<PayrollPreset> list_payroll_presets(Database&);
std::vector<WorkSchedule> list_work_schedules(Database&, int64_t org_id);

CalcInput build_calc_input(
    Database&, int64_t org_id, int32_t year, int32_t month);

void persist_calculation(Database&, const CalcOutput&);
std::vector<CalculationLine> read_calculation_lines(
    Database&, int64_t calculation_id);

}

#endif
