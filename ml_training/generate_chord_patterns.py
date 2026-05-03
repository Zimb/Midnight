#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
generate_chord_patterns.py
===========================
Génère 1000 patterns d'accompagnement (main gauche) indexes par seed (0-999).

Chaque pattern combine :
  progression[4] : 4 degrés diatoniques de racines sur 4 mesures (ex. 0,3,4,0 = I-IV-V-I)
  voicing[3]     : intervalles relatifs de l'accord (ex. 0,2,4 = triade)
  rhythm         : uint16 grille 16 slots pour l'accompagnement
  style          : 0=block 1=alberti 2=arpeggio 3=bass_melody 4=octave_bass

Techniques d'extrapolation :
  direct         : progressions les plus fréquentes du corpus
  transpose      : transposition de la progression d'un degré
  invert_prog    : renversement de la progression (lecture à l'envers)
  substitute     : remplacement d'un degré par son substitut (ex. IV->bVII)
  extend         : ajout d'un accord de passage entre deux degrés éloignés
  pivot          : emprunt modal (ex. degré mineur dans progression majeure)
  mutate         : variation d'un degré ±1

Sortie : data/patterns/generated_chord_patterns.h
"""

import io, json, random, sys
from pathlib import Path

sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")

BASE = Path(__file__).parent / "data" / "patterns"
OUT  = BASE / "generated_chord_patterns.h"
SEED = 42
MAX_DEG = 6

# ---------------------------------------------------------------------------
# Chargement
# ---------------------------------------------------------------------------

def load_counts(path: Path, key: str) -> list[tuple[tuple, int]]:
    data = json.loads(path.read_text(encoding="utf-8"))
    return [(tuple(row[0]), row[1]) for row in data.get(key, [])]

def rhythm_as_uint16(r: tuple) -> int:
    v = 0
    for i, b in enumerate(r[:16]):
        if b: v |= (1 << (15 - i))
    return v

# ---------------------------------------------------------------------------
# Techniques d'extrapolation : progressions
# ---------------------------------------------------------------------------

def transpose_prog(p: tuple, step: int) -> tuple:
    """Transpose tous les degrés de +step (modulo 7)."""
    return tuple((d + step) % (MAX_DEG + 1) for d in p)

def invert_prog(p: tuple) -> tuple:
    """Lecture à l'envers."""
    return p[::-1]

def substitute(p: tuple, rng: random.Random) -> tuple:
    """Remplace un degré par son substitut (II->IV, VI->IV, VII->V)."""
    subs = {1: 3, 5: 3, 6: 4, 2: 0}  # degrés -> substituts courants
    lst = list(p)
    candidates = [i for i, d in enumerate(lst) if d in subs]
    if candidates:
        i = rng.choice(candidates)
        lst[i] = subs[lst[i]]
    return tuple(lst)

def extend_prog(p: tuple) -> tuple:
    """Remplace le 3ème degré par un accord de passage (milieu entre deg2 et deg3)."""
    lst = list(p)
    mid = round((p[1] + p[2]) / 2) % (MAX_DEG + 1)
    lst[2] = mid
    return tuple(lst)

def pivot_prog(p: tuple, rng: random.Random) -> tuple:
    """Emprunt modal : remplace un accord majeur par son équivalent mineur (±3 demi-tons)."""
    lst = list(p)
    i = rng.randrange(len(lst))
    lst[i] = (lst[i] + 2) % (MAX_DEG + 1)
    return tuple(lst)

def mutate_prog(p: tuple, rng: random.Random) -> tuple:
    i = rng.randrange(len(p))
    lst = list(p)
    lst[i] = (lst[i] + rng.choice([-1, 1])) % (MAX_DEG + 1)
    return tuple(lst)

# ---------------------------------------------------------------------------
# Styles de rythme par accompagnement
# ---------------------------------------------------------------------------

# Rythmes prédéfinis par style
STYLE_RHYTHMS = {
    "block":      [0x8888, 0x8080, 0x8000, 0xCCCC],
    "alberti":    [0xAAAA, 0xA8A8, 0xAA88, 0xA888],
    "arpeggio":   [0xAAAA, 0xA2A2, 0x8888, 0xAA88],
    "bass_melody":[0x8880, 0x8808, 0x8888, 0x8800],
    "octave_bass":[0x8008, 0x8080, 0x8000, 0x8888],
}

STYLE_IDS = {"block": 0, "alberti": 1, "arpeggio": 2, "bass_melody": 3, "octave_bass": 4}

# Voicings canoniques par nombre de notes
VOICINGS = {
    "dyad_3rd":    (0, 2),       # R + tierce
    "dyad_5th":    (0, 4),       # R + quinte
    "triad":       (0, 2, 4),    # R + tierce + quinte
    "sus4":        (0, 3, 4),    # R + quarte + quinte
    "6th":         (0, 2, 5),    # R + tierce + sixte
    "7th":         (0, 2, 4, 6), # R + tierce + quinte + septieme
    "open_5th":    (0, 4),       # quinte ouverte
    "add9":        (0, 2, 4, 1), # accord add 9eme
}

VOICING_NAMES = list(VOICINGS.keys())

# Correspondance style LH -> voicings préférés
STYLE_VOICINGS = {
    "block":       ["triad", "7th", "6th"],
    "alberti":     ["triad", "dyad_3rd"],
    "arpeggio":    ["triad", "7th", "6th"],
    "bass_melody": ["dyad_5th", "dyad_3rd"],
    "octave_bass": ["dyad_5th", "open_5th"],
}

# ---------------------------------------------------------------------------
# Pipeline principal
# ---------------------------------------------------------------------------

def main() -> None:
    rng = random.Random(SEED)

    ichigos_path = BASE / "ichigos_report.json"
    if not ichigos_path.exists():
        sys.exit("[ERROR] Lancer d'abord analyze_ichigos.py")

    # Progressions extraites
    prog_counts = load_counts(ichigos_path, "all_lh_prog_counts")
    chord_counts = load_counts(ichigos_path, "all_lh_chord_counts")
    lh_rhythm_counts = load_counts(ichigos_path, "all_lh_rhythm_counts")
    lh_styles = json.loads(ichigos_path.read_text(encoding="utf-8")).get("lh_styles", {})

    if not prog_counts:
        sys.exit("[ERROR] Pas de progressions dans le JSON — relancer analyze_ichigos.py d'abord")

    print(f"Progressions uniques : {len(prog_counts)}")
    print(f"Voicings uniques     : {len(chord_counts)}")
    print(f"Rythmes LH uniques   : {len(lh_rhythm_counts)}")
    print(f"Styles LH            : {dict(list(lh_styles.items())[:6])}")
    print()

    # Top données de base
    top_progs  = [p for p, _ in prog_counts[:50]]
    top_chords = [c for c, _ in chord_counts[:20]]
    top_rhythms = [r for r, _ in lh_rhythm_counts[:20]]

    # Déterminer le style dominant pour chaque rythme slot
    total_lh = sum(lh_styles.values()) or 1
    dominant_style = max(lh_styles.items(), key=lambda x: x[1])[0]

    # Ordre des styles par fréquence
    style_order = [s for s, _ in sorted(lh_styles.items(), key=lambda x: -x[1])]
    style_order = [s for s in style_order if s in STYLE_RHYTHMS]

    patterns: list[dict] = []
    seen: set[tuple] = set()

    def add(prog: tuple, voicing: tuple, rhy: int, style: str, src: str) -> bool:
        key = (prog, voicing, rhy)
        if key in seen:
            return False
        seen.add(key)
        patterns.append({"prog": prog, "voicing": voicing, "rhythm": rhy,
                          "style": style, "src": src})
        return True

    # --- 1. Direct : top progressions × styles × voicings ---
    for i, prog in enumerate(top_progs[:30]):
        style = style_order[i % len(style_order)]
        r_rhythms = STYLE_RHYTHMS[style]
        r_u16 = r_rhythms[i % len(r_rhythms)]
        # Préférer le voicing corpus si disponible
        if top_chords:
            voicing = tuple(top_chords[i % len(top_chords)])
        else:
            v_name = STYLE_VOICINGS[style][0]
            voicing = VOICINGS[v_name]
        add(prog, voicing, r_u16, style, "direct")

    # --- 2. Transpositions (tous les degrés +1, +2, +3, +5) ---
    for step in [1, 2, 3, 5]:
        for prog in top_progs[:15]:
            tp = transpose_prog(prog, step)
            style = style_order[step % len(style_order)]
            rhy = STYLE_RHYTHMS[style][step % len(STYLE_RHYTHMS[style])]
            v_name = STYLE_VOICINGS[style][0]
            voicing = VOICINGS[v_name]
            add(tp, voicing, rhy, style, "transpose")

    # --- 3. Rétrogrades ---
    for prog in top_progs[:20]:
        inv = invert_prog(prog)
        style = style_order[1 % len(style_order)]
        rhy = STYLE_RHYTHMS[style][0]
        voicing = VOICINGS["triad"] if "triad" in VOICINGS else top_chords[0] if top_chords else (0, 2, 4)
        add(inv, tuple(voicing), rhy, style, "invert_prog")

    # --- 4. Substitutions ---
    for prog in top_progs[:20]:
        sub = substitute(prog, rng)
        if sub != prog:
            style = style_order[2 % len(style_order)]
            rhy = STYLE_RHYTHMS[style][1]
            voicing = VOICINGS["6th"]
            add(sub, voicing, rhy, style, "substitute")

    # --- 5. Extensions (accord de passage) ---
    for prog in top_progs[:15]:
        ext = extend_prog(prog)
        if ext != prog:
            style = "arpeggio" if "arpeggio" in STYLE_RHYTHMS else style_order[0]
            rhy = STYLE_RHYTHMS[style][0]
            voicing = VOICINGS["7th"]
            add(ext, voicing, rhy, style, "extend")

    # --- 6. Pivot modal ---
    for prog in top_progs[:15]:
        piv = pivot_prog(prog, rng)
        if piv != prog:
            style = style_order[3 % len(style_order)]
            rhy = STYLE_RHYTHMS[style][2]
            voicing = VOICINGS["sus4"]
            add(piv, voicing, rhy, style, "pivot")

    # --- 7. Mutations ---
    for i, prog in enumerate(top_progs[:20]):
        mut = mutate_prog(prog, rng)
        style = style_order[i % len(style_order)]
        rhy = STYLE_RHYTHMS[style][i % len(STYLE_RHYTHMS[style])]
        v_name = STYLE_VOICINGS[style][i % len(STYLE_VOICINGS[style])]
        add(mut, VOICINGS[v_name], rhy, style, "mutate")

    # --- 8. Croisements voicings × rythmes corpus ---
    for i, (chord, _) in enumerate(chord_counts[:15]):
        prog = top_progs[i % len(top_progs)]
        style = style_order[i % len(style_order)]
        if top_rhythms:
            rhy = rhythm_as_uint16(top_rhythms[i % len(top_rhythms)])
        else:
            rhy = STYLE_RHYTHMS[style][0]
        add(prog, tuple(chord), rhy, style, "chord_cross")

    # --- Remplissage jusqu'à 1000 avec variantes ---
    fill_techniques = [
        lambda p, i: (transpose_prog(p, (i % 6) + 1), "transpose"),
        lambda p, i: (mutate_prog(p, rng), "mutate"),
        lambda p, i: (invert_prog(mutate_prog(p, rng)), "inv_mut"),
        lambda p, i: (substitute(p, rng), "substitute"),
        lambda p, i: (pivot_prog(p, rng), "pivot"),
        lambda p, i: (extend_prog(p), "extend"),
    ]

    fill_i = 0  # compteur indépendant pour éviter infinite loop sur doublons
    while len(patterns) < 1000:
        i = fill_i
        fill_i += 1
        if fill_i > 200000:  # garde-fou
            break
        base_prog = top_progs[i % len(top_progs)]
        tech_fn = fill_techniques[i % len(fill_techniques)]
        new_prog, src = tech_fn(base_prog, i)

        style = style_order[i % len(style_order)]
        rhy_list = STYLE_RHYTHMS[style]
        rhy = rhy_list[i % len(rhy_list)]

        if top_chords:
            voicing = tuple(top_chords[(i * 3 + 1) % len(top_chords)])
        else:
            v_name = STYLE_VOICINGS[style][(i // len(style_order)) % len(STYLE_VOICINGS[style])]
            voicing = VOICINGS[v_name]

        add(new_prog, voicing, rhy, style, src)

    print(f"Genere : {len(patterns)} patterns d'accords")
    stats: dict[str, int] = {}
    style_stats: dict[str, int] = {}
    for p in patterns:
        stats[p["src"]] = stats.get(p["src"], 0) + 1
        style_stats[p["style"]] = style_stats.get(p["style"], 0) + 1
    print("Techniques :")
    for src, n in sorted(stats.items(), key=lambda x: -x[1]):
        print(f"  {src:<14} {n}")
    print("Styles :")
    for s, n in sorted(style_stats.items(), key=lambda x: -x[1]):
        print(f"  {s:<14} {n}")

    # --- Header C++ -----------------------------------------------------------
    def style_id(s: str) -> int:
        return STYLE_IDS.get(s, 0)

    def fmt_voicing(v: tuple) -> str:
        padded = list(v)[:4]
        while len(padded) < 4:
            padded.append(0xFF)  # 0xFF = slot vide
        return "{" + ",".join(str(x) for x in padded[:4]) + "}"

    lines = [
        "// generated_chord_patterns.h -- generated by generate_chord_patterns.py",
        "// 1000 patterns d'accompagnement indexes par seed (0-999)",
        "// extraits et extrapoles depuis 575 MIDIs (371 ichigos + 204 VGMidi)",
        "//",
        "// progression[4] : degres diatoniques racines sur 4 mesures (0=I .. 6=VII)",
        "// voicing[4]     : intervalles relatifs de l'accord, 0xFF=slot vide",
        "// rhythm         : grille 16 slots, bit15=slot0",
        "// style          : 0=block 1=alberti 2=arpeggio 3=bass_melody 4=octave_bass",
        "//",
        "// Usage : chord_patterns::get(seed) -> pattern pour ce seed",
        "#pragma once",
        "#include <cstdint>",
        "",
        "namespace chord_patterns {",
        "",
        "struct ChordPattern {",
        "    uint8_t  progression[4];  // degres racines sur 4 mesures",
        "    uint8_t  voicing[4];      // intervalles accord (0xFF=non utilise)",
        "    uint16_t rhythm;          // grille 16 slots",
        "    uint8_t  style;           // 0=block 1=alberti 2=arpeggio 3=bass_melody 4=octave_bass",
        "    uint8_t  src;             // technique (0=direct 1=transpose 2=invert 3=sub 4=extend 5=pivot 6=mutate)",
        "};",
        "",
        "static const ChordPattern patterns[1000] = {",
    ]

    src_ids = {
        "direct": 0, "transpose": 1, "invert_prog": 2, "substitute": 3,
        "extend": 4, "pivot": 5, "mutate": 6, "chord_cross": 7,
        "inv_mut": 8,
    }

    for p in patterns:
        prog = p["prog"]
        voic = fmt_voicing(p["voicing"])
        rhy  = p["rhythm"]
        st   = style_id(p["style"])
        src  = src_ids.get(p["src"], 6)
        p_str = ",".join(str(d) for d in prog)
        lines.append(f"    {{{{{p_str}}}, {voic}, 0x{rhy:04X}, {st}, {src}}},  // {p['src']} / {p['style']}")

    lines += [
        "};",
        "",
        "static constexpr int CHORD_PATTERN_COUNT = 1000;",
        "",
        "inline const ChordPattern& get(int seed) {",
        "    return patterns[seed < 0 ? 0 : seed % CHORD_PATTERN_COUNT];",
        "}",
        "",
        "}  // namespace chord_patterns",
        "",
    ]

    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text("\n".join(lines), encoding="utf-8")
    print(f"[OK] {OUT}")

if __name__ == "__main__":
    main()
