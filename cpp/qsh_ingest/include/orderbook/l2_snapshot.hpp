#pragma once

#include "qsh/qsh_types.hpp"
#include "orderbook/order_book.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace qsh {

struct L2SnapshotEntry {
    Timestamp timestamp = 0;
    std::vector<L2SnapshotRow> levels;
    double mid = 0.0;
    Price spread = 0;
};

// Diagnostic reason for invalid L2 snapshot.
enum class L2DiagReason : uint8_t {
    CrossedBook,
    NonPositiveSpread,
    EmptyBid,
    EmptyAsk
};

inline const char* l2_diag_reason_name(L2DiagReason r) {
    switch (r) {
        case L2DiagReason::CrossedBook:        return "CROSSED_BOOK";
        case L2DiagReason::NonPositiveSpread:  return "NON_POSITIVE_SPREAD";
        case L2DiagReason::EmptyBid:           return "EMPTY_BID";
        case L2DiagReason::EmptyAsk:           return "EMPTY_ASK";
        default:                               return "UNKNOWN";
    }
}

// Single diagnostic entry for a bad L2 snapshot.
struct L2DiagnosticEntry {
    Timestamp timestamp = 0;
    L2DiagReason reason = L2DiagReason::CrossedBook;
    Price best_bid = 0;
    Price best_ask = 0;
    Price spread = 0;
    Volume bid_qty_1 = 0;
    Volume ask_qty_1 = 0;
    int64_t snapshots_written = 0;
    int64_t records_processed = 0;
};

// Write L2 snapshots to CSV file.
// Returns number of snapshots written.
size_t write_l2_csv(const std::string& path, const std::vector<L2SnapshotEntry>& snapshots, int depth);

// Generate CSV header for L2 snapshots.
std::string l2_csv_header(int depth);

// Write L2 diagnostics CSV. Returns number of entries written.
size_t write_l2_diagnostics_csv(const std::string& path, const std::vector<L2DiagnosticEntry>& entries);

}  // namespace qsh
