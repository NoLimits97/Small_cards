#include "main_window.hpp"

#include "employee_table_model.hpp"
#include "gui_format.hpp"
#include "money_formatter.hpp"
#include "mk/org_details.hpp"
#include "mk/payroll_service.hpp"
#include "mk/repositories.hpp"
#include "navigation_sidebar.hpp"
#include "payroll_table_model.hpp"
#include "section_page.hpp"
#include "timesheet_table_model.hpp"
#include "workspace_tabs.hpp"

#include <QAbstractItemView>
#include <QCloseEvent>
#include <QComboBox>
#include <QDate>
#include <QDateEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSortFilterProxyModel>
#include <QSpinBox>
#include <QStringList>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVariant>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <exception>
#include <functional>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace mk::gui {
namespace {

bool create_employee_dialog(
    QWidget* parent,
    Database& database,
    int64_t org_id,
    int64_t& created_id) {
    QDialog dialog(parent);
    dialog.setWindowTitle(QStringLiteral("Создать сотрудника"));
    auto* layout = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout;

    auto* number = new QLineEdit(&dialog);
    auto* full_name = new QLineEdit(&dialog);
    form->addRow(QStringLiteral("Табельный номер:"), number);
    form->addRow(QStringLiteral("ФИО:"), full_name);

    auto* department = new QComboBox(&dialog);
    auto* position = new QComboBox(&dialog);
    auto* employment_kind = new QComboBox(&dialog);
    auto* schedule = new QComboBox(&dialog);
    auto* preset = new QComboBox(&dialog);
    preset->addItem(QStringLiteral("Без набора"), QVariant());

    try {
        for (const Department& value : list_departments(database, org_id)) {
            department->addItem(
                QString::fromStdString(value.name),
                QVariant::fromValue(static_cast<qlonglong>(value.id)));
        }
        for (const Position& value : list_positions(database)) {
            position->addItem(
                QString::fromStdString(value.name),
                QVariant::fromValue(static_cast<qlonglong>(value.id)));
        }
        for (const EmploymentKind& value : list_employment_kinds(database)) {
            employment_kind->addItem(
                QString::fromStdString(value.name),
                QVariant::fromValue(static_cast<qlonglong>(value.id)));
        }
        for (const WorkSchedule& value : list_work_schedules(database)) {
            schedule->addItem(
                QString::fromStdString(value.name),
                QVariant::fromValue(static_cast<qlonglong>(value.id)));
        }
        for (const PayrollPreset& value : list_payroll_presets(database)) {
            preset->addItem(
                QString::fromStdString(value.name),
                QVariant::fromValue(static_cast<qlonglong>(value.id)));
        }
    } catch (const std::exception& error) {
        QMessageBox::critical(
            &dialog,
            QStringLiteral("Ошибка загрузки справочников"),
            QString::fromUtf8(error.what()));
        return false;
    }

    if (department->count() == 0 ||
        position->count() == 0 ||
        employment_kind->count() == 0 ||
        schedule->count() == 0) {
        QMessageBox::warning(
            &dialog,
            QStringLiteral("Создать сотрудника"),
            QStringLiteral(
                "Для создания сотрудника нужны подразделение, "
                "должность, вид занятости и график работы."));
        return false;
    }

    form->addRow(QStringLiteral("Подразделение:"), department);
    form->addRow(QStringLiteral("Должность:"), position);
    form->addRow(QStringLiteral("Вид занятости:"), employment_kind);
    form->addRow(QStringLiteral("График работы:"), schedule);
    form->addRow(QStringLiteral("Набор начислений:"), preset);

    auto* rate = new QLineEdit(&dialog);
    rate->setPlaceholderText(QStringLiteral("0,00"));
    form->addRow(QStringLiteral("Ставка:"), rate);

    auto* rate_unit = new QComboBox(&dialog);
    rate_unit->addItem(
        QStringLiteral("В месяц"), static_cast<int>(RateUnit::Month));
    rate_unit->addItem(
        QStringLiteral("В день"), static_cast<int>(RateUnit::Day));
    rate_unit->addItem(
        QStringLiteral("В час"), static_cast<int>(RateUnit::Hour));
    form->addRow(QStringLiteral("Единица ставки:"), rate_unit);

    auto* hire_date = new QDateEdit(&dialog);
    hire_date->setCalendarPopup(true);
    hire_date->setDisplayFormat(QStringLiteral("dd.MM.yyyy"));
    hire_date->setMinimumDate(QDate(1, 1, 1));
    hire_date->setMaximumDate(QDate(9999, 12, 31));
    hire_date->setDate(QDate::currentDate());
    form->addRow(QStringLiteral("Дата приёма:"), hire_date);
    layout->addLayout(form);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]() {
        const QString trimmed_number = number->text().trimmed();
        const QString trimmed_name = full_name->text().trimmed();
        if (trimmed_number.isEmpty() || trimmed_name.isEmpty()) {
            QMessageBox::warning(
                &dialog,
                QStringLiteral("Создать сотрудника"),
                QStringLiteral("Введите табельный номер и ФИО."));
            return;
        }

        const MoneyParseResult parsed_rate = parse_money(rate->text());
        if (!parsed_rate.value.has_value()) {
            QMessageBox::warning(
                &dialog, QStringLiteral("Ставка"), parsed_rate.error);
            return;
        }
        if (parsed_rate.value->cents < 0) {
            QMessageBox::warning(
                &dialog,
                QStringLiteral("Ставка"),
                QStringLiteral("Ставка не может быть отрицательной."));
            return;
        }

        Employee employee;
        employee.personnel_number = trimmed_number.toStdString();
        employee.full_name = trimmed_name.toStdString();
        employee.org_id = org_id;
        employee.department_id = department->currentData().toLongLong();
        employee.position_id = position->currentData().toLongLong();
        employee.employment_kind_id = employment_kind->currentData().toLongLong();
        employee.work_schedule_id = schedule->currentData().toLongLong();
        if (preset->currentIndex() > 0) {
            employee.payroll_preset_id = preset->currentData().toLongLong();
        }
        employee.rate = *parsed_rate.value;
        employee.rate_unit =
            static_cast<RateUnit>(rate_unit->currentData().toInt());
        const QDate date = hire_date->date();
        employee.hire_date = Date(date.year(), date.month(), date.day());

        try {
            created_id = create_employee(database, employee);
            dialog.accept();
        } catch (const std::exception& error) {
            QMessageBox::critical(
                &dialog,
                QStringLiteral("Ошибка создания сотрудника"),
                QString::fromUtf8(error.what()));
        }
    });
    QObject::connect(
        buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    return dialog.exec() == QDialog::Accepted;
}

QDateEdit* create_date_edit(QWidget* parent) {
    auto* editor = new QDateEdit(parent);
    editor->setCalendarPopup(true);
    editor->setDisplayFormat(QStringLiteral("dd.MM.yyyy"));
    editor->setMinimumDate(QDate(1, 1, 1));
    editor->setMaximumDate(QDate(9999, 12, 31));
    editor->setDate(QDate::currentDate());
    return editor;
}

Date date_from_qdate(const QDate& value) {
    return Date(value.year(), value.month(), value.day());
}

QString department_name_or_placeholder(
    const std::vector<Department>& departments,
    int64_t id) {
    for (const Department& value : departments) {
        if (value.id == id) {
            return QString::fromStdString(value.name);
        }
    }
    return QStringLiteral("№%1").arg(id);
}

QString position_name_or_placeholder(
    const std::vector<Position>& positions,
    int64_t id) {
    for (const Position& value : positions) {
        if (value.id == id) {
            return QString::fromStdString(value.name);
        }
    }
    return QStringLiteral("№%1").arg(id);
}

QString employment_kind_name_or_placeholder(
    const std::vector<EmploymentKind>& kinds,
    int64_t id) {
    for (const EmploymentKind& value : kinds) {
        if (value.id == id) {
            return QString::fromStdString(value.name);
        }
    }
    return QStringLiteral("№%1").arg(id);
}

QString work_schedule_name_or_placeholder(
    const std::vector<WorkSchedule>& schedules,
    int64_t id) {
    for (const WorkSchedule& value : schedules) {
        if (value.id == id) {
            return QString::fromStdString(value.name);
        }
    }
    return QStringLiteral("№%1").arg(id);
}

QString preset_name_or_placeholder(
    const std::vector<PayrollPreset>& presets,
    const std::optional<int64_t>& id) {
    if (!id.has_value()) {
        return QStringLiteral("Без набора");
    }
    for (const PayrollPreset& value : presets) {
        if (value.id == *id) {
            return QString::fromStdString(value.name);
        }
    }
    return QStringLiteral("№%1").arg(*id);
}

void populate_combo(
    QComboBox* combo,
    const QStringList& names,
    const std::vector<int64_t>& ids,
    int64_t selected_id,
    bool add_empty) {
    if (add_empty) {
        combo->addItem(QStringLiteral("Без набора"), QVariant());
    }
    const qsizetype count =
        qMin(names.size(), static_cast<qsizetype>(ids.size()));
    for (qsizetype index = 0; index < count; ++index) {
        combo->addItem(
            names[index],
            QVariant::fromValue(
                static_cast<qlonglong>(ids[index])));
    }
    const int selected_index = combo->findData(
        QVariant::fromValue(
            static_cast<qlonglong>(selected_id)));
    if (selected_index >= 0) {
        combo->setCurrentIndex(selected_index);
    }
}

bool transfer_employee_dialog(
    QWidget* parent,
    Database& database,
    int64_t org_id,
    int64_t employee_id,
    const QString& employee_name) {
    QDialog dialog(parent);
    dialog.setWindowTitle(QStringLiteral("Перевод сотрудника"));
    auto* layout = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout;

    auto* name_label = new QLabel(employee_name, &dialog);
    form->addRow(QStringLiteral("Сотрудник:"), name_label);

    auto* date = create_date_edit(&dialog);
    form->addRow(QStringLiteral("Дата перевода:"), date);

    auto* department = new QComboBox(&dialog);
    auto* position = new QComboBox(&dialog);
    auto* employment_kind = new QComboBox(&dialog);
    auto* schedule = new QComboBox(&dialog);
    auto* preset = new QComboBox(&dialog);

    QStringList department_names;
    std::vector<int64_t> department_ids;
    QStringList position_names;
    std::vector<int64_t> position_ids;
    QStringList kind_names;
    std::vector<int64_t> kind_ids;
    QStringList schedule_names;
    std::vector<int64_t> schedule_ids;
    QStringList preset_names;
    std::vector<int64_t> preset_ids;
    int64_t current_department_id = 0;
    int64_t current_position_id = 0;
    int64_t current_kind_id = 0;
    int64_t current_schedule_id = 0;
    std::optional<int64_t> current_preset_id;

    try {
        const auto saved_employee = read_employee(database, employee_id);
        if (!saved_employee.has_value()) {
            throw std::runtime_error("сотрудник не найден");
        }
        current_department_id = saved_employee->department_id;
        current_position_id = saved_employee->position_id;
        current_kind_id = saved_employee->employment_kind_id;
        current_schedule_id = saved_employee->work_schedule_id;
        current_preset_id = saved_employee->payroll_preset_id;

        for (const Department& value : list_departments(database, org_id)) {
            department_names << QString::fromStdString(value.name);
            department_ids.push_back(value.id);
        }
        for (const Position& value : list_positions(database)) {
            position_names << QString::fromStdString(value.name);
            position_ids.push_back(value.id);
        }
        for (const EmploymentKind& value : list_employment_kinds(database)) {
            kind_names << QString::fromStdString(value.name);
            kind_ids.push_back(value.id);
        }
        for (const WorkSchedule& value : list_work_schedules(database)) {
            schedule_names << QString::fromStdString(value.name);
            schedule_ids.push_back(value.id);
        }
        for (const PayrollPreset& value : list_payroll_presets(database)) {
            preset_names << QString::fromStdString(value.name);
            preset_ids.push_back(value.id);
        }
    } catch (const std::exception& error) {
        QMessageBox::critical(
            &dialog,
            QStringLiteral("Ошибка загрузки справочников"),
            QString::fromUtf8(error.what()));
        return false;
    }

    if (department_ids.empty() || position_ids.empty() ||
        kind_ids.empty() || schedule_ids.empty()) {
        QMessageBox::warning(
            &dialog,
            QStringLiteral("Перевод сотрудника"),
            QStringLiteral(
                "Для перевода нужны подразделение, должность, "
                "вид занятости и график работы."));
        return false;
    }

    populate_combo(department, department_names, department_ids,
                   current_department_id, false);
    populate_combo(position, position_names, position_ids,
                   current_position_id, false);
    populate_combo(employment_kind, kind_names, kind_ids,
                   current_kind_id, false);
    populate_combo(schedule, schedule_names, schedule_ids,
                   current_schedule_id, false);
    populate_combo(preset, preset_names, preset_ids,
                   current_preset_id.value_or(0), true);

    form->addRow(QStringLiteral("Подразделение:"), department);
    form->addRow(QStringLiteral("Должность:"), position);
    form->addRow(QStringLiteral("Вид занятости:"), employment_kind);
    form->addRow(QStringLiteral("График работы:"), schedule);
    form->addRow(QStringLiteral("Набор начислений:"), preset);
    layout->addLayout(form);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]() {
        try {
            std::optional<int64_t> preset_id;
            if (preset->currentIndex() > 0) {
                preset_id = preset->currentData().toLongLong();
            }
            transfer_employee(
                database,
                org_id,
                employee_id,
                date_from_qdate(date->date()),
                department->currentData().toLongLong(),
                position->currentData().toLongLong(),
                employment_kind->currentData().toLongLong(),
                schedule->currentData().toLongLong(),
                preset_id);
            dialog.accept();
        } catch (const std::exception& error) {
            QMessageBox::critical(
                &dialog,
                QStringLiteral("Ошибка перевода"),
                QString::fromUtf8(error.what()));
        }
    });
    QObject::connect(
        buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    return dialog.exec() == QDialog::Accepted;
}

