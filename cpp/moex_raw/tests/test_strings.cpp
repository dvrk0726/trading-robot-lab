#ifdef _MSC_VER
#pragma warning(disable : 4189 4101)
#endif

#include "moex_raw/strings.hpp"
#include "test_check.hpp"
#include <iostream>

int main() {
    using namespace moex_raw;

    // Valid UTF-8
    CHECK(validate_utf8_string("hello"));
    CHECK(validate_utf8_string(""));

    // 128-byte string accepted
    CHECK(validate_utf8_string(std::string(128, 'x')));

    // 129-byte string rejected
    CHECK(!validate_utf8_string(std::string(129, 'x')));

    // Embedded NUL rejected
    CHECK(!validate_utf8_string(std::string("hello\0world", 11)));

    // Valid 2-byte UTF-8
    CHECK(validate_utf8_string("\xC3\xA9"));

    // Valid 3-byte UTF-8
    CHECK(validate_utf8_string("\xE2\x82\xAC"));

    // Invalid continuation byte
    CHECK(!validate_utf8_string("\x80"));

    // Overlong 2-byte
    CHECK(!validate_utf8_string("\xC0\x80"));

    // Length string round-trip
    {
        std::vector<std::uint8_t> buf;
        write_length_string(buf, "test");
        CHECK(buf.size() == 6);

        std::string out;
        std::size_t consumed = 0;
        CHECK(read_length_string(buf.data(), buf.size(), out, consumed));
        CHECK(out == "test");
        CHECK(consumed == 6);
    }

    // Empty string
    {
        std::vector<std::uint8_t> buf;
        write_length_string(buf, "");
        CHECK(buf.size() == 2);

        std::string out;
        std::size_t consumed = 0;
        CHECK(read_length_string(buf.data(), buf.size(), out, consumed));
        CHECK(out.empty());
        CHECK(consumed == 2);
    }

    // Truncated read
    {
        std::vector<std::uint8_t> buf = {0x04};
        std::string out;
        std::size_t consumed = 0;
        CHECK(!read_length_string(buf.data(), buf.size(), out, consumed));
        (void)consumed;
    }

    std::cout << "test_strings: ALL PASSED\n";
    return 0;
}
