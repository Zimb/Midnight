# Generateur d'orchestre MIDI/WAV infini en temps reel

## Objectif

Imaginer puis construire un systeme ou l'utilisateur dirige un orchestre generatif en temps reel, comme un chef d'orchestre. L'utilisateur ne compose pas chaque note a la main : il controle l'intention musicale, l'energie, les pupitres, les transitions et les moments forts.

Le moteur genere en continu des evenements MIDI, les envoie a un synthetiseur comme FluidSynth, puis produit du son en direct. Le WAV n'est pas la source principale : c'est l'enregistrement optionnel de la performance.

Pipeline cible :

```text
Chef d'orchestre UI -> Moteur musical -> Scheduler MIDI live -> FluidSynth -> Audio live
                                                                          -> WAV optionnel
                                                                          -> MIDI log optionnel
```

## Etat actuel du projet

Le dossier contient deja une base utile :

- `midi.py` : genere un morceau orchestral lofi/anime/game de 3 minutes.
- `heroic_adventure_theme.py` : genere un theme original aventure/fantasy avec orchestration.
- `midi_editor.py` : editeur MIDI graphique type piano-roll avec lecture et export.
- `render_with_soundfont.py` : convertit un MIDI en WAV via FluidSynth et une SoundFont.
- `SOUND_QUALITY.md` : notes sur FluidSynth, SoundFonts et rendu audio.
- `heroic_adventure_theme_original.mid` : dernier MIDI genere.
- `heroic_adventure_theme_original_generaluser.wav` : rendu WAV avec GeneralUser GS.

SoundFont deja utilisee sur le poste actuel :

```text
soundfonts/GeneralUser_GS_1.472/GeneralUser GS 1.472/GeneralUser GS v1.472.sf2
```

FluidSynth detecte sur le poste actuel :

```text
C:\ProgramData\chocolatey\bin\fluidsynth.EXE
```

Sur un autre poste, ces chemins peuvent changer. Il faudra soit copier le dossier `soundfonts`, soit telecharger une SoundFont compatible GM/GS.

## Idee musicale

Le systeme doit se comporter comme un orchestre vivant :

- Cordes : nappes, tremolos, ostinatos, soutien harmonique.
- Bois : flute, clarinette, hautbois pour melodies et reponses.
- Cuivres : cor, trombone, trompette pour appels heroiques et tutti.
- Harpe : arpeges, transitions, texture magique.
- Basse : fondation harmonique.
- Percussions : energie, accents, transitions.
- Choeur : renfort pour les moments larges.

Le theme ne doit pas rester toujours sur le meme instrument. Il doit passer d'un pupitre a l'autre : cor -> flute -> clarinette -> cordes -> hautbois -> trombone -> tutti.

## Controle utilisateur

L'utilisateur agit comme chef d'orchestre avec des controles simples :

- Play / Stop
- Tempo
- Energie
- Tension
- Densite
- Volume par pupitre
- Pupitres actifs : cordes, bois, cuivres, harpe, basse, percussions, choeur
- Sections : intro, theme, variation, pont, final, calme
- Actions : tutti, solo, transition, crescendo, decrescendo, break

Exemples d'intentions :

```text
Plus doux -> moins de percussions, plus de harpe et clarinette.
Plus heroique -> cors + trombones + cordes larges.
Solo flute -> la melodie passe a la flute pendant quelques mesures.
Final -> le moteur prepare une montee sur 8 mesures.
Calme -> basse simple, cordes tenues, bois leger.
```

## Architecture proposee

### 1. Moteur musical

Responsabilites :

- Choisir la progression d'accords.
- Generer basse, harmonie, arpges, melodies, contrechants et percussions.
- Garder une memoire musicale pour eviter une boucle trop repetee.
- Produire des blocs courts : 1, 2 ou 4 mesures.

### 2. Etat de direction

Objet central qui contient les choix du chef :

```python
ConductorState(
    tempo=108,
    energy=0.65,
    tension=0.35,
    density=0.55,
    section="theme_a",
    focus="woodwinds",
    active_parts={"strings", "harp", "bass", "flute", "horn"},
)
```

### 3. Scheduler MIDI temps reel

Responsabilites :

- Planifier les notes avec un peu d'avance.
- Garder un timing stable.
- Envoyer `note_on`, `note_off`, program changes et controles MIDI.
- Eviter les coupures quand l'utilisateur change de section.

Principe recommande : generer 1 ou 2 mesures d'avance, mais appliquer les changements utilisateur au prochain point musical propre.

### 4. Synthese audio

