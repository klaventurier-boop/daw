# Format de projet Studio Voice Pro — `.svpproj.json`

Version du document : 1 (identique à `svp::project::kSchemaVersion`).
Code de référence : `include/svp/project/Project.h` (modèle), `ProjectIO.h` /
`src/project/ProjectIO.cpp` (lecture, écriture, sauvegarde), `include/svp/json/Json.h` (lecteur /
écrivain JSON maison, aucune dépendance externe).

Ce document fait foi. Si le code et ce document divergent, c'est un défaut : le code se corrige, ou
ce document est mis à jour **dans le même changement**.

---

## 1. Objet et portée

Un projet est **un seul fichier texte JSON** (UTF-8, sans BOM) portant l'extension
`.svpproj.json`. Il décrit : les paramètres de session, les pistes, les clips posés sur ces pistes
et les paramètres de mixage de chaque piste. Les **médias audio ne sont pas dans le fichier** : les
clips les référencent par un chemin (§5).

Le fichier est écrit intégralement à chaque sauvegarde (pas de journal incrémental). Il est
volontairement lisible par un humain et comparable par un `diff` : c'est la base des tests de
fidélité (« sauver → recharger → resauver » doit produire le même texte).

Fichiers voisins, **distincts du projet** :

| Fichier | Rôle |
|---|---|
| `<nom>.svpproj.json` | le projet lui-même |
| `<nom>.svpproj.json.tmp` | fichier temporaire d'écriture atomique (§7.1) — supprimé si tout va bien |
| `<nom>.svpproj.json.autosave.json` | sauvegarde automatique (§7.2), jamais le même fichier que le projet |
| `preferences.json` | préférences de l'application, hors projet (voir §10) |

---

## 2. Numéro de schéma : `schema` et `schemaVersion`

```json
"schema": "studio-voice-pro.project",
"schemaVersion": 1
```

- **`schema`** est l'identité du document (`svp::project::kSchemaName`). S'il est présent et
  différent, le fichier est **refusé** : « ce fichier n'est pas un projet Studio Voice Pro ».
  S'il est absent, le chargement continue avec un **avertissement** « schema : absent (projet écrit
  par une version antérieure ?) ».
- **`schemaVersion`** est un entier ≥ 1 : le numéro de révision de la structure décrite ici. C'est
  le seul champ **obligatoire sans défaut** : absent → erreur « schemaVersion : champ obligatoire
  absent » ; non entier → erreur ; `< 1` → erreur.
- **Version plus récente que celle du logiciel** (`schemaVersion > 1` aujourd'hui) → **erreur de
  chargement**, avec le message « version de schéma N inconnue : cette version lit jusqu'à la
  version 1 ». Le fichier n'est **pas** modifié, **pas** converti, **pas** écrasé : ouvrir un projet
  récent avec un logiciel ancien ne détruit rien, l'utilisateur doit installer une version plus
  récente. C'est volontaire : un lecteur ancien ignorerait des champs qui changent le sens du
  projet et réécrirait ensuite un fichier appauvri.

À quoi sert de monter la version : voir la règle de compatibilité (§6).

---

## 3. Structure générale

```json
{ schema, schemaVersion, appVersion, name, sampleRate, bitDepth,
  tempoBpm, lengthFrames, mediaPathBase, tracks: [ Track ] }

Track = { id, name, color, input, output, params: Params, clips: [ Clip ] }

Params = { gainDb, pan, mute, solo }

Clip = { id, name, mediaPath, startFrames, sourceOffsetFrames, lengthFrames,
         gainDb, fadeInSeconds, fadeOutSeconds }
```

Ordre des champs à l'écriture : celui du code (`toJson`). Le lecteur, lui, **ne dépend pas de
l'ordre** des membres JSON.

---

## 4. Description des champs

### 4.1 Niveau projet

| Champ | Type | Défaut | Sens |
|---|---|---|---|
| `schema` | chaîne | — | Identité du document, `"studio-voice-pro.project"`. |
| `schemaVersion` | entier | — (obligatoire) | Révision du format, `1` aujourd'hui (§2). |
| `appVersion` | chaîne | `"0.2.0"` | Version du logiciel qui a écrit le fichier. Informatif : **jamais** utilisé pour décider comment lire. |
| `name` | chaîne | `"Sans titre"` | Nom affiché de la session. Une valeur vide est remplacée par « Sans titre » **avec un avertissement**. |
| `sampleRate` | entier (Hz) | `48000` | Taux d'échantillonnage de référence de la session. Valeur inconnue → avertissement et repli (le taux n'est jamais arrondi en texte). |
| `bitDepth` | entier | `32` | Profondeur visée à l'export : `16`, `24` ou `32` (32 = flottant). Autre valeur → avertissement, repli à 32. |
| `tempoBpm` | flottant | `120.0` | Tempo de la grille musicale (battements par minute), > 0. |
| `lengthFrames` | entier 64 bits | `0` | Longueur de session **explicitement fixée**, en échantillons. `0` = aucune longueur fixée : la longueur effective se déduit des clips. Le chargement ne réécrit pas ce champ. |
| `mediaPathBase` | chaîne | `"projectFile"` | Convention d'interprétation des `mediaPath` : relative au dossier du fichier de projet. Toute autre valeur → avertissement, et les chemins sont résolus depuis le dossier du projet (comportement de `"projectFile"`). |
| `tracks` | tableau de `Track` | `[]` | Les pistes, dans l'ordre d'affichage. |

