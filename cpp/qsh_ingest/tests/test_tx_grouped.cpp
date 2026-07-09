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
    rec.event = OLMsgType::Cancel;
    rec.order_flags = OLFlags::Canceled | (side == Side::Buy ? OLFlags::Buy : OLFlags::Sell);
    rec.timestamp = 1002;
    return rec;
}

static OrderLogRecord make_remove(UID id, Price price, Side side) {
    OrderLogRecord rec;
    rec.order_id = id;
    rec.price = price;
    rec.amount_rest = 0;
    rec.side = side;
    rec.event = OLMsgType::Remove;
    rec.order_flags = OLFlags::CrossTrade | (side == Side::Buy ? OLFlags::Buy : OLFlags::Sell);
    rec.timestamp = 1003;
    return rec;
}

// Test 1: Per-record mode unchanged — apply one by one
static void test_per_record_mode_unchanged() {
    OrderBook book;
    book.apply(make_add(1, 100, 10, Side::Buy));
    book.apply(make_add(2, 200, 8, Side::Sell));

    assert(book.best_bid() == 100);
    assert(book.best_ask() == 200);
    assert(book.errors().missing_order_id == 0);

    // Fill order 1
    book.apply(make_fill(1, 100, 5, 5, Side::Buy));
    assert(book.best_bid() == 100);
    auto snap = book.snapshot(1);
    assert(snap[0].bid_qty == 5);

    // Cancel order 1
    book.apply(make_cancel(1, 100, 0, Side::Buy));
    assert(book.bid_depth() == 0);

    std::cout << "  PASS: per_record_mode_unchanged" << std::endl;
}

// Test 2: tx-grouped mode batches until TxEnd
static void test_tx_grouped_batches_until_txend() {
    OrderBook book;

    // Transaction 1: ADD two orders, no TxEnd yet
    std::vector<OrderLogRecord> tx1;
    tx1.push_back(make_add(1, 100, 10, Side::Buy));
    tx1.push_back(make_add(2, 200, 8, Side::Sell));

    // Apply as transaction
    auto result = book.apply_transaction(tx1);
    assert(result.records_processed == 2);
    assert(result.missing_order_id == 0);

    // Book should have both orders
    assert(book.best_bid() == 100);
    assert(book.best_ask() == 200);
    assert(book.bid_depth() == 1);
    assert(book.ask_depth() == 1);

    std::cout << "  PASS: tx_grouped_batches_until_txend" << std::endl;
}

// Test 3: ADD+FILL in same transaction
static void test_add_fill_same_transaction() {
    OrderBook book;

    // Transaction: ADD order 1, then FILL order 1 partially
    std::vector<OrderLogRecord> tx;
    tx.push_back(make_add(1, 100, 10, Side::Buy));
    tx.push_back(make_fill(1, 100, 3, 7, Side::Buy));

    auto result = book.apply_transaction(tx);
    assert(result.records_processed == 2);
    assert(result.orphan_fill_events == 0);  // Not orphan — added in same tx
    assert(result.orphan_fill_resolved_in_transaction == 0);  // Was in book when filled
    assert(result.missing_order_id == 0);

    // Order should have 7 remaining
    auto snap = book.snapshot(1);
    assert(snap[0].bid_qty == 7);

    std::cout << "  PASS: add_fill_same_transaction" << std::endl;
}

// Test 4: ADD+FILL+REMOVE in same transaction (order fully consumed)
static void test_add_fill_remove_same_transaction() {
    OrderBook book;

    // Transaction: ADD order 1, FILL fully, then attempt REMOVE
    // After full fill, order is gone — REMOVE should be "resolved in transaction"
    std::vector<OrderLogRecord> tx;
    tx.push_back(make_add(1, 100, 10, Side::Buy));
    tx.push_back(make_fill(1, 100, 10, 0, Side::Buy));  // Full fill
    tx.push_back(make_remove(1, 100, Side::Buy));  // Remove after fill

    auto result = book.apply_transaction(tx);
    assert(result.records_processed == 3);
    // The REMOVE references order 1 which was added in this tx but already consumed
    assert(result.orphan_remove_events == 0);
    assert(result.orphan_remove_resolved_in_transaction == 1);
    assert(result.missing_order_id == 0);

    // Book should be empty on bid side
    assert(book.bid_depth() == 0);

    std::cout << "  PASS: add_fill_remove_same_transaction" << std::endl;
}

