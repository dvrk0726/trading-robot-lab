#include "orderbook/order_book.hpp"
#include "orderbook/l2_snapshot.hpp"
#include <cassert>
#include <iostream>
#include <fstream>
#include <cstdio>
#include <string>
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

// Test: normal book has no crossed state
static void test_normal_book_not_crossed() {
    OrderBook book;
    book.apply(make_add(1, 100, 10, Side::Buy));
    book.apply(make_add(2, 200, 8, Side::Sell));

    assert(book.best_bid() == 100);
    assert(book.best_ask() == 200);
    assert(book.best_bid() < book.best_ask());
    assert(book.spread() == 100);
    assert(!book.check_crossed());
    std::cout << "  PASS: normal book not crossed" << std::endl;
}

// Test: crossed book detection (best_bid >= best_ask)
static void test_crossed_book_detection() {
    // Simulate a crossed book by directly constructing the condition.
    // We can't easily make a crossed book via normal apply() since the engine
    // should prevent it. Instead, test the check_crossed() method directly.
    OrderBook book;
    // Add orders on both sides at the same price (touching = crossed)
    book.apply(make_add(1, 150, 10, Side::Buy));
    book.apply(make_add(2, 150, 8, Side::Sell));

    // best_bid == best_ask => crossed
    assert(book.best_bid() == 150);
    assert(book.best_ask() == 150);
    assert(book.check_crossed());
    assert(book.spread() == 0);
    std::cout << "  PASS: crossed book detection" << std::endl;
}

// Test: non-positive spread detection (spread == 0)
static void test_non_positive_spread() {
    OrderBook book;
    book.apply(make_add(1, 150, 10, Side::Buy));
    book.apply(make_add(2, 150, 8, Side::Sell));

    assert(book.spread() == 0);
    // spread <= 0 is non-positive
    assert(book.spread() <= 0);
    std::cout << "  PASS: non-positive spread detection" << std::endl;
}

// Test: empty bid detection
static void test_empty_bid() {
    OrderBook book;
    book.apply(make_add(1, 200, 8, Side::Sell));

    assert(book.best_bid() == 0);
    assert(book.best_ask() == 200);
    assert(book.bid_depth() == 0);
    std::cout << "  PASS: empty bid detection" << std::endl;
}

// Test: empty ask detection
static void test_empty_ask() {
    OrderBook book;
    book.apply(make_add(1, 100, 10, Side::Buy));

    assert(book.best_bid() == 100);
    assert(book.best_ask() == 0);
    assert(book.ask_depth() == 0);
    std::cout << "  PASS: empty ask detection" << std::endl;
}

// Test: diagnostics CSV writer
static void test_diagnostics_csv_write() {
    std::vector<L2DiagnosticEntry> entries;

    L2DiagnosticEntry e1;
    e1.timestamp = 12345;
    e1.reason = L2DiagReason::CrossedBook;
    e1.best_bid = 15266;
    e1.best_ask = 14062;
    e1.spread = -1204;
    e1.bid_qty_1 = 50;
    e1.ask_qty_1 = 30;
    e1.snapshots_written = 100;
    e1.records_processed = 5000;
    entries.push_back(e1);

    L2DiagnosticEntry e2;
    e2.timestamp = 12400;
    e2.reason = L2DiagReason::EmptyBid;
    e2.best_bid = 0;
    e2.best_ask = 200;
    e2.spread = 0;
    e2.bid_qty_1 = 0;
    e2.ask_qty_1 = 15;
    e2.snapshots_written = 101;
    e2.records_processed = 5100;
    entries.push_back(e2);

    L2DiagnosticEntry e3;
    e3.timestamp = 12500;
    e3.reason = L2DiagReason::EmptyAsk;
    e3.best_bid = 100;
    e3.best_ask = 0;
    e3.spread = 0;
    e3.bid_qty_1 = 20;
    e3.ask_qty_1 = 0;
    e3.snapshots_written = 102;
    e3.records_processed = 5200;
    entries.push_back(e3);

    L2DiagnosticEntry e4;
    e4.timestamp = 12600;
    e4.reason = L2DiagReason::NonPositiveSpread;
    e4.best_bid = 100;
    e4.best_ask = 100;
    e4.spread = 0;
    e4.bid_qty_1 = 25;
    e4.ask_qty_1 = 25;
    e4.snapshots_written = 103;
    e4.records_processed = 5300;
    entries.push_back(e4);

    const std::string path = "test_diag_output.csv";
    size_t written = write_l2_diagnostics_csv(path, entries);
    (void)written;
    assert(written == 4);

    // Verify file contents
    std::ifstream in(path);
    assert(in.is_open());

    std::string header;
    std::getline(in, header);
    assert(header == "ts,reason,best_bid,best_ask,spread,bid_qty_1,ask_qty_1,snapshots_written,records_processed");

    std::string line;
    int line_count = 0;
    while (std::getline(in, line)) {
        if (!line.empty()) ++line_count;
    }
    assert(line_count == 4);

    in.close();
    std::remove(path.c_str());
    std::cout << "  PASS: diagnostics CSV write" << std::endl;
}

