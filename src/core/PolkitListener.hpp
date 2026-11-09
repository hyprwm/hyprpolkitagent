#pragma once

#include <sdbus-c++/sdbus-c++.h>

#include <hyprutils/memory/SharedPtr.hpp>

#include <hyprtoolkit/core/Backend.hpp>

#include <deque>
#include <memory>
#include <string>
#include <vector>

class CPolkitListener {
  public:
    struct SIdentity {
        std::string raw;     // e.g. "unix-user:1000"
        std::string display; // "alice" or "1000"
        uint32_t    uid = (uint32_t)-1;
    };

    struct SAuthRequest {
        std::string                                      actionId;
        std::string                                      message;
        std::string                                      iconName;
        std::string                                      cookie;
        std::vector<SIdentity>                           identities;
        std::string                                      command;
        std::vector<std::pair<std::string, std::string>> details;
        std::string                                      vendor;
        std::string                                      vendorUrl;
    };

    CPolkitListener();
    ~CPolkitListener();

    bool registerAgent(Hyprutils::Memory::CSharedPointer<Hyprtoolkit::IBackend> backend);

    void submitResponse(const std::string& password);
    void cancelCurrent();
    void selectIdentity(const std::string& identityString);

  private:
    enum eHelperMode {
        HELPER_NONE,
        HELPER_SOCKET,
        HELPER_FORK,
    };

    struct SHelperProc {
        eHelperMode mode      = HELPER_NONE;
        pid_t       pid       = -1;
        int         stdinFd   = -1;
        int         stdoutFd  = -1;
        bool        gotPrompt = false;
        bool        triedFork = false;
        std::string buffer;
    };

    struct SActiveAuth {
        SAuthRequest                     req;
        std::unique_ptr<sdbus::Result<>> reply;
        int                              selectedIdx = 0;
        SHelperProc                      helper;
    };

    void        onBeginAuthentication(sdbus::Result<>&& result, std::string actionId, std::string message, std::string iconName, std::map<std::string, std::string> details,
                                      std::string cookie, std::vector<sdbus::Struct<std::string, std::map<std::string, sdbus::Variant>>> identities);
    void        onCancelAuthentication(std::string cookie);

    void        startHelper(SActiveAuth& a);
    bool        tryHelperSocket(SActiveAuth& a);
    bool        tryHelperFork(SActiveAuth& a);
    void        killHelper(SHelperProc& h);
    void        onHelperReadable();
    void        handleLine(SActiveAuth& a, const std::string& line);
    void        completeAuth(bool succeeded, bool cancelled);
    void        startNextQueued();
    void        rejectActive(const std::string& msg);

    void        pumpBus();
    bool        registerWithAuthority();
    bool        isSessionLocked();
    std::string getOwnSessionId();
    std::string identityToString(const sdbus::Struct<std::string, std::map<std::string, sdbus::Variant>>& id) const;

    Hyprutils::Memory::CSharedPointer<Hyprtoolkit::IBackend> m_backend;

    std::unique_ptr<sdbus::IConnection>                      m_conn;
    std::unique_ptr<sdbus::IObject>                          m_agentObject;
    // watches org.freedesktop.DBus for PolicyKit1 changing owner, i.e. polkitd restarting.
    // declared after m_conn so it is destroyed first (the signal match lives on that connection).
    std::unique_ptr<sdbus::IProxy>                           m_dbusProxy;
    std::string                                              m_objectPath = "/org/hyprland/PolicyKit1/AuthenticationAgent";
    std::string                                              m_sessionId;

    int                                                      m_watchedFd      = -1;
    int                                                      m_watchedEventFd = -1;

    std::unique_ptr<SActiveAuth>                             m_current;
    std::deque<std::unique_ptr<SActiveAuth>>                 m_queue;
};
