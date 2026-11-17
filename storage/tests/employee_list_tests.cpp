#include "test_framework.hpp"

#include "mk/repositories.hpp"

#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void exec_sql(mk::Database& database, const QString& sql) {
    QSqlQuery query(database.sql());
    if (!query.exec(sql)) {
        throw std::runtime_error(query.lastError().text().toStdString());
    }
}

int64_t count_rows(mk::Database& database, const QString& table) {
    QSqlQuery query(database.sql());
    if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM ") + table)) {
        throw std::runtime_error(query.lastError().text().toStdString());
    }
    if (!query.next()) {
        throw std::runtime_error("count query returned no rows");
    }
    return query.value(0).toLongLong();
}

mk::Employee make_employee(
    int64_t org_id,
    int64_t department_id,
    const std::string& number,
    const std::string& name) {
    mk::Employee employee;
    employee.personnel_number = number;
    employee.full_name = name;
    employee.org_id = org_id;
    employee.department_id = department_id;
    employee.position_id = 1;
    employee.employment_kind_id = 1;
    employee.work_schedule_id = 1;
    employee.rate = mk::Money(100000);
    employee.rate_unit = mk::RateUnit::Month;
    employee.hire_date = mk::Date(2026, 1, 1);
    return employee;
}

mk::EmploymentEvent hire_for(const mk::Employee& employee) {
    mk::EmploymentEvent event;
    event.date = employee.hire_date;
    event.kind = mk::EventKind::Hire;
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

}

TEST(storage_employee_list_is_isolated_by_organization) {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    mk::Database database;
    database.create_new(directory.filePath("employees.sqlite"));

    const int64_t second_org_id =
        mk::create_org(database, {0, "Вторая организация"});
    const int64_t empty_org_id =
        mk::create_org(database, {0, "Без сотрудников"});

    exec_sql(
        database,
        QStringLiteral(
            "INSERT INTO Department(org_id,name) "
            "VALUES(%1,'Второе подразделение')")
            .arg(second_org_id));

    QSqlQuery department(database.sql());
    department.prepare(
        "SELECT id FROM Department WHERE org_id=?");
    department.addBindValue(second_org_id);
    REQUIRE(department.exec());
    REQUIRE(department.next());
    const int64_t second_department_id =
        department.value(0).toLongLong();

    const int64_t first_id = mk::create_employee(
        database,
        make_employee(1, 1, "001", "Первый сотрудник"));
    const int64_t second_id = mk::create_employee(
        database,
        make_employee(
            second_org_id,
            second_department_id,
            "002",
            "Второй сотрудник"));

    const std::vector<mk::Employee> first =
        mk::list_employees(database, 1);
    const std::vector<mk::Employee> second =
        mk::list_employees(database, second_org_id);
    const std::vector<mk::Employee> empty =
        mk::list_employees(database, empty_org_id);

    REQUIRE_EQUAL(first.size(), std::size_t{1});
    REQUIRE_EQUAL(first.front().id, first_id);
    REQUIRE_EQUAL(first.front().org_id, int64_t{1});
    REQUIRE_EQUAL(first.front().personnel_number, std::string("001"));
    REQUIRE_EQUAL(first.front().full_name, std::string("Первый сотрудник"));

    REQUIRE_EQUAL(second.size(), std::size_t{1});
    REQUIRE_EQUAL(second.front().id, second_id);
    REQUIRE_EQUAL(second.front().org_id, second_org_id);
    REQUIRE_EQUAL(second.front().personnel_number, std::string("002"));
    REQUIRE_EQUAL(second.front().full_name, std::string("Второй сотрудник"));

    REQUIRE(empty.empty());
}

TEST(storage_departments_are_isolated_by_organization) {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    mk::Database database;
    database.create_new(directory.filePath("departments.sqlite"));

    const int64_t second_org =
        mk::create_org(database, {0, "Вторая"});
    const int64_t empty_org =
        mk::create_org(database, {0, "Пустая"});

    exec_sql(
        database,
        QStringLiteral(
            "INSERT INTO Department(id,org_id,name) "
            "VALUES(20,%1,'Второе подразделение')")
            .arg(second_org));

    const auto first = mk::list_departments(database, 1);
    const auto second = mk::list_departments(database, second_org);
    const auto empty = mk::list_departments(database, empty_org);

    REQUIRE_EQUAL(first.size(), std::size_t{1});
    REQUIRE_EQUAL(first[0].id, int64_t{1});
    REQUIRE_EQUAL(first[0].org_id, int64_t{1});
    REQUIRE_EQUAL(first[0].name, std::string("Основное подразделение"));
    REQUIRE_EQUAL(second.size(), std::size_t{1});
    REQUIRE_EQUAL(second[0].id, int64_t{20});
    REQUIRE_EQUAL(second[0].org_id, second_org);
    REQUIRE_EQUAL(second[0].name, std::string("Второе подразделение"));
    REQUIRE(empty.empty());
}

