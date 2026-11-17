#ifndef MK_GUI_MAIN_WINDOW_HPP
#define MK_GUI_MAIN_WINDOW_HPP

#include "mk/database.hpp"
#include "mk/payroll_service.hpp"

#include <QMainWindow>
#include <QString>

#include <cstdint>
#include <optional>

class QCloseEvent;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSortFilterProxyModel;
class QSpinBox;
class QTableView;
class QTableWidget;

namespace mk::gui {

class EmployeeTableModel;
class NavigationSidebar;
class PayrollTableModel;
class TimesheetTableModel;
class WorkspaceTabs;

enum class SectionId : int {
    Dashboard = 0,
    Organization,
    References,
    Employees,
    Timesheet,
    Calculation
};

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

    QWidget* create_dashboard_page();
    QWidget* create_organization_page();
    QWidget* create_organization_details_page();
    QWidget* create_database_settings_page();
    QWidget* create_references_page();
    QWidget* create_employees_page();
    QWidget* create_timesheet_page();
    QWidget* create_calculation_page();
    QWidget* create_placeholder_content(
        const QString& title,
        const QString& subtitle,
        const QString& message);

    void open_section(SectionId section);
    void update_navigation_state();
    void refresh_dashboard();
    void refresh_organization_status();

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
    void open_employee_card();
    void select_employee_row(int64_t employee_id);
    std::optional<int64_t> selected_employee_id() const;

    void update_timesheet_access();
    void refresh_timesheet();
    bool save_timesheet();
    void configure_timesheet_columns();

    void update_calculation_access();
    void refresh_calculation_parameters();
    void run_calculation();
    void save_calculation_result();
    void clear_calculation_results(const QString& message);

    Database database_;
    bool database_open_ = false;
    std::optional<int64_t> selected_org_id_;
    QString database_path_;
    QString saved_name_;
    QString saved_inn_;
    QString saved_kpp_;

    NavigationSidebar* sidebar_ = nullptr;
    WorkspaceTabs* workspace_tabs_ = nullptr;

    QWidget* dashboard_page_ = nullptr;
    QWidget* organization_page_ = nullptr;
    QWidget* references_page_ = nullptr;
    QWidget* employees_page_ = nullptr;
    QWidget* timesheet_page_ = nullptr;
    QWidget* calculation_page_ = nullptr;

    QLabel* dashboard_database_label_ = nullptr;
    QLabel* dashboard_organization_label_ = nullptr;

    QLabel* organization_status_label_ = nullptr;
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
    QTableView* employees_table_ = nullptr;
    EmployeeTableModel* employees_model_ = nullptr;
    QSortFilterProxyModel* employees_proxy_ = nullptr;
    QPushButton* add_employee_button_ = nullptr;
    QPushButton* employee_card_button_ = nullptr;
    QPushButton* refresh_employees_button_ = nullptr;

    QLabel* timesheet_status_label_ = nullptr;
    QTableView* timesheet_table_ = nullptr;
    TimesheetTableModel* timesheet_model_ = nullptr;
    QComboBox* timesheet_month_combo_ = nullptr;
    QSpinBox* timesheet_year_spin_ = nullptr;
    QPushButton* timesheet_build_button_ = nullptr;
    QPushButton* timesheet_save_button_ = nullptr;

    QLabel* calculation_org_label_ = nullptr;
    QLabel* calculation_norm_label_ = nullptr;
    QLabel* calculation_timesheet_label_ = nullptr;
    QLabel* calculation_settings_label_ = nullptr;
    QLabel* calculation_status_label_ = nullptr;
    QLabel* calculation_results_status_label_ = nullptr;
    QComboBox* calculation_month_combo_ = nullptr;
    QSpinBox* calculation_year_spin_ = nullptr;
    QPushButton* calculation_run_button_ = nullptr;
    QPushButton* calculation_refresh_button_ = nullptr;
    QPushButton* calculation_save_button_ = nullptr;
    QTableView* calculation_table_ = nullptr;
    PayrollTableModel* calculation_model_ = nullptr;
    QSortFilterProxyModel* calculation_proxy_ = nullptr;
    std::optional<PayrollReport> payroll_report_;
};

}

#endif
