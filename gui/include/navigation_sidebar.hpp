#ifndef MK_GUI_NAVIGATION_SIDEBAR_HPP
#define MK_GUI_NAVIGATION_SIDEBAR_HPP

#include <QFrame>
#include <QHash>
#include <QString>

#include <functional>

class QButtonGroup;
class QLabel;
class QPushButton;
class QVBoxLayout;

namespace mk::gui {

class NavigationSidebar : public QFrame {
public:
    explicit NavigationSidebar(QWidget* parent = nullptr);

    void set_on_section_selected(std::function<void(int)> handler);
    void set_active_section(int section_id);
    void set_section_enabled(int section_id, bool enabled);
    void set_database_status(
        const QString& text,
        const QString& tooltip = QString());

private:
    void add_section(int section_id, const QString& icon, const QString& title);

    QVBoxLayout* menu_layout_ = nullptr;
    QButtonGroup* group_ = nullptr;
    QHash<int, QPushButton*> buttons_;
    QLabel* status_label_ = nullptr;
    std::function<void(int)> on_section_selected_;
};

}

#endif
