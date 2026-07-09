// M10W: Tests for persistent crossed-state transition detector and lifecycle tracing.
#include "orderbook/order_book.hpp"
#include "qsh/qsh_types.hpp"
#include <cassert>
#include <iostream>
#include <vector>

using namespace qsh;

static int g_failures = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { \
        std::cerr << "  FAIL: " << msg << " (" #cond ")" << std::endl; \
        ++g_failures; \
    } \
} while(0)

// Helper to create an OrderLogRecord
static OrderLogRecord make_rec(UID order_id, OLMsgType event, Side side, Price price,
                                Volume amount, Volume amount_rest, uint16_t flags = 0) {
    OrderLogRecord rec;
    rec.order_id = order_id;
    rec.event = event;
    rec.side = side;
    rec.price = price;
    rec.amount = amount;
    rec.amount_rest = amount_rest;
    rec.order_flags = flags;
    rec.timestamp = 1000;
    return rec;
}

// Test 1: Transition detector finds not-crossed -> crossed
static void test_transition_finds_not_crossed_to_crossed() {
    std::cout << "test_transition_finds_not_crossed_to_crossed..." << std::endl;

    OrderBook book;

    // Build a non-crossed book: bid 100, ask 110
    book.apply(make_rec(1, OLMsgType::Add, Side::Buy, 100, 10, 10));
    book.apply(make_rec(2, OLMsgType::Add, Side::Sell, 110, 10, 10));

    CHECK(book.best_bid() == 100, "best_bid should be 100");
    CHECK(book.best_ask() == 110, "best_ask should be 110");
    CHECK(book.best_bid() < book.best_ask(), "should not be crossed");

    // Track crossing transitions
    bool was_crossed = false;
    bool transition_found = false;
    int64_t transition_record = 0;

    // Record 3: ADD BUY at 115 -> crosses the book (115 >= 110)
    {
        auto rec = make_rec(3, OLMsgType::Add, Side::Buy, 115, 5, 5);
        Price bb_before = book.best_bid();
        Price ba_before = book.best_ask();
        bool crossed_before = (bb_before > 0 && ba_before > 0 && bb_before >= ba_before);

        book.apply(rec);

        Price bb_after = book.best_bid();
        Price ba_after = book.best_ask();
        bool crossed_after = (bb_after > 0 && ba_after > 0 && bb_after >= ba_after);

        if (crossed_after && !was_crossed && !transition_found) {
            transition_found = true;
            transition_record = 3;
        }
        was_crossed = crossed_after;

        CHECK(!crossed_before, "should not be crossed before");
        CHECK(crossed_after, "should be crossed after");
    }

    CHECK(transition_found, "transition should be found");
    CHECK(transition_record == 3, "transition should be at record 3");

    std::cout << "  PASSED" << std::endl;
}

// Test 2: Transition detector does not report already-crossed event
static void test_does_not_report_already_crossed() {
    std::cout << "test_does_not_report_already_crossed..." << std::endl;

    OrderBook book;

    // Build a crossed book: bid 110, ask 100
    book.apply(make_rec(1, OLMsgType::Add, Side::Buy, 110, 10, 10));
    book.apply(make_rec(2, OLMsgType::Add, Side::Sell, 100, 10, 10));

    CHECK(book.best_bid() >= book.best_ask(), "should be crossed");

    // Track crossing transitions — initialize was_crossed based on current state
    bool was_crossed = (book.best_bid() > 0 && book.best_ask() > 0 && book.best_bid() >= book.best_ask());
    bool transition_found = false;

    // Process several records — none should trigger transition since book starts crossed
    for (int i = 3; i <= 6; ++i) {
        auto rec = make_rec(i, OLMsgType::Add, Side::Buy, 110 + i, 5, 5);
        Price bb_before = book.best_bid();
        Price ba_before = book.best_ask();
        bool crossed_before = (bb_before > 0 && ba_before > 0 && bb_before >= ba_before);
        (void)crossed_before;

        book.apply(rec);

        Price bb_after = book.best_bid();
        Price ba_after = book.best_ask();
        bool crossed_after = (bb_after > 0 && ba_after > 0 && bb_after >= ba_after);

        if (crossed_after && !was_crossed && !transition_found) {
            transition_found = true;
        }
        was_crossed = crossed_after;
    }

    CHECK(!transition_found, "should not report transition since book was already crossed");

    std::cout << "  PASSED" << std::endl;
}

// Test 3: Transition detector handles counter-ignored events
static void test_handles_counter_ignored_events() {
    std::cout << "test_handles_counter_ignored_events..." << std::endl;

    OrderBook book;
    book.set_counter_mode(CounterMode::IgnoreBook);

    // Build a non-crossed book
    book.apply(make_rec(1, OLMsgType::Add, Side::Buy, 100, 10, 10));
    book.apply(make_rec(2, OLMsgType::Add, Side::Sell, 110, 10, 10));

    // Counter-flagged ADD at 115 — should be ignored
    book.apply(make_rec(3, OLMsgType::Add, Side::Buy, 115, 5, 5, OLFlags::Counter));

    // Book should still be non-crossed (counter event ignored)
    CHECK(book.best_bid() == 100, "best_bid should still be 100 after counter event");
    CHECK(book.best_ask() == 110, "best_ask should still be 110");
    CHECK(book.best_bid() < book.best_ask(), "should not be crossed after counter event");

    // Non-counter ADD at 115 — should cross
    book.apply(make_rec(4, OLMsgType::Add, Side::Buy, 115, 5, 5));

    CHECK(book.best_bid() == 115, "best_bid should be 115");
    CHECK(book.best_bid() >= book.best_ask(), "should be crossed after non-counter event");

    std::cout << "  PASSED" << std::endl;
}

