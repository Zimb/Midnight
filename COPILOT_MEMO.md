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
│   │   └── plugin_vst3.cpp             # VST3 implementation (~7760 lignes)
│   └── build/                          # dossier de build CMake (ne pas éditer manuellement)
├── ml_training/                        # scripts Python d'entraînement ML
├── soundfonts/                         # SF2 assets
├── diagnose_cpp_syntax.py              # outil de diagnostic C++ maison
└── COPILOT_MEMO.md                     # CE FICHIER
```

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

## 4. Structure de plugin_vst3.cpp (état post-corrections)

```
Lignes 1-156     : includes, defines, forward declarations
Ligne 157        : class MelodyMakerVST3 : public SingleComponentEffect {
Lignes 158-291   : méthodes simples inline dans la classe (initialize, terminate, etc.)
Ligne 292        : tresult PLUGIN_API process(ProcessData& data) override;  ← DÉCLARATION SEULE
Lignes 293-~2200 : reste du corps de la classe (membres, méthodes)
Ligne ~2200      : };   ← fin de MelodyMakerVST3
Lignes ~2201-7007: définitions out-of-class (setState, getState, createView prep, etc.)
Ligne 7008       : tresult PLUGIN_API MelodyMakerVST3::process(ProcessData& data) { ... }
Lignes ~7009-7756: suite du corps de process()
Ligne 7757+      : IPlugView* PLUGIN_API MelodyMakerVST3::createView(...), IMPLEMENT_FUNKNOWN_METHODS, etc.
```

---

## 5. Patterns à éviter / règles de code

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

## 7. Points en suspens / À faire

- [ ] **SF2 Editor dans l'onglet Maker** — vérifier que l'UI fonctionne (question originale de l'utilisateur)
- [ ] **Warnings C4100 / C4189** — paramètres et variables non référencés (non bloquants)
- [ ] **Warning C4310** — cast tronque une constante (lignes 3972, 3976)
- [ ] **Tester dans FL Studio** — charger le VST3 depuis `midnight-plugins/build/VST3/MidnightMelodyMaker(beta).vst3`
- [ ] **Plugin CLAP** — n'a pas été testé dans cette session

---

## 8. Workflow de débogage type

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

## 9. Le problème VS Code ↔ disque

**Phénomène** : Quand un fichier est modifié via PowerShell (hors VS Code), VS Code peut :
- Afficher l'ancienne version dans l'éditeur
- Servir l'ancienne version via `read_file` (outil Copilot)
- Parfois **écraser** la version sur disque avec sa version en cache si un auto-save se déclenche

**Règle** : Toujours utiliser PowerShell `Get-Content` pour vérifier l'état réel du fichier sur disque. `read_file` peut retourner le cache VS Code.

**Indicateur** : Si `Get-Content` montre X lignes mais que `read_file` montre Y lignes → conflit de cache.

