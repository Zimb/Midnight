#!/usr/bin/env python3
"""
scrape_ichigos.py
=================
Scrape et télécharge tous les fichiers MIDI disponibles sur ichigos.com.

Phase 1 — COUNT  : parcourt toutes les pages de listing, compte les MIDIs.
Phase 2 — DOWNLOAD : télécharge chaque MIDI avec progression.

Usage :
  python scrape_ichigos.py               # count puis download tout
  python scrape_ichigos.py --count-only  # affiche seulement le compte
  python scrape_ichigos.py --filter "final fantasy,zelda,chrono"
  python scrape_ichigos.py --out-dir data/ichigos_midi
  python scrape_ichigos.py --delay 1.2   # secondes entre requêtes (défaut 0.8)
"""

from __future__ import annotations

import argparse
import re
import sys
import time
import warnings
from dataclasses import dataclass, field
from pathlib import Path
from urllib.parse import urljoin, urlparse, parse_qs

warnings.filterwarnings("ignore")  # supprime InsecureRequestWarning

try:
    import requests
    from bs4 import BeautifulSoup
except ImportError:
    sys.exit("Erreur: pip install requests beautifulsoup4")

# ─────────────────────────────────────────────────────────────────────────────
# Config
# ─────────────────────────────────────────────────────────────────────────────

BASE_URL   = "https://ichigos.com"
HEADERS    = {
    "User-Agent": (
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
        "AppleWebKit/537.36 Chrome/124 Safari/537.36"
    ),
    "Accept-Language": "en-US,en;q=0.9",
}
SESSION    = requests.Session()
SESSION.verify  = False
SESSION.headers.update(HEADERS)

# Toutes les pages de listing connues
LISTING_PAGES = (
    [f"/sheets/{c}" for c in "abcdefghijklmnopqrstuvwxyz"]
    + ["/sheets/fi",     # Final Fantasy (sous-page dédiée)
       "/sheets/new",
       "/sheets/others"]
)

DEFAULT_OUT = Path(__file__).parent / "data" / "ichigos_midi"


# ─────────────────────────────────────────────────────────────────────────────
# Data
# ─────────────────────────────────────────────────────────────────────────────

@dataclass
class SheetEntry:
    title:      str        # titre de la pièce
    source:     str        # jeu / anime source
    composer:   str        # compositeur / transcripteur
    instruments: str       # instruments (texte libre)
    midi_url:   str        # URL complète du .mid
    page:       str        # page de listing d'origine (ex: /sheets/f)
    file_id:    str = ""   # id dans getfile.php


# ─────────────────────────────────────────────────────────────────────────────
# HTML Parsing helpers
# ─────────────────────────────────────────────────────────────────────────────

def _safe_get(url: str, retries: int = 3, delay: float = 2.0) -> requests.Response | None:
    for attempt in range(retries):
        try:
            r = SESSION.get(url, timeout=20)
            if r.status_code == 200:
                return r
            print(f"  [WARN] {url} → HTTP {r.status_code}")
        except Exception as e:
            print(f"  [WARN] {url} tentative {attempt+1}/{retries}: {e}")
            if attempt < retries - 1:
                time.sleep(delay)
    return None


def _extract_id(href: str) -> str:
    """Extrait l'id depuis /res/getfile.php?id=1234&..."""
    try:
        qs = parse_qs(urlparse(href).query)
        return qs.get("id", [""])[0]
    except Exception:
        return ""


