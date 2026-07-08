#pragma once

#include "qsh/qsh_types.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace qsh {

struct DataQuality {
    std::string source_file_name;
    std::string source_file_sha256;
    std::string stream_type;
    std::string instrument;
    std::string recording_time;

    int64_t records_total = 0;
    int64_t records_parsed = 0;
    int64_t records_failed = 0;

    int64_t first_timestamp = 0;
    int64_t last_timestamp = 0;

    // OrdLog-specific counters
    int64_t add_count = 0;
    int64_t fill_count = 0;
    int64_t cancel_count = 0;
    int64_t remove_count = 0;
    int64_t new_session_count = 0;
    int64_t tx_end_count = 0;
    int64_t buy_count = 0;
    int64_t sell_count = 0;
    int64_t unknown_side_count = 0;
    int64_t non_system_count = 0;
    int64_t non_zero_repl_act_count = 0;
    int64_t snapshot_count = 0;
    int64_t moved_count = 0;
    int64_t cross_trade_count = 0;

    // TxEnd grouping
    int64_t tx_count = 0;
    int64_t tx_max_size = 0;

    // Book reconstruction errors
    int64_t book_missing_order_id = 0;
    int64_t book_level_not_found = 0;
    int64_t book_amount_mismatch = 0;
    int64_t book_negative_level_volume = 0;
    int64_t book_crossed = 0;
    int64_t book_invalid_side = 0;

    // L2 export diagnostics
    int64_t l2_crossed_book_snapshots = 0;
    int64_t l2_non_positive_spread_snapshots = 0;
    int64_t l2_empty_bid_snapshots = 0;
    int64_t l2_empty_ask_snapshots = 0;

    std::vector<std::string> warnings;
    std::vector<std::string> errors;
};

// Write DataQuality to JSON file.
bool write_data_quality_json(const std::string& path, const DataQuality& dq);

// Write metadata JSON file.
bool write_metadata_json(const std::string& path, const DataQuality& dq);

// Print DataQuality summary to stdout.
void print_data_quality_summary(const DataQuality& dq);

}  // namespace qsh
