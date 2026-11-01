#include "test_framework.hpp"

#include "mk/repositories.hpp"

#include <QDir>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QVariant>

#include <cstdint>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

mk::Database make_database(QTemporaryDir& directory) {
    mk::Database database;
    database.create_new(
        directory.filePath("storage.sqlite"));
    return database;
}

void exec_sql(
    mk::Database& database,
    const QString& sql) {
    QSqlQuery query(database.sql());
    if (!query.exec(sql)) {
        throw std::runtime_error(
            query.lastError().text().toStdString());
    }
}

int64_t scalar_int64(
    mk::Database& database,
    const QString& sql) {
    QSqlQuery query(database.sql());
    if (!query.exec(sql)) {
        throw std::runtime_error(
            query.lastError().text().toStdString());
    }
    if (!query.next()) {
        throw std::runtime_error("query returned no rows");
    }
    return query.value(0).toLongLong();
}

std::string scalar_text(
    mk::Database& database,
    const QString& sql) {
    QSqlQuery query(database.sql());
    if (!query.exec(sql)) {
        throw std::runtime_error(
            query.lastError().text().toStdString());
    }
    if (!query.next()) {
        throw std::runtime_error("query returned no rows");
    }
    return query.value(0).toString().toStdString();
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

mk::EmploymentEvent event_for(
    int64_t employee_id,
    const mk::Date& date) {
    mk::EmploymentEvent result;
    result.employee_id = employee_id;
    result.date = date;
    result.kind = mk::EventKind::Hire;
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
    return result;
}

mk::TimeFact fact_for(
    int64_t employee_id,
    const mk::Date& date,
    int32_t minutes) {
    mk::TimeFact result;
    result.employee_id = employee_id;
    result.date = date;
    result.worked_minutes = minutes;
    return result;
}

void require_employee_equal(
    const mk::Employee& actual,
    const mk::Employee& expected) {
    REQUIRE_EQUAL(actual.id, expected.id);
    REQUIRE_EQUAL(
        actual.personnel_number,
        expected.personnel_number);
    REQUIRE_EQUAL(actual.full_name, expected.full_name);
    REQUIRE_EQUAL(actual.org_id, expected.org_id);
    REQUIRE_EQUAL(
        actual.department_id,
        expected.department_id);
    REQUIRE_EQUAL(actual.position_id, expected.position_id);
    REQUIRE_EQUAL(
        actual.employment_kind_id,
        expected.employment_kind_id);
    REQUIRE_EQUAL(
        actual.work_schedule_id,
        expected.work_schedule_id);
    REQUIRE_EQUAL(
        actual.payroll_preset_id,
        expected.payroll_preset_id);
    REQUIRE_EQUAL(actual.rate, expected.rate);
    REQUIRE_EQUAL(actual.rate_unit, expected.rate_unit);
    REQUIRE_EQUAL(actual.hire_date, expected.hire_date);
    REQUIRE_EQUAL(
        actual.termination_date,
        expected.termination_date);
}

void require_event_equal(
    const mk::EmploymentEvent& actual,
    const mk::EmploymentEvent& expected) {
    REQUIRE_EQUAL(
        actual.employee_id,
        expected.employee_id);
    REQUIRE_EQUAL(actual.date, expected.date);
    REQUIRE_EQUAL(actual.kind, expected.kind);
    REQUIRE_EQUAL(
        actual.personnel_number,
        expected.personnel_number);
    REQUIRE_EQUAL(actual.full_name, expected.full_name);
    REQUIRE_EQUAL(actual.org_id, expected.org_id);
    REQUIRE_EQUAL(
        actual.department_id,
        expected.department_id);
    REQUIRE_EQUAL(
        actual.position_id,
        expected.position_id);
    REQUIRE_EQUAL(
        actual.employment_kind_id,
        expected.employment_kind_id);
    REQUIRE_EQUAL(
        actual.work_schedule_id,
        expected.work_schedule_id);
    REQUIRE_EQUAL(
        actual.payroll_preset_id,
        expected.payroll_preset_id);
    REQUIRE_EQUAL(actual.rate, expected.rate);
    REQUIRE_EQUAL(actual.rate_unit, expected.rate_unit);
    REQUIRE_EQUAL(
        actual.termination_reason_id,
        expected.termination_reason_id);
}

void require_fact_equal(
    const mk::TimeFact& actual,
    const mk::TimeFact& expected) {
    REQUIRE_EQUAL(
        actual.employee_id,
        expected.employee_id);
    REQUIRE_EQUAL(actual.date, expected.date);
    REQUIRE_EQUAL(
        actual.worked_minutes,
        expected.worked_minutes);
    REQUIRE_EQUAL(
        actual.absence_kind_id,
        expected.absence_kind_id);
}

}

