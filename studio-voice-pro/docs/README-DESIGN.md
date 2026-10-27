# Studio Voice Pro — dossier de design : par où commencer

Version 1 (2026-10-17, designer). Interface native Windows 11 (Win32 + Direct2D /
DirectWrite), pas une application web.

## Ordre de lecture

1. **`STYLE-GUIDE.md`** — la référence unique : palette hexadécimale, contrastes mesurés,
   typographie (Segoe UI Variable / Consolas, chiffres tabulaires), échelle d'espacement,
   rayons, traits, états des contrôles, spécification du vumètre. C'est le document à
   appliquer ; en cas de désaccord avec une maquette, **le cahier de style gagne**.
2. **`LAYOUT-SPEC.md`** — les zones en rectangles, leurs minima et le comportement au
   redimensionnement. À lire avant d'écrire le code de mise en page.
3. **`maquettes/`** — trois images de référence :
   - `01-fenetre-principale-1440x900.png` — fenêtre complète : navigateur, liste de 8
     pistes, timeline avec clips et formes d'onde, console intégrée en bas, rack d'effets,
     barre d'état.
   - `02-console-mixage-1440x900.png` — console en pleine hauteur : 8 pistes, 2 bus,
     master, inserts et envois par tranche.
   - `03-detail-transport-tranche-vumetre.png` — planche de détail : barre de transport
     (×2), tranche de canal (×2), vumètre avec témoin d'écrêtage (×4), table des états,
     règle d'infobulle.
4. **`maquettes/sources/`** — les sources HTML/CSS qui ont produit les PNG. Ce ne sont
   **pas** l'implémentation : c'est un véhicule de dessin qui permet de régénérer ou de
   retoucher une maquette sans repartir de zéro. Le produit reste du C++ natif.

## Ce qui est réellement prévu dans ces maquettes

Barre de transport (lecture/pause, arrêt, retour au début, enregistrement, boucle,
métronome), position, mesures, tempo, signature ; liste de pistes avec nom, gain, pan,
mute, solo, armement ; clips avec en-têtes, fondus et formes d'onde ; vumètres crête + RMS
avec maintien de crête et témoin d'écrêtage maintenu ; console de mixage (pistes, bus,
master, inserts, envois) ; rack d'effets par piste ; navigateur de fichiers ; barre d'état
moteur/format/CPU.

## Ce qui est décoratif ou simulé — donc PAS encore implémenté

Les points suivants sont dessinés pour donner la bonne densité à l'écran, mais **aucune
donnée réelle ne les alimente aujourd'hui** ; ils ne doivent pas être codés sans une tâche
explicite :

1. **Les formes d'onde** sont générées par un algorithme de dessin déterministe, pas
   lues dans un fichier. L'implémentation devra calculer des enveloppes min/max par
   colonne de pixels à partir des tampons audio.
2. **Les niveaux des vumètres, les valeurs P/R et le témoin d'écrêtage allumé** sont des
   valeurs figées d'exemple. Le moteur fournit déjà la mesure crête et RMS ; le témoin
   maintenu jusqu'au clic est une règle d'IHM à implémenter.
3. **Le contenu de session** (noms de pistes, fichiers, effets « Égaliseur vocal »,
   « Compresseur », « Dé-esseur », « Réverbération », « Limiteur de sortie ») est un
   exemple de projet. Les effets vocaux et le limiteur sont **prévus au plan** mais pas
   développés ; l'écran ne doit pas laisser croire qu'ils existent tant qu'ils n'existent
   pas : à l'implémentation, une chaîne vide affiche « emplacement libre » et les modules
   indisponibles doivent être grisés ou absents.
4. **Les sections Envois, Entrée & monitoring et le bouton de réglages audio** décrivent
   des réglages prévus (routage, choix de périphérique, gain d'entrée) ; le routage
   d'envois et le monitoring ne sont pas encore dans le moteur.
5. **Le bloc « valeur au survol (règle) » dans le rack de la maquette 1** est une note de
   spécification dessinée à l'écran : ce n'est pas un élément d'interface du produit. Il
   ne doit pas être implémenté — la règle, elle, s'applique (cf. STYLE-GUIDE §5).
6. **Les boutons de fenêtre**, la marque « SVP » et l'indicateur de projet « non
   enregistré » sont des éléments d'habillage ; la logique de sauvegarde n'existe pas
   encore.
7. **Le format d'affichage** (WASAPI exclusif, 48 000 Hz, 256 échantillons, 8,2 ms) est un
   exemple : la couche audio Windows n'est pas écrite, rien de tout cela n'est mesuré.

## Écarts et points à trancher par le propriétaire

- **Couleur d'accent** : le jade `#37D2B2` est proposé. Deux variantes existent :
  ambre `#E9A94C` (plus « matériel de studio ») et azur `#5AA6F0`. Une seule doit être
  retenue : elle colore la sélection, le focus et la tête de lecture.
- **Densité** : hauteur de piste 56 px (confortable) ou 40 px (compact, ~30 % de pistes en
  plus à l'écran). Les deux sont spécifiées ; la valeur par défaut reste à choisir.
- **Couleurs de piste** : six teintes proposées. Si le propriétaire préfère une interface
  strictement monochrome hors états, on retire la palette de pistes et on garde le liseré
  en `text-secondary`.
- **Placement de la console** : intégrée en bas de la fenêtre principale (maquette 1) ou
  dans une seconde fenêtre détachable. La maquette 2 montre la vue pleine hauteur.
- Le document `docs/ADR.md` de l'équipe décrit encore JUCE pour l'IHM ; le plan de
  référence (révision 3) a retenu une IHM native sans JUCE. **Ce cahier de style est écrit
  pour une IHM native** (Direct2D / DirectWrite) : l'ADR est à réaligner.

## Limites de ce dossier

- Maquettes produites hors Windows : la police de rendu est la police de secours du poste
  de production (Liberation Sans). Sur Windows 11, Segoe UI Variable Text/Display et
  Consolas ont des métriques légèrement différentes (largeur des glyphes, hauteur de
  ligne) : les **tailles et positions** restent valables, le rendu exact du texte doit être
  revérifié sur la machine cible.
- Aucune maquette d'état d'enregistrement en cours, de dialogue d'export ni de fenêtre de
  préférences : elles restent à produire.
