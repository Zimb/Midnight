#pragma once
// =============================================================================
// algo.h – Shared algorithmic core for Midnight Melody Maker
//
// No DAW-specific headers (CLAP, VST3, etc.) are included here.
// Both plugin.cpp (CLAP) and plugin_vst3.cpp (VST3) use these definitions.
// =============================================================================

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace mm {

// -----------------------------------------------------------------------------
// Music theory
// -----------------------------------------------------------------------------

// Semitone offsets from root for the supported modes/scales.
// IMPORTANT: indices 0..3 must stay (Major, Minor, Dorian, Mixolydian) for
// backwards compatibility with previously saved presets/states.
// NOTE: all rows are padded to 12 entries. kScaleLengths[i] gives the
// actual number of notes used; padded cells (beyond length) repeat the
// last interval so accidental out-of-bounds access stays in-octave.
inline constexpr int kScaleIntervals[27][12] = {
    {0, 2, 4, 5, 7, 9,11,11,11,11,11,11}, // 0  Ionian (Major)            7
    {0, 2, 3, 5, 7, 8,10,10,10,10,10,10}, // 1  Aeolian (Natural minor)   7
    {0, 2, 3, 5, 7, 9,10,10,10,10,10,10}, // 2  Dorian                    7
    {0, 2, 4, 5, 7, 9,10,10,10,10,10,10}, // 3  Mixolydian                7
    {0, 1, 3, 5, 7, 8,10,10,10,10,10,10}, // 4  Phrygian                  7
    {0, 2, 4, 6, 7, 9,11,11,11,11,11,11}, // 5  Lydian                    7
    {0, 1, 3, 5, 6, 8,10,10,10,10,10,10}, // 6  Locrian                   7
    {0, 2, 3, 5, 7, 8,11,11,11,11,11,11}, // 7  Harmonic minor             7
    {0, 2, 3, 5, 7, 9,11,11,11,11,11,11}, // 8  Melodic minor (asc.)      7
    {0, 1, 4, 5, 7, 8,10,10,10,10,10,10}, // 9  Phrygian dominant          7
    {0, 2, 3, 6, 7, 8,11,11,11,11,11,11}, // 10 Hungarian minor            7
    {0, 1, 4, 5, 7, 8,11,11,11,11,11,11}, // 11 Double harmonic            7
    {0, 2, 4, 6, 7, 9,10,10,10,10,10,10}, // 12 Lydian dominant            7
    {0, 2, 4, 6, 8, 9,11,11,11,11,11,11}, // 13 Lydian augmented           7
    {0, 1, 3, 4, 6, 8,10,10,10,10,10,10}, // 14 Super Locrian (altered)    7
    {0, 3, 4, 6, 7, 9,10,10,10,10,10,10}, // 15 Hungarian major            7
    {0, 1, 3, 5, 7, 8,11,11,11,11,11,11}, // 16 Neapolitan minor           7
    {0, 1, 3, 5, 7, 9,11,11,11,11,11,11}, // 17 Neapolitan major           7
    {0, 1, 4, 6, 8,10,11,11,11,11,11,11}, // 18 Enigmatic                  7
    {0, 3, 5, 6, 7, 9,10,10,10,10,10,10}, // 19 Blues mineur hepta         7
    {0, 2, 3, 4, 7, 9,10,10,10,10,10,10}, // 20 Blues majeur hepta         7
    {0, 3, 5, 7,10,10,10,10,10,10,10,10}, // 21 Penta mineure              5
    {0, 2, 4, 7, 9, 9, 9, 9, 9, 9, 9, 9}, // 22 Penta majeure              5
    {0, 2, 3, 7, 8, 8, 8, 8, 8, 8, 8, 8}, // 23 Hirajoshi (japonaise)      5
    {0, 2, 4, 6, 8,10,10,10,10,10,10,10}, // 24 Gamme par tons             6
    {0, 1, 3, 4, 6, 7, 9,10,10,10,10,10}, // 25 Diminuee octatonique       8
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11}, // 26 Chromatique               12
};
inline constexpr int kScaleCount = 27;
// Actual note count per scale (determines modulo in choose_pitch).
inline constexpr int kScaleLengths[kScaleCount] = {
    7, 7, 7, 7, 7, 7, 7,   // 0-6
    7, 7, 7, 7, 7, 7, 7,   // 7-13
    7, 7, 7, 7, 7,          // 14-18
    7, 7,                   // 19-20 blues hepta
    5, 5, 5,                // 21-23 pentatoniques
    6,                      // 24 whole-tone
    8,                      // 25 diminuee octatonique
    12,                     // 26 chromatique
};

inline constexpr const char* kModeNames[kScaleCount] = {
    "Ionian (Major)",
    "Aeolian (minor)",
    "Dorian",
    "Mixolydian",
    "Phrygian",
    "Lydian",
    "Locrian",
    "Harmonic minor",
    "Melodic minor",
    "Phrygian dominant",
    "Hungarian minor",
    "Double harmonic",
    "Lydian dominant",
    "Lydian augmented",
    "Super Locrian (altered)",
    "Hungarian major",
    "Neapolitan minor",
    "Neapolitan major",
    "Enigmatic",
    "Blues mineur",
    "Blues majeur",
    "Penta mineure",
    "Penta majeure",
    "Hirajoshi (japonaise)",
    "Gamme par tons",
    "Diminuee octatonique",
    "Chromatique",
};

inline constexpr const char* kKeyNames[12] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

// -----------------------------------------------------------------------------
// PRNG
// -----------------------------------------------------------------------------

inline uint32_t xorshift32(uint32_t& s) {
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
}

// -----------------------------------------------------------------------------
// Parameters
// -----------------------------------------------------------------------------

enum ParamId : uint32_t {
    kParamKey = 0,
    kParamMode,
    kParamOctave,
    kParamSubdiv,
    kParamDensity,
    kParamSeed,
    kParamNoteLen,
    kParamCount,
};

struct ParamDef {
    const char* name;
    double min_value;
    double max_value;
    double default_value;
    bool   is_stepped;
};

inline constexpr ParamDef kParamDefs[kParamCount] = {
    {"Key",     0.0,  11.0,   0.0,  true},
    {"Mode",    0.0,  26.0,   0.0,  true},
    {"Octave",  3.0,   6.0,   4.0,  true},
    {"Subdiv",  1.0,  32.0,   4.0,  true},
    {"Density", 0.0,   1.0,   0.8,  false},
    {"Seed",    0.0, 999.0,   7.0,  true},
    {"NoteLen", 0.05,  1.0,  0.85,  false},
};

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------

// Round subdivision parameter to the closest supported value (1, 2, 4, 8, 16, 32).
inline int normalized_subdiv(double v) {
    static constexpr int allowed[] = {1, 2, 4, 8, 16, 32};
    int    best   = 4;
    double best_d = 1e9;
    for (int a : allowed) {
        double d = std::fabs(v - a);
        if (d < best_d) { best_d = d; best = a; }
    }
    return best;
}

