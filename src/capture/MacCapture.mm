#include "MacCapture.h"

#import <Cocoa/Cocoa.h>

#include <algorithm>
#include <cstring>

MacCapture::~MacCapture() { close(); }

std::unique_ptr<CaptureBackend> createCaptureBackendMac() {
    return std::make_unique<MacCapture>();
}

static CGDirectDisplayID displayContaining(const QPoint& center) {
    uint32_t count = 0;
    CGDirectDisplayID displays[32] = {0};
    if (CGGetActiveDisplayList(32, displays, &count) != kCGErrorSuccess) return CGMainDisplayID();
    for (uint32_t i = 0; i < count; ++i) {
        const CGRect b = CGDisplayBounds(displays[i]);
        if (b.origin.x <= center.x() && b.origin.x + b.size.width > center.x()
            && b.origin.y <= center.y() && b.origin.y + b.size.height > center.y())
            return displays[i];
    }
    return CGMainDisplayID();
}

bool MacCapture::open(const CaptureSpec& spec) {
    close();
    m_spec = spec;

    if (m_spec.windowId != 0) {
        // Validate the window exists and is on screen.
        CFArrayRef list = CGWindowListCopyWindowInfo(kCGWindowListOptionIncludingWindow,
                                                     CGWindowID(m_spec.windowId));
        if (!list) return false;
        const CFIndex n = CFArrayGetCount(list);
        CFRelease(list);
        if (n == 0) {
            fprintf(stderr, "azravibe: target window not available (permission missing or window closed)\n");
            return false;
        }
        return true;
    }

    const QRect region = m_spec.region.isEmpty()
        ? QRect(0, 0, CGDisplayPixelsWide(CGMainDisplayID()), CGDisplayPixelsHigh(CGMainDisplayID()))
        : m_spec.region;
    m_displayID = displayContaining(QPoint(region.x() + region.width() / 2,
                                           region.y() + region.height() / 2));
    return m_displayID != kCGNullDirectDisplay;
}

void MacCapture::close() {
    if (m_context) { CGContextRelease(m_context); m_context = nullptr; }
    m_contextBuf.clear();
    m_buffer.clear();
    m_buffer.shrink_to_fit();
    m_w = m_h = m_stride = 0;
}

bool MacCapture::ensureContext(int w, int h) {
    if (m_context && m_w == w && m_h == h) return true;
    if (m_context) { CGContextRelease(m_context); m_context = nullptr; }

    m_contextBuf.assign((size_t)w * (size_t)h * 4, 0);
    CGColorSpaceRef rgb = CGColorSpaceCreateDeviceRGB();
    m_context = CGBitmapContextCreate(
        m_contextBuf.data(), (size_t)w, (size_t)h, 8, (size_t)w * 4, rgb,
        kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little);
    CGColorSpaceRelease(rgb);
    if (!m_context) { m_contextBuf.clear(); return false; }
    return true;
}

bool MacCapture::grabImageIntoBuffer(CGImageRef image, const QRect& windowBounds, bool isWindow) {
    const int w = (int)CGImageGetWidth(image) & ~1;
    const int h = (int)CGImageGetHeight(image) & ~1;
    if (w < 2 || h < 2) { CGImageRelease(image); return false; }

    if (!ensureContext(w, h)) { CGImageRelease(image); return false; }

    CGContextClearRect(m_context, CGRectMake(0, 0, (CGFloat)w, (CGFloat)h));
    CGContextDrawImage(m_context, CGRectMake(0, 0, (CGFloat)w, (CGFloat)h), image);

    m_stride = w * 4;
    if (m_buffer.size() != (size_t)m_stride * h) m_buffer.resize((size_t)m_stride * h);
    std::memcpy(m_buffer.data(), m_contextBuf.data(), (size_t)m_stride * h);
    CGImageRelease(image);

    m_w = w; m_h = h;
    ++m_sizeGen;

    if (m_showCursor)
        blendCursor(QRect(0, 0, w, h), isWindow, windowBounds);
    return true;
}

