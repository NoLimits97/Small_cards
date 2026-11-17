#include "timesheet_table_model.hpp"

#include "mk/calendar.hpp"

#include <QBrush>
#include <QColor>
#include <QDate>
#include <QVariant>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace mk::gui {
namespace {

const char* const kWeekdayShort[] = {
    "Пн", "Вт", "Ср", "Чт", "Пт", "Сб", "Вс"};
const char* const kWeekdayFull[] = {
    "понедельник", "вторник", "среда", "четверг",
    "пятница", "суббота", "воскресенье"};

int weekday_index(const Date& date) {
    return QDate(
        date.year(), date.month(), date.day()).dayOfWeek();
}

QString weekday_name_short(const Date& date) {
    const int index = weekday_index(date) - 1;
    return QString::fromUtf8(kWeekdayShort[index]);
}

QString weekday_name_full(const Date& date) {
    const int index = weekday_index(date) - 1;
    return QString::fromUtf8(kWeekdayFull[index]);
}

QString format_date(const Date& value) {
    return QStringLiteral("%1.%2.%3")
        .arg(value.day(), 2, 10, QLatin1Char('0'))
        .arg(value.month(), 2, 10, QLatin1Char('0'))
        .arg(value.year(), 4, 10, QLatin1Char('0'));
}

const WorkSchedule* find_schedule(
    const std::vector<WorkSchedule>& schedules,
    int64_t id) {
    for (const WorkSchedule& schedule : schedules) {
        if (schedule.id == id) {
            return &schedule;
        }
    }
    return nullptr;
}

const AbsenceKind* find_absence_kind(
    const std::vector<AbsenceKind>& kinds,
    int64_t id) {
    for (const AbsenceKind& kind : kinds) {
        if (kind.id == id) {
            return &kind;
        }
    }
    return nullptr;
}

void collect_facts(
    const std::vector<TimeFact>& facts,
    const Date& date,
    int32_t normative_minutes,
    bool working_day,
    TimesheetDayCell& cell) {
    int64_t worked_minutes = 0;
    int64_t fact_count = 0;
    const TimeFact* first_fact = nullptr;

    for (const TimeFact& fact : facts) {
        if (fact.date != date) {
            continue;
        }
        ++fact_count;
        if (first_fact == nullptr) {
            first_fact = &fact;
        }
        worked_minutes += fact.worked_minutes;
    }

    if (first_fact == nullptr) {
        cell.absence_minutes =
            working_day ? normative_minutes : 0;
        return;
    }

    if (worked_minutes >
        std::numeric_limits<int32_t>::max()) {
        throw std::runtime_error(
            "too many worked minutes in a day");
    }

    cell.has_fact = true;
    cell.time_fact_id = first_fact->id;
    cell.worked_minutes =
        static_cast<int32_t>(worked_minutes);
    cell.multiple_facts = fact_count > 1;
    cell.absence_kind_id = first_fact->absence_kind_id;
    cell.absence_minutes =
        working_day
            ? std::max(
                  normative_minutes - cell.worked_minutes, 0)
            : 0;
}

}

TimesheetTableModel::TimesheetTableModel(QObject* parent)
    : QAbstractTableModel(parent) {}

int TimesheetTableModel::rowCount(
    const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(rows_.size());
}

int TimesheetTableModel::columnCount(
    const QModelIndex& parent) const {
    if (parent.isValid() || days_ == 0) {
        return 0;
    }
    return InfoColumnCount + days_ + TotalColumnCount;
}

