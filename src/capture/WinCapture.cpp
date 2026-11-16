#include "WinCapture.h"

#include <windows.h>
#include <dwmapi.h>
#include <gdiplus.h>
#include <wrl/client.h>

#include <algorithm>
#include <cstring>
#include <vector>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")

namespace {
struct MonitorProbe {
    POINT  target{};
    HMONITOR hit = nullptr;
};

QRect monitorRectContaining(const QRect& region) {
    MonitorProbe probe;
    probe.target = POINT{ region.center().x(), region.center().y() };

    EnumDisplayMonitors(nullptr, nullptr,
        [](HMONITOR hMon, HDC, LPRECT rc, LPARAM lp) -> BOOL {
            auto* p = reinterpret_cast<MonitorProbe*>(lp);
            if (PtInRect(rc, p->target)) { p->hit = hMon; return FALSE; }   // stop enumeration
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&probe));

    if (probe.hit) {
        MONITORINFO mi{};
        mi.cbSize = sizeof(mi);
        if (GetMonitorInfoW(probe.hit, &mi))
            return QRect(mi.rcMonitor.left, mi.rcMonitor.top,
                         mi.rcMonitor.right - mi.rcMonitor.left,
                         mi.rcMonitor.bottom - mi.rcMonitor.top);
    }
    // Fall back to the primary monitor.
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfoW(MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY), &mi))
        return QRect(mi.rcMonitor.left, mi.rcMonitor.top,
                     mi.rcMonitor.right - mi.rcMonitor.left,
                     mi.rcMonitor.bottom - mi.rcMonitor.top);
    return QRect(0, 0, 2, 2);
}
} // namespace

WinCapture::WinCapture() = default;

WinCapture::~WinCapture() { close(); }

std::unique_ptr<CaptureBackend> createCaptureBackendWin32() {
    return std::make_unique<WinCapture>();
}

bool WinCapture::open(const CaptureSpec& spec) {
    close();
    m_spec = spec;

    if (m_spec.windowId != 0) {
        // Window path: GDI PrintWindow.
        HWND hwnd = reinterpret_cast<HWND>(m_spec.windowId);
        QRect r;
        bool ok = false;
        if (!resolveWindowRect(hwnd, r, ok) || !ok) return false;
        m_windowIsUsable = true;
        m_stride = (r.width() & ~1) * 4;
        m_buffer.assign((size_t)(r.width() & ~1) * (size_t)(r.height() & ~1) * 4, 0);
        m_w = r.width() & ~1;
        m_h = r.height() & ~1;
        return ensureGdiResources(m_w, m_h);
    }

    // Fullscreen path: DXGI duplication of the monitor that contains the region.
    const QRect region = m_spec.region.isEmpty()
        ? QRect(0, 0, GetSystemMetrics(SM_CXVIRTUALSCREEN), GetSystemMetrics(SM_CYVIRTUALSCREEN))
        : m_spec.region;
    const QRect monitor = monitorRectContaining(region);
    m_dupRegion = monitor;
    return initDxgi(monitor);
}

void WinCapture::close() {
    m_dup.Reset();
    m_context.Reset();
    m_device.Reset();
    if (m_stagingTexture) { m_stagingTexture->Release(); m_stagingTexture = nullptr; }

    if (m_memDC) { DeleteDC(m_memDC); m_memDC = nullptr; }
    if (m_bitmap) { DeleteObject(m_bitmap); m_bitmap = nullptr; }
    m_oldBmp = nullptr;
    m_gdiW = m_gdiH = 0;

    m_buffer.clear();
    m_buffer.shrink_to_fit();
    m_w = m_h = m_stride = 0;
}

bool WinCapture::grab() {
    if (m_spec.windowId != 0) {
        HWND hwnd = reinterpret_cast<HWND>(m_spec.windowId);
        return grabWindowFrame(hwnd);
    }
    return grabDxgi();
}

