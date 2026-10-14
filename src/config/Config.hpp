#pragma once

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
    CConfigManager();

    void              load();

    const SHPAConfig& get() const {
        return m_cfg;
    }

  private:
    SHPAConfig  m_cfg;
    std::string m_path;
};

inline CConfigManager* g_pConfigManager = nullptr;
