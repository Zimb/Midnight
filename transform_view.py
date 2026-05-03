"""
Transform view.cpp: Replace slider controls with KnobWidget for
- FX panel: 3 knobs per FX, horizontal, title above
- Maker Envelope: 4 knobs, centered, side-by-side with Timbre
- Maker Timbre: 3 knobs, to the right of Envelope
"""
import re

path = r"C:\Users\dossa\OneDrive\Desktop\Midnight-1\midnight-plugins\plugins\melody_maker\view.cpp"

with open(path, encoding='utf-8') as f:
    text = f.read()

# ─────────────────────────────────────────────────────────────────────────────
# 1. Change mkSlider lambda → mkKnob  (Maker ADSR + Timbre)
# ─────────────────────────────────────────────────────────────────────────────
old = '''        // Helper: create a horizontal trackbar (no ticks)
        auto mkSlider = [&](int id, int rangeMin, int rangeMax, int defVal) -> HWND {
            HWND s = CreateWindowW(TRACKBAR_CLASS, L"",
                WS_CHILD | TBS_HORZ | TBS_NOTICKS | WS_TABSTOP,
                0, 0, 460, 22, hWnd, (HMENU)(LONG_PTR)id, hi, nullptr);
            SendMessageW(s, TBM_SETRANGE, TRUE, MAKELONG(rangeMin, rangeMax));
            SendMessageW(s, TBM_SETPOS, TRUE, defVal);
            return s;
        };'''
new = '''        // Helper: create a knob (KnobWidget) for Maker envelope/timbre
        auto mkKnob = [&](int id, int rangeMin, int rangeMax, int defVal) -> HWND {
            HWND h = KnobWidget::create(hWnd, id, 0, 0, kKnobSize, kKnobSize);
            auto* kw = reinterpret_cast<KnobWidget*>(GetWindowLongPtrW(h, GWLP_USERDATA));
            if (kw) { kw->posMin = rangeMin; kw->posMax = rangeMax;
                      kw->posDefault = defVal; kw->setPos(defVal); }
            return h;
        };'''
assert old in text, "mkSlider lambda not found"
text = text.replace(old, new, 1)

# ─────────────────────────────────────────────────────────────────────────────
# 2. Change mkValLbl for Maker → SS_CENTER (better under-knob display)
# ─────────────────────────────────────────────────────────────────────────────
old = '''        auto mkValLbl = [&]() -> HWND {
            HWND v = CreateWindowW(L"STATIC", L"",
                WS_CHILD | SS_LEFT, 0, 0, 90, 20, hWnd, nullptr, hi, nullptr);
            SendMessageW(v, WM_SETFONT, (WPARAM)fontBold, TRUE);
            return v;
        };'''
new = '''        auto mkValLbl = [&]() -> HWND {
            HWND v = CreateWindowW(L"STATIC", L"",
                WS_CHILD | SS_CENTER, 0, 0, kKnobSize, 18, hWnd, nullptr, hi, nullptr);
            SendMessageW(v, WM_SETFONT, (WPARAM)fontBold, TRUE);
            return v;
        };'''
assert old in text, "mkValLbl not found"
text = text.replace(old, new, 1)

# ─────────────────────────────────────────────────────────────────────────────
# 3. Change 7 Maker slider creations to mkKnob
# ─────────────────────────────────────────────────────────────────────────────
old = '''        hMakerAttackSlider  = mkSlider(kIdMakerAttack,  1, 200,  1); hMakerAttackVal  = mkValLbl();
        hMakerDecaySlider   = mkSlider(kIdMakerDecay,   1, 300, 40); hMakerDecayVal   = mkValLbl();
        hMakerSustainSlider = mkSlider(kIdMakerSustain, 0, 100, 45); hMakerSustainVal = mkValLbl();
        hMakerReleaseSlider = mkSlider(kIdMakerRelease, 1, 400, 60); hMakerReleaseVal = mkValLbl();
        // Timbre sliders: ModIndex ×0.1→0..20, ModDecay ×0.1→0..30, Gain ×0.01→0.01..1
        hMakerModIndexSlider = mkSlider(kIdMakerModIndex, 0, 200, 40); hMakerModIndexVal = mkValLbl();
        hMakerModDecaySlider = mkSlider(kIdMakerModDecay, 0, 300, 60); hMakerModDecayVal = mkValLbl();
        hMakerGainSlider     = mkSlider(kIdMakerGain,     1, 100, 90); hMakerGainVal     = mkValLbl();'''
new = '''        hMakerAttackSlider  = mkKnob(kIdMakerAttack,  1, 200,  1); hMakerAttackVal  = mkValLbl();
        hMakerDecaySlider   = mkKnob(kIdMakerDecay,   1, 300, 40); hMakerDecayVal   = mkValLbl();
        hMakerSustainSlider = mkKnob(kIdMakerSustain, 0, 100, 45); hMakerSustainVal = mkValLbl();
        hMakerReleaseSlider = mkKnob(kIdMakerRelease, 1, 400, 60); hMakerReleaseVal = mkValLbl();
        // Timbre knobs: ModIndex ×0.1→0..20, ModDecay ×0.1→0..30, Gain ×0.01→0.01..1
        hMakerModIndexSlider = mkKnob(kIdMakerModIndex, 0, 200, 40); hMakerModIndexVal = mkValLbl();
        hMakerModDecaySlider = mkKnob(kIdMakerModDecay, 0, 300, 60); hMakerModDecayVal = mkValLbl();
        hMakerGainSlider     = mkKnob(kIdMakerGain,     1, 100, 90); hMakerGainVal     = mkValLbl();'''
assert old in text, "Maker slider creations not found"
text = text.replace(old, new, 1)

# ─────────────────────────────────────────────────────────────────────────────
# 4. Change mkFxSlider lambda → mkFxKnob
# ─────────────────────────────────────────────────────────────────────────────
old = '''        auto mkFxSlider = [&](int id, int lo, int hi_, int init) -> HWND {
            HWND s = CreateWindowW(TRACKBAR_CLASSW, L"",
                WS_CHILD | TBS_HORZ | TBS_NOTICKS,
                0, 0, 200, 22, hWnd, (HMENU)(LONG_PTR)id, hi, nullptr);
            SendMessageW(s, TBM_SETRANGE, TRUE, MAKELPARAM(lo, hi_));
            SendMessageW(s, TBM_SETPOS, TRUE, init);
            return s;
        };'''
new = '''        auto mkFxKnob = [&](int id, int lo, int hi_, int init) -> HWND {
            HWND h = KnobWidget::create(hWnd, id, 0, 0, kKnobSize, kKnobSize);
            auto* kw = reinterpret_cast<KnobWidget*>(GetWindowLongPtrW(h, GWLP_USERDATA));
            if (kw) { kw->posMin = lo; kw->posMax = hi_;
                      kw->posDefault = init; kw->setPos(init); }
            return h;
        };'''
assert old in text, "mkFxSlider lambda not found"
text = text.replace(old, new, 1)

# ─────────────────────────────────────────────────────────────────────────────
# 5. Change mkFxValLbl to SS_CENTER
# ─────────────────────────────────────────────────────────────────────────────
old = '''        auto mkFxValLbl = [&]() -> HWND {
            HWND v = CreateWindowW(L"STATIC", L"", WS_CHILD | SS_LEFT,
                0, 0, 70, 20, hWnd, nullptr, hi, nullptr);
            SendMessageW(v, WM_SETFONT, (WPARAM)fontBold, TRUE);
            return v;
        };'''
