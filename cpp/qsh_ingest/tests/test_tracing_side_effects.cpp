#include "orderbook/order_book.hpp"
#include <cassert>
#include <iostream>
#include <vector>

using namespace qsh;

static OrderLogRecord make_add(UID id, Price price, Volume amount, Side side, Timestamp ts = 1000) {
    OrderLogRecord rec;
    rec.order_id = id;
    rec.price = price;
    rec.amount = amount;
    rec.amount_rest = amount;
    rec.side = side;
    rec.event = OLMsgType::Add;
    rec.order_flags = OLFlags::Add | (side == Side::Buy ? OLFlags::Buy : OLFlags::Sell);
    rec.timestamp = ts;
    return rec;
}

static OrderLogRecord make_fill(UID id, Price price, Volume amount, Volume rest, Side side, Timestamp ts = 1001) {
    OrderLogRecord rec;
    rec.order_id = id;
    rec.price = price;
    rec.amount = amount;
    rec.amount_rest = rest;
    rec.side = side;
    rec.event = OLMsgType::Fill;
    rec.order_flags = OLFlags::Fill | (side == Side::Buy ? OLFlags::Buy : OLFlags::Sell);
    rec.timestamp = ts;
    return rec;
}

static OrderLogRecord make_cancel(UID id, Price price, Volume rest, Side side, Timestamp ts = 1002) {
    OrderLogRecord rec;
    rec.order_id = id;
    rec.price = price;
    rec.amount_rest = rest;
    rec.side = side;
    rec.event = rest == 0 ? OLMsgType::Remove : OLMsgType::Cancel;
    rec.order_flags = OLFlags::Canceled | (side == Side::Buy ? OLFlags::Buy : OLFlags::Sell);
    rec.timestamp = ts;
    return rec;
}

static OrderLogRecord make_tx_end(UID id, Price price, Volume amount, Volume rest, Side side, Timestamp ts = 1003) {
    OrderLogRecord rec;
    rec.order_id = id;
    rec.price = price;
    rec.amount = amount;
    rec.amount_rest = rest;
    rec.side = side;
    rec.event = OLMsgType::Fill;
    rec.order_flags = OLFlags::Fill | OLFlags::TxEnd | (side == Side::Buy ? OLFlags::Buy : OLFlags::Sell);
    rec.timestamp = ts;
    return rec;
}

static void check(bool cond, const char* msg) {
    if (!cond) {
        std::cerr << "FAIL: " << msg << std::endl;
        std::abort();
    }
}

// Test: best-level tracing does not mutate the book
static void test_best_level_tracing_no_mutation() {
    OrderBook book;
    book.apply(make_add(1, 100, 10, Side::Buy, 1000));
    book.apply(make_add(2, 100, 5, Side::Buy, 1001));
    book.apply(make_add(3, 102, 3, Side::Buy, 1002));
    book.apply(make_add(4, 200, 8, Side::Sell, 1003));
    book.apply(make_add(5, 200, 4, Side::Sell, 1004));

    auto bid_ids_before = book.best_bid_order_ids(20);
    auto ask_ids_before = book.best_ask_order_ids(20);

    // Call all tracing methods (simulating what main.cpp does)
    auto bid_ids = book.best_bid_order_ids(20);
    auto ask_ids = book.best_ask_order_ids(20);
    book.best_bid_total_qty();
    book.best_ask_total_qty();
    book.best_bid_order_count();
    book.best_ask_order_count();
    book.best_bid();
    book.best_ask();
    book.spread();
    book.mid_price();
    book.snapshot(5);

    check(bid_ids == bid_ids_before, "bid_ids changed after tracing");
    check(ask_ids == ask_ids_before, "ask_ids changed after tracing");

    // Verify idempotent: calling again yields same result
    auto bid_ids2 = book.best_bid_order_ids(20);
    auto ask_ids2 = book.best_ask_order_ids(20);
    check(bid_ids2 == bid_ids, "bid_ids not idempotent");
    check(ask_ids2 == ask_ids, "ask_ids not idempotent");

    std::cout << "  PASS: best-level tracing no mutation" << std::endl;
}

// Test: best-level tracing does not mutate crossed book
static void test_best_level_tracing_no_mutation_crossed() {
    OrderBook book;
    book.apply(make_add(1, 200, 10, Side::Buy, 1000));
    book.apply(make_add(2, 100, 8, Side::Sell, 1001));

    check(book.best_bid() >= book.best_ask(), "book should be crossed");

    auto bid_ids_before = book.best_bid_order_ids(20);
    auto ask_ids_before = book.best_ask_order_ids(20);

    auto bid_ids = book.best_bid_order_ids(20);
    auto ask_ids = book.best_ask_order_ids(20);
    book.best_bid_total_qty();
    book.best_ask_total_qty();

    check(bid_ids == bid_ids_before, "crossed bid_ids changed after tracing");
    check(ask_ids == ask_ids_before, "crossed ask_ids changed after tracing");

    std::cout << "  PASS: best-level tracing no mutation crossed" << std::endl;
}

