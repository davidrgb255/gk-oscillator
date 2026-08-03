#include "scope_widget.h"

#include <QPainter>
#include <QPainterPath>
#include <algorithm>
#include <cmath>

ScopeWidget::ScopeWidget(QWidget *parent)
    : QWidget(parent)
    , m_timebaseMs(20.0f)
    , m_gain(1.0f)
    , m_frozen(false)
    , m_bg(14, 16, 22)
    , m_grid(40, 46, 58)
    , m_zero(70, 80, 100)
    , m_trace(61, 214, 140)
    , m_text(140, 150, 168)
{
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setMinimumHeight(200);
}

void ScopeWidget::setSamples(const QVector<float> &samples)
{
    if (m_frozen) {
        return;
    }
    m_samples = samples;
    update();
}

void ScopeWidget::setTimebaseMs(float ms)
{
    if (ms < 1.0f) {
        ms = 1.0f;
    }
    if (ms > 500.0f) {
        ms = 500.0f;
    }
    m_timebaseMs = ms;
    update();
}

void ScopeWidget::setGain(float gain)
{
    if (gain < 0.1f) {
        gain = 0.1f;
    }
    if (gain > 8.0f) {
        gain = 8.0f;
    }
    m_gain = gain;
    update();
}

void ScopeWidget::setFrozen(bool frozen)
{
    m_frozen = frozen;
    update();
}

void ScopeWidget::setTraceLabel(const QString &label)
{
    m_label = label;
    update();
}

QSize ScopeWidget::minimumSizeHint() const
{
    return QSize(400, 200);
}

QSize ScopeWidget::sizeHint() const
{
    return QSize(860, 340);
}

void ScopeWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const QRect r = rect();
    p.fillRect(r, m_bg);

    const int marginL = 12;
    const int marginR = 12;
    const int marginT = 28;
    const int marginB = 20;
    const QRect plot(marginL, marginT, r.width() - marginL - marginR,
                     r.height() - marginT - marginB);

    if (plot.width() < 8 || plot.height() < 8) {
        return;
    }

    /* Grid: 10 horizontal divs, 8 vertical */
    p.setPen(QPen(m_grid, 1.0));
    for (int i = 0; i <= 10; ++i) {
        const int x = plot.left() + (plot.width() * i) / 10;
        p.drawLine(x, plot.top(), x, plot.bottom());
    }
    for (int i = 0; i <= 8; ++i) {
        const int y = plot.top() + (plot.height() * i) / 8;
        p.drawLine(plot.left(), y, plot.right(), y);
    }

    /* Zero line */
    const int midY = plot.center().y();
    p.setPen(QPen(m_zero, 1.2));
    p.drawLine(plot.left(), midY, plot.right(), midY);

    /* Waveform */
    if (m_samples.size() >= 2) {
        QPainterPath path;
        const int n = m_samples.size();
        const float halfH = plot.height() * 0.5f;
        const float g = m_gain;

        auto yAt = [&](float s) -> float {
            float v = s * g;
            if (v > 1.0f) {
                v = 1.0f;
            } else if (v < -1.0f) {
                v = -1.0f;
            }
            return midY - v * halfH * 0.92f;
        };

        path.moveTo(plot.left(), yAt(m_samples[0]));
        for (int i = 1; i < n; ++i) {
            const float x = plot.left() + (float)i * (float)plot.width() / (float)(n - 1);
            path.lineTo(x, yAt(m_samples[i]));
        }

        p.setPen(QPen(m_trace, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPath(path);

        /* Soft glow under trace */
        QColor glow = m_trace;
        glow.setAlpha(40);
        p.setPen(QPen(glow, 4.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPath(path);
    } else {
        p.setPen(m_text);
        p.drawText(plot, Qt::AlignCenter, QStringLiteral("No signal — start audio"));
    }

    /* Chrome labels */
    p.setPen(QColor(236, 240, 248));
    QFont f = font();
    f.setPointSize(10);
    f.setBold(true);
    p.setFont(f);
    p.drawText(QRect(marginL, 4, plot.width(), 22), Qt::AlignLeft | Qt::AlignVCenter,
               m_label.isEmpty() ? QStringLiteral("SCOPE") : m_label);

    f.setBold(false);
    f.setPointSize(9);
    p.setFont(f);
    p.setPen(m_text);
    const QString right = QStringLiteral("%1 ms  ·  ×%2%3")
                              .arg(m_timebaseMs, 0, 'f', 0)
                              .arg(m_gain, 0, 'f', 2)
                              .arg(m_frozen ? QStringLiteral("  ·  FREEZE") : QString());
    p.drawText(QRect(marginL, 4, plot.width(), 22), Qt::AlignRight | Qt::AlignVCenter, right);

    /* Border */
    p.setPen(QPen(QColor(48, 56, 74), 1.0));
    p.drawRect(plot.adjusted(0, 0, -1, -1));
}
