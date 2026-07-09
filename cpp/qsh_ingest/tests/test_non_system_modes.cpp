#include "orderbook/order_book.hpp"
#include <cassert>
#include <iostream>

using namespace qsh;

static OrderLogRecord make_add(UID id, Price price, Volume amount, Side side, bool non_system = false) {
    OrderLogRecord rec;
    rec.order_id = id;
    rec.price = price;
    rec.amount = amount;
    rec.amount_rest = amount;
    rec.side = side;
    rec.event = OLMsgType::Add;
    rec.order_flags = OLFlags::Add | (side == Side::Buy ? OLFlags::Buy : OLFlags::Sell);
    if (non_system) rec.order_flags |= OLFlags::NonSystem;
    rec.timestamp = 1000;
    return rec;
}

static OrderLogRecord make_fill(UID id, Price price, Volume amount, Volume rest, Side side, bool non_system = false) {
    OrderLogRecord rec;
    rec.order_id = id;
    rec.price = price;
    rec.amount = amount;
    rec.amount_rest = rest;
    rec.side = side;
    rec.event = OLMsgType::Fill;
    rec.order_flags = OLFlags::Fill | (side == Side::Buy ? OLFlags::Buy : OLFlags::Sell);
    if (non_system) rec.order_flags |= OLFlags::NonSystem;
    rec.timestamp = 1001;
    return rec;
}

static OrderLogRecord make_cancel(UID id, Price price, Volume rest, Side side, bool non_system = false) {
    OrderLogRecord rec;
    rec.order_id = id;
    rec.price = price;
    rec.amount_rest = rest;
    rec.side = side;
    rec.event = rest == 0 ? OLMsgType::Remove : OLMsgType::Cancel;
    rec.order_flags = OLFlags::Canceled | (side == Side::Buy ? OLFlags::Buy : OLFlags::Sell);
    if (non_system) rec.order_flags |= OLFlags::NonSystem;
    rec.timestamp = 1002;
    return rec;
}

// Test: NonSystem mode include allows add to mutate book
static void test_non_system_mode_include_allows_add_to_mutate_book() {
    OrderBook book;
    book.set_non_system_mode(NonSystemMode::Include);

    // Add a normal order
    book.apply(make_add(1, 100, 10, Side::Buy));
    assert(book.best_bid() == 100);

    // Add a NonSystem order — should mutate book in include mode
    book.apply(make_add(2, 105, 5, Side::Buy, true));
    assert(book.best_bid() == 105);
    std::cout << "  PASS: non_system_mode_include_allows_add_to_mutate_book" << std::endl;
}

// Test: NonSystem mode ignore-book skips add mutation
static void test_non_system_mode_ignore_book_skips_add_mutation() {
    OrderBook book;
    book.set_non_system_mode(NonSystemMode::IgnoreBook);

    // Add a normal order
    book.apply(make_add(1, 100, 10, Side::Buy));
    assert(book.best_bid() == 100);

    // Add a NonSystem order — should NOT mutate book in ignore-book mode
    book.apply(make_add(2, 105, 5, Side::Buy, true));
    assert(book.best_bid() == 100);  // unchanged
    std::cout << "  PASS: non_system_mode_ignore_book_skips_add_mutation" << std::endl;
}

// Test: NonSystem mode ignore-book counts skipped add
static void test_non_system_mode_ignore_book_counts_skipped_add() {
    OrderBook book;
    book.set_non_system_mode(NonSystemMode::IgnoreBook);

    book.apply(make_add(1, 100, 10, Side::Buy));
    book.apply(make_add(2, 105, 5, Side::Buy, true));

    assert(book.errors().non_system_records_seen == 1);
    assert(book.errors().non_system_add == 1);
    assert(book.errors().non_system_records_ignored_for_book == 1);
    assert(book.errors().non_system_add_ignored_for_book == 1);
    std::cout << "  PASS: non_system_mode_ignore_book_counts_skipped_add" << std::endl;
}

// Test: NonSystem mode include can create crossed book when add crosses
static void test_non_system_mode_include_can_create_crossed_book_when_add_crosses() {
    OrderBook book;
    book.set_non_system_mode(NonSystemMode::Include);

    // Normal ask at 200
    book.apply(make_add(1, 200, 8, Side::Sell));
    // Normal bid at 100
    book.apply(make_add(2, 100, 10, Side::Buy));
    assert(!book.check_crossed());

    // NonSystem bid at 250 (above ask) — creates crossed book
    book.apply(make_add(3, 250, 5, Side::Buy, true));
    assert(book.best_bid() == 250);
    assert(book.best_ask() == 200);
    assert(book.check_crossed());
    std::cout << "  PASS: non_system_mode_include_can_create_crossed_book_when_add_crosses" << std::endl;
}

// Test: NonSystem mode ignore-book prevents that specific crossing
static void test_non_system_mode_ignore_book_prevents_that_specific_crossing() {
    OrderBook book;
    book.set_non_system_mode(NonSystemMode::IgnoreBook);

    // Normal ask at 200
    book.apply(make_add(1, 200, 8, Side::Sell));
    // Normal bid at 100
    book.apply(make_add(2, 100, 10, Side::Buy));
    assert(!book.check_crossed());

    // NonSystem bid at 250 — should NOT mutate book
    book.apply(make_add(3, 250, 5, Side::Buy, true));
    assert(book.best_bid() == 100);  // unchanged
    assert(!book.check_crossed());
    std::cout << "  PASS: non_system_mode_ignore_book_prevents_that_specific_crossing" << std::endl;
}

