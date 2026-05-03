# COPILOT_MEMO — Midnight Melody Maker VST3/CLAP
*Mémo de contexte persistant pour GitHub Copilot. Lis ce fichier EN PREMIER à chaque session.*

---

## 1. Architecture du projet

```
Midnight-1/
├── midnight-plugins/
│   ├── CMakeLists.txt                  # top-level: fetche CLAP + VST3 SDK via FetchContent
│   ├── common/
│   │   ├── algo.h                      # logique musicale partagée (CLAP + VST3)
│   │   ├── fx.h                        # DSP header-only: Chorus, Delay, Reverb, CassetteNoise
│   │   ├── sf2_editor.h                # delta-patcher SF2 in-memory (compatible tsf_load_memory)
│   │   ├── sf2_maker.h                 # générateur SF2 procédural (header-only)
│   │   └── tsf.h                       # TinySoundFont (single-header, MIT)
│   ├── plugins/melody_maker/
│   │   ├── CMakeLists.txt              # targets: melody_maker_clap + midnight_melody_vst3
│   │   ├── plugin.cpp                  # CLAP implementation
│   │   ├── plugin_vst3.cpp             # VST3 DLL entry point (39 lignes) — TSF_IMPLEMENTATION ici
│   │   ├── plugin_vst3.h               # Classe MelodyMakerVST3 + toutes méthodes (~2935 lignes)
│   │   ├── view.cpp                    # MelodyMakerView + createView() (~4456 lignes)
│   │   ├── ui_constants.h              # IDs contrôles, MkStyleDef, palette couleurs (166 lignes)
│   │   ├── knob.h                      # KnobWidget Win32 custom (199 lignes, header-only)
│   │   └── logger.h                    # Logger debug thread-safe → %TEMP%\midnight_debug.log
│   └── build/                          # dossier de build CMake (ne pas éditer manuellement)
├── ml_training/                        # scripts Python d'entraînement ML
├── soundfonts/                         # SF2 assets
├── diagnose_cpp_syntax.py              # outil de diagnostic C++ maison
└── COPILOT_MEMO.md                     # CE FICHIER
```

### Découpe de plugin_vst3.cpp (opération réalisée)

L'ancien monofichier de 7754 lignes a été découpé en :

| Fichier | Contenu | Lignes |
|---------|---------|--------|
| `plugin_vst3.cpp` | DLL entry point : `TSF_IMPLEMENTATION`, `g_hInst`, `DllMain`, factory macros | 39 |
| `plugin_vst3.h` | Classe `MelodyMakerVST3` + toutes méthodes inline / out-of-class | ~2935 |
| `view.cpp` | `MelodyMakerView` (IPlugView) + `MelodyMakerVST3::createView()` | ~4456 |
| `ui_constants.h` | IDs, styles, couleurs | 166 |
| `knob.h` | `KnobWidget` (Win32 custom rotary) | 199 |
| `logger.h` | Logger debug zero-overhead | ~110 |

**Règle critique** : `#define TSF_IMPLEMENTATION` doit apparaître dans **exactement un seul .cpp** (plugin_vst3.cpp). Jamais dans un header.

**Règle critique** : `g_hInst` est défini dans `plugin_vst3.cpp` (non-static global), déclaré `extern HINSTANCE g_hInst;` dans `knob.h`.

---

## 2. Commandes essentielles

### Build VST3 (commande principale)
```powershell
$env:PATH = "C:\Program Files\CMake\bin;" + $env:PATH
cmake --build "C:\Users\dossa\OneDrive\Desktop\Midnight-1\midnight-plugins\build" --config Release --target midnight_melody_vst3 2>&1 | Where-Object { $_ -match "error C|Build FAILED|Build succeeded" }
```

