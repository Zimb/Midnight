"""
patch_full_sf2.py — exposes all SF2 generator parameters in the Maker UI.
Run from the Midnight-1 root directory.
"""
import re, sys

def read(path):
    with open(path, encoding='utf-8') as f:
        return f.read()

def write(path, content):
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write(content)

def replace_once(content, old, new, label):
    if old not in content:
        print(f"  MISS: {label}")
        return content
    print(f"  OK:   {label}")
    return content.replace(old, new, 1)

# ============================================================
# 1. sf2_editor.h — add missing GEN constants, Sf2Delta fields,
#    patchGen cases, conversion helpers, applyDeltaTargeted
# ============================================================

SF2_EDITOR = r'midnight-plugins\common\sf2_editor.h'
content = read(SF2_EDITOR)

# 1a. Add missing GEN_ constants after GEN_SCALE_TUNING
OLD_CONSTS = "static constexpr uint16_t GEN_SCALE_TUNING        = 56;  // cents/demi-ton (default 100)"
NEW_CONSTS = OLD_CONSTS + """
static constexpr uint16_t GEN_MOD_LFO_TO_PITCH    =  5;  // cents (±12000)
static constexpr uint16_t GEN_VIB_LFO_TO_PITCH    =  6;  // cents (±12000)
static constexpr uint16_t GEN_MOD_ENV_TO_PITCH     =  7;  // cents (±12000)
static constexpr uint16_t GEN_MOD_LFO_TO_FILTER    = 10;  // cents (±12000)
static constexpr uint16_t GEN_MOD_ENV_TO_FILTER    = 11;  // cents (±12000)
static constexpr uint16_t GEN_MOD_LFO_TO_VOLUME    = 13;  // centibels (±960)
static constexpr uint16_t GEN_DELAY_MOD_LFO        = 21;  // timecents
static constexpr uint16_t GEN_FREQ_MOD_LFO         = 22;  // cents (0=8.176Hz)
static constexpr uint16_t GEN_DELAY_VIB_LFO        = 23;  // timecents
static constexpr uint16_t GEN_FREQ_VIB_LFO         = 24;  // cents (0=8.176Hz)
static constexpr uint16_t GEN_DELAY_MOD_ENV        = 25;  // timecents
static constexpr uint16_t GEN_HOLD_MOD_ENV         = 27;  // timecents
static constexpr uint16_t GEN_DELAY_VOL_ENV        = 33;  // timecents"""
content = replace_once(content, OLD_CONSTS, NEW_CONSTS, "add GEN constants")

