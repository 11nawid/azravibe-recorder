// macOS audio source enumeration via CoreAudio.
// Lists input-scope devices (microphones / line-in) — the only capturable
// sources in v1. System-audio loopback (Core Audio process taps) is a planned
// follow-up; output devices are not listed because they cannot be recorded yet.
#include "AudioDevices.h"

#include <CoreAudio/CoreAudio.h>
#include <AudioToolbox/AudioToolbox.h>

namespace {

QVector<AudioObjectID> allDevices() {
    AudioObjectPropertyAddress address = {
        kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain
    };
    UInt32 size = 0;
    AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &address, 0, nullptr, &size);
    const UInt32 count = size / sizeof(AudioObjectID);
    QVector<AudioObjectID> ids(count, 0);
    if (count > 0) {
        size = (UInt32)(count * sizeof(AudioObjectID));
        AudioObjectGetPropertyData(kAudioObjectSystemObject, &address, 0, nullptr, &size, ids.data());
    }
    return ids;
}

UInt32 inputChannelCount(AudioObjectID device) {
    AudioObjectPropertyAddress address = {
        kAudioDevicePropertyStreamConfiguration, kAudioDevicePropertyScopeInput, kAudioObjectPropertyElementMain
    };
    UInt32 size = 0;
    if (AudioObjectGetPropertyDataSize(device, &address, 0, nullptr, &size) != noErr || size == 0) return 0;

    QVector<uint8_t> storage(size, 0);
    AudioBufferList* list = reinterpret_cast<AudioBufferList*>(storage.data());
    if (AudioObjectGetPropertyData(device, &address, 0, nullptr, &size, list) != noErr) return 0;

    UInt32 channels = 0;
    for (UInt32 i = 0; i < list->mNumberBuffers; ++i)
        channels += list->mBuffers[i].mNumberChannels;
    return channels;
}

QString deviceName(AudioObjectID device) {
    AudioObjectPropertyAddress address = {
        kAudioObjectPropertyName, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain
    };
    CFStringRef name = nullptr;
    UInt32 size = sizeof(CFStringRef);
    if (AudioObjectGetPropertyData(device, &address, 0, nullptr, &size, &name) != noErr || !name)
        return QStringLiteral("Unknown input");

    char buffer[256] = {0};
    if (!CFStringGetCString(name, buffer, sizeof(buffer), kCFStringEncodingUTF8))
        buffer[0] = 0;
    CFRelease(name);
    return QString::fromUtf8(buffer);
}

QString deviceUid(AudioObjectID device) {
    AudioObjectPropertyAddress address = {
        kAudioDevicePropertyDeviceUID, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain
    };
    CFStringRef uid = nullptr;
    UInt32 size = sizeof(CFStringRef);
    if (AudioObjectGetPropertyData(device, &address, 0, nullptr, &size, &uid) != noErr || !uid)
        return QString();

    char buffer[256] = {0};
    CFStringGetCString(uid, buffer, sizeof(buffer), kCFStringEncodingUTF8);
    CFRelease(uid);
    return QString::fromLatin1(buffer);
}

} // namespace

QVector<AudioSourceItem> AudioDevices::listSources() {
    QVector<AudioSourceItem> items;
    for (AudioObjectID device : allDevices()) {
        if (inputChannelCount(device) == 0) continue;
        const QString name = deviceName(device);
        const QString uid  = deviceUid(device);
        if (uid.isEmpty()) continue;
        items << AudioSourceItem{ uid, name, /*isMonitor=*/false };
    }
    return items;
}

bool AudioDevices::hasMonitorSources() {
    return false;   // system-audio loopback not available on macOS yet
}

int AudioDevices::defaultIndex(const QVector<AudioSourceItem>& items) {
    return items.isEmpty() ? -1 : 0;
}
