#include "qsh/qsh_reader.hpp"
#include "qsh/qsh_header.hpp"
#include "qsh/ordlog_reader.hpp"
#include "qsh/quotes_reader.hpp"
#include "qsh/deals_reader.hpp"
#include "qsh/auxinfo_reader.hpp"
#include "orderbook/order_book.hpp"
#include "orderbook/l2_snapshot.hpp"
#include "quality/data_quality.hpp"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <cstring>

using namespace qsh;

static void print_help() {
    std::cout << "qsh-ingest — QScalp History Data parser (C++20)\n\n";
    std::cout << "Commands:\n";
    std::cout << "  inspect <file.qsh>                    Read and display QSH header\n";
    std::cout << "  quality <file.qsh>                    Scan records and print counters\n";
    std::cout << "  convert <file.qsh> --out <dir>        Export normalized metadata\n";
    std::cout << "  l3-to-l2 <OrdLog.qsh> [--depth N] [--max-records N] [--max-snapshots N]\n";
    std::cout << "                        [--out <file.csv>] [--diagnostics-out <file.csv>]\n";
    std::cout << "                        [--max-diagnostics N]\n";
    std::cout << "                          L3->L2 reconstruction\n";
    std::cout << "\nOptions:\n";
    std::cout << "  --depth N              L2 depth (default: 20)\n";
    std::cout << "  --max-records N        Stop after N OrdLog records (for testing)\n";
    std::cout << "  --max-snapshots N      Stop after N L2 snapshots (for testing)\n";
    std::cout << "  --out <path>           Output file or directory\n";
    std::cout << "  --diagnostics-out <f>  Write L2 diagnostics CSV\n";
    std::cout << "  --max-diagnostics N    Max diagnostics entries (default: 100)\n";
    std::cout << "\nSafety:\n";
    std::cout << "  No broker connection. No live trading. Historical files only.\n";
}

static int cmd_inspect(const std::string& path) {
    auto file = open_qsh_file(path);
    if (!file.valid) {
        std::cerr << "Error: " << file.error << std::endl;
        return 1;
    }

    auto& h = file.header;
    std::cout << "=== QSH File Inspection ===" << std::endl;
    std::cout << "File:       " << path << std::endl;
    std::cout << "Version:    " << static_cast<int>(h.version) << std::endl;
    std::cout << "Stream:     " << stream_type_name(h.stream) << std::endl;
    std::cout << "Instrument: " << h.instrument << std::endl;
    std::cout << "Recorder:   " << h.recorder << std::endl;
    std::cout << "Comment:    " << h.comment << std::endl;
    std::cout << "Recorded:   " << dotnet_ticks_to_string(h.recording_time) << std::endl;

    return 0;
}

