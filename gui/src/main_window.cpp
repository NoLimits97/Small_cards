#include "main_window.hpp"

#include "gui_format.hpp"
#include "money_formatter.hpp"
#include "mk/org_details.hpp"
#include "mk/repositories.hpp"

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
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSpinBox>
#include <QStyle>
#include <QTabWidget>
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
#include <vector>

namespace mk::gui {
namespace {

bool create_employee_dialog(
    QWidget* parent,
    Database& database,
    int64_t org_id,
    int64_t& created_id) {
    QDialog dialog(parent);
    dialog.setWindowTitle(QStringLiteral("Добавить сотрудника"));
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
            QStringLiteral("Добавить сотрудника"),
            QStringLiteral(
                "Для приёма нужны подразделение, должность, "
                "вид занятости и график работы."));
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
                QStringLiteral("Добавить сотрудника"),
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

        EmploymentEvent hire;
        hire.date = employee.hire_date;
        hire.kind = EventKind::Hire;
        hire.personnel_number = employee.personnel_number;
        hire.full_name = employee.full_name;
        hire.org_id = employee.org_id;
        hire.department_id = employee.department_id;
        hire.position_id = employee.position_id;
        hire.employment_kind_id = employee.employment_kind_id;
        hire.work_schedule_id = employee.work_schedule_id;
        hire.payroll_preset_id = employee.payroll_preset_id;
        hire.rate = employee.rate;
        hire.rate_unit = employee.rate_unit;

        try {
            created_id = create_employee_with_hire_event(
                database, employee, hire);
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

}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("Малые кадры"));
    resize(1000, 700);

    auto* tabs = new QTabWidget(this);
    setCentralWidget(tabs);

    auto* organization_tab = new QWidget(tabs);
    organization_tab->setObjectName(QStringLiteral("organization_tab"));
    organization_tab->setAutoFillBackground(true);
    auto* layout = new QVBoxLayout(organization_tab);

    auto* database_buttons = new QHBoxLayout;
    auto* create_database_button =
        new QPushButton(QStringLiteral("Создать файл БД"), organization_tab);
    auto* open_database_button =
        new QPushButton(QStringLiteral("Открыть файл БД"), organization_tab);
    database_buttons->addWidget(create_database_button);
    database_buttons->addWidget(open_database_button);
    database_buttons->addStretch();
    layout->addLayout(database_buttons);

    database_path_label_ =
        new QLabel(QStringLiteral("Файл БД не открыт"), organization_tab);
    database_path_label_->setObjectName(QStringLiteral("database_path_label"));
    database_path_label_->setWordWrap(true);
    database_path_label_->setSizePolicy(
        QSizePolicy::Ignored, QSizePolicy::Preferred);
    layout->addWidget(database_path_label_);

    auto* form = new QFormLayout;
    organization_combo_ = new QComboBox(organization_tab);
    organization_name_edit_ = new QLineEdit(organization_tab);
    organization_name_edit_->setObjectName(
        QStringLiteral("organization_name_edit"));
    organization_inn_edit_ = new QLineEdit(organization_tab);
    organization_kpp_edit_ = new QLineEdit(organization_tab);
    organization_combo_->setMinimumWidth(0);
    organization_name_edit_->setMinimumWidth(0);
    organization_inn_edit_->setMinimumWidth(0);
    organization_kpp_edit_->setMinimumWidth(0);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->addRow(QStringLiteral("Организация:"), organization_combo_);
    form->addRow(QStringLiteral("Название:"), organization_name_edit_);
    form->addRow(QStringLiteral("ИНН:"), organization_inn_edit_);
    form->addRow(QStringLiteral("КПП:"), organization_kpp_edit_);
    layout->addLayout(form);

    auto* organization_buttons = new QHBoxLayout;
    create_organization_button_ =
        new QPushButton(QStringLiteral("Создать организацию"), organization_tab);
    save_organization_button_ =
        new QPushButton(QStringLiteral("Сохранить организацию"), organization_tab);
    organization_buttons->addWidget(create_organization_button_);
    organization_buttons->addWidget(save_organization_button_);
    organization_buttons->addStretch();
    layout->addLayout(organization_buttons);
    layout->addStretch();

