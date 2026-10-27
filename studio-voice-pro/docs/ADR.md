# ADR 001 — Architecture de Studio Voice Pro (sans JUCE)

Statut : **adopté** — conforme à la révision 3 du plan, ratifiée par le propriétaire le 17/10/2026.
Remplace la version du 16/10/2026, qui retenait JUCE pour l'IHM et les périphériques audio : cette
option est désormais **écartée** (§6).
Date de cette révision : 17 octobre 2026.

---

## 1. Décision en une phrase

Studio Voice Pro est une station audio **native Windows 11** écrite entièrement par nous, en C++20,
**sans framework tiers** : moteur DSP (`svp_core`, déjà livré), couche audio Windows écrite maison
sur les API publiques — **WASAPI d'abord, ASIO ensuite** — et interface native **Win32 + Direct2D /
DirectWrite**, avec le **SDK VST3** (MIT) pour l'hébergement des plugins.

## 2. Les cinq décisions

1. **C++20, un seul dépôt, aucune dépendance externe au socle.**
   `svp_core` (modules `audio`, `io`, `dsp`, `metering`, `transport`, `json`, `project`) n'a aucune
   dépendance : ni JUCE, ni API graphique, ni API système propre à Windows. Il se compile et se
   teste en ligne de commande sur la machine de développement actuelle (Ubuntu, sans carte son).
2. **Moteur strictement isolé de l'interface.**
   Le thread audio ne connaît que des structures déjà allouées ; le projet, les médias et les
   plugins sont chargés sur un thread de travail, puis publiés au moteur par échange de pointeur.
   Cette séparation existe pour une raison mesurable : elle rend le moteur testable sans écran ni
   périphérique audio, et elle est la condition d'un fonctionnement temps réel stable.
3. **Couche audio Windows écrite maison : WASAPI en priorité, ASIO ensuite.**
   - **WASAPI** (Windows Audio Session API), en **mode exclusif** pour la latence basse : API
     publique de Microsoft, fournie avec le Windows SDK — aucun SDK séparé à télécharger, aucune
     redevance, aucun accord à accepter, redistribution du produit libre.
   - **ASIO** (Steinberg) en second : indispensable pour être crédible auprès des studios équipés de
     cartes ASIO, mais le SDK **ne se redistribue pas** — chaque poste de compilation le télécharge
     et accepte l'accord de licence Steinberg. Le support ASIO est donc une **option de compilation**,
     pas une dépendance du produit livré.
   - Dans les deux cas, la logique commune (énumération des périphériques, négociation du format,
     taille de bloc, horloge, récupération après débranchement, bascule de périphérique à chaud) est
     du code à nous : c'est précisément ce que le plan accepte de payer en travail supplémentaire.
   - Un **backend de test** sans périphérique (horloge simulée) permet de développer et de tester la
     boucle audio ici, sur cette machine, avant toute exécution sur Windows.
4. **Interface native Win32 + Direct2D / DirectWrite.**
   Fenêtre et boucle de messages Win32 ; rendu Direct2D (formes, clips, vumètres, courbes) ;
   texte DirectWrite (rendu net aux échelles Windows 11, y compris multi-écrans et DPI mixte).
   Un seul chemin de rendu, pas de couche Web ni de moteur d'interface intermédiaire.
   La direction visuelle est fixée par `docs/STYLE-GUIDE.md` et `docs/LAYOUT-SPEC.md` (palette
   sombre, grille de 4 px, texte 12 px, zones en DIP), avec les maquettes de `docs/maquettes/`.
5. **Plugins : SDK VST3 (licence MIT).**
   Hébergement des plugins VST3 tiers, et plus tard publication de nos propres effets vocaux au
   format VST3. Le SDK est utilisable sous MIT avec mention de copyright ; les « VST3 Usage
   Guidelines » de Steinberg (usage de la marque, nom du produit) restent à respecter.

## 3. Pourquoi ce choix : code fermé vendable sans redevance de framework

Le propriétaire a demandé « une version à nous » plutôt qu'une licence de framework. Le raisonnement,
sans détour :

- **Ce que nous gagnons** : l'intégralité du code est à nous. Le produit peut être **vendu en code
  fermé sans payer de redevance de framework**, ni par siège, ni par revenu, ni par an. Aucune
  dépendance dont le prix ou les conditions peuvent changer après la livraison. Aucune obligation de
  publier nos sources.
- **Ce que cela coûte** : nettement plus de travail. L'énumération et la gestion des périphériques,
  les bizarreries de pilotes, la récupération d'erreurs audio et toute la boîte à outils d'interface
  sont à écrire. C'est assumé, avec une règle de conduite : **on n'écrit que ce dont le produit a
  besoin**, on ne refait pas un framework généraliste.
