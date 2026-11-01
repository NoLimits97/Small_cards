#include "test_framework.hpp"

#include "mk/repositories.hpp"

#include <QCoreApplication>
#include <QSqlQuery>
#include <QTemporaryDir>

#include <cstdio>

namespace {

mk::Database make_database(QTemporaryDir& directory) {
    mk::Database database;
    database.create_new(directory.filePath("storage.sqlite"));
    return database;
}

mk::Employee employee() {
    mk::Employee result;
    result.personnel_number = "001";
    result.full_name = "Иванов Иван";
    result.org_id = 1;
    result.department_id = 1;
    result.position_id = 1;
    result.employment_kind_id = 1;
    result.work_schedule_id = 1;
    result.payroll_preset_id = 1;
    result.rate = mk::Money(100000);
    result.rate_unit = mk::RateUnit::Month;
    result.hire_date = mk::Date(2026, 1, 1);
    return result;
}

}

TEST(storage_create_database) {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    mk::Database database;
    database.create_new(directory.filePath("database.sqlite"));

    REQUIRE(database.sql().isOpen());
    database.close();
}

TEST(storage_employee_crud) {
    QTemporaryDir directory;
    mk::Database database = make_database(directory);

    const int64_t employee_id =
        mk::create_employee(database, employee());

    const auto loaded = mk::read_employee(database, employee_id);
    REQUIRE(loaded.has_value());
    REQUIRE_EQUAL(loaded->full_name, std::string("Иванов Иван"));

    mk::Employee changed = *loaded;
    changed.full_name = "Петров Пётр";
    mk::update_employee(database, changed);

    REQUIRE_EQUAL(
        mk::read_employee(database, employee_id)->full_name,
        std::string("Петров Пётр"));

    mk::delete_employee(database, employee_id);
    REQUIRE(!mk::read_employee(database, employee_id).has_value());
}

TEST(storage_event_and_time_fact) {
    QTemporaryDir directory;
    mk::Database database = make_database(directory);

    const int64_t employee_id =
        mk::create_employee(database, employee());

    mk::EmploymentEvent event;
    event.employee_id = employee_id;
    event.date = mk::Date(2025, 12, 1);
    event.kind = mk::EventKind::Hire;
    event.position_id = 1;
    event.department_id = 1;
    event.work_schedule_id = 1;
    event.rate = mk::Money(100000);
    event.rate_unit = mk::RateUnit::Month;

    const int64_t event_id =
        mk::create_employment_event(database, event);
    REQUIRE(mk::read_employment_event(database, event_id).has_value());

    mk::TimeFact fact;
    fact.employee_id = employee_id;
    fact.date = mk::Date(2026, 1, 12);
    fact.worked_minutes = 480;

    const int64_t fact_id = mk::create_time_fact(database, fact);
    const auto facts = mk::list_time_facts(
        database,
        employee_id,
        mk::Date(2026, 1, 1),
        mk::Date(2026, 1, 31));

    REQUIRE_EQUAL(facts.size(), std::size_t{1});
    REQUIRE_EQUAL(facts.front().worked_minutes, int32_t{480});

    mk::delete_time_fact(database, fact_id);
}

TEST(storage_build_input) {
    QTemporaryDir directory;
    mk::Database database = make_database(directory);

    mk::create_employee(database, employee());

    const mk::CalcInput input =
        mk::build_calc_input(database, 1, 2026, 1);

    REQUIRE_EQUAL(input.employees.size(), std::size_t{1});
    REQUIRE_EQUAL(input.calc_items.size(), std::size_t{1});
    REQUIRE_EQUAL(input.payroll_presets.size(), std::size_t{1});
    REQUIRE_EQUAL(input.norm_days.size(), std::size_t{31});
}

TEST(storage_persist_calculation_and_lines) {
    QTemporaryDir directory;
    mk::Database database = make_database(directory);

    const int64_t employee_id =
        mk::create_employee(database, employee());

    mk::CalcOutput output;
    output.input_snapshot.period.date_from = mk::Date(2026, 1, 1);
    output.input_snapshot.period.date_to = mk::Date(2026, 1, 31);

    mk::Calculation calculation;
    calculation.employee_id = employee_id;
    calculation.status = mk::CalcStatus::Ok;
    calculation.engine_version = "0.1.0";
    calculation.lines.push_back({
        1,
        "oklad",
        mk::CalcRole::Accrual,
        1,
        0,
        mk::Money(100000),
        "base_amount",
        mk::RoundingPolicy::PerLine
    });

    output.calculations.push_back(calculation);
    mk::persist_calculation(database, output);

    QSqlQuery query(database.sql());
    REQUIRE(query.exec("SELECT id FROM Calculation LIMIT 1"));
    REQUIRE(query.next());

    const int64_t calculation_id = query.value(0).toLongLong();
    const auto lines =
        mk::read_calculation_lines(database, calculation_id);

    REQUIRE_EQUAL(lines.size(), std::size_t{1});
    REQUIRE_EQUAL(lines.front().amount, mk::Money(100000));
    REQUIRE_EQUAL(
        lines.front().formula_snapshot,
        std::string("base_amount"));
}