    auto* references_tab = new QWidget(tabs);
    references_tab->setObjectName(QStringLiteral("references_tab"));
    references_tab->setAutoFillBackground(true);
    auto* references_outer_layout = new QVBoxLayout(references_tab);
    auto* references_scroll = new QScrollArea(references_tab);
    references_scroll->setWidgetResizable(true);
    references_scroll->setFrameShape(QFrame::NoFrame);
    references_outer_layout->addWidget(references_scroll);

    auto* references_content = new QWidget(references_scroll);
    auto* references_layout = new QVBoxLayout(references_content);

    references_status_label_ = new QLabel(references_content);
    references_layout->addWidget(references_status_label_);

    auto* positions_group =
        new QGroupBox(QStringLiteral("Должности"), references_content);
    auto* positions_layout = new QVBoxLayout(positions_group);
    positions_table_ = new QTableWidget(positions_group);
    setup_table(
        positions_table_,
        {QStringLiteral("ID"), QStringLiteral("Название")});
    setup_columns(positions_table_, 1);
    positions_layout->addWidget(positions_table_);
    auto* positions_buttons = new QHBoxLayout;
    add_position_button_ =
        new QPushButton(QStringLiteral("Добавить"), positions_group);
    edit_position_button_ =
        new QPushButton(QStringLiteral("Изменить"), positions_group);
    remove_position_button_ =
        new QPushButton(QStringLiteral("Удалить"), positions_group);
    refresh_positions_button_ =
        new QPushButton(QStringLiteral("Обновить"), positions_group);
    positions_buttons->addWidget(add_position_button_);
    positions_buttons->addWidget(edit_position_button_);
    positions_buttons->addWidget(remove_position_button_);
    positions_buttons->addWidget(refresh_positions_button_);
    positions_buttons->addStretch();
    positions_layout->addLayout(positions_buttons);
    references_layout->addWidget(positions_group);

    auto* schedules_group =
        new QGroupBox(QStringLiteral("Графики работы"), references_content);
    auto* schedules_layout = new QVBoxLayout(schedules_group);
    work_schedules_table_ = new QTableWidget(schedules_group);
    setup_table(
        work_schedules_table_,
        {QStringLiteral("ID"),
         QStringLiteral("Название"),
         QStringLiteral("Тип"),
         QStringLiteral("Минут в день"),
         QStringLiteral("Цикл"),
         QStringLiteral("Опорная дата")});
    setup_columns(work_schedules_table_, 1);
    schedules_layout->addWidget(work_schedules_table_);
    auto* schedules_buttons = new QHBoxLayout;
    add_work_schedule_button_ =
        new QPushButton(QStringLiteral("Добавить"), schedules_group);
    edit_work_schedule_button_ =
        new QPushButton(QStringLiteral("Изменить"), schedules_group);
    remove_work_schedule_button_ =
        new QPushButton(QStringLiteral("Удалить"), schedules_group);
    refresh_work_schedules_button_ =
        new QPushButton(QStringLiteral("Обновить"), schedules_group);
    schedules_buttons->addWidget(add_work_schedule_button_);
    schedules_buttons->addWidget(edit_work_schedule_button_);
    schedules_buttons->addWidget(remove_work_schedule_button_);
    schedules_buttons->addWidget(refresh_work_schedules_button_);
    schedules_buttons->addStretch();
    schedules_layout->addLayout(schedules_buttons);
    references_layout->addWidget(schedules_group);
    references_scroll->setWidget(references_content);

    auto* employees_tab = new QWidget(tabs);
    employees_tab->setObjectName(QStringLiteral("employees_tab"));
    employees_tab->setAutoFillBackground(true);
    auto* employees_layout = new QVBoxLayout(employees_tab);
    employees_status_label_ = new QLabel(employees_tab);
    employees_layout->addWidget(employees_status_label_);
    employees_table_ = new QTableWidget(employees_tab);
    setup_table(
        employees_table_,
        {QStringLiteral("Табельный номер"),
         QStringLiteral("ФИО"),
         QStringLiteral("Статус")});
    employees_table_->setObjectName(QStringLiteral("employees_table"));
    employees_table_->setSortingEnabled(true);
    setup_columns(employees_table_, 1, true);
    employees_layout->addWidget(employees_table_);
    auto* employees_buttons = new QHBoxLayout;
    add_employee_button_ =
        new QPushButton(QStringLiteral("Добавить"), employees_tab);
    employee_card_button_ =
        new QPushButton(QStringLiteral("Карточка"), employees_tab);
    refresh_employees_button_ =
        new QPushButton(QStringLiteral("Обновить"), employees_tab);
    employees_buttons->addWidget(add_employee_button_);
    employees_buttons->addWidget(employee_card_button_);
    employees_buttons->addWidget(refresh_employees_button_);
    employees_buttons->addStretch();
    employees_layout->addLayout(employees_buttons);

