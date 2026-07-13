#include "moex_fast/fast_decoder.hpp"
#include "moex_fast/wire_cursor.hpp"
#include <cstring>

namespace moex_fast {

// --- PImpl ---
struct DecoderSession::Impl {
    CompiledTemplateSet templates;
    DecodeLimits limits;

    // Session state
    bool has_prev_template_id = false;
    std::uint32_t prev_template_id = 0;

    // Transaction snapshot (template-ID only; no field dictionary)
    bool has_prev_template_id_snapshot = false;
    std::uint32_t prev_template_id_snapshot = 0;

    // Decode context
    struct DecodeCtx {
        WireCursor cursor;
        const CompiledTemplate* tmpl = nullptr;
        std::vector<bool> pmap;
        std::size_t pmap_index = 0;
        std::uint32_t node_count = 0;
        std::string current_field_path;
        std::vector<DecodeIssue> issues;
        DecodeStatus worst_status = DecodeStatus::Ok;

        void add_issue(const std::string& code, const std::string& msg) {
            DecodeIssue issue;
            issue.code = code;
            issue.offset = cursor.position();
            issue.field_path = current_field_path;
            issue.message = msg;
            if (tmpl) {
                issue.template_id = tmpl->id;
                issue.has_template_id = true;
            }
            issues.push_back(std::move(issue));
        }

        void set_error(DecodeStatus status, const std::string& code, const std::string& msg) {
            if (worst_status == DecodeStatus::Ok || status == DecodeStatus::NeedMoreData) {
                worst_status = status;
            }
            add_issue(code, msg);
        }
    };

    // --- Transaction (template-ID snapshot only) ---

    void begin_transaction() {
        has_prev_template_id_snapshot = has_prev_template_id;
        prev_template_id_snapshot = prev_template_id;
    }

    void rollback() {
        has_prev_template_id = has_prev_template_id_snapshot;
        prev_template_id = prev_template_id_snapshot;
    }

    void commit() {
        // Template-ID commit is done by the caller after successful decode.
    }

    // --- Wire reading helpers ---

    bool consume_pmap_bit(DecodeCtx& ctx) {
        if (ctx.pmap_index >= ctx.pmap.size()) {
            return false;  // implicit zero beyond end
        }
        return ctx.pmap[ctx.pmap_index++];
    }

    bool check_node_limit(DecodeCtx& ctx) {
        if (ctx.node_count >= limits.max_total_nodes) {
            ctx.set_error(DecodeStatus::LimitExceeded, "node_limit_exceeded",
                          "Total decoded nodes exceeded limit");
            return false;
        }
        ctx.node_count++;
        return true;
    }

    // --- Wire reading ---

