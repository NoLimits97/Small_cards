#include "test_framework.hpp"

#include "payroll_table_model.hpp"

#include <QSortFilterProxyModel>
#include <QString>
#include <QVariant>

#include <cstdint>
#include <vector>

namespace {

mk::gui::PayrollTableRow make_calculated(
    int64_t id,
    const QString& name,
    int64_t accrual_cents,
    int64_t deductions_cents,
    int64_t net_cents) {
    mk::gui::PayrollTableRow row;
    row.employee_id = id;
    row.full_name = name;
    row.position = QStringLiteral("Бухгалтер");
    row.worked_minutes = 10560;
    row.worked_days = 22;
    row.calculated = true;
    row.accrual_cents = accrual_cents;
    row.deductions_cents = deductions_cents;
    row.net_cents = net_cents;
    row.status = QStringLiteral("Рассчитано");
    return row;
}

mk::gui::PayrollTableRow make_not_calculated(
    int64_t id,
    const QString& name) {
    mk::gui::PayrollTableRow row;
    row.employee_id = id;
    row.full_name = name;
    row.position = QStringLiteral("Сотрудник");
    row.worked_minutes = 0;
    row.worked_days = 0;
    row.calculated = false;
    row.status = QStringLiteral("Не рассчитывается");
    row.reasons = QStringLiteral("не задана ставка (оклад)");
    return row;
}

}

TEST(payroll_table_model_reports_rows_and_columns) {
    mk::gui::PayrollTableModel model;
    REQUIRE_EQUAL(model.rowCount(), 0);
    REQUIRE_EQUAL(model.columnCount(), 8);
    REQUIRE_EQUAL(
        int(mk::gui::PayrollTableModel::ColumnCount), 8);
    REQUIRE_EQUAL(model.calculated_row_count(), 0);
}

TEST(payroll_table_model_exposes_headers_and_values) {
    mk::gui::PayrollTableModel model;
    model.set_rows({
        make_calculated(
            10,
            QStringLiteral("Иванов Иван"),
            10000000,
            0,
            10000000)});

    REQUIRE_EQUAL(
        model.headerData(
            mk::gui::PayrollTableModel::FullName,
            Qt::Horizontal,
            Qt::DisplayRole).toString(),
        QStringLiteral("Сотрудник"));
    REQUIRE_EQUAL(
        model.headerData(
            mk::gui::PayrollTableModel::WorkedHours,
            Qt::Horizontal,
            Qt::DisplayRole).toString(),
        QStringLiteral("Отработано, ч:мм"));
    REQUIRE_EQUAL(
        model.headerData(
            mk::gui::PayrollTableModel::Net,
            Qt::Horizontal,
            Qt::DisplayRole).toString(),
        QStringLiteral("К выплате, ₽"));

    REQUIRE_EQUAL(
        model.index(0, mk::gui::PayrollTableModel::FullName)
            .data(Qt::DisplayRole).toString(),
        QStringLiteral("Иванов Иван"));
    REQUIRE_EQUAL(
        model.index(0, mk::gui::PayrollTableModel::WorkedHours)
            .data(Qt::DisplayRole).toString(),
        QStringLiteral("176:00"));
    REQUIRE_EQUAL(
        model.index(0, mk::gui::PayrollTableModel::WorkedDays)
            .data(Qt::DisplayRole).toString(),
        QStringLiteral("22"));
    REQUIRE_EQUAL(
        model.index(0, mk::gui::PayrollTableModel::Accrual)
            .data(Qt::DisplayRole).toString(),
        QStringLiteral("100000,00"));
    REQUIRE_EQUAL(
        model.index(0, mk::gui::PayrollTableModel::Net)
            .data(Qt::DisplayRole).toString(),
        QStringLiteral("100000,00"));
    REQUIRE_EQUAL(model.calculated_row_count(), 1);
}

TEST(payroll_table_model_hides_amounts_for_uncalculated_rows) {
    mk::gui::PayrollTableModel model;
    model.set_rows({
        make_not_calculated(
            20, QStringLiteral("Петров Пётр"))});

    REQUIRE_EQUAL(
        model.index(0, mk::gui::PayrollTableModel::Accrual)
            .data(Qt::DisplayRole).toString(),
        QStringLiteral("не рассчитывается"));
    REQUIRE_EQUAL(
        model.index(0, mk::gui::PayrollTableModel::Deductions)
            .data(Qt::DisplayRole).toString(),
        QStringLiteral("не рассчитывается"));
    REQUIRE_EQUAL(
        model.index(0, mk::gui::PayrollTableModel::Net)
            .data(Qt::DisplayRole).toString(),
        QStringLiteral("не рассчитывается"));
    REQUIRE_EQUAL(
        model.index(0, mk::gui::PayrollTableModel::Status)
            .data(Qt::DisplayRole).toString(),
        QStringLiteral("Не рассчитывается"));
    REQUIRE_EQUAL(
        model.index(0, mk::gui::PayrollTableModel::Status)
            .data(Qt::ToolTipRole).toString(),
        QStringLiteral("не задана ставка (оклад)"));
    REQUIRE_EQUAL(model.calculated_row_count(), 0);
}

TEST(payroll_table_model_sorts_numeric_columns_through_proxy) {
    mk::gui::PayrollTableModel model;
    model.set_rows({
        make_calculated(
            30,
            QStringLiteral("Второй"),
            5000000,
            0,
            5000000),
        make_calculated(
            31,
            QStringLiteral("Первый"),
            10000000,
            0,
            10000000)});

    QSortFilterProxyModel proxy;
    proxy.setSourceModel(&model);
    proxy.setSortRole(Qt::UserRole);
    proxy.sort(
        mk::gui::PayrollTableModel::Net,
        Qt::AscendingOrder);

    REQUIRE_EQUAL(
        proxy.index(0, mk::gui::PayrollTableModel::FullName)
            .data(Qt::DisplayRole).toString(),
        QStringLiteral("Второй"));
    REQUIRE_EQUAL(
        proxy.index(1, mk::gui::PayrollTableModel::FullName)
            .data(Qt::DisplayRole).toString(),
        QStringLiteral("Первый"));
}

TEST(payroll_table_model_clears_rows) {
    mk::gui::PayrollTableModel model;
    model.set_rows({make_not_calculated(40, QStringLiteral("Сотрудник"))});
    REQUIRE_EQUAL(model.employee_id_at(0), int64_t{40});

    model.clear();
    REQUIRE_EQUAL(model.rowCount(), 0);
    REQUIRE_EQUAL(model.employee_id_at(0), int64_t{0});
}
