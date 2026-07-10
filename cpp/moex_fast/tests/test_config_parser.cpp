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
        "  <MarketDataGroup feedType='Incremental' marketID='D' label='TEST'>"
        "    <connections>"
        "      <connection>"
        "        <type>MarketData</type>"
        "        <protocol>UDP/IP</protocol>"
        "        <src-ip>1.2.3.4</src-ip>"
        "        <ip>233.0.0.1</ip>"
        "        <port>0</port>"
        "        <feed>A</feed>"
        "      </connection>"
        "    </connections>"
        "  </MarketDataGroup>"
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
        "  <MarketDataGroup feedType='Incremental' marketID='D' label='TEST'>"
        "    <connections>"
        "      <connection>"
        "        <type>MarketData</type>"
        "        <protocol>UDP/IP</protocol>"
        "        <src-ip>1.2.3.4</src-ip>"
        "        <ip>233.0.0.1</ip>"
        "        <port>-1</port>"
        "        <feed>A</feed>"
        "      </connection>"
        "    </connections>"
        "  </MarketDataGroup>"
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
        "  <MarketDataGroup feedType='Incremental' marketID='D' label='TEST'>"
        "    <connections>"
        "      <connection>"
        "        <type>MarketData</type>"
        "        <protocol>UDP/IP</protocol>"
        "        <src-ip>1.2.3.4</src-ip>"
        "        <ip>233.0.0.1</ip>"
        "        <port>99999</port>"
        "        <feed>A</feed>"
        "      </connection>"
        "    </connections>"
        "  </MarketDataGroup>"
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
        "  <MarketDataGroup feedType='Incremental' marketID='D' label='TEST'>"
        "    <connections>"
        "      <connection>"
        "        <type>MarketData</type>"
        "        <protocol>UDP/IP</protocol>"
        "        <src-ip>1.2.3.4</src-ip>"
        "        <ip>233.0.0.1</ip>"
        "        <port>abc</port>"
        "        <feed>A</feed>"
        "      </connection>"
        "    </connections>"
        "  </MarketDataGroup>"
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

    int tcp_hist_count = 0;
    for (const auto& ep : ol->endpoints) {
        if (ep.feed_type == "Historical Replay" && ep.is_tcp) {
            tcp_hist_count++;
            CHECK(ep.port == 8022);
        }
    }
    CHECK(tcp_hist_count == 2);

    TEST_PASS("TCP Historical Replay");
}

void test_unknown_protocol() {
    write_file("fixtures/unknown_proto.xml",
        "<configuration>"
        "  <MarketDataGroup feedType='Incremental' marketID='D' label='TEST'>"
        "    <connections>"
        "      <connection>"
        "        <type>MarketData</type>"
        "        <protocol>XYZ</protocol>"
        "        <src-ip>1.2.3.4</src-ip>"
        "        <ip>233.0.0.1</ip>"
        "        <port>1234</port>"
        "        <feed>A</feed>"
        "      </connection>"
        "    </connections>"
        "  </MarketDataGroup>"
        "</configuration>");
    std::vector<moex_fast::FeedGroup> groups;
    std::vector<moex_fast::InspectionIssue> issues;
    moex_fast::parse_configuration_xml("fixtures/unknown_proto.xml", groups, issues);

    bool found_error = false;
    for (const auto& iss : issues) {
        if (iss.message.find("Unknown protocol") != std::string::npos) {
            found_error = true;
        }
    }
    CHECK(found_error);

    TEST_PASS("unknown protocol detected");
}

void test_missing_udp_src_ip() {
    write_file("fixtures/no_src_ip.xml",
        "<configuration>"
        "  <MarketDataGroup feedType='Incremental' marketID='D' label='TEST'>"
        "    <connections>"
        "      <connection>"
        "        <type>MarketData</type>"
        "        <protocol>UDP/IP</protocol>"
        "        <ip>233.0.0.1</ip>"
        "        <port>1234</port>"
        "        <feed>A</feed>"
        "      </connection>"
        "    </connections>"
        "  </MarketDataGroup>"
        "</configuration>");
    std::vector<moex_fast::FeedGroup> groups;
    std::vector<moex_fast::InspectionIssue> issues;
    moex_fast::parse_configuration_xml("fixtures/no_src_ip.xml", groups, issues);

    bool found_error = false;
    for (const auto& iss : issues) {
        if (iss.message.find("src-ip") != std::string::npos) {
            found_error = true;
        }
    }
    CHECK(found_error);

    TEST_PASS("missing UDP src-ip detected");
}

