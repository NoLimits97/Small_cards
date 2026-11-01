#include "money_formatter.hpp"

#include <cstdint>
#include <limits>

namespace mk::gui {
namespace {

MoneyParseResult invalid_money() {
    return {
        std::nullopt,
        QStringLiteral(
            "Введите сумму цифрами, с одной или двумя цифрами "
            "после запятой или точки.")
    };
}

MoneyParseResult money_overflow() {
    return {
        std::nullopt,
        QStringLiteral("Сумма выходит за допустимый диапазон.")
    };
}

bool is_ascii_digit(QChar character) {
    return character >= QLatin1Char('0') &&
           character <= QLatin1Char('9');
}

}

QString format_money(Money value) {
    const bool negative = value.cents < 0;
    const uint64_t magnitude = negative
        ? static_cast<uint64_t>(-(value.cents + 1)) + 1
        : static_cast<uint64_t>(value.cents);

    const QString rubles = QString::number(magnitude / 100);
    const QString kopecks = QString::number(magnitude % 100)
        .rightJustified(2, QLatin1Char('0'));

    return (negative ? QStringLiteral("-") : QString()) +
           rubles + QStringLiteral(",") + kopecks;
}

MoneyParseResult parse_money(const QString& text) {
    const QString input = text.trimmed();
    if (input.isEmpty()) {
        return invalid_money();
    }

    qsizetype index = 0;
    bool negative = false;
    if (input[index] == QLatin1Char('+') ||
        input[index] == QLatin1Char('-')) {
        negative = input[index] == QLatin1Char('-');
        ++index;
    }

    const uint64_t positive_limit =
        static_cast<uint64_t>(std::numeric_limits<int64_t>::max());
    const uint64_t limit = positive_limit + (negative ? 1U : 0U);
    const uint64_t ruble_limit = limit / 100;
    uint64_t rubles = 0;
    qsizetype integer_digits = 0;

    while (index < input.size() && is_ascii_digit(input[index])) {
        const uint64_t digit =
            static_cast<uint64_t>(input[index].unicode() - '0');
        if (rubles > ruble_limit / 10 ||
            (rubles == ruble_limit / 10 &&
             digit > ruble_limit % 10)) {
            return money_overflow();
        }
        rubles = rubles * 10 + digit;
        ++integer_digits;
        ++index;
    }

    if (integer_digits == 0) {
        return invalid_money();
    }

    uint64_t kopecks = 0;
    if (index < input.size()) {
        if (input[index] != QLatin1Char(',') &&
            input[index] != QLatin1Char('.')) {
            return invalid_money();
        }
        ++index;

        qsizetype fractional_digits = 0;
        while (index < input.size() &&
               fractional_digits < 2 &&
               is_ascii_digit(input[index])) {
            kopecks = kopecks * 10 +
                static_cast<uint64_t>(input[index].unicode() - '0');
            ++fractional_digits;
            ++index;
        }

        if (fractional_digits == 0 || index != input.size()) {
            return invalid_money();
        }
        if (fractional_digits == 1) {
            kopecks *= 10;
        }
    }

    const uint64_t whole_kopecks = rubles * 100;
    if (kopecks > limit - whole_kopecks) {
        return money_overflow();
    }

    const uint64_t magnitude = whole_kopecks + kopecks;
    const int64_t cents = negative
        ? magnitude == positive_limit + 1
            ? std::numeric_limits<int64_t>::min()
            : -static_cast<int64_t>(magnitude)
        : static_cast<int64_t>(magnitude);

    return {Money(cents), QString()};
}

}
