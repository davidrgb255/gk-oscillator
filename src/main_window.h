#ifndef GK_MAIN_WINDOW_H
#define GK_MAIN_WINDOW_H

#include "audio.h"
#include "config.h"

#include <QMainWindow>
#include <QRect>
#include <QVector>

class ScopeWidget;
class QComboBox;
class QDoubleSpinBox;
class QEvent;
class QKeyEvent;
class QSlider;
class QLabel;
class QCheckBox;
class QPushButton;
class QTimer;
class QWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(gk_audio *audio, QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void changeEvent(QEvent *event) override;

private slots:
    void onRefreshDevices();
    void onStartStop();
    void onWaveChanged(int index);
    void onFreqChanged(double v);
    void onAmpSlider(int v);
    void onGenToggled(bool on);
    void onMonitorToggled(bool on);
    void onSourceChanged(int index);
    void onTimebaseChanged(int index);
    void onGainChanged(int index);
    void onFreezeToggled(bool on);
    void onToggleFullscreen();
    void tick();

private:
    void applyDarkTheme();
    void rebuildDeviceLists();
    void applyConfigToUi();
    void collectConfig(gk_config *c) const;
    int selectedInputIndex() const;
    int selectedOutputIndex() const;
    void updateStatus();
    void updateScope();
    void updateScopeLabel();
    void enterScopeFullscreen();
    void exitScopeFullscreen();
    void setChromeVisible(bool visible);

    gk_audio *m_audio;
    gk_config m_cfg;

    ScopeWidget *m_scope;
    QWidget *m_devPanel;
    QWidget *m_ctrlPanel;
    QComboBox *m_inCombo;
    QComboBox *m_outCombo;
    QComboBox *m_waveCombo;
    QComboBox *m_sourceCombo;
    QComboBox *m_timebaseCombo;
    QComboBox *m_gainCombo;
    QDoubleSpinBox *m_freqSpin;
    QSlider *m_ampSlider;
    QLabel *m_ampLabel;
    QCheckBox *m_genCheck;
    QCheckBox *m_monitorCheck;
    QCheckBox *m_freezeCheck;
    QPushButton *m_startBtn;
    QPushButton *m_refreshBtn;
    QPushButton *m_fullscreenBtn;
    QLabel *m_status;

    QTimer *m_timer;
    QVector<gk_device_info> m_devices;
    QVector<float> m_scopeBuf;

    bool m_scopeFullscreen;
    bool m_savedMaximized;
    QRect m_savedGeometry;
};

#endif /* GK_MAIN_WINDOW_H */
