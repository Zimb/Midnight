# Plugin VST3 d'improvisation melodique avec ML

## Ce qui a ete construit

Un plugin VST3 developpe en C++ pour FL Studio. Il genere une phrase melodique
en temps reel a partir d'une note posee dans le piano roll, sur la gamme detectee,
avec la possibilite d'exporter le resultat en fichier MIDI.

### Workflow utilisateur

```text
1. L'utilisateur pose une note (ex: Do4) dans le piano roll FL Studio
2. Le plugin genere une phrase melodique sur la gamme correspondante
3. L'utilisateur clique "Export MIDI"
4. Il drag-and-drop le .mid dans le piano roll FL Studio
5. Il edite ce qu'il veut garder
```

### Ce qui fonctionne (moteur procédural actuel)

- Detection de la note de base depuis le piano roll
- Generation d'une phrase dans la gamme correspondante
- Export MIDI du resultat

---

## Prochaine etape : moteur ML conditionne par l'emotion

L'objectif est de remplacer le moteur procedural par un **Transformer ML**
qui genere des phrases plus naturelles, avec du phrasé, des respirations,
des motifs developpes — et surtout conditionne par une **emotion choisie**.

### Ce que le ML apporte vs le procédural actuel

| Critere              | Procédural (actuel) | ML (en cours)             |
|----------------------|---------------------|---------------------------|
| Notes dans la gamme  | Oui                 | Oui                       |
| Phrasé naturel       | Non                 | Oui                       |
| Motifs développés    | Non                 | Oui                       |
| Silences musicaux    | Non                 | Oui                       |
| Feeling "joué"       | Non                 | Oui                       |
| Controle émotionnel  | Non                 | Oui (knob 4 humeurs)      |

### Conditioning émotion (knob dans le VST)

| Bouton    | Quadrant | Valence | Arousal | Style                     |
|-----------|----------|---------|---------|---------------------------|
| Happy     | Q1       | +       | +       | Majeur vif, sauts joyeux  |
| Tense     | Q2       | -       | +       | Mineur intense, dissonances|
| Sad       | Q3       | -       | -       | Mineur lent, descendants  |
| Peaceful  | Q4       | +       | -       | Majeur calme, conjoint    |

---

## Pipeline ML (dossier ml_training/)

### Fichiers

```text
ml_training/
├── download_vgmidi.py   — télécharge VGMIDI (tarball, filtre NTFS Windows)
├── prepare_data.py      — tokenisation REMI via miditok -> tokens.pt
├── dataset.py           — Dataset PyTorch avec label émotion
├── model.py             — Transformer GPT ~2M params conditionné émotion
├── train.py             — boucle entraînement AMP fp16 + cosine LR + checkpoints
├── generate.py          — génération top-k/temperature -> .mid
├── export_onnx.py       — export ONNX pour ONNX Runtime C++ dans le VST
└── requirements.txt     — dépendances Python
```

### Dataset utilisé

**VGMIDI** — musiques de jeux vidéo annotées valence/arousal (~200 morceaux labellisés).
Téléchargé via tarball GitHub (pas git clone : le repo contient des noms de fichiers
invalides sur NTFS Windows dans le dossier unlabelled/).

Seul le dossier `labelled/` est extrait. Le dossier `unlabelled/` est ignoré
(noms avec guillemets `"` illégaux sur Windows, et pas d'annotations émotion utiles).

Dataset suivant prévu : **EMOPIA** (~1 100 clips pop piano, même schéma Q1-Q4).

### Architecture du modèle

Petit Transformer décodeur (GPT-like) :
- 4 layers, 4 heads, d_model=192, ~2M paramètres
- Token embedding + position embedding + emotion embedding (ajouté à chaque position)
- Flash attention causale (PyTorch >= 2.0)
- Weight tying embedding/tête de sortie
- Tokenisation REMI (miditok) : vocab ~500 tokens

### Commandes

```powershell
cd ml_training
.venv\Scripts\Activate.ps1

# 1. Télécharger VGMIDI (tarball filtré)
python download_vgmidi.py

# 2. Tokeniser le dataset
python prepare_data.py

# 3. Entraîner (~1h35 sur Ryzen 5 5650G, ~20-40 min sur RTX 3060)
python train.py

# 4. Tester la génération
python generate.py --emotion happy --seed_midi ..\ghibli_proper.mid --out test_happy.mid
python generate.py --emotion sad      --out test_sad.mid
python generate.py --emotion epic     --out test_epic.mid
python generate.py --emotion peaceful --out test_peaceful.mid

# 5. Exporter pour le VST C++
python export_onnx.py
```

### Résultats d'entraînement (repères)

- ~140 secondes par epoch sur Ryzen 5 PRO 5650G (CPU)
- 40 epochs -> ~1h35 sur CPU
- `best.pt` sauvegardé automatiquement à la meilleure val_loss
- val_loss cible : < 2.0 pour une qualité musicale correcte

---

## Intégration du modèle dans le VST C++

### Ce qui sera ajouté

```cpp
// Au clic Generate / Export — pas de contrainte temps réel
auto tokens  = onnxModel.generate(seedTokens, emotionId, numTokens);
auto midi    = tokenizer.decode(tokens);
exportMidiFile(midi, outputPath);
```

### Dépendances C++ à ajouter

- **ONNX Runtime C++** (lib statique) — charge `emotion_gpt.onnx`
- Tokenisation REMI en C++ (port du comportement de miditok)
- Sampling top-k + température (quelques dizaines de lignes)

### Fichier produit

```text
ml_training/checkpoints/emotion_gpt.onnx   (~8 MB)
```

Entrées :  `input_ids [batch, seq]` (int64) + `emotion [batch]` (int64)
Sortie :   `logits [batch, seq, vocab]` (float32)

---

## Etat des fichiers Python existants (contexte initial)

Ces fichiers etaient presents avant le VST3 et restent utiles pour la generation
rapide de MIDI de reference ou de test :

- `midi.py` : genere un morceau orchestral lofi/anime/game de 3 minutes
- `heroic_adventure_theme.py` : theme aventure/fantasy orchestral procedural
- `midi_editor.py` : editeur MIDI graphique type piano-roll
- `render_with_soundfont.py` : convertit un MIDI en WAV via FluidSynth

SoundFont disponible :

```text
soundfonts/GeneralUser_GS_1.472/GeneralUser GS 1.472/GeneralUser GS v1.472.sf2
```

---

## Prochaines etapes

1. Finir l'entraînement sur VGMIDI (en cours)
2. Ecouter les 4 MIDI generes (happy/tense/sad/peaceful), evaluer la qualite
3. Si qualite insuffisante -> ajouter EMOPIA (~8h d'entrainement sur CPU nuit)
4. Exporter `emotion_gpt.onnx`
5. Integrer ONNX Runtime dans le VST C++ (bouton Generate existant)
6. Brancher le knob émotion sur l'emotion_id passe au modele
7. Optionnel V2 : XY pad valence/arousal continus au lieu de 4 boutons discrets
8. Optionnel V3 : pré-entraîner sur MetaMIDI/Lakh -> fine-tune émotion