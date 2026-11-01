#include "test_framework.hpp"

#include "mk/dependency_graph.hpp"

#include <string>
#include <vector>

namespace {

mk::CalcItem item(
    const std::string& code,
    const std::string& formula,
    int32_t stage,
    int32_t priority = 0) {
    mk::CalcItem result;
    result.code = code;
    result.formula = formula;
    result.stage = stage;
    result.priority = priority;
    return result;
}

std::vector<std::string> codes(
    const std::vector<mk::CalcItem>& items) {
    std::vector<std::string> result;

    for (const mk::CalcItem& item : items) {
        result.push_back(item.code);
    }

    return result;
}

}

TEST(dependency_graph_salary_before_bonus) {
    const std::vector<mk::CalcItem> items{
        item("premija", "oklad * 0.1", 2),
        item("oklad", "1000", 1)
    };

    const mk::DependencyGraph graph =
        mk::build_dependency_graph(items);

    REQUIRE_EQUAL(graph.at("premija"), (std::vector<std::string>{"oklad"}));
    REQUIRE(graph.at("oklad").empty());

    const std::vector<mk::CalcItem> sorted =
        mk::sort_calc_items(items);

    REQUIRE_EQUAL(
        codes(sorted),
        (std::vector<std::string>{"oklad", "premija"}));
}

TEST(dependency_graph_bonus_before_tax) {
    const std::vector<mk::CalcItem> items{
        item("ndfl", "premija * 0.13", 3),
        item("premija", "oklad * 0.1", 2),
        item("oklad", "1000", 1)
    };

    REQUIRE_EQUAL(
        codes(mk::sort_calc_items(items)),
        (std::vector<std::string>{"oklad", "premija", "ndfl"}));
}

TEST(dependency_graph_cycle) {
    const std::vector<mk::CalcItem> items{
        item("a", "b + 1", 1),
        item("b", "a + 1", 1)
    };

    const std::vector<std::string> errors =
        mk::validate_dependencies(items);

    REQUIRE(!errors.empty());
    REQUIRE(
        errors.front().find("cyclic dependency") != std::string::npos);
    REQUIRE_THROWS(mk::sort_calc_items(items));
}

TEST(dependency_graph_unknown_item) {
    const std::vector<mk::CalcItem> items{
        item("premija", "oklad * 0.1", 2)
    };

    const std::vector<std::string> errors =
        mk::validate_dependencies(items);

    REQUIRE(!errors.empty());
    REQUIRE(
        errors.front().find("unknown calc item dependency") !=
        std::string::npos);
    REQUIRE_THROWS(mk::build_dependency_graph(items));
}

TEST(dependency_graph_later_stage_is_forbidden) {
    const std::vector<mk::CalcItem> items{
        item("oklad", "ndfl + 1", 1),
        item("ndfl", "100", 4)
    };

    const std::vector<std::string> errors =
        mk::validate_dependencies(items);

    REQUIRE(!errors.empty());
    REQUIRE(
        errors.front().find("later stage") != std::string::npos);
    REQUIRE_THROWS(mk::sort_calc_items(items));
}

TEST(dependency_graph_priority_order) {
    const std::vector<mk::CalcItem> items{
        item("z", "100", 1, 20),
        item("a", "100", 1, 10),
        item("m", "100", 1, 10)
    };

    REQUIRE_EQUAL(
        codes(mk::sort_calc_items(items)),
        (std::vector<std::string>{"a", "m", "z"}));
}

TEST(dependency_graph_deterministic_order) {
    const std::vector<mk::CalcItem> items{
        item("z", "100", 2),
        item("b", "100", 1),
        item("a", "100", 1),
        item("c", "b + 1", 2)
    };

    REQUIRE_EQUAL(
        codes(mk::sort_calc_items(items)),
        (std::vector<std::string>{"a", "b", "z", "c"}));
}

TEST(dependency_graph_duplicate_codes) {
    const std::vector<mk::CalcItem> items{
        item("oklad", "100", 1),
        item("oklad", "200", 1)
    };

    const std::vector<std::string> errors =
        mk::validate_dependencies(items);

    REQUIRE(!errors.empty());
    REQUIRE(
        errors.front().find("duplicate calc item code") !=
        std::string::npos);
}

TEST(dependency_graph_stage_range) {
    const std::vector<mk::CalcItem> items{
        item("oklad", "100", 0)
    };

    const std::vector<std::string> errors =
        mk::validate_dependencies(items);

    REQUIRE(!errors.empty());
    REQUIRE(
        errors.front().find("stage out of range") !=
        std::string::npos);
}

TEST(dependency_graph_self_dependency) {
    const std::vector<mk::CalcItem> items{
        item("oklad", "oklad + 100", 1)
    };

    const std::vector<std::string> errors =
        mk::validate_dependencies(items);

    REQUIRE(!errors.empty());
    REQUIRE(
        errors.front().find("self dependency") !=
        std::string::npos);
}
