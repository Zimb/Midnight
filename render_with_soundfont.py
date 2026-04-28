from argparse import ArgumentParser
from pathlib import Path
import shutil
import subprocess
import sys


DEFAULT_MIDI = Path(__file__).with_name("lofi_orchestral_anime_game_3min.mid")
DEFAULT_OUTPUT = Path(__file__).with_name("lofi_orchestral_anime_game_3min.wav")


def find_fluidsynth(explicit_path=None):
    if explicit_path:
        path = Path(explicit_path)
        if path.exists():
            return str(path)
        raise FileNotFoundError(f"FluidSynth introuvable: {path}")

    executable = shutil.which("fluidsynth") or shutil.which("fluidsynth.exe")
    if executable:
        return executable

    common_paths = [
        Path("C:/Program Files/FluidSynth/bin/fluidsynth.exe"),
        Path("C:/Program Files/fluidsynth/bin/fluidsynth.exe"),
        Path("C:/Program Files (x86)/FluidSynth/bin/fluidsynth.exe"),
    ]
    for path in common_paths:
        if path.exists():
            return str(path)

    raise FileNotFoundError(
        "FluidSynth n'est pas installe ou n'est pas dans le PATH. "
        "Installe FluidSynth, puis relance ce script avec --fluidsynth C:/chemin/fluidsynth.exe si besoin."
    )


def render_midi(midi_path, soundfont_path, output_path, fluidsynth_path=None, sample_rate=44100, gain=0.7):
    midi_path = Path(midi_path)
    soundfont_path = Path(soundfont_path)
    output_path = Path(output_path)

    if not midi_path.exists():
        raise FileNotFoundError(f"MIDI introuvable: {midi_path}")
    if not soundfont_path.exists():
        raise FileNotFoundError(f"SoundFont introuvable: {soundfont_path}")

    fluidsynth = find_fluidsynth(fluidsynth_path)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    command = [
        fluidsynth,
        "-ni",
        "-F",
        str(output_path),
        "-r",
        str(sample_rate),
        "-g",
        str(gain),
        str(soundfont_path),
        str(midi_path),
    ]

    subprocess.run(command, check=True)
    return output_path


def parse_args():
    parser = ArgumentParser(description="Rend un fichier MIDI en WAV avec une banque de sons SoundFont.")
    parser.add_argument("--midi", default=str(DEFAULT_MIDI), help="Fichier MIDI source")
    parser.add_argument("--soundfont", required=True, help="Fichier .sf2 ou .sf3 a utiliser")
    parser.add_argument("--output", default=str(DEFAULT_OUTPUT), help="Fichier WAV de sortie")
    parser.add_argument("--fluidsynth", default=None, help="Chemin vers fluidsynth.exe si absent du PATH")
    parser.add_argument("--sample-rate", type=int, default=44100, help="Frequence d'echantillonnage")
    parser.add_argument("--gain", type=float, default=0.7, help="Gain FluidSynth, par exemple 0.5 a 1.2")
    return parser.parse_args()


def main():
    args = parse_args()
    try:
        output = render_midi(
            midi_path=args.midi,
            soundfont_path=args.soundfont,
            output_path=args.output,
            fluidsynth_path=args.fluidsynth,
            sample_rate=args.sample_rate,
            gain=args.gain,
        )
    except Exception as exc:
        print(f"Erreur: {exc}", file=sys.stderr)
        raise SystemExit(1) from exc

    print(f"WAV cree: {output}")


if __name__ == "__main__":
    main()