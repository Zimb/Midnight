from copy import deepcopy
import ctypes
from dataclasses import dataclass
import os
from pathlib import Path
import tempfile
import time
import tkinter as tk
from tkinter import filedialog, messagebox, ttk

try:
    from mido import MetaMessage, Message, MidiFile, MidiTrack, bpm2tempo, tempo2bpm
except ModuleNotFoundError as exc:
    raise SystemExit(
        "Module manquant: mido\n"
        "Installe-le avec: python -m pip install mido"
    ) from exc

try:
    from render_with_soundfont import find_fluidsynth, render_midi
except ModuleNotFoundError:
    find_fluidsynth = None
    render_midi = None


SUPPORTED_FILES = [("Fichiers MIDI", "*.mid *.midi"), ("Tous les fichiers", "*.*")]
SOUNDFONT_FILES = [("SoundFont", "*.sf2 *.sf3"), ("Tous les fichiers", "*.*")]
WAV_FILES = [("Audio WAV", "*.wav"), ("Tous les fichiers", "*.*")]
DEFAULT_OUTPUT_SUFFIX = "_pianoroll"

KEYBOARD_WIDTH = 78
HEADER_HEIGHT = 26
ROW_HEIGHT = 14
PIXELS_PER_TICK = 0.08
MIN_NOTE_TICKS = 30
DEFAULT_NOTE_TICKS = 240
DEFAULT_VELOCITY = 82
LOW_PITCH = 24
HIGH_PITCH = 96
NOTE_NAMES = ("C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B")
BLACK_KEYS = {1, 3, 6, 8, 10}


@dataclass
class TrackInfo:
    index: int
    name: str
    notes: int
    channels: str
    programs: str
    muted: bool = False


@dataclass
class PianoNote:
    note_id: int
    start: int
    end: int
    pitch: int
    velocity: int
    channel: int


def clamp(value, low, high):
    return max(low, min(high, value))


def pitch_name(pitch):
    return f"{NOTE_NAMES[pitch % 12]}{(pitch // 12) - 1}"


class MidiPlaybackError(Exception):
    pass


class WindowsMidiPlayer:
    def __init__(self):
        self.supported = os.name == "nt"
        self.alias = "midi_editor_player"
        self.is_open = False
        self.temp_path = None

    def play_file(self, path):
        if not self.supported:
            raise MidiPlaybackError("La lecture audio integree est disponible uniquement sur Windows.")
        self.close()
        self.temp_path = Path(path)
        self._send(f'open "{self.temp_path}" type sequencer alias {self.alias}')
        self.is_open = True
        try:
            self._send(f"set {self.alias} time format milliseconds")
        except MidiPlaybackError:
            pass
        self._send(f"play {self.alias}")

    def pause(self):
        if self.is_open:
            try:
                self._send(f"pause {self.alias}")
            except MidiPlaybackError:
                self.close()

    def close(self):
        if self.supported and self.is_open:
            try:
                self._send(f"stop {self.alias}")
            except MidiPlaybackError:
                pass
            try:
                self._send(f"close {self.alias}")
            except MidiPlaybackError:
                pass
            self.is_open = False
        self.cleanup_temp_file()

    def cleanup_temp_file(self):
        if self.temp_path and self.temp_path.exists():
            try:
                self.temp_path.unlink()
            except OSError:
                pass
        self.temp_path = None

    def _send(self, command):
        error_code = ctypes.windll.winmm.mciSendStringW(command, None, 0, None)
        if error_code:
            buffer = ctypes.create_unicode_buffer(255)
            ctypes.windll.winmm.mciGetErrorStringW(error_code, buffer, 255)
            raise MidiPlaybackError(buffer.value or f"Erreur MCI {error_code}")


