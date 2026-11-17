#include "workspace_tabs.hpp"

#include <QEnterEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace mk::gui {

namespace {

void repolish(QWidget* widget) {
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

}

class TabButton : public QFrame {
public:
    TabButton(
        const QString& title,
        bool closable,
        std::function<void()> on_activate,
        std::function<void()> on_close,
        QWidget* parent = nullptr)
        : QFrame(parent),
          on_activate_(std::move(on_activate)),
          on_close_(std::move(on_close)) {
        setObjectName(QStringLiteral("workspaceTab"));
        setProperty("active", false);
        setProperty("hovered", false);
        setAttribute(Qt::WA_StyledBackground, true);
        setFrameShape(QFrame::NoFrame);
        setFixedHeight(40);
        setMinimumWidth(120);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        setCursor(Qt::PointingHandCursor);

        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(13, 0, 8, 0);
        layout->setSpacing(8);

        auto* label = new QLabel(title, this);
        label->setObjectName(QStringLiteral("workspaceTabTitle"));
        layout->addWidget(label);
        layout->addStretch();

        if (closable) {
            auto* close = new QPushButton(QStringLiteral("×"), this);
            close->setObjectName(QStringLiteral("tabCloseButton"));
            close->setFixedSize(18, 18);
            close->setFlat(true);
            close->setCursor(Qt::ArrowCursor);
            close->setToolTip(QStringLiteral("Закрыть вкладку"));
            connect(close, &QPushButton::clicked, this, [this]() {
                if (on_close_) {
                    on_close_();
                }
            });
            layout->addWidget(close);
        }
    }

    void set_active(bool active) {
        if (property("active").toBool() == active) {
            return;
        }
        setProperty("active", active);
        setCursor(
            active ? Qt::ArrowCursor : Qt::PointingHandCursor);
        repolish(this);
    }

protected:
    void mouseReleaseEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton &&
            rect().contains(event->position().toPoint()) &&
            on_activate_) {
            on_activate_();
        }
        QFrame::mouseReleaseEvent(event);
    }

    void enterEvent(QEnterEvent* event) override {
        QFrame::enterEvent(event);
        setProperty("hovered", true);
        repolish(this);
    }

    void leaveEvent(QEvent* event) override {
        QFrame::leaveEvent(event);
        setProperty("hovered", false);
        repolish(this);
    }

private:
    std::function<void()> on_activate_;
    std::function<void()> on_close_;
};

WorkspaceTabs::WorkspaceTabs(QWidget* parent)
    : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    bar_ = new QFrame(this);
    bar_->setObjectName(QStringLiteral("tabsBar"));
    bar_->setFixedHeight(58);
    auto* bar_layout = new QHBoxLayout(bar_);
    bar_layout->setContentsMargins(18, 16, 18, 0);
    bar_layout->setSpacing(0);

    scroll_ = new QScrollArea(bar_);
    scroll_->setObjectName(QStringLiteral("tabsScroll"));
    scroll_->setWidgetResizable(true);
    scroll_->setFrameShape(QFrame::NoFrame);
    scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll_->setFixedHeight(42);
    scroll_->viewport()->setAutoFillBackground(false);

    tab_container_ = new QWidget(scroll_);
    tab_layout_ = new QHBoxLayout(tab_container_);
    tab_layout_->setContentsMargins(0, 0, 0, 0);
    tab_layout_->setSpacing(3);
    tab_layout_->addStretch();
    scroll_->setWidget(tab_container_);
    connect(
        scroll_->horizontalScrollBar(),
        &QScrollBar::rangeChanged,
        this,
        [this](int, int maximum) {
            const bool needs_scrollbar = maximum > 0;
            scroll_->setFixedHeight(needs_scrollbar ? 50 : 42);
            bar_->setFixedHeight(needs_scrollbar ? 66 : 58);
        });
    bar_layout->addWidget(scroll_, 1);
    layout->addWidget(bar_);

    stack_ = new QStackedWidget(this);
    stack_->setObjectName(QStringLiteral("workspaceStack"));
    layout->addWidget(stack_, 1);
}

void WorkspaceTabs::add_page(int section_id, QWidget* page) {
    if (page == nullptr) {
        return;
    }
    pages_.insert(section_id, page);
    stack_->addWidget(page);
}

void WorkspaceTabs::open_section(
    int section_id,
    const QString& title,
    bool closable) {
    if (!pages_.contains(section_id)) {
        return;
    }

    if (!buttons_.contains(section_id)) {
        auto* button = new TabButton(
            title,
            closable,
            [this, section_id]() { activate_section(section_id); },
            [this, section_id]() { close_section(section_id); },
            tab_container_);
        buttons_.insert(section_id, button);
        titles_.insert(section_id, title);
        closable_.insert(section_id, closable);
        order_.append(section_id);
        tab_layout_->insertWidget(
            std::max(0, tab_layout_->count() - 1), button);
    }

    activate_section(section_id);
}

void WorkspaceTabs::activate_section(int section_id) {
    const auto button_it = buttons_.find(section_id);
    const auto page_it = pages_.find(section_id);
    if (button_it == buttons_.end() || page_it == pages_.end()) {
        return;
    }

    if (current_ == section_id) {
        stack_->setCurrentWidget(page_it.value());
        return;
    }

    current_ = section_id;
    for (auto it = buttons_.begin(); it != buttons_.end(); ++it) {
        it.value()->set_active(it.key() == section_id);
    }
    stack_->setCurrentWidget(page_it.value());
    tab_layout_->activate();
    scroll_->ensureWidgetVisible(button_it.value());
    QTimer::singleShot(0, button_it.value(), [this, section_id]() {
        if (!buttons_.contains(section_id)) {
            return;
        }
        tab_layout_->activate();
        scroll_->ensureWidgetVisible(buttons_.value(section_id));
    });

    if (current_changed_) {
        current_changed_(section_id);
    }
}

void WorkspaceTabs::close_section(int section_id) {
    const auto button_it = buttons_.find(section_id);
    if (button_it == buttons_.end()) {
        return;
    }
    if (can_close_ && !can_close_(section_id)) {
        return;
    }

    const int position = order_.indexOf(section_id);
    auto* button = button_it.value();
    buttons_.erase(button_it);
    titles_.remove(section_id);
    closable_.remove(section_id);
    order_.removeAll(section_id);
    tab_layout_->removeWidget(button);
    button->hide();
    button->deleteLater();

    if (order_.isEmpty()) {
        current_ = -1;
        stack_->setCurrentIndex(-1);
        return;
    }

    if (current_ == section_id) {
        const int next_position = std::clamp(
            position, 0, static_cast<int>(order_.size()) - 1);
        const int next_section = order_[next_position];
        current_ = -1;
        activate_section(next_section);
    }
}

bool WorkspaceTabs::is_open(int section_id) const {
    return buttons_.contains(section_id);
}

int WorkspaceTabs::current_section() const {
    return current_;
}

void WorkspaceTabs::set_current_changed_handler(
    std::function<void(int)> handler) {
    current_changed_ = std::move(handler);
}

void WorkspaceTabs::set_can_close_handler(
    std::function<bool(int)> handler) {
    can_close_ = std::move(handler);
}

}
