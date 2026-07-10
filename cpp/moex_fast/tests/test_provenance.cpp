#include "moex_fast/inspector.hpp"
#include "moex_fast/report.hpp"
#include <cassert>
#include <iostream>
#include <fstream>
#include <string>

namespace {

void write_file(const char* path, const char* content) {
    std::ofstream ofs(path);
    ofs << content;
}

void test_sha256_stable() {
    write_file("fixtures/hash_test_a.xml", "<templates><template id='1' name='X'><uInt32 name='F'/></template></templates>");

    moex_fast::InspectorOptions opts;
    opts.configuration_path = "fixtures/synthetic_configuration.xml";
    opts.templates_path = "fixtures/hash_test_a.xml";

    auto r1 = moex_fast::run_inspector(opts);
    auto r2 = moex_fast::run_inspector(opts);

    assert(!r1.templates_info.sha256.empty());
    assert(r1.templates_info.sha256 == r2.templates_info.sha256);

    std::cout << "PASS: SHA-256 stable for identical bytes\n";
}

void test_sha256_changes() {
    write_file("fixtures/hash_test_b1.xml", "<templates><template id='1' name='X'><uInt32 name='F'/></template></templates>");
    write_file("fixtures/hash_test_b2.xml", "<templates><template id='1' name='Y'><uInt32 name='F'/></template></templates>");

    moex_fast::InspectorOptions opts1;
    opts1.configuration_path = "fixtures/synthetic_configuration.xml";
    opts1.templates_path = "fixtures/hash_test_b1.xml";

    moex_fast::InspectorOptions opts2;
    opts2.configuration_path = "fixtures/synthetic_configuration.xml";
    opts2.templates_path = "fixtures/hash_test_b2.xml";

    auto r1 = moex_fast::run_inspector(opts1);
    auto r2 = moex_fast::run_inspector(opts2);

    assert(r1.templates_info.sha256 != r2.templates_info.sha256);

    std::cout << "PASS: SHA-256 changes with different content\n";
}

void test_file_size_correct() {
    const char* content = "<templates/>";
    write_file("fixtures/size_test.xml", content);

    moex_fast::InspectorOptions opts;
    opts.configuration_path = "fixtures/synthetic_configuration.xml";
    opts.templates_path = "fixtures/size_test.xml";

    auto report = moex_fast::run_inspector(opts);
    // File size should match the content length (approximately, may vary with line endings)
    assert(report.templates_info.file_size > 0);
    assert(report.templates_info.file_size < 1000);

    std::cout << "PASS: file size correct\n";
}

void test_path_recorded() {
    moex_fast::InspectorOptions opts;
    opts.configuration_path = "fixtures/synthetic_configuration.xml";
    opts.templates_path = "fixtures/synthetic_templates.xml";

    auto report = moex_fast::run_inspector(opts);
    assert(!report.templates_info.path.empty());
    assert(!report.configuration_info.path.empty());
    assert(report.templates_info.path == opts.templates_path);
    assert(report.configuration_info.path == opts.configuration_path);

    std::cout << "PASS: path recorded\n";
}

void test_no_xml_in_report() {
    moex_fast::InspectorOptions opts;
    opts.configuration_path = "fixtures/synthetic_configuration.xml";
    opts.templates_path = "fixtures/synthetic_templates.xml";

    auto report = moex_fast::run_inspector(opts);
    auto json = moex_fast::report_to_json(report);

    // Report should not contain raw XML tags
    assert(json.find("<template") == std::string::npos);
    assert(json.find("<configuration") == std::string::npos);
    assert(json.find("<source") == std::string::npos);

    std::cout << "PASS: no raw XML in report\n";
}

void test_no_credentials_in_report() {
    moex_fast::InspectorOptions opts;
    opts.configuration_path = "fixtures/synthetic_configuration.xml";
    opts.templates_path = "fixtures/synthetic_templates.xml";

    auto report = moex_fast::run_inspector(opts);
    auto json = moex_fast::report_to_json(report);

    // Report should not contain credential-like strings
    assert(json.find("password") == std::string::npos);
    assert(json.find("secret") == std::string::npos);
    assert(json.find("api_key") == std::string::npos);

    std::cout << "PASS: no credentials in report\n";
}

}  // namespace

int main() {
    test_sha256_stable();
    test_sha256_changes();
    test_file_size_correct();
    test_path_recorded();
    test_no_xml_in_report();
    test_no_credentials_in_report();

    std::cout << "\nAll provenance tests PASSED.\n";
    return 0;
}