QVariant TimesheetTableModel::data(
    const QModelIndex& index,
    int role) const {
    if (!index.isValid()) {
        return {};
    }

    const TimesheetEmployeeRow* row = row_at(index.row());
    if (row == nullptr) {
        return {};
    }

    if (index.column() < InfoColumnCount) {
        if (role == Qt::DisplayRole || role == Qt::ToolTipRole) {
            switch (index.column()) {
            case FullName:
                return row->full_name;
            case PersonnelNumber:
                return row->personnel_number;
            case Position:
                return row->position;
            default:
                return {};
            }
        }
        return {};
    }

    if (index.column() < InfoColumnCount + days_) {
        const TimesheetDayCell* cell = cell_at(*row, index.column());
        if (cell == nullptr) {
            return {};
        }

        switch (role) {
        case Qt::DisplayRole: {
            if (!row->schedule_available) {
                return QStringLiteral("—");
            }
            if (!cell->working_day) {
                return QStringLiteral("В");
            }
            if (!cell->absence_code.isEmpty() &&
                cell->worked_minutes == 0) {
                return cell->absence_code;
            }
            if (cell->has_fact || cell->edited ||
                cell->worked_minutes > 0) {
                return format_minutes(cell->worked_minutes);
            }
            return QStringLiteral("—");
        }
        case Qt::EditRole: {
            const Qt::ItemFlags cell_flags = flags(index);
            if (!(cell_flags & Qt::ItemIsEditable)) {
                return {};
            }
            return cell->has_fact
                ? QVariant(format_minutes(cell->worked_minutes))
                : QVariant(QString());
        }
        case Qt::ToolTipRole: {
            QString tooltip = format_date(cell->date) +
                QStringLiteral(", ") +
                weekday_name_full(cell->date) + QLatin1Char('.');
            if (!row->schedule_available) {
                tooltip += QStringLiteral(
                    "\nГрафик работы не найден.");
                return tooltip;
            }
            if (cell->working_day) {
                tooltip += QStringLiteral("\nРабочий день. Норма: %1.")
                    .arg(format_minutes(cell->normative_minutes));
            } else {
                tooltip += QStringLiteral("\nВыходной день.");
            }
            if (cell->has_fact) {
                tooltip += QStringLiteral("\nФакт: %1.")
                    .arg(format_minutes(cell->worked_minutes));
            }
            if (!cell->absence_name.isEmpty()) {
                tooltip += QStringLiteral("\nОтсутствие: %1.")
                    .arg(cell->absence_name);
            }
            if (cell->multiple_facts) {
                tooltip += QStringLiteral(
                    "\nЗа день несколько записей TimeFact — "
                    "редактирование в табеле недоступно.");
            } else if (cell->working_day) {
                tooltip += QStringLiteral(
                    "\nВведите часы: 8, 8,5 или 8:30.");
            }
            return tooltip;
        }
        case Qt::BackgroundRole: {
            if (!row->schedule_available) {
                return QBrush(QColor(QStringLiteral("#F8FAFC")));
            }
            if (!cell->working_day) {
                return QBrush(QColor(QStringLiteral("#EEF2F7")));
            }
            if (cell->edited) {
                return QBrush(QColor(QStringLiteral("#FEF3C7")));
            }
            return {};
        }
        case Qt::ForegroundRole: {
            if (!row->schedule_available ||
                !cell->working_day) {
                return QBrush(QColor(QStringLiteral("#94A3B8")));
            }
            return {};
        }
        case Qt::TextAlignmentRole:
            return static_cast<int>(
                Qt::AlignHCenter | Qt::AlignVCenter);
        case Qt::UserRole:
            return QVariant::fromValue(
                static_cast<qlonglong>(cell->worked_minutes));
        case Qt::UserRole + 1:
            return cell->working_day;
        case Qt::UserRole + 2:
            return cell->has_fact;
        default:
            return {};
        }
    }

    const int total_index =
        index.column() - InfoColumnCount - days_;
    switch (role) {
    case Qt::DisplayRole:
        switch (total_index) {
        case WorkedHours:
            return format_minutes(
                total_worked_minutes(*row));
        case WorkedDays:
            return total_worked_days(*row);
        case MissedHours:
            return format_minutes(
                total_missed_minutes(*row));
        case MissedDays:
            return total_missed_days(*row);
        default:
            return {};
        }
    case Qt::TextAlignmentRole:
        return static_cast<int>(
            (total_index == WorkedHours ||
             total_index == MissedHours)
                ? (Qt::AlignRight | Qt::AlignVCenter)
                : (Qt::AlignHCenter | Qt::AlignVCenter));
    default:
        return {};
    }
}

