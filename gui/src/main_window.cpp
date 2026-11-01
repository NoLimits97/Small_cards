#include "main_window.hpp"

#include "mk/org_details.hpp"
#include "mk/repositories.hpp"

#include <QAbstractItemView>
#include <QCloseEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QDate>
#include <QDateEdit>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
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
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextStream>
#include <QTimer>
#include <QVariant>
#include <QVBoxLayout>
#include <QWidget>

#include <exception>
#include <stdexcept>
#include <vector>

namespace mk::gui {
namespace {

QFile& diagnostic_file() {
    static QFile file(
        QCoreApplication::applicationDirPath() +
        QStringLiteral("/m7_gui_debug.log"));
    return file;
}

void flush_diagnostic() {
    if (!diagnostic_file().flush()) {
        throw std::runtime_error(
            "cannot write m7_gui_debug.log: " +
            diagnostic_file().errorString().toStdString());
    }
}

void start_diagnostic() {
    QFile& file = diagnostic_file();
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        throw std::runtime_error(
            "cannot open m7_gui_debug.log: " +
            file.errorString().toStdString());
    }

    QTextStream stream(&file);
    stream << "applicationFilePath="
           << QCoreApplication::applicationFilePath() << '\n';
    stream << "started="
           << QDateTime::currentDateTime().toString(Qt::ISODateWithMs)
           << '\n';
    flush_diagnostic();
}

QString widget_address(const QWidget* widget) {
    if (widget == nullptr) {
        return QStringLiteral("null");
    }
    return QStringLiteral("0x%1").arg(
        QString::number(reinterpret_cast<quintptr>(widget), 16));
}

void log_widget(QTextStream& stream, const QString& label, QWidget* widget) {
    stream << label;
    if (widget == nullptr) {
        stream << " = null\n";
        return;
    }

    QWidget* parent = widget->parentWidget();
    const QRect rect = widget->geometry();
    stream << " objectName=" << widget->objectName()
           << " address=" << widget_address(widget)
           << " isVisible=" << widget->isVisible()
           << " isHidden=" << widget->isHidden()
           << " geometry=(" << rect.x() << ',' << rect.y()
           << ',' << rect.width() << ',' << rect.height() << ')'
           << " parentWidget=" << widget_address(parent)
           << " parentObjectName="
           << (parent != nullptr ? parent->objectName()
                                 : QStringLiteral("null"))
           << '\n';
}

void log_tab_state(
    QTabWidget* tabs,
    QWidget* organization_name,
    QWidget* database_path,
    const QString& phase) {
    QFile& file = diagnostic_file();
    QTextStream stream(&file);
    QWidget* current = tabs->currentWidget();

    stream << "phase=" << phase
           << " time="
           << QDateTime::currentDateTime().toString(Qt::ISODateWithMs)
           << " currentIndex=" << tabs->currentIndex()
           << " currentObjectName="
           << (current != nullptr ? current->objectName()
                                  : QStringLiteral("null"))
           << '\n';

    for (int index = 0; index < tabs->count(); ++index) {
        log_widget(
            stream,
            QStringLiteral("page[%1]").arg(index),
            tabs->widget(index));
    }
    log_widget(stream, QStringLiteral("organization_name"), organization_name);
    log_widget(stream, QStringLiteral("database_path"), database_path);
    stream << '\n';
    flush_diagnostic();
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
    table->horizontalHeader()->setStretchLastSection(true);
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
    start_diagnostic();

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
    layout->addWidget(database_path_label_);

    auto* form = new QFormLayout;
    organization_combo_ = new QComboBox(organization_tab);
    organization_name_edit_ = new QLineEdit(organization_tab);
    organization_name_edit_->setObjectName(
        QStringLiteral("organization_name_edit"));
    organization_inn_edit_ = new QLineEdit(organization_tab);
    organization_kpp_edit_ = new QLineEdit(organization_tab);
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
    auto* references_layout = new QVBoxLayout(references_tab);

    auto* positions_group =
        new QGroupBox(QStringLiteral("Должности"), references_tab);
    auto* positions_layout = new QVBoxLayout(positions_group);
    positions_table_ = new QTableWidget(positions_group);
    setup_table(
        positions_table_,
        {QStringLiteral("ID"), QStringLiteral("Название")});
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
        new QGroupBox(QStringLiteral("Графики работы"), references_tab);
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

    auto* employees_tab = new QWidget(tabs);
    employees_tab->setObjectName(QStringLiteral("employees_tab"));
    employees_tab->setAutoFillBackground(true);
    new QVBoxLayout(employees_tab);

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

    connect(tabs, &QTabWidget::currentChanged, this,
            [this, tabs](int) {
                log_tab_state(
                    tabs,
                    organization_name_edit_,
                    database_path_label_,
                    QStringLiteral("currentChanged"));
                QTimer::singleShot(0, this, [this, tabs]() {
                    log_tab_state(
                        tabs,
                        organization_name_edit_,
                        database_path_label_,
                        QStringLiteral("singleShot(0)"));
                });
            });

    connect(tabs, &QTabWidget::currentChanged, this, [this, tabs](int index) {
        if (index == 1 && !selected_org_id_.has_value()) {
            QMessageBox::information(
                this,
                QStringLiteral("Справочники"),
                QStringLiteral(
                    "Сначала откройте базу данных и выберите "
                    "рабочую организацию на вкладке «Организация»."));
            tabs->setCurrentIndex(0);
        }
    });

    QTimer::singleShot(0, this, [this, tabs]() {
        log_tab_state(
            tabs,
            organization_name_edit_,
            database_path_label_,
            QStringLiteral("startup"));
    });
}

void MainWindow::update_reference_access() {
    const bool enabled = database_open_ && selected_org_id_.has_value();
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
    update_reference_access();
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
            refresh_positions();
            refresh_work_schedules();
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
        refresh_positions();
        refresh_work_schedules();
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
