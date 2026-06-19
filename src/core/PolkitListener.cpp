#include "PolkitListener.hpp"

#include "Agent.hpp"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <print>
#include <signal.h>
#include <pwd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>

namespace {
    constexpr const char* AGENT_HELPER_SOCKET = "/run/polkit/agent-helper.socket";

    const char*           findHelperPath() {
        static constexpr const char* candidates[] = {
            "/run/wrappers/bin/polkit-agent-helper-1",        "/run/wrappers/bin/.polkit-agent-helper-1-wrapped", "/usr/lib/polkit-1/polkit-agent-helper-1",
            "/usr/libexec/polkit-1/polkit-agent-helper-1",    "/usr/lib64/polkit-1/polkit-agent-helper-1",        "/usr/lib/policykit-1/polkit-agent-helper-1",
            "/usr/libexec/policykit-1/polkit-agent-helper-1",
        };
        for (const char* path : candidates) {
            if (access(path, X_OK) == 0)
                return path;
        }
        return nullptr;
    }

    bool helperIsSetuid(const char* path) {
        struct stat st{};
        return path && stat(path, &st) == 0 && (st.st_mode & S_ISUID);
    }

    bool socketReachable() {
        return access(AGENT_HELPER_SOCKET, W_OK) == 0;
    }

    constexpr const char* POLKIT_BUS         = "org.freedesktop.PolicyKit1";
    constexpr const char* POLKIT_OBJ         = "/org/freedesktop/PolicyKit1/Authority";
    constexpr const char* POLKIT_AUTHORITY   = "org.freedesktop.PolicyKit1.Authority";
    constexpr const char* POLKIT_AGENT_IFACE = "org.freedesktop.PolicyKit1.AuthenticationAgent";

    constexpr const char* LOGIND_BUS       = "org.freedesktop.login1";
    constexpr const char* LOGIND_MGR_OBJ   = "/org/freedesktop/login1";
    constexpr const char* LOGIND_MGR_IFACE = "org.freedesktop.login1.Manager";
    constexpr const char* LOGIND_SESSION   = "org.freedesktop.login1.Session";

    std::string           usernameForUid(uint32_t uid) {
        struct passwd  pwbuf;
        struct passwd* result = nullptr;
        char           buf[4096];
        if (getpwuid_r(uid, &pwbuf, buf, sizeof(buf), &result) == 0 && result && result->pw_name)
            return result->pw_name;
        return std::to_string(uid);
    }

}

CPolkitListener::CPolkitListener() {
    signal(SIGPIPE, SIG_IGN);
}

CPolkitListener::~CPolkitListener() {
    if (m_current)
        killHelper(m_current->helper);
    if (m_backend) {
        if (m_watchedFd >= 0)
            m_backend->removeFd(m_watchedFd);
        if (m_watchedEventFd >= 0 && m_watchedEventFd != m_watchedFd)
            m_backend->removeFd(m_watchedEventFd);
    }
    if (m_conn && m_agentObject) {
        try {
            auto                                                              proxy = sdbus::createProxy(*m_conn, sdbus::ServiceName{POLKIT_BUS}, sdbus::ObjectPath{POLKIT_OBJ});
            sdbus::Struct<std::string, std::map<std::string, sdbus::Variant>> subject{"unix-session", {{"session-id", sdbus::Variant{m_sessionId}}}};
            proxy->callMethod("UnregisterAuthenticationAgent").onInterface(POLKIT_AUTHORITY).withArguments(subject, m_objectPath);
        } catch (...) {}
    }
}

