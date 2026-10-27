// Studio Voice Pro — gain et panoramique (temps réel, sans allocation).
#include "svp/dsp/GainPan.h"

#include <algorithm>
#include <cmath>

#include "svp/audio/Convert.h"

namespace svp::dsp {

namespace {
constexpr float kPi = 3.14159265358979323846f;
} // namespace

void applyGain(audio::AudioBufferView buffer, float linearGain) noexcept
{
    for (int ch = 0; ch < buffer.channels(); ++ch) {
        float* data = buffer.channel(ch);
        if (data == nullptr)
            continue;
        for (int i = 0; i < buffer.frames(); ++i)
            data[i] *= linearGain;
    }
}

void applyGainDb(audio::AudioBufferView buffer, double gainDb) noexcept
{
    applyGain(buffer, static_cast<float>(audio::decibelsToGain(gainDb)));
}

void applyGainRamp(audio::AudioBufferView buffer, float startGain, float endGain) noexcept
{
    const int frames = buffer.frames();
    if (frames <= 0)
        return;
    const float step = (frames > 1) ? (endGain - startGain) / static_cast<float>(frames - 1) : 0.0f;
    for (int i = 0; i < frames; ++i) {
        const float g = startGain + step * static_cast<float>(i);
        for (int ch = 0; ch < buffer.channels(); ++ch)
            buffer.setSample(ch, i, buffer.sample(ch, i) * g);
    }
}

void applyGainTo(audio::AudioBufferConstView source, audio::AudioBufferView destination,
                 float linearGain) noexcept
{
    const int frames = std::min(source.frames(), destination.frames());
    const int channels = std::min(source.channels(), destination.channels());
    for (int ch = 0; ch < channels; ++ch) {
        const float* in = source.channel(ch);
        float* out = destination.channel(ch);
        if (in == nullptr || out == nullptr)
            continue;
        for (int i = 0; i < frames; ++i)
            out[i] = in[i] * linearGain;
    }
    // Canaux de destination sans source correspondante : silence explicite.
    for (int ch = channels; ch < destination.channels(); ++ch) {
        float* out = destination.channel(ch);
        if (out == nullptr)
            continue;
        std::fill(out, out + destination.frames(), 0.0f);
    }
}

void applyGainRampTo(audio::AudioBufferConstView source, audio::AudioBufferView destination,
                     float startGain, float endGain) noexcept
{
    const int frames = std::min(source.frames(), destination.frames());
    if (frames <= 0)
        return;
    const float step = (frames > 1) ? (endGain - startGain) / static_cast<float>(frames - 1) : 0.0f;
    const int channels = std::min(source.channels(), destination.channels());
    for (int ch = 0; ch < channels; ++ch) {
        const float* in = source.channel(ch);
        float* out = destination.channel(ch);
        if (in == nullptr || out == nullptr)
            continue;
        for (int i = 0; i < frames; ++i)
            out[i] = in[i] * (startGain + step * static_cast<float>(i));
    }
    for (int ch = channels; ch < destination.channels(); ++ch) {
        float* out = destination.channel(ch);
        if (out == nullptr)
            continue;
        std::fill(out, out + destination.frames(), 0.0f);
    }
}

void panGains(float pan, float& outLeft, float& outRight) noexcept
{
    const float clamped = std::clamp(pan, -1.0f, 1.0f);
    const float angle = (clamped + 1.0f) * 0.25f * kPi; // 0 .. pi/2
    outLeft = std::cos(angle);
    outRight = std::sin(angle);
}

void applyPanTo(audio::AudioBufferConstView source, audio::AudioBufferView destination,
                float pan) noexcept
{
    if (destination.empty())
        return;

    // Mono : la loi constante en puissance est sans perte (cos^2 + sin^2 = 1),
    // l'amplitude est donc conservée et le placement stéréo se fait au mixage
    // vers le bus principal (mixStrips). Copie simple, source intacte.
    if (source.channels() == 1 || destination.channels() == 1) {
        applyGainTo(source, destination, 1.0f);
        return;
    }

    float left = 1.0f;
    float right = 1.0f;
    panGains(pan, left, right);

    const int frames = std::min(source.frames(), destination.frames());
    const int channels = std::min(source.channels(), destination.channels());
    const bool isStereo = (channels == 2);
    const float average = 0.5f * (left + right);

    for (int ch = 0; ch < channels; ++ch) {
        const float* in = source.channel(ch);
        float* out = destination.channel(ch);
        if (in == nullptr || out == nullptr)
            continue;
        // Stéréo : le canal gauche porte le signal gauche, le droit le droit.
        // Plus de 2 canaux : gain moyen, sans réorganisation.
        const float gain = isStereo ? ((ch == 0) ? left : right) : average;
        for (int i = 0; i < frames; ++i)
            out[i] = in[i] * gain;
    }
    for (int ch = channels; ch < destination.channels(); ++ch) {
        float* out = destination.channel(ch);
        if (out == nullptr)
            continue;
        std::fill(out, out + destination.frames(), 0.0f);
    }
}

void applyPan(audio::AudioBufferView buffer, float pan) noexcept
{
    applyPanTo(buffer.toConst(), buffer, pan);
}

} // namespace svp::dsp
