#include "mk/dependency_graph.hpp"

#include "mk/formula.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace mk {
namespace {

std::map<std::string, const CalcItem*> index_items(
    const std::vector<CalcItem>& items) {
    std::map<std::string, const CalcItem*> indexed;

    for (const CalcItem& item : items) {
        if (item.code.empty()) {
            throw CalcError("calc item code must not be empty");
        }

        if (!indexed.emplace(item.code, &item).second) {
            throw CalcError("duplicate calc item code: " + item.code);
        }

        if (item.stage < 1 || item.stage > 5) {
            throw CalcError(
                "calc item stage out of range for " + item.code);
        }
    }

    return indexed;
}

DependencyGraph build_graph(
    const std::vector<CalcItem>& items,
    const std::map<std::string, const CalcItem*>& indexed) {
    DependencyGraph graph;

    for (const CalcItem& item : items) {
        std::set<std::string> unique_dependencies;
        const Formula formula = parse(item.formula);

        for (const std::string& variable : extract_variables(formula)) {
            if (indexed.find(variable) == indexed.end()) {
                throw CalcError(
                    "unknown calc item dependency: " + item.code +
                    " -> " + variable);
            }

            if (variable == item.code) {
                throw CalcError(
                    "self dependency: " + item.code);
            }

            unique_dependencies.insert(variable);
        }

        graph[item.code] = std::vector<std::string>(
            unique_dependencies.begin(),
            unique_dependencies.end());
    }

    return graph;
}

std::string cycle_error(const std::string& code) {
    return "cyclic dependency involving calc item: " + code;
}

}

DependencyGraph build_dependency_graph(
    const std::vector<CalcItem>& items) {
    const std::map<std::string, const CalcItem*> indexed =
        index_items(items);

    return build_graph(items, indexed);
}

std::vector<std::string> validate_dependencies(
    const std::vector<CalcItem>& items) {
    std::vector<std::string> errors;
    std::map<std::string, const CalcItem*> indexed;

    try {
        indexed = index_items(items);
    } catch (const CalcError& error) {
        errors.push_back(error.what());
        return errors;
    }

    DependencyGraph graph;
    try {
        graph = build_graph(items, indexed);
    } catch (const CalcError& error) {
        errors.push_back(error.what());
        return errors;
    }

    for (const CalcItem& item : items) {
        const auto dependencies = graph.find(item.code);
        if (dependencies == graph.end()) {
            continue;
        }

        for (const std::string& dependency : dependencies->second) {
            const CalcItem& dependency_item = *indexed.at(dependency);

            if (dependency_item.stage > item.stage) {
                errors.push_back(
                    "calc item " + item.code +
                    " depends on later stage " + dependency);
            }
        }
    }

    std::map<std::string, int> remaining_dependencies;
    std::map<std::string, std::vector<std::string>> dependents;

    for (const auto& entry : graph) {
        remaining_dependencies[entry.first] =
            static_cast<int>(entry.second.size());

        for (const std::string& dependency : entry.second) {
            dependents[dependency].push_back(entry.first);
        }
    }

    std::set<std::string> ready;
    for (const auto& entry : remaining_dependencies) {
        if (entry.second == 0) {
            ready.insert(entry.first);
        }
    }

    std::size_t processed = 0;
    while (!ready.empty()) {
        const std::string code = *ready.begin();
        ready.erase(ready.begin());
        ++processed;

        for (const std::string& dependent : dependents[code]) {
            --remaining_dependencies[dependent];
            if (remaining_dependencies[dependent] == 0) {
                ready.insert(dependent);
            }
        }
    }

    if (processed != items.size()) {
        for (const auto& entry : remaining_dependencies) {
            if (entry.second > 0) {
                errors.push_back(cycle_error(entry.first));
            }
        }
    }

    return errors;
}

std::vector<CalcItem> sort_calc_items(
    const std::vector<CalcItem>& items) {
    const std::vector<std::string> errors =
        validate_dependencies(items);

    if (!errors.empty()) {
        throw CalcError(errors.front());
    }

    const std::map<std::string, const CalcItem*> indexed =
        index_items(items);
    const DependencyGraph graph = build_graph(items, indexed);

    std::map<std::string, int> remaining_dependencies;
    std::map<std::string, std::vector<std::string>> dependents;

    for (const auto& entry : graph) {
        remaining_dependencies[entry.first] =
            static_cast<int>(entry.second.size());

        for (const std::string& dependency : entry.second) {
            dependents[dependency].push_back(entry.first);
        }
    }

    auto compare_ready = [&indexed](
        const std::string& left,
        const std::string& right) {
        const CalcItem& left_item = *indexed.at(left);
        const CalcItem& right_item = *indexed.at(right);

        if (left_item.stage != right_item.stage) {
            return left_item.stage < right_item.stage;
        }

        if (left_item.priority != right_item.priority) {
            return left_item.priority < right_item.priority;
        }

        return left_item.code < right_item.code;
    };

    std::set<std::string, decltype(compare_ready)> ready(compare_ready);

    for (const auto& entry : remaining_dependencies) {
        if (entry.second == 0) {
            ready.insert(entry.first);
        }
    }

    std::vector<CalcItem> result;
    result.reserve(items.size());

    while (!ready.empty()) {
        const std::string code = *ready.begin();
        ready.erase(ready.begin());
        result.push_back(*indexed.at(code));

        for (const std::string& dependent : dependents[code]) {
            --remaining_dependencies[dependent];
            if (remaining_dependencies[dependent] == 0) {
                ready.insert(dependent);
            }
        }
    }

    return result;
}

}
