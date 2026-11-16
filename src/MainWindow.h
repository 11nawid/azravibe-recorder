// Azravibe Recorder — main window. Frameless, minimal: preview, two
// dropdowns, live waveform, record/pause, minimize/close.
#pragma once

#include <QWidget>
#include <QTimer>
#include "RecorderController.h"

class RefreshComboBox;
class PreviewWidget;
class WaveformWidget;
class RecordButton;
class PauseButton;
class TitleBar;
struct AudioSourceItem;

class MainWindow : public QWidget {
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    // Debug/smoke-test helpers (used by AZRAVIBE_* env hooks in main.cpp).
    void debugStartDefaultRecording();
    void debugStopAndReport();

protected:
    void closeEvent(QCloseEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    void buildUi();
    void refreshTargets(bool keepSelection = true);
    void refreshAudioSources(bool keepSelection = true);
    void poll();
    void startRecording();
    void stopRecording();
    void applyStateToWidgets();
    void syncEnabledState();
    static QString fileNameOf(const QString& path);

    RefreshComboBox* m_targetCombo = nullptr;
    RefreshComboBox* m_audioCombo  = nullptr;
    PreviewWidget*   m_preview     = nullptr;
    WaveformWidget*  m_wave        = nullptr;
    RecordButton*    m_recordBtn   = nullptr;
    PauseButton*     m_pauseBtn    = nullptr;
    TitleBar*        m_titleBar    = nullptr;
    QTimer           m_pollTimer;

    RecorderController m_controller;
    QVector<TargetItem>      m_targets;
    QVector<AudioSourceItem> m_audioSources;

    // Selection keys preserved across refreshes
    QString m_targetKey;
    QString m_audioKey;

    bool    m_uiBusy = false;        // cached "state != Idle"
    QString m_lastShownError;
};