### 4.2 Piste (`Track`)

| Champ | Type | Défaut | Sens |
|---|---|---|---|
| `id` | chaîne | `t1`, `t2`… | Identifiant stable et déterministe (`t` + numéro de piste). Sert aux références internes et à des `diff` stables. |
| `name` | chaîne | `"Piste N"` | Nom affiché. |
| `color` | chaîne | `"#5AA9E6"` | Couleur d'affichage, `#RRGGBB`. Purement esthétique : une valeur libre est conservée telle quelle. |
| `input` | chaîne | `"none"` | Routage d'entrée (périphérique, ou bus). `"none"` = aucune entrée. Interprété par le mixer (v0.4) ; stocké tel quel pour l'instant. |
| `output` | chaîne | `"master"` | Routage de sortie : nom de bus (`"master"`) ou identifiant d'une autre piste. Stocké tel quel ; le routage n'est **pas encore** appliqué au mixage. |
| `params` | objet `Params` | voir 4.3 | Paramètres de mixage de la piste. |
| `clips` | tableau de `Clip` | `[]` | Clips posés sur la piste. |

### 4.3 Paramètres de piste (`params`)

| Champ | Type | Défaut | Sens |
|---|---|---|---|
| `gainDb` | flottant (dB) | `0.0` | Gain de la piste en dBFS ; `0` = neutre (unit gain). |
| `pan` | flottant | `0.0` | Panoramique : `-1` = tout à gauche, `0` = centre, `+1` = tout à droite. Loi de panoramique constante en puissance (côté moteur, `dsp::panGains`). |
| `mute` | booléen | `false` | Piste muette. |
| `solo` | booléen | `false` | Piste en solo. Si au moins une piste est en solo, seules les pistes en solo et non muettes sont audibles (`dsp::isChannelAudible`). |

### 4.4 Clip (`Clip`)

| Champ | Type | Défaut | Sens |
|---|---|---|---|
| `id` | chaîne | `t1c1` | `t<n>c<m>` : piste n, clip m. Déterministe, donc `diff` stable. |
| `name` | chaîne | nom du fichier | Nom affiché. Vide au chargement ⇒ remplacé par le nom de fichier du média. |
| `mediaPath` | chaîne | — (obligatoire) | Chemin du média **tel que stocké** : relatif (séparateurs `/`) ou absolu (§5). Absent ⇒ erreur de chargement. |
| `startFrames` | entier 64 bits | `0` | Position du début du clip sur la timeline, en échantillons depuis le début de la session. |
| `sourceOffsetFrames` | entier 64 bits | `0` | Décalage à l'intérieur du média : l'échantillon du média qui est joué au début du clip. |
| `lengthFrames` | entier 64 bits | `0` | Durée du clip en échantillons. `0` = convention « jusqu'à la fin du média » ; ce champ n'est pas réécrit au chargement, c'est le lecteur du média qui connaît sa longueur. `svp-project add-clip` écrit, lui, la longueur résolue depuis l'en-tête WAV. |
| `gainDb` | flottant (dB) | `0.0` | Gain propre au clip, en dBFS. |
| `fadeInSeconds` | flottant (s) | `0.0` | Fondu d'entrée, en secondes (converti en échantillons avec le `sampleRate` du projet). |
| `fadeOutSeconds` | flottant (s) | `0.0` | Fondu de sortie, en secondes. |

