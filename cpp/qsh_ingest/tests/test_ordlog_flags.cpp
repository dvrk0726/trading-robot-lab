#include "qsh/qsh_types.hpp"
#include "qsh/ordlog_reader.hpp"
#include <cassert>
#include <iostream>

using namespace qsh;

static void test_ol_flags() {
    uint16_t flags = OLFlags::Add | OLFlags::Buy | OLFlags::TxEnd;
    assert(has_flag(flags, OLFlags::Add));
    assert(has_flag(flags, OLFlags::Buy));
    assert(has_flag(flags, OLFlags::TxEnd));
    assert(!has_flag(flags, OLFlags::Fill));
    assert(!has_flag(flags, OLFlags::Sell));
    assert(!has_flag(flags, OLFlags::Canceled));
    std::cout << "  PASS: OLFlags basic" << std::endl;
}

static void test_entry_flags() {
    uint8_t flags = OLEntryFlags::DateTime | OLEntryFlags::OrderId | OLEntryFlags::Price;
    assert(has_flag(flags, OLEntryFlags::DateTime));
    assert(has_flag(flags, OLEntryFlags::OrderId));
    assert(has_flag(flags, OLEntryFlags::Price));
    assert(!has_flag(flags, OLEntryFlags::Amount));
    assert(!has_flag(flags, OLEntryFlags::DealId));
    std::cout << "  PASS: OLEntryFlags basic" << std::endl;
}

static void test_deal_flags() {
    uint8_t flags = DealFlags::Timestamp | DealFlags::Price | DealFlags::Amount;
    assert(has_flag(flags, DealFlags::Timestamp));
    assert(has_flag(flags, DealFlags::Price));
    assert(has_flag(flags, DealFlags::Amount));
    assert(!has_flag(flags, DealFlags::OI));
    std::cout << "  PASS: DealFlags basic" << std::endl;
}

static void test_auxinfo_flags() {
    uint8_t flags = AuxInfoFlags::Timestamp | AuxInfoFlags::Price | AuxInfoFlags::SessionInfo;
    assert(has_flag(flags, AuxInfoFlags::Timestamp));
    assert(has_flag(flags, AuxInfoFlags::Price));
    assert(has_flag(flags, AuxInfoFlags::SessionInfo));
    assert(!has_flag(flags, AuxInfoFlags::Message));
    std::cout << "  PASS: AuxInfoFlags basic" << std::endl;
}

static void test_classify_add() {
    OrderLogRecord rec;
    rec.order_flags = OLFlags::Add | OLFlags::Buy;
    rec.amount_rest = 10;
    assert(classify_ol_event(rec) == OLMsgType::Add);
    std::cout << "  PASS: classify Add" << std::endl;
}

static void test_classify_fill() {
    OrderLogRecord rec;
    rec.order_flags = OLFlags::Fill | OLFlags::Sell;
    rec.amount = 5;
    assert(classify_ol_event(rec) == OLMsgType::Fill);
    std::cout << "  PASS: classify Fill" << std::endl;
}

static void test_classify_cancel() {
    OrderLogRecord rec;
    rec.order_flags = OLFlags::Canceled | OLFlags::Buy;
    assert(classify_ol_event(rec) == OLMsgType::Cancel);
    std::cout << "  PASS: classify Cancel" << std::endl;
}

static void test_classify_cancel_group() {
    OrderLogRecord rec;
    rec.order_flags = OLFlags::CanceledGroup | OLFlags::Sell;
    assert(classify_ol_event(rec) == OLMsgType::Cancel);
    std::cout << "  PASS: classify CancelGroup" << std::endl;
}

static void test_classify_moved() {
    OrderLogRecord rec;
    rec.order_flags = OLFlags::Moved | OLFlags::Buy;
    assert(classify_ol_event(rec) == OLMsgType::Cancel);
    std::cout << "  PASS: classify Moved" << std::endl;
}

static void test_classify_remove_cross_trade() {
    OrderLogRecord rec;
    rec.order_flags = OLFlags::CrossTrade | OLFlags::Buy;
    rec.amount_rest = 0;
    assert(classify_ol_event(rec) == OLMsgType::Remove);
    std::cout << "  PASS: classify Remove (CrossTrade)" << std::endl;
}

static void test_classify_remove_zero_rest() {
    OrderLogRecord rec;
    rec.order_flags = OLFlags::Buy;
    rec.amount_rest = 0;
    assert(classify_ol_event(rec) == OLMsgType::Remove);
    std::cout << "  PASS: classify Remove (amount_rest=0)" << std::endl;
}

static void test_side_from_flags() {
    OrderLogRecord rec;
    rec.order_flags = OLFlags::Buy;
    rec.side = has_flag(rec.order_flags, OLFlags::Buy) ? Side::Buy : Side::Unknown;
    assert(rec.side == Side::Buy);

    rec.order_flags = OLFlags::Sell;
    rec.side = has_flag(rec.order_flags, OLFlags::Sell) ? Side::Sell : Side::Unknown;
    assert(rec.side == Side::Sell);
    std::cout << "  PASS: side from flags" << std::endl;
}

static void test_system_record() {
    OrderLogRecord rec;
    rec.order_flags = OLFlags::Add | OLFlags::Buy;
    rec.side = Side::Buy;
    assert(is_system_record(rec));

    rec.order_flags = OLFlags::NonSystem | OLFlags::Add | OLFlags::Buy;
    assert(!is_system_record(rec));
    std::cout << "  PASS: system record check" << std::endl;
}

static void test_tx_end() {
    OrderLogRecord rec;
    rec.order_flags = OLFlags::TxEnd | OLFlags::Fill;
    assert(is_tx_end(rec));

    rec.order_flags = OLFlags::Fill;
    assert(!is_tx_end(rec));
    std::cout << "  PASS: TxEnd check" << std::endl;
}

int main() {
    std::cout << "=== test_ordlog_flags ===" << std::endl;
    test_ol_flags();
    test_entry_flags();
    test_deal_flags();
    test_auxinfo_flags();
    test_classify_add();
    test_classify_fill();
    test_classify_cancel();
    test_classify_cancel_group();
    test_classify_moved();
    test_classify_remove_cross_trade();
    test_classify_remove_zero_rest();
    test_side_from_flags();
    test_system_record();
    test_tx_end();
    std::cout << "\nAll OrdLog flags tests passed." << std::endl;
    return 0;
}
