#include "ScreenCapturer.h"

#include "X11Headers.h"   // XShmGetImage / XShmCreateImage / XShmQueryExtension
#include <sys/ipc.h>
#include <sys/shm.h>
#include <cstring>

ScreenCapturer::ScreenCapturer() = default;

ScreenCapturer::~ScreenCapturer() { close(); }

bool ScreenCapturer::open(const CaptureSpec& spec) {
    close();
    m_spec = spec;
    m_disp = XOpenDisplay(nullptr);
    if (!m_disp) return false;
    m_root = DefaultRootWindow(m_disp);

    // Detect channel order once from the root window's visual masks.
    XWindowAttributes rootAttr{};
    if (XGetWindowAttributes(m_disp, m_root, &rootAttr) == 0) { close(); return false; }

    Visual* vis = rootAttr.visual;
    if (vis && (vis->red_mask == 0xff0000))
        m_format = RawPixelFormat::BGR0;   // little-endian bytes: B G R X
    else
        m_format = RawPixelFormat::RGB0;

    if (m_spec.window == 0 && m_spec.region.isEmpty())
        m_spec.region = QRect(0, 0, DisplayWidth(m_disp, DefaultScreen(m_disp)),
                                   DisplayHeight(m_disp, DefaultScreen(m_disp)));
    m_buffer.reserve((size_t)DisplayWidth(m_disp, DefaultScreen(m_disp))
                   * (size_t)DisplayHeight(m_disp, DefaultScreen(m_disp)) * 4);

    int fixesEventBase = 0, fixesErrorBase = 0;
    m_fixesOk = XFixesQueryExtension(m_disp, &fixesEventBase, &fixesErrorBase);

    m_sizeGen = 1;
    return true;
}

void ScreenCapturer::close() {
    destroyShm();
    if (m_disp) { XCloseDisplay(m_disp); m_disp = nullptr; }
    m_buffer.clear();
    m_buffer.shrink_to_fit();
    m_w = m_h = m_stride = 0;
}

void ScreenCapturer::destroyShm() {
    if (!m_disp) return;
    if (m_shmImage) {
        if (m_shmAttached) XShmDetach(m_disp, &m_shm);
        XDestroyImage(m_shmImage);              // does not free shmaddr
        m_shmImage = nullptr;
    }
    if (m_shmAttached) {
        shmdt(m_shm.shmaddr);
        shmctl(m_shm.shmid, IPC_RMID, nullptr);
        m_shmAttached = false;
    }
    m_shmW = m_shmH = 0;
}

bool ScreenCapturer::resolveWindowRect(QRect& out, bool& ok) const {
    XWindowAttributes attr{};
    if (XGetWindowAttributes(m_disp, m_spec.window, &attr) == 0 || attr.map_state != IsViewable) {
        ok = false;
        return false;
    }
    Window child = 0;
    int absX = 0, absY = 0;
    if (XTranslateCoordinates(m_disp, m_spec.window, m_root, 0, 0, &absX, &absY, &child) == 0) {
        ok = false;
        return false;
    }
    out = QRect(absX, absY, attr.width, attr.height);
    ok = true;
    return true;
}

bool ScreenCapturer::grab() {
    if (!m_disp) return false;

    QRect region = m_spec.region;
    if (m_spec.window != 0) {
        bool ok = false;
        QRect r;
        if (!resolveWindowRect(r, ok) || !ok) return false;   // window gone/minimized
        const QRect rootRect(0, 0, DisplayWidth(m_disp, DefaultScreen(m_disp)),
                                   DisplayHeight(m_disp, DefaultScreen(m_disp)));
        region = r.intersected(rootRect);
        if (region.width() < 2 || region.height() < 2) return false;
    }
    // Even dimensions for yuv420p.
    region.setWidth(region.width() & ~1);
    region.setHeight(region.height() & ~1);
    if (region.width() < 2 || region.height() < 2) return false;

    const bool captured = grabRegion(region);
    if (captured && m_showCursor && m_fixesOk)
        blendCursor(region);
    return captured;
}

