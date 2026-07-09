#include "orderbook/order_book.hpp"
#include <cassert>
#include <iostream>

using namespace qsh;

static OrderLogRecord make_add(UID id, Price price, Volume amount, Side side, bool quote = false) {
    OrderLogRecord rec;
    rec.order_id = id;
    rec.price = price;
    rec.amount = amount;
    rec.amount_rest = amount;
    rec.side = side;
    rec.event = OLMsgType::Add;
    rec.order_flags = OLFlags::Add | (side == Side::Buy ? OLFlags::Buy : OLFlags::Sell);
    if (quote) rec.order_flags |= OLFlags::Quote;
    rec.timestamp = 1000;
    return rec;
}

static OrderLogRecord make_fill(UID id, Price price, Volume amount, Volume rest, Side side, bool quote = false) {
    OrderLogRecord rec;
    rec.order_id = id;
    rec.price = price;
    rec.amount = amount;
    rec.amount_rest = rest;
    rec.side = side;
    rec.event = OLMsgType::Fill;
    rec.order_flags = OLFlags::Fill | (side == Side::Buy ? OLFlags::Buy : OLFlags::Sell);
    if (quote) rec.order_flags |= OLFlags::Quote;
    rec.timestamp = 1001;
    return rec;
}

static OrderLogRecord make_cancel(UID id, Price price, Volume rest, Side side, bool quote = false) {
    OrderLogRecord rec;
    rec.order_id = id;
    rec.price = price;
    rec.amount_rest = rest;
    rec.side = side;
    rec.event = rest == 0 ? OLMsgType::Remove : OLMsgType::Cancel;
    rec.order_flags = OLFlags::Canceled | (side == Side::Buy ? OLFlags::Buy : OLFlags::Sell);
    if (quote) rec.order_flags |= OLFlags::Quote;
    rec.timestamp = 1002;
    return rec;
}

// Test: Quote mode include allows add to mutate book
static void test_quote_mode_include_allows_add_to_mutate_book() {
    OrderBook book;
    book.set_quote_mode(QuoteMode::Include);

    book.apply(make_add(1, 100, 10, Side::Buy));
    assert(book.best_bid() == 100);

    // Quote add — should mutate book in include mode
    book.apply(make_add(2, 105, 5, Side::Buy, true));
    assert(book.best_bid() == 105);
    std::cout << "  PASS: quote_mode_include_allows_add_to_mutate_book" << std::endl;
}

// Test: Quote mode ignore-book skips add mutation
static void test_quote_mode_ignore_book_skips_add_mutation() {
    OrderBook book;
    book.set_quote_mode(QuoteMode::IgnoreBook);

    book.apply(make_add(1, 100, 10, Side::Buy));
    assert(book.best_bid() == 100);

    // Quote add — should NOT mutate book in ignore-book mode
    book.apply(make_add(2, 105, 5, Side::Buy, true));
    assert(book.best_bid() == 100);  // unchanged
    std::cout << "  PASS: quote_mode_ignore_book_skips_add_mutation" << std::endl;
}

// Test: Quote mode ignore-book counts skipped add
static void test_quote_mode_ignore_book_counts_skipped_add() {
    OrderBook book;
    book.set_quote_mode(QuoteMode::IgnoreBook);

    book.apply(make_add(1, 100, 10, Side::Buy));
    book.apply(make_add(2, 105, 5, Side::Buy, true));

    assert(book.errors().quote_records_seen == 1);
    assert(book.errors().quote_add == 1);
    assert(book.errors().quote_records_ignored_for_book == 1);
    assert(book.errors().quote_add_ignored_for_book == 1);
    std::cout << "  PASS: quote_mode_ignore_book_counts_skipped_add" << std::endl;
}

// Test: Quote mode include can create crossed book when quote add crosses
static void test_quote_mode_include_can_create_crossed_book_when_quote_add_crosses() {
    OrderBook book;
    book.set_quote_mode(QuoteMode::Include);

    // Normal ask at 200
    book.apply(make_add(1, 200, 8, Side::Sell));
    // Normal bid at 100
    book.apply(make_add(2, 100, 10, Side::Buy));
    assert(!book.check_crossed());

    // Quote bid at 250 (above ask) — creates crossed book
    book.apply(make_add(3, 250, 5, Side::Buy, true));
    assert(book.best_bid() == 250);
    assert(book.best_ask() == 200);
    assert(book.check_crossed());
    std::cout << "  PASS: quote_mode_include_can_create_crossed_book_when_quote_add_crosses" << std::endl;
}

// Test: Quote mode ignore-book prevents that specific crossing
static void test_quote_mode_ignore_book_prevents_that_specific_crossing() {
    OrderBook book;
    book.set_quote_mode(QuoteMode::IgnoreBook);

    // Normal ask at 200
    book.apply(make_add(1, 200, 8, Side::Sell));
    // Normal bid at 100
    book.apply(make_add(2, 100, 10, Side::Buy));
    assert(!book.check_crossed());

    // Quote bid at 250 — should NOT mutate book
    book.apply(make_add(3, 250, 5, Side::Buy, true));
    assert(book.best_bid() == 100);  // unchanged
    assert(!book.check_crossed());
    std::cout << "  PASS: quote_mode_ignore_book_prevents_that_specific_crossing" << std::endl;
}