bool CPolkitListener::registerAgent(Hyprutils::Memory::CSharedPointer<Hyprtoolkit::IBackend> backend) {
    m_backend = backend;

    try {
        m_conn = sdbus::createSystemBusConnection();
    } catch (const sdbus::Error& e) {
        std::print(stderr, "failed to open system bus: {}\n", e.what());
        return false;
    }

    m_sessionId = getOwnSessionId();
    if (m_sessionId.empty()) {
        std::print(stderr,
                   "could not resolve our logind session id (no XDG_SESSION_ID env, no session for pid, no active User.Display). "
                   "Is there an active graphical session for this user?\n");
        return false;
    }

    try {
        m_agentObject = sdbus::createObject(*m_conn, sdbus::ObjectPath{m_objectPath});
        m_agentObject
            ->addVTable(
                sdbus::registerMethod("BeginAuthentication")
                    .withInputParamNames("action_id", "message", "icon_name", "details", "cookie", "identities")
                    .implementedAs([this](sdbus::Result<>&& res, std::string actionId, std::string message, std::string iconName, std::map<std::string, std::string> details,
                                          std::string cookie, std::vector<sdbus::Struct<std::string, std::map<std::string, sdbus::Variant>>> identities) {
                        onBeginAuthentication(std::move(res), std::move(actionId), std::move(message), std::move(iconName), std::move(details), std::move(cookie),
                                              std::move(identities));
                    }),
                sdbus::registerMethod("CancelAuthentication").withInputParamNames("cookie").implementedAs([this](std::string cookie) {
                    onCancelAuthentication(std::move(cookie));
                }))
            .forInterface(POLKIT_AGENT_IFACE);
    } catch (const sdbus::Error& e) {
        std::print(stderr, "failed to expose agent vtable: {}\n", e.what());
        return false;
    }

    try {
        auto                                                              proxy = sdbus::createProxy(*m_conn, sdbus::ServiceName{POLKIT_BUS}, sdbus::ObjectPath{POLKIT_OBJ});
        sdbus::Struct<std::string, std::map<std::string, sdbus::Variant>> subject{"unix-session", {{"session-id", sdbus::Variant{m_sessionId}}}};
        const char*                                                       locale = getenv("LANG");
        proxy->callMethod("RegisterAuthenticationAgent").onInterface(POLKIT_AUTHORITY).withArguments(subject, std::string{locale ? locale : ""}, m_objectPath);
    } catch (const sdbus::Error& e) {
        std::print(stderr, "failed to register agent: {}\n", e.what());
        return false;
    }

    const auto poll  = m_conn->getEventLoopPollData();
    m_watchedFd      = poll.fd;
    m_watchedEventFd = poll.eventFd;
    m_backend->addFd(m_watchedFd, [this] { pumpBus(); });
    if (m_watchedEventFd >= 0 && m_watchedEventFd != m_watchedFd)
        m_backend->addFd(m_watchedEventFd, [this] { pumpBus(); });

    pumpBus();

    const char* helper = findHelperPath();
    std::print(stderr, "registered polkit agent for session {} (helper-socket={} helper-path={} setuid={})\n", m_sessionId, socketReachable() ? "yes" : "no",
               helper ? helper : "none", helperIsSetuid(helper) ? "yes" : "no");
    if (!socketReachable() && (!helper || !helperIsSetuid(helper)))
        std::print(stderr, "warning: no working helper detected; auth requests will fail. install polkit or enable polkit-agent-helper.socket\n");
    return true;
}

void CPolkitListener::pumpBus() {
    if (!m_conn)
        return;
    try {
        while (m_conn->processPendingEvent()) {}
    } catch (const sdbus::Error& e) { std::print(stderr, "bus pump error: {}\n", e.what()); }
}

std::string CPolkitListener::getOwnSessionId() {
    // 1) env var, fast path. pam_systemd sets it, but the user manager may not import it.
    if (const char* env = getenv("XDG_SESSION_ID"); env && *env)
        return env;

    // 2) our pid's session, via cgroup. only works if we're in a session.scope.
    try {
        auto              proxy = sdbus::createProxy(*m_conn, sdbus::ServiceName{LOGIND_BUS}, sdbus::ObjectPath{LOGIND_MGR_OBJ});
        sdbus::ObjectPath path;
        proxy->callMethod("GetSessionByPID").onInterface(LOGIND_MGR_IFACE).withArguments((uint32_t)getpid()).storeResultsTo(path);

        auto           sproxy = sdbus::createProxy(*m_conn, sdbus::ServiceName{LOGIND_BUS}, path);
        sdbus::Variant v;
        sproxy->callMethod("Get").onInterface("org.freedesktop.DBus.Properties").withArguments(std::string{LOGIND_SESSION}, std::string{"Id"}).storeResultsTo(v);
        return v.get<std::string>();
    } catch (const sdbus::Error&) {}

    // 3) user's active display session, via logind User.Display. survives user@.service cgroup.
    try {
        auto              mgr = sdbus::createProxy(*m_conn, sdbus::ServiceName{LOGIND_BUS}, sdbus::ObjectPath{LOGIND_MGR_OBJ});
        sdbus::ObjectPath userPath;
        mgr->callMethod("GetUser").onInterface(LOGIND_MGR_IFACE).withArguments((uint32_t)getuid()).storeResultsTo(userPath);

        auto           user = sdbus::createProxy(*m_conn, sdbus::ServiceName{LOGIND_BUS}, userPath);
        sdbus::Variant v;
        user->callMethod("Get").onInterface("org.freedesktop.DBus.Properties").withArguments(std::string{"org.freedesktop.login1.User"}, std::string{"Display"}).storeResultsTo(v);
        const auto disp = v.get<sdbus::Struct<std::string, sdbus::ObjectPath>>();
        if (!disp.get<0>().empty())
            return disp.get<0>();
    } catch (const sdbus::Error& e) {
        std::print(stderr, "logind session lookup failed: {}\n", e.what());
    }

    return {};
}

