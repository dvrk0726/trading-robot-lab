#include "moex_fast/fast_decoder.hpp"
#include "moex_fast/wire_cursor.hpp"
#include <unordered_map>
#include <functional>
#include <cstring>

namespace moex_fast {

namespace {

// Dictionary entry value
struct DictValue {
    bool defined = false;
    bool is_null = false;
    std::uint64_t uint_val = 0;
    std::int64_t int_val = 0;
    std::string str_val;
    std::vector<std::uint8_t> bytes_val;
    DecWireType wire_type = DecWireType::uInt32;
};

// Journal entry for transactional rollback
struct JournalEntry {
    std::string key;
    DictValue previous;
    bool was_defined = false;
};

}  // namespace

// --- PImpl ---
struct DecoderSession::Impl {
    const CompiledTemplateSet* templates;
    DecodeLimits limits;

    // Session state
    bool has_prev_template_id = false;
    std::uint32_t prev_template_id = 0;

    // Dictionary: canonical key -> value
    std::unordered_map<std::string, DictValue> dictionary;

    // Transaction journal
    std::vector<JournalEntry> journal;
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

    void begin_transaction() {
        journal.clear();
        has_prev_template_id_snapshot = has_prev_template_id;
        prev_template_id_snapshot = prev_template_id;
    }

    void rollback() {
        // Restore dictionary entries
        for (auto it = journal.rbegin(); it != journal.rend(); ++it) {
            if (it->was_defined) {
                dictionary[it->key] = it->previous;
            } else {
                dictionary.erase(it->key);
            }
        }
        journal.clear();

        // Restore template-id state
        has_prev_template_id = has_prev_template_id_snapshot;
        prev_template_id = prev_template_id_snapshot;
    }

    void commit() {
        journal.clear();
        // Template-id and dictionary changes are already applied
    }

    // Store an integer value in the correct variant type based on wire type
    static void store_int_value(DecodedField& out, DecWireType wire_type, std::int64_t val) {
        if (wire_type == DecWireType::uInt32 || wire_type == DecWireType::uInt64) {
            out.value = static_cast<std::uint64_t>(val);
        } else {
            out.value = val;
        }
    }

    // Record a dictionary update in the journal
    void journal_update(const std::string& key) {
        JournalEntry entry;
        entry.key = key;
        auto it = dictionary.find(key);
        if (it != dictionary.end()) {
            entry.was_defined = true;
            entry.previous = it->second;
        } else {
            entry.was_defined = false;
        }
        journal.push_back(std::move(entry));
    }

    // Check if a pmap bit is set (consuming it)
    bool consume_pmap_bit(DecodeCtx& ctx) {
        if (ctx.pmap_index >= ctx.pmap.size()) {
            return false;  // implicit zero
        }
        return ctx.pmap[ctx.pmap_index++];
    }

    // Check node limit
    bool check_node_limit(DecodeCtx& ctx) {
        if (ctx.node_count >= limits.max_total_nodes) {
            ctx.set_error(DecodeStatus::LimitExceeded, "node_limit_exceeded",
                          "Total decoded nodes exceeded limit");
            return false;
        }
        ctx.node_count++;
        return true;
    }

    // Get or create dictionary entry
    DictValue* get_dict(const std::string& key) {
        auto it = dictionary.find(key);
        if (it == dictionary.end()) return nullptr;
        return &it->second;
    }

    // Set dictionary value (journaling)
    void set_dict(const std::string& key, const DictValue& val) {
        journal_update(key);
        dictionary[key] = val;
    }

    // --- Decode a scalar field ---
    DecodeStatus decode_scalar(DecodeCtx& ctx, const CompiledField& field,
                                DecodedField& out, const std::string& field_path) {
        out.name = field.name;
        out.has_fix_tag = field.has_fix_tag;
        out.fix_tag = field.fix_tag;
        out.field_path = field_path;
        out.is_null = false;
        out.is_present = true;

        // Check presence map for optional fields or fields with operators
        bool pmap_present = true;
        if (field.has_pmap_bit) {
            pmap_present = consume_pmap_bit(ctx);
        }

        // Handle operators
        switch (field.op.kind) {
            case OpKind::None:
                return decode_none_op(ctx, field, out, pmap_present);
            case OpKind::Constant:
                return decode_constant_op(ctx, field, out, pmap_present);
            case OpKind::Default:
                return decode_default_op(ctx, field, out, pmap_present);
            case OpKind::Copy:
                return decode_copy_op(ctx, field, out, pmap_present);
            case OpKind::Increment:
                return decode_increment_op(ctx, field, out, pmap_present);
            case OpKind::Delta:
                return decode_delta_op(ctx, field, out, pmap_present);
            case OpKind::Tail:
                return decode_tail_op(ctx, field, out, pmap_present);
        }
        return DecodeStatus::InternalError;
    }

