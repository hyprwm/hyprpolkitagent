#include "Dialog.hpp"

#include "../core/Agent.hpp"
#include "../config/Config.hpp"

#include <hyprtoolkit/element/Rectangle.hpp>
#include <hyprtoolkit/element/RowLayout.hpp>
#include <hyprtoolkit/element/ColumnLayout.hpp>
#include <hyprtoolkit/element/Combobox.hpp>
#include <hyprtoolkit/types/SizeType.hpp>
#include <hyprtoolkit/types/FontTypes.hpp>
#include <hyprtoolkit/palette/Color.hpp>
#include <hyprtoolkit/core/Input.hpp>

#include <xkbcommon/xkbcommon-keysyms.h>
#include <unistd.h>
#include <print>

using namespace Hyprutils::Memory;
using namespace Hyprutils::Math;
using namespace Hyprtoolkit;

CDialog::CDialog(const CPolkitListener::SAuthRequest& req, CSharedPointer<IBackend> backend) : m_backend(backend), m_req(req) {
    build();
}

CDialog::~CDialog() {
    if (m_window)
        m_window->close();
}

void CDialog::show() {
    if (!m_window)
        return;
    m_window->open();
    if (m_passwordField)
        m_passwordField->focus();
}

void CDialog::close() {
    if (m_window) {
        m_window->close();
        m_window.reset();
    }
}

void CDialog::setPrompt(const std::string& text, bool echo) {
    std::string newPrompt = text;
    while (!newPrompt.empty() && (newPrompt.back() == ' ' || newPrompt.back() == '\t'))
        newPrompt.pop_back();
    if (!newPrompt.empty() && newPrompt.back() == ':')
        newPrompt.pop_back();
    while (!newPrompt.empty() && (newPrompt.back() == ' ' || newPrompt.back() == '\t'))
        newPrompt.pop_back();
    if (newPrompt.empty())
        newPrompt = "Password";

    // initial prompt arrives after build() and would clobber the first keystroke
    const bool clean = m_currentPassword.empty() && newPrompt == m_promptText && echo == m_promptEcho;

    m_promptText = newPrompt;
    m_promptEcho = echo;

    if (clean) {
        if (m_passwordField)
            m_passwordField->focus();
        return;
    }

    if (!m_passwordField)
        return;

    m_currentPassword.clear();
    m_passwordField->rebuild()->placeholder(std::string{newPrompt})->commence();
    m_passwordField->setText(std::string{});
    m_passwordField->setPassword(!echo);
    if (m_authEnabled && m_authButton) {
        m_authEnabled = false;
        m_authButton->setEnabled(false);
    }
    m_passwordField->focus();
}

void CDialog::buildPasswordField() {
    const auto& cfg = g_pConfigManager->get();
    m_passwordField = CTextboxBuilder::begin()
                          ->placeholder(std::string{m_promptText})
                          ->password(!m_promptEcho)
                          ->eyeIcon(true)
                          ->multiline(false)
                          ->size({CDynamicSize::HT_SIZE_ABSOLUTE, CDynamicSize::HT_SIZE_ABSOLUTE, {(double)cfg.passwordFieldWidth, 36.0}})
                          ->onTextEdited([this](CSharedPointer<CTextboxElement>, const std::string& s) {
                              m_currentPassword     = s;
                              const bool nowEnabled = !s.empty();
                              if (nowEnabled != m_authEnabled && m_authButton) {
                                  m_authEnabled = nowEnabled;
                                  m_authButton->setEnabled(nowEnabled);
                              }
                              // keep the previous error visible until a new one arrives or the
                              // dialog closes. clearing on first keystroke produced a red flash
                              // because the helper restart resets the field, fires onTextEdited
                              // with an empty string, then again with the first typed char.
                          })
                          ->commence();
}

void CDialog::showStatus(CSharedPointer<IElement>& wrap, bool& shown, bool show) {
    if (!m_statusContainer || !wrap)
        return;
    if (show && !shown) {
        m_statusContainer->addChild(wrap);
        shown = true;
    } else if (!show && shown) {
        m_statusContainer->removeChild(wrap);
        shown = false;
    }
}

void CDialog::setInfo(const std::string& text) {
    if (m_infoLabel)
        m_infoLabel->setText(std::string{text});
    showStatus(m_infoWrap, m_infoShown, !text.empty());
}

void CDialog::setError(const std::string& text) {
    if (m_errorLabel)
        m_errorLabel->setText(std::string{text});
    showStatus(m_errWrap, m_errShown, !text.empty());
}