bool change_rate_dialog(
    QWidget* parent,
    Database& database,
    int64_t org_id,
    int64_t employee_id,
    const QString& employee_name,
    Money current_rate,
    RateUnit current_unit) {
    QDialog dialog(parent);
    dialog.setWindowTitle(QStringLiteral("Изменение ставки"));
    auto* layout = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout;

    auto* name_label = new QLabel(employee_name, &dialog);
    form->addRow(QStringLiteral("Сотрудник:"), name_label);

    auto* date = create_date_edit(&dialog);
    form->addRow(QStringLiteral("Дата изменения:"), date);

    auto* rate = new QLineEdit(&dialog);
    rate->setText(format_money(current_rate));
    form->addRow(QStringLiteral("Новая ставка:"), rate);

    auto* rate_unit = new QComboBox(&dialog);
    rate_unit->addItem(
        QStringLiteral("В месяц"), static_cast<int>(RateUnit::Month));
    rate_unit->addItem(
        QStringLiteral("В день"), static_cast<int>(RateUnit::Day));
    rate_unit->addItem(
        QStringLiteral("В час"), static_cast<int>(RateUnit::Hour));
    const int unit_index = rate_unit->findData(
        static_cast<int>(current_unit));
    if (unit_index >= 0) {
        rate_unit->setCurrentIndex(unit_index);
    }
    form->addRow(QStringLiteral("Единица ставки:"), rate_unit);
    layout->addLayout(form);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]() {
        const MoneyParseResult parsed = parse_money(rate->text());
        if (!parsed.value.has_value()) {
            QMessageBox::warning(
                &dialog, QStringLiteral("Ставка"), parsed.error);
            return;
        }
        try {
            change_employee_rate(
                database,
                org_id,
                employee_id,
                date_from_qdate(date->date()),
                *parsed.value,
                static_cast<RateUnit>(
                    rate_unit->currentData().toInt()));
            dialog.accept();
        } catch (const std::exception& error) {
            QMessageBox::critical(
                &dialog,
                QStringLiteral("Ошибка изменения ставки"),
                QString::fromUtf8(error.what()));
        }
    });
    QObject::connect(
        buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    return dialog.exec() == QDialog::Accepted;
}

bool terminate_employee_dialog(
    QWidget* parent,
    Database& database,
    int64_t org_id,
    int64_t employee_id,
    const QString& employee_name) {
    QDialog dialog(parent);
    dialog.setWindowTitle(QStringLiteral("Увольнение сотрудника"));
    auto* layout = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout;

    auto* name_label = new QLabel(employee_name, &dialog);
    form->addRow(QStringLiteral("Сотрудник:"), name_label);

    auto* date = create_date_edit(&dialog);
    form->addRow(QStringLiteral("Дата увольнения:"), date);

    auto* reason = new QComboBox(&dialog);
    reason->addItem(QStringLiteral("Без причины"), QVariant());
    try {
        for (const TerminationReason& value :
             list_termination_reasons(database)) {
            reason->addItem(
                QString::fromStdString(value.name),
                QVariant::fromValue(
                    static_cast<qlonglong>(value.id)));
        }
    } catch (const std::exception& error) {
        QMessageBox::critical(
            &dialog,
            QStringLiteral("Ошибка загрузки причин"),
            QString::fromUtf8(error.what()));
        return false;
    }
    form->addRow(QStringLiteral("Причина:"), reason);
    layout->addLayout(form);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]() {
        try {
            std::optional<int64_t> reason_id;
            if (reason->currentIndex() > 0) {
                reason_id = reason->currentData().toLongLong();
            }
            terminate_employee(
                database,
                org_id,
                employee_id,
                date_from_qdate(date->date()),
                reason_id);
            dialog.accept();
        } catch (const std::exception& error) {
            QMessageBox::critical(
                &dialog,
                QStringLiteral("Ошибка увольнения"),
                QString::fromUtf8(error.what()));
        }
    });
    QObject::connect(
        buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    return dialog.exec() == QDialog::Accepted;
}

bool edit_position_dialog(QWidget* parent, Position& position) {
    QDialog dialog(parent);
    dialog.setWindowTitle(
        position.id == 0
            ? QStringLiteral("Добавить должность")
            : QStringLiteral("Изменить должность"));

    auto* layout = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout;
    auto* name = new QLineEdit(&dialog);
    name->setText(QString::fromStdString(position.name));
    form->addRow(QStringLiteral("Название:"), name);
    layout->addLayout(form);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]() {
        if (name->text().trimmed().isEmpty()) {
            QMessageBox::warning(
                &dialog,
                QStringLiteral("Должность"),
                QStringLiteral("Введите название должности."));
            return;
        }
        dialog.accept();
    });
    QObject::connect(
        buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    position.name = name->text().trimmed().toStdString();
    return true;
}

bool edit_work_schedule_dialog(
    QWidget* parent,
    WorkSchedule& schedule) {
    QDialog dialog(parent);
    dialog.setWindowTitle(
        schedule.id == 0
            ? QStringLiteral("Добавить график")
            : QStringLiteral("Изменить график"));

    auto* layout = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout;
    auto* name = new QLineEdit(&dialog);
    name->setText(QString::fromStdString(schedule.name));
    form->addRow(QStringLiteral("Название:"), name);

    auto* kind = new QComboBox(&dialog);
    kind->addItem(
        QStringLiteral("Пятидневка"),
        static_cast<int>(ScheduleKind::FiveDay));
    kind->addItem(
        QStringLiteral("Сменный"),
        static_cast<int>(ScheduleKind::Shift));
    const int kind_index = kind->findData(static_cast<int>(schedule.kind));
    if (kind_index >= 0) {
        kind->setCurrentIndex(kind_index);
    }
    form->addRow(QStringLiteral("Тип:"), kind);

    auto* minutes = new QSpinBox(&dialog);
    minutes->setRange(1, 1440);
    minutes->setValue(
        schedule.normative_minutes_per_day > 0
            ? schedule.normative_minutes_per_day
            : 480);
    form->addRow(QStringLiteral("Норма, минут в день:"), minutes);

    auto* cycle = new QLineEdit(&dialog);
    cycle->setText(QString::fromStdString(schedule.shift_cycle));
    cycle->setPlaceholderText(QStringLiteral("Например, WWRR"));
    form->addRow(QStringLiteral("Цикл W/R:"), cycle);

    auto* anchor = new QDateEdit(&dialog);
    anchor->setCalendarPopup(true);
    anchor->setDisplayFormat(QStringLiteral("dd.MM.yyyy"));
    anchor->setMinimumDate(QDate(1, 1, 1));
    anchor->setMaximumDate(QDate(9999, 12, 31));
    if (schedule.anchor_date.has_value()) {
        anchor->setDate(QDate(
            schedule.anchor_date->year(),
            schedule.anchor_date->month(),
            schedule.anchor_date->day()));
    } else {
        anchor->setDate(QDate::currentDate());
    }
    form->addRow(QStringLiteral("Опорная дата:"), anchor);
    layout->addLayout(form);

    const auto update_shift_fields = [=]() {
        const bool shift =
            kind->currentData().toInt() ==
            static_cast<int>(ScheduleKind::Shift);
        cycle->setEnabled(shift);
        anchor->setEnabled(shift);
    };
    QObject::connect(
        kind,
        QOverload<int>::of(&QComboBox::currentIndexChanged),
        &dialog,
        [=](int) { update_shift_fields(); });
    update_shift_fields();

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]() {
        if (name->text().trimmed().isEmpty()) {
            QMessageBox::warning(
                &dialog,
                QStringLiteral("График"),
                QStringLiteral("Введите название графика."));
            return;
        }

        if (kind->currentData().toInt() ==
            static_cast<int>(ScheduleKind::Shift)) {
            const QString value = cycle->text().trimmed();
            if (value.isEmpty() ||
                value.contains(QChar::fromLatin1(' '))) {
                QMessageBox::warning(
                    &dialog,
                    QStringLiteral("График"),
                    QStringLiteral("Введите непустой цикл из W и R."));
                return;
            }
            for (const QChar character : value) {
                if (character != QLatin1Char('W') &&
                    character != QLatin1Char('R')) {
                    QMessageBox::warning(
                        &dialog,
                        QStringLiteral("График"),
                        QStringLiteral("Цикл может содержать только W и R."));
                    return;
                }
            }
        }
        dialog.accept();
    });
    QObject::connect(
        buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    schedule.name = name->text().trimmed().toStdString();
    schedule.kind = static_cast<ScheduleKind>(kind->currentData().toInt());
    schedule.normative_minutes_per_day = minutes->value();
    if (schedule.kind == ScheduleKind::Shift) {
        schedule.shift_cycle = cycle->text().trimmed().toStdString();
        const QDate date = anchor->date();
        schedule.anchor_date = Date(
            date.year(), date.month(), date.day());
    } else {
        schedule.shift_cycle.clear();
        schedule.anchor_date.reset();
    }
    return true;
}

