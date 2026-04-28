from pathlib import Path
import random

from midiutil import MIDIFile


# ============================================================
# 3-minute orchestral chill-hop / lofi generator
# Jazzy anime and video-game inspired mood, original material.
# Requires: pip install midiutil
# ============================================================

SEED = 260428
random.seed(SEED)

TEMPO = 72
BEATS_PER_BAR = 4
TOTAL_BARS = 54  # 54 bars at 72 BPM in 4/4 = 180 seconds
OUTPUT_FILE = Path(__file__).with_name("lofi_orchestral_anime_game_3min.mid")


TRACKS = {
    "piano": 0,
    "rhodes": 1,
    "bass": 2,
    "strings": 3,
    "cello": 4,
    "harp": 5,
    "vibes": 6,
    "lead": 7,
    "counter": 8,
    "choir": 9,
    "drums": 10,
}

CHANNELS = {
    "piano": 0,
    "rhodes": 1,
    "bass": 2,
    "strings": 3,
    "cello": 4,
    "harp": 5,
    "vibes": 6,
    "lead": 7,
    "counter": 8,
    "choir": 10,
    "drums": 9,  # General MIDI percussion channel
}

PROGRAMS = {
    "piano": 0,       # Acoustic Grand Piano
    "rhodes": 4,      # Electric Piano 1
    "bass": 32,       # Acoustic Bass
    "strings": 48,    # String Ensemble 1
    "cello": 42,      # Cello
    "harp": 46,       # Orchestral Harp
    "vibes": 11,      # Vibraphone
    "lead": 71,       # Clarinet
    "counter": 69,    # English Horn
    "choir": 52,      # Choir Aahs
}

MIX = {
    "piano": {"volume": 92, "pan": 54, "reverb": 54, "chorus": 18},
    "rhodes": {"volume": 78, "pan": 42, "reverb": 44, "chorus": 38},
    "bass": {"volume": 90, "pan": 64, "reverb": 18, "chorus": 4},
    "strings": {"volume": 82, "pan": 72, "reverb": 72, "chorus": 18},
    "cello": {"volume": 76, "pan": 35, "reverb": 60, "chorus": 10},
    "harp": {"volume": 70, "pan": 78, "reverb": 70, "chorus": 24},
    "vibes": {"volume": 72, "pan": 88, "reverb": 58, "chorus": 42},
    "lead": {"volume": 76, "pan": 58, "reverb": 62, "chorus": 10},
    "counter": {"volume": 62, "pan": 46, "reverb": 64, "chorus": 10},
    "choir": {"volume": 52, "pan": 64, "reverb": 82, "chorus": 20},
    "drums": {"volume": 80, "pan": 64, "reverb": 24, "chorus": 4},
}


# Warm F major / D minor progression. The voicings stay consonant so they
# survive the harsher General MIDI instruments without sounding sour.
PROGRESSION = [
    {"name": "Fmaj9", "root": 53, "tones": [0, 4, 7, 11, 14]},
    {"name": "Cadd9/E", "root": 52, "tones": [0, 3, 8, 10, 15]},
    {"name": "Dm7", "root": 50, "tones": [0, 3, 7, 10]},
    {"name": "Bbmaj9", "root": 46, "tones": [0, 4, 7, 11, 14]},
    {"name": "Gm7", "root": 43, "tones": [0, 3, 7, 10]},
    {"name": "C7sus9", "root": 48, "tones": [0, 5, 7, 10, 14]},
    {"name": "Am7", "root": 45, "tones": [0, 3, 7, 10, 12]},
    {"name": "Bb/C", "root": 48, "tones": [0, 5, 10, 14, 17]},
]

