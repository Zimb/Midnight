#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <algorithm>
#include <cmath>
#include "ui_constants.h"

extern HINSTANCE g_hInst;

class KnobWidget {
public:
    static constexpr const wchar_t* kClassName = L"MidnightMMKnob";
    int  posMin = 0, posMax = 1000, pos = 500, posDefault = 500;
    HWND hWnd   = nullptr;
    HWND hOwner = nullptr;
    int  ctrlId = 0;
    bool dragging = false;
    POINT dragOrigin{};
    int   dragOriginPos = 0;

    static void registerClass(HINSTANCE hi) {
        WNDCLASSW existing{};
        if (GetClassInfoW(hi, kClassName, &existing)) return;
        WNDCLASSW wc{};
        wc.lpfnWndProc   = staticProc;
        wc.hInstance     = hi;
        wc.hbrBackground = nullptr; // we paint everything
        wc.lpszClassName = kClassName;
        wc.hCursor       = LoadCursor(nullptr, IDC_HAND);
        wc.style         = CS_DBLCLKS;
        RegisterClassW(&wc);
    }

    static HWND create(HWND parent, int id, int x, int y, int w, int h) {
        HWND wnd = CreateWindowExW(0, kClassName, L"",
            WS_CHILD | WS_VISIBLE,
            x, y, w, h, parent, (HMENU)(LONG_PTR)id, g_hInst, nullptr);
        return wnd;
    }

