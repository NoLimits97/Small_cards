#include "main_window.hpp"

#include <QTabWidget>
#include <QWidget>

namespace mk::gui {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("Малые кадры"));
    resize(1000, 700);

    auto* tabs = new QTabWidget(this);
    tabs->addTab(new QWidget(tabs), QStringLiteral("Организация"));
    tabs->addTab(new QWidget(tabs), QStringLiteral("Справочники"));
    tabs->addTab(new QWidget(tabs), QStringLiteral("Сотрудники"));
    tabs->addTab(new QWidget(tabs), QStringLiteral("Табель"));
    tabs->addTab(new QWidget(tabs), QStringLiteral("Расчёт"));
    setCentralWidget(tabs);
}

}