# 1b. Expand Sf2Delta struct
OLD_DELTA = """struct Sf2Delta {
    // Accordage
    int   coarseTune      = 0;   // demi-tons à ajouter (−24..+24)
    int   fineTune        = 0;   // cents à ajouter (−99..+99)

    // Enveloppe de volume (timecents : +1200 = 2× plus long, −1200 = 2× plus court)
    int   attackDelta     = 0;
    int   holdDelta       = 0;
    int   decayDelta      = 0;
    int   releaseDelta    = 0;
    int   sustainDelta    = 0;   // centibels (+100 = sustain plus faible)

    // Enveloppe de modulation (même unités)
    int   modAttackDelta  = 0;
    int   modDecayDelta   = 0;
    int   modSustainDelta = 0;
    int   modReleaseDelta = 0;

    // Volume global
    int   attenuationDelta = 0;  // centibels (positif = plus silencieux)

    // Filtre passe-bas
    int   filterFcDelta   = 0;   // cents sur la fréquence de coupure
    int   filterQDelta    = 0;   // centibels de résonance

    // Effets intégrés au SF2
    int   chorusDelta     = 0;   // 0..1000 per-mille
    int   reverbDelta     = 0;   // 0..1000 per-mille

    // Stéréo
    int   panDelta        = 0;   // -500..+500
};"""
NEW_DELTA = """struct Sf2Delta {
    // Accordage
    int   coarseTune      = 0;   // demi-tons (−24..+24)
    int   fineTune        = 0;   // cents (−99..+99)
    int   scaleTuning     = 0;   // delta cents/demi-ton (−100..+100, default offset from 100)

    // Enveloppe de volume (timecents)
    int   delayVolDelta   = 0;
    int   attackDelta     = 0;
    int   holdDelta       = 0;
    int   decayDelta      = 0;
    int   releaseDelta    = 0;
    int   sustainDelta    = 0;   // centibels (+100 = sustain plus faible)

    // Enveloppe de modulation
    int   delayModDelta   = 0;
    int   modAttackDelta  = 0;
    int   modHoldDelta    = 0;
    int   modDecayDelta   = 0;
    int   modSustainDelta = 0;
    int   modReleaseDelta = 0;
    int   modEnvToPitch   = 0;   // cents (±12000)
    int   modEnvToFilter  = 0;   // cents (±12000)

    // LFO de modulation
    int   modLfoDelay     = 0;   // timecents
    int   modLfoFreq      = 0;   // cents sur la fréquence LFO
    int   modLfoToPitch   = 0;   // cents (±12000)
    int   modLfoToFilter  = 0;   // cents (±12000)
    int   modLfoToVolume  = 0;   // centibels (±960)

    // LFO de vibrato
    int   vibLfoDelay     = 0;   // timecents
    int   vibLfoFreq      = 0;   // cents sur la fréquence vibrato
    int   vibLfoToPitch   = 0;   // cents (±12000)

    // Volume global
    int   attenuationDelta = 0;  // centibels (positif = plus silencieux)

    // Filtre passe-bas
    int   filterFcDelta   = 0;   // cents sur la fréquence de coupure
    int   filterQDelta    = 0;   // centibels de résonance

    // Effets intégrés au SF2
    int   chorusDelta     = 0;   // 0..1000 per-mille
    int   reverbDelta     = 0;   // 0..1000 per-mille

    // Stéréo
    int   panDelta        = 0;   // -500..+500
};"""
content = replace_once(content, OLD_DELTA, NEW_DELTA, "expand Sf2Delta")

# 1c. Add new cases to patchGen switch
OLD_PATCH_END = """    case GEN_PAN:                 out = clampI16(amt + d.panDelta,           -500,  500); break;
    default: hit = false; break;"""
NEW_PATCH_END = """    case GEN_PAN:                 out = clampI16(amt + d.panDelta,           -500,  500); break;
    case GEN_SCALE_TUNING:        out = clampI16(amt + d.scaleTuning,            0, 1200); break;
    case GEN_DELAY_VOL_ENV:       out = clampI16(amt + d.delayVolDelta,      -12000, 5000); break;
    case GEN_DELAY_MOD_ENV:       out = clampI16(amt + d.delayModDelta,      -12000, 5000); break;
    case GEN_HOLD_MOD_ENV:        out = clampI16(amt + d.modHoldDelta,       -12000, 5000); break;
    case GEN_MOD_ENV_TO_PITCH:    out = clampI16(amt + d.modEnvToPitch,      -12000,12000); break;
    case GEN_MOD_ENV_TO_FILTER:   out = clampI16(amt + d.modEnvToFilter,     -12000,12000); break;
    case GEN_DELAY_MOD_LFO:       out = clampI16(amt + d.modLfoDelay,        -12000, 5000); break;
    case GEN_FREQ_MOD_LFO:        out = clampI16(amt + d.modLfoFreq,         -16000, 4500); break;
    case GEN_MOD_LFO_TO_PITCH:    out = clampI16(amt + d.modLfoToPitch,      -12000,12000); break;
    case GEN_MOD_LFO_TO_FILTER:   out = clampI16(amt + d.modLfoToFilter,     -12000,12000); break;
    case GEN_MOD_LFO_TO_VOLUME:   out = clampI16(amt + d.modLfoToVolume,       -960,  960); break;
    case GEN_DELAY_VIB_LFO:       out = clampI16(amt + d.vibLfoDelay,        -12000, 5000); break;
    case GEN_FREQ_VIB_LFO:        out = clampI16(amt + d.vibLfoFreq,         -16000, 4500); break;
    case GEN_VIB_LFO_TO_PITCH:    out = clampI16(amt + d.vibLfoToPitch,      -12000,12000); break;
    default: hit = false; break;"""
