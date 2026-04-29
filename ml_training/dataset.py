"""Dataset PyTorch : séquences tokenisées + label émotion (conditioning)."""
from __future__ import annotations

from pathlib import Path

import torch
from torch.utils.data import Dataset


class VGMidiDataset(Dataset):
    """
    Chaque item est :
      input_ids  : LongTensor [seq_len]      (tokens)
      emotion_id : LongTensor scalaire       (0..NUM_EMOTIONS-1)

    Le modèle prepend un "emotion embedding" -> pas besoin d'insérer un token spécial
    dans la séquence elle-même.
    """

    def __init__(self, tokens_pt: Path, block_size: int = 512):
        data = torch.load(tokens_pt, weights_only=False)
        self.sequences: list[torch.Tensor] = data["sequences"]
        self.emotions: torch.Tensor = data["emotions"]
        self.vocab_size: int = data["vocab_size"]
        self.num_emotions: int = data["num_emotions"]
        self.block_size = block_size

    def __len__(self) -> int:
        return len(self.sequences)

    def __getitem__(self, idx: int):
        seq = self.sequences[idx]
        # crop ou pad à block_size + 1 (pour input/target décalés)
        if len(seq) > self.block_size + 1:
            start = torch.randint(0, len(seq) - self.block_size - 1, (1,)).item()
            seq = seq[start : start + self.block_size + 1]
        x = seq[:-1].clone()
        y = seq[1:].clone()
        # pad si trop court
        pad = self.block_size - len(x)
        if pad > 0:
            x = torch.cat([x, torch.zeros(pad, dtype=torch.long)])
            y = torch.cat([y, torch.full((pad,), -100, dtype=torch.long)])  # -100 = ignore_index
        return x, y, self.emotions[idx]
