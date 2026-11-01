#include "main_window.hpp"

#include <QApplication>

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    mk::gui::MainWindow window;
    window.show();
    return application.exec();
}
