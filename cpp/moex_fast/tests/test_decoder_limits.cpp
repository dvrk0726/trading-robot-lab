#include "moex_fast/fast_compiler.hpp"
#include "moex_fast/fast_decoder.hpp"
#include "moex_fast/wire_cursor.hpp"
#include "test_check.hpp"
#include <iostream>
#include <vector>
#include <limits>

using namespace moex_fast;

static std::vector<std::uint8_t> hex(const char* h) {
    std::vector<std::uint8_t> bytes;
    for (int i = 0; h[i] && h[i + 1]; i += 2) {
        auto hv = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return 0;
        };
        bytes.push_back(static_cast<std::uint8_t>(hv(h[i]) * 16 + hv(h[i + 1])));
    }
    return bytes;
}

static CompiledTemplateSet compile(const char* xml) {
    auto r = compile_templates_from_string(xml);
    CHECK(r.ok);
    return r.compiled;
}

static void check_sentinel_unchanged(const std::vector<std::uint8_t>& v) {
    CHECK_EQ(v.size(), 2u);
    CHECK_EQ(v[0], 0xDEu);
    CHECK_EQ(v[1], 0xADu);
}

// Test: max_message_bytes enforcement
static void test_max_message_bytes() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="10" name="Msg">
    <uInt32 name="Val" id="1"/>
  </template>
</templates>)";

    auto ts = compile(xml);

    DecodeLimits limits;
    limits.max_message_bytes = 2;  // Very small limit

    DecoderSession session(ts, limits);

    // A valid message is 3 bytes (pmap + tmpl-id + val), but limit is 2
    auto msg = hex("C0" "8A" "81");
    auto r = session.decode_exact(msg.data(), msg.size());
    CHECK(r.status == DecodeStatus::LimitExceeded);
    CHECK(!r.issues.empty());
    CHECK_EQ(r.issues[0].code, "message_size_limit");

    TEST_PASS("max_message_bytes");
}

// Test: hard ceiling enforcement (limits above hard ceilings are clamped)
static void test_hard_ceilings() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="10" name="Msg">
    <uInt32 name="Val" id="1"/>
  </template>
</templates>)";

    auto ts = compile(xml);

    // Request limits above hard ceilings
    DecodeLimits limits;
    limits.max_message_bytes = 100 * 1024 * 1024;  // 100 MiB, above 1 MiB hard ceiling
    limits.max_presence_map_bytes = 1000;            // Above 64 hard ceiling
    limits.max_sequence_entries = 1000000;            // Above 100000 hard ceiling
    limits.max_total_nodes = 10000000;                // Above 1000000 hard ceiling
    limits.max_string_bytes = 100 * 1024 * 1024;     // Above 1 MiB hard ceiling

    // Constructor should clamp to hard ceilings
    DecoderSession session(ts, limits);

    // A 3-byte message should decode fine with clamped limits
    auto msg = hex("C0" "8A" "81");
    auto r = session.decode_exact(msg.data(), msg.size());
    CHECK(r.status == DecodeStatus::Ok);
    CHECK_EQ(std::get<std::uint64_t>(r.message.fields[0].value), 1u);

    TEST_PASS("hard_ceilings");
}

// Test: presence map limit
static void test_pmap_limit() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="10" name="Msg">
    <uInt32 name="Val" id="1"/>
  </template>
</templates>)";

    auto ts = compile(xml);

    DecodeLimits limits;
    limits.max_presence_map_bytes = 1;  // Only 7 bits allowed

    DecoderSession session(ts, limits);

    // Normal message with 1-bit pmap should work
    auto msg = hex("C0" "8A" "81");
    auto r = session.decode_exact(msg.data(), msg.size());
    CHECK(r.status == DecodeStatus::Ok);

    TEST_PASS("pmap_limit");
}