content = replace_once(content, OLD_PATCH_END, NEW_PATCH_END, "add patchGen cases")

# 1d. Add applyDeltaTargeted before the existing applyDelta function
OLD_APPLY_DELTA_COMMENT = "// applyDelta — modifie tous les générateurs IGEN et PGEN dans le buffer SF2."
NEW_APPLY_DELTA_COMMENT = """// applyDeltaTargeted — modifie uniquement le preset (bank, program) sélectionné :
// - patches PGEN pour ce preset
// - patches IGEN pour les instruments liés à ce preset (préserve les autres presets)
// Retourne false si SF2 invalide ou preset introuvable.
inline bool applyDeltaTargeted(std::vector<uint8_t>& sf2,
                                const Sf2Delta& delta,
                                uint16_t bank, uint16_t program) {
    if (sf2.size() < 12) return false;
    uint8_t* data    = sf2.data();
    uint8_t* fileEnd = data + sf2.size();
    if (!tag4(data, "RIFF") || !tag4(data + 8, "sfbk")) return false;

    uint8_t* phdr=nullptr; uint32_t phdrSz=0;
    uint8_t* pbag=nullptr; uint32_t pbagSz=0;
    uint8_t* pgen=nullptr; uint32_t pgenSz=0;
    uint8_t* inst=nullptr; uint32_t instSz=0;
    uint8_t* ibag=nullptr; uint32_t ibagSz=0;
    uint8_t* igen=nullptr; uint32_t igenSz=0;

    uint8_t* pos = data + 12;
    while (pos + 8 <= fileEnd) {
        uint32_t chunkSz = readU32LE(pos + 4);
        if (tag4(pos, "LIST") && pos + 12 <= fileEnd && tag4(pos + 8, "pdta")) {
            uint8_t* sub    = pos + 12;
            uint8_t* subEnd = pos + 8 + chunkSz;
            if (subEnd > fileEnd) subEnd = fileEnd;
            while (sub + 8 <= subEnd) {
                uint32_t subSz = readU32LE(sub + 4);
                if (tag4(sub,"PHDR")){ phdr=sub+8; phdrSz=subSz; }
                if (tag4(sub,"PBAG")){ pbag=sub+8; pbagSz=subSz; }
                if (tag4(sub,"PGEN")){ pgen=sub+8; pgenSz=subSz; }
                if (tag4(sub,"INST")){ inst=sub+8; instSz=subSz; }
                if (tag4(sub,"IBAG")){ ibag=sub+8; ibagSz=subSz; }
                if (tag4(sub,"IGEN")){ igen=sub+8; igenSz=subSz; }
                uint32_t adv = 8 + ((subSz + 1u) & ~1u);
                if (adv < 8) break;
                sub += adv;
            }
            break;
        }
        uint32_t adv = 8 + ((chunkSz + 1u) & ~1u);
        if (adv < 8) break;
        pos += adv;
    }
    if (!phdr || !pbag || !pgen || !inst || !ibag || !igen) return false;

    // Find preset in PHDR (38 bytes/record)
    uint16_t presetBagStart = 0xFFFF, presetBagEnd = 0xFFFF;
    {
        uint8_t* rec = phdr;
        uint8_t* recEnd = phdr + phdrSz;
        for (; rec + 38 <= recEnd; rec += 38) {
            uint16_t p  = readU16LE(rec + 20);
            uint16_t b  = readU16LE(rec + 22);
            uint16_t bg = readU16LE(rec + 24);
            if (p == program && b == bank) {
                presetBagStart = bg;
                if (rec + 76 <= recEnd) presetBagEnd = readU16LE(rec + 38 + 24);
                break;
            }
        }
    }
    if (presetBagStart == 0xFFFF || presetBagEnd == 0xFFFF) return false;

    // Walk preset bags: patch PGEN and collect instrument indices
    std::vector<uint16_t> instIndices;
    for (uint16_t bi = presetBagStart; bi < presetBagEnd; ++bi) {
        if ((bi + 1u) * 4u > pbagSz) break;
        uint16_t genIdx = readU16LE(pbag + bi * 4);
        uint16_t genEnd = ((bi + 1u) < presetBagEnd && (bi + 2u) * 4u <= pbagSz)
                          ? readU16LE(pbag + (bi + 1u) * 4) : genIdx + 1;
        for (uint16_t gi = genIdx; gi < genEnd; ++gi) {
            if ((gi + 1u) * 4u > pgenSz) break;
            uint16_t oper = readU16LE(pgen + gi * 4);
            if (oper == 41) { // GEN_INSTRUMENT
                instIndices.push_back(readU16LE(pgen + gi * 4 + 2));
            } else {
                patchGen(pgen + gi * 4, delta);
            }
        }
    }

    // For each instrument, walk IBAG and patch IGEN
    for (uint16_t iidx : instIndices) {
        if ((iidx + 2u) * 22u > instSz) continue; // INST record = 22 bytes
        uint16_t ibagStart = readU16LE(inst + iidx * 22 + 20);
        uint16_t ibagEndR  = readU16LE(inst + (iidx + 1u) * 22 + 20);
        for (uint16_t ib = ibagStart; ib < ibagEndR; ++ib) {
            if ((ib + 1u) * 4u > ibagSz) break;
            uint16_t igenIdx = readU16LE(ibag + ib * 4);
            uint16_t igenEnd = ((ib + 1u) < ibagEndR && (ib + 2u) * 4u <= ibagSz)
                               ? readU16LE(ibag + (ib + 1u) * 4) : igenIdx + 1;
            for (uint16_t ig = igenIdx; ig < igenEnd; ++ig) {
                if ((ig + 1u) * 4u > igenSz) break;
                patchGen(igen + ig * 4, delta);
            }
        }
    }
    return true;
}

// applyDelta — modifie tous les générateurs IGEN et PGEN dans le buffer SF2."""
content = replace_once(content, OLD_APPLY_DELTA_COMMENT, NEW_APPLY_DELTA_COMMENT, "add applyDeltaTargeted")