    DecodeStatus read_wire_value(DecodeCtx& ctx, DecWireType wire_type,
                                  bool is_mandatory, DecodedField& out) {
        switch (wire_type) {
            case DecWireType::uInt32: {
                if (is_mandatory) {
                    std::uint32_t val = 0;
                    DecodeStatus st = ctx.cursor.read_stopbit_u32(val);
                    if (st != DecodeStatus::Ok) return st;
                    out.value = static_cast<std::uint64_t>(val);
                } else {
                    std::uint32_t val = 0;
                    bool is_null = false;
                    DecodeStatus st = ctx.cursor.read_nullable_u32(val, is_null);
                    if (st != DecodeStatus::Ok) return st;
                    if (is_null) {
                        out.is_null = true;
                        out.is_present = false;
                        out.value = std::monostate{};
                    } else {
                        out.value = static_cast<std::uint64_t>(val);
                    }
                }
                break;
            }
            case DecWireType::uInt64: {
                if (is_mandatory) {
                    std::uint64_t val = 0;
                    DecodeStatus st = ctx.cursor.read_stopbit_u64(val);
                    if (st != DecodeStatus::Ok) return st;
                    out.value = val;
                } else {
                    std::uint64_t val = 0;
                    bool is_null = false;
                    DecodeStatus st = ctx.cursor.read_nullable_u64(val, is_null);
                    if (st != DecodeStatus::Ok) return st;
                    if (is_null) {
                        out.is_null = true;
                        out.is_present = false;
                        out.value = std::monostate{};
                    } else {
                        out.value = val;
                    }
                }
                break;
            }
            case DecWireType::Int32: {
                if (is_mandatory) {
                    std::int32_t val = 0;
                    DecodeStatus st = ctx.cursor.read_stopbit_i32(val);
                    if (st != DecodeStatus::Ok) return st;
                    out.value = static_cast<std::int64_t>(val);
                } else {
                    std::int32_t val = 0;
                    bool is_null = false;
                    DecodeStatus st = ctx.cursor.read_nullable_i32(val, is_null);
                    if (st != DecodeStatus::Ok) return st;
                    if (is_null) {
                        out.is_null = true;
                        out.is_present = false;
                        out.value = std::monostate{};
                    } else {
                        out.value = static_cast<std::int64_t>(val);
                    }
                }
                break;
            }
            case DecWireType::Int64: {
                if (is_mandatory) {
                    std::int64_t val = 0;
                    DecodeStatus st = ctx.cursor.read_stopbit_i64(val);
                    if (st != DecodeStatus::Ok) return st;
                    out.value = val;
                } else {
                    std::int64_t val = 0;
                    bool is_null = false;
                    DecodeStatus st = ctx.cursor.read_nullable_i64(val, is_null);
                    if (st != DecodeStatus::Ok) return st;
                    if (is_null) {
                        out.is_null = true;
                        out.is_present = false;
                        out.value = std::monostate{};
                    } else {
                        out.value = val;
                    }
                }
                break;
            }
            case DecWireType::AsciiString: {
                std::string val;
                if (!is_mandatory) {
                    bool str_null = false;
                    DecodeStatus st = ctx.cursor.read_nullable_ascii(val, str_null, limits.max_string_bytes);
                    if (st != DecodeStatus::Ok) return st;
                    if (str_null) {
                        out.is_null = true;
                        out.is_present = false;
                        out.value = std::monostate{};
                    } else {
                        out.value = val;
                    }
                } else {
                    DecodeStatus st = ctx.cursor.read_ascii_string(val, limits.max_string_bytes);
                    if (st != DecodeStatus::Ok) return st;
                    out.value = val;
                }
                break;
            }
            case DecWireType::UnicodeString: {
                std::string val;
                if (!is_mandatory) {
                    bool str_null = false;
                    DecodeStatus st = ctx.cursor.read_nullable_unicode(val, str_null, limits.max_string_bytes);
                    if (st != DecodeStatus::Ok) return st;
                    if (str_null) {
                        out.is_null = true;
                        out.is_present = false;
                        out.value = std::monostate{};
                    } else {
                        out.value = val;
                    }
                } else {
                    DecodeStatus st = ctx.cursor.read_unicode_string(val, limits.max_string_bytes);
                    if (st != DecodeStatus::Ok) return st;
                    out.value = val;
                }
                break;
            }
            case DecWireType::Decimal: {
                std::int32_t exp = 0;
                std::int64_t man = 0;
                bool is_null = false;
                DecodeStatus st = ctx.cursor.read_decimal(exp, man, is_null,
                                                          !is_mandatory, false);
                if (st != DecodeStatus::Ok) return st;
                if (is_null) {
                    out.is_null = true;
                    out.value = std::monostate{};
                } else {
                    DecodedDecimal dd;
                    dd.exponent = exp;
                    dd.mantissa = man;
                    dd.is_null = false;
                    out.value = dd;
                }
                break;
            }
            default:
                return DecodeStatus::UnsupportedTemplateFeature;
        }
        return DecodeStatus::Ok;
    }

    // --- Scalar decode dispatch ---

