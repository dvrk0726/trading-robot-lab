#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <variant>
#include <memory>
#include <cstddef>

namespace moex_fast {

// --- Compile limits ---
struct CompileLimits {
    std::uint32_t max_templates = 4096;
    std::uint32_t max_fields_per_template = 65535;
    std::uint32_t max_nesting_depth = 32;
};

// --- Decode limits ---
struct DecodeLimits {
    std::size_t max_message_bytes = 1024 * 1024;        // 1 MiB
    std::size_t max_presence_map_bytes = 64;
    std::uint32_t max_sequence_entries = 100000;
    std::uint32_t max_total_nodes = 1000000;
    std::size_t max_string_bytes = 1024 * 1024;          // 1 MiB
};

// --- Wire types ---
enum class DecWireType : std::uint8_t {
    uInt32, uInt64, Int32, Int64,
    AsciiString, UnicodeString,
    Decimal, Sequence
};

const char* dec_wire_type_name(DecWireType wt);

// --- Operator kinds ---
enum class OpKind : std::uint8_t {
    None,
    Constant,
    Default,
    Copy,
    Increment,
    Delta,
    Tail
};

const char* op_kind_name(OpKind ok);

// --- Compile issue ---
struct CompileIssue {
    std::string code;
    std::string message;
    std::string field_path;
};

// --- Operator instruction ---
struct OpInstruction {
    OpKind kind = OpKind::None;

    bool has_constant = false;
    std::int64_t constant_int = 0;
    std::uint64_t constant_uint = 0;
    std::string constant_str;
};

// --- Compiled field ---
struct CompiledField {
    std::uint32_t index = 0;
    std::string name;
    std::int32_t fix_tag = 0;
    bool has_fix_tag = false;
    DecWireType wire_type = DecWireType::uInt32;
    bool is_mandatory = true;
    bool has_pmap_bit = false;

    OpInstruction op;

    bool is_decimal = false;

    bool is_sequence = false;
    OpInstruction length_op;
    std::uint32_t length_field_index = 0;

    std::vector<CompiledField> children;

    bool entry_has_pmap = false;
};

// --- Compiled template ---
struct CompiledTemplate {
    std::uint32_t id = 0;
    std::string name;
    std::vector<CompiledField> fields;
    std::uint32_t total_pmap_bits = 0;
};

// Forward declarations for compiler (friend of CompiledTemplateSet)
struct CompileLimits;
struct CompileResult;
CompileResult compile_templates_from_string(const std::string& xml_content, const CompileLimits& limits);
CompileResult compile_templates(const std::string& xml_path, const CompileLimits& limits);

// --- Compiled template set (immutable shared-handle) ---
class CompiledTemplateSet {
public:
    // Default construction produces an invalid/empty handle.
    CompiledTemplateSet();

    // Copyable and movable; copies share immutable storage.
    CompiledTemplateSet(const CompiledTemplateSet&) = default;
    CompiledTemplateSet& operator=(const CompiledTemplateSet&) = default;
    CompiledTemplateSet(CompiledTemplateSet&&) noexcept = default;
    CompiledTemplateSet& operator=(CompiledTemplateSet&&) noexcept = default;

    // Const observers
    bool valid() const;
    bool empty() const;
    std::size_t size() const;

    // Ordered const template collection
    const std::vector<CompiledTemplate>& templates() const;

    // Lookup by id (returns nullptr if not found or handle invalid)
    const CompiledTemplate* find(std::uint32_t id) const;

    // Provenance (empty/zero on invalid handle)
    const std::string& templates_sha256() const;
    const std::string& compiler_version() const;
    const std::string& schema_version() const;
    std::size_t templates_file_size() const;

private:
    struct Storage {
        std::vector<CompiledTemplate> templates;
        std::map<std::uint32_t, std::size_t> id_to_index;
        std::string templates_sha256;
        std::string compiler_version = "0.2.0";
        std::string schema_version = "1.0";
        std::size_t templates_file_size = 0;
    };

    // Builder used internally by the compiler; only friend can construct.
    struct Builder {
        std::vector<CompiledTemplate> templates;
        std::map<std::uint32_t, std::size_t> id_to_index;
        std::string templates_sha256;
        std::string compiler_version = "0.2.0";
        std::string schema_version = "1.0";
        std::size_t templates_file_size = 0;
    };

    std::shared_ptr<const Storage> storage_;

    explicit CompiledTemplateSet(std::shared_ptr<const Storage> s);

    // Compiler creates handles through this.
    static CompiledTemplateSet seal(Builder&& b);

    friend struct CompileResult;
    friend CompileResult compile_templates_from_string(const std::string&, const CompileLimits&);
    friend CompileResult compile_templates(const std::string&, const CompileLimits&);
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
    std::monostate,
    std::uint64_t,
    std::int64_t,
    std::string,
    DecodedDecimal
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
    bool is_present = true;
    std::string field_path;

    bool is_sequence = false;
    std::vector<std::vector<DecodedField>> entries;
    bool sequence_is_null = false;
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

// --- Session fingerprint for rollback testing (previous-template-ID state) ---
struct SessionFingerprint {
    bool has_template_id = false;
    std::uint32_t template_id = 0;
};

}  // namespace moex_fast
