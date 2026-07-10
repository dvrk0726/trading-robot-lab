#include "moex_raw/raw_segment.hpp"
#include "moex_raw/raw_types.hpp"
#include "moex_raw/endian.hpp"
#include "moex_raw/crc32c.hpp"
#include "moex_raw/sha256.hpp"
#include "moex_raw/strings.hpp"
#include <cstdio>
#include <filesystem>
#include <cstring>

namespace moex_raw {

// --- DefaultFileSystem ---

bool DefaultFileSystem::exists(const std::string& path) {
    return std::filesystem::exists(path);
}

bool DefaultFileSystem::rename(const std::string& from, const std::string& to) {
    std::error_code ec;
    std::filesystem::rename(from, to, ec);
    return !ec;
}

bool DefaultFileSystem::remove(const std::string& path) {
    std::error_code ec;
    return std::filesystem::remove(path, ec);
}

// --- Metadata validation ---

static bool is_all_zero(const std::uint8_t* data, std::size_t len) {
    for (std::size_t i = 0; i < len; ++i) {
        if (data[i] != 0) return false;
    }
    return true;
}

static std::string validate_metadata(const RawSegmentMetadata& meta) {
    // session_id must not be all zero
    if (is_all_zero(meta.session.session_id, 16))
        return "session_id must not be all zero";

    // created_utc_ns must be non-zero
    if (meta.created_utc_ns == 0)
        return "created_utc_ns must be non-zero";

    // source_id and channel_id must be non-zero
    if (meta.source.source_id == 0)
        return "source_id must be non-zero";
    if (meta.source.channel_id == 0)
        return "channel_id must be non-zero";

    // All three SHA-256 values must not be all zero
    if (is_all_zero(meta.source.configuration_sha256, 32))
        return "configuration_sha256 must not be all zero";
    if (is_all_zero(meta.source.templates_sha256, 32))
        return "templates_sha256 must not be all zero";
    if (is_all_zero(meta.source.endpoint_fingerprint_sha256, 32))
        return "endpoint_fingerprint_sha256 must not be all zero";

    // Enum validation
    auto clock_val = static_cast<std::uint8_t>(meta.source.clock_domain);
    if (clock_val < 1 || clock_val > 3)
        return "unsupported clock_domain value";

    auto transport_val = static_cast<std::uint8_t>(meta.source.transport);
    if (transport_val > 2)
        return "unsupported transport value";

    auto side_val = static_cast<std::uint8_t>(meta.source.source_side);
    if (side_val > 2)
        return "unsupported source_side value";

    // String validation: UTF-8, no NUL, 128-byte limit
    if (!validate_utf8_string(meta.session.feed_group))
        return "invalid feed_group string";
    if (meta.session.feed_group.empty())
        return "feed_group must be non-empty";

    if (!validate_utf8_string(meta.session.endpoint_role))
        return "invalid endpoint_role string";
    if (meta.session.endpoint_role.empty())
        return "endpoint_role must be non-empty";

    if (!validate_utf8_string(meta.session.source_label))
        return "invalid source_label string";

    // Validate that the header would not exceed 4096 bytes
    std::vector<std::uint8_t> header_probe;
    serialize_header(header_probe, meta);
    if (header_probe.size() > kMaxHeaderSize)
        return "serialized header exceeds 4096 bytes";

    return "";
}

// --- RawSegmentWriter ---

RawSegmentWriter::RawSegmentWriter(RawSegmentMetadata metadata,
                                   std::string output_dir,
                                   RawSegmentRotationPolicy policy,
                                   IFileSystem* fs)
    : metadata_(std::move(metadata))
    , output_dir_(std::move(output_dir))
    , policy_(policy)
    , fs_(fs ? fs : &default_fs_)
    , current_segment_index_(metadata_.segment_index)
    , next_capture_index_(metadata_.start_capture_index) {
}

RawSegmentWriter::~RawSegmentWriter() {
    if (file_) {
        std::fclose(file_);
        file_ = nullptr;
    }
}

std::string RawSegmentWriter::current_partial_path() const {
    return output_dir_ + "/" + canonical_filename(metadata_.session.session_id,
                                                  metadata_.source.source_id,
                                                  metadata_.source.channel_id,
                                                  current_segment_index_) + ".partial";
}

std::string RawSegmentWriter::current_final_path() const {
    return output_dir_ + "/" + canonical_filename(metadata_.session.session_id,
                                                  metadata_.source.source_id,
                                                  metadata_.source.channel_id,
                                                  current_segment_index_);
}

std::string RawSegmentWriter::open() {
    if (state_ != WriterState::Created) {
        return "Writer not in Created state";
    }

    // Validate all metadata before creating any files
    auto meta_err = validate_metadata(metadata_);
    if (!meta_err.empty()) return meta_err;

    // Enforce hard 64 GiB cap regardless of rotation policy
    if (policy_.max_segment_bytes > kMaxSegmentBytes) {
        return "max_segment_bytes exceeds 64 GiB";
    }

    // Compute actual header size for limit checks
    std::vector<std::uint8_t> header_probe;
    serialize_header(header_probe, metadata_);
    std::size_t actual_header_size = header_probe.size();

    // Check that limits fit header + one record + footer
    if (policy_.max_segment_bytes > 0) {
        std::size_t min_size = actual_header_size + kRecordHeaderSize + 4 + kFooterSize;
        if (policy_.max_segment_bytes < min_size) {
            return "max_segment_bytes too small for header + one record + footer";
        }
    }

    auto partial = current_partial_path();
    auto final_path = current_final_path();

    // Check neither exists
    if (fs_->exists(partial)) {
        return "Partial file already exists: " + partial;
    }
    if (fs_->exists(final_path)) {
        return "Final file already exists: " + final_path;
    }

    // Create directory if needed
    std::error_code ec;
    std::filesystem::create_directories(output_dir_, ec);
    if (ec) {
        return "Cannot create output directory: " + ec.message();
    }

    file_ = std::fopen(partial.c_str(), "wb");
    if (!file_) {
        return "Cannot open partial file for writing";
    }

    // Initialize incremental content SHA-256
    sha256_init(content_sha_ctx_);
    content_sha_initialized_ = true;

    // Write header and feed into SHA-256
    content_buffer_.clear();
    serialize_header(content_buffer_, metadata_);

    if (std::fwrite(content_buffer_.data(), 1, content_buffer_.size(), file_) != content_buffer_.size()) {
        state_ = WriterState::Failed;
        return "Failed to write header";
    }
    sha256_update(content_sha_ctx_, content_buffer_.data(), content_buffer_.size());
    current_file_bytes_ = content_buffer_.size();

    state_ = WriterState::Open;
    return "";
}

std::string RawSegmentWriter::append(const RawPacketRecord& rec) {
    if (state_ != WriterState::Open) {
        return "Writer not in Open state";
    }

    // Validate payload size
    if (rec.payload.size() > kMaxPayloadSize) {
        return "Payload exceeds 1 MiB";
    }

    // Check capture_index is sequential
    if (rec.capture_index != next_capture_index_) {
        return "capture_index not sequential";
    }

    // Compute actual serialized record size
    std::uint32_t payload_size = static_cast<std::uint32_t>(rec.payload.size());
    std::uint32_t record_size = kRecordHeaderSize + payload_size + 4;

    // Validate first record fits: actual header + record + footer <= max_segment_bytes
    if (record_count_ == 0 && policy_.max_segment_bytes > 0) {
        std::uint64_t total;
        if (!checked_add_u64(current_file_bytes_, record_size, total)) {
            return "segment size overflow";
        }
        if (!checked_add_u64(total, kFooterSize, total)) {
            return "segment size overflow";
        }
        if (total > policy_.max_segment_bytes) {
            return "First record too large for segment byte limit";
        }
    }

    // Enforce hard 64 GiB cap: current_file_bytes + record + footer <= 64 GiB
    {
        std::uint64_t total;
        if (!checked_add_u64(current_file_bytes_, record_size, total)) {
            return "segment size overflow (64 GiB cap)";
        }
        if (!checked_add_u64(total, kFooterSize, total)) {
            return "segment size overflow (64 GiB cap)";
        }
        if (total > kMaxSegmentBytes) {
            return "segment would exceed 64 GiB hard limit";
        }
    }

    // Check rotation (only for subsequent records)
    if (should_rotate(rec)) {
        auto err = finalize_current();
        if (!err.empty()) return err;

        err = start_new_segment();
        if (!err.empty()) return err;
    }

    auto err = write_record(rec);
    if (!err.empty()) return err;

    next_capture_index_++;
    record_count_++;
    total_payload_bytes_ += rec.payload.size();

    return "";
}

std::string RawSegmentWriter::finalize() {
    if (state_ != WriterState::Open) {
        return "Writer not in Open state";
    }
    if (record_count_ == 0) {
        return "Cannot finalize empty segment";
    }
    return finalize_current();
}

std::size_t RawSegmentWriter::estimate_segment_bytes() const {
    return current_file_bytes_ + kFooterSize;
}

bool RawSegmentWriter::should_rotate(const RawPacketRecord& rec) const {
    // Don't rotate for the first record — it was validated in append()
    if (record_count_ == 0) return false;

    std::uint32_t payload_size = static_cast<std::uint32_t>(rec.payload.size());
    std::uint32_t record_size = kRecordHeaderSize + payload_size + 4;
    std::uint64_t new_file_bytes = current_file_bytes_ + record_size;

    // Check record count limit
    if (policy_.max_records_per_segment > 0 &&
        record_count_ >= policy_.max_records_per_segment) {
        return true;
    }

    // Check byte limit
    if (policy_.max_segment_bytes > 0 &&
        new_file_bytes + kFooterSize > policy_.max_segment_bytes) {
        return true;
    }

    return false;
}

std::string RawSegmentWriter::write_record(const RawPacketRecord& rec) {
    std::vector<std::uint8_t> rec_buf;
    serialize_record(rec_buf, rec);

    if (std::fwrite(rec_buf.data(), 1, rec_buf.size(), file_) != rec_buf.size()) {
        state_ = WriterState::Failed;
        return "Failed to write record";
    }

    // Feed into incremental SHA-256
    sha256_update(content_sha_ctx_, rec_buf.data(), rec_buf.size());
    current_file_bytes_ += rec_buf.size();
    return "";
}

std::string RawSegmentWriter::finalize_current() {
    // Finalize incremental content SHA-256
    std::uint8_t content_hash[32];
    sha256_final(content_sha_ctx_, content_hash);
    content_sha_initialized_ = false;

    RawFooter footer;
    footer.record_count = record_count_;
    footer.first_capture_index = metadata_.start_capture_index;
    footer.last_capture_index = next_capture_index_ - 1;
    footer.total_payload_bytes = total_payload_bytes_;
    footer.data_bytes_before_footer = current_file_bytes_;
    std::memcpy(footer.content_sha256, content_hash, 32);

    std::vector<std::uint8_t> footer_buf;
    serialize_footer(footer_buf, footer);

    if (std::fwrite(footer_buf.data(), 1, footer_buf.size(), file_) != footer_buf.size()) {
        state_ = WriterState::Failed;
        return "Failed to write footer";
    }

    if (std::fflush(file_) != 0) {
        state_ = WriterState::Failed;
        return "Failed to flush";
    }

    if (std::fclose(file_) != 0) {
        file_ = nullptr;
        state_ = WriterState::Failed;
        return "Failed to close file";
    }
    file_ = nullptr;

    // Rename partial to final
    auto partial = current_partial_path();
    auto final_path = current_final_path();

    if (!fs_->rename(partial, final_path)) {
        state_ = WriterState::Failed;
        return "Failed to rename partial to final";
    }

    finalized_paths_.push_back(final_path);

    // Reset for next segment
    content_buffer_.clear();
    current_file_bytes_ = 0;
    record_count_ = 0;
    total_payload_bytes_ = 0;

    state_ = WriterState::Finalized;
    return "";
}

std::string RawSegmentWriter::start_new_segment() {
    current_segment_index_++;
    metadata_.segment_index = current_segment_index_;
    metadata_.start_capture_index = next_capture_index_;

    auto partial = current_partial_path();
    auto final_path = current_final_path();

    if (fs_->exists(partial)) {
        return "Partial file already exists for new segment: " + partial;
    }
    if (fs_->exists(final_path)) {
        return "Final file already exists for new segment: " + final_path;
    }

    file_ = std::fopen(partial.c_str(), "wb");
    if (!file_) {
        return "Cannot open new segment partial file";
    }

    // Initialize incremental content SHA-256 for new segment
    sha256_init(content_sha_ctx_);
    content_sha_initialized_ = true;

    // Write header for new segment and feed into SHA-256
    content_buffer_.clear();
    serialize_header(content_buffer_, metadata_);

    if (std::fwrite(content_buffer_.data(), 1, content_buffer_.size(), file_) != content_buffer_.size()) {
        state_ = WriterState::Failed;
        return "Failed to write header for new segment";
    }
    sha256_update(content_sha_ctx_, content_buffer_.data(), content_buffer_.size());
    current_file_bytes_ = content_buffer_.size();

    state_ = WriterState::Open;
    return "";
}

}  // namespace moex_raw
