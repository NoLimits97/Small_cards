#include "test_framework.hpp"

#include "timesheet_table_model.hpp"

#include <QDate>
#include <QString>

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace {

mk::WorkSchedule five_day_schedule() {
    mk::WorkSchedule schedule;
    schedule.id = 1;
    schedule.name = "Пятидневка";
    schedule.kind = mk::ScheduleKind::FiveDay;
    schedule.normative_minutes_per_day = 480;
    return schedule;
}

mk::Employee employee(
    int64_t id,
    const std::string& name) {
    mk::Employee value;
    value.id = id;
    value.personnel_number = "0001";
    value.full_name = name;
    value.work_schedule_id = 1;
    return value;
}

mk::gui::TimesheetEmployeeSource source(
    int64_t id,
    const std::string& name,
    std::vector<mk::TimeFact> facts = {}) {
    mk::gui::TimesheetEmployeeSource value;
    value.employee = employee(id, name);
    value.position_name = QStringLiteral("Сотрудник");
    value.time_facts = std::move(facts);
    return value;
}

mk::TimeFact fact(
    int64_t employee_id,
    int year,
    int month,
    int day,
    int32_t minutes,
    std::optional<int64_t> absence_kind_id = std::nullopt) {
    mk::TimeFact value;
    value.employee_id = employee_id;
    value.date = mk::Date(year, month, day);
    value.worked_minutes = minutes;
    value.absence_kind_id = absence_kind_id;
    return value;
}

int32_t expected_missed_minutes(
    int year,
    int month,
    const std::vector<mk::TimeFact>& facts) {
    int32_t result = 0;
    const int days = mk::Date::days_in_month(year, month);
    for (int day = 1; day <= days; ++day) {
        const QDate date(year, month, day);
        if (date.dayOfWeek() > 5) {
            continue;
        }
        int32_t worked = 0;
        for (const mk::TimeFact& value : facts) {
            if (value.date == mk::Date(year, month, day)) {
                worked += value.worked_minutes;
            }
        }
        result += std::max(480 - worked, 0);
    }
    return result;
}

int expected_missed_days(
    int year,
    int month,
    const std::vector<mk::TimeFact>& facts) {
    int result = 0;
    const int days = mk::Date::days_in_month(year, month);
    for (int day = 1; day <= days; ++day) {
        const QDate date(year, month, day);
        if (date.dayOfWeek() > 5) {
            continue;
        }
        int32_t worked = 0;
        for (const mk::TimeFact& value : facts) {
            if (value.date == mk::Date(year, month, day)) {
                worked += value.worked_minutes;
            }
        }
        if (worked < 480) {
            ++result;
        }
    }
    return result;
}

}

TEST(timesheet_model_handles_month_lengths_and_weekends) {
    struct MonthCase {
        int year;
        int month;
        int days;
    };
    const MonthCase cases[] = {
        {2026, 2, 28},
        {2028, 2, 29},
        {2026, 4, 30},
        {2026, 1, 31}};

    for (const MonthCase& value : cases) {
        mk::gui::TimesheetTableModel model;
        model.set_data(
            value.year,
            value.month,
            {source(1, "Иванов Иван")},
            {five_day_schedule()},
            {});

        REQUIRE_EQUAL(model.rowCount(), 1);
        REQUIRE_EQUAL(model.day_count(), value.days);
        REQUIRE_EQUAL(
            model.columnCount(),
            3 + value.days + 4);

        for (int day = 1; day <= value.days; ++day) {
            const QDate date(value.year, value.month, day);
            const QModelIndex index =
                model.index(0, model.day_column(day));
            REQUIRE(index.isValid());
            const bool working = date.dayOfWeek() <= 5;
            REQUIRE_EQUAL(
                index.data(Qt::UserRole + 1).toBool(),
                working);
            if (working) {
                REQUIRE_EQUAL(
                    index.data().toString(),
                    QStringLiteral("—"));
            } else {
                REQUIRE_EQUAL(
                    index.data().toString(),
                    QStringLiteral("В"));
            }
        }
    }
}

TEST(timesheet_model_computes_totals_from_time_facts) {
    const std::vector<mk::TimeFact> facts = {
        fact(1, 2026, 1, 5, 480),
        fact(1, 2026, 1, 6, 240)};

    mk::gui::TimesheetTableModel model;
    model.set_data(
        2026,
        1,
        {source(1, "Иванов Иван", facts)},
        {five_day_schedule()},
        {});

    REQUIRE_EQUAL(
        model.index(0, model.total_column(
            mk::gui::TimesheetTableModel::WorkedHours))
            .data().toString(),
        QStringLiteral("12:00"));
    REQUIRE_EQUAL(
        model.index(0, model.total_column(
            mk::gui::TimesheetTableModel::WorkedDays))
            .data().toInt(),
        2);
    REQUIRE_EQUAL(
        model.index(0, model.total_column(
            mk::gui::TimesheetTableModel::MissedHours))
            .data().toString(),
        mk::gui::TimesheetTableModel::format_minutes(
            expected_missed_minutes(2026, 1, facts)));
    REQUIRE_EQUAL(
        model.index(0, model.total_column(
            mk::gui::TimesheetTableModel::MissedDays))
            .data().toInt(),
        expected_missed_days(2026, 1, facts));
    REQUIRE_EQUAL(
        model.index(0, model.day_column(5)).data().toString(),
        QStringLiteral("8:00"));
    REQUIRE_EQUAL(
        model.index(0, model.day_column(6)).data().toString(),
        QStringLiteral("4:00"));
}