bool WinCapture::resolveWindowRect(HWND hwnd, QRect& out, bool& ok) const {
    if (!IsWindow(hwnd) || !IsWindowVisible(hwnd)) { ok = false; return false; }

    BOOL cloaked = FALSE;
    DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
    if (cloaked) { ok = false; return false; }

    WINDOWPLACEMENT wp{};
    wp.length = sizeof(wp);
    if (!GetWindowPlacement(hwnd, &wp) || wp.showCmd == SW_SHOWMINIMIZED) { ok = false; return false; }

    RECT rc{};
    if (!GetWindowRect(hwnd, &rc)) { ok = false; return false; }
    if (rc.right - rc.left < 2 || rc.bottom - rc.top < 2) { ok = false; return false; }
    out = QRect(rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top);
    ok = true;
    return true;
}

// ---------- DXGI fullscreen path ----------

bool WinCapture::initDxgi(const QRect& region) {
    IDXGIFactory1* factory = nullptr;
    HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory));
    if (FAILED(hr)) return false;

    Microsoft::WRL::ComPtr<IDXGIAdapter1> pickedAdapter;
    Microsoft::WRL::ComPtr<IDXGIOutput1>  pickedOutput;

    for (UINT ai = 0;; ++ai) {
        Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
        if (factory->EnumAdapters1(ai, adapter.GetAddressOf()) == DXGI_ERROR_NOT_FOUND) break;

        for (UINT oi = 0;; ++oi) {
            Microsoft::WRL::ComPtr<IDXGIOutput1> output;
            if (adapter->EnumOutputs(oi, reinterpret_cast<IDXGIOutput**>(output.GetAddressOf())) == DXGI_ERROR_NOT_FOUND) break;

            DXGI_OUTPUT_DESC desc{};
            if (FAILED(output->GetDesc(&desc))) continue;
            // Does this output's desktop rect contain the region's center?
            const QRect outRect(desc.DesktopCoordinates.left, desc.DesktopCoordinates.top,
                                desc.DesktopCoordinates.right - desc.DesktopCoordinates.left,
                                desc.DesktopCoordinates.bottom - desc.DesktopCoordinates.top);
            if (outRect.contains(region.center())) {
                pickedAdapter = adapter;
                pickedOutput  = output;
                break;
            }
        }
        if (pickedOutput) break;
    }
    factory->Release();
    if (!pickedOutput) return false;   // shouldn't happen; caller retries? no — fall back visually

    hr = D3D11CreateDevice(
        pickedAdapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0,
        D3D11_SDK_VERSION, &m_device, nullptr, &m_context);
    if (FAILED(hr)) return false;

    hr = pickedOutput->DuplicateOutput(m_device.Get(), &m_dup);
    if (FAILED(hr)) { m_device.Reset(); m_context.Reset(); return false; }

    DXGI_OUTDUPL_DESC dupDesc{};
    m_dup->GetDesc(&dupDesc);

    // Staging texture for CPU readback.
    D3D11_TEXTURE2D_DESC texDesc{};
    texDesc.Width  = dupDesc.ModeDesc.Width;
    texDesc.Height = dupDesc.ModeDesc.Height;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Usage = D3D11_USAGE_STAGING;
    texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    texDesc.BindFlags = 0;
    texDesc.MiscFlags = 0;

    ID3D11Texture2D* staging = nullptr;
    hr = m_device->CreateTexture2D(&texDesc, nullptr, &staging);
    if (FAILED(hr)) { m_dup.Reset(); m_device.Reset(); m_context.Reset(); return false; }
    m_stagingTexture = staging;

    m_stride = (int)texDesc.Width * 4;
    m_buffer.assign((size_t)texDesc.Width * texDesc.Height * 4, 0);
    m_w = (int)texDesc.Width;
    m_h = (int)texDesc.Height;
    return true;
}

bool WinCapture::reinitDxgi() {
    QRect region = m_dupRegion;
    if (region.isEmpty()) region = QRect(0, 0, 2, 2);
    if (m_stagingTexture) { m_stagingTexture->Release(); m_stagingTexture = nullptr; }
    m_dup.Reset();
    m_context.Reset();
    m_device.Reset();
    return initDxgi(region);
}

