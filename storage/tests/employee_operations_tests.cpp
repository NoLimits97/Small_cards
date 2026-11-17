#include "test_framework.hpp"

#include "mk/repositories.hpp"

#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>

#include <cstdint>
#include <optional>
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

mk::Database make_database(QTemporaryDir& directory, const QString& name) {
    mk::Database database;
    database.create_new(directory.filePath(name));
    return database;
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
    employee.payroll_preset_id = 1;
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

int64_t hire(
    mk::Database& database,
    const mk::Employee& employee) {
    return mk::create_employee_with_hire_event(
        database, employee, hire_for(employee));
}

int64_t create_references(mk::Database& database) {
    exec_sql(
        database,
        QStringLiteral(
            "INSERT INTO Department(id,org_id,name) "
            "VALUES(20,1,'Приёмный отдел')"));
    exec_sql(
        database,
        QStringLiteral(
            "INSERT INTO Position(id,name) VALUES(20,'Мастер')"));
    exec_sql(
        database,
        QStringLiteral(
            "INSERT INTO EmploymentKind(id,code,name) "
            "VALUES(20,'part_time','Совместительство')"));
    exec_sql(
        database,
        QStringLiteral(
            "INSERT INTO WorkSchedule("
            "id,name,kind,normative_minutes_per_day,shift_cycle)"
            " VALUES(20,'SixDay',1,420,'')"));
    exec_sql(
        database,
        QStringLiteral(
            "INSERT INTO TerminationReason(id,code,name) "
            "VALUES(20,'own','Собственное желание')"));
    return 20;
}

}

TEST(employee_operations_transfer_updates_employee_and_history) {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    mk::Database database =
        make_database(directory, QStringLiteral("transfer.sqlite"));
    create_references(database);

    const int64_t employee_id =
        hire(database, make_employee(1, 1, "001", "Иванов"));

    const int64_t event_id = mk::transfer_employee(
        database,
        1,
        employee_id,
        mk::Date(2026, 2, 1),
        20,
        20,
        20,
        20,
        std::nullopt);

    const auto saved = mk::read_employee(database, employee_id);
    REQUIRE(saved.has_value());
    REQUIRE_EQUAL(saved->department_id, int64_t{20});
    REQUIRE_EQUAL(saved->position_id, int64_t{20});
    REQUIRE_EQUAL(saved->employment_kind_id, int64_t{20});
    REQUIRE_EQUAL(saved->work_schedule_id, int64_t{20});
    REQUIRE(!saved->payroll_preset_id.has_value());
    REQUIRE_EQUAL(saved->rate, mk::Money(100000));
    REQUIRE_EQUAL(saved->personnel_number, std::string("001"));
    REQUIRE(!saved->termination_date.has_value());

    const auto event = mk::read_employment_event(database, event_id);
    REQUIRE(event.has_value());
    REQUIRE_EQUAL(event->kind, mk::EventKind::Transfer);
    REQUIRE_EQUAL(event->date, mk::Date(2026, 2, 1));
    REQUIRE_EQUAL(event->employee_id, employee_id);
    REQUIRE_EQUAL(event->department_id, std::optional<int64_t>{20});
    REQUIRE_EQUAL(event->position_id, std::optional<int64_t>{20});
    REQUIRE_EQUAL(event->employment_kind_id, std::optional<int64_t>{20});
    REQUIRE_EQUAL(event->work_schedule_id, std::optional<int64_t>{20});
    REQUIRE(!event->payroll_preset_id.has_value());
    REQUIRE_EQUAL(event->rate, std::optional<mk::Money>(mk::Money(100000)));
    REQUIRE_EQUAL(event->org_id, std::optional<int64_t>{1});
    REQUIRE_EQUAL(event->personnel_number,
                  std::optional<std::string>("001"));
    REQUIRE_EQUAL(event->full_name,
                  std::optional<std::string>("Иванов"));

    const auto events =
        mk::list_employment_events(database, employee_id);
    REQUIRE_EQUAL(events.size(), std::size_t{2});
    REQUIRE_EQUAL(events[0].kind, mk::EventKind::Hire);
    REQUIRE_EQUAL(events[1].kind, mk::EventKind::Transfer);
}

