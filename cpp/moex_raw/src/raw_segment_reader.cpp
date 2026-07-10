#include "moex_raw/raw_segment.hpp"
#include "moex_raw/raw_types.hpp"
#include "moex_raw/endian.hpp"
#include "moex_raw/crc32c.hpp"
#include "moex_raw/sha256.hpp"
#include <cstdio>
#include <filesystem>
#include <cstring>
#include <algorithm>

namespace moex_raw {

static void add_issue(std::vector<RawValidationIssue>& issues,
                      ValidationSeverity sev, const std::string& code,
                      const std::string& msg) {
    issues.push_back({sev, code, msg});
}

SegmentStatus validate_segment(const std::string& path,
                               RawSegmentMetadata& meta,
                               RawFooter& footer,
                               std::vector<RawValidationIssue>& issues,
                               std::string& content_sha256_hex,
                               std::string& file_sha256_hex) {
    // Read file
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        add_issue(issues, ValidationSeverity::Error, "IO_ERROR", "Cannot open file");
        return SegmentStatus::IoError;
    }

    std::fseek(f, 0, SEEK_END);
    auto file_size = static_cast<std::size_t>(std::ftell(f));
    std::fseek(f, 0, SEEK_SET);

    if (file_size == 0) {
        std::fclose(f);
        add_issue(issues, ValidationSeverity::Error, "EMPTY_FILE", "Empty file");
        return SegmentStatus::Truncated;
    }

    // Read entire file for validation (bounded by file size)
    std::vector<std::uint8_t> data(file_size);
    if (std::fread(data.data(), 1, file_size, f) != file_size) {
        std::fclose(f);
        add_issue(issues, ValidationSeverity::Error, "IO_ERROR", "Cannot read file");
        return SegmentStatus::IoError;
    }
    std::fclose(f);

    // Compute whole-file SHA-256
    file_sha256_hex = sha256_hex(data.data(), data.size());

    // Validate header
    std::size_t header_size = 0;
    if (!deserialize_header(data.data(), data.size(), meta, header_size, issues)) {
        // Check if it's partial (no valid footer)
        if (data.size() >= 8 && std::memcmp(data.data() + data.size() - 8, kMagicEnd, 8) != 0) {
            return SegmentStatus::Partial;
        }
        return SegmentStatus::Corrupt;
    }

    // Find and validate footer — must be at the end
    if (data.size() < header_size + kFooterSize) {
        add_issue(issues, ValidationSeverity::Error, "TRUNCATED", "File too short for footer");
        return SegmentStatus::Truncated;
    }

    // Check for bytes after footer
    if (data.size() > header_size + kFooterSize) {
        // Check if there's a footer at the expected position
        std::size_t footer_offset = data.size() - kFooterSize;
        if (std::memcmp(data.data() + footer_offset, kMagicEnd, 8) != 0) {
            add_issue(issues, ValidationSeverity::Error, "NO_FOOTER", "No valid footer at end of file");
            return SegmentStatus::Corrupt;
        }

        // Validate footer
        if (!deserialize_footer(data.data() + footer_offset, kFooterSize, footer, issues)) {
            return SegmentStatus::Corrupt;
        }

        // Check no bytes after footer
        // Actually footer IS at the end. Let me check: data_bytes_before_footer should equal footer_offset.
        if (footer.data_bytes_before_footer != footer_offset) {
            add_issue(issues, ValidationSeverity::Error, "WRONG_DATA_BYTES",
                      "data_bytes_before_footer mismatch");
            return SegmentStatus::Corrupt;
        }
    } else {
        // Footer is exactly at header_size
        if (!deserialize_footer(data.data() + header_size, kFooterSize, footer, issues)) {
            return SegmentStatus::Corrupt;
        }
        if (footer.data_bytes_before_footer != header_size) {
            add_issue(issues, ValidationSeverity::Error, "WRONG_DATA_BYTES",
                      "data_bytes_before_footer mismatch");
            return SegmentStatus::Corrupt;
        }
    }

    // Validate content SHA-256
    std::uint8_t computed_content_hash[32];
    sha256(data.data(), footer.data_bytes_before_footer, computed_content_hash);
    std::string computed_content_hex = sha256_bytes_to_hex(computed_content_hash);
    content_sha256_hex = computed_content_hex;

