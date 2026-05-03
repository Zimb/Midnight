#!/usr/bin/env python3
"""
midi_pattern_learner.py
=======================
Mini-module d'analyse et d'apprentissage de patterns MIDI pour Midnight Melody Maker.

Extrait depuis les MIDIs VGM (Final Fantasy, Zelda, Chrono Trigger, etc.) :
  â€¢ Patterns main droite  : contours 4-temps en degrÃ©s scalaires (format plugin)
  â€¢ Patterns main gauche  : voicings harmoniques + rythme d'accords
  â€¢ Classification instrument : piano, harpe, guitare, cordes, bois, etc.
  â€¢ Score d'originalitÃ©   : raretÃ© corpus + entropie + distance inter-patterns
  â€¢ K-means clustering    : trouve les k contours les plus reprÃ©sentatifs

Sorties :
  data/patterns/report.json   â€” rapport dÃ©taillÃ© par source (machine-readable)
  data/patterns/patterns.h    â€” header C++ prÃªt pour plugin_vst3.cpp
  data/patterns/summary.txt   â€” rÃ©sumÃ© humain lisible

Usage :
  python midi_pattern_learner.py
  python midi_pattern_learner.py --focus "Final Fantasy,Zelda" --top 6
  python midi_pattern_learner.py --midi-dir path/to/midis --bars 32
"""

from __future__ import annotations

import argparse
import json
import math
import re
import sys
from collections import Counter, defaultdict
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

try:
    import mido
except ImportError:
    sys.exit("Erreur: mido non installÃ©. Lancez: pip install mido")

try:
    import numpy as np
except ImportError:
    sys.exit("Erreur: numpy non installÃ©. Lancez: pip install numpy")

# â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
# Paths & defaults
# â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

ROOT_DIR = Path(__file__).parent
DATA_DIR = ROOT_DIR / "data" / "vgmidi_raw" / "labelled" / "midi"
OUT_DIR  = ROOT_DIR / "data" / "patterns"

# â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
# GM Instrument Classification
# â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

# Individual programs that deserve a specific label (0-based GM)
_GM_SPECIAL: dict[int, tuple[str, str]] = {
    0:  ("piano",   "Piano Ã  queue"),
    1:  ("piano",   "Piano acoustique"),
    4:  ("piano",   "Piano Ã©lectrique 1"),
    5:  ("piano",   "Piano Ã©lectrique 2"),
    6:  ("piano",   "Clavecin"),
    11: ("chroma",  "Vibraphone"),
    12: ("chroma",  "Marimba"),
    13: ("chroma",  "Xylophone"),
    14: ("chroma",  "Carillon"),
    24: ("guitar",  "Guitare acoustique (nylon)"),
    25: ("guitar",  "Guitare acoustique (acier)"),
    26: ("guitar",  "Guitare Ã©lectrique (jazz)"),
    27: ("guitar",  "Guitare Ã©lectrique (clean)"),
    29: ("guitar",  "Guitare saturÃ©e"),
    40: ("strings", "Violon"),
    41: ("strings", "Alto"),
    42: ("strings", "Violoncelle"),
    45: ("strings", "Pizzicato"),
    46: ("harp",    "Harpe"),           # â† Orchestral Harp
    47: ("brass",   "Timbales"),
    52: ("ensemble","Voix (Aahs)"),
    56: ("brass",   "Trompette"),
    57: ("brass",   "Trombone"),
    60: ("brass",   "Cor d'harmonie"),
    68: ("reed",    "Hautbois"),
    69: ("reed",    "Cor anglais"),
    71: ("reed",    "Clarinette"),
    73: ("pipe",    "FlÃ»te traversiÃ¨re"),
    74: ("pipe",    "FlÃ»te de roseau"),
}

# Fallback ranges (lo inclusive, hi exclusive)
_GM_RANGES: list[tuple[int, int, str, str]] = [
    (0,   8,  "piano",       "Piano"),
    (8,  16,  "chroma",      "Percussions accordÃ©es"),
    (16,  24, "organ",       "Orgue"),
    (24,  32, "guitar",      "Guitare"),
    (32,  40, "bass",        "Basse"),
    (40,  48, "strings",     "Cordes"),
    (48,  56, "ensemble",    "Ensemble cordes"),
    (56,  64, "brass",       "Cuivres"),
    (64,  72, "reed",        "Bois (anche)"),
    (72,  80, "pipe",        "Bois (flÃ»te)"),
    (80,  88, "synth_lead",  "Synth Lead"),
    (88,  96, "synth_pad",   "Synth Pad"),
    (96, 104, "synth_fx",    "Synth FX"),
    (104, 112, "ethnic",     "Instruments ethniques"),
    (112, 120, "percussive", "Percussif"),
    (120, 128, "sfx",        "Effets"),
]

# Keywords in track names for instrument detection (when program is ambiguous)
_TRACK_NAME_KEYWORDS: list[tuple[list[str], str, str]] = [
    (["harp", "harpe", "koto", "lyre"],        "harp",    "Harpe"),
    (["guitar", "guitare", "guit", "git"],     "guitar",  "Guitare"),
    (["piano", "pno", "clav"],                 "piano",   "Piano"),
    (["violin", "viola", "cello", "string",
      "violon", "alto", "corde"],              "strings", "Cordes"),
    (["flute", "flÃ»te", "piccolo", "fl."],     "pipe",    "FlÃ»te"),
    (["oboe", "hautbois", "bassoon", "clarinet",
      "clari", "basson"],                      "reed",    "Bois"),
    (["trumpet", "trompette", "horn", "brass",
      "trombone", "cuivre"],                   "brass",   "Cuivres"),
    (["bass", "basse", "contrebasse"],         "bass",    "Basse"),
    (["drum", "perc", "batterie", "kit",
      "snare", "kick"],                        "drums",   "Batterie"),
    (["synth", "lead", "pad", "choir"],        "synth_lead", "Synth"),
]


