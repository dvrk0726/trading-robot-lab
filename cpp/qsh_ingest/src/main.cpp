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

// M10K: Book update mode.
enum class BookUpdateMode { PerRecord, TxGrouped };

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
    std::cout << "  missing-cancel-probe <OrdLog.qsh> [--out <file.csv>] [--max-probes N]\n";
    std::cout << "                        Probe missing_on_cancel orders for prior occurrences\n";
    std::cout << "  orphan-cancel-audit <OrdLog.qsh> [--out <file.csv>] [--max-audits N]\n";
    std::cout << "                        Detailed audit of orphan cancel/remove records\n";
    std::cout << "  snapshot-audit <OrdLog.qsh> [--out <file.csv>] [--max-records N]\n";
    std::cout << "                        Audit snapshot records before first crossing\n";
    std::cout << "  crossing-window-audit <OrdLog.qsh> --from N --to N --out <file.csv>\n";
    std::cout << "                        [--target-order-id ID] [--target-price P]\n";
    std::cout << "                        Audit records in range with book state tracking\n";
    std::cout << "  counter-flag-audit <OrdLog.qsh> [--out <file.csv>] [--max-records N]\n";
    std::cout << "                        Audit Counter flag (0x100) records and book impact\n";
    std::cout << "  first-crossed-root-cause <OrdLog.qsh> [--out-dir <dir>] [--context N]\n";
    std::cout << "                        Trace the exact orders causing first crossed book\n";
    std::cout << "  crossed-persistence-audit <OrdLog.qsh> --from N [--max-records N] --out <file.csv>\n";
    std::cout << "                        Audit crossed-state persistence and order lifecycle\n";
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
    std::cout << "                        [--orphan-cancel-mode strict|ignore]\n";
    std::cout << "                        [--counter-mode include|ignore-book]\n";
    std::cout << "                        [--book-update-mode per-record|tx-grouped]\n";
    std::cout << "                        [--summary-out <file.json>]\n";
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
    std::cout << "  --orphan-cancel-mode <mode>  Orphan cancel/remove handling: strict (default),\n";
    std::cout << "                               or ignore (skip cancel/remove of unknown order)\n";
    std::cout << "  --counter-mode <mode>        Counter event handling: include (default),\n";
    std::cout << "                               or ignore-book (skip Counter events for book mutation)\n";
    std::cout << "  --book-update-mode <mode>  Book update: per-record (default) or tx-grouped\n";
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

// M10L: Probe missing_on_cancel orders for prior occurrences
static int cmd_missing_cancel_probe(const std::string& path, const std::string& out_path, int64_t max_probes) {
    auto file = open_qsh_file(path);
    if (!file.valid) {
        std::cerr << "Error: " << file.error << std::endl;
        return 1;
    }

    if (file.header.stream != StreamType::OrderLog) {
        std::cerr << "Error: missing-cancel-probe requires OrdLog stream, got "
                  << stream_type_name(file.header.stream) << std::endl;
        return 1;
    }

    std::cout << "Probing missing_on_cancel orders..." << std::endl;

    // First pass: collect missing_on_cancel order IDs
    OrderBook book;
    OrdLogReader reader;
    OrderLogRecord rec;
    int64_t record_count = 0;

    struct CancelProbe {
        int64_t record_index;
        Timestamp ts;
        UID order_id;
        OLMsgType event_type;
        Side side;
        Price price;
        Volume amount;
        Volume amount_rest;
        uint16_t flags;
        Volume repl_act;
        bool is_system;
        bool is_non_system;
    };
    std::vector<CancelProbe> cancel_probes;

    while (reader.next(file, rec)) {
        ++record_count;

        if (!is_system_record(rec)) continue;

        if (has_flag(rec.order_flags, OLFlags::NewSession)) {
            book.clear();
            continue;
        }

        int64_t missing_before = book.errors().missing_order_id;
        book.apply(rec);

        if (book.errors().missing_order_id > missing_before &&
            book.errors().missing_on_cancel > (cancel_probes.size() > 0 ? 1 : 0)) {
            // This is a missing_on_cancel event
            CancelProbe probe;
            probe.record_index = record_count;
            probe.ts = rec.timestamp;
            probe.order_id = rec.order_id;
            probe.event_type = rec.event;
            probe.side = rec.side;
            probe.price = rec.price;
            probe.amount = rec.amount;
            probe.amount_rest = rec.amount_rest;
            probe.flags = rec.order_flags;
            probe.repl_act = has_flag(rec.order_flags, OLFlags::NonZeroReplAct) ? 1 : 0;
            probe.is_system = is_system_record(rec);
            probe.is_non_system = has_flag(rec.order_flags, OLFlags::NonSystem);
            cancel_probes.push_back(probe);

            if (max_probes > 0 && static_cast<int64_t>(cancel_probes.size()) >= max_probes) {
                break;
            }
        }
    }

    if (cancel_probes.empty()) {
        std::cout << "No missing_on_cancel events found." << std::endl;
        return 0;
    }

    std::cout << "Found " << cancel_probes.size() << " missing_on_cancel events." << std::endl;

    // Second pass: search backward for prior occurrences of each order_id
    file = open_qsh_file(path);
    if (!file.valid) {
        std::cerr << "Error: " << file.error << std::endl;
        return 1;
    }

    struct ProbeResult {
        CancelProbe probe;
        bool prior_add_found = false;
        int64_t prior_add_index = 0;
        bool prior_snapshot_found = false;
        int64_t prior_snapshot_index = 0;
        bool prior_any_occurrence_found = false;
    };
    std::vector<ProbeResult> results;
    for (const auto& p : cancel_probes) {
        ProbeResult r;
        r.probe = p;
        results.push_back(r);
    }

    // Build set of order_ids to search for
    std::set<UID> probe_order_ids;
    for (const auto& r : results) {
        probe_order_ids.insert(r.probe.order_id);
    }

    // Scan backward through file
    OrdLogReader reader2;
    OrderLogRecord rec2;
    record_count = 0;

    // Track first occurrences
    std::set<UID> found_add;
    std::set<UID> found_snapshot;
    std::set<UID> found_any;

    while (reader2.next(file, rec2)) {
        ++record_count;

        // Skip records after the first probe
        if (!results.empty() && record_count >= results[0].probe.record_index) {
            break;
        }

        if (probe_order_ids.count(rec2.order_id) == 0) continue;

        bool is_snap = has_flag(rec2.order_flags, OLFlags::Snapshot);

        // Track ADD occurrences
        if (rec2.event == OLMsgType::Add && found_add.count(rec2.order_id) == 0) {
            found_add.insert(rec2.order_id);
            for (auto& r : results) {
                if (r.probe.order_id == rec2.order_id) {
                    r.prior_add_found = true;
                    r.prior_add_index = record_count;
                }
            }
        }

        // Track Snapshot occurrences
        if (is_snap && found_snapshot.count(rec2.order_id) == 0) {
            found_snapshot.insert(rec2.order_id);
            for (auto& r : results) {
                if (r.probe.order_id == rec2.order_id) {
                    r.prior_snapshot_found = true;
                    r.prior_snapshot_index = record_count;
                }
            }
        }

        // Track any occurrence
        if (found_any.count(rec2.order_id) == 0) {
            found_any.insert(rec2.order_id);
            for (auto& r : results) {
                if (r.probe.order_id == rec2.order_id) {
                    r.prior_any_occurrence_found = true;
                }
            }
        }
    }

    // Write results to CSV
    if (!out_path.empty()) {
        std::ofstream csv(out_path);
        if (csv.is_open()) {
            csv << "record_index,ts,order_id,event_type,side,price,amount,amount_rest,"
                << "prior_add_found,prior_snapshot_found,prior_any_occurrence_found,"
                << "raw_flags,raw_repl_act,is_system,is_non_system\n";

            for (const auto& r : results) {
                csv << r.probe.record_index << ","
                    << r.probe.ts << ","
                    << r.probe.order_id << ","
                    << ol_msg_type_name(r.probe.event_type) << ","
                    << side_name(r.probe.side) << ","
                    << r.probe.price << ","
                    << r.probe.amount << ","
                    << r.probe.amount_rest << ","
                    << (r.prior_add_found ? 1 : 0) << ","
                    << (r.prior_snapshot_found ? 1 : 0) << ","
                    << (r.prior_any_occurrence_found ? 1 : 0) << ","
                    << r.probe.flags << ","
                    << r.probe.repl_act << ","
                    << (r.probe.is_system ? 1 : 0) << ","
                    << (r.probe.is_non_system ? 1 : 0) << "\n";
            }
            csv.close();
            std::cout << "Wrote " << results.size() << " probe results to " << out_path << std::endl;
        } else {
            std::cerr << "Warning: cannot open output file: " << out_path << std::endl;
        }
    }

    // Print summary
    int64_t with_prior_add = 0;
    int64_t with_prior_snapshot = 0;
    int64_t with_any = 0;
    for (const auto& r : results) {
        if (r.prior_add_found) ++with_prior_add;
        if (r.prior_snapshot_found) ++with_prior_snapshot;
        if (r.prior_any_occurrence_found) ++with_any;
    }

    std::cout << "\nProbe summary:" << std::endl;
    std::cout << "  Total missing_on_cancel: " << results.size() << std::endl;
    std::cout << "  With prior ADD:          " << with_prior_add << std::endl;
    std::cout << "  With prior Snapshot:     " << with_prior_snapshot << std::endl;
    std::cout << "  With any occurrence:     " << with_any << std::endl;

    return 0;
}

// M10N: Orphan cancel/remove audit — detailed analysis of missing_on_cancel/remove records
static int cmd_orphan_cancel_audit(const std::string& path, const std::string& out_path, int64_t max_audits) {
    auto file = open_qsh_file(path);
    if (!file.valid) {
        std::cerr << "Error: " << file.error << std::endl;
        return 1;
    }

    if (file.header.stream != StreamType::OrderLog) {
        std::cerr << "Error: orphan-cancel-audit requires OrdLog stream, got "
                  << stream_type_name(file.header.stream) << std::endl;
        return 1;
    }

    std::cout << "Running orphan cancel/remove audit..." << std::endl;

    // First pass: collect orphan cancel/remove events
    struct OrphanRecord {
        int64_t record_index;
        Timestamp ts;
        int64_t tx_index;
        UID order_id;
        OLMsgType event_type;
        Side side;
        Price price;
        Volume amount;
        Volume amount_rest;
        uint16_t flags;
        Volume repl_act;
        bool is_system;
        bool is_non_system;
        bool is_snapshot;
        bool is_new_session;
        bool is_txend;
        bool has_order_id_field;
        bool has_price_field;
        bool has_amount_field;
    };
    std::vector<OrphanRecord> orphans;

    OrderBook book;
    OrdLogReader reader;
    OrderLogRecord rec;
    int64_t record_count = 0;
    int64_t tx_counter = 0;

    while (reader.next(file, rec)) {
        ++record_count;

        if (!is_system_record(rec)) continue;

        if (has_flag(rec.order_flags, OLFlags::TxEnd)) {
            ++tx_counter;
        }

        if (has_flag(rec.order_flags, OLFlags::NewSession)) {
            book.clear();
            continue;
        }

        int64_t missing_before = book.errors().missing_order_id;
        book.apply(rec);

        if (book.errors().missing_order_id > missing_before) {
            // Check if this is a cancel or remove
            bool is_cancel = (rec.event == OLMsgType::Cancel);
            bool is_remove = (rec.event == OLMsgType::Remove);
            if (is_cancel || is_remove) {
                OrphanRecord o;
                o.record_index = record_count;
                o.ts = rec.timestamp;
                o.tx_index = tx_counter;
                o.order_id = rec.order_id;
                o.event_type = rec.event;
                o.side = rec.side;
                o.price = rec.price;
                o.amount = rec.amount;
                o.amount_rest = rec.amount_rest;
                o.flags = rec.order_flags;
                o.repl_act = has_flag(rec.order_flags, OLFlags::NonZeroReplAct) ? 1 : 0;
                o.is_system = is_system_record(rec);
                o.is_non_system = has_flag(rec.order_flags, OLFlags::NonSystem);
                o.is_snapshot = has_flag(rec.order_flags, OLFlags::Snapshot);
                o.is_new_session = has_flag(rec.order_flags, OLFlags::NewSession);
                o.is_txend = has_flag(rec.order_flags, OLFlags::TxEnd);
                o.has_order_id_field = has_flag(rec.entry_flags, OLEntryFlags::OrderId);
                o.has_price_field = has_flag(rec.entry_flags, OLEntryFlags::Price);
                o.has_amount_field = has_flag(rec.entry_flags, OLEntryFlags::Amount);
                orphans.push_back(o);

                if (max_audits > 0 && static_cast<int64_t>(orphans.size()) >= max_audits) {
                    break;
                }
            }
        }
    }

    if (orphans.empty()) {
        std::cout << "No orphan cancel/remove events found." << std::endl;
        return 0;
    }

    std::cout << "Found " << orphans.size() << " orphan cancel/remove events." << std::endl;

    // Second pass: search for prior and next occurrences of each order_id
    file = open_qsh_file(path);
    if (!file.valid) {
        std::cerr << "Error: " << file.error << std::endl;
        return 1;
    }

    // Build set of order_ids to search for
    std::set<UID> probe_order_ids;
    for (const auto& o : orphans) {
        probe_order_ids.insert(o.order_id);
    }

    struct AuditResult {
        OrphanRecord orphan;
        bool prior_add_found = false;
        int64_t prior_add_index = 0;
        bool prior_snapshot_found = false;
        int64_t prior_snapshot_index = 0;
        bool prior_any_occurrence_found = false;
        int64_t prior_any_index = 0;
        bool next_any_occurrence_found = false;
        int64_t next_any_index = 0;
        bool near_new_session = false;
    };
    std::vector<AuditResult> results;
    for (const auto& o : orphans) {
        AuditResult r;
        r.orphan = o;
        results.push_back(r);
    }

    // Scan forward through file for prior and next occurrences
    OrdLogReader reader2;
    OrderLogRecord rec2;
    record_count = 0;

    // Track first occurrences before each orphan
    std::map<UID, bool> found_add;
    std::map<UID, int64_t> found_add_idx;
    std::map<UID, bool> found_snap;
    std::map<UID, int64_t> found_snap_idx;
    std::map<UID, bool> found_any;
    std::map<UID, int64_t> found_any_idx;

    // Track the first orphan index per order_id
    std::map<UID, int64_t> first_orphan_idx;
    for (const auto& r : results) {
        if (first_orphan_idx.find(r.orphan.order_id) == first_orphan_idx.end()) {
            first_orphan_idx[r.orphan.order_id] = r.orphan.record_index;
        }
    }

    while (reader2.next(file, rec2)) {
        ++record_count;

        // Stop after the last orphan we care about
        int64_t max_orphan_idx = 0;
        for (const auto& r : results) {
            if (r.orphan.record_index > max_orphan_idx) max_orphan_idx = r.orphan.record_index;
        }
        if (record_count > max_orphan_idx + 1000) break;  // look 1000 records past last orphan

        if (probe_order_ids.count(rec2.order_id) == 0) continue;

        bool is_snap = has_flag(rec2.order_flags, OLFlags::Snapshot);
        bool is_new_sess = has_flag(rec2.order_flags, OLFlags::NewSession);

        // Track ADD occurrences (only before first orphan for that order)
        if (rec2.event == OLMsgType::Add && !found_add.count(rec2.order_id)) {
            auto foid_it = first_orphan_idx.find(rec2.order_id);
            if (foid_it != first_orphan_idx.end() && record_count < foid_it->second) {
                found_add[rec2.order_id] = true;
                found_add_idx[rec2.order_id] = record_count;
            }
        }

        // Track Snapshot occurrences (only before first orphan)
        if (is_snap && !found_snap.count(rec2.order_id)) {
            auto foid_it = first_orphan_idx.find(rec2.order_id);
            if (foid_it != first_orphan_idx.end() && record_count < foid_it->second) {
                found_snap[rec2.order_id] = true;
                found_snap_idx[rec2.order_id] = record_count;
            }
        }

        // Track any occurrence before first orphan
        if (!found_any.count(rec2.order_id)) {
            auto foid_it = first_orphan_idx.find(rec2.order_id);
            if (foid_it != first_orphan_idx.end() && record_count < foid_it->second) {
                found_any[rec2.order_id] = true;
                found_any_idx[rec2.order_id] = record_count;
            }
        }

        // Track next occurrence after orphan
        for (auto& r : results) {
            if (r.orphan.order_id == rec2.order_id && record_count > r.orphan.record_index && !r.next_any_occurrence_found) {
                r.next_any_occurrence_found = true;
                r.next_any_index = record_count;
            }
        }

        // Track near NewSession (within 50 records of orphan)
        if (is_new_sess) {
            for (auto& r : results) {
                if (std::abs(record_count - r.orphan.record_index) < 50) {
                    r.near_new_session = true;
                }
            }
        }
    }

    // Apply prior occurrence results
    for (auto& r : results) {
        UID oid = r.orphan.order_id;
        if (found_add.count(oid)) {
            r.prior_add_found = true;
            r.prior_add_index = found_add_idx[oid];
        }
        if (found_snap.count(oid)) {
            r.prior_snapshot_found = true;
            r.prior_snapshot_index = found_snap_idx[oid];
        }
        if (found_any.count(oid)) {
            r.prior_any_occurrence_found = true;
            r.prior_any_index = found_any_idx[oid];
        }
    }

    // Write results to CSV
    if (!out_path.empty()) {
        std::ofstream csv(out_path);
        if (csv.is_open()) {
            csv << "record_index,ts,tx_index,order_id,event_type,side,price,amount,amount_rest,"
                << "flags_hex,repl_act,is_system,is_non_system,is_snapshot,is_new_session,is_txend,"
                << "has_order_id_field,has_price_field,has_amount_field,"
                << "prior_add_found,prior_snapshot_found,prior_any_occurrence_found,"
                << "next_any_occurrence_found,near_new_session\n";

            for (const auto& r : results) {
                csv << r.orphan.record_index << ","
                    << r.orphan.ts << ","
                    << r.orphan.tx_index << ","
                    << r.orphan.order_id << ","
                    << ol_msg_type_name(r.orphan.event_type) << ","
                    << side_name(r.orphan.side) << ","
                    << r.orphan.price << ","
                    << r.orphan.amount << ","
                    << r.orphan.amount_rest << ","
                    << "0x" << std::hex << r.orphan.flags << std::dec << ","
                    << r.orphan.repl_act << ","
                    << (r.orphan.is_system ? 1 : 0) << ","
                    << (r.orphan.is_non_system ? 1 : 0) << ","
                    << (r.orphan.is_snapshot ? 1 : 0) << ","
                    << (r.orphan.is_new_session ? 1 : 0) << ","
                    << (r.orphan.is_txend ? 1 : 0) << ","
                    << (r.orphan.has_order_id_field ? 1 : 0) << ","
                    << (r.orphan.has_price_field ? 1 : 0) << ","
                    << (r.orphan.has_amount_field ? 1 : 0) << ","
                    << (r.prior_add_found ? 1 : 0) << ","
                    << (r.prior_snapshot_found ? 1 : 0) << ","
                    << (r.prior_any_occurrence_found ? 1 : 0) << ","
                    << (r.next_any_occurrence_found ? 1 : 0) << ","
                    << (r.near_new_session ? 1 : 0) << "\n";
            }
            csv.close();
            std::cout << "Wrote " << results.size() << " audit results to " << out_path << std::endl;
        } else {
            std::cerr << "Warning: cannot open output file: " << out_path << std::endl;
        }
    }

    // Print summary
    int64_t with_prior_add = 0;
    int64_t with_prior_snap = 0;
    int64_t with_any = 0;
    int64_t with_next = 0;
    int64_t near_ns = 0;
    int64_t system_count = 0;
    int64_t non_system_count = 0;
    int64_t snapshot_count = 0;
    int64_t cancel_count = 0;
    int64_t remove_count = 0;

    for (const auto& r : results) {
        if (r.prior_add_found) ++with_prior_add;
        if (r.prior_snapshot_found) ++with_prior_snap;
        if (r.prior_any_occurrence_found) ++with_any;
        if (r.next_any_occurrence_found) ++with_next;
        if (r.near_new_session) ++near_ns;
        if (r.orphan.is_system) ++system_count;
        if (r.orphan.is_non_system) ++non_system_count;
        if (r.orphan.is_snapshot) ++snapshot_count;
        if (r.orphan.event_type == OLMsgType::Cancel) ++cancel_count;
        if (r.orphan.event_type == OLMsgType::Remove) ++remove_count;
    }

    std::cout << "\nOrphan Cancel/Remove Audit Summary:" << std::endl;
    std::cout << "  Total orphan cancel/remove: " << results.size() << std::endl;
    std::cout << "  Cancel events:              " << cancel_count << std::endl;
    std::cout << "  Remove events:              " << remove_count << std::endl;
    std::cout << "  System records:             " << system_count << std::endl;
    std::cout << "  Non-system records:         " << non_system_count << std::endl;
    std::cout << "  Snapshot records:           " << snapshot_count << std::endl;
    std::cout << "  With prior ADD:             " << with_prior_add << std::endl;
    std::cout << "  With prior Snapshot:        " << with_prior_snap << std::endl;
    std::cout << "  With any prior occurrence:  " << with_any << std::endl;
    std::cout << "  With next occurrence:       " << with_next << std::endl;
    std::cout << "  Near NewSession (±50):      " << near_ns << std::endl;

    // Evidence-based conclusion
    std::cout << "\nEvidence-based conclusion:" << std::endl;
    if (with_any == 0 && with_next == 0) {
        std::cout << "  A. Orphan cancel/remove is likely BENIGN — orders never appear anywhere" << std::endl;
        std::cout << "     in the OrdLog stream. These are likely system-level or cross-session" << std::endl;
        std::cout << "     orders whose cancel events can be safely ignored for active-book mutation." << std::endl;
    } else if (with_any > 0) {
        std::cout << "  B. Orphan cancel/remove indicates DECODER/SPEC MISMATCH — some orders" << std::endl;
        std::cout << "     have prior occurrences but are not in the active book. Must stay an error." << std::endl;
    } else {
        std::cout << "  C. INCONCLUSIVE — keep strict behavior." << std::endl;
    }

    return 0;
}

