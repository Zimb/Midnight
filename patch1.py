import re

path = r'C:\Users\dossa\OneDrive\Desktop\Midnight-1\midnight-plugins\plugins\melody_maker\view.cpp'
with open(path, encoding='utf-8') as f:
    src = f.read()

changes_done = []

# ===================================================================
# CHANGE 1: Add hRefreshBtn member variable after hDiceBtn
# ===================================================================
old = '    HWND hDiceBtn = nullptr;'
new = '    HWND hDiceBtn = nullptr;\n    HWND hRefreshBtn = nullptr;  // Circular arrow refresh button'
if src.count(old) == 1:
    src = src.replace(old, new, 1)
    changes_done.append("1: hRefreshBtn member")
else:
    print(f"SKIP 1: count={src.count(old)}")

# ===================================================================
# CHANGE 2: Make hMakerEnable owner-drawn (like JAM)
# ===================================================================
old = 'WS_CHILD | BS_PUSHBUTTON | WS_TABSTOP,\n            0, 0, 110, 28, hWnd, (HMENU)(LONG_PTR)kIdMakerEnable, hi, nullptr);'
new = 'WS_CHILD | BS_PUSHBUTTON | BS_OWNERDRAW | WS_TABSTOP,\n            0, 0, 140, 26, hWnd, (HMENU)(LONG_PTR)kIdMakerEnable, hi, nullptr);'
if src.count(old) == 1:
    src = src.replace(old, new, 1)
    changes_done.append("2: hMakerEnable owner-drawn")
else:
    print(f"SKIP 2: count={src.count(old)}")

# ===================================================================
# CHANGE 3: Add hRefreshBtn creation after dice button
# ===================================================================
old = '        SendMessageW(hDiceBtn, WM_SETFONT, (WPARAM)fontBold, TRUE);\n\n        // ---------- Piano: "M'
new = ('        SendMessageW(hDiceBtn, WM_SETFONT, (WPARAM)fontBold, TRUE);\n\n'
       '        // ---------- Refresh button (circular arrow) shown in Maker tab top-right\n'
       '        hRefreshBtn = CreateWindowW(L"BUTTON", L"\\u27F3",\n'
       '            WS_CHILD | BS_PUSHBUTTON | WS_TABSTOP,\n'
       '            0, 0, 36, 36, hWnd,\n'
       '            (HMENU)(LONG_PTR)kIdRefresh, hi, nullptr);\n'
       '        SendMessageW(hRefreshBtn, WM_SETFONT, (WPARAM)fontBold, TRUE);\n\n'
       '        // ---------- Piano: "M')
if src.count(old) == 1:
    src = src.replace(old, new, 1)
    changes_done.append("3: hRefreshBtn created")
else:
    print(f"SKIP 3: count={src.count(old)}")

# ===================================================================
# CHANGE 4: Convert mkDeltaSlider to mkDeltaKnob (find unique anchor)
# ===================================================================
old4 = 'auto mkDeltaSlider = [&](int id, int lo, int hi_, int init) -> HWND {'
new4 = 'auto mkDeltaKnob = [&](int id, int lo, int hi_, int init) -> HWND {'
if src.count(old4) == 1:
    src = src.replace(old4, new4, 1)
    changes_done.append("4a: mkDeltaSlider renamed")
else:
    print(f"SKIP 4a: count={src.count(old4)}")

old4b = ('            HWND s = CreateWindowW(TRACKBAR_CLASSW, L"",\n'
         '                WS_CHILD | TBS_HORZ | TBS_NOTICKS,\n'
         '                0, 0, 200, 22, hWnd, (HMENU)(LONG_PTR)id, hi, nullptr);\n'
         '            SendMessageW(s, TBM_SETRANGE, TRUE, MAKELPARAM(lo, hi_));\n'
         '            SendMessageW(s, TBM_SETPOS, TRUE, init);\n'
         '            return s;')
new4b = ('            HWND h2 = KnobWidget::create(hWnd, id, 0, 0, kKnobSize, kKnobSize);\n'
         '            auto* kw2 = reinterpret_cast<KnobWidget*>(GetWindowLongPtrW(h2, GWLP_USERDATA));\n'
         '            if (kw2) { kw2->posMin = lo; kw2->posMax = hi_;\n'
         '                       kw2->posDefault = init; kw2->setPos(init); }\n'
         '            return h2;')
if src.count(old4b) == 1:
    src = src.replace(old4b, new4b, 1)
    changes_done.append("4b: mkDeltaKnob body")
else:
    print(f"SKIP 4b: count={src.count(old4b)}")

old4c = ('        auto mkDeltaLabel = [&]() -> HWND {\n'
         '            HWND lv = CreateWindowW(L"STATIC", L"0",\n'
         '                WS_CHILD | SS_RIGHT,\n'
         '                0, 0, 44, 18, hWnd, nullptr, hi, nullptr);\n'
         '            SendMessageW(lv, WM_SETFONT, (WPARAM)fontReg, TRUE);\n'
         '            return lv;\n'
         '        };')
new4c = ('        auto mkDeltaValLbl = [&]() -> HWND {\n'
         '            HWND lv = CreateWindowW(L"STATIC", L"0",\n'
         '                WS_CHILD | SS_CENTER,\n'
         '                0, 0, kKnobSize, 16, hWnd, nullptr, hi, nullptr);\n'
         '            SendMessageW(lv, WM_SETFONT, (WPARAM)fontReg, TRUE);\n'
         '            return lv;\n'
         '        };')
if src.count(old4c) == 1:
    src = src.replace(old4c, new4c, 1)
    changes_done.append("4c: mkDeltaValLbl body")
else:
    print(f"SKIP 4c: count={src.count(old4c)}")

# Replace all mkDeltaSlider( calls with mkDeltaKnob(
c = src.count('mkDeltaSlider(')
src = src.replace('mkDeltaSlider(', 'mkDeltaKnob(')
changes_done.append(f"4d: {c} mkDeltaSlider( -> mkDeltaKnob(")

# Replace all mkDeltaLabel() calls with mkDeltaValLbl()
c = src.count('mkDeltaLabel()')
src = src.replace('mkDeltaLabel()', 'mkDeltaValLbl()')
changes_done.append(f"4e: {c} mkDeltaLabel() -> mkDeltaValLbl()")

with open(path, 'w', encoding='utf-8') as f:
    f.write(src)

for c in changes_done:
    print(f"Done: {c}")
print("Batch 1 complete")