// Test: missing-order tracing does not mutate the book
static void test_missing_order_tracing_no_mutation() {
    OrderBook book;
    book.apply(make_add(1, 100, 10, Side::Buy, 1000));
    book.apply(make_add(2, 200, 8, Side::Sell, 1001));

    Price bb = book.best_bid();
    Price ba = book.best_ask();

    // Try to fill a non-existent order (triggers missing_order_id)
    book.apply(make_fill(999, 100, 5, 0, Side::Buy, 1002));

    check(book.errors().missing_order_id == 1, "missing_order_id should be 1");
    check(book.best_bid() == bb, "best_bid changed after missing order");
    check(book.best_ask() == ba, "best_ask changed after missing order");
    check(book.best_bid_total_qty() == 10, "bid qty changed after missing order");
    check(book.best_ask_total_qty() == 8, "ask qty changed after missing order");

    // Call tracing methods after missing order event
    book.best_bid_order_ids(20);
    book.best_ask_order_ids(20);

    check(book.best_bid() == bb, "best_bid changed after tracing");
    check(book.best_ask() == ba, "best_ask changed after tracing");

    std::cout << "  PASS: missing-order tracing no mutation" << std::endl;
}

// Test: fill with dangling reference fix - full fill
static void test_full_fill_no_stale_state() {
    OrderBook book;
    book.apply(make_add(1, 100, 10, Side::Buy, 1000));
    book.apply(make_add(2, 100, 5, Side::Buy, 1001));
    book.apply(make_add(3, 200, 8, Side::Sell, 1002));

    check(book.best_bid_order_count() == 2, "should have 2 bid orders");
    check(book.best_bid_total_qty() == 15, "bid qty should be 15");

    // Fully fill order 1
    book.apply(make_fill(1, 100, 10, 0, Side::Buy, 1003));

    check(book.best_bid() == 100, "best_bid should be 100");
    check(book.best_bid_total_qty() == 5, "bid qty should be 5 after fill");
    check(book.best_bid_order_count() == 1, "should have 1 bid order after fill");
    auto ids = book.best_bid_order_ids(20);
    check(ids.size() == 1, "should have 1 bid order id");
    check(ids[0] == 2, "remaining order should be 2");

    std::cout << "  PASS: full fill no stale state" << std::endl;
}

// Test: fill with dangling reference fix - full fill removes level
static void test_full_fill_removes_level() {
    OrderBook book;
    book.apply(make_add(1, 100, 10, Side::Buy, 1000));
    book.apply(make_add(2, 200, 8, Side::Sell, 1001));

    // Fully fill the only bid order
    book.apply(make_fill(1, 100, 10, 0, Side::Buy, 1002));

    check(book.best_bid() == 0, "bid side should be empty");
    check(book.bid_depth() == 0, "bid depth should be 0");
    check(book.best_bid_total_qty() == 0, "bid qty should be 0");
    check(book.best_bid_order_count() == 0, "bid order count should be 0");
    check(book.best_ask() == 200, "ask should be 200");
    check(book.best_ask_total_qty() == 8, "ask qty should be 8");

    std::cout << "  PASS: full fill removes level" << std::endl;
}

// Test: baseline vs traced txend produce same L2 counters
static void test_baseline_vs_traced_txend_same_counters() {
    std::vector<OrderLogRecord> events;
    events.push_back(make_add(1, 100, 10, Side::Buy, 1000));
    events.push_back(make_add(2, 200, 8, Side::Sell, 1001));
    // Move bid above ask (crossed)
    OrderLogRecord move_rec;
    move_rec.order_id = 1;
    move_rec.price = 210;
    move_rec.amount_rest = 10;
    move_rec.amount = 10;
    move_rec.side = Side::Buy;
    move_rec.event = OLMsgType::Moved;
    move_rec.order_flags = OLFlags::Moved | OLFlags::Buy;
    move_rec.timestamp = 1002;
    events.push_back(move_rec);
    events.push_back(make_cancel(1, 210, 0, Side::Buy, 1003));
    events.push_back(make_tx_end(2, 200, 0, 8, Side::Sell, 1004));
    events.push_back(make_add(3, 150, 5, Side::Buy, 1005));
    events.push_back(make_add(4, 160, 3, Side::Sell, 1006));
    events.push_back(make_tx_end(4, 160, 0, 3, Side::Sell, 1007));

    // Run baseline (no tracing)
    OrderBook book_baseline;
    int64_t crossed_baseline = 0;
    int64_t snapshots_baseline = 0;
    for (const auto& rec : events) {
        book_baseline.apply(rec);
        if (rec.order_flags & OLFlags::TxEnd) {
            ++snapshots_baseline;
            if (book_baseline.best_bid() > 0 && book_baseline.best_ask() > 0 &&
                book_baseline.best_bid() >= book_baseline.best_ask()) {
                ++crossed_baseline;
            }
        }
    }

    // Run traced (with best-level tracing calls)
    OrderBook book_traced;
    int64_t crossed_traced = 0;
    int64_t snapshots_traced = 0;
    for (const auto& rec : events) {
        book_traced.apply(rec);
        if (rec.order_flags & OLFlags::TxEnd) {
            ++snapshots_traced;
            Price bb = book_traced.best_bid();
            Price ba = book_traced.best_ask();
            if (bb > 0 && ba > 0 && bb >= ba) {
                ++crossed_traced;
            }
            // Call tracing methods (side-effect-free)
            book_traced.best_bid_order_ids(20);
            book_traced.best_ask_order_ids(20);
            book_traced.best_bid_total_qty();
            book_traced.best_ask_total_qty();
            book_traced.best_bid_order_count();
            book_traced.best_ask_order_count();
        }
    }

    check(crossed_baseline == crossed_traced, "crossed count differs");
    check(snapshots_baseline == snapshots_traced, "snapshot count differs");
    check(book_baseline.best_bid() == book_traced.best_bid(), "final best_bid differs");
    check(book_baseline.best_ask() == book_traced.best_ask(), "final best_ask differs");

    std::cout << "  PASS: baseline vs traced txend same counters" << std::endl;
}

