cmake_minimum_required(VERSION 3.16)
project(moex_spectra_pipeline VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# --- Disable dependency CLI, tests, and benchmark ---
set(MOEX_FAST_BUILD_CLI OFF CACHE BOOL "" FORCE)
set(MOEX_FAST_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(MOEX_FAST_BUILD_BENCHMARK OFF CACHE BOOL "" FORCE)
set(MOEX_RAW_BUILD_CLI OFF CACHE BOOL "" FORCE)
set(MOEX_RAW_BUILD_TESTS OFF CACHE BOOL "" FORCE)

# --- Pull in moex_fast (provides moex_decode, moex_spectra_transport) via separate binary dir ---
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../moex_fast
                 ${CMAKE_CURRENT_BINARY_DIR}/_deps/moex_fast_build)

# --- Pull in moex_raw (provides moex_raw_lib) via separate binary dir ---
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../moex_raw
                 ${CMAKE_CURRENT_BINARY_DIR}/_deps/moex_raw_build)

# --- Pipeline static library ---
add_library(moex_spectra_pipeline STATIC
    src/ordered_decode.cpp
    src/sequence_reset_probe.cpp
    src/normal_replay_pipeline.cpp
)
target_include_directories(moex_spectra_pipeline PUBLIC include)
target_link_libraries(moex_spectra_pipeline
    PUBLIC moex_decode moex_spectra_transport moex_raw_lib
)

# --- CTest targets ---
enable_testing()

add_executable(test_ordered_decode tests/test_ordered_decode.cpp)
target_link_libraries(test_ordered_decode PRIVATE moex_spectra_pipeline)
target_include_directories(test_ordered_decode PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../moex_fast/tests
)
add_test(NAME test_ordered_decode COMMAND test_ordered_decode)

add_executable(test_sequence_reset_probe tests/test_sequence_reset_probe.cpp)
target_link_libraries(test_sequence_reset_probe PRIVATE moex_spectra_pipeline)
target_include_directories(test_sequence_reset_probe PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../moex_fast/tests
)
add_test(NAME test_sequence_reset_probe COMMAND test_sequence_reset_probe)

add_executable(test_normal_replay_pipeline tests/test_normal_replay_pipeline.cpp)
target_link_libraries(test_normal_replay_pipeline PRIVATE moex_spectra_pipeline)
target_include_directories(test_normal_replay_pipeline PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../moex_fast/tests
    ${CMAKE_CURRENT_SOURCE_DIR}/../moex_raw/include
)
add_test(NAME test_normal_replay_pipeline COMMAND test_normal_replay_pipeline)