// Diatonic chord progressions: 12 chords per cycle (one per bar -> 12-bar form).
// 0=I, 1=ii, 2=iii, 3=IV, 4=V, 5=vi, 6=vii.
// Built as A-A'-B 12-bar phrases: bars 1..4 = identity (matches the original
// 4-chord version, for backwards-compat *feel*); bars 5..8 = variation /
// passing harmony; bars 9..12 = turnaround / cadence back to bar 1.
// IMPORTANT: indices 0..7 must stay in the same order for preset compat.
inline constexpr int kProgressionLength = 12;
static constexpr int kProgressions[27][kProgressionLength] = {
    // 0  Pop / Cinéma            I-V-vi-IV  | I-V-iii-IV  | I-V-vi-V
    {0,4,5,3,  0,4,2,3,  0,4,5,4},
    // 1  Cadence classique       I-IV-V-I   | I-vi-ii-V   | I-IV-V-I
    {0,3,4,0,  0,5,1,4,  0,3,4,0},
    // 2  Pachelbel (canon entier) I-V-vi-iii | IV-I-IV-V   | I-V-vi-iii
    {0,4,5,2,  3,0,3,4,  0,4,5,2},
    // 3  50s / Ballade           I-vi-IV-V  | I-vi-ii-V   | I-vi-IV-V
    {0,5,3,4,  0,5,1,4,  0,5,3,4},
    // 4  Andalouse / Celtique    vi-V-IV-V  | vi-V-IV-iii | vi-V-IV-V (descente phrygienne)
    {5,4,3,4,  5,4,3,2,  5,4,3,4},
    // 5  Jazz ii-V-I             ii-V-I-I   | ii-V-I-vi   | ii-V-I-I  (turnaround bebop)
    {1,4,0,0,  1,4,0,5,  1,4,0,0},
    // 6  Romantique / Sad        vi-IV-I-V  | iii-IV-I-V  | vi-IV-I-V (sub. relative)
    {5,3,0,4,  2,3,0,4,  5,3,0,4},
    // 7  Drone (statique)        I x 12
    {0,0,0,0,  0,0,0,0,  0,0,0,0},
    // 8  Royal Road / J-pop      IV-V-iii-vi| IV-V-iii-vi | IV-V-I-I   (résolution)
    {3,4,2,5,  3,4,2,5,  3,4,0,0},
    // 9  Lament bass / Passacaglia (descente diatonique complète puis cadence)
    //                            i-VII-VI-V | iv-III-ii-i | iv-V-i-i
    {0,6,5,4,  3,2,1,0,  3,4,0,0},
    // 10 Autumn Leaves           vi-ii-V-I  | IV-iii-vi-vi| ii-V-i-i   (pivot mineur)
    {5,1,4,0,  3,2,5,5,  1,4,5,5},
    // 11 Épique / Aeolian        i-VI-III-VII| iv-VI-III-VII| i-VI-VII-i
    {0,5,2,6,  3,5,2,6,  0,5,6,0},
    // 12 Doo-wop alt             I-iii-IV-V | I-vi-IV-V   | I-iii-IV-V
    {0,2,3,4,  0,5,3,4,  0,2,3,4},
    // 13 Alt-pop moderne         I-IV-vi-V  | I-IV-iii-V  | I-IV-vi-V
    {0,3,5,4,  0,3,2,4,  0,3,5,4},
    // 14 Jazz Anatole / rhythm   ii-V-I-vi  | ii-V-I-vi   | iii-vi-ii-V (cycle des quintes)
    {1,4,0,5,  1,4,0,5,  2,5,1,4},
    // 15 Modal rock (Mixolydien) I-bVII-IV-I| bVII-IV-I-I | bVI-bVII-I-I
    {0,6,3,0,  6,3,0,0,  5,6,0,0},
    // 16 Cercle mineur baroque   i-iv-VII-III| VI-ii°-V-i  | i-iv-V-i
    {0,3,6,2,  5,1,4,0,  0,3,4,0},
    // 17 Power / Hymne épique    i-VI-VII-i | iv-VI-VII-i | i-VI-VII-i
    {0,5,6,0,  3,5,6,0,  0,5,6,0},
    // 18 Jazz étendu             iii-vi-ii-V| iii-vi-ii-V | I-vi-ii-V  (résolution sur I)
    {2,5,1,4,  2,5,1,4,  0,5,1,4},
    // 19 Question / Réponse      I-V-vi-V   | I-V-IV-V    | I-V-vi-V
    {0,4,5,4,  0,4,3,4,  0,4,5,4},
    // 20 Blues classique 12 bars   I-I-I-I | IV-IV-I-I | V-IV-I-V
    {0,0,0,0,  3,3,0,0,  4,3,0,4},
    // 21 Blues quick change        I-IV-I-I | IV-IV-I-I | V-IV-I-V
    {0,3,0,0,  3,3,0,0,  4,3,0,4},
    // 22 Blues mineur 12 bars      i-i-i-i | iv-iv-i-i | V-iv-i-i
    {0,0,0,0,  3,3,0,0,  4,3,0,0},
    // 23 Pentatonique rock          I-I-IV-I | I-I-V-IV | I-I-V-I
    //    (valeurs 0,2,3 = deg 0/4th/5th en penta; en diatonique = I/IV/V aussi)
    {0,0,2,0,  0,0,3,2,  0,0,3,0},
    // 24 Pentatonique blues riff    I-I-I-I | IV-IV-I-I | V-IV-I-I
    //    Calque blues classique mais avec les 5 degres penta (pas de 6e/7e)
    {0,0,0,0,  2,2,0,0,  3,2,0,0},
    // 25 Whole-tone / Triton       0-3 oscillation (= triton dans gamme par tons)
    //    Debussy, ambiguite tonale; 3 = mi-chemin sur les 6 notes
    {0,3,0,3,  3,0,3,0,  0,3,0,0},
    // 26 Diminuee symmetrique      0-2-4-6 cycle de tierce mineure
    //    Parfait avec la diminuee octatonique (8 notes); en diatonic = I-iii-V-vii
    {0,2,4,6,  0,2,4,6,  0,2,4,6},
};
static constexpr int kProgressionCount = 27;
static constexpr const char* kProgressionNames[kProgressionCount] = {
    "Pop / Cinema (I-V-vi-IV)",
    "Cadence classique (I-IV-V-I)",
    "Pachelbel (I-V-vi-iii)",
    "50s / Ballade (I-vi-IV-V)",
    "Andalouse / Celtique (vi-V-IV-V)",
    "Jazz ii-V-I",
    "Romantique (vi-IV-I-V)",
    "Drone (I statique)",
    "Royal Road / J-pop (IV-V-iii-vi)",
    "Lament bass (i-VII-VI-V)",
    "Autumn Leaves (vi-ii-V-I)",
    "Epique / Aeolian (i-VI-III-VII)",
    "Doo-wop alt (I-iii-IV-V)",
    "Alt-pop moderne (I-IV-vi-V)",
    "Jazz turnaround (ii-V-I-vi)",
    "Modal rock (I-bVII-IV-I)",
    "Cercle mineur (i-iv-VII-III)",
    "Power / Hymne (i-VI-VII-i)",
    "Jazz etendu (iii-vi-ii-V)",
    "Question/Reponse (I-V-vi-V)",
    "Blues classique 12 bars",
    "Blues quick change 12 bars",
    "Blues mineur 12 bars",
    "Pentatonique rock (I-IV-V)",
    "Pentatonique blues riff",
    "Whole-tone / Triton (0-3)",
    "Diminuee symetrique (0-2-4-6)",
};

// Choose the MIDI pitch for a given subdivision slot.
// params[] is indexed by ParamId (size >= kParamCount).
// progression: index into kProgressions[].
//
// Musical strategy (v3 — seed-driven personality, voice-leading aware):
//   Each seed produces a distinct "character" via these independent axes:
//
//   1. PHRASE SHAPE  — rising / falling / arch / valley / wave (octShape, 5 types).
//   2. MOMENTUM      — tendency to keep moving in the same direction for N steps
//                       before reversing (momentumLen 1..8). Low = zigzag, high = runs.
//   3. CONTOUR BIAS  — which chord tone anchors strong beats (root / 3rd / 5th).
//   4. MOTION BIAS   — ratio of conjunct steps vs. chord-tone leaps (0..100).
//   5. REGISTER BIAS — favours low / mid / high octave shifts within the phrase.
//   6. SYNCO FACTOR  — probability to anticipate the next downbeat by 1 slot.
//   7. BOOT DEG      — which scale degree starts the melody.
//
//   Pitch selection:
//   - Strong beats (1, 3) → chord tone closest to previous pitch.
//   - Other downbeats     → step motion with momentum bias.
//   - Off-beats           → step (momentum) or nearest chord tone, per motionBias.
//   - Syncopation         → off-beat notes can anticipate the next chord tone.
//   - Avoid immediate repetition: nudge by 1 step, direction from momentum.
namespace detail {

// Lightweight, deterministic *raw* scale-degree picker that only knows the
// chord and the slot — used to bootstrap the recursion for voice-leading.
inline int raw_degree(int chord_root, int slot_in_bar, int subdiv,
                      uint32_t& s, int len)
{
    int beat_in_bar = slot_in_bar / subdiv;
    bool is_downbeat = (slot_in_bar % subdiv) == 0;
    bool is_strong   = is_downbeat && (beat_in_bar % 2 == 0);
    int chord_tones[3] = {
        (chord_root + 0) % len,
        (chord_root + 2) % len,
        (chord_root + 4) % len,
    };
    if (is_strong)         return chord_tones[xorshift32(s) % 3];
    if (is_downbeat) {
        if (xorshift32(s) % 4 == 0) {
            int base = chord_tones[xorshift32(s) % 3];
            int step = (xorshift32(s) & 1) ? 1 : -1;
            return (base + step + len) % len;
        }
        return chord_tones[xorshift32(s) % 3];
    }
    if (xorshift32(s) % 4 < 3) return chord_tones[xorshift32(s) % 3];
    return static_cast<int>(xorshift32(s) % len);
}

} // namespace detail

