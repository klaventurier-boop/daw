// Studio Voice Pro — module project : modèle d'un projet (pistes, clips,
// paramètres de piste, routage d'entrée/sortie).
//
// Le format sur disque est décrit dans docs/PROJECT-FORMAT.md. La séparation
// est volontaire : ce fichier ne contient QUE le modèle de données, sans
// entrée/sortie ; `ProjectIO.h` contient la lecture, l'écriture atomique, les
// préférences et la sauvegarde automatique.
//
// Règles temps réel : aucune fonction de ce module n'est appelée depuis le
// thread audio. Charger ou écrire un projet alloue et touche le disque : cela
// se fait dans un thread de travail, puis le résultat est publié au moteur par
// échange de pointeur (le détail viendra avec l'application native).
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace svp::project {

/** Version du schéma du fichier de projet. À incrémenter dès qu'un champ
    change de sens ou disparaît ; ajouter un champ optionnel ne change PAS la
    version (règle de compatibilité : docs/PROJECT-FORMAT.md §6). */
inline constexpr int kSchemaVersion = 1;
/** Identifiant du document, écrit en tête de fichier. */
inline constexpr const char* kSchemaName = "studio-voice-pro.project";
/** Extension recommandée : `ma-session.svpproj.json`. */
inline constexpr const char* kProjectExtension = ".svpproj.json";

/** Paramètres de piste. Tous les champs sont exactement ceux du mixer v0.1 :
    gain en dBFS (0 = neutre), panoramique -1 (gauche) … +1 (droite), mute et
    solo. */
struct TrackParams {
    double gainDb = 0.0;
    double pan = 0.0;
    bool mute = false;
    bool solo = false;
};

/** Un clip = une référence à un média, posée sur la timeline d'une piste.
    `lengthFrames == 0` est la convention « durée du média », résolue au
    chargement si le fichier est lisible (voir docs/PROJECT-FORMAT.md §4). */
struct Clip {
    std::string id;         // « t1c1 » : déterministe, donc diff stable
    std::string name;       // nom affiché ; vide = nom du fichier média
    std::string mediaPath;  // chemin RÉSOLU, utilisable pour ouvrir le média
    /** Forme EXACTE du chemin média telle qu'elle était écrite dans le fichier
        (« media/voix.wav », « /mnt/…/hors-projet.wav »…), mémorisée au
        chargement. `toJson` la restitue telle quelle tant qu'elle résout
        toujours vers `mediaPath` : c'est ce qui rend « sauver → recharger →
        resauver » identique octet pour octet. Vide pour un clip créé en
        mémoire : la forme stockée est alors recalculée depuis `mediaPath`.
        Voir docs/PROJECT-FORMAT.md §5.2 et §8. */
    std::string storedMediaPath;
    int64_t startFrames = 0;         // position du début sur la timeline
    int64_t sourceOffsetFrames = 0;  // décalage dans le média
    int64_t lengthFrames = 0;        // 0 = jusqu'à la fin du média
    double gainDb = 0.0;
    double fadeInSeconds = 0.0;
    double fadeOutSeconds = 0.0;
    /** Renseigné au CHARGEMENT uniquement (jamais écrit dans le fichier) :
        le média référencé est introuvable. Le clip reste chargé. */
    bool mediaMissing = false;
};

struct Track {
    std::string id;                    // « t1 », « t2 »… (déterministe)
    std::string name;
    std::string color = "#5AA9E6";     // couleur d'affichage (IHM v0.2)
    std::string input = "none";        // routage d'entrée (mixer / v0.4)
    std::string output = "master";     // routage de sortie : bus ou piste
    TrackParams params{};
    std::vector<Clip> clips;
};

struct Project {
    int schemaVersion = kSchemaVersion;
    std::string appVersion = "0.2.0";  // version du logiciel qui a écrit le fichier
    std::string name = "Sans titre";
    int sampleRate = 48000;  // Hz, entier (jamais arrondi en texte)
    int bitDepth = 32;       // 16, 24 ou 32 (32 = flottant)
    double tempoBpm = 120.0;
    int64_t lengthFrames = 0;  // longueur de session ; 0 = calculée au chargement
    std::vector<Track> tracks;
};

// --- comparaisons exactes (aucune tolérance : la fidélité est binaire) --------
bool operator==(const TrackParams& a, const TrackParams& b) noexcept;
bool operator==(const Clip& a, const Clip& b) noexcept;
bool operator==(const Track& a, const Track& b) noexcept;
bool operator==(const Project& a, const Project& b) noexcept;
inline bool operator!=(const Project& a, const Project& b) noexcept { return !(a == b); }
inline bool operator!=(const TrackParams& a, const TrackParams& b) noexcept { return !(a == b); }
inline bool operator!=(const Clip& a, const Clip& b) noexcept { return !(a == b); }
inline bool operator!=(const Track& a, const Track& b) noexcept { return !(a == b); }

/** Différence structurée entre deux projets — sert aux tests de fidélité pour
    nommer le champ fautif au lieu d'afficher « objets différents ». Renvoie une
    chaîne vide si les projets sont identiques. */
[[nodiscard]] std::string firstDifference(const Project& a, const Project& b);

/** Contrôles avant écriture : entier de version connu, taux d'échantillonnage
    et profondeur plausibles, valeurs finies, identifiants non vides et
    uniques, positions non négatives. Renvoie false et remplit `error`. */
[[nodiscard]] bool validateProject(const Project& project, std::string& error);

[[nodiscard]] std::size_t clipCount(const Project& project) noexcept;
/** Fin du dernier clip, ou `lengthFrames` s'il est plus grand. */
[[nodiscard]] int64_t projectEndFrames(const Project& project) noexcept;

/** Identifiants déterministes (« t1 », « t1c1 ») : deux constructions
    identiques produisent deux fichiers identiques, ce qui rend le `diff`
    exploitable et les tests reproductibles. */
[[nodiscard]] std::string makeTrackId(std::size_t trackIndex);
[[nodiscard]] std::string makeClipId(std::size_t trackIndex, std::size_t clipIndex);

/** Nouveau projet vierge aux paramètres donnés. */
[[nodiscard]] Project makeProject(const std::string& name, int sampleRate, int bitDepth,
                                  double tempoBpm = 120.0);

/** Piste vierge, avec identifiant tiré de son futur index (`tracks.size()`). */
[[nodiscard]] Track makeTrack(const Project& project, const std::string& name);

} // namespace svp::project