static int cmd_quality(const std::string& path) {
    auto file = open_qsh_file(path);
    if (!file.valid) {
        std::cerr << "Error: " << file.error << std::endl;
        return 1;
    }

    DataQuality dq;
    dq.source_file_name = std::filesystem::path(path).filename().string();
    dq.source_file_sha256 = file_sha256_hex(path);
    dq.stream_type = stream_type_name(file.header.stream);
    dq.instrument = file.header.instrument;
    dq.recording_time = dotnet_ticks_to_string(file.header.recording_time);

    if (file.header.stream == StreamType::OrderLog) {
        OrdLogReader reader;
        int64_t tx_size = 0;

        reader.scan_all(file, [&](const OrderLogRecord& rec) {
            ++dq.records_total;
            ++dq.records_parsed;

            // Timestamps
            if (dq.first_timestamp == 0 || rec.timestamp < dq.first_timestamp) {
                dq.first_timestamp = rec.timestamp;
            }
            if (rec.timestamp > dq.last_timestamp) {
                dq.last_timestamp = rec.timestamp;
            }

            // Counters
            if (has_flag(rec.order_flags, OLFlags::Add))       ++dq.add_count;
            if (has_flag(rec.order_flags, OLFlags::Fill))      ++dq.fill_count;
            if (has_flag(rec.order_flags, OLFlags::Canceled))  ++dq.cancel_count;
            if (has_flag(rec.order_flags, OLFlags::CanceledGroup)) ++dq.cancel_count;
            if (has_flag(rec.order_flags, OLFlags::NewSession)) ++dq.new_session_count;
            if (has_flag(rec.order_flags, OLFlags::TxEnd))     ++dq.tx_end_count;
            if (has_flag(rec.order_flags, OLFlags::Snapshot))  ++dq.snapshot_count;
            if (has_flag(rec.order_flags, OLFlags::Moved))     ++dq.moved_count;
            if (has_flag(rec.order_flags, OLFlags::CrossTrade)) ++dq.cross_trade_count;
            if (has_flag(rec.order_flags, OLFlags::NonSystem)) ++dq.non_system_count;
            if (has_flag(rec.order_flags, OLFlags::NonZeroReplAct)) ++dq.non_zero_repl_act_count;

            if (rec.side == Side::Buy)     ++dq.buy_count;
            if (rec.side == Side::Sell)    ++dq.sell_count;
            if (rec.side == Side::Unknown) ++dq.unknown_side_count;

            // TxEnd grouping
            ++tx_size;
            if (has_flag(rec.order_flags, OLFlags::TxEnd)) {
                ++dq.tx_count;
                if (tx_size > dq.tx_max_size) dq.tx_max_size = tx_size;
                tx_size = 0;
            }

            // Remove by cross-trade or amount_rest == 0 (not via Cancel)
            if (!has_flag(rec.order_flags, OLFlags::Add) &&
                !has_flag(rec.order_flags, OLFlags::Fill) &&
                !has_flag(rec.order_flags, OLFlags::Canceled) &&
                !has_flag(rec.order_flags, OLFlags::CanceledGroup) &&
                (has_flag(rec.order_flags, OLFlags::CrossTrade) || rec.amount_rest == 0)) {
                // This is a Remove event
            }
        });
    } else if (file.header.stream == StreamType::Quotes) {
        QuotesReader reader;
        reader.scan_all(file, [&](const QuotesRecord& rec) {
            ++dq.records_total;
            ++dq.records_parsed;
            if (dq.first_timestamp == 0 || rec.frame_time_delta < dq.first_timestamp) {
                dq.first_timestamp = rec.frame_time_delta;
            }
        });
    } else if (file.header.stream == StreamType::Deals) {
        DealsReader reader;
        reader.scan_all(file, [&](const DealRecord& rec) {
            ++dq.records_total;
            ++dq.records_parsed;
            if (dq.first_timestamp == 0 || rec.timestamp < dq.first_timestamp) {
                dq.first_timestamp = rec.timestamp;
            }
            if (rec.timestamp > dq.last_timestamp) {
                dq.last_timestamp = rec.timestamp;
            }
        });
    } else if (file.header.stream == StreamType::AuxInfo) {
        AuxInfoReader reader;
        reader.scan_all(file, [&](const AuxInfoRecord& rec) {
            ++dq.records_total;
            ++dq.records_parsed;
            if (dq.first_timestamp == 0 || rec.timestamp < dq.first_timestamp) {
                dq.first_timestamp = rec.timestamp;
            }
            if (rec.timestamp > dq.last_timestamp) {
                dq.last_timestamp = rec.timestamp;
            }
        });
    }

    print_data_quality_summary(dq);

    // Write JSON output
    std::string dq_path = std::filesystem::path(path).stem().string() + "_data_quality.json";
    std::string meta_path = std::filesystem::path(path).stem().string() + "_metadata.json";
    if (write_data_quality_json(dq_path, dq)) {
        std::cout << "\nData quality written to: " << dq_path << std::endl;
    }
    if (write_metadata_json(meta_path, dq)) {
        std::cout << "Metadata written to:     " << meta_path << std::endl;
    }

    return 0;
}

