#include "gui_format.hpp"

namespace mk::gui {

QString format_date(Date value) {
    return QStringLiteral("%1.%2.%3")
        .arg(value.day(), 2, 10, QLatin1Char('0'))
        .arg(value.month(), 2, 10, QLatin1Char('0'))
        .arg(value.year(), 4, 10, QLatin1Char('0'));
}

QString format_minutes(int64_t minutes) {
    if (minutes < 0) {
        minutes = 0;
    }
    return QStringLiteral("%1:%2")
        .arg(minutes / 60)
        .arg(
            minutes % 60,
            2,
            10,
            QLatin1Char('0'));
}

QString format_rate_unit(RateUnit value) {
    switch (value) {
        case RateUnit::Month:
            return QStringLiteral("в месяц");
        case RateUnit::Day:
            return QStringLiteral("в день");
        case RateUnit::Hour:
            return QStringLiteral("в час");
    }
    return QStringLiteral("?");
}

QString format_event_kind(EventKind value) {
    switch (value) {
        case EventKind::Hire:
            return QStringLiteral("Приём");
        case EventKind::Transfer:
            return QStringLiteral("Перевод");
        case EventKind::ChangeSalary:
            return QStringLiteral("Изменение ставки");
        case EventKind::Termination:
            return QStringLiteral("Увольнение");
    }
    return QStringLiteral("?");
}

QString format_calc_status(CalcStatus value) {
    switch (value) {
        case CalcStatus::Ok:
            return QStringLiteral("Рассчитано");
        case CalcStatus::Incomplete:
            return QStringLiteral("Не рассчитывается");
        case CalcStatus::NegativeNet:
            return QStringLiteral("Удержания превышают начисление");
    }
    return QStringLiteral("?");
}

QString employee_status_text(const Employee& employee) {
    return employee.termination_date.has_value()
        ? QStringLiteral("Уволен %1")
            .arg(format_date(*employee.termination_date))
        : QStringLiteral("Работает");
}

}
