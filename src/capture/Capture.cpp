// Platform dispatcher: picks the capture backend / target list for this OS.
// Each platform defines createCaptureBackend<OS>() and listTargets<OS>() in its
// own translation unit (X11Capture.cpp / WinCapture.cpp / MacCapture.cpp).
#include "Capture.h"

// Linux / X11
std::unique_ptr<CaptureBackend> createCaptureBackendLinux();
QVector<TargetItem>           listTargetsLinux(uint64_t ownWindow);
// Windows / Win32
std::unique_ptr<CaptureBackend> createCaptureBackendWin32();
QVector<TargetItem>           listTargetsWin32(uint64_t ownWindow);
// macOS / Cocoa + CoreGraphics
std::unique_ptr<CaptureBackend> createCaptureBackendMac();
QVector<TargetItem>           listTargetsMac(uint64_t ownWindow);

std::unique_ptr<CaptureBackend> createCaptureBackend() {
#if defined(Q_OS_LINUX)
    return createCaptureBackendLinux();
#elif defined(Q_OS_WIN)
    return createCaptureBackendWin32();
#elif defined(Q_OS_MACOS)
    return createCaptureBackendMac();
#else
    return {};
#endif
}

QVector<TargetItem> listCaptureTargets(uint64_t ownWindow) {
#if defined(Q_OS_LINUX)
    return listTargetsLinux(ownWindow);
#elif defined(Q_OS_WIN)
    return listTargetsWin32(ownWindow);
#elif defined(Q_OS_MACOS)
    return listTargetsMac(ownWindow);
#else
    return {};
#endif
}
