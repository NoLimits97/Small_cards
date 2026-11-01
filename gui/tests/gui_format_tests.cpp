#include "test_framework.hpp"

#include "gui_format.hpp"

#include <QString>

namespace {

mk::Employee employee(bool terminated) {
    mk::Employee value;
    value.hire_date = mk::Date(2026, 1, 15);
    if (terminated) {
        value.termination_date = mk::Date(2026, 6, 30);
    }
    return value;
}

}

TEST(gui_format_formats_date_with_leading_zeros) {
    REQUIRE_EQUAL(
        mk::gui::format_date(mk::Date(2026, 1, 5)),
        QStringLiteral("05.01.2026"));
    REQUIRE_EQUAL(
        mk::gui::format_date(mk::Date(2026, 12, 31)),
        QStringLiteral("31.12.2026"));
    REQUIRE_EQUAL(
        mk::gui::format_date(mk::Date(1, 1, 1)),
        QStringLiteral("01.01.0001"));
}

TEST(gui_format_formats_rate_units) {
    REQUIRE_EQUAL(
        mk::gui::format_rate_unit(mk::RateUnit::Month),
        QStringLiteral("в месяц"));
    REQUIRE_EQUAL(
        mk::gui::format_rate_unit(mk::RateUnit::Day),
        QStringLiteral("в день"));
    REQUIRE_EQUAL(
        mk::gui::format_rate_unit(mk::RateUnit::Hour),
        QStringLiteral("в час"));
}

TEST(gui_format_formats_event_kinds) {
    REQUIRE_EQUAL(
        mk::gui::format_event_kind(mk::EventKind::Hire),
        QStringLiteral("Приём"));
    REQUIRE_EQUAL(
        mk::gui::format_event_kind(mk::EventKind::Transfer),
        QStringLiteral("Перевод"));
    REQUIRE_EQUAL(
        mk::gui::format_event_kind(mk::EventKind::ChangeSalary),
        QStringLiteral("Изменение ставки"));
    REQUIRE_EQUAL(
        mk::gui::format_event_kind(mk::EventKind::Termination),
        QStringLiteral("Увольнение"));
}

TEST(gui_format_shows_employee_status) {
    REQUIRE_EQUAL(
        mk::gui::employee_status_text(employee(false)),
        QStringLiteral("Работает"));
    REQUIRE_EQUAL(
        mk::gui::employee_status_text(employee(true)),
        QStringLiteral("Уволен 30.06.2026"));
}
