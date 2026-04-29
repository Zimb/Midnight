"""
Télécharge drengskapur/midi-classical-music depuis HuggingFace (~117 MB)
et l'indexe dans data/classical_index.csv.

Dataset : 4 800 MIDI classiques (Bach, Beethoven, Chopin, Mozart…)
Licence : MIT
Hub     : https://huggingface.co/datasets/drengskapur/midi-classical-music

Ces fichiers seront utilisés comme pré-entraînement (sans labels émotion,
emotion_id=4 = UNK) puis fine-tune sur EMOPIA+VGMIDI avec les vrais labels.

Usage :
    python download_classical.py
"""
from __future__ import annotations

import csv
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).parent
RAW_DIR  = ROOT / "data" / "classical_raw"
INDEX_CSV = ROOT / "data" / "classical_index.csv"

HF_REPO   = "drengskapur/midi-classical-music"


def download() -> None:
    if RAW_DIR.exists() and any(RAW_DIR.rglob("*.mid")):
        print(f"[skip] {RAW_DIR} existe déjà avec des fichiers MIDI.")
        return

    try:
        from huggingface_hub import snapshot_download
    except ImportError:
        print("[error] huggingface_hub non installé. Lance : pip install huggingface_hub")
        sys.exit(1)

    print(f"[download] {HF_REPO} → {RAW_DIR}")
    print("           (~117 MB, quelques minutes selon ta connexion…)")

    local = snapshot_download(
        repo_id=HF_REPO,
        repo_type="dataset",
        local_dir=str(RAW_DIR),
        ignore_patterns=["*.parquet", "*.json", "*.txt", "*.md", "*.csv", ".gitattributes"],
    )
    print(f"[download] terminé : {local}")


def build_index() -> None:
    midi_files = sorted(RAW_DIR.rglob("*.mid")) + sorted(RAW_DIR.rglob("*.midi"))
    if not midi_files:
        print(f"[error] aucun fichier MIDI trouvé dans {RAW_DIR}")
        sys.exit(1)

    INDEX_CSV.parent.mkdir(parents=True, exist_ok=True)
    with open(INDEX_CSV, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["path", "valence", "arousal", "quadrant"])
        for p in midi_files:
            # Pas de label émotion → quadrant UNK (5e classe = 4 en 0-indexed)
            w.writerow([str(p), "0.0", "0.0", "UNK"])

    print(f"[index] {len(midi_files)} fichiers → {INDEX_CSV}")


if __name__ == "__main__":
    download()
    build_index()
    print("\n[ok] Lance ensuite : python prepare_data.py")
