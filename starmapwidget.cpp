#include "starmapwidget.h"

#include "blackholewidget.h"
#include "spacecraftmesh.h"
#include "flightpath.h"
#include "nebulaenvironment.h"
#include "themefonts.h"
#include "telemetrytooltip.h"

#include <QDateTime>
#include <QFontMetrics>
#include <QHideEvent>
#include <QHelpEvent>
#include <QImage>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QOpenGLShaderProgram>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QRadialGradient>
#include <QResizeEvent>
#include <QShowEvent>
#include <QSurfaceFormat>
#include <QTimer>
#include <QVector2D>
#include <QVector3D>
#include <array>
#include <cmath>
#include <cstddef>
#include <QWheelEvent>
#include <QtMath>

namespace
{
constexpr qreal kWidth = 364.0;
constexpr qreal kHeight = 322.0;
constexpr qreal kCenterX = 182.0;
constexpr qreal kCenterY = 113.0;
constexpr int kRouteSamples = 257;

const QColor kWhite(QStringLiteral("#FFF7F3"));
const QColor kMuted(QStringLiteral("#C6AAB3"));
const QColor kCpu(QStringLiteral("#FFE0A0"));
const QColor kMemory(QStringLiteral("#C6AFFC"));
const QColor kStorage(QStringLiteral("#FFC56E"));
const QColor kNetwork(QStringLiteral("#75E4BC"));
const QColor kGpu(QStringLiteral("#F39AAE"));

const char *kFullscreenVertexShader = R"GLSL(
#version 330 core
out vec2 vUv;
void main()
{
    vec2 positions[3] = vec2[3](vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));
    vec2 position = positions[gl_VertexID];
    vUv = position * 0.5 + 0.5;
    gl_Position = vec4(position, 0.0, 1.0);
}
)GLSL";

const char *kSpaceFragmentShader = R"GLSL(
#version 330 core
in vec2 vUv;
out vec4 fragColor;
uniform float uTime;
uniform float uAspect;
uniform sampler2D uSpaceTexture;
uniform vec2 uCamera;
uniform vec2 uViewport;
uniform float uDpr;

float hash21(vec2 p)
{
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

float noise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash21(i), hash21(i + vec2(1.0, 0.0)), f.x),
               mix(hash21(i + vec2(0.0, 1.0)), hash21(i + vec2(1.0)), f.x), f.y);
}

float fbm(vec2 p)
{
    float value = 0.0;
    float amplitude = 0.54;
    mat2 rotation = mat2(1.72, -0.46, 0.46, 1.72);
    for (int i = 0; i < 5; ++i) {
        value += noise(p) * amplitude;
        p = rotation * p + vec2(3.7, -2.8);
        amplitude *= 0.51;
    }
    return value;
}

void main()
{
    vec2 uv = clamp(vUv, 0.0, 1.0);
    vec2 drift = vec2(sin(uTime * 0.035), cos(uTime * 0.027)) * 0.0014
                 + vec2(sin(uCamera.x), -sin(uCamera.y)) * 0.0075;
    vec2 sampleUv = clamp(uv + drift, 0.002, 0.998);
    vec4 base = texture(uSpaceTexture, sampleUv);
    // Keep the transparency field still while the colored nebula drifts.
    vec2 edge = min(gl_FragCoord.xy, uViewport - gl_FragCoord.xy);
    vec2 t = clamp(edge / (10.0 * uDpr), 0.0, 1.0);
    vec2 feather = t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
    float alpha = texture(uSpaceTexture, uv).a * feather.x * feather.y;
    float breathe = 0.97 + 0.03 * sin(uTime * 0.19 + uv.x * 4.0);
    vec3 color = base.rgb * breathe;
    fragColor = vec4(color * alpha, alpha);
}
)GLSL";

const char *kNebulaVertexShader = R"GLSL(
#version 330 core
out vec3 vColor;
out float vOpacity;
out float vSeed;
out vec2 vCloudUv;
uniform float uAspect;
uniform float uZoom;
uniform float uFocalLength;
uniform mat4 uCamera;
uniform vec3 uCenters[14];
uniform vec3 uColors[14];

float hash11(float p)
{
    p = fract(p * 0.1031);
    p *= p + 33.33;
    p *= p + p;
    return fract(p);
}

void main()
{
    int puff = gl_VertexID / 6;
    int cluster = puff / 12;
    float id = float(puff) + 1.0;
    vec2 corners[6] = vec2[6](vec2(-1,-1), vec2(1,-1), vec2(1,1),
                              vec2(-1,-1), vec2(1,1), vec2(-1,1));
    vec2 corner = corners[gl_VertexID % 6];
    vCloudUv = corner * 0.5 + 0.5;
    float angle = hash11(id * 1.93 + 4.0) * 6.2831853;
    float spread = 0.50 + pow(hash11(id * 3.71 + 7.0), 0.72) * 1.80;
    vec3 offset = vec3(cos(angle), sin(angle),
                       hash11(id * 5.23 + 3.0) * 2.0 - 1.0) * spread;
    vec3 position = (uCamera * vec4(uCenters[cluster] + offset, 0.0)).xyz;
    vColor = uColors[cluster] * (0.70 + hash11(id * 11.19) * 0.55);
    vOpacity = 0.14 + hash11(id * 13.37) * 0.16;
    vSeed = hash11(id * 17.73);
    // The camera sits inside the sky sphere. Never clamp negative depth into
    // view: clouds on the other side must stay behind the observer.
    if (position.z <= 1.0) {
        gl_Position = vec4(0.0, 0.0, 2.0, 1.0);
        return;
    }
    float halfSize = 1.8 + hash11(id * 7.13) * 1.1;
    vec2 projected = (position.xy + corner * halfSize) * uFocalLength * uZoom;
    projected.x /= uAspect;
    // Real billboard triangles clip smoothly at the viewport, unlike point
    // sprites which disappear as soon as their center leaves the screen.
    gl_Position = vec4(projected, 0.0, position.z);
}
)GLSL";

const char *kNebulaFragmentShader = R"GLSL(
#version 330 core
in vec3 vColor;
in float vOpacity;
in float vSeed;
in vec2 vCloudUv;
out vec4 fragColor;
uniform vec2 uViewport;
uniform float uDpr;
uniform sampler2D uCloudTexture;

void main()
{
    vec2 p = vCloudUv * 2.0 - 1.0;
    float radius2 = dot(p, p);
    if (radius2 > 1.0) discard;
    float angle = vSeed * 6.283185;
    vec2 rotated = mat2(cos(angle), -sin(angle), sin(angle), cos(angle)) * p;
    vec4 cloud = texture(uCloudTexture, rotated * 0.5 + 0.5);
    float gas = cloud.r * cloud.a;
    vec3 ionized = mix(vColor, vec3(0.62, 0.83, 1.0), cloud.g * 0.50);
    ionized = mix(ionized, vec3(1.0, 0.61, 0.34), cloud.b * 0.18);
    vec2 screenEdge = min(gl_FragCoord.xy, uViewport - gl_FragCoord.xy);
    float screenFade = smoothstep(0.0, 10.0 * uDpr, screenEdge.x)
                       * smoothstep(0.0, 10.0 * uDpr, screenEdge.y);
    fragColor = vec4(ionized, vOpacity * gas * screenFade);
}
)GLSL";

const char *kStarVertexShader = R"GLSL(
#version 330 core
out vec3 vColor;
out float vAlpha;
uniform float uTime;
uniform float uAspect;
uniform float uDpr;
uniform float uYaw;
uniform float uPitch;
uniform float uZoom;

float hash11(float p)
{
    p = fract(p * 0.1031);
    p *= p + 33.33;
    p *= p + p;
    return fract(p);
}

mat3 rotateY(float a)
{
    float c = cos(a), s = sin(a);
    return mat3(c, 0.0, -s, 0.0, 1.0, 0.0, s, 0.0, c);
}

mat3 rotateX(float a)
{
    float c = cos(a), s = sin(a);
    return mat3(1.0, 0.0, 0.0, 0.0, c, -s, 0.0, s, c);
}

void main()
{
    float id = float(gl_VertexID) + 1.0;
    float h1 = hash11(id * 1.37);
    float h2 = hash11(id * 2.71 + 4.0);
    float h3 = hash11(id * 5.17 + 9.0);
    float z = h1 * 2.0 - 1.0;
    float angle = h2 * 6.2831853;
    float radius = mix(1.1, 7.2, pow(h3, 0.72));
    float ring = sqrt(max(0.0, 1.0 - z * z));
    vec3 position = vec3(cos(angle) * ring, z, sin(angle) * ring) * radius;
    position = rotateX(-0.08 + uPitch + sin(uTime * 0.035) * 0.025)
               * rotateY(uTime * 0.018 + uYaw) * position;

    float viewZ = position.z + 8.6;
    vec2 ndc = vec2(position.x / uAspect, position.y) * 1.18 * uZoom
               / max(viewZ, 1.2);
    gl_Position = vec4(ndc, 0.0, 1.0);
    float magnitude = pow(hash11(id * 7.91 + 2.0), 7.0);
    gl_PointSize = (0.65 + magnitude * 2.45) * uDpr * (1.0 + 2.2 / viewZ);
    float temperature = hash11(id * 11.73 + 0.7);
    vColor = temperature < 0.20 ? vec3(0.52, 0.75, 1.0)
             : temperature > 0.82 ? vec3(1.0, 0.70, 0.43)
                                  : vec3(0.88, 0.93, 1.0);
    vAlpha = 0.26 + magnitude * 0.74;
}
)GLSL";