void setup_table(QTableWidget* table, const QStringList& headers) {
    table->setColumnCount(headers.size());
    table->setHorizontalHeaderLabels(headers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->horizontalHeader()->setDefaultAlignment(
        Qt::AlignLeft | Qt::AlignVCenter);
    table->horizontalHeader()->setStretchLastSection(false);
}

void setup_columns(
    QTableWidget* table,
    int stretch_column,
    bool sortable = false) {
    QHeaderView* header = table->horizontalHeader();
    header->setStretchLastSection(false);

    int minimum_width = 0;
    for (int column = 0; column < table->columnCount(); ++column) {
        if (column == stretch_column) {
            continue;
        }

        const QTableWidgetItem* item = table->horizontalHeaderItem(column);
        const int text_width = item == nullptr
            ? 0
            : header->fontMetrics().horizontalAdvance(item->text());
        const int sort_width = sortable
            ? header->style()->pixelMetric(
                QStyle::PM_HeaderMarkSize, nullptr, header)
            : 0;
        const int required_width = text_width + sort_width + 32;
        if (required_width > minimum_width) {
            minimum_width = required_width;
        }
    }
    header->setMinimumSectionSize(minimum_width);

    for (int column = 0; column < table->columnCount(); ++column) {
        header->setSectionResizeMode(
            column,
            column == stretch_column
                ? QHeaderView::Stretch
                : QHeaderView::ResizeToContents);
    }
}

std::optional<int64_t> selected_id(QTableWidget* table) {
    const int row = table->currentRow();
    if (row < 0) {
        return std::nullopt;
    }
    const QTableWidgetItem* item = table->item(row, 0);
    if (item == nullptr) {
        return std::nullopt;
    }
    return item->data(Qt::UserRole).toLongLong();
}

void employee_card_dialog(
    QWidget* parent,
    Database& database,
    int64_t org_id,
    int64_t employee_id,
    const std::function<void()>& refresh_list) {
    const auto initial = read_employee(database, employee_id);
    if (!initial.has_value() || initial->org_id != org_id) {
        QMessageBox::information(
            parent,
            QStringLiteral("Карточка сотрудника"),
            QStringLiteral("Сотрудник не найден в выбранной организации."));
        return;
    }

    QDialog dialog(parent);
    dialog.setWindowTitle(QStringLiteral("Карточка сотрудника"));
    dialog.resize(620, 620);
    auto* layout = new QVBoxLayout(&dialog);

    auto* identity_group =
        new QGroupBox(QStringLiteral("Основные данные"), &dialog);
    auto* identity_form = new QFormLayout(identity_group);
    auto* number = new QLineEdit(identity_group);
    auto* full_name = new QLineEdit(identity_group);
    identity_form->addRow(QStringLiteral("Табельный номер:"), number);
    identity_form->addRow(QStringLiteral("ФИО:"), full_name);
    layout->addWidget(identity_group);

    auto* current_group =
        new QGroupBox(QStringLiteral("Текущее состояние"), &dialog);
    auto* current_form = new QFormLayout(current_group);
    auto* status_label = new QLabel(current_group);
    auto* org_label = new QLabel(current_group);
    auto* department_label = new QLabel(current_group);
    auto* position_label = new QLabel(current_group);
    auto* kind_label = new QLabel(current_group);
    auto* schedule_label = new QLabel(current_group);
    auto* preset_label = new QLabel(current_group);
    auto* rate_label = new QLabel(current_group);
    auto* hire_label = new QLabel(current_group);
    auto* termination_label = new QLabel(current_group);
    current_form->addRow(QStringLiteral("Статус:"), status_label);
    current_form->addRow(QStringLiteral("Организация:"), org_label);
    current_form->addRow(QStringLiteral("Подразделение:"), department_label);
    current_form->addRow(QStringLiteral("Должность:"), position_label);
    current_form->addRow(QStringLiteral("Вид занятости:"), kind_label);
    current_form->addRow(QStringLiteral("График работы:"), schedule_label);
    current_form->addRow(QStringLiteral("Набор начислений:"), preset_label);
    current_form->addRow(QStringLiteral("Ставка:"), rate_label);
    current_form->addRow(QStringLiteral("Дата приёма:"), hire_label);
    current_form->addRow(QStringLiteral("Дата увольнения:"), termination_label);
    layout->addWidget(current_group);

    auto* history_group =
        new QGroupBox(QStringLiteral("История кадровых событий"), &dialog);
    auto* history_layout = new QVBoxLayout(history_group);
    auto* history_table = new QTableWidget(history_group);
    setup_table(
        history_table,
        {QStringLiteral("Дата"),
         QStringLiteral("Событие"),
         QStringLiteral("Детали")});
    setup_columns(history_table, 2);
    history_table->setMinimumHeight(160);
    history_layout->addWidget(history_table);
    layout->addWidget(history_group);

    auto* operations_layout = new QHBoxLayout;
    auto* transfer_button =
        new QPushButton(QStringLiteral("Перевести"), &dialog);
    auto* rate_button =
        new QPushButton(QStringLiteral("Изменить ставку"), &dialog);
    auto* terminate_button =
        new QPushButton(QStringLiteral("Уволить"), &dialog);
    operations_layout->addWidget(transfer_button);
    operations_layout->addWidget(rate_button);
    operations_layout->addWidget(terminate_button);
    operations_layout->addStretch();
    layout->addLayout(operations_layout);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Close, &dialog);
    buttons->button(QDialogButtonBox::Save)
        ->setText(QStringLiteral("Сохранить"));
    buttons->button(QDialogButtonBox::Close)
        ->setText(QStringLiteral("Закрыть"));
    layout->addWidget(buttons);

    std::vector<Department> departments;
    std::vector<Position> positions;
    std::vector<EmploymentKind> employment_kinds;
    std::vector<WorkSchedule> work_schedules;
    std::vector<PayrollPreset> payroll_presets;
    std::vector<TerminationReason> termination_reasons;
    std::vector<Org> organizations;

    Employee current = *initial;

    const auto fill_state = [&]() {
        status_label->setText(employee_status_text(current));
        const auto org_it = std::find_if(
            organizations.begin(),
            organizations.end(),
            [&](const Org& value) { return value.id == current.org_id; });
        org_label->setText(
            org_it != organizations.end()
                ? QString::fromStdString(org_it->name)
                : QStringLiteral("№%1").arg(current.org_id));
        department_label->setText(
            department_name_or_placeholder(
                departments, current.department_id));
        position_label->setText(
            position_name_or_placeholder(
                positions, current.position_id));
        kind_label->setText(
            employment_kind_name_or_placeholder(
                employment_kinds, current.employment_kind_id));
        schedule_label->setText(
            work_schedule_name_or_placeholder(
                work_schedules, current.work_schedule_id));
        preset_label->setText(
            preset_name_or_placeholder(
                payroll_presets, current.payroll_preset_id));
        rate_label->setText(
            QStringLiteral("%1 (%2)")
                .arg(format_money(current.rate),
                     format_rate_unit(current.rate_unit)));
        hire_label->setText(format_date(current.hire_date));
        termination_label->setText(
            current.termination_date.has_value()
                ? format_date(*current.termination_date)
                : QStringLiteral("—"));

        const bool terminated = current.termination_date.has_value();
        number->setEnabled(!terminated);
        full_name->setEnabled(!terminated);
        buttons->button(QDialogButtonBox::Save)->setEnabled(!terminated);
        transfer_button->setEnabled(!terminated);
        rate_button->setEnabled(!terminated);
        terminate_button->setEnabled(!terminated);
    };

    const auto fill_history = [&]() {
        history_table->setRowCount(0);
        try {
            for (const EmploymentEvent& event :
                 list_employment_events(database, employee_id)) {
                const int row = history_table->rowCount();
                history_table->insertRow(row);
                history_table->setItem(
                    row, 0,
                    new QTableWidgetItem(format_date(event.date)));
                history_table->setItem(
                    row, 1,
                    new QTableWidgetItem(format_event_kind(event.kind)));

                QStringList details;
                if (event.kind == EventKind::Transfer) {
                    details
                        << department_name_or_placeholder(
                            departments,
                            event.department_id.value_or(0))
                        << position_name_or_placeholder(
                            positions,
                            event.position_id.value_or(0));
                } else if (event.kind == EventKind::ChangeSalary &&
                           event.rate.has_value()) {
                    details << QStringLiteral("%1 (%2)")
                        .arg(format_money(*event.rate),
                             event.rate_unit.has_value()
                                 ? format_rate_unit(*event.rate_unit)
                                 : QStringLiteral("?"));
                } else if (event.kind == EventKind::Termination &&
                           event.termination_reason_id.has_value()) {
                    const auto reason_it = std::find_if(
                        termination_reasons.begin(),
                        termination_reasons.end(),
                        [&](const TerminationReason& value) {
                            return value.id ==
                                *event.termination_reason_id;
                        });
                    details << (
                        reason_it != termination_reasons.end()
                            ? QString::fromStdString(reason_it->name)
                            : QStringLiteral("№%1")
                                .arg(*event.termination_reason_id));
                }
                history_table->setItem(
                    row, 2,
                    new QTableWidgetItem(details.join(QStringLiteral(", "))));
            }
        } catch (const std::exception& error) {
            QMessageBox::warning(
                &dialog,
                QStringLiteral("Ошибка загрузки истории"),
                QString::fromUtf8(error.what()));
        }
    };

    const auto reload_employee = [&]() -> bool {
        try {
            const auto saved = read_employee(database, employee_id);
            if (!saved.has_value()) {
                return false;
            }
            current = *saved;
            number->setText(QString::fromStdString(current.personnel_number));
            full_name->setText(QString::fromStdString(current.full_name));
            fill_state();
            fill_history();
            return true;
        } catch (const std::exception& error) {
            QMessageBox::critical(
                &dialog,
                QStringLiteral("Ошибка загрузки сотрудника"),
                QString::fromUtf8(error.what()));
            return false;
        }
    };

    const auto on_transfer = [&]() {
        if (transfer_employee_dialog(
                &dialog,
                database,
                org_id,
                employee_id,
                QString::fromStdString(current.full_name)) &&
            reload_employee()) {
            refresh_list();
        }
    };
    const auto on_rate = [&]() {
        if (change_rate_dialog(
                &dialog,
                database,
                org_id,
                employee_id,
                QString::fromStdString(current.full_name),
                current.rate,
                current.rate_unit) &&
            reload_employee()) {
            refresh_list();
        }
    };
    const auto on_terminate = [&]() {
        if (terminate_employee_dialog(
                &dialog,
                database,
                org_id,
                employee_id,
                QString::fromStdString(current.full_name)) &&
            reload_employee()) {
            refresh_list();
        }
    };

    QObject::connect(
        transfer_button, &QPushButton::clicked, &dialog, on_transfer);
    QObject::connect(
        rate_button, &QPushButton::clicked, &dialog, on_rate);
    QObject::connect(
        terminate_button, &QPushButton::clicked, &dialog, on_terminate);
    QObject::connect(
        buttons->button(QDialogButtonBox::Save),
        &QPushButton::clicked,
        &dialog,
        [&]() {
            const QString trimmed_number = number->text().trimmed();
            const QString trimmed_name = full_name->text().trimmed();
            try {
                update_employee_identity(
                    database,
                    org_id,
                    employee_id,
                    trimmed_number.toStdString(),
                    trimmed_name.toStdString());
                if (reload_employee()) {
                    refresh_list();
                }
            } catch (const std::exception& error) {
                QMessageBox::critical(
                    &dialog,
                    QStringLiteral("Ошибка сохранения"),
                    QString::fromUtf8(error.what()));
            }
        });
    QObject::connect(
        buttons->button(QDialogButtonBox::Close),
        &QPushButton::clicked,
        &dialog,
        &QDialog::reject);

    try {
        departments = list_departments(database, org_id);
        positions = list_positions(database);
        employment_kinds = list_employment_kinds(database);
        work_schedules = list_work_schedules(database);
        payroll_presets = list_payroll_presets(database);
        termination_reasons = list_termination_reasons(database);
        organizations = list_orgs(database);
    } catch (const std::exception& error) {
        QMessageBox::critical(
            &dialog,
            QStringLiteral("Ошибка загрузки справочников"),
            QString::fromUtf8(error.what()));
        return;
    }

    number->setText(QString::fromStdString(current.personnel_number));
    full_name->setText(QString::fromStdString(current.full_name));
    fill_state();
    fill_history();

    dialog.exec();
}

void show_reference_error(
    QWidget* parent,
    const QString& operation,
    const std::exception& error) {
    QMessageBox::critical(
        parent,
        operation,
        QStringLiteral(
            "Операция не выполнена. Если запись используется "
            "в кадровых данных, удалять её нельзя.\n\n%1")
            .arg(QString::fromUtf8(error.what())));
}

QVBoxLayout* make_page_layout(QWidget* page) {
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(28, 24, 32, 24);
    layout->setSpacing(14);
    return layout;
}

QVBoxLayout* make_subpage_layout(QWidget* page) {
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(14);
    return layout;
}

void add_page_header(
    QVBoxLayout* layout,
    const QString& title,
    const QString& subtitle) {
    auto* title_label = new QLabel(title);
    title_label->setObjectName(QStringLiteral("pageTitle"));
    layout->addWidget(title_label);

    if (!subtitle.isEmpty()) {
        auto* subtitle_label = new QLabel(subtitle);
        subtitle_label->setObjectName(QStringLiteral("pageSubtitle"));
        subtitle_label->setWordWrap(true);
        layout->addWidget(subtitle_label);
    }
}

class TimesheetHoursDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QWidget* createEditor(
        QWidget* parent,
        const QStyleOptionViewItem&,
        const QModelIndex&) const override {
        auto* editor = new QLineEdit(parent);
        editor->setPlaceholderText(QStringLiteral("8:00"));
        editor->setAlignment(Qt::AlignRight);
        return editor;
    }

    void setEditorData(
        QWidget* editor,
        const QModelIndex& index) const override {
        auto* line = qobject_cast<QLineEdit*>(editor);
        if (line == nullptr) {
            QStyledItemDelegate::setEditorData(editor, index);
            return;
        }
        line->setText(index.data(Qt::EditRole).toString());
        line->selectAll();
    }

    void setModelData(
        QWidget* editor,
        QAbstractItemModel* model,
        const QModelIndex& index) const override {
        auto* line = qobject_cast<QLineEdit*>(editor);
        if (line == nullptr) {
            QStyledItemDelegate::setModelData(
                editor, model, index);
            return;
        }

        int32_t minutes = 0;
        QString error;
        if (!TimesheetTableModel::parse_hours(
                line->text(), minutes, error)) {
            QMessageBox::warning(
                qobject_cast<QWidget*>(parent()),
                QStringLiteral("Некорректное количество часов"),
                error);
            return;
        }
        if (!model->setData(
                index,
                static_cast<int>(minutes),
                Qt::EditRole)) {
            QMessageBox::warning(
                qobject_cast<QWidget*>(parent()),
                QStringLiteral("Некорректное количество часов"),
                QStringLiteral(
                    "Часы можно вводить только для рабочего дня "
                    "с одной записью учёта времени."));
        }
    }
};

QString section_title(SectionId section) {
    switch (section) {
    case SectionId::Dashboard:
        return QStringLiteral("Рабочий стол");
    case SectionId::Organization:
        return QStringLiteral("Организация");
    case SectionId::References:
        return QStringLiteral("Справочники");
    case SectionId::Employees:
        return QStringLiteral("Сотрудники");
    case SectionId::Timesheet:
        return QStringLiteral("Табель");
    case SectionId::Calculation:
        return QStringLiteral("Расчёт");
    }
    return QString();
}

}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("Малые кадры"));
    resize(1280, 800);

    dashboard_page_ = create_dashboard_page();
    organization_page_ = create_organization_page();
    references_page_ = create_references_page();
    employees_page_ = create_employees_page();
    timesheet_page_ = create_timesheet_page();
    calculation_page_ = create_calculation_page();

    auto* central = new QWidget(this);
    auto* root_layout = new QHBoxLayout(central);
    root_layout->setContentsMargins(0, 0, 0, 0);
    root_layout->setSpacing(0);

    sidebar_ = new NavigationSidebar(central);
    workspace_tabs_ = new WorkspaceTabs(central);
    root_layout->addWidget(sidebar_);
    root_layout->addWidget(workspace_tabs_, 1);
    setCentralWidget(central);

    workspace_tabs_->add_page(
        static_cast<int>(SectionId::Dashboard), dashboard_page_);
    workspace_tabs_->add_page(
        static_cast<int>(SectionId::Organization), organization_page_);
    workspace_tabs_->add_page(
        static_cast<int>(SectionId::References), references_page_);
    workspace_tabs_->add_page(
        static_cast<int>(SectionId::Employees), employees_page_);
    workspace_tabs_->add_page(
        static_cast<int>(SectionId::Timesheet), timesheet_page_);
    workspace_tabs_->add_page(
        static_cast<int>(SectionId::Calculation), calculation_page_);

    workspace_tabs_->set_current_changed_handler([this](int section_id) {
        if (sidebar_ != nullptr) {
            sidebar_->set_active_section(section_id);
        }
        update_navigation_state();
        if (section_id == static_cast<int>(SectionId::Timesheet) &&
            database_open_ &&
            selected_org_id_.has_value() &&
            timesheet_model_ != nullptr &&
            !timesheet_model_->has_unsaved_changes()) {
            refresh_timesheet();
        }
        if (section_id == static_cast<int>(SectionId::Calculation) &&
            database_open_ &&
            selected_org_id_.has_value()) {
            refresh_calculation_parameters();
        }
    });
    workspace_tabs_->set_can_close_handler([this](int section_id) {
        if (section_id == static_cast<int>(SectionId::Organization)) {
            return confirm_unsaved_changes();
        }
        return true;
    });
    sidebar_->set_on_section_selected([this](int section_id) {
        open_section(static_cast<SectionId>(section_id));
    });

    open_section(SectionId::Dashboard);
    clear_database_view();

    connect(create_organization_button_, &QPushButton::clicked,
            this, [this]() { create_organization(); });
    connect(save_organization_button_, &QPushButton::clicked,
            this, [this]() { save_organization(); });
    connect(
        organization_combo_,
        QOverload<int>::of(&QComboBox::currentIndexChanged),
        this,
        [this](int index) { select_organization(index); });

    connect(add_position_button_, &QPushButton::clicked,
            this, [this]() { add_position(); });
    connect(edit_position_button_, &QPushButton::clicked,
            this, [this]() { edit_position(); });
    connect(remove_position_button_, &QPushButton::clicked,
            this, [this]() { remove_position(); });
    connect(refresh_positions_button_, &QPushButton::clicked,
            this, [this]() { refresh_positions(); });
    connect(add_work_schedule_button_, &QPushButton::clicked,
            this, [this]() { add_work_schedule(); });
    connect(edit_work_schedule_button_, &QPushButton::clicked,
            this, [this]() { edit_work_schedule(); });
    connect(remove_work_schedule_button_, &QPushButton::clicked,
            this, [this]() { remove_work_schedule(); });
    connect(refresh_work_schedules_button_, &QPushButton::clicked,
            this, [this]() { refresh_work_schedules(); });
    connect(add_employee_button_, &QPushButton::clicked,
            this, [this]() { add_employee(); });
    connect(employee_card_button_, &QPushButton::clicked,
            this, [this]() { open_employee_card(); });
    connect(refresh_employees_button_, &QPushButton::clicked,
            this, [this]() { refresh_employees(); });
    connect(employees_table_, &QTableView::doubleClicked,
            this, [this](const QModelIndex&) { open_employee_card(); });
}

