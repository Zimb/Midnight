# Audit algorithmique — MelodyMakerVST3

> Audit complet du code source (plugin_vst3.h, view.cpp, sf2_editor.h).
> Chaque bug est classifié : **CRITIQUE** (crash/UB), **IMPORTANT** (fonctionnalité cassée), **MINEUR** (comportement incorrect mais non bloquant).

---

## Priorité de correction recommandée

| # | Fichier | Bug | Priorité |
|---|---------|-----|----------|
| 1 | plugin_vst3.h | CcPreset vals[5] pour 6 styles → UB | CRITIQUE |
| 2 | plugin_vst3.h | rebuildExtraPresets dangling pointer | CRITIQUE |
| 3 | plugin_vst3.h | findUserSfFor comparaison de pointeurs bruts | CRITIQUE |
| 4 | plugin_vst3.h | État SF2 externe non sérialisé (getState/setState) | IMPORTANT |
| 5 | plugin_vst3.h | wavePerStyle clampé à [0,3] au lieu de [0,15] | IMPORTANT |
| 6 | sf2_editor.h | applyDeltaTargeted → fallback corrompt tout le SF2 | IMPORTANT |
| 7 | plugin_vst3.h | posWeight calculé mais jamais utilisé | IMPORTANT |
| 8 | plugin_vst3.h | introFadeDone jamais réinitialisé | MINEUR |
| 9 | plugin_vst3.h | loadSoundFont utilise kSfPresetCount au lieu de effectivePresetCount | MINEUR |
| 10 | plugin_vst3.h | Race condition note-on/note-off sur swap de synth | MINEUR |

---

## BUG-1 — CcPreset : out-of-bounds sur style Piano [CRITIQUE]

**Fichier** : `plugin_vst3.h`, ligne ~528  
**Symptôme** : Undefined Behaviour (lecture hors tableau) quand le style actif est Piano (curStyle == 5).

### Code bugué

```cpp
struct CcPreset { uint8 cc; int8 vals[5]; };  // ← 5 valeurs
static const CcPreset kPresets[] = {
    //          Mel  Harp Basse Perc  Mari          ← seulement 5 styles
    { 73, { 40,   5,  25,    0,   8 } }, // Attack
    { 72, { 60,  90, 110,   10,  30 } }, // Release
    { 74, { 64, 100,  30,  110,  90 } }, // Brightness
    { 71, { 20,  50,  80,   15,  35 } }, // Resonance
    { 91, { 30,  60,  15,    5,  20 } }, // Reverb
};
// ...
cc.midiCCOut.value = p.vals[curStyle];  // ← curStyle==5 → vals[5] = UB
```

### Comportement attendu
6 presets (un par style, Piano inclus).

### Fix

```cpp
struct CcPreset { uint8 cc; int8 vals[6]; };  // 6 valeurs
static const CcPreset kPresets[] = {
    //          Mel  Harp Basse Perc  Mari Piano
    { 73, { 40,   5,  25,    0,   8,  20 } }, // Attack
    { 72, { 60,  90, 110,   10,  30,  70 } }, // Release
    { 74, { 64, 100,  30,  110,  90,  85 } }, // Brightness
    { 71, { 20,  50,  80,   15,  35,  30 } }, // Resonance
    { 91, { 30,  60,  15,    5,  20,  40 } }, // Reverb
};
```

---

## BUG-2 — rebuildExtraPresets : dangling pointer [CRITIQUE]

**Fichier** : `plugin_vst3.h`, ligne ~2100  
**Symptôme** : Après un second appel à `createOrUpdateUserSf()` ou `scanAndLoadUserSf()`, un `push_back` sur `userSfs` peut provoquer une réallocation du vecteur, invalidant les pointeurs `const wchar_t* name` stockés dans `extraPresetsByStyle`. Accès au pointeur dangling → crash.

### Code bugué

```cpp
void rebuildExtraPresets() {
    for (int s = 0; s < kStyleCount; ++s) extraPresetsByStyle[s].clear();
    for (auto& u : userSfs) {
        SfPreset p;
        p.name = u.nameWide.c_str();  // ← pointe dans u.nameWide
        extraPresetsByStyle[s].push_back(p);
    }
}
// Puis dans createOrUpdateUserSf :
userSfs.push_back(std::move(e));  // peut réalloquer userSfs → anciens u.nameWide.c_str() invalides
rebuildExtraPresets();             // reconstruit, mais entre les deux appels successifs...
```

