#pragma once
#include "inspect_types.hpp"
#include <string>
#include <vector>

namespace moex_fast {

// Parse templates.xml into normalized template descriptors.
// Returns false on fatal parse error; issues are appended to `issues`.
bool parse_templates_xml(
    const std::string& path,
    std::vector<FastTemplateDescriptor>& out,
    std::vector<InspectionIssue>& issues);

// Parse configuration.xml into normalized feed groups.
// Returns false on fatal parse error; issues are appended to `issues`.
bool parse_configuration_xml(
    const std::string& path,
    std::vector<FeedGroup>& out,
    std::vector<InspectionIssue>& issues);

}  // namespace moex_fast