QVariant TimesheetTableModel::headerData(
    int section,
    Qt::Orientation orientation,
    int role) const {
    if (orientation != Qt::Horizontal) {
        return {};
    }
    if (section < 0 || section >= columnCount()) {
        return {};
    }

    if (section < InfoColumnCount) {
        if (role != Qt::DisplayRole) {
            return {};
        }
        switch (section) {
        case FullName:
            return QStringLiteral("ФИО");
        case PersonnelNumber:
            return QStringLiteral("Табельный номер");
        case Position:
            return QStringLiteral("Должность");
        default:
            return {};
        }
    }

    if (section < InfoColumnCount + days_) {
        const int day = section - InfoColumnCount + 1;
        const Date date(year_, month_, day);
        if (role == Qt::DisplayRole) {
            return QString::number(day);
        }
        if (role == Qt::ToolTipRole) {
            return QStringLiteral("%1, %2")
                .arg(format_date(date), weekday_name_short(date));
        }
        return {};
    }

    if (role != Qt::DisplayRole) {
        return {};
    }
    switch (section - InfoColumnCount - days_) {
    case WorkedHours:
        return QStringLiteral("Отработано, ч");
    case WorkedDays:
        return QStringLiteral("Отработано, дн.");
    case MissedHours:
        return QStringLiteral("Пропущено, ч");
    case MissedDays:
        return QStringLiteral("Пропущено, дн.");
    default:
        return {};
    }
}

Qt::ItemFlags TimesheetTableModel::flags(
    const QModelIndex& index) const {
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }

    Qt::ItemFlags result =
        Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    const TimesheetEmployeeRow* row = row_at(index.row());
    if (row == nullptr || !row->schedule_available) {
        return result;
    }

    const TimesheetDayCell* cell = cell_at(*row, index.column());
    if (cell != nullptr && cell->working_day &&
        !cell->multiple_facts) {
        result |= Qt::ItemIsEditable;
    }
    return result;
}

bool TimesheetTableModel::setData(
    const QModelIndex& index,
    const QVariant& value,
    int role) {
    if (role != Qt::EditRole) {
        return false;
    }

    const TimesheetEmployeeRow* row = nullptr;
    TimesheetDayCell* cell = editable_cell(index, &row);
    if (cell == nullptr || row == nullptr) {
        return false;
    }

    bool ok = false;
    const int minutes = value.toInt(&ok);
    if (!ok || minutes < 0 || minutes > 24 * 60) {
        return false;
    }

    if (minutes == cell->worked_minutes && cell->has_fact) {
        return true;
    }

    if (minutes == 0 && cell->time_fact_id == 0 &&
        !cell->absence_kind_id.has_value()) {
        cell->worked_minutes = 0;
        cell->has_fact = false;
        cell->edited = false;
        cell->absence_minutes = cell->normative_minutes;
    } else {
        cell->worked_minutes = minutes;
        cell->has_fact = true;
        cell->edited = true;
        cell->absence_minutes = std::max(
            cell->normative_minutes - minutes, 0);
    }

    emit dataChanged(
        index,
        this->index(index.row(), columnCount() - 1),
        {Qt::DisplayRole, Qt::EditRole, Qt::ToolTipRole,
         Qt::BackgroundRole});
    return true;
}

