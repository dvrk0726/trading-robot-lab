#include "moex_fast/decoder_report.hpp"
#include "moex_fast/wire_cursor.hpp"
#include <sstream>
#include <iomanip>
#include <filesystem>

namespace moex_fast {

namespace {

std::string basename(const std::string& path) {
    std::filesystem::path p(path);
    return p.filename().string();
}

void indent(std::ostringstream& oss, int depth) {
    for (int i = 0; i < depth; ++i) oss << "  ";
}

void write_decoded_scalar(std::ostringstream& oss, const DecodedScalar& val, bool is_null) {
    if (is_null) {
        oss << "null";
        return;
    }
    if (std::holds_alternative<std::monostate>(val)) {
        oss << "null";
    } else if (std::holds_alternative<std::uint64_t>(val)) {
        oss << std::get<std::uint64_t>(val);
    } else if (std::holds_alternative<std::int64_t>(val)) {
        oss << std::get<std::int64_t>(val);
    } else if (std::holds_alternative<std::string>(val)) {
        oss << "\"" << std::get<std::string>(val) << "\"";
    } else if (std::holds_alternative<std::vector<std::uint8_t>>(val)) {
        oss << "0x";
        for (auto b : std::get<std::vector<std::uint8_t>>(val)) {
            oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
        }
        oss << std::dec;
    } else if (std::holds_alternative<DecodedDecimal>(val)) {
        const auto& d = std::get<DecodedDecimal>(val);
        oss << "{exp:" << d.exponent << ",man:" << d.mantissa << "}";
    }
}

void write_field_text(std::ostringstream& oss, const DecodedField& field, int depth) {
    indent(oss, depth);
    oss << field.name;
    if (field.has_fix_tag) oss << " (tag " << field.fix_tag << ")";
    oss << ": ";

    if (field.is_sequence) {
        if (field.sequence_is_null) {
            oss << "null sequence\n";
        } else {
            oss << "[" << field.entries.size() << " entries]\n";
            for (std::size_t i = 0; i < field.entries.size(); ++i) {
                indent(oss, depth + 1);
                oss << "entry[" << i << "]:\n";
                for (const auto& child : field.entries[i]) {
                    write_field_text(oss, child, depth + 2);
                }
            }
        }
    } else if (field.is_group) {
        if (!field.is_present) {
            oss << "absent group\n";
        } else {
            oss << "{group}\n";
            for (const auto& child : field.children) {
                write_field_text(oss, child, depth + 1);
            }
        }
    } else {
        write_decoded_scalar(oss, field.value, field.is_null);
        oss << " [" << value_source_name(field.source) << "]\n";
    }
}

void write_field_json(std::ostringstream& oss, const DecodedField& field, int indent_count);

void write_indent(std::ostringstream& oss, int count) {
    for (int i = 0; i < count; ++i) oss << "  ";
}

void write_scalar_json(std::ostringstream& oss, const DecodedScalar& val, bool is_null) {
    if (is_null || std::holds_alternative<std::monostate>(val)) {
        oss << "null";
    } else if (std::holds_alternative<std::uint64_t>(val)) {
        oss << std::get<std::uint64_t>(val);
    } else if (std::holds_alternative<std::int64_t>(val)) {
        oss << std::get<std::int64_t>(val);
    } else if (std::holds_alternative<std::string>(val)) {
        oss << "\"" << json_escape_string(std::get<std::string>(val)) << "\"";
    } else if (std::holds_alternative<std::vector<std::uint8_t>>(val)) {
        oss << "\"";
        // Lowercase hex for byte vectors
        oss << std::hex << std::nouppercase << std::setfill('0');
        for (auto b : std::get<std::vector<std::uint8_t>>(val)) {
            oss << std::setw(2) << static_cast<int>(b);
        }
        oss << "\"" << std::dec;
    } else if (std::holds_alternative<DecodedDecimal>(val)) {
        const auto& d = std::get<DecodedDecimal>(val);
        oss << "{\"exponent\":" << d.exponent << ",\"mantissa\":" << d.mantissa << "}";
    }
}

std::string wire_type_for_field(const DecodedField& field) {
    if (field.is_sequence) return "sequence";
    if (field.is_group) return "group";
    if (std::holds_alternative<std::uint64_t>(field.value)) return "uInt";
    if (std::holds_alternative<std::int64_t>(field.value)) return "int";
    if (std::holds_alternative<std::string>(field.value)) return "string";
    if (std::holds_alternative<std::vector<std::uint8_t>>(field.value)) return "byteVector";
    if (std::holds_alternative<DecodedDecimal>(field.value)) return "decimal";
    return "null";
}

void write_field_json(std::ostringstream& oss, const DecodedField& field, int ind) {
    write_indent(oss, ind);
    oss << "{";
    oss << "\"name\":\"" << json_escape_string(field.name) << "\"";
    if (field.has_fix_tag) oss << ",\"fixTag\":" << field.fix_tag;
    oss << ",\"fieldPath\":\"" << json_escape_string(field.field_path) << "\"";
    oss << ",\"isPresent\":" << (field.is_present ? "true" : "false");
    oss << ",\"isNull\":" << (field.is_null ? "true" : "false");
    oss << ",\"source\":\"" << value_source_name(field.source) << "\"";
    oss << ",\"type\":\"" << wire_type_for_field(field) << "\"";

    if (field.is_sequence) {
        if (field.sequence_is_null) {
            oss << ",\"value\":null";
        } else {
            oss << ",\"entries\":[";
            for (std::size_t i = 0; i < field.entries.size(); ++i) {
                if (i > 0) oss << ",";
                oss << "[";
                for (std::size_t j = 0; j < field.entries[i].size(); ++j) {
                    if (j > 0) oss << ",";
                    write_field_json(oss, field.entries[i][j], 0);
                }
                oss << "]";
            }
            oss << "]";
        }
    } else if (field.is_group) {
        if (!field.is_present) {
            oss << ",\"value\":null";
        } else {
            oss << ",\"fields\":[";
            for (std::size_t i = 0; i < field.children.size(); ++i) {
                if (i > 0) oss << ",";
                write_field_json(oss, field.children[i], 0);
            }
            oss << "]";
        }
    } else {
        oss << ",\"value\":";
        write_scalar_json(oss, field.value, field.is_null);
    }

    oss << "}";
}

}  // namespace

std::string decode_text_report(const DecodeResult& result,
                                const std::string& templates_path,
                                std::size_t templates_size,
                                const std::string& templates_sha256,
                                std::size_t input_size,
                                const std::string& input_sha256) {
    std::ostringstream oss;
    oss << "=== FAST Decode Report ===\n";
    oss << "Schema version: 1.0\n";
    oss << "Decoder version: 0.2.0\n";
    oss << "Templates: " << basename(templates_path)
        << " (" << templates_size << " bytes, sha256:" << templates_sha256 << ")\n";
    oss << "Input: " << input_size << " bytes, sha256:" << input_sha256 << "\n";
    oss << "Status: " << decode_status_name(result.status) << "\n";

    if (result.status == DecodeStatus::Ok) {
        oss << "Template: " << result.message.template_id
            << " (" << result.message.template_name << ")\n";
        oss << "Bytes consumed: " << result.bytes_consumed << "\n";
        oss << "Fields:\n";
        for (const auto& field : result.message.fields) {
            write_field_text(oss, field, 1);
        }
    }

    if (!result.issues.empty()) {
        oss << "Issues:\n";
        for (const auto& issue : result.issues) {
            oss << "  [" << issue.code << "] offset=" << issue.offset;
            if (issue.has_template_id) oss << " template=" << issue.template_id;
            if (!issue.field_path.empty()) oss << " path=" << issue.field_path;
            oss << ": " << issue.message << "\n";
        }
    }

    return oss.str();
}

std::string decode_json_report(const DecodeResult& result,
                                const std::string& templates_path,
                                std::size_t templates_size,
                                const std::string& templates_sha256,
                                std::size_t input_size,
                                const std::string& input_sha256) {
    std::ostringstream oss;
    oss << "{";
    oss << "\"schemaVersion\":\"1.0\"";
    oss << ",\"decoderVersion\":\"0.2.0\"";
    oss << ",\"templates\":{\"basename\":\"" << basename(templates_path)
        << "\",\"size\":" << templates_size
        << ",\"sha256\":\"" << templates_sha256 << "\"}";
    oss << ",\"input\":{\"size\":" << input_size
        << ",\"sha256\":\"" << input_sha256 << "\"}";
    oss << ",\"status\":\"" << decode_status_name(result.status) << "\"";

    if (result.status == DecodeStatus::Ok) {
        oss << ",\"templateId\":" << result.message.template_id;
        oss << ",\"templateName\":\"" << result.message.template_name << "\"";
        oss << ",\"bytesConsumed\":" << result.bytes_consumed;
        oss << ",\"fields\":[";
        for (std::size_t i = 0; i < result.message.fields.size(); ++i) {
            if (i > 0) oss << ",";
            write_field_json(oss, result.message.fields[i], 0);
        }
        oss << "]";
    }

    if (!result.issues.empty()) {
        oss << ",\"issues\":[";
        for (std::size_t i = 0; i < result.issues.size(); ++i) {
            if (i > 0) oss << ",";
            oss << "{\"code\":\"" << json_escape_string(result.issues[i].code) << "\"";
            oss << ",\"offset\":" << result.issues[i].offset;
            if (result.issues[i].has_template_id) {
                oss << ",\"templateId\":" << result.issues[i].template_id;
            }
            if (!result.issues[i].field_path.empty()) {
                oss << ",\"fieldPath\":\"" << json_escape_string(result.issues[i].field_path) << "\"";
            }
            oss << ",\"message\":\"" << json_escape_string(result.issues[i].message) << "\"}";
        }
        oss << "]";
    }

    oss << "}";
    return oss.str();
}

}  // namespace moex_fast