- **Ce que cela ne coûte pas** : rien au travail déjà accompli — `svp_core` a été écrit dès le départ
  sans JUCE et sans dépendance externe. Le format de projet, le lecteur/écrivain JSON, les outils en
  ligne de commande et les tests ne changent pas d'une ligne.

## 4. Règles d'ingénierie qui découlent de cette architecture

1. **Temps réel** : aucune allocation mémoire, aucun accès disque, aucun verrou non borné, aucun
   appel COM bloquant dans le thread audio. Toute allocation vit dans une phase de préparation
   (`prepare()`) appelée hors du thread audio ; les structures sont ensuite seulement lues.
2. **Séparation IHM / audio** : l'IHM ne touche jamais l'état du moteur ; elle lit une télémétrie
   publiée (compteurs, positions) et envoie des commandes par file. *(État actuel : la télémétrie
   atomique IHM ↔ audio reste à faire — voir l'avertissement dans `include/svp/metering/Meter.h`.)*
3. **Chargement et retrait de plugins sûrs** : un plugin est instancié et préparé hors du thread
   audio, activé par publication atomique, et détruit hors du thread audio après que le moteur a
   cessé de le référencer.
4. **Aucun reverse engineering** : nous n'analysons, ne décompilons ni ne reproduisons aucun logiciel
   propriétaire. Toutes les API utilisées sont publiques et documentées.
5. **Honnêteté de l'état** : tout code Windows écrit sans être exécuté est livré marqué
   « non vérifié sur Windows » ; rien n'est déclaré testé sans sortie de compilation et de test.

## 5. État des licences (sources datées)

| Composant | Licence constatée | Source (date de consultation) | Conséquence pour nous |
|---|---|---|---|
| Moteur `svp_core` (dont JSON et format de projet) | Écrit par nous, zéro dépendance | ce dépôt | Aucune contrainte tierce sur le socle |
| **WASAPI** (Windows Audio Session API) | API publique Microsoft, fournie avec le Windows SDK ; pas de redevance | documentation Microsoft Learn « Core Audio APIs » ; en-têtes `audioclient.h` / `mmdeviceapi.h` du Windows SDK (API publique) | Aucun SDK à télécharger, aucun accord à accepter, rien à redistribuer séparément. Voie audio **principale** |
| **SDK ASIO** (Steinberg) | SDK sous licence propriétaire Steinberg, **non redistribuable** ; Steinberg propose par ailleurs une variante « open source » sous licence GNU | portail développeurs Steinberg `https://www.steinberg.net/developers/`, consulté le 16/10/2026 | Chaque poste de compilation télécharge le SDK et accepte l'accord. Le SDK **n'entre pas** dans ce dépôt et n'est pas livré avec le produit. Support ASIO = option de compilation |
| **SDK VST3** (Steinberg) | **MIT** (« MIT License, Copyright (c) 2026, Steinberg Media Technologies GmbH ») | `https://github.com/steinbergmedia/vst3sdk/blob/master/LICENSE.txt`, consulté le 16/10/2026 | Utilisable avec mention de copyright. Les « VST3 Usage Guidelines » (marque, nom du produit) restent à respecter. Arrive en v0.4 |
| **Direct2D / DirectWrite** | API publiques Microsoft, fournies avec Windows | documentation Microsoft Learn (Direct2D, DirectWrite) ; en-têtes du Windows SDK | Rendu et texte de l'IHM : aucune dépendance à acquérir |
| **JUCE** *(écarté, voir §6)* | Double licence : **AGPLv3** *ou* **licence commerciale payante** | dépôt officiel JUCE, `LICENSE.md` et `README.md`, constaté le 16/10/2026 | AGPLv3 imposerait de publier nos sources ; la licence commerciale est payante. incompatible avec l'objectif « code fermé vendable sans redevance » |
| Encodeur MP3 (export, v0.4/v0.5) | Hors périmètre aujourd'hui. Options : encodeur de Windows Media Foundation (fourni par le système) ou **LAME** (**LGPL**) | `https://lame.sourceforge.io/license.php` — **non vérifié depuis cette machine**, à confirmer avant usage | Si LAME est lié en bibliothèque partagée, l'application peut rester propriétaire ; une version statique modifiée déclenche les obligations LGPL. Décision à prendre avec le propriétaire |
| Dépendances de test | Aucune (micro-cadre maison `tests/svp_test.hpp`) | ce dépôt | Aucune licence tierce |
| CMake (outil de compilation) | BSD 3-Clauses | cmake.org | Outil de build uniquement, non distribué avec le produit |

Aucun logiciel propriétaire n'a été analysé, décompilé ou reproduit. Aucun SDK tiers n'est présent
dans ce dépôt.

## 6. Option écartée : JUCE — et pourquoi

**Ce qui était envisagé** (version du 16/10/2026 de cet ADR) : utiliser JUCE pour l'IHM et
`AudioDeviceManager` pour l'accès aux périphériques (ASIO en priorité, WASAPI en secours).

**Pourquoi c'est écarté :**

1. **La licence.** Les modules JUCE sont sous double licence : **AGPLv3** *ou* licence commerciale
   payante (source : `LICENSE.md` et `README.md` du dépôt officiel JUCE, constatés le 16/10/2026).
   L'AGPLv3 oblige à publier le code source du logiciel distribué, y compris nos modifications — ce
   n'est pas le produit voulu. La licence commerciale est payante : elle réintroduit exactement la
   redevance de framework que le propriétaire refuse.
2. **L'intention du propriétaire.** La demande est explicite : « une version à nous ». Un cadre
   imposé par un framework tiers, avec ses conventions et son style visuel, va à l'encontre de cette
   demande.
3. **Ce qu'on aurait économisé, et ce qu'on paye à la place.** JUCE aurait fourni l'abstraction des
   pilotes et une boîte à outils d'interface. On écrit à la place une couche WASAPI/ASIO et un rendu
   Direct2D sur Win32. Le surcoût est réel et assumé (plan, révision 3).

**Ce qui reste vrai de JUCE :** c'est un framework compétent, et la voie AGPLv3 **reste ouverte** si
le propriétaire préférait un jour publier les sources plutôt que vendre en code fermé. Ce n'est pas
la décision prise. Aucune ligne de JUCE n'est copiée, dérivée ou replacée sous notre licence : ce
serait une contrefaçon de droits d'auteur, et un produit juridiquement invendable. On ne le fait pas.

## 7. Conséquences et limites assumées

- **Rien de Windows n'est prouvé sur cette machine.** L'environnement de développement est un Ubuntu
  24.04 sans périphérique audio, sans écran et sans compilateur Windows. WASAPI, ASIO, Win32,
  Direct2D/DirectWrite et le `.exe` sont marqués **« à vérifier sur Windows 11 »** ; ils ne seront
  jamais déclarés testés sans compilation et exécution réelles.
- **Le risque principal est l'absence de compilation Windows.** Il est traité comme un chantier à
  part entière : mise en place d'une compilation Windows (chaîne croisée MinGW-w64 sur cette
  machine, ou intégration continue via un dépôt Git) pour que le code Windows compile réellement à
  chaque changement, plutôt que de s'accumuler non vérifié. La chaîne croisée exige l'accord du
  propriétaire (installation système).
