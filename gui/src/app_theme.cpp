#include "app_theme.hpp"

#include <QApplication>
#include <QString>

namespace mk::gui {

void apply_app_theme() {
    qApp->setStyleSheet(QStringLiteral(R"(
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

        QTableView {
            background: #FFFFFF;
            alternate-background-color: #F8FAFC;
            color: #202938;
            gridline-color: #CBD5E1;
            border: 1px solid #CBD5E1;
            border-radius: 4px;
            selection-background-color: #DBEAFE;
            selection-color: #202938;
        }

        QTableView::item:hover {
            background: #EFF6FF;
        }

        QTableView::item:selected {
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

        QFrame#navigationSidebar {
            background: #172033;
        }

        QFrame#logoBar {
            background: #172033;
            border-bottom: 1px solid #2B374D;
        }

        QLabel#logoBadge {
            background: #2563EB;
            color: #FFFFFF;
            border-radius: 7px;
            font-weight: bold;
            font-size: 16px;
        }

        QLabel#logoText {
            color: #FFFFFF;
            font-size: 16px;
            font-weight: 600;
        }

        QLabel#menuTitle {
            color: #7F8BA3;
            font-size: 11px;
        }

        QPushButton#sidebarItem {
            background: transparent;
            color: #CBD5E1;
            border: none;
            border-radius: 6px;
            padding: 10px 12px;
            text-align: left;
            font-size: 14px;
        }

        QPushButton#sidebarItem:hover {
            background: #263650;
            color: #FFFFFF;
            border-color: transparent;
        }

        QPushButton#sidebarItem:checked {
            background: #263650;
            color: #FFFFFF;
            border: none;
            border-left: 3px solid #2563EB;
            border-top-left-radius: 0;
            border-bottom-left-radius: 0;
        }

        QPushButton#sidebarItem:disabled {
            background: transparent;
            color: #64748B;
        }

        QPushButton#sidebarItem:checked:disabled {
            background: #263650;
            color: #64748B;
            border: none;
            border-left: 3px solid #2563EB;
            border-top-left-radius: 0;
            border-bottom-left-radius: 0;
        }

        QFrame#sidebarBottom {
            background: #172033;
            border-top: 1px solid #2B374D;
        }

        QLabel#sidebarStatus {
            color: #94A3B8;
            font-size: 12px;
        }

        QFrame#tabsBar {
            background: #E8EDF3;
            border-bottom: 1px solid #CBD5E1;
        }

        QScrollArea#tabsScroll {
            background: transparent;
            border: none;
        }

        QScrollArea#tabsScroll > QWidget > QWidget {
            background: transparent;
        }

        QScrollArea#tabsScroll QScrollBar:horizontal {
            height: 8px;
            background: transparent;
            border: none;
        }

        QScrollArea#tabsScroll QScrollBar::handle:horizontal {
            background: #B8C4D4;
            border-radius: 4px;
            min-width: 30px;
        }

        QScrollArea#tabsScroll QScrollBar::handle:horizontal:hover {
            background: #94A3B8;
        }

        QScrollArea#tabsScroll QScrollBar::add-line:horizontal,
        QScrollArea#tabsScroll QScrollBar::sub-line:horizontal {
            width: 0;
            height: 0;
            border: none;
            background: transparent;
        }

        QScrollArea#tabsScroll QScrollBar::left-arrow:horizontal,
        QScrollArea#tabsScroll QScrollBar::right-arrow:horizontal {
            width: 0;
            height: 0;
            background: transparent;
        }

        QScrollArea#tabsScroll QScrollBar::add-page:horizontal,
        QScrollArea#tabsScroll QScrollBar::sub-page:horizontal {
            background: transparent;
        }

        QFrame#workspaceTab {
            background: #DCE3EB;
            border: 1px solid #D4DCE6;
            border-bottom: none;
            border-top-left-radius: 7px;
            border-top-right-radius: 7px;
        }

        QFrame#workspaceTab[hovered="true"] {
            background: #F5F8FB;
        }

        QFrame#workspaceTab[active="true"] {
            background: #FFFFFF;
            border-top: 3px solid #2563EB;
            border-left: 1px solid #D4DCE6;
            border-right: 1px solid #D4DCE6;
            border-bottom: 1px solid #F3F5F7;
        }

        QLabel#workspaceTabTitle {
            color: #64748B;
        }

        QFrame#workspaceTab[active="true"] QLabel#workspaceTabTitle {
            color: #2563EB;
            font-weight: 600;
        }

        QPushButton#tabCloseButton {
            background: transparent;
            border: none;
            color: #94A3B8;
            font-size: 15px;
            padding: 0;
        }

        QPushButton#tabCloseButton:hover {
            background: transparent;
            border-color: transparent;
            color: #DC2626;
        }

        QStackedWidget#workspaceStack {
            background: #F3F5F7;
        }

        QWidget#dashboard_page,
        QWidget#organization_tab,
        QWidget#references_tab,
        QWidget#employees_tab,
        QWidget#time_tab,
        QWidget#calculation_tab {
            background: #F3F5F7;
        }

        QLabel#pageTitle {
            color: #202938;
            font-size: 20px;
            font-weight: 600;
        }

        QLabel#pageSubtitle {
            color: #64748B;
        }

        QLabel#pageStatus {
            color: #64748B;
        }

        QFrame#infoCard {
            background: #FFFFFF;
            border: 1px solid #CBD5E1;
            border-radius: 8px;
        }

        QLabel#infoText {
            color: #64748B;
        }

        QLabel#infoValue {
            color: #202938;
        }

        QFrame#subsectionBar {
            background: transparent;
            border-bottom: 1px solid #CBD5E1;
        }

        QPushButton#subsectionButton {
            background: transparent;
            border: none;
            border-bottom: 2px solid transparent;
            color: #64748B;
            padding: 8px 12px;
        }

        QPushButton#subsectionButton:hover {
            background: #EFF6FF;
            border-color: transparent;
            border-bottom: 2px solid transparent;
            border-radius: 4px;
            color: #1D4ED8;
        }

        QPushButton#subsectionButton:checked {
            background: transparent;
            border: none;
            border-bottom: 2px solid #2563EB;
            color: #2563EB;
            font-weight: 600;
        }

        QPushButton#primaryButton {
            background: #2563EB;
            color: #FFFFFF;
            border: 1px solid #2563EB;
        }

        QPushButton#primaryButton:hover {
            background: #1D4ED8;
            border-color: #1D4ED8;
            color: #FFFFFF;
        }

        QPushButton#primaryButton:pressed {
            background: #1E40AF;
            border-color: #1E40AF;
        }

        QPushButton#primaryButton:disabled {
            background: #E2E8F0;
            color: #64748B;
            border-color: #CBD5E1;
        }
    )"));
}

}