def classify_instrument(program: int, channel: int,
                        track_name: str = "") -> dict:
    """Retourne {family, label, confidence} pour un programme GM (0-based)."""
    if channel == 9:
        return {"family": "drums", "label": "Batterie", "confidence": 1.0}

    prog = max(0, min(127, program))

    # 1) Named specials first (most precise)
    if prog in _GM_SPECIAL:
        fam, lbl = _GM_SPECIAL[prog]
        return {"family": fam, "label": lbl, "confidence": 1.0}

    # 2) Fallback ranges
    for lo, hi, fam, lbl in _GM_RANGES:
        if lo <= prog < hi:
            return {"family": fam, "label": lbl, "confidence": 0.85}

    # 3) Track name heuristics (lowest confidence)
    tn = track_name.lower()
    for keywords, fam, lbl in _TRACK_NAME_KEYWORDS:
        if any(kw in tn for kw in keywords):
            return {"family": fam, "label": lbl, "confidence": 0.6}

    return {"family": "unknown", "label": f"Inconnu (prog {prog})", "confidence": 0.2}


# â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
# Data structures
# â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

@dataclass
class Note:
    pitch:    int    # MIDI pitch 0-127
    start:    float  # time in beats (1 beat = 1 crotchet)
    duration: float  # duration in beats
    velocity: int    # 0-127
    channel:  int    # MIDI channel 0-15
    program:  int    # GM program at time of note (0-127)


@dataclass
class TrackInfo:
    name:       str
    channel:    int
    program:    int
    notes:      list[Note] = field(default_factory=list)
    instrument: dict       = field(default_factory=dict)
    avg_pitch:  float      = 0.0
    pitch_std:  float      = 0.0
    role:       str        = ""   # "melody" | "harmony" | "bass" | "drums"


# â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
# MIDI Parsing
# â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

def parse_midi(path: Path) -> tuple[list[TrackInfo], float]:
    """
    Parse un fichier MIDI standard (type 0/1/2).
    Retourne (list[TrackInfo], tempo_bpm).  Les temps sont en beats.
    """
    try:
        mid = mido.MidiFile(str(path), clip=True)
    except Exception:
        return [], 120.0

    ticks_per_beat = max(1, mid.ticks_per_beat or 480)
    tempo_us       = 500_000  # 120 BPM default

    # We'll collect notes per (track_idx, channel) pair so each combo
    # stays independent even in type-0 (single-track) files.
    notes_by_key: dict[tuple[int, int], list[Note]]  = defaultdict(list)
    programs:     dict[tuple[int, int], int]          = {}  # (track, ch) â†’ program
    track_names:  dict[int, str]                      = {}
    active:       dict[tuple[int, int, int], tuple[int, int, int]] = {}
    # active key: (track_idx, ch, pitch) â†’ (velocity, abs_tick, program)

    for t_idx, track in enumerate(mid.tracks):
        track_names[t_idx] = track.name or f"Track {t_idx}"
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
                # note_on with vel=0 == note_off
                key3 = (t_idx, msg.channel, msg.note)
                if key3 in active:
                    vel, start_tick, prog = active.pop(key3)
                    dur_beats = (abs_tick - start_tick) / ticks_per_beat
                    if dur_beats >= 0.01:
                        notes_by_key[(t_idx, msg.channel)].append(Note(
                            pitch=msg.note,
                            start=start_tick / ticks_per_beat,
                            duration=dur_beats,
                            velocity=vel,
                            channel=msg.channel,
                            program=prog,
                        ))

        # Close any notes still open at end of track
        for (ti, ch, note), (vel, start_tick, prog) in list(active.items()):
            if ti != t_idx:
                continue
            dur_beats = (abs_tick - start_tick) / ticks_per_beat
            if dur_beats >= 0.01:
                notes_by_key[(ti, ch)].append(Note(
                    pitch=note,
                    start=start_tick / ticks_per_beat,
                    duration=dur_beats,
                    velocity=vel,
                    channel=ch,
                    program=prog,
                ))
            active.pop((ti, ch, note), None)

    # Build TrackInfo per (track_idx, channel)
    tracks: list[TrackInfo] = []
    for (t_idx, ch), notes in notes_by_key.items():
        if not notes:
            continue
        prog = programs.get((t_idx, ch), 0)
        name = track_names.get(t_idx, f"Track {t_idx}")
        instr = classify_instrument(prog, ch, name)
        pitches = [n.pitch for n in notes if ch != 9]
        avg_p  = float(np.mean(pitches)) if pitches else 60.0
        std_p  = float(np.std(pitches))  if pitches else 0.0
        tracks.append(TrackInfo(
            name=name, channel=ch, program=prog,
            notes=sorted(notes, key=lambda n: n.start),
            instrument=instr, avg_pitch=avg_p, pitch_std=std_p,
        ))

    bpm = 60_000_000.0 / max(1, tempo_us)
    return tracks, round(bpm, 2)


def assign_roles(tracks: list[TrackInfo]) -> None:
    """
    Attribue un rÃ´le musical Ã  chaque piste:
      melody  â€” registre le plus aigu (main droite)
      harmony â€” voix intermÃ©diaires (accords)
      bass    â€” registre le plus grave (main gauche basse)
      drums   â€” canal 9 ou instrument percussion
    """
    drum_tracks = [t for t in tracks if t.instrument["family"] == "drums"]
    non_drum    = [t for t in tracks if t.instrument["family"] != "drums"]

    for t in drum_tracks:
        t.role = "drums"

    if not non_drum:
        return

    # Filter out tracks with very few notes (likely markers/metadata)
    active = [t for t in non_drum if len(t.notes) >= 4]
    if not active:
        active = non_drum

    # Sort by average pitch descending
    active.sort(key=lambda t: t.avg_pitch, reverse=True)

    active[0].role = "melody"

    if len(active) >= 2:
        # Tracks with avg_pitch clearly in bass range â†’ bass role
        for t in active[1:]:
            if t.avg_pitch < 50:   # below D3
                t.role = "bass"
            else:
                t.role = "harmony"
    # If only one non-drum track, it plays both melody and harmony
    if len(active) == 1:
        active[0].role = "melody"