### Comportement attendu
Les `SfPreset::name` pointent toujours vers une chaîne valide.

### Fix (option A — le plus simple)
Réserver à l'avance pour éviter toute réallocation :

```cpp
// Dans initialize() ou loadSoundFont(), avant scanAndLoadUserSf :
userSfs.reserve(64);  // suffisant pour 64 user SFs
```

### Fix (option B — le plus robuste)
Stocker l'index dans `userSfs` plutôt qu'un pointeur brut :

```cpp
// Dans SfPreset, remplacer :
//   const wchar_t* name;
// par :
//   int userSfIdx = -1;  // index dans userSfs (-1 = preset statique)
// Et dans rebuildExtraPresets :
p.userSfIdx = (int)(&u - userSfs.data());
// Dans findUserSfFor : comparer userSfIdx directement.
```

---

## BUG-3 — findUserSfFor : comparaison de pointeurs bruts [CRITIQUE]

**Fichier** : `plugin_vst3.h`, ligne ~1835  
**Symptôme** : Après une réallocation de `userSfs`, `target.name` est dangling. La comparaison `u.nameWide.c_str() == target.name` compare des adresses mémoire — si le vecteur a bougé, le résultat est toujours `false`, même pour le bon élément. `applyPresetForStyle` reçoit `nullptr` pour tous les user SFs → le synth n'est pas swappé → silence ou mauvais instrument.

### Code bugué

```cpp
UserSfEntry* findUserSfFor(int style, int idx) {
    const SfPreset& target = extraPresetsByStyle[style][j];
    for (auto& u : userSfs) {
        if (u.targetStyle == style && u.nameWide.c_str() == target.name)  // ← comparaison d'adresses
            return &u;
    }
    return nullptr;
}
```

### Fix
Comparer les contenus de chaînes, pas les adresses :

```cpp
if (u.targetStyle == style && u.nameWide == target.name)
```

Ou, si on adopte le fix B de BUG-2 (indices) :

```cpp
// target.userSfIdx contient l'index dans userSfs
if (j < (int)userSfs.size())
    return &userSfs[j]; // direct, sans scan
```

---

## BUG-4 — État SF2 externe non sérialisé [IMPORTANT]

**Fichier** : `plugin_vst3.h`, lignes ~1060–1128 (getState), ~1129–1290 (setState)  
**Symptôme** : Les champs suivants ne sont **jamais** écrits dans `getState()` ni lus dans `setState()` :
- `externalSf2PerStyle[]` (fichier SF2 chargé en mémoire, potentiellement plusieurs Mo)
- `sf2DeltaPerStyle[]` (paramètres des 31 sliders du Maker)
- `sf2SelBankPerStyle[]` / `sf2SelProgPerStyle[]` (preset sélectionné dans le combo)

Après rechargement du plugin (ex : fermeture/réouverture du projet DAW), tout l'état du Maker SF2 est perdu. L'instrument revient à son état par défaut.

### Ce qui est sérialisé vs ce qui ne l'est pas

```
getState() écrit :
  ✅ paramValuesPerStyle  ✅ wavePerStyle     ✅ progPerStyle
  ✅ pianoMelodyPerStyle  ✅ percRhythmPerStyle ✅ sfPresetIdxPerStyle
  ✅ sfVolumePerStyle     ✅ lockModePerStyle  ✅ lockProgPerStyle
  ✅ lockSubdivPerStyle   ✅ pianoChordPerStyle ✅ beatsPerBarOverride
  ✅ humanizePerStyle     ✅ retardPerStyle    ✅ currentSection
  ✅ startBarPerStyle     ✅ pinnedNotes       ✅ muteStyles/soloStyles
  ✅ jamPerStyle          ✅ fxParamsPerStyle
  ❌ externalSf2PerStyle  ❌ sf2DeltaPerStyle
  ❌ sf2SelBankPerStyle   ❌ sf2SelProgPerStyle
```

### Fix
Ajouter en fin de `getState()` (nouveau chunk v3.3) :

