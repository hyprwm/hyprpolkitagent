#include "QMLIntegration.hpp"

#include "core/Agent.hpp"
#include "core/PolkitListener.hpp"

void CQMLIntegration::onExit() {
    g_pAgent->submitResultThreadSafe(result.toStdString());
}

void CQMLIntegration::setResult(QString str) {
    result = str;
    g_pAgent->submitResultThreadSafe(result.toStdString());
}

QString CQMLIntegration::getMessage() {
    return g_pAgent->listener.session.inProgress ? g_pAgent->listener.session.message : "An application is requesting authentication.";
}

QString CQMLIntegration::getUser() {
    return g_pAgent->listener.session.inProgress ? g_pAgent->listener.session.selectedUser.toString() : "an unknown user";
}

void CQMLIntegration::setError(QString str) {
    emit setErrorString(str);
}

void CQMLIntegration::focus() {
    emit focusField();
}

void CQMLIntegration::setInputBlocked(bool blocked) {
    emit blockInput(blocked);
}
