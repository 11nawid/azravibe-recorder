// Windows target enumeration: screens + monitors (EnumDisplayMonitors) and
// regular application windows (EnumWindows, filtered like EWMH/_NET_CLIENT_LIST).
#pragma once

#include <QString>
#include <QVector>

#include <windows.h>
#include <QRect>

#include "Capture.h"

class WinWindowEnumerator {
public:
    static QVector<TargetItem> listTargets(uint64_t ownWindow = 0);

private:
    static QVector<TargetItem> listScreens();
    static QVector<TargetItem> listWindows(uint64_t ownWindow);
    static bool isUsableWindow(HWND hwnd);

    static constexpr int kMinWindowSize = 64;
};
