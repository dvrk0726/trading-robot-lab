cmake_minimum_required(VERSION 3.16)
project(moex_raw VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /WX)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# --- Library ---
add_library(moex_raw_lib STATIC
    src/crc32c.cpp
    src/sha256.cpp
    src/endian.cpp
    src/strings.cpp
    src/raw_types.cpp
    src/raw_segment_writer.cpp
    src/raw_segment_reader.cpp
    src/raw_replay.cpp
    src/raw_report.cpp
)
target_include_directories(moex_raw_lib PUBLIC include)

# --- CLI ---
add_executable(moex-raw src/main.cpp)
target_link_libraries(moex-raw PRIVATE moex_raw_lib)

# --- Tests ---
enable_testing()

add_executable(test_crc32c tests/test_crc32c.cpp)
target_link_libraries(test_crc32c PRIVATE moex_raw_lib)
add_test(NAME test_crc32c COMMAND test_crc32c)

add_executable(test_endian tests/test_endian.cpp)
target_link_libraries(test_endian PRIVATE moex_raw_lib)
add_test(NAME test_endian COMMAND test_endian)

add_executable(test_strings tests/test_strings.cpp)
target_link_libraries(test_strings PRIVATE moex_raw_lib)
add_test(NAME test_strings COMMAND test_strings)

add_executable(test_header_contract tests/test_header_contract.cpp)
target_link_libraries(test_header_contract PRIVATE moex_raw_lib)
add_test(NAME test_header_contract COMMAND test_header_contract)

add_executable(test_record_contract tests/test_record_contract.cpp)
target_link_libraries(test_record_contract PRIVATE moex_raw_lib)
add_test(NAME test_record_contract COMMAND test_record_contract)

add_executable(test_footer_validation tests/test_footer_validation.cpp)
target_link_libraries(test_footer_validation PRIVATE moex_raw_lib)
add_test(NAME test_footer_validation COMMAND test_footer_validation)

add_executable(test_writer_lifecycle tests/test_writer_lifecycle.cpp)
target_link_libraries(test_writer_lifecycle PRIVATE moex_raw_lib)
add_test(NAME test_writer_lifecycle COMMAND test_writer_lifecycle)

add_executable(test_rotation tests/test_rotation.cpp)
target_link_libraries(test_rotation PRIVATE moex_raw_lib)
add_test(NAME test_rotation COMMAND test_rotation)

add_executable(test_stream_set tests/test_stream_set.cpp)
target_link_libraries(test_stream_set PRIVATE moex_raw_lib)
add_test(NAME test_stream_set COMMAND test_stream_set)

add_executable(test_replay tests/test_replay.cpp)
target_link_libraries(test_replay PRIVATE moex_raw_lib)
add_test(NAME test_replay COMMAND test_replay)

add_executable(test_cli tests/test_cli.cpp)
target_link_libraries(test_cli PRIVATE moex_raw_lib)
target_compile_definitions(test_cli PRIVATE
    MOEX_RAW_PATH="$<TARGET_FILE:moex-raw>"
)
add_test(NAME test_cli COMMAND test_cli)

add_executable(test_json_report tests/test_json_report.cpp)
target_link_libraries(test_json_report PRIVATE moex_raw_lib)
add_test(NAME test_json_report COMMAND test_json_report)

add_executable(test_resource_safety tests/test_resource_safety.cpp)
target_link_libraries(test_resource_safety PRIVATE moex_raw_lib)
add_test(NAME test_resource_safety COMMAND test_resource_safety)

add_executable(test_golden_record_layout tests/test_golden_record_layout.cpp)
target_link_libraries(test_golden_record_layout PRIVATE moex_raw_lib)
add_test(NAME test_golden_record_layout COMMAND test_golden_record_layout)

add_executable(test_content_sha256_e2e tests/test_content_sha256_e2e.cpp)
target_link_libraries(test_content_sha256_e2e PRIVATE moex_raw_lib)
add_test(NAME test_content_sha256_e2e COMMAND test_content_sha256_e2e)

add_executable(test_round3 tests/test_round3.cpp)
target_link_libraries(test_round3 PRIVATE moex_raw_lib)
add_test(NAME test_round3 COMMAND test_round3)

add_executable(test_round5 tests/test_round5.cpp)
target_link_libraries(test_round5 PRIVATE moex_raw_lib)
add_test(NAME test_round5 COMMAND test_round5)