TEST(storage_schema_seed_and_foreign_keys) {
    QTemporaryDir directory;
    mk::Database database = make_database(directory);

    const std::set<std::string> expected_tables{
        "AbsenceKind",
        "CalcItem",
        "Calculation",
        "CalculationLine",
        "Department",
        "Employee",
        "EmploymentEvent",
        "EmploymentKind",
        "Org",
        "OutputFact",
        "PaymentDoc",
        "PaymentLine",
        "PayrollPeriod",
        "PayrollPreset",
        "PayrollPresetItem",
        "Position",
        "TerminationReason",
        "TimeFact",
        "WorkSchedule"
    };

    QSqlQuery tables(database.sql());
    REQUIRE(tables.exec(
        "SELECT name FROM sqlite_master "
        "WHERE type='table' "
        "AND name NOT LIKE 'sqlite_%' ORDER BY name"));

    std::set<std::string> actual_tables;
    while (tables.next()) {
        actual_tables.insert(
            tables.value(0).toString().toStdString());
    }

    REQUIRE_EQUAL(actual_tables, expected_tables);
    REQUIRE_EQUAL(
        scalar_text(
            database,
            "SELECT name FROM Org WHERE id=1"),
        std::string("Основная организация"));
    REQUIRE_EQUAL(
        scalar_text(
            database,
            "SELECT code FROM CalcItem WHERE id=1"),
        std::string("oklad"));
    REQUIRE_EQUAL(
        scalar_text(
            database,
            "SELECT formula FROM CalcItem WHERE id=1"),
        std::string(
            "base_amount * worked_days / norm_days"));
    REQUIRE_EQUAL(
        scalar_int64(
            database,
            "SELECT calc_item_id FROM PayrollPresetItem "
            "WHERE preset_id=1"),
        int64_t{1});
    REQUIRE_EQUAL(
        scalar_int64(
            database,
            "PRAGMA foreign_keys"),
        int64_t{1});

    QSqlQuery invalid(database.sql());
    REQUIRE(!invalid.exec(
        "INSERT INTO Department(org_id,name) "
        "VALUES(999999,'invalid')"));
}

TEST(storage_resources_work_from_other_directory) {
    QTemporaryDir database_directory;
    QTemporaryDir working_directory;
    REQUIRE(database_directory.isValid());
    REQUIRE(working_directory.isValid());

    const QString original = QDir::currentPath();
    REQUIRE(QDir::setCurrent(working_directory.path()));

    try {
        mk::Database database;
        database.create_new(
            database_directory.filePath("other.sqlite"));
        REQUIRE_EQUAL(
            scalar_int64(
                database,
                "SELECT COUNT(*) FROM Org"),
            int64_t{1});
        database.close();
    } catch (...) {
        QDir::setCurrent(original);
        throw;
    }

    REQUIRE(QDir::setCurrent(original));
}

