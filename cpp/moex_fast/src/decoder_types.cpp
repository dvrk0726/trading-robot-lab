#include "moex_fast/decoder_types.hpp"

namespace moex_fast {

const char* dec_wire_type_name(DecWireType wt) {
    switch (wt) {
        case DecWireType::uInt32:       return "uInt32";
        case DecWireType::uInt64:       return "uInt64";
        case DecWireType::Int32:        return "int32";
        case DecWireType::Int64:        return "int64";
        case DecWireType::AsciiString:  return "ascii";
        case DecWireType::UnicodeString:return "unicode";
        case DecWireType::Decimal:      return "decimal";
        case DecWireType::Sequence:     return "sequence";
    }
    return "unknown";
}

const char* op_kind_name(OpKind ok) {
    switch (ok) {
        case OpKind::None:      return "none";
        case OpKind::Constant:  return "constant";
    }
    return "unknown";
}

const char* decode_status_name(DecodeStatus s) {
    switch (s) {
        case DecodeStatus::Ok:                      return "ok";
        case DecodeStatus::NeedMoreData:            return "need_more_data";
        case DecodeStatus::InvalidEncoding:         return "invalid_encoding";
        case DecodeStatus::NonCanonicalEncoding:    return "non_canonical_encoding";
        case DecodeStatus::IntegerOverflow:         return "integer_overflow";
        case DecodeStatus::InvalidPresenceMap:      return "invalid_presence_map";
        case DecodeStatus::UnknownTemplate:         return "unknown_template";
        case DecodeStatus::MissingPreviousTemplate: return "missing_previous_template";
        case DecodeStatus::InvalidSequenceLength:   return "invalid_sequence_length";
        case DecodeStatus::LimitExceeded:           return "limit_exceeded";
        case DecodeStatus::TrailingBytes:           return "trailing_bytes";
        case DecodeStatus::UnsupportedTemplateFeature: return "unsupported_template_feature";
        case DecodeStatus::InternalError:           return "internal_error";
    }
    return "unknown";
}

const char* value_source_name(ValueSource vs) {
    switch (vs) {
        case ValueSource::Wire:      return "wire";
        case ValueSource::Constant:  return "constant";
    }
    return "unknown";
}

// --- CompiledTemplateSet implementation ---

// Static empty data returned by const-ref accessors on invalid handles.
static const std::vector<CompiledTemplate>& empty_templates() {
    static const std::vector<CompiledTemplate> v{};
    return v;
}
static const std::string& empty_string() {
    static const std::string s{};
    return s;
}

CompiledTemplateSet::CompiledTemplateSet() : storage_(nullptr) {}

CompiledTemplateSet::CompiledTemplateSet(std::shared_ptr<const Storage> s)
    : storage_(std::move(s)) {}

CompiledTemplateSet CompiledTemplateSet::seal(Builder&& b) {
    auto s = std::make_shared<Storage>();
    s->templates = std::move(b.templates);
    s->id_to_index = std::move(b.id_to_index);
    s->templates_sha256 = std::move(b.templates_sha256);
    s->compiler_version = std::move(b.compiler_version);
    s->schema_version = std::move(b.schema_version);
    s->templates_file_size = b.templates_file_size;
    return CompiledTemplateSet(std::shared_ptr<const Storage>(std::move(s)));
}

bool CompiledTemplateSet::valid() const {
    return storage_ != nullptr;
}

bool CompiledTemplateSet::empty() const {
    return !storage_ || storage_->templates.empty();
}

std::size_t CompiledTemplateSet::size() const {
    return storage_ ? storage_->templates.size() : 0;
}

const std::vector<CompiledTemplate>& CompiledTemplateSet::templates() const {
    return storage_ ? storage_->templates : empty_templates();
}

const CompiledTemplate* CompiledTemplateSet::find(std::uint32_t id) const {
    if (!storage_) return nullptr;
    auto it = storage_->id_to_index.find(id);
    if (it == storage_->id_to_index.end()) return nullptr;
    return &storage_->templates[it->second];
}

const std::string& CompiledTemplateSet::templates_sha256() const {
    return storage_ ? storage_->templates_sha256 : empty_string();
}

const std::string& CompiledTemplateSet::compiler_version() const {
    return storage_ ? storage_->compiler_version : empty_string();
}

const std::string& CompiledTemplateSet::schema_version() const {
    return storage_ ? storage_->schema_version : empty_string();
}

std::size_t CompiledTemplateSet::templates_file_size() const {
    return storage_ ? storage_->templates_file_size : 0;
}

}  // namespace moex_fast
