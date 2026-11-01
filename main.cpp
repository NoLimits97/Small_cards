#include "app_theme.hpp"
#include "main_window.hpp"

#include <QApplication>
#include <QCoreApplication>
#include <QStyle>

#include <string_view>

int main(int argc, char* argv[]) {
    bool style_specified = false;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument(argv[index]);
        if (argument == "-style" || argument.starts_with("-style=")) {
            style_specified = true;
            break;
        }
    }

    QApplication application(argc, argv);
    if (!style_specified) {
        QApplication::setStyle(QStringLiteral("Fusion"));
    }

    mk::gui::apply_app_theme();

    mk::gui::MainWindow window;
    window.setWindowTitle(
        window.windowTitle() +
        QStringLiteral(" | style=%1 | Qt=%2")
            .arg(QApplication::style()->objectName(), qVersion()));
    window.setToolTip(QCoreApplication::applicationFilePath());
    window.show();
    return application.exec();
}
