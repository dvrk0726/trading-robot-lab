#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace moex_fast {

enum class Severity { Warning, Error };

struct InspectionIssue {
    Severity severity;
    std::string message;
};

struct InputFileInfo {
    std::string path;
    std::string file_name;
    std::uint64_t file_size{};
    std::string sha256;
    bool parse_ok{};
    bool validation_ok{};
};

enum class WireType {
    Unknown,
    uInt32, uInt64, Int32, Int64,
    AsciiString, UnicodeString,
    Decimal, Sequence
};

struct FastFieldDescriptor {
    std::uint32_t order{};
    std::string name;
    std::int32_t fix_tag{};
    WireType wire_type{WireType::Unknown};
    bool has_fix_tag{};
    bool is_mandatory{};
    bool is_constant{};
    std::string constant_value;
    bool is_sequence_length{};
    std::string charset;
};

struct FastTemplateDescriptor {
    std::uint32_t id{};
    std::string name;
    std::vector<FastFieldDescriptor> fields;
};

struct FeedEndpoint {
    std::string protocol;
    std::string source_ip;
    std::string multicast_group;
    std::uint16_t port{};
    std::string feed_id;
    bool is_tcp{};
};

struct FeedGroup {
    std::string name;
    std::string market_id;
    std::string feed_type;
    std::vector<FeedEndpoint> endpoints;
};

struct InspectionReport {
    std::string schema_version;
    std::string inspector_version;
    InputFileInfo templates_info;
    InputFileInfo configuration_info;
    std::vector<FastTemplateDescriptor> templates;
    std::vector<FeedGroup> feed_groups;
    std::vector<InspectionIssue> issues;
    std::string overall_status;  // "valid", "warning", "invalid"
};

WireType parse_wire_type(const std::string& name);
const char* wire_type_name(WireType wt);

}  // namespace moex_fast
