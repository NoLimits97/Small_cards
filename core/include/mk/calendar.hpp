#ifndef MK_CALENDAR_HPP
#define MK_CALENDAR_HPP

#include "mk/model.hpp"

#include <cstdint>
#include <vector>

namespace mk {

std::vector<NormDay> generate_norm_calendar(
    const WorkSchedule& schedule,
    int32_t year,
    int32_t month);

}

#endif