bool CPolkitListener::isSessionLocked() {
    if (!m_conn || m_sessionId.empty())
        return false;
    try {
        auto           proxy = sdbus::createProxy(*m_conn, sdbus::ServiceName{LOGIND_BUS}, sdbus::ObjectPath{"/org/freedesktop/login1/session/auto"});
        sdbus::Variant v;
        proxy->callMethod("Get").onInterface("org.freedesktop.DBus.Properties").withArguments(std::string{LOGIND_SESSION}, std::string{"LockedHint"}).storeResultsTo(v);
        return v.get<bool>();
    } catch (...) { return false; }
}

std::string CPolkitListener::identityToString(const sdbus::Struct<std::string, std::map<std::string, sdbus::Variant>>& id) const {
    const auto& kind = id.get<0>();
    const auto& dict = id.get<1>();
    if (kind == "unix-user") {
        if (auto it = dict.find("uid"); it != dict.end()) {
            try {
                return "unix-user:" + std::to_string(it->second.get<uint32_t>());
            } catch (...) {}
        }
    } else if (kind == "unix-group") {
        if (auto it = dict.find("gid"); it != dict.end()) {
            try {
                return "unix-group:" + std::to_string(it->second.get<uint32_t>());
            } catch (...) {}
        }
    }
    return kind;
}

void CPolkitListener::onBeginAuthentication(sdbus::Result<>&& result, std::string actionId, std::string message, std::string iconName, std::map<std::string, std::string> details,
                                            std::string cookie, std::vector<sdbus::Struct<std::string, std::map<std::string, sdbus::Variant>>> identities) {
    auto a          = std::make_unique<SActiveAuth>();
    a->reply        = std::make_unique<sdbus::Result<>>(std::move(result));
    a->req.actionId = std::move(actionId);
    a->req.message  = std::move(message);
    a->req.iconName = std::move(iconName);
    a->req.cookie   = std::move(cookie);

    for (const auto& id : identities) {
        SIdentity ent;
        ent.raw = identityToString(id);
        if (id.get<0>() == "unix-user") {
            if (auto it = id.get<1>().find("uid"); it != id.get<1>().end()) {
                try {
                    ent.uid     = it->second.get<uint32_t>();
                    ent.display = usernameForUid(ent.uid);
                } catch (...) {}
            }
        }
        if (ent.display.empty())
            ent.display = ent.raw;
        a->req.identities.push_back(std::move(ent));
    }

    auto take = [&](const char* key) -> std::string {
        if (auto it = details.find(key); it != details.end())
            return it->second;
        return {};
    };

    a->req.command = take("command_line");
    if (a->req.command.empty())
        a->req.command = take("cmdline");
    if (a->req.command.empty())
        a->req.command = take("command");
    a->req.vendor    = take("polkit.vendor");
    a->req.vendorUrl = take("polkit.vendor_url");

    for (const auto& [k, v] : details) {
        if (k.starts_with("polkit."))
            continue;
        if (k == "command_line" || k == "cmdline" || k == "command")
            continue;
        if (!v.empty())
            a->req.details.emplace_back(k, v);
    }

    if (a->req.command.empty() && !a->req.message.empty()) {
        auto s = a->req.message.find("to run ");
        if (s != std::string::npos) {
            s += 7;
            if (s < a->req.message.size() && (a->req.message[s] == '\'' || a->req.message[s] == '`')) {
                ++s;
                auto e = a->req.message.find_first_of("'`", s);
                if (e != std::string::npos)
                    a->req.command = a->req.message.substr(s, e - s);
            }
        }
    }

    if (m_current) {
        m_queue.push_back(std::move(a));
        return;
    }

    if (a->req.identities.empty()) {
        a->reply->returnError(sdbus::Error{sdbus::Error::Name{"org.freedesktop.PolicyKit1.Error.Failed"}, "No identities"});
        return;
    }

    if (isSessionLocked()) {
        a->reply->returnError(sdbus::Error{sdbus::Error::Name{"org.freedesktop.PolicyKit1.Error.Failed"}, "Session is locked"});
        return;
    }

    const std::string preferred = "unix-user:" + std::to_string((uint32_t)geteuid());
    for (size_t i = 0; i < a->req.identities.size(); i++) {
        if (a->req.identities[i].raw == preferred) {
            a->selectedIdx = (int)i;
            break;
        }
    }

    m_current = std::move(a);
    g_pAgent->beginAuth(m_current->req);
    startHelper(*m_current);
}

