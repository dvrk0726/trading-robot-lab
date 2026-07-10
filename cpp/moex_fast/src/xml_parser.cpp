#include "moex_fast/xml_parser.hpp"
#include <pugixml.hpp>
#include <algorithm>
#include <set>

namespace moex_fast {

namespace {

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

FastFieldDescriptor parse_field(pugi::xml_node node, std::uint32_t& field_order) {
    FastFieldDescriptor fd;
    fd.order = field_order++;
    fd.name = node.attribute("name").as_string("");
    fd.wire_type = element_to_wire_type(node.name());

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

    // Check for length child (for sequences)
    auto length = node.child("length");
    if (length) {
        fd.is_sequence_length = true;
    }

    return fd;
}

void parse_template_fields(pugi::xml_node tmpl_node,
                           std::vector<FastFieldDescriptor>& fields) {
    std::uint32_t order = 0;
    for (auto child : tmpl_node.children()) {
        std::string name(child.name());
        if (name == "string" || name == "uInt32" || name == "uint32" ||
            name == "uInt64" || name == "uint64" || name == "int32" ||
            name == "int64" || name == "decimal" || name == "unicode") {
            fields.push_back(parse_field(child, order));
        } else if (name == "sequence") {
            auto fd = parse_field(child, order);
            fd.wire_type = WireType::Sequence;
            fields.push_back(fd);
            // Parse nested sequence fields
            std::uint32_t nested_order = 0;
            for (auto seq_child : child.children()) {
                std::string seq_name(seq_child.name());
                if (seq_name == "length") {
                    auto lfd = parse_field(seq_child, nested_order);
                    lfd.is_sequence_length = true;
                    fields.push_back(lfd);
                } else if (seq_name == "string" || seq_name == "uInt32" ||
                           seq_name == "uint32" || seq_name == "uInt64" ||
                           seq_name == "uint64" || seq_name == "int32" ||
                           seq_name == "int64" || seq_name == "decimal" ||
                           seq_name == "unicode") {
                    fields.push_back(parse_field(seq_child, nested_order));
                }
            }
        }
    }
}

}  // namespace

bool parse_templates_xml(
    const std::string& path,
    std::vector<FastTemplateDescriptor>& out,
    std::vector<InspectionIssue>& issues) {

    pugi::xml_document doc;
    auto result = doc.load_file(path.c_str());
    if (!result) {
        issues.push_back({Severity::Error,
            std::string("Failed to parse templates XML: ") + result.description()});
        return false;
    }

    auto root = doc.child("templates");
    if (!root) {
        // Try alternate root names
        root = doc.child("TemplateConfiguration");
        if (!root) {
            issues.push_back({Severity::Error,
                "Missing root element <templates> in templates XML"});
            return false;
        }
    }

    std::set<std::uint32_t> seen_ids;

    for (auto tmpl : root.children("template")) {
        FastTemplateDescriptor td;

        auto id_attr = tmpl.attribute("id");
        if (!id_attr) {
            issues.push_back({Severity::Error,
                "Template missing 'id' attribute"});
            continue;
        }

        std::string id_str = id_attr.as_string("");
        bool is_numeric = !id_str.empty() &&
            std::all_of(id_str.begin(), id_str.end(), ::isdigit);
        if (!is_numeric) {
            issues.push_back({Severity::Error,
                "Template has non-numeric id: " + id_str});
            continue;
        }

        td.id = static_cast<std::uint32_t>(id_attr.as_int(0));
        td.name = tmpl.attribute("name").as_string("");

        if (td.name.empty()) {
            issues.push_back({Severity::Warning,
                "Template " + std::to_string(td.id) + " has empty name"});
        }

        if (seen_ids.count(td.id)) {
            issues.push_back({Severity::Error,
                "Duplicate template id: " + std::to_string(td.id)});
            continue;
        }
        seen_ids.insert(td.id);

        parse_template_fields(tmpl, td.fields);
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
        issues.push_back({Severity::Error,
            std::string("Failed to parse configuration XML: ") + result.description()});
        return false;
    }

    auto root = doc.child("configuration");
    if (!root) {
        issues.push_back({Severity::Error,
            "Missing root element <configuration> in configuration XML"});
        return false;
    }

    for (auto group : root.children("group")) {
        FeedGroup fg;
        fg.name = group.attribute("name").as_string("");
        fg.market_id = group.attribute("marketId").as_string("");

        if (fg.name.empty()) {
            issues.push_back({Severity::Warning, "Feed group missing name"});
        }

        for (auto feed : group.children("feed")) {
            std::string feed_type = feed.attribute("type").as_string("");

            for (auto source : feed.children("source")) {
                FeedEndpoint ep;
                ep.protocol = source.attribute("protocol").as_string("");
                ep.source_ip = source.attribute("ip").as_string("");
                ep.multicast_group = source.attribute("multicastGroup").as_string("");
                ep.port = static_cast<std::uint16_t>(
                    source.attribute("port").as_int(0));
                ep.feed_id = source.attribute("feed").as_string("");
                ep.is_tcp = (ep.protocol.find("TCP") != std::string::npos);

                fg.feed_type = feed_type;
                fg.endpoints.push_back(std::move(ep));
            }
        }

        out.push_back(std::move(fg));
    }

    return true;
}

}  // namespace moex_fast
