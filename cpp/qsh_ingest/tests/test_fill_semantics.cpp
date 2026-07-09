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

// Helper: get order quantity (ignores side/price)
[[maybe_unused]] static Volume get_qty(const OrderBook& book, UID id) {
    Side s; Price p; Volume q;
    if (!book.get_order_info(id, s, p, q)) return -1;
    (void)s; (void)p;
    return q;
}

// Test: partial fill in delta mode (amount = filled qty, amount_rest = remaining)
static void test_partial_fill_delta_mode() {
    OrderBook book;
    book.set_fill_delta_mode(true);
    book.apply(make_add(1, 100, 10, Side::Buy));
    book.apply(make_add(2, 100, 5, Side::Buy));
    assert(book.snapshot(1)[0].bid_qty == 15);

    // Fill 3 from order 1: amount=3 (delta), amount_rest=7 (remaining)
    book.apply(make_fill(1, 100, 3, 7, Side::Buy));
    auto snap = book.snapshot(1);
    assert(snap[0].bid_qty == 12);  // 15 - 3 = 12
    assert(get_qty(book, 1) == 7);

    std::cout << "  PASS: partial fill delta mode" << std::endl;
}

// Test: partial fill in rest mode (amount = original qty, fill = amount - amount_rest)
static void test_partial_fill_rest_mode() {
    OrderBook book;
    book.set_fill_delta_mode(false);
    book.apply(make_add(1, 100, 10, Side::Buy));
    book.apply(make_add(2, 100, 5, Side::Buy));
    assert(book.snapshot(1)[0].bid_qty == 15);

    // In rest mode: amount=10 (original), amount_rest=7 (remaining), fill = 10-7 = 3
    book.apply(make_fill(1, 100, 10, 7, Side::Buy));
    auto snap = book.snapshot(1);
    assert(snap[0].bid_qty == 12);  // 15 - 3 = 12
    assert(get_qty(book, 1) == 7);

    std::cout << "  PASS: partial fill rest mode" << std::endl;
}

// Test: full fill removes order in delta mode
static void test_full_fill_delta_mode() {
    OrderBook book;
    book.set_fill_delta_mode(true);
    book.apply(make_add(1, 100, 10, Side::Buy));
    book.apply(make_add(2, 100, 5, Side::Buy));

    // Fully fill order 1: amount=10 (delta), amount_rest=0
    book.apply(make_fill(1, 100, 10, 0, Side::Buy));
    auto snap = book.snapshot(1);
    assert(snap[0].bid_qty == 5);
    assert(get_qty(book, 1) == -1);  // order gone

    std::cout << "  PASS: full fill delta mode" << std::endl;
}

// Test: full fill removes order in rest mode
static void test_full_fill_rest_mode() {
    OrderBook book;
    book.set_fill_delta_mode(false);
    book.apply(make_add(1, 100, 10, Side::Buy));
    book.apply(make_add(2, 100, 5, Side::Buy));

    // In rest mode: amount=10 (original), amount_rest=0, fill = 10-0 = 10
    book.apply(make_fill(1, 100, 10, 0, Side::Buy));
    auto snap = book.snapshot(1);
    assert(snap[0].bid_qty == 5);
    assert(get_qty(book, 1) == -1);  // order gone

    std::cout << "  PASS: full fill rest mode" << std::endl;
}

// Test: cancel with amount_rest removes partial volume
static void test_cancel_partial_semantics() {
    OrderBook book;
    book.apply(make_add(1, 100, 10, Side::Buy));
    book.apply(make_add(2, 200, 8, Side::Sell));

    // Cancel 6 from order 1 (amount_rest=4 means 4 remaining)
    book.apply(make_cancel(1, 100, 4, Side::Buy));
    assert(book.snapshot(1)[0].bid_qty == 4);
    assert(get_qty(book, 1) == 4);

    std::cout << "  PASS: cancel partial semantics" << std::endl;
}

// Test: cancel with amount_rest=0 removes order completely
static void test_cancel_full_semantics() {
    OrderBook book;
    book.apply(make_add(1, 100, 10, Side::Buy));
    book.apply(make_add(2, 200, 8, Side::Sell));

    // Full cancel: amount_rest=0
    book.apply(make_cancel(1, 100, 0, Side::Buy));
    assert(book.bid_depth() == 0);
    assert(get_qty(book, 1) == -1);

    std::cout << "  PASS: cancel full semantics" << std::endl;
}

// Test: remove removes order completely
static void test_remove_semantics() {
    OrderBook book;
    book.apply(make_add(1, 100, 10, Side::Buy));
    book.apply(make_add(2, 200, 8, Side::Sell));

    // Remove via CrossTrade flag
    OrderLogRecord rec;
    rec.order_id = 1;
    rec.price = 100;
    rec.amount_rest = 0;
    rec.amount = 0;
    rec.side = Side::Buy;
    rec.event = OLMsgType::Remove;
    rec.order_flags = OLFlags::CrossTrade | OLFlags::Buy;
    rec.timestamp = 1002;
    book.apply(rec);

    assert(book.bid_depth() == 0);
    assert(get_qty(book, 1) == -1);

    std::cout << "  PASS: remove semantics" << std::endl;
}

