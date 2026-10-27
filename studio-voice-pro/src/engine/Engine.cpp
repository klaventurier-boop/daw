// Studio Voice Pro — façade moteur (voir l'en-tête pour le contrat complet).
#include "svp/engine/Engine.h"

#include <algorithm>

#include "svp/dsp/Denormals.h"
#include "svp/dsp/Sanitize.h"

namespace svp::engine {

void Engine::prepare(double sampleRate, int maxBlockSize, int maxChannels, int maxSources)
{
    sampleRate_ = (sampleRate > 0.0) ? sampleRate : 48000.0;
    maxBlockSize_ = (maxBlockSize > 0) ? maxBlockSize : 512;
    maxChannels_ = (maxChannels > 0) ? maxChannels : 2;
    const int sourceSlots = (maxSources > 0) ? maxSources : 0;

    // 1. Tampon de travail : alloué ici, jamais redimensionné ensuite
    //    (voir la règle de propriété d'AudioBuffer : resize() est interdit
    //    après prepare()).
    work_.resize(maxChannels_, maxBlockSize_);
    work_.clear();

    // 2. Table de pointeurs de canaux du tampon de travail : le chemin temps
    //    réel n'a plus qu'à la lire, sans dériver ni allouer de pointeurs.
    workChannelPtrs_.assign(static_cast<std::size_t>(maxChannels_), nullptr);
    for (int ch = 0; ch < maxChannels_; ++ch)
        workChannelPtrs_[static_cast<std::size_t>(ch)] = work_.channel(ch);

    // 3. Contexte de mixage pré-alloué : pointeurs de sources + tranches.
    sourcePtrs_.assign(static_cast<std::size_t>(sourceSlots), nullptr);
    strips_.assign(static_cast<std::size_t>(sourceSlots), dsp::ChannelStrip{});
    sourceCount_ = 0;

    // 4. Mesureur.
    meter_.prepare(sampleRate_, maxBlockSize_);

    prepared_ = true;
}

void Engine::reset() noexcept
{
    meter_.reset();
    work_.clear();
}

void Engine::setSource(int index, const audio::AudioBuffer* source, const dsp::ChannelStrip& strip) noexcept
{
    if (index < 0 || index >= static_cast<int>(sourcePtrs_.size()))
        return;
    sourcePtrs_[static_cast<std::size_t>(index)] = source;
    strips_[static_cast<std::size_t>(index)] = strip;
    if (index + 1 > sourceCount_)
        sourceCount_ = index + 1; // les sources intermédiaires restent nulles (ignorées)
}

void Engine::setSourceCount(int count) noexcept
{
    const int maximum = static_cast<int>(sourcePtrs_.size());
    sourceCount_ = std::clamp(count, 0, maximum);
}

void Engine::process(audio::AudioBufferView destination) noexcept
{
    process(destination,
            std::span<const audio::AudioBuffer* const>(sourcePtrs_.data(),
                                                       static_cast<std::size_t>(sourceCount_)),
            std::span<const dsp::ChannelStrip>(strips_.data(),
                                               static_cast<std::size_t>(sourceCount_)));
}

void Engine::process(audio::AudioBufferView destination,
                     std::span<const audio::AudioBuffer* const> sources,
                     std::span<const dsp::ChannelStrip> strips) noexcept
{
    // 1. Hygiène numérique dès l'entrée : FTZ/DAZ pour tout ce bloc.
    //    (Restauration automatique de MXCSR en sortie de portée.)
    const dsp::ScopedNoDenormals noDenormals;

    if (destination.empty())
        return;

    if (!prepared_ || maxBlockSize_ <= 0 || maxChannels_ <= 0 || workChannelPtrs_.empty()) {
        // Moteur non préparé : on ne peut rien rendre, mais on rend un SILENCE
        // borné plutôt que de laisser de la mémoire non initialisée.
        destination.clear();
        return;
    }

    // 2. Taille de bloc réellement traitable : bornée à celle annoncée à
    //    prepare(). Au-delà, le reste de la vue est mis à zéro (aucune
    //    écriture hors bornes, aucun dépassement de tampon).
    const int frames = std::min(destination.frames(), maxBlockSize_);
    const int mixChannels = std::min(destination.channels(), maxChannels_);

    // 3. Vue sur le tampon de travail, construite sur la table de pointeurs
    //    pré-allouée : aucune allocation, aucune dérivation de pointeur.
    const audio::AudioBufferView block(workChannelPtrs_.data(), mixChannels, frames);

    // 4. Mixage : lit les médias (jamais modifiés), écrit dans le scratch.
    dsp::mixStrips(block, sources, strips);

    // 5. Mesure AVANT assainissement : c'est le seul endroit où un NaN ou un
    //    Inf encore présent peut être vu et signalé (clipped = true).
    meter_.process(block.toConst());

    // 6. Assainissement : ce qui suit ne peut plus sortir invalide.
    dsp::sanitizeBlock(block);

    // 7. Écriture dans la mémoire du pilote, via la vue fournie.
    for (int ch = 0; ch < destination.channels(); ++ch) {
        float* out = destination.channel(ch);
        if (out == nullptr)
            continue;
        const float* in = (ch < mixChannels) ? block.channel(ch) : nullptr;
        if (in != nullptr) {
            for (int i = 0; i < frames; ++i)
                out[i] = in[i];
        }
        for (int i = frames; i < destination.frames(); ++i)
            out[i] = 0.0f; // au-delà de la taille de bloc préparée : silence
    }
}

} // namespace svp::engine
