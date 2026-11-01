#ifndef MK_GUI_MONEY_FORMATTER_HPP
#define MK_GUI_MONEY_FORMATTER_HPP

#include "mk/types.hpp"

#include <QString>

#include <optional>

namespace mk::gui {

struct MoneyParseResult {
    std::optional<Money> value;
    QString error;
};

QString format_money(Money value);
MoneyParseResult parse_money(const QString& text);

}

#endif
