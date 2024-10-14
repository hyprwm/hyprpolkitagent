#pragma once

#include <QApplication>
#include <QObject>
#include <QSocketNotifier>

class CSigDaemon : public QObject {
  Q_OBJECT;
  public:
    CSigDaemon(QObject* parent = nullptr);

    static void onSignal(int signo);

  public slots:
    void handleSigHup();
    void handleSigTerm();
    void handleSigInt();

  private:
    QSocketNotifier* snHup  = nullptr;
    QSocketNotifier* snTerm = nullptr;
    QSocketNotifier* snInt  = nullptr;
};