const char *kPointFragmentShader = R"GLSL(
#version 330 core
in vec3 vColor;
in float vAlpha;
out vec4 fragColor;
uniform vec2 uViewport;
uniform float uDpr;
void main()
{
    vec2 p = gl_PointCoord * 2.0 - 1.0;
    float d2 = dot(p, p);
    if (d2 > 1.0) discard;
    float glow = exp(-d2 * 2.8);
    vec2 screenEdge = min(gl_FragCoord.xy, uViewport - gl_FragCoord.xy);
    float fade = smoothstep(0.0, 10.0 * uDpr, screenEdge.x)
                 * smoothstep(0.0, 10.0 * uDpr, screenEdge.y);
    fragColor = vec4(vColor * (0.42 + glow * 0.96), vAlpha * glow * fade);
}
)GLSL";

const char *kDiskVertexShader = R"GLSL(
#version 330 core
out vec3 vColor;
out float vAlpha;
uniform float uTime;
uniform float uAspect;
uniform float uDpr;
uniform float uCpu;
uniform float uMemory;
uniform float uYaw;
uniform float uPitch;
uniform float uZoom;

float hash11(float p)
{
    p = fract(p * 0.1031);
    p *= p + 33.33;
    p *= p + p;
    return fract(p);
}

mat3 rotateX(float a)
{
    float c = cos(a), s = sin(a);
    return mat3(1.0, 0.0, 0.0, 0.0, c, -s, 0.0, s, c);
}

mat3 rotateZ(float a)
{
    float c = cos(a), s = sin(a);
    return mat3(c, -s, 0.0, s, c, 0.0, 0.0, 0.0, 1.0);
}

mat3 rotateY(float a)
{
    float c = cos(a), s = sin(a);
    return mat3(c, 0.0, -s, 0.0, 1.0, 0.0, s, 0.0, c);
}

void main()
{
    float id = float(gl_VertexID) + 1.0;
    float h1 = hash11(id * 1.91);
    float h2 = hash11(id * 3.17 + 1.4);
    float h3 = hash11(id * 5.73 + 8.2);
    float radius = 0.25 + pow(h1, 0.61) * 1.55;
    float arm = mod(float(gl_VertexID), 3.0);
    float angularSpeed = (0.11 + uCpu * 0.13) * pow(0.82 / radius, 0.72);
    float angle = arm * 2.0943951 + radius * 3.55 + h2 * 0.73 + uTime * angularSpeed;
    float thickness = (h3 - 0.5) * (0.028 + radius * 0.052);
    vec3 position = vec3(cos(angle) * radius, thickness, sin(angle) * radius);
    position = rotateZ(-0.14)
               * rotateX(0.247 + uPitch)
               * rotateY(uYaw) * position;
    float viewZ = 4.25 + position.z * 0.34;
    vec2 ndc = vec2(position.x / uAspect, position.y) * 2.13 * uZoom / viewZ;
    ndc.y += 0.292;
    gl_Position = vec4(ndc, 0.0, 1.0);

    float magnitude = pow(hash11(id * 9.13 + 3.4), 4.5);
    gl_PointSize = (0.72 + magnitude * 2.30 + uCpu * 0.36) * uDpr;
    if (radius < 0.52) {
        vColor = vec3(1.0, 0.88, 0.62);
    } else if (arm < 0.5) {
        vColor = vec3(0.31, 0.83, 0.96);
    } else if (arm < 1.5) {
        vColor = vec3(1.0, 0.58, 0.31);
    } else {
        vColor = vec3(0.70, 0.51, 1.0);
    }
    vColor *= 0.74 + magnitude * 0.72 + uMemory * 0.12;
    vAlpha = 0.20 + magnitude * 0.68;
}
)GLSL";

const char *kShipVertexShader = R"GLSL(
#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aColor;
layout(location = 3) in vec2 aUv;
layout(location = 4) in vec3 aSurface;
uniform mat4 uModel;
uniform float uAspect;
uniform float uZoom;
out vec3 vPosition;
out vec3 vNormal;
out vec3 vAlbedo;
out vec2 vUv;
out vec3 vSurface;
void main()
{
    vPosition = (uModel * vec4(aPosition, 1.0)).xyz;
    vNormal = normalize(mat3(uModel) * aNormal);
    vAlbedo = aColor;
    vUv = aUv;
    vSurface = aSurface;
    // Same projected world scale and black-hole origin as the accretion field.
    float distance = 4.25 + vPosition.z * 0.34;
    vec2 clip = vec2(vPosition.x / uAspect, vPosition.y) * 2.13 * uZoom;
    clip.y += 0.292 * distance;
    gl_Position = vec4(clip, vPosition.z / 16.0 * distance, distance);
}
)GLSL";

const char *kShipFragmentShader = R"GLSL(
#version 330 core
in vec3 vPosition;
in vec3 vNormal;
in vec3 vAlbedo;
in vec2 vUv;
in vec3 vSurface;
uniform int uSide;
uniform sampler2DArray uMaterials;
out vec4 fragColor;

vec3 lightSurface(vec3 albedo, vec3 normal, vec3 view, vec3 light,
                  vec3 radiance, float roughness, float metallic)
{
    vec3 halfway = normalize(view + light);
    float nl = max(dot(normal, light), 0.0);
    float nv = max(dot(normal, view), 0.001);
    float nh = max(dot(normal, halfway), 0.0);
    float vh = max(dot(view, halfway), 0.0);
    float a2 = pow(roughness, 4.0);
    float denominator = nh * nh * (a2 - 1.0) + 1.0;
    float distribution = a2 / max(3.141593 * denominator * denominator, 0.00001);
    float k = (roughness + 1.0) * (roughness + 1.0) / 8.0;
    float geometry = nv / (nv * (1.0 - k) + k) * nl / (nl * (1.0 - k) + k);
    vec3 f0 = mix(vec3(0.04), albedo, metallic);
    vec3 fresnel = f0 + (1.0 - f0) * pow(1.0 - vh, 5.0);
    vec3 specular = distribution * geometry * fresnel / max(4.0 * nl * nv, 0.001);
    vec3 diffuse = (1.0 - fresnel) * (1.0 - metallic) * albedo / 3.141593;
    return (diffuse + specular) * radiance * nl;
}
void main()
{
    // Split the actual triangles around the black-hole image plane. The rear
    // pass is behind the lensed image and the front pass can cross in front of it.
    if (uSide < 0 && vPosition.z <= 0.0) discard;
    if (uSide > 0 && vPosition.z > 0.0) discard;
    // Real UV maps, sampled in linear light from an sRGB texture array.
    vec3 albedo = vAlbedo * texture(uMaterials, vec3(vUv, vSurface.x)).rgb;
    float roughness = clamp(vSurface.y, 0.10, 1.0);
    float metallic = clamp(vSurface.z, 0.0, 1.0);
    vec3 normal = normalize(vNormal);
    vec3 view = normalize(vec3(0.0, 0.0, -12.5) - vPosition);
    vec3 key = normalize(vec3(-2.0, 3.0, -4.0));
    vec3 holeLight = normalize(-vPosition);
    vec3 color = albedo * vec3(0.15, 0.19, 0.25);
    color += lightSurface(albedo, normal, view, key, vec3(3.1, 3.5, 4.0), roughness, metallic);
    color += lightSurface(albedo, normal, view, holeLight, vec3(1.6, 0.67, 0.20), roughness, metallic);
    float fresnel = pow(1.0 - max(dot(normal, view), 0.0), 5.0);
    vec3 reflectedSky = mix(vec3(0.015, 0.025, 0.04), vec3(0.16, 0.28, 0.40),
                            smoothstep(-0.3, 0.6, reflect(-view, normal).y));
    color += reflectedSky * (0.04 + fresnel) * (1.0 - roughness * 0.65);
    color = pow(1.0 - exp(-color * 1.25), vec3(1.0 / 2.2));
    fragColor = vec4(color, 1.0);
}
)GLSL";

const char *kRouteVertexShader = R"GLSL(
#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in float aProgress;
uniform mat4 uCamera;
uniform float uAspect;
uniform float uZoom;
out float vProgress;
out float vDepth;
void main()
{
    vec3 position = (uCamera * vec4(aPosition, 1.0)).xyz;
    float distance = 4.25 + position.z * 0.34;
    vec2 clip = vec2(position.x / uAspect, position.y) * 2.13 * uZoom;
    clip.y += 0.292 * distance;
    gl_Position = vec4(clip, position.z / 16.0 * distance, distance);
    vProgress = aProgress;
    vDepth = position.z;
}
)GLSL";

const char *kRouteFragmentShader = R"GLSL(
#version 330 core
in float vProgress;
in float vDepth;
uniform float uProgress;
uniform int uSide;
out vec4 fragColor;
void main()
{
    if (uSide < 0 && vDepth <= 0.0) discard;
    if (uSide > 0 && vDepth > 0.0) discard;
    float behind = fract(uProgress - vProgress + 1.0);
    float wake = 1.0 - smoothstep(0.0, 0.14, behind);
    float dash = 1.0 - smoothstep(0.58, 0.80, fract(vProgress * 72.0));
    float opacity = 0.14 * dash + 0.66 * wake;
    fragColor = vec4(mix(vec3(0.31, 0.63, 0.72), vec3(0.64, 0.94, 1.0), wake), opacity);
}
)GLSL";

const char *kHoleVertexShader = R"GLSL(
#version 330 core
out vec2 vUv;
uniform vec2 uHalfSize;
void main()
{
    vec2 corners[6] = vec2[6](vec2(-1,-1), vec2(1,-1), vec2(1,1),
                              vec2(-1,-1), vec2(1,1), vec2(-1,1));
    vec2 corner = corners[gl_VertexID];
    vUv = corner * 0.5 + 0.5;
    gl_Position = vec4(corner * uHalfSize + vec2(0.0, 0.292), 0.0, 1.0);
}
)GLSL";

