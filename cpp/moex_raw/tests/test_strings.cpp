#ifdef _MSC_VER
#pragma warning(disable : 4189 4101)
#endif

#include "moex_raw/strings.hpp"
#include <cassert>
#include <iostream>

int main() {
    using namespace moex_raw;

    // Valid UTF-8
    assert(validate_utf8_string("hello"));
    assert(validate_utf8_string(""));

    // 128-byte string accepted
    assert(validate_utf8_string(std::string(128, 'x')));

    // 129-byte string rejected
    assert(!validate_utf8_string(std::string(129, 'x')));

    // Embedded NUL rejected
    assert(!validate_utf8_string(std::string("hello\0world", 11)));

    // Valid 2-byte UTF-8
    assert(validate_utf8_string("\xC3\xA9"));

    // Valid 3-byte UTF-8
    assert(validate_utf8_string("\xE2\x82\xAC"));

    // Invalid continuation byte
    assert(!validate_utf8_string("\x80"));

    // Overlong 2-byte
    assert(!validate_utf8_string("\xC0\x80"));

    // Length string round-trip
    {
        std::vector<std::uint8_t> buf;
        write_length_string(buf, "test");
        assert(buf.size() == 6);

        std::string out;
        std::size_t consumed = 0;
        assert(read_length_string(buf.data(), buf.size(), out, consumed));
        assert(out == "test");
        assert(consumed == 6);
    }

    // Empty string
    {
        std::vector<std::uint8_t> buf;
        write_length_string(buf, "");
        assert(buf.size() == 2);

        std::string out;
        std::size_t consumed = 0;
        assert(read_length_string(buf.data(), buf.size(), out, consumed));
        assert(out.empty());
        assert(consumed == 2);
    }

    // Truncated read
    {
        std::vector<std::uint8_t> buf = {0x04};
        std::string out;
        std::size_t consumed = 0;
        assert(!read_length_string(buf.data(), buf.size(), out, consumed));
        (void)consumed;
    }

    std::cout << "test_strings: ALL PASSED\n";
    return 0;
}
