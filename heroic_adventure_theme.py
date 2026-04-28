from pathlib import Path
import random

from midiutil import MIDIFile


# Original heroic fantasy / adventure game theme.
# This is not a transcription or recreation of any existing Zelda theme.

OUTPUT_FILE = Path(__file__).with_name("heroic_adventure_theme_original.mid")
TEMPO = 108
BEATS_PER_BAR = 4
TOTAL_BARS = 64
SEED = 4282026

random.seed(SEED)

TRACKS = {
    "horn": 0,
    "flute": 1,
    "clarinet": 2,
    "oboe": 3,
    "trombone": 4,
    "strings": 5,
    "pizz": 6,
    "harp": 7,
    "bass": 8,
    "choir": 9,
    "drums": 10,
}

CHANNELS = {
    "horn": 0,
    "flute": 1,
    "clarinet": 2,
    "oboe": 3,
    "trombone": 4,
    "strings": 5,
    "pizz": 6,
    "harp": 7,
    "bass": 8,
    "choir": 10,
    "drums": 9,
}

PROGRAMS = {
    "horn": 60,      # French Horn
    "flute": 73,     # Flute
    "clarinet": 71,  # Clarinet
    "oboe": 68,      # Oboe
    "trombone": 57,  # Trombone
    "strings": 48,   # String Ensemble
    "pizz": 45,      # Pizzicato Strings
    "harp": 46,      # Orchestral Harp
    "bass": 32,      # Acoustic Bass
    "choir": 52,     # Choir Aahs
}

MIX = {
    "horn": {"volume": 92, "pan": 58, "reverb": 64, "chorus": 8},
    "flute": {"volume": 74, "pan": 78, "reverb": 68, "chorus": 8},
    "clarinet": {"volume": 72, "pan": 48, "reverb": 62, "chorus": 6},
    "oboe": {"volume": 68, "pan": 38, "reverb": 62, "chorus": 6},
    "trombone": {"volume": 80, "pan": 34, "reverb": 58, "chorus": 5},
    "strings": {"volume": 86, "pan": 72, "reverb": 70, "chorus": 12},
    "pizz": {"volume": 70, "pan": 46, "reverb": 42, "chorus": 4},
    "harp": {"volume": 76, "pan": 84, "reverb": 66, "chorus": 10},
    "bass": {"volume": 84, "pan": 54, "reverb": 22, "chorus": 2},
    "choir": {"volume": 50, "pan": 64, "reverb": 82, "chorus": 18},
    "drums": {"volume": 78, "pan": 64, "reverb": 28, "chorus": 0},
}

PART_RANGES = {
    "horn": (58, 86),
    "flute": (72, 96),
    "clarinet": (60, 88),
    "oboe": (62, 86),
    "trombone": (45, 74),
    "strings": (55, 88),
    "choir": (55, 84),
}

SECTION_STARTS = {
    "intro": 0,
    "theme_a": 8,
    "theme_b": 24,
    "bridge": 40,
    "final": 48,
    "outro": 60,
}

ORCHESTRATION = {
    "intro": [
        [("flute", 12, -12, 1.0)],
        [("clarinet", 0, -10, 1.0)],
        [("oboe", 0, -12, 1.0)],
        [("horn", -12, -8, 1.15)],
    ],
    "theme_a": [
        [("horn", 0, 0, 1.0)],
        [("flute", 12, -10, 0.95)],
        [("clarinet", 0, -8, 1.0)],
        [("oboe", 0, -10, 1.0)],
        [("horn", 0, 2, 1.0), ("trombone", -12, -12, 0.95)],
        [("clarinet", 0, -6, 1.0)],
        [("strings", 0, -16, 1.08)],
        [("horn", 0, 4, 1.0), ("flute", 12, -16, 0.9)],
    ],
    "theme_b": [
        [("horn", 0, 4, 1.0), ("trombone", -12, -8, 0.95)],
        [("flute", 12, -10, 0.92)],
        [("strings", 0, -14, 1.0)],
        [("clarinet", 0, -4, 1.0), ("oboe", 0, -14, 0.9)],
        [("trombone", -12, -2, 1.05)],
        [("oboe", 0, -6, 1.0)],
        [("horn", 0, 6, 1.0), ("strings", 0, -18, 1.0)],
        [("flute", 12, -8, 0.9), ("clarinet", 0, -12, 1.0)],
    ],
    "bridge": [
        [("clarinet", 0, -8, 1.2)],
        [("oboe", 0, -8, 1.15)],
        [("flute", 12, -14, 1.05)],
        [("horn", -12, -8, 1.25)],
    ],
    "final": [
        [("horn", 0, 8, 1.0), ("trombone", -12, -2, 0.95)],
        [("strings", 0, -8, 1.0), ("flute", 12, -14, 0.9)],
        [("horn", 0, 6, 1.0), ("clarinet", 0, -10, 0.95)],
        [("trombone", -12, 2, 1.0), ("oboe", 0, -10, 0.9)],
        [("horn", 0, 8, 1.0), ("choir", -12, -20, 1.35)],
        [("flute", 12, -8, 0.9), ("strings", 0, -12, 1.0)],
        [("clarinet", 0, -4, 1.0), ("oboe", 0, -12, 0.9)],
        [("horn", 0, 10, 1.0), ("trombone", -12, 0, 0.95), ("strings", 0, -18, 1.0)],
    ],
    "outro": [
        [("flute", 0, -14, 1.25)],
        [("clarinet", -12, -12, 1.3)],
        [("horn", -12, -12, 1.35)],
        [("strings", -12, -18, 1.4)],
    ],
}

