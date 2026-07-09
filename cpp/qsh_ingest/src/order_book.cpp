#include "orderbook/order_book.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>

namespace qsh {

bool OrderBook::apply(const OrderLogRecord& rec) {
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

void OrderBook::clear() {
    bid_levels_.clear();
    ask_levels_.clear();
    bid_level_orders_.clear();
    ask_level_orders_.clear();
    orders_.clear();
}

void OrderBook::add_order(const OrderLogRecord& rec) {
    if (rec.side == Side::Unknown) {
        ++errors_.invalid_side;
        return;
    }
    if (rec.amount_rest == 0) {
        // This shouldn't happen for Add but handle gracefully
        return;
    }

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
        ++errors_.missing_order_id;
        return;
    }

    auto& info = it->second;
    Volume fill_amount = rec.amount;
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
        ++errors_.missing_order_id;
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
        ++errors_.missing_order_id;
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

}  // namespace qsh
