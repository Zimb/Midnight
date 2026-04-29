# Entraînement modèle d'impro sur VGMIDI

Pipeline complet pour entraîner un petit Transformer conditionné émotion
sur le dataset VGMIDI, exportable en ONNX pour intégration dans un VST3 C++.

## Setup (Windows + GPU NVIDIA)

```powershell
cd ml_training
python -m venv .venv
.venv\Scripts\Activate.ps1

# PyTorch CUDA 12.1
pip install torch --index-url https://download.pytorch.org/whl/cu121

# Le reste
pip install -r requirements.txt

# Vérifier GPU
python -c "import torch; print(torch.cuda.is_available(), torch.cuda.get_device_name(0))"
```

## Pipeline

```powershell
# 1. Télécharger VGMIDI (~200 MIDI annotés émotion)
python download_vgmidi.py

# 2. Tokenizer + construire le dataset (cache .pt)
python prepare_data.py

# 3. Entraîner le Transformer conditionné émotion
python train.py

# 4. Générer un MIDI à partir d'une seed + d'une émotion choisie
python generate.py --emotion happy --seed_midi ../ghibli_proper.mid --out generated.mid

# 5. Exporter en ONNX pour le VST C++
python export_onnx.py
```

## Conditioning émotion

VGMIDI annote chaque morceau sur 2 axes (valence, arousal) → 4 quadrants :

| Quadrant | Valence | Arousal | Mood              |
|----------|---------|---------|-------------------|
| Q1       | +       | +       | happy / heroic    |
| Q2       | -       | +       | tense / epic dark |
| Q3       | -       | -       | sad / melancholic |
| Q4       | +       | -       | peaceful / calm   |

Au moment du sampling, tu choisis l'émotion via un **emotion token** prepended.

## Fichiers

- `download_vgmidi.py` — clone le repo VGMIDI + extrait labels
- `prepare_data.py` — tokenisation REMI + cache binaire
- `dataset.py` — `torch.utils.data.Dataset` avec conditioning
- `model.py` — Transformer décodeur (~2M params)
- `train.py` — boucle d'entraînement + checkpoints
- `generate.py` — génération avec top-k + température
- `export_onnx.py` — export ONNX pour ONNX Runtime C++