new = '''        auto mkFxValLbl = [&]() -> HWND {
            HWND v = CreateWindowW(L"STATIC", L"", WS_CHILD | SS_CENTER,
                0, 0, kKnobSize, 16, hWnd, nullptr, hi, nullptr);
            SendMessageW(v, WM_SETFONT, (WPARAM)fontBold, TRUE);
            return v;
        };'''
assert old in text, "mkFxValLbl not found"
text = text.replace(old, new, 1)

# ─────────────────────────────────────────────────────────────────────────────
# 6. Change 12 FX slider creations to mkFxKnob
# ─────────────────────────────────────────────────────────────────────────────
old = '''        hFxChorusRate     = mkFxSlider(kIdFxChorusRate,    5,   600, 70);   // 0.05..6 Hz (×0.01)'''
new = '''        hFxChorusRate     = mkFxKnob(kIdFxChorusRate,    5,   600, 70);   // 0.05..6 Hz (×0.01)'''
assert old in text
text = text.replace(old, new, 1)

old = '''        hFxChorusDepth    = mkFxSlider(kIdFxChorusDepth,   0,   200, 50);   // 0..20 ms (×0.1)'''
new = '''        hFxChorusDepth    = mkFxKnob(kIdFxChorusDepth,   0,   200, 50);   // 0..20 ms (×0.1)'''
assert old in text
text = text.replace(old, new, 1)

old = '''        hFxChorusMix      = mkFxSlider(kIdFxChorusMix,     0,   100, 50);   // 0..100%'''
new = '''        hFxChorusMix      = mkFxKnob(kIdFxChorusMix,     0,   100, 50);   // 0..100%'''
assert old in text
text = text.replace(old, new, 1)

old = '''        hFxDelayTime      = mkFxSlider(kIdFxDelayTime,    10,  1500, 350);  // 10..1500 ms'''
new = '''        hFxDelayTime      = mkFxKnob(kIdFxDelayTime,    10,  1500, 350);  // 10..1500 ms'''
assert old in text
text = text.replace(old, new, 1)

old = '''        hFxDelayFb        = mkFxSlider(kIdFxDelayFb,       0,    92, 40);   // 0..92%'''
new = '''        hFxDelayFb        = mkFxKnob(kIdFxDelayFb,       0,    92, 40);   // 0..92%'''
assert old in text
text = text.replace(old, new, 1)

old = '''        hFxDelayMix       = mkFxSlider(kIdFxDelayMix,      0,   100, 30);   // 0..100%'''
new = '''        hFxDelayMix       = mkFxKnob(kIdFxDelayMix,      0,   100, 30);   // 0..100%'''
assert old in text
text = text.replace(old, new, 1)

old = '''        hFxReverbSize     = mkFxSlider(kIdFxReverbSize,    0,   100, 55);   // 0..100%'''
new = '''        hFxReverbSize     = mkFxKnob(kIdFxReverbSize,    0,   100, 55);   // 0..100%'''
assert old in text
text = text.replace(old, new, 1)

old = '''        hFxReverbDamp     = mkFxSlider(kIdFxReverbDamp,    0,   100, 30);   // 0..100%'''
new = '''        hFxReverbDamp     = mkFxKnob(kIdFxReverbDamp,    0,   100, 30);   // 0..100%'''
assert old in text
text = text.replace(old, new, 1)

old = '''        hFxReverbMix      = mkFxSlider(kIdFxReverbMix,     0,   100, 25);   // 0..100%'''
new = '''        hFxReverbMix      = mkFxKnob(kIdFxReverbMix,     0,   100, 25);   // 0..100%'''
assert old in text
text = text.replace(old, new, 1)

old = '''        hFxNoiseLevel     = mkFxSlider(kIdFxNoiseLevel,    0,   100, 10);   // 0..100%'''
new = '''        hFxNoiseLevel     = mkFxKnob(kIdFxNoiseLevel,    0,   100, 10);   // 0..100%'''
assert old in text
text = text.replace(old, new, 1)

old = '''        hFxNoiseFlutter   = mkFxSlider(kIdFxNoiseFlutter,  0,   100, 50);   // 0..100%'''
new = '''        hFxNoiseFlutter   = mkFxKnob(kIdFxNoiseFlutter,  0,   100, 50);   // 0..100%'''
assert old in text
text = text.replace(old, new, 1)

old = '''        hFxNoiseTone      = mkFxSlider(kIdFxNoiseTone,     0,   100, 40);   // 0..100%'''
new = '''        hFxNoiseTone      = mkFxKnob(kIdFxNoiseTone,     0,   100, 40);   // 0..100%'''
assert old in text
text = text.replace(old, new, 1)

# ─────────────────────────────────────────────────────────────────────────────
# 7. Add kwGetPos / kwSetPos / kwSetRange helpers after the gp() static
# ─────────────────────────────────────────────────────────────────────────────
old = '    static int gp(HWND s) { return s ? (int)SendMessageW(s, TBM_GETPOS, 0, 0) : 0; }'
new = '''    static int gp(HWND s) { return s ? (int)SendMessageW(s, TBM_GETPOS, 0, 0) : 0; }

    // KnobWidget helpers (used for Maker ADSR/Timbre + FX knobs)
    static int kwGetPos(HWND h) {
        if (!h) return 0;
        auto* kw = reinterpret_cast<KnobWidget*>(GetWindowLongPtrW(h, GWLP_USERDATA));
        return kw ? kw->getPos() : 0;
    }
    static void kwSetPos(HWND h, int v) {
        if (!h) return;
        auto* kw = reinterpret_cast<KnobWidget*>(GetWindowLongPtrW(h, GWLP_USERDATA));
        if (kw) kw->setPos(v);
    }
    static void kwSetRange(HWND h, int lo, int hi) {
        if (!h) return;
        auto* kw = reinterpret_cast<KnobWidget*>(GetWindowLongPtrW(h, GWLP_USERDATA));
        if (kw) { kw->posMin = lo; kw->posMax = hi; }
    }'''
assert old in text, "gp() static not found"
text = text.replace(old, new, 1)

# ─────────────────────────────────────────────────────────────────────────────
# 8. Update refreshFxLabels: use kwGetPos for FX knobs
# ─────────────────────────────────────────────────────────────────────────────
old = '''    void refreshFxLabels() {
        wchar_t buf[64];
        auto setLbl = [&](HWND lbl, const wchar_t* txt) {
            if (lbl) SetWindowTextW(lbl, txt);
        };
        // Chorus
        swprintf(buf, 64, L"%.2f Hz", gp(hFxChorusRate)  * 0.01f);   setLbl(hFxChorusRateVal, buf);
        swprintf(buf, 64, L"%.1f ms", gp(hFxChorusDepth) * 0.1f);    setLbl(hFxChorusDepthVal, buf);
        swprintf(buf, 64, L"%d%%",    gp(hFxChorusMix));             setLbl(hFxChorusMixVal, buf);
        // Delay
        swprintf(buf, 64, L"%d ms",   gp(hFxDelayTime));             setLbl(hFxDelayTimeVal, buf);
        swprintf(buf, 64, L"%d%%",    gp(hFxDelayFb));               setLbl(hFxDelayFbVal, buf);
        swprintf(buf, 64, L"%d%%",    gp(hFxDelayMix));              setLbl(hFxDelayMixVal, buf);
        // Reverb
        swprintf(buf, 64, L"%d%%",    gp(hFxReverbSize));            setLbl(hFxReverbSizeVal, buf);
        swprintf(buf, 64, L"%d%%",    gp(hFxReverbDamp));            setLbl(hFxReverbDampVal, buf);
        swprintf(buf, 64, L"%d%%",    gp(hFxReverbMix));             setLbl(hFxReverbMixVal, buf);
        // Cassette noise
        swprintf(buf, 64, L"%d%%",    gp(hFxNoiseLevel));            setLbl(hFxNoiseLevelVal, buf);
        swprintf(buf, 64, L"%d%%",    gp(hFxNoiseFlutter));          setLbl(hFxNoiseFlutterVal, buf);
        swprintf(buf, 64, L"%d%%",    gp(hFxNoiseTone));             setLbl(hFxNoiseToneVal, buf);
    }'''
