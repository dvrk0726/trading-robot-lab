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

// Test: NewSession clears the book
static void test_new_session_clears_book() {
    OrderBook book;

    // Add some orders
    book.apply(make_add(1, 100, 10, Side::Buy, 1000));
    book.apply(make_add(2, 200, 8, Side::Sell, 1001));
    assert(book.bid_depth() == 1);
    assert(book.ask_depth() == 1);

    // NewSession should clear the book
    book.apply(make_new_session(1002));
    assert(book.bid_depth() == 0);
    assert(book.ask_depth() == 0);
    assert(book.errors().new_session_records_seen == 1);
    assert(book.errors().book_clears_due_to_new_session == 1);

    std::cout << "  PASS: NewSession clears book" << std::endl;
}

// Test: Snapshot records are tracked
static void test_snapshot_records_tracked() {
    OrderBook book;

    // Add snapshot record
    book.apply(make_snapshot(1, 100, 10, Side::Buy, 1000));
    assert(book.errors().snapshot_records_seen == 1);

    // Add another snapshot record
    book.apply(make_snapshot(2, 200, 8, Side::Sell, 1001));
    assert(book.errors().snapshot_records_seen == 2);

    std::cout << "  PASS: Snapshot records tracked" << std::endl;
}

// Test: ADD applied/skipped counters
static void test_add_counters() {
    OrderBook book;

    // Valid ADD
    book.apply(make_add(1, 100, 10, Side::Buy, 1000));
    assert(book.errors().add_records_seen == 1);
    assert(book.errors().add_records_applied == 1);
    assert(book.errors().add_records_skipped == 0);

    // Another valid ADD
    book.apply(make_add(2, 200, 8, Side::Sell, 1001));
    assert(book.errors().add_records_seen == 2);
    assert(book.errors().add_records_applied == 2);
    assert(book.errors().add_records_skipped == 0);

    std::cout << "  PASS: ADD applied/skipped counters" << std::endl;
}

// Test: missing_order_id before crossed book
static void test_missing_order_before_crossed() {
    OrderBook book;

    // Add bid and ask
    book.apply(make_add(1, 100, 10, Side::Buy, 1000));
    book.apply(make_add(2, 200, 8, Side::Sell, 1001));

    // Fill non-existent order (missing_order_id)
    book.apply(make_fill(999, 150, 5, 0, Side::Buy, 1002));
    assert(book.errors().missing_order_id == 1);
    assert(book.errors().missing_on_fill == 1);
    assert(book.errors().missing_on_buy == 1);

    // Create crossed book by moving bid above ask
    OrderLogRecord move_rec;
    move_rec.order_id = 1;
    move_rec.price = 210;
    move_rec.amount_rest = 10;
    move_rec.amount = 10;
    move_rec.side = Side::Buy;
    move_rec.event = OLMsgType::Moved;
    move_rec.order_flags = OLFlags::Moved | OLFlags::Buy;
    move_rec.timestamp = 1003;
    book.apply(move_rec);

    assert(book.check_crossed());
    assert(book.errors().missing_order_id == 1);

    std::cout << "  PASS: missing_order_id before crossed book" << std::endl;
}

// Test: missing_order_id by event type
static void test_missing_order_by_event_type() {
    OrderBook book;

    // Add an order
    book.apply(make_add(1, 100, 10, Side::Buy, 1000));

    // Fill non-existent order
    book.apply(make_fill(999, 100, 5, 0, Side::Buy, 1001));
    assert(book.errors().missing_on_fill == 1);

    // Cancel non-existent order
    book.apply(make_cancel(998, 100, 0, Side::Buy, 1002));
    assert(book.errors().missing_on_cancel == 1);

    // Remove non-existent order (cancel with rest=0)
    book.apply(make_cancel(997, 100, 0, Side::Sell, 1003));
    assert(book.errors().missing_on_remove == 1);

    // Move non-existent order
    OrderLogRecord move_rec;
    move_rec.order_id = 996;
    move_rec.price = 110;
    move_rec.amount_rest = 10;
    move_rec.amount = 10;
    move_rec.side = Side::Buy;
    move_rec.event = OLMsgType::Moved;
    move_rec.order_flags = OLFlags::Moved | OLFlags::Buy;
    move_rec.timestamp = 1004;
    book.apply(move_rec);
    assert(book.errors().missing_on_move == 1);

    assert(book.errors().missing_order_id == 4);

    std::cout << "  PASS: missing_order_id by event type" << std::endl;
}

