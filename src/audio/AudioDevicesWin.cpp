// Windows audio source enumeration via WASAPI/IMMDevice.
// Render endpoints are exposed as "Monitor of ..." (loopback capture) and act as
// the system-audio source; capture endpoints are microphones/line-ins.
#include "AudioDevices.h"

#include <objbase.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <propvarutil.h>

#include <algorithm>

namespace {

void enumerateEndpoints(EDataFlow flow, bool isMonitor, QVector<AudioSourceItem>& out, QString& error) {
    IMMDeviceEnumerator* enumerator = nullptr;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                  IID_PPV_ARGS(&enumerator));
    if (FAILED(hr) || !enumerator) { error = QStringLiteral("Audio endpoint enumerator unavailable"); return; }

    IMMDeviceCollection* collection = nullptr;
    hr = enumerator->EnumAudioEndpoints(flow, DEVICESTATE_ACTIVE, &collection);
    if (FAILED(hr) || !collection) { enumerator->Release(); return; }   // fresh machine may have zero endpoints

    UINT count = 0;
    collection->GetCount(&count);
    for (UINT i = 0; i < count; ++i) {
        IMMDevice* device = nullptr;
        if (FAILED(collection->Item(i, &device)) || !device) continue;

        LPWSTR id = nullptr;
        if (SUCCEEDED(device->GetId(&id)) && id) {
            QString name = QString::fromWCharArray(id);
            QString description = name;

            IPropertyStore* props = nullptr;
            if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, &props)) && props) {
                PROPVARIANT var{};
                if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &var))) {
                    if (var.vt == VT_LPWSTR && var.pwszVal) description = QString::fromWCharArray(var.pwszVal);
                }
                PropVariantClear(&var);
                props->Release();
            }
            out << AudioSourceItem{ name, description, isMonitor };
            CoTaskMemFree(id);
        }
        device->Release();
    }

    collection->Release();
    enumerator->Release();
}

} // namespace

QVector<AudioSourceItem> AudioDevices::listSources() {
    QVector<AudioSourceItem> items;
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    QString error;
    enumerateEndpoints(eRender,  /*isMonitor=*/true,  items, error);   // speakers -> loopback
    enumerateEndpoints(eCapture, /*isMonitor=*/false, items, error);   // microphones / line-in

    std::sort(items.begin(), items.end(), [](const AudioSourceItem& a, const AudioSourceItem& b) {
        if (a.isMonitor != b.isMonitor) return !a.isMonitor;   // inputs first, monitors last
        return a.description.localeAwareCompare(b.description) < 0;
    });

    CoUninitialize();
    return items;
}

bool AudioDevices::hasMonitorSources() {
    const auto items = listSources();
    for (const auto& i : items) if (i.isMonitor) return true;
    return false;
}

int AudioDevices::defaultIndex(const QVector<AudioSourceItem>& items) {
    // Prefer a monitor (system audio) when available, else the first mic.
    for (int i = 0; i < items.size(); ++i)
        if (items[i].isMonitor) return i;
    return items.isEmpty() ? -1 : 0;
}
