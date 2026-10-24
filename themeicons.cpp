#include "themeicons.h"

#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>

namespace
{
QPixmap drawGlyph(ThemeIcon glyph, int size, const QColor &color)
{
    QPixmap result(size, size);
    result.fill(Qt::transparent);

    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.scale(size / 24.0, size / 24.0);

    QPen pen(color, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    switch (glyph) {
    case ThemeIcon::App: {
        QRadialGradient space(9.0, 7.5, 18.0);
        space.setColorAt(0.0, QColor(QStringLiteral("#172844")));
        space.setColorAt(0.48, QColor(QStringLiteral("#080E1E")));
        space.setColorAt(1.0, QColor(QStringLiteral("#02040A")));
        painter.setPen(QPen(QColor(150, 234, 245, 34), 0.65));
        painter.setBrush(space);
        painter.drawRoundedRect(QRectF(1.2, 1.2, 21.6, 21.6), 5.7, 5.7);

        QRadialGradient glow(12, 12, 9.8);
        glow.setColorAt(0.0, QColor(255, 236, 188, 22));
        glow.setColorAt(0.42, QColor(242, 139, 81, 34));
        glow.setColorAt(0.72, QColor(133, 91, 231, 22));
        glow.setColorAt(1.0, QColor(53, 202, 224, 0));
        painter.setPen(Qt::NoPen);
        painter.setBrush(glow);
        painter.drawEllipse(QPointF(12, 12), 9.8, 8.4);

        painter.save();
        painter.translate(12, 12);
        painter.rotate(-11);
        painter.setBrush(Qt::NoBrush);
        QLinearGradient farRing(-8.2, 2.2, 8.2, -2.2);
        farRing.setColorAt(0.0, QColor(QStringLiteral("#45D9E8")));
        farRing.setColorAt(0.42, QColor(QStringLiteral("#8E82FF")));
        farRing.setColorAt(0.72, QColor(QStringLiteral("#F38E70")));
        farRing.setColorAt(1.0, QColor(QStringLiteral("#FFD77F")));
        painter.setPen(QPen(QBrush(farRing), 1.0,
                            Qt::SolidLine, Qt::RoundCap));
        painter.drawEllipse(QRectF(-8.4, -2.75, 16.8, 5.5));
        painter.setPen(QPen(QColor(178, 163, 255, 110), 0.55,
                            Qt::SolidLine, Qt::RoundCap));
        painter.drawEllipse(QRectF(-7.1, -1.9, 14.2, 3.8));
        painter.restore();

        QRadialGradient horizon(10.8, 10.6, 5.0);
        horizon.setColorAt(0.0, QColor(QStringLiteral("#000000")));
        horizon.setColorAt(0.72, QColor(QStringLiteral("#000000")));
        horizon.setColorAt(0.86, QColor(QStringLiteral("#060A12")));
        horizon.setColorAt(1.0, QColor(QStringLiteral("#FFE8B1")));
        painter.setPen(Qt::NoPen);
        painter.setBrush(horizon);
        painter.drawEllipse(QPointF(12, 12), 4.15, 4.15);
        painter.setBrush(QColor(QStringLiteral("#000000")));
        painter.drawEllipse(QPointF(12, 12), 3.05, 3.05);

        painter.save();
        painter.translate(12, 12);
        painter.rotate(-11);
        QPainterPath foreground;
        foreground.moveTo(-7.3, 0.65);
        foreground.cubicTo(-3.4, 3.0, 3.7, 3.1, 7.6, 0.65);
        QLinearGradient hotRing(-7.3, 0.0, 7.6, 0.0);
        hotRing.setColorAt(0.0, QColor(QStringLiteral("#E57A5B")));
        hotRing.setColorAt(0.43, QColor(QStringLiteral("#FFF3CB")));
        hotRing.setColorAt(1.0, QColor(QStringLiteral("#F6A357")));
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QBrush(hotRing), 1.35, Qt::SolidLine, Qt::RoundCap));
        painter.drawPath(foreground);
        painter.restore();

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(QStringLiteral("#E7FBFF")));
        painter.drawEllipse(QPointF(5.7, 6.2), 0.38, 0.38);
        painter.setBrush(QColor(QStringLiteral("#FFF1BF")));
        painter.drawEllipse(QPointF(18.0, 9.0), 0.52, 0.52);
        break;
    }
    case ThemeIcon::Pin: {
        QPainterPath head;
        head.moveTo(8.4, 4.0);
        head.lineTo(15.6, 4.0);
        head.lineTo(14.1, 9.2);
        head.lineTo(17.1, 12.1);
        head.lineTo(17.1, 13.4);
        head.lineTo(6.9, 13.4);
        head.lineTo(6.9, 12.1);
        head.lineTo(9.9, 9.2);
        head.closeSubpath();
        painter.drawPath(head);
        painter.drawLine(QPointF(12, 13.7), QPointF(12, 20.5));
        painter.drawLine(QPointF(12, 20.5), QPointF(10.7, 18.8));
        break;
    }
    case ThemeIcon::Refresh: {
        QPainterPath arc;
        arc.arcMoveTo(QRectF(4.0, 4.0, 16.0, 16.0), 38);
        arc.arcTo(QRectF(4.0, 4.0, 16.0, 16.0), 38, 274);
        painter.drawPath(arc);
        painter.drawPolyline(QPolygonF({QPointF(18.9, 4.2), QPointF(19.2, 9.0), QPointF(14.6, 7.8)}));
        break;
    }
    case ThemeIcon::Close:
        painter.drawLine(QPointF(6.2, 6.2), QPointF(17.8, 17.8));
        painter.drawLine(QPointF(17.8, 6.2), QPointF(6.2, 17.8));
        break;
    case ThemeIcon::Cpu: {
        painter.drawRoundedRect(QRectF(6.0, 6.0, 12.0, 12.0), 2.2, 2.2);
        painter.drawRoundedRect(QRectF(9.0, 9.0, 6.0, 6.0), 1.0, 1.0);
        for (int offset : {8, 12, 16}) {
            painter.drawLine(QPointF(offset, 3.5), QPointF(offset, 6.0));
            painter.drawLine(QPointF(offset, 18.0), QPointF(offset, 20.5));
            painter.drawLine(QPointF(3.5, offset), QPointF(6.0, offset));
            painter.drawLine(QPointF(18.0, offset), QPointF(20.5, offset));
        }
        break;
    }
    case ThemeIcon::Memory:
        painter.drawRoundedRect(QRectF(3.5, 6.0, 17.0, 11.5), 2.0, 2.0);
        painter.drawRoundedRect(QRectF(6.3, 8.8, 3.0, 5.0), 0.8, 0.8);
        painter.drawRoundedRect(QRectF(10.5, 8.8, 3.0, 5.0), 0.8, 0.8);
        painter.drawRoundedRect(QRectF(14.7, 8.8, 3.0, 5.0), 0.8, 0.8);
        for (int x : {6, 9, 12, 15, 18})
            painter.drawLine(QPointF(x, 17.5), QPointF(x, 20.0));
        break;
    case ThemeIcon::Storage:
        painter.drawEllipse(QRectF(4.0, 4.2, 16.0, 5.3));
        painter.drawLine(QPointF(4.0, 6.9), QPointF(4.0, 17.1));
        painter.drawLine(QPointF(20.0, 6.9), QPointF(20.0, 17.1));
        painter.drawArc(QRectF(4.0, 14.4, 16.0, 5.3), 180 * 16, 180 * 16);
        painter.drawArc(QRectF(4.0, 9.1, 16.0, 5.3), 180 * 16, 180 * 16);
        break;
    case ThemeIcon::Network:
        painter.drawLine(QPointF(8.0, 19.5), QPointF(8.0, 5.0));
        painter.drawPolyline(QPolygonF({QPointF(4.3, 8.7), QPointF(8.0, 5.0), QPointF(11.7, 8.7)}));
        painter.drawLine(QPointF(16.0, 4.5), QPointF(16.0, 19.0));
        painter.drawPolyline(QPolygonF({QPointF(12.3, 15.3), QPointF(16.0, 19.0), QPointF(19.7, 15.3)}));
        break;
    case ThemeIcon::Gpu:
        painter.drawRoundedRect(QRectF(3.0, 5.5, 18.0, 13.0), 2.2, 2.2);
        painter.drawEllipse(QPointF(10.2, 12.0), 4.0, 4.0);
        painter.drawEllipse(QPointF(10.2, 12.0), 1.1, 1.1);
        painter.drawLine(QPointF(16.1, 9.2), QPointF(18.2, 9.2));
        painter.drawLine(QPointF(16.1, 12.0), QPointF(18.2, 12.0));
        painter.drawLine(QPointF(16.1, 14.8), QPointF(18.2, 14.8));
        painter.drawLine(QPointF(6.5, 18.5), QPointF(6.5, 21.0));
        painter.drawLine(QPointF(10.0, 18.5), QPointF(10.0, 21.0));
        painter.drawLine(QPointF(13.5, 18.5), QPointF(13.5, 21.0));
        break;
    case ThemeIcon::Overview:
        painter.drawRoundedRect(QRectF(4.0, 4.0, 6.2, 6.2), 1.3, 1.3);
        painter.drawRoundedRect(QRectF(13.8, 4.0, 6.2, 6.2), 1.3, 1.3);
        painter.drawRoundedRect(QRectF(4.0, 13.8, 6.2, 6.2), 1.3, 1.3);
        painter.drawRoundedRect(QRectF(13.8, 13.8, 6.2, 6.2), 1.3, 1.3);
        break;
    case ThemeIcon::Show: {
        QPainterPath eye;
        eye.moveTo(3.0, 12.0);
        eye.cubicTo(7.0, 6.3, 17.0, 6.3, 21.0, 12.0);
        eye.cubicTo(17.0, 17.7, 7.0, 17.7, 3.0, 12.0);
        painter.drawPath(eye);
        painter.drawEllipse(QPointF(12.0, 12.0), 2.8, 2.8);
        break;
    }
    case ThemeIcon::Power:
        painter.drawArc(QRectF(4.0, 4.0, 16.0, 16.0), 55 * 16, 250 * 16);
        painter.drawLine(QPointF(12.0, 2.8), QPointF(12.0, 11.5));
        break;
    }

    return result;
}
}

QIcon ThemeIcons::icon(ThemeIcon glyph, const QColor &color)
{
    QIcon result;
    for (int size : {16, 20, 24, 32, 48, 64})
        result.addPixmap(drawGlyph(glyph, size, color));
    return result;
}

QIcon ThemeIcons::toggleIcon(ThemeIcon glyph, const QColor &offColor, const QColor &onColor)
{
    QIcon result;
    for (int size : {16, 20, 24, 32}) {
        result.addPixmap(drawGlyph(glyph, size, offColor), QIcon::Normal, QIcon::Off);
        result.addPixmap(drawGlyph(glyph, size, onColor), QIcon::Normal, QIcon::On);
        result.addPixmap(drawGlyph(glyph, size, QColor(QStringLiteral("#FFFFFF"))), QIcon::Active, QIcon::Off);
        result.addPixmap(drawGlyph(glyph, size, onColor), QIcon::Active, QIcon::On);
    }
    return result;
}

QIcon ThemeIcons::applicationIcon()
{
    return icon(ThemeIcon::App);
}

QPixmap ThemeIcons::pixmap(ThemeIcon glyph, const QSize &size, const QColor &color)
{
    const int extent = qMax(size.width(), size.height());
    return drawGlyph(glyph, extent, color).scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}