void ScreenCapturer::blendCursor(const QRect& capturedRegion) {
    XFixesCursorImage* img = XFixesGetCursorImage(m_disp);
    if (!img) return;

    const int iw = (int)img->width, ih = (int)img->height;
    if (iw <= 0 || ih <= 0) { XFree(img); return; }

    // Cursor image coords (root-relative) → buffer-relative.
    const int x0 = img->x - capturedRegion.x() - (int)img->xhot;
    const int y0 = img->y - capturedRegion.y() - (int)img->yhot;
    const bool rgbFirst = (m_format == RawPixelFormat::RGB0);

    for (int y = 0; y < ih; ++y) {
        const int dy = y0 + y;
        if (dy < 0 || dy >= m_h) continue;
        const unsigned long* srow = img->pixels + (size_t)y * (size_t)iw;
        uint8_t* drow = m_buffer.data() + (size_t)dy * (size_t)m_stride;
        for (int x = 0; x < iw; ++x) {
            const uint32_t s = (uint32_t)srow[x];
            const uint32_t a = (s >> 24) & 0xff;
            if (a == 0) continue;
            const int dx = x0 + x;
            if (dx < 0 || dx >= m_w) continue;
            const uint32_t sr = (s >> 16) & 0xff, sg = (s >> 8) & 0xff, sb = s & 0xff;
            uint8_t* d = drow + (size_t)dx * 4;
            const uint32_t inv = 255 - a;
            const int bOff = rgbFirst ? 2 : 0, rOff = rgbFirst ? 0 : 2;
            d[bOff] = (uint8_t)((sb * a + d[bOff] * inv + 127) / 255);
            d[1]    = (uint8_t)((sg * a + d[1]    * inv + 127) / 255);
            d[rOff] = (uint8_t)((sr * a + d[rOff] * inv + 127) / 255);
        }
    }
    XFree(img);
}

bool ScreenCapturer::grabRegion(const QRect& r) {
    if (m_shmSupported) {
        if (grabShm(r)) return true;
        m_shmSupported = false;   // permanent fallback for this session
    }
    return grabFallback(r);
}

bool ScreenCapturer::ensureShm(int w, int h) {
    if (m_shmImage && m_shmW == w && m_shmH == h) return true;
    destroyShm();

    if (!XShmQueryExtension(m_disp)) return false;

    const int screen = DefaultScreen(m_disp);
    XImage* img = XShmCreateImage(m_disp, DefaultVisual(m_disp, screen),
                                  DefaultDepth(m_disp, screen),
                                  ZPixmap, nullptr, &m_shm,
                                  (unsigned)w, (unsigned)h);
    if (!img) return false;

    m_shm.shmid = shmget(IPC_PRIVATE, (size_t)img->bytes_per_line * (size_t)img->height, IPC_CREAT | 0600);
    if (m_shm.shmid < 0) { XDestroyImage(img); return false; }

    m_shm.shmaddr = reinterpret_cast<char*>(shmat(m_shm.shmid, nullptr, 0));
    if (m_shm.shmaddr == reinterpret_cast<char*>(-1)) {
        shmctl(m_shm.shmid, IPC_RMID, nullptr);
        XDestroyImage(img);
        return false;
    }
    m_shm.readOnly = False;
    img->data = m_shm.shmaddr;

    if (!XShmAttach(m_disp, &m_shm)) {
        shmdt(m_shm.shmaddr);
        shmctl(m_shm.shmid, IPC_RMID, nullptr);
        XDestroyImage(img);
        return false;
    }
    XSync(m_disp, False);

    m_shmImage    = img;
    m_shmW        = w;
    m_shmH        = h;
    m_shmAttached = true;
    return true;
}

bool ScreenCapturer::grabShm(const QRect& r) {
    if (!ensureShm(r.width(), r.height())) return false;

    if (!XShmGetImage(m_disp, m_root, m_shmImage, r.x(), r.y(), AllPlanes))
        return false;

    const int stride = m_shmImage->bytes_per_line;
    const size_t total = (size_t)stride * (size_t)m_shmImage->height;

    if (m_buffer.size() != total) m_buffer.resize(total);
    std::memcpy(m_buffer.data(), m_shmImage->data, total);

    m_stride = stride;
    if (m_w != r.width() || m_h != r.height()) { m_w = r.width(); m_h = r.height(); ++m_sizeGen; }
    return true;
}

bool ScreenCapturer::grabFallback(const QRect& r) {
    XImage* img = XGetImage(m_disp, m_root, r.x(), r.y(),
                            (unsigned)r.width(), (unsigned)r.height(), AllPlanes, ZPixmap);
    if (!img) return false;

    const int stride = img->bytes_per_line;
    const size_t total = (size_t)stride * (size_t)img->height;

    if (m_buffer.size() != total) m_buffer.resize(total);
    std::memcpy(m_buffer.data(), img->data, total);
    XDestroyImage(img);

    m_stride = stride;
    if (m_w != r.width() || m_h != r.height()) { m_w = r.width(); m_h = r.height(); ++m_sizeGen; }
    return true;
}
