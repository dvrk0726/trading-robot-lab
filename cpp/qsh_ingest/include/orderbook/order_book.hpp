#pragma once

#include "qsh/qsh_types.hpp"
#include "orderbook/l3_event.hpp"
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace qsh {

struct BookLevel {
    Price price = 0;
    Volume total_volume = 0;
    int order_count = 0;
};

struct L2SnapshotRow {
    Price bid_price = 0;
    Volume bid_qty = 0;
    Price ask_price = 0;
    Volume ask_qty = 0;
};

// M10J: Orphan fill handling mode
enum class OrphanFillMode {
    Strict,           // Current: require order_id in active map, count missing_order_id
    Ignore,           // Skip orphan fills entirely, do not mutate book
    ReduceSamePrice,  // Reduce volume at same price level without order_id lookup
    TransactionRest   // Use amount_rest to update most recent resting order in same transaction
};

// M10N: Orphan cancel/remove handling mode
enum class OrphanCancelMode {
    Strict,  // Default: count missing_order_id for cancel/remove of unknown order
    Ignore   // Skip cancel/remove of unknown order without mutating book
};

inline const char* orphan_cancel_mode_name(OrphanCancelMode m) {
    switch (m) {
        case OrphanCancelMode::Strict: return "strict";
        case OrphanCancelMode::Ignore: return "ignore";
        default:                       return "unknown";
    }
}

inline const char* orphan_fill_mode_name(OrphanFillMode m) {
    switch (m) {
        case OrphanFillMode::Strict:          return "strict";
        case OrphanFillMode::Ignore:          return "ignore";
        case OrphanFillMode::ReduceSamePrice: return "reduce-same-price";
        case OrphanFillMode::TransactionRest: return "transaction-rest";
        default:                              return "unknown";
    }
}

// M10K: Transaction batch result for tx-grouped mode.
struct TransactionBatchResult {
    int64_t records_processed = 0;
    int64_t orphan_fill_events = 0;
    int64_t orphan_cancel_events = 0;
    int64_t orphan_remove_events = 0;
    int64_t orphan_fill_resolved_in_transaction = 0;
    int64_t orphan_cancel_resolved_in_transaction = 0;
    int64_t orphan_remove_resolved_in_transaction = 0;
    int64_t missing_order_id = 0;
};

// Error counters for book reconstruction.
struct BookErrors {
    int64_t missing_order_id = 0;
    int64_t level_not_found = 0;
    int64_t amount_mismatch = 0;
    int64_t negative_level_volume = 0;
    int64_t crossed_book_after_update = 0;
    int64_t invalid_side = 0;

    // M10G: Missing order diagnostics by event type
    int64_t missing_on_fill = 0;
    int64_t missing_on_cancel = 0;
    int64_t missing_on_remove = 0;
    int64_t missing_on_move = 0;

    // M10G: Missing order diagnostics by side
    int64_t missing_on_buy = 0;
    int64_t missing_on_sell = 0;
    int64_t missing_on_unknown_side = 0;

    // M10G: Snapshot/NewSession tracking
    int64_t snapshot_records_seen = 0;
    int64_t snapshot_orders_loaded = 0;
    int64_t new_session_records_seen = 0;
    int64_t book_clears_due_to_new_session = 0;
    int64_t first_valid_book_record_index = 0;

    // M10G: ADD record tracking
    int64_t add_records_seen = 0;
    int64_t add_records_applied = 0;
    int64_t add_records_skipped = 0;
    int64_t skip_invalid_side = 0;
    int64_t skip_zero_amount = 0;
    int64_t skip_non_system = 0;
    int64_t skip_non_zero_repl_act = 0;

    // M10J: Orphan fill counters
    int64_t orphan_fill_events = 0;
    int64_t orphan_fill_ignored = 0;
    int64_t orphan_fill_level_reductions = 0;
    int64_t orphan_fill_transaction_rest_updates = 0;
    int64_t crossed_book_snapshots = 0;
    int64_t non_positive_spread_snapshots = 0;

    // M10N: Orphan cancel/remove counters
    int64_t orphan_cancel_ignored = 0;
    int64_t orphan_remove_ignored = 0;

    // M10K: Transaction-grouped mode counters
    int64_t transactions_grouped = 0;
    int64_t records_in_grouped_transactions = 0;
    int64_t max_records_in_transaction = 0;
    int64_t tx_grouped_orphan_fill_events = 0;
    int64_t tx_grouped_orphan_cancel_events = 0;
    int64_t tx_grouped_orphan_remove_events = 0;
    int64_t tx_grouped_orphan_fill_resolved = 0;
    int64_t tx_grouped_orphan_cancel_resolved = 0;
    int64_t tx_grouped_orphan_remove_resolved = 0;
    int64_t tx_grouped_missing_order_id = 0;
    int64_t tx_grouped_crossed_book_snapshots = 0;
};

// L3 order book reconstruction from OrdLog events.
class OrderBook {
public:
    // Apply an OrderLogRecord to the book.
    // Returns true on success, false on error (error counted internally).
    bool apply(const OrderLogRecord& rec);

    // M10K: Apply a transaction batch (grouped by TxEnd).
    // Returns diagnostics about orphan events within the transaction.
    TransactionBatchResult apply_transaction(const std::vector<OrderLogRecord>& records);

    // Clear the book (on NewSession).
    void clear();

    // Generate L2 snapshot with given depth.
    std::vector<L2SnapshotRow> snapshot(int depth) const;

    // Get mid price. Returns 0 if book is empty.
    double mid_price() const;

    // Get spread. Returns 0 if book is empty.
    Price spread() const;

    // Current best bid price, or 0.
    Price best_bid() const;

    // Current best ask price, or 0.
    Price best_ask() const;

    // Number of bid levels.
    size_t bid_depth() const { return bid_levels_.size(); }

    // Number of ask levels.
    size_t ask_depth() const { return ask_levels_.size(); }

    // Error counters.
    const BookErrors& errors() const { return errors_; }
    BookErrors& errors_ref() { return errors_; }

    // Timestamp of last update.
    Timestamp last_timestamp() const { return last_ts_; }

    // Get order IDs at best bid level (up to max_ids).
    std::vector<UID> best_bid_order_ids(int max_ids = 20) const;

    // Get order IDs at best ask level (up to max_ids).
    std::vector<UID> best_ask_order_ids(int max_ids = 20) const;

    // Get total volume at best bid level.
    Volume best_bid_total_qty() const;

    // Get total volume at best ask level.
    Volume best_ask_total_qty() const;

    // Get order count at best bid level.
    int best_bid_order_count() const;

    // Get order count at best ask level.
    int best_ask_order_count() const;

    // Query individual order state (for lifecycle trace).
    // Returns true if order exists, false otherwise.
    bool get_order_info(UID order_id, Side& out_side, Price& out_price, Volume& out_qty) const;

    // M10Q: Get total volume at a specific price level on given side.
    // Returns 0 if level does not exist.
    Volume level_qty(Price price, Side side) const;

    // Set fill semantics mode: "delta" (amount = filled qty) or "rest" (amount = original, fill = amount - amount_rest).
    void set_fill_delta_mode(bool use_delta) { fill_delta_mode_ = use_delta; }

    // M10J: Set orphan fill handling mode.
    void set_orphan_fill_mode(OrphanFillMode mode) { orphan_fill_mode_ = mode; }

    // M10N: Set orphan cancel/remove handling mode.
    void set_orphan_cancel_mode(OrphanCancelMode mode) { orphan_cancel_mode_ = mode; }

    // M10G: Record index of first missing_order_id event (for timing diagnostics)
    int64_t first_missing_order_record_index() const { return first_missing_order_record_index_; }
    void set_first_missing_order_record_index(int64_t idx) {
        if (first_missing_order_record_index_ == 0) first_missing_order_record_index_ = idx;
    }

    // M10G: Record index of first crossed book snapshot
    int64_t first_crossed_book_record_index() const { return first_crossed_book_record_index_; }
    void set_first_crossed_book_record_index(int64_t idx) {
        if (first_crossed_book_record_index_ == 0) first_crossed_book_record_index_ = idx;
    }

    // M10G: Record index of first valid book state
    int64_t first_valid_book_record_index() const { return errors_.first_valid_book_record_index; }
    void set_first_valid_book_record_index(int64_t idx) {
        if (errors_.first_valid_book_record_index == 0) errors_.first_valid_book_record_index = idx;
    }

    // M10O: Unambiguous crossing index accessors
    int64_t first_crossing_event_record_index() const { return first_crossing_event_record_index_; }
    void set_first_crossing_event_record_index(int64_t idx) {
        if (first_crossing_event_record_index_ == 0) first_crossing_event_record_index_ = idx;
    }

    int64_t first_crossing_snapshot_record_index() const { return first_crossing_snapshot_record_index_; }
    void set_first_crossing_snapshot_record_index(int64_t idx) {
        if (first_crossing_snapshot_record_index_ == 0) first_crossing_snapshot_record_index_ = idx;
    }

    int64_t first_crossing_snapshot_index() const { return first_crossing_snapshot_index_; }
    void set_first_crossing_snapshot_index(int64_t idx) {
        if (first_crossing_snapshot_index_ == 0) first_crossing_snapshot_index_ = idx;
    }

    // M10O: Session/snapshot state accessors
    int64_t first_new_session_record_index() const { return first_new_session_record_index_; }
    void set_first_new_session_record_index(int64_t idx) {
        if (first_new_session_record_index_ == 0) first_new_session_record_index_ = idx;
    }

    int64_t snapshot_records_before_first_crossing() const { return snapshot_records_before_first_crossing_; }
    void set_snapshot_records_before_first_crossing(int64_t n) { snapshot_records_before_first_crossing_ = n; }

    int64_t tx_index_at_first_crossing() const { return tx_index_at_first_crossing_; }
    void set_tx_index_at_first_crossing(int64_t idx) { tx_index_at_first_crossing_ = idx; }

    int64_t records_in_first_crossing_tx() const { return records_in_first_crossing_tx_; }
    void set_records_in_first_crossing_tx(int64_t n) { records_in_first_crossing_tx_ = n; }

private:
    // Levels stored as price -> (volume, order_count), sorted.
    // Bid: descending by price (highest first).
    // Ask: ascending by price (lowest first).
    std::map<Price, std::pair<Volume, int>, std::greater<Price>> bid_levels_;
    std::map<Price, std::pair<Volume, int>> ask_levels_;

    // Order IDs at each price level: price -> set of order_ids
    std::map<Price, std::vector<UID>, std::greater<Price>> bid_level_orders_;
    std::map<Price, std::vector<UID>> ask_level_orders_;

    // Order tracking: order_id -> (side, price, amount)
    struct OrderInfo {
        Side side = Side::Unknown;
        Price price = 0;
        Volume amount = 0;
    };
    std::map<UID, OrderInfo> orders_;

    BookErrors errors_;
    Timestamp last_ts_ = 0;
    bool fill_delta_mode_ = true;  // true: amount=delta (default), false: amount=original, fill=amount-amount_rest
    OrphanFillMode orphan_fill_mode_ = OrphanFillMode::Strict;  // M10J: orphan fill handling mode
    OrphanCancelMode orphan_cancel_mode_ = OrphanCancelMode::Strict;  // M10N: orphan cancel/remove handling mode

    void add_order(const OrderLogRecord& rec);
    void fill_order(const OrderLogRecord& rec);
    void cancel_order(const OrderLogRecord& rec);
    void remove_order(const OrderLogRecord& rec);
    void move_order(const OrderLogRecord& rec);
    bool check_crossed() const;

    // M10G: Timing diagnostics
    int64_t first_missing_order_record_index_ = 0;
    int64_t first_crossed_book_record_index_ = 0;

    // M10O: Unambiguous crossing index fields
    int64_t first_crossing_event_record_index_ = 0;    // record whose apply() first makes bb >= ba
    int64_t first_crossing_snapshot_record_index_ = 0;  // record index where first crossed snapshot is emitted
    int64_t first_crossing_snapshot_index_ = 0;          // snapshot number (1-based) that is first crossed

    // M10O: Session/snapshot state tracking
    int64_t first_new_session_record_index_ = 0;
    int64_t snapshot_records_before_first_crossing_ = 0;
    int64_t tx_index_at_first_crossing_ = 0;
    int64_t records_in_first_crossing_tx_ = 0;
};

}  // namespace qsh
