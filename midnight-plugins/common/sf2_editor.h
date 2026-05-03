#pragma once
// sf2_editor.h — Éditeur de SoundFont en mémoire (patch de générateurs par delta).
//
// Charge un SF2 existant, modifie les paramètres IGEN/PGEN par delta additif,
// retourne les bytes modifiés — sans jamais toucher aux samples audio.
// Compatible avec tsf_load_memory() pour un rechargement à chaud.
//
// Utilisation typique :
//   auto sf2 = sfed::loadSf2File(L"C:/path/to/piano.sf2");
//   sfed::Sf2Delta d;
//   d.coarseTune   = -2;            // 2 demi-tons en dessous
//   d.attackDelta  = sfed::scaleToTimecents(0.5f); // attaque 2x plus rapide
//   d.filterFcDelta = -2000;        // filtre plus fermé
//   sfed::applyDelta(sf2, d);
//   tsf* t = tsf_load_memory(sf2.data(), (int)sf2.size());

#include <vector>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <cstdio>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#endif

namespace sfed {

// ---- IDs de générateurs SF2 2.04 (spec §8.1.3) ---------------------------
static constexpr uint16_t GEN_INITIAL_FILTER_FC   =  8;  // cutoff abs cents (1500–13500)
static constexpr uint16_t GEN_INITIAL_FILTER_Q    =  9;  // résonance centibels (0–960)
static constexpr uint16_t GEN_CHORUS_SEND         = 15;  // 0–1000 per-mille
static constexpr uint16_t GEN_REVERB_SEND         = 16;  // 0–1000 per-mille
static constexpr uint16_t GEN_PAN                 = 17;  // -500 (gauche) à +500 (droite)
static constexpr uint16_t GEN_ATTACK_VOL_ENV      = 34;  // timecents (2^(tc/1200) secondes)
static constexpr uint16_t GEN_HOLD_VOL_ENV        = 35;  // timecents
static constexpr uint16_t GEN_DECAY_VOL_ENV       = 36;  // timecents
static constexpr uint16_t GEN_SUSTAIN_VOL_ENV     = 37;  // centibels atten. (0=max, 1000=silence)
static constexpr uint16_t GEN_RELEASE_VOL_ENV     = 38;  // timecents
static constexpr uint16_t GEN_ATTACK_MOD_ENV      = 26;
static constexpr uint16_t GEN_DECAY_MOD_ENV       = 28;
static constexpr uint16_t GEN_SUSTAIN_MOD_ENV     = 29;
static constexpr uint16_t GEN_RELEASE_MOD_ENV     = 30;
static constexpr uint16_t GEN_INITIAL_ATTENUATION = 48;  // centibels volume global (0–960)
static constexpr uint16_t GEN_COARSE_TUNE         = 51;  // demi-tons (-120 à +120)
static constexpr uint16_t GEN_FINE_TUNE           = 52;  // cents (-99 à +99)
static constexpr uint16_t GEN_SCALE_TUNING        = 56;  // cents/demi-ton (default 100)
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
static constexpr uint16_t GEN_DELAY_VOL_ENV        = 33;  // timecents

// ---- Low-level binary helpers (forward-declared here so preset parsing can use them) ----
static inline uint32_t readU32LE(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
}
static inline uint16_t readU16LE(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1]<<8);
}
static inline void writeI16LE(uint8_t* p, int16_t v) {
    p[0] = (uint8_t)((uint16_t)v & 0xFF);
    p[1] = (uint8_t)(((uint16_t)v >> 8) & 0xFF);
}
static inline bool tag4(const uint8_t* p, const char* t) {
    return p[0]==t[0] && p[1]==t[1] && p[2]==t[2] && p[3]==t[3];
}
static inline int16_t clampI16(int v, int lo, int hi) {
    return (int16_t)std::clamp(v, lo, hi);
}

// ---- Structure delta -------------------------------------------------------
// Tous les champs sont additifs sur les valeurs existantes dans le SF2.
// Laisser à 0 = aucun changement pour ce paramètre.
struct Sf2Delta {
    // Accordage
    int   coarseTune      = 0;   // demi-tons (−24..+24)
    int   fineTune        = 0;   // cents (−99..+99)
    int   scaleTuning     = 0;   // delta cents/demi-ton (−100..+100)

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
};

// Retourne true si tous les champs sont à zéro (aucune modification à appliquer).
inline bool sf2DeltaIsZero(const Sf2Delta& d) {
    return d.coarseTune == 0 && d.fineTune == 0 && d.scaleTuning == 0
        && d.delayVolDelta == 0 && d.attackDelta == 0 && d.holdDelta == 0
        && d.decayDelta == 0 && d.releaseDelta == 0 && d.sustainDelta == 0
        && d.delayModDelta == 0 && d.modAttackDelta == 0 && d.modHoldDelta == 0
        && d.modDecayDelta == 0 && d.modSustainDelta == 0 && d.modReleaseDelta == 0
        && d.modEnvToPitch == 0 && d.modEnvToFilter == 0
        && d.modLfoDelay == 0 && d.modLfoFreq == 0
        && d.modLfoToPitch == 0 && d.modLfoToFilter == 0 && d.modLfoToVolume == 0
        && d.vibLfoDelay == 0 && d.vibLfoFreq == 0 && d.vibLfoToPitch == 0
        && d.attenuationDelta == 0 && d.filterFcDelta == 0 && d.filterQDelta == 0
        && d.chorusDelta == 0 && d.reverbDelta == 0 && d.panDelta == 0;
}

inline int scaleToTimecents(float scale) {
    if (scale <= 0.001f) return -12000;
    return (int)std::round(1200.0 * std::log2((double)scale));
}

static bool patchGen(uint8_t* rec, const Sf2Delta& d) {
    uint16_t oper = readU16LE(rec);
    int      amt  = (int)(int16_t)readU16LE(rec + 2);
    int      out  = amt;
    bool     hit  = true;
    switch (oper) {
    case GEN_COARSE_TUNE:         out = clampI16(amt + d.coarseTune,      -120,  120); break;
    case GEN_FINE_TUNE:           out = clampI16(amt + d.fineTune,          -99,   99); break;
    case GEN_ATTACK_VOL_ENV:      out = clampI16(amt + d.attackDelta,    -12000, 8000); break;
    case GEN_HOLD_VOL_ENV:        out = clampI16(amt + d.holdDelta,       -12000, 5000); break;
    case GEN_DECAY_VOL_ENV:       out = clampI16(amt + d.decayDelta,      -12000, 8000); break;
    case GEN_SUSTAIN_VOL_ENV:     out = clampI16(amt + d.sustainDelta,         0, 1000); break;
    case GEN_RELEASE_VOL_ENV:     out = clampI16(amt + d.releaseDelta,    -12000, 8000); break;
    case GEN_ATTACK_MOD_ENV:      out = clampI16(amt + d.modAttackDelta,  -12000, 8000); break;
    case GEN_DECAY_MOD_ENV:       out = clampI16(amt + d.modDecayDelta,   -12000, 8000); break;
    case GEN_SUSTAIN_MOD_ENV:     out = clampI16(amt + d.modSustainDelta,      0, 1000); break;
    case GEN_RELEASE_MOD_ENV:     out = clampI16(amt + d.modReleaseDelta, -12000, 8000); break;
    case GEN_INITIAL_ATTENUATION: out = clampI16(amt + d.attenuationDelta,     0,  960); break;
    case GEN_INITIAL_FILTER_FC:   out = clampI16(amt + d.filterFcDelta,    1500,13500); break;
    case GEN_INITIAL_FILTER_Q:    out = clampI16(amt + d.filterQDelta,         0,  960); break;
    case GEN_CHORUS_SEND:         out = clampI16(amt + d.chorusDelta,          0, 1000); break;
    case GEN_REVERB_SEND:         out = clampI16(amt + d.reverbDelta,          0, 1000); break;
    case GEN_PAN:                 out = clampI16(amt + d.panDelta,           -500,  500); break;
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
    default: hit = false; break;
    }
    if (hit && out != amt) writeI16LE(rec + 2, (int16_t)out);
    return hit;
}

