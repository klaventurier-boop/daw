// Studio Voice Pro — module transport : lecture, arrêt, position, boucle,
// conversion temps musical <-> échantillons.
#pragma once

#include <cstdint>

namespace svp::transport {

enum class State {
    Stopped,
    Playing,
};

/** Position de transport en trames, avec tempo pour la grille musicale.
    Le module ne touche ni au disque ni au matériel : il ne fait que compter,
    ce qui le rend testable entièrement hors ligne. */
class Transport {
public:
    Transport() = default;

    /** Prépare le transport. `sampleRate` en Hz, `tempoBpm` en noires/minute,
        `lengthFrames` = durée totale du contenu (0 = inconnue). */
    void prepare(double sampleRate, double tempoBpm, int64_t lengthFrames = 0) noexcept;

    void play() noexcept { state_ = State::Playing; }
    void stop() noexcept { state_ = State::Stopped; }
    void togglePlayStop() noexcept { state_ = (state_ == State::Playing) ? State::Stopped : State::Playing; }

    /** Retour au début tout en conservant l'état lecture/arrêt. */
    void rewind() noexcept { positionFrames_ = 0; }

    [[nodiscard]] State state() const noexcept { return state_; }
    [[nodiscard]] bool isPlaying() const noexcept { return state_ == State::Playing; }

    [[nodiscard]] int64_t positionFrames() const noexcept { return positionFrames_; }
    [[nodiscard]] double positionSeconds() const noexcept;
    [[nodiscard]] double positionBeats() const noexcept;

    void setPositionFrames(int64_t frames) noexcept;
    void setPositionSeconds(double seconds) noexcept;

    /** Avance le transport de `numFrames`, en respectant la boucle si active.
        Sémantique de boucle : dès que la position atteint ou dépasse loopEnd,
        elle revient dans [loopStart, loopEnd) avec rebouclage multiple si
        l'avance dépasse plusieurs tours. Une position située avant loopStart
        joue la zone qui précède puis entre dans la boucle. Sans boucle active,
        atteindre lengthFrames arrête la lecture. Ne fait rien si Stopped. [RT] */
    void advance(int64_t numFrames) noexcept;

    void setLoopEnabled(bool enabled) noexcept { loopEnabled_ = enabled; }
    [[nodiscard]] bool loopEnabled() const noexcept { return loopEnabled_; }
    void setLoopRange(int64_t startFrames, int64_t endFrames) noexcept;
    [[nodiscard]] int64_t loopStart() const noexcept { return loopStart_; }
    [[nodiscard]] int64_t loopEnd() const noexcept { return loopEnd_; }

    void setTempo(double bpm) noexcept;
    [[nodiscard]] double tempo() const noexcept { return tempoBpm_; }
    [[nodiscard]] double sampleRate() const noexcept { return sampleRate_; }
    [[nodiscard]] int64_t lengthFrames() const noexcept { return lengthFrames_; }
    void setLengthFrames(int64_t frames) noexcept;

    // --- conversion temps musical <-> échantillons -------------------------
    [[nodiscard]] int64_t secondsToFrames(double seconds) const noexcept;
    [[nodiscard]] double framesToSeconds(int64_t frames) const noexcept;
    [[nodiscard]] int64_t beatsToFrames(double beats) const noexcept;
    [[nodiscard]] double framesToBeats(int64_t frames) const noexcept;

private:
    double sampleRate_ = 48000.0;
    double tempoBpm_ = 120.0;
    int64_t positionFrames_ = 0;
    int64_t lengthFrames_ = 0;
    int64_t loopStart_ = 0;
    int64_t loopEnd_ = 0;
    bool loopEnabled_ = false;
    State state_ = State::Stopped;
};

} // namespace svp::transport
