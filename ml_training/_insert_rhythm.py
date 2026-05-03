"""One-shot script: insert rhythm functions into midi_pattern_learner.py."""
import pathlib

f = pathlib.Path(__file__).parent / "midi_pattern_learner.py"
lines = f.read_text(encoding="utf-8").splitlines(keepends=True)

# Locate the box-drawing comment line just before "# Originality Analysis"
# We find "# Originality Analysis" first (guaranteed unique), then step back 2
target_idx = None
for i, l in enumerate(lines):
    if l.strip() == "# Originality Analysis":
        target_idx = i  # 0-based
        break

assert target_idx is not None, "Could not find '# Originality Analysis'"
# Insert 2 lines before (the separator line + blank line before it)
insert_at = target_idx - 2   # before the box-drawing separator
print(f"Inserting at line {insert_at+1} (1-based)")

new_block = """\n# -- Rhythm Pattern Extraction --------------------------------------------------

_RHYTHM_SLOTS = 16   # 4 beats x 4 sixteenth notes per bar


def extract_rhythm_patterns(tracks, max_bars=16):
    \"\"\"Extract 16-slot binary rhythm grids (one per bar) from melody track.\"\"\"
    melody_tracks = [t for t in tracks if t.role == "melody"]
    if not melody_tracks:
        non_drum = [t for t in tracks if t.instrument["family"] != "drums"]
        if not non_drum:
            return []
        melody_tracks = [max(non_drum, key=lambda t: t.avg_pitch)]
    mel_notes = sorted(
        [n for t in melody_tracks for n in t.notes], key=lambda n: n.start
    )
    if not mel_notes:
        return []
    total_beats = max(n.start + n.duration for n in mel_notes)
    n_bars = min(int(total_beats / 4) + 1, max_bars)
    patterns = []
    for bar in range(n_bars):
        bar_start = float(bar * 4)
        grid = [0] * _RHYTHM_SLOTS
        bar_notes = [n for n in mel_notes if bar_start <= n.start < bar_start + 4.0]
        if not bar_notes:
            continue
        for n in bar_notes:
            slot = int((n.start - bar_start) / 0.25)
            if 0 <= slot < _RHYTHM_SLOTS:
                grid[slot] = 1
        if sum(grid) >= 2:
            patterns.append(tuple(grid))
    return patterns


def rhythm_as_uint16(pat):
    \"\"\"Pack a 16-slot pattern into a uint16 bitmask.\"\"\"
    val = 0
    for i, b in enumerate(pat[:16]):
        if b:
            val |= (1 << i)
    return val


def rhythm_grid_str(pat):
    \"\"\"ASCII grid: x=onset, .=silence.\"\"\"
    return "".join("x" if b else "." for b in pat[:16])


def describe_rhythm(pat):
    \"\"\"Short label for a 16-slot rhythm pattern.\"\"\"
    if len(pat) < 16:
        return "?"
    downbeats  = sum(pat[i] for i in (0, 4,  8, 12))
    offbeats   = sum(pat[i] for i in (2, 6, 10, 14))
    syncopated = sum(pat[i] for i in (1, 3, 5, 7, 9, 11, 13, 15))
    density    = sum(pat)
    if density >= 12:                        return "very dense"
    if syncopated >= 4:                      return "syncopated"
    if density >= 8:                         return "dense"
    if offbeats >= 3 and downbeats <= 2:     return "offbeat"
    if downbeats == 4 and syncopated == 0:   return "quarter notes"
    if downbeats >= 3 and offbeats >= 2:     return "shuffle"
    if syncopated >= 2:                      return "light syncopation"
    if offbeats >= 2 and density <= 6:       return "even eighths"
    return "mixed"


def rhythm_hamming(a, b):
    \"\"\"Normalised Hamming distance between two 16-slot patterns.\"\"\"
    n = min(len(a), len(b))
    return sum(1 for x, y in zip(a[:n], b[:n]) if x != y) / n if n else 1.0


def rhythm_originality_score(pat, corpus_counter, sample):
    \"\"\"Originality score [0,1]: 40% rarity + 20% density + 40% Hamming distance.\"\"\"
    total  = sum(corpus_counter.values())
    count  = corpus_counter.get(pat, 0)
    rarity = 1.0 - (count / total) if total > 0 else 1.0
    density_norm = max(0.0, (sum(pat) - 2) / 14)
    others   = [p for p in sample if p != pat]
    avg_dist = sum(rhythm_hamming(pat, o) for o in others) / len(others) if others else 0.5
    return 0.40 * rarity + 0.20 * density_norm + 0.40 * avg_dist


def kmeans_rhythms(patterns, k=8, n_iter=100, seed=42):
    \"\"\"K-means on 16-slot rhythm patterns; returns k representative centroids.\"\"\"
    import numpy as _np
    if not patterns:
        return []
    k    = min(k, len(patterns))
    rng  = _np.random.default_rng(seed)
    data = _np.array(patterns, dtype=float)
    idx  = [int(rng.integers(len(data)))]
    for _ in range(k - 1):
        dists = _np.min(
            [_np.sum((data - data[i]) ** 2, axis=1) for i in idx], axis=0
        )
        probs = dists / (dists.sum() + 1e-12)
        idx.append(int(rng.choice(len(data), p=probs)))
    centers = data[idx].copy()
    for _ in range(n_iter):
        dist_mat = _np.sqrt(_np.sum((data[:, None] - centers[None]) ** 2, axis=2))
        labels   = _np.argmin(dist_mat, axis=1)
        new_centers = _np.zeros_like(centers)
        for c in range(k):
            mask = labels == c
            new_centers[c] = data[mask].mean(axis=0) if mask.any() else centers[c]
        if _np.allclose(centers, new_centers, atol=1e-3):
            break
        centers = new_centers
    result, seen = [], set()
    for c in centers:
        t = tuple(int(round(float(v))) for v in c)
        if t not in seen and sum(t) >= 2:
            seen.add(t)
            result.append(t)
    return result


"""

lines_new = lines[:insert_at] + [new_block] + lines[insert_at:]
f.write_text("".join(lines_new), encoding="utf-8")
print(f"Done. File now has {len(lines_new)} logical entries (one is the big block).")

# Quick syntax check
import ast
ast.parse(f.read_text(encoding="utf-8"))
print("Syntax OK")
