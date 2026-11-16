#include "MainWindow.h"
#include "Style.h"
#include "RefreshComboBox.h"
#include "PreviewWidget.h"
#include "WaveformWidget.h"
#include "RecordButton.h"
#include "PauseButton.h"
#include "TitleBar.h"
#include "FolderButton.h"
#include "CheckMark.h"
#include "audio/AudioDevices.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QLabel>
#include <QFontMetrics>
#include <QShortcut>
#include <QKeySequence>
#include <QCloseEvent>
#include <QShowEvent>
#include <QFileInfo>
#include <QFileDialog>
#include <QDesktopServices>
#include <QUrl>
#include <QSettings>
#include <QEvent>
#include <QDir>
#include <QStandardPaths>
#include <QApplication>
#include <cstdio>

namespace {
QString targetKey(const TargetItem& t) {
    if (t.type == TargetItem::Type::Window) return QStringLiteral("w:%1").arg((qulonglong)t.windowId);
    return QStringLiteral("s:%1:%2:%3:%4").arg(t.screenRect.x()).arg(t.screenRect.y())
                                            .arg(t.screenRect.width()).arg(t.screenRect.height());
}
} // namespace

MainWindow::MainWindow(QWidget* parent) : QWidget(parent) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
    setWindowTitle(QStringLiteral("Azravibe Recorder"));   // taskbar / Alt-Tab name
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedSize(664, 734);
    setStyleSheet(az::styleSheet());

    buildUi();

    refreshTargets(false);
    refreshAudioSources(false);

    // Restore persisted preferences (output folder + cursor toggle).
    {
        QSettings settings(QStringLiteral("azravibe"), QStringLiteral("azravibe-recorder"));
        const QString outDir = settings.value(QStringLiteral("outputDir")).toString();
        if (!outDir.isEmpty() && QDir(outDir).exists())
            m_controller.setOutputDir(outDir);

        const bool showCursor = settings.value(QStringLiteral("showCursor"), true).toBool();
        m_cursorCheck->setChecked(showCursor);
        m_controller.setCursorEnabled(showCursor);
    }
    updateLocationLabel();

    m_pollTimer.setInterval(33);   // ~30 Hz UI handoff
    connect(&m_pollTimer, &QTimer::timeout, this, &MainWindow::poll);
    m_pollTimer.start();

    // Keyboard shortcuts (no extra chrome)
    auto* recordShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+R")), this);
    connect(recordShortcut, &QShortcut::activated, this, [this] {
        if (m_controller.state() == RecorderController::State::Idle) startRecording();
        else                                                        stopRecording();
    });
    auto* pauseShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Space")), this);
    connect(pauseShortcut, &QShortcut::activated, this, [this] {
        if (m_controller.state() != RecorderController::State::Idle)
            m_controller.togglePause();
    });
    auto* quitShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Q")), this);
    connect(quitShortcut, &QShortcut::activated, this, [this] { close(); });
}

MainWindow::~MainWindow() {
    m_controller.stopRecording();
}

