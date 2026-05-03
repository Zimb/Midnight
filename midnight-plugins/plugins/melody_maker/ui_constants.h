#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "../../common/sf2_maker.h"

static const wchar_t* kWndClass = L"MidnightMelodyMakerView";
static constexpr int kIdBase   = 1000;
static constexpr int kIdExport = 1007; // kIdBase + kParamCount
static constexpr int kIdWave   = 1008;
static constexpr int kIdStyle  = 1009;
static constexpr int kIdProg   = 1010;
static constexpr int kIdAuto   = 1011; // Auto-Key (listen) toggle
static constexpr int kIdDice   = 1012; // Randomize seed of active style
static constexpr int kIdPianoMel   = 1013; // Piano: enable right-hand melody
static constexpr int kIdPercRhy    = 1014; // Percussion: rhythm pattern
static constexpr int kIdPianoChord = 1015; // Piano: play chords (vs single notes)
static constexpr int kIdVol        = 1016; // Per-style SoundFont volume slider
static constexpr int kIdLockMode   = 1017; // Padlock for Mode
static constexpr int kIdLockProg   = 1018; // Padlock for Progression
static constexpr int kIdLockSubdiv = 1019; // Padlock for Subdiv
static constexpr int kIdExportWav  = 1020; // Export rendered audio (WAV)
static constexpr int kIdSavePreset = 1021; // Save current state to .mmp file
static constexpr int kIdLoadPreset = 1022; // Load state from .mmp file
static constexpr int kIdMeter      = 1023; // Global time-signature numerator combo
static constexpr int kIdHumanize   = 1024; // Per-style timing humanization slider
static constexpr int kIdRetard     = 1025; // Per-style groove retard slider
static constexpr int kIdSection   = 1026; // Global section selector
static constexpr int kIdStartBar  = 1027; // Per-style start-bar delay
static constexpr int kIdUndo      = 1028; // Editor undo
static constexpr int kIdRedo      = 1029; // Editor redo
static constexpr int kIdFile      = 1030; // File dropdown toggle
// ---- Maker (SoundFont Maker) control IDs ----
static constexpr int kIdMakerEnable    = 1031; // Toggle SoundFont personnel OUI/NON
static constexpr int kIdMakerSynth     = 1032; // Combo: type de synthèse
static constexpr int kIdMakerAttack    = 1033; // Slider: attaque
static constexpr int kIdMakerDecay     = 1034; // Slider: déclin
static constexpr int kIdMakerSustain   = 1035; // Slider: maintien
static constexpr int kIdMakerRelease   = 1036; // Slider: relâchement
static constexpr int kIdMakerModIndex  = 1037; // Slider: profondeur FM
static constexpr int kIdMakerModDecay  = 1038; // Slider: déclin FM
static constexpr int kIdMakerGain      = 1039; // Slider: gain
static constexpr int kIdMakerGenerate  = 1040; // Bouton GÉNÉRER
static constexpr int kIdMakerLowNote   = 1041; // Combo: note basse
static constexpr int kIdMakerHighNote  = 1042; // Combo: note haute
static constexpr int kIdMakerStep      = 1043; // Combo: pas d'échantillonnage
static constexpr int kIdMakerName      = 1044; // Edit: nom du preset
// Éditeur SF2 externe
static constexpr int kIdMakerLoadSf2   = 1045; // Bouton: Charger SF2...
static constexpr int kIdMakerClearSf2  = 1046; // Bouton: ✕ Effacer SF2
static constexpr int kIdMakerDeltaTune = 1047; // Slider: accordage (±24 demi-tons)
static constexpr int kIdMakerDeltaAtk  = 1048; // Slider: attaque delta
static constexpr int kIdMakerDeltaDec  = 1049; // Slider: déclin delta
static constexpr int kIdMakerDeltaSus  = 1050; // Slider: sustain delta
static constexpr int kIdMakerDeltaRel  = 1051; // Slider: relâche delta
static constexpr int kIdMakerDeltaVol  = 1052; // Slider: volume delta
static constexpr int kIdMakerDeltaFilt = 1053; // Slider: filtre cutoff delta

