// Studio Voice Pro — module audio : mémoire d'échantillons (buffers et vues).
// Aucune dépendance à JUCE ni à une quelconque IHM : ce module est le socle
// utilisable aussi bien par le moteur hors ligne (svp-render) que par le
// thread audio de l'application native (WASAPI/ASIO).
#pragma once

#include <cstddef>
#include <type_traits>
#include <vector>

namespace svp::audio {

/** Vue NON PROPRIÉTAIRE sur un bloc d'échantillons flottants.

    Rôle : permettre au DSP d'écrire directement dans la mémoire d'un pilote
    (WASAPI en mode partagé/exclusif, ASIO) sans jamais la copier ni
    l'allouer. La vue ne possède rien : elle ne survit pas au bloc et ne doit
    jamais être conservée au-delà de la portée qui l'a construite.

    Deux formes de mémoire sont couvertes :

      1. **Table de pointeurs de canaux** — `BasicAudioBufferView(float* const*
         channels, n, frames)`, exactement ce que fournit un pilote ASIO
         (`ASIOBufferInfo::buffers[ch]`) ou une sortie WASAPI déjà
         « dé-entrelacée » par le backend.
      2. **Bloc plan « canal majeur »** — `fromPlanar(base, n, frames, stride)`,
         la disposition interne de `AudioBuffer` (tous les échantillons du
         canal 0, puis du canal 1, ...). `stride` est exprimé en trames et
         vaut par défaut `frames`, ce qui permet aussi de découper une vue
         plus courte (un bloc de taille variable) à l'intérieur d'un tampon
         alloué pour la taille de bloc maximale : c'est le cas de
         `Engine::process()`.

    Deux instanciations, choisies par le type de pointeur d'échantillon :

      * `AudioBufferView`      = vue **mutable** (`float*`), écriture permise ;
      * `AudioBufferConstView` = vue **en lecture seule** (`const float*`).

    Le `const` est vérifié par le compilateur : `setSample()`/`addSample()`/
    `clear()` n'existent pas sur une vue constante. Une fonction qui ne doit
    pas modifier ses entrées prend donc un `AudioBufferConstView` — la
    règle « on lit une source, on écrit une destination » est ainsi portée
    par les types et non par un commentaire.

    Aucune méthode ne peut allouer : ce type n'a ni constructeur allouant, ni
    méthode de redimensionnement, et son constructeur est `noexcept`. [RT] */
template <class SamplePtr>
class BasicAudioBufferView {
public:
    using SampleType = std::remove_pointer_t<SamplePtr>; // float ou const float
    static constexpr bool kIsMutable = !std::is_const_v<SampleType>;

    /** Vue vide (aucun canal). */
    BasicAudioBufferView() noexcept = default;

    /** Vue sur une table de pointeurs de canaux fournie par un pilote.
        `channels` peut être `nullptr` (vue vide) ; les valeurs négatives de
        `numChannels`/`numFrames` sont ramenées à zéro. Les pointeurs doivent
        rester valides pendant toute la vie de la vue. [RT] */
    BasicAudioBufferView(SamplePtr const* channels, int numChannels, int numFrames) noexcept
        : channels_(channels),
          numChannels_((numChannels > 0) ? numChannels : 0),
          numFrames_((numFrames > 0) ? numFrames : 0)
    {
    }

    /** Vue sur un bloc plan « canal majeur » : le canal `ch` commence à
        `base + ch * stride`. `stride` est en trames (>= numFrames en
        pratique). [RT] */
    static BasicAudioBufferView fromPlanar(SamplePtr base, int numChannels, int numFrames,
                                           int stride = -1) noexcept
    {
        BasicAudioBufferView view;
        view.base_ = base;
        view.numChannels_ = (numChannels > 0) ? numChannels : 0;
        view.numFrames_ = (numFrames > 0) ? numFrames : 0;
        view.stride_ = (stride >= 0) ? stride : view.numFrames_;
        return view;
    }

    [[nodiscard]] int channels() const noexcept { return numChannels_; }
    [[nodiscard]] int frames() const noexcept { return numFrames_; }
    [[nodiscard]] bool empty() const noexcept { return numChannels_ <= 0 || numFrames_ <= 0; }

