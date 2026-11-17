#include "section_page.hpp"

#include <QButtonGroup>
#include <QFrame>
#include <QHBoxLayout>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace mk::gui {

SectionPage::SectionPage(QWidget* parent)
    : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(14);

    auto* nav_bar = new QFrame(this);
    nav_bar_ = nav_bar;
    nav_bar->setObjectName(QStringLiteral("subsectionBar"));
    nav_layout_ = new QHBoxLayout(nav_bar);
    nav_layout_->setContentsMargins(0, 0, 0, 0);
    nav_layout_->setSpacing(4);
    nav_layout_->addStretch();
    nav_bar->hide();
    layout->addWidget(nav_bar);

    stack_ = new QStackedWidget(this);
    stack_->setObjectName(QStringLiteral("subsectionStack"));
    layout->addWidget(stack_, 1);

    group_ = new QButtonGroup(this);
    group_->setExclusive(true);
}

void SectionPage::add_page(const QString& title, QWidget* page) {
    if (page == nullptr) {
        return;
    }

    const int index = page_count_++;
    stack_->addWidget(page);

    auto* button = new QPushButton(title, nav_bar_);
    button->setObjectName(QStringLiteral("subsectionButton"));
    button->setCheckable(true);
    button->setCursor(Qt::PointingHandCursor);
    if (index == 0) {
        button->setChecked(true);
    }
    group_->addButton(button, index);
    nav_layout_->insertWidget(
        nav_layout_->count() - 1, button);

    connect(button, &QPushButton::clicked, this, [this, index]() {
        stack_->setCurrentIndex(index);
    });

    nav_bar_->setVisible(page_count_ > 1);
}

}