void CPolkitListener::onCancelAuthentication(std::string cookie) {
    if (m_current && m_current->req.cookie == cookie) {
        completeAuth(false, true);
        return;
    }
    for (auto it = m_queue.begin(); it != m_queue.end(); ++it) {
        if ((*it)->req.cookie == cookie) {
            (*it)->reply->returnError(sdbus::Error{sdbus::Error::Name{"org.freedesktop.PolicyKit1.Error.Cancelled"}, "Cancelled"});
            m_queue.erase(it);
            return;
        }
    }
}

void CPolkitListener::startHelper(SActiveAuth& a) {
    if (a.selectedIdx < 0 || a.selectedIdx >= (int)a.req.identities.size())
        return;

    if (!a.helper.triedFork && tryHelperSocket(a))
        return;
    a.helper.triedFork = true;
    if (tryHelperFork(a))
        return;

    std::print(stderr, "no usable polkit helper (socket={}, fork-helper={}); auth not possible\n", socketReachable() ? "yes" : "no", findHelperPath() ? findHelperPath() : "none");
    completeAuth(false, false);
}

bool CPolkitListener::tryHelperSocket(SActiveAuth& a) {
    if (!socketReachable())
        return false;

    int sockFd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    if (sockFd < 0) {
        std::print(stderr, "socket() failed: {}\n", strerror(errno));
        return false;
    }
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, AGENT_HELPER_SOCKET, sizeof(addr.sun_path) - 1);
    if (connect(sockFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0 && errno != EINPROGRESS) {
        const int e = errno;
        if (e == EACCES)
            std::print(stderr, "helper socket connect: permission denied (possible LSM/SELinux denial; check `ausearch -m AVC`)\n");
        else
            std::print(stderr, "helper socket connect failed: {}\n", strerror(e));
        ::close(sockFd);
        return false;
    }

    const auto&       ident    = a.req.identities[a.selectedIdx];
    const std::string preamble = ident.display + "\n" + a.req.cookie + "\n";
    (void)write(sockFd, preamble.data(), preamble.size());

    a.helper.mode     = HELPER_SOCKET;
    a.helper.pid      = 0;
    a.helper.stdinFd  = sockFd;
    a.helper.stdoutFd = sockFd;

    m_backend->addFd(sockFd, [this] { onHelperReadable(); });
    return true;
}

