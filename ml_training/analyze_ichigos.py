#!/usr/bin/env python3
# -*- coding: utf-8 -*-
from __future__ import annotations

import io
import sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")
sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding="utf-8", errors="replace")

"""
analyze_ichigos.py
==================
Analyse les MIDIs ichigos.com pour extraire des patterns de melodie et de
rythme separement pour la main droite (melodie) et la main gauche (accords).

Structure d'entree attendue :
    data/ichigos_midi/<Source>/<titre>_<id>.mid

Detection main gauche / droite :
  * Si 2+ pistes distinctes -> la plus aigue = main droite
  * Si piste unique -> separation par split-point MIDI
  * Heuristiques : polyphonie simultanee, tessiture, velocite

Sorties :
  data/patterns/ichigos_report.json   -- rapport machine-readable complet
  data/patterns/ichigos_patterns.h    -- header C++ (contours + rythmes RH/LH)
  data/patterns/ichigos_summary.txt   -- resume lisible

Usage :
  python analyze_ichigos.py
  python analyze_ichigos.py --midi-dir data/ichigos_midi --top 8
  python analyze_ichigos.py --source "Final Fantasy" --top 6
"""

import argparse
import json
import math
import re
from collections import Counter, defaultdict
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

try:
    import mido
except ImportError:
    sys.exit("Erreur: mido non installe. Lancez: pip install mido")

try:
    import numpy as np
except ImportError:
    sys.exit("Erreur: numpy non installe. Lancez: pip install numpy")

# -----------------------------------------------------------------------------
# Paths
# -----------------------------------------------------------------------------

ROOT_DIR    = Path(__file__).parent
DEFAULT_DIR = ROOT_DIR / "data" / "ichigos_midi"
OUT_DIR     = ROOT_DIR / "data" / "patterns"

# -----------------------------------------------------------------------------
# Data structures
# -----------------------------------------------------------------------------

@dataclass
class Note:
    pitch:    int
    start:    float   # en beats
    duration: float   # en beats
    velocity: int
    channel:  int
    program:  int


@dataclass
class Hand:
    """Represente une main (gauche ou droite) ou une piste fonctionnelle."""
    name:       str           # "right" | "left" | "unknown"
    notes:      list[Note]    = field(default_factory=list)
    avg_pitch:  float         = 0.0
    pitch_std:  float         = 0.0
    avg_poly:   float         = 0.0   # polyphonie moyenne (notes simultanees)
    is_chordal: bool          = False  # True si main gauche harmonique


# -----------------------------------------------------------------------------
# MIDI Parsing
# -----------------------------------------------------------------------------

def parse_midi(path: Path) -> tuple[list[list[Note]], float, int]:
    """
    Parse un fichier MIDI.
    Retourne (tracks_notes, bpm, ticks_per_beat).
    tracks_notes[i] = liste de Note pour la piste i.
    """
    try:
        mid = mido.MidiFile(str(path), clip=True)
    except Exception as e:
        return [], 120.0, 480

    ticks_per_beat = max(1, mid.ticks_per_beat or 480)
    tempo_us       = 500_000  # 120 BPM

    notes_by_track: dict[int, list[Note]]         = defaultdict(list)
    programs:       dict[tuple[int, int], int]     = {}
    active:         dict[tuple[int, int, int], tuple[int, int, int]] = {}
    # active: (track, ch, pitch) -> (velocity, abs_tick, program)

    for t_idx, track in enumerate(mid.tracks):
        abs_tick = 0
        for msg in track:
            abs_tick += msg.time

            if msg.type == "set_tempo":
                tempo_us = msg.tempo

            elif msg.type == "program_change":
                programs[(t_idx, msg.channel)] = msg.program

            elif msg.type == "note_on" and msg.velocity > 0:
                prog = programs.get((t_idx, msg.channel), 0)
                active[(t_idx, msg.channel, msg.note)] = (
                    msg.velocity, abs_tick, prog
                )

            elif msg.type in ("note_off", "note_on"):
                key3 = (t_idx, msg.channel, msg.note)
                if key3 in active:
                    vel, start_tick, prog = active.pop(key3)
                    dur_beats = (abs_tick - start_tick) / ticks_per_beat
                    if dur_beats >= 0.02:
                        notes_by_track[t_idx].append(Note(
                            pitch=msg.note,
                            start=start_tick / ticks_per_beat,
                            duration=dur_beats,
                            velocity=vel,
                            channel=msg.channel,
                            program=prog,
                        ))

        # Close any notes still open
        for (ti, ch, pitch), (vel, start_tick, prog) in list(active.items()):
            if ti != t_idx:
                continue
            dur_beats = (abs_tick - start_tick) / ticks_per_beat
            if dur_beats >= 0.02:
                notes_by_track[ti].append(Note(
                    pitch=pitch,
                    start=start_tick / ticks_per_beat,
                    duration=dur_beats,
                    velocity=vel, channel=ch, program=prog,
                ))
            active.pop((ti, ch, pitch), None)

    bpm = 60_000_000.0 / max(1, tempo_us)
    tracks = [sorted(notes_by_track[i], key=lambda n: n.start)
              for i in sorted(notes_by_track)]
    return tracks, round(bpm, 2), ticks_per_beat


# -----------------------------------------------------------------------------
# Main gauche / droite — Detection
# -----------------------------------------------------------------------------

def _avg_pitch(notes: list[Note]) -> float:
    if not notes:
        return 60.0
    return float(np.mean([n.pitch for n in notes]))


def _pitch_std(notes: list[Note]) -> float:
    if not notes:
        return 0.0
    return float(np.std([n.pitch for n in notes]))


def _avg_polyphony(notes: list[Note], resolution: float = 0.1) -> float:
    """Nombre moyen de notes simultanement actives (echantillonne)."""
    if not notes:
        return 0.0
    end = max(n.start + n.duration for n in notes)
    samples, t = 0, 0.0
    poly_sum = 0
    while t < end:
        active = sum(1 for n in notes if n.start <= t < n.start + n.duration)
        poly_sum += active
        samples  += 1
        t += resolution
    return poly_sum / max(1, samples)


