// Captures raw BGRX-style frames from the root window (works for both whole
// screens and individual windows by cropping a region of the root).
//
// Fast path: MIT-SHM shared memory through XShmGetImage.
// Fallback:  plain XGetImage if SHM is unavailable or a grab fails at runtime.
#pragma once

#include <QRect>
#include <cstdint>
#include <memory>
#include <vector>

#include "X11Headers.h"

#include "../media/RawPixel.h"

struct CaptureSpec {
    QRect  region;       // initial capture rectangle in root coordinates
    Window window = 0;   // 0 => static full-screen capture; otherwise track this window
};

class ScreenCapturer {
public:
    ScreenCapturer();
    ~ScreenCapturer();

    ScreenCapturer(const ScreenCapturer&) = delete;
    ScreenCapturer& operator=(const ScreenCapturer&) = delete;

    bool open(const CaptureSpec& spec);
    void close();

    // Toggle live cursor compositing onto captured frames.
    void setShowCursor(bool show) { m_showCursor = show; }

    // Grabs the current frame into the internal buffer.
    // Returns false when the target window disappeared / is unmapped; the
    // caller should stop the recording and finalize the file.
    bool grab();

    const uint8_t* data() const { return m_buffer.data(); }
    int            bytesPerLine() const { return m_stride; }
    int            width()  const { return m_w; }
    int            height() const { return m_h; }
    RawPixelFormat format() const { return m_format; }
    // Incremented whenever the capture rectangle changed size.
    unsigned       sizeGeneration() const { return m_sizeGen; }

private:
    bool grabRegion(const QRect& r);
    bool grabShm(const QRect& r);
    bool grabFallback(const QRect& r);
    bool ensureShm(int w, int h);
    void destroyShm();
    bool resolveWindowRect(QRect& outRect, bool& ok) const;
    void blendCursor(const QRect& capturedRegion);

    Display*        m_disp = nullptr;
    Window          m_root = 0;
    CaptureSpec     m_spec;

    bool            m_fixesOk = false;     // XFixes extension present
    bool            m_showCursor = true;   // user toggle (composited when true)

    bool            m_shmSupported = true;   // flips to false after the first SHM failure
    XImage*         m_shmImage     = nullptr;
    XShmSegmentInfo m_shm{};
    int             m_shmW = 0, m_shmH = 0;
    bool            m_shmAttached = false;

    std::vector<uint8_t> m_buffer;
    int            m_w = 0, m_h = 0, m_stride = 0;
    RawPixelFormat m_format = RawPixelFormat::BGR0;
    unsigned       m_sizeGen = 1;
};
