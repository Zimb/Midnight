"""
Télécharge VGMIDI (tarball GitHub) et collecte les chemins MIDI + labels émotion.
Produit data/vgmidi_index.csv avec colonnes : path, valence, arousal, quadrant.

On télécharge le tarball plutôt que `git clone` car le repo VGMIDI contient
des fichiers dans `unlabelled/` avec des caractères illégaux sur NTFS Windows
(guillemets `"`). Avec un tarball + extraction filtrée en Python, on n'écrit
jamais ces fichiers sur le disque.
"""
from __future__ import annotations

import csv
import io
import shutil
import sys
import tarfile
import urllib.request
from pathlib import Path

TARBALL_URL = "https://codeload.github.com/lucasnfe/vgmidi/tar.gz/refs/heads/master"
ROOT = Path(__file__).parent
RAW_DIR = ROOT / "data" / "vgmidi_raw"
INDEX_CSV = ROOT / "data" / "vgmidi_index.csv"

# Caractères illégaux sur NTFS Windows
ILLEGAL_NTFS = set('<>:"|?*')


def _is_safe_for_ntfs(name: str) -> bool:
    return not any(c in ILLEGAL_NTFS for c in name)


def download_and_extract() -> None:
    """
    Télécharge le tarball GitHub et extrait UNIQUEMENT :
      - le dossier `labelled/` (MIDI + annotations)
      - les CSV de labels à la racine
      - le README
    Ignore tout le reste (notamment `unlabelled/` aux noms invalides).
    """
    if RAW_DIR.exists() and any(RAW_DIR.iterdir()):
        print(f"[skip] {RAW_DIR} existe déjà.")
        return
    if RAW_DIR.exists():
        shutil.rmtree(RAW_DIR, ignore_errors=True)
    RAW_DIR.mkdir(parents=True, exist_ok=True)

    print(f"[download] {TARBALL_URL}")
    with urllib.request.urlopen(TARBALL_URL) as resp:
        data = resp.read()
    print(f"[download] {len(data)/1e6:.1f} MB reçus, extraction...")

    extracted = 0
    skipped_invalid = 0
    skipped_filtered = 0

    with tarfile.open(fileobj=io.BytesIO(data), mode="r:gz") as tar:
        for member in tar.getmembers():
            # Le tarball place tout sous "vgmidi-<sha>/" ; on enlève ce préfixe
            parts = member.name.split("/", 1)
            if len(parts) < 2:
                continue
            rel = parts[1]
            if not rel:
                continue

            # Filtre : on ne garde que labelled/ et les fichiers à la racine
            top = rel.split("/", 1)[0]
            if top == "unlabelled":
                skipped_filtered += 1
                continue
            # Exclure formats lourds/inutiles
            if rel.lower().endswith((".wav", ".mp3", ".flac", ".ogg", ".zip")):
                skipped_filtered += 1
                continue

            # Sécurité : noms invalides NTFS
            if not all(_is_safe_for_ntfs(p) for p in rel.split("/")):
                skipped_invalid += 1
                continue

            target = RAW_DIR / rel
            if member.isdir():
                target.mkdir(parents=True, exist_ok=True)
                continue
            if not member.isfile():
                continue

            target.parent.mkdir(parents=True, exist_ok=True)
            f = tar.extractfile(member)
            if f is None:
                continue
            with target.open("wb") as out:
                shutil.copyfileobj(f, out)
            extracted += 1

    print(f"[extract] {extracted} fichiers extraits, "
          f"{skipped_filtered} filtrés (unlabelled/audio), "
          f"{skipped_invalid} ignorés (noms invalides NTFS).")


def quadrant(valence: float, arousal: float) -> str:
    """Q1 happy, Q2 tense, Q3 sad, Q4 peaceful."""
    if valence >= 0 and arousal >= 0:
        return "Q1"
    if valence < 0 and arousal >= 0:
        return "Q2"
    if valence < 0 and arousal < 0:
        return "Q3"
    return "Q4"


def build_index() -> None:
    """
    VGMIDI fournit deux corpus :
      - labelled/  : annotés (valence, arousal) -> on les utilise pour le conditioning
      - unlabelled/: non annotés -> ignorés ici (réutilisables plus tard pour pré-entraînement)

    Le repo expose un CSV des annotations (vgmidi_labelled.csv ou similaire).
    Si le format change, ajuste les noms de colonnes ci-dessous.
    """
    INDEX_CSV.parent.mkdir(parents=True, exist_ok=True)

    candidates = list(RAW_DIR.rglob("*.csv"))
    label_csv = None
    for c in candidates:
        if "label" in c.name.lower():
            label_csv = c
            break
    if label_csv is None:
        print("[warn] aucun CSV de labels trouvé. Index basé sur noms de fichiers uniquement.")

    rows: list[dict[str, object]] = []

    if label_csv is not None:
        print(f"[index] lecture labels depuis {label_csv}")
        with label_csv.open("r", encoding="utf-8") as f:
            reader = csv.DictReader(f)
            for r in reader:
                # tolère plusieurs schémas de colonnes possibles
                fname = r.get("midi") or r.get("filename") or r.get("file") or r.get("name")
                if not fname:
                    continue
                try:
                    val = float(r.get("valence", 0))
                    aro = float(r.get("arousal", 0))
                except ValueError:
                    continue
                # cherche le fichier MIDI correspondant
                matches = list(RAW_DIR.rglob(fname))
                if not matches:
                    matches = list(RAW_DIR.rglob(Path(fname).stem + ".mid"))
                if not matches:
                    continue
                rows.append({
                    "path": str(matches[0].resolve()),
                    "valence": val,
                    "arousal": aro,
                    "quadrant": quadrant(val, aro),
                })

    # fallback / complément : tout MIDI sans label = quadrant inconnu
    seen = {Path(r["path"]).resolve() for r in rows}
    for mid in RAW_DIR.rglob("*.mid"):
        if mid.resolve() in seen:
            continue
        rows.append({
            "path": str(mid.resolve()),
            "valence": 0.0,
            "arousal": 0.0,
            "quadrant": "UNK",
        })

    with INDEX_CSV.open("w", encoding="utf-8", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=["path", "valence", "arousal", "quadrant"])
        writer.writeheader()
        writer.writerows(rows)

    n_labelled = sum(1 for r in rows if r["quadrant"] != "UNK")
    print(f"[done] {len(rows)} MIDI indexés ({n_labelled} avec label émotion). -> {INDEX_CSV}")


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