def _split_point(all_notes: list[Note]) -> int:
    """
    Trouve la limite main gauche / droite quand tout est dans une seule piste.
    Methode : cherche le plus grand ecart de pitch entre notes simultanees.
    Retourne la valeur de pitch separatrice (notes >= split -> RH).
    """
    if not all_notes:
        return 60

    # Construction d'un histogramme de pitch pondere par duree
    hist = np.zeros(128)
    for n in all_notes:
        hist[n.pitch] += n.duration

    # Cherche la "vallee" dans la distribution (registre median vide)
    # On regarde dans la plage 40-80 (region typique du split piano)
    best_split   = 60
    best_gap_val = -1.0
    window       = 3

    for pivot in range(40, 81):
        below = hist[max(0, pivot - window): pivot].mean()
        above = hist[pivot: min(128, pivot + window)].mean()
        gap   = below + above  # cherche le minimum de densite (les deux cotes faibles)
        # On veut le creux : minimiser below + above pondere
        valley = 1.0 / (below + above + 1e-3)
        if valley > best_gap_val:
            best_gap_val = valley
            best_split   = pivot

    return best_split


def detect_hands(tracks_notes: list[list[Note]]) -> tuple[Hand, Hand]:
    """
    Detecte main droite (melodie, registre aigu) et main gauche (accords, graves).

    Strategies :
    1. 2+ pistes distinctes avec notes -> separer par tessiture
    2. 2 canaux MIDI dans la meme piste -> canal le plus aigu = RH
    3. Piste unique -> split-point automatique par vallee de densite
    """
    # Filtrer les pistes vides et les batteries (ch 9)
    nonempty = [
        [n for n in t if n.channel != 9]
        for t in tracks_notes
    ]
    nonempty = [t for t in nonempty if len(t) >= 4]

    if not nonempty:
        return Hand("right"), Hand("left")

    # -- Cas A : 2+ pistes distinctes ------------------------------------------
    if len(nonempty) >= 2:
        # Trier par tessiture moyenne decroissante
        ranked = sorted(nonempty, key=_avg_pitch, reverse=True)
        rh_notes = ranked[0]
        lh_notes = sorted([n for t in ranked[1:] for n in t],
                          key=lambda n: n.start)

        rh = Hand("right", rh_notes)
        lh = Hand("left",  lh_notes)

    # -- Cas B : piste unique avec plusieurs canaux -----------------------------
    elif len(nonempty) == 1:
        all_notes = nonempty[0]
        channels = defaultdict(list)
        for n in all_notes:
            channels[n.channel].append(n)

        if len(channels) >= 2:
            # Canal le plus aigu = RH
            ranked_ch = sorted(channels.values(),
                                key=_avg_pitch, reverse=True)
            rh_notes = ranked_ch[0]
            lh_notes = sorted([n for ch_notes in ranked_ch[1:] for n in ch_notes],
                              key=lambda n: n.start)
        else:
            # Tout dans un seul canal -> split-point automatique
            all_notes_sorted = sorted(all_notes, key=lambda n: n.start)
            split = _split_point(all_notes_sorted)
            rh_notes = [n for n in all_notes_sorted if n.pitch >= split]
            lh_notes = [n for n in all_notes_sorted if n.pitch <  split]

        rh = Hand("right", sorted(rh_notes, key=lambda n: n.start))
        lh = Hand("left",  sorted(lh_notes, key=lambda n: n.start))

    else:
        return Hand("right"), Hand("left")

    # Calculer les statistiques
    for hand in (rh, lh):
        hand.avg_pitch = _avg_pitch(hand.notes)
        hand.pitch_std = _pitch_std(hand.notes)
        hand.avg_poly  = _avg_polyphony(hand.notes)
        # Main gauche = polyphonie plus elevee (accords) ou tessiure grave
        hand.is_chordal = (hand.avg_poly > 1.5 or hand.avg_pitch < 55)

    # Sanity check : si la "RH" est plus grave que la "LH", permuter
    if rh.avg_pitch < lh.avg_pitch and lh.notes:
        rh, lh = lh, rh
        rh.name = "right"
        lh.name = "left"

    return rh, lh


# -----------------------------------------------------------------------------
# Detection de tonalite (Krumhansl-Schmuckler simplifie)
# -----------------------------------------------------------------------------

_KS_MAJOR = np.array([6.35, 2.23, 3.48, 2.33, 4.38, 4.09,
                       2.52, 5.19, 2.39, 3.66, 2.29, 2.88])
_KS_MINOR = np.array([6.33, 2.68, 3.52, 5.38, 2.60, 3.53,
                       2.54, 4.75, 3.98, 2.69, 3.34, 3.17])

_NOTE_NAMES = ["C","C#","D","D#","E","F","F#","G","G#","A","A#","B"]


def detect_key(all_notes: list[Note]) -> tuple[int, str]:
    """Retourne (root 0-11, mode 'major'/'minor') par correlation KS."""
    hist = np.zeros(12)
    for n in all_notes:
        if n.channel != 9:
            hist[n.pitch % 12] += n.duration
    total = hist.sum()
    if total == 0:
        return 0, "major"
    hist /= total

    best_score, best_root, best_mode = -np.inf, 0, "major"
    for root in range(12):
        for profile, mode in [(_KS_MAJOR, "major"), (_KS_MINOR, "minor")]:
            shifted = np.roll(profile, root) / profile.sum()
            corr = float(np.corrcoef(hist, shifted)[0, 1])
            if corr > best_score:
                best_score, best_root, best_mode = corr, root, mode
    return best_root, best_mode


# -----------------------------------------------------------------------------
# Extraction de patterns — Main droite (melodie)
# -----------------------------------------------------------------------------

_SEMI_TO_DEG = [0, 0, 1, 2, 2, 3, 4, 4, 5, 5, 6, 6]


def semitone_to_degree(semitones: int) -> int:
    return _SEMI_TO_DEG[semitones % 12]


def melody_pitch_at(notes: list[Note], beat: float,
                    window: float = 0.55) -> Optional[int]:
    """Note la plus haute active (ou a venir) au temps beat."""
    active   = [n for n in notes
                if n.start <= beat + 0.05 and n.start + n.duration > beat - 0.05]
    upcoming = [n for n in notes if 0 < n.start - beat < window]
    candidates = active + upcoming
    return max(n.pitch for n in candidates) if candidates else None