    std::string stored_content_hex = sha256_bytes_to_hex(footer.content_sha256);
    if (computed_content_hex != stored_content_hex) {
        add_issue(issues, ValidationSeverity::Error, "WRONG_CONTENT_SHA256",
                  "content_sha256 mismatch");
        return SegmentStatus::Corrupt;
    }

    // Validate records
    std::size_t pos = header_size;
    std::uint64_t expected_capture_index = meta.start_capture_index;
    std::uint64_t last_monotonic_ns = 0;
    std::uint64_t actual_record_count = 0;
    std::uint64_t actual_payload_bytes = 0;
    std::uint64_t actual_first_capture = 0;
    std::uint64_t actual_last_capture = 0;
    bool first_record = true;

    while (pos < footer.data_bytes_before_footer) {
        RawPacketRecord rec;
        std::size_t record_total_size = 0;

        if (!deserialize_record_header(data.data() + pos,
                                       footer.data_bytes_before_footer - pos,
                                       rec, record_total_size, issues)) {
            return SegmentStatus::Corrupt;
        }

        // Validate capture_index continuity
        if (rec.capture_index != expected_capture_index) {
            add_issue(issues, ValidationSeverity::Error, "WRONG_CAPTURE_INDEX",
                      "capture_index not contiguous");
            return SegmentStatus::Corrupt;
        }

        // Validate monotonic timestamp
        if (rec.capture_monotonic_ns < last_monotonic_ns) {
            add_issue(issues, ValidationSeverity::Error, "MONOTONIC_DECREASE",
                      "capture_monotonic_ns decreased");
            return SegmentStatus::Corrupt;
        }
        last_monotonic_ns = rec.capture_monotonic_ns;

        if (first_record) {
            actual_first_capture = rec.capture_index;
            first_record = false;
        }
        actual_last_capture = rec.capture_index;
        actual_record_count++;
        actual_payload_bytes += rec.payload.size();

        expected_capture_index++;
        pos += record_total_size;
    }

    // Validate footer counts
    if (actual_record_count != footer.record_count) {
        add_issue(issues, ValidationSeverity::Error, "WRONG_RECORD_COUNT",
                  "Record count mismatch");
        return SegmentStatus::Corrupt;
    }
    if (actual_payload_bytes != footer.total_payload_bytes) {
        add_issue(issues, ValidationSeverity::Error, "WRONG_PAYLOAD_TOTAL",
                  "Total payload bytes mismatch");
        return SegmentStatus::Corrupt;
    }
    if (actual_first_capture != footer.first_capture_index) {
        add_issue(issues, ValidationSeverity::Error, "WRONG_FIRST_INDEX",
                  "first_capture_index mismatch");
        return SegmentStatus::Corrupt;
    }
    if (actual_last_capture != footer.last_capture_index) {
        add_issue(issues, ValidationSeverity::Error, "WRONG_LAST_INDEX",
                  "last_capture_index mismatch");
        return SegmentStatus::Corrupt;
    }

    return SegmentStatus::ValidFinalized;
}

