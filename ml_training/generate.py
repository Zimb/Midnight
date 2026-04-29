"""
Génère un MIDI conditionné par une émotion choisie, à partir d'un MIDI seed
ou d'un démarrage à froid.

Usage :
    python generate.py --emotion happy --out generated.mid
    python generate.py --emotion sad --seed_midi ../ghibli_proper.mid --max_tokens 800
"""
from __future__ import annotations

import argparse
from pathlib import Path

import torch
from miditok import REMI
from symusic import Score

from model import EmotionGPT, GPTConfig

ROOT = Path(__file__).parent
TOKENIZER_JSON = ROOT / "data" / "tokenizer.json"
CKPT_BEST = ROOT / "checkpoints" / "best.pt"

EMOTION_NAMES = {
    "happy":    0,  # Q1
    "tense":    1,  # Q2
    "epic":     1,  # alias Q2
    "sad":      2,  # Q3
    "peaceful": 3,  # Q4
    "calm":     3,
    "any":      4,  # UNK
}


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser()
    p.add_argument("--emotion", choices=list(EMOTION_NAMES), default="happy")
    p.add_argument("--seed_midi", type=str, default=None,
                   help="MIDI de démarrage. Si absent, démarre à froid.")
    p.add_argument("--seed_tokens", type=int, default=64,
                   help="Nb de tokens du seed à conserver.")
    p.add_argument("--max_tokens", type=int, default=512,
                   help="Tokens à générer.")
    p.add_argument("--temperature", type=float, default=1.0)
    p.add_argument("--top_k", type=int, default=40)
    p.add_argument("--out", type=str, default="generated.mid")
    p.add_argument("--ckpt", type=str, default=str(CKPT_BEST))
    return p.parse_args()


def main() -> int:
    args = parse_args()
    device = "cuda" if torch.cuda.is_available() else "cpu"

    ckpt = torch.load(args.ckpt, map_location=device, weights_only=False)
    cfg = GPTConfig(**ckpt["config"])
    model = EmotionGPT(cfg).to(device)
    model.load_state_dict(ckpt["model"])
    model.eval()

    tokenizer = REMI(params=TOKENIZER_JSON)

    # seed
    if args.seed_midi:
        score = Score(args.seed_midi)
        toks = tokenizer(score)
        ids = []
        if isinstance(toks, list):
            for t in toks:
                ids.extend(t.ids)
        else:
            ids = list(toks.ids)
        ids = ids[: args.seed_tokens] if len(ids) > args.seed_tokens else ids
        if not ids:
            ids = [0]
    else:
        ids = [0]  # cold start

    idx = torch.tensor([ids], dtype=torch.long, device=device)
    emo = torch.tensor([EMOTION_NAMES[args.emotion]], dtype=torch.long, device=device)

    print(f"[gen] emotion={args.emotion}  seed_tokens={len(ids)}  "
          f"new={args.max_tokens}  T={args.temperature}  top_k={args.top_k}")
    out = model.generate(
        idx, emo,
        max_new_tokens=args.max_tokens,
        temperature=args.temperature,
        top_k=args.top_k,
    )
    out_ids = out[0].tolist()

    score_out = tokenizer.decode([out_ids])
    score_out.dump_midi(args.out)
    print(f"[done] -> {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
