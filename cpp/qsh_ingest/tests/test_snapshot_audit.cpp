#include "orderbook/order_book.hpp"
#include <cassert>
#include <iostream>
#include <vector>
#include <map>
#include <functional>

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

static OrderLogRecord make_snapshot_add(UID id, Price price, Volume amount, Side side, Timestamp ts = 1000) {
    OrderLogRecord rec;
    rec.order_id = id;
    rec.price = price;
    rec.amount = amount;
    rec.amount_rest = amount;
    rec.side = side;
    rec.event = OLMsgType::Add;
    rec.order_flags = OLFlags::Snapshot | OLFlags::Add | (side == Side::Buy ? OLFlags::Buy : OLFlags::Sell);
    rec.timestamp = ts;
    return rec;
}

static OrderLogRecord make_new_session(Timestamp ts = 1000) {
    OrderLogRecord rec;
    rec.order_id = 0;
    rec.price = 0;
    rec.amount = 0;
    rec.amount_rest = 0;
    rec.side = Side::Buy;
    rec.event = OLMsgType::Unknown;
    rec.order_flags = OLFlags::NewSession;
    rec.timestamp = ts;
    return rec;
}

// Test: snapshot records are correctly identified by Snapshot flag
static void test_snapshot_flag_detection() {
    auto snap = make_snapshot_add(1, 100, 10, Side::Buy);
    auto normal = make_add(2, 200, 8, Side::Sell);

    assert(has_flag(snap.order_flags, OLFlags::Snapshot));
    assert(!has_flag(normal.order_flags, OLFlags::Snapshot));

    std::cout << "  PASS: snapshot flag detection" << std::endl;
}

// Test: snapshot records load into book correctly
static void test_snapshot_book_loading() {
    OrderBook book;

    // Simulate NewSession then snapshot records
    book.apply(make_new_session(1000));
    book.apply(make_snapshot_add(1, 100, 10, Side::Buy, 1001));
    book.apply(make_snapshot_add(2, 90, 5, Side::Buy, 1002));
    book.apply(make_snapshot_add(3, 200, 8, Side::Sell, 1003));
    book.apply(make_snapshot_add(4, 210, 12, Side::Sell, 1004));

    assert(book.bid_depth() == 2);
    assert(book.ask_depth() == 2);
    assert(book.best_bid() == 100);
    assert(book.best_ask() == 200);
    assert(book.spread() == 100);

    std::cout << "  PASS: snapshot book loading" << std::endl;
}

// Test: crossed book detection after snapshot loading
static void test_crossed_book_detection() {
    OrderBook book;

    book.apply(make_new_session(1000));
    // Bid at 100, ask at 90 -> crossed
    book.apply(make_snapshot_add(1, 100, 10, Side::Buy, 1001));
    book.apply(make_snapshot_add(2, 90, 8, Side::Sell, 1002));

    assert(book.best_bid() == 100);
    assert(book.best_ask() == 90);
    assert(book.best_bid() >= book.best_ask());

    std::cout << "  PASS: crossed book detection" << std::endl;
}

// Test: snapshot records counted correctly
static void test_snapshot_record_counting() {
    OrderBook book;
    int64_t snapshot_count = 0;

    auto count_snapshot = [&](const OrderLogRecord& rec) {
        if (has_flag(rec.order_flags, OLFlags::Snapshot)) {
            ++snapshot_count;
        }
        book.apply(rec);
    };

    count_snapshot(make_new_session(1000));
    count_snapshot(make_snapshot_add(1, 100, 10, Side::Buy, 1001));
    count_snapshot(make_snapshot_add(2, 200, 8, Side::Sell, 1002));
    count_snapshot(make_snapshot_add(3, 90, 5, Side::Buy, 1003));

    assert(snapshot_count == 3);
    assert(book.errors().snapshot_records_seen == 3);

    std::cout << "  PASS: snapshot record counting" << std::endl;
}

// Test: empty snapshot handling
static void test_empty_snapshot() {
    OrderBook book;

    book.apply(make_new_session(1000));
    // No snapshot records, just a normal add
    book.apply(make_add(1, 100, 10, Side::Buy, 1001));

    assert(book.bid_depth() == 1);
    assert(book.best_bid() == 100);
    assert(book.ask_depth() == 0);

    std::cout << "  PASS: empty snapshot handling" << std::endl;
}

// Test: snapshot buy/sell price tracking
static void test_snapshot_price_tracking() {
    std::vector<OrderLogRecord> records;
    records.push_back(make_new_session(1000));
    records.push_back(make_snapshot_add(1, 100, 10, Side::Buy, 1001));
    records.push_back(make_snapshot_add(2, 95, 5, Side::Buy, 1002));
    records.push_back(make_snapshot_add(3, 200, 8, Side::Sell, 1003));
    records.push_back(make_snapshot_add(4, 210, 12, Side::Sell, 1004));

    Price min_buy = 0, max_buy = 0, min_sell = 0, max_sell = 0;
    int64_t buy_count = 0, sell_count = 0;

    for (const auto& rec : records) {
        if (!has_flag(rec.order_flags, OLFlags::Snapshot)) continue;
        if (rec.side == Side::Buy) {
            ++buy_count;
            if (min_buy == 0 || rec.price < min_buy) min_buy = rec.price;
            if (rec.price > max_buy) max_buy = rec.price;
        } else if (rec.side == Side::Sell) {
            ++sell_count;
            if (min_sell == 0 || rec.price < min_sell) min_sell = rec.price;
            if (rec.price > max_sell) max_sell = rec.price;
        }
    }

    assert(buy_count == 2);
    assert(sell_count == 2);
    assert(min_buy == 95);
    assert(max_buy == 100);
    assert(min_sell == 200);
    assert(max_sell == 210);

    std::cout << "  PASS: snapshot price tracking" << std::endl;
}

// Test: summary fields are deterministic
static void test_summary_determinism() {
    // Run the same sequence twice, verify same results
    auto run_sequence = []() -> std::pair<Price, Price> {
        OrderBook book;
        book.apply(make_new_session(1000));
        book.apply(make_snapshot_add(1, 100, 10, Side::Buy, 1001));
        book.apply(make_snapshot_add(2, 200, 8, Side::Sell, 1002));
        return {book.best_bid(), book.best_ask()};
    };

    auto [bb1, ba1] = run_sequence();
    auto [bb2, ba2] = run_sequence();

    assert(bb1 == bb2);
    assert(ba1 == ba2);

    std::cout << "  PASS: summary determinism" << std::endl;
}

int main() {
    std::cout << "=== test_snapshot_audit ===" << std::endl;
    test_snapshot_flag_detection();
    test_snapshot_book_loading();
    test_crossed_book_detection();
    test_snapshot_record_counting();
    test_empty_snapshot();
    test_snapshot_price_tracking();
    test_summary_determinism();
    std::cout << "\nAll snapshot audit tests passed." << std::endl;
    return 0;
}
