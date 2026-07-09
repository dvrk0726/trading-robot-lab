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

// Error counters for book reconstruction.
struct BookErrors {
    int64_t missing_order_id = 0;
    int64_t level_not_found = 0;
    int64_t amount_mismatch = 0;
    int64_t negative_level_volume = 0;
    int64_t crossed_book_after_update = 0;
    int64_t invalid_side = 0;
};

// L3 order book reconstruction from OrdLog events.
class OrderBook {
public:
    // Apply an OrderLogRecord to the book.
    // Returns true on success, false on error (error counted internally).
    bool apply(const OrderLogRecord& rec);

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

    // Set fill semantics mode: "delta" (amount = filled qty) or "rest" (amount = original, fill = amount - amount_rest).
    void set_fill_delta_mode(bool use_delta) { fill_delta_mode_ = use_delta; }

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

    void add_order(const OrderLogRecord& rec);
    void fill_order(const OrderLogRecord& rec);
    void cancel_order(const OrderLogRecord& rec);
    void remove_order(const OrderLogRecord& rec);
    void move_order(const OrderLogRecord& rec);
    bool check_crossed() const;
};

}  // namespace qsh