- **Le rééchantillonnage linéaire** de la v0.1 sera remplacé par une interpolation à fenêtre
  (polyphase) avant l'enregistrement vocal.
- **Le format de projet** est documenté et versionné (`docs/PROJECT-FORMAT.md`) : champs additifs
  sans changement de version, champ inconnu ignoré avec avertissement, version incrémentée seulement
  si un champ change de sens ou disparaît.
- **Build hors de `/home`** : les répertoires de compilation vont dans `/tmp` (le système de fichiers
  `/home` ne dispose que d'environ 290 Mo, `/tmp` de 3 Go).

## 8. Décisions qui appartiennent encore au propriétaire

1. **Installer la chaîne croisée Windows** (MinGW-w64) sur cette machine, pour compiler réellement
   le code Windows à chaque changement — modification système, donc accord requis.
2. **Fournir un dépôt Git** : permet une compilation Windows automatique qui produit vraiment le
   `.exe`, sans dépendre de son poste.
3. **Accepter la licence du SDK ASIO** sur chaque poste de compilation (le SDK ne peut pas être
   livré avec le dépôt).
4. **Choix de l'encodeur MP3** pour l'export (v0.4/v0.5), et acceptation des obligations LGPL si
   LAME est retenu.
5. **Nom commercial et usage des marques « VST3 » / « ASIO » / « WASAPI »** : à vérifier auprès de
   Steinberg (VST3, ASIO) avant toute communication produit.

## 9. Historique

| Date | Changement |
|---|---|
| 16/10/2026 | Version initiale : moteur isolé + JUCE pour l'IHM et les périphériques audio. |
| 17/10/2026 | **Réalignement sur le plan révision 3** : JUCE écarté (licence AGPLv3 ou commerciale, intention « à nous »). Couche audio maison WASAPI puis ASIO, rendu Win32 + Direct2D/DirectWrite, VST3 (MIT) pour les plugins. Licences et sources datées. |
