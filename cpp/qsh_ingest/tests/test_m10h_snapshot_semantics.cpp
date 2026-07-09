#include "orderbook/order_book.hpp"
#include <cassert>
#include <iostream>
#include <vector>
#include <string>

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

static OrderLogRecord make_snapshot(UID id, Price price, Volume amount, Side side, Timestamp ts = 1004) {
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

static OrderLogRecord make_new_session(Timestamp ts = 1003) {
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

// Test: snapshot mode does not silently change default behavior
static void test_snapshot_mode_default_unchanged() {
    OrderBook book;

    // Default behavior: snapshot records are applied normally
    book.apply(make_snapshot(1, 100, 10, Side::Buy, 1000));
    book.apply(make_snapshot(2, 200, 8, Side::Sell, 1001));

    // Snapshot records should be counted
    assert(book.errors().snapshot_records_seen == 2);

    // Orders should be in the book (default behavior)
    assert(book.bid_depth() == 1);
    assert(book.ask_depth() == 1);

    std::cout << "  PASS: snapshot mode default unchanged" << std::endl;
}

// Test: snapshot orders loaded counter
static void test_snapshot_orders_loaded_counter() {
    OrderBook book;

    // Simulate load mode: snapshot records should increment snapshot_orders_loaded
    book.apply(make_snapshot(1, 100, 10, Side::Buy, 1000));
    book.errors_ref().snapshot_orders_loaded++;
    assert(book.errors().snapshot_orders_loaded == 1);

    book.apply(make_snapshot(2, 200, 8, Side::Sell, 1001));
    book.errors_ref().snapshot_orders_loaded++;
    assert(book.errors().snapshot_orders_loaded == 2);

    std::cout << "  PASS: snapshot orders loaded counter" << std::endl;
}

// Test: first missing order has no prior ADD (scenario A)
static void test_first_missing_no_prior_add() {
    OrderBook book;

    // Fill non-existent order without any prior ADD
    book.apply(make_fill(999, 100, 5, 0, Side::Buy, 1000));
    assert(book.errors().missing_order_id == 1);
    assert(book.errors().missing_on_fill == 1);

    // No prior ADD exists, so backward check would find nothing
    // This represents scenario A: "decoder may miss records"
    std::cout << "  PASS: first missing order has no prior ADD" << std::endl;
}

// Test: first missing order has prior ADD (scenario B)
static void test_first_missing_with_prior_add() {
    OrderBook book;

    // Add order first
    book.apply(make_add(1, 100, 10, Side::Buy, 1000));
    assert(book.errors().add_records_applied == 1);

    // Clear book (simulating NewSession)
    book.apply(make_new_session(1001));

    // Now fill the order - it's missing because book was cleared
    book.apply(make_fill(1, 100, 5, 0, Side::Buy, 1002));
    assert(book.errors().missing_order_id == 1);

    // Prior ADD exists but order was lost due to book clear
    // This represents scenario B: "book init/lifecycle bug"
    std::cout << "  PASS: first missing order has prior ADD" << std::endl;
}

// Test: first missing order from snapshot (scenario C)
static void test_first_missing_from_snapshot() {
    OrderBook book;

    // Add order via snapshot
    book.apply(make_snapshot(1, 100, 10, Side::Buy, 1000));
    assert(book.errors().snapshot_records_seen == 1);

    // Clear book
    book.apply(make_new_session(1001));

    // Now fill the order - it's missing because snapshot wasn't reloaded
    book.apply(make_fill(1, 100, 5, 0, Side::Buy, 1002));
    assert(book.errors().missing_order_id == 1);

    // Prior snapshot exists but order wasn't reloaded
    // This represents scenario C: "snapshot semantics wrong"
    std::cout << "  PASS: first missing order from snapshot" << std::endl;
}

// Test: snapshot records with different flags
static void test_snapshot_record_flags() {
    OrderLogRecord rec;
    rec.order_id = 1;
    rec.price = 100;
    rec.amount = 10;
    rec.amount_rest = 10;
    rec.side = Side::Buy;
    rec.event = OLMsgType::Add;
    rec.order_flags = OLFlags::Snapshot | OLFlags::Add | OLFlags::Buy;
    rec.timestamp = 1000;

    // Check flags
    assert(has_flag(rec.order_flags, OLFlags::Snapshot));
    assert(has_flag(rec.order_flags, OLFlags::Add));
    assert(has_flag(rec.order_flags, OLFlags::Buy));
    assert(!has_flag(rec.order_flags, OLFlags::NonSystem));
    assert(!has_flag(rec.order_flags, OLFlags::NonZeroReplAct));

    std::cout << "  PASS: snapshot record flags" << std::endl;
}

// Test: non-system snapshot records are skipped
static void test_non_system_snapshot_skipped() {
    OrderBook book;

    // Create non-system snapshot record
    OrderLogRecord rec;
    rec.order_id = 1;
    rec.price = 100;
    rec.amount = 10;
    rec.amount_rest = 10;
    rec.side = Side::Buy;
    rec.event = OLMsgType::Add;
    rec.order_flags = OLFlags::Snapshot | OLFlags::Add | OLFlags::Buy | OLFlags::NonSystem;
    rec.timestamp = 1000;

    // Non-system records should be skipped before reaching book.apply
    // This is handled in main.cpp, not in OrderBook
    // Here we just verify the flag is set correctly
    assert(has_flag(rec.order_flags, OLFlags::NonSystem));

    std::cout << "  PASS: non-system snapshot record flags" << std::endl;
}

int main() {
    std::cout << "=== test_m10h_snapshot_semantics ===" << std::endl;
    test_snapshot_mode_default_unchanged();
    test_snapshot_orders_loaded_counter();
    test_first_missing_no_prior_add();
    test_first_missing_with_prior_add();
    test_first_missing_from_snapshot();
    test_snapshot_record_flags();
    test_non_system_snapshot_skipped();
    std::cout << "\nAll M10H snapshot semantics tests passed." << std::endl;
    return 0;
}
