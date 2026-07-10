#pragma once
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>

// CHECK macros that remain active in Release builds (unlike assert).
// On failure they print the file:line and expression, then exit(1).

#define CHECK(expr)                                                     \
    do {                                                                \
        if (!(expr)) {                                                  \
            std::cerr << "CHECK FAILED: " << #expr                      \
                      << "\n  at " << __FILE__ << ":" << __LINE__       \
                      << "\n";                                          \
            std::exit(1);                                               \
        }                                                               \
    } while (0)

#define CHECK_MSG(expr, msg)                                            \
    do {                                                                \
        if (!(expr)) {                                                  \
            std::cerr << "CHECK FAILED: " << #expr                      \
                      << "\n  " << (msg)                                \
                      << "\n  at " << __FILE__ << ":" << __LINE__       \
                      << "\n";                                          \
            std::exit(1);                                               \
        }                                                               \
    } while (0)

#define CHECK_EQ(a, b) CHECK((a) == (b))
#define CHECK_NE(a, b) CHECK((a) != (b))

#define TEST_PASS(name) std::cout << "PASS: " << (name) << "\n"

// Temporary file helpers — writes go to build/temp (gitignored via build/).
// This keeps runtime-generated test artifacts out of version control.

inline std::string temp_path(const char* filename) {
    return std::string("../../../build/temp/") + filename;
}

inline void write_temp_file(const char* filename, const char* content) {
    static bool dir_created = false;
    if (!dir_created) {
        std::error_code ec;
        if (!std::filesystem::create_directories("../../../build/temp", ec)) {
            if (ec) {
                std::cerr << "FAILED to create temp directory: " << ec.message() << "\n";
                std::exit(1);
            }
        }
        dir_created = true;
    }
    auto path = temp_path(filename);
    std::ofstream ofs(path);
    if (!ofs.is_open()) {
        std::cerr << "FAILED to open temp file for writing: " << path << "\n";
        std::exit(1);
    }
    ofs << content;
    if (ofs.fail()) {
        std::cerr << "FAILED to write temp file: " << path << "\n";
        std::exit(1);
    }
}
