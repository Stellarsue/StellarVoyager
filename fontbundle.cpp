#include "fontbundle.h"
#include "themefonts.h"

#include <QApplication>
#include <QCryptographicHash>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QLabel>
#include <QMap>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QRawFont>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTextBrowser>
#include <QVBoxLayout>

namespace {
const QString packagePath = QStringLiteral(":/stellarsue/fonts/times32.exe");
const QString licensePath = QStringLiteral(":/stellarsue/fonts/EULA.html");
const QByteArray packageHash = "db56595ec6ef5d3de5c24994f001f03b2a13e37cee27bc25c58f6f43e8f807ab";
const QStringList fontNames = {QStringLiteral("Times.TTF"), QStringLiteral("Timesbd.TTF"),
                               QStringLiteral("Timesbi.TTF"), QStringLiteral("Timesi.TTF")};

QByteArray readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

bool saveFile(const QString &path, const QByteArray &bytes)
{
    QSaveFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit();
}

bool runProcess(const QString &program, const QStringList &arguments, QString *error)
{
    QProcess process;
    process.start(program, arguments);
    if (!process.waitForStarted(3000)) {
        *error = process.errorString();
        return false;
    }
    if (!process.waitForFinished(15000)) {
        process.kill();
        process.waitForFinished(1000);
        *error = QStringLiteral("操作超时");
        return false;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        *error = QString::fromLocal8Bit(process.readAllStandardError()).left(1500);
        return false;
    }
    return true;
}
}

bool FontBundle::available()
{
#if !defined(Q_OS_LINUX) && !defined(STELLARSUE_TESTING)
    return false;
#else
    return QFile::exists(packagePath) && QFile::exists(licensePath);
#endif
}

QString FontBundle::installDirectory()
{
#if defined(STELLARSUE_TESTING)
    const QString testDataHome = qEnvironmentVariable("STELLARSUE_TEST_DATA_HOME");
    if (!testDataHome.isEmpty())
        return QDir(testDataHome).filePath(QStringLiteral("fonts/stellarsue-times"));
#endif
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
           + QStringLiteral("/fonts/stellarsue-times");
}

FontBundle::InstallResult FontBundle::installForCurrentUser(bool licenseAccepted)
{
    InstallResult result;
#if !defined(Q_OS_LINUX) && !defined(STELLARSUE_TESTING)
    Q_UNUSED(licenseAccepted);
    result.error = QStringLiteral("共享字体安装当前仅支持 Linux。");
    return result;
#else
    if (!licenseAccepted) {
        result.error = QStringLiteral("尚未接受微软原始字体许可，未安装任何字体。");
        return result;
    }
    const QByteArray package = readFile(packagePath);
    const QByteArray license = readFile(licensePath);
    if (QCryptographicHash::hash(package, QCryptographicHash::Sha256).toHex() != packageHash
        || !license.contains("END-USER LICENSE AGREEMENT")) {
        result.error = QStringLiteral("内置字体包缺失或校验失败，未安装字体。");
        return result;
    }

    QString extractor = QStandardPaths::findExecutable(QStringLiteral("cabextract"));
    const bool isCabextract = !extractor.isEmpty();
    if (extractor.isEmpty()) extractor = QStandardPaths::findExecutable(QStringLiteral("bsdtar"));
    if (extractor.isEmpty()) {
        result.error = QStringLiteral("需要 cabextract 或 bsdtar 解压原始字体包。\n"
                                      "Ubuntu / Debian：sudo apt install cabextract\n"
                                      "安装依赖后，可从托盘菜单重试。不会执行 Windows 安装程序。");
        return result;
    }
    QTemporaryDir staging;
    const QString archive = staging.filePath(QStringLiteral("times32.exe"));
    if (!staging.isValid() || !saveFile(archive, package)) {
        result.error = QStringLiteral("无法创建字体安装临时文件。");
        return result;
    }
    const QStringList arguments = isCabextract
        ? QStringList{QStringLiteral("-q"), QStringLiteral("-d"), staging.path(),
                      QStringLiteral("-F"), QStringLiteral("Times*.TTF"), archive}
        : QStringList{QStringLiteral("-xf"), archive, QStringLiteral("-C"), staging.path()} + fontNames;
    if (!runProcess(extractor, arguments, &result.error)) return result;

    QMap<QString, QByteArray> files;
    for (const QString &name : fontNames) {
        const QByteArray bytes = readFile(staging.filePath(name));
        const QRawFont font(bytes, 16, QFont::PreferDefaultHinting);
        if (!font.isValid() || font.familyName() != QStringLiteral("Times New Roman")) {
            result.error = QStringLiteral("字体解压或校验失败：%1").arg(name);
            return result;
        }
        files.insert(name, bytes);
    }
    files.insert(QStringLiteral("EULA.html"), license);
    const QString directory = installDirectory();
    // Do not silently replace another version or follow a destination symlink.
    if (QFileInfo(directory).isSymLink()) {
        result.error = QStringLiteral("字体目标目录是符号链接，已停止安装：%1").arg(directory);
        return result;
    }
    for (auto it = files.cbegin(); it != files.cend(); ++it) {
        const QString destination = directory + QLatin1Char('/') + it.key();
        const QFileInfo info(destination);
        if (info.isSymLink() || (info.exists() && readFile(destination) != it.value())) {
            result.error = QStringLiteral("已有不同文件，未覆盖：%1").arg(destination);
            return result;
        }
    }
    if (!QDir().mkpath(directory)) {
        result.error = QStringLiteral("无法创建用户字体目录：%1").arg(directory);
        return result;
    }
    for (auto it = files.cbegin(); it != files.cend(); ++it) {
        const QString destination = directory + QLatin1Char('/') + it.key();
        if (!QFile::exists(destination) && !saveFile(destination, it.value())) {
            result.error = QStringLiteral("无法写入字体文件：%1").arg(destination);
            return result;
        }
    }

    const QString cache = QStandardPaths::findExecutable(QStringLiteral("fc-cache"));
    QString cacheError;
    if (cache.isEmpty() || !runProcess(cache, {QStringLiteral("-f"), directory}, &cacheError))
        result.warning = QStringLiteral("字体已保存，但字体缓存刷新未完成。请运行 fc-cache -f 并重启其他程序。\n%1").arg(cacheError);
    // Make the newly installed files visible immediately in this Qt process too.
    for (const QString &name : fontNames) {
        if (QFontDatabase::addApplicationFont(directory + QLatin1Char('/') + name) < 0) {
            result.error = QStringLiteral("字体已保存，但当前应用加载失败：%1").arg(name);
            return result;
        }
    }
    QApplication::setFont(ThemeFonts::text());
    result.success = true;
    result.directory = directory;
    return result;
#endif
}

