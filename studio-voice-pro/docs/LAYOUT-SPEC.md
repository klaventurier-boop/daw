# Studio Voice Pro — disposition des zones (LAYOUT-SPEC)

Version 1 (2026-10-17, designer). Cible : fenêtre native Win32/ Direct2D, **pas** de CSS :
les valeurs ci-dessous sont des rectangles en pixels indépendants de la résolution (DIP) à
100 % de mise à l'échelle.

Référence de dessin : fenêtre de **1440 × 900**. Toutes les cotes sont exprimées en `x, y,
largeur × hauteur` depuis le coin supérieur gauche de la zone client (sous la barre de
titre native si le système la dessine ; ici la barre de titre est dessinée par l'application).

---

## 1. Grille générale

```
┌──────────────────────────────────────────────────────────────────────────┐  0
│ BARRE DE TITRE                                        H = 32   (fixe)     │
├──────────────────────────────────────────────────────────────────────────┤  32
│ BARRE DE TRANSPORT                                     H = 56   (fixe)    │
├───────────┬──────────────────────────────────────────────┬───────────────┤  88
│           │  ARRANGEMENT (règle 28 + pistes)             │               │
│ NAVIGATEUR│  (flexible : hauteur)                        │  RACK D'EFFETS│
│  W = 236  │                                              │   W = 280     │
│  (fixe)   ├──────────────────────────────────────────────┤  (largeur     │
│           │  CONSOLE DE MIXAGE   H = 236  (redimension.) │   ajustable)  │
├───────────┴──────────────────────────────────────────────┴───────────────┤
│ BARRE D'ÉTAT                                           H = 24   (fixe)    │
└──────────────────────────────────────────────────────────────────────────┘
```

Largeur client 1440 : 236 (navigateur) + 924 (centre) + 280 (rack) = 1440. La colonne
centrale se partage verticalement entre arrangement (flexible) et console (hauteur
réglable).

### Tableau des zones

| Zone | Ancrage | Largeur | Hauteur | Redimensionnable |
|---|---|---|---|---|
| Barre de titre | haut, pleine largeur | 100 % | 32 fixe | non |
| Barre de transport | haut, pleine largeur | 100 % | 56 fixe | non |
| Navigateur | gauche | 236 fixes (mini 180, maxi 480) | reste | **largeur** par sa poignée droite |
| Rack d'effets | droite | 280 par défaut (mini 240, maxi 420) | reste | **largeur** par sa poignée gauche |
| Arrangement | centre, haut | reste | ≥ 240 | **hauteur** par la poignée de la console |
| Console de mixage | centre, bas | reste | 236 par défaut (mini 160, maxi 60 % du centre) | **hauteur** |
| Barre d'état | bas, pleine largeur | 100 % | 24 fixe | non |

Toutes les poignées font **6 px** d'épaisseur visible au survol (`line-drag`, 2 px dans une
bande de 6 px de zone sensible), curseur `IDC_SIZEWE` / `IDC_SIZENS`. Double-clic sur la
poignée : retour à la valeur par défaut.

---

## 2. Barre de titre (0, 0, 1440 × 32)

Ancrage horizontal avec ressort entre les groupes :

| Élément | x | Largeur | Détail |
|---|---|---|---|
| Marque « SVP » | 10 | 18 | carré 18 × 18, rayon 2, fond `accent` |
| Nom du produit | 34 | 140 | 13 px SemiBold |
| Nom du projet | 184 → ressort | variable, tronqué par ellipse | 12 px, `text-tertiary`, 1 px de séparateur à gauche |
| Info moteur | à droite, avant les boutons | 300 | 11 px, `text-secondary` |
| Boutons système | 1308 | 132 (3 × 44) | Réduire / Agrandir / Fermer, 32 px de haut, pleine hauteur |

Comportement : si la largeur client < 900, le nom du projet disparaît en premier, puis
l'info moteur. La marque et le nom du produit ne disparaissent jamais.

## 3. Barre de transport (0, 32, 1440 × 56)

Disposition en groupes, marge interne 12, gouttière 10, séparateurs verticaux de 1 px
(`line-strong`, 28 px de haut, marge 6).

| Groupe | x | Éléments |
|---|---|---|
| Transport | 12 | Retour au début (32), Arrêt (32), **Lecture/Pause** (36 × 36) |
| Enregistrement | 122 | Enregistrement (36 × 36) |
| Boucle / Métronome | 168 | Boucle (28 × 28), Métronome (28 × 28) |
| Séparateur | 224 | |
| Position | 236 | Afficheur 170 × 40 (Consolas 22 px) |
| Mesures | 412 | Cellule 104 × 40 |
| Tempo | 522 | Cellule 92 × 40 |
| Signature | 620 | Cellule 64 × 40 |
| Séparateur | 696 | |
| Master | 706 | Libellé 30, valeurs P/R 56, vumètre horizontal 148 × 10 + témoin 8 × 6 |
| Ressort | — | |
| Réglages audio | bord droit − 40 | Bouton 28 × 28 |

Comportement en largeur réduite : le bloque Master se réduit en premier (le vumètre
horizontal passe à 96 px), puis les cellules Mesures et Signature passent en mode compact
(valeur seule, libellé supprimé) sous 1100 px client. La position et les boutons de
transport ne sont jamais réduits ni repliés.

## 4. Navigateur (0, 88, 236 × reste)

- En-tête 24 px (titre + compteur), onglets 24 px (Projet / Disques / Effets), arborescence
  en dessous (ligne de 22 px, repli 12 px).