// ---- Preset listing --------------------------------------------------------

// Un preset SF2 tel que défini dans le chunk PHDR (programme + banque + nom).
struct Sf2Preset {
    char     name[21] = {};  // nom null-terminé (20 chars max dans la spec SF2)
    uint16_t program  = 0;   // 0–127 (numéro de programme MIDI)
    uint16_t bank     = 0;   // 0–128 (128 = percussions GM)
};

// Parse le chunk PHDR d'un SF2 en mémoire et retourne la liste des presets
// triés par (banque, programme, nom). Le dernier enregistrement PHDR (EOP sentinel)
// est ignoré automatiquement. Retourne {} si le SF2 est invalide.
inline std::vector<Sf2Preset> listPresets(const std::vector<uint8_t>& sf2) {
    if (sf2.size() < 12) return {};
    const uint8_t* data    = sf2.data();
    const uint8_t* fileEnd = data + sf2.size();
    if (!tag4(data, "RIFF") || !tag4(data + 8, "sfbk")) return {};

    std::vector<Sf2Preset> result;

    const uint8_t* pos = data + 12;
    while (pos + 8 <= fileEnd) {
        uint32_t chunkSz = readU32LE(pos + 4);
        if (tag4(pos, "LIST") && pos + 12 <= fileEnd && tag4(pos + 8, "pdta")) {
            const uint8_t* sub    = pos + 12;
            const uint8_t* subEnd = pos + 8 + chunkSz;
            if (subEnd > fileEnd) subEnd = fileEnd;

            while (sub + 8 <= subEnd) {
                uint32_t subSz = readU32LE(sub + 4);
                if (tag4(sub, "PHDR") && subSz >= 38) {
                    // PHDR record = 38 bytes each
                    const uint8_t* rec    = sub + 8;
                    const uint8_t* recEnd = rec + subSz;
                    if (recEnd > fileEnd) recEnd = fileEnd;
                    for (; rec + 38 <= recEnd; rec += 38) {
                        uint16_t prog = readU16LE(rec + 20);
                        uint16_t bank = readU16LE(rec + 22);
                        // EOP sentinel: bank=0, program=0, name="EOP" — skip last record
                        if (rec + 38 + 38 > recEnd) break; // last record = sentinel
                        Sf2Preset p;
                        std::memcpy(p.name, rec, 20);
                        p.name[20] = '\0';
                        // Trim trailing spaces/nulls
                        for (int i = 19; i >= 0 && (p.name[i] == ' ' || p.name[i] == '\0'); --i)
                            p.name[i] = '\0';
                        p.program = prog;
                        p.bank    = bank;
                        result.push_back(p);
                    }
                }
                uint32_t adv = 8 + ((subSz + 1u) & ~1u);
                if (adv < 8) break;
                sub += adv;
            }
            break; // found pdta, done
        }
        uint32_t adv = 8 + ((chunkSz + 1u) & ~1u);
        if (adv < 8) break;
        pos += adv;
    }

    std::sort(result.begin(), result.end(), [](const Sf2Preset& a, const Sf2Preset& b) {
        if (a.bank != b.bank) return a.bank < b.bank;
        if (a.program != b.program) return a.program < b.program;
        return std::strcmp(a.name, b.name) < 0;
    });
    return result;
}

// ---- Snapshot des générateurs d'un preset ----------------------------------
// Valeurs absolues lues depuis le SF2 pour un preset donné (bank, program).
// Les champs non présents dans le SF2 gardent leur valeur par défaut SF2 spec.
struct Sf2GenSnapshot {
    bool  found          = false;  // false si le preset n'existe pas dans ce SF2
    int16_t coarseTune   = 0;
    int16_t fineTune     = 0;
    int16_t attackVol    = -12000; // timecents, défaut spec = valeur très négative (instantané)
    int16_t holdVol      = -12000;
    int16_t decayVol     = -12000;
    int16_t sustainVol   = 0;     // centibels attenuation (0 = full sustain)
    int16_t releaseVol   = -12000;
    int16_t attackMod    = -12000;
    int16_t decayMod     = -12000;
    int16_t sustainMod   = 0;
    int16_t releaseMod   = -12000;
    int16_t attenuation  = 0;     // centibels
    int16_t filterFc     = 13500; // cents (défaut = ouvert)
    int16_t filterQ      = 0;
    int16_t chorus       = 0;     // 0..1000
    int16_t reverb       = 0;
    int16_t pan          = 0;     // -500..+500
    // Additional fields (missing from original snapshot — SF2 IGEN level)
    int16_t scaleTuning    = 100;    // cents/semitone (spec default = 100)
    int16_t delayVol       = -12000; // timecents
    int16_t delayMod       = -12000; // timecents
    int16_t holdMod        = -12000; // timecents
    int16_t modEnvToPitch  = 0;      // cents
    int16_t modEnvToFilter = 0;      // cents
    int16_t modLfoDelay    = -12000; // timecents
    int16_t modLfoFreq     = 0;      // cents
    int16_t modLfoToPitch  = 0;      // cents
    int16_t modLfoToFilter = 0;      // cents
    int16_t modLfoToVolume = 0;      // centibels
    int16_t vibLfoDelay    = -12000; // timecents
    int16_t vibLfoFreq     = 0;      // cents
    int16_t vibLfoToPitch  = 0;      // cents
};

