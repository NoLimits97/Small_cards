#ifndef MK_GUI_WORKSPACE_TABS_HPP
#define MK_GUI_WORKSPACE_TABS_HPP

#include <QHash>
#include <QList>
#include <QString>
#include <QWidget>

#include <functional>

class QFrame;
class QHBoxLayout;
class QScrollArea;
class QStackedWidget;

namespace mk::gui {

class TabButton;

class WorkspaceTabs : public QWidget {
public:
    explicit WorkspaceTabs(QWidget* parent = nullptr);

    void add_page(int section_id, QWidget* page);
    void open_section(int section_id, const QString& title, bool closable);
    void close_section(int section_id);
    void activate_section(int section_id);

    bool is_open(int section_id) const;
    int current_section() const;

    void set_current_changed_handler(std::function<void(int)> handler);
    void set_can_close_handler(std::function<bool(int)> handler);

private:
    QHash<int, QWidget*> pages_;
    QHash<int, TabButton*> buttons_;
    QHash<int, QString> titles_;
    QHash<int, bool> closable_;
    QList<int> order_;

    QStackedWidget* stack_ = nullptr;
    QFrame* bar_ = nullptr;
    QScrollArea* scroll_ = nullptr;
    QWidget* tab_container_ = nullptr;
    QHBoxLayout* tab_layout_ = nullptr;
    int current_ = -1;

    std::function<void(int)> current_changed_;
    std::function<bool(int)> can_close_;
};

}

#endif