def chord_root_at(notes: list[Note], beat: float,
                  window_before: float = 0.1,
                  window_after: float  = 0.5) -> Optional[int]:
    """Note la plus basse active dans la fenetre = racine probable de l'accord."""
    active = [n for n in notes
              if n.start - window_before <= beat <= n.start + n.duration + window_after]
    return min(n.pitch for n in active) % 12 if active else None


def extract_rh_contours(rh: Hand, lh: Hand, key_root: int,
                         max_bars: int = 32) -> list[tuple[int, ...]]:
    """
    Contours 4 temps depuis la main droite.
    La racine de l'accord est lue sur la main gauche (ou tonique si absente).
    """
    mel_notes  = rh.notes
    harm_notes = lh.notes

    if not mel_notes:
        return []

    total_beats = max(n.start + n.duration for n in mel_notes)
    n_bars = min(int(total_beats / 4) + 1, max_bars)
    contours: list[tuple[int, ...]] = []

    for bar in range(n_bars):
        bar_start = float(bar * 4)
        degrees = []
        valid = True

        for beat_idx in range(4):
            beat_time = bar_start + beat_idx
            root = chord_root_at(harm_notes, beat_time) if harm_notes else key_root
            if root is None:
                root = key_root
            pitch = melody_pitch_at(mel_notes, beat_time)
            if pitch is None:
                valid = False
                break
            degrees.append(semitone_to_degree(pitch - root))

        if valid and len(degrees) == 4:
            contours.append(tuple(degrees))

    return contours


# -----------------------------------------------------------------------------
# Extraction de patterns — Main gauche (accords)
# -----------------------------------------------------------------------------

def extract_chord_progression(lh: Hand, key_root: int,
                               max_bars: int = 16) -> list[tuple[int, ...]]:
    """
    Extrait la progression d'accords par mesure (1 degre par mesure = racine).
    Retourne des tuples de 4 racines relatives (0-6) sur 4 mesures.
    Ex. (0, 3, 4, 0) = I - IV - V - I en degrés.
    """
    notes = lh.notes
    if not notes:
        return []

    total_dur = max(n.start + n.duration for n in notes)
    total_bars = min(int(total_dur / 4) + 1, max_bars)
    progressions: list[tuple[int, ...]] = []

    for bar_start in range(0, total_bars - 3, 1):
        roots = []
        valid = True
        for b in range(4):
            t = float((bar_start + b) * 4)
            active = [n for n in notes if n.start <= t + 0.25 and n.start + n.duration > t]
            if not active:
                valid = False
                break
            bass = min(n.pitch for n in active)
            root_abs = bass % 12
            root_deg = semitone_to_degree(root_abs - key_root)
            roots.append(root_deg)
        if valid:
            progressions.append(tuple(roots))

    return progressions


def extract_lh_chord_patterns(lh: Hand, key_root: int,
                               max_beats: int = 128) -> list[tuple[int, ...]]:
    """
    Voicings harmoniques de la main gauche : degres scalaires simultanes par temps.
    Retourne des tuples tries de degres (ex. (0, 2, 4) = accord de 1ere tierce).
    """
    notes = lh.notes
    if not notes:
        return []

    total_beats = min(max(n.start + n.duration for n in notes), max_beats)
    patterns: list[tuple[int, ...]] = []

    for beat in range(int(total_beats)):
        bt = float(beat)
        active = [n for n in notes
                  if n.start <= bt + 0.1 and n.start + n.duration > bt]
        if len(active) < 2:
            continue
        root = min(n.pitch for n in active) % 12
        degs = tuple(sorted(set(semitone_to_degree(n.pitch - root) for n in active)))
        if len(degs) >= 2:
            patterns.append(degs)

    return patterns


# -----------------------------------------------------------------------------
# Extraction de phrases mélodiques avec durée réelle
# -----------------------------------------------------------------------------

# 8 archétypes de progressions — couvrent ~90% du corpus
PROG_ARCHETYPES_8: list[tuple] = [
    (0, 0, 0, 0),   # 0: pédale tonique
    (0, 4, 0, 4),   # 1: I-V
    (0, 3, 4, 0),   # 2: I-IV-V-I
    (0, 5, 3, 4),   # 3: I-VI-IV-V (pop cadence)
    (0, 6, 5, 4),   # 4: descente basse
    (0, 0, 0, 4),   # 5: tonique → dominante
    (0, 5, 0, 4),   # 6: I-VI-I-V
    (5, 5, 5, 5),   # 7: pédale VI (relative mineure)
]

# Durées valides en 16èmes de note
_VALID_DURS_16 = [1, 2, 3, 4, 6, 8, 12, 16]


def prog_to_arch8(prog: tuple) -> int:
    """Mappe une progression 4-degrés vers l'archétype le plus proche (0-7)."""
    return min(range(8), key=lambda i: sum(
        abs(a - b) for a, b in zip(PROG_ARCHETYPES_8[i], prog)
    ))


def extract_rh_phrases(rh: Hand, key_root: int, max_bars: int = 32) -> list[list]:
    """
    Extrait des phrases mélodiques réelles (degree, dur_16ths) depuis la main droite.
    Segmente aux silences > 1 beat ou quand 8 notes sont atteintes.
    Retourne uniquement les phrases de 3-8 notes.

    dur_16ths = IOI (inter-onset interval), i.e. time from this note's onset to the
    next note's onset, quantized to 16ths. This is the value the plugin uses as
    spacing between note triggers. For the last note of a phrase, the note's
    own sounding duration is used.
    """
    notes = sorted(rh.notes, key=lambda n: n.start)
    if not notes:
        return []

    phrases: list[list] = []
    # current accumulates (degree, abs_onset_beats, note_duration_beats)
    current: list = []
    prev_end = 0.0

    def _flush(cur: list) -> list:
        """Convert (deg, onset, dur) triples to (deg, ioi_16ths) pairs."""
        result = []
        for i, (deg, onset, dur) in enumerate(cur):
            if i < len(cur) - 1:
                ioi_beats = cur[i + 1][1] - onset
            else:
                ioi_beats = dur  # last note: use its own duration
            dur_16 = max(1, min(16, round(ioi_beats * 4)))
            dur_snap = min(_VALID_DURS_16, key=lambda d: abs(d - dur_16))
            result.append([deg, dur_snap])
        return result

    for note in notes:
        if note.start >= max_bars * 4:
            break
        gap = note.start - prev_end

        # Coupe de phrase sur silence > 1 beat
        if gap > 1.0 and current:
            if 3 <= len(current) <= 8:
                phrases.append(_flush(current))
            current = []

        # Coupe de phrase à longueur max
        if len(current) >= 8:
            if len(current) >= 3:
                phrases.append(_flush(current))
            current = []

        deg = semitone_to_degree((note.pitch % 12) - key_root)
        current.append([deg, note.start, note.duration])
        prev_end = note.start + note.duration

    if 3 <= len(current) <= 8:
        phrases.append(_flush(current))

    return phrases