// M10P: Snapshot record audit — dumps snapshot records before first crossing
// and summarizes initial book state after snapshot initialization.
static int cmd_snapshot_audit(const std::string& path, const std::string& out_path, int64_t max_records) {
    auto file = open_qsh_file(path);
    if (!file.valid) {
        std::cerr << "Error: " << file.error << std::endl;
        return 1;
    }

    if (file.header.stream != StreamType::OrderLog) {
        std::cerr << "Error: snapshot-audit requires OrdLog stream, got "
                  << stream_type_name(file.header.stream) << std::endl;
        return 1;
    }

    std::cout << "Running snapshot audit..." << std::endl;

    // Use full OrderBook for accurate book state tracking
    OrderBook book;

    struct SnapshotRecord {
        int64_t record_index;
        Timestamp ts;
        int64_t tx_index;
        UID order_id;
        OLMsgType event_type;
        Side side;
        Price price;
        Volume amount;
        Volume amount_rest;
        uint16_t flags;
        Volume repl_act;
        bool is_snapshot;
        bool is_new_session;
        bool is_txend;
        bool is_system;
        bool is_non_system;
        // Raw decoder state
        int64_t raw_data_offset;
        int raw_entry_flags;
        uint16_t raw_order_flags;
        bool has_timestamp_field;
        bool has_order_id_field;
        bool has_price_field;
        bool has_amount_field;
        int64_t order_id_before_delta;
        int64_t order_id_after_delta;
        Price price_before_delta;
        Price price_after_delta;
        Volume amount_before;
        Volume amount_after;
        Volume amount_rest_before;
        Volume amount_rest_after;
        bool is_add_order_id_path;
    };
    std::vector<SnapshotRecord> snapshot_records;

    OrdLogReader reader;
    OrderLogRecord rec;
    int64_t record_count = 0;
    int64_t tx_counter = 0;
    bool first_crossed_found = false;
    int64_t first_crossed_record = 0;
    int64_t first_non_snapshot_after_new_session = 0;
    bool saw_new_session = false;
    bool in_snapshot_streak = false;
    int64_t snapshot_streak_start = 0;

    // Buy/sell stats for snapshot records
    int64_t buy_count = 0;
    int64_t sell_count = 0;
    Price min_buy_price = 0;
    Price max_buy_price = 0;
    Price min_sell_price = 0;
    Price max_sell_price = 0;

    // Track the target bid order
    const UID target_bid_order = 1925033994466246392;
    bool target_order_found = false;
    int64_t target_order_record_index = 0;
    Side target_order_side = Side::Unknown;
    Price target_order_price = 0;
    Volume target_order_qty = 0;
    uint16_t target_order_flags = 0;
    Volume target_order_repl_act = 0;

    while (reader.next_debug(file, rec)) {
        ++record_count;

        if (has_flag(rec.order_flags, OLFlags::TxEnd)) {
            ++tx_counter;
        }

        bool is_snapshot = has_flag(rec.order_flags, OLFlags::Snapshot);
        bool is_new_session = has_flag(rec.order_flags, OLFlags::NewSession);
        bool is_txend = has_flag(rec.order_flags, OLFlags::TxEnd);
        bool is_system = is_system_record(rec);
        bool is_non_system = has_flag(rec.order_flags, OLFlags::NonSystem);
        Volume repl_act = has_flag(rec.order_flags, OLFlags::NonZeroReplAct) ? 1 : 0;

        // Track NewSession
        if (is_new_session) {
            saw_new_session = true;
            in_snapshot_streak = false;
            book.clear();
            continue;
        }

        // Track snapshot streak boundary
        if (saw_new_session) {
            if (is_snapshot) {
                if (!in_snapshot_streak) {
                    in_snapshot_streak = true;
                    snapshot_streak_start = record_count;
                }
            } else {
                if (in_snapshot_streak && first_non_snapshot_after_new_session == 0) {
                    first_non_snapshot_after_new_session = record_count;
                    in_snapshot_streak = false;
                    // Snapshot streak ended — stop processing
                    break;
                }
            }
        }

        // Process snapshot records
        if (is_snapshot && !first_crossed_found) {
            // Track buy/sell counts and prices
            if (rec.side == Side::Buy) {
                ++buy_count;
                if (min_buy_price == 0 || rec.price < min_buy_price) min_buy_price = rec.price;
                if (rec.price > max_buy_price) max_buy_price = rec.price;
            } else if (rec.side == Side::Sell) {
                ++sell_count;
                if (min_sell_price == 0 || rec.price < min_sell_price) min_sell_price = rec.price;
                if (rec.price > max_sell_price) max_sell_price = rec.price;
            }

            // Track target bid order
            if (rec.order_id == target_bid_order) {
                target_order_found = true;
                target_order_record_index = record_count;
                target_order_side = rec.side;
                target_order_price = rec.price;
                target_order_qty = rec.amount_rest;
                target_order_flags = rec.order_flags;
                target_order_repl_act = repl_act;
            }

            // Apply to full OrderBook for accurate state tracking
            book.apply(rec);

            // Check for crossed book in current state
            if (book.bid_depth() > 0 && book.ask_depth() > 0) {
                Price bb = book.best_bid();
                Price ba = book.best_ask();
                if (bb > 0 && ba > 0 && bb >= ba && !first_crossed_found) {
                    first_crossed_found = true;
                    first_crossed_record = record_count;
                }
            }

            // Store snapshot record
            SnapshotRecord sr;
            sr.record_index = record_count;
            sr.ts = rec.timestamp;
            sr.tx_index = tx_counter;
            sr.order_id = rec.order_id;
            sr.event_type = rec.event;
            sr.side = rec.side;
            sr.price = rec.price;
            sr.amount = rec.amount;
            sr.amount_rest = rec.amount_rest;
            sr.flags = rec.order_flags;
            sr.repl_act = repl_act;
            sr.is_snapshot = is_snapshot;
            sr.is_new_session = is_new_session;
            sr.is_txend = is_txend;
            sr.is_system = is_system;
            sr.is_non_system = is_non_system;
            sr.raw_data_offset = rec.debug.raw_data_offset;
            sr.raw_entry_flags = static_cast<int>(rec.debug.raw_entry_flags);
            sr.raw_order_flags = rec.debug.raw_order_flags;
            sr.has_timestamp_field = rec.debug.has_timestamp_field;
            sr.has_order_id_field = rec.debug.has_order_id_field;
            sr.has_price_field = rec.debug.has_price_field;
            sr.has_amount_field = rec.debug.has_amount_field;
            sr.order_id_before_delta = rec.debug.order_id_before_delta;
            sr.order_id_after_delta = rec.debug.order_id_after_delta;
            sr.price_before_delta = rec.debug.price_before_delta;
            sr.price_after_delta = rec.debug.price_after_delta;
            sr.amount_before = rec.debug.amount_before;
            sr.amount_after = rec.debug.amount_after;
            sr.amount_rest_before = 0;  // not tracked by debug
            sr.amount_rest_after = rec.amount_rest;
            sr.is_add_order_id_path = rec.debug.is_add_order_id_path;
            snapshot_records.push_back(sr);

            if (max_records > 0 && static_cast<int64_t>(snapshot_records.size()) >= max_records) {
                break;
            }
        } else if (!is_snapshot && !first_crossed_found) {
            // Non-snapshot record: apply to book for accurate state tracking
            book.apply(rec);
        }
    }

    // Write CSV
    if (!out_path.empty()) {
        std::ofstream csv(out_path);
        if (csv.is_open()) {
            csv << "record_index,ts,tx_index,order_id,event_type,side,price,amount,amount_rest,"
                << "flags_hex,repl_act,is_snapshot,is_new_session,is_txend,is_system,is_non_system,"
                << "raw_data_offset,raw_entry_flags,raw_order_flags_hex,raw_side_bits,raw_event_bits,"
                << "has_timestamp_field,has_order_id_field,has_price_field,has_amount_field,"
                << "order_id_before_delta,order_id_after_delta,order_id_delta,"
                << "price_before_delta,price_after_delta,price_delta,"
                << "amount_before,amount_after,amount_rest_before,amount_rest_after,"
                << "order_id_path\n";

            for (const auto& sr : snapshot_records) {
                uint16_t side_bits = sr.flags & (OLFlags::Buy | OLFlags::Sell);
                uint16_t event_bits = sr.flags & (OLFlags::Add | OLFlags::Fill | OLFlags::Moved |
                    OLFlags::Canceled | OLFlags::CanceledGroup | OLFlags::CrossTrade);

                csv << sr.record_index << ","
                    << sr.ts << ","
                    << sr.tx_index << ","
                    << sr.order_id << ","
                    << ol_msg_type_name(sr.event_type) << ","
                    << side_name(sr.side) << ","
                    << sr.price << ","
                    << sr.amount << ","
                    << sr.amount_rest << ","
                    << "0x" << std::hex << sr.flags << std::dec << ","
                    << sr.repl_act << ","
                    << (sr.is_snapshot ? 1 : 0) << ","
                    << (sr.is_new_session ? 1 : 0) << ","
                    << (sr.is_txend ? 1 : 0) << ","
                    << (sr.is_system ? 1 : 0) << ","
                    << (sr.is_non_system ? 1 : 0) << ","
                    << sr.raw_data_offset << ","
                    << sr.raw_entry_flags << ","
                    << "0x" << std::hex << sr.raw_order_flags << std::dec << ","
                    << "0x" << std::hex << side_bits << std::dec << ","
                    << "0x" << std::hex << event_bits << std::dec << ","
                    << (sr.has_timestamp_field ? 1 : 0) << ","
                    << (sr.has_order_id_field ? 1 : 0) << ","
                    << (sr.has_price_field ? 1 : 0) << ","
                    << (sr.has_amount_field ? 1 : 0) << ","
                    << sr.order_id_before_delta << ","
                    << sr.order_id_after_delta << ","
                    << (sr.order_id_after_delta - sr.order_id_before_delta) << ","
                    << sr.price_before_delta << ","
                    << sr.price_after_delta << ","
                    << (sr.price_after_delta - sr.price_before_delta) << ","
                    << sr.amount_before << ","
                    << sr.amount_after << ","
                    << sr.amount_rest_before << ","
                    << sr.amount_rest_after << ","
                    << (sr.is_add_order_id_path ? "growing" : "leb128") << "\n";
            }
            csv.close();
            std::cout << "Wrote " << snapshot_records.size() << " snapshot records to " << out_path << std::endl;
        } else {
            std::cerr << "Warning: cannot open output file: " << out_path << std::endl;
        }
    }

    // Compute initial book state summary using full OrderBook
    Price snapshot_best_bid = book.best_bid();
    Price snapshot_best_ask = book.best_ask();
    Price snapshot_spread = book.spread();
    bool snapshot_crossed = (snapshot_best_bid > 0 && snapshot_best_ask > 0 && snapshot_best_bid >= snapshot_best_ask);

    // Count orders at best levels
    Volume snapshot_top_bid_qty = book.best_bid_total_qty();
    Volume snapshot_top_ask_qty = book.best_ask_total_qty();
    int snapshot_top_bid_order_count = book.best_bid_order_count();
    int snapshot_top_ask_order_count = book.best_ask_order_count();

    // Count crossed snapshot records using first_crossed_record
    int64_t crossed_snapshot_records = 0;
    if (first_crossed_record > 0) {
        // Count how many snapshot records are at or after the first crossed record
        for (const auto& sr : snapshot_records) {
            if (sr.record_index >= first_crossed_record) ++crossed_snapshot_records;
        }
    }

    // Print snapshot audit summary
    std::cout << "\n=== Snapshot Audit Summary ===" << std::endl;
    std::cout << "snapshot_records_processed:           " << snapshot_records.size() << std::endl;
    std::cout << "snapshot_buy_records:                 " << buy_count << std::endl;
    std::cout << "snapshot_sell_records:                " << sell_count << std::endl;
    std::cout << "snapshot_min_buy_price:               " << min_buy_price << std::endl;
    std::cout << "snapshot_max_buy_price:               " << max_buy_price << std::endl;
    std::cout << "snapshot_min_sell_price:              " << min_sell_price << std::endl;
    std::cout << "snapshot_max_sell_price:              " << max_sell_price << std::endl;

    // Print initial book state summary
    std::cout << "\n=== Initial Book State (after snapshot load) ===" << std::endl;
    std::cout << "snapshot_records_loaded:              " << snapshot_records.size() << std::endl;
    std::cout << "snapshot_buy_orders_loaded:           " << buy_count << std::endl;
    std::cout << "snapshot_sell_orders_loaded:           " << sell_count << std::endl;
    std::cout << "snapshot_best_bid:                    " << snapshot_best_bid << std::endl;
    std::cout << "snapshot_best_ask:                    " << snapshot_best_ask << std::endl;
    std::cout << "snapshot_spread:                      " << snapshot_spread << std::endl;
    std::cout << "snapshot_crossed_at_initial_load:     " << (snapshot_crossed ? "YES" : "NO") << std::endl;
    std::cout << "snapshot_crossed_order_count:         " << crossed_snapshot_records << std::endl;
    std::cout << "snapshot_top_bid_qty:                 " << snapshot_top_bid_qty << std::endl;
    std::cout << "snapshot_top_ask_qty:                 " << snapshot_top_ask_qty << std::endl;
    std::cout << "snapshot_top_bid_order_count:         " << snapshot_top_bid_order_count << std::endl;
    std::cout << "snapshot_top_ask_order_count:         " << snapshot_top_ask_order_count << std::endl;

    // Snapshot boundary
    std::cout << "\n=== Snapshot Boundary ===" << std::endl;
    std::cout << "saw_new_session:                      " << (saw_new_session ? "YES" : "NO") << std::endl;
    std::cout << "snapshot_streak_start:                " << snapshot_streak_start << std::endl;
    std::cout << "first_non_snapshot_after_new_session: " << first_non_snapshot_after_new_session << std::endl;
    std::cout << "first_crossed_record:                 " << first_crossed_record << std::endl;

    // Target bid order trace
    std::cout << "\n=== Bid Order Trace (1925033994466246392) ===" << std::endl;
    if (target_order_found) {
        std::cout << "CONCLUSION: A" << std::endl;
        std::cout << "  order enters via a snapshot record decoded as "
                  << side_name(target_order_side) << " " << target_order_price
                  << " qty=" << target_order_qty << std::endl;
        std::cout << "  record_index:  " << target_order_record_index << std::endl;
        std::cout << "  flags_hex:     0x" << std::hex << target_order_flags << std::dec << std::endl;
        std::cout << "  repl_act:      " << target_order_repl_act << std::endl;
    } else {
        std::cout << "CONCLUSION: E" << std::endl;
        std::cout << "  order does not appear in snapshot records before first crossing." << std::endl;
    }

    return 0;
}

