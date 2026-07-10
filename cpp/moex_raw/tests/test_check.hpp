#pragma once
// Release-active test assertion macros.
// Unlike <cassert>/assert(), these remain active under NDEBUG / Release builds.

#include <cstdlib>
#include <iostream>
#include <string>
#include <cstdint>

namespace test_check_detail {

inline void fail(const char* expr, const char* file, int line) {
    std::cerr << "CHECK FAILED: " << expr << "\n  at " << file << ":" << line << "\n";
    std::exit(1);
}

inline void fail_eq(const char* a_expr, const char* b_expr,
                    const std::string& a_val, const std::string& b_val,
                    const char* file, int line) {
    std::cerr << "CHECK FAILED: " << a_expr << " == " << b_expr
              << "\n  left:  " << a_val
              << "\n  right: " << b_val
              << "\n  at " << file << ":" << line << "\n";
    std::exit(1);
}

template<typename A, typename B>
inline void check_eq(const A& a, const B& b, const char* a_expr, const char* b_expr,
                     const char* file, int line) {
    if (!(a == b)) {
        fail_eq(a_expr, b_expr, std::to_string(a), std::to_string(b), file, line);
    }
}

inline void check_eq(const std::string& a, const std::string& b,
                     const char* a_expr, const char* b_expr,
                     const char* file, int line) {
    if (!(a == b)) {
        fail_eq(a_expr, b_expr, "\"" + a + "\"", "\"" + b + "\"", file, line);
    }
}

inline void check_eq(const char* a, const char* b,
                     const char* a_expr, const char* b_expr,
                     const char* file, int line) {
    if (std::string(a) != std::string(b)) {
        fail_eq(a_expr, b_expr, std::string(a), std::string(b), file, line);
    }
}

}  // namespace test_check_detail

#define CHECK(expr) \
    do { if (!(expr)) test_check_detail::fail(#expr, __FILE__, __LINE__); } while (0)

#define CHECK_EQ(a, b) \
    test_check_detail::check_eq((a), (b), #a, #b, __FILE__, __LINE__)

#define CHECK_NE(a, b) \
    do { if ((a) == (b)) test_check_detail::fail(#a " != " #b, __FILE__, __LINE__); } while (0)

#define CHECK_LT(a, b) \
    do { if (!((a) < (b))) test_check_detail::fail(#a " < " #b, __FILE__, __LINE__); } while (0)

#define CHECK_LE(a, b) \
    do { if (!((a) <= (b))) test_check_detail::fail(#a " <= " #b, __FILE__, __LINE__); } while (0)

#define CHECK_GT(a, b) \
    do { if (!((a) > (b))) test_check_detail::fail(#a " > " #b, __FILE__, __LINE__); } while (0)

#define CHECK_GE(a, b) \
    do { if (!((a) >= (b))) test_check_detail::fail(#a " >= " #b, __FILE__, __LINE__); } while (0)