# â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
# Key Detection  (simplified Krumhansl-Schmuckler)
# â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

_KS_MAJOR = np.array([6.35, 2.23, 3.48, 2.33, 4.38, 4.09,
                       2.52, 5.19, 2.39, 3.66, 2.29, 2.88])
_KS_MINOR = np.array([6.33, 2.68, 3.52, 5.38, 2.60, 3.53,
                       2.54, 4.75, 3.98, 2.69, 3.34, 3.17])


def detect_key(tracks: list[TrackInfo]) -> tuple[int, str]:
    """
    Retourne (root_pitch_class 0-11, mode 'major'/'minor').
    Utilise un histogramme de classes de hauteur pondÃ©rÃ© par la durÃ©e,
    corrÃ©lÃ© avec les profils de Krumhansl-Schmuckler.
    """
    histogram = np.zeros(12)
    for t in tracks:
        if t.instrument["family"] == "drums":
            continue
        for n in t.notes:
            histogram[n.pitch % 12] += n.duration

    total = histogram.sum()
    if total == 0:
        return 0, "major"
    histogram /= total

    best_score = -np.inf
    best_root  = 0
    best_mode  = "major"

    for root in range(12):
        for profile, mode in [(_KS_MAJOR, "major"), (_KS_MINOR, "minor")]:
            shifted = np.roll(profile, root)
            shifted /= shifted.sum()
            corr = float(np.corrcoef(histogram, shifted)[0, 1])
            if corr > best_score:
                best_score = corr
                best_root  = root
                best_mode  = mode

    return best_root, best_mode


# â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
# Contour Extraction
# â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

# Semitone offset from chord root â†’ nearest diatonic scale degree
# 0=Root, 1=2nd, 2=3rd, 3=4th, 4=5th, 5=6th, 6=7th
_SEMI_TO_DEG = [0, 0, 1, 2, 2, 3, 4, 4, 5, 5, 6, 6]


def semitone_to_degree(semitones: int) -> int:
    """Convertit un Ã©cart en demi-tons (depuis la racine) en degrÃ© 0-6."""
    return _SEMI_TO_DEG[semitones % 12]


def chord_root_at(notes: list[Note], beat: float,
                  window_before: float = 0.1,
                  window_after:  float = 0.5) -> Optional[int]:
    """
    Retourne la classe de hauteur de la note la plus basse active
    dans la fenÃªtre [beat - window_before, beat + window_after].
    ReprÃ©sente la racine probable de l'accord courant.
    """
    active = [
        n for n in notes
        if n.start - window_before <= beat <= n.start + n.duration + window_after
    ]
    if not active:
        return None
    return min(n.pitch for n in active) % 12


def melody_pitch_at(notes: list[Note], beat: float,
                    window: float = 0.55) -> Optional[int]:
    """
    Retourne la hauteur MIDI de la note mÃ©lodique au temps `beat`.
    Prend la note la plus haute active ou dÃ©marrant juste aprÃ¨s le temps.
    """
    # Notes active at this exact beat
    active = [
        n for n in notes
        if n.start <= beat + 0.05 and n.start + n.duration > beat - 0.05
    ]
    # Notes starting slightly ahead (pick-ups, anticipations)
    upcoming = [
        n for n in notes
        if 0 < n.start - beat < window
    ]
    candidates = active + upcoming
    if not candidates:
        return None
    # Take the highest pitch (treble melody)
    return max(n.pitch for n in candidates)


def extract_contours(tracks: list[TrackInfo], key_root: int,
                     max_bars: int = 16) -> list[tuple[int, ...]]:
    """
    Extrait les contours 4-temps (un degrÃ© par beat) depuis la piste mÃ©lodique.
    Format identique aux tableaux `contours[6][4]` du plugin.
    Retourne une liste de tuples (d0, d1, d2, d3).
    """
    # Pick melody track(s)
    melody_tracks = [t for t in tracks if t.role == "melody"]
    if not melody_tracks:
        non_drum = [t for t in tracks if t.instrument["family"] != "drums"]
        if not non_drum:
            return []
        melody_tracks = [max(non_drum, key=lambda t: t.avg_pitch)]

    # Pick harmony/bass tracks for chord root detection
    harmony_tracks = [t for t in tracks if t.role in ("harmony", "bass")]

    mel_notes = sorted(
        [n for t in melody_tracks for n in t.notes],
        key=lambda n: n.start,
    )
    harm_notes = sorted(
        [n for t in harmony_tracks for n in t.notes],
        key=lambda n: n.start,
    )

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

            # Chord root (prefer harmony; fall back to key root)
            root = chord_root_at(harm_notes, beat_time) if harm_notes else key_root
            if root is None:
                root = key_root

            # Melody pitch
            pitch = melody_pitch_at(mel_notes, beat_time)
            if pitch is None:
                valid = False
                break

            degrees.append(semitone_to_degree(pitch - root))

        if valid and len(degrees) == 4:
            contours.append(tuple(degrees))

    return contours


