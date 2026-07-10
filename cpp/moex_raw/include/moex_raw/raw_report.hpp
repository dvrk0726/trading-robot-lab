#pragma once
#include "moex_raw/raw_types.hpp"
#include <string>

namespace moex_raw {

// Generate JSON report for a segment inspection/replay.
std::string generate_json_report(const RawSegmentReport& report, bool pretty = true);

// Generate text report for console output.
std::string generate_text_report(const RawSegmentReport& report);

}  // namespace moex_raw
