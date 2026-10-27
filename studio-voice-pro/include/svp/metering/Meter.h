// Studio Voice Pro — module metering : crête, RMS, maintien du pic (peak hold).
#pragma once

#include "svp/audio/Buffer.h"

namespace svp::metering {

/** Mesures d'un bloc audio, en amplitude linéaire. */
struct MeterValues {
    float peak = 0.0f;     // valeur absolue maximale du bloc (toujours finie)
    float rms = 0.0f;      // valeur efficace du bloc (toujours finie)
    float peakHold = 0.0f; // pic maintenu (décroît seulement après le maintien)
    bool clipped = false;  // vrai si un échantillon atteint/dépasse 1.0 OU si une
                           // valeur non finie (NaN, ±Inf) a été trouvée — un NaN
                           // laissait auparavant `clipped` faux, donc le témoin
                           // n'allumait jamais sur une sortie invalide
    bool nonFinite = false; // vrai si un NaN ou un ±Inf a été trouvé dans le bloc
};

/** Mesure crête + RMS pour un bloc, sans état. [RT]

    Traitement des valeurs non finies : un échantillon NaN ou ±Inf est ÉCARTÉ
    de la crête et de la RMS (sinon les deux mesures deviennent NaN et le
    témoin devient inexploitable) et il allume `nonFinite` ainsi que
    `clipped`. Autrement dit `clipped` signifie désormais « la sortie est
    écrêtée OU invalide », ce qui est l'information utile au bout d'une chaîne
    de traitement. */
[[nodiscard]] MeterValues measureBlock(audio::AudioBufferConstView buffer) noexcept;

/** Compteur temps réel avec maintien du pic et décroissance.
    Utilisation : prepare() hors thread audio, puis process() dans la boucle.

    État réel (à ne pas surinterpréter) : `values_` est une simple structure
    `MeterValues` — quatre flottants et deux booléens, sans `std::atomic`, sans
    verrou, sans double buffer. `values()` n'alloue pas et se lit en temps
    constant, MAIS ce n'est PAS une lecture sûre depuis un autre thread :
    rien n'empêche l'IHM de lire pendant que `process()` réécrit la structure
    (déchirure de valeur, mélange de deux blocs). Aujourd'hui le compteur
    n'est utilisé que par les tests, par l'outil en ligne de commande et par
    `engine::Engine`, qui appellent `process()` et `values()` sur le même
    thread.

    À FAIRE — prochaine étape (télémétrie IHM ↔ audio) : publier les valeurs
    dans un instantané atomique et ne lire que celui-ci depuis l'IHM (par
    exemple un `std::atomic<std::uint64_t>` portant les flottants en bits, ou
    un double buffer avec indicateur atomique). Tant que ce n'est pas fait,
    `values()` ne doit être appelé que depuis le thread audio. [RT] */
class PeakRmsMeter {
public:
    /** `sampleRate` sert à convertir le maintien exprimé en millisecondes
        en nombre de blocs. */
    void prepare(double sampleRate, int blockSize, float holdMilliseconds = 1200.0f) noexcept;

    /** Réinitialise les valeurs (silence), témoin d'écrêtage compris. [RT] */
    void reset() noexcept;

    /** Éteint le témoin d'écrêtage SANS toucher au pic, à la RMS ni au maintien.
        Le témoin est collant par conception (il reste allumé jusqu'à
        `clearClip()` ou `reset()`) : sans cela, un seul échantillon écrêté
        entre deux rafraîchissements d'écran passerait inaperçu. [RT] */
    void clearClip() noexcept { values_.clipped = false; }

    /** Traite un bloc et met à jour crête, RMS et maintien. [RT] */
    void process(audio::AudioBufferConstView buffer) noexcept;

    [[nodiscard]] MeterValues values() const noexcept { return values_; }

private:
    double sampleRate_ = 48000.0;
    int blockSize_ = 512;
    float holdSeconds_ = 1.2f;
    float holdCounter_ = 0.0f;   // secondes restantes de maintien
    MeterValues values_{};
};

} // namespace svp::metering
