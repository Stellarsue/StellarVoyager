#include "spacecraftmesh.h"

#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QVector2D>
#include <QVector3D>
#include <algorithm>
#include <cmath>
#include <limits>

QVector<SpacecraftVertex> loadSpacecraftMesh(const QString &path, QString *error, QStringList *texturePaths)
{
    if (texturePaths) texturePaths->clear();
    auto fail = [error](const QString &message) {
        if (error) *error = message;
        return QVector<SpacecraftVertex>();
    };
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 8 * 1024 * 1024)
        return fail(QStringLiteral("Missing or oversized spacecraft OBJ"));
    QVector<QVector3D> positions, normals;
    QVector<QVector2D> uvs;
    QVector<SpacecraftVertex> vertices;
    struct Material {
        QVector3D color{0.8F, 0.85F, 0.9F};
        float texture = 0;
        float roughness = 0.47F;
        float metallic = 0;
    };
    QHash<QByteArray, Material> materials;
    QStringList textures;
    Material material;
    QVector3D minimum(1e9F, 1e9F, 1e9F), maximum(-1e9F, -1e9F, -1e9F);
    auto indexOf = [](const QByteArray &field, qsizetype size) -> int {
        bool ok = false;
        int index = field.toInt(&ok);
        if (!ok || index == 0) return -1;
        index = index > 0 ? index - 1 : int(size) + index;
        return index >= 0 && index < size ? index : -1;
    };
    for (const auto &rawLine : file.readAll().split('\n')) {
        const auto line = rawLine.simplified();
        if (line.isEmpty() || line.startsWith('#')) continue;
        const auto fields = line.split(' ');
        if (fields[0] == "mtllib" && fields.size() == 2) {
            if (fields[1].contains('/') || fields[1].contains('\\') || fields[1].contains(".."))
                return fail(QStringLiteral("Material path must be a sibling file"));
            QFile materialFile(QFileInfo(path).path() + QLatin1Char('/')
                               + QString::fromUtf8(fields[1]));
            if (!materialFile.open(QIODevice::ReadOnly) || materialFile.size() > 65536)
                return fail(QStringLiteral("Missing or oversized material library"));
            QByteArray materialName;
            for (const auto &materialLine : materialFile.readAll().split('\n')) {
                const auto parts = materialLine.simplified().split(' ');
                if (parts.size() == 2 && parts[0] == "newmtl") {
                    materialName = parts[1];
                    materials.insert(materialName, Material{});
                }
                if (parts.size() == 4 && parts[0] == "Kd" && !materialName.isEmpty()) {
                    QVector3D color;
                    for (int i = 0; i < 3; ++i) {
                        bool ok = false;
                        color[i] = parts[i + 1].toFloat(&ok);
                        if (!ok || !std::isfinite(color[i]) || color[i] < 0 || color[i] > 1)
                            return fail(QStringLiteral("Invalid material color"));
                    }
                    materials[materialName].color = color;
                }
                if (parts.size() == 2 && !materialName.isEmpty()
                    && (parts[0] == "Pr" || parts[0] == "Pm")) {
                    bool ok = false;
                    const float value = parts[1].toFloat(&ok);
                    if (!ok || !std::isfinite(value) || value < 0 || value > 1)
                        return fail(QStringLiteral("Invalid material surface"));
                    if (parts[0] == "Pr") materials[materialName].roughness = qMax(0.10F, value);
                    else materials[materialName].metallic = value;
                }
                if (parts.size() == 2 && parts[0] == "map_Kd" && !materialName.isEmpty()) {
                    if (parts[1].contains('/') || parts[1].contains('\\') || parts[1].contains(".."))
                        return fail(QStringLiteral("Texture path must be a sibling file"));
                    const QString texturePath = QFileInfo(path).path() + QLatin1Char('/')
                                                + QString::fromUtf8(parts[1]);
                    if (!QFile::exists(texturePath)) return fail(QStringLiteral("Missing material texture"));
                    if (!textures.contains(texturePath)) textures.append(texturePath);
                    if (textures.size() > 32) return fail(QStringLiteral("Too many material textures"));
                    materials[materialName].texture = textures.indexOf(texturePath) + 1;
                }
            }
        } else if (fields[0] == "usemtl" && fields.size() == 2) {
            if (!materials.contains(fields[1])) return fail(QStringLiteral("Unknown OBJ material"));
            material = materials.value(fields[1]);
        } else if (fields[0] == "v" || fields[0] == "vn" || fields[0] == "vt") {
            const int count = fields[0] == "vt" ? 2 : 3;
            if (fields.size() < count + 1) return fail(QStringLiteral("Incomplete OBJ vector"));
            float values[3] = {};
            for (int i = 0; i < count; ++i) {
                bool ok = false;
                values[i] = fields[i + 1].toFloat(&ok);
                if (!ok || !std::isfinite(values[i])) return fail(QStringLiteral("Invalid OBJ coordinate"));
            }
            const QVector3D vector(values[0], values[1], values[2]);
            if (fields[0] == "v") {
                positions.append(vector);
                for (int i = 0; i < 3; ++i) {
                    minimum[i] = qMin(minimum[i], vector[i]);
                    maximum[i] = qMax(maximum[i], vector[i]);
                }
            } else if (fields[0] == "vn") normals.append(vector.normalized());
            else uvs.append(QVector2D(values[0], values[1]));
        } else if (fields[0] == "f") {
            if (fields.size() < 4 || fields.size() > 32)
                return fail(QStringLiteral("Invalid OBJ face"));
            QVector<SpacecraftVertex> face;
            for (qsizetype i = 1; i < fields.size(); ++i) {
                const auto indices = fields[i].split('/');
                if (indices.size() != 3) return fail(QStringLiteral("OBJ requires UVs and normals"));
                const int v = indexOf(indices[0], positions.size());
                const int t = indexOf(indices[1], uvs.size());
                const int n = indexOf(indices[2], normals.size());
                if (v < 0 || t < 0 || n < 0) return fail(QStringLiteral("OBJ index out of range"));
                const auto p = positions[v];
                const auto normal = normals[n];
                const auto uv = uvs[t];
                face.append(SpacecraftVertex{{p.x(), p.y(), p.z()}, {normal.x(), normal.y(), normal.z()},
                             {material.color.x(), material.color.y(), material.color.z()},
                             {uv.x(), uv.y()}, {material.texture, material.roughness, material.metallic}});
            }
            for (qsizetype i = 1; i + 1 < face.size(); ++i) {
                vertices.append(face[0]); vertices.append(face[i]); vertices.append(face[i + 1]);
            }
            if (vertices.size() > 300000) return fail(QStringLiteral("OBJ triangle limit exceeded"));
        }
    }
    const QVector3D extent = maximum - minimum;
    const float longest = qMax(extent.x(), qMax(extent.y(), extent.z()));
    if (vertices.isEmpty() || longest < 0.0001F)
        return fail(QStringLiteral("Empty or degenerate OBJ"));
    const QVector3D center = (maximum + minimum) * 0.5F;
    for (auto &vertex : vertices)
        for (int i = 0; i < 3; ++i)
            vertex.position[i] = (vertex.position[i] - center[i]) * 2.0F / longest;
    if (error) error->clear();
    if (texturePaths) *texturePaths = textures;
    return vertices;
}