inline int16_t choose_pitch(const double* params, int64_t slot, int progression = 0) {
    int key  = static_cast<int>(std::round(params[kParamKey])) % 12;
    int mode = std::clamp(static_cast<int>(std::round(params[kParamMode])), 0, kScaleCount - 1);
    int len  = kScaleLengths[mode];  // actual note count for this scale
    int oct  = std::clamp(static_cast<int>(std::round(params[kParamOctave])), 0, 9);
    int seed = static_cast<int>(std::round(params[kParamSeed]));

    int subdiv = normalized_subdiv(params[kParamSubdiv]);
    int slotsPerBar = subdiv * 4;
    int prog        = std::clamp(progression, 0, kProgressionCount - 1);

    auto chord_at = [&](int64_t sl) {
        int64_t b = sl / slotsPerBar;
        return kProgressions[prog][((b % kProgressionLength) + kProgressionLength) % kProgressionLength];
    };
    auto slot_in_bar_of = [&](int64_t sl) {
        return static_cast<int>(((sl % slotsPerBar) + slotsPerBar) % slotsPerBar);
    };
    auto rng_at = [&](int64_t sl) {
        // SplitMix64-style mix so seed and slot interact strongly: any
        // change to the seed reshuffles the entire melody, not just a
        // small offset. Two close seeds give two genuinely different
        // sequences (instead of the previous "same melody +/- 1 note").
        uint64_t z = static_cast<uint64_t>(sl) * 0x9E3779B97F4A7C15ULL
                   ^ static_cast<uint64_t>(static_cast<uint32_t>(seed))
                     * 0xBF58476D1CE4E5B9ULL
                   ^ 0xD1B54A32D192ED03ULL;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        z =  z ^ (z >> 31);
        uint32_t s = static_cast<uint32_t>(z) ^ static_cast<uint32_t>(z >> 32);
        if (s == 0) s = 1;
        return s;
    };

    // Seed-driven "personality" — 7 independent musical axes.
    // Each axis is derived from a different hash of the seed so they are
    // uncorrelated: changing seed by 1 reshuffles all axes simultaneously.
    uint32_t pers = static_cast<uint32_t>(static_cast<uint32_t>(seed) * 2654435761u
                                          ^ 0xDEADBEEFu);
    if (pers == 0) pers = 1;
    xorshift32(pers);
    int  contourBias  = static_cast<int>(pers % 3);           // 0,1,2 — anchor root/3rd/5th
    xorshift32(pers);
    int  motionBias   = static_cast<int>(pers % 100);         // 0..99 step vs chord-tone
    xorshift32(pers);
    int  octShape     = static_cast<int>(pers % 7);           // 0..6 phrase shape (extended)
    xorshift32(pers);
    int  bootDeg      = static_cast<int>(pers % (uint32_t)len); // start degree
    xorshift32(pers);
    int  momentumLen  = 1 + static_cast<int>(pers % 7);       // 1..7 direction persistence
    xorshift32(pers);
    int  regBias      = static_cast<int>(pers % 3);           // 0=low 1=mid 2=high register
    xorshift32(pers);
    int  syncoFactor  = static_cast<int>(pers % 30);          // 0..29% syncopation chance

    // ---- Previous degree + momentum direction ------------------------------
    // We compute the last 2 degrees to derive current momentum direction.
    int prev_deg;
    int momentum_dir = 0; // -1=falling, 0=neutral, +1=rising
    if (slot == 0) {
        prev_deg = bootDeg;
    } else {
        uint32_t sp = rng_at(slot - 1);
        prev_deg = detail::raw_degree(chord_at(slot - 1),
                                      slot_in_bar_of(slot - 1),
                                      subdiv, sp, len);
        if (slot >= 2) {
            uint32_t sp2 = rng_at(slot - 2);
            int prev2 = detail::raw_degree(chord_at(slot - 2),
                                           slot_in_bar_of(slot - 2),
                                           subdiv, sp2, len);
            // Signed distance on the ring (prefer short path).
            int diff = prev_deg - prev2;
            if (diff > len / 2)  diff -= len;
            if (diff < -len / 2) diff += len;
            if (diff > 0)       momentum_dir = +1;
            else if (diff < 0)  momentum_dir = -1;
        }
    }
    // Momentum persistence: once established, continue in same direction
    // for momentumLen steps before random reversal kicks in.
    // We approximate it: slot % momentumLen tells us if we are "mid-run".
    bool mid_run = (momentumLen > 1) && ((slot % momentumLen) != 0);
    int preferred_step = (momentum_dir != 0 && mid_run) ? momentum_dir : 0;

    // ---- Current slot ------------------------------------------------------
    int chord_root  = chord_at(slot);
    int slot_in_bar = slot_in_bar_of(slot);
    int beat_in_bar = slot_in_bar / subdiv;
    bool is_downbeat = (slot_in_bar % subdiv) == 0;
    bool is_strong   = is_downbeat && (beat_in_bar % 2 == 0);

    // Rotate the chord-tone preference (root/3/5) per seed so different
    // seeds emphasise different "anchor" notes on strong beats.
    int chord_tones[3] = {
        (chord_root + (0 + contourBias * 2) % 6) % len,
        (chord_root + (2 + contourBias * 2) % 6) % len,
        (chord_root + (4 + contourBias * 2) % 6) % len,
    };

    uint32_t s = rng_at(slot);

    // Pick degree biased toward small motion from prev_deg.
    auto nearest_chord_tone = [&](int from) {
        int best = chord_tones[0];
        int best_d = 99;
        for (int ct : chord_tones) {
            // Wrap-around scale distance on a `len`-note ring.
            int d = std::abs(((ct - from + len) % len));
            if (d > len / 2) d = len - d;
            if (d < best_d) { best_d = d; best = ct; }
        }
        return best;
    };

    // Step/chord-tone probability shifts with the seed.
    int stepProbMid = std::clamp(70 + (motionBias - 50) / 3, 40, 90);
    int stepProbOff = std::clamp(60 + (motionBias - 50) / 3, 30, 85);

    // Syncopation: on an off-beat slot, occasionally anticipate the NEXT
    // downbeat's chord tone (makes some seeds sound ahead-of-the-beat).
    bool is_synco = (!is_downbeat) && ((int)(xorshift32(s) % 100) < syncoFactor);
    int synco_root = chord_at(slot + 1);  // next bar's chord
    int synco_ct   = (synco_root + (contourBias * 2)) % len;

    int idx;
    if (is_strong) {
        // Strong beat: anchor to nearest chord tone.
        idx = nearest_chord_tone(prev_deg);
    } else if (is_synco) {
        // Syncopated anticipation: jump to next chord's tone.
        idx = synco_ct;
    } else if (is_downbeat) {
        // Weak downbeat: step with momentum bias.
        if ((int)(xorshift32(s) % 100) < stepProbMid) {
            int step = (preferred_step != 0) ? preferred_step
                       : ((xorshift32(s) & 1) ? 1 : -1);
            idx = (prev_deg + step + len) % len;
        } else {
            idx = nearest_chord_tone(prev_deg);
        }
    } else {
        // Off-beat: step (with momentum) vs chord tone.
        if ((int)(xorshift32(s) % 100) < stepProbOff) {
            int step = (preferred_step != 0) ? preferred_step
                       : ((xorshift32(s) & 1) ? 1 : -1);
            idx = (prev_deg + step + len) % len;
        } else {
            idx = nearest_chord_tone(prev_deg);
        }
    }

    // Avoid immediate repetition: nudge in momentum direction (or random).
    if (idx == prev_deg) {
        int step = (preferred_step != 0) ? preferred_step
                   : ((xorshift32(s) & 1) ? 1 : -1);
        idx = (idx + step + len) % len;
    }

    // Octave: base register shifted by regBias (low/mid/high personality).
    int oct_reg = oct + (regBias - 1);  // -1=low, 0=mid, +1=high
    int oct_off = 0;
    bool bar_start = (slot_in_bar == 0);
    if (bar_start && (xorshift32(s) % 32) == 0) {
        oct_off = (xorshift32(s) & 1) ? 1 : -1;
    }
    // Seed-driven phrase shape — 7 types (extended from 5).
    int64_t bar = slot / slotsPerBar;
    int barInPhrase = (int)(((bar % 4) + 4) % 4);
    switch (octShape) {
    case 0: break;                                                // flat
    case 1: oct_off += (barInPhrase == 2) ? 1 : 0; break;        // bump mid
    case 2: oct_off += (barInPhrase >= 2) ? 1 : 0; break;        // ascending
    case 3: oct_off += (barInPhrase == 1 || barInPhrase == 2) ? 1 : 0; break; // arch
    case 4: oct_off += (barInPhrase == 3) ? -1 : 0; break;       // dip end
    case 5: oct_off += (barInPhrase == 0 || barInPhrase == 2) ? 1 : 0; break; // wave
    case 6: oct_off += (barInPhrase <= 1) ? 1 : -1; break;       // descending
    default: break;
    }

    int semitone = kScaleIntervals[mode][idx] + key + (oct_reg + oct_off) * 12;
    return static_cast<int16_t>(std::clamp(semitone, 0, 127));
}

