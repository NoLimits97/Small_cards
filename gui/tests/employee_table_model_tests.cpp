#include "test_framework.hpp"

#include "employee_table_model.hpp"

#include <QSortFilterProxyModel>
#include <QString>
#include <QVariant>

#include <cstdint>
#include <vector>

namespace {

mk::gui::EmployeeTableRow make_row(
    int64_t id,
    const QString& name,
    const QString& number) {
    return mk::gui::EmployeeTableRow{
        id,
        name,
        number,
        QStringLiteral("Бухгалтер"),
        QStringLiteral("Бухгалтерия"),
        QStringLiteral("Работает")};
}

std::vector<mk::gui::EmployeeTableRow> sample_rows() {
    return {
        make_row(10, QStringLiteral("Zeta, Zoya"), QStringLiteral("000010")),
        make_row(20, QStringLiteral("Alpha, Anna"), QStringLiteral("000020"))};
}

}

TEST(employee_table_model_reports_rows_and_columns) {
    mk::gui::EmployeeTableModel model;
    REQUIRE_EQUAL(model.rowCount(), 0);
    REQUIRE_EQUAL(model.columnCount(), 5);
    REQUIRE_EQUAL(
        int(mk::gui::EmployeeTableModel::ColumnCount), 5);
}

TEST(employee_table_model_exposes_values_and_headers) {
    mk::gui::EmployeeTableModel model;
    model.set_rows(sample_rows());

    REQUIRE_EQUAL(model.rowCount(), 2);
    REQUIRE_EQUAL(
        model.headerData(
            0, Qt::Horizontal, Qt::DisplayRole).toString(),
        QStringLiteral("ФИО"));
    REQUIRE_EQUAL(
        model.headerData(
            1, Qt::Horizontal, Qt::DisplayRole).toString(),
        QStringLiteral("Табельный номер"));
    REQUIRE_EQUAL(
        model.headerData(
            2, Qt::Horizontal, Qt::DisplayRole).toString(),
        QStringLiteral("Должность"));
    REQUIRE_EQUAL(
        model.headerData(
            3, Qt::Horizontal, Qt::DisplayRole).toString(),
        QStringLiteral("Подразделение"));
    REQUIRE_EQUAL(
        model.headerData(
            4, Qt::Horizontal, Qt::DisplayRole).toString(),
        QStringLiteral("Статус"));

    REQUIRE_EQUAL(
        model.index(0, mk::gui::EmployeeTableModel::FullName)
            .data(Qt::DisplayRole).toString(),
        QStringLiteral("Zeta, Zoya"));
    REQUIRE_EQUAL(
        model.index(1, mk::gui::EmployeeTableModel::PersonnelNumber)
            .data(Qt::DisplayRole).toString(),
        QStringLiteral("000020"));
    REQUIRE_EQUAL(
        model.index(0, mk::gui::EmployeeTableModel::Position)
            .data(Qt::DisplayRole).toString(),
        QStringLiteral("Бухгалтер"));
    REQUIRE_EQUAL(
        model.index(0, mk::gui::EmployeeTableModel::Department)
            .data(Qt::DisplayRole).toString(),
        QStringLiteral("Бухгалтерия"));
    REQUIRE_EQUAL(
        model.index(0, mk::gui::EmployeeTableModel::Status)
            .data(Qt::DisplayRole).toString(),
        QStringLiteral("Работает"));
}

TEST(employee_table_model_maps_ids_to_rows) {
    mk::gui::EmployeeTableModel model;
    model.set_rows(sample_rows());

    REQUIRE_EQUAL(model.employee_id_at(0), int64_t{10});
    REQUIRE_EQUAL(model.employee_id_at(1), int64_t{20});
    REQUIRE_EQUAL(model.employee_id_at(2), int64_t{0});
    REQUIRE_EQUAL(model.employee_id_at(-1), int64_t{0});
    REQUIRE_EQUAL(model.row_for_employee(20), 1);
    REQUIRE_EQUAL(model.row_for_employee(999), -1);

    REQUIRE_EQUAL(
        model.index(1, 0).data(Qt::UserRole).toLongLong(),
        qlonglong{20});
}

TEST(employee_table_model_clears_rows) {
    mk::gui::EmployeeTableModel model;
    model.set_rows(sample_rows());
    model.clear();

    REQUIRE_EQUAL(model.rowCount(), 0);
    REQUIRE_EQUAL(model.employee_id_at(0), int64_t{0});
}

TEST(employee_table_model_sorts_through_proxy) {
    mk::gui::EmployeeTableModel model;
    model.set_rows(sample_rows());

    QSortFilterProxyModel proxy;
    proxy.setSourceModel(&model);
    proxy.setSortCaseSensitivity(Qt::CaseInsensitive);
    proxy.sort(
        mk::gui::EmployeeTableModel::FullName,
        Qt::AscendingOrder);

    REQUIRE_EQUAL(proxy.rowCount(), 2);
    REQUIRE_EQUAL(
        proxy.index(0, mk::gui::EmployeeTableModel::FullName)
            .data(Qt::DisplayRole).toString(),
        QStringLiteral("Alpha, Anna"));
    REQUIRE_EQUAL(
        proxy.index(1, mk::gui::EmployeeTableModel::FullName)
            .data(Qt::DisplayRole).toString(),
        QStringLiteral("Zeta, Zoya"));
}