QWidget* MainWindow::create_dashboard_page() {
    auto* page = new QWidget;
    page->setObjectName(QStringLiteral("dashboard_page"));
    page->setAutoFillBackground(true);
    auto* layout = make_page_layout(page);

    auto* header = new QHBoxLayout;
    header->setSpacing(12);
    auto* title_column = new QVBoxLayout;
    title_column->setSpacing(4);
    add_page_header(
        title_column,
        QStringLiteral("Рабочий стол"),
        QStringLiteral("Обзор текущего состояния кадрового учёта"));
    header->addLayout(title_column);
    header->addStretch();

    auto* create_button =
        new QPushButton(QStringLiteral("Создать файл БД"), page);
    auto* open_button =
        new QPushButton(QStringLiteral("Открыть базу данных"), page);
    open_button->setObjectName(QStringLiteral("primaryButton"));
    header->addWidget(create_button);
    header->addWidget(open_button);
    layout->addLayout(header);

    auto* card = new QFrame(page);
    card->setObjectName(QStringLiteral("infoCard"));
    auto* form = new QFormLayout(card);
    form->setContentsMargins(20, 18, 20, 18);
    form->setSpacing(10);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    dashboard_database_label_ = new QLabel(card);
    dashboard_database_label_->setObjectName(QStringLiteral("infoValue"));
    dashboard_database_label_->setWordWrap(true);
    dashboard_database_label_->setSizePolicy(
        QSizePolicy::Ignored, QSizePolicy::Preferred);

    dashboard_organization_label_ = new QLabel(card);
    dashboard_organization_label_->setObjectName(QStringLiteral("infoValue"));
    dashboard_organization_label_->setWordWrap(true);

    form->addRow(
        QStringLiteral("Файл базы данных:"), dashboard_database_label_);
    form->addRow(
        QStringLiteral("Организация:"), dashboard_organization_label_);
    layout->addWidget(card);
    layout->addStretch();

    connect(create_button, &QPushButton::clicked,
            this, [this]() { create_database(); });
    connect(open_button, &QPushButton::clicked,
            this, [this]() { open_database(); });
    return page;
}

QWidget* MainWindow::create_organization_page() {
    auto* page = new QWidget;
    page->setObjectName(QStringLiteral("organization_tab"));
    page->setAutoFillBackground(true);
    auto* layout = make_page_layout(page);

    auto* sections = new SectionPage(page);
    sections->add_page(
        QStringLiteral("Реквизиты"),
        create_organization_details_page());
    sections->add_page(
        QStringLiteral("База данных"),
        create_database_settings_page());
    layout->addWidget(sections, 1);
    return page;
}

QWidget* MainWindow::create_organization_details_page() {
    auto* page = new QWidget;
    auto* layout = make_subpage_layout(page);
    add_page_header(
        layout,
        QStringLiteral("Организация"),
        QStringLiteral("Выбор и редактирование реквизитов организации"));

    organization_status_label_ = new QLabel(page);
    organization_status_label_->setObjectName(QStringLiteral("pageStatus"));
    organization_status_label_->setWordWrap(true);
    layout->addWidget(organization_status_label_);

    auto* card = new QFrame(page);
    card->setObjectName(QStringLiteral("infoCard"));
    auto* card_layout = new QVBoxLayout(card);
    card_layout->setContentsMargins(20, 18, 20, 18);
    card_layout->setSpacing(12);

    auto* form = new QFormLayout;
    organization_combo_ = new QComboBox(card);
    organization_name_edit_ = new QLineEdit(card);
    organization_name_edit_->setObjectName(
        QStringLiteral("organization_name_edit"));
    organization_inn_edit_ = new QLineEdit(card);
    organization_kpp_edit_ = new QLineEdit(card);
    organization_combo_->setMinimumWidth(0);
    organization_name_edit_->setMinimumWidth(0);
    organization_inn_edit_->setMinimumWidth(0);
    organization_kpp_edit_->setMinimumWidth(0);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->addRow(QStringLiteral("Организация:"), organization_combo_);
    form->addRow(QStringLiteral("Название:"), organization_name_edit_);
    form->addRow(QStringLiteral("ИНН:"), organization_inn_edit_);
    form->addRow(QStringLiteral("КПП:"), organization_kpp_edit_);
    card_layout->addLayout(form);

    auto* organization_buttons = new QHBoxLayout;
    create_organization_button_ =
        new QPushButton(QStringLiteral("Создать организацию"), card);
    save_organization_button_ =
        new QPushButton(QStringLiteral("Сохранить организацию"), card);
    save_organization_button_->setObjectName(
        QStringLiteral("primaryButton"));
    organization_buttons->addWidget(create_organization_button_);
    organization_buttons->addWidget(save_organization_button_);
    organization_buttons->addStretch();
    card_layout->addLayout(organization_buttons);

    layout->addWidget(card);
    layout->addStretch();
    return page;
}

QWidget* MainWindow::create_database_settings_page() {
    auto* page = new QWidget;
    auto* layout = make_subpage_layout(page);
    add_page_header(
        layout,
        QStringLiteral("База данных"),
        QStringLiteral("Создание и подключение файла SQLite"));

    auto* card = new QFrame(page);
    card->setObjectName(QStringLiteral("infoCard"));
    auto* card_layout = new QVBoxLayout(card);
    card_layout->setContentsMargins(20, 18, 20, 18);
    card_layout->setSpacing(12);

    auto* database_buttons = new QHBoxLayout;
    auto* create_database_button =
        new QPushButton(QStringLiteral("Создать файл БД"), card);
    auto* open_database_button =
        new QPushButton(QStringLiteral("Открыть файл БД"), card);
    database_buttons->addWidget(create_database_button);
    database_buttons->addWidget(open_database_button);
    database_buttons->addStretch();
    card_layout->addLayout(database_buttons);

    database_path_label_ =
        new QLabel(QStringLiteral("Файл БД не открыт"), card);
    database_path_label_->setObjectName(QStringLiteral("database_path_label"));
    database_path_label_->setWordWrap(true);
    database_path_label_->setSizePolicy(
        QSizePolicy::Ignored, QSizePolicy::Preferred);
    card_layout->addWidget(database_path_label_);

    layout->addWidget(card);
    layout->addStretch();

    connect(create_database_button, &QPushButton::clicked,
            this, [this]() { create_database(); });
    connect(open_database_button, &QPushButton::clicked,
            this, [this]() { open_database(); });
    return page;
}

QWidget* MainWindow::create_references_page() {
    auto* page = new QWidget;
    page->setObjectName(QStringLiteral("references_tab"));
    page->setAutoFillBackground(true);
    auto* layout = make_page_layout(page);

    references_status_label_ = new QLabel(page);
    references_status_label_->setObjectName(QStringLiteral("pageStatus"));
    references_status_label_->setWordWrap(true);
    layout->addWidget(references_status_label_);

    auto* sections = new SectionPage(page);

    auto* positions_page = new QWidget;
    auto* positions_layout = make_subpage_layout(positions_page);
    add_page_header(
        positions_layout,
        QStringLiteral("Должности"),
        QStringLiteral("Справочник должностей организации"));

    auto* positions_card = new QFrame(positions_page);
    positions_card->setObjectName(QStringLiteral("infoCard"));
    auto* positions_card_layout = new QVBoxLayout(positions_card);
    positions_card_layout->setContentsMargins(16, 16, 16, 16);
    positions_card_layout->setSpacing(10);
    positions_table_ = new QTableWidget(positions_card);
    setup_table(
        positions_table_,
        {QStringLiteral("ID"), QStringLiteral("Название")});
    setup_columns(positions_table_, 1);
    positions_card_layout->addWidget(positions_table_);
    auto* positions_buttons = new QHBoxLayout;
    add_position_button_ =
        new QPushButton(QStringLiteral("Добавить"), positions_card);
    edit_position_button_ =
        new QPushButton(QStringLiteral("Изменить"), positions_card);
    remove_position_button_ =
        new QPushButton(QStringLiteral("Удалить"), positions_card);
    refresh_positions_button_ =
        new QPushButton(QStringLiteral("Обновить"), positions_card);
    positions_buttons->addWidget(add_position_button_);
    positions_buttons->addWidget(edit_position_button_);
    positions_buttons->addWidget(remove_position_button_);
    positions_buttons->addWidget(refresh_positions_button_);
    positions_buttons->addStretch();
    positions_card_layout->addLayout(positions_buttons);
    positions_layout->addWidget(positions_card, 1);

    auto* schedules_page = new QWidget;
    auto* schedules_layout = make_subpage_layout(schedules_page);
    add_page_header(
        schedules_layout,
        QStringLiteral("Графики работы"),
        QStringLiteral("Графики и нормы рабочего времени"));

    auto* schedules_card = new QFrame(schedules_page);
    schedules_card->setObjectName(QStringLiteral("infoCard"));
    auto* schedules_card_layout = new QVBoxLayout(schedules_card);
    schedules_card_layout->setContentsMargins(16, 16, 16, 16);
    schedules_card_layout->setSpacing(10);
    work_schedules_table_ = new QTableWidget(schedules_card);
    setup_table(
        work_schedules_table_,
        {QStringLiteral("ID"),
         QStringLiteral("Название"),
         QStringLiteral("Тип"),
         QStringLiteral("Минут в день"),
         QStringLiteral("Цикл"),
         QStringLiteral("Опорная дата")});
    setup_columns(work_schedules_table_, 1);
    schedules_card_layout->addWidget(work_schedules_table_);
    auto* schedules_buttons = new QHBoxLayout;
    add_work_schedule_button_ =
        new QPushButton(QStringLiteral("Добавить"), schedules_card);
    edit_work_schedule_button_ =
        new QPushButton(QStringLiteral("Изменить"), schedules_card);
    remove_work_schedule_button_ =
        new QPushButton(QStringLiteral("Удалить"), schedules_card);
    refresh_work_schedules_button_ =
        new QPushButton(QStringLiteral("Обновить"), schedules_card);
    schedules_buttons->addWidget(add_work_schedule_button_);
    schedules_buttons->addWidget(edit_work_schedule_button_);
    schedules_buttons->addWidget(remove_work_schedule_button_);
    schedules_buttons->addWidget(refresh_work_schedules_button_);
    schedules_buttons->addStretch();
    schedules_card_layout->addLayout(schedules_buttons);
    schedules_layout->addWidget(schedules_card, 1);

    sections->add_page(
        QStringLiteral("Должности"), positions_page);
    sections->add_page(
        QStringLiteral("Графики работы"), schedules_page);
    layout->addWidget(sections, 1);
    return page;
}

QWidget* MainWindow::create_employees_page() {
    auto* page = new QWidget;
    page->setObjectName(QStringLiteral("employees_tab"));
    page->setAutoFillBackground(true);
    auto* layout = make_page_layout(page);

    employees_status_label_ = new QLabel(page);
    employees_status_label_->setObjectName(QStringLiteral("pageStatus"));
    employees_status_label_->setWordWrap(true);
    layout->addWidget(employees_status_label_);

    auto* sections = new SectionPage(page);

    auto* list_page = new QWidget;
    auto* list_layout = make_subpage_layout(list_page);
    add_page_header(
        list_layout,
        QStringLiteral("Сотрудники"),
        QStringLiteral("Список сотрудников выбранной организации"));

    auto* employees_card = new QFrame(list_page);
    employees_card->setObjectName(QStringLiteral("infoCard"));
    auto* employees_card_layout = new QVBoxLayout(employees_card);
    employees_card_layout->setContentsMargins(16, 16, 16, 16);
    employees_card_layout->setSpacing(10);
    employees_model_ = new EmployeeTableModel(this);
    employees_proxy_ = new QSortFilterProxyModel(this);
    employees_proxy_->setSourceModel(employees_model_);
    employees_proxy_->setSortCaseSensitivity(Qt::CaseInsensitive);

    employees_table_ = new QTableView(employees_card);
    employees_table_->setObjectName(QStringLiteral("employees_table"));
    employees_table_->setModel(employees_proxy_);
    employees_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    employees_table_->setSelectionMode(QAbstractItemView::SingleSelection);
    employees_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    employees_table_->setAlternatingRowColors(true);
    employees_table_->setSortingEnabled(true);
    employees_table_->sortByColumn(
        EmployeeTableModel::FullName, Qt::AscendingOrder);

    QHeaderView* employees_header = employees_table_->horizontalHeader();
    employees_header->setDefaultAlignment(
        Qt::AlignLeft | Qt::AlignVCenter);
    employees_header->setStretchLastSection(false);
    employees_header->setSectionResizeMode(
        EmployeeTableModel::FullName, QHeaderView::Stretch);
    for (int column = EmployeeTableModel::PersonnelNumber;
         column < EmployeeTableModel::ColumnCount;
         ++column) {
        employees_header->setSectionResizeMode(
            column, QHeaderView::ResizeToContents);
    }
    employees_table_->verticalHeader()->setVisible(false);
    employees_card_layout->addWidget(employees_table_);
    auto* employees_buttons = new QHBoxLayout;
    add_employee_button_ =
        new QPushButton(QStringLiteral("Создать сотрудника"), employees_card);
    employee_card_button_ =
        new QPushButton(QStringLiteral("Открыть"), employees_card);
    refresh_employees_button_ =
        new QPushButton(QStringLiteral("Обновить"), employees_card);
    employees_buttons->addWidget(add_employee_button_);
    employees_buttons->addWidget(employee_card_button_);
    employees_buttons->addWidget(refresh_employees_button_);
    employees_buttons->addStretch();
    employees_card_layout->addLayout(employees_buttons);
    list_layout->addWidget(employees_card, 1);

    sections->add_page(
        QStringLiteral("Список сотрудников"), list_page);
    sections->add_page(
        QStringLiteral("Архив"),
        create_placeholder_content(
            QStringLiteral("Архив сотрудников"),
            QStringLiteral("Архив уволенных сотрудников и кадровых документов"),
            QStringLiteral(
                "Раздел будет добавлен в следующих этапах. "
                "Сейчас история кадровых событий доступна в карточке "
                "сотрудника.")));
    layout->addWidget(sections, 1);
    return page;
}