void MainWindow::buildUi() {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto* root = new QWidget(this);
    root->setObjectName(QStringLiteral("root"));
    outer->addWidget(root);

    auto* col = new QVBoxLayout(root);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(0);

    // ---- title bar ----
    m_titleBar = new TitleBar(root);
    m_titleBar->onMinimize = [this] { showMinimized(); };
    m_titleBar->onClose    = [this] { close(); };
    col->addWidget(m_titleBar);

    auto* body = new QWidget(root);
    auto* bodyCol = new QVBoxLayout(body);
    bodyCol->setContentsMargins(17, 12, 17, 16);
    bodyCol->setSpacing(13);
    col->addWidget(body, 1);

    // ---- preview ----
    m_preview = new PreviewWidget(body);
    m_preview->setMinimumHeight(300);
    bodyCol->addWidget(m_preview, 1);

    // ---- dropdowns ----
    auto* row = new QHBoxLayout;
    row->setSpacing(10);
    m_targetCombo = new RefreshComboBox(body);
    m_audioCombo  = new RefreshComboBox(body);
    m_targetCombo->setToolTip(QStringLiteral("Choose what to record"));
    m_audioCombo->setToolTip(QStringLiteral("Choose the audio source — you can switch this while recording"));
    m_targetCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_audioCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_targetCombo->setMaxVisibleItems(14);
    m_audioCombo->setMaxVisibleItems(14);
    m_targetCombo->onAboutToShow = [this] { refreshTargets(true); };
    m_audioCombo->onAboutToShow  = [this] { refreshAudioSources(true); };
    connect(m_targetCombo, &QComboBox::currentTextChanged, this, [this](const QString& t) {
        m_targetCombo->setToolTip(t);
    });
    connect(m_audioCombo, &QComboBox::currentTextChanged, this, [this](const QString& t) {
        m_audioCombo->setToolTip(t);
    });
    row->addWidget(m_targetCombo, 1);
    row->addWidget(m_audioCombo, 1);
    bodyCol->addLayout(row);

    // ---- output location + cursor toggle ----
    auto* outputBar = new QWidget(body);
    outputBar->setObjectName(QStringLiteral("outputbar"));
    outputBar->setFixedHeight(38);
    auto* locRow = new QHBoxLayout(outputBar);
    locRow->setContentsMargins(6, 0, 6, 0);
    locRow->setSpacing(9);

    m_outputBtn = new FolderButton(outputBar);
    m_outputBtn->onClicked = [this] { pickOutputDir(); };
    locRow->addWidget(m_outputBtn, 0, Qt::AlignVCenter);

    m_locationLabel = new QLabel(outputBar);
    m_locationLabel->setObjectName(QStringLiteral("locationPath"));
    m_locationLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_locationLabel->setCursor(Qt::PointingHandCursor);
    m_locationLabel->installEventFilter(this);   // click → open folder
    locRow->addWidget(m_locationLabel, 1, Qt::AlignVCenter);

    m_cursorCheck = new CheckMarkBox(outputBar);
    m_cursorCheck->onToggled = [this](bool on) {
        m_controller.setCursorEnabled(on);
        QSettings settings(QStringLiteral("azravibe"), QStringLiteral("azravibe-recorder"));
        settings.setValue(QStringLiteral("showCursor"), on);
    };
    locRow->addWidget(m_cursorCheck, 0, Qt::AlignVCenter);
    bodyCol->addWidget(outputBar);

    // ---- waveform ----
    m_wave = new WaveformWidget(body);
    m_wave->setFixedHeight(74);
    bodyCol->addWidget(m_wave);

    // ---- transport controls ----
    auto* controls = new QHBoxLayout;
    controls->setSpacing(26);
    controls->addStretch(1);

    m_pauseBtn = new PauseButton(body);
    m_pauseBtn->onClicked = [this] { m_controller.togglePause(); };
    controls->addWidget(m_pauseBtn, 0, Qt::AlignVCenter);

    m_recordBtn = new RecordButton(body);
    m_recordBtn->onClicked = [this] {
        if (m_controller.state() == RecorderController::State::Idle) startRecording();
        else                                                        stopRecording();
    };
    controls->addWidget(m_recordBtn, 0, Qt::AlignVCenter);

    controls->addStretch(1);
    bodyCol->addLayout(controls);
}

QString MainWindow::fileNameOf(const QString& path) {
    return QFileInfo(path).fileName();
}

void MainWindow::refreshTargets(bool keepSelection) {
    if (m_uiBusy) return;   // locked while recording

    const QString previousKey = keepSelection ? m_targetKey : QString();
    const QVector<TargetItem> items = listCaptureTargets((uint64_t)winId());

    m_targets = items;
    m_targetCombo->blockSignals(true);
    m_targetCombo->clear();

    int selectIndex = 0;
    int i = 0;
    for (const auto& t : items) {
        m_targetCombo->addItem(t.label, QVariant(i));
        if (keepSelection && targetKey(t) == previousKey) selectIndex = i;
        ++i;
    }
    if (!items.isEmpty()) m_targetCombo->setCurrentIndex(selectIndex);
    else                  m_targetCombo->addItem(QStringLiteral("No screen found"), QVariant(-1));
    m_targetCombo->blockSignals(false);

    const int idx = m_targetCombo->currentIndex();
    if (idx >= 0 && idx < m_targets.size()) m_targetKey = targetKey(m_targets[idx]);
}