    // --- Operator: none ---
    DecodeStatus decode_none_op(DecodeCtx& ctx, const CompiledField& field,
                                 DecodedField& out, bool pmap_present) {
        if (!field.is_mandatory && !pmap_present) {
            // Optional field absent => null
            out.is_null = true;
            out.is_present = false;
            out.value = std::monostate{};
            out.source = ValueSource::Wire;
            return DecodeStatus::Ok;
        }

        out.source = ValueSource::Wire;
        return read_wire_value(ctx, field.wire_type, field.is_mandatory, out);
    }

    // --- Operator: constant ---
    DecodeStatus decode_constant_op(DecodeCtx& /*ctx*/, const CompiledField& field,
                                     DecodedField& out, bool /*pmap_present*/) {
        // Constant: value is always the constant, no wire bytes consumed
        out.source = ValueSource::Constant;
        if (field.op.has_constant) {
            if (field.wire_type == DecWireType::AsciiString ||
                field.wire_type == DecWireType::UnicodeString) {
                out.value = field.op.constant_str;
            } else {
                out.value = static_cast<std::int64_t>(field.op.constant_int);
            }
        }
        return DecodeStatus::Ok;
    }

    // --- Operator: default ---
    DecodeStatus decode_default_op(DecodeCtx& ctx, const CompiledField& field,
                                    DecodedField& out, bool pmap_present) {
        if (pmap_present) {
            // Wire value present
            out.source = ValueSource::Wire;
            DecodeStatus st = read_wire_value(ctx, field.wire_type, field.is_mandatory, out);
            if (st != DecodeStatus::Ok) return st;
            // Update dictionary
            update_dict_from_decoded(field.op.dict_key, field.wire_type, out);
        } else {
            // Absent: use initial default value
            out.source = ValueSource::Default;
            if (field.op.has_initial) {
                if (field.wire_type == DecWireType::AsciiString ||
                    field.wire_type == DecWireType::UnicodeString) {
                    out.value = field.op.initial_str;
                } else {
                    store_int_value(out, field.wire_type, field.op.initial_int);
                }
            } else {
                // No initial value defined: this is an error for mandatory fields
                if (field.is_mandatory) {
                    ctx.set_error(DecodeStatus::MissingDictionaryValue, "missing_default",
                                  "No default value for mandatory field " + field.name);
                    return DecodeStatus::MissingDictionaryValue;
                }
                out.is_null = true;
                out.value = std::monostate{};
            }
        }
        return DecodeStatus::Ok;
    }

    // --- Operator: copy ---
    DecodeStatus decode_copy_op(DecodeCtx& ctx, const CompiledField& field,
                                 DecodedField& out, bool pmap_present) {
        if (pmap_present) {
            // Wire value present - read and update dictionary
            out.source = ValueSource::Wire;
            DecodeStatus st = read_wire_value(ctx, field.wire_type, field.is_mandatory, out);
            if (st != DecodeStatus::Ok) return st;
            update_dict_from_decoded(field.op.dict_key, field.wire_type, out);
        } else {
            // Absent: copy from dictionary
            out.source = ValueSource::Copy;
            DictValue* dict = get_dict(field.op.dict_key);
            if (dict && dict->defined) {
                if (dict->is_null) {
                    out.is_null = true;
                    out.value = std::monostate{};
                } else {
                    copy_dict_to_decoded(*dict, field.wire_type, out);
                }
            } else if (field.op.has_initial) {
                // Use initial value
                if (field.wire_type == DecWireType::AsciiString ||
                    field.wire_type == DecWireType::UnicodeString) {
                    out.value = field.op.initial_str;
                } else {
                    store_int_value(out, field.wire_type, field.op.initial_int);
                }
                // Also set in dictionary
                DictValue dv;
                dv.defined = true;
                dv.is_null = false;
                if (field.wire_type == DecWireType::AsciiString ||
                    field.wire_type == DecWireType::UnicodeString) {
                    dv.str_val = field.op.initial_str;
                } else {
                    dv.int_val = field.op.initial_int;
                    dv.uint_val = static_cast<std::uint64_t>(field.op.initial_int);
                }
                dv.wire_type = field.wire_type;
                set_dict(field.op.dict_key, dv);
            } else {
                // Undefined previous value
                if (field.is_mandatory) {
                    ctx.set_error(DecodeStatus::MissingDictionaryValue, "undefined_copy",
                                  "No previous value for copy on " + field.name);
                    return DecodeStatus::MissingDictionaryValue;
                }
                out.is_null = true;
                out.value = std::monostate{};
            }
        }
        return DecodeStatus::Ok;
    }

