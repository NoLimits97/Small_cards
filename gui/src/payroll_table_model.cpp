#include "payroll_table_model.hpp"

#include "gui_format.hpp"
#include "money_formatter.hpp"

#include <QVariant>

#include <limits>
#include <utility>

namespace mk::gui {
namespace {

const char* const kNotCalculated = "не рассчитывается";

bool is_money_column(int column) {
    return column == PayrollTableModel::Accrual ||
        column == PayrollTableModel::Deductions ||
        column == PayrollTableModel::Net;
}

}

PayrollTableModel::PayrollTableModel(QObject* parent)
    : QAbstractTableModel(parent) {}

int PayrollTableModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}

int PayrollTableModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant PayrollTableModel::data(
    const QModelIndex& index,
    int role) const {
    if (!index.isValid() ||
        index.row() < 0 ||
        index.row() >= static_cast<int>(rows_.size()) ||
        index.column() < 0 ||
        index.column() >= ColumnCount) {
        return {};
    }

    const PayrollTableRow& row =
        rows_[static_cast<std::size_t>(index.row())];
    const int column = index.column();

    if (role == Qt::TextAlignmentRole && column != FullName &&
        column != Position && column != Status) {
        return static_cast<int>(
            Qt::AlignRight | Qt::AlignVCenter);
    }

    if (role == Qt::ToolTipRole && !row.reasons.isEmpty()) {
        return row.reasons;
    }

    if (role == Qt::DisplayRole) {
        if (is_money_column(column)) {
            if (!row.calculated) {
                return QString::fromUtf8(kNotCalculated);
            }
            const int64_t cents = column == Accrual
                ? row.accrual_cents
                : (column == Deductions
                       ? row.deductions_cents
                       : row.net_cents);
            return format_money(Money(cents));
        }

        switch (column) {
            case FullName:
                return row.full_name;
            case Position:
                return row.position;
            case WorkedHours:
                return format_minutes(row.worked_minutes);
            case WorkedDays:
                return QString::number(row.worked_days);
            case Status:
                return row.status;
            default:
                break;
        }
    }

    if (role == Qt::UserRole) {
        if (is_money_column(column)) {
            if (!row.calculated) {
                return QVariant::fromValue(
                    std::numeric_limits<qlonglong>::min());
            }
            const int64_t cents = column == Accrual
                ? row.accrual_cents
                : (column == Deductions
                       ? row.deductions_cents
                       : row.net_cents);
            return QVariant::fromValue(
                static_cast<qlonglong>(cents));
        }

        switch (column) {
            case FullName:
                return row.full_name;
            case Position:
                return row.position;
            case WorkedHours:
                return QVariant::fromValue(
                    static_cast<qlonglong>(row.worked_minutes));
            case WorkedDays:
                return QVariant::fromValue(
                    static_cast<qlonglong>(row.worked_days));
            case Status:
                return row.status;
            default:
                break;
        }
    }

    return {};
}

QVariant PayrollTableModel::headerData(
    int section,
    Qt::Orientation orientation,
    int role) const {
    if (role != Qt::DisplayRole ||
        orientation != Qt::Horizontal) {
        return QAbstractTableModel::headerData(
            section, orientation, role);
    }

    switch (section) {
        case FullName:
            return QStringLiteral("Сотрудник");
        case Position:
            return QStringLiteral("Должность");
        case WorkedHours:
            return QStringLiteral("Отработано, ч:мм");
        case WorkedDays:
            return QStringLiteral("Отработано, дней");
        case Accrual:
            return QStringLiteral("Начислено, ₽");
        case Deductions:
            return QStringLiteral("Удержания, ₽");
        case Net:
            return QStringLiteral("К выплате, ₽");
        case Status:
            return QStringLiteral("Статус");
        default:
            break;
    }

    return {};
}

void PayrollTableModel::set_rows(
    std::vector<PayrollTableRow> rows) {
    beginResetModel();
    rows_ = std::move(rows);
    endResetModel();
}

void PayrollTableModel::clear() {
    set_rows({});
}

int64_t PayrollTableModel::employee_id_at(int row) const {
    if (row < 0 || row >= static_cast<int>(rows_.size())) {
        return 0;
    }
    return rows_[static_cast<std::size_t>(row)].employee_id;
}

int PayrollTableModel::calculated_row_count() const {
    int result = 0;
    for (const PayrollTableRow& row : rows_) {
        if (row.calculated) {
            ++result;
        }
    }
    return result;
}

}