// -----------------------------------------------------------------------------
// Chord detection from a set of held MIDI pitches.
//
// Given up to 8 pitch values (MIDI note numbers), identifies the most likely
// chord quality and returns:
//   outRoot  — root note (0-11, pitch class of the lowest note)
//   outMode  — best matching scale index (from kModeNames)
//   outProg  — suggested progression index (from kProgressions)
//
// Chord vocabulary recognised (by interval set):
//   Major triad → Ionian + Pop
//   Minor triad → Aeolian + Romantique
//   Dominant 7  → Mixolydian + Jazz ii-V-I
//   Minor 7     → Dorian + Autumn Leaves
//   Major 7     → Lydian + Jazz étendu
//   Diminished  → Locrian + Diminuée symétrique
//   Half-dim 7  → Locrian + Cercle mineur
//   Augmented   → Lydian augmented + Whole-tone
//   Sus2        → Ionian + Pentatonique rock
//   Sus4        → Mixolydian + Modal rock
//   Minor(b6)   → Harmonic minor + Lament bass
//   Phrygian    → Phrygian dominant + Andalouse
// -----------------------------------------------------------------------------
struct ChordContext {
    int root;       // 0-11
    int mode;       // index into kModeNames
    int prog;       // index into kProgressions
};

inline ChordContext detect_chord(const int* pitches, int count) {
    if (count <= 0) return {0, 0, 0};
    // Collect pitch classes, find lowest note as root candidate.
    bool pc[12] = {};
    int lowest = pitches[0];
    for (int i = 0; i < count; ++i) {
        if (pitches[i] < lowest) lowest = pitches[i];
        pc[pitches[i] % 12] = true;
    }
    int root = lowest % 12;

    // Compute interval set relative to root.
    bool iv[12] = {};
    for (int i = 0; i < 12; ++i)
        if (pc[(root + i) % 12]) iv[i] = true;

    // Distinguish chord quality by key intervals present.
    bool has_M3  = iv[4];   // major third
    bool has_m3  = iv[3];   // minor third
    bool has_5   = iv[7];   // perfect fifth
    bool has_b5  = iv[6];   // tritone / diminished fifth
    bool has_a5  = iv[8];   // augmented fifth
    bool has_m7  = iv[10];  // minor seventh
    bool has_M7  = iv[11];  // major seventh
    bool has_M2  = iv[2];   // major second (sus2)
    bool has_P4  = iv[5];   // perfect fourth (sus4)
    bool has_m6  = iv[8];   // minor sixth (same bit as aug5)

    // --- Dominant 7 (M3 + 5 + m7): Mixolydian, Jazz ii-V-I
    if (has_M3 && has_5 && has_m7 && !has_M7)
        return {root, 3 /* Mixolydian */, 5 /* Jazz ii-V-I */};
    // --- Major 7 (M3 + 5 + M7): Lydian, Jazz étendu
    if (has_M3 && has_5 && has_M7)
        return {root, 5 /* Lydian */, 18 /* Jazz étendu */};
    // --- Minor 7 (m3 + 5 + m7): Dorian, Autumn Leaves
    if (has_m3 && has_5 && has_m7 && !has_M3)
        return {root, 2 /* Dorian */, 10 /* Autumn Leaves */};
    // --- Half-diminished (m3 + b5 + m7): Locrian, Cercle mineur
    if (has_m3 && has_b5 && has_m7 && !has_5)
        return {root, 6 /* Locrian */, 16 /* Cercle mineur */};
    // --- Diminished triad (m3 + b5): Locrian, Diminuée symétrique
    if (has_m3 && has_b5 && !has_5 && !has_m7)
        return {root, 6 /* Locrian */, 26 /* Dim symétrique */};
    // --- Augmented (M3 + a5): Lydian augmented, Whole-tone
    if (has_M3 && has_a5 && !has_5)
        return {root, 13 /* Lydian augmented */, 25 /* Whole-tone */};
    // --- Sus2 (M2 + 5, no 3rd): Ionian, Pentatonique rock
    if (has_M2 && has_5 && !has_M3 && !has_m3)
        return {root, 0 /* Ionian */, 23 /* Penta rock */};
    // --- Sus4 (P4 + 5, no 3rd): Mixolydian, Modal rock
    if (has_P4 && has_5 && !has_M3 && !has_m3)
        return {root, 3 /* Mixolydian */, 15 /* Modal rock */};
    // --- Minor with b6 (m3 + 5 + m6): Harmonic minor, Lament bass
    if (has_m3 && has_5 && has_m6 && !has_m7)
        return {root, 7 /* Harmonic minor */, 9 /* Lament bass */};
    // --- Phrygian flavour (m2 + m3): Phrygian dominant, Andalouse
    if (iv[1] && has_m3)
        return {root, 9 /* Phrygian dominant */, 4 /* Andalouse */};
    // --- Major triad: Ionian, Pop
    if (has_M3 && has_5)
        return {root, 0 /* Ionian */, 0 /* Pop */};
    // --- Minor triad: Aeolian, Romantique
    if (has_m3 && has_5)
        return {root, 1 /* Aeolian */, 6 /* Romantique */};
    // --- Power chord (root + 5 only): Mixolydian, Modal rock
    if (has_5 && !has_M3 && !has_m3)
        return {root, 3 /* Mixolydian */, 15 /* Modal rock */};
    // --- Fallback: single note or unrecognized → use root as Ionian
    return {root, 0, 0};
}

