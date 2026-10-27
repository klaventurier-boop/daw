# Compiler et tester Studio Voice Pro sur Windows 11

> **Document NON TESTÉ.** Cette procédure est écrite depuis une machine Ubuntu qui ne
> dispose ni de compilateur Windows, ni de périphérique audio, ni d'écran. Rien de ce qui
> suit n'a été exécuté. Elle décrit ce qu'il faudra **exactement** sur un poste Windows 11 ;
> chaque étape devra être vérifiée et corrigée là-bas avant d'être déclarée fiable.

## 1. Ce qui doit être présent sur le poste

| Élément | Version / rôle | Source | Précautions |
|---|---|---|---|
| **Visual Studio 2022 Build Tools** | charge de travail « Développement Desktop en C++ » (MSVC v143, Windows 11 SDK) | microsoft.com (téléchargement officiel) | pas besoin de l'IDE complet ; les Build Tools suffisent |
| **CMake** | 3.20 ou plus récent | cmake.org | cocher « Add to PATH » |
| **Git** | toute version récente | git-scm.com | nécessaire pour cloner le dépôt du projet |
| **Windows SDK** | fourni avec Visual Studio (Windows 11 SDK) | microsoft.com | contient WASAPI (`mmdeviceapi.h`, `audioclient.h`), Direct2D et DirectWrite : **aucun SDK séparé à télécharger** pour l'audio ni pour l'interface |
| **Aucun framework d'IHM** | — | — | l'interface est écrite maison sur Win32 + Direct2D/DirectWrite, et la couche audio sur WASAPI/ASIO : **aucune bibliothèque tierce à acquérir, aucune redevance** (voir `docs/ADR.md`) |
| **SDK ASIO** | requis pour compiler la sortie ASIO (optionnelle) | à télécharger **soi-même** sur le portail développeurs Steinberg (`steinberg.net/developers/`) et à placer dans le projet | **ne pas redistribuer le SDK** ; l'accord de licence Steinberg doit être accepté sur chaque poste. Sans le SDK, la couche ASIO n'est pas compilée : WASAPI reste disponible |
| **SDK VST3** | v0.4+ (hôte de plugins) | dépôt officiel `steinbergmedia/vst3sdk` (licence MIT) | respecter les « VST3 Usage Guidelines » (marque) |

À ne **pas** installer : aucun pilote ASIO tiers n'est embarqué ; c'est celui de la carte son
qui est utilisé à l'exécution.

## 2. Moteur (sans IHM) — compilation et tests

Le moteur ne dépend d'aucun SDK Windows : c'est le même code que celui validé sur Linux.

```powershell
cd C:\dev\studio-voice-pro
cmake -S . -B C:\build\svp -G "Visual Studio 17 2022" -A x64
cmake --build C:\build\svp --config Release
ctest --test-dir C:\build\svp -C Release --output-on-failure
```

Sous Windows, le chemin de build est libre (la contrainte `/tmp` de la machine de
développement ne s'applique pas). Les chemins de fichiers WAV de test sont fournis par la
variable d'environnement `SVP_TEST_TMPDIR`, positionnée automatiquement par CMake.

Résultat attendu : 7 suites, 0 échec (mêmes tests que sur Linux).

## 3. Application native (v0.2, pas encore écrite)

Étapes prévues, à écrire dans un `CMakeLists.txt` séparé (par exemple `app/`) — **aucune n'est
encore exécutée, rien n'existe à ce jour dans le dépôt** :

1. Cibles distinctes : `svp_win_audio` (WASAPI d'abord, ASIO ensuite, API COM Win32) et `svp_app`
   (fenêtre Win32 + rendu Direct2D/DirectWrite), toutes deux liées à `svp_core`.
2. Lier les bibliothèques système `ole32`, `uuid`, `d2d1`, `dwrite`, `dwmapi` (et `avrt` pour le
   thread audio) : elles viennent du Windows SDK, aucune n'est à télécharger.
3. Compiler en `Release`, lancer l'application, choisir un périphérique WASAPI en mode exclusif,
   puis vérifier : ouverture/fermeture sans plantage, latence annoncée, absence de craquement à
   128 échantillons, bascule vers le périphérique par défaut si celui choisi disparaît. Recommencer
   avec ASIO si le SDK ASIO a été téléchargé sur le poste.

## 4. À vérifier obligatoirement sur le poste Windows 11

- [ ] `svp_tests` : 7 suites, 0 échec (comparer avec la sortie Linux du `README.md`).
- [ ] `svp-render --in <wav> --out <wav> --gain -6 --pan -0.3 --meter` produit les mêmes
      valeurs qu'ici (RMS avant −3,010 dBFS pour un sinus pleine échelle, après −9,010 dBFS).
- [ ] Écoute réelle du fichier de sortie (aucun test audio réel n'a été fait à ce jour).
- [ ] Lecture d'un WAV produit par un autre logiciel (Reaper, Audacity) et inversement.
- [ ] Choix ASIO puis WASAPI, avec latence et absence de clics.
- [ ] Comportement si la carte son est débranchée pendant la lecture (repli, pas de plantage).
- [ ] Performances du callback audio : aucune allocation, aucun accès disque
      (à instrumenter : compteur d'allocations sur le thread audio).

## 5. Points d'attention connus

- **SDK ASIO** : s'il est absent, la couche ASIO n'est simplement pas compilée (WASAPI reste
  disponible). Le SDK ne peut pas être livré avec le dépôt : chaque poste le télécharge.
- **Aucune licence de framework** : l'interface (Win32 + Direct2D/DirectWrite) et la couche audio
  (WASAPI) reposent sur des API publiques de Windows, sans redevance ni obligation de publier nos
  sources. Le seul composant tiers à licence contraignante est le SDK ASIO, non redistribuable.
- **Définition d'en-têtes `min`/`max`** de `windows.h` : le moteur compile sans problème
  (`NOMINMAX` conseillé côté application).
- **Chemins accentués** : `svp-render` ouvre les fichiers via `std::ifstream` avec un
  `std::string` étroit ; sur Windows, un chemin contenant des caractères non-ASCII peut
  échouer. À corriger en v0.2 (passage à `std::filesystem::path` et `_wfopen` si besoin).
