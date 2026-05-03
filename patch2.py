path = r'C:\Users\dossa\OneDrive\Desktop\Midnight-1\midnight-plugins\plugins\melody_maker\view.cpp'
with open(path, encoding='utf-8') as f:
    src = f.read()

changes = []

# ===================================================================
# CHANGE A: refreshDeltaLabels - replace TBM_GETPOS lambda with kwGetPos
# ===================================================================
old = ('    void refreshDeltaLabels() {\n'
       '        if (!plugin) return;\n'
       '        int st = plugin->getStyleType();\n'
       '        const sfed::Sf2GenSnapshot& base = plugin->sf2BaseGensPerStyle[st];\n'
       '        auto gp = [&](HWND s) -> int { return s ? (int)SendMessageW(s, TBM_GETPOS, 0, 0) : 0; };')
new = ('    void refreshDeltaLabels() {\n'
       '        if (!plugin) return;\n'
       '        int st = plugin->getStyleType();\n'
       '        const sfed::Sf2GenSnapshot& base = plugin->sf2BaseGensPerStyle[st];\n'
       '        auto gp = [&](HWND s) -> int { return kwGetPos(s); };')
if src.count(old) == 1:
    src = src.replace(old, new, 1)
    changes.append("A: refreshDeltaLabels gp->kwGetPos")
else:
    print(f"SKIP A: {src.count(old)}")

# ===================================================================
# CHANGE B: commitDeltaAndReload - replace TBM_GETPOS lambda with kwGetPos
# ===================================================================
old = ('    void commitDeltaAndReload(int st) {\n'
       '        if (!plugin || plugin->externalSf2PerStyle[st].empty()) return;\n'
       '        auto gp = [&](HWND s) -> int { return s ? (int)SendMessageW(s, TBM_GETPOS, 0, 0) : 0; };')
new = ('    void commitDeltaAndReload(int st) {\n'
       '        if (!plugin || plugin->externalSf2PerStyle[st].empty()) return;\n'
       '        auto gp = [&](HWND s) -> int { return kwGetPos(s); };')
if src.count(old) == 1:
    src = src.replace(old, new, 1)
    changes.append("B: commitDeltaAndReload gp->kwGetPos")
else:
    print(f"SKIP B: {src.count(old)}")

# ===================================================================
# CHANGE C: Reset slider -> reset knob (resetSlider uses TBM_SETPOS)
# ===================================================================
old = ('                            auto resetSlider = [&](HWND s, int centre) {\n'
       '                                if (s) SendMessageW(s, TBM_SETPOS, TRUE, centre);\n'
       '                            };')
new = ('                            auto resetSlider = [&](HWND s, int centre) {\n'
       '                                kwSetPos(s, centre);\n'
       '                            };')
if src.count(old) == 1:
    src = src.replace(old, new, 1)
    changes.append("C: resetSlider -> kwSetPos")
else:
    print(f"SKIP C: {src.count(old)}")

# ===================================================================
# CHANGE D: WM_HSCROLL delta handler - the id range check is still valid for knobs
# (KnobWidget sends WM_HSCROLL/SB_THUMBTRACK), so no change needed there.
# ===================================================================

# ===================================================================
# CHANGE E: showExternalSf2Controls - just sh() calls, still valid (ShowWindow)
# No change needed.
# ===================================================================

# ===================================================================
# CHANGE F: Fix delta label drawing - knobs use GetWindowRect via HWND same as sliders.
# The existing code in WM_PAINT iterates kDeltaSlidersL/R and calls GetWindowRect.
# Since KnobWidget is also an HWND, this should still work. No change needed.
# ===================================================================

# ===================================================================
# CHANGE G: Fix dice button visibility in hideAllMakerControls
# ===================================================================
old = ('    void hideAllMakerControls() {\n'
       '        auto h = [](HWND w) { if (w) ShowWindow(w, SW_HIDE); };\n'
       '        h(hMakerEnable);   h(hMakerSynthCombo); h(hMakerNameEdit);')
new = ('    void hideAllMakerControls() {\n'
       '        auto h = [](HWND w) { if (w) ShowWindow(w, SW_HIDE); };\n'
       '        h(hDiceBtn); h(hRefreshBtn);\n'
       '        h(hMakerEnable);   h(hMakerSynthCombo); h(hMakerNameEdit);')
if src.count(old) == 1:
    src = src.replace(old, new, 1)
    changes.append("G: hideAllMakerControls hides dice+refresh")
else:
    print(f"SKIP G: {src.count(old)}")

