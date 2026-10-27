// Studio Voice Pro — transport : position, lecture/arrêt, boucle, tempo.
#include "svp/transport/Transport.h"

#include <algorithm>
#include <cmath>

namespace svp::transport {

void Transport::prepare(double sampleRate, double tempoBpm, int64_t lengthFrames) noexcept
{
    sampleRate_ = (sampleRate > 0.0) ? sampleRate : 48000.0;
    setTempo(tempoBpm);
    setLengthFrames(lengthFrames);
    positionFrames_ = 0;
    loopStart_ = 0;
    loopEnd_ = 0;
    loopEnabled_ = false;
    state_ = State::Stopped;
}

void Transport::setTempo(double bpm) noexcept
{
    tempoBpm_ = (bpm > 0.0) ? bpm : 120.0;
}

void Transport::setLengthFrames(int64_t frames) noexcept
{
    lengthFrames_ = std::max<int64_t>(0, frames);
    positionFrames_ = std::min(positionFrames_, lengthFrames_ > 0 ? lengthFrames_ : positionFrames_);
}

void Transport::setLoopRange(int64_t startFrames, int64_t endFrames) noexcept
{
    const int64_t start = std::max<int64_t>(0, startFrames);
    const int64_t end = std::max<int64_t>(start, endFrames);
    loopStart_ = start;
    loopEnd_ = end;
}

void Transport::setPositionFrames(int64_t frames) noexcept
{
    positionFrames_ = std::max<int64_t>(0, frames);
}

void Transport::setPositionSeconds(double seconds) noexcept
{
    setPositionFrames(secondsToFrames(seconds));
}

double Transport::positionSeconds() const noexcept
{
    return framesToSeconds(positionFrames_);
}

double Transport::positionBeats() const noexcept
{
    return framesToBeats(positionFrames_);
}

int64_t Transport::secondsToFrames(double seconds) const noexcept
{
    return static_cast<int64_t>(std::llround(std::max(0.0, seconds) * sampleRate_));
}

double Transport::framesToSeconds(int64_t frames) const noexcept
{
    return static_cast<double>(frames) / sampleRate_;
}

int64_t Transport::beatsToFrames(double beats) const noexcept
{
    const double secondsPerBeat = 60.0 / tempoBpm_;
    return secondsToFrames(beats * secondsPerBeat);
}

double Transport::framesToBeats(int64_t frames) const noexcept
{
    const double secondsPerBeat = 60.0 / tempoBpm_;
    return framesToSeconds(frames) / secondsPerBeat;
}

void Transport::advance(int64_t numFrames) noexcept
{
    if (!isPlaying() || numFrames <= 0)
        return;

    positionFrames_ += numFrames;

    if (loopEnabled_ && loopEnd_ > loopStart_) {
        const int64_t length = loopEnd_ - loopStart_;
        if (positionFrames_ >= loopEnd_)
            positionFrames_ = loopStart_ + (positionFrames_ - loopEnd_) % length;
        return;
    }

    if (lengthFrames_ > 0 && positionFrames_ >= lengthFrames_) {
        positionFrames_ = lengthFrames_;
        stop();
    }
}

} // namespace svp::transport