    DecodeStatus decode_scalar(DecodeCtx& ctx, const CompiledField& field,
                                DecodedField& out, const std::string& field_path) {
        out.name = field.name;
        out.has_fix_tag = field.has_fix_tag;
        out.fix_tag = field.fix_tag;
        out.field_path = field_path;
        out.is_null = false;
        out.is_present = true;

        // Consume pmap bit if this field has one
        bool pmap_present = true;
        if (field.has_pmap_bit) {
            pmap_present = consume_pmap_bit(ctx);
        }

        switch (field.op.kind) {
            case OpKind::None:
                return decode_none_op(ctx, field, out, pmap_present);
            case OpKind::Constant:
                return decode_constant_op(ctx, field, out, pmap_present);
            case OpKind::Default:
            case OpKind::Copy:
            case OpKind::Increment:
            case OpKind::Delta:
            case OpKind::Tail: {
                ctx.set_error(DecodeStatus::UnsupportedTemplateFeature,
                              "unsupported_operator_runtime",
                              "Excluded operator " + std::string(op_kind_name(field.op.kind)) +
                              " reached runtime on field " + field_path);
                return DecodeStatus::UnsupportedTemplateFeature;
            }
        }
        return DecodeStatus::InternalError;
    }

    // --- Operator: none ---
    DecodeStatus decode_none_op(DecodeCtx& ctx, const CompiledField& field,
                                 DecodedField& out, bool /*pmap_present*/) {
        out.source = ValueSource::Wire;
        return read_wire_value(ctx, field.wire_type, field.is_mandatory, out);
    }

    // --- Operator: constant ---
    DecodeStatus decode_constant_op(DecodeCtx& /*ctx*/, const CompiledField& field,
                                     DecodedField& out, bool pmap_present) {
        out.source = ValueSource::Constant;

        if (!field.is_mandatory && !pmap_present) {
            out.is_null = true;
            out.is_present = false;
            out.value = std::monostate{};
            return DecodeStatus::Ok;
        }

        if (field.op.has_constant) {
            if (field.wire_type == DecWireType::AsciiString ||
                field.wire_type == DecWireType::UnicodeString) {
                out.value = field.op.constant_str;
            } else if (field.wire_type == DecWireType::uInt32 ||
                       field.wire_type == DecWireType::uInt64) {
                out.value = field.op.constant_uint;
            } else {
                out.value = field.op.constant_int;
            }
        }
        return DecodeStatus::Ok;
    }

    // --- Decimal field decode ---
    // Only no-operator exponent and no-operator mantissa are supported.
    // If exponent null => whole decimal null, mantissa NOT consumed.
    DecodeStatus decode_decimal_field(DecodeCtx& ctx, const CompiledField& field,
                                       DecodedField& out, const std::string& field_path) {
        out.name = field.name;
        out.has_fix_tag = field.has_fix_tag;
        out.fix_tag = field.fix_tag;
        out.field_path = field_path;
        out.is_null = false;
        out.is_present = true;

        // Preflight: validate both decimal component operators before
        // consuming any decimal field presence-map bit or decimal wire bytes.
        if (field.exponent_op.kind != OpKind::None) {
            ctx.set_error(DecodeStatus::UnsupportedTemplateFeature,
                          "unsupported_operator_runtime",
                          "Excluded decimal exponent operator " +
                          std::string(op_kind_name(field.exponent_op.kind)) +
                          " reached runtime on field " + field_path);
            return DecodeStatus::UnsupportedTemplateFeature;
        }

        if (field.mantissa_op.kind != OpKind::None) {
            ctx.set_error(DecodeStatus::UnsupportedTemplateFeature,
                          "unsupported_operator_runtime",
                          "Excluded decimal mantissa operator " +
                          std::string(op_kind_name(field.mantissa_op.kind)) +
                          " reached runtime on field " + field_path);
            return DecodeStatus::UnsupportedTemplateFeature;
        }

        // Consume the field-level pmap bit
        bool pmap_present = true;
        if (field.has_pmap_bit) {
            pmap_present = consume_pmap_bit(ctx);
        }

        if (!field.is_mandatory && !pmap_present) {
            out.is_null = true;
            out.is_present = false;
            out.value = std::monostate{};
            return DecodeStatus::Ok;
        }

        std::int32_t exponent = 0;
        std::int64_t mantissa = 0;
        bool dec_null = false;

        if (field.is_mandatory) {
            DecodeStatus st = ctx.cursor.read_stopbit_i32(exponent);
            if (st != DecodeStatus::Ok) return st;
        } else {
            bool is_null = false;
            DecodeStatus st = ctx.cursor.read_nullable_i32(exponent, is_null);
            if (st != DecodeStatus::Ok) return st;
            if (is_null) {
                dec_null = true;
            }
        }

        if (dec_null) {
            out.is_null = true;
            out.is_present = false;
            out.value = std::monostate{};
            return DecodeStatus::Ok;
        }

        DecodeStatus st = ctx.cursor.read_stopbit_i64(mantissa);
        if (st != DecodeStatus::Ok) return st;

        DecodedDecimal dd;
        dd.exponent = exponent;
        dd.mantissa = mantissa;
        dd.is_null = false;
        out.value = dd;

        return DecodeStatus::Ok;
    }

