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
