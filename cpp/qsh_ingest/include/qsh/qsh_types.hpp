#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace qsh {

// QSH signature: "QScalp History Data"
inline constexpr uint8_t kQshSignature[] = {
    0x51, 0x53, 0x63, 0x61, 0x6c, 0x70, 0x20, 0x48, 0x69, 0x73,
    0x74, 0x6f, 0x72, 0x79, 0x20, 0x44, 0x61, 0x74, 0x61
};
inline constexpr size_t kQshSignatureLen = 19;
inline constexpr uint8_t kQshVersion = 4;

// .NET ticks epoch offset: ticks from 0001-01-01 to 1970-01-01
inline constexpr int64_t kDotNetTicksToUnixMs = 62135596800000LL;

using Price = int64_t;
using Volume = int64_t;
using Timestamp = int64_t;
using UID = int64_t;

enum class StreamType : uint8_t {
    Quotes   = 0x10,
    Deals    = 0x20,
    AuxInfo  = 0x60,
    OrderLog = 0x70,
    Unknown  = 0xFF
};

inline StreamType stream_type_from_u8(uint8_t v) {
    switch (v) {
        case 0x10: return StreamType::Quotes;
        case 0x20: return StreamType::Deals;
        case 0x60: return StreamType::AuxInfo;
        case 0x70: return StreamType::OrderLog;
        default:   return StreamType::Unknown;
    }
}

inline const char* stream_type_name(StreamType t) {
    switch (t) {
        case StreamType::Quotes:   return "Quotes";
        case StreamType::Deals:    return "Deals";
        case StreamType::AuxInfo:  return "AuxInfo";
        case StreamType::OrderLog: return "OrderLog";
        default:                   return "Unknown";
    }
}

enum class Side : uint8_t {
    Buy     = 1,
    Sell    = 2,
    Unknown = 0
};

inline Side side_from_u8(uint8_t v) {
    switch (v) {
        case 1: return Side::Buy;
        case 2: return Side::Sell;
        default: return Side::Unknown;
    }
}

inline const char* side_name(Side s) {
    switch (s) {
        case Side::Buy:     return "BUY";
        case Side::Sell:    return "SELL";
        default:            return "UNKNOWN";
    }
}

// --- OrdLog entry flags (OLEntryFlags) ---
struct OLEntryFlags {
    static constexpr uint8_t DateTime   = 1;
    static constexpr uint8_t OrderId    = 1 << 1;
    static constexpr uint8_t Price      = 1 << 2;
    static constexpr uint8_t Amount     = 1 << 3;
    static constexpr uint8_t AmountRest = 1 << 4;
    static constexpr uint8_t DealId     = 1 << 5;
    static constexpr uint8_t DealPrice  = 1 << 6;
    static constexpr uint8_t OI         = 1 << 7;
};

// --- OrdLog order flags (OLFlags) ---
struct OLFlags {
    static constexpr uint16_t NonZeroReplAct  = 1;
    static constexpr uint16_t NewSession      = 1 << 1;
    static constexpr uint16_t Add             = 1 << 2;
    static constexpr uint16_t Fill            = 1 << 3;
    static constexpr uint16_t Buy             = 1 << 4;
    static constexpr uint16_t Sell            = 1 << 5;
    static constexpr uint16_t Snapshot        = 1 << 6;
    static constexpr uint16_t Quote           = 1 << 7;
    static constexpr uint16_t Counter         = 1 << 8;
    static constexpr uint16_t NonSystem       = 1 << 9;
    static constexpr uint16_t TxEnd           = 1 << 10;
    static constexpr uint16_t FillOrKill      = 1 << 11;
    static constexpr uint16_t Moved           = 1 << 12;
    static constexpr uint16_t Canceled        = 1 << 13;
    static constexpr uint16_t CanceledGroup   = 1 << 14;
    static constexpr uint16_t CrossTrade      = 1 << 15;
};

inline bool has_flag(uint16_t flags, uint16_t flag) {
    return (flags & flag) != 0;
}

inline bool has_flag(uint8_t flags, uint8_t flag) {
    return (flags & flag) != 0;
}

enum class OLMsgType : uint8_t {
    Add,
    Fill,
    Cancel,
    Remove,
    Moved,
    Unknown
};

inline const char* ol_msg_type_name(OLMsgType t) {
    switch (t) {
        case OLMsgType::Add:     return "ADD";
        case OLMsgType::Fill:    return "FILL";
        case OLMsgType::Cancel:  return "CANCEL";
        case OLMsgType::Remove:  return "REMOVE";
        case OLMsgType::Moved:   return "MOVED";
        default:                 return "UNKNOWN";
    }
}

// --- Deal flags ---
struct DealFlags {
    static constexpr uint8_t Timestamp = 1 << 2;
    static constexpr uint8_t DealId    = 1 << 3;
    static constexpr uint8_t OrderId   = 1 << 4;
    static constexpr uint8_t Price     = 1 << 5;
    static constexpr uint8_t Amount    = 1 << 6;
    static constexpr uint8_t OI        = 1 << 7;
};

// --- AuxInfo flags ---
struct AuxInfoFlags {
    static constexpr uint8_t Timestamp   = 1;
    static constexpr uint8_t AskTotal    = 1 << 1;
    static constexpr uint8_t BidTotal    = 1 << 2;
    static constexpr uint8_t OI          = 1 << 3;
    static constexpr uint8_t Price       = 1 << 4;
    static constexpr uint8_t SessionInfo = 1 << 5;
    static constexpr uint8_t Rate        = 1 << 6;
    static constexpr uint8_t Message     = 1 << 7;
};

// --- Data structures ---

struct Header {
    int64_t recording_time = 0;  // .NET ticks
    uint8_t version = 0;
    StreamType stream = StreamType::Unknown;
    std::string instrument;
    std::string recorder;
    std::string comment;
};

struct OrderLogRecord {
    Timestamp frame_time_delta = 0;
    Timestamp timestamp = 0;
    UID order_id = 0;
    Price price = 0;
    Volume amount = 0;
    Volume amount_rest = 0;
    UID deal_id = 0;
    Price deal_price = 0;
    Volume oi = 0;
    uint16_t order_flags = 0;
    uint8_t entry_flags = 0;
    Side side = Side::Unknown;
    OLMsgType event = OLMsgType::Unknown;
};

struct QuoteLevel {
    Price price = 0;
    Volume volume = 0;  // positive = ask, negative = bid
};

struct QuotesRecord {
    Timestamp frame_time_delta = 0;
    std::vector<QuoteLevel> levels;
};

struct DealRecord {
    Timestamp frame_time_delta = 0;
    Timestamp timestamp = 0;
    Side side = Side::Unknown;
    UID deal_id = 0;
    UID order_id = 0;
    Price price = 0;
    Volume amount = 0;
    Volume oi = 0;
};

struct AuxInfoRecord {
    Timestamp frame_time_delta = 0;
    Timestamp timestamp = 0;
    Price price = 0;
    Volume ask_total = 0;
    Volume bid_total = 0;
    Volume oi = 0;
    Price hi_limit = 0;
    Price low_limit = 0;
    double deposit = 0.0;
    double rate = 0.0;
    std::string message;
};

}  // namespace qsh