// M10Q: Crossing window audit — dump records in a range with book state tracking.
// Used to find the exact record that creates BUY 14100 in the active book.
static int cmd_crossing_window_audit(const std::string& path, int64_t from_index, int64_t to_index,
                                     const std::string& out_path, UID target_order_id, Price target_price) {
    auto file = open_qsh_file(path);
    if (!file.valid) {
        std::cerr << "Error: " << file.error << std::endl;
        return 1;
    }

    if (file.header.stream != StreamType::OrderLog) {
        std::cerr << "Error: crossing-window-audit requires OrdLog stream, got "
                  << stream_type_name(file.header.stream) << std::endl;
        return 1;
    }

    std::cout << "Running crossing window audit (records " << from_index << ".." << to_index << ")..." << std::endl;
    std::cout << "Target order_id: " << target_order_id << std::endl;
    std::cout << "Target price:    " << target_price << std::endl;

    std::ofstream csv(out_path);
    if (!csv.is_open()) {
        std::cerr << "Error: cannot open output file: " << out_path << std::endl;
        return 1;
    }

    // CSV header
    csv << "record_index,ts,tx_index,order_id,event_type,side,price,amount,amount_rest,"
        << "flags_hex,repl_act,is_snapshot,is_new_session,is_txend,is_system,is_non_system,"
        << "raw_data_offset,raw_entry_flags,raw_order_flags_hex,raw_side_bits,raw_event_bits,"
        << "has_order_id_field,has_price_field,has_amount_field,"
        << "order_id_delta,price_delta,"
        << "best_bid_before,best_ask_before,best_bid_after,best_ask_after,"
        << "qty_target_bid_before,qty_target_bid_after,"
        << "is_target_order,is_target_price_buy,"
        << "target_order_active_before,target_order_active_after,"
        << "mutation_path\n";

    OrderBook book;
    OrdLogReader reader;
    OrderLogRecord rec;
    int64_t record_count = 0;
    int64_t tx_counter = 0;
    int64_t written = 0;

    // Entry point tracking
    bool target_order_first_seen = false;
    int64_t target_order_first_record = 0;
    bool target_order_in_book_first = false;
    int64_t target_order_in_book_first_record = 0;
    bool target_price_buy_first = false;
    int64_t target_price_buy_first_record = 0;
    bool best_bid_is_target = false;
    int64_t best_bid_target_first_record = 0;
    bool target_level_qty_first = false;
    int64_t target_level_qty_first_record = 0;

    while (reader.next_debug(file, rec)) {
        ++record_count;

        if (has_flag(rec.order_flags, OLFlags::TxEnd)) {
            ++tx_counter;
        }

        // Skip non-system records
        if (!is_system_record(rec)) continue;

        // NewSession clears the book
        if (has_flag(rec.order_flags, OLFlags::NewSession)) {
            book.clear();
            if (record_count >= from_index && record_count <= to_index) {
                // Still output the NewSession record
                bool is_snapshot = has_flag(rec.order_flags, OLFlags::Snapshot);
                bool is_new_session = true;
                bool is_txend = has_flag(rec.order_flags, OLFlags::TxEnd);
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
                    << "0x" << std::hex << rec.order_flags << std::dec << ","
                    << repl_act << ","
                    << (is_snapshot ? 1 : 0) << ","
                    << (is_new_session ? 1 : 0) << ","
                    << (is_txend ? 1 : 0) << ","
                    << 1 << ","
                    << (is_non_system ? 1 : 0) << ","
                    << rec.debug.raw_data_offset << ","
                    << static_cast<int>(rec.debug.raw_entry_flags) << ","
                    << "0x" << std::hex << rec.debug.raw_order_flags << std::dec << ","
                    << "0x" << std::hex << (rec.order_flags & (OLFlags::Buy | OLFlags::Sell)) << std::dec << ","
                    << "0x" << std::hex << (rec.order_flags & (OLFlags::Add | OLFlags::Fill | OLFlags::Moved | OLFlags::Canceled | OLFlags::CanceledGroup | OLFlags::CrossTrade)) << std::dec << ","
                    << (rec.debug.has_order_id_field ? 1 : 0) << ","
                    << (rec.debug.has_price_field ? 1 : 0) << ","
                    << (rec.debug.has_amount_field ? 1 : 0) << ","
                    << (rec.debug.order_id_after_delta - rec.debug.order_id_before_delta) << ","
                    << (rec.debug.price_after_delta - rec.debug.price_before_delta) << ","
                    << 0 << "," << 0 << "," << 0 << "," << 0 << ","
                    << 0 << "," << 0 << ","
                    << (rec.order_id == target_order_id ? 1 : 0) << ","
                    << (rec.price == target_price && rec.side == Side::Buy ? 1 : 0) << ","
                    << 0 << "," << 0 << ","
                    << "NEW_SESSION"
                    << "\n";
                ++written;
            }
            continue;
        }

        // Track state before apply
        Price best_bid_before = book.best_bid();
        Price best_ask_before = book.best_ask();
        Volume qty_target_bid_before = book.level_qty(target_price, Side::Buy);

        bool target_order_active_before = false;
        {
            Side dummy_s; Price dummy_p; Volume dummy_v;
            target_order_active_before = book.get_order_info(target_order_id, dummy_s, dummy_p, dummy_v);
        }

        // Detect mutation path
        std::string mutation_path;
        if (rec.order_id == target_order_id) {
            if (rec.event == OLMsgType::Add) mutation_path = "add_order";
            else if (rec.event == OLMsgType::Moved) mutation_path = "move_order";
            else if (rec.event == OLMsgType::Fill) mutation_path = "fill_order";
            else if (rec.event == OLMsgType::Cancel) mutation_path = "cancel_order";
            else if (rec.event == OLMsgType::Remove) mutation_path = "remove_order";
            else mutation_path = "other";
        }
        // Also detect if a BUY add at target_price creates the level
        if (rec.event == OLMsgType::Add && rec.side == Side::Buy && rec.price == target_price) {
            if (qty_target_bid_before == 0) {
                mutation_path = "add_order_creates_level";
            }
        }

        // Apply to book
        book.apply(rec);

        // Track state after apply
        Price best_bid_after = book.best_bid();
        Price best_ask_after = book.best_ask();
        Volume qty_target_bid_after = book.level_qty(target_price, Side::Buy);

        bool target_order_active_after = false;
        {
            Side dummy_s; Price dummy_p; Volume dummy_v;
            target_order_active_after = book.get_order_info(target_order_id, dummy_s, dummy_p, dummy_v);
        }

        // Track entry points (earliest only)
        if (rec.order_id == target_order_id && !target_order_first_seen) {
            target_order_first_seen = true;
            target_order_first_record = record_count;
        }
        if (target_order_active_after && !target_order_active_before && !target_order_in_book_first) {
            target_order_in_book_first = true;
            target_order_in_book_first_record = record_count;
            if (mutation_path.empty()) {
                // Infer from event type
                if (rec.event == OLMsgType::Add) mutation_path = "add_order";
                else if (rec.event == OLMsgType::Moved) mutation_path = "move_order";
                else mutation_path = "inferred_" + std::string(ol_msg_type_name(rec.event));
            }
        }
        if (rec.price == target_price && rec.side == Side::Buy && !target_price_buy_first) {
            target_price_buy_first = true;
            target_price_buy_first_record = record_count;
        }
        if (best_bid_after == target_price && !best_bid_is_target) {
            best_bid_is_target = true;
            best_bid_target_first_record = record_count;
        }
        if (qty_target_bid_after > 0 && qty_target_bid_before == 0 && !target_level_qty_first) {
            target_level_qty_first = true;
            target_level_qty_first_record = record_count;
        }

        // Output CSV for records in range
        if (record_count >= from_index && record_count <= to_index) {
            bool is_snapshot = has_flag(rec.order_flags, OLFlags::Snapshot);
            bool is_new_session = has_flag(rec.order_flags, OLFlags::NewSession);
            bool is_txend = has_flag(rec.order_flags, OLFlags::TxEnd);
            bool is_non_system = has_flag(rec.order_flags, OLFlags::NonSystem);
            Volume repl_act = has_flag(rec.order_flags, OLFlags::NonZeroReplAct) ? 1 : 0;

            uint16_t side_bits = rec.order_flags & (OLFlags::Buy | OLFlags::Sell);
            uint16_t event_bits = rec.order_flags & (OLFlags::Add | OLFlags::Fill | OLFlags::Moved |
                OLFlags::Canceled | OLFlags::CanceledGroup | OLFlags::CrossTrade);

            csv << record_count << ","
                << rec.timestamp << ","
                << tx_counter << ","
                << rec.order_id << ","
                << ol_msg_type_name(rec.event) << ","
                << side_name(rec.side) << ","
                << rec.price << ","
                << rec.amount << ","
                << rec.amount_rest << ","
                << "0x" << std::hex << rec.order_flags << std::dec << ","
                << repl_act << ","
                << (is_snapshot ? 1 : 0) << ","
                << (is_new_session ? 1 : 0) << ","
                << (is_txend ? 1 : 0) << ","
                << 1 << ","
                << (is_non_system ? 1 : 0) << ","
                << rec.debug.raw_data_offset << ","
                << static_cast<int>(rec.debug.raw_entry_flags) << ","
                << "0x" << std::hex << rec.debug.raw_order_flags << std::dec << ","
                << "0x" << std::hex << side_bits << std::dec << ","
                << "0x" << std::hex << event_bits << std::dec << ","
                << (rec.debug.has_order_id_field ? 1 : 0) << ","
                << (rec.debug.has_price_field ? 1 : 0) << ","
                << (rec.debug.has_amount_field ? 1 : 0) << ","
                << (rec.debug.order_id_after_delta - rec.debug.order_id_before_delta) << ","
                << (rec.debug.price_after_delta - rec.debug.price_before_delta) << ","
                << best_bid_before << ","
                << best_ask_before << ","
                << best_bid_after << ","
                << best_ask_after << ","
                << qty_target_bid_before << ","
                << qty_target_bid_after << ","
                << (rec.order_id == target_order_id ? 1 : 0) << ","
                << (rec.price == target_price && rec.side == Side::Buy ? 1 : 0) << ","
                << (target_order_active_before ? 1 : 0) << ","
                << (target_order_active_after ? 1 : 0) << ","
                << mutation_path
                << "\n";
            ++written;
        }

        if (record_count > to_index + 100) break;  // some margin past range
    }

    csv.close();
    std::cout << "\nWrote " << written << " records to " << out_path << std::endl;

    // Print entry point summary
    std::cout << "\n=== Entry Point Summary ===" << std::endl;
    std::cout << "target_order_id:                         " << target_order_id << std::endl;
    std::cout << "target_price:                            " << target_price << std::endl;

    std::cout << "\nFirst record with target order_id:       " << (target_order_first_seen ? std::to_string(target_order_first_record) : "NOT FOUND") << std::endl;
    std::cout << "First record with target price BUY:      " << (target_price_buy_first ? std::to_string(target_price_buy_first_record) : "NOT FOUND") << std::endl;
    std::cout << "First record with target order in book:  " << (target_order_in_book_first ? std::to_string(target_order_in_book_first_record) : "NOT FOUND") << std::endl;
    std::cout << "First record with best_bid == target:    " << (best_bid_is_target ? std::to_string(best_bid_target_first_record) : "NOT FOUND") << std::endl;
    std::cout << "First record with qty at target > 0:     " << (target_level_qty_first ? std::to_string(target_level_qty_first_record) : "NOT FOUND") << std::endl;

    // Identify the actual entry record
    int64_t entry_record = 0;
    std::string entry_source;
    if (target_order_in_book_first) {
        entry_record = target_order_in_book_first_record;
        entry_source = "order enters active book";
    } else if (target_level_qty_first) {
        entry_record = target_level_qty_first_record;
        entry_source = "level qty becomes positive";
    } else if (target_order_first_seen) {
        entry_record = target_order_first_record;
        entry_source = "order_id appears in record";
    }

    if (entry_record > 0) {
        std::cout << "\nACTUAL ENTRY RECORD: " << entry_record << " (" << entry_source << ")" << std::endl;
    } else {
        std::cout << "\nENTRY RECORD NOT FOUND in range " << from_index << ".." << to_index << std::endl;
    }

    return 0;
}

