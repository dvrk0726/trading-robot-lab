#include "orderbook/order_book.hpp"
#include <algorithm>
#include <cmath>
#include <set>
#include <sstream>

namespace qsh {

bool OrderBook::apply(const OrderLogRecord& rec) {
    bool is_counter = has_flag(rec.order_flags, OLFlags::Counter);
    bool is_non_system = has_flag(rec.order_flags, OLFlags::NonSystem);
    bool is_quote = has_flag(rec.order_flags, OLFlags::Quote);

    // M10S: Track all Counter records regardless of mode
    if (is_counter) {
        ++errors_.counter_records_seen;
        if (rec.event == OLMsgType::Add)    ++errors_.counter_add;
        if (rec.event == OLMsgType::Fill)   ++errors_.counter_fill;
        if (rec.event == OLMsgType::Cancel) ++errors_.counter_cancel;
        if (rec.event == OLMsgType::Remove) ++errors_.counter_remove;
        if (rec.event == OLMsgType::Moved)  ++errors_.counter_move;
        if (rec.side == Side::Buy)          ++errors_.counter_buy;
        if (rec.side == Side::Sell)         ++errors_.counter_sell;
        if (has_flag(rec.order_flags, OLFlags::Snapshot))     ++errors_.counter_snapshot;
        if (has_flag(rec.order_flags, OLFlags::NewSession))   ++errors_.counter_new_session;
        if (has_flag(rec.order_flags, OLFlags::TxEnd))        ++errors_.counter_txend;
        if (has_flag(rec.order_flags, OLFlags::NonSystem))    ++errors_.counter_non_system;
        if (has_flag(rec.order_flags, OLFlags::CrossTrade))   ++errors_.counter_cross_trade;
    }

    // M10V: Track all NonSystem records regardless of mode
    if (is_non_system) {
        ++errors_.non_system_records_seen;
        if (first_non_system_record_index_ == 0) first_non_system_record_index_ = 0;  // set externally
        if (rec.event == OLMsgType::Add)    ++errors_.non_system_add;
        if (rec.event == OLMsgType::Fill)   ++errors_.non_system_fill;
        if (rec.event == OLMsgType::Cancel) ++errors_.non_system_cancel;
        if (rec.event == OLMsgType::Remove) ++errors_.non_system_remove;
        if (rec.event == OLMsgType::Moved)  ++errors_.non_system_moved;
        if (rec.event == OLMsgType::Unknown) ++errors_.non_system_unknown_event;
        if (rec.side == Side::Buy)          ++errors_.non_system_buy;
        if (rec.side == Side::Sell)         ++errors_.non_system_sell;
        if (has_flag(rec.order_flags, OLFlags::Counter))     ++errors_.non_system_counter;
        if (has_flag(rec.order_flags, OLFlags::CrossTrade))  ++errors_.non_system_cross_trade;
        if (has_flag(rec.order_flags, OLFlags::Snapshot))    ++errors_.non_system_snapshot;
        if (has_flag(rec.order_flags, OLFlags::NewSession))  ++errors_.non_system_new_session;
        if (has_flag(rec.order_flags, OLFlags::TxEnd))       ++errors_.non_system_txend;
    }

    // M10X: Track all Quote records regardless of mode
    if (is_quote) {
        ++errors_.quote_records_seen;
        if (first_quote_record_index_ == 0) first_quote_record_index_ = 0;  // set externally
        if (rec.event == OLMsgType::Add)    { ++errors_.quote_add; if (first_quote_add_record_index_ == 0) first_quote_add_record_index_ = 0; }
        if (rec.event == OLMsgType::Fill)   ++errors_.quote_fill;
        if (rec.event == OLMsgType::Cancel) ++errors_.quote_cancel;
        if (rec.event == OLMsgType::Remove) ++errors_.quote_remove;
        if (rec.event == OLMsgType::Moved)  ++errors_.quote_moved;
        if (rec.side == Side::Buy)          ++errors_.quote_buy;
        if (rec.side == Side::Sell)         ++errors_.quote_sell;
        if (has_flag(rec.order_flags, OLFlags::Counter))      ++errors_.quote_counter;
        if (has_flag(rec.order_flags, OLFlags::NonSystem))    ++errors_.quote_non_system;
        if (has_flag(rec.order_flags, OLFlags::CrossTrade))   ++errors_.quote_cross_trade;
        if (has_flag(rec.order_flags, OLFlags::Snapshot))     ++errors_.quote_snapshot;
        if (has_flag(rec.order_flags, OLFlags::NewSession))   ++errors_.quote_new_session;
        if (has_flag(rec.order_flags, OLFlags::TxEnd))        ++errors_.quote_txend;
        if (has_flag(rec.order_flags, OLFlags::FillOrKill))   ++errors_.quote_fill_or_kill;
        if (has_flag(rec.order_flags, OLFlags::Canceled))     ++errors_.quote_canceled;
        if (has_flag(rec.order_flags, OLFlags::CanceledGroup))++errors_.quote_canceled_group;
    }

    // M10S: In ignore-book mode, skip book mutation for Counter events
    if (is_counter && counter_mode_ == CounterMode::IgnoreBook) {
        ++errors_.counter_records_ignored_for_book;
        if (rec.event == OLMsgType::Add)    ++errors_.counter_add_ignored_for_book;
        if (rec.event == OLMsgType::Fill)   ++errors_.counter_fill_ignored_for_book;
        if (rec.event == OLMsgType::Cancel) ++errors_.counter_cancel_ignored_for_book;
        if (rec.event == OLMsgType::Remove) ++errors_.counter_remove_ignored_for_book;
        if (rec.event == OLMsgType::Moved)  ++errors_.counter_move_ignored_for_book;
        last_ts_ = rec.timestamp;
        return true;
    }

    // M10V: In ignore-book mode, skip book mutation for NonSystem events
    if (is_non_system && non_system_mode_ == NonSystemMode::IgnoreBook) {
        ++errors_.non_system_records_ignored_for_book;
        if (rec.event == OLMsgType::Add)    ++errors_.non_system_add_ignored_for_book;
        if (rec.event == OLMsgType::Fill)   ++errors_.non_system_fill_ignored_for_book;
        if (rec.event == OLMsgType::Cancel) ++errors_.non_system_cancel_ignored_for_book;
        if (rec.event == OLMsgType::Remove) ++errors_.non_system_remove_ignored_for_book;
        last_ts_ = rec.timestamp;
        return true;
    }

    // M10X: In ignore-book mode, skip book mutation for Quote events
    if (is_quote && quote_mode_ == QuoteMode::IgnoreBook) {
        ++errors_.quote_records_ignored_for_book;
        if (rec.event == OLMsgType::Add)    ++errors_.quote_add_ignored_for_book;
        if (rec.event == OLMsgType::Fill)   ++errors_.quote_fill_ignored_for_book;
        if (rec.event == OLMsgType::Cancel) ++errors_.quote_cancel_ignored_for_book;
        if (rec.event == OLMsgType::Remove) ++errors_.quote_remove_ignored_for_book;
        if (rec.event == OLMsgType::Moved)  ++errors_.quote_moved_ignored_for_book;
        last_ts_ = rec.timestamp;
        return true;
    }

    // Original logic for Include mode or non-Counter/non-NonSystem/non-Quote events
    if (rec.event == OLMsgType::Add) {
        add_order(rec);
    } else if (rec.event == OLMsgType::Fill) {
        fill_order(rec);
    } else if (rec.event == OLMsgType::Moved) {
        move_order(rec);
    } else if (rec.event == OLMsgType::Cancel) {
        cancel_order(rec);
    } else if (rec.event == OLMsgType::Remove) {
        remove_order(rec);
    }

    // Update timestamp
    last_ts_ = rec.timestamp;

    return true;
}

TransactionBatchResult OrderBook::apply_transaction(const std::vector<OrderLogRecord>& records) {
    TransactionBatchResult result;
    result.records_processed = static_cast<int64_t>(records.size());

    // Snapshot which order_ids are in the book before this transaction
    std::set<UID> pre_tx_orders;
    for (const auto& [id, _] : orders_) {
        pre_tx_orders.insert(id);
    }

    // Track order_ids added within this transaction
    std::set<UID> tx_added_orders;

    int64_t missing_before = errors_.missing_order_id;

    for (const auto& rec : records) {
        if (rec.event == OLMsgType::Add) {
            // Track that this order was added in this transaction
            tx_added_orders.insert(rec.order_id);
            apply(rec);
        } else if (rec.event == OLMsgType::Fill) {
            bool in_book = orders_.find(rec.order_id) != orders_.end();
            bool added_in_tx = tx_added_orders.count(rec.order_id) > 0;

            if (!in_book && !added_in_tx) {
                // Truly orphan: order not in book AND not added in this tx
                ++result.orphan_fill_events;
                // Still apply to count missing_order_id in BookErrors
                apply(rec);
            } else if (!in_book && added_in_tx) {
                // Order was added in this tx but already consumed before this fill
                // This is a "resolved" case: the order existed in this tx
                ++result.orphan_fill_resolved_in_transaction;
                apply(rec);
            } else {
                // Normal: order in book
                apply(rec);
            }
        } else if (rec.event == OLMsgType::Cancel || rec.event == OLMsgType::Moved) {
            bool in_book = orders_.find(rec.order_id) != orders_.end();
            bool added_in_tx = tx_added_orders.count(rec.order_id) > 0;

            if (!in_book && !added_in_tx) {
                ++result.orphan_cancel_events;
                apply(rec);
            } else if (!in_book && added_in_tx) {
                ++result.orphan_cancel_resolved_in_transaction;
                apply(rec);
            } else {
                apply(rec);
            }
        } else if (rec.event == OLMsgType::Remove) {
            bool in_book = orders_.find(rec.order_id) != orders_.end();
            bool added_in_tx = tx_added_orders.count(rec.order_id) > 0;

            if (!in_book && !added_in_tx) {
                ++result.orphan_remove_events;
                apply(rec);
            } else if (!in_book && added_in_tx) {
                ++result.orphan_remove_resolved_in_transaction;
                apply(rec);
            } else {
                apply(rec);
            }
        } else {
            apply(rec);
        }
    }

    result.missing_order_id = errors_.missing_order_id - missing_before;

    // Update BookErrors M10K counters
    errors_.tx_grouped_orphan_fill_events += result.orphan_fill_events;
    errors_.tx_grouped_orphan_cancel_events += result.orphan_cancel_events;
    errors_.tx_grouped_orphan_remove_events += result.orphan_remove_events;
    errors_.tx_grouped_orphan_fill_resolved += result.orphan_fill_resolved_in_transaction;
    errors_.tx_grouped_orphan_cancel_resolved += result.orphan_cancel_resolved_in_transaction;
    errors_.tx_grouped_orphan_remove_resolved += result.orphan_remove_resolved_in_transaction;
    errors_.tx_grouped_missing_order_id += result.missing_order_id;

    return result;
}

void OrderBook::clear() {
    bid_levels_.clear();
    ask_levels_.clear();
    bid_level_orders_.clear();
    ask_level_orders_.clear();
    orders_.clear();
}

void OrderBook::add_order(const OrderLogRecord& rec) {
    ++errors_.add_records_seen;
    if (rec.side == Side::Unknown) {
        ++errors_.invalid_side;
        ++errors_.add_records_skipped;
        ++errors_.skip_invalid_side;
        return;
    }
    if (rec.amount_rest == 0) {
        // This shouldn't happen for Add but handle gracefully
        ++errors_.add_records_skipped;
        ++errors_.skip_zero_amount;
        return;
    }
    ++errors_.add_records_applied;

    // Track order
    orders_[rec.order_id] = {rec.side, rec.price, rec.amount_rest};

    // Add to level
    if (rec.side == Side::Buy) {
        auto& lvl = bid_levels_[rec.price];
        lvl.first += rec.amount_rest;
        ++lvl.second;
        bid_level_orders_[rec.price].push_back(rec.order_id);
    } else {
        auto& lvl = ask_levels_[rec.price];
        lvl.first += rec.amount_rest;
        ++lvl.second;
        ask_level_orders_[rec.price].push_back(rec.order_id);
    }
}

void OrderBook::fill_order(const OrderLogRecord& rec) {
    auto it = orders_.find(rec.order_id);
    if (it == orders_.end()) {
        // M10J: Orphan fill handling
        ++errors_.orphan_fill_events;
        if (orphan_fill_mode_ == OrphanFillMode::Ignore) {
            ++errors_.orphan_fill_ignored;
            return;
        } else if (orphan_fill_mode_ == OrphanFillMode::ReduceSamePrice) {
            // Reduce volume at same price level without order_id lookup
            Volume fill_amount = fill_delta_mode_ ? rec.amount : (rec.amount - rec.amount_rest);
            if (fill_amount <= 0) {
                ++errors_.orphan_fill_ignored;
                return;
            }
            if (rec.side == Side::Buy) {
                auto lvl_it = bid_levels_.find(rec.price);
                if (lvl_it != bid_levels_.end()) {
                    lvl_it->second.first -= fill_amount;
                    ++errors_.orphan_fill_level_reductions;
                    if (lvl_it->second.first <= 0) {
                        bid_levels_.erase(lvl_it);
                        bid_level_orders_.erase(rec.price);
                    }
                } else {
                    ++errors_.orphan_fill_ignored;
                }
            } else if (rec.side == Side::Sell) {
                auto lvl_it = ask_levels_.find(rec.price);
                if (lvl_it != ask_levels_.end()) {
                    lvl_it->second.first -= fill_amount;
                    ++errors_.orphan_fill_level_reductions;
                    if (lvl_it->second.first <= 0) {
                        ask_levels_.erase(lvl_it);
                        ask_level_orders_.erase(rec.price);
                    }
                } else {
                    ++errors_.orphan_fill_ignored;
                }
            }
            return;
        } else if (orphan_fill_mode_ == OrphanFillMode::TransactionRest) {
            // Use amount_rest to update most recent resting order in same transaction
            // For now, fall through to strict mode (requires transaction context)
            ++errors_.orphan_fill_ignored;
            return;
        }
        // Strict mode: count as missing_order_id (original behavior)
        ++errors_.missing_order_id;
        ++errors_.missing_on_fill;
        if (rec.side == Side::Buy) ++errors_.missing_on_buy;
        else if (rec.side == Side::Sell) ++errors_.missing_on_sell;
        else ++errors_.missing_on_unknown_side;
        return;
    }

    auto& info = it->second;

    // Two interpretations of fill semantics:
    // delta mode (default): amount = filled quantity, amount_rest = remaining after fill
    // rest mode:            amount = original order quantity, fill = amount - amount_rest
    Volume fill_amount;
    if (fill_delta_mode_) {
        fill_amount = rec.amount;
    } else {
        // In rest mode: amount is the original quantity, amount_rest is what remains
        // fill = original - rest = amount - amount_rest
        fill_amount = rec.amount - rec.amount_rest;
        if (fill_amount < 0) {
            ++errors_.amount_mismatch;
            return;
        }
    }
    bool fully_filled = (info.amount <= fill_amount);

    // Save side and price before potential erase (info becomes dangling after erase)
    Side side = info.side;
    Price price = info.price;

    // Reduce order volume
    if (fully_filled) {
        fill_amount = info.amount;
        orders_.erase(it);
    } else {
        info.amount -= fill_amount;
    }

    // Reduce level volume
    if (side == Side::Buy) {
        auto lvl_it = bid_levels_.find(price);
        if (lvl_it != bid_levels_.end()) {
            lvl_it->second.first -= fill_amount;
            if (fully_filled && lvl_it->second.second > 0) {
                --lvl_it->second.second;
            }
            if (lvl_it->second.first <= 0) {
                bid_levels_.erase(lvl_it);
                bid_level_orders_.erase(price);
            } else if (fully_filled) {
                // Remove order id from level tracking
                auto& ids = bid_level_orders_[price];
                ids.erase(std::remove(ids.begin(), ids.end(), rec.order_id), ids.end());
            }
        } else {
            ++errors_.level_not_found;
        }
    } else {
        auto lvl_it = ask_levels_.find(price);
        if (lvl_it != ask_levels_.end()) {
            lvl_it->second.first -= fill_amount;
            if (fully_filled && lvl_it->second.second > 0) {
                --lvl_it->second.second;
            }
            if (lvl_it->second.first <= 0) {
                ask_levels_.erase(lvl_it);
                ask_level_orders_.erase(price);
            } else if (fully_filled) {
                // Remove order id from level tracking
                auto& ids = ask_level_orders_[price];
                ids.erase(std::remove(ids.begin(), ids.end(), rec.order_id), ids.end());
            }
        } else {
            ++errors_.level_not_found;
        }
    }

    // If order fully consumed, remove from tracking
    if (!fully_filled && orders_.find(rec.order_id) != orders_.end() && orders_[rec.order_id].amount <= 0) {
        orders_.erase(rec.order_id);
    }
}

void OrderBook::cancel_order(const OrderLogRecord& rec) {
    auto it = orders_.find(rec.order_id);
    if (it == orders_.end()) {
        if (orphan_cancel_mode_ == OrphanCancelMode::Ignore) {
            ++errors_.orphan_cancel_ignored;
            return;
        }
        ++errors_.missing_order_id;
        ++errors_.missing_on_cancel;
        if (rec.side == Side::Buy) ++errors_.missing_on_buy;
        else if (rec.side == Side::Sell) ++errors_.missing_on_sell;
        else ++errors_.missing_on_unknown_side;
        return;
    }

    auto& info = it->second;

    if (rec.amount_rest == 0) {
        // Full cancel - remove order entirely
        Volume amount_to_remove = info.amount;

        if (info.side == Side::Buy) {
            auto lvl_it = bid_levels_.find(info.price);
            if (lvl_it != bid_levels_.end()) {
                lvl_it->second.first -= amount_to_remove;
                --lvl_it->second.second;
                if (lvl_it->second.second <= 0 || lvl_it->second.first <= 0) {
                    bid_levels_.erase(lvl_it);
                    bid_level_orders_.erase(info.price);
                } else {
                    // Remove order id from level tracking
                    auto& ids = bid_level_orders_[info.price];
                    ids.erase(std::remove(ids.begin(), ids.end(), rec.order_id), ids.end());
                }
            } else {
                ++errors_.level_not_found;
            }
        } else {
            auto lvl_it = ask_levels_.find(info.price);
            if (lvl_it != ask_levels_.end()) {
                lvl_it->second.first -= amount_to_remove;
                --lvl_it->second.second;
                if (lvl_it->second.second <= 0 || lvl_it->second.first <= 0) {
                    ask_levels_.erase(lvl_it);
                    ask_level_orders_.erase(info.price);
                } else {
                    // Remove order id from level tracking
                    auto& ids = ask_level_orders_[info.price];
                    ids.erase(std::remove(ids.begin(), ids.end(), rec.order_id), ids.end());
                }
            } else {
                ++errors_.level_not_found;
            }
        }

        orders_.erase(it);
    } else {
        // Partial cancel - reduce to amount_rest
        Volume diff = info.amount - rec.amount_rest;
        if (diff < 0) {
            ++errors_.amount_mismatch;
            return;
        }

        if (info.side == Side::Buy) {
            auto lvl_it = bid_levels_.find(info.price);
            if (lvl_it != bid_levels_.end()) {
                lvl_it->second.first -= diff;
                if (lvl_it->second.first < 0) {
                    ++errors_.negative_level_volume;
                    lvl_it->second.first = 0;
                }
            } else {
                ++errors_.level_not_found;
            }
        } else {
            auto lvl_it = ask_levels_.find(info.price);
            if (lvl_it != ask_levels_.end()) {
                lvl_it->second.first -= diff;
                if (lvl_it->second.first < 0) {
                    ++errors_.negative_level_volume;
                    lvl_it->second.first = 0;
                }
            } else {
                ++errors_.level_not_found;
            }
        }

        info.amount = rec.amount_rest;
    }
}

void OrderBook::remove_order(const OrderLogRecord& rec) {
    auto it = orders_.find(rec.order_id);
    if (it == orders_.end()) {
        if (orphan_cancel_mode_ == OrphanCancelMode::Ignore) {
            ++errors_.orphan_remove_ignored;
            return;
        }
        ++errors_.missing_order_id;
        ++errors_.missing_on_remove;
        if (rec.side == Side::Buy) ++errors_.missing_on_buy;
        else if (rec.side == Side::Sell) ++errors_.missing_on_sell;
        else ++errors_.missing_on_unknown_side;
        return;
    }

    auto& info = it->second;

    if (info.side == Side::Buy) {
        auto lvl_it = bid_levels_.find(info.price);
        if (lvl_it != bid_levels_.end()) {
            lvl_it->second.first -= info.amount;
            --lvl_it->second.second;
            if (lvl_it->second.second <= 0 || lvl_it->second.first <= 0) {
                bid_levels_.erase(lvl_it);
                bid_level_orders_.erase(info.price);
            } else {
                // Remove order id from level tracking
                auto& ids = bid_level_orders_[info.price];
                ids.erase(std::remove(ids.begin(), ids.end(), rec.order_id), ids.end());
            }
        }
    } else {
        auto lvl_it = ask_levels_.find(info.price);
        if (lvl_it != ask_levels_.end()) {
            lvl_it->second.first -= info.amount;
            --lvl_it->second.second;
            if (lvl_it->second.second <= 0 || lvl_it->second.first <= 0) {
                ask_levels_.erase(lvl_it);
                ask_level_orders_.erase(info.price);
            } else {
                // Remove order id from level tracking
                auto& ids = ask_level_orders_[info.price];
                ids.erase(std::remove(ids.begin(), ids.end(), rec.order_id), ids.end());
            }
        }
    }

    orders_.erase(it);
}

void OrderBook::move_order(const OrderLogRecord& rec) {
    auto it = orders_.find(rec.order_id);
    if (it == orders_.end()) {
        ++errors_.missing_order_id;
        ++errors_.missing_on_move;
        if (rec.side == Side::Buy) ++errors_.missing_on_buy;
        else if (rec.side == Side::Sell) ++errors_.missing_on_sell;
        else ++errors_.missing_on_unknown_side;
        return;
    }

    auto& info = it->second;
    Price old_price = info.price;
    Volume old_amount = info.amount;

    // Remove full amount from old price level
    if (info.side == Side::Buy) {
        auto lvl_it = bid_levels_.find(old_price);
        if (lvl_it != bid_levels_.end()) {
            lvl_it->second.first -= old_amount;
            --lvl_it->second.second;
            if (lvl_it->second.second <= 0 || lvl_it->second.first <= 0) {
                bid_levels_.erase(lvl_it);
                bid_level_orders_.erase(old_price);
            } else {
                // Remove order id from old level
                auto& ids = bid_level_orders_[old_price];
                ids.erase(std::remove(ids.begin(), ids.end(), rec.order_id), ids.end());
            }
        } else {
            ++errors_.level_not_found;
        }
    } else {
        auto lvl_it = ask_levels_.find(old_price);
        if (lvl_it != ask_levels_.end()) {
            lvl_it->second.first -= old_amount;
            --lvl_it->second.second;
            if (lvl_it->second.second <= 0 || lvl_it->second.first <= 0) {
                ask_levels_.erase(lvl_it);
                ask_level_orders_.erase(old_price);
            } else {
                // Remove order id from old level
                auto& ids = ask_level_orders_[old_price];
                ids.erase(std::remove(ids.begin(), ids.end(), rec.order_id), ids.end());
            }
        } else {
            ++errors_.level_not_found;
        }
    }

    // Add to new price level with amount_rest
    Volume new_amount = rec.amount_rest > 0 ? rec.amount_rest : old_amount;
    Price new_price = rec.price;

    if (info.side == Side::Buy) {
        auto& lvl = bid_levels_[new_price];
        lvl.first += new_amount;
        ++lvl.second;
        bid_level_orders_[new_price].push_back(rec.order_id);
    } else {
        auto& lvl = ask_levels_[new_price];
        lvl.first += new_amount;
        ++lvl.second;
        ask_level_orders_[new_price].push_back(rec.order_id);
    }

    // Update order tracking
    info.price = new_price;
    info.amount = new_amount;
}

std::vector<L2SnapshotRow> OrderBook::snapshot(int depth) const {
    std::vector<L2SnapshotRow> result;

    auto bid_it = bid_levels_.begin();
    auto ask_it = ask_levels_.begin();

    for (int i = 0; i < depth; ++i) {
        L2SnapshotRow row;
        if (bid_it != bid_levels_.end()) {
            row.bid_price = bid_it->first;
            row.bid_qty = bid_it->second.first;
            ++bid_it;
        }
        if (ask_it != ask_levels_.end()) {
            row.ask_price = ask_it->first;
            row.ask_qty = ask_it->second.first;
            ++ask_it;
        }
        result.push_back(row);
    }

    return result;
}

double OrderBook::mid_price() const {
    if (bid_levels_.empty() || ask_levels_.empty()) return 0.0;
    return (bid_levels_.begin()->first + ask_levels_.begin()->first) * 0.5;
}

Price OrderBook::spread() const {
    if (bid_levels_.empty() || ask_levels_.empty()) return 0;
    return ask_levels_.begin()->first - bid_levels_.begin()->first;
}

Price OrderBook::best_bid() const {
    if (bid_levels_.empty()) return 0;
    return bid_levels_.begin()->first;
}

Price OrderBook::best_ask() const {
    if (ask_levels_.empty()) return 0;
    return ask_levels_.begin()->first;
}

bool OrderBook::check_crossed() const {
    if (bid_levels_.empty() || ask_levels_.empty()) return false;
    return bid_levels_.begin()->first >= ask_levels_.begin()->first;
}

std::vector<UID> OrderBook::best_bid_order_ids(int max_ids) const {
    std::vector<UID> result;
    if (bid_levels_.empty()) return result;
    Price best = bid_levels_.begin()->first;
    auto it = bid_level_orders_.find(best);
    if (it != bid_level_orders_.end()) {
        int count = 0;
        for (UID id : it->second) {
            if (count >= max_ids) break;
            result.push_back(id);
            ++count;
        }
    }
    return result;
}

std::vector<UID> OrderBook::best_ask_order_ids(int max_ids) const {
    std::vector<UID> result;
    if (ask_levels_.empty()) return result;
    Price best = ask_levels_.begin()->first;
    auto it = ask_level_orders_.find(best);
    if (it != ask_level_orders_.end()) {
        int count = 0;
        for (UID id : it->second) {
            if (count >= max_ids) break;
            result.push_back(id);
            ++count;
        }
    }
    return result;
}

Volume OrderBook::best_bid_total_qty() const {
    if (bid_levels_.empty()) return 0;
    return bid_levels_.begin()->second.first;
}

Volume OrderBook::best_ask_total_qty() const {
    if (ask_levels_.empty()) return 0;
    return ask_levels_.begin()->second.first;
}

int OrderBook::best_bid_order_count() const {
    if (bid_levels_.empty()) return 0;
    return bid_levels_.begin()->second.second;
}

int OrderBook::best_ask_order_count() const {
    if (ask_levels_.empty()) return 0;
    return ask_levels_.begin()->second.second;
}

bool OrderBook::get_order_info(UID order_id, Side& out_side, Price& out_price, Volume& out_qty) const {
    auto it = orders_.find(order_id);
    if (it == orders_.end()) return false;
    out_side = it->second.side;
    out_price = it->second.price;
    out_qty = it->second.amount;
    return true;
}

Volume OrderBook::level_qty(Price price, Side side) const {
    if (side == Side::Buy) {
        auto it = bid_levels_.find(price);
        if (it != bid_levels_.end()) return it->second.first;
    } else if (side == Side::Sell) {
        auto it = ask_levels_.find(price);
        if (it != ask_levels_.end()) return it->second.first;
    }
    return 0;
}

}  // namespace qsh