    auto* time_tab = new QWidget(tabs);
    time_tab->setObjectName(QStringLiteral("time_tab"));
    time_tab->setAutoFillBackground(true);
    new QVBoxLayout(time_tab);

    auto* calculation_tab = new QWidget(tabs);
    calculation_tab->setObjectName(QStringLiteral("calculation_tab"));
    calculation_tab->setAutoFillBackground(true);
    new QVBoxLayout(calculation_tab);

    tabs->addTab(organization_tab, QStringLiteral("Организация"));
    tabs->addTab(references_tab, QStringLiteral("Справочники"));
    tabs->addTab(employees_tab, QStringLiteral("Сотрудники"));
    tabs->addTab(time_tab, QStringLiteral("Табель"));
    tabs->addTab(calculation_tab, QStringLiteral("Расчёт"));

    clear_database_view();

    connect(create_database_button, &QPushButton::clicked,
            this, [this]() { create_database(); });
    connect(open_database_button, &QPushButton::clicked,
            this, [this]() { open_database(); });
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
    connect(employees_table_, &QTableWidget::doubleClicked,
            this, [this](const QModelIndex&) { open_employee_card(); });
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
        employees_table_->setRowCount(0);
        employees_table_->clearSelection();
        employees_table_->setCurrentCell(-1, -1);
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

    employees_table_->setSortingEnabled(false);
    employees_table_->setRowCount(0);
    employees_table_->clearSelection();
    employees_table_->setCurrentCell(-1, -1);

    try {
        const std::vector<Employee> employees =
            list_employees(database_, *selected_org_id_);

        for (const Employee& employee : employees) {
            const int row = employees_table_->rowCount();
            employees_table_->insertRow(row);
            auto* number = new QTableWidgetItem(
                QString::fromStdString(employee.personnel_number));
            number->setData(
                Qt::UserRole,
                QVariant::fromValue(static_cast<qlonglong>(employee.id)));
            employees_table_->setItem(row, 0, number);
            employees_table_->setItem(
                row, 1,
                new QTableWidgetItem(
                    QString::fromStdString(employee.full_name)));
            employees_table_->setItem(
                row, 2,
                new QTableWidgetItem(
                    employee_status_text(employee)));
        }

        employees_table_->setSortingEnabled(true);
        employees_table_->clearSelection();
        employees_table_->setCurrentCell(-1, -1);
        if (employees.empty()) {
            employees_status_label_->setText(
                QStringLiteral("В выбранной организации нет сотрудников"));
            employees_status_label_->show();
        } else {
            employees_status_label_->hide();
        }
    } catch (const std::exception& error) {
        employees_table_->setRowCount(0);
        employees_table_->clearSelection();
        employees_table_->setCurrentCell(-1, -1);
        employees_table_->setSortingEnabled(true);
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
    const auto id = selected_id(employees_table_);
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
    for (int row = 0; row < employees_table_->rowCount(); ++row) {
        const QTableWidgetItem* item = employees_table_->item(row, 0);
        if (item != nullptr &&
            item->data(Qt::UserRole).toLongLong() == employee_id) {
            employees_table_->selectRow(row);
            employees_table_->scrollToItem(item);
            break;
        }
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
        reload_organizations();
        database_path_label_->setText(
            QStringLiteral("Файл БД: %1").arg(path));
        database_path_label_->setToolTip(path);
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
        reload_organizations();
        database_path_label_->setText(
            QStringLiteral("Файл БД: %1").arg(path));
        database_path_label_->setToolTip(path);
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
