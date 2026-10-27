# Studio Voice Pro — v0.2 « moteur + format de projet »

Moteur audio C++20 **sans IHM, sans JUCE et sans aucune dépendance externe**, compilable et
testable sans installation système, accompagné du **format de projet versionné** (JSON, sauvegarde
atomique, chemins médias relatifs) et de ses outils en ligne de commande.

C'est le socle sur lequel la couche audio Windows écrite maison (WASAPI puis ASIO) et l'IHM native
Win32 + Direct2D/DirectWrite seront construits — voir `docs/ADR.md` (architecture ratifiée, sans
JUCE).

> Toutes les commandes et toutes les sorties montrées ci-dessous ont été réellement exécutées sur la
> machine de développement (Ubuntu 24.04, g++ 13.3, CMake 3.28.3). Voir « Preuves ».
> Rien de ce qui touche Windows (WASAPI, ASIO, `.exe`, IHM native) n'a été compilé ni exécuté :
> ces éléments sont marqués **à vérifier sur Windows 11**.

---

## 1. Compiler

**Important : compiler dans `/tmp`, jamais dans `/home`** (le système de fichiers `/home` ne dispose
que d'environ 290 Mo, `/tmp` de 3 Go).

```bash
cd /home/team/shared/studio-voice-pro
rm -rf /tmp/svp-build
cmake -S . -B /tmp/svp-build -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/svp-build -j2
```

Binaires produits dans `/tmp/svp-build/bin/` : `svp-render`, `svp-gen`, `svp-project`, `svp_tests`.

## 2. Lancer les tests

```bash
ctest --test-dir /tmp/svp-build --output-on-failure
```

**Sept suites** inscrites dans ctest :

| Suite | Contenu |
|---|---|
| `svp.audio` | buffers, canaux, cadence, dBFS ↔ linéaire |
| `svp.convert` | conversion PCM 16/24/32f, rééchantillonnage, taux d'échantillonnage |
| `svp.dsp` | gain, gain en dB, fondus, panoramique, mute/solo, mixage multipiste |
| `svp.metering` | crête, RMS, maintien du pic |
| `svp.transport` | lecture, arrêt, position, boucle, tempo, temps musical |
| `svp.io` | WAV PCM 16/24 bits et flottant 32 bits (aller-retour, en-tête, fichiers invalides) |
| `svp.project` | format de projet : fidélité aller-retour, chemins, sauvegarde atomique, autosave, préférences, compatibilité |

Pour n'en exécuter qu'une et voir le détail des cas :

```bash
/tmp/svp-build/bin/svp_tests project     # ou audio | convert | dsp | metering | transport | io
```

Sans argument, `svp_tests` exécute tout et affiche le total. Les fichiers WAV de test sont écrits dans
`/tmp/svp-build/test-tmp`.

## 3. Traiter un fichier WAV avec `svp-render`

```bash
mkdir -p /tmp/svp-demo && cd /tmp/svp-demo
/tmp/svp-build/bin/svp-gen --out sinus1k-48k-2s.wav --freq 1000 --rate 48000 --dur 2 --bits 32f
/tmp/svp-build/bin/svp-render --in sinus1k-48k-2s.wav --out sortie.wav --gain -6 --pan -0.3 --meter
```

Options de `svp-render` : `--in`, `--out`, `--gain <dBFS>`, `--pan <-1..+1>`, `--bits <16|24|32f>`,
`--mute`, `--meter`, `--help`. `--meter` imprime crête et RMS en dBFS avant et après traitement.

## 4. Créer et éditer un projet avec `svp-project`

```
svp-project <commande> --project <fichier.svpproj.json> [options]

  new        crée un projet vierge (--name, --rate, --bits, --tempo, --force)
  add-track  ajoute une piste (--name, --gain, --pan, --mute, --solo, --input,
             --output, --color)
  add-clip   ajoute un clip (--track N, --file, --name, --at, --offset,
             --length, --gain, --fade-in, --fade-out)
  set        modifie un projet ou une piste (--track N, --gain, --pan, --mute,
             --no-mute, --solo, --no-solo, --input, --output, --track-name,
             --color ; sans --track : --name, --rate, --bits, --tempo, --length)
  save       réécrit le projet de façon atomique (--autosave pour écrire en
             plus <projet>.autosave.json)
  load       recharge le projet et affiche son état (--dump, --autosave)
  help       cette aide
```

Les pistes sont numérotées à partir de 1, les positions sont en échantillons. Un média introuvable
n'empêche pas l'ajout : le clip est signalé au chargement. Aucune commande n'écrit directement dans
le fichier du projet : le document complet part dans `<projet>.tmp`, puis est renommé (écriture
atomique).

Démonstration complète — créer → piste → clip → régler → sauver → recharger → afficher :

```bash
cd /tmp/svp-demo
/tmp/svp-build/bin/svp-gen --out media/voix.wav --freq 220 --rate 48000 --dur 3 --bits 24
/tmp/svp-build/bin/svp-project new       --project session.svpproj.json --name "Demo voix" \
                                         --rate 48000 --bits 24 --tempo 96 --force
/tmp/svp-build/bin/svp-project add-track --project session.svpproj.json --name "Voix principale" \
                                         --gain -3 --pan 0.25
/tmp/svp-build/bin/svp-project add-clip  --project session.svpproj.json --track 1 \
                                         --file media/voix.wav --at 0 --fade-in 0.1 --fade-out 0.2
/tmp/svp-build/bin/svp-project set       --project session.svpproj.json --track 1 \
                                         --gain -4.5 --pan -0.2 --solo
/tmp/svp-build/bin/svp-project save      --project session.svpproj.json --autosave
/tmp/svp-build/bin/svp-project load      --project session.svpproj.json --dump
```

Sortie réelle de cette démonstration (voir « Preuves », §7) : le projet est rouvert avec le gain de
piste −4,500 dB, le panoramique −0,20, le solo actif, le clip `t1c1` de 144 000 échantillons
(3,000 s) et le chemin média **relatif** `media/voix.wav` conservé tel quel.

Le format sur disque est spécifié champ par champ dans **`docs/PROJECT-FORMAT.md`** (schéma et
version, règle de compatibilité, précision numérique, chemins relatifs/absolus, sauvegarde atomique et
autosauvegarde, règle « jamais appelé depuis le thread audio », exemple minimal commenté).

## 5. Arborescence

```
studio-voice-pro/
├── CMakeLists.txt          bibliothèque svp_core, outils, tests
├── include/svp/            en-têtes publics (aucune dépendance externe)
│   ├── audio/              Buffer.h, Convert.h, SampleRate.h
│   ├── io/                 WavFile.h
│   ├── dsp/                GainPan.h, Mixer.h
│   ├── metering/           Meter.h
│   ├── transport/          Transport.h
│   ├── json/               Json.h (lecteur/écrivain JSON maison)
│   └── project/            Project.h (modèle), ProjectIO.h (E/S, préférences)
├── src/                    implémentations (mêmes modules)
├── tools/                  svp_render.cpp, svp_gen.cpp, svp_project.cpp
├── tests/                  svp_test.hpp + une suite par module + main.cpp
└── docs/                   ADR.md, PROJECT-FORMAT.md, WINDOWS-BUILD.md,
                            STYLE-GUIDE.md, LAYOUT-SPEC.md, README-DESIGN.md, maquettes/
```

## 6. Statut : TERMINÉ / PARTIEL / NON DÉVELOPPÉ

| Élément | Statut | Preuve / raison |
|---|---|---|
| Arborescence CMake C++20, `svp_core` sans JUCE ni IHM | **TERMINÉ** | compilation `-Wall -Wextra -Wpedantic -Wshadow` : 0 avertissement, 0 erreur |
| Module `audio` (buffers, conversion dBFS/PCM, taux, rééchantillonnage linéaire) | **TERMINÉ** | suites `svp.audio` + `svp.convert` : 9 cas, 0 échec |
| Module `io` (WAV PCM 16/24, flottant 32, code maison) | **TERMINÉ** | suite `svp.io` : 8 cas, 0 échec |
| Module `dsp` (gain, fondu, panoramique, mute/solo, mixage) | **TERMINÉ** | suite `svp.dsp` : 10 cas, 0 échec |
| Module `metering` (crête, RMS, maintien du pic) | **TERMINÉ** | suite `svp.metering` : 6 cas, 0 échec (sinus pleine échelle à −3,010 dBFS) |
| Module `transport` (lecture, arrêt, position, boucle, tempo) | **TERMINÉ** | suite `svp.transport` : 8 cas, 0 échec |
| Lecteur/écrivain JSON maison (aucune dépendance) | **TERMINÉ** | `src/json/Json.cpp`, cas `project :: json_reader_details` |
| Format de projet v1 : modèle, sauvegarde atomique, chemins relatifs, autosauvegarde, préférences | **TERMINÉ** | suite `svp.project` : 14 cas, 0 échec + démonstration `svp-project` en §4 |
| Documentation du format de projet | **TERMINÉ** | `docs/PROJECT-FORMAT.md` |
| Outils `svp-render`, `svp-gen`, `svp-project` | **TERMINÉ** | sorties réelles en §7 |
| Tests unitaires lancés par `ctest` | **TERMINÉ** | 7/7 suites, 55 cas, 100 % de réussite, deux exécutions consécutives identiques |
| Télémétrie IHM ↔ audio atomique (compteurs lisibles depuis un autre thread) | **NON DÉVELOPPÉ** | `PeakRmsMeter::values()` n'est pas atomique ; l'en-tête le déclare désormais explicitement (`Meter.h`) — prochaine tâche |
| Couche audio Windows écrite maison (WASAPI, puis ASIO) | **NON DÉVELOPPÉ** | aucune ligne écrite ; backend de test à venir avec elle |
| IHM native Win32 + Direct2D/DirectWrite | **NON DÉVELOPPÉ** | direction visuelle prête (`docs/STYLE-GUIDE.md`, `docs/LAYOUT-SPEC.md`, `docs/maquettes/`) ; aucun code |
| Enregistrement vocal, timeline multipiste, mixer complet | **NON DÉVELOPPÉ** | v0.3 |
| Effets vocaux, MIDI/piano roll, VST3, export avancé, installateur | **NON DÉVELOPPÉ** | v0.4/v0.5 |
| Routage entrée/sortie des pistes **appliqué** au mixage (`input`/`output` du projet) | **PARTIEL** | champs stockés et affichés ; le mixage ne les applique pas encore |
| ASIO / WASAPI / périphériques audio réels | **NON TESTÉ — à vérifier sur Windows 11** | aucun périphérique audio ici ; voir `docs/WINDOWS-BUILD.md` |
| Compilation du `.exe` Windows | **NON TESTÉ — à vérifier sur Windows 11** | aucun compilateur Windows ici |
| Build sur Windows 11 | **NON TESTÉ — à vérifier sur Windows 11** | procédure écrite dans `docs/WINDOWS-BUILD.md`, pas encore exécutée |

## 7. Preuves (sorties réelles du 17/10/2026)

Compilation depuis un répertoire de build neuf (`rm -rf /tmp/svp-build`, configure, build) :

```
[ 92%] Built target svp_tests
[100%] Built target svp-project
BUILD_EXIT=0
0 avertissement, 0 erreur
```

Tests — `ctest --test-dir /tmp/svp-build --output-on-failure`, **première exécution** :

```
    Start 1: svp.audio
1/7 Test #1: svp.audio ........................   Passed    0.00 sec
    Start 2: svp.convert
2/7 Test #2: svp.convert ......................   Passed    0.00 sec
    Start 3: svp.dsp
3/7 Test #3: svp.dsp ..........................   Passed    0.00 sec
    Start 4: svp.metering
4/7 Test #4: svp.metering .....................   Passed    0.00 sec
    Start 5: svp.transport
5/7 Test #5: svp.transport ....................   Passed    0.00 sec
    Start 6: svp.io
6/7 Test #6: svp.io ...........................   Passed    0.00 sec
    Start 7: svp.project
7/7 Test #7: svp.project ......................   Passed    0.00 sec

100% tests passed, 0 tests failed out of 7
```

**Seconde exécution immédiate** (stabilité, mêmes conditions) :

```
100% tests passed, 0 tests failed out of 7
Total Test time (real) =   0.02 sec
```

Total des cas unitaires : `suite « (toutes) » : 55 cas exécuté(s), 0 échec(s)`.

`svp-render` sur le sinus 1 kHz / 48 kHz / 2 s généré par `svp-gen` :

```
entrée      : sinus1k-48k-2s.wav
  format    : flottant 32 bits, 48000 Hz, 1 canal(aux), 96000 trames (2.000 s)
mesures AVANT traitement :
  crête =    0.000 dBFS   RMS =   -3.010 dBFS   [écrêtage]
mesures APRÈS traitement (gain -6 dB, pan -0.3) :
  crête =   -6.000 dBFS   RMS =   -9.010 dBFS
  écart de gain mesuré : -6.000 dB
sortie      : sortie-gain-6-pan.wav
```

La valeur RMS de −3,010 dBFS correspond exactement au sinus pleine échelle attendu (1/√2 =
−3,0103 dB).

Démonstration `svp-project` (séquence du §4), sortie intégrale :

```
nouveau projet « Demo voix » : session.svpproj.json
  48000 Hz, 24 bits, 96.000 bpm, 0 piste(s)
piste 1 « Voix principale » ajoutée (gain -3.000 dB, pan 0.25, mute non, solo non)
projet enregistré : session.svpproj.json
  écriture atomique via session.svpproj.json.tmp puis renommage
longueur déduite de l'en-tête WAV : 144000 échantillons
clip t1c1 « voix.wav » ajouté sur la piste 1
projet enregistré : session.svpproj.json
  écriture atomique via session.svpproj.json.tmp puis renommage
piste 1 mise à jour : gain -4.500 dB, pan -0.20, mute non, solo oui
projet enregistré : session.svpproj.json
  écriture atomique via session.svpproj.json.tmp puis renommage
projet enregistré : session.svpproj.json
  écriture atomique via session.svpproj.json.tmp puis renommage
  sauvegarde automatique : session.svpproj.json.autosave.json
Projet « Demo voix » — session.svpproj.json
  schéma            : studio-voice-pro.project v1 (écrit par Studio Voice Pro 0.2.0)
  taux d'échantillonnage : 48000 Hz
  profondeur        : 24 bits entier
  tempo             : 96.000 bpm
  longueur          : 0 échantillons (0.000 s)
  pistes            : 1
  clips             : 1
  [1] t1 « Voix principale »  gain -4.500 dB  pan -0.20  mute non  solo oui  entrée none  sortie master  couleur #5AA9E6
      clip t1c1 « voix.wav »  media/voix.wav
        début 0  décalage 0  longueur 144000 (3.000 s)  gain 0.00 dB  fondus 0.100/0.200 s
        chemin stocké dans le fichier : media/voix.wav
```

Ce que cette sortie prouve, précisément : le projet rouvert est **identique** à ce qui a été réglé
(gain −4,5 dB, pan −0,2, solo), le clip a gardé sa position, sa longueur et ses fondus, le chemin
média est resté **relatif**, et la sauvegarde automatique a écrit un fichier **distinct** dont le
contenu est identique au projet (`diff` sans différence).

Compatibilité du format, vérifiée avec le même binaire :

```
# document ne contenant que schema, schemaVersion, un champ inconnu et tracks: []
avertissements :
  - projet.champDuFutur : champ inconnu ignoré        → projet chargé (vide, 48 kHz, 32 bits, 120 bpm)

# même document avec "schemaVersion": 2
ERREUR : version de schéma 2 inconnue : cette version lit jusqu'à la version 1.
         Le projet n'a pas été modifié ; ouvrez-le avec une version plus récente de Studio Voice Pro.
(exit 1)
```

## 8. Défauts trouvés et corrigés pendant la mise au point

1. L'écriture WAV écrivait les canaux à la suite (canal majeur) au lieu de les **entrelacer** : les
   fichiers multipistes étaient relus de travers. Corrigé ; le test d'aller-retour stéréo le protège.
2. Le mixage d'une source **mono vers un bus stéréo** n'alimentait que le canal gauche. Corrigé : une
   source mono alimente les deux canaux avec les gains de panoramique.
3. `dsp::mixStrips` était la seule fonction marquée `[RT]` sans `noexcept`, et son commentaire
   renvoyait à une fonction `réserveMix()` **qui n'a jamais existé**. Corrigé : `noexcept` ajouté, et
   la fausse consigne remplacée par la vraie contrainte d'appel (buffer de mixage dimensionné hors
   temps réel).