    // --- Sequence decode ---
    DecodeStatus decode_sequence(DecodeCtx& ctx, const CompiledField& field,
                                  DecodedField& out, const std::string& field_path) {
        out.name = field.name;
        out.has_fix_tag = field.has_fix_tag;
        out.fix_tag = field.fix_tag;
        out.field_path = field_path;
        out.is_sequence = true;
        out.sequence_is_null = false;

        std::uint32_t length = 0;
        DecodeStatus st;
        if (field.is_mandatory) {
            st = ctx.cursor.read_sequence_length(length);
        } else {
            bool is_null = false;
            st = ctx.cursor.read_nullable_u32(length, is_null);
            if (st != DecodeStatus::Ok) return st;
            if (is_null) {
                out.sequence_is_null = true;
                out.is_present = false;
                return DecodeStatus::Ok;
            }
        }
        if (st != DecodeStatus::Ok) return st;

        if (length > limits.max_sequence_entries) {
            ctx.set_error(DecodeStatus::LimitExceeded, "sequence_limit",
                          "Sequence entry count exceeds limit in " + field_path);
            return DecodeStatus::LimitExceeded;
        }

        // Get entry fields (skip length field which is first child)
        std::vector<CompiledField> entry_fields;
        for (std::size_t i = 1; i < field.children.size(); ++i) {
            entry_fields.push_back(field.children[i]);
        }

        // Derive entry pmap from compiled entry instructions that actually allocate bits.
        std::uint32_t entry_pmap_bits = 0;
        for (const auto& ef : entry_fields) {
            if (ef.has_pmap_bit) entry_pmap_bits++;
        }

        out.entries.reserve(length);
        for (std::uint32_t i = 0; i < length; ++i) {
            if (!check_node_limit(ctx)) return DecodeStatus::LimitExceeded;

            // Save and restore message-level pmap for each entry
            std::vector<bool> saved_pmap = ctx.pmap;
            std::size_t saved_pmap_index = ctx.pmap_index;

            if (entry_pmap_bits > 0) {
                st = ctx.cursor.read_presence_map(entry_pmap_bits, ctx.pmap,
                                                    limits.max_presence_map_bytes);
                if (st != DecodeStatus::Ok) return st;
                ctx.pmap_index = 0;
            } else {
                ctx.pmap.clear();
                ctx.pmap_index = 0;
            }

            std::vector<DecodedField> entry;
            std::string entry_path = field_path + "[" + std::to_string(i) + "]";
            st = decode_fields(ctx, entry_fields, entry, entry_path);
            if (st != DecodeStatus::Ok) return st;

            out.entries.push_back(std::move(entry));

            // Restore message-level pmap
            ctx.pmap = saved_pmap;
            ctx.pmap_index = saved_pmap_index;
        }

        return DecodeStatus::Ok;
    }

