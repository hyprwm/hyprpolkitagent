#pragma once

#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QScreen>

#include "PolkitListener.hpp"
#include <polkitqt1-subject.h>

#include <hyprutils/memory/WeakPtr.hpp>
using namespace Hyprutils::Memory;
#define SP CSharedPointer
#define WP CWeakPointer

class CQMLIntegration;
class CSigDaemon;

class CAgent {
  public:
    CAgent();
    ~CAgent();

    void submitResultThreadSafe(const std::string& result);
    void resetAuthState();
    bool start();
    void initAuthPrompt();
    void setAuthError(const QString& err);

  private:
    struct {
        bool                      authing = false;
        SP<QQmlApplicationEngine> qmlEngine;
        SP<CQMLIntegration>       qmlIntegration;
    } authState;

    struct {
        std::mutex  resultMutex;
        std::string result;
        bool        used = true;
    } lastAuthResult;

    CPolkitListener                   listener;
    SP<CSigDaemon>                    sigDaemon;
    SP<PolkitQt1::UnixSessionSubject> sessionSubject;

    bool                              resultReady();

    friend class CQMLIntegration;
};

inline std::unique_ptr<CAgent> g_pAgent;