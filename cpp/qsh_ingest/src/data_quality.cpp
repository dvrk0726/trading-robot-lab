#include "quality/data_quality.hpp"
#include <fstream>
#include <iostream>
#include <sstream>

namespace qsh {

static std::string json_escape(const std::string& s) {
    std::string result;
    result.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"':  result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:   result += c;
        }
    }
    return result;
}

static void write_json_string_array(std::ofstream& out, const std::string& key,
                                     const std::vector<std::string>& items, int indent) {
    std::string pad(indent * 2, ' ');
    out << pad << "\"" << key << "\": [\n";
    for (size_t i = 0; i < items.size(); ++i) {
        out << pad << "  \"" << json_escape(items[i]) << "\"";
        if (i + 1 < items.size()) out << ",";
        out << "\n";
    }
    out << pad << "]";
}

bool write_data_quality_json(const std::string& path, const DataQuality& dq) {
    std::ofstream out(path);
    if (!out.is_open()) return false;

    out << "{\n";
    out << "  \"source_file_name\": \"" << json_escape(dq.source_file_name) << "\",\n";
    out << "  \"source_file_sha256\": \"" << json_escape(dq.source_file_sha256) << "\",\n";
    out << "  \"stream_type\": \"" << json_escape(dq.stream_type) << "\",\n";
    out << "  \"instrument\": \"" << json_escape(dq.instrument) << "\",\n";
    out << "  \"recording_time\": \"" << json_escape(dq.recording_time) << "\",\n";
    out << "  \"records_total\": " << dq.records_total << ",\n";
    out << "  \"records_valid\": " << dq.records_parsed << ",\n";
    out << "  \"records_rejected\": " << dq.records_failed << ",\n";
    out << "  \"timestamp_min\": " << dq.first_timestamp << ",\n";
    out << "  \"timestamp_max\": " << dq.last_timestamp << ",\n";
    out << "  \"add_count\": " << dq.add_count << ",\n";
    out << "  \"fill_count\": " << dq.fill_count << ",\n";
    out << "  \"cancel_count\": " << dq.cancel_count << ",\n";
    out << "  \"remove_count\": " << dq.remove_count << ",\n";
    out << "  \"new_session_count\": " << dq.new_session_count << ",\n";
    out << "  \"tx_end_count\": " << dq.tx_end_count << ",\n";
    out << "  \"tx_count\": " << dq.tx_count << ",\n";
    out << "  \"tx_max_size\": " << dq.tx_max_size << ",\n";
    out << "  \"buy_count\": " << dq.buy_count << ",\n";
    out << "  \"sell_count\": " << dq.sell_count << ",\n";
    out << "  \"unknown_side_count\": " << dq.unknown_side_count << ",\n";
    out << "  \"non_system_count\": " << dq.non_system_count << ",\n";
    out << "  \"non_zero_repl_act_count\": " << dq.non_zero_repl_act_count << ",\n";
    out << "  \"snapshot_count\": " << dq.snapshot_count << ",\n";
    out << "  \"moved_count\": " << dq.moved_count << ",\n";
    out << "  \"cross_trade_count\": " << dq.cross_trade_count << ",\n";
    out << "  \"book_reconstruction_errors\": {\n";
    out << "    \"missing_order_id\": " << dq.book_missing_order_id << ",\n";
    out << "    \"level_not_found\": " << dq.book_level_not_found << ",\n";
    out << "    \"amount_mismatch\": " << dq.book_amount_mismatch << ",\n";
    out << "    \"negative_level_volume\": " << dq.book_negative_level_volume << ",\n";
    out << "    \"crossed_book\": " << dq.book_crossed << ",\n";
    out << "    \"invalid_side\": " << dq.book_invalid_side << "\n";
    out << "  },\n";
    out << "  \"l2_export_diagnostics\": {\n";
    out << "    \"crossed_book_snapshots\": " << dq.l2_crossed_book_snapshots << ",\n";
    out << "    \"non_positive_spread_snapshots\": " << dq.l2_non_positive_spread_snapshots << ",\n";
    out << "    \"empty_bid_snapshots\": " << dq.l2_empty_bid_snapshots << ",\n";
    out << "    \"empty_ask_snapshots\": " << dq.l2_empty_ask_snapshots << "\n";
    out << "  },\n";
    write_json_string_array(out, "warnings", dq.warnings, 1);
    out << ",\n";
    write_json_string_array(out, "errors", dq.errors, 1);
    out << "\n}\n";

    return out.good();
}