QWidget* MainWindow::create_timesheet_page() {
    auto* page = new QWidget;
    page->setObjectName(QStringLiteral("time_tab"));
    page->setAutoFillBackground(true);
    auto* layout = make_page_layout(page);

    auto* sections = new SectionPage(page);

    auto* month_page = new QWidget;
    auto* month_layout = make_subpage_layout(month_page);
    add_page_header(
        month_layout,
        QStringLiteral("Табель за месяц"),
        QStringLiteral(
            "Рабочие дни по графику и фактически отработанное время"));

    auto* controls = new QHBoxLayout;
    controls->setSpacing(10);
    controls->addWidget(
        new QLabel(QStringLiteral("Месяц:"), month_page));
    timesheet_month_combo_ = new QComboBox(month_page);
    const QStringList month_names = {
        QStringLiteral("Январь"),
        QStringLiteral("Февраль"),
        QStringLiteral("Март"),
        QStringLiteral("Апрель"),
        QStringLiteral("Май"),
        QStringLiteral("Июнь"),
        QStringLiteral("Июль"),
        QStringLiteral("Август"),
        QStringLiteral("Сентябрь"),
        QStringLiteral("Октябрь"),
        QStringLiteral("Ноябрь"),
        QStringLiteral("Декабрь")};
    for (int index = 0; index < month_names.size(); ++index) {
        timesheet_month_combo_->addItem(
            month_names[index], index + 1);
    }
    controls->addWidget(timesheet_month_combo_);

    controls->addWidget(
        new QLabel(QStringLiteral("Год:"), month_page));
    timesheet_year_spin_ = new QSpinBox(month_page);
    timesheet_year_spin_->setRange(1900, 9999);
    timesheet_year_spin_->setValue(QDate::currentDate().year());
    timesheet_month_combo_->setCurrentIndex(
        QDate::currentDate().month() - 1);
    controls->addWidget(timesheet_year_spin_);

    timesheet_build_button_ = new QPushButton(
        QStringLiteral("Сформировать"), month_page);
    timesheet_build_button_->setObjectName(
        QStringLiteral("primaryButton"));
    timesheet_save_button_ = new QPushButton(
        QStringLiteral("Сохранить"), month_page);
    timesheet_save_button_->setEnabled(false);
    controls->addWidget(timesheet_build_button_);
    controls->addWidget(timesheet_save_button_);
    controls->addStretch();
    month_layout->addLayout(controls);

    timesheet_status_label_ = new QLabel(month_page);
    timesheet_status_label_->setObjectName(
        QStringLiteral("pageStatus"));
    timesheet_status_label_->setWordWrap(true);
    month_layout->addWidget(timesheet_status_label_);

    auto* card = new QFrame(month_page);
    card->setObjectName(QStringLiteral("infoCard"));
    auto* card_layout = new QVBoxLayout(card);
    card_layout->setContentsMargins(16, 16, 16, 16);
    card_layout->setSpacing(10);

    timesheet_model_ = new TimesheetTableModel(this);
    timesheet_table_ = new QTableView(card);
    timesheet_table_->setObjectName(
        QStringLiteral("timesheet_table"));
    timesheet_table_->setModel(timesheet_model_);
    timesheet_table_->setItemDelegate(
        new TimesheetHoursDelegate(timesheet_table_));
    timesheet_table_->setSelectionBehavior(
        QAbstractItemView::SelectItems);
    timesheet_table_->setSelectionMode(
        QAbstractItemView::SingleSelection);
    timesheet_table_->setEditTriggers(
        QAbstractItemView::DoubleClicked |
        QAbstractItemView::SelectedClicked |
        QAbstractItemView::EditKeyPressed);
    timesheet_table_->setAlternatingRowColors(true);
    timesheet_table_->setWordWrap(false);
    timesheet_table_->setHorizontalScrollMode(
        QAbstractItemView::ScrollPerPixel);
    timesheet_table_->setVerticalScrollMode(
        QAbstractItemView::ScrollPerPixel);
    timesheet_table_->verticalHeader()->setVisible(false);

    QHeaderView* header =
        timesheet_table_->horizontalHeader();
    header->setDefaultAlignment(
        Qt::AlignHCenter | Qt::AlignVCenter);
    header->setStretchLastSection(false);
    header->setMinimumSectionSize(36);
    card_layout->addWidget(timesheet_table_);
    month_layout->addWidget(card, 1);

    connect(
        timesheet_build_button_,
        &QPushButton::clicked,
        this,
        [this]() {
            if (!database_open_ ||
                !selected_org_id_.has_value()) {
                update_timesheet_access();
                return;
            }
            if (timesheet_model_->has_unsaved_changes()) {
                const auto answer = QMessageBox::warning(
                    this,
                    QStringLiteral("Несохранённые изменения"),
                    QStringLiteral(
                        "Сохранить изменения табеля?"),
                    QMessageBox::Save |
                        QMessageBox::Discard |
                        QMessageBox::Cancel,
                    QMessageBox::Cancel);
                if (answer == QMessageBox::Cancel) {
                    return;
                }
                if (answer == QMessageBox::Save) {
                    save_timesheet();
                    return;
                }
            }
            refresh_timesheet();
        });
    connect(
        timesheet_save_button_,
        &QPushButton::clicked,
        this,
        [this]() { save_timesheet(); });
    connect(
        timesheet_model_,
        &QAbstractItemModel::dataChanged,
        this,
        [this]() {
            timesheet_save_button_->setEnabled(
                timesheet_model_->has_unsaved_changes());
        });

    sections->add_page(
        QStringLiteral("Табель за месяц"), month_page);
    layout->addWidget(sections, 1);
    update_timesheet_access();
    return page;
}

QWidget* MainWindow::create_calculation_page() {
    auto* page = new QWidget;
    page->setObjectName(QStringLiteral("calculation_tab"));
    page->setAutoFillBackground(true);
    auto* layout = make_page_layout(page);

    auto* sections = new SectionPage(page);

    auto* parameters_page = new QWidget;
    auto* parameters_layout =
        make_subpage_layout(parameters_page);
    add_page_header(
        parameters_layout,
        QStringLiteral("Параметры расчёта"),
        QStringLiteral(
            "Период, норма рабочего времени и данные табеля"));

    auto* controls = new QHBoxLayout;
    controls->setSpacing(10);
    controls->addWidget(
        new QLabel(QStringLiteral("Месяц:"), parameters_page));
    calculation_month_combo_ = new QComboBox(parameters_page);
    const QStringList month_names = {
        QStringLiteral("Январь"),
        QStringLiteral("Февраль"),
        QStringLiteral("Март"),
        QStringLiteral("Апрель"),
        QStringLiteral("Май"),
        QStringLiteral("Июнь"),
        QStringLiteral("Июль"),
        QStringLiteral("Август"),
        QStringLiteral("Сентябрь"),
        QStringLiteral("Октябрь"),
        QStringLiteral("Ноябрь"),
        QStringLiteral("Декабрь")};
    for (int index = 0; index < month_names.size(); ++index) {
        calculation_month_combo_->addItem(
            month_names[index], index + 1);
    }
    calculation_month_combo_->setCurrentIndex(
        QDate::currentDate().month() - 1);
    controls->addWidget(calculation_month_combo_);

    controls->addWidget(
        new QLabel(QStringLiteral("Год:"), parameters_page));
    calculation_year_spin_ = new QSpinBox(parameters_page);
    calculation_year_spin_->setRange(1900, 9999);
    calculation_year_spin_->setValue(QDate::currentDate().year());
    controls->addWidget(calculation_year_spin_);

    calculation_run_button_ = new QPushButton(
        QStringLiteral("Рассчитать"), parameters_page);
    calculation_run_button_->setObjectName(
        QStringLiteral("primaryButton"));
    controls->addWidget(calculation_run_button_);
    controls->addStretch();
    parameters_layout->addLayout(controls);

    calculation_org_label_ = new QLabel(parameters_page);
    calculation_org_label_->setWordWrap(true);
    parameters_layout->addWidget(calculation_org_label_);

    calculation_norm_label_ = new QLabel(parameters_page);
    calculation_norm_label_->setWordWrap(true);
    parameters_layout->addWidget(calculation_norm_label_);

    calculation_timesheet_label_ = new QLabel(parameters_page);
    calculation_timesheet_label_->setWordWrap(true);
    parameters_layout->addWidget(calculation_timesheet_label_);

    auto* settings_card = new QFrame(parameters_page);
    settings_card->setObjectName(QStringLiteral("infoCard"));
    auto* settings_layout = new QVBoxLayout(settings_card);
    settings_layout->setContentsMargins(16, 16, 16, 16);
    settings_layout->setSpacing(8);
    auto* settings_title =
        new QLabel(QStringLiteral("Настройки расчёта"), settings_card);
    settings_title->setObjectName(QStringLiteral("infoText"));
    settings_layout->addWidget(settings_title);
    calculation_settings_label_ = new QLabel(settings_card);
    calculation_settings_label_->setObjectName(
        QStringLiteral("infoText"));
    calculation_settings_label_->setWordWrap(true);
    settings_layout->addWidget(calculation_settings_label_);
    parameters_layout->addWidget(settings_card);

    calculation_status_label_ = new QLabel(parameters_page);
    calculation_status_label_->setObjectName(
        QStringLiteral("pageStatus"));
    calculation_status_label_->setWordWrap(true);
    parameters_layout->addWidget(calculation_status_label_);
    parameters_layout->addStretch();

    auto* results_page = new QWidget;
    auto* results_layout = make_subpage_layout(results_page);
    add_page_header(
        results_layout,
        QStringLiteral("Результаты расчёта"),
        QStringLiteral(
            "Сотрудники, отработанное время и суммы за период"));

    calculation_results_status_label_ =
        new QLabel(results_page);
    calculation_results_status_label_->setObjectName(
        QStringLiteral("pageStatus"));
    calculation_results_status_label_->setWordWrap(true);
    results_layout->addWidget(calculation_results_status_label_);

    auto* results_card = new QFrame(results_page);
    results_card->setObjectName(QStringLiteral("infoCard"));
    auto* results_card_layout = new QVBoxLayout(results_card);
    results_card_layout->setContentsMargins(16, 16, 16, 16);
    results_card_layout->setSpacing(10);

    calculation_model_ = new PayrollTableModel(this);
    calculation_proxy_ = new QSortFilterProxyModel(this);
    calculation_proxy_->setSourceModel(calculation_model_);
    calculation_proxy_->setSortCaseSensitivity(Qt::CaseInsensitive);
    calculation_proxy_->setSortRole(Qt::UserRole);

    calculation_table_ = new QTableView(results_card);
    calculation_table_->setObjectName(
        QStringLiteral("calculation_table"));
    calculation_table_->setModel(calculation_proxy_);
    calculation_table_->setSelectionBehavior(
        QAbstractItemView::SelectRows);
    calculation_table_->setSelectionMode(
        QAbstractItemView::SingleSelection);
    calculation_table_->setEditTriggers(
        QAbstractItemView::NoEditTriggers);
    calculation_table_->setAlternatingRowColors(true);
    calculation_table_->setWordWrap(false);
    calculation_table_->setSortingEnabled(true);
    calculation_table_->sortByColumn(
        PayrollTableModel::FullName, Qt::AscendingOrder);
    calculation_table_->verticalHeader()->setVisible(false);

    QHeaderView* calculation_header =
        calculation_table_->horizontalHeader();
    calculation_header->setDefaultAlignment(
        Qt::AlignLeft | Qt::AlignVCenter);
    calculation_header->setStretchLastSection(false);
    calculation_header->setSectionResizeMode(
        PayrollTableModel::FullName, QHeaderView::Stretch);
    calculation_header->setSectionResizeMode(
        PayrollTableModel::Position, QHeaderView::Stretch);
    for (int column = PayrollTableModel::WorkedHours;
         column < PayrollTableModel::ColumnCount;
         ++column) {
        calculation_header->setSectionResizeMode(
            column, QHeaderView::ResizeToContents);
    }
    results_card_layout->addWidget(calculation_table_);

    auto* results_buttons = new QHBoxLayout;
    calculation_refresh_button_ = new QPushButton(
        QStringLiteral("Обновить"), results_card);
    calculation_save_button_ = new QPushButton(
        QStringLiteral("Сохранить расчёт"), results_card);
    calculation_save_button_->setEnabled(false);
    results_buttons->addWidget(calculation_refresh_button_);
    results_buttons->addWidget(calculation_save_button_);
    results_buttons->addStretch();
    results_card_layout->addLayout(results_buttons);
    results_layout->addWidget(results_card, 1);

    connect(
        calculation_run_button_,
        &QPushButton::clicked,
        this,
        [this]() { run_calculation(); });
    connect(
        calculation_refresh_button_,
        &QPushButton::clicked,
        this,
        [this]() { run_calculation(); });
    connect(
        calculation_save_button_,
        &QPushButton::clicked,
        this,
        [this]() { save_calculation_result(); });
    connect(
        calculation_month_combo_,
        QOverload<int>::of(&QComboBox::currentIndexChanged),
        this,
        [this](int) {
            refresh_calculation_parameters();
            clear_calculation_results(QStringLiteral(
                "Период изменён. Нажмите «Рассчитать»."));
        });
    connect(
        calculation_year_spin_,
        QOverload<int>::of(&QSpinBox::valueChanged),
        this,
        [this](int) {
            refresh_calculation_parameters();
            clear_calculation_results(QStringLiteral(
                "Период изменён. Нажмите «Рассчитать»."));
        });

    sections->add_page(
        QStringLiteral("Параметры расчёта"), parameters_page);
    sections->add_page(
        QStringLiteral("Результаты"), results_page);
    layout->addWidget(sections, 1);
    update_calculation_access();
    return page;
}