TEST(employee_operations_change_rate_updates_only_rate) {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    mk::Database database =
        make_database(directory, QStringLiteral("rate.sqlite"));
    create_references(database);

    const int64_t employee_id =
        hire(database, make_employee(1, 1, "001", "Петров"));

    const int64_t event_id = mk::change_employee_rate(
        database,
        1,
        employee_id,
        mk::Date(2026, 3, 1),
        mk::Money(250001),
        mk::RateUnit::Hour);

    const auto saved = mk::read_employee(database, employee_id);
    REQUIRE(saved.has_value());
    REQUIRE_EQUAL(saved->rate, mk::Money(250001));
    REQUIRE_EQUAL(saved->rate_unit, mk::RateUnit::Hour);
    REQUIRE_EQUAL(saved->department_id, int64_t{1});
    REQUIRE_EQUAL(saved->position_id, int64_t{1});
    REQUIRE_EQUAL(saved->payroll_preset_id, std::optional<int64_t>{1});

    const auto event = mk::read_employment_event(database, event_id);
    REQUIRE(event.has_value());
    REQUIRE_EQUAL(event->kind, mk::EventKind::ChangeSalary);
    REQUIRE_EQUAL(event->rate,
                  std::optional<mk::Money>(mk::Money(250001)));
    REQUIRE_EQUAL(event->rate_unit,
                  std::optional<mk::RateUnit>(mk::RateUnit::Hour));
    REQUIRE_EQUAL(event->department_id, std::optional<int64_t>{1});
    REQUIRE_EQUAL(event->position_id, std::optional<int64_t>{1});
}

TEST(employee_operations_reject_negative_rate) {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    mk::Database database =
        make_database(directory, QStringLiteral("neg.sqlite"));

    const int64_t employee_id =
        hire(database, make_employee(1, 1, "001", "Сидоров"));

    REQUIRE_THROWS(mk::change_employee_rate(
        database,
        1,
        employee_id,
        mk::Date(2026, 3, 1),
        mk::Money(-1),
        mk::RateUnit::Month));

    const auto saved = mk::read_employee(database, employee_id);
    REQUIRE_EQUAL(saved->rate, mk::Money(100000));
    REQUIRE_EQUAL(
        count_rows(database, QStringLiteral("EmploymentEvent")),
        int64_t{1});
}

TEST(employee_operations_terminate_marks_employee_and_keeps_record) {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    mk::Database database =
        make_database(directory, QStringLiteral("terminate.sqlite"));
    create_references(database);

    const int64_t employee_id =
        hire(database, make_employee(1, 1, "001", "Козлов"));

    const int64_t event_id = mk::terminate_employee(
        database,
        1,
        employee_id,
        mk::Date(2026, 4, 15),
        std::optional<int64_t>{20});

    const auto saved = mk::read_employee(database, employee_id);
    REQUIRE(saved.has_value());
    REQUIRE_EQUAL(saved->termination_date,
                  std::optional<mk::Date>(mk::Date(2026, 4, 15)));
    REQUIRE_EQUAL(saved->full_name, std::string("Козлов"));

    const auto event = mk::read_employment_event(database, event_id);
    REQUIRE(event.has_value());
    REQUIRE_EQUAL(event->kind, mk::EventKind::Termination);
    REQUIRE_EQUAL(event->date, mk::Date(2026, 4, 15));
    REQUIRE_EQUAL(event->termination_reason_id,
                  std::optional<int64_t>{20});
    REQUIRE_EQUAL(event->department_id, std::optional<int64_t>{1});

    const auto listed = mk::list_employees(database, 1);
    REQUIRE_EQUAL(listed.size(), std::size_t{1});
    REQUIRE(listed[0].termination_date.has_value());
}