TEST(timesheet_model_parses_hours) {
    int32_t minutes = 0;
    QString error;

    REQUIRE(mk::gui::TimesheetTableModel::parse_hours(
        QStringLiteral("8"), minutes, error));
    REQUIRE_EQUAL(minutes, int32_t{480});
    REQUIRE(mk::gui::TimesheetTableModel::parse_hours(
        QStringLiteral("8:30"), minutes, error));
    REQUIRE_EQUAL(minutes, int32_t{510});
    REQUIRE(mk::gui::TimesheetTableModel::parse_hours(
        QStringLiteral("8,5"), minutes, error));
    REQUIRE_EQUAL(minutes, int32_t{510});
    REQUIRE(mk::gui::TimesheetTableModel::parse_hours(
        QStringLiteral(""), minutes, error));
    REQUIRE_EQUAL(minutes, int32_t{0});

    REQUIRE(!mk::gui::TimesheetTableModel::parse_hours(
        QStringLiteral("25"), minutes, error));
    REQUIRE(!mk::gui::TimesheetTableModel::parse_hours(
        QStringLiteral("-1"), minutes, error));
    REQUIRE(!mk::gui::TimesheetTableModel::parse_hours(
        QStringLiteral("8:75"), minutes, error));
    REQUIRE(!mk::gui::TimesheetTableModel::parse_hours(
        QStringLiteral("abc"), minutes, error));
}

TEST(timesheet_model_edits_working_days_only) {
    mk::gui::TimesheetTableModel model;
    model.set_data(
        2026,
        1,
        {source(1, "Иванов Иван")},
        {five_day_schedule()},
        {});

    const QModelIndex working =
        model.index(0, model.day_column(5));
    REQUIRE(working.flags() & Qt::ItemIsEditable);
    REQUIRE(model.setData(working, 510, Qt::EditRole));
    REQUIRE_EQUAL(working.data().toString(), QStringLiteral("8:30"));
    REQUIRE(model.has_unsaved_changes());
    const auto changes = model.pending_changes();
    REQUIRE_EQUAL(changes.size(), std::size_t{1});
    REQUIRE_EQUAL(changes[0].employee_id, int64_t{1});
    REQUIRE_EQUAL(changes[0].worked_minutes, int32_t{510});
    REQUIRE_EQUAL(changes[0].time_fact_id, int64_t{0});

    REQUIRE(!model.setData(working, 1501, Qt::EditRole));
    REQUIRE(!model.setData(working, -1, Qt::EditRole));
    REQUIRE_EQUAL(working.data().toString(), QStringLiteral("8:30"));

    const QModelIndex weekend =
        model.index(0, model.day_column(3));
    REQUIRE(!(weekend.flags() & Qt::ItemIsEditable));
    REQUIRE(!model.setData(weekend, 480, Qt::EditRole));

    const QModelIndex info =
        model.index(0, mk::gui::TimesheetTableModel::FullName);
    REQUIRE(!model.setData(info, 480, Qt::EditRole));
}

TEST(timesheet_model_shows_absence_code) {
    const std::vector<mk::TimeFact> facts = {
        fact(1, 2026, 1, 5, 0, 1)};
    const std::vector<mk::AbsenceKind> kinds = {
        {1, "sick", "Больничный", true}};

    mk::gui::TimesheetTableModel model;
    model.set_data(
        2026,
        1,
        {source(1, "Иванов Иван", facts)},
        {five_day_schedule()},
        kinds);

    const QModelIndex index =
        model.index(0, model.day_column(5));
    REQUIRE_EQUAL(index.data().toString(), QStringLiteral("sick"));
    REQUIRE(index.data(Qt::ToolTipRole).toString().contains(
        QStringLiteral("Больничный")));
}

TEST(timesheet_model_reports_missing_schedule) {
    mk::gui::TimesheetTableModel model;
    mk::gui::TimesheetEmployeeSource employee_source =
        source(1, "Иванов Иван");
    employee_source.employee.work_schedule_id = 99;
    model.set_data(
        2026,
        1,
        {employee_source},
        {five_day_schedule()},
        {});

    REQUIRE_EQUAL(model.rowCount(), 1);
    REQUIRE_EQUAL(model.warnings().size(), 1);
    const QModelIndex index =
        model.index(0, model.day_column(5));
    REQUIRE_EQUAL(index.data().toString(), QStringLiteral("—"));
    REQUIRE(!(index.flags() & Qt::ItemIsEditable));
    REQUIRE(!model.setData(index, 480, Qt::EditRole));
}

TEST(timesheet_model_handles_empty_employee_list) {
    mk::gui::TimesheetTableModel model;
    model.set_data(
        2026,
        1,
        {},
        {five_day_schedule()},
        {});

    REQUIRE_EQUAL(model.rowCount(), 0);
    REQUIRE_EQUAL(model.day_count(), 31);
    REQUIRE_EQUAL(model.columnCount(), 3 + 31 + 4);
    REQUIRE(!model.has_unsaved_changes());
    REQUIRE(model.pending_changes().empty());
}
