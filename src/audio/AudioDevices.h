// PulseAudio/PipeWire source enumeration via `pactl -f json` (kept tiny on
// purpose: pactl ships with every supported distro, avoiding the async libpulse
// context API in the UI thread).
#pragma once

#include <QString>
#include <QVector>

struct AudioSourceItem {
    QString name;         // id used to reopen the device
    QString description;  // human readable, e.g. "Monitor of Alder Lake ..."
    bool    isMonitor = false;   // true => capturing output/system audio
};

class AudioDevices {
public:
    static QVector<AudioSourceItem> listSources();
    static bool hasMonitorSources();
    static int  defaultIndex(const QVector<AudioSourceItem>& items);
};