TEST(employee_operations_terminate_without_reason_is_allowed) {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    mk::Database database =
        make_database(directory, QStringLiteral("no_reason.sqlite"));

    const int64_t employee_id =
        hire(database, make_employee(1, 1, "001", "Без причины"));

    const int64_t event_id = mk::terminate_employee(
        database,
        1,
        employee_id,
        mk::Date(2026, 4, 15),
        std::nullopt);

    const auto event = mk::read_employment_event(database, event_id);
    REQUIRE(event.has_value());
    REQUIRE(!event->termination_reason_id.has_value());
    REQUIRE(mk::read_employee(database, employee_id)
        ->termination_date.has_value());
}

TEST(employee_operations_reject_terminated_employee) {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    mk::Database database =
        make_database(directory, QStringLiteral("terminated.sqlite"));
    create_references(database);

    const int64_t employee_id =
        hire(database, make_employee(1, 1, "001", "Уволен"));

    mk::terminate_employee(
        database, 1, employee_id, mk::Date(2026, 4, 15), std::nullopt);

    const int64_t events_after_termination =
        count_rows(database, QStringLiteral("EmploymentEvent"));

    REQUIRE_THROWS(mk::transfer_employee(
        database, 1, employee_id,
        mk::Date(2026, 5, 1), 20, 20, 20, 20, std::nullopt));
    REQUIRE_THROWS(mk::change_employee_rate(
        database, 1, employee_id,
        mk::Date(2026, 5, 1), mk::Money(1), mk::RateUnit::Month));
    REQUIRE_THROWS(mk::terminate_employee(
        database, 1, employee_id,
        mk::Date(2026, 5, 1), std::nullopt));
    REQUIRE_THROWS(mk::update_employee_identity(
        database, 1, employee_id, "009", "Другое имя"));

    REQUIRE_EQUAL(
        count_rows(database, QStringLiteral("EmploymentEvent")),
        events_after_termination);
}

TEST(employee_operations_reject_other_organization) {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    mk::Database database =
        make_database(directory, QStringLiteral("orgs.sqlite"));

    exec_sql(
        database,
        QStringLiteral(
            "INSERT INTO Org(id,name) VALUES(2,'Вторая')"));
    exec_sql(
        database,
        QStringLiteral(
            "INSERT INTO Department(id,org_id,name) "
            "VALUES(20,2,'Второе подразделение')"));

    const int64_t employee_id =
        hire(database, make_employee(2, 20, "001", "Чужой"));

    REQUIRE_THROWS(mk::transfer_employee(
        database, 1, employee_id,
        mk::Date(2026, 2, 1), 1, 1, 1, 1, std::nullopt));
    REQUIRE_THROWS(mk::change_employee_rate(
        database, 1, employee_id,
        mk::Date(2026, 2, 1), mk::Money(1), mk::RateUnit::Month));
    REQUIRE_THROWS(mk::terminate_employee(
        database, 1, employee_id,
        mk::Date(2026, 2, 1), std::nullopt));
    REQUIRE_THROWS(mk::update_employee_identity(
        database, 1, employee_id, "002", "Переименован"));

    const auto saved = mk::read_employee(database, employee_id);
    REQUIRE_EQUAL(saved->personnel_number, std::string("001"));
    REQUIRE(!saved->termination_date.has_value());
    REQUIRE_EQUAL(
        count_rows(database, QStringLiteral("EmploymentEvent")),
        int64_t{1});
}

TEST(employee_operations_reject_date_before_hire) {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    mk::Database database =
        make_database(directory, QStringLiteral("dates.sqlite"));
    create_references(database);

    const int64_t employee_id =
        hire(database, make_employee(1, 1, "001", "Дата"));
    const mk::Date before_hire(2025, 12, 31);

    REQUIRE_THROWS(mk::transfer_employee(
        database, 1, employee_id,
        before_hire, 20, 20, 20, 20, std::nullopt));
    REQUIRE_THROWS(mk::change_employee_rate(
        database, 1, employee_id,
        before_hire, mk::Money(1), mk::RateUnit::Month));
    REQUIRE_THROWS(mk::terminate_employee(
        database, 1, employee_id, before_hire, std::nullopt));

    const auto saved = mk::read_employee(database, employee_id);
    REQUIRE_EQUAL(saved->rate, mk::Money(100000));
    REQUIRE_EQUAL(saved->department_id, int64_t{1});
    REQUIRE(!saved->termination_date.has_value());
    REQUIRE_EQUAL(
        count_rows(database, QStringLiteral("EmploymentEvent")),
        int64_t{1});
}