MELODIC_PHRASES = [
    [69, 72, 76, 74, 72, 69, 67, None, 65, 67, 69, 72, 74, 72, 69, None],
    [67, 72, 74, 76, 74, 72, 67, None, 64, 67, 72, 74, 72, 67, 64, None],
    [65, 69, 72, 74, 72, 69, 65, None, 62, 65, 69, 72, 69, 65, 62, None],
    [65, 69, 72, 74, 72, 69, 65, None, 62, 65, 69, 72, 74, 72, 69, None],
    [67, 70, 74, 77, 74, 70, 67, None, 62, 67, 70, 74, 77, 74, 70, None],
    [67, 70, 74, 77, 74, 70, 67, None, 65, 67, 70, 74, 70, 67, 65, None],
    [69, 72, 76, 79, 76, 72, 69, None, 64, 67, 69, 72, 76, 72, 69, None],
    [65, 70, 74, 77, 74, 70, 65, None, 62, 65, 70, 74, 77, 74, 70, None],
]


def bar_to_beat(bar):
    return bar * BEATS_PER_BAR


def section_for_bar(bar):
    if bar < 8:
        return "intro"
    if bar < 24:
        return "theme_a"
    if bar < 40:
        return "theme_b"
    if bar < 48:
        return "bridge"
    return "outro"


def clamp(value, low, high):
    return max(low, min(high, value))


def human_time(amount=0.018):
    return random.uniform(-amount, amount)


def human_velocity(velocity, amount=7):
    return clamp(velocity + random.randint(-amount, amount), 1, 127)


def add_note(midi, part, pitch, start, duration, velocity, humanize=True):
    track = TRACKS[part]
    channel = CHANNELS[part]
    offset = human_time() if humanize and part != "drums" else 0
    time = max(0, start + offset)
    midi.addNote(track, channel, int(pitch), time, duration, human_velocity(velocity))


def chord_pitches(chord, octave_shift=0, inversion=0, limit_low=36, limit_high=96):
    pitches = [chord["root"] + tone + (12 * octave_shift) for tone in chord["tones"]]
    for _ in range(inversion):
        pitches.append(pitches.pop(0) + 12)
    return [clamp(pitch, limit_low, limit_high) for pitch in pitches]


def setup_midi():
    midi = MIDIFile(len(TRACKS), adjust_origin=True, deinterleave=False)

    for name, track in TRACKS.items():
        channel = CHANNELS[name]
        midi.addTrackName(track, 0, name.replace("_", " ").title())
        midi.addTempo(track, 0, TEMPO)
        midi.addControllerEvent(track, channel, 0, 7, MIX[name]["volume"])
        midi.addControllerEvent(track, channel, 0, 10, MIX[name]["pan"])
        midi.addControllerEvent(track, channel, 0, 91, MIX[name]["reverb"])
        midi.addControllerEvent(track, channel, 0, 93, MIX[name]["chorus"])
        midi.addControllerEvent(track, channel, 0, 11, 112)
        if name in PROGRAMS:
            midi.addProgramChange(track, channel, 0, PROGRAMS[name])

    return midi


def add_warm_chords(midi, bar, chord, section):
    start = bar_to_beat(bar)
    inversion = bar % 3
    piano_voicing = chord_pitches(chord, octave_shift=1, inversion=inversion, limit_low=55, limit_high=84)
    rhodes_voicing = chord_pitches(chord, octave_shift=1, inversion=(inversion + 1) % 3, limit_low=52, limit_high=82)

    if section == "intro":
        rhythm = [(0, 1.8, 58), (2.5, 1.25, 48)]
    elif section == "bridge":
        rhythm = [(0, 3.6, 50)]
    else:
        rhythm = [(0, 1.35, 62), (1.55, 0.55, 44), (2.35, 1.25, 56), (3.45, 0.4, 38)]

    for offset, duration, velocity in rhythm:
        for index, pitch in enumerate(piano_voicing[:4]):
            add_note(midi, "piano", pitch, start + offset + index * 0.015, duration, velocity - index * 2)

    if section != "intro" or bar >= 4:
        for pitch in rhodes_voicing[:5]:
            add_note(midi, "rhodes", pitch, start + 0.02, 3.75, 38 if section == "bridge" else 46)


