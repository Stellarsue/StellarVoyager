#include "statusdashboard.h"

#include "themeicons.h"

#include <QApplication>
#include <QFrame>
#include <QFontDatabase>
#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QProgressBar>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QStorageInfo>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVector>

#include <algorithm>
#include <cmath>

namespace
{
constexpr int kPopupWidth = 456;
constexpr int kGraphSamples = 48;

const QColor kInk(QStringLiteral("#24272B"));
const QColor kMuted(QStringLiteral("#767C82"));
const QColor kBlue(QStringLiteral("#3CAEF0"));
const QColor kBlueText(QStringLiteral("#137DAF"));
const QColor kRed(QStringLiteral("#E64B5D"));

QFont dashboardUiFont()
{
    QFont result = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
#if defined(Q_OS_MACOS)
    result.setFamilies({QStringLiteral("Helvetica Neue"), QStringLiteral("Arial")});
#elif defined(Q_OS_WIN)
    result.setFamilies({QStringLiteral("Segoe UI"), QStringLiteral("Arial")});
#else
    result.setFamilies({QStringLiteral("Noto Sans"), QStringLiteral("DejaVu Sans"),
                        QStringLiteral("Liberation Sans")});
#endif
    result.setStyleHint(QFont::SansSerif);
    return result;
}

QFont fontAt(qreal size, QFont::Weight weight = QFont::Normal)
{
    QFont result = dashboardUiFont();
    result.setPointSizeF(size);
    result.setWeight(weight);
    return result;
}

QLabel *label(QWidget *parent, const QString &text, qreal size = 10.0,
              const QColor &color = kInk, QFont::Weight weight = QFont::Normal)
{
    auto *result = new QLabel(text, parent);
    result->setFont(fontAt(size, weight));
    result->setStyleSheet(QStringLiteral("color: %1; background: transparent;").arg(color.name()));
    result->setMinimumWidth(0);
    return result;
}

QFrame *card(QWidget *parent, const QString &objectName, const QString &accessibleName)
{
    auto *result = new QFrame(parent);
    result->setObjectName(objectName);
    result->setAccessibleName(accessibleName);
    result->setFrameShape(QFrame::NoFrame);
    result->setStyleSheet(QStringLiteral("background-color: #F4F5F6; border-radius: 17px;"));
    return result;
}

QHBoxLayout *heading(QFrame *parent, ThemeIcon glyph, const QString &title,
                     QLabel **titleLabel = nullptr)
{
    auto *row = new QHBoxLayout;
    row->setSpacing(7);
    auto *icon = new QLabel(parent);
    icon->setPixmap(ThemeIcons::pixmap(glyph, QSize(19, 19), kMuted));
    icon->setFixedSize(19, 19);
    icon->setAccessibleName(title + QStringLiteral("图标"));
    row->addWidget(icon);

    auto *text = label(parent, title, 13.0, kInk, QFont::Medium);
    text->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    row->addWidget(text, 1);
    if (titleLabel) *titleLabel = text;
    return row;
}

QProgressBar *capacityBar(QWidget *parent, const QString &objectName)
{
    auto *bar = new QProgressBar(parent);
    bar->setObjectName(objectName);
    bar->setRange(0, 1000);
    bar->setValue(0);
    bar->setTextVisible(false);
    bar->setFixedHeight(8);
    bar->setAccessibleName(QStringLiteral("已使用容量"));
    return bar;
}

void setCapacity(QProgressBar *bar, double percent, bool valid)
{
    bar->setValue(valid ? qRound(std::clamp(percent, 0.0, 100.0) * 10.0) : 0);
    bar->setAccessibleDescription(valid ? formatPercent(percent) : QStringLiteral("N/A"));
}

QString readableBytes(quint64 bytes)
{
    // Format in decimal gigabytes to agree with the unit and figures shown in
    // the reference panel, while still using the real measured byte count.
    const double gigabytes = static_cast<double>(bytes) / 1000000000.0;
    return QStringLiteral("%1GB").arg(gigabytes, 0, 'f', 1);
}

QPixmap fanIcon()
{
    QPixmap image(20, 20);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(kMuted, 1.5));
    painter.setBrush(Qt::NoBrush);
    painter.translate(10, 10);
    for (int index = 0; index < 4; ++index) {
        painter.save();
        painter.rotate(index * 90.0);
        painter.drawEllipse(QRectF(-2.5, -8.0, 5.0, 6.8));
        painter.restore();
    }
    painter.setBrush(QColor(QStringLiteral("#D7DADD")));
    painter.drawEllipse(QPointF(0, 0), 2.3, 2.3);
    return image;
}