TEST(storage_org_and_position_crud) {
    QTemporaryDir directory;
    mk::Database database = make_database(directory);

    const int64_t org_id =
        mk::create_org(database, {0, "Организация"});
    auto org = mk::read_org(database, org_id);
    REQUIRE(org.has_value());
    REQUIRE_EQUAL(org->name, std::string("Организация"));

    org->name = "Новая организация";
    mk::update_org(database, *org);
    REQUIRE_EQUAL(
        mk::read_org(database, org_id)->name,
        std::string("Новая организация"));

    mk::delete_org(database, org_id);
    REQUIRE(!mk::read_org(database, org_id).has_value());
    REQUIRE(!mk::read_org(database, 999999).has_value());
    REQUIRE_THROWS(
        mk::update_org(database, {999999, "missing"}));
    REQUIRE_THROWS(
        mk::delete_org(database, 999999));

    const int64_t position_id =
        mk::create_position(database, {0, "Инженер"});
    auto position =
        mk::read_position(database, position_id);
    REQUIRE(position.has_value());
    REQUIRE_EQUAL(
        position->name,
        std::string("Инженер"));

    position->name = "Ведущий инженер";
    mk::update_position(database, *position);
    REQUIRE_EQUAL(
        mk::read_position(database, position_id)->name,
        std::string("Ведущий инженер"));

    mk::delete_position(database, position_id);
    REQUIRE(
        !mk::read_position(database, position_id)
            .has_value());
    REQUIRE(
        !mk::read_position(database, 999999)
            .has_value());
    REQUIRE_THROWS(
        mk::update_position(
            database,
            {999999, "missing"}));
    REQUIRE_THROWS(
        mk::delete_position(database, 999999));
}

TEST(storage_work_schedule_crud) {
    QTemporaryDir directory;
    mk::Database database = make_database(directory);

    mk::WorkSchedule schedule;
    schedule.name = "Сменный";
    schedule.kind = mk::ScheduleKind::Shift;
    schedule.normative_minutes_per_day = 720;
    schedule.shift_cycle = "WWRR";
    schedule.anchor_date = mk::Date(2026, 1, 10);

    const int64_t schedule_id =
        mk::create_work_schedule(database, schedule);
    auto loaded =
        mk::read_work_schedule(database, schedule_id);

    REQUIRE(loaded.has_value());
    REQUIRE_EQUAL(loaded->name, schedule.name);
    REQUIRE_EQUAL(loaded->kind, schedule.kind);
    REQUIRE_EQUAL(
        loaded->normative_minutes_per_day,
        schedule.normative_minutes_per_day);
    REQUIRE_EQUAL(
        loaded->shift_cycle,
        schedule.shift_cycle);
    REQUIRE_EQUAL(
        loaded->anchor_date,
        schedule.anchor_date);

    loaded->name = "Изменённый";
    loaded->kind = mk::ScheduleKind::FiveDay;
    loaded->normative_minutes_per_day = 420;
    loaded->shift_cycle.clear();
    loaded->anchor_date.reset();
    mk::update_work_schedule(database, *loaded);

    const auto changed =
        mk::read_work_schedule(database, schedule_id);
    REQUIRE_EQUAL(
        changed->name,
        std::string("Изменённый"));
    REQUIRE_EQUAL(
        changed->kind,
        mk::ScheduleKind::FiveDay);
    REQUIRE_EQUAL(
        changed->normative_minutes_per_day,
        int32_t{420});
    REQUIRE(changed->shift_cycle.empty());
    REQUIRE(!changed->anchor_date.has_value());

    mk::delete_work_schedule(database, schedule_id);
    REQUIRE(
        !mk::read_work_schedule(database, schedule_id)
            .has_value());
    REQUIRE(
        !mk::read_work_schedule(database, 999999)
            .has_value());

    loaded->id = 999999;
    REQUIRE_THROWS(
        mk::update_work_schedule(database, *loaded));
    REQUIRE_THROWS(
        mk::delete_work_schedule(database, 999999));
}