// -----------------------------------------------------------------------------
// Mode / Progression compatibility table.
//
// Returns 0 = neutral (white), 1 = recommended (green), 2 = discouraged (red)
// for a given (mode index, progression index) pair.
//
// Progression indices (kProgressionNames order):
//  0=Pop/Cinema  1=Cadence cl.  2=Pachelbel  3=50s/Ballade  4=Andalouse
//  5=Jazz ii-V-I 6=Romantique   7=Drone      8=Royal Road   9=Lament bass
// 10=Autumn Lvs 11=Epique/Aeo  12=Doo-wop   13=Alt-pop    14=Jazz turn.
// 15=Modal rock 16=Cercle min  17=Power/Hym 18=Jazz étendu 19=Q/Réponse
// 20=Blues cl.  21=Blues QC    22=Blues min  23=Penta rock 24=Penta blues
// 25=Whole-tone 26=Dim symét.
// -----------------------------------------------------------------------------
inline int mode_prog_compat(int mode, int prog) {
    if (mode < 0 || mode >= kScaleCount ||
        prog < 0 || prog >= kProgressionCount) return 0;
    // Values: 0=neutral(white) 1=recommended(green) 2=discouraged(red)
    // Rows = mode 0..26, columns = progression 0..26
    static constexpr uint8_t kCompat[27][27] = {
        // 0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26
        {1, 1, 1, 1, 2, 1, 0, 1, 1, 2, 0, 2, 1, 1, 1, 0, 2, 2, 1, 1, 0, 0, 2, 1, 0, 2, 2}, // 0  Ionian
        {2, 2, 2, 2, 1, 0, 1, 0, 2, 1, 0, 1, 2, 2, 0, 1, 1, 1, 0, 2, 0, 0, 1, 0, 0, 2, 2}, // 1  Aeolien
        {2, 2, 2, 2, 2, 1, 0, 0, 2, 0, 1, 0, 2, 2, 1, 1, 0, 2, 1, 2, 1, 1, 0, 0, 0, 2, 2}, // 2  Dorien
        {1, 2, 0, 0, 2, 1, 2, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 2, 1, 0, 1, 1, 0, 1, 1, 0, 2}, // 3  Mixolydien
        {2, 2, 2, 2, 1, 2, 0, 0, 2, 1, 2, 0, 2, 2, 2, 0, 0, 0, 2, 2, 2, 2, 0, 2, 2, 2, 0}, // 4  Phrygien
        {1, 1, 1, 2, 2, 0, 2, 1, 1, 2, 0, 2, 1, 1, 0, 0, 2, 2, 1, 1, 2, 2, 2, 0, 0, 2, 2}, // 5  Lydien
        {2, 2, 2, 2, 0, 0, 0, 0, 2, 0, 0, 0, 2, 2, 0, 2, 1, 1, 0, 2, 2, 2, 0, 2, 2, 0, 1}, // 6  Locrien
        {2, 2, 2, 2, 1, 0, 1, 0, 2, 1, 0, 1, 2, 2, 0, 0, 0, 0, 0, 2, 2, 2, 0, 2, 0, 2, 0}, // 7  Harm. mineur
        {2, 2, 2, 2, 0, 1, 0, 0, 2, 0, 1, 0, 2, 2, 1, 0, 0, 2, 1, 2, 0, 1, 0, 2, 0, 0, 0}, // 8  Mélo. mineur
        {2, 2, 2, 2, 1, 2, 2, 0, 2, 1, 2, 0, 2, 2, 2, 0, 0, 0, 2, 2, 2, 2, 0, 2, 2, 2, 0}, // 9  Phry. dominant
        {2, 2, 2, 0, 2, 1, 2, 0, 2, 2, 1, 0, 2, 0, 1, 1, 2, 2, 1, 0, 1, 1, 0, 0, 0, 1, 2}, // 10 Lydien dominant
        {2, 2, 2, 2, 2, 1, 2, 0, 2, 2, 0, 2, 2, 2, 1, 2, 2, 2, 1, 2, 2, 1, 2, 2, 2, 0, 0}, // 11 Super-Locrien
        {2, 2, 2, 2, 0, 1, 0, 0, 2, 0, 1, 0, 2, 2, 1, 0, 0, 2, 1, 2, 0, 1, 0, 2, 0, 0, 0}, // 12 Dorien b2
        {2, 2, 2, 2, 2, 0, 2, 0, 2, 2, 0, 2, 2, 2, 1, 2, 2, 2, 1, 2, 2, 0, 2, 2, 2, 1, 2}, // 13 Lydien augmenté
        {2, 2, 2, 2, 0, 1, 0, 0, 2, 0, 1, 0, 2, 2, 1, 2, 1, 2, 1, 2, 2, 1, 0, 2, 0, 2, 1}, // 14 Locrien #2
        {0, 0, 0, 2, 2, 1, 2, 0, 0, 2, 0, 2, 0, 0, 1, 2, 2, 2, 1, 2, 2, 0, 2, 2, 2, 0, 2}, // 15 Ionien #5
        {2, 2, 2, 2, 1, 2, 0, 0, 2, 1, 2, 0, 2, 2, 2, 0, 0, 0, 2, 2, 2, 2, 0, 2, 2, 2, 0}, // 16 Double harm.
        {2, 2, 2, 2, 1, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 0, 0, 0, 2, 2, 2, 0, 2, 2, 2, 0}, // 17 Napolitain maj.
        {2, 2, 2, 2, 1, 0, 0, 0, 2, 1, 0, 0, 2, 2, 0, 0, 0, 0, 0, 2, 2, 2, 0, 2, 2, 2, 0}, // 18 Napolitain min.
        {0, 0, 0, 0, 0, 2, 0, 1, 0, 0, 2, 0, 0, 0, 2, 0, 0, 0, 2, 0, 0, 2, 0, 1, 1, 2, 2}, // 19 Égyptien
        {2, 2, 2, 0, 2, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 2, 2, 2, 1, 1, 1, 1, 1, 2, 2}, // 20 Blues hept.
        {2, 2, 2, 0, 1, 0, 1, 0, 0, 0, 0, 1, 2, 2, 0, 1, 0, 2, 2, 2, 1, 0, 1, 1, 1, 2, 2}, // 21 Penta mineure
        {1, 1, 1, 2, 2, 0, 2, 1, 1, 2, 0, 2, 1, 1, 0, 0, 2, 2, 2, 1, 0, 0, 2, 1, 1, 2, 2}, // 22 Penta majeure
        {2, 2, 2, 2, 2, 2, 0, 1, 2, 0, 2, 0, 2, 2, 2, 2, 0, 2, 2, 2, 2, 2, 2, 2, 1, 2, 2}, // 23 Hirajoshi
        {2, 2, 2, 2, 2, 0, 2, 1, 2, 2, 0, 2, 2, 2, 1, 2, 2, 2, 1, 2, 2, 0, 2, 2, 2, 1, 2}, // 24 Gamme par tons
        {2, 2, 2, 2, 2, 1, 2, 0, 2, 2, 0, 0, 2, 2, 1, 2, 0, 2, 1, 2, 2, 1, 2, 2, 2, 2, 1}, // 25 Diminuée octa.
        {0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0}, // 26 Chromatique
    };
    return static_cast<int>(kCompat[mode][prog]);
}