def extract_chord_patterns(tracks: list[TrackInfo],
                            key_root: int,
                            max_beats: int = 64) -> list[tuple[int, ...]]:
    """
    Extrait les voicings d'accords de la main gauche.
    Retourne des tuples de degrÃ©s scalaires simultanÃ©s.
    """
    harmony_tracks = [t for t in tracks if t.role in ("harmony", "bass")]
    if not harmony_tracks:
        return []

    harm_notes = sorted(
        [n for t in harmony_tracks for n in t.notes],
        key=lambda n: n.start,
    )
    if not harm_notes:
        return []

    total_beats = min(max(n.start + n.duration for n in harm_notes), max_beats)
    patterns: list[tuple[int, ...]] = []

    for beat in range(int(total_beats)):
        bt = float(beat)
        active = [
            n for n in harm_notes
            if n.start <= bt + 0.1 and n.start + n.duration > bt
        ]
        if len(active) < 2:
            continue

        root = min(n.pitch for n in active) % 12
        degs = tuple(sorted(set(
            semitone_to_degree(n.pitch - root) for n in active
        )))
        if len(degs) >= 2:
            patterns.append(degs)

    return patterns


# -- Rhythm Pattern Extraction --------------------------------------------------

_RHYTHM_SLOTS = 16   # 4 beats x 4 sixteenth notes per bar


def extract_rhythm_patterns(tracks, max_bars=16):
    """Extract 16-slot binary rhythm grids (one per bar) from melody track."""
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
    """Pack a 16-slot pattern into a uint16 bitmask."""
    val = 0
    for i, b in enumerate(pat[:16]):
        if b:
            val |= (1 << i)
    return val


def rhythm_grid_str(pat):
    """ASCII grid: x=onset, .=silence."""
    return "".join("x" if b else "." for b in pat[:16])


def describe_rhythm(pat):
    """Short label for a 16-slot rhythm pattern."""
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
    """Normalised Hamming distance between two 16-slot patterns."""
    n = min(len(a), len(b))
    return sum(1 for x, y in zip(a[:n], b[:n]) if x != y) / n if n else 1.0


def rhythm_originality_score(pat, corpus_counter, sample):
    """Originality score [0,1]: 40% rarity + 20% density + 40% Hamming distance."""
    total  = sum(corpus_counter.values())
    count  = corpus_counter.get(pat, 0)
    rarity = 1.0 - (count / total) if total > 0 else 1.0
    density_norm = max(0.0, (sum(pat) - 2) / 14)
    others   = [p for p in sample if p != pat]
    avg_dist = sum(rhythm_hamming(pat, o) for o in others) / len(others) if others else 0.5
    return 0.40 * rarity + 0.20 * density_norm + 0.40 * avg_dist


def kmeans_rhythms(patterns, k=8, n_iter=100, seed=42):
    """K-means on 16-slot rhythm patterns; returns k representative centroids."""
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



# â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
# Originality Analysis
# â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

def melodic_entropy(contours: list[tuple]) -> float:
    """
    Entropie de Shannon des degrÃ©s mÃ©lodiques.
    0 = monotone (tous identiques), ~2.8 = maximal (7 degrÃ©s Ã©quirÃ©partis).
    """
    all_degs = [d for c in contours for d in c]
    if not all_degs:
        return 0.0
    cnt   = Counter(all_degs)
    total = sum(cnt.values())
    return -sum((v / total) * math.log2(v / total + 1e-12) for v in cnt.values())


def contour_hamming(a: tuple, b: tuple) -> float:
    """Distance de Hamming normalisÃ©e entre deux contours de mÃªme longueur."""
    n = min(len(a), len(b))
    if n == 0:
        return 1.0
    return sum(1 for x, y in zip(a[:n], b[:n]) if x != y) / n


def originality_score(
    contour: tuple,
    corpus_counter: Counter,
    sample: list[tuple],
) -> float:
    """
    Score d'originalitÃ© [0, 1].
    0 = pattern trÃ¨s courant (clichÃ©), 1 = rarissime et distant des autres.

    Composantes (toutes normalisÃ©es 0â†’1) :
      40%  RaretÃ© corpus      : 1 - count / total_patterns
      20%  DiversitÃ© interne  : nb degrÃ©s distincts / longueur
      40%  Distance moyenne   : Hamming moyen vs. un Ã©chantillon du corpus
    """
    total = sum(corpus_counter.values())
    count = corpus_counter.get(contour, 0)
    rarity = 1.0 - (count / total) if total > 0 else 1.0

    # Internal diversity (0 if all beats same, 1 if all different)
    unique = len(set(contour))
    diversity = (unique - 1) / max(1, len(contour) - 1)

    # Distance to a sample of the corpus (capped at 200 for speed)
    others = [c for c in sample if c != contour]
    if others:
        avg_dist = sum(contour_hamming(contour, o) for o in others) / len(others)
    else:
        avg_dist = 0.5

    return 0.40 * rarity + 0.20 * diversity + 0.40 * avg_dist


# â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
# K-Means Clustering  (numpy only â€” no sklearn dependency)
# â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

def kmeans_contours(contours: list[tuple], k: int = 6,
                    n_iter: int = 100, seed: int = 42) -> list[tuple]:
    """
    K-means sur les contours (reprÃ©sentÃ©s comme vecteurs R^4).
    Retourne k centroids arrondis Ã  des degrÃ©s entiers 0-6.
    Initialisation K-means++ pour meilleure convergence.
    """
    if not contours:
        return []

    k = min(k, len(contours))
    rng  = np.random.default_rng(seed)
    data = np.array(contours, dtype=float)  # (N, 4)

    # K-means++ initialisation
    idx = [int(rng.integers(len(data)))]
    for _ in range(k - 1):
        dists = np.min(
            [np.sum((data - data[i]) ** 2, axis=1) for i in idx],
            axis=0,
        )
        probs = dists / (dists.sum() + 1e-12)
        idx.append(int(rng.choice(len(data), p=probs)))

    centers = data[idx].copy()

    for _ in range(n_iter):
        # Assignment
        dist_mat = np.sqrt(
            np.sum((data[:, None] - centers[None]) ** 2, axis=2)
        )  # (N, k)
        labels = np.argmin(dist_mat, axis=1)

        # Update
        new_centers = np.zeros_like(centers)
        for c in range(k):
            mask = labels == c
            new_centers[c] = data[mask].mean(axis=0) if mask.any() else centers[c]

        if np.allclose(centers, new_centers, atol=1e-3):
            break
        centers = new_centers

    # Round to valid degree integers and deduplicate
    result: list[tuple] = []
    seen: set[tuple] = set()
    for c in centers:
        t = tuple(int(np.clip(round(v), 0, 6)) for v in c)
        if t not in seen:
            seen.add(t)
            result.append(t)

    return result