    // --- Field decode dispatcher ---
    DecodeStatus decode_fields(DecodeCtx& ctx, const std::vector<CompiledField>& fields,
                                std::vector<DecodedField>& out, const std::string& parent_path) {
        for (const auto& field : fields) {
            std::string field_path = parent_path.empty() ? field.name : parent_path + "." + field.name;
            ctx.current_field_path = field_path;

            if (!check_node_limit(ctx)) return DecodeStatus::LimitExceeded;

            DecodedField decoded;

            if (field.is_decimal) {
                DecodeStatus st = decode_decimal_field(ctx, field, decoded, field_path);
                if (st != DecodeStatus::Ok) return st;
            } else if (field.is_sequence) {
                DecodeStatus st = decode_sequence(ctx, field, decoded, field_path);
                if (st != DecodeStatus::Ok) return st;
            } else {
                DecodeStatus st = decode_scalar(ctx, field, decoded, field_path);
                if (st != DecodeStatus::Ok) return st;
            }

            out.push_back(std::move(decoded));
        }
        return DecodeStatus::Ok;
    }

    // --- Hard ceiling enforcement ---
    static DecodeLimits enforce_hard_ceilings(DecodeLimits lim) {
        constexpr std::size_t HARD_MAX_MESSAGE = 1024 * 1024;        // 1 MiB
        constexpr std::size_t HARD_MAX_PMAP = 64;
        constexpr std::uint32_t HARD_MAX_SEQ = 100000;
        constexpr std::uint32_t HARD_MAX_NODES = 1000000;
        constexpr std::size_t HARD_MAX_STRING = 1024 * 1024;         // 1 MiB

        if (lim.max_message_bytes > HARD_MAX_MESSAGE) lim.max_message_bytes = HARD_MAX_MESSAGE;
        if (lim.max_presence_map_bytes > HARD_MAX_PMAP) lim.max_presence_map_bytes = HARD_MAX_PMAP;
        if (lim.max_sequence_entries > HARD_MAX_SEQ) lim.max_sequence_entries = HARD_MAX_SEQ;
        if (lim.max_total_nodes > HARD_MAX_NODES) lim.max_total_nodes = HARD_MAX_NODES;
        if (lim.max_string_bytes > HARD_MAX_STRING) lim.max_string_bytes = HARD_MAX_STRING;
        return lim;
    }