new = '''    void refreshFxLabels() {
        wchar_t buf[64];
        auto setLbl = [&](HWND lbl, const wchar_t* txt) {
            if (lbl) SetWindowTextW(lbl, txt);
        };
        // Chorus
        swprintf(buf, 64, L"%.2f Hz", kwGetPos(hFxChorusRate)  * 0.01f);   setLbl(hFxChorusRateVal, buf);
        swprintf(buf, 64, L"%.1f ms", kwGetPos(hFxChorusDepth) * 0.1f);    setLbl(hFxChorusDepthVal, buf);
        swprintf(buf, 64, L"%d%%",    kwGetPos(hFxChorusMix));             setLbl(hFxChorusMixVal, buf);
        // Delay
        swprintf(buf, 64, L"%d ms",   kwGetPos(hFxDelayTime));             setLbl(hFxDelayTimeVal, buf);
        swprintf(buf, 64, L"%d%%",    kwGetPos(hFxDelayFb));               setLbl(hFxDelayFbVal, buf);
        swprintf(buf, 64, L"%d%%",    kwGetPos(hFxDelayMix));              setLbl(hFxDelayMixVal, buf);
        // Reverb
        swprintf(buf, 64, L"%d%%",    kwGetPos(hFxReverbSize));            setLbl(hFxReverbSizeVal, buf);
        swprintf(buf, 64, L"%d%%",    kwGetPos(hFxReverbDamp));            setLbl(hFxReverbDampVal, buf);
        swprintf(buf, 64, L"%d%%",    kwGetPos(hFxReverbMix));             setLbl(hFxReverbMixVal, buf);
        // Cassette noise
        swprintf(buf, 64, L"%d%%",    kwGetPos(hFxNoiseLevel));            setLbl(hFxNoiseLevelVal, buf);
        swprintf(buf, 64, L"%d%%",    kwGetPos(hFxNoiseFlutter));          setLbl(hFxNoiseFlutterVal, buf);
        swprintf(buf, 64, L"%d%%",    kwGetPos(hFxNoiseTone));             setLbl(hFxNoiseToneVal, buf);
    }'''
assert old in text, "refreshFxLabels not found"
text = text.replace(old, new, 1)

# ─────────────────────────────────────────────────────────────────────────────
# 9. Update commitFxFromControls: use kwGetPos
# ─────────────────────────────────────────────────────────────────────────────
old = '''    void commitFxFromControls(int style) {
        if (!plugin || style < 0 || style >= 6) return;
        auto& p = plugin->fxParamsPerStyle[style];
        // Helper: x^2 curve so 20% slider → 4% effect, 50% → 25%, 100% → 100%.
        // Applied to perceptual "intensity" params (mix, depth, feedback, size).
        // Rate / time / tone / damp stay linear.
        auto sq = [](float v) { return v * v; };
        p.chorusRate  = gp(hFxChorusRate)   * 0.01f;                    // linear (Hz)
        p.chorusDepth = sq(gp(hFxChorusDepth) * 0.01f) * 0.8f;          // log, max 0.8 ms
        p.chorusMix   = sq(gp(hFxChorusMix)   * 0.01f) * 0.60f;         // log, max 60%
        p.delayTime   = (float)gp(hFxDelayTime);                         // linear (ms)
        p.delayFb     = sq(gp(hFxDelayFb)     * 0.01f) * 0.88f;         // log, max 0.88
        p.delayMix    = sq(gp(hFxDelayMix)    * 0.01f) * 0.60f;         // log, max 60%
        p.reverbSize  = sq(gp(hFxReverbSize)  * 0.01f) * 0.92f;         // log, max 0.92
        p.reverbDamp  = gp(hFxReverbDamp)  * 0.01f;                      // linear (tone)
        p.reverbMix   = sq(gp(hFxReverbMix)   * 0.01f) * 0.65f;         // log, max 65%
        p.noiseLevel  = gp(hFxNoiseLevel)  * 0.01f;  // setLevel() applies l² internally
        p.noiseFlutter= gp(hFxNoiseFlutter)* 0.01f;
        p.noiseTone   = gp(hFxNoiseTone)   * 0.01f;'''
new = '''    void commitFxFromControls(int style) {
        if (!plugin || style < 0 || style >= 6) return;
        auto& p = plugin->fxParamsPerStyle[style];
        // Helper: x^2 curve so 20% knob → 4% effect, 50% → 25%, 100% → 100%.
        // Applied to perceptual "intensity" params (mix, depth, feedback, size).
        // Rate / time / tone / damp stay linear.
        auto sq = [](float v) { return v * v; };
        p.chorusRate  = kwGetPos(hFxChorusRate)   * 0.01f;
        p.chorusDepth = sq(kwGetPos(hFxChorusDepth) * 0.01f) * 0.8f;
        p.chorusMix   = sq(kwGetPos(hFxChorusMix)   * 0.01f) * 0.60f;
        p.delayTime   = (float)kwGetPos(hFxDelayTime);
        p.delayFb     = sq(kwGetPos(hFxDelayFb)     * 0.01f) * 0.88f;
        p.delayMix    = sq(kwGetPos(hFxDelayMix)    * 0.01f) * 0.60f;
        p.reverbSize  = sq(kwGetPos(hFxReverbSize)  * 0.01f) * 0.92f;
        p.reverbDamp  = kwGetPos(hFxReverbDamp)  * 0.01f;
        p.reverbMix   = sq(kwGetPos(hFxReverbMix)   * 0.01f) * 0.65f;
        p.noiseLevel  = kwGetPos(hFxNoiseLevel)  * 0.01f;
        p.noiseFlutter= kwGetPos(hFxNoiseFlutter)* 0.01f;
        p.noiseTone   = kwGetPos(hFxNoiseTone)   * 0.01f;'''
assert old in text, "commitFxFromControls not found"
text = text.replace(old, new, 1)

# ─────────────────────────────────────────────────────────────────────────────
# 10. Update refreshFxFromParams: use kwSetPos
# ─────────────────────────────────────────────────────────────────────────────
old = '''    void refreshFxFromParams(int style) {
        if (!plugin || style < 0 || style >= 6) return;
        const auto& p = plugin->fxParamsPerStyle[style];
        auto setS = [&](HWND s, int v) { if (s) SendMessageW(s, TBM_SETPOS, TRUE, v); };
        // Invert the x^2*max curve: slider = round(sqrt(v/max)*100)
        auto invSq = [](float v, float maxV) -> int {
            if (maxV <= 0.0f) return 0;
            return (int)std::round(std::sqrt(std::clamp(v / maxV, 0.0f, 1.0f)) * 100.0f);
        };
        setS(hFxChorusRate,    std::clamp((int)std::round(p.chorusRate / 0.01f), 5, 600));
        setS(hFxChorusDepth,   std::clamp(invSq(p.chorusDepth, 0.80f), 0, 100));
        setS(hFxChorusMix,     std::clamp(invSq(p.chorusMix,   0.60f), 0, 100));
        setS(hFxDelayTime,     std::clamp((int)std::round(p.delayTime), 10, 1500));
        setS(hFxDelayFb,       std::clamp(invSq(p.delayFb,  0.88f), 0, 100));
        setS(hFxDelayMix,      std::clamp(invSq(p.delayMix, 0.60f), 0, 100));
        setS(hFxReverbSize,    std::clamp(invSq(p.reverbSize, 0.92f), 0, 100));
        setS(hFxReverbDamp,    std::clamp((int)std::round(p.reverbDamp * 100.0f), 0, 100));
        setS(hFxReverbMix,     std::clamp(invSq(p.reverbMix, 0.65f), 0, 100));
        setS(hFxNoiseLevel,    std::clamp((int)std::round(p.noiseLevel * 100.0f), 0, 100));
        setS(hFxNoiseFlutter,  std::clamp((int)std::round(p.noiseFlutter * 100.0f), 0, 100));
        setS(hFxNoiseTone,     std::clamp((int)std::round(p.noiseTone * 100.0f), 0, 100));'''
