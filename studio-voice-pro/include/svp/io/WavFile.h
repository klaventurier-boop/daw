// Studio Voice Pro — module io : lecture/écriture WAV, code maison (aucune
// bibliothèque externe, ni libsndfile ni JUCE). Formats v0.1 :
//   - PCM entier signé 16 bits
//   - PCM entier signé 24 bits
//   - flottant IEEE 32 bits
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "svp/audio/Buffer.h"

namespace svp::io {

enum class WavEncoding {
    Pcm16,
    Pcm24,
    Float32,
};

struct WavFormat {
    int sampleRate = 48000;
    int channels = 1;
    WavEncoding encoding = WavEncoding::Float32;

    [[nodiscard]] int bitsPerSample() const noexcept;  // 16 / 24 / 32
    [[nodiscard]] int bytesPerSample() const noexcept; // 2 / 3 / 4
};

struct WavReadResult {
    bool ok = false;
    std::string error;       // message lisible en cas d'échec
    WavFormat format{};
    audio::AudioBuffer buffer{};
};

/** Lit un fichier WAV depuis le disque. Accepte les chunks `fmt ` / `data`
    dans n'importe quel ordre et ignore les chunks inconnus (`LIST`, `fact`...).
    Refuse tout format autre que PCM 16/24 ou flottant 32 bits. N'utilise
    aucune allocation dans un thread audio (fonction hors ligne). */
[[nodiscard]] WavReadResult readWavFile(const std::string& path);

/** Écrit un fichier WAV 16/24/32f. Crée/écrase le fichier cible.
    Le nombre de trames écrites est celui du buffer fourni. */
[[nodiscard]] bool writeWavFile(const std::string& path,
                                const audio::AudioBuffer& buffer,
                                const WavFormat& format,
                                std::string* error = nullptr);

} // namespace svp::io