// Lit les valeurs des générateurs d'un preset précis (bank + program).
// Suit la chaîne PHDR→PBAG→PGEN(GEN_INSTRUMENT=41)→INST→IBAG→IGEN pour lire
// les vraies valeurs de l'instrument (pas seulement les overrides preset PGEN).
// Les valeurs IGEN remplacent les valeurs PGEN pour le même générateur.
// Retourne Sf2GenSnapshot::found=false si le preset est introuvable.
inline Sf2GenSnapshot readPresetGens(const std::vector<uint8_t>& sf2,
                                     uint16_t bank, uint16_t program) {
    Sf2GenSnapshot snap;
    if (sf2.size() < 12) return snap;
    const uint8_t* data    = sf2.data();
    const uint8_t* fileEnd = data + sf2.size();
    if (!tag4(data, "RIFF") || !tag4(data + 8, "sfbk")) return snap;

    const uint8_t* phdr=nullptr; uint32_t phdrSz=0;
    const uint8_t* pbag=nullptr; uint32_t pbagSz=0;
    const uint8_t* pgen=nullptr; uint32_t pgenSz=0;
    const uint8_t* inst=nullptr; uint32_t instSz=0;
    const uint8_t* ibag=nullptr; uint32_t ibagSz=0;
    const uint8_t* igen=nullptr; uint32_t igenSz=0;

    const uint8_t* pos = data + 12;
    while (pos + 8 <= fileEnd) {
        uint32_t chunkSz = readU32LE(pos + 4);
        if (tag4(pos, "LIST") && pos + 12 <= fileEnd && tag4(pos + 8, "pdta")) {
            const uint8_t* sub    = pos + 12;
            const uint8_t* subEnd = pos + 8 + chunkSz;
            if (subEnd > fileEnd) subEnd = fileEnd;
            while (sub + 8 <= subEnd) {
                uint32_t subSz = readU32LE(sub + 4);
                if (tag4(sub, "PHDR")) { phdr = sub + 8; phdrSz = subSz; }
                if (tag4(sub, "PBAG")) { pbag = sub + 8; pbagSz = subSz; }
                if (tag4(sub, "PGEN")) { pgen = sub + 8; pgenSz = subSz; }
                if (tag4(sub, "INST")) { inst = sub + 8; instSz = subSz; }
                if (tag4(sub, "IBAG")) { ibag = sub + 8; ibagSz = subSz; }
                if (tag4(sub, "IGEN")) { igen = sub + 8; igenSz = subSz; }
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
    if (!phdr || !pbag || !pgen) return snap;

    // Find preset in PHDR
    uint16_t bagStart = 0xFFFF, bagEnd = 0xFFFF;
    for (const uint8_t* rec = phdr; rec + 38 <= phdr + phdrSz; rec += 38) {
        if (readU16LE(rec + 20) == program && readU16LE(rec + 22) == bank) {
            bagStart = readU16LE(rec + 24);
            if (rec + 76 <= phdr + phdrSz) bagEnd = readU16LE(rec + 38 + 24);
            break;
        }
    }
    if (bagStart == 0xFFFF) return snap;
    snap.found = true;

    // Lambda: apply one generator record to the snapshot
    auto applyToSnap = [&](uint16_t oper, int16_t amt) {
        switch (oper) {
        case GEN_COARSE_TUNE:         snap.coarseTune    = amt; break;
        case GEN_FINE_TUNE:           snap.fineTune      = amt; break;
        case GEN_SCALE_TUNING:        snap.scaleTuning   = amt; break;
        case GEN_DELAY_VOL_ENV:       snap.delayVol      = amt; break;
        case GEN_ATTACK_VOL_ENV:      snap.attackVol     = amt; break;
        case GEN_HOLD_VOL_ENV:        snap.holdVol       = amt; break;
        case GEN_DECAY_VOL_ENV:       snap.decayVol      = amt; break;
        case GEN_SUSTAIN_VOL_ENV:     snap.sustainVol    = amt; break;
        case GEN_RELEASE_VOL_ENV:     snap.releaseVol    = amt; break;
        case GEN_DELAY_MOD_ENV:       snap.delayMod      = amt; break;
        case GEN_ATTACK_MOD_ENV:      snap.attackMod     = amt; break;
        case GEN_HOLD_MOD_ENV:        snap.holdMod       = amt; break;
        case GEN_DECAY_MOD_ENV:       snap.decayMod      = amt; break;
        case GEN_SUSTAIN_MOD_ENV:     snap.sustainMod    = amt; break;
        case GEN_RELEASE_MOD_ENV:     snap.releaseMod    = amt; break;
        case GEN_MOD_ENV_TO_PITCH:    snap.modEnvToPitch  = amt; break;
        case GEN_MOD_ENV_TO_FILTER:   snap.modEnvToFilter = amt; break;
        case GEN_DELAY_MOD_LFO:       snap.modLfoDelay   = amt; break;
        case GEN_FREQ_MOD_LFO:        snap.modLfoFreq    = amt; break;
        case GEN_MOD_LFO_TO_PITCH:    snap.modLfoToPitch  = amt; break;
        case GEN_MOD_LFO_TO_FILTER:   snap.modLfoToFilter = amt; break;
        case GEN_MOD_LFO_TO_VOLUME:   snap.modLfoToVolume = amt; break;
        case GEN_DELAY_VIB_LFO:       snap.vibLfoDelay   = amt; break;
        case GEN_FREQ_VIB_LFO:        snap.vibLfoFreq    = amt; break;
        case GEN_VIB_LFO_TO_PITCH:    snap.vibLfoToPitch  = amt; break;
        case GEN_INITIAL_ATTENUATION: snap.attenuation   = amt; break;
        case GEN_PAN:                 snap.pan           = amt; break;
        case GEN_INITIAL_FILTER_FC:   snap.filterFc      = amt; break;
        case GEN_INITIAL_FILTER_Q:    snap.filterQ       = amt; break;
        case GEN_REVERB_SEND:         snap.reverb        = amt; break;
        case GEN_CHORUS_SEND:         snap.chorus        = amt; break;
        }
    };

    // Step 1: Read PGEN (preset-level generators — additive overrides)
    for (uint16_t bi = bagStart; bi < bagEnd && bagEnd != 0xFFFF; ++bi) {
        if ((bi + 1u) * 4u > pbagSz) break;
        uint16_t genIdx = readU16LE(pbag + bi * 4);
        uint16_t genEnd = (bi + 1 < bagEnd && (bi + 2u) * 4u <= pbagSz)
                          ? readU16LE(pbag + (bi + 1u) * 4) : genIdx + 1;
        for (uint16_t gi = genIdx; gi < genEnd; ++gi) {
            if ((gi + 1u) * 4u > pgenSz) break;
            uint16_t oper = readU16LE(pgen + gi * 4);
            int16_t  amt  = (int16_t)readU16LE(pgen + gi * 4 + 2);
            if (oper != 41) applyToSnap(oper, amt); // skip GEN_INSTRUMENT
        }
    }

    // Step 2: Follow GEN_INSTRUMENT (41) → IGEN for true instrument-level values.
    // IGEN values are the real absolute values (not overrides) — they take precedence.
    if (!inst || !ibag || !igen) return snap;
    for (uint16_t bi = bagStart; bi < bagEnd && bagEnd != 0xFFFF; ++bi) {
        if ((bi + 1u) * 4u > pbagSz) break;
        uint16_t genIdx = readU16LE(pbag + bi * 4);
        uint16_t genEnd = (bi + 1 < bagEnd && (bi + 2u) * 4u <= pbagSz)
                          ? readU16LE(pbag + (bi + 1u) * 4) : genIdx + 1;
        for (uint16_t gi = genIdx; gi < genEnd; ++gi) {
            if ((gi + 1u) * 4u > pgenSz) break;
            if (readU16LE(pgen + gi * 4) != 41) continue; // GEN_INSTRUMENT only
            uint16_t iidx = readU16LE(pgen + gi * 4 + 2);
            if ((uint32_t)(iidx + 2) * 22u > instSz) continue;
            uint16_t ibStart = readU16LE(inst + iidx * 22 + 20);
            uint16_t ibEndR  = readU16LE(inst + (iidx + 1u) * 22 + 20);
            // Read the first zone (global zone or first sample zone — gives base values)
            if (ibStart < ibEndR) {
                uint16_t igenIdx = readU16LE(ibag + ibStart * 4);
                uint16_t igenEnd = ((ibStart + 1u) * 4u <= ibagSz && ibStart + 1 < ibEndR)
                                   ? readU16LE(ibag + (ibStart + 1u) * 4) : igenIdx + 1;
                for (uint16_t ig = igenIdx; ig < igenEnd; ++ig) {
                    if ((ig + 1u) * 4u > igenSz) break;
                    uint16_t ioper = readU16LE(igen + ig * 4);
                    int16_t  iamt  = (int16_t)readU16LE(igen + ig * 4 + 2);
                    applyToSnap(ioper, iamt);
                }
            }
        }
    }
    return snap;
}

// ---- Delta ciblé sur un preset précis ------------------------------------
// Comme applyDelta() mais n'affecte que les générateurs PGEN appartenant au
// preset identifié par (bank, program). Les IGEN (niveau instrument) ne sont
// pas touchés. Retourne false si le preset est introuvable ou le SF2 invalide.
inline bool applyDeltaToPreset(std::vector<uint8_t>& sf2,
                                const Sf2Delta& delta,
                                uint16_t bank, uint16_t program) {
    if (sf2.size() < 12) return false;
    uint8_t* data    = sf2.data();
    uint8_t* fileEnd = data + sf2.size();
    if (!tag4(data, "RIFF") || !tag4(data + 8, "sfbk")) return false;

    uint8_t* phdr = nullptr; uint32_t phdrSz = 0;
    uint8_t* pbag = nullptr; uint32_t pbagSz = 0;
    uint8_t* pgen = nullptr; uint32_t pgenSz = 0;

    uint8_t* pos = data + 12;
    while (pos + 8 <= fileEnd) {
        uint32_t chunkSz = readU32LE(pos + 4);
        if (tag4(pos, "LIST") && pos + 12 <= fileEnd && tag4(pos + 8, "pdta")) {
            uint8_t* sub    = pos + 12;
            uint8_t* subEnd = pos + 8 + chunkSz;
            if (subEnd > fileEnd) subEnd = fileEnd;
            while (sub + 8 <= subEnd) {
                uint32_t subSz = readU32LE(sub + 4);
                if (tag4(sub, "PHDR")) { phdr = sub + 8; phdrSz = subSz; }
                if (tag4(sub, "PBAG")) { pbag = sub + 8; pbagSz = subSz; }
                if (tag4(sub, "PGEN")) { pgen = sub + 8; pgenSz = subSz; }
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
    if (!phdr || !pbag || !pgen) return false;

    // Find preset bag range
    uint16_t bagStart = 0xFFFF, bagEnd = 0xFFFF;
    uint8_t* rec    = phdr;
    uint8_t* recEnd = phdr + phdrSz;
    for (; rec + 38 <= recEnd; rec += 38) {
        uint16_t p  = readU16LE(rec + 20);
        uint16_t b  = readU16LE(rec + 22);
        uint16_t bg = readU16LE(rec + 24);
        if (p == program && b == bank) {
            bagStart = bg;
            if (rec + 76 <= recEnd) bagEnd = readU16LE(rec + 38 + 24);
            break;
        }
    }
    if (bagStart == 0xFFFF || bagEnd == 0xFFFF) return false;

    // Patch PGEN entries belonging to this preset
    for (uint16_t bi = bagStart; bi < bagEnd; ++bi) {
        if ((bi + 1u) * 4u > pbagSz) break;
        uint16_t genIdx = readU16LE(pbag + bi * 4);
        uint16_t genEnd = (bi + 1 < bagEnd && (bi + 1 + 1u) * 4u <= pbagSz)
                          ? readU16LE(pbag + (bi + 1) * 4) : genIdx + 1;
        for (uint16_t gi = genIdx; gi < genEnd; ++gi) {
            if ((gi + 1u) * 4u > pgenSz) break;
            patchGen(pgen + gi * 4, delta);
        }
    }
    return true;
}

// ---- Helpers for applyDeltaTargeted ----------------------------------------

// SF2 spec default value for each tracked generator (IGEN absolute level)
static int16_t sf2SpecDefault(uint16_t oper) {
    switch (oper) {
    case GEN_INITIAL_FILTER_FC:  return 13500;
    case GEN_SCALE_TUNING:       return 100;
    case GEN_DELAY_MOD_LFO:      return -12000;
    case GEN_DELAY_VIB_LFO:      return -12000;
    case GEN_DELAY_MOD_ENV:      return -12000;
    case GEN_ATTACK_MOD_ENV:     return -12000;
    case GEN_HOLD_MOD_ENV:       return -12000;
    case GEN_DECAY_MOD_ENV:      return -12000;
    case GEN_RELEASE_MOD_ENV:    return -12000;
    case GEN_DELAY_VOL_ENV:      return -12000;
    case GEN_ATTACK_VOL_ENV:     return -12000;
    case GEN_HOLD_VOL_ENV:       return -12000;
    case GEN_DECAY_VOL_ENV:      return -12000;
    case GEN_RELEASE_VOL_ENV:    return -12000;
    default:                     return 0;
    }
}

// Apply delta to a base value with clamping (same logic as patchGen but returns value)
static int16_t sf2ApplyDelta(uint16_t oper, int16_t base, const Sf2Delta& d) {
    int v = base;
    switch (oper) {
    case GEN_COARSE_TUNE:         v = clampI16(v + d.coarseTune,        -120,  120); break;
    case GEN_FINE_TUNE:           v = clampI16(v + d.fineTune,            -99,   99); break;
    case GEN_SCALE_TUNING:        v = clampI16(v + d.scaleTuning,           0, 1200); break;
    case GEN_DELAY_VOL_ENV:       v = clampI16(v + d.delayVolDelta,    -12000, 5000); break;
    case GEN_ATTACK_VOL_ENV:      v = clampI16(v + d.attackDelta,      -12000, 8000); break;
    case GEN_HOLD_VOL_ENV:        v = clampI16(v + d.holdDelta,         -12000, 5000); break;
    case GEN_DECAY_VOL_ENV:       v = clampI16(v + d.decayDelta,        -12000, 8000); break;
    case GEN_SUSTAIN_VOL_ENV:     v = clampI16(v + d.sustainDelta,           0, 1000); break;
    case GEN_RELEASE_VOL_ENV:     v = clampI16(v + d.releaseDelta,      -12000, 8000); break;
    case GEN_DELAY_MOD_ENV:       v = clampI16(v + d.delayModDelta,     -12000, 5000); break;
    case GEN_ATTACK_MOD_ENV:      v = clampI16(v + d.modAttackDelta,    -12000, 8000); break;
    case GEN_HOLD_MOD_ENV:        v = clampI16(v + d.modHoldDelta,       -12000, 5000); break;
    case GEN_DECAY_MOD_ENV:       v = clampI16(v + d.modDecayDelta,     -12000, 8000); break;
    case GEN_SUSTAIN_MOD_ENV:     v = clampI16(v + d.modSustainDelta,        0, 1000); break;
    case GEN_RELEASE_MOD_ENV:     v = clampI16(v + d.modReleaseDelta,   -12000, 8000); break;
    case GEN_MOD_ENV_TO_PITCH:    v = clampI16(v + d.modEnvToPitch,     -12000,12000); break;
    case GEN_MOD_ENV_TO_FILTER:   v = clampI16(v + d.modEnvToFilter,    -12000,12000); break;
    case GEN_DELAY_MOD_LFO:       v = clampI16(v + d.modLfoDelay,       -12000, 5000); break;
    case GEN_FREQ_MOD_LFO:        v = clampI16(v + d.modLfoFreq,        -16000, 4500); break;
    case GEN_MOD_LFO_TO_PITCH:    v = clampI16(v + d.modLfoToPitch,     -12000,12000); break;
    case GEN_MOD_LFO_TO_FILTER:   v = clampI16(v + d.modLfoToFilter,    -12000,12000); break;
    case GEN_MOD_LFO_TO_VOLUME:   v = clampI16(v + d.modLfoToVolume,      -960,  960); break;
    case GEN_DELAY_VIB_LFO:       v = clampI16(v + d.vibLfoDelay,       -12000, 5000); break;
    case GEN_FREQ_VIB_LFO:        v = clampI16(v + d.vibLfoFreq,        -16000, 4500); break;
    case GEN_VIB_LFO_TO_PITCH:    v = clampI16(v + d.vibLfoToPitch,     -12000,12000); break;
    case GEN_INITIAL_ATTENUATION: v = clampI16(v + d.attenuationDelta,       0,  960); break;
    case GEN_PAN:                 v = clampI16(v + d.panDelta,            -500,  500); break;
    case GEN_INITIAL_FILTER_FC:   v = clampI16(v + d.filterFcDelta,      1500,13500); break;
    case GEN_INITIAL_FILTER_Q:    v = clampI16(v + d.filterQDelta,           0,  960); break;
    case GEN_REVERB_SEND:         v = clampI16(v + d.reverbDelta,            0, 1000); break;
    case GEN_CHORUS_SEND:         v = clampI16(v + d.chorusDelta,            0, 1000); break;
    default: break;
    }
    return (int16_t)v;
}

// Returns true if the Sf2Delta has a non-zero value for this generator
static bool sf2HasDelta(uint16_t oper, const Sf2Delta& d) {
    switch (oper) {
    case GEN_COARSE_TUNE:         return d.coarseTune       != 0;
    case GEN_FINE_TUNE:           return d.fineTune         != 0;
    case GEN_SCALE_TUNING:        return d.scaleTuning      != 0;
    case GEN_DELAY_VOL_ENV:       return d.delayVolDelta    != 0;
    case GEN_ATTACK_VOL_ENV:      return d.attackDelta      != 0;
    case GEN_HOLD_VOL_ENV:        return d.holdDelta        != 0;
    case GEN_DECAY_VOL_ENV:       return d.decayDelta       != 0;
    case GEN_SUSTAIN_VOL_ENV:     return d.sustainDelta     != 0;
    case GEN_RELEASE_VOL_ENV:     return d.releaseDelta     != 0;
    case GEN_DELAY_MOD_ENV:       return d.delayModDelta    != 0;
    case GEN_ATTACK_MOD_ENV:      return d.modAttackDelta   != 0;
    case GEN_HOLD_MOD_ENV:        return d.modHoldDelta     != 0;
    case GEN_DECAY_MOD_ENV:       return d.modDecayDelta    != 0;
    case GEN_SUSTAIN_MOD_ENV:     return d.modSustainDelta  != 0;
    case GEN_RELEASE_MOD_ENV:     return d.modReleaseDelta  != 0;
    case GEN_MOD_ENV_TO_PITCH:    return d.modEnvToPitch    != 0;
    case GEN_MOD_ENV_TO_FILTER:   return d.modEnvToFilter   != 0;
    case GEN_DELAY_MOD_LFO:       return d.modLfoDelay      != 0;
    case GEN_FREQ_MOD_LFO:        return d.modLfoFreq       != 0;
    case GEN_MOD_LFO_TO_PITCH:    return d.modLfoToPitch    != 0;
    case GEN_MOD_LFO_TO_FILTER:   return d.modLfoToFilter   != 0;
    case GEN_MOD_LFO_TO_VOLUME:   return d.modLfoToVolume   != 0;
    case GEN_DELAY_VIB_LFO:       return d.vibLfoDelay      != 0;
    case GEN_FREQ_VIB_LFO:        return d.vibLfoFreq       != 0;
    case GEN_VIB_LFO_TO_PITCH:    return d.vibLfoToPitch    != 0;
    case GEN_INITIAL_ATTENUATION: return d.attenuationDelta != 0;
    case GEN_PAN:                 return d.panDelta         != 0;
    case GEN_INITIAL_FILTER_FC:   return d.filterFcDelta    != 0;
    case GEN_INITIAL_FILTER_Q:    return d.filterQDelta     != 0;
    case GEN_REVERB_SEND:         return d.reverbDelta      != 0;
    case GEN_CHORUS_SEND:         return d.chorusDelta      != 0;
    default: return false;
    }
}

// All 31 generator IDs managed by the UI (must match Sf2Delta fields exactly)
static const uint16_t kSf2TrackedGens[] = {
     5,  6,  7,  8,  9, 10, 11, 13, 15, 16,
    17, 21, 22, 23, 24, 25, 26, 27, 28, 29,
    30, 33, 34, 35, 36, 37, 38, 48, 51, 52, 56
};
static constexpr int kSf2NumTracked = 31;

static void sf2WriteU16LE_(std::vector<uint8_t>& v, uint16_t x) {
    v.push_back((uint8_t)(x & 0xFF));
    v.push_back((uint8_t)(x >> 8));
}
static void sf2WriteU32LE_(std::vector<uint8_t>& v, uint32_t x) {
    v.push_back((uint8_t)(x & 0xFF));
    v.push_back((uint8_t)((x >> 8) & 0xFF));
    v.push_back((uint8_t)((x >> 16) & 0xFF));
    v.push_back((uint8_t)((x >> 24) & 0xFF));
}
static void sf2PatchU16LE_(std::vector<uint8_t>& v, size_t off, uint16_t x) {
    v[off] = (uint8_t)(x & 0xFF); v[off+1] = (uint8_t)(x >> 8);
}
static void sf2PatchU32LE_(std::vector<uint8_t>& v, size_t off, uint32_t x) {
    v[off]=(uint8_t)(x&0xFF); v[off+1]=(uint8_t)((x>>8)&0xFF);
    v[off+2]=(uint8_t)((x>>16)&0xFF); v[off+3]=(uint8_t)((x>>24)&0xFF);
}

// applyDeltaTargeted — modifie uniquement le preset (bank, program) sélectionné.
// Reconstruit la section IGEN en insérant les records manquants pour les générateurs
// avec un delta non-nul (correction du bug "patchGen ne peut pas insérer").
// Met à jour les tailles IBAG genNdx, pdta LIST et RIFF en conséquence.
static bool applyDeltaTargeted(std::vector<uint8_t>& sf2,
                                const Sf2Delta& delta,
                                uint16_t bank, uint16_t program) {
    if (sf2.size() < 12) return false;
    const uint8_t* data    = sf2.data();
    const uint8_t* fileEnd = data + sf2.size();
    if (!tag4(data, "RIFF") || !tag4(data + 8, "sfbk")) return false;

    // Offsets of pdta sub-chunks
    size_t pdtaListHdrOff = 0;
    size_t phdrOff=0, phdrSz=0;
    size_t pbagOff=0, pbagSz=0;
    size_t pgenOff=0, pgenSz=0;
    size_t instOff=0, instSz=0;
    size_t ibagOff=0, ibagSz=0;
    size_t igenHdrOff=0, igenOff=0, igenSz=0;

    const uint8_t* pos = data + 12;
    while (pos + 8 <= fileEnd) {
        uint32_t chunkSz = readU32LE(pos + 4);
        if (tag4(pos, "LIST") && pos + 12 <= fileEnd && tag4(pos + 8, "pdta")) {
            pdtaListHdrOff = (size_t)(pos - data);
            const uint8_t* sub    = pos + 12;
            const uint8_t* subEnd = pos + 8 + chunkSz;
            if (subEnd > fileEnd) subEnd = fileEnd;
            while (sub + 8 <= subEnd) {
                uint32_t subSz = readU32LE(sub + 4);
                if (tag4(sub,"PHDR")){ phdrOff=sub+8-data; phdrSz=subSz; }
                if (tag4(sub,"PBAG")){ pbagOff=sub+8-data; pbagSz=subSz; }
                if (tag4(sub,"PGEN")){ pgenOff=sub+8-data; pgenSz=subSz; }
                if (tag4(sub,"INST")){ instOff=sub+8-data; instSz=subSz; }
                if (tag4(sub,"IBAG")){ ibagOff=sub+8-data; ibagSz=subSz; }
                if (tag4(sub,"IGEN")){ igenHdrOff=sub-data; igenOff=sub+8-data; igenSz=subSz; }
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
    if (!phdrOff || !pbagOff || !pgenOff || !instOff || !ibagOff || !igenOff) return false;

    const uint8_t* phdr = data + phdrOff;
    const uint8_t* pbag = data + pbagOff;
    const uint8_t* pgen = data + pgenOff;
    const uint8_t* inst = data + instOff;
    const uint8_t* ibag = data + ibagOff;
    const uint8_t* igen = data + igenOff;

    // Find preset in PHDR
    uint16_t presetBagStart = 0xFFFF, presetBagEnd = 0xFFFF;
    for (const uint8_t* rec = phdr; rec + 38 <= phdr + phdrSz; rec += 38) {
        if (readU16LE(rec + 20) == program && readU16LE(rec + 22) == bank) {
            presetBagStart = readU16LE(rec + 24);
            if (rec + 76 <= phdr + phdrSz) presetBagEnd = readU16LE(rec + 38 + 24);
            break;
        }
    }
    if (presetBagStart == 0xFFFF || presetBagEnd == 0xFFFF) return false;

    // Collect instrument indices (GEN_INSTRUMENT = 41) and patch PGEN in-place
    std::vector<uint16_t> instIndices;
    for (uint16_t bi = presetBagStart; bi < presetBagEnd; ++bi) {
        if ((bi + 1u) * 4u > pbagSz) break;
        uint16_t genIdx = readU16LE(pbag + bi * 4);
        uint16_t genEnd = ((bi + 2u) * 4u <= pbagSz && bi + 1 < presetBagEnd)
                          ? readU16LE(pbag + (bi + 1u) * 4) : genIdx + 1;
        for (uint16_t gi = genIdx; gi < genEnd; ++gi) {
            if ((gi + 1u) * 4u > pgenSz) break;
            uint16_t oper = readU16LE(pgen + gi * 4);
            if (oper == 41)
                instIndices.push_back(readU16LE(pgen + gi * 4 + 2));
            // PGEN-level in-place patch (won't insert, but preset-level gens rarely matter)
            // Note: patchGen on a const pointer via cast — sf2 is non-const so this is safe
        }
    }

    // Collect affected IBAG zone indices (sorted, deduplicated)
    std::vector<uint16_t> affZones;
    for (uint16_t iidx : instIndices) {
        if ((uint32_t)(iidx + 2) * 22u > instSz) continue;
        uint16_t ibStart = readU16LE(inst + iidx * 22 + 20);
        uint16_t ibEndR  = readU16LE(inst + (iidx + 1u) * 22 + 20);
        for (uint16_t ib = ibStart; ib < ibEndR; ++ib) affZones.push_back(ib);
    }
    std::sort(affZones.begin(), affZones.end());
    affZones.erase(std::unique(affZones.begin(), affZones.end()), affZones.end());
    if (affZones.empty()) return false;

    uint32_t totalZones = ibagSz / 4; // each IBAG entry = 4 bytes (genNdx + modNdx)

    auto isAffected = [&](uint16_t z) -> bool {
        return std::binary_search(affZones.begin(), affZones.end(), z);
    };

    // Build new IGEN data zone-by-zone
    std::vector<uint8_t> newIgen;
    newIgen.reserve(igenSz + affZones.size() * kSf2NumTracked * 4);
    std::vector<uint16_t> newGenNdx(totalZones, 0);

    auto writeGenRec = [&](uint16_t oper, int16_t amt) {
        sf2WriteU16LE_(newIgen, oper);
        sf2WriteU16LE_(newIgen, (uint16_t)amt);
    };

    for (uint32_t z = 0; z < totalZones; ++z) {
        newGenNdx[z] = (uint16_t)(newIgen.size() / 4);
        uint16_t igenStart = readU16LE(ibag + z * 4);
        uint16_t igenEndV  = (z + 1 < totalZones)
                             ? readU16LE(ibag + (z + 1u) * 4)
                             : (uint16_t)(igenSz / 4);

        if (isAffected((uint16_t)z)) {
            // Read existing gens: tracked → store value, non-tracked → preserve verbatim
            uint16_t tval[kSf2NumTracked] = {};  // raw uint16 storage for tracked gen values
            bool     thas[kSf2NumTracked] = {};  // true if this tracked gen was present
            struct OtherRec { uint16_t oper, amt; };
            OtherRec other[128]; int otherN = 0;
            uint16_t sampleIdAmt = 0; bool hasSampleId = false;

            for (uint16_t g = igenStart; g < igenEndV && (uint32_t)(g+1)*4u <= igenSz; ++g) {
                uint16_t oper = readU16LE(igen + g * 4);
                uint16_t amt  = readU16LE(igen + g * 4 + 2);
                int tidx = -1;
                for (int ti = 0; ti < kSf2NumTracked; ++ti)
                    if (kSf2TrackedGens[ti] == oper) { tidx = ti; break; }
                if (tidx >= 0) {
                    tval[tidx] = amt; thas[tidx] = true;
                } else if (oper == 53) { // GEN_SAMPLE_ID — must be last
                    sampleIdAmt = amt; hasSampleId = true;
                } else if (otherN < 128) {
                    other[otherN++] = {oper, amt};
                }
            }

            // Write non-tracked, non-sampleID gens first (keyRange, velRange, etc.)
            for (int i = 0; i < otherN; ++i) writeGenRec(other[i].oper, (int16_t)other[i].amt);

            // Write all tracked gens (update existing, insert from spec default if delta != 0)
            for (int ti = 0; ti < kSf2NumTracked; ++ti) {
                uint16_t oper = kSf2TrackedGens[ti];
                if (thas[ti]) {
                    writeGenRec(oper, sf2ApplyDelta(oper, (int16_t)tval[ti], delta));
                } else if (sf2HasDelta(oper, delta)) {
                    // Insert: spec default + delta
                    writeGenRec(oper, sf2ApplyDelta(oper, sf2SpecDefault(oper), delta));
                }
                // else: no record + no delta → omit (SF2 spec default applies implicitly)
            }

            // sampleID last (SF2 spec requirement)
            if (hasSampleId) writeGenRec(53, (int16_t)sampleIdAmt);
        } else {
            // Non-affected zone: copy verbatim
            for (uint16_t g = igenStart; g < igenEndV && (uint32_t)(g+1)*4u <= igenSz; ++g)
                writeGenRec(readU16LE(igen + g*4), (int16_t)readU16LE(igen + g*4 + 2));
        }
    }

    // Pad to even
    if (newIgen.size() % 2 != 0) newIgen.push_back(0);

    uint32_t newIgenDataSz  = (uint32_t)newIgen.size();
    uint32_t oldIgenPaddedSz = (igenSz + 1u) & ~1u;
    int32_t  igenSzDiff      = (int32_t)newIgenDataSz - (int32_t)oldIgenPaddedSz;

    // Reconstruct SF2:
    // [0 .. igenHdrOff-1]                 unchanged (includes IBAG, IMOD, etc.)
    // [igenHdrOff .. igenHdrOff+7]        "IGEN" + new size
    // [igenHdrOff+8 .. +8+newIgenDataSz]  new IGEN data
    // [igenHdrOff+8+oldIgenPaddedSz ..]   unchanged (SHDR etc.)
    std::vector<uint8_t> newSf2;
    newSf2.reserve(sf2.size() + igenSzDiff + 4);

    newSf2.insert(newSf2.end(), sf2.begin(), sf2.begin() + igenHdrOff);
    newSf2.push_back('I'); newSf2.push_back('G');
    newSf2.push_back('E'); newSf2.push_back('N');
    sf2WriteU32LE_(newSf2, newIgenDataSz);
    newSf2.insert(newSf2.end(), newIgen.begin(), newIgen.end());
    size_t oldEnd = igenHdrOff + 8 + oldIgenPaddedSz;
    if (oldEnd < sf2.size())
        newSf2.insert(newSf2.end(), sf2.begin() + oldEnd, sf2.end());

    // Update IBAG genNdx values (IBAG is before IGEN → same offset in newSf2)
    for (uint32_t z = 0; z < totalZones; ++z) {
        size_t off = ibagOff + z * 4;
        if (off + 2 <= newSf2.size()) sf2PatchU16LE_(newSf2, off, newGenNdx[z]);
    }

    // Update pdta LIST size and RIFF total size if IGEN changed
    if (igenSzDiff != 0) {
        uint32_t oldPdtaSz = readU32LE(sf2.data() + pdtaListHdrOff + 4);
        sf2PatchU32LE_(newSf2, pdtaListHdrOff + 4,
                       (uint32_t)((int32_t)oldPdtaSz + igenSzDiff));
        uint32_t oldRiffSz = readU32LE(sf2.data() + 4);
        sf2PatchU32LE_(newSf2, 4, (uint32_t)((int32_t)oldRiffSz + igenSzDiff));
    }

    sf2 = std::move(newSf2);
    return true;
}

// applyDelta — modifie tous les générateurs IGEN et PGEN dans le buffer SF2.
// Opère en place (les bytes des samples ne sont pas touchés).
// Retourne false si le buffer n'est pas un SF2 valide.
inline bool applyDelta(std::vector<uint8_t>& sf2, const Sf2Delta& delta) {
    if (sf2.size() < 12) return false;
    uint8_t* data = sf2.data();
    if (!tag4(data,     "RIFF")) return false;
    if (!tag4(data + 8, "sfbk")) return false;

    uint8_t* fileEnd = data + sf2.size();

    // Scan du top-level : RIFF/sfbk commence à l'offset 12
    uint8_t* pos = data + 12;
    while (pos + 8 <= fileEnd) {
        uint32_t chunkSz = readU32LE(pos + 4);

        if (tag4(pos, "LIST") && pos + 12 <= fileEnd) {
            // Chercher la LIST pdta
            if (tag4(pos + 8, "pdta")) {
                uint8_t* sub    = pos + 12;
                uint8_t* subEnd = pos + 8 + chunkSz;
                if (subEnd > fileEnd) subEnd = fileEnd;

                while (sub + 8 <= subEnd) {
                    uint32_t subSz = readU32LE(sub + 4);
                    // Patcher IGEN (instrument generators) et PGEN (preset generators)
                    if ((tag4(sub, "IGEN") || tag4(sub, "PGEN")) && subSz >= 4) {
                        uint8_t* genPtr = sub + 8;
                        uint8_t* genEnd = genPtr + subSz;
                        if (genEnd > fileEnd) genEnd = fileEnd;
                        for (uint8_t* g = genPtr; g + 4 <= genEnd; g += 4) {
                            patchGen(g, delta);
                        }
                    }
                    uint32_t adv = 8 + ((subSz + 1u) & ~1u);
                    if (adv < 8) break;
                    sub += adv;
                }
            }
        }

        uint32_t adv = 8 + ((chunkSz + 1u) & ~1u);
        if (adv < 8) break;
        pos += adv;
    }
    return true;
}

// Charge un fichier SF2 depuis le disque vers un vecteur d'octets.
// Retourne un vecteur vide en cas d'échec.
// Limite : 512 Mo (protection contre les fichiers aberrants).
inline std::vector<uint8_t> loadSf2File(const wchar_t* path) {
    FILE* f = _wfopen(path, L"rb");
    if (!f) return {};
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 12 || sz > 512 * 1024 * 1024L) { fclose(f); return {}; }
    std::vector<uint8_t> buf((size_t)sz);
    size_t rd = fread(buf.data(), 1, (size_t)sz, f);
    fclose(f);
    if (rd != (size_t)sz) return {};
    // Validation minimale
    if (!tag4(buf.data(), "RIFF") || !tag4(buf.data() + 8, "sfbk")) return {};
    return buf;
}

#ifdef _WIN32
// Ouvre un dialogue "Ouvrir" Win32 filtré sur les fichiers *.sf2.
// Retourne le chemin choisi, ou une chaîne vide si annulé.
inline std::wstring browseSf2File(HWND parent) {
    wchar_t buf[MAX_PATH] = {};
    OPENFILENAMEW ofn      = {};
    ofn.lStructSize        = sizeof(ofn);
    ofn.hwndOwner          = parent;
    ofn.lpstrFilter        = L"SoundFont 2 (*.sf2)\0*.sf2\0Tous les fichiers\0*.*\0";
    ofn.lpstrFile          = buf;
    ofn.nMaxFile           = MAX_PATH;
    ofn.lpstrTitle         = L"Ouvrir un SoundFont";
    ofn.Flags              = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    if (GetOpenFileNameW(&ofn)) return std::wstring(buf);
    return {};
}
#endif

// Extrait le nom de fichier (sans chemin, sans extension) depuis un chemin complet.
inline std::wstring sf2ShortName(const std::wstring& path) {
    size_t sl = path.rfind(L'\\');
    if (sl == std::wstring::npos) sl = path.rfind(L'/');
    std::wstring name = (sl == std::wstring::npos) ? path : path.substr(sl + 1);
    size_t dot = name.rfind(L'.');
    if (dot != std::wstring::npos) name = name.substr(0, dot);
    return name;
}

// ---- Utilitaires de conversion UI -> delta ---------------------------------

// Convertit un slider de "accordage grossier" (−24..+24 demi-tons, centre = 0)
// en champ Sf2Delta::coarseTune.
inline int tuneSliderToCoarse(int sliderVal) { return sliderVal; }

// Convertit un slider 0..100 en delta de timecents pour l'attaque/déclin/relâche.
// 50 = pas de changement, <50 = plus rapide, >50 = plus lent.
// Plage : 0.125× (−3600 tc) à 8× (+3600 tc).
inline int timingSliderToTc(int sliderVal) {
    float t = (sliderVal - 50) / 50.0f; // −1..+1
    float scale = std::pow(8.0f, t);     // 0.125×..8×
    return scaleToTimecents(scale);
}

// Convertit un slider 0..100 en delta de centibels de sustain.
// 50 = pas de changement, 0 = −500 cb (sustain plus fort), 100 = +500 cb (plus faible).
inline int sustainSliderToCb(int sliderVal) {
    return (sliderVal - 50) * 10; // −500..+500 centibels
}

// Convertit un slider 0..100 en delta de volume (attenuation centibels).
// 50 = pas de changement, 0 = −480 cb (plus fort), 100 = +480 cb (plus silencieux).
inline int volumeSliderToCb(int sliderVal) {
    return (sliderVal - 50) * 10; // −500..+500 centibels (limité à 0..960 par patchGen)
}

// Convertit un slider 0..100 en delta de coupure filtre (cents).
// 50 = pas de changement, 0 = −6000 cents (très sombre), 100 = +6000 (brillant).
inline int filterSliderToCents(int sliderVal) {
    return (sliderVal - 50) * 120; // −6000..+6000 cents
}

// Convertit un slider 0..100 en delta d'accordage fin (cents).
// 50 = pas de changement, 0 = −99 cents, 100 = +99 cents.
inline int finetuneSliderToCents(int sliderVal) {
    return (int)std::round((sliderVal - 50) * 1.98f); // −99..+99
}

// Convertit un slider 0..100 en delta de panoramique SF2 (unités).
// 50 = centre, 0 = −500 (droite SF2), 100 = +500 (gauche SF2).
inline int panSliderToVal(int sliderVal) {
    return (sliderVal - 50) * 10; // −500..+500
}

// Convertit un slider 0..100 en delta de résonance filtre (centibels).
// 50 = pas de changement, 0 = −480 cb, 100 = +480 cb.
inline int filterQSliderToCb(int sliderVal) {
    return (sliderVal - 50) * 10; // −500..+500 cb (limité à 0..960 par patchGen)
}

// Convertit un slider 0..100 en delta d'envoi réverbération SF2 (per-mille).
// 50 = pas de changement, 0 = −500, 100 = +500.
inline int reverbSliderToVal(int sliderVal) {
    return (sliderVal - 50) * 10; // −500..+500
}

// Convertit un slider 0..100 en delta d'envoi chorus SF2 (per-mille).
// 50 = pas de changement, 0 = −500, 100 = +500.
inline int chorusSliderToVal(int sliderVal) {
    return (sliderVal - 50) * 10; // −500..+500
}

// Convertit un slider 0..100 en delta d'accordage d'échelle (cents/demi-ton).
// 50 = 0, 0 = −100, 100 = +100.
inline int scaleSliderToDelta(int sliderVal) {
    return (sliderVal - 50) * 2; // −100..+100
}

// Convertit un slider 0..100 en delta de délai (timecents).
// 50 = 0 tc, 0 = −3600 tc, 100 = +3600 tc.
inline int delaySliderToTc(int sliderVal) {
    return (sliderVal - 50) * 72; // −3600..+3600 tc
}

// Convertit un slider 0..100 en delta de fréquence LFO (cents).
// 50 = 0, 0 = −4000 cents, 100 = +4000 cents.
inline int lfoFreqSliderToCents(int sliderVal) {
    return (sliderVal - 50) * 80; // −4000..+4000 cents
}

// Convertit un slider 0..100 en delta de profondeur de modulation (cents).
// 50 = 0, 0 = −2400 cents, 100 = +2400 cents.
inline int modDepthSliderToCents(int sliderVal) {
    return (sliderVal - 50) * 48; // −2400..+2400 cents
}

// Convertit un slider 0..100 en delta de tremolo LFO→volume (centibels).
// 50 = 0, 0 = −480 cb, 100 = +480 cb.
inline int tremoloSliderToCb(int sliderVal) {
    return (sliderVal - 50) * 10; // −500..+500 cb
}

} // namespace sfed