// -----------------------------------------------------------------------------
// Krumhansl-Schmuckler key/mode detection
//
// Given a pitch-class weight histogram (size 12), returns the most likely
// (root, mode) where mode is 0=Major or 1=Minor. Other modes (dorian /
// mixolydian) are not detected – they remain a manual override.
//
// `weights[pc]` should be the total duration (or note count) for each pitch
// class observed in the input MIDI buffer. Returns (root=0, mode=0) if the
// histogram is empty.
// -----------------------------------------------------------------------------
inline void detect_key(const double weights[12], int& outRoot, int& outMode) {
    static constexpr double kMajor[12] = {
        6.35, 2.23, 3.48, 2.33, 4.38, 4.09,
        2.52, 5.19, 2.39, 3.66, 2.29, 2.88,
    };
    static constexpr double kMinor[12] = {
        6.33, 2.68, 3.52, 5.38, 2.60, 3.53,
        2.54, 4.75, 3.98, 2.69, 3.34, 3.17,
    };

    double sum = 0.0;
    for (int i = 0; i < 12; ++i) sum += weights[i];
    if (sum < 1e-6) { outRoot = 0; outMode = 0; return; }

    double mean = sum / 12.0;
    auto correlate = [&](const double* prof, int rot) {
        double pmean = 0.0;
        for (int i = 0; i < 12; ++i) pmean += prof[i];
        pmean /= 12.0;
        double num = 0.0, da = 0.0, db = 0.0;
        for (int i = 0; i < 12; ++i) {
            double a = weights[(i + rot) % 12] - mean;
            double b = prof[i] - pmean;
            num += a * b;
            da  += a * a;
            db  += b * b;
        }
        if (da < 1e-9 || db < 1e-9) return -1.0;
        return num / std::sqrt(da * db);
    };

    double bestScore = -2.0;
    int    bestRoot  = 0;
    int    bestMode  = 0;
    for (int r = 0; r < 12; ++r) {
        double sM = correlate(kMajor, r);
        double sm = correlate(kMinor, r);
        if (sM > bestScore) { bestScore = sM; bestRoot = r; bestMode = 0; }
        if (sm > bestScore) { bestScore = sm; bestRoot = r; bestMode = 1; }
    }
    outRoot = bestRoot;
    outMode = bestMode;
}

// -----------------------------------------------------------------------------
// Drum / percussion rhythm patterns
// -----------------------------------------------------------------------------
// Each pattern is one bar of 16 sixteenth-note steps. For each step we list
// up to four simultaneous drum hits with their velocity (0 = silent).
// GM drum-map pitches:
//   36 Kick   37 RimShot 38 Snare  39 Clap   40 ESnare
//   41 FlTom  42 ClHat   44 PdHat  46 OpHat  47 MidTom  48 HiMidTom
//   49 Crash  50 HiTom   51 Ride   54 Tamb   56 Cowbell
//   60 HiBongo 61 LoBongo 62 MtConga 63 HiConga 64 LoConga
//   70 Maracas 75 Claves  76 HiWood 77 LoWood

struct DrumHit { uint8_t pitch; uint8_t vel; };
struct DrumPattern {
    const char* name;
    DrumHit steps[16][4]; // {0,0} = empty slot
};