    /** Table de pointeurs de canaux, si la vue a été construite ainsi ;
        `nullptr` pour une vue « bloc plan ». [RT] */
    [[nodiscard]] SamplePtr const* channelPointers() const noexcept { return channels_; }

    /** Pointeur sur les échantillons d'un canal. Retourne `nullptr` si
        l'index de canal est invalide. [RT] */
    [[nodiscard]] SamplePtr channel(int ch) const noexcept
    {
        if (ch < 0 || ch >= numChannels_)
            return nullptr;
        if (channels_ != nullptr)
            return channels_[ch];
        if (base_ == nullptr)
            return nullptr;
        return base_ + static_cast<std::size_t>(ch) * static_cast<std::size_t>(stride_);
    }

    /** Lecture d'un échantillon. Hors bornes : 0. [RT] */
    [[nodiscard]] float sample(int ch, int frame) const noexcept
    {
        SamplePtr data = channel(ch);
        if (data == nullptr || frame < 0 || frame >= numFrames_)
            return 0.0f;
        return data[frame];
    }

    /** Écriture d'un échantillon (vue mutable uniquement). Hors bornes :
        sans effet. [RT] */
    void setSample(int ch, int frame, float value) const noexcept
    {
        static_assert(kIsMutable, "setSample() exige une vue mutable (AudioBufferView)");
        SamplePtr data = channel(ch);
        if (data == nullptr || frame < 0 || frame >= numFrames_)
            return;
        const_cast<SampleType*>(data)[frame] = value;
    }

    /** Ajout d'un échantillon (vue mutable uniquement). [RT] */
    void addSample(int ch, int frame, float value) const noexcept
    {
        static_assert(kIsMutable, "addSample() exige une vue mutable (AudioBufferView)");
        SamplePtr data = channel(ch);
        if (data == nullptr || frame < 0 || frame >= numFrames_)
            return;
        const_cast<SampleType*>(data)[frame] += value;
    }

    /** Met tous les échantillons de la vue à zéro (vue mutable uniquement).
        N'écrit que dans la vue : les canaux/trames au-delà ne sont pas
        touchés. [RT] */
    void clear() const noexcept
    {
        static_assert(kIsMutable, "clear() exige une vue mutable (AudioBufferView)");
        for (int ch = 0; ch < numChannels_; ++ch) {
            SampleType* data = const_cast<SampleType*>(channel(ch));
            if (data == nullptr)
                continue;
            for (int i = 0; i < numFrames_; ++i)
                data[i] = 0.0f;
        }
    }

    /** Passage d'une vue mutable à la vue en lecture seule correspondante
        (même mémoire, mêmes bornes). Aucune copie. [RT] */
    template <class P = SamplePtr,
              std::enable_if_t<!std::is_const_v<std::remove_pointer_t<P>>, int> = 0>
    [[nodiscard]] BasicAudioBufferView<const SampleType*> toConst() const noexcept
    {
        BasicAudioBufferView<const SampleType*> view;
        view.channels_ = const_cast<const SampleType* const*>(channels_);
        view.base_ = base_;
        view.stride_ = stride_;
        view.numChannels_ = numChannels_;
        view.numFrames_ = numFrames_;
        return view;
    }

private:
    template <class>
    friend class BasicAudioBufferView;