class MidiEditorApp(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("Editeur MIDI piano-roll")
        self.geometry("1280x760")
        self.minsize(1060, 650)

        self.source_path = None
        self.original_midi = None
        self.midi = None
        self.track_mutes = {}
        self.note_cache = {}
        self.next_note_id = 1

        self.selected_note_id = None
        self.drag_note_id = None
        self.drag_start_x = 0
        self.drag_start_y = 0
        self.drag_original_start = 0
        self.drag_original_end = 0
        self.drag_original_pitch = 0
        self.is_playing = False
        self.playback_tick = 0
        self.playback_start_tick = 0
        self.playback_started_at = 0
        self.playback_after_id = None
        self.playhead_id = None
        self.updating_position = False
        self.audio_player = WindowsMidiPlayer()
        self.audio_warning_shown = False
        self.soundfont_path = self.find_local_soundfont()

        self.transpose_var = tk.IntVar(value=0)
        self.velocity_var = tk.IntVar(value=100)
        self.tempo_var = tk.DoubleVar(value=72.0)
        self.quantize_var = tk.StringVar(value="120")
        self.grid_var = tk.StringVar(value="120")
        self.skip_drums_var = tk.BooleanVar(value=True)
        self.scope_var = tk.StringVar(value="selected")
        self.draw_velocity_var = tk.IntVar(value=82)
        self.draw_length_var = tk.IntVar(value=240)
        self.position_var = tk.DoubleVar(value=0)
        self.time_var = tk.StringVar(value="00:00.0 / 00:00.0")
        self.playback_scope_var = tk.StringVar(value="track")

        self._configure_style()
        self._build_layout()
        self._set_controls_state(False)
        self.protocol("WM_DELETE_WINDOW", self.on_close)

    def _configure_style(self):
        style = ttk.Style(self)
        if "vista" in style.theme_names():
            style.theme_use("vista")
        style.configure("Title.TLabel", font=("Segoe UI", 15, "bold"))
        style.configure("Toolbar.TFrame", padding=10)
        style.configure("Panel.TFrame", padding=10)

    def _build_layout(self):
        self.columnconfigure(0, weight=1)
        self.rowconfigure(1, weight=1)

        top = ttk.Frame(self, style="Toolbar.TFrame")
        top.grid(row=0, column=0, sticky="ew")
        top.columnconfigure(3, weight=1)

        ttk.Label(top, text="Editeur MIDI piano-roll", style="Title.TLabel").grid(row=0, column=0, padx=(0, 16))
        self.open_button = ttk.Button(top, text="Ouvrir", command=self.open_file)
        self.open_button.grid(row=0, column=1, padx=4)
        self.reload_button = ttk.Button(top, text="Recharger", command=self.reload_file)
        self.reload_button.grid(row=0, column=2, padx=4)
        self.file_label = ttk.Label(top, text="Aucun fichier charge", anchor="w")
        self.file_label.grid(row=0, column=3, sticky="ew", padx=12)
        self.export_button = ttk.Button(top, text="Exporter sous", command=self.save_as)
        self.export_button.grid(row=0, column=4, padx=4)
        self.wav_export_button = ttk.Button(top, text="Exporter WAV", command=self.export_wav)
        self.wav_export_button.grid(row=0, column=5, padx=4)

        body = ttk.PanedWindow(self, orient=tk.HORIZONTAL)
        body.grid(row=1, column=0, sticky="nsew", padx=10, pady=(0, 10))

        left = ttk.Frame(body, style="Panel.TFrame")
        right = ttk.Frame(body, style="Panel.TFrame")
        body.add(left, weight=1)
        body.add(right, weight=4)

        self._build_track_panel(left)
        self._build_piano_panel(right)

        self.status_var = tk.StringVar(value="Ouvre un fichier .mid pour commencer.")
        ttk.Label(self, textvariable=self.status_var, anchor="w", relief=tk.SUNKEN, padding=(8, 4)).grid(
            row=2, column=0, sticky="ew"
        )

    def _build_track_panel(self, parent):
        parent.columnconfigure(0, weight=1)
        parent.rowconfigure(1, weight=1)

        ttk.Label(parent, text="Pistes").grid(row=0, column=0, sticky="w")
        columns = ("name", "notes", "channels", "programs", "muted")
        self.track_tree = ttk.Treeview(parent, columns=columns, show="headings", selectmode="browse")
        self.track_tree.heading("name", text="Nom")
        self.track_tree.heading("notes", text="Notes")
        self.track_tree.heading("channels", text="Canaux")
        self.track_tree.heading("programs", text="Prog.")
        self.track_tree.heading("muted", text="Mute")
        self.track_tree.column("name", minwidth=170, width=210, stretch=True)
        self.track_tree.column("notes", width=60, anchor="center")
        self.track_tree.column("channels", width=70, anchor="center")
        self.track_tree.column("programs", width=65, anchor="center")
        self.track_tree.column("muted", width=55, anchor="center")
        self.track_tree.bind("<<TreeviewSelect>>", lambda _event: self.on_track_selected())
        self.track_tree.grid(row=1, column=0, sticky="nsew", pady=(6, 8))

        buttons = ttk.Frame(parent)
        buttons.grid(row=2, column=0, sticky="ew")
        buttons.columnconfigure((0, 1, 2), weight=1)
        ttk.Button(buttons, text="Mute", command=self.toggle_selected_mute).grid(row=0, column=0, sticky="ew", padx=(0, 4))
        ttk.Button(buttons, text="Supprimer", command=self.delete_selected_track).grid(row=0, column=1, sticky="ew", padx=4)
        ttk.Button(buttons, text="Renommer", command=self.rename_selected_track).grid(row=0, column=2, sticky="ew", padx=(4, 0))

        info_box = ttk.LabelFrame(parent, text="Infos fichier", padding=8)
        info_box.grid(row=3, column=0, sticky="ew", pady=(10, 0))
        self.info_var = tk.StringVar(value="- aucune info -")
        ttk.Label(info_box, textvariable=self.info_var, justify="left").grid(row=0, column=0, sticky="w")

        note_box = ttk.LabelFrame(parent, text="Note selectionnee", padding=8)
        note_box.grid(row=4, column=0, sticky="ew", pady=(10, 0))
        self.note_info_var = tk.StringVar(value="Aucune note")
        ttk.Label(note_box, textvariable=self.note_info_var, justify="left").grid(row=0, column=0, sticky="w")

    def _build_piano_panel(self, parent):
        parent.columnconfigure(0, weight=1)
        parent.rowconfigure(2, weight=1)

        controls = ttk.LabelFrame(parent, text="Edition", padding=10)
        controls.grid(row=0, column=0, sticky="ew")
        for column in range(10):
            controls.columnconfigure(column, weight=1)

        ttk.Label(controls, text="Portee").grid(row=0, column=0, sticky="w")
        ttk.Radiobutton(controls, text="Piste", variable=self.scope_var, value="selected").grid(row=1, column=0, sticky="w")
        ttk.Radiobutton(controls, text="Toutes", variable=self.scope_var, value="all").grid(row=2, column=0, sticky="w")

        ttk.Label(controls, text="Transpose").grid(row=0, column=1, sticky="w")
        ttk.Spinbox(controls, from_=-24, to=24, textvariable=self.transpose_var, width=8).grid(row=1, column=1, sticky="ew", padx=4)
        ttk.Button(controls, text="Appliquer", command=self.apply_transpose).grid(row=2, column=1, sticky="ew", padx=4)

        ttk.Label(controls, text="Velocite %").grid(row=0, column=2, sticky="w")
        ttk.Spinbox(controls, from_=10, to=200, textvariable=self.velocity_var, width=8).grid(row=1, column=2, sticky="ew", padx=4)
        ttk.Button(controls, text="Appliquer", command=self.apply_velocity).grid(row=2, column=2, sticky="ew", padx=4)

        ttk.Label(controls, text="Tempo BPM").grid(row=0, column=3, sticky="w")
        ttk.Spinbox(controls, from_=30, to=240, increment=0.5, textvariable=self.tempo_var, width=8).grid(row=1, column=3, sticky="ew", padx=4)
        ttk.Button(controls, text="Definir", command=self.apply_tempo).grid(row=2, column=3, sticky="ew", padx=4)

        ttk.Label(controls, text="Quantize ticks").grid(row=0, column=4, sticky="w")
        ttk.Combobox(controls, textvariable=self.quantize_var, values=("30", "60", "120", "240", "480"), width=8).grid(
            row=1, column=4, sticky="ew", padx=4
        )
        ttk.Button(controls, text="Quantifier", command=self.apply_quantize).grid(row=2, column=4, sticky="ew", padx=4)

        ttk.Label(controls, text="Grille").grid(row=0, column=5, sticky="w")
        ttk.Combobox(controls, textvariable=self.grid_var, values=("30", "60", "120", "240", "480"), width=8).grid(
            row=1, column=5, sticky="ew", padx=4
        )
        ttk.Button(controls, text="Redessiner", command=self.draw_piano_roll).grid(row=2, column=5, sticky="ew", padx=4)

        ttk.Label(controls, text="Nouvelle vel.").grid(row=0, column=6, sticky="w")
        ttk.Spinbox(controls, from_=1, to=127, textvariable=self.draw_velocity_var, width=8).grid(row=1, column=6, sticky="ew", padx=4)
        ttk.Label(controls, text="Longueur").grid(row=0, column=7, sticky="w")
        ttk.Spinbox(controls, from_=30, to=1920, increment=30, textvariable=self.draw_length_var, width=8).grid(
            row=1, column=7, sticky="ew", padx=4
        )

        ttk.Checkbutton(controls, text="Ignorer batterie ch.10", variable=self.skip_drums_var).grid(row=1, column=8, sticky="w", padx=8)
        ttk.Button(controls, text="Suppr. note", command=self.delete_selected_note).grid(row=2, column=8, sticky="ew", padx=8)
        ttk.Button(controls, text="Reset", command=self.reload_file).grid(row=2, column=9, sticky="ew", padx=4)

        transport = ttk.LabelFrame(parent, text="Lecteur de piste", padding=10)
        transport.grid(row=1, column=0, sticky="ew", pady=(10, 0))
        transport.columnconfigure(6, weight=1)

        self.play_button = ttk.Button(transport, text="Play", command=self.play_track)
        self.play_button.grid(row=0, column=0, padx=(0, 4))
        self.pause_button = ttk.Button(transport, text="Pause", command=self.pause_track)
        self.pause_button.grid(row=0, column=1, padx=4)
        self.stop_button = ttk.Button(transport, text="Stop", command=self.stop_track)
        self.stop_button.grid(row=0, column=2, padx=4)
        ttk.Button(transport, text="Debut", command=lambda: self.seek_to_tick(0)).grid(row=0, column=3, padx=(4, 10))
        ttk.Radiobutton(
            transport,
            text="Piste",
            variable=self.playback_scope_var,
            value="track",
            command=self.on_playback_scope_changed,
        ).grid(row=0, column=4, padx=4)
        ttk.Radiobutton(
            transport,
            text="Tout",
            variable=self.playback_scope_var,
            value="all",
            command=self.on_playback_scope_changed,
        ).grid(row=0, column=5, padx=(0, 10))
        self.position_scale = ttk.Scale(transport, from_=0, to=1, variable=self.position_var, command=self.on_position_scale)
        self.position_scale.grid(row=0, column=6, sticky="ew", padx=4)
        ttk.Label(transport, textvariable=self.time_var, width=18, anchor="e").grid(row=0, column=7, padx=(10, 0))

        canvas_frame = ttk.Frame(parent)
        canvas_frame.grid(row=2, column=0, sticky="nsew", pady=(10, 0))
        canvas_frame.columnconfigure(0, weight=1)
        canvas_frame.rowconfigure(0, weight=1)

        self.canvas = tk.Canvas(canvas_frame, background="#f8f8f8", highlightthickness=1, highlightbackground="#c8c8c8")
        xscroll = ttk.Scrollbar(canvas_frame, orient=tk.HORIZONTAL, command=self.canvas.xview)
        yscroll = ttk.Scrollbar(canvas_frame, orient=tk.VERTICAL, command=self.canvas.yview)
        self.canvas.configure(xscrollcommand=xscroll.set, yscrollcommand=yscroll.set)
        self.canvas.grid(row=0, column=0, sticky="nsew")
        yscroll.grid(row=0, column=1, sticky="ns")
        xscroll.grid(row=1, column=0, sticky="ew")

        self.canvas.bind("<Button-1>", self.on_canvas_click)
        self.canvas.bind("<B1-Motion>", self.on_canvas_drag)
        self.canvas.bind("<ButtonRelease-1>", self.on_canvas_release)
        self.canvas.bind("<Double-Button-1>", self.on_canvas_double_click)
        self.canvas.bind("<Delete>", lambda _event: self.delete_selected_note())
        self.canvas.bind("<BackSpace>", lambda _event: self.delete_selected_note())
        self.canvas.bind("<MouseWheel>", self.on_mousewheel)

        help_box = ttk.LabelFrame(parent, text="Piano-roll", padding=8)
        help_box.grid(row=3, column=0, sticky="ew", pady=(10, 0))
        ttk.Label(
            help_box,
            text=(
                "Double-clic dans la grille pour ajouter une note. Clique une note pour la selectionner, "
                "glisse-la pour la deplacer dans le temps ou en hauteur. Suppr efface la note selectionnee. "
                "Le lecteur peut jouer la piste selectionnee ou toutes les pistes non mutees."
            ),
            wraplength=880,
        ).grid(row=0, column=0, sticky="w")

    def _set_controls_state(self, enabled):
        state = "normal" if enabled else "disabled"
        for child in self.winfo_children():
            self._set_widget_state(child, state)
        if hasattr(self, "open_button"):
            self.open_button.configure(state="normal")
        if hasattr(self, "file_label"):
            self.file_label.configure(state="normal")

    def _set_widget_state(self, widget, state):
        for child in widget.winfo_children():
            self._set_widget_state(child, state)
        if isinstance(widget, (ttk.Button, ttk.Spinbox, ttk.Radiobutton, ttk.Checkbutton, ttk.Combobox, ttk.Scale)):
            try:
                widget.configure(state=state)
            except tk.TclError:
                pass

    def open_file(self):
        path = filedialog.askopenfilename(title="Ouvrir un fichier MIDI", filetypes=SUPPORTED_FILES)
        if path:
            self.load_file(Path(path))

    def load_file(self, path):
        try:
            midi = MidiFile(path)
        except Exception as exc:
            messagebox.showerror("Erreur MIDI", f"Impossible d'ouvrir le fichier:\n{exc}")
            return

        self.source_path = path
        self.original_midi = deepcopy(midi)
        self.midi = midi
        self.track_mutes = {index: False for index in range(len(self.midi.tracks))}
        self.selected_note_id = None
        self.stop_track()
        self.rebuild_all_note_caches()

        self._set_controls_state(True)
        self.file_label.configure(text=str(path))
        self.tempo_var.set(round(self.detect_bpm(), 2))
        self.refresh_all()
        self.set_status(f"Charge: {path.name}")

    def reload_file(self):
        if not self.source_path:
            return
        self.stop_track()
        self.midi = deepcopy(self.original_midi)
        self.track_mutes = {index: False for index in range(len(self.midi.tracks))}
        self.selected_note_id = None
        self.rebuild_all_note_caches()
        self.tempo_var.set(round(self.detect_bpm(), 2))
        self.refresh_all()
        self.set_status("Fichier recharge depuis l'original.")

    def save_as(self):
        if not self.require_midi():
            return
        self.commit_all_note_caches()
        default_name = self.default_output_path().name
        path = filedialog.asksaveasfilename(
            title="Exporter le MIDI modifie",
            defaultextension=".mid",
            initialfile=default_name,
            filetypes=SUPPORTED_FILES,
        )
        if not path:
            return
        try:
            export_midi = self.build_export_midi()
            export_midi.save(path)
        except Exception as exc:
            messagebox.showerror("Erreur export", f"Impossible d'exporter le fichier:\n{exc}")
            return
        self.set_status(f"Exporte: {Path(path).name}")
        messagebox.showinfo("Export termine", f"Fichier sauvegarde:\n{path}")

    def export_wav(self):
        if not self.require_midi():
            return
        if render_midi is None:
            messagebox.showerror("Export WAV indisponible", "Le module render_with_soundfont.py est introuvable.")
            return

        soundfont = self.soundfont_path
        if soundfont is None or not soundfont.exists():
            soundfont = self.ask_soundfont_path()
            if soundfont is None:
                return
        else:
            use_detected = messagebox.askyesno(
                "SoundFont detectee",
                f"Utiliser cette SoundFont ?\n\n{soundfont}\n\nChoisir Non pour en selectionner une autre.",
            )
            if not use_detected:
                soundfont = self.ask_soundfont_path()
                if soundfont is None:
                    return

        default_output = self.default_wav_output_path()
        output = filedialog.asksaveasfilename(
            title="Exporter en WAV avec SoundFont",
            defaultextension=".wav",
            initialfile=default_output.name,
            initialdir=str(default_output.parent),
            filetypes=WAV_FILES,
        )
        if not output:
            return

        self.stop_track(reset_position=False)
        self.commit_all_note_caches()

        with tempfile.NamedTemporaryFile(delete=False, suffix=".mid") as temp_file:
            temp_midi_path = Path(temp_file.name)
        try:
            self.build_export_midi().save(temp_midi_path)
            self.set_status("Export WAV en cours avec FluidSynth...")
            self.update_idletasks()
            render_midi(
                midi_path=temp_midi_path,
                soundfont_path=soundfont,
                output_path=output,
                fluidsynth_path=self.detect_fluidsynth_path(),
                gain=0.75,
            )
        except Exception as exc:
            messagebox.showerror("Erreur export WAV", f"Impossible de creer le WAV:\n{exc}")
            self.set_status("Export WAV echoue.")
            return
        finally:
            try:
                temp_midi_path.unlink()
            except OSError:
                pass

        self.soundfont_path = Path(soundfont)
        self.set_status(f"WAV exporte: {Path(output).name}")
        messagebox.showinfo("Export WAV termine", f"Fichier sauvegarde:\n{output}")

    def ask_soundfont_path(self):
        initial_dir = Path(__file__).with_name("soundfonts")
        path = filedialog.askopenfilename(
            title="Choisir une SoundFont .sf2 ou .sf3",
            initialdir=str(initial_dir if initial_dir.exists() else Path.cwd()),
            filetypes=SOUNDFONT_FILES,
        )
        return Path(path) if path else None

    def find_local_soundfont(self):
        search_roots = [Path(__file__).with_name("soundfonts"), Path.cwd()]
        candidates = []
        for root in search_roots:
            if root.exists():
                candidates.extend(root.rglob("*.sf2"))
                candidates.extend(root.rglob("*.sf3"))
        if not candidates:
            return None
        return sorted(candidates, key=lambda path: ("GeneralUser" not in path.name, len(str(path))))[0]

    def detect_fluidsynth_path(self):
        if find_fluidsynth is None:
            return None
        try:
            return find_fluidsynth()
        except Exception:
            return None

    def default_output_path(self):
        if not self.source_path:
            return Path.cwd() / "midi_pianoroll.mid"
        return self.source_path.with_name(f"{self.source_path.stem}{DEFAULT_OUTPUT_SUFFIX}{self.source_path.suffix}")

    def default_wav_output_path(self):
        if not self.source_path:
            return Path.cwd() / "midi_pianoroll.wav"
        return self.source_path.with_name(f"{self.source_path.stem}{DEFAULT_OUTPUT_SUFFIX}.wav")

    def refresh_all(self):
        self.refresh_track_list()
        self.refresh_info()
        self.draw_piano_roll()
        self.refresh_transport_limits()

    def refresh_track_list(self):
        previous_selection = self.selected_track_index()
        self.track_tree.delete(*self.track_tree.get_children())
        for info in self.scan_tracks():
            values = (info.name, info.notes, info.channels, info.programs, "oui" if info.muted else "non")
            self.track_tree.insert("", "end", iid=str(info.index), values=values)
        children = self.track_tree.get_children()
        if previous_selection is not None and str(previous_selection) in children:
            self.track_tree.selection_set(str(previous_selection))
        elif children and not self.track_tree.selection():
            first_note_track = next(
                (child for child in children if len(self.note_cache.get(int(child), [])) > 0),
                children[0],
            )
            self.track_tree.selection_set(first_note_track)

    def scan_tracks(self):
        if not self.midi:
            return []
        infos = []
        for index, track in enumerate(self.midi.tracks):
            name = self.get_track_name(track) or f"Piste {index + 1}"
            notes = len(self.note_cache.get(index, []))
            channels = sorted({message.channel + 1 for message in track if hasattr(message, "channel")})
            programs = sorted({message.program + 1 for message in track if message.type == "program_change"})
            infos.append(
                TrackInfo(
                    index=index,
                    name=name,
                    notes=notes,
                    channels=", ".join(map(str, channels)) or "-",
                    programs=", ".join(map(str, programs)) or "-",
                    muted=self.track_mutes.get(index, False),
                )
            )
        return infos

    def refresh_info(self):
        if not self.midi:
            self.info_var.set("- aucune info -")
            return
        total_notes = sum(len(notes) for notes in self.note_cache.values())
        duration = self.midi.length if self.midi.type in {0, 1} else 0
        self.info_var.set(
            f"Type MIDI: {self.midi.type}\n"
            f"Pistes: {len(self.midi.tracks)}\n"
            f"Ticks / beat: {self.midi.ticks_per_beat}\n"
            f"Notes: {total_notes}\n"
            f"Duree approx.: {duration / 60:.2f} min\n"
            f"Tempo detecte: {self.detect_bpm():.2f} BPM"
        )
        self.update_selected_note_info()

    def rebuild_all_note_caches(self):
        self.note_cache = {}
        self.next_note_id = 1
        if not self.midi:
            return
        for index, track in enumerate(self.midi.tracks):
            self.note_cache[index] = self.extract_notes(track)

    def extract_notes(self, track):
        notes = []
        active = {}
        absolute_tick = 0
        for message in track:
            absolute_tick += message.time
            if message.type == "note_on" and message.velocity > 0:
                key = (message.channel, message.note)
                active.setdefault(key, []).append((absolute_tick, message.velocity))
            elif message.type == "note_off" or (message.type == "note_on" and message.velocity == 0):
                key = (message.channel, message.note)
                if active.get(key):
                    start, velocity = active[key].pop(0)
                    end = max(start + MIN_NOTE_TICKS, absolute_tick)
                    notes.append(PianoNote(self.next_note_id, start, end, message.note, velocity, message.channel))
                    self.next_note_id += 1
        return notes

    def commit_all_note_caches(self):
        if not self.midi:
            return
        for index in range(len(self.midi.tracks)):
            self.commit_notes_to_track(index)

    def commit_notes_to_track(self, track_index):
        if not self.midi or track_index >= len(self.midi.tracks):
            return
        notes = self.note_cache.get(track_index, [])
        original = self.midi.tracks[track_index]
        events = []
        absolute_tick = 0
        order = 0
        end_of_track_tick = 0

        for message in original:
            absolute_tick += message.time
            if message.type == "end_of_track":
                end_of_track_tick = max(end_of_track_tick, absolute_tick)
                continue
            if message.type in {"note_on", "note_off"}:
                continue
            events.append((absolute_tick, order, message.copy(time=0)))
            order += 1

        for note in notes:
            start = max(0, int(note.start))
            end = max(start + MIN_NOTE_TICKS, int(note.end))
            events.append((start, order, Message("note_on", note=note.pitch, velocity=note.velocity, channel=note.channel, time=0)))
            order += 1
            events.append((end, order, Message("note_off", note=note.pitch, velocity=0, channel=note.channel, time=0)))
            order += 1
            end_of_track_tick = max(end_of_track_tick, end)

        events.append((end_of_track_tick, order, MetaMessage("end_of_track", time=0)))

        events.sort(key=lambda event: (event[0], event[1]))
        rebuilt = MidiTrack()
        previous_tick = 0
        for tick, _order, message in events:
            delta = max(0, tick - previous_tick)
            rebuilt.append(message.copy(time=delta))
            previous_tick = tick
        self.midi.tracks[track_index] = rebuilt

    def build_export_midi(self):
        export_midi = deepcopy(self.midi)
        for index, muted in self.track_mutes.items():
            if muted and index < len(export_midi.tracks):
                export_midi.tracks[index] = self.strip_note_messages(export_midi.tracks[index])
        return export_midi

    def strip_note_messages(self, track):
        new_track = MidiTrack()
        carry_time = 0
        for message in track:
            if message.type in {"note_on", "note_off"}:
                carry_time += message.time
                continue
            new_track.append(message.copy(time=message.time + carry_time))
            carry_time = 0
        return new_track

    def draw_piano_roll(self):
        self.canvas.delete("all")
        self.playhead_id = None
        if not self.midi:
            self.canvas.configure(scrollregion=(0, 0, 1200, 600))
            self.canvas.create_text(24, 24, anchor="nw", text="Ouvre un fichier MIDI pour afficher le piano-roll.", fill="#555555")
            return

        track_index = self.selected_track_index()
        notes = self.note_cache.get(track_index, []) if track_index is not None else []
        max_tick = max([note.end for note in notes] + [self.current_playback_max_tick(), self.midi.ticks_per_beat * 16])
        width = KEYBOARD_WIDTH + int(max_tick * PIXELS_PER_TICK) + 260
        height = HEADER_HEIGHT + ((HIGH_PITCH - LOW_PITCH + 1) * ROW_HEIGHT)
        self.canvas.configure(scrollregion=(0, 0, width, height))

        self.draw_grid(width, height)
        self.draw_notes(notes)
        self.draw_playhead()
        self.update_selected_note_info()

    def draw_grid(self, width, height):
        beat_ticks = self.midi.ticks_per_beat if self.midi else 480
        try:
            grid_ticks = max(1, int(self.grid_var.get()))
        except ValueError:
            grid_ticks = 120

        self.canvas.create_rectangle(0, 0, width, HEADER_HEIGHT, fill="#ececec", outline="#d5d5d5")
        for pitch in range(HIGH_PITCH, LOW_PITCH - 1, -1):
            y = self.pitch_to_y(pitch)
            is_black = pitch % 12 in BLACK_KEYS
            row_color = "#eeeeee" if is_black else "#fbfbfb"
            key_color = "#2f2f2f" if is_black else "#ffffff"
            key_text = "#ffffff" if is_black else "#222222"
            self.canvas.create_rectangle(0, y, KEYBOARD_WIDTH, y + ROW_HEIGHT, fill=key_color, outline="#b0b0b0")
            self.canvas.create_rectangle(KEYBOARD_WIDTH, y, width, y + ROW_HEIGHT, fill=row_color, outline="#e4e4e4")
            if pitch % 12 == 0:
                self.canvas.create_text(8, y + ROW_HEIGHT / 2, anchor="w", text=pitch_name(pitch), fill=key_text, font=("Segoe UI", 8))

        max_tick = int((width - KEYBOARD_WIDTH) / PIXELS_PER_TICK)
        tick = 0
        while tick <= max_tick:
            x = self.tick_to_x(tick)
            if tick % (beat_ticks * 4) == 0:
                color = "#9a9a9a"
                label = f"{(tick // (beat_ticks * 4)) + 1}"
                self.canvas.create_text(x + 4, 5, anchor="nw", text=label, fill="#333333", font=("Segoe UI", 8, "bold"))
            elif tick % beat_ticks == 0:
                color = "#c2c2c2"
            else:
                color = "#dddddd"
            self.canvas.create_line(x, HEADER_HEIGHT, x, height, fill=color)
            tick += grid_ticks
        self.canvas.create_line(KEYBOARD_WIDTH, HEADER_HEIGHT, width, HEADER_HEIGHT, fill="#a8a8a8")

    def draw_notes(self, notes):
        for note in notes:
            x1 = self.tick_to_x(note.start)
            x2 = self.tick_to_x(note.end)
            y1 = self.pitch_to_y(note.pitch) + 2
            y2 = y1 + ROW_HEIGHT - 4
            selected = note.note_id == self.selected_note_id
            fill = "#ffad3b" if selected else "#4f8ef7"
            outline = "#8a4a00" if selected else "#2259b7"
            self.canvas.create_rectangle(x1, y1, x2, y2, fill=fill, outline=outline, width=2 if selected else 1, tags=("note", f"note:{note.note_id}"))
            if x2 - x1 > 38:
                self.canvas.create_text(x1 + 5, y1 + 1, anchor="nw", text=pitch_name(note.pitch), fill="#ffffff", font=("Segoe UI", 8), tags=("note", f"note:{note.note_id}"))

    def on_canvas_click(self, event):
        self.canvas.focus_set()
        if not self.require_midi(silent=True):
            return
        x = self.canvas.canvasx(event.x)
        y = self.canvas.canvasy(event.y)
        note = self.note_at_position(x, y)
        if note:
            self.selected_note_id = note.note_id
            self.drag_note_id = note.note_id
            self.drag_start_x = x
            self.drag_start_y = y
            self.drag_original_start = note.start
            self.drag_original_end = note.end
            self.drag_original_pitch = note.pitch
            self.draw_piano_roll()
        else:
            self.selected_note_id = None
            self.drag_note_id = None
            if x >= KEYBOARD_WIDTH and y >= HEADER_HEIGHT:
                self.seek_to_tick(self.x_to_tick(x))
            self.draw_piano_roll()

    def on_canvas_drag(self, event):
        if not self.drag_note_id:
            return
        note = self.find_note(self.drag_note_id)
        if not note:
            return
        x = self.canvas.canvasx(event.x)
        y = self.canvas.canvasy(event.y)
        try:
            grid_ticks = max(1, int(self.grid_var.get()))
        except ValueError:
            grid_ticks = 120
        tick_delta = round(((x - self.drag_start_x) / PIXELS_PER_TICK) / grid_ticks) * grid_ticks
        pitch_delta = round((self.drag_start_y - y) / ROW_HEIGHT)
        duration = self.drag_original_end - self.drag_original_start
        note.start = max(0, self.drag_original_start + tick_delta)
        note.end = note.start + duration
        note.pitch = clamp(self.drag_original_pitch + pitch_delta, LOW_PITCH, HIGH_PITCH)
        self.draw_piano_roll()

    def on_canvas_release(self, _event):
        if self.drag_note_id is not None:
            track_index = self.selected_track_index()
            if track_index is not None:
                self.commit_notes_to_track(track_index)
            self.refresh_track_list()
            self.refresh_info()
        self.drag_note_id = None

    def on_canvas_double_click(self, event):
        if not self.require_midi(silent=True):
            return
        track_index = self.selected_track_index()
        if track_index is None:
            return
        x = self.canvas.canvasx(event.x)
        y = self.canvas.canvasy(event.y)
        if x < KEYBOARD_WIDTH or y < HEADER_HEIGHT:
            return
        try:
            grid_ticks = max(1, int(self.grid_var.get()))
        except ValueError:
            grid_ticks = 120
        start = max(0, round(self.x_to_tick(x) / grid_ticks) * grid_ticks)
        length = max(MIN_NOTE_TICKS, int(self.draw_length_var.get()))
        pitch = self.y_to_pitch(y)
        channel = self.default_channel_for_track(track_index)
        velocity = clamp(int(self.draw_velocity_var.get()), 1, 127)
        note = PianoNote(self.next_note_id, start, start + length, pitch, velocity, channel)
        self.next_note_id += 1
        self.note_cache.setdefault(track_index, []).append(note)
        self.selected_note_id = note.note_id
        self.commit_notes_to_track(track_index)
        self.refresh_all()
        self.set_status(f"Note ajoutee: {pitch_name(pitch)} a {start} ticks.")

    def on_mousewheel(self, event):
        if event.state & 0x0001:
            self.canvas.xview_scroll(int(-1 * (event.delta / 120)), "units")
        else:
            self.canvas.yview_scroll(int(-1 * (event.delta / 120)), "units")

    def note_at_position(self, x, y):
        track_index = self.selected_track_index()
        if track_index is None:
            return None
        for note in reversed(self.note_cache.get(track_index, [])):
            if note.start <= self.x_to_tick(x) <= note.end and note.pitch == self.y_to_pitch(y):
                return note
        return None

    def find_note(self, note_id):
        track_index = self.selected_track_index()
        if track_index is None:
            return None
        for note in self.note_cache.get(track_index, []):
            if note.note_id == note_id:
                return note
        return None

    def delete_selected_note(self):
        if not self.require_midi(silent=True) or self.selected_note_id is None:
            return
        track_index = self.selected_track_index()
        if track_index is None:
            return
        before = len(self.note_cache.get(track_index, []))
        self.note_cache[track_index] = [note for note in self.note_cache.get(track_index, []) if note.note_id != self.selected_note_id]
        if len(self.note_cache[track_index]) == before:
            return
        deleted_id = self.selected_note_id
        self.selected_note_id = None
        self.commit_notes_to_track(track_index)
        self.refresh_all()
        self.set_status(f"Note supprimee: #{deleted_id}")

    def tick_to_x(self, tick):
        return KEYBOARD_WIDTH + (tick * PIXELS_PER_TICK)

    def x_to_tick(self, x):
        return max(0, int((x - KEYBOARD_WIDTH) / PIXELS_PER_TICK))

    def pitch_to_y(self, pitch):
        return HEADER_HEIGHT + ((HIGH_PITCH - pitch) * ROW_HEIGHT)

    def y_to_pitch(self, y):
        pitch = HIGH_PITCH - int((y - HEADER_HEIGHT) / ROW_HEIGHT)
        return clamp(pitch, LOW_PITCH, HIGH_PITCH)

    def on_track_selected(self):
        self.stop_track(reset_position=False)
        self.selected_note_id = None
        self.playback_tick = min(self.playback_tick, self.current_playback_max_tick())
        self.refresh_transport_limits()
        self.draw_piano_roll()

    def on_playback_scope_changed(self):
        self.stop_track(reset_position=False)
        self.playback_tick = min(self.playback_tick, self.current_playback_max_tick())
        self.refresh_transport_limits()
        self.draw_piano_roll()
        label = "toutes les pistes" if self.playback_scope_var.get() == "all" else "la piste selectionnee"
        self.set_status(f"Lecture reglee sur {label}.")

    def play_track(self):
        if not self.require_midi():
            return
        end_tick = self.current_playback_max_tick()
        if self.playback_tick >= end_tick:
            self.playback_tick = 0
        audio_started = self.start_audio_playback()
        self.is_playing = True
        self.playback_start_tick = self.playback_tick
        self.playback_started_at = time.perf_counter()
        if audio_started:
            label = "toutes les pistes" if self.playback_scope_var.get() == "all" else "la piste selectionnee"
            self.set_status(f"Lecture audio de {label}.")
        else:
            self.set_status("Lecture visuelle seulement: aucune note jouable ou sortie MIDI indisponible.")
        self.schedule_playback_tick()

    def pause_track(self):
        if self.playback_after_id is not None:
            self.after_cancel(self.playback_after_id)
            self.playback_after_id = None
        if self.is_playing:
            self.playback_tick = self.current_playback_tick()
        self.audio_player.pause()
        self.is_playing = False
        self.refresh_transport_ui()
        self.draw_playhead()
        self.set_status("Lecture en pause.")

    def stop_track(self, reset_position=True):
        if self.playback_after_id is not None:
            self.after_cancel(self.playback_after_id)
            self.playback_after_id = None
        self.audio_player.close()
        self.is_playing = False
        if reset_position:
            self.playback_tick = 0
        self.refresh_transport_ui()
        self.draw_playhead()
        if self.midi:
            self.set_status("Lecture stoppee.")

    def schedule_playback_tick(self):
        if self.playback_after_id is not None:
            self.after_cancel(self.playback_after_id)
        self.update_playback()

    def update_playback(self):
        if not self.is_playing or not self.midi:
            return
        end_tick = self.current_playback_max_tick()
        self.playback_tick = min(self.current_playback_tick(), end_tick)
        self.refresh_transport_ui()
        self.draw_playhead()
        self.ensure_playhead_visible()
        if self.playback_tick >= end_tick:
            self.is_playing = False
            self.playback_after_id = None
            self.set_status("Fin de piste atteinte.")
            return
        self.playback_after_id = self.after(33, self.update_playback)

    def current_playback_tick(self):
        bpm = max(1.0, float(self.tempo_var.get()))
        ticks_per_second = (bpm / 60.0) * self.midi.ticks_per_beat
        elapsed = time.perf_counter() - self.playback_started_at
        return int(self.playback_start_tick + (elapsed * ticks_per_second))

    def seek_to_tick(self, tick):
        if not self.require_midi(silent=True):
            return
        end_tick = self.current_playback_max_tick()
        self.playback_tick = clamp(int(tick), 0, end_tick)
        if self.is_playing:
            self.playback_start_tick = self.playback_tick
            self.playback_started_at = time.perf_counter()
            self.start_audio_playback(show_errors=False)
        self.refresh_transport_ui()
        self.draw_playhead()

    def start_audio_playback(self, show_errors=True):
        track_indexes = self.playback_track_indexes()
        if not track_indexes:
            return False
        try:
            playback_midi = self.build_playback_midi(track_indexes, self.playback_tick)
            if playback_midi is None:
                self.audio_player.close()
                return False
            with tempfile.NamedTemporaryFile(delete=False, suffix=".mid") as temp_file:
                temp_path = Path(temp_file.name)
            playback_midi.save(temp_path)
            self.audio_player.play_file(temp_path)
            return True
        except Exception as exc:
            self.audio_player.close()
            message = f"Impossible de lancer le son MIDI: {exc}"
            self.set_status(message)
            if show_errors and not self.audio_warning_shown:
                self.audio_warning_shown = True
                messagebox.showwarning(
                    "Lecture MIDI indisponible",
                    f"{message}\n\nVerifie que le synthetiseur MIDI Windows est disponible et que le volume systeme n'est pas coupe.",
                )
            return False

    def build_playback_midi(self, track_indexes, start_tick):
        playable_indexes = [index for index in track_indexes if self.note_cache.get(index) and not self.track_mutes.get(index, False)]
        if not playable_indexes:
            return None

        playback = MidiFile(type=1, ticks_per_beat=self.midi.ticks_per_beat)
        tempo_track = MidiTrack()
        tempo_track.append(MetaMessage("track_name", name="Tempo", time=0))
        tempo_track.append(MetaMessage("set_tempo", tempo=bpm2tempo(float(self.tempo_var.get())), time=0))
        tempo_track.append(MetaMessage("end_of_track", time=0))
        playback.tracks.append(tempo_track)

        added_tracks = 0
        for track_index in playable_indexes:
            track = self.build_playback_track(track_index, start_tick)
            if track is not None:
                playback.tracks.append(track)
                added_tracks += 1

        return playback if added_tracks else None

    def build_playback_track(self, track_index, start_tick):
        notes = [note for note in self.note_cache.get(track_index, []) if note.end > start_tick]
        if not notes:
            return None

        track = MidiTrack()
        track_name = self.get_track_name(self.midi.tracks[track_index]) or f"Piste {track_index + 1}"
        events = [(0, 0, MetaMessage("track_name", name=track_name, time=0))]
        order = 1
        absolute_tick = 0

        for message in self.midi.tracks[track_index]:
            absolute_tick += message.time
            if message.is_meta or message.type in {"note_on", "note_off"}:
                continue
            if hasattr(message, "channel"):
                relative_tick = max(0, absolute_tick - start_tick)
                events.append((relative_tick, order, message.copy(time=0)))
                order += 1

        for note in notes:
            relative_start = max(0, note.start - start_tick)
            relative_end = max(relative_start + MIN_NOTE_TICKS, note.end - start_tick)
            events.append((relative_start, order, Message("note_on", note=note.pitch, velocity=note.velocity, channel=note.channel, time=0)))
            order += 1
            events.append((relative_end, order, Message("note_off", note=note.pitch, velocity=0, channel=note.channel, time=0)))
            order += 1

        end_tick = max(event[0] for event in events)
        events.append((end_tick, order, MetaMessage("end_of_track", time=0)))
        events.sort(key=lambda event: (event[0], event[1]))

        previous_tick = 0
        for tick, _order, message in events:
            delta = max(0, tick - previous_tick)
            track.append(message.copy(time=delta))
            previous_tick = tick
        return track

    def on_position_scale(self, value):
        if self.updating_position or not self.midi:
            return
        self.seek_to_tick(float(value))

    def refresh_transport_limits(self):
        if not hasattr(self, "position_scale"):
            return
        end_tick = self.current_playback_max_tick() if self.midi else 1
        self.position_scale.configure(to=max(1, end_tick))
        self.playback_tick = clamp(int(self.playback_tick), 0, end_tick)
        self.refresh_transport_ui()

    def refresh_transport_ui(self):
        if not hasattr(self, "position_var"):
            return
        end_tick = self.current_playback_max_tick() if self.midi else 1
        self.updating_position = True
        self.position_var.set(clamp(float(self.playback_tick), 0, end_tick))
        self.updating_position = False
        self.time_var.set(f"{self.format_tick_time(self.playback_tick)} / {self.format_tick_time(end_tick)}")

    def current_playback_max_tick(self):
        if not self.midi:
            return 1
        if self.playback_scope_var.get() == "all":
            endings = [note.end for index in self.playback_track_indexes() for note in self.note_cache.get(index, [])]
            if endings:
                return max(endings)
            return self.midi.ticks_per_beat * 16
        track_index = self.selected_track_index()
        notes = self.note_cache.get(track_index, []) if track_index is not None else []
        if notes:
            return max(note.end for note in notes)
        return self.midi.ticks_per_beat * 16

    def playback_track_indexes(self):
        if not self.midi:
            return []
        if self.playback_scope_var.get() == "all":
            return [index for index in range(len(self.midi.tracks)) if not self.track_mutes.get(index, False)]
        selected = self.selected_track_index()
        if selected is None or self.track_mutes.get(selected, False):
            return []
        return [selected]

    def format_tick_time(self, tick):
        if not self.midi:
            return "00:00.0"
        bpm = max(1.0, float(self.tempo_var.get()))
        seconds = (tick / self.midi.ticks_per_beat) * (60.0 / bpm)
        minutes = int(seconds // 60)
        remainder = seconds - (minutes * 60)
        return f"{minutes:02d}:{remainder:04.1f}"

    def draw_playhead(self):
        if not self.midi or not hasattr(self, "canvas"):
            return
        if self.playhead_id is not None:
            self.canvas.delete(self.playhead_id)
        x = self.tick_to_x(self.playback_tick)
        _x1, _y1, _x2, y2 = self.canvas.bbox("all") or (0, 0, 1200, 600)
        self.playhead_id = self.canvas.create_line(
            x,
            HEADER_HEIGHT,
            x,
            y2,
            fill="#e53935",
            width=2,
            tags=("playhead",),
        )
        self.canvas.tag_raise("playhead")

    def ensure_playhead_visible(self):
        if not self.midi:
            return
        x = self.tick_to_x(self.playback_tick)
        view_left = self.canvas.canvasx(0)
        view_right = self.canvas.canvasx(self.canvas.winfo_width())
        if x < view_left + 90 or x > view_right - 90:
            scrollregion = self.canvas.cget("scrollregion").split()
            if len(scrollregion) == 4:
                total_width = max(1, float(scrollregion[2]))
                self.canvas.xview_moveto(clamp((x - 180) / total_width, 0, 1))

    def selected_track_index(self):
        selection = self.track_tree.selection()
        if not selection:
            return None
        return int(selection[0])

    def target_track_indexes(self):
        if self.scope_var.get() == "all":
            return list(range(len(self.midi.tracks)))
        selected = self.selected_track_index()
        return [selected] if selected is not None else []

    def default_channel_for_track(self, track_index):
        for message in self.midi.tracks[track_index]:
            if hasattr(message, "channel") and message.channel != 9:
                return message.channel
        for note in self.note_cache.get(track_index, []):
            return note.channel
        return 0

    def get_track_name(self, track):
        for message in track:
            if message.type == "track_name":
                return message.name
        return ""

    def require_midi(self, silent=False):
        if self.midi is None:
            if not silent:
                messagebox.showwarning("Aucun fichier", "Ouvre d'abord un fichier MIDI.")
            return False
        return True

    def should_skip_channel(self, channel):
        return self.skip_drums_var.get() and channel == 9

    def apply_transpose(self):
        if not self.require_midi():
            return
        semitones = self.transpose_var.get()
        if semitones == 0:
            self.set_status("Transpose ignoree: valeur 0.")
            return
        changed = 0
        for index in self.target_track_indexes():
            for note in self.note_cache.get(index, []):
                if not self.should_skip_channel(note.channel):
                    note.pitch = clamp(note.pitch + semitones, LOW_PITCH, HIGH_PITCH)
                    changed += 1
            self.commit_notes_to_track(index)
        self.refresh_all()
        self.set_status(f"Transpose appliquee: {semitones:+d} demi-tons sur {changed} notes.")

    def apply_velocity(self):
        if not self.require_midi():
            return
        ratio = self.velocity_var.get() / 100
        changed = 0
        for index in self.target_track_indexes():
            for note in self.note_cache.get(index, []):
                if not self.should_skip_channel(note.channel):
                    note.velocity = clamp(round(note.velocity * ratio), 1, 127)
                    changed += 1
            self.commit_notes_to_track(index)
        self.refresh_all()
        self.set_status(f"Velocite appliquee: {self.velocity_var.get()}% sur {changed} notes.")

    def apply_quantize(self):
        if not self.require_midi():
            return
        try:
            grid = int(self.quantize_var.get())
        except ValueError:
            messagebox.showwarning("Grille invalide", "La quantification doit etre un nombre de ticks.")
            return
        if grid <= 0:
            messagebox.showwarning("Grille invalide", "La quantification doit etre superieure a 0.")
            return
        changed = 0
        for index in self.target_track_indexes():
            for note in self.note_cache.get(index, []):
                if self.should_skip_channel(note.channel):
                    continue
                old_start = note.start
                old_end = note.end
                duration = max(MIN_NOTE_TICKS, note.end - note.start)
                note.start = max(0, round(note.start / grid) * grid)
                note.end = max(note.start + MIN_NOTE_TICKS, round((note.start + duration) / grid) * grid)
                changed += int(note.start != old_start or note.end != old_end)
            self.commit_notes_to_track(index)
        self.refresh_all()
        self.set_status(f"Quantification {grid} ticks appliquee sur {changed} notes.")

    def apply_tempo(self):
        if not self.require_midi():
            return
        bpm = float(self.tempo_var.get())
        if bpm <= 0:
            messagebox.showwarning("Tempo invalide", "Le tempo doit etre superieur a 0 BPM.")
            return
        tempo_message = MetaMessage("set_tempo", tempo=bpm2tempo(bpm), time=0)
        for index, track in enumerate(self.midi.tracks):
            self.midi.tracks[index] = self.remove_tempo_messages(track)
        if not self.midi.tracks:
            self.midi.tracks.append(MidiTrack())
        self.midi.tracks[0].insert(0, tempo_message)
        self.refresh_all()
        self.set_status(f"Tempo defini a {bpm:.2f} BPM.")

    def remove_tempo_messages(self, track):
        new_track = MidiTrack()
        carry_time = 0
        for message in track:
            if message.type == "set_tempo":
                carry_time += message.time
                continue
            new_track.append(message.copy(time=message.time + carry_time))
            carry_time = 0
        return new_track

    def toggle_selected_mute(self):
        if not self.require_midi():
            return
        index = self.selected_track_index()
        if index is None:
            return
        self.track_mutes[index] = not self.track_mutes.get(index, False)
        self.refresh_track_list()
        self.track_tree.selection_set(str(index))
        self.set_status(f"Mute piste {index + 1}: {'oui' if self.track_mutes[index] else 'non'}.")

    def delete_selected_track(self):
        if not self.require_midi():
            return
        index = self.selected_track_index()
        if index is None:
            return
        if len(self.midi.tracks) <= 1:
            messagebox.showwarning("Suppression impossible", "Un fichier MIDI doit garder au moins une piste.")
            return
        name = self.get_track_name(self.midi.tracks[index]) or f"Piste {index + 1}"
        if not messagebox.askyesno("Supprimer la piste", f"Supprimer '{name}' ?"):
            return
        old_mutes = self.track_mutes.copy()
        old_notes = self.note_cache.copy()
        del self.midi.tracks[index]
        self.track_mutes = {}
        self.note_cache = {}
        for new_index in range(len(self.midi.tracks)):
            old_index = new_index if new_index < index else new_index + 1
            self.track_mutes[new_index] = old_mutes.get(old_index, False)
            self.note_cache[new_index] = old_notes.get(old_index, [])
        self.selected_note_id = None
        self.refresh_all()
        self.set_status(f"Piste supprimee: {name}")

    def rename_selected_track(self):
        if not self.require_midi():
            return
        index = self.selected_track_index()
        if index is None:
            return
        dialog = RenameDialog(self, self.get_track_name(self.midi.tracks[index]) or f"Piste {index + 1}")
        if not dialog.result:
            return
        self.set_track_name(index, dialog.result)
        self.refresh_all()
        self.track_tree.selection_set(str(index))
        self.set_status(f"Piste renommee: {dialog.result}")

    def set_track_name(self, track_index, name):
        track = self.midi.tracks[track_index]
        for position, message in enumerate(track):
            if message.type == "track_name":
                track[position] = message.copy(name=name)
                return
        track.insert(0, MetaMessage("track_name", name=name, time=0))

    def update_selected_note_info(self):
        note = self.find_note(self.selected_note_id) if self.selected_note_id else None
        if not note:
            self.note_info_var.set("Aucune note")
            return
        self.note_info_var.set(
            f"Note: {pitch_name(note.pitch)} ({note.pitch})\n"
            f"Debut: {note.start} ticks\n"
            f"Fin: {note.end} ticks\n"
            f"Longueur: {note.end - note.start} ticks\n"
            f"Velocite: {note.velocity}\n"
            f"Canal: {note.channel + 1}"
        )

    def detect_bpm(self):
        if not self.midi:
            return 120.0
        for track in self.midi.tracks:
            for message in track:
                if message.type == "set_tempo":
                    return tempo2bpm(message.tempo)
        return 120.0

    def set_status(self, message):
        self.status_var.set(message)

    def on_close(self):
        self.stop_track(reset_position=False)
        self.audio_player.close()
        self.destroy()


class RenameDialog(tk.Toplevel):
    def __init__(self, parent, current_name):
        super().__init__(parent)
        self.title("Renommer la piste")
        self.resizable(False, False)
        self.result = None
        self.transient(parent)
        self.grab_set()

        self.columnconfigure(0, weight=1)
        ttk.Label(self, text="Nouveau nom").grid(row=0, column=0, sticky="w", padx=12, pady=(12, 4))
        self.name_var = tk.StringVar(value=current_name)
        entry = ttk.Entry(self, textvariable=self.name_var, width=42)
        entry.grid(row=1, column=0, sticky="ew", padx=12)
        entry.select_range(0, tk.END)
        entry.focus_set()

        buttons = ttk.Frame(self)
        buttons.grid(row=2, column=0, sticky="e", padx=12, pady=12)
        ttk.Button(buttons, text="Annuler", command=self.destroy).grid(row=0, column=0, padx=4)
        ttk.Button(buttons, text="OK", command=self.accept).grid(row=0, column=1, padx=4)

        self.bind("<Return>", lambda _event: self.accept())
        self.bind("<Escape>", lambda _event: self.destroy())
        self.wait_window(self)

    def accept(self):
        name = self.name_var.get().strip()
        if name:
            self.result = name
        self.destroy()


def main():
    app = MidiEditorApp()
    app.mainloop()


if __name__ == "__main__":
    main()