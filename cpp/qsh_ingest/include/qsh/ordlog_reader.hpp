#pragma once

#include "qsh/qsh_types.hpp"
#include "qsh/qsh_reader.hpp"
#include <functional>

namespace qsh {

// Stateful OrdLog parser. Reads records one at a time from a QshFile.
class OrdLogReader {
public:
    // Read the next OrderLogRecord. Returns false on EOF or error.
    bool next(QshFile& file, OrderLogRecord& record);

    // Scan all records, calling callback for each. Returns total count.
    size_t scan_all(QshFile& file, const std::function<void(const OrderLogRecord&)>& callback);

private:
    UID order_id_ = 0;
    UID deal_id_ = 0;
    Price deal_price_ = 0;
    Volume oi_ = 0;
};

// Classify an OrderLogRecord into an OLMsgType.
OLMsgType classify_ol_event(const OrderLogRecord& rec);

// Check if a record is a system record (not non-system, not non-zero-repl-act, not unknown side).
bool is_system_record(const OrderLogRecord& rec);

// Check if a record is a TxEnd marker.
bool is_tx_end(const OrderLogRecord& rec);

}  // namespace qsh
