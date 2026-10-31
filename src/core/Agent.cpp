#include "Agent.hpp"
#include "../ui/Dialog.hpp"
#include "../config/Config.hpp"

#include <print>

using namespace Hyprutils::Memory;
using namespace Hyprtoolkit;

CAgent::CAgent()  = default;
CAgent::~CAgent() = default;

bool CAgent::start(const std::optional<std::string>& configPath) {
    static CConfigManager s_configManager{configPath};
    g_pConfigManager = &s_configManager;
    g_pConfigManager->load();

    m_backend = IBackend::create();
    if (!m_backend) {
        std::print(stderr, "failed to create hyprtoolkit backend\n");
        return false;
    }

    if (!m_listener.registerAgent(m_backend))
        return false;

    m_backend->enterLoop();
    return true;
}

void CAgent::beginAuth(CPolkitListener::SAuthRequest req) {
    m_dialog = CUniquePointer<CDialog>(new CDialog(req, m_backend));
    m_dialog->show();
}

void CAgent::endAuth() {
    if (m_dialog) {
        m_dialog->close();
        m_dialog.reset();
    }
}

void CAgent::onRequest(const std::string& prompt, bool echo) {
    if (m_dialog)
        m_dialog->setPrompt(prompt, echo);
}

void CAgent::onInfo(const std::string& text) {
    if (m_dialog)
        m_dialog->setInfo(text);
}

void CAgent::onError(const std::string& text) {
    if (m_dialog)
        m_dialog->setError(text);
}

void CAgent::submitPassword(const std::string& password) {
    m_listener.submitResponse(password);
}

void CAgent::cancel() {
    m_listener.cancelCurrent();
}

void CAgent::selectIdentity(const std::string& s) {
    m_listener.selectIdentity(s);
}
