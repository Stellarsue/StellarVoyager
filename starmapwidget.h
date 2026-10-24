#pragma once

#include "systeminfo.h"

#include <QElapsedTimer>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLWidget>
#include <QPixmap>
#include <QPointF>
#include <QMatrix4x4>

class BlackHoleWidget;
class QHideEvent;
class QMouseEvent;
class QOpenGLShaderProgram;
class QPainter;
class QResizeEvent;
class QShowEvent;
class QTimer;
class QWheelEvent;
class StarOverlayWidget;
class TelemetryToolTip;

class StarMapWidget final : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core
{
    Q_OBJECT

public:
    explicit StarMapWidget(QWidget *parent = nullptr);
    ~StarMapWidget() override;

    QSize sizeHint() const override;
    void setSnapshot(const SystemSnapshot &snapshot);
    bool spacecraftReady() const { return m_shipProgram && m_shipTexture && m_shipVertexCount > 0; }
    int spacecraftVertexCount() const { return m_shipVertexCount; }
    QPointF spacecraftScreenPosition() const;
    double flightElapsedSeconds() const { return m_flightSeconds; }
    QVector<int> visibleNebulaClouds() const;

protected:
    bool event(QEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void initializeGL() override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void paintGL() override;
    void resizeEvent(QResizeEvent *event) override;
    void resizeGL(int width, int height) override;
    void showEvent(QShowEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    friend class StarOverlayWidget;
    void cleanupGL();
    void paintOverlay(QPainter &painter);
    void renderOpenGLScene();
    void renderSpacecraft(int side, const QMatrix4x4 &transform);
    void renderBlackHole();
    void renderFlightPath(int side);
    QMatrix4x4 cameraRotation() const;
    QMatrix4x4 spacecraftTransform() const;

    BlackHoleWidget *m_blackHole = nullptr;
    StarOverlayWidget *m_overlay = nullptr;
    TelemetryToolTip *m_tooltip = nullptr;
    QPoint m_tooltipAnchor;
    QTimer *m_timer = nullptr;
    QPixmap m_cosmicLayer;
    QOpenGLShaderProgram *m_spaceProgram = nullptr;
    QOpenGLShaderProgram *m_nebulaProgram = nullptr;
    QOpenGLShaderProgram *m_starProgram = nullptr;
    QOpenGLShaderProgram *m_diskProgram = nullptr;
    QOpenGLShaderProgram *m_shipProgram = nullptr;
    QOpenGLShaderProgram *m_holeProgram = nullptr;
    QOpenGLShaderProgram *m_routeProgram = nullptr;
    GLuint m_emptyVao = 0;
    GLuint m_spaceTexture = 0;
    GLuint m_nebulaTexture = 0;
    GLuint m_shipVao = 0;
    GLuint m_shipVbo = 0;
    GLuint m_shipTexture = 0;
    GLuint m_holeTexture = 0;
    GLuint m_routeVao = 0;
    GLuint m_routeVbo = 0;
    bool m_holeTextureDirty = true;
    int m_shipVertexCount = 0;
    bool m_glReady = false;
    bool m_draggingScene = false;
    QElapsedTimer m_elapsed;
    QElapsedTimer m_flightClock;
    double m_flightSeconds = 0.0;
    double m_lastHoleAnimationSeconds = 0.0;
    bool m_telemetryOnRight = true;
    QPointF m_lastMousePosition;
    qreal m_cameraYaw = 0.0;
    qreal m_cameraPitch = 0.0;
    qreal m_cameraZoom = 1.0;
    SystemSnapshot m_snapshot;
    qreal m_phase = 0.0;
};