void addLegendRow(QVBoxLayout *layout, QWidget *parent, const QColor &color,
                  const QString &caption, QLabel **value)
{
    auto *row = new QHBoxLayout;
    row->setSpacing(7);
    auto *swatch = new QLabel(parent);
    swatch->setFixedSize(10, 10);
    swatch->setStyleSheet(QStringLiteral("background: %1; border-radius: 2px;")
                              .arg(color.name()));
    row->addWidget(swatch);
    row->addWidget(label(parent, caption, 10.0), 1);
    *value = label(parent, QStringLiteral("N/A"), 10.0, kInk, QFont::Medium);
    (*value)->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    row->addWidget(*value);
    layout->addLayout(row);
}
} // namespace

class DashboardCpuGraph final : public QWidget
{
public:
    explicit DashboardCpuGraph(QWidget *parent = nullptr) : QWidget(parent)
    {
        setObjectName(QStringLiteral("cpuHistoryGraph"));
        setAccessibleName(QStringLiteral("CPU 使用率历史曲线"));
        setMinimumHeight(54);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

    void addSample(double user, double system, double total)
    {
        const bool splitValid = user >= 0.0 && system >= 0.0;
        m_user.append(std::clamp(splitValid ? user : total, 0.0, 100.0));
        m_system.append(splitValid ? std::clamp(system, 0.0, 100.0) : 0.0);
        while (m_user.size() > kGraphSamples) {
            m_user.removeFirst();
            m_system.removeFirst();
        }
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QRectF box = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        QPainterPath clip;
        clip.addRoundedRect(box, 9.0, 9.0);
        painter.fillPath(clip, QColor(QStringLiteral("#E5E7E9")));
        painter.setClipPath(clip);

        painter.setPen(QPen(QColor(QStringLiteral("#D6DADF")), 1.0));
        for (int step = 1; step < 4; ++step) {
            const qreal y = box.bottom() - box.height() * step / 4.0;
            painter.drawLine(QPointF(box.left(), y), QPointF(box.right(), y));
        }

        if (m_user.isEmpty()) return;
        const qreal right = box.right();
        const qreal bottom = box.bottom();
        const qreal unitX = box.width() / (kGraphSamples - 1);

        auto area = [&](bool stacked) {
            QPainterPath path;
            const qreal startX = right - unitX * (m_user.size() - 1);
            path.moveTo(startX, bottom);
            for (int index = 0; index < m_user.size(); ++index) {
                const qreal value = stacked
                                        ? std::min(100.0, m_user.at(index) + m_system.at(index))
                                        : m_user.at(index);
                path.lineTo(startX + index * unitX,
                            bottom - box.height() * value / 100.0);
            }
            path.lineTo(right, bottom);
            path.closeSubpath();
            return path;
        };
        painter.fillPath(area(true), kRed);
        painter.fillPath(area(false), kBlue);
    }

private:
    QVector<double> m_user;
    QVector<double> m_system;
};

StatusDashboard::StatusDashboard(QWidget *parent)
    : QWidget(parent, Qt::Popup | Qt::FramelessWindowHint)
{
    setObjectName(QStringLiteral("statusDashboard"));
    setWindowTitle(QStringLiteral("StellarVoyager · 系统状态"));
    setAccessibleName(QStringLiteral("StellarVoyager 系统状态面板"));
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose, false);
    const QFont uiFont = dashboardUiFont();
    setFont(uiFont);

    auto *windowLayout = new QVBoxLayout(this);
    windowLayout->setContentsMargins(0, 0, 0, 0);
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setObjectName(QStringLiteral("dashboardScrollArea"));
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setWidgetResizable(false);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setMinimumSize(0, 0);
    m_scrollArea->viewport()->setAutoFillBackground(false);
    m_scrollArea->viewport()->setStyleSheet(QStringLiteral("background: transparent;"));
    windowLayout->addWidget(m_scrollArea);

    m_scrollContent = new QWidget;
    // Labels below set an explicit size from their parent font while they are
    // constructed, before this widget is reparented into QScrollArea.
    m_scrollContent->setFont(uiFont);
    m_scrollContent->setObjectName(QStringLiteral("dashboardScrollContent"));
    auto *contentLayout = new QVBoxLayout(m_scrollContent);
    contentLayout->setContentsMargins(7, 7, 7, 7);
    auto *surface = new QFrame(m_scrollContent);
    surface->setObjectName(QStringLiteral("dashboardSurface"));
    surface->setFrameShape(QFrame::NoFrame);
    surface->setStyleSheet(QStringLiteral("background-color: #D2D5D8; border-radius: 21px;"));
    contentLayout->addWidget(surface);