inline constexpr DrumPattern kDrumPatterns[] = {
  // 0 — Rock (kick 1&3, snare 2&4, 8th hat, crash on 1, tom fill bar-end)
  {"Rock", {
    /*00*/{{36,112},{42,80},{49,85},{0,0}}, /*01*/{{42,55},{0,0},{0,0},{0,0}},
    /*02*/{{42,70},{0,0},{0,0},{0,0}},      /*03*/{{42,55},{0,0},{0,0},{0,0}},
    /*04*/{{38,105},{42,80},{0,0},{0,0}},   /*05*/{{42,55},{0,0},{0,0},{0,0}},
    /*06*/{{42,70},{0,0},{0,0},{0,0}},      /*07*/{{42,55},{0,0},{0,0},{0,0}},
    /*08*/{{36,110},{42,80},{0,0},{0,0}},   /*09*/{{36,75},{42,55},{0,0},{0,0}},
    /*10*/{{42,70},{0,0},{0,0},{0,0}},      /*11*/{{42,55},{0,0},{0,0},{0,0}},
    /*12*/{{38,105},{42,80},{0,0},{0,0}},   /*13*/{{42,55},{0,0},{0,0},{0,0}},
    /*14*/{{48,82},{46,70},{0,0},{0,0}},    /*15*/{{48,90},{41,88},{0,0},{0,0}},
  }},
  // 1 — Funk (16th hat, ghost snares, syncopated kick, cowbell)
  {"Funk", {
    /*00*/{{36,112},{42,90},{0,0},{0,0}},   /*01*/{{42,52},{38,38},{0,0},{0,0}},
    /*02*/{{42,62},{0,0},{0,0},{0,0}},      /*03*/{{38,42},{42,52},{0,0},{0,0}},
    /*04*/{{38,108},{42,80},{56,65},{0,0}}, /*05*/{{42,52},{38,38},{0,0},{0,0}},
    /*06*/{{36,92},{42,62},{0,0},{0,0}},    /*07*/{{42,52},{0,0},{0,0},{0,0}},
    /*08*/{{42,72},{38,38},{0,0},{0,0}},    /*09*/{{36,102},{42,52},{0,0},{0,0}},
    /*10*/{{42,62},{0,0},{0,0},{0,0}},      /*11*/{{38,42},{42,52},{0,0},{0,0}},
    /*12*/{{38,108},{42,80},{56,65},{0,0}}, /*13*/{{42,52},{38,38},{0,0},{0,0}},
    /*14*/{{36,98},{42,62},{0,0},{0,0}},    /*15*/{{42,52},{0,0},{0,0},{0,0}},
  }},
  // 2 — Bossa Nova (3-2 clave, tamborim, rim, soft kick)
  {"Bossa Nova", {
    /*00*/{{36,88},{75,95},{42,65},{0,0}},  /*01*/{{54,55},{0,0},{0,0},{0,0}},
    /*02*/{{42,50},{37,60},{0,0},{0,0}},    /*03*/{{75,85},{54,55},{0,0},{0,0}},
    /*04*/{{36,72},{42,55},{0,0},{0,0}},    /*05*/{{54,55},{0,0},{0,0},{0,0}},
    /*06*/{{42,50},{75,85},{0,0},{0,0}},    /*07*/{{36,78},{54,55},{0,0},{0,0}},
    /*08*/{{42,55},{37,60},{0,0},{0,0}},    /*09*/{{54,55},{0,0},{0,0},{0,0}},
    /*10*/{{36,82},{42,55},{75,90},{0,0}},  /*11*/{{54,55},{0,0},{0,0},{0,0}},
    /*12*/{{42,50},{37,60},{0,0},{0,0}},    /*13*/{{75,85},{54,55},{0,0},{0,0}},
    /*14*/{{36,72},{42,55},{0,0},{0,0}},    /*15*/{{54,55},{0,0},{0,0},{0,0}},
  }},
  // 3 — Reggae One Drop (kick+snare beat 3, ride + rim off-beats, tamb)
  {"Reggae", {
    /*00*/{{0,0},{0,0},{0,0},{0,0}},        /*01*/{{51,65},{37,55},{0,0},{0,0}},
    /*02*/{{0,0},{0,0},{0,0},{0,0}},        /*03*/{{51,65},{0,0},{0,0},{0,0}},
    /*04*/{{0,0},{0,0},{0,0},{0,0}},        /*05*/{{51,65},{37,55},{0,0},{0,0}},
    /*06*/{{0,0},{0,0},{0,0},{0,0}},        /*07*/{{51,65},{0,0},{0,0},{0,0}},
    /*08*/{{36,112},{38,102},{54,70},{0,0}},/*09*/{{51,65},{37,55},{0,0},{0,0}},
    /*10*/{{0,0},{0,0},{0,0},{0,0}},        /*11*/{{51,65},{0,0},{0,0},{0,0}},
    /*12*/{{0,0},{0,0},{0,0},{0,0}},        /*13*/{{51,65},{37,55},{0,0},{0,0}},
    /*14*/{{36,75},{0,0},{0,0},{0,0}},      /*15*/{{51,65},{0,0},{0,0},{0,0}},
  }},
  // 4 — Afro-Cuban (tumbao congas, cascara ride, cowbell, clave, maracas)
  {"Afro-Cuban", {
    /*00*/{{36,92},{63,82},{75,90},{56,70}},/*01*/{{0,0},{0,0},{0,0},{0,0}},
    /*02*/{{63,65},{51,60},{0,0},{0,0}},    /*03*/{{75,82},{0,0},{0,0},{0,0}},
    /*04*/{{64,97},{56,72},{0,0},{0,0}},    /*05*/{{0,0},{0,0},{0,0},{0,0}},
    /*06*/{{63,78},{75,90},{51,60},{0,0}},  /*07*/{{36,88},{0,0},{0,0},{0,0}},
    /*08*/{{63,72},{51,60},{0,0},{0,0}},    /*09*/{{70,65},{0,0},{0,0},{0,0}},
    /*10*/{{36,92},{64,97},{75,90},{56,72}},/*11*/{{70,65},{0,0},{0,0},{0,0}},
    /*12*/{{63,68},{51,60},{0,0},{0,0}},    /*13*/{{0,0},{0,0},{0,0},{0,0}},
    /*14*/{{64,92},{75,90},{56,70},{0,0}},  /*15*/{{63,62},{70,65},{0,0},{0,0}},
  }},
  // 5 — Trap / Hip-Hop (808 kick, open hat bursts, hi-hat variations)
  {"Trap / Hip-Hop", {
    /*00*/{{36,118},{42,82},{0,0},{0,0}},   /*01*/{{42,50},{0,0},{0,0},{0,0}},
    /*02*/{{42,72},{0,0},{0,0},{0,0}},      /*03*/{{46,60},{42,40},{0,0},{0,0}},
    /*04*/{{42,65},{0,0},{0,0},{0,0}},      /*05*/{{46,88},{0,0},{0,0},{0,0}},
    /*06*/{{36,102},{42,80},{0,0},{0,0}},   /*07*/{{42,52},{0,0},{0,0},{0,0}},
    /*08*/{{38,112},{42,78},{0,0},{0,0}},   /*09*/{{42,50},{0,0},{0,0},{0,0}},
    /*10*/{{42,68},{0,0},{0,0},{0,0}},      /*11*/{{36,98},{46,65},{0,0},{0,0}},
    /*12*/{{42,72},{0,0},{0,0},{0,0}},      /*13*/{{46,88},{0,0},{0,0},{0,0}},
    /*14*/{{36,97},{42,82},{0,0},{0,0}},    /*15*/{{42,50},{0,0},{0,0},{0,0}},
  }},
  // 6 — Maqsum arabe (Doum/Tek/Ka, maracas, tamb, bongos)
  {"Maqsum (arabe)", {
    /*00*/{{36,112},{60,90},{54,65},{0,0}}, /*01*/{{70,58},{0,0},{0,0},{0,0}},
    /*02*/{{60,98},{0,0},{0,0},{0,0}},      /*03*/{{70,55},{61,60},{0,0},{0,0}},
    /*04*/{{61,95},{54,60},{0,0},{0,0}},    /*05*/{{70,55},{0,0},{0,0},{0,0}},
    /*06*/{{36,102},{60,72},{0,0},{0,0}},   /*07*/{{70,55},{0,0},{0,0},{0,0}},
    /*08*/{{60,92},{54,65},{0,0},{0,0}},    /*09*/{{70,55},{61,60},{0,0},{0,0}},
    /*10*/{{36,92},{0,0},{0,0},{0,0}},      /*11*/{{70,55},{0,0},{0,0},{0,0}},
    /*12*/{{60,97},{54,60},{0,0},{0,0}},    /*13*/{{70,55},{0,0},{0,0},{0,0}},
    /*14*/{{61,82},{0,0},{0,0},{0,0}},      /*15*/{{70,55},{54,58},{0,0},{0,0}},
  }},
  // 7 — Tribal djembe (bass/mid/slap congas, kick, wood block)
  {"Tribal", {
    /*00*/{{36,112},{63,97},{76,75},{0,0}}, /*01*/{{63,62},{0,0},{0,0},{0,0}},
    /*02*/{{64,82},{76,65},{0,0},{0,0}},    /*03*/{{63,72},{0,0},{0,0},{0,0}},
    /*04*/{{64,97},{36,62},{0,0},{0,0}},    /*05*/{{63,72},{76,70},{0,0},{0,0}},
    /*06*/{{64,82},{0,0},{0,0},{0,0}},      /*07*/{{63,62},{36,72},{0,0},{0,0}},
    /*08*/{{36,108},{64,92},{76,80},{0,0}}, /*09*/{{63,67},{0,0},{0,0},{0,0}},
    /*10*/{{64,82},{0,0},{0,0},{0,0}},      /*11*/{{63,72},{76,65},{0,0},{0,0}},
    /*12*/{{64,102},{36,72},{0,0},{0,0}},   /*13*/{{63,67},{0,0},{0,0},{0,0}},
    /*14*/{{64,78},{76,70},{0,0},{0,0}},    /*15*/{{63,62},{36,62},{0,0},{0,0}},
  }},
  // 8 — Jazz Swing (ride, brushed snare, light kick, crash opener)
  {"Jazz Swing", {
    /*00*/{{51,102},{36,62},{49,80},{0,0}}, /*01*/{{0,0},{0,0},{0,0},{0,0}},
    /*02*/{{0,0},{0,0},{0,0},{0,0}},        /*03*/{{51,72},{0,0},{0,0},{0,0}},
    /*04*/{{51,92},{38,58},{0,0},{0,0}},    /*05*/{{0,0},{0,0},{0,0},{0,0}},
    /*06*/{{0,0},{0,0},{0,0},{0,0}},        /*07*/{{51,72},{36,50},{0,0},{0,0}},
    /*08*/{{51,97},{36,65},{0,0},{0,0}},    /*09*/{{0,0},{0,0},{0,0},{0,0}},
    /*10*/{{0,0},{0,0},{0,0},{0,0}},        /*11*/{{51,72},{0,0},{0,0},{0,0}},
    /*12*/{{51,92},{38,58},{0,0},{0,0}},    /*13*/{{0,0},{0,0},{0,0},{0,0}},
    /*14*/{{0,0},{0,0},{0,0},{0,0}},        /*15*/{{51,72},{36,50},{42,55},{0,0}},
  }},
  // 9 — Lo-Fi Hip-Hop (lazy open hat, ghost snare, tamb on 2&4)
  {"Lo-Fi Hip-Hop", {
    /*00*/{{36,105},{46,72},{0,0},{0,0}},   /*01*/{{46,50},{0,0},{0,0},{0,0}},
    /*02*/{{46,62},{38,35},{0,0},{0,0}},    /*03*/{{46,50},{0,0},{0,0},{0,0}},
    /*04*/{{38,100},{46,72},{54,80},{0,0}}, /*05*/{{46,50},{0,0},{0,0},{0,0}},
    /*06*/{{36,88},{46,62},{0,0},{0,0}},    /*07*/{{46,50},{38,35},{0,0},{0,0}},
    /*08*/{{36,108},{46,72},{0,0},{0,0}},   /*09*/{{46,50},{0,0},{0,0},{0,0}},
    /*10*/{{46,62},{0,0},{0,0},{0,0}},      /*11*/{{36,80},{46,50},{0,0},{0,0}},
    /*12*/{{38,100},{46,72},{54,80},{0,0}}, /*13*/{{46,50},{0,0},{0,0},{0,0}},
    /*14*/{{46,62},{38,35},{0,0},{0,0}},    /*15*/{{46,50},{0,0},{0,0},{0,0}},
  }},
  // 10 — Breakbeat / Amen (syncopated kick/snare, open hats)
  {"Breakbeat", {
    /*00*/{{36,115},{42,85},{0,0},{0,0}},   /*01*/{{42,55},{0,0},{0,0},{0,0}},
    /*02*/{{38,100},{42,75},{0,0},{0,0}},   /*03*/{{42,55},{36,72},{0,0},{0,0}},
    /*04*/{{46,80},{0,0},{0,0},{0,0}},      /*05*/{{38,88},{42,55},{0,0},{0,0}},
    /*06*/{{36,95},{42,70},{0,0},{0,0}},    /*07*/{{42,55},{38,40},{0,0},{0,0}},
    /*08*/{{36,110},{42,80},{0,0},{0,0}},   /*09*/{{46,75},{0,0},{0,0},{0,0}},
    /*10*/{{38,105},{42,60},{0,0},{0,0}},   /*11*/{{42,55},{0,0},{0,0},{0,0}},
    /*12*/{{36,90},{46,78},{0,0},{0,0}},    /*13*/{{38,95},{42,55},{0,0},{0,0}},
    /*14*/{{42,70},{36,80},{0,0},{0,0}},    /*15*/{{38,42},{46,70},{0,0},{0,0}},
  }},
  // 11 — Disco (4-on-floor, open hat off-beats, snare 2&4, tamb)
  {"Disco", {
    /*00*/{{36,112},{42,80},{0,0},{0,0}},   /*01*/{{46,88},{54,75},{0,0},{0,0}},
    /*02*/{{36,90},{42,72},{0,0},{0,0}},    /*03*/{{46,88},{54,75},{0,0},{0,0}},
    /*04*/{{36,95},{38,108},{42,80},{54,85}},/*05*/{{46,88},{0,0},{0,0},{0,0}},
    /*06*/{{36,90},{42,72},{0,0},{0,0}},    /*07*/{{46,88},{54,75},{0,0},{0,0}},
    /*08*/{{36,110},{42,80},{0,0},{0,0}},   /*09*/{{46,88},{54,75},{0,0},{0,0}},
    /*10*/{{36,90},{42,72},{0,0},{0,0}},    /*11*/{{46,88},{0,0},{0,0},{0,0}},
    /*12*/{{36,95},{38,108},{42,80},{54,85}},/*13*/{{46,88},{54,75},{0,0},{0,0}},
    /*14*/{{36,90},{42,72},{0,0},{0,0}},    /*15*/{{46,88},{49,65},{0,0},{0,0}},
  }},
  // 12 — Samba (surdo kick, rimshot, tamborim 16th, claves 3-2, maracas)
  {"Samba", {
    /*00*/{{36,95},{54,80},{75,90},{0,0}},  /*01*/{{54,65},{70,55},{0,0},{0,0}},
    /*02*/{{37,75},{54,80},{0,0},{0,0}},    /*03*/{{54,65},{75,80},{0,0},{0,0}},
    /*04*/{{36,78},{54,80},{0,0},{0,0}},    /*05*/{{54,65},{0,0},{0,0},{0,0}},
    /*06*/{{37,82},{54,80},{75,90},{0,0}},  /*07*/{{54,65},{36,80},{0,0},{0,0}},
    /*08*/{{36,95},{54,80},{0,0},{0,0}},    /*09*/{{54,65},{70,55},{0,0},{0,0}},
    /*10*/{{37,75},{54,80},{75,82},{0,0}},  /*11*/{{54,65},{0,0},{0,0},{0,0}},
    /*12*/{{36,78},{54,80},{0,0},{0,0}},    /*13*/{{54,65},{75,90},{0,0},{0,0}},
    /*14*/{{37,82},{54,80},{0,0},{0,0}},    /*15*/{{54,65},{36,72},{0,0},{0,0}},
  }},
  // 13 — DnB / Jungle (2-step, fast syncopated kick/snare, open hat)
  {"DnB / Jungle", {
    /*00*/{{36,115},{42,82},{0,0},{0,0}},   /*01*/{{42,55},{0,0},{0,0},{0,0}},
    /*02*/{{42,70},{38,42},{0,0},{0,0}},    /*03*/{{42,55},{0,0},{0,0},{0,0}},
    /*04*/{{38,110},{42,80},{0,0},{0,0}},   /*05*/{{42,55},{36,78},{0,0},{0,0}},
    /*06*/{{42,72},{0,0},{0,0},{0,0}},      /*07*/{{36,88},{42,55},{0,0},{0,0}},
    /*08*/{{38,95},{42,80},{0,0},{0,0}},    /*09*/{{42,55},{0,0},{0,0},{0,0}},
    /*10*/{{36,102},{42,70},{0,0},{0,0}},   /*11*/{{42,55},{38,42},{0,0},{0,0}},
    /*12*/{{38,108},{42,80},{0,0},{0,0}},   /*13*/{{36,82},{42,55},{0,0},{0,0}},
    /*14*/{{42,72},{0,0},{0,0},{0,0}},      /*15*/{{36,90},{38,88},{46,75},{0,0}},
  }},
  // 14 — Soul / R&B 70s (open hat groove, clap+snare 2&4, tamb, ghost snare)
  {"Soul / R&B", {
    /*00*/{{36,110},{46,70},{0,0},{0,0}},   /*01*/{{46,55},{38,38},{0,0},{0,0}},
    /*02*/{{46,68},{0,0},{0,0},{0,0}},      /*03*/{{46,55},{0,0},{0,0},{0,0}},
    /*04*/{{39,102},{38,100},{54,85},{0,0}},/*05*/{{46,55},{38,38},{0,0},{0,0}},
    /*06*/{{36,90},{46,68},{0,0},{0,0}},    /*07*/{{46,55},{0,0},{0,0},{0,0}},
    /*08*/{{36,108},{46,70},{0,0},{0,0}},   /*09*/{{46,55},{38,38},{0,0},{0,0}},
    /*10*/{{36,82},{46,68},{0,0},{0,0}},    /*11*/{{46,55},{0,0},{0,0},{0,0}},
    /*12*/{{39,102},{38,100},{54,85},{0,0}},/*13*/{{46,55},{38,38},{0,0},{0,0}},
    /*14*/{{36,90},{46,68},{0,0},{0,0}},    /*15*/{{46,55},{49,60},{0,0},{0,0}},
  }},
  // 15 — Cumbia (maracas 8th, kick on 1&3, cowbell off-beat, rim fills)
  {"Cumbia", {
    /*00*/{{36,105},{70,75},{56,70},{0,0}}, /*01*/{{70,60},{0,0},{0,0},{0,0}},
    /*02*/{{70,70},{37,55},{0,0},{0,0}},    /*03*/{{70,60},{56,65},{0,0},{0,0}},
    /*04*/{{70,72},{0,0},{0,0},{0,0}},      /*05*/{{70,60},{37,55},{0,0},{0,0}},
    /*06*/{{70,68},{56,70},{0,0},{0,0}},    /*07*/{{70,60},{0,0},{0,0},{0,0}},
    /*08*/{{36,95},{70,75},{0,0},{0,0}},    /*09*/{{70,60},{56,65},{0,0},{0,0}},
    /*10*/{{70,70},{37,55},{0,0},{0,0}},    /*11*/{{70,60},{0,0},{0,0},{0,0}},
    /*12*/{{70,72},{56,70},{0,0},{0,0}},    /*13*/{{70,60},{37,55},{0,0},{0,0}},
    /*14*/{{36,82},{70,68},{0,0},{0,0}},    /*15*/{{70,60},{56,65},{0,0},{0,0}},
  }},
  // 16 — 6/8 Shuffle (triplet feel: crash-hat-hat / snare-hat-hat cycle)
  {"6/8 Shuffle", {
    /*00*/{{36,112},{42,80},{49,75},{0,0}}, /*01*/{{42,58},{0,0},{0,0},{0,0}},
    /*02*/{{42,68},{0,0},{0,0},{0,0}},      /*03*/{{38,90},{42,75},{0,0},{0,0}},
    /*04*/{{42,58},{0,0},{0,0},{0,0}},      /*05*/{{42,68},{0,0},{0,0},{0,0}},
    /*06*/{{36,88},{42,78},{0,0},{0,0}},    /*07*/{{42,60},{0,0},{0,0},{0,0}},
    /*08*/{{36,108},{42,80},{0,0},{0,0}},   /*09*/{{42,58},{0,0},{0,0},{0,0}},
    /*10*/{{42,68},{0,0},{0,0},{0,0}},      /*11*/{{38,95},{42,75},{0,0},{0,0}},
    /*12*/{{42,58},{0,0},{0,0},{0,0}},      /*13*/{{42,68},{0,0},{0,0},{0,0}},
    /*14*/{{36,85},{42,80},{0,0},{0,0}},    /*15*/{{42,58},{46,65},{0,0},{0,0}},
  }},
};
inline constexpr int kDrumPatternCount =
    (int)(sizeof(kDrumPatterns) / sizeof(kDrumPatterns[0]));

} // namespace mm