def detect_lh_style(lh: Hand) -> str:
    """
    Identifie le style d'accompagnement de la main gauche.
    Retourne : "alberti", "arpeggio", "block_chord", "bass_melody",
               "octave_bass", "single_note", "sparse"
    """
    notes = lh.notes
    if not notes:
        return "empty"

    total_dur = max(n.start + n.duration for n in notes)
    if total_dur < 1:
        return "sparse"

    # Polyphonie sur fenetres de 0.25 beat
    poly_counts = []
    t = 0.0
    while t < total_dur:
        cnt = sum(1 for n in notes
                  if n.start <= t < n.start + n.duration)
        poly_counts.append(cnt)
        t += 0.25

    avg_poly = np.mean(poly_counts) if poly_counts else 0

    # Detection Alberti : alternance basse-accord-5te-accord
    if len(notes) >= 6:
        pitches = [n.pitch for n in notes[:16]]
        if len(pitches) >= 4:
            # Pattern Alberti : note grave - aigus alternants
            low_hi_alt = all(
                (pitches[i] < pitches[i+1]) != (pitches[i+1] < pitches[i+2])
                for i in range(min(4, len(pitches) - 2))
            )
            if low_hi_alt and avg_poly < 1.5:
                return "alberti"

    if avg_poly >= 2.5:
        return "block_chord"
    if avg_poly >= 1.5:
        return "arpeggio"

    # Notes uniques graves -> basse seule
    if lh.avg_pitch < 48:
        return "octave_bass" if lh.pitch_std > 3 else "single_note"

    return "bass_melody"


# -----------------------------------------------------------------------------
# Patterns rythmiques (communs RH / LH)
# -----------------------------------------------------------------------------

_RHYTHM_SLOTS = 16   # 4 beats × 4 doubles-croches = une mesure


def extract_rhythm_patterns(notes: list[Note],
                             max_bars: int = 32) -> list[tuple[int, ...]]:
    """Grille binaire 16 slots par mesure (onset = 1, silence = 0)."""
    if not notes:
        return []
    total_beats = max(n.start + n.duration for n in notes)
    n_bars = min(int(total_beats / 4) + 1, max_bars)
    patterns = []
    for bar in range(n_bars):
        bar_start = float(bar * 4)
        grid = [0] * _RHYTHM_SLOTS
        bar_notes = [n for n in notes if bar_start <= n.start < bar_start + 4.0]
        if not bar_notes:
            continue
        for n in bar_notes:
            slot = int((n.start - bar_start) / 0.25)
            if 0 <= slot < _RHYTHM_SLOTS:
                grid[slot] = 1
        if sum(grid) >= 2:
            patterns.append(tuple(grid))
    return patterns


def rhythm_as_uint16(pat: tuple) -> int:
    val = 0
    for i, b in enumerate(pat[:16]):
        if b:
            val |= (1 << i)
    return val


def rhythm_grid_str(pat: tuple) -> str:
    return "".join("x" if b else "." for b in pat[:16])


def describe_rhythm(pat: tuple) -> str:
    if len(pat) < 16:
        return "?"
    downbeats  = sum(pat[i] for i in (0, 4,  8, 12))
    offbeats   = sum(pat[i] for i in (2, 6, 10, 14))
    syncopated = sum(pat[i] for i in (1, 3, 5, 7, 9, 11, 13, 15))
    density    = sum(pat)
    if density >= 12:                       return "very dense"
    if syncopated >= 4:                     return "syncopated"
    if density >= 8:                        return "dense"
    if offbeats >= 3 and downbeats <= 2:    return "offbeat"
    if downbeats == 4 and syncopated == 0:  return "quarter notes"
    if downbeats >= 3 and offbeats >= 2:    return "shuffle"
    if syncopated >= 2:                     return "light syncopation"
    if offbeats >= 2 and density <= 6:      return "even eighths"
    return "mixed"


def rhythm_hamming(a: tuple, b: tuple) -> float:
    n = min(len(a), len(b))
    return sum(1 for x, y in zip(a[:n], b[:n]) if x != y) / n if n else 1.0


# -----------------------------------------------------------------------------
# Originality scores
# -----------------------------------------------------------------------------

def contour_hamming(a: tuple, b: tuple) -> float:
    n = min(len(a), len(b))
    return sum(1 for x, y in zip(a[:n], b[:n]) if x != y) / n if n else 1.0


def originality_score(contour: tuple, corpus: Counter,
                       sample: list[tuple]) -> float:
    """40% rarete + 20% diversite interne + 40% distance Hamming."""
    total  = sum(corpus.values())
    rarity = 1.0 - (corpus.get(contour, 0) / total) if total > 0 else 1.0
    diversity = (len(set(contour)) - 1) / max(1, len(contour) - 1)
    others = [c for c in sample if c != contour]
    avg_dist = (sum(contour_hamming(contour, o) for o in others) / len(others)
                if others else 0.5)
    return 0.40 * rarity + 0.20 * diversity + 0.40 * avg_dist


