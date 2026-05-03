#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
generate_patterns.py
====================
Extrapole 1000 patterns (contour melodique + rythme) a partir des seeds
apprises sur 575 MIDIs (ichigos + VGMidi).

Chaque pattern est index par son seed (0-999) de sorte que dans le plugin
  patterns[seed]  donne directement le contour + rythme associe.

Techniques d'extrapolation :
  direct       -- les N patterns les plus frequents du corpus
  inverse      -- inversion melodique (deg -> (max-deg))
  retrograde   -- lecture a l'envers
  interpolate  -- moyenne element par element de deux contours
  mutate       -- changement d'un degre par +/-1
  swing_rhythm -- conversion d'un rythme binaire en feel swing
  density      -- version allégée ou dense du rythme
  chain        -- enchaînement de deux contours distants

Sortie : data/patterns/generated_patterns.h
"""

import io, json, random, sys
from pathlib import Path

sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")

BASE    = Path(__file__).parent / "data" / "patterns"
OUT     = BASE / "generated_patterns.h"
SEED    = 42
MAX_DEG = 6   # degres diatoniques 0..6

# ---------------------------------------------------------------------------
# Chargement
# ---------------------------------------------------------------------------

def load_json(path: Path, key: str) -> list[tuple[tuple, int]]:
    data = json.loads(path.read_text(encoding="utf-8"))
    return [(tuple(row[0]), row[1]) for row in data.get(key, [])]

def rhythm_as_uint16(r: tuple) -> int:
    v = 0
    for i, b in enumerate(r[:16]):
        if b: v |= (1 << (15 - i))
    return v

def uint16_to_rhythm(v: int) -> tuple:
    return tuple(1 if (v >> (15 - i)) & 1 else 0 for i in range(16))

# ---------------------------------------------------------------------------
# Extrapolation : contours
# ---------------------------------------------------------------------------

def invert(c: tuple) -> tuple:
    """Inversion melodique : deg -> MAX_DEG - deg."""
    return tuple(MAX_DEG - d for d in c)

def retrograde(c: tuple) -> tuple:
    return c[::-1]

def retrograde_inversion(c: tuple) -> tuple:
    return retrograde(invert(c))

def interpolate(a: tuple, b: tuple) -> tuple:
    return tuple(round((x + y) / 2) for x, y in zip(a, b))

def mutate(c: tuple, rng: random.Random, delta: int = 1) -> tuple:
    i = rng.randrange(len(c))
    lst = list(c)
    lst[i] = max(0, min(MAX_DEG, lst[i] + rng.choice([-delta, delta])))
    return tuple(lst)

def augment(c: tuple) -> tuple:
    """Etire le contour : 4 degres -> 4 degres mais avec repetition au milieu."""
    # [a,b,c,d] -> [a, b, b, c]  (accent sur le 2e degre)
    return (c[0], c[1], c[1], c[2])

def sequence_chain(a: tuple, b: tuple) -> tuple:
    """Enchaine la fin de a avec le debut de b, moyenne au raccord."""
    mid = round((a[-1] + b[0]) / 2)
    return (a[0], mid, b[1], b[2])

# ---------------------------------------------------------------------------
# Extrapolation : rythmes
# ---------------------------------------------------------------------------

def swing_rhythm(v: int) -> int:
    """Convertit 8 croches regulieres en feel swing (triple) approx."""
    # Active les 8 croches paires (strong-eighth feel)
    # Swing = garder 1 et 5 de chaque paire de doubles croches
    result = 0
    for beat in range(4):
        base = beat * 4
        # strong : base (slot 0), swing-after : base+2 (slot 2 du groupe)
        result |= (1 << (15 - base))
        result |= (1 << (15 - (base + 2)))
    # Merge avec le rythme original sur les temps forts
    return (v & 0x8888) | result

def thin_rhythm(v: int, keep_mask: int = 0xAAAA) -> int:
    """Allegit un rythme dense en gardant 1 note sur 2."""
    return v & keep_mask

def syncopate(v: int) -> int:
    """Decale les attaques du 2eme et 4eme temps d'un slot en avant."""
    r = list(uint16_to_rhythm(v))
    # Decale slot 4 -> 3, slot 12 -> 11
    for shift_src, shift_dst in [(4, 3), (12, 11)]:
        if r[shift_src] and not r[shift_dst]:
            r[shift_dst] = 1
            r[shift_src] = 0
    return rhythm_as_uint16(tuple(r))

# ---------------------------------------------------------------------------
# Pipeline principal
# ---------------------------------------------------------------------------

