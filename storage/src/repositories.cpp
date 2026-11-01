#include "mk/repositories.hpp"

#include "mk/calendar.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include <algorithm>
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

QString text(QSqlQuery& q, int i) {
    return q.value(i).toString();
}

std::optional<std::string> optional_text(QSqlQuery& q, int i) {
    return q.value(i).isNull()
        ? std::nullopt
        : std::optional<std::string>(text(q, i).toStdString());
}

std::optional<int64_t> optional_id(QSqlQuery& q, int i) {
    return q.value(i).isNull()
        ? std::nullopt
        : std::optional<int64_t>(q.value(i).toLongLong());
}

void exec_bind(QSqlQuery& q) {
    check(q);
}

Org org_from(QSqlQuery& q) {
    return {q.value(0).toLongLong(), text(q, 1).toStdString()};
}

Position position_from(QSqlQuery& q) {
    return {q.value(0).toLongLong(), text(q, 1).toStdString()};
}

WorkSchedule schedule_from(QSqlQuery& q) {
    WorkSchedule result;
    result.id = q.value(0).toLongLong();
    result.name = text(q, 1).toStdString();
    result.kind = static_cast<ScheduleKind>(q.value(2).toInt());
    result.normative_minutes_per_day = q.value(3).toInt();
    result.shift_cycle = text(q, 4).toStdString();

    if (!q.value(5).isNull()) {
        result.anchor_date = Date::from_ordinal(q.value(5).toInt());
    }

    return result;
}

Employee employee_from(QSqlQuery& q) {
    Employee result;
    result.id = q.value(0).toLongLong();
    result.personnel_number = text(q, 1).toStdString();
    result.full_name = text(q, 2).toStdString();
    result.org_id = q.value(3).toLongLong();
    result.department_id = q.value(4).toLongLong();
    result.position_id = q.value(5).toLongLong();
    result.employment_kind_id = q.value(6).toLongLong();
    result.work_schedule_id = q.value(7).toLongLong();
    result.payroll_preset_id = optional_id(q, 8);
    result.rate = Money(q.value(9).toLongLong());
    result.rate_unit = static_cast<RateUnit>(q.value(10).toInt());
    result.hire_date = Date::from_ordinal(q.value(11).toInt());

    if (!q.value(12).isNull()) {
        result.termination_date = Date::from_ordinal(q.value(12).toInt());
    }

    return result;
}

EmployeeSlice employee_slice_from(const Employee& employee) {
    EmployeeSlice result;
    result.employee_id = employee.id;
    result.personnel_number = employee.personnel_number;
    result.full_name = employee.full_name;
    result.org_id = employee.org_id;
    result.department_id = employee.department_id;
    result.position_id = employee.position_id;
    result.employment_kind_id = employee.employment_kind_id;
    result.work_schedule_id = employee.work_schedule_id;
    result.payroll_preset_id = employee.payroll_preset_id;
    result.rate = employee.rate;
    result.rate_unit = employee.rate_unit;
    return result;
}

EmploymentEvent event_from(QSqlQuery& q) {
    EmploymentEvent result;
    result.employee_id = q.value(1).toLongLong();
    result.date = Date::from_ordinal(q.value(2).toInt());
    result.kind = static_cast<EventKind>(q.value(3).toInt());
    result.personnel_number = optional_text(q, 4);
    result.full_name = optional_text(q, 5);
    result.org_id = optional_id(q, 6);
    result.department_id = optional_id(q, 7);
    result.position_id = optional_id(q, 8);
    result.employment_kind_id = optional_id(q, 9);
    result.work_schedule_id = optional_id(q, 10);
    result.payroll_preset_id = optional_id(q, 11);

    if (!q.value(12).isNull()) {
        result.rate = Money(q.value(12).toLongLong());
    }

    if (!q.value(13).isNull()) {
        result.rate_unit =
            static_cast<RateUnit>(q.value(13).toInt());
    }

    result.termination_reason_id = optional_id(q, 14);
    return result;
}

TimeFact fact_from(QSqlQuery& q) {
    TimeFact result;
    result.employee_id = q.value(1).toLongLong();
    result.date = Date::from_ordinal(q.value(2).toInt());
    result.worked_minutes = q.value(3).toInt();
    result.absence_kind_id = optional_id(q, 4);
    return result;
}

