#pragma once

#include "systeminfo.h"
#include <QWidget>

class QLabel;
class QTimer;

class TelemetryToolTip final : public QWidget
{
public:
    explicit TelemetryToolTip(QWidget *owner);
    void setSnapshot(const SystemSnapshot &snapshot, const QString &accessibleDetails);
    void showAt(const QPoint &globalPosition);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QLabel *m_readings = nullptr;
    QTimer *m_timeout = nullptr;
};