void MainWindow::refreshAudioSources(bool keepSelection) {
    // Audio source list can refresh even while recording (needed for hot-swap).

    const QString previousKey = keepSelection ? m_audioKey : QString();
    m_audioSources = AudioDevices::listSources();

    m_audioCombo->blockSignals(true);
    m_audioCombo->clear();

    int selectIndex = 0;
    if (m_audioSources.isEmpty()) {
        m_audioCombo->addItem(QStringLiteral("Default audio device"), QVariant(-1));
    } else {
        int def = AudioDevices::defaultIndex(m_audioSources);
        int i = 0;
        for (const auto& s : m_audioSources) {
            const QString label = s.isMonitor
                ? QStringLiteral("Monitor — %1").arg(s.description)
                : QStringLiteral("Input — %1").arg(s.description);
            m_audioCombo->addItem(label, QVariant(i));
            if (keepSelection && s.name == previousKey) selectIndex = i;
            else if (!keepSelection && i == def)        selectIndex = i;
            ++i;
        }
        m_audioCombo->setCurrentIndex(selectIndex);
    }
    m_audioCombo->blockSignals(false);

    const int idx = m_audioCombo->currentIndex();
    if (idx >= 0 && idx < m_audioSources.size()) m_audioKey = m_audioSources[idx].name;
}

void MainWindow::startRecording() {
    if (m_targets.isEmpty()) {
        m_preview->showToast(QStringLiteral("No capture target available."));
        return;
    }

    int tIdx = m_targetCombo->currentIndex();
    if (tIdx < 0 || tIdx >= m_targets.size()) {
        m_preview->showToast(QStringLiteral("Pick a screen or window first."));
        return;
    }
    const TargetItem target = m_targets[tIdx];

    int aIdx = m_audioCombo->currentIndex();
    const QString source = (aIdx >= 0 && aIdx < m_audioSources.size())
                               ? m_audioSources[aIdx].name
                               : QString();   // empty → server default

    if (!m_controller.startRecording(target, source)) {
        m_preview->showToast(m_controller.error());
        return;
    }

    m_wave->clear();
    m_wave->setActive(true);
    m_uiBusy = true;
    m_audioIndexAtStart = m_audioCombo->currentIndex();
    syncEnabledState();
    applyStateToWidgets();
    std::fprintf(stderr, "azravibe: recording started -> %s\n",
                 m_controller.lastSavedPath().toLocal8Bit().constData());
}

void MainWindow::stopRecording() {
    const QString saved = m_controller.lastSavedPath();
    m_controller.stopRecording();

    m_wave->setActive(false);
    m_uiBusy = false;
    syncEnabledState();
    applyStateToWidgets();

    const QString err = m_controller.error();
    if (!err.isEmpty()) {
        // Actionable first: why the take was short/empty (file is still finalized).
        m_preview->showToast(err);
    } else if (!saved.isEmpty()) {
        m_preview->showToast(QStringLiteral("Saved — %1").arg(fileNameOf(saved)));
        std::fprintf(stderr, "azravibe: recording saved -> %s\n", saved.toLocal8Bit().constData());
    } else {
        m_preview->showToast(QStringLiteral("Recording stopped."));
    }
    m_controller.consumeNotices();   // drop stale notices from the finished session
}

void MainWindow::syncEnabledState() {
    m_targetCombo->setEnabled(!m_uiBusy);
    m_audioCombo->setEnabled(true);   // always live — hot-swappable while recording
}

void MainWindow::applyStateToWidgets() {
    const auto s = m_controller.state();
    switch (s) {
    case RecorderController::State::Idle:
        m_recordBtn->setVisual(RecordButton::Visual::Idle);
        m_pauseBtn->setVisual(PauseButton::Visual::Disabled);
        m_preview->setBadge(PreviewWidget::Badge::None);
        break;
    case RecorderController::State::Recording:
        m_recordBtn->setVisual(RecordButton::Visual::Recording);
        m_pauseBtn->setVisual(PauseButton::Visual::Recording);
        m_preview->setBadge(PreviewWidget::Badge::Recording);
        break;
    case RecorderController::State::Paused:
        m_recordBtn->setVisual(RecordButton::Visual::Recording);
        m_pauseBtn->setVisual(PauseButton::Visual::Paused);
        m_preview->setBadge(PreviewWidget::Badge::Paused);
        break;
    }
}

