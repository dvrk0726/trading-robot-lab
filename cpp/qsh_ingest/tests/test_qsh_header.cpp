#include "qsh/qsh_header.hpp"
#include "qsh/leb128.hpp"
#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>

using namespace qsh;

// Build a minimal valid QSH header in a byte buffer.
static std::vector<uint8_t> build_test_header() {
    std::vector<uint8_t> data;

    // Signature: "QScalp History Data"
    const uint8_t sig[] = {0x51, 0x53, 0x63, 0x61, 0x6c, 0x70, 0x20, 0x48,
                           0x69, 0x73, 0x74, 0x6f, 0x72, 0x79, 0x20, 0x44, 0x61, 0x74, 0x61};
    data.insert(data.end(), sig, sig + 19);

    // Version = 4
    data.push_back(4);

    // Recorder: "TestRecorder" (LEB128 length + bytes)
    auto push_string = [&](const std::string& s) {
        // LEB128 encode length
        uint64_t len = s.size();
        while (len > 0) {
            uint8_t byte = len & 0x7F;
            len >>= 7;
            if (len > 0) byte |= 0x80;
            data.push_back(byte);
        }
        data.insert(data.end(), s.begin(), s.end());
    };

    push_string("TestRecorder");
    push_string("TestComment");

    // Recording time: int64 LE = 637200251900000000
    int64_t rec_time = 637200251900000000LL;
    for (int i = 0; i < 8; ++i) {
        data.push_back(static_cast<uint8_t>(rec_time >> (8 * i)));
    }

    // Stream count = 1
    data.push_back(1);

    // Stream type = OrderLog (0x70)
    data.push_back(0x70);

    // Instrument: "Plaza2:RI-3.21"
    push_string("Plaza2:RI-3.21");

    return data;
}

static void test_valid_header() {
    auto data = build_test_header();
    auto result = parse_qsh_header(data.data(), data.size());
    assert(result.valid);
    assert(result.header.version == 4);
    assert(result.header.stream == StreamType::OrderLog);
    assert(result.header.instrument == "Plaza2:RI-3.21");
    assert(result.header.recorder == "TestRecorder");
    assert(result.header.comment == "TestComment");
    assert(result.header.recording_time == 637200251900000000LL);
    std::cout << "  PASS: valid header parsing" << std::endl;
}

static void test_bad_signature() {
    auto data = build_test_header();
    data[0] = 0x00;  // corrupt first byte
    auto result = parse_qsh_header(data.data(), data.size());
    assert(!result.valid);
    assert(result.error.find("signature") != std::string::npos);
    std::cout << "  PASS: bad signature rejected" << std::endl;
}

static void test_bad_version() {
    auto data = build_test_header();
    data[19] = 3;  // version = 3 instead of 4
    auto result = parse_qsh_header(data.data(), data.size());
    assert(!result.valid);
    assert(result.error.find("version") != std::string::npos);
    std::cout << "  PASS: bad version rejected" << std::endl;
}

static void test_empty_streams() {
    auto data = build_test_header();
    // Find stream_count byte and set to 0
    // It's after signature(19) + version(1) + recorder + comment + time(8)
    // We need to find it. Let's just build a variant.
    data.clear();
    const uint8_t sig[] = {0x51, 0x53, 0x63, 0x61, 0x6c, 0x70, 0x20, 0x48,
                           0x69, 0x73, 0x74, 0x6f, 0x72, 0x79, 0x20, 0x44, 0x61, 0x74, 0x61};
    data.insert(data.end(), sig, sig + 19);
    data.push_back(4);  // version
    data.push_back(1); data.push_back('R');  // recorder "R"
    data.push_back(1); data.push_back('C');  // comment "C"
    for (int i = 0; i < 8; ++i) data.push_back(0);  // time
    data.push_back(0);  // stream_count = 0

    auto result = parse_qsh_header(data.data(), data.size());
    assert(!result.valid);
    assert(result.error.find("no data streams") != std::string::npos);
    std::cout << "  PASS: empty streams rejected" << std::endl;
}

static void test_too_small() {
    uint8_t data[] = {0x51, 0x53};
    auto result = parse_qsh_header(data, 2);
    assert(!result.valid);
    std::cout << "  PASS: too-small file rejected" << std::endl;
}

static void test_dotnet_ticks() {
    // Just verify it doesn't crash
    std::string s = dotnet_ticks_to_string(637200251900000000LL);
    assert(!s.empty());
    std::cout << "  PASS: dotnet_ticks_to_string works" << std::endl;
}

int main() {
    std::cout << "=== test_qsh_header ===" << std::endl;
    test_valid_header();
    test_bad_signature();
    test_bad_version();
    test_empty_streams();
    test_too_small();
    test_dotnet_ticks();
    std::cout << "\nAll QSH header tests passed." << std::endl;
    return 0;
}
