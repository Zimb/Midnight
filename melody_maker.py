"""
melody_maker.py
================

Composition par etapes, mesure par mesure, multi-pistes, avec validation.

Flux d'utilisation :

  1. Reglages projet : tonalite, mode, signature rythmique, tempo.
  2. Progression de reference :
       - Choisir la longueur (en mesures) : 2 / 4 / 8.
       - Generer plusieurs progressions candidates.
       - Ecouter, valider celle qui plait. Elle devient la progression du
         morceau et se repete automatiquement quand les pistes s'allongent.
  3. Pistes :
       - Ajouter des pistes (melodie / basse / pad / arpege) avec leur
         instrument GM.
       - Choisir la longueur du prochain bloc en mesures (1, 2 ou 4).
       - Generer N candidats, ecouter, valider. La piste avance d'un bloc.
       - Les blocs sont toujours alignes sur la mesure et collent aux
         accords en cours.

Aucun LLM. Generation algorithmique. Preview audio via FluidSynth si
disponible. Export MIDI via mido.
"""

from __future__ import annotations

import json
import math
import os
import random
import threading
import time
import tkinter as tk
from dataclasses import dataclass, field
from tkinter import filedialog, messagebox, ttk
from typing import Callable, List, Optional, Sequence, Tuple

# ----------------------------------------------------------------------------
# Optional dependencies
# ----------------------------------------------------------------------------

try:
    import fluidsynth  # type: ignore
    HAVE_FLUIDSYNTH = True
except Exception:
    HAVE_FLUIDSYNTH = False

try:
    import mido  # type: ignore
    from mido import Message, MetaMessage, MidiFile, MidiTrack
    HAVE_MIDO = True
except Exception:
    HAVE_MIDO = False


# ============================================================================
# 1. Music theory
# ============================================================================

NOTE_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]

SCALE_INTERVALS = {
    "major": [0, 2, 4, 5, 7, 9, 11],
    "minor": [0, 2, 3, 5, 7, 8, 10],
    "dorian": [0, 2, 3, 5, 7, 9, 10],
    "mixolydian": [0, 2, 4, 5, 7, 9, 10],
}

DEGREE_QUALITIES = {
    "major": ["maj", "min", "min", "maj", "maj", "min", "dim"],
    "minor": ["min", "dim", "maj", "min", "min", "maj", "maj"],
    "dorian": ["min", "min", "maj", "maj", "min", "dim", "maj"],
    "mixolydian": ["maj", "min", "dim", "maj", "min", "min", "maj"],
}

CHORD_INTERVALS = {
    "maj": [0, 4, 7],
    "min": [0, 3, 7],
    "dim": [0, 3, 6],
    "aug": [0, 4, 8],
}

# Markov-like degree transitions, by mode.
DEGREE_TRANSITIONS = {
    "major": {
        1: [(4, 3), (5, 3), (6, 2), (2, 2), (3, 1)],
        2: [(5, 4), (7, 1), (4, 1)],
        3: [(6, 3), (4, 2), (1, 1)],
        4: [(5, 4), (1, 2), (2, 2), (6, 1)],
        5: [(1, 5), (6, 2)],
        6: [(2, 3), (4, 3), (5, 2), (1, 1)],
        7: [(1, 3), (3, 1)],
    },
    "minor": {
        1: [(4, 3), (5, 3), (6, 2), (3, 2), (7, 1)],
        2: [(5, 3), (1, 1)],
        3: [(6, 3), (7, 2), (4, 1)],
        4: [(5, 4), (1, 2), (7, 1)],
        5: [(1, 5), (6, 2)],
        6: [(3, 2), (4, 2), (7, 1), (1, 1)],
        7: [(1, 3), (3, 2)],
    },
    "dorian": {
        1: [(4, 3), (7, 2), (5, 1), (2, 1)],
        2: [(5, 2), (1, 1)],
        3: [(6, 1), (4, 1)],
        4: [(1, 3), (7, 2), (5, 1)],
        5: [(1, 4), (4, 1)],
        6: [(2, 1), (4, 1)],
        7: [(1, 3), (4, 2)],
    },
    "mixolydian": {
        1: [(7, 3), (4, 2), (5, 1)],
        2: [(5, 1), (1, 1)],
        3: [(6, 1)],
        4: [(1, 3), (5, 1)],
        5: [(1, 2), (4, 1)],
        6: [(2, 1), (4, 1)],
        7: [(1, 4), (4, 2)],
    },
}

# A few hand-curated progression "seeds" per mode (degrees only).
SEED_PROGRESSIONS = {
    "major": [
        [1, 5, 6, 4], [1, 6, 4, 5], [6, 4, 1, 5], [1, 4, 5, 1],
        [2, 5, 1, 6], [1, 3, 4, 5], [4, 1, 5, 6], [1, 5, 4, 5],
    ],
    "minor": [
        [1, 6, 3, 7], [1, 7, 6, 7], [1, 4, 5, 1], [1, 6, 4, 5],
        [1, 5, 6, 4], [4, 1, 5, 1], [1, 3, 7, 6],
    ],
    "dorian": [
        [1, 4, 1, 7], [1, 2, 4, 1], [1, 7, 4, 1], [1, 4, 5, 1],
    ],
    "mixolydian": [
        [1, 7, 4, 1], [1, 5, 7, 1], [1, 4, 7, 1], [1, 7, 1, 4],
    ],
}


def note_index(name: str) -> int:
    name = name.strip()
    flats = {"Db": "C#", "Eb": "D#", "Gb": "F#", "Ab": "G#", "Bb": "A#"}
    if name in flats:
        name = flats[name]
    return NOTE_NAMES.index(name)


def scale_pitches(root_pc: int, mode: str,
                  low: int = 36, high: int = 96) -> List[int]:
    intervals = SCALE_INTERVALS[mode]
    pcs = {(root_pc + i) % 12 for i in intervals}
    return [p for p in range(low, high + 1) if p % 12 in pcs]


def chord_pitches(root_pc: int, quality: str, octave: int = 4) -> List[int]:
    base = octave * 12 + root_pc
    return [base + i for i in CHORD_INTERVALS[quality]]


def degree_to_chord(degree: int, key_root_pc: int,
                    mode: str) -> Tuple[int, str]:
    qualities = DEGREE_QUALITIES[mode]
    intervals = SCALE_INTERVALS[mode]
    interval = intervals[(degree - 1) % 7]
    quality = qualities[(degree - 1) % 7]
    return ((key_root_pc + interval) % 12, quality)


def next_degree(prev: int, mode: str, rng: random.Random) -> int:
    table = DEGREE_TRANSITIONS.get(mode, DEGREE_TRANSITIONS["major"])
    options = table.get(prev, [(1, 1), (4, 1), (5, 1)])
    pool: List[int] = []
    for deg, weight in options:
        pool.extend([deg] * weight)
    return rng.choice(pool)


def chord_label(root_pc: int, quality: str) -> str:
    suffix = {"maj": "", "min": "m", "dim": "dim", "aug": "+"}.get(quality, "")
    return f"{NOTE_NAMES[root_pc]}{suffix}"


# ============================================================================
# 2. Domain model
# ============================================================================

@dataclass
class Note:
    start_beat: float
    duration_beats: float
    pitch: int
    velocity: int = 92


@dataclass
class Block:
    start_beat: float
    length_beats: float
    notes: List[Note] = field(default_factory=list)
    bars: int = 1

    @property
    def end_beat(self) -> float:
        return self.start_beat + self.length_beats


@dataclass
class ChordSegment:
    start_beat: float
    length_beats: float
    root_pc: int
    quality: str
    degree: int

    @property
    def end_beat(self) -> float:
        return self.start_beat + self.length_beats