TEST(storage_employee_crud_all_fields) {
    QTemporaryDir directory;
    mk::Database database = make_database(directory);

    mk::Employee value = employee();
    value.personnel_number = "00042";
    value.full_name = "Иванов Иван Иванович";
    value.rate = mk::Money(9007199254740993LL);
    value.rate_unit = mk::RateUnit::Hour;
    value.hire_date = mk::Date(2025, 12, 15);
    value.termination_date = mk::Date(2026, 12, 31);

    const int64_t employee_id =
        mk::create_employee(database, value);
    value.id = employee_id;

    const auto loaded =
        mk::read_employee(database, employee_id);
    REQUIRE(loaded.has_value());
    require_employee_equal(*loaded, value);

    value.personnel_number = "00043";
    value.full_name = "Петров Пётр Петрович";
    value.payroll_preset_id.reset();
    value.rate = mk::Money(12345678901234567LL);
    value.rate_unit = mk::RateUnit::Day;
    value.hire_date = mk::Date(2025, 11, 1);
    value.termination_date.reset();

    mk::update_employee(database, value);
    require_employee_equal(
        *mk::read_employee(database, employee_id),
        value);

    mk::delete_employee(database, employee_id);
    REQUIRE(
        !mk::read_employee(database, employee_id)
            .has_value());
    REQUIRE(
        !mk::read_employee(database, 999999)
            .has_value());

    value.id = 999999;
    REQUIRE_THROWS(
        mk::update_employee(database, value));
    REQUIRE_THROWS(
        mk::delete_employee(database, 999999));
}

TEST(storage_employment_event_crud_by_record_id) {
    QTemporaryDir directory;
    mk::Database database = make_database(directory);

    const int64_t employee_id =
        mk::create_employee(database, employee());
    REQUIRE(employee_id != 999999);

    mk::EmploymentEvent first = event_for(
        employee_id,
        mk::Date(2025, 12, 1));
    mk::EmploymentEvent second = event_for(
        employee_id,
        mk::Date(2025, 12, 15));
    second.kind = mk::EventKind::Transfer;
    second.full_name = "До изменения";

    const int64_t first_id =
        mk::create_employment_event(database, first);
    const int64_t second_id =
        mk::create_employment_event(database, second);

    REQUIRE(first_id != employee_id);
    REQUIRE(second_id != employee_id);
    require_event_equal(
        *mk::read_employment_event(database, first_id),
        first);
    require_event_equal(
        *mk::read_employment_event(database, second_id),
        second);

    second.date = mk::Date(2026, 1, 5);
    second.kind = mk::EventKind::ChangeSalary;
    second.personnel_number = "002";
    second.full_name = "После изменения";
    second.org_id = 1;
    second.department_id = 1;
    second.position_id = 1;
    second.employment_kind_id = 1;
    second.work_schedule_id = 1;
    second.payroll_preset_id.reset();
    second.rate = mk::Money(9007199254740993LL);
    second.rate_unit = mk::RateUnit::Hour;
    second.termination_reason_id = 77;

    mk::update_employment_event(
        database,
        second_id,
        second);

    require_event_equal(
        *mk::read_employment_event(database, second_id),
        second);
    require_event_equal(
        *mk::read_employment_event(database, first_id),
        first);

    REQUIRE(
        !mk::read_employment_event(database, 999999)
            .has_value());
    REQUIRE_THROWS(
        mk::update_employment_event(
            database,
            999999,
            second));
    REQUIRE_THROWS(
        mk::delete_employment_event(
            database,
            999999));

    mk::delete_employment_event(database, second_id);
    REQUIRE(
        !mk::read_employment_event(database, second_id)
            .has_value());
    REQUIRE(
        mk::read_employment_event(database, first_id)
            .has_value());
}

