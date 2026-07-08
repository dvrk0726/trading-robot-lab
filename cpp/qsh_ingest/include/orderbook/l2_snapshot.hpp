#pragma once

#include "qsh/qsh_types.hpp"
#include "orderbook/order_book.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace qsh {

struct L2SnapshotEntry {
    Timestamp timestamp = 0;
    std::vector<L2SnapshotRow> levels;
    double mid = 0.0;
    Price spread = 0;
};

// Write L2 snapshots to CSV file.
// Returns number of snapshots written.
size_t write_l2_csv(const std::string& path, const std::vector<L2SnapshotEntry>& snapshots, int depth);

// Generate CSV header for L2 snapshots.
std::string l2_csv_header(int depth);

}  // namespace qsh
