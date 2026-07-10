cmake_minimum_required(VERSION 3.16)
project(moex_fast VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# --- pugixml (lightweight XML parser, MIT license) ---
include(FetchContent)
FetchContent_Declare(
    pugixml
    URL https://github.com/zeux/pugixml/archive/refs/tags/v1.14.tar.gz
)
set(PUGIXML_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(PUGIXML_BUILD_SHARED OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(pugixml)

# --- Library ---
add_library(moex_inspect STATIC
    src/inspect_types.cpp
    src/xml_parser.cpp
    src/inspector.cpp
    src/report.cpp
    src/sha256.cpp
)
target_include_directories(moex_inspect PUBLIC include)
target_link_libraries(moex_inspect PUBLIC pugixml)

# --- CLI ---
add_executable(moex-fast-inspect src/main.cpp)
target_link_libraries(moex-fast-inspect PRIVATE moex_inspect)

# --- Tests ---
enable_testing()

set(MOEX_FAST_TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tests)

add_executable(test_template_parser tests/test_template_parser.cpp)
target_link_libraries(test_template_parser PRIVATE moex_inspect)
add_test(NAME test_template_parser COMMAND test_template_parser WORKING_DIRECTORY ${MOEX_FAST_TEST_DIR})

add_executable(test_config_parser tests/test_config_parser.cpp)
target_link_libraries(test_config_parser PRIVATE moex_inspect)
add_test(NAME test_config_parser COMMAND test_config_parser WORKING_DIRECTORY ${MOEX_FAST_TEST_DIR})

add_executable(test_provenance tests/test_provenance.cpp)
target_link_libraries(test_provenance PRIVATE moex_inspect)
add_test(NAME test_provenance COMMAND test_provenance WORKING_DIRECTORY ${MOEX_FAST_TEST_DIR})

add_executable(test_deterministic_report tests/test_deterministic_report.cpp)
target_link_libraries(test_deterministic_report PRIVATE moex_inspect)
target_compile_definitions(test_deterministic_report PRIVATE
    MOEX_FAST_INSPECT_PATH="$<TARGET_FILE:moex-fast-inspect>"
)
add_test(NAME test_deterministic_report COMMAND test_deterministic_report WORKING_DIRECTORY ${MOEX_FAST_TEST_DIR})

add_executable(test_resource_safety tests/test_resource_safety.cpp)
target_link_libraries(test_resource_safety PRIVATE moex_inspect)
add_test(NAME test_resource_safety COMMAND test_resource_safety WORKING_DIRECTORY ${MOEX_FAST_TEST_DIR})

# CLI integration tests need to find the moex-fast-inspect binary.
add_executable(test_cli tests/test_cli.cpp)
target_link_libraries(test_cli PRIVATE moex_inspect)
target_compile_definitions(test_cli PRIVATE
    MOEX_FAST_INSPECT_PATH="$<TARGET_FILE:moex-fast-inspect>"
)
add_test(NAME test_cli COMMAND test_cli WORKING_DIRECTORY ${MOEX_FAST_TEST_DIR})
