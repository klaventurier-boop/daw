#include "svp/project/ProjectIO.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <system_error>
#include "svp/json/Json.h"
#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#endif

namespace svp::project {
namespace {

namespace fs = std::filesystem;

TestHooks g_hooks{};

// ---------------------------------------------------------------- utilitaires

std::string parentDirOf(const std::string& path)
{
    const fs::path parent = fs::path(path).parent_path();
    return parent.empty() ? std::string(".") : parent.string();
}

/** Chemin écrit dans le fichier : séparateurs « / » (un fichier de projet doit
    se relire à l'identique sur Windows et sur Linux, et un `diff` entre les
    deux doit rester vide). */
std::string toStoredSeparators(const std::string& path)
{
    std::string out = path;
    std::replace(out.begin(), out.end(), '\\', '/');
    return out;
}

std::string fromStoredSeparators(const std::string& path)
{
#ifdef _WIN32
    std::string out = path;
    std::replace(out.begin(), out.end(), '/', '\\');
    return out;
#else
    return path;
#endif
}

bool isAbsolutePath(const std::string& path)
{
    return fs::path(path).is_absolute();
}

// ------------------------------------------------- sérialisation (écriture)

void writeTrackParams(json::Writer& writer, const TrackParams& params)
{
    writer.key("gainDb");
    writer.valueDouble(params.gainDb);
    writer.key("pan");
    writer.valueDouble(params.pan);
    writer.key("mute");
    writer.valueBool(params.mute);
    writer.key("solo");
    writer.valueBool(params.solo);
}

void writeClip(json::Writer& writer, const Clip& clip)
{
    writer.beginObject();
    writer.key("id");
    writer.valueString(clip.id);
    writer.key("name");
    writer.valueString(clip.name);
    writer.key("mediaPath");
    writer.valueString(clip.mediaPath);
    writer.key("startFrames");
    writer.valueInt(clip.startFrames);
    writer.key("sourceOffsetFrames");
    writer.valueInt(clip.sourceOffsetFrames);
    writer.key("lengthFrames");
    writer.valueInt(clip.lengthFrames);
    writer.key("gainDb");
    writer.valueDouble(clip.gainDb);
    writer.key("fadeInSeconds");
    writer.valueDouble(clip.fadeInSeconds);
    writer.key("fadeOutSeconds");
    writer.valueDouble(clip.fadeOutSeconds);
    writer.endObject();
}

// -------------------------------------------------- désérialisation (lecture)

struct ReadContext {
    std::string error;
    std::vector<std::string> warnings;
    [[nodiscard]] bool failed() const noexcept { return !error.empty(); }
};

void warnUnknownKeys(const json::Value& object, std::initializer_list<const char*> known,
                     const std::string& path, ReadContext& ctx)
{
    for (const auto& member : object.members) {
        bool isKnown = false;
        for (const char* name : known) {
            if (member.first == name) {
                isKnown = true;
                break;
            }
        }
        if (!isKnown) {
            // Règle de compatibilité ascendante : un champ inconnu est IGNORÉ
            // (jamais une erreur) pour qu'un projet écrit par une version plus
            // récente en lecture seule ne soit pas rejeté en bloc.
            ctx.warnings.push_back(path + "." + member.first + " : champ inconnu ignoré");
        }
    }
}

bool readString(const json::Value& object, const char* key, const std::string& path,
                std::string& out, ReadContext& ctx, bool required)
{
    const json::Value* value = object.find(key);
    if (value == nullptr) {
        if (required)
            ctx.error = path + "." + key + " : champ obligatoire absent";
        return !required;
    }
    if (!value->isString()) {
        ctx.error = path + "." + key + " : chaîne attendue";
        return false;
    }
    out = value->text;
    return true;
}

bool readBool(const json::Value& object, const char* key, const std::string& path, bool& out,
              ReadContext& ctx)
{
    const json::Value* value = object.find(key);
    if (value == nullptr)
        return true;  // valeur par défaut conservée
    if (!value->isBool()) {
        ctx.error = path + "." + key + " : booléen attendu (true/false)";
        return false;
    }
    out = value->boolean;
    return true;
}

bool readDouble(const json::Value& object, const char* key, const std::string& path, double& out,
                ReadContext& ctx)
{
    const json::Value* value = object.find(key);
    if (value == nullptr)
        return true;
    if (!value->isNumber()) {
        ctx.error = path + "." + key + " : nombre attendu";
        return false;
    }
    const double parsed = value->integral ? static_cast<double>(value->integer) : value->number;
    if (!std::isfinite(parsed)) {
        ctx.error = path + "." + key + " : nombre non fini";
        return false;
    }
    out = parsed;
    return true;
}

bool readInt(const json::Value& object, const char* key, const std::string& path, int64_t& out,
             ReadContext& ctx, bool allowNonInteger = false)
{
    const json::Value* value = object.find(key);
    if (value == nullptr)
        return true;
    if (!value->isNumber()) {
        ctx.error = path + "." + key + " : nombre attendu";
        return false;
    }
    if (!value->isInteger() && !allowNonInteger) {
        ctx.error = path + "." + key + " : entier attendu (pas de partie fractionnaire)";
        return false;
    }
    out = value->isInteger() ? value->integer : static_cast<int64_t>(value->number);
    return true;
}

bool readInt32(const json::Value& object, const char* key, const std::string& path, int& out,
               ReadContext& ctx)
{
    int64_t wide = out;
    if (!readInt(object, key, path, wide, ctx))
        return false;
    if (wide < -2147483648LL || wide > 2147483647LL) {
        ctx.error = path + "." + key + " : valeur hors plage 32 bits";
        return false;
    }
    out = static_cast<int>(wide);
    return true;
}

bool parseClip(const json::Value& value, const std::string& path, std::size_t trackIndex,
               std::size_t clipIndex, const std::string& projectDir, Clip& out, ReadContext& ctx)
{
    if (!value.isObject()) {
        ctx.error = path + " : objet attendu";
        return false;
    }
    out.id = makeClipId(trackIndex, clipIndex);
    out.name.clear();
    out.mediaPath.clear();
    if (!readString(value, "id", path, out.id, ctx, false))
        return false;
    if (!readString(value, "name", path, out.name, ctx, false))
        return false;
    if (!readString(value, "mediaPath", path, out.mediaPath, ctx, true))
        return false;
    if (!readInt(value, "startFrames", path, out.startFrames, ctx))
        return false;
    if (!readInt(value, "sourceOffsetFrames", path, out.sourceOffsetFrames, ctx))
        return false;
    if (!readInt(value, "lengthFrames", path, out.lengthFrames, ctx))
        return false;
    if (!readDouble(value, "gainDb", path, out.gainDb, ctx))
        return false;
    if (!readDouble(value, "fadeInSeconds", path, out.fadeInSeconds, ctx))
        return false;
    if (!readDouble(value, "fadeOutSeconds", path, out.fadeOutSeconds, ctx))
        return false;
    warnUnknownKeys(value,
                    {"id", "name", "mediaPath", "startFrames", "sourceOffsetFrames", "lengthFrames",
                     "gainDb", "fadeInSeconds", "fadeOutSeconds"},
                    path, ctx);
    if (ctx.failed())
        return false;
    // Chemin stocké -> chemin réellement utilisable + contrôle d'existence.
    // La forme écrite dans le fichier est MÉMORISÉE avant résolution : c'est
    // elle que `toJson` réécrira, ce qui rend le réenregistrement idempotent
    // (docs/PROJECT-FORMAT.md §5.2).
    out.storedMediaPath = out.mediaPath;
    const std::string resolved = resolveStoredMediaPath(out.mediaPath, projectDir);
    out.mediaPath = resolved;
    if (out.name.empty())
        out.name = fs::path(resolved).filename().string();
    return true;
}

bool parseTrack(const json::Value& value, std::size_t trackIndex, const std::string& projectDir,
                Track& out, ReadContext& ctx)
{
    const std::string path = "tracks[" + std::to_string(trackIndex) + "]";
    if (!value.isObject()) {
        ctx.error = path + " : objet attendu";
        return false;
    }
    out = Track{};
    out.id = makeTrackId(trackIndex);
    out.name = "Piste " + std::to_string(trackIndex + 1);
    if (!readString(value, "id", path, out.id, ctx, false))
        return false;
    if (!readString(value, "name", path, out.name, ctx, false))
        return false;
    if (!readString(value, "color", path, out.color, ctx, false))
        return false;
    if (!readString(value, "input", path, out.input, ctx, false))
        return false;
    if (!readString(value, "output", path, out.output, ctx, false))
        return false;
    if (const json::Value* params = value.find("params")) {
        const std::string paramsPath = path + ".params";
        if (!params->isObject()) {
            ctx.error = paramsPath + " : objet attendu";
            return false;
        }
        if (!readDouble(*params, "gainDb", paramsPath, out.params.gainDb, ctx))
            return false;
        if (!readDouble(*params, "pan", paramsPath, out.params.pan, ctx))
            return false;
        if (!readBool(*params, "mute", paramsPath, out.params.mute, ctx))
            return false;
        if (!readBool(*params, "solo", paramsPath, out.params.solo, ctx))
            return false;
        warnUnknownKeys(*params, {"gainDb", "pan", "mute", "solo"}, paramsPath, ctx);
    }
    if (const json::Value* clips = value.find("clips")) {
        if (!clips->isArray()) {
            ctx.error = path + ".clips : tableau attendu";
            return false;
        }
        for (std::size_t c = 0; c < clips->items.size(); ++c) {
            Clip clip;
            const std::string clipPath = path + ".clips[" + std::to_string(c) + "]";
            if (!parseClip(clips->items[c], clipPath, trackIndex, c, projectDir, clip, ctx))
                return false;
            out.clips.push_back(std::move(clip));
        }
    }
    warnUnknownKeys(value, {"id", "name", "color", "input", "output", "params", "clips"}, path,
                    ctx);
    return !ctx.failed();
}

} // namespace

// -------------------------------------------------------------- écriture JSON

std::string makeStoredMediaPath(const std::string& mediaPath, const std::string& projectDir)
{
    if (mediaPath.empty())
        return mediaPath;
    std::error_code ec;
    const fs::path media = fs::weakly_canonical(fs::path(mediaPath), ec);
    if (ec)
        return toStoredSeparators(mediaPath);
    const fs::path dir = fs::weakly_canonical(fs::path(projectDir), ec);
    if (ec)
        return toStoredSeparators(mediaPath);
    // Relatif seulement si le média est SOUS le dossier du projet : un chemin
    // en « ../ » serait fragile et peu lisible, on préfère alors l'absolu.
    const fs::path relative = media.lexically_relative(dir);
    if (relative.empty())
        return toStoredSeparators(mediaPath);
    for (const fs::path& part : relative) {
        if (part == "..")
            return toStoredSeparators(media.lexically_normal().string());
    }
    if (relative == ".")
        return toStoredSeparators(media.filename().string());
    return toStoredSeparators(relative.string());
}

std::string resolveStoredMediaPath(const std::string& storedPath, const std::string& projectDir)
{
    if (storedPath.empty())
        return storedPath;
    const std::string native = fromStoredSeparators(storedPath);
    if (isAbsolutePath(native))
        return fs::path(native).lexically_normal().string();
    if (projectDir.empty())
        return fs::path(native).lexically_normal().string();
    return (fs::path(projectDir) / fs::path(native)).lexically_normal().string();
}

/** Forme du chemin média à ÉCRIRE pour un clip.

    Règle : la forme mémorisée au chargement (`Clip::storedMediaPath`) est
    restituée TELLE QUELLE tant qu'elle désigne encore le média courant — un
    réenregistrement sans modification ne change donc aucun octet du document.
    Dès que le clip a été déplacé, renommé ou chargé dans un autre dossier de
    projet, la forme stockée est recalculée depuis `mediaPath`. */
static std::string storedMediaPathFor(const Clip& clip, const std::string& projectDir)
{
    if (!clip.storedMediaPath.empty()
        && resolveStoredMediaPath(clip.storedMediaPath, projectDir) == clip.mediaPath)
        return clip.storedMediaPath;
    return makeStoredMediaPath(clip.mediaPath, projectDir);
}

std::string toJson(const Project& project, const std::string& projectDir)
{
    json::Writer writer;
    writer.beginObject();
    writer.key("schema");
    writer.valueString(kSchemaName);
    writer.key("schemaVersion");
    writer.valueInt(project.schemaVersion);
    writer.key("appVersion");
    writer.valueString(project.appVersion);
    writer.key("name");
    writer.valueString(project.name);
    writer.key("sampleRate");
    writer.valueInt(project.sampleRate);
    writer.key("bitDepth");
    writer.valueInt(project.bitDepth);
    writer.key("tempoBpm");
    writer.valueDouble(project.tempoBpm);
    writer.key("lengthFrames");
    writer.valueInt(project.lengthFrames);
    // Convention de chemins, écrite dans le fichier pour qu'un lecteur
    // (ou un humain) sache comment interpréter `mediaPath`.
    writer.key("mediaPathBase");
    writer.valueString("projectFile");
    writer.key("tracks");
    writer.beginArray();
    for (const Track& track : project.tracks) {
        writer.beginObject();
        writer.key("id");
        writer.valueString(track.id);
        writer.key("name");
        writer.valueString(track.name);
        writer.key("color");
        writer.valueString(track.color);
        writer.key("input");
        writer.valueString(track.input);
        writer.key("output");
        writer.valueString(track.output);
        writer.key("params");
        writer.beginObject();
        writeTrackParams(writer, track.params);
        writer.endObject();
        writer.key("clips");
        writer.beginArray();
        for (const Clip& clip : track.clips) {
            Clip stored = clip;
            stored.mediaPath = storedMediaPathFor(clip, projectDir);
            writeClip(writer, stored);
        }
        writer.endArray();
        writer.endObject();
    }
    writer.endArray();
    writer.endObject();
    return writer.str();
}

LoadResult fromJson(const std::string& text, const std::string& projectDir)
{
    LoadResult result;
    const json::ParseResult parsed = json::parse(text);
    if (!parsed.ok) {
        result.ok = false;
        result.error = "JSON invalide — " + parsed.error;
        return result;
    }
    const json::Value& root = parsed.value;
    if (!root.isObject()) {
        result.error = "document de projet attendu (objet JSON à la racine)";
        return result;
    }
    ReadContext ctx;

    // 1. Nature et version du document : contrôlées AVANT tout autre champ.
    if (const json::Value* schema = root.find("schema")) {
        if (!schema->isString()) {
            result.error = "schema : chaîne attendue";
            return result;
        }
        if (schema->text != kSchemaName) {
            result.error = "ce fichier n'est pas un projet Studio Voice Pro (schema = « "
                           + schema->text + " », attendu « " + kSchemaName + " »)";
            return result;
        }
    } else {
        ctx.warnings.push_back("schema : absent (projet écrit par une version antérieure ?)");
    }

    const json::Value* versionValue = root.find("schemaVersion");
    if (versionValue == nullptr) {
        result.error = "schemaVersion : champ obligatoire absent";
        return result;
    }
    if (!versionValue->isInteger()) {
        result.error = "schemaVersion : entier attendu";
        return result;
    }
    const int64_t version = versionValue->integer;
    if (version < 1) {
        result.error = "schemaVersion " + std::to_string(version) + " invalide";
        return result;
    }
    if (version > kSchemaVersion) {
        result.error = "version de schéma " + std::to_string(version)
                       + " inconnue : cette version lit jusqu'à la version "
                       + std::to_string(kSchemaVersion)
                       + ". Le projet n'a pas été modifié ; ouvrez-le avec une version plus récente "
                         "de Studio Voice Pro.";
        return result;
    }

    Project project;
    project.schemaVersion = static_cast<int>(version);
    if (!readString(root, "appVersion", "projet", project.appVersion, ctx, false))
        return result;
    if (!readString(root, "name", "projet", project.name, ctx, false))
        return result;
    if (!readInt32(root, "sampleRate", "projet", project.sampleRate, ctx))
        return result;
    if (!readInt32(root, "bitDepth", "projet", project.bitDepth, ctx))
        return result;
    if (!readDouble(root, "tempoBpm", "projet", project.tempoBpm, ctx))
        return result;
    if (!readInt(root, "lengthFrames", "projet", project.lengthFrames, ctx))
        return result;
    if (const json::Value* base = root.find("mediaPathBase")) {
        if (base->isString() && base->text != "projectFile")
            ctx.warnings.push_back("mediaPathBase = « " + base->text
                                   + " » inconnu : les chemins sont résolus depuis le dossier du "
                                     "projet");
    }

    const json::Value* tracks = root.find("tracks");
    if (tracks == nullptr) {
        result.error = "tracks : champ obligatoire absent";
        return result;
    }
    if (!tracks->isArray()) {
        result.error = "tracks : tableau attendu";
        return result;
    }
    for (std::size_t t = 0; t < tracks->items.size(); ++t) {
        Track track;
        if (!parseTrack(tracks->items[t], t, projectDir, track, ctx)) {
            result.error = ctx.error.empty() ? "piste illisible" : ctx.error;
            return result;
        }
        project.tracks.push_back(std::move(track));
    }
    warnUnknownKeys(root,
                    {"schema", "schemaVersion", "appVersion", "name", "sampleRate", "bitDepth",
                     "tempoBpm", "lengthFrames", "mediaPathBase", "tracks"},
                    "projet", ctx);
    if (ctx.failed()) {
        result.error = ctx.error;
        return result;
    }

    if (project.name.empty()) {
        ctx.warnings.push_back("name : vide, remplacé par « Sans titre »");
        project.name = "Sans titre";
    }
    if (project.bitDepth != 16 && project.bitDepth != 24 && project.bitDepth != 32) {
        ctx.warnings.push_back("bitDepth = " + std::to_string(project.bitDepth)
                               + " inconnu : ramené à 32 (flottant)");
        project.bitDepth = 32;
    }
    if (project.sampleRate < 8000 || project.sampleRate > 768000) {
        ctx.warnings.push_back("sampleRate = " + std::to_string(project.sampleRate)
                               + " invalide : ramené à 48000");
        project.sampleRate = 48000;
    }

    // 2. Médias : contrôle d'existence, SANS échec du chargement.
    for (Track& track : project.tracks) {
        for (Clip& clip : track.clips) {
            std::error_code ec;
            if (!fs::exists(fs::path(clip.mediaPath), ec)) {
                clip.mediaMissing = true;
                result.missingMedia.push_back(clip.mediaPath + "  (clip " + clip.id + ")");
            }
        }
    }
    result.warnings = ctx.warnings;
    result.project = std::move(project);
    result.ok = true;
    return result;
}

// ------------------------------------------------------------------ écriture

SaveResult saveProject(const Project& project, const std::string& projectPath,
                       const SaveOptions& options)
{
    SaveResult result;
    result.finalPath = projectPath;
    if (projectPath.empty()) {
        result.error = "chemin de projet vide";
        return result;
    }
    std::string error;
    if (!validateProject(project, error)) {
        result.error = "projet refusé à l'écriture — " + error;
        return result;
    }
    const fs::path finalPath(projectPath);
    const fs::path directory = finalPath.parent_path().empty() ? fs::path(".")
                                                               : finalPath.parent_path();
    std::error_code ec;
    fs::create_directories(directory, ec);  // déjà présent : ce n'est pas une erreur
    const std::string json = toJson(project, directory.string());
    result.json = json;
    // Même dossier que la cible : le renommage reste dans le même système de
    // fichiers, donc atomique.
    const fs::path tempPath(projectPath + ".tmp");
    result.tempPath = tempPath.string();

    if (g_hooks.failTempWrite) {
        result.error =
            "écriture du fichier temporaire impossible (interruption simulée par un crochet de test)";
        return result;
    }
    {
        std::ofstream out(tempPath, std::ios::binary | std::ios::trunc);
        if (!out) {
            result.error = "impossible d'ouvrir le fichier temporaire « " + tempPath.string() + " »";
            return result;
        }
        out.write(json.data(), static_cast<std::streamsize>(json.size()));
        out.flush();
        out.close();
        if (!out) {
            result.error = "écriture incomplète dans « " + tempPath.string() + " »";
            std::error_code cleanup;
            fs::remove(tempPath, cleanup);
            return result;
        }
    }
#ifndef _WIN32
    // Synchronisation : le contenu doit avoir atteint le disque avant que le
    // renommage ne le rende visible. Sans cela, une coupure de courant peut
    // laisser un fichier renommé mais vide.
    if (const int fd = ::open(tempPath.c_str(), O_RDONLY); fd >= 0) {
        ::fsync(fd);
        ::close(fd);
    }
#endif
    if (g_hooks.abortBeforeReplace) {
        // Simule une coupure juste avant le renommage : le fichier temporaire
        // reste sur le disque (comme après un vrai plantage), le projet lui-même
        // n'a pas été touché.
        result.error = "coupure simulée avant le renommage (crochet de test)";
        return result;
    }

    std::error_code renameError;
    fs::rename(tempPath, finalPath, renameError);
    if (renameError) {
#ifdef _WIN32
        // À VÉRIFIER sur Windows 11 : certaines configurations refusent
        // d'écraser la destination. On retire alors l'ancien fichier puis on
        // renomme — la fenêtre entre les deux appels n'est pas atomique (le
        // fichier existant reste protégé par le temporaire complet).
        std::error_code ignored;
        fs::remove(finalPath, ignored);
        fs::rename(tempPath, finalPath, renameError);
        if (renameError) {
            fs::remove(tempPath, ignored);
            result.error = "renommage impossible : " + renameError.message();
            return result;
        }
#else
        std::error_code cleanup;
        fs::remove(tempPath, cleanup);
        result.error = "renommage impossible (« " + tempPath.string() + " » → « "
                       + finalPath.string() + " ») : " + renameError.message()
                       + " — le projet précédent est intact";
        return result;
#endif
    }
#ifndef _WIN32
    if (const int dirFd = ::open(directory.c_str(), O_RDONLY | O_DIRECTORY); dirFd >= 0) {
        ::fsync(dirFd);  // rend le renommage lui-même durable
        ::close(dirFd);
    }
#endif
    result.ok = true;
    if (options.writeAutosaveCopy) {
        const SaveResult autosave = saveAutosave(project, projectPath);
        result.autosavePath = autosavePathFor(projectPath);
        if (!autosave.ok)
            result.error = "projet enregistré, mais sauvegarde automatique échouée : "
                           + autosave.error;
    }
    return result;
}

LoadResult loadProject(const std::string& projectPath)
{
    LoadResult result;
    if (projectPath.empty()) {
        result.error = "chemin de projet vide";
        return result;
    }
    std::ifstream in(projectPath, std::ios::binary);
    if (!in) {
        result.error = "fichier de projet introuvable : « " + projectPath + " »";
        return result;
    }
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (!in.good() && !in.eof()) {
        result.error = "lecture incomplète de « " + projectPath + " »";
        return result;
    }
    result = fromJson(text, parentDirOf(projectPath));
    if (result.ok && result.warnings.empty() && result.missingMedia.empty())
        result.warnings.clear();
    return result;
}

// ------------------------------------------------------- sauvegarde auto

std::string autosavePathFor(const std::string& projectPath)
{
    return projectPath + ".autosave.json";
}

SaveResult saveAutosave(const Project& project, const std::string& projectPath)
{
    SaveResult result = saveProject(project, autosavePathFor(projectPath));
    if (result.ok)
        result.autosavePath = result.finalPath;
    return result;
}

LoadResult loadAutosave(const std::string& projectPath)
{
    LoadResult result = loadProject(autosavePathFor(projectPath));
    result.fromAutosave = result.ok;
    return result;
}

bool hasAutosave(const std::string& projectPath)
{
    std::error_code ec;
    return fs::exists(fs::path(autosavePathFor(projectPath)), ec);
}

bool removeAutosave(const std::string& projectPath)
{
    std::error_code ec;
    return fs::remove(fs::path(autosavePathFor(projectPath)), ec) || !hasAutosave(projectPath);
}

// -------------------------------------------------------------- préférences

Preferences defaultPreferences()
{
    return Preferences{};
}

std::string defaultPreferencesPath()
{
    if (const char* configDir = std::getenv("SVP_CONFIG_DIR")) {
        if (configDir[0] != '\0')
            return (fs::path(configDir) / "preferences.json").string();
    }
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME")) {
        if (xdg[0] != '\0')
            return (fs::path(xdg) / "studio-voice-pro" / "preferences.json").string();
    }
    if (const char* home = std::getenv("HOME")) {
        if (home[0] != '\0')
            return (fs::path(home) / ".config" / "studio-voice-pro" / "preferences.json").string();
    }
    return "studio-voice-pro-preferences.json";
}