```cpp
// v3.3 trailing fields (per-style external SF2 state)
for (int st = 0; st < kStyleCount; ++st) {
    // Chemin relatif ou absolu du SF2 externe (UTF-8, peut être vide)
    // Écrire seulement le chemin, pas les bytes (le SF2 existe sur disque)
    // Note: pour un SF2 externe chargé via drag&drop, stocker le chemin
    //       (on ne stocke pas les bytes pour éviter d'exploser la taille d'état)
    s.writeInt32(0); // placeholder: pas encore implémenté
    
    // Les 32 champs du delta SF2
    auto& d = sf2DeltaPerStyle[st];
    s.writeInt32(d.coarseTune);   s.writeInt32(d.fineTune);
    s.writeInt32(d.scaleTuning);
    s.writeInt32(d.delayVolDelta); s.writeInt32(d.attackDelta);
    s.writeInt32(d.holdDelta);    s.writeInt32(d.decayDelta);
    s.writeInt32(d.releaseDelta); s.writeInt32(d.sustainDelta);
    s.writeInt32(d.delayModDelta); s.writeInt32(d.modAttackDelta);
    s.writeInt32(d.modHoldDelta); s.writeInt32(d.modDecayDelta);
    s.writeInt32(d.modSustainDelta); s.writeInt32(d.modReleaseDelta);
    s.writeInt32(d.modEnvToPitch); s.writeInt32(d.modEnvToFilter);
    s.writeInt32(d.modLfoDelay);  s.writeInt32(d.modLfoFreq);
    s.writeInt32(d.modLfoToPitch); s.writeInt32(d.modLfoToFilter);
    s.writeInt32(d.modLfoToVolume);
    s.writeInt32(d.vibLfoDelay);  s.writeInt32(d.vibLfoFreq);
    s.writeInt32(d.vibLfoToPitch);
    s.writeInt32(d.attenuationDelta);
    s.writeInt32(d.filterFcDelta); s.writeInt32(d.filterQDelta);
    s.writeInt32(d.chorusDelta);  s.writeInt32(d.reverbDelta);
    s.writeInt32(d.panDelta);
    // Preset sélectionné
    s.writeInt32((int32)sf2SelBankPerStyle[st]);
    s.writeInt32((int32)sf2SelProgPerStyle[st]);
}
```

Et lire en `setState()` sous un nouveau numéro de version, avec guard de version pour la compatibilité ascendante.

---

## BUG-5 — wavePerStyle clampé à [0,3] au lieu de [0,15] [IMPORTANT]

**Fichier** : `plugin_vst3.h`, ligne ~1146  
**Symptôme** : Au rechargement du preset (`setState`), les indices d'instrument 4–15 (user SFs et presets supplémentaires) sont ramenés à 3. Le mauvais instrument est sélectionné silencieusement.

### Code bugué

```cpp
int32 w = 0;
s.readInt32(w);
wavePerStyle[sti] = std::clamp((int)w, 0, 3);  // ← max 3, devrait être 15 au moins
```

### Comportement attendu
`wavePerStyle` est un index dans la liste d'oscillateurs/waveforms. Avec les user SFs, la plage effective est `[0, effectivePresetCount(style) - 1]` qui peut dépasser 15.

### Fix

```cpp
wavePerStyle[sti] = std::clamp((int)w, 0, 127);  // ou 255, borner généreusement
```

Note : `effectivePresetCount` n'est pas connu au moment de `setState` (les user SFs ne sont pas encore scannées), d'où la borne généreuse. La cohérence est rétablie plus tard dans `loadSoundFont` qui appelle `applyPresetForStyle`.

---

## BUG-6 — applyDelta fallback corrompt tous les presets du SF2 [IMPORTANT]

**Fichier** : `plugin_vst3.h`, ligne ~1381–1384  
**Symptôme** : Quand `applyDeltaTargeted` ne trouve pas le preset ciblé (bank+prog), le fallback `applyDelta` modifie **tous** les générateurs IGEN/PGEN du SF2, corrompant tous les instruments. Si un SF2 multi-preset est chargé (ex : GeneralUser), tous ses sons sont affectés par le delta d'un seul style.

### Code bugué

```cpp
bool patched = sfed::applyDeltaTargeted(bytes, sf2DeltaPerStyle[style], bank, prog);
if (!patched) {
    sfed::applyDelta(bytes, sf2DeltaPerStyle[style]);  // ← patche TOUT le SF2
}
```

### Cas déclencheur
`applyDeltaTargeted` retourne `false` si :
- Le preset (bank, prog) n'existe pas dans ce SF2
- Le SF2 n'a pas de chunk PGEN ou PBAG valide

### Fix (option A — supprimer le fallback)

```cpp
bool patched = sfed::applyDeltaTargeted(bytes, sf2DeltaPerStyle[style], bank, prog);
if (!patched) {
    MM_LOG("reloadExternalSf: preset %d/%d not found in SF2 for style %d — delta NOT applied", bank, prog, style);
    // Ne pas appliquer applyDelta global : on charge le SF2 sans delta plutôt
    // que de corrompre tous ses presets.
}
```