bool write_metadata_json(const std::string& path, const DataQuality& dq) {
    std::ofstream out(path);
    if (!out.is_open()) return false;

    out << "{\n";
    out << "  \"source_file\": \"" << json_escape(dq.source_file_name) << "\",\n";
    out << "  \"sha256\": \"" << json_escape(dq.source_file_sha256) << "\",\n";
    out << "  \"stream_type\": \"" << json_escape(dq.stream_type) << "\",\n";
    out << "  \"instrument\": \"" << json_escape(dq.instrument) << "\",\n";
    out << "  \"recording_time\": \"" << json_escape(dq.recording_time) << "\",\n";
    out << "  \"format\": \"QSH v4\",\n";
    out << "  \"engine\": \"qsh_ingest C++20\",\n";
    out << "  \"status\": \"" << (dq.errors.empty() ? "OK" : "WARNINGS") << "\"\n";
    out << "}\n";

    return out.good();
}

void print_data_quality_summary(const DataQuality& dq) {
    std::cout << "=== Data Quality Report ===" << std::endl;
    std::cout << "File:       " << dq.source_file_name << std::endl;
    std::cout << "SHA-256:    " << dq.source_file_sha256.substr(0, 16) << "..." << std::endl;
    std::cout << "Stream:     " << dq.stream_type << std::endl;
    std::cout << "Instrument: " << dq.instrument << std::endl;
    std::cout << "Recorded:   " << dq.recording_time << std::endl;
    std::cout << std::endl;
    std::cout << "Records total:    " << dq.records_total << std::endl;
    std::cout << "Records valid:    " << dq.records_parsed << std::endl;
    std::cout << "Records rejected: " << dq.records_failed << std::endl;
    std::cout << std::endl;

    if (dq.stream_type == "OrderLog") {
        std::cout << "--- OrdLog counters ---" << std::endl;
        std::cout << "Add:           " << dq.add_count << std::endl;
        std::cout << "Fill:          " << dq.fill_count << std::endl;
        std::cout << "Cancel:        " << dq.cancel_count << std::endl;
        std::cout << "Remove:        " << dq.remove_count << std::endl;
        std::cout << "NewSession:    " << dq.new_session_count << std::endl;
        std::cout << "TxEnd:         " << dq.tx_end_count << std::endl;
        std::cout << "Transactions:  " << dq.tx_count << std::endl;
        std::cout << "Max tx size:   " << dq.tx_max_size << std::endl;
        std::cout << "Buy:           " << dq.buy_count << std::endl;
        std::cout << "Sell:          " << dq.sell_count << std::endl;
        std::cout << "Unknown side:  " << dq.unknown_side_count << std::endl;
        std::cout << "Non-system:    " << dq.non_system_count << std::endl;
        std::cout << "NonZeroReplAct:" << dq.non_zero_repl_act_count << std::endl;
        std::cout << "Snapshot:      " << dq.snapshot_count << std::endl;
        std::cout << "Moved:         " << dq.moved_count << std::endl;
        std::cout << "CrossTrade:    " << dq.cross_trade_count << std::endl;
        std::cout << std::endl;
        std::cout << "--- Book reconstruction errors ---" << std::endl;
        std::cout << "missing_order_id:      " << dq.book_missing_order_id << std::endl;
        std::cout << "level_not_found:       " << dq.book_level_not_found << std::endl;
        std::cout << "amount_mismatch:       " << dq.book_amount_mismatch << std::endl;
        std::cout << "negative_level_volume: " << dq.book_negative_level_volume << std::endl;
        std::cout << "crossed_book:          " << dq.book_crossed << std::endl;
        std::cout << "invalid_side:          " << dq.book_invalid_side << std::endl;

        std::cout << std::endl;
        std::cout << "--- L2 export diagnostics ---" << std::endl;
        std::cout << "crossed_book_snapshots:         " << dq.l2_crossed_book_snapshots << std::endl;
        std::cout << "non_positive_spread_snapshots:  " << dq.l2_non_positive_spread_snapshots << std::endl;
        std::cout << "empty_bid_snapshots:            " << dq.l2_empty_bid_snapshots << std::endl;
        std::cout << "empty_ask_snapshots:            " << dq.l2_empty_ask_snapshots << std::endl;
    }

    if (!dq.warnings.empty()) {
        std::cout << "\n--- Warnings ---" << std::endl;
        for (const auto& w : dq.warnings) {
            std::cout << "  " << w << std::endl;
        }
    }
    if (!dq.errors.empty()) {
        std::cout << "\n--- Errors ---" << std::endl;
        for (const auto& e : dq.errors) {
            std::cout << "  " << e << std::endl;
        }
    }
}

}  // namespace qsh
