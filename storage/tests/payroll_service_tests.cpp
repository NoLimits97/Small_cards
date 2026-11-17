#include "test_framework.hpp"

#include "mk/calendar.hpp"
#include "mk/payroll_service.hpp"
#include "mk/repositories.hpp"

#include <QSqlQuery>
#include <QTemporaryDir>

#include <cstdint>
#include <string>
#include <vector>

namespace {

mk::Employee make_employee(const std::string& number) {
    mk::Employee employee;
    employee.personnel_number = number;
    employee.full_name = "Тестовый сотрудник";
    employee.org_id = 1;
    employee.department_id = 1;
    employee.position_id = 1;
    employee.employment_kind_id = 1;
    employee.work_schedule_id = 1;
    employee.payroll_preset_id = 1;
    employee.rate = mk::Money(10000000);
    employee.rate_unit = mk::RateUnit::Month;
    employee.hire_date = mk::Date(2026, 1, 1);
    return employee;
}

void fill_month(
    mk::Database& database,
    int64_t employee_id,
    int year,
    int month,
    int64_t worked_days,
    int32_t full_day_minutes = 480) {
    const auto schedule = mk::read_work_schedule(database, 1);
    REQUIRE(schedule.has_value());

    const std::vector<mk::NormDay> calendar =
        mk::generate_norm_calendar(*schedule, year, month);

    int64_t working_index = 0;
    for (const mk::NormDay& day : calendar) {
        if (!day.working_day) {
            continue;
        }

        mk::TimeFact fact;
        fact.employee_id = employee_id;
        fact.date = day.date;
        fact.worked_minutes = worked_days < 0 ||
                working_index < worked_days
            ? full_day_minutes
            : 0;
        (void)mk::create_time_fact(database, fact);
        ++working_index;
    }
}

bool has_reason(const mk::PayrollRow& row, const std::string& part) {
    for (const std::string& reason : row.reasons) {
        if (reason.find(part) != std::string::npos) {
            return true;
        }
    }
    return false;
}

int64_t count_rows(mk::Database& database, const char* table) {
    QSqlQuery query(database.sql());
    if (!query.exec(
            QStringLiteral("SELECT COUNT(*) FROM %1")
                .arg(QString::fromUtf8(table)))) {
        throw std::runtime_error("count query failed");
    }
    if (!query.next()) {
        throw std::runtime_error("count query returned no rows");
    }
    return query.value(0).toLongLong();
}

}

TEST(storage_payroll_calculates_full_month) {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    mk::Database database;
    database.create_new(directory.filePath("payroll_full.sqlite"));

    const int64_t employee_id = mk::create_employee(
        database, make_employee("0200"));
    fill_month(database, employee_id, 2026, 1, -1);

    const mk::PayrollReport report =
        mk::run_payroll(database, 1, 2026, 1);
    REQUIRE(report.has_timesheet);
    REQUIRE_EQUAL(report.rows.size(), std::size_t{1});
    REQUIRE(report.rows[0].calculated);
    REQUIRE_EQUAL(report.rows[0].worked_days, int64_t{22});
    REQUIRE_EQUAL(report.rows[0].worked_minutes, int64_t{10560});
    REQUIRE_EQUAL(report.rows[0].accrual.cents, int64_t{10000000});
    REQUIRE_EQUAL(report.rows[0].deductions.cents, int64_t{0});
    REQUIRE_EQUAL(report.rows[0].net.cents, int64_t{10000000});

    REQUIRE_EQUAL(report.norms.size(), std::size_t{1});
    REQUIRE_EQUAL(report.norms[0].schedule_id, int64_t{1});
    REQUIRE_EQUAL(report.norms[0].working_days, int32_t{22});
    REQUIRE_EQUAL(report.norms[0].norm_minutes, int32_t{10560});
    REQUIRE_EQUAL(report.norms[0].employee_count, int32_t{1});
}

TEST(storage_payroll_calculates_partial_month) {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    mk::Database database;
    database.create_new(directory.filePath("payroll_partial.sqlite"));

    const int64_t employee_id = mk::create_employee(
        database, make_employee("0201"));
    fill_month(database, employee_id, 2026, 1, 15);

    const mk::PayrollReport report =
        mk::run_payroll(database, 1, 2026, 1);
    REQUIRE_EQUAL(report.rows.size(), std::size_t{1});
    REQUIRE(report.rows[0].calculated);
    REQUIRE_EQUAL(report.rows[0].worked_days, int64_t{15});
    REQUIRE_EQUAL(report.rows[0].worked_minutes, int64_t{7200});
    REQUIRE_EQUAL(report.rows[0].accrual.cents, int64_t{6818182});
    REQUIRE_EQUAL(report.rows[0].net.cents, int64_t{6818182});
}

TEST(storage_payroll_calculates_zero_hours) {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    mk::Database database;
    database.create_new(directory.filePath("payroll_zero.sqlite"));

    const int64_t employee_id = mk::create_employee(
        database, make_employee("0202"));
    fill_month(database, employee_id, 2026, 1, 0);

    const mk::PayrollReport report =
        mk::run_payroll(database, 1, 2026, 1);
    REQUIRE_EQUAL(report.rows.size(), std::size_t{1});
    REQUIRE(report.rows[0].calculated);
    REQUIRE_EQUAL(report.rows[0].worked_days, int64_t{0});
    REQUIRE_EQUAL(report.rows[0].worked_minutes, int64_t{0});
    REQUIRE_EQUAL(report.rows[0].accrual.cents, int64_t{0});
    REQUIRE_EQUAL(report.rows[0].net.cents, int64_t{0});
}

