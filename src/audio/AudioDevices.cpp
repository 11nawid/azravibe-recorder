#include "AudioDevices.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QByteArray>
#include <algorithm>

static QVector<AudioSourceItem> parsePactlJson(const QByteArray& jsonOut) {
    QVector<AudioSourceItem> out;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonOut);
    if (!doc.isArray()) return out;

    for (const QJsonValue& v : doc.array()) {
        const QJsonObject o = v.toObject();
        AudioSourceItem item;
        item.name        = o.value(QStringLiteral("name")).toString();
        item.description  = o.value(QStringLiteral("description")).toString();
        // monitor_of_sink is present (object) for sink monitors.
        item.isMonitor    = !o.value(QStringLiteral("monitor_of_sink")).isNull()
                            || item.name.contains(QStringLiteral(".monitor"));
        if (item.name.isEmpty()) continue;
        if (item.description.isEmpty()) item.description = item.name;
        out << item;
    }
    return out;
}

static QVector<AudioSourceItem> parsePactlShort(const QByteArray& textOut) {
    // Fallback: "pactl list short sources" → index\tname\tdriver\tformat\tstate
    QVector<AudioSourceItem> out;
    const QList<QByteArray> lines = textOut.split('\n');
    for (const QByteArray& raw : lines) {
        const QList<QByteArray> cols = raw.trimmed().split('\t');
        if (cols.size() < 2) continue;
        AudioSourceItem item;
        item.name = QString::fromUtf8(cols[1].trimmed());
        item.description = item.name;
        item.isMonitor = item.name.contains(QStringLiteral(".monitor"));
        out << item;
    }
    return out;
}

QVector<AudioSourceItem> AudioDevices::listSources() {
    auto run = [](const QStringList& args) {
        QProcess proc;
        proc.start(QStringLiteral("pactl"), args);
        if (!proc.waitForFinished(2500)) return QByteArray{};
        return proc.readAllStandardOutput();
    };

    QVector<AudioSourceItem> items = parsePactlJson(run({QStringLiteral("-f"), QStringLiteral("json"),
                                                         QStringLiteral("list"), QStringLiteral("sources")}));
    if (items.isEmpty())
        items = parsePactlShort(run({QStringLiteral("list"), QStringLiteral("short"), QStringLiteral("sources")}));

    // Sort: real inputs first, monitors last (both are selectable).
    std::sort(items.begin(), items.end(), [](const AudioSourceItem& a, const AudioSourceItem& b) {
        if (a.isMonitor != b.isMonitor) return !a.isMonitor;
        return a.description.localeAwareCompare(b.description) < 0;
    });
    return items;
}

bool AudioDevices::hasMonitorSources() {
    const auto items = listSources();
    for (const auto& i : items) if (i.isMonitor) return true;
    return false;
}

int AudioDevices::defaultIndex(const QVector<AudioSourceItem>& items) {
    for (int i = 0; i < items.size(); ++i)
        if (items[i].isMonitor) return i;     // default to system audio — what screen recorders usually want
    return items.isEmpty() ? -1 : 0;
}
