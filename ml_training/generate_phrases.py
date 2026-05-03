#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
generate_phrases.py
====================
Génère 800 phrases mélodiques avec durées réelles, indexées par (mode, prog_arch, seed).
Structure : 2 modes × 8 archétypes × 50 phrases = 800 total.

Chaque phrase = séquence de 3-8 notes avec :
  degree    : degré diatonique (0=I .. 6=VII)
  dur_16ths : durée en 16ièmes (1=double-croche, 4=noire, 8=blanche, ...)

Tri au sein de chaque contexte : weight décroissant.
  weight=3 : phrase extraite du corpus (la plus fidèle)
  weight=2 : variante du corpus (mutation, inversion, rétrograde, etc.)
  weight=1 : générée algorithmiquement

=> seed faible = phrase la plus populaire du corpus.

Sortie : data/patterns/generated_phrases.h
"""

import io, json, math, random, sys
from collections import defaultdict
from pathlib import Path

sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")

BASE = Path(__file__).parent / "data" / "patterns"
OUT  = BASE / "generated_phrases.h"
RNG_SEED = 42

N_MODES      = 2
N_ARCHS      = 8
N_PER_CTX    = 50   # phrases par contexte (mode, arch)

# ---------------------------------------------------------------------------
# Archétypes de progressions (8, couvrent ~90% du corpus ichigos)
# ---------------------------------------------------------------------------
PROG_ARCHETYPES_8 = [
    (0, 0, 0, 0),   # 0: pédale tonique
    (0, 4, 0, 4),   # 1: I-V
    (0, 3, 4, 0),   # 2: I-IV-V-I
    (0, 5, 3, 4),   # 3: I-VI-IV-V (pop cadence)
    (0, 6, 5, 4),   # 4: descente basse
    (0, 0, 0, 4),   # 5: tonique → dominante
    (0, 5, 0, 4),   # 6: I-VI-I-V
    (5, 5, 5, 5),   # 7: pédale VI (relative mineure)
]

ARCH_NAMES = [
    "tonic_pedal", "I_V", "I_IV_V_I", "pop_cadence",
    "descending",  "tonic_dom", "I_VI_I_V", "vi_pedal",
]

VALID_DURS = [1, 2, 3, 4, 6, 8, 12, 16]

# Notes d'accord par degré (pour orienter la fin de phrase)
CHORD_TONES = {
    0: [0, 2, 4],  # I
    1: [1, 3, 5],  # II
    2: [2, 4, 6],  # III
    3: [3, 5, 0],  # IV
    4: [4, 6, 1],  # V
    5: [5, 0, 2],  # VI
    6: [6, 1, 3],  # VII
}

# ---------------------------------------------------------------------------
# Scoring et transformations
# ---------------------------------------------------------------------------

def score_phrase(ph: list) -> float:
    """Qualité : récompense la variété des degrés, durées, mouvement mélodique."""
    degs = [n[0] for n in ph]
    durs = [n[1] for n in ph]
    deg_variety   = len(set(degs)) / len(degs)
    dur_variety   = len(set(durs)) / len(durs)
    step_interest = sum(1 for i in range(1, len(degs))
                        if degs[i] != degs[i-1]) / max(1, len(degs) - 1)
    # Pénalise les phrases avec tous les mêmes degrés
    monotone_pen  = 1.0 if len(set(degs)) > 1 else 0.3
    return (deg_variety * 0.4 + dur_variety * 0.3 + step_interest * 0.3) * monotone_pen


def mutate_phrase(ph: list, rng: random.Random) -> list:
    """Change un degré ±1 (modulo 7)."""
    r = [list(n) for n in ph]
    i = rng.randrange(len(r))
    r[i][0] = (r[i][0] + rng.choice([-1, 1])) % 7
    return r


def invert_phrase(ph: list) -> list:
    """Inversion mélodique : degré → (min+max - degré) % 7."""
    degs = [n[0] for n in ph]
    mn, mx = min(degs), max(degs)
    return [[(mn + mx - n[0]) % 7, n[1]] for n in ph]


def retrograde_phrase(ph: list) -> list:
    """Rétrograde : lecture à l'envers."""
    return list(reversed([[n[0], n[1]] for n in ph]))


def augment_phrase(ph: list) -> list:
    """Augmentation : durées × 2, clampées à 16."""
    return [[n[0], min(16, n[1] * 2)] for n in ph]