    // --- Operator: increment ---
    DecodeStatus decode_increment_op(DecodeCtx& ctx, const CompiledField& field,
                                      DecodedField& out, bool pmap_present) {
        if (pmap_present) {
            // Wire value present - read and update dictionary
            out.source = ValueSource::Wire;
            DecodeStatus st = read_wire_value(ctx, field.wire_type, field.is_mandatory, out);
            if (st != DecodeStatus::Ok) return st;
            update_dict_from_decoded(field.op.dict_key, field.wire_type, out);
        } else {
            // Absent: increment from dictionary
            out.source = ValueSource::Increment;
            DictValue* dict = get_dict(field.op.dict_key);
            if (dict && dict->defined) {
                // Increment previous value
                std::int64_t prev = dict->int_val;
                // Check overflow
                if (field.wire_type == DecWireType::uInt32 && prev >= static_cast<std::int64_t>(UINT32_MAX)) {
                    ctx.set_error(DecodeStatus::IntegerOverflow, "increment_overflow",
                                  "Increment overflow on " + field.name);
                    return DecodeStatus::IntegerOverflow;
                }
                if (field.wire_type == DecWireType::Int32 && prev >= INT32_MAX) {
                    ctx.set_error(DecodeStatus::IntegerOverflow, "increment_overflow",
                                  "Increment overflow on " + field.name);
                    return DecodeStatus::IntegerOverflow;
                }
                if (field.wire_type == DecWireType::uInt64 &&
                    static_cast<std::uint64_t>(prev) >= UINT64_MAX) {
                    ctx.set_error(DecodeStatus::IntegerOverflow, "increment_overflow",
                                  "Increment overflow on " + field.name);
                    return DecodeStatus::IntegerOverflow;
                }
                if (field.wire_type == DecWireType::Int64 && prev >= INT64_MAX) {
                    ctx.set_error(DecodeStatus::IntegerOverflow, "increment_overflow",
                                  "Increment overflow on " + field.name);
                    return DecodeStatus::IntegerOverflow;
                }

                std::int64_t new_val = prev + 1;
                store_int_value(out, field.wire_type, new_val);

                // Update dictionary
                DictValue dv;
                dv.defined = true;
                dv.int_val = new_val;
                dv.uint_val = static_cast<std::uint64_t>(new_val);
                dv.wire_type = field.wire_type;
                set_dict(field.op.dict_key, dv);
            } else if (field.op.has_initial) {
                // Use initial value
                store_int_value(out, field.wire_type, field.op.initial_int);
                DictValue dv;
                dv.defined = true;
                dv.int_val = field.op.initial_int;
                dv.uint_val = static_cast<std::uint64_t>(field.op.initial_int);
                dv.wire_type = field.wire_type;
                set_dict(field.op.dict_key, dv);
            } else {
                // Undefined previous
                ctx.set_error(DecodeStatus::MissingDictionaryValue, "undefined_increment",
                              "No previous value for increment on " + field.name);
                return DecodeStatus::MissingDictionaryValue;
            }
        }
        return DecodeStatus::Ok;
    }