bool WinCapture::grabDxgi() {
    Microsoft::WRL::ComPtr<IDXGIResource> desktopResource;
    DXGI_OUTDUPL_FRAME_INFO frameInfo{};

    HRESULT hr = m_dup->AcquireNextFrame(10, &frameInfo, &desktopResource);
    if (hr == DXGI_ERROR_WAIT_TIMEOUT) return true;   // no new frame; keep last buffer
    if (hr == DXGI_ERROR_ACCESS_LOST || hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET || hr == E_INVALIDARG) {
        if (!reinitDxgi()) return false;
        return true;                                   // frame arrives next tick
    }
    if (FAILED(hr)) return false;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> desktopTexture;
    hr = desktopResource.As(&desktopTexture);
    if (FAILED(hr)) { m_dup->ReleaseFrame(); return false; }

    m_context->CopyResource(m_stagingTexture, desktopTexture.Get());
    m_dup->ReleaseFrame();

    D3D11_MAPPED_SUBRESOURCE mapped{};
    hr = m_context->Map(m_stagingTexture, 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) return false;

    const int copyWidth  = (int)(m_stagingTexture ? m_w : 0);
    const int copyHeight = (int)(m_stagingTexture ? m_h : 0);
    for (int y = 0; y < copyHeight; ++y) {
        const uint8_t* src = reinterpret_cast<const uint8_t*>(mapped.pData) + (size_t)y * mapped.RowPitch;
        uint8_t* dst = m_buffer.data() + (size_t)y * (size_t)m_stride;
        std::memcpy(dst, src, (size_t)copyWidth * 4);
    }
    m_context->Unmap(m_stagingTexture, 0);

    if (m_showCursor) blendCursor(QRect(0, 0, m_w, m_h));
    return true;
}

// ---------- GDI window path ----------

bool WinCapture::ensureGdiResources(int w, int h) {
    if (m_memDC && m_gdiW == w && m_gdiH == h) return true;
    if (m_memDC) { DeleteDC(m_memDC); m_memDC = nullptr; }
    if (m_bitmap) { DeleteObject(m_bitmap); m_bitmap = nullptr; }

    HDC screenDC = GetDC(nullptr);
    m_memDC = CreateCompatibleDC(screenDC);
    m_bitmap = CreateCompatibleBitmap(screenDC, w, h);
    m_oldBmp = SelectObject(m_memDC, m_bitmap);
    ReleaseDC(nullptr, screenDC);
    if (!m_memDC || !m_bitmap) { close(); return false; }
    m_gdiW = w;
    m_gdiH = h;
    return true;
}

bool WinCapture::grabScreenRegion(const QRect& r) {
    HDC screenDC = GetDC(nullptr);
    if (!screenDC) return false;
    if (!ensureGdiResources(r.width(), r.height())) { ReleaseDC(nullptr, screenDC); return false; }

    const BOOL okBit = BitBlt(m_memDC, 0, 0, r.width(), r.height(), screenDC, r.x(), r.y(), SRCCOPY | CAPTUREBLT);
    ReleaseDC(nullptr, screenDC);
    if (!okBit) return false;

    BITMAPINFO bi{};
    bi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth       = r.width();
    bi.bmiHeader.biHeight      = -r.height();   // top-down
    bi.bmiHeader.biPlanes      = 1;
    bi.bmiHeader.biBitCount    = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    if (m_buffer.size() < (size_t)r.width() * r.height() * 4) m_buffer.resize((size_t)r.width() * r.height() * 4);
    const int got = GetDIBits(m_memDC, m_bitmap, 0, r.height(), m_buffer.data(), &bi, DIB_RGB_COLORS);
    if (got == 0) return false;

    m_w = r.width();
    m_h = r.height();
    m_stride = r.width() * 4;
    if (m_showCursor) blendCursor(r);
    return true;
}

