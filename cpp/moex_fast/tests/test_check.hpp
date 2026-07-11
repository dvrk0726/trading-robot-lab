#pragma once
// Release-active test assertion macros for decoder tests.
#include <cstdlib>
#include <iostream>
#include <string>
#include <cstdint>
#include <type_traits>

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

// Overload for (std::string, std::string)
inline void check_eq(const std::string& a, const std::string& b,
                     const char* a_expr, const char* b_expr,
                     const char* file, int line) {
    if (a != b) {
        fail_eq(a_expr, b_expr, "\"" + a + "\"", "\"" + b + "\"", file, line);
    }
}

// Overload for (std::string, const char*)
inline void check_eq(const std::string& a, const char* b,
                     const char* a_expr, const char* b_expr,
                     const char* file, int line) {
    if (a != std::string(b)) {
        fail_eq(a_expr, b_expr, "\"" + a + "\"", "\"" + std::string(b) + "\"", file, line);
    }
}

// Overload for (const char*, std::string)
inline void check_eq(const char* a, const std::string& b,
                     const char* a_expr, const char* b_expr,
                     const char* file, int line) {
    if (std::string(a) != b) {
        fail_eq(a_expr, b_expr, "\"" + std::string(a) + "\"", "\"" + b + "\"", file, line);
    }
}

// Overload for (const char*, const char*)
inline void check_eq(const char* a, const char* b,
                     const char* a_expr, const char* b_expr,
                     const char* file, int line) {
    if (std::string(a) != std::string(b)) {
        fail_eq(a_expr, b_expr, std::string(a), std::string(b), file, line);
    }
}

// Generic overload for numeric types
template<typename A, typename B>
inline typename std::enable_if<!std::is_convertible<A, std::string>::value &&
                               !std::is_convertible<B, std::string>::value>::type
check_eq(const A& a, const B& b, const char* a_expr, const char* b_expr,
         const char* file, int line) {
    if (!(a == b)) {
        fail_eq(a_expr, b_expr, std::to_string(a), std::to_string(b), file, line);
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

#define TEST_PASS(name) std::cout << "PASS: " << (name) << "\n"