TEST(storage_time_fact_crud_by_record_id) {
    QTemporaryDir directory;
    mk::Database database = make_database(directory);

    const int64_t employee_id =
        mk::create_employee(database, employee());

    mk::TimeFact first = fact_for(
        employee_id,
        mk::Date(2026, 1, 10),
        480);
    mk::TimeFact second = fact_for(
        employee_id,
        mk::Date(2026, 1, 11),
        120);

    const int64_t first_id =
        mk::create_time_fact(database, first);
    const int64_t second_id =
        mk::create_time_fact(database, second);

    REQUIRE(first_id != employee_id);
    REQUIRE(second_id != employee_id);

    second.date = mk::Date(2026, 1, 12);
    second.worked_minutes = 300;
    second.absence_kind_id = 1;
    mk::update_time_fact(database, second_id, second);

    require_fact_equal(
        *mk::read_time_fact(database, second_id),
        second);
    require_fact_equal(
        *mk::read_time_fact(database, first_id),
        first);

    const auto january = mk::list_time_facts(
        database,
        employee_id,
        mk::Date(2026, 1, 11),
        mk::Date(2026, 1, 31));
    REQUIRE_EQUAL(january.size(), std::size_t{1});
    require_fact_equal(january.front(), second);

    REQUIRE(
        !mk::read_time_fact(database, 999999)
            .has_value());
    REQUIRE_THROWS(
        mk::update_time_fact(
            database,
            999999,
            second));
    REQUIRE_THROWS(
        mk::delete_time_fact(database, 999999));

    mk::delete_time_fact(database, second_id);
    REQUIRE(
        !mk::read_time_fact(database, second_id)
            .has_value());
    REQUIRE(
        mk::read_time_fact(database, first_id)
            .has_value());
}

TEST(storage_transactions_and_reopen) {
    QTemporaryDir directory;
    const QString path =
        directory.filePath("transactions.sqlite");

    mk::Database database;
    database.create_new(path);

    database.transaction();
    exec_sql(
        database,
        "INSERT INTO Position(name) VALUES('commit')");
    database.commit();

    database.transaction();
    exec_sql(
        database,
        "INSERT INTO Position(name) VALUES('rollback')");
    database.rollback();

    REQUIRE_EQUAL(
        scalar_int64(
            database,
            "SELECT COUNT(*) FROM Position "
            "WHERE name='commit'"),
        int64_t{1});
    REQUIRE_EQUAL(
        scalar_int64(
            database,
            "SELECT COUNT(*) FROM Position "
            "WHERE name='rollback'"),
        int64_t{0});

    database.close();
    database.open(path);

    REQUIRE_EQUAL(
        scalar_int64(
            database,
            "SELECT COUNT(*) FROM Position "
            "WHERE name='commit'"),
        int64_t{1});
    REQUIRE_EQUAL(
        scalar_int64(
            database,
            "SELECT COUNT(*) FROM Position "
            "WHERE name='rollback'"),
        int64_t{0});
}