QWidget* MainWindow::create_placeholder_content(
    const QString& title,
    const QString& subtitle,
    const QString& message) {
    auto* page = new QWidget;
    auto* layout = make_subpage_layout(page);
    add_page_header(layout, title, subtitle);

    auto* card = new QFrame(page);
    card->setObjectName(QStringLiteral("infoCard"));
    auto* card_layout = new QVBoxLayout(card);
    card_layout->setContentsMargins(20, 18, 20, 18);
    auto* text = new QLabel(message, card);
    text->setObjectName(QStringLiteral("infoText"));
    text->setWordWrap(true);
    card_layout->addWidget(text);
    layout->addWidget(card);
    layout->addStretch();
    return page;
}

void MainWindow::open_section(SectionId section) {
    if (workspace_tabs_ == nullptr) {
        return;
    }
    workspace_tabs_->open_section(
        static_cast<int>(section),
        section_title(section),
        section != SectionId::Dashboard);
}

void MainWindow::update_navigation_state() {
    if (sidebar_ == nullptr) {
        return;
    }

    const bool has_organization =
        database_open_ && selected_org_id_.has_value();
    sidebar_->set_section_enabled(
        static_cast<int>(SectionId::Timesheet), has_organization);
    sidebar_->set_section_enabled(
        static_cast<int>(SectionId::Calculation), has_organization);

    if (database_open_ && !database_path_.isEmpty()) {
        sidebar_->set_database_status(
            QStringLiteral("БД: %1")
                .arg(QFileInfo(database_path_).fileName()),
            database_path_);
    } else {
        sidebar_->set_database_status(
            QStringLiteral("База данных не открыта"));
    }
}

void MainWindow::refresh_dashboard() {
    if (dashboard_database_label_ == nullptr ||
        dashboard_organization_label_ == nullptr) {
        return;
    }

    if (database_open_ && !database_path_.isEmpty()) {
        dashboard_database_label_->setText(database_path_);
        dashboard_database_label_->setToolTip(database_path_);
    } else {
        dashboard_database_label_->setText(
            QStringLiteral("База данных не открыта"));
        dashboard_database_label_->setToolTip(QString());
    }

    QString organization = QStringLiteral("Не выбрана");
    if (selected_org_id_.has_value() &&
        organization_combo_ != nullptr &&
        organization_combo_->currentIndex() > 0) {
        organization = organization_combo_->currentText();
    }
    dashboard_organization_label_->setText(organization);
}

void MainWindow::refresh_organization_status() {
    if (organization_status_label_ == nullptr) {
        return;
    }

    if (!database_open_) {
        organization_status_label_->setText(
            QStringLiteral(
                "Откройте базу данных в подразделе «База данных»."));
        organization_status_label_->show();
    } else if (!selected_org_id_.has_value()) {
        organization_status_label_->setText(
            QStringLiteral(
                "Выберите существующую организацию или создайте новую."));
        organization_status_label_->show();
    } else {
        organization_status_label_->hide();
    }
}


void MainWindow::update_reference_access() {
    const bool enabled = database_open_ && selected_org_id_.has_value();
    if (!database_open_) {
        references_status_label_->setText(
            QStringLiteral("Откройте базу данных"));
        references_status_label_->show();
    } else if (!selected_org_id_.has_value()) {
        references_status_label_->setText(
            QStringLiteral("Выберите организацию"));
        references_status_label_->show();
    } else {
        references_status_label_->hide();
    }

    positions_table_->setEnabled(enabled);
    work_schedules_table_->setEnabled(enabled);
    add_position_button_->setEnabled(enabled);
    edit_position_button_->setEnabled(enabled);
    remove_position_button_->setEnabled(enabled);
    refresh_positions_button_->setEnabled(enabled);
    add_work_schedule_button_->setEnabled(enabled);
    edit_work_schedule_button_->setEnabled(enabled);
    remove_work_schedule_button_->setEnabled(enabled);
    refresh_work_schedules_button_->setEnabled(enabled);
    if (!enabled) {
        positions_table_->setRowCount(0);
        work_schedules_table_->setRowCount(0);
    }
}

void MainWindow::update_employee_access() {
    const bool enabled = database_open_ && selected_org_id_.has_value();
    employees_table_->setEnabled(enabled);
    add_employee_button_->setEnabled(enabled);
    employee_card_button_->setEnabled(enabled);
    refresh_employees_button_->setEnabled(enabled);

    if (!enabled) {
        employees_model_->clear();
        employees_table_->clearSelection();
        employees_table_->setCurrentIndex(QModelIndex());
        employees_status_label_->setText(
            database_open_
                ? QStringLiteral("Выберите организацию")
                : QStringLiteral("Откройте базу данных"));
        employees_status_label_->show();
    }
}

void MainWindow::refresh_employees() {
    if (!database_open_ || !selected_org_id_.has_value()) {
        update_employee_access();
        return;
    }

    try {
        const std::vector<Employee> employees =
            list_employees(database_, *selected_org_id_);
        const std::vector<Department> departments =
            list_departments(database_, *selected_org_id_);
        const std::vector<Position> positions =
            list_positions(database_);

        std::vector<EmployeeTableRow> rows;
        rows.reserve(employees.size());
        for (const Employee& employee : employees) {
            rows.push_back({
                employee.id,
                QString::fromStdString(employee.full_name),
                QString::fromStdString(employee.personnel_number),
                position_name_or_placeholder(
                    positions, employee.position_id),
                department_name_or_placeholder(
                    departments, employee.department_id),
                employee_status_text(employee)});
        }

        employees_model_->set_rows(std::move(rows));
        employees_table_->clearSelection();
        employees_table_->setCurrentIndex(QModelIndex());
        if (employees.empty()) {
            employees_status_label_->setText(
                QStringLiteral("В выбранной организации нет сотрудников"));
            employees_status_label_->show();
        } else {
            employees_status_label_->hide();
        }
    } catch (const std::exception& error) {
        employees_model_->clear();
        employees_table_->clearSelection();
        employees_table_->setCurrentIndex(QModelIndex());
        employees_status_label_->setText(
            QStringLiteral("Не удалось загрузить список сотрудников."));
        employees_status_label_->show();
        QMessageBox::critical(
            this,
            QStringLiteral("Ошибка загрузки сотрудников"),
            QStringLiteral("Не удалось загрузить список сотрудников.\n\n%1")
                .arg(QString::fromUtf8(error.what())));
    }
}

void MainWindow::add_employee() {
    if (!database_open_ || !selected_org_id_.has_value()) {
        return;
    }

    int64_t created_id = 0;
    if (!create_employee_dialog(
            this, database_, *selected_org_id_, created_id)) {
        return;
    }

    refresh_employees();
    select_employee_row(created_id);
}

void MainWindow::open_employee_card() {
    const auto id = selected_employee_id();
    if (!id.has_value()) {
        QMessageBox::information(
            this,
            QStringLiteral("Сотрудники"),
            QStringLiteral("Выберите сотрудника."));
        return;
    }
    if (!database_open_ || !selected_org_id_.has_value()) {
        return;
    }

    employee_card_dialog(
        this,
        database_,
        *selected_org_id_,
        *id,
        [this]() { refresh_employees(); });
    select_employee_row(*id);
}

void MainWindow::select_employee_row(int64_t employee_id) {
    if (employees_model_ == nullptr || employees_proxy_ == nullptr) {
        return;
    }

    const int source_row = employees_model_->row_for_employee(employee_id);
    if (source_row < 0) {
        return;
    }

    const QModelIndex source_index =
        employees_model_->index(source_row, EmployeeTableModel::FullName);
    const QModelIndex proxy_index =
        employees_proxy_->mapFromSource(source_index);
    if (!proxy_index.isValid()) {
        return;
    }

    employees_table_->selectRow(proxy_index.row());
    employees_table_->setCurrentIndex(proxy_index);
    employees_table_->scrollTo(
        proxy_index, QAbstractItemView::PositionAtCenter);
}

std::optional<int64_t> MainWindow::selected_employee_id() const {
    const QModelIndex index = employees_table_->currentIndex();
    if (!index.isValid()) {
        return std::nullopt;
    }

    const QModelIndex source_index = employees_proxy_->mapToSource(index);
    if (!source_index.isValid()) {
        return std::nullopt;
    }

    const int64_t id = employees_model_->employee_id_at(source_index.row());
    if (id == 0) {
        return std::nullopt;
    }
    return id;
}

void MainWindow::configure_timesheet_columns() {
    if (timesheet_table_ == nullptr ||
        timesheet_model_ == nullptr) {
        return;
    }

    QHeaderView* header =
        timesheet_table_->horizontalHeader();
    header->setSectionResizeMode(QHeaderView::Interactive);
    timesheet_table_->setColumnWidth(
        TimesheetTableModel::FullName, 220);
    timesheet_table_->setColumnWidth(
        TimesheetTableModel::PersonnelNumber, 110);
    timesheet_table_->setColumnWidth(
        TimesheetTableModel::Position, 150);
    for (int day = 1;
         day <= timesheet_model_->day_count();
         ++day) {
        timesheet_table_->setColumnWidth(
            timesheet_model_->day_column(day), 50);
    }
    for (int column = 0;
         column < TimesheetTableModel::TotalColumnCount;
         ++column) {
        const bool hours =
            column == TimesheetTableModel::WorkedHours ||
            column == TimesheetTableModel::MissedHours;
        timesheet_table_->setColumnWidth(
            timesheet_model_->total_column(
                static_cast<TimesheetTableModel::TotalColumn>(
                    column)),
            hours ? 100 : 88);
    }
}

void MainWindow::update_timesheet_access() {
    if (timesheet_table_ == nullptr ||
        timesheet_model_ == nullptr) {
        return;
    }

    const bool enabled =
        database_open_ && selected_org_id_.has_value();
    timesheet_month_combo_->setEnabled(enabled);
    timesheet_year_spin_->setEnabled(enabled);
    timesheet_build_button_->setEnabled(enabled);
    timesheet_table_->setEnabled(enabled);
    timesheet_save_button_->setEnabled(
        enabled && timesheet_model_->has_unsaved_changes());

    if (!enabled) {
        timesheet_model_->clear();
        configure_timesheet_columns();
        timesheet_status_label_->setText(
            database_open_
                ? QStringLiteral("Выберите организацию")
                : QStringLiteral("Откройте базу данных"));
        timesheet_status_label_->show();
    }
}

void MainWindow::refresh_timesheet() {
    if (timesheet_model_ == nullptr) {
        return;
    }
    if (!database_open_ || !selected_org_id_.has_value()) {
        update_timesheet_access();
        return;
    }
    update_timesheet_access();

    const int month =
        timesheet_month_combo_->currentData().toInt();
    const int year = timesheet_year_spin_->value();
    const QString month_name =
        timesheet_month_combo_->currentText();

    try {
        const Date from(year, month, 1);
        const Date to(
            year,
            month,
            Date::days_in_month(year, month));

        const std::vector<Employee> employees =
            list_employees(database_, *selected_org_id_);
        const std::vector<Position> positions =
            list_positions(database_);
        const std::vector<WorkSchedule> schedules =
            list_work_schedules(database_);
        const std::vector<AbsenceKind> absence_kinds =
            list_absence_kinds(database_);

        std::vector<TimesheetEmployeeSource> sources;
        sources.reserve(employees.size());
        for (const Employee& employee : employees) {
            TimesheetEmployeeSource source;
            source.employee = employee;
            source.position_name = position_name_or_placeholder(
                positions, employee.position_id);
            source.time_facts = list_time_facts(
                database_, employee.id, from, to);
            sources.push_back(std::move(source));
        }

        timesheet_model_->set_data(
            year,
            month,
            std::move(sources),
            schedules,
            absence_kinds);
        configure_timesheet_columns();

        if (employees.empty()) {
            timesheet_status_label_->setText(
                QStringLiteral(
                    "В выбранной организации нет сотрудников."));
        } else {
            QString status = QStringLiteral(
                "Табель сформирован: %1 %2.")
                .arg(month_name)
                .arg(year);
            const QStringList warnings =
                timesheet_model_->warnings();
            if (!warnings.isEmpty()) {
                status += QStringLiteral(
                    "\nГрафик не удалось построить: %1")
                    .arg(warnings.join(QStringLiteral("; ")));
            }
            timesheet_status_label_->setText(status);
        }
        timesheet_status_label_->setWordWrap(true);
        timesheet_status_label_->show();
        timesheet_save_button_->setEnabled(false);
    } catch (const std::exception& error) {
        timesheet_model_->clear();
        configure_timesheet_columns();
        timesheet_status_label_->setText(
            QStringLiteral(
                "Не удалось сформировать табель."));
        timesheet_status_label_->show();
        QMessageBox::critical(
            this,
            QStringLiteral("Ошибка формирования табеля"),
            QString::fromUtf8(error.what()));
    }
}

bool MainWindow::save_timesheet() {
    if (!database_open_ ||
        !selected_org_id_.has_value() ||
        timesheet_model_ == nullptr) {
        return false;
    }

    const std::vector<TimesheetPendingChange> changes =
        timesheet_model_->pending_changes();
    if (changes.empty()) {
        return true;
    }

    try {
        database_.transaction();
        try {
            for (const TimesheetPendingChange& change : changes) {
                TimeFact fact;
                fact.employee_id = change.employee_id;
                fact.date = change.date;
                fact.worked_minutes = change.worked_minutes;
                fact.absence_kind_id =
                    change.absence_kind_id;
                if (change.time_fact_id != 0) {
                    update_time_fact(
                        database_,
                        change.time_fact_id,
                        fact);
                } else {
                    create_time_fact(database_, fact);
                }
            }
            database_.commit();
        } catch (...) {
            database_.rollback();
            throw;
        }
    } catch (const std::exception& error) {
        QMessageBox::critical(
            this,
            QStringLiteral("Ошибка сохранения табеля"),
            QStringLiteral(
                "Не удалось сохранить изменения табеля.\n\n%1")
                .arg(QString::fromUtf8(error.what())));
        return false;
    }

    refresh_timesheet();
    timesheet_status_label_->setText(
        QStringLiteral("Изменения табеля сохранены."));
    timesheet_save_button_->setEnabled(false);
    return true;
}

