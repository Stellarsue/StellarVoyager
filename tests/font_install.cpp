#include "fontbundle.h"

#include <QApplication>
#include <QDir>
#include <QDialog>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QRawFont>
#include <QSettings>
#include <QTemporaryDir>
#include <QTextBrowser>
#include <QTimer>

namespace {
void require(bool ok, const char *message)
{
    if (!ok) qFatal("Font install test failed: %s", message);
}
}

int main(int argc, char **argv)
{
    QTemporaryDir sandbox;
    require(sandbox.isValid(), "isolated data directory");
    qputenv("STELLARSUE_TEST_DATA_HOME", sandbox.path().toUtf8() + "/data");
    qputenv("XDG_DATA_HOME", sandbox.path().toUtf8() + "/data");
    qputenv("XDG_CACHE_HOME", sandbox.path().toUtf8() + "/cache");
    qputenv("XDG_CONFIG_HOME", sandbox.path().toUtf8() + "/config");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                       sandbox.filePath(QStringLiteral("config")));
    QApplication app(argc, argv);
    app.setOrganizationName("StellarsueTests");
    app.setApplicationName("FontInstallation");
    app.setQuitOnLastWindowClosed(false);
    require(FontBundle::available(), "original installer and EULA embedded");
    require(!FontBundle::installForCurrentUser(false).success, "consent required");
    require(!QFile::exists(FontBundle::installDirectory()), "declining writes no font files");
    QTimer::singleShot(100, [&] {
        auto *dialog = qobject_cast<QDialog *>(app.activeModalWidget());
        require(dialog, "interactive consent dialog");
        auto *license = dialog->findChild<QTextBrowser *>();
        require(license && license->toPlainText().contains("END-USER LICENSE AGREEMENT"),
                "original license visible before consent");
        dialog->reject();
    });
    FontBundle::offerInstallation(nullptr, false);
    require(!QFile::exists(FontBundle::installDirectory()), "interactive decline installs nothing");
    const QByteArray oldPath = qgetenv("PATH");
    qputenv("PATH", sandbox.path().toUtf8());
    const auto missingExtractor = FontBundle::installForCurrentUser(true);
    require(!missingExtractor.success && missingExtractor.error.contains("cabextract"),
            "missing dependency produces actionable error");
    require(!QFile::exists(FontBundle::installDirectory()), "missing dependency does not leave a partial install");
    qputenv("PATH", oldPath);
    const auto result = FontBundle::installForCurrentUser(true);
    if (!result.success) qWarning("%s", qPrintable(result.error));
    require(result.success && result.warning.isEmpty(), "offline installation and font cache refresh");
    require(result.directory.startsWith(sandbox.path()), "only isolated user data is modified");
    const QStringList names = {"Times.TTF", "Timesbd.TTF", "Timesbi.TTF", "Timesi.TTF"};
    for (const auto &name : names) {
        const QRawFont font(result.directory + '/' + name, 16);
        require(font.isValid() && font.familyName() == "Times New Roman", "all four original fonts usable");
    }
    require(QFile::exists(result.directory + "/EULA.html"), "original license retained");
    require(QDir(result.directory).entryList(QDir::Files).size() == 5, "only four fonts and license installed");
    const QString regular = result.directory + "/Times.TTF";
    const auto modified = QFileInfo(regular).lastModified();
    require(FontBundle::installForCurrentUser(true).success, "repeat installation succeeds");
    require(QFileInfo(regular).lastModified() == modified, "repeat does not replace existing identical files");
    QFile conflict(regular);
    require(conflict.open(QIODevice::WriteOnly | QIODevice::Truncate), "conflict fixture");
    conflict.write("existing user file");
    conflict.close();
    require(!FontBundle::installForCurrentUser(true).success, "conflicting existing files preserved");
    require(conflict.open(QIODevice::ReadOnly) && conflict.readAll() == "existing user file", "no clobber");
    qInfo("PASS: bundled installer, explicit consent, dependency error, isolated offline install, four faces, license, cache, idempotence, conflict protection.");
}
