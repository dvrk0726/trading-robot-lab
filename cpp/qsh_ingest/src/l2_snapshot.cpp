#include "orderbook/l2_snapshot.hpp"
#include <fstream>
#include <sstream>

namespace qsh {

static std::string l2_diag_csv_header() {
    return "ts,reason,best_bid,best_ask,spread,bid_qty_1,ask_qty_1,snapshots_written,records_processed";
}

size_t write_l2_diagnostics_csv(const std::string& path, const std::vector<L2DiagnosticEntry>& entries) {
    std::ofstream out(path);
    if (!out.is_open()) return 0;

    out << l2_diag_csv_header() << "\n";
    for (const auto& e : entries) {
        out << e.timestamp << ","
            << l2_diag_reason_name(e.reason) << ","
            << e.best_bid << ","
            << e.best_ask << ","
            << e.spread << ","
            << e.bid_qty_1 << ","
            << e.ask_qty_1 << ","
            << e.snapshots_written << ","
            << e.records_processed << "\n";
    }
    return entries.size();
}

std::string l2_csv_header(int depth) {
    std::ostringstream oss;
    oss << "ts";
    for (int i = 1; i <= depth; ++i) {
        oss << ",bid_price_" << i << ",bid_qty_" << i;
    }
    for (int i = 1; i <= depth; ++i) {
        oss << ",ask_price_" << i << ",ask_qty_" << i;
    }
    oss << ",mid,spread";
    return oss.str();
}

size_t write_l2_csv(const std::string& path, const std::vector<L2SnapshotEntry>& snapshots, int depth) {
    std::ofstream out(path);
    if (!out.is_open()) return 0;

    out << l2_csv_header(depth) << "\n";

    for (const auto& snap : snapshots) {
        out << snap.timestamp;
        for (int i = 0; i < depth; ++i) {
            if (i < static_cast<int>(snap.levels.size())) {
                out << "," << snap.levels[i].bid_price << "," << snap.levels[i].bid_qty;
            } else {
                out << ",0,0";
            }
        }
        for (int i = 0; i < depth; ++i) {
            if (i < static_cast<int>(snap.levels.size())) {
                out << "," << snap.levels[i].ask_price << "," << snap.levels[i].ask_qty;
            } else {
                out << ",0,0";
            }
        }
        out << "," << snap.mid << "," << snap.spread << "\n";
    }

    return snapshots.size();
}

}  // namespace qsh