TEST(storage_build_input_filters_org_and_period) {
    QTemporaryDir directory;
    mk::Database database = make_database(directory);

    exec_sql(
        database,
        "INSERT INTO Org(id,name) "
        "VALUES(2,'Другая организация')");
    exec_sql(
        database,
        "INSERT INTO Department(id,org_id,name) "
        "VALUES(2,2,'Другое подразделение')");

    mk::Employee first = employee();
    first.full_name = "Сотрудник первой организации";
    const int64_t first_id =
        mk::create_employee(database, first);

    mk::Employee second = employee();
    second.personnel_number = "002";
    second.full_name = "Сотрудник второй организации";
    second.org_id = 2;
    second.department_id = 2;
    const int64_t second_id =
        mk::create_employee(database, second);

    mk::EmploymentEvent past = event_for(
        first_id,
        mk::Date(2025, 12, 1));
    mk::EmploymentEvent current = event_for(
        first_id,
        mk::Date(2026, 1, 20));
    current.kind = mk::EventKind::ChangeSalary;
    current.rate = mk::Money(200000);
    mk::EmploymentEvent future = event_for(
        first_id,
        mk::Date(2026, 2, 1));
    future.kind = mk::EventKind::Transfer;

    mk::create_employment_event(database, past);
    mk::create_employment_event(database, current);
    mk::create_employment_event(database, future);
    mk::create_employment_event(
        database,
        event_for(second_id, mk::Date(2025, 12, 1)));

    mk::create_time_fact(
        database,
        fact_for(
            first_id,
            mk::Date(2025, 12, 31),
            111));
    mk::create_time_fact(
        database,
        fact_for(
            first_id,
            mk::Date(2026, 1, 15),
            480));
    mk::create_time_fact(
        database,
        fact_for(
            first_id,
            mk::Date(2026, 2, 1),
            222));
    mk::create_time_fact(
        database,
        fact_for(
            second_id,
            mk::Date(2026, 1, 15),
            333));

    QSqlQuery output(database.sql());
    output.prepare(
        "INSERT INTO OutputFact("
        "employee_id,fact_date,code,"
        "value_numerator,value_denominator)"
        " VALUES(?,?,?,?,?)");

    const auto insert_output =
        [&](int64_t employee_id,
            const mk::Date& date,
            const QString& code,
            int64_t numerator,
            int64_t denominator) {
            output.bindValue(0, employee_id);
            output.bindValue(1, date.ordinal());
            output.bindValue(2, code);
            output.bindValue(3, numerator);
            output.bindValue(4, denominator);
            REQUIRE(output.exec());
        };

    insert_output(
        first_id,
        mk::Date(2025, 12, 31),
        "before",
        1,
        1);
    insert_output(
        first_id,
        mk::Date(2026, 1, 16),
        "inside",
        3,
        2);
    insert_output(
        first_id,
        mk::Date(2026, 2, 1),
        "after",
        2,
        1);
    insert_output(
        second_id,
        mk::Date(2026, 1, 16),
        "other_org",
        4,
        1);

    const mk::CalcInput input =
        mk::build_calc_input(database, 1, 2026, 1);

    REQUIRE_EQUAL(input.employees.size(), std::size_t{1});
    REQUIRE_EQUAL(
        input.employees.front().employee_id,
        first_id);
    REQUIRE_EQUAL(
        input.employees.front().full_name,
        std::string(
            "Сотрудник первой организации"));

    REQUIRE_EQUAL(
        input.employment_events.size(),
        std::size_t{2});
    REQUIRE_EQUAL(
        input.employment_events[0].date,
        mk::Date(2025, 12, 1));
    REQUIRE_EQUAL(
        input.employment_events[1].date,
        mk::Date(2026, 1, 20));
    REQUIRE_EQUAL(
        *input.employment_events[1].rate,
        mk::Money(200000));

    REQUIRE_EQUAL(input.time_facts.size(), std::size_t{1});
    REQUIRE_EQUAL(
        input.time_facts.front().employee_id,
        first_id);
    REQUIRE_EQUAL(
        input.time_facts.front().date,
        mk::Date(2026, 1, 15));
    REQUIRE_EQUAL(
        input.time_facts.front().worked_minutes,
        int32_t{480});

    REQUIRE_EQUAL(
        input.output_facts.size(),
        std::size_t{1});
    REQUIRE_EQUAL(
        input.output_facts.front().employee_id,
        first_id);
    REQUIRE_EQUAL(
        input.output_facts.front().date,
        mk::Date(2026, 1, 16));
    REQUIRE_EQUAL(
        input.output_facts.front().code,
        std::string("inside"));
    REQUIRE_EQUAL(
        input.output_facts.front().value,
        (mk::Rational{3, 2}));

    REQUIRE_EQUAL(
        input.period.date_from,
        mk::Date(2026, 1, 1));
    REQUIRE_EQUAL(
        input.period.date_to,
        mk::Date(2026, 1, 31));
    REQUIRE_EQUAL(input.calc_items.size(), std::size_t{1});
    REQUIRE_EQUAL(
        input.calc_items.front().code,
        std::string("oklad"));
    REQUIRE_EQUAL(
        input.payroll_presets.size(),
        std::size_t{1});
    REQUIRE_EQUAL(
        input.payroll_presets.front().calc_item_ids,
        (std::vector<int64_t>{1}));
    REQUIRE_EQUAL(input.norm_days.size(), std::size_t{31});
    REQUIRE_EQUAL(
        input.norm_days.front().date,
        mk::Date(2026, 1, 1));
}

