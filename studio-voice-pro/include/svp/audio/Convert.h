// Studio Voice Pro — module audio : conversions entre unités.
// dBFS (décibels pleine échelle) <-> amplitude linéaire, entiers PCM <-> flottant.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace svp::audio {

/** Plancher numérique utilisé pour exprimer le silence en dBFS. */
inline constexpr double kSilenceDb = -200.0;

/** Amplitude linéaire -> dBFS. 0.0 (ou négatif) donne kSilenceDb, jamais -inf. */
[[nodiscard]] inline double gainToDecibels(double linearGain) noexcept
{
    if (linearGain <= 0.0)
        return kSilenceDb;
    return 20.0 * std::log10(linearGain);
}

/** dBFS -> amplitude linéaire. 0 dB => 1.0, -6 dB => ~0.501187. */
[[nodiscard]] inline double decibelsToGain(double decibels) noexcept
{
    return std::pow(10.0, decibels / 20.0);
}

/** Bornage d'un échantillon flottant dans [-1, +1]. */
[[nodiscard]] inline float clampSample(float value) noexcept
{
    return std::clamp(value, -1.0f, 1.0f);
}

// --- Conversions PCM <-> flottant ------------------------------------------
// Convention : le flottant est normalisé dans [-1, +1] et les entiers PCM
// utilisent la pleine échelle signée (division par 2^(n-1), comme libsndfile).

[[nodiscard]] inline int32_t floatToPcm24(float value) noexcept
{
    const double scaled = static_cast<double>(clampSample(value)) * 8388607.0; // 2^23 - 1
    return static_cast<int32_t>(std::lround(scaled));
}

[[nodiscard]] inline int16_t floatToPcm16(float value) noexcept
{
    const double scaled = static_cast<double>(clampSample(value)) * 32767.0; // 2^15 - 1
    return static_cast<int16_t>(std::lround(scaled));
}

[[nodiscard]] inline float pcm24ToFloat(int32_t value) noexcept
{
    return static_cast<float>(static_cast<double>(value) / 8388607.0);
}

[[nodiscard]] inline float pcm16ToFloat(int16_t value) noexcept
{
    return static_cast<float>(static_cast<double>(value) / 32767.0);
}

} // namespace svp::audio
