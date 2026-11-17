#ifndef MK_GUI_EMPLOYEE_TABLE_MODEL_HPP
#define MK_GUI_EMPLOYEE_TABLE_MODEL_HPP

#include <QAbstractTableModel>
#include <QString>

#include <cstdint>
#include <vector>

namespace mk::gui {

struct EmployeeTableRow {
    int64_t id = 0;
    QString full_name;
    QString personnel_number;
    QString position;
    QString department;
    QString status;
};

class EmployeeTableModel : public QAbstractTableModel {
public:
    enum Column : int {
        FullName = 0,
        PersonnelNumber,
        Position,
        Department,
        Status,
        ColumnCount
    };

    explicit EmployeeTableModel(QObject* parent = nullptr);

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

    void set_rows(std::vector<EmployeeTableRow> rows);
    void clear();

    int64_t employee_id_at(int row) const;
    int row_for_employee(int64_t employee_id) const;

private:
    std::vector<EmployeeTableRow> rows_;
};

}

#endif
