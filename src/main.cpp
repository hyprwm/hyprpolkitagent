#include "core/Agent.hpp"

#include <hyprutils/cli/ArgumentParser.hpp>

#include <memory>
#include <optional>
#include <print>
#include <span>
#include <string>
#include <vector>

int main(int argc, char* argv[]) {
    std::vector<const char*>        args(argv, argv + argc);
    Hyprutils::CLI::CArgumentParser parser{std::span<const char*>{args}};

    if (auto r = parser.registerStringOption("config", "c", "Path to a custom config file"); !r) {
        std::print(stderr, "hyprpolkitagent: internal error: failed to register option --config/-c: {}\n", r.error());
        return 1;
    }

    if (auto r = parser.parse(); !r) {
        std::print(stderr, "hyprpolkitagent: invalid arguments: {}\nusage: hyprpolkitagent [-c|--config <path>]\n", r.error());
        return 1;
    }

    std::optional<std::string> configPath;
    if (auto val = parser.getString("config")) {
        if (val->empty()) {
            std::print(stderr, "Error: config path must not be empty.\n");
            return 1;
        }
        configPath = std::string{*val};
    }

    g_pAgent = std::make_unique<CAgent>();
    return g_pAgent->start(configPath) ? 0 : 1;
}
