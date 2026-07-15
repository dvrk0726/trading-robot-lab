#include "moex_fast/spectra_udp_framing.hpp"

namespace moex::spectra {

static constexpr std::uint32_t read_u32_le(const std::uint8_t* p) noexcept {
    return static_cast<std::uint32_t>(p[0])
         | (static_cast<std::uint32_t>(p[1]) << 8)
         | (static_cast<std::uint32_t>(p[2]) << 16)
         | (static_cast<std::uint32_t>(p[3]) << 24);
}

static constexpr std::uint32_t read_u32_be(const std::uint8_t* p) noexcept {
    return static_cast<std::uint32_t>(p[3])
         | (static_cast<std::uint32_t>(p[2]) << 8)
         | (static_cast<std::uint32_t>(p[1]) << 16)
         | (static_cast<std::uint32_t>(p[0]) << 24);
}

static void reset_output(FramedMessageView& out) noexcept {
    out.feed = LogicalFeedId{};
    out.side = FeedSide::A;
    out.msg_seq_num = 0;
    out.capture_index = 0;
    out.capture_monotonic_ns = 0;
    out.fast_body = {};
}

FrameResult frame_udp_message(
    const DatagramView& input,
    PreambleByteOrder byte_order,
    FramingLimits limits,
    FramedMessageView& output
) noexcept {
    reset_output(output);

    if (limits.max_datagram_bytes < 5 ||
        (byte_order != PreambleByteOrder::LittleEndian &&
         byte_order != PreambleByteOrder::BigEndian)) {
        return {FrameCode::InvalidConfig};
    }

    const auto size = input.payload.size();

    if (size < 4) {
        return {FrameCode::DatagramTooShort};
    }

    if (size == 4) {
        return {FrameCode::EmptyFastBody};
    }

    if (size > limits.max_datagram_bytes) {
        return {FrameCode::DatagramTooLarge};
    }

    const std::uint8_t* data = input.payload.data();

    const std::uint32_t seq = (byte_order == PreambleByteOrder::LittleEndian)
        ? read_u32_le(data)
        : read_u32_be(data);

    output.feed = input.feed;
    output.side = input.side;
    output.msg_seq_num = seq;
    output.capture_index = input.capture_index;
    output.capture_monotonic_ns = input.capture_monotonic_ns;
    output.fast_body = input.payload.subspan(4);

    return {FrameCode::Ok};
}

} // namespace moex::spectra
