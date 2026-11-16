#include "X11WindowEnumerator.h"

#include "X11Headers.h"
#include <cstdio>
#include <cstring>

namespace {
struct DisplayHandle {
    Display* d = nullptr;
    DisplayHandle() { d = XOpenDisplay(nullptr); }
    ~DisplayHandle() { if (d) XCloseDisplay(d); }
    explicit operator bool() const { return d != nullptr; }
};

QString utf8FromProperty(const unsigned char* data, unsigned long n) {
    return QString::fromUtf8(reinterpret_cast<const char*>(data), (int)n);
}
} // namespace

QString X11WindowEnumerator::windowTitle(Display* disp, Window w) {
    Atom netWmName = XInternAtom(disp, "_NET_WM_NAME", False);
    Atom utf8      = XInternAtom(disp, "UTF8_STRING", False);

    Atom actualType = 0;
    int  actualFormat = 0;
    unsigned long nItems = 0, bytesAfter = 0;
    unsigned char* data = nullptr;

    if (XGetWindowProperty(disp, w, netWmName, 0, 1024, False, utf8,
                           &actualType, &actualFormat, &nItems, &bytesAfter, &data) == Success && data) {
        QString s = utf8FromProperty(data, nItems).trimmed();
        XFree(data);
        if (!s.isEmpty()) return s;
    }

    char* legacy = nullptr;
    if (XFetchName(disp, w, &legacy) && legacy) {
        QString s = QString::fromLocal8Bit(legacy).trimmed();
        XFree(legacy);
        if (!s.isEmpty()) return s;
    }
    return QStringLiteral("(untitled window)");
}

bool X11WindowEnumerator::isSkippableWindowType(Display* disp, Window w) {
    Atom typeAtom   = XInternAtom(disp, "_NET_WM_WINDOW_TYPE", False);
    Atom desktop    = XInternAtom(disp, "_NET_WM_WINDOW_TYPE_DESKTOP", False);
    Atom dock       = XInternAtom(disp, "_NET_WM_WINDOW_TYPE_DOCK", False);
    Atom splash     = XInternAtom(disp, "_NET_WM_WINDOW_TYPE_SPLASH", False);
    Atom toolbar    = XInternAtom(disp, "_NET_WM_WINDOW_TYPE_TOOLBAR", False);
    Atom menu       = XInternAtom(disp, "_NET_WM_WINDOW_TYPE_MENU", False);

    Atom actualType = 0;
    int  fmt = 0;
    unsigned long n = 0, after = 0;
    unsigned char* data = nullptr;
    bool skip = false;
    if (XGetWindowProperty(disp, w, typeAtom, 0, 16, False, XA_ATOM,
                           &actualType, &fmt, &n, &after, &data) == Success && data && fmt == 32) {
        const Atom* atoms = reinterpret_cast<const Atom*>(data);
        for (unsigned long i = 0; i < n; ++i) {
            if (atoms[i] == desktop || atoms[i] == dock || atoms[i] == splash ||
                atoms[i] == toolbar || atoms[i] == menu) { skip = true; break; }
        }
    }
    if (data) XFree(data);
    return skip;
}

static bool stateHidden(Display* disp, Window w, Atom stateAtom, Atom hiddenAtom) {
    Atom actualType = 0;
    int  fmt = 0;
    unsigned long n = 0, after = 0;
    unsigned char* data = nullptr;
    bool hidden = false;
    if (XGetWindowProperty(disp, w, stateAtom, 0, 64, False, XA_ATOM,
                           &actualType, &fmt, &n, &after, &data) == Success && data && fmt == 32) {
        const Atom* atoms = reinterpret_cast<const Atom*>(data);
        for (unsigned long i = 0; i < n; ++i) if (atoms[i] == hiddenAtom) { hidden = true; break; }
    }
    if (data) XFree(data);
    return hidden;
}

QVector<TargetItem> X11WindowEnumerator::listScreens(Display* disp, Window root, int width, int height) {
    QVector<TargetItem> out;
    TargetItem whole;
    whole.type = TargetItem::Type::Screen;
    whole.screenRect = QRect(0, 0, width, height);
    whole.label = QStringLiteral("Screen — Entire display (%1×%2)").arg(width).arg(height);
    out << whole;

    int n = 0;
    XRRMonitorInfo* monitors = XRRGetMonitors(disp, root, True, &n);
    if (monitors && n > 0) {
        for (int i = 0; i < n; ++i) {
            const XRRMonitorInfo& m = monitors[i];
            TargetItem item;
            item.type = TargetItem::Type::Screen;
            item.screenRect = QRect(m.x, m.y, (int)m.width, (int)m.height);
            item.label = QStringLiteral("Screen — Monitor %1 (%2×%3)%4")
                             .arg(i + 1).arg(m.width).arg(m.height)
                             .arg(m.primary ? QStringLiteral(" · primary") : QString());
            out << item;
        }
    }
    if (monitors) XRRFreeMonitors(monitors);
    return out;
}

QVector<TargetItem> X11WindowEnumerator::listWindows(Display* disp, Window root, uint64_t ownWindow) {
    QVector<TargetItem> out;
    Atom listAtom  = XInternAtom(disp, "_NET_CLIENT_LIST", False);
    Atom stateAtom = XInternAtom(disp, "_NET_WM_STATE", False);
    Atom hidden    = XInternAtom(disp, "_NET_WM_STATE_HIDDEN", False);

    Atom actualType = 0;
    int  fmt = 0;
    unsigned long n = 0, after = 0;
    unsigned char* data = nullptr;

    if (XGetWindowProperty(disp, root, listAtom, 0, 4096, False, XA_WINDOW,
                           &actualType, &fmt, &n, &after, &data) != Success || !data)
        return out;

    const Window* windows = reinterpret_cast<const Window*>(data);
    for (unsigned long i = 0; i < n; ++i) {
        const Window w = windows[i];
        if ((uint64_t)w == ownWindow) continue;
        if (isSkippableWindowType(disp, w)) continue;
        if (stateHidden(disp, w, stateAtom, hidden)) continue;

        XWindowAttributes attr{};
        if (XGetWindowAttributes(disp, w, &attr) == 0) continue;
        if (attr.width < kMinWindowSize || attr.height < kMinWindowSize) continue;
        if (attr.map_state != IsViewable) continue;

        TargetItem item;
        item.type = TargetItem::Type::Window;
        item.windowId = (uint64_t)w;
        item.label = QStringLiteral("Window — %1").arg(windowTitle(disp, w));
        out << item;
    }
    XFree(data);
    return out;
}

QVector<TargetItem> X11WindowEnumerator::listTargets(uint64_t ownWindow) {
    QVector<TargetItem> out;
    DisplayHandle h;
    if (!h) return out;
    Display* disp = h.d;
    const Window root = DefaultRootWindow(disp);
    const int screen = DefaultScreen(disp);

    const int width  = DisplayWidth(disp, screen);
    const int height = DisplayHeight(disp, screen);

    out << listScreens(disp, root, width, height);
    out << listWindows(disp, root, ownWindow);
    return out;
}

QVector<TargetItem> listTargetsLinux(uint64_t ownWindow) {
    return X11WindowEnumerator::listTargets(ownWindow);
}
