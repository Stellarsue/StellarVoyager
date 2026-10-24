#pragma once

#include <QPixmap>
#include <QImage>
#include <QSize>
#include <QWidget>

class QPaintEvent;

class BlackHoleWidget final : public QWidget
{
public:
    explicit BlackHoleWidget(QWidget *parent = nullptr, int diameter = 72);

    void advanceAnimation(qreal degrees);
    QSize sizeHint() const override;
    void setActivity(double percent);
    void setCameraOrientation(qreal yaw, qreal pitch);
    QImage renderFrame();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void drawOrbitingMatter(QPainter &painter, bool nearSide) const;
    void rebuildLayers();
    void paintLayers(QPainter &painter);

    QPixmap m_backgroundLayer;
    QPixmap m_farDiskLayer;
    QPixmap m_nearDiskLayer;
    QPixmap m_coreLayer;
    QSize m_cachedSize;
    qreal m_phase = 0.0;
    qreal m_activity = 0.0;
    qreal m_cameraYaw = 0.0;
    qreal m_cameraPitch = 0.0;
    bool m_layersDirty = true;
};
