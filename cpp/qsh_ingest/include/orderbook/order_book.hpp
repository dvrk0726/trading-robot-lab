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

private:
    // Levels stored as price -> (volume, order_count), sorted.
    // Bid: descending by price (highest first).
    // Ask: ascending by price (lowest first).
    std::map<Price, std::pair<Volume, int>, std::greater<Price>> bid_levels_;
    std::map<Price, std::pair<Volume, int>> ask_levels_;

    // Order tracking: order_id -> (side, price, amount)
    struct OrderInfo {
        Side side = Side::Unknown;
        Price price = 0;
        Volume amount = 0;
    };
    std::map<UID, OrderInfo> orders_;

    BookErrors errors_;
    Timestamp last_ts_ = 0;

    void add_order(const OrderLogRecord& rec);
    void fill_order(const OrderLogRecord& rec);
    void cancel_order(const OrderLogRecord& rec);
    void remove_order(const OrderLogRecord& rec);
    bool check_crossed() const;
};

}  // namespace qsh
