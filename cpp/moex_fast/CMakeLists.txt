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

# --- RT-1 Inspector Library ---
add_library(moex_inspect STATIC
    src/inspect_types.cpp
    src/xml_parser.cpp
    src/inspector.cpp
    src/report.cpp
    src/sha256.cpp
)
target_include_directories(moex_inspect PUBLIC include)
target_link_libraries(moex_inspect PUBLIC pugixml)

# --- RT-3 Decoder Library ---
add_library(moex_decode STATIC
    src/decoder_types.cpp
    src/wire_cursor.cpp
    src/fast_compiler.cpp
    src/fast_decoder.cpp
    src/decoder_report.cpp
)
target_include_directories(moex_decode PUBLIC include)
target_link_libraries(moex_decode PUBLIC pugixml)

# --- RT-1 Inspector CLI ---
add_executable(moex-fast-inspect src/main.cpp)
target_link_libraries(moex-fast-inspect PRIVATE moex_inspect)

# --- RT-3 Decoder CLI ---
add_executable(moex-fast-decode src/decode_main.cpp)
target_link_libraries(moex-fast-decode PRIVATE moex_decode)

# --- RT-4 A1 SPECTRA UDP Transport Library (no dependency on moex_decode or pugixml) ---
add_library(moex_spectra_transport STATIC
    src/spectra_udp_framing.cpp
)
target_include_directories(moex_spectra_transport PUBLIC include)

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

# --- RT-3 Decoder Tests ---
add_executable(test_decoder_primitives tests/test_decoder_primitives.cpp)
target_link_libraries(test_decoder_primitives PRIVATE moex_decode)
add_test(NAME test_decoder_primitives COMMAND test_decoder_primitives)

add_executable(test_decoder_compiler tests/test_decoder_compiler.cpp)
target_link_libraries(test_decoder_compiler PRIVATE moex_decode)
add_test(NAME test_decoder_compiler COMMAND test_decoder_compiler WORKING_DIRECTORY ${MOEX_FAST_TEST_DIR})

add_executable(test_decoder_session tests/test_decoder_session.cpp)
target_link_libraries(test_decoder_session PRIVATE moex_decode)
add_test(NAME test_decoder_session COMMAND test_decoder_session WORKING_DIRECTORY ${MOEX_FAST_TEST_DIR})

add_executable(test_decoder_operators tests/test_decoder_operators.cpp)
target_link_libraries(test_decoder_operators PRIVATE moex_decode)
add_test(NAME test_decoder_operators COMMAND test_decoder_operators WORKING_DIRECTORY ${MOEX_FAST_TEST_DIR})

add_executable(test_decoder_sequences tests/test_decoder_sequences.cpp)
target_link_libraries(test_decoder_sequences PRIVATE moex_decode)
add_test(NAME test_decoder_sequences COMMAND test_decoder_sequences WORKING_DIRECTORY ${MOEX_FAST_TEST_DIR})

add_executable(test_decoder_rollback tests/test_decoder_rollback.cpp)
target_link_libraries(test_decoder_rollback PRIVATE moex_decode)
add_test(NAME test_decoder_rollback COMMAND test_decoder_rollback WORKING_DIRECTORY ${MOEX_FAST_TEST_DIR})

add_executable(test_decoder_cli tests/test_decoder_cli.cpp)
target_link_libraries(test_decoder_cli PRIVATE moex_decode)
target_compile_definitions(test_decoder_cli PRIVATE
    MOEX_FAST_DECODE_PATH="$<TARGET_FILE:moex-fast-decode>"
)
add_test(NAME test_decoder_cli COMMAND test_decoder_cli WORKING_DIRECTORY ${MOEX_FAST_TEST_DIR})

add_executable(test_decoder_limits tests/test_decoder_limits.cpp)
target_link_libraries(test_decoder_limits PRIVATE moex_decode)
add_test(NAME test_decoder_limits COMMAND test_decoder_limits)

# --- RT-3 Reference Oracle (independent, no production linkage) ---
add_executable(test_decoder_reference_oracle tests/test_decoder_reference_oracle.cpp)
target_include_directories(test_decoder_reference_oracle PRIVATE ${MOEX_FAST_TEST_DIR})
add_test(NAME test_decoder_reference_oracle COMMAND test_decoder_reference_oracle)

# --- RT-4 A1 SPECTRA UDP Framing Test ---
add_executable(test_spectra_udp_framing tests/test_spectra_udp_framing.cpp)
target_link_libraries(test_spectra_udp_framing PRIVATE moex_spectra_transport)
add_test(NAME test_spectra_udp_framing COMMAND test_spectra_udp_framing)
