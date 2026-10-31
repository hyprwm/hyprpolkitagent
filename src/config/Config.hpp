#pragma once

#include <filesystem>
#include <optional>
#include <string>

struct SHPAConfig {
    // general {}
    int  passwordFieldWidth = 340;
    int  windowWidth        = 520;
    int  windowHeight       = 440;
    bool showDetails        = true;
    bool allowEmptyPassword = false;
};

class CConfigManager {
  public:
    CConfigManager(std::optional<std::string> customPath = "");

    void              load();

    const SHPAConfig& get() const {
        return m_cfg;
    }

  private:
    SHPAConfig            m_cfg;
    std::filesystem::path m_path;
    bool                  m_explicit = false;
};

inline CConfigManager* g_pConfigManager = nullptr;
