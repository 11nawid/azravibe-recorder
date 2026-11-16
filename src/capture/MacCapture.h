// macOS / Cocoa capture backend declarations (see MacCapture.mm).
#pragma once

#include "Capture.h"

#import <CoreGraphics/CoreGraphics.h>

#include <vector>

class MacCapture : public CaptureBackend {
public:
    MacCapture() = default;
    ~MacCapture() override;

    bool open(const CaptureSpec& spec) override;
    void close() override;
    bool grab() override;
    void setShowCursor(bool show) override { m_showCursor = show; }

    const uint8_t*  data() const override { return m_buffer.data(); }
    int             bytesPerLine() const override { return m_stride; }
    int             width() const override { return m_w; }
    int             height() const override { return m_h; }
    RawPixelFormat  format() const override { return RawPixelFormat::BGR0; }

private:
    bool grabImageIntoBuffer(CGImageRef image, const QRect& windowBounds, bool isWindow);
    bool ensureContext(int w, int h);
    void blendCursor(const QRect& capturedRegion, bool isWindow, const QRect& windowBounds);

    CaptureSpec       m_spec;
    CGDirectDisplayID m_displayID = kCGNullDirectDisplay;

    CGContextRef      m_context = nullptr;    // persistent BGRA32 context, recreated on resize
    std::vector<uint8_t> m_contextBuf;

    std::vector<uint8_t> m_buffer;
    int            m_w = 0, m_h = 0, m_stride = 0;
    bool           m_showCursor = true;
    unsigned       m_sizeGen = 1;
};
