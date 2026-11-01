#include "main_window.hpp"

#include "mk/repositories.hpp"

#include <QCloseEvent>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTabWidget>
#include <QVariant>
#include <QVBoxLayout>
#include <QWidget>

#include <exception>
#include <stdexcept>

namespace mk::gui {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("Малые кадры"));
    resize(1000, 700);

    auto* tabs = new QTabWidget(this);
    auto* organization_tab = new QWidget(tabs);
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
    layout->addWidget(database_path_label_);

    auto* form = new QFormLayout;
    organization_combo_ = new QComboBox(organization_tab);
    organization_name_edit_ = new QLineEdit(organization_tab);
    form->addRow(QStringLiteral("Организация:"), organization_combo_);
    form->addRow(QStringLiteral("Название:"), organization_name_edit_);
    layout->addLayout(form);

    auto* organization_buttons = new QHBoxLayout;
    create_organization_button_ =
        new QPushButton(QStringLiteral("Создать организацию"), organization_tab);
    save_organization_button_ =
        new QPushButton(QStringLiteral("Сохранить название"), organization_tab);
    organization_buttons->addWidget(create_organization_button_);
    organization_buttons->addWidget(save_organization_button_);
    organization_buttons->addStretch();
    layout->addLayout(organization_buttons);
    layout->addStretch();

    tabs->addTab(organization_tab, QStringLiteral("Организация"));
    tabs->addTab(new QWidget(tabs), QStringLiteral("Справочники"));
    tabs->addTab(new QWidget(tabs), QStringLiteral("Сотрудники"));
    tabs->addTab(new QWidget(tabs), QStringLiteral("Табель"));
    tabs->addTab(new QWidget(tabs), QStringLiteral("Расчёт"));
    setCentralWidget(tabs);

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
}

void MainWindow::clear_database_view() {
    database_open_ = false;
    selected_org_id_.reset();
    saved_name_.clear();

    const QSignalBlocker blocker(organization_combo_);
    organization_combo_->clear();
    organization_combo_->addItem(QStringLiteral("Выберите организацию"));
    organization_combo_->setEnabled(false);
    organization_name_edit_->clear();
    organization_name_edit_->setEnabled(false);
    create_organization_button_->setEnabled(false);
    save_organization_button_->setEnabled(false);
    database_path_label_->setText(QStringLiteral("Файл БД не открыт"));
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
    organization_name_edit_->clear();
    organization_name_edit_->setEnabled(false);
    save_organization_button_->setEnabled(false);
    organization_combo_->setEnabled(true);
    create_organization_button_->setEnabled(true);

    if (selected_id.has_value()) {
        const int index = organization_combo_->findData(
            QVariant::fromValue(static_cast<qlonglong>(*selected_id)));
        if (index > 0) {
            organization_combo_->setCurrentIndex(index);
            selected_org_id_ = selected_id;
            const auto organization = read_org(database_, *selected_id);
            if (!organization.has_value()) {
                throw std::runtime_error("organization not found");
            }
            saved_name_ = QString::fromStdString(organization->name);
            organization_name_edit_->setText(saved_name_);
            organization_name_edit_->setEnabled(true);
            save_organization_button_->setEnabled(true);
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

    try {
        const auto organization = read_org(database_, *selected_org_id_);
        if (!organization.has_value()) {
            throw std::runtime_error("organization not found");
        }

        Org updated = *organization;
        updated.name = name.toStdString();
        update_org(database_, updated);

        saved_name_ = name;
        organization_name_edit_->setText(name);
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
        organization_name_edit_->text() == saved_name_) {
        return true;
    }

    const auto answer = QMessageBox::warning(
        this,
        QStringLiteral("Несохранённые изменения"),
        QStringLiteral("Сохранить изменения названия организации?"),
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
        const int64_t id = create_org(database_, {0, name.toStdString()});
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
        organization_name_edit_->clear();
        organization_name_edit_->setEnabled(false);
        save_organization_button_->setEnabled(false);
        return;
    }

    const int64_t id = organization_combo_->itemData(index).toLongLong();
    try {
        const auto organization = read_org(database_, id);
        if (!organization.has_value()) {
            throw std::runtime_error("organization not found");
        }

        selected_org_id_ = id;
        saved_name_ = QString::fromStdString(organization->name);
        organization_name_edit_->setText(saved_name_);
        organization_name_edit_->setEnabled(true);
        save_organization_button_->setEnabled(true);
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