// Test: string size limit (mandatory and nullable ASCII)
static void test_string_limit() {
    // Mandatory: 100-char string, limit=50 -> LimitExceeded
    {
        std::vector<std::uint8_t> bytes(100, 0x41);
        bytes.back() = 0xC1;  // stop bit on last byte
        WireCursor c(bytes.data(), bytes.size());
        std::string val = "sentinel";
        CHECK(c.read_ascii_string(val, 50) == DecodeStatus::LimitExceeded);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val, "sentinel");
    }
    // Mandatory: 100-char string, limit=100 -> Ok
    {
        std::vector<std::uint8_t> bytes(100, 0x41);
        bytes.back() = 0xC1;
        WireCursor c(bytes.data(), bytes.size());
        std::string val;
        CHECK(c.read_ascii_string(val, 100) == DecodeStatus::Ok);
        CHECK_EQ(val.size(), 100u);
    }
    // Mandatory: limit=0, empty -> Ok
    {
        auto bytes = hex("80");
        WireCursor c(bytes.data(), bytes.size());
        std::string val = "sentinel";
        CHECK(c.read_ascii_string(val, 0) == DecodeStatus::Ok);
        CHECK(val.empty());
        CHECK_EQ(c.position(), 1u);
    }
    // Mandatory: limit=0, non-empty -> LimitExceeded
    {
        auto bytes = hex("C1");
        WireCursor c(bytes.data(), bytes.size());
        std::string val = "sentinel";
        CHECK(c.read_ascii_string(val, 0) == DecodeStatus::LimitExceeded);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val, "sentinel");
    }
    // Nullable: NULL at limit=0 -> Ok, is_null=true
    {
        auto bytes = hex("80");
        WireCursor c(bytes.data(), bytes.size());
        std::string val = "sentinel";
        bool is_null = false;
        CHECK(c.read_nullable_ascii(val, is_null, 0) == DecodeStatus::Ok);
        CHECK(is_null);
        CHECK_EQ(val, "sentinel");
    }
    // Nullable: empty at limit=0 -> Ok, is_null=false
    {
        auto bytes = hex("0080");
        WireCursor c(bytes.data(), bytes.size());
        std::string val = "sentinel";
        bool is_null = true;
        CHECK(c.read_nullable_ascii(val, is_null, 0) == DecodeStatus::Ok);
        CHECK(!is_null);
        CHECK(val.empty());
    }
    // Nullable: non-empty at limit=0 -> LimitExceeded
    {
        auto bytes = hex("C1");
        WireCursor c(bytes.data(), bytes.size());
        std::string val = "sentinel";
        bool is_null = true;
        CHECK(c.read_nullable_ascii(val, is_null, 0) == DecodeStatus::LimitExceeded);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val, "sentinel");
        CHECK(is_null);
    }
    TEST_PASS("string_limit");
}

// Test: nullable u32 NULL [80] and overlong [00 80]
static void test_nullable_u32_null_and_overlong() {
    // [80] is NULL (stop-bit 0)
    {
        auto bytes = hex("80");
        WireCursor c(bytes.data(), bytes.size());
        std::uint32_t val = 0;
        bool is_null = false;
        CHECK(c.read_nullable_u32(val, is_null) == DecodeStatus::Ok);
        CHECK(is_null);
        CHECK_EQ(c.position(), 1u);
    }
    // [00 80] is non-canonical (2-byte encoding of raw=0, fits in 1 byte)
    {
        auto bytes = hex("0080");
        WireCursor c(bytes.data(), bytes.size());
        std::uint32_t val = 0;
        bool is_null = false;
        CHECK(c.read_nullable_u32(val, is_null) == DecodeStatus::NonCanonicalEncoding);
        CHECK_EQ(c.position(), 0u);
    }

    TEST_PASS("nullable_u32_null_and_overlong");
}

