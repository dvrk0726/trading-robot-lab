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

// Test: orphan FILL strict mode keeps missing_order_id
static void test_orphan_fill_strict_mode() {
    OrderBook book;
    book.set_orphan_fill_mode(OrphanFillMode::Strict);
    book.set_fill_delta_mode(true);

    book.apply(make_add(1, 100, 10, Side::Buy));
    book.apply(make_add(2, 200, 8, Side::Sell));

    // Try to fill a non-existent order
    book.apply(make_fill(999, 100, 5, 0, Side::Buy));

    // Volume should not change
    assert(book.best_bid_total_qty() == 10);
    assert(book.best_ask_total_qty() == 8);
    assert(book.errors().missing_order_id == 1);
    assert(book.errors().orphan_fill_events == 1);
    assert(book.errors().orphan_fill_ignored == 0);
    assert(book.errors().orphan_fill_level_reductions == 0);

    std::cout << "  PASS: orphan fill strict mode" << std::endl;
}

// Test: orphan FILL ignored mode does not mutate book
static void test_orphan_fill_ignore_mode() {
    OrderBook book;
    book.set_orphan_fill_mode(OrphanFillMode::Ignore);
    book.set_fill_delta_mode(true);

    book.apply(make_add(1, 100, 10, Side::Buy));
    book.apply(make_add(2, 200, 8, Side::Sell));

    // Try to fill a non-existent order
    book.apply(make_fill(999, 100, 5, 0, Side::Buy));

    // Volume should not change
    assert(book.best_bid_total_qty() == 10);
    assert(book.best_ask_total_qty() == 8);
    assert(book.errors().missing_order_id == 0);
    assert(book.errors().orphan_fill_events == 1);
    assert(book.errors().orphan_fill_ignored == 1);
    assert(book.errors().orphan_fill_level_reductions == 0);

    std::cout << "  PASS: orphan fill ignore mode" << std::endl;
}

// Test: orphan FILL reduce-same-price mode reduces volume
static void test_orphan_fill_reduce_same_price_mode() {
    OrderBook book;
    book.set_orphan_fill_mode(OrphanFillMode::ReduceSamePrice);
    book.set_fill_delta_mode(true);

    book.apply(make_add(1, 100, 10, Side::Buy));
    book.apply(make_add(2, 200, 8, Side::Sell));

    // Fill non-existent order at same price level
    book.apply(make_fill(999, 100, 5, 0, Side::Buy));

    // Volume should be reduced by 5
    assert(book.best_bid_total_qty() == 5);
    assert(book.best_ask_total_qty() == 8);
    assert(book.errors().missing_order_id == 0);
    assert(book.errors().orphan_fill_events == 1);
    assert(book.errors().orphan_fill_ignored == 0);
    assert(book.errors().orphan_fill_level_reductions == 1);

    std::cout << "  PASS: orphan fill reduce-same-price mode" << std::endl;
}

// Test: orphan FILL reduce-same-price mode with rest semantics
static void test_orphan_fill_reduce_same_price_rest_mode() {
    OrderBook book;
    book.set_orphan_fill_mode(OrphanFillMode::ReduceSamePrice);
    book.set_fill_delta_mode(false);

    book.apply(make_add(1, 100, 10, Side::Buy));
    book.apply(make_add(2, 200, 8, Side::Sell));

    // Fill non-existent order at same price level
    // In rest mode: amount=10, rest=5, fill = 10-5 = 5
    book.apply(make_fill(999, 100, 10, 5, Side::Buy));

    // Volume should be reduced by 5
    assert(book.best_bid_total_qty() == 5);
    assert(book.best_ask_total_qty() == 8);
    assert(book.errors().orphan_fill_events == 1);
    assert(book.errors().orphan_fill_level_reductions == 1);

    std::cout << "  PASS: orphan fill reduce-same-price rest mode" << std::endl;
}

// Test: orphan FILL reduce-same-price mode removes level when volume goes to zero
static void test_orphan_fill_reduce_same_price_removes_level() {
    OrderBook book;
    book.set_orphan_fill_mode(OrphanFillMode::ReduceSamePrice);
    book.set_fill_delta_mode(true);

    book.apply(make_add(1, 100, 10, Side::Buy));
    book.apply(make_add(2, 200, 8, Side::Sell));

    // Fill non-existent order with full volume
    book.apply(make_fill(999, 100, 10, 0, Side::Buy));

    // Level should be removed
    assert(book.bid_depth() == 0);
    assert(book.best_ask_total_qty() == 8);
    assert(book.errors().orphan_fill_events == 1);
    assert(book.errors().orphan_fill_level_reductions == 1);

    std::cout << "  PASS: orphan fill reduce-same-price removes level" << std::endl;
}