new = '''    void refreshFxFromParams(int style) {
        if (!plugin || style < 0 || style >= 6) return;
        const auto& p = plugin->fxParamsPerStyle[style];
        // Invert the x^2*max curve: knob = round(sqrt(v/max)*100)
        auto invSq = [](float v, float maxV) -> int {
            if (maxV <= 0.0f) return 0;
            return (int)std::round(std::sqrt(std::clamp(v / maxV, 0.0f, 1.0f)) * 100.0f);
        };
        kwSetPos(hFxChorusRate,    std::clamp((int)std::round(p.chorusRate / 0.01f), 5, 600));
        kwSetPos(hFxChorusDepth,   std::clamp(invSq(p.chorusDepth, 0.80f), 0, 100));
        kwSetPos(hFxChorusMix,     std::clamp(invSq(p.chorusMix,   0.60f), 0, 100));
        kwSetPos(hFxDelayTime,     std::clamp((int)std::round(p.delayTime), 10, 1500));
        kwSetPos(hFxDelayFb,       std::clamp(invSq(p.delayFb,  0.88f), 0, 100));
        kwSetPos(hFxDelayMix,      std::clamp(invSq(p.delayMix, 0.60f), 0, 100));
        kwSetPos(hFxReverbSize,    std::clamp(invSq(p.reverbSize, 0.92f), 0, 100));
        kwSetPos(hFxReverbDamp,    std::clamp((int)std::round(p.reverbDamp * 100.0f), 0, 100));
        kwSetPos(hFxReverbMix,     std::clamp(invSq(p.reverbMix, 0.65f), 0, 100));
        kwSetPos(hFxNoiseLevel,    std::clamp((int)std::round(p.noiseLevel * 100.0f), 0, 100));
        kwSetPos(hFxNoiseFlutter,  std::clamp((int)std::round(p.noiseFlutter * 100.0f), 0, 100));
        kwSetPos(hFxNoiseTone,     std::clamp((int)std::round(p.noiseTone * 100.0f), 0, 100));'''
assert old in text, "refreshFxFromParams not found"
text = text.replace(old, new, 1)

# ─────────────────────────────────────────────────────────────────────────────
# 11. Update refreshMakerVals: use kwGetPos for ADSR+Timbre knobs
# ─────────────────────────────────────────────────────────────────────────────
old = '''        auto getPos = [&](HWND s) -> int {
            return s ? (int)SendMessageW(s, TBM_GETPOS, 0, 0) : 0;
        };
        auto setLbl = [&](HWND lbl, const wchar_t* txt) {
            if (lbl) SetWindowTextW(lbl, txt);
        };
        wchar_t buf[64];
        swprintf(buf, 64, L"%.2f s", getPos(hMakerAttackSlider) * 0.01f);
        setLbl(hMakerAttackVal, buf);
        swprintf(buf, 64, L"%.2f s", getPos(hMakerDecaySlider) * 0.01f);
        setLbl(hMakerDecayVal, buf);
        swprintf(buf, 64, L"%.0f%%", getPos(hMakerSustainSlider) * 1.0f);
        setLbl(hMakerSustainVal, buf);
        swprintf(buf, 64, L"%.2f s", getPos(hMakerReleaseSlider) * 0.01f);
        setLbl(hMakerReleaseVal, buf);
        swprintf(buf, 64, L"%.1f", getPos(hMakerModIndexSlider) * 0.1f);
        setLbl(hMakerModIndexVal, buf);
        swprintf(buf, 64, L"%.1f", getPos(hMakerModDecaySlider) * 0.1f);
        setLbl(hMakerModDecayVal, buf);
        swprintf(buf, 64, L"%.0f%%", getPos(hMakerGainSlider) * 1.0f);
        setLbl(hMakerGainVal, buf);'''
new = '''        auto setLbl = [&](HWND lbl, const wchar_t* txt) {
            if (lbl) SetWindowTextW(lbl, txt);
        };
        wchar_t buf[64];
        swprintf(buf, 64, L"%.2f s", kwGetPos(hMakerAttackSlider) * 0.01f);
        setLbl(hMakerAttackVal, buf);
        swprintf(buf, 64, L"%.2f s", kwGetPos(hMakerDecaySlider) * 0.01f);
        setLbl(hMakerDecayVal, buf);
        swprintf(buf, 64, L"%.0f%%", kwGetPos(hMakerSustainSlider) * 1.0f);
        setLbl(hMakerSustainVal, buf);
        swprintf(buf, 64, L"%.2f s", kwGetPos(hMakerReleaseSlider) * 0.01f);
        setLbl(hMakerReleaseVal, buf);
        swprintf(buf, 64, L"%.1f", kwGetPos(hMakerModIndexSlider) * 0.1f);
        setLbl(hMakerModIndexVal, buf);
        swprintf(buf, 64, L"%.1f", kwGetPos(hMakerModDecaySlider) * 0.1f);
        setLbl(hMakerModDecayVal, buf);
        swprintf(buf, 64, L"%.0f%%", kwGetPos(hMakerGainSlider) * 1.0f);
        setLbl(hMakerGainVal, buf);'''
assert old in text, "refreshMakerVals not found"
text = text.replace(old, new, 1)

# ─────────────────────────────────────────────────────────────────────────────
# 12. Update loadMakerCfgFromControls: use kwGetPos
# ─────────────────────────────────────────────────────────────────────────────
old = '''        auto getPos = [&](HWND s) -> int {
            return s ? (int)SendMessageW(s, TBM_GETPOS, 0, 0) : 0;
        };
        sfm::SfmConfig& cfg = makerCfg[style];'''
new = '''        sfm::SfmConfig& cfg = makerCfg[style];'''
assert old in text, "loadMakerCfgFromControls getPos not found"
text = text.replace(old, new, 1)

old = '''        cfg.attack   = std::max(0.001f, getPos(hMakerAttackSlider)   * 0.01f);
        cfg.decay    = std::max(0.001f, getPos(hMakerDecaySlider)    * 0.01f);
        cfg.sustain  = getPos(hMakerSustainSlider) * 0.01f;
        cfg.release  = std::max(0.001f, getPos(hMakerReleaseSlider)  * 0.01f);
        cfg.modIndex = getPos(hMakerModIndexSlider) * 0.1f;
        cfg.modDecay = getPos(hMakerModDecaySlider) * 0.1f;
        cfg.gain     = std::max(0.01f, getPos(hMakerGainSlider) * 0.01f);'''
new = '''        cfg.attack   = std::max(0.001f, kwGetPos(hMakerAttackSlider)   * 0.01f);
        cfg.decay    = std::max(0.001f, kwGetPos(hMakerDecaySlider)    * 0.01f);
        cfg.sustain  = kwGetPos(hMakerSustainSlider) * 0.01f;
        cfg.release  = std::max(0.001f, kwGetPos(hMakerReleaseSlider)  * 0.01f);
        cfg.modIndex = kwGetPos(hMakerModIndexSlider) * 0.1f;
        cfg.modDecay = kwGetPos(hMakerModDecaySlider) * 0.1f;
        cfg.gain     = std::max(0.01f, kwGetPos(hMakerGainSlider) * 0.01f);'''