    auto *shadow = new QGraphicsDropShadowEffect(surface);
    shadow->setBlurRadius(19);
    shadow->setOffset(0, 4);
    shadow->setColor(QColor(25, 35, 45, 65));
    surface->setGraphicsEffect(shadow);

    auto *surfaceLayout = new QVBoxLayout(surface);
    surfaceLayout->setContentsMargins(10, 6, 10, 8);
    surfaceLayout->setSpacing(3);

    auto *toolbar = new QHBoxLayout;
    toolbar->setContentsMargins(5, 0, 0, 0);
    toolbar->setSpacing(0);
    auto *caption = label(surface, QStringLiteral("实时监控"), 9.0, kMuted,
                          QFont::Medium);
    toolbar->addWidget(caption);
    toolbar->addStretch(1);
    m_settingsButton = new QToolButton(surface);
    m_settingsButton->setObjectName(QStringLiteral("dashboardSettingsButton"));
    m_settingsButton->setIcon(ThemeIcons::icon(ThemeIcon::Settings,
                                              QColor(QStringLiteral("#65747E"))));
    m_settingsButton->setIconSize(QSize(16, 16));
    m_settingsButton->setFixedSize(27, 27);
    m_settingsButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_settingsButton->setPopupMode(QToolButton::InstantPopup);
    m_settingsButton->setCursor(Qt::PointingHandCursor);
    m_settingsButton->setAccessibleName(QStringLiteral("设置与功能菜单"));
    m_settingsButton->setToolTip(QStringLiteral("设置与功能"));
    m_settingsButton->setStyleSheet(QStringLiteral(
        "QToolButton#dashboardSettingsButton {"
        " background-color: #E8ECEF; border: 1px solid #C8CFD4;"
        " border-radius: 8px; padding: 3px; }"
        "QToolButton#dashboardSettingsButton:hover { background-color: #FFFFFF; }"
        "QToolButton#dashboardSettingsButton:pressed { background-color: #D8E3EA; }"
        "QToolButton#dashboardSettingsButton::menu-indicator {"
        " image: none; width: 0px; height: 0px; }"));
    toolbar->addWidget(m_settingsButton);
    surfaceLayout->addLayout(toolbar);

    auto *grid = new QGridLayout;
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(8);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    surfaceLayout->addLayout(grid);

    auto *cpuCard = card(surface, QStringLiteral("cpuCard"), QStringLiteral("CPU 状态"));
    auto *cpu = new QVBoxLayout(cpuCard);
    cpu->setContentsMargins(10, 8, 10, 8);
    cpu->setSpacing(4);
    cpu->addLayout(heading(cpuCard, ThemeIcon::Cpu, QStringLiteral("CPU")));
    auto *cpuSubline = new QHBoxLayout;
    cpuSubline->setSpacing(3);
    m_cpuModel = label(cpuCard, QStringLiteral("N/A"), 9.5);
    m_cpuModel->setObjectName(QStringLiteral("cpuModel"));
    m_cpuModel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    cpuSubline->addWidget(m_cpuModel, 1);
    m_cpuTemperature = label(cpuCard, QStringLiteral("N/A"), 10.0, kBlueText, QFont::Medium);
    m_cpuTemperature->setObjectName(QStringLiteral("cpuTemperature"));
    m_cpuTemperature->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    cpuSubline->addWidget(m_cpuTemperature);
    cpu->addLayout(cpuSubline);
    m_cpuGraph = new DashboardCpuGraph(cpuCard);
    cpu->addWidget(m_cpuGraph, 1);
    addLegendRow(cpu, cpuCard, kBlue, QStringLiteral("用户"), &m_cpuUser);
    addLegendRow(cpu, cpuCard, kRed, QStringLiteral("系统"), &m_cpuSystem);
    addLegendRow(cpu, cpuCard, QColor(QStringLiteral("#25282D")),
                 QStringLiteral("空闲"), &m_cpuIdle);
    cpuCard->setMinimumHeight(190);
    grid->addWidget(cpuCard, 0, 0, 2, 1);