# ===================================================================
# CHANGE H: Add WM_DRAWITEM handler for kIdMakerEnable (like JAM)
# ===================================================================
old = '                if (dis->CtlID == (UINT)kIdAuto) {\n                    drawListenButton(dis);\n                    return TRUE;\n                }'
new = ('                if (dis->CtlID == (UINT)kIdAuto) {\n'
       '                    drawListenButton(dis);\n'
       '                    return TRUE;\n'
       '                }\n'
       '                if (dis->CtlID == (UINT)kIdMakerEnable) {\n'
       '                    // Draw like the JAM button but reflects makerEnabled state\n'
       '                    RECT rc = dis->rcItem;\n'
       '                    int st = plugin ? plugin->getStyleType() : 0;\n'
       '                    bool on = makerEnabled[st];\n'
       '                    bool pressed = (dis->itemState & ODS_SELECTED) != 0;\n'
       '                    COLORREF fill = on\n'
       '                        ? (pressed ? kColAccentDark : kColAccent)\n'
       '                        : (pressed ? kColControlHi  : kColControl);\n'
       '                    COLORREF edge = on ? kColAccent : kColTextDim;\n'
       '                    COLORREF txtC = on ? kColWhite  : kColText;\n'
       '                    HBRUSH br = CreateSolidBrush(fill);\n'
       '                    HPEN   pn = CreatePen(PS_SOLID, 1, edge);\n'
       '                    HBRUSH brOld = (HBRUSH)SelectObject(dis->hDC, br);\n'
       '                    HPEN   pnOld = (HPEN)SelectObject(dis->hDC, pn);\n'
       '                    RoundRect(dis->hDC, rc.left, rc.top, rc.right, rc.bottom, 10, 10);\n'
       '                    SelectObject(dis->hDC, brOld); SelectObject(dis->hDC, pnOld);\n'
       '                    DeleteObject(br); DeleteObject(pn);\n'
       '                    wchar_t txt[64]; int len = GetWindowTextW(dis->hwndItem, txt, 64);\n'
       '                    SetBkMode(dis->hDC, TRANSPARENT);\n'
       '                    SetTextColor(dis->hDC, txtC);\n'
       '                    HFONT old = (HFONT)SelectObject(dis->hDC, fontBold);\n'
       '                    DrawTextW(dis->hDC, txt, len, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);\n'
       '                    SelectObject(dis->hDC, old);\n'
       '                    return TRUE;\n'
       '                }')
if src.count(old) == 1:
    src = src.replace(old, new, 1)
    changes.append("H: WM_DRAWITEM kIdMakerEnable")
else:
    print(f"SKIP H: {src.count(old)}")

# ===================================================================
# CHANGE I: Center FX OUI/NON toggle: placeTog lambda  
# ===================================================================
old = ('            auto placeTog = [&](int colX, int y, HWND tog) {\n'
       '                if (tog) { MoveWindow(tog, colX, y + 2, togW, 24, TRUE); ShowWindow(tog, SW_SHOW); }\n'
       '            };')
new = ('            auto placeTog = [&](int colX, int y, HWND tog) {\n'
       '                if (tog) {\n'
       '                    // Center the toggle in its column\n'
       '                    int tx = colX + (colW - togW) / 2;\n'
       '                    MoveWindow(tog, tx, y + 2, togW, 24, TRUE);\n'
       '                    ShowWindow(tog, SW_SHOW);\n'
       '                }\n'
       '            };')
if src.count(old) == 1:
    src = src.replace(old, new, 1)
    changes.append("I: FX placeTog centered")
else:
    print(f"SKIP I: {src.count(old)}")

# ===================================================================
# CHANGE J: Fix FX WM_PAINT section title to span full column 
# (remove togW offset so title is centered)
# ===================================================================
old = '                // Section title (to the right of toggle)\n                SetTextColor(hdc, kColAccent);\n                RECT rcS = { cx + togW + 6, sy, cx + colW, sy + secH };\n                DrawTextW(hdc, kFxSec[fi], -1, &rcS, DT_LEFT | DT_VCENTER | DT_SINGLELINE);\n                // Separator line\n                int lineX0 = cx + togW + 6 + 90;'
new = '                // Section title — centered over the full column\n                SetTextColor(hdc, kColAccent);\n                RECT rcS = { cx, sy, cx + colW, sy + secH };\n                DrawTextW(hdc, kFxSec[fi], -1, &rcS, DT_CENTER | DT_VCENTER | DT_SINGLELINE);\n                // Separator line\n                int lineX0 = cx + 10;'
if src.count(old) == 1:
    src = src.replace(old, new, 1)
    changes.append("J: FX section title centered")
else:
    print(f"SKIP J: {src.count(old)}")

# ===================================================================
# CHANGE K: Fix knob label truncation in FX WM_PAINT
# Extend rect by 20px on each side
# ===================================================================
old = '                    RECT rcP = { kx, knobY - 16, kx + kKnobSize, knobY };\n                    DrawTextW(hdc, kFxPar[fi][pi], -1, &rcP, DT_CENTER | DT_VCENTER | DT_SINGLELINE);'
new = '                    RECT rcP = { kx - 22, knobY - 16, kx + kKnobSize + 22, knobY };\n                    DrawTextW(hdc, kFxPar[fi][pi], -1, &rcP, DT_CENTER | DT_VCENTER | DT_SINGLELINE);'
if src.count(old) == 1:
    src = src.replace(old, new, 1)
    changes.append("K: FX knob label wider rect")
else:
    print(f"SKIP K: {src.count(old)}")

with open(path, 'w', encoding='utf-8') as f:
    f.write(src)

for c in changes:
    print(f"Done: {c}")
print("Batch 2 complete")