TEST(storage_payroll_does_not_invent_amount_without_rate) {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    mk::Database database;
    database.create_new(directory.filePath("payroll_no_rate.sqlite"));

    mk::Employee employee = make_employee("0203");
    employee.rate = mk::Money(0);
    const int64_t employee_id =
        mk::create_employee(database, employee);
    fill_month(database, employee_id, 2026, 1, -1);

    const mk::PayrollReport report =
        mk::run_payroll(database, 1, 2026, 1);
    REQUIRE_EQUAL(report.rows.size(), std::size_t{1});
    REQUIRE(!report.rows[0].calculated);
    REQUIRE(has_reason(report.rows[0], "ставка"));
    REQUIRE_EQUAL(report.rows[0].accrual.cents, int64_t{0});
    REQUIRE_EQUAL(report.rows[0].net.cents, int64_t{0});
}

TEST(storage_payroll_reports_mid_month_hire) {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    mk::Database database;
    database.create_new(directory.filePath("payroll_mid_month.sqlite"));

    mk::Employee employee = make_employee("0204");
    employee.hire_date = mk::Date(2026, 1, 10);
    const int64_t employee_id =
        mk::create_employee(database, employee);
    fill_month(database, employee_id, 2026, 1, -1);

    const mk::PayrollReport report =
        mk::run_payroll(database, 1, 2026, 1);
    REQUIRE_EQUAL(report.rows.size(), std::size_t{1});
    REQUIRE(!report.rows[0].calculated);
    REQUIRE(has_reason(report.rows[0], "не с начала месяца"));
    REQUIRE_EQUAL(report.rows[0].accrual.cents, int64_t{0});
}

TEST(storage_payroll_rejects_hourly_rate) {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    mk::Database database;
    database.create_new(directory.filePath("payroll_hourly.sqlite"));

    mk::Employee employee = make_employee("0205");
    employee.rate_unit = mk::RateUnit::Hour;
    const int64_t employee_id =
        mk::create_employee(database, employee);
    fill_month(database, employee_id, 2026, 1, -1);

    const mk::PayrollReport report =
        mk::run_payroll(database, 1, 2026, 1);
    REQUIRE_EQUAL(report.rows.size(), std::size_t{1});
    REQUIRE(!report.rows[0].calculated);
    REQUIRE(has_reason(report.rows[0], "не поддерживается"));
}

TEST(storage_payroll_without_timesheet_stays_uncalculated) {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    mk::Database database;
    database.create_new(directory.filePath("payroll_no_timesheet.sqlite"));

    (void)mk::create_employee(database, make_employee("0206"));

    REQUIRE(!mk::payroll_has_timesheet(database, 1, 2026, 1));

    const mk::PayrollReport report =
        mk::run_payroll(database, 1, 2026, 1);
    REQUIRE(!report.has_timesheet);
    REQUIRE_EQUAL(report.rows.size(), std::size_t{1});
    REQUIRE(!report.rows[0].calculated);
    REQUIRE(!report.rows[0].reasons.empty());
    REQUIRE_EQUAL(report.rows[0].accrual.cents, int64_t{0});
    REQUIRE_EQUAL(report.rows[0].net.cents, int64_t{0});
}

TEST(storage_payroll_handles_empty_employee_list) {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    mk::Database database;
    database.create_new(directory.filePath("payroll_empty.sqlite"));

    const mk::PayrollReport report =
        mk::run_payroll(database, 1, 2026, 1);
    REQUIRE(report.rows.empty());
    REQUIRE(report.norms.empty());
    REQUIRE(!report.has_timesheet);
}

TEST(storage_payroll_repeat_calculation_and_save_are_idempotent) {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    mk::Database database;
    database.create_new(directory.filePath("payroll_repeat.sqlite"));

    const int64_t employee_id = mk::create_employee(
        database, make_employee("0207"));
    fill_month(database, employee_id, 2026, 1, -1);

    const mk::PayrollReport first =
        mk::run_payroll(database, 1, 2026, 1);
    const mk::PayrollReport second =
        mk::run_payroll(database, 1, 2026, 1);
    REQUIRE_EQUAL(first.rows.size(), second.rows.size());
    REQUIRE_EQUAL(
        first.rows[0].accrual.cents,
        second.rows[0].accrual.cents);

    mk::save_payroll(database, first);
    mk::save_payroll(database, second);

    REQUIRE_EQUAL(count_rows(database, "PayrollPeriod"), int64_t{1});
    REQUIRE_EQUAL(count_rows(database, "Calculation"), int64_t{1});
    REQUIRE_EQUAL(count_rows(database, "CalculationLine"), int64_t{1});
}

TEST(storage_payroll_does_not_save_uncalculated_rows) {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    mk::Database database;
    database.create_new(directory.filePath("payroll_save_none.sqlite"));

    (void)mk::create_employee(database, make_employee("0208"));
    const mk::PayrollReport report =
        mk::run_payroll(database, 1, 2026, 1);

    REQUIRE_THROWS(mk::save_payroll(database, report));
    REQUIRE_EQUAL(count_rows(database, "PayrollPeriod"), int64_t{0});
}

TEST(storage_payroll_requires_open_database) {
    mk::Database database;
    REQUIRE_THROWS(mk::run_payroll(database, 1, 2026, 1));
    REQUIRE_THROWS(mk::payroll_has_timesheet(database, 1, 2026, 1));
}
