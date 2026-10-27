// Studio Voice Pro — implémentation du tampon audio.
#include "svp/audio/Buffer.h"

#include <algorithm>

namespace svp::audio {

AudioBuffer::AudioBuffer(int numChannels, int numFrames)
{
    resize(numChannels, numFrames);
}

void AudioBuffer::resize(int numChannels, int numFrames)
{
    channels_ = std::max(0, numChannels);
    frames_ = std::max(0, numFrames);
    data_.assign(totalSamples(), 0.0f);
}

void AudioBuffer::clear() noexcept
{
    std::fill(data_.begin(), data_.end(), 0.0f);
}

void AudioBuffer::clearChannel(int channel) noexcept
{
    if (channel < 0 || channel >= channels_)
        return;
    const auto offset = static_cast<std::size_t>(channel) * static_cast<std::size_t>(frames_);
    std::fill(data_.begin() + static_cast<std::ptrdiff_t>(offset),
              data_.begin() + static_cast<std::ptrdiff_t>(offset + static_cast<std::size_t>(frames_)),
              0.0f);
}

float* AudioBuffer::channel(int ch) noexcept
{
    if (ch < 0 || ch >= channels_)
        return nullptr;
    return data_.data() + static_cast<std::size_t>(ch) * static_cast<std::size_t>(frames_);
}

const float* AudioBuffer::channel(int ch) const noexcept
{
    if (ch < 0 || ch >= channels_)
        return nullptr;
    return data_.data() + static_cast<std::size_t>(ch) * static_cast<std::size_t>(frames_);
}

float AudioBuffer::sample(int ch, int frame) const noexcept
{
    const float* c = channel(ch);
    if (c == nullptr || frame < 0 || frame >= frames_)
        return 0.0f;
    return c[frame];
}

void AudioBuffer::setSample(int ch, int frame, float value) noexcept
{
    float* c = channel(ch);
    if (c == nullptr || frame < 0 || frame >= frames_)
        return;
    c[frame] = value;
}

void AudioBuffer::addSample(int ch, int frame, float value) noexcept
{
    float* c = channel(ch);
    if (c == nullptr || frame < 0 || frame >= frames_)
        return;
    c[frame] += value;
}

} // namespace svp::audio
