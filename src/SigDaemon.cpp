#include "SigDaemon.hpp"
#include "core/Agent.hpp"

#include <print>

#include <sys/socket.h>
#include <sys/signal.h>

static int sighupFd[2];
static int sigtermFd[2];
static int sigintFd[2];

//
void CSigDaemon::onSignal(int signo) {
    char a = 1;
    if (signo == SIGHUP)
        ::write(sighupFd[0], &a, sizeof(a));
    else if (signo == SIGINT)
        ::write(sigintFd[0], &a, sizeof(a));
    else if (signo == SIGTERM)
        ::write(sigtermFd[0], &a, sizeof(a));
}

CSigDaemon::CSigDaemon(QObject* parent) : QObject(parent) {
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sighupFd))
        qFatal("Couldn't create HUP socketpair");
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sigtermFd))
        qFatal("Couldn't create TERM socketpair");
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sigintFd))
        qFatal("Couldn't create INT socketpair");
    snHup = new QSocketNotifier(sighupFd[1], QSocketNotifier::Read, this);
    connect(snHup, SIGNAL(activated(QSocketDescriptor)), this, SLOT(handleSigHup()));
    snTerm = new QSocketNotifier(sigtermFd[1], QSocketNotifier::Read, this);
    connect(snTerm, SIGNAL(activated(QSocketDescriptor)), this, SLOT(handleSigTerm()));
    snInt = new QSocketNotifier(sigintFd[1], QSocketNotifier::Read, this);
    connect(snInt, SIGNAL(activated(QSocketDescriptor)), this, SLOT(handleSigInt()));

    struct sigaction sa;

    sa.sa_handler = onSignal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_flags |= SA_RESTART;

    if (sigaction(SIGHUP, &sa, 0))
        std::print(stderr, "sigaction for hup failed\n");

    if (sigaction(SIGTERM, &sa, 0))
        std::print(stderr, "sigaction for term failed\n");

    if (sigaction(SIGINT, &sa, 0))
        std::print(stderr, "sigaction for int failed\n");
}

void CSigDaemon::handleSigHup() {
    std::print("> signal received: SIGHUP\n");
    snHup->setEnabled(false);
    char tmp;
    ::read(sighupFd[1], &tmp, sizeof(tmp));
    g_pAgent->resetAuthState();
    snHup->setEnabled(true);
}

void CSigDaemon::handleSigInt() {
    std::print("> signal received: SIGINT\n");
    snInt->setEnabled(false);
    char tmp;
    ::read(sigintFd[1], &tmp, sizeof(tmp));
    g_pAgent->resetAuthState();
    snInt->setEnabled(true);
    exit(0);
}

void CSigDaemon::handleSigTerm() {
    std::print("> signal received: SIGTERM\n");
    snTerm->setEnabled(false);
    char tmp;
    ::read(sigtermFd[1], &tmp, sizeof(tmp));
    g_pAgent->resetAuthState();
    snTerm->setEnabled(true);
}
