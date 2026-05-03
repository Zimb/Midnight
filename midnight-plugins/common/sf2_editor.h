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

// ---- Structure delta -------------------------------------------------------
// Tous les champs sont additifs sur les valeurs existantes dans le SF2.
// Laisser à 0 = aucun changement pour ce paramètre.
struct Sf2Delta {
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
};

// Convertit un multiplicateur de temps (ex: 2.0 = doubler la durée) en delta timecents.
// scaleToTimecents(2.0f) = +1200   (attaque 2× plus lente)
// scaleToTimecents(0.5f) = −1200   (attaque 2× plus rapide)
inline int scaleToTimecents(float scale) {
    if (scale <= 0.001f) return -12000;
    return (int)std::round(1200.0 * std::log2((double)scale));
}

// Petites fonctions de lecture/écriture binaire --
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

// Applique le delta sur un enregistrement générateur (4 bytes : uint16 oper + int16 amount).
// Modifie les bytes en place. Retourne true si modifié.
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
    default: hit = false; break;
    }

    if (hit && out != amt) writeI16LE(rec + 2, (int16_t)out);
    return hit;
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
// 50 = pas de changement, 0 = −300 cb (sustain plus fort), 100 = +300 cb (plus faible).
inline int sustainSliderToCb(int sliderVal) {
    return (sliderVal - 50) * 6; // −300..+300 centibels
}

// Convertit un slider 0..100 en delta de volume (attenuation centibels).
// 50 = pas de changement, 0 = −200 cb (plus fort), 100 = +200 cb (plus silencieux).
inline int volumeSliderToCb(int sliderVal) {
    return (sliderVal - 50) * 4; // −200..+200 centibels
}

// Convertit un slider 0..100 en delta de coupure filtre (cents).
// 50 = pas de changement, 0 = −3600 cents (très sombre), 100 = +3600 (brillant).
inline int filterSliderToCents(int sliderVal) {
    return (sliderVal - 50) * 72; // −3600..+3600 cents
}

} // namespace sfed
