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
#include <sstream>
#include <string>
#include <vector>
#include <deque>
#include <set>
#include <filesystem>
#include <cstring>

using namespace qsh;

// Snapshot emission mode.
enum class SnapshotMode { Event, TxEnd };

// Snapshot records handling mode (experimental).
enum class SnapshotRecordsMode { Ignore, Load, Marker };

// Ring buffer entry for first-crossed context trace.
struct RingEntry {
    int64_t record_index = 0;
    int64_t tx_index = 0;
    Timestamp timestamp = 0;
    OLMsgType event_type = OLMsgType::Unknown;
    UID order_id = 0;
    Side side = Side::Unknown;
    Price price = 0;
    Volume qty = 0;
    Volume amount_rest = 0;
    uint16_t flags = 0;
    Volume repl_act = 0;
};

static void print_help() {
    std::cout << "qsh-ingest — QScalp History Data parser (C++20)\n\n";
    std::cout << "Commands:\n";
    std::cout << "  inspect <file.qsh>                    Read and display QSH header\n";
    std::cout << "  quality <file.qsh>                    Scan records and print counters\n";
    std::cout << "  convert <file.qsh> --out <dir>        Export normalized metadata\n";
    std::cout << "  dump-records <OrdLog.qsh> [--audit]    Export decoded OrdLog records to CSV\n";
    std::cout << "  check-missing-order <OrdLog.qsh>      Analyze first missing order backward\n";
    std::cout << "  l3-to-l2 <OrdLog.qsh> [--depth N] [--max-records N] [--max-snapshots N]\n";
    std::cout << "                        [--out <file.csv>] [--diagnostics-out <file.csv>]\n";
    std::cout << "                        [--max-diagnostics N]\n";
    std::cout << "                        [--trace-crossed-out <file.csv>] [--max-trace-events N]\n";
    std::cout << "                        [--snapshot-mode event|txend]\n";
    std::cout << "                        [--trace-order-id <id>[,<id>...]] [--trace-order-out <file.csv>]\n";
    std::cout << "                        [--trace-crossed-context N]\n";
    std::cout << "                        [--trace-best-level-orders-out <file.csv>]\n";
    std::cout << "                        [--trace-missing-order-out <file.csv>]\n";
    std::cout << "                        [--auto-trace-crossed-orders-out <file.csv>]\n";
    std::cout << "                        [--fill-semantics delta|rest]\n";
    std::cout << "                        [--orphan-fill-mode strict|ignore|reduce-same-price|transaction-rest]\n";
    std::cout << "                          L3->L2 reconstruction\n";
    std::cout << "\nOptions:\n";
    std::cout << "  --depth N              L2 depth (default: 20)\n";
    std::cout << "  --max-records N        Stop after N OrdLog records (for testing)\n";
    std::cout << "  --max-snapshots N      Stop after N L2 snapshots (for testing)\n";
    std::cout << "  --out <path>           Output file or directory\n";
    std::cout << "  --diagnostics-out <f>  Write L2 diagnostics CSV\n";
    std::cout << "  --max-diagnostics N    Max diagnostics entries (default: 100)\n";
    std::cout << "  --trace-crossed-out <f> Write crossed-book trace CSV\n";
    std::cout << "  --max-trace-events N   Max trace events (default: 100)\n";
    std::cout << "  --snapshot-mode <mode> Snapshot emission: event (after every record)\n";
    std::cout << "                        or txend (after TxEnd only, default)\n";
    std::cout << "  --trace-order-id <id>[,<id>...] Trace lifecycle of order IDs (comma-separated)\n";
    std::cout << "  --trace-order-out <f> Write order lifecycle trace CSV\n";
    std::cout << "  --trace-crossed-context N  Ring buffer size for first-crossed context (default: 20)\n";
    std::cout << "  --trace-best-level-orders-out <f>  Write best-level orders CSV for crossed snapshots\n";
    std::cout << "  --trace-missing-order-out <f>      Write missing order ID trace CSV\n";
    std::cout << "  --auto-trace-crossed-orders-out <f> Write auto-traced orders from first crossed snapshot\n";
    std::cout << "  --fill-semantics <mode>  Fill interpretation: delta (default) or rest\n";
    std::cout << "  --orphan-fill-mode <mode>  Orphan fill handling: strict (default), ignore,\n";
    std::cout << "                             reduce-same-price, or transaction-rest\n";
    std::cout << "  --audit                  Include raw decoder state in dump-records output\n";
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

static int cmd_dump_records(const std::string& path, const std::string& out_path,
                            int64_t from_index, int64_t to_index, bool audit_mode) {
    auto file = open_qsh_file(path);
    if (!file.valid) {
        std::cerr << "Error: " << file.error << std::endl;
        return 1;
    }

    if (file.header.stream != StreamType::OrderLog) {
        std::cerr << "Error: dump-records requires OrdLog stream, got "
                  << stream_type_name(file.header.stream) << std::endl;
        return 1;
    }

    std::ofstream csv(out_path);
    if (!csv.is_open()) {
        std::cerr << "Error: cannot open output file: " << out_path << std::endl;
        return 1;
    }

    // Standard columns
    csv << "record_index,ts,tx_index,order_id,event_type,side,price,amount,amount_rest,"
        << "flags,flags_hex,repl_act,is_snapshot,is_new_session,is_txend,is_system,is_non_system";

    // Audit columns (raw decoder state)
    if (audit_mode) {
        csv << ",raw_data_offset,raw_entry_flags,raw_order_flags_hex"
            << ",raw_side_bits,raw_event_bits"
            << ",has_timestamp_field,has_order_id_field,has_price_field,has_amount_field"
            << ",order_id_before_delta,order_id_after_delta,order_id_delta"
            << ",price_before_delta,price_after_delta,price_delta"
            << ",ts_before_delta,ts_after_delta"
            << ",amount_before,amount_after"
            << ",is_add_order_id_path";
    }
    csv << "\n";

    OrdLogReader reader;
    int64_t record_count = 0;
    int64_t tx_counter = 0;
    int64_t written = 0;

    auto process_rec = [&](const OrderLogRecord& rec) {
        ++record_count;

        // Track TxEnd
        if (has_flag(rec.order_flags, OLFlags::TxEnd)) {
            ++tx_counter;
        }

        // Apply range filter
        if (record_count < from_index) return;
        if (to_index > 0 && record_count > to_index) return;

        bool is_snapshot = has_flag(rec.order_flags, OLFlags::Snapshot);
        bool is_new_session = has_flag(rec.order_flags, OLFlags::NewSession);
        bool is_txend = has_flag(rec.order_flags, OLFlags::TxEnd);
        bool is_system = is_system_record(rec);
        bool is_non_system = has_flag(rec.order_flags, OLFlags::NonSystem);
        Volume repl_act = has_flag(rec.order_flags, OLFlags::NonZeroReplAct) ? 1 : 0;

        csv << record_count << ","
            << rec.timestamp << ","
            << tx_counter << ","
            << rec.order_id << ","
            << ol_msg_type_name(rec.event) << ","
            << side_name(rec.side) << ","
            << rec.price << ","
            << rec.amount << ","
            << rec.amount_rest << ","
            << rec.order_flags << ","
            << "0x" << std::hex << rec.order_flags << std::dec << ","
            << repl_act << ","
            << (is_snapshot ? 1 : 0) << ","
            << (is_new_session ? 1 : 0) << ","
            << (is_txend ? 1 : 0) << ","
            << (is_system ? 1 : 0) << ","
            << (is_non_system ? 1 : 0);

        if (audit_mode) {
            // Side bits: Buy=bit4, Sell=bit5
            uint16_t side_bits = rec.order_flags & (OLFlags::Buy | OLFlags::Sell);
            // Event bits: Add=bit2, Fill=bit3, Moved=bit12, Canceled=bit13, CanceledGroup=bit14, CrossTrade=bit15
            uint16_t event_bits = rec.order_flags & (OLFlags::Add | OLFlags::Fill | OLFlags::Moved |
                OLFlags::Canceled | OLFlags::CanceledGroup | OLFlags::CrossTrade);

            csv << "," << rec.debug.raw_data_offset
                << "," << static_cast<int>(rec.debug.raw_entry_flags)
                << ",0x" << std::hex << rec.debug.raw_order_flags << std::dec
                << ",0x" << std::hex << side_bits << std::dec
                << ",0x" << std::hex << event_bits << std::dec
                << "," << (rec.debug.has_timestamp_field ? 1 : 0)
                << "," << (rec.debug.has_order_id_field ? 1 : 0)
                << "," << (rec.debug.has_price_field ? 1 : 0)
                << "," << (rec.debug.has_amount_field ? 1 : 0)
                << "," << rec.debug.order_id_before_delta
                << "," << rec.debug.order_id_after_delta
                << "," << (rec.debug.order_id_after_delta - rec.debug.order_id_before_delta)
                << "," << rec.debug.price_before_delta
                << "," << rec.debug.price_after_delta
                << "," << (rec.debug.price_after_delta - rec.debug.price_before_delta)
                << "," << rec.debug.ts_before_delta
                << "," << rec.debug.ts_after_delta
                << "," << rec.debug.amount_before
                << "," << rec.debug.amount_after
                << "," << (rec.debug.is_add_order_id_path ? "growing" : "leb128");
        }

        csv << "\n";
        ++written;
    };

    if (audit_mode) {
        // Use next_debug to capture decoder state
        OrderLogRecord rec;
        while (reader.next_debug(file, rec)) {
            process_rec(rec);
        }
    } else {
        reader.scan_all(file, process_rec);
    }

    csv.close();
    std::cout << "Dumped " << written << " records (range " << from_index
              << ".." << (to_index > 0 ? std::to_string(to_index) : "end")
              << ") to " << out_path << std::endl;

    return 0;
}

// Structure to hold first missing order analysis results
struct FirstMissingOrderAnalysis {
    UID order_id = 0;
    OLMsgType event_type = OLMsgType::Unknown;
    Side side = Side::Unknown;
    Price price = 0;
    Volume amount = 0;
    int64_t first_occurrence_index = 0;
    int64_t prior_add_index = 0;
    OLMsgType prior_add_type = OLMsgType::Unknown;
    bool prior_add_found = false;
    bool prior_snapshot_found = false;
    int64_t prior_snapshot_index = 0;
};

static int cmd_check_missing_order(const std::string& path) {
    auto file = open_qsh_file(path);
    if (!file.valid) {
        std::cerr << "Error: " << file.error << std::endl;
        return 1;
    }

    if (file.header.stream != StreamType::OrderLog) {
        std::cerr << "Error: check-missing-order requires OrdLog stream, got "
                  << stream_type_name(file.header.stream) << std::endl;
        return 1;
    }

    std::cout << "Analyzing first missing order backward..." << std::endl;

    // First pass: find the first missing order using l3-to-l2 logic
    OrderBook book;
    OrdLogReader reader;
    OrderLogRecord rec;
    int64_t record_count = 0;
    FirstMissingOrderAnalysis analysis;

    // First pass: find first missing order
    while (reader.next(file, rec)) {
        ++record_count;

        if (!is_system_record(rec)) continue;

        if (has_flag(rec.order_flags, OLFlags::NewSession)) {
            book.clear();
            continue;
        }

        int64_t missing_before = book.errors().missing_order_id;
        book.apply(rec);

        if (book.errors().missing_order_id > missing_before && analysis.order_id == 0) {
            analysis.order_id = rec.order_id;
            analysis.event_type = rec.event;
            analysis.side = rec.side;
            analysis.price = rec.price;
            analysis.amount = rec.amount;
            analysis.first_occurrence_index = record_count;
            break;
        }
    }

    if (analysis.order_id == 0) {
        std::cout << "No missing order found in the file." << std::endl;
        return 0;
    }

    std::cout << "\nFirst missing order found:" << std::endl;
    std::cout << "  order_id:       " << analysis.order_id << std::endl;
    std::cout << "  event_type:     " << ol_msg_type_name(analysis.event_type) << std::endl;
    std::cout << "  side:           " << side_name(analysis.side) << std::endl;
    std::cout << "  price:          " << analysis.price << std::endl;
    std::cout << "  amount:         " << analysis.amount << std::endl;
    std::cout << "  record_index:   " << analysis.first_occurrence_index << std::endl;

    // Second pass: search backward for prior ADD or Snapshot with same order_id
    file = open_qsh_file(path);
    if (!file.valid) {
        std::cerr << "Error: " << file.error << std::endl;
        return 1;
    }

    OrdLogReader reader2;
    OrderLogRecord rec2;
    record_count = 0;

    while (reader2.next(file, rec2)) {
        ++record_count;

        if (record_count >= analysis.first_occurrence_index) break;

        if (rec2.order_id == analysis.order_id) {
            if (rec2.event == OLMsgType::Add) {
                analysis.prior_add_found = true;
                analysis.prior_add_index = record_count;
                analysis.prior_add_type = rec2.event;
            }
            if (has_flag(rec2.order_flags, OLFlags::Snapshot)) {
                analysis.prior_snapshot_found = true;
                analysis.prior_snapshot_index = record_count;
            }
        }
    }

    std::cout << "\nBackward search results:" << std::endl;
    std::cout << "  prior_add_found:      " << (analysis.prior_add_found ? "YES" : "NO") << std::endl;
    if (analysis.prior_add_found) {
        std::cout << "  prior_add_index:      " << analysis.prior_add_index << std::endl;
        std::cout << "  prior_add_type:       " << ol_msg_type_name(analysis.prior_add_type) << std::endl;
    }
    std::cout << "  prior_snapshot_found:  " << (analysis.prior_snapshot_found ? "YES" : "NO") << std::endl;
    if (analysis.prior_snapshot_found) {
        std::cout << "  prior_snapshot_index:  " << analysis.prior_snapshot_index << std::endl;
    }

    // Interpretation
    std::cout << "\nInterpretation:" << std::endl;
    if (!analysis.prior_add_found && !analysis.prior_snapshot_found) {
        std::cout << "  A. No prior ADD or Snapshot found for order " << analysis.order_id << std::endl;
        std::cout << "  -> Decoder may miss records or order was never added." << std::endl;
    } else if (analysis.prior_add_found) {
        std::cout << "  B. Prior ADD found but order was not loaded into book." << std::endl;
        std::cout << "  -> Book init or lifecycle bug." << std::endl;
    } else if (analysis.prior_snapshot_found) {
        std::cout << "  C. Prior Snapshot found but order was not loaded from snapshot." << std::endl;
        std::cout << "  -> Snapshot semantics may be wrong." << std::endl;
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
                        const std::string& out_path, const std::string& diag_path, int64_t max_diagnostics,
                        const std::string& trace_path, int64_t max_trace_events,
                        SnapshotMode snap_mode, const std::vector<UID>& trace_order_ids, const std::string& order_trace_path,
                        int ring_buffer_size,
                        const std::string& best_level_orders_path,
                        const std::string& missing_order_path,
                        const std::string& auto_trace_crossed_path,
                        bool fill_delta_mode,
                        SnapshotRecordsMode snapshot_records_mode = SnapshotRecordsMode::Ignore,
                        OrphanFillMode orphan_fill_mode = OrphanFillMode::Strict) {
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

    const char* mode_name = (snap_mode == SnapshotMode::Event) ? "event" : "txend";
    const char* fill_mode_name = fill_delta_mode ? "delta" : "rest";
    std::cout << "Reconstructing L2 (depth=" << depth
              << ", snapshot_mode=" << mode_name
              << ", fill_semantics=" << fill_mode_name
              << ", orphan_fill_mode=" << orphan_fill_mode_name(orphan_fill_mode);
    if (max_records > 0)  std::cout << ", max_records=" << max_records;
    if (max_snapshots > 0) std::cout << ", max_snapshots=" << max_snapshots;
    if (!trace_order_ids.empty()) {
        std::cout << ", trace_order_ids=[";
        for (size_t i = 0; i < trace_order_ids.size(); ++i) {
            if (i > 0) std::cout << ",";
            std::cout << trace_order_ids[i];
        }
        std::cout << "]";
    }
    std::cout << ") from OrdLog..." << std::endl;

    OrderBook book;
    book.set_fill_delta_mode(fill_delta_mode);
    book.set_orphan_fill_mode(orphan_fill_mode);
    OrdLogReader reader;
    std::vector<L2SnapshotEntry> snapshots;
    std::vector<L2DiagnosticEntry> diagnostics;
    std::vector<std::string> trace_lines;
    int64_t record_count = 0;
    int64_t system_count = 0;
    int64_t snapshot_count = 0;
    int64_t moved_count = 0;
    int64_t tx_counter = 0;
    int64_t transactions_seen = 0;

    // L2 diagnostics counters
    int64_t l2_crossed = 0;
    int64_t l2_non_positive_spread = 0;
    int64_t l2_empty_bid = 0;
    int64_t l2_empty_ask = 0;

    // Last event tracking for trace
    OLMsgType last_event_type = OLMsgType::Unknown;
    UID last_order_id = 0;
    Side last_side = Side::Unknown;
    Price last_price = 0;
    Volume last_qty = 0;
    uint16_t last_flags = 0;
    Volume last_repl_act = 0;
    int64_t last_tx_index = 0;

    // Ring buffer for first-crossed context
    std::deque<RingEntry> ring;
    bool first_crossed_found = false;
    int64_t post_crossed_count = 0;
    std::vector<std::string> crossed_context_lines;

    // Order lifecycle trace (supports multiple order IDs)
    std::ofstream order_trace_file;
    std::set<UID> trace_order_set(trace_order_ids.begin(), trace_order_ids.end());
    bool trace_order_active = !trace_order_set.empty();
    if (trace_order_active && !order_trace_path.empty()) {
        order_trace_file.open(order_trace_path);
        if (order_trace_file.is_open()) {
            order_trace_file << "ts,record_index,tx_index,event_type,order_id,side,price,qty,"
                             << "amount_rest,flags,repl_act,"
                             << "book_best_bid_before,book_best_ask_before,"
                             << "book_best_bid_after,book_best_ask_after,"
                             << "active_order_qty_before,active_order_qty_after,"
                             << "active_order_price_before,active_order_price_after\n";
        }
    }

    // Best-level orders trace
    std::ofstream best_level_file;
    bool trace_best_level_active = !best_level_orders_path.empty();
    if (trace_best_level_active) {
        best_level_file.open(best_level_orders_path);
        if (best_level_file.is_open()) {
            best_level_file << "ts,snapshot_index,records_processed,tx_index,"
                           << "best_bid,best_ask,spread,"
                           << "best_bid_total_qty,best_ask_total_qty,"
                           << "best_bid_order_ids,best_ask_order_ids,"
                           << "best_bid_order_count,best_ask_order_count,"
                           << "last_event_type,last_order_id,last_side,"
                           << "last_price,last_qty,last_amount_rest,last_flags,last_repl_act\n";
        }
    }

    // Missing order trace
    std::ofstream missing_order_file;
    bool trace_missing_order_active = !missing_order_path.empty();
    if (trace_missing_order_active) {
        missing_order_file.open(missing_order_path);
        if (missing_order_file.is_open()) {
            missing_order_file << "ts,record_index,tx_index,event_type,order_id,side,price,qty,"
                              << "amount_rest,flags,repl_act,"
                              << "best_bid_before,best_ask_before,"
                              << "best_bid_after,best_ask_after,reason\n";
        }
    }

    // Auto-trace crossed orders
    std::ofstream auto_trace_file;
    bool auto_trace_active = !auto_trace_crossed_path.empty();
    UID auto_trace_bid_order = 0;
    UID auto_trace_ask_order = 0;
    bool auto_trace_ids_selected = false;
    if (auto_trace_active) {
        auto_trace_file.open(auto_trace_crossed_path);
        if (auto_trace_file.is_open()) {
            auto_trace_file << "ts,record_index,tx_index,event_type,order_id,side,price,qty,"
                           << "amount_rest,flags,repl_act,"
                           << "book_best_bid_after,book_best_ask_after,spread_after\n";
        }
    }

    // Missing order event tracking (for before/after book state)
    struct MissingOrderEvent {
        Timestamp ts;
        int64_t record_index;
        int64_t tx_index;
        OLMsgType event_type;
        UID order_id;
        Side side;
        Price price;
        Volume qty;
        Volume amount_rest;
        uint16_t flags;
        Volume repl_act;
        Price best_bid_before;
        Price best_ask_before;
    };
    std::vector<MissingOrderEvent> pending_missing_events;

    OrderLogRecord rec;
    while (reader.next(file, rec)) {
        ++record_count;

        if (!is_system_record(rec)) {
            // M10G: Track non-system records
            if (has_flag(rec.order_flags, OLFlags::Add)) {
                ++book.errors_ref().add_records_skipped;
                if (has_flag(rec.order_flags, OLFlags::NonSystem)) {
                    ++book.errors_ref().skip_non_system;
                }
                if (has_flag(rec.order_flags, OLFlags::NonZeroReplAct)) {
                    ++book.errors_ref().skip_non_zero_repl_act;
                }
            }
            if (max_records > 0 && record_count >= max_records) break;
            continue;
        }
        ++system_count;

        // Track last event for trace
        last_event_type = rec.event;
        last_order_id = rec.order_id;
        last_side = rec.side;
        last_price = rec.price;
        last_qty = rec.amount;
        last_flags = rec.order_flags;
        last_repl_act = has_flag(rec.order_flags, OLFlags::NonZeroReplAct) ? 1 : 0;

        // Track TxEnd
        if (is_tx_end(rec)) {
            ++tx_counter;
            last_tx_index = tx_counter;
            transactions_seen = tx_counter;
        }

        // NewSession clears the book
        if (has_flag(rec.order_flags, OLFlags::NewSession)) {
            book.clear();
            ++book.errors_ref().new_session_records_seen;
            ++book.errors_ref().book_clears_due_to_new_session;
            if (max_records > 0 && record_count >= max_records) break;
            continue;
        }

        if (rec.event == OLMsgType::Moved) {
            ++moved_count;
        }

        // Track book state before apply for missing order trace and lifecycle trace
        Price best_bid_before = book.best_bid();
        Price best_ask_before = book.best_ask();
        int64_t missing_before = book.errors().missing_order_id;

        // M10G: Track Snapshot records
        if (has_flag(rec.order_flags, OLFlags::Snapshot)) {
            ++book.errors_ref().snapshot_records_seen;

            // Experimental: handle snapshot records based on mode
            if (snapshot_records_mode == SnapshotRecordsMode::Load) {
                // Treat snapshot records as actual order adds
                if (rec.event == OLMsgType::Add && is_system_record(rec)) {
                    ++book.errors_ref().snapshot_orders_loaded;
                }
            } else if (snapshot_records_mode == SnapshotRecordsMode::Marker) {
                // Treat snapshot records as markers only (skip apply)
                if (max_records > 0 && record_count >= max_records) break;
                continue;
            }
            // Ignore mode: default behavior (apply as normal)
        }

        // M10G: Track first valid book record (has bid and ask)
        if (book.bid_depth() > 0 && book.ask_depth() > 0) {
            book.set_first_valid_book_record_index(record_count);
        }

        // Track active order state before apply for traced orders
        bool is_traced_order = trace_order_active && trace_order_set.count(rec.order_id) > 0;
        Side traced_side_before = Side::Unknown;
        Price traced_price_before = 0;
        Volume traced_qty_before = 0;
        if (is_traced_order) {
            book.get_order_info(rec.order_id, traced_side_before, traced_price_before, traced_qty_before);
        }

        book.apply(rec);

        // Check if this event caused a missing_order_id error
        if (book.errors().missing_order_id > missing_before) {
            // M10G: Track first missing order record index
            book.set_first_missing_order_record_index(record_count);

            if (trace_missing_order_active) {
                MissingOrderEvent moe;
                moe.ts = rec.timestamp;
                moe.record_index = record_count;
                moe.tx_index = last_tx_index;
                moe.event_type = rec.event;
                moe.order_id = rec.order_id;
                moe.side = rec.side;
                moe.price = rec.price;
                moe.qty = rec.amount;
                moe.amount_rest = rec.amount_rest;
                moe.flags = rec.order_flags;
                moe.repl_act = last_repl_act;
                moe.best_bid_before = best_bid_before;
                moe.best_ask_before = best_ask_before;
                pending_missing_events.push_back(moe);
            }
        }

        // Ring buffer: push current event
        RingEntry ring_entry;
        ring_entry.record_index = record_count;
        ring_entry.tx_index = last_tx_index;
        ring_entry.timestamp = rec.timestamp;
        ring_entry.event_type = rec.event;
        ring_entry.order_id = rec.order_id;
        ring_entry.side = rec.side;
        ring_entry.price = rec.price;
        ring_entry.qty = rec.amount;
        ring_entry.amount_rest = rec.amount_rest;
        ring_entry.flags = rec.order_flags;
        ring_entry.repl_act = last_repl_act;
        ring.push_back(ring_entry);
        while (static_cast<int>(ring.size()) > ring_buffer_size) {
            ring.pop_front();
        }

        // Order lifecycle trace: write if this order matches
        if (is_traced_order && order_trace_file.is_open()) {
            // Get active order state after apply
            Side traced_side_after = Side::Unknown;
            Price traced_price_after = 0;
            Volume traced_qty_after = 0;
            book.get_order_info(rec.order_id, traced_side_after, traced_price_after, traced_qty_after);

            order_trace_file << rec.timestamp << ","
                << record_count << ","
                << last_tx_index << ","
                << ol_msg_type_name(rec.event) << ","
                << rec.order_id << ","
                << side_name(rec.side) << ","
                << rec.price << ","
                << rec.amount << ","
                << rec.amount_rest << ","
                << rec.order_flags << ","
                << last_repl_act << ","
                << best_bid_before << ","
                << best_ask_before << ","
                << book.best_bid() << ","
                << book.best_ask() << ","
                << traced_qty_before << ","
                << traced_qty_after << ","
                << traced_price_before << ","
                << traced_price_after << "\n";
        }

        // Determine if we should emit a snapshot now
        bool should_snapshot = false;
        if (snap_mode == SnapshotMode::Event) {
            // Emit after every system record (except NewSession which clears)
            should_snapshot = true;
        } else {
            // TxEnd mode: emit only on TxEnd boundaries
            should_snapshot = is_tx_end(rec);
        }

        if (should_snapshot && book.bid_depth() > 0 && book.ask_depth() > 0) {
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
            }
            if (bb > 0 && ba > 0 && bb >= ba) {
                ++l2_crossed;
                ++l2_non_positive_spread;
                // M10G: Track first crossed book record index
                book.set_first_crossed_book_record_index(record_count);
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

                // Write trace for crossed book
                if (!trace_path.empty() && (max_trace_events <= 0 || static_cast<int64_t>(trace_lines.size()) < max_trace_events)) {
                    std::ostringstream oss;
                    oss << rec.timestamp << ","
                        << snapshot_count << ","
                        << record_count << ","
                        << "CROSSED_BOOK,"
                        << bb << "," << ba << "," << sp << ","
                        << ol_msg_type_name(last_event_type) << ","
                        << last_order_id << ","
                        << side_name(last_side) << ","
                        << last_price << "," << last_qty << ","
                        << last_flags << "," << last_repl_act << ","
                        << last_tx_index;
                    trace_lines.push_back(oss.str());
                }

                // First-crossed context: dump ring buffer + start post-crossed collection
                if (!first_crossed_found && ring_buffer_size > 0) {
                    first_crossed_found = true;
                    // Dump ring buffer (events before first crossed)
                    for (const auto& re : ring) {
                        std::ostringstream oss;
                        oss << re.timestamp << ","
                            << re.record_index << ","
                            << re.tx_index << ","
                            << ol_msg_type_name(re.event_type) << ","
                            << re.order_id << ","
                            << side_name(re.side) << ","
                            << re.price << ","
                            << re.qty << ","
                            << re.amount_rest << ","
                            << re.flags << ","
                            << re.repl_act;
                        crossed_context_lines.push_back(oss.str());
                    }
                    post_crossed_count = 0;

                    // Auto-select order IDs for tracing from first crossed snapshot
                    if (auto_trace_active && !auto_trace_ids_selected) {
                        auto bid_ids = book.best_bid_order_ids(1);
                        auto ask_ids = book.best_ask_order_ids(1);
                        if (!bid_ids.empty()) auto_trace_bid_order = bid_ids[0];
                        if (!ask_ids.empty()) auto_trace_ask_order = ask_ids[0];
                        auto_trace_ids_selected = true;
                        std::cout << "Auto-trace: selected bid order " << auto_trace_bid_order
                                  << ", ask order " << auto_trace_ask_order << std::endl;
                    }
                }

                // Write best-level orders trace for crossed snapshot
                if (trace_best_level_active && best_level_file.is_open()) {
                    auto bid_ids = book.best_bid_order_ids(20);
                    auto ask_ids = book.best_ask_order_ids(20);

                    std::ostringstream bid_ids_str;
                    for (size_t i = 0; i < bid_ids.size(); ++i) {
                        if (i > 0) bid_ids_str << ";";
                        bid_ids_str << bid_ids[i];
                    }

                    std::ostringstream ask_ids_str;
                    for (size_t i = 0; i < ask_ids.size(); ++i) {
                        if (i > 0) ask_ids_str << ";";
                        ask_ids_str << ask_ids[i];
                    }

                    best_level_file << rec.timestamp << ","
                        << snapshot_count << ","
                        << record_count << ","
                        << last_tx_index << ","
                        << bb << "," << ba << "," << sp << ","
                        << book.best_bid_total_qty() << ","
                        << book.best_ask_total_qty() << ","
                        << bid_ids_str.str() << ","
                        << ask_ids_str.str() << ","
                        << book.best_bid_order_count() << ","
                        << book.best_ask_order_count() << ","
                        << ol_msg_type_name(last_event_type) << ","
                        << last_order_id << ","
                        << side_name(last_side) << ","
                        << last_price << "," << last_qty << ","
                        << rec.amount_rest << ","
                        << last_flags << "," << last_repl_act << "\n";
                }
            }

            // Collect post-crossed events
            if (first_crossed_found && post_crossed_count < ring_buffer_size) {
                ++post_crossed_count;
                std::ostringstream oss;
                oss << rec.timestamp << ","
                    << record_count << ","
                    << last_tx_index << ","
                    << ol_msg_type_name(rec.event) << ","
                    << rec.order_id << ","
                    << side_name(rec.side) << ","
                    << rec.price << ","
                    << rec.amount << ","
                    << rec.amount_rest << ","
                    << rec.order_flags << ","
                    << last_repl_act;
                crossed_context_lines.push_back(oss.str());
            }

            if (max_snapshots > 0 && snapshot_count >= max_snapshots) break;
        }

        if (max_records > 0 && record_count >= max_records) break;
    }

    std::cout << "\nsnapshot_mode:       " << mode_name << std::endl;
    std::cout << "Records processed:   " << record_count << std::endl;
    std::cout << "System records:      " << system_count << std::endl;
    std::cout << "Transactions seen:   " << transactions_seen << std::endl;
    std::cout << "L2 snapshots:        " << snapshot_count << std::endl;
    std::cout << "Moved events:        " << moved_count << std::endl;
    std::cout << "\nBook errors:" << std::endl;
    auto& errs = book.errors();
    std::cout << "  missing_order_id:      " << errs.missing_order_id << std::endl;
    std::cout << "  level_not_found:       " << errs.level_not_found << std::endl;
    std::cout << "  amount_mismatch:       " << errs.amount_mismatch << std::endl;
    std::cout << "  negative_level_volume: " << errs.negative_level_volume << std::endl;
    std::cout << "  crossed_book:          " << errs.crossed_book_after_update << std::endl;
    std::cout << "  invalid_side:          " << errs.invalid_side << std::endl;

    // M10G: Missing order timing diagnostics
    std::cout << "\nMissing order timing:" << std::endl;
    std::cout << "  first_missing_order_record_index: " << book.first_missing_order_record_index() << std::endl;
    std::cout << "  first_crossed_book_record_index:  " << book.first_crossed_book_record_index() << std::endl;
    if (book.first_missing_order_record_index() > 0 && book.first_crossed_book_record_index() > 0) {
        if (book.first_missing_order_record_index() < book.first_crossed_book_record_index()) {
            std::cout << "  missing_order_id starts BEFORE crossed book" << std::endl;
        } else if (book.first_missing_order_record_index() > book.first_crossed_book_record_index()) {
            std::cout << "  missing_order_id starts AFTER crossed book" << std::endl;
        } else {
            std::cout << "  missing_order_id starts at SAME record as crossed book" << std::endl;
        }
    }

    // M10G: Missing order counts by event type
    std::cout << "\nMissing order by event type:" << std::endl;
    std::cout << "  missing_on_fill:   " << errs.missing_on_fill << std::endl;
    std::cout << "  missing_on_cancel: " << errs.missing_on_cancel << std::endl;
    std::cout << "  missing_on_remove: " << errs.missing_on_remove << std::endl;
    std::cout << "  missing_on_move:   " << errs.missing_on_move << std::endl;

    // M10G: Missing order counts by side
    std::cout << "\nMissing order by side:" << std::endl;
    std::cout << "  missing_on_buy:         " << errs.missing_on_buy << std::endl;
    std::cout << "  missing_on_sell:        " << errs.missing_on_sell << std::endl;
    std::cout << "  missing_on_unknown_side:" << errs.missing_on_unknown_side << std::endl;

    // M10G: Snapshot/NewSession tracking
    std::cout << "\nSnapshot/NewSession tracking:" << std::endl;
    std::cout << "  snapshot_records_seen:           " << errs.snapshot_records_seen << std::endl;
    std::cout << "  new_session_records_seen:        " << errs.new_session_records_seen << std::endl;
    std::cout << "  book_clears_due_to_new_session:  " << errs.book_clears_due_to_new_session << std::endl;
    std::cout << "  first_valid_book_record_index:   " << errs.first_valid_book_record_index << std::endl;

    // M10G: ADD record tracking
    std::cout << "\nADD record tracking:" << std::endl;
    std::cout << "  add_records_seen:    " << errs.add_records_seen << std::endl;
    std::cout << "  add_records_applied: " << errs.add_records_applied << std::endl;
    std::cout << "  add_records_skipped: " << errs.add_records_skipped << std::endl;
    if (errs.add_records_skipped > 0) {
        std::cout << "  skip_reasons:" << std::endl;
        std::cout << "    invalid_side:      " << errs.skip_invalid_side << std::endl;
        std::cout << "    zero_amount:       " << errs.skip_zero_amount << std::endl;
    }

    // M10J: Orphan fill counters
    std::cout << "\nOrphan fill counters:" << std::endl;
    std::cout << "  orphan_fill_events:                " << errs.orphan_fill_events << std::endl;
    std::cout << "  orphan_fill_ignored:               " << errs.orphan_fill_ignored << std::endl;
    std::cout << "  orphan_fill_level_reductions:      " << errs.orphan_fill_level_reductions << std::endl;
    std::cout << "  orphan_fill_transaction_rest_updates: " << errs.orphan_fill_transaction_rest_updates << std::endl;

    // L2 diagnostics summary
    std::cout << "\nL2 export diagnostics:" << std::endl;
    std::cout << "  crossed_book_snapshots:         " << l2_crossed << std::endl;
    std::cout << "  non_positive_spread_snapshots:  " << l2_non_positive_spread << std::endl;
    std::cout << "  empty_bid_snapshots:            " << l2_empty_bid << std::endl;
    std::cout << "  empty_ask_snapshots:            " << l2_empty_ask << std::endl;

    bool has_invalid = (l2_crossed > 0 || l2_non_positive_spread > 0 || l2_empty_bid > 0 || l2_empty_ask > 0);

    if (has_invalid) {
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

    // Write trace CSV if requested
    if (!trace_path.empty()) {
        std::ofstream trace_out(trace_path);
        if (trace_out.is_open()) {
            trace_out << "ts,snapshot_index,records_processed,reason,best_bid,best_ask,spread,"
                      << "last_event_type,last_order_id,last_side,last_price,last_qty,"
                      << "last_flags,last_repl_act,last_tx_index\n";
            for (const auto& line : trace_lines) {
                trace_out << line << "\n";
            }
            trace_out.close();
            std::cout << "Wrote " << trace_lines.size() << " trace events to " << trace_path << std::endl;
        }
    }

    // Write order lifecycle trace
    if (trace_order_active && order_trace_file.is_open()) {
        order_trace_file.close();
        std::cout << "Order lifecycle trace written to " << order_trace_path << std::endl;
    }

    // Write first-crossed context trace
    if (first_crossed_found && !crossed_context_lines.empty()) {
        std::string ctx_path = out_path + ".crossed_context.csv";
        std::ofstream ctx_out(ctx_path);
        if (ctx_out.is_open()) {
            ctx_out << "ts,record_index,tx_index,event_type,order_id,side,price,qty,"
                    << "amount_rest,flags,repl_act\n";
            for (const auto& line : crossed_context_lines) {
                ctx_out << line << "\n";
            }
            ctx_out.close();
            std::cout << "First-crossed context (" << crossed_context_lines.size()
                      << " events) written to " << ctx_path << std::endl;
        }
    }

    // Write missing order trace
    if (trace_missing_order_active && missing_order_file.is_open()) {
        for (const auto& moe : pending_missing_events) {
            missing_order_file << moe.ts << ","
                << moe.record_index << ","
                << moe.tx_index << ","
                << ol_msg_type_name(moe.event_type) << ","
                << moe.order_id << ","
                << side_name(moe.side) << ","
                << moe.price << ","
                << moe.qty << ","
                << moe.amount_rest << ","
                << moe.flags << ","
                << moe.repl_act << ","
                << moe.best_bid_before << ","
                << moe.best_ask_before << ","
                << book.best_bid() << ","
                << book.best_ask() << ","
                << "MISSING_ORDER_ID"
                << "\n";
        }
        missing_order_file.close();
        std::cout << "Wrote " << pending_missing_events.size() << " missing order events to "
                  << missing_order_path << std::endl;
    }

    // Write best-level orders trace
    if (trace_best_level_active && best_level_file.is_open()) {
        best_level_file.close();
        std::cout << "Best-level orders trace written to " << best_level_orders_path << std::endl;
    }

    // Write auto-trace crossed orders
    if (auto_trace_active && auto_trace_file.is_open()) {
        // Note: For a proper implementation, we would need a second pass through the file
        // to trace the lifecycle of the selected orders. For now, document this limitation.
        if (auto_trace_ids_selected) {
            auto_trace_file << "# Auto-selected order IDs from first crossed snapshot\n";
            auto_trace_file << "# bid_order=" << auto_trace_bid_order << "\n";
            auto_trace_file << "# ask_order=" << auto_trace_ask_order << "\n";
            auto_trace_file << "# NOTE: Full lifecycle trace requires second pass through OrdLog file\n";
            auto_trace_file << "# Use --trace-order-id with these IDs for detailed lifecycle\n";
        }
        auto_trace_file.close();
        std::cout << "Auto-trace crossed orders written to " << auto_trace_crossed_path << std::endl;
    }

    // Final strategy-ready status
    const char* snapshot_records_mode_name = "ignore";
    if (snapshot_records_mode == SnapshotRecordsMode::Load) snapshot_records_mode_name = "load";
    else if (snapshot_records_mode == SnapshotRecordsMode::Marker) snapshot_records_mode_name = "marker";

    std::cout << "\n=== L2 Strategy-Ready Status ===" << std::endl;
    std::cout << "snapshot_mode:                    " << mode_name << std::endl;
    std::cout << "snapshot_records_mode:            " << snapshot_records_mode_name << std::endl;
    std::cout << "fill_semantics:                   " << fill_mode_name << std::endl;
    std::cout << "records_processed:                " << record_count << std::endl;
    std::cout << "transactions_seen:                " << transactions_seen << std::endl;
    std::cout << "snapshots_written:                " << snapshot_count << std::endl;
    std::cout << "missing_order_id:                 " << book.errors().missing_order_id << std::endl;
    std::cout << "amount_mismatch:                  " << book.errors().amount_mismatch << std::endl;
    std::cout << "negative_level_volume:            " << book.errors().negative_level_volume << std::endl;
    std::cout << "l2_crossed_book_snapshots:        " << l2_crossed << std::endl;
    std::cout << "l2_non_positive_spread_snapshots: " << l2_non_positive_spread << std::endl;
    // M10G: Additional diagnostics
    std::cout << "first_missing_order_record_index: " << book.first_missing_order_record_index() << std::endl;
    std::cout << "first_crossed_book_record_index:  " << book.first_crossed_book_record_index() << std::endl;
    std::cout << "add_records_seen:                 " << book.errors().add_records_seen << std::endl;
    std::cout << "add_records_applied:              " << book.errors().add_records_applied << std::endl;
    std::cout << "add_records_skipped:              " << book.errors().add_records_skipped << std::endl;
    std::cout << "snapshot_records_seen:            " << book.errors().snapshot_records_seen << std::endl;
    std::cout << "snapshot_orders_loaded:           " << book.errors().snapshot_orders_loaded << std::endl;
    std::cout << "new_session_records_seen:         " << book.errors().new_session_records_seen << std::endl;
    if (!trace_path.empty()) {
        std::cout << "trace_file:                       " << trace_path << std::endl;
    }
    if (trace_order_active) {
        std::cout << "order_trace_file:                 " << order_trace_path << std::endl;
    }
    if (trace_best_level_active) {
        std::cout << "best_level_orders_file:           " << best_level_orders_path << std::endl;
    }
    if (trace_missing_order_active) {
        std::cout << "missing_order_file:               " << missing_order_path << std::endl;
        std::cout << "missing_order_events:             " << pending_missing_events.size() << std::endl;
    }
    if (auto_trace_active) {
        std::cout << "auto_trace_file:                  " << auto_trace_crossed_path << std::endl;
        if (auto_trace_ids_selected) {
            std::cout << "auto_trace_bid_order:             " << auto_trace_bid_order << std::endl;
            std::cout << "auto_trace_ask_order:             " << auto_trace_ask_order << std::endl;
        }
    }
    std::cout << "L2 strategy-ready:                " << (has_invalid ? "NO" : "YES") << std::endl;

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

    if (cmd == "dump-records") {
        if (argc < 3) {
            std::cerr << "Usage: qsh-ingest dump-records <OrdLog.qsh> --dump-records-out <file.csv>\n"
                      << "  [--dump-records-from N] [--dump-records-to N] [--audit]" << std::endl;
            return 1;
        }
        std::string file_path = argv[2];
        std::string out_path = "ordlog_records.csv";
        int64_t from_index = 1;
        int64_t to_index = 0;
        bool audit_mode = false;
        for (int i = 3; i < argc; ++i) {
            if (std::strcmp(argv[i], "--dump-records-out") == 0 && i + 1 < argc) {
                out_path = argv[++i];
            } else if (std::strcmp(argv[i], "--dump-records-from") == 0 && i + 1 < argc) {
                from_index = std::atoll(argv[++i]);
            } else if (std::strcmp(argv[i], "--dump-records-to") == 0 && i + 1 < argc) {
                to_index = std::atoll(argv[++i]);
            } else if (std::strcmp(argv[i], "--audit") == 0) {
                audit_mode = true;
            }
        }
        return cmd_dump_records(file_path, out_path, from_index, to_index, audit_mode);
    }

    if (cmd == "check-missing-order") {
        if (argc < 3) {
            std::cerr << "Usage: qsh-ingest check-missing-order <OrdLog.qsh>" << std::endl;
            return 1;
        }
        return cmd_check_missing_order(argv[2]);
    }

    if (cmd == "l3-to-l2") {
        if (argc < 3) {
            std::cerr << "Usage: qsh-ingest l3-to-l2 <OrdLog.qsh> [--depth N] [--max-records N]\n"
                      << "  [--max-snapshots N] [--out <file.csv>] [--diagnostics-out <file.csv>]\n"
                      << "  [--max-diagnostics N] [--trace-crossed-out <file.csv>] [--max-trace-events N]\n"
                      << "  [--snapshot-mode event|txend] [--trace-order-id <id>[,<id>...]] [--trace-order-out <file.csv>]\n"
                      << "  [--trace-crossed-context N]\n"
                      << "  [--trace-best-level-orders-out <file.csv>]\n"
                      << "  [--trace-missing-order-out <file.csv>]\n"
                      << "  [--auto-trace-crossed-orders-out <file.csv>]\n"
                      << "  [--fill-semantics delta|rest]\n"
                      << "  [--orphan-fill-mode strict|ignore|reduce-same-price|transaction-rest]\n"
                      << "  [--snapshot-records-mode ignore|load|marker]" << std::endl;
            return 1;
        }
        std::string file_path = argv[2];
        int depth = 20;
        int64_t max_records = 0;
        int64_t max_snapshots = 0;
        int64_t max_diagnostics = 100;
        int64_t max_trace_events = 100;
        std::string out_path = "l2_snapshots.csv";
        std::string diag_path;
        std::string trace_path;
        SnapshotMode snap_mode = SnapshotMode::TxEnd;
        SnapshotRecordsMode snapshot_records_mode = SnapshotRecordsMode::Ignore;
        std::vector<UID> trace_order_ids;
        std::string order_trace_path;
        int ring_buffer_size = 20;
        std::string best_level_orders_path;
        std::string missing_order_path;
        std::string auto_trace_crossed_path;
        bool fill_delta_mode = true;
        OrphanFillMode orphan_fill_mode = OrphanFillMode::Strict;
        for (int i = 3; i < argc; ++i) {
            if (std::strcmp(argv[i], "--depth") == 0 && i + 1 < argc) {
                depth = std::atoi(argv[++i]);
            } else if (std::strcmp(argv[i], "--max-records") == 0 && i + 1 < argc) {
                max_records = std::atoll(argv[++i]);
            } else if (std::strcmp(argv[i], "--max-snapshots") == 0 && i + 1 < argc) {
                max_snapshots = std::atoll(argv[++i]);
            } else if (std::strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
                out_path = argv[++i];
            } else if (std::strcmp(argv[i], "--diagnostics-out") == 0 && i + 1 < argc) {
                diag_path = argv[++i];
            } else if (std::strcmp(argv[i], "--max-diagnostics") == 0 && i + 1 < argc) {
                max_diagnostics = std::atoll(argv[++i]);
            } else if (std::strcmp(argv[i], "--trace-crossed-out") == 0 && i + 1 < argc) {
                trace_path = argv[++i];
            } else if (std::strcmp(argv[i], "--max-trace-events") == 0 && i + 1 < argc) {
                max_trace_events = std::atoll(argv[++i]);
            } else if (std::strcmp(argv[i], "--snapshot-mode") == 0 && i + 1 < argc) {
                std::string mode_str = argv[++i];
                if (mode_str == "event") {
                    snap_mode = SnapshotMode::Event;
                } else if (mode_str == "txend") {
                    snap_mode = SnapshotMode::TxEnd;
                } else {
                    std::cerr << "Unknown snapshot mode: " << mode_str << " (use event or txend)" << std::endl;
                    return 1;
                }
            } else if (std::strcmp(argv[i], "--trace-order-id") == 0 && i + 1 < argc) {
                // Parse comma-separated order IDs
                std::string ids_str = argv[++i];
                std::istringstream iss(ids_str);
                std::string token;
                while (std::getline(iss, token, ',')) {
                    UID id = std::atoll(token.c_str());
                    if (id > 0) trace_order_ids.push_back(id);
                }
            } else if (std::strcmp(argv[i], "--trace-order-out") == 0 && i + 1 < argc) {
                order_trace_path = argv[++i];
            } else if (std::strcmp(argv[i], "--trace-crossed-context") == 0 && i + 1 < argc) {
                ring_buffer_size = std::atoi(argv[++i]);
            } else if (std::strcmp(argv[i], "--trace-best-level-orders-out") == 0 && i + 1 < argc) {
                best_level_orders_path = argv[++i];
            } else if (std::strcmp(argv[i], "--trace-missing-order-out") == 0 && i + 1 < argc) {
                missing_order_path = argv[++i];
            } else if (std::strcmp(argv[i], "--auto-trace-crossed-orders-out") == 0 && i + 1 < argc) {
                auto_trace_crossed_path = argv[++i];
            } else if (std::strcmp(argv[i], "--fill-semantics") == 0 && i + 1 < argc) {
                std::string sem = argv[++i];
                if (sem == "delta") {
                    fill_delta_mode = true;
                } else if (sem == "rest") {
                    fill_delta_mode = false;
                } else {
                    std::cerr << "Unknown fill semantics: " << sem << " (use delta or rest)" << std::endl;
                    return 1;
                }
            } else if (std::strcmp(argv[i], "--snapshot-records-mode") == 0 && i + 1 < argc) {
                std::string mode_str = argv[++i];
                if (mode_str == "ignore") {
                    snapshot_records_mode = SnapshotRecordsMode::Ignore;
                } else if (mode_str == "load") {
                    snapshot_records_mode = SnapshotRecordsMode::Load;
                } else if (mode_str == "marker") {
                    snapshot_records_mode = SnapshotRecordsMode::Marker;
                } else {
                    std::cerr << "Unknown snapshot records mode: " << mode_str << " (use ignore, load, or marker)" << std::endl;
                    return 1;
                }
            } else if (std::strcmp(argv[i], "--orphan-fill-mode") == 0 && i + 1 < argc) {
                std::string mode_str = argv[++i];
                if (mode_str == "strict") {
                    orphan_fill_mode = OrphanFillMode::Strict;
                } else if (mode_str == "ignore") {
                    orphan_fill_mode = OrphanFillMode::Ignore;
                } else if (mode_str == "reduce-same-price") {
                    orphan_fill_mode = OrphanFillMode::ReduceSamePrice;
                } else if (mode_str == "transaction-rest") {
                    orphan_fill_mode = OrphanFillMode::TransactionRest;
                } else {
                    std::cerr << "Unknown orphan fill mode: " << mode_str << " (use strict, ignore, reduce-same-price, or transaction-rest)" << std::endl;
                    return 1;
                }
            }
        }
        return cmd_l3_to_l2(file_path, depth, max_records, max_snapshots, out_path, diag_path,
                            max_diagnostics, trace_path, max_trace_events, snap_mode,
                            trace_order_ids, order_trace_path, ring_buffer_size,
                            best_level_orders_path, missing_order_path, auto_trace_crossed_path,
                            fill_delta_mode, snapshot_records_mode, orphan_fill_mode);
    }

    std::cerr << "Unknown command: " << cmd << std::endl;
    print_help();
    return 1;
}
