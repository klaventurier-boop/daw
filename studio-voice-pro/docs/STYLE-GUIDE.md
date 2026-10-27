# Studio Voice Pro — Cahier de style de l'interface

Version 1 (2026-10-17, designer). S'applique à l'IHM native Windows 11 (Win32 + Direct2D /
DirectWrite). Toutes les valeurs sont **exactes et en pixels indépendants de la résolution**
(DIP). À 100 % de mise à l'échelle Windows, 1 DIP = 1 pixel physique.

Ce document ne décrit que des éléments **prévus au plan** : barre de transport, liste de
pistes, timeline et clips, vumètres, console de mixage, rack d'effets, navigateur de
fichiers. Tout ce qui est purement graphique dans les maquettes est listé dans
`README-DESIGN.md`.

**Décisions du propriétaire, 2026-10-17** — elles tranchent les options laissées ouvertes
par la designer et **prévalent sur toute mention contraire ailleurs dans ce document** :
- couleur d'accent : **jade `#37D2B2`** (les alternatives ambre et azur sont écartées) ;
- densité des pistes : **40 px (compact)** — la variante 56 px est abandonnée ;
- console de mixage : **intégrée en bas de la fenêtre principale** (pas de fenêtre détachée) ;
- pistes **colorées** avec les six teintes — l'interface n'est pas monochrome.

---

## 1. Principes

1. **Sombre, mais pas noir.** Le noir pur (`#000000`) est interdit : il crée des halos
   autour du texte clair et fatigue les yeux en studio peu éclairé. Le point le plus
   sombre de l'interface est `#08090B`.
2. **La couleur porte du sens.** Une teinte saturée signale toujours un état
   (enregistrement, solo, mute, sélection, écrêtage). Aucune couleur décorative.
3. **Densité avant confort.** Un ingénieur du son lit des niveaux, des noms de pistes et
   des paramètres d'un seul coup d'œil : on privilégie l'information utile par cm², avec
   une grille de base de 4 px et une police de 12 px.