const char *kHoleFragmentShader = R"GLSL(
#version 330 core
in vec2 vUv;
uniform sampler2D uImage;
out vec4 fragColor;
void main()
{
    fragColor = texture(uImage, vUv);
}
)GLSL";

QPainterPath panelPath()
{
    QPainterPath panel;
    panel.addRoundedRect(QRectF(0.7, 0.7, kWidth - 1.4, kHeight - 1.4), 20.0, 20.0);
    return panel;
}

quint32 hashValue(quint32 value)
{
    value ^= value >> 16;
    value *= 0x7feb352dU;
    value ^= value >> 15;
    value *= 0x846ca68bU;
    value ^= value >> 16;
    return value;
}

qreal hashUnit(quint32 seed)
{
    return (hashValue(seed) & 0xffffU) / 65535.0;
}

qreal smoothUnit(qreal value)
{
    value = qBound(0.0, value, 1.0);
    return value * value * (3.0 - 2.0 * value);
}

qreal latticeNoise(int x, int y, quint32 seed)
{
    const quint32 mixed = quint32(x) * 0x9e3779b1U
                          ^ quint32(y) * 0x85ebca77U ^ seed * 0xc2b2ae3dU;
    return hashUnit(mixed);
}

qreal valueNoise(qreal x, qreal y, quint32 seed)
{
    const int ix = qFloor(x);
    const int iy = qFloor(y);
    const qreal tx = smoothUnit(x - ix);
    const qreal ty = smoothUnit(y - iy);
    const qreal a = latticeNoise(ix, iy, seed);
    const qreal b = latticeNoise(ix + 1, iy, seed);
    const qreal c = latticeNoise(ix, iy + 1, seed);
    const qreal d = latticeNoise(ix + 1, iy + 1, seed);
    const qreal upper = a + (b - a) * tx;
    const qreal lower = c + (d - c) * tx;
    return upper + (lower - upper) * ty;
}

qreal fbm(qreal x, qreal y, quint32 seed, int octaves = 5)
{
    qreal result = 0.0;
    qreal amplitude = 0.54;
    qreal total = 0.0;
    for (int octave = 0; octave < octaves; ++octave) {
        result += valueNoise(x, y, seed + quint32(octave) * 37U) * amplitude;
        total += amplitude;
        const qreal nextX = x * 1.91 - y * 0.23 + 7.7;
        y = x * 0.21 + y * 1.93 - 5.1;
        x = nextX;
        amplitude *= 0.51;
    }
    return result / total;
}

qreal galacticBandY(qreal x)
{
    return 181.0 - x * 0.285 + qSin(x * 0.018 + 0.65) * 9.0;
}

QImage renderCloudTexture()
{
    // Density, ionized ridges and dust are generated once, not per frame.
    QImage texture(256, 256, QImage::Format_RGBA8888);
    for (int y = 0; y < texture.height(); ++y) {
        for (int x = 0; x < texture.width(); ++x) {
            const qreal px = (x + 0.5) / 128.0 - 1.0;
            const qreal py = (y + 0.5) / 128.0 - 1.0;
            const qreal warp = fbm(px * 2.4 + 9, py * 2.4 - 6, 503, 3);
            const qreal density = fbm(px * 5.3 + warp * 3, py * 5.3 - warp * 2, 701, 5);
            const qreal dust = fbm(px * 9 - warp * 4, py * 9 + warp * 2, 811, 3);
            const qreal folds = smoothUnit((density - 0.29) / 0.37);
            const qreal lane = 1.0 - smoothUnit((dust - 0.48) / 0.20) * 0.78;
            const qreal radius2 = px * px + py * py;
            const qreal envelope = (1.0 - smoothUnit((radius2 - 0.20) / 0.80))
                                   * qExp(-radius2 * 0.8);
            texture.setPixelColor(x, y, QColor::fromRgbF(folds * lane,
                smoothUnit((density - 0.49) / 0.22), smoothUnit((dust - 0.42) / 0.34), envelope));
        }
    }
    return texture;
}

QImage renderNebulaField()
{
    const int width = qRound(kWidth);
    const int height = qRound(kHeight);
    QImage field(width, height, QImage::Format_ARGB32);

    for (int y = 0; y < height; ++y) {
        auto *scanline = reinterpret_cast<QRgb *>(field.scanLine(y));
        for (int x = 0; x < width; ++x) {
            const qreal nx = x / kWidth;
            const qreal ny = y / kHeight;
            const qreal warpA = fbm(x * 0.010 + 13.2, y * 0.010 - 8.6, 19U, 4);
            const qreal warpB = fbm(x * 0.013 - 7.4, y * 0.013 + 18.1, 43U, 4);
            const qreal cloud = fbm(x * 0.024 + warpA * 2.7,
                                    y * 0.024 + warpB * 2.7, 71U, 5);
            const qreal detail = fbm(x * 0.061 - warpB * 3.1,
                                     y * 0.061 + warpA * 2.1, 109U, 4);

            const qreal bandDistance = qAbs(y - galacticBandY(x));
            const qreal bandWidth = 34.0 + warpA * 35.0;
            const qreal band = qExp(-qPow(bandDistance / bandWidth, 2.0));
            const qreal cloudy = smoothUnit((cloud - 0.40) / 0.34) * band;
            const qreal filament = smoothUnit((detail - 0.48) / 0.28) * band;

            // An irregular dark molecular lane splits the galactic band.
            const qreal laneOffset = (warpB - 0.5) * 25.0
                                     + qSin(x * 0.071 + warpA * 4.0) * 3.5;
            const qreal laneDistance = qAbs((y - galacticBandY(x)) - laneOffset);
            const qreal lane = qExp(-qPow(laneDistance / (4.8 + detail * 8.5), 2.0))
                               * (0.42 + detail * 0.58);

            qreal red = 2.5 + nx * 1.8 + cloud * 2.5;
            qreal green = 4.5 + (1.0 - ny) * 2.5 + cloud * 3.0;
            qreal blue = 11.0 + (1.0 - nx) * 5.0 + cloud * 5.0;

            const qreal warmWeight = qExp(-qPow((nx - 0.48) / 0.28, 2.0));
            const qreal cyanWeight = smoothUnit((nx - 0.42) / 0.48);
            const qreal violetWeight = 1.0 - smoothUnit(nx / 0.62);
            const qreal emission = cloudy * (0.34 + filament * 0.84);
            red += emission * (35.0 + warmWeight * 46.0 + violetWeight * 24.0);
            green += emission * (22.0 + warmWeight * 14.0 + cyanWeight * 39.0);
            blue += emission * (46.0 + violetWeight * 53.0 + cyanWeight * 48.0);

            const qreal stellarHaze = band * (0.17 + cloudy * 0.48)
                                      * (0.44 + detail * 0.56);
            red += stellarHaze * 31.0;
            green += stellarHaze * 30.0;
            blue += stellarHaze * 37.0;

            const qreal dustAttenuation = 1.0 - qBound(0.0, lane * 0.78, 0.78);
            red *= dustAttenuation;
            green *= dustAttenuation;
            blue *= dustAttenuation;

            const qreal vignetteX = qAbs(nx - 0.5) * 2.0;
            const qreal vignetteY = qAbs(ny - 0.44) * 1.55;
            const qreal vignette = 1.0 - qBound(0.0,
                                               (vignetteX * vignetteX
                                                + vignetteY * vignetteY) * 0.19,
                                               0.58);
            red *= vignette;
            green *= vignette;
            blue *= vignette;

            scanline[x] = qRgba(qBound(0, qRound(red), 255),
                                qBound(0, qRound(green), 255),
                                qBound(0, qRound(blue), 255), 232);
        }
    }
    return field;
}

QColor blackbodyStar(qreal temperature, int alpha)
{
    if (temperature < 0.18)
        return QColor(142, 194, 255, alpha);
    if (temperature > 0.82)
        return QColor(255, 198, 137, alpha);
    if (temperature > 0.61)
        return QColor(255, 231, 193, alpha);
    return QColor(225, 236, 255, alpha);
}

void drawStaticStars(QPainter &painter)
{
    painter.save();
    painter.setCompositionMode(QPainter::CompositionMode_Screen);
    painter.setPen(Qt::NoPen);

    constexpr quint32 starCount = 292;
    for (quint32 index = 0; index < starCount; ++index) {
        const qreal x = 7.0 + hashUnit(index * 17U + 11U) * (kWidth - 14.0);
        const bool inBand = hashUnit(index * 23U + 47U) < 0.58;
        qreal y = 6.0 + hashUnit(index * 31U + 83U) * (kHeight - 18.0);
        if (inBand) {
            const qreal spread = (hashUnit(index * 41U + 107U)
                                  + hashUnit(index * 43U + 139U)
                                  + hashUnit(index * 53U + 167U) - 1.5) * 74.0;
            y = galacticBandY(x) + spread;
        }
        if (y < 6.0 || y > kHeight - 10.0)
            continue;

        const qreal magnitude = qPow(hashUnit(index * 61U + 191U), 9.0);
        const qreal brightness = 0.12 + magnitude * 0.88;
        const qreal size = 0.16 + magnitude * 0.83;
        const int alpha = qRound(62.0 + brightness * 173.0);
        const QColor color = blackbodyStar(hashUnit(index * 67U + 223U), alpha);
        painter.setBrush(color);
        painter.drawEllipse(QPointF(x, y), size, size);

        if (magnitude > 0.84) {
            QColor halo = color;
            halo.setAlpha(qRound(alpha * 0.16));
            painter.setBrush(halo);
            painter.drawEllipse(QPointF(x, y), size * 3.6, size * 3.6);
        }
    }
    painter.restore();
}

