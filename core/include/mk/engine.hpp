#ifndef MK_ENGINE_HPP
#define MK_ENGINE_HPP

#include "mk/model.hpp"

#include <vector>

namespace mk {

std::vector<ValidationError>
validate(const CalcInput& input);

CalcOutput calculate(const CalcInput& input);

}

#endif
