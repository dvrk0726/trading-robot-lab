#include "moex_fast/xml_parser.hpp"
#include "test_helpers.hpp"
#include <iostream>
#include <fstream>

namespace {

const char* VALID_CONFIG = "fixtures/synthetic_configuration.xml";

void write_file(const char* path, const char* content) {
    std::ofstream ofs(path);
    ofs << content;
}

void test_valid_config_parse() {
    std::vector<moex_fast::FeedGroup> groups;
    std::vector<moex_fast::InspectionIssue> issues;
    bool ok = moex_fast::parse_configuration_xml(VALID_CONFIG, groups, issues);
    CHECK(ok);
    CHECK(groups.size() == 2);

    bool has_fut = false;
    bool has_orders = false;
    for (const auto& g : groups) {
        if (g.name == "FUT-INFO") has_fut = true;
        if (g.name == "ORDERS-LOG") has_orders = true;
    }
    CHECK(has_fut);
    CHECK(has_orders);

    TEST_PASS("valid configuration parse");
}

void test_feed_type_per_endpoint() {
    std::vector<moex_fast::FeedGroup> groups;
    std::vector<moex_fast::InspectionIssue> issues;
    moex_fast::parse_configuration_xml(VALID_CONFIG, groups, issues);

    const moex_fast::FeedGroup* ol = nullptr;
    for (const auto& g : groups) {
        if (g.name == "ORDERS-LOG") { ol = &g; break; }
    }
    CHECK(ol != nullptr);

    // Each endpoint should carry its own feed_type
    bool has_incremental = false;
    bool has_snapshot = false;
    bool has_hist = false;
    for (const auto& ep : ol->endpoints) {
        if (ep.feed_type == "Incremental") has_incremental = true;
        if (ep.feed_type == "Snapshot") has_snapshot = true;
        if (ep.feed_type == "Historical Replay") has_hist = true;
    }
    CHECK(has_incremental);
    CHECK(has_snapshot);
    CHECK(has_hist);

    TEST_PASS("feed type per endpoint");
}

void test_feed_endpoints_ab() {
    std::vector<moex_fast::FeedGroup> groups;
    std::vector<moex_fast::InspectionIssue> issues;
    moex_fast::parse_configuration_xml(VALID_CONFIG, groups, issues);

    const moex_fast::FeedGroup* ol = nullptr;
    for (const auto& g : groups) {
        if (g.name == "ORDERS-LOG") { ol = &g; break; }
    }
    CHECK(ol != nullptr);

    bool has_incr_a = false;
    bool has_incr_b = false;
    bool has_snap_a = false;
    bool has_snap_b = false;
    for (const auto& ep : ol->endpoints) {
        if (ep.feed_type == "Incremental" && ep.feed_id == "A") has_incr_a = true;
        if (ep.feed_type == "Incremental" && ep.feed_id == "B") has_incr_b = true;
        if (ep.feed_type == "Snapshot" && ep.feed_id == "A") has_snap_a = true;
        if (ep.feed_type == "Snapshot" && ep.feed_id == "B") has_snap_b = true;
    }
    CHECK(has_incr_a);
    CHECK(has_incr_b);
    CHECK(has_snap_a);
    CHECK(has_snap_b);

    TEST_PASS("feed endpoints A/B");
}

void test_endpoint_attributes() {
    std::vector<moex_fast::FeedGroup> groups;
    std::vector<moex_fast::InspectionIssue> issues;
    moex_fast::parse_configuration_xml(VALID_CONFIG, groups, issues);

    bool found = false;
    for (const auto& g : groups) {
        for (const auto& ep : g.endpoints) {
            if (ep.port > 0 && !ep.protocol.empty()) {
                CHECK(!ep.multicast_group.empty() || ep.is_tcp);
                found = true;
                break;
            }
        }
        if (found) break;
    }
    CHECK(found);

    TEST_PASS("endpoint attributes");
}

void test_malformed_config() {
    write_file("fixtures/bad_config.xml", "not xml");
    std::vector<moex_fast::FeedGroup> groups;
    std::vector<moex_fast::InspectionIssue> issues;
    bool ok = moex_fast::parse_configuration_xml("fixtures/bad_config.xml", groups, issues);
    CHECK(!ok);
    CHECK(!issues.empty());

    TEST_PASS("malformed configuration XML");
}

void test_missing_config_root() {
    write_file("fixtures/no_root_config.xml", "<foo><bar/></foo>");
    std::vector<moex_fast::FeedGroup> groups;
    std::vector<moex_fast::InspectionIssue> issues;
    bool ok = moex_fast::parse_configuration_xml("fixtures/no_root_config.xml", groups, issues);
    CHECK(!ok);

    TEST_PASS("missing configuration root");
}

void test_empty_config() {
    write_file("fixtures/empty_config.xml", "<configuration/>");
    std::vector<moex_fast::FeedGroup> groups;
    std::vector<moex_fast::InspectionIssue> issues;
    bool ok = moex_fast::parse_configuration_xml("fixtures/empty_config.xml", groups, issues);
    CHECK(ok);
    CHECK(groups.empty());

    TEST_PASS("empty configuration");
}

void test_config_file_not_found() {
    std::vector<moex_fast::FeedGroup> groups;
    std::vector<moex_fast::InspectionIssue> issues;
    bool ok = moex_fast::parse_configuration_xml("nonexistent.xml", groups, issues);
    CHECK(!ok);

    TEST_PASS("configuration file not found");
}

void test_udp_tcp_protocol() {
    std::vector<moex_fast::FeedGroup> groups;
    std::vector<moex_fast::InspectionIssue> issues;
    moex_fast::parse_configuration_xml(VALID_CONFIG, groups, issues);

    bool has_udp = false;
    bool has_tcp = false;
    for (const auto& g : groups) {
        for (const auto& ep : g.endpoints) {
            if (ep.protocol.find("UDP") != std::string::npos) has_udp = true;
            if (ep.protocol.find("TCP") != std::string::npos) has_tcp = true;
        }
    }
    CHECK(has_udp);
    CHECK(has_tcp);

    TEST_PASS("UDP/TCP protocol detection");
}

void test_port_zero_rejected() {
    write_file("fixtures/bad_port.xml",
        "<configuration>"
        "  <group name='TEST'>"
        "    <feed type='Incremental'>"
        "      <source ip='1.2.3.4' port='0' protocol='UDP/IP' multicastGroup='233.0.0.1' feed='A'/>"
        "    </feed>"
        "  </group>"
        "</configuration>");
    std::vector<moex_fast::FeedGroup> groups;
    std::vector<moex_fast::InspectionIssue> issues;
    moex_fast::parse_configuration_xml("fixtures/bad_port.xml", groups, issues);

    bool found_error = false;
    for (const auto& iss : issues) {
        if (iss.message.find("Port") != std::string::npos ||
            iss.message.find("port") != std::string::npos) {
            found_error = true;
        }
    }
    CHECK(found_error);

    TEST_PASS("port zero rejected");
}

void test_port_negative_rejected() {
    write_file("fixtures/neg_port.xml",
        "<configuration>"
        "  <group name='TEST'>"
        "    <feed type='Incremental'>"
        "      <source ip='1.2.3.4' port='-1' protocol='UDP/IP' multicastGroup='233.0.0.1' feed='A'/>"
        "    </feed>"
        "  </group>"
        "</configuration>");
    std::vector<moex_fast::FeedGroup> groups;
    std::vector<moex_fast::InspectionIssue> issues;
    moex_fast::parse_configuration_xml("fixtures/neg_port.xml", groups, issues);

    bool found_error = false;
    for (const auto& iss : issues) {
        if (iss.message.find("Non-numeric") != std::string::npos ||
            iss.message.find("port") != std::string::npos) {
            found_error = true;
        }
    }
    CHECK(found_error);

    TEST_PASS("port negative rejected");
}

void test_port_overflow_rejected() {
    write_file("fixtures/big_port.xml",
        "<configuration>"
        "  <group name='TEST'>"
        "    <feed type='Incremental'>"
        "      <source ip='1.2.3.4' port='99999' protocol='UDP/IP' multicastGroup='233.0.0.1' feed='A'/>"
        "    </feed>"
        "  </group>"
        "</configuration>");
    std::vector<moex_fast::FeedGroup> groups;
    std::vector<moex_fast::InspectionIssue> issues;
    moex_fast::parse_configuration_xml("fixtures/big_port.xml", groups, issues);

    bool found_error = false;
    for (const auto& iss : issues) {
        if (iss.message.find("out of range") != std::string::npos) {
            found_error = true;
        }
    }
    CHECK(found_error);

    TEST_PASS("port overflow rejected");
}

void test_port_nonnumeric_rejected() {
    write_file("fixtures/str_port.xml",
        "<configuration>"
        "  <group name='TEST'>"
        "    <feed type='Incremental'>"
        "      <source ip='1.2.3.4' port='abc' protocol='UDP/IP' multicastGroup='233.0.0.1' feed='A'/>"
        "    </feed>"
        "  </group>"
        "</configuration>");
    std::vector<moex_fast::FeedGroup> groups;
    std::vector<moex_fast::InspectionIssue> issues;
    moex_fast::parse_configuration_xml("fixtures/str_port.xml", groups, issues);

    bool found_error = false;
    for (const auto& iss : issues) {
        if (iss.message.find("Non-numeric") != std::string::npos) {
            found_error = true;
        }
    }
    CHECK(found_error);

    TEST_PASS("port non-numeric rejected");
}

void test_issue_source_configuration() {
    std::vector<moex_fast::FeedGroup> groups;
    std::vector<moex_fast::InspectionIssue> issues;
    moex_fast::parse_configuration_xml(VALID_CONFIG, groups, issues);

    for (const auto& iss : issues) {
        CHECK(iss.source == moex_fast::IssueSource::Configuration);
    }

    TEST_PASS("issue source is Configuration");
}

void test_tcp_historical_replay() {
    std::vector<moex_fast::FeedGroup> groups;
    std::vector<moex_fast::InspectionIssue> issues;
    moex_fast::parse_configuration_xml(VALID_CONFIG, groups, issues);

    const moex_fast::FeedGroup* ol = nullptr;
    for (const auto& g : groups) {
        if (g.name == "ORDERS-LOG") { ol = &g; break; }
    }
    CHECK(ol != nullptr);

    bool found_tcp_hist = false;
    for (const auto& ep : ol->endpoints) {
        if (ep.feed_type == "Historical Replay" && ep.is_tcp) {
            found_tcp_hist = true;
            CHECK(ep.port == 8022);
        }
    }
    CHECK(found_tcp_hist);

    TEST_PASS("TCP Historical Replay");
}

}  // namespace

int main() {
    test_valid_config_parse();
    test_feed_type_per_endpoint();
    test_feed_endpoints_ab();
    test_endpoint_attributes();
    test_malformed_config();
    test_missing_config_root();
    test_empty_config();
    test_config_file_not_found();
    test_udp_tcp_protocol();
    test_port_zero_rejected();
    test_port_negative_rejected();
    test_port_overflow_rejected();
    test_port_nonnumeric_rejected();
    test_issue_source_configuration();
    test_tcp_historical_replay();

    std::cout << "\nAll configuration parser tests PASSED.\n";
    return 0;
}