    auto *storageCard = card(surface, QStringLiteral("storageCard"), QStringLiteral("磁盘状态"));
    auto *storage = new QVBoxLayout(storageCard);
    storage->setContentsMargins(10, 8, 10, 8);
    storage->setSpacing(4);
    storage->addLayout(heading(storageCard, ThemeIcon::Storage,
                               QStringLiteral("系统磁盘"), &m_storageName));
    m_storageCapacity = label(storageCard, QStringLiteral("可用 N/A / N/A"), 9.4, kBlueText);
    m_storageCapacity->setObjectName(QStringLiteral("storageCapacity"));
    m_storageCapacity->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    storage->addWidget(m_storageCapacity);
    m_storageBar = capacityBar(storageCard, QStringLiteral("storageUsageBar"));
    storage->addWidget(m_storageBar);
    auto *diskRates = new QHBoxLayout;
    diskRates->setSpacing(4);
    m_storageRead = label(storageCard, QStringLiteral("R: N/A"), 8.5, kBlueText);
    m_storageWrite = label(storageCard, QStringLiteral("W: N/A"), 8.5, kBlueText);
    m_storageRead->setObjectName(QStringLiteral("storageRead"));
    m_storageWrite->setObjectName(QStringLiteral("storageWrite"));
    diskRates->addWidget(m_storageRead);
    diskRates->addWidget(m_storageWrite, 1, Qt::AlignRight);
    storage->addLayout(diskRates);
    storageCard->setMinimumHeight(91);
    grid->addWidget(storageCard, 0, 1);

    auto *memoryCard = card(surface, QStringLiteral("memoryCard"), QStringLiteral("内存状态"));
    auto *memory = new QVBoxLayout(memoryCard);
    memory->setContentsMargins(10, 8, 10, 8);
    memory->setSpacing(4);
    memory->addLayout(heading(memoryCard, ThemeIcon::Memory, QStringLiteral("RAM")));
    m_memoryCapacity = label(memoryCard, QStringLiteral("可用 N/A / N/A"), 9.4, kBlueText);
    m_memoryCapacity->setObjectName(QStringLiteral("memoryCapacity"));
    m_memoryCapacity->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    memory->addWidget(m_memoryCapacity);
    m_memoryBar = capacityBar(memoryCard, QStringLiteral("memoryUsageBar"));
    memory->addWidget(m_memoryBar);
    memory->addStretch(1);
    memoryCard->setMinimumHeight(91);
    grid->addWidget(memoryCard, 1, 1);

    auto *fanCard = card(surface, QStringLiteral("fanCard"), QStringLiteral("风扇状态"));
    auto *fan = new QVBoxLayout(fanCard);
    fan->setContentsMargins(10, 8, 10, 8);
    fan->setSpacing(5);
    auto *fanHeading = new QHBoxLayout;
    fanHeading->setSpacing(7);
    auto *fanGlyph = new QLabel(fanCard);
    fanGlyph->setPixmap(fanIcon());
    fanGlyph->setFixedSize(20, 20);
    fanHeading->addWidget(fanGlyph);
    fanHeading->addWidget(label(fanCard, QStringLiteral("FAN"), 13.0,
                                kInk, QFont::Medium), 1);
    fan->addLayout(fanHeading);
    m_fanSpeed = label(fanCard, QStringLiteral("N/A"), 13.0, kInk, QFont::Medium);
    m_fanSpeed->setObjectName(QStringLiteral("fanSpeed"));
    fan->addWidget(m_fanSpeed);
    fan->addStretch(1);
    fanCard->setMinimumHeight(78);
    grid->addWidget(fanCard, 2, 0);

    auto *networkCard = card(surface, QStringLiteral("networkCard"), QStringLiteral("网络状态"));
    auto *network = new QVBoxLayout(networkCard);
    network->setContentsMargins(10, 8, 10, 8);
    network->setSpacing(5);
    network->addLayout(heading(networkCard, ThemeIcon::Network,
                               QStringLiteral("INTERNET")));
    auto *rates = new QHBoxLayout;
    rates->setSpacing(4);
    m_networkDownload = label(networkCard, QStringLiteral("↓ N/A"), 9.4);
    m_networkUpload = label(networkCard, QStringLiteral("↑ N/A"), 9.4);
    m_networkDownload->setObjectName(QStringLiteral("networkDownload"));
    m_networkUpload->setObjectName(QStringLiteral("networkUpload"));
    rates->addWidget(m_networkDownload);
    rates->addWidget(m_networkUpload, 1, Qt::AlignRight);
    network->addLayout(rates);
    m_networkAddress = label(networkCard, QStringLiteral("IP N/A"), 9.4, kBlueText);
    m_networkAddress->setObjectName(QStringLiteral("networkAddress"));
    network->addWidget(m_networkAddress);
    networkCard->setMinimumHeight(78);
    grid->addWidget(networkCard, 2, 1);