Options possibles :

- `pyfluidsynth` pour piloter FluidSynth directement depuis Python.
- `mido` + port MIDI virtuel + FluidSynth en process externe.
- FluidSynth en mode serveur si besoin.

Pour un MVP, le plus simple est : Python genere les evenements -> FluidSynth joue avec la SoundFont.

### 5. Enregistrement

Deux sorties utiles :

- MIDI log : enregistrer tout ce qui a ete joue en `.mid`.
- WAV live : enregistrer l'audio produit.

Pour une session infinie, eviter un WAV unique sans limite. Preferer des segments :

```text
session_001.wav
session_002.wav
session_003.wav
```

## Dependances Python envisagees

Dependances actuelles du projet :

```text
midiutil
mido
midi2audio
pyFluidSynth
```

Dependances utiles pour le temps reel :

```text
mido
python-rtmidi
pyfluidsynth
sounddevice
numpy
```

Installer :

```powershell
pip install mido python-rtmidi pyfluidsynth sounddevice numpy
```

FluidSynth doit aussi etre installe sur la machine :

```powershell
choco install fluidsynth
```

Ou installer FluidSynth autrement, puis verifier :

```powershell
fluidsynth --version
```

## MVP recommande

Creer un nouveau fichier :

```text
live_orchestra_conductor.py
```

Fonctionnalites du MVP :

1. Interface Tkinter simple.
2. Bouton Play / Stop.
3. Sliders : tempo, energie, tension, densite.
4. Boutons : intro, theme, variation, calme, final.
5. Generation infinie par blocs de 4 mesures.
6. Rotation automatique des instruments melodiques.
7. Lecture live via FluidSynth.
8. Sauvegarde optionnelle de ce qui a ete joue en MIDI.
9. Rendu/export WAV de la performance ou des segments.

## Structure possible du code

```text
live_orchestra_conductor.py
├── ConductorState
├── LiveOrchestraEngine
├── HarmonyGenerator
├── MelodyGenerator
├── Orchestrator
├── MidiScheduler
├── FluidSynthPlayer
├── SessionRecorder
└── ConductorApp
```

## Pseudo-flux temps reel

```python
while playing:
    if scheduler.needs_more_music():
        state = ui.current_state()
        bars = engine.generate_next_bars(state, count=4)
        scheduler.queue(bars)

    scheduler.send_due_events()
    ui.update_meters()
```

## Points techniques a surveiller

- Latence : garder un buffer musical mais pas trop grand.
- Timing : utiliser une horloge monotone, pas `time.time()`.
- Transitions : appliquer les gros changements au debut d'une mesure.
- Notes bloquees : toujours envoyer `all notes off` au stop.
- Ranges instrumentales : eviter flute trop grave, trombone trop aigu, etc.
- Densite : plus de notes ne veut pas toujours dire meilleure musique.
- Export WAV : segmenter les longues sessions.

## Etapes suivantes concretes

1. Verifier que FluidSynth fonctionne sur le nouveau poste.
2. Copier ou reinstaller la SoundFont GeneralUser GS.
3. Installer les dependances Python temps reel.
4. Creer `live_orchestra_conductor.py` avec une premiere boucle infinie simple.
5. Commencer avec 3 pupitres : harp, strings, horn.
6. Ajouter ensuite flute, clarinet, oboe, trombone, percussion.
7. Ajouter l'enregistrement MIDI de la session.
8. Ajouter le rendu ou l'enregistrement WAV.

## Commandes utiles existantes

Regenerer le theme aventure actuel :

```powershell
python heroic_adventure_theme.py
```

Rendre en WAV avec GeneralUser GS :

```powershell
python render_with_soundfont.py --midi heroic_adventure_theme_original.mid --soundfont "soundfonts\GeneralUser_GS_1.472\GeneralUser GS 1.472\GeneralUser GS v1.472.sf2" --output heroic_adventure_theme_original_generaluser.wav
```

Verifier la compilation Python :

```powershell
python -m py_compile heroic_adventure_theme.py midi_editor.py render_with_soundfont.py
```

## Conclusion

Le projet est techniquement faisable. La bonne approche n'est pas de creer un MIDI infini puis un WAV infini, mais de faire jouer un orchestre MIDI vivant en direct. Le WAV devient l'enregistrement de la performance.

La prochaine grande etape est un prototype `live_orchestra_conductor.py` : un chef d'orchestre interactif qui genere quelques mesures d'avance, les joue via FluidSynth, et reagit aux controles utilisateur au debut des prochaines mesures.