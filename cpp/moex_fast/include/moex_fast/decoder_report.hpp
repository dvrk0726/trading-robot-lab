#pragma once
#include "moex_fast/decoder_types.hpp"
#include <string>

namespace moex_fast {

// Generate deterministic text report from a decode result.
std::string decode_text_report(const DecodeResult& result,
                                const std::string& templates_path,
                                const std::string& templates_sha256,
                                std::size_t input_size,
                                const std::string& input_sha256);

// Generate deterministic JSON report from a decode result.
std::string decode_json_report(const DecodeResult& result,
                                const std::string& templates_path,
                                const std::string& templates_sha256,
                                std::size_t input_size,
                                const std::string& input_sha256);

}  // namespace moex_fast
