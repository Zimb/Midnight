"""
Génère un dataset MIDI labélisé par émotion en utilisant MIDI-LLM
(slseanwu/MIDI-LLM_Llama-3.2-1B) comme oracle.

Le modèle MIDI-LLM reçoit des prompts textuels décriant une émotion →
les fichiers générés héritent du label émotion de leur prompt.

Résultat :
    data/midillm_raw/Q1/  ... data/midillm_raw/Q4/   (fichiers .mid)
    data/midillm_index.csv  (même schéma que vgmidi_index.csv)

Pré-requis :
    pip install transformers anticipation

⚠ GPU FORTEMENT RECOMMANDÉ (RTX 3060 6 GB suffit).
   Sur CPU c'est possible mais très lent (~5 min/fichier).

Usage :
    python generate_dataset.py                      # 4 sorties × 10 prompts/quadrant
    python generate_dataset.py --n_per_prompt 2     # rapide, pour tester
    python generate_dataset.py --quadrants Q1 Q3    # seulement happy + sad
    python generate_dataset.py --cpu                # forcer CPU (debug)
"""
from __future__ import annotations

import argparse
import csv
import sys
from pathlib import Path

import torch
from transformers import AutoModelForCausalLM, AutoTokenizer

# ---------------------------------------------------------------------------
# Constantes MIDI-LLM (extraites de midi_llm/utils.py)
# ---------------------------------------------------------------------------
AMT_GPT2_BOS_ID  = 55026
LLAMA_VOCAB_SIZE = 128256
MODEL_ID         = "slseanwu/MIDI-LLM_Llama-3.2-1B"

# Paramètres de génération recommandés par les auteurs
DEFAULT_TEMPERATURE = 1.0
DEFAULT_TOP_P       = 0.98
DEFAULT_MAX_TOKENS  = 2046

SYSTEM_PROMPT = (
    "You are a world-class composer. "
    "Please compose some music according to the following description: "
)

# ---------------------------------------------------------------------------
# Prompts par quadrant émotion
# ---------------------------------------------------------------------------
EMOTION_PROMPTS: dict[str, list[str]] = {
    "Q1": [  # Valence+ Arousal+  →  joyeux / énergique
        "A joyful, upbeat piano melody with bright rhythms and a cheerful feel",
        "An energetic, happy piece with a driving rhythm and lively melody",
        "A cheerful and playful tune with bouncy rhythms, perfect for a sunny day",
        "An optimistic, exuberant piano composition full of energy and joy",
        "A festive, celebratory melody with bright harmonies and an exciting feel",
        "A lively folk-inspired piano piece that feels carefree and joyous",
        "An upbeat adventure theme full of excitement and a sense of triumph",
        "A happy, dancing melody reminiscent of a carnival or festive celebration",
        "A spirited and energetic piano composition with fast-paced, cheerful motifs",
        "A bright and enthusiastic piano piece that radiates happiness and energy",
    ],
    "Q2": [  # Valence- Arousal+  →  tendu / épique / dramatique
        "A tense, dramatic orchestral piece building to an intense climax",
        "An anxious, suspenseful melody with dissonant harmonies and a fast tempo",
        "A dark and powerful theme evoking danger and conflict",
        "An intense battle theme with driving rhythms and fierce energy",
        "A menacing, foreboding melody that creates a sense of dread and urgency",
        "An epic, dramatic piano piece with powerful chords and tense atmosphere",
        "A thriller-like composition with staccato rhythms and nervous energy",
        "A dark, stormy piano piece with turbulent runs and aggressive dynamics",
        "An ominous and tense melody that builds relentlessly toward a crisis point",
        "A high-stakes, urgent composition full of tension and dramatic intensity",
    ],
    "Q3": [  # Valence- Arousal-  →  triste / mélancolique
        "A melancholic, slow piano piece expressing deep sadness and longing",
        "A sorrowful, quiet melody with minor harmonies and a slow, heavy pace",
        "A heartbroken piano ballad full of grief and quiet despair",
        "A somber, introspective composition conveying loss and melancholy",
        "A tearful, gentle piano piece that evokes loneliness and quiet sorrow",
        "A slow, aching melody in a minor key that speaks of regret and sadness",
        "A mournful, subdued piano composition that conveys deep emotional pain",
        "A gentle elegy for piano, slow and filled with grief and remembrance",
        "A quiet, despondent piece with sparse notes and a heavy emotional weight",
        "A sad, lyrical piano melody that slowly unfolds, conveying deep sorrow",
    ],
    "Q4": [  # Valence+ Arousal-  →  paisible / calme
        "A calm, peaceful ambient piano piece with soft, flowing melodies",
        "A serene and tranquil composition that brings a sense of inner peace",
        "A gentle, soothing piano lullaby with slow, tender phrases",
        "A quiet, meditative melody that evokes stillness and contentment",
        "A soft, dreamy piano piece that feels like a warm, peaceful afternoon",
        "A gentle, flowing piece reminiscent of a calm lake at sunrise",
        "A relaxing, atmospheric piano composition with gentle arpeggios",
        "A peaceful pastoral melody evoking open fields and quiet nature",
        "A tender, slow piano piece that conveys warmth, safety, and serenity",
        "A subtle, hushed composition full of gentle beauty and quiet joy",
    ],
}

