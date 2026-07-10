#include "moex_raw/raw_segment.hpp"
#include "moex_raw/raw_types.hpp"
#include "moex_raw/endian.hpp"
#include "moex_raw/crc32c.hpp"
#include "moex_raw/sha256.hpp"
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

    // Validate rotation policy
    if (policy_.max_segment_bytes > kMaxSegmentBytes) {
        return "max_segment_bytes exceeds 64 GiB";
    }

    // Check that limits fit header + one record + footer
    if (policy_.max_segment_bytes > 0) {
        std::size_t min_size = kMaxHeaderSize + kRecordHeaderSize + 4 + kFooterSize;
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

    // Write header
    content_buffer_.clear();
    serialize_header(content_buffer_, metadata_);

    if (std::fwrite(content_buffer_.data(), 1, content_buffer_.size(), file_) != content_buffer_.size()) {
        state_ = WriterState::Failed;
        return "Failed to write header";
    }
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

    // Check rotation
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
    if (record_count_ == 0) return false;

    std::uint32_t payload_size = static_cast<std::uint32_t>(rec.payload.size());
    std::uint32_t record_size = kRecordHeaderSize + payload_size + 4;
    std::uint64_t new_file_bytes = current_file_bytes_ + record_size;

    if (policy_.max_records_per_segment > 0 &&
        record_count_ >= policy_.max_records_per_segment) {
        return true;
    }

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

    current_file_bytes_ += rec_buf.size();
    return "";
}

std::string RawSegmentWriter::finalize_current() {
    // Compute content SHA-256
    std::uint8_t content_hash[32];
    sha256(content_buffer_.data(), content_buffer_.size(), content_hash);

    RawFooter footer;
    footer.record_count = record_count_;
    footer.first_capture_index = metadata_.start_capture_index;
    footer.last_capture_index = next_capture_index_ - 1;
    footer.total_payload_bytes = total_payload_bytes_;
    footer.data_bytes_before_footer = current_file_bytes_;
    std::memcpy(footer.content_sha256, content_hash, 32);

    // Actually we need to hash ALL bytes written so far (header + records).
    // Let me fix this: content_buffer_ was only the header. We need to hash everything before footer.
    // We'll read the file content and hash it.
    if (file_) {
        std::fflush(file_);
        // Re-read file to compute content SHA-256
        auto pos = std::ftell(file_);
        std::fseek(file_, 0, SEEK_SET);

        SHA256Ctx sha_ctx;
        sha256_init(sha_ctx);
        std::uint8_t buf[4096];
        std::size_t total_read = 0;
        while (total_read < current_file_bytes_) {
            std::size_t to_read = std::min(sizeof(buf), current_file_bytes_ - total_read);
            std::size_t n = std::fread(buf, 1, to_read, file_);
            if (n == 0) break;
            sha256_update(sha_ctx, buf, n);
            total_read += n;
        }
        sha256_final(sha_ctx, content_hash);
        std::memcpy(footer.content_sha256, content_hash, 32);

        std::fseek(file_, pos, SEEK_SET);
    }

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

    std::fclose(file_);
    file_ = nullptr;

    // Rename partial to final
    auto partial = current_partial_path();
    auto final_path = current_final_path();

    if (!fs_->rename(partial, final_path)) {
        state_ = WriterState::Failed;
        return "Failed to rename partial to final";
    }

    finalized_paths_.push_back(final_path);

    // Reset content buffer for next segment
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

    // Write header for new segment
    content_buffer_.clear();
    serialize_header(content_buffer_, metadata_);

    if (std::fwrite(content_buffer_.data(), 1, content_buffer_.size(), file_) != content_buffer_.size()) {
        state_ = WriterState::Failed;
        return "Failed to write header for new segment";
    }
    current_file_bytes_ = content_buffer_.size();

    state_ = WriterState::Open;
    return "";
}

}  // namespace moex_raw
