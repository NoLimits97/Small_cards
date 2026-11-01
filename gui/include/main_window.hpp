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

    Database database_;
    bool database_open_ = false;
    std::optional<int64_t> selected_org_id_;
    QString saved_name_;

    QLabel* database_path_label_ = nullptr;
    QComboBox* organization_combo_ = nullptr;
    QLineEdit* organization_name_edit_ = nullptr;
    QPushButton* create_organization_button_ = nullptr;
    QPushButton* save_organization_button_ = nullptr;
};

}

#endif
