#ifdef _MSC_VER
#pragma warning(disable : 4189 4101)
#endif

#include "moex_raw/raw_segment.hpp"
#include "moex_raw/raw_types.hpp"
#include "moex_raw/sha256.hpp"
#include <cassert>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

static std::string temp_dir() {
    static int counter = 0;
    auto dir = fs::temp_directory_path() / "moex_raw_test" / ("stream_set_" + std::to_string(counter++));
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

    // Positive: 3-segment stream set validates
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
        assert(paths.size() == 3);

        std::vector<RawSegmentMetadata> metas;
        std::vector<RawFooter> footers;
        std::vector<RawValidationIssue> issues;
        assert(validate_stream_set(paths, metas, footers, issues) == SegmentStatus::ValidFinalized);
        assert(metas.size() == 3);
        assert(footers.size() == 3);

        // Contiguous capture indexes across segments
        assert(footers[0].last_capture_index + 1 == footers[1].first_capture_index);
        assert(footers[1].last_capture_index + 1 == footers[2].first_capture_index);
    }

    // Negative: duplicate segment_index
    {
        auto dir = temp_dir();
        auto meta = make_meta();

        // Create two segments with same index by manipulating metadata
        RawSegmentWriter w1(meta, dir, {});
        w1.open();
        RawPacketRecord rec;
        rec.capture_index = 0;
        rec.capture_monotonic_ns = 0;
        w1.append(rec);
        w1.finalize();

        // Copy the file to create a duplicate
        auto paths = w1.finalized_paths();
        auto dup_path = paths[0] + ".dup";
        fs::copy(paths[0], dup_path);

        std::vector<RawSegmentMetadata> metas;
        std::vector<RawFooter> footers;
        std::vector<RawValidationIssue> issues;
        auto result = validate_stream_set({paths[0], dup_path}, metas, footers, issues);
        assert(result != SegmentStatus::ValidFinalized);
        (void)result;
    }

    // Directory grouping
    {
        auto dir = temp_dir();

        // Create two independent stream sets
        auto meta1 = make_meta();
        meta1.source.source_id = 1;
        meta1.source.channel_id = 1;

        auto meta2 = make_meta();
        meta2.source.source_id = 2;
        meta2.source.channel_id = 2;

        RawSegmentWriter w1(meta1, dir, {});
        w1.open();
        RawPacketRecord rec;
        rec.capture_index = 0;
        rec.capture_monotonic_ns = 0;
        w1.append(rec);
        w1.finalize();

        RawSegmentWriter w2(meta2, dir, {});
        w2.open();
        rec.capture_index = 0;
        w2.append(rec);
        w2.finalize();

        auto sets = group_stream_sets(dir);
        assert(sets.size() == 2);
    }

    // Partial files not included in stream sets
    {
        auto dir = temp_dir();
        auto meta = make_meta();

        // Create a valid segment
        RawSegmentWriter w(meta, dir, {});
        w.open();
        RawPacketRecord rec;
        rec.capture_index = 0;
        rec.capture_monotonic_ns = 0;
        w.append(rec);
        w.finalize();

        // Create a partial file (write some bytes)
        auto partial_path = dir + "/test_partial.mxraw.partial";
        {
            std::FILE* f = std::fopen(partial_path.c_str(), "wb");
            std::uint8_t data[] = {1, 2, 3};
            std::fwrite(data, 1, 3, f);
            std::fclose(f);
        }

        auto sets = group_stream_sets(dir);
        // Should only have 1 stream set (the valid one)
        assert(sets.size() == 1);
    }

    std::cout << "test_stream_set: ALL PASSED\n";
    return 0;
}
