#ifndef GK_SCOPE_WIDGET_H
#define GK_SCOPE_WIDGET_H

#include <QColor>
#include <QVector>
#include <QWidget>

class ScopeWidget : public QWidget {
    Q_OBJECT
public:
    explicit ScopeWidget(QWidget *parent = nullptr);

    void setSamples(const QVector<float> &samples);
    void setTimebaseMs(float ms);
    void setGain(float gain);
    void setFrozen(bool frozen);
    void setTraceLabel(const QString &label);

    float timebaseMs() const { return m_timebaseMs; }
    float gain() const { return m_gain; }
    bool frozen() const { return m_frozen; }

protected:
    void paintEvent(QPaintEvent *event) override;
    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

private:
    QVector<float> m_samples;
    float m_timebaseMs;
    float m_gain;
    bool m_frozen;
    QString m_label;

    QColor m_bg;
    QColor m_grid;
    QColor m_zero;
    QColor m_trace;
    QColor m_text;
};

#endif /* GK_SCOPE_WIDGET_H */