// Test: diagnostics CSV with empty vector
static void test_diagnostics_csv_empty() {
    std::vector<L2DiagnosticEntry> entries;
    const std::string path = "test_diag_empty.csv";
    size_t written = write_l2_diagnostics_csv(path, entries);
    (void)written;
    assert(written == 0);

    // File should exist with just header
    std::ifstream in(path);
    assert(in.is_open());
    std::string header;
    std::getline(in, header);
    assert(header == "ts,reason,best_bid,best_ask,spread,bid_qty_1,ask_qty_1,snapshots_written,records_processed");

    std::string rest;
    assert(!std::getline(in, rest) || rest.empty());

    in.close();
    std::remove(path.c_str());
    std::cout << "  PASS: diagnostics CSV empty" << std::endl;
}

// Test: L2 diag reason names
static void test_diag_reason_names() {
    assert(std::string(l2_diag_reason_name(L2DiagReason::CrossedBook)) == "CROSSED_BOOK");
    assert(std::string(l2_diag_reason_name(L2DiagReason::NonPositiveSpread)) == "NON_POSITIVE_SPREAD");
    assert(std::string(l2_diag_reason_name(L2DiagReason::EmptyBid)) == "EMPTY_BID");
    assert(std::string(l2_diag_reason_name(L2DiagReason::EmptyAsk)) == "EMPTY_ASK");
    std::cout << "  PASS: diag reason names" << std::endl;
}

// Test: wide spread is valid (not flagged as diagnostic)
static void test_wide_spread_valid() {
    OrderBook book;
    book.apply(make_add(1, 100, 10, Side::Buy));
    book.apply(make_add(2, 500, 8, Side::Sell));

    assert(book.best_bid() == 100);
    assert(book.best_ask() == 500);
    assert(book.spread() == 400);
    assert(book.spread() > 0);
    assert(!book.check_crossed());
    std::cout << "  PASS: wide spread valid" << std::endl;
}

// Test: best_bid > best_ask is crossed
static void test_bid_above_ask_is_crossed() {
    OrderBook book;
    // Add bid at higher price than ask
    book.apply(make_add(1, 200, 10, Side::Buy));
    book.apply(make_add(2, 100, 8, Side::Sell));

    assert(book.best_bid() == 200);
    assert(book.best_ask() == 100);
    assert(book.best_bid() > book.best_ask());
    assert(book.check_crossed());
    assert(book.spread() == -100);
    std::cout << "  PASS: bid above ask is crossed" << std::endl;
}

int main() {
    std::cout << "=== test_l2_diagnostics ===" << std::endl;
    test_normal_book_not_crossed();
    test_crossed_book_detection();
    test_non_positive_spread();
    test_empty_bid();
    test_empty_ask();
    test_diagnostics_csv_write();
    test_diagnostics_csv_empty();
    test_diag_reason_names();
    test_wide_spread_valid();
    test_bid_above_ask_is_crossed();
    std::cout << "\nAll L2 diagnostics tests passed." << std::endl;
    return 0;
}
