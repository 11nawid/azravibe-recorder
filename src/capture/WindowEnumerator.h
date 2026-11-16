// Lists recordable targets: the full screen, individual RandR monitors and
// regular application windows (EWMH _NET_CLIENT_LIST).
#pragma once

#include <QRect>
#include <QString>
#include <QVector>

#include "X11Headers.h"   // Window / Display

struct TargetItem {
    // X11 also has a `Window` type; keep the enumerators namespaced so both coexist.
    enum class Type { Screen, Window } type = Type::Screen;
    QString label;          // what the dropdown shows
    QRect   screenRect;     // for Screen targets: capture rectangle on the root window
    ::Window window = 0;    // for Window targets: the window to track
};

class WindowEnumerator {
public:
    // includeOwn: own window id to exclude from the list (never record ourselves).
    static QVector<TargetItem> listTargets(Window ownWindow = 0);

private:
    static QVector<TargetItem> listScreens(Display* disp, Window root, int width, int height);
    static QVector<TargetItem> listWindows(Display* disp, Window root, Window ownWindow);
    static QString windowTitle(Display* disp, Window w);
    static bool    isSkippableWindowType(Display* disp, Window w);   // desktop/dock/splash
    static constexpr int kMinWindowSize = 64;
};
