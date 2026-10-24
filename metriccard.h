#pragma once

#include <QColor>
#include <QFrame>
#include <QIcon>

class QLabel;
class QProgressBar;

class MetricCard final : public QFrame
{
    Q_OBJECT

public:
    explicit MetricCard(const QString &title,
                        const QColor &accent,
                        const QIcon &icon,
                        QWidget *parent = nullptr);

    void setMetrics(const QString &value, const QString &detail, double ratio = -1.0);

private:
    QLabel *m_titleLabel = nullptr;
    QLabel *m_valueLabel = nullptr;
    QLabel *m_detailLabel = nullptr;
    QProgressBar *m_progressBar = nullptr;
};
