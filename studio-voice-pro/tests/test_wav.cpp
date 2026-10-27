// Studio Voice Pro — tests : écriture puis relecture WAV en 16 / 24 / 32f.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "svp/audio/Convert.h"
#include "svp/io/WavFile.h"
#include "svp_test.hpp"

namespace {

constexpr double kPi = 3.14159265358979323846;

/** Répertoire de travail des tests, fourni par CTest via SVP_TEST_TMPDIR. */
std::string tempPath(const std::string& name)
{
    const char* dir = std::getenv("SVP_TEST_TMPDIR");
    const std::string base = (dir != nullptr) ? dir : ".";
    return base + "/" + name;
}

svp::audio::AudioBuffer makeSine(int channels, int frames, int sampleRate, double frequency)
{
    svp::audio::AudioBuffer buffer(channels, frames);
    for (int i = 0; i < frames; ++i) {
        const double t = static_cast<double>(i) / sampleRate;
        const float value = static_cast<float>(0.5 * std::sin(2.0 * kPi * frequency * t));
        for (int ch = 0; ch < channels; ++ch) {
            // Canal 0 et 1 légèrement différents pour détecter les inversions.
            buffer.setSample(ch, i, (ch == 0) ? value : -value);
        }
    }
    return buffer;
}

void roundtripCase(svp::io::WavEncoding encoding, double tolerance, const std::string& name)
{
    constexpr int kRate = 48000;
    constexpr int kFrames = 4800; // 100 ms
    constexpr int kChannels = 2;

    const svp::audio::AudioBuffer source = makeSine(kChannels, kFrames, kRate, 1000.0);

    svp::io::WavFormat format;
    format.sampleRate = kRate;
    format.channels = kChannels;
    format.encoding = encoding;

    const std::string path = tempPath(name);
    std::string error;
    SVP_CHECK(svp::io::writeWavFile(path, source, format, &error));

    const svp::io::WavReadResult read = svp::io::readWavFile(path);
    if (!read.ok) {
        svptest::reportFailure(__FILE__, __LINE__, "relecture refusée : " + read.error);
        return;
    }

    SVP_CHECK(read.format.encoding == encoding);
    SVP_CHECK(read.format.sampleRate == kRate);
    SVP_CHECK(read.format.channels == kChannels);
    SVP_CHECK(read.buffer.frames() == kFrames);
    SVP_CHECK(read.format.bitsPerSample() == format.bitsPerSample());

    double worst = 0.0;
    for (int ch = 0; ch < kChannels; ++ch)
        for (int i = 0; i < kFrames; ++i)
            worst = std::max(worst, std::fabs(static_cast<double>(read.buffer.sample(ch, i))
                                              - static_cast<double>(source.sample(ch, i))));

    SVP_CHECK_NEAR(worst, 0.0, tolerance);
}

} // namespace

SVP_TEST(io, wav_roundtrip_pcm16)
{
    roundtripCase(svp::io::WavEncoding::Pcm16, 1.0 / 32767.0 + 1e-7, "roundtrip16.wav");
}

SVP_TEST(io, wav_roundtrip_pcm24)
{
    roundtripCase(svp::io::WavEncoding::Pcm24, 1.0 / 8388607.0 + 1e-8, "roundtrip24.wav");
}

SVP_TEST(io, wav_roundtrip_float32)
{
    roundtripCase(svp::io::WavEncoding::Float32, 1e-7, "roundtrip32f.wav");
}

SVP_TEST(io, wav_header_is_riff_wave_of_expected_size)
{
    constexpr int kRate = 44100;
    constexpr int kFrames = 512;
    svp::audio::AudioBuffer buffer(1, kFrames);
    for (int i = 0; i < kFrames; ++i)
        buffer.setSample(0, i, static_cast<float>(i) / static_cast<float>(kFrames));

    svp::io::WavFormat format;
    format.sampleRate = kRate;
    format.channels = 1;
    format.encoding = svp::io::WavEncoding::Pcm16;

    const std::string path = tempPath("header16.wav");
    SVP_CHECK(svp::io::writeWavFile(path, buffer, format, nullptr));

    std::FILE* file = std::fopen(path.c_str(), "rb");
    SVP_CHECK(file != nullptr);
    if (file == nullptr)
        return;
    char magic[12] = {};
    const std::size_t readMagic = std::fread(magic, 1, 12, file);
    SVP_CHECK(readMagic == 12);
    std::fseek(file, 0, SEEK_END);
    const long size = std::ftell(file);
    std::fclose(file);

    SVP_CHECK(std::string(magic, 4) == "RIFF");
    SVP_CHECK(std::string(magic + 8, 4) == "WAVE");
    // 44 octets d'en-tête PCM 16 bits + trames * canaux * 2 octets
    SVP_CHECK(size == 44 + kFrames * 2);
}