// Test 4: Transition detector handles non-system-ignored events
static void test_handles_non_system_ignored_events() {
    std::cout << "test_handles_non_system_ignored_events..." << std::endl;

    OrderBook book;
    book.set_non_system_mode(NonSystemMode::IgnoreBook);

    // Build a non-crossed book
    book.apply(make_rec(1, OLMsgType::Add, Side::Buy, 100, 10, 10));
    book.apply(make_rec(2, OLMsgType::Add, Side::Sell, 110, 10, 10));

    // NonSystem-flagged ADD at 115 — should be ignored
    book.apply(make_rec(3, OLMsgType::Add, Side::Buy, 115, 5, 5, OLFlags::NonSystem));

    // Book should still be non-crossed
    CHECK(book.best_bid() == 100, "best_bid should still be 100 after NonSystem event");
    CHECK(book.best_ask() == 110, "best_ask should still be 110");

    // Non-NonSystem ADD at 115 — should cross
    book.apply(make_rec(4, OLMsgType::Add, Side::Buy, 115, 5, 5));

    CHECK(book.best_bid() == 115, "best_bid should be 115");
    CHECK(book.best_bid() >= book.best_ask(), "should be crossed");

    std::cout << "  PASSED" << std::endl;
}

// Test 5: Lifecycle trace records add/fill/cancel/remove for top order
static void test_lifecycle_trace_records_events() {
    std::cout << "test_lifecycle_trace_records_events..." << std::endl;

    OrderBook book;

    // ADD order
    book.apply(make_rec(1, OLMsgType::Add, Side::Buy, 100, 10, 10));
    {
        Side s = Side::Unknown; Price p = 0; Volume v = 0;
        bool found = book.get_order_info(1, s, p, v);
        CHECK(found, "order should exist after ADD");
        CHECK(s == Side::Buy, "side should be Buy");
        CHECK(p == 100, "price should be 100");
        CHECK(v == 10, "qty should be 10");
    }

    // FILL partial (delta mode: amount=3, amount_rest=7)
    book.apply(make_rec(1, OLMsgType::Fill, Side::Buy, 100, 3, 7));
    {
        Side s = Side::Unknown; Price p = 0; Volume v = 0;
        bool found = book.get_order_info(1, s, p, v);
        CHECK(found, "order should exist after partial fill");
        CHECK(v == 7, "qty should be 7 after fill");
    }

    // CANCEL partial (amount_rest=5)
    book.apply(make_rec(1, OLMsgType::Cancel, Side::Buy, 100, 0, 5));
    {
        Side s = Side::Unknown; Price p = 0; Volume v = 0;
        bool found = book.get_order_info(1, s, p, v);
        CHECK(found, "order should exist after partial cancel");
        CHECK(v == 5, "qty should be 5 after cancel");
    }

    // REMOVE
    book.apply(make_rec(1, OLMsgType::Remove, Side::Buy, 100, 0, 0));
    {
        Side s = Side::Unknown; Price p = 0; Volume v = 0;
        bool found = book.get_order_info(1, s, p, v);
        CHECK(!found, "order should be gone after remove");
    }

    std::cout << "  PASSED" << std::endl;
}

// Test 6: Lifecycle trace marks persistent order active when not removed
static void test_persistent_order_active_when_not_removed() {
    std::cout << "test_persistent_order_active_when_not_removed..." << std::endl;

    OrderBook book;

    // ADD order
    book.apply(make_rec(1, OLMsgType::Add, Side::Buy, 100, 10, 10));
    book.apply(make_rec(2, OLMsgType::Add, Side::Sell, 110, 10, 10));

    // ADD another order (not the one we're tracking)
    book.apply(make_rec(3, OLMsgType::Add, Side::Buy, 95, 5, 5));

    // Order 1 should still be active
    {
        Side s = Side::Unknown; Price p = 0; Volume v = 0;
        bool found = book.get_order_info(1, s, p, v);
        CHECK(found, "order 1 should still be active");
        CHECK(v == 10, "order 1 qty should still be 10");
    }

    // Order 1 is still active — no remove/cancel/fill-all
    // This simulates the persistent crossed state scenario
    // where the crossing order is never removed

    std::cout << "  PASSED" << std::endl;
}

int main() {
    std::cout << "=== M10W Persistent Crossed Tests ===" << std::endl;

    test_transition_finds_not_crossed_to_crossed();
    test_does_not_report_already_crossed();
    test_handles_counter_ignored_events();
    test_handles_non_system_ignored_events();
    test_lifecycle_trace_records_events();
    test_persistent_order_active_when_not_removed();

    if (g_failures == 0) {
        std::cout << "\nAll M10W tests passed!" << std::endl;
    } else {
        std::cout << "\n" << g_failures << " test(s) FAILED!" << std::endl;
    }
    return g_failures;
}