// M10R: Crossed-state persistence audit — track every transition from a given record,
// monitor when crossing clears, and trace order/level lifecycle.
static int cmd_crossed_persistence_audit(const std::string& path, int64_t from_index,
                                          int64_t max_records, const std::string& out_path) {
    auto file = open_qsh_file(path);
    if (!file.valid) {
        std::cerr << "Error: " << file.error << std::endl;
        return 1;
    }

    if (file.header.stream != StreamType::OrderLog) {
        std::cerr << "Error: crossed-persistence-audit requires OrdLog stream, got "
                  << stream_type_name(file.header.stream) << std::endl;
        return 1;
    }

    // Target orders and price levels from M10Q context
    const UID bid_order_id = 1925033994466246392;
    const UID ask_order_id = 1925033994522131746;
    const Price bid_target_price = 14100;
    const Price ask_target_price = 14062;

    std::cout << "Running crossed-persistence-audit from record " << from_index << "..." << std::endl;
    std::cout << "bid_order_id: " << bid_order_id << std::endl;
    std::cout << "ask_order_id: " << ask_order_id << std::endl;
    std::cout << "bid_target_price: " << bid_target_price << std::endl;
    std::cout << "ask_target_price: " << ask_target_price << std::endl;

    std::ofstream csv(out_path);
    if (!csv.is_open()) {
        std::cerr << "Error: cannot open output file: " << out_path << std::endl;
        return 1;
    }

    // CSV header per spec
    csv << "record_index,tx_index,ts,event_type,order_id,side,price,amount,amount_rest,"
        << "flags_hex,is_snapshot,is_txend,"
        << "best_bid_before,best_ask_before,best_bid_after,best_ask_after,"
        << "spread_before,spread_after,crossed_before,crossed_after,"
        << "crossed_segment_id,mutation_path\n";

    OrderBook book;
    OrdLogReader reader;
    OrderLogRecord rec;
    int64_t record_count = 0;
    int64_t tx_counter = 0;
    int64_t written = 0;

    // Crossing state tracking
    bool was_crossed = false;
    int64_t crossed_segment_id = 0;
    bool first_crossing_found = false;
    int64_t first_crossing_record = 0;
    Price cross_start_bb = 0;
    Price cross_start_ba = 0;
    int64_t cross_start_tx = 0;
    bool uncross_found = false;
    int64_t first_uncross_record = 0;
    int64_t uncross_tx = 0;
    Price uncross_bb = 0;
    Price uncross_ba = 0;
    int64_t crossed_records = 0;
    int64_t crossed_snapshots = 0;  // TxEnd events while crossed

    // Order lifecycle tracking
    struct OrderState {
        bool active = false;
        Price price = 0;
        Volume qty = 0;
        int64_t last_event_record = 0;
        std::string last_event_type;
        bool was_filled = false;
        bool was_canceled = false;
        bool was_removed = false;
        int64_t terminal_record = 0;
    };
    OrderState bid_order_state;
    OrderState ask_order_state;

    // Price level tracking
    Volume bid_level_qty_before = 0;
    Volume ask_level_qty_before = 0;

    // Pass 1: process all records from start to build book state
    // We need to process from the beginning because the book state at from_index
    // depends on all prior records. We just don't output CSV until from_index.
    while (reader.next_debug(file, rec)) {
        ++record_count;

        if (has_flag(rec.order_flags, OLFlags::TxEnd)) {
            ++tx_counter;
        }

        // Skip non-system records
        if (!is_system_record(rec)) continue;

        // NewSession clears the book
        if (has_flag(rec.order_flags, OLFlags::NewSession)) {
            book.clear();
            if (record_count >= from_index) {
                // Output NewSession record
                bool is_snapshot = has_flag(rec.order_flags, OLFlags::Snapshot);
                bool is_txend = has_flag(rec.order_flags, OLFlags::TxEnd);

                csv << record_count << ","
                    << tx_counter << ","
                    << rec.timestamp << ","
                    << "NewSession" << ","
                    << 0 << ","
                    << "Unknown" << ","
                    << 0 << "," << 0 << "," << 0 << ","
                    << "0x" << std::hex << rec.order_flags << std::dec << ","
                    << (is_snapshot ? 1 : 0) << ","
                    << (is_txend ? 1 : 0) << ","
                    << 0 << "," << 0 << "," << 0 << "," << 0 << ","
                    << 0 << "," << 0 << ","
                    << 0 << "," << 0 << ","
                    << crossed_segment_id << ","
                    << "NEW_SESSION"
                    << "\n";
                ++written;

                // Reset crossing tracking state on NewSession
                // (but keep first_crossing_found/first_crossing_record for summary)
                was_crossed = false;
            }
            continue;
        }

        // Track state before apply
        Price best_bid_before = book.best_bid();
        Price best_ask_before = book.best_ask();
        Price spread_before = (best_bid_before > 0 && best_ask_before > 0) ? (best_ask_before - best_bid_before) : 0;
        bool crossed_before = (best_bid_before > 0 && best_ask_before > 0 && best_bid_before >= best_ask_before);

        // Track order state before apply
        bool bid_order_active_before = false;
        {
            Side s; Price p; Volume v;
            bid_order_active_before = book.get_order_info(bid_order_id, s, p, v);
        }
        bool ask_order_active_before = false;
        {
            Side s; Price p; Volume v;
            ask_order_active_before = book.get_order_info(ask_order_id, s, p, v);
        }

        // Track price level qty before
        bid_level_qty_before = book.level_qty(bid_target_price, Side::Buy);
        ask_level_qty_before = book.level_qty(ask_target_price, Side::Sell);

        // Detect mutation path for target orders
        std::string mutation_path;
        if (rec.order_id == bid_order_id) {
            if (rec.event == OLMsgType::Add) mutation_path = "bid_add";
            else if (rec.event == OLMsgType::Fill) mutation_path = "bid_fill";
            else if (rec.event == OLMsgType::Cancel) mutation_path = "bid_cancel";
            else if (rec.event == OLMsgType::Remove) mutation_path = "bid_remove";
            else if (rec.event == OLMsgType::Moved) mutation_path = "bid_move";
        } else if (rec.order_id == ask_order_id) {
            if (rec.event == OLMsgType::Add) mutation_path = "ask_add";
            else if (rec.event == OLMsgType::Fill) mutation_path = "ask_fill";
            else if (rec.event == OLMsgType::Cancel) mutation_path = "ask_cancel";
            else if (rec.event == OLMsgType::Remove) mutation_path = "ask_remove";
            else if (rec.event == OLMsgType::Moved) mutation_path = "ask_move";
        }
        // Also detect level-level events
        if (mutation_path.empty()) {
            if (rec.event == OLMsgType::Add && rec.side == Side::Buy && rec.price == bid_target_price) {
                mutation_path = "bid_level_add";
            } else if (rec.event == OLMsgType::Add && rec.side == Side::Sell && rec.price == ask_target_price) {
                mutation_path = "ask_level_add";
            }
        }

        // Apply to book
        book.apply(rec);

        // Track state after apply
        Price best_bid_after = book.best_bid();
        Price best_ask_after = book.best_ask();
        Price spread_after = (best_bid_after > 0 && best_ask_after > 0) ? (best_ask_after - best_bid_after) : 0;
        bool crossed_after = (best_bid_after > 0 && best_ask_after > 0 && best_bid_after >= best_ask_after);

        // Track order state after apply
        bool bid_order_active_after = false;
        {
            Side s; Price p; Volume v;
            bid_order_active_after = book.get_order_info(bid_order_id, s, p, v);
            if (bid_order_active_after) {
                bid_order_state.active = true;
                bid_order_state.price = p;
                bid_order_state.qty = v;
            }
        }
        bool ask_order_active_after = false;
        {
            Side s; Price p; Volume v;
            ask_order_active_after = book.get_order_info(ask_order_id, s, p, v);
            if (ask_order_active_after) {
                ask_order_state.active = true;
                ask_order_state.price = p;
                ask_order_state.qty = v;
            }
        }

        // Track order lifecycle transitions
        if (rec.order_id == bid_order_id) {
            bid_order_state.last_event_record = record_count;
            bid_order_state.last_event_type = std::string(ol_msg_type_name(rec.event));
            if (rec.event == OLMsgType::Fill && rec.amount_rest == 0) {
                bid_order_state.was_filled = true;
                bid_order_state.terminal_record = record_count;
            }
            if (rec.event == OLMsgType::Cancel) {
                bid_order_state.was_canceled = true;
                bid_order_state.terminal_record = record_count;
            }
            if (rec.event == OLMsgType::Remove) {
                bid_order_state.was_removed = true;
                bid_order_state.terminal_record = record_count;
            }
            if (!bid_order_active_after && bid_order_active_before) {
                bid_order_state.active = false;
                if (bid_order_state.terminal_record == 0) {
                    bid_order_state.terminal_record = record_count;
                }
            }
        }
        if (rec.order_id == ask_order_id) {
            ask_order_state.last_event_record = record_count;
            ask_order_state.last_event_type = std::string(ol_msg_type_name(rec.event));
            if (rec.event == OLMsgType::Fill && rec.amount_rest == 0) {
                ask_order_state.was_filled = true;
                ask_order_state.terminal_record = record_count;
            }
            if (rec.event == OLMsgType::Cancel) {
                ask_order_state.was_canceled = true;
                ask_order_state.terminal_record = record_count;
            }
            if (rec.event == OLMsgType::Remove) {
                ask_order_state.was_removed = true;
                ask_order_state.terminal_record = record_count;
            }
            if (!ask_order_active_after && ask_order_active_before) {
                ask_order_state.active = false;
                if (ask_order_state.terminal_record == 0) {
                    ask_order_state.terminal_record = record_count;
                }
            }
        }

        // Track crossing transitions
        if (crossed_after && !was_crossed) {
            // Entering crossed state
            ++crossed_segment_id;
            if (!first_crossing_found) {
                first_crossing_found = true;
                first_crossing_record = record_count;
                cross_start_bb = best_bid_after;
                cross_start_ba = best_ask_after;
                cross_start_tx = tx_counter;
            }
        }
        if (!crossed_after && was_crossed) {
            // Leaving crossed state
            if (!uncross_found) {
                uncross_found = true;
                first_uncross_record = record_count;
                uncross_bb = best_bid_after;
                uncross_ba = best_ask_after;
                uncross_tx = tx_counter;
            }
        }
        if (crossed_after) {
            ++crossed_records;
            if (has_flag(rec.order_flags, OLFlags::TxEnd)) {
                ++crossed_snapshots;
            }
        }
        was_crossed = crossed_after;

        // Output CSV for records in range
        if (record_count >= from_index) {
            bool is_snapshot = has_flag(rec.order_flags, OLFlags::Snapshot);
            bool is_txend = has_flag(rec.order_flags, OLFlags::TxEnd);

            csv << record_count << ","
                << tx_counter << ","
                << rec.timestamp << ","
                << ol_msg_type_name(rec.event) << ","
                << rec.order_id << ","
                << side_name(rec.side) << ","
                << rec.price << ","
                << rec.amount << ","
                << rec.amount_rest << ","
                << "0x" << std::hex << rec.order_flags << std::dec << ","
                << (is_snapshot ? 1 : 0) << ","
                << (is_txend ? 1 : 0) << ","
                << best_bid_before << ","
                << best_ask_before << ","
                << best_bid_after << ","
                << best_ask_after << ","
                << spread_before << ","
                << spread_after << ","
                << (crossed_before ? 1 : 0) << ","
                << (crossed_after ? 1 : 0) << ","
                << crossed_segment_id << ","
                << mutation_path
                << "\n";
            ++written;
        }

        if (max_records > 0 && record_count >= from_index + max_records) break;
    }

    csv.close();
    std::cout << "\nWrote " << written << " records to " << out_path << std::endl;

    // Print summary
    std::cout << "\n=== Crossed Persistence Audit Summary ===" << std::endl;
    std::cout << "from_index:                          " << from_index << std::endl;
    std::cout << "records_processed:                   " << record_count << std::endl;
    std::cout << "transactions_seen:                   " << tx_counter << std::endl;
    std::cout << "crossed_segments:                    " << crossed_segment_id << std::endl;

    std::cout << "\n--- Crossing Duration ---" << std::endl;
    std::cout << "first_crossing_record_index:         " << (first_crossing_found ? std::to_string(first_crossing_record) : "NOT FOUND") << std::endl;
    std::cout << "first_uncross_record_index:          " << (uncross_found ? std::to_string(first_uncross_record) : "NOT FOUND (crossing persists)") << std::endl;
    if (first_crossing_found) {
        std::cout << "records_crossed_duration:            " << (uncross_found ? std::to_string(first_uncross_record - first_crossing_record) : "PERSISTS (not cleared)") << std::endl;
        std::cout << "transactions_crossed_duration:       " << (uncross_found ? std::to_string(uncross_tx - cross_start_tx) : "PERSISTS") << std::endl;
        std::cout << "crossed_records_count:               " << crossed_records << std::endl;
        std::cout << "crossed_snapshots_count:             " << crossed_snapshots << std::endl;
    }
    std::cout << "best_bid_at_cross_start:             " << cross_start_bb << std::endl;
    std::cout << "best_ask_at_cross_start:             " << cross_start_ba << std::endl;
    if (uncross_found) {
        std::cout << "best_bid_at_uncross:                 " << uncross_bb << std::endl;
        std::cout << "best_ask_at_uncross:                 " << uncross_ba << std::endl;
    }

    std::cout << "\n--- Order Lifecycle ---" << std::endl;
    std::cout << "bid_order_id:                        " << bid_order_id << std::endl;
    std::cout << "bid_order_active_at_end:             " << (bid_order_state.active ? "YES" : "NO") << std::endl;
    std::cout << "bid_order_was_filled:                " << (bid_order_state.was_filled ? "YES" : "NO") << std::endl;
    std::cout << "bid_order_was_canceled:              " << (bid_order_state.was_canceled ? "YES" : "NO") << std::endl;
    std::cout << "bid_order_was_removed:               " << (bid_order_state.was_removed ? "YES" : "NO") << std::endl;
    std::cout << "bid_order_terminal_record:           " << (bid_order_state.terminal_record > 0 ? std::to_string(bid_order_state.terminal_record) : "N/A") << std::endl;
    std::cout << "bid_order_last_event:                " << bid_order_state.last_event_type << " at record " << bid_order_state.last_event_record << std::endl;

    std::cout << "ask_order_id:                        " << ask_order_id << std::endl;
    std::cout << "ask_order_active_at_end:             " << (ask_order_state.active ? "YES" : "NO") << std::endl;
    std::cout << "ask_order_was_filled:                " << (ask_order_state.was_filled ? "YES" : "NO") << std::endl;
    std::cout << "ask_order_was_canceled:              " << (ask_order_state.was_canceled ? "YES" : "NO") << std::endl;
    std::cout << "ask_order_was_removed:               " << (ask_order_state.was_removed ? "YES" : "NO") << std::endl;
    std::cout << "ask_order_terminal_record:           " << (ask_order_state.terminal_record > 0 ? std::to_string(ask_order_state.terminal_record) : "N/A") << std::endl;
    std::cout << "ask_order_last_event:                " << ask_order_state.last_event_type << " at record " << ask_order_state.last_event_record << std::endl;

    std::cout << "\n--- Classification ---" << std::endl;
    if (!first_crossing_found) {
        std::cout << "classification: E (inconclusive — no crossing found in range)" << std::endl;
    } else if (uncross_found && (first_uncross_record - first_crossing_record) <= 3) {
        std::cout << "classification: A (short raw transition between related events)" << std::endl;
    } else if (!uncross_found || (uncross_found && (first_uncross_record - first_crossing_record) > 100)) {
        std::cout << "classification: B (persistent crossed state over many records/snapshots)" << std::endl;
    } else {
        std::cout << "classification: E (inconclusive)" << std::endl;
    }

    return 0;
}