# â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
# Contour description helpers
# â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

_DEGREE_NAMES = ["R", "2nd", "3rd", "4th", "5th", "6th", "7th"]


def describe_contour(c: tuple) -> str:
    """GÃ©nÃ¨re un nom musical bref pour la forme du contour."""
    if len(c) < 4:
        return "?"
    a, b, cc, d = c[0], c[1], c[2], c[3]
    ascending  = b >= a and cc >= b and d >= cc
    descending = b <= a and cc <= b and d <= cc
    arch       = b > a  and cc >= b and d < cc
    valley     = b < a  and cc <= b and d > cc
    step_up    = d > a  and b <= a
    step_down  = d < a  and b >= a

    if ascending:  return "rise"
    if descending: return "descent"
    if arch:       return "arch"
    if valley:     return "valley"
    if step_up:    return "zigzag-up"
    if step_down:  return "zigzag-down"
    return "wave"


# â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
# Source metadata
# â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

def parse_source_name(path: Path) -> dict:
    """Extrait game / series / title depuis le naming convention VGMidi."""
    stem  = path.stem
    parts = stem.split("_")
    # Convention: "Game_Platform_Series_Title More Words"
    game  = parts[0] if len(parts) > 0 else "Unknown"
    platform = parts[1] if len(parts) > 1 else ""
    series   = parts[2] if len(parts) > 2 else ""
    title    = " ".join(parts[3:]) if len(parts) > 3 else stem
    return {
        "game":     game,
        "platform": platform,
        "series":   series,
        "title":    title,
        "filename": stem,
    }


# â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
# C++ Header Generation
# â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

def _safe_varname(text: str, max_len: int = 40) -> str:
    """Convertit un texte en identifiant C++ valide en snake_case."""
    v = re.sub(r"[^a-zA-Z0-9]", "_", text.lower())
    v = re.sub(r"_+", "_", v).strip("_")
    return v[:max_len]


def generate_cpp_header(focus_entries: list[dict],
                        top_contours: list[tuple],
                        all_contours_counter: Counter,
                        top_rhythms: Optional[list[tuple]] = None) -> str:
    """
    GÃ©nÃ¨re un header C++ contenant :
      1. Les top-k contours appris (remplacement direct dans voicesForStyle)
      2. Un bloc par source focus (Final Fantasy, Zelda, Chrono...)
    """
    lines = [
        "// ================================================================",
        "// patterns.h â€” Patterns extraits par midi_pattern_learner.py",
        "//              NE PAS Ã‰DITER â€” regÃ©nÃ©rer avec midi_pattern_learner.py",
        "// ================================================================",
        "#pragma once",
        "#include <cstdint>",
        "#include <array>",
        "",
        "namespace midnight_patterns {",
        "",
        "// DegrÃ©s scalaires : 0=R, 1=2nd, 2=3rd, 3=4th, 4=5th, 5=6th, 6=7th",
        "// Format identique aux tableaux contours[6][4] du plugin.",
        "",
    ]

    # â”€â”€ Top learned contours (drop-in replacement for plugin) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    n = min(6, len(top_contours))
    lines += [
        "// â”€â”€ Contours appris â€” remplacement pour voicesForStyle() â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€",
        f"// Remplace: static constexpr int8_t contours[6][4] dans style == 0",
        f"static constexpr int8_t learned_contours[{n}][4] = {{",
    ]
    for c in top_contours[:6]:
        if len(c) < 4:
            continue
        deg_str  = ", ".join(f"{d:2d}" for d in c[:4])
        note_str = " â†’ ".join(_DEGREE_NAMES[min(d, 6)] for d in c[:4])
        shape    = describe_contour(c)
        lines.append(f"    {{ {deg_str} }},  // {shape}: {note_str}")
    lines += ["};", ""]

    # â”€â”€ Per-source patterns â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    lines.append("// â”€â”€ Patterns par source (focus FF / Zelda / Chrono) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€")
    lines.append("")

    # -- Learned rhythm patterns --------------------------------------------------
    if top_rhythms:
        nr = len(top_rhythms)
        lines += [
            "// -- Patterns rythmiques appris ------------------------------------------",
            "// 16 bits par mesure : bit i = 1 si une note demarre sur la i-eme double-croche",
            "// slot 0=temps1  slot 4=temps2  slot 8=temps3  slot 12=temps4",
            f"static constexpr uint16_t learned_rhythms[{nr}] = {{",
        ]
        for r in top_rhythms:
            val   = rhythm_as_uint16(r)
            grid  = rhythm_grid_str(r)
            shape = describe_rhythm(r)
            dens  = sum(r)
            lines.append(f"    0x{val:04X},  // {grid}  {shape} ({dens} onsets)")
        lines += ["};" , ""]

    for entry in focus_entries:
        contours_raw = entry.get("contours", [])
        if not contours_raw:
            continue

        game  = entry.get("game", "Unknown")
        title = entry.get("title", "?")
        instr = entry.get("instrument_label", "?")
        orig  = entry.get("originality", 0.0)

        varname = _safe_varname(f"{game}_{title}")
        n_c     = min(6, len(contours_raw))

        lines += [
            f"// â”€â”€ {game}: {title}",
            f"// Instrument: {instr} | OriginalitÃ©: {orig:.3f} | {n_c} contours",
            f"static constexpr int8_t {varname}[{n_c}][4] = {{",
        ]
        for c in contours_raw[:6]:
            t = tuple(c)
            if len(t) < 4:
                continue
            deg_str  = ", ".join(f"{d:2d}" for d in t[:4])
            note_str = " â†’ ".join(_DEGREE_NAMES[min(d, 6)] for d in t[:4])
            shape    = describe_contour(t)
            freq     = all_contours_counter.get(t, 0)
            lines.append(f"    {{ {deg_str} }},  // {shape}: {note_str}  [{freq}Ã—]")
        lines += ["};", ""]

    lines.append("} // namespace midnight_patterns")
    return "\n".join(lines) + "\n"


