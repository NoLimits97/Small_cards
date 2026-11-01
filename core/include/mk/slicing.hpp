#ifndef MK_SLICING_HPP
#define MK_SLICING_HPP

#include "mk/model.hpp"

#include <optional>
#include <vector>

namespace mk {

std::optional<EmployeeSlice> resolve_slice(
    const std::vector<EmploymentEvent>& events,
    const Date& period_first_day);

}

#endif