SaveResult savePreferences(const Preferences& preferences, const std::string& path)
{
    SaveResult result;
    result.finalPath = path;
    if (path.empty()) {
        result.error = "chemin de préférences vide";
        return result;
    }
    if (preferences.schemaVersion < 1) {
        result.error = "version de préférences invalide";
        return result;
    }
    json::Writer writer;
    writer.beginObject();
    writer.key("schema");
    writer.valueString("studio-voice-pro.preferences");
    writer.key("schemaVersion");
    writer.valueInt(preferences.schemaVersion);
    writer.key("defaultSampleRate");
    writer.valueInt(preferences.defaultSampleRate);
    writer.key("defaultBitDepth");
    writer.valueInt(preferences.defaultBitDepth);
    writer.key("defaultTempoBpm");
    writer.valueDouble(preferences.defaultTempoBpm);
    writer.key("autosaveEnabled");
    writer.valueBool(preferences.autosaveEnabled);
    writer.key("autosaveIntervalSeconds");
    writer.valueInt(preferences.autosaveIntervalSeconds);
    writer.key("autosaveKeepCount");
    writer.valueInt(preferences.autosaveKeepCount);
    writer.key("defaultProjectDirectory");
    writer.valueString(preferences.defaultProjectDirectory);
    writer.key("defaultMediaDirectory");
    writer.valueString(preferences.defaultMediaDirectory);
    writer.key("audioDeviceType");
    writer.valueString(preferences.audioDeviceType);
    writer.key("audioOutputDeviceName");
    writer.valueString(preferences.audioOutputDeviceName);
    writer.key("audioBufferSizeFrames");
    writer.valueInt(preferences.audioBufferSizeFrames);
    writer.endObject();
    const std::string json = writer.str();
    result.json = json;

    const fs::path finalPath(path);
    const fs::path directory =
        finalPath.parent_path().empty() ? fs::path(".") : finalPath.parent_path();
    std::error_code ec;
    fs::create_directories(directory, ec);
    const fs::path tempPath(path + ".tmp");
    result.tempPath = tempPath.string();
    if (g_hooks.failTempWrite) {
        result.error = "écriture impossible (interruption simulée par un crochet de test)";
        return result;
    }
    {
        std::ofstream out(tempPath, std::ios::binary | std::ios::trunc);
        if (!out) {
            result.error = "impossible d'écrire « " + tempPath.string() + " »";
            return result;
        }
        out.write(json.data(), static_cast<std::streamsize>(json.size()));
        out.flush();
        out.close();
        if (!out) {
            std::error_code cleanup;
            fs::remove(tempPath, cleanup);
            result.error = "écriture incomplète des préférences";
            return result;
        }
    }
    if (g_hooks.abortBeforeReplace) {
        result.error = "coupure simulée avant le renommage (crochet de test)";
        return result;
    }
    std::error_code renameError;
    fs::rename(tempPath, finalPath, renameError);
    if (renameError) {
        std::error_code cleanup;
        fs::remove(tempPath, cleanup);
        result.error = "renommage impossible : " + renameError.message();
        return result;
    }
    result.ok = true;
    return result;
}

