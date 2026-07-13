#include "moex_fast/fast_decoder.hpp"
#include "moex_fast/wire_cursor.hpp"
#include <map>
#include <functional>
#include <cstring>
#include <sstream>
#include <iomanip>

namespace moex_fast {

namespace {

struct DictValue {
    bool defined = false;
    bool is_null = false;
    std::uint64_t uint_val = 0;
    std::int64_t int_val = 0;
    std::string str_val;
    std::vector<std::uint8_t> bytes_val;
    DecWireType wire_type = DecWireType::uInt32;
};

struct JournalEntry {
    std::string key;
    DictValue previous;
    bool was_defined = false;
};

}  // namespace

// --- PImpl ---
struct DecoderSession::Impl {
    CompiledTemplateSet templates;
    DecodeLimits limits;

    // Session state
    bool has_prev_template_id = false;
    std::uint32_t prev_template_id = 0;

    // Dictionary: std::map for deterministic iteration order
    std::map<std::string, DictValue> dictionary;

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

    // --- Transaction journal ---

    void begin_transaction() {
        journal.clear();
        has_prev_template_id_snapshot = has_prev_template_id;
        prev_template_id_snapshot = prev_template_id;
    }

    void rollback() {
        for (auto it = journal.rbegin(); it != journal.rend(); ++it) {
            if (it->was_defined) {
                dictionary[it->key] = it->previous;
            } else {
                dictionary.erase(it->key);
            }
        }
        journal.clear();
        has_prev_template_id = has_prev_template_id_snapshot;
        prev_template_id = prev_template_id_snapshot;
    }

    void commit() {
        journal.clear();
    }

    // --- Dictionary helpers ---

    static void store_int_value(DecodedField& out, DecWireType wire_type, std::int64_t val) {
        if (wire_type == DecWireType::uInt32 || wire_type == DecWireType::uInt64) {
            out.value = static_cast<std::uint64_t>(val);
        } else {
            out.value = val;
        }
    }

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

    DictValue* get_dict(const std::string& key) {
        auto it = dictionary.find(key);
        if (it == dictionary.end()) return nullptr;
        return &it->second;
    }

    void set_dict(const std::string& key, const DictValue& val) {
        journal_update(key);
        dictionary[key] = val;
    }

    // --- Decoded field <-> dictionary conversion ---

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
    // No pmap bit. Mandatory: ordinary wire. Optional: nullable wire; NULL from wire value alone.
    DecodeStatus decode_none_op(DecodeCtx& ctx, const CompiledField& field,
                                 DecodedField& out, bool /*pmap_present*/) {
        out.source = ValueSource::Wire;
        return read_wire_value(ctx, field.wire_type, field.is_mandatory, out);
    }

