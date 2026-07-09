#pragma once

#include "qsh/qsh_types.hpp"
#include "orderbook/order_book.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace qsh {

// M10U: Machine-readable reason why a snapshot is not strategy-ready.
enum class StrategyRejectReason : uint8_t {
    Ok,                // Snapshot is strategy-ready
    CrossedBook,       // best_bid > best_ask
    LockedBook,        // best_bid == best_ask, both sides exist
    MissingBestBid,    // No bid levels
    MissingBestAsk,    // No ask levels
    EmptyBook,         // Neither side has levels
    InvalidPrice,      // Best price <= 0 on a side that has levels
    InvalidDepth       // Requested depth <= 0
};

inline const char* strategy_reject_reason_name(StrategyRejectReason r) {
    switch (r) {
        case StrategyRejectReason::Ok:             return "ok";
        case StrategyRejectReason::CrossedBook:    return "crossed_book";
        case StrategyRejectReason::LockedBook:     return "locked_book";
        case StrategyRejectReason::MissingBestBid: return "missing_best_bid";
        case StrategyRejectReason::MissingBestAsk: return "missing_best_ask";
        case StrategyRejectReason::EmptyBook:      return "empty_book";
        case StrategyRejectReason::InvalidPrice:   return "invalid_price";
        case StrategyRejectReason::InvalidDepth:   return "invalid_depth";
        default:                                   return "unknown";
    }
}

// M10U: Classify an L2 snapshot for strategy readiness.
// Pure function — no side effects, deterministic.
inline StrategyRejectReason classify_l2_snapshot(Price best_bid, Price best_ask,
                                                  size_t bid_depth, size_t ask_depth,
                                                  int depth) {
    if (depth <= 0) return StrategyRejectReason::InvalidDepth;
    if (bid_depth == 0 && ask_depth == 0) return StrategyRejectReason::EmptyBook;
    if (bid_depth == 0) return StrategyRejectReason::MissingBestBid;
    if (ask_depth == 0) return StrategyRejectReason::MissingBestAsk;
    if (best_bid <= 0) return StrategyRejectReason::InvalidPrice;
    if (best_ask <= 0) return StrategyRejectReason::InvalidPrice;
    if (best_bid > best_ask) return StrategyRejectReason::CrossedBook;
    if (best_bid == best_ask) return StrategyRejectReason::LockedBook;
    return StrategyRejectReason::Ok;
}

struct L2SnapshotEntry {
    Timestamp timestamp = 0;
    std::vector<L2SnapshotRow> levels;
    double mid = 0.0;
    Price spread = 0;

    // M10U: Strategy readiness fields
    bool is_crossed = false;
    bool is_locked = false;
    bool strategy_ready = false;
    StrategyRejectReason strategy_reject_reason = StrategyRejectReason::Ok;

    // M10U: Recommended additional columns
    Price best_bid = 0;
    Price best_ask = 0;
    int snapshot_index = 0;
    int64_t record_index = 0;
    int64_t tx_index = 0;
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