assert old in text, "loadMakerCfgFromControls body not found"
text = text.replace(old, new, 1)

# ─────────────────────────────────────────────────────────────────────────────
# 13. Update refreshMakerFromCfg: use kwSetRange / kwSetPos
# ─────────────────────────────────────────────────────────────────────────────
old = '''        auto setRange = [&](HWND s, int lo, int hi) {
            if (!s) return;
            SendMessageW(s, TBM_SETRANGEMIN, FALSE, lo);
            SendMessageW(s, TBM_SETRANGEMAX, TRUE,  hi);
        };
        setRange(hMakerAttackSlider,   d.atkMin, d.atkMax);
        setRange(hMakerDecaySlider,    d.decMin, d.decMax);
        setRange(hMakerSustainSlider,  d.susMin, d.susMax);
        setRange(hMakerReleaseSlider,  d.relMin, d.relMax);
        setRange(hMakerModIndexSlider, d.miMin,  d.miMax);
        setRange(hMakerModDecaySlider, d.mdMin,  d.mdMax);
        setRange(hMakerGainSlider,     d.gaMin,  d.gaMax);
        auto setS = [&](HWND s, int v) { if (s) SendMessageW(s, TBM_SETPOS, TRUE, v); };
        setS(hMakerAttackSlider,   std::clamp((int)std::round(cfg.attack   / 0.01f), d.atkMin, d.atkMax));
        setS(hMakerDecaySlider,    std::clamp((int)std::round(cfg.decay    / 0.01f), d.decMin, d.decMax));
        setS(hMakerSustainSlider,  std::clamp((int)std::round(cfg.sustain  / 0.01f), d.susMin, d.susMax));
        setS(hMakerReleaseSlider,  std::clamp((int)std::round(cfg.release  / 0.01f), d.relMin, d.relMax));
        setS(hMakerModIndexSlider, std::clamp((int)std::round(cfg.modIndex / 0.1f),  d.miMin,  d.miMax));
        setS(hMakerModDecaySlider, std::clamp((int)std::round(cfg.modDecay / 0.1f),  d.mdMin,  d.mdMax));
        setS(hMakerGainSlider,     std::clamp((int)std::round(cfg.gain     / 0.01f), d.gaMin,  d.gaMax));'''
new = '''        kwSetRange(hMakerAttackSlider,   d.atkMin, d.atkMax);
        kwSetRange(hMakerDecaySlider,    d.decMin, d.decMax);
        kwSetRange(hMakerSustainSlider,  d.susMin, d.susMax);
        kwSetRange(hMakerReleaseSlider,  d.relMin, d.relMax);
        kwSetRange(hMakerModIndexSlider, d.miMin,  d.miMax);
        kwSetRange(hMakerModDecaySlider, d.mdMin,  d.mdMax);
        kwSetRange(hMakerGainSlider,     d.gaMin,  d.gaMax);
        kwSetPos(hMakerAttackSlider,   std::clamp((int)std::round(cfg.attack   / 0.01f), d.atkMin, d.atkMax));
        kwSetPos(hMakerDecaySlider,    std::clamp((int)std::round(cfg.decay    / 0.01f), d.decMin, d.decMax));
        kwSetPos(hMakerSustainSlider,  std::clamp((int)std::round(cfg.sustain  / 0.01f), d.susMin, d.susMax));
        kwSetPos(hMakerReleaseSlider,  std::clamp((int)std::round(cfg.release  / 0.01f), d.relMin, d.relMax));
        kwSetPos(hMakerModIndexSlider, std::clamp((int)std::round(cfg.modIndex / 0.1f),  d.miMin,  d.miMax));
        kwSetPos(hMakerModDecaySlider, std::clamp((int)std::round(cfg.modDecay / 0.1f),  d.mdMin,  d.mdMax));
        kwSetPos(hMakerGainSlider,     std::clamp((int)std::round(cfg.gain     / 0.01f), d.gaMin,  d.gaMax));'''
assert old in text, "refreshMakerFromCfg setRange/setS not found"
text = text.replace(old, new, 1)

# ─────────────────────────────────────────────────────────────────────────────
# 14. New FX layout: 3 knobs per FX horizontal, title above
# ─────────────────────────────────────────────────────────────────────────────
old = '''            // Layout: 2 columns, 2 FX each.
            //   Left:  Chorus (col 0) + Delay (col 0)
            //   Right: Reverb (col 1) + CassetteNoise (col 1)
            const int colW  = (kViewWidth - 2 * kPadX - 20) / 2; // ~420 px each
            const int col0X = kPadX;
            const int col1X = kPadX + colW + 20;
            const int lblW  = 120;
            const int sldW  = 200;
            const int valW  = 66;
            const int rowH  = 30;
            const int secH  = 24; // section title row height
            const int togW  = 90;

            auto placeRow = [&](int colX, int y, HWND sld, HWND val) {
                if (sld) { MoveWindow(sld, colX + lblW + 4, y + 4, sldW, 22, TRUE); ShowWindow(sld, SW_SHOW); }
                if (val) { MoveWindow(val, colX + lblW + 4 + sldW + 4, y + 6, valW, 18, TRUE); ShowWindow(val, SW_SHOW); }
            };
            auto placeTog = [&](int colX, int y, HWND tog) {
                if (tog) { MoveWindow(tog, colX + lblW + 4, y, togW, 24, TRUE); ShowWindow(tog, SW_SHOW); }
            };

            int startY = kHeaderH + kTabsH + 14;

            // ---- CHORUS ----
            fxChorusY = startY;
            int y = startY + secH;
            fxRowY[0] = y; placeRow(col0X, y, hFxChorusRate,  hFxChorusRateVal);  y += rowH;
            fxRowY[1] = y; placeRow(col0X, y, hFxChorusDepth, hFxChorusDepthVal); y += rowH;
            fxRowY[2] = y; placeRow(col0X, y, hFxChorusMix,   hFxChorusMixVal);   y += rowH;
            placeTog(col0X, fxChorusY + 2, hFxChorusOn);

            // ---- DELAY ----
            y += 10;
            fxDelayY = y;
            y += secH;
            fxRowY[3] = y; placeRow(col0X, y, hFxDelayTime, hFxDelayTimeVal); y += rowH;
            fxRowY[4] = y; placeRow(col0X, y, hFxDelayFb,   hFxDelayFbVal);   y += rowH;
            fxRowY[5] = y; placeRow(col0X, y, hFxDelayMix,  hFxDelayMixVal);  y += rowH;
            placeTog(col0X, fxDelayY + 2, hFxDelayOn);

            // ---- REVERB (right column) ----
            fxReverbY = startY;
            y = startY + secH;
            fxRowY[6] = y; placeRow(col1X, y, hFxReverbSize, hFxReverbSizeVal); y += rowH;
            fxRowY[7] = y; placeRow(col1X, y, hFxReverbDamp, hFxReverbDampVal); y += rowH;
            fxRowY[8] = y; placeRow(col1X, y, hFxReverbMix,  hFxReverbMixVal);  y += rowH;
            placeTog(col1X, fxReverbY + 2, hFxReverbOn);

            // ---- CASSETTE NOISE (right column) ----
            y += 10;
            fxNoiseY = y;
            y += secH;
            fxRowY[9]  = y; placeRow(col1X, y, hFxNoiseLevel,   hFxNoiseLevelVal);   y += rowH;
            fxRowY[10] = y; placeRow(col1X, y, hFxNoiseFlutter, hFxNoiseFlutterVal); y += rowH;
            fxRowY[11] = y; placeRow(col1X, y, hFxNoiseTone,    hFxNoiseToneVal);    y += rowH;
            placeTog(col1X, fxNoiseY + 2, hFxNoiseOn);'''
