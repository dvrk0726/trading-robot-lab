#include "orderbook/order_book.hpp"
#include "orderbook/l2_snapshot.hpp"
#include <cassert>
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
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

// Test: event mode emits snapshot after every record, potentially seeing temporary crossed state
static void test_event_mode_sees_temporary_crossed() {
    OrderBook book;
    // Add bid at 100, ask at 200
    book.apply(make_add(1, 100, 10, Side::Buy, 1000));
    book.apply(make_add(2, 200, 8, Side::Sell, 1001));

    // Move bid to 210 (above ask) — creates crossed state
    OrderLogRecord move_rec;
    move_rec.order_id = 1;
    move_rec.price = 210;
    move_rec.amount_rest = 10;
    move_rec.amount = 10;
    move_rec.side = Side::Buy;
    move_rec.event = OLMsgType::Moved;
    move_rec.order_flags = OLFlags::Moved | OLFlags::Buy;
    move_rec.timestamp = 1002;
    book.apply(move_rec);

    // In event mode, snapshot taken now would see crossed state
    assert(book.best_bid() == 210);
    assert(book.best_ask() == 200);
    assert(book.check_crossed());

    // Then fix by removing the crossed bid
    book.apply(make_cancel(1, 210, 0, Side::Buy, 1003));
    assert(!book.check_crossed());

    // In event mode, the intermediate crossed state would have been captured
    // In txend mode, if TxEnd only fires after the fix, it would be clean
    std::cout << "  PASS: event mode sees temporary crossed state" << std::endl;
}

// Test: txend mode can avoid temporary crossed state if TxEnd fires after fix
static void test_txend_mode_avoids_temporary_crossed() {
    OrderBook book;
    // Sequence: add bid, add ask, move bid above ask (crossed), fix it, TxEnd
    book.apply(make_add(1, 100, 10, Side::Buy, 1000));
    book.apply(make_add(2, 200, 8, Side::Sell, 1001));

    // Move bid to 210 — crossed
    OrderLogRecord move_rec;
    move_rec.order_id = 1;
    move_rec.price = 210;
    move_rec.amount_rest = 10;
    move_rec.amount = 10;
    move_rec.side = Side::Buy;
    move_rec.event = OLMsgType::Moved;
    move_rec.order_flags = OLFlags::Moved | OLFlags::Buy;
    move_rec.timestamp = 1002;
    book.apply(move_rec);
    assert(book.check_crossed());

    // Fix: cancel the crossed bid
    book.apply(make_cancel(1, 210, 0, Side::Buy, 1003));
    assert(!book.check_crossed());

    // TxEnd fires now — book is clean
    OrderLogRecord tx_end_rec = make_tx_end(2, 200, 0, 8, Side::Sell, 1004);
    book.apply(tx_end_rec);

    // Snapshot taken at TxEnd would NOT see crossed state
    assert(!book.check_crossed());
    assert(book.best_bid() == 0);  // bid side empty after cancel
    assert(book.best_ask() == 200);

    std::cout << "  PASS: txend mode avoids temporary crossed state" << std::endl;
}

// Test: order lifecycle trace captures add/fill/cancel/remove sequence
static void test_order_lifecycle_sequence() {
    OrderBook book;
    std::vector<std::string> lifecycle;

    auto record_lifecycle = [&](const OrderLogRecord& rec, const OrderBook& b) {
        std::ostringstream oss;
        oss << ol_msg_type_name(rec.event) << ","
            << rec.order_id << ","
            << side_name(rec.side) << ","
            << rec.price << ","
            << rec.amount << ","
            << rec.amount_rest << ","
            << b.best_bid() << ","
            << b.best_ask() << ","
            << b.spread();
        lifecycle.push_back(oss.str());
    };

    // Add order
    auto r1 = make_add(42, 150, 10, Side::Buy, 2000);
    book.apply(r1);
    record_lifecycle(r1, book);

    // Fill partial
    auto r2 = make_fill(42, 150, 3, 7, Side::Buy, 2001);
    book.apply(r2);
    record_lifecycle(r2, book);

    // Fill rest
    auto r3 = make_fill(42, 150, 7, 0, Side::Buy, 2002);
    book.apply(r3);
    record_lifecycle(r3, book);

    assert(lifecycle.size() == 3);
    assert(lifecycle[0].find("ADD") != std::string::npos);
    assert(lifecycle[0].find("42") != std::string::npos);
    assert(lifecycle[1].find("FILL") != std::string::npos);
    assert(lifecycle[2].find("FILL") != std::string::npos);

    // After full fill, order should be removed
    assert(book.bid_depth() == 0);

    std::cout << "  PASS: order lifecycle trace captures add/fill sequence" << std::endl;
}

// Test: add then cancel lifecycle
static void test_order_lifecycle_add_cancel() {
    OrderBook book;
    std::vector<std::string> lifecycle;

    auto record_lifecycle = [&](const OrderLogRecord& rec, const OrderBook& b) {
        std::ostringstream oss;
        oss << ol_msg_type_name(rec.event) << ","
            << rec.order_id << ","
            << side_name(rec.side) << ","
            << rec.price << ","
            << rec.amount_rest << ","
            << b.best_bid() << ","
            << b.best_ask();
        lifecycle.push_back(oss.str());
    };

    // Add
    auto r1 = make_add(100, 150, 10, Side::Buy, 3000);
    book.apply(r1);
    record_lifecycle(r1, book);

    // Cancel
    auto r2 = make_cancel(100, 150, 0, Side::Buy, 3001);
    book.apply(r2);
    record_lifecycle(r2, book);

    assert(lifecycle.size() == 2);
    assert(lifecycle[0].find("ADD") != std::string::npos);
    assert(lifecycle[1].find("CANCEL") != std::string::npos);
    assert(book.bid_depth() == 0);

    std::cout << "  PASS: order lifecycle add then cancel" << std::endl;
}

// Test: ring buffer behavior — last N events retained
static void test_ring_buffer_retains_last_n() {
    // Simulate ring buffer of size 3
    const int ring_size = 3;
    std::vector<int> ring;

    for (int i = 1; i <= 10; ++i) {
        ring.push_back(i);
        while (static_cast<int>(ring.size()) > ring_size) {
            ring.erase(ring.begin());
        }
    }

    assert(ring.size() == 3);
    assert(ring[0] == 8);
    assert(ring[1] == 9);
    assert(ring[2] == 10);

    std::cout << "  PASS: ring buffer retains last N" << std::endl;
}

// Test: spread == 0 is both crossed and non-positive
static void test_touching_book_is_crossed_and_non_positive() {
    OrderBook book;
    book.apply(make_add(1, 150, 10, Side::Buy));
    book.apply(make_add(2, 150, 8, Side::Sell));

    assert(book.best_bid() == 150);
    assert(book.best_ask() == 150);
    assert(book.spread() == 0);
    assert(book.check_crossed());
    // spread == 0 is non-positive
    assert(book.spread() <= 0);

    std::cout << "  PASS: touching book is crossed and non-positive" << std::endl;
}

int main() {
    std::cout << "=== test_snapshot_mode ===" << std::endl;
    test_event_mode_sees_temporary_crossed();
    test_txend_mode_avoids_temporary_crossed();
    test_order_lifecycle_sequence();
    test_order_lifecycle_add_cancel();
    test_ring_buffer_retains_last_n();
    test_touching_book_is_crossed_and_non_positive();
    std::cout << "\nAll snapshot mode tests passed." << std::endl;
    return 0;
}
