#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <variant>
#include <memory>

namespace moex_fast {

// --- Compile limits ---
struct CompileLimits {
    std::uint32_t max_templates = 4096;
    std::uint32_t max_fields_per_template = 65535;
    std::uint32_t max_nesting_depth = 32;
};

// --- Decode limits ---
struct DecodeLimits {
    std::size_t max_message_bytes = 1024 * 1024;       // 1 MiB
    std::size_t max_presence_map_bytes = 64;
    std::uint32_t max_sequence_entries = 100000;
    std::uint32_t max_total_nodes = 1000000;
    std::size_t max_string_bytes = 1024 * 1024;         // 1 MiB
};

// --- Wire types ---
enum class DecWireType : std::uint8_t {
    uInt32, uInt64, Int32, Int64,
    AsciiString, UnicodeString, ByteVector,
    Decimal, Sequence, Group
};

const char* dec_wire_type_name(DecWireType wt);

// --- Operator kinds ---
enum class OpKind : std::uint8_t {
    None,       // no operator — direct wire value
    Constant,   // static value, no wire bytes consumed
    Default,    // wire value or initial default
    Copy,       // wire value or previous dictionary value
    Increment,  // wire value or previous+1
    Delta,      // base + delta from wire
    Tail        // retained prefix + tail from wire
};

const char* op_kind_name(OpKind ok);

// --- Dictionary scope ---
enum class DictScope : std::uint8_t {
    Global,
    TemplateType,
    TypeRef,
    ExplicitKey
};

const char* dict_scope_name(DictScope ds);

// --- Compile issue ---
struct CompileIssue {
    std::string code;
    std::string message;
    std::string field_path;
};

// --- Operator instruction ---
struct OpInstruction {
    OpKind kind = OpKind::None;
    DictScope scope = DictScope::Global;
    std::string dict_key;           // canonical dictionary key
    std::string explicit_key;       // key attribute if present

    // Initial/default value (for default, copy, increment)
    bool has_initial = false;
    std::int64_t initial_int = 0;
    std::string initial_str;

    // For constant operator
    bool has_constant = false;
    std::int64_t constant_int = 0;
    std::string constant_str;

    // For decimal: nested exponent/mantissa instructions
    bool is_decimal_component = false;  // true if this is exponent or mantissa sub-instruction
};

// --- Compiled field ---
struct CompiledField {
    std::uint32_t index = 0;        // field index within template
    std::string name;
    std::int32_t fix_tag = 0;
    bool has_fix_tag = false;
    DecWireType wire_type = DecWireType::uInt32;
    bool is_mandatory = true;
    bool has_pmap_bit = false;      // whether this field consumes a presence-map bit

    OpInstruction op;               // primary operator

    // For decimal: separate exponent and mantissa instructions
    bool is_decimal = false;
    OpInstruction exponent_op;
    OpInstruction mantissa_op;

    // For sequence: length instruction
    bool is_sequence = false;
    OpInstruction length_op;
    std::uint32_t length_field_index = 0;  // index of length field in parent

    // For group/sequence: child fields
    bool has_children = false;
    std::vector<CompiledField> children;

    // Sequence entry has its own presence map
    bool entry_has_pmap = false;
};

// --- Compiled template ---
struct CompiledTemplate {
    std::uint32_t id = 0;
    std::string name;
    std::vector<CompiledField> fields;
    std::uint32_t total_pmap_bits = 0;  // total presence-map bits for this template
};

// --- Compiled template set ---
struct CompiledTemplateSet {
    std::vector<CompiledTemplate> templates;
    std::unordered_map<std::uint32_t, std::size_t> id_to_index;
    std::string templates_sha256;
    std::string compiler_version = "0.1.0";
    std::string schema_version = "1.0";

    const CompiledTemplate* find(std::uint32_t id) const;
};

// --- Compile result ---
struct CompileResult {
    bool ok = false;
    CompiledTemplateSet compiled;
    std::vector<CompileIssue> issues;
};

// --- Decode status ---
enum class DecodeStatus : std::uint8_t {
    Ok,
    NeedMoreData,
    InvalidEncoding,
    NonCanonicalEncoding,
    IntegerOverflow,
    InvalidPresenceMap,
    UnknownTemplate,
    MissingPreviousTemplate,
    MissingDictionaryValue,
    InvalidOperatorState,
    InvalidSequenceLength,
    LimitExceeded,
    TrailingBytes,
    UnsupportedTemplateFeature,
    InternalError
};

const char* decode_status_name(DecodeStatus s);

// --- Decode issue ---
struct DecodeIssue {
    std::string code;
    std::size_t offset = 0;
    std::uint32_t template_id = 0;
    bool has_template_id = false;
    std::string field_path;
    std::string message;
};

// --- Decoded value types ---
struct DecodedDecimal {
    std::int32_t exponent = 0;
    std::int64_t mantissa = 0;
    bool is_null = false;
};

using DecodedScalar = std::variant<
    std::monostate,         // null
    std::uint64_t,          // uInt32/uInt64
    std::int64_t,           // int32/int64
    std::string,            // ASCII, Unicode
    std::vector<std::uint8_t>, // byte vector
    DecodedDecimal          // decimal
>;

// --- Value source ---
enum class ValueSource : std::uint8_t {
    Wire,
    Constant,
    Default,
    Copy,
    Increment,
    Delta,
    Tail
};

const char* value_source_name(ValueSource vs);

// --- Decoded field ---
struct DecodedField {
    std::string name;
    std::int32_t fix_tag = 0;
    bool has_fix_tag = false;
    DecodedScalar value;
    ValueSource source = ValueSource::Wire;
    bool is_null = false;
    bool is_present = true;     // false for absent optional group
    std::string field_path;

    // For groups/sequences
    bool is_group = false;
    std::vector<DecodedField> children;

    bool is_sequence = false;
    std::vector<std::vector<DecodedField>> entries;  // each entry is a group of fields
    bool sequence_is_null = false;  // null optional sequence vs present empty
};

// --- Decoded message ---
struct DecodedMessage {
    std::uint32_t template_id = 0;
    std::string template_name;
    std::vector<DecodedField> fields;
    std::size_t bytes_consumed = 0;
};

// --- Decode result ---
struct DecodeResult {
    DecodeStatus status = DecodeStatus::Ok;
    std::size_t bytes_consumed = 0;
    DecodedMessage message;
    std::vector<DecodeIssue> issues;
};

// --- Session fingerprint for rollback testing ---
struct SessionFingerprint {
    bool has_template_id = false;
    std::uint32_t template_id = 0;
    std::size_t dict_entry_count = 0;
    std::uint64_t dict_hash = 0;  // deterministic hash of all dict entries
};

}  // namespace moex_fast
