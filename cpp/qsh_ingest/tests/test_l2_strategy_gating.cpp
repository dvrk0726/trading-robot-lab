#include "orderbook/order_book.hpp"
#include "orderbook/l2_snapshot.hpp"
#include <cassert>
#include <iostream>
#include <fstream>
#include <cstdio>
#include <string>
#include <vector>
#include <sstream>

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

// Test: classify_l2_snapshot — normal book -> ok
static void test_classify_normal_book() {
    [[maybe_unused]] auto reason = classify_l2_snapshot(100, 200, 3, 3, 5);
    assert(reason == StrategyRejectReason::Ok);
    assert(std::string(strategy_reject_reason_name(reason)) == "ok");
    std::cout << "  PASS: classify normal book -> ok" << std::endl;
}

// Test: classify_l2_snapshot — crossed book (bid > ask)
static void test_classify_crossed_book() {
    [[maybe_unused]] auto reason = classify_l2_snapshot(200, 100, 3, 3, 5);
    assert(reason == StrategyRejectReason::CrossedBook);
    assert(std::string(strategy_reject_reason_name(reason)) == "crossed_book");
    std::cout << "  PASS: classify crossed book -> crossed_book" << std::endl;
}

// Test: classify_l2_snapshot — locked book (bid == ask)
static void test_classify_locked_book() {
    [[maybe_unused]] auto reason = classify_l2_snapshot(150, 150, 3, 3, 5);
    assert(reason == StrategyRejectReason::LockedBook);
    assert(std::string(strategy_reject_reason_name(reason)) == "locked_book");
    std::cout << "  PASS: classify locked book -> locked_book" << std::endl;
}

// Test: classify_l2_snapshot — missing bid
static void test_classify_missing_bid() {
    [[maybe_unused]] auto reason = classify_l2_snapshot(0, 200, 0, 3, 5);
    assert(reason == StrategyRejectReason::MissingBestBid);
    assert(std::string(strategy_reject_reason_name(reason)) == "missing_best_bid");
    std::cout << "  PASS: classify missing bid -> missing_best_bid" << std::endl;
}

// Test: classify_l2_snapshot — missing ask
static void test_classify_missing_ask() {
    [[maybe_unused]] auto reason = classify_l2_snapshot(100, 0, 3, 0, 5);
    assert(reason == StrategyRejectReason::MissingBestAsk);
    assert(std::string(strategy_reject_reason_name(reason)) == "missing_best_ask");
    std::cout << "  PASS: classify missing ask -> missing_best_ask" << std::endl;
}

// Test: classify_l2_snapshot — empty book
static void test_classify_empty_book() {
    [[maybe_unused]] auto reason = classify_l2_snapshot(0, 0, 0, 0, 5);
    assert(reason == StrategyRejectReason::EmptyBook);
    assert(std::string(strategy_reject_reason_name(reason)) == "empty_book");
    std::cout << "  PASS: classify empty book -> empty_book" << std::endl;
}

// Test: classify_l2_snapshot — invalid depth
static void test_classify_invalid_depth() {
    [[maybe_unused]] auto reason = classify_l2_snapshot(100, 200, 3, 3, 0);
    assert(reason == StrategyRejectReason::InvalidDepth);
    assert(std::string(strategy_reject_reason_name(reason)) == "invalid_depth");
    std::cout << "  PASS: classify invalid depth -> invalid_depth" << std::endl;
}

// Test: classify_l2_snapshot — invalid price (bid side has levels but price <= 0)
static void test_classify_invalid_price_bid() {
    [[maybe_unused]] auto reason = classify_l2_snapshot(0, 200, 3, 3, 5);
    assert(reason == StrategyRejectReason::InvalidPrice);
    assert(std::string(strategy_reject_reason_name(reason)) == "invalid_price");
    std::cout << "  PASS: classify invalid price (bid) -> invalid_price" << std::endl;
}

// Test: classify_l2_snapshot — invalid price (ask side has levels but price <= 0)
static void test_classify_invalid_price_ask() {
    [[maybe_unused]] auto reason = classify_l2_snapshot(100, 0, 3, 3, 5);
    assert(reason == StrategyRejectReason::InvalidPrice);
    assert(std::string(strategy_reject_reason_name(reason)) == "invalid_price");
    std::cout << "  PASS: classify invalid price (ask) -> invalid_price" << std::endl;
}