// Test: signed integer max-width validation
static void test_signed_max_width() {
    // INT32_MAX: 5 bytes
    // 0x7FFFFFFF in 35 bits: 000_0111_1111_1111_1111_1111_1111_1111_1111
    // 7-bit groups: 0000111, 1111111, 1111111, 1111111, 1111111
    // Bytes: 0x07, 0x7F, 0x7F, 0x7F, 0xFF (stop on last)
    {
        auto bytes = hex("077F7F7FFF");
        WireCursor c(bytes.data(), bytes.size());
        std::int32_t val = 0;
        CHECK(c.read_stopbit_i32(val) == DecodeStatus::Ok);
        CHECK_EQ(val, std::numeric_limits<std::int32_t>::max());
    }
    // INT32_MIN: 5 bytes
    // -2147483648 in 35-bit: 111_10000000_00000000_00000000_00000000
    // 7-bit groups: 1111000, 0000000, 0000000, 0000000, 0000000
    // Bytes: 0x78, 0x00, 0x00, 0x00, 0x80 (stop on last)
    {
        auto bytes = hex("7800000080");
        WireCursor c(bytes.data(), bytes.size());
        std::int32_t val = 0;
        CHECK(c.read_stopbit_i32(val) == DecodeStatus::Ok);
        CHECK_EQ(val, std::numeric_limits<std::int32_t>::min());
    }
    // INT64_MAX: 10 bytes
    // 0x7FFFFFFFFFFFFFFF in 70-bit: 0_0000000_1111111_1111111_1111111_1111111_1111111_1111111_1111111_1111111_1111111
    // 7-bit groups: 0000000, 1111111, 1111111, 1111111, 1111111, 1111111, 1111111, 1111111, 1111111, 1111111+stop
    // Bytes: 0x00, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0xFF
    {
        auto bytes = hex("007F7F7F7F7F7F7F7FFF");
        WireCursor c(bytes.data(), bytes.size());
        std::int64_t val = 0;
        CHECK(c.read_stopbit_i64(val) == DecodeStatus::Ok);
        CHECK_EQ(val, std::numeric_limits<std::int64_t>::max());
    }
    // INT64_MIN: 10 bytes
    // 70-bit 2's complement sign extension: 1111111_0000000_0000000_0000000_0000000_0000000_0000000_0000000_0000000_0000000+stop
    // Bytes: 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80
    {
        auto bytes = hex("7F000000000000000080");
        WireCursor c(bytes.data(), bytes.size());
        std::int64_t val = 0;
        CHECK(c.read_stopbit_i64(val) == DecodeStatus::Ok);
        CHECK_EQ(val, std::numeric_limits<std::int64_t>::min());
    }
    TEST_PASS("signed_max_width");
}

// Test: Unicode string size limit (mandatory and nullable)
static void test_unicode_limit() {
    // Mandatory: empty [80] at limit=0 -> Ok
    {
        auto bytes = hex("80");
        WireCursor c(bytes.data(), bytes.size());
        std::string val = "sentinel";
        CHECK(c.read_unicode_string(val, 0) == DecodeStatus::Ok);
        CHECK(val.empty());
        CHECK_EQ(c.position(), 1u);
    }
    // Mandatory: U+00A2 [82 C2 A2] at limit=2 -> Ok
    {
        auto bytes = hex("82C2A2");
        WireCursor c(bytes.data(), bytes.size());
        std::string val;
        CHECK(c.read_unicode_string(val, 2) == DecodeStatus::Ok);
        CHECK_EQ(val.size(), 2u);
    }
    // Mandatory: U+00A2 [82 C2 A2] at limit=1 -> LimitExceeded, rollback
    {
        auto bytes = hex("82C2A2");
        WireCursor c(bytes.data(), bytes.size());
        std::string val = "sentinel";
        CHECK(c.read_unicode_string(val, 1) == DecodeStatus::LimitExceeded);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val, "sentinel");
    }
    // Mandatory: [82 C2] at limit=1 -> LimitExceeded (not NeedMoreData)
    // Limit precedence: limit checked before body availability
    {
        auto bytes = hex("82C2");
        WireCursor c(bytes.data(), bytes.size());
        std::string val = "sentinel";
        CHECK(c.read_unicode_string(val, 1) == DecodeStatus::LimitExceeded);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val, "sentinel");
    }
    // Nullable: NULL [80] at limit=0 -> Ok, is_null=true
    {
        auto bytes = hex("80");
        WireCursor c(bytes.data(), bytes.size());
        std::string val = "sentinel";
        bool is_null = false;
        CHECK(c.read_nullable_unicode(val, is_null, 0) == DecodeStatus::Ok);
        CHECK(is_null);
        CHECK_EQ(val, "sentinel");
    }
    // Nullable: empty [81] at limit=0 -> Ok, is_null=false, string empty
    {
        auto bytes = hex("81");
        WireCursor c(bytes.data(), bytes.size());
        std::string val = "sentinel";
        bool is_null = true;
        CHECK(c.read_nullable_unicode(val, is_null, 0) == DecodeStatus::Ok);
        CHECK(!is_null);
        CHECK(val.empty());
    }
    // Nullable: U+00A2 [83 C2 A2] at limit=2 -> Ok
    {
        auto bytes = hex("83C2A2");
        WireCursor c(bytes.data(), bytes.size());
        std::string val;
        bool is_null = true;
        CHECK(c.read_nullable_unicode(val, is_null, 2) == DecodeStatus::Ok);
        CHECK(!is_null);
        CHECK_EQ(val.size(), 2u);
    }
    // Nullable: U+00A2 [83 C2 A2] at limit=1 -> LimitExceeded
    {
        auto bytes = hex("83C2A2");
        WireCursor c(bytes.data(), bytes.size());
        std::string val = "sentinel";
        bool is_null = true;
        CHECK(c.read_nullable_unicode(val, is_null, 1) == DecodeStatus::LimitExceeded);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val, "sentinel");
        CHECK(is_null);
    }
    // Nullable: [83 C2] at limit=1 -> LimitExceeded (not NeedMoreData)
    // Limit precedence: limit checked before body availability
    {
        auto bytes = hex("83C2");
        WireCursor c(bytes.data(), bytes.size());
        std::string val = "sentinel";
        bool is_null = true;
        CHECK(c.read_nullable_unicode(val, is_null, 1) == DecodeStatus::LimitExceeded);
        CHECK_EQ(c.position(), 0u);
        CHECK_EQ(val, "sentinel");
        CHECK(is_null);
    }
    TEST_PASS("unicode_limit");
}

