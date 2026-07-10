#include "moex_fast/inspector.hpp"
#include "moex_fast/report.hpp"
#include "test_helpers.hpp"
#include <iostream>
#include <fstream>
#include <string>

namespace {

void test_sha256_stable() {
    write_temp_file("hash_test_a.xml", "<templates><template id='1' name='X'><uInt32 name='F'/></template></templates>");

    moex_fast::InspectorOptions opts;
    opts.configuration_path = "fixtures/synthetic_configuration.xml";
    opts.templates_path = temp_path("hash_test_a.xml");

    auto r1 = moex_fast::run_inspector(opts);
    auto r2 = moex_fast::run_inspector(opts);

    CHECK(!r1.templates_info.sha256.empty());
    CHECK(r1.templates_info.sha256 == r2.templates_info.sha256);

    TEST_PASS("SHA-256 stable for identical bytes");
}

void test_sha256_changes() {
    write_temp_file("hash_test_b1.xml", "<templates><template id='1' name='X'><uInt32 name='F'/></template></templates>");
    write_temp_file("hash_test_b2.xml", "<templates><template id='1' name='Y'><uInt32 name='F'/></template></templates>");

    moex_fast::InspectorOptions opts1;
    opts1.configuration_path = "fixtures/synthetic_configuration.xml";
    opts1.templates_path = temp_path("hash_test_b1.xml");

    moex_fast::InspectorOptions opts2;
    opts2.configuration_path = "fixtures/synthetic_configuration.xml";
    opts2.templates_path = temp_path("hash_test_b2.xml");

    auto r1 = moex_fast::run_inspector(opts1);
    auto r2 = moex_fast::run_inspector(opts2);

    CHECK(r1.templates_info.sha256 != r2.templates_info.sha256);

    TEST_PASS("SHA-256 changes with different content");
}

void test_file_size_correct() {
    const char* content = "<templates/>";
    write_temp_file("size_test.xml", content);

    moex_fast::InspectorOptions opts;
    opts.configuration_path = "fixtures/synthetic_configuration.xml";
    opts.templates_path = temp_path("size_test.xml");

    auto report = moex_fast::run_inspector(opts);
    CHECK(report.templates_info.file_size > 0);
    CHECK(report.templates_info.file_size < 1000);

    TEST_PASS("file size correct");
}

void test_path_recorded() {
    moex_fast::InspectorOptions opts;
    opts.configuration_path = "fixtures/synthetic_configuration.xml";
    opts.templates_path = "fixtures/synthetic_templates.xml";

    auto report = moex_fast::run_inspector(opts);
    CHECK(!report.templates_info.path.empty());
    CHECK(!report.configuration_info.path.empty());
    CHECK(report.templates_info.path == opts.templates_path);
    CHECK(report.configuration_info.path == opts.configuration_path);

    TEST_PASS("path recorded");
}

void test_no_xml_in_report() {
    moex_fast::InspectorOptions opts;
    opts.configuration_path = "fixtures/synthetic_configuration.xml";
    opts.templates_path = "fixtures/synthetic_templates.xml";

    auto report = moex_fast::run_inspector(opts);
    auto json = moex_fast::report_to_json(report);

    CHECK(json.find("<template") == std::string::npos);
    CHECK(json.find("<configuration") == std::string::npos);
    CHECK(json.find("<source") == std::string::npos);

    TEST_PASS("no raw XML in report");
}

void test_no_credentials_in_report() {
    moex_fast::InspectorOptions opts;
    opts.configuration_path = "fixtures/synthetic_configuration.xml";
    opts.templates_path = "fixtures/synthetic_templates.xml";

    auto report = moex_fast::run_inspector(opts);
    auto json = moex_fast::report_to_json(report);

    CHECK(json.find("password") == std::string::npos);
    CHECK(json.find("secret") == std::string::npos);
    CHECK(json.find("api_key") == std::string::npos);

    TEST_PASS("no credentials in report");
}

void test_independent_validation_status() {
    // A configuration-only error should not mark templates validation failed
    moex_fast::InspectorOptions opts;
    opts.configuration_path = "fixtures/nonexistent.xml";
    opts.templates_path = "fixtures/synthetic_templates.xml";

    auto report = moex_fast::run_inspector(opts);

    // Templates parsed OK
    CHECK(report.templates_info.parse_ok);
    // Configuration failed to parse
    CHECK(!report.configuration_info.parse_ok);

    // Templates validation should be independent of configuration errors
    CHECK(report.templates_info.validation_ok);

    TEST_PASS("independent validation status");
}

}  // namespace

int main() {
    test_sha256_stable();
    test_sha256_changes();
    test_file_size_correct();
    test_path_recorded();
    test_no_xml_in_report();
    test_no_credentials_in_report();
    test_independent_validation_status();

    std::cout << "\nAll provenance tests PASSED.\n";
    return 0;
}
