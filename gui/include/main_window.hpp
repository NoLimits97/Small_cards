#ifndef MK_GUI_MAIN_WINDOW_HPP
#define MK_GUI_MAIN_WINDOW_HPP

#include "mk/database.hpp"

#include <QMainWindow>
#include <QString>

#include <cstdint>
#include <optional>

class QCloseEvent;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;

namespace mk::gui {

class MainWindow : public QMainWindow {
public:
    explicit MainWindow(QWidget* parent = nullptr);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    bool confirm_unsaved_changes();
    bool save_organization();
    void create_database();
    void open_database();
    void create_organization();
    void select_organization(int index);
    void reload_organizations(std::optional<int64_t> selected_id = std::nullopt);
    void clear_database_view();

    void update_reference_access();
    void refresh_positions();
    void refresh_work_schedules();
    void add_position();
    void edit_position();
    void remove_position();
    void add_work_schedule();
    void edit_work_schedule();
    void remove_work_schedule();

    void update_employee_access();
    void refresh_employees();
    void add_employee();

    Database database_;
    bool database_open_ = false;
    std::optional<int64_t> selected_org_id_;
    QString saved_name_;
    QString saved_inn_;
    QString saved_kpp_;

    QLabel* database_path_label_ = nullptr;
    QComboBox* organization_combo_ = nullptr;
    QLineEdit* organization_name_edit_ = nullptr;
    QLineEdit* organization_inn_edit_ = nullptr;
    QLineEdit* organization_kpp_edit_ = nullptr;
    QPushButton* create_organization_button_ = nullptr;
    QPushButton* save_organization_button_ = nullptr;

    QLabel* references_status_label_ = nullptr;
    QTableWidget* positions_table_ = nullptr;
    QTableWidget* work_schedules_table_ = nullptr;
    QPushButton* add_position_button_ = nullptr;
    QPushButton* edit_position_button_ = nullptr;
    QPushButton* remove_position_button_ = nullptr;
    QPushButton* refresh_positions_button_ = nullptr;
    QPushButton* add_work_schedule_button_ = nullptr;
    QPushButton* edit_work_schedule_button_ = nullptr;
    QPushButton* remove_work_schedule_button_ = nullptr;
    QPushButton* refresh_work_schedules_button_ = nullptr;

    QLabel* employees_status_label_ = nullptr;
    QTableWidget* employees_table_ = nullptr;
    QPushButton* add_employee_button_ = nullptr;
    QPushButton* refresh_employees_button_ = nullptr;
};

}

#endif