    // --- Operator: constant ---
    // Mandatory: no pmap, no wire bytes, value from compiled. Optional: pmap consumed, 0=null, 1=constant.
    DecodeStatus decode_constant_op(DecodeCtx& /*ctx*/, const CompiledField& field,
                                     DecodedField& out, bool pmap_present) {
        out.source = ValueSource::Constant;

        if (!field.is_mandatory && !pmap_present) {
            out.is_null = true;
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

    // --- Operator: default ---
    // pmap consumed. 0=use initial value. 1=wire read + dict update.
    DecodeStatus decode_default_op(DecodeCtx& ctx, const CompiledField& field,
                                    DecodedField& out, bool pmap_present) {
        if (pmap_present) {
            out.source = ValueSource::Wire;
            DecodeStatus st = read_wire_value(ctx, field.wire_type, field.is_mandatory, out);
            if (st != DecodeStatus::Ok) return st;
            update_dict_from_decoded(field.op.dict_key, field.wire_type, out);
        } else {
            out.source = ValueSource::Default;
            if (field.op.has_initial) {
                if (field.wire_type == DecWireType::AsciiString ||
                    field.wire_type == DecWireType::UnicodeString) {
                    out.value = field.op.initial_str;
                } else if (field.wire_type == DecWireType::uInt32 ||
                           field.wire_type == DecWireType::uInt64) {
                    out.value = field.op.initial_uint;
                } else {
                    out.value = field.op.initial_int;
                }
            } else {
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
    // pmap consumed. 0=use dict value or initial. 1=wire read + dict update.
    // Undefined previous (no dict, no initial): mandatory=error, optional=null.
    DecodeStatus decode_copy_op(DecodeCtx& ctx, const CompiledField& field,
                                 DecodedField& out, bool pmap_present) {
        if (pmap_present) {
            out.source = ValueSource::Wire;
            DecodeStatus st = read_wire_value(ctx, field.wire_type, field.is_mandatory, out);
            if (st != DecodeStatus::Ok) return st;
            update_dict_from_decoded(field.op.dict_key, field.wire_type, out);
        } else {
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
                if (field.wire_type == DecWireType::AsciiString ||
                    field.wire_type == DecWireType::UnicodeString) {
                    out.value = field.op.initial_str;
                } else if (field.wire_type == DecWireType::uInt32 ||
                           field.wire_type == DecWireType::uInt64) {
                    out.value = field.op.initial_uint;
                } else {
                    out.value = field.op.initial_int;
                }
                // Store initial in dictionary for subsequent messages
                DictValue dv;
                dv.defined = true;
                dv.is_null = false;
                if (field.wire_type == DecWireType::AsciiString ||
                    field.wire_type == DecWireType::UnicodeString) {
                    dv.str_val = field.op.initial_str;
                } else if (field.wire_type == DecWireType::uInt32 ||
                           field.wire_type == DecWireType::uInt64) {
                    dv.uint_val = field.op.initial_uint;
                } else {
                    dv.int_val = field.op.initial_int;
                    dv.uint_val = static_cast<std::uint64_t>(field.op.initial_int);
                }
                dv.wire_type = field.wire_type;
                set_dict(field.op.dict_key, dv);
            } else {
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
    // pmap consumed. 0=dict+1 or initial. 1=wire read + dict update.
    // Checked overflow arithmetic. Undefined: mandatory=error, optional=null.
    DecodeStatus decode_increment_op(DecodeCtx& ctx, const CompiledField& field,
                                      DecodedField& out, bool pmap_present) {
        if (pmap_present) {
            out.source = ValueSource::Wire;
            DecodeStatus st = read_wire_value(ctx, field.wire_type, field.is_mandatory, out);
            if (st != DecodeStatus::Ok) return st;
            update_dict_from_decoded(field.op.dict_key, field.wire_type, out);
        } else {
            out.source = ValueSource::Increment;
            DictValue* dict = get_dict(field.op.dict_key);
            if (dict && dict->defined) {
                // Checked increment of previous dictionary value
                bool is_unsigned = (field.wire_type == DecWireType::uInt32 ||
                                    field.wire_type == DecWireType::uInt64);
                if (is_unsigned) {
                    std::uint64_t prev = dict->uint_val;
                    std::uint64_t max_val = (field.wire_type == DecWireType::uInt32)
                                                ? static_cast<std::uint64_t>(UINT32_MAX)
                                                : UINT64_MAX;
                    if (prev >= max_val) {
                        ctx.set_error(DecodeStatus::IntegerOverflow, "increment_overflow",
                                      "Increment overflow on " + field.name);
                        return DecodeStatus::IntegerOverflow;
                    }
                    std::uint64_t new_val = prev + 1;
                    out.value = new_val;
                    DictValue dv;
                    dv.defined = true;
                    dv.uint_val = new_val;
                    dv.int_val = static_cast<std::int64_t>(new_val);
                    dv.wire_type = field.wire_type;
                    set_dict(field.op.dict_key, dv);
                } else {
                    std::int64_t prev = dict->int_val;
                    std::int64_t max_val = (field.wire_type == DecWireType::Int32)
                                               ? static_cast<std::int64_t>(INT32_MAX)
                                               : INT64_MAX;
                    if (prev >= max_val) {
                        ctx.set_error(DecodeStatus::IntegerOverflow, "increment_overflow",
                                      "Increment overflow on " + field.name);
                        return DecodeStatus::IntegerOverflow;
                    }
                    std::int64_t new_val = prev + 1;
                    out.value = new_val;
                    DictValue dv;
                    dv.defined = true;
                    dv.int_val = new_val;
                    dv.uint_val = static_cast<std::uint64_t>(new_val);
                    dv.wire_type = field.wire_type;
                    set_dict(field.op.dict_key, dv);
                }
            } else if (field.op.has_initial) {
                if (field.wire_type == DecWireType::uInt32 ||
                    field.wire_type == DecWireType::uInt64) {
                    out.value = field.op.initial_uint;
                } else {
                    out.value = field.op.initial_int;
                }
                DictValue dv;
                dv.defined = true;
                if (field.wire_type == DecWireType::uInt32 ||
                    field.wire_type == DecWireType::uInt64) {
                    dv.uint_val = field.op.initial_uint;
                } else {
                    dv.int_val = field.op.initial_int;
                    dv.uint_val = static_cast<std::uint64_t>(field.op.initial_int);
                }
                dv.wire_type = field.wire_type;
                set_dict(field.op.dict_key, dv);
            } else {
                if (field.is_mandatory) {
                    ctx.set_error(DecodeStatus::MissingDictionaryValue, "undefined_increment",
                                  "No previous value for increment on " + field.name);
                    return DecodeStatus::MissingDictionaryValue;
                }
                out.is_null = true;
                out.value = std::monostate{};
            }
        }
        return DecodeStatus::Ok;
    }

    // --- Operator: delta ---
    // Always reads from wire (no pmap bit effect on reading).
    // Integers: signed delta + base. Strings: prefix length (i32), tail length (u32), tail bytes.
    // Undefined base: delta IS the value.
    DecodeStatus decode_delta_op(DecodeCtx& ctx, const CompiledField& field,
                                  DecodedField& out, bool /*pmap_present*/) {
        out.source = ValueSource::Delta;

        DictValue* dict = get_dict(field.op.dict_key);
        bool has_base = dict && dict->defined;

        if (field.wire_type == DecWireType::uInt32 || field.wire_type == DecWireType::uInt64 ||
            field.wire_type == DecWireType::Int32 || field.wire_type == DecWireType::Int64) {
            // Integer delta: signed delta from wire + base
            std::int64_t delta_val = 0;
            DecodeStatus st = ctx.cursor.read_stopbit_i64(delta_val);
            if (st != DecodeStatus::Ok) return st;

            std::int64_t result;
            if (has_base) {
                // Checked addition: base + delta
                std::int64_t base = dict->int_val;
                if (delta_val > 0 && base > INT64_MAX - delta_val) {
                    ctx.set_error(DecodeStatus::IntegerOverflow, "delta_overflow",
                                  "Delta addition overflow on " + field.name);
                    return DecodeStatus::IntegerOverflow;
                }
                if (delta_val < 0 && base < INT64_MIN - delta_val) {
                    ctx.set_error(DecodeStatus::IntegerOverflow, "delta_overflow",
                                  "Delta addition underflow on " + field.name);
                    return DecodeStatus::IntegerOverflow;
                }
                result = base + delta_val;
            } else {
                // No base: delta IS the value
                result = delta_val;
            }

            store_int_value(out, field.wire_type, result);
            DictValue dv;
            dv.defined = true;
            dv.int_val = result;
            dv.uint_val = static_cast<std::uint64_t>(result);
            dv.wire_type = field.wire_type;
            set_dict(field.op.dict_key, dv);

        } else if (field.wire_type == DecWireType::AsciiString ||
                   field.wire_type == DecWireType::UnicodeString) {
            // String delta: prefix length (i32), tail length (u32), tail bytes
            std::int32_t prefix_len = 0;
            DecodeStatus st = ctx.cursor.read_stopbit_i32(prefix_len);
            if (st != DecodeStatus::Ok) return st;

            std::uint32_t tail_len = 0;
            st = ctx.cursor.read_stopbit_u32(tail_len);
            if (st != DecodeStatus::Ok) return st;

            std::string base = has_base ? dict->str_val : std::string();
            std::string result;

            if (prefix_len >= 0) {
                // Prefix operation: keep first prefix_len bytes, replace tail
                if (static_cast<std::size_t>(prefix_len) > base.size()) {
                    ctx.set_error(DecodeStatus::InvalidEncoding, "delta_prefix_too_long",
                                  "Delta prefix length exceeds base for " + field.name);
                    return DecodeStatus::InvalidEncoding;
                }
                result = base.substr(0, prefix_len);
                const std::uint8_t* tail_ptr = nullptr;
                if (tail_len > 0) {
                    if (tail_len > limits.max_string_bytes - result.size()) {
                        ctx.set_error(DecodeStatus::LimitExceeded, "delta_tail_limit",
                                      "Delta tail exceeds string limit for " + field.name);
                        return DecodeStatus::LimitExceeded;
                    }
                    st = ctx.cursor.read_bytes(tail_len, tail_ptr);
                    if (st != DecodeStatus::Ok) return st;
                    result.append(reinterpret_cast<const char*>(tail_ptr), tail_len);
                }
            } else {
                // Suffix operation: remove last |prefix_len| bytes, append tail
                std::int32_t suffix_len = -prefix_len;
                if (static_cast<std::size_t>(suffix_len) > base.size()) {
                    ctx.set_error(DecodeStatus::InvalidEncoding, "delta_suffix_too_long",
                                  "Delta suffix length exceeds base for " + field.name);
                    return DecodeStatus::InvalidEncoding;
                }
                result = base.substr(0, base.size() - suffix_len);
                const std::uint8_t* tail_ptr = nullptr;
                if (tail_len > 0) {
                    if (tail_len > limits.max_string_bytes - result.size()) {
                        ctx.set_error(DecodeStatus::LimitExceeded, "delta_tail_limit",
                                      "Delta tail exceeds string limit for " + field.name);
                        return DecodeStatus::LimitExceeded;
                    }
                    st = ctx.cursor.read_bytes(tail_len, tail_ptr);
                    if (st != DecodeStatus::Ok) return st;
                    result.append(reinterpret_cast<const char*>(tail_ptr), tail_len);
                }
            }

            out.value = result;
            DictValue dv;
            dv.defined = true;
            dv.str_val = result;
            dv.wire_type = field.wire_type;
            set_dict(field.op.dict_key, dv);

        } else if (field.wire_type == DecWireType::ByteVector) {
            // Byte vector delta: same structure as string
            std::int32_t prefix_len = 0;
            DecodeStatus st = ctx.cursor.read_stopbit_i32(prefix_len);
            if (st != DecodeStatus::Ok) return st;

            std::uint32_t tail_len = 0;
            st = ctx.cursor.read_stopbit_u32(tail_len);
            if (st != DecodeStatus::Ok) return st;

            std::vector<std::uint8_t> base = has_base ? dict->bytes_val : std::vector<std::uint8_t>();
            std::vector<std::uint8_t> result;

            if (prefix_len >= 0) {
                if (static_cast<std::size_t>(prefix_len) > base.size()) {
                    ctx.set_error(DecodeStatus::InvalidEncoding, "delta_prefix_too_long",
                                  "Delta prefix length exceeds base for " + field.name);
                    return DecodeStatus::InvalidEncoding;
                }
                result.assign(base.begin(), base.begin() + prefix_len);
            } else {
                std::int32_t suffix_len = -prefix_len;
                if (static_cast<std::size_t>(suffix_len) > base.size()) {
                    ctx.set_error(DecodeStatus::InvalidEncoding, "delta_suffix_too_long",
                                  "Delta suffix length exceeds base for " + field.name);
                    return DecodeStatus::InvalidEncoding;
                }
                result.assign(base.begin(), base.begin() + (base.size() - suffix_len));
            }

            const std::uint8_t* tail_ptr = nullptr;
            if (tail_len > 0) {
                if (tail_len > limits.max_string_bytes - result.size()) {
                    ctx.set_error(DecodeStatus::LimitExceeded, "delta_tail_limit",
                                  "Delta tail exceeds limit for " + field.name);
                    return DecodeStatus::LimitExceeded;
                }
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

        return DecodeStatus::Ok;
    }

    // --- Operator: tail ---
    // Always reads from wire (no pmap bit effect on reading).
    // Retained prefix length (u32), then tail bytes.
    DecodeStatus decode_tail_op(DecodeCtx& ctx, const CompiledField& field,
                                 DecodedField& out, bool /*pmap_present*/) {
        out.source = ValueSource::Tail;

        DictValue* dict = get_dict(field.op.dict_key);
        bool has_base = dict && dict->defined;

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

            if (field.wire_type == DecWireType::AsciiString) {
                std::string tail;
                st = ctx.cursor.read_ascii_string(tail);
                if (st != DecodeStatus::Ok) return st;
                if (result.size() + tail.size() > limits.max_string_bytes) {
                    ctx.set_error(DecodeStatus::LimitExceeded, "tail_string_limit",
                                  "Tail result exceeds string limit for " + field.name);
                    return DecodeStatus::LimitExceeded;
                }
                result += tail;
            } else {
                std::string tail;
                st = ctx.cursor.read_unicode_string(tail);
                if (st != DecodeStatus::Ok) return st;
                if (result.size() + tail.size() > limits.max_string_bytes) {
                    ctx.set_error(DecodeStatus::LimitExceeded, "tail_string_limit",
                                  "Tail result exceeds string limit for " + field.name);
                    return DecodeStatus::LimitExceeded;
                }
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
            if (result.size() + tail.size() > limits.max_string_bytes) {
                ctx.set_error(DecodeStatus::LimitExceeded, "tail_bytes_limit",
                              "Tail result exceeds limit for " + field.name);
                return DecodeStatus::LimitExceeded;
            }
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

    // --- Decimal field decode ---
    // Exponent and mantissa have separate operators.
    // If exponent null => whole decimal null, mantissa NOT consumed.
    DecodeStatus decode_decimal_field(DecodeCtx& ctx, const CompiledField& field,
                                       DecodedField& out, const std::string& field_path) {
        out.name = field.name;
        out.has_fix_tag = field.has_fix_tag;
        out.fix_tag = field.fix_tag;
        out.field_path = field_path;
        out.is_null = false;
        out.is_present = true;

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

        // Decode exponent using its own operator
        switch (field.exponent_op.kind) {
            case OpKind::None: {
                if (field.is_mandatory) {
                    DecodeStatus st = ctx.cursor.read_stopbit_i32(exponent);
                    if (st != DecodeStatus::Ok) return st;
                } else {
                    bool is_null = false;
                    DecodeStatus st = ctx.cursor.read_nullable_i32(exponent, is_null);
                    if (st != DecodeStatus::Ok) return st;
                    if (is_null) {
                        dec_null = true;
                        break;
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
            case OpKind::Copy: {
                if (field.is_mandatory) {
                    DecodeStatus st = ctx.cursor.read_stopbit_i32(exponent);
                    if (st != DecodeStatus::Ok) return st;
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
                        DictValue dv;
                        dv.defined = true;
                        dv.int_val = exponent;
                        dv.wire_type = DecWireType::Int32;
                        set_dict(field.exponent_op.dict_key, dv);
                    }
                }
                break;
            }
            case OpKind::Default:
            case OpKind::Increment:
            case OpKind::Delta:
            case OpKind::Tail:
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

        // Decode mantissa using its own operator
        // FIX FAST 1.1: mantissa is always an ordinary non-nullable i64.
        // Nullable exponent indicates null decimal; mantissa is present iff exponent non-NULL.
        switch (field.mantissa_op.kind) {
            case OpKind::None:
            case OpKind::Copy:
            case OpKind::Default:
            case OpKind::Increment:
            case OpKind::Delta:
            case OpKind::Tail:
            default: {
                DecodeStatus st = ctx.cursor.read_stopbit_i64(mantissa);
                if (st != DecodeStatus::Ok) return st;

                DictValue dv;
                dv.defined = true;
                dv.int_val = mantissa;
                dv.wire_type = DecWireType::Int64;
                set_dict(field.mantissa_op.dict_key, dv);
                break;
            }
        }

        DecodedDecimal dd;
        dd.exponent = exponent;
        dd.mantissa = mantissa;
        dd.is_null = false;
        out.value = dd;

        return DecodeStatus::Ok;
    }

    // --- Sequence decode ---
    // Optional group pmap consumed. Sequence length checked before loop.
    // Entry presence maps. Transactional rollback on error.
    DecodeStatus decode_sequence(DecodeCtx& ctx, const CompiledField& field,
                                  DecodedField& out, const std::string& field_path) {
        out.name = field.name;
        out.has_fix_tag = field.has_fix_tag;
        out.fix_tag = field.fix_tag;
        out.field_path = field_path;
        out.is_sequence = true;
        out.sequence_is_null = false;

        // A sequence itself never receives a separate presence-map bit.
        // Optionality is conveyed through the sequence length wire form.
        // Read sequence length: mandatory = ordinary uInt32, optional = nullable uInt32.
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
        // The pmap is stop-bit terminated; read up to max bits.
        // Once we know the template, pmap_index tracks which bits have been consumed.
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

    // --- Deterministic fingerprint using FNV-1a ---
    SessionFingerprint make_fingerprint() const {
        SessionFingerprint fp;
        fp.has_template_id = has_prev_template_id;
        fp.template_id = prev_template_id;
        fp.dict_entry_count = dictionary.size();

        // FNV-1a 64-bit hash over sorted dictionary entries
        constexpr std::uint64_t FNV_OFFSET = 0xcbf29ce484222325ULL;
        constexpr std::uint64_t FNV_PRIME = 0x100000001b3ULL;
        std::uint64_t hash = FNV_OFFSET;

        auto fnv_mix = [&hash](const void* data, std::size_t len) {
            const auto* p = static_cast<const std::uint8_t*>(data);
            for (std::size_t i = 0; i < len; ++i) {
                hash ^= p[i];
                hash *= FNV_PRIME;
            }
        };

        // std::map iterates in sorted key order - deterministic
        for (const auto& [key, val] : dictionary) {
            fnv_mix(key.data(), key.size());

            std::uint8_t defined_byte = val.defined ? 1 : 0;
            fnv_mix(&defined_byte, 1);

            std::uint8_t null_byte = val.is_null ? 1 : 0;
            fnv_mix(&null_byte, 1);

            fnv_mix(&val.uint_val, sizeof(val.uint_val));
            fnv_mix(&val.int_val, sizeof(val.int_val));

            fnv_mix(val.str_val.data(), val.str_val.size());

            fnv_mix(val.bytes_val.data(), val.bytes_val.size());

            auto wt = static_cast<std::uint8_t>(val.wire_type);
            fnv_mix(&wt, 1);
        }

        // Convert to hex string
        std::ostringstream oss;
        oss << std::hex << std::setfill('0') << std::setw(16) << hash;
        fp.dict_hash = oss.str();

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
    impl_->dictionary.clear();
    impl_->journal.clear();
}

SessionFingerprint DecoderSession::fingerprint() const {
    return impl_->make_fingerprint();
}

const CompiledTemplateSet& DecoderSession::templates() const {
    return impl_->templates;
}

}  // namespace moex_fast
