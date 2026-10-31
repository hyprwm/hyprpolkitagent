#include "core/Agent.hpp"
#include "core/ArgumentParser.hpp"

#include <optional>
#include <string>

int main(int argc, char* argv[]) {
    std::optional<std::string> customConfigPath;

    if (!parseArguments(argc, argv, customConfigPath))
        return 1;

    g_pAgent = std::make_unique<CAgent>();
    return g_pAgent->start(customConfigPath) ? 0 : 1;
}
