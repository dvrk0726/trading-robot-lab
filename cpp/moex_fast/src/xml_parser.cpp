#include "moex_fast/xml_parser.hpp"
#include <pugixml.hpp>
#include <algorithm>
#include <map>
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

                // Check for unsupported operators in sequence field
                for (auto op : child.children()) {
                    std::string on(op.name());
                    if (on != "constant" && is_known_operator_element(on)) {
                        issues.push_back({Severity::Warning, IssueSource::Template,
                            "Unsupported FAST operator '" + on + "' in field '" + fd.name + "'"});
                    }
                }

                // Parse children of the sequence with the sequence name as parent.
                // Field order continues globally (no reset).
                parse_template_fields(child, fields, issues, field_order, seq_name);
            } else {
                auto fd = parse_field(child, field_order, parent_sequence);
                // Check for unsupported operators in non-sequence field
                for (auto op : child.children()) {
                    std::string on(op.name());
                    if (on != "constant" && is_known_operator_element(on)) {
                        issues.push_back({Severity::Warning, IssueSource::Template,
                            "Unsupported FAST operator '" + on + "' in field '" + fd.name + "'"});
                    }
                }
                fields.push_back(std::move(fd));
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
            "Missing port value in " + context});
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

// Read text content of a child element, return empty string if absent.
std::string child_text(pugi::xml_node node, const char* child_name) {
    auto c = node.child(child_name);
    return c ? std::string(c.text().as_string("")) : std::string();
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

    // Merge FeedGroups by label attribute.
    // Map label -> index in out vector.
    std::map<std::string, std::size_t> label_index;

    for (auto mdg : root.children("MarketDataGroup")) {
        std::string feed_type = mdg.attribute("feedType").as_string("");
        std::string market_id = mdg.attribute("marketID").as_string("");
        std::string label = mdg.attribute("label").as_string("");

        if (label.empty()) {
            issues.push_back({Severity::Warning, IssueSource::Configuration,
                "MarketDataGroup missing 'label' attribute"});
        }

        // Find or create FeedGroup for this label
        auto it = label_index.find(label);
        std::size_t group_idx;
        if (it != label_index.end()) {
            group_idx = it->second;
        } else {
            group_idx = out.size();
            FeedGroup fg;
            fg.name = label;
            fg.market_id = market_id;
            out.push_back(std::move(fg));
            label_index[label] = group_idx;
        }

        auto conns = mdg.child("connections");
        if (!conns) {
            issues.push_back({Severity::Warning, IssueSource::Configuration,
                "MarketDataGroup '" + label + "' missing <connections> element"});
            continue;
        }

        for (auto conn : conns.children("connection")) {
            std::string protocol = child_text(conn, "protocol");
            std::string src_ip = child_text(conn, "src-ip");
            std::string conn_type = child_text(conn, "type");
            std::string feed = child_text(conn, "feed");
            std::string port_text = child_text(conn, "port");

            bool is_tcp = (protocol.find("TCP") != std::string::npos);

            // Validate protocol
            if (protocol.empty()) {
                issues.push_back({Severity::Error, IssueSource::Configuration,
                    "Connection missing <protocol> in group '" + label + "'"});
            } else if (protocol != "UDP/IP" && protocol != "TCP/IP") {
                issues.push_back({Severity::Error, IssueSource::Configuration,
                    "Unknown protocol '" + protocol + "' in group '" + label + "'"});
            }

            // Validate required UDP fields
            if (!is_tcp) {
                if (src_ip.empty()) {
                    issues.push_back({Severity::Error, IssueSource::Configuration,
                        "UDP connection missing <src-ip> in group '" + label + "'"});
                }
                if (feed.empty()) {
                    issues.push_back({Severity::Error, IssueSource::Configuration,
                        "UDP connection missing <feed> in group '" + label + "'"});
                }
            }

            // Parse port
            std::uint16_t port = 0;
            std::string ctx = label + " connection (feed=" + feed + ")";
            parse_port_strict(port_text, port, issues, ctx);

            // Collect all <ip> elements — one endpoint per ip
            std::vector<std::string> ips;
            for (auto ip_node : conn.children("ip")) {
                std::string ip_val = ip_node.text().as_string("");
                if (!ip_val.empty()) {
                    ips.push_back(ip_val);
                }
            }

            if (ips.empty()) {
                issues.push_back({Severity::Error, IssueSource::Configuration,
                    "Connection missing <ip> element in group '" + label + "'"});
            }

            for (const auto& ip : ips) {
                FeedEndpoint ep;
                ep.protocol = protocol;
                ep.source_ip = src_ip;
                ep.multicast_group = ip;
                ep.port = port;
                ep.feed_id = feed;
                ep.is_tcp = is_tcp;
                ep.feed_type = feed_type;
                ep.connection_type = conn_type;
                out[group_idx].endpoints.push_back(std::move(ep));
            }
        }
    }

    return true;
}

}  // namespace moex_fast