    int getPos() const { return pos; }
    void setPos(int p) {
        p = std::clamp(p, posMin, posMax);
        if (p != pos) { pos = p; if (hWnd) InvalidateRect(hWnd, nullptr, FALSE); }
    }

private:
    static LRESULT CALLBACK staticProc(HWND h, UINT m, WPARAM w, LPARAM l) {
        auto* self = reinterpret_cast<KnobWidget*>(GetWindowLongPtrW(h, GWLP_USERDATA));
        if (m == WM_NCCREATE) {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(l);
            self = new KnobWidget();
            self->hWnd   = h;
            self->hOwner = GetParent(h);
            self->ctrlId = (int)(LONG_PTR)cs->hMenu;
            SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)self);
            return TRUE;
        }
        if (!self) return DefWindowProcW(h, m, w, l);
        LRESULT r = self->proc(m, w, l);
        if (m == WM_NCDESTROY) {
            SetWindowLongPtrW(h, GWLP_USERDATA, 0);
            delete self;
        }
        return r;
    }

    void notifyOwner() {
        // Mimic a trackbar: send WM_HSCROLL with SB_THUMBTRACK so the parent
        // refresh path (onTrackbarChange) handles it uniformly.
        if (hOwner)
            SendMessageW(hOwner, WM_HSCROLL,
                         MAKEWPARAM(SB_THUMBTRACK, (WORD)pos),
                         (LPARAM)hWnd);
    }

    LRESULT proc(UINT m, WPARAM w, LPARAM l) {
        switch (m) {
            case WM_LBUTTONDOWN:
                SetCapture(hWnd);
                dragging = true;
                GetCursorPos(&dragOrigin);
                dragOriginPos = pos;
                return 0;
            case WM_MOUSEMOVE:
                if (dragging) {
                    POINT p; GetCursorPos(&p);
                    int dy = dragOrigin.y - p.y; // up = positive
                    bool fine = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                    int range = posMax - posMin;
                    int sensitivity = fine ? 600 : 150; // pixels for full sweep
                    int delta = (int)((double)dy * range / sensitivity);
                    setPos(dragOriginPos + delta);
                    notifyOwner();
                }
                return 0;
            case WM_LBUTTONUP:
                if (dragging) { ReleaseCapture(); dragging = false; }
                return 0;
            case WM_LBUTTONDBLCLK:
                setPos(posDefault);
                notifyOwner();
                return 0;
            case WM_MOUSEWHEEL: {
                int z = GET_WHEEL_DELTA_WPARAM(w) / WHEEL_DELTA;
                bool fine = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                int step = fine ? 4 : 25;
                setPos(pos + z * step);
                notifyOwner();
                return 0;
            }
            case WM_ERASEBKGND: return 1;
            case WM_PAINT: {
                PAINTSTRUCT ps; HDC hdc = BeginPaint(hWnd, &ps);
                paintKnob(hdc);
                EndPaint(hWnd, &ps);
                return 0;
            }
        }
        return DefWindowProcW(hWnd, m, w, l);
    }

    void paintKnob(HDC hdc) {
        RECT rc; GetClientRect(hWnd, &rc);
        int W = rc.right, H = rc.bottom;
        // Background fill (same as parent)
        HBRUSH brBg = CreateSolidBrush(kColBg);
        FillRect(hdc, &rc, brBg);
        DeleteObject(brBg);

        // Geometry
        int cx = W / 2;
        int cy = H / 2 + 1;
        int radius = std::min(W, H) / 2 - 4;
        int trackThick = 5;

        // Background arc (track)
        HPEN penTrack = CreatePen(PS_SOLID | PS_GEOMETRIC, trackThick, kColControl);
        HPEN penOld   = (HPEN)SelectObject(hdc, penTrack);
        SelectObject(hdc, GetStockObject(NULL_BRUSH));
        SetArcDirection(hdc, AD_CLOCKWISE);
        // Arc from 225Â° to -45Â° (going clockwise = 270Â° sweep, music-knob style)
        // We compute endpoints: start angle 225Â° (lower-left), end angle -45Â° (lower-right)
        const double PI = 3.14159265358979323846;
        double a0 = 225.0 * PI / 180.0; // lower-left
        double a1 = -45.0 * PI / 180.0; // lower-right
        int x0 = cx + (int)(radius * std::cos(a0));
        int y0 = cy - (int)(radius * std::sin(a0));
        int x1 = cx + (int)(radius * std::cos(a1));
        int y1 = cy - (int)(radius * std::sin(a1));
        Arc(hdc, cx - radius, cy - radius, cx + radius, cy + radius, x0, y0, x1, y1);

        // Foreground arc (filled portion proportional to value)
        double t = (double)(pos - posMin) / std::max(1, (posMax - posMin));
        t = std::clamp(t, 0.0, 1.0);
        double aSweep = (225.0 + 45.0) * PI / 180.0; // total = 270Â°
        double aEnd   = a0 - t * aSweep;
        int xe = cx + (int)(radius * std::cos(aEnd));
        int ye = cy - (int)(radius * std::sin(aEnd));
        SelectObject(hdc, penOld);
        DeleteObject(penTrack);
        HPEN penArc = CreatePen(PS_SOLID | PS_GEOMETRIC, trackThick, kColAccent);
        SelectObject(hdc, penArc);
        if (t > 0.001)
            Arc(hdc, cx - radius, cy - radius, cx + radius, cy + radius, x0, y0, xe, ye);
        SelectObject(hdc, GetStockObject(BLACK_PEN));
        DeleteObject(penArc);

        // Inner disc (knob body)
        int innerR = radius - 9;
        HBRUSH brBody = CreateSolidBrush(kColControlHi);
        HPEN   penBdy = CreatePen(PS_SOLID, 1, kColBorder);
        HBRUSH brOld  = (HBRUSH)SelectObject(hdc, brBody);
        SelectObject(hdc, penBdy);
        Ellipse(hdc, cx - innerR, cy - innerR, cx + innerR, cy + innerR);
        SelectObject(hdc, brOld);
        DeleteObject(brBody);
        SelectObject(hdc, GetStockObject(BLACK_PEN));
        DeleteObject(penBdy);

        // Indicator line
        double ai = aEnd;
        int ix1 = cx + (int)((innerR - 2) * std::cos(ai));
        int iy1 = cy - (int)((innerR - 2) * std::sin(ai));
        int ix2 = cx + (int)((innerR - 12) * std::cos(ai));
        int iy2 = cy - (int)((innerR - 12) * std::sin(ai));
        HPEN penInd = CreatePen(PS_SOLID, 3, kColAccent);
        HPEN penOldI = (HPEN)SelectObject(hdc, penInd);
        MoveToEx(hdc, ix2, iy2, nullptr);
        LineTo(hdc, ix1, iy1);
        SelectObject(hdc, penOldI);
        DeleteObject(penInd);
    }
};
