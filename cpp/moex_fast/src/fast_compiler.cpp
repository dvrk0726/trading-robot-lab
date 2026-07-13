#include "moex_fast/fast_compiler.hpp"
#include <pugixml.hpp>
#include <algorithm>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <functional>
#include <queue>

namespace moex_fast {

namespace {

// --- SHA-256 ---
struct SHA256Ctx {
    std::uint32_t state[8];
    std::uint64_t count;
    std::uint8_t buf[64];
};

constexpr std::uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

std::uint32_t rotr32(std::uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
std::uint32_t ch32(std::uint32_t x, std::uint32_t y, std::uint32_t z) { return (x & y) ^ (~x & z); }
std::uint32_t maj32(std::uint32_t x, std::uint32_t y, std::uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }
std::uint32_t sigma0_32(std::uint32_t x) { return rotr32(x, 2) ^ rotr32(x, 13) ^ rotr32(x, 22); }
std::uint32_t sigma1_32(std::uint32_t x) { return rotr32(x, 6) ^ rotr32(x, 11) ^ rotr32(x, 25); }
std::uint32_t gamma0_32(std::uint32_t x) { return rotr32(x, 7) ^ rotr32(x, 18) ^ (x >> 3); }
std::uint32_t gamma1_32(std::uint32_t x) { return rotr32(x, 17) ^ rotr32(x, 19) ^ (x >> 10); }

void sha256_transform(SHA256Ctx& ctx, const std::uint8_t block[64]) {
    std::uint32_t W[64];
    for (int i = 0; i < 16; ++i) {
        W[i] = (static_cast<std::uint32_t>(block[i * 4]) << 24) |
               (static_cast<std::uint32_t>(block[i * 4 + 1]) << 16) |
               (static_cast<std::uint32_t>(block[i * 4 + 2]) << 8) |
               static_cast<std::uint32_t>(block[i * 4 + 3]);
    }
    for (int i = 16; i < 64; ++i) {
        W[i] = gamma1_32(W[i - 2]) + W[i - 7] + gamma0_32(W[i - 15]) + W[i - 16];
    }
    std::uint32_t a = ctx.state[0], b = ctx.state[1], c = ctx.state[2], d = ctx.state[3];
    std::uint32_t e = ctx.state[4], f = ctx.state[5], g = ctx.state[6], h = ctx.state[7];
    for (int i = 0; i < 64; ++i) {
        std::uint32_t T1 = h + sigma1_32(e) + ch32(e, f, g) + K[i] + W[i];
        std::uint32_t T2 = sigma0_32(a) + maj32(a, b, c);
        h = g; g = f; f = e; e = d + T1;
        d = c; c = b; b = a; a = T1 + T2;
    }
    ctx.state[0] += a; ctx.state[1] += b; ctx.state[2] += c; ctx.state[3] += d;
    ctx.state[4] += e; ctx.state[5] += f; ctx.state[6] += g; ctx.state[7] += h;
}

void sha256_init(SHA256Ctx& ctx) {
    ctx.state[0] = 0x6a09e667; ctx.state[1] = 0xbb67ae85;
    ctx.state[2] = 0x3c6ef372; ctx.state[3] = 0xa54ff53a;
    ctx.state[4] = 0x510e527f; ctx.state[5] = 0x9b05688c;
    ctx.state[6] = 0x1f83d9ab; ctx.state[7] = 0x5be0cd19;
    ctx.count = 0;
    std::memset(ctx.buf, 0, 64);
}

void sha256_update(SHA256Ctx& ctx, const std::uint8_t* data, std::size_t len) {
    std::size_t idx = static_cast<std::size_t>(ctx.count & 63);
    ctx.count += len;
    if (idx) {
        std::size_t part = 64 - idx;
        if (len >= part) {
            std::memcpy(ctx.buf + idx, data, part);
            sha256_transform(ctx, ctx.buf);
            data += part; len -= part; idx = 0;
        } else {
            std::memcpy(ctx.buf + idx, data, len);
            return;
        }
    }
    while (len >= 64) { sha256_transform(ctx, data); data += 64; len -= 64; }
    if (len) std::memcpy(ctx.buf + idx, data, len);
}

void sha256_final(SHA256Ctx& ctx, std::uint8_t hash[32]) {
    std::size_t idx = static_cast<std::size_t>(ctx.count & 63);
    ctx.buf[idx++] = 0x80;
    if (idx > 56) { std::memset(ctx.buf + idx, 0, 64 - idx); sha256_transform(ctx, ctx.buf); idx = 0; }
    std::memset(ctx.buf + idx, 0, 56 - idx);
    std::uint64_t bits = ctx.count * 8;
    for (int i = 0; i < 8; ++i) ctx.buf[56 + i] = static_cast<std::uint8_t>(bits >> (56 - i * 8));
    sha256_transform(ctx, ctx.buf);
    for (int i = 0; i < 8; ++i) {
        hash[i * 4]     = static_cast<std::uint8_t>(ctx.state[i] >> 24);
        hash[i * 4 + 1] = static_cast<std::uint8_t>(ctx.state[i] >> 16);
        hash[i * 4 + 2] = static_cast<std::uint8_t>(ctx.state[i] >> 8);
        hash[i * 4 + 3] = static_cast<std::uint8_t>(ctx.state[i]);
    }
}

std::string hash_to_hex(const std::uint8_t hash[32]) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (int i = 0; i < 32; ++i) oss << std::setw(2) << static_cast<int>(hash[i]);
    return oss.str();
}

// --- Compiler context ---
struct CompileCtx {
    CompileLimits limits{};
    std::vector<CompileIssue> issues{};
    std::set<std::uint32_t> seen_ids{};
    std::map<std::string, std::string> dict_key_to_path{};

    void error(const std::string& code, const std::string& msg, const std::string& path = "") {
        issues.push_back({code, msg, path});
    }

