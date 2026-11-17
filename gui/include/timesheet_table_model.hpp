#ifndef MK_GUI_TIMESHEET_TABLE_MODEL_HPP
#define MK_GUI_TIMESHEET_TABLE_MODEL_HPP

#include "mk/model.hpp"

#include <QAbstractTableModel>
#include <QString>
#include <QStringList>

#include <cstdint>
#include <optional>
#include <vector>

namespace mk::gui {

struct TimesheetEmployeeSource {
    Employee employee;
    QString position_name;
    std::vector<TimeFact> time_facts;
};

struct TimesheetDayCell {
    Date date = Date(1, 1, 1);
    bool working_day = false;
    int32_t normative_minutes = 0;
    int32_t worked_minutes = 0;
    int32_t absence_minutes = 0;
    int64_t time_fact_id = 0;
    std::optional<int64_t> absence_kind_id;
    QString absence_code;
    QString absence_name;
    bool has_fact = false;
    bool multiple_facts = false;
    bool edited = false;
};

struct TimesheetEmployeeRow {
    int64_t employee_id = 0;
    QString full_name;
    QString personnel_number;
    QString position;
    bool schedule_available = true;
    std::vector<TimesheetDayCell> days;
};

struct TimesheetPendingChange {
    int64_t employee_id = 0;
    Date date = Date(1, 1, 1);
    int64_t time_fact_id = 0;
    std::optional<int64_t> absence_kind_id;
    int32_t worked_minutes = 0;
};

class TimesheetTableModel : public QAbstractTableModel {
public:
    enum InfoColumn : int {
        FullName = 0,
        PersonnelNumber,
        Position,
        InfoColumnCount
    };

    enum TotalColumn : int {
        WorkedHours = 0,
        WorkedDays,
        MissedHours,
        MissedDays,
        TotalColumnCount
    };

    explicit TimesheetTableModel(QObject* parent = nullptr);

    int rowCount(
        const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(
        const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(
        const QModelIndex& index,
        int role = Qt::DisplayRole) const override;
    QVariant headerData(
        int section,
        Qt::Orientation orientation,
        int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool setData(
        const QModelIndex& index,
        const QVariant& value,
        int role = Qt::EditRole) override;

    void set_data(
        int32_t year,
        int32_t month,
        std::vector<TimesheetEmployeeSource> employees,
        const std::vector<WorkSchedule>& schedules,
        const std::vector<AbsenceKind>& absence_kinds);
    void clear();

    int32_t year() const;
    int32_t month() const;
    int day_count() const;
    int day_column(int day) const;
    int total_column(TotalColumn column) const;

    int64_t employee_id_at(int row) const;
    bool has_unsaved_changes() const;
    std::vector<TimesheetPendingChange> pending_changes() const;
    QStringList warnings() const;

    static bool parse_hours(
        const QString& text,
        int32_t& minutes,
        QString& error);
    static QString format_minutes(int32_t minutes);

private:
    const TimesheetEmployeeRow* row_at(int row) const;
    const TimesheetDayCell* cell_at(
        const TimesheetEmployeeRow& row,
        int column) const;
    TimesheetDayCell* editable_cell(
        const QModelIndex& index,
        const TimesheetEmployeeRow** row);
    int32_t total_worked_minutes(const TimesheetEmployeeRow& row) const;
    int32_t total_worked_days(const TimesheetEmployeeRow& row) const;
    int32_t total_missed_minutes(const TimesheetEmployeeRow& row) const;
    int32_t total_missed_days(const TimesheetEmployeeRow& row) const;

    int32_t year_ = 2000;
    int32_t month_ = 1;
    int days_ = 0;
    std::vector<TimesheetEmployeeRow> rows_;
    QStringList warnings_;
};

}

#endif
