#include "Config.hpp"

#include <hyprlang.hpp>
#include <filesystem>
#include <cstdlib>
#include <optional>
#include <print>

static std::optional<std::filesystem::path> configPath() {
    constexpr auto REL = "hyprpolkitagent/hyprpolkitagent.conf";
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME"); xdg && *xdg)
        return std::filesystem::path{xdg} / REL;
    if (const char* home = std::getenv("HOME"); home && *home)
        return std::filesystem::path{home} / ".config" / REL;
    return std::nullopt;
}

CConfigManager::CConfigManager(std::optional<std::string> customPath) {
    if (customPath) {
        m_explicit = true;
        m_path     = std::filesystem::absolute(*customPath);
    } else if (auto d = configPath())
        m_path = *d;
};

void CConfigManager::load() {
    std::error_code ec;
    if (m_explicit && !std::filesystem::is_regular_file(m_path, ec))
        std::print(stderr, "hyprpolkitagent: config file not found: {}\n", m_path.string());

    Hyprlang::SConfigOptions opts;
    opts.allowMissingConfig = !m_explicit;
    Hyprlang::CConfig cfg{m_path.c_str(), opts};

    cfg.addConfigValue("general:password_field_width", Hyprlang::INT{340});
    cfg.addConfigValue("general:window_width", Hyprlang::INT{520});
    cfg.addConfigValue("general:window_height", Hyprlang::INT{440});
    cfg.addConfigValue("general:show_details", Hyprlang::INT{1});
    cfg.addConfigValue("general:allow_empty_password", Hyprlang::INT{0});

    cfg.commence();
    const auto res = cfg.parse();
    if (res.error)
        std::print(stderr, "hyprpolkitagent: config parse error: {}\n", res.getError());

    auto iv = [&](const char* k) { return std::any_cast<Hyprlang::INT>(cfg.getConfigValue(k)); };

    m_cfg.passwordFieldWidth = iv("general:password_field_width");
    m_cfg.windowWidth        = iv("general:window_width");
    m_cfg.windowHeight       = iv("general:window_height");
    m_cfg.showDetails        = iv("general:show_details") != 0;
    m_cfg.allowEmptyPassword = iv("general:allow_empty_password") != 0;
}