def rhythm_originality(pat: tuple, corpus: Counter,
                        sample: list[tuple]) -> float:
    """40% rarete + 20% densite + 40% distance Hamming."""
    total   = sum(corpus.values())
    rarity  = 1.0 - (corpus.get(pat, 0) / total) if total > 0 else 1.0
    density = max(0.0, (sum(pat) - 2) / 14)
    others  = [p for p in sample if p != pat]
    avg_dist = (sum(rhythm_hamming(pat, o) for o in others) / len(others)
                if others else 0.5)
    return 0.40 * rarity + 0.20 * density + 0.40 * avg_dist


# -----------------------------------------------------------------------------
# K-Means (numpy seulement)
# -----------------------------------------------------------------------------

def _kmeans(data_tuples: list[tuple], k: int,
            n_iter: int = 100, seed: int = 42) -> list[tuple]:
    if not data_tuples:
        return []
    k    = min(k, len(data_tuples))
    rng  = np.random.default_rng(seed)
    data = np.array(data_tuples, dtype=float)

    # K-means++ init
    idx = [int(rng.integers(len(data)))]
    for _ in range(k - 1):
        dists = np.min([np.sum((data - data[i])**2, axis=1) for i in idx], axis=0)
        probs = dists / (dists.sum() + 1e-12)
        idx.append(int(rng.choice(len(data), p=probs)))

    centers = data[idx].copy()
    for _ in range(n_iter):
        dist_mat = np.sqrt(np.sum((data[:, None] - centers[None])**2, axis=2))
        labels   = np.argmin(dist_mat, axis=1)
        new_c    = np.zeros_like(centers)
        for c in range(k):
            mask = labels == c
            new_c[c] = data[mask].mean(axis=0) if mask.any() else centers[c]
        if np.allclose(centers, new_c, atol=1e-3):
            break
        centers = new_c

    result, seen = [], set()
    for c in centers:
        dim = data.shape[1]
        if dim == 4:   # contour
            t = tuple(int(np.clip(round(float(v)), 0, 6)) for v in c)
        else:           # rhythm
            t = tuple(int(round(float(v))) for v in c)
            if sum(t) < 2:
                continue
        if t not in seen:
            seen.add(t)
            result.append(t)
    return result


# -----------------------------------------------------------------------------
# Contour helpers
# -----------------------------------------------------------------------------

_DEGREE_NAMES = ["R", "2nd", "3rd", "4th", "5th", "6th", "7th"]


def describe_contour(c: tuple) -> str:
    if len(c) < 4:
        return "?"
    a, b, cc, d = c[0], c[1], c[2], c[3]
    if b >= a and cc >= b and d >= cc:  return "rise"
    if b <= a and cc <= b and d <= cc:  return "descent"
    if b > a  and cc >= b and d < cc:   return "arch"
    if b < a  and cc <= b and d > cc:   return "valley"
    if d > a  and b <= a:               return "zigzag-up"
    if d < a  and b >= a:               return "zigzag-down"
    return "wave"


# -----------------------------------------------------------------------------
# Analyse d'un fichier MIDI
# -----------------------------------------------------------------------------

def analyze_file(path: Path) -> Optional[dict]:
    """
    Analyse un fichier MIDI ichigos.
    Retourne un dict avec contours RH, patterns LH, rythmes, meta.
    """
    tracks_notes, bpm, tpb = parse_midi(path)
    all_notes = [n for t in tracks_notes for n in t if n.channel != 9]
    if not all_notes:
        return None

    key_root, key_mode = detect_key(all_notes)

    rh, lh = detect_hands(tracks_notes)

    rh_contours  = extract_rh_contours(rh, lh, key_root, max_bars=32)
    lh_chords    = extract_lh_chord_patterns(lh, key_root, max_beats=128)
    lh_progs     = extract_chord_progression(lh, key_root, max_bars=32)
    rh_rhythms   = extract_rhythm_patterns(rh.notes, max_bars=32)
    lh_rhythms   = extract_rhythm_patterns(lh.notes, max_bars=32)
    lh_style     = detect_lh_style(lh)
    rh_phrases   = extract_rh_phrases(rh, key_root, max_bars=32)
    # Archétype de progression dominant pour ce fichier
    if lh_progs:
        most_common_prog = Counter(lh_progs).most_common(1)[0][0]
        file_prog_arch = prog_to_arch8(most_common_prog)
    else:
        file_prog_arch = 0

    # Nom de la source = dossier parent
    source = path.parent.name
    title  = re.sub(r"_\d+$", "", path.stem)  # enleve le suffixe _id

    return {
        "path":         str(path),
        "source":       source,
        "title":        title,
        "bpm":          bpm,
        "key":          f"{_NOTE_NAMES[key_root]} {key_mode}",
        "key_root":     key_root,
        "key_mode":     key_mode,
        # Main droite
        "rh_notes":     len(rh.notes),
        "rh_avg_pitch": round(rh.avg_pitch, 1),
        "rh_contours":  rh_contours,
        "rh_rhythms":   rh_rhythms,
        # Main gauche
        "lh_notes":     len(lh.notes),
        "lh_avg_pitch": round(lh.avg_pitch, 1),
        "lh_avg_poly":  round(lh.avg_poly, 2),
        "lh_style":     lh_style,
        "lh_chords":    lh_chords,
        "lh_rhythms":   lh_rhythms,
        "lh_progs":     lh_progs,
        "rh_phrases":   rh_phrases,
        "prog_arch":    file_prog_arch,
    }


# -----------------------------------------------------------------------------
# C++ Header
# -----------------------------------------------------------------------------

def _safe_var(s: str, max_len: int = 40) -> str:
    v = re.sub(r"[^a-zA-Z0-9]", "_", s.lower())
    return re.sub(r"_+", "_", v).strip("_")[:max_len]