// Test 5: Action on order introduced earlier in same transaction
static void test_action_on_order_from_earlier_in_tx() {
    OrderBook book;

    // Pre-populate book with order 3
    book.apply(make_add(3, 200, 8, Side::Sell));

    // Transaction: ADD order 1, then CANCEL order 3 (which was in book before tx)
    std::vector<OrderLogRecord> tx;
    tx.push_back(make_add(1, 100, 10, Side::Buy));
    tx.push_back(make_cancel(3, 200, 0, Side::Sell));

    auto result = book.apply_transaction(tx);
    assert(result.records_processed == 2);
    // Cancel of order 3: order was in book before tx → not orphan
    assert(result.orphan_cancel_events == 0);
    assert(result.orphan_cancel_resolved_in_transaction == 0);
    assert(result.missing_order_id == 0);

    // Ask side should be empty
    assert(book.ask_depth() == 0);
    assert(book.bid_depth() == 1);

    std::cout << "  PASS: action_on_order_from_earlier_in_tx" << std::endl;
}

// Test 6: Orphan Cancel/Remove tracked clearly
static void test_orphan_cancel_remove_tracked() {
    OrderBook book;

    // Transaction: Cancel and Remove orders that were never added
    std::vector<OrderLogRecord> tx;
    tx.push_back(make_cancel(999, 100, 0, Side::Buy));
    tx.push_back(make_remove(998, 200, Side::Sell));

    auto result = book.apply_transaction(tx);
    assert(result.records_processed == 2);
    assert(result.orphan_cancel_events == 1);
    assert(result.orphan_remove_events == 1);
    assert(result.missing_order_id == 2);  // Both count as missing

    std::cout << "  PASS: orphan_cancel_remove_tracked" << std::endl;
}

// Test 7: Orphan Fill tracked clearly
static void test_orphan_fill_tracked() {
    OrderBook book;

    // Transaction: Fill order that was never added
    std::vector<OrderLogRecord> tx;
    tx.push_back(make_fill(999, 100, 5, 0, Side::Buy));

    auto result = book.apply_transaction(tx);
    assert(result.records_processed == 1);
    assert(result.orphan_fill_events == 1);
    assert(result.orphan_fill_resolved_in_transaction == 0);
    assert(result.missing_order_id == 1);

    std::cout << "  PASS: orphan_fill_tracked" << std::endl;
}

// Test 8: Multiple transactions accumulate counters
static void test_multiple_transactions_accumulate() {
    OrderBook book;

    // Transaction 1: normal add
    std::vector<OrderLogRecord> tx1;
    tx1.push_back(make_add(1, 100, 10, Side::Buy));
    tx1.push_back(make_add(2, 200, 8, Side::Sell));
    auto r1 = book.apply_transaction(tx1);
    assert(r1.missing_order_id == 0);

    // Transaction 2: fill and cancel
    std::vector<OrderLogRecord> tx2;
    tx2.push_back(make_fill(1, 100, 3, 7, Side::Buy));
    tx2.push_back(make_cancel(2, 200, 0, Side::Sell));
    auto r2 = book.apply_transaction(tx2);
    assert(r2.missing_order_id == 0);
    assert(r2.orphan_fill_events == 0);
    assert(r2.orphan_cancel_events == 0);

    // Transaction 3: orphan cancel
    std::vector<OrderLogRecord> tx3;
    tx3.push_back(make_cancel(999, 100, 0, Side::Buy));
    auto r3 = book.apply_transaction(tx3);
    assert(r3.orphan_cancel_events == 1);

    // Check accumulated BookErrors counters
    assert(book.errors().tx_grouped_orphan_cancel_events == 1);
    assert(book.errors().tx_grouped_missing_order_id == 1);
    assert(book.errors().transactions_grouped == 3);

    std::cout << "  PASS: multiple_transactions_accumulate" << std::endl;
}