// Test: counter ignore and non-system ignore are independent
static void test_counter_ignore_and_non_system_ignore_are_independent() {
    // Case A: counter ignore-book, non-system include
    {
        OrderBook book;
        book.set_counter_mode(CounterMode::IgnoreBook);
        book.set_non_system_mode(NonSystemMode::Include);

        // Counter Add — should be skipped
        OrderLogRecord counter_add = make_add(1, 100, 10, Side::Buy);
        counter_add.order_flags |= OLFlags::Counter;
        book.apply(counter_add);
        assert(book.best_bid() == 0);  // skipped
        assert(book.errors().counter_records_ignored_for_book == 1);

        // NonSystem Add — should mutate (include mode)
        book.apply(make_add(2, 105, 5, Side::Buy, true));
        assert(book.best_bid() == 105);
        assert(book.errors().non_system_records_ignored_for_book == 0);
    }

    // Case B: counter include, non-system ignore-book
    {
        OrderBook book;
        book.set_counter_mode(CounterMode::Include);
        book.set_non_system_mode(NonSystemMode::IgnoreBook);

        // Counter Add — should mutate (include mode)
        OrderLogRecord counter_add = make_add(1, 100, 10, Side::Buy);
        counter_add.order_flags |= OLFlags::Counter;
        book.apply(counter_add);
        assert(book.best_bid() == 100);
        assert(book.errors().counter_records_ignored_for_book == 0);

        // NonSystem Add — should be skipped
        book.apply(make_add(2, 105, 5, Side::Buy, true));
        assert(book.best_bid() == 100);  // unchanged
        assert(book.errors().non_system_records_ignored_for_book == 1);
    }

    // Case C: both ignore-book
    {
        OrderBook book;
        book.set_counter_mode(CounterMode::IgnoreBook);
        book.set_non_system_mode(NonSystemMode::IgnoreBook);

        // Counter Add — skipped
        OrderLogRecord counter_add = make_add(1, 100, 10, Side::Buy);
        counter_add.order_flags |= OLFlags::Counter;
        book.apply(counter_add);
        assert(book.best_bid() == 0);

        // NonSystem Add — skipped
        book.apply(make_add(2, 105, 5, Side::Buy, true));
        assert(book.best_bid() == 0);

        // Normal Add — should mutate
        book.apply(make_add(3, 110, 8, Side::Buy));
        assert(book.best_bid() == 110);
    }

    std::cout << "  PASS: counter_ignore_and_non_system_ignore_are_independent" << std::endl;
}

// Test: NonSystem fill tracking in ignore-book mode
static void test_non_system_fill_ignored_in_ignore_book() {
    OrderBook book;
    book.set_non_system_mode(NonSystemMode::IgnoreBook);

    // Normal add
    book.apply(make_add(1, 100, 10, Side::Buy));
    assert(book.best_bid() == 100);

    // NonSystem fill — should be skipped
    book.apply(make_fill(1, 100, 3, 7, Side::Buy, true));
    assert(book.best_bid() == 100);  // unchanged
    assert(book.errors().non_system_fill_ignored_for_book == 1);
    std::cout << "  PASS: non_system_fill_ignored_in_ignore_book" << std::endl;
}

// Test: NonSystem cancel tracking in ignore-book mode
static void test_non_system_cancel_ignored_in_ignore_book() {
    OrderBook book;
    book.set_non_system_mode(NonSystemMode::IgnoreBook);

    // Normal add
    book.apply(make_add(1, 100, 10, Side::Buy));
    assert(book.best_bid() == 100);

    // NonSystem cancel — should be skipped
    book.apply(make_cancel(1, 100, 0, Side::Buy, true));
    assert(book.best_bid() == 100);  // unchanged
    assert(book.errors().non_system_cancel_ignored_for_book == 1);
    std::cout << "  PASS: non_system_cancel_ignored_in_ignore_book" << std::endl;
}

// Test: NonSystem mode names
static void test_non_system_mode_names() {
    assert(std::string(non_system_mode_name(NonSystemMode::Include)) == "include");
    assert(std::string(non_system_mode_name(NonSystemMode::IgnoreBook)) == "ignore-book");
    std::cout << "  PASS: non_system_mode_names" << std::endl;
}

// Test: NonSystem counter interaction — a record with both Counter and NonSystem
static void test_non_system_and_counter_both_flags() {
    OrderBook book;
    book.set_counter_mode(CounterMode::IgnoreBook);
    book.set_non_system_mode(NonSystemMode::Include);

    // Record with both Counter and NonSystem — Counter takes precedence (checked first)
    OrderLogRecord rec = make_add(1, 100, 10, Side::Buy);
    rec.order_flags |= OLFlags::Counter | OLFlags::NonSystem;
    book.apply(rec);
    assert(book.best_bid() == 0);  // skipped by counter mode
    assert(book.errors().counter_records_ignored_for_book == 1);
    assert(book.errors().non_system_records_seen == 1);  // still counted
    std::cout << "  PASS: non_system_and_counter_both_flags" << std::endl;
}

int main() {
    std::cout << "=== test_non_system_modes ===" << std::endl;
    test_non_system_mode_include_allows_add_to_mutate_book();
    test_non_system_mode_ignore_book_skips_add_mutation();
    test_non_system_mode_ignore_book_counts_skipped_add();
    test_non_system_mode_include_can_create_crossed_book_when_add_crosses();
    test_non_system_mode_ignore_book_prevents_that_specific_crossing();
    test_counter_ignore_and_non_system_ignore_are_independent();
    test_non_system_fill_ignored_in_ignore_book();
    test_non_system_cancel_ignored_in_ignore_book();
    test_non_system_mode_names();
    test_non_system_and_counter_both_flags();
    std::cout << "\nAll non-system mode tests passed." << std::endl;
    return 0;
}
