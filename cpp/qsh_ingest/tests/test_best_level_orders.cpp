#include "orderbook/order_book.hpp"
#include <cassert>
#include <iostream>
#include <vector>

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

// Test: best-level order ids are visible in normal state
static void test_best_level_order_ids_normal() {
    OrderBook book;
    book.apply(make_add(1, 100, 10, Side::Buy));
    book.apply(make_add(2, 100, 5, Side::Buy));
    book.apply(make_add(3, 102, 3, Side::Buy));
    book.apply(make_add(4, 200, 8, Side::Sell));
    book.apply(make_add(5, 200, 4, Side::Sell));
    book.apply(make_add(6, 202, 2, Side::Sell));

    // Best bid is 102 (order 3)
    auto bid_ids = book.best_bid_order_ids(20);
    assert(bid_ids.size() == 1);
    assert(bid_ids[0] == 3);

    // Best ask is 200 (orders 4, 5)
    auto ask_ids = book.best_ask_order_ids(20);
    assert(ask_ids.size() == 2);
    assert(ask_ids[0] == 4);
    assert(ask_ids[1] == 5);

    std::cout << "  PASS: best-level order ids normal" << std::endl;
}

// Test: best-level order ids are visible in crossed state
static void test_best_level_order_ids_crossed() {
    OrderBook book;
    book.apply(make_add(1, 200, 10, Side::Buy));  // bid above ask
    book.apply(make_add(2, 100, 8, Side::Sell));

    assert(book.check_crossed());

    auto bid_ids = book.best_bid_order_ids(20);
    assert(bid_ids.size() == 1);
    assert(bid_ids[0] == 1);

    auto ask_ids = book.best_ask_order_ids(20);
    assert(ask_ids.size() == 1);
    assert(ask_ids[0] == 2);

    std::cout << "  PASS: best-level order ids crossed" << std::endl;
}

// Test: best-level total qty
static void test_best_level_total_qty() {
    OrderBook book;
    book.apply(make_add(1, 100, 10, Side::Buy));
    book.apply(make_add(2, 100, 5, Side::Buy));
    book.apply(make_add(3, 200, 8, Side::Sell));

    assert(book.best_bid_total_qty() == 15);
    assert(book.best_ask_total_qty() == 8);

    std::cout << "  PASS: best-level total qty" << std::endl;
}

// Test: best-level order count
static void test_best_level_order_count() {
    OrderBook book;
    book.apply(make_add(1, 100, 10, Side::Buy));
    book.apply(make_add(2, 100, 5, Side::Buy));
    book.apply(make_add(3, 200, 8, Side::Sell));

    assert(book.best_bid_order_count() == 2);
    assert(book.best_ask_order_count() == 1);

    std::cout << "  PASS: best-level order count" << std::endl;
}

// Test: fill removes order from level tracking
static void test_fill_removes_from_level_tracking() {
    OrderBook book;
    book.apply(make_add(1, 100, 10, Side::Buy));
    book.apply(make_add(2, 100, 5, Side::Buy));

    assert(book.best_bid_order_count() == 2);

    // Fully fill order 1
    book.apply(make_fill(1, 100, 10, 0, Side::Buy));

    assert(book.best_bid_order_count() == 1);
    auto ids = book.best_bid_order_ids(20);
    assert(ids.size() == 1);
    assert(ids[0] == 2);

    std::cout << "  PASS: fill removes from level tracking" << std::endl;
}

// Test: cancel removes order from level tracking
static void test_cancel_removes_from_level_tracking() {
    OrderBook book;
    book.apply(make_add(1, 100, 10, Side::Buy));
    book.apply(make_add(2, 100, 5, Side::Buy));

    assert(book.best_bid_order_count() == 2);

    // Cancel order 1
    book.apply(make_cancel(1, 100, 0, Side::Buy));

    assert(book.best_bid_order_count() == 1);
    auto ids = book.best_bid_order_ids(20);
    assert(ids.size() == 1);
    assert(ids[0] == 2);

    std::cout << "  PASS: cancel removes from level tracking" << std::endl;
}

// Test: remove removes order from level tracking
static void test_remove_removes_from_level_tracking() {
    OrderBook book;
    book.apply(make_add(1, 100, 10, Side::Buy));
    book.apply(make_add(2, 100, 5, Side::Buy));

    assert(book.best_bid_order_count() == 2);

    // Remove order 1 (amount_rest == 0)
    OrderLogRecord rec;
    rec.order_id = 1;
    rec.price = 100;
    rec.amount = 0;
    rec.amount_rest = 0;
    rec.side = Side::Buy;
    rec.event = OLMsgType::Remove;
    rec.order_flags = OLFlags::CrossTrade | OLFlags::Buy;
    rec.timestamp = 1002;
    book.apply(rec);

    assert(book.best_bid_order_count() == 1);
    auto ids = book.best_bid_order_ids(20);
    assert(ids.size() == 1);
    assert(ids[0] == 2);

    std::cout << "  PASS: remove removes from level tracking" << std::endl;
}

// Test: max_ids limit
static void test_max_ids_limit() {
    OrderBook book;
    book.apply(make_add(1, 100, 10, Side::Buy));
    book.apply(make_add(2, 100, 5, Side::Buy));
    book.apply(make_add(3, 100, 3, Side::Buy));

    auto ids = book.best_bid_order_ids(2);
    assert(ids.size() == 2);

    std::cout << "  PASS: max_ids limit" << std::endl;
}

// Test: empty book
static void test_empty_book_best_level() {
    OrderBook book;

    auto bid_ids = book.best_bid_order_ids(20);
    assert(bid_ids.empty());

    auto ask_ids = book.best_ask_order_ids(20);
    assert(ask_ids.empty());

    assert(book.best_bid_total_qty() == 0);
    assert(book.best_ask_total_qty() == 0);
    assert(book.best_bid_order_count() == 0);
    assert(book.best_ask_order_count() == 0);

    std::cout << "  PASS: empty book best level" << std::endl;
}

int main() {
    std::cout << "=== test_best_level_orders ===" << std::endl;
    test_best_level_order_ids_normal();
    test_best_level_order_ids_crossed();
    test_best_level_total_qty();
    test_best_level_order_count();
    test_fill_removes_from_level_tracking();
    test_cancel_removes_from_level_tracking();
    test_remove_removes_from_level_tracking();
    test_max_ids_limit();
    test_empty_book_best_level();
    std::cout << "\nAll best-level orders tests passed." << std::endl;
    return 0;
}