PreferencesLoadResult loadPreferences(const std::string& path)
{
    PreferencesLoadResult result;
    result.preferences = defaultPreferences();
    if (path.empty()) {
        result.ok = false;
        result.error = "chemin de préférences vide";
        return result;
    }
    std::error_code ec;
    if (!fs::exists(fs::path(path), ec))
        return result;  // fichier absent : valeurs par défaut, pas une erreur
    result.fileExisted = true;
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        result.ok = false;
        result.error = "préférences illisibles : « " + path + " »";
        return result;
    }
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    const json::ParseResult parsed = json::parse(text);
    if (!parsed.ok || !parsed.value.isObject()) {
        result.ok = false;
        result.error = "préférences invalides — "
                       + (parsed.ok ? std::string("objet JSON attendu") : parsed.error);
        return result;
    }
    const json::Value& root = parsed.value;
    if (const json::Value* version = root.find("schemaVersion")) {
        if (!version->isInteger() || version->integer > result.preferences.schemaVersion) {
            result.ok = false;
            result.error = "version de préférences inconnue : valeurs par défaut conservées";
            return result;
        }
    }
    ReadContext ctx;
    readInt32(root, "defaultSampleRate", "preferences", result.preferences.defaultSampleRate, ctx);
    readInt32(root, "defaultBitDepth", "preferences", result.preferences.defaultBitDepth, ctx);
    readDouble(root, "defaultTempoBpm", "preferences", result.preferences.defaultTempoBpm, ctx);
    readBool(root, "autosaveEnabled", "preferences", result.preferences.autosaveEnabled, ctx);
    readInt32(root, "autosaveIntervalSeconds", "preferences",
              result.preferences.autosaveIntervalSeconds, ctx);
    readInt32(root, "autosaveKeepCount", "preferences", result.preferences.autosaveKeepCount, ctx);
    readString(root, "defaultProjectDirectory", "preferences",
               result.preferences.defaultProjectDirectory, ctx, false);
    readString(root, "defaultMediaDirectory", "preferences",
               result.preferences.defaultMediaDirectory, ctx, false);
    readString(root, "audioDeviceType", "preferences", result.preferences.audioDeviceType, ctx,
               false);
    readString(root, "audioOutputDeviceName", "preferences",
               result.preferences.audioOutputDeviceName, ctx, false);
    readInt32(root, "audioBufferSizeFrames", "preferences",
              result.preferences.audioBufferSizeFrames, ctx);
    if (ctx.failed()) {
        // Une valeur illisible n'empêche pas d'ouvrir l'application : on garde
        // la valeur par défaut du champ et on prévient.
        result.warnings.push_back(ctx.error + " — valeur par défaut conservée");
    }
    result.warnings.insert(result.warnings.end(), ctx.warnings.begin(), ctx.warnings.end());
    if (result.preferences.bitDepthInvalid())
        result.warnings.push_back("defaultBitDepth inconnu — ramené à 32");
    return result;
}

