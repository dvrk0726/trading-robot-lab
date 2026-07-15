#pragma once
#include <cstdint>
#include <cstddef>
#include <span>

namespace moex::spectra {

enum class PreambleByteOrder : std::uint8_t {
    LittleEndian,
    BigEndian
};

enum class FeedSide : std::uint8_t {
    A,
    B
};

struct LogicalFeedId {
    std::uint32_t value{};
};

struct DatagramView {
    LogicalFeedId feed;
    FeedSide side;
    std::uint64_t capture_index;
    std::uint64_t capture_monotonic_ns;
    std::span<const std::uint8_t> payload;
};

struct FramedMessageView {
    LogicalFeedId feed{};
    FeedSide side{FeedSide::A};
    std::uint32_t msg_seq_num{};
    std::uint64_t capture_index{};
    std::uint64_t capture_monotonic_ns{};
    std::span<const std::uint8_t> fast_body;
};

struct FramingLimits {
    std::size_t max_datagram_bytes;
};

enum class FrameCode : std::uint8_t {
    Ok,
    InvalidConfig,
    DatagramTooShort,
    DatagramTooLarge,
    EmptyFastBody
};

struct FrameResult {
    FrameCode code;
};

FrameResult frame_udp_message(
    const DatagramView& input,
    PreambleByteOrder byte_order,
    FramingLimits limits,
    FramedMessageView& output
) noexcept;

} // namespace moex::spectra
