#include "mk/repositories.hpp"

#include "mk/calendar.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include <algorithm>
#include <functional>
#include <stdexcept>

namespace mk {
namespace {

void check(QSqlQuery& query) {
    if (!query.exec()) {
        throw std::runtime_error(query.lastError().text().toStdString());
    }
}

int64_t id(QSqlQuery& query) {
    return query.lastInsertId().toLongLong();
}

QVariant optional_value(const std::optional<int64_t>& value) {
    return value.has_value() ? QVariant::fromValue(*value) : QVariant();
}

QVariant optional_value(const std::optional<std::string>& value) {
    return value.has_value()
        ? QVariant(QString::fromStdString(*value))
        : QVariant();
}

QVariant optional_value(const std::optional<Money>& value) {
    return value.has_value()
        ? QVariant::fromValue(value->cents)
        : QVariant();
}

QVariant optional_value(const std::optional<RateUnit>& value) {
    return value.has_value()
        ? QVariant::fromValue(static_cast<int32_t>(*value))
        : QVariant();
}

QVariant optional_date(const std::optional<Date>& value) {
    return value.has_value()
        ? QVariant::fromValue(value->ordinal())
        : QVariant();
}

QString text(QSqlQuery& query, int index) {
    return query.value(index).toString();
}

std::optional<std::string> optional_text(
    QSqlQuery& query,
    int index) {
    return query.value(index).isNull()
        ? std::nullopt
        : std::optional<std::string>(
            text(query, index).toStdString());
}

std::optional<int64_t> optional_id(
    QSqlQuery& query,
    int index) {
    return query.value(index).isNull()
        ? std::nullopt
        : std::optional<int64_t>(
            query.value(index).toLongLong());
}

Org org_from(QSqlQuery& query) {
    return {
        query.value(0).toLongLong(),
        text(query, 1).toStdString()
    };
}

Position position_from(QSqlQuery& query) {
    return {
        query.value(0).toLongLong(),
        text(query, 1).toStdString()
    };
}

WorkSchedule schedule_from(QSqlQuery& query) {
    WorkSchedule result;
    result.id = query.value(0).toLongLong();
    result.name = text(query, 1).toStdString();
    result.kind =
        static_cast<ScheduleKind>(query.value(2).toInt());
    result.normative_minutes_per_day =
        query.value(3).toInt();
    result.shift_cycle =
        text(query, 4).toStdString();

    if (!query.value(5).isNull()) {
        result.anchor_date =
            Date::from_ordinal(query.value(5).toInt());
    }

    return result;
}

Employee employee_from(QSqlQuery& query) {
    Employee result;
    result.id = query.value(0).toLongLong();
    result.personnel_number =
        text(query, 1).toStdString();
    result.full_name =
        text(query, 2).toStdString();
    result.org_id = query.value(3).toLongLong();
    result.department_id = query.value(4).toLongLong();
    result.position_id = query.value(5).toLongLong();
    result.employment_kind_id =
        query.value(6).toLongLong();
    result.work_schedule_id =
        query.value(7).toLongLong();
    result.payroll_preset_id = optional_id(query, 8);
    result.rate = Money(query.value(9).toLongLong());
    result.rate_unit =
        static_cast<RateUnit>(query.value(10).toInt());
    result.hire_date =
        Date::from_ordinal(query.value(11).toInt());

    if (!query.value(12).isNull()) {
        result.termination_date =
            Date::from_ordinal(query.value(12).toInt());
    }

    return result;
}

EmployeeSlice employee_slice_from(
    const Employee& employee) {
    EmployeeSlice result;
    result.employee_id = employee.id;
    result.personnel_number = employee.personnel_number;
    result.full_name = employee.full_name;
    result.org_id = employee.org_id;
    result.department_id = employee.department_id;
    result.position_id = employee.position_id;
    result.employment_kind_id =
        employee.employment_kind_id;
    result.work_schedule_id =
        employee.work_schedule_id;
    result.payroll_preset_id =
        employee.payroll_preset_id;
    result.rate = employee.rate;
    result.rate_unit = employee.rate_unit;
    return result;
}

EmploymentEvent event_from(QSqlQuery& query) {
    EmploymentEvent result;
    result.employee_id =
        query.value(1).toLongLong();
    result.date =
        Date::from_ordinal(query.value(2).toInt());
    result.kind =
        static_cast<EventKind>(query.value(3).toInt());
    result.personnel_number =
        optional_text(query, 4);
    result.full_name =
        optional_text(query, 5);
    result.org_id = optional_id(query, 6);
    result.department_id = optional_id(query, 7);
    result.position_id = optional_id(query, 8);
    result.employment_kind_id = optional_id(query, 9);
    result.work_schedule_id = optional_id(query, 10);
    result.payroll_preset_id = optional_id(query, 11);

    if (!query.value(12).isNull()) {
        result.rate =
            Money(query.value(12).toLongLong());
    }

    if (!query.value(13).isNull()) {
        result.rate_unit =
            static_cast<RateUnit>(
                query.value(13).toInt());
    }

    result.termination_reason_id =
        optional_id(query, 14);
    return result;
}

TimeFact fact_from(QSqlQuery& query) {
    TimeFact result;
    result.id =
        query.value(0).toLongLong();
    result.employee_id =
        query.value(1).toLongLong();
    result.date =
        Date::from_ordinal(query.value(2).toInt());
    result.worked_minutes =
        query.value(3).toInt();
    result.absence_kind_id =
        optional_id(query, 4);
    return result;
}

template <typename T, typename Function>
std::vector<T> query_list(
    Database& database,
    const QString& sql,
    Function&& convert) {
    QSqlQuery query(database.sql());
    query.prepare(sql);
    check(query);

    std::vector<T> result;
    while (query.next()) {
        result.push_back(convert(query));
    }
    return result;
}

void require_affected(
    const QSqlQuery& query,
    const char* message) {
    if (query.numRowsAffected() == 0) {
        throw std::runtime_error(message);
    }
}

}

int64_t create_org(Database& database, const Org& value) {
    QSqlQuery query(database.sql());
    query.prepare("INSERT INTO Org(name) VALUES(?)");
    query.addBindValue(QString::fromStdString(value.name));
    check(query);
    return id(query);
}

std::optional<Org> read_org(
    Database& database,
    int64_t value) {
    QSqlQuery query(database.sql());
    query.prepare("SELECT id,name FROM Org WHERE id=?");
    query.addBindValue(value);
    check(query);
    return query.next()
        ? std::optional<Org>(org_from(query))
        : std::nullopt;
}

void update_org(Database& database, const Org& value) {
    QSqlQuery query(database.sql());
    query.prepare("UPDATE Org SET name=? WHERE id=?");
    query.addBindValue(QString::fromStdString(value.name));
    query.addBindValue(value.id);
    check(query);
    require_affected(query, "organization not found");
}

void delete_org(Database& database, int64_t value) {
    QSqlQuery query(database.sql());
    query.prepare("DELETE FROM Org WHERE id=?");
    query.addBindValue(value);
    check(query);
    require_affected(query, "organization not found");
}

std::vector<Org> list_orgs(Database& database) {
    return query_list<Org>(
        database,
        "SELECT id,name FROM Org ORDER BY id",
        org_from);
}

std::vector<Department> list_departments(
    Database& database,
    int64_t org_id) {
    QSqlQuery query(database.sql());
    query.prepare(
        "SELECT id,org_id,name FROM Department "
        "WHERE org_id=? ORDER BY id");
    query.addBindValue(org_id);
    check(query);

    std::vector<Department> result;
    while (query.next()) {
        result.push_back({
            query.value(0).toLongLong(),
            query.value(1).toLongLong(),
            text(query, 2).toStdString()
        });
    }
    return result;
}

int64_t create_position(
    Database& database,
    const Position& value) {
    QSqlQuery query(database.sql());
    query.prepare("INSERT INTO Position(name) VALUES(?)");
    query.addBindValue(QString::fromStdString(value.name));
    check(query);
    return id(query);
}

std::optional<Position> read_position(
    Database& database,
    int64_t value) {
    QSqlQuery query(database.sql());
    query.prepare(
        "SELECT id,name FROM Position WHERE id=?");
    query.addBindValue(value);
    check(query);
    return query.next()
        ? std::optional<Position>(position_from(query))
        : std::nullopt;
}

void update_position(
    Database& database,
    const Position& value) {
    QSqlQuery query(database.sql());
    query.prepare(
        "UPDATE Position SET name=? WHERE id=?");
    query.addBindValue(
        QString::fromStdString(value.name));
    query.addBindValue(value.id);
    check(query);
    require_affected(query, "position not found");
}

void delete_position(Database& database, int64_t value) {
    QSqlQuery query(database.sql());
    query.prepare("DELETE FROM Position WHERE id=?");
    query.addBindValue(value);
    check(query);
    require_affected(query, "position not found");
}

std::vector<Position> list_positions(
    Database& database) {
    return query_list<Position>(
        database,
        "SELECT id,name FROM Position ORDER BY id",
        position_from);
}

std::vector<EmploymentKind> list_employment_kinds(
    Database& database) {
    QSqlQuery query(database.sql());
    query.prepare(
        "SELECT id,code,name FROM EmploymentKind ORDER BY id");
    check(query);

    std::vector<EmploymentKind> result;
    while (query.next()) {
        result.push_back({
            query.value(0).toLongLong(),
            text(query, 1).toStdString(),
            text(query, 2).toStdString()
        });
    }
    return result;
}

int64_t create_work_schedule(
    Database& database,
    const WorkSchedule& value) {
    QSqlQuery query(database.sql());
    query.prepare(
        "INSERT INTO WorkSchedule("
        "name,kind,normative_minutes_per_day,"
        "shift_cycle,anchor_date) VALUES(?,?,?,?,?)");
    query.addBindValue(
        QString::fromStdString(value.name));
    query.addBindValue(static_cast<int>(value.kind));
    query.addBindValue(
        value.normative_minutes_per_day);
    query.addBindValue(
        QString::fromStdString(value.shift_cycle));
    query.addBindValue(optional_date(value.anchor_date));
    check(query);
    return id(query);
}

std::optional<WorkSchedule> read_work_schedule(
    Database& database,
    int64_t value) {
    QSqlQuery query(database.sql());
    query.prepare(
        "SELECT id,name,kind,"
        "normative_minutes_per_day,shift_cycle,"
        "anchor_date FROM WorkSchedule WHERE id=?");
    query.addBindValue(value);
    check(query);
    return query.next()
        ? std::optional<WorkSchedule>(
            schedule_from(query))
        : std::nullopt;
}

void update_work_schedule(
    Database& database,
    const WorkSchedule& value) {
    QSqlQuery query(database.sql());
    query.prepare(
        "UPDATE WorkSchedule SET name=?,kind=?,"
        "normative_minutes_per_day=?,shift_cycle=?,"
        "anchor_date=? WHERE id=?");
    query.addBindValue(
        QString::fromStdString(value.name));
    query.addBindValue(static_cast<int>(value.kind));
    query.addBindValue(
        value.normative_minutes_per_day);
    query.addBindValue(
        QString::fromStdString(value.shift_cycle));
    query.addBindValue(optional_date(value.anchor_date));
    query.addBindValue(value.id);
    check(query);
    require_affected(query, "work schedule not found");
}

void delete_work_schedule(
    Database& database,
    int64_t value) {
    QSqlQuery query(database.sql());
    query.prepare(
        "DELETE FROM WorkSchedule WHERE id=?");
    query.addBindValue(value);
    check(query);
    require_affected(query, "work schedule not found");
}

std::vector<WorkSchedule> list_work_schedules(
    Database& database) {
    return query_list<WorkSchedule>(
        database,
        "SELECT id,name,kind,"
        "normative_minutes_per_day,shift_cycle,"
        "anchor_date FROM WorkSchedule ORDER BY id",
        schedule_from);
}

std::vector<WorkSchedule> list_work_schedules(
    Database& database,
    int64_t) {
    return list_work_schedules(database);
}

int64_t create_employee(
    Database& database,
    const Employee& value) {
    QSqlQuery query(database.sql());
    query.prepare(
        "INSERT INTO Employee("
        "personnel_number,full_name,org_id,"
        "department_id,position_id,"
        "employment_kind_id,work_schedule_id,"
        "payroll_preset_id,rate_cents,rate_unit,"
        "hire_date,termination_date)"
        " VALUES(?,?,?,?,?,?,?,?,?,?,?,?)");
    query.addBindValue(
        QString::fromStdString(value.personnel_number));
    query.addBindValue(
        QString::fromStdString(value.full_name));
    query.addBindValue(value.org_id);
    query.addBindValue(value.department_id);
    query.addBindValue(value.position_id);
    query.addBindValue(value.employment_kind_id);
    query.addBindValue(value.work_schedule_id);
    query.addBindValue(
        optional_value(value.payroll_preset_id));
    query.addBindValue(value.rate.cents);
    query.addBindValue(
        static_cast<int>(value.rate_unit));
    query.addBindValue(value.hire_date.ordinal());
    query.addBindValue(
        optional_date(value.termination_date));
    check(query);
    return id(query);
}

std::optional<Employee> read_employee(
    Database& database,
    int64_t value) {
    QSqlQuery query(database.sql());
    query.prepare(
        "SELECT id,personnel_number,full_name,"
        "org_id,department_id,position_id,"
        "employment_kind_id,work_schedule_id,"
        "payroll_preset_id,rate_cents,rate_unit,"
        "hire_date,termination_date "
        "FROM Employee WHERE id=?");
    query.addBindValue(value);
    check(query);
    return query.next()
        ? std::optional<Employee>(employee_from(query))
        : std::nullopt;
}

void update_employee(
    Database& database,
    const Employee& value) {
    QSqlQuery query(database.sql());
    query.prepare(
        "UPDATE Employee SET personnel_number=?,"
        "full_name=?,org_id=?,department_id=?,"
        "position_id=?,employment_kind_id=?,"
        "work_schedule_id=?,payroll_preset_id=?,"
        "rate_cents=?,rate_unit=?,hire_date=?,"
        "termination_date=? WHERE id=?");
    query.addBindValue(
        QString::fromStdString(value.personnel_number));
    query.addBindValue(
        QString::fromStdString(value.full_name));
    query.addBindValue(value.org_id);
    query.addBindValue(value.department_id);
    query.addBindValue(value.position_id);
    query.addBindValue(value.employment_kind_id);
    query.addBindValue(value.work_schedule_id);
    query.addBindValue(
        optional_value(value.payroll_preset_id));
    query.addBindValue(value.rate.cents);
    query.addBindValue(
        static_cast<int>(value.rate_unit));
    query.addBindValue(value.hire_date.ordinal());
    query.addBindValue(
        optional_date(value.termination_date));
    query.addBindValue(value.id);
    check(query);
    require_affected(query, "employee not found");
}

void delete_employee(Database& database, int64_t value) {
    QSqlQuery query(database.sql());
    query.prepare("DELETE FROM Employee WHERE id=?");
    query.addBindValue(value);
    check(query);
    require_affected(query, "employee not found");
}

std::vector<Employee> list_employees(
    Database& database,
    int64_t org_id) {
    QSqlQuery query(database.sql());
    query.prepare(
        "SELECT id,personnel_number,full_name,"
        "org_id,department_id,position_id,"
        "employment_kind_id,work_schedule_id,"
        "payroll_preset_id,rate_cents,rate_unit,"
        "hire_date,termination_date FROM Employee "
        "WHERE org_id=? ORDER BY id");
    query.addBindValue(org_id);
    check(query);

    std::vector<Employee> result;
    while (query.next()) {
        result.push_back(employee_from(query));
    }
    return result;
}

int64_t create_employment_event(
    Database& database,
    const EmploymentEvent& value) {
    QSqlQuery query(database.sql());
    query.prepare(
        "INSERT INTO EmploymentEvent("
        "employee_id,event_date,kind,"
        "personnel_number,full_name,org_id,"
        "department_id,position_id,"
        "employment_kind_id,work_schedule_id,"
        "payroll_preset_id,rate_cents,rate_unit,"
        "termination_reason_id)"
        " VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
    query.addBindValue(value.employee_id);
    query.addBindValue(value.date.ordinal());
    query.addBindValue(static_cast<int>(value.kind));
    query.addBindValue(
        optional_value(value.personnel_number));
    query.addBindValue(
        optional_value(value.full_name));
    query.addBindValue(optional_value(value.org_id));
    query.addBindValue(
        optional_value(value.department_id));
    query.addBindValue(
        optional_value(value.position_id));
    query.addBindValue(
        optional_value(value.employment_kind_id));
    query.addBindValue(
        optional_value(value.work_schedule_id));
    query.addBindValue(
        optional_value(value.payroll_preset_id));
    query.addBindValue(optional_value(value.rate));
    query.addBindValue(
        optional_value(value.rate_unit));
    query.addBindValue(
        optional_value(value.termination_reason_id));
    check(query);
    return id(query);
}

std::optional<EmploymentEvent> read_employment_event(
    Database& database,
    int64_t value) {
    QSqlQuery query(database.sql());
    query.prepare(
        "SELECT id,employee_id,event_date,kind,"
        "personnel_number,full_name,org_id,"
        "department_id,position_id,"
        "employment_kind_id,work_schedule_id,"
        "payroll_preset_id,rate_cents,rate_unit,"
        "termination_reason_id "
        "FROM EmploymentEvent WHERE id=?");
    query.addBindValue(value);
    check(query);
    return query.next()
        ? std::optional<EmploymentEvent>(
            event_from(query))
        : std::nullopt;
}

void update_employment_event(
    Database& database,
    int64_t event_id,
    const EmploymentEvent& value) {
    QSqlQuery query(database.sql());
    query.prepare(
        "UPDATE EmploymentEvent SET employee_id=?,"
        "event_date=?,kind=?,personnel_number=?,"
        "full_name=?,org_id=?,department_id=?,"
        "position_id=?,employment_kind_id=?,"
        "work_schedule_id=?,payroll_preset_id=?,"
        "rate_cents=?,rate_unit=?,"
        "termination_reason_id=? WHERE id=?");
    query.addBindValue(value.employee_id);
    query.addBindValue(value.date.ordinal());
    query.addBindValue(static_cast<int>(value.kind));
    query.addBindValue(
        optional_value(value.personnel_number));
    query.addBindValue(
        optional_value(value.full_name));
    query.addBindValue(optional_value(value.org_id));
    query.addBindValue(
        optional_value(value.department_id));
    query.addBindValue(
        optional_value(value.position_id));
    query.addBindValue(
        optional_value(value.employment_kind_id));
    query.addBindValue(
        optional_value(value.work_schedule_id));
    query.addBindValue(
        optional_value(value.payroll_preset_id));
    query.addBindValue(optional_value(value.rate));
    query.addBindValue(
        optional_value(value.rate_unit));
    query.addBindValue(
        optional_value(value.termination_reason_id));
    query.addBindValue(event_id);
    check(query);
    require_affected(query, "employment event not found");
}

void delete_employment_event(
    Database& database,
    int64_t value) {
    QSqlQuery query(database.sql());
    query.prepare(
        "DELETE FROM EmploymentEvent WHERE id=?");
    query.addBindValue(value);
    check(query);
    require_affected(query, "employment event not found");
}

std::vector<EmploymentEvent> list_employment_events(
    Database& database,
    int64_t employee_id) {
    QSqlQuery query(database.sql());
    query.prepare(
        "SELECT id,employee_id,event_date,kind,"
        "personnel_number,full_name,org_id,"
        "department_id,position_id,"
        "employment_kind_id,work_schedule_id,"
        "payroll_preset_id,rate_cents,rate_unit,"
        "termination_reason_id FROM EmploymentEvent "
        "WHERE employee_id=? ORDER BY event_date,id");
    query.addBindValue(employee_id);
    check(query);

    std::vector<EmploymentEvent> result;
    while (query.next()) {
        result.push_back(event_from(query));
    }
    return result;
}

int64_t create_employee_with_hire_event(
    Database& database,
    const Employee& employee,
    const EmploymentEvent& hire_event) {
    if (hire_event.kind != EventKind::Hire) {
        throw std::invalid_argument("initial event must be a hire");
    }

    database.transaction();
    try {
        const int64_t employee_id = create_employee(database, employee);
        EmploymentEvent saved_event = hire_event;
        saved_event.employee_id = employee_id;
        (void)create_employment_event(database, saved_event);
        database.commit();
        return employee_id;
    } catch (...) {
        try {
            database.rollback();
        } catch (...) {
        }
        throw;
    }
}

namespace {

Employee require_active_employee(
    Database& database,
    int64_t org_id,
    int64_t employee_id) {
    const std::optional<Employee> employee =
        read_employee(database, employee_id);
    if (!employee.has_value() || employee->org_id != org_id) {
        throw std::runtime_error("employee not found");
    }
    if (employee->termination_date.has_value()) {
        throw std::runtime_error(
            "operations are not allowed for a terminated employee");
    }
    return *employee;
}

void require_operation_date(
    const Employee& employee,
    const Date& date) {
    if (date < employee.hire_date) {
        throw std::invalid_argument(
            "operation date cannot be earlier than the hire date");
    }
}

void require_department_in_org(
    Database& database,
    int64_t department_id,
    int64_t org_id) {
    QSqlQuery query(database.sql());
    query.prepare(
        "SELECT 1 FROM Department "
        "WHERE id=? AND org_id=?");
    query.addBindValue(department_id);
    query.addBindValue(org_id);
    check(query);
    if (!query.next()) {
        throw std::invalid_argument(
            "department does not belong to the employee organization");
    }
}

EmploymentEvent snapshot_event(
    const Employee& employee,
    const Date& date,
    EventKind kind) {
    EmploymentEvent event;
    event.employee_id = employee.id;
    event.date = date;
    event.kind = kind;
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

int64_t apply_employee_event(
    Database& database,
    int64_t org_id,
    int64_t employee_id,
    const Date& date,
    EventKind kind,
    const std::function<void(Employee&, EmploymentEvent&)>& change) {
    database.transaction();
    try {
        Employee employee =
            require_active_employee(database, org_id, employee_id);
        require_operation_date(employee, date);

        EmploymentEvent event =
            snapshot_event(employee, date, kind);
        change(employee, event);

        update_employee(database, employee);
        const int64_t event_id =
            create_employment_event(database, event);
        database.commit();
        return event_id;
    } catch (...) {
        try {
            database.rollback();
        } catch (...) {
        }
        throw;
    }
}

}

void update_employee_identity(
    Database& database,
    int64_t org_id,
    int64_t employee_id,
    const std::string& personnel_number,
    const std::string& full_name) {
    if (personnel_number.empty() || full_name.empty()) {
        throw std::invalid_argument(
            "personnel number and full name must not be empty");
    }

    database.transaction();
    try {
        Employee employee =
            require_active_employee(database, org_id, employee_id);
        employee.personnel_number = personnel_number;
        employee.full_name = full_name;
        update_employee(database, employee);
        database.commit();
    } catch (...) {
        try {
            database.rollback();
        } catch (...) {
        }
        throw;
    }
}

int64_t transfer_employee(
    Database& database,
    int64_t org_id,
    int64_t employee_id,
    const Date& date,
    int64_t department_id,
    int64_t position_id,
    int64_t employment_kind_id,
    int64_t work_schedule_id,
    std::optional<int64_t> payroll_preset_id) {
    return apply_employee_event(
        database,
        org_id,
        employee_id,
        date,
        EventKind::Transfer,
        [&](Employee& employee, EmploymentEvent& event) {
            require_department_in_org(database, department_id, org_id);
            employee.department_id = department_id;
            employee.position_id = position_id;
            employee.employment_kind_id = employment_kind_id;
            employee.work_schedule_id = work_schedule_id;
            employee.payroll_preset_id = payroll_preset_id;

            event.department_id = department_id;
            event.position_id = position_id;
            event.employment_kind_id = employment_kind_id;
            event.work_schedule_id = work_schedule_id;
            event.payroll_preset_id = payroll_preset_id;
        });
}

int64_t change_employee_rate(
    Database& database,
    int64_t org_id,
    int64_t employee_id,
    const Date& date,
    Money rate,
    RateUnit rate_unit) {
    if (rate.cents < 0) {
        throw std::invalid_argument("rate cannot be negative");
    }

    return apply_employee_event(
        database,
        org_id,
        employee_id,
        date,
        EventKind::ChangeSalary,
        [&](Employee& employee, EmploymentEvent& event) {
            employee.rate = rate;
            employee.rate_unit = rate_unit;
            event.rate = rate;
            event.rate_unit = rate_unit;
        });
}

int64_t terminate_employee(
    Database& database,
    int64_t org_id,
    int64_t employee_id,
    const Date& date,
    std::optional<int64_t> termination_reason_id) {
    return apply_employee_event(
        database,
        org_id,
        employee_id,
        date,
        EventKind::Termination,
        [&](Employee& employee, EmploymentEvent& event) {
            employee.termination_date = date;
            event.termination_reason_id = termination_reason_id;
        });
}

std::vector<TerminationReason> list_termination_reasons(
    Database& database) {
    QSqlQuery query(database.sql());
    query.prepare(
        "SELECT id,code,name "
        "FROM TerminationReason ORDER BY id");
    check(query);

    std::vector<TerminationReason> result;
    while (query.next()) {
        result.push_back({
            query.value(0).toLongLong(),
            text(query, 1).toStdString(),
            text(query, 2).toStdString()
        });
    }
    return result;
}

int64_t create_time_fact(
    Database& database,
    const TimeFact& value) {
    QSqlQuery query(database.sql());
    query.prepare(
        "INSERT INTO TimeFact("
        "employee_id,fact_date,worked_minutes,"
        "absence_kind_id) VALUES(?,?,?,?)");
    query.addBindValue(value.employee_id);
    query.addBindValue(value.date.ordinal());
    query.addBindValue(value.worked_minutes);
    query.addBindValue(
        optional_value(value.absence_kind_id));
    check(query);
    return id(query);
}

std::optional<TimeFact> read_time_fact(
    Database& database,
    int64_t value) {
    QSqlQuery query(database.sql());
    query.prepare(
        "SELECT id,employee_id,fact_date,"
        "worked_minutes,absence_kind_id "
        "FROM TimeFact WHERE id=?");
    query.addBindValue(value);
    check(query);
    return query.next()
        ? std::optional<TimeFact>(fact_from(query))
        : std::nullopt;
}

void update_time_fact(
    Database& database,
    int64_t fact_id,
    const TimeFact& value) {
    QSqlQuery query(database.sql());
    query.prepare(
        "UPDATE TimeFact SET employee_id=?,"
        "fact_date=?,worked_minutes=?,"
        "absence_kind_id=? WHERE id=?");
    query.addBindValue(value.employee_id);
    query.addBindValue(value.date.ordinal());
    query.addBindValue(value.worked_minutes);
    query.addBindValue(
        optional_value(value.absence_kind_id));
    query.addBindValue(fact_id);
    check(query);
    require_affected(query, "time fact not found");
}

void delete_time_fact(
    Database& database,
    int64_t value) {
    QSqlQuery query(database.sql());
    query.prepare("DELETE FROM TimeFact WHERE id=?");
    query.addBindValue(value);
    check(query);
    require_affected(query, "time fact not found");
}

std::vector<TimeFact> list_time_facts(
    Database& database,
    int64_t employee_id,
    Date from,
    Date to) {
    QSqlQuery query(database.sql());
    query.prepare(
        "SELECT id,employee_id,fact_date,"
        "worked_minutes,absence_kind_id "
        "FROM TimeFact WHERE employee_id=? "
        "AND fact_date>=? AND fact_date<=? "
        "ORDER BY fact_date,id");
    query.addBindValue(employee_id);
    query.addBindValue(from.ordinal());
    query.addBindValue(to.ordinal());
    check(query);

    std::vector<TimeFact> result;
    while (query.next()) {
        result.push_back(fact_from(query));
    }
    return result;
}

std::vector<AbsenceKind> list_absence_kinds(
    Database& database) {
    QSqlQuery query(database.sql());
    query.prepare(
        "SELECT id,code,name,paid "
        "FROM AbsenceKind ORDER BY id");
    check(query);

    std::vector<AbsenceKind> result;
    while (query.next()) {
        result.push_back({
            query.value(0).toLongLong(),
            query.value(1).toString().toStdString(),
            query.value(2).toString().toStdString(),
            query.value(3).toBool()
        });
    }
    return result;
}

std::vector<CalcItem> list_calc_items(
    Database& database) {
    QSqlQuery query(database.sql());
    query.prepare(
        "SELECT id,code,name,formula,role,stage,"
        "priority,taxable,included_in_contrib_base,"
        "reduces_income_tax_base,rounding_policy "
        "FROM CalcItem ORDER BY id");
    check(query);

    std::vector<CalcItem> result;
    while (query.next()) {
        CalcItem item;
        item.id = query.value(0).toLongLong();
        item.code =
            query.value(1).toString().toStdString();
        item.name =
            query.value(2).toString().toStdString();
        item.formula =
            query.value(3).toString().toStdString();
        item.role =
            static_cast<CalcRole>(
                query.value(4).toInt());
        item.stage = query.value(5).toInt();
        item.priority = query.value(6).toInt();
        item.taxable = query.value(7).toBool();
        item.included_in_contrib_base =
            query.value(8).toBool();
        item.reduces_income_tax_base =
            query.value(9).toBool();
        item.rounding_policy =
            static_cast<RoundingPolicy>(
                query.value(10).toInt());
        result.push_back(item);
    }
    return result;
}

std::vector<PayrollPreset> list_payroll_presets(
    Database& database) {
    QSqlQuery query(database.sql());
    query.prepare(
        "SELECT id,code,name "
        "FROM PayrollPreset ORDER BY id");
    check(query);

    std::vector<PayrollPreset> result;
    while (query.next()) {
        PayrollPreset preset{
            query.value(0).toLongLong(),
            query.value(1).toString().toStdString(),
            query.value(2).toString().toStdString(),
            {}
        };

        QSqlQuery items(database.sql());
        items.prepare(
            "SELECT calc_item_id "
            "FROM PayrollPresetItem "
            "WHERE preset_id=? ORDER BY calc_item_id");
        items.addBindValue(preset.id);
        check(items);

        while (items.next()) {
            preset.calc_item_ids.push_back(
                items.value(0).toLongLong());
        }
        result.push_back(preset);
    }
    return result;
}

CalcInput build_calc_input(
    Database& database,
    int64_t org_id,
    int32_t year,
    int32_t month) {
    CalcInput result;
    result.period.date_from = Date(year, month, 1);
    result.period.date_to = Date(
        year,
        month,
        Date::days_in_month(year, month));

    const std::vector<Employee> employees =
        list_employees(database, org_id);

    for (const Employee& employee : employees) {
        result.employees.push_back(
            employee_slice_from(employee));
    }

    result.work_schedules =
        list_work_schedules(database);
    result.payroll_presets =
        list_payroll_presets(database);
    result.calc_items =
        list_calc_items(database);
    result.absence_kinds =
        list_absence_kinds(database);

    result.termination_reasons =
        list_termination_reasons(database);

    for (const Employee& employee : employees) {
        const std::vector<EmploymentEvent> events =
            list_employment_events(database, employee.id);

        for (const EmploymentEvent& event : events) {
            if (event.date <= result.period.date_to) {
                result.employment_events.push_back(event);
            }
        }

        const std::vector<TimeFact> facts =
            list_time_facts(
                database,
                employee.id,
                result.period.date_from,
                result.period.date_to);

        result.time_facts.insert(
            result.time_facts.end(),
            facts.begin(),
            facts.end());

        QSqlQuery output_facts(database.sql());
        output_facts.prepare(
            "SELECT employee_id,fact_date,code,"
            "value_numerator,value_denominator "
            "FROM OutputFact WHERE employee_id=? "
            "AND fact_date>=? AND fact_date<=? "
            "ORDER BY fact_date,id");
        output_facts.addBindValue(employee.id);
        output_facts.addBindValue(
            result.period.date_from.ordinal());
        output_facts.addBindValue(
            result.period.date_to.ordinal());
        check(output_facts);

        while (output_facts.next()) {
            result.output_facts.push_back({
                output_facts.value(0).toLongLong(),
                Date::from_ordinal(
                    output_facts.value(1).toInt()),
                output_facts.value(2)
                    .toString().toStdString(),
                Rational(
                    output_facts.value(3).toLongLong(),
                    output_facts.value(4).toLongLong())
            });
        }

        const auto schedule = std::find_if(
            result.work_schedules.begin(),
            result.work_schedules.end(),
            [&](const WorkSchedule& value) {
                return value.id ==
                    employee.work_schedule_id;
            });

        if (schedule != result.work_schedules.end()) {
            const std::vector<NormDay> days =
                generate_norm_calendar(
                    *schedule,
                    year,
                    month);
            result.norm_days.insert(
                result.norm_days.end(),
                days.begin(),
                days.end());
        }
    }

    return result;
}

namespace {

void write_calculations(
    Database& database,
    int64_t period_id,
    const CalcOutput& output) {
    for (const Calculation& value :
         output.calculations) {
        QSqlQuery calculation(database.sql());
        calculation.prepare(
            "INSERT INTO Calculation("
            "employee_id,payroll_period_id,status,"
            "incomplete_reasons,total_accrual_cents,"
            "total_pre_tax_deduction_cents,"
            "income_tax_cents,"
            "total_employer_contribution_cents,"
            "total_post_tax_deduction_cents,"
            "net_amount_cents,engine_version,"
            "input_snapshot)"
            " VALUES(?,?,?,?,?,?,?,?,?,?,?,?)");

        QJsonArray reasons;
        for (const std::string& reason :
             value.incomplete_reasons) {
            reasons.append(
                QString::fromStdString(reason));
        }

        calculation.addBindValue(value.employee_id);
        calculation.addBindValue(period_id);
        calculation.addBindValue(
            static_cast<int>(value.status));
        calculation.addBindValue(
            QString::fromUtf8(
                QJsonDocument(reasons).toJson(
                    QJsonDocument::Compact)));
        calculation.addBindValue(
            value.total_accrual.cents);
        calculation.addBindValue(
            value.total_pre_tax_deduction.cents);
        calculation.addBindValue(
            value.income_tax.cents);
        calculation.addBindValue(
            value.total_employer_contribution.cents);
        calculation.addBindValue(
            value.total_post_tax_deduction.cents);
        calculation.addBindValue(
            value.net_amount.cents);
        calculation.addBindValue(
            QString::fromStdString(
                value.engine_version));
        calculation.addBindValue(
            value.input_snapshot.has_value()
                ? QVariant(QString::fromStdString(
                    *value.input_snapshot))
                : QVariant());
        check(calculation);

        const int64_t calculation_id =
            id(calculation);

        for (const CalculationLine& line :
             value.lines) {
            QSqlQuery item(database.sql());
            item.prepare(
                "INSERT INTO CalculationLine("
                "calculation_id,calc_item_id,"
                "calc_item_code,role,stage,priority,"
                "amount_cents,formula_snapshot,"
                "rounding_policy)"
                " VALUES(?,?,?,?,?,?,?,?,?)");
            item.addBindValue(calculation_id);
            item.addBindValue(line.calc_item_id);
            item.addBindValue(
                QString::fromStdString(
                    line.calc_item_code));
            item.addBindValue(
                static_cast<int>(line.role));
            item.addBindValue(line.stage);
            item.addBindValue(line.priority);
            item.addBindValue(line.amount.cents);
            item.addBindValue(
                QString::fromStdString(
                    line.formula_snapshot));
            item.addBindValue(
                static_cast<int>(
                    line.rounding_policy));
            check(item);
        }
    }
}

}

void persist_calculation(
    Database& database,
    const CalcOutput& output) {
    database.transaction();

    try {
        QSqlQuery period(database.sql());
        period.prepare(
            "INSERT INTO PayrollPeriod("
            "date_from,date_to,status) VALUES(?,?,?)");
        period.addBindValue(
            output.input_snapshot.period
                .date_from.ordinal());
        period.addBindValue(
            output.input_snapshot.period
                .date_to.ordinal());
        period.addBindValue(
            static_cast<int>(
                output.input_snapshot.period.status));
        check(period);
        const int64_t period_id = id(period);

        write_calculations(database, period_id, output);

        database.commit();
    } catch (...) {
        database.rollback();
        throw;
    }
}

void save_calculation(
    Database& database,
    const CalcOutput& output) {
    database.transaction();

    try {
        QSqlQuery existing(database.sql());
        existing.prepare(
            "SELECT id FROM PayrollPeriod "
            "WHERE date_from=? AND date_to=? "
            "ORDER BY id DESC LIMIT 1");
        existing.addBindValue(
            output.input_snapshot.period
                .date_from.ordinal());
        existing.addBindValue(
            output.input_snapshot.period
                .date_to.ordinal());
        check(existing);

        int64_t period_id = 0;
        if (existing.next()) {
            period_id = existing.value(0).toLongLong();

            // Сохраняем расчёты, на которые уже ссылаются платёжные
            // документы; остальные строки периода заменяются.
            QSqlQuery obsolete(database.sql());
            obsolete.prepare(
                "DELETE FROM Calculation "
                "WHERE payroll_period_id=? "
                "AND id NOT IN ("
                "SELECT calculation_id FROM PaymentLine)");
            obsolete.addBindValue(period_id);
            check(obsolete);
        } else {
            QSqlQuery period(database.sql());
            period.prepare(
                "INSERT INTO PayrollPeriod("
                "date_from,date_to,status) VALUES(?,?,?)");
            period.addBindValue(
                output.input_snapshot.period
                    .date_from.ordinal());
            period.addBindValue(
                output.input_snapshot.period
                    .date_to.ordinal());
            period.addBindValue(
                static_cast<int>(
                    output.input_snapshot.period.status));
            check(period);
            period_id = id(period);
        }

        write_calculations(database, period_id, output);

        database.commit();
    } catch (...) {
        database.rollback();
        throw;
    }
}

std::vector<CalculationLine> read_calculation_lines(
    Database& database,
    int64_t calculation_id) {
    QSqlQuery query(database.sql());
    query.prepare(
        "SELECT calc_item_id,calc_item_code,role,"
        "stage,priority,amount_cents,"
        "formula_snapshot,rounding_policy "
        "FROM CalculationLine "
        "WHERE calculation_id=? ORDER BY id");
    query.addBindValue(calculation_id);
    check(query);

    std::vector<CalculationLine> result;
    while (query.next()) {
        CalculationLine line;
        line.calc_item_id =
            query.value(0).toLongLong();
        line.calc_item_code =
            query.value(1).toString().toStdString();
        line.role =
            static_cast<CalcRole>(
                query.value(2).toInt());
        line.stage = query.value(3).toInt();
        line.priority = query.value(4).toInt();
        line.amount =
            Money(query.value(5).toLongLong());
        line.formula_snapshot =
            query.value(6).toString().toStdString();
        line.rounding_policy =
            static_cast<RoundingPolicy>(
                query.value(7).toInt());
        result.push_back(line);
    }
    return result;
}

}
