#pragma once

#include <hyprutils/memory/SharedPtr.hpp>
#include <hyprutils/signal/Signal.hpp>

#include <hyprtoolkit/core/Backend.hpp>
#include <hyprtoolkit/window/Window.hpp>
#include <hyprtoolkit/element/Textbox.hpp>
#include <hyprtoolkit/element/Button.hpp>
#include <hyprtoolkit/element/Text.hpp>
#include <hyprtoolkit/element/Element.hpp>

#include <string>

#include "../core/PolkitListener.hpp"

class CDialog {
  public:
    CDialog(const CPolkitListener::SAuthRequest& req, Hyprutils::Memory::CSharedPointer<Hyprtoolkit::IBackend> backend);
    ~CDialog();

    void show();
    void close();

    void setPrompt(const std::string& text, bool echo);
    void setInfo(const std::string& text);
    void setError(const std::string& text);

  private:
    void                                                            build();
    void                                                            buildPasswordField();
    void                                                            showStatus(Hyprutils::Memory::CSharedPointer<Hyprtoolkit::IElement>& wrap, bool& shown, bool show);
    void                                                            showPasswordField(bool show);

    Hyprutils::Memory::CSharedPointer<Hyprtoolkit::IBackend>        m_backend;
    Hyprutils::Memory::CSharedPointer<Hyprtoolkit::IWindow>         m_window;

    Hyprutils::Memory::CSharedPointer<Hyprtoolkit::CTextboxElement> m_passwordField;
    Hyprutils::Memory::CSharedPointer<Hyprtoolkit::CTextElement>    m_errorLabel;
    Hyprutils::Memory::CSharedPointer<Hyprtoolkit::CTextElement>    m_infoLabel;
    Hyprutils::Memory::CSharedPointer<Hyprtoolkit::CTextElement>    m_capsLockLabel;
    Hyprutils::Memory::CSharedPointer<Hyprtoolkit::CButtonElement>  m_authButton;
    Hyprutils::Memory::CSharedPointer<Hyprtoolkit::CButtonElement>  m_detailsButton;
    Hyprutils::Memory::CSharedPointer<Hyprtoolkit::IElement>        m_detailsContainer;
    Hyprutils::Memory::CSharedPointer<Hyprtoolkit::IElement>        m_detailsParent;
    Hyprutils::Memory::CSharedPointer<Hyprtoolkit::IElement>        m_statusContainer;
    Hyprutils::Memory::CSharedPointer<Hyprtoolkit::IElement>        m_capsWrap;
    Hyprutils::Memory::CSharedPointer<Hyprtoolkit::IElement>        m_errWrap;
    Hyprutils::Memory::CSharedPointer<Hyprtoolkit::IElement>        m_infoWrap;
    Hyprutils::Memory::CSharedPointer<Hyprtoolkit::IElement>        m_passwordWrap;
    Hyprutils::Memory::CSharedPointer<Hyprtoolkit::IElement>        m_btnRow;

    Hyprutils::Signal::CHyprSignalListener                          m_closeListener;
    Hyprutils::Signal::CHyprSignalListener                          m_keyListener;

    CPolkitListener::SAuthRequest                                   m_req;
    std::string                                                     m_currentPassword;
    bool                                                            m_promptEcho     = false;
    bool                                                            m_authEnabled    = false;
    bool                                                            m_detailsVisible = false;
    bool                                                            m_capsLockOn     = false;
    bool                                                            m_capsShown      = false;
    bool                                                            m_errShown       = false;
    bool                                                            m_infoShown      = false;
    bool                                                            m_sawPasswordPrompt = false;
    bool                                                            m_passwordVisible   = true;
    std::string                                                     m_promptText     = "Password";
};