bool WinCapture::grabWindowFrame(HWND hwnd) {
    QRect r;
    bool ok = false;
    if (!resolveWindowRect(hwnd, r, ok) || !ok) return false;

    r.setWidth(r.width() & ~1);
    r.setHeight(r.height() & ~1);
    if (r.width() < 2 || r.height() < 2) return false;

    // Recreate resources on size change.
    if (m_w != r.width() || m_h != r.height()) {
        m_w = r.width();
        m_h = r.height();
        if (m_memDC) { DeleteDC(m_memDC); m_memDC = nullptr; }
        if (m_bitmap) { DeleteObject(m_bitmap); m_bitmap = nullptr; }
        if (!ensureGdiResources(m_w, m_h)) return false;
        m_buffer.assign((size_t)m_w * (size_t)m_h * 4, 0);
        m_stride = m_w * 4;
        ++m_sizeGen;
    }

    if (PrintWindow(hwnd, m_memDC, PW_RENDERFULLCONTENT)) {
        BITMAPINFO bi{};
        bi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth       = m_w;
        bi.bmiHeader.biHeight      = -m_h;
        bi.bmiHeader.biPlanes      = 1;
        bi.bmiHeader.biBitCount    = 32;
        bi.bmiHeader.biCompression = BI_RGB;

        const int got = GetDIBits(m_memDC, m_bitmap, 0, m_h, m_buffer.data(), &bi, DIB_RGB_COLORS);
        if (got == 0) return grabScreenRegion(r);   // broken window? fall back to desktop crop
    } else {
        return grabScreenRegion(r);
    }

    if (m_showCursor) blendCursor(r);
    return true;
}

void WinCapture::blendCursor(const QRect& capturedRegion) {
    CURSORINFO ci{};
    ci.cbSize = sizeof(ci);
    if (!GetCursorInfo(&ci) || !(ci.flags & CURSOR_SHOWING)) return;

    ICONINFO ii{};
    if (!GetIconInfo(ci.hCursor, &ii)) return;

    // Color cursors have hbmColor set; mask bitmaps still needed for size.
    BITMAP bmpDesc{};
    if (!GetObject(ii.hbmMask, sizeof(bmpDesc), &bmpDesc)) {
        if (ii.hbmColor) DeleteObject(ii.hbmColor);
        DeleteObject(ii.hbmMask);
        return;
    }

    const int cw = bmpDesc.bmWidth, ch = bmpDesc.bmHeight;
    if (cw <= 0 || ch <= 0) { if (ii.hbmColor) DeleteObject(ii.hbmColor); DeleteObject(ii.hbmMask); return; }

    std::vector<uint8_t> pixels((size_t)cw * (size_t)ch * 4, 0);
    if (ii.hbmColor) {
        BITMAPINFO bi{};
        bi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth       = cw;
        bi.bmiHeader.biHeight      = -ch;
        bi.bmiHeader.biPlanes      = 1;
        bi.bmiHeader.biBitCount    = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        GetDIBits(m_memDC ? m_memDC : GetDC(nullptr), ii.hbmColor, 0, ch, pixels.data(), &bi, DIB_RGB_COLORS);
    } else {
        DeleteObject(ii.hbmMask);
        return;   // monochrome cursor: skip compositing (rare on modern Windows)
    }
    if (ii.hbmColor) DeleteObject(ii.hbmColor);
    DeleteObject(ii.hbmMask);

    const int x0 = ci.ptScreenPos.x - capturedRegion.x() - (int)ii.xHotspot;
    const int y0 = ci.ptScreenPos.y - capturedRegion.y() - (int)ii.yHotspot;

    for (int y = 0; y < ch; ++y) {
        const int dy = y0 + y;
        if (dy < 0 || dy >= m_h) continue;
        const uint8_t* srow = pixels.data() + (size_t)y * (size_t)cw * 4;
        uint8_t* drow = m_buffer.data() + (size_t)dy * (size_t)m_stride;
        for (int x = 0; x < cw; ++x) {
            const int dx = x0 + x;
            if (dx < 0 || dx >= m_w) continue;
            const uint8_t* s = srow + (size_t)x * 4;
            const uint32_t a = s[3];
            if (a == 0) continue;
            uint8_t* d = drow + (size_t)dx * 4;
            const uint32_t inv = 255 - a;
            d[0] = (uint8_t)((s[0] * a + d[0] * inv + 127) / 255);
            d[1] = (uint8_t)((s[1] * a + d[1] * inv + 127) / 255);
            d[2] = (uint8_t)((s[2] * a + d[2] * inv + 127) / 255);
        }
    }
}
