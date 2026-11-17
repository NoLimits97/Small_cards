#include "employee_table_model.hpp"

#include <QStringList>
#include <QVariant>

#include <utility>

namespace mk::gui {

EmployeeTableModel::EmployeeTableModel(QObject* parent)
    : QAbstractTableModel(parent) {}

int EmployeeTableModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(rows_.size());
}

int EmployeeTableModel::columnCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return ColumnCount;
}

QVariant EmployeeTableModel::data(
    const QModelIndex& index,
    int role) const {
    if (!index.isValid() ||
        index.row() < 0 ||
        index.row() >= static_cast<int>(rows_.size())) {
        return {};
    }

    const EmployeeTableRow& row =
        rows_[static_cast<std::size_t>(index.row())];
    if (role == Qt::UserRole) {
        return QVariant::fromValue(static_cast<qlonglong>(row.id));
    }
    if (role != Qt::DisplayRole) {
        return {};
    }

    switch (index.column()) {
    case FullName:
        return row.full_name;
    case PersonnelNumber:
        return row.personnel_number;
    case Position:
        return row.position;
    case Department:
        return row.department;
    case Status:
        return row.status;
    default:
        return {};
    }
}

QVariant EmployeeTableModel::headerData(
    int section,
    Qt::Orientation orientation,
    int role) const {
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal) {
        return {};
    }

    switch (section) {
    case FullName:
        return QStringLiteral("ФИО");
    case PersonnelNumber:
        return QStringLiteral("Табельный номер");
    case Position:
        return QStringLiteral("Должность");
    case Department:
        return QStringLiteral("Подразделение");
    case Status:
        return QStringLiteral("Статус");
    default:
        return {};
    }
}

void EmployeeTableModel::set_rows(std::vector<EmployeeTableRow> rows) {
    beginResetModel();
    rows_ = std::move(rows);
    endResetModel();
}

void EmployeeTableModel::clear() {
    set_rows({});
}

int64_t EmployeeTableModel::employee_id_at(int row) const {
    if (row < 0 || row >= static_cast<int>(rows_.size())) {
        return 0;
    }
    return rows_[static_cast<std::size_t>(row)].id;
}

int EmployeeTableModel::row_for_employee(int64_t employee_id) const {
    for (std::size_t index = 0; index < rows_.size(); ++index) {
        if (rows_[index].id == employee_id) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

}
