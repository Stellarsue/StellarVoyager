#pragma once

#include <QColor>
#include <QIcon>
#include <QSize>

enum class ThemeIcon
{
    App,
    Pin,
    Refresh,
    Close,
    Cpu,
    Memory,
    Storage,
    Network,
    Gpu,
    Overview,
    Settings,
    Show,
    Power
};

namespace ThemeIcons
{
QIcon icon(ThemeIcon glyph, const QColor &color = QColor(QStringLiteral("#F8E9ED")));
QIcon toggleIcon(ThemeIcon glyph, const QColor &offColor, const QColor &onColor);
QIcon applicationIcon();
QPixmap pixmap(ThemeIcon glyph, const QSize &size, const QColor &color);
}