    // --- Operator: delta ---
    DecodeStatus decode_delta_op(DecodeCtx& ctx, const CompiledField& field,
                                  DecodedField& out, bool /*pmap_present*/) {
        // Delta always reads from wire
        out.source = ValueSource::Delta;

        // Read the delta value from wire
        DictValue* dict = get_dict(field.op.dict_key);
        bool has_base = dict && dict->defined;

        if (field.wire_type == DecWireType::uInt32 || field.wire_type == DecWireType::uInt64 ||
            field.wire_type == DecWireType::Int32 || field.wire_type == DecWireType::Int64) {
            // Integer delta: read signed delta from wire, add to base
            std::int64_t delta_val = 0;
            DecodeStatus st = ctx.cursor.read_stopbit_i64(delta_val);
            if (st != DecodeStatus::Ok) return st;

            if (has_base) {
                std::int64_t result = dict->int_val + delta_val;
                store_int_value(out, field.wire_type, result);
                // Update dictionary
                DictValue dv;
                dv.defined = true;
                dv.int_val = result;
                dv.uint_val = static_cast<std::uint64_t>(result);
                dv.wire_type = field.wire_type;
                set_dict(field.op.dict_key, dv);
            } else {
                // No base: the delta IS the value
                store_int_value(out, field.wire_type, delta_val);
                DictValue dv;
                dv.defined = true;
                dv.int_val = delta_val;
                dv.uint_val = static_cast<std::uint64_t>(delta_val);
                dv.wire_type = field.wire_type;
                set_dict(field.op.dict_key, dv);
            }
        } else if (field.wire_type == DecWireType::AsciiString ||
                   field.wire_type == DecWireType::UnicodeString) {
            // String delta: read length prefix (i32), then bytes
            std::int32_t prefix_len = 0;
            DecodeStatus st = ctx.cursor.read_stopbit_i32(prefix_len);
            if (st != DecodeStatus::Ok) return st;

            if (prefix_len < 0) {
                // Suffix operation
                std::int32_t suffix_len = -prefix_len;
                std::uint32_t tail_len = 0;
                st = ctx.cursor.read_stopbit_u32(tail_len);
                if (st != DecodeStatus::Ok) return st;

                std::string base = has_base ? dict->str_val : std::string();
                if (static_cast<std::size_t>(suffix_len) > base.size()) {
                    ctx.set_error(DecodeStatus::InvalidEncoding, "delta_suffix_too_long",
                                  "Delta suffix length exceeds base for " + field.name);
                    return DecodeStatus::InvalidEncoding;
                }
                std::string result = base.substr(0, base.size() - suffix_len);
                const std::uint8_t* tail_ptr = nullptr;
                if (tail_len > 0) {
                    st = ctx.cursor.read_bytes(tail_len, tail_ptr);
                    if (st != DecodeStatus::Ok) return st;
                    result.append(reinterpret_cast<const char*>(tail_ptr), tail_len);
                }
                out.value = result;
                DictValue dv;
                dv.defined = true;
                dv.str_val = result;
                dv.wire_type = field.wire_type;
                set_dict(field.op.dict_key, dv);
            } else {
                // Prefix operation
                std::uint32_t tail_len = 0;
                st = ctx.cursor.read_stopbit_u32(tail_len);
                if (st != DecodeStatus::Ok) return st;

                std::string base = has_base ? dict->str_val : std::string();
                if (static_cast<std::size_t>(prefix_len) > base.size()) {
                    ctx.set_error(DecodeStatus::InvalidEncoding, "delta_prefix_too_long",
                                  "Delta prefix length exceeds base for " + field.name);
                    return DecodeStatus::InvalidEncoding;
                }
                std::string result = base.substr(0, prefix_len);
                const std::uint8_t* tail_ptr = nullptr;
                if (tail_len > 0) {
                    st = ctx.cursor.read_bytes(tail_len, tail_ptr);
                    if (st != DecodeStatus::Ok) return st;
                    result.append(reinterpret_cast<const char*>(tail_ptr), tail_len);
                }
                out.value = result;
                DictValue dv;
                dv.defined = true;
                dv.str_val = result;
                dv.wire_type = field.wire_type;
                set_dict(field.op.dict_key, dv);
            }
        } else if (field.wire_type == DecWireType::ByteVector) {
            // Byte vector delta: similar to string
            std::int32_t prefix_len = 0;
            DecodeStatus st = ctx.cursor.read_stopbit_i32(prefix_len);
            if (st != DecodeStatus::Ok) return st;

            std::uint32_t tail_len = 0;
            st = ctx.cursor.read_stopbit_u32(tail_len);
            if (st != DecodeStatus::Ok) return st;

            std::vector<std::uint8_t> base = has_base ? dict->bytes_val : std::vector<std::uint8_t>();
            if (prefix_len < 0) {
                std::int32_t suffix_len = -prefix_len;
                if (static_cast<std::size_t>(suffix_len) > base.size()) {
                    ctx.set_error(DecodeStatus::InvalidEncoding, "delta_suffix_too_long",
                                  "Delta suffix length exceeds base for " + field.name);
                    return DecodeStatus::InvalidEncoding;
                }
                std::vector<std::uint8_t> result(base.begin(), base.begin() + (base.size() - suffix_len));
                const std::uint8_t* tail_ptr = nullptr;
                if (tail_len > 0) {
                    st = ctx.cursor.read_bytes(tail_len, tail_ptr);
                    if (st != DecodeStatus::Ok) return st;
                    result.insert(result.end(), tail_ptr, tail_ptr + tail_len);
                }
                out.value = result;
                DictValue dv;
                dv.defined = true;
                dv.bytes_val = result;
                dv.wire_type = field.wire_type;
                set_dict(field.op.dict_key, dv);
            } else {
                if (static_cast<std::size_t>(prefix_len) > base.size()) {
                    ctx.set_error(DecodeStatus::InvalidEncoding, "delta_prefix_too_long",
                                  "Delta prefix length exceeds base for " + field.name);
                    return DecodeStatus::InvalidEncoding;
                }
                std::vector<std::uint8_t> result(base.begin(), base.begin() + prefix_len);
                const std::uint8_t* tail_ptr = nullptr;
                if (tail_len > 0) {
                    st = ctx.cursor.read_bytes(tail_len, tail_ptr);
                    if (st != DecodeStatus::Ok) return st;
                    result.insert(result.end(), tail_ptr, tail_ptr + tail_len);
                }
                out.value = result;
                DictValue dv;
                dv.defined = true;
                dv.bytes_val = result;
                dv.wire_type = field.wire_type;
                set_dict(field.op.dict_key, dv);
            }
        }

        return DecodeStatus::Ok;
    }