new = '''            // Layout: 2 columns (Chorus+Delay left, Reverb+Noise right)
            // Each FX block: title+toggle row, then 3 knobs horizontal, then val labels
            const int colW  = (kViewWidth - 2 * kPadX - 20) / 2; // ~422 px each
            const int col0X = kPadX;
            const int col1X = kPadX + colW + 20;
            const int togW  = 90;
            const int secH  = 28; // title row height
            // 3 knobs per FX, evenly spaced in column
            const int knobGapFx = (colW - 3 * kKnobSize) / 4; // ~(422-168)/4=63
            const int fxKnobH   = kKnobSize;                   // 56
            const int fxValH    = 18;
            const int fxSecGap  = 18; // gap between two FX in same column

            // Helper: place 3 knobs + val labels in a column, starting at knobY
            auto placeFxKnobs = [&](int colX, int knobY,
                HWND k0, HWND v0, HWND k1, HWND v1, HWND k2, HWND v2) {
                int kx = colX + knobGapFx;
                int ky = knobY;
                int vy = ky + fxKnobH + 2;
                if (k0) { MoveWindow(k0, kx, ky, kKnobSize, kKnobSize, TRUE); ShowWindow(k0, SW_SHOW); }
                if (v0) { MoveWindow(v0, kx, vy, kKnobSize, fxValH, TRUE);    ShowWindow(v0, SW_SHOW); }
                kx += kKnobSize + knobGapFx;
                if (k1) { MoveWindow(k1, kx, ky, kKnobSize, kKnobSize, TRUE); ShowWindow(k1, SW_SHOW); }
                if (v1) { MoveWindow(v1, kx, vy, kKnobSize, fxValH, TRUE);    ShowWindow(v1, SW_SHOW); }
                kx += kKnobSize + knobGapFx;
                if (k2) { MoveWindow(k2, kx, ky, kKnobSize, kKnobSize, TRUE); ShowWindow(k2, SW_SHOW); }
                if (v2) { MoveWindow(v2, kx, vy, kKnobSize, fxValH, TRUE);    ShowWindow(v2, SW_SHOW); }
            };
            auto placeTog = [&](int colX, int y, HWND tog) {
                if (tog) { MoveWindow(tog, colX, y + 2, togW, 24, TRUE); ShowWindow(tog, SW_SHOW); }
            };

            const int startY = kHeaderH + kTabsH + 14;
            // Height of one FX block: secH + kKnobSize + fxValH + margin
            const int fxBlockH = secH + fxKnobH + fxValH + 4;

            // ---- CHORUS (col 0) ----
            fxChorusY = startY;
            placeTog(col0X, fxChorusY, hFxChorusOn);
            {
                int knobY = fxChorusY + secH;
                fxRowY[0] = knobY; fxRowY[1] = knobY; fxRowY[2] = knobY; // same row for paint
                placeFxKnobs(col0X, knobY,
                    hFxChorusRate, hFxChorusRateVal,
                    hFxChorusDepth, hFxChorusDepthVal,
                    hFxChorusMix, hFxChorusMixVal);
            }

            // ---- DELAY (col 0, below Chorus) ----
            fxDelayY = startY + fxBlockH + fxSecGap;
            placeTog(col0X, fxDelayY, hFxDelayOn);
            {
                int knobY = fxDelayY + secH;
                fxRowY[3] = knobY; fxRowY[4] = knobY; fxRowY[5] = knobY;
                placeFxKnobs(col0X, knobY,
                    hFxDelayTime, hFxDelayTimeVal,
                    hFxDelayFb,   hFxDelayFbVal,
                    hFxDelayMix,  hFxDelayMixVal);
            }

            // ---- REVERB (col 1) ----
            fxReverbY = startY;
            placeTog(col1X, fxReverbY, hFxReverbOn);
            {
                int knobY = fxReverbY + secH;
                fxRowY[6] = knobY; fxRowY[7] = knobY; fxRowY[8] = knobY;
                placeFxKnobs(col1X, knobY,
                    hFxReverbSize, hFxReverbSizeVal,
                    hFxReverbDamp, hFxReverbDampVal,
                    hFxReverbMix,  hFxReverbMixVal);
            }

            // ---- CASSETTE NOISE (col 1, below Reverb) ----
            fxNoiseY = startY + fxBlockH + fxSecGap;
            placeTog(col1X, fxNoiseY, hFxNoiseOn);
            {
                int knobY = fxNoiseY + secH;
                fxRowY[9] = knobY; fxRowY[10] = knobY; fxRowY[11] = knobY;
                placeFxKnobs(col1X, knobY,
                    hFxNoiseLevel,   hFxNoiseLevelVal,
                    hFxNoiseFlutter, hFxNoiseFlutterVal,
                    hFxNoiseTone,    hFxNoiseToneVal);
            }'''
assert old in text, "FX layout block not found"
text = text.replace(old, new, 1)

# ─────────────────────────────────────────────────────────────────────────────
# 15. New Maker Envelope + Timbre layout: side-by-side knob rows
# ─────────────────────────────────────────────────────────────────────────────
old = '''            // ---- ENVELOPPE section ----
            mkEnvY = y;
            y += 22;

            auto placeSliderRow = [&](HWND sld, HWND val, int yRow) {
                if (sld) { MoveWindow(sld, mkX + mkLblW + 8, yRow + 3, mkSldW, 22, TRUE); ShowWindow(sld, SW_SHOW); }
                if (val) { MoveWindow(val, mkX + mkLblW + 8 + mkSldW + 8, yRow + 5, mkValW, 18, TRUE); ShowWindow(val, SW_SHOW); }
            };
            mkRowY[2] = y; placeSliderRow(hMakerAttackSlider,   hMakerAttackVal,   y); y += mkRowH;
            mkRowY[3] = y; placeSliderRow(hMakerDecaySlider,    hMakerDecayVal,    y); y += mkRowH;
            mkRowY[4] = y; placeSliderRow(hMakerSustainSlider,  hMakerSustainVal,  y); y += mkRowH;
            mkRowY[5] = y; placeSliderRow(hMakerReleaseSlider,  hMakerReleaseVal,  y); y += mkRowH + 6;

            // ---- TIMBRE section ----
            mkTimbreY = y;
            y += 22;
            mkRowY[6] = y; placeSliderRow(hMakerModIndexSlider, hMakerModIndexVal, y); y += mkRowH;
            mkRowY[7] = y; placeSliderRow(hMakerModDecaySlider, hMakerModDecayVal, y); y += mkRowH;
            mkRowY[8] = y; placeSliderRow(hMakerGainSlider,     hMakerGainVal,     y); y += mkRowH + 6;'''