    SamplePtr const* channels_ = nullptr; // forme « table de pointeurs »
    SamplePtr base_ = nullptr;            // forme « bloc plan »
    int stride_ = 0;                      // en trames (forme « bloc plan »)
    int numChannels_ = 0;
    int numFrames_ = 0;
};

using AudioBufferView = BasicAudioBufferView<float*>;
using AudioBufferConstView = BasicAudioBufferView<const float*>;

/** Tampon audio multicanal PROPRIÉTAIRE, échantillons flottants 32 bits,
    disposition « canal majeur » (tous les échantillons du canal 0, puis du
    canal 1, ...).

    Règle temps réel : les méthodes marquées [RT] n'allouent jamais et ne
    jettent jamais ; les autres peuvent allouer et ne doivent être appelées
    que hors du thread audio (préparation, chargement).

    RÈGLE DE PROPRIÉTÉ — `resize()` est INTERDIT APRÈS `Engine::prepare()`.
    ---------------------------------------------------------------------
    Un `AudioBuffer` possède sa mémoire et peut la réallouer ; une vue
    (`AudioBufferView`) pointe vers de la mémoire qu'elle ne possède pas. Dès
    qu'un buffer est exposé au moteur ou à un pilote (comme source de mixage,
    comme tampon de travail, ou parce qu'une vue a été construite sur lui),
    tout appel à `resize()` invalide les pointeurs que les vues détiennent :
    le rendu écrirait alors dans de la mémoire libérée — corruption de média,
    plantage ou bruit. « Après prepare() » désigne ici l'état préparé du
    moteur : entre deux `Engine::prepare()`, tout `resize()` doit passer par
    la procédure de rechargement (arrêt du flux audio, `prepare()` de nouveau),
    et jamais par le thread audio.

    Corollaire : `resize()` N'EST PAS une méthode temps réel, et aucun chemin
    DSP ne doit y faire appel. Pour traiter un bloc, on construit une vue
    (`view()`, `constView()`, `wrapExternal()`) — jamais on ne redimensionne.
*/
class AudioBuffer {
public:
    AudioBuffer() = default;
    AudioBuffer(int numChannels, int numFrames);

    /** Alloue (ou réalloue) le stockage. HORS THREAD AUDIO, et interdit après
        `prepare()` dès que le buffer est exposé au moteur (voir la règle de
        propriété ci-dessus). */
    void resize(int numChannels, int numFrames);

    /** Vue mutable sur tout le buffer. Ne copie rien, n'alloue rien. [RT] */
    [[nodiscard]] AudioBufferView view() noexcept
    {
        return AudioBufferView::fromPlanar(data_.data(), channels_, frames_);
    }

    /** Vue en lecture seule sur tout le buffer. [RT] */
    [[nodiscard]] AudioBufferConstView view() const noexcept
    {
        return AudioBufferConstView::fromPlanar(data_.data(), channels_, frames_);
    }

    /** Alias explicite de `view() const` (lisibilité des appels DSP). [RT] */
    [[nodiscard]] AudioBufferConstView constView() const noexcept { return view(); }

    /** Vue sur une mémoire fournie de l'extérieur (typiquement par un pilote),
        sans transfert de propriété : le buffer reste vide et n'écrit jamais
        dans cette mémoire. [RT] */
    [[nodiscard]] static AudioBufferView wrapExternal(float* const* channels, int numChannels,
                                                      int numFrames) noexcept
    {
        return AudioBufferView(channels, numChannels, numFrames);
    }

    /** Conversions implicites vers les vues : elles ne font que dériver des
        pointeurs, sans allocation, et permettent d'écrire
        `dsp::applyGain(buffer, g)` aussi bien que `dsp::applyGain(vue, g)`.
        La vue ne doit jamais être conservée plus longtemps que le buffer. [RT] */
    operator AudioBufferView() noexcept { return view(); }
    operator AudioBufferConstView() const noexcept { return view(); }

    /** Met tous les échantillons à zéro. [RT] */
    void clear() noexcept;

    /** Silencie une seule piste. [RT] */
    void clearChannel(int channel) noexcept;

    [[nodiscard]] int channels() const noexcept { return channels_; }
    [[nodiscard]] int frames() const noexcept { return frames_; }
    [[nodiscard]] bool empty() const noexcept { return channels_ == 0 || frames_ == 0; }

    /** Accès direct aux échantillons d'un canal. [RT] Retourne nullptr si l'index est invalide. */
    [[nodiscard]] float* channel(int ch) noexcept;
    [[nodiscard]] const float* channel(int ch) const noexcept;

    /** Accès échantillon par échantillon. [RT] */
    [[nodiscard]] float sample(int ch, int frame) const noexcept;
    void setSample(int ch, int frame, float value) noexcept;

    /** Ajoute `value` à l'échantillon (utilisé par le mixage). [RT] */
    void addSample(int ch, int frame, float value) noexcept;

    [[nodiscard]] std::size_t totalSamples() const noexcept
    {
        return static_cast<std::size_t>(channels_) * static_cast<std::size_t>(frames_);
    }

private:
    std::vector<float> data_;
    int channels_ = 0;
    int frames_ = 0;
};

} // namespace svp::audio