TEST(storage_employment_kinds_read_existing_records) {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    mk::Database database;
    database.create_new(directory.filePath("kinds.sqlite"));

    exec_sql(
        database,
        QStringLiteral(
            "INSERT INTO EmploymentKind(id,code,name) "
            "VALUES(20,'part_time','Совместительство')"));

    const int64_t before = count_rows(
        database, QStringLiteral("EmploymentKind"));
    const auto kinds = mk::list_employment_kinds(database);
    REQUIRE_EQUAL(kinds.size(), std::size_t{2});
    REQUIRE_EQUAL(kinds[0].id, int64_t{1});
    REQUIRE_EQUAL(kinds[0].code, std::string("main"));
    REQUIRE_EQUAL(kinds[0].name, std::string("Основное место работы"));
    REQUIRE_EQUAL(kinds[1].id, int64_t{20});
    REQUIRE_EQUAL(kinds[1].code, std::string("part_time"));
    REQUIRE_EQUAL(kinds[1].name, std::string("Совместительство"));
    REQUIRE_EQUAL(
        count_rows(database, QStringLiteral("EmploymentKind")),
        before);
}

TEST(storage_create_employee_without_hire_event_saves_employee) {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    mk::Database database;
    database.create_new(directory.filePath("employee_only.sqlite"));

    const mk::Employee employee =
        make_employee(1, 1, "0007", "Петров Пётр");
    const int64_t employee_id =
        mk::create_employee(database, employee);

    const auto saved = mk::read_employee(database, employee_id);
    REQUIRE(saved.has_value());
    REQUIRE_EQUAL(saved->id, employee_id);
    REQUIRE_EQUAL(saved->personnel_number, std::string("0007"));
    REQUIRE_EQUAL(saved->full_name, std::string("Петров Пётр"));
    REQUIRE_EQUAL(saved->org_id, int64_t{1});
    REQUIRE_EQUAL(saved->department_id, int64_t{1});
    REQUIRE_EQUAL(saved->position_id, int64_t{1});
    REQUIRE_EQUAL(saved->employment_kind_id, int64_t{1});
    REQUIRE_EQUAL(saved->work_schedule_id, int64_t{1});
    REQUIRE_EQUAL(saved->hire_date, mk::Date(2026, 1, 1));

    const auto employees = mk::list_employees(database, 1);
    REQUIRE_EQUAL(employees.size(), std::size_t{1});
    REQUIRE_EQUAL(employees.front().id, employee_id);
    REQUIRE_EQUAL(employees.front().full_name, std::string("Петров Пётр"));

    REQUIRE_EQUAL(
        count_rows(database, QStringLiteral("EmploymentEvent")),
        int64_t{0});
}

TEST(storage_create_employee_with_hire_event_saves_full_snapshot) {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    mk::Database database;
    database.create_new(directory.filePath("hire.sqlite"));

    mk::Employee employee =
        make_employee(1, 1, "0042", "Иванов Иван");
    employee.payroll_preset_id = 1;
    employee.rate = mk::Money(250001);
    employee.rate_unit = mk::RateUnit::Day;
    employee.hire_date = mk::Date(2026, 2, 3);

    mk::EmploymentEvent event = hire_for(employee);
    event.employee_id = 999999;
    event.termination_reason_id = 77;
    const int64_t original_event_employee_id = event.employee_id;

    const int64_t employee_id =
        mk::create_employee_with_hire_event(database, employee, event);

    REQUIRE_EQUAL(employee.id, int64_t{0});
    REQUIRE_EQUAL(event.employee_id, original_event_employee_id);
    const auto saved_employee = mk::read_employee(database, employee_id);
    REQUIRE(saved_employee.has_value());
    REQUIRE_EQUAL(saved_employee->id, employee_id);
    REQUIRE_EQUAL(saved_employee->personnel_number, employee.personnel_number);
    REQUIRE_EQUAL(saved_employee->full_name, employee.full_name);
    REQUIRE_EQUAL(saved_employee->org_id, employee.org_id);
    REQUIRE_EQUAL(saved_employee->department_id, employee.department_id);
    REQUIRE_EQUAL(saved_employee->position_id, employee.position_id);
    REQUIRE_EQUAL(saved_employee->employment_kind_id, employee.employment_kind_id);
    REQUIRE_EQUAL(saved_employee->work_schedule_id, employee.work_schedule_id);
    REQUIRE_EQUAL(saved_employee->payroll_preset_id, employee.payroll_preset_id);
    REQUIRE_EQUAL(saved_employee->rate, employee.rate);
    REQUIRE_EQUAL(saved_employee->rate_unit, employee.rate_unit);
    REQUIRE_EQUAL(saved_employee->hire_date, employee.hire_date);
    REQUIRE_EQUAL(saved_employee->termination_date, employee.termination_date);

    const auto events = mk::list_employment_events(database, employee_id);
    REQUIRE_EQUAL(events.size(), std::size_t{1});
    REQUIRE_EQUAL(events[0].employee_id, employee_id);
    REQUIRE_EQUAL(events[0].date, event.date);
    REQUIRE_EQUAL(events[0].kind, event.kind);
    REQUIRE_EQUAL(events[0].personnel_number, event.personnel_number);
    REQUIRE_EQUAL(events[0].full_name, event.full_name);
    REQUIRE_EQUAL(events[0].org_id, event.org_id);
    REQUIRE_EQUAL(events[0].department_id, event.department_id);
    REQUIRE_EQUAL(events[0].position_id, event.position_id);
    REQUIRE_EQUAL(events[0].employment_kind_id, event.employment_kind_id);
    REQUIRE_EQUAL(events[0].work_schedule_id, event.work_schedule_id);
    REQUIRE_EQUAL(events[0].payroll_preset_id, event.payroll_preset_id);
    REQUIRE_EQUAL(events[0].rate, event.rate);
    REQUIRE_EQUAL(events[0].rate_unit, event.rate_unit);
    REQUIRE_EQUAL(events[0].termination_reason_id, event.termination_reason_id);
    REQUIRE_EQUAL(count_rows(database, QStringLiteral("EmploymentEvent")),
                  int64_t{1});
}

