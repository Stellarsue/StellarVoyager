#include "telemetrytooltip.h"
#include "themefonts.h"

#include <QGuiApplication>
#include <QLabel>
#include <QLinearGradient>
#include <QPainter>
#include <QScreen>
#include <QTimer>
#include <QVBoxLayout>

TelemetryToolTip::TelemetryToolTip(QWidget *owner)
    : QWidget(owner, Qt::ToolTip | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint
                     | Qt::WindowDoesNotAcceptFocus | Qt::WindowTransparentForInput)
{
    setObjectName(QStringLiteral("telemetryToolTip"));
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setFocusPolicy(Qt::NoFocus);
    setFont(ThemeFonts::text(12));
    setFixedWidth(278);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(13, 11, 13, 10);
    layout->setSpacing(6);
    auto *title = new QLabel(QStringLiteral("STELLARVOYAGER  /  LIVE TELEMETRY"), this);
    title->setStyleSheet(QStringLiteral("color: #9ce8ed; background: transparent; font-size: 11px; letter-spacing: 0.6px;"));
    layout->addWidget(title);
    m_readings = new QLabel(this);
    m_readings->setObjectName(QStringLiteral("tooltipReadings"));
    m_readings->setTextFormat(Qt::RichText);
    m_readings->setWordWrap(true);
    m_readings->setStyleSheet(QStringLiteral("color: #e7edf4; background: transparent; font-size: 12px;"));
    layout->addWidget(m_readings);
    auto *footer = new QLabel(QStringLiteral("拖动旋转 · 滚轮缩放 · 双击复位\n模型 NASA/JSC · 非 NASA 官方应用"), this);
    footer->setStyleSheet(QStringLiteral("color: #93a8bb; background: transparent; font-size: 10px;"));
    layout->addWidget(footer);
    m_timeout = new QTimer(this);
    m_timeout->setSingleShot(true);
    QObject::connect(m_timeout, &QTimer::timeout, this, &QWidget::hide);
}

void TelemetryToolTip::setSnapshot(const SystemSnapshot &s, const QString &accessibleDetails)
{
    QString rows;
    const auto row = [&rows](const QString &label, const QString &value) {
        rows += QStringLiteral("<tr><td valign='top' style='color:#8bc5d3'>%1&nbsp;&nbsp;</td><td>%2</td></tr>")
                    .arg(label.toHtmlEscaped(), value.toHtmlEscaped());
    };
    row(QStringLiteral("CPU"), QStringLiteral("%1 · %2 核心").arg(formatPercent(s.cpuUsagePercent)).arg(s.cpuCoreCount));
    row(QStringLiteral("RAM"), QStringLiteral("%1 / %2 · %3").arg(formatBytes(s.memoryUsed), formatBytes(s.memoryTotal), formatPercent(s.memoryUsagePercent)));
    row(QStringLiteral("磁盘 /"), QStringLiteral("%1 / %2 · %3").arg(formatBytes(s.storageUsed), formatBytes(s.storageTotal), formatPercent(s.storageUsagePercent)));
    row(QStringLiteral("网络"), s.networkInterface.isEmpty() ? QStringLiteral("未发现活动网卡") : s.networkInterface);
    row(QStringLiteral("↓ / ↑"), QStringLiteral("%1 / %2").arg(formatRate(s.networkReceiveBytesPerSecond), formatRate(s.networkTransmitBytesPerSecond)));
    row(QStringLiteral("GPU"), s.gpu.available ? s.gpu.name : QStringLiteral("未检测到 GPU 数据"));
    if (s.gpu.usagePercent >= 0) row(QStringLiteral("负载"), formatPercent(s.gpu.usagePercent));
    if (s.gpu.memoryTotal > 0)
        row(QStringLiteral("显存"), QStringLiteral("%1 / %2").arg(formatBytes(s.gpu.memoryUsed), formatBytes(s.gpu.memoryTotal)));
    if (s.gpu.temperatureCelsius >= 0)
        row(QStringLiteral("温度"), QStringLiteral("%1 °C").arg(s.gpu.temperatureCelsius, 0, 'f', 0));
    row(QStringLiteral("在线"), formatUptime(s.uptimeSeconds));
    m_readings->setText(QStringLiteral("<table cellspacing='0' cellpadding='1' width='100%'>%1</table>").arg(rows));
    setAccessibleName(QStringLiteral("StellarVoyager 实时遥测详情"));
    setAccessibleDescription(accessibleDetails);
    adjustSize();
    // Keep the fixed width after the content-driven height calculation.
    resize(278, sizeHint().height());
}

void TelemetryToolTip::showAt(const QPoint &globalPosition)
{
    QScreen *screen = QGuiApplication::screenAt(globalPosition);
    if (!screen) screen = QGuiApplication::primaryScreen();
    if (!screen) return;
    const QRect bounds = screen->availableGeometry().adjusted(6, 6, -6, -6);
    QPoint position = globalPosition + QPoint(16, 18);
    if (position.x() + width() > bounds.right()) position.setX(globalPosition.x() - width() - 16);
    if (position.y() + height() > bounds.bottom()) position.setY(globalPosition.y() - height() - 12);
    position.setX(qBound(bounds.left(), position.x(), qMax(bounds.left(), bounds.right() - width() + 1)));
    position.setY(qBound(bounds.top(), position.y(), qMax(bounds.top(), bounds.bottom() - height() + 1)));
    move(position);
    show();
    m_timeout->start(10000);
}

void TelemetryToolTip::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QLinearGradient background(0, 0, width(), height());
    background.setColorAt(0, QColor(13, 29, 45, 235));
    background.setColorAt(1, QColor(12, 19, 34, 228));
    painter.setBrush(background);
    painter.setPen(QPen(QColor(130, 219, 232, 65), 0.8));
    painter.drawRoundedRect(QRectF(rect()).adjusted(0.8, 0.8, -0.8, -0.8), 12, 12);
}