def add_orchestral_bed(midi, bar, chord, section):
    start = bar_to_beat(bar)
    strings = chord_pitches(chord, octave_shift=2, inversion=bar % 2, limit_low=60, limit_high=88)
    cello_root = chord["root"] - 12

    if section in {"intro", "bridge", "outro"}:
        string_velocity = 46
    else:
        string_velocity = 58

    for pitch in strings[:4]:
        add_note(midi, "strings", pitch, start, 3.85, string_velocity, humanize=False)

    add_note(midi, "cello", cello_root, start, 2.0, 48 if section == "intro" else 58)
    if section != "intro":
        add_note(midi, "cello", cello_root + 7, start + 2.0, 1.75, 44)

    if section in {"theme_b", "bridge", "outro"}:
        choir_notes = chord_pitches(chord, octave_shift=2, inversion=1, limit_low=64, limit_high=86)[:3]
        for pitch in choir_notes:
            add_note(midi, "choir", pitch, start + 0.15, 3.65, 32, humanize=False)


def add_bass(midi, bar, chord, section):
    if section == "intro":
        return

    start = bar_to_beat(bar)
    root = chord["root"] - 24
    fifth = root + 7
    tenth = root + 16

    pattern = [(0, root, 0.78, 72), (1.5, fifth, 0.38, 58), (2.0, root + 12, 0.58, 66), (3.25, tenth, 0.42, 52)]
    if section == "bridge":
        pattern = [(0, root, 1.7, 60), (2.5, fifth, 0.85, 48)]
    if section == "outro":
        pattern = [(0, root, 1.0, 62), (2.0, fifth, 0.72, 50)]

    for offset, pitch, duration, velocity in pattern:
        add_note(midi, "bass", pitch, start + offset, duration, velocity)


def add_harp_and_vibes(midi, bar, chord, section):
    if section == "intro" and bar < 2:
        return

    start = bar_to_beat(bar)
    arp = chord_pitches(chord, octave_shift=2, inversion=bar % 4, limit_low=62, limit_high=91)
    step = 0.5 if section in {"intro", "bridge"} else 0.25
    count = 8 if step == 0.5 else 16

    for index in range(count):
        pitch = arp[index % len(arp)] + (12 if index % 7 == 6 else 0)
        velocity = 42 if section in {"intro", "bridge"} else 50
        add_note(midi, "harp", pitch, start + index * step, 0.28, velocity)

    if section in {"theme_a", "theme_b", "outro"}:
        for offset in [0.75, 2.75]:
            tone = random.choice(arp[:4]) + 12
            add_note(midi, "vibes", tone, start + offset, 0.7, 45)


def phrase_for_bar(bar, section):
    phrase = MELODIC_PHRASES[bar % len(MELODIC_PHRASES)]
    if section == "bridge":
        return [note - 12 if note is not None and note > 72 else note for note in phrase]
    return phrase


def add_melody(midi, bar, chord, section):
    if section == "intro" and bar < 4:
        return

    start = bar_to_beat(bar)
    phrase = phrase_for_bar(bar, section)
    note_length = 0.5

    if section == "intro":
        active_steps = [0, 2, 4, 8, 10, 12]
        lead_part = "lead"
        velocity = 62
    elif section == "bridge":
        active_steps = [0, 3, 5, 8, 11, 13]
        lead_part = "counter"
        velocity = 58
    else:
        active_steps = list(range(16))
        lead_part = "lead"
        velocity = 78 if section == "theme_b" else 72

    for step_index in active_steps:
        note = phrase[step_index % len(phrase)]
        if note is None:
            continue

        pitch = note
        duration = 0.42 if step_index % 4 else 0.68
        add_note(midi, lead_part, pitch, start + step_index * note_length, duration, velocity)

    if section in {"theme_b", "outro"}:
        counter_tones = chord_pitches(chord, octave_shift=2, inversion=2, limit_low=62, limit_high=83)
        for offset, pitch in [(1.0, counter_tones[1]), (2.5, counter_tones[2]), (3.5, counter_tones[0])]:
            add_note(midi, "counter", pitch, start + offset, 0.55, 50)


