#include "main_window.hpp"

#include <QApplication>

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    QApplication::setStyle(QStringLiteral("Fusion"));
    mk::gui::MainWindow window;
    window.show();
    return application.exec();
}
