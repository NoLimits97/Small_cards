#include "navigation_sidebar.hpp"

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

namespace mk::gui {

NavigationSidebar::NavigationSidebar(QWidget* parent)
    : QFrame(parent) {
    setObjectName(QStringLiteral("navigationSidebar"));
    setFixedWidth(230);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* logo_bar = new QFrame(this);
    logo_bar->setObjectName(QStringLiteral("logoBar"));
    logo_bar->setFixedHeight(64);
    auto* logo_layout = new QHBoxLayout(logo_bar);
    logo_layout->setContentsMargins(20, 0, 20, 0);
    logo_layout->setSpacing(11);

    auto* logo_badge = new QLabel(QStringLiteral("М"), logo_bar);
    logo_badge->setObjectName(QStringLiteral("logoBadge"));
    logo_badge->setFixedSize(28, 28);
    logo_badge->setAlignment(Qt::AlignCenter);

    auto* logo_text = new QLabel(QStringLiteral("Малые кадры"), logo_bar);
    logo_text->setObjectName(QStringLiteral("logoText"));

    logo_layout->addWidget(logo_badge);
    logo_layout->addWidget(logo_text);
    logo_layout->addStretch();
    layout->addWidget(logo_bar);

    auto* menu = new QWidget(this);
    menu_layout_ = new QVBoxLayout(menu);
    menu_layout_->setContentsMargins(10, 16, 10, 16);
    menu_layout_->setSpacing(3);

    auto* menu_title = new QLabel(QStringLiteral("РАЗДЕЛЫ"), menu);
    menu_title->setObjectName(QStringLiteral("menuTitle"));
    menu_title->setContentsMargins(12, 0, 12, 6);
    menu_layout_->addWidget(menu_title);

    group_ = new QButtonGroup(this);
    group_->setExclusive(true);

    add_section(
        0, QStringLiteral("⌂"), QStringLiteral("Рабочий стол"));
    add_section(
        1, QStringLiteral("▣"), QStringLiteral("Организация"));
    add_section(
        2, QStringLiteral("☷"), QStringLiteral("Справочники"));
    add_section(
        3, QStringLiteral("♙"), QStringLiteral("Сотрудники"));
    add_section(
        4, QStringLiteral("▦"), QStringLiteral("Табель"));
    add_section(
        5, QStringLiteral("▤"), QStringLiteral("Расчёт"));
    menu_layout_->addStretch();
    layout->addWidget(menu, 1);

    auto* bottom = new QFrame(this);
    bottom->setObjectName(QStringLiteral("sidebarBottom"));
    auto* bottom_layout = new QVBoxLayout(bottom);
    bottom_layout->setContentsMargins(16, 12, 16, 16);
    status_label_ = new QLabel(
        QStringLiteral("База данных не открыта"), bottom);
    status_label_->setObjectName(QStringLiteral("sidebarStatus"));
    status_label_->setWordWrap(true);
    bottom_layout->addWidget(status_label_);
    layout->addWidget(bottom);
}

void NavigationSidebar::add_section(
    int section_id,
    const QString& icon,
    const QString& title) {
    auto* button = new QPushButton(
        QStringLiteral("%1   %2").arg(icon, title), this);
    button->setObjectName(QStringLiteral("sidebarItem"));
    button->setCheckable(true);
    button->setCursor(Qt::PointingHandCursor);
    button->setMinimumHeight(40);
    group_->addButton(button, section_id);
    buttons_.insert(section_id, button);
    menu_layout_->addWidget(button);

    connect(button, &QPushButton::clicked, this, [this, section_id]() {
        if (on_section_selected_) {
            on_section_selected_(section_id);
        }
    });
}

void NavigationSidebar::set_on_section_selected(
    std::function<void(int)> handler) {
    on_section_selected_ = std::move(handler);
}

void NavigationSidebar::set_active_section(int section_id) {
    const auto it = buttons_.find(section_id);
    if (it != buttons_.end()) {
        it.value()->setChecked(true);
    }
}

void NavigationSidebar::set_section_enabled(
    int section_id,
    bool enabled) {
    const auto it = buttons_.find(section_id);
    if (it != buttons_.end()) {
        it.value()->setEnabled(enabled);
    }
}

void NavigationSidebar::set_database_status(
    const QString& text,
    const QString& tooltip) {
    if (status_label_ == nullptr) {
        return;
    }
    status_label_->setText(text);
    status_label_->setToolTip(tooltip);
}

}
