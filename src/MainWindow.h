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
class FolderButton;
class CheckMarkBox;
class QLabel;
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
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void buildUi();
    void refreshTargets(bool keepSelection = true);
    void refreshAudioSources(bool keepSelection = true);
    void poll();
    void startRecording();
    void stopRecording();
    void applyStateToWidgets();
    void syncEnabledState();
    void pickOutputDir();
    QString currentOutputDir() const;      // effective folder (configured or default)
    void updateLocationLabel();
    static QString fileNameOf(const QString& path);

    RefreshComboBox* m_targetCombo = nullptr;
    RefreshComboBox* m_audioCombo  = nullptr;
    PreviewWidget*   m_preview     = nullptr;
    WaveformWidget*  m_wave        = nullptr;
    RecordButton*    m_recordBtn   = nullptr;
    PauseButton*     m_pauseBtn    = nullptr;
    TitleBar*        m_titleBar    = nullptr;
    FolderButton*    m_outputBtn   = nullptr;
    QLabel*          m_locationLabel = nullptr;
    CheckMarkBox*    m_cursorCheck  = nullptr;
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