4. `metering::PeakRmsMeter` affirmait que ses valeurs étaient « sûres à lire depuis l'IHM sur un
   instantané atomique » alors qu'il n'existe **aucun `std::atomic`** dans le module. Le commentaire
   décrit maintenant l'état réel (aucune synchronisation) et marque la télémétrie atomique comme
   travail à faire — sans changer le comportement du code.

## 9. Limites connues de cette version

- **Windows non testé** : l'exécutable Windows 11, WASAPI, ASIO et l'IHM native ne peuvent pas être
  compilés ni testés sur cette machine (aucun compilateur Windows, aucun périphérique audio). Voir
  `docs/WINDOWS-BUILD.md`.
- **Pas encore d'IHM** : uniquement des outils en ligne de commande.
- **Pas encore de lecture audio** : le transport est testé, mais rien n'est joué par une carte son.
- **Routage des pistes non appliqué** au mixage (champs stockés et affichés seulement).
- **Rééchantillonnage linéaire** seulement : à remplacer par une interpolation à fenêtre en v0.3.
- **Formats WAV acceptés** : PCM 16, PCM 24, flottant 32 bits. Pas d'AIFF, pas de PCM 32 entier, pas
  d'ADPCM ; les chunks inconnus sont ignorés.
- **Pas d'écrêtage de sortie** dans le mixage : le mix peut dépasser 1,0 ; un limiteur viendra avec
  le mixer (v0.3).
