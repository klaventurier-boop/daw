// Studio Voice Pro — mixage de pistes avec mute/solo/gain/pan.
// Chemin temps réel : aucune allocation, aucun verrou, aucun accès disque.
#include "svp/dsp/Mixer.h"

#include <algorithm>

#include "svp/dsp/GainPan.h"

namespace svp::dsp {

bool isChannelAudible(const ChannelStrip& strip, bool anySolo) noexcept
{
    if (strip.mute)
        return false;
    if (anySolo && !strip.solo)
        return false;
    return true;
}

bool anySoloActive(std::span<const ChannelStrip> strips) noexcept
{
    return std::any_of(strips.begin(), strips.end(), [](const ChannelStrip& s) { return s.solo; });
}

void applyStripTo(audio::AudioBufferConstView source, audio::AudioBufferView destination,
                  const ChannelStrip& strip, bool anySolo) noexcept
{
    const int frames = std::min(source.frames(), destination.frames());
    const int channels = std::min(source.channels(), destination.channels());

    if (!isChannelAudible(strip, anySolo)) {
        // Piste inaudible : c'est la DESTINATION qui reçoit le silence.
        // La source (média) n'est jamais touchée.
        for (int ch = 0; ch < destination.channels(); ++ch) {
            float* out = destination.channel(ch);
            if (out == nullptr)
                continue;
            std::fill(out, out + destination.frames(), 0.0f);
        }
        return;
    }

    // Gain constant de la piste, puis panoramique : la lecture de la source et
    // l'écriture de la destination sont séparées, canal par canal.
    float left = 1.0f;
    float right = 1.0f;
    panGains(strip.pan, left, right);

    for (int ch = 0; ch < channels; ++ch) {
        const float* in = source.channel(ch);
        float* out = destination.channel(ch);
        if (in == nullptr || out == nullptr)
            continue;

        float channelPan = 1.0f;
        if (destination.channels() > 1) {
            if (ch == 0)
                channelPan = left;
            else if (ch == 1)
                channelPan = right;
            else
                channelPan = 0.5f * (left + right);
        }
        const float gain = strip.gain * channelPan;
        for (int i = 0; i < frames; ++i)
            out[i] = in[i] * gain;
    }

    // Canaux de destination sans source correspondante : silence explicite
    // (jamais de mémoire non initialisée remise au pilote).
    for (int ch = channels; ch < destination.channels(); ++ch) {
        float* out = destination.channel(ch);
        if (out == nullptr)
            continue;
        std::fill(out, out + destination.frames(), 0.0f);
    }
}

void mixStrips(audio::AudioBufferView destination,
               std::span<const audio::AudioBuffer* const> sources,
               std::span<const ChannelStrip> strips) noexcept
{
    destination.clear();
    const bool anySolo = anySoloActive(strips);

    const std::size_t count = std::min(sources.size(), strips.size());
    for (std::size_t s = 0; s < count; ++s) {
        const audio::AudioBuffer* source = sources[s];
        if (source == nullptr || source->empty())
            continue;

        const ChannelStrip& strip = strips[s];
        if (!isChannelAudible(strip, anySolo))
            continue;

        float left = 1.0f;
        float right = 1.0f;
        panGains(strip.pan, left, right);

        const int destChannels = destination.channels();
        const int frames = std::min(destination.frames(), source->frames());

        for (int ch = 0; ch < destChannels; ++ch) {
            // Une source mono alimente tous les canaux de destination (placement
            // stéréo par le panoramique) ; une source multicanal est alignée
            // canal à canal avec la destination.
            const int sourceChannel = (source->channels() == 1)
                                          ? 0
                                          : std::min(ch, source->channels() - 1);

            float channelPan = 1.0f;
            if (destChannels > 1) {
                if (ch == 0)
                    channelPan = left;
                else if (ch == 1)
                    channelPan = right;
                else
                    channelPan = 0.5f * (left + right);
            }

            const float gain = strip.gain * channelPan;
            const float* in = source->channel(sourceChannel); // lecture seule : média jamais modifié
            float* out = destination.channel(ch);
            if (in == nullptr || out == nullptr)
                continue;
            for (int i = 0; i < frames; ++i)
                out[i] += in[i] * gain;
        }
    }
}

} // namespace svp::dsp