// M10N: First crossed-book root cause diagnostics
static int cmd_first_crossed_root_cause(const std::string& path, const std::string& out_dir, int context_events) {
    auto file = open_qsh_file(path);
    if (!file.valid) {
        std::cerr << "Error: " << file.error << std::endl;
        return 1;
    }

    if (file.header.stream != StreamType::OrderLog) {
        std::cerr << "Error: first-crossed-root-cause requires OrdLog stream, got "
                  << stream_type_name(file.header.stream) << std::endl;
        return 1;
    }

    std::cout << "Tracing first crossed-book root cause..." << std::endl;

    // Ring buffer for context events before first crossed
    struct ContextEvent {
        int64_t record_index;
        int64_t tx_index;
        Timestamp ts;
        OLMsgType event_type;
        UID order_id;
        Side side;
        Price price;
        Volume amount;
        Volume amount_rest;
        uint16_t flags;
        Volume repl_act;
    };
    std::deque<ContextEvent> ring;

    OrderBook book;
    OrdLogReader reader;
    OrderLogRecord rec;
    int64_t record_count = 0;
    int64_t tx_counter = 0;
    bool first_crossed_found = false;
    int64_t first_crossed_record = 0;
    int64_t post_crossed_count = 0;
    std::vector<ContextEvent> post_crossed_events;

    // Best bid/ask orders at first crossed
    Price first_crossed_bb = 0;
    Price first_crossed_ba = 0;
    std::vector<UID> first_crossed_bid_ids;
    std::vector<UID> first_crossed_ask_ids;

    // Captured order info at moment of first crossed detection
    struct BestOrderInfo {
        UID order_id = 0;
        Side side = Side::Unknown;
        Price price = 0;
        Volume qty = 0;
    };
    std::vector<BestOrderInfo> captured_bid_orders;
    std::vector<BestOrderInfo> captured_ask_orders;

    // Lifecycle tracking for best-level orders
    struct OrderLifecycle {
        UID order_id = 0;
        Side side = Side::Unknown;
        Price add_price = 0;
        Volume add_amount = 0;
        int64_t add_record = 0;
        Timestamp add_ts = 0;
        bool had_valid_add = false;
        std::vector<std::string> events;  // event descriptions
    };
    std::map<UID, OrderLifecycle> lifecycles;

    // Collect lifecycle events for best-level orders
    std::set<UID> tracked_orders;

    // M10O: Saved copies of lifecycle state at first-crossed detection
    // (protected from NewSession clearing during post-crossed collection)
    std::map<UID, OrderLifecycle> saved_lifecycles;
    std::set<UID> saved_tracked_orders;
    std::vector<UID> saved_first_crossed_bid_ids;
    std::vector<UID> saved_first_crossed_ask_ids;
    bool lifecycles_saved = false;

    while (reader.next(file, rec)) {
        ++record_count;

        if (has_flag(rec.order_flags, OLFlags::TxEnd)) {
            ++tx_counter;
        }

        if (has_flag(rec.order_flags, OLFlags::NewSession)) {
            book.clear();
            ring.clear();
            tracked_orders.clear();
            lifecycles.clear();
            continue;
        }

        if (!is_system_record(rec)) continue;

        // Track lifecycle of tracked orders
        if (tracked_orders.count(rec.order_id) > 0) {
            auto& lc = lifecycles[rec.order_id];
            std::ostringstream ev;
            ev << record_count << ":" << ol_msg_type_name(rec.event)
               << " p=" << rec.price << " a=" << rec.amount << " r=" << rec.amount_rest;
            lc.events.push_back(ev.str());
        }

        // Ring buffer: push current event
        ContextEvent ce;
        ce.record_index = record_count;
        ce.tx_index = tx_counter;
        ce.ts = rec.timestamp;
        ce.event_type = rec.event;
        ce.order_id = rec.order_id;
        ce.side = rec.side;
        ce.price = rec.price;
        ce.amount = rec.amount;
        ce.amount_rest = rec.amount_rest;
        ce.flags = rec.order_flags;
        ce.repl_act = has_flag(rec.order_flags, OLFlags::NonZeroReplAct) ? 1 : 0;
        ring.push_back(ce);
        while (static_cast<int>(ring.size()) > context_events) {
            ring.pop_front();
        }

        book.apply(rec);

        // Check for crossed book
        if (book.bid_depth() > 0 && book.ask_depth() > 0) {
            Price bb = book.best_bid();
            Price ba = book.best_ask();
            if (bb > 0 && ba > 0 && bb >= ba) {
                if (!first_crossed_found) {
                    first_crossed_found = true;
                    first_crossed_record = record_count;
                    first_crossed_bb = bb;
                    first_crossed_ba = ba;
                    first_crossed_bid_ids = book.best_bid_order_ids(20);
                    first_crossed_ask_ids = book.best_ask_order_ids(20);

                    // Capture order info at this moment (before loop continues)
                    for (UID id : first_crossed_bid_ids) {
                        BestOrderInfo info;
                        info.order_id = id;
                        book.get_order_info(id, info.side, info.price, info.qty);
                        captured_bid_orders.push_back(info);
                    }
                    for (UID id : first_crossed_ask_ids) {
                        BestOrderInfo info;
                        info.order_id = id;
                        book.get_order_info(id, info.side, info.price, info.qty);
                        captured_ask_orders.push_back(info);
                    }

                    // Start tracking best-level orders
                    for (UID id : first_crossed_bid_ids) {
                        tracked_orders.insert(id);
                        auto& lc = lifecycles[id];
                        lc.order_id = id;
                        lc.side = Side::Buy;
                    }
                    for (UID id : first_crossed_ask_ids) {
                        tracked_orders.insert(id);
                        auto& lc = lifecycles[id];
                        lc.order_id = id;
                        lc.side = Side::Sell;
                    }

                    // Check if tracked orders had valid ADD path
                    // We need a second pass for this — for now, mark as needing check

                    // M10O: Save lifecycle state immediately to protect from NewSession clearing
                    saved_lifecycles = lifecycles;
                    saved_tracked_orders = tracked_orders;
                    saved_first_crossed_bid_ids = first_crossed_bid_ids;
                    saved_first_crossed_ask_ids = first_crossed_ask_ids;
                    lifecycles_saved = true;
                }

                // Collect post-crossed events
                if (first_crossed_found && post_crossed_count < context_events) {
                    ++post_crossed_count;
                    post_crossed_events.push_back(ce);
                }
            }
        }

        // Track ADD events for lifecycles
        if (rec.event == OLMsgType::Add && tracked_orders.count(rec.order_id) > 0) {
            auto& lc = lifecycles[rec.order_id];
            if (!lc.had_valid_add) {
                lc.had_valid_add = true;
                lc.add_price = rec.price;
                lc.add_amount = rec.amount_rest;
                lc.add_record = record_count;
                lc.add_ts = rec.timestamp;
            }
        }
    }

    if (!first_crossed_found) {
        std::cout << "No crossed book found in the file." << std::endl;
        return 0;
    }

    std::cout << "\nFirst crossed book at record " << first_crossed_record << std::endl;
    std::cout << "  best_bid: " << first_crossed_bb << std::endl;
    std::cout << "  best_ask: " << first_crossed_ba << std::endl;

    // Use saved lifecycle state (protected from NewSession clearing)
    auto& final_lifecycles = lifecycles_saved ? saved_lifecycles : lifecycles;
    auto& final_tracked_orders = lifecycles_saved ? saved_tracked_orders : tracked_orders;
    auto& final_bid_ids = lifecycles_saved ? saved_first_crossed_bid_ids : first_crossed_bid_ids;
    auto& final_ask_ids = lifecycles_saved ? saved_first_crossed_ask_ids : first_crossed_ask_ids;

    // Second pass: check if best-level orders had prior ADD events
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
        if (record_count >= first_crossed_record) break;

        if (rec2.event == OLMsgType::Add && final_tracked_orders.count(rec2.order_id) > 0) {
            auto& lc = final_lifecycles[rec2.order_id];
            if (!lc.had_valid_add) {
                lc.had_valid_add = true;
                lc.add_price = rec2.price;
                lc.add_amount = rec2.amount_rest;
                lc.add_record = record_count;
                lc.add_ts = rec2.timestamp;
            }
        }
    }

    // Create report directory
    if (!out_dir.empty()) {
        std::filesystem::create_directories(out_dir);
    }

    // Write first_crossed_root_cause.csv
    {
        std::string csv_path = out_dir.empty() ? "first_crossed_root_cause.csv" : out_dir + "/first_crossed_root_cause.csv";
        std::ofstream csv(csv_path);
        if (csv.is_open()) {
            csv << "first_crossed_book_record_index\n";
            csv << first_crossed_record << "\n";
            csv.close();
            std::cout << "Wrote root cause to " << csv_path << std::endl;
        }
    }

    // M10O: Write first_crossed_event_window_audit.csv (ring buffer context around first crossing)
    {
        std::string csv_path = out_dir.empty() ? "first_crossed_event_window_audit.csv" : out_dir + "/first_crossed_event_window_audit.csv";
        std::ofstream csv(csv_path);
        if (csv.is_open()) {
            csv << "record_index,ts,tx_index,event_type,order_id,side,price,qty,amount_rest,flags_hex,repl_act,is_before_crossing\n";
            for (const auto& ce : ring) {
                csv << ce.record_index << "," << ce.ts << "," << ce.tx_index << ","
                    << ol_msg_type_name(ce.event_type) << "," << ce.order_id << ","
                    << side_name(ce.side) << "," << ce.price << "," << ce.amount << ","
                    << ce.amount_rest << ",0x" << std::hex << ce.flags << std::dec << ","
                    << ce.repl_act << ",true\n";
            }
            for (const auto& ce : post_crossed_events) {
                csv << ce.record_index << "," << ce.ts << "," << ce.tx_index << ","
                    << ol_msg_type_name(ce.event_type) << "," << ce.order_id << ","
                    << side_name(ce.side) << "," << ce.price << "," << ce.amount << ","
                    << ce.amount_rest << ",0x" << std::hex << ce.flags << std::dec << ","
                    << ce.repl_act << ",false\n";
            }
            csv.close();
            std::cout << "Wrote event window audit (" << (ring.size() + post_crossed_events.size())
                      << " events) to " << csv_path << std::endl;
        }
    }

    // M10O: Write first_crossed_snapshot_window_audit.csv (same data, snapshot-focused)
    {
        std::string csv_path = out_dir.empty() ? "first_crossed_snapshot_window_audit.csv" : out_dir + "/first_crossed_snapshot_window_audit.csv";
        std::ofstream csv(csv_path);
        if (csv.is_open()) {
            csv << "record_index,ts,tx_index,event_type,order_id,side,price,qty,amount_rest,flags_hex,repl_act\n";
            // Write all context events
            for (const auto& ce : ring) {
                csv << ce.record_index << "," << ce.ts << "," << ce.tx_index << ","
                    << ol_msg_type_name(ce.event_type) << "," << ce.order_id << ","
                    << side_name(ce.side) << "," << ce.price << "," << ce.amount << ","
                    << ce.amount_rest << ",0x" << std::hex << ce.flags << std::dec << ","
                    << ce.repl_act << "\n";
            }
            for (const auto& ce : post_crossed_events) {
                csv << ce.record_index << "," << ce.ts << "," << ce.tx_index << ","
                    << ol_msg_type_name(ce.event_type) << "," << ce.order_id << ","
                    << side_name(ce.side) << "," << ce.price << "," << ce.amount << ","
                    << ce.amount_rest << ",0x" << std::hex << ce.flags << std::dec << ","
                    << ce.repl_act << "\n";
            }
            csv.close();
            std::cout << "Wrote snapshot window audit to " << csv_path << std::endl;
        }
    }

    // Write first_crossed_best_orders.csv (using captured info at moment of detection)
    {
        std::string csv_path = out_dir.empty() ? "first_crossed_best_orders.csv" : out_dir + "/first_crossed_best_orders.csv";
        std::ofstream csv(csv_path);
        if (csv.is_open()) {
            csv << "side,order_id,price,qty_at_crossed\n";
            for (const auto& info : captured_bid_orders) {
                csv << side_name(info.side) << "," << info.order_id << "," << info.price << "," << info.qty << "\n";
            }
            for (const auto& info : captured_ask_orders) {
                csv << side_name(info.side) << "," << info.order_id << "," << info.price << "," << info.qty << "\n";
            }
            csv.close();
            std::cout << "Wrote best orders to " << csv_path << std::endl;
        }
    }

    // Write first_crossed_lifecycle.csv
    {
        std::string csv_path = out_dir.empty() ? "first_crossed_lifecycle.csv" : out_dir + "/first_crossed_lifecycle.csv";
        std::ofstream csv(csv_path);
        if (csv.is_open()) {
            csv << "order_id,side,had_valid_add,add_record,add_ts,add_price,add_amount,event_count,events\n";
            for (const auto& [id, lc] : lifecycles) {
                csv << lc.order_id << ","
                    << side_name(lc.side) << ","
                    << (lc.had_valid_add ? "YES" : "NO") << ","
                    << lc.add_record << ","
                    << lc.add_ts << ","
                    << lc.add_price << ","
                    << lc.add_amount << ","
                    << lc.events.size() << ",";
                for (size_t i = 0; i < lc.events.size(); ++i) {
                    if (i > 0) csv << ";";
                    csv << lc.events[i];
                }
                csv << "\n";
            }
            csv.close();
            std::cout << "Wrote lifecycle to " << csv_path << std::endl;
        }
    }

    // M10O: Third pass — produce detailed lifecycle CSVs for crossing orders
    // Collect lifecycle events with full audit columns
    struct LifecycleEvent {
        int64_t record_index;
        Timestamp ts;
        int64_t tx_index;
        UID order_id;
        std::string event_type;
        std::string side;
        Price price;
        Volume amount;
        Volume amount_rest;
        std::string flags_hex;
        Volume repl_act;
        bool is_snapshot;
        bool is_new_session;
        bool is_txend;
        bool is_system;
        bool is_non_system;
        int64_t raw_data_offset;
        int raw_entry_flags;
        std::string raw_order_flags_hex;
        std::string raw_side_bits;
        std::string raw_event_bits;
        bool has_timestamp_field;
        bool has_order_id_field;
        bool has_price_field;
        bool has_amount_field;
        int64_t order_id_before_delta;
        int64_t order_id_after_delta;
        int64_t price_before_delta;
        int64_t price_after_delta;
        int64_t ts_before_delta;
        int64_t ts_after_delta;
        Volume amount_before;
        Volume amount_after;
        std::string order_id_path;
        Price best_bid_before;
        Price best_ask_before;
        Price best_bid_after;
        Price best_ask_after;
        Volume active_qty_before;
        Volume active_qty_after;
    };

    // Do a third pass for detailed lifecycle
    file = open_qsh_file(path);
    if (!file.valid) {
        std::cerr << "Error: " << file.error << std::endl;
        return 1;
    }

    // Build set of tracked order IDs
    std::set<UID> lifecycle_order_ids;
    for (const auto& [id, _] : final_lifecycles) {
        lifecycle_order_ids.insert(id);
    }

    std::vector<LifecycleEvent> bid_lifecycle_events;
    std::vector<LifecycleEvent> ask_lifecycle_events;
    std::vector<LifecycleEvent> combined_lifecycle_events;

    // Separate bid and ask order IDs
    std::set<UID> bid_order_set(final_bid_ids.begin(), final_bid_ids.end());
    std::set<UID> ask_order_set(final_ask_ids.begin(), final_ask_ids.end());

    OrdLogReader reader3;
    OrderLogRecord rec3;
    record_count = 0;
    tx_counter = 0;
    OrderBook lifecycle_book;

    while (reader3.next_debug(file, rec3)) {
        ++record_count;

        if (has_flag(rec3.order_flags, OLFlags::TxEnd)) {
            ++tx_counter;
        }

        if (has_flag(rec3.order_flags, OLFlags::NewSession)) {
            lifecycle_book.clear();
            continue;
        }

        if (!is_system_record(rec3)) continue;

        // Check if this order is one we're tracking
        bool is_tracked = lifecycle_order_ids.count(rec3.order_id) > 0;

        if (is_tracked) {
            // Capture book state before apply
            Price bb_before = lifecycle_book.best_bid();
            Price ba_before = lifecycle_book.best_ask();
            Volume active_qty_before = 0;
            Side dummy_side;
            Price dummy_price;
            lifecycle_book.get_order_info(rec3.order_id, dummy_side, dummy_price, active_qty_before);

            lifecycle_book.apply(rec3);

            // Capture book state after apply
            Price bb_after = lifecycle_book.best_bid();
            Price ba_after = lifecycle_book.best_ask();
            Volume active_qty_after = 0;
            lifecycle_book.get_order_info(rec3.order_id, dummy_side, dummy_price, active_qty_after);

            // Build lifecycle event
            LifecycleEvent le;
            le.record_index = record_count;
            le.ts = rec3.timestamp;
            le.tx_index = tx_counter;
            le.order_id = rec3.order_id;
            le.event_type = ol_msg_type_name(rec3.event);
            le.side = side_name(rec3.side);
            le.price = rec3.price;
            le.amount = rec3.amount;
            le.amount_rest = rec3.amount_rest;

            // Flags
            std::ostringstream flags_ss;
            flags_ss << "0x" << std::hex << rec3.order_flags;
            le.flags_hex = flags_ss.str();
            le.repl_act = has_flag(rec3.order_flags, OLFlags::NonZeroReplAct) ? 1 : 0;
            le.is_snapshot = has_flag(rec3.order_flags, OLFlags::Snapshot);
            le.is_new_session = has_flag(rec3.order_flags, OLFlags::NewSession);
            le.is_txend = has_flag(rec3.order_flags, OLFlags::TxEnd);
            le.is_system = is_system_record(rec3);
            le.is_non_system = has_flag(rec3.order_flags, OLFlags::NonSystem);

            // Raw decoder audit fields
            le.raw_data_offset = rec3.debug.raw_data_offset;
            le.raw_entry_flags = static_cast<int>(rec3.debug.raw_entry_flags);
            std::ostringstream raw_flags_ss;
            raw_flags_ss << "0x" << std::hex << rec3.debug.raw_order_flags;
            le.raw_order_flags_hex = raw_flags_ss.str();

            uint16_t side_bits = rec3.order_flags & (OLFlags::Buy | OLFlags::Sell);
            uint16_t event_bits = rec3.order_flags & (OLFlags::Add | OLFlags::Fill | OLFlags::Moved |
                OLFlags::Canceled | OLFlags::CanceledGroup | OLFlags::CrossTrade);
            std::ostringstream side_bits_ss;
            side_bits_ss << "0x" << std::hex << side_bits;
            le.raw_side_bits = side_bits_ss.str();
            std::ostringstream event_bits_ss;
            event_bits_ss << "0x" << std::hex << event_bits;
            le.raw_event_bits = event_bits_ss.str();

            le.has_timestamp_field = rec3.debug.has_timestamp_field;
            le.has_order_id_field = rec3.debug.has_order_id_field;
            le.has_price_field = rec3.debug.has_price_field;
            le.has_amount_field = rec3.debug.has_amount_field;

            // Delta fields
            le.order_id_before_delta = rec3.debug.order_id_before_delta;
            le.order_id_after_delta = rec3.debug.order_id_after_delta;
            le.price_before_delta = rec3.debug.price_before_delta;
            le.price_after_delta = rec3.debug.price_after_delta;
            le.ts_before_delta = rec3.debug.ts_before_delta;
            le.ts_after_delta = rec3.debug.ts_after_delta;
            le.amount_before = rec3.debug.amount_before;
            le.amount_after = rec3.debug.amount_after;
            le.order_id_path = rec3.debug.is_add_order_id_path ? "growing" : "leb128";

            // Book state
            le.best_bid_before = bb_before;
            le.best_ask_before = ba_before;
            le.best_bid_after = bb_after;
            le.best_ask_after = ba_after;
            le.active_qty_before = active_qty_before;
            le.active_qty_after = active_qty_after;

            // Add to appropriate collections
            if (bid_order_set.count(rec3.order_id) > 0) {
                bid_lifecycle_events.push_back(le);
            }
            if (ask_order_set.count(rec3.order_id) > 0) {
                ask_lifecycle_events.push_back(le);
            }
            combined_lifecycle_events.push_back(le);
        } else {
            lifecycle_book.apply(rec3);
        }

        // Stop after crossing record + some margin
        if (record_count > first_crossed_record + 1000) break;
    }

    // Write bid order lifecycle CSV
    {
        std::string csv_path = out_dir.empty() ? "first_crossed_bid_order_lifecycle.csv" : out_dir + "/first_crossed_bid_order_lifecycle.csv";
        std::ofstream csv(csv_path);
        if (csv.is_open()) {
            csv << "record_index,ts,tx_index,order_id,event_type,side,price,amount,amount_rest,"
                << "flags_hex,repl_act,is_snapshot,is_new_session,is_txend,is_system,is_non_system,"
                << "raw_data_offset,raw_entry_flags,raw_order_flags_hex,raw_side_bits,raw_event_bits,"
                << "has_timestamp_field,has_order_id_field,has_price_field,has_amount_field,"
                << "order_id_before_delta,order_id_after_delta,order_id_delta,"
                << "price_before_delta,price_after_delta,price_delta,"
                << "ts_before_delta,ts_after_delta,"
                << "amount_before,amount_after,order_id_path,"
                << "best_bid_before,best_ask_before,best_bid_after,best_ask_after,"
                << "active_qty_before,active_qty_after\n";
            for (const auto& le : bid_lifecycle_events) {
                csv << le.record_index << "," << le.ts << "," << le.tx_index << ","
                    << le.order_id << "," << le.event_type << "," << le.side << ","
                    << le.price << "," << le.amount << "," << le.amount_rest << ","
                    << le.flags_hex << "," << le.repl_act << ","
                    << (le.is_snapshot ? 1 : 0) << "," << (le.is_new_session ? 1 : 0) << ","
                    << (le.is_txend ? 1 : 0) << "," << (le.is_system ? 1 : 0) << ","
                    << (le.is_non_system ? 1 : 0) << ","
                    << le.raw_data_offset << "," << le.raw_entry_flags << ","
                    << le.raw_order_flags_hex << "," << le.raw_side_bits << "," << le.raw_event_bits << ","
                    << (le.has_timestamp_field ? 1 : 0) << "," << (le.has_order_id_field ? 1 : 0) << ","
                    << (le.has_price_field ? 1 : 0) << "," << (le.has_amount_field ? 1 : 0) << ","
                    << le.order_id_before_delta << "," << le.order_id_after_delta << ","
                    << (le.order_id_after_delta - le.order_id_before_delta) << ","
                    << le.price_before_delta << "," << le.price_after_delta << ","
                    << (le.price_after_delta - le.price_before_delta) << ","
                    << le.ts_before_delta << "," << le.ts_after_delta << ","
                    << le.amount_before << "," << le.amount_after << "," << le.order_id_path << ","
                    << le.best_bid_before << "," << le.best_ask_before << ","
                    << le.best_bid_after << "," << le.best_ask_after << ","
                    << le.active_qty_before << "," << le.active_qty_after << "\n";
            }
            csv.close();
            std::cout << "Wrote bid order lifecycle (" << bid_lifecycle_events.size() << " events) to " << csv_path << std::endl;
        }
    }

    // Write ask order lifecycle CSV
    {
        std::string csv_path = out_dir.empty() ? "first_crossed_ask_order_lifecycle.csv" : out_dir + "/first_crossed_ask_order_lifecycle.csv";
        std::ofstream csv(csv_path);
        if (csv.is_open()) {
            csv << "record_index,ts,tx_index,order_id,event_type,side,price,amount,amount_rest,"
                << "flags_hex,repl_act,is_snapshot,is_new_session,is_txend,is_system,is_non_system,"
                << "raw_data_offset,raw_entry_flags,raw_order_flags_hex,raw_side_bits,raw_event_bits,"
                << "has_timestamp_field,has_order_id_field,has_price_field,has_amount_field,"
                << "order_id_before_delta,order_id_after_delta,order_id_delta,"
                << "price_before_delta,price_after_delta,price_delta,"
                << "ts_before_delta,ts_after_delta,"
                << "amount_before,amount_after,order_id_path,"
                << "best_bid_before,best_ask_before,best_bid_after,best_ask_after,"
                << "active_qty_before,active_qty_after\n";
            for (const auto& le : ask_lifecycle_events) {
                csv << le.record_index << "," << le.ts << "," << le.tx_index << ","
                    << le.order_id << "," << le.event_type << "," << le.side << ","
                    << le.price << "," << le.amount << "," << le.amount_rest << ","
                    << le.flags_hex << "," << le.repl_act << ","
                    << (le.is_snapshot ? 1 : 0) << "," << (le.is_new_session ? 1 : 0) << ","
                    << (le.is_txend ? 1 : 0) << "," << (le.is_system ? 1 : 0) << ","
                    << (le.is_non_system ? 1 : 0) << ","
                    << le.raw_data_offset << "," << le.raw_entry_flags << ","
                    << le.raw_order_flags_hex << "," << le.raw_side_bits << "," << le.raw_event_bits << ","
                    << (le.has_timestamp_field ? 1 : 0) << "," << (le.has_order_id_field ? 1 : 0) << ","
                    << (le.has_price_field ? 1 : 0) << "," << (le.has_amount_field ? 1 : 0) << ","
                    << le.order_id_before_delta << "," << le.order_id_after_delta << ","
                    << (le.order_id_after_delta - le.order_id_before_delta) << ","
                    << le.price_before_delta << "," << le.price_after_delta << ","
                    << (le.price_after_delta - le.price_before_delta) << ","
                    << le.ts_before_delta << "," << le.ts_after_delta << ","
                    << le.amount_before << "," << le.amount_after << "," << le.order_id_path << ","
                    << le.best_bid_before << "," << le.best_ask_before << ","
                    << le.best_bid_after << "," << le.best_ask_after << ","
                    << le.active_qty_before << "," << le.active_qty_after << "\n";
            }
            csv.close();
            std::cout << "Wrote ask order lifecycle (" << ask_lifecycle_events.size() << " events) to " << csv_path << std::endl;
        }
    }

    // Write combined lifecycle CSV
    {
        std::string csv_path = out_dir.empty() ? "first_crossed_orders_lifecycle_combined.csv" : out_dir + "/first_crossed_orders_lifecycle_combined.csv";
        std::ofstream csv(csv_path);
        if (csv.is_open()) {
            csv << "record_index,ts,tx_index,order_id,event_type,side,price,amount,amount_rest,"
                << "flags_hex,repl_act,is_snapshot,is_new_session,is_txend,is_system,is_non_system,"
                << "raw_data_offset,raw_entry_flags,raw_order_flags_hex,raw_side_bits,raw_event_bits,"
                << "has_timestamp_field,has_order_id_field,has_price_field,has_amount_field,"
                << "order_id_before_delta,order_id_after_delta,order_id_delta,"
                << "price_before_delta,price_after_delta,price_delta,"
                << "ts_before_delta,ts_after_delta,"
                << "amount_before,amount_after,order_id_path,"
                << "best_bid_before,best_ask_before,best_bid_after,best_ask_after,"
                << "active_qty_before,active_qty_after\n";
            for (const auto& le : combined_lifecycle_events) {
                csv << le.record_index << "," << le.ts << "," << le.tx_index << ","
                    << le.order_id << "," << le.event_type << "," << le.side << ","
                    << le.price << "," << le.amount << "," << le.amount_rest << ","
                    << le.flags_hex << "," << le.repl_act << ","
                    << (le.is_snapshot ? 1 : 0) << "," << (le.is_new_session ? 1 : 0) << ","
                    << (le.is_txend ? 1 : 0) << "," << (le.is_system ? 1 : 0) << ","
                    << (le.is_non_system ? 1 : 0) << ","
                    << le.raw_data_offset << "," << le.raw_entry_flags << ","
                    << le.raw_order_flags_hex << "," << le.raw_side_bits << "," << le.raw_event_bits << ","
                    << (le.has_timestamp_field ? 1 : 0) << "," << (le.has_order_id_field ? 1 : 0) << ","
                    << (le.has_price_field ? 1 : 0) << "," << (le.has_amount_field ? 1 : 0) << ","
                    << le.order_id_before_delta << "," << le.order_id_after_delta << ","
                    << (le.order_id_after_delta - le.order_id_before_delta) << ","
                    << le.price_before_delta << "," << le.price_after_delta << ","
                    << (le.price_after_delta - le.price_before_delta) << ","
                    << le.ts_before_delta << "," << le.ts_after_delta << ","
                    << le.amount_before << "," << le.amount_after << "," << le.order_id_path << ","
                    << le.best_bid_before << "," << le.best_ask_before << ","
                    << le.best_bid_after << "," << le.best_ask_after << ","
                    << le.active_qty_before << "," << le.active_qty_after << "\n";
            }
            csv.close();
            std::cout << "Wrote combined lifecycle (" << combined_lifecycle_events.size() << " events) to " << csv_path << std::endl;
        }
    }

    // M10O: Write first_crossed_orders_raw_audit.csv (raw decoder state for crossing orders)
    {
        std::string csv_path = out_dir.empty() ? "first_crossed_orders_raw_audit.csv" : out_dir + "/first_crossed_orders_raw_audit.csv";
        std::ofstream csv(csv_path);
        if (csv.is_open()) {
            csv << "record_index,ts,tx_index,order_id,event_type,side,price,amount,amount_rest,"
                << "flags_hex,repl_act,is_snapshot,is_new_session,is_txend,is_system,is_non_system,"
                << "raw_data_offset,raw_entry_flags,raw_order_flags_hex,raw_side_bits,raw_event_bits,"
                << "has_timestamp_field,has_order_id_field,has_price_field,has_amount_field,"
                << "order_id_before_delta,order_id_after_delta,price_before_delta,price_after_delta,"
                << "ts_before_delta,ts_after_delta,amount_before,amount_after,order_id_path\n";
            for (const auto& le : combined_lifecycle_events) {
                csv << le.record_index << "," << le.ts << "," << le.tx_index << ","
                    << le.order_id << "," << le.event_type << "," << le.side << ","
                    << le.price << "," << le.amount << "," << le.amount_rest << ","
                    << le.flags_hex << "," << le.repl_act << ","
                    << (le.is_snapshot ? 1 : 0) << "," << (le.is_new_session ? 1 : 0) << ","
                    << (le.is_txend ? 1 : 0) << "," << (le.is_system ? 1 : 0) << ","
                    << (le.is_non_system ? 1 : 0) << ","
                    << le.raw_data_offset << "," << le.raw_entry_flags << ","
                    << le.raw_order_flags_hex << "," << le.raw_side_bits << "," << le.raw_event_bits << ","
                    << (le.has_timestamp_field ? 1 : 0) << "," << (le.has_order_id_field ? 1 : 0) << ","
                    << (le.has_price_field ? 1 : 0) << "," << (le.has_amount_field ? 1 : 0) << ","
                    << le.order_id_before_delta << "," << le.order_id_after_delta << ","
                    << le.price_before_delta << "," << le.price_after_delta << ","
                    << le.ts_before_delta << "," << le.ts_after_delta << ","
                    << le.amount_before << "," << le.amount_after << "," << le.order_id_path << "\n";
            }
            csv.close();
            std::cout << "Wrote raw audit (" << combined_lifecycle_events.size() << " events) to " << csv_path << std::endl;
        }
    }

    // Print summary
    std::cout << "\nFirst Crossed Root Cause Summary:" << std::endl;
    std::cout << "  First crossed record:     " << first_crossed_record << std::endl;
    std::cout << "  Best bid:                 " << first_crossed_bb << std::endl;
    std::cout << "  Best ask:                 " << first_crossed_ba << std::endl;
    std::cout << "  Best bid order count:     " << final_bid_ids.size() << std::endl;
    std::cout << "  Best ask order count:     " << final_ask_ids.size() << std::endl;

    int64_t valid_add_count = 0;
    int64_t no_add_count = 0;
    for (const auto& [id, lc] : final_lifecycles) {
        if (lc.had_valid_add) ++valid_add_count;
        else ++no_add_count;
    }
    std::cout << "  Orders with valid ADD:    " << valid_add_count << std::endl;
    std::cout << "  Orders without ADD:       " << no_add_count << std::endl;

    // M10O: Print lifecycle summary
    std::cout << "\nLifecycle trace summary:" << std::endl;
    std::cout << "  Bid order lifecycle events: " << bid_lifecycle_events.size() << std::endl;
    std::cout << "  Ask order lifecycle events: " << ask_lifecycle_events.size() << std::endl;
    std::cout << "  Combined lifecycle events:  " << combined_lifecycle_events.size() << std::endl;

    // M10O: Answer lifecycle questions
    std::cout << "\nLifecycle questions:" << std::endl;
    // Check if ADD was system or non-system
    for (const auto& [id, lc] : final_lifecycles) {
        std::cout << "  Order " << id << ":" << std::endl;
        std::cout << "    Had valid ADD: " << (lc.had_valid_add ? "YES" : "NO") << std::endl;
        if (lc.had_valid_add) {
            std::cout << "    ADD at record: " << lc.add_record << std::endl;
            std::cout << "    ADD price:     " << lc.add_price << std::endl;
            std::cout << "    ADD amount:    " << lc.add_amount << std::endl;
        }
        std::cout << "    Event count:   " << lc.events.size() << std::endl;
    }

    return 0;
}