# Mapping quadrant → (valence, arousal)
QUADRANT_META = {
    "Q1": ( 1.0,  1.0),
    "Q2": (-1.0,  1.0),
    "Q3": (-1.0, -1.0),
    "Q4": ( 1.0, -1.0),
}

ROOT      = Path(__file__).parent
RAW_DIR   = ROOT / "data" / "midillm_raw"
INDEX_CSV = ROOT / "data" / "midillm_index.csv"


# ---------------------------------------------------------------------------
# Conversion AMT tokens → MIDI  (réplique de midi_llm/utils.py)
# ---------------------------------------------------------------------------

def _tokens_to_midi(tokens: list[int]):
    """Convertit une liste de tokens AMT en objet MIDI (anticipation)."""
    try:
        from anticipation.convert import events_to_midi
    except ImportError:
        print("[error] La librairie 'anticipation' est requise.")
        print("        Installe-la avec : pip install anticipation")
        sys.exit(1)
    return events_to_midi(tokens)


def _has_excessive_notes(tokens: list[int], max_per_time: int = 64) -> bool:
    """Détecte les générations aberrantes (trop de notes simultanées)."""
    t = torch.tensor(tokens)
    times = t[::3]
    if times.numel() == 0:
        return True
    counts = torch.bincount(times.clamp(min=0))
    return bool(torch.any(counts > max_per_time).item())


def save_midi(tokens: list[int], out_path: Path) -> bool:
    """Valide les tokens et sauvegarde en .mid. Retourne True si succès."""
    if len(tokens) < 3:
        return False
    if _has_excessive_notes(tokens):
        return False
    try:
        midi_obj = _tokens_to_midi(tokens)
        out_path.parent.mkdir(parents=True, exist_ok=True)
        midi_obj.save(str(out_path))
        return True
    except Exception as e:
        print(f"    [warn] Échec sauvegarde : {e}")
        return False


# ---------------------------------------------------------------------------
# Chargement du modèle
# ---------------------------------------------------------------------------

def load_model(device: str):
    print(f"[model] Chargement de {MODEL_ID} en BF16 sur {device}…")
    dtype = torch.bfloat16 if device == "cuda" else torch.float32
    tokenizer = AutoTokenizer.from_pretrained(MODEL_ID, pad_token="<|eot_id|>")
    model = AutoModelForCausalLM.from_pretrained(
        MODEL_ID, dtype=dtype, trust_remote_code=True
    ).to(device)
    model.eval()
    print(f"[model] Chargé. Paramètres : {sum(p.numel() for p in model.parameters())/1e6:.0f}M")
    return model, tokenizer


# ---------------------------------------------------------------------------
# Génération
# ---------------------------------------------------------------------------

