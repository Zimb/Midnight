"""
Exporte le modèle entraîné en ONNX pour intégration dans le VST3 C++
(via ONNX Runtime).

Le modèle exporté prend :
    input_ids  : int64 [batch, seq]
    emotion    : int64 [batch]
Et retourne :
    logits     : float32 [batch, seq, vocab]

Côté C++, tu fais une boucle de génération token-par-token (cf. README VST).
"""
from __future__ import annotations

from pathlib import Path

import torch

from model import EmotionGPT, GPTConfig

ROOT = Path(__file__).parent
CKPT = ROOT / "checkpoints" / "best.pt"
OUT_ONNX = ROOT / "checkpoints" / "emotion_gpt.onnx"


class WrappedModel(torch.nn.Module):
    """Wrap pour ne renvoyer que les logits (ONNX export friendly)."""
    def __init__(self, m: EmotionGPT):
        super().__init__()
        self.m = m

    def forward(self, idx: torch.Tensor, emotion: torch.Tensor) -> torch.Tensor:
        logits, _ = self.m(idx, emotion)
        return logits


def main() -> int:
    ckpt = torch.load(CKPT, map_location="cpu", weights_only=False)
    cfg = GPTConfig(**ckpt["config"])
    model = EmotionGPT(cfg)
    model.load_state_dict(ckpt["model"])
    model.eval()
    wrapped = WrappedModel(model)

    dummy_idx = torch.zeros(1, 64, dtype=torch.long)
    dummy_emo = torch.zeros(1, dtype=torch.long)

    OUT_ONNX.parent.mkdir(exist_ok=True)
    torch.onnx.export(
        wrapped,
        (dummy_idx, dummy_emo),
        str(OUT_ONNX),
        input_names=["input_ids", "emotion"],
        output_names=["logits"],
        dynamic_axes={
            "input_ids": {0: "batch", 1: "seq"},
            "emotion":   {0: "batch"},
            "logits":    {0: "batch", 1: "seq"},
        },
        opset_version=17,
    )
    print(f"[done] -> {OUT_ONNX}")
    print("        Charge ce fichier dans ton VST3 via ONNX Runtime C++.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
