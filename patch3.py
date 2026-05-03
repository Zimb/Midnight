path = r'C:\Users\dossa\OneDrive\Desktop\Midnight-1\midnight-plugins\plugins\melody_maker\view.cpp'
with open(path, encoding='utf-8') as f:
    src = f.read()

changes = []

# =============================================================================
# FIX 1: Add viewMode==2 guard to delta WM_HSCROLL handler
# Prevents FX knobs (ids 1051-1053 overlap!) from triggering SF2 reload
# =============================================================================
old = (
    '                // ---- Sliders delta SF2 externe ----\n'
    '                if ((id >= kIdMakerDeltaTune && id <= kIdMakerDeltaFilt) ||\n'
    '                    (id >= kIdMakerDeltaFineTune && id <= kIdMakerDeltaChorus)) {\n'
    '                    if (plugin) {\n'
    '                        int st = plugin->getStyleType();\n'
    '                        if (!plugin->externalSf2PerStyle[st].empty())\n'
    '                            commitDeltaAndReload(st);\n'
    '                        else\n'
    '                            refreshDeltaLabels();\n'
    '                    }\n'
    '                    return 0;\n'
    '                }'
)
new = (
    '                // ---- Sliders delta SF2 externe (viewMode==2 only, IDs overlap with FX) ----\n'
    '                if (viewMode == 2 &&\n'
    '                    ((id >= kIdMakerDeltaTune && id <= kIdMakerDeltaFilt) ||\n'
    '                     (id >= kIdMakerDeltaFineTune && id <= kIdMakerDeltaChorus))) {\n'
    '                    if (plugin) {\n'
    '                        refreshDeltaLabels();\n'
    '                        // Debounce: only reload SF2 after 400ms of inactivity\n'
    '                        KillTimer(hWnd, 1202);\n'
    '                        SetTimer(hWnd, 1202, 400, nullptr);\n'
    '                    }\n'
    '                    return 0;\n'
    '                }'
)
if src.count(old) == 1:
    src = src.replace(old, new, 1)
    changes.append("FIX 1: viewMode guard + debounce timer for delta handler")
else:
    print(f"SKIP FIX 1: {src.count(old)}")

# =============================================================================
# FIX 2: Add debounce timer handler (1202) in WM_TIMER
# =============================================================================
old = (
    '                // ---- Maker auto-refresh timer (id 1201) ----\n'
    '                if (w == 1201 && plugin && viewMode == 2) {'
)
new = (
    '                // ---- Delta SF2 debounce timer (id 1202): reload after 400ms idle ----\n'
    '                if (w == 1202 && plugin && viewMode == 2) {\n'
    '                    KillTimer(hWnd, 1202);\n'
    '                    int st = plugin->getStyleType();\n'
    '                    if (!plugin->externalSf2PerStyle[st].empty())\n'
    '                        commitDeltaAndReload(st);\n'
    '                }\n'
    '                // ---- Maker auto-refresh timer (id 1201) ----\n'
    '                if (w == 1201 && plugin && viewMode == 2) {'
)
if src.count(old) == 1:
    src = src.replace(old, new, 1)
    changes.append("FIX 2: WM_TIMER 1202 debounce handler")
else:
    print(f"SKIP FIX 2: {src.count(old)}")

# =============================================================================
# FIX 3: Call applyVisibilityForStyle + populateInstrumentCombo after SF2 load
# Previously only showExternalSf2Controls(true) was called — synthesis controls
# remained visible on top of delta knobs (the "superposition" bug)
# =============================================================================
old = (
    '                            // Charger imm\u00e9diatement\n'
    '                            plugin->reloadExternalSf(st);\n'
    '                            showExternalSf2Controls(true);\n'
    '                        } else {\n'
    '                            MessageBoxW(hWnd, L"Impossible de lire ce fichier SF2.",\n'
    '                                        L"Erreur", MB_OK | MB_ICONWARNING);\n'
    '                        }\n'
    '                    }\n'
    '                    return 0;\n'
    '                }\n'
    '                if (id == kIdMakerClearSf2'
)
new = (
    '                            // Charger imm\u00e9diatement\n'
    '                            plugin->reloadExternalSf(st);\n'
    '                            populateInstrumentCombo(st);\n'
    '                            // Re-layout Maker view to hide synthesis controls\n'
    '                            applyVisibilityForStyle(st);\n'
    '                        } else {\n'
    '                            MessageBoxW(hWnd, L"Impossible de lire ce fichier SF2.",\n'
    '                                        L"Erreur", MB_OK | MB_ICONWARNING);\n'
    '                        }\n'
    '                    }\n'
    '                    return 0;\n'
    '                }\n'
    '                if (id == kIdMakerClearSf2'
)
if src.count(old) == 1:
    src = src.replace(old, new, 1)
    changes.append("FIX 3: applyVisibilityForStyle + populateInstrumentCombo after SF2 load")
else:
    print(f"SKIP FIX 3: {src.count(old)}")

# =============================================================================
# FIX 4: Call applyVisibilityForStyle after SF2 clear
# Previously only showExternalSf2Controls(false) was called — synthesis
# controls were never re-shown
# =============================================================================
old = (
    '                    showExternalSf2Controls(false);\n'
    '                    // Revenir au SF2 g\u00e9n\u00e9r\u00e9 si Maker est actif\n'
    '                    if (makerEnabled[st]) plugin->reloadLiveSf(st, makerCfg[st]);\n'
    '                    return 0;\n'
    '                }\n'
    '                // ---- FX toggles ----'
)
new = (
    '                    showExternalSf2Controls(false);\n'
    '                    // Revenir au SF2 g\u00e9n\u00e9r\u00e9 si Maker est actif\n'
    '                    if (makerEnabled[st]) plugin->reloadLiveSf(st, makerCfg[st]);\n'
    '                    // Re-layout Maker view to show synthesis controls again\n'
    '                    applyVisibilityForStyle(st);\n'
    '                    return 0;\n'
    '                }\n'
    '                // ---- FX toggles ----'
)
if src.count(old) == 1:
    src = src.replace(old, new, 1)
    changes.append("FIX 4: applyVisibilityForStyle after SF2 clear")
else:
    print(f"SKIP FIX 4: {src.count(old)}")

# =============================================================================
# FIX 5: Kill debounce timer when leaving Maker view or destroying window
# Add to the "kill maker timer" block in applyVisibilityForStyle
# =============================================================================
old = (
    '        // Kill maker timer when navigating away from maker view\n'
    '        if (viewMode != 2 && makerTimerId) { KillTimer(hWnd, makerTimerId); makerTimerId = 0; }'
)
new = (
    '        // Kill maker timers when navigating away from maker view\n'
    '        if (viewMode != 2 && makerTimerId) { KillTimer(hWnd, makerTimerId); makerTimerId = 0; }\n'
    '        if (viewMode != 2) KillTimer(hWnd, 1202); // kill delta debounce if any'
)
if src.count(old) == 1:
    src = src.replace(old, new, 1)
    changes.append("FIX 5: Kill debounce timer on view change")
else:
    print(f"SKIP FIX 5: {src.count(old)}")

with open(path, 'w', encoding='utf-8') as f:
    f.write(src)

for c in changes:
    print(f"Done: {c}")
print("Patch 3 complete")
