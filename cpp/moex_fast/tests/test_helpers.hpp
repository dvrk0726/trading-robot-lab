#pragma once
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <string>

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
        std::system("if not exist ..\\..\\..\\build\\temp mkdir ..\\..\\..\\build\\temp");
        dir_created = true;
    }
    std::ofstream ofs(temp_path(filename).c_str());
    ofs << content;
}