    bool has_errors() const {
        for (const auto& issue : issues) {
            // All issues are considered errors that make the compiled set unusable
            if (!issue.code.empty()) return true;
        }
        return false;
    }
};

DecWireType element_to_wire_type(const char* name) {
    if (!name) return DecWireType::uInt32;
    std::string n(name);
    if (n == "uInt32" || n == "uint32") return DecWireType::uInt32;
    if (n == "uInt64" || n == "uint64") return DecWireType::uInt64;
    if (n == "int32") return DecWireType::Int32;
    if (n == "int64") return DecWireType::Int64;
    if (n == "string") return DecWireType::AsciiString;
    if (n == "unicode") return DecWireType::UnicodeString;
    if (n == "byteVector" || n == "byte-vector") return DecWireType::ByteVector;
    if (n == "decimal") return DecWireType::Decimal;
    if (n == "sequence") return DecWireType::Sequence;
    if (n == "group") return DecWireType::Group;
    return DecWireType::uInt32;
}

bool is_known_field_element(const std::string& n) {
    return n == "string" || n == "uInt32" || n == "uint32" ||
           n == "uInt64" || n == "uint64" || n == "int32" ||
           n == "int64" || n == "decimal" || n == "unicode" ||
           n == "byteVector" || n == "byte-vector" ||
           n == "sequence" || n == "group" || n == "length" ||
           n == "exponent" || n == "mantissa";
}

bool is_operator_element(const std::string& n) {
    return n == "constant" || n == "default" || n == "copy" ||
           n == "increment" || n == "delta" || n == "tail";
}

bool is_reference_element(const std::string& n) {
    return n == "typeRef" || n == "templateRef" || n == "groupRef";
}

bool is_xmlns_attr(const std::string& name) {
    return name == "xmlns" || (name.size() > 6 && name.substr(0, 6) == "xmlns:");
}

// Concatenate all direct PCDATA and CDATA child nodes of an operator in document order.
// Comments and processing instructions are ignored; element children are not included
// (they are governed by the existing unknown_element structural validation).
std::string collect_operator_direct_text(pugi::xml_node op_node) {
    std::string result;
    for (auto child : op_node.children()) {
        auto type = child.type();
        if (type == pugi::node_pcdata || type == pugi::node_cdata) {
            result += child.value();
        }
    }
    return result;
}

void validate_element_attributes(pugi::xml_node node,
                                  const std::set<std::string>& allowed,
                                  CompileCtx& ctx, const std::string& desc) {
    for (auto attr : node.attributes()) {
        std::string name(attr.name());
        if (is_xmlns_attr(name)) continue;
        if (allowed.find(name) == allowed.end()) {
            ctx.error("unknown_attribute",
                      "Unknown attribute '" + name + "' on " + desc, desc);
        }
    }
}

}  // namespace (anonymous)

// Normative operator/type matrix for the accepted MOEX SPECTRA profile
bool is_supported_op_type(OpKind op, DecWireType wire_type, bool is_mandatory) {
    (void)is_mandatory;
    switch (op) {
        case OpKind::None:
            return true;  // all types supported with none
        case OpKind::Constant:
            return wire_type == DecWireType::uInt32 || wire_type == DecWireType::uInt64 ||
                   wire_type == DecWireType::Int32 || wire_type == DecWireType::Int64 ||
                   wire_type == DecWireType::AsciiString || wire_type == DecWireType::UnicodeString;
        case OpKind::Default:
            return wire_type == DecWireType::uInt32 || wire_type == DecWireType::uInt64 ||
                   wire_type == DecWireType::Int32 || wire_type == DecWireType::Int64 ||
                   wire_type == DecWireType::AsciiString || wire_type == DecWireType::UnicodeString ||
                   wire_type == DecWireType::ByteVector || wire_type == DecWireType::Decimal;
        case OpKind::Copy:
            return wire_type == DecWireType::uInt32 || wire_type == DecWireType::uInt64 ||
                   wire_type == DecWireType::Int32 || wire_type == DecWireType::Int64 ||
                   wire_type == DecWireType::AsciiString || wire_type == DecWireType::UnicodeString ||
                   wire_type == DecWireType::ByteVector || wire_type == DecWireType::Decimal;
        case OpKind::Increment:
            return wire_type == DecWireType::uInt32 || wire_type == DecWireType::uInt64 ||
                   wire_type == DecWireType::Int32 || wire_type == DecWireType::Int64;
        case OpKind::Delta:
            return wire_type == DecWireType::uInt32 || wire_type == DecWireType::uInt64 ||
                   wire_type == DecWireType::Int32 || wire_type == DecWireType::Int64 ||
                   wire_type == DecWireType::AsciiString || wire_type == DecWireType::UnicodeString ||
                   wire_type == DecWireType::ByteVector || wire_type == DecWireType::Decimal;
        case OpKind::Tail:
            return wire_type == DecWireType::AsciiString || wire_type == DecWireType::UnicodeString ||
                   wire_type == DecWireType::ByteVector;
    }
    return false;
}

namespace {

bool parse_u32(const std::string& text, std::uint32_t& out) {
    if (text.empty()) return false;
    std::uint64_t val = 0;
    for (char c : text) {
        if (c < '0' || c > '9') return false;
        if (val > UINT64_MAX / 10) return false;
        val *= 10;
        std::uint64_t d = static_cast<std::uint64_t>(c - '0');
        if (val > UINT64_MAX - d) return false;
        val += d;
    }
    if (val > UINT32_MAX) return false;
    out = static_cast<std::uint32_t>(val);
    return true;
}

bool parse_u64(const std::string& text, std::uint64_t& out) {
    if (text.empty()) return false;
    std::uint64_t val = 0;
    for (char c : text) {
        if (c < '0' || c > '9') return false;
        if (val > UINT64_MAX / 10) return false;
        val *= 10;
        std::uint64_t d = static_cast<std::uint64_t>(c - '0');
        if (val > UINT64_MAX - d) return false;
        val += d;
    }
    out = val;
    return true;
}

bool parse_i32(const std::string& text, std::int32_t& out) {
    if (text.empty()) return false;
    const char* p = text.data();
    const char* e = p + text.size();
    bool neg = false;
    if (*p == '-') { neg = true; ++p; }
    if (p == e) return false;
    std::uint64_t val = 0;
    for (; p < e; ++p) {
        char c = *p;
        if (c < '0' || c > '9') return false;
        if (val > UINT64_MAX / 10) return false;
        val *= 10;
        std::uint64_t d = static_cast<std::uint64_t>(c - '0');
        if (val > UINT64_MAX - d) return false;
        val += d;
    }
    if (neg) {
        if (val > static_cast<std::uint64_t>(INT32_MAX) + 1) return false;
        out = (val <= static_cast<std::uint64_t>(INT32_MAX))
              ? static_cast<std::int32_t>(-static_cast<std::int64_t>(val))
              : INT32_MIN;
    } else {
        if (val > INT32_MAX) return false;
        out = static_cast<std::int32_t>(val);
    }
    return true;
}

bool parse_i64(const std::string& text, std::int64_t& out) {
    if (text.empty()) return false;
    const char* p = text.data();
    const char* e = p + text.size();
    bool neg = false;
    if (*p == '-') { neg = true; ++p; }
    if (p == e) return false;
    std::uint64_t val = 0;
    for (; p < e; ++p) {
        char c = *p;
        if (c < '0' || c > '9') return false;
        if (val > UINT64_MAX / 10) return false;
        val *= 10;
        std::uint64_t d = static_cast<std::uint64_t>(c - '0');
        if (val > UINT64_MAX - d) return false;
        val += d;
    }
    if (neg) {
        if (val > static_cast<std::uint64_t>(INT64_MAX) + 1) return false;
        out = (val <= static_cast<std::uint64_t>(INT64_MAX))
              ? -static_cast<std::int64_t>(val)
              : INT64_MIN;
    } else {
        if (val > INT64_MAX) return false;
        out = static_cast<std::int64_t>(val);
    }
    return true;
}

bool validate_ascii_static(const std::string& val) {
    constexpr std::size_t kMaxStaticBytes = 1024 * 1024;
    if (val.size() > kMaxStaticBytes) return false;
    for (unsigned char c : val) {
        if (c == 0 || c > 0x7F) return false;
    }
    return true;
}

bool validate_unicode_static(const std::string& val) {
    constexpr std::size_t kMaxStaticBytes = 1024 * 1024;
    if (val.size() > kMaxStaticBytes) return false;
    const unsigned char* p = reinterpret_cast<const unsigned char*>(val.data());
    const unsigned char* e = p + val.size();
    while (p < e) {
        std::uint32_t cp = 0;
        std::uint32_t needed = 0;
        if (*p <= 0x7F) { cp = *p; needed = 1; }
        else if ((*p & 0xE0) == 0xC0) { cp = *p & 0x1F; needed = 2; }
        else if ((*p & 0xF0) == 0xE0) { cp = *p & 0x0F; needed = 3; }
        else if ((*p & 0xF8) == 0xF0) { cp = *p & 0x07; needed = 4; }
        else return false;
        if (static_cast<std::uint32_t>(e - p) < needed) return false;
        for (std::uint32_t i = 1; i < needed; ++i) {
            if ((p[i] & 0xC0) != 0x80) return false;
            cp = (cp << 6) | (p[i] & 0x3F);
        }
        if (needed == 2 && cp < 0x80) return false;
        if (needed == 3 && cp < 0x800) return false;
        if (needed == 4 && (cp < 0x10000 || cp > 0x10FFFF)) return false;
        if (cp >= 0xD800 && cp <= 0xDFFF) return false;
        if (cp > 0x10FFFF) return false;
        p += needed;
    }
    return true;
}

std::string build_dict_key(DictScope scope, const std::string& template_name,
                           const std::string& field_name, DecWireType wire_type,
                           const std::string& explicit_key, bool is_exponent, bool is_mantissa) {
    std::ostringstream oss;
    oss << dict_scope_name(scope) << ":";
    if (!explicit_key.empty()) {
        oss << explicit_key;
    } else {
        oss << template_name << "." << field_name;
        if (is_exponent) oss << ".exponent";
        if (is_mantissa) oss << ".mantissa";
    }
    oss << ":" << dec_wire_type_name(wire_type);
    return oss.str();
}

OpInstruction parse_operator(pugi::xml_node field_node, DecWireType wire_type,
                             const std::string& template_name, const std::string& field_name,
                             CompileCtx& ctx, const std::string& field_path) {
    OpInstruction op;
    op.kind = OpKind::None;

    for (auto child : field_node.children()) {
        std::string name(child.name());
        if (!is_operator_element(name)) continue;

        // Validate: operator must not have element children
        for (auto opchild : child.children()) {
            std::string opchild_name(opchild.name());
            if (!opchild_name.empty()) {
                ctx.error("unknown_element",
                          "Nested element <" + opchild_name + "> in operator <" + name + "> of " + field_path, field_path);
            }
        }

        if (name == "constant") {
            op.kind = OpKind::Constant;
            op.has_constant = true;
            validate_element_attributes(child, {"value"}, ctx, field_path);
            std::string val_text = collect_operator_direct_text(child);
            std::string val_attr_str;
            auto val_attr = child.attribute("value");
            if (val_attr) val_attr_str = val_attr.as_string("");
            if (!val_text.empty() && val_attr) {
                ctx.error("ambiguous_operator_value",
                          "Operator <constant> in " + field_path + " has both direct text and value attribute", field_path);
            } else if (val_text.empty() && val_attr) {
                val_text = val_attr_str;
            } else if (val_text.empty() && !val_attr) {
                val_text = "";
            }
            if (wire_type == DecWireType::ByteVector) {
                if (!val_text.empty()) {
                    ctx.error("invalid_constant_value",
                              "Non-empty byte-vector constant not supported for " + field_path, field_path);
                }
            } else if (wire_type == DecWireType::AsciiString) {
                op.constant_str = val_text;
                if (!validate_ascii_static(val_text)) {
                    ctx.error("invalid_constant_value",
                              "Invalid ASCII constant value for " + field_path, field_path);
                }
            } else if (wire_type == DecWireType::UnicodeString) {
                op.constant_str = val_text;
                if (!validate_unicode_static(val_text)) {
                    ctx.error("invalid_constant_value",
                              "Invalid Unicode constant value for " + field_path, field_path);
                }
            } else if (wire_type == DecWireType::uInt32 || wire_type == DecWireType::uInt64) {
                std::uint64_t tmp = 0;
                if (wire_type == DecWireType::uInt32) {
                    std::uint32_t tmp32 = 0;
                    if (!parse_u32(val_text, tmp32)) {
                        ctx.error("invalid_constant_value",
                                  "Invalid constant value '" + val_text + "' for uInt32 " + field_path +
                                  " (range 0..4294967295)", field_path);
                    } else {
                        tmp = tmp32;
                    }
                } else {
                    if (!parse_u64(val_text, tmp)) {
                        ctx.error("invalid_constant_value",
                                  "Invalid constant value '" + val_text + "' for uInt64 " + field_path +
                                  " (range 0..18446744073709551615)", field_path);
                    }
                }
                op.constant_uint = tmp;
            } else {
                std::int64_t tmp = 0;
                if (wire_type == DecWireType::Int32) {
                    std::int32_t tmp32 = 0;
                    if (!parse_i32(val_text, tmp32)) {
                        ctx.error("invalid_constant_value",
                                  "Invalid constant value '" + val_text + "' for int32 " + field_path +
                                  " (range -2147483648..2147483647)", field_path);
                    } else {
                        tmp = tmp32;
                    }
                } else {
                    if (!parse_i64(val_text, tmp)) {
                        ctx.error("invalid_constant_value",
                                  "Invalid constant value '" + val_text + "' for int64 " + field_path +
                                  " (range -9223372036854775808..9223372036854775807)", field_path);
                    }
                }
                op.constant_int = tmp;
            }
        } else if (name == "default") {
            op.kind = OpKind::Default;
            validate_element_attributes(child, {"value", "key", "dictionary"}, ctx, field_path);
            std::string val_text = collect_operator_direct_text(child);
            std::string val_attr_str;
            auto val_attr = child.attribute("value");
            if (val_attr) val_attr_str = val_attr.as_string("");
            if (!val_text.empty() && val_attr) {
                ctx.error("ambiguous_operator_value",
                          "Operator <default> in " + field_path + " has both direct text and value attribute", field_path);
            } else if (val_text.empty() && val_attr) {
                val_text = val_attr_str;
            } else if (val_text.empty() && !val_attr) {
                val_text = "";
            }
            if (!val_text.empty()) {
                op.has_initial = true;
                if (wire_type == DecWireType::ByteVector) {
                    ctx.error("invalid_default_value",
                              "Non-empty byte-vector default initial value not supported for " + field_path, field_path);
                } else if (wire_type == DecWireType::AsciiString) {
                    op.initial_str = val_text;
                    if (!validate_ascii_static(val_text)) {
                        ctx.error("invalid_default_value",
                                  "Invalid ASCII default value for " + field_path, field_path);
                    }
                } else if (wire_type == DecWireType::UnicodeString) {
                    op.initial_str = val_text;
                    if (!validate_unicode_static(val_text)) {
                        ctx.error("invalid_default_value",
                                  "Invalid Unicode default value for " + field_path, field_path);
                    }
                } else if (wire_type == DecWireType::uInt32 || wire_type == DecWireType::uInt64) {
                    if (wire_type == DecWireType::uInt32) {
                        std::uint32_t tmp32 = 0;
                        if (!parse_u32(val_text, tmp32)) {
                            ctx.error("invalid_default_value",
                                      "Invalid default value '" + val_text + "' for uInt32 " + field_path +
                                      " (range 0..4294967295)", field_path);
                        } else {
                            op.initial_uint = tmp32;
                        }
                    } else {
                        std::uint64_t tmp64 = 0;
                        if (!parse_u64(val_text, tmp64)) {
                            ctx.error("invalid_default_value",
                                      "Invalid default value '" + val_text + "' for uInt64 " + field_path +
                                      " (range 0..18446744073709551615)", field_path);
                        } else {
                            op.initial_uint = tmp64;
                        }
                    }
                } else {
                    if (wire_type == DecWireType::Int32) {
                        std::int32_t tmp32 = 0;
                        if (!parse_i32(val_text, tmp32)) {
                            ctx.error("invalid_default_value",
                                      "Invalid default value '" + val_text + "' for int32 " + field_path +
                                      " (range -2147483648..2147483647)", field_path);
                        } else {
                            op.initial_int = tmp32;
                        }
                    } else {
                        if (!parse_i64(val_text, op.initial_int)) {
                            ctx.error("invalid_default_value",
                                      "Invalid default value '" + val_text + "' for int64 " + field_path +
                                      " (range -9223372036854775808..9223372036854775807)", field_path);
                        }
                    }
                }
            }
            auto key_attr = child.attribute("key");
            if (key_attr) op.explicit_key = key_attr.as_string("");
            auto dict_attr = child.attribute("dictionary");
            if (dict_attr) {
                std::string ds = dict_attr.as_string("");
                if (ds == "global") op.scope = DictScope::Global;
                else if (ds == "template" || ds == "type") op.scope = DictScope::TemplateType;
                else ctx.error("unknown_dictionary_scope",
                               "Unknown dictionary scope '" + ds + "' for " + field_path, field_path);
            }
        } else if (name == "copy") {
            op.kind = OpKind::Copy;
            validate_element_attributes(child, {"value", "key", "dictionary"}, ctx, field_path);
            std::string val_text = collect_operator_direct_text(child);
            std::string val_attr_str;
            auto val_attr = child.attribute("value");
            if (val_attr) val_attr_str = val_attr.as_string("");
            if (!val_text.empty() && val_attr) {
                ctx.error("ambiguous_operator_value",
                          "Operator <copy> in " + field_path + " has both direct text and value attribute", field_path);
            } else if (val_text.empty() && val_attr) {
                val_text = val_attr_str;
            } else if (val_text.empty() && !val_attr) {
                val_text = "";
            }
            if (!val_text.empty()) {
                op.has_initial = true;
                if (wire_type == DecWireType::ByteVector) {
                    ctx.error("invalid_copy_value",
                              "Non-empty byte-vector copy initial value not supported for " + field_path, field_path);
                } else if (wire_type == DecWireType::AsciiString) {
                    op.initial_str = val_text;
                    if (!validate_ascii_static(val_text)) {
                        ctx.error("invalid_copy_value",
                                  "Invalid ASCII copy value for " + field_path, field_path);
                    }
                } else if (wire_type == DecWireType::UnicodeString) {
                    op.initial_str = val_text;
                    if (!validate_unicode_static(val_text)) {
                        ctx.error("invalid_copy_value",
                                  "Invalid Unicode copy value for " + field_path, field_path);
                    }
                } else if (wire_type == DecWireType::uInt32 || wire_type == DecWireType::uInt64) {
                    if (wire_type == DecWireType::uInt32) {
                        std::uint32_t tmp32 = 0;
                        if (!parse_u32(val_text, tmp32)) {
                            ctx.error("invalid_copy_value",
                                      "Invalid copy initial value '" + val_text + "' for uInt32 " + field_path +
                                      " (range 0..4294967295)", field_path);
                        } else {
                            op.initial_uint = tmp32;
                        }
                    } else {
                        std::uint64_t tmp64 = 0;
                        if (!parse_u64(val_text, tmp64)) {
                            ctx.error("invalid_copy_value",
                                      "Invalid copy initial value '" + val_text + "' for uInt64 " + field_path +
                                      " (range 0..18446744073709551615)", field_path);
                        } else {
                            op.initial_uint = tmp64;
                        }
                    }
                } else {
                    if (wire_type == DecWireType::Int32) {
                        std::int32_t tmp32 = 0;
                        if (!parse_i32(val_text, tmp32)) {
                            ctx.error("invalid_copy_value",
                                      "Invalid copy initial value '" + val_text + "' for int32 " + field_path +
                                      " (range -2147483648..2147483647)", field_path);
                        } else {
                            op.initial_int = tmp32;
                        }
                    } else {
                        if (!parse_i64(val_text, op.initial_int)) {
                            ctx.error("invalid_copy_value",
                                      "Invalid copy initial value '" + val_text + "' for int64 " + field_path +
                                      " (range -9223372036854775808..9223372036854775807)", field_path);
                        }
                    }
                }
            }
            auto key_attr = child.attribute("key");
            if (key_attr) op.explicit_key = key_attr.as_string("");
            auto dict_attr = child.attribute("dictionary");
            if (dict_attr) {
                std::string ds = dict_attr.as_string("");
                if (ds == "global") op.scope = DictScope::Global;
                else if (ds == "template" || ds == "type") op.scope = DictScope::TemplateType;
                else ctx.error("unknown_dictionary_scope",
                               "Unknown dictionary scope '" + ds + "' for " + field_path, field_path);
            }
        } else if (name == "increment") {
            op.kind = OpKind::Increment;
            validate_element_attributes(child, {"value", "key", "dictionary"}, ctx, field_path);
            std::string val_text = collect_operator_direct_text(child);
            std::string val_attr_str;
            auto val_attr = child.attribute("value");
            if (val_attr) val_attr_str = val_attr.as_string("");
            if (!val_text.empty() && val_attr) {
                ctx.error("ambiguous_operator_value",
                          "Operator <increment> in " + field_path + " has both direct text and value attribute", field_path);
            } else if (val_text.empty() && val_attr) {
                val_text = val_attr_str;
            } else if (val_text.empty() && !val_attr) {
                val_text = "";
            }
            if (!val_text.empty()) {
                op.has_initial = true;
                if (wire_type == DecWireType::uInt32 || wire_type == DecWireType::uInt64) {
                    if (wire_type == DecWireType::uInt32) {
                        std::uint32_t tmp32 = 0;
                        if (!parse_u32(val_text, tmp32)) {
                            ctx.error("invalid_increment_value",
                                      "Invalid increment initial value '" + val_text + "' for uInt32 " + field_path +
                                      " (range 0..4294967295)", field_path);
                        } else {
                            op.initial_uint = tmp32;
                        }
                    } else {
                        std::uint64_t tmp64 = 0;
                        if (!parse_u64(val_text, tmp64)) {
                            ctx.error("invalid_increment_value",
                                      "Invalid increment initial value '" + val_text + "' for uInt64 " + field_path +
                                      " (range 0..18446744073709551615)", field_path);
                        } else {
                            op.initial_uint = tmp64;
                        }
                    }
                } else {
                    if (wire_type == DecWireType::Int32) {
                        std::int32_t tmp32 = 0;
                        if (!parse_i32(val_text, tmp32)) {
                            ctx.error("invalid_increment_value",
                                      "Invalid increment initial value '" + val_text + "' for int32 " + field_path +
                                      " (range -2147483648..2147483647)", field_path);
                        } else {
                            op.initial_int = tmp32;
                        }
                    } else {
                        if (!parse_i64(val_text, op.initial_int)) {
                            ctx.error("invalid_increment_value",
                                      "Invalid increment initial value '" + val_text + "' for int64 " + field_path +
                                      " (range -9223372036854775808..9223372036854775807)", field_path);
                        }
                    }
                }
            }
            auto key_attr = child.attribute("key");
            if (key_attr) op.explicit_key = key_attr.as_string("");
            auto dict_attr = child.attribute("dictionary");
            if (dict_attr) {
                std::string ds = dict_attr.as_string("");
                if (ds == "global") op.scope = DictScope::Global;
                else if (ds == "template" || ds == "type") op.scope = DictScope::TemplateType;
                else ctx.error("unknown_dictionary_scope",
                               "Unknown dictionary scope '" + ds + "' for " + field_path, field_path);
            }
        } else if (name == "delta") {
            op.kind = OpKind::Delta;
            validate_element_attributes(child, {"key", "dictionary"}, ctx, field_path);
            auto key_attr = child.attribute("key");
            if (key_attr) op.explicit_key = key_attr.as_string("");
            auto dict_attr = child.attribute("dictionary");
            if (dict_attr) {
                std::string ds = dict_attr.as_string("");
                if (ds == "global") op.scope = DictScope::Global;
                else if (ds == "template" || ds == "type") op.scope = DictScope::TemplateType;
                else ctx.error("unknown_dictionary_scope",
                               "Unknown dictionary scope '" + ds + "' for " + field_path, field_path);
            }
        } else if (name == "tail") {
            op.kind = OpKind::Tail;
            validate_element_attributes(child, {"key", "dictionary"}, ctx, field_path);
            auto key_attr = child.attribute("key");
            if (key_attr) op.explicit_key = key_attr.as_string("");
            auto dict_attr = child.attribute("dictionary");
            if (dict_attr) {
                std::string ds = dict_attr.as_string("");
                if (ds == "global") op.scope = DictScope::Global;
                else if (ds == "template" || ds == "type") op.scope = DictScope::TemplateType;
                else ctx.error("unknown_dictionary_scope",
                               "Unknown dictionary scope '" + ds + "' for " + field_path, field_path);
            }
        }

        break;  // one operator per field
    }

    // Build dictionary key and register for collision detection
    if (op.kind != OpKind::None && op.kind != OpKind::Constant) {
        op.dict_key = build_dict_key(op.scope, template_name, field_name, wire_type,
                                      op.explicit_key, false, false);
        // Register dictionary key for collision detection
        auto it = ctx.dict_key_to_path.find(op.dict_key);
        if (it != ctx.dict_key_to_path.end() && it->second != field_path) {
            ctx.error("duplicate_dictionary_key",
                      "Dictionary key collision: '" + op.dict_key + "' used by both " +
                      it->second + " and " + field_path, field_path);
        } else {
            ctx.dict_key_to_path[op.dict_key] = field_path;
        }
    }

    // Validate operator/type combination
    if (op.kind != OpKind::None && !is_supported_op_type(op.kind, wire_type, true)) {
        ctx.error("unsupported_operator_type",
                  "Unsupported operator/type combination: " + std::string(op_kind_name(op.kind)) +
                  " on " + dec_wire_type_name(wire_type) + " field " + field_path, field_path);
    }

    return op;
}

CompiledField parse_decimal_field(pugi::xml_node node, std::uint32_t& field_index,
                                  const std::string& template_name, CompileCtx& ctx,
                                  const std::string& parent_path, bool has_pmap_bit) {
    CompiledField f;
    f.index = field_index;
    f.name = node.attribute("name").as_string("");
    f.wire_type = DecWireType::Decimal;
    f.is_decimal = true;
    f.has_pmap_bit = has_pmap_bit;

    std::string field_path = parent_path.empty() ? f.name : parent_path + "." + f.name;

    // Validate decimal field attributes
    static const std::set<std::string> decimal_attrs = {"id", "name", "presence"};
    validate_element_attributes(node, decimal_attrs, ctx, field_path);

    auto fix_attr = node.attribute("id");
    if (fix_attr) {
        std::string fix_str = fix_attr.as_string("");
        std::int32_t fix_val = 0;
        if (!parse_i32(fix_str, fix_val) || fix_val <= 0) {
            ctx.error("invalid_fix_tag",
                      "Invalid FIX tag '" + fix_str + "' in " + field_path, field_path);
        } else {
            f.has_fix_tag = true;
            f.fix_tag = fix_val;
        }
    }

    auto pres = node.attribute("presence");
    if (pres) {
        std::string pv = pres.as_string("");
        if (pv == "mandatory") {
            f.is_mandatory = true;
        } else if (pv == "optional") {
            f.is_mandatory = false;
        } else {
            ctx.error("unknown_presence", "Unknown decimal presence value '" + pv + "' in " + field_path, field_path);
            f.is_mandatory = true;
        }
    } else {
        f.is_mandatory = true;
    }

    if (f.name.empty()) {
        ctx.error("empty_field_name",
                  "Decimal field in " + parent_path + " has empty name", parent_path);
    }

    auto exponent_node = node.child("exponent");
    auto mantissa_node = node.child("mantissa");

    // Validate decimal children: reject unknown elements and duplicate exponent/mantissa
    {
        int exp_count = 0, man_count = 0;
        for (auto dchild : node.children()) {
            std::string dname(dchild.name());
            if (dname == "exponent") {
                exp_count++;
                if (exp_count > 1) {
                    ctx.error("duplicate_decimal_exponent",
                              "Duplicate <exponent> in decimal " + field_path, field_path);
                }
            } else if (dname == "mantissa") {
                man_count++;
                if (man_count > 1) {
                    ctx.error("duplicate_decimal_mantissa",
                              "Duplicate <mantissa> in decimal " + field_path, field_path);
                }
            } else if (is_operator_element(dname)) {
                ctx.error("unknown_element",
                          "Misplaced operator <" + dname + "> in decimal " + field_path, field_path);
            } else if (dname == "length") {
                ctx.error("unknown_element",
                          "Misplaced <length> in decimal " + field_path, field_path);
            } else {
                ctx.error("unknown_element",
                          "Unknown child <" + dname + "> in decimal " + field_path, field_path);
            }
        }
    }

    if (exponent_node) {
        // Validate exponent attributes (no ordinary attributes)
        validate_element_attributes(exponent_node, {}, ctx, field_path + ".exponent");
        // Validate exponent children: zero or one operator; no other element children
        int exp_op_count = 0;
        for (auto echild : exponent_node.children()) {
            std::string ename(echild.name());
            if (is_operator_element(ename)) {
                exp_op_count++;
                if (exp_op_count > 1) {
                    ctx.error("multiple_operators",
                              "Multiple operators in exponent " + field_path + ".exponent", field_path);
                }
            } else if (!ename.empty()) {
                ctx.error("unknown_element",
                          "Unknown child <" + ename + "> in exponent " + field_path + ".exponent", field_path);
            }
        }
        f.exponent_op = parse_operator(exponent_node, DecWireType::Int32, template_name,
                                        f.name + ".exponent", ctx, field_path + ".exponent");
        f.exponent_op.is_decimal_component = true;
    }
    if (mantissa_node) {
        // Validate mantissa attributes (no ordinary attributes)
        validate_element_attributes(mantissa_node, {}, ctx, field_path + ".mantissa");
        // Validate mantissa children: zero or one operator; no other element children
        int man_op_count = 0;
        for (auto mchild : mantissa_node.children()) {
            std::string mname(mchild.name());
            if (is_operator_element(mname)) {
                man_op_count++;
                if (man_op_count > 1) {
                    ctx.error("multiple_operators",
                              "Multiple operators in mantissa " + field_path + ".mantissa", field_path);
                }
            } else if (!mname.empty()) {
                ctx.error("unknown_element",
                          "Unknown child <" + mname + "> in mantissa " + field_path + ".mantissa", field_path);
            }
        }
        f.mantissa_op = parse_operator(mantissa_node, DecWireType::Int64, template_name,
                                        f.name + ".mantissa", ctx, field_path + ".mantissa");
        f.mantissa_op.is_decimal_component = true;
    }

    if (!exponent_node) {
        f.exponent_op.kind = OpKind::None;
        f.exponent_op.dict_key = build_dict_key(DictScope::Global, template_name,
                                                 f.name + ".exponent", DecWireType::Int32, "", true, false);
    }
    if (!mantissa_node) {
        f.mantissa_op.kind = OpKind::None;
        f.mantissa_op.dict_key = build_dict_key(DictScope::Global, template_name,
                                                 f.name + ".mantissa", DecWireType::Int64, "", false, true);
    }

    // Register decimal component keys for collision detection (skip empty keys from Constant/None)
    std::string exp_path = field_path + ".exponent";
    std::string man_path = field_path + ".mantissa";
    if (!f.exponent_op.dict_key.empty()) {
        auto exp_it = ctx.dict_key_to_path.find(f.exponent_op.dict_key);
        if (exp_it != ctx.dict_key_to_path.end() && exp_it->second != exp_path) {
            ctx.error("duplicate_dictionary_key",
                      "Dictionary key collision: exponent key '" + f.exponent_op.dict_key +
                      "' used by both " + exp_it->second + " and " + exp_path, field_path);
        } else {
            ctx.dict_key_to_path[f.exponent_op.dict_key] = exp_path;
        }
    }
    if (!f.mantissa_op.dict_key.empty()) {
        auto man_it = ctx.dict_key_to_path.find(f.mantissa_op.dict_key);
        if (man_it != ctx.dict_key_to_path.end() && man_it->second != man_path) {
            ctx.error("duplicate_dictionary_key",
                      "Dictionary key collision: mantissa key '" + f.mantissa_op.dict_key +
                      "' used by both " + man_it->second + " and " + man_path, field_path);
        } else {
            ctx.dict_key_to_path[f.mantissa_op.dict_key] = man_path;
        }
    }

    return f;
}

void parse_fields(pugi::xml_node parent, std::vector<CompiledField>& fields,
                  const std::string& template_name, CompileCtx& ctx,
                  const std::string& parent_path, std::uint32_t nesting_depth,
                  std::uint32_t& field_index, bool parent_is_sequence);

CompiledField parse_field(pugi::xml_node node, std::uint32_t& field_index,
                          const std::string& template_name, CompileCtx& ctx,
                          const std::string& parent_path, std::uint32_t nesting_depth) {
    CompiledField f;
    f.index = field_index - 1;  // index already pre-allocated by caller
    f.name = node.attribute("name").as_string("");
    std::string elem_name = node.name();

    std::string field_path = parent_path.empty() ? f.name : parent_path + "." + f.name;

    if (f.name.empty() && elem_name != "length") {
        ctx.error("empty_field_name",
                  "Field in " + (parent_path.empty() ? std::string("template") : parent_path) +
                  " has empty name", field_path);
    }

    auto pres = node.attribute("presence");
    if (!pres) {
        f.is_mandatory = true;
        f.has_pmap_bit = false;
    } else {
        std::string pv = pres.as_string("");
        if (pv == "optional") {
            f.is_mandatory = false;
            f.has_pmap_bit = true;
        } else if (pv == "mandatory") {
            f.is_mandatory = true;
            f.has_pmap_bit = false;
        } else {
            ctx.error("unknown_presence", "Unknown presence value '" + pv + "' in " + field_path, field_path);
            f.is_mandatory = true;
        }
    }

    auto fix_attr = node.attribute("id");
    if (fix_attr) {
        std::string fix_str = fix_attr.as_string("");
        std::int32_t fix_val = 0;
        if (!parse_i32(fix_str, fix_val) || fix_val <= 0) {
            ctx.error("invalid_fix_tag",
                      "Invalid FIX tag '" + fix_str + "' in " + field_path, field_path);
        } else {
            f.has_fix_tag = true;
            f.fix_tag = fix_val;
        }
    }

    if (elem_name == "decimal") {
        std::uint32_t decimal_index = field_index - 1;
        // Decimal fields never consume a field-level presence-map bit.
        // Optionality is conveyed through nullable exponent wire form.
        return parse_decimal_field(node, decimal_index, template_name, ctx, parent_path, false);
    }

    // Validate field attributes (non-decimal fields)
    if (elem_name == "string") {
        static const std::set<std::string> string_attrs = {"id", "name", "presence", "charset"};
        validate_element_attributes(node, string_attrs, ctx, field_path);
    } else if (is_known_field_element(elem_name)) {
        static const std::set<std::string> field_attrs = {"id", "name", "presence"};
        validate_element_attributes(node, field_attrs, ctx, field_path);
    } else if (is_reference_element(elem_name)) {
        static const std::set<std::string> ref_attrs = {"name"};
        validate_element_attributes(node, ref_attrs, ctx, field_path);
    }

    if (elem_name == "sequence") {
        f.wire_type = DecWireType::Sequence;
        f.is_sequence = true;
        // A sequence itself never receives a separate presence-map bit.
        // Optionality is conveyed through the sequence length wire form.
        f.has_pmap_bit = false;

        auto length_node = node.child("length");
        if (!length_node) {
            ctx.error("missing_sequence_length", "Sequence " + field_path + " missing <length>", field_path);
        } else {
            // Check for duplicate length
            int length_count = 0;
            for (auto c : node.children("length")) {
                (void)c;
                length_count++;
            }
            if (length_count > 1) {
                ctx.error("duplicate_sequence_length",
                          "Sequence " + field_path + " has multiple <length> elements", field_path);
            }
        }
        if (length_node) {
            // Validate length attributes
            static const std::set<std::string> length_attrs = {"id", "name"};
            validate_element_attributes(length_node, length_attrs, ctx, field_path + ".length");

            // C. Budget check for length child before allocation
            if (field_index >= ctx.limits.max_fields_per_template) {
                ctx.error("excessive_fields",
                          "Field count exceeds limit in template " + template_name);
                return f;
            }

            f.length_op.kind = OpKind::None;
            f.length_field_index = field_index;
            CompiledField len_field;
            len_field.index = field_index++;
            len_field.name = length_node.attribute("name").as_string("");
            len_field.wire_type = DecWireType::uInt32;
            // Mandatory sequence: length is ordinary uInt32.
            // Optional sequence: length is nullable uInt32 (NULL = absent).
            len_field.is_mandatory = f.is_mandatory;
            len_field.has_pmap_bit = false;
            auto len_fix = length_node.attribute("id");
            if (len_fix) {
                std::string lfix_str = len_fix.as_string("");
                std::int32_t lfix_val = 0;
                if (!parse_i32(lfix_str, lfix_val) || lfix_val <= 0) {
                    ctx.error("invalid_fix_tag",
                              "Invalid FIX tag '" + lfix_str + "' in " + field_path + ".length", field_path);
                } else {
                    len_field.has_fix_tag = true;
                    len_field.fix_tag = lfix_val;
                }
            }

            // Parse length operator if present; validate children
            {
                int len_op_count = 0;
                for (auto lchild : length_node.children()) {
                    std::string lname(lchild.name());
                    if (is_operator_element(lname)) {
                        len_op_count++;
                        if (len_op_count > 1) {
                            ctx.error("multiple_operators",
                                      "Multiple operators in length " + field_path + ".length", field_path);
                        } else {
                            f.length_op = parse_operator(length_node, DecWireType::uInt32, template_name,
                                                         len_field.name, ctx, field_path + ".length");
                        }
                    } else if (!lname.empty()) {
                        ctx.error("unknown_element",
                                  "Unknown child <" + lname + "> in length " + field_path + ".length", field_path);
                    }
                }
            }

            f.children.push_back(len_field);
        }

        f.has_children = true;
        f.entry_has_pmap = true;

        // Reject misplaced elements at sequence level
        for (auto schild : node.children()) {
            std::string sname(schild.name());
            if (is_operator_element(sname) || sname == "exponent" || sname == "mantissa") {
                ctx.error("unknown_element",
                          "Misplaced <" + sname + "> in sequence " + field_path, field_path);
            }
        }

        std::vector<CompiledField> entry_fields;
        parse_fields(node, entry_fields, template_name, ctx, field_path, nesting_depth + 1, field_index, true);

        for (auto& ef : entry_fields) {
            f.children.push_back(std::move(ef));
        }

        return f;
    }

    if (elem_name == "group") {
        f.wire_type = DecWireType::Group;
        f.has_children = true;
        f.has_pmap_bit = !f.is_mandatory;

        parse_fields(node, f.children, template_name, ctx, field_path, nesting_depth + 1, field_index, false);
        return f;
    }

    f.wire_type = element_to_wire_type(elem_name.c_str());

    // MOEX charset semantics for string fields
    if (elem_name == "string") {
        auto charset_attr = node.attribute("charset");
        if (charset_attr) {
            std::string cv = charset_attr.as_string("");
            if (cv == "unicode") {
                f.wire_type = DecWireType::UnicodeString;
            } else {
                ctx.error("unknown_charset",
                          "Unknown charset '" + cv + "' in " + field_path, field_path);
            }
        }
    }

    // Validate scalar field children: zero or one operator; no other element children
    {
        int scalar_op_count = 0;
        for (auto sc : node.children()) {
            std::string scname(sc.name());
            if (is_operator_element(scname)) {
                scalar_op_count++;
                if (scalar_op_count > 1) {
                    ctx.error("multiple_operators",
                              "Multiple operators in scalar " + field_path, field_path);
                }
            } else if (!scname.empty()) {
                ctx.error("unknown_element",
                          "Unknown child <" + scname + "> in scalar " + field_path, field_path);
            }
        }
    }

    f.op = parse_operator(node, f.wire_type, template_name, f.name, ctx, field_path);

    // Presence-map matrix for the accepted MOEX SPECTRA profile:
    // - mandatory field without operator: no field bit
    // - optional field without operator: no field bit (nullable wire; NULL from wire value alone)
    // - mandatory constant: no field bit
    // - optional constant: one field bit
    // - other operators (default/copy/increment/delta/tail): one field bit
    switch (f.op.kind) {
        case OpKind::None:
            f.has_pmap_bit = false;
            break;
        case OpKind::Constant:
            f.has_pmap_bit = !f.is_mandatory;
            break;
        case OpKind::Default:
        case OpKind::Copy:
        case OpKind::Increment:
        case OpKind::Delta:
        case OpKind::Tail:
            f.has_pmap_bit = true;
            break;
    }

    return f;
}

void parse_fields(pugi::xml_node parent, std::vector<CompiledField>& fields,
                  const std::string& template_name, CompileCtx& ctx,
                  const std::string& parent_path, std::uint32_t nesting_depth,
                  std::uint32_t& field_index, bool parent_is_sequence) {
    for (auto child : parent.children()) {
        std::string name(child.name());

        if (is_operator_element(name)) {
            ctx.error("unknown_element",
                      "Misplaced operator <" + name + "> in " +
                      (parent_path.empty() ? std::string("template") : parent_path), parent_path);
            continue;
        }

        if (name == "exponent" || name == "mantissa") {
            ctx.error("unknown_element",
                      "Misplaced <" + name + "> in " +
                      (parent_path.empty() ? std::string("template") : parent_path), parent_path);
            continue;
        }

        if (name == "length") {
            if (parent_is_sequence) continue;
            ctx.error("unknown_element",
                      "Misplaced <length> in " +
                      (parent_path.empty() ? std::string("template") : parent_path), parent_path);
            continue;
        }

        if (!is_known_field_element(name) && !is_reference_element(name)) {
            ctx.error("unknown_element",
                      "Unknown wire-affecting XML element <" + name + "> in " +
                      (parent_path.empty() ? std::string("template") : parent_path), parent_path);
            continue;
        }

        // Handle reference elements (typeRef, templateRef, groupRef)
        if (is_reference_element(name)) {
            static const std::set<std::string> ref_attrs = {"name"};
            validate_element_attributes(child, ref_attrs, ctx, parent_path + "." + name);
            std::string ref_name = child.attribute("name").as_string("");
            if (ref_name.empty()) {
                ctx.error("empty_reference_name",
                          "Reference <" + name + "> in " + parent_path + " has empty name", parent_path);
            } else {
                // References are not supported in the accepted MOEX SPECTRA profile
                // Reject as compile error — the profile must use inline definitions
                ctx.error("unsupported_reference",
                          "Unsupported reference <" + name + " name=\"" + ref_name + "\"> in " +
                          parent_path + ": the accepted profile does not use references", parent_path);
            }
            continue;
        }

        // D. Nesting depth accounting: reject before budget reservation, index increment, or subtree descent
        if (nesting_depth > ctx.limits.max_nesting_depth) {
            ctx.error("excessive_nesting",
                      "Nesting depth exceeds limit in " +
                      (parent_path.empty() ? std::string("template") : parent_path),
                      parent_path);
            return;
        }

        // C. Early field/node accounting: check budget before allocating
        if (field_index >= ctx.limits.max_fields_per_template) {
            ctx.error("excessive_fields",
                      "Field count exceeds limit in template " + template_name);
            return;
        }
        field_index++;

        auto field = parse_field(child, field_index, template_name, ctx, parent_path, nesting_depth);
        if (ctx.has_errors()) return;
        fields.push_back(std::move(field));
    }
}

std::uint32_t count_pmap_bits(const std::vector<CompiledField>& fields) {
    std::uint32_t bits = 0;
    for (const auto& f : fields) {
        if (f.has_pmap_bit) bits++;
    }
    return bits;
}

// Intermediate result from parsing; sealed into CompiledTemplateSet by the friend functions.
struct CompileDraft {
    std::vector<CompiledTemplate> templates;
    std::map<std::uint32_t, std::size_t> id_to_index;
    std::string templates_sha256;
    std::string compiler_version = "0.2.0";
    std::string schema_version = "1.0";
    std::size_t templates_file_size = 0;
    std::vector<CompileIssue> issues;
    bool ok = false;
};

CompileDraft compile_from_doc(pugi::xml_document& doc, const std::string& xml_content,
                               const CompileLimits& limits) {
    CompileDraft draft;
    CompileCtx ctx;
    ctx.limits = limits;

    // A. Hard ceiling validation
    if (limits.max_templates > 4096) {
        ctx.error("compile_limit_exceeds_hard_ceiling",
                  "max_templates exceeds hard ceiling of 4096");
    }
    if (limits.max_fields_per_template > 65535) {
        ctx.error("compile_limit_exceeds_hard_ceiling",
                  "max_fields_per_template exceeds hard ceiling of 65535");
    }
    if (limits.max_nesting_depth > 32) {
        ctx.error("compile_limit_exceeds_hard_ceiling",
                  "max_nesting_depth exceeds hard ceiling of 32");
    }
    if (ctx.has_errors()) {
        draft.issues = std::move(ctx.issues);
        return draft;
    }

    draft.templates_sha256 = compute_sha256_bytes(
        reinterpret_cast<const std::uint8_t*>(xml_content.data()), xml_content.size());
    draft.templates_file_size = xml_content.size();

    auto root = doc.child("templates");
    if (!root) root = doc.child("TemplateConfiguration");
    if (!root) {
        ctx.error("missing_root", "Missing root element <templates>");
        draft.issues = std::move(ctx.issues);
        return draft;
    }

    // Validate root attributes (no ordinary attributes allowed)
    validate_element_attributes(root, {}, ctx, std::string("<") + root.name() + ">");

    // Validate root children: only <template> allowed
    for (auto rchild : root.children()) {
        std::string rname(rchild.name());
        if (rname != "template") {
            ctx.error("unknown_element",
                      "Unknown root child <" + rname + "> in <templates>");
        }
    }

    std::uint32_t template_count = 0;
    for (auto tmpl : root.children("template")) {
        if (template_count >= limits.max_templates) {
            ctx.error("excessive_templates", "Template count exceeds limit");
            break;
        }
        template_count++;

        CompiledTemplate ct;

        auto id_attr = tmpl.attribute("id");
        if (!id_attr) {
            ctx.error("missing_template_id", "Template missing 'id' attribute");
            continue;
        }

        std::string id_str = id_attr.as_string("");
        if (id_str.empty()) {
            ctx.error("missing_template_id", "Template missing 'id' attribute");
            continue;
        }

        std::uint32_t parsed_id = 0;
        if (!parse_u32(id_str, parsed_id)) {
            // Check if it's non-decimal
            bool has_non_digit = false;
            for (char c : id_str) {
                if (c < '0' || c > '9') { has_non_digit = true; break; }
            }
            if (has_non_digit) {
                ctx.error("non_numeric_template_id", "Template has non-numeric id: " + id_str);
            } else {
                ctx.error("template_id_out_of_range", "Template id out of range: " + id_str);
            }
            continue;
        }
        ct.id = parsed_id;
        if (ct.id == 0) {
            ctx.error("invalid_template_id", "Template id cannot be 0");
            continue;
        }

        ct.name = tmpl.attribute("name").as_string("");
        if (ct.name.empty()) {
            ctx.error("empty_template_name",
                      "Template " + std::to_string(ct.id) + " has empty name");
        }

        if (ctx.seen_ids.count(ct.id)) {
            ctx.error("duplicate_template_id",
                      "Duplicate template id: " + std::to_string(ct.id));
            continue;
        }
        ctx.seen_ids.insert(ct.id);

        // Validate template attributes
        validate_element_attributes(tmpl, {"id", "name"}, ctx, "template " + ct.name);

        std::uint32_t field_index = 0;
        parse_fields(tmpl, ct.fields, ct.name, ctx, "", 0, field_index, false);

        ct.total_pmap_bits = count_pmap_bits(ct.fields);

        std::size_t idx = draft.templates.size();
        draft.templates.push_back(std::move(ct));
        draft.id_to_index[draft.templates.back().id] = idx;
    }

    // Any issue makes the compiled set unusable; seal only on zero issues + at least one template
    draft.ok = ctx.issues.empty() && !draft.templates.empty();
    draft.issues = std::move(ctx.issues);
    return draft;
}

}  // namespace

CompileResult compile_templates(const std::string& xml_path, const CompileLimits& limits) {
    CompileResult result;

    std::ifstream file(xml_path, std::ios::binary);
    if (!file) {
        result.issues.push_back({"file_not_found", "Cannot open file: " + xml_path, ""});
        return result;
    }
    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());