- Marges internes 8 px ; colonne de taille de fichier alignée à droite sur 8 px du bord.
- Défilement vertical uniquement ; pas de défilement horizontal (les noms sont tronqués
  par ellipse).
- En dessous de 180 px de large : les tailles de fichiers et les icônes disparaissent,
  seul le nom reste.

## 5. Arrangement (236, 88, 924 × flexible)

Sous-zones internes :

| Élément | Rectangle | Remarque |
|---|---|---|
| Règle temporelle | x+0, y+0, 924 × 28 | hauteur fixe ; colonne gauche de 300 réservée à l'en-tête de colonne |
| Liste des pistes (en-têtes) | x+0, y+28, **300 × (n × 56)** | largeur **fixe**, 300 px |
| Zone des clips | x+300, y+28, **(924 − 300) = 624** × reste | défile horizontalement |
| Ligne « Ajouter une piste » | sous la dernière piste, 28 px | toujours visible |
| Tête de lecture | 2 px de large, de y+8 au bas de la zone des clips | par-dessus les clips |

- Hauteur de piste : 56 px par défaut ; compact 40 px ; étendue 88 px (contrôles
  supplémentaires). Le changement est global (menu), pas par piste.
- Si le nombre de pistes dépasse la hauteur disponible, un ascenseur vertical apparaît à
  droite de la zone des clips **et** défile les en-têtes en même temps (défilement
  synchronisé, jamais désolidarisé).
- Le zoom horizontal change la largeur d'une mesure (`39 px` à 16 mesures visibles) mais
  **pas** la largeur de l'en-tête de piste (300 px) ni la hauteur des lignes.
- Largeur minimale de la zone des clips : 320 px. Si la fenêtre descend sous
  236 + 320 + 240 + poignées ≈ **810 px de large**, la fenêtre bute (taille minimale de
  fenêtre imposée : 900 × 600).

## 6. Console de mixage (236, 88 + hArr, 924 × 236)

- En-tête 24 px (titre, compteur, rappel de l'échelle).
- Tranches de canal de **104 px** de large, hauteur restante ; tranche master **152 px**
  collée à droite ; séparateur 1 px entre tranches.
- Si `924 < n × 104 + 152`, la console défile horizontalement (barre de défilement 8 px).
- Les tranches ne se compriment jamais en dessous de 96 px : on défile, on ne tasse pas.
- Hauteur minimale 160 px : en dessous, les inserts et les envois d'une tranche sont
  masqués (fader + vumètre + nom restent).

## 7. Rack d'effets (1160, 88, 280 × reste)

- En-tête 24 px, corps avec marge 8 et gouttière 8.
- Emplacements d'insertion de 28 px, liste verticale ; en dessous, section Envois (2
  boutons rotatifs par ligne), puis section Entrée & monitoring.
- Sous 240 px de large : les libellés d'envois passent à la ligne au-dessus des boutons
  (le rack défile verticalement).

## 8. Barre d'état (0, 876, 1440 × 24)

Trois groupes : moteur (gauche, indicateur + périphérique), format audio (centre-gauche),
compteurs CPU / disque / latence (droite). En dessous de 1000 px de large, le nom du
périphérique est tronqué ; en dessous de 800, les compteurs CPU/disque disparaissent
(latence conservée).

---

## 9. Comportement au redimensionnement de la fenêtre

| Situation | Attendu |
|---|---|
| Élargissement horizontal | la colonne centrale (arrangement + console) gagne **toute** la largeur ; navigateur et rack gardent leur largeur courante (définie par l'utilisateur) |
| Rétrécissement horizontal | la colonne centrale perd d'abord ; quand elle atteint 810 px de large (320 de zone clips + 300 d'en-têtes + marges), les largeurs du navigateur et du rack sont réduites proportionnellement, jusqu'à leurs minima (180 et 240) ; ensuite la fenêtre bute à 900 px de large |
| Élargissement vertical | l'arrangement gagne toute la hauteur ; la console et la barre d'état gardent leur hauteur |
| Rétrécissement vertical | l'arrangement perd d'abord ; à 240 px de haut il cesse de se réduire, puis la console descend jusqu'à 160 px ; la fenêtre bute à 600 px de haut |
| Fenêtre maximisée / multi-écran | les valeurs restent en DIP : à 150 % de mise à l'échelle, tous les pixels logiques sont multipliés par 1,5, la mise en page ne change pas de structure |
| Double-clic sur une poignée | restauration de la largeur/hauteur par défaut de la zone concernée |
| Fermeture / réouverture | les largeurs du navigateur et du rack et la hauteur de la console sont enregistrées dans le fichier de préférences de l'application (pas dans le projet) |

Règles invariantes (à ne jamais casser) :

1. La barre de transport ne se replie jamais et ne défile jamais : ses commandes restent
   toutes accessibles à la taille minimale de fenêtre.
2. L'en-tête de colonne de la liste des pistes (300 px) reste aligné au pixel avec la
   première mesure de la règle temporelle, quel que soit le défilement horizontal.
3. La tête de lecture est tracée une seule fois, en surimpression, sur la règle et sur les
   clips : jamais deux traits désalignés.
4. Le défilement vertical de l'arrangement est unique et synchronisé entre en-têtes et
   clips.
5. Toute poignée de redimensionnement reste atteignable au clavier (Tab) et déplaçable
   avec les flèches (± 8 px, ± 1 px avec Ctrl).
