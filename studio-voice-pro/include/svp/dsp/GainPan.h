// Studio Voice Pro — module dsp : gain, panorama, gain en dB, fondus.
// Toutes les fonctions de ce module sont temps réel : pas d'allocation,
// pas d'accès disque, pas de verrou.
//
// RÈGLE DSP NON DESTRUCTIVE : « on lit une source, on écrit une destination ».
//   * Les variantes `applyGainTo`, `applyGainRampTo`, `applyPanTo` prennent une
//     source en LECTURE SEULE (`AudioBufferConstView`) et écrivent dans une
//     destination (`AudioBufferView`) : c'est la forme à utiliser dès qu'une
//     des deux mémoires est un média chargé.
//   * Les variantes en place (`applyGain`, `applyGainRamp`, `applyPan`) doivent
//     être réservées aux TAMPONS DE TRAVAIL (mémoire du pilote ou scratch du
//     moteur). Il est INTERDIT de les appliquer à un média chargé (`AudioBuffer`
//     issu d'un import WAV) : le fichier importé en mémoire serait corrompu —
//     mesure faussée, réexport faux, retour en arrière impossible, et aucun
//     avertissement. Si un média doit être traité, on lit le média et on écrit
//     dans un autre buffer (ou on recharge le média depuis le disque).
//   * Un appel peut passer un `AudioBuffer` : la conversion implicite en vue
//     ne change pas la règle ci-dessus, elle ne fait que dériver des pointeurs.
//     L'aliasing source == destination est autorisé : l'opération est
//     purement échantillon par échantillon (aucune dépendance entre voisins).
#pragma once

#include "svp/audio/Buffer.h"

namespace svp::dsp {

/** Applique un gain linéaire constant. EN PLACE (tampon de travail seulement). [RT] */
void applyGain(audio::AudioBufferView buffer, float linearGain) noexcept;

/** Applique un gain en décibels (0 dB = unité, -6 dB = moitié environ).
    EN PLACE (tampon de travail seulement). [RT] */
void applyGainDb(audio::AudioBufferView buffer, double gainDb) noexcept;

/** Applique un gain qui évolue linéairement de startGain à endGain sur le
    buffer (utilisé pour les fondus). EN PLACE (tampon de travail seulement). [RT] */
void applyGainRamp(audio::AudioBufferView buffer, float startGain, float endGain) noexcept;

/** Applique un panoramique. `pan` est borné à [-1, +1] :
      -1 = gauche, 0 = centre, +1 = droite.
    Loi constante en puissance (centre = -3 dB sur chaque canal), identique à
    la loi par défaut des consoles numériques. Un buffer mono est d'abord
    dupliqué mentalement : le canal 0 reste mono, seul le gain total varie.
    EN PLACE (tampon de travail seulement). [RT] */
void applyPan(audio::AudioBufferView buffer, float pan) noexcept;

/** `destination = source * gain`, SANS toucher à la source (média sûr).
    Les échantillons de `destination` hors de la source sont laissés tels
    quels ; les canaux de destination sans canal source correspondant sont mis
    à zéro (aucune mémoire non initialisée vers le pilote). [RT] */
void applyGainTo(audio::AudioBufferConstView source, audio::AudioBufferView destination,
                 float linearGain) noexcept;

/** Fondu vers `endGain` dans `destination`, la source restant intacte. [RT] */
void applyGainRampTo(audio::AudioBufferConstView source, audio::AudioBufferView destination,
                     float startGain, float endGain) noexcept;

/** Même règle de panoramique qu'`applyPan`, mais de la source vers la
    destination : la source n'est jamais modifiée. [RT] */
void applyPanTo(audio::AudioBufferConstView source, audio::AudioBufferView destination,
                float pan) noexcept;

/** Gains gauche/droite correspondant à une position de panoramique. [RT] */
void panGains(float pan, float& outLeft, float& outRight) noexcept;

} // namespace svp::dsp