void CDialog::build() {
    const auto& cfg = g_pConfigManager->get();

    // the window is sized once for the details-expanded case and never resizes.
    // details start hidden; toggling them only fills space already reserved here.
    size_t detailRows = 0;
    if (cfg.showDetails) {
        if (!m_req.actionId.empty())
            ++detailRows;
        if (!m_req.vendor.empty())
            ++detailRows;
        if (!m_req.vendorUrl.empty())
            ++detailRows;
        detailRows += m_req.details.size();
    }
    const double extraForDetails = detailRows > 0 ? (double)detailRows * 8.0 : 0.0;
    const double targetHeight    = (double)cfg.windowHeight + extraForDetails;

    m_window = CWindowBuilder::begin()
                   ->preferredSize({(double)cfg.windowWidth, targetHeight})
                   ->minSize({460.0, targetHeight})
                   ->maxSize({600.0, targetHeight})
                   ->appTitle("Authentication Required")
                   ->appClass("hyprpolkitagent")
                   ->commence();

    m_closeListener = m_window->m_events.closeRequest.listen([] { g_pAgent->cancel(); });

    m_keyListener = m_window->m_events.keyboardKey.listen([this](const Input::SKeyboardKeyEvent& ev) {
        const bool caps = ev.modMask & Input::HT_MODIFIER_CAPS;
        if (caps != m_capsLockOn) {
            m_capsLockOn = caps;
            if (m_capsLockLabel && caps)
                m_capsLockLabel->setText(std::string{"Caps Lock is on"});
            showStatus(m_capsWrap, m_capsShown, caps);
        }

        if (!ev.down || ev.repeat)
            return;
        if (ev.xkbKeysym == XKB_KEY_Return || ev.xkbKeysym == XKB_KEY_KP_Enter) {
            if (!m_currentPassword.empty())
                g_pAgent->submitPassword(m_currentPassword);
        } else if (ev.xkbKeysym == XKB_KEY_Escape) {
            g_pAgent->cancel();
        }
    });

    m_window->m_rootElement->addChild(CRectangleBuilder::begin()->color([] { return g_pAgent->backend()->getPalette()->m_colors.background; })->commence());

    auto outer = CColumnLayoutBuilder::begin()->size({CDynamicSize::HT_SIZE_PERCENT, CDynamicSize::HT_SIZE_AUTO, {0.88F, 0.0F}})->gap(12)->commence();
    outer->setMargin(20);
    outer->setPositionMode(IElement::HT_POSITION_ABSOLUTE);
    outer->setPositionFlag(IElement::HT_POSITION_FLAG_CENTER, true);
    m_window->m_rootElement->addChild(outer);

    {
        auto wrap = CRowLayoutBuilder::begin()->commence();
        wrap->setPositionMode(IElement::HT_POSITION_ABSOLUTE);
        wrap->setPositionFlag(IElement::HT_POSITION_FLAG_HCENTER, true);
        wrap->addChild(CTextBuilder::begin()
                           ->text(std::string{"Authentication Required"})
                           ->fontSize({CFontSize::HT_FONT_H2})
                           ->color([] { return g_pAgent->backend()->getPalette()->m_colors.text; })
                           ->commence());
        outer->addChild(wrap);
    }

    {
        auto wrap = CRowLayoutBuilder::begin()->commence();
        wrap->setPositionMode(IElement::HT_POSITION_ABSOLUTE);
        wrap->setPositionFlag(IElement::HT_POSITION_FLAG_HCENTER, true);

        if (m_req.identities.size() > 1) {
            auto                     cb = CComboboxBuilder::begin();
            std::vector<std::string> items;
            for (const auto& id : m_req.identities)
                items.push_back(id.display);
            cb->items(std::move(items));
            const std::string preferred = "unix-user:" + std::to_string(::geteuid());
            int               sel       = 0;
            for (size_t i = 0; i < m_req.identities.size(); i++) {
                if (m_req.identities[i].raw == preferred) {
                    sel = (int)i;
                    break;
                }
            }
            cb->currentItem((size_t)sel);
            cb->onChanged([this](CSharedPointer<CComboboxElement>, size_t idx) {
                if (idx < m_req.identities.size())
                    g_pAgent->selectIdentity(m_req.identities[idx].raw);
            });
            wrap->addChild(cb->commence());
        } else {
            const std::string display = m_req.identities.empty() ? "" : m_req.identities[0].display;
            wrap->addChild(CTextBuilder::begin()
                               ->text(std::string{"for "} + display)
                               ->fontSize({CFontSize::HT_FONT_SMALL})
                               ->clampSize({(double)cfg.passwordFieldWidth, -1.0})
                               ->color([] { return g_pAgent->backend()->getPalette()->m_colors.text.darken(0.3); })
                               ->commence());
        }
        outer->addChild(wrap);
    }

    {
        std::string msg = m_req.message;
        if (!m_req.command.empty())
            msg = "Allow this action?";

        constexpr size_t MAX_MSG = 120;
        if (msg.size() > MAX_MSG)
            msg = msg.substr(0, MAX_MSG - 3) + "...";

        auto wrap = CRowLayoutBuilder::begin()->commence();
        wrap->setPositionMode(IElement::HT_POSITION_ABSOLUTE);
        wrap->setPositionFlag(IElement::HT_POSITION_FLAG_HCENTER, true);
        wrap->addChild(CTextBuilder::begin()
                           ->text(std::string{msg})
                           ->fontSize({CFontSize::HT_FONT_H3})
                           ->clampSize({(double)cfg.passwordFieldWidth, -1.0})
                           ->color([] { return g_pAgent->backend()->getPalette()->m_colors.text; })
                           ->commence());
        outer->addChild(wrap);
    }

    if (!m_req.command.empty()) {
        auto wrap = CRowLayoutBuilder::begin()->commence();
        wrap->setPositionMode(IElement::HT_POSITION_ABSOLUTE);
        wrap->setPositionFlag(IElement::HT_POSITION_FLAG_HCENTER, true);

        std::string      cmd     = m_req.command;
        constexpr size_t MAX_CMD = 55;
        if (cmd.size() > MAX_CMD)
            cmd = cmd.substr(0, MAX_CMD - 3) + "...";

        const auto palette = g_pAgent->backend()->getPalette();
        auto       box     = CRectangleBuilder::begin()
                       ->color([] { return g_pAgent->backend()->getPalette()->m_colors.alternateBase; })
                       ->rounding(palette->m_vars.bigRounding)
                       ->size(CDynamicSize{CDynamicSize::HT_SIZE_AUTO, CDynamicSize::HT_SIZE_AUTO, {}})
                       ->commence();

        auto inner = CRowLayoutBuilder::begin()->gap(0)->commence();
        inner->setMargin(12);
        inner->addChild(CTextBuilder::begin()
                            ->text(std::string{"$ "})
                            ->fontFamily(std::string{palette->m_vars.fontFamilyMonospace})
                            ->color([] { return g_pAgent->backend()->getPalette()->m_colors.accent; })
                            ->commence());
        inner->addChild(CTextBuilder::begin()
                            ->text(std::string{cmd})
                            ->fontFamily(std::string{palette->m_vars.fontFamilyMonospace})
                            ->clampSize({(double)cfg.passwordFieldWidth, -1.0})
                            ->color([] { return g_pAgent->backend()->getPalette()->m_colors.text; })
                            ->commence());
        box->addChild(inner);
        wrap->addChild(box);
        outer->addChild(wrap);
    }

    {
        auto wrap = CRowLayoutBuilder::begin()->commence();
        wrap->setPositionMode(IElement::HT_POSITION_ABSOLUTE);
        wrap->setPositionFlag(IElement::HT_POSITION_FLAG_HCENTER, true);
        buildPasswordField();
        wrap->addChild(m_passwordField);
        outer->addChild(wrap);
    }

    // wraps are only attached when their label has text, so empty labels reserve no space
    {
        auto statusCol = CColumnLayoutBuilder::begin()->gap(4)->commence();
        statusCol->setPositionMode(IElement::HT_POSITION_ABSOLUTE);
        statusCol->setPositionFlag(IElement::HT_POSITION_FLAG_HCENTER, true);
        outer->addChild(statusCol);
        m_statusContainer = statusCol;

        auto makeWrap = [](CSharedPointer<CTextElement> label) {
            auto w = CRowLayoutBuilder::begin()->commence();
            w->setPositionMode(IElement::HT_POSITION_ABSOLUTE);
            w->setPositionFlag(IElement::HT_POSITION_FLAG_HCENTER, true);
            w->addChild(label);
            return w;
        };

        m_capsLockLabel = CTextBuilder::begin()->text(std::string{""})->fontSize({CFontSize::HT_FONT_SMALL})->color([] { return CHyprColor{1.0F, 0.75F, 0.2F, 1.F}; })->commence();
        m_errorLabel    = CTextBuilder::begin()->text(std::string{""})->color([] { return CHyprColor{0.9F, 0.4F, 0.4F, 1.F}; })->commence();
        m_infoLabel     = CTextBuilder::begin()->text(std::string{""})->color([] { return g_pAgent->backend()->getPalette()->m_colors.text.darken(0.4); })->commence();

        m_capsWrap = makeWrap(m_capsLockLabel);
        m_errWrap  = makeWrap(m_errorLabel);
        m_infoWrap = makeWrap(m_infoLabel);
    }

    {
        auto btnRow = CRowLayoutBuilder::begin()->gap(10)->commence();
        btnRow->setMargin(8);
        btnRow->setPositionMode(IElement::HT_POSITION_ABSOLUTE);
        btnRow->setPositionFlag(IElement::HT_POSITION_FLAG_HCENTER, true);

        btnRow->addChild(
            CButtonBuilder::begin()->label(std::string{"Cancel"})->noBorder(true)->noBg(true)->onMainClick([](CSharedPointer<CButtonElement>) { g_pAgent->cancel(); })->commence());

        m_authButton = CButtonBuilder::begin()
                           ->label(std::string{"Authenticate"})
                           ->enabled(false)
                           ->onMainClick([this](CSharedPointer<CButtonElement>) {
                               if (!m_currentPassword.empty())
                                   g_pAgent->submitPassword(m_currentPassword);
                           })
                           ->commence();
        btnRow->addChild(m_authButton);
        outer->addChild(btnRow);
    }

    std::vector<std::pair<std::string, std::string>> fields;
    if (!m_req.actionId.empty())
        fields.emplace_back("Action", m_req.actionId);
    if (!m_req.vendor.empty())
        fields.emplace_back("Vendor", m_req.vendor);
    if (!m_req.vendorUrl.empty())
        fields.emplace_back("URL", m_req.vendorUrl);
    for (const auto& [k, v] : m_req.details)
        fields.emplace_back(k, v);

    if (cfg.showDetails && !fields.empty()) {
        auto detailsWrap = CRowLayoutBuilder::begin()->commence();
        detailsWrap->setPositionMode(IElement::HT_POSITION_ABSOLUTE);
        detailsWrap->setPositionFlag(IElement::HT_POSITION_FLAG_HCENTER, true);

        m_detailsButton = CButtonBuilder::begin()
                              ->label(std::string{"Show details"})
                              ->noBorder(true)
                              ->noBg(true)
                              ->fontSize({CFontSize::HT_FONT_SMALL})
                              ->onMainClick([this](CSharedPointer<CButtonElement>) {
                                  m_detailsVisible = !m_detailsVisible;
                                  if (m_detailsVisible)
                                      m_detailsParent->addChild(m_detailsContainer);
                                  else
                                      m_detailsParent->removeChild(m_detailsContainer);
                                  m_detailsButton->rebuild()->label(std::string{m_detailsVisible ? "Hide details" : "Show details"})->commence();
                              })
                              ->commence();
        detailsWrap->addChild(m_detailsButton);
        outer->addChild(detailsWrap);

        auto detailsBoxWrap = CRowLayoutBuilder::begin()->commence();
        detailsBoxWrap->setPositionMode(IElement::HT_POSITION_ABSOLUTE);
        detailsBoxWrap->setPositionFlag(IElement::HT_POSITION_FLAG_HCENTER, true);

        auto detailsBox = CRectangleBuilder::begin()
                              ->color([] { return g_pAgent->backend()->getPalette()->m_colors.alternateBase; })
                              ->rounding(g_pAgent->backend()->getPalette()->m_vars.bigRounding)
                              ->size(CDynamicSize{CDynamicSize::HT_SIZE_AUTO, CDynamicSize::HT_SIZE_AUTO, {}})
                              ->commence();

        auto detailsCol = CColumnLayoutBuilder::begin()->gap(4)->commence();
        detailsCol->setMargin(12);

        for (const auto& [key, val] : fields) {
            auto rowWrap = CRowLayoutBuilder::begin()->commence();
            rowWrap->setPositionMode(IElement::HT_POSITION_ABSOLUTE);
            rowWrap->setPositionFlag(IElement::HT_POSITION_FLAG_HCENTER, true);
            rowWrap->addChild(CTextBuilder::begin()
                                  ->text(key + ": " + val)
                                  ->fontSize({CFontSize::HT_FONT_SMALL})
                                  ->color([] { return g_pAgent->backend()->getPalette()->m_colors.text.darken(0.3); })
                                  ->commence());
            detailsCol->addChild(rowWrap);
        }

        detailsBox->addChild(detailsCol);
        detailsBoxWrap->addChild(detailsBox);
        m_detailsContainer = detailsBoxWrap;
        m_detailsParent    = outer;
    }

    if (m_passwordField)
        m_passwordField->focus();
}
