// Studio Voice Pro — tests du module project : format de projet JSON, fidélité
// de l'aller-retour, écriture atomique, chemins relatifs, média manquant,
// erreurs signalées proprement (jamais de plantage).
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include "svp/audio/Buffer.h"
#include "svp/io/WavFile.h"
#include "svp/json/Json.h"
#include "svp/project/Project.h"
#include "svp/project/ProjectIO.h"
#include "svp_test.hpp"

namespace {

namespace fs = std::filesystem;
using namespace svp::project;

/** Racine ABSOLUE et PROPRE des fichiers de cette suite.

    Deux propriétés sont nécessaires pour que la suite donne toujours le même
    résultat :
      * le chemin est absolu même quand `SVP_TEST_TMPDIR` est absent (sinon, un
        lancement direct depuis le dépôt ferait de `outsideDir()` un chemin
        relatif, le « média hors projet » retomberait SOUS le dossier du projet
        et serait stocké en relatif : deux échecs de plus, variables selon le
        répertoire courant) ;
      * le dossier est vidé au démarrage du processus : aucun fichier laissé
        par l'exécution précédente ne peut influencer le résultat.

    `SVP_TEST_TMPDIR` reste utilisé quand il est fourni par ctest ; la suite
    s'isole dans son sous-dossier `project-suite`. */
const std::string& suiteRoot()
{
    static const std::string root = [] {
        std::error_code ec;
        fs::path base;
        if (const char* dir = std::getenv("SVP_TEST_TMPDIR"); dir != nullptr && *dir != '\0')
            base = fs::path(dir);
        else
            base = fs::temp_directory_path(ec) / "svp-project-tests";
        fs::path absolute = fs::weakly_canonical(base, ec);
        if (ec)
            absolute = fs::absolute(base, ec);
        const fs::path suiteDir = (absolute / "project-suite").lexically_normal();
        std::error_code cleanupError;
        fs::remove_all(suiteDir, cleanupError);  // état propre, quoi qu'il arrive
        std::error_code createError;
        fs::create_directories(suiteDir, createError);
        return suiteDir.string();
    }();
    return root;
}

std::string tmpDir()
{
    return suiteRoot();
}

std::string path(const std::string& name)
{
    return (fs::path(tmpDir()) / name).string();
}

std::string outsideDir()
{
    // Dossier VOLONTAIREMENT hors du dossier du projet : c'est ce qui doit
    // déclencher le repli sur un chemin absolu.
    return (fs::path(tmpDir()).parent_path() / "svp-project-media-externe").string();
}

std::string outsideMediaPath()
{
    return (fs::path(outsideDir()) / "hors-projet.wav").string();
}

std::string canonical(const std::string& p)
{
    return fs::weakly_canonical(fs::path(p)).string();
}

void ensureDir(const std::string& dir)
{
    std::error_code ec;
    fs::create_directories(dir, ec);
}

void writeClick(const std::string& filePath, int frames)
{
    svp::audio::AudioBuffer buffer(1, frames);
    for (int i = 0; i < frames; ++i)
        buffer.setSample(0, i, (i % 32 == 0) ? 0.5f : 0.0f);
    svp::io::WavFormat format;
    format.sampleRate = 48000;
    format.channels = 1;
    format.encoding = svp::io::WavEncoding::Float32;
    std::string error;
    const bool ok = svp::io::writeWavFile(filePath, buffer, format, &error);
    SVP_CHECK(ok);
}

std::string readTextFile(const std::string& filePath)
{
    std::ifstream in(filePath, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

bool fileExists(const std::string& filePath)
{
    std::error_code ec;
    return fs::exists(fs::path(filePath), ec);
}

/** Projet de référence : 3 pistes, 5 clips, valeurs volontairement difficiles
    à représenter (tiers, 0.1+0.2, positions au-delà de 2^53).

    Les chemins médias sont posés sous la forme « résolue » (celle que le
    chargement produit) : la convention de stockage, elle, est vérifiée
    séparément, en clair, dans `paths_relative_and_absolute`. */
std::string resolved(const std::string& filePath, const std::string& dir)
{
    return resolveStoredMediaPath(makeStoredMediaPath(canonical(filePath), dir), dir);
}

Project buildReferenceProject(const std::string& dir)
{
    Project project = makeProject("Session de contrôle", 48000, 32, 128.5);
    project.lengthFrames = 9007199254740993LL;  // 2^53 + 1 : impossible en double
    const std::string mediaA = resolved(path("media/voix.wav"), dir);
    const std::string mediaB = resolved(path("media/guitare 01.wav"), dir);
    const std::string outside = resolved(outsideMediaPath(), dir);

    Track voice = makeTrack(project, "Voix principale");
    voice.params.gainDb = -6.0;
    voice.params.pan = -0.3;
    voice.params.mute = false;
    voice.params.solo = true;
    voice.input = "mic 1";
    voice.output = "master";
    voice.color = "#E4572E";
    Clip take1;
    take1.id = makeClipId(0, 0);
    take1.name = "prise 1";
    take1.mediaPath = mediaA;
    take1.startFrames = 0;
    take1.sourceOffsetFrames = 0;
    take1.lengthFrames = 96000;
    take1.gainDb = 1.0 / 3.0;               // 0.333… : texte non exact
    take1.fadeInSeconds = 0.01;
    take1.fadeOutSeconds = 0.02;
    Clip take2;
    take2.id = makeClipId(0, 1);
    take2.name = "prise 2";
    take2.mediaPath = mediaA;
    take2.startFrames = 100000;
    take2.sourceOffsetFrames = 480;
    take2.lengthFrames = 48000;
    take2.gainDb = 0.1 + 0.2;               // 0.30000000000000004
    voice.clips = {take1, take2};
    project.tracks.push_back(voice);

    Track guitar = makeTrack(project, "Guitare");
    guitar.params.gainDb = -0.0;
    guitar.params.pan = 0.25;
    guitar.output = "bus guitares";
    Clip take3;
    take3.id = makeClipId(1, 0);
    take3.name = "guitare";
    take3.mediaPath = mediaB;
    take3.startFrames = 24000;
    take3.sourceOffsetFrames = 12;
    take3.lengthFrames = 72000;
    take3.gainDb = -1e-9;                   // très petite valeur signée
    guitar.clips = {take3};
    project.tracks.push_back(guitar);

    Track ref = makeTrack(project, "Référence");
    ref.params.pan = 0.0;
    ref.output = "master";
    Clip refClip;
    refClip.id = makeClipId(2, 0);
    refClip.name = "référence hors projet";  // chemin absolu (hors dossier projet)
    refClip.mediaPath = outside;
    refClip.startFrames = 480;
    refClip.lengthFrames = 48000;
    refClip.fadeInSeconds = 0.0;
    refClip.fadeOutSeconds = 0.5;
    ref.clips = {refClip};
    project.tracks.push_back(ref);
    return project;
}

void prepareMedia()
{
    ensureDir(path("media"));
    writeClick(path("media/voix.wav"), 2000);
    writeClick(path("media/guitare 01.wav"), 2000);
    ensureDir(outsideDir());
    writeClick(outsideMediaPath(), 600);
}

bool doublesBitEqual(double a, double b)
{
    return std::memcmp(&a, &b, sizeof(double)) == 0;
}

} // namespace

// ---------------------------------------------------------------------- cas

SVP_TEST(project, roundtrip_fidelity)
{
    prepareMedia();
    const std::string projectPath = path("session.svpproj.json");
    const std::string dir = tmpDir();
    const Project original = buildReferenceProject(dir);
    SVP_CHECK(original.tracks.size() == 3);
    SVP_CHECK(clipCount(original) == 4);

    const SaveResult saved = saveProject(original, projectPath);
    SVP_CHECK(saved.ok);
    if (!saved.ok) {
        std::printf("      %s\n", saved.error.c_str());
        return;
    }
    const LoadResult loaded = loadProject(projectPath);
    SVP_CHECK(loaded.ok);
    if (!loaded.ok) {
        std::printf("      %s\n", loaded.error.c_str());
        return;
    }
    // Comparaison champ par champ : le premier écart est nommé.
    const std::string difference = firstDifference(original, loaded.project);
    if (!difference.empty())
        std::printf("      différence : %s\n", difference.c_str());
    SVP_CHECK(difference.empty());
    SVP_CHECK(loaded.project == original);
    SVP_CHECK(loaded.missingMedia.empty());

    // 2e sauvegarde : le document est BYTE POUR BYTE identique (ordre stable).
    const SaveResult again = saveProject(loaded.project, path("session-2.svpproj.json"));
    SVP_CHECK(again.ok);
    SVP_CHECK(again.json == saved.json);

    // Vérifications ciblées sur les valeurs récalcitrantes.
    const Track& voice = loaded.project.tracks[0];
    SVP_CHECK(voice.params.gainDb == -6.0);
    SVP_CHECK(voice.params.pan == -0.3);
    SVP_CHECK(voice.params.solo);
    SVP_CHECK(voice.name == "Voix principale");
    SVP_CHECK(voice.input == "mic 1");
    SVP_CHECK(voice.clips[0].gainDb == 1.0 / 3.0);
    SVP_CHECK(doublesBitEqual(voice.clips[0].gainDb, 1.0 / 3.0));
    SVP_CHECK(voice.clips[1].gainDb == 0.1 + 0.2);
    SVP_CHECK(doublesBitEqual(voice.clips[1].gainDb, 0.1 + 0.2));
    SVP_CHECK(loaded.project.tracks[1].clips[0].gainDb == -1e-9);
    SVP_CHECK(loaded.project.tracks[2].clips[0].fadeOutSeconds == 0.5);
    // Position au-delà de 2^53 : exacte uniquement parce qu'elle est écrite en
    // entier décimal et relue par from_chars<int64_t>.
    SVP_CHECK(loaded.project.lengthFrames == 9007199254740993LL);
    SVP_CHECK(loaded.project.tempoBpm == 128.5);
    SVP_CHECK(loaded.project.tracks[0].clips[0].name == "prise 1");
}

// Idempotence — la propriété qui rend un format utilisable en gestion de
// version : écrire → recharger → réécrire produit un fichier identique OCTET
// POUR OCTET, et la forme stockée d'un chemin média est conservée telle quelle.
SVP_TEST(project, idempotent_resave_byte_identical)
{
    prepareMedia();
    const std::string firstPath = path("idempotent-1.svpproj.json");
    const std::string secondPath = path("idempotent-2.svpproj.json");
    const std::string thirdPath = path("idempotent-3.svpproj.json");
    const Project original = buildReferenceProject(tmpDir());

    // 1. écrire.
    const SaveResult first = saveProject(original, firstPath);
    SVP_CHECK(first.ok);
    const std::string onDisk = readTextFile(firstPath);
    SVP_CHECK(!onDisk.empty());
    SVP_CHECK(onDisk == first.json);  // ce qui est sur disque = document annoncé

    // 2. recharger.
    const LoadResult loaded = loadProject(firstPath);
    SVP_CHECK(loaded.ok);
    if (!loaded.ok) {
        std::printf("      %s\n", loaded.error.c_str());
        return;
    }
    SVP_CHECK(loaded.project == original);  // identité des médias, champ par champ
    SVP_CHECK(!loaded.project.tracks.empty() && loaded.project.tracks.size() == 3);
    SVP_CHECK(loaded.missingMedia.empty());
    // La FORME écrite dans le fichier est mémorisée, pas seulement sa résolution.
    SVP_CHECK(loaded.project.tracks[0].clips[0].storedMediaPath == "media/voix.wav");
    SVP_CHECK(loaded.project.tracks[0].clips[1].storedMediaPath == "media/voix.wav");
    SVP_CHECK(loaded.project.tracks[1].clips[0].storedMediaPath == "media/guitare 01.wav");
    SVP_CHECK(loaded.project.tracks[2].clips[0].storedMediaPath == canonical(outsideMediaPath()));
    SVP_CHECK(loaded.project.tracks[2].clips[0].storedMediaPath.front() == '/');
    // ... et le chemin résolu reste celui réellement ouvert.
    SVP_CHECK(loaded.project.tracks[0].clips[0].mediaPath == canonical(path("media/voix.wav")));

    // 3. réenregistrer dans le MÊME dossier de projet (autre nom de fichier).
    const SaveResult again = saveProject(loaded.project, secondPath);
    SVP_CHECK(again.ok);
    SVP_CHECK(readTextFile(secondPath) == onDisk);  // identique octet pour octet
    SVP_CHECK(again.json == first.json);

    // Point fixe : un troisième aller-retour ne change toujours rien.
    const LoadResult third = loadProject(secondPath);
    SVP_CHECK(third.ok);
    const SaveResult thirdSave = saveProject(third.project, thirdPath);
    SVP_CHECK(thirdSave.ok);
    SVP_CHECK(readTextFile(thirdPath) == onDisk);
    SVP_CHECK(third.project.tracks[2].clips[0].storedMediaPath == canonical(outsideMediaPath()));

    // Déplacement du projet dans un autre dossier : aucune forme périmée n'est
    // recopiée, les chemins sont recalculés pour le nouveau dossier.
    const std::string movedDir = path("sous-dossier");
    ensureDir(movedDir);
    const std::string movedPath = (fs::path(movedDir) / "deplace.svpproj.json").string();
    const SaveResult moved = saveProject(third.project, movedPath);
    SVP_CHECK(moved.ok);
    const LoadResult movedBack = loadProject(movedPath);
    SVP_CHECK(movedBack.ok);
    SVP_CHECK(movedBack.project.tracks[0].clips[0].storedMediaPath != "media/voix.wav");
    SVP_CHECK(movedBack.project.tracks[0].clips[0].mediaPath
              == canonical(path("media/voix.wav")));  // les médias ne bougent pas
}

SVP_TEST(project, json_stable_order)
{
    prepareMedia();
    const Project original = buildReferenceProject(tmpDir());
    const std::string first = toJson(original, tmpDir());
    const std::string second = toJson(original, tmpDir());
    SVP_CHECK(first == second);
    // Ordre des champs : schema, schemaVersion, puis tracks en dernier.
    const std::size_t schema = first.find("\"schema\"");
    const std::size_t version = first.find("\"schemaVersion\"");
    const std::size_t tracks = first.find("\"tracks\"");
    SVP_CHECK(schema != std::string::npos);
    SVP_CHECK(version != std::string::npos);
    SVP_CHECK(tracks != std::string::npos);
    SVP_CHECK(schema < version);
    SVP_CHECK(version < tracks);
    SVP_CHECK(first.rfind("{\n  \"schema\"", 0) == 0);  // version de schéma en tête
    // Fin de ligne « \n » uniquement : un diff entre Windows et Linux reste vide.
    SVP_CHECK(first.find('\r') == std::string::npos);
    SVP_CHECK(!first.empty() && first.back() == '\n');
    // En-tête exploitable par un humain et par un lecteur tiers.
    SVP_CHECK(first.find("studio-voice-pro.project") != std::string::npos);
    SVP_CHECK(first.find("\"mediaPathBase\": \"projectFile\"") != std::string::npos);
    // Les positions sont des entiers JSON (pas « 9.007199254740993e+15 »).
    SVP_CHECK(first.find("\"lengthFrames\": 9007199254740993") != std::string::npos);
}

SVP_TEST(project, exact_numbers)
{
    Project project = makeProject("Précision", 44100, 24, 90.0);
    Track track = makeTrack(project, "Mesures");
    const double values[] = {-6.0, -0.3, 1.0 / 3.0, 0.1 + 0.2, 1e-9, -1e-9, 1234.5678901234567,
                             1e15, 2.220446049250313e-16};
    for (double value : values) {
        Clip clip;
        clip.id = std::string("c") + std::to_string(project.tracks.size()) + "_"
                  + std::to_string(track.clips.size());
        clip.name = "clip";
        clip.mediaPath = canonical(path("media/voix.wav"));
        clip.gainDb = value;
        clip.fadeInSeconds = value;
        clip.startFrames = 9007199254740993LL;
        track.clips.push_back(clip);
    }
    project.tracks.push_back(track);
    const std::string json = toJson(project, tmpDir());
    const LoadResult loaded = fromJson(json, tmpDir());
    SVP_CHECK(loaded.ok);
    if (!loaded.ok) {
        std::printf("      %s\n", loaded.error.c_str());
        return;
    }
    SVP_CHECK(loaded.project.tracks.size() == 1);
    SVP_CHECK(loaded.project.tracks[0].clips.size() == sizeof(values) / sizeof(values[0]));
    for (std::size_t i = 0; i < loaded.project.tracks[0].clips.size(); ++i) {
        const Clip& clip = loaded.project.tracks[0].clips[i];
        SVP_CHECK(doublesBitEqual(clip.gainDb, values[i]));
        SVP_CHECK(doublesBitEqual(clip.fadeInSeconds, values[i]));
        SVP_CHECK(clip.startFrames == 9007199254740993LL);
    }
}

SVP_TEST(project, paths_relative_and_absolute)
{
    prepareMedia();
    const Project original = buildReferenceProject(tmpDir());
    const std::string json = toJson(original, tmpDir());
    // Média dans le dossier du projet : chemin relatif, séparateur « / ».
    SVP_CHECK(json.find("\"mediaPath\": \"media/voix.wav\"") != std::string::npos);
    // Nom de fichier avec espace conservé.
    SVP_CHECK(json.find("media/guitare 01.wav") != std::string::npos);
    // Média hors du dossier du projet : repli sur l'absolu.
    SVP_CHECK(json.find("hors-projet.wav") != std::string::npos);
    SVP_CHECK(json.find("\"mediaPath\": \"/") != std::string::npos);
    SVP_CHECK(json.find("svp-project-media-externe") != std::string::npos);
    // La résolution redonne un fichier réellement présent.
    const std::string resolvedPath = resolveStoredMediaPath("media/voix.wav", tmpDir());
    SVP_CHECK(fileExists(resolvedPath));
    SVP_CHECK(resolveStoredMediaPath("media/voix.wav", tmpDir())
              == resolveStoredMediaPath("media/voix.wav", tmpDir()));
    SVP_CHECK(fs::weakly_canonical(fs::path(resolvedPath)) == fs::weakly_canonical(fs::path(path("media/voix.wav"))));
    SVP_CHECK(resolveStoredMediaPath("/absolu/x.wav", "/tmp/autre") == "/absolu/x.wav");
}

SVP_TEST(project, missing_media_is_reported)
{
    prepareMedia();
    const std::string projectPath = path("média-manquant.svpproj.json");
    Project project = makeProject("Média manquant", 48000, 32);
    Track track = makeTrack(project, "Voix");
    Clip clip;
    clip.id = makeClipId(0, 0);
    clip.name = "disparu";
    clip.mediaPath = canonical(path("media/sera-supprime.wav"));
    clip.lengthFrames = 4800;
    track.clips.push_back(clip);
    project.tracks.push_back(track);
    writeClick(path("media/sera-supprime.wav"), 100);
    SVP_CHECK(saveProject(project, projectPath).ok);
    // Le média disparaît après l'enregistrement (disque amovible, dossier déplacé…).
    std::error_code ec;
    fs::remove(fs::path(path("media/sera-supprime.wav")), ec);
    const LoadResult loaded = loadProject(projectPath);
    SVP_CHECK(loaded.ok);                        // le projet s'ouvre malgré tout
    SVP_CHECK(loaded.missingMedia.size() == 1);  // et le média est signalé
    SVP_CHECK(loaded.project.tracks[0].clips.size() == 1);
    SVP_CHECK(loaded.project.tracks[0].clips[0].mediaMissing);
    SVP_CHECK(loaded.project.tracks[0].clips[0].lengthFrames == 4800);
    SVP_CHECK(loaded.project.tracks[0].clips[0].name == "disparu");
    const std::string dump = dumpProject(loaded.project, loaded, projectPath);
    SVP_CHECK(dump.find("MÉDIA MANQUANT") != std::string::npos);
    SVP_CHECK(dump.find("médias manquants") != std::string::npos);
}

SVP_TEST(project, corrupt_input_reported)
{
    prepareMedia();
    const std::string good = toJson(buildReferenceProject(tmpDir()), tmpDir());
    const std::string broken[] = {
        "",                                                       // fichier vide
        good.substr(0, good.size() / 2),                          // tronqué
        "ceci n'est pas du JSON",                                 // texte quelconque
        "{\"schema\": \"studio-voice-pro.project\", }",           // virgule finale
        "[1, 2, 3]",                                              // racine non objet
        "{\"schema\": \"autre-logiciel.project\", \"schemaVersion\": 1, \"tracks\": []}",
        "{\"schema\": \"studio-voice-pro.project\", \"schemaVersion\": \"1\", \"tracks\": []}",
        "{\"schema\": \"studio-voice-pro.project\", \"schemaVersion\": 1}",  // tracks absent
        "{\"schema\": \"studio-voice-pro.project\", \"schemaVersion\": 1, \"tracks\": {}}",
        "{\"schema\": \"studio-voice-pro.project\", \"schemaVersion\": 1, \"tracks\": [42]}",
        "{\"schema\": \"studio-voice-pro.project\", \"schemaVersion\": 1, \"tracks\": "
        "[{\"params\": {\"gainDb\": \"fort\"}}]}",
        "{\"schema\": \"studio-voice-pro.project\", \"schemaVersion\": 1, \"tracks\": "
        "[{\"clips\": [{\"startFrames\": 1.5, \"mediaPath\": \"x.wav\"}]}]}",
        "{\"schema\": \"studio-voice-pro.project\", \"schemaVersion\": 1, \"tracks\": "
        "[{\"clips\": [{\"startFrames\": 9223372036854775808}]}]}",  // dépasse int64
        "{\"schema\": \"studio-voice-pro.project\", \"schemaVersion\": 1, \"tracks\": "
        "[{\"clips\": [[1, 2]]}]}",
    };
    for (const std::string& text : broken) {
        const LoadResult loaded = fromJson(text, tmpDir());
        SVP_CHECK(!loaded.ok);            // refusé…
        SVP_CHECK(!loaded.error.empty());  // …avec un message exploitable
        SVP_CHECK(loaded.project.tracks.empty());
        if (!loaded.ok && loaded.error.empty())
            std::printf("      message d'erreur vide\n");
    }
}

SVP_TEST(project, unknown_schema_version)
{
    const std::string newer =
        "{\"schema\": \"studio-voice-pro.project\", \"schemaVersion\": 999, \"tracks\": []}";
    const LoadResult future = fromJson(newer, tmpDir());
    SVP_CHECK(!future.ok);
    SVP_CHECK(future.error.find("999") != std::string::npos);
    SVP_CHECK(future.error.find("inconnue") != std::string::npos);

    const LoadResult older = fromJson(
        "{\"schema\": \"studio-voice-pro.project\", \"schemaVersion\": 0, \"tracks\": []}",
        tmpDir());
    SVP_CHECK(!older.ok);
    SVP_CHECK(!older.error.empty());

    // Version courante et champ inconnu : CHARGE avec un avertissement (règle de
    // compatibilité ascendante — un champ ajouté par une version ultérieure ne
    // doit pas rendre le projet illisible).
    const LoadResult known = fromJson(
        "{\"schema\": \"studio-voice-pro.project\", \"schemaVersion\": 1, \"name\": \"v1\", "
        "\"mixer\": {\"a\": 1}, \"tracks\": [{\"id\": \"t1\", \"name\": \"Piste\", "
        "\"fx\": []}]}",
        tmpDir());
    SVP_CHECK(known.ok);
    SVP_CHECK(!known.warnings.empty());
    SVP_CHECK(known.project.name == "v1");
    SVP_CHECK(known.project.tracks.size() == 1);

    // Version absente : refus explicite (pas de supposition silencieuse).
    const LoadResult noVersion =
        fromJson("{\"schema\": \"studio-voice-pro.project\", \"tracks\": []}", tmpDir());
    SVP_CHECK(!noVersion.ok);
    SVP_CHECK(noVersion.error.find("schemaVersion") != std::string::npos);
}

SVP_TEST(project, atomic_save_never_destroys_previous)
{
    prepareMedia();
    const std::string projectPath = path("atomique.svpproj.json");
    Project first = makeProject("Version 1", 48000, 32);
    first.tracks.push_back(makeTrack(first, "Voix"));
    SVP_CHECK(saveProject(first, projectPath).ok);
    const std::string before = readTextFile(projectPath);
    SVP_CHECK(!before.empty());

    // Coupure simulée JUSTE AVANT le renommage (l'écriture du temporaire a
    // pourtant eu lieu).
    Project second = first;
    second.name = "Version 2";
    second.tracks[0].params.gainDb = -12.0;
    setProjectIoTestHooks(TestHooks{true, false});
    const SaveResult interrupted = saveProject(second, projectPath);
    setProjectIoTestHooks(TestHooks{});
    SVP_CHECK(!interrupted.ok);
    SVP_CHECK(!interrupted.error.empty());
    SVP_CHECK(fileExists(projectPath + ".tmp"));     // l'écriture avait bien commencé
    SVP_CHECK(readTextFile(projectPath) == before);  // projet intact, à l'octet près
    const LoadResult stillFirst = loadProject(projectPath);
    SVP_CHECK(stillFirst.ok);
    SVP_CHECK(stillFirst.project.name == "Version 1");
    SVP_CHECK(stillFirst.project.tracks[0].params.gainDb == 0.0);

    // Échec d'écriture du temporaire : rien n'est touché non plus.
    std::error_code cleanup;
    fs::remove(fs::path(projectPath + ".tmp"), cleanup);  // nettoyage de la coupure simulée
    setProjectIoTestHooks(TestHooks{false, true});
    const SaveResult failedWrite = saveProject(second, projectPath);
    setProjectIoTestHooks(TestHooks{});
    SVP_CHECK(!failedWrite.ok);
    SVP_CHECK(readTextFile(projectPath) == before);
    SVP_CHECK(!fileExists(projectPath + ".tmp"));

    // Sauvegarde nominale : le nouveau contenu remplace l'ancien, et aucun
    // fichier temporaire ne subsiste.
    const SaveResult ok = saveProject(second, projectPath);
    SVP_CHECK(ok.ok);
    SVP_CHECK(!fileExists(projectPath + ".tmp"));
    const LoadResult nowSecond = loadProject(projectPath);
    SVP_CHECK(nowSecond.ok);
    SVP_CHECK(nowSecond.project.name == "Version 2");
    SVP_CHECK(nowSecond.project.tracks[0].params.gainDb == -12.0);
    SVP_CHECK(ok.tempPath == projectPath + ".tmp");  // même dossier que la cible
}

SVP_TEST(project, autosave_is_a_separate_file)
{
    prepareMedia();
    const std::string projectPath = path("auto.svpproj.json");
    Project project = makeProject("Autosauvegarde", 48000, 32);
    project.tracks.push_back(makeTrack(project, "Voix"));
    SVP_CHECK(saveProject(project, projectPath).ok);
    const std::string projectBefore = readTextFile(projectPath);

    Project modified = project;
    modified.tracks[0].params.gainDb = -3.5;
    const SaveResult autosaved = saveAutosave(modified, projectPath);
    SVP_CHECK(autosaved.ok);
    SVP_CHECK(autosavePathFor(projectPath) != projectPath);
    SVP_CHECK(autosavePathFor(projectPath) == projectPath + ".autosave.json");
    SVP_CHECK(fileExists(autosavePathFor(projectPath)));
    // Le fichier de projet n'a PAS été touché par la sauvegarde automatique.
    SVP_CHECK(readTextFile(projectPath) == projectBefore);

    const LoadResult recovered = loadAutosave(projectPath);
    SVP_CHECK(recovered.ok);
    SVP_CHECK(recovered.fromAutosave);
    SVP_CHECK(recovered.project.tracks[0].params.gainDb == -3.5);

    // Sauvegarde volontaire avec copie automatique demandée.
    const SaveResult both =
        saveProject(modified, projectPath, SaveOptions{true});
    SVP_CHECK(both.ok);
    SVP_CHECK(!both.autosavePath.empty());
    SVP_CHECK(hasAutosave(projectPath));
    SVP_CHECK(removeAutosave(projectPath));
    SVP_CHECK(!hasAutosave(projectPath));
    SVP_CHECK(loadAutosave(projectPath).ok == false);
}

SVP_TEST(project, preferences_file)
{
    const std::string configDir = path("config");
    ensureDir(configDir);
    const std::string prefsPath = (fs::path(configDir) / "preferences.json").string();
    std::error_code prefsCleanup;
    fs::remove(fs::path(prefsPath), prefsCleanup);  // état propre entre deux exécutions

    // Fichier absent : valeurs par défaut, ce n'est pas une erreur.
    const PreferencesLoadResult missing = loadPreferences(prefsPath);
    SVP_CHECK(missing.ok);
    SVP_CHECK(!missing.fileExisted);
    SVP_CHECK(missing.preferences.defaultSampleRate == 48000);
    SVP_CHECK(missing.preferences.audioDeviceType == "ASIO");

    Preferences preferences = defaultPreferences();
    preferences.defaultSampleRate = 96000;
    preferences.defaultBitDepth = 24;
    preferences.audioBufferSizeFrames = 128;
    preferences.autosaveIntervalSeconds = 300;
    preferences.defaultProjectDirectory = path("projets");
    preferences.defaultMediaDirectory = path("media");
    preferences.audioDeviceType = "WASAPI";
    const SaveResult saved = savePreferences(preferences, prefsPath);
    SVP_CHECK(saved.ok);
    SVP_CHECK(fileExists(prefsPath));

    const PreferencesLoadResult reloaded = loadPreferences(prefsPath);
    SVP_CHECK(reloaded.ok);
    SVP_CHECK(reloaded.fileExisted);
    SVP_CHECK(reloaded.preferences.defaultSampleRate == 96000);
    SVP_CHECK(reloaded.preferences.defaultBitDepth == 24);
    SVP_CHECK(reloaded.preferences.audioBufferSizeFrames == 128);
    SVP_CHECK(reloaded.preferences.autosaveIntervalSeconds == 300);
    SVP_CHECK(reloaded.preferences.audioDeviceType == "WASAPI");
    SVP_CHECK(reloaded.preferences.defaultProjectDirectory == path("projets"));

    // Un nouveau projet hérite des préférences (sortie par défaut + durée).
    const Project fresh = newProjectFromPreferences(reloaded.preferences, "Hérité");
    SVP_CHECK(fresh.sampleRate == 96000);
    SVP_CHECK(fresh.bitDepth == 24);
    SVP_CHECK(fresh.tracks.empty());

    // Préférences corrompues : signalées, jamais un plantage.
    std::ofstream out(prefsPath, std::ios::binary | std::ios::trunc);
    out << "{ ceci n'est pas un JSON ";
    out.close();
    const PreferencesLoadResult broken = loadPreferences(prefsPath);
    SVP_CHECK(!broken.ok);
    SVP_CHECK(!broken.error.empty());
    // Une profondeur inconnue est réparée avec un avertissement.
    std::ofstream out2(prefsPath, std::ios::binary | std::ios::trunc);
    out2 << "{\"schemaVersion\": 1, \"defaultBitDepth\": 7, \"defaultSampleRate\": 123}";
    out2.close();
    const PreferencesLoadResult repaired = loadPreferences(prefsPath);
    SVP_CHECK(repaired.ok);
    SVP_CHECK(!repaired.warnings.empty());
    const Project repairedProject = newProjectFromPreferences(repaired.preferences, "Réparé");
    SVP_CHECK(repairedProject.bitDepth == 32);
    SVP_CHECK(repairedProject.sampleRate == 48000);
    // SVP_CONFIG_DIR dirige l'emplacement par défaut.
    SVP_CHECK(defaultPreferencesPath().find("preferences.json") != std::string::npos);
}

SVP_TEST(project, validation_refuses_bad_project)
{
    prepareMedia();
    const std::string projectPath = path("invalide.svpproj.json");
    Project project = makeProject("Validation", 48000, 32);
    Track track = makeTrack(project, "Voix");
    Clip clip;
    clip.id = makeClipId(0, 0);
    clip.name = "clip";
    clip.mediaPath = canonical(path("media/voix.wav"));
    clip.lengthFrames = 48000;
    track.clips.push_back(clip);
    project.tracks.push_back(track);

    std::string error;
    SVP_CHECK(validateProject(project, error));

    Project badPan = project;
    badPan.tracks[0].params.pan = 2.0;
    SVP_CHECK(!validateProject(badPan, error));
    SVP_CHECK(!saveProject(badPan, projectPath).ok);
    SVP_CHECK(!fileExists(projectPath));  // rien n'a été écrit

    Project nanGain = project;
    nanGain.tracks[0].params.gainDb = std::nan("");
    SVP_CHECK(!validateProject(nanGain, error));

    Project emptyName = project;
    emptyName.name.clear();
    SVP_CHECK(!validateProject(emptyName, error));

    Project badRate = project;
    badRate.sampleRate = 0;
    SVP_CHECK(!validateProject(badRate, error));

    Project duplicate = project;
    const Clip clone = duplicate.tracks[0].clips[0];
    duplicate.tracks[0].clips.push_back(clone);  // identifiant de clip en double
    SVP_CHECK(!validateProject(duplicate, error));
    SVP_CHECK(error.find("double") != std::string::npos);

    Project longFade = project;
    longFade.tracks[0].clips[0].fadeInSeconds = 10.0;
    SVP_CHECK(!validateProject(longFade, error));

    Project negative = project;
    negative.tracks[0].clips[0].startFrames = -1;
    SVP_CHECK(!validateProject(negative, error));
}

SVP_TEST(project, dump_shows_state)
{
    prepareMedia();
    const std::string projectPath = path("dump.svpproj.json");
    const Project original = buildReferenceProject(tmpDir());
    SVP_CHECK(saveProject(original, projectPath).ok);
    const LoadResult loaded = loadProject(projectPath);
    SVP_CHECK(loaded.ok);
    const std::string dump = dumpProject(loaded.project, loaded, projectPath);
    SVP_CHECK(dump.find("Session de contrôle") != std::string::npos);
    SVP_CHECK(dump.find("Voix principale") != std::string::npos);
    SVP_CHECK(dump.find("prise 1") != std::string::npos);
    SVP_CHECK(dump.find("gain -6.000 dB") != std::string::npos);
    SVP_CHECK(dump.find("sortie bus guitares") != std::string::npos);
    SVP_CHECK(dump.find("studio-voice-pro.project v1") != std::string::npos);
    SVP_CHECK(dump.find("pistes            : 3") != std::string::npos);
    SVP_CHECK(dump.find("clips             : 4") != std::string::npos);
    SVP_CHECK(projectEndFrames(loaded.project) >= 9007199254740993LL);
}

SVP_TEST(project, json_reader_details)
{
    // Le lecteur JSON maison : échappements, nombres, structures vides.
    const svp::json::ParseResult parsed = svp::json::parse(
        "{\"a\": \"ligne\\n\\ttab\\\"fin\\u00e9\", \"b\": [1, 2.5, -3e2, true, false, null], "
        "\"c\": {}, \"d\": []}");
    SVP_CHECK(parsed.ok);
    if (!parsed.ok) {
        std::printf("      %s\n", parsed.error.c_str());
        return;
    }
    SVP_CHECK(parsed.value.find("a")->text == "ligne\n\ttab\"finé");
    const svp::json::Value* array = parsed.value.find("b");
    SVP_CHECK(array != nullptr && array->size() == 6);
    SVP_CHECK(array->item(0)->isInteger() && array->item(0)->integer == 1);
    SVP_CHECK(!array->item(1)->isInteger());
    SVP_CHECK(array->item(1)->number == 2.5);
    SVP_CHECK(array->item(2)->number == -300.0);
    SVP_CHECK(array->item(3)->boolOr(false));
    SVP_CHECK(array->item(4)->isBool() && !array->item(4)->boolean);
    SVP_CHECK(array->item(5)->isNull());
    SVP_CHECK(parsed.value.find("c")->isObject() && parsed.value.find("c")->size() == 0);
    SVP_CHECK(parsed.value.find("d")->isArray() && parsed.value.find("d")->size() == 0);
    SVP_CHECK(parsed.value.find("absente") == nullptr);

    // Erreurs de syntaxe : refusées avec un message situé.
    SVP_CHECK(!svp::json::parse("{\"a\": 1 \"b\": 2}").ok);
    SVP_CHECK(!svp::json::parse("{\"a\": 01}").ok);
    SVP_CHECK(!svp::json::parse("{\"a\": NaN}").ok);
    SVP_CHECK(!svp::json::parse("[1, 2] extra").ok);
    SVP_CHECK(svp::json::parse("[1, 2]").ok);

    // Échappement à l'écriture (chemin Windows, guillemets, accent).
    const std::string escaped = svp::json::escape("C:\\Users\\année \"1\"\\voix.wav");
    SVP_CHECK(escaped.find("\\\\") != std::string::npos);
    SVP_CHECK(escaped.find("\\\"") != std::string::npos);
    SVP_CHECK(escaped.find("année") != std::string::npos);
}