// Test 9: ADD then CANCEL in same transaction (order added then cancelled)
static void test_add_cancel_same_transaction() {
    OrderBook book;

    std::vector<OrderLogRecord> tx;
    tx.push_back(make_add(1, 100, 10, Side::Buy));
    tx.push_back(make_cancel(1, 100, 0, Side::Buy));

    auto result = book.apply_transaction(tx);
    assert(result.records_processed == 2);
    assert(result.orphan_cancel_events == 0);
    assert(result.orphan_cancel_resolved_in_transaction == 0);
    assert(result.missing_order_id == 0);

    // Book should be empty on bid side
    assert(book.bid_depth() == 0);

    std::cout << "  PASS: add_cancel_same_transaction" << std::endl;
}

// Test 10: Empty transaction batch
static void test_empty_transaction() {
    OrderBook book;

    std::vector<OrderLogRecord> tx;
    auto result = book.apply_transaction(tx);
    assert(result.records_processed == 0);
    assert(result.missing_order_id == 0);
    assert(result.orphan_fill_events == 0);

    std::cout << "  PASS: empty_transaction" << std::endl;
}

// Test 11: max_records_in_transaction tracking
static void test_max_records_in_transaction() {
    OrderBook book;

    // Small transaction
    std::vector<OrderLogRecord> tx1;
    tx1.push_back(make_add(1, 100, 10, Side::Buy));
    book.apply_transaction(tx1);

    // Larger transaction
    std::vector<OrderLogRecord> tx2;
    tx2.push_back(make_add(2, 101, 5, Side::Buy));
    tx2.push_back(make_add(3, 102, 3, Side::Buy));
    tx2.push_back(make_add(4, 200, 8, Side::Sell));
    book.apply_transaction(tx2);

    assert(book.errors().max_records_in_transaction == 3);
    assert(book.errors().records_in_grouped_transactions == 4);
    assert(book.errors().transactions_grouped == 2);

    std::cout << "  PASS: max_records_in_transaction" << std::endl;
}

// Test 12: ADD+FILL+ADD in same transaction (fill resolved, second add works)
static void test_add_fill_add_same_transaction() {
    OrderBook book;

    std::vector<OrderLogRecord> tx;
    tx.push_back(make_add(1, 100, 10, Side::Buy));
    tx.push_back(make_fill(1, 100, 10, 0, Side::Buy));  // Full fill
    tx.push_back(make_add(2, 101, 5, Side::Buy));  // New order

    auto result = book.apply_transaction(tx);
    assert(result.records_processed == 3);
    assert(result.orphan_fill_events == 0);
    assert(result.missing_order_id == 0);

    // Book should have order 2 only
    assert(book.bid_depth() == 1);
    assert(book.best_bid() == 101);
    auto snap = book.snapshot(1);
    assert(snap[0].bid_qty == 5);

    std::cout << "  PASS: add_fill_add_same_transaction" << std::endl;
}

// Test 13: TransactionBatchResult fields all initialized
static void test_transaction_result_defaults() {
    TransactionBatchResult result;
    assert(result.records_processed == 0);
    assert(result.orphan_fill_events == 0);
    assert(result.orphan_cancel_events == 0);
    assert(result.orphan_remove_events == 0);
    assert(result.orphan_fill_resolved_in_transaction == 0);
    assert(result.orphan_cancel_resolved_in_transaction == 0);
    assert(result.orphan_remove_resolved_in_transaction == 0);
    assert(result.missing_order_id == 0);

    std::cout << "  PASS: transaction_result_defaults" << std::endl;
}

int main() {
    std::cout << "=== test_tx_grouped ===" << std::endl;
    test_per_record_mode_unchanged();
    test_tx_grouped_batches_until_txend();
    test_add_fill_same_transaction();
    test_add_fill_remove_same_transaction();
    test_action_on_order_from_earlier_in_tx();
    test_orphan_cancel_remove_tracked();
    test_orphan_fill_tracked();
    test_multiple_transactions_accumulate();
    test_add_cancel_same_transaction();
    test_empty_transaction();
    test_max_records_in_transaction();
    test_add_fill_add_same_transaction();
    test_transaction_result_defaults();
    std::cout << "\nAll tx-grouped tests passed." << std::endl;
    return 0;
}
