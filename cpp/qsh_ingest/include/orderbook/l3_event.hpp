#pragma once

#include "qsh/qsh_types.hpp"

namespace qsh {

// L3 order book event types for external consumers.
enum class L3EventType : uint8_t {
    Add,
    Fill,
    Cancel,
    Remove,
    NewSession
};

struct L3Event {
    L3EventType type;
    Timestamp timestamp;
    UID order_id;
    Price price;
    Volume amount;
    Volume amount_rest;
    Side side;
};

}  // namespace qsh