bool MacCapture::grab() {
    if (m_spec.windowId != 0) {
        const CGWindowID wid = (CGWindowID)m_spec.windowId;

        // Fetch window bounds for relative cursor placement and existence checks.
        CFArrayRef info = CGWindowListCopyWindowInfo(kCGWindowListOptionIncludingWindow, wid);
        if (!info || CFArrayGetCount(info) == 0) {
            if (info) CFRelease(info);
            return false;
        }
        NSDictionary* d = (__bridge NSDictionary*)CFArrayGetValueAtIndex(info, 0);
        CGRect b = CGRectMake(0, 0, 0, 0);
        NSDictionary* bounds = d[(__bridge NSString*)kCGWindowBounds];
        if (bounds) b = CGRectMake([bounds[@"X"] doubleValue], [bounds[@"Y"] doubleValue],
                                   [bounds[@"Width"] doubleValue], [bounds[@"Height"] doubleValue]);
        CFRelease(info);

        QRect wb((int)b.origin.x, (int)b.origin.y, (int)b.size.width, (int)b.size.height);
        wb.setWidth(wb.width() & ~1);
        wb.setHeight(wb.height() & ~1);
        if (wb.width() < 2 || wb.height() < 2) return false;

        CGImageRef image = CGWindowListCreateImage(
            b, kCGWindowListOptionIncludingWindow, wid,
            kCGWindowImageBoundsIgnoreFraming | kCGWindowImageShouldBeOpaque | kCGWindowImageNominalResolution);
        if (!image) {
            fprintf(stderr, "azravibe: window grab failed (Screen Recording permission missing?)\n");
            return false;
        }
        return grabImageIntoBuffer(image, wb, /*isWindow=*/true);
    }

    if (m_displayID == kCGNullDirectDisplay) return false;

    const QRect bounds((int)CGDisplayBounds(m_displayID).origin.x, (int)CGDisplayBounds(m_displayID).origin.y,
                       (int)CGDisplayBounds(m_displayID).size.width, (int)CGDisplayBounds(m_displayID).size.height);

    CGImageRef image = CGDisplayCreateImage(m_displayID);
    if (!image) {
        fprintf(stderr, "azravibe: display grab failed — grant Screen Recording permission in System Settings.\n");
        return false;
    }
    return grabImageIntoBuffer(image, bounds, /*isWindow=*/false);
}

void MacCapture::blendCursor(const QRect& capturedRegion, bool isWindow, const QRect& windowBounds) {
    NSCursor* cursor = [NSCursor currentSystemCursor];
    if (!cursor) return;

    NSImage* nsImage = cursor.image;
    if (!nsImage) return;

    NSRect rect = NSMakeRect(0, 0, nsImage.size.width, nsImage.size.height);
    CGImageRef cursorImage = [nsImage CGImageForProposedRect:&rect context:nil hints:nil];
    if (!cursorImage) return;

    // Global desktop coordinates (top-left origin) for the pointer.
    NSPoint nsPoint = [NSEvent mouseLocation];
    NSScreen* primary = [NSScreen screens].firstObject;
    const CGFloat primaryHeight = primary ? primary.frame.size.height : 0;
    const int globalX = (int)nsPoint.x;
    const int globalY = (int)(primaryHeight - nsPoint.y);

    const int hotX = (int)cursor.hotSpot.x;
    const int hotY = (int)cursor.hotSpot.y;

    const int x0 = (isWindow ? globalX - windowBounds.x() : globalX) - hotX;
    const int y0 = (isWindow ? globalY - windowBounds.y() : globalY) - hotY;

    const int iw = (int)CGImageGetWidth(cursorImage);
    const int ih = (int)CGImageGetHeight(cursorImage);

    if (!ensureContext(m_w, m_h)) { CGImageRelease(cursorImage); return; }

    CGContextDrawImage(m_context,
                       CGRectMake((CGFloat)x0, (CGFloat)(m_h - y0 - ih), (CGFloat)iw, (CGFloat)ih),
                       cursorImage);
    CGImageRelease(cursorImage);

    std::memcpy(m_buffer.data(), m_contextBuf.data(), (size_t)m_stride * m_h);
}