bool CPolkitListener::tryHelperFork(SActiveAuth& a) {
    const char* helper = findHelperPath();
    if (!helper) {
        std::print(stderr, "no polkit-agent-helper-1 binary found in any known path\n");
        return false;
    }
    if (!helperIsSetuid(helper)) {
        std::print(stderr, "{} is not setuid root; cannot use fork+exec fallback\n", helper);
        return false;
    }

    int in[2]  = {-1, -1};
    int out[2] = {-1, -1};
    if (pipe2(in, O_CLOEXEC) < 0 || pipe2(out, O_CLOEXEC | O_NONBLOCK) < 0) {
        std::print(stderr, "pipe failed: {}\n", strerror(errno));
        return false;
    }

    pid_t pid = fork();
    if (pid < 0) {
        std::print(stderr, "fork failed: {}\n", strerror(errno));
        ::close(in[0]);
        ::close(in[1]);
        ::close(out[0]);
        ::close(out[1]);
        return false;
    }

    if (pid == 0) {
        dup2(in[0], STDIN_FILENO);
        dup2(out[1], STDOUT_FILENO);
        ::close(in[0]);
        ::close(in[1]);
        ::close(out[0]);
        ::close(out[1]);

        const auto&       ident = a.req.identities[a.selectedIdx];
        const std::string user  = ident.display;
        execl(helper, "polkit-agent-helper-1", user.c_str(), (char*)nullptr);
        if (errno == EACCES)
            _exit(126);
        _exit(127);
    }

    ::close(in[0]);
    ::close(out[1]);

    a.helper.mode     = HELPER_FORK;
    a.helper.pid      = pid;
    a.helper.stdinFd  = in[1];
    a.helper.stdoutFd = out[0];

    const std::string ck = a.req.cookie + "\n";
    (void)write(a.helper.stdinFd, ck.data(), ck.size());

    m_backend->addFd(a.helper.stdoutFd, [this] { onHelperReadable(); });
    return true;
}

void CPolkitListener::onHelperReadable() {
    if (!m_current || m_current->helper.stdoutFd < 0)
        return;

    auto*   cur = m_current.get();
    bool    eof = false;

    char    buf[1024];
    ssize_t n;
    while ((n = read(cur->helper.stdoutFd, buf, sizeof(buf))) > 0)
        cur->helper.buffer.append(buf, (size_t)n);

    if (n == 0)
        eof = true;
    else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
        eof = true;

    while (m_current.get() == cur) {
        const auto pos = cur->helper.buffer.find('\n');
        if (pos == std::string::npos)
            break;
        std::string line = cur->helper.buffer.substr(0, pos);
        cur->helper.buffer.erase(0, pos + 1);
        handleLine(*cur, line);
    }

    if (eof && m_current.get() == cur) {
        const bool wasSocket = (cur->helper.mode == HELPER_SOCKET);
        const bool hadPrompt = cur->helper.gotPrompt;
        if (cur->helper.pid > 0) {
            int status = 0;
            waitpid(cur->helper.pid, &status, 0);
            cur->helper.pid = -1;
        }
        if (wasSocket && !hadPrompt && !cur->helper.triedFork) {
            std::print(stderr, "socket helper closed without prompt (likely kernel pre-SO_PEERPIDFD); falling back to fork+exec\n");
            killHelper(cur->helper);
            cur->helper.triedFork = true;
            startHelper(*cur);
            return;
        }
        // helper exited without ever prompting AND fork fallback was already tried.
        // unrecoverable PAM state (locked account, broken stack). complete as cancelled
        // so polkitd does not re-dispatch BeginAuthentication and reopen forever.
        completeAuth(false, !hadPrompt && cur->helper.triedFork);
    }
}

void CPolkitListener::handleLine(SActiveAuth& a, const std::string& line) {
    auto prefix = [&](const char* tag) -> std::optional<std::string> {
        const size_t tlen = std::strlen(tag);
        if (line.size() >= tlen && line.compare(0, tlen, tag) == 0) {
            size_t s = tlen;
            if (s < line.size() && line[s] == ' ')
                ++s;
            return line.substr(s);
        }
        return std::nullopt;
    };

    if (auto p = prefix("PAM_PROMPT_ECHO_OFF")) {
        a.helper.gotPrompt = true;
        g_pAgent->onRequest(*p, false);
        return;
    }
    if (auto p = prefix("PAM_PROMPT_ECHO_ON")) {
        a.helper.gotPrompt = true;
        g_pAgent->onRequest(*p, true);
        return;
    }
    if (auto p = prefix("PAM_ERROR_MSG")) {
        g_pAgent->onError(*p);
        return;
    }
    if (auto p = prefix("PAM_TEXT_INFO")) {
        g_pAgent->onInfo(*p);
        return;
    }
    if (line == "SUCCESS") {
        int status = 0;
        if (a.helper.pid > 0) {
            waitpid(a.helper.pid, &status, 0);
            a.helper.pid = -1;
        }
        completeAuth(true, false);
        return;
    }
    if (line == "FAILURE") {
        const bool hadPrompt = a.helper.gotPrompt;
        const bool wasSocket = (a.helper.mode == HELPER_SOCKET);
        g_pAgent->onError("Authentication failed");
        killHelper(a.helper);
        if (!hadPrompt) {
            if (wasSocket && !a.helper.triedFork) {
                std::print(stderr, "socket helper FAILURE before prompt; retrying via fork+exec\n");
                a.helper.triedFork = true;
                startHelper(a);
                return;
            }
            // helper rejected the conversation outright (account locked, broken PAM
            // stack, etc.). complete as cancelled so polkitd does not re-dispatch
            // BeginAuthentication and reopen an empty dialog forever.
            completeAuth(false, true);
            return;
        }
        startHelper(a);
        return;
    }
}