# â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
# Per-file analysis
# â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

def analyze_midi(path: Path, max_bars: int = 16) -> Optional[dict]:
    """Analyse complÃ¨te d'un fichier MIDI. Retourne un dict ou None si erreur."""
    tracks, bpm = parse_midi(path)
    if not tracks:
        return None

    assign_roles(tracks)
    key_root, mode = detect_key(tracks)
    contours     = extract_contours(tracks, key_root, max_bars=max_bars)
    chord_pats   = extract_chord_patterns(tracks, key_root)
    rhythm_pats  = extract_rhythm_patterns(tracks, max_bars=max_bars)

    # Primary instrument = melody track
    mel_tracks = [t for t in tracks if t.role == "melody"]
    if mel_tracks:
        instr     = mel_tracks[0].instrument
        avg_pitch = mel_tracks[0].avg_pitch
        pitch_std = mel_tracks[0].pitch_std
    elif tracks:
        instr     = tracks[0].instrument
        avg_pitch = tracks[0].avg_pitch
        pitch_std = tracks[0].pitch_std
    else:
        return None

    # All instrument families present (excluding drums)
    families = sorted(set(
        t.instrument["family"] for t in tracks
        if t.instrument["family"] != "drums"
    ))

    src = parse_source_name(path)

    return {
        "game":              src["game"],
        "platform":          src["platform"],
        "series":            src["series"],
        "title":             src["title"],
        "filename":          src["filename"],
        "bpm":               round(bpm, 1),
        "key_root":          key_root,
        "mode":              mode,
        "instrument_family": instr["family"],
        "instrument_label":  instr["label"],
        "instrument_conf":   round(instr.get("confidence", 0.0), 2),
        "avg_pitch":         round(avg_pitch, 1),
        "pitch_range_std":   round(pitch_std, 1),
        "n_tracks":          len(tracks),
        "families_present":  families,
        "contours":          [list(c) for c in contours],
        "chord_patterns":    [list(c) for c in chord_pats[:24]],
        "rhythm_patterns":   [list(r) for r in rhythm_pats],
        # Filled in post-processing:
        "originality":           0.0,
        "best_originality":      0.0,
        "most_original_contour": [],
    }


# â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
# Main pipeline
# â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

