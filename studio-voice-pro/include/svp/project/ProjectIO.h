// Studio Voice Pro — module project : entrée/sortie d'un projet.
//   * écriture ATOMIQUE (fichier temporaire puis renommage) : une coupure en
//     pleine écriture ne détruit jamais le projet existant ;
//   * chemins médias RELATIFS au fichier de projet quand c'est possible,
//     repli sur le chemin absolu, et média manquant signalé sans planter ;
//   * préférences par défaut et fichier de sauvegarde automatique distincts.
//
// Règles temps réel : tout ce fichier touche le disque ou alloue — il ne doit
// JAMAIS être appelé depuis le thread audio (voir docs/PROJECT-FORMAT.md §9).
#pragma once
#include <string>
#include <vector>
#include "svp/project/Project.h"

namespace svp::project {

// --- sérialisation pure (aucun accès disque ; testable directement) ---------

/** Sérialise le projet en JSON selon docs/PROJECT-FORMAT.md. `projectDir` est
    le dossier du fichier de projet : c'est lui qui décide si un chemin média
    peut être écrit en relatif. */
[[nodiscard]] std::string toJson(const Project& project, const std::string& projectDir);

struct LoadResult {
    bool ok = false;
    std::string error;                   // message lisible si ok == false
    Project project{};
    std::vector<std::string> warnings;   // clés inconnues, valeurs réparées…
    std::vector<std::string> missingMedia;  // médias introuvables (chemins résolus)
    bool fromAutosave = false;
};

/** Désérialise un document de projet. Ne lève jamais : toute anomalie est
    signalée (`error`) ou journalisée (`warnings`), y compris pour un JSON
    tronqué, un type de champ erroné ou une version de schéma inconnue. */
[[nodiscard]] LoadResult fromJson(const std::string& text, const std::string& projectDir);

// --- chemins médias ---------------------------------------------------------

/** Chemin tel qu'il sera STOCKÉ dans le fichier : relatif (séparateurs « / »)
    si le média est sous le dossier du projet, sinon absolu. */
[[nodiscard]] std::string makeStoredMediaPath(const std::string& mediaPath,
                                              const std::string& projectDir);
/** Chemin tel qu'il sera OUVERT : un chemin relatif est résolu depuis le
    dossier du projet, un chemin absolu est rendu tel quel. */
[[nodiscard]] std::string resolveStoredMediaPath(const std::string& storedPath,
                                                 const std::string& projectDir);

// --- disque -----------------------------------------------------------------

struct SaveOptions {
    /** Écrit en plus un fichier de sauvegarde automatique distinct du projet. */
    bool writeAutosaveCopy = false;
};

struct SaveResult {
    bool ok = false;
    std::string error;
    std::string finalPath;
    std::string tempPath;
    std::string autosavePath;
    std::string json;  // document réellement écrit (utile pour l'affichage)
};

/** Écrit le projet de façon atomique : document complet dans `<projet>.tmp`,
    synchronisation disque, puis renommage sur le fichier final. Le dossier
    parent est créé au besoin. Refuse d'écrire un projet invalide. */
[[nodiscard]] SaveResult saveProject(const Project& project, const std::string& projectPath,
                                     const SaveOptions& options = {});

/** Charge un projet ; les chemins médias relatifs sont résolus depuis le
    dossier du fichier de projet. Un média absent n'empêche pas le chargement. */
[[nodiscard]] LoadResult loadProject(const std::string& projectPath);

/** Rendu texte de l'état d'un projet — utilisé par `svp-project load --dump`.
    Affiche pistes, paramètres, clips, routages et médias manquants. */
[[nodiscard]] std::string dumpProject(const Project& project, const LoadResult& info,
                                      const std::string& projectPath);

// --- sauvegarde automatique (fichier DISTINCT du projet) --------------------

/** `<projet>.autosave.json` : jamais le même fichier que le projet, pour
    qu'une sauvegarde automatique ne puisse pas écraser une sauvegarde
    volontaire. */
[[nodiscard]] std::string autosavePathFor(const std::string& projectPath);
[[nodiscard]] SaveResult saveAutosave(const Project& project, const std::string& projectPath);
[[nodiscard]] LoadResult loadAutosave(const std::string& projectPath);
[[nodiscard]] bool hasAutosave(const std::string& projectPath);
bool removeAutosave(const std::string& projectPath);

// --- préférences ------------------------------------------------------------

/** Entrée de préférences par défaut de l'application (fichier séparé, à
    l'extérieur de tout projet). */
struct Preferences {
    int schemaVersion = 1;
    int defaultSampleRate = 48000;
    int defaultBitDepth = 32;
    double defaultTempoBpm = 120.0;
    bool autosaveEnabled = true;
    int autosaveIntervalSeconds = 120;  // 0 = désactivé
    int autosaveKeepCount = 3;
    std::string defaultProjectDirectory;
    std::string defaultMediaDirectory;
    std::string audioDeviceType = "ASIO";       // ASIO prioritaire, WASAPI en secours
    std::string audioOutputDeviceName = "";     // vide = périphérique par défaut
    int audioBufferSizeFrames = 256;

    /** Vrai si `defaultBitDepth` n'est ni 16, ni 24, ni 32. */
    [[nodiscard]] bool bitDepthInvalid() const noexcept
    {
        return defaultBitDepth != 16 && defaultBitDepth != 24 && defaultBitDepth != 32;
    }
};

[[nodiscard]] Preferences defaultPreferences();

/** Emplacement par défaut : `$SVP_CONFIG_DIR`, sinon `$XDG_CONFIG_HOME`,
    sinon `~/.config`, puis `studio-voice-pro/preferences.json`. */
[[nodiscard]] std::string defaultPreferencesPath();

[[nodiscard]] SaveResult savePreferences(const Preferences& preferences,
                                         const std::string& path);
struct PreferencesLoadResult {
    bool ok = true;          // un fichier absent n'est PAS une erreur
    bool fileExisted = false;
    std::string error;
    std::vector<std::string> warnings;
    Preferences preferences{};
};
[[nodiscard]] PreferencesLoadResult loadPreferences(const std::string& path);
/** Projet vierge construit à partir des préférences (nouvelle session). */
[[nodiscard]] Project newProjectFromPreferences(const Preferences& preferences,
                                                const std::string& name);

// --- crochets de test -------------------------------------------------------
/** Interruption simulée de l'écriture atomique, pour prouver qu'un échec ne
    détruit pas le projet existant (`abortBeforeReplace`) ou qu'une écriture
    de fichier temporaire ratée laisse le disque propre (`failTempWrite`).
    Désactivés par défaut ; jamais activés par l'application. */
struct TestHooks {
    bool abortBeforeReplace = false;
    bool failTempWrite = false;
};
void setProjectIoTestHooks(const TestHooks& hooks);
[[nodiscard]] TestHooks projectIoTestHooks();

} // namespace svp::project
