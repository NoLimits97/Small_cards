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

std::map<std::string, std::vector<std::string>> build_dependents(
    const DependencyGraph& graph) {
    std::map<std::string, std::vector<std::string>> dependents;

    for (const auto& entry : graph) {
        for (const std::string& dependency : entry.second) {
            dependents[dependency].push_back(entry.first);
        }
    }

    return dependents;
}

std::map<std::string, int> dependency_levels(
    const DependencyGraph& graph) {
    std::map<std::string, int> remaining_dependencies;
    std::map<std::string, int> levels;
    const std::map<std::string, std::vector<std::string>> dependents =
        build_dependents(graph);

    for (const auto& entry : graph) {
        remaining_dependencies[entry.first] =
            static_cast<int>(entry.second.size());
        levels[entry.first] = 0;
    }

    std::set<std::string> ready;

    for (const auto& entry : remaining_dependencies) {
        if (entry.second == 0) {
            ready.insert(entry.first);
        }
    }

    while (!ready.empty()) {
        const std::string code = *ready.begin();
        ready.erase(ready.begin());

        for (const std::string& dependent : dependents.at(code)) {
            levels[dependent] = std::max(
                levels[dependent],
                levels[code] + 1);

            --remaining_dependencies[dependent];
            if (remaining_dependencies[dependent] == 0) {
                ready.insert(dependent);
            }
        }
    }

    return levels;
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
    const std::map<std::string, std::vector<std::string>> dependents =
        build_dependents(graph);

    for (const auto& entry : graph) {
        remaining_dependencies[entry.first] =
            static_cast<int>(entry.second.size());
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

        for (const std::string& dependent : dependents.at(code)) {
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
    const std::map<std::string, int> levels =
        dependency_levels(graph);

    std::vector<CalcItem> result = items;

    std::sort(
        result.begin(),
        result.end(),
        [&levels](const CalcItem& left, const CalcItem& right) {
            if (left.stage != right.stage) {
                return left.stage < right.stage;
            }

            if (levels.at(left.code) != levels.at(right.code)) {
                return levels.at(left.code) < levels.at(right.code);
            }

            if (left.priority != right.priority) {
                return left.priority < right.priority;
            }

            return left.code < right.code;
        });

    return result;
}

}
