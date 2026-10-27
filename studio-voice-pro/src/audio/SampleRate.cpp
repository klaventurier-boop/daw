// Studio Voice Pro — taux d'échantillonnage et rééchantillonnage linéaire.
#include "svp/audio/SampleRate.h"

#include <algorithm>
#include <cmath>

namespace svp::audio {

namespace {
constexpr int kMinRate = 8000;
constexpr int kMaxRate = 384000;
} // namespace

bool isValidSampleRate(int sampleRate) noexcept
{
    return sampleRate >= kMinRate && sampleRate <= kMaxRate;
}

bool sampleRatesCompatible(int sourceRate, int deviceRate) noexcept
{
    return isValidSampleRate(sourceRate) && isValidSampleRate(deviceRate);
}

AudioBuffer resampleLinear(const AudioBuffer& input, int sourceRate, int targetRate)
{
    if (input.empty() || !sampleRatesCompatible(sourceRate, targetRate))
        return {};

    if (sourceRate == targetRate)
        return input;

    const double ratio = static_cast<double>(targetRate) / static_cast<double>(sourceRate);
    const auto targetFrames = static_cast<int>(std::llround(static_cast<double>(input.frames()) * ratio));
    AudioBuffer output(input.channels(), std::max(1, targetFrames));

    for (int ch = 0; ch < input.channels(); ++ch) {
        const float* in = input.channel(ch);
        float* out = output.channel(ch);
        for (int i = 0; i < output.frames(); ++i) {
            const double sourcePos = static_cast<double>(i) / ratio;
            const auto index = static_cast<int>(sourcePos);
            const double frac = sourcePos - static_cast<double>(index);
            const float a = (index < input.frames()) ? in[index] : 0.0f;
            const float b = (index + 1 < input.frames()) ? in[index + 1] : 0.0f;
            out[i] = static_cast<float>(static_cast<double>(a) + (static_cast<double>(b) - static_cast<double>(a)) * frac);
        }
    }
    return output;
}

} // namespace svp::audio