def generate_cpp_header(top_rh_contours: list[tuple],
                         top_rh_rhythms:  list[tuple],
                         top_lh_rhythms:  list[tuple],
                         lh_chord_counts: Counter,
                         source_samples:  dict[str, list[tuple]]) -> str:
    """Genere le header C++ avec patterns RH (contours + rythmes) et LH (accords + rythmes)."""
    lines = [
        "// ================================================================",
        "// ichigos_patterns.h — extrait par analyze_ichigos.py",
        "//              NE PAS ÉDITER — regenerer avec analyze_ichigos.py",
        "// ================================================================",
        "#pragma once",
        "#include <cstdint>",
        "#include <array>",
        "",
        "namespace ichigos_patterns {",
        "",
        "// Degres scalaires : 0=R, 1=2nd, 2=3rd, 3=4th, 4=5th, 5=6th, 6=7th",
        "",
    ]

    # -- Contours main droite ---------------------------------------------------
    n = min(8, len(top_rh_contours))
    lines += [
        "// -- Contours melodie (main droite) -------------------------------------",
        f"static constexpr int8_t rh_contours[{n}][4] = {{",
    ]
    for c in top_rh_contours[:n]:
        if len(c) < 4:
            continue
        deg_str  = ", ".join(f"{d:2d}" for d in c[:4])
        note_str = " -> ".join(_DEGREE_NAMES[min(d, 6)] for d in c[:4])
        shape    = describe_contour(c)
        lines.append(f"    {{ {deg_str} }},  // {shape}: {note_str}")
    lines += ["};", ""]

    # -- Rythmes main droite ----------------------------------------------------
    nr = min(8, len(top_rh_rhythms))
    if top_rh_rhythms:
        lines += [
            "// -- Rythmes melodie (main droite) --------------------------------------",
            f"static constexpr uint16_t rh_rhythms[{nr}] = {{",
        ]
        for r in top_rh_rhythms[:nr]:
            val   = rhythm_as_uint16(r)
            grid  = rhythm_grid_str(r)
            shape = describe_rhythm(r)
            lines.append(f"    0x{val:04X},  // {grid}  {shape}")
        lines += ["};", ""]

    # -- Rythmes main gauche ----------------------------------------------------
    nl = min(8, len(top_lh_rhythms))
    if top_lh_rhythms:
        lines += [
            "// -- Rythmes accompagnement (main gauche) -------------------------------",
            f"static constexpr uint16_t lh_rhythms[{nl}] = {{",
        ]
        for r in top_lh_rhythms[:nl]:
            val   = rhythm_as_uint16(r)
            grid  = rhythm_grid_str(r)
            shape = describe_rhythm(r)
            lines.append(f"    0x{val:04X},  // {grid}  {shape}")
        lines += ["};", ""]

    # -- Top voicings main gauche -----------------------------------------------
    top_chords = lh_chord_counts.most_common(12)
    if top_chords:
        lines += [
            "// -- Voicings accords main gauche (top 12) ------------------------------",
            "// Format : degres simultanes (0=racine, 2=3ce, 4=5te...)",
            "// Compte = nb d'occurrences dans le corpus",
        ]
        for chord, count in top_chords:
            degs = ", ".join(str(d) for d in chord)
            names = "+".join(_DEGREE_NAMES[min(d, 6)] for d in chord)
            lines.append(f"// {{{degs}}}  // {names}  (×{count})")
        lines.append("")

    # -- Patterns par source ----------------------------------------------------
    if source_samples:
        lines.append("// -- Contours par source ------------------------------------------------")
        lines.append("")
    for source, contours in sorted(source_samples.items()):
        if not contours:
            continue
        var = _safe_var(source)
        n_c = min(4, len(contours))
        lines += [
            f"// {source}",
            f"static constexpr int8_t {var}_contours[{n_c}][4] = {{",
        ]
        for c in contours[:n_c]:
            deg_str = ", ".join(f"{d:2d}" for d in c[:4])
            shape   = describe_contour(c)
            lines.append(f"    {{ {deg_str} }},  // {shape}")
        lines += ["};", ""]

    lines += ["", "} // namespace ichigos_patterns", ""]
    return "\n".join(lines)


# -----------------------------------------------------------------------------
# Main
# -----------------------------------------------------------------------------

