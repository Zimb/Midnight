#include "plugin_vst3.h"
#include "knob.h"

class MelodyMakerView : public IPlugView
{
public:
    explicit MelodyMakerView(MelodyMakerVST3* p) : plugin(p) {}

    DECLARE_FUNKNOWN_METHODS

    tresult PLUGIN_API isPlatformTypeSupported(FIDString type) override {
        return (std::strcmp(type, kPlatformTypeHWND) == 0) ? kResultTrue : kResultFalse;
    }

    tresult PLUGIN_API attached(void* parent, FIDString /*type*/) override {
        ensureWndClass();
        INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_BAR_CLASSES | ICC_STANDARD_CLASSES };
        InitCommonControlsEx(&icc);

        // Theme resources
        brBg = CreateSolidBrush(kColBg);
        auto mkFont = [](int height, int weight, const wchar_t* face) {
            LOGFONTW lf{}; lf.lfHeight = height; lf.lfWeight = weight;
            lf.lfQuality = CLEARTYPE_QUALITY;
            wcscpy_s(lf.lfFaceName, face);
            return CreateFontIndirectW(&lf);
        };
        fontReg     = mkFont(-12, FW_NORMAL,   L"Segoe UI");
        fontBold    = mkFont(-12, FW_SEMIBOLD, L"Segoe UI");
        fontTitle   = mkFont(-22, FW_BOLD,     L"Segoe UI");
        fontSection = mkFont(-11, FW_BOLD,     L"Segoe UI");
        fontTab     = mkFont(-13, FW_SEMIBOLD, L"Segoe UI");
        fontMono    = mkFont(-13, FW_NORMAL,   L"Consolas");

        KnobWidget::registerClass(g_hInst);

        hParent = (HWND)parent;
        hWnd = CreateWindowExW(
            0, kWndClass, L"",
            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
            0, 0, kViewWidth, kViewHeight,
            hParent, nullptr, g_hInst, nullptr);
        if (!hWnd) return kResultFalse;

        SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)this);
        createControls();
        refreshAll();
        SetTimer(hWnd, 1, 120, nullptr); // 120 ms refresh for note display
        return kResultOk;
    }

    tresult PLUGIN_API removed() override {
        if (hWnd) { KillTimer(hWnd, 1); DestroyWindow(hWnd); hWnd = nullptr; }
        if (brBg)        { DeleteObject(brBg);        brBg = nullptr; }
        if (fontReg)     { DeleteObject(fontReg);     fontReg = nullptr; }
        if (fontBold)    { DeleteObject(fontBold);    fontBold = nullptr; }
        if (fontTitle)   { DeleteObject(fontTitle);   fontTitle = nullptr; }
        if (fontSection) { DeleteObject(fontSection); fontSection = nullptr; }
        if (fontTab)     { DeleteObject(fontTab);     fontTab = nullptr; }
        if (fontMono)    { DeleteObject(fontMono);    fontMono = nullptr; }
        return kResultOk;
    }

    tresult PLUGIN_API onWheel(float) override { return kResultFalse; }
    tresult PLUGIN_API onKeyDown(char16, int16, int16) override { return kResultFalse; }
    tresult PLUGIN_API onKeyUp(char16, int16, int16) override { return kResultFalse; }

    tresult PLUGIN_API getSize(ViewRect* size) override {
        if (!size) return kInvalidArgument;
        size->left = 0; size->top = 0;
        size->right = kViewWidth; size->bottom = kViewHeight;
        return kResultOk;
    }
    tresult PLUGIN_API onSize(ViewRect*) override { return kResultOk; }
    tresult PLUGIN_API onFocus(TBool) override { return kResultOk; }
    tresult PLUGIN_API setFrame(IPlugFrame*) override { return kResultOk; }
    tresult PLUGIN_API canResize() override { return kResultFalse; }
    tresult PLUGIN_API checkSizeConstraint(ViewRect*) override { return kResultOk; }

