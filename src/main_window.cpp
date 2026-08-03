#include "main_window.h"
#include "scope_widget.h"

#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLayout>
#include <QPushButton>
#include <QShortcut>
#include <QSlider>
#include <QStatusBar>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <cstring>

MainWindow::MainWindow(gk_audio *audio, QWidget *parent)
    : QMainWindow(parent)
    , m_audio(audio)
    , m_scopeFullscreen(false)
    , m_savedMaximized(false)
{
    gk_config_load(&m_cfg);
    setWindowTitle(QStringLiteral("GK Oscillator"));
    applyDarkTheme();

    auto *central = new QWidget(this);
    setCentralWidget(central);
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(12, 12, 12, 8);
    root->setSpacing(10);

    m_scope = new ScopeWidget(central);
    m_scope->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_scope->setFocusPolicy(Qt::StrongFocus);
    root->addWidget(m_scope, 1);

    /* Device row (panel so it can be hidden in scope fullscreen) */
    m_devPanel = new QWidget(central);
    auto *devRow = new QHBoxLayout(m_devPanel);
    devRow->setContentsMargins(0, 0, 0, 0);
    m_inCombo = new QComboBox(m_devPanel);
    m_outCombo = new QComboBox(m_devPanel);
    m_inCombo->setMinimumWidth(220);
    m_outCombo->setMinimumWidth(220);
    m_refreshBtn = new QPushButton(QStringLiteral("Refresh"), m_devPanel);
    m_startBtn = new QPushButton(QStringLiteral("Start"), m_devPanel);
    m_startBtn->setMinimumWidth(88);

    auto *inLab = new QLabel(QStringLiteral("Input"), m_devPanel);
    auto *outLab = new QLabel(QStringLiteral("Output"), m_devPanel);
    devRow->addWidget(inLab);
    devRow->addWidget(m_inCombo, 1);
    devRow->addWidget(outLab);
    devRow->addWidget(m_outCombo, 1);
    devRow->addWidget(m_refreshBtn);
    devRow->addWidget(m_startBtn);
    root->addWidget(m_devPanel);

    /* Oscillator + scope controls */
    m_ctrlPanel = new QWidget(central);
    auto *ctrl = new QHBoxLayout(m_ctrlPanel);
    ctrl->setContentsMargins(0, 0, 0, 0);
    ctrl->setSpacing(16);

    auto *oscBox = new QGroupBox(QStringLiteral("Oscillator"), m_ctrlPanel);
    auto *oscLay = new QGridLayout(oscBox);
    m_waveCombo = new QComboBox(oscBox);
    for (int w = 0; w < GK_WAVE_COUNT; ++w) {
        m_waveCombo->addItem(QString::fromUtf8(gk_wave_name((gk_wave)w)), w);
    }
    m_freqSpin = new QDoubleSpinBox(oscBox);
    m_freqSpin->setRange(20.0, 20000.0);
    m_freqSpin->setDecimals(2);
    m_freqSpin->setSuffix(QStringLiteral(" Hz"));
    m_freqSpin->setSingleStep(1.0);
    m_ampSlider = new QSlider(Qt::Horizontal, oscBox);
    m_ampSlider->setRange(0, 1000);
    m_ampLabel = new QLabel(oscBox);
    m_genCheck = new QCheckBox(QStringLiteral("Generator on"), oscBox);
    m_monitorCheck = new QCheckBox(QStringLiteral("Monitor input"), oscBox);

    oscLay->addWidget(new QLabel(QStringLiteral("Wave"), oscBox), 0, 0);
    oscLay->addWidget(m_waveCombo, 0, 1, 1, 2);
    oscLay->addWidget(new QLabel(QStringLiteral("Freq"), oscBox), 1, 0);
    oscLay->addWidget(m_freqSpin, 1, 1, 1, 2);
    oscLay->addWidget(new QLabel(QStringLiteral("Amp"), oscBox), 2, 0);
    oscLay->addWidget(m_ampSlider, 2, 1);
    oscLay->addWidget(m_ampLabel, 2, 2);
    oscLay->addWidget(m_genCheck, 3, 0, 1, 3);
    oscLay->addWidget(m_monitorCheck, 4, 0, 1, 3);

    auto *scopeBox = new QGroupBox(QStringLiteral("Display"), m_ctrlPanel);
    auto *scopeLay = new QGridLayout(scopeBox);
    m_sourceCombo = new QComboBox(scopeBox);
    m_sourceCombo->addItem(QStringLiteral("Generator"), (int)GK_SRC_GEN);
    m_sourceCombo->addItem(QStringLiteral("Input"), (int)GK_SRC_INPUT);
    m_sourceCombo->addItem(QStringLiteral("Mix"), (int)GK_SRC_MIX);
    m_timebaseCombo = new QComboBox(scopeBox);
    const double tbs[] = {5, 10, 20, 50, 100};
    for (double t : tbs) {
        m_timebaseCombo->addItem(QStringLiteral("%1 ms").arg(t), t);
    }
    m_gainCombo = new QComboBox(scopeBox);
    const double gains[] = {0.25, 0.5, 1.0, 2.0, 4.0};
    for (double g : gains) {
        m_gainCombo->addItem(QStringLiteral("×%1").arg(g), g);
    }
    m_freezeCheck = new QCheckBox(QStringLiteral("Freeze"), scopeBox);
    m_fullscreenBtn = new QPushButton(QStringLiteral("Fullscreen"), scopeBox);
    m_fullscreenBtn->setToolTip(QStringLiteral("Show wave full screen (Esc to exit)"));

    scopeLay->addWidget(new QLabel(QStringLiteral("Source"), scopeBox), 0, 0);
    scopeLay->addWidget(m_sourceCombo, 0, 1);
    scopeLay->addWidget(new QLabel(QStringLiteral("Timebase"), scopeBox), 1, 0);
    scopeLay->addWidget(m_timebaseCombo, 1, 1);
    scopeLay->addWidget(new QLabel(QStringLiteral("Gain"), scopeBox), 2, 0);
    scopeLay->addWidget(m_gainCombo, 2, 1);
    scopeLay->addWidget(m_freezeCheck, 3, 0, 1, 2);
    scopeLay->addWidget(m_fullscreenBtn, 4, 0, 1, 2);

    ctrl->addWidget(oscBox, 1);
    ctrl->addWidget(scopeBox, 1);
    root->addWidget(m_ctrlPanel);

    m_status = new QLabel(central);
    m_status->setWordWrap(true);
    statusBar()->addWidget(m_status, 1);

    /* Geometry */
    if (m_cfg.has_geometry && m_cfg.win_w > 200 && m_cfg.win_h > 160) {
        resize(m_cfg.win_w, m_cfg.win_h);
        if (m_cfg.win_x != 0 || m_cfg.win_y != 0) {
            move(m_cfg.win_x, m_cfg.win_y);
        }
    } else {
        resize(900, 560);
    }

    applyConfigToUi();
    rebuildDeviceLists();

    connect(m_refreshBtn, &QPushButton::clicked, this, &MainWindow::onRefreshDevices);
    connect(m_startBtn, &QPushButton::clicked, this, &MainWindow::onStartStop);
    connect(m_waveCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onWaveChanged);
    connect(m_freqSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::onFreqChanged);
    connect(m_ampSlider, &QSlider::valueChanged, this, &MainWindow::onAmpSlider);
    connect(m_genCheck, &QCheckBox::toggled, this, &MainWindow::onGenToggled);
    connect(m_monitorCheck, &QCheckBox::toggled, this, &MainWindow::onMonitorToggled);
    connect(m_sourceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onSourceChanged);
    connect(m_timebaseCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onTimebaseChanged);
    connect(m_gainCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onGainChanged);
    connect(m_freezeCheck, &QCheckBox::toggled, this, &MainWindow::onFreezeToggled);
    connect(m_fullscreenBtn, &QPushButton::clicked, this, &MainWindow::onToggleFullscreen);

    /* Esc exits scope fullscreen even if focus is on a child widget */
    auto *esc = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    esc->setContext(Qt::ApplicationShortcut);
    connect(esc, &QShortcut::activated, this, [this]() {
        if (m_scopeFullscreen) {
            exitScopeFullscreen();
        }
    });

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &MainWindow::tick);
    m_timer->start(33);

    /* Push initial params into audio */
    onWaveChanged(m_waveCombo->currentIndex());
    onFreqChanged(m_freqSpin->value());
    onAmpSlider(m_ampSlider->value());
    onGenToggled(m_genCheck->isChecked());
    onMonitorToggled(m_monitorCheck->isChecked());
    updateScopeLabel();
    onTimebaseChanged(m_timebaseCombo->currentIndex());
    onGainChanged(m_gainCombo->currentIndex());

    updateStatus();
}

