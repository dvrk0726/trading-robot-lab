#ifdef _MSC_VER
#pragma warning(disable : 4189 4101)
#endif

#include "moex_raw/raw_segment.hpp"
#include "moex_raw/raw_types.hpp"
#include "moex_raw/sha256.hpp"
#include "test_check.hpp"
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

static std::string temp_dir() {
    static int counter = 0;
    auto base = fs::temp_directory_path() / "moex_raw_test";
    auto dir = base / ("rotation_" + std::to_string(counter++));
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

    // Rotation by max_records
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
            rec.capture_monotonic_ns = i;
            rec.payload = {static_cast<std::uint8_t>(i)};
            CHECK(writer.append(rec).empty());
        }

        // Finalize last segment
        writer.finalize();

        // Should have 3 segments (9 records / 3 per segment)
        CHECK(writer.finalized_paths().size() == 3);
    }

    // Rotation preserves contiguous capture_index
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentRotationPolicy pol;
        pol.max_records_per_segment = 2;

        RawSegmentWriter writer(meta, dir, pol);
        writer.open();

        for (std::uint64_t i = 0; i < 6; ++i) {
            RawPacketRecord rec;
            rec.capture_index = i;
            rec.capture_monotonic_ns = i * 1000;
            writer.append(rec);
        }
        writer.finalize();

        CHECK(writer.finalized_paths().size() == 3);

        // Validate each segment
        for (const auto& path : writer.finalized_paths()) {
            RawSegmentMetadata m;
            RawFooter f;
            std::vector<RawValidationIssue> issues;
            std::string ch, fh;
            CHECK(validate_segment(path, m, f, issues, ch, fh) == SegmentStatus::ValidFinalized);
        }
    }

    // Canonical filename format
    {
        std::uint8_t sid[16] = {0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,
                                 0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,0x10};
        auto name = canonical_filename(sid, 42, 7, 0);
        CHECK(name.find("_src") != std::string::npos);
        CHECK(name.find("_ch") != std::string::npos);
        CHECK(name.find("_seg") != std::string::npos);
        CHECK(name.find(".mxraw") != std::string::npos);
    }

    // Parse segment index from filename
    {
        std::uint8_t sid[16] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
        auto name = canonical_filename(sid, 1, 1, 0xABCD);
        std::uint64_t parsed = 0;
        CHECK(parse_segment_index_from_filename(name, parsed));
        CHECK(parsed == 0xABCD);
    }

    // Identical input produces byte-identical files
    {
        auto dir1 = temp_dir();
        auto dir2 = temp_dir();
        auto meta1 = make_meta();
        auto meta2 = make_meta();

        RawSegmentWriter w1(meta1, dir1, {});
        w1.open();
        RawSegmentWriter w2(meta2, dir2, {});
        w2.open();

        for (std::uint64_t i = 0; i < 5; ++i) {
            RawPacketRecord rec;
            rec.capture_index = i;
            rec.capture_monotonic_ns = i;
            rec.payload = {static_cast<std::uint8_t>(i), 0xFF};
            w1.append(rec);
            w2.append(rec);
        }
        w1.finalize();
        w2.finalize();

        // Read both files and compare
        CHECK(w1.finalized_paths().size() == 1);
        CHECK(w2.finalized_paths().size() == 1);

        std::FILE* f1 = std::fopen(w1.finalized_paths()[0].c_str(), "rb");
        std::FILE* f2 = std::fopen(w2.finalized_paths()[0].c_str(), "rb");
        CHECK(f1 && f2);

        std::fseek(f1, 0, SEEK_END);
        std::fseek(f2, 0, SEEK_END);
        auto sz1 = std::ftell(f1);
        auto sz2 = std::ftell(f2);
        CHECK(sz1 == sz2);

        std::fseek(f1, 0, SEEK_SET);
        std::fseek(f2, 0, SEEK_SET);
        std::vector<std::uint8_t> b1(sz1), b2(sz2);
        std::fread(b1.data(), 1, sz1, f1);
        std::fread(b2.data(), 1, sz2, f2);
        std::fclose(f1);
        std::fclose(f2);
        CHECK(b1 == b2);
    }

    std::cout << "test_rotation: ALL PASSED\n";
    return 0;
}