new = '''            // ---- ENVELOPPE + TIMBRE side-by-side knob sections ----
            mkEnvY    = y;
            mkTimbreY = y; // same title row, split left/right
            y += 24;       // title row height

            // Geometry: left half = Enveloppe (4 knobs), right half = Timbre (3 knobs)
            const int halfW   = kViewWidth / 2 - kPadX; // ~432px per half
            const int halfMid = kViewWidth / 2;           // 450

            // 4 evenly-spaced knobs in left half
            const int envGap  = (halfW - 4 * kKnobSize) / 5;
            // 3 evenly-spaced knobs in right half
            const int tmbGap  = (halfW - 3 * kKnobSize) / 4;

            // Knob Y: same for both sections
            int knobY = y;
            int valLY = knobY + kKnobSize + 2;

            // Place Enveloppe knobs
            {
                int kx = kPadX + envGap;
                auto placeKnob = [&](HWND k, HWND v) {
                    if (k) { MoveWindow(k, kx, knobY, kKnobSize, kKnobSize, TRUE); ShowWindow(k, SW_SHOW); }
                    if (v) { MoveWindow(v, kx, valLY, kKnobSize, 16, TRUE);        ShowWindow(v, SW_SHOW); }
                    kx += kKnobSize + envGap;
                };
                mkRowY[2] = knobY; placeKnob(hMakerAttackSlider,  hMakerAttackVal);
                mkRowY[3] = knobY; placeKnob(hMakerDecaySlider,   hMakerDecayVal);
                mkRowY[4] = knobY; placeKnob(hMakerSustainSlider, hMakerSustainVal);
                mkRowY[5] = knobY; placeKnob(hMakerReleaseSlider, hMakerReleaseVal);
            }

            // Place Timbre knobs (right half)
            {
                int kx = halfMid + tmbGap;
                auto placeKnob = [&](HWND k, HWND v) {
                    if (k) { MoveWindow(k, kx, knobY, kKnobSize, kKnobSize, TRUE); ShowWindow(k, SW_SHOW); }
                    if (v) { MoveWindow(v, kx, valLY, kKnobSize, 16, TRUE);        ShowWindow(v, SW_SHOW); }
                    kx += kKnobSize + tmbGap;
                };
                mkRowY[6] = knobY; placeKnob(hMakerModIndexSlider, hMakerModIndexVal);
                mkRowY[7] = knobY; placeKnob(hMakerModDecaySlider, hMakerModDecayVal);
                mkRowY[8] = knobY; placeKnob(hMakerGainSlider,     hMakerGainVal);
            }

            y = valLY + 18 + 10; // advance past knobs + val labels + gap'''
assert old in text, "Maker Envelope+Timbre layout not found"
text = text.replace(old, new, 1)

# ─────────────────────────────────────────────────────────────────────────────
# 16. Update WM_PAINT for FX panel: draw section headers + knob param names
# ─────────────────────────────────────────────────────────────────────────────
old = '''        // ---- FX panel (viewMode == 3) ----
        if (viewMode == 3) {
            SetBkMode(hdc, TRANSPARENT);
            const int colW  = (kViewWidth - 2 * kPadX - 20) / 2;
            const int col0X = kPadX;
            const int col1X = kPadX + colW + 20;
            const int lblW  = 120;
            const int togW  = 90;

            // Section headers
            static const wchar_t* kFxSec[] = { L"CHORUS", L"D\\u00c9LAI", L"R\\u00c9VERB\\u00c9RATION", L"BRUIT CASSETTE" };
            const int kFxY[]  = { fxChorusY,   fxDelayY,   fxReverbY,   fxNoiseY  };
            const int kFxCX[] = { col0X,        col0X,       col1X,        col1X    };
            SelectObject(hdc, fontSection);
            HPEN penFx = CreatePen(PS_SOLID, 1, kColBorder);
            HPEN penFxOld = (HPEN)SelectObject(hdc, penFx);
            for (int i = 0; i < 4; ++i) {
                int sy = kFxY[i]; int cx = kFxCX[i];
                if (sy <= 0) continue;
                SetTextColor(hdc, kColAccent);
                RECT rcS = { cx + togW + 8, sy, cx + colW, sy + 20 };
                DrawTextW(hdc, kFxSec[i], -1, &rcS, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                MoveToEx(hdc, cx + togW + 8 + 100, sy + 11, nullptr);
                LineTo(hdc, cx + colW, sy + 11);
            }
            SelectObject(hdc, penFxOld); DeleteObject(penFx);

            // Row labels per column
            static const wchar_t* kFxLabC0[] = {
                L"Vitesse :", L"Profondeur :", L"Mixage :",     // Chorus
                L"Temps :",   L"R\\u00e9injection :", L"Mixage :"    // Delay
            };
            static const wchar_t* kFxLabC1[] = {
                L"Taille :", L"Amortissement :", L"Mixage :",  // Reverb
                L"Niveau :", L"Flutter :", L"Couleur :"        // Noise
            };
            SelectObject(hdc, fontReg);
            SetTextColor(hdc, kColText);
            for (int i = 0; i < 6; ++i) {
                int ry = fxRowY[i]; if (ry <= 0) continue;
                RECT rcL = { col0X, ry, col0X + lblW, ry + 24 };
                DrawTextW(hdc, kFxLabC0[i], -1, &rcL, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
            }
            for (int i = 0; i < 6; ++i) {
                int ry = fxRowY[i + 6]; if (ry <= 0) continue;
                RECT rcL = { col1X, ry, col1X + lblW, ry + 24 };
                DrawTextW(hdc, kFxLabC1[i], -1, &rcL, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
            }
        }'''
# We need to handle the unicode escape sequences as actual chars in the source
import codecs

# Read the actual characters in the file
old_fx_paint = '        // ---- FX panel (viewMode == 3) ----\n        if (viewMode == 3) {'
# Find the block boundaries
start_idx = text.find('        // ---- FX panel (viewMode == 3) ----\n        if (viewMode == 3) {')
end_marker = '        SelectObject(hdc, old);\n    }\n\n\n    // Paint the 5-tab instrument selector'
end_idx = text.find(end_marker, start_idx)

if start_idx < 0:
    print("ERROR: FX paint start not found")
else:
    # Find the closing brace of the if block (before "SelectObject(hdc, old)")
    # The FX paint block ends with "        }" before "        SelectObject(hdc, old);"
    block_end = text.rfind('\n        }', start_idx, end_idx) + len('\n        }')
    old_fx_block = text[start_idx:block_end]
    
    new_fx_paint_block = '''        // ---- FX panel (viewMode == 3) ----
        if (viewMode == 3) {
            SetBkMode(hdc, TRANSPARENT);
            const int colW    = (kViewWidth - 2 * kPadX - 20) / 2;
            const int col0X   = kPadX;
            const int col1X   = kPadX + colW + 20;
            const int togW    = 90;
            const int secH    = 28;
            const int knobGapFx = (colW - 3 * kKnobSize) / 4;
            const int fxBlockH  = secH + kKnobSize + 18 + 4;
            const int fxSecGap  = 18;

            // Section names + param names per FX
            static const wchar_t* kFxSec[] = {
                L"CHORUS", L"D\u00c9LAI", L"R\u00c9VERB\u00c9RATION", L"BRUIT CASSETTE"
            };
            // 3 param names per FX: [FX][param]
            static const wchar_t* kFxPar[4][3] = {
                { L"Vitesse",    L"Profondeur", L"Mixage"    },  // Chorus
                { L"Temps",      L"R\u00e9inject.", L"Mixage" },  // Delay
                { L"Taille",     L"Amort.",     L"Mixage"    },  // Reverb
                { L"Niveau",     L"Flutter",    L"Couleur"   },  // Noise
            };
            const int kFxSY[4] = { fxChorusY, fxDelayY, fxReverbY, fxNoiseY };
            const int kFxCX[4] = { col0X, col0X, col1X, col1X };

            SelectObject(hdc, fontSection);
            HPEN penFx = CreatePen(PS_SOLID, 1, kColBorder);
            HPEN penFxOld = (HPEN)SelectObject(hdc, penFx);

            for (int fi = 0; fi < 4; ++fi) {
                int sy = kFxSY[fi]; int cx = kFxCX[fi];
                if (sy <= 0) continue;

                // Section title (to the right of toggle)
                SetTextColor(hdc, kColAccent);
                RECT rcS = { cx + togW + 6, sy, cx + colW, sy + secH };
                DrawTextW(hdc, kFxSec[fi], -1, &rcS, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                // Separator line
                int lineX0 = cx + togW + 6 + 90;
                MoveToEx(hdc, lineX0, sy + secH/2, nullptr);
                LineTo(hdc, cx + colW, sy + secH/2);

                // Knob param labels (drawn above each knob)
                SelectObject(hdc, fontReg);
                SetTextColor(hdc, kColText);
                int knobY = sy + secH;
                int kx = cx + knobGapFx;
                for (int pi = 0; pi < 3; ++pi) {
                    RECT rcP = { kx, knobY - 16, kx + kKnobSize, knobY };
                    DrawTextW(hdc, kFxPar[fi][pi], -1, &rcP, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                    kx += kKnobSize + knobGapFx;
                }
                SelectObject(hdc, fontSection);
            }
            SelectObject(hdc, penFxOld); DeleteObject(penFx);
        }'''
    
    text = text[:start_idx] + new_fx_paint_block + text[block_end:]
    print("FX WM_PAINT block replaced successfully")