@dataclass
class Track:
    name: str
    role: str
    program: int
    channel: int
    blocks: List[Block] = field(default_factory=list)
    color: str = "#3aa3ff"
    muted: bool = False

    @property
    def cursor_beat(self) -> float:
        return self.blocks[-1].end_beat if self.blocks else 0.0

    @property
    def cursor_bars(self) -> int:
        return sum(b.bars for b in self.blocks)

    def all_notes(self) -> List[Note]:
        notes: List[Note] = []
        for b in self.blocks:
            notes.extend(b.notes)
        return notes


@dataclass
class Project:
    key_root: str = "C"
    mode: str = "major"
    tempo_bpm: int = 100
    time_sig_num: int = 4
    time_sig_den: int = 4
    progression_length_bars: int = 4
    progression_pattern: List[int] = field(default_factory=list)  # degrees
    chords: List[ChordSegment] = field(default_factory=list)
    tracks: List[Track] = field(default_factory=list)

    @property
    def key_root_pc(self) -> int:
        return note_index(self.key_root)

    @property
    def bar_beats(self) -> float:
        """Length of a bar in quarter-note beats (MIDI tempo unit)."""
        return self.time_sig_num * 4.0 / self.time_sig_den

    def chord_at(self, beat: float) -> Optional[ChordSegment]:
        for c in self.chords:
            if c.start_beat <= beat < c.end_beat:
                return c
        return self.chords[-1] if self.chords else None

    def chords_covering(self, start: float, end: float) -> List[ChordSegment]:
        return [c for c in self.chords
                if c.end_beat > start and c.start_beat < end]


# ============================================================================
# 3. Progression engine
# ============================================================================

