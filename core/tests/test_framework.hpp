#ifndef MK_TEST_FRAMEWORK_HPP
#define MK_TEST_FRAMEWORK_HPP

#include <exception>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace test_framework {

struct TestCase {
    const char* name;
    std::function<void()> function;
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

struct Registrar {
    Registrar(const char* name, std::function<void()> function) {
        registry().push_back({name, std::move(function)});
    }
};

class Failure : public std::runtime_error {
public:
    explicit Failure(const std::string& message) : std::runtime_error(message) {}
};

inline void require(bool condition, const char* expression, const char* file, int line) {
    if (!condition) {
        throw Failure(
            std::string(file) + ":" + std::to_string(line) +
            ": assertion failed: " + expression);
    }
}

template <typename A, typename B>
void require_equal(
    const A& actual,
    const B& expected,
    const char* actual_expression,
    const char* expected_expression,
    const char* file,
    int line) {
    if (!(actual == expected)) {
        throw Failure(
            std::string(file) + ":" + std::to_string(line) +
            ": expected " + expected_expression + ", got " + actual_expression);
    }
}

template <typename Function>
void require_throws(
    Function&& function,
    const char* file,
    int line) {
    bool thrown = false;
    try {
        function();
    } catch (const std::exception&) {
        thrown = true;
    }
    if (!thrown) {
        throw Failure(
            std::string(file) + ":" + std::to_string(line) +
            ": expected exception");
    }
}

inline int run_all() {
    if (registry().empty()) {
        std::cerr << "No tests registered\n";
        return 1;
    }

    int failed = 0;
    for (const TestCase& test : registry()) {
        try {
            test.function();
            std::cout << "[PASS] " << test.name << "\n";
        } catch (const std::exception& error) {
            ++failed;
            std::cerr << "[FAIL] " << test.name << ": " << error.what() << "\n";
        } catch (...) {
            ++failed;
            std::cerr << "[FAIL] " << test.name << ": unknown exception\n";
        }
    }

    std::cout << "Executed: " << registry().size()
              << ", failed: " << failed << "\n";
    return failed == 0 ? 0 : 1;
}

}

#define TEST(test_identifier) \
    static void test_identifier(); \
    static ::test_framework::Registrar registrar_##test_identifier( \
        #test_identifier, test_identifier); \
    static void test_identifier()

#define REQUIRE(expression) \
    ::test_framework::require((expression), #expression, __FILE__, __LINE__)

#define REQUIRE_EQUAL(actual, expected) \
    ::test_framework::require_equal( \
        (actual), (expected), #actual, #expected, __FILE__, __LINE__)

#define REQUIRE_THROWS(expression) \
    ::test_framework::require_throws([&]() { (expression); }, __FILE__, __LINE__)

#endif
