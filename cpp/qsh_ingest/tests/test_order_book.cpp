#include "orderbook/order_book.hpp"
#include <cassert>
#include <iostream>
#include <cmath>

using namespace qsh;

static OrderLogRecord make_add(UID id, Price price, Volume amount, Side side) {
    OrderLogRecord rec;
    rec.order_id = id;
    rec.price = price;
    rec.amount = amount;
    rec.amount_rest = amount;
    rec.side = side;
    rec.event = OLMsgType::Add;
    rec.order_flags = OLFlags::Add | (side == Side::Buy ? OLFlags::Buy : OLFlags::Sell);
    rec.timestamp = 1000;
    return rec;
}

static OrderLogRecord make_fill(UID id, Price price, Volume amount, Volume rest, Side side) {
    OrderLogRecord rec;
    rec.order_id = id;
    rec.price = price;
    rec.amount = amount;
    rec.amount_rest = rest;
    rec.side = side;
    rec.event = OLMsgType::Fill;
    rec.order_flags = OLFlags::Fill | (side == Side::Buy ? OLFlags::Buy : OLFlags::Sell);
    rec.timestamp = 1001;
    return rec;
}

static OrderLogRecord make_cancel(UID id, Price price, Volume rest, Side side) {
    OrderLogRecord rec;
    rec.order_id = id;
    rec.price = price;
    rec.amount_rest = rest;
    rec.side = side;
    rec.event = rest == 0 ? OLMsgType::Remove : OLMsgType::Cancel;
    rec.order_flags = OLFlags::Canceled | (side == Side::Buy ? OLFlags::Buy : OLFlags::Sell);
    rec.timestamp = 1002;
    return rec;
}

static OrderLogRecord make_move(UID id, Price new_price, Volume amount_rest, Side side) {
    OrderLogRecord rec;
    rec.order_id = id;
    rec.price = new_price;
    rec.amount_rest = amount_rest;
    rec.amount = amount_rest;
    rec.side = side;
    rec.event = OLMsgType::Moved;
    rec.order_flags = OLFlags::Moved | (side == Side::Buy ? OLFlags::Buy : OLFlags::Sell);
    rec.timestamp = 1003;
    return rec;
}

static void test_add_orders() {
    OrderBook book;
    book.apply(make_add(1, 100, 10, Side::Buy));
    book.apply(make_add(2, 102, 5, Side::Buy));
    book.apply(make_add(3, 200, 8, Side::Sell));
    book.apply(make_add(4, 202, 3, Side::Sell));

    assert(book.bid_depth() == 2);
    assert(book.ask_depth() == 2);
    assert(book.best_bid() == 102);
    assert(book.best_ask() == 200);
    std::cout << "  PASS: add orders" << std::endl;
}

static void test_snapshot() {
    OrderBook book;
    book.apply(make_add(1, 100, 10, Side::Buy));
    book.apply(make_add(2, 102, 5, Side::Buy));
    book.apply(make_add(3, 200, 8, Side::Sell));
    book.apply(make_add(4, 202, 3, Side::Sell));

    auto snap = book.snapshot(2);
    assert(snap.size() == 2);
    // Bid sorted descending: 102, 100
    assert(snap[0].bid_price == 102);
    assert(snap[0].bid_qty == 5);
    assert(snap[1].bid_price == 100);
    assert(snap[1].bid_qty == 10);
    // Ask sorted ascending: 200, 202
    assert(snap[0].ask_price == 200);
    assert(snap[0].ask_qty == 8);
    assert(snap[1].ask_price == 202);
    assert(snap[1].ask_qty == 3);
    std::cout << "  PASS: snapshot" << std::endl;
}

static void test_mid_price() {
    OrderBook book;
    book.apply(make_add(1, 100, 10, Side::Buy));
    book.apply(make_add(2, 200, 8, Side::Sell));
    assert(std::abs(book.mid_price() - 150.0) < 0.01);
    assert(book.spread() == 100);
    std::cout << "  PASS: mid price and spread" << std::endl;
}

static void test_fill_reduces_level() {
    OrderBook book;
    book.apply(make_add(1, 100, 10, Side::Buy));
    book.apply(make_add(2, 100, 5, Side::Buy));
    assert(book.snapshot(1)[0].bid_qty == 15);

    // Fill 3 from order 1
    book.apply(make_fill(1, 100, 3, 7, Side::Buy));
    auto snap = book.snapshot(1);
    assert(snap[0].bid_qty == 12);
    std::cout << "  PASS: fill reduces level" << std::endl;
}

static void test_fill_removes_order() {
    OrderBook book;
    book.apply(make_add(1, 100, 10, Side::Buy));
    book.apply(make_add(2, 100, 5, Side::Buy));

    // Fully fill order 1
    book.apply(make_fill(1, 100, 10, 0, Side::Buy));
    auto snap = book.snapshot(1);
    assert(snap[0].bid_qty == 5);
    assert(snap[0].bid_price == 100);
    std::cout << "  PASS: fill removes order" << std::endl;
}

static void test_cancel_full() {
    OrderBook book;
    book.apply(make_add(1, 100, 10, Side::Buy));
    book.apply(make_add(2, 200, 8, Side::Sell));

    book.apply(make_cancel(1, 100, 0, Side::Buy));
    assert(book.bid_depth() == 0);
    assert(book.ask_depth() == 1);
    std::cout << "  PASS: full cancel" << std::endl;
}

static void test_cancel_partial() {
    OrderBook book;
    book.apply(make_add(1, 100, 10, Side::Buy));

    book.apply(make_cancel(1, 100, 4, Side::Buy));
    assert(book.snapshot(1)[0].bid_qty == 4);
    std::cout << "  PASS: partial cancel" << std::endl;
}