# 1e. Add conversion helpers for new params
OLD_HELPERS_END = """// Convertit un slider 0..100 en delta d'envoi chorus SF2 (per-mille).
// 50 = pas de changement, 0 = −500, 100 = +500.
inline int chorusSliderToVal(int sliderVal) {
    return (sliderVal - 50) * 10; // −500..+500
}

} // namespace sfed"""
NEW_HELPERS_END = """// Convertit un slider 0..100 en delta d'envoi chorus SF2 (per-mille).
// 50 = pas de changement, 0 = −500, 100 = +500.
inline int chorusSliderToVal(int sliderVal) {
    return (sliderVal - 50) * 10; // −500..+500
}

// Convertit un slider 0..100 en delta d'accordage d'échelle (cents/demi-ton).
// 50 = pas de changement, 0 = −100, 100 = +100.
inline int scaleSliderToDelta(int sliderVal) {
    return (sliderVal - 50) * 2; // −100..+100
}

// Convertit un slider 0..100 en delta de délai d'enveloppe ou de LFO (timecents).
// 50 = pas de changement (délai inchangé), 0 = −3600 tc (plus court), 100 = +3600 tc (plus long).
inline int delaySliderToTc(int sliderVal) {
    float t = (sliderVal - 50) / 50.0f;
    float scale = std::pow(8.0f, t);
    return scaleToTimecents(scale);
}

// Convertit un slider 0..100 en delta de fréquence LFO (cents sur la fréquence absolue).
// 50 = pas de changement, 0 = −4000 cents (plus lent), 100 = +4000 cents (plus rapide).
inline int lfoFreqSliderToCents(int sliderVal) {
    return (sliderVal - 50) * 80; // −4000..+4000 cents
}

// Convertit un slider 0..100 en delta de modulation de pitch/filtre par enveloppe ou LFO.
// 50 = pas de changement, 0 = −2400 cents, 100 = +2400 cents (±2 octaves).
inline int modDepthSliderToCents(int sliderVal) {
    return (sliderVal - 50) * 48; // −2400..+2400 cents
}

// Convertit un slider 0..100 en delta de tremolo (mod LFO → volume).
// 50 = pas de changement, 0 = −480 cb, 100 = +480 cb.
inline int tremoloSliderToCb(int sliderVal) {
    return (sliderVal - 50) * 10; // −500..+500 cb
}

} // namespace sfed"""
content = replace_once(content, OLD_HELPERS_END, NEW_HELPERS_END, "add conversion helpers")

