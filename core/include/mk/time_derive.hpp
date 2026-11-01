#ifndef MK_TIME_DERIVE_HPP
#define MK_TIME_DERIVE_HPP

#include "mk/model.hpp"

#include <cstdint>
#include <vector>

namespace mk {

std::vector<DerivedTime> derive_time(
    int64_t employee_id,
    const std::vector<NormDay>& norm_days,
    const std::vector<TimeFact>& time_facts,
    const std::vector<AbsenceKind>& absence_kinds);

}

#endif
