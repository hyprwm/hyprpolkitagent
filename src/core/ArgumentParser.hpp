#pragma once

#include <optional>
#include <string>

bool parseArguments(int argc, char* argv[], std::optional<std::string>& configPath);