write(SF2_EDITOR, content)
print("sf2_editor.h done")

# ============================================================
# 2. ui_constants.h — add new IDs (1072..1090)
# ============================================================

UI_CONST = r'midnight-plugins\plugins\melody_maker\ui_constants.h'
content = read(UI_CONST)

OLD_LAST_ID = "static constexpr int kIdRefresh           = 1071; // Refresh / regenerate button"
NEW_IDS = OLD_LAST_ID + """
// ---- Delta SF2 — paramètres avancés (exposés en v2 du Maker) ----
static constexpr int kIdMakerDeltaScale      = 1072; // Accordage d'échelle (cents/demi-ton)
static constexpr int kIdMakerDeltaDelayVol   = 1073; // Délai env volume
static constexpr int kIdMakerDeltaHoldVol    = 1074; // Maintien env volume
static constexpr int kIdMakerDeltaDelayMod   = 1075; // Délai env mod
static constexpr int kIdMakerDeltaAtkMod     = 1076; // Attaque env mod
static constexpr int kIdMakerDeltaHoldMod    = 1077; // Maintien env mod
static constexpr int kIdMakerDeltaDecMod     = 1078; // Déclin env mod
static constexpr int kIdMakerDeltaSusMod     = 1079; // Sustain env mod
static constexpr int kIdMakerDeltaRelMod     = 1080; // Relâche env mod
static constexpr int kIdMakerDeltaModEnvPitch= 1081; // Env mod → pitch
static constexpr int kIdMakerDeltaModEnvFilt = 1082; // Env mod → filtre
static constexpr int kIdMakerDeltaModLfoDly  = 1083; // Délai LFO mod
static constexpr int kIdMakerDeltaModLfoFq   = 1084; // Fréq LFO mod
static constexpr int kIdMakerDeltaModLfoPitch= 1085; // LFO mod → pitch
static constexpr int kIdMakerDeltaModLfoFilt = 1086; // LFO mod → filtre
static constexpr int kIdMakerDeltaModLfoVol  = 1087; // LFO mod → volume (tremolo)
static constexpr int kIdMakerDeltaVibLfoDly  = 1088; // Délai LFO vibrato
static constexpr int kIdMakerDeltaVibLfoFq   = 1089; // Fréq LFO vibrato
static constexpr int kIdMakerDeltaVibLfoPitch= 1090; // LFO vibrato → pitch"""
content = replace_once(content, OLD_LAST_ID, NEW_IDS, "add new delta IDs")
write(UI_CONST, content)
print("ui_constants.h done")

print("\nDone. Run patch_view_sf2.py next for view.cpp changes.")
