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
        case DecWireType::ByteVector:   return "byteVector";
        case DecWireType::Decimal:      return "decimal";
        case DecWireType::Sequence:     return "sequence";
        case DecWireType::Group:        return "group";
    }
    return "unknown";
}

const char* op_kind_name(OpKind ok) {
    switch (ok) {
        case OpKind::None:      return "none";
        case OpKind::Constant:  return "constant";
        case OpKind::Default:   return "default";
        case OpKind::Copy:      return "copy";
        case OpKind::Increment: return "increment";
        case OpKind::Delta:     return "delta";
        case OpKind::Tail:      return "tail";
    }
    return "unknown";
}

const char* dict_scope_name(DictScope ds) {
    switch (ds) {
        case DictScope::Global:     return "global";
        case DictScope::TemplateType: return "templateType";
        case DictScope::TypeRef:    return "typeRef";
        case DictScope::ExplicitKey: return "explicitKey";
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
        case DecodeStatus::MissingDictionaryValue:  return "missing_dictionary_value";
        case DecodeStatus::InvalidOperatorState:    return "invalid_operator_state";
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
        case ValueSource::Wire:     return "wire";
        case ValueSource::Constant: return "constant";
        case ValueSource::Default:  return "default";
        case ValueSource::Copy:     return "copy";
        case ValueSource::Increment:return "increment";
        case ValueSource::Delta:    return "delta";
        case ValueSource::Tail:     return "tail";
    }
    return "unknown";
}

const CompiledTemplate* CompiledTemplateSet::find(std::uint32_t id) const {
    auto it = id_to_index.find(id);
    if (it == id_to_index.end()) return nullptr;
    return &templates[it->second];
}

}  // namespace moex_fast
