# Cahier des charges — Midnight Melody Maker VST3
> Reconstitué à partir de tous les prompts utilisateur de la session.

---

## 1. Outil Python de composition musicale

| Fonctionnalité | Statut |
|---|---|
| Script Python de composition musicale progressive | ✅ Fait |
| Blocs de longueur variable (pas de mesures rigides) | ✅ Fait |
| Multi-pistes : melody / bass / pad / arp | ✅ Fait |
| Piano roll multi-pistes | ✅ Fait |
| Progressions d'accords Markov | ✅ Fait |
| Prévisualisation audio via FluidSynth (optionnelle) | ✅ Fait |
| Export MIDI via mido | ✅ Fait |
| Sauvegarde / chargement JSON | ✅ Fait |
| Auto-détection du SoundFont GeneralUser GS | ✅ Fait |

---

## 2. SoundFonts

| Fonctionnalité | Statut |
|---|---|
| Téléchargement d'un bon SoundFont (GeneralUser GS 1.472) | ✅ Fait |

---

## 3. Plugin VST3 — Interface générale

| Fonctionnalité | Statut |
|---|---|
| Numéro de version affiché à côté du titre dans l'en-tête | ✅ Fait |
| JAM affiché dans l'en-tête (comme un toggle) | ✅ Fait |
| Own-SF2 affiché dans l'en-tête avec le même style que JAM | ✅ Fait (bouton SF2 ON/OFF dans le header, clic → onglet Maker) |
| 6 sorties stéréo séparées par instrument dans le patcher | ❌ Non fonctionnel |
| Boutons Mute par instrument | ✅ Déjà présent |
| Icône rafraîchir (flèche circulaire) en haut à droite | ✅ Présent dans l'onglet Maker |
| Dé de Seed affiché uniquement dans le bon onglet | ✅ Corrigé (caché dans viewMode==2 Maker) |
| Ordre des instruments modifié | ✅ Fait |

---

## 4. Plugin VST3 — Onglet Melody (génération musicale)

| Fonctionnalité | Statut |
|---|---|
| Mode JAM : suit la mélodie des autres instruments | ✅ Fait |
| Basse en rythme avec la batterie quand JAM est activé | ✅ Fait |
| La basse fait des doubles/triples notes en mode JAM | ✅ Fait |
| Correction arrêts de génération de notes aléatoires | ✅ Fait |
| Correction du son double quand le volume FL Studio est monté | ✅ Fait |

---

## 5. Plugin VST3 — Onglet FX

| Fonctionnalité | Statut |
|---|---|
| Chorus / Delay / Reverb par instrument (activable indépendamment) | ✅ Fait |
| Générateur de bruit cassette aléatoire | ✅ Fait |
| Bruit plus aléatoire (moins rythmé) | ✅ Fait |
| Curseur bruit logarithmique (pas exponentiel) | ✅ Fait |
| Potentiomètres FX : 3 par effet, alignés horizontalement | ✅ Fait |
| Intensité des effets FX moins agressive (logarithme) | ✅ Fait |
| Trigger Oui/Non centré dans FX | ✅ Fait |

---

## 6. Plugin VST3 — Onglet Maker (éditeur SF2)

| Fonctionnalité | Statut |
|---|---|
| Bouton "Personal SoundFont" Oui/Non par instrument | ✅ Fait |
| Menu déroulant avec liste des instruments dans le SF2 | ✅ Corrigé (populateInstrumentCombo montre les vrais presets SF2) |
| Parsing et tri des instruments disponibles dans le SF2 | ✅ Fait (sfed::listPresets) |
| Rechargement SF2 dynamique (au relâchement de la souris) | ✅ Corrigé (debounce 400ms + fix tsf_channel_set_bank_preset) |
| Grille de 31 potentiomètres pour paramètres SF2 | ✅ Fait |
| Potentiomètres Enveloppe : 4 centrés (Atk/Hold/Dec/Rel) | ❌ Non fonctionnel |
| Potentiomètres Timbre : 3 centrés (Filtre, Q, Attenuation) | ✅ Fait |
| Labels compréhensibles pour chaque paramètre | ✅ Fait |
| Scroller dans le Maker pour voir tous les paramètres | ✅ Fait |
| Le scroller cache les instruments qui défilent (z-order) | ❌ Non fonctionnel |
| Alignement titre bleu avec la 2ème partie | ✅ Fait |
| Présentation en 5 colonnes | ✅ Fait |
| Tous les paramètres SF2 spec présents (31 générateurs) | ✅ Fait |
| Bons min/max par style (harpe = son de harpe même en extrêmes) | ✅ Fait |
| Curseurs centrés à zéro-delta par défaut | ✅ Fait |
| Pas de doublon de paramètres avec les autres onglets | ✅ Fait |
| **La sélection d'un instrument change le son entendu** | ✅ Corrigé (tsf_channel_set_bank_preset + vrais presets dans combo) |
| **Les 31 curseurs modifient réellement le son** | ⚠️ Code corrigé (applyDeltaTargeted reconstruit IGEN), à vérifier |
| **Modifier variables internes SF2 (pas créer de zéro)** | ⚠️ Code corrigé, à vérifier |

---

## 7. Bugs identifiés — Cause racine diagnostiquée