private:
    static constexpr int kViewWidth      = 900;
    static constexpr int kViewHeight     = 640;  // compacted layout
    static constexpr int kPadX           = 18;
    static constexpr int kHeaderH        = 50;
    static constexpr int kTabsH          = 52;
    static constexpr int kSectionTitleH  = 24;
    static constexpr int kRowH           = 34;
    static constexpr int kSectionGap     = 14;
    static constexpr int kColumnGap      = 24;
    static constexpr int kColumnW        = (kViewWidth - 2 * kPadX - kColumnGap) / 2; // 420
    static constexpr int kRightColX      = kPadX + kColumnW + kColumnGap;             // 462
    static constexpr int kLabelW         = 78;
    static constexpr int kCtrlW          = kColumnW - kLabelW - 8 - 80 - 8;           // 246
    static constexpr int kValueW         = 80;
    static constexpr int kButtonH        = 38;
    static constexpr int kKnobSize       = 56;
    static constexpr int kTabCount       = 6;

    MelodyMakerVST3* plugin;
    HWND hParent = nullptr;
    HWND hWnd    = nullptr;
    HWND hCtrl[mm::kParamCount]   = {};
    HWND hValue[mm::kParamCount]  = {};
    HWND hLabel[mm::kParamCount]  = {};
    HWND hWaveLabel               = nullptr;
    HWND hProgLabel               = nullptr;
    HWND hNotesLabel              = nullptr;
    HWND hNoteDisplay             = nullptr;
    HWND hExportBtn               = nullptr;
    HWND hExportWavBtn            = nullptr; // Export rendered audio (WAV)
    HWND hSavePresetBtn           = nullptr; // Save preset (.mmp)
    HWND hLoadPresetBtn           = nullptr; // Load preset (.mmp)
    HWND hWaveCombo               = nullptr;
    HWND hProgCombo               = nullptr;
    HWND hAutoBtn                 = nullptr; // JAM toggle
    HWND hDiceBtn                 = nullptr; // Randomize seed
    HWND hPianoMelChk             = nullptr; // Piano: "MÃ©lodie" toggle
    HWND hPianoChordChk           = nullptr; // Piano: "Accords" toggle
    HWND hPercRhyLabel            = nullptr;
    HWND hPercRhyCombo            = nullptr; // Percussion rhythm picker
    HWND hVolLabel                = nullptr; // Volume label
    HWND hVolSlider               = nullptr; // Volume trackbar (0..200)
    HWND hVolValue                = nullptr; // Volume value text
    HWND hLockMode                = nullptr; // Padlock toggle for Mode
    HWND hLockProg                = nullptr; // Padlock toggle for Progr.
    HWND hLockSubdiv              = nullptr; // Padlock toggle for Subdiv
    HWND hMeterCombo              = nullptr; // Global time-signature combo
    HWND hMeterLabel               = nullptr; // Label for Mesure combo
    HWND hSectionLabel             = nullptr; // Label for Section combo
    HWND hHumanizeLabel           = nullptr; // Humanize label
    HWND hHumanizeKnob            = nullptr; // Humanize knob (0..1000)
    HWND hHumanizeValue           = nullptr; // Humanize value text
    HWND hRetardLabel             = nullptr; // Retard label
    HWND hRetardKnob              = nullptr; // Retard knob (0..1000)
    HWND hRetardValue             = nullptr; // Retard value text
    HWND hSectionCombo  = nullptr; // Global section (Intro/Verse/Chorus/Bridge/Outro)
    HWND hStartBarLabel = nullptr; // Per-style start-bar label
    HWND hStartBarCombo = nullptr; // Per-style start-bar combo
    HWND hFileBtn       = nullptr; // FILE dropdown toggle button
    bool fileMenuOpen   = false;   // Is the file dropdown panel visible?
    HWND hPianoRoll     = nullptr; // Miniature horizontal piano roll

    // Snapshot used by the piano roll painter (updated every timer tick)
    struct PRNote { double beatOn; double beatLen; int16_t pitch; float vel; uint8_t style; };
    std::vector<PRNote> prSnapshot; // GUI-thread only
    double              prCurrentBeat = 0.0; // beat position of the playhead
    static constexpr int kPRHeight     = 220; // px (params mode, Synthesia roll)
    static constexpr int kPRBeats      = 8;   // beats shown (scrolling window, ~2 bars)
    static constexpr int kPianoStripH  = 60;  // px — piano keyboard at bottom of roll

    // ---- View mode ----
    // 0 = MIDNIGHT (params), 1 = MELODY (piano roll), 2 = MAKER (SF maker)
    int  viewMode        = 0;
    bool paramsCollapsed = false;
    RECT titleRectMidnight = {};
    RECT titleRectMelody   = {};
    RECT titleRectMaker    = {};
    RECT titleRectFx       = {};
    RECT paramsTitleRect   = {}; // hit-rect for PARAMÈTRES toggle header

    // ---- Interactive edit state (works in both params + editor mode) ----
    bool   prFreezeSnapshot = false; // when true, timer won't overwrite prSnapshot
    std::vector<std::vector<PRNote>> undoStack;
    std::vector<std::vector<PRNote>> redoStack;
    static constexpr size_t kUndoLimit = 64;

    bool   prDragging      = false;
    int    prDragNoteIndex = -1;   // index into prSnapshot
    double prDragStartBeat = 0.0;
    int    prPressedKey     = -1;  // piano-strip key currently held (-1=none)
    DWORD  prPressedMs      = 0;   // GetTickCount() at press time
    int    prPressedNoteIdx = -1;  // index of held note in prSnapshot

    // Undo/redo buttons
    HWND hUndoBtn = nullptr;
    HWND hRedoBtn = nullptr;

    // ---- Maker (SoundFont Maker) controls ----
    HWND hMakerEnable         = nullptr; // toggle ● OUI / ○ NON
    HWND hMakerSynthCombo     = nullptr; // type de synthèse
    HWND hMakerNameEdit       = nullptr; // nom du preset
    HWND hMakerAttackSlider   = nullptr; HWND hMakerAttackVal   = nullptr;
    HWND hMakerDecaySlider    = nullptr; HWND hMakerDecayVal    = nullptr;
    HWND hMakerSustainSlider  = nullptr; HWND hMakerSustainVal  = nullptr;
    HWND hMakerReleaseSlider  = nullptr; HWND hMakerReleaseVal  = nullptr;
    HWND hMakerModIndexSlider = nullptr; HWND hMakerModIndexVal = nullptr;
    HWND hMakerModDecaySlider = nullptr; HWND hMakerModDecayVal = nullptr;
    HWND hMakerGainSlider     = nullptr; HWND hMakerGainVal     = nullptr;
    HWND hMakerLowNoteCombo   = nullptr;
    HWND hMakerHighNoteCombo  = nullptr;
    HWND hMakerStepCombo      = nullptr;
    HWND hMakerGenerateBtn    = nullptr;
    // Éditeur SF2 externe — boutons + sliders delta
    HWND hMakerLoadSf2Btn     = nullptr;
    HWND hMakerClearSf2Btn    = nullptr;
    HWND hMakerSf2Label       = nullptr; // nom du fichier chargé
    HWND hMakerDeltaTuneSlider= nullptr; HWND hMakerDeltaTuneVal = nullptr;
    HWND hMakerDeltaAtkSlider = nullptr; HWND hMakerDeltaAtkVal  = nullptr;
    HWND hMakerDeltaDecSlider = nullptr; HWND hMakerDeltaDecVal  = nullptr;
    HWND hMakerDeltaSusSlider = nullptr; HWND hMakerDeltaSusVal  = nullptr;
    HWND hMakerDeltaRelSlider = nullptr; HWND hMakerDeltaRelVal  = nullptr;
    HWND hMakerDeltaVolSlider = nullptr; HWND hMakerDeltaVolVal  = nullptr;
    HWND hMakerDeltaFiltSlider= nullptr; HWND hMakerDeltaFiltVal = nullptr;
    // Per-style working configuration for the Maker editor
    sfm::SfmConfig makerCfg[kTabCount]    = {};
    bool           makerEnabled[kTabCount] = {};
    bool           makerDirty[kTabCount]   = {}; // set when sliders change, cleared after refresh
    UINT_PTR       makerTimerId = 0;             // WM_TIMER id for auto-refresh
    // Y positions of Maker section dividers (set in applyVisibilityForStyle)
    int mkEnvY = 0, mkTimbreY = 0, mkTessY = 0;
    // Y positions of each Maker row (for label drawing in paintBackground)
    // 0=Nom, 1=Synth, 2=Attaque, 3=Déclin, 4=Maintien, 5=Relâchement,
    // 6=ProfondeurFM, 7=DéclinFM, 8=Gain, 9=Tessiture
    int mkRowY[10] = {};

    // ---- FX (effects) controls ----
    HWND hFxChorusOn = nullptr;
    HWND hFxChorusRate = nullptr,  hFxChorusRateVal = nullptr;
    HWND hFxChorusDepth = nullptr, hFxChorusDepthVal = nullptr;
    HWND hFxChorusMix = nullptr,   hFxChorusMixVal = nullptr;
    HWND hFxDelayOn = nullptr;
    HWND hFxDelayTime = nullptr,   hFxDelayTimeVal = nullptr;
    HWND hFxDelayFb = nullptr,     hFxDelayFbVal = nullptr;
    HWND hFxDelayMix = nullptr,    hFxDelayMixVal = nullptr;
    HWND hFxReverbOn = nullptr;
    HWND hFxReverbSize = nullptr,  hFxReverbSizeVal = nullptr;
    HWND hFxReverbDamp = nullptr,  hFxReverbDampVal = nullptr;
    HWND hFxReverbMix = nullptr,   hFxReverbMixVal = nullptr;
    HWND hFxNoiseOn = nullptr;
    HWND hFxNoiseLevel = nullptr,  hFxNoiseLevelVal = nullptr;
    HWND hFxNoiseFlutter = nullptr,hFxNoiseFlutterVal = nullptr;
    HWND hFxNoiseTone = nullptr,   hFxNoiseToneVal = nullptr;
    // Section Y positions for paintBackground (set in applyVisibilityForStyle)
    int fxChorusY = 0, fxDelayY = 0, fxReverbY = 0, fxNoiseY = 0;
    // Row Y for label drawing: each FX has 3 rows
    int fxRowY[12] = {};

    // Tab strip (custom-painted, no child windows)
    RECT tabRects[kTabCount]     = {};
    RECT tabDotRects[kTabCount]  = {};
    RECT tabMuteRects[kTabCount] = {}; // M button rect per tab
    RECT tabSoloRects[kTabCount] = {}; // S button rect per tab
    int  hoverTab = -1;

    // Theme resources (created in attached, freed in removed)
    HBRUSH brBg     = nullptr;
    HFONT  fontReg  = nullptr;
    HFONT  fontBold = nullptr;
    HFONT  fontTitle= nullptr;
    HFONT  fontSection = nullptr;
    HFONT  fontTab     = nullptr;
    HFONT  fontMono = nullptr;

    // Section paint info â€” y position of each section title (filled by applyLayout)
    int sectionY[4] = {0,0,0,0};
    int sectionX[4] = {0,0,0,0};
    int sectionW[4] = {0,0,0,0};
    static const wchar_t* sectionTitle(int i) {
        static const wchar_t* t[4] = { L"SOUND", L"RYTHME", L"HARMONIE", L"EXPRESSION" };
        return t[i];
    }

    static const wchar_t* tabLabel(int i) {
        static const wchar_t* t[kTabCount] = {
            L"M\u00e9lodie", L"Arp\u00e8ge", L"Basse", L"Percu.", L"Contre", L"Piano"
        };
        return t[i];
    }

    static void ensureWndClass() {
        // Check if already registered (survives plugin reload within same process).
        WNDCLASSW existing{};
        if (GetClassInfoW(g_hInst, kWndClass, &existing)) return;

        WNDCLASSW wc{};
        wc.lpfnWndProc   = staticWndProc;
        wc.hInstance     = g_hInst;
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = kWndClass;
        wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
        RegisterClassW(&wc);
    }

    static LRESULT CALLBACK staticWndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
        auto* self = reinterpret_cast<MelodyMakerView*>(GetWindowLongPtrW(h, GWLP_USERDATA));
        if (self) return self->wndProc(h, m, w, l);
        return DefWindowProcW(h, m, w, l);
    }

    // Piano roll child window proc
    static constexpr const wchar_t* kPRClass = L"MidnightMMPianoRoll";
    static void ensurePRClass() {
        WNDCLASSW ex{};
        if (GetClassInfoW(g_hInst, kPRClass, &ex)) return;
        WNDCLASSW wc{};
        wc.lpfnWndProc   = prWndProc;
        wc.hInstance     = g_hInst;
        wc.hbrBackground = nullptr;
        wc.lpszClassName = kPRClass;
        wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
        RegisterClassW(&wc);
    }
    static LRESULT CALLBACK prWndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
        if (m == WM_ERASEBKGND) return 1;
        auto* self = reinterpret_cast<MelodyMakerView*>(GetWindowLongPtrW(h, GWLP_USERDATA));
        if (m == WM_PAINT) {
            if (self) { self->paintPianoRoll(h); return 0; }
        }
        if (self && self->plugin) {
            int mx = GET_X_LPARAM(l), my = GET_Y_LPARAM(l);
            switch (m) {
                case WM_LBUTTONDOWN: {
                    SetCapture(h);
                    self->prEditorMouseDown(mx, my);
                    return 0;
                }
                case WM_MOUSEMOVE:
                    if (self->prDragging) self->prEditorMouseDrag(mx, my);
                    return 0;
                case WM_LBUTTONUP: {
                    bool relCapture = false;
                    // Piano-strip key release: send note-off, finalize held-note length
                    if (self->prPressedKey >= 0) {
                        int style = self->plugin->getStyleType();
                        self->plugin->guiNoteOff(self->prPressedKey, style);
                        if (self->prPressedNoteIdx >= 0 &&
                            self->prPressedNoteIdx < (int)self->prSnapshot.size()) {
                            DWORD held = GetTickCount() - self->prPressedMs;
                            float bpm = self->plugin->lastBpm.load(std::memory_order_relaxed);
                            double beatsHeld = (held / 1000.0) * (bpm / 60.0);
                            double len = std::max(1.0 / 16.0, std::round(beatsHeld * 16.0) / 16.0);
                            self->prSnapshot[self->prPressedNoteIdx].beatLen = len;
                        }
                        self->prPressedKey     = -1;
                        self->prPressedNoteIdx = -1;
                        relCapture = true;
                    }
                    if (self->prDragging) {
                        self->prDragging      = false;
                        self->prDragNoteIndex = -1;
                        relCapture = true;
                    }
                    // Flush edits to recNotes then unfreeze snapshot
                    if (self->prFreezeSnapshot) {
                        self->flushSnapshotToRecNotes();
                        self->prFreezeSnapshot = false;
                    }
                    if (relCapture) ReleaseCapture();
                    if (self->hPianoRoll) InvalidateRect(self->hPianoRoll, nullptr, FALSE);
                    return 0;
                }
                case WM_RBUTTONDOWN:
                    self->prEditorRightClick(mx, my);
                    return 0;
            }
        }
        return DefWindowProcW(h, m, w, l);
    }
    // -- Coordinate helpers (used by both painter and edit handlers) --
    // Last computed pitch range, beat range, and key-strip width for the
    // current piano roll painting; updated on each WM_PAINT.
    int    prKeyW       = 0;    // left-strip width (0 in Synthesia mode, no left strip)
    int    prPianoStripH = kPianoStripH; // bottom keyboard strip height (updated at paint)
    int    prPitchLo    = 36;
    int    prPitchHi    = 84;
    double prStartBeat  = 0.0;
    double prEndBeat    = 16.0;
    int    prW          = 0;
    int    prH          = 0;

    bool isEditorActive() const { return viewMode == 1 && plugin != nullptr; }
    static constexpr int kEditorBeats = 8; // kept for compat — editor now uses same kPRBeats

    // Convert roll client coords → (beat, pitch).  Returns false if outside the note area.
    // Synthesia layout: X = pitch, Y = time (top=oldest, bottom=newest), piano strip at bottom.
    bool prClientToBeatPitch(int mx, int my, double& beatOut, int& pitchOut) const {
        int rollH = prH - prPianoStripH;
        if (mx < 0 || mx >= prW || my < 0 || my >= rollH) return false;
        int range = prPitchHi - prPitchLo;
        if (range <= 0 || prW <= 0 || rollH <= 0) return false;
        pitchOut = prPitchLo + (int)((double)mx / prW * range);
        pitchOut = std::clamp(pitchOut, prPitchLo, prPitchHi - 1);
        double beatRange = prEndBeat - prStartBeat;
        if (beatRange <= 0) return false;
        beatOut = prStartBeat + (double)my / rollH * beatRange;
        return true;
    }
    // Convert X coord only → pitch (used for piano-strip clicks).
    bool prPianoXToPitch(int mx, int& pitchOut) const {
        if (mx < 0 || mx >= prW) return false;
        int range = prPitchHi - prPitchLo;
        if (range <= 0) return false;
        pitchOut = prPitchLo + (int)((double)mx / prW * range);
        pitchOut = std::clamp(pitchOut, prPitchLo, prPitchHi - 1);
        return true;
    }

    // ============================================================
    //  paintPianoRoll  —  Synthesia style
    //  X axis: pitch (low = left, high = right)
    //  Y axis: time  (top = oldest, bottom = most recent)
    //  Piano keyboard strip at the very bottom (kPianoStripH px).
    // ============================================================
    void paintPianoRoll(HWND h) {
        PAINTSTRUCT ps; HDC hdc = BeginPaint(h, &ps);
        RECT rc; GetClientRect(h, &rc);
        int W = rc.right, H = rc.bottom;
        prW = W; prH = H;
        prKeyW = 0; // no left strip in Synthesia mode
        prPianoStripH = kPianoStripH;

        HDC mem = CreateCompatibleDC(hdc);
        HBITMAP bmp = CreateCompatibleBitmap(hdc, W, H);
        HBITMAP oldBmp = (HBITMAP)SelectObject(mem, bmp);

        // ---- Background ----
        HBRUSH brBack = CreateSolidBrush(RGB(18, 22, 30));
        FillRect(mem, &rc, brBack); DeleteObject(brBack);

        // ---- Pick note source (always prSnapshot, both modes) ----
        int curStyle = plugin ? plugin->getStyleType() : 0;
        bool editor = isEditorActive(); // used for playhead colour / UI hints
        const std::vector<PRNote>* src = nullptr;
        std::vector<PRNote> filtered;
        filtered.reserve(prSnapshot.size());
        for (auto& n : prSnapshot)
            if ((int)n.style == curStyle) filtered.push_back(n);
        src = &filtered;

        // ---- Time window (always scrolling, both modes) ----
        // Show all notes ever recorded when playhead is at 0 (not yet started)
        double endBeat   = (prCurrentBeat > (double)kPRBeats) ? prCurrentBeat : (double)kPRBeats;
        double startBeat = endBeat - (double)kPRBeats;
        prStartBeat = startBeat;
        prEndBeat   = endBeat;

        // ---- Dynamic pitch range ----
        static const bool kIsBlack[12] = {0,1,0,1,0,0,1,0,1,0,1,0};
        int pLo = 48, pHi = 84; // default C3..C6
        if (!src->empty()) {
            int mn = 127, mx2 = 0;
            for (auto& n : *src) {
                if (n.pitch < mn) mn = n.pitch;
                if (n.pitch > mx2) mx2 = n.pitch;
            }
            mn = std::max(0, mn - 2);
            mx2 = std::min(127, mx2 + 2);
            mn -= (mn % 12);               // snap to C
            mx2 += (12 - (mx2 % 12)) % 12;
            while ((mx2 - mn) < 24) { if (mn >= 12) mn -= 12; else mx2 += 12; }
            pLo = mn; pHi = mx2;
        }
        prPitchLo = pLo;
        prPitchHi = pHi;
        const int pitchRange = pHi - pLo;

        int rollH = H - kPianoStripH;
        double beatRange = endBeat - startBeat;

        // Coordinate lambdas ---------------------------------------------------
        // pitchToX: left edge of a semitone column
        auto pitchToX1 = [&](int p) -> int {
            return (int)((double)(p - pLo) / pitchRange * W);
        };
        auto pitchToX2 = [&](int p) -> int {  // right edge = left edge of next
            return (int)((double)(p + 1 - pLo) / pitchRange * W);
        };
        // beatToY: Y coord in roll area (0=top/oldest, rollH=bottom/most-recent)
        auto beatToY = [&](double b) -> int {
            return (int)((b - startBeat) / beatRange * rollH);
        };

        // ---- Semitone column shading (black-key lanes slightly darker) ----
        for (int p = pLo; p < pHi; ++p) {
            if (!kIsBlack[(p + 120) % 12]) continue;
            int x1 = pitchToX1(p), x2 = pitchToX2(p);
            RECT rl = { x1, 0, x2, rollH };
            HBRUSH bl = CreateSolidBrush(RGB(25, 30, 40));
            FillRect(mem, &rl, bl); DeleteObject(bl);
        }

        // ---- Octave / beat grid ----
        // Vertical: C-note column borders
        for (int p = pLo; p <= pHi; ++p) {
            if (((p + 120) % 12) == 0) {
                int x = pitchToX1(p);
                HPEN pn = CreatePen(PS_SOLID, 1, RGB(55, 65, 80));
                HPEN op = (HPEN)SelectObject(mem, pn);
                MoveToEx(mem, x, 0, nullptr); LineTo(mem, x, rollH);
                SelectObject(mem, op); DeleteObject(pn);
            }
        }
        // Horizontal: beat grid (beat lines + bar lines)
        for (int b = 0; b <= (int)kPRBeats; ++b) {
            int y = beatToY(startBeat + b);
            y = std::clamp(y, 0, rollH - 1);
            int absBeat = (int)std::floor(startBeat) + b;
            COLORREF c = (absBeat % 4 == 0) ? RGB(55, 65, 80) : RGB(35, 42, 52);
            HPEN pn = CreatePen(PS_SOLID, 1, c);
            HPEN op = (HPEN)SelectObject(mem, pn);
            MoveToEx(mem, 0, y, nullptr); LineTo(mem, W, y);
            SelectObject(mem, op); DeleteObject(pn);
        }
        // Beat / bar numbers (always shown)
        {
            SetBkMode(mem, TRANSPARENT);
            HFONT of = (HFONT)SelectObject(mem, fontReg);
            SetTextColor(mem, RGB(100, 115, 135));
            for (int b = 0; b <= (int)kPRBeats; b++) {
                int absBeat = (int)std::floor(startBeat) + b;
                if (absBeat % 4 != 0) continue;
                int y = beatToY(startBeat + b);
                y = std::clamp(y, 0, rollH - 14);
                wchar_t s[8]; swprintf_s(s, L"%d", absBeat / 4 + 1);
                TextOutW(mem, 2, y + 1, s, (int)wcslen(s));
            }
            SelectObject(mem, of);
        }

        // ---- Per-style colors ----
        static const COLORREF kCol[] = {
            RGB(79,195,247), RGB(129,199,132), RGB(255,183,77),
            RGB(240,98,146), RGB(206,147,216), RGB(255,241,118) };
        static const COLORREF kColFill[] = {
            RGB(25,70,105),  RGB(35,75,42),   RGB(90,65,18),
            RGB(95,28,52),   RGB(72,45,88),   RGB(90,85,28) };

        // ---- Notes ----
        // Active pitches this frame (to highlight piano keys)
        bool activeKey[128] = {};

        for (auto& n : *src) {
            double nOn = n.beatOn, nOff = n.beatOn + std::max(n.beatLen, 0.05);
            if (nOff < startBeat || nOn > endBeat) continue;
            int p = (int)n.pitch;
            if (p < pLo || p >= pHi) continue;
            int s = std::clamp((int)n.style, 0, 5);

            int x1 = pitchToX1(p), x2 = std::max(pitchToX2(p), x1 + 2);
            int y1 = std::clamp(beatToY(nOn),  0, rollH);
            int y2 = std::clamp(beatToY(nOff), 0, rollH);
            // Enforce minimum visible height (10px) so short notes are legible
            if (y2 < y1 + 10) y2 = std::min(rollH, y1 + 10);

            // Note body
            RECT rn = { x1 + 1, y1, x2 - 1, y2 };
            HBRUSH bf = CreateSolidBrush(kColFill[s]);
            FillRect(mem, &rn, bf); DeleteObject(bf);
            // Accent border
            HPEN pn = CreatePen(PS_SOLID, 1, kCol[s]);
            HPEN op = (HPEN)SelectObject(mem, pn);
            HBRUSH nb = (HBRUSH)SelectObject(mem, GetStockObject(NULL_BRUSH));
            Rectangle(mem, rn.left, rn.top, rn.right, rn.bottom);
            SelectObject(mem, op); DeleteObject(pn);
            SelectObject(mem, nb);

            // Bright top cap (note head / attack indicator)
            HPEN cap = CreatePen(PS_SOLID, 2, kCol[s]);
            HPEN oc  = (HPEN)SelectObject(mem, cap);
            MoveToEx(mem, x1 + 1, y1 + 1, nullptr);
            LineTo   (mem, x2 - 1, y1 + 1);
            SelectObject(mem, oc); DeleteObject(cap);

            // Mark key as active if the note is currently sounding
            if (nOn <= prCurrentBeat && prCurrentBeat <= nOff) activeKey[p] = true;
        }

        // ---- Playhead (horizontal line) ----
        {
            int py = std::clamp(beatToY(prCurrentBeat), 0, rollH - 1);
            HPEN pph = CreatePen(PS_SOLID, 2, editor ? RGB(255,255,120) : RGB(220,220,255));
            HPEN oph = (HPEN)SelectObject(mem, pph);
            MoveToEx(mem, 0, py, nullptr); LineTo(mem, W, py);
            SelectObject(mem, oph); DeleteObject(pph);
        }

        // ---- Piano keyboard strip (bottom kPianoStripH px) ----
        {
            int kbY = rollH; // top of keyboard strip
            // White key background
            HBRUSH wkBr = CreateSolidBrush(RGB(235,232,225));
            RECT rcKb = { 0, kbY, W, H };
            FillRect(mem, &rcKb, wkBr); DeleteObject(wkBr);

            // White key dividers
            for (int p = pLo; p < pHi; ++p) {
                if (kIsBlack[(p + 120) % 12]) continue;
                int x2 = pitchToX2(p);
                HPEN pn = CreatePen(PS_SOLID, 1, RGB(150,145,135));
                HPEN op = (HPEN)SelectObject(mem, pn);
                MoveToEx(mem, x2, kbY, nullptr); LineTo(mem, x2, H);
                SelectObject(mem, op); DeleteObject(pn);
                // Active highlight (playback or GUI press)
                bool pressed = (p == prPressedKey);
                if (activeKey[p] || pressed) {
                    int xw1 = pitchToX1(p) + 1, xw2 = pitchToX2(p) - 1;
                    COLORREF hc = pressed ? RGB(255,255,255) : kCol[curStyle];
                    HBRUSH ah = CreateSolidBrush(hc);
                    RECT ra = { xw1, kbY + kPianoStripH/3, xw2, H - 2 };
                    FillRect(mem, &ra, ah); DeleteObject(ah);
                    if (pressed) {
                        // Small glowing outline
                        HPEN gp = CreatePen(PS_SOLID, 2, kCol[curStyle]);
                        HPEN og = (HPEN)SelectObject(mem, gp);
                        HBRUSH nbr = (HBRUSH)SelectObject(mem, GetStockObject(NULL_BRUSH));
                        Rectangle(mem, xw1, kbY + kPianoStripH/3, xw2, H - 2);
                        SelectObject(mem, og); DeleteObject(gp);
                        SelectObject(mem, nbr);
                    }
                }
            }

            // Black keys (drawn on top, 60% height, 65% width)
            int bkH = (int)(kPianoStripH * 0.62);
            for (int p = pLo; p < pHi; ++p) {
                if (!kIsBlack[(p + 120) % 12]) continue;
                double semW = (double)W / pitchRange;
                int cx = pitchToX1(p) + (int)(semW / 2.0);
                int bkW = std::max(3, (int)(semW * 0.65));
                int xb1 = cx - bkW / 2, xb2 = xb1 + bkW;
                COLORREF bkFill = (p == prPressedKey) ? RGB(200,200,200) : (activeKey[p] ? kCol[curStyle] : RGB(30, 28, 24));
                HBRUSH bb = CreateSolidBrush(bkFill);
                RECT rb = { xb1, kbY, xb2, kbY + bkH };
                FillRect(mem, &rb, bb); DeleteObject(bb);
                // Border
                HPEN pn = CreatePen(PS_SOLID, 1, RGB(15, 12, 10));
                HPEN op = (HPEN)SelectObject(mem, pn);
                HBRUSH nob = (HBRUSH)SelectObject(mem, GetStockObject(NULL_BRUSH));
                Rectangle(mem, rb.left, rb.top, rb.right, rb.bottom);
                SelectObject(mem, op); DeleteObject(pn);
                SelectObject(mem, nob);
            }

            // Divider line between roll and keyboard
            HPEN div = CreatePen(PS_SOLID, 1, RGB(60, 70, 85));
            HPEN odiv = (HPEN)SelectObject(mem, div);
            MoveToEx(mem, 0, kbY, nullptr); LineTo(mem, W, kbY);
            SelectObject(mem, odiv); DeleteObject(div);
        }

        // ---- Border ----
        HPEN penB = CreatePen(PS_SOLID, 1, RGB(55,65,80));
        HPEN opB  = (HPEN)SelectObject(mem, penB);
        HBRUSH nb2 = (HBRUSH)SelectObject(mem, GetStockObject(NULL_BRUSH));
        Rectangle(mem, 0, 0, W, H);
        SelectObject(mem, opB); DeleteObject(penB);
        SelectObject(mem, nb2);

        BitBlt(hdc, 0, 0, W, H, mem, 0, 0, SRCCOPY);
        SelectObject(mem, oldBmp);
        DeleteObject(bmp); DeleteDC(mem);
        EndPaint(h, &ps);
    }

    // ---- Flush prSnapshot → recNotes ----
    void flushSnapshotToRecNotes() {
        if (!plugin) return;
        std::lock_guard<std::mutex> lk(plugin->recMtx);
        double rsb = plugin->recStartBeat;
        plugin->recNotes.clear();
        plugin->pinnedNotes.clear();
        for (auto& n : prSnapshot) {
            auto rn = MelodyMakerVST3::RecordedNote{
                n.beatOn - rsb, n.beatLen, n.pitch, n.vel, n.style };
            plugin->recNotes.push_back(rn);
            plugin->pinnedNotes.push_back(rn);
        }
        // Also refresh the audio-thread copy so pinned notes play
        // at the next transport start (or immediately if already rolling,
        // the restart detection will repopulate audioNotes).
        plugin->audioNotes = plugin->pinnedNotes;
    }

    // ---- View mode + edit helpers ----
    void setViewMode(int m) {
        if (m == viewMode) return;
        viewMode = m;
        applyVisibilityForStyle(plugin ? plugin->getStyleType() : 0);
        InvalidateRect(hWnd, nullptr, TRUE);
        if (hPianoRoll) InvalidateRect(hPianoRoll, nullptr, FALSE);
    }

    void pushUndo() {
        undoStack.push_back(prSnapshot);
        if (undoStack.size() > kUndoLimit) undoStack.erase(undoStack.begin());
        redoStack.clear();
    }
    void doUndo() {
        if (undoStack.empty()) return;
        redoStack.push_back(prSnapshot);
        prSnapshot = undoStack.back();
        undoStack.pop_back();
        flushSnapshotToRecNotes();
        if (hPianoRoll) InvalidateRect(hPianoRoll, nullptr, FALSE);
    }
    void doRedo() {
        if (redoStack.empty()) return;
        undoStack.push_back(prSnapshot);
        prSnapshot = redoStack.back();
        redoStack.pop_back();
        flushSnapshotToRecNotes();
        if (hPianoRoll) InvalidateRect(hPianoRoll, nullptr, FALSE);
    }

    // Hit-test against prSnapshot (returns index into prSnapshot, -1 if miss)
    int hitTestNote(int mx, int my) const {
        if (!plugin) return -1;
        int rollH = prH - prPianoStripH;
        if (mx < 0 || mx >= prW || my < 0 || my >= rollH) return -1;
        double beatRange = prEndBeat - prStartBeat;
        if (beatRange <= 0 || prW <= 0 || rollH <= 0) return -1;
        int range = prPitchHi - prPitchLo;
        if (range <= 0) return -1;
        int curStyle = plugin->getStyleType();
        for (int i = (int)prSnapshot.size() - 1; i >= 0; --i) {
            const auto& n = prSnapshot[i];
            if ((int)n.style != curStyle) continue;
            int x1 = (int)((double)((int)n.pitch - prPitchLo) / range * prW);
            int x2 = (int)((double)((int)n.pitch + 1 - prPitchLo) / range * prW);
            if (x2 <= x1) x2 = x1 + 2;
            int y1 = (int)((n.beatOn - prStartBeat) / beatRange * rollH);
            int y2 = (int)((n.beatOn + std::max(n.beatLen, 0.05) - prStartBeat) / beatRange * rollH);
            if (y2 <= y1) y2 = y1 + 3;
            if (mx >= x1 && mx < x2 && my >= y1 && my <= y2) return i;
        }
        return -1;
    }

    void prEditorMouseDown(int mx, int my) {
        if (!plugin) return;
        int rollH = prH - prPianoStripH;
        int curStyle = plugin->getStyleType();

        // ---- Click in the piano keyboard strip → tap a key / insert note ----
        if (my >= rollH) {
            int pitch;
            if (!prPianoXToPitch(mx, pitch)) return;
            plugin->guiNoteOn(pitch, 0.85f, curStyle);
            prPressedKey     = pitch;
            prPressedMs      = GetTickCount();
            prFreezeSnapshot = true;
            pushUndo();
            // Snap to 1/16 at current playhead
            double insertBeat = std::round(prCurrentBeat * 16.0) / 16.0;
            PRNote n{ insertBeat, 0.25, (int16_t)pitch, 0.85f, (uint8_t)curStyle };
            prSnapshot.push_back(n);
            prPressedNoteIdx = (int)prSnapshot.size() - 1;
            if (hPianoRoll) InvalidateRect(hPianoRoll, nullptr, FALSE);
            return;
        }

        // ---- Click in the note roll area ----
        int hit = hitTestNote(mx, my);
        prFreezeSnapshot = true;
        pushUndo();
        if (hit >= 0) {
            // Grab existing note to drag-extend
            prDragging = true;
            prDragNoteIndex = hit;
            prDragStartBeat = prSnapshot[hit].beatOn;
            return;
        }
        // Empty area: add a new note (pitch from X, beat from Y), snap 1/16
        double beat; int pitch;
        if (!prClientToBeatPitch(mx, my, beat, pitch)) { prFreezeSnapshot = false; return; }
        beat = std::round(beat * 16.0) / 16.0;
        PRNote n{ beat, 0.25, (int16_t)pitch, 0.85f, (uint8_t)curStyle };
        prSnapshot.push_back(n);
        prDragging = true;
        prDragNoteIndex = (int)prSnapshot.size() - 1;
        prDragStartBeat = beat;
        // Preview the note sound immediately
        plugin->guiNoteOn(pitch, 0.85f, curStyle);
        prPressedKey = pitch;
        if (hPianoRoll) InvalidateRect(hPianoRoll, nullptr, FALSE);
    }
    void prEditorMouseDrag(int mx, int my) {
        if (prDragNoteIndex < 0 || prDragNoteIndex >= (int)prSnapshot.size()) return;
        int rollH = prH - prPianoStripH;
        double beatRange = prEndBeat - prStartBeat;
        if (rollH <= 0 || beatRange <= 0) return;
        double curBeat = prStartBeat + (double)std::clamp(my, 0, rollH) / rollH * beatRange;
        curBeat = std::round(curBeat * 16.0) / 16.0; // 1/16 snap
        double newLen = std::max(1.0 / 16.0, curBeat - prDragStartBeat + 1.0 / 16.0);
        prSnapshot[prDragNoteIndex].beatLen = newLen;
        if (hPianoRoll) InvalidateRect(hPianoRoll, nullptr, FALSE);
    }
    void prEditorRightClick(int mx, int my) {
        if (!plugin) return;
        int hit = hitTestNote(mx, my);
        if (hit < 0) return;
        pushUndo();
        prFreezeSnapshot = true;
        prSnapshot.erase(prSnapshot.begin() + hit);
        flushSnapshotToRecNotes();
        prFreezeSnapshot = false;
        if (hPianoRoll) InvalidateRect(hPianoRoll, nullptr, FALSE);
    }

    LRESULT wndProc(HWND h, UINT msg, WPARAM w, LPARAM l) {
        switch (msg) {
            case WM_COMMAND: {
                int id   = LOWORD(w);
                int code = HIWORD(w);
                if (id == kIdExport && code == BN_CLICKED) {
                    doExportMidi();
                    fileMenuOpen = false;
                    applyVisibilityForStyle(plugin ? plugin->getStyleType() : 0);
                    return 0;
                }
                if (id == kIdExportWav && code == BN_CLICKED) {
                    doExportWav();
                    fileMenuOpen = false;
                    applyVisibilityForStyle(plugin ? plugin->getStyleType() : 0);
                    return 0;
                }
                if (id == kIdSavePreset && code == BN_CLICKED) {
                    doSavePreset();
                    fileMenuOpen = false;
                    applyVisibilityForStyle(plugin ? plugin->getStyleType() : 0);
                    return 0;
                }
                if (id == kIdLoadPreset && code == BN_CLICKED) {
                    doLoadPreset();
                    fileMenuOpen = false;
                    applyVisibilityForStyle(plugin ? plugin->getStyleType() : 0);
                    return 0;
                }
                if (id == kIdFile && code == BN_CLICKED) {
                    fileMenuOpen = !fileMenuOpen;
                    applyVisibilityForStyle(plugin ? plugin->getStyleType() : 0);
                    return 0;
                }
                // ---- Maker buttons ----
                if (id == kIdMakerEnable && code == BN_CLICKED) {
                    int st = plugin ? plugin->getStyleType() : 0;
                    makerEnabled[st] = !makerEnabled[st];
                    if (hMakerEnable) SetWindowTextW(hMakerEnable,
                        makerEnabled[st] ? L"\u25cf OUI" : L"\u25cb NON");
                    InvalidateRect(hMakerEnable, nullptr, TRUE);
                    return 0;
                }
                if (id == kIdMakerGenerate && code == BN_CLICKED) {
                    if (!plugin) return 0;
                    int st = plugin->getStyleType();
                    loadMakerCfgFromControls(st);
                    makerEnabled[st] = true;
                    makerDirty[st]   = false; // manual generate clears the dirty flag
                    if (hMakerEnable) SetWindowTextW(hMakerEnable, L"\u25cf OUI");
                    SetWindowTextW(hMakerGenerateBtn, L"\u23f3 Synth\u00e8se...");
                    // Save to disk + load live preview
                    plugin->createOrUpdateUserSf(makerCfg[st], st);
                    plugin->reloadLiveSf(st, makerCfg[st]);   // live in-memory preview
                    SetWindowTextW(hMakerGenerateBtn, L"RAFRA\u00ceCHIR  \u21ba");
                    // Refresh instrument combo and auto-select the new preset
                    populateInstrumentCombo(st);
                    // Auto-select: find the user preset matching the name we just saved
                    {
                        int n = plugin->effectivePresetCount(st);
                        for (int i = n - 1; i >= 0; --i) {
                            SfPreset p = plugin->effectivePreset(st, i);
                            if (p.source == 2) { // user-generated
                                plugin->setSfPreset(st, i);
                                SendMessageW(hWaveCombo, CB_SETCURSEL, i, 0);
                                break;
                            }
                        }
                    }
                    InvalidateRect(hWnd, nullptr, TRUE);
                    return 0;
                }
                // ---- Charger / Effacer SF2 externe -------------------------
                if (id == kIdMakerLoadSf2 && code == BN_CLICKED && plugin) {
                    int st = plugin->getStyleType();
                    std::wstring path = sfed::browseSf2File(hWnd);
                    if (!path.empty()) {
                        std::vector<uint8_t> bytes = sfed::loadSf2File(path.c_str());
                        if (!bytes.empty()) {
                            plugin->externalSf2PerStyle[st] = std::move(bytes);
                            plugin->externalSf2PathPerStyle[st] = path;
                            // Réinitialiser delta à 0 lors d'un nouveau chargement
                            plugin->sf2DeltaPerStyle[st] = sfed::Sf2Delta{};
                            // Remettre sliders delta au centre
                            auto resetSlider = [&](HWND s, int centre) {
                                if (s) SendMessageW(s, TBM_SETPOS, TRUE, centre);
                            };
                            resetSlider(hMakerDeltaTuneSlider, 24);
                            resetSlider(hMakerDeltaAtkSlider, 50);  resetSlider(hMakerDeltaDecSlider, 50);
                            resetSlider(hMakerDeltaSusSlider, 50);  resetSlider(hMakerDeltaRelSlider, 50);
                            resetSlider(hMakerDeltaVolSlider, 50);  resetSlider(hMakerDeltaFiltSlider, 50);
                            refreshDeltaLabels();
                            // Afficher le nom court
                            std::wstring shortName = sfed::sf2ShortName(path);
                            if (hMakerSf2Label) SetWindowTextW(hMakerSf2Label, shortName.c_str());
                            // Charger immédiatement
                            plugin->reloadExternalSf(st);
                            showExternalSf2Controls(true);
                        } else {
                            MessageBoxW(hWnd, L"Impossible de lire ce fichier SF2.",
                                        L"Erreur", MB_OK | MB_ICONWARNING);
                        }
                    }
                    return 0;
                }
                if (id == kIdMakerClearSf2 && code == BN_CLICKED && plugin) {
                    int st = plugin->getStyleType();
                    plugin->externalSf2PerStyle[st].clear();
                    plugin->externalSf2PathPerStyle[st].clear();
                    plugin->sf2DeltaPerStyle[st] = sfed::Sf2Delta{};
                    if (hMakerSf2Label) SetWindowTextW(hMakerSf2Label, L"(aucun SF2 chargé)");
                    showExternalSf2Controls(false);
                    // Revenir au SF2 généré si Maker est actif
                    if (makerEnabled[st]) plugin->reloadLiveSf(st, makerCfg[st]);
                    return 0;
                }
                // ---- FX toggles ----
                if (code == BN_CLICKED && plugin) {
                    int st = plugin->getStyleType();
                    auto& p = plugin->fxParamsPerStyle[st];
                    if (id == kIdFxChorusOn) {
                        p.chorusOn = !p.chorusOn;
                        if (hFxChorusOn) SetWindowTextW(hFxChorusOn, p.chorusOn ? L"\u25cf OUI" : L"\u25cb NON");
                        plugin->applyFxParamsToChain(st); return 0;
                    }
                    if (id == kIdFxDelayOn) {
                        p.delayOn = !p.delayOn;
                        if (hFxDelayOn)  SetWindowTextW(hFxDelayOn, p.delayOn ? L"\u25cf OUI" : L"\u25cb NON");
                        plugin->applyFxParamsToChain(st); return 0;
                    }
                    if (id == kIdFxReverbOn) {
                        p.reverbOn = !p.reverbOn;
                        if (hFxReverbOn) SetWindowTextW(hFxReverbOn, p.reverbOn ? L"\u25cf OUI" : L"\u25cb NON");
                        plugin->applyFxParamsToChain(st); return 0;
                    }
                    if (id == kIdFxNoiseOn) {
                        p.noiseOn = !p.noiseOn;
                        if (hFxNoiseOn)  SetWindowTextW(hFxNoiseOn, p.noiseOn ? L"\u25cf OUI" : L"\u25cb NON");
                        plugin->applyFxParamsToChain(st); return 0;
                    }
                }
                if (id == kIdUndo && code == BN_CLICKED) { doUndo(); return 0; }
                if (id == kIdRedo && code == BN_CLICKED) { doRedo(); return 0; }
                if (id == kIdAuto && code == BN_CLICKED) {
                    int st = plugin->getStyleType();
                    bool now = !plugin->jamPerStyle[st];
                    plugin->jamPerStyle[st] = now;
                    SetWindowTextW(hAutoBtn,
                        now ? L"JAM  \u25CF  ON" : L"JAM  \u25CB  OFF");
                    InvalidateRect(hAutoBtn, nullptr, TRUE);
                    return 0;
                }
                if (id == kIdDice && code == BN_CLICKED) {
                    plugin->randomizeSeed();
                    refreshOne(mm::kParamSeed);
                    return 0;
                }
                if (id == kIdPianoMel && code == BN_CLICKED) {
                    LRESULT chk = SendMessageW(hPianoMelChk, BM_GETCHECK, 0, 0);
                    int st = plugin->getStyleType();
                    plugin->pianoMelodyPerStyle[st] = (chk == BST_CHECKED);
                    return 0;
                }
                if (id == kIdPianoChord && code == BN_CLICKED) {
                    LRESULT chk = SendMessageW(hPianoChordChk, BM_GETCHECK, 0, 0);
                    int st = plugin->getStyleType();
                    plugin->pianoChordPerStyle[st] = (chk == BST_CHECKED);
                    return 0;
                }
                if (id == kIdPercRhy && code == CBN_SELCHANGE) {
                    int sel = (int)SendMessageW(hPercRhyCombo, CB_GETCURSEL, 0, 0);
                    int st = plugin->getStyleType();
                    plugin->percRhythmPerStyle[st] =
                        std::clamp(sel, 0, mm::kDrumPatternCount - 1);
                    return 0;
                }
                if (id == kIdWave && code == CBN_SELCHANGE) {
                    int sel = (int)SendMessageW(hWaveCombo, CB_GETCURSEL, 0, 0);
                    if (sel != CB_ERR) {
                        int st = plugin->getStyleType();
                        plugin->setSfPreset(st, sel);
                        plugin->setWaveType(sel); // keep legacy state happy
                    }
                    return 0;
                }
                if (id == kIdProg && code == CBN_SELCHANGE) {
                    int sel = (int)SendMessageW(hProgCombo, CB_GETCURSEL, 0, 0);
                    if (sel != CB_ERR) {
                        plugin->setProgType(sel);
                        // Refresh compat dots on Mode combo.
                        if (hCtrl[mm::kParamMode]) {
                            InvalidateRect(hCtrl[mm::kParamMode], nullptr, FALSE);
                            UpdateWindow(hCtrl[mm::kParamMode]);
                        }
                    }
                    return 0;
                }
                if ((id == kIdLockMode || id == kIdLockProg ||
                     id == kIdLockSubdiv) && code == BN_CLICKED) {
                    int s = plugin->getStyleType();
                    if (id == kIdLockMode) {
                        plugin->setLockMode(s, !plugin->lockModePerStyle[s]);
                        InvalidateRect(hLockMode, nullptr, TRUE);
                    } else if (id == kIdLockProg) {
                        plugin->setLockProg(s, !plugin->lockProgPerStyle[s]);
                        InvalidateRect(hLockProg, nullptr, TRUE);
                    } else {
                        plugin->setLockSubdiv(s, !plugin->lockSubdivPerStyle[s]);
                        InvalidateRect(hLockSubdiv, nullptr, TRUE);
                    }
                    return 0;
                }
                if (id == kIdMeter && code == CBN_SELCHANGE) {
                    int sel = (int)SendMessageW(hMeterCombo, CB_GETCURSEL, 0, 0);
                    if (sel != CB_ERR) {
                        plugin->beatsPerBarOverride = (sel == 0) ? 0 : sel + 1;
                        if (plugin->beatsPerBarOverride >= 2)
                            plugin->beatsPerBarAtomic.store(plugin->beatsPerBarOverride, std::memory_order_relaxed);
                    }
                    return 0;
                }
                if (id == kIdSection && code == CBN_SELCHANGE) {
                    int sel = (int)SendMessageW(hSectionCombo, CB_GETCURSEL, 0, 0);
                    if (sel >= 0 && sel < kSecCount) {
                        plugin->currentSection.store(sel, std::memory_order_relaxed);
                        // Notify the host so it tracks the change immediately
                        // (without this the host may restore an old section on setState).
                        double norm = sel / 4.0;
                        plugin->beginEdit(MelodyMakerVST3::kParamSection);
                        plugin->performEdit(MelodyMakerVST3::kParamSection, norm);
                        plugin->endEdit(MelodyMakerVST3::kParamSection);
                        if (sel == kSecIntro && !plugin->introFadeDone) plugin->sectionVolumeRamp = 0.0f;
                        if (sel == kSecOutro) plugin->sectionVolumeRamp = 1.0f;
                    }
                    return 0;
                }
                if (id == kIdStartBar && code == CBN_SELCHANGE) {
                    static constexpr int kSBV[] = { 0,1,2,4,8 };
                    int sel = (int)SendMessageW(hStartBarCombo, CB_GETCURSEL, 0, 0);
                    if (sel >= 0 && sel < 5)
                        plugin->startBarPerStyle[plugin->getStyleType()] = kSBV[sel];
                    return 0;
                }
                if (code == CBN_SELCHANGE && id >= kIdBase && id < kIdExport) {
                    onComboChange(id - kIdBase);
                    return 0;
                }
                // Maker combos (synth type, tessiture) → mark dirty for auto-refresh
                if (code == CBN_SELCHANGE && plugin &&
                    (id == kIdMakerSynth || id == kIdMakerLowNote ||
                     id == kIdMakerHighNote || id == kIdMakerStep)) {
                    makerDirty[plugin->getStyleType()] = true;
                    return 0;
                }
                break;
            }
            case WM_HSCROLL: {
                HWND tb = (HWND)l;
                int id = (int)GetWindowLongPtrW(tb, GWLP_ID);
                if (id >= kIdBase && id < kIdBase + (int)mm::kParamCount) {
                    onTrackbarChange(id - kIdBase);
                    return 0;
                }
                if (id == kIdVol) {
                    int pos = (int)SendMessageW(hVolSlider, TBM_GETPOS, 0, 0);
                    plugin->setSfVolume(plugin->getStyleType(), pos / 100.0f);
                    refreshVolValue();
                    return 0;
                }
                if (id == kIdHumanize) {
                    auto* kw = reinterpret_cast<KnobWidget*>(GetWindowLongPtrW((HWND)l, GWLP_USERDATA));
                    int pos = kw ? kw->getPos() : 0;
                    plugin->humanizePerStyle[plugin->getStyleType()] = pos / 1000.0f;
                    refreshHumanizeValue();
                    return 0;
                }
                if (id == kIdRetard) {
                    auto* kw = reinterpret_cast<KnobWidget*>(GetWindowLongPtrW((HWND)l, GWLP_USERDATA));
                    int pos = kw ? kw->getPos() : 0;
                    plugin->retardPerStyle[plugin->getStyleType()] = pos / 1000.0f;
                    refreshRetardValue();
                    return 0;
                }
                // ---- Maker sliders ----
                if (id >= kIdMakerAttack && id <= kIdMakerGain) {
                    refreshMakerVals();
                    if (plugin) makerDirty[plugin->getStyleType()] = true;
                    return 0;
                }
                // ---- Sliders delta SF2 externe ----
                if (id >= kIdMakerDeltaTune && id <= kIdMakerDeltaFilt) {
                    if (plugin) {
                        int st = plugin->getStyleType();
                        if (!plugin->externalSf2PerStyle[st].empty())
                            commitDeltaAndReload(st);
                        else
                            refreshDeltaLabels();
                    }
                    return 0;
                }
                // ---- FX sliders ----
                if (id >= kIdFxChorusRate && id <= kIdFxNoiseTone) {
                    refreshFxLabels();
                    if (plugin) commitFxFromControls(plugin->getStyleType());
                    return 0;
                }
                break;
            }
            case WM_TIMER:
                if (w == 1) {
                    refreshNoteDisplay();
                    // Update playhead beat position (used by both modes for visuals)
                    if (plugin)
                        prCurrentBeat = plugin->lastBeatPos.load(std::memory_order_relaxed);
                    // Refresh snapshot from recNotes (both modes), unless we're mid-edit.
                    // Strategy: merge incoming notes into prSnapshot rather than
                    // replacing it — notes already in the snapshot are kept until
                    // they have fully scrolled past the top of the roll.
                    if (hPianoRoll && plugin && !prFreezeSnapshot) {
                        if (plugin->recMtx.try_lock()) {
                            double rsb = plugin->recStartBeat;
                            // Collect the current recNotes as absolute-beat PRNotes.
                            std::vector<PRNote> incoming;
                            incoming.reserve(plugin->recNotes.size());
                            for (auto& n : plugin->recNotes) {
                                PRNote pr;
                                pr.beatOn  = rsb + n.beatOn;
                                pr.beatLen = n.beatLen;
                                pr.pitch   = n.pitch;
                                pr.vel     = n.vel;
                                pr.style   = n.style;
                                incoming.push_back(pr);
                            }
                            plugin->recMtx.unlock();

                            // Step 1 — prune notes whose tail has scrolled
                            // completely above the top of the visible roll.
                            double visStart = prCurrentBeat - (double)kPRBeats;
                            prSnapshot.erase(
                                std::remove_if(prSnapshot.begin(), prSnapshot.end(),
                                    [visStart](const PRNote& n) {
                                        return (n.beatOn + std::max(n.beatLen, 0.05)) < visStart;
                                    }),
                                prSnapshot.end());

                            // Step 2 — add incoming notes not already present.
                            // Identity key: (pitch, style, beatOn within 10ms tolerance).
                            for (auto& nr : incoming) {
                                bool found = false;
                                for (auto& ex : prSnapshot) {
                                    if (ex.pitch == nr.pitch &&
                                        ex.style == nr.style &&
                                        std::fabs(ex.beatOn - nr.beatOn) < 0.01) {
                                        found = true; break;
                                    }
                                }
                                if (!found) prSnapshot.push_back(nr);
                            }
                        }
                        InvalidateRect(hPianoRoll, nullptr, FALSE);
                    } else if (hPianoRoll) {
                        InvalidateRect(hPianoRoll, nullptr, FALSE);
                    }
                }
                // ---- Maker auto-refresh timer (id 1201) ----
                if (w == 1201 && plugin && viewMode == 2) {
                    int st = plugin->getStyleType();
                    if (makerDirty[st]) {
                        makerDirty[st] = false;
                        loadMakerCfgFromControls(st);
                        SetWindowTextW(hMakerGenerateBtn, L"\u23f3 Synth\u00e8se...");
                        plugin->reloadLiveSf(st, makerCfg[st]);
                        SetWindowTextW(hMakerGenerateBtn, L"RAFRA\u00ceCHIR  \u21ba");
                        // Update instrument combo so the new preset is visible
                        populateInstrumentCombo(st);
                    }
                }
                return 0;
            case WM_ERASEBKGND:
                return 1; // we paint ourselves in WM_PAINT
            case WM_PAINT: {
                PAINTSTRUCT ps; HDC hdc = BeginPaint(h, &ps);
                paintBackground(hdc);
                paintTabs(hdc);
                EndPaint(h, &ps);
                return 0;
            }
            case WM_LBUTTONDOWN: {
                int x = GET_X_LPARAM(l), y = GET_Y_LPARAM(l);
                // Title: MIDNIGHT | MELODY | MAKER | FX
                if (PtInRect(&titleRectMidnight, POINT{x, y})) {
                    setViewMode(0); return 0;
                }
                if (PtInRect(&titleRectMelody, POINT{x, y})) {
                    setViewMode(1); return 0;
                }
                if (PtInRect(&titleRectMaker, POINT{x, y})) {
                    setViewMode(2); return 0;
                }
                if (PtInRect(&titleRectFx, POINT{x, y})) {
                    setViewMode(3); return 0;
                }
                // Fallback: any header click far to the right → FX
                if (y < kHeaderH && x > titleRectMaker.right && x < kViewWidth - 190) {
                    setViewMode(3); return 0;
                }
                // PARAMÈTRES collapsible header click
                if (paramsTitleRect.bottom > paramsTitleRect.top &&
                    PtInRect(&paramsTitleRect, POINT{x, y})) {
                    paramsCollapsed = !paramsCollapsed;
                    applyVisibilityForStyle(plugin ? plugin->getStyleType() : 0);
                    InvalidateRect(hWnd, nullptr, TRUE);
                    return 0;
                }
                // Test M/S buttons before the power dot and tab body.
                {
                    RECT rcTabs2 = { 0, kHeaderH, kViewWidth, kHeaderH + kTabsH };
                    for (int i = 0; i < kTabCount; ++i) {
                        if (PtInRect(&tabSoloRects[i], POINT{x, y})) {
                            plugin->toggleSolo(i);
                            InvalidateRect(hWnd, &rcTabs2, FALSE);
                            return 0;
                        }
                        if (PtInRect(&tabMuteRects[i], POINT{x, y})) {
                            plugin->toggleMute(i);
                            InvalidateRect(hWnd, &rcTabs2, FALSE);
                            return 0;
                        }
                    }
                }
                // Test M/S buttons before the power dot and tab body.
                {
                    RECT rcTabs2 = { 0, kHeaderH, kViewWidth, kHeaderH + kTabsH };
                    for (int i = 0; i < kTabCount; ++i) {
                        if (PtInRect(&tabSoloRects[i], POINT{x, y})) {
                            plugin->toggleSolo(i);
                            InvalidateRect(hWnd, &rcTabs2, FALSE);
                            return 0;
                        }
                        if (PtInRect(&tabMuteRects[i], POINT{x, y})) {
                            plugin->toggleMute(i);
                            InvalidateRect(hWnd, &rcTabs2, FALSE);
                            return 0;
                        }
                    }
                }
                // First test the power dot â€” toggling does NOT change the
                // currently edited tab.
                for (int i = 0; i < kTabCount; ++i) {
                    if (PtInRect(&tabDotRects[i], POINT{x, y})) {
                        plugin->toggleStyleEnabled(i);
                        RECT rcTabs = { 0, kHeaderH, kViewWidth, kHeaderH + kTabsH };
                        InvalidateRect(hWnd, &rcTabs, FALSE);
                        return 0;
                    }
                }
                // Otherwise tab body click â†’ select that tab (and enable it).
                for (int i = 0; i < kTabCount; ++i) {
                    if (PtInRect(&tabRects[i], POINT{x, y})) {
                        plugin->setStyleType(i); // setStyleType also enables it
                        applyVisibilityForStyle(i);
                        // Refresh all on-screen controls from the new style.
                        refreshAll();
                        populateInstrumentCombo(i);
                        if (hVolSlider)
                            SendMessageW(hVolSlider, TBM_SETPOS, TRUE,
                                (LPARAM)(int)std::round(
                                    plugin->sfVolumePerStyle[i] * 100.0f));
                        refreshVolValue();
                        if (hHumanizeKnob)
                            if (auto* kw = reinterpret_cast<KnobWidget*>(GetWindowLongPtrW(hHumanizeKnob, GWLP_USERDATA)))
                                kw->setPos((int)std::round(plugin->humanizePerStyle[i] * 1000.0f));
                        refreshHumanizeValue();
                        if (hRetardKnob)
                            if (auto* kw = reinterpret_cast<KnobWidget*>(GetWindowLongPtrW(hRetardKnob, GWLP_USERDATA)))
                                kw->setPos((int)std::round(plugin->retardPerStyle[i] * 1000.0f));
                        refreshRetardValue();
                        if (hStartBarCombo) {
                            static constexpr int kSBV[] = { 0,1,2,4,8 };
                            int sv = plugin->startBarPerStyle[i];
                            int si = 0; for (int k=0;k<5;k++) if (kSBV[k]==sv) si=k;
                            SendMessageW(hStartBarCombo, CB_SETCURSEL, si, 0);
                        }
                        // Refresh JAM button for the newly selected tab
                        if (hAutoBtn) {
                            bool jam = plugin->jamPerStyle[i];
                            SetWindowTextW(hAutoBtn,
                                jam ? L"JAM  \u25CF  ON" : L"JAM  \u25CB  OFF");
                            InvalidateRect(hAutoBtn, nullptr, TRUE);
                        }
                        if (hSectionCombo)
                            SendMessageW(hSectionCombo, CB_SETCURSEL,
                                std::clamp(plugin->currentSection.load(std::memory_order_relaxed), 0, 4), 0);
                        if (hProgCombo) SendMessageW(hProgCombo, CB_SETCURSEL,
                                                     plugin->getProgType(), 0);
                        InvalidateRect(hWnd, nullptr, TRUE);
                        break;
                    }
                }
                return 0;
            }
            case WM_MOUSEMOVE: {
                int x = GET_X_LPARAM(l), y = GET_Y_LPARAM(l);
                int newHover = -1;
                for (int i = 0; i < kTabCount; ++i)
                    if (PtInRect(&tabRects[i], POINT{x, y})) { newHover = i; break; }
                if (newHover != hoverTab) {
                    hoverTab = newHover;
                    // Repaint just the tab strip
                    RECT rcTabs = { 0, kHeaderH, kViewWidth, kHeaderH + kTabsH };
                    InvalidateRect(hWnd, &rcTabs, FALSE);
                    if (newHover >= 0) {
                        TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, hWnd, 0 };
                        TrackMouseEvent(&tme);
                    }
                }
                return 0;
            }
            case WM_MOUSELEAVE:
                if (hoverTab != -1) {
                    hoverTab = -1;
                    RECT rcTabs = { 0, kHeaderH, kViewWidth, kHeaderH + kTabsH };
                    InvalidateRect(hWnd, &rcTabs, FALSE);
                }
                return 0;
            case WM_CTLCOLORSTATIC: {
                HDC hdc = (HDC)w;
                HWND ctl = (HWND)l;
                SetBkMode(hdc, TRANSPARENT);
                bool isValue = false;
                for (int i = 0; i < (int)mm::kParamCount; ++i)
                    if (ctl == hValue[i]) { isValue = true; break; }
                if (ctl == hNoteDisplay)      SetTextColor(hdc, kColAccent);
                else if (isValue)             SetTextColor(hdc, kColTextValue);
                else                          SetTextColor(hdc, kColText);
                return (LRESULT)brBg;
            }
            case WM_CTLCOLORBTN:
            case WM_CTLCOLOREDIT:
            case WM_CTLCOLORLISTBOX: {
                HDC hdc = (HDC)w;
                SetBkMode(hdc, OPAQUE);
                SetBkColor(hdc, kColControl);
                SetTextColor(hdc, kColText);
                static HBRUSH brCtl = nullptr;
                if (!brCtl) brCtl = CreateSolidBrush(kColControl);
                return (LRESULT)brCtl;
            }
            case WM_MEASUREITEM: {
                auto* mis = (MEASUREITEMSTRUCT*)l;
                if (mis && mis->CtlType == ODT_COMBOBOX) {
                    mis->itemHeight = 22;
                    return TRUE;
                }
                break;
            }
            case WM_DRAWITEM: {
                auto* dis = (DRAWITEMSTRUCT*)l;
                if (!dis) break;
                if (dis->CtlID == (UINT)kIdFile) {
                    RECT rc = dis->rcItem;
                    bool pressed = (dis->itemState & ODS_SELECTED) != 0;
                    COLORREF fill = (pressed || fileMenuOpen) ? kColAccentDark : RGB(30, 40, 60);
                    HBRUSH br = CreateSolidBrush(fill);
                    FillRect(dis->hDC, &rc, br); DeleteObject(br);
                    HPEN pn = CreatePen(PS_SOLID, 1, fileMenuOpen ? kColAccent : RGB(70, 100, 140));
                    HPEN op = (HPEN)SelectObject(dis->hDC, pn);
                    HBRUSH nb = (HBRUSH)SelectObject(dis->hDC, GetStockObject(NULL_BRUSH));
                    Rectangle(dis->hDC, rc.left, rc.top, rc.right, rc.bottom);
                    SelectObject(dis->hDC, op); DeleteObject(pn);
                    SelectObject(dis->hDC, nb);
                    wchar_t txt[32]; int len = GetWindowTextW(dis->hwndItem, txt, 32);
                    SetBkMode(dis->hDC, TRANSPARENT);
                    SetTextColor(dis->hDC, fileMenuOpen ? kColAccent : kColWhite);
                    HFONT old = (HFONT)SelectObject(dis->hDC, fontBold);
                    DrawTextW(dis->hDC, txt, len, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                    SelectObject(dis->hDC, old);
                    return TRUE;
                }
                if (dis->CtlID == (UINT)kIdExport) {
                    drawAccentButton(dis);
                    return TRUE;
                }
                if (dis->CtlID == (UINT)kIdExportWav) {
                    drawAccentButton(dis);
                    return TRUE;
                }
                if (dis->CtlID == (UINT)kIdSavePreset ||
                    dis->CtlID == (UINT)kIdLoadPreset) {
                    drawAccentButton(dis);
                    return TRUE;
                }
                if (dis->CtlID == (UINT)kIdAuto) {
                    drawListenButton(dis);
                    return TRUE;
                }
                if (dis->CtlID == (UINT)kIdDice) {
                    drawDiceButton(dis);
                    return TRUE;
                }
                if (dis->CtlID == (UINT)kIdLockMode) {
                    int s = plugin->getStyleType();
                    drawLockButton(dis, plugin->lockModePerStyle[s]);
                    return TRUE;
                }
                if (dis->CtlID == (UINT)kIdLockProg) {
                    int s = plugin->getStyleType();
                    drawLockButton(dis, plugin->lockProgPerStyle[s]);
                    return TRUE;
                }
                if (dis->CtlID == (UINT)kIdLockSubdiv) {
                    int s = plugin->getStyleType();
                    drawLockButton(dis, plugin->lockSubdivPerStyle[s]);
                    return TRUE;
                }
                if (dis->CtlID == (UINT)kIdUndo || dis->CtlID == (UINT)kIdRedo) {
                    drawArrowButton(dis, dis->CtlID == (UINT)kIdUndo);
                    return TRUE;
                }
                if (dis->CtlType == ODT_COMBOBOX) {
                    drawComboItem(dis);
                    return TRUE;
                }
                break;
            }
        }
        return DefWindowProcW(h, msg, w, l);
    }

    // ---- Custom painting helpers --------------------------------------------
    void paintBackground(HDC hdc) {
        RECT rc; GetClientRect(hWnd, &rc);

        // Fill base bg
        FillRect(hdc, &rc, brBg);

        // Header gradient
        RECT rcH = { 0, 0, rc.right, kHeaderH };
        TRIVERTEX vtx[2] = {};
        vtx[0].x = rcH.left;  vtx[0].y = rcH.top;
        vtx[0].Red   = (COLOR16)(GetRValue(kColHeader1) << 8);
        vtx[0].Green = (COLOR16)(GetGValue(kColHeader1) << 8);
        vtx[0].Blue  = (COLOR16)(GetBValue(kColHeader1) << 8);
        vtx[1].x = rcH.right; vtx[1].y = rcH.bottom;
        vtx[1].Red   = (COLOR16)(GetRValue(kColHeader2) << 8);
        vtx[1].Green = (COLOR16)(GetGValue(kColHeader2) << 8);
        vtx[1].Blue  = (COLOR16)(GetBValue(kColHeader2) << 8);
        GRADIENT_RECT gr = { 0, 1 };
        GradientFill(hdc, vtx, 2, &gr, 1, GRADIENT_FILL_RECT_V);

        // Title: MIDNIGHT  MELODY  MAKER  FX (four clickable words)
        SetBkMode(hdc, TRANSPARENT);
        HFONT old = (HFONT)SelectObject(hdc, fontTitle);
        const wchar_t* sW0 = L"MIDNIGHT";
        const wchar_t* sW1 = L"  MELODY";
        const wchar_t* sW2 = L"  MAKER";
        const wchar_t* sW3 = L"  FX (beta)";
        SIZE sz0{}, sz1{}, sz2{}, sz3{};
        GetTextExtentPoint32W(hdc, sW0, (int)wcslen(sW0), &sz0);
        GetTextExtentPoint32W(hdc, sW1, (int)wcslen(sW1), &sz1);
        GetTextExtentPoint32W(hdc, sW2, (int)wcslen(sW2), &sz2);
        GetTextExtentPoint32W(hdc, sW3, (int)wcslen(sW3), &sz3);
        int yT = (kHeaderH - sz0.cy) / 2;
        int xT = kPadX + 74;
        int x1 = xT + sz0.cx;
        int x2 = x1 + sz1.cx;
        int x3 = x2 + sz2.cx;

        titleRectMidnight = { xT, 0, x1, kHeaderH };
        titleRectMelody   = { x1, 0, x2, kHeaderH };
        titleRectMaker    = { x2, 0, x3, kHeaderH };
        titleRectFx       = { x3, 0, x3 + sz3.cx, kHeaderH };

        SetTextColor(hdc, viewMode == 0 ? kColWhite   : RGB(120, 140, 165));
        TextOutW(hdc, xT, yT, sW0, (int)wcslen(sW0));
        SetTextColor(hdc, viewMode == 1 ? kColAccent  : RGB(100, 120, 145));
        TextOutW(hdc, x1, yT, sW1, (int)wcslen(sW1));
        SetTextColor(hdc, viewMode == 2 ? kColAccent  : RGB(100, 120, 145));
        TextOutW(hdc, x2, yT, sW2, (int)wcslen(sW2));
        SetTextColor(hdc, viewMode == 3 ? kColAccent  : RGB(100, 120, 145));
        TextOutW(hdc, x3, yT, sW3, (int)wcslen(sW3));

        // Underline indicator
        {
            auto underline = [&](int x0, int x1u, bool active) {
                int yU = yT + sz0.cy + 2;
                HPEN p = CreatePen(active ? PS_SOLID : PS_DOT, active ? 2 : 1,
                                    active ? kColAccent : RGB(70, 85, 105));
                HPEN o = (HPEN)SelectObject(hdc, p);
                MoveToEx(hdc, x0, yU, nullptr); LineTo(hdc, x1u, yU);
                SelectObject(hdc, o); DeleteObject(p);
            };
            underline(xT, x1,           viewMode == 0);
            underline(x1, x2,           viewMode == 1);
            underline(x2, x3,           viewMode == 2);
            underline(x3, x3 + sz3.cx,  viewMode == 3);
        }

        SelectObject(hdc, fontReg);

        // File dropdown background panel (drawn when menu is open)
        if (fileMenuOpen) {
            RECT rcDrop = { 0, kHeaderH, kViewWidth, kHeaderH + 34 };
            HBRUSH brDrop = CreateSolidBrush(RGB(14, 22, 40));
            FillRect(hdc, &rcDrop, brDrop); DeleteObject(brDrop);
            HPEN penD = CreatePen(PS_SOLID, 1, RGB(40, 70, 110));
            HPEN opD  = (HPEN)SelectObject(hdc, penD);
            MoveToEx(hdc, 0, kHeaderH + 33, nullptr);
            LineTo(hdc, kViewWidth, kHeaderH + 33);
            SelectObject(hdc, opD); DeleteObject(penD);
        }

        // Section titles + thin separators
        SelectObject(hdc, fontSection);
        HPEN penSep = CreatePen(PS_SOLID, 1, kColBorder);
        HPEN penOld = (HPEN)SelectObject(hdc, penSep);
        for (int i = 0; i < 4; ++i) {
            int y = sectionY[i];
            if (y <= 0) continue;
            int sx = sectionX[i] > 0 ? sectionX[i] : kPadX;
            int sw = sectionW[i] > 0 ? sectionW[i] : (rc.right - 2 * kPadX);
            SetTextColor(hdc, kColAccent);
            RECT rcS = { sx, y, sx + sw, y + 16 };
            DrawTextW(hdc, sectionTitle(i), -1, &rcS,
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            int sepY = y + 18;
            MoveToEx(hdc, sx, sepY, nullptr);
            LineTo(hdc, sx + sw, sepY);
        }
        SelectObject(hdc, penOld);
        DeleteObject(penSep);

        // "PARAMÈTRES" collapsible header (params mode only)
        if (viewMode == 0) {
            const int yTop = kHeaderH + kTabsH + 4;
            const int hdrH = 22;
            paramsTitleRect = { 0, yTop, rc.right, yTop + hdrH };
            HBRUSH hdrBr = CreateSolidBrush(RGB(28, 34, 44));
            FillRect(hdc, &paramsTitleRect, hdrBr); DeleteObject(hdrBr);
            SetTextColor(hdc, paramsCollapsed ? kColWhite : kColAccent);
            SetBkMode(hdc, TRANSPARENT);
            const wchar_t* arrow = paramsCollapsed ? L"  \u25B6  PARAM\u00c8TRES" : L"  \u25BC  PARAM\u00c8TRES";
            RECT rcHdr = paramsTitleRect;
            DrawTextW(hdc, arrow, -1, &rcHdr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        } else {
            paramsTitleRect = {};
        }

        // ---- SoundFont Maker panel (viewMode == 2) ----
        if (viewMode == 2) {
            SetBkMode(hdc, TRANSPARENT);
            const int yTop = kHeaderH + kTabsH + 8;
            const int mkStyle = plugin ? std::clamp(plugin->getStyleType(), 0, kTabCount - 1) : 0;

            // Instrument + enable toggle header bar
            {
                RECT rcBar = { 0, yTop, kViewWidth, yTop + 36 };
                HBRUSH brBar = CreateSolidBrush(RGB(28, 34, 44));
                FillRect(hdc, &rcBar, brBar); DeleteObject(brBar);

                static const wchar_t* kTabLabels[kTabCount] = {
                    L"M\u00e9lodie", L"Arp\u00e8ge", L"Basse", L"Percu.", L"Contre", L"Piano"
                };
                int s = mkStyle;
                wchar_t hdr[128];
                swprintf(hdr, 128, L"  Instrument : %s", kTabLabels[s]);
                SelectObject(hdc, fontBold);
                SetTextColor(hdc, kColWhite);
                RECT rcInstr = { kPadX, yTop, kViewWidth / 2, yTop + 36 };
                DrawTextW(hdc, hdr, -1, &rcInstr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

                // "SoundFont Personnel ?" label before the button
                SelectObject(hdc, fontReg);
                SetTextColor(hdc, RGB(180, 200, 220));
                RECT rcQ = { kViewWidth / 2, yTop, kViewWidth - kPadX - 114, yTop + 36 };
                DrawTextW(hdc, L"SoundFont personnel ?", -1, &rcQ, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
            }

            // Section dividers (ENVELOPPE / TIMBRE / TESSITURE)
            static const wchar_t* kMkSec[] = { L"ENVELOPPE", L"TIMBRE", L"TESSITURE" };
            const int kMkY[] = { mkEnvY, mkTimbreY, mkTessY };
            SelectObject(hdc, fontSection);
            HPEN penMk = CreatePen(PS_SOLID, 1, kColBorder);
            HPEN penMkOld = (HPEN)SelectObject(hdc, penMk);
            for (int i = 0; i < 3; ++i) {
                int y = kMkY[i];
                if (y <= 0) continue;
                SetTextColor(hdc, kColAccent);
                RECT rcS = { kPadX, y, kViewWidth - kPadX, y + 18 };
                DrawTextW(hdc, kMkSec[i], -1, &rcS, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                MoveToEx(hdc, kPadX + 100, y + 10, nullptr);
                LineTo(hdc, kViewWidth - kPadX, y + 10);
            }
            SelectObject(hdc, penMkOld); DeleteObject(penMk);

            // Row labels (drawn here since STATIC controls can't theme easily)
            const MkStyleDef& mkDef = kMkStyle[mkStyle];
            const wchar_t* kMkRowLabels[] = {
                L"Nom du preset :",
                L"Type de synth\u00e8se :",
                L"Attaque :",
                L"D\u00e9clin :",
                L"Maintien :",
                L"Rel\u00e2chement :",
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
                RECT rcL = { kPadX, ry, kPadX + 150, ry + 24 };
                DrawTextW(hdc, kMkRowLabels[i], -1, &rcL, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
            }
            // Tessiture sub-labels (inline, next to combos)
            if (mkRowY[9] > 0) {
                const int mkLblW = 150;
                const int cmbW   = 130;
                const int gap2   = 20;
                const int lblW2  = 90;
                int ry = mkRowY[9];
                int cx = kPadX + mkLblW + 8;
                SetTextColor(hdc, kColTextDim);
                RECT rcLN = { cx - lblW2 - 4, ry, cx - 4, ry + 24 };
                DrawTextW(hdc, L"Note basse :", -1, &rcLN, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
                cx += cmbW + gap2;
                RECT rcHN = { cx - lblW2 - 4, ry, cx - 4, ry + 24 };
                DrawTextW(hdc, L"Note haute :", -1, &rcHN, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
                cx += cmbW + gap2;
                RECT rcST = { cx - lblW2 - 4, ry, cx - 4, ry + 24 };
                DrawTextW(hdc, L"Pas :", -1, &rcST, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
            }
        }

        // ---- FX panel (viewMode == 3) ----
        if (viewMode == 3) {
            SetBkMode(hdc, TRANSPARENT);
            const int colW  = (kViewWidth - 2 * kPadX - 20) / 2;
            const int col0X = kPadX;
            const int col1X = kPadX + colW + 20;
            const int lblW  = 120;
            const int togW  = 90;

            // Section headers
            static const wchar_t* kFxSec[] = { L"CHORUS", L"D\u00c9LAI", L"R\u00c9VERB\u00c9RATION", L"BRUIT CASSETTE" };
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
                L"Temps :",   L"Réinjection :", L"Mixage :"    // Delay
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
        }

        SelectObject(hdc, old);
    }


    // Paint the 5-tab instrument selector below the header.
    void paintTabs(HDC hdc) {
        RECT rcStrip = { 0, kHeaderH, kViewWidth, kHeaderH + kTabsH };
        HBRUSH brStrip = CreateSolidBrush(kColBg);
        FillRect(hdc, &rcStrip, brStrip);
        DeleteObject(brStrip);

        int active = std::clamp(plugin->getStyleType(), 0, kTabCount - 1);
        int margin = 12;
        int gap    = 6;
        int totalW = kViewWidth - 2 * margin;
        int tabW   = (totalW - (kTabCount - 1) * gap) / kTabCount;
        int tabH   = kTabsH - 12;
        int top    = kHeaderH + 6;
        int radius = 14;

        SetBkMode(hdc, TRANSPARENT);
        HFONT oldF = (HFONT)SelectObject(hdc, fontTab);

        for (int i = 0; i < kTabCount; ++i) {
            int x = margin + i * (tabW + gap);
            tabRects[i] = { x, top, x + tabW, top + tabH };

            bool isActive  = (i == active);
            bool isHover   = (i == hoverTab) && !isActive;
            bool isEnabled = plugin->isStyleEnabled(i);
            COLORREF fill  = isActive ? kColTabActive
                           : isHover  ? kColControlHi
                                      : kColTabBg;
            // Disabled tabs are drawn dimmer to make state obvious.
            COLORREF txt   = isActive ? kColWhite
                           : isEnabled ? kColText
                                       : kColTextDim;

            HBRUSH br = CreateSolidBrush(fill);
            HPEN   pn = CreatePen(PS_SOLID, 1, isActive ? kColTabActive : kColBorder);
            HBRUSH brOld = (HBRUSH)SelectObject(hdc, br);
            HPEN   pnOld = (HPEN)SelectObject(hdc, pn);
            RoundRect(hdc, x, top, x + tabW, top + tabH, radius, radius);
            SelectObject(hdc, brOld);
            SelectObject(hdc, pnOld);
            DeleteObject(br);
            DeleteObject(pn);

            // Power dot (top-left of the tab) â€” click toggles enable.
            int dotR = 7;
            int dotCx = x + 14;
            int dotCy = top + tabH / 2;
            tabDotRects[i] = { dotCx - dotR, dotCy - dotR,
                               dotCx + dotR, dotCy + dotR };
            COLORREF dotFill = isEnabled
                ? (isActive ? kColWhite : kColAccent)
                : kColControl;
            COLORREF dotEdge = isEnabled ? kColAccent : kColTextDim;
            HBRUSH brDot = CreateSolidBrush(dotFill);
            HPEN   pnDot = CreatePen(PS_SOLID, 2, dotEdge);
            HBRUSH brDotOld = (HBRUSH)SelectObject(hdc, brDot);
            HPEN   pnDotOld = (HPEN)SelectObject(hdc, pnDot);
            Ellipse(hdc, tabDotRects[i].left, tabDotRects[i].top,
                         tabDotRects[i].right, tabDotRects[i].bottom);
            SelectObject(hdc, brDotOld);
            SelectObject(hdc, pnDotOld);
            DeleteObject(brDot);
            DeleteObject(pnDot);

            SetTextColor(hdc, txt);
            // Label: reserve space on the right for M/S buttons (14px each + 4px gap)
            RECT rcL = { x + 2 * dotR + 14, top, x + tabW - 34, top + tabH };
            DrawTextW(hdc, tabLabel(i), -1, &rcL,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            // ---- M (Mute) and S (Solo) buttons — stacked right side ----
            {
                bool muted  = plugin->isStyleMuted(i);
                bool soloed = plugin->isStyleSoloed(i);
                bool anySolo = (plugin->soloStyles.load(std::memory_order_relaxed) != 0u);

                int btnW = 14, btnH = (tabH - 6) / 2;
                int btnX = x + tabW - btnW - 4;
                int sY   = top + 3;
                int mY   = sY + btnH + 2;

                tabSoloRects[i] = { btnX, sY,          btnX + btnW, sY + btnH };
                tabMuteRects[i] = { btnX, mY, btnX + btnW, mY + btnH };

                // Helper to draw one pill button
                auto drawBtn = [&](RECT rc2, bool active, COLORREF acol, const wchar_t* lbl) {
                    COLORREF bf = active ? acol : RGB(40, 50, 65);
                    COLORREF ef = active ? acol : RGB(70, 85, 105);
                    COLORREF tf = active ? kColWhite : RGB(130, 145, 165);
                    HBRUSH bb = CreateSolidBrush(bf);
                    HPEN   pb = CreatePen(PS_SOLID, 1, ef);
                    HBRUSH bbo = (HBRUSH)SelectObject(hdc, bb);
                    HPEN   pbo = (HPEN)SelectObject(hdc, pb);
                    RoundRect(hdc, rc2.left, rc2.top, rc2.right, rc2.bottom, 4, 4);
                    SelectObject(hdc, bbo); SelectObject(hdc, pbo);
                    DeleteObject(bb); DeleteObject(pb);
                    SetTextColor(hdc, tf);
                    DrawTextW(hdc, lbl, -1, &rc2, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                };

                // Solo button (S): yellow when active, dimmed when another is soloed
                bool sDim = anySolo && !soloed;
                COLORREF sCol = sDim ? RGB(120, 110, 40) : RGB(255, 220, 50);
                drawBtn(tabSoloRects[i], soloed, sCol, L"S");

                // Mute button (M): red when muted
                drawBtn(tabMuteRects[i], muted, RGB(220, 70, 70), L"M");
            }
        }

        SelectObject(hdc, oldF);
    }

    void drawAccentButton(DRAWITEMSTRUCT* dis) {
        RECT rc = dis->rcItem;
        bool pressed = (dis->itemState & ODS_SELECTED) != 0;
        bool focused = (dis->itemState & ODS_FOCUS)    != 0;
        COLORREF fill = pressed ? kColAccentDark : kColAccent;
        HBRUSH br = CreateSolidBrush(fill);
        FillRect(dis->hDC, &rc, br);
        DeleteObject(br);
        if (focused) {
            HBRUSH brB = CreateSolidBrush(kColWhite);
            FrameRect(dis->hDC, &rc, brB);
            DeleteObject(brB);
        }
        wchar_t txt[128];
        int len = GetWindowTextW(dis->hwndItem, txt, 128);
        SetBkMode(dis->hDC, TRANSPARENT);
        SetTextColor(dis->hDC, kColWhite);
        HFONT old = (HFONT)SelectObject(dis->hDC, fontBold);
        DrawTextW(dis->hDC, txt, len, &rc,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(dis->hDC, old);
    }

    // Owner-draw the LISTEN toggle (filled when on, hollow when off).
    void drawListenButton(DRAWITEMSTRUCT* dis) {
        RECT rc = dis->rcItem;
        bool on = plugin->getAutoKey();
        bool pressed = (dis->itemState & ODS_SELECTED) != 0;

        COLORREF fill = on
            ? (pressed ? kColAccentDark : kColAccent)
            : (pressed ? kColControlHi  : kColControl);
        COLORREF edge = on ? kColAccent : kColTextDim;
        COLORREF txtC = on ? kColWhite  : kColText;

        HBRUSH br = CreateSolidBrush(fill);
        HPEN   pn = CreatePen(PS_SOLID, 1, edge);
        HBRUSH brOld = (HBRUSH)SelectObject(dis->hDC, br);
        HPEN   pnOld = (HPEN)SelectObject(dis->hDC, pn);
        RoundRect(dis->hDC, rc.left, rc.top, rc.right, rc.bottom, 10, 10);
        SelectObject(dis->hDC, brOld);
        SelectObject(dis->hDC, pnOld);
        DeleteObject(br);
        DeleteObject(pn);

        wchar_t txt[128];
        int len = GetWindowTextW(dis->hwndItem, txt, 128);
        SetBkMode(dis->hDC, TRANSPARENT);
        SetTextColor(dis->hDC, txtC);
        HFONT oldF = (HFONT)SelectObject(dis->hDC, fontBold);
        DrawTextW(dis->hDC, txt, len, &rc,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(dis->hDC, oldF);
    }

    // Owner-draw the dice button (small square with a die glyph).
    void drawDiceButton(DRAWITEMSTRUCT* dis) {
        RECT rc = dis->rcItem;
        bool pressed = (dis->itemState & ODS_SELECTED) != 0;
        COLORREF fill = pressed ? kColAccent     : kColControl;
        COLORREF edge = pressed ? kColAccent     : kColTextDim;
        COLORREF txtC = pressed ? kColWhite      : kColAccent;
        HBRUSH br = CreateSolidBrush(fill);
        HPEN   pn = CreatePen(PS_SOLID, 1, edge);
        HBRUSH brOld = (HBRUSH)SelectObject(dis->hDC, br);
        HPEN   pnOld = (HPEN)SelectObject(dis->hDC, pn);
        RoundRect(dis->hDC, rc.left, rc.top, rc.right, rc.bottom, 6, 6);
        SelectObject(dis->hDC, brOld);
        SelectObject(dis->hDC, pnOld);
        DeleteObject(br);
        DeleteObject(pn);

        // Draw a 5-pip die face (top-left, top-right, center,
        // bottom-left, bottom-right) â€” works on any GDI font.
        SetBkMode(dis->hDC, TRANSPARENT);
        HBRUSH dotBr = CreateSolidBrush(txtC);
        HBRUSH dotOld = (HBRUSH)SelectObject(dis->hDC, dotBr);
        HPEN   dotPn  = CreatePen(PS_SOLID, 1, txtC);
        HPEN   dotPnO = (HPEN)SelectObject(dis->hDC, dotPn);
        int cx = (rc.left + rc.right) / 2;
        int cy = (rc.top  + rc.bottom) / 2;
        int r  = 2;
        int dx = (rc.right - rc.left) / 4;
        int dy = (rc.bottom - rc.top) / 4;
        auto pip = [&](int x, int y) {
            Ellipse(dis->hDC, x - r, y - r, x + r + 1, y + r + 1);
        };
        pip(cx - dx, cy - dy);
        pip(cx + dx, cy - dy);
        pip(cx,      cy);
        pip(cx - dx, cy + dy);
        pip(cx + dx, cy + dy);
        SelectObject(dis->hDC, dotOld);
        SelectObject(dis->hDC, dotPnO);
        DeleteObject(dotBr);
        DeleteObject(dotPn);
    }

    // Owner-draw a curved-arrow undo/redo button. `undo` selects ↶, else ↷.
    void drawArrowButton(DRAWITEMSTRUCT* dis, bool undo) {
        RECT rc = dis->rcItem;
        bool pressed = (dis->itemState & ODS_SELECTED) != 0;
        COLORREF fill = pressed ? kColControlHi : kColControl;
        HBRUSH br = CreateSolidBrush(fill);
        HPEN   pn = CreatePen(PS_SOLID, 1, kColTextDim);
        HBRUSH brOld = (HBRUSH)SelectObject(dis->hDC, br);
        HPEN   pnOld = (HPEN)SelectObject(dis->hDC, pn);
        RoundRect(dis->hDC, rc.left, rc.top, rc.right, rc.bottom, 5, 5);
        SelectObject(dis->hDC, brOld);
        SelectObject(dis->hDC, pnOld);
        DeleteObject(br); DeleteObject(pn);

        SetBkMode(dis->hDC, TRANSPARENT);
        SetTextColor(dis->hDC, kColWhite);
        HFONT of = (HFONT)SelectObject(dis->hDC, fontBold);
        const wchar_t* glyph = undo ? L"\u21B6" : L"\u21B7";
        DrawTextW(dis->hDC, glyph, 1, &rc,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(dis->hDC, of);
    }

    // Owner-draw a small padlock toggle. `locked` = closed shackle, accent
    // fill; unlocked = open shackle, neutral fill.
    void drawLockButton(DRAWITEMSTRUCT* dis, bool locked) {
        RECT rc = dis->rcItem;
        bool pressed = (dis->itemState & ODS_SELECTED) != 0;
        COLORREF fill = locked
            ? (pressed ? kColAccentDark : kColAccent)
            : (pressed ? kColControlHi  : kColControl);
        COLORREF edge = locked ? kColAccent : kColTextDim;
        COLORREF glyph = locked ? kColWhite : kColTextDim;

        HBRUSH br = CreateSolidBrush(fill);
        HPEN   pn = CreatePen(PS_SOLID, 1, edge);
        HBRUSH brOld = (HBRUSH)SelectObject(dis->hDC, br);
        HPEN   pnOld = (HPEN)SelectObject(dis->hDC, pn);
        RoundRect(dis->hDC, rc.left, rc.top, rc.right, rc.bottom, 5, 5);
        SelectObject(dis->hDC, brOld);
        SelectObject(dis->hDC, pnOld);
        DeleteObject(br);
        DeleteObject(pn);

        // Draw padlock glyph centered.
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;
        int cx = rc.left + w / 2;
        int cy = rc.top  + h / 2;
        int bodyW = std::max(8, w / 2);
        int bodyH = std::max(6, h / 3);
        int bodyT = cy + 1;                          // body top
        int bodyL = cx - bodyW / 2;
        int bodyR = bodyL + bodyW;
        int bodyB = bodyT + bodyH;
        int archR = bodyW / 2 - 1;                   // shackle radius
        int archCx = cx;
        int archCy = bodyT;

        HPEN gPen   = CreatePen(PS_SOLID, 2, glyph);
        HPEN gPenO  = (HPEN)SelectObject(dis->hDC, gPen);
        HBRUSH gBr  = CreateSolidBrush(glyph);
        HBRUSH gBrO = (HBRUSH)SelectObject(dis->hDC, gBr);

        // Body (filled rounded rect).
        RoundRect(dis->hDC, bodyL, bodyT, bodyR, bodyB, 3, 3);

        // Shackle (arc above the body, half circle).
        SelectObject(dis->hDC, GetStockObject(NULL_BRUSH));
        if (locked) {
            // Closed shackle: half circle anchored on body top.
            Arc(dis->hDC,
                archCx - archR, archCy - archR,
                archCx + archR, archCy + archR,
                archCx - archR, archCy,
                archCx + archR, archCy);
        } else {
            // Open shackle: rotated arc â€” left side detached, right side
            // attached. Draw an arc from ~190Â° to ~10Â° (open at left).
            Arc(dis->hDC,
                archCx - archR + 2, archCy - archR,
                archCx + archR + 2, archCy + archR,
                archCx + archR + 2, archCy,
                archCx - archR + 2, archCy);
        }

        SelectObject(dis->hDC, gBrO);
        SelectObject(dis->hDC, gPenO);
        DeleteObject(gPen);
        DeleteObject(gBr);
    }

    // Owner-draw a combobox item (Omnisphere-style dark dropdown).
    void drawComboItem(DRAWITEMSTRUCT* dis) {
        if ((int)dis->itemID < 0) {
            // Empty / no selection: just clear.
            FillRect(dis->hDC, &dis->rcItem, brBg);
            return;
        }
        bool selected = (dis->itemState & ODS_SELECTED) != 0;
        bool isEdit   = (dis->itemState & ODS_COMBOBOXEDIT) != 0;
        COLORREF bg = isEdit  ? kColControl
                    : selected ? kColAccentDark
                               : kColPanel;
        HBRUSH brI = CreateSolidBrush(bg);
        FillRect(dis->hDC, &dis->rcItem, brI);
        DeleteObject(brI);

        // Subtle separator between items
        if (!isEdit && !selected) {
            HPEN pn = CreatePen(PS_SOLID, 1, kColBorder);
            HPEN po = (HPEN)SelectObject(dis->hDC, pn);
            MoveToEx(dis->hDC, dis->rcItem.left + 4, dis->rcItem.bottom - 1, nullptr);
            LineTo(dis->hDC, dis->rcItem.right - 4, dis->rcItem.bottom - 1);
            SelectObject(dis->hDC, po);
            DeleteObject(pn);
        }

        // -- Compatibility color dot (Mode ↔ Prog) ----------------------------
        // Identify which combo is being drawn and look up the current selection
        // of the other combo to derive the compat rating.
        int compatVal = 0; // 0=neutral 1=green 2=red
        bool isModeCmb = (dis->CtlID == (UINT)(kIdBase + mm::kParamMode));
        bool isProgCmb = (dis->CtlID == (UINT)kIdProg);
        if (isModeCmb && hProgCombo) {
            int curProg = (int)SendMessageW(hProgCombo, CB_GETCURSEL, 0, 0);
            if (curProg != CB_ERR)
                compatVal = mm::mode_prog_compat((int)dis->itemID, curProg);
        } else if (isProgCmb && hCtrl[mm::kParamMode]) {
            int curMode = (int)SendMessageW(hCtrl[mm::kParamMode], CB_GETCURSEL, 0, 0);
            if (curMode != CB_ERR)
                compatVal = mm::mode_prog_compat(curMode, (int)dis->itemID);
        }

        // Draw a small filled circle on the right edge of the item.
        if (compatVal != 0) {
            COLORREF dotCol = (compatVal == 1)
                ? RGB(72, 205, 72)   // green  = recommended
                : RGB(220, 65, 65);  // red    = discouraged
            int itemH   = dis->rcItem.bottom - dis->rcItem.top;
            int dotR    = 4;
            int dotX    = dis->rcItem.right - 12;
            int dotY    = dis->rcItem.top  + itemH / 2;
            HBRUSH brDot = CreateSolidBrush(dotCol);
            HBRUSH brOld = (HBRUSH)SelectObject(dis->hDC, brDot);
            HPEN   pnOld = (HPEN)SelectObject(dis->hDC,
                               CreatePen(PS_SOLID, 1, dotCol));
            Ellipse(dis->hDC,
                    dotX - dotR, dotY - dotR,
                    dotX + dotR, dotY + dotR);
            DeleteObject(SelectObject(dis->hDC, pnOld));
            SelectObject(dis->hDC, brOld);
            DeleteObject(brDot);
        }
        // ---------------------------------------------------------------------

        wchar_t txt[256] = {};
        SendMessageW(dis->hwndItem, CB_GETLBTEXT, dis->itemID, (LPARAM)txt);
        SetBkMode(dis->hDC, TRANSPARENT);
        SetTextColor(dis->hDC, selected ? kColWhite : kColText);
        HFONT old = (HFONT)SelectObject(dis->hDC, fontReg);
        RECT rcText = dis->rcItem;
        rcText.left += 8;
        rcText.right -= (compatVal != 0) ? 20 : 4; // leave room for dot
        DrawTextW(dis->hDC, txt, -1, &rcText,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        SelectObject(dis->hDC, old);
    }

    void createControls() {
        HINSTANCE hi = g_hInst;

        // ---------- Param rows (Key/Mode/Octave/Subdiv/Density/Seed/NoteLen)
        for (int i = 0; i < (int)mm::kParamCount; ++i) {
            wchar_t wname[64];
            MultiByteToWideChar(CP_UTF8, 0, mm::kParamDefs[i].name, -1, wname, 64);
            HWND lbl = CreateWindowW(L"STATIC", wname,
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                0, 0, kLabelW, 20, hWnd, nullptr, hi, nullptr);
            SendMessageW(lbl, WM_SETFONT, (WPARAM)fontReg, TRUE);
            hLabel[i] = lbl;

            int ctrlId = kIdBase + i;
            HWND ctrl = nullptr;
            if (mm::kParamDefs[i].is_stepped) {
                ctrl = CreateWindowW(L"COMBOBOX", L"",
                    WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED |
                    CBS_HASSTRINGS | WS_VSCROLL | WS_TABSTOP,
                    0, 0, kCtrlW, 220, hWnd,
                    (HMENU)(LONG_PTR)ctrlId, hi, nullptr);
                fillCombo(i, ctrl);
            } else {
                // Custom knob widget for Density / NoteLen
                ctrl = KnobWidget::create(hWnd, ctrlId, 0, 0, kKnobSize, kKnobSize);
            }
            if (ctrl && mm::kParamDefs[i].is_stepped)
                SendMessageW(ctrl, WM_SETFONT, (WPARAM)fontReg, TRUE);
            hCtrl[i] = ctrl;

            // Stepped params already display their value inside the
            // combobox â€“ don't add a duplicated value label on the right.
            if (mm::kParamDefs[i].is_stepped) {
                hValue[i] = nullptr;
            } else {
                HWND val = CreateWindowW(L"STATIC", L"",
                    WS_CHILD | WS_VISIBLE | SS_LEFT,
                    0, 0, kValueW, 20, hWnd, nullptr, hi, nullptr);
                SendMessageW(val, WM_SETFONT, (WPARAM)fontBold, TRUE);
                hValue[i] = val;
            }
        }

        // ---------- Sound (waveform) row
        hWaveLabel = CreateWindowW(L"STATIC", L"Instrument",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            0, 0, kLabelW, 20, hWnd, nullptr, hi, nullptr);
        SendMessageW(hWaveLabel, WM_SETFONT, (WPARAM)fontReg, TRUE);
        hWaveCombo = CreateWindowW(L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED |
            CBS_HASSTRINGS | WS_VSCROLL | WS_TABSTOP,
            0, 0, kCtrlW + kValueW, 320, hWnd,
            (HMENU)(LONG_PTR)kIdWave, hi, nullptr);
        SendMessageW(hWaveCombo, WM_SETFONT, (WPARAM)fontReg, TRUE);
        // Items are populated per-style in populateInstrumentCombo().
        populateInstrumentCombo(plugin->getStyleType());

        // ---------- Progression row
        hProgLabel = CreateWindowW(L"STATIC", L"Progr.",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            0, 0, kLabelW, 20, hWnd, nullptr, hi, nullptr);
        SendMessageW(hProgLabel, WM_SETFONT, (WPARAM)fontReg, TRUE);
        hProgCombo = CreateWindowW(L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED |
            CBS_HASSTRINGS | WS_VSCROLL | WS_TABSTOP,
            0, 0, kCtrlW + kValueW, 260, hWnd,
            (HMENU)(LONG_PTR)kIdProg, hi, nullptr);
        SendMessageW(hProgCombo, WM_SETFONT, (WPARAM)fontReg, TRUE);
        for (int i = 0; i < mm::kProgressionCount; ++i) {
            wchar_t wn[96];
            MultiByteToWideChar(CP_UTF8, 0, mm::kProgressionNames[i], -1, wn, 96);
            SendMessageW(hProgCombo, CB_ADDSTRING, 0, (LPARAM)wn);
        }
        SendMessageW(hProgCombo, CB_SETCURSEL, plugin->getProgType(), 0);

        // ---------- Notes display + Export button
        hNotesLabel = CreateWindowW(L"STATIC", L"Notes  \u25B8",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            0, 0, 60, 20, hWnd, nullptr, hi, nullptr);
        SendMessageW(hNotesLabel, WM_SETFONT, (WPARAM)fontReg, TRUE);

        hNoteDisplay = CreateWindowW(L"STATIC", L"\u2014",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            0, 0, kViewWidth - 2 * kPadX - 64, 20, hWnd, nullptr, hi, nullptr);
        SendMessageW(hNoteDisplay, WM_SETFONT, (WPARAM)fontMono, TRUE);

        hExportBtn = CreateWindowW(L"BUTTON", L"EXPORT SESSION  \u2192  MIDI",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW | WS_TABSTOP,
            0, 0, 100, kButtonH, hWnd,
            (HMENU)(LONG_PTR)kIdExport, hi, nullptr);
        SendMessageW(hExportBtn, WM_SETFONT, (WPARAM)fontBold, TRUE);

        hExportWavBtn = CreateWindowW(L"BUTTON", L"EXPORT AUDIO  \u2192  WAV",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW | WS_TABSTOP,
            0, 0, 100, kButtonH, hWnd,
            (HMENU)(LONG_PTR)kIdExportWav, hi, nullptr);
        SendMessageW(hExportWavBtn, WM_SETFONT, (WPARAM)fontBold, TRUE);

        hSavePresetBtn = CreateWindowW(L"BUTTON", L"SAVE PRESET",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW | WS_TABSTOP,
            0, 0, 100, kButtonH, hWnd,
            (HMENU)(LONG_PTR)kIdSavePreset, hi, nullptr);
        SendMessageW(hSavePresetBtn, WM_SETFONT, (WPARAM)fontBold, TRUE);

        hLoadPresetBtn = CreateWindowW(L"BUTTON", L"LOAD PRESET",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW | WS_TABSTOP,
            0, 0, 100, kButtonH, hWnd,
            (HMENU)(LONG_PTR)kIdLoadPreset, hi, nullptr);
        SendMessageW(hLoadPresetBtn, WM_SETFONT, (WPARAM)fontBold, TRUE);

        // ---------- FILE dropdown button (top-left header)
        hFileBtn = CreateWindowW(L"BUTTON", L"FILE  \u25BE",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW | WS_TABSTOP,
            kPadX, (kHeaderH - 26) / 2, 66, 26, hWnd,
            (HMENU)(LONG_PTR)kIdFile, hi, nullptr);
        SendMessageW(hFileBtn, WM_SETFONT, (WPARAM)fontBold, TRUE);

        // ---------- JAM toggle in the header (per-tab)
        hAutoBtn = CreateWindowW(L"BUTTON",
            plugin->jamPerStyle[plugin->getStyleType()] ? L"JAM  \u25CF  ON" : L"JAM  \u25CB  OFF",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW | WS_TABSTOP,
            0, 0, 140, 26, hWnd,
            (HMENU)(LONG_PTR)kIdAuto, hi, nullptr);
        SendMessageW(hAutoBtn, WM_SETFONT, (WPARAM)fontBold, TRUE);

        // ---------- Dice button: randomize Seed of the active style
        hDiceBtn = CreateWindowW(L"BUTTON", L"\U0001F3B2",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW | WS_TABSTOP,
            0, 0, 28, 28, hWnd,
            (HMENU)(LONG_PTR)kIdDice, hi, nullptr);
        SendMessageW(hDiceBtn, WM_SETFONT, (WPARAM)fontBold, TRUE);

        // ---------- Piano: "MÃ©lodie main droite" toggle (BS_AUTOCHECKBOX)
        hPianoMelChk = CreateWindowW(L"BUTTON", L"M\u00e9lodie",
            WS_CHILD | BS_AUTOCHECKBOX | WS_TABSTOP,
            0, 0, 120, 22, hWnd,
            (HMENU)(LONG_PTR)kIdPianoMel, hi, nullptr);
        SendMessageW(hPianoMelChk, WM_SETFONT, (WPARAM)fontReg, TRUE);

        // ---------- Piano: "Accords" toggle
        hPianoChordChk = CreateWindowW(L"BUTTON", L"Accords",
            WS_CHILD | BS_AUTOCHECKBOX | WS_TABSTOP,
            0, 0, 120, 22, hWnd,
            (HMENU)(LONG_PTR)kIdPianoChord, hi, nullptr);
        SendMessageW(hPianoChordChk, WM_SETFONT, (WPARAM)fontReg, TRUE);

        // ---------- Percussion: rhythm pattern picker
        hPercRhyLabel = CreateWindowW(L"STATIC", L"Rythme",
            WS_CHILD | SS_LEFT,
            0, 0, kLabelW, 20, hWnd, nullptr, hi, nullptr);
        SendMessageW(hPercRhyLabel, WM_SETFONT, (WPARAM)fontReg, TRUE);
        hPercRhyCombo = CreateWindowW(L"COMBOBOX", L"",
            WS_CHILD | CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED |
            CBS_HASSTRINGS | WS_VSCROLL | WS_TABSTOP,
            0, 0, 200, 220, hWnd,
            (HMENU)(LONG_PTR)kIdPercRhy, hi, nullptr);
        SendMessageW(hPercRhyCombo, WM_SETFONT, (WPARAM)fontReg, TRUE);
        for (int i = 0; i < mm::kDrumPatternCount; ++i) {
            wchar_t wn[64]; MultiByteToWideChar(CP_UTF8, 0,
                mm::kDrumPatterns[i].name, -1, wn, 64);
            SendMessageW(hPercRhyCombo, CB_ADDSTRING, 0, (LPARAM)wn);
        }
        SendMessageW(hPercRhyCombo, CB_SETCURSEL, 0, 0);

        // ---------- Volume row (per-style SoundFont channel volume)
        hVolLabel = CreateWindowW(L"STATIC", L"Volume",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            0, 0, kLabelW, 20, hWnd, nullptr, hi, nullptr);
        SendMessageW(hVolLabel, WM_SETFONT, (WPARAM)fontReg, TRUE);
        hVolSlider = CreateWindowW(TRACKBAR_CLASS, L"",
            WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS | WS_TABSTOP,
            0, 0, kCtrlW, 24, hWnd,
            (HMENU)(LONG_PTR)kIdVol, hi, nullptr);
        SendMessageW(hVolSlider, TBM_SETRANGE, TRUE, MAKELONG(0, 200));
        SendMessageW(hVolSlider, TBM_SETPOS, TRUE,
            (LPARAM)(int)std::round(plugin->sfVolumePerStyle[plugin->getStyleType()] * 100.0f));
        hVolValue = CreateWindowW(L"STATIC", L"",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            0, 0, kValueW, 20, hWnd, nullptr, hi, nullptr);
        SendMessageW(hVolValue, WM_SETFONT, (WPARAM)fontBold, TRUE);
        refreshVolValue();

        // ---------- Humanize knob (per-style random timing jitter) ----------
        hHumanizeLabel = CreateWindowW(L"STATIC", L"Humanize",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            0, 0, kLabelW, 20, hWnd, nullptr, hi, nullptr);
        SendMessageW(hHumanizeLabel, WM_SETFONT, (WPARAM)fontReg, TRUE);
        hHumanizeKnob = KnobWidget::create(hWnd, kIdHumanize, 0, 0, kKnobSize, kKnobSize);
        { auto* kw = reinterpret_cast<KnobWidget*>(GetWindowLongPtrW(hHumanizeKnob, GWLP_USERDATA));
          if (kw) { kw->posDefault = 0;
            kw->setPos((int)std::round(plugin->humanizePerStyle[plugin->getStyleType()] * 1000.0f)); } }
        hHumanizeValue = CreateWindowW(L"STATIC", L"",
            WS_CHILD | WS_VISIBLE | SS_CENTER,
            0, 0, kValueW, 20, hWnd, nullptr, hi, nullptr);
        SendMessageW(hHumanizeValue, WM_SETFONT, (WPARAM)fontBold, TRUE);
        refreshHumanizeValue();

        // ---------- Retard knob (per-style groove lag) ----------------------
        hRetardLabel = CreateWindowW(L"STATIC", L"Retard",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            0, 0, kLabelW, 20, hWnd, nullptr, hi, nullptr);
        SendMessageW(hRetardLabel, WM_SETFONT, (WPARAM)fontReg, TRUE);
        hRetardKnob = KnobWidget::create(hWnd, kIdRetard, 0, 0, kKnobSize, kKnobSize);
        { auto* kw = reinterpret_cast<KnobWidget*>(GetWindowLongPtrW(hRetardKnob, GWLP_USERDATA));
          if (kw) { kw->posDefault = 0;
            kw->setPos((int)std::round(plugin->retardPerStyle[plugin->getStyleType()] * 1000.0f)); } }
        hRetardValue = CreateWindowW(L"STATIC", L"",
            WS_CHILD | WS_VISIBLE | SS_CENTER,
            0, 0, kValueW, 20, hWnd, nullptr, hi, nullptr);
        SendMessageW(hRetardValue, WM_SETFONT, (WPARAM)fontBold, TRUE);
        refreshRetardValue();

        // ---------- Lock buttons (Mode / Progression / Subdiv) -----------
        // Owner-drawn padlock toggles broadcasting their value to all tabs
        // when locked. Created here, positioned in applyVisibilityForStyle.
        auto makeLock = [&](int id) {
            HWND b = CreateWindowW(L"BUTTON", L"",
                WS_CHILD | BS_PUSHBUTTON | BS_OWNERDRAW | WS_TABSTOP,
                0, 0, 24, 24, hWnd,
                (HMENU)(LONG_PTR)id, hi, nullptr);
            return b;
        };
        hLockMode   = makeLock(kIdLockMode);
        hLockProg   = makeLock(kIdLockProg);
        hLockSubdiv = makeLock(kIdLockSubdiv);

        // ---------- Global: Mesure (time signature numerator) combo
        hMeterCombo = CreateWindowW(L"COMBOBOX", L"",
            WS_CHILD | CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS | WS_VSCROLL | WS_TABSTOP,
            0, 0, 1, 180, hWnd,
            (HMENU)(LONG_PTR)kIdMeter, hi, nullptr);
        SendMessageW(hMeterCombo, WM_SETFONT, (WPARAM)fontReg, TRUE);
        static const wchar_t* meterLabels[] = {
            L"Auto", L"2/4", L"3/4", L"4/4", L"5/4", L"6/4", L"7/4"
        };
        for (auto* lbl : meterLabels)
            SendMessageW(hMeterCombo, CB_ADDSTRING, 0, (LPARAM)lbl);
        // Restore saved override: 0=Auto, 2=2/4 → index 1, 3→2 … 7→6
        int meterIdx = 0;
        if (plugin->beatsPerBarOverride >= 2 && plugin->beatsPerBarOverride <= 7)
            meterIdx = plugin->beatsPerBarOverride - 1; // Auto=0, 2/4=1...
        SendMessageW(hMeterCombo, CB_SETCURSEL, meterIdx, 0);

        // ---- Header: global section combo ----
        // Section and Mesure now live in the RYTHME section (params mode)
        // Create them hidden; applyVisibilityForStyle will position them.
        hSectionLabel = CreateWindowW(L"STATIC", L"Section",
            WS_CHILD | SS_LEFT, 0, 0, 1, 1, hWnd, nullptr, hi, nullptr);
        SendMessageW(hSectionLabel, WM_SETFONT, (WPARAM)fontReg, TRUE);
        hSectionCombo = CreateWindowW(L"COMBOBOX", L"",
            WS_CHILD | CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS | WS_VSCROLL | WS_TABSTOP,
            0, 0, 1, 200, hWnd, (HMENU)(LONG_PTR)kIdSection, hi, nullptr);
        SendMessageW(hSectionCombo, WM_SETFONT, (WPARAM)fontReg, TRUE);
        { const wchar_t* s0[] = { L"Intro", L"Verse", L"Chorus", L"Bridge", L"Outro" };
          for (auto* sl : s0) SendMessageW(hSectionCombo, CB_ADDSTRING, 0, (LPARAM)sl); }
        SendMessageW(hSectionCombo, CB_SETCURSEL,
            std::clamp(plugin->currentSection.load(std::memory_order_relaxed), 0, 4), 0);
        hMeterLabel = CreateWindowW(L"STATIC", L"Mesure",
            WS_CHILD | SS_LEFT, 0, 0, 1, 1, hWnd, nullptr, hi, nullptr);
        SendMessageW(hMeterLabel, WM_SETFONT, (WPARAM)fontReg, TRUE);

        // ---- Per-style: start-bar delay ----
        hStartBarLabel = CreateWindowW(L"STATIC", L"Debut",
            WS_CHILD | SS_LEFT, 0, 0, 1, 1, hWnd, nullptr, hi, nullptr);
        SendMessageW(hStartBarLabel, WM_SETFONT, (WPARAM)fontReg, TRUE);
        hStartBarCombo = CreateWindowW(L"COMBOBOX", L"",
            WS_CHILD | CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS | WS_VSCROLL | WS_TABSTOP,
            0, 0, 1, 200, hWnd, (HMENU)(LONG_PTR)kIdStartBar, hi, nullptr);
        SendMessageW(hStartBarCombo, WM_SETFONT, (WPARAM)fontReg, TRUE);
        { const wchar_t* s1[] = { L"0 bar", L"+1 bar", L"+2 bars", L"+4 bars", L"+8 bars" };
          for (auto* sl : s1) SendMessageW(hStartBarCombo, CB_ADDSTRING, 0, (LPARAM)sl); }
        { static constexpr int kSBV[] = { 0,1,2,4,8 };
          int sv = plugin->startBarPerStyle[plugin->getStyleType()];
          int si = 0; for (int k=0;k<5;k++) if (kSBV[k]==sv) si=k;
          SendMessageW(hStartBarCombo, CB_SETCURSEL, si, 0); }

        // ---------- Miniature horizontal piano roll -------------------------
        ensurePRClass();
        hPianoRoll = CreateWindowExW(0, kPRClass, L"",
            WS_CHILD | WS_VISIBLE,
            kPadX, 0, kViewWidth - 2 * kPadX, kPRHeight,
            hWnd, nullptr, g_hInst, nullptr);
        if (hPianoRoll)
            SetWindowLongPtrW(hPianoRoll, GWLP_USERDATA, (LONG_PTR)this);

        // ---------- Undo / Redo buttons (visible in editor mode only) ----
        hUndoBtn = CreateWindowW(L"BUTTON", L"\u21B6",
            WS_CHILD | BS_PUSHBUTTON | BS_OWNERDRAW | WS_TABSTOP,
            0, 0, 28, 24, hWnd,
            (HMENU)(LONG_PTR)kIdUndo, hi, nullptr);
        SendMessageW(hUndoBtn, WM_SETFONT, (WPARAM)fontBold, TRUE);
        hRedoBtn = CreateWindowW(L"BUTTON", L"\u21B7",
            WS_CHILD | BS_PUSHBUTTON | BS_OWNERDRAW | WS_TABSTOP,
            0, 0, 28, 24, hWnd,
            (HMENU)(LONG_PTR)kIdRedo, hi, nullptr);
        SendMessageW(hRedoBtn, WM_SETFONT, (WPARAM)fontBold, TRUE);

        // ---------- Maker (SoundFont Maker) controls (created hidden) ----------
        // Helper: create a horizontal trackbar (no ticks)
        auto mkSlider = [&](int id, int rangeMin, int rangeMax, int defVal) -> HWND {
            HWND s = CreateWindowW(TRACKBAR_CLASS, L"",
                WS_CHILD | TBS_HORZ | TBS_NOTICKS | WS_TABSTOP,
                0, 0, 460, 22, hWnd, (HMENU)(LONG_PTR)id, hi, nullptr);
            SendMessageW(s, TBM_SETRANGE, TRUE, MAKELONG(rangeMin, rangeMax));
            SendMessageW(s, TBM_SETPOS, TRUE, defVal);
            return s;
        };
        auto mkValLbl = [&]() -> HWND {
            HWND v = CreateWindowW(L"STATIC", L"",
                WS_CHILD | SS_LEFT, 0, 0, 90, 20, hWnd, nullptr, hi, nullptr);
            SendMessageW(v, WM_SETFONT, (WPARAM)fontBold, TRUE);
            return v;
        };
        // Initialize default configs per style from kMkStyle table
        static const char* kMkDefNames[kTabCount] = {
            "Voix Additif", "Harpe KS", "Basse FM", "Perc FM", "Contre FM", "Piano KS"
        };
        static const int kLNvals[] = { 24,30,36,42,48 };
        static const int kHNvals[] = { 60,72,84,96,108 };
        static const int kSTvals[] = { 1,2,3,4,6,12 };
        for (int s = 0; s < kTabCount; ++s) {
            const MkStyleDef& d = kMkStyle[s];
            makerCfg[s].name      = kMkDefNames[s];
            makerCfg[s].synthType = d.synth;
            makerCfg[s].attack    = d.attack;
            makerCfg[s].decay     = d.decay;
            makerCfg[s].sustain   = d.sustain;
            makerCfg[s].release   = d.release;
            makerCfg[s].modIndex  = d.modIndex;
            makerCfg[s].modDecay  = d.modDecay;
            makerCfg[s].gain      = d.gain;
            makerCfg[s].lowNote   = kLNvals[std::clamp(d.lowIdx, 0, 4)];
            makerCfg[s].highNote  = kHNvals[std::clamp(d.highIdx, 0, 4)];
            makerCfg[s].step      = kSTvals[std::clamp(d.stepIdx, 0, 5)];
        }
        // Enable/disable toggle button
        hMakerEnable = CreateWindowW(L"BUTTON", L"\u25cb NON",
            WS_CHILD | BS_PUSHBUTTON | WS_TABSTOP,
            0, 0, 110, 28, hWnd, (HMENU)(LONG_PTR)kIdMakerEnable, hi, nullptr);
        SendMessageW(hMakerEnable, WM_SETFONT, (WPARAM)fontBold, TRUE);
        // Name edit field
        hMakerNameEdit = CreateWindowW(L"EDIT", L"Voix FM",
            WS_CHILD | WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP,
            0, 0, 350, 22, hWnd, (HMENU)(LONG_PTR)kIdMakerName, hi, nullptr);
        SendMessageW(hMakerNameEdit, WM_SETFONT, (WPARAM)fontReg, TRUE);
        // Synth type combo
        hMakerSynthCombo = CreateWindowW(L"COMBOBOX", L"",
            WS_CHILD | CBS_DROPDOWNLIST | CBS_HASSTRINGS | WS_TABSTOP,
            0, 0, 260, 120, hWnd, (HMENU)(LONG_PTR)kIdMakerSynth, hi, nullptr);
        SendMessageW(hMakerSynthCombo, WM_SETFONT, (WPARAM)fontReg, TRUE);
        SendMessageW(hMakerSynthCombo, CB_ADDSTRING, 0, (LPARAM)L"FM \u2014 modulation de fr\u00e9quence");
        SendMessageW(hMakerSynthCombo, CB_ADDSTRING, 0, (LPARAM)L"Additif \u2014 partiels harmoniques");
        SendMessageW(hMakerSynthCombo, CB_ADDSTRING, 0, (LPARAM)L"Karplus-Strong \u2014 cordes pinc\u00e9es");
        SendMessageW(hMakerSynthCombo, CB_SETCURSEL, 0, 0);
        // Envelope sliders: Attack ×0.01→0.01..2s, Decay ×0.01→0.01..3s,
        //                   Sustain ×0.01→0..1,    Release ×0.01→0.01..4s
        hMakerAttackSlider  = mkSlider(kIdMakerAttack,  1, 200,  1); hMakerAttackVal  = mkValLbl();
        hMakerDecaySlider   = mkSlider(kIdMakerDecay,   1, 300, 40); hMakerDecayVal   = mkValLbl();
        hMakerSustainSlider = mkSlider(kIdMakerSustain, 0, 100, 45); hMakerSustainVal = mkValLbl();
        hMakerReleaseSlider = mkSlider(kIdMakerRelease, 1, 400, 60); hMakerReleaseVal = mkValLbl();
        // Timbre sliders: ModIndex ×0.1→0..20, ModDecay ×0.1→0..30, Gain ×0.01→0.01..1
        hMakerModIndexSlider = mkSlider(kIdMakerModIndex, 0, 200, 40); hMakerModIndexVal = mkValLbl();
        hMakerModDecaySlider = mkSlider(kIdMakerModDecay, 0, 300, 60); hMakerModDecayVal = mkValLbl();
        hMakerGainSlider     = mkSlider(kIdMakerGain,     1, 100, 90); hMakerGainVal     = mkValLbl();
        // Tessiture combos
        hMakerLowNoteCombo = CreateWindowW(L"COMBOBOX", L"",
            WS_CHILD | CBS_DROPDOWNLIST | CBS_HASSTRINGS | WS_TABSTOP,
            0, 0, 130, 140, hWnd, (HMENU)(LONG_PTR)kIdMakerLowNote, hi, nullptr);
        SendMessageW(hMakerLowNoteCombo, WM_SETFONT, (WPARAM)fontReg, TRUE);
        { const wchar_t* lns[] = { L"C1 (24)", L"F#1 (30)", L"C2 (36)", L"F#2 (42)", L"C3 (48)" };
          for (auto* s2 : lns) SendMessageW(hMakerLowNoteCombo, CB_ADDSTRING, 0, (LPARAM)s2); }
        SendMessageW(hMakerLowNoteCombo, CB_SETCURSEL, 2, 0); // C2 (36) default
        hMakerHighNoteCombo = CreateWindowW(L"COMBOBOX", L"",
            WS_CHILD | CBS_DROPDOWNLIST | CBS_HASSTRINGS | WS_TABSTOP,
            0, 0, 130, 140, hWnd, (HMENU)(LONG_PTR)kIdMakerHighNote, hi, nullptr);
        SendMessageW(hMakerHighNoteCombo, WM_SETFONT, (WPARAM)fontReg, TRUE);
        { const wchar_t* hns[] = { L"C4 (60)", L"C5 (72)", L"C6 (84)", L"C7 (96)", L"C8 (108)" };
          for (auto* s2 : hns) SendMessageW(hMakerHighNoteCombo, CB_ADDSTRING, 0, (LPARAM)s2); }
        SendMessageW(hMakerHighNoteCombo, CB_SETCURSEL, 3, 0); // C7 (96) default
        hMakerStepCombo = CreateWindowW(L"COMBOBOX", L"",
            WS_CHILD | CBS_DROPDOWNLIST | CBS_HASSTRINGS | WS_TABSTOP,
            0, 0, 190, 140, hWnd, (HMENU)(LONG_PTR)kIdMakerStep, hi, nullptr);
        SendMessageW(hMakerStepCombo, WM_SETFONT, (WPARAM)fontReg, TRUE);
        { const wchar_t* sts[] = {
            L"1 (chromatique)", L"2 (ton)", L"3 (tierce min)",
            L"4 (tierce maj)",  L"6 (sixte)", L"12 (octave)" };
          for (auto* s2 : sts) SendMessageW(hMakerStepCombo, CB_ADDSTRING, 0, (LPARAM)s2); }
        SendMessageW(hMakerStepCombo, CB_SETCURSEL, 3, 0); // step=4 default
        // Generate button
        hMakerGenerateBtn = CreateWindowW(L"BUTTON", L"RAFRA\u00ceCHIR  \u21ba",
            WS_CHILD | BS_PUSHBUTTON | WS_TABSTOP,
            0, 0, 200, 36, hWnd, (HMENU)(LONG_PTR)kIdMakerGenerate, hi, nullptr);
        SendMessageW(hMakerGenerateBtn, WM_SETFONT, (WPARAM)fontBold, TRUE);

        // ---- Éditeur SF2 externe -------------------------------------------
        hMakerLoadSf2Btn = CreateWindowW(L"BUTTON", L"\U0001f4c2 Charger SF2...",
            WS_CHILD | BS_PUSHBUTTON | WS_TABSTOP,
            0, 0, 160, 28, hWnd, (HMENU)(LONG_PTR)kIdMakerLoadSf2, hi, nullptr);
        SendMessageW(hMakerLoadSf2Btn, WM_SETFONT, (WPARAM)fontReg, TRUE);
        hMakerClearSf2Btn = CreateWindowW(L"BUTTON", L"\u2715 Effacer",
            WS_CHILD | BS_PUSHBUTTON | WS_TABSTOP,
            0, 0, 90, 28, hWnd, (HMENU)(LONG_PTR)kIdMakerClearSf2, hi, nullptr);
        SendMessageW(hMakerClearSf2Btn, WM_SETFONT, (WPARAM)fontReg, TRUE);
        hMakerSf2Label = CreateWindowW(L"STATIC", L"(aucun SF2 chargé)",
            WS_CHILD | SS_ENDELLIPSIS,
            0, 0, 340, 18, hWnd, nullptr, hi, nullptr);
        SendMessageW(hMakerSf2Label, WM_SETFONT, (WPARAM)fontReg, TRUE);

        auto mkDeltaSlider = [&](int id, int lo, int hi_, int init) -> HWND {
            HWND s = CreateWindowW(TRACKBAR_CLASSW, L"",
                WS_CHILD | TBS_HORZ | TBS_NOTICKS,
                0, 0, 200, 22, hWnd, (HMENU)(LONG_PTR)id, hi, nullptr);
            SendMessageW(s, TBM_SETRANGE, TRUE, MAKELPARAM(lo, hi_));
            SendMessageW(s, TBM_SETPOS, TRUE, init);
            return s;
        };
        auto mkDeltaLabel = [&]() -> HWND {
            HWND lv = CreateWindowW(L"STATIC", L"0",
                WS_CHILD | SS_RIGHT,
                0, 0, 44, 18, hWnd, nullptr, hi, nullptr);
            SendMessageW(lv, WM_SETFONT, (WPARAM)fontReg, TRUE);
            return lv;
        };
        // Tune: −24..+24 demi-tons, centre = 0 → slider 0..48, centre=24
        hMakerDeltaTuneSlider = mkDeltaSlider(kIdMakerDeltaTune, 0, 48, 24);
        hMakerDeltaTuneVal    = mkDeltaLabel();
        // Timing sliders 0..100, centre=50 = ×1
        hMakerDeltaAtkSlider  = mkDeltaSlider(kIdMakerDeltaAtk,  0, 100, 50);
        hMakerDeltaAtkVal     = mkDeltaLabel();
        hMakerDeltaDecSlider  = mkDeltaSlider(kIdMakerDeltaDec,  0, 100, 50);
        hMakerDeltaDecVal     = mkDeltaLabel();
        hMakerDeltaSusSlider  = mkDeltaSlider(kIdMakerDeltaSus,  0, 100, 50);
        hMakerDeltaSusVal     = mkDeltaLabel();
        hMakerDeltaRelSlider  = mkDeltaSlider(kIdMakerDeltaRel,  0, 100, 50);
        hMakerDeltaRelVal     = mkDeltaLabel();
        hMakerDeltaVolSlider  = mkDeltaSlider(kIdMakerDeltaVol,  0, 100, 50);
        hMakerDeltaVolVal     = mkDeltaLabel();
        hMakerDeltaFiltSlider = mkDeltaSlider(kIdMakerDeltaFilt, 0, 100, 50);
        hMakerDeltaFiltVal    = mkDeltaLabel();

        refreshMakerVals();

        // ---------------------------------------------------------------------
        // FX panel (viewMode == 3) controls — created hidden, shown by
        // applyVisibilityForStyle when viewMode == 3.
        // ---------------------------------------------------------------------
        auto mkFxToggle = [&](int id) -> HWND {
            HWND b = CreateWindowW(L"BUTTON", L"\u25cb NON",
                WS_CHILD | BS_PUSHBUTTON | WS_TABSTOP,
                0, 0, 100, 24, hWnd, (HMENU)(LONG_PTR)id, hi, nullptr);
            SendMessageW(b, WM_SETFONT, (WPARAM)fontBold, TRUE);
            return b;
        };
        auto mkFxSlider = [&](int id, int lo, int hi_, int init) -> HWND {
            HWND s = CreateWindowW(TRACKBAR_CLASSW, L"",
                WS_CHILD | TBS_HORZ | TBS_NOTICKS,
                0, 0, 200, 22, hWnd, (HMENU)(LONG_PTR)id, hi, nullptr);
            SendMessageW(s, TBM_SETRANGE, TRUE, MAKELPARAM(lo, hi_));
            SendMessageW(s, TBM_SETPOS, TRUE, init);
            return s;
        };
        auto mkFxValLbl = [&]() -> HWND {
            HWND v = CreateWindowW(L"STATIC", L"", WS_CHILD | SS_LEFT,
                0, 0, 70, 20, hWnd, nullptr, hi, nullptr);
            SendMessageW(v, WM_SETFONT, (WPARAM)fontBold, TRUE);
            return v;
        };
        // Chorus
        hFxChorusOn       = mkFxToggle(kIdFxChorusOn);
        hFxChorusRate     = mkFxSlider(kIdFxChorusRate,    5,   600, 70);   // 0.05..6 Hz (×0.01)
        hFxChorusRateVal  = mkFxValLbl();
        hFxChorusDepth    = mkFxSlider(kIdFxChorusDepth,   0,   200, 50);   // 0..20 ms (×0.1)
        hFxChorusDepthVal = mkFxValLbl();
        hFxChorusMix      = mkFxSlider(kIdFxChorusMix,     0,   100, 50);   // 0..100%
        hFxChorusMixVal   = mkFxValLbl();
        // Delay
        hFxDelayOn        = mkFxToggle(kIdFxDelayOn);
        hFxDelayTime      = mkFxSlider(kIdFxDelayTime,    10,  1500, 350);  // 10..1500 ms
        hFxDelayTimeVal   = mkFxValLbl();
        hFxDelayFb        = mkFxSlider(kIdFxDelayFb,       0,    92, 40);   // 0..92%
        hFxDelayFbVal     = mkFxValLbl();
        hFxDelayMix       = mkFxSlider(kIdFxDelayMix,      0,   100, 30);   // 0..100%
        hFxDelayMixVal    = mkFxValLbl();
        // Reverb
        hFxReverbOn       = mkFxToggle(kIdFxReverbOn);
        hFxReverbSize     = mkFxSlider(kIdFxReverbSize,    0,   100, 55);   // 0..100%
        hFxReverbSizeVal  = mkFxValLbl();
        hFxReverbDamp     = mkFxSlider(kIdFxReverbDamp,    0,   100, 30);   // 0..100%
        hFxReverbDampVal  = mkFxValLbl();
        hFxReverbMix      = mkFxSlider(kIdFxReverbMix,     0,   100, 25);   // 0..100%
        hFxReverbMixVal   = mkFxValLbl();
        // Cassette noise
        hFxNoiseOn        = mkFxToggle(kIdFxNoiseOn);
        hFxNoiseLevel     = mkFxSlider(kIdFxNoiseLevel,    0,   100, 10);   // 0..100%
        hFxNoiseLevelVal  = mkFxValLbl();
        hFxNoiseFlutter   = mkFxSlider(kIdFxNoiseFlutter,  0,   100, 50);   // 0..100%
        hFxNoiseFlutterVal= mkFxValLbl();
        hFxNoiseTone      = mkFxSlider(kIdFxNoiseTone,     0,   100, 40);   // 0..100%
        hFxNoiseToneVal   = mkFxValLbl();

        applyVisibilityForStyle(plugin->getStyleType());
        computeTitleRects(); // pre-populate rects before first WM_PAINT
    }

    // Pre-compute the three title nav rects using a memory DC + fontTitle.
    // Called at createControls() time AND in paintBackground() to keep in sync.
    void computeTitleRects() {
        HDC dc = CreateCompatibleDC(nullptr);
        HFONT old = (HFONT)SelectObject(dc, fontTitle);
        const wchar_t* sW0 = L"MIDNIGHT";
        const wchar_t* sW1 = L"  MELODY";
        const wchar_t* sW2 = L"  MAKER";
        const wchar_t* sW3 = L"  FX (beta)";
        SIZE sz0{}, sz1{}, sz2{}, sz3{};
        GetTextExtentPoint32W(dc, sW0, (int)wcslen(sW0), &sz0);
        GetTextExtentPoint32W(dc, sW1, (int)wcslen(sW1), &sz1);
        GetTextExtentPoint32W(dc, sW2, (int)wcslen(sW2), &sz2);
        GetTextExtentPoint32W(dc, sW3, (int)wcslen(sW3), &sz3);
        SelectObject(dc, old);
        DeleteDC(dc);
        int xT = kPadX + 74;
        int x1 = xT + sz0.cx;
        int x2 = x1 + sz1.cx;
        int x3 = x2 + sz2.cx;
        titleRectMidnight = { xT, 0, x1, kHeaderH };
        titleRectMelody   = { x1, 0, x2, kHeaderH };
        titleRectMaker    = { x2, 0, x3, kHeaderH };
        titleRectFx       = { x3, 0, x3 + sz3.cx, kHeaderH };
    }

    // -------------------------------------------------------------------------
    // Lay out the visible widgets into 3 sections + tabs at top + bottom export
    //   Tabs (header)  : 5 instruments selector
    //   1. SOUND       : Son
    //   2. RYTHME      : Subdiv, Density (knob), NoteLen (knob)
    //   3. HARMONIE    : Mode, Progr., Seed
    //   Bottom         : Notes display + Export button
    //
    // Key is always hidden (trigger note pitch fixes the tonal centre).
    // Octave is shown as an offset control (+0, +1, +2, -1).
    // -------------------------------------------------------------------------
    // -------------------------------------------------------------------------
    // Maker helpers
    // -------------------------------------------------------------------------
    void hideAllMakerControls() {
        auto h = [](HWND w) { if (w) ShowWindow(w, SW_HIDE); };
        h(hMakerEnable);   h(hMakerSynthCombo); h(hMakerNameEdit);
        h(hMakerAttackSlider);   h(hMakerAttackVal);
        h(hMakerDecaySlider);    h(hMakerDecayVal);
        h(hMakerSustainSlider);  h(hMakerSustainVal);
        h(hMakerReleaseSlider);  h(hMakerReleaseVal);
        h(hMakerModIndexSlider); h(hMakerModIndexVal);
        h(hMakerModDecaySlider); h(hMakerModDecayVal);
        h(hMakerGainSlider);     h(hMakerGainVal);
        h(hMakerLowNoteCombo);   h(hMakerHighNoteCombo);
        h(hMakerStepCombo);      h(hMakerGenerateBtn);
        mkEnvY = 0; mkTimbreY = 0; mkTessY = 0;
    }

    void refreshMakerVals() {
        auto getPos = [&](HWND s) -> int {
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
        setLbl(hMakerGainVal, buf);
    }

    // Rafraîchit les labels des sliders delta SF2 externe.
    void refreshDeltaLabels() {
        auto gp = [&](HWND s) -> int { return s ? (int)SendMessageW(s, TBM_GETPOS, 0, 0) : 0; };
        auto sl = [&](HWND lbl, const wchar_t* txt) { if (lbl) SetWindowTextW(lbl, txt); };
        wchar_t buf[32];
        // Tune : centre=24 → 0, 0→-24, 48→+24
        swprintf(buf, 32, L"%+d", gp(hMakerDeltaTuneSlider) - 24);
        sl(hMakerDeltaTuneVal, buf);
        // Timing : centre=50 → ×1.0
        auto fmtScale = [](wchar_t* b, int pos) {
            float t = (pos - 50) / 50.0f;
            float sc = std::pow(8.0f, t);
            swprintf(b, 32, L"\u00d7%.2f", sc);
        };
        fmtScale(buf, gp(hMakerDeltaAtkSlider));  sl(hMakerDeltaAtkVal, buf);
        fmtScale(buf, gp(hMakerDeltaDecSlider));  sl(hMakerDeltaDecVal, buf);
        fmtScale(buf, gp(hMakerDeltaRelSlider));  sl(hMakerDeltaRelVal, buf);
        // Sustain, volume, filtre : delta centré en 50
        swprintf(buf, 32, L"%+d cb", (gp(hMakerDeltaSusSlider) - 50) * 6);
        sl(hMakerDeltaSusVal, buf);
        swprintf(buf, 32, L"%+d cb", (gp(hMakerDeltaVolSlider) - 50) * 4);
        sl(hMakerDeltaVolVal, buf);
        swprintf(buf, 32, L"%+d ct", (gp(hMakerDeltaFiltSlider) - 50) * 72);
        sl(hMakerDeltaFiltVal, buf);
    }

    // Affiche/cache les contrôles delta SF2 externe selon si un SF2 est chargé.
    void showExternalSf2Controls(bool show) {
        int sw = show ? SW_SHOW : SW_HIDE;
        auto sh = [&](HWND h) { if (h) ShowWindow(h, sw); };
        sh(hMakerClearSf2Btn);
        sh(hMakerDeltaTuneSlider); sh(hMakerDeltaTuneVal);
        sh(hMakerDeltaAtkSlider);  sh(hMakerDeltaAtkVal);
        sh(hMakerDeltaDecSlider);  sh(hMakerDeltaDecVal);
        sh(hMakerDeltaSusSlider);  sh(hMakerDeltaSusVal);
        sh(hMakerDeltaRelSlider);  sh(hMakerDeltaRelVal);
        sh(hMakerDeltaVolSlider);  sh(hMakerDeltaVolVal);
        sh(hMakerDeltaFiltSlider); sh(hMakerDeltaFiltVal);
        InvalidateRect(hWnd, nullptr, TRUE);
    }

    // Lit les sliders delta et met à jour sf2DeltaPerStyle[st] + recharge le SF2.
    void commitDeltaAndReload(int st) {
        if (!plugin || plugin->externalSf2PerStyle[st].empty()) return;
        auto gp = [&](HWND s) -> int { return s ? (int)SendMessageW(s, TBM_GETPOS, 0, 0) : 0; };
        sfed::Sf2Delta& d = plugin->sf2DeltaPerStyle[st];
        d.coarseTune      = gp(hMakerDeltaTuneSlider) - 24;
        d.attackDelta     = sfed::timingSliderToTc(gp(hMakerDeltaAtkSlider));
        d.decayDelta      = sfed::timingSliderToTc(gp(hMakerDeltaDecSlider));
        d.releaseDelta    = sfed::timingSliderToTc(gp(hMakerDeltaRelSlider));
        d.sustainDelta    = sfed::sustainSliderToCb(gp(hMakerDeltaSusSlider));
        d.attenuationDelta= sfed::volumeSliderToCb(gp(hMakerDeltaVolSlider));
        d.filterFcDelta   = sfed::filterSliderToCents(gp(hMakerDeltaFiltSlider));
        refreshDeltaLabels();
        plugin->reloadExternalSf(st);
    }

    void loadMakerCfgFromControls(int style) {
        auto getPos = [&](HWND s) -> int {
            return s ? (int)SendMessageW(s, TBM_GETPOS, 0, 0) : 0;
        };
        sfm::SfmConfig& cfg = makerCfg[style];
        if (hMakerNameEdit) {
            wchar_t wbuf[128] = {};
            GetWindowTextW(hMakerNameEdit, wbuf, 128);
            char nbuf[256] = {};
            WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, nbuf, 256, nullptr, nullptr);
            if (nbuf[0]) cfg.name = nbuf;
        }
        if (hMakerSynthCombo) {
            int idx = (int)SendMessageW(hMakerSynthCombo, CB_GETCURSEL, 0, 0);
            cfg.synthType = (sfm::SfmSynth)std::clamp(idx, 0, 2);
        }
        cfg.attack   = std::max(0.001f, getPos(hMakerAttackSlider)   * 0.01f);
        cfg.decay    = std::max(0.001f, getPos(hMakerDecaySlider)    * 0.01f);
        cfg.sustain  = getPos(hMakerSustainSlider) * 0.01f;
        cfg.release  = std::max(0.001f, getPos(hMakerReleaseSlider)  * 0.01f);
        cfg.modIndex = getPos(hMakerModIndexSlider) * 0.1f;
        cfg.modDecay = getPos(hMakerModDecaySlider) * 0.1f;
        cfg.gain     = std::max(0.01f, getPos(hMakerGainSlider) * 0.01f);
        static const int kLN[] = { 24, 30, 36, 42, 48 };
        if (hMakerLowNoteCombo) {
            int idx = (int)SendMessageW(hMakerLowNoteCombo, CB_GETCURSEL, 0, 0);
            cfg.lowNote = kLN[std::clamp(idx, 0, 4)];
        }
        static const int kHN[] = { 60, 72, 84, 96, 108 };
        if (hMakerHighNoteCombo) {
            int idx = (int)SendMessageW(hMakerHighNoteCombo, CB_GETCURSEL, 0, 0);
            cfg.highNote = kHN[std::clamp(idx, 0, 4)];
        }
        static const int kST[] = { 1, 2, 3, 4, 6, 12 };
        if (hMakerStepCombo) {
            int idx = (int)SendMessageW(hMakerStepCombo, CB_GETCURSEL, 0, 0);
            cfg.step = kST[std::clamp(idx, 0, 5)];
        }
    }

    void refreshMakerFromCfg(int style) {
        const sfm::SfmConfig& cfg = makerCfg[style];
        if (hMakerNameEdit) {
            wchar_t wbuf[256] = {};
            MultiByteToWideChar(CP_UTF8, 0, cfg.name.c_str(), -1, wbuf, 256);
            SetWindowTextW(hMakerNameEdit, wbuf);
        }
        if (hMakerSynthCombo)
            SendMessageW(hMakerSynthCombo, CB_SETCURSEL, (int)cfg.synthType, 0);
        // Apply per-style slider ranges so the delta stays meaningful
        const MkStyleDef& d = kMkStyle[style];
        auto setRange = [&](HWND s, int lo, int hi) {
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
        setS(hMakerGainSlider,     std::clamp((int)std::round(cfg.gain     / 0.01f), d.gaMin,  d.gaMax));
        static const int kLN[] = { 24, 30, 36, 42, 48 };
        if (hMakerLowNoteCombo) {
            int sel = 2;
            for (int i = 0; i < 5; ++i) if (kLN[i] == cfg.lowNote) { sel = i; break; }
            SendMessageW(hMakerLowNoteCombo, CB_SETCURSEL, sel, 0);
        }
        static const int kHN[] = { 60, 72, 84, 96, 108 };
        if (hMakerHighNoteCombo) {
            int sel = 3;
            for (int i = 0; i < 5; ++i) if (kHN[i] == cfg.highNote) { sel = i; break; }
            SendMessageW(hMakerHighNoteCombo, CB_SETCURSEL, sel, 0);
        }
        static const int kST[] = { 1, 2, 3, 4, 6, 12 };
        if (hMakerStepCombo) {
            int sel = 3;
            for (int i = 0; i < 6; ++i) if (kST[i] == cfg.step) { sel = i; break; }
            SendMessageW(hMakerStepCombo, CB_SETCURSEL, sel, 0);
        }
        if (hMakerEnable) SetWindowTextW(hMakerEnable,
            makerEnabled[style] ? L"\u25cf OUI" : L"\u25cb NON");
        refreshMakerVals();
    }

    // -------------------------------------------------------------------------
    // FX helpers
    // -------------------------------------------------------------------------
    void hideAllFxControls() {
        auto h = [](HWND w) { if (w) ShowWindow(w, SW_HIDE); };
        h(hFxChorusOn);    h(hFxChorusRate);   h(hFxChorusRateVal);
        h(hFxChorusDepth); h(hFxChorusDepthVal); h(hFxChorusMix); h(hFxChorusMixVal);
        h(hFxDelayOn);     h(hFxDelayTime);    h(hFxDelayTimeVal);
        h(hFxDelayFb);     h(hFxDelayFbVal);   h(hFxDelayMix);  h(hFxDelayMixVal);
        h(hFxReverbOn);    h(hFxReverbSize);   h(hFxReverbSizeVal);
        h(hFxReverbDamp);  h(hFxReverbDampVal); h(hFxReverbMix); h(hFxReverbMixVal);
        h(hFxNoiseOn);     h(hFxNoiseLevel);   h(hFxNoiseLevelVal);
        h(hFxNoiseFlutter); h(hFxNoiseFlutterVal); h(hFxNoiseTone); h(hFxNoiseToneVal);
        fxChorusY = fxDelayY = fxReverbY = fxNoiseY = 0;
        for (int i = 0; i < 12; ++i) fxRowY[i] = 0;
    }

    static int gp(HWND s) { return s ? (int)SendMessageW(s, TBM_GETPOS, 0, 0) : 0; }

    void refreshFxLabels() {
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
    }

    void refreshFxFromParams(int style) {
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
        setS(hFxNoiseTone,     std::clamp((int)std::round(p.noiseTone * 100.0f), 0, 100));
        if (hFxChorusOn) SetWindowTextW(hFxChorusOn, p.chorusOn ? L"\u25cf OUI" : L"\u25cb NON");
        if (hFxDelayOn)  SetWindowTextW(hFxDelayOn,  p.delayOn  ? L"\u25cf OUI" : L"\u25cb NON");
        if (hFxReverbOn) SetWindowTextW(hFxReverbOn, p.reverbOn ? L"\u25cf OUI" : L"\u25cb NON");
        if (hFxNoiseOn)  SetWindowTextW(hFxNoiseOn,  p.noiseOn  ? L"\u25cf OUI" : L"\u25cb NON");
        refreshFxLabels();
    }

    void commitFxFromControls(int style) {
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
        p.noiseTone   = gp(hFxNoiseTone)   * 0.01f;
        std::lock_guard<std::mutex> lk(plugin->fxMutex);
        plugin->applyFxParamsToChain(style);
    }

    void applyVisibilityForStyle(int style) {
        bool isPerc  = (style == 3);
        bool isPiano = (style == 5);
        bool showMode    = !isPerc;
        bool showNoteLen = !isPerc;
        bool showProg    = !isPerc;
        bool showDensity = !isPerc; // perc rhythm has fixed density (pattern)
        bool showOctave  = !isPerc;
        // Kill maker timer when navigating away from maker view
        if (viewMode != 2 && makerTimerId) { KillTimer(hWnd, makerTimerId); makerTimerId = 0; }

        auto hideRow = [&](HWND lbl, HWND ctl, HWND val) {
            if (lbl) ShowWindow(lbl, SW_HIDE);
            if (ctl) ShowWindow(ctl, SW_HIDE);
            if (val) ShowWindow(val, SW_HIDE);
        };
        for (int i = 0; i < (int)mm::kParamCount; ++i)
            hideRow(hLabel[i], hCtrl[i], hValue[i]);
        hideRow(hWaveLabel, hWaveCombo, nullptr);
        hideRow(hProgLabel, hProgCombo, nullptr);
        hideRow(hVolLabel, hVolSlider, hVolValue);
        hideRow(hHumanizeLabel, hHumanizeKnob, hHumanizeValue);
        hideRow(hRetardLabel, hRetardKnob, hRetardValue);
        if (hPianoMelChk)  ShowWindow(hPianoMelChk,  SW_HIDE);
        if (hPianoChordChk) ShowWindow(hPianoChordChk, SW_HIDE);
        if (hPercRhyLabel) ShowWindow(hPercRhyLabel, SW_HIDE);
        if (hPercRhyCombo) ShowWindow(hPercRhyCombo, SW_HIDE);
        if (hLockMode)     ShowWindow(hLockMode,     SW_HIDE);
        if (hLockProg)     ShowWindow(hLockProg,     SW_HIDE);
        if (hLockSubdiv)   ShowWindow(hLockSubdiv,   SW_HIDE);
        if (hStartBarLabel) ShowWindow(hStartBarLabel, SW_HIDE);
        if (hStartBarCombo) ShowWindow(hStartBarCombo, SW_HIDE);
        if (hSectionLabel)  ShowWindow(hSectionLabel,  SW_HIDE);
        if (hSectionCombo)  ShowWindow(hSectionCombo,  SW_HIDE);
        if (hMeterLabel)    ShowWindow(hMeterLabel,    SW_HIDE);
        if (hMeterCombo)    ShowWindow(hMeterCombo,    SW_HIDE);

        // ---- FILE button: always visible in the header (fixed position) ----
        if (hFileBtn) {
            MoveWindow(hFileBtn, kPadX, (kHeaderH - 26) / 2, 66, 26, TRUE);
            ShowWindow(hFileBtn, SW_SHOW);
            InvalidateRect(hFileBtn, nullptr, TRUE);
        }
        // ---- File dropdown panel: 4 buttons in a horizontal row ----
        // Positioned just below the header as a dropdown overlay.
        {
            const int dropY  = kHeaderH + 2;
            const int dropH  = 30;
            const int totalW = kViewWidth - 2 * kPadX;
            const int gap    = 6;
            const int btnW   = (totalW - 3 * gap) / 4;
            auto posBtn = [&](HWND btn, int i) {
                if (!btn) return;
                if (fileMenuOpen) {
                    MoveWindow(btn, kPadX + i * (btnW + gap), dropY, btnW, dropH, TRUE);
                    ShowWindow(btn, SW_SHOW);
                    InvalidateRect(btn, nullptr, TRUE);
                } else {
                    ShowWindow(btn, SW_HIDE);
                }
            };
            posBtn(hExportBtn,     0);
            posBtn(hExportWavBtn,  1);
            posBtn(hSavePresetBtn, 2);
            posBtn(hLoadPresetBtn, 3);
        }

        // ============ MELODY view (piano roll) ============================================
        if (viewMode == 1) {
            hideAllMakerControls();
            hideAllFxControls();
            if (makerTimerId) { KillTimer(hWnd, makerTimerId); makerTimerId = 0; }
            if (hAutoBtn)        ShowWindow(hAutoBtn,        SW_HIDE);
            if (hMeterCombo)     ShowWindow(hMeterCombo,     SW_HIDE);
            if (hMeterLabel)     ShowWindow(hMeterLabel,     SW_HIDE);
            if (hSectionCombo)   ShowWindow(hSectionCombo,   SW_HIDE);
            if (hSectionLabel)   ShowWindow(hSectionLabel,   SW_HIDE);
            for (int i = 0; i < 4; ++i) { sectionY[i] = 0; sectionX[i] = 0; sectionW[i] = 0; }

            const int yTopE = kHeaderH + kTabsH + kSectionGap;
            int prX = kPadX;
            int prY = yTopE;
            int prWi = kViewWidth - 2 * kPadX;
            int prHe = kViewHeight - yTopE - kSectionGap;
            if (hPianoRoll) {
                MoveWindow(hPianoRoll, prX, prY, prWi, prHe, TRUE);
                ShowWindow(hPianoRoll, SW_SHOW);
            }
            if (hUndoBtn) {
                MoveWindow(hUndoBtn, prX + prWi - 68, prY + prHe - 34, 28, 28, TRUE);
                ShowWindow(hUndoBtn, SW_SHOW);
            }
            if (hRedoBtn) {
                MoveWindow(hRedoBtn, prX + prWi - 36, prY + prHe - 34, 28, 28, TRUE);
                ShowWindow(hRedoBtn, SW_SHOW);
            }
            InvalidateRect(hWnd, nullptr, TRUE);
            return;
        }

        // ============ MAKER view (SoundFont Maker) ============================================
        if (viewMode == 2) {
            hideAllFxControls();
            // Start the auto-refresh timer (fires every 1.2 s)
            if (!makerTimerId) makerTimerId = SetTimer(hWnd, 1201, 1200, nullptr);
            if (hAutoBtn)        ShowWindow(hAutoBtn,        SW_HIDE);
            if (hMeterCombo)     ShowWindow(hMeterCombo,     SW_HIDE);
            if (hMeterLabel)     ShowWindow(hMeterLabel,     SW_HIDE);
            if (hSectionCombo)   ShowWindow(hSectionCombo,   SW_HIDE);
            if (hSectionLabel)   ShowWindow(hSectionLabel,   SW_HIDE);
            if (hPianoRoll)      ShowWindow(hPianoRoll,      SW_HIDE);
            if (hUndoBtn)        ShowWindow(hUndoBtn,        SW_HIDE);
            if (hRedoBtn)        ShowWindow(hRedoBtn,        SW_HIDE);
            for (int i = 0; i < 4; ++i) { sectionY[i] = 0; sectionX[i] = 0; sectionW[i] = 0; }

            // ---- Layout constants ----
            const int mkX     = kPadX;          // left margin
            const int mkW     = kViewWidth - 2 * kPadX; // usable width (864px)
            const int mkLblW  = 150;            // label width
            const int mkSldW  = 480;            // slider width
            const int mkValW  = 90;             // value label width
            const int mkRowH  = 30;             // row height

            int y = kHeaderH + kTabsH + 44;    // start below instrument header bar

            // Enable button (top-right of header bar)
            if (hMakerEnable) {
                MoveWindow(hMakerEnable, kViewWidth - kPadX - 114, kHeaderH + kTabsH + 8, 110, 28, TRUE);
                ShowWindow(hMakerEnable, SW_SHOW);
            }
            // Sync enable button text
            if (hMakerEnable) SetWindowTextW(hMakerEnable,
                makerEnabled[style] ? L"\u25cf OUI" : L"\u25cb NON");

            // ---- Nom du preset ----
            mkRowY[0] = y;
            if (hMakerNameEdit) {
                MoveWindow(hMakerNameEdit, mkX + mkLblW + 8, y, mkSldW, 22, TRUE);
                ShowWindow(hMakerNameEdit, SW_SHOW);
            }
            y += mkRowH;

            // ---- Type de synth\u00e8se ----
            mkRowY[1] = y;
            if (hMakerSynthCombo) {
                MoveWindow(hMakerSynthCombo, mkX + mkLblW + 8, y, 280, 24, TRUE);
                ShowWindow(hMakerSynthCombo, SW_SHOW);
            }
            y += mkRowH + 4;

            // ---- ENVELOPPE section ----
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
            mkRowY[8] = y; placeSliderRow(hMakerGainSlider,     hMakerGainVal,     y); y += mkRowH + 6;

            // ---- TESSITURE section ----
            mkTessY = y;
            y += 22;
            mkRowY[9] = y;
            // Three combos on one row: Note basse | Note haute | Pas
            {
                const int cmbW  = 130;
                const int cmbW2 = 190;
                const int gap   = 20;
                const int lblW2 = 90;
                int cx = mkX + mkLblW + 8;
                if (hMakerLowNoteCombo) {
                    MoveWindow(hMakerLowNoteCombo, cx, y, cmbW, 24, TRUE);
                    ShowWindow(hMakerLowNoteCombo, SW_SHOW);
                }
                cx += cmbW + gap + lblW2 + 4;
                if (hMakerHighNoteCombo) {
                    MoveWindow(hMakerHighNoteCombo, cx, y, cmbW, 24, TRUE);
                    ShowWindow(hMakerHighNoteCombo, SW_SHOW);
                }
                cx += cmbW + gap + lblW2 + 4;
                if (hMakerStepCombo) {
                    MoveWindow(hMakerStepCombo, cx, y, cmbW2, 24, TRUE);
                    ShowWindow(hMakerStepCombo, SW_SHOW);
                }
            }
            y += mkRowH + 10;

            // ---- GÉNÉRER button ----
            if (hMakerGenerateBtn) {
                MoveWindow(hMakerGenerateBtn, mkX, y, 200, 36, TRUE);
                ShowWindow(hMakerGenerateBtn, SW_SHOW);
            }
            y += 44;

            // ---- ÉDITEUR SF2 EXTERNE ----
            // Bouton "Charger SF2..." toujours visible dans Maker
            if (hMakerLoadSf2Btn) {
                MoveWindow(hMakerLoadSf2Btn, mkX, y, 160, 26, TRUE);
                ShowWindow(hMakerLoadSf2Btn, SW_SHOW);
            }
            if (hMakerClearSf2Btn) {
                MoveWindow(hMakerClearSf2Btn, mkX + 166, y, 85, 26, TRUE);
            }
            if (hMakerSf2Label) {
                MoveWindow(hMakerSf2Label, mkX, y + 28, mkSldW + mkValW + 8, 18, TRUE);
                ShowWindow(hMakerSf2Label, SW_SHOW);
            }
            y += 50;
            // Sliders delta — visibles seulement si un SF2 externe est chargé
            bool hasSf2 = plugin && !plugin->externalSf2PerStyle[style].empty();
            if (hasSf2) {
                auto placeDelta = [&](HWND sl, HWND vl) {
                    if (sl) { MoveWindow(sl, mkX + mkLblW + 8, y, mkSldW, 22, TRUE); ShowWindow(sl, SW_SHOW); }
                    if (vl) { MoveWindow(vl, mkX + mkLblW + 8 + mkSldW + 4, y, mkValW, 18, TRUE); ShowWindow(vl, SW_SHOW); }
                    y += mkRowH;
                };
                placeDelta(hMakerDeltaTuneSlider, hMakerDeltaTuneVal);
                placeDelta(hMakerDeltaAtkSlider,  hMakerDeltaAtkVal);
                placeDelta(hMakerDeltaDecSlider,  hMakerDeltaDecVal);
                placeDelta(hMakerDeltaSusSlider,  hMakerDeltaSusVal);
                placeDelta(hMakerDeltaRelSlider,  hMakerDeltaRelVal);
                placeDelta(hMakerDeltaVolSlider,  hMakerDeltaVolVal);
                placeDelta(hMakerDeltaFiltSlider, hMakerDeltaFiltVal);
                ShowWindow(hMakerClearSf2Btn, SW_SHOW);
            } else {
                showExternalSf2Controls(false);
            }
            refreshDeltaLabels();

            refreshMakerFromCfg(style);
            InvalidateRect(hWnd, nullptr, TRUE);
            return;
        }

        // ============ FX view (Chorus / Delay / Reverb / Cassette Noise) ==================
        if (viewMode == 3) {
            hideAllMakerControls();
            if (makerTimerId) { KillTimer(hWnd, makerTimerId); makerTimerId = 0; }
            if (hAutoBtn)      ShowWindow(hAutoBtn,      SW_HIDE);
            if (hMeterCombo)   ShowWindow(hMeterCombo,   SW_HIDE);
            if (hMeterLabel)   ShowWindow(hMeterLabel,   SW_HIDE);
            if (hSectionCombo) ShowWindow(hSectionCombo, SW_HIDE);
            if (hSectionLabel) ShowWindow(hSectionLabel, SW_HIDE);
            if (hPianoRoll)    ShowWindow(hPianoRoll,    SW_HIDE);
            if (hUndoBtn)      ShowWindow(hUndoBtn,      SW_HIDE);
            if (hRedoBtn)      ShowWindow(hRedoBtn,      SW_HIDE);
            for (int i = 0; i < 4; ++i) { sectionY[i] = 0; sectionX[i] = 0; sectionW[i] = 0; }

            // Layout: 2 columns, 2 FX each.
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
            placeTog(col1X, fxNoiseY + 2, hFxNoiseOn);

            refreshFxFromParams(style);
            InvalidateRect(hWnd, nullptr, TRUE);
            return;
        }
        if (hUndoBtn) ShowWindow(hUndoBtn, SW_HIDE);
        if (hRedoBtn) ShowWindow(hRedoBtn, SW_HIDE);
        hideAllMakerControls();
        hideAllFxControls();

        // Place LISTEN toggle in header (now has more room without Section/Mesure)
        if (hAutoBtn) {
            int btnW = 160, btnH = 26;
            MoveWindow(hAutoBtn, kViewWidth - kPadX - btnW,
                       (kHeaderH - btnH) / 2, btnW, btnH, TRUE);
            ShowWindow(hAutoBtn, SW_SHOW);
        }

        // Y where the PARAMÈTRES header ends (drawn at kHeaderH+kTabsH+4, height 22)
        static constexpr int kParamHdrH = 22;
        static constexpr int kParamHdrY = kHeaderH + kTabsH + 4;
        static constexpr int kContentY  = kParamHdrY + kParamHdrH + 4;

        // ---- Collapsed params: show only the piano roll ----
        if (paramsCollapsed) {
            for (int i = 0; i < 4; ++i) { sectionY[i] = 0; sectionX[i] = 0; sectionW[i] = 0; }
            if (hPianoRoll) {
                int prHe = kViewHeight - kContentY - 4;
                MoveWindow(hPianoRoll, kPadX, kContentY, kViewWidth - 2 * kPadX, prHe, TRUE);
                ShowWindow(hPianoRoll, SW_SHOW);
            }
            InvalidateRect(hWnd, nullptr, TRUE);
            return;
        }

        const int yTop = kContentY;

        // ============ LEFT COLUMN: SOUND + RYTHME ============
        int yL = yTop;

        // === SOUND ===
        sectionY[0] = yL; sectionX[0] = kPadX; sectionW[0] = kColumnW;
        yL += kSectionTitleH;
        placeRowCol(yL, hWaveLabel, hWaveCombo, nullptr, kPadX, kColumnW, /*spanCtrl=*/true);
        yL += kRowH;
        placeRowCol(yL, hVolLabel, hVolSlider, hVolValue, kPadX, kColumnW, false);
        yL += kRowH;
        { static constexpr int kSBV[] = { 0,1,2,4,8 };
          int sv2 = plugin->startBarPerStyle[style];
          int si2 = 0; for (int k=0;k<5;k++) if (kSBV[k]==sv2) si2=k;
          if (hStartBarCombo) SendMessageW(hStartBarCombo, CB_SETCURSEL, si2, 0); }
        placeRowCol(yL, hStartBarLabel, hStartBarCombo, nullptr, kPadX, kColumnW, /*spanCtrl=*/true);
        yL += kRowH;
        yL += kSectionGap;

        // === RYTHME ===
        sectionY[1] = yL; sectionX[1] = kPadX; sectionW[1] = kColumnW;
        yL += kSectionTitleH;
        // Section (global) — placed first in RYTHME, spans full control width
        placeRowCol(yL, hSectionLabel, hSectionCombo, nullptr, kPadX, kColumnW, /*spanCtrl=*/true);
        yL += kRowH;
        // Mesure / time signature (global)
        placeRowCol(yL, hMeterLabel, hMeterCombo, nullptr, kPadX, kColumnW, /*spanCtrl=*/true);
        yL += kRowH;
        // Subdiv
        placeRowCol(yL, hLabel[mm::kParamSubdiv], hCtrl[mm::kParamSubdiv], hValue[mm::kParamSubdiv], kPadX, kColumnW, false);
        // Subdiv lock: at the right edge of the Subdiv row.
        if (hLockSubdiv) {
            int lockX = kPadX + kColumnW - 26;
            MoveWindow(hLockSubdiv, lockX, yL + 4, 24, 24, TRUE);
            ShowWindow(hLockSubdiv, SW_SHOW);
            InvalidateRect(hLockSubdiv, nullptr, TRUE);
        }
        yL += kRowH;
        if (showOctave) {
            placeRowCol(yL, hLabel[mm::kParamOctave], hCtrl[mm::kParamOctave], hValue[mm::kParamOctave], kPadX, kColumnW, false);
            yL += kRowH;
        }

        // ============ RIGHT COLUMN: HARMONIE ============
        int yR = yTop;
        sectionY[2] = yR; sectionX[2] = kRightColX; sectionW[2] = kColumnW;
        yR += kSectionTitleH;
        if (showMode) {
            placeRowCol(yR, hLabel[mm::kParamMode], hCtrl[mm::kParamMode], hValue[mm::kParamMode], kRightColX, kColumnW, false);
            if (hLockMode) {
                int lockX = kRightColX + kColumnW - 26;
                MoveWindow(hLockMode, lockX, yR + 4, 24, 24, TRUE);
                ShowWindow(hLockMode, SW_SHOW);
                InvalidateRect(hLockMode, nullptr, TRUE);
            }
            yR += kRowH;
        }
        if (showProg) {
            placeRowCol(yR, hProgLabel, hProgCombo, nullptr, kRightColX, kColumnW, /*spanCtrl=*/true);
            // Shrink the prog combo to leave space for the padlock.
            if (hProgCombo && hLockProg) {
                RECT rc; GetWindowRect(hProgCombo, &rc);
                POINT pt = { rc.left, rc.top };
                ScreenToClient(hWnd, &pt);
                int newW = (rc.right - rc.left) - 30;
                MoveWindow(hProgCombo, pt.x, pt.y, newW, rc.bottom - rc.top, TRUE);
                int lockX = kRightColX + kColumnW - 26;
                MoveWindow(hLockProg, lockX, yR + 4, 24, 24, TRUE);
                ShowWindow(hLockProg, SW_SHOW);
                InvalidateRect(hLockProg, nullptr, TRUE);
            }
            yR += kRowH;
        }
        // Percussion-only: rhythm-pattern picker replaces Mode/Prog rows.
        if (isPerc && hPercRhyCombo) {
            placeRowCol(yR, hPercRhyLabel, hPercRhyCombo, nullptr, kRightColX, kColumnW, /*spanCtrl=*/true);
            // Sync combo selection with this style's stored pattern.
            int idx = std::clamp(plugin->percRhythmPerStyle[style],
                                 0, mm::kDrumPatternCount - 1);
            SendMessageW(hPercRhyCombo, CB_SETCURSEL, idx, 0);
            yR += kRowH;
        }
        placeRowCol(yR, hLabel[mm::kParamSeed], hCtrl[mm::kParamSeed], hValue[mm::kParamSeed], kRightColX, kColumnW, false);
        // Dice button right next to the Seed control.
        if (hDiceBtn && hCtrl[mm::kParamSeed]) {
            RECT rv; GetWindowRect(hCtrl[mm::kParamSeed], &rv);
            POINT pt = { rv.right, rv.top };
            ScreenToClient(hWnd, &pt);
            MoveWindow(hDiceBtn, pt.x + 8, pt.y, 28, 28, TRUE);
            ShowWindow(hDiceBtn, SW_SHOW);
        }
        yR += kRowH;
        // Piano-only: "Accords" + "Melodie" toggles below the Seed row.
        if (isPiano) {
            if (hPianoChordChk) {
                MoveWindow(hPianoChordChk, kRightColX + kLabelW + 8, yR + 4, 100, 22, TRUE);
                SendMessageW(hPianoChordChk, BM_SETCHECK,
                    plugin->pianoChordPerStyle[style] ? BST_CHECKED : BST_UNCHECKED, 0);
                ShowWindow(hPianoChordChk, SW_SHOW);
            }
            if (hPianoMelChk) {
                MoveWindow(hPianoMelChk, kRightColX + kLabelW + 8 + 108, yR + 4, 100, 22, TRUE);
                SendMessageW(hPianoMelChk, BM_SETCHECK,
                    plugin->pianoMelodyPerStyle[style] ? BST_CHECKED : BST_UNCHECKED, 0);
                ShowWindow(hPianoMelChk, SW_SHOW);
            }
            yR += kRowH;
        }
        // ============ EXPRESSION section: 2x2 knob grid in right column ============
        yR += kSectionGap;
        sectionY[3] = yR; sectionX[3] = kRightColX; sectionW[3] = kColumnW;
        yR += kSectionTitleH;
        {
            // Each half-cell is kColumnW/2 wide (210px):
            //   label(62) + gap(6) + knob(56) + gap(6) + value(80) = 210
            const int kHalfW   = kColumnW / 2;
            const int kSmLbl   = 62;
            const int kGap     = 6;
            const int kValW    = kHalfW - kSmLbl - kGap - kKnobSize - kGap; // 80px

            auto placeKnobHalf = [&](int y, int cellX, HWND lbl, HWND knob, HWND val) {
                if (lbl)  { MoveWindow(lbl,  cellX,                       y + (kKnobSize/2) - 8, kSmLbl, 20, TRUE); ShowWindow(lbl,  SW_SHOW); }
                if (knob) { MoveWindow(knob, cellX + kSmLbl + kGap,       y, kKnobSize, kKnobSize,            TRUE); ShowWindow(knob, SW_SHOW); }
                if (val)  { MoveWindow(val,  cellX + kSmLbl + kGap + kKnobSize + kGap, y + (kKnobSize/2) - 8, kValW, 20, TRUE); ShowWindow(val, SW_SHOW); }
            };

            // Sync knob positions from stored per-style values
            if (hHumanizeKnob)
                if (auto* kw = reinterpret_cast<KnobWidget*>(GetWindowLongPtrW(hHumanizeKnob, GWLP_USERDATA)))
                    kw->setPos((int)std::round(plugin->humanizePerStyle[style] * 1000.0f));
            if (hRetardKnob)
                if (auto* kw = reinterpret_cast<KnobWidget*>(GetWindowLongPtrW(hRetardKnob, GWLP_USERDATA)))
                    kw->setPos((int)std::round(plugin->retardPerStyle[style] * 1000.0f));
            refreshHumanizeValue();
            refreshRetardValue();

            // Row 1: Density | NoteLen  (hidden for Perc which has no note lengths)
            if (showDensity || showNoteLen) {
                if (showDensity)
                    placeKnobHalf(yR, kRightColX,           hLabel[mm::kParamDensity], hCtrl[mm::kParamDensity], hValue[mm::kParamDensity]);
                else
                    hideRow(hLabel[mm::kParamDensity], hCtrl[mm::kParamDensity], hValue[mm::kParamDensity]);
                if (showNoteLen)
                    placeKnobHalf(yR, kRightColX + kHalfW,  hLabel[mm::kParamNoteLen],  hCtrl[mm::kParamNoteLen],  hValue[mm::kParamNoteLen]);
                else
                    hideRow(hLabel[mm::kParamNoteLen], hCtrl[mm::kParamNoteLen], hValue[mm::kParamNoteLen]);
                yR += kKnobSize + 8;
            }

            // Row 2: Humanize | Retard  (always shown)
            placeKnobHalf(yR, kRightColX,           hHumanizeLabel, hHumanizeKnob, hHumanizeValue);
            placeKnobHalf(yR, kRightColX + kHalfW,  hRetardLabel,   hRetardKnob,   hRetardValue);
            yR += kKnobSize + 8;
        }
        // ============ Bottom: piano roll (full width) ============
        int yBot = std::max(yL, yR) + kSectionGap;
        // Piano roll: full-width, kPRHeight tall
        if (hPianoRoll) {
            MoveWindow(hPianoRoll, kPadX, yBot, kViewWidth - 2 * kPadX, kPRHeight, TRUE);
            ShowWindow(hPianoRoll, SW_SHOW);
        }
        // Keep legacy text row hidden
        if (hNotesLabel)  ShowWindow(hNotesLabel,  SW_HIDE);
        if (hNoteDisplay) ShowWindow(hNoteDisplay, SW_HIDE);

        InvalidateRect(hWnd, nullptr, TRUE);
    }

    // Place a label + control (+optional value) inside the given column box.
    // If spanCtrl is true, the control fills (colW - labelW - 8) (no value column).
    void placeRowCol(int y, HWND lbl, HWND ctl, HWND val, int colX, int colW, bool spanCtrl) {
        if (lbl) {
            MoveWindow(lbl, colX, y + 6, kLabelW, 20, TRUE);
            ShowWindow(lbl, SW_SHOW);
        }
        int cx = colX + kLabelW + 8;
        int cw = spanCtrl ? (colW - kLabelW - 8) : kCtrlW;
        if (ctl) {
            MoveWindow(ctl, cx, y + 4, cw, 24, TRUE);
            ShowWindow(ctl, SW_SHOW);
        }
        if (val && !spanCtrl) {
            int vx = cx + cw + 8;
            MoveWindow(val, vx, y + 6, kValueW, 20, TRUE);
            ShowWindow(val, SW_SHOW);
        }
    }

    // Knob variant â€” knob is square (kKnobSize), label on left, value on right.
    void placeKnobRowCol(int y, HWND lbl, HWND knob, HWND val, int colX) {
        if (lbl) {
            MoveWindow(lbl, colX, y + (kKnobSize / 2) - 8, kLabelW, 20, TRUE);
            ShowWindow(lbl, SW_SHOW);
        }
        int cx = colX + kLabelW + 8;
        if (knob) {
            MoveWindow(knob, cx, y, kKnobSize, kKnobSize, TRUE);
            ShowWindow(knob, SW_SHOW);
        }
        if (val) {
            int vx = cx + kKnobSize + 12;
            MoveWindow(val, vx, y + (kKnobSize / 2) - 8, kValueW, 20, TRUE);
            ShowWindow(val, SW_SHOW);
        }
    }

    // Old single-column wrappers kept for compatibility (not used in 2-col layout).
    void placeRow(int y, HWND lbl, HWND ctl, HWND val, bool show) {
        if (!show) return;
        placeRowCol(y, lbl, ctl, val, kPadX, kViewWidth - 2 * kPadX, false);
    }
    void placeKnobRow(int y, HWND lbl, HWND knob, HWND val) {
        placeKnobRowCol(y, lbl, knob, val, kPadX);
    }

    void fillCombo(int paramId, HWND combo) {
        SendMessageW(combo, CB_RESETCONTENT, 0, 0);
        switch (paramId) {
            case mm::kParamKey:
                for (int k = 0; k < 12; ++k) {
                    wchar_t wn[8]; MultiByteToWideChar(CP_UTF8, 0, mm::kKeyNames[k], -1, wn, 8);
                    SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)wn);
                }
                break;
            case mm::kParamMode:
                for (int m = 0; m < mm::kScaleCount; ++m) {
                    wchar_t wn[32]; MultiByteToWideChar(CP_UTF8, 0, mm::kModeNames[m], -1, wn, 32);
                    SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)wn);
                }
                break;
            case mm::kParamOctave: {
                // Values 3-6 stored, displayed as offset relative to trigger note
                // (default=4 → +0, 5 → +1, 6 → +2, 3 → -1)
                static const wchar_t* octLabels[] = { L"-1 oct", L"+0 (normal)", L"+1 oct", L"+2 oct" };
                for (auto* lbl : octLabels) SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)lbl);
                break;
            }
            case mm::kParamSubdiv: {
                static const wchar_t* labels[] = { L"1/1 (whole)", L"1/2", L"1/4", L"1/8", L"1/16", L"1/32" };
                for (auto* s : labels) SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)s);
                break;
            }
            case mm::kParamSeed:
                for (int s = 0; s <= 999; ++s) {
                    wchar_t wn[8]; swprintf_s(wn, L"%d", s);
                    SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)wn);
                }
                break;
            default: break;
        }
    }

    void refreshAll() {
        for (int i = 0; i < (int)mm::kParamCount; ++i) refreshOne(i);
    }

    // Rebuild the Instrument combo for the active style and select its
    // currently stored preset index.
    void populateInstrumentCombo(int style) {
        if (!hWaveCombo) return;
        style = std::clamp(style, 0, 5);
        SendMessageW(hWaveCombo, CB_RESETCONTENT, 0, 0);
        const int n = plugin->effectivePresetCount(style);
        for (int i = 0; i < n; ++i) {
            SfPreset p = plugin->effectivePreset(style, i);
            if (!p.name || !*p.name) continue;
            SendMessageW(hWaveCombo, CB_ADDSTRING, 0, (LPARAM)p.name);
        }
        int idx = std::clamp(plugin->sfPresetIdxPerStyle[style], 0, std::max(1, n) - 1);
        SendMessageW(hWaveCombo, CB_SETCURSEL, idx, 0);
    }

    void refreshVolValue() {
        if (!hVolValue) return;
        float v = plugin->sfVolumePerStyle[plugin->getStyleType()];
        wchar_t buf[32];
        swprintf_s(buf, L"%d %%", (int)std::round(v * 100.0f));
        SetWindowTextW(hVolValue, buf);
    }

    void refreshHumanizeValue() {
        if (!hHumanizeValue) return;
        float v = plugin->humanizePerStyle[plugin->getStyleType()];
        wchar_t buf[32];
        swprintf_s(buf, L"%d %%", (int)std::round(v * 100.0f));
        SetWindowTextW(hHumanizeValue, buf);
    }

    void refreshRetardValue() {
        if (!hRetardValue) return;
        float v = plugin->retardPerStyle[plugin->getStyleType()];
        wchar_t buf[32];
        swprintf_s(buf, L"%d %%", (int)std::round(v * 100.0f));
        SetWindowTextW(hRetardValue, buf);
    }

    void refreshOne(int i) {
        double v = plugin->getActualParamValue(i);
        if (mm::kParamDefs[i].is_stepped) {
            int idx = comboIndexFromValue(i, v);
            SendMessageW(hCtrl[i], CB_SETCURSEL, idx, 0);
        } else {
            const auto& d = mm::kParamDefs[i];
            double norm = (v - d.min_value) / std::max(1e-9, d.max_value - d.min_value);
            int pos = (int)std::round(norm * 1000.0);
            wchar_t cls[64] = {};
            GetClassNameW(hCtrl[i], cls, 64);
            if (wcscmp(cls, KnobWidget::kClassName) == 0) {
                auto* kw = reinterpret_cast<KnobWidget*>(GetWindowLongPtrW(hCtrl[i], GWLP_USERDATA));
                if (kw) kw->setPos(pos);
            } else {
                SendMessageW(hCtrl[i], TBM_SETPOS, TRUE, pos);
            }
        }
        updateValueLabel(i, v);
    }

    void updateValueLabel(int i, double v) {
        if (!hValue[i]) return;
        wchar_t buf[64];
        switch (i) {
            case mm::kParamKey: {
                int k = std::clamp((int)std::round(v), 0, 11);
                wchar_t wn[8]; MultiByteToWideChar(CP_UTF8, 0, mm::kKeyNames[k], -1, wn, 8);
                swprintf_s(buf, L"%s", wn); break;
            }
            case mm::kParamMode: {
                int m = std::clamp((int)std::round(v), 0, mm::kScaleCount - 1);
                wchar_t wn[32]; MultiByteToWideChar(CP_UTF8, 0, mm::kModeNames[m], -1, wn, 32);
                swprintf_s(buf, L"%s", wn); break;
            }
            case mm::kParamOctave: swprintf_s(buf, L"%d", (int)std::round(v)); break;
            case mm::kParamSubdiv: swprintf_s(buf, L"1/%d", mm::normalized_subdiv(v)); break;
            case mm::kParamDensity: swprintf_s(buf, L"%.0f%%", v * 100.0); break;
            case mm::kParamNoteLen: swprintf_s(buf, L"%.0f%%", v * 100.0); break;
            case mm::kParamSeed:    swprintf_s(buf, L"%d", (int)std::round(v)); break;
            default:                swprintf_s(buf, L"%.3f", v); break;
        }
        SetWindowTextW(hValue[i], buf);
    }

    int comboIndexFromValue(int paramId, double v) {
        switch (paramId) {
            case mm::kParamKey:    return std::clamp((int)std::round(v), 0, 11);
            case mm::kParamMode:   return std::clamp((int)std::round(v), 0, mm::kScaleCount - 1);
            case mm::kParamOctave: return std::clamp((int)std::round(v) - 3, 0, 3);
            case mm::kParamSubdiv: {
                int s = mm::normalized_subdiv(v);
                if (s == 1)  return 0;
                if (s == 2)  return 1;
                if (s == 4)  return 2;
                if (s == 8)  return 3;
                if (s == 16) return 4;
                return 5; // 32
            }
            case mm::kParamSeed:   return std::clamp((int)std::round(v), 0, 999);
            default: return 0;
        }
    }

    double valueFromComboIndex(int paramId, int idx) {
        switch (paramId) {
            case mm::kParamKey:    return (double)idx;
            case mm::kParamMode:   return (double)idx;
            case mm::kParamOctave: return 3.0 + idx;
            case mm::kParamSubdiv: { static const int s[] = {1,2,4,8,16,32}; return s[std::clamp(idx,0,5)]; }
            case mm::kParamSeed:   return (double)idx;
            default: return 0.0;
        }
    }

    static void pitchToWstr(int16_t p, wchar_t* out, int len) {
        static const wchar_t* names[] =
            {L"C",L"C#",L"D",L"D#",L"E",L"F",L"F#",L"G",L"G#",L"A",L"A#",L"B"};
        if (p < 0 || p > 127) { swprintf_s(out, len, L"?"); return; }
        swprintf_s(out, len, L"%s%d", names[p % 12], (p / 12) - 1);
    }

    void refreshNoteDisplay() {
        if (!hNoteDisplay || !plugin) return;
        uint32_t head = plugin->noteHistHead.load(std::memory_order_relaxed);
        if (head == 0) { SetWindowTextW(hNoteDisplay, L"\u2014"); return; }

        constexpr int N = MelodyMakerVST3::kNoteHistSize;
        int count = (int)std::min(head, (uint32_t)N);
        wchar_t buf[128]{};
        int pos = 0;
        // oldest to newest, left to right
        for (int i = count - 1; i >= 0 && pos < 120; --i) {
            uint32_t idx = (head - 1 - (uint32_t)i + (uint32_t)N * 4) % (uint32_t)N;
            int16_t pitch = plugin->noteHist[idx];
            wchar_t nb[8];
            pitchToWstr(pitch, nb, 8);
            if (pos > 0) { buf[pos++] = L' '; buf[pos++] = 0xBB; buf[pos++] = L' '; }
            int nlen = (int)wcslen(nb);
            wcscpy_s(buf + pos, 128 - pos, nb);
            pos += nlen;
        }
        SetWindowTextW(hNoteDisplay, buf);
    }

    // -----------------------------------------------------------------------
    // MIDI export helpers
    // -----------------------------------------------------------------------
    static void midiVarLen(std::vector<uint8_t>& b, uint32_t v) {
        uint8_t buf[4]; int n = 0;
        do { buf[n++] = (uint8_t)(v & 0x7F); v >>= 7; } while (v);
        for (int i = n - 1; i >= 0; --i)
            b.push_back(i > 0 ? (buf[i] | 0x80) : buf[i]);
    }
    static void midiBE16(std::vector<uint8_t>& b, uint16_t v) {
        b.push_back((uint8_t)(v >> 8)); b.push_back((uint8_t)(v & 0xFF));
    }
    static void midiBE32(std::vector<uint8_t>& b, uint32_t v) {
        b.push_back((uint8_t)(v >> 24)); b.push_back((uint8_t)((v >> 16) & 0xFF));
        b.push_back((uint8_t)((v >> 8) & 0xFF)); b.push_back((uint8_t)(v & 0xFF));
    }

    // Build a standard MIDI file (format 1) from the given recorded notes.
    // Returns an empty vector if notes is empty.
    std::vector<uint8_t> buildMidiBytes(
            const std::vector<MelodyMakerVST3::RecordedNote>& notes) {
        static constexpr uint16_t TPQN = 480;
        if (notes.empty()) return {};

        struct BEv { uint32_t tick; bool isOn; uint8_t pitch; uint8_t vel; };
        constexpr int kStyles = MelodyMakerVST3::kStyleCount;
        std::vector<BEv> perStyle[kStyles];

        for (auto& n : notes) {
            if (n.beatOn < 0.0) continue;
            int s = std::clamp((int)n.style, 0, kStyles - 1);
            uint32_t onTick  = (uint32_t)std::round(n.beatOn * TPQN);
            uint32_t offTick = (uint32_t)std::round((n.beatOn + n.beatLen) * TPQN);
            if (offTick <= onTick) offTick = onTick + 1;
            uint8_t vel = (uint8_t)std::clamp((int)std::round(n.vel * 127.f), 1, 127);
            uint8_t p   = (uint8_t)std::clamp((int)n.pitch, 0, 127);
            perStyle[s].push_back({onTick,  true,  p, vel});
            perStyle[s].push_back({offTick, false, p, 0});
        }

        float bpm = plugin->lastBpm.load(std::memory_order_relaxed);
        if (bpm < 1.f) bpm = 120.f;
        uint32_t usPerBeat = (uint32_t)std::round(60000000.0 / bpm);

        auto pushTC = [](std::vector<uint8_t>& midi,
                         const std::vector<uint8_t>& body) {
            for (char c : {'M','T','r','k'}) midi.push_back((uint8_t)c);
            midiBE32(midi, (uint32_t)body.size());
            midi.insert(midi.end(), body.begin(), body.end());
        };
        auto pushName = [](std::vector<uint8_t>& trk, const char* name) {
            trk.push_back(0x00); trk.push_back(0xFF); trk.push_back(0x03);
            size_t n = std::strlen(name);
            midiVarLen(trk, (uint32_t)n);
            for (size_t i = 0; i < n; ++i) trk.push_back((uint8_t)name[i]);
        };

        std::vector<uint8_t> tempoTrk;
        tempoTrk.push_back(0x00); tempoTrk.push_back(0xFF);
        tempoTrk.push_back(0x58); tempoTrk.push_back(0x04);
        tempoTrk.push_back(0x04); tempoTrk.push_back(0x02);
        tempoTrk.push_back(0x18); tempoTrk.push_back(0x08);
        tempoTrk.push_back(0x00); tempoTrk.push_back(0xFF);
        tempoTrk.push_back(0x51); tempoTrk.push_back(0x03);
        tempoTrk.push_back((usPerBeat >> 16) & 0xFF);
        tempoTrk.push_back((usPerBeat >>  8) & 0xFF);
        tempoTrk.push_back( usPerBeat        & 0xFF);
        pushName(tempoTrk, "Midnight Melody Maker");
        tempoTrk.push_back(0x00); tempoTrk.push_back(0xFF);
        tempoTrk.push_back(0x2F); tempoTrk.push_back(0x00);

        static const uint8_t kProg[kStyles] = { 0, 46, 33, 0, 40, 0 };
        static const char* kName[kStyles] = {
            "Melodie","Harpe","Basse","Percussions","Contre","Piano"
        };
        std::vector<std::vector<uint8_t>> styleTracks;
        for (int s = 0; s < kStyles; ++s) {
            auto& evs = perStyle[s];
            if (evs.empty()) continue;
            std::sort(evs.begin(), evs.end(), [](const BEv& a, const BEv& b) {
                return a.tick != b.tick ? a.tick < b.tick : !a.isOn;
            });
            std::vector<uint8_t> trk;
            pushName(trk, kName[s]);
            uint8_t chan = (s == 3) ? 9 : (uint8_t)std::min(8, s);
            if (s != 3) {
                trk.push_back(0x00);
                trk.push_back(0xC0 | chan);
                trk.push_back(kProg[s]);
            }
            uint32_t prev = 0;
            for (auto& ev : evs) {
                midiVarLen(trk, ev.tick - prev); prev = ev.tick;
                if (ev.isOn) {
                    trk.push_back(0x90 | chan); trk.push_back(ev.pitch); trk.push_back(ev.vel);
                } else {
                    trk.push_back(0x80 | chan); trk.push_back(ev.pitch); trk.push_back(0x00);
                }
            }
            trk.push_back(0x00); trk.push_back(0xFF);
            trk.push_back(0x2F); trk.push_back(0x00);
            styleTracks.push_back(std::move(trk));
        }

        uint16_t numTracks = (uint16_t)(1 + styleTracks.size());
        std::vector<uint8_t> midi;
        for (char c : {'M','T','h','d'}) midi.push_back((uint8_t)c);
        midiBE32(midi, 6); midiBE16(midi, 1);
        midiBE16(midi, numTracks); midiBE16(midi, TPQN);
        pushTC(midi, tempoTrk);
        for (auto& t : styleTracks) pushTC(midi, t);
        return midi;
    }

    void doExportMidi() {
        static constexpr uint16_t TPQN = 480; // ticks per quarter note

        // Snapshot the notes recorded during playback
        std::vector<MelodyMakerVST3::RecordedNote> notes;
        {
            std::lock_guard<std::mutex> lk(plugin->recMtx);
            notes = plugin->recNotes;
        }

        if (notes.empty()) {
            MessageBoxW(hWnd,
                L"Aucune note enregistr\u00e9e.\n\n"
                L"Lance Play dans FL Studio avec des notes dans la piano roll,\n"
                L"laisse g\u00e9n\u00e9rer quelques mesures, stop, puis clique ici.",
                L"Export MIDI", MB_OK | MB_ICONINFORMATION);
            return;
        }

        struct Ev { uint32_t tick; bool isOn; uint8_t pitch; uint8_t vel; };

        // Bucket notes per style (one MIDI track per instrument).
        constexpr int kStyles = MelodyMakerVST3::kStyleCount; // 5
        std::vector<Ev> perStyle[kStyles];

        for (auto& n : notes) {
            if (n.beatOn < 0.0) continue;
            int s = std::clamp((int)n.style, 0, kStyles - 1);
            uint32_t onTick  = (uint32_t)std::round(n.beatOn * TPQN);
            uint32_t offTick = (uint32_t)std::round((n.beatOn + n.beatLen) * TPQN);
            if (offTick <= onTick) offTick = onTick + 1;
            uint8_t vel = (uint8_t)std::clamp((int)std::round(n.vel * 127.f), 1, 127);
            uint8_t p   = (uint8_t)std::clamp((int)n.pitch, 0, 127);
            perStyle[s].push_back({onTick,  true,  p, vel});
            perStyle[s].push_back({offTick, false, p, 0});
        }

        // Embed correct tempo so the MIDI matches the host project BPM
        float bpm = plugin->lastBpm.load(std::memory_order_relaxed);
        if (bpm < 1.f) bpm = 120.f;
        uint32_t usPerBeat = (uint32_t)std::round(60000000.0 / bpm);

        auto pushTrackChunk = [](std::vector<uint8_t>& midi,
                                 const std::vector<uint8_t>& body) {
            for (char c : {'M','T','r','k'}) midi.push_back((uint8_t)c);
            midiBE32(midi, (uint32_t)body.size());
            midi.insert(midi.end(), body.begin(), body.end());
        };

        auto pushTrackName = [](std::vector<uint8_t>& trk, const char* name) {
            trk.push_back(0x00);
            trk.push_back(0xFF); trk.push_back(0x03);
            size_t n = std::strlen(name);
            midiVarLen(trk, (uint32_t)n);
            for (size_t i = 0; i < n; ++i) trk.push_back((uint8_t)name[i]);
        };

        // Track 1: tempo / time signature only.
        std::vector<uint8_t> tempoTrack;
        // Time signature 4/4 (24 MIDI clocks per beat, 8 32nds per quarter)
        tempoTrack.push_back(0x00);
        tempoTrack.push_back(0xFF); tempoTrack.push_back(0x58); tempoTrack.push_back(0x04);
        tempoTrack.push_back(0x04); tempoTrack.push_back(0x02);
        tempoTrack.push_back(0x18); tempoTrack.push_back(0x08);
        // Tempo
        tempoTrack.push_back(0x00);
        tempoTrack.push_back(0xFF); tempoTrack.push_back(0x51); tempoTrack.push_back(0x03);
        tempoTrack.push_back((usPerBeat >> 16) & 0xFF);
        tempoTrack.push_back((usPerBeat >>  8) & 0xFF);
        tempoTrack.push_back( usPerBeat        & 0xFF);
        pushTrackName(tempoTrack, "Midnight Melody Maker");
        // End of track
        tempoTrack.push_back(0x00); tempoTrack.push_back(0xFF);
        tempoTrack.push_back(0x2F); tempoTrack.push_back(0x00);

        // One MTrk per non-empty style. GM program per style (helps host pick a sound).
        // 0=Melodie 1=Harpe 2=Basse 3=Percu 4=Contre 5=Piano
        static const uint8_t kProgram[kStyles] = {
            0,   // 1 Acoustic Grand Piano (Melodie placeholder)
            46,  // 47 Orchestral Harp
            33,  // 34 Electric Bass (finger)
            0,   // unused on ch 9 (Percu)
            40,  // 41 Violin (Contre)
            0,   // 1 Acoustic Grand Piano (Piano)

        };
        static const char* kTrackName[kStyles] = {
            "Melodie", "Harpe", "Basse", "Percussions", "Contre", "Piano"
        };

        std::vector<std::vector<uint8_t>> styleTracks;
        for (int s = 0; s < kStyles; ++s) {
            auto& evs = perStyle[s];
            if (evs.empty()) continue;
            std::sort(evs.begin(), evs.end(), [](const Ev& a, const Ev& b) {
                if (a.tick != b.tick) return a.tick < b.tick;
                return !a.isOn; // note-offs before note-ons at same tick
            });

            std::vector<uint8_t> trk;
            pushTrackName(trk, kTrackName[s]);
            // Channel: percussions on MIDI ch 10 (0x09), others on ch 1..4.
            uint8_t chan = (s == 3) ? 9 : (uint8_t)std::min(8, s);
            // Program change at delta 0 (skip for drums on ch 10).
            if (s != 3) {
                trk.push_back(0x00);
                trk.push_back(0xC0 | chan);
                trk.push_back(kProgram[s]);
            }
            uint32_t prev = 0;
            for (auto& ev : evs) {
                midiVarLen(trk, ev.tick - prev);
                prev = ev.tick;
                if (ev.isOn) {
                    trk.push_back(0x90 | chan);
                    trk.push_back(ev.pitch);
                    trk.push_back(ev.vel);
                } else {
                    trk.push_back(0x80 | chan);
                    trk.push_back(ev.pitch);
                    trk.push_back(0x00);
                }
            }
            // End of track
            trk.push_back(0x00); trk.push_back(0xFF);
            trk.push_back(0x2F); trk.push_back(0x00);
            styleTracks.push_back(std::move(trk));
        }

        // Build MIDI file (Format 1 = synchronous multi-track)
        uint16_t numTracks = (uint16_t)(1 + styleTracks.size());
        std::vector<uint8_t> midi;
        for (char c : {'M','T','h','d'}) midi.push_back((uint8_t)c);
        midiBE32(midi, 6);
        midiBE16(midi, 1);          // format 1
        midiBE16(midi, numTracks);
        midiBE16(midi, TPQN);
        pushTrackChunk(midi, tempoTrack);
        for (auto& t : styleTracks) pushTrackChunk(midi, t);

        // Open Save File dialog so the user can choose name and location
        wchar_t desktop[MAX_PATH]{};
        SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr,
                         SHGFP_TYPE_CURRENT, desktop);

        wchar_t filePath[MAX_PATH] = L"MelodyMaker.mid";
        OPENFILENAMEW ofn{};
        ofn.lStructSize     = sizeof(ofn);
        ofn.hwndOwner       = hWnd;
        ofn.lpstrFilter     = L"Fichiers MIDI (*.mid)\0*.mid\0Tous les fichiers (*.*)\0*.*\0";
        ofn.lpstrFile       = filePath;
        ofn.nMaxFile        = MAX_PATH;
        ofn.lpstrInitialDir = desktop;
        ofn.lpstrTitle      = L"Sauvegarder le MIDI";
        ofn.lpstrDefExt     = L"mid";
        ofn.Flags           = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

        if (!GetSaveFileNameW(&ofn)) return; // user cancelled

        HANDLE hf = CreateFileW(filePath, GENERIC_WRITE, 0, nullptr,
                                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hf == INVALID_HANDLE_VALUE) {
            MessageBoxW(hWnd, L"Impossible de cr\u00e9er le fichier.",
                        L"Export MIDI", MB_OK | MB_ICONERROR);
            return;
        }
        DWORD written = 0;
        WriteFile(hf, midi.data(), (DWORD)midi.size(), &written, nullptr);
        CloseHandle(hf);

        wchar_t msg[MAX_PATH + 384];
        swprintf_s(msg,
            L"Fichier MIDI sauvegard\u00e9 (%d notes, %d pistes, %.0f BPM) :\n%s\n\n"
            L"Format 1 multi-piste : une piste MIDI par instrument activ\u00e9.\n"
            L"Importez dans votre DAW pour retrouver les voix s\u00e9par\u00e9es.",
            (int)notes.size(), (int)styleTracks.size(),
            (double)bpm, filePath);
        MessageBoxW(hWnd, msg, L"Export MIDI \u2014 Multi-piste", MB_OK | MB_ICONINFORMATION);
    }

    // Render the recorded session through TSF + the loaded SoundFont and
    // save the result as a stereo 16-bit WAV. This produces the exact sound
    // currently coming out of the plugin (same SF presets / volumes).
    void doExportWav() {
        // Snapshot the recorded notes
        std::vector<MelodyMakerVST3::RecordedNote> notes;
        {
            std::lock_guard<std::mutex> lk(plugin->recMtx);
            notes = plugin->recNotes;
        }
        if (notes.empty()) {
            MessageBoxW(hWnd,
                L"Aucune note enregistr\u00e9e.\n\n"
                L"Lance Play dans ton DAW, laisse g\u00e9n\u00e9rer quelques mesures,\n"
                L"stop, puis r\u00e9-essaie l'export.",
                L"Export WAV", MB_OK | MB_ICONINFORMATION);
            return;
        }

        float bpm = plugin->lastBpm.load(std::memory_order_relaxed);
        if (bpm < 1.f) bpm = 120.f;
        const int    SR     = 48000;
        const double secsPerBeat = 60.0 / (double)bpm;

        // Total length in samples (longest note end + 2 s tail for releases).
        double endBeats = 0.0;
        for (const auto& n : notes) {
            if (n.beatOn < 0.0) continue;
            double e = n.beatOn + n.beatLen;
            if (e > endBeats) endBeats = e;
        }
        const int64_t totalSamples =
            (int64_t)std::ceil((endBeats * secsPerBeat + 2.0) * SR);
        if (totalSamples <= 0) return;

        // Ask the user where to save before doing the heavy work.
        wchar_t desktop[MAX_PATH]{};
        SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr,
                         SHGFP_TYPE_CURRENT, desktop);
        wchar_t filePath[MAX_PATH] = L"MelodyMaker.wav";
        OPENFILENAMEW ofn{};
        ofn.lStructSize     = sizeof(ofn);
        ofn.hwndOwner       = hWnd;
        ofn.lpstrFilter     = L"Fichiers WAV (*.wav)\0*.wav\0Tous les fichiers (*.*)\0*.*\0";
        ofn.lpstrFile       = filePath;
        ofn.nMaxFile        = MAX_PATH;
        ofn.lpstrInitialDir = desktop;
        ofn.lpstrTitle      = L"Sauvegarder le rendu audio (WAV)";
        ofn.lpstrDefExt     = L"wav";
        ofn.Flags           = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
        if (!GetSaveFileNameW(&ofn)) return;

        // Locate the bundled SF2 (same logic as MelodyMakerVST3::loadSoundFont).
        wchar_t modPath[MAX_PATH] = {};
        if (!GetModuleFileNameW(g_hInst, modPath, MAX_PATH)) {
            MessageBoxW(hWnd, L"Module introuvable.", L"Export WAV",
                        MB_OK | MB_ICONERROR);
            return;
        }
        for (int i = (int)wcslen(modPath) - 1; i >= 0; --i) {
            if (modPath[i] == L'\\' || modPath[i] == L'/') { modPath[i] = 0; break; }
        }
        std::wstring sfPath = std::wstring(modPath) + L"\\..\\Resources\\GeneralUser.sf2";
        std::wstring sfPathLofi = std::wstring(modPath) + L"\\..\\Resources\\Midnight\\midnight_lofi.sf2";
        auto loadOffline = [&](const std::wstring& wp) -> tsf* {
            char p8[MAX_PATH * 4] = {};
            WideCharToMultiByte(CP_ACP, 0, wp.c_str(), -1, p8, sizeof(p8), nullptr, nullptr);
            tsf* t = tsf_load_filename(p8);
            if (!t) return nullptr;
            tsf_set_output(t, TSF_STEREO_INTERLEAVED, SR, -3.0f);
            tsf_set_max_voices(t, 64);
            return t;
        };

        // Spin up private TSF instances so we don't disturb live playback.
        tsf* offline     = loadOffline(sfPath);
        tsf* offlineLofi = loadOffline(sfPathLofi);  // optional
        if (!offline) {
            MessageBoxW(hWnd, L"Chargement de la SoundFont impossible.",
                        L"Export WAV", MB_OK | MB_ICONERROR);
            return;
        }
        // Cache offline TSF per user SF2 path, lazily.
        std::vector<std::pair<std::wstring, tsf*>> offlineUser;
        auto getOfflineUser = [&](const std::wstring& path) -> tsf* {
            for (auto& kv : offlineUser) if (kv.first == path) return kv.second;
            tsf* t = loadOffline(path);
            offlineUser.emplace_back(path, t);
            return t;
        };
        // Per-style routing: which offline synth handles each style's notes.
        tsf* styleSynth[MelodyMakerVST3::kStyleCount] = {};
        for (int s = 0; s < MelodyMakerVST3::kStyleCount; ++s) {
            int ch  = MelodyMakerVST3::kSfChannelOf(s);
            int idx = std::clamp(plugin->sfPresetIdxPerStyle[s],
                                 0, std::max(1, plugin->effectivePresetCount(s)) - 1);
            SfPreset p = plugin->effectivePreset(s, idx);
            tsf* t = nullptr;
            if (p.source == 2) {
                MelodyMakerVST3::UserSfEntry* u = plugin->findUserSfFor(s, idx);
                if (u) t = getOfflineUser(u->sf2Path);
            }
            if (!t) t = (p.source == 1 && offlineLofi) ? offlineLofi : offline;
            styleSynth[s] = t;
            tsf_channel_set_presetnumber(t, ch, p.program,
                                         p.bank == 128 ? 1 : 0);
            tsf_channel_set_volume(t, ch, plugin->sfVolumePerStyle[s]);
        }

        // Build a sorted event list (note-off before note-on at same sample).
        struct Ev { int64_t samp; bool isOn; uint8_t pitch; uint8_t vel; uint8_t chan; uint8_t style; };
        std::vector<Ev> evs;
        evs.reserve(notes.size() * 2);
        for (const auto& n : notes) {
            if (n.beatOn < 0.0) continue;
            int s = std::clamp((int)n.style, 0,
                               MelodyMakerVST3::kStyleCount - 1);
            uint8_t ch  = (uint8_t)MelodyMakerVST3::kSfChannelOf(s);
            uint8_t p   = (uint8_t)std::clamp((int)n.pitch, 0, 127);
            uint8_t v   = (uint8_t)std::clamp((int)std::round(n.vel * 127.f),
                                              1, 127);
            int64_t on  = (int64_t)std::round(n.beatOn * secsPerBeat * SR);
            int64_t off = (int64_t)std::round(
                (n.beatOn + n.beatLen) * secsPerBeat * SR);
            if (off <= on) off = on + 1;
            evs.push_back({on,  true,  p, v, ch, (uint8_t)s});
            evs.push_back({off, false, p, 0, ch, (uint8_t)s});
        }
        std::sort(evs.begin(), evs.end(), [](const Ev& a, const Ev& b) {
            if (a.samp != b.samp) return a.samp < b.samp;
            return !a.isOn; // offs first
        });

        // Render in chunks, writing interleaved stereo float into a buffer.
        std::vector<float> pcm((size_t)totalSamples * 2, 0.0f);
        std::vector<float> tmp(0);
        if (offlineLofi) tmp.resize(2 * 1024);
        size_t outPos = 0;
        size_t evIdx  = 0;
        const size_t kChunk = 1024;
        while (outPos < (size_t)totalSamples) {
            // Fire all events whose sample <= outPos.
            while (evIdx < evs.size() && evs[evIdx].samp <= (int64_t)outPos) {
                const Ev& e = evs[evIdx++];
                tsf* t = styleSynth[e.style];
                if (e.isOn) tsf_channel_note_on (t, e.chan, e.pitch, e.vel / 127.f);
                else        tsf_channel_note_off(t, e.chan, e.pitch);
            }
            // Render up to the next event (or chunk size).
            size_t nextEvSamp = (evIdx < evs.size())
                ? (size_t)evs[evIdx].samp : (size_t)totalSamples;
            size_t want = std::min<size_t>(kChunk,
                std::min<size_t>(nextEvSamp - outPos,
                                 (size_t)totalSamples - outPos));
            if (want == 0) want = 1;
            // Render GU into pcm directly.
            tsf_render_float(offline, pcm.data() + outPos * 2, (int)want, 0);
            // Render Lofi into tmp and mix-add into pcm.
            if (offlineLofi) {
                if (tmp.size() < want * 2) tmp.resize(want * 2);
                tsf_render_float(offlineLofi, tmp.data(), (int)want, 0);
                float* dst = pcm.data() + outPos * 2;
                for (size_t i = 0; i < want * 2; ++i) dst[i] += tmp[i];
            }
            // Render each user-SF offline TSF and mix-add as well.
            for (auto& kv : offlineUser) {
                if (!kv.second) continue;
                if (tmp.size() < want * 2) tmp.resize(want * 2);
                tsf_render_float(kv.second, tmp.data(), (int)want, 0);
                float* dst = pcm.data() + outPos * 2;
                for (size_t i = 0; i < want * 2; ++i) dst[i] += tmp[i];
            }
            outPos += want;
        }
        tsf_close(offline);
        if (offlineLofi) tsf_close(offlineLofi);
        for (auto& kv : offlineUser) if (kv.second) tsf_close(kv.second);

        // Find peak for normalisation hint (do NOT actually normalise â€” keep
        // the same level the user hears live).
        float peak = 0.f;
        for (float v : pcm) { float a = std::fabs(v); if (a > peak) peak = a; }
        // Soft clip prevention.
        float gain = 1.0f;
        if (peak > 0.98f) gain = 0.98f / peak;

        // Convert to 16-bit PCM little-endian.
        std::vector<int16_t> pcm16(pcm.size());
        for (size_t i = 0; i < pcm.size(); ++i) {
            float v = std::clamp(pcm[i] * gain, -1.0f, 1.0f);
            pcm16[i] = (int16_t)std::lrint(v * 32767.0f);
        }

        // Build minimal WAV (PCM16 stereo).
        const uint16_t channels   = 2;
        const uint32_t byteRate   = (uint32_t)SR * channels * 2;
        const uint16_t blockAlign = channels * 2;
        const uint32_t dataBytes  = (uint32_t)(pcm16.size() * sizeof(int16_t));
        std::vector<uint8_t> wav;
        auto put32 = [&](uint32_t v) {
            wav.push_back((uint8_t)(v & 0xFF));
            wav.push_back((uint8_t)((v >>  8) & 0xFF));
            wav.push_back((uint8_t)((v >> 16) & 0xFF));
            wav.push_back((uint8_t)((v >> 24) & 0xFF));
        };
        auto put16 = [&](uint16_t v) {
            wav.push_back((uint8_t)(v & 0xFF));
            wav.push_back((uint8_t)((v >> 8) & 0xFF));
        };
        for (char c : {'R','I','F','F'}) wav.push_back((uint8_t)c);
        put32(36u + dataBytes);
        for (char c : {'W','A','V','E'}) wav.push_back((uint8_t)c);
        for (char c : {'f','m','t',' '}) wav.push_back((uint8_t)c);
        put32(16);                  // fmt size
        put16(1);                   // PCM
        put16(channels);
        put32((uint32_t)SR);
        put32(byteRate);
        put16(blockAlign);
        put16(16);                  // bits per sample
        for (char c : {'d','a','t','a'}) wav.push_back((uint8_t)c);
        put32(dataBytes);
        size_t hdr = wav.size();
        wav.resize(hdr + dataBytes);
        std::memcpy(wav.data() + hdr, pcm16.data(), dataBytes);

        HANDLE hf = CreateFileW(filePath, GENERIC_WRITE, 0, nullptr,
                                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hf == INVALID_HANDLE_VALUE) {
            MessageBoxW(hWnd, L"Impossible de cr\u00e9er le fichier.",
                        L"Export WAV", MB_OK | MB_ICONERROR);
            return;
        }
        DWORD written = 0;
        WriteFile(hf, wav.data(), (DWORD)wav.size(), &written, nullptr);
        CloseHandle(hf);

        double durSec = (double)totalSamples / (double)SR;
        wchar_t msg[MAX_PATH + 384];
        swprintf_s(msg,
            L"Audio rendu sauvegard\u00e9 (%d notes, %.1f s, %.0f BPM, %d Hz stereo) :\n%s\n\n"
            L"Le fichier WAV contient le son exact qui sort du plugin\n"
            L"avec les SoundFonts et volumes actuels.",
            (int)notes.size(), durSec, (double)bpm, SR, filePath);
        MessageBoxW(hWnd, msg, L"Export WAV \u2014 Rendu audio",
                    MB_OK | MB_ICONINFORMATION);
    }

    // Tiny in-memory IBStream (vector-backed) for preset save/load.
    // Used only on the GUI thread, no real refcounting needed.
    class MemBufStream : public IBStream {
    public:
        std::vector<uint8_t> buf;
        int64                 pos = 0;
        explicit MemBufStream(std::vector<uint8_t> initial = {})
            : buf(std::move(initial)) {}
        tresult PLUGIN_API read (void* dst, int32 n, int32* nRead) override {
            int64 avail = (int64)buf.size() - pos;
            int32 toRead = (int32)std::min<int64>(avail, n);
            if (toRead > 0) std::memcpy(dst, buf.data() + pos, (size_t)toRead);
            pos += toRead;
            if (nRead) *nRead = toRead;
            return kResultOk;
        }
        tresult PLUGIN_API write (void* src, int32 n, int32* nWritten) override {
            if (n < 0) { if (nWritten) *nWritten = 0; return kResultOk; }
            if ((size_t)(pos + n) > buf.size()) buf.resize((size_t)(pos + n));
            std::memcpy(buf.data() + pos, src, (size_t)n);
            pos += n;
            if (nWritten) *nWritten = n;
            return kResultOk;
        }
        tresult PLUGIN_API seek (int64 p, int32 mode, int64* result) override {
            int64 np = pos;
            if (mode == kIBSeekSet)      np = p;
            else if (mode == kIBSeekCur) np = pos + p;
            else if (mode == kIBSeekEnd) np = (int64)buf.size() + p;
            if (np < 0) np = 0;
            pos = np;
            if (result) *result = pos;
            return kResultOk;
        }
        tresult PLUGIN_API tell (int64* p) override {
            if (p) *p = pos; return kResultOk;
        }
        // FUnknown stubs (no real refcount; lifetime is the local var).
        tresult PLUGIN_API queryInterface(const TUID, void** obj) override {
            if (obj) *obj = nullptr; return kNoInterface;
        }
        uint32  PLUGIN_API addRef ()  override { return 1; }
        uint32  PLUGIN_API release () override { return 1; }
    };

    void doSavePreset() {
        // Capture current plugin state into an in-memory stream.
        MemBufStream mem;
        if (plugin->getState(&mem) != kResultOk) {
            MessageBoxW(hWnd, L"\u00c9chec de la r\u00e9cup\u00e9ration de l'\u00e9tat.",
                        L"Sauvegarder le preset", MB_OK | MB_ICONERROR);
            return;
        }

        wchar_t desktop[MAX_PATH]{};
        SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr,
                         SHGFP_TYPE_CURRENT, desktop);
        wchar_t filePath[MAX_PATH] = L"MelodyMaker.mmp";
        OPENFILENAMEW ofn{};
        ofn.lStructSize     = sizeof(ofn);
        ofn.hwndOwner       = hWnd;
        ofn.lpstrFilter     = L"Preset Melody Maker (*.mmp)\0*.mmp\0Tous (*.*)\0*.*\0";
        ofn.lpstrFile       = filePath;
        ofn.nMaxFile        = MAX_PATH;
        ofn.lpstrInitialDir = desktop;
        ofn.lpstrTitle      = L"Sauvegarder un preset";
        ofn.lpstrDefExt     = L"mmp";
        ofn.Flags           = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
        if (!GetSaveFileNameW(&ofn)) return;

        // File format: 4-byte magic 'MMPR' + 4-byte version (1) + raw state bytes.
        const uint8_t magic[4] = { 'M','M','P','R' };
        const uint32_t ver = 1u;

        HANDLE hf = CreateFileW(filePath, GENERIC_WRITE, 0, nullptr,
                                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hf == INVALID_HANDLE_VALUE) {
            MessageBoxW(hWnd, L"Impossible de cr\u00e9er le fichier.",
                        L"Sauvegarder le preset", MB_OK | MB_ICONERROR);
            return;
        }
        DWORD wr = 0;
        WriteFile(hf, magic, 4, &wr, nullptr);
        WriteFile(hf, &ver,  4, &wr, nullptr);
        if (!mem.buf.empty())
            WriteFile(hf, mem.buf.data(), (DWORD)mem.buf.size(), &wr, nullptr);
        CloseHandle(hf);

        // Auto-save MIDI alongside the preset if notes have been recorded.
        bool midiSaved = false;
        wchar_t midPath[MAX_PATH];
        {
            std::vector<MelodyMakerVST3::RecordedNote> notes;
            { std::lock_guard<std::mutex> lk(plugin->recMtx); notes = plugin->recNotes; }
            if (!notes.empty()) {
                std::vector<uint8_t> midiBytes = buildMidiBytes(notes);
                if (!midiBytes.empty()) {
                    wcscpy_s(midPath, filePath);
                    wchar_t* dot = wcsrchr(midPath, L'.');
                    if (dot) *dot = L'\0';
                    wcscat_s(midPath, L".mid");
                    HANDLE hm = CreateFileW(midPath, GENERIC_WRITE, 0, nullptr,
                        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
                    if (hm != INVALID_HANDLE_VALUE) {
                        DWORD mw = 0;
                        WriteFile(hm, midiBytes.data(), (DWORD)midiBytes.size(), &mw, nullptr);
                        CloseHandle(hm);
                        midiSaved = true;
                    }
                }
            }
        }

        wchar_t msg[MAX_PATH + 400];
        if (midiSaved)
            swprintf_s(msg, L"Preset + MIDI sauvegard\u00e9s :\n  %s\n  %s", filePath, midPath);
        else
            swprintf_s(msg, L"Preset sauvegard\u00e9 (%zu octets) :\n%s",
                       mem.buf.size() + 8, filePath);
        MessageBoxW(hWnd, msg, L"Sauvegarder le preset", MB_OK | MB_ICONINFORMATION);
    }

    void doLoadPreset() {
        wchar_t desktop[MAX_PATH]{};
        SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr,
                         SHGFP_TYPE_CURRENT, desktop);
        wchar_t filePath[MAX_PATH] = L"";
        OPENFILENAMEW ofn{};
        ofn.lStructSize     = sizeof(ofn);
        ofn.hwndOwner       = hWnd;
        ofn.lpstrFilter     = L"Preset Melody Maker (*.mmp)\0*.mmp\0Tous (*.*)\0*.*\0";
        ofn.lpstrFile       = filePath;
        ofn.nMaxFile        = MAX_PATH;
        ofn.lpstrInitialDir = desktop;
        ofn.lpstrTitle      = L"Charger un preset";
        ofn.lpstrDefExt     = L"mmp";
        ofn.Flags           = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
        if (!GetOpenFileNameW(&ofn)) return;

        HANDLE hf = CreateFileW(filePath, GENERIC_READ, FILE_SHARE_READ, nullptr,
                                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hf == INVALID_HANDLE_VALUE) {
            MessageBoxW(hWnd, L"Impossible d'ouvrir le fichier.",
                        L"Charger le preset", MB_OK | MB_ICONERROR);
            return;
        }
        LARGE_INTEGER sz{};
        if (!GetFileSizeEx(hf, &sz) || sz.QuadPart < 8 || sz.QuadPart > (1<<24)) {
            CloseHandle(hf);
            MessageBoxW(hWnd, L"Fichier de preset invalide.",
                        L"Charger le preset", MB_OK | MB_ICONERROR);
            return;
        }
        std::vector<uint8_t> file((size_t)sz.QuadPart);
        DWORD rd = 0;
        ReadFile(hf, file.data(), (DWORD)file.size(), &rd, nullptr);
        CloseHandle(hf);
        if (rd != file.size() ||
            file[0] != 'M' || file[1] != 'M' ||
            file[2] != 'P' || file[3] != 'R') {
            MessageBoxW(hWnd, L"Ce n'est pas un preset Melody Maker.",
                        L"Charger le preset", MB_OK | MB_ICONERROR);
            return;
        }
        // file[4..7] = version (currently 1; ignored beyond presence).
        std::vector<uint8_t> body(file.begin() + 8, file.end());
        MemBufStream mem(std::move(body));
        if (plugin->setState(&mem) != kResultOk) {
            MessageBoxW(hWnd, L"\u00c9tat illisible.",
                        L"Charger le preset", MB_OK | MB_ICONERROR);
            return;
        }
        // Refresh the entire UI from the freshly loaded state.
        int s = plugin->getStyleType();
        applyVisibilityForStyle(s);
        refreshAll();
        populateInstrumentCombo(s);
        if (hVolSlider)
            SendMessageW(hVolSlider, TBM_SETPOS, TRUE,
                (LPARAM)(int)std::round(plugin->sfVolumePerStyle[s] * 100.0f));
        refreshVolValue();
        // Refresh new parameters not handled by applyVisibilityForStyle.
        if (hHumanizeKnob)
            if (auto* kw = reinterpret_cast<KnobWidget*>(GetWindowLongPtrW(hHumanizeKnob, GWLP_USERDATA)))
                kw->setPos((int)std::round(plugin->humanizePerStyle[s] * 1000.0f));
        refreshHumanizeValue();
        if (hRetardKnob)
            if (auto* kw = reinterpret_cast<KnobWidget*>(GetWindowLongPtrW(hRetardKnob, GWLP_USERDATA)))
                kw->setPos((int)std::round(plugin->retardPerStyle[s] * 1000.0f));
        refreshRetardValue();
        if (hSectionCombo)
            SendMessageW(hSectionCombo, CB_SETCURSEL,
                std::clamp(plugin->currentSection.load(std::memory_order_relaxed), 0, 4), 0);
        if (hStartBarCombo) {
            static constexpr int kSBV[] = { 0,1,2,4,8 };
            int sbv = plugin->startBarPerStyle[s];
            int sbi = 0; for (int k=0;k<5;k++) if (kSBV[k]==sbv) sbi=k;
            SendMessageW(hStartBarCombo, CB_SETCURSEL, sbi, 0);
        }
        if (hProgCombo) SendMessageW(hProgCombo, CB_SETCURSEL,
                                     plugin->getProgType(), 0);
        InvalidateRect(hWnd, nullptr, TRUE);

        MessageBoxW(hWnd, L"Preset charg\u00e9 avec succ\u00e8s.",
                    L"Charger le preset", MB_OK | MB_ICONINFORMATION);
    }

    void sendParam(int i, double actual) {
        // Update local plugin state immediately so process() picks it up
        // even before the host echoes the change back.
        plugin->paramValues[i] = actual;
        // Mirror to active tab, then broadcast to other locked tabs only.
        int activeStyle = plugin->getStyleType();
        plugin->paramValuesPerStyle[activeStyle][i] = actual;
        if (i == mm::kParamSubdiv || i == mm::kParamMode)
            plugin->broadcastParamLocked(i, activeStyle);
        double norm = MelodyMakerVST3::actualToNorm(i, actual);
        plugin->beginEdit(i);
        plugin->performEdit(i, norm);
        plugin->endEdit(i);
        plugin->setParamNormalized(i, norm);
        updateValueLabel(i, actual);
    }

    void onComboChange(int paramId) {
        int idx = (int)SendMessageW(hCtrl[paramId], CB_GETCURSEL, 0, 0);
        if (idx == CB_ERR) return;
        sendParam(paramId, valueFromComboIndex(paramId, idx));
        // Refresh compat dots: when Mode changes, repaint Prog combo (and vice versa).
        if (paramId == mm::kParamMode && hProgCombo) {
            InvalidateRect(hProgCombo, nullptr, FALSE);
            UpdateWindow(hProgCombo);
        }
    }

    void onTrackbarChange(int paramId) {
        HWND ctrl = hCtrl[paramId];
        int pos = 0;
        // Knob widget? Read its stored pos directly.
        wchar_t cls[64] = {};
        GetClassNameW(ctrl, cls, 64);
        if (wcscmp(cls, KnobWidget::kClassName) == 0) {
            auto* kw = reinterpret_cast<KnobWidget*>(GetWindowLongPtrW(ctrl, GWLP_USERDATA));
            pos = kw ? kw->getPos() : 0;
        } else {
            pos = (int)SendMessageW(ctrl, TBM_GETPOS, 0, 0);
        }
        const auto& d = mm::kParamDefs[paramId];
        double v = d.min_value + (pos / 1000.0) * (d.max_value - d.min_value);
        sendParam(paramId, v);
    }
};

IMPLEMENT_FUNKNOWN_METHODS(MelodyMakerView, IPlugView, IPlugView::iid)

IPlugView* PLUGIN_API MelodyMakerVST3::createView(FIDString name) {
    if (name && std::strcmp(name, ViewType::kEditor) == 0)
        return new MelodyMakerView(this);
    return nullptr;
}

// =============================================================================
// Plugin factory
// =============================================================================