    // --- Main decode entry point ---
    DecodeResult do_decode(const std::uint8_t* data, std::size_t size, bool exact) {
        DecodeResult result;

        // Invalid handle: fail before reading input or changing state
        if (!templates.valid()) {
            result.status = DecodeStatus::InternalError;
            result.bytes_consumed = 0;
            DecodeIssue issue;
            issue.code = "invalid_compiled_template_set";
            issue.offset = 0;
            issue.message = "CompiledTemplateSet is invalid or empty";
            result.issues.push_back(std::move(issue));
            return result;
        }

        DecodeCtx ctx;
        ctx.cursor = WireCursor(data, size);
        ctx.node_count = 0;

        // Enforce max_message_bytes before any decoding
        if (size > limits.max_message_bytes) {
            result.status = DecodeStatus::LimitExceeded;
            DecodeIssue issue;
            issue.code = "message_size_limit";
            issue.offset = 0;
            issue.message = "Message size " + std::to_string(size) +
                            " exceeds limit " + std::to_string(limits.max_message_bytes);
            result.issues.push_back(std::move(issue));
            return result;
        }

        // Begin transaction for rollback on failure
        begin_transaction();

        // Read message-level presence map.
        std::size_t max_pmap_bits = limits.max_presence_map_bytes * 7;
        DecodeStatus st = ctx.cursor.read_presence_map(max_pmap_bits, ctx.pmap,
                                                        limits.max_presence_map_bytes);
        if (st != DecodeStatus::Ok) {
            rollback();
            result.status = st;
            result.issues = std::move(ctx.issues);
            return result;
        }
        ctx.pmap_index = 0;

        // Bit 0 of pmap: template-ID present flag
        bool template_id_present = ctx.pmap[ctx.pmap_index++];

        std::uint32_t template_id = 0;
        if (template_id_present) {
            st = ctx.cursor.read_stopbit_u32(template_id);
            if (st != DecodeStatus::Ok) {
                rollback();
                result.status = st;
                result.issues = std::move(ctx.issues);
                return result;
            }

            if (!templates.find(template_id)) {
                rollback();
                result.status = DecodeStatus::UnknownTemplate;
                ctx.set_error(DecodeStatus::UnknownTemplate, "unknown_template",
                              "Unknown template ID: " + std::to_string(template_id));
                result.issues = std::move(ctx.issues);
                return result;
            }
        } else {
            if (!has_prev_template_id) {
                rollback();
                result.status = DecodeStatus::MissingPreviousTemplate;
                ctx.set_error(DecodeStatus::MissingPreviousTemplate, "missing_previous_template",
                              "No previous template ID and template-ID bit not set");
                result.issues = std::move(ctx.issues);
                return result;
            }
            template_id = prev_template_id;
        }

        ctx.tmpl = templates.find(template_id);
        if (!ctx.tmpl) {
            rollback();
            result.status = DecodeStatus::UnknownTemplate;
            result.issues = std::move(ctx.issues);
            return result;
        }

        // Decode message fields using the remaining pmap bits
        DecodedMessage msg;
        msg.template_id = template_id;
        msg.template_name = ctx.tmpl->name;

        st = decode_fields(ctx, ctx.tmpl->fields, msg.fields, ctx.tmpl->name);
        if (st != DecodeStatus::Ok) {
            rollback();
            result.status = st;
            result.issues = std::move(ctx.issues);
            return result;
        }

        // Check for trailing bytes under decode_exact
        if (exact && !ctx.cursor.at_end()) {
            rollback();
            result.status = DecodeStatus::TrailingBytes;
            ctx.set_error(DecodeStatus::TrailingBytes, "trailing_bytes",
                          "Trailing bytes after message");
            result.issues = std::move(ctx.issues);
            return result;
        }

        // Success: commit transaction, update session state
        commit();
        has_prev_template_id = true;
        prev_template_id = template_id;

        msg.bytes_consumed = ctx.cursor.position();
        result.status = DecodeStatus::Ok;
        result.bytes_consumed = ctx.cursor.position();
        result.message = std::move(msg);
        result.issues = std::move(ctx.issues);
        return result;
    }

    // --- Deterministic fingerprint (no field dictionary) ---
    SessionFingerprint make_fingerprint() const {
        SessionFingerprint fp;
        fp.has_template_id = has_prev_template_id;
        fp.template_id = prev_template_id;
        fp.dict_entry_count = 0;
        fp.dict_hash = "cbf29ce484222325";
        return fp;
    }
};

// --- Public API ---

DecoderSession::DecoderSession(const CompiledTemplateSet& templates, const DecodeLimits& limits)
    : impl_(std::make_unique<Impl>()) {
    impl_->templates = templates;
    impl_->limits = Impl::enforce_hard_ceilings(limits);
}

DecoderSession::~DecoderSession() = default;
DecoderSession::DecoderSession(DecoderSession&&) noexcept = default;
DecoderSession& DecoderSession::operator=(DecoderSession&&) noexcept = default;

DecodeResult DecoderSession::decode_one(const std::uint8_t* data, std::size_t size) {
    return impl_->do_decode(data, size, false);
}

DecodeResult DecoderSession::decode_exact(const std::uint8_t* data, std::size_t size) {
    return impl_->do_decode(data, size, true);
}

void DecoderSession::reset() {
    impl_->has_prev_template_id = false;
    impl_->prev_template_id = 0;
    impl_->has_prev_template_id_snapshot = false;
    impl_->prev_template_id_snapshot = 0;
}

SessionFingerprint DecoderSession::fingerprint() const {
    return impl_->make_fingerprint();
}

const CompiledTemplateSet& DecoderSession::templates() const {
    return impl_->templates;
}

}  // namespace moex_fast