def parse_listing_page(page_path: str) -> list[SheetEntry]:
    """
    Parse une page de listing (/sheets/X) et retourne les entrées MIDI.

    Structure HTML : les entrées sont du texte brut + <a> non wrappés dans <tr>.
    Chaque groupe ressemble à :
        <span class='title2'>Source Title</span><br><br>
        Piece Title (Transcribed by X)<br>
        <i>for Piano</i> | <a href='...&type=midi...'>midi</a> | ...<br><br>
    """
    url  = urljoin(BASE_URL, page_path)
    resp = _safe_get(url)
    if resp is None:
        return []

    soup = BeautifulSoup(resp.text, "html.parser")
    entries: list[SheetEntry] = []

    # The main content div — try common class names then fall back to body
    main = (soup.find("div", id="main")
            or soup.find("div", class_="main")
            or soup.find("div", id="content")
            or soup.body)
    if main is None:
        return []

    # Walk all MIDI links; each one is adjacent to its context text
    current_source = ""

    for tag in main.descendants:
        # Track section headers (source game/anime name)
        if getattr(tag, "name", None) == "span" and "title2" in (tag.get("class") or []):
            current_source = tag.get_text(strip=True)
            continue

        # MIDI download link
        if getattr(tag, "name", None) != "a":
            continue
        href = tag.get("href", "")
        if "type=midi" not in href and "type=mid" not in href:
            continue

        full_url = urljoin(BASE_URL, href)
        file_id  = _extract_id(href)

        # Navigate backwards through siblings to find the title text.
        # Structure: NavigableString(title) <br> [\n] [<i>for X</i>] [\n] <br> | pdf | midi ...
        title_text  = ""
        instruments = ""
        try:
            # Walk backwards from the midi <a> tag collecting siblings
            node = tag.previous_sibling
            br_count = 0
            while node is not None and br_count < 6:
                if getattr(node, "name", None) == "i":
                    if not instruments:
                        instruments = re.sub(r"^for\s+", "",
                                             node.get_text(strip=True), flags=re.I)
                elif getattr(node, "name", None) == "br":
                    br_count += 1
                elif getattr(node, "name", None) == "span":
                    # Hit a source header — stop
                    break
                elif getattr(node, "name", None) in ("a",):
                    pass  # skip other links (pdf, mus, youtube)
                else:
                    # NavigableString or tag with text
                    text = (node.get_text(strip=True)
                            if hasattr(node, "get_text")
                            else str(node).strip())
                    if (text and "getfile" not in text and "youtu" not in text
                            and text.lower() not in ("|", "pdf", "midi", "mus",
                                                     "gp5", "xml", "youtube",
                                                     "sib", "nwc")):
                        if not title_text:
                            title_text = text
                node = getattr(node, "previous_sibling", None)

        except Exception:
            pass

        # Extract transcriber from parentheses
        composer = ""
        m = re.search(r"\(Transcribed by ([^)]+)\)", title_text, re.I)
        if m:
            composer   = m.group(1).strip()
            title_text = title_text[:m.start()].strip()

        entries.append(SheetEntry(
            title       = title_text or "(inconnu)",
            source      = current_source,
            composer    = composer,
            instruments = instruments,
            midi_url    = full_url,
            page        = page_path,
            file_id     = file_id,
        ))

    return entries


# ─────────────────────────────────────────────────────────────────────────────
# Phase 1 : COUNT
# ─────────────────────────────────────────────────────────────────────────────

def count_all(delay: float) -> list[SheetEntry]:
    """Parcourt toutes les pages et retourne la liste complète d'entrées MIDI."""
    all_entries: list[SheetEntry]   = []
    seen_urls:   set[str]           = set()
    by_page:     dict[str, int]     = {}

    print()
    print("=" * 64)
    print("  PHASE 1 — COMPTAGE")
    print("=" * 64)

    for i, page in enumerate(LISTING_PAGES):
        label = f"{BASE_URL}{page}"
        print(f"  [{i+1:2d}/{len(LISTING_PAGES)}] {label:<50}", end=" ... ", flush=True)

        entries = parse_listing_page(page)

        # Deduplicate across pages (same file_id can appear on /sheets/f and /sheets/fi)
        unique = [e for e in entries if e.midi_url not in seen_urls]
        seen_urls.update(e.midi_url for e in unique)

        all_entries.extend(unique)
        by_page[page] = len(unique)

        print(f"{len(unique):4d} MIDIs  (total {len(all_entries)})")
        time.sleep(delay)

    # Summary
    print()
    print("=" * 64)
    print(f"  TOTAL MIDIs trouvés    : {len(all_entries)}")

    # Top sources
    from collections import Counter
    source_ctr = Counter(e.source for e in all_entries if e.source)
    print(f"  Sources (jeux/animes)  : {len(source_ctr)}")
    print()
    print("  Top 20 sources par nb de MIDIs :")
    for src, cnt in source_ctr.most_common(20):
        bar = "█" * min(cnt, 40)
        print(f"    {cnt:4d}  {bar}  {src}")

    # Instrument breakdown
    instr_text = " ".join(e.instruments.lower() for e in all_entries)
    piano_ct  = instr_text.count("piano")
    harp_ct   = instr_text.count("harp")
    guitar_ct = instr_text.count("guitar")
    string_ct = instr_text.count("violin") + instr_text.count("cello") + instr_text.count("viola")
    flute_ct  = instr_text.count("flute")
    print()
    print(f"  Instruments détectés (mentions dans les métadonnées) :")
    print(f"    Piano   : {piano_ct}")
    print(f"    Harpe   : {harp_ct}")
    print(f"    Guitare : {guitar_ct}")
    print(f"    Cordes  : {string_ct}")
    print(f"    Flûte   : {flute_ct}")
    print()

    return all_entries


# ─────────────────────────────────────────────────────────────────────────────
# Phase 2 : DOWNLOAD
# ─────────────────────────────────────────────────────────────────────────────