    setStyleSheet(QStringLiteral(R"(
        QScrollArea#dashboardScrollArea,
        QWidget#dashboardScrollContent {
            background: transparent;
            border: none;
        }
        QFrame#dashboardSurface {
            background-color: #D2D5D8;
            border-radius: 21px;
        }
        QFrame#cpuCard, QFrame#storageCard, QFrame#memoryCard,
        QFrame#fanCard, QFrame#networkCard {
            background-color: #F4F5F6;
            border-radius: 17px;
        }
        QProgressBar#storageUsageBar, QProgressBar#memoryUsageBar {
            border: none;
            border-radius: 4px;
            background-color: #D7DBDE;
        }
        QProgressBar#storageUsageBar::chunk, QProgressBar#memoryUsageBar::chunk {
            border-radius: 4px;
            background-color: #228FF2;
        }
    )"));
    m_scrollContent->setFixedWidth(kPopupWidth);
    m_scrollContent->adjustSize();
    m_scrollArea->setWidget(m_scrollContent);
    m_scrollContent->setAutoFillBackground(false);
    resize(kPopupWidth, m_scrollContent->height());
}

void StatusDashboard::updateSnapshot(const SystemSnapshot &snapshot)
{
    m_cpuGraph->addSample(snapshot.cpuUserPercent, snapshot.cpuSystemPercent,
                          snapshot.cpuUsagePercent);
    m_cpuUser->setText(snapshot.cpuUserPercent >= 0.0
                           ? formatPercent(snapshot.cpuUserPercent) : QStringLiteral("N/A"));
    m_cpuSystem->setText(snapshot.cpuSystemPercent >= 0.0
                             ? formatPercent(snapshot.cpuSystemPercent) : QStringLiteral("N/A"));
    m_cpuIdle->setText(snapshot.cpuIdlePercent >= 0.0
                           ? formatPercent(snapshot.cpuIdlePercent) : QStringLiteral("N/A"));
    m_cpuGraph->setAccessibleDescription(
        QStringLiteral("当前 CPU 使用率 %1").arg(formatPercent(snapshot.cpuUsagePercent)));

    const QString model = snapshot.cpuModel.trimmed();
    m_cpuModel->setText(model.isEmpty() ? QStringLiteral("N/A")
                                       : m_cpuModel->fontMetrics().elidedText(
                                             model, Qt::ElideRight, 121));
    m_cpuModel->setToolTip(model);
    m_cpuTemperature->setText(snapshot.cpuTemperatureCelsius >= 0.0
                                  ? QStringLiteral("%1°C").arg(
                                        snapshot.cpuTemperatureCelsius, 0, 'f', 1)
                                  : QStringLiteral("N/A"));

    const QStorageInfo volume = QStorageInfo::root();
    QString volumeName = snapshot.storageName.trimmed();
    if (volumeName.isEmpty()) volumeName = volume.name().trimmed();
    if (volumeName.isEmpty()) volumeName = volume.displayName().trimmed();
    if (volumeName.isEmpty()) volumeName = QStringLiteral("系统磁盘");
    m_storageName->setText(m_storageName->fontMetrics().elidedText(
        volumeName, Qt::ElideRight, 147));
    m_storageName->setToolTip(volumeName);

    if (snapshot.storageTotal > 0) {
        const quint64 free = snapshot.storageTotal > snapshot.storageUsed
                                 ? snapshot.storageTotal - snapshot.storageUsed : 0;
        const QString capacity = QStringLiteral("可用 %1 / %2")
                                     .arg(readableBytes(free),
                                          readableBytes(snapshot.storageTotal));
        m_storageCapacity->setText(m_storageCapacity->fontMetrics().elidedText(
            capacity, Qt::ElideRight, 172));
        m_storageCapacity->setToolTip(capacity);
    } else {
        m_storageCapacity->setText(QStringLiteral("可用 N/A / N/A"));
        m_storageCapacity->setToolTip(QString());
    }
    setCapacity(m_storageBar, snapshot.storageUsagePercent, snapshot.storageTotal > 0);

    if (snapshot.memoryTotal > 0) {
        const quint64 free = snapshot.memoryTotal > snapshot.memoryUsed
                                 ? snapshot.memoryTotal - snapshot.memoryUsed : 0;
        const QString capacity = QStringLiteral("可用 %1 / %2")
                                     .arg(readableBytes(free),
                                          readableBytes(snapshot.memoryTotal));
        m_memoryCapacity->setText(m_memoryCapacity->fontMetrics().elidedText(
            capacity, Qt::ElideRight, 172));
        m_memoryCapacity->setToolTip(capacity);
    } else {
        m_memoryCapacity->setText(QStringLiteral("可用 N/A / N/A"));
        m_memoryCapacity->setToolTip(QString());
    }
    setCapacity(m_memoryBar, snapshot.memoryUsagePercent, snapshot.memoryTotal > 0);

    m_fanSpeed->setText(snapshot.fanRpm >= 0.0
                            ? QStringLiteral("%1 RPM").arg(qRound(snapshot.fanRpm))
                            : QStringLiteral("N/A"));
    const bool networkAvailable = !snapshot.networkInterface.trimmed().isEmpty()
                                  || !snapshot.localIpv4Address.trimmed().isEmpty();
    m_networkDownload->setText(networkAvailable
                                   ? QStringLiteral("↓ %1").arg(
                                         formatRate(snapshot.networkReceiveBytesPerSecond))
                                   : QStringLiteral("↓ N/A"));
    m_networkUpload->setText(networkAvailable
                                 ? QStringLiteral("↑ %1").arg(
                                       formatRate(snapshot.networkTransmitBytesPerSecond))
                                 : QStringLiteral("↑ N/A"));
    m_networkAddress->setText(snapshot.localIpv4Address.isEmpty()
                                  ? QStringLiteral("IP N/A")
                                  : snapshot.localIpv4Address);
    m_networkAddress->setToolTip(snapshot.networkInterface);
    m_storageRead->setText(snapshot.storageReadBytesPerSecond >= 0.0
                               ? QStringLiteral("R: %1").arg(
                                     formatRate(snapshot.storageReadBytesPerSecond))
                               : QStringLiteral("R: N/A"));
    m_storageWrite->setText(snapshot.storageWriteBytesPerSecond >= 0.0
                                ? QStringLiteral("W: %1").arg(
                                      formatRate(snapshot.storageWriteBytesPerSecond))
                                : QStringLiteral("W: N/A"));
}