MainWindow::~MainWindow()
{
    if (m_audio) {
        gk_audio_stop(m_audio);
    }
}

void MainWindow::applyDarkTheme()
{
    setStyleSheet(QStringLiteral(
        "QMainWindow, QWidget { background-color: #0e1016; color: #ecf0f8; }"
        "QGroupBox { border: 1px solid #30384a; border-radius: 6px; margin-top: 10px; padding-top: 8px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; color: #8b95a8; }"
        "QComboBox, QDoubleSpinBox {"
        "  background: #202636; border: 1px solid #3a4458; border-radius: 4px; padding: 4px 8px; }"
        "QComboBox QAbstractItemView { background: #1a2030; color: #ecf0f8; selection-background-color: #3d5a80; }"
        "QSlider::groove:horizontal { height: 6px; background: #282e3a; border-radius: 3px; }"
        "QSlider::handle:horizontal { width: 14px; margin: -5px 0; background: #3dd68c; border-radius: 7px; }"
        "QPushButton {"
        "  background: #202636; border: 1px solid #3a4458; border-radius: 5px; padding: 6px 12px; }"
        "QPushButton:hover { background: #30384a; }"
        "QPushButton:checked, QPushButton#startOn {"
        "  background: #1a3a2a; border-color: #3dd68c; color: #3dd68c; }"
        "QCheckBox { spacing: 8px; }"
        "QCheckBox::indicator { width: 16px; height: 16px; border-radius: 3px;"
        "  border: 1px solid #3a4458; background: #202636; }"
        "QCheckBox::indicator:checked { background: #3dd68c; border-color: #3dd68c; }"
        "QStatusBar, QStatusBar QLabel { background: #0e1016; color: #8b95a8; }"
        "QLabel { color: #c8d0e0; }"));
}

