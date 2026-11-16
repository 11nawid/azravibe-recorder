// Linux / X11 capture backend (MIT-SHM fast path + XGetImage fallback),
// capturing regions of the root window or tracked windows.
#pragma once

#include "Capture.h"
#include "X11Headers.h"   // Display*, Window, XShmSegmentInfo

#include <atomic>
#include <cstring>
#include <vector>

class X11Capture : public CaptureBackend {
public:
    X11Capture();
    ~X11Capture() override;

    X11Capture(const X11Capture&) = delete;
    X11Capture& operator=(const X11Capture&) = delete;

    bool open(const CaptureSpec& spec) override;
    void close() override;
    bool grab() override;
    void setShowCursor(bool show) override { m_showCursor = show; }

    const uint8_t*  data() const override { return m_buffer.data(); }
    int             bytesPerLine() const override { return m_stride; }
    int             width() const override { return m_w; }
    int             height() const override { return m_h; }
    RawPixelFormat  format() const override { return m_format; }
    unsigned        sizeGeneration() const   { return m_sizeGen; }   // kept for diagnostics

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

    bool            m_fixesOk = false;
    bool            m_showCursor = true;
    bool            m_shmSupported = true;
    XImage*         m_shmImage = nullptr;
    XShmSegmentInfo m_shm{};
    int             m_shmW = 0, m_shmH = 0;
    bool            m_shmAttached = false;

    std::vector<uint8_t> m_buffer;
    int            m_w = 0, m_h = 0, m_stride = 0;
    RawPixelFormat m_format = RawPixelFormat::BGR0;
    unsigned       m_sizeGen = 1;
};
