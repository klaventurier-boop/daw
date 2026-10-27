// Studio Voice Pro — assainissement d'un bloc avant remise au pilote.
#pragma once

#include "svp/audio/Buffer.h"

namespace svp::dsp {

/** Nettoie un bloc destiné au pilote : tout échantillon non fini (NaN, +Inf,
    -Inf) est remplacé par 0, et tout dénormal est ramené à 0.

    Pourquoi : un NaN ou un Inf produit par un effet (division par zéro dans
    un filtre, plugin tiers défaillant) se propage et « colle » au matériel —
    sur certaines cartes, la sortie reste muette ou bloquée jusqu'au
    redémarrage du pilote. Un dénormail est en outre lent à traiter. Cette
    fonction est le dernier filet de sécurité avant l'écriture dans la
    mémoire du pilote : même si un étage en amont se comporte mal, ce que
    reçoit le pilote est toujours fini et normal.

    Les valeurs valides ne sont jamais modifiées (0 n'est pas un NaN, ±1.0
    n'est pas un Inf) : l'assainissement est transparent sur un signal sain.

    Retourne vrai si au moins un échantillon a été remplacé — l'appelant peut
    s'en servir pour allumer un témoin d'état, il ne doit pas allouer pour
    autant. Aucune allocation, aucun verrou, aucun accès disque. [RT] */
[[nodiscard]] bool sanitizeBlock(audio::AudioBufferView block) noexcept;

/** Vrai si tous les échantillons du bloc sont finis ET qu'aucun n'est
    dénormal. Utile aux tests et aux diagnostics ; pas utilisé sur le chemin
    temps réel chaud. [RT] */
[[nodiscard]] bool isBlockClean(audio::AudioBufferConstView block) noexcept;

} // namespace svp::dsp