// Test: byte vector size limit (mandatory and nullable)
static void test_byte_vector_limit() {
    // Mandatory: empty [80] at limit=0 -> Ok
    {
        auto bytes = hex("80");
        WireCursor c(bytes.data(), bytes.size());
        std::vector<std::uint8_t> val{0xDE, 0xAD};
        CHECK(c.read_byte_vector(val, 0) == DecodeStatus::Ok);
        CHECK(val.empty());
        CHECK_EQ(c.position(), 1u);
    }
    // Mandatory: one byte [81 AA] at limit=0 -> LimitExceeded
    {
        auto bytes = hex("81AA");
        WireCursor c(bytes.data(), bytes.size());
        std::vector<std::uint8_t> val{0xDE, 0xAD};
        CHECK(c.read_byte_vector(val, 0) == DecodeStatus::LimitExceeded);
        CHECK_EQ(c.position(), 0u);
        check_sentinel_unchanged(val);
    }
    // Mandatory: len=3 at limit=3 -> Ok
    {
        auto bytes = hex("83AABBCC");
        WireCursor c(bytes.data(), bytes.size());
        std::vector<std::uint8_t> val;
        CHECK(c.read_byte_vector(val, 3) == DecodeStatus::Ok);
        CHECK_EQ(val.size(), 3u);
    }
    // Mandatory: len=3 at limit=2 -> LimitExceeded
    {
        auto bytes = hex("83AABBCC");
        WireCursor c(bytes.data(), bytes.size());
        std::vector<std::uint8_t> val{0xDE, 0xAD};
        CHECK(c.read_byte_vector(val, 2) == DecodeStatus::LimitExceeded);
        CHECK_EQ(c.position(), 0u);
        check_sentinel_unchanged(val);
    }
    // Mandatory: truncated [83 AA] at limit=2 -> LimitExceeded (not NeedMoreData)
    {
        auto bytes = hex("83AA");
        WireCursor c(bytes.data(), bytes.size());
        std::vector<std::uint8_t> val{0xDE, 0xAD};
        CHECK(c.read_byte_vector(val, 2) == DecodeStatus::LimitExceeded);
        CHECK_EQ(c.position(), 0u);
        check_sentinel_unchanged(val);
    }
    // Nullable: NULL [80] at limit=0 -> Ok, is_null=true
    {
        auto bytes = hex("80");
        WireCursor c(bytes.data(), bytes.size());
        std::vector<std::uint8_t> val{0xDE, 0xAD};
        bool is_null = false;
        CHECK(c.read_nullable_byte_vector(val, is_null, 0) == DecodeStatus::Ok);
        CHECK(is_null);
        check_sentinel_unchanged(val);
    }
    // Nullable: empty [81] at limit=0 -> Ok, is_null=false, empty vector
    {
        auto bytes = hex("81");
        WireCursor c(bytes.data(), bytes.size());
        std::vector<std::uint8_t> val{0xDE, 0xAD};
        bool is_null = true;
        CHECK(c.read_nullable_byte_vector(val, is_null, 0) == DecodeStatus::Ok);
        CHECK(!is_null);
        CHECK(val.empty());
    }
    // Nullable: one byte [82 AA] at limit=0 -> LimitExceeded
    {
        auto bytes = hex("82AA");
        WireCursor c(bytes.data(), bytes.size());
        std::vector<std::uint8_t> val{0xDE, 0xAD};
        bool is_null = true;
        CHECK(c.read_nullable_byte_vector(val, is_null, 0) == DecodeStatus::LimitExceeded);
        CHECK_EQ(c.position(), 0u);
        check_sentinel_unchanged(val);
        CHECK(is_null);
    }
    // Nullable: len=3 at limit=3 -> Ok
    {
        auto bytes = hex("84AABBCC");
        WireCursor c(bytes.data(), bytes.size());
        std::vector<std::uint8_t> val;
        bool is_null = true;
        CHECK(c.read_nullable_byte_vector(val, is_null, 3) == DecodeStatus::Ok);
        CHECK(!is_null);
        CHECK_EQ(val.size(), 3u);
    }
    // Nullable: len=3 at limit=2 -> LimitExceeded
    {
        auto bytes = hex("84AABBCC");
        WireCursor c(bytes.data(), bytes.size());
        std::vector<std::uint8_t> val{0xDE, 0xAD};
        bool is_null = true;
        CHECK(c.read_nullable_byte_vector(val, is_null, 2) == DecodeStatus::LimitExceeded);
        CHECK_EQ(c.position(), 0u);
        check_sentinel_unchanged(val);
        CHECK(is_null);
    }
    // Nullable: truncated [84 AA] at limit=2 -> LimitExceeded (not NeedMoreData)
    {
        auto bytes = hex("84AA");
        WireCursor c(bytes.data(), bytes.size());
        std::vector<std::uint8_t> val{0xDE, 0xAD};
        bool is_null = true;
        CHECK(c.read_nullable_byte_vector(val, is_null, 2) == DecodeStatus::LimitExceeded);
        CHECK_EQ(c.position(), 0u);
        check_sentinel_unchanged(val);
        CHECK(is_null);
    }
    // Checked-length: nullable prefix [10 00 00 00 80] = non-null length UINT32_MAX,
    // default max_bytes should return LimitExceeded without allocation
    {
        auto bytes = hex("1000000080");
        WireCursor c(bytes.data(), bytes.size());
        std::vector<std::uint8_t> val{0xDE, 0xAD};
        bool is_null = true;
        CHECK(c.read_nullable_byte_vector(val, is_null) == DecodeStatus::LimitExceeded);
        CHECK_EQ(c.position(), 0u);
        check_sentinel_unchanged(val);
        CHECK(is_null);
    }
    TEST_PASS("byte_vector_limit");
}