void MainWindow::applyConfigToUi()
{
    int wi = m_cfg.wave;
    if (wi < 0 || wi >= GK_WAVE_COUNT) {
        wi = 0;
    }
    m_waveCombo->setCurrentIndex(wi);
    m_freqSpin->setValue(m_cfg.freq > 0 ? m_cfg.freq : 440.0);
    m_ampSlider->setValue((int)std::lround(std::clamp(m_cfg.amp, 0.0f, 1.0f) * 1000.0f));
    m_ampLabel->setText(QStringLiteral("%1").arg(m_cfg.amp, 0, 'f', 2));
    m_genCheck->setChecked(m_cfg.gen_enabled != 0);
    m_monitorCheck->setChecked(m_cfg.monitor != 0);

    int si = m_sourceCombo->findData(m_cfg.scope_source);
    if (si >= 0) {
        m_sourceCombo->setCurrentIndex(si);
    }
    int ti = m_timebaseCombo->findData((double)m_cfg.timebase_ms);
    if (ti < 0) {
        ti = m_timebaseCombo->findData(20.0);
    }
    if (ti >= 0) {
        m_timebaseCombo->setCurrentIndex(ti);
    }
    int gi = m_gainCombo->findData((double)m_cfg.scope_gain);
    if (gi < 0) {
        gi = m_gainCombo->findData(1.0);
    }
    if (gi >= 0) {
        m_gainCombo->setCurrentIndex(gi);
    }
}