QPixmap renderCosmicLayer()
{
    constexpr qreal supersampling = 2.0;
    QPixmap layer(QSize(qRound(kWidth * supersampling), qRound(kHeight * supersampling)));
    layer.setDevicePixelRatio(supersampling);
    layer.fill(Qt::transparent);

    QPainter painter(&layer);
    painter.setRenderHint(QPainter::Antialiasing);
    const QImage nebula = renderNebulaField();
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawImage(QRectF(0.0, 0.0, kWidth, kHeight), nebula,
                      QRectF(0.0, 0.0, nebula.width(), nebula.height()));

    drawStaticStars(painter);

    // Preserve the interior. A 10 logical-pixel feather is applied in screen
    // space by the shader, independent of texture resolution and scene zoom.
    return layer;
}

void drawTwinklingStars(QPainter &painter, qreal phase)
{
    painter.save();
    painter.setCompositionMode(QPainter::CompositionMode_Screen);
    for (quint32 index = 0; index < 17; ++index) {
        const qreal x = 10.0 + hashUnit(index * 31U + 67U) * (kWidth - 20.0);
        const qreal y = 9.0 + hashUnit(index * 43U + 83U) * 224.0;
        const qreal pulse = 0.5 + 0.5 * qSin(phase * (0.025 + (index % 4U) * 0.006)
                                             + index * 1.71);
        const qreal size = 0.55 + hashUnit(index * 53U + 97U) * 1.05;
        const int alpha = qRound(45.0 + pulse * 155.0);
        const QColor color = index % 3U == 0U ? QColor(173, 226, 255, alpha)
                                              : QColor(255, 239, 219, alpha);
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawEllipse(QPointF(x, y), size, size);
        if (index % 6U == 0U && pulse > 0.68) {
            painter.setPen(QPen(QColor(color.red(), color.green(), color.blue(), alpha / 2), 0.55,
                                Qt::SolidLine, Qt::RoundCap));
            painter.drawLine(QPointF(x - 4.0 * pulse, y), QPointF(x + 4.0 * pulse, y));
            painter.drawLine(QPointF(x, y - 4.0 * pulse), QPointF(x, y + 4.0 * pulse));
        }
    }
    painter.restore();
}

QPointF diskParticlePosition(quint32 index, qreal phase, qreal activity)
{
    constexpr qreal pi = 3.14159265358979323846;
    const qreal radialSeed = hashUnit(index * 79U + 101U);
    const qreal angularSeed = hashUnit(index * 97U + 131U);
    const qreal scatterSeed = hashUnit(index * 59U + 173U) - 0.5;
    const int arm = int(index % 3U);
    const qreal radius = 35.0 + qPow(radialSeed, 0.64) * 101.0;
    const qreal armAngle = arm * (2.0 * pi / 3.0) + radius * 0.044;
    const qreal scatter = scatterSeed * (0.22 + radius * 0.0027);
    const qreal angularSpeed = 0.0021 * qPow(76.0 / radius, 1.32) * (0.72 + activity * 0.48);
    const qreal angle = armAngle + angularSeed * 0.72 + scatter + phase * angularSpeed;
    const qreal thickness = (hashUnit(index * 113U + 211U) - 0.5)
                            * (2.2 + radius * 0.038);

    QPointF point(qCos(angle) * radius,
                  qSin(angle) * radius * (0.31 + radius * 0.00045) + thickness);
    const qreal tilt = -0.14;
    return QPointF(kCenterX + point.x() * qCos(tilt) - point.y() * qSin(tilt),
                   kCenterY + point.x() * qSin(tilt) + point.y() * qCos(tilt));
}

QColor diskParticleColor(quint32 index, qreal radiusFraction, int alpha)
{
    if (radiusFraction < 0.22)
        return QColor(255, 244, 214, alpha);
    if (index % 9U < 3U)
        return QColor(103, 211, 235, alpha);
    if (index % 9U < 6U)
        return QColor(242, 169, 101, alpha);
    return QColor(186, 151, 255, alpha);
}

