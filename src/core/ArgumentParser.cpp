#include "ArgumentParser.hpp"

#include <print>

bool parseArguments(int argc, char* argv[], std::optional<std::string>& configPath) {
    configPath.reset();
    for (int i = 1; i < argc; ++i) {
        const std::string_view          arg = argv[i];
        std::optional<std::string_view> value;

        if (arg == "-c" || arg == "--config") {
            if (i + 1 >= argc) {
                std::print(stderr, "Error: {} Requires a path.\n", arg);
                return false;
            }

            value = argv[++i];
        } else if (arg.starts_with("--config=")) {
            value = arg.substr(sizeof("--config=") - 1);
        } else {
            std::print(stderr, "Unknown option: {}\n", arg);
            return false;
        }

        if (value->empty()) {
            std::print(stderr, "Error: config path must not be empty.\n");
            return false;
        }

        configPath = std::string{*value};
    }

    return true;
}
