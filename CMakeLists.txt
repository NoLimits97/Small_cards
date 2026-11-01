cmake_minimum_required(VERSION 3.21)

project(malyye_kadry LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(mk_core STATIC
    core/src/types.cpp
    core/src/formula.cpp
    core/src/calendar.cpp
    core/src/slicing.cpp
    core/src/time_derive.cpp
    core/src/dependency_graph.cpp
)

target_include_directories(mk_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/core/include
)

if(MSVC)
    target_compile_options(mk_core PRIVATE /W4 /WX)
else()
    target_compile_options(mk_core PRIVATE -Wall -Wextra -Werror)
endif()

add_executable(core_tests
    core/tests/main.cpp
    core/tests/types_tests.cpp
    core/tests/formula_tests.cpp
    core/tests/model_tests.cpp
    core/tests/calendar_tests.cpp
    core/tests/slicing_tests.cpp
    core/tests/time_derive_tests.cpp
    core/tests/dependency_graph_tests.cpp
)

target_include_directories(core_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/core/tests
)

target_link_libraries(core_tests PRIVATE mk_core)

if(MSVC)
    target_compile_options(core_tests PRIVATE /W4 /WX)
else()
    target_compile_options(core_tests PRIVATE -Wall -Wextra -Werror)
endif()

enable_testing()
add_test(NAME core_tests COMMAND core_tests)
