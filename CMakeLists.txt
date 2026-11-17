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
    storage/src/payroll_service.cpp
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

add_library(mk_gui_money_formatter STATIC
    gui/src/money_formatter.cpp
    gui/include/money_formatter.hpp
)

target_include_directories(mk_gui_money_formatter PUBLIC gui/include)
target_link_libraries(mk_gui_money_formatter PUBLIC mk_core Qt6::Core)

if(MSVC)
    target_compile_options(mk_gui_money_formatter PRIVATE /W4 /WX)
else()
    target_compile_options(mk_gui_money_formatter PRIVATE -Wall -Wextra -Werror)
endif()

add_library(mk_gui_format STATIC
    gui/src/gui_format.cpp
    gui/include/gui_format.hpp
)

target_include_directories(mk_gui_format PUBLIC gui/include)
target_link_libraries(mk_gui_format PUBLIC mk_core Qt6::Core)

if(MSVC)
    target_compile_options(mk_gui_format PRIVATE /W4 /WX)
else()
    target_compile_options(mk_gui_format PRIVATE -Wall -Wextra -Werror)
endif()

add_library(mk_gui_employee_model STATIC
    gui/src/employee_table_model.cpp
    gui/include/employee_table_model.hpp
)

target_include_directories(mk_gui_employee_model PUBLIC gui/include)
target_link_libraries(mk_gui_employee_model PUBLIC Qt6::Core)

if(MSVC)
    target_compile_options(mk_gui_employee_model PRIVATE /W4 /WX)
else()
    target_compile_options(mk_gui_employee_model PRIVATE -Wall -Wextra -Werror)
endif()

add_library(mk_gui_timesheet_model STATIC
    gui/src/timesheet_table_model.cpp
    gui/include/timesheet_table_model.hpp
)

target_include_directories(mk_gui_timesheet_model PUBLIC gui/include)
target_link_libraries(mk_gui_timesheet_model PUBLIC mk_core Qt6::Core Qt6::Gui)

if(MSVC)
    target_compile_options(mk_gui_timesheet_model PRIVATE /W4 /WX)
else()
    target_compile_options(mk_gui_timesheet_model PRIVATE -Wall -Wextra -Werror)
endif()

add_library(mk_gui_payroll_model STATIC
    gui/src/payroll_table_model.cpp
    gui/include/payroll_table_model.hpp
)

target_include_directories(mk_gui_payroll_model PUBLIC gui/include)
target_link_libraries(mk_gui_payroll_model PUBLIC
    mk_gui_money_formatter
    mk_gui_format
    Qt6::Core
)

if(MSVC)
    target_compile_options(mk_gui_payroll_model PRIVATE /W4 /WX)
else()
    target_compile_options(mk_gui_payroll_model PRIVATE -Wall -Wextra -Werror)
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
    storage/tests/employee_list_tests.cpp
    storage/tests/employee_operations_tests.cpp
    storage/tests/timesheet_tests.cpp
    storage/tests/payroll_service_tests.cpp
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

add_executable(gui_tests
    core/tests/main.cpp
    gui/tests/money_formatter_tests.cpp
    gui/tests/gui_format_tests.cpp
    gui/tests/employee_table_model_tests.cpp
    gui/tests/timesheet_table_model_tests.cpp
    gui/tests/payroll_table_model_tests.cpp
)

target_include_directories(gui_tests PRIVATE core/tests)
target_link_libraries(gui_tests PRIVATE
    mk_gui_money_formatter
    mk_gui_format
    mk_gui_employee_model
    mk_gui_timesheet_model
    mk_gui_payroll_model
)

if(MSVC)
    target_compile_options(gui_tests PRIVATE /W4 /WX)
else()
    target_compile_options(gui_tests PRIVATE -Wall -Wextra -Werror)
endif()

add_executable(mk_app
    gui/src/main.cpp
    gui/src/app_theme.cpp
    gui/src/main_window.cpp
    gui/src/navigation_sidebar.cpp
    gui/src/section_page.cpp
    gui/src/workspace_tabs.cpp
    gui/include/app_theme.hpp
    gui/include/main_window.hpp
    gui/include/navigation_sidebar.hpp
    gui/include/section_page.hpp
    gui/include/workspace_tabs.hpp
)

target_include_directories(mk_app PRIVATE gui/include)
target_link_libraries(mk_app PRIVATE
    mk_storage
    mk_core
    mk_gui_money_formatter
    mk_gui_format
    mk_gui_employee_model
    mk_gui_timesheet_model
    mk_gui_payroll_model
    Qt6::Widgets
)

enable_testing()
add_test(NAME core_tests COMMAND core_tests)
add_test(NAME storage_tests COMMAND storage_tests)
add_test(NAME gui_tests COMMAND gui_tests)
