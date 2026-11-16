// Enumerates PulseAudio / PipeWire(-pulse) sources using `pactl -f json`.
// Kept tiny on purpose: pactl ships with pulseaudio-utils and pipewire-pulse,
// and avoids the complexity of the async libpulse context API in the UI thread.
#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

struct AudioSourceItem {
    QString name;         // pulse source name passed to pa_simple as the device
    QString description;  // human readable, e.g. "Monitor of Alder Lake ..."
    bool    isMonitor = false;   // true => capturing output/system audio
};

class AudioDevices {
public:
    static QVector<AudioSourceItem> listSources();
    // True when at least one monitor source exists (i.e. system audio is recordable).
    static bool hasMonitorSources();
    // Best default: a monitor source if present (what most recorders do),
    // otherwise the first real input.
    static int defaultIndex(const QVector<AudioSourceItem>& items);
};