void MainWindow::update_calculation_access() {
    if (calculation_table_ == nullptr ||
        calculation_model_ == nullptr) {
        return;
    }

    const bool enabled =
        database_open_ && selected_org_id_.has_value();
    calculation_month_combo_->setEnabled(enabled);
    calculation_year_spin_->setEnabled(enabled);
    calculation_run_button_->setEnabled(enabled);
    calculation_refresh_button_->setEnabled(enabled);
    calculation_table_->setEnabled(enabled);
    calculation_save_button_->setEnabled(false);
    calculation_model_->clear();
    payroll_report_.reset();

    if (!enabled) {
        calculation_org_label_->setText(
            database_open_
                ? QStringLiteral("Организация: не выбрана")
                : QStringLiteral(
                    "Организация: база данных не открыта"));
        calculation_norm_label_->setText(
            QStringLiteral(
                "Норма рабочего времени: не определена."));
        calculation_timesheet_label_->setText(
            QStringLiteral("Табель за период: не проверен."));
        calculation_settings_label_->setText(
            QStringLiteral(
                "Откройте базу данных и выберите организацию, "
                "чтобы увидеть настройки расчёта."));

        const QString message = database_open_
            ? QStringLiteral("Выберите организацию")
            : QStringLiteral("Откройте базу данных");
        calculation_status_label_->setText(message);
        calculation_status_label_->show();
        calculation_results_status_label_->setText(message);
        calculation_results_status_label_->show();
        return;
    }

    refresh_calculation_parameters();
    const QString message = QStringLiteral(
        "Нажмите «Рассчитать», чтобы получить результаты "
        "за выбранный период.");
    calculation_status_label_->setText(message);
    calculation_status_label_->show();
    calculation_results_status_label_->setText(message);
    calculation_results_status_label_->show();
}

void MainWindow::refresh_calculation_parameters() {
    if (!database_open_ ||
        !selected_org_id_.has_value() ||
        calculation_norm_label_ == nullptr) {
        return;
    }

    const int month =
        calculation_month_combo_->currentData().toInt();
    const int year = calculation_year_spin_->value();

    try {
        const QString organization =
            organization_combo_ != nullptr &&
                organization_combo_->currentIndex() > 0
            ? organization_combo_->currentText()
            : QStringLiteral("не выбрана");
        calculation_org_label_->setText(
            QStringLiteral("Организация: %1").arg(organization));

        const std::vector<PayrollNormInfo> norms =
            payroll_norms(
                database_, *selected_org_id_, year, month);
        if (norms.empty()) {
            calculation_norm_label_->setText(
                QStringLiteral(
                    "Норма рабочего времени: не определена — "
                    "у сотрудников не указан график работы."));
        } else {
            QStringList lines;
            for (const PayrollNormInfo& norm : norms) {
                lines << QStringLiteral(
                    "%1 — %2 раб. дн., %3 (сотрудников: %4)")
                    .arg(
                        QString::fromStdString(norm.schedule_name))
                    .arg(norm.working_days)
                    .arg(format_minutes(norm.norm_minutes))
                    .arg(norm.employee_count);
            }
            calculation_norm_label_->setText(
                QStringLiteral("Норма рабочего времени:\n%1")
                    .arg(lines.join(QStringLiteral("\n"))));
        }

        const bool has_timesheet = payroll_has_timesheet(
            database_, *selected_org_id_, year, month);
        calculation_timesheet_label_->setText(
            has_timesheet
                ? QStringLiteral("Табель за период: заполнен.")
                : QStringLiteral(
                    "Табель за период: не заполнен. "
                    "Без табеля суммы не рассчитываются."));

        QStringList settings;
        const std::vector<PayrollPreset> presets =
            list_payroll_presets(database_);
        const std::vector<CalcItem> items =
            list_calc_items(database_);

        if (presets.empty()) {
            settings << QStringLiteral(
                "Наборы начислений не настроены.");
        }
        for (const PayrollPreset& preset : presets) {
            settings << QStringLiteral("Набор: %1 (%2)")
                .arg(
                    QString::fromStdString(preset.name),
                    QString::fromStdString(preset.code));
        }

        if (items.empty()) {
            settings << QStringLiteral(
                "Начисления не настроены — суммы не рассчитываются.");
        }
        for (const CalcItem& item : items) {
            settings << QStringLiteral(
                "Начисление: %1 (%2), формула: %3")
                .arg(
                    QString::fromStdString(item.name),
                    QString::fromStdString(item.code),
                    QString::fromStdString(item.formula));
        }

        settings << QStringLiteral(
            "Налоги, удержания и премии не настроены в базе "
            "и не рассчитываются.");
        calculation_settings_label_->setText(
            settings.join(QStringLiteral("\n")));
    } catch (const std::exception& error) {
        calculation_norm_label_->setText(
            QStringLiteral("Норма рабочего времени: не определена."));
        calculation_timesheet_label_->setText(
            QStringLiteral("Табель за период: не проверен."));
        calculation_settings_label_->setText(
            QStringLiteral("Настройки расчёта недоступны."));
        calculation_status_label_->setText(
            QStringLiteral(
                "Не удалось прочитать параметры расчёта: %1")
                .arg(QString::fromUtf8(error.what())));
        calculation_status_label_->show();
    }
}

void MainWindow::clear_calculation_results(
    const QString& message) {
    payroll_report_.reset();
    if (calculation_model_ != nullptr) {
        calculation_model_->clear();
    }
    if (calculation_save_button_ != nullptr) {
        calculation_save_button_->setEnabled(false);
    }
    if (calculation_results_status_label_ != nullptr) {
        calculation_results_status_label_->setText(message);
        calculation_results_status_label_->show();
    }
    if (calculation_status_label_ != nullptr) {
        calculation_status_label_->setText(message);
        calculation_status_label_->show();
    }
}

void MainWindow::run_calculation() {
    if (!database_open_ ||
        !selected_org_id_.has_value() ||
        calculation_model_ == nullptr) {
        update_calculation_access();
        return;
    }

    const int month =
        calculation_month_combo_->currentData().toInt();
    const int year = calculation_year_spin_->value();
    const QString period = QStringLiteral("%1 %2")
        .arg(calculation_month_combo_->currentText())
        .arg(year);

    try {
        const PayrollReport report = run_payroll(
            database_, *selected_org_id_, year, month);

        std::vector<PayrollTableRow> rows;
        rows.reserve(report.rows.size());
        int calculated = 0;

        for (const PayrollRow& value : report.rows) {
            PayrollTableRow row;
            row.employee_id = value.employee_id;
            row.full_name =
                QString::fromStdString(value.full_name);
            row.position =
                QString::fromStdString(value.position_name);
            row.worked_minutes = value.worked_minutes;
            row.worked_days = value.worked_days;
            row.calculated = value.calculated;
            row.accrual_cents = value.accrual.cents;
            row.deductions_cents = value.deductions.cents;
            row.net_cents = value.net.cents;
            row.status = format_calc_status(value.status);

            QStringList reasons;
            for (const std::string& reason : value.reasons) {
                reasons << QString::fromStdString(reason);
            }
            row.reasons = reasons.join(QStringLiteral("\n"));

            if (value.calculated) {
                ++calculated;
            }
            rows.push_back(std::move(row));
        }

        calculation_model_->set_rows(std::move(rows));
        payroll_report_ = report;
        calculation_save_button_->setEnabled(calculated > 0);

        QString status;
        if (report.rows.empty()) {
            status = QStringLiteral(
                "В выбранной организации нет сотрудников.");
        } else if (!report.has_timesheet) {
            status = QStringLiteral(
                "Табель за %1 не заполнен — суммы не рассчитаны. "
                "Заполните табель и повторите расчёт.")
                .arg(period);
        } else if (calculated == 0) {
            status = QStringLiteral(
                "Ни один сотрудник не рассчитан. Причина указана "
                "в колонке «Статус»; наведите курсор на строку, "
                "чтобы увидеть подробности.");
        } else {
            status = QStringLiteral(
                "Рассчитано сотрудников: %1 из %2 за %3.")
                .arg(calculated)
                .arg(report.rows.size())
                .arg(period);
        }

        for (const std::string& warning : report.warnings) {
            status += QStringLiteral("\n%1")
                .arg(QString::fromStdString(warning));
        }

        calculation_status_label_->setText(status);
        calculation_status_label_->show();
        calculation_results_status_label_->setText(status);
        calculation_results_status_label_->show();
    } catch (const std::exception& error) {
        clear_calculation_results(QStringLiteral(
            "Расчёт не выполнен."));
        calculation_status_label_->setText(
            QStringLiteral("Расчёт не выполнен: %1")
                .arg(QString::fromUtf8(error.what())));
        calculation_status_label_->show();
        QMessageBox::critical(
            this,
            QStringLiteral("Ошибка расчёта"),
            QString::fromUtf8(error.what()));
    }
}

void MainWindow::save_calculation_result() {
    if (!payroll_report_.has_value()) {
        QMessageBox::warning(
            this,
            QStringLiteral("Сохранение расчёта"),
            QStringLiteral("Сначала выполните расчёт."));
        return;
    }
    if (calculation_model_->calculated_row_count() == 0) {
        QMessageBox::warning(
            this,
            QStringLiteral("Сохранение расчёта"),
            QStringLiteral(
                "Нет рассчитанных сотрудников для сохранения."));
        return;
    }

    const QString period = QStringLiteral("%1 %2")
        .arg(calculation_month_combo_->currentText())
        .arg(calculation_year_spin_->value());

    try {
        save_payroll(database_, *payroll_report_);
        const QString message = QStringLiteral(
            "Расчёт за %1 сохранён. Повторное сохранение "
            "заменит результат за этот период.")
            .arg(period);
        calculation_status_label_->setText(message);
        calculation_status_label_->show();
        calculation_results_status_label_->setText(message);
        calculation_results_status_label_->show();
    } catch (const std::exception& error) {
        QMessageBox::critical(
            this,
            QStringLiteral("Ошибка сохранения расчёта"),
            QString::fromUtf8(error.what()));
    }
}

void MainWindow::refresh_positions() {
    if (!database_open_ || !selected_org_id_.has_value()) {
        update_reference_access();
        return;
    }
    try {
        const std::vector<Position> positions = list_positions(database_);
        positions_table_->setRowCount(0);
        for (const Position& position : positions) {
            const int row = positions_table_->rowCount();
            positions_table_->insertRow(row);
            auto* id_item =
                new QTableWidgetItem(QString::number(position.id));
            id_item->setData(
                Qt::UserRole,
                QVariant::fromValue(static_cast<qlonglong>(position.id)));
            positions_table_->setItem(row, 0, id_item);
            positions_table_->setItem(
                row, 1,
                new QTableWidgetItem(
                    QString::fromStdString(position.name)));
        }
    } catch (const std::exception& error) {
        show_reference_error(
            this, QStringLiteral("Ошибка загрузки должностей"), error);
    }
}

void MainWindow::refresh_work_schedules() {
    if (!database_open_ || !selected_org_id_.has_value()) {
        update_reference_access();
        return;
    }
    try {
        const std::vector<WorkSchedule> schedules =
            list_work_schedules(database_);
        work_schedules_table_->setRowCount(0);
        for (const WorkSchedule& schedule : schedules) {
            const int row = work_schedules_table_->rowCount();
            work_schedules_table_->insertRow(row);
            auto* id_item =
                new QTableWidgetItem(QString::number(schedule.id));
            id_item->setData(
                Qt::UserRole,
                QVariant::fromValue(static_cast<qlonglong>(schedule.id)));
            work_schedules_table_->setItem(row, 0, id_item);
            work_schedules_table_->setItem(
                row, 1,
                new QTableWidgetItem(
                    QString::fromStdString(schedule.name)));
            work_schedules_table_->setItem(
                row, 2,
                new QTableWidgetItem(
                    schedule.kind == ScheduleKind::FiveDay
                        ? QStringLiteral("Пятидневка")
                        : QStringLiteral("Сменный")));
            work_schedules_table_->setItem(
                row, 3,
                new QTableWidgetItem(
                    QString::number(schedule.normative_minutes_per_day)));
            work_schedules_table_->setItem(
                row, 4,
                new QTableWidgetItem(
                    QString::fromStdString(schedule.shift_cycle)));
            QString anchor_text;
            if (schedule.anchor_date.has_value()) {
                anchor_text = QDate(
                    schedule.anchor_date->year(),
                    schedule.anchor_date->month(),
                    schedule.anchor_date->day())
                    .toString(QStringLiteral("dd.MM.yyyy"));
            }
            work_schedules_table_->setItem(
                row, 5, new QTableWidgetItem(anchor_text));
        }
    } catch (const std::exception& error) {
        show_reference_error(
            this, QStringLiteral("Ошибка загрузки графиков"), error);
    }
}

void MainWindow::add_position() {
    if (!selected_org_id_.has_value()) {
        return;
    }
    Position position;
    if (!edit_position_dialog(this, position)) {
        return;
    }
    try {
        (void)create_position(database_, position);
        refresh_positions();
    } catch (const std::exception& error) {
        show_reference_error(
            this, QStringLiteral("Ошибка добавления должности"), error);
    }
}

void MainWindow::edit_position() {
    const auto id = selected_id(positions_table_);
    if (!id.has_value()) {
        QMessageBox::information(
            this, QStringLiteral("Должности"),
            QStringLiteral("Выберите должность."));
        return;
    }
    try {
        auto position = read_position(database_, *id);
        if (!position.has_value()) {
            throw std::runtime_error("должность не найдена");
        }
        if (edit_position_dialog(this, *position)) {
            update_position(database_, *position);
            refresh_positions();
        }
    } catch (const std::exception& error) {
        show_reference_error(
            this, QStringLiteral("Ошибка изменения должности"), error);
    }
}