def sanitize_filename(s: str, max_len: int = 80) -> str:
    """Crée un nom de fichier valide depuis une chaîne libre."""
    s = re.sub(r'[<>:"/\\|?*]', "_", s)
    s = re.sub(r"\s+", " ", s).strip()
    return s[:max_len]


def download_all(
    entries:    list[SheetEntry],
    out_dir:    Path,
    filter_kws: list[str],
    delay:      float,
) -> None:
    """Télécharge les MIDIs avec progression et gestion d'erreurs."""
    out_dir.mkdir(parents=True, exist_ok=True)

    # Apply keyword filter
    if filter_kws:
        kws_lower = [k.lower() for k in filter_kws]
        filtered = [
            e for e in entries
            if any(
                kw in e.source.lower() or kw in e.title.lower()
                for kw in kws_lower
            )
        ]
        print(f"  Filtre actif : {', '.join(filter_kws)}")
        print(f"  MIDIs filtrés: {len(filtered)} / {len(entries)}")
    else:
        filtered = entries

    print()
    print("=" * 64)
    print("  PHASE 2 — TÉLÉCHARGEMENT")
    print("=" * 64)
    print(f"  Destination : {out_dir}")
    print(f"  Fichiers    : {len(filtered)}")
    print()

    ok = 0
    skipped = 0
    errors  = 0
    t_start = time.time()

    for i, entry in enumerate(filtered):
        # Build safe filename: "Source - Title.mid"
        source_safe = sanitize_filename(entry.source)  if entry.source else "Misc"
        title_safe  = sanitize_filename(entry.title)   if entry.title  else f"id_{entry.file_id}"

        # Organise in subfolders by source
        src_dir = out_dir / source_safe
        src_dir.mkdir(exist_ok=True)

        uid      = entry.file_id or str(i)
        filename = f"{title_safe}_{uid}.mid"
        dest     = src_dir / filename

        elapsed  = time.time() - t_start
        speed    = (ok + errors) / max(elapsed, 1)
        remaining = (len(filtered) - i) / max(speed, 0.001)

        print(
            f"  [{i+1:4d}/{len(filtered)}]"
            f"  {ok:4d}↓  {errors}✗"
            f"  ETA {remaining/60:.1f}min"
            f"  {source_safe[:22]:<22} / {title_safe[:35]:<35}",
            end="  ",
        )

        # Skip already downloaded
        if dest.exists() and dest.stat().st_size > 100:
            print("SKIP (existe)")
            skipped += 1
            continue

        # Download
        try:
            resp = SESSION.get(entry.midi_url, timeout=30, stream=True)
            if resp.status_code != 200:
                print(f"ERR HTTP {resp.status_code}")
                errors += 1
                time.sleep(delay)
                continue

            content = resp.content
            if len(content) < 50:
                print(f"ERR trop petit ({len(content)} octets)")
                errors += 1
                time.sleep(delay)
                continue

            # Verify MIDI magic bytes (MThd)
            if content[:4] != b"MThd":
                print(f"ERR pas un MIDI (magic={content[:4]})")
                errors += 1
                time.sleep(delay)
                continue

            dest.write_bytes(content)
            print(f"OK  ({len(content)//1024}kB)")
            ok += 1

        except Exception as e:
            print(f"ERR {e}")
            errors += 1

        time.sleep(delay)

    elapsed = time.time() - t_start
    print()
    print("=" * 64)
    print(f"  Résultat final")
    print(f"  Téléchargés  : {ok}")
    print(f"  Déjà présents: {skipped}")
    print(f"  Erreurs      : {errors}")
    print(f"  Durée totale : {elapsed/60:.1f} min")
    print(f"  Dossier      : {out_dir}")
    print("=" * 64)


# ─────────────────────────────────────────────────────────────────────────────
# Entry point
# ─────────────────────────────────────────────────────────────────────────────

def main() -> None:
    parser = argparse.ArgumentParser(
        description="Scraper MIDI — ichigos.com",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument(
        "--count-only", action="store_true",
        help="Affiche seulement le comptage, ne télécharge pas",
    )
    parser.add_argument(
        "--filter", type=str, default="",
        help="Sous-chaînes de filtrage des sources/titres (séparées par virgule)",
    )
    parser.add_argument(
        "--out-dir", type=Path, default=DEFAULT_OUT,
        help=f"Dossier de destination (défaut: {DEFAULT_OUT})",
    )
    parser.add_argument(
        "--delay", type=float, default=0.8,
        help="Délai entre requêtes en secondes (défaut: 0.8)",
    )
    args = parser.parse_args()

    filter_kws = [k.strip() for k in args.filter.split(",") if k.strip()]

    entries = count_all(args.delay)

    if args.count_only:
        print("  (--count-only : téléchargement ignoré)")
        return

    download_all(entries, args.out_dir, filter_kws, args.delay)


if __name__ == "__main__":
    main()