    // --- Operator: tail ---
    DecodeStatus decode_tail_op(DecodeCtx& ctx, const CompiledField& field,
                                 DecodedField& out, bool /*pmap_present*/) {
        out.source = ValueSource::Tail;

        DictValue* dict = get_dict(field.op.dict_key);
        bool has_base = dict && dict->defined;

        // Read retained prefix length (uInt32) then tail bytes
        std::uint32_t retained_len = 0;
        DecodeStatus st = ctx.cursor.read_stopbit_u32(retained_len);
        if (st != DecodeStatus::Ok) return st;

        if (field.wire_type == DecWireType::AsciiString ||
            field.wire_type == DecWireType::UnicodeString) {
            std::string base = has_base ? dict->str_val : std::string();
            if (retained_len > base.size()) {
                ctx.set_error(DecodeStatus::InvalidEncoding, "tail_retained_too_long",
                              "Tail retained length exceeds base for " + field.name);
                return DecodeStatus::InvalidEncoding;
            }
            std::string result = base.substr(0, retained_len);

            // Read tail bytes (stop-bit terminated ASCII or length-prefixed)
            if (field.wire_type == DecWireType::AsciiString) {
                std::string tail;
                st = ctx.cursor.read_ascii_string(tail);
                if (st != DecodeStatus::Ok) return st;
                result += tail;
            } else {
                std::string tail;
                st = ctx.cursor.read_unicode_string(tail);
                if (st != DecodeStatus::Ok) return st;
                result += tail;
            }

            out.value = result;
            DictValue dv;
            dv.defined = true;
            dv.str_val = result;
            dv.wire_type = field.wire_type;
            set_dict(field.op.dict_key, dv);
        } else if (field.wire_type == DecWireType::ByteVector) {
            std::vector<std::uint8_t> base = has_base ? dict->bytes_val : std::vector<std::uint8_t>();
            if (retained_len > base.size()) {
                ctx.set_error(DecodeStatus::InvalidEncoding, "tail_retained_too_long",
                              "Tail retained length exceeds base for " + field.name);
                return DecodeStatus::InvalidEncoding;
            }
            std::vector<std::uint8_t> result(base.begin(), base.begin() + retained_len);

            std::vector<std::uint8_t> tail;
            st = ctx.cursor.read_byte_vector(tail);
            if (st != DecodeStatus::Ok) return st;
            result.insert(result.end(), tail.begin(), tail.end());

            out.value = result;
            DictValue dv;
            dv.defined = true;
            dv.bytes_val = result;
            dv.wire_type = field.wire_type;
            set_dict(field.op.dict_key, dv);
        }

        return DecodeStatus::Ok;
    }