### Régénérer le build system (après changement dans CMakeLists.txt)
```powershell
$env:PATH = "C:\Program Files\CMake\bin;" + $env:PATH
cmake -S "C:\Users\dossa\OneDrive\Desktop\Midnight-1\midnight-plugins" -B "C:\Users\dossa\OneDrive\Desktop\Midnight-1\midnight-plugins\build"
```

### Voir les 10 premières erreurs seulement
```powershell
cmake --build ... 2>&1 | Where-Object { $_ -match "error C" } | Select-Object -First 10
```

### Installer le VST3 (après build réussi)
Le bundle est assemblé automatiquement dans :
`midnight-plugins/build/VST3/MidnightMelodyMaker(beta).vst3/`

### Lire un fichier volumineux (VS Code cache le fichier sur disque — toujours lire via PowerShell)
```powershell
$lines = Get-Content "chemin\vers\fichier.cpp" -Encoding UTF8
$lines[291] # ligne 292 (0-indexed)
$lines[500..510] | ForEach-Object -Begin {$i=501} -Process {"$i: $_"; $i++}
```

---

## 3. Problèmes résolus et leurs corrections

### 3.1 NOMINMAX / macros Windows (RÉSOLU ✅)
**Symptôme** : `error C2589: '(' : jeton non conforme à droite de '::'` dans fx.h ou partout où `std::max/min/clamp` est utilisé.  
**Cause** : `windows.h` définit des macros `max(a,b)` et `min(a,b)` qui corrompent `std::max`, `std::min`, `std::clamp`.  
**Fix appliqué** :
- `CMakeLists.txt` (melody_maker) : ajout de `target_compile_definitions(midnight_melody_vst3 PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)`
- `sf2_editor.h` : garde `#ifndef NOMINMAX / WIN32_LEAN_AND_MEAN` avant `#include <windows.h>`
- `fx.h` : `(std::max)(8, ...)` — parenthèses autour du nom pour éviter l'expansion macro

**Règle** : Toujours définir NOMINMAX et WIN32_LEAN_AND_MEAN **avant** tout include Windows. Le faire via CMake `target_compile_definitions` est la méthode la plus robuste.

---

### 3.2 const uint8_t* dans sf2_editor.h (RÉSOLU ✅)
**Symptôme** : `error C2440: '=' : impossible de convertir de 'const uint8_t *' en 'uint8_t *'`  
**Cause** : `uint8_t* fileEnd = data + sf2.size()` était déclaré `const uint8_t*` à tort.  
**Fix** : Suppression du `const` sur `fileEnd` (ligne ~158).

---