void MainWindow::collectConfig(gk_config *c) const
{
    if (c == NULL) {
        return;
    }
    *c = m_cfg;
    /* Prefer windowed geometry so fullscreen size is not restored next launch */
    if (m_scopeFullscreen && m_savedGeometry.isValid()) {
        c->win_x = m_savedGeometry.x();
        c->win_y = m_savedGeometry.y();
        c->win_w = m_savedGeometry.width();
        c->win_h = m_savedGeometry.height();
    } else {
        c->win_x = x();
        c->win_y = y();
        c->win_w = width();
        c->win_h = height();
    }
    c->has_geometry = 1;
    c->wave = m_waveCombo->currentData().toInt();
    c->freq = (float)m_freqSpin->value();
    c->amp = m_ampSlider->value() / 1000.0f;
    c->gen_enabled = m_genCheck->isChecked() ? 1 : 0;
    c->monitor = m_monitorCheck->isChecked() ? 1 : 0;
    c->scope_source = m_sourceCombo->currentData().toInt();
    c->timebase_ms = (float)m_timebaseCombo->currentData().toDouble();
    c->scope_gain = (float)m_gainCombo->currentData().toDouble();

    if (m_inCombo->currentData().toInt() >= 0) {
        snprintf(c->input_name, sizeof(c->input_name), "%s",
                 m_inCombo->currentText().toUtf8().constData());
    } else {
        c->input_name[0] = '\0';
    }
    if (m_outCombo->currentData().toInt() >= 0) {
        snprintf(c->output_name, sizeof(c->output_name), "%s",
                 m_outCombo->currentText().toUtf8().constData());
    } else {
        c->output_name[0] = '\0';
    }
}

void MainWindow::rebuildDeviceLists()
{
    gk_device_info tmp[128];
    const int n = gk_audio_list_devices(m_audio, tmp, 128);
    m_devices.clear();
    for (int i = 0; i < n && i < 128; ++i) {
        m_devices.push_back(tmp[i]);
    }

    const QString prevIn = m_inCombo->currentText();
    const QString prevOut = m_outCombo->currentText();

    m_inCombo->blockSignals(true);
    m_outCombo->blockSignals(true);
    m_inCombo->clear();
    m_outCombo->clear();

    m_inCombo->addItem(QStringLiteral("(none)"), -1);
    m_outCombo->addItem(QStringLiteral("(none)"), -1);

    int matchIn = 0;
    int matchOut = 0;
    const int defIn = gk_audio_default_input(m_audio);
    const int defOut = gk_audio_default_output(m_audio);

    for (int i = 0; i < m_devices.size(); ++i) {
        const gk_device_info &d = m_devices[i];
        if (d.is_input) {
            m_inCombo->addItem(QString::fromUtf8(d.name), d.index);
            if (m_cfg.input_name[0] && strcmp(d.name, m_cfg.input_name) == 0) {
                matchIn = m_inCombo->count() - 1;
            } else if (matchIn == 0 && d.index == defIn && m_cfg.input_name[0] == '\0') {
                matchIn = m_inCombo->count() - 1;
            } else if (!prevIn.isEmpty() && prevIn == QString::fromUtf8(d.name)) {
                matchIn = m_inCombo->count() - 1;
            }
        }
        if (d.is_output) {
            m_outCombo->addItem(QString::fromUtf8(d.name), d.index);
            if (m_cfg.output_name[0] && strcmp(d.name, m_cfg.output_name) == 0) {
                matchOut = m_outCombo->count() - 1;
            } else if (matchOut == 0 && d.index == defOut && m_cfg.output_name[0] == '\0') {
                matchOut = m_outCombo->count() - 1;
            } else if (!prevOut.isEmpty() && prevOut == QString::fromUtf8(d.name)) {
                matchOut = m_outCombo->count() - 1;
            }
        }
    }

    m_inCombo->setCurrentIndex(matchIn);
    m_outCombo->setCurrentIndex(matchOut);
    m_inCombo->blockSignals(false);
    m_outCombo->blockSignals(false);

    updateStatus();
}

