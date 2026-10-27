// Studio Voice Pro — module dsp : mixage de pistes avec mute/solo.
//
// RÈGLE DSP NON DESTRUCTIVE (à respecter par tout appelant) :
//   « on lit une source, on écrit une destination ».
//   Une piste de média chargée (`AudioBuffer` issu d'un WAV, adressé par un
//   `const AudioBuffer*`) n'est JAMAIS modifiée par le mixage ni par une
//   application de gain/panoramique. Les moteurs ne reçoivent donc pas des
//   pointeurs mutables sur les médias : ce module lit des
//   `audio::AudioBufferConstView` (voir `applyStripTo`) et n'écrit que dans
//   des `audio::AudioBufferView` — tampon de travail ou mémoire du pilote.
//   Historiquement, `applyStrip()` remettait à zéro le buffer d'entrée quand
//   la piste était inaudible (mute/solo) : appliqué à un média, un simple
//   mute effaçait les échantillons. Cette fonction a été supprimée et
//   remplacée par `applyStripTo()`, non destructive.
#pragma once

#include <span>

#include "svp/audio/Buffer.h"

namespace svp::dsp {

/** État d'une piste au moment du mixage. Tous les champs sont des copies
    de paramètres : aucune modification n'est faite depuis le thread audio. */
struct ChannelStrip {
    float gain = 1.0f;   // gain linéaire
    float pan = 0.0f;    // -1 gauche, +1 droite
    bool mute = false;
    bool solo = false;
};

/** Règle d'audibilité : si au moins une piste est en solo, seules les pistes
    en solo (et non muettes) sont audibles ; sinon tout ce qui n'est pas muet. */
[[nodiscard]] bool isChannelAudible(const ChannelStrip& strip, bool anySoloActive) noexcept;

/** Vrai si au moins une piste est en solo. Ne copie ni n'alloue le tableau :
    `strips` est une vue sur la mémoire de l'appelant. [RT] */
[[nodiscard]] bool anySoloActive(std::span<const ChannelStrip> strips) noexcept;

/** Mixe les pistes dans `destination` (qui est remise à zéro puis remplie).
    Les pistes sont additionnées ; le résultat n'est PAS écrêté ici (le
    limiteur/mesure d'écrêtage est du ressort de l'étage suivant).
    `destination` doit avoir au moins 2 canaux et au moins autant de trames
    que la plus longue piste ; les pistes plus courtes sont traitées comme
    silencieuses après leur fin.

    Contrat d'appel — AUCUNE ALLOCATION, ni ici ni chez l'appelant :
      * `destination` est un `AudioBufferView` : mémoire fournie par le
        pilote (sortie WASAPI/ASIO) ou tampon de travail du moteur, alloué
        dans `Engine::prepare()`. Si elle est trop petite, le mélange est
        tronqué à sa taille.
      * `sources` et `strips` sont des `std::span` : de simples vues sur des
        tableaux déjà existants. L'ancienne signature prenait des
        `std::vector`, ce qui obligeait l'appelant à construire un vecteur
        (donc à allouer) à chaque bloc de 512 trames — c'est corrigé.
      * Le contexte de mixage (table de pointeurs de sources, tableau de
        `ChannelStrip`) est pré-alloué une fois pour toutes par
        `Engine::prepare()`.

    Précondition : `sources[i]` correspond à `strips[i]` ; les entrées en trop
    d'un côté sont ignorées. Les médias pointés ne sont jamais écrits. [RT] */
void mixStrips(audio::AudioBufferView destination,
               std::span<const audio::AudioBuffer* const> sources,
               std::span<const ChannelStrip> strips) noexcept;

/** Applique les règles mute/solo/gain/pan d'une piste en lisant `source` et
    en écrivant `destination`. NON DESTRUCTIF : `source` n'est jamais modifiée.
    Si la piste n'est pas audible, c'est `destination` qui est mise à zéro
    (jamais `source`). `destination` peut être plus courte que `source` : le
    traitement est alors limité à la destination, à partir de la trame 0.

    C'est la seule façon correcte de « traiter une piste » : l'appelant qui
    veut modifier un média doit écrire le résultat dans un autre buffer. [RT] */
void applyStripTo(audio::AudioBufferConstView source, audio::AudioBufferView destination,
                  const ChannelStrip& strip, bool anySoloActive) noexcept;

} // namespace svp::dsp