void TimesheetTableModel::set_data(
    int32_t year,
    int32_t month,
    std::vector<TimesheetEmployeeSource> employees,
    const std::vector<WorkSchedule>& schedules,
    const std::vector<AbsenceKind>& absence_kinds) {
    beginResetModel();

    year_ = year;
    month_ = month;
    days_ = Date::days_in_month(year, month);
    rows_.clear();
    warnings_.clear();
    rows_.reserve(employees.size());

    for (TimesheetEmployeeSource& source : employees) {
        TimesheetEmployeeRow row;
        row.employee_id = source.employee.id;
        row.full_name = QString::fromStdString(
            source.employee.full_name);
        row.personnel_number = QString::fromStdString(
            source.employee.personnel_number);
        row.position = source.position_name;

        std::vector<NormDay> calendar;
        const WorkSchedule* schedule = find_schedule(
            schedules, source.employee.work_schedule_id);
        if (schedule == nullptr) {
            row.schedule_available = false;
            warnings_.append(
                QStringLiteral("%1: график работы не найден.")
                    .arg(row.full_name));
        } else {
            try {
                calendar = generate_norm_calendar(
                    *schedule, year, month);
            } catch (const std::exception& error) {
                row.schedule_available = false;
                warnings_.append(
                    QStringLiteral("%1: %2")
                        .arg(
                            row.full_name,
                            QString::fromUtf8(error.what())));
            }
        }

        row.days.reserve(static_cast<std::size_t>(days_));
        for (int day = 1; day <= days_; ++day) {
            TimesheetDayCell cell;
            cell.date = Date(year, month, day);
            if (row.schedule_available &&
                day <= static_cast<int>(calendar.size())) {
                const NormDay& norm_day =
                    calendar[static_cast<std::size_t>(day - 1)];
                cell.working_day = norm_day.working_day;
                cell.normative_minutes = norm_day.normative_minutes;
            }

            collect_facts(
                source.time_facts,
                cell.date,
                cell.normative_minutes,
                cell.working_day,
                cell);

            if (cell.absence_kind_id.has_value()) {
                const AbsenceKind* kind = find_absence_kind(
                    absence_kinds, *cell.absence_kind_id);
                if (kind == nullptr) {
                    cell.absence_code = QStringLiteral("?");
                    warnings_.append(
                        QStringLiteral(
                            "%1, %2: неизвестный вид отсутствия.")
                            .arg(
                                row.full_name,
                                format_date(cell.date)));
                } else {
                    cell.absence_code =
                        QString::fromStdString(kind->code);
                    cell.absence_name =
                        QString::fromStdString(kind->name);
                }
            }

            row.days.push_back(std::move(cell));
        }

        rows_.push_back(std::move(row));
    }

    endResetModel();
}

void TimesheetTableModel::clear() {
    set_data(year_, month_, {}, {}, {});
}

int32_t TimesheetTableModel::year() const {
    return year_;
}

int32_t TimesheetTableModel::month() const {
    return month_;
}

int TimesheetTableModel::day_count() const {
    return days_;
}

int TimesheetTableModel::day_column(int day) const {
    if (day < 1 || day > days_) {
        return -1;
    }
    return InfoColumnCount + day - 1;
}

int TimesheetTableModel::total_column(
    TotalColumn column) const {
    if (days_ == 0 ||
        column < 0 || column >= TotalColumnCount) {
        return -1;
    }
    return InfoColumnCount + days_ + column;
}

int64_t TimesheetTableModel::employee_id_at(int row) const {
    const TimesheetEmployeeRow* value = row_at(row);
    return value == nullptr ? 0 : value->employee_id;
}

bool TimesheetTableModel::has_unsaved_changes() const {
    for (const TimesheetEmployeeRow& row : rows_) {
        for (const TimesheetDayCell& cell : row.days) {
            if (cell.edited) {
                return true;
            }
        }
    }
    return false;
}

std::vector<TimesheetPendingChange>
TimesheetTableModel::pending_changes() const {
    std::vector<TimesheetPendingChange> result;
    for (const TimesheetEmployeeRow& row : rows_) {
        for (const TimesheetDayCell& cell : row.days) {
            if (!cell.edited) {
                continue;
            }
            result.push_back({
                row.employee_id,
                cell.date,
                cell.time_fact_id,
                cell.absence_kind_id,
                cell.worked_minutes});
        }
    }
    return result;
}

QStringList TimesheetTableModel::warnings() const {
    return warnings_;
}

