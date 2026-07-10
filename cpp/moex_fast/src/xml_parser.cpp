#include "moex_fast/xml_parser.hpp"
#include <pugixml.hpp>
#include <algorithm>
#include <set>
#include <cctype>
#include <cstdlib>

namespace moex_fast {

namespace {

// Known FAST field element names that produce a field descriptor.
bool is_known_field_element(const std::string& n) {
    return n == "string" || n == "uInt32" || n == "uint32" ||
           n == "uInt64" || n == "uint64" || n == "int32" ||
           n == "int64" || n == "decimal" || n == "unicode" ||
           n == "sequence" || n == "length";
}

// Known FAST operator/container child elements that are valid but not fields.
bool is_known_operator_element(const std::string& n) {
    return n == "constant" || n == "default" || n == "copy" ||
           n == "increment" || n == "delta" || n == "tail" ||
           n == "exponent" || n == "mantissa" ||
           n == "padding" || n == "groupRef" || n == "typeRef";
}

WireType element_to_wire_type(const char* name) {
    if (!name) return WireType::Unknown;
    std::string n(name);
    if (n == "uInt32" || n == "uint32") return WireType::uInt32;
    if (n == "uInt64" || n == "uint64") return WireType::uInt64;
    if (n == "int32") return WireType::Int32;
    if (n == "int64") return WireType::Int64;
    if (n == "string") return WireType::AsciiString;
    if (n == "unicode") return WireType::UnicodeString;
    if (n == "decimal") return WireType::Decimal;
    if (n == "sequence") return WireType::Sequence;
    return WireType::Unknown;
}

FastFieldDescriptor parse_field(pugi::xml_node node, std::uint32_t& field_order,
                                 const std::string& parent_sequence) {
    FastFieldDescriptor fd;
    fd.order = field_order++;
    fd.name = node.attribute("name").as_string("");
    fd.wire_type = element_to_wire_type(node.name());
    fd.parent_sequence = parent_sequence;

    auto fix_attr = node.attribute("id");
    if (fix_attr) {
        fd.has_fix_tag = true;
        fd.fix_tag = fix_attr.as_int(0);
    }

    auto pres = node.attribute("presence");
    fd.is_mandatory = (pres && std::string(pres.as_string("")) == "mandatory");

    auto charset = node.attribute("charset");
    if (charset) {
        fd.charset = charset.as_string("");
    }

    // Check for constant child
    auto constant = node.child("constant");
    if (constant) {
        fd.is_constant = true;
        fd.constant_value = constant.text().as_string("");
        if (fd.constant_value.empty()) {
            auto val_attr = constant.attribute("value");
            if (val_attr) fd.constant_value = val_attr.as_string("");
        }
    }

    // A <length> element is the sequence-length field.
    if (std::string(node.name()) == "length") {
        fd.is_sequence_length = true;
    }

    return fd;
}

// Recursively parse template fields, preserving global order and sequence structure.
void parse_template_fields(pugi::xml_node parent_node,
                           std::vector<FastFieldDescriptor>& fields,
                           std::vector<InspectionIssue>& issues,
                           std::uint32_t& field_order,
                           const std::string& parent_sequence) {
    for (auto child : parent_node.children()) {
        std::string name(child.name());

        if (is_known_field_element(name)) {
            if (name == "sequence") {
                // Parse the sequence element itself as a field
                auto fd = parse_field(child, field_order, parent_sequence);
                fd.wire_type = WireType::Sequence;
                std::string seq_name = fd.name;
                fields.push_back(fd);
                // Parse children of the sequence with the sequence name as parent.
                // Field order continues globally (no reset).
                parse_template_fields(child, fields, issues, field_order, seq_name);
            } else {
                fields.push_back(parse_field(child, field_order, parent_sequence));
            }
        } else if (is_known_operator_element(name)) {
            // Operators like constant, default, copy, etc. are valid children
            // but are not standalone field elements at the template level.
            // They are handled inside parse_field for the parent field.
            // If they appear at the top level without a parent field, report.
            if (parent_sequence.empty() && name != "length" &&
                name != "constant" && name != "default") {
                issues.push_back({Severity::Warning, IssueSource::Template,
                    "Top-level FAST operator element <" + name +
                    "> without parent field"});
            }
        } else {
            // Truly unknown element — report instead of silently discarding
            issues.push_back({Severity::Warning, IssueSource::Template,
                "Unknown XML element <" + name + "> in " +
                (parent_sequence.empty() ? "template" : "sequence " + parent_sequence)});
        }
    }
}

// Parse port text strictly: reject non-numeric, zero, negative, >65535.
bool parse_port_strict(const std::string& text, std::uint16_t& out_port,
                       std::vector<InspectionIssue>& issues,
                       const std::string& context) {
    if (text.empty()) {
        issues.push_back({Severity::Error, IssueSource::Configuration,
            "Missing port attribute in " + context});
        return false;
    }
    // Check all characters are digits
    for (char c : text) {
        if (!std::isdigit(static_cast<unsigned char>(c))) {
            issues.push_back({Severity::Error, IssueSource::Configuration,
                "Non-numeric port value '" + text + "' in " + context});
            return false;
        }
    }
    // Parse as long to catch overflow
    char* end = nullptr;
    long val = std::strtol(text.c_str(), &end, 10);
    if (val <= 0 || val > 65535) {
        issues.push_back({Severity::Error, IssueSource::Configuration,
            "Port value " + text + " out of range [1, 65535] in " + context});
        return false;
    }
    out_port = static_cast<std::uint16_t>(val);
    return true;
}

}  // namespace

bool parse_templates_xml(
    const std::string& path,
    std::vector<FastTemplateDescriptor>& out,
    std::vector<InspectionIssue>& issues) {

    pugi::xml_document doc;
    auto result = doc.load_file(path.c_str());
    if (!result) {
        issues.push_back({Severity::Error, IssueSource::Template,
            std::string("Failed to parse templates XML: ") + result.description()});
        return false;
    }

    auto root = doc.child("templates");
    if (!root) {
        root = doc.child("TemplateConfiguration");
        if (!root) {
            issues.push_back({Severity::Error, IssueSource::Template,
                "Missing root element <templates> in templates XML"});
            return false;
        }
    }

    std::set<std::uint32_t> seen_ids;

    for (auto tmpl : root.children("template")) {
        FastTemplateDescriptor td;

        auto id_attr = tmpl.attribute("id");
        if (!id_attr) {
            issues.push_back({Severity::Error, IssueSource::Template,
                "Template missing 'id' attribute"});
            continue;
        }

        std::string id_str = id_attr.as_string("");
        bool is_numeric = !id_str.empty() &&
            std::all_of(id_str.begin(), id_str.end(), ::isdigit);
        if (!is_numeric) {
            issues.push_back({Severity::Error, IssueSource::Template,
                "Template has non-numeric id: " + id_str});
            continue;
        }

        td.id = static_cast<std::uint32_t>(id_attr.as_int(0));
        td.name = tmpl.attribute("name").as_string("");

        if (td.name.empty()) {
            issues.push_back({Severity::Warning, IssueSource::Template,
                "Template " + std::to_string(td.id) + " has empty name"});
        }

        if (seen_ids.count(td.id)) {
            issues.push_back({Severity::Error, IssueSource::Template,
                "Duplicate template id: " + std::to_string(td.id)});
            continue;
        }
        seen_ids.insert(td.id);

        std::uint32_t field_order = 0;
        parse_template_fields(tmpl, td.fields, issues, field_order, "");
        out.push_back(std::move(td));
    }

    return true;
}

bool parse_configuration_xml(
    const std::string& path,
    std::vector<FeedGroup>& out,
    std::vector<InspectionIssue>& issues) {

    pugi::xml_document doc;
    auto result = doc.load_file(path.c_str());
    if (!result) {
        issues.push_back({Severity::Error, IssueSource::Configuration,
            std::string("Failed to parse configuration XML: ") + result.description()});
        return false;
    }

    auto root = doc.child("configuration");
    if (!root) {
        issues.push_back({Severity::Error, IssueSource::Configuration,
            "Missing root element <configuration> in configuration XML"});
        return false;
    }

    for (auto group : root.children("group")) {
        FeedGroup fg;
        fg.name = group.attribute("name").as_string("");
        fg.market_id = group.attribute("marketId").as_string("");

        if (fg.name.empty()) {
            issues.push_back({Severity::Warning, IssueSource::Configuration,
                "Feed group missing name"});
        }

        for (auto feed : group.children("feed")) {
            std::string feed_type = feed.attribute("type").as_string("");
            if (feed_type.empty()) {
                issues.push_back({Severity::Warning, IssueSource::Configuration,
                    "Feed element missing 'type' attribute in group " + fg.name});
            }

            for (auto source : feed.children("source")) {
                FeedEndpoint ep;
                ep.feed_type = feed_type;  // role per endpoint, not per group

                ep.protocol = source.attribute("protocol").as_string("");
                ep.source_ip = source.attribute("ip").as_string("");
                ep.multicast_group = source.attribute("multicastGroup").as_string("");
                ep.feed_id = source.attribute("feed").as_string("");

                // Validate protocol
                if (ep.protocol.empty()) {
                    issues.push_back({Severity::Error, IssueSource::Configuration,
                        "Source missing 'protocol' attribute in " + fg.name});
                }

                // Strict port parsing
                std::string port_text = source.attribute("port").as_string("");
                std::string ctx = fg.name + " endpoint (feed=" + ep.feed_id + ")";
                parse_port_strict(port_text, ep.port, issues, ctx);

                ep.is_tcp = (ep.protocol.find("TCP") != std::string::npos);

                fg.endpoints.push_back(std::move(ep));
            }
        }

        out.push_back(std::move(fg));
    }

    return true;
}

}  // namespace moex_fast