class ProgressionEngine:
    """Build progression candidates and extend the committed pattern."""

    def __init__(self, project: Project, rng: random.Random):
        self.project = project
        self.rng = rng

    def propose_candidates(self, n: int) -> List[List[int]]:
        """Return n candidate progressions, each a list of degrees of length
        progression_length_bars."""
        p = self.project
        L = p.progression_length_bars
        seeds = SEED_PROGRESSIONS.get(p.mode, SEED_PROGRESSIONS["major"])
        out: List[List[int]] = []
        seen: set = set()

        # Use seeds first (looped/truncated to fit L).
        seed_pool = list(seeds)
        self.rng.shuffle(seed_pool)
        for s in seed_pool:
            if len(out) >= n // 2 + 1:
                break
            seq = (s * ((L // len(s)) + 1))[:L]
            key = tuple(seq)
            if key not in seen:
                seen.add(key)
                out.append(seq)

        # Markov-ish progressions for the rest.
        guard = 0
        while len(out) < n and guard < 200:
            guard += 1
            seq = [1]
            cur = 1
            for _ in range(L - 1):
                cur = next_degree(cur, p.mode, self.rng)
                seq.append(cur)
            # Bias towards plausible cadence at the end.
            if seq[-1] in (2, 3, 7) and self.rng.random() < 0.6:
                seq[-1] = self.rng.choice([1, 5, 4])
            key = tuple(seq)
            if key not in seen:
                seen.add(key)
                out.append(seq)
        return out[:n]

    def commit(self, degrees: List[int]) -> None:
        """Set this progression as the project's reference. Builds the first
        cycle of ChordSegments. The progression then loops on demand."""
        p = self.project
        p.progression_pattern = list(degrees)
        p.chords = []
        self._append_cycle(0)

    def _append_cycle(self, start_beat: float) -> float:
        p = self.project
        bar = p.bar_beats
        cur = start_beat
        for deg in p.progression_pattern:
            root_pc, quality = degree_to_chord(deg, p.key_root_pc, p.mode)
            p.chords.append(ChordSegment(cur, bar, root_pc, quality, deg))
            cur += bar
        return cur

    def ensure_covers(self, end_beat: float) -> None:
        p = self.project
        if not p.progression_pattern:
            return
        while p.chords and p.chords[-1].end_beat < end_beat:
            self._append_cycle(p.chords[-1].end_beat)


def progression_to_blocks(degrees: List[int], project: Project,
                          bars_per_chord: float = 1.0) -> List[ChordSegment]:
    """Turn a list of degrees into a sequence of ChordSegments starting at 0,
    used for previewing a candidate progression without committing."""
    bar = project.bar_beats
    chord_len = bar * bars_per_chord
    out: List[ChordSegment] = []
    t = 0.0
    for deg in degrees:
        root_pc, quality = degree_to_chord(deg, project.key_root_pc,
                                           project.mode)
        out.append(ChordSegment(t, chord_len, root_pc, quality, deg))
        t += chord_len
    return out


# ============================================================================
# 4. Block generators (per role) - aligned on bars
# ============================================================================

# Rhythm vocabulary in quarter-note beats.
RHYTHM_VOCAB = [
    (0.25, 2),
    (0.5, 5),
    (0.75, 1),
    (1.0, 4),
    (1.5, 2),
    (2.0, 2),
    (3.0, 1),
]


def _weighted_choice(rng: random.Random,
                     items: Sequence[Tuple[float, float]]) -> float:
    total = sum(w for _, w in items)
    r = rng.uniform(0, total)
    acc = 0.0
    for value, w in items:
        acc += w
        if r <= acc:
            return value
    return items[-1][0]


def _build_rhythm(rng: random.Random, length: float,
                  activity: float) -> List[Tuple[float, float]]:
    """Return (offset_in_block, duration) cells totalling length beats."""
    cells: List[Tuple[float, float]] = []
    cursor = 0.0
    while cursor < length - 1e-6:
        remaining = length - cursor
        candidates = [(d, w) for d, w in RHYTHM_VOCAB if d <= remaining + 0.001]
        if not candidates:
            break
        biased = []
        for d, w in candidates:
            bias = 1.0
            if activity > 0.55 and d <= 0.5:
                bias *= 1.6
            if activity < 0.45 and d >= 1.0:
                bias *= 1.6
            biased.append((d, w * bias))
        # occasional rest for breathing
        if rng.random() < 0.06 and cursor > 0.1:
            cursor += min(0.5, remaining)
            continue
        dur = _weighted_choice(rng, biased)
        cells.append((cursor, dur))
        cursor += dur
    return cells


def _nearest_pitch(target: float, pool: Sequence[int]) -> int:
    return min(pool, key=lambda p: abs(p - target))


def _step_or_leap(prev_pitch: int, scale: Sequence[int],
                  chord_pcs: Sequence[int], rng: random.Random,
                  leap_prob: float = 0.18) -> int:
    idx = scale.index(_nearest_pitch(prev_pitch, scale))
    if rng.random() < leap_prob:
        candidates = [p for p in scale
                      if p % 12 in chord_pcs and abs(p - prev_pitch) <= 12]
        if candidates:
            return rng.choice(candidates)
    step = rng.choice([-2, -1, -1, 1, 1, 2])
    new_idx = max(0, min(len(scale) - 1, idx + step))
    return scale[new_idx]


def generate_melody_block(project: Project, track: Track,
                          start_beat: float, bars: int,
                          activity: float, rng: random.Random) -> Block:
    length = bars * project.bar_beats
    segments = project.chords_covering(start_beat, start_beat + length)
    scale = scale_pitches(project.key_root_pc, project.mode,
                          low=55, high=84)

    if track.blocks and track.blocks[-1].notes:
        prev_pitch = track.blocks[-1].notes[-1].pitch
    else:
        prev_pitch = 60 + project.key_root_pc + 12

    rhythm = _build_rhythm(rng, length, activity)
    notes: List[Note] = []
    for offset, dur in rhythm:
        beat = start_beat + offset
        seg = next((s for s in segments if s.start_beat <= beat < s.end_beat),
                   segments[0] if segments else None)
        if seg is None:
            chord_pcs = [project.key_root_pc]
        else:
            chord_pcs = [(seg.root_pc + i) % 12
                         for i in CHORD_INTERVALS[seg.quality]]
        leap_prob = 0.10 + 0.18 * activity
        pitch = _step_or_leap(prev_pitch, scale, chord_pcs, rng, leap_prob)

        on_strong = abs(offset - round(offset)) < 0.05 and round(offset) % 2 == 0
        if on_strong and pitch % 12 not in chord_pcs and rng.random() > 0.4:
            close_chord = [p for p in scale
                           if p % 12 in chord_pcs and abs(p - pitch) <= 4]
            if close_chord:
                pitch = min(close_chord, key=lambda p: abs(p - pitch))

        velocity = 78 + int(rng.uniform(-6, 14))
        if on_strong:
            velocity += 6
        notes.append(Note(beat, dur, pitch, max(40, min(120, velocity))))
        prev_pitch = pitch

    # End on a chord tone half the time for a settled feel.
    if notes and segments and rng.random() < 0.55:
        last_seg = segments[-1]
        chord_pcs = [(last_seg.root_pc + i) % 12
                     for i in CHORD_INTERVALS[last_seg.quality]]
        last = notes[-1]
        target = _nearest_pitch(last.pitch,
                                [p for p in scale if p % 12 in chord_pcs])
        notes[-1] = Note(last.start_beat, last.duration_beats,
                         target, last.velocity)

    return Block(start_beat=start_beat, length_beats=length,
                 notes=notes, bars=bars)


def generate_bass_block(project: Project, track: Track,
                        start_beat: float, bars: int,
                        activity: float, rng: random.Random) -> Block:
    length = bars * project.bar_beats
    segments = project.chords_covering(start_beat, start_beat + length)
    notes: List[Note] = []
    for seg in segments:
        s = max(seg.start_beat, start_beat)
        e = min(seg.end_beat, start_beat + length)
        seg_len = e - s
        if seg_len <= 0:
            continue
        root = 36 + seg.root_pc
        fifth = root + 7
        if activity < 0.4:
            notes.append(Note(s, seg_len, root, 88))
        elif activity < 0.7:
            half = seg_len / 2
            notes.append(Note(s, half, root, 92))
            if seg_len >= 1.5:
                notes.append(Note(s + half, seg_len - half, fifth, 86))
        else:
            step = max(0.5, seg_len / 4)
            pitches = [root, fifth, root + 12, fifth]
            t = s
            i = 0
            while t < e - 1e-6:
                d = min(step, e - t)
                notes.append(Note(t, d, pitches[i % 4], 90))
                t += d
                i += 1
    return Block(start_beat=start_beat, length_beats=length,
                 notes=notes, bars=bars)


def generate_pad_block(project: Project, track: Track,
                       start_beat: float, bars: int,
                       activity: float, rng: random.Random) -> Block:
    length = bars * project.bar_beats
    segments = project.chords_covering(start_beat, start_beat + length)
    notes: List[Note] = []
    for seg in segments:
        s = max(seg.start_beat, start_beat)
        e = min(seg.end_beat, start_beat + length)
        if e <= s:
            continue
        for p in chord_pitches(seg.root_pc, seg.quality, octave=4):
            notes.append(Note(s, e - s, p, 64))
    return Block(start_beat=start_beat, length_beats=length,
                 notes=notes, bars=bars)


def generate_arp_block(project: Project, track: Track,
                       start_beat: float, bars: int,
                       activity: float, rng: random.Random) -> Block:
    length = bars * project.bar_beats
    segments = project.chords_covering(start_beat, start_beat + length)
    step = 0.5 if activity < 0.5 else 0.25
    direction = rng.choice([1, -1, 1])
    notes: List[Note] = []
    for seg in segments:
        s = max(seg.start_beat, start_beat)
        e = min(seg.end_beat, start_beat + length)
        pitches = chord_pitches(seg.root_pc, seg.quality, octave=4)
        if direction < 0:
            pitches = list(reversed(pitches))
        i = 0
        t = s
        while t < e - 1e-6:
            d = min(step, e - t)
            notes.append(Note(t, d, pitches[i % len(pitches)], 80))
            t += d
            i += 1
    return Block(start_beat=start_beat, length_beats=length,
                 notes=notes, bars=bars)


GENERATORS: dict[str, Callable[..., Block]] = {
    "melody": generate_melody_block,
    "bass": generate_bass_block,
    "pad": generate_pad_block,
    "arp": generate_arp_block,
}


def score_block(block: Block, prev: Optional[Block]) -> float:
    if not block.notes:
        return -1e9
    score = 0.0
    pitches = [n.pitch for n in block.notes]
    rng_p = max(pitches) - min(pitches)
    score += 1.0 - abs(rng_p - 9) / 12
    leaps = sum(1 for a, b in zip(pitches, pitches[1:]) if abs(a - b) > 7)
    score -= 0.4 * leaps
    if prev and prev.notes:
        gap = abs(pitches[0] - prev.notes[-1].pitch)
        score += 1.2 - gap / 6
    return score


# ============================================================================
# 5. FluidSynth playback
# ============================================================================

class FluidPlayer:

    def __init__(self):
        self.fs = None
        self.sfid = None
        self.soundfont_path: Optional[str] = None
        self._stop = threading.Event()
        self._thread: Optional[threading.Thread] = None

    def init(self, soundfont_path: str) -> bool:
        if not HAVE_FLUIDSYNTH:
            return False
        try:
            self.fs = fluidsynth.Synth()
            for drv in ("dsound", "wasapi", "portaudio", None):
                try:
                    if drv is None:
                        self.fs.start()
                    else:
                        self.fs.start(driver=drv)
                    break
                except Exception:
                    continue
            self.sfid = self.fs.sfload(soundfont_path)
            self.soundfont_path = soundfont_path
            return True
        except Exception as exc:
            print("FluidSynth init failed:", exc)
            self.fs = None
            return False

    def set_program(self, channel: int, program: int) -> None:
        if self.fs is None or self.sfid is None:
            return
        try:
            self.fs.program_select(channel, self.sfid, 0, program)
        except Exception:
            pass

    def stop(self) -> None:
        self._stop.set()
        if self._thread and self._thread.is_alive():
            self._thread.join(timeout=0.5)
        self._all_notes_off()

    def _all_notes_off(self) -> None:
        if self.fs is None:
            return
        for ch in range(16):
            try:
                self.fs.cc(ch, 123, 0)
            except Exception:
                pass

    def play_events(self, events: List[Tuple[float, str, int, int, int]],
                    bpm: int) -> None:
        if self.fs is None or not events:
            return
        self.stop()
        self._stop = threading.Event()
        events = sorted(events,
                        key=lambda e: (e[0], 0 if e[1] == "off" else 1))
        beat_seconds = 60.0 / max(20, bpm)

        def runner():
            t0 = time.monotonic()
            for t, kind, ch, pitch, vel in events:
                if self._stop.is_set():
                    break
                target = t0 + t * beat_seconds
                while True:
                    now = time.monotonic()
                    if now >= target or self._stop.is_set():
                        break
                    time.sleep(min(0.01, target - now))
                if self._stop.is_set():
                    break
                try:
                    if kind == "on":
                        self.fs.noteon(ch, pitch, vel)
                    else:
                        self.fs.noteoff(ch, pitch)
                except Exception:
                    pass
            self._all_notes_off()

        self._thread = threading.Thread(target=runner, daemon=True)
        self._thread.start()


def block_to_events(block: Block, channel: int
                    ) -> List[Tuple[float, str, int, int, int]]:
    out: List[Tuple[float, str, int, int, int]] = []
    base = block.notes[0].start_beat if block.notes else 0.0
    for n in block.notes:
        t = n.start_beat - base
        out.append((t, "on", channel, n.pitch, n.velocity))
        out.append((t + n.duration_beats, "off", channel, n.pitch, 0))
    return out


def progression_to_events(segments: List[ChordSegment],
                          channel: int = 0,
                          program_setter=None) -> List[Tuple[float, str, int, int, int]]:
    out: List[Tuple[float, str, int, int, int]] = []
    if not segments:
        return out
    base = segments[0].start_beat
    for seg in segments:
        for p in chord_pitches(seg.root_pc, seg.quality, octave=4):
            t = seg.start_beat - base
            out.append((t, "on", channel, p, 76))
            out.append((t + seg.length_beats, "off", channel, p, 0))
    return out


def project_to_events(project: Project) -> List[Tuple[float, str, int, int, int]]:
    out: List[Tuple[float, str, int, int, int]] = []
    for tr in project.tracks:
        if tr.muted:
            continue
        for n in tr.all_notes():
            out.append((n.start_beat, "on", tr.channel, n.pitch, n.velocity))
            out.append((n.start_beat + n.duration_beats, "off",
                        tr.channel, n.pitch, 0))
    return out


# ============================================================================
# 6. MIDI export
# ============================================================================

def export_midi(project: Project, path: str) -> None:
    if not HAVE_MIDO:
        raise RuntimeError("Le module 'mido' n'est pas installe.")
    tpb = 480
    mid = MidiFile(ticks_per_beat=tpb)

    meta = MidiTrack()
    mid.tracks.append(meta)
    meta.append(MetaMessage("set_tempo",
                            tempo=mido.bpm2tempo(project.tempo_bpm), time=0))
    meta.append(MetaMessage("time_signature",
                            numerator=project.time_sig_num,
                            denominator=project.time_sig_den, time=0))

    for tr in project.tracks:
        mt = MidiTrack()
        mid.tracks.append(mt)
        mt.append(MetaMessage("track_name", name=tr.name, time=0))
        mt.append(Message("program_change", channel=tr.channel,
                          program=tr.program, time=0))
        events: List[Tuple[int, int, str, int, int]] = []
        for n in tr.all_notes():
            on_tick = max(0, int(round(n.start_beat * tpb)))
            off_tick = max(on_tick + 1,
                           int(round((n.start_beat + n.duration_beats) * tpb)))
            events.append((on_tick, 1, "on", n.pitch, n.velocity))
            events.append((off_tick, 0, "off", n.pitch, 0))
        events.sort(key=lambda e: (e[0], e[1]))
        last = 0
        for tick, _, kind, pitch, vel in events:
            delta = tick - last
            last = tick
            mt.append(Message("note_on" if kind == "on" else "note_off",
                              channel=tr.channel, note=pitch,
                              velocity=vel, time=delta))
    mid.save(path)


# ============================================================================
# 7. Save / load
# ============================================================================

def project_to_dict(p: Project) -> dict:
    return {
        "key_root": p.key_root, "mode": p.mode,
        "tempo_bpm": p.tempo_bpm,
        "time_sig_num": p.time_sig_num, "time_sig_den": p.time_sig_den,
        "progression_length_bars": p.progression_length_bars,
        "progression_pattern": p.progression_pattern,
        "chords": [c.__dict__ for c in p.chords],
        "tracks": [
            {"name": t.name, "role": t.role, "program": t.program,
             "channel": t.channel, "color": t.color, "muted": t.muted,
             "blocks": [
                 {"start_beat": b.start_beat,
                  "length_beats": b.length_beats,
                  "bars": b.bars,
                  "notes": [n.__dict__ for n in b.notes]}
                 for b in t.blocks]
             } for t in p.tracks],
    }


def project_from_dict(d: dict) -> Project:
    p = Project(
        key_root=d.get("key_root", "C"),
        mode=d.get("mode", "major"),
        tempo_bpm=d.get("tempo_bpm", 100),
        time_sig_num=d.get("time_sig_num", 4),
        time_sig_den=d.get("time_sig_den", 4),
        progression_length_bars=d.get("progression_length_bars", 4),
        progression_pattern=d.get("progression_pattern", []),
    )
    p.chords = [ChordSegment(**c) for c in d.get("chords", [])]
    for td in d.get("tracks", []):
        tr = Track(name=td["name"], role=td["role"],
                   program=td["program"], channel=td["channel"],
                   color=td.get("color", "#3aa3ff"),
                   muted=td.get("muted", False))
        for bd in td.get("blocks", []):
            tr.blocks.append(Block(
                start_beat=bd["start_beat"],
                length_beats=bd["length_beats"],
                bars=bd.get("bars", 1),
                notes=[Note(**n) for n in bd.get("notes", [])],
            ))
        p.tracks.append(tr)
    return p


# ============================================================================
# 8. UI
# ============================================================================

GM_PRESETS = [
    ("Piano", 0), ("Electric Piano", 4), ("Music Box", 10),
    ("Vibraphone", 11), ("Marimba", 12), ("Church Organ", 19),
    ("Acoustic Guitar", 24), ("Electric Guitar (clean)", 27),
    ("Acoustic Bass", 32), ("Electric Bass (finger)", 33),
    ("Synth Bass 1", 38), ("Violin", 40), ("Viola", 41),
    ("Cello", 42), ("Strings Ensemble", 48), ("Slow Strings", 49),
    ("Synth Strings 1", 50), ("Choir Aahs", 52), ("Voice Oohs", 53),
    ("Trumpet", 56), ("Trombone", 57), ("French Horn", 60),
    ("Brass Section", 61), ("Soprano Sax", 64), ("Alto Sax", 65),
    ("Tenor Sax", 66), ("Oboe", 68), ("Clarinet", 71),
    ("Flute", 73), ("Recorder", 74), ("Pan Flute", 75),
    ("Pad 1 (new age)", 88), ("Pad 2 (warm)", 89),
    ("Pad 3 (polysynth)", 90), ("Pad 4 (choir)", 91),
    ("Pad 8 (sweep)", 95), ("Harp", 46),
]

ROLE_PRESET_HINTS = {
    "melody": ["Flute", "Oboe", "Violin", "Trumpet", "Piano"],
    "bass": ["Acoustic Bass", "Electric Bass (finger)", "Cello"],
    "pad": ["Strings Ensemble", "Slow Strings", "Pad 2 (warm)", "Choir Aahs"],
    "arp": ["Harp", "Music Box", "Vibraphone", "Pad 3 (polysynth)"],
}

TRACK_COLORS = ["#3aa3ff", "#ff7a45", "#52c41a", "#b37feb",
                "#faad14", "#13c2c2", "#eb2f96", "#a0d911"]

DEFAULT_SOUNDFONT_CANDIDATES = [
    r"soundfonts\GeneralUser GS 1.472\GeneralUser GS v1.472.sf2",
    r"soundfonts\GeneralUser_GS_1.472\GeneralUser GS v1.472.sf2",
    r"soundfonts\GeneralUser GS v1.472.sf2",
]

TIME_SIGNATURES = ["2/4", "3/4", "4/4", "5/4", "6/4", "6/8", "7/8", "9/8", "12/8"]


# ----- shared candidate card -----

class CandidateCard(ttk.Frame):
    """Compact card showing one candidate (block or progression)."""

    def __init__(self, master, app: "MelodyMakerApp", index: int):
        super().__init__(master, padding=4)
        self.app = app
        self.index = index
        self.payload = None  # block or list of degrees, set by callers
        self.kind: str = "none"  # 'block' or 'progression'

        self.title_var = tk.StringVar(value=f"#{index + 1}")
        self.info_var = tk.StringVar(value="")

        ttk.Label(self, textvariable=self.title_var,
                  font=("Segoe UI", 9, "bold")).pack(anchor="w")
        self.canvas = tk.Canvas(self, height=80, width=260,
                                bg="#101820", highlightthickness=0)
        self.canvas.pack(fill="x")
        ttk.Label(self, textvariable=self.info_var,
                  font=("Segoe UI", 8)).pack(anchor="w")
        btns = ttk.Frame(self)
        btns.pack(anchor="w", pady=2)
        ttk.Button(btns, text="Play", width=6,
                   command=self._play).pack(side="left", padx=2)
        ttk.Button(btns, text="Valider", width=8,
                   command=self._validate).pack(side="left", padx=2)

    def set_block(self, block: Optional[Block]) -> None:
        self.kind = "block"
        self.payload = block
        self.canvas.delete("all")
        if block is None:
            self.info_var.set("(vide)")
            return
        self.info_var.set(f"{block.bars} mes. - "
                          f"{len(block.notes)} notes")
        self._draw_block(block)

    def set_progression(self, degrees: List[int]) -> None:
        self.kind = "progression"
        self.payload = degrees
        self.canvas.delete("all")
        labels = [chord_label(*degree_to_chord(d, self.app.project.key_root_pc,
                                               self.app.project.mode))
                  for d in degrees]
        self.info_var.set(" - ".join(labels))
        self._draw_progression(degrees)

    def _draw_block(self, block: Block) -> None:
        c = self.canvas
        w = int(c.cget("width"))
        h = int(c.cget("height"))
        if not block.notes:
            return
        pitches = [n.pitch for n in block.notes]
        pmin, pmax = min(pitches) - 2, max(pitches) + 2
        if pmax - pmin < 6:
            pmax = pmin + 6
        start = block.start_beat
        end = block.end_beat
        span = max(0.5, end - start)
        for n in block.notes:
            x1 = (n.start_beat - start) / span * (w - 4) + 2
            x2 = (n.start_beat + n.duration_beats - start) / span * (w - 4) + 2
            y = (pmax - n.pitch) / (pmax - pmin) * (h - 4) + 2
            c.create_rectangle(x1, y - 2, x2, y + 2,
                               fill=self.app.current_track_color(),
                               outline="")

    def _draw_progression(self, degrees: List[int]) -> None:
        c = self.canvas
        w = int(c.cget("width"))
        h = int(c.cget("height"))
        proj = self.app.project
        n = len(degrees)
        if n == 0:
            return
        cell = (w - 4) / n
        for i, d in enumerate(degrees):
            root_pc, quality = degree_to_chord(d, proj.key_root_pc, proj.mode)
            x1 = 2 + i * cell
            x2 = x1 + cell - 2
            color = "#2d5a87" if quality == "maj" else (
                "#7a2d6a" if quality == "min" else "#6a3030")
            c.create_rectangle(x1, 4, x2, h - 18, fill=color, outline="#4a6b85")
            c.create_text((x1 + x2) / 2, (h - 18) / 2 + 2,
                          text=chord_label(root_pc, quality),
                          fill="#e8eef5", font=("Segoe UI", 10, "bold"))
            c.create_text((x1 + x2) / 2, h - 8,
                          text=f"deg {d}", fill="#8aa8be",
                          font=("Segoe UI", 7))

    def _play(self) -> None:
        if self.kind == "block" and self.payload is not None:
            self.app.play_block(self.payload)
        elif self.kind == "progression" and self.payload is not None:
            self.app.play_progression(self.payload)

    def _validate(self) -> None:
        if self.kind == "block":
            self.app.validate_block_candidate(self.index)
        elif self.kind == "progression":
            self.app.validate_progression_candidate(self.index)


# ============================================================================
# 9. Main app
# ============================================================================

class MelodyMakerApp:

    def __init__(self, root: tk.Tk):
        self.root = root
        self.root.title("Melody Maker - composition pas a pas")
        self.root.geometry("1320x880")

        self.project = Project()
        self.rng = random.Random()
        self.player = FluidPlayer()

        # Candidates state
        self.progression_candidates: List[List[int]] = []
        self.block_candidates: List[Block] = []
        self.active_card_kind: str = "none"

        self.active_track_index: Optional[int] = None

        # Vars
        self.key_var = tk.StringVar(value="C")
        self.mode_var = tk.StringVar(value="major")
        self.timesig_var = tk.StringVar(value="4/4")
        self.tempo_var = tk.IntVar(value=100)
        self.prog_length_var = tk.IntVar(value=4)
        self.block_bars_var = tk.IntVar(value=2)
        self.activity_var = tk.DoubleVar(value=0.5)
        self.num_candidates_var = tk.IntVar(value=5)
        self.status_var = tk.StringVar(value="Pret.")

        self._build_ui()
        self._auto_init_audio()

    # --- Audio init ---
    def _auto_init_audio(self) -> None:
        if not HAVE_FLUIDSYNTH:
            self.status_var.set("pyfluidsynth absent : preview audio desactivee.")
            return
        for cand in DEFAULT_SOUNDFONT_CANDIDATES:
            if os.path.isfile(cand) and self.player.init(cand):
                self.status_var.set(f"SoundFont chargee : {cand}")
                return
        self.status_var.set("SoundFont non trouvee. Menu Audio -> Choisir SoundFont.")

    def _choose_soundfont(self) -> None:
        path = filedialog.askopenfilename(
            title="Choisir une SoundFont",
            filetypes=[("SoundFont", "*.sf2 *.sf3"), ("Tous", "*.*")])
        if not path:
            return
        if self.player.init(path):
            self.status_var.set(f"SoundFont chargee : {path}")
            for tr in self.project.tracks:
                self.player.set_program(tr.channel, tr.program)
        else:
            messagebox.showerror("FluidSynth",
                                 "Impossible d'initialiser FluidSynth.")

    # --- UI build ---
    def _build_ui(self) -> None:
        # Menu
        menubar = tk.Menu(self.root)
        filemenu = tk.Menu(menubar, tearoff=0)
        filemenu.add_command(label="Nouveau projet", command=self.new_project)
        filemenu.add_command(label="Charger projet (JSON)", command=self.load_project)
        filemenu.add_command(label="Sauver projet (JSON)", command=self.save_project)
        filemenu.add_separator()
        filemenu.add_command(label="Exporter MIDI...", command=self.export_midi_dialog)
        filemenu.add_separator()
        filemenu.add_command(label="Quitter", command=self.root.destroy)
        menubar.add_cascade(label="Fichier", menu=filemenu)
        audiomenu = tk.Menu(menubar, tearoff=0)
        audiomenu.add_command(label="Choisir SoundFont...", command=self._choose_soundfont)
        audiomenu.add_command(label="Stop audio", command=self.stop_audio)
        menubar.add_cascade(label="Audio", menu=audiomenu)
        self.root.config(menu=menubar)

        # ========== TOP: project settings ==========
        top = ttk.LabelFrame(self.root, text="1. Reglages du morceau", padding=6)
        top.pack(fill="x", padx=4, pady=4)

        ttk.Label(top, text="Tonalite").grid(row=0, column=0, padx=2)
        ttk.Combobox(top, textvariable=self.key_var, width=4,
                     values=NOTE_NAMES, state="readonly").grid(row=0, column=1, padx=2)
        ttk.Label(top, text="Mode").grid(row=0, column=2, padx=2)
        ttk.Combobox(top, textvariable=self.mode_var, width=10,
                     values=list(SCALE_INTERVALS.keys()),
                     state="readonly").grid(row=0, column=3, padx=2)
        ttk.Label(top, text="Signature").grid(row=0, column=4, padx=2)
        ttk.Combobox(top, textvariable=self.timesig_var, width=6,
                     values=TIME_SIGNATURES, state="readonly").grid(row=0, column=5, padx=2)
        ttk.Label(top, text="Tempo").grid(row=0, column=6, padx=2)
        ttk.Spinbox(top, from_=40, to=220, textvariable=self.tempo_var,
                    width=5).grid(row=0, column=7, padx=2)
        ttk.Button(top, text="Appliquer reglages",
                   command=self.apply_project_settings).grid(row=0, column=8, padx=8)
        ttk.Button(top, text="Play tout",
                   command=self.play_full).grid(row=0, column=9, padx=2)
        ttk.Button(top, text="Stop",
                   command=self.stop_audio).grid(row=0, column=10, padx=2)

        # ========== MIDDLE: progression ==========
        prog = ttk.LabelFrame(self.root,
                              text="2. Progression de reference",
                              padding=6)
        prog.pack(fill="x", padx=4, pady=4)

        row = ttk.Frame(prog)
        row.pack(fill="x")
        ttk.Label(row, text="Longueur (mesures)").pack(side="left", padx=2)
        ttk.Combobox(row, textvariable=self.prog_length_var, width=4,
                     values=[2, 4, 8], state="readonly").pack(side="left", padx=2)
        ttk.Label(row, text="Nb candidats").pack(side="left", padx=(8, 2))
        ttk.Spinbox(row, from_=2, to=8, textvariable=self.num_candidates_var,
                    width=4).pack(side="left", padx=2)
        ttk.Button(row, text="Generer progressions candidates",
                   command=self.generate_progression_candidates).pack(side="left", padx=10)
        self.committed_prog_label = ttk.Label(
            row, text="(aucune progression validee)",
            foreground="#7a8b9b")
        self.committed_prog_label.pack(side="left", padx=10)

        prog_cards_frame = ttk.Frame(prog)
        prog_cards_frame.pack(fill="x", pady=4)
        self.progression_cards: List[CandidateCard] = []
        for i in range(8):
            card = CandidateCard(prog_cards_frame, self, i)
            card.grid(row=i // 4, column=i % 4, padx=2, pady=2, sticky="w")
            card.grid_remove()
            self.progression_cards.append(card)

        # ========== BOTTOM: tracks + roll ==========
        main = ttk.Panedwindow(self.root, orient="horizontal")
        main.pack(fill="both", expand=True, padx=4, pady=4)

        left = ttk.LabelFrame(main, text="3. Pistes", padding=4)
        main.add(left, weight=1)
        self.tracks_listbox = tk.Listbox(left, height=14, exportselection=False)
        self.tracks_listbox.pack(fill="both", expand=True)
        self.tracks_listbox.bind("<<ListboxSelect>>", self._on_track_select)
        track_btns = ttk.Frame(left)
        track_btns.pack(fill="x", pady=4)
        ttk.Button(track_btns, text="+ Piste",
                   command=self.add_track_dialog).pack(side="left", padx=2)
        ttk.Button(track_btns, text="Mute",
                   command=self.toggle_mute).pack(side="left", padx=2)
        ttk.Button(track_btns, text="Suppr.",
                   command=self.delete_track).pack(side="left", padx=2)
        ttk.Button(track_btns, text="Annuler bloc",
                   command=self.undo_last_block).pack(side="left", padx=2)

        center = ttk.Frame(main, padding=4)
        main.add(center, weight=4)

        ttk.Label(center, text="Vue d'ensemble (piano roll)",
                  font=("Segoe UI", 10, "bold")).pack(anchor="w")
        roll_frame = ttk.Frame(center)
        roll_frame.pack(fill="both", expand=True)
        self.h_scroll = ttk.Scrollbar(roll_frame, orient="horizontal")
        self.h_scroll.pack(side="bottom", fill="x")
        self.v_scroll = ttk.Scrollbar(roll_frame, orient="vertical")
        self.v_scroll.pack(side="right", fill="y")
        self.roll_canvas = tk.Canvas(
            roll_frame, bg="#0a0f14", highlightthickness=0,
            xscrollcommand=self.h_scroll.set,
            yscrollcommand=self.v_scroll.set)
        self.roll_canvas.pack(side="left", fill="both", expand=True)
        self.h_scroll.config(command=self.roll_canvas.xview)
        self.v_scroll.config(command=self.roll_canvas.yview)

        gen = ttk.LabelFrame(center, text="4. Prochain bloc de la piste active",
                             padding=4)
        gen.pack(fill="x", pady=4)
        params = ttk.Frame(gen)
        params.pack(fill="x")
        ttk.Label(params, text="Longueur (mesures)").pack(side="left", padx=2)
        ttk.Combobox(params, textvariable=self.block_bars_var, width=4,
                     values=[1, 2, 4], state="readonly").pack(side="left", padx=2)
        ttk.Label(params, text="Activite").pack(side="left", padx=(8, 2))
        ttk.Scale(params, variable=self.activity_var, from_=0, to=1,
                  length=160).pack(side="left", padx=2)
        ttk.Label(params, text="Nb candidats").pack(side="left", padx=(8, 2))
        ttk.Spinbox(params, from_=2, to=8,
                    textvariable=self.num_candidates_var,
                    width=4).pack(side="left", padx=2)
        ttk.Button(params, text="Generer candidats de bloc",
                   command=self.generate_block_candidates).pack(side="left", padx=10)
        # quick help on Activite
        ttk.Label(params,
                  text="(Activite : a gauche = peu de notes longues, "
                       "a droite = beaucoup de notes courtes)",
                  foreground="#7a8b9b").pack(side="left", padx=8)

        cards_frame = ttk.Frame(gen)
        cards_frame.pack(fill="x", pady=4)
        self.block_cards: List[CandidateCard] = []
        for i in range(8):
            card = CandidateCard(cards_frame, self, i)
            card.grid(row=i // 4, column=i % 4, padx=2, pady=2, sticky="w")
            card.grid_remove()
            self.block_cards.append(card)

        ttk.Label(self.root, textvariable=self.status_var,
                  relief="sunken", anchor="w").pack(fill="x", side="bottom")

    # ------------------------------------------------------------------ utils
    def current_track_color(self) -> str:
        tr = self._active_track()
        return tr.color if tr else "#3aa3ff"

    def _active_track(self) -> Optional[Track]:
        if self.active_track_index is None:
            return None
        if 0 <= self.active_track_index < len(self.project.tracks):
            return self.project.tracks[self.active_track_index]
        return None

    def _parse_timesig(self) -> Tuple[int, int]:
        s = self.timesig_var.get()
        try:
            n, d = s.split("/")
            return int(n), int(d)
        except Exception:
            return 4, 4

    # ------------------------------------------------------------------ project
    def apply_project_settings(self) -> None:
        # If tracks already have content, warn before resetting progression.
        had_content = any(t.blocks for t in self.project.tracks) \
            or self.project.progression_pattern
        n, d = self._parse_timesig()
        self.project.key_root = self.key_var.get()
        self.project.mode = self.mode_var.get()
        self.project.tempo_bpm = int(self.tempo_var.get())
        self.project.time_sig_num = n
        self.project.time_sig_den = d
        self.project.progression_length_bars = int(self.prog_length_var.get())
        # Reset progression and clear track content (it depends on chords).
        self.project.progression_pattern = []
        self.project.chords = []
        for tr in self.project.tracks:
            tr.blocks = []
        self._refresh_committed_progression_label()
        self._refresh_tracks()
        self._clear_progression_candidates()
        self._clear_block_candidates()
        self._redraw_roll()
        if had_content:
            self.status_var.set("Reglages appliques. Progression et "
                                "blocs reinitialises.")
        else:
            self.status_var.set("Reglages appliques.")

    def new_project(self) -> None:
        if not messagebox.askyesno("Nouveau projet",
                                    "Reinitialiser le projet courant ?"):
            return
        self.project = Project()
        self.active_track_index = None
        self._clear_progression_candidates()
        self._clear_block_candidates()
        self._refresh_tracks()
        self._refresh_committed_progression_label()
        self._redraw_roll()
        self.status_var.set("Nouveau projet.")

    def save_project(self) -> None:
        path = filedialog.asksaveasfilename(
            defaultextension=".json",
            filetypes=[("Projet melody_maker", "*.json")])
        if not path:
            return
        with open(path, "w", encoding="utf-8") as f:
            json.dump(project_to_dict(self.project), f, indent=2)
        self.status_var.set(f"Projet sauve : {path}")

    def load_project(self) -> None:
        path = filedialog.askopenfilename(
            filetypes=[("Projet melody_maker", "*.json")])
        if not path:
            return
        with open(path, "r", encoding="utf-8") as f:
            self.project = project_from_dict(json.load(f))
        self.key_var.set(self.project.key_root)
        self.mode_var.set(self.project.mode)
        self.tempo_var.set(self.project.tempo_bpm)
        self.timesig_var.set(f"{self.project.time_sig_num}/"
                              f"{self.project.time_sig_den}")
        self.prog_length_var.set(self.project.progression_length_bars)
        self.active_track_index = None
        if self.player.fs is not None:
            for tr in self.project.tracks:
                self.player.set_program(tr.channel, tr.program)
        self._refresh_tracks()
        self._refresh_committed_progression_label()
        self._redraw_roll()
        self.status_var.set(f"Projet charge : {path}")

    def export_midi_dialog(self) -> None:
        if not HAVE_MIDO:
            messagebox.showerror("Export MIDI",
                                 "Le module 'mido' n'est pas installe.")
            return
        path = filedialog.asksaveasfilename(
            defaultextension=".mid",
            filetypes=[("MIDI", "*.mid")])
        if not path:
            return
        export_midi(self.project, path)
        self.status_var.set(f"MIDI exporte : {path}")

    # ------------------------------------------------------------------ progression
    def _refresh_committed_progression_label(self) -> None:
        p = self.project
        if not p.progression_pattern:
            self.committed_prog_label.config(
                text="(aucune progression validee)",
                foreground="#7a8b9b")
            return
        labels = [chord_label(*degree_to_chord(d, p.key_root_pc, p.mode))
                  for d in p.progression_pattern]
        self.committed_prog_label.config(
            text="Progression validee : " + " - ".join(labels),
            foreground="#cfe5ff")

    def generate_progression_candidates(self) -> None:
        # Make sure project settings are committed.
        self.project.progression_length_bars = int(self.prog_length_var.get())
        engine = ProgressionEngine(self.project, self.rng)
        n = max(1, min(8, int(self.num_candidates_var.get())))
        self.progression_candidates = engine.propose_candidates(n)
        for i, card in enumerate(self.progression_cards):
            if i < len(self.progression_candidates):
                card.set_progression(self.progression_candidates[i])
                card.grid()
            else:
                card.set_block(None)
                card.grid_remove()
        self.active_card_kind = "progression"
        self.status_var.set(f"{len(self.progression_candidates)} progressions "
                            f"candidates generees.")

    def _clear_progression_candidates(self) -> None:
        self.progression_candidates = []
        for card in self.progression_cards:
            card.set_block(None)
            card.grid_remove()

    def validate_progression_candidate(self, index: int) -> None:
        if index >= len(self.progression_candidates):
            return
        engine = ProgressionEngine(self.project, self.rng)
        engine.commit(self.progression_candidates[index])
        # Reset all track blocks since chords just changed.
        for tr in self.project.tracks:
            tr.blocks = []
        self._clear_progression_candidates()
        self._clear_block_candidates()
        self._refresh_committed_progression_label()
        self._refresh_tracks()
        self._redraw_roll()
        self.status_var.set(
            "Progression validee. Tu peux maintenant generer des blocs.")

    def play_progression(self, degrees: List[int]) -> None:
        if self.player.fs is None:
            self.status_var.set("Audio indisponible.")
            return
        # Use channel 15 with a pad sound to preview chords.
        ch = 15
        self.player.set_program(ch, 89)  # Pad 2 (warm)
        segs = progression_to_blocks(degrees, self.project)
        events = progression_to_events(segs, channel=ch)
        self.player.play_events(events, self.project.tempo_bpm)

    # ------------------------------------------------------------------ tracks
    def add_track_dialog(self) -> None:
        dlg = tk.Toplevel(self.root)
        dlg.title("Nouvelle piste")
        dlg.transient(self.root)
        dlg.grab_set()
        ttk.Label(dlg, text="Nom").grid(row=0, column=0, sticky="w", padx=4, pady=2)
        name_var = tk.StringVar(value=f"Piste {len(self.project.tracks) + 1}")
        ttk.Entry(dlg, textvariable=name_var, width=24).grid(row=0, column=1)
        ttk.Label(dlg, text="Role").grid(row=1, column=0, sticky="w", padx=4, pady=2)
        role_var = tk.StringVar(value="melody")
        role_cb = ttk.Combobox(dlg, textvariable=role_var,
                               values=list(GENERATORS.keys()),
                               state="readonly", width=22)
        role_cb.grid(row=1, column=1)
        ttk.Label(dlg, text="Instrument").grid(row=2, column=0, sticky="w", padx=4, pady=2)
        preset_var = tk.StringVar(value="Piano")
        preset_cb = ttk.Combobox(dlg, textvariable=preset_var,
                                 values=[n for n, _ in GM_PRESETS],
                                 state="readonly", width=22)
        preset_cb.grid(row=2, column=1)

        def on_role_change(_evt=None):
            hints = ROLE_PRESET_HINTS.get(role_var.get(), [])
            if hints:
                preset_var.set(hints[0])
        role_cb.bind("<<ComboboxSelected>>", on_role_change)
        on_role_change()

        def confirm():
            program = next((p for n, p in GM_PRESETS
                            if n == preset_var.get()), 0)
            channel = len(self.project.tracks) % 16
            if channel == 9:
                channel = (channel + 1) % 16
            color = TRACK_COLORS[len(self.project.tracks) % len(TRACK_COLORS)]
            tr = Track(name=name_var.get().strip() or "Piste",
                       role=role_var.get(), program=program,
                       channel=channel, color=color)
            self.project.tracks.append(tr)
            if self.player.fs is not None:
                self.player.set_program(channel, program)
            self._refresh_tracks()
            self.tracks_listbox.selection_clear(0, "end")
            self.tracks_listbox.selection_set(len(self.project.tracks) - 1)
            self._on_track_select()
            dlg.destroy()

        ttk.Button(dlg, text="OK", command=confirm).grid(
            row=3, column=0, columnspan=2, pady=6)
        dlg.wait_window()

    def _refresh_tracks(self) -> None:
        self.tracks_listbox.delete(0, "end")
        for i, tr in enumerate(self.project.tracks):
            preset = next((n for n, p in GM_PRESETS if p == tr.program),
                          str(tr.program))
            mute = " [M]" if tr.muted else ""
            self.tracks_listbox.insert(
                "end",
                f"{i + 1}. {tr.name} - {tr.role} ({preset}){mute} "
                f"@ {tr.cursor_bars} mes.")
        if self.active_track_index is not None \
                and 0 <= self.active_track_index < len(self.project.tracks):
            self.tracks_listbox.selection_set(self.active_track_index)

    def _on_track_select(self, _evt=None) -> None:
        sel = self.tracks_listbox.curselection()
        if not sel:
            self.active_track_index = None
        else:
            self.active_track_index = sel[0]
        self._clear_block_candidates()
        self._redraw_roll()

    def toggle_mute(self) -> None:
        tr = self._active_track()
        if tr is None:
            return
        tr.muted = not tr.muted
        self._refresh_tracks()

    def delete_track(self) -> None:
        if self.active_track_index is None:
            return
        del self.project.tracks[self.active_track_index]
        self.active_track_index = None
        self._refresh_tracks()
        self._redraw_roll()
        self._clear_block_candidates()

    def undo_last_block(self) -> None:
        tr = self._active_track()
        if tr and tr.blocks:
            tr.blocks.pop()
            self._refresh_tracks()
            self._redraw_roll()

    # ------------------------------------------------------------------ blocks
    def generate_block_candidates(self) -> None:
        tr = self._active_track()
        if tr is None:
            messagebox.showinfo("Generation",
                                "Selectionne une piste (ou cree-en une).")
            return
        if not self.project.progression_pattern:
            messagebox.showinfo("Generation",
                                "Valide d'abord une progression de reference.")
            return
        gen = GENERATORS[tr.role]
        bars = max(1, int(self.block_bars_var.get()))
        n = max(1, min(8, int(self.num_candidates_var.get())))

        # Make sure the chord progression covers the upcoming bars.
        engine = ProgressionEngine(self.project, self.rng)
        engine.ensure_covers(tr.cursor_beat + bars * self.project.bar_beats + 0.001)

        prev = tr.blocks[-1] if tr.blocks else None
        pool: List[Tuple[float, Block]] = []
        attempts = max(8, n * 3)
        for _ in range(attempts):
            local_rng = random.Random(self.rng.randrange(1 << 30))
            block = gen(self.project, tr, tr.cursor_beat, bars,
                        self.activity_var.get(), local_rng)
            s = score_block(block, prev)
            pool.append((s, block))
        pool.sort(key=lambda x: x[0], reverse=True)
        candidates: List[Block] = []
        for _, b in pool:
            if len(candidates) >= n:
                break
            # Avoid near-duplicates by comparing pitch sequences.
            sig = tuple((round(n_.start_beat, 3), n_.pitch) for n_ in b.notes[:6])
            if not any(
                    sig == tuple((round(n2.start_beat, 3), n2.pitch)
                                 for n2 in c.notes[:6])
                    for c in candidates):
                candidates.append(b)
        if not candidates:
            candidates = [pool[0][1]] if pool else []

        self.block_candidates = candidates
        for i, card in enumerate(self.block_cards):
            if i < len(candidates):
                card.set_block(candidates[i])
                card.grid()
            else:
                card.set_block(None)
                card.grid_remove()
        self.active_card_kind = "block"
        self.status_var.set(
            f"{len(candidates)} blocs candidats pour '{tr.name}' "
            f"({bars} mes. a partir de la mesure {tr.cursor_bars + 1}).")

    def _clear_block_candidates(self) -> None:
        self.block_candidates = []
        for card in self.block_cards:
            card.set_block(None)
            card.grid_remove()

    def validate_block_candidate(self, index: int) -> None:
        tr = self._active_track()
        if tr is None or index >= len(self.block_candidates):
            return
        block = self.block_candidates[index]
        tr.blocks.append(block)
        self._clear_block_candidates()
        self._refresh_tracks()
        self._redraw_roll()
        self.status_var.set(
            f"Bloc valide ({block.bars} mes.). "
            f"Curseur piste = {tr.cursor_bars} mes.")

    # ------------------------------------------------------------------ audio
    def play_block(self, block: Block) -> None:
        tr = self._active_track()
        if self.player.fs is None or tr is None:
            self.status_var.set("Audio indisponible ou pas de piste active.")
            return
        self.player.set_program(tr.channel, tr.program)
        self.player.play_events(block_to_events(block, tr.channel),
                                self.project.tempo_bpm)

    def play_full(self) -> None:
        if self.player.fs is None:
            self.status_var.set("Audio indisponible.")
            return
        for tr in self.project.tracks:
            self.player.set_program(tr.channel, tr.program)
        self.player.play_events(project_to_events(self.project),
                                self.project.tempo_bpm)

    def stop_audio(self) -> None:
        self.player.stop()

    # ------------------------------------------------------------------ piano roll
    def _redraw_roll(self) -> None:
        c = self.roll_canvas
        c.delete("all")
        p = self.project
        bar = p.bar_beats

        total_bars = max(1, sum(b.bars for tr in p.tracks for b in tr.blocks))
        # Show some headroom
        total_beats = max(bar * 4, total_bars * bar + bar)

        pitches: List[int] = []
        for tr in p.tracks:
            for b in tr.blocks:
                for n in b.notes:
                    pitches.append(n.pitch)
        if not pitches:
            pitches = [60]
        pmin = min(pitches) - 4
        pmax = max(pitches) + 4
        if pmax - pmin < 18:
            pmax = pmin + 18

        beat_w = 28
        pitch_h = 10
        width = int(total_beats * beat_w + 80)
        height = (pmax - pmin + 1) * pitch_h + 24

        c.config(scrollregion=(0, 0, width, height))

        # piano background
        for pitch in range(pmin, pmax + 1):
            y = (pmax - pitch) * pitch_h
            color = "#161e26" if (pitch % 12) in {1, 3, 6, 8, 10} else "#1d2731"
            c.create_rectangle(0, y, width, y + pitch_h,
                               fill=color, outline="")
            if pitch % 12 == 0:
                c.create_text(4, y + pitch_h / 2, text=f"C{pitch // 12 - 1}",
                              fill="#6f8395", anchor="w",
                              font=("Segoe UI", 7))

        # bar lines
        beat_count = int(math.ceil(total_beats)) if False else int(total_beats) + 1
        # mark each bar
        nb_bars = int(math.ceil(total_beats / bar)) + 1
        for bi in range(nb_bars + 1):
            x = bi * bar * beat_w + 30
            c.create_line(x, 18, x, height, fill="#2c3a47")
            c.create_text(x + 3, 22, text=f"{bi + 1}", fill="#6a8095",
                          anchor="nw", font=("Segoe UI", 7))

        # subdivisions (every quarter)
        for q in range(int(total_beats) + 1):
            x = q * beat_w + 30
            c.create_line(x, 18, x, height, fill="#1a242e")

        # chord segments header
        for seg in p.chords:
            if seg.start_beat > total_beats:
                continue
            x1 = seg.start_beat * beat_w + 30
            x2 = seg.end_beat * beat_w + 30
            color = "#243341"
            if seg.quality == "min":
                color = "#2e2440"
            elif seg.quality == "dim":
                color = "#3a2424"
            c.create_rectangle(x1, 0, x2, 18, fill=color, outline="#33485a")
            c.create_text((x1 + x2) / 2, 9,
                          text=chord_label(seg.root_pc, seg.quality),
                          fill="#cfe5ff", font=("Segoe UI", 8, "bold"))

        # notes
        for ti, tr in enumerate(p.tracks):
            is_active = (ti == self.active_track_index)
            color = tr.color
            outline = "#ffffff" if is_active else ""
            for b in tr.blocks:
                bx1 = b.start_beat * beat_w + 30
                bx2 = b.end_beat * beat_w + 30
                if is_active:
                    c.create_rectangle(bx1, 18, bx2, height,
                                       fill="", outline="#3a5468",
                                       dash=(3, 3))
                for n in b.notes:
                    x1 = n.start_beat * beat_w + 30
                    x2 = (n.start_beat + n.duration_beats) * beat_w + 30
                    y = (pmax - n.pitch) * pitch_h
                    c.create_rectangle(x1 + 1, y + 1, x2 - 1, y + pitch_h - 1,
                                       fill=color, outline=outline,
                                       width=1 if is_active else 0)

        # cursor
        tr = self._active_track()
        if tr is not None:
            x = tr.cursor_beat * beat_w + 30
            c.create_line(x, 0, x, height, fill="#ff5566", width=2)


# ============================================================================
# 10. Entry point
# ============================================================================


def main() -> None:
    root = tk.Tk()
    try:
        style = ttk.Style()
        if "vista" in style.theme_names():
            style.theme_use("vista")
    except Exception:
        pass
    MelodyMakerApp(root)
    root.mainloop()


if __name__ == "__main__":
    main()