template <typename T, typename F>
std::vector<T> query_list(
    Database& database,
    const QString& sql,
    F&& convert) {
    QSqlQuery query(database.sql());
    query.prepare(sql);
    check(query);

    std::vector<T> result;
    while (query.next()) {
        result.push_back(convert(query));
    }

    return result;
}

}

int64_t create_org(Database& db, const Org& value) {
    QSqlQuery q(db.sql());
    q.prepare("INSERT INTO Org(name) VALUES(?)");
    q.addBindValue(QString::fromStdString(value.name));
    check(q);
    return id(q);
}

std::optional<Org> read_org(Database& db, int64_t value) {
    QSqlQuery q(db.sql());
    q.prepare("SELECT id,name FROM Org WHERE id=?");
    q.addBindValue(value);
    check(q);
    return q.next()
        ? std::optional<Org>(org_from(q))
        : std::nullopt;
}

void update_org(Database& db, const Org& value) {
    QSqlQuery q(db.sql());
    q.prepare("UPDATE Org SET name=? WHERE id=?");
    q.addBindValue(QString::fromStdString(value.name));
    q.addBindValue(value.id);
    check(q);
    if (q.numRowsAffected() == 0) {
        throw std::runtime_error("organization not found");
    }
}

void delete_org(Database& db, int64_t value) {
    QSqlQuery q(db.sql());
    q.prepare("DELETE FROM Org WHERE id=?");
    q.addBindValue(value);
    check(q);
    if (q.numRowsAffected() == 0) {
        throw std::runtime_error("organization not found");
    }
}

std::vector<Org> list_orgs(Database& db) {
    return query_list<Org>(db, "SELECT id,name FROM Org ORDER BY id", org_from);
}

int64_t create_position(Database& db, const Position& value) {
    QSqlQuery q(db.sql());
    q.prepare("INSERT INTO Position(name) VALUES(?)");
    q.addBindValue(QString::fromStdString(value.name));
    check(q);
    return id(q);
}

std::optional<Position> read_position(Database& db, int64_t value) {
    QSqlQuery q(db.sql());
    q.prepare("SELECT id,name FROM Position WHERE id=?");
    q.addBindValue(value);
    check(q);
    return q.next()
        ? std::optional<Position>(position_from(q))
        : std::nullopt;
}

void update_position(Database& db, const Position& value) {
    QSqlQuery q(db.sql());
    q.prepare("UPDATE Position SET name=? WHERE id=?");
    q.addBindValue(QString::fromStdString(value.name));
    q.addBindValue(value.id);
    check(q);
    if (q.numRowsAffected() == 0) {
        throw std::runtime_error("position not found");
    }
}

void delete_position(Database& db, int64_t value) {
    QSqlQuery q(db.sql());
    q.prepare("DELETE FROM Position WHERE id=?");
    q.addBindValue(value);
    check(q);
    if (q.numRowsAffected() == 0) {
        throw std::runtime_error("position not found");
    }
}

std::vector<Position> list_positions(Database& db) {
    return query_list<Position>(
        db, "SELECT id,name FROM Position ORDER BY id", position_from);
}

int64_t create_work_schedule(Database& db, const WorkSchedule& value) {
    QSqlQuery q(db.sql());
    q.prepare(
        "INSERT INTO WorkSchedule("
        "name,kind,normative_minutes_per_day,shift_cycle,anchor_date)"
        " VALUES(?,?,?,?,?)");
    q.addBindValue(QString::fromStdString(value.name));
    q.addBindValue(static_cast<int>(value.kind));
    q.addBindValue(value.normative_minutes_per_day);
    q.addBindValue(QString::fromStdString(value.shift_cycle));
    q.addBindValue(value.anchor_date.has_value()
        ? QVariant(value.anchor_date->ordinal())
        : QVariant());
    check(q);
    return id(q);
}

