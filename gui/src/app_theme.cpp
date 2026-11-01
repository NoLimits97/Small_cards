#include "app_theme.hpp"

#include <QApplication>
#include <QString>

namespace mk::gui {

void apply_app_theme() {
    QApplication::setStyleSheet(QStringLiteral(R"(
        QMainWindow {
            background: #F3F5F7;
            color: #202938;
        }

        QLineEdit,
        QComboBox,
        QDateEdit,
        QSpinBox {
            background: #FFFFFF;
            color: #202938;
            border: 1px solid #CBD5E1;
            border-radius: 4px;
            padding: 5px 8px;
            selection-background-color: #2563EB;
            selection-color: #FFFFFF;
        }

        QLineEdit:hover,
        QComboBox:hover,
        QDateEdit:hover,
        QSpinBox:hover {
            border-color: #94A3B8;
        }

        QLineEdit:focus,
        QComboBox:focus,
        QDateEdit:focus,
        QSpinBox:focus {
            border: 1px solid #2563EB;
        }

        QLineEdit:disabled,
        QComboBox:disabled,
        QDateEdit:disabled,
        QSpinBox:disabled {
            background: #E2E8F0;
            color: #64748B;
            border-color: #CBD5E1;
        }

        QPushButton {
            background: #FFFFFF;
            color: #202938;
            border: 1px solid #CBD5E1;
            border-radius: 4px;
            padding: 6px 12px;
        }

        QPushButton:hover {
            background: #EFF6FF;
            border-color: #2563EB;
            color: #1D4ED8;
        }

        QPushButton:pressed {
            background: #DBEAFE;
            border-color: #1D4ED8;
        }

        QPushButton:focus {
            border: 1px solid #2563EB;
        }

        QPushButton:disabled {
            background: #E2E8F0;
            color: #64748B;
            border-color: #CBD5E1;
        }

        QTabWidget::pane {
            background: #F3F5F7;
            border: 1px solid #CBD5E1;
            border-radius: 4px;
            top: -1px;
        }

        QTabBar::tab {
            background: #E2E8F0;
            color: #475569;
            border: 1px solid #CBD5E1;
            padding: 7px 14px;
            margin-right: 2px;
        }

        QTabBar::tab:hover {
            background: #EFF6FF;
            color: #1D4ED8;
        }

        QTabBar::tab:selected {
            background: #FFFFFF;
            color: #1D4ED8;
            border-top: 2px solid #2563EB;
            border-bottom-color: #FFFFFF;
        }

        QTabBar::tab:pressed {
            background: #DBEAFE;
        }

        QGroupBox {
            background: #F3F5F7;
            color: #202938;
            border: 1px solid #CBD5E1;
            border-radius: 4px;
            margin-top: 10px;
            padding-top: 8px;
        }

        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 4px;
            color: #202938;
            background: #F3F5F7;
        }

        QTableWidget {
            background: #FFFFFF;
            alternate-background-color: #F8FAFC;
            color: #202938;
            gridline-color: #CBD5E1;
            border: 1px solid #CBD5E1;
            border-radius: 4px;
            selection-background-color: #DBEAFE;
            selection-color: #202938;
        }

        QTableWidget::item:hover {
            background: #EFF6FF;
        }

        QTableWidget::item:selected {
            background: #DBEAFE;
            color: #202938;
        }

        QHeaderView::section {
            background: #E2E8F0;
            color: #202938;
            border: 0;
            border-right: 1px solid #CBD5E1;
            border-bottom: 1px solid #CBD5E1;
            padding: 6px 8px;
        }

        QScrollBar:vertical,
        QScrollBar:horizontal {
            background: #E2E8F0;
            border: 1px solid #CBD5E1;
        }

        QScrollBar::handle:vertical,
        QScrollBar::handle:horizontal {
            background: #94A3B8;
            border-radius: 4px;
            min-height: 24px;
            min-width: 24px;
        }

        QScrollBar::handle:vertical:hover,
        QScrollBar::handle:horizontal:hover {
            background: #64748B;
        }
    )"));
}

}
