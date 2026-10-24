#include "blackholewidget.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QRadialGradient>
#include <QtMath>

namespace
{
constexpr qreal kPi = 3.14159265358979323846;
constexpr qreal kSupersampling = 2.0;
constexpr qreal kBaseInclination = 0.247;
constexpr qreal kPitchResponse = 1.0;
constexpr qreal kYawResponse = 1.0;
constexpr qreal kDiskRoll = -0.14;

struct ProjectedDiskPoint
{
    QPointF screen;
    qreal depth;
};

ProjectedDiskPoint projectDisk(qreal angle, qreal radius, qreal yaw, qreal pitch)
{
    // Keep this camera transform in step with the OpenGL accretion particles.
    const qreal x = qCos(angle) * radius;
    const qreal z = qSin(angle) * radius;
    const qreal cameraYaw = yaw * kYawResponse;
    const qreal turnedX = qCos(cameraYaw) * x + qSin(cameraYaw) * z;
    const qreal turnedZ = -qSin(cameraYaw) * x + qCos(cameraYaw) * z;
    const qreal inclination = kBaseInclination + pitch * kPitchResponse;
    const qreal raisedY = qSin(inclination) * turnedZ;
    const qreal depth = qCos(inclination) * turnedZ;
    const qreal screenX = qCos(kDiskRoll) * turnedX + qSin(kDiskRoll) * raisedY;
    const qreal screenY = qSin(kDiskRoll) * turnedX - qCos(kDiskRoll) * raisedY;
    return {QPointF(screenX, screenY), depth};
}

QPixmap makeLayer(const QSize &logicalSize)
{
    QPixmap layer(logicalSize * kSupersampling);
    layer.setDevicePixelRatio(kSupersampling);
    layer.fill(Qt::transparent);
    return layer;
}

QColor mixColors(const QColor &first, const QColor &second, qreal amount)
{
    amount = qBound(0.0, amount, 1.0);
    return QColor::fromRgbF(first.redF() + (second.redF() - first.redF()) * amount,
                            first.greenF() + (second.greenF() - first.greenF()) * amount,
                            first.blueF() + (second.blueF() - first.blueF()) * amount,
                            first.alphaF() + (second.alphaF() - first.alphaF()) * amount);
}

qreal lineOfSightVelocity(qreal angle, qreal yaw, qreal pitch)
{
    return qCos(angle - yaw * kYawResponse)
           * qCos(kBaseInclination + pitch * kPitchResponse);
}

QColor diskColor(qreal radialPosition, qreal velocity, qreal opacity)
{
    // A thin disk gets hotter toward the ISCO. The left side is the approaching
    // side in the default view; turning the camera moves the beamed side.
    const QColor inner(QStringLiteral("#FFFDEB"));
    const QColor middle(QStringLiteral("#FFB368"));
    const QColor outer(QStringLiteral("#A43D23"));
    QColor thermal = radialPosition < 0.48
                         ? mixColors(inner, middle, radialPosition / 0.48)
                         : mixColors(middle, outer, (radialPosition - 0.48) / 0.52);

    const qreal approaching = qBound(0.0, (1.0 - velocity) * 0.5, 1.0);
    thermal = mixColors(thermal, QColor(QStringLiteral("#FFFAE6")), approaching * 0.18);
    const qreal beam = 0.58 + approaching * 0.42;
    thermal.setAlphaF(qBound(0.0, opacity * beam, 1.0));
    return thermal;
}

void drawDiskHalf(QPainter &painter,
                  bool nearSide,
                  qreal innerRadius,
                  qreal outerRadius,
                  qreal yaw,
                  qreal pitch)
{
    constexpr int rings = 52;
    constexpr int segments = 128;
    const qreal opacity = nearSide ? 1.0 : 0.78;
    const qreal firstAngle = yaw * kYawResponse + (nearSide ? kPi : 0.0);
    const qreal radialStep = (outerRadius - innerRadius) / (rings - 1);
    painter.setBrush(Qt::NoBrush);

    for (int ring = rings - 1; ring >= 0; --ring) {
        const qreal radialPosition = ring / qreal(rings - 1);
        const qreal radius = innerRadius + (outerRadius - innerRadius) * radialPosition;
        // Continuous paths avoid segment caps forming a tessellated wire grid.
        QPainterPath strand;
        for (int segment = 0; segment <= segments; ++segment) {
            const qreal angle = firstAngle + kPi * segment / segments;
            const QPointF point = projectDisk(angle, radius, yaw, pitch).screen;
            if (segment == 0) strand.moveTo(point); else strand.lineTo(point);
        }
        const qreal filament = 0.88 + 0.12 * qSin(ring * 2.31);
        const qreal edgeFade = qBound(0.0, (1.0 - radialPosition) * 5.0, 1.0);
        const qreal intensity = opacity * filament * edgeFade;
        const qreal velocity = qCos(kBaseInclination + pitch * kPitchResponse);
        QLinearGradient emission(-radius, 0, radius, 0);
        emission.setColorAt(0, diskColor(radialPosition, -velocity, intensity));
        emission.setColorAt(0.5, diskColor(radialPosition, 0, intensity));
        emission.setColorAt(1, diskColor(radialPosition, velocity, intensity));
        painter.setPen(QPen(QBrush(emission), radialStep * 1.25,
                            Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPath(strand);
    }
}

void drawLensedBackside(QPainter &painter, qreal shadowRadius,
                        qreal yaw, qreal pitch)
{
    // A cinematic strong-lensing approximation, not a relativistic ray tracer.
    // Smoothly exchange the upper/lower images when crossing the disk plane.
    const qreal inclination = qSin(kBaseInclination + pitch * kPitchResponse);
    const qreal upperWeight = 0.5 + 0.5 * qBound(-1.0, inclination / 0.12, 1.0);
    painter.save();
    painter.rotate(qRadiansToDegrees(kDiskRoll));
    painter.setBrush(Qt::NoBrush);
    for (int side : {-1, 1}) {
        const qreal weight = side < 0 ? upperWeight : 1.0 - upperWeight;
        const qreal brightness = 0.16 + 0.84 * weight;
        for (int band = 11; band >= 0; --band) {
            const qreal t = band / 11.0;
            const qreal r = shadowRadius * (1.075 + t * 0.30);
            const qreal wing = shadowRadius * (2.38 + t * 0.18);
            QPainterPath arc;
            arc.moveTo(-wing, side * r * 0.07);
            arc.cubicTo(-r * 1.30, side * r * 0.08,
                        -r * 1.05, side * r * 0.14,
                        -r * 0.94, side * r * 0.40);
            arc.cubicTo(-r * 0.76, side * r * 0.88,
                        -r * 0.43, side * r, 0, side * r);
            arc.cubicTo(r * 0.43, side * r, r * 0.76, side * r * 0.88,
                        r * 0.94, side * r * 0.40);
            arc.cubicTo(r * 1.05, side * r * 0.14,
                        r * 1.30, side * r * 0.08, wing, side * r * 0.07);

            if (band % 3 == 0) {
                painter.setPen(QPen(QColor(255, 95, 32, qRound(22 * brightness)),
                                    shadowRadius * 0.34, Qt::SolidLine, Qt::RoundCap));
                painter.drawPath(arc);
            }
            QLinearGradient emission(-wing, 0, wing, 0);
            const qreal filament = band % 3 == 1 ? 0.48 : 1.0;
            emission.setColorAt(0.0, QColor(223, 76, 28, 0));
            emission.setColorAt(0.23, diskColor(t * 0.72, -0.9,
                                               brightness * filament));
            emission.setColorAt(0.58, diskColor(t * 0.72, -0.2,
                                               brightness * filament));
            emission.setColorAt(0.80, diskColor(t * 0.72, 0.8,
                                               brightness * filament));
            emission.setColorAt(1.0, QColor(223, 76, 28, 0));
            painter.setPen(QPen(QBrush(emission), 0.65 + (1.0 - t) * 0.55,
                                Qt::SolidLine, Qt::RoundCap));
            painter.drawPath(arc);
        }
    }
    Q_UNUSED(yaw);
    painter.restore();
}

void drawPhotonRing(QPainter &painter, qreal radius, qreal yaw)
{
    constexpr int segments = 96;
    for (int segment = 0; segment < segments; ++segment) {
        const qreal a0 = 2.0 * kPi * segment / segments;
        const qreal a1 = 2.0 * kPi * (segment + 1) / segments;
        const QPointF from(qCos(a0) * radius, qSin(a0) * radius);
        const QPointF to(qCos(a1) * radius, qSin(a1) * radius);
        const qreal approaching = qBound(0.0,
                                        (1.0 - qCos((a0 + a1) * 0.5
                                                     - yaw * kYawResponse)) * 0.5,
                                        1.0);
        QColor color = mixColors(QColor(QStringLiteral("#FF8A57")),
                                 QColor(QStringLiteral("#FFFBE3")), approaching);
        color.setAlphaF(0.28 + approaching * 0.34);
        painter.setPen(QPen(color, 0.42 + approaching * 0.25,
                            Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(from, to);
    }
}
}

BlackHoleWidget::BlackHoleWidget(QWidget *parent, int diameter)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setFixedSize(diameter, diameter);
}

void BlackHoleWidget::advanceAnimation(qreal degrees)
{
    m_phase += degrees;
    if (m_phase >= 360.0)
        m_phase -= 360.0;
    update();
}

QSize BlackHoleWidget::sizeHint() const
{
    return size();
}

void BlackHoleWidget::setActivity(double percent)
{
    const qreal activity = qBound(0.0, percent / 100.0, 1.0);
    if (qAbs(m_activity - activity) < 0.001)
        return;
    m_activity = activity;
    update();
}

void BlackHoleWidget::setCameraOrientation(qreal yaw, qreal pitch)
{
    if (qAbs(m_cameraYaw - yaw) < 0.001 && qAbs(m_cameraPitch - pitch) < 0.001)
        return;
    m_cameraYaw = yaw;
    m_cameraPitch = pitch;
    m_layersDirty = true;
    update();
}

void BlackHoleWidget::rebuildLayers()
{
    const QSize layerSize = size();
    if (layerSize.isEmpty())
        return;

    m_cachedSize = layerSize;
    m_layersDirty = false;
    const qreal radius = qMin(width(), height()) * 0.5;
    const qreal diskOuterRadius = radius * 0.84;
    const qreal diskInnerRadius = radius * 0.38;
    const qreal shadowRadius = radius * 0.34;

    m_backgroundLayer = makeLayer(layerSize);
    {
        QPainter painter(&m_backgroundLayer);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.translate(width() / 2.0, height() / 2.0);

        QRadialGradient glow(0, 0, radius);
        glow.setColorAt(0.0, QColor(255, 233, 182, 18));
        glow.setColorAt(0.34, QColor(255, 147, 59, 48));
        glow.setColorAt(0.68, QColor(188, 57, 28, 26));
        glow.setColorAt(1.0, QColor(76, 24, 52, 0));
        painter.setPen(Qt::NoPen);
        painter.setBrush(glow);
        painter.drawEllipse(QRectF(-radius, -radius, radius * 2.0, radius * 2.0));

        const QPointF stars[] = {{-25.0, -18.0}, {23.0, -21.0}, {-27.0, 13.0},
                                 {26.0, 17.0}, {-17.0, 25.0}, {18.0, 27.0}};
        for (int index = 0; index < 6; ++index) {
            const qreal starSize = index % 3 == 0 ? 0.72 : 0.48;
            painter.setBrush(QColor(236, 228, 238, 75 + (index % 2) * 38));
            painter.drawEllipse(stars[index], starSize, starSize);
        }
    }

    m_farDiskLayer = makeLayer(layerSize);
    {
        QPainter painter(&m_farDiskLayer);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.translate(width() / 2.0, height() / 2.0);
        drawDiskHalf(painter, false, diskInnerRadius, diskOuterRadius,
                     m_cameraYaw, m_cameraPitch);
        drawLensedBackside(painter, shadowRadius, m_cameraYaw, m_cameraPitch);
    }

    m_nearDiskLayer = makeLayer(layerSize);
    {
        QPainter painter(&m_nearDiskLayer);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.translate(width() / 2.0, height() / 2.0);
        drawDiskHalf(painter, true, diskInnerRadius, diskOuterRadius,
                     m_cameraYaw, m_cameraPitch);
    }

    m_coreLayer = makeLayer(layerSize);
    {
        QPainter painter(&m_coreLayer);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.translate(width() / 2.0, height() / 2.0);

        QRadialGradient eventHorizon(0, 0, shadowRadius * 1.035);
        eventHorizon.setColorAt(0.0, QColor(0, 0, 0, 255));
        eventHorizon.setColorAt(0.94, QColor(0, 0, 0, 255));
        eventHorizon.setColorAt(0.985, QColor(2, 1, 3, 252));
        eventHorizon.setColorAt(1.0, QColor(0, 0, 0, 0));
        painter.setBrush(eventHorizon);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(QPointF(0, 0), shadowRadius * 1.035, shadowRadius * 1.035);
        drawPhotonRing(painter, shadowRadius * 1.035, m_cameraYaw);
    }
}

void BlackHoleWidget::drawOrbitingMatter(QPainter &painter, bool nearSide) const
{
    const qreal radius = qMin(width(), height()) * 0.5;
    const qreal outerRadius = radius * 0.84;
    const qreal innerRadius = radius * 0.40;

    painter.save();
    painter.translate(width() / 2.0, height() / 2.0);
    for (int index = 0; index < 22; ++index) {
        const qreal fraction = qreal((index * 37) % 101) / 100.0;
        const qreal orbit = innerRadius + (outerRadius - innerRadius) * (0.08 + fraction * 0.90);
        const qreal keplerSpeed = qPow(outerRadius / orbit, 1.5);
        const qreal angle = qDegreesToRadians(m_phase * keplerSpeed + index * 137.508);
        const ProjectedDiskPoint projected = projectDisk(angle, orbit,
                                                          m_cameraYaw, m_cameraPitch);
        const bool particleNear = projected.depth < 0.0;
        if (particleNear != nearSide)
            continue;

        const QPointF position = projected.screen;
        QPointF tangent = projectDisk(angle + 0.015, orbit,
                                       m_cameraYaw, m_cameraPitch).screen - position;
        const qreal tangentLength = qSqrt(tangent.x() * tangent.x() + tangent.y() * tangent.y());
        if (tangentLength > 0.0)
            tangent /= tangentLength;

        const qreal radialPosition = (orbit - innerRadius) / (outerRadius - innerRadius);
        QColor color = diskColor(radialPosition,
                                 lineOfSightVelocity(angle, m_cameraYaw,
                                                     m_cameraPitch),
                                 nearSide ? 0.86 : 0.50);
        const qreal streakLength = 1.1 + (1.0 - radialPosition) * 1.9;
        painter.setPen(QPen(color, 0.55 + (index % 3) * 0.22,
                            Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(position - tangent * streakLength * 0.5,
                         position + tangent * streakLength * 0.5);
    }
    painter.restore();
}

void BlackHoleWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    paintLayers(painter);
}

QImage BlackHoleWidget::renderFrame()
{
    QImage frame(size() * kSupersampling, QImage::Format_RGBA8888);
    frame.setDevicePixelRatio(kSupersampling);
    frame.fill(Qt::transparent);
    QPainter painter(&frame);
    paintLayers(painter);
    return frame;
}

void BlackHoleWidget::paintLayers(QPainter &painter)
{
    painter.setRenderHint(QPainter::Antialiasing);
    if (m_cachedSize != size() || m_layersDirty)
        rebuildLayers();
    if (m_backgroundLayer.isNull())
        return;

    painter.save();
    painter.setOpacity(0.72 + m_activity * 0.28);
    painter.drawPixmap(0, 0, m_backgroundLayer);
    painter.restore();
    painter.save();
    painter.setOpacity(0.90 + m_activity * 0.10);
    painter.drawPixmap(0, 0, m_farDiskLayer);
    drawOrbitingMatter(painter, false);
    painter.restore();
    painter.drawPixmap(0, 0, m_coreLayer);
    painter.save();
    painter.setOpacity(0.90 + m_activity * 0.10);
    painter.drawPixmap(0, 0, m_nearDiskLayer);
    drawOrbitingMatter(painter, true);
    painter.restore();
}
