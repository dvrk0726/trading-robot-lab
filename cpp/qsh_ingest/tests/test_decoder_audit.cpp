#include "qsh/qsh_types.hpp"
#include "qsh/ordlog_reader.hpp"
#include "qsh/leb128.hpp"
#include <cassert>
#include <iostream>
#include <vector>
#include <cstring>

using namespace qsh;

// Helper: build a minimal QshFile with raw OrdLog bytes
static QshFile make_test_qsh(const std::vector<uint8_t>& raw_data) {
    QshFile f;
    f.valid = true;
    f.data = raw_data;
    f.data_offset = 0;
    f.header.stream = StreamType::OrderLog;
    return f;
}

// Helper: encode a ULEB128 value into a byte vector
static void push_uleb128(std::vector<uint8_t>& buf, uint64_t value) {
    do {
        uint8_t byte = value & 0x7F;
        value >>= 7;
        if (value != 0) byte |= 0x80;
        buf.push_back(byte);
    } while (value != 0);
}

// Helper: encode a signed LEB128 value into a byte vector
static void push_leb128(std::vector<uint8_t>& buf, int64_t value) {
    bool more = true;
    while (more) {
        uint8_t byte = value & 0x7F;
        value >>= 7;
        if ((value == 0 && (byte & 0x40) == 0) || (value == -1 && (byte & 0x40) != 0)) {
            more = false;
        } else {
            byte |= 0x80;
        }
        buf.push_back(byte);
    }
}

