#pragma once

#include <QString>

class QWidget;

namespace FontBundle {
struct InstallResult {
    bool success = false;
    QString directory;
    QString error;
    QString warning;
};

bool available();
QString installDirectory();
// Consent is required even for non-interactive installation.
InstallResult installForCurrentUser(bool licenseAccepted);
void offerInstallation(QWidget *parent = nullptr, bool onlyIfMissing = true);
}