### Bug #1 — `readPresetGens` ne lit que PGEN
- **Symptôme** : La sélection d'un instrument depuis le combo SF2 ne change pas le son affiché dans les labels (valeurs hardcodées au lieu des vraies valeurs SF2).
- **Cause** : `readPresetGens` scanne uniquement PGEN (niveau preset). Or, les enveloppes ADSR, LFO, filtre sont stockées dans IGEN (niveau instrument). Le snapshot retourne donc toujours les valeurs par défaut de la spec SF2.
- **Fix nécessaire** : Suivre la chaîne PHDR→PBAG→PGEN(GEN_INSTRUMENT=41)→INST→IBAG→IGEN pour lire les vraies valeurs.
- **Statut** : ❌ **Non corrigé — fix en cours**

### Bug #2 — `patchGen` ne peut pas insérer de nouveaux records
- **Symptôme** : Les 31 curseurs delta n'ont aucun effet audible.
- **Cause** : `patchGen` ne modifie QUE les records déjà présents dans le SF2. Si un générateur n'a pas de record explicite (utilise le défaut implicite SF2), le delta est silencieusement ignoré.
- **Fix nécessaire** : Réécrire `applyDeltaTargeted` pour reconstruire la section IGEN avec insertion de nouveaux records quand le delta est non-nul.
- **Statut** : ❌ **Non corrigé — fix en cours**

---

## 8. Architecture / Refactoring

| Fonctionnalité | Statut |
|---|---|
| Découpage plugin en plusieurs fichiers (plugin_vst3.h, view.cpp, plugin_vst3.cpp) | ✅ Fait |
| Mémo COPILOT_MEMO.md avec fonctionnalités et index des lignes | ✅ Fait |
| Script Python check_braces.py (vérification accolades) | ✅ Fait |
| Script Python diagnose_cpp_syntax.py (tracking erreurs syntaxe) | ✅ Fait |
| Gestion d'erreur et logs dans fichier externe | ✅ Fait (logs_ichigos.txt) |

---

## 9. Preset saver / loader

| Fonctionnalité | Statut |
|---|---|
| Preset saver/loader natif | ✅ Fait |
| Override du preset loader/saver FL Studio | ⚠️ Partiellement (VST3 state save/load) |

---

## 10. Piano Roll (notes 30–31) ❌ Non fait

| Fonctionnalité | Détail | Statut |
|---|---|---|
| Décaler la ligne de lecture | Position du curseur de lecture ajustable | ❌ |
| Bouton Undo / Redo | Remettre les boutons undo/redo dans le piano roll | ❌ |
| Clavier numérique pour rythme | Saisir les notes rythmiques via pavé numérique | ❌ |
| Clavier AZERTY piano | Mapping clavier AZERTY → notes piano | ❌ |

---

## 11. Paramétrage musical (notes 28–29) ❌ Non fait

| Fonctionnalité | Détail | Statut |
|---|---|---|
| Fréquence de tune | Ajouter un paramètre de fréquence de référence (ex. 432 Hz) | ❌ |
| Humanize — notes décalées | Notes légèrement décalées dans le temps de façon aléatoire | ✅ Fait (knob Humanize dans onglet Melody) |
| Subdiv — problème de rythme | Régler le problème de rythme appliqué à tous les instruments | ❌ |
| Subdiv — notes longues | Pas nécessaire de doubler le tempo — mettre plus de longues notes, à partir de 1/4 | ❌ |
| Subdiv — laisser respirer | Ajouter des silences / respiration dans les patterns | ❌ |

---

## 12. Export MIDI / WAV (note 26) ❌ Non fait

| Fonctionnalité | Détail | Statut |
|---|---|---|
| Appliquer le soundfont à l'export | Rendre le son SF2 dans le fichier WAV exporté | ❌ |
| Export multi-piste | Une piste / un fichier par instrument lors de l'export | ❌ |
| Drag & drop to export | Glisser-déposer le pattern/bloc pour exporter | ❌ |

---

## 13. Looper (note 28) ❌ Non fait

| Fonctionnalité | Détail | Statut |
|---|---|---|
| Cases à cocher lecture progressive | Sélectionner quelles mesures (n°a/b) jouent dans la boucle | ❌ |
| Multi seed | Plusieurs seeds simultanés ou alternés dans le looper | ❌ |

---

## 14. Instruments — améliorations musicales (note 29) ❌ Non fait

| Fonctionnalité | Détail | Statut |
|---|---|---|
| Percussions — ajouts de rythmes | Plus de patterns rythmiques pour les percussions | ❌ |
| Percussions — plus de notes | Utiliser plus de notes/sons différents dans les kits batterie | ❌ |
| Basse — plus riche | Voicing plus riche pour la basse (harmoniques, variations) | ❌ |
| Contre-mélodie | Instrument dédié à la contre-mélodie | ❌ |
| Atmosphère — notes longues | Mode atmosphère avec des notes tenues longues | ❌ |

---

## 15. Pad (note 30) ❌ Non fait

| Fonctionnalité | Détail | Statut |
|---|---|---|
| Instrument Pad | Ajouter un instrument de type Pad dans le plugin | ❌ |

---

## 16. Pattern (note 31) ❌ Non fait

| Fonctionnalité | Détail | Statut |
|---|---|---|
| Sauvegarde d'un pattern édité | Sauvegarder un pattern modifié manuellement | ❌ |
| Import d'un pattern MIDI | Importer un fichier MIDI comme pattern de base | ❌ |

---

## Résumé

- **Réellement fonctionnel** : ~20 fonctionnalités confirmées
- **En code mais non confirmé fonctionnel** : 6 sorties, z-order scroller, enveloppe 4 knobs centrés, curseurs SF2 (applyDeltaTargeted)
- **Jamais implémenté** : 25 fonctionnalités du mind map (sections 10–16)
- **Priorité** : Vérifier et implémenter les items ❌ dans l'ordre du mind map
