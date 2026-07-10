#include "moex_fast/inspector.hpp"
#include "moex_fast/report.hpp"
#include <cstring>
#include <iostream>

static void print_usage() {
    std::cout <<
        "Usage: moex-fast-inspect \\\n"
        "  --configuration <path/configuration.xml> \\\n"
        "  --templates <path/templates.xml> \\\n"
        "  [--json-out <path/report.json>] \\\n"
        "  [--profile auto|spectra-1.29|spectra-1.30] \\\n"
        "  [--strict]\n"
        "\n"
        "Inspect MOEX SPECTRA FAST configuration and template XML files.\n"
        "Produces a human-readable summary and optional deterministic JSON report.\n"
        "\n"
        "No network access is performed.\n";
}

int main(int argc, char* argv[]) {
    moex_fast::InspectorOptions opts;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            print_usage();
            return 0;
        }
        if (std::strcmp(argv[i], "--configuration") == 0) {
            if (i + 1 >= argc) {
                std::cerr << "Error: --configuration requires a path argument\n";
                return 1;
            }
            opts.configuration_path = argv[++i];
        } else if (std::strcmp(argv[i], "--templates") == 0) {
            if (i + 1 >= argc) {
                std::cerr << "Error: --templates requires a path argument\n";
                return 1;
            }
            opts.templates_path = argv[++i];
        } else if (std::strcmp(argv[i], "--json-out") == 0) {
            if (i + 1 >= argc) {
                std::cerr << "Error: --json-out requires a path argument\n";
                return 1;
            }
            opts.json_out_path = argv[++i];
        } else if (std::strcmp(argv[i], "--profile") == 0) {
            if (i + 1 >= argc) {
                std::cerr << "Error: --profile requires an argument (auto|spectra-1.29|spectra-1.30)\n";
                return 1;
            }
            opts.profile = argv[++i];
            if (opts.profile != "auto" && opts.profile != "spectra-1.29" && opts.profile != "spectra-1.30") {
                std::cerr << "Error: unsupported --profile value '" << opts.profile
                          << "'. Allowed: auto, spectra-1.29, spectra-1.30\n";
                return 1;
            }
        } else if (std::strcmp(argv[i], "--strict") == 0) {
            opts.strict = true;
        } else {
            std::cerr << "Error: unknown argument: " << argv[i] << "\n";
            print_usage();
            return 1;
        }
    }

    if (opts.configuration_path.empty()) {
        std::cerr << "Error: --configuration is required\n";
        return 1;
    }
    if (opts.templates_path.empty()) {
        std::cerr << "Error: --templates is required\n";
        return 1;
    }

    auto report = moex_fast::run_inspector(opts);

    // Print human-readable summary
    std::cout << moex_fast::report_to_text(report);

    // Write JSON if requested
    if (!opts.json_out_path.empty()) {
        auto err = moex_fast::write_json_report(report, opts.json_out_path);
        if (!err.empty()) {
            std::cerr << "Error writing JSON: " << err << "\n";
            return 1;
        }
        std::cout << "\nJSON report written to: " << opts.json_out_path << "\n";
    }

    return report.overall_status == "invalid" ? 1 : 0;
}
