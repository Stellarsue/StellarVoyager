#include "metriccard.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QtMath>
#include <QVBoxLayout>

MetricCard::MetricCard(const QString &title,
                       const QColor &accent,
                       const QIcon &icon,
                       QWidget *parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("metricCard"));
    setMinimumHeight(64);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    setStyleSheet(QStringLiteral(
                      "QFrame#metricCard {"
                      " background: rgba(255, 255, 255, 15);"
                      " border: 1px solid rgba(255, 232, 237, 35);"
                      " border-radius: 10px;"
                      "}"
                      "QFrame#metricCard:hover {"
                      " background: rgba(255, 255, 255, 23);"
                      " border-color: rgba(255, 232, 237, 58);"
                      "}"
                      "QLabel#cardIcon {"
                      " background: %2;"
                      " border: 1px solid %3;"
                      " border-radius: 8px;"
                      "}"
                      "QLabel#cardTitle { color: #ead2d9; font-size: 10px; font-weight: 600; }"
                      "QLabel#cardValue { color: #fffafb; font-size: 14px; font-weight: 700; }"
                      "QLabel#cardDetail { color: #cfaeb8; font-size: 9px; }"
                      "QProgressBar {"
                      " background: rgba(255, 255, 255, 20);"
                      " border: none;"
                      " border-radius: 3px;"
                      " min-height: 4px;"
                      " max-height: 4px;"
                      "}"
                      "QProgressBar::chunk { background: %1; border-radius: 3px; }")
                      .arg(accent.name(),
                           QStringLiteral("rgba(%1, %2, %3, 30)")
                               .arg(accent.red()).arg(accent.green()).arg(accent.blue()),
                           QStringLiteral("rgba(%1, %2, %3, 72)")
                               .arg(accent.red()).arg(accent.green()).arg(accent.blue())));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 9, 8);
    layout->setSpacing(4);

    auto *contentRow = new QHBoxLayout;
    contentRow->setContentsMargins(0, 0, 0, 0);
    contentRow->setSpacing(8);

    auto *iconLabel = new QLabel(this);
    iconLabel->setObjectName(QStringLiteral("cardIcon"));
    iconLabel->setFixedSize(28, 28);
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setPixmap(icon.pixmap(16, 16));
    iconLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

    auto *textLayout = new QVBoxLayout;
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(1);

    auto *topRow = new QHBoxLayout;
    topRow->setSpacing(6);

    m_titleLabel = new QLabel(title, this);
    m_titleLabel->setObjectName(QStringLiteral("cardTitle"));

    m_valueLabel = new QLabel(QStringLiteral("--"), this);
    m_valueLabel->setObjectName(QStringLiteral("cardValue"));
    m_valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    topRow->addWidget(m_titleLabel);
    topRow->addStretch(1);
    topRow->addWidget(m_valueLabel);

    m_detailLabel = new QLabel(QStringLiteral("等待数据…"), this);
    m_detailLabel->setObjectName(QStringLiteral("cardDetail"));
    m_detailLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    m_detailLabel->setWordWrap(true);
    m_detailLabel->setMinimumWidth(0);
    m_detailLabel->setTextInteractionFlags(Qt::NoTextInteraction);

    textLayout->addLayout(topRow);
    textLayout->addWidget(m_detailLabel);
    contentRow->addWidget(iconLabel, 0, Qt::AlignTop);
    contentRow->addLayout(textLayout, 1);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(false);

    layout->addLayout(contentRow);
    layout->addWidget(m_progressBar);
}

void MetricCard::setMetrics(const QString &value, const QString &detail, double ratio)
{
    m_valueLabel->setText(value);
    m_detailLabel->setText(detail);
    m_detailLabel->setToolTip(detail);

    const bool hasRatio = ratio >= 0.0;
    m_progressBar->setVisible(hasRatio);
    if (hasRatio)
        m_progressBar->setValue(qBound(0, qRound(ratio), 100));
}