SVP_TEST(io, full_scale_pcm_write_does_not_wrap)
{
    svp::audio::AudioBuffer buffer(1, 4);
    buffer.setSample(0, 0, 1.0f);
    buffer.setSample(0, 1, -1.0f);
    buffer.setSample(0, 2, 1.5f);  // doit être borné à +1
    buffer.setSample(0, 3, -1.5f); // doit être borné à -1

    svp::io::WavFormat format;
    format.sampleRate = 48000;
    format.channels = 1;
    format.encoding = svp::io::WavEncoding::Pcm16;

    const std::string path = tempPath("clip16.wav");
    SVP_CHECK(svp::io::writeWavFile(path, buffer, format, nullptr));

    const svp::io::WavReadResult read = svp::io::readWavFile(path);
    SVP_CHECK(read.ok);
    SVP_CHECK_NEAR(read.buffer.sample(0, 0), 1.0, 1e-4);
    SVP_CHECK_NEAR(read.buffer.sample(0, 1), -1.0, 1e-4);
    SVP_CHECK_NEAR(read.buffer.sample(0, 2), 1.0, 1e-4);
    SVP_CHECK_NEAR(read.buffer.sample(0, 3), -1.0, 1e-4);
}

SVP_TEST(io, reading_missing_or_invalid_file_fails_cleanly)
{
    const svp::io::WavReadResult missing = svp::io::readWavFile(tempPath("inexistant-xyz.wav"));
    SVP_CHECK(!missing.ok);
    SVP_CHECK(!missing.error.empty());

    const std::string path = tempPath("pas-un-wav.txt");
    if (std::FILE* file = std::fopen(path.c_str(), "wb")) {
        std::fputs("ceci n'est pas un fichier wav, mais assez long pour depasser l'entete", file);
        std::fclose(file);
    }
    const svp::io::WavReadResult invalid = svp::io::readWavFile(path);
    SVP_CHECK(!invalid.ok);
    SVP_CHECK(!invalid.error.empty());
}

SVP_TEST(io, writing_empty_buffer_is_refused)
{
    svp::audio::AudioBuffer empty;
    svp::io::WavFormat format;
    format.channels = 1;
    std::string error;
    SVP_CHECK(!svp::io::writeWavFile(tempPath("vide.wav"), empty, format, &error));
    SVP_CHECK(!error.empty());
}

SVP_TEST(io, write_then_read_is_byte_stable_for_24_bits)
{
    // Écriture d'un signal déjà quantifié 24 bits : la relecture doit donner
    // exactement les mêmes échantillons (aucune perte supplémentaire).
    svp::audio::AudioBuffer buffer(1, 256);
    for (int i = 0; i < 256; ++i) {
        const int32_t quantized = (i - 128) * 32768; // multiple de 256 sur 24 bits
        buffer.setSample(0, i, svp::audio::pcm24ToFloat(quantized));
    }

    svp::io::WavFormat format;
    format.sampleRate = 48000;
    format.channels = 1;
    format.encoding = svp::io::WavEncoding::Pcm24;

    const std::string path = tempPath("stable24.wav");
    SVP_CHECK(svp::io::writeWavFile(path, buffer, format, nullptr));

    const svp::io::WavReadResult read = svp::io::readWavFile(path);
    SVP_CHECK(read.ok);
    for (int i = 0; i < 256; ++i)
        SVP_CHECK_NEAR(read.buffer.sample(0, i), buffer.sample(0, i), 1e-9);
}