// Test: cursor restore on failure
static void test_cursor_restore() {
    // Try to read a uInt32 from truncated data
    auto bytes = hex("01");  // continuation byte with no stop
    WireCursor c(bytes.data(), bytes.size());
    std::uint32_t val = 0;
    CHECK(c.read_stopbit_u32(val) == DecodeStatus::NeedMoreData);
    CHECK_EQ(c.position(), 0u);  // cursor restored

    // Try nullable i32 null from empty data
    WireCursor c2(nullptr, 0);
    std::int32_t val2 = 0;
    bool is_null = false;
    CHECK(c2.read_nullable_i32(val2, is_null) == DecodeStatus::NeedMoreData);

    TEST_PASS("cursor_restore");
}

// Test: two independent sessions sharing one compiled template set
static void test_session_independence() {
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<templates>
  <template id="10" name="Msg">
    <uInt32 name="Val" id="1"><copy/></uInt32>
  </template>
</templates>)";

    auto ts = compile(xml);

    DecoderSession s1(ts);
    DecoderSession s2(ts);

    // Set value in s1
    auto msg1 = hex("E0" "8A" "B7");  // Val=55
    s1.decode_exact(msg1.data(), msg1.size());

    // s2 should not see s1's state
    auto msg2 = hex("E0" "8A" "81");  // Val=1
    auto r2 = s2.decode_exact(msg2.data(), msg2.size());
    CHECK(r2.status == DecodeStatus::Ok);
    CHECK_EQ(std::get<std::uint64_t>(r2.message.fields[0].value), 1u);

    // s1 should still have its own state
    auto msg3 = hex("80");  // copy previous = 55
    auto r3 = s1.decode_exact(msg3.data(), msg3.size());
    CHECK(r3.status == DecodeStatus::Ok);
    CHECK(r3.message.fields[0].source == ValueSource::Copy);
    CHECK_EQ(std::get<std::uint64_t>(r3.message.fields[0].value), 55u);

    TEST_PASS("session_independence");
}

int main() {
    test_max_message_bytes();
    test_hard_ceilings();
    test_pmap_limit();
    test_string_limit();
    test_unicode_limit();
    test_byte_vector_limit();
    test_nullable_u32_null_and_overlong();
    test_signed_max_width();
    test_cursor_restore();
    test_session_independence();
    std::cout << "All decoder limits tests passed.\n";
    return 0;
}
