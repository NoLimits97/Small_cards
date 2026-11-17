#include "test_framework.hpp"

#include "mk/repositories.hpp"

#include <QTemporaryDir>

#include <cstdint>
#include <optional>
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
    employee.rate = mk::Money(100000);
    employee.rate_unit = mk::RateUnit::Month;
    employee.hire_date = mk::Date(2026, 1, 1);
    return employee;
}

mk::TimeFact make_fact(
    int64_t employee_id,
    int year,
    int month,
    int day,
    int32_t minutes,
    std::optional<int64_t> absence_kind_id = std::nullopt) {
    mk::TimeFact fact;
    fact.employee_id = employee_id;
    fact.date = mk::Date(year, month, day);
    fact.worked_minutes = minutes;
    fact.absence_kind_id = absence_kind_id;
    return fact;
}

}

TEST(storage_timesheet_facts_round_trip_with_ids) {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    mk::Database database;
    database.create_new(directory.filePath("timesheet.sqlite"));

    const int64_t employee_id = mk::create_employee(
        database, make_employee("0100"));

    const int64_t first_id = mk::create_time_fact(
        database,
        make_fact(employee_id, 2026, 1, 5, 480));
    const int64_t second_id = mk::create_time_fact(
        database,
        make_fact(employee_id, 2026, 1, 6, 240, 1));

    const std::vector<mk::TimeFact> facts =
        mk::list_time_facts(
            database,
            employee_id,
            mk::Date(2026, 1, 1),
            mk::Date(2026, 1, 31));
    REQUIRE_EQUAL(facts.size(), std::size_t{2});
    REQUIRE_EQUAL(facts[0].id, first_id);
    REQUIRE_EQUAL(facts[0].worked_minutes, int32_t{480});
    REQUIRE_EQUAL(facts[1].id, second_id);
    REQUIRE_EQUAL(facts[1].worked_minutes, int32_t{240});
    REQUIRE(facts[1].absence_kind_id.has_value());
    REQUIRE_EQUAL(*facts[1].absence_kind_id, int64_t{1});

    mk::TimeFact updated = facts[0];
    updated.worked_minutes = 300;
    mk::update_time_fact(database, updated.id, updated);

    const auto saved = mk::read_time_fact(database, first_id);
    REQUIRE(saved.has_value());
    REQUIRE_EQUAL(saved->id, first_id);
    REQUIRE_EQUAL(saved->worked_minutes, int32_t{300});

    mk::delete_time_fact(database, second_id);
    const std::vector<mk::TimeFact> remaining =
        mk::list_time_facts(
            database,
            employee_id,
            mk::Date(2026, 1, 1),
            mk::Date(2026, 1, 31));
    REQUIRE_EQUAL(remaining.size(), std::size_t{1});
    REQUIRE_EQUAL(remaining[0].id, first_id);
}

TEST(storage_timesheet_facts_respect_month_boundaries) {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    mk::Database database;
    database.create_new(directory.filePath("timesheet_periods.sqlite"));

    const int64_t employee_id = mk::create_employee(
        database, make_employee("0101"));

    (void)mk::create_time_fact(
        database,
        make_fact(employee_id, 2026, 1, 31, 480));
    (void)mk::create_time_fact(
        database,
        make_fact(employee_id, 2026, 2, 1, 480));
    (void)mk::create_time_fact(
        database,
        make_fact(employee_id, 2026, 4, 30, 240));
    (void)mk::create_time_fact(
        database,
        make_fact(employee_id, 2026, 5, 1, 480));
    (void)mk::create_time_fact(
        database,
        make_fact(employee_id, 2028, 2, 29, 480));
    (void)mk::create_time_fact(
        database,
        make_fact(employee_id, 2028, 3, 1, 480));

    const auto read_month = [&](int year, int month) {
        return mk::list_time_facts(
            database,
            employee_id,
            mk::Date(year, month, 1),
            mk::Date(
                year,
                month,
                mk::Date::days_in_month(year, month)));
    };

    const auto january_2026 = read_month(2026, 1);
    REQUIRE_EQUAL(january_2026.size(), std::size_t{1});
    REQUIRE_EQUAL(january_2026[0].date, mk::Date(2026, 1, 31));

    const auto february_2026 = read_month(2026, 2);
    REQUIRE_EQUAL(february_2026.size(), std::size_t{1});
    REQUIRE_EQUAL(february_2026[0].date, mk::Date(2026, 2, 1));
    REQUIRE_EQUAL(
        mk::Date::days_in_month(2026, 2),
        int32_t{28});

    const auto april_2026 = read_month(2026, 4);
    REQUIRE_EQUAL(april_2026.size(), std::size_t{1});
    REQUIRE_EQUAL(april_2026[0].date, mk::Date(2026, 4, 30));
    REQUIRE_EQUAL(
        mk::Date::days_in_month(2026, 4),
        int32_t{30});

    const auto february_2028 = read_month(2028, 2);
    REQUIRE_EQUAL(february_2028.size(), std::size_t{1});
    REQUIRE_EQUAL(february_2028[0].date, mk::Date(2028, 2, 29));
    REQUIRE_EQUAL(
        mk::Date::days_in_month(2028, 2),
        int32_t{29});
}
