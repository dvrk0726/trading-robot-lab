#include "moex_fast/inspect_types.hpp"

namespace moex_fast {

WireType parse_wire_type(const std::string& name) {
    if (name == "uInt32" || name == "uint32") return WireType::uInt32;
    if (name == "uInt64" || name == "uint64") return WireType::uInt64;
    if (name == "int32") return WireType::Int32;
    if (name == "int64") return WireType::Int64;
    if (name == "string") return WireType::AsciiString;
    if (name == "unicode") return WireType::UnicodeString;
    if (name == "decimal") return WireType::Decimal;
    if (name == "sequence") return WireType::Sequence;
    return WireType::Unknown;
}

const char* wire_type_name(WireType wt) {
    switch (wt) {
        case WireType::uInt32: return "uInt32";
        case WireType::uInt64: return "uInt64";
        case WireType::Int32: return "Int32";
        case WireType::Int64: return "Int64";
        case WireType::AsciiString: return "AsciiString";
        case WireType::UnicodeString: return "UnicodeString";
        case WireType::Decimal: return "Decimal";
        case WireType::Sequence: return "Sequence";
        default: return "Unknown";
    }
}

}  // namespace moex_fast