void drawNBodyDisk(QPainter &painter, qreal phase, qreal cpuActivity)
{
    painter.save();
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.setBrush(Qt::NoBrush);

    // A few very soft arm traces keep the cloud readable even between particles.
    for (int arm = 0; arm < 3; ++arm) {
        QPainterPath path;
        for (int sample = 0; sample < 54; ++sample) {
            const qreal radius = 37.0 + sample * 1.88;
            const qreal angle = arm * 2.0943951023931953 + radius * 0.044
                                + phase * 0.00135;
            QPointF point(qCos(angle) * radius, qSin(angle) * radius * 0.36);
            const qreal tilt = -0.14;
            point = QPointF(kCenterX + point.x() * qCos(tilt) - point.y() * qSin(tilt),
                            kCenterY + point.x() * qSin(tilt) + point.y() * qCos(tilt));
            sample == 0 ? path.moveTo(point) : path.lineTo(point);
        }
        const QColor trace = arm == 0 ? QColor(65, 192, 225, 6)
                                      : arm == 1 ? QColor(242, 146, 94, 5)
                                                 : QColor(171, 116, 238, 5);
        painter.setPen(QPen(trace, 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPath(path);
    }

    constexpr quint32 particleCount = 126;
    for (quint32 index = 0; index < particleCount; ++index) {
        const QPointF point = diskParticlePosition(index, phase, cpuActivity);
        const QPointF previous = diskParticlePosition(index, phase - 9.0, cpuActivity);
        const qreal dx = point.x() - kCenterX;
        const qreal dy = point.y() - kCenterY;
        const qreal radiusFraction = qBound(0.0, qSqrt(dx * dx + dy * dy) / 137.0, 1.0);
        const qreal magnitude = qPow(hashUnit(index * 127U + 257U), 5.0);
        const qreal pulse = 0.82 + 0.18 * qSin(phase * 0.023 + index * 0.91);
        const int alpha = qRound((42.0 + magnitude * 144.0) * pulse);
        const QColor color = diskParticleColor(index, radiusFraction, alpha);
        const qreal size = 0.20 + magnitude * 0.72;

        if (magnitude > 0.38) {
            QColor streakColor = color;
            streakColor.setAlpha(qRound(alpha * 0.58));
            painter.setPen(QPen(streakColor, qMax(0.45, size * 0.62),
                                Qt::SolidLine, Qt::RoundCap));
            const qreal streakScale = 0.55 + magnitude * (1.35 + cpuActivity * 1.45);
            painter.drawLine(point - (point - previous) * streakScale, point);
        }
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawEllipse(point, size, size);
        if (magnitude > 0.90) {
            QColor halo = color;
            halo.setAlpha(qRound(alpha * 0.10));
            painter.setBrush(halo);
            painter.drawEllipse(point, size * 3.0, size * 3.0);
        }
    }
    painter.restore();
}

void drawOrbitingBody(QPainter &painter, qreal phase, qreal activity,
                      qreal radiusX, qreal radiusY, qreal phaseOffset,
                      const QColor &color, bool clockwise)
{
    const qreal direction = clockwise ? 1.0 : -1.0;
    const qreal angle = phase * 0.0045 * direction + phaseOffset;
    const int segments = 30;
    painter.save();
    painter.setCompositionMode(QPainter::CompositionMode_Plus);
    for (int index = segments - 1; index >= 0; --index) {
        const qreal fade = 1.0 - qreal(index) / segments;
        const qreal trailAngle = angle - direction * index * (0.020 + activity * 0.006);
        const QPointF from(kCenterX + qCos(trailAngle) * radiusX,
                           kCenterY + qSin(trailAngle) * radiusY);
        const qreal nextAngle = trailAngle - direction * 0.022;
        const QPointF to(kCenterX + qCos(nextAngle) * radiusX,
                         kCenterY + qSin(nextAngle) * radiusY);
        QColor segmentColor = color;
        segmentColor.setAlphaF((0.025 + 0.34 * fade * fade) * (0.55 + activity * 0.45));
        painter.setPen(QPen(segmentColor, 0.45 + fade * 1.15,
                            Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(from, to);
    }

    const QPointF body(kCenterX + qCos(angle) * radiusX,
                       kCenterY + qSin(angle) * radiusY);
    QColor glow = color;
    glow.setAlpha(42 + qRound(activity * 58));
    painter.setPen(Qt::NoPen);
    painter.setBrush(glow);
    painter.drawEllipse(body, 5.8, 5.8);
    painter.setBrush(color);
    painter.drawEllipse(body, 1.45 + activity * 0.65, 1.45 + activity * 0.65);
    painter.setBrush(QColor(255, 255, 244, 230));
    painter.drawEllipse(body - QPointF(0.35, 0.35), 0.55, 0.55);
    painter.restore();
}

void drawComet(QPainter &painter, qreal phase)
{
    const qreal cycle = phase / 420.0;
    const qreal progress = cycle - qFloor(cycle);
    if (progress > 0.42)
        return;

    const qreal travel = progress / 0.42;
    const QPointF head(30.0 + travel * 128.0, 48.0 + travel * 54.0);
    const QPointF tail = head - QPointF(30.0, 13.0);
    const qreal fade = qSin(travel * 3.14159265358979323846);
    QLinearGradient streak(tail, head);
    streak.setColorAt(0.0, QColor(116, 191, 255, 0));
    streak.setColorAt(0.74, QColor(171, 218, 255, qRound(88 * fade)));
    streak.setColorAt(1.0, QColor(255, 249, 231, qRound(220 * fade)));
    painter.setPen(QPen(QBrush(streak), 1.15, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(tail, head);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 250, 229, qRound(230 * fade)));
    painter.drawEllipse(head, 1.3, 1.3);
}

void drawGravityReticle(QPainter &painter, qreal phase, qreal activity)
{
    painter.save();
    painter.translate(kCenterX, kCenterY);
    painter.rotate(phase * 0.075);
    const qreal rx = 87.0;
    const qreal ry = 54.0;
    for (int index = 0; index < 36; ++index) {
        const qreal angle = qDegreesToRadians(index * 10.0);
        const qreal emphasis = index % 6 == 0 ? 1.0 : 0.45;
        const QPointF inner(qCos(angle) * rx, qSin(angle) * ry);
        const QPointF outer(qCos(angle) * (rx + 3.0 + emphasis * 2.0),
                            qSin(angle) * (ry + 2.0 + emphasis * 2.0));
        const QColor color = index % 2 == 0
                                 ? QColor(143, 213, 255, qRound(28 + emphasis * 43))
                                 : QColor(255, 179, 216, qRound(23 + emphasis * 39));
        painter.setPen(QPen(color, emphasis > 0.8 ? 0.9 : 0.55,
                            Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(inner, outer);
    }
    painter.restore();

    const QRectF scanner(kCenterX - 91.0, kCenterY - 58.0, 182.0, 116.0);
    const int startAngle = qRound(-phase * 0.9) % 360;
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(119, 224, 224, qRound(48 + activity * 62)),
                        1.2, Qt::SolidLine, Qt::RoundCap));
    painter.drawArc(scanner, startAngle * 16, 42 * 16);
    painter.setPen(QPen(QColor(255, 206, 143, qRound(28 + activity * 45)),
                        0.75, Qt::DashLine, Qt::RoundCap));
    painter.drawArc(scanner.adjusted(4.0, 3.0, -4.0, -3.0),
                    (startAngle + 176) * 16, 58 * 16);
}

QFont font(int pixelSize, int weight = QFont::Normal)
{
    return ThemeFonts::text(pixelSize, static_cast<QFont::Weight>(weight));
}

void drawText(QPainter &painter, const QRectF &rect, const QString &value,
              const QColor &color, int size, int weight = QFont::Normal,
              Qt::Alignment alignment = Qt::AlignLeft | Qt::AlignVCenter)
{
    painter.setFont(font(size, weight));
    painter.setPen(color);
    painter.drawText(rect, alignment, value);
}

void drawElided(QPainter &painter, const QRectF &rect, const QString &value,
                const QColor &color, int size,
                Qt::Alignment alignment = Qt::AlignLeft | Qt::AlignVCenter)
{
    painter.setFont(font(size));
    painter.setPen(color);
    const QString elided = QFontMetrics(painter.font()).elidedText(
        value, Qt::ElideRight, qMax(0, qRound(rect.width())));
    painter.drawText(rect, alignment, elided);
}

void drawReadout(QPainter &painter, bool right, qreal y, const QString &name,
                 const QString &value, const QString &detail, const QColor &accent)
{
    const qreal x = right ? 264.0 : 17.0;
    const Qt::Alignment alignment = (right ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter;
    const QRectF field(x, y, 83.0, 18.0);

    painter.setPen(Qt::NoPen);
    painter.setBrush(accent);
    painter.drawEllipse(QPointF(right ? 344.0 : 20.0, y + 7.0), 2.3, 2.3);
    drawText(painter, field.adjusted(right ? 0 : 10, 0, right ? -10 : 0, 0),
             name, kMuted, 10, QFont::Medium, alignment);
    drawText(painter, QRectF(x, y + 19.0, 83.0, 30.0), value, kWhite,
             value.size() > 4 ? 17 : 21, QFont::Bold, alignment);
    drawElided(painter, QRectF(x, y + 47.0, 83.0, 14.0), detail,
               accent, 9, alignment);
}

void drawOrbit(QPainter &painter, const QRectF &ellipse, qreal ratio,
               const QColor &color, int startDegrees, int spanDegrees)
{
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(255, 235, 234, 14), 0.75));
    painter.drawEllipse(ellipse);
    if (ratio < 0.0)
        return;

    const int clampedSpan = qRound(qBound(0.0, ratio / 100.0, 1.0) * spanDegrees * 16.0);
    if (clampedSpan == 0)
        return;

    painter.setPen(QPen(QColor(color.red(), color.green(), color.blue(), 29),
                        3.2, Qt::SolidLine, Qt::RoundCap));
    painter.drawArc(ellipse, startDegrees * 16, clampedSpan);
    painter.setPen(QPen(QColor(color.red(), color.green(), color.blue(), 205),
                        1.25, Qt::SolidLine, Qt::RoundCap));
    painter.drawArc(ellipse, startDegrees * 16, clampedSpan);
}

qreal flowAmount(double bytesPerSecond)
{
    if (bytesPerSecond < 1.0)
        return 0.0;
    return qBound(0.0, qLn(1.0 + bytesPerSecond) / qLn(1.0e8), 1.0);
}

void drawFlow(QPainter &painter, qreal phase, double bytesPerSecond,
              bool receiving)
{
    const qreal amount = flowAmount(bytesPerSecond);
    if (amount <= 0.0)
        return;

    const int particles = 2 + qRound(amount * 8.0);
    const qreal start = receiving ? 34.0 : kCenterX;
    const qreal finish = receiving ? kCenterX : 330.0;
    const qreal y = receiving ? 213.0 : 219.0;
    const QColor color = receiving ? QColor(QStringLiteral("#8BEAC9"))
                                   : QColor(QStringLiteral("#F5BA8B"));

    for (int index = 0; index < particles; ++index) {
        const qreal progress = qreal(index) / particles + phase * (0.00065 + amount * 0.0013);
        const qreal fraction = progress - qFloor(progress);
        const qreal x = start + (finish - start) * fraction;
        const qreal curve = qSin(fraction * 3.14159265358979323846) * 3.4;
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(color.red(), color.green(), color.blue(),
                                qRound((0.42 + amount * 0.48) * 255)));
        painter.drawEllipse(QPointF(x, y - curve), 0.75 + amount * 0.7,
                            0.75 + amount * 0.7);
    }
}

void drawJet(QPainter &painter, qreal usagePercent)
{
    if (usagePercent <= 0.0)
        return;
    const qreal activity = qBound(0.0, usagePercent / 100.0, 1.0);
    QLinearGradient jet(kCenterX, 9.0, kCenterX, 54.0);
    jet.setColorAt(0.0, QColor(130, 205, 255, 0));
    jet.setColorAt(0.65, QColor(156, 196, 252, qRound(28 + activity * 70)));
    jet.setColorAt(1.0, QColor(252, 191, 231, qRound(42 + activity * 100)));
    QPainterPath plume;
    plume.moveTo(kCenterX - 1.5, 54.0);
    plume.quadTo(kCenterX - 5.0 - activity * 3.0, 28.0, kCenterX - 2.0, 9.0);
    plume.lineTo(kCenterX + 2.0, 9.0);
    plume.quadTo(kCenterX + 5.0 + activity * 3.0, 28.0, kCenterX + 1.5, 54.0);
    plume.closeSubpath();
    painter.setPen(Qt::NoPen);
    painter.setBrush(jet);
    painter.drawPath(plume);
}
}

class StarOverlayWidget final : public QWidget
{
public:
    explicit StarOverlayWidget(StarMapWidget *owner)
        : QWidget(owner), m_owner(owner)
    {
        setAttribute(Qt::WA_NoSystemBackground);
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_TransparentForMouseEvents);
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event);
        QPainter painter(this);
        m_owner->paintOverlay(painter);
    }

private:
    StarMapWidget *m_owner = nullptr;
};

StarMapWidget::StarMapWidget(QWidget *parent)
    : QOpenGLWidget(parent)
{
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setAlphaBufferSize(8);
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(0);
    format.setSamples(4);
    setFormat(format);
    setUpdateBehavior(QOpenGLWidget::NoPartialUpdate);

    setFixedHeight(458);
    setMinimumWidth(340);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TranslucentBackground);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setCursor(Qt::OpenHandCursor);

    m_cosmicLayer = renderCosmicLayer();
    m_elapsed.start();
    m_overlay = new StarOverlayWidget(this);
    m_tooltip = new TelemetryToolTip(this);
    m_blackHole = new BlackHoleWidget(this, 210);
    // The cached painter output is a texture in this scene, not a child overlay.
    // This lets the mesh pass both behind and in front of the black hole.
    m_blackHole->hide();
    m_timer = new QTimer(this);
    m_timer->setTimerType(Qt::CoarseTimer);
    m_timer->setInterval(80);
    connect(m_timer, &QTimer::timeout, this, [this] {
        const double dt = m_flightClock.isValid() ? m_flightClock.restart() / 1000.0 : 0.0;
        m_flightSeconds += dt;
        m_phase += 25.0 * dt;
        // Flight is smooth, while the expensive lensed light texture is cached.
        const double holeDt = m_flightSeconds - m_lastHoleAnimationSeconds;
        if (holeDt >= 0.25) {
            m_blackHole->advanceAnimation(18.08 * holeDt);
            m_lastHoleAnimationSeconds = m_flightSeconds;
            m_holeTextureDirty = true;
        }
        update();
        m_overlay->update();
    });
}

StarMapWidget::~StarMapWidget()
{
    if (context()) {
        makeCurrent();
        cleanupGL();
        doneCurrent();
    }
}

