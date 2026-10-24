#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

struct SpacecraftVertex
{
    float position[3];
    float normal[3];
    float color[3];
    float uv[2];
    float surface[3]; // texture-array layer, roughness, metallic
};

// Loads the packaged, triangulated OBJ geometry without a runtime asset dependency.
QVector<SpacecraftVertex> loadSpacecraftMesh(const QString &path, QString *error = nullptr,
                                          QStringList *texturePaths = nullptr);