### Fix (option B — fallback ciblé sur prog 0/banque 0 uniquement)

```cpp
if (!patched) {
    // Tenter avec prog 0 (premier preset du SF2) comme fallback minimal
    patched = sfed::applyDeltaTargeted(bytes, sf2DeltaPerStyle[style], 0, 0);
}
```

---

## BUG-7 — posWeight calculé mais jamais utilisé [IMPORTANT]

**Fichier** : `plugin_vst3.h`, ligne ~704  
**Symptôme** : La pondération rythmique de la vélocité (accents sur les temps forts/faibles) est calculée mais n'est jamais appliquée. MSVC génère le warning C4189 `local variable initialized but not referenced`. Le résultat : toutes les notes ont la même vélocité de base, sans accent rythmique.

### Code bugué

```cpp
bool isStrong = (beatIdx == 0);
bool isDown   = (beatIdx == 2);
double posWeight = isStrong ? 1.20 : isDown ? 1.00 : 0.55;  // ← jamais utilisé
// ...
velocity = velForStyle(style, slot, sp, baseVelShared)
           * sv.vels[vi];  // posWeight absent
```

### Comportement attendu
La vélocité de chaque note devrait être multipliée par `posWeight` pour accentuer le beat 1 (forte) vs les sous-divisions légères (piano).

### Fix

```cpp
velocity = velForStyle(style, slot, sp, baseVelShared)
           * sv.vels[vi]
           * (float)posWeight;  // ← appliquer la pondération
```

Note : vérifier que `velocity` reste dans [0.0f, 1.0f] après multiplication.

---

## BUG-8 — introFadeDone jamais réinitialisé [MINEUR]

**Fichier** : `plugin_vst3.h` (membres de la classe)  
**Symptôme** : `introFadeDone` est mis à `true` une fois (au passage de kSecIntro → kSecMain), puis reste `true` pour toute la session. Si l'utilisateur repasse manuellement en `kSecIntro`, la fade-in de l'intro est sautée.

### Fix

```cpp
// Dans le bloc qui détecte le passage vers kSecIntro :
if (prevSection != kSecIntro && newSection == kSecIntro) {
    introFadeDone = false;  // réinitialiser pour rejouer la fade-in
}
```

---

## BUG-9 — loadSoundFont utilise kSfPresetCount au lieu de effectivePresetCount [MINEUR]

**Fichier** : `plugin_vst3.h`, ligne ~1907 (dans `loadSoundFont`)  
**Symptôme** : La première passe de binding dans `loadSoundFont` utilise le count statique :

```cpp
// Premier binding (avant scanAndLoadUserSf) :
int idx = std::clamp(sfPresetIdxPerStyle[s], 0, kSfPresetCount[s] - 1);  // ← tronque au max statique
```

Si un index sauvegardé dans l'état pointe dans la plage des user SFs (idx >= kSfPresetCount[s]), il est tronqué ici. La seconde passe (après scanAndLoadUserSf) utilise `effectivePresetCount` et corrige le problème... mais seulement si le second clamp ne tronque pas non plus (il utilise `effectivePresetCount` correct à ce stade).

**Conséquence réelle** : Transitoire, corrigé dans la seconde passe. Impact nul en pratique si la seconde passe s'exécute toujours (elle est dans le même `loadSoundFont`). Problème potentiel si `scanAndLoadUserSf` échoue ou est désactivée.

### Fix (optionnel)
Supprimer la première passe de binding ou la remplacer par un binding neutre, et ne faire qu'une seule passe finale après `scanAndLoadUserSf`.

---

## BUG-10 — Race condition note-on/note-off lors du swap de synth [MINEUR]

**Fichier** : `plugin_vst3.h` (`sfNoteOn` / `reloadExternalSf`)  
**Symptôme** : Une note est déclenchée sur `sfSynth[style]` (note-on). Pendant que la note est tenue, l'UI déclenche `reloadExternalSf`, qui acquiert `sfMutex` et swap `sfSynth[style]` avec un nouveau TSF. La note-off suivante appelle `sfNoteOff`, qui acquiert à son tour `sfMutex` et cherche la note par `styleIdx` dans `sfActive`. Elle envoie le note-off au **nouveau** synth — qui n'a jamais reçu le note-on correspondant. La note est "stuck" sur l'ancien synth (deallocated).

### Analyse détaillée