void FontBundle::offerInstallation(QWidget *parent, bool onlyIfMissing)
{
    if (!available()) return;
    QSettings settings;
    if (onlyIfMissing && (QFontDatabase::families().contains(QStringLiteral("Times New Roman"))
                          || settings.value(QStringLiteral("fonts/timesPrompted"), false).toBool()))
        return;

    QDialog dialog(parent);
    dialog.setWindowTitle(QStringLiteral("StellarVoyager · 安装共享字体"));
    dialog.resize(620, 510);
    dialog.setStyleSheet(QStringLiteral(
        "QDialog { background: #101e2c; color: #e7edf4; }"
        "QLabel { color: #e7edf4; background: transparent; }"
        "QTextBrowser { background: #f4f6f8; color: #172331; border: 1px solid #405665; padding: 8px; }"
        "QPushButton { color: #e7edf4; background: #243e50; border: 1px solid #527086; padding: 7px 12px; border-radius: 4px; }"
        "QPushButton:hover { background: #30536a; }"));
    auto *layout = new QVBoxLayout(&dialog);
    auto *description = new QLabel(QStringLiteral(
        "内置原始 Times New Roman 安装包，包含常规、粗体、斜体、粗斜体。\n"
        "同意许可后，仅安装到当前用户的字体目录，其他桌面程序也可使用。\n"
        "无需 root 或网络；不会更改其他程序的默认字体。\n"
        "此字体包不得随收费产品分发。请阅读以下微软原始许可。"), &dialog);
    description->setWordWrap(true);
    layout->addWidget(description);
    auto *license = new QTextBrowser(&dialog);
    license->setHtml(QString::fromUtf8(readFile(licensePath)));
    layout->addWidget(license);
    auto *buttons = new QDialogButtonBox(&dialog);
    auto *accept = buttons->addButton(QStringLiteral("同意许可并安装"), QDialogButtonBox::AcceptRole);
    auto *cancel = buttons->addButton(QStringLiteral("暂不安装"), QDialogButtonBox::RejectRole);
    cancel->setDefault(true);
    accept->setAutoDefault(false);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    const bool accepted = dialog.exec() == QDialog::Accepted;
    settings.setValue(QStringLiteral("fonts/timesPrompted"), true);
    if (!accepted) return;

    const InstallResult result = installForCurrentUser(true);
    if (!result.success) {
        QMessageBox::warning(parent, QStringLiteral("字体未安装完成"), result.error);
        return;
    }
    QMessageBox::information(parent, QStringLiteral("字体已安装"),
        QStringLiteral("Times New Roman 四种字形已安装到：\n%1\n\n"
                       "StellarVoyager 已启用。其他桌面程序重启后可在字体列表中选择。\n%2")
            .arg(result.directory, result.warning));
}
