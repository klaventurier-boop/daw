// Studio Voice Pro — svp-project : fabriquer, sauver, recharger et inspecter un
// projet depuis la ligne de commande, sans aucune IHM.
//
// Cycle complet :
//   svp-project new --project /tmp/demo/session.svpproj.json --name "Ma session" --rate 48000 --bits 32
//   svp-project add-track --project ... --name "Voix" --gain -6 --pan -0.3 --output master
//   svp-project add-clip  --project ... --track 1 --file voix.wav --at 0 --length 96000
//   svp-project set       --project ... --track 1 --gain -6 --pan -0.3 --mute
//   svp-project save      --project ... [--autosave]
//   svp-project load      --project ... --dump
//
// Chaque commande qui modifie le projet recharge le fichier, applique la
// modification puis réécrit le tout de façon atomique : le fichier sur disque
// est toujours un projet complet et lisible.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "svp/io/WavFile.h"
#include "svp/project/Project.h"
#include "svp/project/ProjectIO.h"

namespace {

namespace fs = std::filesystem;

struct Args {
    std::string command;
    std::vector<std::pair<std::string, std::string>> options;

    [[nodiscard]] bool has(const std::string& key) const
    {
        for (const auto& option : options) {
            if (option.first == key)
                return true;
        }
        return false;
    }
    [[nodiscard]] std::optional<std::string> get(const std::string& key) const
    {
        for (const auto& option : options) {
            if (option.first == key)
                return option.second;
        }
        return std::nullopt;
    }
    /** Valeur d'un drapeau : « true » si le drapeau est présent sans valeur. */
    [[nodiscard]] bool flag(const std::string& key) const
    {
        const std::optional<std::string> value = get(key);
        if (!value)
            return false;
        if (*value == "false" || *value == "0" || *value == "non" || *value == "no")
            return false;
        return true;
    }
    [[nodiscard]] std::string text(const std::string& key, const std::string& fallback = "") const
    {
        const std::optional<std::string> value = get(key);
        return value ? *value : fallback;
    }
};

void printUsage()
{
    std::printf(
        "svp-project 0.2 — projets Studio Voice Pro (format JSON documenté :\n"
        "              docs/PROJECT-FORMAT.md)\n"
        "\n"
        "  svp-project <commande> --project <fichier.svpproj.json> [options]\n"
        "\n"
        "Commandes :\n"
        "  new        crée un projet vierge (--name, --rate, --bits, --tempo, --force)\n"
        "  add-track  ajoute une piste (--name, --gain, --pan, --mute, --solo, --input,\n"
        "             --output, --color)\n"
        "  add-clip   ajoute un clip (--track N, --file, --name, --at, --offset,\n"
        "             --length, --gain, --fade-in, --fade-out)\n"
        "  set        modifie un projet ou une piste (--track N, --gain, --pan, --mute,\n"
        "             --no-mute, --solo, --no-solo, --input, --output, --track-name,\n"
        "             --color ; sans --track : --name, --rate, --bits, --tempo, --length)\n"
        "  save       réécrit le projet de façon atomique (--autosave pour écrire en\n"
        "             plus <projet>.autosave.json)\n"
        "  load       recharge le projet et affiche son état (--dump, --autosave)\n"
        "  help       cette aide\n"
        "\n"
        "Les pistes sont numérotées à partir de 1. Les positions sont en échantillons.\n"
        "Un média introuvable n'empêche pas l'ajout : le clip est signalé au chargement.\n"
        "Aucune commande n'écrit jamais directement dans le fichier du projet : le\n"
        "document complet part dans <projet>.tmp puis est renommé (écriture atomique).\n");
}

bool parseArgs(int argc, char** argv, Args& args, std::string& error)
{
    if (argc < 2) {
        error = "commande manquante";
        return false;
    }
    args.command = argv[1];
    for (int i = 2; i < argc; ++i) {
        const std::string token = argv[i];
        if (token.rfind("--", 0) != 0) {
            error = "argument inattendu : « " + token + " »";
            return false;
        }
        std::string key = token.substr(2);
        std::string value = "true";
        if (i + 1 < argc && std::strncmp(argv[i + 1], "--", 2) != 0) {
            value = argv[i + 1];
            ++i;
        }
        args.options.emplace_back(std::move(key), std::move(value));
    }
    return true;
}

bool parseDoubleArg(const Args& args, const std::string& key, double& out, std::string& error)
{
    const std::optional<std::string> text = args.get(key);
    if (!text)
        return true;
    try {
        std::size_t consumed = 0;
        out = std::stod(*text, &consumed);
        if (consumed != text->size()) {
            error = "--" + key + " : valeur numérique attendue, reçu « " + *text + " »";
            return false;
        }
    } catch (const std::exception&) {
        error = "--" + key + " : valeur numérique attendue, reçu « " + *text + " »";
        return false;
    }
    return true;
}

bool parseIntArg(const Args& args, const std::string& key, int64_t& out, std::string& error)
{
    const std::optional<std::string> text = args.get(key);
    if (!text)
        return true;
    try {
        std::size_t consumed = 0;
        out = std::stoll(*text, &consumed);
        if (consumed != text->size()) {
            error = "--" + key + " : entier attendu, reçu « " + *text + " »";
            return false;
        }
    } catch (const std::exception&) {
        error = "--" + key + " : entier attendu, reçu « " + *text + " »";
        return false;
    }
    return true;
}

/** Projet du disque, ou projet vierge si le fichier n'existe pas encore (les
    commandes de modification créent ainsi la session au premier appel). */
bool openOrCreate(const std::string& path, svp::project::Project& out, bool& created,
                  std::string& error)
{
    created = false;
    std::error_code ec;
    if (fs::exists(fs::path(path), ec)) {
        svp::project::LoadResult loaded = svp::project::loadProject(path);
        if (!loaded.ok) {
            error = loaded.error;
            return false;
        }
        if (!loaded.warnings.empty()) {
            std::fprintf(stderr, "avertissement au chargement :\n");
            for (const std::string& warning : loaded.warnings)
                std::fprintf(stderr, "  - %s\n", warning.c_str());
        }
        out = std::move(loaded.project);
        return true;
    }
    created = true;
    const svp::project::PreferencesLoadResult preferences =
        svp::project::loadPreferences(svp::project::defaultPreferencesPath());
    out = svp::project::newProjectFromPreferences(preferences.preferences,
                                                  fs::path(path).stem().string());
    return true;
}

void reportSave(const svp::project::SaveResult& saved)
{
    if (!saved.ok) {
        std::fprintf(stderr, "ERREUR : %s\n", saved.error.c_str());
        return;
    }
    std::printf("projet enregistré : %s\n", saved.finalPath.c_str());
    if (!saved.tempPath.empty())
        std::printf("  écriture atomique via %s puis renommage\n", saved.tempPath.c_str());
    if (!saved.autosavePath.empty())
        std::printf("  sauvegarde automatique : %s\n", saved.autosavePath.c_str());
    if (!saved.error.empty())
        std::fprintf(stderr, "avertissement : %s\n", saved.error.c_str());
}

svp::project::Track* findTrack(svp::project::Project& project, int64_t index1Based,
                               std::string& error)
{
    if (index1Based < 1 || static_cast<std::size_t>(index1Based) > project.tracks.size()) {
        error = "piste " + std::to_string(index1Based) + " inexistante (le projet en compte "
                + std::to_string(project.tracks.size()) + ")";
        return nullptr;
    }
    return &project.tracks[static_cast<std::size_t>(index1Based) - 1];
}

// ------------------------------------------------------------------ commandes

int commandNew(const Args& args)
{
    const std::string path = args.text("project");
    if (path.empty()) {
        std::fprintf(stderr, "ERREUR : --project <fichier> obligatoire\n");
        return 1;
    }
    std::error_code ec;
    if (fs::exists(fs::path(path), ec) && !args.flag("force")) {
        std::fprintf(stderr,
                     "ERREUR : « %s » existe déjà (utiliser --force pour l'écraser)\n",
                     path.c_str());
        return 1;
    }
    const svp::project::PreferencesLoadResult preferences =
        svp::project::loadPreferences(svp::project::defaultPreferencesPath());
    svp::project::Project project =
        svp::project::newProjectFromPreferences(preferences.preferences, args.text("name", "Sans titre"));
    if (args.has("name"))
        project.name = args.text("name");
    int64_t rate = project.sampleRate;
    int64_t bits = project.bitDepth;
    double tempo = project.tempoBpm;
    std::string error;
    if (!parseIntArg(args, "rate", rate, error) || !parseIntArg(args, "bits", bits, error)
        || !parseDoubleArg(args, "tempo", tempo, error)) {
        std::fprintf(stderr, "ERREUR : %s\n", error.c_str());
        return 1;
    }
    project.sampleRate = static_cast<int>(rate);
    project.bitDepth = static_cast<int>(bits);
    project.tempoBpm = tempo;
    const svp::project::SaveResult saved = svp::project::saveProject(project, path);
    if (!saved.ok) {
        std::fprintf(stderr, "ERREUR : %s\n", saved.error.c_str());
        return 1;
    }
    std::printf("nouveau projet « %s » : %s\n", project.name.c_str(), saved.finalPath.c_str());
    std::printf("  %d Hz, %d bits, %.3f bpm, %zu piste(s)\n", project.sampleRate, project.bitDepth,
                project.tempoBpm, project.tracks.size());
    return 0;
}

int commandAddTrack(const Args& args)
{
    const std::string path = args.text("project");
    if (path.empty()) {
        std::fprintf(stderr, "ERREUR : --project <fichier> obligatoire\n");
        return 1;
    }
    svp::project::Project project;
    bool created = false;
    std::string error;
    if (!openOrCreate(path, project, created, error)) {
        std::fprintf(stderr, "ERREUR : %s\n", error.c_str());
        return 1;
    }
    if (created)
        std::printf("projet absent : nouvelle session « %s » créée\n", project.name.c_str());
    svp::project::Track track = svp::project::makeTrack(project, args.text("name"));
    if (args.has("gain") && !parseDoubleArg(args, "gain", track.params.gainDb, error)) {
        std::fprintf(stderr, "ERREUR : %s\n", error.c_str());
        return 1;
    }
    if (args.has("pan") && !parseDoubleArg(args, "pan", track.params.pan, error)) {
        std::fprintf(stderr, "ERREUR : %s\n", error.c_str());
        return 1;
    }
    track.params.mute = args.flag("mute");
    track.params.solo = args.flag("solo");
    if (args.has("input"))
        track.input = args.text("input");
    if (args.has("output"))
        track.output = args.text("output");
    if (args.has("color"))
        track.color = args.text("color");
    project.tracks.push_back(track);
    const svp::project::SaveResult saved = svp::project::saveProject(project, path);
    if (!saved.ok) {
        std::fprintf(stderr, "ERREUR : %s\n", saved.error.c_str());
        return 1;
    }
    std::printf("piste %zu « %s » ajoutée (gain %.3f dB, pan %.2f, mute %s, solo %s)\n",
                project.tracks.size(), track.name.c_str(), track.params.gainDb, track.params.pan,
                track.params.mute ? "oui" : "non", track.params.solo ? "oui" : "non");
    reportSave(saved);
    return 0;
}

/** Durée d'un média, lue depuis son en-tête WAV. Au-delà de 64 Mio on
    n'ouvre pas le fichier : le champ reste à 0 (« durée du média »), résolu
    par le moteur au moment de la lecture. */
bool mediaFrameCount(const std::string& mediaPath, int64_t& frames, std::string& note)
{
    std::error_code ec;
    const auto size = fs::file_size(fs::path(mediaPath), ec);
    if (!ec && size > 64ull * 1024ull * 1024ull) {
        note = "média > 64 Mio : longueur laissée à 0 (durée du média)";
        return false;
    }
    const svp::io::WavReadResult read = svp::io::readWavFile(mediaPath);
    if (!read.ok) {
        note = "en-tête WAV illisible (" + read.error + ") : longueur laissée à 0";
        return false;
    }
    frames = read.buffer.frames();
    return true;
}

int commandAddClip(const Args& args)
{
    const std::string path = args.text("project");
    if (path.empty()) {
        std::fprintf(stderr, "ERREUR : --project <fichier> obligatoire\n");
        return 1;
    }
    const std::string media = args.text("file");
    if (media.empty()) {
        std::fprintf(stderr, "ERREUR : --file <média.wav> obligatoire\n");
        return 1;
    }
    int64_t trackIndex = 1;
    std::string error;
    if (!parseIntArg(args, "track", trackIndex, error)) {
        std::fprintf(stderr, "ERREUR : %s\n", error.c_str());
        return 1;
    }
    svp::project::Project project;
    bool created = false;
    if (!openOrCreate(path, project, created, error)) {
        std::fprintf(stderr, "ERREUR : %s\n", error.c_str());
        return 1;
    }
    if (created)
        std::printf("projet absent : nouvelle session « %s » créée\n", project.name.c_str());
    svp::project::Track* track = findTrack(project, trackIndex, error);
    if (track == nullptr) {
        std::fprintf(stderr, "ERREUR : %s\n", error.c_str());
        return 1;
    }
    svp::project::Clip clip;
    clip.id = svp::project::makeClipId(static_cast<std::size_t>(trackIndex) - 1, track->clips.size());
    clip.mediaPath = fs::absolute(fs::path(media)).lexically_normal().string();
    clip.name = args.text("name");
    if (clip.name.empty())
        clip.name = fs::path(media).filename().string();
    if (!parseIntArg(args, "at", clip.startFrames, error)
        || !parseIntArg(args, "offset", clip.sourceOffsetFrames, error)
        || !parseDoubleArg(args, "gain", clip.gainDb, error)
        || !parseDoubleArg(args, "fade-in", clip.fadeInSeconds, error)
        || !parseDoubleArg(args, "fade-out", clip.fadeOutSeconds, error)) {
        std::fprintf(stderr, "ERREUR : %s\n", error.c_str());
        return 1;
    }
    if (args.has("length")) {
        if (!parseIntArg(args, "length", clip.lengthFrames, error)) {
            std::fprintf(stderr, "ERREUR : %s\n", error.c_str());
            return 1;
        }
    } else {
        int64_t frames = 0;
        std::string note;
        std::error_code ec;
        if (!fs::exists(fs::path(clip.mediaPath), ec)) {
            std::fprintf(stderr,
                         "avertissement : média introuvable « %s » — le clip est ajouté et sera "
                         "signalé comme manquant au chargement\n",
                         clip.mediaPath.c_str());
        } else if (mediaFrameCount(clip.mediaPath, frames, note)) {
            clip.lengthFrames = frames > clip.sourceOffsetFrames ? frames - clip.sourceOffsetFrames : 0;
            std::printf("longueur déduite de l'en-tête WAV : %lld échantillons\n",
                        static_cast<long long>(clip.lengthFrames));
        } else {
            std::fprintf(stderr, "avertissement : %s\n", note.c_str());
        }
    }
    track->clips.push_back(clip);
    const svp::project::SaveResult saved = svp::project::saveProject(project, path);
    if (!saved.ok) {
        std::fprintf(stderr, "ERREUR : %s\n", saved.error.c_str());
        return 1;
    }
    std::printf("clip %s « %s » ajouté sur la piste %lld\n", clip.id.c_str(), clip.name.c_str(),
                static_cast<long long>(trackIndex));
    reportSave(saved);
    return 0;
}

int commandSet(const Args& args)
{
    const std::string path = args.text("project");
    if (path.empty()) {
        std::fprintf(stderr, "ERREUR : --project <fichier> obligatoire\n");
        return 1;
    }
    svp::project::Project project;
    bool created = false;
    std::string error;
    if (!openOrCreate(path, project, created, error)) {
        std::fprintf(stderr, "ERREUR : %s\n", error.c_str());
        return 1;
    }
    if (created) {
        std::fprintf(stderr, "ERREUR : « %s » n'existe pas (créer d'abord avec `new`)\n",
                     path.c_str());
        return 1;
    }
    if (args.has("track")) {
        int64_t trackIndex = 1;
        if (!parseIntArg(args, "track", trackIndex, error)) {
            std::fprintf(stderr, "ERREUR : %s\n", error.c_str());
            return 1;
        }
        svp::project::Track* track = findTrack(project, trackIndex, error);
        if (track == nullptr) {
            std::fprintf(stderr, "ERREUR : %s\n", error.c_str());
            return 1;
        }
        if (!parseDoubleArg(args, "gain", track->params.gainDb, error)
            || !parseDoubleArg(args, "pan", track->params.pan, error)) {
            std::fprintf(stderr, "ERREUR : %s\n", error.c_str());
            return 1;
        }
        if (args.has("mute"))
            track->params.mute = args.flag("mute");
        if (args.has("no-mute"))
            track->params.mute = false;
        if (args.has("solo"))
            track->params.solo = args.flag("solo");
        if (args.has("no-solo"))
            track->params.solo = false;
        if (args.has("input"))
            track->input = args.text("input");
        if (args.has("output"))
            track->output = args.text("output");
        if (args.has("track-name"))
            track->name = args.text("track-name");
        if (args.has("color"))
            track->color = args.text("color");
        std::printf("piste %lld mise à jour : gain %.3f dB, pan %.2f, mute %s, solo %s\n",
                    static_cast<long long>(trackIndex), track->params.gainDb, track->params.pan,
                    track->params.mute ? "oui" : "non", track->params.solo ? "oui" : "non");
    } else {
        if (args.has("name"))
            project.name = args.text("name");
        int64_t rate = project.sampleRate;
        int64_t bits = project.bitDepth;
        int64_t length = project.lengthFrames;
        if (!parseIntArg(args, "rate", rate, error) || !parseIntArg(args, "bits", bits, error)
            || !parseIntArg(args, "length", length, error)
            || !parseDoubleArg(args, "tempo", project.tempoBpm, error)) {
            std::fprintf(stderr, "ERREUR : %s\n", error.c_str());
            return 1;
        }
        project.sampleRate = static_cast<int>(rate);
        project.bitDepth = static_cast<int>(bits);
        project.lengthFrames = length;
        std::printf("projet mis à jour : « %s », %d Hz, %d bits, %.3f bpm\n", project.name.c_str(),
                    project.sampleRate, project.bitDepth, project.tempoBpm);
    }
    const svp::project::SaveResult saved = svp::project::saveProject(project, path);
    if (!saved.ok) {
        std::fprintf(stderr, "ERREUR : %s\n", saved.error.c_str());
        return 1;
    }
    reportSave(saved);
    return 0;
}

int commandSave(const Args& args)
{
    const std::string path = args.text("project");
    if (path.empty()) {
        std::fprintf(stderr, "ERREUR : --project <fichier> obligatoire\n");
        return 1;
    }
    svp::project::LoadResult loaded = svp::project::loadProject(path);
    if (!loaded.ok) {
        std::fprintf(stderr, "ERREUR : %s\n", loaded.error.c_str());
        return 1;
    }
    svp::project::SaveOptions options;
    options.writeAutosaveCopy = args.flag("autosave");
    const svp::project::SaveResult saved = svp::project::saveProject(loaded.project, path, options);
    reportSave(saved);
    return saved.ok ? 0 : 1;
}

int commandLoad(const Args& args)
{
    const std::string path = args.text("project");
    if (path.empty()) {
        std::fprintf(stderr, "ERREUR : --project <fichier> obligatoire\n");
        return 1;
    }
    const bool autosave = args.flag("autosave");
    const svp::project::LoadResult loaded =
        autosave ? svp::project::loadAutosave(path) : svp::project::loadProject(path);
    if (!loaded.ok) {
        std::fprintf(stderr, "ERREUR : %s\n", loaded.error.c_str());
        return 1;
    }
    if (!args.has("dump") && !args.has("quiet")) {
        // Sans --dump, l'état est quand même affiché : c'est le seul mode
        // d'inspection dont dispose l'outil (l'option rend l'intention explicite).
    }
    std::printf("%s", svp::project::dumpProject(loaded.project, loaded, path).c_str());
    if (!loaded.missingMedia.empty()) {
        std::fprintf(stderr,
                     "le projet a été chargé malgré %zu média(s) manquant(s) : les clips sont "
                     "conservés intacts\n",
                     loaded.missingMedia.size());
    }
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    Args args;
    std::string error;
    if (!parseArgs(argc, argv, args, error)) {
        std::fprintf(stderr, "ERREUR : %s\n\n", error.c_str());
        printUsage();
        return 1;
    }
    if (args.command == "help" || args.command == "--help" || args.command == "-h") {
        printUsage();
        return 0;
    }
    if (args.command == "new")
        return commandNew(args);
    if (args.command == "add-track")
        return commandAddTrack(args);
    if (args.command == "add-clip")
        return commandAddClip(args);
    if (args.command == "set")
        return commandSet(args);
    if (args.command == "save")
        return commandSave(args);
    if (args.command == "load" || args.command == "dump" || args.command == "inspect")
        return commandLoad(args);
    std::fprintf(stderr, "ERREUR : commande inconnue « %s »\n\n", args.command.c_str());
    printUsage();
    return 1;
}
