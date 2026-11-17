#ifndef MK_GUI_SECTION_PAGE_HPP
#define MK_GUI_SECTION_PAGE_HPP

#include <QWidget>

class QButtonGroup;
class QHBoxLayout;
class QStackedWidget;

namespace mk::gui {

class SectionPage : public QWidget {
public:
    explicit SectionPage(QWidget* parent = nullptr);

    void add_page(const QString& title, QWidget* page);

private:
    QWidget* nav_bar_ = nullptr;
    QHBoxLayout* nav_layout_ = nullptr;
    QStackedWidget* stack_ = nullptr;
    QButtonGroup* group_ = nullptr;
    int page_count_ = 0;
};

}

#endif