### 4.5 Unités et conventions

- **Positions et durées** : entiers d'échantillons (`int64_t`), jamais des secondes flottantes pour
  la position — pas de dérive cumulée. Les fondus sont les seules durées exprimées en secondes
  (valeurs littérales choisies par l'utilisateur, converties à l'usage).
- **Gains** : dBFS, flottants, `0.0` = neutre.
- **Panoramique** : `[-1, +1]`, `0` = centre.
- **Temps musical** : `tempoBpm` en bpm ; la conversion temps musical ↔ échantillons est faite par
  `transport` (le format ne stocke pas de positions musicales).

### 4.6 Champs à sens unique (écrits **ou** lus, jamais les deux)

- `Clip::mediaMissing` (C++) : renseigné **au chargement** seulement, jamais écrit dans le fichier ;
  il indique qu'un média référencé est introuvable. Un média absent **n'empêche pas** le chargement
  du projet : le clip reste, il est listé dans « médias manquants » par `svp-project load`.
- `Clip::storedMediaPath` (C++) : forme du chemin mémorisée au chargement, jamais écrite sous ce nom
  (c'est `mediaPath` qui porte la valeur, §5.2).

---

## 5. Chemins médias : relatifs, absolus, et forme stockée

### 5.1 Ce qui est écrit

`makeStoredMediaPath()` décide de la forme **à l'écriture** :

- média **sous** le dossier du projet → chemin **relatif** au dossier du projet, séparateurs `/`
  (`media/voix.wav`, `voix.wav` si le média est à côté du projet) ;
- média **hors** du dossier du projet (ou chemin en `..`, ou dossier non résolvable) → chemin
  **absolu** normalisé.

Un chemin relatif en `../` n'est jamais écrit : il est fragile dès qu'on déplace le dossier, et
illisible. On préfère alors l'absolu. Les séparateurs sont toujours `/`, même si le système écrit
`\` : le fichier reste lisible et identique sur Windows et sur Linux.

`resolveStoredMediaPath()` fait l'opération inverse **à la lecture** : relatif → résolu depuis le
dossier du fichier de projet ; absolu → rendu tel quel. Le dossier du projet est celui du **fichier
de projet**, pas le répertoire courant au moment du chargement.

Conséquence pratique : déplacer le dossier du projet **avec** son sous-dossier `media/` conserve
tous les liens ; déplacer le seul fichier `.svpproj.json` casse les liens relatifs (ils restent
relatifs au dossier du fichier, où les médias ne sont plus).

### 5.2 Règle de conservation de la forme stockée

Le chargement **mémorise la forme exacte** lue dans le fichier (`Clip::storedMediaPath`), puis la
réutilise à l'écriture **tant qu'elle désigne encore le même média** :

```
forme réécrite = storedMediaPath  si  resolve(storedMediaPath, dossierProjet) == mediaPath
               = makeStoredMediaPath(mediaPath, dossierProjet)   sinon
```

C'est ce qui rend « sauver → recharger → resauver » **identique octet pour octet** : un projet
ouvert sur une autre machine, dont les chemins absolus sont valides, n'est pas réécrit avec des
chemins recalculés. La forme n'est recalculée que si le clip a réellement changé de média, de nom
ou de dossier de projet.

---

## 6. Compatibilité : la règle d'évolution du format

Les trois cas, et rien d'autre :

| Changement | `schemaVersion` | Comportement d'un lecteur plus ancien |
|---|---|---|
| **Champ ajouté** (nouveau nom, avec une valeur par défaut côté lecteur) | **inchangé** | ignore le champ, **avertissement** « … : champ inconnu ignoré », chargement réussi |
| **Champ inconnu rencontré** (écrit par une version plus récente) | — | **ignoré avec avertissement**, le reste du projet est chargé |
| **Champ qui change de sens, champ supprimé, unité ou type modifié** | **incrémenté** | version trop récente ⇒ **erreur explicite**, fichier non modifié |

Détails vérifiables dans le code (`src/project/ProjectIO.cpp`, `warnUnknownKeys`) :

- Chaque niveau (racine, piste, `params`, clip) a sa **liste blanche** de clés connues ; tout membre
  absent de cette liste produit un avertissement nommant le chemin exact, par exemple
  `tracks[0].params.foo : champ inconnu ignoré`. Rien n'est silencieux.
- Un avertissement **n'empêche pas** le chargement et **n'altère pas** les valeurs lues.
- En revanche, un champ connu mais d'un **type erroné** (chaîne là où un entier est attendu…) est une
  **erreur** de chargement : mieux vaut refuser que deviner.
- Un champ optionnel absent prend sa valeur par défaut : c'est exactement ce qui rend un champ
  ajouté rétro-compatible, et c'est la raison pour laquelle **le défaut doit être le comportement
  d'avant l'ajout**.

Vérifié sur cette machine (svp-project, build du 17/10/2026) : un document ne contenant que
`schema`, `schemaVersion`, un champ inventé `champDuFutur` et `tracks: []` se charge comme un projet
vide nommé « Sans titre » (48 kHz, 32 bits, 120 bpm) avec un seul avertissement
`projet.champDuFutur : champ inconnu ignoré` ; le même document avec `"schemaVersion": 2` est refusé
(code de sortie 1) avec le message « version de schéma 2 inconnue : cette version lit jusqu'à la
version 1 ».

Corollaire à ne pas oublier lors d'un ajout de champ : l'ancien lecteur **réécrira** le fichier
sans le champ inconnu (il ne le conserve pas). Ajouter un champ est donc sans risque de lecture,
mais pas sans perte si l'utilisateur rouvre et sauve avec l'ancienne version. Un champ dont la perte
serait grave (par exemple un réglage qu'on ne peut pas retrouver) doit déclencher une montée de
version, pas un simple ajout.

---

## 7. Sauvegarde

### 7.1 Écriture atomique

`saveProject()` n'écrit **jamais** directement dans le fichier du projet :

1. le document complet est sérialisé en mémoire ;
2. il est écrit dans `<projet>.tmp` ;
3. le fichier temporaire est **synchronisé sur disque** (`fsync`) ;
4. il est **renommé** sur le fichier final (renommage atomique sur le même système de fichiers) ;
5. le **dossier** est synchronisé à son tour, pour que le renommage lui-même survive à une coupure.

Le dossier parent est créé au besoin. Un projet **invalide** est refusé (rien n'est écrit).

Si l'étape 2 échoue, le `.tmp` est supprimé et le projet existant reste **intact**. Si l'étape 4
échoue, le `.tmp` est supprimé et le fichier précédent reste en place. Ces deux cas sont couverts
par les crochets de test `TestHooks` (`failTempWrite`, `abortBeforeReplace`), **désactivés en
production**, qui simulent l'échec pour le prouver.

Ce que l'écriture atomique garantit : à tout instant, le fichier `.svpproj.json` est soit
l'ancienne version complète, soit la nouvelle version complète — jamais un fichier tronqué ou
mélangé. Elle ne garantit pas la survie du **contenu** si le disque ment (cache matériel sans
alimentation) ; c'est le rôle du `fsync`.

### 7.2 Fichier d'autosauvegarde distinct

`saveAutosave()` écrit **`<projet>.autosave.json`**, un fichier **différent** du projet. Règle
tenue : une sauvegarde automatique ne peut **jamais** écraser une sauvegarde volontaire. Le
chargement de l'autosauvegarde est une action explicite (`svp-project load --autosave`) ; le
logiciel ne substitue pas silencieusement l'autosauvegarde au projet.

L'autosauvegarde est elle aussi écrite de façon atomique (`<projet>.autosave.json.tmp` puis
renommage). Sur le disque, projet et autosauvegarde sont **identiques quand rien n'a changé entre
les deux écritures** (vérifié par `diff` dans la démonstration du README).

### 7.3 Préférences

`preferences.json` est un fichier **séparé**, hors de tout projet (par défaut
`$SVP_CONFIG_DIR`, sinon `$XDG_CONFIG_HOME`, sinon `~/.config`, puis
`studio-voice-pro/preferences.json`). Il porte sa propre version (`schemaVersion`, aujourd'hui 1).
Un fichier absent **n'est pas une erreur** (valeurs par défaut) ; une version plus récente que
celle du logiciel est une erreur, et les valeurs par défaut sont conservées.

---

## 8. Garanties de précision numérique

Le format est conçu pour qu'un aller-retour **ne perde rien**, et les tests le vérifient champ par
champ (`tests/test_project.cpp`, comparaison sans tolérance via `operator==` et `firstDifference`) :

1. **Entiers** : positions, durées, taux et profondeur sont écrits en **décimal, sans exposant,
   sans séparateur de milliers** — `144000`, jamais `1.44e5`. Un entier 64 bits conserve donc
   exactement sa valeur, y compris au-delà de 2^53 (là où un flottant double perdrait des unités).
   Un nombre à virgule reçu là où un entier est attendu est refusé plutôt que tronqué.
2. **Flottants** : `writeDouble` écrit la **représentation décimale la plus courte qui se relit à
   l'identique** (`std::to_chars`, mode `chars_format::general`), avec repli sur `%.17g` si
   `to_chars` échoue. `0.1` s'écrit `0.1` et se relit comme le même double ; aucune valeur ne
   change au rechargement, et le texte reste court et lisible.
3. **Aucune dépendance à la locale** : ni la lecture ni l'écriture ne passent par `iostream`,
   `printf` avec conversion localisée, ni `setlocale`. Le séparateur décimal est toujours `.` et le
   séparateur de liste toujours `,`, quelle que soit la locale du système (français, allemand…).
   Un projet écrit sur une machine en `fr_FR` se relit à l'identique en `en_US`.

Ce que ces garanties **ne** couvrent pas : la valeur **audio** elle-même (un WAV PCM 16 bits relu
reste un PCM 16 bits ; il n'y a pas de conversion de format dans le projet), et les chemins de
fichiers, qui dépendent du système de fichiers (§5).

---

## 9. Threading : ce module n'est jamais appelé depuis le thread audio

**Règle absolue** : aucune fonction de `svp::project` (lecture, écriture, sérialisation,
résolution de chemins, préférences, autosauvegarde) n'est appelée depuis le thread audio.

Ces fonctions allouent de la mémoire, font des appels système (open/read/write/fsync/rename) et
peuvent bloquer pendant des dizaines de millisecondes : les appeler dans le callback audio
produirait des décrochages (glitches) et des dépassements d'échéance, impossibles à rattraper.

Le schéma d'appel prévu pour l'application (v0.2) :

- chargement/sauvegarde sur un **thread de travail** (thread d'interface ou pool dédié) ;
- publication du résultat au moteur audio par **échange de pointeur** (le moteur reçoit un état
  immuable prêt à l'emploi) ;
- le thread audio ne voit que des structures déjà allouées ; les éventuelles libérations sont
  différées hors du thread audio.

Le format de fichier n'est donc **jamais** dans le chemin temps réel : le thread audio ne connaît
que le modèle en mémoire que lui a transmis le thread de travail.

---

## 10. Exemple minimal commenté

Projet réel écrit par `svp-project` (voir la démonstration du `README.md`). Lecture :
une piste, un clip sur `media/voix.wav`, gain de piste −4,5 dB, panoramique −0,2, piste en solo.

```json
{
  "schema": "studio-voice-pro.project",
  "schemaVersion": 1,
  "appVersion": "0.2.0",
  "name": "Demo voix",
  "sampleRate": 48000,
  "bitDepth": 24,
  "tempoBpm": 96,
  "lengthFrames": 0,
  "mediaPathBase": "projectFile",
  "tracks": [
    {
      "id": "t1",
      "name": "Voix principale",
      "color": "#5AA9E6",
      "input": "none",
      "output": "master",
      "params": {
        "gainDb": -4.5,
        "pan": -0.2,
        "mute": false,
        "solo": true
      },
      "clips": [
        {
          "id": "t1c1",
          "name": "voix.wav",
          "mediaPath": "media/voix.wav",
          "startFrames": 0,
          "sourceOffsetFrames": 0,
          "lengthFrames": 144000,
          "gainDb": 0,
          "fadeInSeconds": 0.1,
          "fadeOutSeconds": 0.2
        }
      ]
    }
  ]
}
```

Correspondance ligne à ligne avec le sens des champs :

| Ligne | Lecture |
|---|---|
| `schema` / `schemaVersion` | identité et révision du format : ici la version 1, lue par tout logiciel ≥ 0.2 |
| `appVersion` | écrit par Studio Voice Pro 0.2.0 — informatif, aucune décision de lecture n'en dépend |
| `name` | « Demo voix », nom affiché de la session |
| `sampleRate` / `bitDepth` | session à 48 kHz, export visé en PCM 24 bits |
| `tempoBpm` | grille à 96 bpm (positions musicales calculées par `transport`) |
| `lengthFrames: 0` | aucune longueur fixée : la session s'étend selon ses clips |
| `mediaPathBase` | les `mediaPath` sont relatifs au dossier du fichier de projet (§5) |
| `tracks[0].id` / `name` / `color` | piste 1, « Voix principale », couleur d'affichage bleue |
| `input` / `output` | pas d'entrée assignée ; sortie sur le bus « master » |
| `params.gainDb: -4.5` | atténuation de 4,5 dB sur la piste |
| `params.pan: -0.2` | légèrement à gauche |
| `params.mute: false`, `params.solo: true` | piste non muette, en solo : les autres pistes seraient réduites au silence |
| `clips[0].mediaPath: "media/voix.wav"` | média **sous** le dossier du projet ⇒ chemin relatif (portable avec le dossier) |
| `startFrames: 0` | le clip commence au début de la session |
| `sourceOffsetFrames: 0` | il démarre au premier échantillon du média |
| `lengthFrames: 144000` | 3,000 s à 48 kHz — écrit par `add-clip` depuis l'en-tête WAV |
| `gainDb: 0` | pas de gain propre au clip |
| `fadeInSeconds` / `fadeOutSeconds` | fondu d'entrée de 100 ms, fondu de sortie de 200 ms |

Exemple minimal valide, réduit à l'essentiel (tous les champs optionnels omis) :

```json
{
  "schema": "studio-voice-pro.project",
  "schemaVersion": 1
}
```

Il se charge comme un projet vide nommé « Sans titre » à 48 kHz / 32 bits / 120 bpm, sans piste :
c'est exactement le contrat de compatibilité du §6 (chaque champ absent prend son défaut).

---

## 11. Ce que ce format ne décrit pas encore

À ne pas chercher dans le fichier tant que les versions correspondantes ne l'ont pas ajouté (et,
le jour où elles l'ajouteront, voir §6 pour la façon de le faire) :

- chaînes d'effets et plugins (VST3) par piste ou sur le bus master (v0.4) ;
- automation des paramètres dans le temps (v0.4) ;
- objets MIDI et pistes instrument (v0.4) ;
- réglages d'export (format, dithering, nom de fichier) ;
- points de boucle et marqueurs de la timeline ;
- historique d'annulation (jamais persisté : il vit en mémoire).

Ces absences sont volontaires : le format ne contient que ce que le moteur sait réellement
restituer aujourd'hui.