def generate_for_prompt(
    model,
    tokenizer,
    prompt: str,
    n_outputs: int,
    device: str,
    temperature: float = DEFAULT_TEMPERATURE,
    top_p: float = DEFAULT_TOP_P,
    max_new_tokens: int = DEFAULT_MAX_TOKENS,
) -> list[list[int]]:
    """Retourne une liste de séquences de tokens AMT (déjà décalés)."""
    full_prompt = SYSTEM_PROMPT + prompt + " "
    inputs = tokenizer(full_prompt, return_tensors="pt", padding=False)
    input_ids = inputs["input_ids"]

    # Ajoute le token BOS MIDI
    midi_bos = torch.tensor([[AMT_GPT2_BOS_ID + LLAMA_VOCAB_SIZE]])
    input_ids = torch.cat([input_ids, midi_bos], dim=1).to(device)

    with torch.no_grad():
        outputs = model.generate(
            input_ids=input_ids,
            do_sample=True,
            max_new_tokens=max_new_tokens,
            temperature=temperature,
            top_p=top_p,
            num_return_sequences=n_outputs,
            pad_token_id=tokenizer.pad_token_id,
        )

    prompt_len = input_ids.shape[1]
    results = []
    for seq in outputs:
        tokens = (seq[prompt_len:] - LLAMA_VOCAB_SIZE).cpu().tolist()
        # Garde uniquement les tokens valides (0 à AMT_GPT2_BOS_ID-1)
        clean = [t for t in tokens if 0 <= t < AMT_GPT2_BOS_ID]
        results.append(clean)
    return results


# ---------------------------------------------------------------------------
# Boucle principale
# ---------------------------------------------------------------------------

def main() -> int:
    parser = argparse.ArgumentParser(description="Génère un dataset MIDI avec MIDI-LLM")
    parser.add_argument("--n_per_prompt", type=int, default=4,
                        help="Fichiers générés par prompt (défaut: 4)")
    parser.add_argument("--quadrants", nargs="+", default=list(EMOTION_PROMPTS.keys()),
                        choices=list(EMOTION_PROMPTS.keys()),
                        help="Quadrants à générer (défaut: Q1 Q2 Q3 Q4)")
    parser.add_argument("--temperature", type=float, default=DEFAULT_TEMPERATURE)
    parser.add_argument("--top_p",       type=float, default=DEFAULT_TOP_P)
    parser.add_argument("--max_tokens",  type=int,   default=DEFAULT_MAX_TOKENS)
    parser.add_argument("--cpu",         action="store_true",
                        help="Forcer CPU (déconseillé sauf debug)")
    args = parser.parse_args()

    device = "cpu" if args.cpu else ("cuda" if torch.cuda.is_available() else "cpu")
    if device == "cpu":
        print("[warn] GPU non disponible — la génération sera très lente sur CPU.")
        print("       Lance ce script sur la machine avec RTX 3060.")

    model, tokenizer = load_model(device)

    # Compteurs globaux
    total_ok = 0
    total_fail = 0
    index_rows: list[dict] = []

    for quadrant in args.quadrants:
        prompts = EMOTION_PROMPTS[quadrant]
        valence, arousal = QUADRANT_META[quadrant]
        q_dir = RAW_DIR / quadrant
        q_dir.mkdir(parents=True, exist_ok=True)

        # Compte les fichiers déjà générés pour numéroter correctement
        existing = list(q_dir.glob("*.mid"))
        file_idx = len(existing)

        print(f"\n[{quadrant}] {len(prompts)} prompts × {args.n_per_prompt} sorties")
        for p_idx, prompt in enumerate(prompts):
            print(f"  [{p_idx+1}/{len(prompts)}] {prompt[:70]}…")
            seqs = generate_for_prompt(
                model, tokenizer, prompt,
                n_outputs=args.n_per_prompt,
                device=device,
                temperature=args.temperature,
                top_p=args.top_p,
                max_new_tokens=args.max_tokens,
            )
            for seq in seqs:
                out_path = q_dir / f"gen_{file_idx:04d}.mid"
                if save_midi(seq, out_path):
                    print(f"    ✓ {out_path.name}")
                    index_rows.append({
                        "path": str(out_path),
                        "valence": valence,
                        "arousal": arousal,
                        "quadrant": quadrant,
                    })
                    total_ok += 1
                    file_idx += 1
                else:
                    print(f"    ✗ séquence invalide ignorée")
                    total_fail += 1

    # Écriture de l'index
    INDEX_CSV.parent.mkdir(parents=True, exist_ok=True)
    with open(INDEX_CSV, "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=["path", "valence", "arousal", "quadrant"])
        w.writeheader()
        w.writerows(index_rows)

    print(f"\n[done] {total_ok} fichiers générés, {total_fail} invalides ignorés")
    print(f"[index] {INDEX_CSV}")
    print("\nLance ensuite : python prepare_data.py")
    return 0


if __name__ == "__main__":
    sys.exit(main())
