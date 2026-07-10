#ifdef _MSC_VER
#pragma warning(disable : 4189 4101)
#endif

#include "moex_raw/raw_report.hpp"
#include "moex_raw/raw_types.hpp"
#include <cassert>
#include <iostream>
#include <sstream>

int main() {
    using namespace moex_raw;

    // Valid JSON with required fields
    {
        RawSegmentReport report;
        report.operation = "inspect";
        report.session_id_hex = "0123456789abcdef0123456789abcdef";
        report.feed_group = "ORDERS-LOG";
        report.endpoint_role = "Incremental";
        report.source_label = "test";
        report.stream_key = "0123456789abcdef0123456789abcdef_src0000000000000001_ch0000000000000001";
        report.segment_indexes = {0, 1};
        report.segment_sizes = {1024, 2048};
        report.content_sha256 = "abcdef";
        report.file_sha256 = "123456";
        report.record_count = 100;
        report.total_payload_bytes = 6400;
        report.first_capture_index = 0;
        report.last_capture_index = 99;
        report.replay_sha256 = "fedcba";
        report.overall_status = "valid";

        auto json = generate_json_report(report);
        assert(json.find("\"schema_version\"") != std::string::npos);
        assert(json.find("\"tool_version\"") != std::string::npos);
        assert(json.find("\"operation\"") != std::string::npos);
        assert(json.find("\"session_id\"") != std::string::npos);
        assert(json.find("\"feed_group\"") != std::string::npos);
        assert(json.find("\"endpoint_role\"") != std::string::npos);
        assert(json.find("\"stream_key\"") != std::string::npos);
        assert(json.find("\"segment_indexes\"") != std::string::npos);
        assert(json.find("\"record_count\"") != std::string::npos);
        assert(json.find("\"replay_sha256\"") != std::string::npos);
        assert(json.find("\"overall_status\"") != std::string::npos);
        assert(json.find("\"issues\"") != std::string::npos);
    }

    // Issues in JSON
    {
        RawSegmentReport report;
        report.issues.push_back({ValidationSeverity::Error, "TEST_ERR", "test error message"});
        report.issues.push_back({ValidationSeverity::Warning, "TEST_WARN", "test warning"});
        report.overall_status = "invalid";

        auto json = generate_json_report(report);
        assert(json.find("TEST_ERR") != std::string::npos);
        assert(json.find("TEST_WARN") != std::string::npos);
        assert(json.find("error") != std::string::npos);
        assert(json.find("warning") != std::string::npos);
    }

    // Text report
    {
        RawSegmentReport report;
        report.operation = "inspect";
        report.overall_status = "valid";
        report.record_count = 42;

        auto text = generate_text_report(report);
        assert(text.find("inspect") != std::string::npos);
        assert(text.find("valid") != std::string::npos);
        assert(text.find("42") != std::string::npos);
    }

    // No payload bytes in report
    {
        RawSegmentReport report;
        report.operation = "inspect";
        report.overall_status = "valid";

        auto json = generate_json_report(report);
        // Should not contain raw packet dumps
        assert(json.find("payload_bytes") == std::string::npos);
        assert(json.find("raw_dump") == std::string::npos);
    }

    // Deterministic JSON key order
    {
        RawSegmentReport report;
        report.schema_version = "1.0";
        report.tool_version = "0.1.0";
        report.operation = "inspect";
        report.overall_status = "valid";

        auto json1 = generate_json_report(report);
        auto json2 = generate_json_report(report);
        assert(json1 == json2);
    }

    std::cout << "test_json_report: ALL PASSED\n";
    return 0;
}