Project newProjectFromPreferences(const Preferences& preferences, const std::string& name)
{
    Preferences effective = preferences;
    if (effective.bitDepthInvalid())
        effective.defaultBitDepth = 32;
    if (effective.defaultSampleRate < 8000 || effective.defaultSampleRate > 768000)
        effective.defaultSampleRate = 48000;
    if (!std::isfinite(effective.defaultTempoBpm) || effective.defaultTempoBpm <= 0.0)
        effective.defaultTempoBpm = 120.0;
    return makeProject(name, effective.defaultSampleRate, effective.defaultBitDepth,
                       effective.defaultTempoBpm);
}

// ------------------------------------------------------------------ affichage

namespace {

std::string fixed(double value, int decimals)
{
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.*f", decimals, value);
    return buffer;
}

std::string seconds(int64_t frames, int sampleRate)
{
    const double value = (sampleRate > 0) ? static_cast<double>(frames) / sampleRate : 0.0;
    return fixed(value, 3) + " s";
}

} // namespace

std::string dumpProject(const Project& project, const LoadResult& info,
                        const std::string& projectPath)
{
    std::string out;
    char line[512];
    std::snprintf(line, sizeof(line), "Projet « %s » — %s\n", project.name.c_str(),
                  projectPath.empty() ? "(non enregistré)" : projectPath.c_str());
    out += line;
    std::snprintf(line, sizeof(line), "  schéma            : %s v%d (écrit par Studio Voice Pro %s)\n",
                  kSchemaName, project.schemaVersion, project.appVersion.c_str());
    out += line;
    std::snprintf(line, sizeof(line), "  taux d'échantillonnage : %d Hz\n", project.sampleRate);
    out += line;
    const char* depth = project.bitDepth == 32 ? "32 bits flottant" : nullptr;
    std::string depthText;
    if (depth != nullptr)
        depthText = depth;
    else
        depthText = std::to_string(project.bitDepth) + " bits entier";
    std::snprintf(line, sizeof(line), "  profondeur        : %s\n", depthText.c_str());
    out += line;
    std::snprintf(line, sizeof(line), "  tempo             : %s bpm\n",
                  fixed(project.tempoBpm, 3).c_str());
    out += line;
    std::snprintf(line, sizeof(line), "  longueur          : %lld échantillons (%s)\n",
                  static_cast<long long>(project.lengthFrames),
                  seconds(project.lengthFrames, project.sampleRate).c_str());
    out += line;
    std::snprintf(line, sizeof(line), "  pistes            : %zu\n", project.tracks.size());
    out += line;
    std::snprintf(line, sizeof(line), "  clips             : %zu\n", clipCount(project));
    out += line;
    for (std::size_t t = 0; t < project.tracks.size(); ++t) {
        const Track& track = project.tracks[t];
        std::snprintf(line, sizeof(line),
                      "  [%zu] %s « %s »  gain %s dB  pan %s  mute %s  solo %s  entrée %s  "
                      "sortie %s  couleur %s\n",
                      t + 1, track.id.c_str(), track.name.c_str(),
                      fixed(track.params.gainDb, 3).c_str(), fixed(track.params.pan, 2).c_str(),
                      track.params.mute ? "oui" : "non", track.params.solo ? "oui" : "non",
                      track.input.c_str(), track.output.c_str(), track.color.c_str());
        out += line;
        for (const Clip& clip : track.clips) {
            std::snprintf(
                line, sizeof(line),
                "      clip %s « %s »  %s\n        début %lld  décalage %lld  longueur %lld (%s)  "
                "gain %s dB  fondus %s/%s s\n",
                clip.id.c_str(), clip.name.c_str(), clip.mediaPath.c_str(),
                static_cast<long long>(clip.startFrames),
                static_cast<long long>(clip.sourceOffsetFrames),
                static_cast<long long>(clip.lengthFrames),
                seconds(clip.lengthFrames, project.sampleRate).c_str(),
                fixed(clip.gainDb, 2).c_str(), fixed(clip.fadeInSeconds, 3).c_str(),
                fixed(clip.fadeOutSeconds, 3).c_str());
            out += line;
            if (!clip.storedMediaPath.empty()) {
                std::snprintf(line, sizeof(line), "        chemin stocké dans le fichier : %s\n",
                              clip.storedMediaPath.c_str());
                out += line;
            }
            if (clip.mediaMissing) {
                std::snprintf(line, sizeof(line), "        MÉDIA MANQUANT : %s\n",
                              clip.mediaPath.c_str());
                out += line;
            }
        }
    }
    if (info.fromAutosave)
        out += "  source            : fichier de sauvegarde automatique\n";
    if (!info.warnings.empty()) {
        out += "  avertissements     :\n";
        for (const std::string& warning : info.warnings)
            out += "    - " + warning + "\n";
    }
    if (!info.missingMedia.empty()) {
        std::snprintf(line, sizeof(line), "  médias manquants   : %zu (clips conservés)\n",
                      info.missingMedia.size());
        out += line;
        for (const std::string& missing : info.missingMedia)
            out += "    - " + missing + "\n";
    }
    return out;
}

// ------------------------------------------------------------------- crochets

void setProjectIoTestHooks(const TestHooks& hooks)
{
    g_hooks = hooks;
}

TestHooks projectIoTestHooks()
{
    return g_hooks;
}

} // namespace svp::project