# D major / B minor color. Kept intentionally consonant for General MIDI.
PROGRESSION = [
    {"name": "D", "root": 50, "tones": [0, 4, 7]},
    {"name": "A/C#", "root": 49, "tones": [0, 5, 9]},
    {"name": "Bm", "root": 47, "tones": [0, 3, 7]},
    {"name": "Gadd9", "root": 43, "tones": [0, 4, 7, 14]},
    {"name": "Em7", "root": 40, "tones": [0, 3, 7, 10]},
    {"name": "A", "root": 45, "tones": [0, 4, 7]},
    {"name": "F#m", "root": 42, "tones": [0, 3, 7]},
    {"name": "G", "root": 43, "tones": [0, 4, 7]},
]

THEME_A_MOTIFS = [
    [(0.0, 74, 0.85), (1.0, 69, 0.45), (1.55, 78, 0.7), (2.55, 76, 0.5), (3.2, 74, 0.55)],
    [(0.0, 73, 0.75), (0.9, 69, 0.45), (1.5, 76, 0.95), (2.75, 73, 0.45), (3.25, 71, 0.45)],
    [(0.0, 71, 0.65), (0.85, 78, 0.7), (1.75, 74, 0.45), (2.35, 71, 0.55), (3.1, 66, 0.55)],
    [(0.0, 67, 0.65), (0.8, 74, 0.75), (1.75, 71, 0.45), (2.4, 69, 0.45), (3.0, 67, 0.75)],
    [(0.0, 76, 0.8), (1.0, 71, 0.4), (1.5, 79, 0.65), (2.35, 78, 0.45), (3.0, 76, 0.7)],
    [(0.0, 73, 0.75), (0.85, 76, 0.45), (1.45, 81, 0.9), (2.75, 78, 0.42), (3.25, 76, 0.42)],
    [(0.0, 69, 0.65), (0.85, 76, 0.75), (1.8, 73, 0.4), (2.35, 69, 0.55), (3.05, 66, 0.65)],
    [(0.0, 67, 0.7), (0.95, 71, 0.45), (1.55, 74, 0.7), (2.55, 72, 0.45), (3.1, 74, 0.7)],
]

THEME_B_MOTIFS = [
    [(0.0, 81, 0.75), (0.9, 74, 0.45), (1.45, 86, 0.75), (2.45, 83, 0.5), (3.05, 81, 0.65)],
    [(0.0, 78, 0.65), (0.75, 81, 0.45), (1.35, 85, 0.85), (2.6, 81, 0.45), (3.15, 78, 0.45)],
    [(0.0, 78, 0.75), (0.95, 83, 0.45), (1.55, 86, 0.75), (2.55, 83, 0.45), (3.1, 78, 0.55)],
    [(0.0, 79, 0.8), (1.0, 74, 0.45), (1.55, 83, 0.7), (2.55, 81, 0.45), (3.1, 79, 0.65)],
    [(0.0, 83, 0.7), (0.8, 76, 0.42), (1.35, 86, 0.95), (2.7, 81, 0.4), (3.2, 78, 0.42)],
    [(0.0, 81, 0.85), (1.0, 76, 0.45), (1.6, 85, 0.65), (2.45, 83, 0.45), (3.05, 81, 0.65)],
    [(0.0, 78, 0.7), (0.85, 85, 0.7), (1.85, 81, 0.4), (2.4, 78, 0.5), (3.05, 73, 0.65)],
    [(0.0, 79, 0.6), (0.75, 83, 0.45), (1.35, 86, 0.75), (2.35, 88, 0.45), (3.0, 86, 0.85)],
]

BRIDGE_MOTIFS = [
    [(0.0, 69, 1.0), (1.35, 76, 0.55), (2.1, 73, 0.55), (3.0, 69, 0.65)],
    [(0.0, 66, 0.9), (1.25, 73, 0.55), (2.0, 71, 0.45), (2.75, 69, 0.85)],
    [(0.0, 67, 1.0), (1.35, 74, 0.55), (2.1, 71, 0.55), (3.0, 67, 0.65)],
    [(0.0, 69, 0.8), (1.0, 73, 0.5), (1.7, 76, 0.75), (2.85, 73, 0.75)],
]


