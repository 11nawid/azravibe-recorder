#include "WinWindowEnumerator.h"

#include <dwmapi.h>
#include <algorithm>

#pragma comment(lib, "dwmapi.lib")

struct MonitorEnumCtx {
    QVector<TargetItem> items;
};

QVector<TargetItem> WinWindowEnumerator::listScreens() {
    QVector<TargetItem> out;

    // Entire desktop (virtual screen) — falls back to the primary monitor.
    const int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    const int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if (vw >= 2 && vh >= 2) {
        TargetItem whole;
        whole.type = TargetItem::Type::Screen;
        whole.screenRect = QRect(vx, vy, vw, vh);
        whole.label = QStringLiteral("Screen — Entire display (%1×%2)").arg(vw).arg(vh);
        out << whole;
    }

    // Per-monitor entries.
    struct MonCtx { QVector<TargetItem> items; } ctx;
    EnumDisplayMonitors(nullptr, nullptr,
        [](HMONITOR hMon, HDC, LPRECT, LPARAM lp) -> BOOL {
            auto* c = reinterpret_cast<MonCtx*>(lp);
            MONITORINFO mi{};
            mi.cbSize = sizeof(mi);
            if (!GetMonitorInfoW(hMon, &mi)) return TRUE;
            const QRect r(mi.rcMonitor.left, mi.rcMonitor.top,
                          mi.rcMonitor.right - mi.rcMonitor.left,
                          mi.rcMonitor.bottom - mi.rcMonitor.top);
            if (r.width() < 2 || r.height() < 2) return TRUE;

            TargetItem item;
            item.type = TargetItem::Type::Screen;
            item.screenRect = r;
            const bool isPrimary = (mi.dwFlags & MONITORINFOF_PRIMARY) != 0;
            item.label = QStringLiteral("Screen — Monitor %1 (%2×%3)%4")
                             .arg(c->items.size() + 1)
                             .arg(r.width()).arg(r.height())
                             .arg(isPrimary ? QStringLiteral(" · primary") : QString());
            c->items << item;
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&ctx));

    return out + ctx.items;
}

bool WinWindowEnumerator::isUsableWindow(HWND hwnd) {
    if (!IsWindowVisible(hwnd)) return false;
    // Skip owner popups (dialogs, menus, tooltips) — we only list top-level app windows.
    if (GetWindow(hwnd, GW_OWNER) != nullptr) return false;
    // Skip tool windows (floating palettes, toolbars).
    if (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) return false;

    wchar_t cls[128]{};
    GetClassNameW(hwnd, cls, 128);
    const QString className = QString::fromWCharArray(cls);
    // Skip shell windows: desktop, taskbar, trays, start menu.
    if (className.compare(QLatin1String("Progman"), Qt::CaseInsensitive) == 0) return false;
    if (className.compare(QLatin1String("WorkerW"), Qt::CaseInsensitive) == 0) return false;
    if (className.compare(QLatin1String("Shell_TrayWnd"), Qt::CaseInsensitive) == 0) return false;
    if (className.compare(QLatin1String("Shell_SecondaryTrayWnd"), Qt::CaseInsensitive) == 0) return false;

    wchar_t title[512]{};
    GetWindowTextW(hwnd, title, 512);
    const QString text = QString::fromWCharArray(title).trimmed();
    if (text.isEmpty()) return false;

    BOOL cloaked = FALSE;
    DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
    if (cloaked) return false;   // UWP shell-hidden windows

    RECT rc{};
    if (!GetWindowRect(hwnd, &rc)) return false;
    if (rc.right - rc.left < kMinWindowSize || rc.bottom - rc.top < kMinWindowSize) return false;

    return true;
}

struct WindowEnumCtx {
    QVector<TargetItem> items;
    uint64_t ownWindow = 0;
};

QVector<TargetItem> WinWindowEnumerator::listWindows(uint64_t ownWindow) {
    WindowEnumCtx ctx;
    ctx.ownWindow = ownWindow;

    EnumWindows(
        [](HWND hwnd, LPARAM lp) -> BOOL {
            auto* c = reinterpret_cast<WindowEnumCtx*>(lp);
            if ((uint64_t)hwnd == c->ownWindow) return TRUE;
            if (!WinWindowEnumerator::isUsableWindow(hwnd)) return TRUE;

            wchar_t title[512]{};
            GetWindowTextW(hwnd, title, 512);
            const QString text = QString::fromWCharArray(title).trimmed();

            TargetItem item;
            item.type = TargetItem::Type::Window;
            item.windowId = (uint64_t)hwnd;
            item.label = QStringLiteral("Window — %1").arg(text);
            c->items << item;
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&ctx));

    return ctx.items;
}

QVector<TargetItem> WinWindowEnumerator::listTargets(uint64_t ownWindow) {
    QVector<TargetItem> out;
    out << listScreens();
    out << listWindows(ownWindow);
    return out;
}

QVector<TargetItem> listTargetsWin32(uint64_t ownWindow) {
    return WinWindowEnumerator::listTargets(ownWindow);
}
