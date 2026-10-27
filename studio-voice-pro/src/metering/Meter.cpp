// Studio Voice Pro — mesure crête, RMS et maintien du pic.
#include "svp/metering/Meter.h"

#include <algorithm>
#include <cmath>

namespace svp::metering {

MeterValues measureBlock(audio::AudioBufferConstView buffer) noexcept
{
    MeterValues values{};
    if (buffer.empty())
        return values;

    // Nombre d'échantillons lu par la mesure : canaux × trames de la vue.
    const double total = static_cast<double>(buffer.channels()) * static_cast<double>(buffer.frames());
    double sumOfSquares = 0.0;

    for (int ch = 0; ch < buffer.channels(); ++ch) {
        const float* data = buffer.channel(ch);
        if (data == nullptr)
            continue;
        for (int i = 0; i < buffer.frames(); ++i) {
            const float value = data[i];
            if (!std::isfinite(value)) {
                // NaN / ±Inf : écarté du calcul (il le rendrait NaN et le
                // témoin inexploitable) mais signalé comme sortie invalide.
                values.nonFinite = true;
                continue;
            }
            const float magnitude = std::fabs(value);
            if (magnitude > values.peak)
                values.peak = magnitude;
            sumOfSquares += static_cast<double>(value) * static_cast<double>(value);
        }
    }

    values.rms = (total > 0.0) ? static_cast<float>(std::sqrt(sumOfSquares / total)) : 0.0f;
    values.clipped = values.nonFinite || (values.peak >= 1.0f);
    values.peakHold = values.peak;
    return values;
}

void PeakRmsMeter::prepare(double sampleRate, int blockSize, float holdMilliseconds) noexcept
{
    sampleRate_ = (sampleRate > 0.0) ? sampleRate : 48000.0;
    blockSize_ = (blockSize > 0) ? blockSize : 512;
    holdSeconds_ = holdMilliseconds / 1000.0f;
    reset();
}

void PeakRmsMeter::reset() noexcept
{
    values_ = MeterValues{};
    holdCounter_ = 0.0f;
}

void PeakRmsMeter::process(audio::AudioBufferConstView buffer) noexcept
{
    const MeterValues block = measureBlock(buffer);
    values_.peak = block.peak;
    values_.rms = block.rms;
    // Témoins collants : ils restent allumés jusqu'à clearClip()/reset().
    values_.clipped = values_.clipped || block.clipped;
    values_.nonFinite = values_.nonFinite || block.nonFinite;

    const double blockSeconds = (buffer.frames() > 0)
                                    ? static_cast<double>(buffer.frames()) / sampleRate_
                                    : static_cast<double>(blockSize_) / sampleRate_;
    if (block.peak >= values_.peakHold) {
        values_.peakHold = block.peak;
        holdCounter_ = holdSeconds_;
        return;
    }
    holdCounter_ -= static_cast<float>(blockSeconds);
    if (holdCounter_ <= 0.0f) {
        constexpr float kDecayPerBlock = 0.9f; // ~ -0.92 dB par bloc
        values_.peakHold = std::max(block.peak, values_.peakHold * kDecayPerBlock);
    }
}

} // namespace svp::metering
