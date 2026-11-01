#include "test_framework.hpp"

#include "money_formatter.hpp"

#include <QString>

#include <cstdint>
#include <limits>
#include <vector>

namespace {

void require_parsed(const QString& text, int64_t cents) {
    const mk::gui::MoneyParseResult result = mk::gui::parse_money(text);
    REQUIRE(result.value.has_value());
    REQUIRE(result.error.isEmpty());
    REQUIRE_EQUAL(*result.value, mk::Money(cents));
}

void require_invalid(const QString& text) {
    const mk::gui::MoneyParseResult result = mk::gui::parse_money(text);
    REQUIRE(!result.value.has_value());
    REQUIRE(!result.error.isEmpty());
}

}

TEST(money_formatter_formats_values) {
    REQUIRE_EQUAL(mk::gui::format_money(mk::Money(0)),
                  QStringLiteral("0,00"));
    REQUIRE_EQUAL(mk::gui::format_money(mk::Money(1)),
                  QStringLiteral("0,01"));
    REQUIRE_EQUAL(mk::gui::format_money(mk::Money(-1)),
                  QStringLiteral("-0,01"));
    REQUIRE_EQUAL(mk::gui::format_money(mk::Money(1200)),
                  QStringLiteral("12,00"));
    REQUIRE_EQUAL(mk::gui::format_money(mk::Money(-12345)),
                  QStringLiteral("-123,45"));
    REQUIRE_EQUAL(
        mk::gui::format_money(
            mk::Money(std::numeric_limits<int64_t>::max())),
        QStringLiteral("92233720368547758,07"));
    REQUIRE_EQUAL(
        mk::gui::format_money(
            mk::Money(std::numeric_limits<int64_t>::min())),
        QStringLiteral("-92233720368547758,08"));
}

TEST(money_formatter_parses_valid_values) {
    require_parsed(QStringLiteral("0"), 0);
    require_parsed(QStringLiteral("0,01"), 1);
    require_parsed(QStringLiteral("-0,01"), -1);
    require_parsed(QStringLiteral("12"), 1200);
    require_parsed(QStringLiteral("12,3"), 1230);
    require_parsed(QStringLiteral("12.30"), 1230);
    require_parsed(QStringLiteral("-123,45"), -12345);
    require_parsed(QStringLiteral("  +12,3  "), 1230);
    require_parsed(QStringLiteral("\t-12.30\n"), -1230);
    require_parsed(QStringLiteral("-0,00"), 0);
    require_parsed(QStringLiteral("92233720368547758,07"),
                   std::numeric_limits<int64_t>::max());
    require_parsed(QStringLiteral("-92233720368547758,08"),
                   std::numeric_limits<int64_t>::min());
}

TEST(money_formatter_rejects_invalid_values) {
    const std::vector<QString> invalid{
        QString(),
        QStringLiteral("   "),
        QStringLiteral(".50"),
        QStringLiteral("12,"),
        QStringLiteral("1 2"),
        QStringLiteral("12, 3"),
        QStringLiteral("1 234,56"),
        QStringLiteral("1,234.56"),
        QStringLiteral("abc"),
        QStringLiteral("12a"),
        QStringLiteral("1e2"),
        QStringLiteral("12,3,4"),
        QStringLiteral("12.3.4"),
        QStringLiteral("12,345"),
        QStringLiteral("12,300"),
        QStringLiteral("+"),
        QStringLiteral("--12"),
        QStringLiteral("１２,３０"),
        QStringLiteral("999999999999999999999999999999999999999999999"),
        QStringLiteral("-999999999999999999999999999999999999999999999"),
        QStringLiteral("92233720368547758,08"),
        QStringLiteral("-92233720368547758,09")
    };

    for (const QString& text : invalid) {
        require_invalid(text);
    }
}

TEST(money_formatter_round_trip) {
    const std::vector<int64_t> values{
        std::numeric_limits<int64_t>::min(),
        -12345,
        -100,
        -1,
        0,
        1,
        100,
        12345,
        std::numeric_limits<int64_t>::max()
    };

    for (const int64_t cents : values) {
        require_parsed(
            mk::gui::format_money(mk::Money(cents)),
            cents);
    }
}