def run(midi_dir: Path, focus: list[str], top_k: int, max_bars: int) -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    # â”€â”€ Collect MIDI files â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    all_midis = sorted(set(midi_dir.glob("**/*.mid")))
    if not all_midis:
        print(f"Aucun fichier .mid trouvÃ© dans {midi_dir}")
        return

    print(f"\n{'='*60}")
    print(f"  MIDI Pattern Learner â€” Midnight Melody Maker")
    print(f"{'='*60}")
    print(f"  Corpus    : {len(all_midis)} fichiers MIDI")
    print(f"  Focus     : {', '.join(focus) if focus else 'tous'}")
    print(f"  Top-K     : {top_k} contours reprÃ©sentatifs")
    print(f"  Max bars  : {max_bars} mesures par fichier")
    print(f"  Sortie    : {OUT_DIR}")
    print()

    # â”€â”€ Analyze all files â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    all_results:   list[dict] = []
    focus_results: list[dict] = []
    errors = 0

    for i, path in enumerate(all_midis):
        label = path.name[:55]
        print(f"\r  [{i+1:3d}/{len(all_midis)}] {label:<55}", end="", flush=True)

        result = analyze_midi(path, max_bars=max_bars)
        if result is None:
            errors += 1
            continue

        all_results.append(result)
        is_focus = (not focus) or any(
            f.lower() in path.name.lower() for f in focus
        )
        if is_focus:
            focus_results.append(result)

    print(f"\n\n  AnalysÃ©s : {len(all_results)} OK, {errors} erreurs")
    print(f"  Focus    : {len(focus_results)} morceaux")

    # â”€â”€ Build corpus-wide contour catalog â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    all_contours_flat: list[tuple] = [
        tuple(c) for r in all_results for c in r["contours"]
    ]
    corpus_counter = Counter(all_contours_flat)

    print(f"\n  Contours  : {len(all_contours_flat)} total, "
          f"{len(corpus_counter)} patterns uniques")

    # â”€â”€ Originality scoring â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    # Use a sample of up to 300 contours for distance computations (speed)
    rng = np.random.default_rng(0)
    sample_idx = rng.choice(
        len(all_contours_flat),
        size=min(300, len(all_contours_flat)),
        replace=False,
    )
    corpus_sample = [all_contours_flat[i] for i in sample_idx]

    for r in all_results:
        ctrs = [tuple(c) for c in r["contours"]]
        if not ctrs:
            continue
        scores = [
            originality_score(c, corpus_counter, corpus_sample)
            for c in ctrs
        ]
        best_idx            = int(np.argmax(scores))
        r["originality"]    = round(float(np.mean(scores)), 4)
        r["best_originality"]      = round(scores[best_idx], 4)
        r["most_original_contour"] = list(ctrs[best_idx])

    # â”€â”€ Melodic entropy â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    focus_contours = [tuple(c) for r in focus_results for c in r["contours"]]
    global_entropy = melodic_entropy(focus_contours)
    all_entropy    = melodic_entropy(all_contours_flat)

    print(f"\n  Entropie mÃ©lodique â€” focus : {global_entropy:.3f} bits  "
          f"| corpus : {all_entropy:.3f} bits")
    print(f"  (max thÃ©orique 7 degrÃ©s = {math.log2(7):.3f} bits)")

    # â”€â”€ K-means clustering â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    print(f"\n  K-means : {len(all_contours_flat)} contours â†’ {top_k} centroids...")
    top_contours = kmeans_contours(all_contours_flat, k=top_k)

    print(f"\n  Top {len(top_contours)} contours appris (degrÃ© par beat) :")
    for i, c in enumerate(top_contours):
        shape    = describe_contour(c)
        deg_str  = " â†’ ".join(_DEGREE_NAMES[min(d, 6)] for d in c)
        freq     = corpus_counter.get(c, 0)
        print(f"    [{i}] {list(c)}  {shape:12s}  {deg_str}  ({freq}Ã—)")

    # â”€â”€ Instrument distribution â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    # -- Rhythm pattern analysis --------------------------------------------------
    all_rhythms_flat: list[tuple] = [
        tuple(r) for res in all_results for r in res.get("rhythm_patterns", [])
    ]
    rhythm_counter = Counter(all_rhythms_flat)

    print(f"\n  Rythmes   : {len(all_rhythms_flat)} total, "
          f"{len(rhythm_counter)} patterns uniques")

    rng_r = np.random.default_rng(1)
    rsample_idx = rng_r.choice(
        len(all_rhythms_flat),
        size=min(300, len(all_rhythms_flat)),
        replace=False,
    ) if all_rhythms_flat else []
    rhythm_sample = [all_rhythms_flat[i] for i in rsample_idx]

    for res in all_results:
        rpats = [tuple(r) for r in res.get("rhythm_patterns", [])]
        if not rpats:
            res["rhythm_originality"]      = 0.0
            res["best_rhythm_originality"] = 0.0
            res["most_original_rhythm"]    = []
            continue
        rscores = [
            rhythm_originality_score(r, rhythm_counter, rhythm_sample)
            for r in rpats
        ]
        best_idx = int(np.argmax(rscores))
        res["rhythm_originality"]      = round(float(np.mean(rscores)), 4)
        res["best_rhythm_originality"] = round(rscores[best_idx], 4)
        res["most_original_rhythm"]    = list(rpats[best_idx])

    top_rhythms = kmeans_rhythms(all_rhythms_flat, k=8) if all_rhythms_flat else []

    print(f"\n  Top {len(top_rhythms)} rythmes appris (patterns 16 slots) :")
    for i, r in enumerate(top_rhythms):
        shape = describe_rhythm(r)
        grid  = rhythm_grid_str(r)
        freq  = rhythm_counter.get(r, 0)
        val   = rhythm_as_uint16(r)
        print(f"    [{i}] 0x{val:04X}  {grid}  {shape:20s}  ({freq}x)")

    focus_r_sorted = sorted(
        [res for res in focus_results if res.get("most_original_rhythm")],
        key=lambda r: r.get("best_rhythm_originality", 0),
        reverse=True,
    )
    print(f"\n  Top 10 rythmes originaux (focus) :")
    for res in focus_r_sorted[:10]:
        o    = res["best_rhythm_originality"]
        mor  = res["most_original_rhythm"]
        grid = rhythm_grid_str(tuple(mor)) if mor else ""
        print(f"    {o:.3f}  {res['game']}: {res['title'][:35]:<35}  {grid}")

    instr_dist = Counter(r["instrument_family"] for r in all_results)
    print(f"\n  Instruments (piste mÃ©lodique) :")
    for fam, cnt in instr_dist.most_common():
        bar = "â–ˆ" * min(cnt, 35)
        print(f"    {fam:<15} {bar} {cnt}")

    # â”€â”€ Originality top-15 â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    focus_sorted = sorted(
        focus_results,
        key=lambda r: r.get("best_originality", 0),
        reverse=True,
    )
    print(f"\n  Top 15 patterns originaux (focus) :")
    for r in focus_sorted[:15]:
        o   = r["best_originality"]
        mc  = r["most_original_contour"]
        fam = r["instrument_family"]
        print(f"    {o:.3f}  [{fam:<10}]  "
              f"{r['game']}: {r['title'][:35]}  â†’ {mc}")

    # â”€â”€ Export 1: JSON â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    report = {
        "meta": {
            "corpus_size":          len(all_results),
            "focus_size":           len(focus_results),
            "total_contours":       len(all_contours_flat),
            "unique_contours":      len(corpus_counter),
            "total_rhythms":        len(all_rhythms_flat),
            "unique_rhythms":       len(rhythm_counter),
            "entropy_focus_bits":   round(global_entropy, 4),
            "entropy_corpus_bits":  round(all_entropy, 4),
            "entropy_max_bits":     round(math.log2(7), 4),
        },
        "instrument_distribution": dict(instr_dist),
        "top_contours": [list(c) for c in top_contours],
        "all_contour_counts": [[list(c), n] for c, n in corpus_counter.most_common()],
        "all_rhythm_counts": [[list(r), n] for r, n in rhythm_counter.most_common()],
        "top_rhythms":  [
            {
                "pattern":  list(r),
                "uint16":   rhythm_as_uint16(r),
                "grid":     rhythm_grid_str(r),
                "shape":    describe_rhythm(r),
                "density":  sum(r),
                "freq":     rhythm_counter.get(r, 0),
            }
            for r in top_rhythms
        ],
        "focus_results": focus_results,
        "all_results":  all_results,
    }
    report_path = OUT_DIR / "report.json"
    with open(report_path, "w", encoding="utf-8") as f:
        json.dump(report, f, ensure_ascii=False, indent=2)
    print(f"\n  [Export] JSON   â†’ {report_path}")

    # â”€â”€ Export 2: C++ header â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    # Only include focus results that actually have contours, sorted by originality
    focus_with_ctrs = sorted(
        [r for r in focus_results if r.get("contours")],
        key=lambda r: r.get("best_originality", 0),
        reverse=True,
    )
    cpp = generate_cpp_header(focus_with_ctrs[:50], top_contours, corpus_counter, top_rhythms)
    cpp_path = OUT_DIR / "patterns.h"
    with open(cpp_path, "w", encoding="utf-8") as f:
        f.write(cpp)
    print(f"  [Export] C++    â†’ {cpp_path}")

    # â”€â”€ Export 3: Human summary â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    sep  = "=" * 60
    sep2 = "-" * 60
    lines = [
        sep,
        "  MIDI Pattern Learner â€” RÃ©sumÃ© d'analyse",
        sep,
        "",
        f"  Corpus total     : {len(all_results)} morceaux",
        f"  Focus (FF/Zelda) : {len(focus_results)} morceaux",
        f"  Contours total   : {len(all_contours_flat)}",
        f"  Contours uniques : {len(corpus_counter)}",
        f"  Entropie focus   : {global_entropy:.3f} bits",
        f"  Entropie corpus  : {all_entropy:.3f} bits",
        f"  (max thÃ©orique 7 degrÃ©s = {math.log2(7):.3f} bits)",
        "",
        sep2,
        f"  Top {len(top_contours)} contours appris",
        f"  (remplacement direct du plugin, voicesForStyle style==0)",
        sep2,
    ]
    for i, c in enumerate(top_contours):
        shape   = describe_contour(c)
        deg_str = " â†’ ".join(_DEGREE_NAMES[min(d, 6)] for d in c)
        freq    = corpus_counter.get(c, 0)
        lines.append(f"  [{i}] {list(c)}  {shape:12s}  {deg_str}  ({freq}Ã—)")

    lines += ["", sep2, "  RÃ©partition instruments", sep2]
    for fam, cnt in instr_dist.most_common():
        bar = "â–ˆ" * min(cnt, 30)
        lines.append(f"  {fam:<15} {bar} ({cnt})")

    lines += ["", sep2, "  Top 20 patterns les plus originaux (focus)", sep2]
    for r in focus_sorted[:20]:
        o   = r["best_originality"]
        mc  = r["most_original_contour"]
        fam = r["instrument_family"]
        conf = r.get("instrument_conf", 0)
        lines.append(
            f"  {o:.3f}  [{fam:<12} conf={conf:.0%}]  "
            f"{r['game']}: {r['title'][:30]}  â†’ {mc}"
        )

    # Chord patterns
    all_chord_ctr = Counter(
        tuple(cp) for r in focus_results
        for cp in r.get("chord_patterns", [])
    )
    lines += ["", sep2, "  Top rythmes appris (K-means)", sep2]
    for i, r in enumerate(top_rhythms):
        shape = describe_rhythm(r)
        grid  = rhythm_grid_str(r)
        val   = rhythm_as_uint16(r)
        freq  = rhythm_counter.get(r, 0)
        lines.append(f"  [{i}] 0x{val:04X}  {grid}  {shape}  ({freq}x)")

    lines += ["", sep2, "  Top 10 rythmes originaux (focus)", sep2]
    for res in focus_r_sorted[:10]:
        o    = res["best_rhythm_originality"]
        mor  = res["most_original_rhythm"]
        grid = rhythm_grid_str(tuple(mor)) if mor else ""
        lines.append(f"  {o:.3f}  {res['game']}: {res['title'][:30]:<30}  {grid}")

    lines += ["", sep2, "  Patterns d'accords main gauche (top 15)", sep2]
    for cp, cnt in all_chord_ctr.most_common(15):
        deg_str = " + ".join(_DEGREE_NAMES[min(d, 6)] for d in cp)
        lines.append(f"  {cnt:4d}Ã—  {list(cp)}  ({deg_str})")

    # Instrument analysis by game
    game_instrs: dict[str, Counter] = defaultdict(Counter)
    for r in focus_results:
        game_instrs[r["game"]][r["instrument_family"]] += 1

    lines += ["", sep2, "  Instruments par jeu", sep2]
    for game, ctr in sorted(game_instrs.items()):
        instr_str = ", ".join(f"{f}Ã—{c}" for f, c in ctr.most_common(5))
        lines.append(f"  {game:<20}  {instr_str}")

    summary_path = OUT_DIR / "summary.txt"
    with open(summary_path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    print(f"  [Export] RÃ©sumÃ© â†’ {summary_path}")

    print(f"\n{'='*60}")
    print(f"  âœ“ Analyse terminÃ©e â€” {len(top_contours)} contours prÃªts pour le plugin")
    print(f"{'='*60}\n")


# â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
# Entry point
# â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="MIDI Pattern Learner â€” Midnight Melody Maker",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument(
        "--midi-dir", type=Path, default=DATA_DIR,
        help="RÃ©pertoire des fichiers MIDI (dÃ©faut: data/vgmidi_raw/labelled/midi)",
    )
    parser.add_argument(
        "--focus", type=str, default="Final Fantasy,Zelda,Chrono",
        help="Sous-chaÃ®nes de filtrage des sources focus (sÃ©parÃ©es par virgule)",
    )
    parser.add_argument(
        "--top", type=int, default=6,
        help="Nombre de contours reprÃ©sentatifs Ã  extraire par k-means (dÃ©faut: 6)",
    )
    parser.add_argument(
        "--bars", type=int, default=16,
        help="Nombre de mesures max analysÃ©es par fichier (dÃ©faut: 16)",
    )
    args = parser.parse_args()

    focus_list = [f.strip() for f in args.focus.split(",") if f.strip()]
    run(args.midi_dir, focus_list, args.top, args.bars)