static int cmd_convert(const std::string& path, const std::string& out_dir) {
    auto file = open_qsh_file(path);
    if (!file.valid) {
        std::cerr << "Error: " << file.error << std::endl;
        return 1;
    }

    std::filesystem::create_directories(out_dir);

    // Write header info
    std::string header_path = out_dir + "/header.json";
    DataQuality dq;
    dq.source_file_name = std::filesystem::path(path).filename().string();
    dq.source_file_sha256 = file_sha256_hex(path);
    dq.stream_type = stream_type_name(file.header.stream);
    dq.instrument = file.header.instrument;
    dq.recording_time = dotnet_ticks_to_string(file.header.recording_time);

    if (file.header.stream == StreamType::OrderLog) {
        OrdLogReader reader;
        std::string csv_path = out_dir + "/ordlog_events.csv";
        std::ofstream csv(csv_path);
        csv << "timestamp,order_id,event,side,price,amount,amount_rest,deal_id,deal_price\n";

        reader.scan_all(file, [&](const OrderLogRecord& rec) {
            ++dq.records_total;
            ++dq.records_parsed;
            csv << rec.timestamp << ","
                << rec.order_id << ","
                << ol_msg_type_name(rec.event) << ","
                << side_name(rec.side) << ","
                << rec.price << ","
                << rec.amount << ","
                << rec.amount_rest << ","
                << rec.deal_id << ","
                << rec.deal_price << "\n";
        });
        csv.close();
        std::cout << "Exported " << dq.records_parsed << " OrdLog events to " << csv_path << std::endl;
    }

    write_metadata_json(header_path, dq);
    std::cout << "Metadata written to: " << header_path << std::endl;

    return 0;
}