void StarMapWidget::initializeGL()
{
    initializeOpenGLFunctions();
    connect(context(), &QOpenGLContext::aboutToBeDestroyed, this, [this] {
        makeCurrent();
        cleanupGL();
        doneCurrent();
    }, Qt::DirectConnection);

    auto buildProgram = [this](const char *vertexSource, const char *fragmentSource,
                               const char *label) -> QOpenGLShaderProgram * {
        auto *program = new QOpenGLShaderProgram(this);
        const bool compiled = program->addShaderFromSourceCode(QOpenGLShader::Vertex,
                                                                vertexSource)
                              && program->addShaderFromSourceCode(QOpenGLShader::Fragment,
                                                                  fragmentSource)
                              && program->link();
        if (!compiled) {
            qWarning("OpenGL %s shader failed: %s", label,
                     qPrintable(program->log()));
            delete program;
            return nullptr;
        }
        return program;
    };

    m_spaceProgram = buildProgram(kFullscreenVertexShader, kSpaceFragmentShader, "space");
    m_nebulaProgram = buildProgram(kNebulaVertexShader,
                                   kNebulaFragmentShader, "nebula");
    m_starProgram = buildProgram(kStarVertexShader, kPointFragmentShader, "star");
    m_diskProgram = buildProgram(kDiskVertexShader, kPointFragmentShader, "disk");
    m_shipProgram = buildProgram(kShipVertexShader, kShipFragmentShader, "spacecraft");
    m_holeProgram = buildProgram(kHoleVertexShader, kHoleFragmentShader, "black-hole image");
    m_routeProgram = buildProgram(kRouteVertexShader, kRouteFragmentShader, "flight path");
    glGenVertexArrays(1, &m_emptyVao);

    const QByteArray renderer(reinterpret_cast<const char *>(glGetString(GL_RENDERER)));
    const bool softwareRenderer = renderer.contains("llvmpipe") || renderer.contains("softpipe");
    m_timer->setInterval(softwareRenderer ? 125 : 80);
    std::array<float, kRouteSamples * 4> route{};
    for (int i = 0; i < kRouteSamples; ++i) {
        const float fraction = float(i) / (kRouteSamples - 1);
        const QVector3D point = FlightPath::point(fraction);
        route[i * 4] = point.x();
        route[i * 4 + 1] = point.y();
        route[i * 4 + 2] = point.z();
        route[i * 4 + 3] = fraction;
    }
    glGenVertexArrays(1, &m_routeVao);
    glGenBuffers(1, &m_routeVbo);
    glBindVertexArray(m_routeVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_routeVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(route), route.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          reinterpret_cast<void *>(3 * sizeof(float)));
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    const QImage textureImage = m_cosmicLayer.toImage()
                                    .convertToFormat(QImage::Format_RGBA8888)
                                    .mirrored(false, true);
    glGenTextures(1, &m_spaceTexture);
    glBindTexture(GL_TEXTURE_2D, m_spaceTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, textureImage.width(), textureImage.height(),
                 0, GL_RGBA, GL_UNSIGNED_BYTE, textureImage.constBits());
    glBindTexture(GL_TEXTURE_2D, 0);

    const QImage cloudTexture = renderCloudTexture();
    glGenTextures(1, &m_nebulaTexture);
    glBindTexture(GL_TEXTURE_2D, m_nebulaTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, cloudTexture.width(), cloudTexture.height(),
                 0, GL_RGBA, GL_UNSIGNED_BYTE, cloudTexture.constBits());
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);

    QString meshError;
    QStringList texturePaths;
    const auto mesh = loadSpacecraftMesh(QStringLiteral(":/stellarsue/spacecraft.obj"), &meshError, &texturePaths);
    if (!mesh.isEmpty()) {
        m_shipVertexCount = int(mesh.size());
        glGenVertexArrays(1, &m_shipVao);
        glGenBuffers(1, &m_shipVbo);
        glBindVertexArray(m_shipVao);
        glBindBuffer(GL_ARRAY_BUFFER, m_shipVbo);
        glBufferData(GL_ARRAY_BUFFER, mesh.size() * sizeof(SpacecraftVertex),
                     mesh.constData(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(SpacecraftVertex),
                              reinterpret_cast<void *>(offsetof(SpacecraftVertex, position)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(SpacecraftVertex),
                              reinterpret_cast<void *>(offsetof(SpacecraftVertex, normal)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(SpacecraftVertex),
                              reinterpret_cast<void *>(offsetof(SpacecraftVertex, color)));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(SpacecraftVertex),
                              reinterpret_cast<void *>(offsetof(SpacecraftVertex, uv)));
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(SpacecraftVertex),
                              reinterpret_cast<void *>(offsetof(SpacecraftVertex, surface)));
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
        glGenTextures(1, &m_shipTexture);
        glBindTexture(GL_TEXTURE_2D_ARRAY, m_shipTexture);
        constexpr int textureSize = 512;
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_SRGB8_ALPHA8, textureSize, textureSize,
                     texturePaths.size() + 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        for (int layer = 0; layer <= texturePaths.size(); ++layer) {
            QImage image(textureSize, textureSize, QImage::Format_RGBA8888);
            image.fill(Qt::white);
            if (layer > 0) {
                const QImage source(texturePaths[layer - 1]);
                if (source.isNull()) {
                    qWarning("Spacecraft texture failed: %s", qPrintable(texturePaths[layer - 1]));
                    m_shipVertexCount = 0;
                    break;
                }
                image = source.scaled(textureSize, textureSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                            .convertToFormat(QImage::Format_RGBA8888).mirrored(false, true);
            }
            glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, layer, textureSize, textureSize, 1,
                            GL_RGBA, GL_UNSIGNED_BYTE, image.constBits());
        }
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
        glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    } else {
        qWarning("Spacecraft resource failed: %s", qPrintable(meshError));
    }
    glGenTextures(1, &m_holeTexture);
    glBindTexture(GL_TEXTURE_2D, m_holeTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_blackHole->width() * 2,
                 m_blackHole->height() * 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);

    m_holeTextureDirty = true;
    m_glReady = m_spaceProgram && m_nebulaProgram && m_starProgram && m_diskProgram && m_holeProgram
                && m_emptyVao != 0 && m_spaceTexture != 0;
}

void StarMapWidget::cleanupGL()
{
    m_glReady = false;
    if (m_emptyVao != 0) {
        glDeleteVertexArrays(1, &m_emptyVao);
        m_emptyVao = 0;
    }
    if (m_spaceTexture != 0) {
        glDeleteTextures(1, &m_spaceTexture);
        m_spaceTexture = 0;
    }
    if (m_shipVao) glDeleteVertexArrays(1, &m_shipVao);
    if (m_shipVbo) glDeleteBuffers(1, &m_shipVbo);
    if (m_shipTexture) glDeleteTextures(1, &m_shipTexture);
    if (m_nebulaTexture) glDeleteTextures(1, &m_nebulaTexture);
    if (m_holeTexture) glDeleteTextures(1, &m_holeTexture);
    if (m_routeVao) glDeleteVertexArrays(1, &m_routeVao);
    if (m_routeVbo) glDeleteBuffers(1, &m_routeVbo);
    m_shipVao = m_shipVbo = m_holeTexture = 0;
    m_shipTexture = 0;
    m_nebulaTexture = 0;
    m_routeVao = m_routeVbo = 0;
    m_shipVertexCount = 0;
    delete m_spaceProgram;
    delete m_nebulaProgram;
    delete m_starProgram;
    delete m_diskProgram;
    delete m_shipProgram;
    delete m_holeProgram;
    delete m_routeProgram;
    m_spaceProgram = nullptr;
    m_nebulaProgram = nullptr;
    m_starProgram = nullptr;
    m_diskProgram = nullptr;
    m_shipProgram = nullptr;
    m_holeProgram = nullptr;
    m_routeProgram = nullptr;
}

QMatrix4x4 StarMapWidget::cameraRotation() const
{
    return NebulaEnvironment::cameraRotation(m_cameraYaw, m_cameraPitch);
}

QVector<int> StarMapWidget::visibleNebulaClouds() const
{
    return NebulaEnvironment::visibleClouds(m_cameraYaw, m_cameraPitch, m_cameraZoom,
                                           width() / qreal(qMax(1, height())));
}

QMatrix4x4 StarMapWidget::spacecraftTransform() const
{
    return cameraRotation() * FlightPath::sample(m_flightSeconds).model;
}

void StarMapWidget::renderFlightPath(int side)
{
    if (!m_routeProgram || !m_routeVao) return;
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glBindVertexArray(m_routeVao);
    m_routeProgram->bind();
    m_routeProgram->setUniformValue("uCamera", cameraRotation());
    m_routeProgram->setUniformValue("uAspect", float(width()) / qMax(1, height()));
    m_routeProgram->setUniformValue("uZoom", float(m_cameraZoom));
    m_routeProgram->setUniformValue("uProgress", float(FlightPath::progress(m_flightSeconds)));
    m_routeProgram->setUniformValue("uSide", side);
    glLineWidth(1.0F);
    glDrawArrays(GL_LINE_STRIP, 0, kRouteSamples);
    m_routeProgram->release();
    glBindVertexArray(m_emptyVao);
    glDepthMask(GL_TRUE);
    glDisable(GL_DEPTH_TEST);
}

QPointF StarMapWidget::spacecraftScreenPosition() const
{
    const QVector3D point = spacecraftTransform().map(QVector3D());
    const qreal distance = 4.25 + point.z() * 0.34;
    const qreal aspect = width() / qreal(qMax(1, height()));
    const qreal x = point.x() / aspect * 2.13 * m_cameraZoom / distance;
    const qreal y = point.y() * 2.13 * m_cameraZoom / distance + 0.292;
    return QPointF((x + 1.0) * width() * 0.5, (1.0 - y) * height() * 0.5);
}

