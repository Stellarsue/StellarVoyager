#include "nebulaenvironment.h"

#include <QtMath>

namespace NebulaEnvironment
{
const std::array<Cloud, CloudCount> &clouds()
{
    static const auto result = [] {
        // Eight azimuth sectors plus three upper and three lower sky fields.
        constexpr float azimuth[] = {-22, 26, 72, 116, 166, 214, 264, 310, 38, 158, 278, 0, 120, 240};
        constexpr float elevation[] = {17, -19, 12, -22, 20, -14, 16, -21, 58, 61, 55, -58, -54, -61};
        const QVector3D palette[] = {{0.78F, 0.21F, 0.72F}, {0.14F, 0.66F, 0.96F},
            {0.22F, 0.73F, 0.68F}, {0.98F, 0.42F, 0.16F},
            {0.43F, 0.24F, 0.95F}, {0.73F, 0.23F, 0.54F},
            {0.95F, 0.64F, 0.21F}, {0.18F, 0.72F, 0.52F}};
        std::array<Cloud, CloudCount> fields;
        for (int i = 0; i < CloudCount; ++i) {
            const float az = qDegreesToRadians(azimuth[i]);
            const float el = qDegreesToRadians(elevation[i]);
            const float radius = 30.0F + (i % 4) * 3.0F;
            fields[i] = {QVector3D(qSin(az) * qCos(el), qSin(el), qCos(az) * qCos(el)) * radius,
                         palette[i % 8]};
        }
        return fields;
    }();
    return result;
}

QMatrix4x4 cameraRotation(qreal yaw, qreal pitch)
{
    QMatrix4x4 view;
    view.rotate(-qRadiansToDegrees(pitch), 1, 0, 0);
    view.rotate(qRadiansToDegrees(yaw), 0, 1, 0);
    return view;
}

QVector<int> visibleClouds(qreal yaw, qreal pitch, qreal zoom, qreal aspect)
{
    QVector<int> result;
    if (zoom <= 0 || aspect <= 0) return result;
    const auto rotation = cameraRotation(yaw, pitch);
    // Conservative bounds include every puff and its view-facing quad. Culling
    // the center alone would make a cloud pop out while its edge is still visible.
    constexpr float extent = 7.5F;
    const float vertical = FocalLength * float(zoom);
    const float horizontal = vertical / float(aspect);
    for (int i = 0; i < CloudCount; ++i) {
        const QVector3D p = rotation.mapVector(clouds()[i].center);
        if (p.z() + extent <= 1.0F) continue;
        if (qAbs(p.x()) * horizontal > p.z() + extent * qSqrt(1 + horizontal * horizontal)) continue;
        if (qAbs(p.y()) * vertical > p.z() + extent * qSqrt(1 + vertical * vertical)) continue;
        result.append(i);
    }
    return result;
}
}