def add_lofi_drums(midi, bar, section):
    if section == "intro" and bar < 4:
        return

    start = bar_to_beat(bar)
    drum_track = TRACKS["drums"]
    drum_channel = CHANNELS["drums"]
    swing = 0.055

    def drum(note, offset, duration, velocity):
        midi.addNote(drum_track, drum_channel, note, start + offset, duration, human_velocity(velocity, 5))

    sparse = section in {"intro", "bridge", "outro"}
    kick_pattern = [0, 2.0] if sparse else [0, 1.5, 2.0, 3.5]
    snare_pattern = [1.0, 3.0]

    for offset in kick_pattern:
        drum(36, offset, 0.12, 72 if not sparse else 58)

    for offset in snare_pattern:
        drum(38, offset + human_time(0.012), 0.12, 60 if sparse else 70)
        drum(37, offset + 0.025, 0.08, 36)

    hat_steps = 4 if sparse else 8
    for index in range(hat_steps):
        offset = index * 0.5
        if index % 2 == 1:
            offset += swing
        drum(42, offset, 0.08, 33 if sparse else 43)

    if section in {"theme_a", "theme_b"}:
        for offset in [0.75, 2.75, 3.75]:
            drum(70, offset + human_time(0.02), 0.08, 28)
        if bar % 8 == 7:
            for offset in [3.0, 3.25, 3.5, 3.75]:
                drum(38, offset, 0.08, 48 + int((offset - 3.0) * 20))


def add_section_markers(midi):
    markers = [
        (0, "Intro"),
        (8, "Theme A"),
        (24, "Theme B"),
        (40, "Bridge"),
        (48, "Outro"),
    ]
    for bar, label in markers:
        midi.addText(TRACKS["piano"], bar_to_beat(bar), label)


def add_final_cadence(midi):
    start = bar_to_beat(TOTAL_BARS - 1)
    final_chord = {"name": "F6/9", "root": 53, "tones": [0, 4, 7, 9, 14, 16]}
    final_piano = chord_pitches(final_chord, octave_shift=1, inversion=1, limit_low=55, limit_high=86)
    final_strings = chord_pitches(final_chord, octave_shift=2, inversion=0, limit_low=64, limit_high=91)

    for pitch in final_piano:
        add_note(midi, "piano", pitch, start, 3.8, 58)
    for pitch in final_strings[:4]:
        add_note(midi, "strings", pitch, start, 3.8, 44, humanize=False)
    add_note(midi, "bass", final_chord["root"] - 24, start, 3.6, 58)
    add_note(midi, "lead", 77, start + 1.0, 1.8, 55)


def build_song():
    midi = setup_midi()
    add_section_markers(midi)

    for bar in range(TOTAL_BARS):
        section = section_for_bar(bar)
        chord = PROGRESSION[bar % len(PROGRESSION)]

        add_orchestral_bed(midi, bar, chord, section)
        add_warm_chords(midi, bar, chord, section)
        add_bass(midi, bar, chord, section)
        add_harp_and_vibes(midi, bar, chord, section)
        add_melody(midi, bar, chord, section)
        add_lofi_drums(midi, bar, section)

    add_final_cadence(midi)
    return midi


def main():
    midi = build_song()
    with OUTPUT_FILE.open("wb") as midi_file:
        midi.writeFile(midi_file)

    seconds = int((TOTAL_BARS * BEATS_PER_BAR / TEMPO) * 60)
    print(f"Saved: {OUTPUT_FILE}")
    print(f"Duration: {seconds // 60}:{seconds % 60:02d} at {TEMPO} BPM")
    print("Tracks: piano, rhodes, bass, strings, cello, harp, vibes, clarinet, english horn, choir, drums")


if __name__ == "__main__":
    main()