// Test: auto-trace does not change crossed-book count
static void test_auto_trace_no_change_crossed_count() {
    std::vector<OrderLogRecord> events;
    events.push_back(make_add(1, 200, 10, Side::Buy, 1000));
    events.push_back(make_add(2, 100, 8, Side::Sell, 1001));
    events.push_back(make_tx_end(2, 100, 0, 8, Side::Sell, 1002));
    events.push_back(make_add(3, 150, 5, Side::Buy, 1003));
    events.push_back(make_add(4, 160, 3, Side::Sell, 1004));
    events.push_back(make_tx_end(4, 160, 0, 3, Side::Sell, 1005));

    // Run without auto-trace
    OrderBook book_no_trace;
    int64_t crossed_no_trace = 0;
    for (const auto& rec : events) {
        book_no_trace.apply(rec);
        if (rec.order_flags & OLFlags::TxEnd) {
            Price bb = book_no_trace.best_bid();
            Price ba = book_no_trace.best_ask();
            if (bb > 0 && ba > 0 && bb >= ba) {
                ++crossed_no_trace;
            }
        }
    }

    // Run with auto-trace (simulates selecting order IDs from first crossed)
    OrderBook book_with_trace;
    int64_t crossed_with_trace = 0;
    bool auto_trace_ids_selected = false;
    for (const auto& rec : events) {
        book_with_trace.apply(rec);
        if (rec.order_flags & OLFlags::TxEnd) {
            Price bb = book_with_trace.best_bid();
            Price ba = book_with_trace.best_ask();
            if (bb > 0 && ba > 0 && bb >= ba) {
                ++crossed_with_trace;
                if (!auto_trace_ids_selected) {
                    book_with_trace.best_bid_order_ids(1);
                    book_with_trace.best_ask_order_ids(1);
                    auto_trace_ids_selected = true;
                }
            }
        }
    }

    check(crossed_no_trace == crossed_with_trace, "auto-trace changed crossed count");

    std::cout << "  PASS: auto-trace no change crossed count" << std::endl;
}

// Test: fill order then check best-level order IDs
static void test_fill_then_best_level_ids() {
    OrderBook book;
    book.apply(make_add(1, 100, 10, Side::Buy, 1000));
    book.apply(make_add(2, 100, 5, Side::Buy, 1001));
    book.apply(make_add(3, 100, 3, Side::Buy, 1002));
    book.apply(make_add(4, 200, 8, Side::Sell, 1003));

    check(book.best_bid_order_count() == 3, "should have 3 bid orders");
    auto ids_before = book.best_bid_order_ids(20);
    check(ids_before.size() == 3, "should have 3 bid order ids");

    // Fully fill order 2
    book.apply(make_fill(2, 100, 5, 0, Side::Buy, 1004));

    check(book.best_bid_order_count() == 2, "should have 2 bid orders after fill");
    auto ids_after = book.best_bid_order_ids(20);
    check(ids_after.size() == 2, "should have 2 bid order ids after fill");
    check(ids_after[0] == 1, "first remaining order should be 1");
    check(ids_after[1] == 3, "second remaining order should be 3");
    check(book.best_bid_total_qty() == 13, "bid qty should be 13");

    std::cout << "  PASS: fill then best level ids" << std::endl;
}

int main() {
    std::cout << "=== test_tracing_side_effects ===" << std::endl;
    test_best_level_tracing_no_mutation();
    test_best_level_tracing_no_mutation_crossed();
    test_missing_order_tracing_no_mutation();
    test_full_fill_no_stale_state();
    test_full_fill_removes_level();
    test_baseline_vs_traced_txend_same_counters();
    test_auto_trace_no_change_crossed_count();
    test_fill_then_best_level_ids();
    std::cout << "\nAll tracing side-effects tests passed." << std::endl;
    return 0;
}