bool TimesheetTableModel::parse_hours(
    const QString& text,
    int32_t& minutes,
    QString& error) {
    QString value = text.trimmed();
    if (value.isEmpty()) {
        minutes = 0;
        return true;
    }
    value.replace(QLatin1Char(','), QLatin1Char('.'));

    if (value.contains(QLatin1Char(':'))) {
        const QStringList parts =
            value.split(QLatin1Char(':'));
        if (parts.size() != 2 ||
            parts[0].trimmed().isEmpty() ||
            parts[1].trimmed().isEmpty()) {
            error = QStringLiteral(
                "Введите часы в формате 8 или 8:30.");
            return false;
        }
        bool hours_ok = false;
        bool minutes_ok = false;
        const int hours =
            parts[0].trimmed().toInt(&hours_ok);
        const int remainder =
            parts[1].trimmed().toInt(&minutes_ok);
        if (!hours_ok || !minutes_ok ||
            hours < 0 || remainder < 0 || remainder > 59) {
            error = QStringLiteral(
                "Минуты должны быть от 0 до 59.");
            return false;
        }
        const int64_t total =
            static_cast<int64_t>(hours) * 60 + remainder;
        if (total > 24 * 60) {
            error = QStringLiteral(
                "За день нельзя указать больше 24 часов.");
            return false;
        }
        minutes = static_cast<int32_t>(total);
    } else {
        bool ok = false;
        const double hours = value.toDouble(&ok);
        if (!ok || !std::isfinite(hours) || hours < 0) {
            error = QStringLiteral(
                "Введите неотрицательное число часов, "
                "например 8 или 8,5.");
            return false;
        }
        if (hours > 24.0) {
            error = QStringLiteral(
                "За день нельзя указать больше 24 часов.");
            return false;
        }
        minutes = static_cast<int32_t>(
            std::llround(hours * 60.0));
    }

    if (minutes > 24 * 60) {
        error = QStringLiteral(
            "За день нельзя указать больше 24 часов.");
        return false;
    }
    return true;
}

QString TimesheetTableModel::format_minutes(
    int32_t minutes) {
    if (minutes < 0) {
        minutes = 0;
    }
    return QStringLiteral("%1:%2")
        .arg(minutes / 60)
        .arg(
            minutes % 60,
            2,
            10,
            QLatin1Char('0'));
}

const TimesheetEmployeeRow* TimesheetTableModel::row_at(
    int row) const {
    if (row < 0 || row >= static_cast<int>(rows_.size())) {
        return nullptr;
    }
    return &rows_[static_cast<std::size_t>(row)];
}

const TimesheetDayCell* TimesheetTableModel::cell_at(
    const TimesheetEmployeeRow& row,
    int column) const {
    const int index = column - InfoColumnCount;
    if (index < 0 ||
        index >= static_cast<int>(row.days.size())) {
        return nullptr;
    }
    return &row.days[static_cast<std::size_t>(index)];
}

TimesheetDayCell* TimesheetTableModel::editable_cell(
    const QModelIndex& index,
    const TimesheetEmployeeRow** row) {
    if (!index.isValid()) {
        return nullptr;
    }
    if (index.row() < 0 ||
        index.row() >= static_cast<int>(rows_.size())) {
        return nullptr;
    }

    TimesheetEmployeeRow& value =
        rows_[static_cast<std::size_t>(index.row())];
    if (row != nullptr) {
        *row = &value;
    }
    if (!value.schedule_available) {
        return nullptr;
    }

    const int day_index = index.column() - InfoColumnCount;
    if (day_index < 0 ||
        day_index >= static_cast<int>(value.days.size())) {
        return nullptr;
    }

    TimesheetDayCell& cell =
        value.days[static_cast<std::size_t>(day_index)];
    if (!cell.working_day || cell.multiple_facts) {
        return nullptr;
    }
    return &cell;
}

int32_t TimesheetTableModel::total_worked_minutes(
    const TimesheetEmployeeRow& row) const {
    int32_t result = 0;
    for (const TimesheetDayCell& cell : row.days) {
        result += cell.worked_minutes;
    }
    return result;
}

int32_t TimesheetTableModel::total_worked_days(
    const TimesheetEmployeeRow& row) const {
    int32_t result = 0;
    for (const TimesheetDayCell& cell : row.days) {
        if (cell.has_fact && cell.worked_minutes > 0) {
            ++result;
        }
    }
    return result;
}

int32_t TimesheetTableModel::total_missed_minutes(
    const TimesheetEmployeeRow& row) const {
    int32_t result = 0;
    for (const TimesheetDayCell& cell : row.days) {
        result += cell.absence_minutes;
    }
    return result;
}

int32_t TimesheetTableModel::total_missed_days(
    const TimesheetEmployeeRow& row) const {
    int32_t result = 0;
    for (const TimesheetDayCell& cell : row.days) {
        if (cell.absence_minutes > 0) {
            ++result;
        }
    }
    return result;
}

}
