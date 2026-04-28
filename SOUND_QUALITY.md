# Ameliorer la qualite sonore MIDI

Le rendu MIDI de Windows utilise une banque General MIDI basique. Pour obtenir des instruments plus propres, il faut rendre le MIDI avec une banque de sons SoundFont (`.sf2` ou `.sf3`) via FluidSynth.

## Solution recommandee

1. Installer FluidSynth pour Windows.
2. Telecharger une SoundFont General MIDI.
3. Rendre le MIDI en WAV avec `render_with_soundfont.py`.

## Banques de sons utiles

- `GeneralUser GS` : bonne banque General MIDI, legere, souvent plus musicale que le synth Windows.
- `FluidR3 GM` : classique, complete, bon choix polyvalent pour orchestre / piano / basses.
- `MuseScore General` (`.sf3`) : rendu propre et moderne si FluidSynth la charge correctement.

Place le fichier `.sf2` ou `.sf3` dans un dossier simple, par exemple :

```text
D:\SoundFonts\GeneralUser-GS.sf2
```

## Commande de rendu

Depuis ce dossier :

```powershell
python render_with_soundfont.py --soundfont "D:\SoundFonts\GeneralUser-GS.sf2"
```

Avec un chemin FluidSynth explicite :

```powershell
python render_with_soundfont.py --soundfont "D:\SoundFonts\GeneralUser-GS.sf2" --fluidsynth "C:\Program Files\FluidSynth\bin\fluidsynth.exe"
```

Le fichier cree par defaut sera :

```text
lofi_orchestral_anime_game_3min.wav
```

## Reglages utiles

- `--gain 0.5` si le son sature.
- `--gain 1.0` si le rendu est trop faible.
- `--sample-rate 48000` pour un WAV en 48 kHz.

Exemple :

```powershell
python render_with_soundfont.py --soundfont "D:\SoundFonts\FluidR3_GM.sf2" --gain 0.8 --sample-rate 48000
```