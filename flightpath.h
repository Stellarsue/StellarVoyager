#pragma once

#include <QMatrix4x4>
#include <QVector3D>

namespace FlightPath
{
constexpr double LapSeconds = 48.0;
constexpr float ModelScale = 0.27F;

struct Pose
{
    QVector3D position;
    QVector3D forward;
    QMatrix4x4 model;
};

// The route is fixed in world space. Camera movement never changes the route.
double progress(double seconds);
QVector3D point(double fraction);
Pose sample(double seconds);
}
