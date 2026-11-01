#ifndef MK_DEPENDENCY_GRAPH_HPP
#define MK_DEPENDENCY_GRAPH_HPP

#include "mk/model.hpp"

#include <map>
#include <string>
#include <vector>

namespace mk {

using DependencyGraph = std::map<std::string, std::vector<std::string>>;

DependencyGraph build_dependency_graph(
    const std::vector<CalcItem>& items);

std::vector<std::string> validate_dependencies(
    const std::vector<CalcItem>& items);

std::vector<CalcItem> sort_calc_items(
    const std::vector<CalcItem>& items);

}

#endif