void StatusDashboard::showAt(const QPoint &globalAnchor)
{
    QScreen *screen = nullptr;
    for (QScreen *candidate : QGuiApplication::screens()) {
        if (candidate->geometry().contains(globalAnchor)) {
            screen = candidate;
            break;
        }
    }
    if (!screen) screen = QGuiApplication::primaryScreen();
    if (!screen) return;

    const QRect available = screen->availableGeometry();
    const int width = std::max(1, std::min(kPopupWidth, available.width() - 8));
    const int maximumHeight = std::max(1, available.height() - 8);
    const int preferredHeight = m_scrollContent->sizeHint().height();
    const bool needsVerticalScroll = preferredHeight > maximumHeight;
    const int scrollbarExtent = style()->pixelMetric(QStyle::PM_ScrollBarExtent);
    const int scrollbarWidth = needsVerticalScroll ? scrollbarExtent : 0;
    // Keep the cards at a readable width on unusually narrow screens; the
    // scroll area then offers horizontal navigation instead of clipping text.
    m_scrollContent->setFixedWidth(std::max(440, width - scrollbarWidth));
    m_scrollContent->adjustSize();
    const bool needsHorizontalScroll = m_scrollContent->width() > width - scrollbarWidth;
    const int preferredPopupHeight = m_scrollContent->height()
                                     + (needsHorizontalScroll ? scrollbarExtent : 0);
    resize(width, std::min(preferredPopupHeight, maximumHeight));
    const QSize popupSize = size();

    // Most anchors come from the top menu bar, but the same method also works
    // for bottom/right tray areas on Windows and Linux.
    const int gap = 5;
    int x = globalAnchor.x() - width / 2;
    int y = globalAnchor.y() + gap;
    if (y + popupSize.height() > available.bottom() + 1)
        y = globalAnchor.y() - popupSize.height() - gap;
    x = std::clamp(x, available.left(),
                   std::max(available.left(), available.right() - width + 1));
    y = std::clamp(y, available.top(),
                   std::max(available.top(), available.bottom() - popupSize.height() + 1));
    move(x, y);
    show();
    raise();
    activateWindow();
}
