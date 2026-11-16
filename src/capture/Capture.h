// Azravibe Recorder — platform-neutral capture interface.
// Every platform backend (X11 / Win32 / Cocoa) implements this exact contract:
// capture into a reusable buffer, even-sized output, BGRA-ish pixels, cursor composited.
#pragma once

#include <QRect>
#include <QString>
#include <QVector>

#include <cstdint>
#include <memory>
#include <vector>

#include "media/RawPixel.h"

struct CaptureSpec {
    QRect    region;             // Screen targets: capture rectangle (root coords)
    uint64_t windowId = 0;       // Window targets: native window handle (0 = fullscreen)
};

struct TargetItem {
    enum class Type { Screen, Window } type = Type::Screen;
    QString  label;              // what the dropdown shows
    QRect    screenRect;         // Screen targets: capture rectangle on the desktop
    uint64_t windowId = 0;       // Window targets: native window handle
};

class CaptureBackend {
public:
    virtual ~CaptureBackend() = default;

    // Opens the capture session. False if the target can't be resolved.
    virtual bool open(const CaptureSpec& spec) = 0;
    virtual void close() = 0;

    // Grabs one frame into the internal buffer. False when the target window
    // disappeared/unmapped — the caller should stop the recording cleanly.
    virtual bool grab() = 0;

    // Composites the system cursor into captured frames (X11 XFixes / Win32 / Cocoa).
    virtual void setShowCursor(bool show) = 0;

    virtual const uint8_t* data() const = 0;
    virtual int            bytesPerLine() const = 0;
    virtual int            width() const = 0;
    virtual int            height() const = 0;
    virtual RawPixelFormat format() const = 0;
};

// Factory: returns this platform's capture backend.
std::unique_ptr<CaptureBackend> createCaptureBackend();

// Lists recordable targets (screens + regular windows), excluding ownWindow.
QVector<TargetItem> listCaptureTargets(uint64_t ownWindow = 0);