void StarMapWidget::renderSpacecraft(int side, const QMatrix4x4 &transform)
{
    if (!spacecraftReady()) return;
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    glDisable(GL_BLEND);
    glBindVertexArray(m_shipVao);
    m_shipProgram->bind();
    m_shipProgram->setUniformValue("uModel", transform);
    m_shipProgram->setUniformValue("uAspect", float(width()) / qMax(1, height()));
    m_shipProgram->setUniformValue("uZoom", float(m_cameraZoom));
    m_shipProgram->setUniformValue("uSide", side);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_shipTexture);
    m_shipProgram->setUniformValue("uMaterials", 0);
    glDrawArrays(GL_TRIANGLES, 0, m_shipVertexCount);
    m_shipProgram->release();
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    glBindVertexArray(m_emptyVao);
    glDisable(GL_DEPTH_TEST);
}

void StarMapWidget::renderBlackHole()
{
    if (!m_holeProgram || !m_holeTexture) return;
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_holeTexture);
    if (m_holeTextureDirty) {
        const QImage frame = m_blackHole->renderFrame().mirrored(false, true);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, frame.width(), frame.height(),
                        GL_RGBA, GL_UNSIGNED_BYTE, frame.constBits());
        m_holeTextureDirty = false;
    }
    m_holeProgram->bind();
    m_holeProgram->setUniformValue("uImage", 0);
    m_holeProgram->setUniformValue("uHalfSize",
        QVector2D(float(m_blackHole->width() * m_cameraZoom / width()),
                  float(m_blackHole->height() * m_cameraZoom / height())));
    glDrawArrays(GL_TRIANGLES, 0, 6);
    m_holeProgram->release();
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_BLEND);
}

void StarMapWidget::renderOpenGLScene()
{
    const qreal dpr = devicePixelRatioF();
    const int viewportWidth = qMax(1, qRound(width() * dpr));
    const int viewportHeight = qMax(1, qRound(height() * dpr));
    const float seconds = float(m_elapsed.elapsed()) / 1000.0F;
    const float aspect = float(width()) / float(qMax(1, height()));
    const float cpu = float(qBound(0.0, m_snapshot.cpuUsagePercent / 100.0, 1.0));
    const float memory = float(qBound(0.0, m_snapshot.memoryUsagePercent / 100.0, 1.0));

    glViewport(0, 0, viewportWidth, viewportHeight);
    glClearColor(0.0F, 0.0F, 0.0F, 0.0F);
    glDepthMask(GL_TRUE);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glBindVertexArray(m_emptyVao);

    glDisable(GL_BLEND);
    m_spaceProgram->bind();
    m_spaceProgram->setUniformValue("uTime", seconds);
    m_spaceProgram->setUniformValue("uAspect", aspect);
    m_spaceProgram->setUniformValue("uCamera",
                                    QVector2D(float(m_cameraYaw), float(m_cameraPitch)));
    m_spaceProgram->setUniformValue("uViewport", QVector2D(float(viewportWidth), float(viewportHeight)));
    m_spaceProgram->setUniformValue("uDpr", float(dpr));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_spaceTexture);
    m_spaceProgram->setUniformValue("uSpaceTexture", 0);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindTexture(GL_TEXTURE_2D, 0);
    m_spaceProgram->release();

    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_ZERO, GL_ONE);

    m_nebulaProgram->bind();
    m_nebulaProgram->setUniformValue("uAspect", aspect);
    m_nebulaProgram->setUniformValue("uDpr", float(dpr));
    m_nebulaProgram->setUniformValue("uViewport",
                                     QVector2D(float(viewportWidth),
                                               float(viewportHeight)));
    m_nebulaProgram->setUniformValue("uZoom", float(m_cameraZoom));
    m_nebulaProgram->setUniformValue("uFocalLength", NebulaEnvironment::FocalLength);
    m_nebulaProgram->setUniformValue("uCamera", cameraRotation());
    std::array<QVector3D, NebulaEnvironment::CloudCount> centers, colors;
    for (int i = 0; i < NebulaEnvironment::CloudCount; ++i) {
        centers[i] = NebulaEnvironment::clouds()[i].center;
        colors[i] = NebulaEnvironment::clouds()[i].color;
    }
    m_nebulaProgram->setUniformValueArray("uCenters", centers.data(), int(centers.size()));
    m_nebulaProgram->setUniformValueArray("uColors", colors.data(), int(colors.size()));
    glBindTexture(GL_TEXTURE_2D, m_nebulaTexture);
    m_nebulaProgram->setUniformValue("uCloudTexture", 0);
    constexpr int verticesPerCloud = NebulaEnvironment::PuffsPerCloud * 6;
    for (int cluster : visibleNebulaClouds())
        glDrawArrays(GL_TRIANGLES, cluster * verticesPerCloud, verticesPerCloud);
    glBindTexture(GL_TEXTURE_2D, 0);
    m_nebulaProgram->release();

    m_starProgram->bind();
    m_starProgram->setUniformValue("uTime", seconds);
    m_starProgram->setUniformValue("uAspect", aspect);
    m_starProgram->setUniformValue("uDpr", float(dpr));
    m_starProgram->setUniformValue("uViewport",
                                   QVector2D(float(viewportWidth),
                                             float(viewportHeight)));
    m_starProgram->setUniformValue("uYaw", float(m_cameraYaw));
    m_starProgram->setUniformValue("uPitch", float(m_cameraPitch));
    m_starProgram->setUniformValue("uZoom", float(m_cameraZoom));
    glDrawArrays(GL_POINTS, 0, 760);
    m_starProgram->release();

    m_diskProgram->bind();
    m_diskProgram->setUniformValue("uTime", seconds);
    m_diskProgram->setUniformValue("uAspect", aspect);
    m_diskProgram->setUniformValue("uDpr", float(dpr));
    m_diskProgram->setUniformValue("uViewport",
                                   QVector2D(float(viewportWidth),
                                             float(viewportHeight)));
    m_diskProgram->setUniformValue("uCpu", cpu);
    m_diskProgram->setUniformValue("uMemory", memory);
    m_diskProgram->setUniformValue("uYaw", float(m_cameraYaw));
    m_diskProgram->setUniformValue("uPitch", float(m_cameraPitch));
    m_diskProgram->setUniformValue("uZoom", float(m_cameraZoom));
    glDrawArrays(GL_POINTS, 0, 920);
    m_diskProgram->release();

    const auto shipTransform = spacecraftTransform();
    renderFlightPath(-1);
    renderSpacecraft(-1, shipTransform);
    renderBlackHole();
    renderFlightPath(1);
    renderSpacecraft(1, shipTransform);

    glDisable(GL_BLEND);
    glBindVertexArray(0);
}

QSize StarMapWidget::sizeHint() const
{
    return QSize(364, 458);
}

void StarMapWidget::setSnapshot(const SystemSnapshot &snapshot)
{
    m_snapshot = snapshot;
    m_blackHole->setActivity(snapshot.cpuUsagePercent);
    m_holeTextureDirty = true;
    const QString interfaceName = snapshot.networkInterface.isEmpty()
                                      ? QStringLiteral("未发现活动网卡")
                                      : snapshot.networkInterface;
    QString gpuDetails = snapshot.gpu.available ? snapshot.gpu.name
                                                 : QStringLiteral("未检测到 GPU 数据");
    if (snapshot.gpu.memoryTotal > 0) {
        gpuDetails += QStringLiteral("，显存 %1 / %2")
                          .arg(formatBytes(snapshot.gpu.memoryUsed),
                               formatBytes(snapshot.gpu.memoryTotal));
    }
    if (snapshot.gpu.temperatureCelsius >= 0.0) {
        gpuDetails += QStringLiteral("，温度 %1 °C")
                          .arg(QString::number(snapshot.gpu.temperatureCelsius, 'f', 0));
    }
    const QString details = QStringLiteral(
        "CPU %1，%2 个逻辑核心\n内存 %3 / %4，%5\n磁盘 / %6 / %7，%8\n"
        "网络 %9：下载 %10，上传 %11\nGPU %12\n已运行 %13\n"
        "鼠标：拖动旋转，滚轮缩放，双击复位\n"
        "航天飞机模型来源：NASA/Johnson Space Center（非 NASA 官方应用）")
                                .arg(formatPercent(snapshot.cpuUsagePercent))
                                .arg(snapshot.cpuCoreCount)
                                .arg(formatBytes(snapshot.memoryUsed))
                                .arg(formatBytes(snapshot.memoryTotal))
                                .arg(formatPercent(snapshot.memoryUsagePercent))
                                .arg(formatBytes(snapshot.storageUsed))
                                .arg(formatBytes(snapshot.storageTotal))
                                .arg(formatPercent(snapshot.storageUsagePercent))
                                .arg(interfaceName)
                                .arg(formatRate(snapshot.networkReceiveBytesPerSecond))
                                .arg(formatRate(snapshot.networkTransmitBytesPerSecond))
                                .arg(gpuDetails)
                                .arg(formatUptime(snapshot.uptimeSeconds));
    m_tooltip->setSnapshot(snapshot, details);
    setAccessibleName(QStringLiteral("StellarVoyager 三维飞行器与事件视界星图"));
    setAccessibleDescription(details);
    update();
    m_overlay->update();
}

void StarMapWidget::resizeEvent(QResizeEvent *event)
{
    QOpenGLWidget::resizeEvent(event);
    m_overlay->setGeometry(rect());
    m_overlay->raise();
}

bool StarMapWidget::event(QEvent *event)
{
    if (event->type() == QEvent::ToolTip && m_tooltip) {
        if (!m_draggingScene) {
            m_tooltipAnchor = static_cast<QHelpEvent *>(event)->globalPos();
            m_tooltip->showAt(m_tooltipAnchor);
        }
        event->accept();
        return true; // Never fall through to Qt's native rectangular tooltip.
    }
    if ((event->type() == QEvent::Leave || event->type() == QEvent::WindowDeactivate)
        && m_tooltip) m_tooltip->hide();
    return QOpenGLWidget::event(event);
}

