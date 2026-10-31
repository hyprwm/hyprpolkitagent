#pragma once

#include <hyprutils/memory/SharedPtr.hpp>
#include <hyprutils/memory/UniquePtr.hpp>

#include <hyprtoolkit/core/Backend.hpp>

#include <memory>
#include <string>

#include "PolkitListener.hpp"

class CDialog;

class CAgent {
  public:
    CAgent();
    ~CAgent();

    bool                                                     start(const std::optional<std::string>& configPath);
    void                                                     beginAuth(CPolkitListener::SAuthRequest req);
    void                                                     endAuth();
    void                                                     onRequest(const std::string& prompt, bool echo);
    void                                                     onInfo(const std::string& text);
    void                                                     onError(const std::string& text);
    void                                                     submitPassword(const std::string& password);
    void                                                     cancel();
    void                                                     selectIdentity(const std::string& identityString);

    Hyprutils::Memory::CSharedPointer<Hyprtoolkit::IBackend> backend() {
        return m_backend;
    }

    CPolkitListener& listener() {
        return m_listener;
    }

  private:
    Hyprutils::Memory::CSharedPointer<Hyprtoolkit::IBackend> m_backend;

    CPolkitListener                                          m_listener;

    Hyprutils::Memory::CUniquePointer<CDialog>               m_dialog;
};

inline std::unique_ptr<CAgent> g_pAgent;