4. **Aucune animation décorative.** Les seules choses qui bougent sont : la tête de
   lecture, les aiguilles de vumètre, les compteurs, le défilement pendant la lecture, la
   barre de progression de l'export et le clignotement du bouton d'enregistrement armé
   (1 Hz, uniquement quand l'enregistrement est armé, jamais en lecture seule).
   Pas de fondu, pas de rebond, pas de transition d'ouverture de panneau.
5. **Rien qui ressemble à un site web.** Pas de coins très arrondis, pas de dégradés
   décoratifs, pas de grosses cartes flottantes, pas d'ombres portées sur les panneaux,
   pas de texte centré dans un bouton large, pas de bandeau plein écran.

---

## 2. Palette

### 2.1 Profondeur (du plus sombre au plus clair)

| Jeton | Hex | Usage |
|---|---|---|
| `bg-void` | `#08090B` | Fond de fenêtre visible hors zones de travail (liseré entre panneaux), fond des popups |
| `bg-canvas` | `#101216` | Fond de la timeline (grille), fond de la console de mixage |
| `bg-panel` | `#171A20` | Surfaces de panneaux : liste des pistes, tranches de canal, navigateur, rack d'effets |
| `bg-chrome` | `#1C2027` | Barre de titre, barre de transport, en-têtes de panneaux, règle temporelle |
| `bg-raised` | `#22272F` | Contrôles et champs au repos, boutons de transport, badges de piste |
| `bg-hover` | `#2A3038` | Survolement d'un contrôle ou d'une ligne |
| `bg-active` | `#333A44` | Contrôle enfoncé / maintenu, piste en cours d'édition |

Règle d'empilement : un élément posé sur `bg-panel` utilise `bg-raised` ; un élément posé
sur `bg-chrome` utilise `bg-raised` également (jamais plus de deux niveaux d'écart entre un
contenant et son contenu).

### 2.2 Séparateurs et traits

| Jeton | Hex | Épaisseur | Usage |
|---|---|---|---|
| `line-hair` | `#262C34` | 1 px | Séparateur interne, contour de champ, ligne de grille de mesure |
| `line-strong` | `#39414C` | 1 px | Séparateur entre zones, cadre de la fenêtre, bord supérieur des panneaux |
| `line-drag` | `#4B5462` | 2 px (au survol) | Poignée de redimensionnement entre deux zones |
| `grid-bar` | `#1B1F26` | 1 px | Ligne de mesure dans la timeline (toutes les 4 temps) |
| `grid-beat` | `#15181E` | 1 px | Ligne de temps (battement) dans la timeline |

### 2.3 Texte

| Jeton | Hex | Contraste mesuré | Usage |
|---|---|---|---|
| `text-primary` | `#E8EBEF` | **15,7:1** sur `bg-canvas`, **14,6:1** sur `bg-panel` | Noms de pistes, valeurs, titres, timecode |
| `text-secondary` | `#A9B2BF` | **8,8:1** sur `bg-canvas`, **8,1:1** sur `bg-panel` | Libellés, unités, graduations, en-têtes de colonnes |
| `text-tertiary` | `#7B8492` | **4,8:1** sur `bg-canvas`, 4,5:1 sur `bg-panel` | Métadonnées peu importantes, texte des pistes désactivées |
| `text-disabled` | `#626A76` | 3,4:1 | **Uniquement** les contrôles réellement inactifs (exemptés par la règle 4,5:1 de la WCAG, car non opérables) |
| `text-on-accent` | `#08090B` | **10,2:1** sur `bg-accent` | Texte posé sur un aplat d'accent |

Le texte courant ne descend jamais sous **4,5:1**. Deux exceptions assumées et
documentées : `text-disabled` (contrôles inactifs) et les crêtes de vumètre (graphiques,
pas du texte).

### 2.4 Accent

| Jeton | Hex | Usage |
|---|---|---|
| `accent` | `#37D2B2` | Tête de lecture, anneau de focus, onglet actif, valeur de fader en cours de modification, clip sélectionné, bouton Lecture actif |
| `accent-hover` | `#4FE0C2` | Survolement d'un élément déjà accentué |
| `accent-pressed` | `#26B79A` | Enfoncement d'un élément accentué |
| `accent-veil` | `rgba(55,210,178,0.12)` | Remplissage d'une sélection de plage ou d'un clip sélectionné |

L'accent teinte `#37D2B2` est nommé **« Jade Studio »**. Il est volontairement éloigné du
vert de vumètre (`#35C46B`, plus jaune) et du rouge d'enregistrement : aucun état ne se
confond avec un autre. Alternative proposée au propriétaire : ambre `#E9A94C`
(cf. `README-DESIGN.md`, décisions ouvertes).

### 2.5 Couleurs d'état (sémantiques, jamais décoratives)

| Jeton | Hex | Contraste sur `bg-panel` | Usage |
|---|---|---|---|
| `state-record` | `#FF4D5E` | 5,0:1 | Bouton Enregistrement, piste armée, contour de clip en cours d'enregistrement |
| `state-solo` | `#FFC64B` | 11,2:1 | Bouton Solo actif (badge plein + lettre noire) |
| `state-mute` | `#9B87F5` | 7,6:1 | Bouton Mute actif (badge plein + lettre noire) |
| `state-arm` | `#FF4D5E` | 5,0:1 | Bouton d'armement d'enregistrement de piste (cercle plein) |
| `state-selection` | `#37D2B2` | 9,2:1 | Clip sélectionné, plage sélectionnée |
| `state-focus` | `#37D2B2` | — | Anneau de focus clavier, 2 px, à l'extérieur du contrôle |

Règle : un état actif est un **aplat plein** avec texte/icône `text-on-accent` (et non un
texte coloré sur fond sombre) — c'est ce qui reste lisible du premier regard, en faible
luminosité, sur un écran en angle.

### 2.6 Vumètres (échelle de niveau)

Échelle en dBFS, de `-60 dBFS` (bas) à `0 dBFS` (haut), **linéaire en décibels** (pas en
amplitude) : 1 dB = 1/60 de la hauteur du vumètre.

| Zone | Plage dBFS | Hex | Remarque |
|---|---|---|---|
| Zone sûre | −60 → −18 | `#35C46B` | Vert soutenu, 8,6:1 sur `bg-canvas` |
| Zone de vigilance | −18 → −6 | `#E8C547` | Jaune |
| Zone de surcharge | −6 → 0 | `#E5484D` | Rouge |
| Fond de piste du vumètre | — | `#0B0D10` | Avec 1 px de contour `line-hair` |
| Maintien de crête (peak hold) | — | `#E8EBEF` à 80 % | Trait horizontal de 1 px sur toute la largeur du canal |
| Maintien RMS | — | couleur de zone à 40 % | Bande translucide de 3 px |
| Témoin d'écrêtage **éteint** | — | `#2A3038` | Rectangle 8 × 6 px en haut du canal |
| Témoin d'écrêtage **allumé** | — | `#FF2D55` | Reste allumé **jusqu'au clic** sur le témoin (ou sur le master) ; c'est un maintien, pas un clignotement |

Règles de dessin du vumètre :
- Largeur d'un canal : **6 px**, gouttière 2 px, donc stéréo = 14 px, mono = 6 px.
- La barre de crête instantanée occupe toute la largeur ; le RMS est une bande de 3 px
  centrée, plus sombre.
- Attaque instantanée, retour : −20 dB/s sur le vumètre, maintien de crête figé 1 500 ms
  puis descente à −20 dB/s.
- Graduations chiffrées à −60, −30, −18, −12, −6, 0, en `text-secondary` 9 px, à droite du
  vumètre, uniquement sur le master et sur les tranches de console (pas sur les mini-
  vumètres de la liste des pistes).

### 2.7 Couleurs de piste (identification, 6 teintes)

Chaque piste reçoit l'une de ces six teintes, utilisée **uniquement** pour son liseré de
4 px dans la liste des pistes, le corps de ses clips (remplissage à 18 % d'opacité) et le
tracé de sa forme d'onde. Jamais pour du texte.

| # | Nom | Hex |
|---|---|---|
| 1 | Jade | `#37D2B2` |
| 2 | Azur | `#5AA6F0` |
| 3 | Ambre | `#E9A94C` |
| 4 | Lilas | `#A78BFA` |
| 5 | Corail | `#F07A6E` |
| 6 | Menthe | `#6BD5A8` |

---

## 3. Typographie

Polices **livrées avec Windows 11**, aucune police à installer, aucune police à licencier :

| Rôle | Police | Graisse | Taille | Interlettrage |
|---|---|---|---|---|
| Titre de fenêtre / nom de projet | Segoe UI Variable Display | 600 (SemiBold) | 13 px | 0 |
| Titre de panneau (« PISTES », « MIXAGE ») | Segoe UI Variable Text | 600 | 11 px, **majuscules**, +0,6 px d'interlettrage | +0,6 |
| Texte d'interface courant | Segoe UI Variable Text | 400 | 12 px | 0 |
| Libellé secondaire, unité, graduation | Segoe UI Variable Text | 400 | 10 px | +0,2 |
| Nom de piste | Segoe UI Variable Text | 600 | 12 px | 0 |
| Valeur numérique (gain, pan, fréquence, dB) | Segoe UI Variable Text | 600 | 12 px | 0 |
| **Timecode / position** | Consolas | 400 | 22 px | 0 |
| Valeurs de mesure (P/R, LUFS) | Consolas | 400 | 11 px | 0 |
| Nom de fichier / chemin | Consolas | 400 | 11 px | 0 |
| Grand nombre de la console (dB du fader) | Consolas | 400 | 13 px | 0 |

Règle absolue — **chiffres tabulaires pour toute valeur numérique** : positions
(`00:00:12,480`), mesures de temps (`3.2.1`), décibels (`-6,0 dB`), panoramique
(`C`, `L32`), fréquences (`120 Hz`, `6,5 kHz`), gains (`+3,0 dB`), échantillonnage
(`48 000 Hz`), taille de tampon (`256`), CPU (`12 %`).
- OpenType `tnum` activé : `DWRITE_FONT_FEATURE_TAG_TABULAR_FIGURES` pour Segoe UI
  Variable Text (`font-variant-numeric: tabular-nums` côté maquette).
- Consolas est déjà à chasse fixe : aucune action.
- Conséquence testable : deux timecodes successifs doivent avoir exactement la même
  largeur ; les colonnes ne doivent jamais « sauter » quand un chiffre change.

Autres règles :
- **Taille minimale de texte : 10 px.** Jamais en dessous, même pour une graduation.
- Police à chasse variable pour tout ce qui est du texte ; chasse fixe réservée aux
  nombres, chemins et noms de fichiers.
- Pas d'italique (sauf `hors-ligne` / `non disponible` en commentaire, jamais dans un
  contrôle).
- Pas de texte en gras au-delà de 600 (700 réservé au titre de projet et au timecode
  « enregistrement »).
- Décimales : **virgule** en français (`-6,0 dB`), point uniquement dans les versionnages
  et les noms de fichiers.

---

## 4. Grille, espacement, géométrie

### 4.1 Échelle d'espacement (px)

`2 · 4 · 6 · 8 · 12 · 16 · 24 · 32 · 48`
- 2 px : écart entre une icône et son libellé collé.
- 4 px : marge interne verticale d'un petit contrôle, écart entre boutons voisins.
- 6 px : marge interne horizontale d'un champ.
- 8 px : marge interne standard d'un panneau, gouttière entre colonnes de contrôles.
- 12 px : marge interne d'un en-tête de panneau.
- 16 px : séparation entre deux groupes fonctionnels.
- 24 px : séparation entre deux zones majeures.
- 32 / 48 px : marges de dialogue uniquement.

Tout alignement horizontal se fait sur un multiple de 2 px ; les colonnes de valeurs
numériques s'alignent à droite sur la même abscisse.

### 4.2 Hauteurs de référence

| Élément | Hauteur |
|---|---|
| Barre de titre (native, dessinée) | 32 px |
| Barre de transport | 56 px |
| Règle temporelle (timeline) | 28 px |
| Ligne de piste (hauteur par défaut) | 56 px (compakt : 40 px, étendue : 88 px) |
| En-tête de panneau | 24 px |
| Barre d'état | 24 px |
| Bouton de transport | 32 × 32 px |
| Bouton carré (M / S / armement) | 18 × 18 px |
| Champ de valeur | 24 px de haut |
| Tranche de canal (console) | 72 px de large |
| Bandeau de la console (en-tête de tranche) | 24 px |
| Ligne de rack d'effets | 28 px |

### 4.3 Rayons d'angle

| Jeton | Rayon | Usage |
|---|---|---|
| `radius-0` | 0 px | Timeline, clips, grille, séparateurs |
| `radius-1` | 2 px | Boutons carrés, badges, champs de valeur, caps de fader |
| `radius-2` | 3 px | Panneaux, boutons de transport, en-têtes |
| `radius-3` | 4 px | Fenêtres flottantes, menus, infobulles |
| — | **jamais plus de 4 px** | Pas de pastilles arrondies, pas de cartes |

### 4.4 Épaisseurs de trait

- 1 px : tous les traits structurels, contours de contrôle, grilles, graduations.
- 2 px : focus clavier, bordure de clip sélectionné, poignée de redimensionnement
  survolée, tête de lecture (2 px de large, non arrondie).
- 3 px : indicateur de valeur sur un fader, arc actif d'un bouton rotatif.
- 4 px : liseré de couleur d'une piste (liste des pistes et tranche de console).

### 4.5 Ombres

Pas d'ombre sur les panneaux, barres, boutons ni clips. Une seule ombre autorisée, pour
les surfaces réellement flottantes (menu contextuel, boîte de dialogue, infobulle) :
`0 8px 24px rgba(0,0,0,0.55)` + bordure 1 px `line-strong`.
Le survol ne se traduit **jamais** par une ombre : uniquement par `bg-hover`.

---

## 5. États des contrôles

Chaque contrôle doit fournir ces cinq états, plus un état « accentué » quand il porte un
état sémantique. Aucune transition animée entre les états : le changement est instantané.

| État | Fond | Bordure | Texte / icône |
|---|---|---|---|
| Repos | `bg-raised` `#22272F` | 1 px `line-hair` `#262C34` | `text-primary` |
| Survol | `bg-hover` `#2A3038` | 1 px `#39414C` | `text-primary` |
| Enfoncé | `bg-active` `#333A44` | 1 px `#39414C` | `text-primary`, icône décalée de 1 px vers le bas |
| Actif (sémantique) | couleur d'état (accent, rouge, ambre, violet) | 1 px, même couleur | `text-on-accent` `#08090B`, graisse 600 |
| Désactivé | `bg-panel` `#171A20` | 1 px `line-hair` | `text-disabled` `#626A76`, aucun survol, curseur par défaut |
| Focus clavier | inchangé | 2 px `accent` à l'extérieur (offset +1 px) | inchangé |

Détails par famille :

- **Bouton de transport** (32 × 32, `radius-2`) : icône 16 × 16 centrée, trait 1,5 px.
  Lecture active → fond `accent`, icône `text-on-accent`. Pause = même emplacement que
  Lecture (bascule), Stop et Retour au début séparés. Enregistrement → fond
  `state-record`, et clignotement 1 Hz du seul pictogramme quand l'enregistrement est armé
  mais pas lancé.
- **Bouton carré M / S / armement** (18 × 18, `radius-1`) : lettre 10 px, graisse 600,
  majuscule. Inactif = `bg-raised` + `text-secondary`. Actif = aplat plein de la couleur
  d'état + lettre `#08090B`. Survolement : la bordure prend 20 % de la couleur d'état, pour
  annoncer ce qui va s'allumer.
- **Champ de valeur** (24 px de haut, min 56 px de large) : fond `#0B0D10`, contour
  `line-hair`, chiffres tabulaires alignés à droite, unité en `text-secondary` 10 px
  collée à droite (marge 4 px). En édition : contour `accent`, sélection du texte en
  `accent-veil`.
- **Fader vertical** : rail de 4 px `#262C34`, cap 18 × 11 px `radius-1` rempli
  `#3A424C` avec un liseré supérieur 1 px `#4B5462`, trait de valeur 3 px en `accent`
  sous le cap. Graduations 1 px `line-hair` tous les 6 dB à gauche, chiffrées à
  0 / −6 / −18 / −36 / −60. Zone sensible : 24 px de large (le rail reste fin, la cible
  est large).
- **Bouton rotatif (pan, send, fréquence)** : 20 px de diamètre (28 px en console). Piste
  d'arc 3 px `#2E353E` de −135° à +135°, arc de valeur `accent`, indicateur 2 × 7 px
  `text-primary` du centre vers l'extérieur. Le rayon visuel est le rayon actif.
- **Curseur de valeur au survol (faders et boutons rotatifs)** — obligatoire : après
  **200 ms** de survol, une infobulle de 20 px de haut s'affiche **au-dessus** du contrôle,
  centrée, à 6 px au-dessus du cap : fond `#0B0D10`, bordure 1 px `#39414C`,
  `radius-2`, texte `text-primary` 11 px graisse 600 **chiffres tabulaires**, format
  `-6,0 dB` / `C` / `L32` / `120 Hz`. Pendant un glisser, l'infobulle reste affichée et se
  met à jour en continu ; elle disparaît au relâchement. Aucun fondu.
- **Case à cocher / bascule** (métronome, boucle) : 18 × 18 `radius-1`, état actif = aplat
  `accent` + coche 2 px `text-on-accent`. La bascule Boucle et le Métronome sont des
  boutons de transport de même taille que Lecture, pas des cases.
- **Ligne sélectionnée** (piste, fichier du navigateur, effet du rack) : fond `bg-hover`,
  liseré gauche 4 px `accent` sur la liste des pistes, ou bordure 1 px `accent` sur une
  ligne de fichier.
- **Curseurs de la souris** : flèche standard ; redimensionnement → `IDC_SIZEWE` /
  `IDC_SIZENS` sur les poignées ; déplacement d'un clip → `IDC_SIZEALL`.

---

## 6. Vumètres — spécification de dessin (récapitulatif)

1. Conteneur : fond `#0B0D10`, contour 1 px `line-hair`, `radius-0`.
2. Un canal = 6 px de large ; stéréo = deux barres de 6 px séparées de 2 px.
3. Échelle linéaire en dB de −60 à 0 sur la hauteur utile (hauteur − 8 px réservés au
   témoin d'écrêtage).
4. Segments vert / jaune / rouge aux seuils `-18` et `-6` dBFS, coupures franches.
5. Peak hold : trait 1 px `#E8EBEF` à 80 %, maintenu 1 500 ms.
6. RMS : bande de 3 px centrée, couleur de zone à 40 % d'opacité, retour −20 dB/s.
7. Témoin d'écrêtage : 8 × 6 px en haut, `#2A3038` éteint, `#FF2D55` allumé **maintenu**
   jusqu'au clic. Le clic sur le témoin le remet à zéro (et seulement lui).
8. Valeur numérique associée (console et master uniquement) : sous le vumètre, sur deux
   lignes, `P -3,2` et `R -12,6` en Consolas 11 px `text-secondary`, chiffres alignés.
9. Mini-vumètres de la liste des pistes : 4 px de large, sans graduations, sans valeur
   numérique.

---

## 7. Iconographie

- Style : pictogrammes monochromes **au trait**, 16 × 16 px, grille de 16, épaisseur 1,5 px,
  angles vifs ou `radius-1`, extrémités droites (pas d'arrondi excessif).
- Aucune icône pleine, sauf : Enregistrement (cercle plein), le point d'armement, et la
  coche d'un état actif.
- Jeu d'icônes à dessiner (aucune provenant d'une bibliothèque tierce, tout est original) :
  `play`, `pause`, `stop`, `record`, `loop`, `metronome`, `goto-start`, `piste` (trois
  lignes), `dossier`, `fichier-audio` (forme d'onde), `effet` (trois curseurs verticaux),
  `mixage` (deux faders), `crayon`, `gomme`, `loupe`, `engrenage` (réglages audio),
  `chevron-bas`, `fermer`.
- Une icône seule n'est acceptée que dans la barre de transport et la barre d'outils ; dans
  un menu, icône + libellé.
- Taille des pictogrammes de piste (M/S) : lettre, pas d'icône.

---

## 8. Contenu et libellés

- Langue de l'interface : **français**, avec les termes du métier conservés en anglais
  quand ils sont l'usage (`Mix`, `Pan`, `Solo`, `Mute`, `Loop`, `BPM`, `dB`).
- Libellés d'en-tête de panneau en majuscules 11 px : `NAVIGATEUR`, `PISTES`, `MIXAGE`,
  `EFFETS`.
- Les unités sont toujours affichées à côté de la valeur, jamais dans une infobulle
  seulement : `-6,0 dB`, `120 Hz`, `48 000 Hz`, `256 éch.`, `4/4`.
- Un nombre sans unité doit être compris par son contexte (`Solo`, `Mute`, tempo).

---

## 9. Ce que l'interface ne fait pas

- Pas de dégradés, sauf : le corps d'un clip (aplat de couleur de piste à 18 % vers 8 %
  sur la hauteur) et le vumètre (segments pleins).
- Pas de transparence floutée (aucun effet « verre »), pas de `backdrop-blur`.
- Pas de couleur d'accent sur trois éléments à la fois dans un même écran : l'accent sert
  au focus et à la sélection, s'il est partout il ne signale plus rien.
- Pas de texte sur une image, pas de bannière, pas d'illustration.
- Pas de bouton dont le libellé décrit une fonction absente : chaque contrôle visible est
  implémenté **ou** marqué « non vérifié » dans `README-DESIGN.md`.

---

## 10. Vérifications automatiques à faire en revue de code

1. Aucun couple (texte, fond) en dessous de 4,5:1, hors `text-disabled`.
2. Toute valeur numérique affichée avec `tnum` (ou Consolas).
3. Aucune police hors Segoe UI Variable Text/Display, Segoe UI, Consolas, Cascadia Mono.
4. Aucune animation hors les cinq autorisées au §1.4.
5. Aucun rayon > 4 px.
6. Toute couleur utilisée appartient à ce document (aucun hexadécimal en dur dans le code
   d'IHM : passer par la table de jetons).
