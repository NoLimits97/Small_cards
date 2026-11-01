#ifndef MK_MODEL_HPP
#define MK_MODEL_HPP

#include "mk/types.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace mk {

enum class RateUnit : int32_t {
    Month = 1,
    Day = 2,
    Hour = 3
};

enum class EventKind : int32_t {
    Hire = 1,
    Transfer = 2,
    ChangeSalary = 3,
    Termination = 4
};

enum class ScheduleKind : int32_t {
    FiveDay = 1,
    Shift = 2
};

enum class PeriodStatus : int32_t {
    Draft = 1,
    Closed = 2
};

enum class CalcStatus : int32_t {
    Ok = 1,
    Incomplete = 2,
    NegativeNet = 3
};

enum class PayChannel : int32_t {
    Bank = 1,
    Cash = 2
};

enum class CalcRole : int32_t {
    Accrual = 1,
    PreTaxDeduction = 2,
    IncomeTax = 3,
    EmployerContribution = 4,
    PostTaxDeduction = 5
};

enum class RoundingPolicy : int32_t {
    PerLine = 1,
    Carry = 2
};

struct Org {
    int64_t id = 0;
    std::string name;
};

struct Department {
    int64_t id = 0;
    int64_t org_id = 0;
    std::string name;
};

struct Position {
    int64_t id = 0;
    std::string name;
};

struct EmploymentKind {
    int64_t id = 0;
    std::string code;
    std::string name;
};

struct WorkSchedule {
    int64_t id = 0;
    std::string name;
    ScheduleKind kind = ScheduleKind::FiveDay;
    int32_t normative_minutes_per_day = 0;
    std::string shift_cycle;
    std::optional<Date> anchor_date;
};

struct AbsenceKind {
    int64_t id = 0;
    std::string code;
    std::string name;
    bool paid = false;
};

struct TerminationReason {
    int64_t id = 0;
    std::string code;
    std::string name;
};

struct CalcItem {
    int64_t id = 0;
    std::string code;
    std::string name;
    std::string formula;
    CalcRole role = CalcRole::Accrual;
    int32_t stage = 0;
    int32_t priority = 0;
    bool taxable = false;
    bool included_in_contrib_base = false;
    bool reduces_income_tax_base = false;
    RoundingPolicy rounding_policy = RoundingPolicy::PerLine;
};

struct PayrollPreset {
    int64_t id = 0;
    std::string code;
    std::string name;
    std::vector<int64_t> calc_item_ids;
};

struct Employee {
    int64_t id = 0;
    std::string personnel_number;
    std::string full_name;
    int64_t org_id = 0;
    int64_t department_id = 0;
    int64_t position_id = 0;
    int64_t employment_kind_id = 0;
    int64_t work_schedule_id = 0;
    std::optional<int64_t> payroll_preset_id;
    Money rate;
    RateUnit rate_unit = RateUnit::Month;
    Date hire_date = Date(1, 1, 1);
    std::optional<Date> termination_date;
};

struct EmploymentEvent {
    int64_t employee_id = 0;
    Date date = Date(1, 1, 1);
    EventKind kind = EventKind::Hire;

    std::optional<std::string> personnel_number;
    std::optional<std::string> full_name;
    std::optional<int64_t> org_id;
    std::optional<int64_t> department_id;
    std::optional<int64_t> position_id;
    std::optional<int64_t> employment_kind_id;
    std::optional<int64_t> work_schedule_id;
    std::optional<int64_t> payroll_preset_id;
    std::optional<Money> rate;
    std::optional<RateUnit> rate_unit;
    std::optional<int64_t> termination_reason_id;
};

struct TimeFact {
    int64_t employee_id = 0;
    Date date = Date(1, 1, 1);
    int32_t worked_minutes = 0;
    std::optional<int64_t> absence_kind_id;
};

struct OutputFact {
    int64_t employee_id = 0;
    Date date = Date(1, 1, 1);
    std::string code;
    Rational value;
};

struct NormDay {
    Date date = Date(1, 1, 1);
    int32_t normative_minutes = 0;
    bool working_day = false;
};

struct DerivedTime {
    int64_t employee_id = 0;
    Date date = Date(1, 1, 1);
    int32_t normative_minutes = 0;
    int32_t worked_minutes = 0;
    int32_t absence_minutes = 0;
};

struct PayrollPeriod {
    int64_t id = 0;
    Date date_from = Date(1, 1, 1);
    Date date_to = Date(1, 1, 1);
    PeriodStatus status = PeriodStatus::Draft;
};

struct CalculationLine {
    int64_t calc_item_id = 0;
    std::string calc_item_code;
    CalcRole role = CalcRole::Accrual;
    int32_t stage = 0;
    int32_t priority = 0;
    Money amount;
    std::string formula_snapshot;
    RoundingPolicy rounding_policy = RoundingPolicy::PerLine;
};

struct Calculation {
    int64_t id = 0;
    int64_t employee_id = 0;
    int64_t payroll_period_id = 0;
    CalcStatus status = CalcStatus::Ok;
    std::vector<std::string> incomplete_reasons;
    Money total_accrual;
    Money total_pre_tax_deduction;
    Money income_tax;
    Money total_employer_contribution;
    Money total_post_tax_deduction;
    Money net_amount;
    std::string engine_version;
    std::vector<CalculationLine> lines;
    std::optional<std::string> input_snapshot;
};

struct PaymentDoc {
    int64_t id = 0;
    int64_t employee_id = 0;
    int64_t payroll_period_id = 0;
    Date date = Date(1, 1, 1);
    PayChannel channel = PayChannel::Bank;
    Money amount;
};

struct PaymentLine {
    int64_t payment_doc_id = 0;
    int64_t calculation_id = 0;
    Money amount;
};

struct EmployeeSlice {
    int64_t employee_id = 0;
    std::string personnel_number;
    std::string full_name;
    int64_t org_id = 0;
    int64_t department_id = 0;
    int64_t position_id = 0;
    int64_t employment_kind_id = 0;
    int64_t work_schedule_id = 0;
    std::optional<int64_t> payroll_preset_id;
    Money rate;
    RateUnit rate_unit = RateUnit::Month;
};

struct EmployeeCalcUnit {
    EmployeeSlice employee;
    std::vector<EmploymentEvent> events;
    std::vector<TimeFact> time_facts;
    std::vector<DerivedTime> derived_time;
    std::vector<OutputFact> output_facts;
};

struct CalcInput {
    PayrollPeriod period;
    std::vector<EmployeeSlice> employees;
    std::vector<EmploymentEvent> employment_events;
    std::vector<WorkSchedule> work_schedules;
    std::vector<PayrollPreset> payroll_presets;
    std::vector<CalcItem> calc_items;
    std::vector<AbsenceKind> absence_kinds;
    std::vector<TerminationReason> termination_reasons;
    std::vector<TimeFact> time_facts;
    std::vector<NormDay> norm_days;
    std::vector<DerivedTime> derived_time;
    std::vector<OutputFact> output_facts;
};

struct CalcOutput {
    CalcInput input_snapshot;
    std::vector<Calculation> calculations;
};

struct ValidationError {
    std::string field;
    std::string message;
};

}

#endif