// Test: move with amount_rest changes quantity
static void test_move_semantics_with_rest() {
    OrderBook book;
    book.apply(make_add(1, 100, 10, Side::Buy));
    book.apply(make_add(2, 200, 8, Side::Sell));

    // Move order 1 from 100 to 105 with reduced quantity 6
    book.apply(make_move(1, 105, 6, Side::Buy));

    assert(book.best_bid() == 105);
    auto snap = book.snapshot(1);
    assert(snap[0].bid_qty == 6);
    assert(get_qty(book, 1) == 6);

    std::cout << "  PASS: move semantics with rest" << std::endl;
}

// Test: move with amount_rest=0 keeps old amount (fallback)
static void test_move_zero_rest_keeps_old() {
    OrderBook book;
    book.apply(make_add(1, 100, 10, Side::Buy));
    book.apply(make_add(2, 200, 8, Side::Sell));

    // Move order 1 from 100 to 105 with amount_rest=0 (should keep old amount=10)
    book.apply(make_move(1, 105, 0, Side::Buy));

    assert(book.best_bid() == 105);
    auto snap = book.snapshot(1);
    assert(snap[0].bid_qty == 10);  // kept old amount

    std::cout << "  PASS: move zero rest keeps old" << std::endl;
}

// Test: missing_order_id does not leave stale active volume
static void test_missing_order_no_stale_volume() {
    OrderBook book;
    book.apply(make_add(1, 100, 10, Side::Buy));
    book.apply(make_add(2, 200, 8, Side::Sell));

    [[maybe_unused]] Volume bid_qty_before = book.best_bid_total_qty();
    [[maybe_unused]] Volume ask_qty_before = book.best_ask_total_qty();

    // Try to fill a non-existent order
    book.apply(make_fill(999, 100, 5, 0, Side::Buy));

    // Volume should not change
    assert(book.best_bid_total_qty() == bid_qty_before);
    assert(book.best_ask_total_qty() == ask_qty_before);
    assert(book.errors().missing_order_id == 1);

    std::cout << "  PASS: missing order no stale volume" << std::endl;
}

// Test: multiple partial fills reduce order correctly in delta mode
static void test_multiple_partial_fills_delta() {
    OrderBook book;
    book.set_fill_delta_mode(true);
    book.apply(make_add(1, 100, 10, Side::Buy));

    // Fill 3 (remaining 7)
    book.apply(make_fill(1, 100, 3, 7, Side::Buy));
    assert(get_qty(book, 1) == 7);

    // Fill 4 more (remaining 3)
    book.apply(make_fill(1, 100, 4, 3, Side::Buy));
    assert(get_qty(book, 1) == 3);

    // Fill last 3 (remaining 0)
    book.apply(make_fill(1, 100, 3, 0, Side::Buy));
    assert(get_qty(book, 1) == -1);

    std::cout << "  PASS: multiple partial fills delta" << std::endl;
}

// Test: get_order_info returns false for unknown order
static void test_get_order_info_unknown() {
    OrderBook book;
    Side s; Price p; Volume q;
    assert(!book.get_order_info(999, s, p, q));
    (void)s; (void)p; (void)q;

    std::cout << "  PASS: get order info unknown" << std::endl;
}

// Test: fill semantics mode is preserved across operations
static void test_fill_mode_preserved() {
    OrderBook book;
    book.set_fill_delta_mode(false);
    book.apply(make_add(1, 100, 10, Side::Buy));
    book.apply(make_add(2, 200, 8, Side::Sell));

    // In rest mode: amount=10, rest=7, fill=3
    book.apply(make_fill(1, 100, 10, 7, Side::Buy));
    assert(get_qty(book, 1) == 7);

    // Still in rest mode: amount=7, rest=2, fill=5
    book.apply(make_fill(1, 100, 7, 2, Side::Buy));
    assert(get_qty(book, 1) == 2);

    std::cout << "  PASS: fill mode preserved" << std::endl;
}

int main() {
    std::cout << "=== test_fill_semantics ===" << std::endl;
    test_partial_fill_delta_mode();
    test_partial_fill_rest_mode();
    test_full_fill_delta_mode();
    test_full_fill_rest_mode();
    test_cancel_partial_semantics();
    test_cancel_full_semantics();
    test_remove_semantics();
    test_move_semantics_with_rest();
    test_move_zero_rest_keeps_old();
    test_missing_order_no_stale_volume();
    test_multiple_partial_fills_delta();
    test_get_order_info_unknown();
    test_fill_mode_preserved();
    std::cout << "\nAll fill semantics tests passed." << std::endl;
    return 0;
}