int MainWindow::selectedInputIndex() const
{
    return m_inCombo->currentData().toInt();
}

int MainWindow::selectedOutputIndex() const
{
    return m_outCombo->currentData().toInt();
}

void MainWindow::onRefreshDevices()
{
    rebuildDeviceLists();
}

void MainWindow::onStartStop()
{
    if (gk_audio_is_running(m_audio)) {
        gk_audio_stop(m_audio);
        m_startBtn->setText(QStringLiteral("Start"));
        m_startBtn->setObjectName(QString());
        m_startBtn->style()->unpolish(m_startBtn);
        m_startBtn->style()->polish(m_startBtn);
    } else {
        const int inDev = selectedInputIndex();
        const int outDev = selectedOutputIndex();
        if (gk_audio_start(m_audio, inDev, outDev, 48000.0) != 0) {
            const char *err = gk_audio_last_error(m_audio);
            m_status->setText(err ? QString::fromUtf8(err) : QStringLiteral("Start failed"));
            return;
        }
        m_startBtn->setText(QStringLiteral("Stop"));
        m_startBtn->setObjectName(QStringLiteral("startOn"));
        m_startBtn->style()->unpolish(m_startBtn);
        m_startBtn->style()->polish(m_startBtn);
    }
    updateStatus();
}

void MainWindow::onWaveChanged(int index)
{
    (void)index;
    gk_audio_set_wave(m_audio, (gk_wave)m_waveCombo->currentData().toInt());
}

void MainWindow::onFreqChanged(double v)
{
    gk_audio_set_freq(m_audio, (float)v);
}

void MainWindow::onAmpSlider(int v)
{
    const float amp = v / 1000.0f;
    m_ampLabel->setText(QStringLiteral("%1").arg(amp, 0, 'f', 2));
    gk_audio_set_amp(m_audio, amp);
}

void MainWindow::onGenToggled(bool on)
{
    gk_audio_set_gen_enabled(m_audio, on ? 1 : 0);
}

void MainWindow::onMonitorToggled(bool on)
{
    gk_audio_set_monitor(m_audio, on ? 1 : 0);
}

void MainWindow::onSourceChanged(int index)
{
    (void)index;
    updateScopeLabel();
}

void MainWindow::updateScopeLabel()
{
    const int src = m_sourceCombo->currentData().toInt();
    QString label = QStringLiteral("SCOPE · ");
    if (src == GK_SRC_GEN) {
        label += QStringLiteral("Generator");
    } else if (src == GK_SRC_INPUT) {
        label += QStringLiteral("Input");
    } else {
        label += QStringLiteral("Mix");
    }
    if (m_scopeFullscreen) {
        label += QStringLiteral("  ·  Esc to exit");
    }
    m_scope->setTraceLabel(label);
}

void MainWindow::setChromeVisible(bool visible)
{
    if (m_devPanel) {
        m_devPanel->setVisible(visible);
    }
    if (m_ctrlPanel) {
        m_ctrlPanel->setVisible(visible);
    }
    if (statusBar()) {
        statusBar()->setVisible(visible);
    }
    auto *central = centralWidget();
    if (central && central->layout()) {
        if (visible) {
            central->layout()->setContentsMargins(12, 12, 12, 8);
        } else {
            central->layout()->setContentsMargins(0, 0, 0, 0);
        }
    }
}

