#pragma once

#include <QMatrix4x4>
#include <QVector>
#include <QVector3D>
#include <array>

namespace NebulaEnvironment
{
constexpr int CloudCount = 14;
constexpr int PuffsPerCloud = 12;
constexpr float FocalLength = 1.55F;

struct Cloud
{
    QVector3D center;
    QVector3D color;
};

// Fixed world-space clouds on a distant enclosing sphere, not a front-facing layer.
const std::array<Cloud, CloudCount> &clouds();
QMatrix4x4 cameraRotation(qreal yaw, qreal pitch);
QVector<int> visibleClouds(qreal yaw, qreal pitch, qreal zoom, qreal aspect);
}