# ─────────────────────────────────────────────────────────────────────────────
# 17. Update WM_PAINT for Maker: replace slider row labels with knob labels
# for the Envelope/Timbre sections (rows 2-8)
# ─────────────────────────────────────────────────────────────────────────────
old_mk_labels = '''            // Row labels (drawn here since STATIC controls can't theme easily)
            const MkStyleDef& mkDef = kMkStyle[mkStyle];
            const wchar_t* kMkRowLabels[] = {
                L"Nom du preset :",
                L"Type de synth\\u00e8se :",
                L"Attaque :",
                L"D\\u00e9clin :",
                L"Maintien :",
                L"Rel\\u00e2chement :",
                mkDef.miLabel,
                mkDef.mdLabel,
                L"Gain :",
                nullptr, // Tessiture row uses separate combos sub-labels
            };
            SelectObject(hdc, fontReg);
            SetTextColor(hdc, kColText);
            for (int i = 0; i < 9; ++i) {
                int ry = mkRowY[i];
                if (ry <= 0) continue;
                RECT rcL = { kPadX, ry, kPadX + 150, ry + 24 };'''

# Find that block and the closing brace
idx_mk = text.find('            // Row labels (drawn here since STATIC controls can\'t theme easily)')
if idx_mk < 0:
    print("ERROR: Maker row labels section not found, trying alternate...")
    # Try with unicode escaped
    idx_mk = text.find('            // Row labels (drawn here since STATIC controls can\u2019t theme easily)')
    
print(f"Maker row labels found at char {idx_mk}")

# Let's find the exact block to replace - look for the section
search_start = '            // Row labels (drawn here since STATIC controls can\'t theme easily)'
idx2 = text.find(search_start)
print(f"Found Maker label block: {idx2 >= 0}")

# ─────────────────────────────────────────────────────────────────────────────
# 17 (alt): Find and replace the Maker WM_PAINT row labels section
# ─────────────────────────────────────────────────────────────────────────────
# The block goes from "// Row labels" to the delta labels section
mk_labels_start = '            // Row labels (drawn here since STATIC controls can\'t theme easily)'
delta_section = '            // Delta slider labels'
idx_mk_labels = text.find(mk_labels_start)
idx_delta = text.find(delta_section, idx_mk_labels)
print(f"mk_labels start: {idx_mk_labels}, delta section: {idx_delta}")

if idx_mk_labels >= 0 and idx_delta >= 0:
    old_mk_block = text[idx_mk_labels:idx_delta]
    
    new_mk_block = '''            // Row labels for non-knob rows (Nom, Synthèse)
            const MkStyleDef& mkDef = kMkStyle[mkStyle];
            SelectObject(hdc, fontReg);
            SetTextColor(hdc, kColText);
            // Row 0: Nom du preset
            if (mkRowY[0] > 0) {
                RECT rcL = { kPadX, mkRowY[0], kPadX + 150, mkRowY[0] + 24 };
                DrawTextW(hdc, L"Nom du preset :", -1, &rcL, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
            }
            // Row 1: Type de synthèse
            if (mkRowY[1] > 0) {
                RECT rcL = { kPadX, mkRowY[1], kPadX + 150, mkRowY[1] + 24 };
                DrawTextW(hdc, L"Type de synth\u00e8se :", -1, &rcL, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
            }

            // Knob param names for Enveloppe (above each knob) + Timbre
            if (mkEnvY > 0) {
                const int halfW    = kViewWidth / 2 - kPadX;
                const int halfMid  = kViewWidth / 2;
                const int envGap   = (halfW - 4 * kKnobSize) / 5;
                const int tmbGap   = (halfW - 3 * kKnobSize) / 4;
                const int knobY    = mkEnvY + 24;
                const int labelY   = knobY - 17;

                // Enveloppe: Attaque / Déclin / Maintien / Relâchement
                static const wchar_t* kEnvLabels[4] = {
                    L"Attaque", L"D\u00e9clin", L"Maintien", L"Rel\u00e2ch."
                };
                int kx = kPadX + envGap;
                for (int i = 0; i < 4; ++i) {
                    RECT rcP = { kx, labelY, kx + kKnobSize, knobY };
                    DrawTextW(hdc, kEnvLabels[i], -1, &rcP, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                    kx += kKnobSize + envGap;
                }

                // Timbre: miLabel / mdLabel / Gain
                const wchar_t* kTmbLabels[3] = { mkDef.miLabel, mkDef.mdLabel, L"Gain" };
                // Strip trailing ":" if present
                wchar_t tl0[32], tl1[32], tl2[8] = L"Gain";
                auto stripColon = [](const wchar_t* src, wchar_t* dst, int dsz) {
                    wcsncpy_s(dst, dsz, src, _TRUNCATE);
                    int n = (int)wcslen(dst);
                    while (n > 0 && (dst[n-1] == L':' || dst[n-1] == L' ')) dst[--n] = 0;
                };
                stripColon(mkDef.miLabel, tl0, 32);
                stripColon(mkDef.mdLabel, tl1, 32);
                const wchar_t* kTmbLbl[3] = { tl0, tl1, tl2 };
                kx = halfMid + tmbGap;
                for (int i = 0; i < 3; ++i) {
                    RECT rcP = { kx, labelY, kx + kKnobSize, knobY };
                    DrawTextW(hdc, kTmbLbl[i], -1, &rcP, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                    kx += kKnobSize + tmbGap;
                }
            }

            '''
    text = text[:idx_mk_labels] + new_mk_block + text[idx_delta:]
    print("Maker WM_PAINT labels replaced successfully")
else:
    print("ERROR: Could not find Maker label block boundaries")

# ─────────────────────────────────────────────────────────────────────────────
# 18. Fix mkRowY indices that reference DrawTextW in old label drawing loop
# The old loop used: DrawTextW(hdc, kMkRowLabels[i], -1, &rcL, DT_RIGHT...)
# We've replaced that loop. But we still need Tessiture row (index 9) labels
# Those are drawn elsewhere (separate combos sub-labels section)
# Let's find and clean up the old Tessiture label drawing if it references kMkRowLabels
# ─────────────────────────────────────────────────────────────────────────────
# Check for any remaining references to kMkRowLabels
if 'kMkRowLabels' in text:
    print("WARNING: kMkRowLabels still present in text, cleaning up...")
    # Find and remove/replace any remaining references
    idx_rem = text.find('kMkRowLabels')
    print(f"  Found at char: {idx_rem}")
    print(f"  Context: {repr(text[max(0,idx_rem-50):idx_rem+100])}")

with open(path, 'w', encoding='utf-8') as f:
    f.write(text)
print("File written successfully.")
