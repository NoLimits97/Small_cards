#ifndef MK_GUI_PAYROLL_TABLE_MODEL_HPP
#define MK_GUI_PAYROLL_TABLE_MODEL_HPP

#include <QAbstractTableModel>
#include <QString>

#include <cstdint>
#include <vector>

namespace mk::gui {

struct PayrollTableRow {
    int64_t employee_id = 0;
    QString full_name;
    QString position;
    int64_t worked_minutes = 0;
    int64_t worked_days = 0;
    bool calculated = false;
    int64_t accrual_cents = 0;
    int64_t deductions_cents = 0;
    int64_t net_cents = 0;
    QString status;
    QString reasons;
};

class PayrollTableModel : public QAbstractTableModel {
public:
    enum Column : int {
        FullName = 0,
        Position,
        WorkedHours,
        WorkedDays,
        Accrual,
        Deductions,
        Net,
        Status,
        ColumnCount
    };

    explicit PayrollTableModel(QObject* parent = nullptr);

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

    void set_rows(std::vector<PayrollTableRow> rows);
    void clear();

    int64_t employee_id_at(int row) const;
    int calculated_row_count() const;

private:
    std::vector<PayrollTableRow> rows_;
};

}

#endif