def describe_source(s: str) -> str:
    codes = {
        "direct": 0, "invert": 1, "retro": 2, "retro_inv": 3,
        "interp": 4, "mutate": 5, "augment": 6, "chain": 7,
        "swing": 8, "thin": 9, "synco": 10,
    }
    return codes.get(s, 15)

def main() -> None:
    rng = random.Random(SEED)

    ichigos = BASE / "ichigos_report.json"
    vgmidi  = BASE / "report.json"
    if not ichigos.exists() or not vgmidi.exists():
        sys.exit("[ERROR] Lancer d'abord analyze_ichigos.py et midi_pattern_learner.py")

    # Contours fusionnes (sommer les counts)
    cont_map: dict[tuple, int] = {}
    for c, n in load_json(ichigos, "all_rh_contour_counts"):
        cont_map[c] = cont_map.get(c, 0) + n
    for c, n in load_json(vgmidi, "all_contour_counts"):
        cont_map[c] = cont_map.get(c, 0) + n
    contours = sorted(cont_map.items(), key=lambda x: -x[1])

    # Rythmes fusionnes
    rhy_map: dict[tuple, int] = {}
    for r, n in load_json(ichigos, "all_rh_rhythm_counts"):
        rhy_map[r] = rhy_map.get(r, 0) + n
    for r, n in load_json(vgmidi, "all_rhythm_counts"):
        rhy_map[r] = rhy_map.get(r, 0) + n
    rhythms = sorted(rhy_map.items(), key=lambda x: -x[1])

    print(f"Base : {len(contours)} contours, {len(rhythms)} rythmes")

    top_c = [c for c, _ in contours[:30]]
    top_r = [r for r, _ in rhythms[:30]]

    patterns: list[dict] = []

    def add(cont: tuple, rhy_u16: int, src: str) -> None:
        patterns.append({"contour": cont, "rhythm": rhy_u16, "src": src})

    # 1. Direct : top 20 contours x top 3 rythmes les plus typiques
    for i in range(min(20, len(top_c))):
        r_idx = i % 3
        add(top_c[i], rhythm_as_uint16(top_r[r_idx]), "direct")

    # 2. Inversion des 15 premiers contours
    seen_inv: set[tuple] = set()
    for c in top_c[:15]:
        inv = invert(c)
        if inv not in seen_inv and inv != c:
            r_idx = len(seen_inv) % 5 + 3
            add(inv, rhythm_as_uint16(top_r[r_idx]), "invert")
            seen_inv.add(inv)

    # 3. Retrogrades
    for c in top_c[:12]:
        ret = retrograde(c)
        if ret != c:
            add(ret, rhythm_as_uint16(top_r[len(patterns) % 6]), "retro")

    # 4. Retrograde-inversion
    for c in top_c[:8]:
        ri = retrograde_inversion(c)
        add(ri, rhythm_as_uint16(top_r[len(patterns) % 4]), "retro_inv")

    # 5. Interpolations entre paires consecutives
    for i in range(0, min(14, len(top_c) - 1), 2):
        interp = interpolate(top_c[i], top_c[i + 1])
        add(interp, rhythm_as_uint16(top_r[(i // 2) % 8]), "interp")

    # 6. Mutations aleatoires depuis les top 10
    for i in range(10):
        base = top_c[i % len(top_c)]
        mut = mutate(base, rng)
        add(mut, rhythm_as_uint16(top_r[(i + 2) % 10]), "mutate")

    # 7. Augmentation
    for c in top_c[:8]:
        add(augment(c), rhythm_as_uint16(top_r[2]), "augment")

    # 8. Chaining entre paires eloignees
    for i in range(6):
        a = top_c[i]
        b = top_c[(i + 7) % len(top_c)]
        add(sequence_chain(a, b), rhythm_as_uint16(top_r[i % 5]), "chain")

    # ---- Rythmes varies avec le contour le plus typique -------------------
    anchor_c = top_c[0]

    # 9. Swing
    for r, _ in rhythms[:6]:
        swung = swing_rhythm(rhythm_as_uint16(r))
        add(anchor_c, swung, "swing")

    # 10. Thinned
    for r, _ in rhythms[:4]:
        thinned = thin_rhythm(rhythm_as_uint16(r))
        if thinned:
            add(top_c[1] if len(top_c) > 1 else anchor_c, thinned, "thin")

    # 11. Syncope
    for r, _ in rhythms[:4]:
        synco = syncopate(rhythm_as_uint16(r))
        if synco:
            add(top_c[2] if len(top_c) > 2 else anchor_c, synco, "synco")

    # Deduplique et plafonne a 1000
    seen: set[tuple] = set()
    unique: list[dict] = []
    for p in patterns:
        key = (p["contour"], p["rhythm"])
        if key not in seen:
            seen.add(key)
            unique.append(p)
        if len(unique) >= 1000:
            break

    # Remplit jusqu'a 1000 avec des variants mutes si besoin
    # On varie aussi le rythme pour maximiser la diversite
    fill_rng = rng
    fill_i = 0  # compteur independant pour eviter infinite loop sur doublons
    while len(unique) < 1000:
        i = fill_i
        fill_i += 1
        if fill_i > 200000:  # garde-fou
            break
        base = top_c[i % len(top_c)]
        r_src = top_r[(i * 7 + 3) % len(top_r)]
        r_u16 = rhythm_as_uint16(r_src)

        # Alterner les techniques de remplissage
        tech = i % 6
        if tech == 0:
            cand = mutate(base, fill_rng, delta=1)
        elif tech == 1:
            cand = mutate(invert(base), fill_rng, delta=1)
        elif tech == 2:
            cand = interpolate(base, top_c[(i + 13) % len(top_c)])
        elif tech == 3:
            cand = retrograde(mutate(base, fill_rng))
        elif tech == 4:
            cand = augment(base)
            r_u16 = syncopate(r_u16) or r_u16
        else:
            a = top_c[i % len(top_c)]
            b = top_c[(i + 17) % len(top_c)]
            cand = sequence_chain(a, b)

        src_name = ["mutate","inv_mut","interp","retro_mut","augment","chain"][tech]
        key = (cand, r_u16)
        if key not in seen:
            seen.add(key)
            unique.append({"contour": cand, "rhythm": r_u16, "src": src_name})

    # Si toujours < 1000 (trop de collisions), on cycle les contours corpus
    # avec tous les rythmes disponibles (pas de dedup strict)
    if len(unique) < 1000:
        all_rhy_u16 = [rhythm_as_uint16(r) for r, _ in rhythms]
        idx = 0
        while len(unique) < 1000:
            c_idx = (idx * 7 + len(unique)) % len(top_c)
            r_idx = (idx * 13 + len(unique)) % len(all_rhy_u16)
            unique.append({
                "contour": top_c[c_idx],
                "rhythm": all_rhy_u16[r_idx],
                "src": "cycle"
            })
            idx += 1

    print(f"Genere : {len(unique)} patterns")
    stats: dict[str, int] = {}
    for p in unique:
        stats[p["src"]] = stats.get(p["src"], 0) + 1
    for src, n in sorted(stats.items(), key=lambda x: -x[1]):
        print(f"  {src:<12} {n}")

    # --- Header C++ ---------------------------------------------------------
    lines = [
        "// generated_patterns.h -- generated by generate_patterns.py",
        "// 1000 patterns indexes par seed (0-999), extraits de 575 MIDIs",
        "// contour : 4 degres diatoniques (0=racine .. 6=7eme)",
        "// rhythm  : grille 16 slots binaires, bit15=premier temps",
        "// src     : 0=direct 1=invert 2=retro 3=retro_inv 4=interp",
        "//           5=mutate 6=augment 7=chain 8=swing 9=thin 10=synco",
        "// Usage   : gen_patterns::get(seed) -> contour + rythme pour ce seed",
        "#pragma once",
        "#include <cstdint>",
        "",
        "namespace gen_patterns {",
        "",
        "struct Pattern {",
        "    uint8_t  contour[4];  // degres diatoniques par beat",
        "    uint16_t rhythm;      // grille 16 slots, bit15=slot0",
        "    uint8_t  src;         // technique d'extrapolation",
        "};",
        "",
        f"// {len(unique)} patterns, un par seed (0-{len(unique)-1})",
        "static const Pattern patterns[1000] = {",
    ]

    for p in unique:
        c = p["contour"]
        r = p["rhythm"]
        s = describe_source(p["src"])
        lines.append(f"    {{{{{c[0]},{c[1]},{c[2]},{c[3]}}}, 0x{r:04X}, {s}}},  // {p['src']}")

    lines += [
        "};",
        "",
        "static constexpr int PATTERN_COUNT = 1000;",
        "",
        "// Lookup rapide : retourne le pattern pour un seed donne",
        "inline const Pattern& get(int seed) {",
        "    return patterns[seed < 0 ? 0 : seed % PATTERN_COUNT];",
        "}",
        "",
        "}  // namespace gen_patterns",
        "",
    ]

    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text("\n".join(lines), encoding="utf-8")
    print(f"[OK] {OUT}")

if __name__ == "__main__":
    main()
