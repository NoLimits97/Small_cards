#ifndef MK_GUI_MAIN_WINDOW_HPP
#define MK_GUI_MAIN_WINDOW_HPP

#include <QMainWindow>

namespace mk::gui {

class MainWindow : public QMainWindow {
public:
    explicit MainWindow(QWidget* parent = nullptr);
};

}

#endif
