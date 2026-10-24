#include "flightpath.h"

#include <QVector4D>
#include <QtMath>
#include <cmath>

namespace FlightPath
{
namespace
{
constexpr double kTau = 6.28318530717958647692;
constexpr double kStart = 3.95;

QMatrix4x4 routeOrientation()
{
    QMatrix4x4 orientation;
    orientation.rotate(-7.0F, 0, 0, 1);
    orientation.rotate(-39.0F, 1, 0, 0);
    return orientation;
}
}

double progress(double seconds)
{
    if (!std::isfinite(seconds)) return 0.0;
    const double cycles = seconds / LapSeconds;
    return cycles - std::floor(cycles);
}

QVector3D point(double fraction)
{
    const double angle = kStart + kTau * fraction;
    return routeOrientation().mapVector(QVector3D(float(1.30 * qCos(angle)), 0,
                                                  float(1.13 * qSin(angle))))
           + QVector3D(0, -0.10F, 0);
}

Pose sample(double seconds)
{
    const double fraction = progress(seconds);
    const double angle = kStart + kTau * fraction;
    const QMatrix4x4 plane = routeOrientation();
    Pose pose;
    pose.position = point(fraction);
    pose.forward = plane.mapVector(QVector3D(float(-1.30 * qSin(angle)), 0,
                                             float(1.13 * qCos(angle)))).normalized();
    const QVector3D normal = plane.mapVector(QVector3D(0, 1, 0));
    const QVector3D right = QVector3D::crossProduct(normal, pose.forward).normalized();
    const QVector3D up = QVector3D::crossProduct(pose.forward, right).normalized();
    // The normalized model nose is +Z. Use the tangent frame for continuous heading
    // through the loop, with a small smooth bank into the turn.
    pose.model.setColumn(0, QVector4D(right, 0));
    pose.model.setColumn(1, QVector4D(up, 0));
    pose.model.setColumn(2, QVector4D(pose.forward, 0));
    pose.model.setColumn(3, QVector4D(pose.position, 1));
    pose.model.rotate(float(-9.0 + 3.0 * qCos(angle)), 0, 0, 1);
    pose.model.scale(ModelScale);
    return pose;
}
}
