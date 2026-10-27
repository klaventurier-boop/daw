// Studio Voice Pro — svp-gen : générateur de fichiers WAV de test.
// Usage :
//   svp-gen --out test.wav --freq 1000 --rate 48000 --dur 2 --bits 32f --channels 1 --amp 0
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "svp/audio/Convert.h"
#include "svp/io/WavFile.h"

namespace {

constexpr double kPi = 3.14159265358979323846;

void printUsage()
{
    std::printf(
        "svp-gen 0.1 — générateur de WAV de test (Studio Voice Pro)\n"
        "  --out <fichier.wav>   fichier à créer (obligatoire)\n"
        "  --freq <Hz>           fréquence du sinus (défaut 1000)\n"
        "  --rate <Hz>           taux d'échantillonnage (défaut 48000)\n"
        "  --dur <secondes>      durée (défaut 2.0)\n"
        "  --bits <16|24|32f>    profondeur (défaut 32f)\n"
        "  --channels <1|2>      canaux (défaut 1)\n"
        "  --amp <dBFS>          amplitude crête (défaut 0 = pleine échelle)\n"
        "  --help                cette aide\n");
}

} // namespace

int main(int argc, char** argv)
{
    std::string outPath;
    double frequency = 1000.0;
    int sampleRate = 48000;
    double duration = 2.0;
    int channels = 1;
    double amplitudeDb = 0.0;
    svp::io::WavEncoding encoding = svp::io::WavEncoding::Float32;

    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        auto next = [&](const char* name) -> const char* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "svp-gen : %s attend une valeur\n", name);
                std::exit(2);
            }
            return argv[++i];
        };

        if (std::strcmp(arg, "--out") == 0)
            outPath = next("--out");
        else if (std::strcmp(arg, "--freq") == 0)
            frequency = std::atof(next("--freq"));
        else if (std::strcmp(arg, "--rate") == 0)
            sampleRate = std::atoi(next("--rate"));
        else if (std::strcmp(arg, "--dur") == 0)
            duration = std::atof(next("--dur"));
        else if (std::strcmp(arg, "--channels") == 0)
            channels = std::atoi(next("--channels"));
        else if (std::strcmp(arg, "--amp") == 0)
            amplitudeDb = std::atof(next("--amp"));
        else if (std::strcmp(arg, "--bits") == 0) {
            const char* value = next("--bits");
            if (std::strcmp(value, "16") == 0)
                encoding = svp::io::WavEncoding::Pcm16;
            else if (std::strcmp(value, "24") == 0)
                encoding = svp::io::WavEncoding::Pcm24;
            else if (std::strcmp(value, "32f") == 0 || std::strcmp(value, "32") == 0)
                encoding = svp::io::WavEncoding::Float32;
            else {
                printUsage();
                return 2;
            }
        } else if (std::strcmp(arg, "--help") == 0 || std::strcmp(arg, "-h") == 0) {
            printUsage();
            return 0;
        } else {
            std::fprintf(stderr, "svp-gen : argument inconnu « %s »\n", arg);
            printUsage();
            return 2;
        }
    }

    if (outPath.empty()) {
        printUsage();
        return 2;
    }

    const int frames = static_cast<int>(duration * sampleRate);
    svp::audio::AudioBuffer buffer(channels, frames);
    const double amplitude = svp::audio::decibelsToGain(amplitudeDb);

    for (int i = 0; i < frames; ++i) {
        const double t = static_cast<double>(i) / sampleRate;
        const float value = static_cast<float>(amplitude * std::sin(2.0 * kPi * frequency * t));
        for (int ch = 0; ch < channels; ++ch)
            buffer.setSample(ch, i, value);
    }

    svp::io::WavFormat format;
    format.sampleRate = sampleRate;
    format.channels = channels;
    format.encoding = encoding;

    std::string error;
    if (!svp::io::writeWavFile(outPath, buffer, format, &error)) {
        std::fprintf(stderr, "svp-gen : erreur : %s\n", error.c_str());
        return 1;
    }

    std::printf("svp-gen : %s écrit (%d Hz, %d canal(aux), %d trames, %.3f s, %.1f Hz, crête %.2f dBFS)\n",
                outPath.c_str(), sampleRate, channels, frames, duration, frequency, amplitudeDb);
    return 0;
}
