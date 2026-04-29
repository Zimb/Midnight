"""
Télécharge EMOPIA depuis Zenodo et l'indexe dans data/emopia_index.csv.

EMOPIA : ~1 087 clips pop piano annotés Q1/Q2/Q3/Q4 (même schéma que VGMIDI).
Structure du zip :
    EMOPIA_1.0/
        Q1/  (happy  : valence+, arousal+)
        Q2/  (tense  : valence-, arousal+)
        Q3/  (sad    : valence-, arousal-)
        Q4/  (peaceful: valence+, arousal-)

Référence : ISMIR 2021 — https://annahung31.github.io/EMOPIA/
Licence    : CC BY-NC-SA 4.0 (non-commercial)
Zenodo     : https://zenodo.org/record/5090631
"""
from __future__ import annotations

import csv
import io
import shutil
import sys
import zipfile
import urllib.request
from pathlib import Path

# URL directe du fichier zip sur Zenodo
ZENODO_URL = "https://zenodo.org/record/5090631/files/EMOPIA_1.0.zip?download=1"

ROOT = Path(__file__).parent
RAW_DIR = ROOT / "data" / "emopia_raw"
INDEX_CSV = ROOT / "data" / "emopia_index.csv"

# Mapping dossier -> (valence, arousal, quadrant)
QUADRANT_META = {
    "Q1": ( 1.0,  1.0, "Q1"),
    "Q2": (-1.0,  1.0, "Q2"),
    "Q3": (-1.0, -1.0, "Q3"),
    "Q4": ( 1.0, -1.0, "Q4"),
}


def download_and_extract() -> None:
    if RAW_DIR.exists() and any(RAW_DIR.iterdir()):
        print(f"[skip] {RAW_DIR} existe déjà.")
        return
    if RAW_DIR.exists():
        shutil.rmtree(RAW_DIR, ignore_errors=True)
    RAW_DIR.mkdir(parents=True, exist_ok=True)

    print(f"[download] {ZENODO_URL}")
    print("           (fichier ~150 MB, patiente quelques minutes...)")

    with urllib.request.urlopen(ZENODO_URL) as resp:
        data = resp.read()
    print(f"[download] {len(data)/1e6:.1f} MB reçus, extraction...")

    extracted = 0
    with zipfile.ZipFile(io.BytesIO(data)) as zf:
        for member in zf.infolist():
            name = member.filename
            # On ne garde que les .mid dans Q1/Q2/Q3/Q4
            parts = name.replace("\\", "/").split("/")
            # Structure attendue : EMOPIA_1.0/Q1/xxx.mid
            if len(parts) < 3:
                continue
            quadrant_part = parts[1] if parts[0].startswith("EMOPIA") else parts[0]
            if quadrant_part not in QUADRANT_META:
                continue
            if not name.lower().endswith((".mid", ".midi")):
                continue
            if member.is_dir():
                continue

            # Chemin de destination : data/emopia_raw/Q1/xxx.mid
            rel = f"{quadrant_part}/{Path(name).name}"
            target = RAW_DIR / rel
            target.parent.mkdir(parents=True, exist_ok=True)
            with zf.open(member) as src, target.open("wb") as dst:
                shutil.copyfileobj(src, dst)
            extracted += 1

    print(f"[extract] {extracted} fichiers MIDI extraits -> {RAW_DIR}")


def build_index() -> None:
    INDEX_CSV.parent.mkdir(parents=True, exist_ok=True)
    rows: list[dict] = []

    for quadrant, (valence, arousal, q) in QUADRANT_META.items():
        qdir = RAW_DIR / quadrant
        if not qdir.exists():
            print(f"[warn] dossier {qdir} introuvable, quadrant {quadrant} ignoré.")
            continue
        for mid in sorted(qdir.rglob("*.mid")):
            rows.append({
                "path":     str(mid.resolve()),
                "valence":  valence,
                "arousal":  arousal,
                "quadrant": q,
            })

    with INDEX_CSV.open("w", encoding="utf-8", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=["path", "valence", "arousal", "quadrant"])
        writer.writeheader()
        writer.writerows(rows)

    for q in QUADRANT_META:
        n = sum(1 for r in rows if r["quadrant"] == q)
        print(f"  {q}: {n} morceaux")
    print(f"[done] {len(rows)} MIDI indexés -> {INDEX_CSV}")


def main() -> int:
    try:
        download_and_extract()
    except Exception as e:
        print(f"[error] téléchargement échoué : {e}", file=sys.stderr)
        return 1
    build_index()
    return 0


if __name__ == "__main__":
    sys.exit(main())