    // Read a value directly from wire
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
                        out.value = std::monostate{};
                    } else {
                        out.value = val;
                    }
                }
                break;
            }
            case DecWireType::AsciiString: {
                std::string val;
                DecodeStatus st = ctx.cursor.read_ascii_string(val);
                if (st != DecodeStatus::Ok) return st;
                out.value = val;
                break;
            }
            case DecWireType::UnicodeString: {
                std::string val;
                DecodeStatus st = ctx.cursor.read_unicode_string(val);
                if (st != DecodeStatus::Ok) return st;
                out.value = val;
                break;
            }
            case DecWireType::ByteVector: {
                std::vector<std::uint8_t> val;
                DecodeStatus st = ctx.cursor.read_byte_vector(val);
                if (st != DecodeStatus::Ok) return st;
                out.value = val;
                break;
            }
            case DecWireType::Decimal: {
                std::int32_t exp = 0;
                std::int64_t man = 0;
                bool is_null = false;
                DecodeStatus st = ctx.cursor.read_decimal(exp, man, is_null);
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

    // Update dictionary from a decoded field
    void update_dict_from_decoded(const std::string& key, DecWireType wire_type,
                                   const DecodedField& decoded) {
        DictValue dv;
        dv.defined = true;
        dv.wire_type = wire_type;
        dv.is_null = decoded.is_null;

        if (!decoded.is_null) {
            if (wire_type == DecWireType::uInt32 || wire_type == DecWireType::uInt64) {
                dv.uint_val = std::get<std::uint64_t>(decoded.value);
                dv.int_val = static_cast<std::int64_t>(dv.uint_val);
            } else if (wire_type == DecWireType::Int32 || wire_type == DecWireType::Int64) {
                dv.int_val = std::get<std::int64_t>(decoded.value);
                dv.uint_val = static_cast<std::uint64_t>(dv.int_val);
            } else if (wire_type == DecWireType::AsciiString || wire_type == DecWireType::UnicodeString) {
                dv.str_val = std::get<std::string>(decoded.value);
            } else if (wire_type == DecWireType::ByteVector) {
                dv.bytes_val = std::get<std::vector<std::uint8_t>>(decoded.value);
            }
        }

        set_dict(key, dv);
    }

    // Copy dictionary value to decoded field
    void copy_dict_to_decoded(const DictValue& dict, DecWireType wire_type, DecodedField& out) {
        if (dict.is_null) {
            out.is_null = true;
            out.value = std::monostate{};
            return;
        }
        switch (wire_type) {
            case DecWireType::uInt32:
            case DecWireType::uInt64:
                out.value = dict.uint_val;
                break;
            case DecWireType::Int32:
            case DecWireType::Int64:
                out.value = dict.int_val;
                break;
            case DecWireType::AsciiString:
            case DecWireType::UnicodeString:
                out.value = dict.str_val;
                break;
            case DecWireType::ByteVector:
                out.value = dict.bytes_val;
                break;
            default:
                break;
        }
    }

    // --- Decode decimal field ---
    DecodeStatus decode_decimal_field(DecodeCtx& ctx, const CompiledField& field,
                                       DecodedField& out, const std::string& field_path) {
        out.name = field.name;
        out.has_fix_tag = field.has_fix_tag;
        out.fix_tag = field.fix_tag;
        out.field_path = field_path;
        out.is_null = false;
        out.is_present = true;

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

        // For decimal, read exponent and mantissa
        // The exponent and mantissa have their own operators
        std::int32_t exponent = 0;
        std::int64_t mantissa = 0;
        bool dec_null = false;
        ValueSource exp_source = ValueSource::Wire;

        // Decode exponent
        switch (field.exponent_op.kind) {
            case OpKind::None: {
                // Read exponent from wire
                if (field.is_mandatory) {
                    DecodeStatus st = ctx.cursor.read_stopbit_i32(exponent);
                    if (st != DecodeStatus::Ok) return st;
                } else {
                    bool is_null = false;
                    DecodeStatus st = ctx.cursor.read_nullable_i32(exponent, is_null);
                    if (st != DecodeStatus::Ok) return st;
                    if (is_null) {
                        dec_null = true;
                        // Don't read mantissa
                        break;
                    }
                }
                // Update exponent dict
                DictValue dv;
                dv.defined = true;
                dv.int_val = exponent;
                dv.wire_type = DecWireType::Int32;
                set_dict(field.exponent_op.dict_key, dv);
                break;
            }
            case OpKind::Copy: {
                // Read exponent from wire
                if (field.is_mandatory) {
                    DecodeStatus st = ctx.cursor.read_stopbit_i32(exponent);
                    if (st != DecodeStatus::Ok) return st;
                    exp_source = ValueSource::Wire;
                    DictValue dv;
                    dv.defined = true;
                    dv.int_val = exponent;
                    dv.wire_type = DecWireType::Int32;
                    set_dict(field.exponent_op.dict_key, dv);
                } else {
                    bool is_null = false;
                    DecodeStatus st = ctx.cursor.read_nullable_i32(exponent, is_null);
                    if (st != DecodeStatus::Ok) return st;
                    if (is_null) {
                        dec_null = true;
                    } else {
                        exp_source = ValueSource::Wire;
                        DictValue dv;
                        dv.defined = true;
                        dv.int_val = exponent;
                        dv.wire_type = DecWireType::Int32;
                        set_dict(field.exponent_op.dict_key, dv);
                    }
                }
                break;
            }
            default: {
                // For other operators on exponent, read from wire
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
                if (!dec_null) {
                    DictValue dv;
                    dv.defined = true;
                    dv.int_val = exponent;
                    dv.wire_type = DecWireType::Int32;
                    set_dict(field.exponent_op.dict_key, dv);
                }
                break;
            }
        }

        if (dec_null) {
            out.is_null = true;
            out.value = std::monostate{};
            return DecodeStatus::Ok;
        }

        // Decode mantissa
        if (field.is_mandatory) {
            DecodeStatus st = ctx.cursor.read_stopbit_i64(mantissa);
            if (st != DecodeStatus::Ok) return st;
        } else {
            bool is_null = false;
            DecodeStatus st = ctx.cursor.read_nullable_i64(mantissa, is_null);
            if (st != DecodeStatus::Ok) return st;
            if (is_null) {
                // Mantissa null after non-null exponent is unusual but allowed
                mantissa = 0;
            }
        }

        // Update mantissa dict
        DictValue dv;
        dv.defined = true;
        dv.int_val = mantissa;
        dv.wire_type = DecWireType::Int64;
        set_dict(field.mantissa_op.dict_key, dv);

        DecodedDecimal dd;
        dd.exponent = exponent;
        dd.mantissa = mantissa;
        dd.is_null = false;
        out.value = dd;

        return DecodeStatus::Ok;
    }

    // --- Decode fields recursively ---
    DecodeStatus decode_fields(DecodeCtx& ctx, const std::vector<CompiledField>& fields,
                                std::vector<DecodedField>& out, const std::string& parent_path) {
        for (const auto& field : fields) {
            // Skip length fields - they're handled inside sequence decoding
            if (field.wire_type == DecWireType::uInt32 && !field.name.empty() &&
                field.is_mandatory && field.has_pmap_bit == false) {
                // Check if this is a length field inside a sequence
                // Length fields are decoded as part of sequence handling
                // For now, detect by checking if the parent is a sequence
                // Actually, we need a better way to detect length fields
            }

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
            } else if (field.has_children) {
                // Group
                decoded.name = field.name;
                decoded.has_fix_tag = field.has_fix_tag;
                decoded.fix_tag = field.fix_tag;
                decoded.field_path = field_path;
                decoded.is_group = true;

                bool pmap_present = true;
                if (field.has_pmap_bit) {
                    pmap_present = consume_pmap_bit(ctx);
                }

                if (!field.is_mandatory && !pmap_present) {
                    decoded.is_null = true;
                    decoded.is_present = false;
                } else {
                    DecodeStatus st = decode_fields(ctx, field.children, decoded.children, field_path);
                    if (st != DecodeStatus::Ok) return st;
                }
            } else {
                DecodeStatus st = decode_scalar(ctx, field, decoded, field_path);
                if (st != DecodeStatus::Ok) return st;
            }

            out.push_back(std::move(decoded));
        }
        return DecodeStatus::Ok;
    }

    // --- Decode sequence ---
    DecodeStatus decode_sequence(DecodeCtx& ctx, const CompiledField& field,
                                  DecodedField& out, const std::string& field_path) {
        out.name = field.name;
        out.has_fix_tag = field.has_fix_tag;
        out.fix_tag = field.fix_tag;
        out.field_path = field_path;
        out.is_sequence = true;
        out.sequence_is_null = false;

        // Check presence map for optional sequence
        bool pmap_present = true;
        if (field.has_pmap_bit) {
            pmap_present = consume_pmap_bit(ctx);
        }

        if (!field.is_mandatory && !pmap_present) {
            out.sequence_is_null = true;
            out.is_present = false;
            return DecodeStatus::Ok;
        }

        // Read sequence length
        std::uint32_t length = 0;
        DecodeStatus st = ctx.cursor.read_stopbit_u32(length);
        if (st != DecodeStatus::Ok) return st;

        if (length > limits.max_sequence_entries) {
            ctx.set_error(DecodeStatus::LimitExceeded, "sequence_limit",
                          "Sequence entry count exceeds limit in " + field_path);
            return DecodeStatus::LimitExceeded;
        }

        // Get entry fields (skip the length field which is first child)
        std::vector<CompiledField> entry_fields;
        for (std::size_t i = 1; i < field.children.size(); ++i) {
            entry_fields.push_back(field.children[i]);
        }

        out.entries.reserve(length);
        for (std::uint32_t i = 0; i < length; ++i) {
            if (!check_node_limit(ctx)) return DecodeStatus::LimitExceeded;

            // Each entry has its own presence map if needed
            std::vector<bool> saved_pmap = ctx.pmap;
            std::size_t saved_pmap_index = ctx.pmap_index;

            if (field.entry_has_pmap) {
                // Read entry-level presence map
                // The number of bits needed is the count of pmap-consuming fields in the entry
                std::uint32_t entry_pmap_bits = 0;
                for (const auto& ef : entry_fields) {
                    if (ef.has_pmap_bit) entry_pmap_bits++;
                }
                if (entry_pmap_bits > 0) {
                    st = ctx.cursor.read_presence_map(entry_pmap_bits, ctx.pmap);
                    if (st != DecodeStatus::Ok) return st;
                    ctx.pmap_index = 0;
                } else {
                    ctx.pmap.clear();
                    ctx.pmap_index = 0;
                }
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

    // --- Main decode ---
    DecodeResult do_decode(const std::uint8_t* data, std::size_t size, bool exact) {
        DecodeResult result;
        DecodeCtx ctx;
        ctx.cursor = WireCursor(data, size);
        ctx.node_count = 0;

        // Begin transaction
        begin_transaction();

        // Read message-level presence map
        // We need to know how many pmap bits the template needs
        // First, we need to determine which template to use

        // Check for template-ID in pmap
        // The message-level pmap always has at least 1 bit for template-ID
        // We read a preliminary pmap to check the template-ID bit
        std::vector<bool> msg_pmap;
        DecodeStatus st = ctx.cursor.read_presence_map(1, msg_pmap);
        if (st != DecodeStatus::Ok) {
            rollback();
            result.status = st;
            result.issues = std::move(ctx.issues);
            return result;
        }

        bool template_id_present = msg_pmap[0];

        // Determine template ID
        std::uint32_t template_id = 0;
        if (template_id_present) {
            // Read template ID from wire
            st = ctx.cursor.read_stopbit_u32(template_id);
            if (st != DecodeStatus::Ok) {
                rollback();
                result.status = st;
                result.issues = std::move(ctx.issues);
                return result;
            }

            // Validate template ID
            if (!templates->find(template_id)) {
                rollback();
                result.status = DecodeStatus::UnknownTemplate;
                ctx.set_error(DecodeStatus::UnknownTemplate, "unknown_template",
                              "Unknown template ID: " + std::to_string(template_id));
                result.issues = std::move(ctx.issues);
                return result;
            }
        } else {
            // Use previous template ID
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

        ctx.tmpl = templates->find(template_id);
        if (!ctx.tmpl) {
            rollback();
            result.status = DecodeStatus::UnknownTemplate;
            result.issues = std::move(ctx.issues);
            return result;
        }

        // Now read the full presence map for this template
        // We already consumed 1 bit (template-ID), read the rest
        // Read the full presence map upfront.
        // The pmap size depends on the template, but we read it as a stop-bit-terminated byte sequence.
        // We already read 1 bit. Let's re-read the full pmap.

        // Reset cursor to start of pmap
        ctx.cursor = WireCursor(data, size);

        // Read the full presence map (up to 64 bytes * 7 bits = 448 bits)
        st = ctx.cursor.read_presence_map(64 * 7, ctx.pmap);
        if (st != DecodeStatus::Ok) {
            rollback();
            result.status = st;
            result.issues = std::move(ctx.issues);
            return result;
        }
        ctx.pmap_index = 0;

        // Consume template-ID bit
        template_id_present = ctx.pmap[ctx.pmap_index++];

        if (template_id_present) {
            st = ctx.cursor.read_stopbit_u32(template_id);
            if (st != DecodeStatus::Ok) {
                rollback();
                result.status = st;
                result.issues = std::move(ctx.issues);
                return result;
            }
            ctx.tmpl = templates->find(template_id);
            if (!ctx.tmpl) {
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
                              "No previous template ID");
                result.issues = std::move(ctx.issues);
                return result;
            }
            template_id = prev_template_id;
            ctx.tmpl = templates->find(template_id);
        }

        // Decode message fields
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

        // Success: commit transaction
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

    SessionFingerprint make_fingerprint() const {
        SessionFingerprint fp;
        fp.has_template_id = has_prev_template_id;
        fp.template_id = prev_template_id;
        fp.dict_entry_count = dictionary.size();

        // Compute deterministic hash of dictionary entries
        std::uint64_t hash = 0;
        for (const auto& [key, val] : dictionary) {
            // Simple hash combining
            for (char c : key) {
                hash = hash * 31 + static_cast<std::uint64_t>(c);
            }
            hash = hash * 31 + static_cast<std::uint64_t>(val.int_val);
            hash = hash * 31 + (val.defined ? 1 : 0);
            hash = hash * 31 + (val.is_null ? 1 : 0);
        }
        fp.dict_hash = hash;
        return fp;
    }
};

// --- Public API ---

DecoderSession::DecoderSession(const CompiledTemplateSet& templates, const DecodeLimits& limits)
    : impl_(std::make_unique<Impl>()) {
    impl_->templates = &templates;
    impl_->limits = limits;
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
    impl_->dictionary.clear();
    impl_->journal.clear();
}

SessionFingerprint DecoderSession::fingerprint() const {
    return impl_->make_fingerprint();
}

const CompiledTemplateSet& DecoderSession::templates() const {
    return *impl_->templates;
}

}  // namespace moex_fast
