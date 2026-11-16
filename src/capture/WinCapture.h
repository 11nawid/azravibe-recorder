// Windows / Win32 capture backend.
// Screens/monitors: DXGI Desktop Duplication (fast, GPU-copied).
// Windows:          PrintWindow (PW_RENDERFULLCONTENT) with a screen-crop fallback.
// Cursor is composited from GetCursorInfo each frame.
//
// NOTE for CI/testers: DXGI requires a desktop session with an NVIDIA/AMD/Intel
// GPU (all supported); GDI works everywhere else. Both paths funnel into the
// same BGRA buffer the rest of the pipeline already understands.
#pragma once

#include "Capture.h"

#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include <memory>
#include <vector>

class WinCapture : public CaptureBackend {
public:
    WinCapture();
    ~WinCapture() override;

    WinCapture(const WinCapture&) = delete;
    WinCapture& operator=(const WinCapture&) = delete;

    bool open(const CaptureSpec& spec) override;
    void close() override;
    bool grab() override;
    void setShowCursor(bool show) override { m_showCursor = show; }

    const uint8_t*  data() const override { return m_buffer.data(); }
    int             bytesPerLine() const override { return m_stride; }
    int             width() const override { return m_w; }
    int             height() const override { return m_h; }
    RawPixelFormat  format() const override { return RawPixelFormat::BGR0; }  // Windows desktop is BGRA

private:
    // --- fullscreen (screen/monitor) path via DXGI Desktop Duplication ---
    bool initDxgi(const QRect& region);
    bool reinitDxgi();                        // after DXGI_ERROR_ACCESS_LOST
    bool grabDxgi();

    // --- window path via Win32 GDI ---
    bool grabWindowFrame(HWND hwnd);
    bool grabScreenRegion(const QRect& r);    // fallback for windows that refuse PrintWindow
    bool ensureGdiResources(int w, int h);

    void blendCursor(const QRect& capturedRegion);
    bool resolveWindowRect(HWND hwnd, QRect& out, bool& ok) const;

    CaptureSpec m_spec;

    // DXGI state
    Microsoft::WRL::ComPtr<ID3D11Device>           m_device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext>    m_context;
    Microsoft::WRL::ComPtr<IDXGIOutputDuplication> m_dup;
    ID3D11Texture2D*                               m_stagingTexture = nullptr;   // plain pointer, released manually
    DXGI_OUTDUPL_FRAME_INFO                        m_lastFrameInfo{};
    QRect                                          m_dupRegion;

    // GDI state (window path)
    HDC    m_memDC  = nullptr;
    HBITMAP m_bitmap = nullptr;
    HGDIOBJ m_oldBmp = nullptr;
    int    m_gdiW = 0, m_gdiH = 0;

    std::vector<uint8_t> m_buffer;
    int            m_w = 0, m_h = 0, m_stride = 0;
    unsigned       m_sizeGen = 1;
    bool           m_showCursor = true;
    bool           m_windowIsUsable = true;   // window exists & is visible
    bool           m_frameDirty = false;      // DXGI returned fresh content this tick
};