std::optional<WorkSchedule> read_work_schedule(
    Database& db, int64_t value) {
    QSqlQuery q(db.sql());
    q.prepare(
        "SELECT id,name,kind,normative_minutes_per_day,"
        "shift_cycle,anchor_date FROM WorkSchedule WHERE id=?");
    q.addBindValue(value);
    check(q);
    return q.next()
        ? std::optional<WorkSchedule>(schedule_from(q))
        : std::nullopt;
}

void update_work_schedule(Database& db, const WorkSchedule& value) {
    QSqlQuery q(db.sql());
    q.prepare(
        "UPDATE WorkSchedule SET name=?,kind=?,"
        "normative_minutes_per_day=?,shift_cycle=?,anchor_date=? WHERE id=?");
    q.addBindValue(QString::fromStdString(value.name));
    q.addBindValue(static_cast<int>(value.kind));
    q.addBindValue(value.normative_minutes_per_day);
    q.addBindValue(QString::fromStdString(value.shift_cycle));
    q.addBindValue(optional_date(value.anchor_date));
    q.addBindValue(value.id);
    check(q);
    if (q.numRowsAffected() == 0) {
        throw std::runtime_error("work schedule not found");
    }
}

void delete_work_schedule(Database& db, int64_t value) {
    QSqlQuery q(db.sql());
    q.prepare("DELETE FROM WorkSchedule WHERE id=?");
    q.addBindValue(value);
    check(q);
    if (q.numRowsAffected() == 0) {
        throw std::runtime_error("work schedule not found");
    }
}

std::vector<WorkSchedule> list_work_schedules(Database& db) {
    return query_list<WorkSchedule>(
        db,
        "SELECT id,name,kind,normative_minutes_per_day,"
        "shift_cycle,anchor_date FROM WorkSchedule ORDER BY id",
        schedule_from);
}

std::vector<WorkSchedule> list_work_schedules(
    Database& db, int64_t) {
    return list_work_schedules(db);
}

int64_t create_employee(Database& db, const Employee& value) {
    QSqlQuery q(db.sql());
    q.prepare(
        "INSERT INTO Employee("
        "personnel_number,full_name,org_id,department_id,position_id,"
        "employment_kind_id,work_schedule_id,payroll_preset_id,rate_cents,"
        "rate_unit,hire_date,termination_date)"
        " VALUES(?,?,?,?,?,?,?,?,?,?,?,?)");
    q.addBindValue(QString::fromStdString(value.personnel_number));
    q.addBindValue(QString::fromStdString(value.full_name));
    q.addBindValue(value.org_id);
    q.addBindValue(value.department_id);
    q.addBindValue(value.position_id);
    q.addBindValue(value.employment_kind_id);
    q.addBindValue(value.work_schedule_id);
    q.addBindValue(optional_value(value.payroll_preset_id));
    q.addBindValue(value.rate.cents);
    q.addBindValue(static_cast<int>(value.rate_unit));
    q.addBindValue(value.hire_date.ordinal());
    q.addBindValue(value.termination_date.has_value()
        ? QVariant(value.termination_date->ordinal())
        : QVariant());
    check(q);
    return id(q);
}

std::optional<Employee> read_employee(Database& db, int64_t value) {
    QSqlQuery q(db.sql());
    q.prepare(
        "SELECT id,personnel_number,full_name,org_id,department_id,"
        "position_id,employment_kind_id,work_schedule_id,payroll_preset_id,"
        "rate_cents,rate_unit,hire_date,termination_date "
        "FROM Employee WHERE id=?");
    q.addBindValue(value);
    check(q);
    return q.next()
        ? std::optional<Employee>(employee_from(q))
        : std::nullopt;
}

void update_employee(Database& db, const Employee& value) {
    QSqlQuery q(db.sql());
    q.prepare(
        "UPDATE Employee SET personnel_number=?,full_name=?,org_id=?,"
        "department_id=?,position_id=?,employment_kind_id=?,"
        "work_schedule_id=?,payroll_preset_id=?,rate_cents=?,rate_unit=?,"
        "hire_date=?,termination_date=? WHERE id=?");
    q.addBindValue(QString::fromStdString(value.personnel_number));
    q.addBindValue(QString::fromStdString(value.full_name));
    q.addBindValue(value.org_id);
    q.addBindValue(value.department_id);
    q.addBindValue(value.position_id);
    q.addBindValue(value.employment_kind_id);
    q.addBindValue(value.work_schedule_id);
    q.addBindValue(optional_value(value.payroll_preset_id));
    q.addBindValue(value.rate.cents);
    q.addBindValue(static_cast<int>(value.rate_unit));
    q.addBindValue(value.hire_date.ordinal());
    q.addBindValue(value.termination_date.has_value()
        ? QVariant(value.termination_date->ordinal())
        : QVariant());
    q.addBindValue(value.id);
    check(q);
    if (q.numRowsAffected() == 0) {
        throw std::runtime_error("employee not found");
    }
}

