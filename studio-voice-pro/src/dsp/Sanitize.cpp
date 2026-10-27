// Studio Voice Pro — assainissement d'un bloc (NaN/Inf/dénormaux).
#include "svp/dsp/Sanitize.h"

#include <cmath>

namespace svp::dsp {

namespace {
/** Vrai si la valeur doit être remplacée : non finie (NaN, ±Inf) ou dénormal. */
[[nodiscard]] bool isReplaceable(float value) noexcept
{
    if (!std::isfinite(value))
        return true;
    return std::fpclassify(value) == FP_SUBNORMAL;
}
} // namespace

bool sanitizeBlock(audio::AudioBufferView block) noexcept
{
    bool replacedSomething = false;
    for (int ch = 0; ch < block.channels(); ++ch) {
        float* data = block.channel(ch);
        if (data == nullptr)
            continue;
        for (int i = 0; i < block.frames(); ++i) {
            if (isReplaceable(data[i])) {
                data[i] = 0.0f;
                replacedSomething = true;
            }
        }
    }
    return replacedSomething;
}

bool isBlockClean(audio::AudioBufferConstView block) noexcept
{
    for (int ch = 0; ch < block.channels(); ++ch) {
        const float* data = block.channel(ch);
        if (data == nullptr)
            continue;
        for (int i = 0; i < block.frames(); ++i) {
            if (isReplaceable(data[i]))
                return false;
        }
    }
    return true;
}

} // namespace svp::dsp
