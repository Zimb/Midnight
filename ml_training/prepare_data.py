"""
Tokenisation REMI de tous les MIDI indexés + cache PyTorch.

Sortie :
  data/tokens.pt              -> dict {sequences: list[Tensor], emotions: list[int]}
  data/tokenizer.json         -> tokenizer miditok (à recharger pour la génération)
"""
from __future__ import annotations

import csv
import sys
from pathlib import Path

import torch
from miditok import REMI, TokenizerConfig
from symusic import Score
from tqdm import tqdm

ROOT = Path(__file__).parent
# Tous les fichiers index disponibles sont fusionnés automatiquement
INDEX_CSVS = [
    ROOT / "data" / "vgmidi_index.csv",
    ROOT / "data" / "emopia_index.csv",      # ajouté si download_emopia.py a tourné
    ROOT / "data" / "classical_index.csv",   # ajouté si download_classical.py a tourné
    ROOT / "data" / "midillm_index.csv",     # ajouté si generate_dataset.py a tourné
]
TOKENS_PT = ROOT / "data" / "tokens.pt"
TOKENIZER_JSON = ROOT / "data" / "tokenizer.json"

# 4 émotions + 1 inconnue (utilisée comme "wildcard" à l'inférence)
EMOTION_TO_ID = {"Q1": 0, "Q2": 1, "Q3": 2, "Q4": 3, "UNK": 4}
NUM_EMOTIONS = len(EMOTION_TO_ID)

# Bornes : on coupe les morceaux trop longs en sous-séquences
MAX_SEQ_LEN = 1024
MIN_SEQ_LEN = 64


def build_tokenizer() -> REMI:
    config = TokenizerConfig(
        num_velocities=16,
        use_chords=False,
        use_programs=False,
        use_tempos=True,
        num_tempos=24,
        use_time_signatures=True,
        use_rests=True,
        beat_res={(0, 4): 8, (4, 12): 4},
    )
    return REMI(config)


def main() -> int:
    available = [p for p in INDEX_CSVS if p.exists()]
    if not available:
        print(f"[error] Aucun index trouvé. Lance download_vgmidi.py et/ou download_emopia.py d'abord.", file=sys.stderr)
        return 1

    # Fusion de tous les index disponibles
    rows: list[dict] = []
    for csv_path in available:
        with csv_path.open("r", encoding="utf-8") as f:
            rows.extend(csv.DictReader(f))
        print(f"[index] {csv_path.name} chargé ({len(rows)} entrées cumulées)")

    tokenizer = build_tokenizer()
    sequences: list[torch.Tensor] = []
    emotions: list[int] = []
    skipped = 0

    for row in tqdm(rows, desc="Tokenizing"):
        path = Path(row["path"])
        if not path.exists():
            skipped += 1
            continue
        try:
            score = Score(str(path))
            tokens = tokenizer(score)
        except Exception as e:  # MIDI corrompu / vide
            tqdm.write(f"[skip] {path.name}: {e}")
            skipped += 1
            continue

        # miditok renvoie une liste TokSequence (1 par piste). On les concatène
        # pour rester monophonique conceptuellement (le piano roll utilisateur).
        ids: list[int] = []
        if isinstance(tokens, list):
            for ts in tokens:
                ids.extend(ts.ids)
        else:
            ids = list(tokens.ids)

        if len(ids) < MIN_SEQ_LEN:
            skipped += 1
            continue

        emo = EMOTION_TO_ID.get(row["quadrant"], EMOTION_TO_ID["UNK"])

        # découpage en chunks chevauchants
        step = MAX_SEQ_LEN // 2
        for start in range(0, len(ids) - MIN_SEQ_LEN, step):
            chunk = ids[start : start + MAX_SEQ_LEN]
            if len(chunk) < MIN_SEQ_LEN:
                continue
            sequences.append(torch.tensor(chunk, dtype=torch.long))
            emotions.append(emo)

    print(f"[done] {len(sequences)} séquences extraites, {skipped} fichiers ignorés.")
    print(f"[info] vocab size = {tokenizer.vocab_size}")

    TOKENS_PT.parent.mkdir(parents=True, exist_ok=True)
    torch.save(
        {
            "sequences": sequences,
            "emotions": torch.tensor(emotions, dtype=torch.long),
            "vocab_size": tokenizer.vocab_size,
            "num_emotions": NUM_EMOTIONS,
            "max_seq_len": MAX_SEQ_LEN,
        },
        TOKENS_PT,
    )
    tokenizer.save(TOKENIZER_JSON)
    print(f"[save] {TOKENS_PT}")
    print(f"[save] {TOKENIZER_JSON}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
