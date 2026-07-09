// M10N: Tests for orphan cancel/remove handling modes
#include "orderbook/order_book.hpp"
#include "qsh/qsh_types.hpp"
#include <cassert>
#include <iostream>
#include <vector>

using namespace qsh;

static OrderLogRecord make_add(UID id, Side side, Price price, Volume amount) {
    OrderLogRecord rec;
    rec.order_id = id;
    rec.event = OLMsgType::Add;
    rec.side = side;
    rec.price = price;
    rec.amount = amount;
    rec.amount_rest = amount;
    rec.order_flags = OLFlags::Add | (side == Side::Buy ? OLFlags::Buy : OLFlags::Sell);
    return rec;
}

static OrderLogRecord make_cancel(UID id, Side side, Price price, Volume amount_rest) {
    OrderLogRecord rec;
    rec.order_id = id;
    rec.event = OLMsgType::Cancel;
    rec.side = side;
    rec.price = price;
    rec.amount = 0;
    rec.amount_rest = amount_rest;
    rec.order_flags = OLFlags::Canceled | (side == Side::Buy ? OLFlags::Buy : OLFlags::Sell);
    return rec;
}

static OrderLogRecord make_remove(UID id, Side side, Price price) {
    OrderLogRecord rec;
    rec.order_id = id;
    rec.event = OLMsgType::Remove;
    rec.side = side;
    rec.price = price;
    rec.amount = 0;
    rec.amount_rest = 0;
    rec.order_flags = OLFlags::CrossTrade | (side == Side::Buy ? OLFlags::Buy : OLFlags::Sell);
    return rec;
}

// Test 1: Strict mode — cancel of unknown order counts missing_order_id
static void test_strict_cancel_unknown() {
    OrderBook book;
    book.set_orphan_cancel_mode(OrphanCancelMode::Strict);

    // Cancel an order that was never added
    book.apply(make_cancel(999, Side::Buy, 100, 0));

    assert(book.errors().missing_order_id == 1);
    assert(book.errors().missing_on_cancel == 1);
    assert(book.errors().orphan_cancel_ignored == 0);
    std::cout << "PASS: test_strict_cancel_unknown\n";
}

// Test 2: Ignore mode — cancel of unknown order is silently ignored
static void test_ignore_cancel_unknown() {
    OrderBook book;
    book.set_orphan_cancel_mode(OrphanCancelMode::Ignore);

    // Cancel an order that was never added
    book.apply(make_cancel(999, Side::Buy, 100, 0));

    assert(book.errors().missing_order_id == 0);
    assert(book.errors().missing_on_cancel == 0);
    assert(book.errors().orphan_cancel_ignored == 1);
    std::cout << "PASS: test_ignore_cancel_unknown\n";
}

// Test 3: Strict mode — remove of unknown order counts missing_order_id
static void test_strict_remove_unknown() {
    OrderBook book;
    book.set_orphan_cancel_mode(OrphanCancelMode::Strict);

    // Remove an order that was never added
    book.apply(make_remove(999, Side::Sell, 200));

    assert(book.errors().missing_order_id == 1);
    assert(book.errors().missing_on_remove == 1);
    assert(book.errors().orphan_remove_ignored == 0);
    std::cout << "PASS: test_strict_remove_unknown\n";
}

// Test 4: Ignore mode — remove of unknown order is silently ignored
static void test_ignore_remove_unknown() {
    OrderBook book;
    book.set_orphan_cancel_mode(OrphanCancelMode::Ignore);

    // Remove an order that was never added
    book.apply(make_remove(999, Side::Sell, 200));

    assert(book.errors().missing_order_id == 0);
    assert(book.errors().missing_on_remove == 0);
    assert(book.errors().orphan_remove_ignored == 1);
    std::cout << "PASS: test_ignore_remove_unknown\n";
}

