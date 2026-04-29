"""
IterableDataset PyTorch pour entraîner en streaming depuis HuggingFace.

Compatible avec roszcz/giant-midi-piano et tout dataset HF qui expose
un champ MIDI bytes (champ "midi" ou "bytes").

Pas de preprocessing préalable, pas de stockage local :
les fichiers sont tokenisés à la volée pendant l'entraînement.

Usage dans train.py :
    from hf_dataset import HFMidiIterableDataset
    ds = HFMidiIterableDataset("roszcz/giant-midi-piano", tokenizer, block_size=512)
"""
from __future__ import annotations

import io
import random
from typing import Iterator

import torch
from torch.utils.data import IterableDataset
from miditok import REMI

# Emotion "any" (UNK=4) : giant-midi-piano n'a pas de labels émotion.
# Le modèle apprend la syntaxe musicale sans conditioning émotion fort.
# Le fine-tune sur EMOPIA+VGMIDI apprend ensuite le conditioning.
DEFAULT_EMOTION = 4  # UNK

MIN_SEQ_LEN = 64
MAX_SEQ_LEN = 1024  # clip avant de découper en blocs


class HFMidiIterableDataset(IterableDataset):
    """
    Lit un dataset HuggingFace en streaming, tokenise les MIDI à la volée,
    découpe en blocs de `block_size` tokens et les retourne comme des
    tuples (x, y, emotion_id).

    Paramètres
    ----------
    hf_repo : str
        Identifiant du repo HF, ex: "roszcz/giant-midi-piano"
    tokenizer : REMI
        Tokenizer miditok déjà initialisé (chargé depuis tokenizer.json)
    block_size : int
        Longueur des séquences d'entrée (en tokens)
    split : str
        Split HF à utiliser ("train", "validation", etc.)
    emotion_id : int
        Emotion à assigner à toutes les séquences (4=UNK pour pré-entraînement)
    buffer_size : int
        Nombre de blocs à pré-générer en mémoire avant de les mélanger.
        Plus c'est grand, plus l'ordre est aléatoire (mais plus de RAM).
    midi_field : str
        Nom du champ contenant les bytes MIDI dans le dataset HF.
    """

    def __init__(
        self,
        hf_repo: str,
        tokenizer: REMI,
        block_size: int = 512,
        split: str = "train",
        emotion_id: int = DEFAULT_EMOTION,
        buffer_size: int = 512,
        midi_field: str | None = None,
    ):
        super().__init__()
        self.hf_repo = hf_repo
        self.tokenizer = tokenizer
        self.block_size = block_size
        self.split = split
        self.emotion_id = emotion_id
        self.buffer_size = buffer_size
        self.midi_field = midi_field  # None = auto-détecté

    def _detect_midi_field(self, example: dict) -> str:
        """Détecte automatiquement le champ qui contient les bytes MIDI."""
        for candidate in ("midi", "bytes", "audio", "data", "content"):
            if candidate in example:
                return candidate
        # Dernier recours : premier champ bytes
        for k, v in example.items():
            if isinstance(v, (bytes, bytearray)):
                return k
        raise ValueError(f"Impossible de trouver un champ MIDI bytes dans : {list(example.keys())}")

    def _tokenize(self, midi_bytes: bytes) -> list[int]:
        """Tokenise les bytes MIDI bruts -> liste d'ids."""
        try:
            from symusic import Score
            score = Score.from_midi(midi_bytes)
            toks = self.tokenizer(score)
        except Exception:
            return []

        ids: list[int] = []
        if isinstance(toks, list):
            for t in toks:
                ids.extend(t.ids)
        else:
            ids = list(toks.ids)
        return ids[:MAX_SEQ_LEN]

    def _iter_blocks(self) -> Iterator[torch.Tensor]:
        """Génère des blocs de tokens depuis le stream HF."""
        from datasets import load_dataset

        ds = load_dataset(self.hf_repo, streaming=True, split=self.split, trust_remote_code=True)
        field = self.midi_field

        for example in ds:
            if field is None:
                try:
                    field = self._detect_midi_field(example)
                except ValueError:
                    continue

            raw = example.get(field)
            if raw is None:
                continue
            if isinstance(raw, dict):
                # Format audio HF : {"bytes": b"...", "path": "..."}
                raw = raw.get("bytes") or raw.get("array")
            if not isinstance(raw, (bytes, bytearray)):
                continue

            ids = self._tokenize(bytes(raw))
            if len(ids) < MIN_SEQ_LEN:
                continue

            step = self.block_size // 2
            for start in range(0, len(ids) - self.block_size - 1, step):
                chunk = ids[start: start + self.block_size + 1]
                if len(chunk) < MIN_SEQ_LEN + 1:
                    continue
                yield torch.tensor(chunk, dtype=torch.long)

    def __iter__(self):
        """
        Itère sur les blocs avec un shuffle par buffer en mémoire.
        Renvoie (x, y, emotion) comme VGMidiDataset.
        """
        buffer: list[torch.Tensor] = []
        emo = torch.tensor(self.emotion_id, dtype=torch.long)

        for block in self._iter_blocks():
            buffer.append(block)
            if len(buffer) >= self.buffer_size:
                random.shuffle(buffer)
                while buffer:
                    seq = buffer.pop()
                    x = seq[:-1]
                    y = seq[1:]
                    yield x, y, emo

        # Vider le buffer restant
        random.shuffle(buffer)
        for seq in buffer:
            x = seq[:-1]
            y = seq[1:]
            yield x, y, emo
