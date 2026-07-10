#pragma once
#include <cstdlib>
#include <iostream>
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