void CPolkitListener::submitResponse(const std::string& password) {
    if (!m_current || m_current->helper.stdinFd < 0)
        return;
    const std::string line = password + "\n";
    (void)write(m_current->helper.stdinFd, line.data(), line.size());
}

void CPolkitListener::cancelCurrent() {
    if (!m_current)
        return;
    completeAuth(false, true);
}

void CPolkitListener::selectIdentity(const std::string& identityString) {
    if (!m_current)
        return;

    int idx = -1;
    for (size_t i = 0; i < m_current->req.identities.size(); i++) {
        if (m_current->req.identities[i].raw == identityString) {
            idx = (int)i;
            break;
        }
    }
    if (idx < 0 || idx == m_current->selectedIdx)
        return;

    m_current->selectedIdx = idx;
    killHelper(m_current->helper);
    g_pAgent->onError("");
    g_pAgent->onInfo("");
    startHelper(*m_current);
}

void CPolkitListener::killHelper(SHelperProc& h) {
    const bool shared = (h.stdinFd >= 0 && h.stdinFd == h.stdoutFd);
    if (h.stdoutFd >= 0) {
        m_backend->removeFd(h.stdoutFd);
        ::close(h.stdoutFd);
        h.stdoutFd = -1;
    }
    if (h.stdinFd >= 0) {
        if (!shared)
            ::close(h.stdinFd);
        h.stdinFd = -1;
    }
    if (h.pid > 0) {
        kill(h.pid, SIGTERM);
        int status = 0;
        waitpid(h.pid, &status, 0);
        h.pid = -1;
    }
    h.buffer.clear();
}

void CPolkitListener::completeAuth(bool succeeded, bool cancelled) {
    if (!m_current)
        return;

    killHelper(m_current->helper);

    if (m_current->reply) {
        if (succeeded || cancelled)
            m_current->reply->returnResults();
        else
            m_current->reply->returnError(sdbus::Error{sdbus::Error::Name{"org.freedesktop.PolicyKit1.Error.Failed"}, "Authentication failed"});
        m_current->reply.reset();
    }

    g_pAgent->endAuth();

    m_current.reset();
    startNextQueued();
}

void CPolkitListener::rejectActive(const std::string& msg) {
    if (!m_current)
        return;
    if (m_current->reply) {
        m_current->reply->returnError(sdbus::Error{sdbus::Error::Name{"org.freedesktop.PolicyKit1.Error.Failed"}, msg});
        m_current->reply.reset();
    }
    m_current.reset();
    startNextQueued();
}

void CPolkitListener::startNextQueued() {
    if (m_current || m_queue.empty())
        return;
    auto next = std::move(m_queue.front());
    m_queue.pop_front();

    if (next->req.identities.empty()) {
        next->reply->returnError(sdbus::Error{sdbus::Error::Name{"org.freedesktop.PolicyKit1.Error.Failed"}, "No identities"});
        startNextQueued();
        return;
    }
    if (isSessionLocked()) {
        next->reply->returnError(sdbus::Error{sdbus::Error::Name{"org.freedesktop.PolicyKit1.Error.Failed"}, "Session is locked"});
        startNextQueued();
        return;
    }

    const std::string preferred = "unix-user:" + std::to_string((uint32_t)geteuid());
    for (size_t i = 0; i < next->req.identities.size(); i++) {
        if (next->req.identities[i].raw == preferred) {
            next->selectedIdx = (int)i;
            break;
        }
    }

    m_current = std::move(next);
    g_pAgent->beginAuth(m_current->req);
    startHelper(*m_current);
}
