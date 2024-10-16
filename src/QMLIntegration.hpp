#pragma once

#include <QObject>
#include <QQmlApplicationEngine>
#include <QPixmap>
#include <QIcon>

class CQMLIntegration : public QObject {
    Q_OBJECT;
    Q_PROPERTY(QString errorText MEMBER errorText);

  public:
    explicit CQMLIntegration(QObject* parent = nullptr) : QObject(parent) {
        ;
    }
    virtual ~CQMLIntegration() {
        ;
    }

    void                setError(QString str);
    void                focus();
    void                setInputBlocked(bool blocked);

    QString             result = "fail", errorText = "";

    Q_INVOKABLE QString getMessage();
    Q_INVOKABLE QString getUser();

    Q_INVOKABLE void    setResult(QString str);

  public slots:
    void onExit();

  signals:
    void setErrorString(QString err);
    void focusField();
    void blockInput(bool block);
};
