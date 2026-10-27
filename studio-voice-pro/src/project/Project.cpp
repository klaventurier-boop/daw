#include "svp/project/Project.h"
#include <charconv>
#include <cmath>
#include <set>
#include <string>

namespace svp::project {
namespace {

/** Représentation exacte et lisible d'un double dans les messages de
    diagnostic (même règle que le JSON : la plus courte qui se relit). */
std::string show(double value)
{
    char buffer[64];
    const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
    return std::string(buffer, result.ptr);
}

std::string quote(const std::string& text)
{
    return "« " + text + " »";
}

bool numbersEqual(double a, double b) noexcept
{
    // Comparaison STRICTE : la fidélité du projet est binaire. Le seul cas
    // particulier géré est -0.0 == +0.0 (même valeur, deux écritures).
    if (a == b)
        return true;
    return false;
}

} // namespace

bool operator==(const TrackParams& a, const TrackParams& b) noexcept
{
    return numbersEqual(a.gainDb, b.gainDb) && numbersEqual(a.pan, b.pan) && a.mute == b.mute
           && a.solo == b.solo;
}

bool operator==(const Clip& a, const Clip& b) noexcept
{
    return a.id == b.id && a.name == b.name && a.mediaPath == b.mediaPath
           && a.startFrames == b.startFrames && a.sourceOffsetFrames == b.sourceOffsetFrames
           && a.lengthFrames == b.lengthFrames && numbersEqual(a.gainDb, b.gainDb)
           && numbersEqual(a.fadeInSeconds, b.fadeInSeconds)
           && numbersEqual(a.fadeOutSeconds, b.fadeOutSeconds)
           && a.mediaMissing == b.mediaMissing;
}

bool operator==(const Track& a, const Track& b) noexcept
{
    return a.id == b.id && a.name == b.name && a.color == b.color && a.input == b.input
           && a.output == b.output && a.params == b.params && a.clips == b.clips;
}

bool operator==(const Project& a, const Project& b) noexcept
{
    return a.schemaVersion == b.schemaVersion && a.appVersion == b.appVersion && a.name == b.name
           && a.sampleRate == b.sampleRate && a.bitDepth == b.bitDepth
           && numbersEqual(a.tempoBpm, b.tempoBpm) && a.lengthFrames == b.lengthFrames
           && a.tracks == b.tracks;
}

std::string firstDifference(const Project& a, const Project& b)
{
    if (a.schemaVersion != b.schemaVersion)
        return "schemaVersion : " + std::to_string(a.schemaVersion) + " ≠ "
               + std::to_string(b.schemaVersion);
    if (a.appVersion != b.appVersion)
        return "appVersion : " + quote(a.appVersion) + " ≠ " + quote(b.appVersion);
    if (a.name != b.name)
        return "name : " + quote(a.name) + " ≠ " + quote(b.name);
    if (a.sampleRate != b.sampleRate)
        return "sampleRate : " + std::to_string(a.sampleRate) + " ≠ "
               + std::to_string(b.sampleRate);
    if (a.bitDepth != b.bitDepth)
        return "bitDepth : " + std::to_string(a.bitDepth) + " ≠ " + std::to_string(b.bitDepth);
    if (!numbersEqual(a.tempoBpm, b.tempoBpm))
        return "tempoBpm : " + show(a.tempoBpm) + " ≠ " + show(b.tempoBpm);
    if (a.lengthFrames != b.lengthFrames)
        return "lengthFrames : " + std::to_string(a.lengthFrames) + " ≠ "
               + std::to_string(b.lengthFrames);
    if (a.tracks.size() != b.tracks.size())
        return "nombre de pistes : " + std::to_string(a.tracks.size()) + " ≠ "
               + std::to_string(b.tracks.size());
    for (std::size_t t = 0; t < a.tracks.size(); ++t) {
        const Track& ta = a.tracks[t];
        const Track& tb = b.tracks[t];
        const std::string prefix = "tracks[" + std::to_string(t) + "]";
        if (ta.id != tb.id)
            return prefix + ".id : " + quote(ta.id) + " ≠ " + quote(tb.id);
        if (ta.name != tb.name)
            return prefix + ".name : " + quote(ta.name) + " ≠ " + quote(tb.name);
        if (ta.color != tb.color)
            return prefix + ".color : " + quote(ta.color) + " ≠ " + quote(tb.color);
        if (ta.input != tb.input)
            return prefix + ".input : " + quote(ta.input) + " ≠ " + quote(tb.input);
        if (ta.output != tb.output)
            return prefix + ".output : " + quote(ta.output) + " ≠ " + quote(tb.output);
        if (!numbersEqual(ta.params.gainDb, tb.params.gainDb))
            return prefix + ".params.gainDb : " + show(ta.params.gainDb) + " ≠ "
                   + show(tb.params.gainDb);
        if (!numbersEqual(ta.params.pan, tb.params.pan))
            return prefix + ".params.pan : " + show(ta.params.pan) + " ≠ "
                   + show(tb.params.pan);
        if (ta.params.mute != tb.params.mute)
            return prefix + ".params.mute : " + (ta.params.mute ? "true" : "false") + " ≠ "
                   + (tb.params.mute ? "true" : "false");
        if (ta.params.solo != tb.params.solo)
            return prefix + ".params.solo : " + (ta.params.solo ? "true" : "false") + " ≠ "
                   + (tb.params.solo ? "true" : "false");
        if (ta.clips.size() != tb.clips.size())
            return prefix + ".clips : " + std::to_string(ta.clips.size()) + " ≠ "
                   + std::to_string(tb.clips.size());
        for (std::size_t c = 0; c < ta.clips.size(); ++c) {
            const Clip& ca = ta.clips[c];
            const Clip& cb = tb.clips[c];
            const std::string clipPrefix = prefix + ".clips[" + std::to_string(c) + "]";
            if (ca.id != cb.id)
                return clipPrefix + ".id : " + quote(ca.id) + " ≠ " + quote(cb.id);
            if (ca.name != cb.name)
                return clipPrefix + ".name : " + quote(ca.name) + " ≠ " + quote(cb.name);
            if (ca.mediaPath != cb.mediaPath)
                return clipPrefix + ".mediaPath : " + quote(ca.mediaPath) + " ≠ "
                       + quote(cb.mediaPath);
            if (ca.startFrames != cb.startFrames)
                return clipPrefix + ".startFrames : " + std::to_string(ca.startFrames) + " ≠ "
                       + std::to_string(cb.startFrames);
            if (ca.sourceOffsetFrames != cb.sourceOffsetFrames)
                return clipPrefix + ".sourceOffsetFrames : "
                       + std::to_string(ca.sourceOffsetFrames) + " ≠ "
                       + std::to_string(cb.sourceOffsetFrames);
            if (ca.lengthFrames != cb.lengthFrames)
                return clipPrefix + ".lengthFrames : " + std::to_string(ca.lengthFrames) + " ≠ "
                       + std::to_string(cb.lengthFrames);
            if (!numbersEqual(ca.gainDb, cb.gainDb))
                return clipPrefix + ".gainDb : " + show(ca.gainDb) + " ≠ " + show(cb.gainDb);
            if (!numbersEqual(ca.fadeInSeconds, cb.fadeInSeconds))
                return clipPrefix + ".fadeInSeconds : " + show(ca.fadeInSeconds) + " ≠ "
                       + show(cb.fadeInSeconds);
            if (!numbersEqual(ca.fadeOutSeconds, cb.fadeOutSeconds))
                return clipPrefix + ".fadeOutSeconds : " + show(ca.fadeOutSeconds) + " ≠ "
                       + show(cb.fadeOutSeconds);
            if (ca.mediaMissing != cb.mediaMissing)
                return clipPrefix + ".mediaMissing : " + (ca.mediaMissing ? "true" : "false")
                       + " ≠ " + (cb.mediaMissing ? "true" : "false");
        }
    }
    return {};
}

bool validateProject(const Project& project, std::string& error)
{
    if (project.schemaVersion != kSchemaVersion) {
        error = "version de schéma non pris en charge à l'écriture : "
                + std::to_string(project.schemaVersion);
        return false;
    }
    if (project.name.empty()) {
        error = "le projet doit avoir un nom";
        return false;
    }
    if (project.sampleRate < 8000 || project.sampleRate > 768000) {
        error = "taux d'échantillonnage invalide : " + std::to_string(project.sampleRate);
        return false;
    }
    if (project.bitDepth != 16 && project.bitDepth != 24 && project.bitDepth != 32) {
        error = "profondeur invalide : " + std::to_string(project.bitDepth) + " (16, 24 ou 32)";
        return false;
    }
    if (!std::isfinite(project.tempoBpm) || project.tempoBpm <= 0.0 || project.tempoBpm > 1000.0) {
        error = "tempo invalide : " + show(project.tempoBpm);
        return false;
    }
    if (project.lengthFrames < 0) {
        error = "lengthFrames négatif";
        return false;
    }
    std::set<std::string> trackIds;
    std::set<std::string> ids;
    for (std::size_t t = 0; t < project.tracks.size(); ++t) {
        const Track& track = project.tracks[t];
        const std::string prefix = "tracks[" + std::to_string(t) + "]";
        if (track.id.empty()) {
            error = prefix + " : identifiant de piste vide";
            return false;
        }
        if (!trackIds.insert(track.id).second) {
            error = prefix + " : identifiant de piste en double « " + track.id + " »";
            return false;
        }
        if (track.name.empty()) {
            error = prefix + " : nom de piste vide";
            return false;
        }
        if (track.output.empty()) {
            error = prefix + " : routage de sortie vide";
            return false;
        }
        if (!std::isfinite(track.params.gainDb) || std::abs(track.params.gainDb) > 200.0) {
            error = prefix + ".params.gainDb hors plage : " + show(track.params.gainDb);
            return false;
        }
        if (!std::isfinite(track.params.pan) || track.params.pan < -1.0 || track.params.pan > 1.0) {
            error = prefix + ".params.pan hors plage [-1, 1] : " + show(track.params.pan);
            return false;
        }
        for (std::size_t c = 0; c < track.clips.size(); ++c) {
            const Clip& clip = track.clips[c];
            const std::string clipPrefix = prefix + ".clips[" + std::to_string(c) + "]";
            if (clip.id.empty()) {
                error = clipPrefix + " : identifiant de clip vide";
                return false;
            }
            if (!ids.insert(clip.id).second) {
                error = clipPrefix + " : identifiant de clip en double « " + clip.id + " »";
                return false;
            }
            if (clip.mediaPath.empty()) {
                error = clipPrefix + " : chemin média vide";
                return false;
            }
            if (clip.startFrames < 0 || clip.sourceOffsetFrames < 0 || clip.lengthFrames < 0) {
                error = clipPrefix + " : position négative";
                return false;
            }
            if (!std::isfinite(clip.gainDb) || !std::isfinite(clip.fadeInSeconds)
                || !std::isfinite(clip.fadeOutSeconds)) {
                error = clipPrefix + " : valeur non finie (gain ou fondu)";
                return false;
            }
            if (clip.fadeInSeconds < 0.0 || clip.fadeOutSeconds < 0.0) {
                error = clipPrefix + " : fondu négatif";
                return false;
            }
            if (clip.lengthFrames > 0) {
                const double clipSeconds =
                    static_cast<double>(clip.lengthFrames) / static_cast<double>(project.sampleRate);
                if (clip.fadeInSeconds + clip.fadeOutSeconds > clipSeconds) {
                    error = clipPrefix + " : fondus plus longs que le clip ("
                            + show(clip.fadeInSeconds + clip.fadeOutSeconds) + " s > "
                            + show(clipSeconds) + " s)";
                    return false;
                }
            }
        }
    }
    return true;
}

std::size_t clipCount(const Project& project) noexcept
{
    std::size_t total = 0;
    for (const Track& track : project.tracks)
        total += track.clips.size();
    return total;
}

int64_t projectEndFrames(const Project& project) noexcept
{
    int64_t end = project.lengthFrames;
    for (const Track& track : project.tracks) {
        for (const Clip& clip : track.clips) {
            const int64_t clipEnd = clip.startFrames + clip.lengthFrames;
            if (clipEnd > end)
                end = clipEnd;
        }
    }
    return end;
}

std::string makeTrackId(std::size_t trackIndex)
{
    return "t" + std::to_string(trackIndex + 1);
}

std::string makeClipId(std::size_t trackIndex, std::size_t clipIndex)
{
    return makeTrackId(trackIndex) + "c" + std::to_string(clipIndex + 1);
}

Project makeProject(const std::string& name, int sampleRate, int bitDepth, double tempoBpm)
{
    Project project;
    project.name = name.empty() ? "Sans titre" : name;
    project.sampleRate = sampleRate;
    project.bitDepth = bitDepth;
    project.tempoBpm = tempoBpm;
    return project;
}

Track makeTrack(const Project& project, const std::string& name)
{
    Track track;
    track.id = makeTrackId(project.tracks.size());
    track.name = name.empty() ? ("Piste " + std::to_string(project.tracks.size() + 1)) : name;
    return track;
}

} // namespace svp::project
