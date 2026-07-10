#ifdef _MSC_VER
#pragma warning(disable : 4189 4101)
#endif

#include "moex_raw/raw_segment.hpp"
#include "moex_raw/raw_types.hpp"
#include "moex_raw/sha256.hpp"
#include "test_check.hpp"
#include <iostream>
#include <filesystem>
#include <cstring>
#include <vector>
#include <cstdio>

namespace fs = std::filesystem;

static std::string temp_dir() {
    static int counter = 0;
    auto base = fs::temp_directory_path() / "moex_raw_test";
    auto dir = base / ("content_sha_" + std::to_string(counter++));
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir);
    return dir.string();
}

static moex_raw::RawSegmentMetadata make_meta() {
    moex_raw::RawSegmentMetadata meta;
    for (int i = 0; i < 16; ++i) meta.session.session_id[i] = static_cast<std::uint8_t>(i + 1);
    meta.session.feed_group = "ORDERS-LOG";
    meta.session.endpoint_role = "Incremental";
    meta.session.source_label = "test";
    meta.source.clock_domain = moex_raw::ClockDomain::Synthetic;
    meta.source.transport = moex_raw::Transport::Synthetic;
    meta.source.source_side = moex_raw::SourceSide::None;
    meta.source.source_id = 1;
    meta.source.channel_id = 1;
    moex_raw::sha256("c", 1, meta.source.configuration_sha256);
    moex_raw::sha256("t", 1, meta.source.templates_sha256);
    moex_raw::sha256("f", 1, meta.source.endpoint_fingerprint_sha256);
    meta.created_utc_ns = 1700000000000000000ULL;
    return meta;
}

int main() {
    using namespace moex_raw;

    // End-to-end: content SHA-256 matches independently computed hash
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentWriter writer(meta, dir, {});
        writer.open();

        for (std::uint64_t i = 0; i < 5; ++i) {
            RawPacketRecord rec;
            rec.record_flags = kRecordFlagUtcValid;
            rec.capture_index = i;
            rec.capture_utc_ns = 1000 + i;
            rec.capture_monotonic_ns = i * 1000;
            rec.payload = {static_cast<std::uint8_t>(i), 0xAB, 0xCD};
            writer.append(rec);
        }
        writer.finalize();

        auto paths = writer.finalized_paths();
        CHECK(paths.size() == 1);

        // Read the file and independently compute content SHA-256
        std::FILE* f = std::fopen(paths[0].c_str(), "rb");
        CHECK(f != nullptr);

        std::fseek(f, 0, SEEK_END);
        auto file_size = static_cast<std::size_t>(std::ftell(f));
        std::fseek(f, 0, SEEK_SET);

        std::vector<std::uint8_t> file_data(file_size);
        CHECK(std::fread(file_data.data(), 1, file_size, f) == file_size);
        std::fclose(f);

        // Footer is last 92 bytes
        CHECK(file_size > kFooterSize);
        std::size_t content_size = file_size - kFooterSize;

        // Independently hash everything before the footer
        std::uint8_t independent_hash[32];
        sha256(file_data.data(), content_size, independent_hash);
        std::string independent_hex = sha256_bytes_to_hex(independent_hash);

        // Read stored hash from footer
        RawFooter footer;
        std::vector<RawValidationIssue> issues;
        CHECK(deserialize_footer(file_data.data() + content_size, kFooterSize, footer, issues));
        std::string stored_hex = sha256_bytes_to_hex(footer.content_sha256);

        // They must match
        CHECK(independent_hex == stored_hex);

        // Also verify via validate_segment
        RawSegmentMetadata vmeta;
        RawFooter vfooter;
        std::vector<RawValidationIssue> vissues;
        std::string content_hex, file_hex;
        auto status = validate_segment(paths[0], vmeta, vfooter, vissues, content_hex, file_hex);
        CHECK(status == SegmentStatus::ValidFinalized);
        CHECK(content_hex == stored_hex);
    }

    // Multi-segment: each segment has correct content SHA-256
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentRotationPolicy pol;
        pol.max_records_per_segment = 3;

        RawSegmentWriter writer(meta, dir, pol);
        writer.open();

        for (std::uint64_t i = 0; i < 9; ++i) {
            RawPacketRecord rec;
            rec.capture_index = i;
            rec.capture_monotonic_ns = i * 1000;
            rec.payload = {static_cast<std::uint8_t>(i)};
            writer.append(rec);
        }
        writer.finalize();

        auto paths = writer.finalized_paths();
        CHECK(paths.size() == 3);

        for (const auto& path : paths) {
            RawSegmentMetadata vmeta;
            RawFooter vfooter;
            std::vector<RawValidationIssue> vissues;
            std::string content_hex, file_hex;
            auto status = validate_segment(path, vmeta, vfooter, vissues, content_hex, file_hex);
            CHECK(status == SegmentStatus::ValidFinalized);
            CHECK(!content_hex.empty());
            CHECK(content_hex.size() == 64);
        }
    }

    std::cout << "test_content_sha256_e2e: ALL PASSED\n";
    return 0;
}