TEST(storage_create_employee_with_hire_event_rolls_back_insert_failure) {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    mk::Database database;
    database.create_new(directory.filePath("hire_rollback.sqlite"));

    const int64_t employees_before =
        count_rows(database, QStringLiteral("Employee"));
    const int64_t events_before =
        count_rows(database, QStringLiteral("EmploymentEvent"));

    mk::Employee employee =
        make_employee(1, 1, "bad", "Ошибка события");
    mk::EmploymentEvent invalid = hire_for(employee);
    invalid.personnel_number.reset();

    exec_sql(
        database,
        QStringLiteral(
            "CREATE TRIGGER require_hire_number "
            "BEFORE INSERT ON EmploymentEvent "
            "WHEN NEW.personnel_number IS NULL "
            "BEGIN "
            "SELECT RAISE(ABORT,'hire number required'); "
            "END"));

    bool failed_with_insert_error = false;
    try {
        (void)mk::create_employee_with_hire_event(
            database, employee, invalid);
    } catch (const std::runtime_error& error) {
        failed_with_insert_error =
            std::string(error.what()).find("hire number required") !=
            std::string::npos;
    }
    REQUIRE(failed_with_insert_error);
    REQUIRE_EQUAL(
        count_rows(database, QStringLiteral("Employee")),
        employees_before);
    REQUIRE_EQUAL(
        count_rows(database, QStringLiteral("EmploymentEvent")),
        events_before);

    employee.personnel_number = "good";
    const int64_t id = mk::create_employee_with_hire_event(
        database, employee, hire_for(employee));
    REQUIRE(mk::read_employee(database, id).has_value());
    REQUIRE_EQUAL(mk::list_employment_events(database, id).size(),
                  std::size_t{1});
}

TEST(storage_hired_employees_remain_in_their_organizations) {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    mk::Database database;
    database.create_new(directory.filePath("hire_orgs.sqlite"));

    const int64_t second_org =
        mk::create_org(database, {0, "Вторая"});
    exec_sql(
        database,
        QStringLiteral(
            "INSERT INTO Department(id,org_id,name) "
            "VALUES(20,%1,'Второе подразделение')")
            .arg(second_org));

    const mk::Employee first =
        make_employee(1, 1, "001", "Первый");
    const mk::Employee second =
        make_employee(second_org, 20, "002", "Второй");
    const int64_t first_id = mk::create_employee_with_hire_event(
        database, first, hire_for(first));
    const int64_t second_id = mk::create_employee_with_hire_event(
        database, second, hire_for(second));

    const auto first_list = mk::list_employees(database, 1);
    const auto second_list = mk::list_employees(database, second_org);
    REQUIRE_EQUAL(first_list.size(), std::size_t{1});
    REQUIRE_EQUAL(first_list[0].id, first_id);
    REQUIRE_EQUAL(second_list.size(), std::size_t{1});
    REQUIRE_EQUAL(second_list[0].id, second_id);
}

TEST(storage_hire_does_not_commit_an_existing_transaction) {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    mk::Database database;
    database.create_new(directory.filePath("hire_nested.sqlite"));

    const mk::Employee employee =
        make_employee(1, 1, "nested", "Транзакция");
    const int64_t before =
        count_rows(database, QStringLiteral("Employee"));

    database.transaction();
    try {
        REQUIRE_THROWS(
            mk::create_employee_with_hire_event(
                database, employee, hire_for(employee)));
        REQUIRE_EQUAL(
            count_rows(database, QStringLiteral("Employee")),
            before);
        database.rollback();
    } catch (...) {
        try {
            database.rollback();
        } catch (...) {
        }
        throw;
    }

    const int64_t id = mk::create_employee_with_hire_event(
        database, employee, hire_for(employee));
    REQUIRE(mk::read_employee(database, id).has_value());
}