- **Métrologie** : mesure sur le bloc complet (pas de fenêtrage LUFS/EBU R128 ; prévu en v0.4).
- **`svp-render` charge le fichier entier en mémoire** : convient aux tests, pas encore aux fichiers
  très longs.
- **Aucun plugin, aucun effet, aucune automation** : le format de projet ne les décrit pas encore.

## 10. Documentation

| Document | Contenu |
|---|---|
| **`docs/PROJECT-FORMAT.md`** | Spécification du format de projet : schéma et version, chaque champ, règles de compatibilité, précision numérique, chemins médias, sauvegarde atomique, autosauvegarde, threading, exemple minimal commenté |
| **`docs/ADR.md`** | Décision d'architecture : sans JUCE, couche audio Windows écrite maison (WASAPI puis ASIO), rendu Win32 + Direct2D/DirectWrite, VST3 pour les plugins, état des licences avec sources datées |
| **`docs/STYLE-GUIDE.md`** | Cahier de style de l'IHM native (couleurs, typographie, états des contrôles, densité) |
| **`docs/LAYOUT-SPEC.md`** | Disposition des zones de la fenêtre en DIP (barre de titre, transport, timeline, console de mixage…) |
| `docs/README-DESIGN.md` | Index des maquettes et de leurs sources |
| `docs/maquettes/` | Maquettes d'écran (PNG) de la fenêtre principale, de la console de mixage et du détail transport/vumètre |
| `docs/WINDOWS-BUILD.md` | Procédure de compilation et de test **sur Windows 11** — **non testée**, à vérifier sur le poste du propriétaire |