TEST(employee_operations_allow_hire_date_and_future_dates) {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    mk::Database database =
        make_database(directory, QStringLiteral("ok_dates.sqlite"));
    create_references(database);

    const int64_t employee_id =
        hire(database, make_employee(1, 1, "001", "Сотрудник"));

    mk::change_employee_rate(
        database, 1, employee_id,
        mk::Date(2026, 1, 1), mk::Money(120000), mk::RateUnit::Month);

    mk::transfer_employee(
        database, 1, employee_id,
        mk::Date(2030, 1, 10), 20, 20, 20, 20, 1);

    const auto saved = mk::read_employee(database, employee_id);
    REQUIRE_EQUAL(saved->rate, mk::Money(120000));
    REQUIRE_EQUAL(saved->department_id, int64_t{20});
    REQUIRE_EQUAL(saved->work_schedule_id, int64_t{20});
    REQUIRE_EQUAL(saved->payroll_preset_id, std::optional<int64_t>{1});

    const auto events =
        mk::list_employment_events(database, employee_id);
    REQUIRE_EQUAL(events.size(), std::size_t{3});
    REQUIRE_EQUAL(events[1].kind, mk::EventKind::ChangeSalary);
    REQUIRE_EQUAL(events[2].kind, mk::EventKind::Transfer);
}

TEST(employee_operations_transfer_is_atomic_on_event_failure) {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    mk::Database database =
        make_database(directory, QStringLiteral("atomic_tr.sqlite"));
    create_references(database);

    const int64_t employee_id =
        hire(database, make_employee(1, 1, "001", "Атомар"));

    exec_sql(
        database,
        QStringLiteral(
            "CREATE TRIGGER reject_transfer BEFORE INSERT "
            "ON EmploymentEvent WHEN NEW.kind=2 "
            "BEGIN SELECT RAISE(ABORT,'transfer rejected'); END"));

    REQUIRE_THROWS(mk::transfer_employee(
        database, 1, employee_id,
        mk::Date(2026, 2, 1), 20, 20, 20, 20, std::nullopt));

    const auto saved = mk::read_employee(database, employee_id);
    REQUIRE_EQUAL(saved->department_id, int64_t{1});
    REQUIRE_EQUAL(saved->position_id, int64_t{1});
    REQUIRE_EQUAL(
        count_rows(database, QStringLiteral("EmploymentEvent")),
        int64_t{1});
}

TEST(employee_operations_terminate_is_atomic_on_update_failure) {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    mk::Database database =
        make_database(directory, QStringLiteral("atomic_term.sqlite"));

    const int64_t employee_id =
        hire(database, make_employee(1, 1, "001", "Атомар"));

    exec_sql(
        database,
        QStringLiteral(
            "CREATE TRIGGER reject_termination BEFORE UPDATE ON Employee "
            "WHEN NEW.termination_date IS NOT NULL "
            "BEGIN SELECT RAISE(ABORT,'termination rejected'); END"));

    REQUIRE_THROWS(mk::terminate_employee(
        database, 1, employee_id,
        mk::Date(2026, 4, 15), std::nullopt));

    const auto saved = mk::read_employee(database, employee_id);
    REQUIRE(!saved->termination_date.has_value());
    REQUIRE_EQUAL(
        count_rows(database, QStringLiteral("EmploymentEvent")),
        int64_t{1});
}