// Test: counter, non-system, and quote ignore are independent
static void test_counter_non_system_quote_ignore_are_independent() {
    // Case A: all three ignore-book
    {
        OrderBook book;
        book.set_counter_mode(CounterMode::IgnoreBook);
        book.set_non_system_mode(NonSystemMode::IgnoreBook);
        book.set_quote_mode(QuoteMode::IgnoreBook);

        // Counter Add — skipped
        OrderLogRecord counter_add = make_add(1, 100, 10, Side::Buy);
        counter_add.order_flags |= OLFlags::Counter;
        book.apply(counter_add);
        assert(book.best_bid() == 0);

        // NonSystem Add — skipped
        OrderLogRecord ns_add = make_add(2, 105, 5, Side::Buy);
        ns_add.order_flags |= OLFlags::NonSystem;
        book.apply(ns_add);
        assert(book.best_bid() == 0);

        // Quote Add — skipped
        book.apply(make_add(3, 110, 8, Side::Buy, true));
        assert(book.best_bid() == 0);

        // Normal Add — should mutate
        book.apply(make_add(4, 115, 6, Side::Buy));
        assert(book.best_bid() == 115);
    }

    // Case B: only quote ignore-book, counter and non-system include
    {
        OrderBook book;
        book.set_counter_mode(CounterMode::Include);
        book.set_non_system_mode(NonSystemMode::Include);
        book.set_quote_mode(QuoteMode::IgnoreBook);

        // Counter Add — should mutate
        OrderLogRecord counter_add = make_add(1, 100, 10, Side::Buy);
        counter_add.order_flags |= OLFlags::Counter;
        book.apply(counter_add);
        assert(book.best_bid() == 100);

        // Quote Add — should be skipped
        book.apply(make_add(2, 105, 5, Side::Buy, true));
        assert(book.best_bid() == 100);  // unchanged
        assert(book.errors().quote_records_ignored_for_book == 1);
    }

    std::cout << "  PASS: counter_non_system_quote_ignore_are_independent" << std::endl;
}

// Test: flag_names decode 0x94 as Add + Buy + Quote
static void test_flag_names_decode_0x94_as_add_buy_quote() {
    uint16_t flags = 0x94;
    std::string names = ol_flag_names(flags);
    // 0x94 = 0x80 (Quote) + 0x10 (Buy) + 0x04 (Add)
    assert(names.find("Add") != std::string::npos);
    assert(names.find("Buy") != std::string::npos);
    assert(names.find("Quote") != std::string::npos);
    assert(names.find("TxEnd") == std::string::npos);  // must NOT contain TxEnd
    std::cout << "  PASS: flag_names_decode_0x94_as_add_buy_quote (decoded: " << names << ")" << std::endl;
}

// Test: flag_names decode 0x414 as Add + Buy + TxEnd
static void test_flag_names_decode_0x414_as_add_buy_txend() {
    uint16_t flags = 0x414;
    std::string names = ol_flag_names(flags);
    // 0x414 = 0x400 (TxEnd) + 0x10 (Buy) + 0x04 (Add)
    assert(names.find("Add") != std::string::npos);
    assert(names.find("Buy") != std::string::npos);
    assert(names.find("TxEnd") != std::string::npos);
    assert(names.find("Quote") == std::string::npos);  // must NOT contain Quote
    std::cout << "  PASS: flag_names_decode_0x414_as_add_buy_txend (decoded: " << names << ")" << std::endl;
}

// Test: Quote mode names
static void test_quote_mode_names() {
    assert(std::string(quote_mode_name(QuoteMode::Include)) == "include");
    assert(std::string(quote_mode_name(QuoteMode::IgnoreBook)) == "ignore-book");
    std::cout << "  PASS: quote_mode_names" << std::endl;
}

// Test: Quote fill tracking in ignore-book mode
static void test_quote_fill_ignored_in_ignore_book() {
    OrderBook book;
    book.set_quote_mode(QuoteMode::IgnoreBook);

    book.apply(make_add(1, 100, 10, Side::Buy));
    assert(book.best_bid() == 100);

    // Quote fill — should be skipped
    book.apply(make_fill(1, 100, 3, 7, Side::Buy, true));
    assert(book.best_bid() == 100);  // unchanged
    assert(book.errors().quote_fill_ignored_for_book == 1);
    std::cout << "  PASS: quote_fill_ignored_in_ignore_book" << std::endl;
}

// Test: Quote cancel tracking in ignore-book mode
static void test_quote_cancel_ignored_in_ignore_book() {
    OrderBook book;
    book.set_quote_mode(QuoteMode::IgnoreBook);

    book.apply(make_add(1, 100, 10, Side::Buy));
    assert(book.best_bid() == 100);

    // Quote cancel — should be skipped
    book.apply(make_cancel(1, 100, 0, Side::Buy, true));
    assert(book.best_bid() == 100);  // unchanged
    assert(book.errors().quote_cancel_ignored_for_book == 1);
    std::cout << "  PASS: quote_cancel_ignored_in_ignore_book" << std::endl;
}

int main() {
    std::cout << "=== test_quote_modes ===" << std::endl;
    test_quote_mode_include_allows_add_to_mutate_book();
    test_quote_mode_ignore_book_skips_add_mutation();
    test_quote_mode_ignore_book_counts_skipped_add();
    test_quote_mode_include_can_create_crossed_book_when_quote_add_crosses();
    test_quote_mode_ignore_book_prevents_that_specific_crossing();
    test_counter_non_system_quote_ignore_are_independent();
    test_flag_names_decode_0x94_as_add_buy_quote();
    test_flag_names_decode_0x414_as_add_buy_txend();
    test_quote_mode_names();
    test_quote_fill_ignored_in_ignore_book();
    test_quote_cancel_ignored_in_ignore_book();
    std::cout << "\nAll quote mode tests passed." << std::endl;
    return 0;
}