// Test 5: Ignore mode — cancel of known order still works normally
static void test_ignore_cancel_known() {
    OrderBook book;
    book.set_orphan_cancel_mode(OrphanCancelMode::Ignore);

    // Add then cancel
    book.apply(make_add(100, Side::Buy, 100, 10));
    assert(book.best_bid() == 100);

    book.apply(make_cancel(100, Side::Buy, 100, 0));
    assert(book.best_bid() == 0);
    assert(book.errors().missing_order_id == 0);
    assert(book.errors().orphan_cancel_ignored == 0);
    std::cout << "PASS: test_ignore_cancel_known\n";
}

// Test 6: Ignore mode — remove of known order still works normally
static void test_ignore_remove_known() {
    OrderBook book;
    book.set_orphan_cancel_mode(OrphanCancelMode::Ignore);

    // Add then remove
    book.apply(make_add(100, Side::Sell, 200, 5));
    assert(book.best_ask() == 200);

    book.apply(make_remove(100, Side::Sell, 200));
    assert(book.best_ask() == 0);
    assert(book.errors().missing_order_id == 0);
    assert(book.errors().orphan_remove_ignored == 0);
    std::cout << "PASS: test_ignore_remove_known\n";
}

// Test 7: Mode name strings
static void test_mode_names() {
    assert(std::string(orphan_cancel_mode_name(OrphanCancelMode::Strict)) == "strict");
    assert(std::string(orphan_cancel_mode_name(OrphanCancelMode::Ignore)) == "ignore");
    std::cout << "PASS: test_mode_names\n";
}

// Test 8: Multiple orphan cancels accumulate counter
static void test_multiple_orphan_cancels() {
    OrderBook book;
    book.set_orphan_cancel_mode(OrphanCancelMode::Ignore);

    book.apply(make_cancel(1, Side::Buy, 100, 0));
    book.apply(make_cancel(2, Side::Sell, 200, 0));
    book.apply(make_cancel(3, Side::Buy, 150, 0));

    assert(book.errors().orphan_cancel_ignored == 3);
    assert(book.errors().missing_order_id == 0);
    std::cout << "PASS: test_multiple_orphan_cancels\n";
}

// Test 9: Ignore mode does not mutate book on orphan cancel
static void test_ignore_cancel_no_mutation() {
    OrderBook book;
    book.set_orphan_cancel_mode(OrphanCancelMode::Ignore);

    // Add two orders
    book.apply(make_add(100, Side::Buy, 100, 10));
    book.apply(make_add(200, Side::Buy, 99, 5));
    assert(book.best_bid() == 100);
    assert(book.bid_depth() == 2);

    // Cancel unknown order
    book.apply(make_cancel(999, Side::Buy, 100, 0));

    // Book should be unchanged
    assert(book.best_bid() == 100);
    assert(book.bid_depth() == 2);
    assert(book.errors().orphan_cancel_ignored == 1);
    std::cout << "PASS: test_ignore_cancel_no_mutation\n";
}

// Test 10: Orphan cancel mode preserved across operations
static void test_mode_preserved() {
    OrderBook book;
    book.set_orphan_cancel_mode(OrphanCancelMode::Ignore);

    book.apply(make_add(100, Side::Buy, 100, 10));
    book.apply(make_cancel(999, Side::Buy, 100, 0));
    book.apply(make_remove(998, Side::Sell, 200));

    assert(book.errors().orphan_cancel_ignored == 1);
    assert(book.errors().orphan_remove_ignored == 1);
    assert(book.errors().missing_order_id == 0);
    std::cout << "PASS: test_mode_preserved\n";
}

int main() {
    test_strict_cancel_unknown();
    test_ignore_cancel_unknown();
    test_strict_remove_unknown();
    test_ignore_remove_unknown();
    test_ignore_cancel_known();
    test_ignore_remove_known();
    test_mode_names();
    test_multiple_orphan_cancels();
    test_ignore_cancel_no_mutation();
    test_mode_preserved();

    std::cout << "\nAll test_orphan_cancel_modes tests passed!\n";
    return 0;
}