def diminish_phrase(ph: list) -> list:
    """Diminution : durées ÷ 2, minimum 1."""
    return [[n[0], max(1, n[1] // 2)] for n in ph]


def transpose_phrase(ph: list, step: int) -> list:
    """Transposition de tous les degrés."""
    return [[(n[0] + step) % 7, n[1]] for n in ph]


# ---------------------------------------------------------------------------
# Génération algorithmique
# ---------------------------------------------------------------------------

# Poids de départ par mode (degré initial)
_START_W = {
    0: [10, 2, 4, 3, 6, 3, 2],   # major : tonique forte
    1: [8,  2, 2, 4, 4, 6, 2],   # minor : tonique + VI fort
}

# Poids de durées [1,2,3,4,6,8,12,16] selon position métrique
_DUR_W_STRONG = [1, 3, 4, 8, 4, 3, 1, 1]   # temps fort → noire probable
_DUR_W_WEAK   = [3, 8, 4, 4, 2, 1, 0, 0]   # temps faible → croche probable


def algo_phrase(rng: random.Random, mode: int, arch: int) -> list:
    """
    Génère une phrase algorithmiquement.
    Utilise les règles modales + l'archétype de progression pour guider le mouvement.
    """
    n_notes = rng.choices([4, 5, 6, 7], weights=[10, 20, 15, 8])[0]
    prog    = PROG_ARCHETYPES_8[arch]

    deg   = rng.choices(range(7), weights=_START_W[mode])[0]
    notes = []

    for i in range(n_notes):
        # Durée selon la position métrique (pair = fort)
        dur_w = _DUR_W_STRONG if i % 2 == 0 else _DUR_W_WEAK
        dur   = rng.choices(VALID_DURS, weights=dur_w)[0]
        notes.append([deg, dur])

        # Degré suivant
        bar_pos   = min(3, i * 4 // n_notes)
        curr_root = prog[bar_pos]
        chord     = CHORD_TONES.get(curr_root, [0, 2, 4])

        is_last       = (i == n_notes - 2)
        is_very_last  = (i == n_notes - 1)

        if is_very_last:
            # Résolution sur tonique ou quinte
            deg = rng.choice([0, 4]) if rng.random() < 0.7 else rng.choice(chord)
        elif is_last and rng.random() < 0.6:
            # Pénultième : mène vers la tonique par demi-pas
            deg = (0 + rng.choice([-1, 1])) % 7
        else:
            roll = rng.random()
            if roll < 0.45:
                deg = (deg + rng.choice([-1, 1])) % 7          # pas
            elif roll < 0.70:
                deg = rng.choice(chord)                          # note d'accord
            elif roll < 0.88:
                pass                                             # répétition
            else:
                deg = (deg + rng.choice([-2, 2, -3, 3])) % 7  # saut

    return notes


# ---------------------------------------------------------------------------
# Pipeline principal
# ---------------------------------------------------------------------------

def main() -> None:
    rng = random.Random(RNG_SEED)

    # Charger le corpus de phrases si disponible
    ichigos_path = BASE / "ichigos_report.json"
    phrase_corpus: dict[str, list] = {}

    if ichigos_path.exists():
        report = json.loads(ichigos_path.read_text(encoding="utf-8"))
        phrase_corpus = report.get("phrase_corpus", {})
        total_corpus  = sum(len(v) for v in phrase_corpus.values())
        print(f"Corpus phrases : {total_corpus} phrases dans {len(phrase_corpus)} contextes")
        if phrase_corpus:
            for k, v in sorted(phrase_corpus.items()):
                mode_id, arch = int(k.split("_")[0]), int(k.split("_")[1])
                print(f"  mode={mode_id} arch={arch} ({ARCH_NAMES[arch]:<16}): {len(v)} phrases corpus")
    else:
        print("[WARN] ichigos_report.json absent — mode 100% algorithmique")

    print()
    all_phrases: list[dict] = []

    for mode in range(N_MODES):
        for arch in range(N_ARCHS):
            ctx_key       = f"{mode}_{arch}"
            raw_corpus    = phrase_corpus.get(ctx_key, [])
            corpus_tuples = [[n for n in ph] for ph in raw_corpus if 3 <= len(ph) <= 8]

            # 1. Déduplication + scoring des phrases corpus
            seen: set[tuple] = set()
            scored: list[tuple[float, list]] = []
            for ph in corpus_tuples:
                key = tuple(tuple(n) for n in ph)
                if key not in seen:
                    seen.add(key)
                    scored.append((score_phrase(ph), ph))
            scored.sort(key=lambda x: -x[0])
            top_corpus = [ph for _, ph in scored[:N_PER_CTX]]

            phrases_ctx: list[dict] = []

            # 2. Ajouter les phrases corpus (weight=3)
            for ph in top_corpus:
                phrases_ctx.append({"notes": ph, "weight": 3, "src": "corpus"})

            # 3. Variantes du corpus (weight=2)
            transforms = [
                ("mutate",     lambda p, _: mutate_phrase(p, rng)),
                ("invert",     lambda p, _: invert_phrase(p)),
                ("retrograde", lambda p, _: retrograde_phrase(p)),
                ("augment",    lambda p, _: augment_phrase(p)),
                ("diminish",   lambda p, _: diminish_phrase(p)),
                ("transp+1",   lambda p, _: transpose_phrase(p, 1)),
                ("transp+2",   lambda p, _: transpose_phrase(p, 2)),
                ("retro_inv",  lambda p, _: retrograde_phrase(invert_phrase(p))),
            ]
            for base_ph in top_corpus[:20]:
                if len(phrases_ctx) >= N_PER_CTX:
                    break
                for name, fn in transforms:
                    if len(phrases_ctx) >= N_PER_CTX:
                        break
                    new_ph = fn(base_ph, rng)
                    if not (3 <= len(new_ph) <= 8):
                        continue
                    key = tuple(tuple(n) for n in new_ph)
                    if key not in seen:
                        seen.add(key)
                        phrases_ctx.append({"notes": new_ph, "weight": 2, "src": name})

            # 4. Remplissage algorithmique (weight=1)
            fill_tries = 0
            while len(phrases_ctx) < N_PER_CTX and fill_tries < 5000:
                fill_tries += 1
                new_ph = algo_phrase(rng, mode, arch)
                key    = tuple(tuple(n) for n in new_ph)
                if key not in seen:
                    seen.add(key)
                    phrases_ctx.append({"notes": new_ph, "weight": 1, "src": "algo"})

            # Trier par poids décroissant (seed faible = plus populaire)
            phrases_ctx.sort(key=lambda x: -x["weight"])
            phrases_ctx = phrases_ctx[:N_PER_CTX]

            n_corp = sum(1 for p in phrases_ctx if p["src"] == "corpus")
            n_var  = sum(1 for p in phrases_ctx if p["src"] not in ("corpus", "algo"))
            n_algo = sum(1 for p in phrases_ctx if p["src"] == "algo")
            print(f"  mode={mode} arch={arch} ({ARCH_NAMES[arch]:<14}): "
                  f"{n_corp:3d} corpus  {n_var:3d} variantes  {n_algo:3d} algo")

            for p in phrases_ctx:
                all_phrases.append({**p, "mode": mode, "arch": arch})

    total = len(all_phrases)
    n_corp_total = sum(1 for p in all_phrases if p["src"] == "corpus")
    n_algo_total = sum(1 for p in all_phrases if p["src"] == "algo")
    print(f"\nTotal : {total} phrases  ({n_corp_total} corpus, {total-n_corp_total-n_algo_total} variantes, {n_algo_total} algo)")

    # ---------------------------------------------------------------------------
    # Header C++
    # ---------------------------------------------------------------------------
    src_ids = {
        "corpus": 0, "mutate": 1, "invert": 2, "retrograde": 3,
        "augment": 4, "diminish": 5, "transp+1": 6, "transp+2": 7,
        "retro_inv": 8, "algo": 9,
    }

    lines = [
        "// generated_phrases.h -- generated by generate_phrases.py",
        f"// {total} phrases melodiques avec durees, par contexte (mode x prog_arch)",
        f"// {N_MODES} modes x {N_ARCHS} archetypes x {N_PER_CTX} phrases = {total}",
        "//",
        "// degree    : degre diatonique 0=I .. 6=VII",
        "// dur_16ths : duree en 16iemes (1=dbl-croche, 2=croche, 4=noire, 8=blanche)",
        "// mode      : 0=majeur  1=mineur",
        "// prog_arch : 0=tonic_pedal  1=I_V  2=I_IV_V_I  3=pop_cadence",
        "//             4=descending   5=tonic_dom  6=I_VI_I_V  7=vi_pedal",
        "// weight    : 3=corpus  2=variante  1=algo",
        "// src       : 0=corpus 1=mutate 2=invert 3=retrograde 4=augment",
        "//             5=diminish 6=transp+1 7=transp+2 8=retro_inv 9=algo",
        "//",
        "// Usage : phrases::get(seed, mode, prog_arch) -> phrase",
        "// Tri   : weight decroissant dans chaque contexte",
        "//         => seed faible = phrase corpus la plus frequente",
        "#pragma once",
        "#include <cstdint>",
        "",
        "namespace phrases {",
        "",
        "struct PhraseNote {",
        "    uint8_t degree;    // degre diatonique 0-6",
        "    uint8_t dur_16ths; // duree en 16iemes: 1,2,3,4,6,8,12,16",
        "};",
        "",
        "struct Phrase {",
        "    PhraseNote notes[8];  // notes[0..n_notes-1] sont valides",
        "    uint8_t    n_notes;   // nombre de notes effectives (3-8)",
        "    uint8_t    mode;      // 0=major 1=minor",
        "    uint8_t    prog_arch; // 0-7",
        "    uint8_t    weight;    // poids de selection (1-3)",
        "    uint8_t    src;       // source (0=corpus..9=algo)",
        "};",
        "",
        f"static constexpr int PHRASE_TOTAL    = {total};",
        f"static constexpr int PHRASES_PER_CTX = {N_PER_CTX};",
        f"static constexpr int N_MODES         = {N_MODES};",
        f"static constexpr int N_ARCHS         = {N_ARCHS};",
        "",
        "// Les 8 archetypes de progressions (degres de racines sur 4 mesures)",
        "static constexpr uint8_t PROG_ARCHETYPES[8][4] = {",
    ]
    for at in PROG_ARCHETYPES_8:
        lines.append(f"    {{{','.join(str(d) for d in at)}}},  // {ARCH_NAMES[PROG_ARCHETYPES_8.index(at)]}")
    lines += [
        "};",
        "",
        f"static const Phrase phrases[{total}] = {{",
    ]

    for p in all_phrases:
        ph    = p["notes"]
        n     = len(ph)
        # Pad à 8 notes avec (0, 4) = tonique noire
        pad   = [[0, 4]] * (8 - n)
        full  = (ph + pad)[:8]
        notes_str = ", ".join("{%d,%d}" % (nn[0], nn[1]) for nn in full)
        src_id    = src_ids.get(p["src"], 9)
        arch_name = ARCH_NAMES[p["arch"]]
        mode_name = "major" if p["mode"] == 0 else "minor"
        lines.append(
            f"    {{{{{notes_str}}}, {n}, {p['mode']}, {p['arch']}, "
            f"{p['weight']}, {src_id}}},  // {mode_name}/{arch_name}/{p['src']}"
        )

    lines += [
        "};",
        "",
        "// Trouve l'archetype le plus proche pour une progression 4-degrés donnée",
        "inline int prog_to_arch(const uint8_t prog[4]) {",
        "    int best = 0, best_dist = 9999;",
        "    for (int a = 0; a < N_ARCHS; a++) {",
        "        int dist = 0;",
        "        for (int i = 0; i < 4; i++)",
        "            dist += prog[i] > PROG_ARCHETYPES[a][i]",
        "                  ? prog[i] - PROG_ARCHETYPES[a][i]",
        "                  : PROG_ARCHETYPES[a][i] - prog[i];",
        "        if (dist < best_dist) { best_dist = dist; best = a; }",
        "    }",
        "    return best;",
        "}",
        "",
        "// Accès direct (mode=0/1, prog_arch=0-7, seed=quelconque)",
        "// Phrases triées poids décroissant => seed faible = plus populaire corpus",
        "inline const Phrase& get(int seed, int mode, int prog_arch) {",
        "    int ctx  = (mode & 1) * N_ARCHS + (prog_arch & (N_ARCHS - 1));",
        "    int base = ctx * PHRASES_PER_CTX;",
        "    int idx  = seed < 0 ? 0 : seed % PHRASES_PER_CTX;",
        "    return phrases[base + idx];",
        "}",
        "",
        "}  // namespace phrases",
        "",
    ]

    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text("\n".join(lines), encoding="utf-8")
    print(f"\n[OK] {OUT}")
    print(f"     {(OUT.stat().st_size / 1024):.1f} KB")


if __name__ == "__main__":
    main()