def run(midi_dir: Path, source_filter: str, top_k: int,
        max_bars: int, verbose: bool) -> None:
    midi_files = sorted(midi_dir.rglob("*.mid"))
    if not midi_files:
        midi_files = sorted(midi_dir.rglob("*.MID"))
    if not midi_files:
        print(f"Aucun fichier MIDI dans {midi_dir}")
        return

    # Filtre optionnel par source
    filters = [f.strip().lower() for f in source_filter.split(",") if f.strip()] \
              if source_filter else []

    if filters:
        midi_files = [
            f for f in midi_files
            if any(kw in f.parent.name.lower() for kw in filters)
        ]

    print(f"\n{'='*64}")
    print(f"  Analyse ichigos — {len(midi_files)} fichiers MIDI")
    if filters:
        print(f"  Filtre : {', '.join(filters)}")
    print(f"{'='*64}\n")

    # -- Phase d'analyse --------------------------------------------------------
    results: list[dict]    = []
    errors: list[str]      = []

    # Accumulateurs corpus
    all_rh_contours: list[tuple] = []
    all_rh_rhythms:  list[tuple] = []
    all_lh_rhythms:  list[tuple] = []
    all_lh_chords:   list[tuple] = []
    all_lh_progs:    list[tuple] = []
    # Mode-split : meme donnees separees par mode
    all_rh_contours_major: list[tuple] = []
    all_rh_contours_minor: list[tuple] = []
    all_lh_progs_major:    list[tuple] = []
    all_lh_progs_minor:    list[tuple] = []
    phrase_corpus: dict[str, list] = defaultdict(list)  # key="{mode_id}_{arch}"
    source_contours: dict[str, list[tuple]] = defaultdict(list)
    lh_styles_count: Counter = Counter()

    for i, path in enumerate(midi_files, 1):
        rel = path.relative_to(midi_dir)
        print(f"  [{i:4d}/{len(midi_files)}]  {str(rel)[:60]:<60}", end="  ")

        try:
            r = analyze_file(path)
        except Exception as e:
            errors.append(f"{path}: {e}")
            print(f"ERR: {e}")
            continue

        if r is None:
            print("vide")
            continue

        results.append(r)
        all_rh_contours.extend(r["rh_contours"])
        all_rh_rhythms.extend(r["rh_rhythms"])
        all_lh_rhythms.extend(r["lh_rhythms"])
        all_lh_chords.extend(r["lh_chords"])
        all_lh_progs.extend(r["lh_progs"])
        # Mode-split
        if r["key_mode"] == "major":
            all_rh_contours_major.extend(r["rh_contours"])
            all_lh_progs_major.extend(r["lh_progs"])
        else:
            all_rh_contours_minor.extend(r["rh_contours"])
            all_lh_progs_minor.extend(r["lh_progs"])
        # Corpus de phrases par (mode, arch)
        mode_id = 0 if r["key_mode"] == "major" else 1
        ctx_key = f"{mode_id}_{r.get('prog_arch', 0)}"
        phrase_corpus[ctx_key].extend(r.get("rh_phrases", []))
        source_contours[r["source"]].extend(r["rh_contours"])
        lh_styles_count[r["lh_style"]] += 1

        status = (f"RH:{len(r['rh_contours'])}cnt  "
                  f"LH:{r['lh_style']}/{len(r['lh_chords'])}chd  "
                  f"{r['key']}")
        print(status)

    if not results:
        print("Aucun resultat.")
        return

    print(f"\n  Analyses   : {len(results)}/{len(midi_files)}")
    print(f"  Erreurs    : {len(errors)}")

    # -- Comptages --------------------------------------------------------------
    rh_contour_counter        = Counter(all_rh_contours)
    rh_rhythm_counter         = Counter(all_rh_rhythms)
    lh_rhythm_counter         = Counter(all_lh_rhythms)
    lh_chord_counter          = Counter(all_lh_chords)
    lh_prog_counter           = Counter(all_lh_progs)
    rh_contour_counter_major  = Counter(all_rh_contours_major)
    rh_contour_counter_minor  = Counter(all_rh_contours_minor)
    lh_prog_counter_major     = Counter(all_lh_progs_major)
    lh_prog_counter_minor     = Counter(all_lh_progs_minor)

    unique_rh_c = len(rh_contour_counter)
    unique_rh_r = len(rh_rhythm_counter)
    unique_lh_r = len(lh_rhythm_counter)

    print(f"\n  Contours RH  : {len(all_rh_contours)} total, {unique_rh_c} uniques")
    print(f"  Rythmes RH   : {len(all_rh_rhythms)} total, {unique_rh_r} uniques")
    print(f"  Rythmes LH   : {len(all_lh_rhythms)} total, {unique_lh_r} uniques")
    print(f"  Accords LH   : {len(all_lh_chords)} total, {len(lh_chord_counter)} uniques")
    print(f"  Prog. accords: {len(all_lh_progs)} total, {len(lh_prog_counter)} uniques")

    print(f"\n  Styles main gauche :")
    for style, cnt in lh_styles_count.most_common():
        bar = "#" * int(40 * cnt / max(1, len(results)))
        print(f"    {style:<18} {bar}  ({cnt})")

    # -- K-means ----------------------------------------------------------------
    print(f"\n  K-means contours RH (k={top_k})…")
    top_rh_contours = _kmeans(all_rh_contours, k=top_k)

    print(f"  K-means rythmes RH (k={top_k})…")
    top_rh_rhythms  = _kmeans(all_rh_rhythms, k=top_k)

    print(f"  K-means rythmes LH (k={top_k})…")
    top_lh_rhythms  = _kmeans(all_lh_rhythms, k=top_k)

    # -- Scores d'originalite ---------------------------------------------------
    sample_c = list(rh_contour_counter.keys())[:200]
    sample_r = list(rh_rhythm_counter.keys())[:200]

    # Enrichir les resultats avec scores
    for r in results:
        if r["rh_contours"]:
            scores = [originality_score(c, rh_contour_counter, sample_c)
                      for c in r["rh_contours"]]
            r["rh_originality"] = round(max(scores), 4)
            r["rh_best_contour"] = r["rh_contours"][scores.index(max(scores))]
        else:
            r["rh_originality"]  = 0.0
            r["rh_best_contour"] = None

    # -- Affichage des top contours ---------------------------------------------
    print(f"\n{'-'*64}")
    print(f"  TOP {len(top_rh_contours)} CONTOURS MÉLODIE (main droite)")
    print(f"{'-'*64}")
    for c in top_rh_contours:
        count = rh_contour_counter.get(c, 0)
        shape = describe_contour(c)
        degs  = " ".join(f"{d}" for d in c)
        print(f"  {degs}  [{shape}]  ×{count}")

    print(f"\n{'-'*64}")
    print(f"  TOP {len(top_rh_rhythms)} RYTHMES (main droite)")
    print(f"{'-'*64}")
    for r in top_rh_rhythms:
        val   = rhythm_as_uint16(r)
        grid  = rhythm_grid_str(r)
        shape = describe_rhythm(r)
        count = rh_rhythm_counter.get(r, 0)
        print(f"  0x{val:04X}  {grid}  {shape}  ×{count}")

    print(f"\n{'-'*64}")
    print(f"  TOP {len(top_lh_rhythms)} RYTHMES (main gauche)")
    print(f"{'-'*64}")
    for r in top_lh_rhythms:
        val   = rhythm_as_uint16(r)
        grid  = rhythm_grid_str(r)
        shape = describe_rhythm(r)
        count = lh_rhythm_counter.get(r, 0)
        print(f"  0x{val:04X}  {grid}  {shape}  ×{count}")

    print(f"\n{'-'*64}")
    print(f"  TOP VOICINGS ACCORDS (main gauche)")
    print(f"{'-'*64}")
    for chord, count in lh_chord_counter.most_common(10):
        names = "+".join(_DEGREE_NAMES[min(d, 6)] for d in chord)
        print(f"  {names:<24}  ×{count}")

    print(f"\n{'-'*64}")
    print(f"  TOP 10 ORIGINALITÉ MÉLODIE (main droite)")
    print(f"{'-'*64}")
    top_orig = sorted(results,
                      key=lambda r: r.get("rh_originality", 0), reverse=True)[:10]
    for r in top_orig:
        bc = r.get("rh_best_contour")
        bc_str = " ".join(str(d) for d in bc) if bc else "—"
        print(f"  {r['source'][:28]:<28} / {r['title'][:28]:<28}"
              f"  orig={r['rh_originality']:.3f}  [{bc_str}]")

    # -- Sorties ----------------------------------------------------------------
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    # Selection de contours representatifs par source (top 4 par originalite)
    source_samples: dict[str, list[tuple]] = {}
    for source in sorted(source_contours):
        cnts = source_contours[source]
        if not cnts:
            continue
        uniq = list(dict.fromkeys(cnts))  # dedupliquer en gardant ordre
        scored = sorted(uniq,
                        key=lambda c: originality_score(c, rh_contour_counter, sample_c),
                        reverse=True)
        source_samples[source] = scored[:4]

    # JSON
    report = {
        "n_files":          len(results),
        "n_errors":         len(errors),
        "lh_styles":        dict(lh_styles_count),
        "top_rh_contours":  [list(c) for c in top_rh_contours],
        "top_rh_rhythms":   [{"grid": rhythm_grid_str(r),
                              "uint16": f"0x{rhythm_as_uint16(r):04X}",
                              "shape": describe_rhythm(r)}
                             for r in top_rh_rhythms],
        "top_lh_rhythms":   [{"grid": rhythm_grid_str(r),
                              "uint16": f"0x{rhythm_as_uint16(r):04X}",
                              "shape": describe_rhythm(r)}
                             for r in top_lh_rhythms],
        "top_lh_chords":    [list(c) for c, _ in lh_chord_counter.most_common(20)],
        "all_rh_contour_counts": [[list(c), n] for c, n in rh_contour_counter.most_common()],
        "all_rh_rhythm_counts": [[list(r), n] for r, n in rh_rhythm_counter.most_common()],
        "all_lh_rhythm_counts": [[list(r), n] for r, n in lh_rhythm_counter.most_common()],
        "all_lh_chord_counts":  [[list(c), n] for c, n in lh_chord_counter.most_common()],
        "all_lh_prog_counts":   [[list(p), n] for p, n in lh_prog_counter.most_common()],
        # Mode-split
        "all_rh_contour_counts_major": [[list(c), n] for c, n in rh_contour_counter_major.most_common()],
        "all_rh_contour_counts_minor": [[list(c), n] for c, n in rh_contour_counter_minor.most_common()],
        "all_lh_prog_counts_major":    [[list(p), n] for p, n in lh_prog_counter_major.most_common()],
        "all_lh_prog_counts_minor":    [[list(p), n] for p, n in lh_prog_counter_minor.most_common()],
        # Corpus de phrases par contexte (mode_arch) — max 300 par contexte
        "phrase_corpus": {
            k: v[:300] for k, v in phrase_corpus.items()
        },
        "files":            [
            {k: v for k, v in r.items()
             if k not in ("rh_contours", "lh_chords", "rh_rhythms", "lh_rhythms", "lh_progs", "rh_phrases")}
            for r in results
        ],
        "errors": errors,
    }

    json_path = OUT_DIR / "ichigos_report.json"
    json_path.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
    print(f"\n  Rapport JSON     : {json_path}")

    # C++ header
    cpp = generate_cpp_header(top_rh_contours, top_rh_rhythms, top_lh_rhythms,
                               lh_chord_counter, source_samples)
    h_path = OUT_DIR / "ichigos_patterns.h"
    h_path.write_text(cpp, encoding="utf-8")
    print(f"  Header C++       : {h_path}")

    # Resume texte
    summary_lines = [
        "ICHIGOS PATTERN ANALYSIS SUMMARY",
        "=" * 60,
        f"Fichiers analyses : {len(results)}",
        f"Contours RH uniques : {unique_rh_c}",
        f"Rythmes RH uniques  : {unique_rh_r}",
        f"Rythmes LH uniques  : {unique_lh_r}",
        f"Voicings LH uniques : {len(lh_chord_counter)}",
        "",
        "Styles main gauche :",
    ]
    for style, cnt in lh_styles_count.most_common():
        summary_lines.append(f"  {style:<18} {cnt}")

    summary_lines += ["", "Top contours RH :"]
    for c in top_rh_contours:
        summary_lines.append(
            f"  {' '.join(str(d) for d in c)}  [{describe_contour(c)}]"
            f"  ×{rh_contour_counter.get(c, 0)}"
        )
    summary_lines += ["", "Top rythmes RH :"]
    for r in top_rh_rhythms:
        summary_lines.append(
            f"  0x{rhythm_as_uint16(r):04X}  {rhythm_grid_str(r)}"
            f"  {describe_rhythm(r)}  ×{rh_rhythm_counter.get(r, 0)}"
        )
    summary_lines += ["", "Top rythmes LH :"]
    for r in top_lh_rhythms:
        summary_lines.append(
            f"  0x{rhythm_as_uint16(r):04X}  {rhythm_grid_str(r)}"
            f"  {describe_rhythm(r)}  ×{lh_rhythm_counter.get(r, 0)}"
        )
    summary_lines += ["", "Top voicings LH :"]
    for chord, count in lh_chord_counter.most_common(10):
        names = "+".join(_DEGREE_NAMES[min(d, 6)] for d in chord)
        summary_lines.append(f"  {names:<24}  ×{count}")

    txt_path = OUT_DIR / "ichigos_summary.txt"
    txt_path.write_text("\n".join(summary_lines), encoding="utf-8")
    print(f"  Resume texte     : {txt_path}")
    print()


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Analyse MIDIs ichigos.com — patterns melodie + accords + rythme"
    )
    parser.add_argument("--midi-dir", default=str(DEFAULT_DIR),
                        help=f"Dossier racine des MIDIs (defaut: {DEFAULT_DIR})")
    parser.add_argument("--source",   default="",
                        help="Filtre par source, ex: 'final fantasy,zelda'")
    parser.add_argument("--top",      type=int, default=8,
                        help="Nombre de patterns top (K pour K-means, defaut: 8)")
    parser.add_argument("--bars",     type=int, default=32,
                        help="Nombre max de mesures a analyser par fichier (defaut: 32)")
    parser.add_argument("--verbose",  action="store_true",
                        help="Affichage detaille")
    args = parser.parse_args()

    run(
        midi_dir=Path(args.midi_dir),
        source_filter=args.source,
        top_k=args.top,
        max_bars=args.bars,
        verbose=args.verbose,
    )


if __name__ == "__main__":
    main()