void MainWindow::enterScopeFullscreen()
{
    if (m_scopeFullscreen) {
        return;
    }
    m_scopeFullscreen = true;
    m_savedMaximized = isMaximized();
    m_savedGeometry = normalGeometry().isValid() ? normalGeometry() : geometry();
    setChromeVisible(false);
    m_fullscreenBtn->setText(QStringLiteral("Exit full"));
    showFullScreen();
    m_scope->setFocus(Qt::OtherFocusReason);
    updateScopeLabel();
}

void MainWindow::exitScopeFullscreen()
{
    if (!m_scopeFullscreen) {
        return;
    }
    m_scopeFullscreen = false;
    showNormal();
    if (m_savedMaximized) {
        showMaximized();
    } else if (m_savedGeometry.isValid()) {
        setGeometry(m_savedGeometry);
    }
    setChromeVisible(true);
    m_fullscreenBtn->setText(QStringLiteral("Fullscreen"));
    updateScopeLabel();
}

void MainWindow::onToggleFullscreen()
{
    if (m_scopeFullscreen) {
        exitScopeFullscreen();
    } else {
        enterScopeFullscreen();
    }
}

void MainWindow::onTimebaseChanged(int index)
{
    (void)index;
    m_scope->setTimebaseMs((float)m_timebaseCombo->currentData().toDouble());
}

void MainWindow::onGainChanged(int index)
{
    (void)index;
    m_scope->setGain((float)m_gainCombo->currentData().toDouble());
}

void MainWindow::onFreezeToggled(bool on)
{
    m_scope->setFrozen(on);
}

void MainWindow::updateScope()
{
    if (!gk_audio_is_running(m_audio)) {
        return;
    }
    const double sr = gk_audio_sample_rate(m_audio);
    if (sr <= 0.0) {
        return;
    }
    const float tb = m_scope->timebaseMs();
    size_t n = (size_t)std::lround(sr * (double)tb / 1000.0);
    if (n < 32) {
        n = 32;
    }
    if (n > 16384) {
        n = 16384;
    }
    m_scopeBuf.resize((int)n);
    const gk_scope_source src = (gk_scope_source)m_sourceCombo->currentData().toInt();
    const size_t got = gk_audio_pull_scope(m_audio, m_scopeBuf.data(), n, src);
    if (got > 0) {
        m_scopeBuf.resize((int)got);
        m_scope->setSamples(m_scopeBuf);
    }
}

void MainWindow::updateStatus()
{
    QString s;
    if (gk_audio_is_running(m_audio)) {
        s = QStringLiteral("Running · %1 Hz · buffer %2 · xruns %3")
                .arg(gk_audio_sample_rate(m_audio), 0, 'f', 0)
                .arg(gk_audio_frames_per_buffer(m_audio))
                .arg(gk_audio_xruns(m_audio));
    } else {
        s = QStringLiteral("Stopped · %1 devices · select I/O and press Start")
                .arg(m_devices.size());
    }
    const char *err = gk_audio_last_error(m_audio);
    if (err) {
        s += QStringLiteral(" · ") + QString::fromUtf8(err);
    }
    m_status->setText(s);
}

void MainWindow::tick()
{
    updateScope();
    if (gk_audio_is_running(m_audio)) {
        updateStatus();
    }
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape && m_scopeFullscreen) {
        exitScopeFullscreen();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_F11) {
        onToggleFullscreen();
        event->accept();
        return;
    }
    QMainWindow::keyPressEvent(event);
}

void MainWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);
    /* Window manager may leave fullscreen (e.g. super+d); restore chrome */
    if (event->type() == QEvent::WindowStateChange && m_scopeFullscreen
        && !isFullScreen()) {
        m_scopeFullscreen = false;
        setChromeVisible(true);
        m_fullscreenBtn->setText(QStringLiteral("Fullscreen"));
        updateScopeLabel();
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_scopeFullscreen) {
        exitScopeFullscreen();
    }
    gk_config c;
    collectConfig(&c);
    gk_config_save(&c);
    if (m_audio) {
        gk_audio_stop(m_audio);
    }
    QMainWindow::closeEvent(event);
}