void MainWindow::remove_position() {
    const auto id = selected_id(positions_table_);
    if (!id.has_value()) {
        QMessageBox::information(
            this, QStringLiteral("Должности"),
            QStringLiteral("Выберите должность."));
        return;
    }
    if (QMessageBox::question(
            this,
            QStringLiteral("Удалить должность"),
            QStringLiteral("Удалить выбранную должность?")) !=
        QMessageBox::Yes) {
        return;
    }
    try {
        delete_position(database_, *id);
        refresh_positions();
    } catch (const std::exception& error) {
        show_reference_error(
            this, QStringLiteral("Ошибка удаления должности"), error);
    }
}

void MainWindow::add_work_schedule() {
    if (!selected_org_id_.has_value()) {
        return;
    }
    WorkSchedule schedule;
    if (!edit_work_schedule_dialog(this, schedule)) {
        return;
    }
    try {
        (void)create_work_schedule(database_, schedule);
        refresh_work_schedules();
    } catch (const std::exception& error) {
        show_reference_error(
            this, QStringLiteral("Ошибка добавления графика"), error);
    }
}

void MainWindow::edit_work_schedule() {
    const auto id = selected_id(work_schedules_table_);
    if (!id.has_value()) {
        QMessageBox::information(
            this, QStringLiteral("Графики работы"),
            QStringLiteral("Выберите график."));
        return;
    }
    try {
        auto schedule = read_work_schedule(database_, *id);
        if (!schedule.has_value()) {
            throw std::runtime_error("график не найден");
        }
        if (edit_work_schedule_dialog(this, *schedule)) {
            update_work_schedule(database_, *schedule);
            refresh_work_schedules();
        }
    } catch (const std::exception& error) {
        show_reference_error(
            this, QStringLiteral("Ошибка изменения графика"), error);
    }
}

void MainWindow::remove_work_schedule() {
    const auto id = selected_id(work_schedules_table_);
    if (!id.has_value()) {
        QMessageBox::information(
            this, QStringLiteral("Графики работы"),
            QStringLiteral("Выберите график."));
        return;
    }
    if (QMessageBox::question(
            this,
            QStringLiteral("Удалить график"),
            QStringLiteral("Удалить выбранный график?")) !=
        QMessageBox::Yes) {
        return;
    }
    try {
        delete_work_schedule(database_, *id);
        refresh_work_schedules();
    } catch (const std::exception& error) {
        show_reference_error(
            this, QStringLiteral("Ошибка удаления графика"), error);
    }
}

void MainWindow::clear_database_view() {
    database_open_ = false;
    selected_org_id_.reset();
    database_path_.clear();
    saved_name_.clear();
    saved_inn_.clear();
    saved_kpp_.clear();

    const QSignalBlocker blocker(organization_combo_);
    organization_combo_->clear();
    organization_combo_->addItem(QStringLiteral("Выберите организацию"));
    organization_combo_->setEnabled(false);
    organization_name_edit_->clear();
    organization_name_edit_->setEnabled(false);
    organization_inn_edit_->clear();
    organization_inn_edit_->setEnabled(false);
    organization_kpp_edit_->clear();
    organization_kpp_edit_->setEnabled(false);
    create_organization_button_->setEnabled(false);
    save_organization_button_->setEnabled(false);
    database_path_label_->setText(QStringLiteral("Файл БД не открыт"));
    database_path_label_->setToolTip(QString());
    update_reference_access();
    update_employee_access();
    update_timesheet_access();
    update_calculation_access();
    refresh_dashboard();
    refresh_organization_status();
    update_navigation_state();
}

void MainWindow::reload_organizations(std::optional<int64_t> selected_id) {
    const std::vector<Org> organizations = list_orgs(database_);

    const QSignalBlocker blocker(organization_combo_);
    organization_combo_->clear();
    organization_combo_->addItem(QStringLiteral("Выберите организацию"));

    for (const Org& organization : organizations) {
        organization_combo_->addItem(
            QString::fromStdString(organization.name),
            QVariant::fromValue(static_cast<qlonglong>(organization.id)));
    }

    selected_org_id_.reset();
    saved_name_.clear();
    saved_inn_.clear();
    saved_kpp_.clear();
    organization_name_edit_->clear();
    organization_name_edit_->setEnabled(false);
    organization_inn_edit_->clear();
    organization_inn_edit_->setEnabled(false);
    organization_kpp_edit_->clear();
    organization_kpp_edit_->setEnabled(false);
    save_organization_button_->setEnabled(false);
    organization_combo_->setEnabled(true);
    create_organization_button_->setEnabled(true);
    update_reference_access();
    update_employee_access();
    update_timesheet_access();
    update_calculation_access();
    refresh_dashboard();
    refresh_organization_status();
    update_navigation_state();

    if (selected_id.has_value()) {
        const int index = organization_combo_->findData(
            QVariant::fromValue(static_cast<qlonglong>(*selected_id)));
        if (index > 0) {
            const auto organization = read_org(database_, *selected_id);
            if (!organization.has_value()) {
                throw std::runtime_error("organization not found");
            }
            const auto details = read_org_details(database_, *selected_id);

            organization_combo_->setCurrentIndex(index);
            selected_org_id_ = selected_id;
            saved_name_ = QString::fromStdString(organization->name);
            saved_inn_ = details.has_value()
                ? QString::fromStdString(details->inn) : QString();
            saved_kpp_ = details.has_value()
                ? QString::fromStdString(details->kpp) : QString();
            organization_name_edit_->setText(saved_name_);
            organization_inn_edit_->setText(saved_inn_);
            organization_kpp_edit_->setText(saved_kpp_);
            organization_name_edit_->setEnabled(true);
            organization_inn_edit_->setEnabled(true);
            organization_kpp_edit_->setEnabled(true);
            save_organization_button_->setEnabled(true);
            update_reference_access();
            update_employee_access();
            refresh_positions();
            refresh_work_schedules();
            refresh_employees();
            refresh_timesheet();
            update_calculation_access();
            refresh_dashboard();
            refresh_organization_status();
            update_navigation_state();
        }
    }
}

bool MainWindow::save_organization() {
    if (!database_open_ || !selected_org_id_.has_value()) {
        return false;
    }

    const QString name = organization_name_edit_->text().trimmed();
    if (name.isEmpty()) {
        QMessageBox::warning(
            this,
            QStringLiteral("Организация"),
            QStringLiteral("Введите название организации."));
        return false;
    }

    const QString inn = organization_inn_edit_->text().trimmed();
    const QString kpp = organization_kpp_edit_->text().trimmed();

    try {
        const auto organization = read_org(database_, *selected_org_id_);
        if (!organization.has_value()) {
            throw std::runtime_error("organization not found");
        }

        Org updated = *organization;
        updated.name = name.toStdString();
        update_org_with_details(
            database_,
            updated,
            {*selected_org_id_, inn.toStdString(), kpp.toStdString()});

        saved_name_ = name;
        saved_inn_ = inn;
        saved_kpp_ = kpp;
        organization_name_edit_->setText(name);
        organization_inn_edit_->setText(inn);
        organization_kpp_edit_->setText(kpp);
        organization_combo_->setItemText(
            organization_combo_->currentIndex(), name);
        refresh_dashboard();
        return true;
    } catch (const std::exception& error) {
        QMessageBox::critical(
            this,
            QStringLiteral("Ошибка сохранения организации"),
            QString::fromUtf8(error.what()));
        return false;
    }
}

bool MainWindow::confirm_unsaved_changes() {
    if (timesheet_model_ != nullptr &&
        timesheet_model_->has_unsaved_changes()) {
        const auto timesheet_answer = QMessageBox::warning(
            this,
            QStringLiteral("Несохранённые изменения"),
            QStringLiteral("Сохранить изменения табеля?"),
            QMessageBox::Save |
                QMessageBox::Discard |
                QMessageBox::Cancel,
            QMessageBox::Cancel);
        if (timesheet_answer == QMessageBox::Cancel) {
            return false;
        }
        if (timesheet_answer == QMessageBox::Save &&
            !save_timesheet()) {
            return false;
        }
    }

    if (!selected_org_id_.has_value() ||
        (organization_name_edit_->text() == saved_name_ &&
         organization_inn_edit_->text() == saved_inn_ &&
         organization_kpp_edit_->text() == saved_kpp_)) {
        return true;
    }

    const auto answer = QMessageBox::warning(
        this,
        QStringLiteral("Несохранённые изменения"),
        QStringLiteral("Сохранить изменения организации?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Cancel);

    if (answer == QMessageBox::Cancel) {
        return false;
    }
    if (answer == QMessageBox::Save) {
        return save_organization();
    }
    return true;
}

void MainWindow::create_database() {
    const QString path = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("Создать файл БД"),
        QString(),
        QStringLiteral("База данных SQLite (*.sqlite *.db);;Все файлы (*)"));
    if (path.isEmpty()) {
        return;
    }
    if (QFileInfo::exists(path)) {
        QMessageBox::warning(
            this,
            QStringLiteral("Создание файла БД"),
            QStringLiteral(
                "Файл уже существует. Используйте «Открыть файл БД», "
                "чтобы не изменять существующие данные."));
        return;
    }
    if (!confirm_unsaved_changes()) {
        return;
    }

    try {
        database_.create_new(path);
        database_open_ = true;
        database_path_ = path;
        reload_organizations();
        database_path_label_->setText(
            QStringLiteral("Файл БД: %1").arg(path));
        database_path_label_->setToolTip(path);
        refresh_dashboard();
        update_navigation_state();
    } catch (const std::exception& error) {
        database_.close();
        clear_database_view();
        QMessageBox::critical(
            this,
            QStringLiteral("Ошибка создания файла БД"),
            QString::fromUtf8(error.what()));
    }
}

void MainWindow::open_database() {
    const QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("Открыть файл БД"),
        QString(),
        QStringLiteral("База данных SQLite (*.sqlite *.db);;Все файлы (*)"));
    if (path.isEmpty()) {
        return;
    }
    if (!QFileInfo(path).isFile()) {
        QMessageBox::warning(
            this,
            QStringLiteral("Открытие файла БД"),
            QStringLiteral("Выбранный файл не существует."));
        return;
    }
    if (!confirm_unsaved_changes()) {
        return;
    }

    try {
        database_.open(path);
        database_open_ = true;
        database_path_ = path;
        reload_organizations();
        database_path_label_->setText(
            QStringLiteral("Файл БД: %1").arg(path));
        database_path_label_->setToolTip(path);
        refresh_dashboard();
        update_navigation_state();
    } catch (const std::exception& error) {
        database_.close();
        clear_database_view();
        QMessageBox::critical(
            this,
            QStringLiteral("Ошибка открытия файла БД"),
            QString::fromUtf8(error.what()));
    }
}

void MainWindow::create_organization() {
    if (!database_open_ || !confirm_unsaved_changes()) {
        return;
    }

    bool accepted = false;
    const QString name = QInputDialog::getText(
        this,
        QStringLiteral("Создать организацию"),
        QStringLiteral("Название:"),
        QLineEdit::Normal,
        QString(),
        &accepted).trimmed();
    if (!accepted) {
        return;
    }
    if (name.isEmpty()) {
        QMessageBox::warning(
            this,
            QStringLiteral("Организация"),
            QStringLiteral("Введите название организации."));
        return;
    }

    try {
        const int64_t id = create_org_with_details(
            database_, {0, name.toStdString()}, {});
        reload_organizations(id);
    } catch (const std::exception& error) {
        QMessageBox::critical(
            this,
            QStringLiteral("Ошибка создания организации"),
            QString::fromUtf8(error.what()));
    }
}

void MainWindow::select_organization(int index) {
    const std::optional<int64_t> previous_id = selected_org_id_;
    if (!confirm_unsaved_changes()) {
        const QSignalBlocker blocker(organization_combo_);
        organization_combo_->setCurrentIndex(
            previous_id.has_value()
                ? organization_combo_->findData(
                    QVariant::fromValue(
                        static_cast<qlonglong>(*previous_id)))
                : 0);
        return;
    }

    if (index <= 0) {
        selected_org_id_.reset();
        saved_name_.clear();
        saved_inn_.clear();
        saved_kpp_.clear();
        organization_name_edit_->clear();
        organization_name_edit_->setEnabled(false);
        organization_inn_edit_->clear();
        organization_inn_edit_->setEnabled(false);
        organization_kpp_edit_->clear();
        organization_kpp_edit_->setEnabled(false);
        save_organization_button_->setEnabled(false);
        update_reference_access();
        update_employee_access();
        update_timesheet_access();
        update_calculation_access();
        refresh_dashboard();
        refresh_organization_status();
        update_navigation_state();
        return;
    }

    const int64_t id = organization_combo_->itemData(index).toLongLong();
    try {
        const auto organization = read_org(database_, id);
        if (!organization.has_value()) {
            throw std::runtime_error("organization not found");
        }
        const auto details = read_org_details(database_, id);

        selected_org_id_ = id;
        saved_name_ = QString::fromStdString(organization->name);
        saved_inn_ = details.has_value()
            ? QString::fromStdString(details->inn) : QString();
        saved_kpp_ = details.has_value()
            ? QString::fromStdString(details->kpp) : QString();
        organization_name_edit_->setText(saved_name_);
        organization_inn_edit_->setText(saved_inn_);
        organization_kpp_edit_->setText(saved_kpp_);
        organization_name_edit_->setEnabled(true);
        organization_inn_edit_->setEnabled(true);
        organization_kpp_edit_->setEnabled(true);
        save_organization_button_->setEnabled(true);
        update_reference_access();
        update_employee_access();
        refresh_positions();
        refresh_work_schedules();
        refresh_employees();
        refresh_timesheet();
        update_calculation_access();
        refresh_dashboard();
        refresh_organization_status();
        update_navigation_state();
    } catch (const std::exception& error) {
        const QSignalBlocker blocker(organization_combo_);
        organization_combo_->setCurrentIndex(
            previous_id.has_value()
                ? organization_combo_->findData(
                    QVariant::fromValue(
                        static_cast<qlonglong>(*previous_id)))
                : 0);
        QMessageBox::critical(
            this,
            QStringLiteral("Ошибка чтения организации"),
            QString::fromUtf8(error.what()));
    }
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (confirm_unsaved_changes()) {
        event->accept();
    } else {
        event->ignore();
    }
}

}