void StarMapWidget::resizeGL(int width, int height)
{
    Q_UNUSED(width);
    Q_UNUSED(height);
}

void StarMapWidget::showEvent(QShowEvent *event)
{
    QOpenGLWidget::showEvent(event);
    m_flightClock.start();
    m_timer->start();
}

void StarMapWidget::hideEvent(QHideEvent *event)
{
    m_tooltip->hide();
    m_timer->stop();
    m_flightClock.invalidate();
    QOpenGLWidget::hideEvent(event);
}

void StarMapWidget::mousePressEvent(QMouseEvent *event)
{
    m_tooltip->hide();
    if (event->button() == Qt::LeftButton) {
        m_draggingScene = true;
        m_lastMousePosition = event->position();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QOpenGLWidget::mousePressEvent(event);
}

void StarMapWidget::mouseMoveEvent(QMouseEvent *event)
{
    if ((event->globalPosition().toPoint() - m_tooltipAnchor).manhattanLength() > 10)
        m_tooltip->hide();
    if (m_draggingScene && (event->buttons() & Qt::LeftButton)) {
        const QPointF delta = event->position() - m_lastMousePosition;
        m_lastMousePosition = event->position();
        m_cameraYaw = std::remainder(m_cameraYaw + delta.x() * 0.0085, 6.28318530717958647692);
        m_cameraPitch = qBound(-1.20, m_cameraPitch + delta.y() * 0.0070, 1.20);
        m_blackHole->setCameraOrientation(m_cameraYaw, m_cameraPitch);
        m_holeTextureDirty = true;
        m_overlay->update();
        update();
        event->accept();
        return;
    }
    QOpenGLWidget::mouseMoveEvent(event);
}

void StarMapWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_draggingScene) {
        m_draggingScene = false;
        setCursor(Qt::OpenHandCursor);
        event->accept();
        return;
    }
    QOpenGLWidget::mouseReleaseEvent(event);
}

void StarMapWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    m_tooltip->hide();
    if (event->button() == Qt::LeftButton) {
        m_cameraYaw = 0.0;
        m_cameraPitch = 0.0;
        m_cameraZoom = 1.0;
        m_blackHole->setCameraOrientation(m_cameraYaw, m_cameraPitch);
        m_holeTextureDirty = true;
        m_overlay->update();
        update();
        event->accept();
        return;
    }
    QOpenGLWidget::mouseDoubleClickEvent(event);
}

void StarMapWidget::wheelEvent(QWheelEvent *event)
{
    m_tooltip->hide();
    qreal steps = event->angleDelta().y() / 120.0;
    if (qFuzzyIsNull(steps))
        steps = event->pixelDelta().y() / 60.0;
    if (!qFuzzyIsNull(steps)) {
        m_cameraZoom = qBound(0.72, m_cameraZoom * qPow(1.105, steps), 1.55);
        m_overlay->update();
        update();
        event->accept();
        return;
    }
    QOpenGLWidget::wheelEvent(event);
}

void StarMapWidget::paintGL()
{
    if (m_glReady) {
        renderOpenGLScene();
    } else {
        glClearColor(0.0F, 0.0F, 0.0F, 0.0F);
        glClear(GL_COLOR_BUFFER_BIT);
    }
}

void StarMapWidget::paintOverlay(QPainter &painter)
{
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.scale(width() / kWidth, height() / kHeight);

    const QPainterPath panel = panelPath();
    if (!m_glReady) {
        painter.drawPixmap(QPointF(0.0, 0.0), m_cosmicLayer);
        const QSizeF logicalSize(m_blackHole->width() * kWidth / width(),
                                 m_blackHole->height() * kHeight / height());
        painter.drawImage(QRectF(QPointF(kCenterX - logicalSize.width() / 2,
                                         kCenterY - logicalSize.height() / 2), logicalSize),
                          m_blackHole->renderFrame());
    }
    painter.setClipPath(panel);
    if (!m_glReady) {
        drawTwinklingStars(painter, m_phase);
        drawComet(painter, m_phase);
    }

    const qreal cpuActivity = qBound(0.0, m_snapshot.cpuUsagePercent / 100.0, 1.0);
    if (!m_glReady)
        drawNBodyDisk(painter, m_phase, cpuActivity);

    drawText(painter, QRectF(17, 35, 225, 15), QStringLiteral("EVENT HORIZON  /  STELLARVOYAGER"),
             QColor(QStringLiteral("#9EB6C9")), 8, QFont::Medium);
    drawText(painter, QRectF(17, 49, 210, 13), QStringLiteral("巡航 01 · 固定航线 · 48 s / 圈"),
             QColor(QStringLiteral("#829BAA")), 7);
    drawText(painter, QRectF(126, 151, 174, 12), QStringLiteral("LENSED LIGHT · ACCRETION FIELD"),
             QColor(QStringLiteral("#B49B83")), 6, QFont::Normal, Qt::AlignCenter);

    const QPointF pixelAnchor = spacecraftScreenPosition();
    const QPointF anchor(pixelAnchor.x() * kWidth / width(),
                         pixelAnchor.y() * kHeight / height());
    if (anchor.x() < kWidth * 0.40) m_telemetryOnRight = true;
    if (anchor.x() > kWidth * 0.60) m_telemetryOnRight = false;
    const bool labelsOnRight = m_telemetryOnRight;
    const qreal x = labelsOnRight ? 244.0 : 17.0;
    const QColor cyan(QStringLiteral("#92DEE7"));
    if (spacecraftReady()) {
        // The tether follows the projected 3D hull, rather than a fixed picture.
        painter.setPen(QPen(QColor(146, 222, 231, 100), 0.6));
        QPainterPath tether;
        tether.moveTo(anchor + QPointF(labelsOnRight ? 10 : -10, -6));
        tether.lineTo(QPointF(labelsOnRight ? x - 14 : x + 117, 174));
        tether.lineTo(QPointF(labelsOnRight ? x : x + 103, 174));
        painter.drawPath(tether);
        painter.setPen(Qt::NoPen);
        painter.setBrush(cyan);
        painter.drawEllipse(anchor + QPointF(labelsOnRight ? 10 : -10, -6), 1.3, 1.3);
        drawText(painter, QRectF(anchor.x() - 50, anchor.y() + 20, 100, 14),
                 QStringLiteral("SS–01  /  ORBITER"), cyan, 7,
                 QFont::Normal, Qt::AlignCenter);
    }
    drawText(painter, QRectF(x, 158, 103, 16),
             spacecraftReady() ? QStringLiteral("本机遥测 / LIVE") : QStringLiteral("三维模型不可用"),
             cyan, 9, QFont::DemiBold);
    auto telemetry = [&](int row, const QString &name, const QString &value,
                         qreal percent, const QColor &accent) {
        const qreal y = 182.0 + row * 22.0;
        drawText(painter, QRectF(x, y, 63, 17), name, kMuted, 8);
        drawText(painter, QRectF(x + 60, y - 1, 43, 19), value, kWhite, 13,
                 QFont::DemiBold, Qt::AlignRight | Qt::AlignVCenter);
        painter.setPen(QPen(QColor(accent.red(), accent.green(), accent.blue(), 35), 0.7));
        painter.drawLine(QPointF(x, y + 19), QPointF(x + 103, y + 19));
        if (percent >= 0) {
            painter.setPen(QPen(accent, 1.1));
            painter.drawLine(QPointF(x, y + 19),
                             QPointF(x + 103 * qBound(0.0, percent / 100.0, 1.0), y + 19));
        }
    };
    const qreal gpu = m_snapshot.gpu.available ? m_snapshot.gpu.usagePercent : -1.0;
    telemetry(0, QStringLiteral("CPU · 动力"), formatPercent(m_snapshot.cpuUsagePercent),
              m_snapshot.cpuUsagePercent, kCpu);
    telemetry(1, QStringLiteral("RAM · 内存"), formatPercent(m_snapshot.memoryUsagePercent),
              m_snapshot.memoryUsagePercent, kMemory);
    telemetry(2, QStringLiteral("GPU · 导航"), gpu >= 0 ? formatPercent(gpu) : QStringLiteral("N/A"),
              gpu, cyan);
    telemetry(3, QStringLiteral("DISK · /"), formatPercent(m_snapshot.storageUsagePercent),
              m_snapshot.storageUsagePercent, kStorage);

    drawText(painter, QRectF(17, 278, 164, 17),
             QStringLiteral("↓ %1").arg(formatRate(m_snapshot.networkReceiveBytesPerSecond)),
             cyan, 11, QFont::Medium);
    drawText(painter, QRectF(183, 278, 164, 17),
             QStringLiteral("↑ %1").arg(formatRate(m_snapshot.networkTransmitBytesPerSecond)),
             kCpu, 11, QFont::Medium, Qt::AlignRight | Qt::AlignVCenter);
    drawElided(painter, QRectF(17, 297, 180, 12),
               QStringLiteral("%1 线程 · RAM %2 / %3").arg(m_snapshot.cpuCoreCount)
                   .arg(formatBytes(m_snapshot.memoryUsed), formatBytes(m_snapshot.memoryTotal)),
               kMuted, 8);
    drawElided(painter, QRectF(198, 297, 149, 12),
               QStringLiteral("在线 %1").arg(formatUptime(m_snapshot.uptimeSeconds)),
               kMuted, 8, Qt::AlignRight | Qt::AlignVCenter);
    drawText(painter, QRectF(17, 311, 330, 10),
             QStringLiteral("拖动旋转场景  ·  滚轮缩放  ·  双击复位  ·  悬停查看完整数据"),
             QColor(QStringLiteral("#7D98AA")), 7, QFont::Normal, Qt::AlignCenter);
}
