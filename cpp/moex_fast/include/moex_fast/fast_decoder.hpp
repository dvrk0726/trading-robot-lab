#pragma once
#include "moex_fast/decoder_types.hpp"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <functional>

namespace moex_fast {

// Decoder session for one logical ordered source stream.
class DecoderSession {
public:
    DecoderSession(const CompiledTemplateSet& templates, const DecodeLimits& limits = {});
    ~DecoderSession();

    DecoderSession(const DecoderSession&) = delete;
    DecoderSession& operator=(const DecoderSession&) = delete;
    DecoderSession(DecoderSession&&) noexcept;
    DecoderSession& operator=(DecoderSession&&) noexcept;

    // Decode exactly one FAST message from the byte span.
    DecodeResult decode_one(const std::uint8_t* data, std::size_t size);

    // Like decode_one, but trailing bytes after one message are an error.
    DecodeResult decode_exact(const std::uint8_t* data, std::size_t size);

    // Explicit reset: clears previous-template-ID state.
    void reset();

    // Session fingerprint capturing previous-template-ID state for rollback testing.
    SessionFingerprint fingerprint() const;

    // Access compiled templates.
    const CompiledTemplateSet& templates() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace moex_fast