TEST(employee_operations_identity_update_changes_only_number_and_name) {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    mk::Database database =
        make_database(directory, QStringLiteral("identity.sqlite"));

    const int64_t employee_id =
        hire(database, make_employee(1, 1, "001", "Старое имя"));

    mk::update_employee_identity(
        database, 1, employee_id, "0077", "Новое имя");

    const auto saved = mk::read_employee(database, employee_id);
    REQUIRE_EQUAL(saved->personnel_number, std::string("0077"));
    REQUIRE_EQUAL(saved->full_name, std::string("Новое имя"));
    REQUIRE_EQUAL(saved->department_id, int64_t{1});
    REQUIRE_EQUAL(saved->rate, mk::Money(100000));

    REQUIRE_EQUAL(
        count_rows(database, QStringLiteral("EmploymentEvent")),
        int64_t{1});
}

TEST(employee_operations_identity_rejects_empty_values) {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    mk::Database database =
        make_database(directory, QStringLiteral("identity_empty.sqlite"));

    const int64_t employee_id =
        hire(database, make_employee(1, 1, "001", "Имя"));

    REQUIRE_THROWS(mk::update_employee_identity(
        database, 1, employee_id, "", "Имя"));
    REQUIRE_THROWS(mk::update_employee_identity(
        database, 1, employee_id, "002", ""));

    const auto saved = mk::read_employee(database, employee_id);
    REQUIRE_EQUAL(saved->personnel_number, std::string("001"));
    REQUIRE_EQUAL(saved->full_name, std::string("Имя"));
}

TEST(employee_operations_missing_employee_throws) {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    mk::Database database =
        make_database(directory, QStringLiteral("missing.sqlite"));

    REQUIRE_THROWS(mk::transfer_employee(
        database, 1, 999999,
        mk::Date(2026, 2, 1), 1, 1, 1, 1, std::nullopt));
    REQUIRE_THROWS(mk::change_employee_rate(
        database, 1, 999999,
        mk::Date(2026, 2, 1), mk::Money(1), mk::RateUnit::Month));
    REQUIRE_THROWS(mk::terminate_employee(
        database, 1, 999999, mk::Date(2026, 2, 1), std::nullopt));
    REQUIRE_THROWS(mk::update_employee_identity(
        database, 1, 999999, "x", "y"));
}

TEST(employee_operations_termination_reasons_listed) {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    mk::Database database =
        make_database(directory, QStringLiteral("reasons.sqlite"));

    REQUIRE(mk::list_termination_reasons(database).empty());

    exec_sql(
        database,
        QStringLiteral(
            "INSERT INTO TerminationReason(id,code,name) "
            "VALUES(30,'own','Собственное желание')"));

    const auto reasons = mk::list_termination_reasons(database);
    REQUIRE_EQUAL(reasons.size(), std::size_t{1});
    REQUIRE_EQUAL(reasons[0].id, int64_t{30});
    REQUIRE_EQUAL(reasons[0].code, std::string("own"));
    REQUIRE_EQUAL(
        reasons[0].name,
        std::string("Собственное желание"));
}

TEST(employee_operations_reject_transfer_to_other_organization_department) {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    mk::Database database =
        make_database(directory, QStringLiteral("department_org.sqlite"));
    create_references(database);
    exec_sql(
        database,
        QStringLiteral("INSERT INTO Org(id,name) VALUES(2,'Вторая')"));
    exec_sql(
        database,
        QStringLiteral(
            "INSERT INTO Department(id,org_id,name) "
            "VALUES(30,2,'Чужой отдел')"));

    const int64_t employee_id =
        hire(database, make_employee(1, 1, "001", "Сотрудник"));

    REQUIRE_THROWS(mk::transfer_employee(
        database, 1, employee_id,
        mk::Date(2026, 2, 1), 30, 20, 20, 20, std::nullopt));

    const auto saved = mk::read_employee(database, employee_id);
    REQUIRE(saved.has_value());
    REQUIRE_EQUAL(saved->department_id, int64_t{1});
    REQUIRE_EQUAL(
        count_rows(database, QStringLiteral("EmploymentEvent")),
        int64_t{1});
}