    pugi::xml_document doc;
    auto parse_result = doc.load_buffer(content.data(), content.size());
    if (!parse_result) {
        result.issues.push_back({"xml_parse_error",
                                  std::string("XML parse error: ") + parse_result.description(), ""});
        return result;
    }

    auto draft = compile_from_doc(doc, content, limits);
    result.ok = draft.ok;
    result.issues = std::move(draft.issues);
    if (draft.ok) {
        CompiledTemplateSet::Builder b;
        b.templates = std::move(draft.templates);
        b.id_to_index = std::move(draft.id_to_index);
        b.templates_sha256 = std::move(draft.templates_sha256);
        b.compiler_version = std::move(draft.compiler_version);
        b.schema_version = std::move(draft.schema_version);
        b.templates_file_size = draft.templates_file_size;
        result.compiled = CompiledTemplateSet::seal(std::move(b));
    }
    return result;
}

CompileResult compile_templates_from_string(const std::string& xml_content, const CompileLimits& limits) {
    CompileResult result;

    pugi::xml_document doc;
    auto parse_result = doc.load_buffer(xml_content.data(), xml_content.size());
    if (!parse_result) {
        result.issues.push_back({"xml_parse_error",
                                  std::string("XML parse error: ") + parse_result.description(), ""});
        return result;
    }

    auto draft = compile_from_doc(doc, xml_content, limits);
    result.ok = draft.ok;
    result.issues = std::move(draft.issues);
    if (draft.ok) {
        CompiledTemplateSet::Builder b;
        b.templates = std::move(draft.templates);
        b.id_to_index = std::move(draft.id_to_index);
        b.templates_sha256 = std::move(draft.templates_sha256);
        b.compiler_version = std::move(draft.compiler_version);
        b.schema_version = std::move(draft.schema_version);
        b.templates_file_size = draft.templates_file_size;
        result.compiled = CompiledTemplateSet::seal(std::move(b));
    }
    return result;
}

std::string compute_sha256_bytes(const std::uint8_t* data, std::size_t len) {
    SHA256Ctx ctx;
    sha256_init(ctx);
    sha256_update(ctx, data, len);
    std::uint8_t hash[32];
    sha256_final(ctx, hash);
    return hash_to_hex(hash);
}

std::string compute_sha256_string(const std::string& s) {
    return compute_sha256_bytes(reinterpret_cast<const std::uint8_t*>(s.data()), s.size());
}

}  // namespace moex_fast
