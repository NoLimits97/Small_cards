#ifndef MK_PAYROLL_SERVICE_HPP
#define MK_PAYROLL_SERVICE_HPP

#include "mk/database.hpp"
#include "mk/engine.hpp"
#include "mk/model.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace mk {

/*
 * Норма рабочего времени по одному графику за выбранный месяц.
 */
struct PayrollNormInfo {
    int64_t schedule_id = 0;
    std::string schedule_name;
    int32_t working_days = 0;
    int32_t norm_minutes = 0;
    int32_t employee_count = 0;
};

/*
 * Результат расчёта по одному сотруднику.
 *
 * calculated == false означает, что сумму показывать нельзя: не хватает
 * данных или формула не поддерживает параметры сотрудника. В reasons
 * перечислены причины.
 */
struct PayrollRow {
    int64_t employee_id = 0;
    std::string personnel_number;
    std::string full_name;
    std::string position_name;
    RateUnit rate_unit = RateUnit::Month;
    bool has_time_facts = false;
    int64_t worked_days = 0;
    int64_t worked_minutes = 0;
    bool calculated = false;
    CalcStatus status = CalcStatus::Ok;
    std::vector<std::string> reasons;
    Money accrual;
    Money pre_tax_deduction;
    Money income_tax;
    Money post_tax_deduction;
    Money deductions;
    Money net;
};

struct PayrollReport {
    int64_t org_id = 0;
    int32_t year = 0;
    int32_t month = 0;
    Date date_from = Date(1, 1, 1);
    Date date_to = Date(1, 1, 1);
    bool has_timesheet = false;
    std::vector<PayrollNormInfo> norms;
    std::vector<PayrollRow> rows;
    std::vector<std::string> warnings;

    // Исходные данные и строки расчёта движка. Нужны для сохранения
    // результата через storage API.
    CalcOutput output;
};

/*
 * Нормы рабочего времени за месяц в разрезе графиков сотрудников
 * организации. Используется для отображения параметров расчёта.
 */
std::vector<PayrollNormInfo> payroll_norms(
    Database& database,
    int64_t org_id,
    int32_t year,
    int32_t month);

/*
 * Есть ли в табеле организации хотя бы одна запись за выбранный месяц.
 */
bool payroll_has_timesheet(
    Database& database,
    int64_t org_id,
    int32_t year,
    int32_t month);

/*
 * Полный расчёт за месяц: табель, кадровый срез и настроенные в базе
 * начисления. Не содержит SQL; использует storage-репозитории и
 * core-движок.
 */
PayrollReport run_payroll(
    Database& database,
    int64_t org_id,
    int32_t year,
    int32_t month);

/*
 * Сохраняет рассчитанные строки за период. Повторное сохранение за тот
 * же период заменяет предыдущий результат, а не создаёт дубликаты.
 * Строки, которые не были рассчитаны (calculated == false), не
 * сохраняются.
 */
void save_payroll(Database& database, const PayrollReport& report);

}

#endif
