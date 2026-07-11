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

namespace fs = std::filesystem;

static std::string temp_dir() {
    static int counter = 0;
    auto base = fs::temp_directory_path() / "moex_raw_test";
    auto dir = base / ("writer_lifecycle_" + std::to_string(counter++));
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

    // Created -> Open -> append -> Finalized
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentRotationPolicy pol;
        RawSegmentWriter writer(meta, dir, pol);

        CHECK(writer.state() == WriterState::Created);
        CHECK(writer.open().empty());
        CHECK(writer.state() == WriterState::Open);

        RawPacketRecord rec;
        rec.record_flags = kRecordFlagUtcValid;
        rec.capture_index = 0;
        rec.capture_utc_ns = 1000;
        rec.capture_monotonic_ns = 1000;
        rec.payload = {1, 2, 3};

        CHECK(writer.append(rec).empty());
        CHECK(writer.record_count() == 1);

        CHECK(writer.finalize().empty());
        CHECK(writer.state() == WriterState::Finalized);
        CHECK(writer.finalized_paths().size() == 1);
    }

    // Finalize empty segment rejected
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        meta.start_capture_index = 100;
        RawSegmentWriter writer(meta, dir, {});
        writer.open();
        CHECK(!writer.finalize().empty());
    }

    // Existing partial path rejected
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentWriter writer1(meta, dir, {});
        writer1.open();

        // Try to open another writer with same metadata
        RawSegmentWriter writer2(meta, dir, {});
        CHECK(!writer2.open().empty());
    }

    // Finalized writer cannot append
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentWriter writer(meta, dir, {});
        writer.open();

        RawPacketRecord rec;
        rec.capture_index = 0;
        rec.capture_monotonic_ns = 0;
        writer.append(rec);
        writer.finalize();

        RawPacketRecord rec2;
        rec2.capture_index = 1;
        rec2.capture_monotonic_ns = 1;
        CHECK(!writer.append(rec2).empty());
    }

    // Finalize twice rejected
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentWriter writer(meta, dir, {});
        writer.open();

        RawPacketRecord rec;
        rec.capture_index = 0;
        rec.capture_monotonic_ns = 0;
        writer.append(rec);
        writer.finalize();
        CHECK(!writer.finalize().empty());
    }

    // Failed record write does not advance capture_index
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentWriter writer(meta, dir, {});
        writer.open();

        // Append a valid record
        RawPacketRecord rec;
        rec.capture_index = 0;
        rec.capture_monotonic_ns = 0;
        rec.payload = {1, 2, 3};
        writer.append(rec);

        // Try to append with wrong capture_index
        RawPacketRecord bad_rec;
        bad_rec.capture_index = 5;  // should be 1
        bad_rec.capture_monotonic_ns = 1;
        CHECK(!writer.append(bad_rec).empty());
        CHECK(writer.next_capture_index() == 1);  // unchanged
    }

    // Payload > 1 MiB rejected
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentWriter writer(meta, dir, {});
        writer.open();

        RawPacketRecord rec;
        rec.capture_index = 0;
        rec.payload.resize(kMaxPayloadSize + 1);
        CHECK(!writer.append(rec).empty());
    }

    // Final path exists only after successful finalize
    {
        auto dir = temp_dir();
        auto meta = make_meta();
        RawSegmentWriter writer(meta, dir, {});
        writer.open();

        auto final_path = writer.current_final_path();
        CHECK(!fs::exists(final_path));

        RawPacketRecord rec;
        rec.capture_index = 0;
        writer.append(rec);

        auto partial_path = writer.current_partial_path();
        CHECK(fs::exists(partial_path));
        CHECK(!fs::exists(final_path));

        writer.finalize();
        CHECK(fs::exists(final_path));
    }

    // Core API output does not depend on wall clock
    {
        auto meta = make_meta();
        std::vector<std::uint8_t> buf1, buf2;
        serialize_header(buf1, meta);
        serialize_header(buf2, meta);
        CHECK(buf1 == buf2);
    }

    std::cout << "test_writer_lifecycle: ALL PASSED\n";
    return 0;
}
