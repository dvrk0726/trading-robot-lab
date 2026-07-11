#pragma once
#include "moex_fast/decoder_types.hpp"
#include <string>

namespace moex_fast {

// Compile templates.xml into an immutable CompiledTemplateSet.
CompileResult compile_templates(const std::string& xml_path, const CompileLimits& limits = {});

// Compile from in-memory XML string (for testing).
CompileResult compile_templates_from_string(const std::string& xml_content, const CompileLimits& limits = {});

// Compute SHA-256 of arbitrary bytes (hex lowercase).
std::string compute_sha256_bytes(const std::uint8_t* data, std::size_t len);
std::string compute_sha256_string(const std::string& s);

// Validate operator/type combination for the accepted profile.
// Returns true if supported, false if must be rejected at compile time.
bool is_supported_op_type(OpKind op, DecWireType wire_type, bool is_mandatory);

}  // namespace moex_fast