// Test: L2SnapshotEntry fields populated correctly via OrderBook
static void test_snapshot_entry_fields() {
    OrderBook book;
    book.apply(make_add(1, 100, 10, Side::Buy));
    book.apply(make_add(2, 200, 8, Side::Sell));

    auto rows = book.snapshot(5);
    L2SnapshotEntry entry;
    entry.timestamp = 1000;
    entry.levels = rows;
    entry.mid = book.mid_price();
    entry.spread = book.spread();
    entry.best_bid = book.best_bid();
    entry.best_ask = book.best_ask();

    auto reason = classify_l2_snapshot(entry.best_bid, entry.best_ask,
                                        book.bid_depth(), book.ask_depth(), 5);
    entry.is_crossed = (entry.best_bid > 0 && entry.best_ask > 0 && entry.best_bid > entry.best_ask);
    entry.is_locked = (entry.best_bid > 0 && entry.best_ask > 0 && entry.best_bid == entry.best_ask);
    entry.strategy_ready = (reason == StrategyRejectReason::Ok);
    entry.strategy_reject_reason = reason;

    assert(entry.best_bid == 100);
    assert(entry.best_ask == 200);
    assert(!entry.is_crossed);
    assert(!entry.is_locked);
    assert(entry.strategy_ready);
    assert(entry.strategy_reject_reason == StrategyRejectReason::Ok);
    std::cout << "  PASS: snapshot entry fields (normal)" << std::endl;
}

// Test: L2SnapshotEntry fields for crossed book
static void test_snapshot_entry_crossed() {
    OrderBook book;
    book.apply(make_add(1, 200, 10, Side::Buy));
    book.apply(make_add(2, 100, 8, Side::Sell));

    auto rows = book.snapshot(5);
    L2SnapshotEntry entry;
    entry.timestamp = 1000;
    entry.levels = rows;
    entry.mid = book.mid_price();
    entry.spread = book.spread();
    entry.best_bid = book.best_bid();
    entry.best_ask = book.best_ask();

    auto reason = classify_l2_snapshot(entry.best_bid, entry.best_ask,
                                        book.bid_depth(), book.ask_depth(), 5);
    entry.is_crossed = (entry.best_bid > 0 && entry.best_ask > 0 && entry.best_bid > entry.best_ask);
    entry.is_locked = (entry.best_bid > 0 && entry.best_ask > 0 && entry.best_bid == entry.best_ask);
    entry.strategy_ready = (reason == StrategyRejectReason::Ok);
    entry.strategy_reject_reason = reason;

    assert(entry.best_bid == 200);
    assert(entry.best_ask == 100);
    assert(entry.is_crossed);
    assert(!entry.is_locked);
    assert(!entry.strategy_ready);
    assert(entry.strategy_reject_reason == StrategyRejectReason::CrossedBook);
    std::cout << "  PASS: snapshot entry fields (crossed)" << std::endl;
}

// Test: L2SnapshotEntry fields for locked book
static void test_snapshot_entry_locked() {
    OrderBook book;
    book.apply(make_add(1, 150, 10, Side::Buy));
    book.apply(make_add(2, 150, 8, Side::Sell));

    auto rows = book.snapshot(5);
    L2SnapshotEntry entry;
    entry.timestamp = 1000;
    entry.levels = rows;
    entry.mid = book.mid_price();
    entry.spread = book.spread();
    entry.best_bid = book.best_bid();
    entry.best_ask = book.best_ask();

    auto reason = classify_l2_snapshot(entry.best_bid, entry.best_ask,
                                        book.bid_depth(), book.ask_depth(), 5);
    entry.is_crossed = (entry.best_bid > 0 && entry.best_ask > 0 && entry.best_bid > entry.best_ask);
    entry.is_locked = (entry.best_bid > 0 && entry.best_ask > 0 && entry.best_bid == entry.best_ask);
    entry.strategy_ready = (reason == StrategyRejectReason::Ok);
    entry.strategy_reject_reason = reason;

    assert(entry.best_bid == 150);
    assert(entry.best_ask == 150);
    assert(!entry.is_crossed);
    assert(entry.is_locked);
    assert(!entry.strategy_ready);
    assert(entry.strategy_reject_reason == StrategyRejectReason::LockedBook);
    std::cout << "  PASS: snapshot entry fields (locked)" << std::endl;
}

