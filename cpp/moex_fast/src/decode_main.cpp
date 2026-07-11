#include "moex_fast/fast_compiler.hpp"
#include "moex_fast/fast_decoder.hpp"
#include "moex_fast/decoder_report.hpp"
#include "moex_fast/sha256.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <string>
#include <vector>
#include <cstring>

void print_usage() {
    std::cerr << "Usage: moex-fast-decode --templates <templates.xml> "
              << "--hex <one-message-hex>\n"
              << "       moex-fast-decode --templates <templates.xml> "
              << "--input <one-message.bin>\n"
              << "                      [--json-out <report.json>] [--exact]\n";
}

std::vector<std::uint8_t> hex_to_bytes(const std::string& hex) {
    std::vector<std::uint8_t> bytes;
    std::string clean_hex;
    for (char c : hex) {
        if (c == ' ' || c == '\n' || c == '\r' || c == '\t') continue;
        clean_hex += c;
    }
    if (clean_hex.size() % 2 != 0) return {};
    for (std::size_t i = 0; i < clean_hex.size(); i += 2) {
        int hi = 0, lo = 0;
        char c1 = clean_hex[i], c2 = clean_hex[i + 1];
        if (c1 >= '0' && c1 <= '9') hi = c1 - '0';
        else if (c1 >= 'a' && c1 <= 'f') hi = c1 - 'a' + 10;
        else if (c1 >= 'A' && c1 <= 'F') hi = c1 - 'A' + 10;
        else return {};
        if (c2 >= '0' && c2 <= '9') lo = c2 - '0';
        else if (c2 >= 'a' && c2 <= 'f') lo = c2 - 'a' + 10;
        else if (c2 >= 'A' && c2 <= 'F') lo = c2 - 'A' + 10;
        else return {};
        bytes.push_back(static_cast<std::uint8_t>(hi * 16 + lo));
    }
    return bytes;
}

int main(int argc, char* argv[]) {
    std::string templates_path;
    std::string hex_input;
    std::string file_input;
    std::string json_out;
    bool exact = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--templates" && i + 1 < argc) {
            templates_path = argv[++i];
        } else if (arg == "--hex" && i + 1 < argc) {
            hex_input = argv[++i];
        } else if (arg == "--input" && i + 1 < argc) {
            file_input = argv[++i];
        } else if (arg == "--json-out" && i + 1 < argc) {
            json_out = argv[++i];
        } else if (arg == "--exact") {
            exact = true;
        } else if (arg == "--help") {
            print_usage();
            return 0;
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            print_usage();
            return 1;
        }
    }

    if (templates_path.empty()) {
        std::cerr << "Error: --templates is required\n";
        print_usage();
        return 1;
    }

    if (hex_input.empty() && file_input.empty()) {
        std::cerr << "Error: exactly one of --hex or --input is required\n";
        print_usage();
        return 1;
    }

    if (!hex_input.empty() && !file_input.empty()) {
        std::cerr << "Error: only one of --hex or --input may be specified\n";
        print_usage();
        return 1;
    }

    // Compile templates
    auto compile_result = moex_fast::compile_templates(templates_path);
    if (!compile_result.ok) {
        std::cerr << "Template compilation failed:\n";
        for (const auto& issue : compile_result.issues) {
            std::cerr << "  [" << issue.code << "] " << issue.message << "\n";
        }
        return 1;
    }

    // Read input
    std::vector<std::uint8_t> input_bytes;
    if (!hex_input.empty()) {
        input_bytes = hex_to_bytes(hex_input);
        if (input_bytes.empty()) {
            std::cerr << "Error: invalid hex input\n";
            return 1;
        }
    } else {
        std::ifstream file(file_input, std::ios::binary);
        if (!file) {
            std::cerr << "Error: cannot open input file: " << file_input << "\n";
            return 1;
        }
        input_bytes.assign(std::istreambuf_iterator<char>(file),
                            std::istreambuf_iterator<char>());
    }

    // Compute input SHA-256
    std::string input_sha256 = moex_fast::compute_sha256_bytes(input_bytes.data(), input_bytes.size());

    // Decode
    moex_fast::DecoderSession session(compile_result.compiled);
    moex_fast::DecodeResult result;
    if (exact) {
        result = session.decode_exact(input_bytes.data(), input_bytes.size());
    } else {
        result = session.decode_one(input_bytes.data(), input_bytes.size());
    }

    // Generate report
    std::string text = moex_fast::decode_text_report(
        result, templates_path, compile_result.compiled.templates_sha256,
        input_bytes.size(), input_sha256);

    std::cout << text;

    if (!json_out.empty()) {
        std::string json = moex_fast::decode_json_report(
            result, templates_path, compile_result.compiled.templates_sha256,
            input_bytes.size(), input_sha256);
        std::ofstream ofs(json_out);
        if (!ofs) {
            std::cerr << "Error: cannot write JSON report to " << json_out << "\n";
            return 1;
        }
        ofs << json;
    }

    return result.status == moex_fast::DecodeStatus::Ok ? 0 : 1;
}
