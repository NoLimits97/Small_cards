#ifndef MK_GUI_FORMAT_HPP
#define MK_GUI_FORMAT_HPP

#include "mk/types.hpp"
#include "mk/model.hpp"

#include <QString>

#include <cstdint>

namespace mk::gui {

QString format_date(Date value);
QString format_minutes(int64_t minutes);
QString format_rate_unit(RateUnit value);
QString format_event_kind(EventKind value);
QString format_calc_status(CalcStatus value);
QString employee_status_text(const Employee& employee);

}

#endif