SegmentStatus validate_stream_set(const std::vector<std::string>& paths,
                                  std::vector<RawSegmentMetadata>& metas,
                                  std::vector<RawFooter>& footers,
                                  std::vector<RawValidationIssue>& issues) {
    if (paths.empty()) {
        add_issue(issues, ValidationSeverity::Error, "EMPTY_STREAM_SET", "No segments provided");
        return SegmentStatus::Corrupt;
    }

    metas.clear();
    footers.clear();
    metas.reserve(paths.size());
    footers.reserve(paths.size());

    // Validate each segment and collect metadata
    for (const auto& path : paths) {
        RawSegmentMetadata meta;
        RawFooter footer;
        std::vector<RawValidationIssue> seg_issues;
        std::string content_hex, file_hex;

        auto status = validate_segment(path, meta, footer, seg_issues, content_hex, file_hex);
        issues.insert(issues.end(), seg_issues.begin(), seg_issues.end());

        if (status != SegmentStatus::ValidFinalized) {
            add_issue(issues, ValidationSeverity::Error, "INVALID_SEGMENT",
                      "Segment not valid finalized: " + path);
            return status;
        }

        metas.push_back(meta);
        footers.push_back(footer);
    }

    // Check consistent metadata
    for (std::size_t i = 1; i < metas.size(); ++i) {
        if (std::memcmp(metas[i].session.session_id, metas[0].session.session_id, 16) != 0) {
            add_issue(issues, ValidationSeverity::Error, "MIXED_SESSION_ID",
                      "Different session_id in stream set");
            return SegmentStatus::Corrupt;
        }
        if (metas[i].source.source_id != metas[0].source.source_id) {
            add_issue(issues, ValidationSeverity::Error, "MIXED_SOURCE_ID",
                      "Different source_id in stream set");
            return SegmentStatus::Corrupt;
        }
        if (metas[i].source.channel_id != metas[0].source.channel_id) {
            add_issue(issues, ValidationSeverity::Error, "MIXED_CHANNEL_ID",
                      "Different channel_id in stream set");
            return SegmentStatus::Corrupt;
        }
    }

    // Check contiguous segment indexes
    for (std::size_t i = 1; i < metas.size(); ++i) {
        if (metas[i].segment_index != metas[i-1].segment_index + 1) {
            add_issue(issues, ValidationSeverity::Error, "NONCONTIGUOUS_SEGMENT_INDEX",
                      "Segment indexes not contiguous");
            return SegmentStatus::Corrupt;
        }
    }

    // Check contiguous capture indexes across segments
    for (std::size_t i = 1; i < footers.size(); ++i) {
        if (footers[i].first_capture_index != footers[i-1].last_capture_index + 1) {
            add_issue(issues, ValidationSeverity::Error, "NONCONTIGUOUS_CAPTURE_INDEX",
                      "capture_index not contiguous across segments");
            return SegmentStatus::Corrupt;
        }
    }

    return SegmentStatus::ValidFinalized;
}

std::vector<StreamSetInfo> group_stream_sets(const std::string& directory) {
    std::vector<StreamSetInfo> result;

    if (!std::filesystem::exists(directory)) return result;

    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (!entry.is_regular_file()) continue;

        auto filename = entry.path().filename().string();
        auto ext = entry.path().extension().string();

        // Only process .mxraw files (not .partial)
        if (ext != ".mxraw") continue;
        if (filename.find(".partial") != std::string::npos) continue;

        // Read header to get session/source/channel
        std::FILE* f = std::fopen(entry.path().string().c_str(), "rb");
        if (!f) continue;

        std::vector<std::uint8_t> header_data(4096);
        std::size_t n = std::fread(header_data.data(), 1, 4096, f);
        std::fclose(f);

        RawSegmentMetadata meta;
        std::size_t header_size = 0;
        std::vector<RawValidationIssue> issues;

        if (!deserialize_header(header_data.data(), n, meta, header_size, issues)) {
            continue;  // skip invalid files
        }

        // Find or create stream set
        StreamSetInfo* found = nullptr;
        for (auto& ss : result) {
            if (std::memcmp(ss.session_id, meta.session.session_id, 16) == 0 &&
                ss.source_id == meta.source.source_id &&
                ss.channel_id == meta.source.channel_id) {
                found = &ss;
                break;
            }
        }

        if (!found) {
            result.push_back({});
            found = &result.back();
            std::memcpy(found->session_id, meta.session.session_id, 16);
            found->source_id = meta.source.source_id;
            found->channel_id = meta.source.channel_id;
        }

        found->segment_paths.push_back(entry.path().string());
        found->segment_indexes.push_back(meta.segment_index);
    }

    // Sort each stream set by segment index
    for (auto& ss : result) {
        // Sort both vectors together by segment_indexes
        std::vector<std::pair<std::uint64_t, std::string>> pairs;
        for (std::size_t i = 0; i < ss.segment_indexes.size(); ++i) {
            pairs.emplace_back(ss.segment_indexes[i], ss.segment_paths[i]);
        }
        std::sort(pairs.begin(), pairs.end());
        for (std::size_t i = 0; i < pairs.size(); ++i) {
            ss.segment_indexes[i] = pairs[i].first;
            ss.segment_paths[i] = pairs[i].second;
        }
    }

    return result;
}

}  // namespace moex_raw