TEST(storage_persist_calculation_all_fields) {
    QTemporaryDir directory;
    mk::Database database = make_database(directory);

    const int64_t employee_id =
        mk::create_employee(database, employee());
    const int64_t large = 9007199254740993LL;

    mk::CalcOutput output;
    output.input_snapshot.period.date_from =
        mk::Date(2026, 1, 1);
    output.input_snapshot.period.date_to =
        mk::Date(2026, 1, 31);
    output.input_snapshot.period.status =
        mk::PeriodStatus::Closed;

    mk::Calculation calculation;
    calculation.employee_id = employee_id;
    calculation.status = mk::CalcStatus::Incomplete;
    calculation.incomplete_reasons = {
        "первая причина",
        "вторая причина"
    };
    calculation.total_accrual = mk::Money(large);
    calculation.total_pre_tax_deduction =
        mk::Money(101);
    calculation.income_tax = mk::Money(202);
    calculation.total_employer_contribution =
        mk::Money(303);
    calculation.total_post_tax_deduction =
        mk::Money(404);
    calculation.net_amount =
        mk::Money(large - 707);
    calculation.engine_version = "0.1.0";
    calculation.input_snapshot = "{\"snapshot\":true}";

    calculation.lines.push_back({
        1,
        "oklad",
        mk::CalcRole::Accrual,
        1,
        10,
        mk::Money(large),
        "base_amount",
        mk::RoundingPolicy::PerLine
    });
    calculation.lines.push_back({
        2,
        "tax",
        mk::CalcRole::IncomeTax,
        4,
        20,
        mk::Money(202),
        "ndfl_base * 0.13",
        mk::RoundingPolicy::Carry
    });

    output.calculations.push_back(calculation);
    mk::persist_calculation(database, output);

    QSqlQuery period(database.sql());
    REQUIRE(period.exec(
        "SELECT date_from,date_to,status "
        "FROM PayrollPeriod"));
    REQUIRE(period.next());
    REQUIRE_EQUAL(
        period.value(0).toInt(),
        mk::Date(2026, 1, 1).ordinal());
    REQUIRE_EQUAL(
        period.value(1).toInt(),
        mk::Date(2026, 1, 31).ordinal());
    REQUIRE_EQUAL(
        period.value(2).toInt(),
        static_cast<int>(mk::PeriodStatus::Closed));

    QSqlQuery query(database.sql());
    REQUIRE(query.exec(
        "SELECT id,employee_id,payroll_period_id,"
        "status,incomplete_reasons,"
        "total_accrual_cents,"
        "total_pre_tax_deduction_cents,"
        "income_tax_cents,"
        "total_employer_contribution_cents,"
        "total_post_tax_deduction_cents,"
        "net_amount_cents,engine_version,"
        "input_snapshot,typeof(engine_version) "
        "FROM Calculation"));
    REQUIRE(query.next());

    const int64_t calculation_id =
        query.value(0).toLongLong();
    REQUIRE_EQUAL(
        query.value(1).toLongLong(),
        employee_id);
    REQUIRE_EQUAL(query.value(2).toLongLong(), int64_t{1});
    REQUIRE_EQUAL(
        query.value(3).toInt(),
        static_cast<int>(mk::CalcStatus::Incomplete));
    REQUIRE_EQUAL(
        query.value(4).toString().toStdString(),
        std::string(
            "[\"первая причина\",\"вторая причина\"]"));
    REQUIRE_EQUAL(query.value(5).toLongLong(), large);
    REQUIRE_EQUAL(query.value(6).toLongLong(), int64_t{101});
    REQUIRE_EQUAL(query.value(7).toLongLong(), int64_t{202});
    REQUIRE_EQUAL(query.value(8).toLongLong(), int64_t{303});
    REQUIRE_EQUAL(query.value(9).toLongLong(), int64_t{404});
    REQUIRE_EQUAL(
        query.value(10).toLongLong(),
        large - 707);
    REQUIRE_EQUAL(
        query.value(11).toString().toStdString(),
        std::string("0.1.0"));
    REQUIRE_EQUAL(
        query.value(12).toString().toStdString(),
        std::string("{\"snapshot\":true}"));
    REQUIRE_EQUAL(
        query.value(13).toString().toStdString(),
        std::string("text"));

    const auto lines =
        mk::read_calculation_lines(
            database,
            calculation_id);
    REQUIRE_EQUAL(lines.size(), std::size_t{2});

    REQUIRE_EQUAL(lines[0].calc_item_id, int64_t{1});
    REQUIRE_EQUAL(
        lines[0].calc_item_code,
        std::string("oklad"));
    REQUIRE_EQUAL(
        lines[0].role,
        mk::CalcRole::Accrual);
    REQUIRE_EQUAL(lines[0].stage, int32_t{1});
    REQUIRE_EQUAL(lines[0].priority, int32_t{10});
    REQUIRE_EQUAL(lines[0].amount, mk::Money(large));
    REQUIRE_EQUAL(
        lines[0].formula_snapshot,
        std::string("base_amount"));
    REQUIRE_EQUAL(
        lines[0].rounding_policy,
        mk::RoundingPolicy::PerLine);

    REQUIRE_EQUAL(lines[1].calc_item_id, int64_t{2});
    REQUIRE_EQUAL(
        lines[1].calc_item_code,
        std::string("tax"));
    REQUIRE_EQUAL(
        lines[1].role,
        mk::CalcRole::IncomeTax);
    REQUIRE_EQUAL(lines[1].stage, int32_t{4});
    REQUIRE_EQUAL(lines[1].priority, int32_t{20});
    REQUIRE_EQUAL(lines[1].amount, mk::Money(202));
    REQUIRE_EQUAL(
        lines[1].formula_snapshot,
        std::string("ndfl_base * 0.13"));
    REQUIRE_EQUAL(
        lines[1].rounding_policy,
        mk::RoundingPolicy::Carry);
}