void MainWindow::poll() {
    m_controller.setPreviewWanted(isVisible() && !isMinimized());

    const auto s = m_controller.state();

    // Reflect state transitions (e.g. auto-stop when the target window closed).
    const bool busy = (s != RecorderController::State::Idle);
    if (busy != m_uiBusy) {
        if (!busy) {
            m_uiBusy = false;
            syncEnabledState();
            applyStateToWidgets();
            m_wave->setActive(false);
            const QString saved = m_controller.lastSavedPath();
            if (!saved.isEmpty())
                m_preview->showToast(QStringLiteral("Saved — %1").arg(fileNameOf(saved)));
        } else {
            m_uiBusy = true;
            syncEnabledState();
            applyStateToWidgets();
        }
    }
    if (m_controller.autoStopRequested() && busy) {
        stopRecording();
        return;
    }

    if (!busy) {
        // Idle: keep the preview calm; nothing to do.
        return;
    }

    // Hot-swap audio source mid-recording (dropdown stays live).
    const int curIdx = m_audioCombo->currentIndex();
    if (curIdx != m_audioIndexAtStart && curIdx >= 0 && curIdx < m_audioSources.size()) {
        const QString err = m_controller.setAudioSource(m_audioSources[curIdx].name);
        if (err.isEmpty())
            m_preview->showToast(QStringLiteral("Audio source switched"));
        else
            m_preview->showToast(err);
        m_audioIndexAtStart = curIdx;
    }

    // Preview frame (skip rendering entirely while minimized)
    if (isVisible() && !isMinimized()) {
        const QImage frame = m_controller.previewImage();
        if (!frame.isNull()) m_preview->setFrame(frame);
    }
    m_preview->setElapsedUs(m_controller.mediaUs());

    // Waveform
    const std::vector<float> peaks = m_controller.consumePeaks();
    if (!peaks.empty()) m_wave->pushPeaks(peaks);

    // Notices (non-fatal info from the pipeline)
    const QVector<QString> notices = m_controller.consumeNotices();
    for (const QString& n : notices) m_preview->showToast(n);

    applyStateToWidgets();
}

void MainWindow::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
}

QString MainWindow::currentOutputDir() const {
    const QString configured = m_controller.outputDir();
    if (!configured.isEmpty()) return configured;
    QString baseDir = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    if (baseDir.isEmpty()) baseDir = QDir::homePath() + QStringLiteral("/Videos");
    return baseDir + QStringLiteral("/Azravibe Recorder");
}

void MainWindow::updateLocationLabel() {
    const QString dir = currentOutputDir();
    const QFontMetrics fm(m_locationLabel ? m_locationLabel->font() : QFont());
    if (m_locationLabel) {
        m_locationLabel->setText(fm.elidedText(dir, Qt::ElideLeft, 300));
        m_locationLabel->setToolTip(dir + QStringLiteral("\nClick to open in file manager"));
    }
}

void MainWindow::pickOutputDir() {
    const QString picked = QFileDialog::getExistingDirectory(
        this, QStringLiteral("Choose output folder"), currentOutputDir(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (picked.isEmpty()) return;

    m_controller.setOutputDir(picked);
    QSettings settings(QStringLiteral("azravibe"), QStringLiteral("azravibe-recorder"));
    settings.setValue(QStringLiteral("outputDir"), picked);
    updateLocationLabel();
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_locationLabel && event->type() == QEvent::MouseButtonPress) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(currentOutputDir()));
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

void MainWindow::closeEvent(QCloseEvent* event) {
    // Close = quit the whole tool: finalize the recording first, then exit.
    m_controller.stopRecording();
    event->accept();
    QApplication::quit();
}

void MainWindow::debugStartDefaultRecording() {
    startRecording();
}

void MainWindow::debugStopAndReport() {
    stopRecording();
    std::fprintf(stderr, "azravibe: saved=%s\n",
                 m_controller.lastSavedPath().toLocal8Bit().constData());
}
