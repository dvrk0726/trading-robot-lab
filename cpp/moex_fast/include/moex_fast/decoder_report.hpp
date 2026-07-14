#pragma once
#include "moex_fast/decoder_types.hpp"
#include <string>

namespace moex_fast {

std::string decode_text_report(const DecodeResult& result,
                                const std::string& templates_path,
                                std::size_t templates_size,
                                const std::string& templates_sha256,
                                std::size_t input_size,
                                const std::string& input_sha256);
std::string decode_json_report(const DecodeResult& result,
                                const std::string& templates_path,
                                std::size_t templates_size,
                                const std::string& templates_sha256,
                                std::size_t input_size,
                                const std::string& input_sha256);

}  // namespace moex_fast
