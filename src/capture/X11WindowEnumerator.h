// Linux / X11 target enumeration: whole screen + RandR monitors + EWMH windows.
#pragma once

#include "Capture.h"

#include "X11Headers.h"   // Display*, Window

#include <QRect>
#include <QString>
#include <QVector>

class X11WindowEnumerator {
public:
    // includeOwn: own window id to exclude from the list (never record ourselves).
    static QVector<TargetItem> listTargets(uint64_t ownWindow = 0);

private:
    static QVector<TargetItem> listScreens(Display* disp, Window root, int width, int height);
    static QVector<TargetItem> listWindows(Display* disp, Window root, uint64_t ownWindow);
    static QString windowTitle(Display* disp, Window w);
    static bool    isSkippableWindowType(Display* disp, Window w);

    static constexpr int kMinWindowSize = 64;
};
