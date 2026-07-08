#include "qsh/ordlog_reader.hpp"
#include "qsh/leb128.hpp"

namespace qsh {

bool OrdLogReader::next(QshFile& file, OrderLogRecord& record) {
    if (file.eof()) return false;

    const uint8_t* data = file.data.data();
    size_t size = file.data.size();
    size_t& offset = file.data_offset;

    try {
        // frame_time_delta (growing integer)
        record.frame_time_delta = read_growing(data, size, offset);

        // entry_flags
        record.entry_flags = read_u8(data, size, offset);

        // order_flags (u16 LE)
        record.order_flags = read_u16_le(data, size, offset);

        // Reset per-record fields
        record.amount_rest = 0;
        record.deal_id = 0;
        record.deal_price = 0;
        record.oi = 0;

        // Read fields based on entry_flags
        if (has_flag(record.entry_flags, OLEntryFlags::DateTime)) {
            record.timestamp += read_growing(data, size, offset);
        }

        if (has_flag(record.entry_flags, OLEntryFlags::OrderId)) {
            if (has_flag(record.order_flags, OLFlags::Add)) {
                // Add: absolute increment to running order_id
                order_id_ += read_growing(data, size, offset);
                record.order_id = order_id_;
            } else {
                // Not Add: delta from current order_id
                order_id_ += read_leb128(data, size, offset);
                record.order_id = order_id_;
            }
        } else {
            record.order_id = order_id_;
        }

        if (has_flag(record.entry_flags, OLEntryFlags::Price)) {
            record.price += read_leb128(data, size, offset);
        }

        if (has_flag(record.entry_flags, OLEntryFlags::Amount)) {
            record.amount = read_leb128(data, size, offset);
        }

        // Fill-specific fields
        if (has_flag(record.order_flags, OLFlags::Fill)) {
            if (has_flag(record.entry_flags, OLEntryFlags::AmountRest)) {
                record.amount_rest = read_leb128(data, size, offset);
            }
            if (has_flag(record.entry_flags, OLEntryFlags::DealId)) {
                deal_id_ += read_growing(data, size, offset);
                record.deal_id = deal_id_;
            } else {
                record.deal_id = deal_id_;
            }
            if (has_flag(record.entry_flags, OLEntryFlags::DealPrice)) {
                deal_price_ += read_leb128(data, size, offset);
                record.deal_price = deal_price_;
            } else {
                record.deal_price = deal_price_;
            }
            if (has_flag(record.entry_flags, OLEntryFlags::OI)) {
                oi_ += read_leb128(data, size, offset);
                record.oi = oi_;
            } else {
                record.oi = oi_;
            }
        } else if (has_flag(record.order_flags, OLFlags::Add)) {
            record.amount_rest = record.amount;
        }

        // Determine side
        bool buy = has_flag(record.order_flags, OLFlags::Buy);
        bool sell = has_flag(record.order_flags, OLFlags::Sell);
        if (buy && sell) {
            record.side = Side::Unknown;  // Both flags set is invalid
        } else if (buy) {
            record.side = Side::Buy;
        } else if (sell) {
            record.side = Side::Sell;
        } else {
            record.side = Side::Unknown;
        }

        // Classify event type
        record.event = classify_ol_event(record);

        return true;

    } catch (const std::exception&) {
        return false;
    }
}

size_t OrdLogReader::scan_all(QshFile& file, const std::function<void(const OrderLogRecord&)>& callback) {
    size_t count = 0;
    OrderLogRecord rec;
    while (next(file, rec)) {
        callback(rec);
        ++count;
    }
    return count;
}

OLMsgType classify_ol_event(const OrderLogRecord& rec) {
    if (has_flag(rec.order_flags, OLFlags::Add)) {
        return OLMsgType::Add;
    }
    if (has_flag(rec.order_flags, OLFlags::Fill)) {
        return OLMsgType::Fill;
    }
    if (has_flag(rec.order_flags, OLFlags::Canceled) ||
        has_flag(rec.order_flags, OLFlags::CanceledGroup) ||
        has_flag(rec.order_flags, OLFlags::Moved)) {
        return OLMsgType::Cancel;
    }
    if (has_flag(rec.order_flags, OLFlags::CrossTrade) || rec.amount_rest == 0) {
        return OLMsgType::Remove;
    }
    return OLMsgType::Unknown;
}

bool is_system_record(const OrderLogRecord& rec) {
    return !has_flag(rec.order_flags, OLFlags::NonSystem) &&
           !has_flag(rec.order_flags, OLFlags::NonZeroReplAct) &&
           rec.side != Side::Unknown;
}

bool is_tx_end(const OrderLogRecord& rec) {
    return has_flag(rec.order_flags, OLFlags::TxEnd);
}

}  // namespace qsh
