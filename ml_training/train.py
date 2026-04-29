"""
Boucle d'entraînement du Transformer conditionné émotion sur VGMIDI.

Usage :
    python train.py
    python train.py --epochs 50 --batch_size 32 --lr 3e-4
"""
from __future__ import annotations

import argparse
import math
import time
from pathlib import Path

import torch
from torch.utils.data import DataLoader, random_split

from dataset import VGMidiDataset
from model import EmotionGPT, GPTConfig

ROOT = Path(__file__).parent
TOKENS_PT = ROOT / "data" / "tokens.pt"
CKPT_DIR = ROOT / "checkpoints"


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser()
    p.add_argument("--epochs", type=int, default=40)
    p.add_argument("--batch_size", type=int, default=24)
    p.add_argument("--lr", type=float, default=3e-4)
    p.add_argument("--weight_decay", type=float, default=0.01)
    p.add_argument("--block_size", type=int, default=512)
    p.add_argument("--warmup_steps", type=int, default=200)
    p.add_argument("--val_split", type=float, default=0.1)
    p.add_argument("--seed", type=int, default=1337)
    p.add_argument("--compile", action="store_true", help="torch.compile (PyTorch 2.x)")
    return p.parse_args()


def cosine_lr(step: int, max_steps: int, base_lr: float, warmup: int) -> float:
    if step < warmup:
        return base_lr * step / max(warmup, 1)
    progress = (step - warmup) / max(max_steps - warmup, 1)
    return 0.5 * base_lr * (1 + math.cos(math.pi * min(progress, 1.0)))


def main() -> int:
    args = parse_args()
    torch.manual_seed(args.seed)

    if not TOKENS_PT.exists():
        print(f"[error] {TOKENS_PT} introuvable. Lance prepare_data.py d'abord.")
        return 1

    device = "cuda" if torch.cuda.is_available() else "cpu"
    print(f"[device] {device}")
    if device == "cuda":
        torch.set_float32_matmul_precision("high")
        print(f"[gpu] {torch.cuda.get_device_name(0)}")

    full_ds = VGMidiDataset(TOKENS_PT, block_size=args.block_size)
    n_val = max(1, int(len(full_ds) * args.val_split))
    n_train = len(full_ds) - n_val
    train_ds, val_ds = random_split(
        full_ds, [n_train, n_val], generator=torch.Generator().manual_seed(args.seed)
    )

    train_loader = DataLoader(
        train_ds, batch_size=args.batch_size, shuffle=True,
        num_workers=2, pin_memory=(device == "cuda"), drop_last=True,
    )
    val_loader = DataLoader(
        val_ds, batch_size=args.batch_size, shuffle=False,
        num_workers=2, pin_memory=(device == "cuda"),
    )
    print(f"[data] train={len(train_ds)}  val={len(val_ds)}  vocab={full_ds.vocab_size}")

    cfg = GPTConfig(
        vocab_size=full_ds.vocab_size,
        num_emotions=full_ds.num_emotions,
        block_size=args.block_size,
        n_layer=4, n_head=4, d_model=192, dropout=0.1,
    )
    model = EmotionGPT(cfg).to(device)

    if args.compile and hasattr(torch, "compile"):
        print("[compile] torch.compile activé")
        model = torch.compile(model)

    optim = torch.optim.AdamW(
        model.parameters(), lr=args.lr, weight_decay=args.weight_decay,
        betas=(0.9, 0.95),
    )
    scaler = torch.amp.GradScaler("cuda", enabled=(device == "cuda"))

    max_steps = args.epochs * len(train_loader)
    CKPT_DIR.mkdir(exist_ok=True)
    best_val = float("inf")
    step = 0

    for epoch in range(1, args.epochs + 1):
        model.train()
        t0 = time.time()
        running = 0.0
        for x, y, emo in train_loader:
            x, y, emo = x.to(device, non_blocking=True), y.to(device, non_blocking=True), emo.to(device, non_blocking=True)

            lr = cosine_lr(step, max_steps, args.lr, args.warmup_steps)
            for g in optim.param_groups:
                g["lr"] = lr

            optim.zero_grad(set_to_none=True)
            with torch.amp.autocast(device_type=device, dtype=torch.float16, enabled=(device == "cuda")):
                _, loss = model(x, emo, y)
            scaler.scale(loss).backward()
            scaler.unscale_(optim)
            torch.nn.utils.clip_grad_norm_(model.parameters(), 1.0)
            scaler.step(optim)
            scaler.update()

            running += loss.item()
            step += 1

        train_loss = running / len(train_loader)

        # validation
        model.eval()
        vrun = 0.0
        with torch.no_grad():
            for x, y, emo in val_loader:
                x, y, emo = x.to(device), y.to(device), emo.to(device)
                with torch.amp.autocast(device_type=device, dtype=torch.float16, enabled=(device == "cuda")):
                    _, loss = model(x, emo, y)
                vrun += loss.item()
        val_loss = vrun / max(len(val_loader), 1)
        dt = time.time() - t0

        print(
            f"[epoch {epoch:02d}/{args.epochs}] "
            f"train={train_loss:.4f}  val={val_loss:.4f}  "
            f"lr={lr:.2e}  ({dt:.1f}s)"
        )

        if val_loss < best_val:
            best_val = val_loss
            ckpt = {
                "model": (model._orig_mod if hasattr(model, "_orig_mod") else model).state_dict(),
                "config": cfg.__dict__,
                "val_loss": val_loss,
                "epoch": epoch,
            }
            torch.save(ckpt, CKPT_DIR / "best.pt")
            print(f"  -> [save] best.pt (val={val_loss:.4f})")

    # checkpoint final
    final = {
        "model": (model._orig_mod if hasattr(model, "_orig_mod") else model).state_dict(),
        "config": cfg.__dict__,
        "val_loss": val_loss,
        "epoch": args.epochs,
    }
    torch.save(final, CKPT_DIR / "last.pt")
    print(f"[done] best val_loss = {best_val:.4f}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