static void test_clear() {
    OrderBook book;
    book.apply(make_add(1, 100, 10, Side::Buy));
    book.apply(make_add(2, 200, 8, Side::Sell));
    book.clear();
    assert(book.bid_depth() == 0);
    assert(book.ask_depth() == 0);
    std::cout << "  PASS: clear" << std::endl;
}

static void test_empty_book() {
    OrderBook book;
    assert(book.mid_price() == 0.0);
    assert(book.spread() == 0);
    assert(book.best_bid() == 0);
    assert(book.best_ask() == 0);
    auto snap = book.snapshot(5);
    assert(snap.size() == 5);
    for (const auto& row : snap) {
        (void)row;
        assert(row.bid_price == 0);
        assert(row.ask_price == 0);
    }
    std::cout << "  PASS: empty book" << std::endl;
}

// Test: move bid order to higher price
static void test_move_bid_up() {
    OrderBook book;
    book.apply(make_add(1, 100, 10, Side::Buy));
    book.apply(make_add(2, 102, 5, Side::Buy));
    book.apply(make_add(3, 200, 8, Side::Sell));

    assert(book.best_bid() == 102);

    // Move order 1 from 100 to 105
    book.apply(make_move(1, 105, 10, Side::Buy));

    // Old level should be gone, new level at 105
    assert(book.bid_depth() == 2);
    assert(book.best_bid() == 105);
    assert(book.best_ask() == 200);
    assert(book.spread() == 95);
    std::cout << "  PASS: move bid up" << std::endl;
}

// Test: move ask order to lower price
static void test_move_ask_down() {
    OrderBook book;
    book.apply(make_add(1, 100, 10, Side::Buy));
    book.apply(make_add(2, 200, 8, Side::Sell));
    book.apply(make_add(3, 202, 3, Side::Sell));

    assert(book.best_ask() == 200);

    // Move order 2 from 200 to 195
    book.apply(make_move(2, 195, 8, Side::Sell));

    // Old level should be gone, new level at 195
    assert(book.ask_depth() == 2);
    assert(book.best_bid() == 100);
    assert(book.best_ask() == 195);
    assert(book.spread() == 95);
    std::cout << "  PASS: move ask down" << std::endl;
}

// Test: move order across spread creates crossed book (bad input)
static void test_move_creates_crossed() {
    OrderBook book;
    book.apply(make_add(1, 100, 10, Side::Buy));
    book.apply(make_add(2, 200, 8, Side::Sell));

    assert(!book.check_crossed());

    // Move bid order to 210 (above ask) - simulates bad input
    book.apply(make_move(1, 210, 10, Side::Buy));

    // Book should now be crossed
    assert(book.best_bid() == 210);
    assert(book.best_ask() == 200);
    assert(book.check_crossed());
    std::cout << "  PASS: move creates crossed (bad input)" << std::endl;
}

// Test: move order with partial amount_rest
static void test_move_partial_rest() {
    OrderBook book;
    book.apply(make_add(1, 100, 10, Side::Buy));
    book.apply(make_add(2, 200, 8, Side::Sell));

    // Move order 1 from 100 to 105 with only 6 remaining
    book.apply(make_move(1, 105, 6, Side::Buy));

    assert(book.bid_depth() == 1);
    assert(book.best_bid() == 105);
    auto snap = book.snapshot(1);
    assert(snap[0].bid_qty == 6);
    std::cout << "  PASS: move partial rest" << std::endl;
}

// Test: normal bid < ask state
static void test_normal_bid_below_ask() {
    OrderBook book;
    book.apply(make_add(1, 100, 10, Side::Buy));
    book.apply(make_add(2, 110, 5, Side::Buy));
    book.apply(make_add(3, 200, 8, Side::Sell));
    book.apply(make_add(4, 210, 3, Side::Sell));

    assert(book.best_bid() == 110);
    assert(book.best_ask() == 200);
    assert(book.best_bid() < book.best_ask());
    assert(!book.check_crossed());
    assert(book.spread() == 90);
    std::cout << "  PASS: normal bid below ask" << std::endl;
}

// Test: spread counter consistency
static void test_spread_counter_consistency() {
    OrderBook book;
    book.apply(make_add(1, 100, 10, Side::Buy));
    book.apply(make_add(2, 200, 8, Side::Sell));

    // Normal spread > 0
    assert(book.spread() > 0);
    assert(!book.check_crossed());

    // Touching book: spread == 0, crossed
    book.clear();
    book.apply(make_add(3, 150, 10, Side::Buy));
    book.apply(make_add(4, 150, 8, Side::Sell));
    assert(book.spread() == 0);
    assert(book.check_crossed());

    // Crossed book: spread < 0, crossed
    book.clear();
    book.apply(make_add(5, 200, 10, Side::Buy));
    book.apply(make_add(6, 100, 8, Side::Sell));
    assert(book.spread() == -100);
    assert(book.check_crossed());

    std::cout << "  PASS: spread counter consistency" << std::endl;
}

int main() {
    std::cout << "=== test_order_book ===" << std::endl;
    test_add_orders();
    test_snapshot();
    test_mid_price();
    test_fill_reduces_level();
    test_fill_removes_order();
    test_cancel_full();
    test_cancel_partial();
    test_clear();
    test_empty_book();
    test_move_bid_up();
    test_move_ask_down();
    test_move_creates_crossed();
    test_move_partial_rest();
    test_normal_bid_below_ask();
    test_spread_counter_consistency();
    std::cout << "\nAll order book tests passed." << std::endl;
    return 0;
}