// Per-style Maker defaults and slider ranges.
// Slider units: attack/decay/sustain/release = pos*0.01, modIndex/modDecay = pos*0.1, gain = pos*0.01
struct MkStyleDef {
    // SfmConfig defaults
    sfm::SfmSynth synth;
    float attack, decay, sustain, release, modIndex, modDecay, gain;
    int   lowNote, highNote, step; // MIDI note values
    // Slider min/max (in slider pos units)
    int atkMin, atkMax;
    int decMin, decMax;
    int susMin, susMax;
    int relMin, relMax;
    int miMin,  miMax;  // modIndex
    int mdMin,  mdMax;  // modDecay
    int gaMin,  gaMax;  // gain
    // Combo indices: lowNote, highNote, step
    int lowIdx, highIdx, stepIdx;
    // Label strings for the two "timbre" sliders (shown in UI)
    const wchar_t* miLabel; // e.g. L"Brillance"
    const wchar_t* mdLabel; // e.g. L"Durée anneau"
};
// LowNote combo: idx→{24,30,36,42,48}  HighNote combo: idx→{60,72,84,96,108}  Step combo: idx→{1,2,3,4,6,12}
static const MkStyleDef kMkStyle[6] = {
    // 0: Mélodie — Additive FM pad / voice
    { sfm::SfmSynth::Additive,
      0.05f, 0.60f, 0.65f, 0.50f, 5.0f, 4.0f, 0.90f,   36,96,4,
      1,80,  20,200, 30,90, 15,150, 1,80,  10,120, 60,100,
      2, 3, 3, L"Harmoniques", L"Détune (ct)" },
    // 1: Arpège — Karplus-Strong harp
    { sfm::SfmSynth::KS,
      0.01f, 1.30f, 0.00f, 1.00f, 2.0f, 15.0f, 0.90f,  24,96,4,
      1,3,   50,250, 0,15,  40,200, 0,30,  80,250, 70,100,
      0, 3, 3, L"Brillance", L"Durée anneau" },
    // 2: Basse — FM bass (rich, low range)
    { sfm::SfmSynth::FM,
      0.05f, 1.20f, 0.80f, 0.70f, 7.0f, 6.0f, 0.90f,   24,60,4,
      1,20,  50,300, 50,100, 20,200, 30,150, 20,200, 60,100,
      0, 0, 3, L"Profondeur FM", L"Déclin FM" },
    // 3: Percu. — FM percussion (punchy, zero sustain)
    { sfm::SfmSynth::FM,
      0.01f, 0.25f, 0.00f, 0.20f, 13.0f, 4.0f, 0.90f,  36,60,4,
      1,3,   5,100,  0,15,  5,80,   80,200, 10,120, 60,100,
      2, 0, 3, L"Profondeur FM", L"Déclin FM" },
    // 4: Contre — FM contrebasse (slow bow, high sustain)
    { sfm::SfmSynth::FM,
      0.50f, 0.60f, 0.85f, 0.70f, 2.0f, 6.0f, 0.90f,   24,60,4,
      20,100, 10,200, 60,100, 20,150, 5,50,  20,150, 60,100,
      0, 0, 3, L"Profondeur FM", L"Déclin FM" },
    // 5: Piano — Karplus-Strong piano (fast pluck, medium decay)
    { sfm::SfmSynth::KS,
      0.01f, 0.80f, 0.15f, 0.60f, 2.0f, 12.0f, 0.85f,  36,96,4,
      1,3,   30,200, 0,40,  30,150, 5,60,  60,250, 50,100,
      2, 3, 3, L"Brillance", L"Durée anneau" },
};

// ---- FX (chorus / delay / reverb / cassette noise) control IDs ----
static constexpr int kIdFxChorusOn      = 1050;
static constexpr int kIdFxChorusRate    = 1051;
static constexpr int kIdFxChorusDepth   = 1052;
static constexpr int kIdFxChorusMix     = 1053;
static constexpr int kIdFxDelayOn       = 1054;
static constexpr int kIdFxDelayTime     = 1055;
static constexpr int kIdFxDelayFb       = 1056;
static constexpr int kIdFxDelayMix      = 1057;
static constexpr int kIdFxReverbOn      = 1058;
static constexpr int kIdFxReverbSize    = 1059;
static constexpr int kIdFxReverbDamp    = 1060;
static constexpr int kIdFxReverbMix     = 1061;
static constexpr int kIdFxNoiseOn       = 1062;
static constexpr int kIdFxNoiseLevel    = 1063;
static constexpr int kIdFxNoiseFlutter  = 1064;
static constexpr int kIdFxNoiseTone     = 1065;

// Arrangement section types (used for volume dynamics).

// ---------- Theme palette (Omnisphere-style anthracite blue-grey) ------------
static constexpr COLORREF kColBg        = RGB(26, 31, 40);    // #1A1F28 main bg
static constexpr COLORREF kColPanel     = RGB(35, 41, 52);    // #232934 panels
static constexpr COLORREF kColControl   = RGB(44, 51, 62);    // #2C333E controls
static constexpr COLORREF kColControlHi = RGB(56, 65, 78);    // hover/lighter
static constexpr COLORREF kColBorder    = RGB(58, 66, 80);    // #3A4250
static constexpr COLORREF kColHeader1   = RGB(20, 25, 34);    // top of gradient
static constexpr COLORREF kColHeader2   = RGB(38, 46, 60);    // bottom of gradient
static constexpr COLORREF kColAccent    = RGB(79, 195, 247);  // bright cyan
static constexpr COLORREF kColAccentDark= RGB(56, 145, 200);
static constexpr COLORREF kColAccentWarm= RGB(255, 159, 67);  // Omnisphere warm
static constexpr COLORREF kColText      = RGB(221, 225, 232); // #DDE1E8
static constexpr COLORREF kColTextDim   = RGB(122, 130, 148); // #7A8294
static constexpr COLORREF kColTextValue = RGB(255, 255, 255);
static constexpr COLORREF kColTabBg     = RGB(44, 51, 62);
static constexpr COLORREF kColTabActive = RGB(58, 130, 246);  // bright blue
static constexpr COLORREF kColWhite     = RGB(255, 255, 255);

#ifndef TBS_TRANSPARENTBKGND
#define TBS_TRANSPARENTBKGND 0x1000
#endif

// =============================================================================
// KnobWidget â€“ custom rotary control for continuous params (Density / NoteLen)
//   - Drag vertically (up = +, down = -). Shift = fine. Double-click = default.
//   - Painted with anti-aliased arcs via GDI (SetStretchBltMode + GdiPlus-free).
//   - Sends WM_HSCROLL / SB_THUMBPOSITION 0..1000 like a trackbar so existing
//     handler can read the value without changes.
// =============================================================================