// Test: missing_order_id by side
static void test_missing_order_by_side() {
    OrderBook book;

    // Fill non-existent buy order
    book.apply(make_fill(999, 100, 5, 0, Side::Buy, 1000));
    assert(book.errors().missing_on_buy == 1);
    assert(book.errors().missing_on_sell == 0);

    // Fill non-existent sell order
    book.apply(make_fill(998, 200, 5, 0, Side::Sell, 1001));
    assert(book.errors().missing_on_buy == 1);
    assert(book.errors().missing_on_sell == 1);

    std::cout << "  PASS: missing_order_id by side" << std::endl;
}

// Test: selected order lifecycle trace
static void test_selected_order_lifecycle() {
    OrderBook book;

    // Add order
    book.apply(make_add(42, 150, 10, Side::Buy, 1000));
    {
        Side side; Price price; Volume qty;
        assert(book.get_order_info(42, side, price, qty));
        assert(side == Side::Buy);
        assert(price == 150);
        assert(qty == 10);
        (void)side; (void)price; (void)qty;
    }

    // Fill partial
    book.apply(make_fill(42, 150, 3, 7, Side::Buy, 1001));
    {
        Side side; Price price; Volume qty;
        assert(book.get_order_info(42, side, price, qty));
        assert(side == Side::Buy);
        assert(price == 150);
        assert(qty == 7);
        (void)side; (void)price; (void)qty;
    }

    // Fill rest
    book.apply(make_fill(42, 150, 7, 0, Side::Buy, 1002));
    {
        Side side; Price price; Volume qty;
        assert(!book.get_order_info(42, side, price, qty));
        (void)side; (void)price; (void)qty;
    }

    std::cout << "  PASS: selected order lifecycle" << std::endl;
}

// Test: first valid book record index
static void test_first_valid_book_record_index() {
    OrderBook book;

    // Book is empty initially
    assert(book.first_valid_book_record_index() == 0);

    // Add bid only
    book.apply(make_add(1, 100, 10, Side::Buy, 1000));
    assert(book.first_valid_book_record_index() == 0);

    // Add ask - now book has both sides
    book.apply(make_add(2, 200, 8, Side::Sell, 1001));
    // Note: first_valid_book_record_index is set in main.cpp, not in OrderBook::apply
    // This test just verifies the getter/setter work
    book.set_first_valid_book_record_index(2);
    assert(book.first_valid_book_record_index() == 2);

    std::cout << "  PASS: first valid book record index" << std::endl;
}

// Test: first missing order record index
static void test_first_missing_order_record_index() {
    OrderBook book;

    // Initially 0
    assert(book.first_missing_order_record_index() == 0);

    // Set first missing order record index
    book.set_first_missing_order_record_index(100);
    assert(book.first_missing_order_record_index() == 100);

    // Setting again should not change
    book.set_first_missing_order_record_index(200);
    assert(book.first_missing_order_record_index() == 100);

    std::cout << "  PASS: first missing order record index" << std::endl;
}

// Test: first crossed book record index
static void test_first_crossed_book_record_index() {
    OrderBook book;

    // Initially 0
    assert(book.first_crossed_book_record_index() == 0);

    // Set first crossed book record index
    book.set_first_crossed_book_record_index(50);
    assert(book.first_crossed_book_record_index() == 50);

    // Setting again should not change
    book.set_first_crossed_book_record_index(100);
    assert(book.first_crossed_book_record_index() == 50);

    std::cout << "  PASS: first crossed book record index" << std::endl;
}

int main() {
    std::cout << "=== test_m10g_diagnostics ===" << std::endl;
    test_new_session_clears_book();
    test_snapshot_records_tracked();
    test_add_counters();
    test_missing_order_before_crossed();
    test_missing_order_by_event_type();
    test_missing_order_by_side();
    test_selected_order_lifecycle();
    test_first_valid_book_record_index();
    test_first_missing_order_record_index();
    test_first_crossed_book_record_index();
    std::cout << "\nAll M10G diagnostics tests passed." << std::endl;
    return 0;
}
