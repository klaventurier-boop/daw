// Studio Voice Pro — module audio : taux d'échantillonnage et rééchantillonnage simple.
#pragma once

#include <vector>

#include "svp/audio/Buffer.h"

namespace svp::audio {

/** Taux d'échantillonnage supportés en v0.1 (Hz). */
inline constexpr int kSampleRate44100 = 44100;
inline constexpr int kSampleRate48000 = 48000;
inline constexpr int kSampleRate88200 = 88200;
inline constexpr int kSampleRate96000 = 96000;
inline constexpr int kSampleRate192000 = 192000;

/** Vrai si le taux est un entier strictement positif dans la plage audio usuelle. */
[[nodiscard]] bool isValidSampleRate(int sampleRate) noexcept;

/** Vrai si les taux sont compatibles avec les convertisseurs matériels
    ASIO/WASAPI (les taux ne sont pas tenus d'être identiques, mais les taux
    hors plage 8 kHz - 384 kHz sont refusés). */
[[nodiscard]] bool sampleRatesCompatible(int sourceRate, int deviceRate) noexcept;

/** Rééchantillonnage linéaire (interpolation simple, sans dépendance externe).
    Suffisant pour la v0.1 ; la v0.2 prévoit une interpolation à fenêtre
    (polyphase) pour une qualité irréprochable. Hors thread audio. */
[[nodiscard]] AudioBuffer resampleLinear(const AudioBuffer& input, int sourceRate, int targetRate);

} // namespace svp::audio