// M10S: Counter flag audit — count all Counter-flagged events and measure book impact
static int cmd_counter_flag_audit(const std::string& path, const std::string& out_path, int64_t max_records) {
    auto file = open_qsh_file(path);
    if (!file.valid) {
        std::cerr << "Error: " << file.error << std::endl;
        return 1;
    }

    if (file.header.stream != StreamType::OrderLog) {
        std::cerr << "Error: counter-flag-audit requires OrdLog stream, got "
                  << stream_type_name(file.header.stream) << std::endl;
        return 1;
    }

    std::cout << "Running counter flag audit..." << std::endl;

    // Counters
    int64_t counter_records_total = 0;
    int64_t counter_add = 0;
    int64_t counter_fill = 0;
    int64_t counter_cancel = 0;
    int64_t counter_remove = 0;
    int64_t counter_move = 0;
    int64_t counter_buy = 0;
    int64_t counter_sell = 0;
    int64_t counter_snapshot = 0;
    int64_t counter_new_session = 0;
    int64_t counter_txend = 0;
    int64_t counter_non_system = 0;
    int64_t counter_cross_trade = 0;
    int64_t first_counter_record_index = 0;
    int64_t first_counter_add_record_index = 0;

    // Book impact counters
    int64_t counter_events_that_create_new_best_bid = 0;
    int64_t counter_events_that_create_new_best_ask = 0;
    int64_t counter_events_that_create_crossed_book = 0;
    int64_t counter_events_inside_crossed_state = 0;
    int64_t counter_events_that_uncross_book = 0;
    int64_t first_counter_crossing_record_index = 0;

    // Record 2136/2137 inspection
    bool record_2136_found = false;
    bool record_2137_found = false;
    uint16_t record_2136_flags = 0;
    uint16_t record_2137_flags = 0;
    bool record_2136_is_counter = false;
    bool record_2137_is_counter = false;

    // CSV output
    std::ofstream csv;
    bool write_csv = !out_path.empty();
    if (write_csv) {
        csv.open(out_path);
        if (!csv.is_open()) {
            std::cerr << "Warning: cannot open output file: " << out_path << std::endl;
            write_csv = false;
        } else {
            csv << "record_index,ts,tx_index,order_id,event_type,side,price,amount,amount_rest,"
                << "flags_hex,is_counter,is_snapshot,is_new_session,is_txend,is_system,is_non_system,"
                << "best_bid_before,best_ask_before,best_bid_after,best_ask_after,"
                << "crossed_before,crossed_after,creates_new_best_bid,creates_new_best_ask,"
                << "creates_crossed,inside_crossed,uncrosses\n";
        }
    }

    OrderBook book;
    OrdLogReader reader;
    OrderLogRecord rec;
    int64_t record_count = 0;
    int64_t tx_counter = 0;
    int64_t written = 0;

    while (reader.next(file, rec)) {
        ++record_count;

        if (has_flag(rec.order_flags, OLFlags::TxEnd)) {
            ++tx_counter;
        }

        bool is_counter = has_flag(rec.order_flags, OLFlags::Counter);

        // Track records 2136 and 2137
        if (record_count == 2136) {
            record_2136_found = true;
            record_2136_flags = rec.order_flags;
            record_2136_is_counter = is_counter;
        }
        if (record_count == 2137) {
            record_2137_found = true;
            record_2137_flags = rec.order_flags;
            record_2137_is_counter = is_counter;
        }

        if (is_counter) {
            ++counter_records_total;
            if (first_counter_record_index == 0) first_counter_record_index = record_count;

            if (rec.event == OLMsgType::Add) {
                ++counter_add;
                if (first_counter_add_record_index == 0) first_counter_add_record_index = record_count;
            }
            if (rec.event == OLMsgType::Fill)   ++counter_fill;
            if (rec.event == OLMsgType::Cancel) ++counter_cancel;
            if (rec.event == OLMsgType::Remove) ++counter_remove;
            if (rec.event == OLMsgType::Moved)  ++counter_move;
            if (rec.side == Side::Buy)          ++counter_buy;
            if (rec.side == Side::Sell)         ++counter_sell;
            if (has_flag(rec.order_flags, OLFlags::Snapshot))   ++counter_snapshot;
            if (has_flag(rec.order_flags, OLFlags::NewSession)) ++counter_new_session;
            if (has_flag(rec.order_flags, OLFlags::TxEnd))      ++counter_txend;
            if (has_flag(rec.order_flags, OLFlags::NonSystem))  ++counter_non_system;
            if (has_flag(rec.order_flags, OLFlags::CrossTrade)) ++counter_cross_trade;

            // Book impact measurement
            Price best_bid_before = book.best_bid();
            Price best_ask_before = book.best_ask();
            bool crossed_before = (best_bid_before > 0 && best_ask_before > 0 && best_bid_before >= best_ask_before);

            book.apply(rec);

            Price best_bid_after = book.best_bid();
            Price best_ask_after = book.best_ask();
            bool crossed_after = (best_bid_after > 0 && best_ask_after > 0 && best_bid_after >= best_ask_after);

            bool creates_new_best_bid = (best_bid_after != best_bid_before && best_bid_after > 0);
            bool creates_new_best_ask = (best_ask_after != best_ask_before && best_ask_after > 0);
            bool creates_crossed = (crossed_after && !crossed_before);
            bool inside_crossed = crossed_before && crossed_after;
            bool uncrosses = (!crossed_after && crossed_before);

            if (creates_new_best_bid) ++counter_events_that_create_new_best_bid;
            if (creates_new_best_ask) ++counter_events_that_create_new_best_ask;
            if (creates_crossed) {
                ++counter_events_that_create_crossed_book;
                if (first_counter_crossing_record_index == 0) first_counter_crossing_record_index = record_count;
            }
            if (inside_crossed) ++counter_events_inside_crossed_state;
            if (uncrosses)      ++counter_events_that_uncross_book;

            if (write_csv && (max_records <= 0 || written < max_records)) {
                bool is_snapshot = has_flag(rec.order_flags, OLFlags::Snapshot);
                bool is_new_session = has_flag(rec.order_flags, OLFlags::NewSession);
                bool is_txend = has_flag(rec.order_flags, OLFlags::TxEnd);
                bool is_system = is_system_record(rec);
                bool is_non_system = has_flag(rec.order_flags, OLFlags::NonSystem);

                csv << record_count << ","
                    << rec.timestamp << ","
                    << tx_counter << ","
                    << rec.order_id << ","
                    << ol_msg_type_name(rec.event) << ","
                    << side_name(rec.side) << ","
                    << rec.price << ","
                    << rec.amount << ","
                    << rec.amount_rest << ","
                    << "0x" << std::hex << rec.order_flags << std::dec << ","
                    << 1 << ","
                    << (is_snapshot ? 1 : 0) << ","
                    << (is_new_session ? 1 : 0) << ","
                    << (is_txend ? 1 : 0) << ","
                    << (is_system ? 1 : 0) << ","
                    << (is_non_system ? 1 : 0) << ","
                    << best_bid_before << ","
                    << best_ask_before << ","
                    << best_bid_after << ","
                    << best_ask_after << ","
                    << (crossed_before ? 1 : 0) << ","
                    << (crossed_after ? 1 : 0) << ","
                    << (creates_new_best_bid ? 1 : 0) << ","
                    << (creates_new_best_ask ? 1 : 0) << ","
                    << (creates_crossed ? 1 : 0) << ","
                    << (inside_crossed ? 1 : 0) << ","
                    << (uncrosses ? 1 : 0) << "\n";
                ++written;
            }
        } else {
            // Still apply non-Counter events to maintain accurate book state
            book.apply(rec);
        }

        if (max_records > 0 && record_count >= max_records) break;
    }

    if (write_csv) {
        csv.close();
        std::cout << "Wrote " << written << " counter records to " << out_path << std::endl;
    }

    // Print summary
    std::cout << "\n=== Counter Flag Audit Summary ===" << std::endl;
    std::cout << "counter_records_total:                  " << counter_records_total << std::endl;
    std::cout << "counter_add:                            " << counter_add << std::endl;
    std::cout << "counter_fill:                           " << counter_fill << std::endl;
    std::cout << "counter_cancel:                         " << counter_cancel << std::endl;
    std::cout << "counter_remove:                         " << counter_remove << std::endl;
    std::cout << "counter_move:                           " << counter_move << std::endl;
    std::cout << "counter_buy:                            " << counter_buy << std::endl;
    std::cout << "counter_sell:                           " << counter_sell << std::endl;
    std::cout << "counter_snapshot:                       " << counter_snapshot << std::endl;
    std::cout << "counter_new_session:                    " << counter_new_session << std::endl;
    std::cout << "counter_txend:                          " << counter_txend << std::endl;
    std::cout << "counter_non_system:                     " << counter_non_system << std::endl;
    std::cout << "counter_cross_trade:                    " << counter_cross_trade << std::endl;
    std::cout << "first_counter_record_index:             " << first_counter_record_index << std::endl;
    std::cout << "first_counter_add_record_index:         " << first_counter_add_record_index << std::endl;

    std::cout << "\n--- Book Impact ---" << std::endl;
    std::cout << "counter_events_that_create_new_best_bid: " << counter_events_that_create_new_best_bid << std::endl;
    std::cout << "counter_events_that_create_new_best_ask: " << counter_events_that_create_new_best_ask << std::endl;
    std::cout << "counter_events_that_create_crossed_book: " << counter_events_that_create_crossed_book << std::endl;
    std::cout << "counter_events_inside_crossed_state:     " << counter_events_inside_crossed_state << std::endl;
    std::cout << "counter_events_that_uncross_book:        " << counter_events_that_uncross_book << std::endl;
    std::cout << "first_counter_crossing_record_index:     " << first_counter_crossing_record_index << std::endl;

    std::cout << "\n--- Record 2136/2137 Inspection ---" << std::endl;
    if (record_2136_found) {
        std::cout << "record_2136_flags:     0x" << std::hex << record_2136_flags << std::dec << std::endl;
        std::cout << "record_2136_is_counter: " << (record_2136_is_counter ? "YES" : "NO") << std::endl;
    } else {
        std::cout << "record_2136: NOT FOUND" << std::endl;
    }
    if (record_2137_found) {
        std::cout << "record_2137_flags:     0x" << std::hex << record_2137_flags << std::dec << std::endl;
        std::cout << "record_2137_is_counter: " << (record_2137_is_counter ? "YES" : "NO") << std::endl;
    } else {
        std::cout << "record_2137: NOT FOUND" << std::endl;
    }

    // Final book state
    Price final_bb = book.best_bid();
    Price final_ba = book.best_ask();
    bool final_crossed = (final_bb > 0 && final_ba > 0 && final_bb >= final_ba);
    std::cout << "\n--- Final Book State ---" << std::endl;
    std::cout << "best_bid:     " << final_bb << std::endl;
    std::cout << "best_ask:     " << final_ba << std::endl;
    std::cout << "crossed:      " << (final_crossed ? "YES" : "NO") << std::endl;
    std::cout << "bid_depth:    " << book.bid_depth() << std::endl;
    std::cout << "ask_depth:    " << book.ask_depth() << std::endl;

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
                        OrphanFillMode orphan_fill_mode = OrphanFillMode::Strict,
                        BookUpdateMode book_update_mode = BookUpdateMode::PerRecord,
                        const std::string& summary_out_path = "",
                        OrphanCancelMode orphan_cancel_mode = OrphanCancelMode::Strict,
                        CounterMode counter_mode = CounterMode::Include) {
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
              << ", orphan_fill_mode=" << orphan_fill_mode_name(orphan_fill_mode)
              << ", orphan_cancel_mode=" << orphan_cancel_mode_name(orphan_cancel_mode);
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
    book.set_orphan_cancel_mode(orphan_cancel_mode);
    book.set_counter_mode(counter_mode);
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

    // M10K: Transaction buffer for tx-grouped mode
    std::vector<OrderLogRecord> tx_buffer;
    int64_t tx_grouped_crossed = 0;

    // M10O: Track records in current transaction for crossing tx size
    int64_t records_in_current_tx = 0;

    OrderLogRecord rec;
    while (reader.next(file, rec)) {
        ++record_count;

        // M10K: Tx-grouped mode — buffer records until TxEnd
        if (book_update_mode == BookUpdateMode::TxGrouped) {
            if (!is_system_record(rec)) {
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

            // NewSession clears the book and flushes buffer
            if (has_flag(rec.order_flags, OLFlags::NewSession)) {
                tx_buffer.clear();
                book.clear();
                ++book.errors_ref().new_session_records_seen;
                ++book.errors_ref().book_clears_due_to_new_session;
                book.set_first_new_session_record_index(record_count);
                if (max_records > 0 && record_count >= max_records) break;
                continue;
            }

            if (rec.event == OLMsgType::Moved) {
                ++moved_count;
            }

            // M10G: Track Snapshot records
            if (has_flag(rec.order_flags, OLFlags::Snapshot)) {
                ++book.errors_ref().snapshot_records_seen;
                // M10O: Count snapshot records before first crossing
                if (book.first_crossing_event_record_index() == 0) {
                    book.set_snapshot_records_before_first_crossing(
                        book.snapshot_records_before_first_crossing() + 1);
                }
                if (snapshot_records_mode == SnapshotRecordsMode::Load) {
                    if (rec.event == OLMsgType::Add && is_system_record(rec)) {
                        ++book.errors_ref().snapshot_orders_loaded;
                    }
                } else if (snapshot_records_mode == SnapshotRecordsMode::Marker) {
                    if (max_records > 0 && record_count >= max_records) break;
                    continue;
                }
            }

            // Buffer the record
            tx_buffer.push_back(rec);

            // On TxEnd, flush the transaction buffer
            if (is_tx_end(rec)) {
                // Track book state before apply for missing order detection
                int64_t missing_before_tx = book.errors().missing_order_id;

                // Apply entire transaction batch
                TransactionBatchResult tx_result = book.apply_transaction(tx_buffer);

                // Update tx-grouped diagnostics
                ++book.errors_ref().transactions_grouped;
                book.errors_ref().records_in_grouped_transactions += tx_result.records_processed;
                if (tx_result.records_processed > book.errors_ref().max_records_in_transaction) {
                    book.errors_ref().max_records_in_transaction = tx_result.records_processed;
                }

                // M10L: Propagate first_missing_order_record_index in tx-grouped mode
                if (book.errors().missing_order_id > missing_before_tx) {
                    book.set_first_missing_order_record_index(record_count);
                }

                // Track first valid book record
                if (book.bid_depth() > 0 && book.ask_depth() > 0) {
                    book.set_first_valid_book_record_index(record_count);
                }

                // Emit snapshot after TxEnd (same as txend snapshot mode)
                bool should_snapshot = (snap_mode == SnapshotMode::TxEnd) || (snap_mode == SnapshotMode::Event);
                if (should_snapshot && book.bid_depth() > 0 && book.ask_depth() > 0) {
                    L2SnapshotEntry entry;
                    entry.timestamp = rec.timestamp;
                    entry.levels = book.snapshot(depth);
                    entry.mid = book.mid_price();
                    entry.spread = book.spread();
                    snapshots.push_back(entry);
                    ++snapshot_count;

                    // L2 diagnostics
                    Price bb = book.best_bid();
                    Price ba = book.best_ask();
                    [[maybe_unused]] Price sp = book.spread();

                    if (bb <= 0) ++l2_empty_bid;
                    if (ba <= 0) ++l2_empty_ask;
                    if (bb > 0 && ba > 0 && bb >= ba) {
                        ++l2_crossed;
                        ++l2_non_positive_spread;
                        ++tx_grouped_crossed;
                        book.set_first_crossed_book_record_index(record_count);
                        // M10O: Track unambiguous crossing indices for tx-grouped mode
                        book.set_first_crossing_event_record_index(record_count);
                        book.set_first_crossing_snapshot_record_index(record_count);
                        book.set_first_crossing_snapshot_index(snapshot_count);
                        if (book.tx_index_at_first_crossing() == 0) {
                            book.set_tx_index_at_first_crossing(tx_counter);
                            book.set_records_in_first_crossing_tx(tx_result.records_processed);
                        }
                    }

                    if (max_snapshots > 0 && snapshot_count >= max_snapshots) break;
                }

                // Clear buffer for next transaction
                tx_buffer.clear();
            }

            if (max_records > 0 && record_count >= max_records) break;
            continue;
        }

        // Per-record mode (original behavior)
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
            records_in_current_tx = 0;
        } else {
            ++records_in_current_tx;
        }

        // NewSession clears the book
        if (has_flag(rec.order_flags, OLFlags::NewSession)) {
            book.clear();
            ++book.errors_ref().new_session_records_seen;
            ++book.errors_ref().book_clears_due_to_new_session;
            book.set_first_new_session_record_index(record_count);
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

            // M10O: Count snapshot records before first crossing
            if (book.first_crossing_event_record_index() == 0) {
                book.set_snapshot_records_before_first_crossing(
                    book.snapshot_records_before_first_crossing() + 1);
            }

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

        // M10O: Track first crossing event record index (the apply that first makes bb >= ba)
        if (book.bid_depth() > 0 && book.ask_depth() > 0) {
            Price bb_after = book.best_bid();
            Price ba_after = book.best_ask();
            if (bb_after > 0 && ba_after > 0 && bb_after >= ba_after) {
                book.set_first_crossing_event_record_index(record_count);
                if (book.tx_index_at_first_crossing() == 0) {
                    book.set_tx_index_at_first_crossing(last_tx_index);
                    book.set_records_in_first_crossing_tx(records_in_current_tx);
                }
            }
        }

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
                // M10O: Track unambiguous crossing indices
                book.set_first_crossing_snapshot_record_index(record_count);
                book.set_first_crossing_snapshot_index(snapshot_count);
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

    // M10K: Transaction-grouped diagnostics
    const char* book_update_mode_name = (book_update_mode == BookUpdateMode::TxGrouped) ? "tx-grouped" : "per-record";
    std::cout << "\nTransaction-grouped diagnostics:" << std::endl;
    std::cout << "  book_update_mode:                       " << book_update_mode_name << std::endl;
    std::cout << "  transactions_grouped:                   " << errs.transactions_grouped << std::endl;
    std::cout << "  records_in_grouped_transactions:        " << errs.records_in_grouped_transactions << std::endl;
    std::cout << "  max_records_in_transaction:             " << errs.max_records_in_transaction << std::endl;
    std::cout << "  orphan_fill_events (tx):                " << errs.tx_grouped_orphan_fill_events << std::endl;
    std::cout << "  orphan_cancel_events (tx):              " << errs.tx_grouped_orphan_cancel_events << std::endl;
    std::cout << "  orphan_remove_events (tx):              " << errs.tx_grouped_orphan_remove_events << std::endl;
    std::cout << "  orphan_fill_resolved_in_transaction:    " << errs.tx_grouped_orphan_fill_resolved << std::endl;
    std::cout << "  orphan_cancel_resolved_in_transaction:  " << errs.tx_grouped_orphan_cancel_resolved << std::endl;
    std::cout << "  orphan_remove_resolved_in_transaction:  " << errs.tx_grouped_orphan_remove_resolved << std::endl;
    std::cout << "  tx_grouped_missing_order_id:            " << errs.tx_grouped_missing_order_id << std::endl;
    std::cout << "  tx_grouped_crossed_book_snapshots:      " << tx_grouped_crossed << std::endl;

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
    std::cout << "book_update_mode:                 " << book_update_mode_name << std::endl;
    std::cout << "snapshot_mode:                    " << mode_name << std::endl;
    std::cout << "snapshot_records_mode:            " << snapshot_records_mode_name << std::endl;
    std::cout << "fill_semantics:                   " << fill_mode_name << std::endl;
    std::cout << "orphan_cancel_mode:               " << orphan_cancel_mode_name(orphan_cancel_mode) << std::endl;
    std::cout << "orphan_cancel_ignored:            " << book.errors().orphan_cancel_ignored << std::endl;
    std::cout << "orphan_remove_ignored:            " << book.errors().orphan_remove_ignored << std::endl;
    std::cout << "counter_mode:                     " << counter_mode_name(counter_mode) << std::endl;
    std::cout << "counter_records_seen:             " << book.errors().counter_records_seen << std::endl;
    std::cout << "counter_records_ignored_for_book: " << book.errors().counter_records_ignored_for_book << std::endl;
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
    // M10O: Unambiguous crossing index definitions
    std::cout << "first_crossing_event_record_index:    " << book.first_crossing_event_record_index() << std::endl;
    std::cout << "first_crossing_snapshot_record_index: " << book.first_crossing_snapshot_record_index() << std::endl;
    std::cout << "first_crossing_snapshot_index:        " << book.first_crossing_snapshot_index() << std::endl;
    // M10O: Session/snapshot state
    std::cout << "first_new_session_record_index:       " << book.first_new_session_record_index() << std::endl;
    std::cout << "first_valid_book_record_index:        " << book.first_valid_book_record_index() << std::endl;
    std::cout << "snapshot_records_before_first_crossing: " << book.snapshot_records_before_first_crossing() << std::endl;
    std::cout << "tx_index_at_first_crossing:           " << book.tx_index_at_first_crossing() << std::endl;
    std::cout << "records_in_first_crossing_tx:         " << book.records_in_first_crossing_tx() << std::endl;
    if (book.first_crossing_event_record_index() > 0 && book.first_new_session_record_index() > 0) {
        std::cout << "records_between_new_session_and_first_crossing: " << (book.first_crossing_event_record_index() - book.first_new_session_record_index()) << std::endl;
    }

    // M10O: Session/snapshot state interpretation
    std::cout << "\nSession/snapshot state at first crossing:" << std::endl;
    if (book.first_crossing_event_record_index() == 0) {
        std::cout << "  No crossing detected." << std::endl;
    } else {
        bool before_new_session = (book.first_new_session_record_index() == 0) ||
                                  (book.first_crossing_event_record_index() < book.first_new_session_record_index());
        bool after_new_session = !before_new_session && book.first_new_session_record_index() > 0;
        bool inside_snapshot_init = book.snapshot_records_before_first_crossing() > 0 &&
                                    book.first_crossing_event_record_index() <= book.first_valid_book_record_index();
        bool before_continuous = book.first_valid_book_record_index() > 0 &&
                                 book.first_crossing_event_record_index() <= book.first_valid_book_record_index();

        std::cout << "  before_first_new_session:     " << (before_new_session ? "YES" : "NO") << std::endl;
        std::cout << "  immediately_after_new_session: " << (after_new_session && (book.first_crossing_event_record_index() - book.first_new_session_record_index() < 50) ? "YES" : "NO") << std::endl;
        std::cout << "  inside_snapshot_init:         " << (inside_snapshot_init ? "YES" : "NO") << std::endl;
        std::cout << "  before_continuous_trading:    " << (before_continuous ? "YES" : "NO") << std::endl;
        std::cout << "  inside_transaction_group:     " << (book.records_in_first_crossing_tx() > 1 ? "YES" : "NO") << std::endl;
        std::cout << "  at_txend:                     " << (book.records_in_first_crossing_tx() == 1 ? "POSSIBLE" : "NO") << std::endl;
    }
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

    // M10L: Write machine-readable summary if requested
    if (!summary_out_path.empty()) {
        std::ofstream summary_file(summary_out_path);
        if (summary_file.is_open()) {
            summary_file << "{\n";
            summary_file << "  \"book_update_mode\": \"" << book_update_mode_name << "\",\n";
            summary_file << "  \"snapshot_mode\": \"" << mode_name << "\",\n";
            summary_file << "  \"snapshot_records_mode\": \"" << snapshot_records_mode_name << "\",\n";
            summary_file << "  \"fill_semantics\": \"" << fill_mode_name << "\",\n";
            summary_file << "  \"orphan_fill_mode\": \"" << orphan_fill_mode_name(orphan_fill_mode) << "\",\n";
            summary_file << "  \"orphan_cancel_mode\": \"" << orphan_cancel_mode_name(orphan_cancel_mode) << "\",\n";
            summary_file << "  \"orphan_cancel_ignored\": " << book.errors().orphan_cancel_ignored << ",\n";
            summary_file << "  \"orphan_remove_ignored\": " << book.errors().orphan_remove_ignored << ",\n";
            summary_file << "  \"counter_mode\": \"" << counter_mode_name(counter_mode) << "\",\n";
            summary_file << "  \"counter_records_seen\": " << book.errors().counter_records_seen << ",\n";
            summary_file << "  \"counter_records_ignored_for_book\": " << book.errors().counter_records_ignored_for_book << ",\n";
            summary_file << "  \"counter_add_ignored_for_book\": " << book.errors().counter_add_ignored_for_book << ",\n";
            summary_file << "  \"counter_fill_ignored_for_book\": " << book.errors().counter_fill_ignored_for_book << ",\n";
            summary_file << "  \"counter_cancel_ignored_for_book\": " << book.errors().counter_cancel_ignored_for_book << ",\n";
            summary_file << "  \"counter_remove_ignored_for_book\": " << book.errors().counter_remove_ignored_for_book << ",\n";
            summary_file << "  \"counter_move_ignored_for_book\": " << book.errors().counter_move_ignored_for_book << ",\n";
            summary_file << "  \"records_processed\": " << record_count << ",\n";
            summary_file << "  \"transactions_seen\": " << transactions_seen << ",\n";
            summary_file << "  \"snapshots_written\": " << snapshot_count << ",\n";
            summary_file << "  \"missing_order_id\": " << book.errors().missing_order_id << ",\n";
            summary_file << "  \"missing_on_fill\": " << book.errors().missing_on_fill << ",\n";
            summary_file << "  \"missing_on_cancel\": " << book.errors().missing_on_cancel << ",\n";
            summary_file << "  \"missing_on_remove\": " << book.errors().missing_on_remove << ",\n";
            summary_file << "  \"missing_on_move\": " << book.errors().missing_on_move << ",\n";
            summary_file << "  \"orphan_fill_events\": " << book.errors().orphan_fill_events << ",\n";
            summary_file << "  \"orphan_fill_level_reductions\": " << book.errors().orphan_fill_level_reductions << ",\n";
            summary_file << "  \"crossed_book_snapshots\": " << l2_crossed << ",\n";
            summary_file << "  \"non_positive_spread_snapshots\": " << l2_non_positive_spread << ",\n";
            summary_file << "  \"first_missing_order_record_index\": " << book.first_missing_order_record_index() << ",\n";
            summary_file << "  \"first_crossed_book_record_index\": " << book.first_crossed_book_record_index() << ",\n";
            summary_file << "  \"first_crossing_event_record_index\": " << book.first_crossing_event_record_index() << ",\n";
            summary_file << "  \"first_crossing_snapshot_record_index\": " << book.first_crossing_snapshot_record_index() << ",\n";
            summary_file << "  \"first_crossing_snapshot_index\": " << book.first_crossing_snapshot_index() << ",\n";
            summary_file << "  \"first_new_session_record_index\": " << book.first_new_session_record_index() << ",\n";
            summary_file << "  \"first_valid_book_record_index\": " << book.first_valid_book_record_index() << ",\n";
            summary_file << "  \"snapshot_records_before_first_crossing\": " << book.snapshot_records_before_first_crossing() << ",\n";
            summary_file << "  \"tx_index_at_first_crossing\": " << book.tx_index_at_first_crossing() << ",\n";
            summary_file << "  \"records_in_first_crossing_tx\": " << book.records_in_first_crossing_tx() << ",\n";
            summary_file << "  \"records_between_new_session_and_first_crossing\": " << (book.first_crossing_event_record_index() > 0 && book.first_new_session_record_index() > 0 ? book.first_crossing_event_record_index() - book.first_new_session_record_index() : 0) << ",\n";
            summary_file << "  \"l2_strategy_ready\": " << (has_invalid ? "false" : "true") << "\n";
            summary_file << "}\n";
            summary_file.close();
            std::cout << "Summary written to " << summary_out_path << std::endl;
        } else {
            std::cerr << "Warning: cannot open summary output file: " << summary_out_path << std::endl;
        }
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

    if (cmd == "missing-cancel-probe") {
        if (argc < 3) {
            std::cerr << "Usage: qsh-ingest missing-cancel-probe <OrdLog.qsh> [--out <file.csv>] [--max-probes N]" << std::endl;
            return 1;
        }
        std::string file_path = argv[2];
        std::string out_path = "missing_cancel_probe.csv";
        int64_t max_probes = 20;
        for (int i = 3; i < argc; ++i) {
            if (std::strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
                out_path = argv[++i];
            } else if (std::strcmp(argv[i], "--max-probes") == 0 && i + 1 < argc) {
                max_probes = std::atoll(argv[++i]);
            }
        }
        return cmd_missing_cancel_probe(file_path, out_path, max_probes);
    }

    if (cmd == "orphan-cancel-audit") {
        if (argc < 3) {
            std::cerr << "Usage: qsh-ingest orphan-cancel-audit <OrdLog.qsh> [--out <file.csv>] [--max-audits N]" << std::endl;
            return 1;
        }
        std::string file_path = argv[2];
        std::string out_path = "orphan_cancel_audit.csv";
        int64_t max_audits = 200;
        for (int i = 3; i < argc; ++i) {
            if (std::strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
                out_path = argv[++i];
            } else if (std::strcmp(argv[i], "--max-audits") == 0 && i + 1 < argc) {
                max_audits = std::atoll(argv[++i]);
            }
        }
        return cmd_orphan_cancel_audit(file_path, out_path, max_audits);
    }

    if (cmd == "snapshot-audit") {
        if (argc < 3) {
            std::cerr << "Usage: qsh-ingest snapshot-audit <OrdLog.qsh> [--out <file.csv>] [--max-records N]" << std::endl;
            return 1;
        }
        std::string file_path = argv[2];
        std::string out_path = "snapshot_audit.csv";
        int64_t max_records = 0;
        for (int i = 3; i < argc; ++i) {
            if (std::strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
                out_path = argv[++i];
            } else if (std::strcmp(argv[i], "--max-records") == 0 && i + 1 < argc) {
                max_records = std::atoll(argv[++i]);
            }
        }
        return cmd_snapshot_audit(file_path, out_path, max_records);
    }

    if (cmd == "counter-flag-audit") {
        if (argc < 3) {
            std::cerr << "Usage: qsh-ingest counter-flag-audit <OrdLog.qsh> [--out <file.csv>] [--max-records N]" << std::endl;
            return 1;
        }
        std::string file_path = argv[2];
        std::string out_path;
        int64_t max_records = 0;
        for (int i = 3; i < argc; ++i) {
            if (std::strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
                out_path = argv[++i];
            } else if (std::strcmp(argv[i], "--max-records") == 0 && i + 1 < argc) {
                max_records = std::atoll(argv[++i]);
            }
        }
        return cmd_counter_flag_audit(file_path, out_path, max_records);
    }

    if (cmd == "crossing-window-audit") {
        if (argc < 3) {
            std::cerr << "Usage: qsh-ingest crossing-window-audit <OrdLog.qsh> --from N --to N --out <file.csv>\n"
                      << "  [--target-order-id ID] [--target-price P]" << std::endl;
            return 1;
        }
        std::string file_path = argv[2];
        std::string out_path = "crossing_window_audit.csv";
        int64_t from_index = 1;
        int64_t to_index = 0;
        UID target_order_id = 1925033994466246392;
        Price target_price = 14100;
        for (int i = 3; i < argc; ++i) {
            if (std::strcmp(argv[i], "--from") == 0 && i + 1 < argc) {
                from_index = std::atoll(argv[++i]);
            } else if (std::strcmp(argv[i], "--to") == 0 && i + 1 < argc) {
                to_index = std::atoll(argv[++i]);
            } else if (std::strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
                out_path = argv[++i];
            } else if (std::strcmp(argv[i], "--target-order-id") == 0 && i + 1 < argc) {
                target_order_id = std::atoll(argv[++i]);
            } else if (std::strcmp(argv[i], "--target-price") == 0 && i + 1 < argc) {
                target_price = std::atoll(argv[++i]);
            }
        }
        return cmd_crossing_window_audit(file_path, from_index, to_index, out_path, target_order_id, target_price);
    }

    if (cmd == "first-crossed-root-cause") {
        if (argc < 3) {
            std::cerr << "Usage: qsh-ingest first-crossed-root-cause <OrdLog.qsh> [--out-dir <dir>] [--context N]" << std::endl;
            return 1;
        }
        std::string file_path = argv[2];
        std::string out_dir = "data/reports/qsh";
        int context_events = 40;
        for (int i = 3; i < argc; ++i) {
            if (std::strcmp(argv[i], "--out-dir") == 0 && i + 1 < argc) {
                out_dir = argv[++i];
            } else if (std::strcmp(argv[i], "--context") == 0 && i + 1 < argc) {
                context_events = std::atoi(argv[++i]);
            }
        }
        return cmd_first_crossed_root_cause(file_path, out_dir, context_events);
    }

    if (cmd == "crossed-persistence-audit") {
        if (argc < 3) {
            std::cerr << "Usage: qsh-ingest crossed-persistence-audit <OrdLog.qsh> --from N --max-records N --out <file.csv>" << std::endl;
            return 1;
        }
        std::string file_path = argv[2];
        int64_t from_index = 2136;
        int64_t max_records = 0;
        std::string out_path = "crossed_persistence_audit.csv";
        for (int i = 3; i < argc; ++i) {
            if (std::strcmp(argv[i], "--from") == 0 && i + 1 < argc) {
                from_index = std::atoll(argv[++i]);
            } else if (std::strcmp(argv[i], "--max-records") == 0 && i + 1 < argc) {
                max_records = std::atoll(argv[++i]);
            } else if (std::strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
                out_path = argv[++i];
            }
        }
        return cmd_crossed_persistence_audit(file_path, from_index, max_records, out_path);
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
                      << "  [--orphan-cancel-mode strict|ignore]\n"
                      << "  [--book-update-mode per-record|tx-grouped]\n"
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
        OrphanCancelMode orphan_cancel_mode = OrphanCancelMode::Strict;
        CounterMode counter_mode = CounterMode::Include;
        BookUpdateMode book_update_mode = BookUpdateMode::PerRecord;
        std::string summary_out_path;
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
            } else if (std::strcmp(argv[i], "--orphan-cancel-mode") == 0 && i + 1 < argc) {
                std::string mode_str = argv[++i];
                if (mode_str == "strict") {
                    orphan_cancel_mode = OrphanCancelMode::Strict;
                } else if (mode_str == "ignore") {
                    orphan_cancel_mode = OrphanCancelMode::Ignore;
                } else {
                    std::cerr << "Unknown orphan cancel mode: " << mode_str << " (use strict or ignore)" << std::endl;
                    return 1;
                }
            } else if (std::strcmp(argv[i], "--counter-mode") == 0 && i + 1 < argc) {
                std::string mode_str = argv[++i];
                if (mode_str == "include") {
                    counter_mode = CounterMode::Include;
                } else if (mode_str == "ignore-book") {
                    counter_mode = CounterMode::IgnoreBook;
                } else {
                    std::cerr << "Unknown counter mode: " << mode_str << " (use include or ignore-book)" << std::endl;
                    return 1;
                }
            } else if (std::strcmp(argv[i], "--book-update-mode") == 0 && i + 1 < argc) {
                std::string mode_str = argv[++i];
                if (mode_str == "per-record") {
                    book_update_mode = BookUpdateMode::PerRecord;
                } else if (mode_str == "tx-grouped") {
                    book_update_mode = BookUpdateMode::TxGrouped;
                } else {
                    std::cerr << "Unknown book update mode: " << mode_str << " (use per-record or tx-grouped)" << std::endl;
                    return 1;
                }
            } else if (std::strcmp(argv[i], "--summary-out") == 0 && i + 1 < argc) {
                summary_out_path = argv[++i];
            }
        }
        return cmd_l3_to_l2(file_path, depth, max_records, max_snapshots, out_path, diag_path,
                            max_diagnostics, trace_path, max_trace_events, snap_mode,
                            trace_order_ids, order_trace_path, ring_buffer_size,
                            best_level_orders_path, missing_order_path, auto_trace_crossed_path,
                            fill_delta_mode, snapshot_records_mode, orphan_fill_mode,
                            book_update_mode, summary_out_path, orphan_cancel_mode, counter_mode);
    }

    std::cerr << "Unknown command: " << cmd << std::endl;
    print_help();
    return 1;
}
