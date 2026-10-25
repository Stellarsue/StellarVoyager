#include "macmenubarstatus.h"

#include "systeminfo.h"

#include <QBuffer>
#include <QFont>
#include <QFontDatabase>
#include <QImage>
#include <QPainter>
#include <QString>
#include <QTimer>
#include <QtMath>

#import <AppKit/AppKit.h>

#include <utility>

namespace {
constexpr int kReadoutWidth = 88;
constexpr int kReadoutHeight = 22;
constexpr int kNetworkWidth = 36;
constexpr int kMiddleWidth = 28;
constexpr int kLastWidth = kReadoutWidth - kNetworkWidth - kMiddleWidth;
constexpr int kScale = 2;

QString compactRate(double bytesPerSecond)
{
    if (bytesPerSecond >= 1e9)
        return QString::number(bytesPerSecond / 1e9, 'f', 1) + QLatin1Char('G');
    if (bytesPerSecond >= 1e6)
        return QString::number(bytesPerSecond / 1e6, 'f', 1) + QLatin1Char('M');
    if (bytesPerSecond >= 1e3)
        return QString::number(qRound(bytesPerSecond / 1e3)) + QLatin1Char('K');
    return QString::number(qRound(bytesPerSecond)) + QLatin1Char('B');
}

QString compactPercent(double value, bool available, QChar label)
{
    return QString(label) + (available ? QString::number(qRound(value))
                                       : QStringLiteral("—"));
}

void drawCell(QPainter &painter, int x, int y, int width, const QString &value,
              Qt::Alignment horizontalAlignment = Qt::AlignHCenter)
{
    painter.drawText(QRect(x, y, width, kReadoutHeight / 2),
                     horizontalAlignment | Qt::AlignVCenter, value);
}

QImage renderReadout(const SystemSnapshot &snapshot)
{
    QImage image(kReadoutWidth * kScale, kReadoutHeight * kScale,
                 QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(kScale);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setPen(Qt::black); // AppKit recolors this template image for light/dark menu bars.
    QFont font = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
    font.setPixelSize(9);
    painter.setFont(font);

    drawCell(painter, 0, 0, kNetworkWidth,
             QStringLiteral("↑%1").arg(compactRate(snapshot.networkTransmitBytesPerSecond)),
             Qt::AlignLeft);
    drawCell(painter, 0, 11, kNetworkWidth,
             QStringLiteral("↓%1").arg(compactRate(snapshot.networkReceiveBytesPerSecond)),
             Qt::AlignLeft);

    const QString fan = snapshot.fanRpm >= 0.0
                            ? QStringLiteral("F%1").arg(qRound(snapshot.fanRpm))
                            : QStringLiteral("F—");
    drawCell(painter, kNetworkWidth, 0, kMiddleWidth, fan);
    drawCell(painter, kNetworkWidth, 11, kMiddleWidth,
             compactPercent(snapshot.memoryUsagePercent, snapshot.memoryTotal > 0, QLatin1Char('M')));
    drawCell(painter, kNetworkWidth + kMiddleWidth, 0, kLastWidth,
             compactPercent(snapshot.storageUsagePercent, snapshot.storageTotal > 0, QLatin1Char('S')));
    drawCell(painter, kNetworkWidth + kMiddleWidth, 11, kLastWidth,
             compactPercent(snapshot.cpuUsagePercent, true, QLatin1Char('C')));
    return image;
}

NSImage *nativeImage(const QImage &image)
{
    QByteArray png;
    QBuffer buffer(&png);
    if (!buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, "PNG"))
        return nil;
    NSData *data = [NSData dataWithBytes:png.constData() length:png.size()];
    NSImage *result = [[NSImage alloc] initWithData:data];
    [result setSize:NSMakeSize(kReadoutWidth, kReadoutHeight)];
    [result setTemplate:YES];
    return result;
}
}

@interface StellarStatusActions : NSObject {
@public
    std::function<void()> toggleDashboard;
    std::function<void()> showWindow;
    std::function<void()> installFonts;
    std::function<void()> quit;
    NSStatusItem *statusItem;
    NSMenu *contextMenu;
}
- (void)statusItemClicked:(id)sender;
- (void)showWindow:(id)sender;
- (void)installFonts:(id)sender;
- (void)quit:(id)sender;
@end

@implementation StellarStatusActions
- (void)statusItemClicked:(id)sender
{
    (void)sender;
    NSEvent *event = [NSApp currentEvent];
    const bool contextClick = event &&
        ([event type] == NSEventTypeRightMouseDown ||
         [event type] == NSEventTypeRightMouseUp ||
         ([event modifierFlags] & NSEventModifierFlagControl));
    if (contextClick) {
        if (statusItem && contextMenu)
            [statusItem popUpStatusItemMenu:contextMenu];
        return;
    }

    // AppKit is still dispatching the status-button click. Open the Qt popup
    // on the next event turn so the button does not immediately dismiss it.
    const std::function<void()> callback = toggleDashboard;
    QTimer::singleShot(0, [callback] {
        if (callback)
            callback();
    });
}
- (void)showWindow:(id)sender
{
    (void)sender;
    if (showWindow) showWindow();
}
- (void)installFonts:(id)sender
{
    (void)sender;
    if (installFonts) installFonts();
}
- (void)quit:(id)sender
{
    (void)sender;
    if (quit) quit();
}
@end

