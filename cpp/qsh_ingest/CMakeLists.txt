cmake_minimum_required(VERSION 3.16)
project(qsh_ingest VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Sanitizer options for debug builds
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)

if(MSVC)
    add_compile_options(/W4 /WX)
    if(ENABLE_ASAN)
        add_compile_options(/fsanitize=address)
    endif()
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
    if(ENABLE_ASAN)
        add_compile_options(-fsanitize=address)
        add_link_options(-fsanitize=address)
    endif()
    if(ENABLE_UBSAN)
        add_compile_options(-fsanitize=undefined)
        add_link_options(-fsanitize=undefined)
    endif()
endif()

# --- zlib (for gzip/deflate) ---
# On Windows with vcpkg: -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake
find_package(ZLIB REQUIRED)

# --- Library ---
add_library(qsh_lib STATIC
    src/qsh_header.cpp
    src/leb128.cpp
    src/qsh_reader.cpp
    src/ordlog_reader.cpp
    src/quotes_reader.cpp
    src/deals_reader.cpp
    src/auxinfo_reader.cpp
    src/order_book.cpp
    src/l2_snapshot.cpp
    src/data_quality.cpp
)
target_include_directories(qsh_lib PUBLIC include)
target_link_libraries(qsh_lib PUBLIC ZLIB::ZLIB)

# --- CLI ---
add_executable(qsh_ingest src/main.cpp)
target_link_libraries(qsh_ingest PRIVATE qsh_lib)

# --- Tests ---
enable_testing()

add_executable(test_leb128 tests/test_leb128.cpp)
target_link_libraries(test_leb128 PRIVATE qsh_lib)
add_test(NAME test_leb128 COMMAND test_leb128)

add_executable(test_qsh_header tests/test_qsh_header.cpp)
target_link_libraries(test_qsh_header PRIVATE qsh_lib)
add_test(NAME test_qsh_header COMMAND test_qsh_header)

add_executable(test_ordlog_flags tests/test_ordlog_flags.cpp)
target_link_libraries(test_ordlog_flags PRIVATE qsh_lib)
add_test(NAME test_ordlog_flags COMMAND test_ordlog_flags)

add_executable(test_order_book tests/test_order_book.cpp)
target_link_libraries(test_order_book PRIVATE qsh_lib)
add_test(NAME test_order_book COMMAND test_order_book)

add_executable(test_l2_diagnostics tests/test_l2_diagnostics.cpp)
target_link_libraries(test_l2_diagnostics PRIVATE qsh_lib)
add_test(NAME test_l2_diagnostics COMMAND test_l2_diagnostics)

add_executable(test_snapshot_mode tests/test_snapshot_mode.cpp)
target_link_libraries(test_snapshot_mode PRIVATE qsh_lib)
add_test(NAME test_snapshot_mode COMMAND test_snapshot_mode)

add_executable(test_best_level_orders tests/test_best_level_orders.cpp)
target_link_libraries(test_best_level_orders PRIVATE qsh_lib)
add_test(NAME test_best_level_orders COMMAND test_best_level_orders)

add_executable(test_tracing_side_effects tests/test_tracing_side_effects.cpp)
target_link_libraries(test_tracing_side_effects PRIVATE qsh_lib)
add_test(NAME test_tracing_side_effects COMMAND test_tracing_side_effects)

add_executable(test_fill_semantics tests/test_fill_semantics.cpp)
target_link_libraries(test_fill_semantics PRIVATE qsh_lib)
add_test(NAME test_fill_semantics COMMAND test_fill_semantics)

add_executable(test_m10g_diagnostics tests/test_m10g_diagnostics.cpp)
target_link_libraries(test_m10g_diagnostics PRIVATE qsh_lib)
add_test(NAME test_m10g_diagnostics COMMAND test_m10g_diagnostics)