// Test: orphan FILL reduce-same-price mode ignores when level not found
static void test_orphan_fill_reduce_same_price_level_not_found() {
    OrderBook book;
    book.set_orphan_fill_mode(OrphanFillMode::ReduceSamePrice);
    book.set_fill_delta_mode(true);

    book.apply(make_add(1, 100, 10, Side::Buy));
    book.apply(make_add(2, 200, 8, Side::Sell));

    // Fill non-existent order at non-existent price level
    book.apply(make_fill(999, 150, 5, 0, Side::Buy));

    // Volume should not change
    assert(book.best_bid_total_qty() == 10);
    assert(book.best_ask_total_qty() == 8);
    assert(book.errors().orphan_fill_events == 1);
    assert(book.errors().orphan_fill_ignored == 1);
    assert(book.errors().orphan_fill_level_reductions == 0);

    std::cout << "  PASS: orphan fill reduce-same-price level not found" << std::endl;
}

// Test: orphan FILL transaction-rest mode (falls through to ignore for now)
static void test_orphan_fill_transaction_rest_mode() {
    OrderBook book;
    book.set_orphan_fill_mode(OrphanFillMode::TransactionRest);
    book.set_fill_delta_mode(true);

    book.apply(make_add(1, 100, 10, Side::Buy));
    book.apply(make_add(2, 200, 8, Side::Sell));

    // Try to fill a non-existent order
    book.apply(make_fill(999, 100, 5, 0, Side::Buy));

    // Volume should not change (transaction-rest falls through to ignore)
    assert(book.best_bid_total_qty() == 10);
    assert(book.best_ask_total_qty() == 8);
    assert(book.errors().orphan_fill_events == 1);
    assert(book.errors().orphan_fill_ignored == 1);
    assert(book.errors().orphan_fill_level_reductions == 0);

    std::cout << "  PASS: orphan fill transaction-rest mode" << std::endl;
}

// Test: orphan FILL mode is preserved across operations
static void test_orphan_fill_mode_preserved() {
    OrderBook book;
    book.set_orphan_fill_mode(OrphanFillMode::ReduceSamePrice);
    book.set_fill_delta_mode(true);

    book.apply(make_add(1, 100, 10, Side::Buy));

    // First orphan fill
    book.apply(make_fill(999, 100, 3, 0, Side::Buy));
    assert(book.best_bid_total_qty() == 7);

    // Second orphan fill
    book.apply(make_fill(998, 100, 2, 0, Side::Buy));
    assert(book.best_bid_total_qty() == 5);

    assert(book.errors().orphan_fill_events == 2);
    assert(book.errors().orphan_fill_level_reductions == 2);

    std::cout << "  PASS: orphan fill mode preserved" << std::endl;
}

// Test: orphan FILL on sell side
static void test_orphan_fill_sell_side() {
    OrderBook book;
    book.set_orphan_fill_mode(OrphanFillMode::ReduceSamePrice);
    book.set_fill_delta_mode(true);

    book.apply(make_add(1, 100, 10, Side::Buy));
    book.apply(make_add(2, 200, 8, Side::Sell));

    // Fill non-existent order on sell side
    book.apply(make_fill(999, 200, 3, 0, Side::Sell));

    // Volume should be reduced by 3
    assert(book.best_bid_total_qty() == 10);
    assert(book.best_ask_total_qty() == 5);
    assert(book.errors().orphan_fill_events == 1);
    assert(book.errors().orphan_fill_level_reductions == 1);

    std::cout << "  PASS: orphan fill sell side" << std::endl;
}

// Test: orphan FILL with zero fill amount is ignored
static void test_orphan_fill_zero_amount() {
    OrderBook book;
    book.set_orphan_fill_mode(OrphanFillMode::ReduceSamePrice);
    book.set_fill_delta_mode(true);

    book.apply(make_add(1, 100, 10, Side::Buy));

    // Fill with zero amount
    book.apply(make_fill(999, 100, 0, 0, Side::Buy));

    // Volume should not change
    assert(book.best_bid_total_qty() == 10);
    assert(book.errors().orphan_fill_events == 1);
    assert(book.errors().orphan_fill_ignored == 1);

    std::cout << "  PASS: orphan fill zero amount" << std::endl;
}

int main() {
    std::cout << "=== test_orphan_fill_modes ===" << std::endl;
    test_orphan_fill_strict_mode();
    test_orphan_fill_ignore_mode();
    test_orphan_fill_reduce_same_price_mode();
    test_orphan_fill_reduce_same_price_rest_mode();
    test_orphan_fill_reduce_same_price_removes_level();
    test_orphan_fill_reduce_same_price_level_not_found();
    test_orphan_fill_transaction_rest_mode();
    test_orphan_fill_mode_preserved();
    test_orphan_fill_sell_side();
    test_orphan_fill_zero_amount();
    std::cout << "\nAll orphan fill mode tests passed." << std::endl;
    return 0;
}