static int cmd_l3_to_l2(const std::string& path, int depth, int64_t max_records, int64_t max_snapshots,
                        const std::string& out_path, const std::string& diag_path, int64_t max_diagnostics) {
    auto file = open_qsh_file(path);
    if (!file.valid) {
        std::cerr << "Error: " << file.error << std::endl;
        return 1;
    }

    if (file.header.stream != StreamType::OrderLog) {
        std::cerr << "Error: l3-to-l2 requires OrdLog stream, got "
                  << stream_type_name(file.header.stream) << std::endl;
        return 1;
    }

    std::cout << "Reconstructing L2 (depth=" << depth;
    if (max_records > 0)  std::cout << ", max_records=" << max_records;
    if (max_snapshots > 0) std::cout << ", max_snapshots=" << max_snapshots;
    std::cout << ") from OrdLog..." << std::endl;

    OrderBook book;
    OrdLogReader reader;
    std::vector<L2SnapshotEntry> snapshots;
    std::vector<L2DiagnosticEntry> diagnostics;
    int64_t record_count = 0;
    int64_t system_count = 0;
    int64_t snapshot_count = 0;

    // L2 diagnostics counters
    int64_t l2_crossed = 0;
    int64_t l2_non_positive_spread = 0;
    int64_t l2_empty_bid = 0;
    int64_t l2_empty_ask = 0;

    OrderLogRecord rec;
    while (reader.next(file, rec)) {
        ++record_count;

        if (!is_system_record(rec)) {
            if (max_records > 0 && record_count >= max_records) break;
            continue;
        }
        ++system_count;

        // NewSession clears the book
        if (has_flag(rec.order_flags, OLFlags::NewSession)) {
            book.clear();
            if (max_records > 0 && record_count >= max_records) break;
            continue;
        }

        book.apply(rec);

        // Take snapshot on TxEnd
        if (is_tx_end(rec) && book.bid_depth() > 0 && book.ask_depth() > 0) {
            L2SnapshotEntry entry;
            entry.timestamp = rec.timestamp;
            entry.levels = book.snapshot(depth);
            entry.mid = book.mid_price();
            entry.spread = book.spread();
            snapshots.push_back(entry);
            ++snapshot_count;

            // L2 diagnostics: check for invalid state
            Price bb = book.best_bid();
            Price ba = book.best_ask();
            Price sp = book.spread();
            bool has_diag = false;

            if (bb <= 0) {
                ++l2_empty_bid;
                if (max_diagnostics <= 0 || static_cast<int64_t>(diagnostics.size()) < max_diagnostics) {
                    L2DiagnosticEntry de;
                    de.timestamp = rec.timestamp;
                    de.reason = L2DiagReason::EmptyBid;
                    de.best_bid = bb;
                    de.best_ask = ba;
                    de.spread = sp;
                    de.bid_qty_1 = 0;
                    de.ask_qty_1 = entry.levels.empty() ? 0 : entry.levels[0].ask_qty;
                    de.snapshots_written = snapshot_count;
                    de.records_processed = record_count;
                    diagnostics.push_back(de);
                }
                has_diag = true;
            }
            if (ba <= 0) {
                ++l2_empty_ask;
                if (max_diagnostics <= 0 || static_cast<int64_t>(diagnostics.size()) < max_diagnostics) {
                    L2DiagnosticEntry de;
                    de.timestamp = rec.timestamp;
                    de.reason = L2DiagReason::EmptyAsk;
                    de.best_bid = bb;
                    de.best_ask = ba;
                    de.spread = sp;
                    de.bid_qty_1 = entry.levels.empty() ? 0 : entry.levels[0].bid_qty;
                    de.ask_qty_1 = 0;
                    de.snapshots_written = snapshot_count;
                    de.records_processed = record_count;
                    diagnostics.push_back(de);
                }
                has_diag = true;
            }
            if (bb > 0 && ba > 0 && bb >= ba) {
                ++l2_crossed;
                if (max_diagnostics <= 0 || static_cast<int64_t>(diagnostics.size()) < max_diagnostics) {
                    L2DiagnosticEntry de;
                    de.timestamp = rec.timestamp;
                    de.reason = L2DiagReason::CrossedBook;
                    de.best_bid = bb;
                    de.best_ask = ba;
                    de.spread = sp;
                    de.bid_qty_1 = entry.levels.empty() ? 0 : entry.levels[0].bid_qty;
                    de.ask_qty_1 = entry.levels.empty() ? 0 : entry.levels[0].ask_qty;
                    de.snapshots_written = snapshot_count;
                    de.records_processed = record_count;
                    diagnostics.push_back(de);
                }
                has_diag = true;
            }
            if (bb > 0 && ba > 0 && sp <= 0 && bb < ba) {
                // non-positive spread but not crossed (touching: bb == ba already caught above)
                // This case covers sp == 0 when bb == ba (already caught), so here sp < 0
                // Actually if bb < ba and sp <= 0, that means sp == 0 only if bb == ba.
                // So this branch is only for sp < 0 which can't happen if bb < ba.
                // Keep as safety net.
            }
            (void)has_diag;

            if (max_snapshots > 0 && snapshot_count >= max_snapshots) break;
        }

        if (max_records > 0 && record_count >= max_records) break;
    }

    std::cout << "Records processed: " << record_count << std::endl;
    std::cout << "System records:    " << system_count << std::endl;
    std::cout << "L2 snapshots:      " << snapshot_count << std::endl;
    std::cout << "Book errors:" << std::endl;
    auto& errs = book.errors();
    std::cout << "  missing_order_id:      " << errs.missing_order_id << std::endl;
    std::cout << "  level_not_found:       " << errs.level_not_found << std::endl;
    std::cout << "  amount_mismatch:       " << errs.amount_mismatch << std::endl;
    std::cout << "  negative_level_volume: " << errs.negative_level_volume << std::endl;
    std::cout << "  crossed_book:          " << errs.crossed_book_after_update << std::endl;
    std::cout << "  invalid_side:          " << errs.invalid_side << std::endl;

    // L2 diagnostics summary
    std::cout << "\nL2 export diagnostics:" << std::endl;
    std::cout << "  crossed_book_snapshots:         " << l2_crossed << std::endl;
    std::cout << "  non_positive_spread_snapshots:  " << l2_non_positive_spread << std::endl;
    std::cout << "  empty_bid_snapshots:            " << l2_empty_bid << std::endl;
    std::cout << "  empty_ask_snapshots:            " << l2_empty_ask << std::endl;

    if (l2_crossed > 0 || l2_non_positive_spread > 0 || l2_empty_bid > 0 || l2_empty_ask > 0) {
        std::cout << "\nWARNING: exported L2 contains invalid best bid / best ask state." << std::endl;
        std::cout << "This L2 output is not strategy-ready until reconstruction diagnostics are clean." << std::endl;
    }

    size_t written = write_l2_csv(out_path, snapshots, depth);
    std::cout << "\nWrote " << written << " snapshots to " << out_path << std::endl;

    // Write diagnostics CSV if requested
    if (!diag_path.empty()) {
        size_t diag_written = write_l2_diagnostics_csv(diag_path, diagnostics);
        std::cout << "Wrote " << diag_written << " diagnostics to " << diag_path << std::endl;
    }

    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_help();
        return 0;
    }

    std::string cmd = argv[1];

    if (cmd == "--help" || cmd == "-h" || cmd == "help") {
        print_help();
        return 0;
    }

    if (cmd == "inspect") {
        if (argc < 3) {
            std::cerr << "Usage: qsh-ingest inspect <file.qsh>" << std::endl;
            return 1;
        }
        return cmd_inspect(argv[2]);
    }

    if (cmd == "quality") {
        if (argc < 3) {
            std::cerr << "Usage: qsh-ingest quality <file.qsh>" << std::endl;
            return 1;
        }
        return cmd_quality(argv[2]);
    }

    if (cmd == "convert") {
        if (argc < 3) {
            std::cerr << "Usage: qsh-ingest convert <file.qsh> --out <dir>" << std::endl;
            return 1;
        }
        std::string file_path = argv[2];
        std::string out_dir = "output";
        for (int i = 3; i < argc - 1; ++i) {
            if (std::strcmp(argv[i], "--out") == 0) {
                out_dir = argv[i + 1];
                break;
            }
        }
        return cmd_convert(file_path, out_dir);
    }

    if (cmd == "l3-to-l2") {
        if (argc < 3) {
            std::cerr << "Usage: qsh-ingest l3-to-l2 <OrdLog.qsh> [--depth N] [--max-records N]\n"
                      << "  [--max-snapshots N] [--out <file.csv>] [--diagnostics-out <file.csv>]\n"
                      << "  [--max-diagnostics N]" << std::endl;
            return 1;
        }
        std::string file_path = argv[2];
        int depth = 20;
        int64_t max_records = 0;
        int64_t max_snapshots = 0;
        int64_t max_diagnostics = 100;
        std::string out_path = "l2_snapshots.csv";
        std::string diag_path;
        for (int i = 3; i < argc - 1; ++i) {
            if (std::strcmp(argv[i], "--depth") == 0 && i + 1 < argc) {
                depth = std::atoi(argv[i + 1]);
            }
            if (std::strcmp(argv[i], "--max-records") == 0 && i + 1 < argc) {
                max_records = std::atoll(argv[i + 1]);
            }
            if (std::strcmp(argv[i], "--max-snapshots") == 0 && i + 1 < argc) {
                max_snapshots = std::atoll(argv[i + 1]);
            }
            if (std::strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
                out_path = argv[i + 1];
            }
            if (std::strcmp(argv[i], "--diagnostics-out") == 0 && i + 1 < argc) {
                diag_path = argv[i + 1];
            }
            if (std::strcmp(argv[i], "--max-diagnostics") == 0 && i + 1 < argc) {
                max_diagnostics = std::atoll(argv[i + 1]);
            }
        }
        return cmd_l3_to_l2(file_path, depth, max_records, max_snapshots, out_path, diag_path, max_diagnostics);
    }

    std::cerr << "Unknown command: " << cmd << std::endl;
    print_help();
    return 1;
}