### 3.3 process() inline dans la classe (RÉSOLU ✅)
**Symptôme** : Cascade de centaines d'erreurs (`undeclared identifier` pour des membres de classe).  
**Cause** : `tresult PLUGIN_API process(ProcessData& data) override { ... }` était défini **inline à la ligne 292 de la classe**, mais référençait des membres (`currentSection`, `GuiNote`, `Voice`, etc.) déclarés **des centaines de lignes plus loin** dans la classe. MSVC en mode inline ne "voit" pas les membres déclarés après.  
**Fix appliqué** :
1. Remplacer le corps inline par une déclaration seule : `tresult PLUGIN_API process(ProcessData& data) override;`
2. Définir la fonction **hors-classe** après `};` : `tresult PLUGIN_API MelodyMakerVST3::process(ProcessData& data) { ... }`
3. **Méthode** : script PowerShell de brace-matching pour extraire le corps (lignes 292-1041 dans l'ancien fichier), strip de 4 espaces d'indentation, insertion avant `createView`.

**ATTENTION** : VS Code peut écraser les modifications du fichier sur disque via son cache éditeur. Toujours vérifier avec PowerShell que les changements sont réellement sur disque avant de relancer le build.

---

### 3.4 Corps double de reloadLiveSf (RÉSOLU ✅)
**Symptôme** : `error C2059: erreur de syntaxe: 'if'`, `error C2628: 'MelodyMakerVST3' suivi de 'void'` à partir de la ligne ~660.  
**Cause** : Un double du corps de `reloadLiveSf()` **sans signature de fonction** flottait après la vraie fonction, à portée de classe. MSVC interprétait ces instructions comme du code à portée classe → erreurs syntaxiques.  
**Fix** : Supprimer les lignes du doublon flottant (25 lignes) via PowerShell array splice.

---

### 3.5 mkSliderW → mkSldW (RÉSOLU ✅)
**Symptôme** : `error C2065: 'mkSliderW' : identificateur non déclaré`  
**Cause** : La variable s'appelle `mkSldW` (définie à la ligne ~5549), mais 3 utilisations dans la section SF2 editor du Maker UI utilisaient le nom incorrect `mkSliderW`.  
**Fix** : Remplacement global `mkSliderW` → `mkSldW` (3 occurrences).

---

## 4. Structure de plugin_vst3.h (état post-découpe)

```
plugin_vst3.h — ~2935 lignes
  L1        : #pragma once
  L4        : #include "logger.h"    ← logger zero-overhead
  L5        : #include "../../common/algo.h"
  L10       : #include "../../common/tsf.h"   (sans TSF_IMPLEMENTATION)
  L60       : extern HINSTANCE g_hInst;
  L70       : using namespace Steinberg; ...
  L~90      : class MelodyMakerVST3 : public SingleComponentEffect {
  L~150     :   initialize, terminate, setupProcessing... (déclarations + petites inline)
  L292      :   tresult PLUGIN_API process(...) override;  ← déclaration seule
  L~400     :   processEvents(), NoteOn/NoteOff handlers
  L~660     : };  ← fin de classe
  L~680+    : tresult PLUGIN_API MelodyMakerVST3::process(...) {  ← corps out-of-class
  L~1030    : }  ← fin process
  L~1035+   : loadSoundFont, closeSoundFont, reloadLiveSf, getState, setState...
```

---

## 5. Logger de debug (logger.h)

## 5. Logger de debug (logger.h)

Fichier : `midnight-plugins/plugins/melody_maker/logger.h`
Sortie  : `%TEMP%\midnight_debug.log` (p.ex. `C:\Users\dossa\AppData\Local\Temp\midnight_debug.log`)

### Macros disponibles

| Macro | Quand utiliser |
|-------|----------------|
| `MM_LOG(fmt, ...)` | Log immédiat (NoteOn/Off, changement de section) |
| `MM_LOG_ONCE(fmt, ...)` | Erreur qui ne doit apparaître qu'une fois (sfReady faux) |
| `MM_LOG_CHANGE(label, expr)` | Détecte un changement d'état (rolling, hasTrigger) |
| `MM_LOG_EVERY(n, fmt, ...)` | Transport / density gate — 1 log toutes les n invocations |

### Activation / désactivation
Activé via `CMakeLists.txt` : `target_compile_definitions(... PRIVATE MM_DEBUG_LOG)`  
Pour build release final sans log : supprimer `MM_DEBUG_LOG` de CMakeLists.txt. Aucun code supplémentaire à toucher.

### Points de log déjà insérés dans plugin_vst3.h

| Endroit | Macro | Info loguée |
|---------|-------|-------------|
| Début de process() (1/500 blocks) | `MM_LOG_EVERY` | rolling, beatPos, bpm, triggerCount, sfReady |
| `!rolling` détecté | `MM_LOG_CHANGE` | passage rolling→stopped |
| `!hasTrigger()` détecté | `MM_LOG_CHANGE` | passage triggered→idle et retour |
| Changement de section | `MM_LOG` | pitch, holdBeats, nouveau sec, sectionVolumeRamp |
| density gate filtré (1/200) | `MM_LOG_EVERY` | style, slot, roll, effDensity |
| velocity < 0.02 après ramp | `MM_LOG_EVERY` | vel, ramp, section — **note ignorée** |
| `sfReady = true` (loadSoundFont) | `MM_LOG` | confirmation chargement |
| `sfReady = false` (closeSoundFont) | `MM_LOG` | confirmation fermeture |

### Comment lire le log en temps réel
```powershell
$log = "$env:TEMP\midnight_debug.log"
Get-Content $log -Wait -Tail 40
```

---

## 6. Patterns à éviter / règles de code

### Windows.h
```cpp
// TOUJOURS avant #include <windows.h> :
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
```
Ou mieux, via CMake : `target_compile_definitions(... PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)`

### std::max/min avec MSVC
```cpp
// Sûr même sans NOMINMAX :
int n = (std::max)(a, b);   // parenthèses empêchent l'expansion macro
// ou utiliser std::clamp
```

### std::clamp avec indexation de tableau (MSVC bug workaround)
```cpp
// Peut causer C2589 si l'argument contient un indexeur :
float v = std::clamp(vel * sfVolumePerStyle[style], 0.f, 1.f);
// Fix : wrapper en parenthèses
float v = (std::clamp)(vel * sfVolumePerStyle[style], 0.f, 1.f);
```

### Méthodes de classe avec gros corps
Si une méthode **inline** dans la classe référence des membres déclarés plus loin :
→ Extraire le corps **hors-classe** (définition `MaClasse::maMethode(...) { ... }` après `};`)
→ Laisser seulement la déclaration dans la classe

---

## 6. Fichiers et outils auxiliaires

| Fichier | Rôle |
|---------|------|
| `check_braces.py` | Rapport des lignes d'ouverture/fermeture des classes |
| `diagnose_cpp_syntax.py` | Vérification équilibre `{}()[]`, patterns std::clamp suspects, build optionnel |
| `_move_process_out.ps1` | Script PowerShell utilisé pour déplacer process() — peut être réutilisé |
| `diagnose_cpp_syntax.py --build` | Lance le build CMake et filtre les erreurs |

---

## 7. Fichiers et outils auxiliaires

| Fichier | Rôle |
|---------|------|
| `check_braces.py` | Rapport des lignes d'ouverture/fermeture des classes |
| `diagnose_cpp_syntax.py` | Vérification équilibre `{}()[]`, patterns std::clamp suspects, build optionnel |
| `_move_process_out.ps1` | Script PowerShell utilisé pour déplacer process() — peut être réutilisé |
| `diagnose_cpp_syntax.py --build` | Lance le build CMake et filtre les erreurs |

---

## 8. Fonctionnalités existantes (inventaire complet)

### 8.1 Architecture audio/MIDI
- **6 sorties stéréo VST3** indépendantes : Melodie / Arpege / Basse / Percu. / Contre / Piano
- Chaque style → son propre bus (`outBuses[st]`). FL Studio reçoit bien `numOutputs=6` avec buffers valides (confirmé par log).
- **1 entrée MIDI** (déclenchement par notes entrantes — `hasTrigger()`)
- **Sortie MIDI** : EXPORT SESSION → MIDI (format 1, multi-track, une track par style)
- **Export WAV** : bouton "EXPORT AUDIO → WAV" dans l'UI

### 8.2 6 Styles musicaux
Chaque style = onglet (tab) avec ses propres paramètres indépendants.

| Idx | Nom | Type de synthèse Maker |
|-----|-----|------------------------|
| 0 | Mélodie | Additive FM pad/voice |
| 1 | Arpège | Karplus-Strong harp |
| 2 | Basse | FM bass |
| 3 | Percu. | FM percussion |
| 4 | Contrechant | FM organ |
| 5 | Piano | Échantillon direct |

### 8.3 Paramètres musicaux (par style, via `kParamXxx`)
- **Tonalité** (Key) : 12 notes chromatiques
- **Mode** : 27 gammes/modes disponibles (`kModeNames[]`)
- **Octave** : registre
- **Subdivision** : durée de base des notes
- **Densité** : probabilité de jouer une note
- **Seed** : graine aléatoire (bouton 🎲 Dice pour randomiser)
- **Durée de note** (NoteLen)

### 8.4 Paramètres par style (persistés dans getState/setState)
| Paramètre | Champ | Défaut |
|-----------|-------|--------|
| Volume SF2 | `sfVolumePerStyle[6]` | 0.85 (0..1.5) |
| Humanize | `humanizePerStyle[6]` | 0.0 (timing jitter) |
| Retard | `retardPerStyle[6]` | 0.0 (groove lag) |
| Mute | `muteStyles` (bitmask) | 0 (aucun muté) |
| Solo | `soloStyles` (bitmask) | 0 (aucun solo) |
| Lock Mode | `lockModePerStyle[6]` | true |
| Lock Progression | `lockProgPerStyle[6]` | true |
| Lock Subdiv | `lockSubdivPerStyle[6]` | true |
| JAM mode | `jamPerStyle[6]` | false |
| Start Bar | `startBarPerStyle[6]` | 0 (délai démarrage) |
| Piano Mélodie | `pianoMelPerStyle[6]` | true |
| Piano Accords | `pianoChordPerStyle[6]` | true |

### 8.5 Boutons M / S sur chaque onglet de style
- **M** (Mute) : bouton rouge sur chaque tab → `toggleMute(i)` → bit dans `muteStyles`
- **S** (Solo) : bouton jaune sur chaque tab → `toggleSolo(i)` → exclusif (désactive les autres solos)
- Logique : si `soloStyles != 0`, seuls les styles dont le bit est actif jouent
- Rendu : `tabMuteRects[i]` / `tabSoloRects[i]` dans `WM_PAINT` des tabs

### 8.6 Progression harmonique
- 27 progressions prédéfinies (`kProgressionNames[]`, `kProgressions[27][12]`)
- Verrou 🔒 sur Mode, Progression, Subdiv (les synchronise entre styles)
- **AUTO-KEY** : écoute le MIDI entrant pour détecter la tonalité automatiquement

### 8.7 Onglet MAKER (SoundFont)
**Mode SF2 procédural** (`kIdMakerEnable`) :
- Synthèse au choix : Additive, FM, Karplus-Strong, selon le style
- Paramètres ADSR, ModIndex/ModDecay, Gain, plage de notes, pas d'échantillonnage
- Bouton GÉNÉRER → recharge `sfSynth[style]` avec le nouveau SF2

**Mode SF2 externe** (`kIdMakerLoadSf2`) :
- Charge un `.sf2` externe via dialogue fichier
- Sélection du preset (bank/program) via combo
- 12 sliders de delta SF2 (ajustements en mémoire sans toucher le fichier) :
  - Accordage coarse (±24 st), Accordage fin (±99 ct)
  - Attaque / Déclin / Sustain / Relâchement (enveloppe volume)
  - Volume (atténuation ±500 cb)
  - Filtre Fc (±6000 ct), Filtre Q (±500 cb)
  - Panoramique (±500)
  - Réverb. send (±500), Chorus send (±500)
- Labels **valeurs absolues** (base + delta) dans `refreshDeltaLabels()`
- `sf2BaseGensPerStyle[6]` : snapshot des gens de base à la lecture du SF2

### 8.8 FX par style (`fxChainPerStyle[6]`)
Appliqué après le rendu TSF, avant écriture dans le bus de sortie.
| FX | IDs |
|----|-----|
| Chorus | kIdFxChorusOn/Rate/Depth/Mix (1050-1053) |
| Delay | kIdFxDelayOn/Time/Fb/Mix (1054-1057) |
| Reverb | kIdFxReverbOn/Size/Damp/Mix (1058-1061) |
| Cassette Noise | kIdFxNoiseOn/Level/Flutter/Tone (1062-1065) |

### 8.9 Section / arrangement
- **Sections** : Intro / Verse / Pre-chorus / Chorus / Bridge / Outro (sélecteur global `kIdSection`)
- **Mesure** : signature temporelle (numérateur combo `kIdMeter`)
- **Start Bar** par style : délai de démarrage (style démarre après N barres)

### 8.10 Piano roll
- Visualisation Synthesia (X=pitch, Y=temps, barre de piano en bas)
- Notes de toutes les voix actives
- Clic sur la barre de piano → joue une note de preview

### 8.11 Preset
- **SAVE PRESET** → fichier `.mmp` (sérialisation complète via `getState`)
- **LOAD PRESET** → restauration complète via `setState`
- **FILE** dropdown → accès aux options fichier

### 8.12 TinySoundFont (TSF)
- 6 instances `sfSynth[0..5]`, une par style (indépendantes)
- Canal MIDI : identique au style (ch=st)
- Volume par canal : `tsf_channel_set_volume(t, ch, sfVolumePerStyle[st])`
- Rendu interleaved float → split L/R dans `sfBuf`

---

## 10. Points en suspens / À faire

- [x] **Découpe plugin_vst3.cpp** → plugin_vst3.h + view.cpp + ui_constants.h + knob.h + logger.h ✅
- [x] **Build vérifié après découpe** — Build succeeded ✅
- [x] **Logger debug** — logger.h créé, MM_DEBUG_LOG activé dans CMakeLists, build OK ✅
- [x] **Fix velocity=0 NoteOn** — `if (velocity < 0.02f) continue;` + log ✅
- [ ] **Bug silence intermittent** — root cause probable: long note sur C/C# → kSecIntro → ramp=0 → silence 16 bars. Logger en place pour confirmer. Envisager de relever le seuil `holdBeats >= 1.5` → `4.0`
- [ ] **SF2 Editor dans l'onglet Maker** — vérifier que l'UI fonctionne
- [ ] **Warnings C4100 / C4189** — paramètres et variables non référencés (non bloquants)
- [ ] **Tester dans FL Studio** — charger le VST3 depuis `midnight-plugins/build/VST3/MidnightMelodyMaker(beta).vst3`
- [ ] **Plugin CLAP** — n'a pas été testé dans cette session

---

## 11. Workflow de débogage type

1. Lancer le build et filtrer les erreurs :
   ```powershell
   cmake --build ... 2>&1 | Where-Object { $_ -match "error C" } | Select-Object -First 10
   ```
2. Identifier la **première** erreur (les suivantes sont souvent des cascades)
3. Lire le fichier **via PowerShell** (pas read_file qui peut être le cache VS Code) :
   ```powershell
   $lines = Get-Content "fichier.cpp" -Encoding UTF8
   $lines[N-5..N+5] | ForEach-Object -Begin {$i=N-4} -Process {"$i: $_"; $i++}
   ```
4. Appliquer les fixes **via PowerShell** (pour éviter la collision VS Code cache/disque) :
   ```powershell
   $content = [System.IO.File]::ReadAllText($file, [System.Text.Encoding]::UTF8)
   $content = $content -replace "ancienNom", "nouveauNom"
   [System.IO.File]::WriteAllText($file, $content, [System.Text.UTF8Encoding]::new($false))
   ```
5. Vérifier la modification avec PowerShell avant de relancer le build
6. Si VS Code affiche une version différente de celle sur disque → normal, le disque fait foi

---

## 12. Le problème VS Code ↔ disque

**Phénomène** : Quand un fichier est modifié via PowerShell (hors VS Code), VS Code peut :
- Afficher l'ancienne version dans l'éditeur
- Servir l'ancienne version via `read_file` (outil Copilot)
- Parfois **écraser** la version sur disque avec sa version en cache si un auto-save se déclenche

**Règle** : Toujours utiliser PowerShell `Get-Content` pour vérifier l'état réel du fichier sur disque. `read_file` peut retourner le cache VS Code.

**Indicateur** : Si `Get-Content` montre X lignes mais que `read_file` montre Y lignes → conflit de cache.