def bar_to_beat(bar):
    return bar * BEATS_PER_BAR


def clamp(value, low, high):
    return max(low, min(high, value))


def human_time(amount=0.012):
    return random.uniform(-amount, amount)


def human_velocity(velocity, amount=5):
    return clamp(velocity + random.randint(-amount, amount), 1, 127)


def add_note(midi, part, pitch, start, duration, velocity, humanize=True):
    offset = human_time() if humanize and part != "drums" else 0
    midi.addNote(
        TRACKS[part],
        CHANNELS[part],
        int(pitch),
        max(0, start + offset),
        duration,
        human_velocity(velocity),
    )


def chord_pitches(chord, octave_shift=0, inversion=0, low=36, high=96):
    pitches = [chord["root"] + tone + (octave_shift * 12) for tone in chord["tones"]]
    for _ in range(inversion):
        pitches.append(pitches.pop(0) + 12)
    return [clamp(pitch, low, high) for pitch in pitches]


def fit_to_part_range(part, pitch):
    low, high = PART_RANGES.get(part, (36, 96))
    while pitch < low:
        pitch += 12
    while pitch > high:
        pitch -= 12
    return clamp(pitch, low, high)


def section_for_bar(bar):
    if bar < 8:
        return "intro"
    if bar < 24:
        return "theme_a"
    if bar < 40:
        return "theme_b"
    if bar < 48:
        return "bridge"
    if bar < 60:
        return "final"
    return "outro"


def setup_midi():
    midi = MIDIFile(len(TRACKS), adjust_origin=True, deinterleave=False)
    for name, track in TRACKS.items():
        channel = CHANNELS[name]
        midi.addTrackName(track, 0, name.title())
        midi.addTempo(track, 0, TEMPO)
        midi.addControllerEvent(track, channel, 0, 7, MIX[name]["volume"])
        midi.addControllerEvent(track, channel, 0, 10, MIX[name]["pan"])
        midi.addControllerEvent(track, channel, 0, 91, MIX[name]["reverb"])
        midi.addControllerEvent(track, channel, 0, 93, MIX[name]["chorus"])
        if name in PROGRAMS:
            midi.addProgramChange(track, channel, 0, PROGRAMS[name])
    return midi


def add_harmony(midi, bar, chord, section):
    start = bar_to_beat(bar)
    inversion = bar % 3
    string_notes = chord_pitches(chord, octave_shift=2, inversion=inversion, low=58, high=86)
    lower_notes = chord_pitches(chord, octave_shift=1, inversion=inversion, low=48, high=76)

    string_velocity = 46 if section in {"intro", "bridge", "outro"} else 60
    for pitch in string_notes[:4]:
        add_note(midi, "strings", pitch, start, 3.85, string_velocity, humanize=False)

    if section != "intro":
        for offset, duration, velocity in [(0, 0.62, 62), (1.0, 0.48, 52), (2.0, 0.62, 58), (3.0, 0.48, 48)]:
            for pitch in lower_notes[:3]:
                add_note(midi, "pizz", pitch, start + offset, duration, velocity)

    if section in {"theme_b", "final", "outro"}:
        choir_notes = chord_pitches(chord, octave_shift=2, inversion=1, low=60, high=84)[:3]
        for pitch in choir_notes:
            add_note(midi, "choir", pitch, start + 0.05, 3.7, 34, humanize=False)


def add_bass(midi, bar, chord, section):
    if section == "intro":
        return
    start = bar_to_beat(bar)
    root = chord["root"] - 24
    fifth = root + 7
    if section == "bridge":
        pattern = [(0, root, 1.4, 60), (2.0, fifth, 1.0, 48)]
    else:
        pattern = [(0, root, 0.8, 76), (1.5, fifth, 0.35, 58), (2.0, root + 12, 0.65, 68), (3.25, fifth, 0.4, 54)]
    for offset, pitch, duration, velocity in pattern:
        add_note(midi, "bass", pitch, start + offset, duration, velocity)


def add_harp(midi, bar, chord, section):
    if section == "intro" and bar < 2:
        return
    start = bar_to_beat(bar)
    notes = chord_pitches(chord, octave_shift=2, inversion=bar % 2, low=62, high=91)
    step = 0.5 if section in {"intro", "bridge", "outro"} else 0.25
    count = 8 if step == 0.5 else 16
    for index in range(count):
        add_note(midi, "harp", notes[index % len(notes)], start + index * step, 0.24, 42)