// Helper: push u16 LE
static void push_u16_le(std::vector<uint8_t>& buf, uint16_t v) {
    buf.push_back(static_cast<uint8_t>(v & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
}

// Build a single OrdLog record's raw bytes
// frame_time_delta=1, flags as given, optional fields
struct RecordBuilder {
    uint8_t entry_flags = 0;
    uint16_t order_flags = 0;
    int64_t order_id_delta = 0;
    int64_t price_delta = 0;
    int64_t amount = 0;
    bool has_order_id = false;
    bool has_price = false;
    bool has_amount = false;

    std::vector<uint8_t> build() const {
        std::vector<uint8_t> buf;
        // frame_time_delta = 1 (growing integer, ULEB128 encoded)
        push_uleb128(buf, 1);
        // entry_flags
        buf.push_back(entry_flags);
        // order_flags
        push_u16_le(buf, order_flags);

        if (has_order_id) {
            if (order_flags & OLFlags::Add) {
                push_uleb128(buf, static_cast<uint64_t>(order_id_delta));
            } else {
                push_leb128(buf, order_id_delta);
            }
        }
        if (has_price) {
            push_leb128(buf, price_delta);
        }
        if (has_amount) {
            push_leb128(buf, amount);
        }
        return buf;
    }
};

// --- Tests ---

// Test 1: Add event classification from flags
static void test_flag_mapping_add() {
    OrderLogRecord rec;
    rec.order_flags = OLFlags::Add | OLFlags::Buy;
    rec.amount_rest = 10;
    assert(classify_ol_event(rec) == OLMsgType::Add);

    // Add with Sell
    rec.order_flags = OLFlags::Add | OLFlags::Sell;
    assert(classify_ol_event(rec) == OLMsgType::Add);
    std::cout << "  PASS: flag mapping Add" << std::endl;
}

// Test 2: Fill event classification from flags
static void test_flag_mapping_fill() {
    OrderLogRecord rec;
    rec.order_flags = OLFlags::Fill | OLFlags::Buy;
    assert(classify_ol_event(rec) == OLMsgType::Fill);

    rec.order_flags = OLFlags::Fill | OLFlags::Sell;
    assert(classify_ol_event(rec) == OLMsgType::Fill);
    std::cout << "  PASS: flag mapping Fill" << std::endl;
}

// Test 3: Cancel event classification
static void test_flag_mapping_cancel() {
    OrderLogRecord rec;
    rec.order_flags = OLFlags::Canceled | OLFlags::Buy;
    assert(classify_ol_event(rec) == OLMsgType::Cancel);

    rec.order_flags = OLFlags::CanceledGroup | OLFlags::Sell;
    assert(classify_ol_event(rec) == OLMsgType::Cancel);
    std::cout << "  PASS: flag mapping Cancel" << std::endl;
}

// Test 4: Moved event classification
static void test_flag_mapping_moved() {
    OrderLogRecord rec;
    rec.order_flags = OLFlags::Moved | OLFlags::Buy;
    assert(classify_ol_event(rec) == OLMsgType::Moved);
    std::cout << "  PASS: flag mapping Moved" << std::endl;
}

// Test 5: Remove event classification (CrossTrade)
static void test_flag_mapping_remove_cross_trade() {
    OrderLogRecord rec;
    rec.order_flags = OLFlags::CrossTrade | OLFlags::Buy;
    rec.amount_rest = 0;
    assert(classify_ol_event(rec) == OLMsgType::Remove);
    std::cout << "  PASS: flag mapping Remove (CrossTrade)" << std::endl;
}

// Test 6: Remove event classification (amount_rest == 0)
static void test_flag_mapping_remove_zero_rest() {
    OrderLogRecord rec;
    rec.order_flags = OLFlags::Buy;
    rec.amount_rest = 0;
    assert(classify_ol_event(rec) == OLMsgType::Remove);
    std::cout << "  PASS: flag mapping Remove (amount_rest=0)" << std::endl;
}

// Test 7: Side mapping from flags
static void test_side_mapping() {
    OrderLogRecord rec;

    // Buy only
    rec.order_flags = OLFlags::Add | OLFlags::Buy;
    bool buy = has_flag(rec.order_flags, OLFlags::Buy);
    bool sell = has_flag(rec.order_flags, OLFlags::Sell);
    assert(buy && !sell);
    Side side = buy ? Side::Buy : (sell ? Side::Sell : Side::Unknown);
    assert(side == Side::Buy);

    // Sell only
    rec.order_flags = OLFlags::Add | OLFlags::Sell;
    buy = has_flag(rec.order_flags, OLFlags::Buy);
    sell = has_flag(rec.order_flags, OLFlags::Sell);
    assert(!buy && sell);
    side = buy ? Side::Buy : (sell ? Side::Sell : Side::Unknown);
    assert(side == Side::Sell);

    // Neither
    rec.order_flags = OLFlags::Add;
    buy = has_flag(rec.order_flags, OLFlags::Buy);
    sell = has_flag(rec.order_flags, OLFlags::Sell);
    assert(!buy && !sell);
    side = buy ? Side::Buy : (sell ? Side::Sell : Side::Unknown);
    assert(side == Side::Unknown);

    // Both (invalid)
    rec.order_flags = OLFlags::Add | OLFlags::Buy | OLFlags::Sell;
    buy = has_flag(rec.order_flags, OLFlags::Buy);
    sell = has_flag(rec.order_flags, OLFlags::Sell);
    assert(buy && sell);
    // Both set = Unknown (as implemented)
    std::cout << "  PASS: side mapping" << std::endl;
}

// Test 8: repl_act (NonZeroReplAct) mapping
static void test_repl_act_mapping() {
    OrderLogRecord rec;

    // NonZeroReplAct set
    rec.order_flags = OLFlags::Add | OLFlags::Buy | OLFlags::NonZeroReplAct;
    assert(has_flag(rec.order_flags, OLFlags::NonZeroReplAct));
    assert(!is_system_record(rec));  // NonZeroReplAct makes it non-system

    // NonZeroReplAct not set
    rec.order_flags = OLFlags::Add | OLFlags::Buy;
    assert(!has_flag(rec.order_flags, OLFlags::NonZeroReplAct));
    assert(is_system_record(rec));  // System record
    std::cout << "  PASS: repl_act mapping" << std::endl;
}

// Test 9: system vs non-system record detection
static void test_system_vs_non_system() {
    OrderLogRecord rec;

    // System record: no NonSystem, no NonZeroReplAct, valid side
    rec.order_flags = OLFlags::Add | OLFlags::Buy;
    rec.side = Side::Buy;
    assert(is_system_record(rec));

    // Non-system: NonSystem flag
    rec.order_flags = OLFlags::NonSystem | OLFlags::Add | OLFlags::Buy;
    rec.side = Side::Buy;
    assert(!is_system_record(rec));

    // Non-system: NonZeroReplAct flag
    rec.order_flags = OLFlags::NonZeroReplAct | OLFlags::Add | OLFlags::Buy;
    rec.side = Side::Buy;
    assert(!is_system_record(rec));

    // Non-system: Unknown side
    rec.order_flags = OLFlags::Add;
    rec.side = Side::Unknown;
    assert(!is_system_record(rec));
    std::cout << "  PASS: system vs non-system" << std::endl;
}

// Test 10: TxEnd detection
static void test_tx_end_detection() {
    OrderLogRecord rec;
    rec.order_flags = OLFlags::TxEnd | OLFlags::Fill;
    assert(is_tx_end(rec));

    rec.order_flags = OLFlags::Fill;
    assert(!is_tx_end(rec));
    std::cout << "  PASS: TxEnd detection" << std::endl;
}

// Test 11: order_id carry-forward when OrderId flag absent
static void test_order_id_carry_forward() {
    // Build two records: first has OrderId, second does not
    RecordBuilder rb1;
    rb1.entry_flags = OLEntryFlags::OrderId;
    rb1.order_flags = OLFlags::Add | OLFlags::Buy;
    rb1.has_order_id = true;
    rb1.order_id_delta = 100;  // order_id goes from 0 to 100

    RecordBuilder rb2;
    rb2.entry_flags = 0;  // No OrderId flag
    rb2.order_flags = OLFlags::Fill | OLFlags::Buy;

    std::vector<uint8_t> data;
    auto d1 = rb1.build();
    auto d2 = rb2.build();
    data.insert(data.end(), d1.begin(), d1.end());
    data.insert(data.end(), d2.begin(), d2.end());

    auto file = make_test_qsh(data);
    OrdLogReader reader;

    OrderLogRecord rec;
    assert(reader.next(file, rec));
    assert(rec.order_id == 100);

    assert(reader.next(file, rec));
    // order_id should be carried forward (still 100)
    assert(rec.order_id == 100);
    std::cout << "  PASS: order_id carry-forward" << std::endl;
}

// Test 12: order_id delta for non-Add records
static void test_order_id_delta_non_add() {
    RecordBuilder rb1;
    rb1.entry_flags = OLEntryFlags::OrderId;
    rb1.order_flags = OLFlags::Add | OLFlags::Buy;
    rb1.has_order_id = true;
    rb1.order_id_delta = 100;

    RecordBuilder rb2;
    rb2.entry_flags = OLEntryFlags::OrderId;
    rb2.order_flags = OLFlags::Fill | OLFlags::Buy;
    rb2.has_order_id = true;
    rb2.order_id_delta = 5;  // delta = +5, so order_id = 105

    std::vector<uint8_t> data;
    auto d1 = rb1.build();
    auto d2 = rb2.build();
    data.insert(data.end(), d1.begin(), d1.end());
    data.insert(data.end(), d2.begin(), d2.end());

    auto file = make_test_qsh(data);
    OrdLogReader reader;

    OrderLogRecord rec;
    assert(reader.next(file, rec));
    assert(rec.order_id == 100);

    assert(reader.next(file, rec));
    assert(rec.order_id == 105);  // 100 + 5
    std::cout << "  PASS: order_id delta for non-Add" << std::endl;
}

// Test 13: unknown flags do not silently become ADD or FILL
static void test_unknown_flags_not_misclassified() {
    OrderLogRecord rec;

    // Only TxEnd set (no Add, no Fill, no Canceled, no CrossTrade)
    rec.order_flags = OLFlags::TxEnd;
    rec.amount_rest = 10;
    assert(classify_ol_event(rec) == OLMsgType::Unknown);

    // Only Snapshot set
    rec.order_flags = OLFlags::Snapshot;
    rec.amount_rest = 10;
    assert(classify_ol_event(rec) == OLMsgType::Unknown);

    // Only Quote set
    rec.order_flags = OLFlags::Quote;
    rec.amount_rest = 10;
    assert(classify_ol_event(rec) == OLMsgType::Unknown);

    // Only Counter set
    rec.order_flags = OLFlags::Counter;
    rec.amount_rest = 10;
    assert(classify_ol_event(rec) == OLMsgType::Unknown);

    // Only FillOrKill set
    rec.order_flags = OLFlags::FillOrKill;
    rec.amount_rest = 10;
    assert(classify_ol_event(rec) == OLMsgType::Unknown);
    std::cout << "  PASS: unknown flags not misclassified" << std::endl;
}

// Test 14: priority order in classify_ol_event (Add > Fill > Moved > Cancel > Remove)
static void test_classify_priority() {
    OrderLogRecord rec;

    // Add + Fill: Add wins
    rec.order_flags = OLFlags::Add | OLFlags::Fill;
    rec.amount_rest = 10;
    assert(classify_ol_event(rec) == OLMsgType::Add);

    // Add + Canceled: Add wins
    rec.order_flags = OLFlags::Add | OLFlags::Canceled;
    rec.amount_rest = 10;
    assert(classify_ol_event(rec) == OLMsgType::Add);

    // Fill + Canceled: Fill wins
    rec.order_flags = OLFlags::Fill | OLFlags::Canceled;
    rec.amount_rest = 10;
    assert(classify_ol_event(rec) == OLMsgType::Fill);

    // Fill + Moved: Fill wins
    rec.order_flags = OLFlags::Fill | OLFlags::Moved;
    rec.amount_rest = 10;
    assert(classify_ol_event(rec) == OLMsgType::Fill);

    // Moved + Canceled: Moved wins
    rec.order_flags = OLFlags::Moved | OLFlags::Canceled;
    rec.amount_rest = 10;
    assert(classify_ol_event(rec) == OLMsgType::Moved);
    std::cout << "  PASS: classify priority order" << std::endl;
}

// Test 15: debug fields capture before/after delta
static void test_debug_fields_capture() {
    RecordBuilder rb1;
    rb1.entry_flags = OLEntryFlags::OrderId | OLEntryFlags::Price | OLEntryFlags::Amount;
    rb1.order_flags = OLFlags::Add | OLFlags::Buy;
    rb1.has_order_id = true;
    rb1.has_price = true;
    rb1.has_amount = true;
    rb1.order_id_delta = 50;
    rb1.price_delta = 1000;
    rb1.amount = 10;

    RecordBuilder rb2;
    rb2.entry_flags = OLEntryFlags::OrderId | OLEntryFlags::Price;
    rb2.order_flags = OLFlags::Fill | OLFlags::Buy;
    rb2.has_order_id = true;
    rb2.has_price = true;
    rb2.order_id_delta = 3;
    rb2.price_delta = -5;

    std::vector<uint8_t> data;
    auto d1 = rb1.build();
    auto d2 = rb2.build();
    data.insert(data.end(), d1.begin(), d1.end());
    data.insert(data.end(), d2.begin(), d2.end());

    auto file = make_test_qsh(data);
    OrdLogReader reader;

    OrderLogRecord rec;
    // First record (Add)
    assert(reader.next_debug(file, rec));
    assert(rec.debug.order_id_before_delta == 0);
    assert(rec.debug.order_id_after_delta == 50);
    assert(rec.debug.price_before_delta == 0);
    assert(rec.debug.price_after_delta == 1000);
    assert(rec.debug.has_order_id_field == true);
    assert(rec.debug.has_price_field == true);
    assert(rec.debug.has_amount_field == true);
    assert(rec.debug.is_add_order_id_path == true);  // Add uses growing

    // Second record (Fill)
    assert(reader.next_debug(file, rec));
    assert(rec.debug.order_id_before_delta == 50);
    assert(rec.debug.order_id_after_delta == 53);  // 50 + 3
    assert(rec.debug.price_before_delta == 1000);
    assert(rec.debug.price_after_delta == 995);    // 1000 + (-5)
    assert(rec.debug.has_order_id_field == true);
    assert(rec.debug.has_price_field == true);
    assert(rec.debug.has_amount_field == false);   // No Amount flag
    assert(rec.debug.is_add_order_id_path == false);  // Non-Add uses leb128
    std::cout << "  PASS: debug fields capture" << std::endl;
}

// Test 16: Add uses growing integer, non-Add uses signed LEB128 for order_id
static void test_order_id_encoding_path() {
    RecordBuilder rb_add;
    rb_add.entry_flags = OLEntryFlags::OrderId;
    rb_add.order_flags = OLFlags::Add | OLFlags::Buy;
    rb_add.has_order_id = true;
    rb_add.order_id_delta = 200;

    RecordBuilder rb_fill;
    rb_fill.entry_flags = OLEntryFlags::OrderId;
    rb_fill.order_flags = OLFlags::Fill | OLFlags::Buy;
    rb_fill.has_order_id = true;
    rb_fill.order_id_delta = -3;  // Negative delta for non-Add

    std::vector<uint8_t> data;
    auto d1 = rb_add.build();
    auto d2 = rb_fill.build();
    data.insert(data.end(), d1.begin(), d1.end());
    data.insert(data.end(), d2.begin(), d2.end());

    auto file = make_test_qsh(data);
    OrdLogReader reader;

    OrderLogRecord rec;
    assert(reader.next_debug(file, rec));
    assert(rec.debug.is_add_order_id_path == true);
    assert(rec.order_id == 200);

    assert(reader.next_debug(file, rec));
    assert(rec.debug.is_add_order_id_path == false);
    assert(rec.order_id == 197);  // 200 + (-3)
    std::cout << "  PASS: order_id encoding path" << std::endl;
}

// Test 17: flags/repl_act for Snapshot record
static void test_snapshot_flags() {
    OrderLogRecord rec;
    rec.order_flags = OLFlags::Snapshot | OLFlags::Add | OLFlags::Buy;
    rec.side = Side::Buy;
    assert(has_flag(rec.order_flags, OLFlags::Snapshot));
    assert(has_flag(rec.order_flags, OLFlags::Add));
    assert(classify_ol_event(rec) == OLMsgType::Add);
    assert(is_system_record(rec));
    std::cout << "  PASS: snapshot flags" << std::endl;
}

// Test 18: flags for NewSession record
static void test_new_session_flags() {
    OrderLogRecord rec;
    rec.order_flags = OLFlags::NewSession;
    assert(has_flag(rec.order_flags, OLFlags::NewSession));
    assert(classify_ol_event(rec) == OLMsgType::Unknown);
    // NewSession with no side = non-system
    rec.side = Side::Unknown;
    assert(!is_system_record(rec));
    std::cout << "  PASS: new session flags" << std::endl;
}

int main() {
    std::cout << "=== test_decoder_audit ===" << std::endl;
    test_flag_mapping_add();
    test_flag_mapping_fill();
    test_flag_mapping_cancel();
    test_flag_mapping_moved();
    test_flag_mapping_remove_cross_trade();
    test_flag_mapping_remove_zero_rest();
    test_side_mapping();
    test_repl_act_mapping();
    test_system_vs_non_system();
    test_tx_end_detection();
    test_order_id_carry_forward();
    test_order_id_delta_non_add();
    test_unknown_flags_not_misclassified();
    test_classify_priority();
    test_debug_fields_capture();
    test_order_id_encoding_path();
    test_snapshot_flags();
    test_new_session_flags();
    std::cout << "\nAll decoder audit tests passed." << std::endl;
    return 0;
}
