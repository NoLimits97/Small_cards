cmake_minimum_required(VERSION 3.21)

project(malyye_kadry LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Core Sql Widgets)

add_library(mk_core STATIC
    core/src/types.cpp
    core/src/formula.cpp
    core/src/calendar.cpp
    core/src/slicing.cpp
    core/src/time_derive.cpp
    core/src/dependency_graph.cpp
    core/src/engine.cpp
)

target_include_directories(mk_core PUBLIC core/include)

if(MSVC)
    target_compile_options(mk_core PRIVATE /W4 /WX)
else()
    target_compile_options(mk_core PRIVATE -Wall -Wextra -Werror)
endif()

add_library(mk_storage STATIC
    storage/src/database.cpp
    storage/src/repositories.cpp
    storage/src/org_details.cpp
    storage/storage_resources.qrc
)

set_target_properties(mk_storage PROPERTIES AUTORCC ON)

target_include_directories(mk_storage
    PUBLIC
        core/include
        storage/include
)

target_link_libraries(mk_storage
    PUBLIC
        mk_core
        Qt6::Core
        Qt6::Sql
)

if(MSVC)
    target_compile_options(mk_storage PRIVATE /W4 /WX)
else()
    target_compile_options(mk_storage PRIVATE -Wall -Wextra -Werror)
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
    core/tests/engine_tests.cpp
)

target_include_directories(core_tests PRIVATE core/tests)
target_link_libraries(core_tests PRIVATE mk_core)

if(MSVC)
    target_compile_options(core_tests PRIVATE /W4 /WX)
else()
    target_compile_options(core_tests PRIVATE -Wall -Wextra -Werror)
endif()

add_executable(storage_tests
    storage/tests/main.cpp
    storage/tests/storage_tests.cpp
    storage/tests/org_details_tests.cpp
)

target_include_directories(storage_tests PRIVATE
    core/tests
    storage/include
)

target_link_libraries(storage_tests PRIVATE mk_storage)

if(MSVC)
    target_compile_options(storage_tests PRIVATE /W4 /WX)
else()
    target_compile_options(storage_tests PRIVATE -Wall -Wextra -Werror)
endif()

add_executable(mk_app
    gui/src/main.cpp
    gui/src/app_theme.cpp
    gui/src/main_window.cpp
    gui/include/app_theme.hpp
    gui/include/main_window.hpp
)

target_include_directories(mk_app PRIVATE gui/include)
target_link_libraries(mk_app PRIVATE mk_storage mk_core Qt6::Widgets)

enable_testing()
add_test(NAME core_tests COMMAND core_tests)
add_test(NAME storage_tests COMMAND storage_tests)