def motif_for_section(section, bar):
    if section == "theme_b" or section == "final":
        return THEME_B_MOTIFS[bar % len(THEME_B_MOTIFS)]
    if section == "bridge":
        return BRIDGE_MOTIFS[bar % len(BRIDGE_MOTIFS)]
    return THEME_A_MOTIFS[bar % len(THEME_A_MOTIFS)]


def melody_layers_for_section(section, bar):
    choices = ORCHESTRATION[section]
    local_bar = bar - SECTION_STARTS[section]
    return choices[local_bar % len(choices)]


def add_melody(midi, bar, section):
    if section == "intro" and bar < 4:
        return

    start = bar_to_beat(bar)
    motif = motif_for_section(section, bar)
    layers = melody_layers_for_section(section, bar)
    velocity = 66 if section in {"intro", "bridge", "outro"} else 82
    if section == "final":
        velocity = 90

    for offset, pitch, duration in motif:
        if section == "intro" and offset > 2.75:
            continue
        if section == "outro":
            pitch -= 12 if pitch > 76 else 0
            duration *= 1.25
        if section == "final" and bar % 8 in {0, 1, 7} and pitch < 84:
            pitch += 12
        accent = 8 if offset == 0 else 0
        for part, transpose, velocity_delta, duration_scale in layers:
            part_pitch = fit_to_part_range(part, pitch + transpose)
            part_velocity = velocity + accent + velocity_delta
            add_note(midi, part, part_pitch, start + offset, duration * duration_scale, part_velocity)

    if section in {"theme_b", "final"}:
        counter = [(0.5, 74, 0.35), (1.5, 78, 0.35), (2.5, 76, 0.35), (3.25, 73, 0.35)]
        for offset, pitch, duration in counter:
            answer_part = "clarinet" if bar % 2 else "oboe"
            add_note(midi, answer_part, fit_to_part_range(answer_part, pitch), start + offset, duration, 46)


def add_percussion(midi, bar, section):
    if section == "intro" and bar < 4:
        return
    start = bar_to_beat(bar)
    track = TRACKS["drums"]
    channel = CHANNELS["drums"]

    def drum(note, offset, duration, velocity):
        midi.addNote(track, channel, note, start + offset, duration, human_velocity(velocity, 4))

    sparse = section in {"intro", "bridge", "outro"}
    for offset in ([0, 2] if sparse else [0, 1.5, 2, 3.5]):
        drum(36, offset, 0.12, 66 if sparse else 78)
    for offset in [1, 3]:
        drum(38, offset, 0.12, 58 if sparse else 70)
    for offset in [0, 0.5, 1, 1.5, 2, 2.5, 3, 3.5]:
        drum(42, offset + (0.04 if offset % 1 else 0), 0.07, 32 if sparse else 42)
    if section in {"theme_b", "final"} and bar % 4 == 3:
        for offset in [3.0, 3.25, 3.5, 3.75]:
            drum(47, offset, 0.1, 50 + int((offset - 3.0) * 20))


def add_markers(midi):
    for bar, name in [(0, "Intro"), (8, "Theme A"), (24, "Theme B"), (40, "Bridge"), (48, "Final"), (60, "Outro")]:
        midi.addText(TRACKS["horn"], bar_to_beat(bar), name)


def add_final_chord(midi):
    start = bar_to_beat(TOTAL_BARS - 1)
    chord = {"name": "D6/9", "root": 50, "tones": [0, 4, 7, 9, 14]}
    for pitch in chord_pitches(chord, octave_shift=2, inversion=1, low=58, high=88):
        add_note(midi, "strings", pitch, start, 3.8, 50, humanize=False)
    for pitch in chord_pitches(chord, octave_shift=1, inversion=0, low=46, high=78):
        add_note(midi, "horn", pitch + 12, start + 0.2, 3.2, 58)
    add_note(midi, "bass", chord["root"] - 24, start, 3.8, 64)


def build_song():
    midi = setup_midi()
    add_markers(midi)
    for bar in range(TOTAL_BARS):
        section = section_for_bar(bar)
        chord = PROGRESSION[bar % len(PROGRESSION)]
        add_harmony(midi, bar, chord, section)
        add_bass(midi, bar, chord, section)
        add_harp(midi, bar, chord, section)
        add_melody(midi, bar, section)
        add_percussion(midi, bar, section)
    add_final_chord(midi)
    return midi


def main():
    midi = build_song()
    with OUTPUT_FILE.open("wb") as file:
        midi.writeFile(file)
    seconds = int((TOTAL_BARS * BEATS_PER_BAR / TEMPO) * 60)
    print(f"Saved: {OUTPUT_FILE}")
    print(f"Duration: {seconds // 60}:{seconds % 60:02d} at {TEMPO} BPM")
    print("Original heroic adventure theme generated.")


if __name__ == "__main__":
    main()