```cpp
// Thread audio :
sfNoteOn(style=2, pitch=60, ...) → tsf_channel_note_on(sfSynth[2], 0, 60, vel)
sfActive[i] = { used=true, pitch=60, styleIdx=2, ... }

// Thread UI (reloadExternalSf) :
{ lock_guard lk(sfMutex);
  oldSynth = liveSfPerStyle[2].synth;
  sfSynth[2] = newSynth;   // ← swapé
}
tsf_close(oldSynth);        // ← l'ancien synth (avec la note active) est fermé

// Thread audio (note-off) :
sfNoteOff(noteId, 60) → tsf_channel_note_off(sfSynth[2]=newSynth, 0, 60)
// newSynth n'a pas reçu le note-on → note ignorée
// oldSynth (qui avait la note) est fermé → possible corruption mémoire si tsf_close
// libère des ressources pendant que renderVoices les utilise encore
```

### Fix
Avant de fermer `oldSynth`, lui envoyer un `tsf_channel_sounds_off_all` sur tous les canaux (déjà fait pour `sfSynth[style]`), mais en séquençant correctement depuis le thread audio. Une solution robuste : différer `tsf_close(oldSynth)` d'un bloc audio complet via une queue SPSC.

```cpp
// Déjà présent dans reloadExternalSf :
if (sfSynth[style]) tsf_channel_sounds_off_all(sfSynth[style], ch);  // ← OK
// Le oldSynth est fermé immédiatement — c'est ici le problème.
// Fix minimal : ne pas fermer l'ancien synth depuis le thread UI ;
// le marquer pour fermeture différée (ex: pendingCloseSynths.push_back(oldSynth))
// et le fermer depuis le thread audio au début du prochain bloc.
```

---

## Synthèse des bugs dans sf2_editor.h

### readPresetGens : lecture PGEN puis IGEN ✅ (OK)

La fonction `readPresetGens` suit correctement la chaîne PHDR → PBAG → PGEN (step 1) → IGEN (step 2). Les valeurs IGEN remplacent les valeurs PGEN pour le même générateur. **Pas de bug ici** — contrairement à ce qui était supposé dans l'analyse initiale. La fonction lit bien au niveau de l'instrument (IGEN).

**Attention cependant** : `readPresetGens` ne lit que la première zone d'instrument (zone globale ou première zone de sample). Les SF2 complexes avec plusieurs zones (splits de note) donnent une valeur représentative mais pas nécessairement la valeur de chaque split. Pour l'affichage des labels delta, c'est suffisant.

### applyDeltaTargeted ✅ (OK)

`applyDeltaTargeted` patche uniquement les PGEN du preset ciblé. La logique est correcte. Le problème (BUG-6) vient du code d'appel dans `plugin_vst3.h`, pas de `applyDeltaTargeted` lui-même.

### applyDelta (global) ⚠️ (utilisé comme fallback non sécurisé)

`applyDelta` patche tous les IGEN et PGEN du SF2 — comportement correct pour un usage délibéré (SF2 mono-preset). Le problème est l'usage comme fallback inconditionnel dans `reloadExternalSf`.

---

## Récapitulatif des corrections à appliquer

### Corrections immédiates (CRITIQUEs)

1. **plugin_vst3.h L528** : `vals[5]` → `vals[6]`, ajouter valeur pour Piano dans chaque preset.
2. **plugin_vst3.h `userSfs`** : Ajouter `userSfs.reserve(64)` dans `initialize()` ou avant `scanAndLoadUserSf()`.
3. **plugin_vst3.h `findUserSfFor`** : `u.nameWide.c_str() == target.name` → `u.nameWide == target.name`.

### Corrections importantes

4. **plugin_vst3.h `getState`/`setState`** : Sérialiser `sf2DeltaPerStyle`, `sf2SelBankPerStyle`, `sf2SelProgPerStyle` (v3.3).
5. **plugin_vst3.h L1146** : `std::clamp((int)w, 0, 3)` → `std::clamp((int)w, 0, 127)`.
6. **plugin_vst3.h L1384** : Supprimer ou limiter le fallback `sfed::applyDelta` global.
7. **plugin_vst3.h L~704** : Appliquer `posWeight` dans le calcul de vélocité.

### Corrections mineures

8. **plugin_vst3.h** : Réinitialiser `introFadeDone = false` quand on passe en `kSecIntro`.
9. **plugin_vst3.h** : Différer `tsf_close(oldSynth)` dans `reloadExternalSf` pour éviter UB.