void delete_employee(Database& db, int64_t value) {
    QSqlQuery q(db.sql());
    q.prepare("DELETE FROM Employee WHERE id=?");
    q.addBindValue(value);
    check(q);
    if (q.numRowsAffected() == 0) {
        throw std::runtime_error("employee not found");
    }
}

std::vector<Employee> list_employees(Database& db, int64_t org_id) {
    QSqlQuery q(db.sql());
    q.prepare(
        "SELECT id,personnel_number,full_name,org_id,department_id,"
        "position_id,employment_kind_id,work_schedule_id,payroll_preset_id,"
        "rate_cents,rate_unit,hire_date,termination_date "
        "FROM Employee WHERE org_id=? ORDER BY id");
    q.addBindValue(org_id);
    check(q);

    std::vector<Employee> result;
    while (q.next()) {
        result.push_back(employee_from(q));
    }

    return result;
}

int64_t create_employment_event(Database& db, const EmploymentEvent& value) {
    QSqlQuery q(db.sql());
    q.prepare(
        "INSERT INTO EmploymentEvent("
        "employee_id,event_date,kind,personnel_number,full_name,org_id,"
        "department_id,position_id,employment_kind_id,work_schedule_id,"
        "payroll_preset_id,rate_cents,rate_unit,termination_reason_id)"
        " VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
    q.addBindValue(value.employee_id);
    q.addBindValue(value.date.ordinal());
    q.addBindValue(static_cast<int>(value.kind));
    q.addBindValue(optional_value(value.personnel_number));
    q.addBindValue(optional_value(value.full_name));
    q.addBindValue(optional_value(value.org_id));
    q.addBindValue(optional_value(value.department_id));
    q.addBindValue(optional_value(value.position_id));
    q.addBindValue(optional_value(value.employment_kind_id));
    q.addBindValue(optional_value(value.work_schedule_id));
    q.addBindValue(optional_value(value.payroll_preset_id));
    q.addBindValue(optional_value(value.rate));
    q.addBindValue(optional_value(value.rate_unit));
    q.addBindValue(optional_value(value.termination_reason_id));
    check(q);
    return id(q);
}

std::optional<EmploymentEvent> read_employment_event(
    Database& db, int64_t value) {
    QSqlQuery q(db.sql());
    q.prepare(
        "SELECT id,employee_id,event_date,kind,personnel_number,full_name,"
        "org_id,department_id,position_id,employment_kind_id,"
        "work_schedule_id,payroll_preset_id,rate_cents,rate_unit,"
        "termination_reason_id FROM EmploymentEvent WHERE id=?");
    q.addBindValue(value);
    check(q);
    return q.next()
        ? std::optional<EmploymentEvent>(event_from(q))
        : std::nullopt;
}

void update_employment_event(Database& db, const EmploymentEvent& value) {
    QSqlQuery q(db.sql());
    q.prepare(
        "UPDATE EmploymentEvent SET employee_id=?,event_date=?,kind=?,"
        "personnel_number=?,full_name=?,org_id=?,department_id=?,"
        "position_id=?,employment_kind_id=?,work_schedule_id=?,"
        "payroll_preset_id=?,rate_cents=?,rate_unit=?,"
        "termination_reason_id=? WHERE id=?");
    q.addBindValue(value.employee_id);
    q.addBindValue(value.date.ordinal());
    q.addBindValue(static_cast<int>(value.kind));
    q.addBindValue(optional_value(value.personnel_number));
    q.addBindValue(optional_value(value.full_name));
    q.addBindValue(optional_value(value.org_id));
    q.addBindValue(optional_value(value.department_id));
    q.addBindValue(optional_value(value.position_id));
    q.addBindValue(optional_value(value.employment_kind_id));
    q.addBindValue(optional_value(value.work_schedule_id));
    q.addBindValue(optional_value(value.payroll_preset_id));
    q.addBindValue(optional_value(value.rate));
    q.addBindValue(optional_value(value.rate_unit));
    q.addBindValue(optional_value(value.termination_reason_id));
    q.addBindValue(value.employee_id);
    check(q);
    if (q.numRowsAffected() == 0) {
        throw std::runtime_error("employment event not found");
    }
}

void delete_employment_event(Database& db, int64_t value) {
    QSqlQuery q(db.sql());
    q.prepare("DELETE FROM EmploymentEvent WHERE id=?");
    q.addBindValue(value);
    check(q);
    if (q.numRowsAffected() == 0) {
        throw std::runtime_error("employment event not found");
    }
}

std::vector<EmploymentEvent> list_employment_events(
    Database& db, int64_t employee_id) {
    QSqlQuery q(db.sql());
    q.prepare(
        "SELECT id,employee_id,event_date,kind,personnel_number,full_name,"
        "org_id,department_id,position_id,employment_kind_id,"
        "work_schedule_id,payroll_preset_id,rate_cents,rate_unit,"
        "termination_reason_id FROM EmploymentEvent "
        "WHERE employee_id=? ORDER BY event_date,id");
    q.addBindValue(employee_id);
    check(q);

    std::vector<EmploymentEvent> result;
    while (q.next()) {
        result.push_back(event_from(q));
    }

    return result;
}

int64_t create_time_fact(Database& db, const TimeFact& value) {
    QSqlQuery q(db.sql());
    q.prepare(
        "INSERT INTO TimeFact("
        "employee_id,fact_date,worked_minutes,absence_kind_id)"
        " VALUES(?,?,?,?)");
    q.addBindValue(value.employee_id);
    q.addBindValue(value.date.ordinal());
    q.addBindValue(value.worked_minutes);
    q.addBindValue(optional_value(value.absence_kind_id));
    check(q);
    return id(q);
}

std::optional<TimeFact> read_time_fact(Database& db, int64_t value) {
    QSqlQuery q(db.sql());
    q.prepare(
        "SELECT id,employee_id,fact_date,worked_minutes,absence_kind_id "
        "FROM TimeFact WHERE id=?");
    q.addBindValue(value);
    check(q);
    return q.next()
        ? std::optional<TimeFact>(fact_from(q))
        : std::nullopt;
}

void update_time_fact(Database& db, const TimeFact& value) {
    QSqlQuery q(db.sql());
    q.prepare(
        "UPDATE TimeFact SET employee_id=?,fact_date=?,"
        "worked_minutes=?,absence_kind_id=? WHERE id=?");
    q.addBindValue(value.employee_id);
    q.addBindValue(value.date.ordinal());
    q.addBindValue(value.worked_minutes);
    q.addBindValue(optional_value(value.absence_kind_id));
    q.addBindValue(value.employee_id);
    check(q);
    if (q.numRowsAffected() == 0) {
        throw std::runtime_error("time fact not found");
    }
}

void delete_time_fact(Database& db, int64_t value) {
    QSqlQuery q(db.sql());
    q.prepare("DELETE FROM TimeFact WHERE id=?");
    q.addBindValue(value);
    check(q);
    if (q.numRowsAffected() == 0) {
        throw std::runtime_error("time fact not found");
    }
}

std::vector<TimeFact> list_time_facts(
    Database& db, int64_t employee_id, Date from, Date to) {
    QSqlQuery q(db.sql());
    q.prepare(
        "SELECT id,employee_id,fact_date,worked_minutes,absence_kind_id "
        "FROM TimeFact WHERE employee_id=? AND fact_date>=? "
        "AND fact_date<=? ORDER BY fact_date,id");
    q.addBindValue(employee_id);
    q.addBindValue(from.ordinal());
    q.addBindValue(to.ordinal());
    check(q);

    std::vector<TimeFact> result;
    while (q.next()) {
        result.push_back(fact_from(q));
    }

    return result;
}

std::vector<AbsenceKind> list_absence_kinds(Database& db) {
    QSqlQuery q(db.sql());
    q.prepare("SELECT id,code,name,paid FROM AbsenceKind ORDER BY id");
    check(q);

    std::vector<AbsenceKind> result;
    while (q.next()) {
        result.push_back({
            q.value(0).toLongLong(),
            q.value(1).toString().toStdString(),
            q.value(2).toString().toStdString(),
            q.value(3).toBool()
        });
    }
    return result;
}

std::vector<CalcItem> list_calc_items(Database& db) {
    QSqlQuery q(db.sql());
    q.prepare(
        "SELECT id,code,name,formula,role,stage,priority,taxable,"
        "included_in_contrib_base,reduces_income_tax_base,rounding_policy "
        "FROM CalcItem ORDER BY id");
    check(q);

    std::vector<CalcItem> result;
    while (q.next()) {
        CalcItem item;
        item.id = q.value(0).toLongLong();
        item.code = q.value(1).toString().toStdString();
        item.name = q.value(2).toString().toStdString();
        item.formula = q.value(3).toString().toStdString();
        item.role = static_cast<CalcRole>(q.value(4).toInt());
        item.stage = q.value(5).toInt();
        item.priority = q.value(6).toInt();
        item.taxable = q.value(7).toBool();
        item.included_in_contrib_base = q.value(8).toBool();
        item.reduces_income_tax_base = q.value(9).toBool();
        item.rounding_policy =
            static_cast<RoundingPolicy>(q.value(10).toInt());
        result.push_back(item);
    }
    return result;
}

std::vector<PayrollPreset> list_payroll_presets(Database& db) {
    QSqlQuery q(db.sql());
    q.prepare("SELECT id,code,name FROM PayrollPreset ORDER BY id");
    check(q);

    std::vector<PayrollPreset> result;
    while (q.next()) {
        PayrollPreset preset{
            q.value(0).toLongLong(),
            q.value(1).toString().toStdString(),
            q.value(2).toString().toStdString(),
            {}
        };

        QSqlQuery items(db.sql());
        items.prepare(
            "SELECT calc_item_id FROM PayrollPresetItem "
            "WHERE preset_id=? ORDER BY calc_item_id");
        items.addBindValue(preset.id);
        check(items);

        while (items.next()) {
            preset.calc_item_ids.push_back(items.value(0).toLongLong());
        }

        result.push_back(preset);
    }
    return result;
}

CalcInput build_calc_input(
    Database& db, int64_t org_id, int32_t year, int32_t month) {
    CalcInput result;
    result.period.date_from = Date(year, month, 1);
    result.period.date_to =
        Date(year, month, Date::days_in_month(year, month));

    const std::vector<Employee> employees =
        list_employees(db, org_id);

    for (const Employee& employee : employees) {
        result.employees.push_back(employee_slice_from(employee));
    }

    result.work_schedules = list_work_schedules(db);
    result.payroll_presets = list_payroll_presets(db);
    result.calc_items = list_calc_items(db);
    result.absence_kinds = list_absence_kinds(db);

    const Date end = result.period.date_to;

    for (const Employee& employee : employees) {
        for (const EmploymentEvent& event :
             list_employment_events(db, employee.id)) {
            if (event.date <= end) {
                result.employment_events.push_back(event);
            }
        }

        const std::vector<TimeFact> facts =
            list_time_facts(
                db,
                employee.id,
                result.period.date_from,
                result.period.date_to);

        result.time_facts.insert(
            result.time_facts.end(), facts.begin(), facts.end());

        const auto schedule = std::find_if(
            result.work_schedules.begin(),
            result.work_schedules.end(),
            [&](const WorkSchedule& value) {
                return value.id == employee.work_schedule_id;
            });

        if (schedule != result.work_schedules.end()) {
            const std::vector<NormDay> days =
                generate_norm_calendar(*schedule, year, month);
            result.norm_days.insert(
                result.norm_days.end(), days.begin(), days.end());
        }
    }

    return result;
}

void persist_calculation(Database& db, const CalcOutput& output) {
    db.transaction();

    try {
        QSqlQuery period(db.sql());
        period.prepare(
            "INSERT INTO PayrollPeriod(date_from,date_to,status)"
            " VALUES(?,?,?)");
        period.addBindValue(output.input_snapshot.period.date_from.ordinal());
        period.addBindValue(output.input_snapshot.period.date_to.ordinal());
        period.addBindValue(
            static_cast<int>(output.input_snapshot.period.status));
        check(period);
        const int64_t period_id = id(period);

        for (const Calculation& value : output.calculations) {
            QSqlQuery calculation(db.sql());
            calculation.prepare(
                "INSERT INTO Calculation("
                "employee_id,payroll_period_id,status,incomplete_reasons,"
                "total_accrual_cents,total_pre_tax_deduction_cents,"
                "income_tax_cents,total_employer_contribution_cents,"
                "total_post_tax_deduction_cents,net_amount_cents,"
                "engine_version,input_snapshot)"
                " VALUES(?,?,?,?,?,?,?,?,?,?,?,?)");

            QJsonArray reasons;
            for (const std::string& reason : value.incomplete_reasons) {
                reasons.append(QString::fromStdString(reason));
            }

            calculation.addBindValue(value.employee_id);
            calculation.addBindValue(period_id);
            calculation.addBindValue(static_cast<int>(value.status));
            calculation.addBindValue(
                QString::fromUtf8(
                    QJsonDocument(reasons).toJson(QJsonDocument::Compact)));
            calculation.addBindValue(value.total_accrual.cents);
            calculation.addBindValue(value.total_pre_tax_deduction.cents);
            calculation.addBindValue(value.income_tax.cents);
            calculation.addBindValue(value.total_employer_contribution.cents);
            calculation.addBindValue(value.total_post_tax_deduction.cents);
            calculation.addBindValue(value.net_amount.cents);
            calculation.addBindValue(
                QString::fromStdString(value.engine_version));

            if (value.input_snapshot.has_value()) {
                calculation.addBindValue(
                    QString::fromStdString(*value.input_snapshot));
            } else {
                calculation.addBindValue(QVariant());
            }

            check(calculation);
            const int64_t calculation_id = id(calculation);

            for (const CalculationLine& line : value.lines) {
                QSqlQuery item(db.sql());
                item.prepare(
                    "INSERT INTO CalculationLine("
                    "calculation_id,calc_item_id,calc_item_code,role,stage,"
                    "priority,amount_cents,formula_snapshot,rounding_policy)"
                    " VALUES(?,?,?,?,?,?,?,?,?)");
                item.addBindValue(calculation_id);
                item.addBindValue(line.calc_item_id);
                item.addBindValue(
                    QString::fromStdString(line.calc_item_code));
                item.addBindValue(static_cast<int>(line.role));
                item.addBindValue(line.stage);
                item.addBindValue(line.priority);
                item.addBindValue(line.amount.cents);
                item.addBindValue(
                    QString::fromStdString(line.formula_snapshot));
                item.addBindValue(static_cast<int>(line.rounding_policy));
                check(item);
            }
        }

        db.commit();
    } catch (...) {
        db.rollback();
        throw;
    }
}

std::vector<CalculationLine> read_calculation_lines(
    Database& db, int64_t calculation_id) {
    QSqlQuery q(db.sql());
    q.prepare(
        "SELECT calc_item_id,calc_item_code,role,stage,priority,"
        "amount_cents,formula_snapshot,rounding_policy "
        "FROM CalculationLine WHERE calculation_id=? ORDER BY id");
    q.addBindValue(calculation_id);
    check(q);

    std::vector<CalculationLine> result;
    while (q.next()) {
        CalculationLine line;
        line.calc_item_id = q.value(0).toLongLong();
        line.calc_item_code = q.value(1).toString().toStdString();
        line.role = static_cast<CalcRole>(q.value(2).toInt());
        line.stage = q.value(3).toInt();
        line.priority = q.value(4).toInt();
        line.amount = Money(q.value(5).toLongLong());
        line.formula_snapshot = q.value(6).toString().toStdString();
        line.rounding_policy =
            static_cast<RoundingPolicy>(q.value(7).toInt());
        result.push_back(line);
    }
    return result;
}

}