TEST(storage_persist_calculation_is_atomic) {
    QTemporaryDir directory;
    mk::Database database = make_database(directory);

    const int64_t employee_id =
        mk::create_employee(database, employee());

    exec_sql(
        database,
        "CREATE TRIGGER reject_bad_line "
        "BEFORE INSERT ON CalculationLine "
        "WHEN NEW.calc_item_code='fail' "
        "BEGIN "
        "SELECT RAISE(ABORT,'intentional failure'); "
        "END");

    mk::CalcOutput output;
    output.input_snapshot.period.date_from =
        mk::Date(2026, 1, 1);
    output.input_snapshot.period.date_to =
        mk::Date(2026, 1, 31);

    mk::Calculation calculation;
    calculation.employee_id = employee_id;
    calculation.engine_version = "0.1.0";
    calculation.lines.push_back({
        1,
        "ok",
        mk::CalcRole::Accrual,
        1,
        0,
        mk::Money(100),
        "1",
        mk::RoundingPolicy::PerLine
    });
    calculation.lines.push_back({
        2,
        "fail",
        mk::CalcRole::Accrual,
        1,
        0,
        mk::Money(200),
        "2",
        mk::RoundingPolicy::PerLine
    });
    output.calculations.push_back(calculation);

    REQUIRE_THROWS(
        mk::persist_calculation(database, output));

    REQUIRE_EQUAL(
        scalar_int64(
            database,
            "SELECT COUNT(*) FROM PayrollPeriod"),
        int64_t{0});
    REQUIRE_EQUAL(
        scalar_int64(
            database,
            "SELECT COUNT(*) FROM Calculation"),
        int64_t{0});
    REQUIRE_EQUAL(
        scalar_int64(
            database,
            "SELECT COUNT(*) FROM CalculationLine"),
        int64_t{0});
}