struct MacMenuBarStatus::Impl
{
    NSStatusItem *item = nil;
    StellarStatusActions *actions = nil;
    NSMenu *menu = nil;
    NSMenuItem *networkItem = nil;
    NSMenuItem *metricsItem = nil;

    Impl(std::function<void()> toggleDashboard, std::function<void()> showWindow,
         std::function<void()> installFonts,
         std::function<void()> quit, bool fontAvailable)
    {
        actions = [[StellarStatusActions alloc] init];
        actions->toggleDashboard = std::move(toggleDashboard);
        actions->showWindow = std::move(showWindow);
        actions->installFonts = std::move(installFonts);
        actions->quit = std::move(quit);

        item = [[[NSStatusBar systemStatusBar]
                    statusItemWithLength:kReadoutWidth + 8] retain];
        item.button.imagePosition = NSImageOnly;
        // A native tooltip can linger above the popup and obscure its cards.
        // The item already displays live values and exposes a context menu.
        item.button.toolTip = nil;
        actions->statusItem = item;
        item.button.target = actions;
        item.button.action = @selector(statusItemClicked:);
        [item.button sendActionOn:(NSEventMaskLeftMouseUp | NSEventMaskRightMouseUp)];

        menu = [[NSMenu alloc] initWithTitle:@"StellarVoyager"];
        menu.autoenablesItems = NO;
        networkItem = [[NSMenuItem alloc] initWithTitle:@"↑ 0 B/s   ↓ 0 B/s"
                                                   action:nullptr keyEquivalent:@""];
        [menu addItem:networkItem];
        [networkItem release];
        metricsItem = [[NSMenuItem alloc] initWithTitle:@"FAN N/A   SSD —   MEM —   CPU —"
                                                   action:nullptr keyEquivalent:@""];
        [menu addItem:metricsItem];
        [metricsItem release];
        [menu addItem:[NSMenuItem separatorItem]];
        NSMenuItem *show = [[NSMenuItem alloc] initWithTitle:@"显示 StellarVoyager"
                                                     action:@selector(showWindow:)
                                              keyEquivalent:@""];
        show.target = actions;
        [menu addItem:show];
        [show release];

        if (fontAvailable) {
            NSMenuItem *fonts = [[NSMenuItem alloc] initWithTitle:@"安装 Times New Roman…"
                                                          action:@selector(installFonts:)
                                                   keyEquivalent:@""];
            fonts.target = actions;
            [menu addItem:fonts];
            [fonts release];
        }
        [menu addItem:[NSMenuItem separatorItem]];
        NSMenuItem *exit = [[NSMenuItem alloc] initWithTitle:@"退出"
                                                     action:@selector(quit:)
                                              keyEquivalent:@""];
        exit.target = actions;
        [menu addItem:exit];
        [exit release];
        // Assigning item.menu would force the menu to open on every left click.
        // Keep the menu for a right/control click instead.
        actions->contextMenu = menu;
    }

    ~Impl()
    {
        item.button.target = nil;
        item.button.action = nullptr;
        actions->statusItem = nil;
        actions->contextMenu = nil;
        [[NSStatusBar systemStatusBar] removeStatusItem:item];
        [item release];
        [menu release];
        [actions release];
    }
};

MacMenuBarStatus::MacMenuBarStatus(std::function<void()> toggleDashboard,
                                   std::function<void()> showWindow,
                                   std::function<void()> installFonts,
                                   std::function<void()> quit,
                                   bool fontAvailable)
    : m_impl(std::make_unique<Impl>(std::move(toggleDashboard),
                                    std::move(showWindow), std::move(installFonts),
                                    std::move(quit), fontAvailable))
{
}

MacMenuBarStatus::~MacMenuBarStatus() = default;

void MacMenuBarStatus::update(const SystemSnapshot &snapshot)
{
    NSImage *image = nativeImage(renderReadout(snapshot));
    if (image) {
        m_impl->item.button.image = image;
        [image release];
    }

    const QString network = QStringLiteral("↑ %1   ↓ %2")
                                .arg(formatRate(snapshot.networkTransmitBytesPerSecond),
                                     formatRate(snapshot.networkReceiveBytesPerSecond));
    const QString fan = snapshot.fanRpm >= 0.0
                            ? QString::number(qRound(snapshot.fanRpm))
                            : QStringLiteral("N/A");
    const QString storage = snapshot.storageTotal > 0
                                ? formatPercent(snapshot.storageUsagePercent) : QStringLiteral("N/A");
    const QString memory = snapshot.memoryTotal > 0
                               ? formatPercent(snapshot.memoryUsagePercent) : QStringLiteral("N/A");
    const QString metrics = QStringLiteral("FAN %1   SSD %2   MEM %3   CPU %4")
                                .arg(fan, storage, memory, formatPercent(snapshot.cpuUsagePercent));
    const QByteArray networkBytes = network.toUtf8();
    const QByteArray metricBytes = metrics.toUtf8();
    m_impl->networkItem.title = [NSString stringWithUTF8String:networkBytes.constData()];
    m_impl->metricsItem.title = [NSString stringWithUTF8String:metricBytes.constData()];
}