// Test: CSV output includes strategy readiness columns
static void test_csv_includes_strategy_columns() {
    std::vector<L2SnapshotEntry> entries;

    L2SnapshotEntry e;
    e.timestamp = 1000;
    e.levels.push_back({100, 10, 200, 8});
    e.mid = 150.0;
    e.spread = 100;
    e.best_bid = 100;
    e.best_ask = 200;
    e.is_crossed = false;
    e.is_locked = false;
    e.strategy_ready = true;
    e.strategy_reject_reason = StrategyRejectReason::Ok;
    e.snapshot_index = 1;
    e.record_index = 10;
    e.tx_index = 1;
    entries.push_back(e);

    L2SnapshotEntry e2;
    e2.timestamp = 2000;
    e2.levels.push_back({200, 10, 100, 8});
    e2.mid = 150.0;
    e2.spread = -100;
    e2.best_bid = 200;
    e2.best_ask = 100;
    e2.is_crossed = true;
    e2.is_locked = false;
    e2.strategy_ready = false;
    e2.strategy_reject_reason = StrategyRejectReason::CrossedBook;
    e2.snapshot_index = 2;
    e2.record_index = 20;
    e2.tx_index = 2;
    entries.push_back(e2);

    const std::string path = "test_strategy_csv.csv";
    size_t written = write_l2_csv(path, entries, 1);
    (void)written;
    assert(written == 2);

    // Read back and verify header contains strategy columns
    std::ifstream in(path);
    assert(in.is_open());
    std::string header;
    std::getline(in, header);

    assert(header.find("is_crossed") != std::string::npos);
    assert(header.find("is_locked") != std::string::npos);
    assert(header.find("strategy_ready") != std::string::npos);
    assert(header.find("strategy_reject_reason") != std::string::npos);
    assert(header.find("best_bid") != std::string::npos);
    assert(header.find("best_ask") != std::string::npos);
    assert(header.find("snapshot_index") != std::string::npos);
    assert(header.find("record_index") != std::string::npos);
    assert(header.find("tx_index") != std::string::npos);

    // Verify first data row
    std::string line1;
    std::getline(in, line1);
    assert(line1.find("false") != std::string::npos);  // is_crossed=false
    assert(line1.find("true") != std::string::npos);   // strategy_ready=true
    assert(line1.find(",ok") != std::string::npos);    // reason=ok

    // Verify second data row
    std::string line2;
    std::getline(in, line2);
    assert(line2.find("crossed_book") != std::string::npos);

    in.close();
    std::remove(path.c_str());
    std::cout << "  PASS: CSV includes strategy columns" << std::endl;
}

// Test: strategy_reject_reason_name all values
static void test_reject_reason_names() {
    assert(std::string(strategy_reject_reason_name(StrategyRejectReason::Ok)) == "ok");
    assert(std::string(strategy_reject_reason_name(StrategyRejectReason::CrossedBook)) == "crossed_book");
    assert(std::string(strategy_reject_reason_name(StrategyRejectReason::LockedBook)) == "locked_book");
    assert(std::string(strategy_reject_reason_name(StrategyRejectReason::MissingBestBid)) == "missing_best_bid");
    assert(std::string(strategy_reject_reason_name(StrategyRejectReason::MissingBestAsk)) == "missing_best_ask");
    assert(std::string(strategy_reject_reason_name(StrategyRejectReason::EmptyBook)) == "empty_book");
    assert(std::string(strategy_reject_reason_name(StrategyRejectReason::InvalidPrice)) == "invalid_price");
    assert(std::string(strategy_reject_reason_name(StrategyRejectReason::InvalidDepth)) == "invalid_depth");
    std::cout << "  PASS: reject reason names" << std::endl;
}

int main() {
    std::cout << "=== test_l2_strategy_gating ===" << std::endl;
    test_classify_normal_book();
    test_classify_crossed_book();
    test_classify_locked_book();
    test_classify_missing_bid();
    test_classify_missing_ask();
    test_classify_empty_book();
    test_classify_invalid_depth();
    test_classify_invalid_price_bid();
    test_classify_invalid_price_ask();
    test_snapshot_entry_fields();
    test_snapshot_entry_crossed();
    test_snapshot_entry_locked();
    test_csv_includes_strategy_columns();
    test_reject_reason_names();
    std::cout << "\nAll L2 strategy gating tests passed." << std::endl;
    return 0;
}
