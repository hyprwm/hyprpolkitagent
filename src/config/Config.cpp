#include "Config.hpp"

#include <hyprlang.hpp>
#include <cstdlib>
#include <print>

static std::string configPath() {
    const char* xdg  = getenv("XDG_CONFIG_HOME");
    std::string base = xdg ? xdg : (std::string{getenv("HOME")} + "/.config");
    return base + "/hyprpolkitagent/hyprpolkitagent.conf";
}

CConfigManager::CConfigManager() : m_path(configPath()) {}

void CConfigManager::load() {
    Hyprlang::SConfigOptions opts;
    opts.allowMissingConfig = true;
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