void test_missing_udp_feed() {
    write_file("fixtures/no_feed.xml",
        "<configuration>"
        "  <MarketDataGroup feedType='Incremental' marketID='D' label='TEST'>"
        "    <connections>"
        "      <connection>"
        "        <type>MarketData</type>"
        "        <protocol>UDP/IP</protocol>"
        "        <src-ip>1.2.3.4</src-ip>"
        "        <ip>233.0.0.1</ip>"
        "        <port>1234</port>"
        "      </connection>"
        "    </connections>"
        "  </MarketDataGroup>"
        "</configuration>");
    std::vector<moex_fast::FeedGroup> groups;
    std::vector<moex_fast::InspectionIssue> issues;
    moex_fast::parse_configuration_xml("fixtures/no_feed.xml", groups, issues);

    bool found_error = false;
    for (const auto& iss : issues) {
        if (iss.message.find("feed") != std::string::npos &&
            iss.message.find("missing") != std::string::npos) {
            found_error = true;
        }
    }
    CHECK(found_error);

    TEST_PASS("missing UDP feed detected");
}

void test_tcp_no_feed_ok() {
    write_file("fixtures/tcp_no_feed.xml",
        "<configuration>"
        "  <MarketDataGroup feedType='Historical Replay' marketID='D' label='TEST'>"
        "    <connections>"
        "      <connection>"
        "        <type>MarketData</type>"
        "        <protocol>TCP/IP</protocol>"
        "        <src-ip>1.2.3.4</src-ip>"
        "        <ip>192.168.1.1</ip>"
        "        <port>8022</port>"
        "      </connection>"
        "    </connections>"
        "  </MarketDataGroup>"
        "</configuration>");
    std::vector<moex_fast::FeedGroup> groups;
    std::vector<moex_fast::InspectionIssue> issues;
    moex_fast::parse_configuration_xml("fixtures/tcp_no_feed.xml", groups, issues);

    // Should NOT have a "missing feed" error for TCP
    bool found_feed_error = false;
    for (const auto& iss : issues) {
        if (iss.message.find("feed") != std::string::npos &&
            iss.message.find("missing") != std::string::npos) {
            found_feed_error = true;
        }
    }
    CHECK(!found_feed_error);

    TEST_PASS("TCP no feed is OK");
}

void test_true_duplicate_endpoint() {
    write_file("fixtures/dup_endpoint.xml",
        "<configuration>"
        "  <MarketDataGroup feedType='Incremental' marketID='D' label='TEST'>"
        "    <connections>"
        "      <connection>"
        "        <type>MarketData</type>"
        "        <protocol>UDP/IP</protocol>"
        "        <src-ip>1.2.3.4</src-ip>"
        "        <ip>233.0.0.1</ip>"
        "        <port>1234</port>"
        "        <feed>A</feed>"
        "      </connection>"
        "      <connection>"
        "        <type>MarketData</type>"
        "        <protocol>UDP/IP</protocol>"
        "        <src-ip>1.2.3.4</src-ip>"
        "        <ip>233.0.0.1</ip>"
        "        <port>1234</port>"
        "        <feed>A</feed>"
        "      </connection>"
        "    </connections>"
        "  </MarketDataGroup>"
        "</configuration>");
    std::vector<moex_fast::FeedGroup> groups;
    std::vector<moex_fast::InspectionIssue> issues;
    moex_fast::parse_configuration_xml("fixtures/dup_endpoint.xml", groups, issues);

    // The parser itself doesn't detect duplicates (inspector does),
    // but we verify both endpoints are parsed
    CHECK(groups.size() == 1);
    CHECK(groups[0].endpoints.size() == 2);

    TEST_PASS("true duplicate endpoint");
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
    test_unknown_protocol();
    test_missing_udp_src_ip();
    test_missing_udp_feed();
    test_tcp_no_feed_ok();
    test_true_duplicate_endpoint();

    std::cout << "\nAll configuration parser tests PASSED.\n";
    return 0;
}
